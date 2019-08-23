#pragma once
#define CINTERFACE
#define D3D9_NO_HELPERS
#define COBJMACROS
namespace D3D9Base
{

#include <d3d9.h>

}
#undef COBJMACROS
#undef D3D9_NO_HELPERS
#undef CINTERFACE
D3D9Base::IDirect3DSwapChain9Ex* hook_swapchain(D3D9Base::IDirect3DSwapChain9Ex *orig_swapchain, D3D9Base::IDirect3DSwapChain9Ex *hacker_swapchain);
D3D9Base::IDirect3DSwapChain9* hook_swapchain(D3D9Base::IDirect3DSwapChain9 *orig_swapchain, D3D9Base::IDirect3DSwapChain9 *hacker_swapchain);
D3D9Base::IDirect3DSwapChain9Ex* lookup_hooked_swapchain(D3D9Base::IDirect3DSwapChain9Ex *orig_swapchain);
D3D9Base::IDirect3DSwapChain9* lookup_hooked_swapchain(D3D9Base::IDirect3DSwapChain9 *orig_swapchain);