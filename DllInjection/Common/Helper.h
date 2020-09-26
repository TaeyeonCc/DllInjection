#pragma once
#include <windows.h>

#include <string>
using namespace std;

#include <tlhelp32.h>


std::wstring  stringformat(const wchar_t* fmt, ...);
std::string  stringformatA(const char* fmt, ...);
std::string wstr2str(const std::wstring wstrSrc, UINT CodePage = CP_ACP/*CP_UTF8*/);
std::wstring str2wstr(const std::string wstrSrc, UINT CodePage = CP_ACP/*CP_UTF8*/);

BOOL  GetProcessNameByProcessId(int  nID, wstring& strName);