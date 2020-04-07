#pragma once
#include "Main.h"


static HRESULT(WINAPI *trampoline_D3DXComputeNormalMap)(
	_Out_       ::LPDIRECT3DTEXTURE9 pTexture,
	_In_        ::LPDIRECT3DTEXTURE9 pSrcTexture,
	_In_  const PALETTEENTRY       *pSrcPalette,
	_In_        DWORD              Flags,
	_In_        DWORD              Channel,
	_In_        FLOAT              Amplitude) = ::D3DXComputeNormalMap;

 static HRESULT(WINAPI *trampoline_D3DXCreateCubeTexture)(
	_In_  ::LPDIRECT3DDEVICE9      pDevice,
	_In_  UINT                   Size,
	_In_  UINT                   MipLevels,
	_In_  DWORD                  Usage,
	_In_  ::D3DFORMAT              Format,
	_In_  ::D3DPOOL                Pool,
	_Out_ ::LPDIRECT3DCUBETEXTURE9 *ppCubeTexture
	) = ::D3DXCreateCubeTexture;


 static HRESULT(WINAPI *trampoline_D3DXCreateCubeTextureFromFile)(
	_In_  ::LPDIRECT3DDEVICE9      pDevice,
	_In_  LPCTSTR                pSrcFile,
	_Out_ ::LPDIRECT3DCUBETEXTURE9 *ppCubeTexture
	) = ::D3DXCreateCubeTextureFromFile;
 static HRESULT(WINAPI *trampoline_D3DXCreateCubeTextureFromFileEx)(
	_In_  ::LPDIRECT3DDEVICE9      pDevice,
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
	_Out_ ::LPDIRECT3DCUBETEXTURE9 *ppCubeTexture
	) = ::D3DXCreateCubeTextureFromFileEx;
 static HRESULT(WINAPI *trampoline_D3DXCreateCubeTextureFromFileInMemory)(
	_In_  ::LPDIRECT3DDEVICE9      pDevice,
	_In_  LPCVOID                pSrcData,
	_In_  UINT                   SrcDataSize,
	_Out_ ::LPDIRECT3DCUBETEXTURE9 *ppCubeTexture
	) = ::D3DXCreateCubeTextureFromFileInMemory;

 static HRESULT(WINAPI *trampoline_D3DXCreateCubeTextureFromFileInMemoryEx)(
	_In_    ::LPDIRECT3DDEVICE9      pDevice,
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
	_Out_   ::LPDIRECT3DCUBETEXTURE9 *ppCubeTexture
	) = ::D3DXCreateCubeTextureFromFileInMemoryEx;

 static HRESULT(WINAPI *trampoline_D3DXCreateCubeTextureFromResource)(
	_In_  ::LPDIRECT3DDEVICE9      pDevice,
	_In_  HMODULE                hSrcModule,
	_In_  LPCTSTR                pSrcResource,
	_Out_ ::LPDIRECT3DCUBETEXTURE9 *ppCubeTexture
	) = ::D3DXCreateCubeTextureFromResource;

 static HRESULT(WINAPI *trampoline_D3DXCreateCubeTextureFromResourceEx)(
	_In_    ::LPDIRECT3DDEVICE9      pDevice,
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
	_Out_   ::LPDIRECT3DCUBETEXTURE9 *ppCubeTexture
	) = ::D3DXCreateCubeTextureFromResourceEx;

 static HRESULT(WINAPI *trampoline_D3DXCreateTexture)(
	_In_  ::LPDIRECT3DDEVICE9  pDevice,
	_In_  UINT               Width,
	_In_  UINT               Height,
	_In_  UINT               MipLevels,
	_In_  DWORD              Usage,
	_In_  ::D3DFORMAT          Format,
	_In_  ::D3DPOOL            Pool,
	_Out_ ::LPDIRECT3DTEXTURE9 *ppTexture
	) = ::D3DXCreateTexture;


 static HRESULT(WINAPI *trampoline_D3DXCreateTextureFromFile)(
	_In_  ::LPDIRECT3DDEVICE9  pDevice,
	_In_  LPCTSTR            pSrcFile,
	_Out_ ::LPDIRECT3DTEXTURE9 *ppTexture
	) = ::D3DXCreateTextureFromFile;

 static HRESULT(WINAPI *trampoline_D3DXCreateTextureFromFileEx)(
	_In_    ::LPDIRECT3DDEVICE9  pDevice,
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
	_Out_   ::LPDIRECT3DTEXTURE9 *ppTexture
	) = ::D3DXCreateTextureFromFileEx;

 static HRESULT(WINAPI *trampoline_D3DXCreateTextureFromFileInMemory)(
	_In_  ::LPDIRECT3DDEVICE9  pDevice,
	_In_  LPCVOID            pSrcData,
	_In_  UINT               SrcDataSize,
	_Out_ ::LPDIRECT3DTEXTURE9 *ppTexture
	) = ::D3DXCreateTextureFromFileInMemory;

 static HRESULT(WINAPI *trampoline_D3DXCreateTextureFromFileInMemoryEx)(
	_In_    ::LPDIRECT3DDEVICE9  pDevice,
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
	_Out_   ::LPDIRECT3DTEXTURE9 *ppTexture
	) = ::D3DXCreateTextureFromFileInMemoryEx;

 static HRESULT(WINAPI *trampoline_D3DXCreateTextureFromResource)(
	_In_  ::LPDIRECT3DDEVICE9  pDevice,
	_In_  HMODULE            hSrcModule,
	_In_  LPCTSTR            pSrcResource,
	_Out_ ::LPDIRECT3DTEXTURE9 *ppTexture
	) = ::D3DXCreateTextureFromResource;

 static HRESULT(WINAPI *trampoline_D3DXCreateTextureFromResourceEx)(
	_In_    ::LPDIRECT3DDEVICE9  pDevice,
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
	_Out_   ::LPDIRECT3DTEXTURE9 *ppTexture
	) = ::D3DXCreateTextureFromResourceEx;
 static HRESULT(WINAPI *trampoline_D3DXCreateVolumeTextureFromFileEx)(
	_In_    ::LPDIRECT3DDEVICE9        pDevice,
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
	_Out_   ::LPDIRECT3DVOLUMETEXTURE9 *ppTexture
	) = ::D3DXCreateVolumeTextureFromFileEx;

 static HRESULT(WINAPI *trampoline_D3DXCreateVolumeTexture)(
	 _In_  ::LPDIRECT3DDEVICE9        pDevice,
	 _In_  UINT                     Width,
	 _In_  UINT                     Height,
	 _In_  UINT                     Depth,
	 _In_  UINT                     MipLevels,
	 _In_  DWORD                    Usage,
	 _In_  ::D3DFORMAT                Format,
	 _In_  ::D3DPOOL                  Pool,
	 _Out_ ::LPDIRECT3DVOLUMETEXTURE9 *ppVolumeTexture
	 ) = ::D3DXCreateVolumeTexture;

 static HRESULT(WINAPI *trampoline_D3DXCreateVolumeTextureFromFile)(
	 _In_  ::LPDIRECT3DDEVICE9        pDevice,
	 _In_  LPCTSTR                  pSrcFile,
	 _Out_ ::LPDIRECT3DVOLUMETEXTURE9 *ppVolumeTexture
	 ) = ::D3DXCreateVolumeTextureFromFile;

 static HRESULT(WINAPI *trampoline_D3DXCreateVolumeTextureFromFileInMemory)(
	::LPDIRECT3DDEVICE9         pDevice,
	LPCVOID                   pSrcData,
	UINT                      SrcDataSize,
	::LPDIRECT3DVOLUMETEXTURE9* ppVolumeTexture
	) = ::D3DXCreateVolumeTextureFromFileInMemory;

 static HRESULT(WINAPI *trampoline_D3DXCreateVolumeTextureFromFileInMemoryEx)(
	_In_    ::LPDIRECT3DDEVICE9        pDevice,
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
	_Out_   ::LPDIRECT3DVOLUMETEXTURE9 *ppVolumeTexture
	) = ::D3DXCreateVolumeTextureFromFileInMemoryEx;

 static HRESULT(WINAPI *trampoline_D3DXCreateVolumeTextureFromResource)(
	_In_  ::LPDIRECT3DDEVICE9        pDevice,
	_In_  HMODULE                  hSrcModule,
	_In_  LPCTSTR                  pSrcResource,
	_Out_ ::LPDIRECT3DVOLUMETEXTURE9 *ppVolumeTexture
	) = ::D3DXCreateVolumeTextureFromResource;

 static HRESULT(WINAPI *trampoline_D3DXCreateVolumeTextureFromResourceEx)(
	_In_    ::LPDIRECT3DDEVICE9        pDevice,
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
	_Out_   ::LPDIRECT3DVOLUMETEXTURE9 *ppVolumeTexture
	) = ::D3DXCreateVolumeTextureFromResourceEx;

 static HRESULT(WINAPI *trampoline_D3DXFillCubeTexture)(
	_Out_ ::LPDIRECT3DCUBETEXTURE9 pTexture,
	_In_  ::LPD3DXFILL3D           pFunction,
	_In_  LPVOID                 pData
	) = ::D3DXFillCubeTexture;

 static HRESULT(WINAPI *trampoline_D3DXFillCubeTextureTX)(
	_In_ ::LPDIRECT3DCUBETEXTURE9 pTexture,
	_In_ ::LPD3DXTEXTURESHADER    pTextureShader
	) = ::D3DXFillCubeTextureTX;

 static HRESULT(WINAPI *trampoline_D3DXFillTexture)(
	_Out_ ::LPDIRECT3DTEXTURE9 pTexture,
	_In_  ::LPD3DXFILL2D       pFunction,
	_In_  LPVOID             pData
	) = ::D3DXFillTexture;

 static HRESULT(WINAPI *trampoline_D3DXFillTextureTX)(
	_Inout_ ::LPDIRECT3DTEXTURE9  pTexture,
	_In_    ::LPD3DXTEXTURESHADER pTextureShader
	) = ::D3DXFillTextureTX;

 static HRESULT(WINAPI *trampoline_D3DXFillVolumeTexture)(
	_Out_ ::LPDIRECT3DVOLUMETEXTURE9 pTexture,
	_In_  ::LPD3DXFILL3D             pFunction,
	_In_  LPVOID                   pData
	) = ::D3DXFillVolumeTexture;

 static HRESULT(WINAPI *trampoline_D3DXFillVolumeTextureTX)(
	_In_ ::LPDIRECT3DVOLUMETEXTURE9 pTexture,
	_In_ ::LPD3DXTEXTURESHADER      pTextureShader
	) = ::D3DXFillVolumeTextureTX;

 static HRESULT(WINAPI *trampoline_D3DXFilterTexture)(
	_In_        ::LPDIRECT3DBASETEXTURE9 pBaseTexture,
	_Out_ const PALETTEENTRY           *pPalette,
	_In_        UINT                   SrcLevel,
	_In_        DWORD                  MipFilter
	) = ::D3DXFilterTexture;

 static HRESULT(WINAPI *trampoline_D3DXLoadSurfaceFromFile)(
	_In_          ::LPDIRECT3DSURFACE9 pDestSurface,
	_In_    const PALETTEENTRY       *pDestPalette,
	_In_    const RECT               *pDestRect,
	_In_          LPCTSTR            pSrcFile,
	_In_    const RECT               *pSrcRect,
	_In_          DWORD              Filter,
	_In_          ::D3DCOLOR           ColorKey,
	_Inout_       ::D3DXIMAGE_INFO     *pSrcInfo
	) = ::D3DXLoadSurfaceFromFile;

 static HRESULT(WINAPI *trampoline_D3DXLoadSurfaceFromFileInMemory)(
	_In_          ::LPDIRECT3DSURFACE9 pDestSurface,
	_In_    const PALETTEENTRY       *pDestPalette,
	_In_    const RECT               *pDestRect,
	_In_          LPCVOID            pSrcData,
	_In_          UINT               SrcData,
	_In_    const RECT               *pSrcRect,
	_In_          DWORD              Filter,
	_In_          ::D3DCOLOR           ColorKey,
	_Inout_       ::D3DXIMAGE_INFO     *pSrcInfo
	) = ::D3DXLoadSurfaceFromFileInMemory;

 static HRESULT(WINAPI *trampoline_D3DXLoadSurfaceFromMemory)(
	_In_       ::LPDIRECT3DSURFACE9 pDestSurface,
	_In_ const PALETTEENTRY       *pDestPalette,
	_In_ const RECT               *pDestRect,
	_In_       LPCVOID            pSrcMemory,
	_In_       ::D3DFORMAT          SrcFormat,
	_In_       UINT               SrcPitch,
	_In_ const PALETTEENTRY       *pSrcPalette,
	_In_ const RECT               *pSrcRect,
	_In_       DWORD              Filter,
	_In_       ::D3DCOLOR           ColorKey
	) = ::D3DXLoadSurfaceFromMemory;

 static HRESULT(WINAPI *trampoline_D3DXLoadSurfaceFromResource)(
	_In_          ::LPDIRECT3DSURFACE9 pDestSurface,
	_In_    const PALETTEENTRY       *pDestPalette,
	_In_    const RECT               *pDestRect,
	_In_          HMODULE            hSrcModule,
	_In_          LPCTSTR            pSrcResource,
	_In_    const RECT               *pSrcRect,
	_In_          DWORD              Filter,
	_In_          ::D3DCOLOR           ColorKey,
	_Inout_       ::D3DXIMAGE_INFO     *pSrcInfo
	) = ::D3DXLoadSurfaceFromResource;

 static HRESULT(WINAPI *trampoline_D3DXLoadSurfaceFromSurface)(
	_In_       ::LPDIRECT3DSURFACE9 pDestSurface,
	_In_ const PALETTEENTRY       *pDestPalette,
	_In_ const RECT               *pDestRect,
	_In_       ::LPDIRECT3DSURFACE9 pSrcSurface,
	_In_ const PALETTEENTRY       *pSrcPalette,
	_In_ const RECT               *pSrcRect,
	_In_       DWORD              Filter,
	_In_       ::D3DCOLOR           ColorKey
	) = ::D3DXLoadSurfaceFromSurface;

 static HRESULT(WINAPI *trampoline_D3DXLoadVolumeFromFile)(
	_In_       ::LPDIRECT3DVOLUME9 pDestVolume,
	_In_ const PALETTEENTRY      *pDestPalette,
	_In_ const ::D3DBOX            *pDestBox,
	_In_       LPCTSTR           pSrcFile,
	_In_ const ::D3DBOX            *pSrcBox,
	_In_       DWORD             Filter,
	_In_       ::D3DCOLOR          ColorKey,
	_In_       ::D3DXIMAGE_INFO    *pSrcInfo
	) = ::D3DXLoadVolumeFromFile;

 static HRESULT(WINAPI *trampoline_D3DXLoadVolumeFromFileInMemory)(
	_In_       ::LPDIRECT3DVOLUME9 pDestVolume,
	_In_ const PALETTEENTRY      *pDestPalette,
	_In_ const ::D3DBOX            *pDestBox,
	_In_       LPCVOID           pSrcData,
	_In_       UINT              SrcDataSize,
	_In_ const ::D3DBOX            *pSrcBox,
	_In_       DWORD             Filter,
	_In_       ::D3DCOLOR          ColorKey,
	_In_       ::D3DXIMAGE_INFO    *pSrcInfo
	) = ::D3DXLoadVolumeFromFileInMemory;

 static HRESULT(WINAPI *trampoline_D3DXLoadVolumeFromMemory)(
	_In_       ::LPDIRECT3DVOLUME9 pDestVolume,
	_In_ const PALETTEENTRY      *pDestPalette,
	_In_ const ::D3DBOX            *pDestBox,
	_In_       LPCVOID           pSrcMemory,
	_In_       ::D3DFORMAT         SrcFormat,
	_In_       UINT              SrcRowPitch,
	_In_       UINT              SrcSlicePitch,
	_In_ const PALETTEENTRY      *pSrcPalette,
	_In_ const ::D3DBOX            *pSrcBox,
	_In_       DWORD             Filter,
	_In_       ::D3DCOLOR          ColorKey
	) = ::D3DXLoadVolumeFromMemory;
 static HRESULT(WINAPI *trampoline_D3DXLoadVolumeFromResource)(
	 ::LPDIRECT3DVOLUME9         pDestVolume,
	 CONST PALETTEENTRY*       pDestPalette,
	 CONST ::D3DBOX*             pDestBox,
	 HMODULE                   hSrcModule,
	 LPCWSTR                   pSrcResource,
	 CONST ::D3DBOX*             pSrcBox,
	 DWORD                     Filter,
	 ::D3DCOLOR                  ColorKey,
	 ::D3DXIMAGE_INFO*           pSrcInfo
	 ) = ::D3DXLoadVolumeFromResource;
 static HRESULT(WINAPI *trampoline_D3DXLoadVolumeFromVolume)(
	 _In_       ::LPDIRECT3DVOLUME9 pDestVolume,
	 _In_ const PALETTEENTRY      *pDestPalette,
	 _In_ const ::D3DBOX            *pDestBox,
	 _In_       ::LPDIRECT3DVOLUME9 pSrcVolume,
	 _In_ const PALETTEENTRY      *pSrcPalette,
	 _In_ const ::D3DBOX            *pSrcBox,
	 _In_       DWORD             Filter,
	 _In_       ::D3DCOLOR          ColorKey
	 ) = ::D3DXLoadVolumeFromVolume;
 HRESULT WINAPI Hooked_D3DXComputeNormalMap(
	 _Out_       D3D9Wrapper::IDirect3DTexture9* pTexture,
	 _In_        D3D9Wrapper::IDirect3DTexture9* pSrcTexture,
	 _In_  const PALETTEENTRY       *pSrcPalette,
	 _In_        DWORD              Flags,
	 _In_        DWORD              Channel,
	 _In_        FLOAT              Amplitude);
 HRESULT WINAPI Hooked_D3DXCreateCubeTexture(
	 _In_  D3D9Wrapper::IDirect3DDevice9*      pDevice,
	 _In_  UINT                   Size,
	 _In_  UINT                   MipLevels,
	 _In_  DWORD                  Usage,
	 _In_  ::D3DFORMAT              Format,
	 _In_  ::D3DPOOL                Pool,
	 _Out_ D3D9Wrapper::IDirect3DCubeTexture9* *ppCubeTexture);
 HRESULT WINAPI Hooked_D3DXCreateCubeTextureFromFile(
	 _In_  D3D9Wrapper::IDirect3DDevice9*      pDevice,
	 _In_  LPCTSTR                pSrcFile,
	 _Out_ D3D9Wrapper::IDirect3DCubeTexture9* *ppCubeTexture);
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
	 _Out_ D3D9Wrapper::IDirect3DCubeTexture9* *ppCubeTexture);

 HRESULT WINAPI Hooked_D3DXCreateCubeTextureFromFileInMemory(
	 _In_  D3D9Wrapper::IDirect3DDevice9*      pDevice,
	 _In_  LPCVOID                pSrcData,
	 _In_  UINT                   SrcDataSize,
	 _Out_ D3D9Wrapper::IDirect3DCubeTexture9* *ppCubeTexture);
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
	 _Out_   D3D9Wrapper::IDirect3DCubeTexture9* *ppCubeTexture);
 HRESULT WINAPI Hooked_D3DXCreateCubeTextureFromResource(
	 _In_  D3D9Wrapper::IDirect3DDevice9*      pDevice,
	 _In_  HMODULE                hSrcModule,
	 _In_  LPCTSTR                pSrcResource,
	 _Out_ D3D9Wrapper::IDirect3DCubeTexture9* *ppCubeTexture);
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
	 _Out_   D3D9Wrapper::IDirect3DCubeTexture9* *ppCubeTexture);
 HRESULT WINAPI Hooked_D3DXCreateTexture(
	 _In_  D3D9Wrapper::IDirect3DDevice9*  pDevice,
	 _In_  UINT               Width,
	 _In_  UINT               Height,
	 _In_  UINT               MipLevels,
	 _In_  DWORD              Usage,
	 _In_  ::D3DFORMAT          Format,
	 _In_  ::D3DPOOL            Pool,
	 _Out_ D3D9Wrapper::IDirect3DTexture9* *ppTexture);
 HRESULT WINAPI Hooked_D3DXCreateTextureFromFile(
	 _In_  D3D9Wrapper::IDirect3DDevice9*  pDevice,
	 _In_  LPCTSTR            pSrcFile,
	 _Out_ D3D9Wrapper::IDirect3DTexture9* *ppTexture);
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
	 _Out_   D3D9Wrapper::IDirect3DTexture9* *ppTexture);
 HRESULT WINAPI Hooked_D3DXCreateTextureFromFileInMemory(
	 _In_  D3D9Wrapper::IDirect3DDevice9*  pDevice,
	 _In_  LPCVOID            pSrcData,
	 _In_  UINT               SrcDataSize,
	 _Out_ D3D9Wrapper::IDirect3DTexture9* *ppTexture);
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
	 _Out_   D3D9Wrapper::IDirect3DTexture9* *ppTexture);
 HRESULT WINAPI Hooked_D3DXCreateTextureFromResource(
	 _In_  D3D9Wrapper::IDirect3DDevice9*  pDevice,
	 _In_  HMODULE            hSrcModule,
	 _In_  LPCTSTR            pSrcResource,
	 _Out_ D3D9Wrapper::IDirect3DTexture9* *ppTexture);
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
	 _Out_   D3D9Wrapper::IDirect3DTexture9* *ppTexture);
 HRESULT WINAPI Hooked_D3DXCreateVolumeTexture(
	 _In_  D3D9Wrapper::IDirect3DDevice9*        pDevice,
	 _In_  UINT                     Width,
	 _In_  UINT                     Height,
	 _In_  UINT                     Depth,
	 _In_  UINT                     MipLevels,
	 _In_  DWORD                    Usage,
	 _In_  ::D3DFORMAT                Format,
	 _In_  ::D3DPOOL                  Pool,
	 _Out_ D3D9Wrapper::IDirect3DVolumeTexture9* *ppVolumeTexture);
 HRESULT WINAPI Hooked_D3DXCreateVolumeTextureFromFile(
	 _In_  D3D9Wrapper::IDirect3DDevice9*        pDevice,
	 _In_  LPCTSTR                  pSrcFile,
	 _Out_ D3D9Wrapper::IDirect3DVolumeTexture9* *ppVolumeTexture);

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
	 _Out_   D3D9Wrapper::IDirect3DVolumeTexture9* *ppTexture);
 HRESULT WINAPI Hooked_D3DXCreateVolumeTextureFromFileInMemory(
	 D3D9Wrapper::IDirect3DDevice9*         pDevice,
	 LPCVOID                   pSrcData,
	 UINT                      SrcDataSize,
	 D3D9Wrapper::IDirect3DVolumeTexture9** ppVolumeTexture);
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
	 _Out_   D3D9Wrapper::IDirect3DVolumeTexture9* *ppVolumeTexture);
 HRESULT WINAPI Hooked_D3DXCreateVolumeTextureFromResource(
	 _In_  D3D9Wrapper::IDirect3DDevice9*        pDevice,
	 _In_  HMODULE                  hSrcModule,
	 _In_  LPCTSTR                  pSrcResource,
	 _Out_ D3D9Wrapper::IDirect3DVolumeTexture9* *ppVolumeTexture);
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
	 _Out_   D3D9Wrapper::IDirect3DVolumeTexture9* *ppVolumeTexture);
 HRESULT WINAPI Hooked_D3DXFillCubeTexture(
	 _Out_ D3D9Wrapper::IDirect3DCubeTexture9* pTexture,
	 _In_  ::LPD3DXFILL3D           pFunction,
	 _In_  LPVOID                 pData);
 HRESULT WINAPI Hooked_D3DXFillCubeTextureTX(
	 _In_ D3D9Wrapper::IDirect3DCubeTexture9* pTexture,
	 _In_ ::LPD3DXTEXTURESHADER    pTextureShader);
 HRESULT WINAPI Hooked_D3DXFillTexture(
	 _Out_ D3D9Wrapper::IDirect3DTexture9* pTexture,
	 _In_  ::LPD3DXFILL2D       pFunction,
	 _In_  LPVOID             pData);
 HRESULT WINAPI Hooked_D3DXFillTextureTX(
	 _Inout_ D3D9Wrapper::IDirect3DTexture9*  pTexture,
	 _In_    ::LPD3DXTEXTURESHADER pTextureShader);
 HRESULT WINAPI Hooked_D3DXFillVolumeTexture(
	 _Out_ D3D9Wrapper::IDirect3DVolumeTexture9* pTexture,
	 _In_  ::LPD3DXFILL3D             pFunction,
	 _In_  LPVOID                   pData);
 HRESULT WINAPI Hooked_D3DXFillVolumeTextureTX(
	 _In_ D3D9Wrapper::IDirect3DVolumeTexture9* pTexture,
	 _In_ ::LPD3DXTEXTURESHADER      pTextureShader);
 HRESULT WINAPI Hooked_D3DXFilterTexture(
	 _In_        D3D9Wrapper::IDirect3DBaseTexture9* pBaseTexture,
	 _Out_ const PALETTEENTRY           *pPalette,
	 _In_        UINT                   SrcLevel,
	 _In_        DWORD                  MipFilter);
 HRESULT WINAPI Hooked_D3DXLoadSurfaceFromFile(
	 _In_          D3D9Wrapper::IDirect3DSurface9* pDestSurface,
	 _In_    const PALETTEENTRY       *pDestPalette,
	 _In_    const RECT               *pDestRect,
	 _In_          LPCTSTR            pSrcFile,
	 _In_    const RECT               *pSrcRect,
	 _In_          DWORD              Filter,
	 _In_          ::D3DCOLOR           ColorKey,
	 _Inout_       ::D3DXIMAGE_INFO     *pSrcInfo);
 HRESULT WINAPI Hooked_D3DXLoadSurfaceFromFileInMemory(
	 _In_          D3D9Wrapper::IDirect3DSurface9* pDestSurface,
	 _In_    const PALETTEENTRY       *pDestPalette,
	 _In_    const RECT               *pDestRect,
	 _In_          LPCVOID            pSrcData,
	 _In_          UINT               SrcData,
	 _In_    const RECT               *pSrcRect,
	 _In_          DWORD              Filter,
	 _In_          ::D3DCOLOR           ColorKey,
	 _Inout_       ::D3DXIMAGE_INFO     *pSrcInfo);
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
	 _In_       ::D3DCOLOR           ColorKey);
 HRESULT WINAPI Hooked_D3DXLoadSurfaceFromResource(
	 _In_          D3D9Wrapper::IDirect3DSurface9* pDestSurface,
	 _In_    const PALETTEENTRY       *pDestPalette,
	 _In_    const RECT               *pDestRect,
	 _In_          HMODULE            hSrcModule,
	 _In_          LPCTSTR            pSrcResource,
	 _In_    const RECT               *pSrcRect,
	 _In_          DWORD              Filter,
	 _In_          ::D3DCOLOR           ColorKey,
	 _Inout_       ::D3DXIMAGE_INFO     *pSrcInfo);
 HRESULT WINAPI Hooked_D3DXLoadSurfaceFromSurface(
	 _In_       D3D9Wrapper::IDirect3DSurface9* pDestSurface,
	 _In_ const PALETTEENTRY       *pDestPalette,
	 _In_ const RECT               *pDestRect,
	 _In_       D3D9Wrapper::IDirect3DSurface9* pSrcSurface,
	 _In_ const PALETTEENTRY       *pSrcPalette,
	 _In_ const RECT               *pSrcRect,
	 _In_       DWORD              Filter,
	 _In_       ::D3DCOLOR           ColorKey);

 HRESULT WINAPI Hooked_D3DXLoadVolumeFromFile(
	 _In_       D3D9Wrapper::IDirect3DVolume9* pDestVolume,
	 _In_ const PALETTEENTRY      *pDestPalette,
	 _In_ const ::D3DBOX            *pDestBox,
	 _In_       LPCTSTR           pSrcFile,
	 _In_ const ::D3DBOX            *pSrcBox,
	 _In_       DWORD             Filter,
	 _In_       ::D3DCOLOR          ColorKey,
	 _In_       ::D3DXIMAGE_INFO    *pSrcInfo);

 HRESULT WINAPI Hooked_D3DXLoadVolumeFromFileInMemory(
	 _In_       D3D9Wrapper::IDirect3DVolume9* pDestVolume,
	 _In_ const PALETTEENTRY      *pDestPalette,
	 _In_ const ::D3DBOX            *pDestBox,
	 _In_       LPCVOID           pSrcData,
	 _In_       UINT              SrcDataSize,
	 _In_ const ::D3DBOX            *pSrcBox,
	 _In_       DWORD             Filter,
	 _In_       ::D3DCOLOR          ColorKey,
	 _In_       ::D3DXIMAGE_INFO    *pSrcInfo);
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
	_In_       ::D3DCOLOR          ColorKey);

 HRESULT WINAPI Hooked_D3DXLoadVolumeFromResource(
	D3D9Wrapper::IDirect3DVolume9*         pDestVolume,
	CONST PALETTEENTRY*       pDestPalette,
	CONST ::D3DBOX*             pDestBox,
	HMODULE                   hSrcModule,
	LPCWSTR                   pSrcResource,
	CONST ::D3DBOX*             pSrcBox,
	DWORD                     Filter,
	::D3DCOLOR                  ColorKey,
	::D3DXIMAGE_INFO*           pSrcInfo);

 HRESULT WINAPI Hooked_D3DXLoadVolumeFromVolume(
	_In_       D3D9Wrapper::IDirect3DVolume9* pDestVolume,
	_In_ const PALETTEENTRY      *pDestPalette,
	_In_ const ::D3DBOX            *pDestBox,
	_In_       D3D9Wrapper::IDirect3DVolume9* pSrcVolume,
	_In_ const PALETTEENTRY      *pSrcPalette,
	_In_ const ::D3DBOX            *pSrcBox,
	_In_       DWORD             Filter,
	_In_       ::D3DCOLOR          ColorKey);
