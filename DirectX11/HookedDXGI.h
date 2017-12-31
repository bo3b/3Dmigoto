#pragma once

#include <d3d11.h>
#include <dxgi.h>

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


extern "C" HRESULT (__stdcall *fnOrigCreateDXGIFactory)(
	REFIID riid,
	_Out_ void   **ppFactory
	);

extern "C" HRESULT __stdcall Hooked_CreateDXGIFactory(REFIID riid, void **ppFactory);


extern "C" HRESULT(__stdcall *fnOrigCreateDXGIFactory1)(
	REFIID riid,
	_Out_ void   **ppFactory
	);

extern "C" HRESULT __stdcall Hooked_CreateDXGIFactory1(REFIID riid, void **ppFactory1);


extern "C" LPVOID lpvtbl_CreateSwapChain(IDXGIFactory* pFactory);

extern "C" LPVOID lpvtbl_Present(IDXGISwapChain* pSwapChain);
