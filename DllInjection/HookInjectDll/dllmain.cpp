// dllmain.cpp : 定义 DLL 应用程序的入口点。
#include "pch.h"
#include "../Common/Helper.h"

#define EXPORTFUN extern "C" __declspec(dllexport)

HHOOK g_hHook;
HMODULE g_hModule;

#pragma data_seg("flag_data")
char g_strDllPath[MAX_PATH] = { 0 };
#pragma data_seg()
#pragma comment(linker,"/SECTION:flag_data,RWS")

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
		g_hModule = hModule;
		break;
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}


//钩子回调函数
LRESULT CALLBACK HookProc(
	int code,       // hook code
	WPARAM wParam,  // virtual-key code
	LPARAM lParam   // keystroke-message information
)
{
	LoadLibrary(str2wstr(g_strDllPath).c_str());

	return CallNextHookEx(g_hHook, code, wParam, lParam);;
}



EXPORTFUN void SetHook(DWORD  dwPid, char* szDllPath)
{
	THREADENTRY32 te32 = { 0 };
	HANDLE hThreadSnap = NULL;
	DWORD  dwThreadId = 0;

	strcpy_s(g_strDllPath, szDllPath);
	//创建线程快照查找目标程序主线程
	te32.dwSize = sizeof(te32);
	hThreadSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
	if (hThreadSnap == INVALID_HANDLE_VALUE)
	{
		return;
	}

	//遍历查询目标程序主线程ID
	if (Thread32First(hThreadSnap, &te32))
	{
		do
		{
			if (dwPid == te32.th32OwnerProcessID)
			{
				dwThreadId = te32.th32ThreadID;
				break;
			}
		} while (Thread32Next(hThreadSnap, &te32));
	}

	if (dwThreadId > 0)
	{
		g_hHook = SetWindowsHookEx(WH_CBT, HookProc, g_hModule, dwThreadId);
	}
	else
	{
		g_hHook = SetWindowsHookEx(WH_CBT, HookProc, g_hModule, 0);
	}
}

EXPORTFUN void UnHook()
{
	UnhookWindowsHookEx(g_hHook);
}


