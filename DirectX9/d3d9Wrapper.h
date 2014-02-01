#pragma once

#include <D3D9.h>

class IDirect3D9;
class IDirect3DDevice9;
class IDirect3DSwapChain9;
class IDirect3DSurface9;
class IDirect3DVertexDeclaration9;
class IDirect3DTexture9;
class IDirect3DVertexBuffer9;
class IDirect3DIndexBuffer9;
class IDirect3DQuery9;
class IDirect3DVertexShader9;
class IDirect3DPixelShader9;

typedef HRESULT (WINAPI *D3DCREATE)(UINT, D3D9Base::LPDIRECT3D9EX *ppD3D);
typedef int (WINAPI *D3DPERF_BeginEvent)(D3D9Base::D3DCOLOR color, LPCWSTR name);
typedef int (WINAPI *D3DPERF_EndEvent)(void);
typedef DWORD (WINAPI *D3DPERF_GetStatus)(void);
typedef BOOL (WINAPI *D3DPERF_QueryRepeatFrame)(void);
typedef void (WINAPI *D3DPERF_SetMarker)(D3D9Base::D3DCOLOR color, LPCWSTR name);
typedef void (WINAPI *D3DPERF_SetOptions)(DWORD options);
typedef void (WINAPI *D3DPERF_SetRegion)(D3D9Base::D3DCOLOR color, LPCWSTR name); 	
typedef void (WINAPI *DebugSetLevel)(int a1, int a2);
typedef void (WINAPI *DebugSetMute)(int a);
typedef void* (WINAPI *Direct3DShaderValidatorCreate9)(void);
typedef void (WINAPI *PSGPError)(void *D3DFE_PROCESSVERTICES, int PSGPERRORID, unsigned int a);
typedef void (WINAPI *PSGPSampleTexture)(void *D3DFE_PROCESSVERTICES, unsigned int a, float (* const b)[4], unsigned int c, float (* const d)[4]);

class IDirect3DUnknown
{
public:
    IUnknown*   m_pUnk;
    ULONG       m_ulRef;

    IDirect3DUnknown(IUnknown* pUnk)
    {
        m_pUnk = pUnk;
        m_ulRef = 1;
    }

    /*** IUnknown methods ***/
    STDMETHOD(QueryInterface)(THIS_ REFIID riid, void** ppvObj);

    STDMETHOD_(ULONG,AddRef)(THIS)
    {
		++m_ulRef;
        return m_pUnk->AddRef();
    }

    STDMETHOD_(ULONG,Release)(THIS)
    {
		--m_ulRef;
		ULONG ulRef = m_pUnk->Release();

        if (ulRef == 0)
        {
			m_pUnk = 0;
            delete this;
            return 0;
        }
        return ulRef;
    }
};

class IDirect3D9Base : public IDirect3DUnknown
{
public:
    static ThreadSafePointerSet	m_List;

    IDirect3D9Base(IUnknown* pUnk)
		: IDirect3DUnknown(pUnk) {}

    /*** IDirect3DUnknown methods ***/
	STDMETHOD_(ULONG,AddRef)(THIS);
    STDMETHOD_(ULONG,Release)(THIS);

    /*** IDirect3D9 methods ***/
    STDMETHOD(RegisterSoftwareDevice)(THIS_ void* pInitializeFunction);
    STDMETHOD_(UINT, GetAdapterCount)(THIS) PURE;
    STDMETHOD(GetAdapterIdentifier)(THIS_ UINT Adapter,DWORD Flags,D3D9Base::D3DADAPTER_IDENTIFIER9* pIdentifier) PURE;
    STDMETHOD_(UINT, GetAdapterModeCount)(THIS_ UINT Adapter,D3D9Base::D3DFORMAT Format) PURE;
    STDMETHOD(EnumAdapterModes)(THIS_ UINT Adapter,D3D9Base::D3DFORMAT Format,UINT Mode,D3D9Base::D3DDISPLAYMODE* pMode) PURE;
    STDMETHOD(GetAdapterDisplayMode)(THIS_ UINT Adapter,D3D9Base::D3DDISPLAYMODE* pMode) PURE;
    STDMETHOD(CheckDeviceType)(THIS_ UINT Adapter,D3D9Base::D3DDEVTYPE DevType,D3D9Base::D3DFORMAT AdapterFormat,D3D9Base::D3DFORMAT BackBufferFormat,BOOL bWindowed) PURE;
    STDMETHOD(CheckDeviceFormat)(THIS_ UINT Adapter,D3D9Base::D3DDEVTYPE DeviceType,D3D9Base::D3DFORMAT AdapterFormat,DWORD Usage,D3D9Base::D3DRESOURCETYPE RType,D3D9Base::D3DFORMAT CheckFormat) PURE;
    STDMETHOD(CheckDeviceMultiSampleType)(THIS_ UINT Adapter,D3D9Base::D3DDEVTYPE DeviceType,D3D9Base::D3DFORMAT SurfaceFormat,BOOL Windowed,D3D9Base::D3DMULTISAMPLE_TYPE MultiSampleType,DWORD* pQualityLevels) PURE;
    STDMETHOD(CheckDepthStencilMatch)(THIS_ UINT Adapter,D3D9Base::D3DDEVTYPE DeviceType,D3D9Base::D3DFORMAT AdapterFormat,D3D9Base::D3DFORMAT RenderTargetFormat,D3D9Base::D3DFORMAT DepthStencilFormat) PURE;
    STDMETHOD(CheckDeviceFormatConversion)(THIS_ UINT Adapter,D3D9Base::D3DDEVTYPE DeviceType,D3D9Base::D3DFORMAT SourceFormat,D3D9Base::D3DFORMAT TargetFormat) PURE;
    STDMETHOD(GetDeviceCaps)(THIS_ UINT Adapter,D3D9Base::D3DDEVTYPE DeviceType,D3D9Base::D3DCAPS9* pCaps) PURE;
    STDMETHOD_(HMONITOR, GetAdapterMonitor)(THIS_ UINT Adapter) PURE;
    STDMETHOD(CreateDevice)(THIS_ UINT Adapter,D3D9Base::D3DDEVTYPE DeviceType,HWND hFocusWindow,DWORD BehaviorFlags,D3D9Base::D3DPRESENT_PARAMETERS* pPresentationParameters,D3D9Wrapper::IDirect3DDevice9** ppReturnedDeviceInterface) PURE;
};

