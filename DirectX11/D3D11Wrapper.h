#pragma once

#include <Windows.h>
#include <d3d11_1.h>
#include <dxgi1_2.h>


void InitD311();

void NvAPIOverride();

extern "C" HMODULE (__stdcall *fnOrigLoadLibraryExW)(
	_In_       LPCTSTR lpFileName,
	_Reserved_ HANDLE  hFile,
	_In_       DWORD   dwFlags
	);

extern "C" HMODULE __stdcall Hooked_LoadLibraryExW(_In_ LPCWSTR lpLibFileName, _Reserved_ HANDLE hFile, _In_ DWORD dwFlags);


// These are moved up to the header file so we can call them from elsewhere.
// We can't include d3d11.lib as a linker connection, because we are reimplementing
// all those routines, and thus get duplicate symbols.

typedef HRESULT(WINAPI *tD3D11CreateDevice)(
	IDXGIAdapter *pAdapter,
	D3D_DRIVER_TYPE DriverType,
	HMODULE Software,
	UINT Flags,
	const D3D_FEATURE_LEVEL *pFeatureLevels,
	UINT FeatureLevels,
	UINT SDKVersion,
	ID3D11Device **ppDevice,
	D3D_FEATURE_LEVEL *pFeatureLevel,
	ID3D11DeviceContext **ppImmediateContext);
extern "C" tD3D11CreateDevice _D3D11CreateDevice;
typedef HRESULT(WINAPI *tD3D11CreateDeviceAndSwapChain)(
	IDXGIAdapter *pAdapter,
	D3D_DRIVER_TYPE DriverType,
	HMODULE Software,
	UINT Flags,
	const D3D_FEATURE_LEVEL *pFeatureLevels,
	UINT FeatureLevels,
	UINT SDKVersion,
	DXGI_SWAP_CHAIN_DESC *pSwapChainDesc,
	IDXGISwapChain **ppSwapChain,
	ID3D11Device **ppDevice,
	D3D_FEATURE_LEVEL *pFeatureLevel,
	ID3D11DeviceContext **ppImmediateContext);
extern "C" tD3D11CreateDeviceAndSwapChain _D3D11CreateDeviceAndSwapChain;

