// Debugger.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include "../Common/Helper.h"

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

void OnInject(string strExePath, string strDllPath)
{
	
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
		//// int3
		//0xcc
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
		//// int3
		//0xcc
	};
#endif



	BOOL bRet;
	DWORD dwProcessId = 0;
	HANDLE hThread = NULL;
	HANDLE hProcess = NULL;
	DEBUG_EVENT dbgEvent = { 0 };
	STARTUPINFO si = { sizeof(si) };
	PROCESS_INFORMATION pi = { 0 };
	BOOL bIsSystemBp = TRUE;
	DWORD dwOldEip = 0;
	CONTEXT contextOld;
	CONTEXT contextNew;
	char* lpBaseAddress = NULL;
	const auto page_size = 1 << 12;

	bRet = CreateProcess(str2wstr(strExePath).c_str(),
		NULL,
		NULL,
		NULL,
		FALSE,
		DEBUG_ONLY_THIS_PROCESS,
		NULL,
		NULL,
		&si,
		&pi);
	if (!bRet)
	{
		MessageBox("CreateProcess 失败");
		return;
	}

	//防止被调试进程和调试器一起关闭
	bRet = DebugSetProcessKillOnExit(FALSE);

	while (WaitForDebugEvent(&dbgEvent, INFINITE))
	{
		switch (dbgEvent.dwDebugEventCode)
		{
		case CREATE_PROCESS_DEBUG_EVENT:
		{
			hProcess = dbgEvent.u.CreateProcessInfo.hProcess;
			hThread = dbgEvent.u.CreateProcessInfo.hThread;

			lpBaseAddress = static_cast<char*>(::VirtualAllocEx(hProcess, nullptr, page_size,
				MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
			if (NULL == lpBaseAddress)
			{
				MessageBox("VirtualAllocEx 失败");
				return;
			}

			contextOld.ContextFlags = CONTEXT_FULL;
			if (!::GetThreadContext(hThread, &contextOld))
				return;

			contextNew = contextOld;

			auto loadLibraryAddress = ::GetProcAddress(::GetModuleHandle(L"kernel32.dll"), "LoadLibraryA");

#ifdef _WIN64
			// set dll path
			* reinterpret_cast<PVOID*>(code + 0x10) = static_cast<void*>(lpBaseAddress + page_size / 2);
			// set LoadLibraryA address
			*reinterpret_cast<PVOID*>(code + 0x1a) = static_cast<void*>(loadLibraryAddress);
			// jump address (back to the original code)
			*reinterpret_cast<unsigned long long*>(code + 0x34) = contextNew.Rip;
#else
			// set dll path
			* reinterpret_cast<PVOID*>(code + 2) = static_cast<void*>(lpBaseAddress + page_size / 2);
			// set LoadLibraryA address
			*reinterpret_cast<PVOID*>(code + 7) = static_cast<void*>(loadLibraryAddress);
			// jump address (back to the original code)
			*reinterpret_cast<unsigned*>(code + 0xf) = contextNew.Eip;
#endif

			//
			// copy the injected function into the buffer
			//

			if (!::WriteProcessMemory(hProcess, lpBaseAddress, code, sizeof(code), nullptr))
				return;

			//
			// copy the DLL name into the buffer
			//
			if (!::WriteProcessMemory(hProcess, lpBaseAddress + page_size / 2, strDllPath.c_str(), ::strlen(strDllPath.c_str()) + 1, NULL))
				return;

			//
			// change thread context and let it go!
			//
#ifdef _WIN64
			contextNew.Rip = reinterpret_cast<unsigned long long>(lpBaseAddress);
#else
			contextNew.Eip = reinterpret_cast<DWORD>(lpBaseAddress);
#endif
			if (!::SetThreadContext(hThread, &contextNew))
				return;

			ExitProcess(0);
		}

			break;
		case EXCEPTION_DEBUG_EVENT:
		{
			if (dbgEvent.u.Exception.ExceptionRecord.ExceptionCode == EXCEPTION_BREAKPOINT)
			{
				//屏蔽掉系统断点
				if (bIsSystemBp)
				{
					bIsSystemBp = FALSE;
					break;
				}

				//释放内存
				bRet = VirtualFreeEx(hProcess,
					lpBaseAddress,
					0,
					MEM_RELEASE
				);

				if (!bRet)
				{
					MessageBox("VirtualFreeEx 失败");
					return;
				}

				//恢复到程序创建时的EIP
				bRet = SetThreadContext(hThread, &contextOld);
				if (!bRet)
				{
					MessageBox("SetThreadContext 失败");
					return;
				}

				bRet = ContinueDebugEvent(dbgEvent.dwProcessId, dbgEvent.dwThreadId, DBG_CONTINUE);
				if (!bRet)
				{
					MessageBox("ContinueDebugEvent 失败!!");
					return;
				}
				//退出本进程，让被调试程序跑起来
				ExitProcess(0);
				return;

			}
			break;
		}

		bRet = ContinueDebugEvent(dbgEvent.dwProcessId, dbgEvent.dwThreadId, DBG_CONTINUE);
		if (!bRet)
		{
			MessageBox("ContinueDebugEvent 失败!!");
			return;
		}
		break;
		}
	}
}