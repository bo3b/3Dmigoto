// This file is compiled as a C file only, not C++
// The reason to do this is because we can directly access the lpVtbl
// pointers for DX9 functions, by using their normal C interface.
// 
// For hooking purposes, we only actually need the exact address of any
// given function.
//
//----------------------------------------------------------------------
// Warning!  The C interface for DX9Ex is broken.  It is missing a
// routine found in the subobject, and thus compiles wrong.  Use the
// DX9 interface only. (Missing RegisterSoftwareDevice in IDirect3D9Ex)
//----------------------------------------------------------------------
//
// This is a bit weird.  By setting the CINTERFACE before including d3d9.h, we get access
// to the C style interface, which includes direct access to the vTable for the objects.
// That makes it possible to just reference the lpVtbl->CreateDevice, instead of having
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

#include <dxgi.h>


// Input object must be IDirect3D9.  If it is the subclass of IDirect3D9Ex, 
// then it will fetch the wrong address, because the header file is wrong.
// It is OK to pass in an IDirect3D9Ex, but any references here must be the subclass.

LPVOID lpvtbl_CreateSwapChain(IDXGIFactory* pFactory)
{
	if (!pFactory)
		return NULL;

	return pFactory->lpVtbl->CreateSwapChain;
}



#undef CINTERFACE
