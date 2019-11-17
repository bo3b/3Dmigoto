#pragma once
#define CINTERFACE
#define D3D9_NO_HELPERS
#define COBJMACROS
#include <d3d9.h>
#undef COBJMACROS
#undef D3D9_NO_HELPERS
#undef CINTERFACE
::IDirect3DSwapChain9Ex* hook_swapchain(::IDirect3DSwapChain9Ex *orig_swapchain, ::IDirect3DSwapChain9Ex *hacker_swapchain);
::IDirect3DSwapChain9* hook_swapchain(::IDirect3DSwapChain9 *orig_swapchain, ::IDirect3DSwapChain9 *hacker_swapchain);
::IDirect3DSwapChain9Ex* lookup_hooked_swapchain(::IDirect3DSwapChain9Ex *orig_swapchain);
::IDirect3DSwapChain9* lookup_hooked_swapchain(::IDirect3DSwapChain9 *orig_swapchain);
