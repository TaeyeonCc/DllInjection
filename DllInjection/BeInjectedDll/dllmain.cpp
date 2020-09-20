// dllmain.cpp : 定义 DLL 应用程序的入口点。
#include "pch.h"
#include "Helper.h"

BOOL APIENTRY DllMain( HMODULE hModule,
                       DWORD  ul_reason_for_call,
                       LPVOID lpReserved
                     )
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
    {
        wstring strDbg = L"jjcc BeInjectedDll.dll was successfully injected into the target process, processId: ";
        strDbg.append(to_wstring(GetCurrentProcessId()));
        strDbg.append(L"  processName: ");

        wstring strProcessName = L"";
        GetProcessNameByProcessId(GetCurrentProcessId(), strProcessName);

        strDbg.append(strProcessName);
        OutputDebugString(strDbg.c_str());
    }
    case DLL_THREAD_ATTACH:
    case DLL_THREAD_DETACH:
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

