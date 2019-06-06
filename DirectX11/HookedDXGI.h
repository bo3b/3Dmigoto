#pragma once

#include <d3d11_1.h>
#include <dxgi1_2.h>

#include "HackerDevice.h"

// -----------------------------------------------------------------------------
// This class is 'Hooked', instead of 'Hacker', because it's a hook version, instead
// of a wrapper version.  The difference is that this will only need a small number
// of calls to be hooked, instead of needing to wrap every single call the class
// supports. 
//
// This is going to use the Nektra in-proc hooking technique to hook the three
// primary factory creation calls in the dxgi.dll.  Using this technique will allow
// us access to the factory creation without having to wrap the entire thing, and
// ship a dxgi.dll as well.
// 
// Most of the dxgi.dll is full of junk calls that we don't care about, like stuff
// for DX10 and OpenGL interfaces.  
// We care specifically about CreateDXGIFactory1 and CreateDXGIFactory.
// CreateDXGIFactory2 is also moderately interesting, but only available on Win8.1,
// so we'll come back to it later.


// Called from DLLMainHook

extern "C" HRESULT (__stdcall *fnOrigCreateDXGIFactory)(
	REFIID riid,
	_Out_ void   **ppFactory
	);

extern "C" HRESULT(__stdcall *fnOrigCreateDXGIFactory1)(
	REFIID riid,
	_Out_ void   **ppFactory
	);

extern "C" HRESULT(__stdcall *fnOrigCreateDXGIFactory2)(
	UINT Flags,
	REFIID riid,
	_Out_ void   **ppFactory
	);


// Called from d3d11Wrapper for CreateDeviceAndSwapChain

void override_swap_chain(DXGI_SWAP_CHAIN_DESC *pDesc, DXGI_SWAP_CHAIN_DESC *origSwapChainDesc);
void wrap_swap_chain(HackerDevice *hackerDevice,
		IDXGISwapChain **ppSwapChain,
		DXGI_SWAP_CHAIN_DESC *overrideSwapChainDesc,
		DXGI_SWAP_CHAIN_DESC *origSwapChainDesc);

// Called from HookedDXGI

extern "C" HRESULT __stdcall Hooked_CreateDXGIFactory(REFIID riid, void **ppFactory);

extern "C" HRESULT __stdcall Hooked_CreateDXGIFactory1(REFIID riid, void **ppFactory1);

extern "C" HRESULT __stdcall Hooked_CreateDXGIFactory2(UINT Flags, REFIID riid, void **ppFactory1);


extern "C" LPVOID lpvtbl_QueryInterface(IDXGIFactory* pFactory);

extern "C" LPVOID lpvtbl_CreateSwapChain(IDXGIFactory* pFactory);

extern "C" LPVOID lpvtbl_CreateSwapChainForHwnd(IDXGIFactory2* pFactory2);

extern "C" LPVOID lpvtbl_CreateSwapChainForCoreWindow(IDXGIFactory2* pFactory2);

extern "C" LPVOID lpvtbl_CreateSwapChainForComposition(IDXGIFactory2* pFactory2);

extern "C" LPVOID lpvtbl_Present(IDXGISwapChain* pSwapChain);

extern "C" LPVOID lpvtbl_Present1(IDXGISwapChain1* pSwapChain1);

