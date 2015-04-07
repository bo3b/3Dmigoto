#pragma once

#include <d3d11.h>
#include <dxgi.h>

#include "Overlay.h"


// Cannot be declared static, even though that is what I want, because of some
// twisted C++ concept of keeping that local to only this translation unit, .h file.
// All non-member functions are implicitly static, and I hate implict stuff.  

bool InstallDXGIHooks(void);

void HookSwapChain(IDXGISwapChain* pSwapChain, ID3D11Device* pDevice, ID3D11DeviceContext* pContext);



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

//class HookedSwapChain
//{
//private:
//	IDXGISwapChain* mOrigSwapChain;
//
//public:
//	HookedSwapChain(IDXGISwapChain* pOrigSwapChain);
//	~HookedSwapChain();
//
//};