class IDirect3D9 : public IDirect3D9Base
{
public:
	IDirect3D9(D3D9Base::LPDIRECT3D9EX pD3D);
    static IDirect3D9* GetDirect3D(D3D9Base::LPDIRECT3D9EX pD3D);
    __forceinline D3D9Base::LPDIRECT3D9EX GetDirect3D9() { return (D3D9Base::LPDIRECT3D9EX)m_pUnk; }

    /*** IDirect3D9 methods ***/
    STDMETHOD_(UINT, GetAdapterCount)(THIS);
    STDMETHOD(GetAdapterIdentifier)(THIS_ UINT Adapter,DWORD Flags,D3D9Base::D3DADAPTER_IDENTIFIER9* pIdentifier);
    STDMETHOD_(UINT, GetAdapterModeCount)(THIS_ UINT Adapter,D3D9Base::D3DFORMAT Format);
    STDMETHOD(EnumAdapterModes)(THIS_ UINT Adapter,D3D9Base::D3DFORMAT Format,UINT Mode,D3D9Base::D3DDISPLAYMODE* pMode);
    STDMETHOD(GetAdapterDisplayMode)(THIS_ UINT Adapter,D3D9Base::D3DDISPLAYMODE* pMode);
    STDMETHOD(CheckDeviceType)(THIS_ UINT Adapter,D3D9Base::D3DDEVTYPE DevType,D3D9Base::D3DFORMAT AdapterFormat,D3D9Base::D3DFORMAT BackBufferFormat,BOOL bWindowed);
    STDMETHOD(CheckDeviceFormat)(THIS_ UINT Adapter,D3D9Base::D3DDEVTYPE DeviceType,D3D9Base::D3DFORMAT AdapterFormat,DWORD Usage,D3D9Base::D3DRESOURCETYPE RType,D3D9Base::D3DFORMAT CheckFormat);
    STDMETHOD(CheckDeviceMultiSampleType)(THIS_ UINT Adapter,D3D9Base::D3DDEVTYPE DeviceType,D3D9Base::D3DFORMAT SurfaceFormat,BOOL Windowed,D3D9Base::D3DMULTISAMPLE_TYPE MultiSampleType,DWORD* pQualityLevels);
    STDMETHOD(CheckDepthStencilMatch)(THIS_ UINT Adapter,D3D9Base::D3DDEVTYPE DeviceType,D3D9Base::D3DFORMAT AdapterFormat,D3D9Base::D3DFORMAT RenderTargetFormat,D3D9Base::D3DFORMAT DepthStencilFormat);
    STDMETHOD(CheckDeviceFormatConversion)(THIS_ UINT Adapter,D3D9Base::D3DDEVTYPE DeviceType,D3D9Base::D3DFORMAT SourceFormat,D3D9Base::D3DFORMAT TargetFormat);
    STDMETHOD(GetDeviceCaps)(THIS_ UINT Adapter,D3D9Base::D3DDEVTYPE DeviceType,D3D9Base::D3DCAPS9* pCaps);
    STDMETHOD_(HMONITOR, GetAdapterMonitor)(THIS_ UINT Adapter);
    STDMETHOD(CreateDevice)(THIS_ UINT Adapter,D3D9Base::D3DDEVTYPE DeviceType,HWND hFocusWindow,DWORD BehaviorFlags,D3D9Base::D3DPRESENT_PARAMETERS* pPresentationParameters,IDirect3DDevice9** ppReturnedDeviceInterface);

	/*** IDirect3D9Ex methods ***/
    STDMETHOD_(UINT, GetAdapterModeCountEx)(THIS_ UINT Adapter,CONST D3D9Base::D3DDISPLAYMODEFILTER* pFilter );
    STDMETHOD(EnumAdapterModesEx)(THIS_ UINT Adapter,CONST D3D9Base::D3DDISPLAYMODEFILTER* pFilter,UINT Mode,D3D9Base::D3DDISPLAYMODEEX* pMode);
    STDMETHOD(GetAdapterDisplayModeEx)(THIS_ UINT Adapter,D3D9Base::D3DDISPLAYMODEEX* pMode,D3D9Base::D3DDISPLAYROTATION* pRotation);
    STDMETHOD(CreateDeviceEx)(THIS_ UINT Adapter,D3D9Base::D3DDEVTYPE DeviceType,HWND hFocusWindow,DWORD BehaviorFlags,D3D9Base::D3DPRESENT_PARAMETERS* pPresentationParameters,D3D9Base::D3DDISPLAYMODEEX* pFullscreenDisplayMode,D3D9Wrapper::IDirect3DDevice9** ppReturnedDeviceInterface);
    STDMETHOD(GetAdapterLUID)(THIS_ UINT Adapter,LUID * pLUID);
};

class IDirect3DDevice9 : public IDirect3DUnknown
{
public:
    static ThreadSafePointerSet	 m_List;
	// Creation parameters
	HANDLE _CreateThread;
    D3D9Base::LPDIRECT3D9EX _pD3D;
	UINT _Adapter;
	D3D9Base::D3DDEVTYPE _DeviceType;
	HWND _hFocusWindow;
	DWORD _BehaviorFlags;
	D3D9Base::D3DDISPLAYMODEEX _pFullscreenDisplayMode;
	D3D9Base::D3DPRESENT_PARAMETERS _pPresentationParameters;	
	IDirect3DSurface9 *pendingCreateDepthStencilSurface;
	IDirect3DSurface9 *pendingSetDepthStencilSurface;

    IDirect3DDevice9(D3D9Base::LPDIRECT3DDEVICE9EX pDevice);
    static IDirect3DDevice9* GetDirect3DDevice(D3D9Base::LPDIRECT3DDEVICE9EX pDevice);
	__forceinline D3D9Base::LPDIRECT3DDEVICE9EX GetD3D9Device() { return (D3D9Base::LPDIRECT3DDEVICE9EX) m_pUnk; }

