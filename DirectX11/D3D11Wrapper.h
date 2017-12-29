#pragma once

#include "HackerDevice.h"


void NvAPIOverride();

static HMODULE(__stdcall *pOrigLoadLibraryExW)(
	_In_       LPCTSTR lpFileName,
	_Reserved_ HANDLE  hFile,
	_In_       DWORD   dwFlags
	) = nullptr;

static HMODULE __stdcall Hooked_LoadLibraryExW(_In_ LPCWSTR lpLibFileName, _Reserved_ HANDLE hFile, _In_ DWORD dwFlags);



