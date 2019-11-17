#include "HookedD3DX.h"

HRESULT WINAPI Hooked_D3DXComputeNormalMap(
	_Out_       D3D9Wrapper::IDirect3DTexture9* pTexture,
	_In_        D3D9Wrapper::IDirect3DTexture9* pSrcTexture,
	_In_  const PALETTEENTRY       *pSrcPalette,
	_In_        DWORD              Flags,
	_In_        DWORD              Channel,
	_In_        FLOAT              Amplitude) {

	LogInfo("Hooked_D3DXComputeNormalMap called with SourceTexture=%p, DestinationTexture=%p\n", pSrcTexture, pTexture);

	::IDirect3DTexture9 *baseSourceTexture = baseTexture9(pSrcTexture);
	::IDirect3DTexture9 *baseDestTexture = baseTexture9(pTexture);
	D3D9Wrapper::IDirect3DTexture9 *wrappedSource = wrappedTexture9(pSrcTexture);
	D3D9Wrapper::IDirect3DTexture9 *wrappedDest = wrappedTexture9(pTexture);

	if (G->hunting && G->track_texture_updates != 2) { // Any hunting mode - want to catch hash contamination even while soft disabled
		MarkResourceHashContaminated(wrappedDest, 0, wrappedSource, 0, 'C', NULL, NULL);
	}

	HRESULT hr;
	if (G->gForceStereo == 2) {

		::IDirect3DTexture9* pSourceTextureLeft = wrappedSource->DirectModeGetLeft();
		::IDirect3DTexture9* pSourceTextureRight = wrappedSource->DirectModeGetRight();
		::IDirect3DTexture9* pDestTextureLeft = wrappedDest->DirectModeGetLeft();
		::IDirect3DTexture9* pDestTextureRight = wrappedDest->DirectModeGetRight();

		hr = trampoline_D3DXComputeNormalMap(pDestTextureLeft, pSourceTextureLeft, pSrcPalette, Flags, Channel, Amplitude);
		if (SUCCEEDED(hr)) {
			if (!pSourceTextureRight && pDestTextureRight) {
				LogDebug("Hooked_D3DXComputeNormalMap Direct Mode, INFO: Source is not stereo, destination is stereo. Copying source to both sides of destination.\n");
				hr = trampoline_D3DXComputeNormalMap(pDestTextureRight, pSourceTextureLeft, pSrcPalette, Flags, Channel, Amplitude);
				if (FAILED(hr))
					LogDebug("ERROR: Hooked_D3DXComputeNormalMap - Failed to copy source left to destination right.\n");
			}
			else if (pSourceTextureRight && !pDestTextureRight) {
				LogDebug("Hooked_D3DXComputeNormalMap Direct Mode, INFO:  Source is stereo, destination is not stereo. Copied Left side only.\n");
			}
			else if (pSourceTextureRight && pDestTextureRight) {
				hr = trampoline_D3DXComputeNormalMap(pDestTextureRight, pSourceTextureRight, pSrcPalette, Flags, Channel, Amplitude);
				if (FAILED(hr))
					LogDebug("Hooked_D3DXComputeNormalMap Direct Mode, ERROR: Failed to copy source right to destination right.\n");
			}
		}
	}
	else {
		hr = trampoline_D3DXComputeNormalMap(baseDestTexture, baseSourceTexture, pSrcPalette, Flags, Channel, Amplitude);
	}

	if (G->track_texture_updates == 1)
		PropagateResourceHash(wrappedDest, wrappedSource);

	LogInfo("  returns result=%x\n", hr);

	return hr;
}

HRESULT WINAPI Hooked_D3DXCreateCubeTexture(
	_In_  D3D9Wrapper::IDirect3DDevice9*      pDevice,
	_In_  UINT                   Size,
	_In_  UINT                   MipLevels,
	_In_  DWORD                  Usage,
	_In_  ::D3DFORMAT              Format,
	_In_  ::D3DPOOL                Pool,
	_Out_ D3D9Wrapper::IDirect3DCubeTexture9* *ppCubeTexture)
{

	LogInfo("Hooked_D3DXCreateCubeTexture EdgeLength=%d Format=%d\n", Size, Format);
	D3D9Wrapper::IDirect3DDevice9 *wrappedDevice = wrappedDevice9(pDevice);

	if (!wrappedDevice->GetD3D9Device())
	{
		LogInfo("  postponing call because device was not created yet.\n");

		D3D9Wrapper::IDirect3DCubeTexture9 *wrapper;
		wrapper = D3D9Wrapper::IDirect3DCubeTexture9::GetDirect3DCubeTexture9((::LPDIRECT3DCUBETEXTURE9) 0, wrappedDevice, NULL);
		wrapper->_EdgeLength = Size;
		wrapper->_Levels = MipLevels;
		wrapper->_Usage = Usage;
		wrapper->_Format = Format;
		wrapper->_Pool = Pool;
		wrapper->_Device = wrappedDevice;
		wrapper->pendingCreateTexture = true;
		*ppCubeTexture = wrapper;
		LogInfo("  returns handle=%p\n", wrapper);
		return S_OK;
	}

	if (Pool == ::D3DPOOL_MANAGED)
	{
		LogDebug("  Pool changed from MANAGED to DEFAULT because of DirectX9Ex migration.\n");

		Pool = ::D3DPOOL_DEFAULT;
		if (!(Usage & D3DUSAGE_DYNAMIC))
			Usage = Usage | D3DUSAGE_DYNAMIC;
	}

	D3D2DTEXTURE_DESC pDesc;// = {};
	pDesc.Width = Size;
	pDesc.Height = Size;
	pDesc.Levels = MipLevels;
	pDesc.Usage = Usage;
	pDesc.Format = Format;
	pDesc.Pool = Pool;

	::LPDIRECT3DCUBETEXTURE9 baseTexture = 0;
	HRESULT hr;
	D3D9Wrapper::IDirect3DCubeTexture9 *wrapper;
	if (G->gForceStereo == 2) {
		::IDirect3DCubeTexture9* pRightTexture = NULL;
		hr = trampoline_D3DXCreateCubeTexture(wrappedDevice->GetD3D9Device(), Size, MipLevels, Usage, Format, Pool, &baseTexture);
		if (!FAILED(hr) && (ShouldDuplicate(&pDesc))) {
			hr = trampoline_D3DXCreateCubeTexture(wrappedDevice->GetD3D9Device(), Size, MipLevels, Usage, Format, Pool, &pRightTexture);
			if (!FAILED(hr)) {
				wrapper = D3D9Wrapper::IDirect3DCubeTexture9::GetDirect3DCubeTexture9(baseTexture, wrappedDevice, pRightTexture);
			}
			else {
				LogDebug("Hooked_D3DXCreateCubeTexture Direct Mode, failed to create right texture, falling back to mono, hr = %d", hr);
				wrapper = D3D9Wrapper::IDirect3DCubeTexture9::GetDirect3DCubeTexture9(baseTexture, wrappedDevice, NULL);
			}
		}
		else {
			if (FAILED(hr))
				LogDebug("Hooked_D3DXCreateCubeTexture Direct Mode, failed to create left surface, hr = %d ", hr);
			wrapper = D3D9Wrapper::IDirect3DCubeTexture9::GetDirect3DCubeTexture9(baseTexture, wrappedDevice, NULL);
		}
	}
	else {
		hr = trampoline_D3DXCreateCubeTexture(wrappedDevice->GetD3D9Device(), Size, MipLevels, Usage, Format, Pool, &baseTexture);
		wrapper = D3D9Wrapper::IDirect3DCubeTexture9::GetDirect3DCubeTexture9(baseTexture, wrappedDevice, NULL);
	}

	if (ppCubeTexture) {
		if (!(G->enable_hooks >= EnableHooksDX9::ALL) && wrapper) {
			*ppCubeTexture = wrapper;
		}
		else {
			*ppCubeTexture = reinterpret_cast<D3D9Wrapper::IDirect3DCubeTexture9*>(baseTexture);
		}
	}

	if (ppCubeTexture) LogDebug("  returns result=%x, handle=%p, wrapper=%p\n", hr, baseTexture, *ppCubeTexture);

	LogInfo("  returns result=%x\n", hr);

	return hr;
}

HRESULT WINAPI Hooked_D3DXCreateCubeTextureFromFile(
	_In_  D3D9Wrapper::IDirect3DDevice9*      pDevice,
	_In_  LPCTSTR                pSrcFile,
	_Out_ D3D9Wrapper::IDirect3DCubeTexture9* *ppCubeTexture) {
	LogInfo("Hooked_D3DXCreateCubeTextureFromFile EdgeLength=%d Format=%d\n", D3DX_DEFAULT, ::D3DFMT_UNKNOWN);
	D3D9Wrapper::IDirect3DDevice9 *wrappedDevice = wrappedDevice9(pDevice);

	if (!wrappedDevice->GetD3D9Device())
	{
		LogInfo("  postponing call because device was not created yet.\n");

		D3D9Wrapper::IDirect3DCubeTexture9 *wrapper;
		wrapper = D3D9Wrapper::IDirect3DCubeTexture9::GetDirect3DCubeTexture9((::LPDIRECT3DCUBETEXTURE9) 0, wrappedDevice, NULL);
		wrapper->_EdgeLength = D3DX_DEFAULT;
		wrapper->_Levels = D3DX_DEFAULT;
		wrapper->_Usage = 0;
		wrapper->_Format = ::D3DFMT_UNKNOWN;
		wrapper->_Pool = ::D3DPOOL_DEFAULT;
		wrapper->_Device = wrappedDevice;
		wrapper->pendingCreateTexture = true;
		*ppCubeTexture = wrapper;
		LogInfo("  returns handle=%p\n", wrapper);
		return S_OK;
	}
	D3D2DTEXTURE_DESC pDesc;
	pDesc.Width = D3DX_DEFAULT;
	pDesc.Height = D3DX_DEFAULT;
	pDesc.Levels = D3DX_DEFAULT;
	pDesc.Usage = 0;
	pDesc.Format = ::D3DFMT_UNKNOWN;
	pDesc.Pool = ::D3DPOOL_DEFAULT;

	::LPDIRECT3DCUBETEXTURE9 baseTexture = 0;
	HRESULT hr;
	D3D9Wrapper::IDirect3DCubeTexture9 *wrapper;
	if (G->gForceStereo == 2) {
		::IDirect3DCubeTexture9* pRightTexture = NULL;
		hr = trampoline_D3DXCreateCubeTextureFromFile(wrappedDevice->GetD3D9Device(), pSrcFile, &baseTexture);
		if (!FAILED(hr) && (ShouldDuplicate(&pDesc))) {
			hr = trampoline_D3DXCreateCubeTextureFromFile(wrappedDevice->GetD3D9Device(), pSrcFile, &pRightTexture);
			if (!FAILED(hr)) {
				wrapper = D3D9Wrapper::IDirect3DCubeTexture9::GetDirect3DCubeTexture9(baseTexture, wrappedDevice, pRightTexture);
			}
			else {
				LogDebug("Hooked_D3DXCreateCubeTextureFromFile Direct Mode, failed to create right texture, falling back to mono, hr = %d", hr);
				wrapper = D3D9Wrapper::IDirect3DCubeTexture9::GetDirect3DCubeTexture9(baseTexture, wrappedDevice, NULL);
			}
		}
		else {
			if (FAILED(hr))
				LogDebug("Hooked_D3DXCreateCubeTextureFromFile Direct Mode, failed to create left surface, hr = %d ", hr);
			wrapper = D3D9Wrapper::IDirect3DCubeTexture9::GetDirect3DCubeTexture9(baseTexture, wrappedDevice, NULL);
		}
	}
	else {
		hr = trampoline_D3DXCreateCubeTextureFromFile(wrappedDevice->GetD3D9Device(), pSrcFile, &baseTexture);
		wrapper = D3D9Wrapper::IDirect3DCubeTexture9::GetDirect3DCubeTexture9(baseTexture, wrappedDevice, NULL);
	}

	if (ppCubeTexture) {
		if (!(G->enable_hooks >= EnableHooksDX9::ALL) && wrapper) {
			*ppCubeTexture = wrapper;
		}
		else {
			*ppCubeTexture = reinterpret_cast<D3D9Wrapper::IDirect3DCubeTexture9*>(baseTexture);
		}
	}

	if (ppCubeTexture) LogDebug("  returns result=%x, handle=%p, wrapper=%p\n", hr, baseTexture, *ppCubeTexture);

	LogInfo("  returns result=%x\n", hr);

	return hr;
}

HRESULT WINAPI Hooked_D3DXCreateCubeTextureFromFileEx(
	_In_  D3D9Wrapper::IDirect3DDevice9*      pDevice,
	_In_  LPCTSTR                pSrcFile,
	_In_  UINT                   Size,
	_In_  UINT                   MipLevels,
	_In_  DWORD                  Usage,
	_In_  ::D3DFORMAT              Format,
	_In_  ::D3DPOOL                Pool,
	_In_  DWORD                  Filter,
	_In_  DWORD                  MipFilter,
	_In_  ::D3DCOLOR               ColorKey,
	_Out_ ::D3DXIMAGE_INFO         *pSrcInfo,
	_Out_ PALETTEENTRY           *pPalette,
	_Out_ D3D9Wrapper::IDirect3DCubeTexture9* *ppCubeTexture) {
	LogInfo("Hooked_D3DXCreateCubeTextureFromFileEx EdgeLength=%d Format=%d\n", Size, Format);
	D3D9Wrapper::IDirect3DDevice9 *wrappedDevice = wrappedDevice9(pDevice);

	if (!wrappedDevice->GetD3D9Device())
	{
		LogInfo("  postponing call because device was not created yet.\n");

		D3D9Wrapper::IDirect3DCubeTexture9 *wrapper;
		wrapper = D3D9Wrapper::IDirect3DCubeTexture9::GetDirect3DCubeTexture9((::LPDIRECT3DCUBETEXTURE9) 0, wrappedDevice, NULL);
		wrapper->_EdgeLength = Size;
		wrapper->_Levels = MipLevels;
		wrapper->_Usage = Usage;
		wrapper->_Format = Format;
		wrapper->_Pool = Pool;
		wrapper->_Device = wrappedDevice;
		wrapper->pendingCreateTexture = true;
		*ppCubeTexture = wrapper;
		LogInfo("  returns handle=%p\n", wrapper);
		return S_OK;
	}

	if (Pool == ::D3DPOOL_MANAGED)
	{
		LogDebug("  Pool changed from MANAGED to DEFAULT because of DirectX9Ex migration.\n");

		Pool = ::D3DPOOL_DEFAULT;
		if (!(Usage & D3DUSAGE_DYNAMIC))
			Usage = Usage | D3DUSAGE_DYNAMIC;
	}

	D3D2DTEXTURE_DESC pDesc;
	pDesc.Width = Size;
	pDesc.Height = Size;
	pDesc.Levels = MipLevels;
	pDesc.Usage = Usage;
	pDesc.Format = Format;
	pDesc.Pool = ::D3DPOOL_DEFAULT;

	::LPDIRECT3DCUBETEXTURE9 baseTexture = 0;
	HRESULT hr;
	D3D9Wrapper::IDirect3DCubeTexture9 *wrapper;
	if (G->gForceStereo == 2) {
		::IDirect3DCubeTexture9* pRightTexture = NULL;
		hr = trampoline_D3DXCreateCubeTextureFromFileEx(wrappedDevice->GetD3D9Device(), pSrcFile, Size, MipLevels, Usage, Format, Pool, Filter, MipFilter, ColorKey, pSrcInfo, pPalette, &baseTexture);
		if (!FAILED(hr) && (ShouldDuplicate(&pDesc))) {
			hr = trampoline_D3DXCreateCubeTextureFromFileEx(wrappedDevice->GetD3D9Device(), pSrcFile, Size, MipLevels, Usage, Format, Pool, Filter, MipFilter, ColorKey, pSrcInfo, pPalette, &pRightTexture);
			if (!FAILED(hr)) {
				wrapper = D3D9Wrapper::IDirect3DCubeTexture9::GetDirect3DCubeTexture9(baseTexture, wrappedDevice, pRightTexture);
			}
			else {
				LogDebug("Hooked_D3DXCreateCubeTextureFromFileEx Direct Mode, failed to create right texture, falling back to mono, hr = %d", hr);
				wrapper = D3D9Wrapper::IDirect3DCubeTexture9::GetDirect3DCubeTexture9(baseTexture, wrappedDevice, NULL);
			}
		}
		else {
			if (FAILED(hr))
				LogDebug("Hooked_D3DXCreateCubeTextureFromFileEx Direct Mode, failed to create left surface, hr = %d ", hr);
			wrapper = D3D9Wrapper::IDirect3DCubeTexture9::GetDirect3DCubeTexture9(baseTexture, wrappedDevice, NULL);
		}
	}
	else {
		hr = trampoline_D3DXCreateCubeTextureFromFileEx(wrappedDevice->GetD3D9Device(), pSrcFile, Size, MipLevels, Usage, Format, Pool, Filter, MipFilter, ColorKey, pSrcInfo, pPalette, &baseTexture);
		wrapper = D3D9Wrapper::IDirect3DCubeTexture9::GetDirect3DCubeTexture9(baseTexture, wrappedDevice, NULL);
	}

	if (ppCubeTexture) {
		if (!(G->enable_hooks >= EnableHooksDX9::ALL) && wrapper) {
			*ppCubeTexture = wrapper;
		}
		else {
			*ppCubeTexture = reinterpret_cast<D3D9Wrapper::IDirect3DCubeTexture9*>(baseTexture);
		}
	}

	if (ppCubeTexture) LogDebug("  returns result=%x, handle=%p, wrapper=%p\n", hr, baseTexture, *ppCubeTexture);

	LogInfo("  returns result=%x\n", hr);

	return hr;
}