    /*** IDirect3DUnknown methods ***/
	STDMETHOD_(ULONG,AddRef)(THIS);
    STDMETHOD_(ULONG,Release)(THIS);

    /*** IDirect3DDevice9 methods ***/
    STDMETHOD(TestCooperativeLevel)(THIS);
    STDMETHOD_(UINT, GetAvailableTextureMem)(THIS);
    STDMETHOD(EvictManagedResources)(THIS);
    STDMETHOD(GetDirect3D)(THIS_ IDirect3D9** ppD3D9);
    STDMETHOD(GetDeviceCaps)(THIS_ D3D9Base::D3DCAPS9* pCaps);
    STDMETHOD(GetDisplayMode)(THIS_ UINT iSwapChain,D3D9Base::D3DDISPLAYMODE* pMode);
    STDMETHOD(GetCreationParameters)(THIS_ D3D9Base::D3DDEVICE_CREATION_PARAMETERS *pParameters);
    STDMETHOD(SetCursorProperties)(THIS_ UINT XHotSpot,UINT YHotSpot,D3D9Wrapper::IDirect3DSurface9 *pCursorBitmap);
    STDMETHOD_(void, SetCursorPosition)(THIS_ int X,int Y,DWORD Flags);
    STDMETHOD_(BOOL, ShowCursor)(THIS_ BOOL bShow);
    STDMETHOD(CreateAdditionalSwapChain)(THIS_ D3D9Base::D3DPRESENT_PARAMETERS* pPresentationParameters,IDirect3DSwapChain9** pSwapChain);
    STDMETHOD(GetSwapChain)(THIS_ UINT iSwapChain,IDirect3DSwapChain9** pSwapChain);
    STDMETHOD_(UINT, GetNumberOfSwapChains)(THIS);
    STDMETHOD(Reset)(THIS_ D3D9Base::D3DPRESENT_PARAMETERS* pPresentationParameters);
    STDMETHOD(Present)(THIS_ CONST RECT* pSourceRect,CONST RECT* pDestRect,HWND hDestWindowOverride,CONST RGNDATA* pDirtyRegion);
    STDMETHOD(GetBackBuffer)(THIS_ UINT iSwapChain,UINT iBackBuffer,D3D9Base::D3DBACKBUFFER_TYPE Type, IDirect3DSurface9 **ppBackBuffer);
    STDMETHOD(GetRasterStatus)(THIS_ UINT iSwapChain,D3D9Base::D3DRASTER_STATUS* pRasterStatus);
    STDMETHOD(SetDialogBoxMode)(THIS_ BOOL bEnableDialogs);
    STDMETHOD_(void, SetGammaRamp)(THIS_ UINT iSwapChain,DWORD Flags,CONST D3D9Base::D3DGAMMARAMP* pRamp);
    STDMETHOD_(void, GetGammaRamp)(THIS_ UINT iSwapChain,D3D9Base::D3DGAMMARAMP* pRamp);
    STDMETHOD(CreateTexture)(THIS_ UINT Width,UINT Height,UINT Levels,DWORD Usage,D3D9Base::D3DFORMAT Format,D3D9Base::D3DPOOL Pool, IDirect3DTexture9** ppTexture,HANDLE* pSharedHandle);
    STDMETHOD(CreateVolumeTexture)(THIS_ UINT Width,UINT Height,UINT Depth,UINT Levels,DWORD Usage,D3D9Base::D3DFORMAT Format,D3D9Base::D3DPOOL Pool,D3D9Base::IDirect3DVolumeTexture9** ppVolumeTexture,HANDLE* pSharedHandle);
    STDMETHOD(CreateCubeTexture)(THIS_ UINT EdgeLength,UINT Levels,DWORD Usage,D3D9Base::D3DFORMAT Format,D3D9Base::D3DPOOL Pool,D3D9Base::IDirect3DCubeTexture9** ppCubeTexture,HANDLE* pSharedHandle);
    STDMETHOD(CreateVertexBuffer)(THIS_ UINT Length,DWORD Usage,DWORD FVF,D3D9Base::D3DPOOL Pool, IDirect3DVertexBuffer9 **ppVertexBuffer,HANDLE* pSharedHandle);
    STDMETHOD(CreateIndexBuffer)(THIS_ UINT Length,DWORD Usage,D3D9Base::D3DFORMAT Format,D3D9Base::D3DPOOL Pool, IDirect3DIndexBuffer9 **ppIndexBuffer,HANDLE* pSharedHandle);
    STDMETHOD(CreateRenderTarget)(THIS_ UINT Width,UINT Height,D3D9Base::D3DFORMAT Format,D3D9Base::D3DMULTISAMPLE_TYPE MultiSample,DWORD MultisampleQuality,BOOL Lockable, IDirect3DSurface9 **ppSurface,HANDLE* pSharedHandle);
    STDMETHOD(CreateDepthStencilSurface)(THIS_ UINT Width,UINT Height,D3D9Base::D3DFORMAT Format,D3D9Base::D3DMULTISAMPLE_TYPE MultiSample,DWORD MultisampleQuality,BOOL Discard,D3D9Wrapper::IDirect3DSurface9 **ppSurface,HANDLE* pSharedHandle);
    STDMETHOD(UpdateSurface)(THIS_ D3D9Wrapper::IDirect3DSurface9 *pSourceSurface,CONST RECT* pSourceRect,D3D9Wrapper::IDirect3DSurface9 *pDestinationSurface,CONST POINT* pDestPoint);
    STDMETHOD(UpdateTexture)(THIS_ D3D9Base::LPDIRECT3DBASETEXTURE9 pSourceTexture,D3D9Base::LPDIRECT3DBASETEXTURE9 pDestinationTexture);
    STDMETHOD(GetRenderTargetData)(THIS_ D3D9Wrapper::IDirect3DSurface9 *pRenderTarget, D3D9Wrapper::IDirect3DSurface9 *pDestSurface);
    STDMETHOD(GetFrontBufferData)(THIS_ UINT iSwapChain, D3D9Wrapper::IDirect3DSurface9 *pDestSurface);
    STDMETHOD(StretchRect)(THIS_ D3D9Wrapper::IDirect3DSurface9 *pSourceSurface,CONST RECT* pSourceRect, D3D9Wrapper::IDirect3DSurface9 *pDestSurface,CONST RECT* pDestRect,D3D9Base::D3DTEXTUREFILTERTYPE Filter);
    STDMETHOD(ColorFill)(THIS_ D3D9Wrapper::IDirect3DSurface9 *pSurface,CONST RECT* pRect,D3D9Base::D3DCOLOR color);
    STDMETHOD(CreateOffscreenPlainSurface)(THIS_ UINT Width,UINT Height,D3D9Base::D3DFORMAT Format,D3D9Base::D3DPOOL Pool, IDirect3DSurface9 **ppSurface,HANDLE* pSharedHandle);
    STDMETHOD(SetRenderTarget)(THIS_ DWORD RenderTargetIndex, IDirect3DSurface9 *pRenderTarget);
    STDMETHOD(GetRenderTarget)(THIS_ DWORD RenderTargetIndex, IDirect3DSurface9 **ppRenderTarget);
    STDMETHOD(SetDepthStencilSurface)(THIS_ IDirect3DSurface9 *pNewZStencil);
    STDMETHOD(GetDepthStencilSurface)(THIS_ IDirect3DSurface9 **ppZStencilSurface);
    STDMETHOD(BeginScene)(THIS);
    STDMETHOD(EndScene)(THIS);
    STDMETHOD(Clear)(THIS_ DWORD Count,CONST D3D9Base::D3DRECT* pRects,DWORD Flags,D3D9Base::D3DCOLOR Color,float Z,DWORD Stencil);
    STDMETHOD(SetTransform)(THIS_ D3D9Base::D3DTRANSFORMSTATETYPE State,CONST D3D9Base::D3DMATRIX* pMatrix);
    STDMETHOD(GetTransform)(THIS_ D3D9Base::D3DTRANSFORMSTATETYPE State,D3D9Base::D3DMATRIX* pMatrix);
    STDMETHOD(MultiplyTransform)(THIS_ D3D9Base::D3DTRANSFORMSTATETYPE,CONST D3D9Base::D3DMATRIX*);
    STDMETHOD(SetViewport)(THIS_ CONST D3D9Base::D3DVIEWPORT9* pViewport);
    STDMETHOD(GetViewport)(THIS_ D3D9Base::D3DVIEWPORT9* pViewport);
    STDMETHOD(SetMaterial)(THIS_ CONST D3D9Base::D3DMATERIAL9* pMaterial);
    STDMETHOD(GetMaterial)(THIS_ D3D9Base::D3DMATERIAL9* pMaterial);
    STDMETHOD(SetLight)(THIS_ DWORD Index,CONST D3D9Base::D3DLIGHT9*);
    STDMETHOD(GetLight)(THIS_ DWORD Index,D3D9Base::D3DLIGHT9*);
    STDMETHOD(LightEnable)(THIS_ DWORD Index,BOOL Enable);
    STDMETHOD(GetLightEnable)(THIS_ DWORD Index,BOOL* pEnable);
    STDMETHOD(SetClipPlane)(THIS_ DWORD Index,CONST float* pPlane);
    STDMETHOD(GetClipPlane)(THIS_ DWORD Index,float* pPlane);
    STDMETHOD(SetRenderState)(THIS_ D3D9Base::D3DRENDERSTATETYPE State,DWORD Value);
    STDMETHOD(GetRenderState)(THIS_ D3D9Base::D3DRENDERSTATETYPE State,DWORD* pValue);
    STDMETHOD(CreateStateBlock)(THIS_ D3D9Base::D3DSTATEBLOCKTYPE Type,D3D9Base::IDirect3DStateBlock9** ppSB);
    STDMETHOD(BeginStateBlock)(THIS);
    STDMETHOD(EndStateBlock)(THIS_ D3D9Base::IDirect3DStateBlock9** ppSB);
    STDMETHOD(SetClipStatus)(THIS_ CONST D3D9Base::D3DCLIPSTATUS9* pClipStatus);
    STDMETHOD(GetClipStatus)(THIS_ D3D9Base::D3DCLIPSTATUS9* pClipStatus);
    STDMETHOD(GetTexture)(THIS_ DWORD Stage,D3D9Base::LPDIRECT3DBASETEXTURE9* ppTexture);
    STDMETHOD(SetTexture)(THIS_ DWORD Stage,D3D9Base::LPDIRECT3DBASETEXTURE9 pTexture);
    STDMETHOD(GetTextureStageState)(THIS_ DWORD Stage,D3D9Base::D3DTEXTURESTAGESTATETYPE Type,DWORD* pValue);
    STDMETHOD(SetTextureStageState)(THIS_ DWORD Stage,D3D9Base::D3DTEXTURESTAGESTATETYPE Type,DWORD Value);
    STDMETHOD(GetSamplerState)(THIS_ DWORD Sampler,D3D9Base::D3DSAMPLERSTATETYPE Type,DWORD* pValue);
    STDMETHOD(SetSamplerState)(THIS_ DWORD Sampler,D3D9Base::D3DSAMPLERSTATETYPE Type,DWORD Value);
    STDMETHOD(ValidateDevice)(THIS_ DWORD* pNumPasses);
    STDMETHOD(SetPaletteEntries)(THIS_ UINT PaletteNumber,CONST PALETTEENTRY* pEntries);
    STDMETHOD(GetPaletteEntries)(THIS_ UINT PaletteNumber,PALETTEENTRY* pEntries);
    STDMETHOD(SetCurrentTexturePalette)(THIS_ UINT PaletteNumber);
    STDMETHOD(GetCurrentTexturePalette)(THIS_ UINT *PaletteNumber);
    STDMETHOD(SetScissorRect)(THIS_ CONST RECT* pRect);
    STDMETHOD(GetScissorRect)(THIS_ RECT* pRect);
    STDMETHOD(SetSoftwareVertexProcessing)(THIS_ BOOL bSoftware);
    STDMETHOD_(BOOL, GetSoftwareVertexProcessing)(THIS);
    STDMETHOD(SetNPatchMode)(THIS_ float nSegments);
    STDMETHOD_(float, GetNPatchMode)(THIS);
    STDMETHOD(DrawPrimitive)(THIS_ D3D9Base::D3DPRIMITIVETYPE PrimitiveType,UINT StartVertex,UINT PrimitiveCount);
    STDMETHOD(DrawIndexedPrimitive)(THIS_ D3D9Base::D3DPRIMITIVETYPE,INT BaseVertexIndex,UINT MinVertexIndex,UINT NumVertices,UINT startIndex,UINT primCount);
    STDMETHOD(DrawPrimitiveUP)(THIS_ D3D9Base::D3DPRIMITIVETYPE PrimitiveType,UINT PrimitiveCount,CONST void* pVertexStreamZeroData,UINT VertexStreamZeroStride);
    STDMETHOD(DrawIndexedPrimitiveUP)(THIS_ D3D9Base::D3DPRIMITIVETYPE PrimitiveType,UINT MinVertexIndex,UINT NumVertices,UINT PrimitiveCount,CONST void* pIndexData,D3D9Base::D3DFORMAT IndexDataFormat,CONST void* pVertexStreamZeroData,UINT VertexStreamZeroStride);
    STDMETHOD(ProcessVertices)(THIS_ UINT SrcStartIndex,UINT DestIndex,UINT VertexCount, IDirect3DVertexBuffer9 *pDestBuffer,IDirect3DVertexDeclaration9* pVertexDecl,DWORD Flags);
    STDMETHOD(CreateVertexDeclaration)(THIS_ CONST D3D9Base::D3DVERTEXELEMENT9* pVertexElements,IDirect3DVertexDeclaration9** ppDecl);
    STDMETHOD(SetVertexDeclaration)(THIS_ IDirect3DVertexDeclaration9* pDecl);
    STDMETHOD(GetVertexDeclaration)(THIS_ IDirect3DVertexDeclaration9** ppDecl);
    STDMETHOD(SetFVF)(THIS_ DWORD FVF);
    STDMETHOD(GetFVF)(THIS_ DWORD* pFVF);
    STDMETHOD(CreateVertexShader)(THIS_ CONST DWORD* pFunction, IDirect3DVertexShader9 **ppShader);
    STDMETHOD(SetVertexShader)(THIS_ IDirect3DVertexShader9 *pShader);
    STDMETHOD(GetVertexShader)(THIS_ IDirect3DVertexShader9 **ppShader);
    STDMETHOD(SetVertexShaderConstantF)(THIS_ UINT StartRegister,CONST float* pConstantData,UINT Vector4fCount);
    STDMETHOD(GetVertexShaderConstantF)(THIS_ UINT StartRegister,float* pConstantData,UINT Vector4fCount);
    STDMETHOD(SetVertexShaderConstantI)(THIS_ UINT StartRegister,CONST int* pConstantData,UINT Vector4iCount);
    STDMETHOD(GetVertexShaderConstantI)(THIS_ UINT StartRegister,int* pConstantData,UINT Vector4iCount);
    STDMETHOD(SetVertexShaderConstantB)(THIS_ UINT StartRegister,CONST BOOL* pConstantData,UINT  BoolCount);
    STDMETHOD(GetVertexShaderConstantB)(THIS_ UINT StartRegister,BOOL* pConstantData,UINT BoolCount);
    STDMETHOD(SetStreamSource)(THIS_ UINT StreamNumber, IDirect3DVertexBuffer9 *pStreamData,UINT OffsetInBytes,UINT Stride);
    STDMETHOD(GetStreamSource)(THIS_ UINT StreamNumber, IDirect3DVertexBuffer9 **ppStreamData,UINT* pOffsetInBytes,UINT* pStride);
    STDMETHOD(SetStreamSourceFreq)(THIS_ UINT StreamNumber,UINT Setting);
    STDMETHOD(GetStreamSourceFreq)(THIS_ UINT StreamNumber,UINT* pSetting);
    STDMETHOD(SetIndices)(THIS_ IDirect3DIndexBuffer9 *pIndexData);
    STDMETHOD(GetIndices)(THIS_ IDirect3DIndexBuffer9 **ppIndexData);
    STDMETHOD(CreatePixelShader)(THIS_ CONST DWORD* pFunction, IDirect3DPixelShader9 **ppShader);
    STDMETHOD(SetPixelShader)(THIS_ IDirect3DPixelShader9 *pShader);
    STDMETHOD(GetPixelShader)(THIS_ IDirect3DPixelShader9 **ppShader);
    STDMETHOD(SetPixelShaderConstantF)(THIS_ UINT StartRegister,CONST float* pConstantData,UINT Vector4fCount);
    STDMETHOD(GetPixelShaderConstantF)(THIS_ UINT StartRegister,float* pConstantData,UINT Vector4fCount);
    STDMETHOD(SetPixelShaderConstantI)(THIS_ UINT StartRegister,CONST int* pConstantData,UINT Vector4iCount);
    STDMETHOD(GetPixelShaderConstantI)(THIS_ UINT StartRegister,int* pConstantData,UINT Vector4iCount);
    STDMETHOD(SetPixelShaderConstantB)(THIS_ UINT StartRegister,CONST BOOL* pConstantData,UINT  BoolCount);
    STDMETHOD(GetPixelShaderConstantB)(THIS_ UINT StartRegister,BOOL* pConstantData,UINT BoolCount);
    STDMETHOD(DrawRectPatch)(THIS_ UINT Handle,CONST float* pNumSegs,CONST D3D9Base::D3DRECTPATCH_INFO* pRectPatchInfo);
    STDMETHOD(DrawTriPatch)(THIS_ UINT Handle,CONST float* pNumSegs,CONST D3D9Base::D3DTRIPATCH_INFO* pTriPatchInfo);
    STDMETHOD(DeletePatch)(THIS_ UINT Handle);
    STDMETHOD(CreateQuery)(THIS_ D3D9Base::D3DQUERYTYPE Type, IDirect3DQuery9** ppQuery);

