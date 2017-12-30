#pragma once

#include "HackerDevice.h"

void InitD311();

void NvAPIOverride();

extern "C" HMODULE (__stdcall *pOrigLoadLibraryExW)(
	_In_       LPCTSTR lpFileName,
	_Reserved_ HANDLE  hFile,
	_In_       DWORD   dwFlags
	);

extern "C" HMODULE __stdcall Hooked_LoadLibraryExW(_In_ LPCWSTR lpLibFileName, _Reserved_ HANDLE hFile, _In_ DWORD dwFlags);