HRESULT WINAPI Hooked_D3DXCreateCubeTextureFromFileInMemory(
	_In_  D3D9Wrapper::IDirect3DDevice9*      pDevice,
	_In_  LPCVOID                pSrcData,
	_In_  UINT                   SrcDataSize,
	_Out_ D3D9Wrapper::IDirect3DCubeTexture9* *ppCubeTexture) {
	LogInfo("Hooked_D3DXCreateCubeTextureFromFileInMemory EdgeLength=%d Format=%d\n", D3DX_DEFAULT, ::D3DFMT_UNKNOWN);
	D3D9Wrapper::IDirect3DDevice9 *wrappedDevice = wrappedDevice9(pDevice);

	if (!wrappedDevice->GetD3D9Device())
	{
		LogInfo("  postponing call because device was not created yet.\n");

		D3D9Wrapper::IDirect3DCubeTexture9 *wrapper;
		wrapper = D3D9Wrapper::IDirect3DCubeTexture9::GetDirect3DCubeTexture9((::LPDIRECT3DCUBETEXTURE9) 0, wrappedDevice, NULL);
		wrapper->_EdgeLength = D3DX_DEFAULT;
		wrapper->_Levels = D3DX_DEFAULT;
		wrapper->_Usage = 0;
		wrapper->_Format = ::D3DFMT_UNKNOWN;
		wrapper->_Pool = ::D3DPOOL_DEFAULT;
		wrapper->_Device = wrappedDevice;
		wrapper->pendingCreateTexture = true;
		*ppCubeTexture = wrapper;
		LogInfo("  returns handle=%p\n", wrapper);
		return S_OK;
	}
	D3D2DTEXTURE_DESC pDesc;
	pDesc.Width = D3DX_DEFAULT;
	pDesc.Height = D3DX_DEFAULT;
	pDesc.Levels = D3DX_DEFAULT;
	pDesc.Usage = 0;
	pDesc.Format = ::D3DFMT_UNKNOWN;
	pDesc.Pool = ::D3DPOOL_DEFAULT;

	::LPDIRECT3DCUBETEXTURE9 baseTexture = 0;
	HRESULT hr;
	D3D9Wrapper::IDirect3DCubeTexture9 *wrapper;
	if (G->gForceStereo == 2) {
		::IDirect3DCubeTexture9* pRightTexture = NULL;
		hr = trampoline_D3DXCreateCubeTextureFromFileInMemory(wrappedDevice->GetD3D9Device(), pSrcData, SrcDataSize, &baseTexture);
		if (!FAILED(hr) && (ShouldDuplicate(&pDesc))) {
			hr = trampoline_D3DXCreateCubeTextureFromFileInMemory(wrappedDevice->GetD3D9Device(), pSrcData, SrcDataSize, &pRightTexture);
			if (!FAILED(hr)) {
				wrapper = D3D9Wrapper::IDirect3DCubeTexture9::GetDirect3DCubeTexture9(baseTexture, wrappedDevice, pRightTexture);
			}
			else {
				LogDebug("Hooked_D3DXCreateCubeTextureFromFileInMemory Direct Mode, failed to create right texture, falling back to mono, hr = %d", hr);
				wrapper = D3D9Wrapper::IDirect3DCubeTexture9::GetDirect3DCubeTexture9(baseTexture, wrappedDevice, NULL);
			}
		}
		else {
			if (FAILED(hr))
				LogDebug("Hooked_D3DXCreateCubeTextureFromFileInMemory Direct Mode, failed to create left surface, hr = %d ", hr);
			wrapper = D3D9Wrapper::IDirect3DCubeTexture9::GetDirect3DCubeTexture9(baseTexture, wrappedDevice, NULL);
		}
	}
	else {
		hr = trampoline_D3DXCreateCubeTextureFromFileInMemory(wrappedDevice->GetD3D9Device(), pSrcData, SrcDataSize, &baseTexture);
		wrapper = D3D9Wrapper::IDirect3DCubeTexture9::GetDirect3DCubeTexture9(baseTexture, wrappedDevice, NULL);
	}

	if (ppCubeTexture) {
		if (!(G->enable_hooks >= EnableHooksDX9::ALL) && wrapper) {
			*ppCubeTexture = wrapper;
		}
		else {
			*ppCubeTexture = reinterpret_cast<D3D9Wrapper::IDirect3DCubeTexture9*>(baseTexture);
		}
	}

	if (ppCubeTexture) LogDebug("  returns result=%x, handle=%p, wrapper=%p\n", hr, baseTexture, *ppCubeTexture);

	LogInfo("  returns result=%x\n", hr);

	return hr;
}

HRESULT WINAPI Hooked_D3DXCreateCubeTextureFromFileInMemoryEx(
	_In_    D3D9Wrapper::IDirect3DDevice9*      pDevice,
	_In_    LPCVOID                pSrcData,
	_In_    UINT                   SrcDataSize,
	_In_    UINT                   Size,
	_In_    UINT                   MipLevels,
	_In_    DWORD                  Usage,
	_In_    ::D3DFORMAT              Format,
	_In_    ::D3DPOOL                Pool,
	_In_    DWORD                  Filter,
	_In_    DWORD                  MipFilter,
	_In_    ::D3DCOLOR               ColorKey,
	_Inout_ ::D3DXIMAGE_INFO         *pSrcInfo,
	_Out_   PALETTEENTRY           *pPalette,
	_Out_   D3D9Wrapper::IDirect3DCubeTexture9* *ppCubeTexture) {
	LogInfo("Hooked_D3DXCreateCubeTextureFromFileInMemoryEx EdgeLength=%d Format=%d\n", Size, Format);
	D3D9Wrapper::IDirect3DDevice9 *wrappedDevice = wrappedDevice9(pDevice);

	if (!wrappedDevice->GetD3D9Device())
	{
		LogInfo("  postponing call because device was not created yet.\n");

		D3D9Wrapper::IDirect3DCubeTexture9 *wrapper;
		wrapper = D3D9Wrapper::IDirect3DCubeTexture9::GetDirect3DCubeTexture9((::LPDIRECT3DCUBETEXTURE9) 0, wrappedDevice, NULL);
		wrapper->_EdgeLength = Size;
		wrapper->_Levels = MipLevels;
		wrapper->_Usage = Usage;
		wrapper->_Format = Format;
		wrapper->_Pool = Pool;
		wrapper->_Device = wrappedDevice;
		wrapper->pendingCreateTexture = true;
		*ppCubeTexture = wrapper;
		LogInfo("  returns handle=%p\n", wrapper);
		return S_OK;
	}

	if (Pool == ::D3DPOOL_MANAGED)
	{
		LogDebug("  Pool changed from MANAGED to DEFAULT because of DirectX9Ex migration.\n");

		Pool = ::D3DPOOL_DEFAULT;
		if (!(Usage & D3DUSAGE_DYNAMIC))
			Usage = Usage | D3DUSAGE_DYNAMIC;
	}

	D3D2DTEXTURE_DESC pDesc;
	pDesc.Width = Size;
	pDesc.Height = Size;
	pDesc.Levels = MipLevels;
	pDesc.Usage = Usage;
	pDesc.Format = Format;
	pDesc.Pool = ::D3DPOOL_DEFAULT;

	::LPDIRECT3DCUBETEXTURE9 baseTexture = 0;
	HRESULT hr;
	D3D9Wrapper::IDirect3DCubeTexture9 *wrapper;
	if (G->gForceStereo == 2) {
		::IDirect3DCubeTexture9* pRightTexture = NULL;
		hr = trampoline_D3DXCreateCubeTextureFromFileInMemoryEx(wrappedDevice->GetD3D9Device(), pSrcData, SrcDataSize, Size, MipLevels, Usage, Format, Pool, Filter, MipFilter, ColorKey, pSrcInfo, pPalette, &baseTexture);
		if (!FAILED(hr) && (ShouldDuplicate(&pDesc))) {
			hr = trampoline_D3DXCreateCubeTextureFromFileInMemoryEx(wrappedDevice->GetD3D9Device(), pSrcData, SrcDataSize, Size, MipLevels, Usage, Format, Pool, Filter, MipFilter, ColorKey, pSrcInfo, pPalette, &pRightTexture);
			if (!FAILED(hr)) {
				wrapper = D3D9Wrapper::IDirect3DCubeTexture9::GetDirect3DCubeTexture9(baseTexture, wrappedDevice, pRightTexture);
			}
			else {
				LogDebug("Hooked_D3DXCreateCubeTextureFromFileInMemoryEx Direct Mode, failed to create right texture, falling back to mono, hr = %d", hr);
				wrapper = D3D9Wrapper::IDirect3DCubeTexture9::GetDirect3DCubeTexture9(baseTexture, wrappedDevice, NULL);
			}
		}
		else {
			if (FAILED(hr))
				LogDebug("Hooked_D3DXCreateCubeTextureFromFileInMemoryEx Direct Mode, failed to create left surface, hr = %d ", hr);
			wrapper = D3D9Wrapper::IDirect3DCubeTexture9::GetDirect3DCubeTexture9(baseTexture, wrappedDevice, NULL);
		}
	}
	else {
		hr = trampoline_D3DXCreateCubeTextureFromFileInMemoryEx(wrappedDevice->GetD3D9Device(), pSrcData, SrcDataSize, Size, MipLevels, Usage, Format, Pool, Filter, MipFilter, ColorKey, pSrcInfo, pPalette, &baseTexture);
		wrapper = D3D9Wrapper::IDirect3DCubeTexture9::GetDirect3DCubeTexture9(baseTexture, wrappedDevice, NULL);
	}

	if (ppCubeTexture) {
		if (!(G->enable_hooks >= EnableHooksDX9::ALL) && wrapper) {
			*ppCubeTexture = wrapper;
		}
		else {
			*ppCubeTexture = reinterpret_cast<D3D9Wrapper::IDirect3DCubeTexture9*>(baseTexture);
		}
	}

	if (ppCubeTexture) LogDebug("  returns result=%x, handle=%p, wrapper=%p\n", hr, baseTexture, *ppCubeTexture);

	LogInfo("  returns result=%x\n", hr);

	return hr;
}

HRESULT WINAPI Hooked_D3DXCreateCubeTextureFromResource(
	_In_  D3D9Wrapper::IDirect3DDevice9*      pDevice,
	_In_  HMODULE                hSrcModule,
	_In_  LPCTSTR                pSrcResource,
	_Out_ D3D9Wrapper::IDirect3DCubeTexture9* *ppCubeTexture) {
	LogInfo("Hooked_D3DXCreateCubeTextureFromResource EdgeLength=%d Format=%d\n", D3DX_DEFAULT, ::D3DFMT_UNKNOWN);
	D3D9Wrapper::IDirect3DDevice9 *wrappedDevice = wrappedDevice9(pDevice);

	if (!wrappedDevice->GetD3D9Device())
	{
		LogInfo("  postponing call because device was not created yet.\n");

		D3D9Wrapper::IDirect3DCubeTexture9 *wrapper;
		wrapper = D3D9Wrapper::IDirect3DCubeTexture9::GetDirect3DCubeTexture9((::LPDIRECT3DCUBETEXTURE9) 0, wrappedDevice, NULL);
		wrapper->_EdgeLength = D3DX_DEFAULT;
		wrapper->_Levels = D3DX_DEFAULT;
		wrapper->_Usage = 0;
		wrapper->_Format = ::D3DFMT_UNKNOWN;
		wrapper->_Pool = ::D3DPOOL_DEFAULT;
		wrapper->_Device = wrappedDevice;
		wrapper->pendingCreateTexture = true;
		*ppCubeTexture = wrapper;
		LogInfo("  returns handle=%p\n", wrapper);
		return S_OK;
	}
	D3D2DTEXTURE_DESC pDesc;
	pDesc.Width = D3DX_DEFAULT;
	pDesc.Height = D3DX_DEFAULT;
	pDesc.Levels = D3DX_DEFAULT;
	pDesc.Usage = 0;
	pDesc.Format = ::D3DFMT_UNKNOWN;
	pDesc.Pool = ::D3DPOOL_DEFAULT;

	::LPDIRECT3DCUBETEXTURE9 baseTexture = 0;
	HRESULT hr;
	D3D9Wrapper::IDirect3DCubeTexture9 *wrapper;
	if (G->gForceStereo == 2) {
		::IDirect3DCubeTexture9* pRightTexture = NULL;
		hr = trampoline_D3DXCreateCubeTextureFromResource(wrappedDevice->GetD3D9Device(), hSrcModule, pSrcResource, &baseTexture);
		if (!FAILED(hr) && (ShouldDuplicate(&pDesc))) {
			hr = trampoline_D3DXCreateCubeTextureFromResource(wrappedDevice->GetD3D9Device(), hSrcModule, pSrcResource, &pRightTexture);
			if (!FAILED(hr)) {
				wrapper = D3D9Wrapper::IDirect3DCubeTexture9::GetDirect3DCubeTexture9(baseTexture, wrappedDevice, pRightTexture);
			}
			else {
				LogDebug("Hooked_D3DXCreateCubeTextureFromResource Direct Mode, failed to create right texture, falling back to mono, hr = %d", hr);
				wrapper = D3D9Wrapper::IDirect3DCubeTexture9::GetDirect3DCubeTexture9(baseTexture, wrappedDevice, NULL);
			}
		}
		else {
			if (FAILED(hr))
				LogDebug("Hooked_D3DXCreateCubeTextureFromResource Direct Mode, failed to create left surface, hr = %d ", hr);
			wrapper = D3D9Wrapper::IDirect3DCubeTexture9::GetDirect3DCubeTexture9(baseTexture, wrappedDevice, NULL);
		}
	}
	else {
		hr = trampoline_D3DXCreateCubeTextureFromResource(wrappedDevice->GetD3D9Device(), hSrcModule, pSrcResource, &baseTexture);
		wrapper = D3D9Wrapper::IDirect3DCubeTexture9::GetDirect3DCubeTexture9(baseTexture, wrappedDevice, NULL);
	}

	if (ppCubeTexture) {
		if (!(G->enable_hooks >= EnableHooksDX9::ALL) && wrapper) {
			*ppCubeTexture = wrapper;
		}
		else {
			*ppCubeTexture = reinterpret_cast<D3D9Wrapper::IDirect3DCubeTexture9*>(baseTexture);
		}
	}

	if (ppCubeTexture) LogDebug("  returns result=%x, handle=%p, wrapper=%p\n", hr, baseTexture, *ppCubeTexture);

	LogInfo("  returns result=%x\n", hr);

	return hr;
}

