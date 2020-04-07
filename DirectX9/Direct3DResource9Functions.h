#pragma once
#include "d3d9Wrapper.h"
inline D3D9Wrapper::IDirect3DResource9::IDirect3DResource9(::LPDIRECT3DRESOURCE9 pResource, D3D9Wrapper::IDirect3DDevice9 * hackerDevice)
	: D3D9Wrapper::IDirect3DUnknown((IUnknown*)pResource),
	hackerDevice(hackerDevice)
{
}

void D3D9Wrapper::IDirect3DResource9::Delete()
{
	vector<::IDirect3DResource9*>::iterator it = find(hackerDevice->nvapi_registered_resources.begin(), hackerDevice->nvapi_registered_resources.end(), GetD3DResource9());

	if (it != hackerDevice->nvapi_registered_resources.end()) {
		NvAPI_D3D9_UnregisterResource(GetD3DResource9());
		hackerDevice->nvapi_registered_resources.erase(it);
	}
}