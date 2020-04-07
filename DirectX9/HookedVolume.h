#pragma once
#define CINTERFACE
#define D3D9_NO_HELPERS
#define COBJMACROS
#include <d3d9.h>
#undef COBJMACROS
#undef D3D9_NO_HELPERS
#undef CINTERFACE
::IDirect3DVolume9* hook_volume(::IDirect3DVolume9 *orig_volume, ::IDirect3DVolume9 *hacker_volume);
::IDirect3DVolume9* lookup_hooked_volume(::IDirect3DVolume9 *orig_volume);