    /*** IDirect3DDevice9Ex methods ***/
    STDMETHOD(GetDisplayModeEx)(THIS_ UINT iSwapChain,D3D9Base::D3DDISPLAYMODEEX* pMode,D3D9Base::D3DDISPLAYROTATION* pRotation);
};

class IDirect3DSwapChain9 : public IDirect3DUnknown
{
public:
//    D3D9Wrapper::IDirect3DDevice9* m_pDevice;
    static ThreadSafePointerSet	   m_List;
	// Postponed creation parameters.
	bool pendingGetSwapChain;
	UINT _SwapChain;
	IDirect3DDevice9 *pendingDevice;

    IDirect3DSwapChain9(D3D9Base::LPDIRECT3DSWAPCHAIN9EX pSwapChain, D3D9Wrapper::IDirect3DDevice9* pDevice);

    /*** IDirect3DUnknown methods ***/
	STDMETHOD_(ULONG,AddRef)(THIS);
    STDMETHOD_(ULONG,Release)(THIS);

    static IDirect3DSwapChain9* GetSwapChain(D3D9Base::LPDIRECT3DSWAPCHAIN9EX pSwapChain, D3D9Wrapper::IDirect3DDevice9* pDevice);
    inline D3D9Base::LPDIRECT3DSWAPCHAIN9EX GetSwapChain9() { return (D3D9Base::LPDIRECT3DSWAPCHAIN9EX)m_pUnk; }