HRESULT WINAPI Hooked_D3DXCreateCubeTextureFromResourceEx(
	_In_    D3D9Wrapper::IDirect3DDevice9*      pDevice,
	_In_    HMODULE                hSrcModule,
	_In_    LPCTSTR                pSrcResource,
	_In_    UINT                   Size,
	_In_    UINT                   MipLevels,
	_In_    DWORD                  Usage,
	_In_    ::D3DFORMAT              Format,
	_In_    ::D3DPOOL                Pool,
	_In_    DWORD                  Filter,
	_In_    DWORD                  MipFilter,
	_In_    ::D3DCOLOR               ColorKey,
	_Inout_ ::D3DXIMAGE_INFO         *pSrcInfo,
	_Out_   PALETTEENTRY           *pPalette,
	_Out_   D3D9Wrapper::IDirect3DCubeTexture9* *ppCubeTexture) {
	LogInfo("Hooked_D3DXCreateCubeTextureFromResourceEx EdgeLength=%d Format=%d\n", Size, Format);
	D3D9Wrapper::IDirect3DDevice9 *wrappedDevice = wrappedDevice9(pDevice);

	if (!wrappedDevice->GetD3D9Device())
	{
		LogInfo("  postponing call because device was not created yet.\n");

		D3D9Wrapper::IDirect3DCubeTexture9 *wrapper;
		wrapper = D3D9Wrapper::IDirect3DCubeTexture9::GetDirect3DCubeTexture9((::LPDIRECT3DCUBETEXTURE9) 0, wrappedDevice, NULL);
		wrapper->_EdgeLength = Size;
		wrapper->_Levels = MipLevels;
		wrapper->_Usage = Usage;
		wrapper->_Format = Format;
		wrapper->_Pool = Pool;
		wrapper->_Device = wrappedDevice;
		wrapper->pendingCreateTexture = true;
		*ppCubeTexture = wrapper;
		LogInfo("  returns handle=%p\n", wrapper);
		return S_OK;
	}

	if (Pool == ::D3DPOOL_MANAGED)
	{
		LogDebug("  Pool changed from MANAGED to DEFAULT because of DirectX9Ex migration.\n");

		Pool = ::D3DPOOL_DEFAULT;
		if (!(Usage & D3DUSAGE_DYNAMIC))
			Usage = Usage | D3DUSAGE_DYNAMIC;
	}

	D3D2DTEXTURE_DESC pDesc;
	pDesc.Width = Size;
	pDesc.Height = Size;
	pDesc.Levels = MipLevels;
	pDesc.Usage = Usage;
	pDesc.Format = Format;
	pDesc.Pool = ::D3DPOOL_DEFAULT;

	::LPDIRECT3DCUBETEXTURE9 baseTexture = 0;
	HRESULT hr;
	D3D9Wrapper::IDirect3DCubeTexture9 *wrapper;
	if (G->gForceStereo == 2) {
		::IDirect3DCubeTexture9* pRightTexture = NULL;
		hr = trampoline_D3DXCreateCubeTextureFromResourceEx(wrappedDevice->GetD3D9Device(), hSrcModule, pSrcResource, Size, MipLevels, Usage, Format, Pool, Filter, MipFilter, ColorKey, pSrcInfo, pPalette, &baseTexture);
		if (!FAILED(hr) && (ShouldDuplicate(&pDesc))) {
			hr = trampoline_D3DXCreateCubeTextureFromResourceEx(wrappedDevice->GetD3D9Device(), hSrcModule, pSrcResource, Size, MipLevels, Usage, Format, Pool, Filter, MipFilter, ColorKey, pSrcInfo, pPalette, &pRightTexture);
			if (!FAILED(hr)) {
				wrapper = D3D9Wrapper::IDirect3DCubeTexture9::GetDirect3DCubeTexture9(baseTexture, wrappedDevice, pRightTexture);
			}
			else {
				LogDebug("Hooked_D3DXCreateCubeTextureFromResourceEx Direct Mode, failed to create right texture, falling back to mono, hr = %d", hr);
				wrapper = D3D9Wrapper::IDirect3DCubeTexture9::GetDirect3DCubeTexture9(baseTexture, wrappedDevice, NULL);
			}
		}
		else {
			if (FAILED(hr))
				LogDebug("Hooked_D3DXCreateCubeTextureFromResourceEx Direct Mode, failed to create left surface, hr = %d ", hr);
			wrapper = D3D9Wrapper::IDirect3DCubeTexture9::GetDirect3DCubeTexture9(baseTexture, wrappedDevice, NULL);
		}
	}
	else {
		hr = trampoline_D3DXCreateCubeTextureFromResourceEx(wrappedDevice->GetD3D9Device(), hSrcModule, pSrcResource, Size, MipLevels, Usage, Format, Pool, Filter, MipFilter, ColorKey, pSrcInfo, pPalette, &baseTexture);
		wrapper = D3D9Wrapper::IDirect3DCubeTexture9::GetDirect3DCubeTexture9(baseTexture, wrappedDevice, NULL);
	}

	if (ppCubeTexture) {
		if (!(G->enable_hooks >= EnableHooksDX9::ALL) && wrapper) {
			*ppCubeTexture = wrapper;
		}
		else {
			*ppCubeTexture = reinterpret_cast<D3D9Wrapper::IDirect3DCubeTexture9*>(baseTexture);
		}
	}

	if (ppCubeTexture) LogDebug("  returns result=%x, handle=%p, wrapper=%p\n", hr, baseTexture, *ppCubeTexture);

	LogInfo("  returns result=%x\n", hr);

	return hr;
}

HRESULT WINAPI Hooked_D3DXCreateTexture(
	_In_  D3D9Wrapper::IDirect3DDevice9*  pDevice,
	_In_  UINT               Width,
	_In_  UINT               Height,
	_In_  UINT               MipLevels,
	_In_  DWORD              Usage,
	_In_  ::D3DFORMAT          Format,
	_In_  ::D3DPOOL            Pool,
	_Out_ D3D9Wrapper::IDirect3DTexture9* *ppTexture)
{
	LogDebug("Hooked_D3DXCreateTexture called with Width=%d, Height=%d, Levels=%d, Usage=%x, Format=%d, Pool=%d\n",
		Width, Height, MipLevels, Usage, Format, Pool);

	D3D9Wrapper::IDirect3DDevice9 *wrappedDevice = wrappedDevice9(pDevice);

	if (!wrappedDevice->GetD3D9Device())
	{
		LogInfo("  postponing call because device was not created yet.\n");
		D3D9Wrapper::IDirect3DTexture9 *wrapper;
		wrapper = D3D9Wrapper::IDirect3DTexture9::GetDirect3DTexture9((::LPDIRECT3DTEXTURE9) 0, wrappedDevice, NULL);
		wrapper->_Width = Width;
		wrapper->_Height = Height;
		wrapper->_Levels = MipLevels;
		wrapper->_Usage = Usage;
		wrapper->_Format = Format;
		wrapper->_Pool = Pool;
		wrapper->_Device = wrappedDevice;
		wrapper->pendingCreateTexture = true;
		*ppTexture = wrapper;
		LogInfo("  returns handle=%p\n", wrapper);

		return S_OK;
	}

	::LPDIRECT3DTEXTURE9 baseTexture = 0;
	if (Pool == ::D3DPOOL_MANAGED)
	{
		LogDebug("  Pool changed from MANAGED to DEFAULT because of DirectX9Ex migration.\n");

		Pool = ::D3DPOOL_DEFAULT;
		if (!(Usage & D3DUSAGE_DYNAMIC))
			Usage = Usage | D3DUSAGE_DYNAMIC;
	}
	D3D2DTEXTURE_DESC pDesc;
	pDesc.Width = Width;
	pDesc.Height = Height;
	pDesc.Levels = MipLevels;
	pDesc.Usage = Usage;
	pDesc.Format = Format;
	pDesc.Pool = Pool;
	HRESULT hr;
	D3D9Wrapper::IDirect3DTexture9 *wrapper;
	if (G->gForceStereo == 2) {
		::IDirect3DTexture9* pRightTexture = NULL;
		hr = trampoline_D3DXCreateTexture(wrappedDevice->GetD3D9Device(), Width, Height, MipLevels, Usage, Format, Pool, &baseTexture);
		if (!FAILED(hr) && (ShouldDuplicate(&pDesc))) {
			hr = trampoline_D3DXCreateTexture(wrappedDevice->GetD3D9Device(), Width, Height, MipLevels, Usage, Format, Pool, &pRightTexture);
			if (!FAILED(hr)) {
				wrapper = D3D9Wrapper::IDirect3DTexture9::GetDirect3DTexture9(baseTexture, wrappedDevice, pRightTexture);
			}
			else {
				LogDebug("Hooked_D3DXCreateTexture Direct Mode, failed to create right texture, falling back to mono, hr = %d", hr);
				wrapper = D3D9Wrapper::IDirect3DTexture9::GetDirect3DTexture9(baseTexture, wrappedDevice, NULL);
			}
		}
		else {
			if (FAILED(hr))
				LogDebug("Hooked_D3DXCreateTexture Direct Mode, failed to create left surface, hr = %d ", hr);
			wrapper = D3D9Wrapper::IDirect3DTexture9::GetDirect3DTexture9(baseTexture, wrappedDevice, NULL);
		}
	}
	else {
		hr = trampoline_D3DXCreateTexture(wrappedDevice->GetD3D9Device(), Width, Height, MipLevels, Usage, Format, Pool, &baseTexture);
		wrapper = D3D9Wrapper::IDirect3DTexture9::GetDirect3DTexture9(baseTexture, wrappedDevice, NULL);
	}

	if (ppTexture) {
		if (!(G->enable_hooks >= EnableHooksDX9::ALL) && wrapper) {
			*ppTexture = wrapper;
		}
		else {
			*ppTexture = (D3D9Wrapper::IDirect3DTexture9*)baseTexture;
		}
	}

	if (ppTexture) LogDebug("  returns result=%x, handle=%p, wrapper=%p\n", hr, baseTexture, *ppTexture);

	return hr;
}

HRESULT WINAPI Hooked_D3DXCreateTextureFromFile(
	_In_  D3D9Wrapper::IDirect3DDevice9*  pDevice,
	_In_  LPCTSTR            pSrcFile,
	_Out_ D3D9Wrapper::IDirect3DTexture9* *ppTexture) {

	LogDebug("Hooked_D3DXCreateTextureFromFile called with Width=%d, Height=%d, Levels=%d, Usage=%x, Format=%d, Pool=%d\n",
		D3DX_DEFAULT, D3DX_DEFAULT, D3DX_DEFAULT, 0, ::D3DFMT_UNKNOWN, ::D3DPOOL_DEFAULT);

	D3D9Wrapper::IDirect3DDevice9 *wrappedDevice = wrappedDevice9(pDevice);

	if (!wrappedDevice->GetD3D9Device())
	{
		LogInfo("  postponing call because device was not created yet.\n");
		D3D9Wrapper::IDirect3DTexture9 *wrapper;
		wrapper = D3D9Wrapper::IDirect3DTexture9::GetDirect3DTexture9((::LPDIRECT3DTEXTURE9) 0, wrappedDevice, NULL);
		wrapper->_Width = D3DX_DEFAULT;
		wrapper->_Height = D3DX_DEFAULT;
		wrapper->_Levels = D3DX_DEFAULT;
		wrapper->_Usage = 0;
		wrapper->_Format = ::D3DFMT_UNKNOWN;
		wrapper->_Pool = ::D3DPOOL_DEFAULT;
		wrapper->_Device = wrappedDevice;
		wrapper->pendingCreateTexture = true;
		*ppTexture = wrapper;
		LogInfo("  returns handle=%p\n", wrapper);
		return S_OK;
	}

	::LPDIRECT3DTEXTURE9 baseTexture = 0;
	D3D2DTEXTURE_DESC pDesc;
	pDesc.Width = D3DX_DEFAULT;
	pDesc.Height = D3DX_DEFAULT;
	pDesc.Levels = D3DX_DEFAULT;
	pDesc.Usage = 0;
	pDesc.Format = ::D3DFMT_UNKNOWN;
	pDesc.Pool = ::D3DPOOL_DEFAULT;
	HRESULT hr;
	D3D9Wrapper::IDirect3DTexture9 *wrapper;
	if (G->gForceStereo == 2) {
		::IDirect3DTexture9* pRightTexture = NULL;
		hr = trampoline_D3DXCreateTextureFromFile(wrappedDevice->GetD3D9Device(), pSrcFile, &baseTexture);
		if (!FAILED(hr) && (ShouldDuplicate(&pDesc))) {
			hr = trampoline_D3DXCreateTextureFromFile(wrappedDevice->GetD3D9Device(), pSrcFile, &pRightTexture);
			if (!FAILED(hr)) {
				wrapper = D3D9Wrapper::IDirect3DTexture9::GetDirect3DTexture9(baseTexture, wrappedDevice, pRightTexture);
			}
			else {
				LogDebug("Hooked_D3DXCreateTextureFromFile Direct Mode, failed to create right texture, falling back to mono, hr = %d", hr);
				wrapper = D3D9Wrapper::IDirect3DTexture9::GetDirect3DTexture9(baseTexture, wrappedDevice, NULL);
			}
		}
		else {
			if (FAILED(hr))
				LogDebug("Hooked_D3DXCreateTextureFromFile Direct Mode, failed to create left surface, hr = %d ", hr);
			wrapper = D3D9Wrapper::IDirect3DTexture9::GetDirect3DTexture9(baseTexture, wrappedDevice, NULL);
		}
	}
	else {
		hr = trampoline_D3DXCreateTextureFromFile(wrappedDevice->GetD3D9Device(), pSrcFile, &baseTexture);
		wrapper = D3D9Wrapper::IDirect3DTexture9::GetDirect3DTexture9(baseTexture, wrappedDevice, NULL);
	}

	if (ppTexture) {
		if (!(G->enable_hooks >= EnableHooksDX9::ALL) && wrapper) {
			*ppTexture = wrapper;
		}
		else {
			*ppTexture = (D3D9Wrapper::IDirect3DTexture9*)baseTexture;
		}
	}

	if (ppTexture) LogDebug("  returns result=%x, handle=%p, wrapper=%p\n", hr, baseTexture, *ppTexture);

	return hr;
}

