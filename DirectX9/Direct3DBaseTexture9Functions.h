#include "d3d9Wrapper.h"
#pragma once
D3D9Wrapper::IDirect3DBaseTexture9::IDirect3DBaseTexture9(::LPDIRECT3DBASETEXTURE9 pTexture, D3D9Wrapper::IDirect3DDevice9 *hackerDevice, TextureType type)
	: D3D9Wrapper::IDirect3DResource9((::LPDIRECT3DRESOURCE9)pTexture, hackerDevice),
	pendingCreateTexture(false),
	pendingLockUnlock(false),
	magic(0x7da43feb),
	texType(type),
	shared_ref_count(1),
	zero_d3d_ref_count(false)
{

}
void D3D9Wrapper::IDirect3DBaseTexture9::Delete()
{
	switch (texType) {
	case TextureType::Texture2D:
		reinterpret_cast<D3D9Wrapper::IDirect3DTexture9*>(this)->Delete();
		break;
	case TextureType::Cube:
		reinterpret_cast<D3D9Wrapper::IDirect3DCubeTexture9*>(this)->Delete();
		break;
	case TextureType::Volume:
		reinterpret_cast<D3D9Wrapper::IDirect3DVolumeTexture9*>(this)->Delete();
		break;
	}
}
void D3D9Wrapper::IDirect3DBaseTexture9::Bound(DWORD Stage)
{
	LogInfo("IDirect3DSurface9::Bound stage=%u handle=%p counter=%d this=%p\n", Stage, m_pUnk, m_ulRef, this);

	bound.insert(Stage);
}
void D3D9Wrapper::IDirect3DBaseTexture9::Unbound(DWORD Stage)
{
	LogInfo("IDirect3DSurface9::Unbound stage=%u handle=%p counter=%d this=%p\n", Stage, m_pUnk, m_ulRef, this);

	bound.erase(Stage);
	if (shared_ref_count == 0)
	{
		switch (texType) {
		case TextureType::Texture2D:
			reinterpret_cast<D3D9Wrapper::IDirect3DTexture9*>(this)->Delete();
			break;
		case TextureType::Cube:
			reinterpret_cast<D3D9Wrapper::IDirect3DCubeTexture9*>(this)->Delete();
			break;
		case TextureType::Volume:
			reinterpret_cast<D3D9Wrapper::IDirect3DVolumeTexture9*>(this)->Delete();
			break;
		};
	}
}