    /*** IDirect3DSwapChain9 methods ***/
    STDMETHOD(Present)(THIS_ CONST RECT* pSourceRect,CONST RECT* pDestRect,HWND hDestWindowOverride,CONST RGNDATA* pDirtyRegion,DWORD dwFlags);
    STDMETHOD(GetFrontBufferData)(THIS_ IDirect3DSurface9 *pDestSurface);
    STDMETHOD(GetBackBuffer)(THIS_ UINT iBackBuffer,D3D9Base::D3DBACKBUFFER_TYPE Type, IDirect3DSurface9 **ppBackBuffer);
    STDMETHOD(GetRasterStatus)(THIS_ D3D9Base::D3DRASTER_STATUS* pRasterStatus);
    STDMETHOD(GetDisplayMode)(THIS_ D3D9Base::D3DDISPLAYMODE* pMode);
    STDMETHOD(GetDevice)(THIS_ IDirect3DDevice9** ppDevice);
    STDMETHOD(GetPresentParameters)(THIS_ D3D9Base::D3DPRESENT_PARAMETERS* pPresentationParameters);

    /*** IDirect3DSwapChain9Ex methods ***/
    STDMETHOD(GetDisplayModeEx)(THIS_ D3D9Base::D3DDISPLAYMODEEX* pMode,D3D9Base::D3DDISPLAYROTATION* pRotation);
};

