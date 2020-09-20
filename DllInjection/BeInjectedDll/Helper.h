#pragma once



std::wstring  stringformat(const wchar_t* fmt, ...);
std::string  stringformatA(const char* fmt, ...);

BOOL  GetProcessNameByProcessId(int  nID, wstring& strName);