HRESULT WINAPI Hooked_D3DXCreateTextureFromFileEx(
	_In_    D3D9Wrapper::IDirect3DDevice9*  pDevice,
	_In_    LPCTSTR            pSrcFile,
	_In_    UINT               Width,
	_In_    UINT               Height,
	_In_    UINT               MipLevels,
	_In_    DWORD              Usage,
	_In_    ::D3DFORMAT          Format,
	_In_    ::D3DPOOL            Pool,
	_In_    DWORD              Filter,
	_In_    DWORD              MipFilter,
	_In_    ::D3DCOLOR           ColorKey,
	_Inout_ ::D3DXIMAGE_INFO     *pSrcInfo,
	_Out_   PALETTEENTRY       *pPalette,
	_Out_   D3D9Wrapper::IDirect3DTexture9* *ppTexture) {
	LogDebug("Hooked_D3DXCreateTextureFromFileEx called with Width=%d, Height=%d, Levels=%d, Usage=%x, Format=%d, Pool=%d\n",
		Width, Height, MipLevels, Usage, Format, Pool);

	D3D9Wrapper::IDirect3DDevice9 *wrappedDevice = wrappedDevice9(pDevice);

	if (!wrappedDevice->GetD3D9Device())
	{
		LogInfo("  postponing call because device was not created yet.\n");
		D3D9Wrapper::IDirect3DTexture9 *wrapper;
		wrapper = D3D9Wrapper::IDirect3DTexture9::GetDirect3DTexture9((::LPDIRECT3DTEXTURE9) 0, wrappedDevice, NULL);
		wrapper->_Width = Width;
		wrapper->_Height = Height;
		wrapper->_Levels = MipLevels;
		wrapper->_Usage = Usage;
		wrapper->_Format = Format;
		wrapper->_Pool = Pool;
		wrapper->_Device = wrappedDevice;
		wrapper->pendingCreateTexture = true;
		*ppTexture = wrapper;
		LogInfo("  returns handle=%p\n", wrapper);

		return S_OK;
	}

	::LPDIRECT3DTEXTURE9 baseTexture = 0;
	if (Pool == ::D3DPOOL_MANAGED)
	{
		LogDebug("  Pool changed from MANAGED to DEFAULT because of DirectX9Ex migration.\n");

		Pool = ::D3DPOOL_DEFAULT;
		if (!(Usage & D3DUSAGE_DYNAMIC))
			Usage = Usage | D3DUSAGE_DYNAMIC;
	}
	D3D2DTEXTURE_DESC pDesc;
	pDesc.Width = Width;
	pDesc.Height = Height;
	pDesc.Levels = MipLevels;
	pDesc.Usage = Usage;
	pDesc.Format = Format;
	pDesc.Pool = Pool;
	HRESULT hr;
	D3D9Wrapper::IDirect3DTexture9 *wrapper;
	if (G->gForceStereo == 2) {
		::IDirect3DTexture9* pRightTexture = NULL;
		hr = trampoline_D3DXCreateTextureFromFileEx(wrappedDevice->GetD3D9Device(), pSrcFile, Width, Height, MipLevels, Usage, Format, Pool, Filter, MipFilter, ColorKey, pSrcInfo, pPalette, &baseTexture);
		if (!FAILED(hr) && (ShouldDuplicate(&pDesc))) {
			hr = trampoline_D3DXCreateTextureFromFileEx(wrappedDevice->GetD3D9Device(), pSrcFile, Width, Height, MipLevels, Usage, Format, Pool, Filter, MipFilter, ColorKey, pSrcInfo, pPalette,  &pRightTexture);
			if (!FAILED(hr)) {
				wrapper = D3D9Wrapper::IDirect3DTexture9::GetDirect3DTexture9(baseTexture, wrappedDevice, pRightTexture);
			}
			else {
				LogDebug("Hooked_D3DXCreateTextureFromFileEx Direct Mode, failed to create right texture, falling back to mono, hr = %d", hr);
				wrapper = D3D9Wrapper::IDirect3DTexture9::GetDirect3DTexture9(baseTexture, wrappedDevice, NULL);
			}
		}
		else {
			if (FAILED(hr))
				LogDebug("Hooked_D3DXCreateTextureFromFileEx Direct Mode, failed to create left surface, hr = %d ", hr);
			wrapper = D3D9Wrapper::IDirect3DTexture9::GetDirect3DTexture9(baseTexture, wrappedDevice, NULL);
		}
	}
	else {
		hr = trampoline_D3DXCreateTextureFromFileEx(wrappedDevice->GetD3D9Device(), pSrcFile, Width, Height, MipLevels, Usage, Format, Pool, Filter, MipFilter, ColorKey, pSrcInfo, pPalette, &baseTexture);
		wrapper = D3D9Wrapper::IDirect3DTexture9::GetDirect3DTexture9(baseTexture, wrappedDevice, NULL);
	}

	if (ppTexture) {
		if (!(G->enable_hooks >= EnableHooksDX9::ALL) && wrapper) {
			*ppTexture = wrapper;
		}
		else {
			*ppTexture = (D3D9Wrapper::IDirect3DTexture9*)baseTexture;
		}
	}

	if (ppTexture) LogDebug("  returns result=%x, handle=%p, wrapper=%p\n", hr, baseTexture, *ppTexture);

	return hr;
}

HRESULT WINAPI Hooked_D3DXCreateTextureFromFileInMemory(
	_In_  D3D9Wrapper::IDirect3DDevice9*  pDevice,
	_In_  LPCVOID            pSrcData,
	_In_  UINT               SrcDataSize,
	_Out_ D3D9Wrapper::IDirect3DTexture9* *ppTexture) {
	LogDebug("Hooked_D3DXCreateTextureFromFileInMemory called with Width=%d, Height=%d, Levels=%d, Usage=%x, Format=%d, Pool=%d\n",
		D3DX_DEFAULT, D3DX_DEFAULT, D3DX_DEFAULT, 0, ::D3DFMT_UNKNOWN, ::D3DPOOL_DEFAULT);

	D3D9Wrapper::IDirect3DDevice9 *wrappedDevice = wrappedDevice9(pDevice);

	if (!wrappedDevice->GetD3D9Device())
	{
		LogInfo("  postponing call because device was not created yet.\n");
		D3D9Wrapper::IDirect3DTexture9 *wrapper;
		wrapper = D3D9Wrapper::IDirect3DTexture9::GetDirect3DTexture9((::LPDIRECT3DTEXTURE9) 0, wrappedDevice, NULL);
		wrapper->_Width = D3DX_DEFAULT;
		wrapper->_Height = D3DX_DEFAULT;
		wrapper->_Levels = D3DX_DEFAULT;
		wrapper->_Usage = 0;
		wrapper->_Format = ::D3DFMT_UNKNOWN;
		wrapper->_Pool = ::D3DPOOL_DEFAULT;
		wrapper->_Device = wrappedDevice;
		wrapper->pendingCreateTexture = true;
		*ppTexture = wrapper;
		LogInfo("  returns handle=%p\n", wrapper);
		return S_OK;
	}

	::LPDIRECT3DTEXTURE9 baseTexture = 0;
	D3D2DTEXTURE_DESC pDesc;
	pDesc.Width = D3DX_DEFAULT;
	pDesc.Height = D3DX_DEFAULT;
	pDesc.Levels = D3DX_DEFAULT;
	pDesc.Usage = 0;
	pDesc.Format = ::D3DFMT_UNKNOWN;
	pDesc.Pool = ::D3DPOOL_DEFAULT;
	HRESULT hr;
	D3D9Wrapper::IDirect3DTexture9 *wrapper;
	if (G->gForceStereo == 2) {
		::IDirect3DTexture9* pRightTexture = NULL;
		hr = trampoline_D3DXCreateTextureFromFileInMemory(wrappedDevice->GetD3D9Device(), pSrcData, SrcDataSize, &baseTexture);
		if (!FAILED(hr) && (ShouldDuplicate(&pDesc))) {
			hr = trampoline_D3DXCreateTextureFromFileInMemory(wrappedDevice->GetD3D9Device(), pSrcData, SrcDataSize, &pRightTexture);
			if (!FAILED(hr)) {
				wrapper = D3D9Wrapper::IDirect3DTexture9::GetDirect3DTexture9(baseTexture, wrappedDevice, pRightTexture);
			}
			else {
				LogDebug("Hooked_D3DXCreateTextureFromFileInMemory Direct Mode, failed to create right texture, falling back to mono, hr = %d", hr);
				wrapper = D3D9Wrapper::IDirect3DTexture9::GetDirect3DTexture9(baseTexture, wrappedDevice, NULL);
			}
		}
		else {
			if (FAILED(hr))
				LogDebug("Hooked_D3DXCreateTextureFromFileInMemory Direct Mode, failed to create left surface, hr = %d ", hr);
			wrapper = D3D9Wrapper::IDirect3DTexture9::GetDirect3DTexture9(baseTexture, wrappedDevice, NULL);
		}
	}
	else {
		hr = trampoline_D3DXCreateTextureFromFileInMemory(wrappedDevice->GetD3D9Device(), pSrcData, SrcDataSize, &baseTexture);
		wrapper = D3D9Wrapper::IDirect3DTexture9::GetDirect3DTexture9(baseTexture, wrappedDevice, NULL);
	}

	if (ppTexture) {
		if (!(G->enable_hooks >= EnableHooksDX9::ALL) && wrapper) {
			*ppTexture = wrapper;
		}
		else {
			*ppTexture = (D3D9Wrapper::IDirect3DTexture9*)baseTexture;
		}
	}

	if (ppTexture) LogDebug("  returns result=%x, handle=%p, wrapper=%p\n", hr, baseTexture, *ppTexture);

	return hr;
}

HRESULT WINAPI Hooked_D3DXCreateTextureFromFileInMemoryEx(
	_In_    D3D9Wrapper::IDirect3DDevice9*  pDevice,
	_In_    LPCVOID            pSrcData,
	_In_    UINT               SrcDataSize,
	_In_    UINT               Width,
	_In_    UINT               Height,
	_In_    UINT               MipLevels,
	_In_    DWORD              Usage,
	_In_    ::D3DFORMAT          Format,
	_In_    ::D3DPOOL            Pool,
	_In_    DWORD              Filter,
	_In_    DWORD              MipFilter,
	_In_    ::D3DCOLOR           ColorKey,
	_Inout_ ::D3DXIMAGE_INFO     *pSrcInfo,
	_Out_   PALETTEENTRY       *pPalette,
	_Out_   D3D9Wrapper::IDirect3DTexture9* *ppTexture) {
	LogDebug("Hooked_D3DXCreateTextureFromFileInMemoryEx called with Width=%d, Height=%d, Levels=%d, Usage=%x, Format=%d, Pool=%d\n",
		Width, Height, MipLevels, Usage, Format, Pool);

	D3D9Wrapper::IDirect3DDevice9 *wrappedDevice = wrappedDevice9(pDevice);

	if (!wrappedDevice->GetD3D9Device())
	{
		LogInfo("  postponing call because device was not created yet.\n");
		D3D9Wrapper::IDirect3DTexture9 *wrapper;
		wrapper = D3D9Wrapper::IDirect3DTexture9::GetDirect3DTexture9((::LPDIRECT3DTEXTURE9) 0, wrappedDevice, NULL);
		wrapper->_Width = Width;
		wrapper->_Height = Height;
		wrapper->_Levels = MipLevels;
		wrapper->_Usage = Usage;
		wrapper->_Format = Format;
		wrapper->_Pool = Pool;
		wrapper->_Device = wrappedDevice;
		wrapper->pendingCreateTexture = true;
		*ppTexture = wrapper;
		LogInfo("  returns handle=%p\n", wrapper);

		return S_OK;
	}

	::LPDIRECT3DTEXTURE9 baseTexture = 0;
	if (Pool == ::D3DPOOL_MANAGED)
	{
		LogDebug("  Pool changed from MANAGED to DEFAULT because of DirectX9Ex migration.\n");

		Pool = ::D3DPOOL_DEFAULT;
		if (!(Usage & D3DUSAGE_DYNAMIC))
			Usage = Usage | D3DUSAGE_DYNAMIC;
	}
	D3D2DTEXTURE_DESC pDesc;
	pDesc.Width = Width;
	pDesc.Height = Height;
	pDesc.Levels = MipLevels;
	pDesc.Usage = Usage;
	pDesc.Format = Format;
	pDesc.Pool = Pool;
	HRESULT hr;
	D3D9Wrapper::IDirect3DTexture9 *wrapper;
	if (G->gForceStereo == 2) {
		::IDirect3DTexture9* pRightTexture = NULL;
		hr = trampoline_D3DXCreateTextureFromFileInMemoryEx(wrappedDevice->GetD3D9Device(), pSrcData, SrcDataSize, Width, Height, MipLevels, Usage, Format, Pool, Filter, MipFilter, ColorKey, pSrcInfo, pPalette, &baseTexture);
		if (!FAILED(hr) && (ShouldDuplicate(&pDesc))) {
			hr = trampoline_D3DXCreateTextureFromFileInMemoryEx(wrappedDevice->GetD3D9Device(), pSrcData, SrcDataSize, Width, Height, MipLevels, Usage, Format, Pool, Filter, MipFilter, ColorKey, pSrcInfo, pPalette, &pRightTexture);
			if (!FAILED(hr)) {
				wrapper = D3D9Wrapper::IDirect3DTexture9::GetDirect3DTexture9(baseTexture, wrappedDevice, pRightTexture);
			}
			else {
				LogDebug("Hooked_D3DXCreateTextureFromFileInMemoryEx Direct Mode, failed to create right texture, falling back to mono, hr = %d", hr);
				wrapper = D3D9Wrapper::IDirect3DTexture9::GetDirect3DTexture9(baseTexture, wrappedDevice, NULL);
			}
		}
		else {
			if (FAILED(hr))
				LogDebug("Hooked_D3DXCreateTextureFromFileInMemoryEx Direct Mode, failed to create left surface, hr = %d ", hr);
			wrapper = D3D9Wrapper::IDirect3DTexture9::GetDirect3DTexture9(baseTexture, wrappedDevice, NULL);
		}
	}
	else {
		hr = trampoline_D3DXCreateTextureFromFileInMemoryEx(wrappedDevice->GetD3D9Device(), pSrcData, SrcDataSize, Width, Height, MipLevels, Usage, Format, Pool, Filter, MipFilter, ColorKey, pSrcInfo, pPalette, &baseTexture);
		wrapper = D3D9Wrapper::IDirect3DTexture9::GetDirect3DTexture9(baseTexture, wrappedDevice, NULL);
	}

	if (ppTexture) {
		if (!(G->enable_hooks >= EnableHooksDX9::ALL) && wrapper) {
			*ppTexture = wrapper;
		}
		else {
			*ppTexture = (D3D9Wrapper::IDirect3DTexture9*)baseTexture;
		}
	}

	if (ppTexture) LogDebug("  returns result=%x, handle=%p, wrapper=%p\n", hr, baseTexture, *ppTexture);

	return hr;
}