class IDirect3DSurface9 : public IDirect3DUnknown
{
public:
    static ThreadSafePointerSet m_List;
	// Delayed creation parameters from device.
	int magic;
	UINT _Width;
	UINT _Height;
	D3D9Base::D3DFORMAT _Format;
	D3D9Base::D3DMULTISAMPLE_TYPE _MultiSample;
	DWORD _MultisampleQuality;
	BOOL _Discard;
    D3D9Wrapper::IDirect3DDevice9 *_Device;
	// Delayed creation parameters from texture.
	UINT _Level;
	D3D9Wrapper::IDirect3DTexture9 *_Texture;
	bool pendingGetSurfaceLevel;

	IDirect3DSurface9(D3D9Base::LPDIRECT3DSURFACE9 pSurface);
    static IDirect3DSurface9* GetDirect3DSurface9(D3D9Base::LPDIRECT3DSURFACE9 pSurface);
	__forceinline D3D9Base::LPDIRECT3DSURFACE9 GetD3DSurface9() { return (D3D9Base::LPDIRECT3DSURFACE9)m_pUnk; }

    /*** IUnknown methods ***/
	STDMETHOD_(ULONG,AddRef)(THIS);
    STDMETHOD_(ULONG,Release)(THIS);
	
    /*** IDirect3DResource9 methods ***/
    STDMETHOD(GetDevice)(THIS_ IDirect3DDevice9** ppDevice);
    STDMETHOD(SetPrivateData)(THIS_ REFGUID refguid,CONST void* pData,DWORD SizeOfData,DWORD Flags);
    STDMETHOD(GetPrivateData)(THIS_ REFGUID refguid,void* pData,DWORD* pSizeOfData);
    STDMETHOD(FreePrivateData)(THIS_ REFGUID refguid);
    STDMETHOD_(DWORD, SetPriority)(THIS_ DWORD PriorityNew);
    STDMETHOD_(DWORD, GetPriority)(THIS);
    STDMETHOD_(void, PreLoad)(THIS);
    STDMETHOD_(D3D9Base::D3DRESOURCETYPE, GetType)(THIS);

    /*** IDirect3DSurface9 methods ***/
    STDMETHOD(GetContainer)(THIS_ REFIID riid,void** ppContainer);
    STDMETHOD(GetDesc)(THIS_ D3D9Base::D3DSURFACE_DESC *pDesc);
    STDMETHOD(LockRect)(THIS_ D3D9Base::D3DLOCKED_RECT* pLockedRect,CONST RECT* pRect,DWORD Flags);
    STDMETHOD(UnlockRect)(THIS);
    STDMETHOD(GetDC)(THIS_ HDC *phdc);
    STDMETHOD(ReleaseDC)(THIS_ HDC hdc);
};

class IDirect3DVertexDeclaration9 : public IDirect3DUnknown
{
public:
    static ThreadSafePointerSet m_List;
	int magic;
	// Delayed creation parameters.
	D3D9Base::D3DVERTEXELEMENT9 _VertexElements;
	IDirect3DDevice9 *pendingDevice;
	bool pendingCreateVertexDeclaration;

