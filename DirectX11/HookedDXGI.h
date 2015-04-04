#pragma once

// For this object, we want to use the CINTERFACE, not the C++ interface.
// The reason is that it allows us easy access to the COM object vtable, which
// we need to hook in order to override the functions.  Once we include dxgi.h
// it will be defined with C headers instead.

// This is a little odd, but it's the way that Detours hooks COM objects, and
// thus it seems superior to the Nektra approach of groping the vtable directly
// using constants.

#define CINTERFACE
#define COBJMACROS

#include <dxgi.h>

// Cannot be declared static, even though that is what I want, because of some
// twisted C++ concept of keeping that local to only this translation unit, .h file.
// All non-member functions are implicitly static, and I hate implict stuff.  

bool InstallDXGIHooks(void);


// This class is 'Hooked', instead of 'Hacker', because it's a hook version, instead
// of a wrapper version.  The difference is that this will only need a small number
// of calls to be hooked, instead of needing to wrap every single call the class
// supports.  It also specifically does not descend from the IDXGISwapChain class.
//
// It needs to be a class, because we can have multiple instantiations of SwapChains,
// and we need to use the right instance at the right time.
//
// We will use the same static Present call for each instance though, because the
// underlying code is the same in every case.

class HookedSwapChain
{
private:
	IDXGISwapChain* mOrigSwapChain;

public:
	HookedSwapChain(IDXGISwapChain* pOrigSwapChain);
	~HookedSwapChain();

};

