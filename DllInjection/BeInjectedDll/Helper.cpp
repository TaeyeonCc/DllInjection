#include "Helper.h"
#include "pch.h"

std::wstring  stringformat(const wchar_t* fmt, ...)
{
	std::wstring s = L"";
	try
	{
		va_list   argptr;
		va_start(argptr, fmt);
#pragma warning( push )
#pragma warning( disable : 4996 )
		int   bufsize = _vsnwprintf(NULL, 0, fmt, argptr) + 2;
#pragma warning( pop )
		wchar_t* buf = new wchar_t[bufsize];
		_vsnwprintf_s(buf, bufsize, _TRUNCATE, fmt, argptr);
		s = buf;
		delete[] buf;
		va_end(argptr);
	}
	catch (...)
	{
		s = L"TryError!";
	}
	return   s;
}

std::string  stringformatA(const char* fmt, ...)
{
	std::string s = "";
	try
	{
		va_list   argptr;
		va_start(argptr, fmt);
#pragma warning( push )
#pragma warning( disable : 4996 )
		int   bufsize = _vsnprintf(NULL, 0, fmt, argptr) + 1;
#pragma warning( pop )
		char* buf = new char[bufsize];
		_vsnprintf_s(buf, bufsize, _TRUNCATE, fmt, argptr);
		s = buf;
		delete[] buf;
		va_end(argptr);
	}
	catch (...)
	{
		s = "TryError!";
	}
	return   s;
}


BOOL  GetProcessNameByProcessId(int  nID, wstring& strName)
{
	HANDLE   hProcessSnap = NULL;
	PROCESSENTRY32   pe32 = { 0 };
	hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (hProcessSnap == (HANDLE)-1)
	{
		return   FALSE;
	}
	pe32.dwSize = sizeof(PROCESSENTRY32);
	if (Process32First(hProcessSnap, &pe32))
	{
		do
		{
			if (nID == pe32.th32ProcessID)     //判断制定进程号 
			{
				strName = stringformat(L"%s", pe32.szExeFile);
			}
		} while (Process32Next(hProcessSnap, &pe32));
	}
	else
	{
		CloseHandle(hProcessSnap);
		return   FALSE;
	}
	CloseHandle(hProcessSnap);
	return   TRUE;
}