	IDirect3DVertexDeclaration9(D3D9Base::LPDIRECT3DVERTEXDECLARATION9 pVertexDeclaration);
    static IDirect3DVertexDeclaration9* GetDirect3DVertexDeclaration9(D3D9Base::LPDIRECT3DVERTEXDECLARATION9 pVertexDeclaration);
	__forceinline D3D9Base::LPDIRECT3DVERTEXDECLARATION9 GetD3DVertexDeclaration9() { return (D3D9Base::LPDIRECT3DVERTEXDECLARATION9) m_pUnk; }

    /*** IUnknown methods ***/
    STDMETHOD_(ULONG,AddRef)(THIS);
    STDMETHOD_(ULONG,Release)(THIS);

    /*** IDirect3DVertexDeclaration9 methods ***/
    STDMETHOD(GetDevice)(THIS_ IDirect3DDevice9** ppDevice);
    STDMETHOD(GetDeclaration)(THIS_ D3D9Base::D3DVERTEXELEMENT9* pElement,UINT* pNumElements);
};

class IDirect3DTexture9 : public IDirect3DUnknown
{
public:
    static ThreadSafePointerSet m_List;
	// Delayed creation parameters.
	int magic;
	UINT _Width;
	UINT _Height;
	UINT _Levels;
	DWORD _Usage;
	D3D9Base::D3DFORMAT _Format;
	D3D9Base::D3DPOOL _Pool;
	IDirect3DDevice9 *_Device;
	bool pendingCreateTexture;
	// Delayed lock/unlock.
	DWORD _Flags;
	UINT _Level;
	char *_Buffer;
	bool pendingLockUnlock;

	IDirect3DTexture9(D3D9Base::LPDIRECT3DTEXTURE9 pTexture);
    static IDirect3DTexture9* GetDirect3DVertexDeclaration9(D3D9Base::LPDIRECT3DTEXTURE9 pTexture);
	__forceinline D3D9Base::LPDIRECT3DTEXTURE9 GetD3DTexture9() { return (D3D9Base::LPDIRECT3DTEXTURE9) m_pUnk; }

    /*** IUnknown methods ***/
    STDMETHOD_(ULONG,AddRef)(THIS);
    STDMETHOD_(ULONG,Release)(THIS);

    /*** IDirect3DBaseTexture9 methods ***/
    STDMETHOD(GetDevice)(THIS_ IDirect3DDevice9** ppDevice);
    STDMETHOD(SetPrivateData)(THIS_ REFGUID refguid,CONST void* pData,DWORD SizeOfData,DWORD Flags);
    STDMETHOD(GetPrivateData)(THIS_ REFGUID refguid,void* pData,DWORD* pSizeOfData);
    STDMETHOD(FreePrivateData)(THIS_ REFGUID refguid);
    STDMETHOD_(DWORD, SetPriority)(THIS_ DWORD PriorityNew);
    STDMETHOD_(DWORD, GetPriority)(THIS);
    STDMETHOD_(void, PreLoad)(THIS);
    STDMETHOD_(D3D9Base::D3DRESOURCETYPE, GetType)(THIS);
    STDMETHOD_(DWORD, SetLOD)(THIS_ DWORD LODNew);
    STDMETHOD_(DWORD, GetLOD)(THIS);
    STDMETHOD_(DWORD, GetLevelCount)(THIS);
    STDMETHOD(SetAutoGenFilterType)(THIS_ D3D9Base::D3DTEXTUREFILTERTYPE FilterType);
    STDMETHOD_(D3D9Base::D3DTEXTUREFILTERTYPE, GetAutoGenFilterType)(THIS);
    STDMETHOD_(void, GenerateMipSubLevels)(THIS);
    STDMETHOD(GetLevelDesc)(THIS_ UINT Level,D3D9Base::D3DSURFACE_DESC *pDesc);
    STDMETHOD(GetSurfaceLevel)(THIS_ UINT Level,IDirect3DSurface9** ppSurfaceLevel);
    STDMETHOD(LockRect)(THIS_ UINT Level,D3D9Base::D3DLOCKED_RECT* pLockedRect,CONST RECT* pRect,DWORD Flags);
    STDMETHOD(UnlockRect)(THIS_ UINT Level);
    STDMETHOD(AddDirtyRect)(THIS_ CONST RECT* pDirtyRect);
};

class IDirect3DVertexBuffer9 : public IDirect3DUnknown
{
public:
    static ThreadSafePointerSet m_List;
	int magic;
	// Delayed creation parameters.
	UINT _Length;
	DWORD _Usage;
	DWORD _FVF;
	D3D9Base::D3DPOOL _Pool;
	IDirect3DDevice9 *_Device;
	bool pendingCreateVertexBuffer;
	// Delayed buffer lock/unlock.
	char *_Buffer;
	DWORD _Flags;
	bool pendingLockUnlock;

	IDirect3DVertexBuffer9(D3D9Base::LPDIRECT3DVERTEXBUFFER9 pVertexBuffer);
    static IDirect3DVertexBuffer9* GetDirect3DVertexBuffer9(D3D9Base::LPDIRECT3DVERTEXBUFFER9 pVertexBuffer);
	__forceinline D3D9Base::LPDIRECT3DVERTEXBUFFER9 GetD3DVertexBuffer9() { return (D3D9Base::LPDIRECT3DVERTEXBUFFER9)m_pUnk; }

    /*** IUnknown methods ***/
    STDMETHOD_(ULONG,AddRef)(THIS);
    STDMETHOD_(ULONG,Release)(THIS);

    /*** IDirect3DResource9 methods ***/
    STDMETHOD(GetDevice)(THIS_ IDirect3DDevice9** ppDevice);
    STDMETHOD(SetPrivateData)(THIS_ REFGUID refguid,CONST void* pData,DWORD SizeOfData,DWORD Flags);
    STDMETHOD(GetPrivateData)(THIS_ REFGUID refguid,void* pData,DWORD* pSizeOfData);
    STDMETHOD(FreePrivateData)(THIS_ REFGUID refguid);
    STDMETHOD_(DWORD, SetPriority)(THIS_ DWORD PriorityNew);
    STDMETHOD_(DWORD, GetPriority)(THIS);
    STDMETHOD_(void, PreLoad)(THIS);
    STDMETHOD_(D3D9Base::D3DRESOURCETYPE, GetType)(THIS);