HRESULT WINAPI Hooked_D3DXCreateTextureFromResource(
	_In_  D3D9Wrapper::IDirect3DDevice9*  pDevice,
	_In_  HMODULE            hSrcModule,
	_In_  LPCTSTR            pSrcResource,
	_Out_ D3D9Wrapper::IDirect3DTexture9* *ppTexture) {
	LogDebug("Hooked_D3DXCreateTextureFromResource called with Width=%d, Height=%d, Levels=%d, Usage=%x, Format=%d, Pool=%d\n",
		D3DX_DEFAULT, D3DX_DEFAULT, D3DX_DEFAULT, 0, ::D3DFMT_UNKNOWN, ::D3DPOOL_DEFAULT);

	D3D9Wrapper::IDirect3DDevice9 *wrappedDevice = wrappedDevice9(pDevice);

	if (!wrappedDevice->GetD3D9Device())
	{
		LogInfo("  postponing call because device was not created yet.\n");
		D3D9Wrapper::IDirect3DTexture9 *wrapper;
		wrapper = D3D9Wrapper::IDirect3DTexture9::GetDirect3DTexture9((::LPDIRECT3DTEXTURE9) 0, wrappedDevice, NULL);
		wrapper->_Width = D3DX_DEFAULT;
		wrapper->_Height = D3DX_DEFAULT;
		wrapper->_Levels = D3DX_DEFAULT;
		wrapper->_Usage = 0;
		wrapper->_Format = ::D3DFMT_UNKNOWN;
		wrapper->_Pool = ::D3DPOOL_DEFAULT;
		wrapper->_Device = wrappedDevice;
		wrapper->pendingCreateTexture = true;
		*ppTexture = wrapper;
		LogInfo("  returns handle=%p\n", wrapper);
		return S_OK;
	}

	::LPDIRECT3DTEXTURE9 baseTexture = 0;
	D3D2DTEXTURE_DESC pDesc;
	pDesc.Width = D3DX_DEFAULT;
	pDesc.Height = D3DX_DEFAULT;
	pDesc.Levels = D3DX_DEFAULT;
	pDesc.Usage = 0;
	pDesc.Format = ::D3DFMT_UNKNOWN;
	pDesc.Pool = ::D3DPOOL_DEFAULT;
	HRESULT hr;
	D3D9Wrapper::IDirect3DTexture9 *wrapper;
	if (G->gForceStereo == 2) {
		::IDirect3DTexture9* pRightTexture = NULL;
		hr = trampoline_D3DXCreateTextureFromResource(wrappedDevice->GetD3D9Device(), hSrcModule, pSrcResource, &baseTexture);
		if (!FAILED(hr) && (ShouldDuplicate(&pDesc))) {
			hr = trampoline_D3DXCreateTextureFromResource(wrappedDevice->GetD3D9Device(), hSrcModule, pSrcResource, &pRightTexture);
			if (!FAILED(hr)) {
				wrapper = D3D9Wrapper::IDirect3DTexture9::GetDirect3DTexture9(baseTexture, wrappedDevice, pRightTexture);
			}
			else {
				LogDebug("Hooked_D3DXCreateTextureFromResource Direct Mode, failed to create right texture, falling back to mono, hr = %d", hr);
				wrapper = D3D9Wrapper::IDirect3DTexture9::GetDirect3DTexture9(baseTexture, wrappedDevice, NULL);
			}
		}
		else {
			if (FAILED(hr))
				LogDebug("Hooked_D3DXCreateTextureFromResource Direct Mode, failed to create left surface, hr = %d ", hr);
			wrapper = D3D9Wrapper::IDirect3DTexture9::GetDirect3DTexture9(baseTexture, wrappedDevice, NULL);
		}
	}
	else {
		hr = trampoline_D3DXCreateTextureFromResource(wrappedDevice->GetD3D9Device(), hSrcModule, pSrcResource, &baseTexture);
		wrapper = D3D9Wrapper::IDirect3DTexture9::GetDirect3DTexture9(baseTexture, wrappedDevice, NULL);
	}

	if (ppTexture) {
		if (!(G->enable_hooks >= EnableHooksDX9::ALL) && wrapper) {
			*ppTexture = wrapper;
		}
		else {
			*ppTexture = (D3D9Wrapper::IDirect3DTexture9*)baseTexture;
		}
	}

	if (ppTexture) LogDebug("  returns result=%x, handle=%p, wrapper=%p\n", hr, baseTexture, *ppTexture);

	return hr;
}

HRESULT WINAPI Hooked_D3DXCreateTextureFromResourceEx(
	_In_    D3D9Wrapper::IDirect3DDevice9*  pDevice,
	_In_    HMODULE            hSrcModule,
	_In_    LPCTSTR            pSrcResource,
	_In_    UINT               Width,
	_In_    UINT               Height,
	_In_    UINT               MipLevels,
	_In_    DWORD              Usage,
	_In_    ::D3DFORMAT          Format,
	_In_    ::D3DPOOL            Pool,
	_In_    DWORD              Filter,
	_In_    DWORD              MipFilter,
	_In_    ::D3DCOLOR           ColorKey,
	_Inout_ ::D3DXIMAGE_INFO     *pSrcInfo,
	_Out_   PALETTEENTRY       *pPalette,
	_Out_   D3D9Wrapper::IDirect3DTexture9* *ppTexture) {
	LogDebug("Hooked_D3DXCreateTextureFromResourceEx called with Width=%d, Height=%d, Levels=%d, Usage=%x, Format=%d, Pool=%d\n",
		Width, Height, MipLevels, Usage, Format, Pool);

	D3D9Wrapper::IDirect3DDevice9 *wrappedDevice = wrappedDevice9(pDevice);

	if (!wrappedDevice->GetD3D9Device())
	{
		LogInfo("  postponing call because device was not created yet.\n");
		D3D9Wrapper::IDirect3DTexture9 *wrapper;
		wrapper = D3D9Wrapper::IDirect3DTexture9::GetDirect3DTexture9((::LPDIRECT3DTEXTURE9) 0, wrappedDevice, NULL);
		wrapper->_Width = Width;
		wrapper->_Height = Height;
		wrapper->_Levels = MipLevels;
		wrapper->_Usage = Usage;
		wrapper->_Format = Format;
		wrapper->_Pool = Pool;
		wrapper->_Device = wrappedDevice;
		wrapper->pendingCreateTexture = true;
		*ppTexture = wrapper;
		LogInfo("  returns handle=%p\n", wrapper);

		return S_OK;
	}

	::LPDIRECT3DTEXTURE9 baseTexture = 0;
	if (Pool == ::D3DPOOL_MANAGED)
	{
		LogDebug("  Pool changed from MANAGED to DEFAULT because of DirectX9Ex migration.\n");

		Pool = ::D3DPOOL_DEFAULT;
		if (!(Usage & D3DUSAGE_DYNAMIC))
			Usage = Usage | D3DUSAGE_DYNAMIC;
	}
	D3D2DTEXTURE_DESC pDesc;
	pDesc.Width = Width;
	pDesc.Height = Height;
	pDesc.Levels = MipLevels;
	pDesc.Usage = Usage;
	pDesc.Format = Format;
	pDesc.Pool = Pool;
	HRESULT hr;
	D3D9Wrapper::IDirect3DTexture9 *wrapper;
	if (G->gForceStereo == 2) {
		::IDirect3DTexture9* pRightTexture = NULL;
		hr = trampoline_D3DXCreateTextureFromResourceEx(wrappedDevice->GetD3D9Device(), hSrcModule, pSrcResource, Width, Height, MipLevels, Usage, Format, Pool, Filter, MipFilter, ColorKey, pSrcInfo, pPalette, &baseTexture);
		if (!FAILED(hr) && (ShouldDuplicate(&pDesc))) {
			hr = trampoline_D3DXCreateTextureFromResourceEx(wrappedDevice->GetD3D9Device(), hSrcModule, pSrcResource, Width, Height, MipLevels, Usage, Format, Pool, Filter, MipFilter, ColorKey, pSrcInfo, pPalette, &pRightTexture);
			if (!FAILED(hr)) {
				wrapper = D3D9Wrapper::IDirect3DTexture9::GetDirect3DTexture9(baseTexture, wrappedDevice, pRightTexture);
			}
			else {
				LogDebug("Hooked_D3DXCreateTextureFromResourceEx Direct Mode, failed to create right texture, falling back to mono, hr = %d", hr);
				wrapper = D3D9Wrapper::IDirect3DTexture9::GetDirect3DTexture9(baseTexture, wrappedDevice, NULL);
			}
		}
		else {
			if (FAILED(hr))
				LogDebug("Hooked_D3DXCreateTextureFromResourceEx Direct Mode, failed to create left surface, hr = %d ", hr);
			wrapper = D3D9Wrapper::IDirect3DTexture9::GetDirect3DTexture9(baseTexture, wrappedDevice, NULL);
		}
	}
	else {
		hr = trampoline_D3DXCreateTextureFromResourceEx(wrappedDevice->GetD3D9Device(), hSrcModule, pSrcResource, Width, Height, MipLevels, Usage, Format, Pool, Filter, MipFilter, ColorKey, pSrcInfo, pPalette, &baseTexture);
		wrapper = D3D9Wrapper::IDirect3DTexture9::GetDirect3DTexture9(baseTexture, wrappedDevice, NULL);
	}

	if (ppTexture) {
		if (!(G->enable_hooks >= EnableHooksDX9::ALL) && wrapper) {
			*ppTexture = wrapper;
		}
		else {
			*ppTexture = (D3D9Wrapper::IDirect3DTexture9*)baseTexture;
		}
	}

	if (ppTexture) LogDebug("  returns result=%x, handle=%p, wrapper=%p\n", hr, baseTexture, *ppTexture);

	return hr;
}

HRESULT WINAPI Hooked_D3DXCreateVolumeTexture(
	_In_  D3D9Wrapper::IDirect3DDevice9*        pDevice,
	_In_  UINT                     Width,
	_In_  UINT                     Height,
	_In_  UINT                     Depth,
	_In_  UINT                     MipLevels,
	_In_  DWORD                    Usage,
	_In_  ::D3DFORMAT                Format,
	_In_  ::D3DPOOL                  Pool,
	_Out_ D3D9Wrapper::IDirect3DVolumeTexture9* *ppVolumeTexture) {
	LogInfo("Hooked_D3DXCreateVolumeTexture Width=%d Height=%d Format=%d\n", Width, Height, Format);
	D3D9Wrapper::IDirect3DDevice9 *wrappedDevice = wrappedDevice9(pDevice);

	if (!wrappedDevice->GetD3D9Device())
	{
		LogInfo("  postponing call because device was not created yet.\n");

		D3D9Wrapper::IDirect3DVolumeTexture9 *wrapper;
		wrapper = D3D9Wrapper::IDirect3DVolumeTexture9::GetDirect3DVolumeTexture9((::LPDIRECT3DVOLUMETEXTURE9) 0, wrappedDevice);
		wrapper->_Height = Height;
		wrapper->_Width = Width;
		wrapper->_Depth = Depth;
		wrapper->_Levels = MipLevels;
		wrapper->_Usage = Usage;
		wrapper->_Format = Format;
		wrapper->_Pool = Pool;
		wrapper->_Device = wrappedDevice;
		wrapper->pendingCreateTexture = true;
		*ppVolumeTexture = wrapper;
		LogInfo("  returns handle=%p\n", wrapper);

		return S_OK;
	}

	if (Pool == ::D3DPOOL_MANAGED)
	{
		LogDebug("  Pool changed from MANAGED to DEFAULT because of DirectX9Ex migration.\n");
		Pool = ::D3DPOOL_DEFAULT;
		if (!(Usage & D3DUSAGE_DYNAMIC))
			Usage = Usage | D3DUSAGE_DYNAMIC;
	}

	D3D3DTEXTURE_DESC pDesc;
	pDesc.Width = Width;
	pDesc.Height = Height;
	pDesc.Depth = Depth;
	pDesc.Levels = MipLevels;
	pDesc.Usage = Usage;
	pDesc.Format = Format;
	pDesc.Pool = Pool;

	::LPDIRECT3DVOLUMETEXTURE9 baseTexture = 0;
	HRESULT hr = trampoline_D3DXCreateVolumeTexture(wrappedDevice->GetD3D9Device(), Width, Height, Depth, MipLevels, Usage, Format, Pool, &baseTexture);
	if (baseTexture) {
		D3D9Wrapper::IDirect3DVolumeTexture9 *wrapper = D3D9Wrapper::IDirect3DVolumeTexture9::GetDirect3DVolumeTexture9(baseTexture, wrappedDevice);
		if (ppVolumeTexture) {
			if (!(G->enable_hooks >= EnableHooksDX9::ALL) && wrapper) {
				*ppVolumeTexture = wrapper;
			}
			else {
				*ppVolumeTexture = (D3D9Wrapper::IDirect3DVolumeTexture9*)baseTexture;
			}
		}

	}
	if (ppVolumeTexture) LogDebug("  returns result=%x, handle=%p, wrapper=%p\n", hr, baseTexture, *ppVolumeTexture);
	LogInfo("  returns result=%x\n", hr);

	return hr;
}

HRESULT WINAPI Hooked_D3DXCreateVolumeTextureFromFile(
	_In_  D3D9Wrapper::IDirect3DDevice9*        pDevice,
	_In_  LPCTSTR                  pSrcFile,
	_Out_ D3D9Wrapper::IDirect3DVolumeTexture9* *ppVolumeTexture) {
	LogInfo("Hooked_D3DXCreateVolumeTextureFromFile Width=%d Height=%d Format=%d\n", D3DX_DEFAULT, D3DX_DEFAULT, ::D3DFMT_UNKNOWN);
	D3D9Wrapper::IDirect3DDevice9 *wrappedDevice = wrappedDevice9(pDevice);

	if (!wrappedDevice->GetD3D9Device())
	{
		LogInfo("  postponing call because device was not created yet.\n");

		D3D9Wrapper::IDirect3DVolumeTexture9 *wrapper;
		wrapper = D3D9Wrapper::IDirect3DVolumeTexture9::GetDirect3DVolumeTexture9((::LPDIRECT3DVOLUMETEXTURE9) 0, wrappedDevice);
		wrapper->_Height = D3DX_DEFAULT;
		wrapper->_Width = D3DX_DEFAULT;
		wrapper->_Depth = D3DX_DEFAULT;
		wrapper->_Levels = D3DX_DEFAULT;
		wrapper->_Usage = 0;
		wrapper->_Format = ::D3DFMT_UNKNOWN;
		wrapper->_Pool = ::D3DPOOL_DEFAULT;
		wrapper->_Device = wrappedDevice;
		wrapper->pendingCreateTexture = true;
		*ppVolumeTexture = wrapper;
		LogInfo("  returns handle=%p\n", wrapper);

		return S_OK;
	}

	D3D3DTEXTURE_DESC pDesc;
	pDesc.Width = D3DX_DEFAULT;
	pDesc.Height = D3DX_DEFAULT;
	pDesc.Depth = D3DX_DEFAULT;
	pDesc.Levels = D3DX_DEFAULT;
	pDesc.Usage = 0;
	pDesc.Format = ::D3DFMT_UNKNOWN;
	pDesc.Pool = ::D3DPOOL_DEFAULT;

	::LPDIRECT3DVOLUMETEXTURE9 baseTexture = 0;
	HRESULT hr = trampoline_D3DXCreateVolumeTextureFromFile(wrappedDevice->GetD3D9Device(), pSrcFile, &baseTexture);
	if (baseTexture) {
		D3D9Wrapper::IDirect3DVolumeTexture9 *wrapper = D3D9Wrapper::IDirect3DVolumeTexture9::GetDirect3DVolumeTexture9(baseTexture, wrappedDevice);
		if (ppVolumeTexture) {
			if (!(G->enable_hooks >= EnableHooksDX9::ALL) && wrapper) {
				*ppVolumeTexture = wrapper;
			}
			else {
				*ppVolumeTexture = (D3D9Wrapper::IDirect3DVolumeTexture9*)baseTexture;
			}
		}
	}
	if (ppVolumeTexture) LogDebug("  returns result=%x, handle=%p, wrapper=%p\n", hr, baseTexture, *ppVolumeTexture);
	LogInfo("  returns result=%x\n", hr);

	return hr;
}

