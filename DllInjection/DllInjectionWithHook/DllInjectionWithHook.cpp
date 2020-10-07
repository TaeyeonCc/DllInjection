// DllInjectionWithHook.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include "../Common/Helper.h"

void SetHook(DWORD pid,string strDllPath);

int main(int argc, char* argv[])
{
	if (argc < 3) {
		printf("Usage: InjectDll <ProcessId>  <dll Path>\n");
		return 0;
	}
	
	SetHook(_wtoi(str2wstr(argv[1]).c_str()),argv[2]);

	return 0;
}

typedef void(*LPFUN)();
typedef void(*LPFUN2)(DWORD dwPid, char* szDllPath);
HINSTANCE g_hDll;
LPFUN2 g_pfnSetHook = NULL;
LPFUN g_pfnUnHook = NULL;

void SetHook(DWORD pid, string strDllPath)
{
	// TODO:  在此添加控件通知处理程序代码
	g_hDll = LoadLibrary(L"HookInjectDll.dll");

	if (g_hDll != NULL)
	{
		g_pfnSetHook = (LPFUN2)GetProcAddress(g_hDll, "SetHook");
		g_pfnUnHook = (LPFUN)GetProcAddress(g_hDll, "UnHook");
	}
	else
	{
		MessageBox("加载DLL失败！");
		return;
	}

	//安装钩子函数
	if (g_pfnSetHook != NULL)
	{
		g_pfnSetHook(pid, (char*)strDllPath.c_str());
	}
	else
	{
		MessageBox("安装钩子失败！");
		return;
	}
	Sleep(100000);
}


void Unhook()
{
	// TODO:  在此添加控件通知处理程序代码
	if (g_hDll != NULL)
	{
		//卸载钩子函数
		g_pfnUnHook();
		//抹掉DLL
		FreeLibrary(g_hDll);
	}
}