    /*** IDirect3DResource9 methods ***/
    STDMETHOD(Lock)(THIS_ UINT OffsetToLock,UINT SizeToLock,void** ppbData,DWORD Flags);
    STDMETHOD(Unlock)(THIS);
    STDMETHOD(GetDesc)(THIS_ D3D9Base::D3DVERTEXBUFFER_DESC *pDesc);
};

class IDirect3DIndexBuffer9 : public IDirect3DUnknown
{
public:
    static ThreadSafePointerSet m_List;
	int magic;
	// Delayed creation parameters.
	UINT _Length;
	DWORD _Usage;
	D3D9Base::D3DFORMAT _Format;
	D3D9Base::D3DPOOL _Pool;
	IDirect3DDevice9 *_Device;
	bool pendingCreateIndexBuffer;
	// Delayed buffer lock/unlock.
	char *_Buffer;
	DWORD _Flags;
	bool pendingLockUnlock;

	IDirect3DIndexBuffer9(D3D9Base::LPDIRECT3DINDEXBUFFER9 pIndexBuffer);
    static IDirect3DIndexBuffer9* GetDirect3DIndexBuffer9(D3D9Base::LPDIRECT3DINDEXBUFFER9 pIndexBuffer);
	__forceinline D3D9Base::LPDIRECT3DINDEXBUFFER9 GetD3DIndexBuffer9() { return (D3D9Base::LPDIRECT3DINDEXBUFFER9)m_pUnk; }

    /*** IUnknown methods ***/
    STDMETHOD_(ULONG,AddRef)(THIS);
    STDMETHOD_(ULONG,Release)(THIS);

    /*** IDirect3DResource9 methods ***/
    STDMETHOD(GetDevice)(THIS_ IDirect3DDevice9** ppDevice);
    STDMETHOD(SetPrivateData)(THIS_ REFGUID refguid,CONST void* pData,DWORD SizeOfData,DWORD Flags);
    STDMETHOD(GetPrivateData)(THIS_ REFGUID refguid,void* pData,DWORD* pSizeOfData);
    STDMETHOD(FreePrivateData)(THIS_ REFGUID refguid);
    STDMETHOD_(DWORD, SetPriority)(THIS_ DWORD PriorityNew);
    STDMETHOD_(DWORD, GetPriority)(THIS);
    STDMETHOD_(void, PreLoad)(THIS);
    STDMETHOD_(D3D9Base::D3DRESOURCETYPE, GetType)(THIS);

    /*** IDirect3DResource9 methods ***/
    STDMETHOD(Lock)(THIS_ UINT OffsetToLock,UINT SizeToLock,void** ppbData,DWORD Flags);
    STDMETHOD(Unlock)(THIS);
    STDMETHOD(GetDesc)(THIS_ D3D9Base::D3DINDEXBUFFER_DESC *pDesc);
};

class IDirect3DQuery9 : public IDirect3DUnknown
{
public:
    static ThreadSafePointerSet m_List;
	int magic;

	IDirect3DQuery9(D3D9Base::LPDIRECT3DQUERY9 pQuery);
    static IDirect3DQuery9* GetDirect3DQuery9(D3D9Base::LPDIRECT3DQUERY9 pQuery);
	__forceinline D3D9Base::LPDIRECT3DQUERY9 GetD3DQuery9() { return (D3D9Base::LPDIRECT3DQUERY9)m_pUnk; }

    /*** IUnknown methods ***/
    STDMETHOD_(ULONG,AddRef)(THIS);
    STDMETHOD_(ULONG,Release)(THIS);

    /*** IDirect3DQuery9 methods ***/
    STDMETHOD(GetDevice)(THIS_ IDirect3DDevice9** ppDevice);
    STDMETHOD_(D3D9Base::D3DQUERYTYPE, GetType)(THIS);
    STDMETHOD_(DWORD, GetDataSize)(THIS);
    STDMETHOD(Issue)(THIS_ DWORD dwIssueFlags);
    STDMETHOD(GetData)(THIS_ void* pData,DWORD dwSize,DWORD dwGetDataFlags);    
};

class IDirect3DVertexShader9 : public IDirect3DUnknown
{
public:
    static ThreadSafePointerSet m_List;
	int magic;

	IDirect3DVertexShader9(D3D9Base::LPDIRECT3DVERTEXSHADER9 pVS);
    static IDirect3DVertexShader9* GetDirect3DVertexShader9(D3D9Base::LPDIRECT3DVERTEXSHADER9 pVS);
	__forceinline D3D9Base::LPDIRECT3DVERTEXSHADER9 GetD3DVertexShader9() { return (D3D9Base::LPDIRECT3DVERTEXSHADER9)m_pUnk; }

    /*** IUnknown methods ***/
    STDMETHOD_(ULONG,AddRef)(THIS);
    STDMETHOD_(ULONG,Release)(THIS);

    /*** IDirect3DVertexShader9 methods ***/
    STDMETHOD(GetDevice)(THIS_ IDirect3DDevice9** ppDevice);
    STDMETHOD(GetFunction)(THIS_ void*,UINT* pSizeOfData);    
};

class IDirect3DPixelShader9 : public IDirect3DUnknown
{
public:
    static ThreadSafePointerSet m_List;
	int magic;

	IDirect3DPixelShader9(D3D9Base::LPDIRECT3DPIXELSHADER9 pVS);
    static IDirect3DPixelShader9* GetDirect3DPixelShader9(D3D9Base::LPDIRECT3DPIXELSHADER9 pVS);
	__forceinline D3D9Base::LPDIRECT3DPIXELSHADER9 GetD3DPixelShader9() { return (D3D9Base::LPDIRECT3DPIXELSHADER9)m_pUnk; }

    /*** IUnknown methods ***/
    STDMETHOD_(ULONG,AddRef)(THIS);
    STDMETHOD_(ULONG,Release)(THIS);

    /*** IDirect3DPixelShader9 methods ***/
    STDMETHOD(GetDevice)(THIS_ IDirect3DDevice9** ppDevice);
    STDMETHOD(GetFunction)(THIS_ void*,UINT* pSizeOfData);
};