HRESULT WINAPI Hooked_D3DXCreateVolumeTextureFromFileEx(
	_In_    D3D9Wrapper::IDirect3DDevice9*        pDevice,
	_In_    LPCTSTR                  pSrcFile,
	_In_    UINT                     Width,
	_In_    UINT                     Height,
	_In_    UINT                     Depth,
	_In_    UINT                     MipLevels,
	_In_    DWORD                    Usage,
	::D3DFORMAT                Format,
	_In_    ::D3DPOOL                  Pool,
	_In_    DWORD                    Filter,
	_In_    DWORD                    MipFilter,
	_In_    ::D3DCOLOR                 ColorKey,
	_Inout_ ::D3DXIMAGE_INFO           *pSrcInfo,
	_Out_   PALETTEENTRY             *pPalette,
	_Out_   D3D9Wrapper::IDirect3DVolumeTexture9* *ppTexture) {
	LogInfo("Hooked_D3DXCreateVolumeTextureFromFileEx Width=%d Height=%d Format=%d\n", Width, Height, Format);
	D3D9Wrapper::IDirect3DDevice9 *wrappedDevice = wrappedDevice9(pDevice);

	if (!wrappedDevice->GetD3D9Device())
	{
		LogInfo("  postponing call because device was not created yet.\n");

		D3D9Wrapper::IDirect3DVolumeTexture9 *wrapper;
		wrapper = D3D9Wrapper::IDirect3DVolumeTexture9::GetDirect3DVolumeTexture9((::LPDIRECT3DVOLUMETEXTURE9) 0, wrappedDevice);
		wrapper->_Height = Height;
		wrapper->_Width = Width;
		wrapper->_Depth = Depth;
		wrapper->_Levels = MipLevels;
		wrapper->_Usage = Usage;
		wrapper->_Format = Format;
		wrapper->_Pool = Pool;
		wrapper->_Device = wrappedDevice;
		wrapper->pendingCreateTexture = true;
		*ppTexture = wrapper;
		LogInfo("  returns handle=%p\n", wrapper);

		return S_OK;
	}

	if (Pool == ::D3DPOOL_MANAGED)
	{
		LogDebug("  Pool changed from MANAGED to DEFAULT because of DirectX9Ex migration.\n");
		Pool = ::D3DPOOL_DEFAULT;
		if (!(Usage & D3DUSAGE_DYNAMIC))
			Usage = Usage | D3DUSAGE_DYNAMIC;
	}

	D3D3DTEXTURE_DESC pDesc;
	pDesc.Width = Width;
	pDesc.Height = Height;
	pDesc.Depth = Depth;
	pDesc.Levels = MipLevels;
	pDesc.Usage = Usage;
	pDesc.Format = Format;
	pDesc.Pool = Pool;

	::LPDIRECT3DVOLUMETEXTURE9 baseTexture = 0;
	HRESULT hr = trampoline_D3DXCreateVolumeTexture(wrappedDevice->GetD3D9Device(), Width, Height, Depth, MipLevels, Usage, Format, Pool, &baseTexture);
	if (baseTexture) {
		D3D9Wrapper::IDirect3DVolumeTexture9 *wrapper = D3D9Wrapper::IDirect3DVolumeTexture9::GetDirect3DVolumeTexture9(baseTexture, wrappedDevice);
		if (ppTexture) {
			if (!(G->enable_hooks >= EnableHooksDX9::ALL) && wrapper) {
				*ppTexture = wrapper;
			}
			else {
				*ppTexture = (D3D9Wrapper::IDirect3DVolumeTexture9*)baseTexture;
			}
		}
	}

	if (ppTexture) LogDebug("  returns result=%x, handle=%p, wrapper=%p\n", hr, baseTexture, *ppTexture);
	LogInfo("  returns result=%x\n", hr);

	return hr;
}

HRESULT WINAPI Hooked_D3DXCreateVolumeTextureFromFileInMemory(
	D3D9Wrapper::IDirect3DDevice9*         pDevice,
	LPCVOID                   pSrcData,
	UINT                      SrcDataSize,
	D3D9Wrapper::IDirect3DVolumeTexture9** ppVolumeTexture) {
	LogInfo("Hooked_D3DXCreateVolumeTextureFromFileInMemory Width=%d Height=%d Format=%d\n", D3DX_DEFAULT, D3DX_DEFAULT, ::D3DFMT_UNKNOWN);
	D3D9Wrapper::IDirect3DDevice9 *wrappedDevice = wrappedDevice9(pDevice);

	if (!wrappedDevice->GetD3D9Device())
	{
		LogInfo("  postponing call because device was not created yet.\n");

		D3D9Wrapper::IDirect3DVolumeTexture9 *wrapper;
		wrapper = D3D9Wrapper::IDirect3DVolumeTexture9::GetDirect3DVolumeTexture9((::LPDIRECT3DVOLUMETEXTURE9) 0, wrappedDevice);
		wrapper->_Height = D3DX_DEFAULT;
		wrapper->_Width = D3DX_DEFAULT;
		wrapper->_Depth = D3DX_DEFAULT;
		wrapper->_Levels = D3DX_DEFAULT;
		wrapper->_Usage = 0;
		wrapper->_Format = ::D3DFMT_UNKNOWN;
		wrapper->_Pool = ::D3DPOOL_DEFAULT;
		wrapper->_Device = wrappedDevice;
		wrapper->pendingCreateTexture = true;
		*ppVolumeTexture = wrapper;
		LogInfo("  returns handle=%p\n", wrapper);

		return S_OK;
	}

	D3D3DTEXTURE_DESC pDesc;
	pDesc.Width = D3DX_DEFAULT;
	pDesc.Height = D3DX_DEFAULT;
	pDesc.Depth = D3DX_DEFAULT;
	pDesc.Levels = D3DX_DEFAULT;
	pDesc.Usage = 0;
	pDesc.Format = ::D3DFMT_UNKNOWN;
	pDesc.Pool = ::D3DPOOL_DEFAULT;

	::LPDIRECT3DVOLUMETEXTURE9 baseTexture = 0;
	HRESULT hr = D3DXCreateVolumeTextureFromFileInMemory(wrappedDevice->GetD3D9Device(), pSrcData, SrcDataSize, &baseTexture);
	if (baseTexture) {
		D3D9Wrapper::IDirect3DVolumeTexture9 *wrapper = D3D9Wrapper::IDirect3DVolumeTexture9::GetDirect3DVolumeTexture9(baseTexture, wrappedDevice);
		if (ppVolumeTexture) {
			if (!(G->enable_hooks >= EnableHooksDX9::ALL) && wrapper) {
				*ppVolumeTexture = wrapper;
			}
			else {
				*ppVolumeTexture = (D3D9Wrapper::IDirect3DVolumeTexture9*)baseTexture;
			}
		}
	}
	if (ppVolumeTexture) LogDebug("  returns result=%x, handle=%p, wrapper=%p\n", hr, baseTexture, *ppVolumeTexture);
	LogInfo("  returns result=%x\n", hr);

	return hr;
}

HRESULT WINAPI Hooked_D3DXCreateVolumeTextureFromFileInMemoryEx(
	_In_    D3D9Wrapper::IDirect3DDevice9*        pDevice,
	_In_    LPCVOID                  pSrcData,
	_In_    UINT                     SrcDataSize,
	_In_    UINT                     Width,
	_In_    UINT                     Height,
	_In_    UINT                     Depth,
	_In_    UINT                     MipLevels,
	_In_    DWORD                    Usage,
	_In_    ::D3DFORMAT                Format,
	_In_    ::D3DPOOL                  Pool,
	_In_    DWORD                    Filter,
	_In_    DWORD                    MipFilter,
	_In_    ::D3DCOLOR                 ColorKey,
	_Inout_ ::D3DXIMAGE_INFO           *pSrcInfo,
	_Out_   PALETTEENTRY             *pPalette,
	_Out_   D3D9Wrapper::IDirect3DVolumeTexture9* *ppVolumeTexture) {
	LogInfo("Hooked_D3DXCreateVolumeTextureFromFileInMemoryEx Width=%d Height=%d Format=%d\n", Width, Height, Format);
	D3D9Wrapper::IDirect3DDevice9 *wrappedDevice = wrappedDevice9(pDevice);

	if (!wrappedDevice->GetD3D9Device())
	{
		LogInfo("  postponing call because device was not created yet.\n");

		D3D9Wrapper::IDirect3DVolumeTexture9 *wrapper;
		wrapper = D3D9Wrapper::IDirect3DVolumeTexture9::GetDirect3DVolumeTexture9((::LPDIRECT3DVOLUMETEXTURE9) 0, wrappedDevice);
		wrapper->_Height = Height;
		wrapper->_Width = Width;
		wrapper->_Depth = Depth;
		wrapper->_Levels = MipLevels;
		wrapper->_Usage = Usage;
		wrapper->_Format = Format;
		wrapper->_Pool = Pool;
		wrapper->_Device = wrappedDevice;
		wrapper->pendingCreateTexture = true;
		*ppVolumeTexture = wrapper;
		LogInfo("  returns handle=%p\n", wrapper);

		return S_OK;
	}

	if (Pool == ::D3DPOOL_MANAGED)
	{
		LogDebug("  Pool changed from MANAGED to DEFAULT because of DirectX9Ex migration.\n");
		Pool = ::D3DPOOL_DEFAULT;
		if (!(Usage & D3DUSAGE_DYNAMIC))
			Usage = Usage | D3DUSAGE_DYNAMIC;
	}

	D3D3DTEXTURE_DESC pDesc;
	pDesc.Width = Width;
	pDesc.Height = Height;
	pDesc.Depth = Depth;
	pDesc.Levels = MipLevels;
	pDesc.Usage = Usage;
	pDesc.Format = Format;
	pDesc.Pool = Pool;

	::LPDIRECT3DVOLUMETEXTURE9 baseTexture = 0;
	HRESULT hr = trampoline_D3DXCreateVolumeTextureFromFileInMemoryEx(wrappedDevice->GetD3D9Device(), pSrcData, SrcDataSize, Width, Height, Depth, MipLevels, Usage, Format, Pool, Filter, MipFilter, ColorKey, pSrcInfo, pPalette, &baseTexture);
	if (baseTexture) {
		D3D9Wrapper::IDirect3DVolumeTexture9 *wrapper = D3D9Wrapper::IDirect3DVolumeTexture9::GetDirect3DVolumeTexture9(baseTexture, wrappedDevice);
		if (ppVolumeTexture) {
			if (!(G->enable_hooks >= EnableHooksDX9::ALL) && wrapper) {
				*ppVolumeTexture = wrapper;
			}
			else {
				*ppVolumeTexture = (D3D9Wrapper::IDirect3DVolumeTexture9*)baseTexture;
			}
		}
	}

	if (ppVolumeTexture) LogDebug("  returns result=%x, handle=%p, wrapper=%p\n", hr, baseTexture, *ppVolumeTexture);
	LogInfo("  returns result=%x\n", hr);

	return hr;
}

HRESULT WINAPI Hooked_D3DXCreateVolumeTextureFromResource(
	_In_  D3D9Wrapper::IDirect3DDevice9*        pDevice,
	_In_  HMODULE                  hSrcModule,
	_In_  LPCTSTR                  pSrcResource,
	_Out_ D3D9Wrapper::IDirect3DVolumeTexture9* *ppVolumeTexture) {
	LogInfo("Hooked_D3DXCreateVolumeTextureFromResource Width=%d Height=%d Format=%d\n", D3DX_DEFAULT, D3DX_DEFAULT, ::D3DFMT_UNKNOWN);
	D3D9Wrapper::IDirect3DDevice9 *wrappedDevice = wrappedDevice9(pDevice);

	if (!wrappedDevice->GetD3D9Device())
	{
		LogInfo("  postponing call because device was not created yet.\n");

		D3D9Wrapper::IDirect3DVolumeTexture9 *wrapper;
		wrapper = D3D9Wrapper::IDirect3DVolumeTexture9::GetDirect3DVolumeTexture9((::LPDIRECT3DVOLUMETEXTURE9) 0, wrappedDevice);
		wrapper->_Height = D3DX_DEFAULT;
		wrapper->_Width = D3DX_DEFAULT;
		wrapper->_Depth = D3DX_DEFAULT;
		wrapper->_Levels = D3DX_DEFAULT;
		wrapper->_Usage = 0;
		wrapper->_Format = ::D3DFMT_UNKNOWN;
		wrapper->_Pool = ::D3DPOOL_DEFAULT;
		wrapper->_Device = wrappedDevice;
		wrapper->pendingCreateTexture = true;
		*ppVolumeTexture = wrapper;
		LogInfo("  returns handle=%p\n", wrapper);

		return S_OK;
	}

	D3D3DTEXTURE_DESC pDesc;
	pDesc.Width = D3DX_DEFAULT;
	pDesc.Height = D3DX_DEFAULT;
	pDesc.Depth = D3DX_DEFAULT;
	pDesc.Levels = D3DX_DEFAULT;
	pDesc.Usage = 0;
	pDesc.Format = ::D3DFMT_UNKNOWN;
	pDesc.Pool = ::D3DPOOL_DEFAULT;

	::LPDIRECT3DVOLUMETEXTURE9 baseTexture = 0;
	HRESULT hr = trampoline_D3DXCreateVolumeTextureFromResource(wrappedDevice->GetD3D9Device(), hSrcModule, pSrcResource, &baseTexture);
	if (baseTexture) {
		D3D9Wrapper::IDirect3DVolumeTexture9 *wrapper = D3D9Wrapper::IDirect3DVolumeTexture9::GetDirect3DVolumeTexture9(baseTexture, wrappedDevice);
		if (ppVolumeTexture) {
			if (!(G->enable_hooks >= EnableHooksDX9::ALL) && wrapper) {
				*ppVolumeTexture = wrapper;
			}
			else {
				*ppVolumeTexture = (D3D9Wrapper::IDirect3DVolumeTexture9*)baseTexture;
			}
		}
	}
	if (ppVolumeTexture) LogDebug("  returns result=%x, handle=%p, wrapper=%p\n", hr, baseTexture, *ppVolumeTexture);
	LogInfo("  returns result=%x\n", hr);

	return hr;
}

