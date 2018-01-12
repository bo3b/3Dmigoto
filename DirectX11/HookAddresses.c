// This file is compiled as a C file only, not C++
// The reason to do this is because we can directly access the lpVtbl
// pointers for DX11 functions, by using their normal C interface.
// 
// For hooking purposes, we only actually need the exact address of any
// given function.
//
// This is a bit weird.  By setting the CINTERFACE before including dxgi1_2.h, we get access
// to the C style interface, which includes direct access to the vTable for the objects.
// That makes it possible to just reference the lpVtbl->CreateSwapChain, instead of having
// magic constants, and multiple casts to fetch the address of the CreateDevice routine.
//
// There is no other legal way to fetch these addresses, as described in the Detours
// header file.  Nektra samples just use hardcoded offsets and raw pointers.
// https://stackoverflow.com/questions/8121320/get-memory-address-of-member-function
//
// This can only be included here where it's used to fetch those routine addresses, because
// it will make other C++ units fail to compile, like NativePlugin.cpp, so this is 
// separated into this different compilation unit.

#define CINTERFACE

// This adds debug stack and routine info, very helpful for source level object view.
#ifdef _DEBUG
#define D3D_DEBUG_INFO
#endif

#include <dxgi1_2.h>


LPVOID lpvtbl_QueryInterface(IDXGIFactory* pFactory)
{
	if (!pFactory)
		return NULL;

	return pFactory->lpVtbl->QueryInterface;
}

LPVOID lpvtbl_CreateSwapChain(IDXGIFactory* pFactory)
{
	if (!pFactory)
		return NULL;

	return pFactory->lpVtbl->CreateSwapChain;
}

LPVOID lpvtbl_CreateSwapChainForHwnd(IDXGIFactory2* pFactory2)
{
	if (!pFactory2)
		return NULL;

	return pFactory2->lpVtbl->CreateSwapChainForHwnd;
}

LPVOID lpvtbl_CreateSwapChainForComposition(IDXGIFactory2* pFactory2)
{
	if (!pFactory2)
		return NULL;

	return pFactory2->lpVtbl->CreateSwapChainForComposition;
}

LPVOID lpvtbl_CreateSwapChainForCoreWindow(IDXGIFactory2* pFactory2)
{
	if (!pFactory2)
		return NULL;

	return pFactory2->lpVtbl->CreateSwapChainForCoreWindow;
}

LPVOID lpvtbl_Present(IDXGISwapChain* pSwapChain)
{
	if (!pSwapChain)
		return NULL;

	return pSwapChain->lpVtbl->Present;
}

LPVOID lpvtbl_Present1(IDXGISwapChain1* pSwapChain1)
{
	if (!pSwapChain1)
		return NULL;

	return pSwapChain1->lpVtbl->Present1;
}



#undef CINTERFACE
