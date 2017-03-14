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



// Cannot be declared static, even though that is what I want, because of some
// twisted C++ concept of keeping that local to only this translation unit, .h file.
// All non-member functions are implicitly static, and I hate implict stuff.  

bool InstallDXGIHooks(void);

void HookSwapChain(IDXGISwapChain* pSwapChain, ID3D11Device* pDevice, ID3D11DeviceContext* pContext);



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

