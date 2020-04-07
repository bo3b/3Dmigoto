#pragma once

#include <Windows.h>
#include <d3d11_1.h>
#include <dxgi1_2.h>


void InitD311();
void DestroyDLL();

void NvAPIOverride();

extern "C" HMODULE (__stdcall *fnOrigLoadLibraryExW)(
	_In_       LPCWSTR lpLibFileName,
	_Reserved_ HANDLE  hFile,
	_In_       DWORD   dwFlags
	);

extern "C" HMODULE __stdcall Hooked_LoadLibraryExW(_In_ LPCWSTR lpLibFileName, _Reserved_ HANDLE hFile, _In_ DWORD dwFlags);


// These are moved up to the header file so we can call them from elsewhere.
// We can't include d3d11.lib as a linker connection, because we are reimplementing
// all those routines, and thus get duplicate symbols.

extern "C" PFN_D3D11_CREATE_DEVICE _D3D11CreateDevice;
extern "C" PFN_D3D11_CREATE_DEVICE_AND_SWAP_CHAIN _D3D11CreateDeviceAndSwapChain;

#ifdef NTDDI_WIN10
// 11On12 requires Win 10 SDK
#include <d3d11on12.h>

extern "C" PFN_D3D11ON12_CREATE_DEVICE _D3D11On12CreateDevice;
#endif
