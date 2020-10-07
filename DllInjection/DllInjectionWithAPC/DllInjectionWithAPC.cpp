// DllInjectionWithAPC.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include "../Common/Helper.h"

void DoInject(string strExePath, string strDllPath);

int main(int argc, char* argv[])
{
	if (argc < 3) {
		printf("Usage: InjectDll <exe Path>  <dll Path>\n");
		return 0;
	}

	DoInject(argv[1], argv[2]);

	return 0;
}


void DoInject(string strExePath, string strDllPath)
{
	// TODO:  在此添加控件通知处理程序代码
	DWORD dwRet = 0;
	PROCESS_INFORMATION pi;
	STARTUPINFO si;
	ZeroMemory(&pi, sizeof(pi));
	ZeroMemory(&si, sizeof(si));
	si.cb = sizeof(STARTUPINFO);

	//以挂起的方式创建进程
	dwRet = CreateProcess(str2wstr(strExePath).c_str(),
		NULL,
		NULL,
		NULL,
		FALSE,
		CREATE_SUSPENDED,
		NULL,
		NULL,
		&si,
		&pi);

	if (!dwRet)
	{
		MessageBox("CreateProcess失败！！");
		return;
	}

	PVOID lpDllName = VirtualAllocEx(pi.hProcess,
		NULL,
		::strlen(strDllPath.c_str()) + 1,
		MEM_COMMIT,
		PAGE_READWRITE);



	if (lpDllName)
	{
		//将DLL路径写入目标进程空间
		if (WriteProcessMemory(pi.hProcess, lpDllName, strDllPath.c_str(), ::strlen(strDllPath.c_str()) + 1, NULL))
		{
			LPVOID nLoadLibrary = (LPVOID)GetProcAddress(GetModuleHandle(L"kernel32.dll"), "LoadLibraryA");
			//向远程APC队列插入LoadLibraryA
			if (!QueueUserAPC((PAPCFUNC)nLoadLibrary, pi.hThread, (DWORD)lpDllName))
			{
				MessageBox("QueueUserAPC失败！！");
				return;
			}
		}
		else
		{
			MessageBox("WriteProcessMemory失败！！");
			return;
		}
	}

	//恢复主线程
	ResumeThread(pi.hThread);
}


void test()//也可以把进程中的所有线程都插入APC
{
	//1.查找窗口
	HWND hWnd = ::FindWindow(NULL, TEXT("APCTest"));
	if (NULL == hWnd)
	{
		return;
	}
	/*2.获得进程的PID,当然通用的则是你把进程PID当做要注入的程序,这样不局限
	于窗口了.这里简单编写,进程PID可以快照遍历获取
	*/
	DWORD dwPid = 0;
	DWORD dwTid = 0;
	dwTid = GetWindowThreadProcessId(hWnd, &dwPid);

	//3.打开进程
	HANDLE hProcess = NULL;
	hProcess = OpenProcess(PROCESS_ALL_ACCESS, FALSE, dwPid);
	if (NULL == hProcess)
	{
		return;
	}
	//4.成功了,申请远程内存
	void* lpAddr = NULL;
	lpAddr = VirtualAllocEx(hProcess, 0, 0x1000, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
	if (NULL == lpAddr)
	{
		return;
	}
	//5.写入我们的DLL路径,这里我写入当前根目录下的路径
	char szBuf[] = "MyDll.dll";
	BOOL bRet = WriteProcessMemory(hProcess, lpAddr, szBuf, strlen(szBuf) + 1, NULL);
	if (!bRet)
	{
		return;
	}
	//6.根据线程Tid,打开线程句柄
	HANDLE hThread = NULL;
	hThread = OpenThread(THREAD_ALL_ACCESS, FALSE, dwTid);
	if (NULL == hThread)
	{
		return;
	}
	//7.给APC队列中插入回调函数
	QueueUserAPC((PAPCFUNC)LoadLibraryA, hThread, (ULONG_PTR)lpAddr);

	CloseHandle(hThread);
	CloseHandle(hProcess);
}

//APC(Asynchronous procedure call)异步程序调用，
//在 NT 中，有两种类型的 APCs：用户模式和内核模式。
//用户 APCs 运行在用户模式下目 标线程当前上下文中，并且需要从目标线程得到许可来运行。
//特别是，用户模式的 APCs 需 要目标线程处在 alertable 等待状态才能被成功的调度执行。
//通过调用下面任意一个函数， 都可以让线程进入这种状态。
//这些函数是：KeWaitForSingleObject, KeWaitForMultipleObjects, KeWaitForMutexObject, KeDelayExecutionThread。
//对 于 用 户 模 式 下 ， 可 以 调 用 函 数 SleepEx, SignalObjectAndWait, WaitForSingleObjectEx,
//WaitForMultipleObjectsEx, MsgWaitForMultipleObjectsEx 都可以使目标线程处于 alertable 等 待 状 态 ，
//从 而 让 用 户 模 式 APCs 执 行, 原 因 是 这 些 函 数 最 终 都 是 调 用 了 内 核 中 的
//KeWaitForSingleObject, KeWaitForMultipleObjects, KeWaitForMutexObject, KeDelayExecutionThread 等 函 数 。
//另 外 通 过 调 用 一 个 未 公 开 的 alert - test 服 务 KeTestAlertThread，用户线程可以使用户模式 APCs 执行。
//当一个用户模式 APC 被投递到一个线程，调用上面的等待函数，如果返回等待状态 STATUS_USER_APC，在返回用户模式时，
//内核转去控制 APC 例程，当 APC 例程完成后，再继 续线程的执行.上面一大堆内容的意思就是，
//当进程某个线程调用函数 SleepEx, SignalObjectAndWait, WaitForSingleObjectEx, WaitForMultipleObjectsEx, MsgWaitForMultipleObjectsEx
//这 些 函 数 时 候，会让执行的线程中断，我们需要利用 QueueUserAPC()在线程中断的时间内向 APC 中插 入一个函数指针，
//当线程苏醒的时候 APC 队列里面这个函数指针就会被执行，我们在 APC
//队列中插入 LoadLibrary 函数就可以完成 DLL 注入的工作。