HRESULT WINAPI Hooked_D3DXCreateVolumeTextureFromResourceEx(
	_In_    D3D9Wrapper::IDirect3DDevice9*        pDevice,
	_In_    HMODULE                  hSrcModule,
	_In_    LPCTSTR                  pSrcResource,
	_In_    UINT                     Width,
	_In_    UINT                     Height,
	_In_    UINT                     Depth,
	_In_    UINT                     MipLevels,
	_In_    DWORD                    Usage,
	_In_    ::D3DFORMAT                Format,
	_In_    ::D3DPOOL                  Pool,
	_In_    DWORD                    Filter,
	_In_    DWORD                    MipFilter,
	_In_    ::D3DCOLOR                 ColorKey,
	_Inout_ ::D3DXIMAGE_INFO           *pSrcInfo,
	_Out_   PALETTEENTRY             *pPalette,
	_Out_   D3D9Wrapper::IDirect3DVolumeTexture9* *ppVolumeTexture) {
	LogInfo("Hooked_D3DXCreateVolumeTextureFromResourceEx Width=%d Height=%d Format=%d\n", Width, Height, Format);
	D3D9Wrapper::IDirect3DDevice9 *wrappedDevice = wrappedDevice9(pDevice);

	if (!wrappedDevice->GetD3D9Device())
	{
		LogInfo("  postponing call because device was not created yet.\n");

		D3D9Wrapper::IDirect3DVolumeTexture9 *wrapper;
		wrapper = D3D9Wrapper::IDirect3DVolumeTexture9::GetDirect3DVolumeTexture9((::LPDIRECT3DVOLUMETEXTURE9) 0, wrappedDevice);
		wrapper->_Height = Height;
		wrapper->_Width = Width;
		wrapper->_Depth = Depth;
		wrapper->_Levels = MipLevels;
		wrapper->_Usage = Usage;
		wrapper->_Format = Format;
		wrapper->_Pool = Pool;
		wrapper->_Device = wrappedDevice;
		wrapper->pendingCreateTexture = true;
		*ppVolumeTexture = wrapper;
		LogInfo("  returns handle=%p\n", wrapper);

		return S_OK;
	}

	if (Pool == ::D3DPOOL_MANAGED)
	{
		LogDebug("  Pool changed from MANAGED to DEFAULT because of DirectX9Ex migration.\n");
		Pool = ::D3DPOOL_DEFAULT;
		if (!(Usage & D3DUSAGE_DYNAMIC))
			Usage = Usage | D3DUSAGE_DYNAMIC;
	}

	D3D3DTEXTURE_DESC pDesc;
	pDesc.Width = Width;
	pDesc.Height = Height;
	pDesc.Depth = Depth;
	pDesc.Levels = MipLevels;
	pDesc.Usage = Usage;
	pDesc.Format = Format;
	pDesc.Pool = Pool;

	::LPDIRECT3DVOLUMETEXTURE9 baseTexture = 0;
	HRESULT hr = trampoline_D3DXCreateVolumeTextureFromResourceEx(wrappedDevice->GetD3D9Device(), hSrcModule, pSrcResource, Width, Height, Depth, MipLevels, Usage, Format, Pool, Filter, MipFilter, ColorKey, pSrcInfo, pPalette, &baseTexture);
	if (baseTexture) {
		D3D9Wrapper::IDirect3DVolumeTexture9 *wrapper = D3D9Wrapper::IDirect3DVolumeTexture9::GetDirect3DVolumeTexture9(baseTexture, wrappedDevice);
		if (ppVolumeTexture) {
			if (!(G->enable_hooks >= EnableHooksDX9::ALL) && wrapper) {
				*ppVolumeTexture = wrapper;
			}
			else {
				*ppVolumeTexture = (D3D9Wrapper::IDirect3DVolumeTexture9*)baseTexture;
			}
		}
	}

	if (ppVolumeTexture) LogDebug("  returns result=%x, handle=%p, wrapper=%p\n", hr, baseTexture, *ppVolumeTexture);
	LogInfo("  returns result=%x\n", hr);

	return hr;
}

HRESULT WINAPI Hooked_D3DXFillCubeTexture(
	_Out_ D3D9Wrapper::IDirect3DCubeTexture9* pTexture,
	_In_  ::LPD3DXFILL3D           pFunction,
	_In_  LPVOID                 pData) {
	LogInfo("Hooked_D3DXFillCubeTexture called with DestinationTexture=%p\n", pTexture);

	::IDirect3DCubeTexture9 *baseDestTexture = baseTexture9(pTexture);

	HRESULT hr;
	if (G->gForceStereo == 2) {
		D3D9Wrapper::IDirect3DCubeTexture9 *wrappedDest = wrappedTexture9(pTexture);

		::IDirect3DCubeTexture9* pDestTextureLeft = wrappedDest->DirectModeGetLeft();
		::IDirect3DCubeTexture9* pDestTextureRight = wrappedDest->DirectModeGetRight();

		hr = trampoline_D3DXFillCubeTexture(pDestTextureLeft, pFunction, pData);
		if (SUCCEEDED(hr)) {
			if (!pDestTextureRight) {
				LogDebug("Hooked_D3DXFillCubeTexture Direct Mode, INFO:  Destination is not stereo. Filled Left side only.\n");
			}
			else{
				hr = trampoline_D3DXFillCubeTexture(pDestTextureRight, pFunction, pData);
				if (FAILED(hr))
					LogDebug("Hooked_D3DXFillCubeTexture Direct Mode, ERROR: Failed to filled destination right.\n");
			}
		}
	}
	else {
		hr = trampoline_D3DXFillCubeTexture(baseDestTexture, pFunction, pData);
	}

	LogInfo("  returns result=%x\n", hr);

	return hr;
}

HRESULT WINAPI Hooked_D3DXFillCubeTextureTX(
	_In_ D3D9Wrapper::IDirect3DCubeTexture9* pTexture,
	_In_ ::LPD3DXTEXTURESHADER    pTextureShader) {
	LogInfo("Hooked_D3DXFillCubeTextureTX called with DestinationTexture=%p\n", pTexture);

	::IDirect3DCubeTexture9 *baseDestTexture = baseTexture9(pTexture);

	HRESULT hr;
	if (G->gForceStereo == 2) {
		D3D9Wrapper::IDirect3DCubeTexture9 *wrappedDest = wrappedTexture9(pTexture);

		::IDirect3DCubeTexture9* pDestTextureLeft = wrappedDest->DirectModeGetLeft();
		::IDirect3DCubeTexture9* pDestTextureRight = wrappedDest->DirectModeGetRight();

		hr = trampoline_D3DXFillCubeTextureTX(pDestTextureLeft, pTextureShader);
		if (SUCCEEDED(hr)) {
			if (!pDestTextureRight) {
				LogDebug("Hooked_D3DXFillCubeTextureTX Direct Mode, INFO:  Destination is not stereo. Filled Left side only.\n");
			}
			else {
				hr = trampoline_D3DXFillCubeTextureTX(pDestTextureRight, pTextureShader);
				if (FAILED(hr))
					LogDebug("Hooked_D3DXFillCubeTextureTX Direct Mode, ERROR: Failed to filled destination right.\n");
			}
		}
	}
	else {
		hr = trampoline_D3DXFillCubeTextureTX(baseDestTexture, pTextureShader);
	}

	LogInfo("  returns result=%x\n", hr);

	return hr;
}

HRESULT WINAPI Hooked_D3DXFillTexture(
	_Out_ D3D9Wrapper::IDirect3DTexture9* pTexture,
	_In_  ::LPD3DXFILL2D       pFunction,
	_In_  LPVOID             pData) {
	LogInfo("Hooked_D3DXFillCubeTexture called with DestinationTexture=%p\n", pTexture);

	::IDirect3DTexture9 *baseDestTexture = baseTexture9(pTexture);

	HRESULT hr;
	if (G->gForceStereo == 2) {
		D3D9Wrapper::IDirect3DTexture9 *wrappedDest = wrappedTexture9(pTexture);

		::IDirect3DTexture9* pDestTextureLeft = wrappedDest->DirectModeGetLeft();
		::IDirect3DTexture9* pDestTextureRight = wrappedDest->DirectModeGetRight();

		hr = trampoline_D3DXFillTexture(pDestTextureLeft, pFunction, pData);
		if (SUCCEEDED(hr)) {
			if (!pDestTextureRight) {
				LogDebug("Hooked_D3DXFillCubeTexture Direct Mode, INFO:  Destination is not stereo. Filled Left side only.\n");
			}
			else {
				hr = trampoline_D3DXFillTexture(pDestTextureRight, pFunction, pData);
				if (FAILED(hr))
					LogDebug("Hooked_D3DXFillCubeTexture Direct Mode, ERROR: Failed to filled destination right.\n");
			}
		}
	}
	else {
		hr = trampoline_D3DXFillTexture(baseDestTexture, pFunction, pData);
	}
	LogInfo("  returns result=%x\n", hr);

	return hr;
}

HRESULT WINAPI Hooked_D3DXFillTextureTX(
	_Inout_ D3D9Wrapper::IDirect3DTexture9*  pTexture,
	_In_    ::LPD3DXTEXTURESHADER pTextureShader) {
	LogInfo("Hooked_D3DXFillTextureTX called with DestinationTexture=%p\n", pTexture);

	::IDirect3DTexture9 *baseDestTexture = baseTexture9(pTexture);

	HRESULT hr;
	if (G->gForceStereo == 2) {
		D3D9Wrapper::IDirect3DTexture9 *wrappedDest = wrappedTexture9(pTexture);

		::IDirect3DTexture9* pDestTextureLeft = wrappedDest->DirectModeGetLeft();
		::IDirect3DTexture9* pDestTextureRight = wrappedDest->DirectModeGetRight();

		hr = trampoline_D3DXFillTextureTX(pDestTextureLeft, pTextureShader);
		if (SUCCEEDED(hr)) {
			if (!pDestTextureRight) {
				LogDebug("Hooked_D3DXFillTextureTX Direct Mode, INFO:  Destination is not stereo. Filled Left side only.\n");
			}
			else {
				hr = trampoline_D3DXFillTextureTX(pDestTextureRight, pTextureShader);
				if (FAILED(hr))
					LogDebug("Hooked_D3DXFillTextureTX Direct Mode, ERROR: Failed to filled destination right.\n");
			}
		}
	}
	else {
		hr = trampoline_D3DXFillTextureTX(baseDestTexture, pTextureShader);
	}

	LogInfo("  returns result=%x\n", hr);

	return hr;
}

HRESULT WINAPI Hooked_D3DXFillVolumeTexture(
	_Out_ D3D9Wrapper::IDirect3DVolumeTexture9* pTexture,
	_In_  ::LPD3DXFILL3D             pFunction,
	_In_  LPVOID                   pData) {
	LogInfo("Hooked_D3DXFillVolumeTexture called with DestinationTexture=%p\n", pTexture);

	::IDirect3DVolumeTexture9 *baseDestTexture = baseTexture9(pTexture);

	HRESULT hr = trampoline_D3DXFillVolumeTexture(baseDestTexture, pFunction, pData);
	LogInfo("  returns result=%x\n", hr);

	return hr;
}

HRESULT WINAPI Hooked_D3DXFillVolumeTextureTX(
	_In_ D3D9Wrapper::IDirect3DVolumeTexture9* pTexture,
	_In_ ::LPD3DXTEXTURESHADER      pTextureShader) {
	LogInfo("Hooked_D3DXFillVolumeTextureTX called with DestinationTexture=%p\n", pTexture);

	::IDirect3DVolumeTexture9 *baseDestTexture = baseTexture9(pTexture);

	HRESULT hr = trampoline_D3DXFillVolumeTextureTX(baseDestTexture, pTextureShader);
	LogInfo("  returns result=%x\n", hr);

	return hr;
}

HRESULT WINAPI Hooked_D3DXFilterTexture(
	_In_        D3D9Wrapper::IDirect3DBaseTexture9* pBaseTexture,
	_Out_ const PALETTEENTRY           *pPalette,
	_In_        UINT                   SrcLevel,
	_In_        DWORD                  MipFilter) {
	LogInfo("Hooked_D3DXFilterTexture called with DestinationTexture=%p\n", pBaseTexture);

	::IDirect3DBaseTexture9 *baseDestTexture = baseTexture9(pBaseTexture);

	HRESULT hr;
	if (G->gForceStereo == 2) {
		D3D9Wrapper::IDirect3DBaseTexture9 *wrappedDest = wrappedTexture9(pBaseTexture);
		::IDirect3DBaseTexture9* pDestTextureLeft = NULL;
		::IDirect3DBaseTexture9* pDestTextureRight = NULL;

		UnWrapTexture(wrappedDest, &pDestTextureLeft, &pDestTextureRight);

		hr = trampoline_D3DXFilterTexture(pDestTextureLeft, pPalette, SrcLevel, MipFilter);
		if (SUCCEEDED(hr)) {
			if (!pDestTextureRight) {
				LogDebug("Hooked_D3DXFillTextureTX Direct Mode, INFO:  Destination is not stereo. Filled Left side only.\n");
			}
			else {
				hr = trampoline_D3DXFilterTexture(pDestTextureRight, pPalette, SrcLevel, MipFilter);
				if (FAILED(hr))
					LogDebug("Hooked_D3DXFillTextureTX Direct Mode, ERROR: Failed to filled destination right.\n");
			}
		}
	}
	else {
		hr = trampoline_D3DXFilterTexture(baseDestTexture, pPalette, SrcLevel, MipFilter);
	}
	LogInfo("  returns result=%x\n", hr);

	return hr;
}

HRESULT WINAPI Hooked_D3DXLoadSurfaceFromFile(
	_In_          D3D9Wrapper::IDirect3DSurface9* pDestSurface,
	_In_    const PALETTEENTRY       *pDestPalette,
	_In_    const RECT               *pDestRect,
	_In_          LPCTSTR            pSrcFile,
	_In_    const RECT               *pSrcRect,
	_In_          DWORD              Filter,
	_In_          ::D3DCOLOR           ColorKey,
	_Inout_       ::D3DXIMAGE_INFO     *pSrcInfo) {
	LogDebug("Hooked_D3DXLoadSurfaceFromFile called using DestSurface=%p\n", pDestSurface);
	::LPDIRECT3DSURFACE9 baseDestSurface = baseSurface9(pDestSurface);
	HRESULT hr;
	if (G->gForceStereo == 2) {
		D3D9Wrapper::IDirect3DSurface9* pWrappedDest = wrappedSurface9(pDestSurface);
		::IDirect3DSurface9* pDestSurfaceLeft = pWrappedDest->DirectModeGetLeft();
		::IDirect3DSurface9* pDestSurfaceRight = pWrappedDest->DirectModeGetRight();
		hr = trampoline_D3DXLoadSurfaceFromFile(pDestSurfaceLeft, pDestPalette, pDestRect, pSrcFile, pSrcRect, Filter, ColorKey, pSrcInfo);
		if (SUCCEEDED(hr)) {
			if (!pDestSurfaceRight) {
				LogDebug("INFO: Hooked_D3DXLoadSurfaceFromFile - Destination is not stereo. Loaded Left side only.\n");
			}
			else{
				hr = trampoline_D3DXLoadSurfaceFromFile(pDestSurfaceRight, pDestPalette, pDestRect, pSrcFile, pSrcRect, Filter, ColorKey, pSrcInfo);
				if (FAILED(hr))
					LogDebug("ERROR: Hooked_D3DXLoadSurfaceFromFile - Failed to load right side.\n");
			}
		}
	}
	else {
		hr = trampoline_D3DXLoadSurfaceFromFile(baseDestSurface, pDestPalette, pDestRect, pSrcFile, pSrcRect, Filter, ColorKey, pSrcInfo);
	}
	LogInfo("  returns result=%x\n", hr);

	return hr;
}

