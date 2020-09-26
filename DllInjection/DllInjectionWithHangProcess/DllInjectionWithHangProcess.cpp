// DllInjectionWithHangProcess.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include "../Common/Helper.h"

char* g_buff;

void OnInject(string strExePath, string strDllPath);

int main(int argc, char* argv[])
{
	if (argc < 3) {
		printf("Usage: InjectDll <Exe Path> <dll Path>\n");
		return 0;
	}

	OnInject(argv[1], argv[2]);

	return 0;
}

bool DoInjection(HANDLE hProcess, HANDLE hThread, PCSTR dllPath) {
#ifdef _WIN64
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
#else
	BYTE code[] = {
		0x60,
		0x68, 0x11, 0x11, 0x11, 0x11,
		0xb8, 0x22, 0x22, 0x22, 0x22,
		0xff, 0xd0,
		0x61,
		0x68, 0x33, 0x33, 0x33, 0x33,
		0xc3
	};
#endif

	const auto page_size = 1 << 12;

	//
	// allocate buffer in target process to hold DLL path and injected function code
	//
	auto buffer = static_cast<char*>(::VirtualAllocEx(hProcess, nullptr, page_size,
		MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
	if (!buffer)
		return false;

	g_buff = buffer;

	printf("Buffer in remote process: %p\n", buffer);

	CONTEXT context;
	context.ContextFlags = CONTEXT_FULL;
	if (!::GetThreadContext(hThread, &context))
		return false;

	auto loadLibraryAddress = ::GetProcAddress(::GetModuleHandle(L"kernel32.dll"), "LoadLibraryA");

#ifdef _WIN64
	// set dll path
	* reinterpret_cast<PVOID*>(code + 0x10) = static_cast<void*>(buffer + page_size / 2);
	// set LoadLibraryA address
	*reinterpret_cast<PVOID*>(code + 0x1a) = static_cast<void*>(loadLibraryAddress);
	// jump address (back to the original code)
	*reinterpret_cast<unsigned long long*>(code + 0x34) = context.Rip;
#else
	// set dll path
	* reinterpret_cast<PVOID*>(code + 2) = static_cast<void*>(buffer + page_size / 2);
	// set LoadLibraryA address
	*reinterpret_cast<PVOID*>(code + 7) = static_cast<void*>(loadLibraryAddress);
	// jump address (back to the original code)
	*reinterpret_cast<unsigned*>(code + 0xf) = context.Eip;
#endif

	//
	// copy the injected function into the buffer
	//

	if (!::WriteProcessMemory(hProcess, buffer, code, sizeof(code), nullptr))
		return false;

	//
	// copy the DLL name into the buffer
	//
	if (!::WriteProcessMemory(hProcess, buffer + page_size / 2, dllPath, ::strlen(dllPath) + 1, NULL))
		return false;

	//
	// change thread context and let it go!
	//
#ifdef _WIN64
	context.Rip = reinterpret_cast<unsigned long long>(buffer);
#else
	context.Eip = reinterpret_cast<DWORD>(buffer);
#endif
	if (!::SetThreadContext(hThread, &context))
		return false;

	return true;
}

void OnInject(string strExePath,string strDllPath)
{
	BOOL bRet = FALSE;

	STARTUPINFO si = { 0 };
	PROCESS_INFORMATION pi = { 0 };
	si.wShowWindow = SW_SHOWDEFAULT;
	si.cb = sizeof(PROCESS_INFORMATION);
	
	bRet = CreateProcess(str2wstr(strExePath).c_str(), NULL, NULL, NULL, FALSE, CREATE_SUSPENDED,
		NULL, NULL, &si, &pi);

	if (!bRet)
	{
		MessageBox("CreateProcess 失败");
		return;
	}

	DoInjection(pi.hProcess, pi.hThread, strDllPath.c_str());
	

	::ResumeThread(pi.hThread);


	Sleep(500);
	if (g_buff)
	{
		if (!VirtualFreeEx(pi.hProcess, g_buff, 0, MEM_RELEASE))
		{
			MessageBox("VirtualFreeEx 失败");
			return;
		}
	}
	CloseHandle(pi.hProcess);
	CloseHandle(pi.hThread);
	
	return;
}