HRESULT WINAPI Hooked_D3DXLoadSurfaceFromFileInMemory(
	_In_          D3D9Wrapper::IDirect3DSurface9* pDestSurface,
	_In_    const PALETTEENTRY       *pDestPalette,
	_In_    const RECT               *pDestRect,
	_In_          LPCVOID            pSrcData,
	_In_          UINT               SrcData,
	_In_    const RECT               *pSrcRect,
	_In_          DWORD              Filter,
	_In_          ::D3DCOLOR           ColorKey,
	_Inout_       ::D3DXIMAGE_INFO     *pSrcInfo) {
	LogDebug("Hooked_D3DXLoadSurfaceFromFileInMemory called using DestSurface=%p\n", pDestSurface);
	::LPDIRECT3DSURFACE9 baseDestSurface = baseSurface9(pDestSurface);
	HRESULT hr;
	if (G->gForceStereo == 2) {
		D3D9Wrapper::IDirect3DSurface9* pWrappedDest = wrappedSurface9(pDestSurface);
		::IDirect3DSurface9* pDestSurfaceLeft = pWrappedDest->DirectModeGetLeft();
		::IDirect3DSurface9* pDestSurfaceRight = pWrappedDest->DirectModeGetRight();
		hr = trampoline_D3DXLoadSurfaceFromFileInMemory(pDestSurfaceLeft, pDestPalette, pDestRect, pSrcData, SrcData, pSrcRect, Filter, ColorKey, pSrcInfo);
		if (SUCCEEDED(hr)) {
			if (!pDestSurfaceRight) {
				LogDebug("INFO: Hooked_D3DXLoadSurfaceFromFileInMemory - Destination is not stereo. Loaded Left side only.\n");
			}
			else {
				hr = trampoline_D3DXLoadSurfaceFromFileInMemory(pDestSurfaceRight, pDestPalette, pDestRect, pSrcData, SrcData, pSrcRect, Filter, ColorKey, pSrcInfo);
				if (FAILED(hr))
					LogDebug("ERROR: Hooked_D3DXLoadSurfaceFromFileInMemory - Failed to load right side.\n");
			}
		}
	}
	else {
		hr = trampoline_D3DXLoadSurfaceFromFileInMemory(baseDestSurface, pDestPalette, pDestRect, pSrcData, SrcData, pSrcRect, Filter, ColorKey, pSrcInfo);
	}
	LogInfo("  returns result=%x\n", hr);

	return hr;
}

HRESULT WINAPI Hooked_D3DXLoadSurfaceFromMemory(
	_In_       D3D9Wrapper::IDirect3DSurface9* pDestSurface,
	_In_ const PALETTEENTRY       *pDestPalette,
	_In_ const RECT               *pDestRect,
	_In_       LPCVOID            pSrcMemory,
	_In_       ::D3DFORMAT          SrcFormat,
	_In_       UINT               SrcPitch,
	_In_ const PALETTEENTRY       *pSrcPalette,
	_In_ const RECT               *pSrcRect,
	_In_       DWORD              Filter,
	_In_       ::D3DCOLOR           ColorKey) {
	LogDebug("Hooked_D3DXLoadSurfaceFromMemory called using DestSurface=%p\n", pDestSurface);
	::LPDIRECT3DSURFACE9 baseDestSurface = baseSurface9(pDestSurface);
	HRESULT hr;
	if (G->gForceStereo == 2) {
		D3D9Wrapper::IDirect3DSurface9* pWrappedDest = wrappedSurface9(pDestSurface);
		::IDirect3DSurface9* pDestSurfaceLeft = pWrappedDest->DirectModeGetLeft();
		::IDirect3DSurface9* pDestSurfaceRight = pWrappedDest->DirectModeGetRight();
		hr = trampoline_D3DXLoadSurfaceFromMemory(pDestSurfaceLeft, pDestPalette, pDestRect, pSrcMemory, SrcFormat, SrcPitch, pSrcPalette, pSrcRect, Filter, ColorKey);
		if (SUCCEEDED(hr)) {
			if (!pDestSurfaceRight) {
				LogDebug("INFO: Hooked_D3DXLoadSurfaceFromMemory - Destination is not stereo. Loaded Left side only.\n");
			}
			else {
				hr = trampoline_D3DXLoadSurfaceFromMemory(pDestSurfaceRight, pDestPalette, pDestRect, pSrcMemory, SrcFormat, SrcPitch, pSrcPalette, pSrcRect, Filter, ColorKey);
				if (FAILED(hr))
					LogDebug("ERROR: Hooked_D3DXLoadSurfaceFromMemory - Failed to load right side.\n");
			}
		}
	}
	else {
		hr = trampoline_D3DXLoadSurfaceFromMemory(baseDestSurface, pDestPalette, pDestRect, pSrcMemory, SrcFormat, SrcPitch, pSrcPalette, pSrcRect, Filter, ColorKey);
	}
	LogInfo("  returns result=%x\n", hr);

	return hr;
}

HRESULT WINAPI Hooked_D3DXLoadSurfaceFromResource(
	_In_          D3D9Wrapper::IDirect3DSurface9* pDestSurface,
	_In_    const PALETTEENTRY       *pDestPalette,
	_In_    const RECT               *pDestRect,
	_In_          HMODULE            hSrcModule,
	_In_          LPCTSTR            pSrcResource,
	_In_    const RECT               *pSrcRect,
	_In_          DWORD              Filter,
	_In_          ::D3DCOLOR           ColorKey,
	_Inout_       ::D3DXIMAGE_INFO     *pSrcInfo) {
	LogDebug("Hooked_D3DXLoadSurfaceFromResource called using DestSurface=%p\n", pDestSurface);
	::LPDIRECT3DSURFACE9 baseDestSurface = baseSurface9(pDestSurface);
	HRESULT hr;
	if (G->gForceStereo == 2) {
		D3D9Wrapper::IDirect3DSurface9* pWrappedDest = wrappedSurface9(pDestSurface);
		::IDirect3DSurface9* pDestSurfaceLeft = pWrappedDest->DirectModeGetLeft();
		::IDirect3DSurface9* pDestSurfaceRight = pWrappedDest->DirectModeGetRight();
		hr = trampoline_D3DXLoadSurfaceFromResource(pDestSurfaceLeft, pDestPalette, pDestRect, hSrcModule, pSrcResource, pSrcRect, Filter, ColorKey, pSrcInfo);
		if (SUCCEEDED(hr)) {
			if (!pDestSurfaceRight) {
				LogDebug("INFO: Hooked_D3DXLoadSurfaceFromResource - Destination is not stereo. Loaded Left side only.\n");
			}
			else {
				hr = trampoline_D3DXLoadSurfaceFromResource(pDestSurfaceRight, pDestPalette, pDestRect, hSrcModule, pSrcResource, pSrcRect, Filter, ColorKey, pSrcInfo);
				if (FAILED(hr))
					LogDebug("ERROR: Hooked_D3DXLoadSurfaceFromResource - Failed to load right side.\n");
			}
		}
	}
	else {
		hr = trampoline_D3DXLoadSurfaceFromResource(baseDestSurface, pDestPalette, pDestRect, hSrcModule, pSrcResource, pSrcRect, Filter, ColorKey, pSrcInfo);
	}
	LogInfo("  returns result=%x\n", hr);

	return hr;
}
HRESULT WINAPI Hooked_D3DXLoadSurfaceFromSurface(
	_In_       D3D9Wrapper::IDirect3DSurface9* pDestSurface,
	_In_ const PALETTEENTRY       *pDestPalette,
	_In_ const RECT               *pDestRect,
	_In_       D3D9Wrapper::IDirect3DSurface9* pSrcSurface,
	_In_ const PALETTEENTRY       *pSrcPalette,
	_In_ const RECT               *pSrcRect,
	_In_       DWORD              Filter,
	_In_       ::D3DCOLOR           ColorKey) {
	LogDebug("Hooked_D3DXLoadSurfaceFromSurface called using SourceSurface=%p, DestSurface=%p\n", pSrcSurface, pDestSurface);
	::LPDIRECT3DSURFACE9 baseSourceSurface = baseSurface9(pSrcSurface);
	::LPDIRECT3DSURFACE9 baseDestSurface = baseSurface9(pDestSurface);
	HRESULT hr;
	if (G->gForceStereo == 2) {
		D3D9Wrapper::IDirect3DSurface9* pWrappedSource = wrappedSurface9(pSrcSurface);
		D3D9Wrapper::IDirect3DSurface9* pWrappedDest = wrappedSurface9(pDestSurface);
		::IDirect3DSurface9* pSourceSurfaceLeft = pWrappedSource->DirectModeGetLeft();
		::IDirect3DSurface9* pSourceSurfaceRight = pWrappedSource->DirectModeGetRight();
		::IDirect3DSurface9* pDestSurfaceLeft = pWrappedDest->DirectModeGetLeft();
		::IDirect3DSurface9* pDestSurfaceRight = pWrappedDest->DirectModeGetRight();

		hr = trampoline_D3DXLoadSurfaceFromSurface(pDestSurfaceLeft, pDestPalette, pDestRect, pSourceSurfaceLeft, pSrcPalette, pSrcRect, Filter, ColorKey);
		if (SUCCEEDED(hr)) {
			if (!pSourceSurfaceRight && pDestSurfaceRight) {
				LogDebug("Hooked_D3DXLoadSurfaceFromSurface, Direct Mode, - Source is not stereo, destination is stereo. Copying source to both sides of destination.\n");
				hr = trampoline_D3DXLoadSurfaceFromSurface(pDestSurfaceRight, pDestPalette, pDestRect, pSourceSurfaceLeft, pSrcPalette, pSrcRect, Filter, ColorKey);
				if (FAILED(hr))
					LogDebug("ERROR: Hooked_D3DXLoadSurfaceFromSurface - Failed to copy source left to destination right.\n");
			}
			else if (pSourceSurfaceRight && !pDestSurfaceRight) {
				LogDebug("INFO: Hooked_D3DXLoadSurfaceFromSurface - Source is stereo, destination is not stereo. Copied Left side only.\n");
			}
			else if (pSourceSurfaceRight && pDestSurfaceRight) {
				hr = trampoline_D3DXLoadSurfaceFromSurface(pDestSurfaceRight, pDestPalette, pDestRect, pSourceSurfaceRight, pSrcPalette, pSrcRect, Filter, ColorKey);
				if (FAILED(hr))
					LogDebug("ERROR: Hooked_D3DXLoadSurfaceFromSurface - Failed to copy source right to destination right.\n");
			}
		}
	}
	else {
		hr = trampoline_D3DXLoadSurfaceFromSurface(baseDestSurface, pDestPalette, pDestRect, baseSourceSurface, pSrcPalette, pSrcRect, Filter, ColorKey);
	}
	LogInfo("  returns result=%x\n", hr);

	return hr;
}

HRESULT WINAPI Hooked_D3DXLoadVolumeFromFile(
	_In_       D3D9Wrapper::IDirect3DVolume9* pDestVolume,
	_In_ const PALETTEENTRY      *pDestPalette,
	_In_ const ::D3DBOX            *pDestBox,
	_In_       LPCTSTR           pSrcFile,
	_In_ const ::D3DBOX            *pSrcBox,
	_In_       DWORD             Filter,
	_In_       ::D3DCOLOR          ColorKey,
	_In_       ::D3DXIMAGE_INFO    *pSrcInfo) {
	LogDebug("Hooked_D3DXLoadVolumeFromFile called using DestSurface=%p\n", pDestVolume);
	::LPDIRECT3DVOLUME9 baseDestVolume = baseVolume9(pDestVolume);
	HRESULT hr = trampoline_D3DXLoadVolumeFromFile(baseDestVolume, pDestPalette, pDestBox, pSrcFile, pSrcBox, Filter, ColorKey, pSrcInfo);
	LogInfo("  returns result=%x\n", hr);

	return hr;
}

HRESULT WINAPI Hooked_D3DXLoadVolumeFromFileInMemory(
	_In_       D3D9Wrapper::IDirect3DVolume9* pDestVolume,
	_In_ const PALETTEENTRY      *pDestPalette,
	_In_ const ::D3DBOX            *pDestBox,
	_In_       LPCVOID           pSrcData,
	_In_       UINT              SrcDataSize,
	_In_ const ::D3DBOX            *pSrcBox,
	_In_       DWORD             Filter,
	_In_       ::D3DCOLOR          ColorKey,
	_In_       ::D3DXIMAGE_INFO    *pSrcInfo) {
	LogDebug("Hooked_D3DXLoadVolumeFromFileInMemory called using DestSurface=%p\n", pDestVolume);
	::LPDIRECT3DVOLUME9 baseDestVolume = baseVolume9(pDestVolume);
	HRESULT hr = trampoline_D3DXLoadVolumeFromFileInMemory(baseDestVolume, pDestPalette, pDestBox, pSrcData, SrcDataSize, pSrcBox, Filter, ColorKey, pSrcInfo);
	LogInfo("  returns result=%x\n", hr);

	return hr;
}

HRESULT WINAPI Hooked_D3DXLoadVolumeFromMemory(
	_In_       D3D9Wrapper::IDirect3DVolume9* pDestVolume,
	_In_ const PALETTEENTRY      *pDestPalette,
	_In_ const ::D3DBOX            *pDestBox,
	_In_       LPCVOID           pSrcMemory,
	_In_       ::D3DFORMAT         SrcFormat,
	_In_       UINT              SrcRowPitch,
	_In_       UINT              SrcSlicePitch,
	_In_ const PALETTEENTRY      *pSrcPalette,
	_In_ const ::D3DBOX            *pSrcBox,
	_In_       DWORD             Filter,
	_In_       ::D3DCOLOR          ColorKey) {
	LogDebug("Hooked_D3DXLoadVolumeFromMemory called using DestSurface=%p\n", pDestVolume);
	::LPDIRECT3DVOLUME9 baseDestVolume = baseVolume9(pDestVolume);
	HRESULT hr = trampoline_D3DXLoadVolumeFromMemory(baseDestVolume, pDestPalette, pDestBox, pSrcMemory, SrcFormat, SrcRowPitch, SrcSlicePitch, pSrcPalette, pSrcBox, Filter, ColorKey);
	LogInfo("  returns result=%x\n", hr);

	return hr;
}

HRESULT WINAPI Hooked_D3DXLoadVolumeFromResource(
	D3D9Wrapper::IDirect3DVolume9*         pDestVolume,
	CONST PALETTEENTRY*       pDestPalette,
	CONST ::D3DBOX*             pDestBox,
	HMODULE                   hSrcModule,
	LPCWSTR                   pSrcResource,
	CONST ::D3DBOX*             pSrcBox,
	DWORD                     Filter,
	::D3DCOLOR                  ColorKey,
	::D3DXIMAGE_INFO*           pSrcInfo) {
	LogDebug("Hooked_D3DXLoadVolumeFromResource called using DestSurface=%p\n", pDestVolume);
	::LPDIRECT3DVOLUME9 baseDestVolume = baseVolume9(pDestVolume);
	HRESULT hr = trampoline_D3DXLoadVolumeFromResource(baseDestVolume, pDestPalette, pDestBox, hSrcModule, pSrcResource, pSrcBox, Filter, ColorKey, pSrcInfo);
	LogInfo("  returns result=%x\n", hr);

	return hr;
}

HRESULT WINAPI Hooked_D3DXLoadVolumeFromVolume(
	_In_       D3D9Wrapper::IDirect3DVolume9* pDestVolume,
	_In_ const PALETTEENTRY      *pDestPalette,
	_In_ const ::D3DBOX            *pDestBox,
	_In_       D3D9Wrapper::IDirect3DVolume9* pSrcVolume,
	_In_ const PALETTEENTRY      *pSrcPalette,
	_In_ const ::D3DBOX            *pSrcBox,
	_In_       DWORD             Filter,
	_In_       ::D3DCOLOR          ColorKey) {
	LogDebug("Hooked_D3DXLoadVolumeFromVolume called using DestSurface=%p\n", pDestVolume);
	::LPDIRECT3DVOLUME9 baseSourceVolume = baseVolume9(pSrcVolume);
	::LPDIRECT3DVOLUME9 baseDestVolume = baseVolume9(pDestVolume);
	HRESULT hr = trampoline_D3DXLoadVolumeFromVolume(baseDestVolume, pDestPalette, pDestBox, baseSourceVolume, pSrcPalette, pSrcBox, Filter, ColorKey);
	LogInfo("  returns result=%x\n", hr);

	return hr;
}
