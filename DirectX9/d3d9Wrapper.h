#pragma once
class IDirect3DUnknown;
class IDirect3D9;
class IDirect3DDevice9;
class IDirect3DSwapChain9;
class IDirect3DSurface9;
class IDirect3DVertexDeclaration9;
class IDirect3DTexture9;
class IDirect3DVertexBuffer9;
class IDirect3DIndexBuffer9;
class IDirect3DQuery9;
class IDirect3DShader9;
class IDirect3DVertexShader9;
class IDirect3DPixelShader9;
class IDirect3DCubeTexture9;
class IDirect3DVolumeTexture9;
class IDirect3DVolume9;
class IDirect3DBaseTexture9;
class IDirect3DStateBlock9;

typedef HRESULT (WINAPI *D3DCREATEEX)(UINT, ::LPDIRECT3D9EX *ppD3D);
typedef ::LPDIRECT3D9(WINAPI *D3DCREATE)(UINT);
typedef int (WINAPI *D3DPERF_BeginEvent)(::D3DCOLOR color, LPCWSTR name);
typedef int (WINAPI *D3DPERF_EndEvent)(void);
typedef DWORD (WINAPI *D3DPERF_GetStatus)(void);
typedef BOOL (WINAPI *D3DPERF_QueryRepeatFrame)(void);
typedef void (WINAPI *D3DPERF_SetMarker)(::D3DCOLOR color, LPCWSTR name);
typedef void (WINAPI *D3DPERF_SetOptions)(DWORD options);
typedef void (WINAPI *D3DPERF_SetRegion)(::D3DCOLOR color, LPCWSTR name);
typedef void (WINAPI *DebugSetLevel)(int a1, int a2);
//typedef void (WINAPI *DebugSetMute)(int a);
typedef void* (WINAPI *Direct3DShaderValidatorCreate9)(void);
typedef void (WINAPI *PSGPError)(void *D3DFE_PROCESSVERTICES, int PSGPERRORID, unsigned int a);
typedef void (WINAPI *PSGPSampleTexture)(void *D3DFE_PROCESSVERTICES, unsigned int a, float (* const b)[4], unsigned int c, float (* const d)[4]);

//DEFINE_GUID(IID_D3D9Wrapper_IDirect3DDevice9,
//	FIXME: Change entire DX9 project to use the correct GUID definition and comparison macros
//	- Removed the commented out GUID here as it was copy + pasted from DX11's HackerDevice, and didn't match any uses of IF_GUID
#define IF_GUID(riid,a,b,c,d,e,f,g) if ((riid.Data1==a)&&(riid.Data2==b)&&(riid.Data3==c)&&(riid.Data4[0]==d)&&(riid.Data4[1]==e)&&(riid.Data4[2]==f)&&(riid.Data4[3]==g))
class IDirect3DUnknown
{
public:
	IUnknown * m_pRealUnk;
	IUnknown * m_pUnk;
	ULONG       m_ulRef;
	int m_dm_recordedCalls;

	IDirect3DUnknown(IUnknown* pUnk)
	{
		m_pUnk = pUnk;
		m_pRealUnk = pUnk;
		m_ulRef = 1;
		m_dm_recordedCalls = 0;
	}

	/*** IUnknown methods ***/
	STDMETHOD(QueryInterface)(THIS_ REFIID riid, void** ppvObj) PURE;
	STDMETHOD_(ULONG, AddRef)(THIS) PURE;
	STDMETHOD_(ULONG, Release)(THIS) PURE;

	bool QueryInterface_DXGI_Callback(REFIID riid, void** ppvObj, HRESULT *result);
	D3D9Wrapper::IDirect3DUnknown* QueryInterface_Find_Wrapper(void* ppvObj);

	IUnknown* GetOrig()
	{
		return m_pUnk;
	}

	IUnknown* GetRealOrig()
	{
		return m_pRealUnk;
	}
};

class IDirect3D9 : public D3D9Wrapper::IDirect3DUnknown
{
public:
	void Delete();
	bool _ex;
	bool m_isRESZ;
	IDirect3D9(::LPDIRECT3D9 pD3D, bool ex);
	static IDirect3D9* GetDirect3D(::LPDIRECT3D9 pD3D, bool ex);
	__forceinline ::LPDIRECT3D9 GetDirect3D9() { return (::LPDIRECT3D9)m_pUnk; }
	__forceinline ::LPDIRECT3D9EX GetDirect3D9Ex() { return (::LPDIRECT3D9EX)m_pUnk; }

	void HookD9();
    static ThreadSafePointerSet	m_List;

    /*** IDirect3DUnknown methods ***/
	STDMETHOD(QueryInterface)(THIS_ REFIID riid, void** ppvObj) override;
	STDMETHOD_(ULONG,AddRef)(THIS) override;
    STDMETHOD_(ULONG,Release)(THIS) override;

    /*** IDirect3D9 methods ***/
    STDMETHOD(RegisterSoftwareDevice)(THIS_ void* pInitializeFunction);
    STDMETHOD_(UINT, GetAdapterCount)(THIS);
    STDMETHOD(GetAdapterIdentifier)(THIS_ UINT Adapter,DWORD Flags,::D3DADAPTER_IDENTIFIER9* pIdentifier);
    STDMETHOD_(UINT, GetAdapterModeCount)(THIS_ UINT Adapter,::D3DFORMAT Format);
    STDMETHOD(EnumAdapterModes)(THIS_ UINT Adapter,::D3DFORMAT Format,UINT Mode,::D3DDISPLAYMODE* pMode);
    STDMETHOD(GetAdapterDisplayMode)(THIS_ UINT Adapter,::D3DDISPLAYMODE* pMode);
    STDMETHOD(CheckDeviceType)(THIS_ UINT Adapter,::D3DDEVTYPE DevType,::D3DFORMAT AdapterFormat,::D3DFORMAT BackBufferFormat,BOOL bWindowed);
    STDMETHOD(CheckDeviceFormat)(THIS_ UINT Adapter,::D3DDEVTYPE DeviceType,::D3DFORMAT AdapterFormat,DWORD Usage,::D3DRESOURCETYPE RType,::D3DFORMAT CheckFormat);
    STDMETHOD(CheckDeviceMultiSampleType)(THIS_ UINT Adapter,::D3DDEVTYPE DeviceType,::D3DFORMAT SurfaceFormat,BOOL Windowed,::D3DMULTISAMPLE_TYPE MultiSampleType,DWORD* pQualityLevels);
    STDMETHOD(CheckDepthStencilMatch)(THIS_ UINT Adapter,::D3DDEVTYPE DeviceType,::D3DFORMAT AdapterFormat,::D3DFORMAT RenderTargetFormat,::D3DFORMAT DepthStencilFormat);
    STDMETHOD(CheckDeviceFormatConversion)(THIS_ UINT Adapter,::D3DDEVTYPE DeviceType,::D3DFORMAT SourceFormat,::D3DFORMAT TargetFormat);
    STDMETHOD(GetDeviceCaps)(THIS_ UINT Adapter,::D3DDEVTYPE DeviceType,::D3DCAPS9* pCaps);
    STDMETHOD_(HMONITOR, GetAdapterMonitor)(THIS_ UINT Adapter) ;
    STDMETHOD(CreateDevice)(THIS_ UINT Adapter,::D3DDEVTYPE DeviceType,HWND hFocusWindow,DWORD BehaviorFlags,::D3DPRESENT_PARAMETERS* pPresentationParameters,D3D9Wrapper::IDirect3DDevice9** ppReturnedDeviceInterface) ;

	/*** IDirect3D9Ex methods ***/
	STDMETHOD_(UINT, GetAdapterModeCountEx)(THIS_ UINT Adapter, CONST ::D3DDISPLAYMODEFILTER* pFilter);
	STDMETHOD(EnumAdapterModesEx)(THIS_ UINT Adapter, CONST ::D3DDISPLAYMODEFILTER* pFilter, UINT Mode, ::D3DDISPLAYMODEEX* pMode);
	STDMETHOD(GetAdapterDisplayModeEx)(THIS_ UINT Adapter, ::D3DDISPLAYMODEEX* pMode, ::D3DDISPLAYROTATION* pRotation);
	STDMETHOD(CreateDeviceEx)(THIS_ UINT Adapter, ::D3DDEVTYPE DeviceType, HWND hFocusWindow, DWORD BehaviorFlags, ::D3DPRESENT_PARAMETERS* pPresentationParameters, ::D3DDISPLAYMODEEX* pFullscreenDisplayMode, D3D9Wrapper::IDirect3DDevice9** ppReturnedDeviceInterface);
	STDMETHOD(GetAdapterLUID)(THIS_ UINT Adapter, LUID * pLUID);
};

// These are per-context so we shouldn't need locks
struct DepthSourceInfo
{
	UINT width, height;
	UINT drawcall_count, vertices_count;
	D3D9Wrapper::IDirect3DSurface9 *resolvedAA_source;
	D3D9Wrapper::IDirect3DSurface9 *resolvedAA_dest;
	::D3DCMPFUNC last_cmp_func;
	DepthSourceInfo() : width(0), height(0), drawcall_count(0), vertices_count(0), last_cmp_func(D3DCMP_LESSEQUAL), resolvedAA_source(NULL), resolvedAA_dest(NULL){};
	DepthSourceInfo(UINT width, UINT height) : width(width), height(height), drawcall_count(0), vertices_count(0), last_cmp_func(D3DCMP_LESSEQUAL), resolvedAA_source(NULL), resolvedAA_dest(NULL){};
};

struct LockedResourceInfo {
	::D3DLOCKED_BOX lockedBox;
	bool locked_writable;
	void *orig_pData;
	size_t size;

	LockedResourceInfo() :
		orig_pData(NULL),
		size(0),
		locked_writable(false)
	{}
};


struct FakeSwapChain {
	UINT swapChainIndex;
	vector<D3D9Wrapper::IDirect3DSurface9*> mFakeBackBuffers;
	vector<D3D9Wrapper::IDirect3DSurface9*> mDirectModeUpscalingBackBuffers;
	UINT mUpscalingWidth;
	UINT mUpscalingHeight;
	UINT mOrignalWidth;
	UINT mOrignalHeight;
};
struct DrawContext
{
	float oldSeparation;
	D3D9Wrapper::IDirect3DPixelShader9 *oldPixelShader;
	D3D9Wrapper::IDirect3DVertexShader9 *oldVertexShader;
	CommandList *post_commands[5];
	DrawCallInfo call_info;
	CachedStereoValues cachedStereoValues;

	DrawContext(DrawCall type, ::D3DPRIMITIVETYPE primitive_type,
		UINT PrimitiveCount,
		UINT StartVertex,
		INT BaseVertexIndex,
		UINT MinVertexIndex,
		UINT NumVertices,
		UINT StartIndex,
		const void *pVertexStreamZeroData,
		UINT VertexStreamZeroStride,
		const void *pIndexData,
		::D3DFORMAT IndexDataFormat,
		UINT Handle,
		const float *pNumSegs,
		const ::D3DRECTPATCH_INFO *pRectPatchInfo,
		const ::D3DTRIPATCH_INFO *pTriPatchInfo) :
		oldSeparation(FLT_MAX),
		oldVertexShader(NULL),
		oldPixelShader(NULL),
		cachedStereoValues(cachedStereoValues),
		call_info(type, primitive_type, PrimitiveCount, StartVertex, BaseVertexIndex, MinVertexIndex, NumVertices,
			StartIndex, pVertexStreamZeroData, VertexStreamZeroStride, pIndexData,
			IndexDataFormat,
			Handle, pNumSegs, pRectPatchInfo, pTriPatchInfo)
	{
		memset(post_commands, 0, sizeof(post_commands));
	}
};
enum RenderPosition
{
	Left = 1,
	Right = 2
};
struct activeVertexBuffer {
	D3D9Wrapper::IDirect3DVertexBuffer9 *m_vertexBuffer = NULL;
	UINT m_offsetInBytes;
	UINT m_pStride;
};
// Stereo Blit defines
#define NVSTEREO_IMAGE_SIGNATURE 0x4433564e //NV3D
typedef struct _Nv_Stereo_Image_Header
{
	unsigned int dwSignature;
	unsigned int dwWidth;
	unsigned int dwHeight;
	unsigned int dwBPP;
	unsigned int dwFlags;
} NVSTEREOIMAGEHEADER, *LPNVSTEREOIMAGEHEADER;
// ORed flags in the dwFlags fiels of the _Nv_Stereo_Image_Header structure above
#define SIH_SWAP_EYES 0x00000001
#define SIH_SWAP_EYES_R 0x00000002
#define SIH_SCALE_TO_FIT 0x00000002

#define D3DFMT_RESZ ((::D3DFORMAT)(MAKEFOURCC('R','E','S','Z')))
#define D3DFMT_INTZ ((::D3DFORMAT)(MAKEFOURCC('I','N','T','Z')))
#define RESZ_CODE 0x7fa05000
#define	NULL_CODE	((::D3DFORMAT)(MAKEFOURCC('N','U','L','L')))
class RecordRenderPass;
class RecordDeviceStateBlock;
class IDirect3DDevice9 : public D3D9Wrapper::IDirect3DUnknown
{
public:
	void Delete();
	ULONG migotoResourceCount;
	void UnbindResources();
	bool stereo_params_updated_this_frame;

	::D3DCMPFUNC current_zfunc;

	vector<::IDirect3DResource9*> nvapi_registered_resources;
	HRESULT NVAPIStretchRect(::IDirect3DResource9 *src, ::IDirect3DResource9 *dst, RECT *srcRect, RECT *dstRect);
	D3D9Wrapper::IDirect3DVertexShader9 *mCurrentVertexShaderHandle;
	D3D9Wrapper::IDirect3DPixelShader9 *mCurrentPixelShaderHandle;
	chrono::high_resolution_clock::duration update_stereo_params_interval;
	chrono::high_resolution_clock::time_point update_stereo_params_last_run;
	void UpdateStereoParams(bool forceUpdate, CachedStereoValues *cachedStereoValues = NULL);
	D3D9Wrapper::IDirect3DSurface9 *depthstencil_replacement;
	unordered_set<D3D9Wrapper::IDirect3DSurface9*> depth_sources;
	unsigned int drawCalls = 0, vertices = 0;
	void DetectDepthSource();
	bool CreateDepthStencilReplacement(D3D9Wrapper::IDirect3DSurface9 *depthStencil, bool resolvedAA);

	::IDirect3DTexture9 *cursor_mask_tex;
	::IDirect3DTexture9 *cursor_color_tex;

	void InitIniParams();

	NV_STEREO_ACTIVE_EYE nvapi_active_eye;
	bool SwitchDrawingSide();
	bool SetDrawingSide(D3D9Wrapper::RenderPosition side);
	vector<D3D9Wrapper::IDirect3DSurface9*> m_activeRenderTargets;
	bool m_bActiveViewportIsDefault;
	bool isViewportDefaultForMainRT(CONST ::D3DVIEWPORT9* pViewport);
	::D3DVIEWPORT9 m_LastViewportSet;
	D3D9Wrapper::IDirect3DSurface9* m_pActiveDepthStencil;
	unordered_map<DWORD, D3D9Wrapper::IDirect3DBaseTexture9*> m_activeStereoTextureStages;
	unordered_map<DWORD, D3D9Wrapper::IDirect3DBaseTexture9*> m_activeTextureStages;
	::IDirect3DSurface9 *DirectModeIntermediateRT;

	D3D9Wrapper::RenderPosition currentRenderingSide;
	D3D9Wrapper::RecordRenderPass *currentRenderPass;
	D3D9Wrapper::RecordDeviceStateBlock *recordDeviceStateBlock;

	D3D9Wrapper::IDirect3DIndexBuffer9 *m_activeIndexBuffer;
	unordered_map<DWORD, D3D9Wrapper::activeVertexBuffer> m_activeVertexBuffers;
	D3D9Wrapper::IDirect3DVertexDeclaration9 *m_pActiveVertexDeclaration;

	::D3DXMATRIX m_gameProjection;
	::D3DXMATRIX m_leftProjection;
	::D3DXMATRIX m_rightProjection;
	::D3DXMATRIX m_currentProjection;
	bool DirectModeGameProjectionIsSet;
	bool DirectModeProjectionNeedsUpdate;
	bool DirectModeUpdateTransforms();
	bool HackerDeviceShouldDuplicateSurface(D3D2DTEXTURE_DESC* pDesc);
	void HackerDeviceUnWrapTexture(D3D9Wrapper::IDirect3DBaseTexture9* pWrappedTexture, ::IDirect3DBaseTexture9** ppActualLeftTexture, ::IDirect3DBaseTexture9** ppActualRightTexture);
	void OnCreateOrRestore(::D3DPRESENT_PARAMETERS* pOrigParams, ::D3DPRESENT_PARAMETERS* pNewParams);
	bool get_sli_enabled();
	bool sli_enabled();
	bool retreivedInitial3DSettings;
	D3D9Wrapper::IDirect3DSurface9 *mFakeDepthSurface;
    static ThreadSafePointerSet	 m_List;
	// Creation parameters
	HANDLE _CreateThread;
	UINT _Adapter;
	::D3DDEVTYPE _DeviceType;
	HWND _hFocusWindow;
	DWORD _BehaviorFlags;
	::D3DDISPLAYMODEEX *_pFullscreenDisplayMode;
	::D3DPRESENT_PARAMETERS _pPresentationParameters;
	::D3DPRESENT_PARAMETERS _pOrigPresentationParameters;
	D3D9Wrapper::IDirect3DSurface9 *pendingCreateDepthStencilSurface;
	D3D9Wrapper::IDirect3DSurface9 *pendingCreateDepthStencilSurfaceEx;
	D3D9Wrapper::IDirect3DSurface9 *pendingSetDepthStencilSurface;
	bool _ex;
	::IDirect3DVertexBuffer9 *mClDrawVertexBuffer;
	::IDirect3DVertexDeclaration9 *mClDrawVertexDecl;

	void TrackAndDivertUnlock(D3D9Wrapper::IDirect3DResource9 *pResource, UINT Level = 0);
	template <typename Surface>
	void TrackAndDivertLock(HRESULT lock_hr, Surface *pResource,
		::D3DLOCKED_RECT *pLockedRect, DWORD MapFlags, UINT Level = 0);
	void TrackAndDivertLock(HRESULT lock_hr, D3D9Wrapper::IDirect3DVolumeTexture9 *pResource,
		 ::D3DLOCKED_BOX *pLockedRect, DWORD MapFlags, UINT Level);
	template<typename Buffer, typename Desc>
	void TrackAndDivertLock(HRESULT lock_hr, Buffer *pResource,
		UINT SizeToLock, void *ppbData, DWORD MapFlags);

	StereoHandle mStereoHandle;
	nv::stereo::ParamTextureManagerD3D9 mParamTextureManager;
	::IDirect3DTexture9 *mStereoTexture;

	char *ReplaceShader(UINT64 hash, const wchar_t *shaderType, const void *pShaderBytecode,
		SIZE_T BytecodeLength, SIZE_T &pCodeSize, string &foundShaderModel, FILETIME &timeStamp,
		wstring &headerLine, const char *overrideShaderModel);
	bool NeedOriginalShader(D3D9Wrapper::IDirect3DShader9 *wrapper);

	template <class ID3D9ShaderWrapper, class ID3D9Shader,
		HRESULT(__stdcall ::IDirect3DDevice9::*OrigCreateShader)(THIS_
			__in const DWORD *pFunction,
			__out_opt ID3D9Shader **ppShader)
	>void KeepOriginalShader(wchar_t *shaderType, ID3D9ShaderWrapper *pShader,
			const DWORD *pFunction);

	HRESULT CreateStereoTexture();
	HRESULT CreateStereoHandle();
	HRESULT InitStereoHandle();
	void CreatePinkHuntingResources();
	HRESULT SetGlobalNVSurfaceCreationMode();

	HRESULT ReleaseStereoTexture();
	void ReleasePinkHuntingResources();

	// Templates of nearly identical functions
	template <class ID3D9ShaderWrapper, class ID3D9Shader,
		ID3D9ShaderWrapper* (*GetDirect3DShader9)(ID3D9Shader *pS, D3D9Wrapper::IDirect3DDevice9 *hackerDevice),
		HRESULT(__stdcall ::IDirect3DDevice9::*OrigCreateShader)(THIS_
			__in const DWORD *pFunction,
			__out_opt ID3D9Shader **ppShader)
	>
		HRESULT CreateShader(THIS_
			__in  const DWORD *pFunction,
			__out_opt  ID3D9Shader **ppShader,
			wchar_t *shaderType,
			ID3D9ShaderWrapper **ppShaderWrapper);

	void Create3DMigotoResources();
	::IDirect3DDevice9* GetPassThroughOrigDevice();
	void HookDevice();

	void Bind3DMigotoResources();
	vector<UINT> resetVertexIniConstants;
	vector<UINT> resetPixelIniConstants;

    IDirect3DDevice9(::LPDIRECT3DDEVICE9 pDevice, D3D9Wrapper::IDirect3D9 *pD3D, bool ex);
    static IDirect3DDevice9* GetDirect3DDevice(::LPDIRECT3DDEVICE9 pDevice, D3D9Wrapper::IDirect3D9 *pD3D, bool ex);
	__forceinline ::LPDIRECT3DDEVICE9 GetD3D9Device() { return (::LPDIRECT3DDEVICE9) m_pUnk; }
	__forceinline ::LPDIRECT3DDEVICE9EX GetD3D9DeviceEx() { return (::LPDIRECT3DDEVICE9EX) m_pUnk; }

	// public to allow CommandList access
//	void FrameAnalysisLog(char *fmt, ...);
//	// An alias for the above function that we use to denote that omitting
//	// the newline was done intentionally. For now this is just for our
//	// reference, but later we might actually make the default function
//	// insert a newline:
//#define FrameAnalysisLogNoNL FrameAnalysisLog


    /*** IDirect3DUnknown methods ***/
	/*** IUnknown methods ***/
	STDMETHOD(QueryInterface)(THIS_ REFIID riid, void** ppvObj) override;
	STDMETHOD_(ULONG,AddRef)(THIS) override;
    STDMETHOD_(ULONG,Release)(THIS) override;

    /*** IDirect3DDevice9 methods ***/
    STDMETHOD(TestCooperativeLevel)(THIS);
    STDMETHOD_(UINT, GetAvailableTextureMem)(THIS);
    STDMETHOD(EvictManagedResources)(THIS);
    STDMETHOD(GetDirect3D)(THIS_ D3D9Wrapper::IDirect3D9** ppD3D9);
    STDMETHOD(GetDeviceCaps)(THIS_ ::D3DCAPS9* pCaps);
    STDMETHOD(GetDisplayMode)(THIS_ UINT iSwapChain,::D3DDISPLAYMODE* pMode);
    STDMETHOD(GetCreationParameters)(THIS_ ::D3DDEVICE_CREATION_PARAMETERS *pParameters);
    STDMETHOD(SetCursorProperties)(THIS_ UINT XHotSpot,UINT YHotSpot,D3D9Wrapper::IDirect3DSurface9 *pCursorBitmap);
    STDMETHOD_(void, SetCursorPosition)(THIS_ int X,int Y,DWORD Flags);
    STDMETHOD_(BOOL, ShowCursor)(THIS_ BOOL bShow);
    STDMETHOD(CreateAdditionalSwapChain)(THIS_ ::D3DPRESENT_PARAMETERS* pPresentationParameters, D3D9Wrapper::IDirect3DSwapChain9** pSwapChain);
    STDMETHOD(GetSwapChain)(THIS_ UINT iSwapChain, D3D9Wrapper::IDirect3DSwapChain9** pSwapChain);
    STDMETHOD_(UINT, GetNumberOfSwapChains)(THIS);
    STDMETHOD(Reset)(THIS_ ::D3DPRESENT_PARAMETERS* pPresentationParameters);
    STDMETHOD(Present)(THIS_ CONST RECT* pSourceRect,CONST RECT* pDestRect,HWND hDestWindowOverride,CONST RGNDATA* pDirtyRegion);
    STDMETHOD(GetBackBuffer)(THIS_ UINT iSwapChain,UINT iBackBuffer,::D3DBACKBUFFER_TYPE Type, D3D9Wrapper::IDirect3DSurface9 **ppBackBuffer);
    STDMETHOD(GetRasterStatus)(THIS_ UINT iSwapChain,::D3DRASTER_STATUS* pRasterStatus);
    STDMETHOD(SetDialogBoxMode)(THIS_ BOOL bEnableDialogs);
    STDMETHOD_(void, SetGammaRamp)(THIS_ UINT iSwapChain,DWORD Flags,CONST ::D3DGAMMARAMP* pRamp);
    STDMETHOD_(void, GetGammaRamp)(THIS_ UINT iSwapChain,::D3DGAMMARAMP* pRamp);
    STDMETHOD(CreateTexture)(THIS_ UINT Width,UINT Height,UINT Levels,DWORD Usage,::D3DFORMAT Format,::D3DPOOL Pool, D3D9Wrapper::IDirect3DTexture9** ppTexture,HANDLE* pSharedHandle);
    STDMETHOD(CreateVolumeTexture)(THIS_ UINT Width,UINT Height,UINT Depth,UINT Levels,DWORD Usage,::D3DFORMAT Format,::D3DPOOL Pool, D3D9Wrapper::IDirect3DVolumeTexture9** ppVolumeTexture,HANDLE* pSharedHandle);
    STDMETHOD(CreateCubeTexture)(THIS_ UINT EdgeLength,UINT Levels,DWORD Usage,::D3DFORMAT Format,::D3DPOOL Pool, D3D9Wrapper::IDirect3DCubeTexture9** ppCubeTexture,HANDLE* pSharedHandle);
    STDMETHOD(CreateVertexBuffer)(THIS_ UINT Length,DWORD Usage,DWORD FVF,::D3DPOOL Pool, D3D9Wrapper::IDirect3DVertexBuffer9 **ppVertexBuffer,HANDLE* pSharedHandle);
    STDMETHOD(CreateIndexBuffer)(THIS_ UINT Length,DWORD Usage,::D3DFORMAT Format,::D3DPOOL Pool, D3D9Wrapper::IDirect3DIndexBuffer9 **ppIndexBuffer,HANDLE* pSharedHandle);
    STDMETHOD(CreateRenderTarget)(THIS_ UINT Width,UINT Height,::D3DFORMAT Format,::D3DMULTISAMPLE_TYPE MultiSample,DWORD MultisampleQuality,BOOL Lockable, D3D9Wrapper::IDirect3DSurface9 **ppSurface,HANDLE* pSharedHandle);
    STDMETHOD(CreateDepthStencilSurface)(THIS_ UINT Width,UINT Height,::D3DFORMAT Format,::D3DMULTISAMPLE_TYPE MultiSample,DWORD MultisampleQuality,BOOL Discard,D3D9Wrapper::IDirect3DSurface9 **ppSurface,HANDLE* pSharedHandle);
    STDMETHOD(UpdateSurface)(THIS_ D3D9Wrapper::IDirect3DSurface9 *pSourceSurface,CONST RECT* pSourceRect,D3D9Wrapper::IDirect3DSurface9 *pDestinationSurface,CONST POINT* pDestPoint);
    STDMETHOD(UpdateTexture)(THIS_ D3D9Wrapper::IDirect3DBaseTexture9 *pSourceTexture, D3D9Wrapper::IDirect3DBaseTexture9 *pDestinationTexture);
    STDMETHOD(GetRenderTargetData)(THIS_ D3D9Wrapper::IDirect3DSurface9 *pRenderTarget, D3D9Wrapper::IDirect3DSurface9 *pDestSurface);
    STDMETHOD(GetFrontBufferData)(THIS_ UINT iSwapChain, D3D9Wrapper::IDirect3DSurface9 *pDestSurface);
    STDMETHOD(StretchRect)(THIS_ D3D9Wrapper::IDirect3DSurface9 *pSourceSurface,CONST RECT* pSourceRect, D3D9Wrapper::IDirect3DSurface9 *pDestSurface,CONST RECT* pDestRect,::D3DTEXTUREFILTERTYPE Filter);
    STDMETHOD(ColorFill)(THIS_ D3D9Wrapper::IDirect3DSurface9 *pSurface,CONST RECT* pRect,::D3DCOLOR color);
    STDMETHOD(CreateOffscreenPlainSurface)(THIS_ UINT Width,UINT Height,::D3DFORMAT Format,::D3DPOOL Pool, D3D9Wrapper::IDirect3DSurface9 **ppSurface,HANDLE* pSharedHandle);
    STDMETHOD(SetRenderTarget)(THIS_ DWORD RenderTargetIndex, D3D9Wrapper::IDirect3DSurface9 *pRenderTarget);
    STDMETHOD(GetRenderTarget)(THIS_ DWORD RenderTargetIndex, D3D9Wrapper::IDirect3DSurface9 **ppRenderTarget);
    STDMETHOD(SetDepthStencilSurface)(THIS_ D3D9Wrapper::IDirect3DSurface9 *pNewZStencil);
    STDMETHOD(GetDepthStencilSurface)(THIS_ D3D9Wrapper::IDirect3DSurface9 **ppZStencilSurface);
    STDMETHOD(BeginScene)(THIS);
    STDMETHOD(EndScene)(THIS);
    STDMETHOD(Clear)(THIS_ DWORD Count,CONST ::D3DRECT* pRects,DWORD Flags,::D3DCOLOR Color,float Z,DWORD Stencil);
    STDMETHOD(SetTransform)(THIS_ ::D3DTRANSFORMSTATETYPE State,CONST ::D3DMATRIX* pMatrix);
    STDMETHOD(GetTransform)(THIS_ ::D3DTRANSFORMSTATETYPE State,::D3DMATRIX* pMatrix);
    STDMETHOD(MultiplyTransform)(THIS_ ::D3DTRANSFORMSTATETYPE a,CONST ::D3DMATRIX *b);
    STDMETHOD(SetViewport)(THIS_ CONST ::D3DVIEWPORT9* pViewport);
    STDMETHOD(GetViewport)(THIS_ ::D3DVIEWPORT9* pViewport);
    STDMETHOD(SetMaterial)(THIS_ CONST ::D3DMATERIAL9* pMaterial);
    STDMETHOD(GetMaterial)(THIS_ ::D3DMATERIAL9* pMaterial);
    STDMETHOD(SetLight)(THIS_ DWORD Index,CONST ::D3DLIGHT9 *Light);
    STDMETHOD(GetLight)(THIS_ DWORD Index,::D3DLIGHT9*);
    STDMETHOD(LightEnable)(THIS_ DWORD Index,BOOL Enable);
    STDMETHOD(GetLightEnable)(THIS_ DWORD Index,BOOL* pEnable);
    STDMETHOD(SetClipPlane)(THIS_ DWORD Index,CONST float* pPlane);
    STDMETHOD(GetClipPlane)(THIS_ DWORD Index,float* pPlane);
    STDMETHOD(SetRenderState)(THIS_ ::D3DRENDERSTATETYPE State,DWORD Value);
    STDMETHOD(GetRenderState)(THIS_ ::D3DRENDERSTATETYPE State,DWORD* pValue);
    STDMETHOD(CreateStateBlock)(THIS_ ::D3DSTATEBLOCKTYPE Type, D3D9Wrapper::IDirect3DStateBlock9** ppSB);
    STDMETHOD(BeginStateBlock)(THIS);
    STDMETHOD(EndStateBlock)(THIS_ D3D9Wrapper::IDirect3DStateBlock9** ppSB);
    STDMETHOD(SetClipStatus)(THIS_ CONST ::D3DCLIPSTATUS9* pClipStatus);
    STDMETHOD(GetClipStatus)(THIS_ ::D3DCLIPSTATUS9* pClipStatus);
    STDMETHOD(GetTexture)(THIS_ DWORD Stage,D3D9Wrapper::IDirect3DBaseTexture9 **ppTexture);
    STDMETHOD(SetTexture)(THIS_ DWORD Stage, D3D9Wrapper::IDirect3DBaseTexture9 *pTexture);
    STDMETHOD(GetTextureStageState)(THIS_ DWORD Stage,::D3DTEXTURESTAGESTATETYPE Type,DWORD* pValue);
    STDMETHOD(SetTextureStageState)(THIS_ DWORD Stage,::D3DTEXTURESTAGESTATETYPE Type,DWORD Value);
    STDMETHOD(GetSamplerState)(THIS_ DWORD Sampler,::D3DSAMPLERSTATETYPE Type,DWORD* pValue);
    STDMETHOD(SetSamplerState)(THIS_ DWORD Sampler,::D3DSAMPLERSTATETYPE Type,DWORD Value);
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
    STDMETHOD(DrawPrimitive)(THIS_ ::D3DPRIMITIVETYPE PrimitiveType,UINT StartVertex,UINT PrimitiveCount);
    STDMETHOD(DrawIndexedPrimitive)(THIS_ ::D3DPRIMITIVETYPE,INT BaseVertexIndex,UINT MinVertexIndex,UINT NumVertices,UINT startIndex,UINT primCount);
    STDMETHOD(DrawPrimitiveUP)(THIS_ ::D3DPRIMITIVETYPE PrimitiveType,UINT PrimitiveCount,CONST void* pVertexStreamZeroData,UINT VertexStreamZeroStride);
    STDMETHOD(DrawIndexedPrimitiveUP)(THIS_ ::D3DPRIMITIVETYPE PrimitiveType,UINT MinVertexIndex,UINT NumVertices,UINT PrimitiveCount,CONST void* pIndexData,::D3DFORMAT IndexDataFormat,CONST void* pVertexStreamZeroData,UINT VertexStreamZeroStride);
    STDMETHOD(ProcessVertices)(THIS_ UINT SrcStartIndex,UINT DestIndex,UINT VertexCount, D3D9Wrapper::IDirect3DVertexBuffer9 *pDestBuffer, D3D9Wrapper::IDirect3DVertexDeclaration9* pVertexDecl,DWORD Flags);
    STDMETHOD(CreateVertexDeclaration)(THIS_ CONST ::D3DVERTEXELEMENT9* pVertexElements, D3D9Wrapper::IDirect3DVertexDeclaration9** ppDecl);
    STDMETHOD(SetVertexDeclaration)(THIS_ D3D9Wrapper::IDirect3DVertexDeclaration9* pDecl);
    STDMETHOD(GetVertexDeclaration)(THIS_ D3D9Wrapper::IDirect3DVertexDeclaration9** ppDecl);
    STDMETHOD(SetFVF)(THIS_ DWORD FVF);
    STDMETHOD(GetFVF)(THIS_ DWORD* pFVF);
    STDMETHOD(CreateVertexShader)(THIS_ CONST DWORD* pFunction, D3D9Wrapper::IDirect3DVertexShader9 **ppShader);
    STDMETHOD(SetVertexShader)(THIS_ D3D9Wrapper::IDirect3DVertexShader9 *pShader);
    STDMETHOD(GetVertexShader)(THIS_ D3D9Wrapper::IDirect3DVertexShader9 **ppShader);
    STDMETHOD(SetVertexShaderConstantF)(THIS_ UINT StartRegister,CONST float* pConstantData,UINT Vector4fCount);
    STDMETHOD(GetVertexShaderConstantF)(THIS_ UINT StartRegister,float* pConstantData,UINT Vector4fCount);
    STDMETHOD(SetVertexShaderConstantI)(THIS_ UINT StartRegister,CONST int* pConstantData,UINT Vector4iCount);
    STDMETHOD(GetVertexShaderConstantI)(THIS_ UINT StartRegister,int* pConstantData,UINT Vector4iCount);
    STDMETHOD(SetVertexShaderConstantB)(THIS_ UINT StartRegister,CONST BOOL* pConstantData,UINT  BoolCount);
    STDMETHOD(GetVertexShaderConstantB)(THIS_ UINT StartRegister,BOOL* pConstantData,UINT BoolCount);
    STDMETHOD(SetStreamSource)(THIS_ UINT StreamNumber, D3D9Wrapper::IDirect3DVertexBuffer9 *pStreamData,UINT OffsetInBytes,UINT Stride);
    STDMETHOD(GetStreamSource)(THIS_ UINT StreamNumber, D3D9Wrapper::IDirect3DVertexBuffer9 **ppStreamData,UINT* pOffsetInBytes,UINT* pStride);
    STDMETHOD(SetStreamSourceFreq)(THIS_ UINT StreamNumber,UINT Setting);
    STDMETHOD(GetStreamSourceFreq)(THIS_ UINT StreamNumber,UINT* pSetting);
    STDMETHOD(SetIndices)(THIS_ D3D9Wrapper::IDirect3DIndexBuffer9 *pIndexData);
    STDMETHOD(GetIndices)(THIS_ D3D9Wrapper::IDirect3DIndexBuffer9 **ppIndexData);
    STDMETHOD(CreatePixelShader)(THIS_ CONST DWORD* pFunction, D3D9Wrapper::IDirect3DPixelShader9 **ppShader);
    STDMETHOD(SetPixelShader)(THIS_ D3D9Wrapper::IDirect3DPixelShader9 *pShader);
    STDMETHOD(GetPixelShader)(THIS_ D3D9Wrapper::IDirect3DPixelShader9 **ppShader);
    STDMETHOD(SetPixelShaderConstantF)(THIS_ UINT StartRegister,CONST float* pConstantData,UINT Vector4fCount);
    STDMETHOD(GetPixelShaderConstantF)(THIS_ UINT StartRegister,float* pConstantData,UINT Vector4fCount);
    STDMETHOD(SetPixelShaderConstantI)(THIS_ UINT StartRegister,CONST int* pConstantData,UINT Vector4iCount);
    STDMETHOD(GetPixelShaderConstantI)(THIS_ UINT StartRegister,int* pConstantData,UINT Vector4iCount);
    STDMETHOD(SetPixelShaderConstantB)(THIS_ UINT StartRegister,CONST BOOL* pConstantData,UINT  BoolCount);
    STDMETHOD(GetPixelShaderConstantB)(THIS_ UINT StartRegister,BOOL* pConstantData,UINT BoolCount);
    STDMETHOD(DrawRectPatch)(THIS_ UINT Handle,CONST float* pNumSegs,CONST ::D3DRECTPATCH_INFO* pRectPatchInfo);
    STDMETHOD(DrawTriPatch)(THIS_ UINT Handle,CONST float* pNumSegs,CONST ::D3DTRIPATCH_INFO* pTriPatchInfo);
    STDMETHOD(DeletePatch)(THIS_ UINT Handle);
    STDMETHOD(CreateQuery)(THIS_ ::D3DQUERYTYPE Type, ::IDirect3DQuery9** ppQuery);

	/*** IDirect3DDevice9Ex methods ***/
	STDMETHOD(GetDisplayModeEx)(THIS_ UINT iSwapChain, ::D3DDISPLAYMODEEX* pMode, ::D3DDISPLAYROTATION* pRotation);

	STDMETHOD(ResetEx)(THIS_ ::D3DPRESENT_PARAMETERS* pPresentationParameters, ::D3DDISPLAYMODEEX *pFullscreenDisplayMode);

	STDMETHOD(CheckDeviceState)(THIS_ HWND hWindow);
	STDMETHOD(CheckResourceResidency)(THIS_ ::IDirect3DResource9 **pResourceArray, UINT32 NumResources);
	STDMETHOD(ComposeRects)(THIS_ D3D9Wrapper::IDirect3DSurface9 *pSource, D3D9Wrapper::IDirect3DSurface9 *pDestination, D3D9Wrapper::IDirect3DVertexBuffer9 *pSrcRectDescriptors, UINT NumRects, D3D9Wrapper::IDirect3DVertexBuffer9 *pDstRectDescriptors, ::D3DCOMPOSERECTSOP Operation, INT XOffset, INT YOffset);
	STDMETHOD(CreateDepthStencilSurfaceEx)(THIS_ UINT Width, UINT Height, ::D3DFORMAT Format, ::D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Discard, D3D9Wrapper::IDirect3DSurface9 **ppSurface, HANDLE *pSharedHandle, DWORD Usage);
	STDMETHOD(CreateOffscreenPlainSurfaceEx)(THIS_ UINT Width, UINT Height, ::D3DFORMAT Format, ::D3DPOOL Pool, D3D9Wrapper::IDirect3DSurface9 **ppSurface, HANDLE *pSharedHandle, DWORD Usage);
	STDMETHOD(CreateRenderTargetEx)(THIS_ UINT Width, UINT Height, ::D3DFORMAT Format, ::D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Lockable, D3D9Wrapper::IDirect3DSurface9 **ppSurface, HANDLE *pSharedHandle, DWORD Usage);
	STDMETHOD(GetGPUThreadPriority)(THIS_ INT *pPriority);
	STDMETHOD(GetMaximumFrameLatency)(THIS_ UINT *pMaxLatency);
	STDMETHOD(PresentEx)(THIS_ RECT *pSourceRect, const RECT *pDestRect, HWND hDestWindowOverride, const RGNDATA *pDirtyRegion, DWORD dwFlags);
	STDMETHOD(SetConvolutionMonoKernel)(THIS_ UINT Width, UINT  Height, float *RowWeights, float *ColumnWeights);
	STDMETHOD(SetGPUThreadPriority)(THIS_ INT pPriority);
	STDMETHOD(SetMaximumFrameLatency)(THIS_ UINT pMaxLatency);
	STDMETHOD(WaitForVBlank)(THIS_ UINT SwapChainIndex);

// public to allow CommandList access
	virtual void FrameAnalysisLog(char *fmt, ...) {};
	virtual void FrameAnalysisTrigger(FrameAnalysisOptions new_options) {};
	virtual void FrameAnalysisDump(::IDirect3DResource9 *resource, FrameAnalysisOptions options,
		const wchar_t *target, ::D3DFORMAT format, UINT stride, UINT offset, ResourceHandleInfo *info) {};
	virtual void DumpCB(char shader_type, wchar_t constant_type, UINT start_slot, UINT num_slots) {};
	virtual void FrameAnalysisLogResourceHash(::IDirect3DResource9 *resource) {};
	virtual void FrameAnalysisAfterUnlock(::IDirect3DResource9 *resource) {};
	virtual void FrameAnalysisLogResource(int slot, char *slot_name, ::IDirect3DResource9 *resource) {};
	virtual void FrameAnalysisLogQuery(::IDirect3DQuery9 *query) {};
	virtual void FrameAnalysisLogData(void *buf, UINT size) {};

	Overlay* getOverlay();
	HRESULT createOverlay();
	D3D9Wrapper::IDirect3D9 *m_pD3D;
	D3D9Wrapper::IDirect3DSwapChain9 *deviceSwapChain;
	vector<D3D9Wrapper::IDirect3DSwapChain9*> mWrappedSwapChains;
	vector<D3D9Wrapper::FakeSwapChain> mFakeSwapChains;
	private:
		void ReleaseDeviceResources();
		bool sli;
		Overlay * mOverlay;
		void RecordGraphicsShaderStats();
		void RecordVertexShaderResourceUsage(ShaderInfoData *shader_info);
		void RecordPixelShaderResourceUsage(ShaderInfoData *shader_info);
		void RecordResourceStats(D3D9Wrapper::IDirect3DResource9 *resource, std::set<uint32_t> *resource_info);
		void RecordPeerShaders(set<UINT64> *PeerShaders, D3D9Wrapper::IDirect3DShader9 *this_shader);
		void SwitchPSShader(::IDirect3DPixelShader9 *shader);
		void SwitchVSShader(::IDirect3DVertexShader9 *shader);
		void DeferredShaderReplacementBeforeDraw();
		template<typename ID3D9Shader, HRESULT (__stdcall ::IDirect3DDevice9::*_CreateShader)(const DWORD*, ID3D9Shader**)>
		void DeferredShaderReplacement(D3D9Wrapper::IDirect3DShader9 *shader, wchar_t *shader_type);
		void ProcessShaderOverride(ShaderOverride *shaderOverride, bool isPixelShader, D3D9Wrapper::DrawContext *data);
		void RecordRenderTargetInfo(D3D9Wrapper::IDirect3DSurface9 *target, DWORD RenderTargetIndex);
		void RecordDepthStencil(D3D9Wrapper::IDirect3DSurface9 *target);
		template <class ID3D9ShaderWrapper, class ID3D9Shader,
			HRESULT(__stdcall ::IDirect3DDevice9::*OrigSetShader)(THIS_
				ID3D9Shader *pShader)
		>
			HRESULT SetShader(ID3D9ShaderWrapper *pShader,
				set<UINT64> *visitedShaders,
				UINT64 selectedShader,
				ID3D9ShaderWrapper **currentShaderHandle);
		HRESULT _SetTexture(DWORD Sampler, ::IDirect3DBaseTexture9 *pTexture);

		// These private methods are utility routines for HackerDevice.
		void BeforeDraw(D3D9Wrapper::DrawContext &data);
		void AfterDraw(D3D9Wrapper::DrawContext &data);

		protected:
			// These are per-context, moved from globals.h:
			uint32_t mCurrentIndexBuffer;
			uint32_t mCurrentVertexBuffers[D3D9_VERTEX_INPUT_RESOURCE_SLOT_COUNT];
			std::vector<D3D9Wrapper::IDirect3DSurface9*> mCurrentRenderTargets;
			FILE * frame_analysis_log;
			FrameAnalysisOptions analyse_options;
};

class IDirect3DSwapChain9 : public D3D9Wrapper::IDirect3DUnknown
{
private:
	D3D9Wrapper::IDirect3DDevice9 *hackerDevice;
public:
	void Delete();
	ULONG shared_ref_count;
	bool zero_d3d_ref_count;
	bool bound;
	void Bound() {
		bound = true;
	};
	void Unbound() {
		bound = false;
		if (shared_ref_count == 0)
			Delete();
	};
	::D3DPRESENT_PARAMETERS origPresentationParameters;
	D3D9Wrapper::FakeSwapChain *mFakeSwapChain;
    static ThreadSafePointerSet m_List;
	// Postponed creation parameters.
	bool pendingGetSwapChain;
	UINT _SwapChain;
	D3D9Wrapper::IDirect3DDevice9 *pendingDevice;

	vector<D3D9Wrapper::IDirect3DSurface9*> m_backBuffers;
	void HookSwapChain();
    IDirect3DSwapChain9(::LPDIRECT3DSWAPCHAIN9 pSwapChain, D3D9Wrapper::IDirect3DDevice9* pDevice);

    /*** IDirect3DUnknown methods ***/
	STDMETHOD(QueryInterface)(THIS_ REFIID riid, void** ppvObj) override;
	STDMETHOD_(ULONG,AddRef)(THIS) override;
    STDMETHOD_(ULONG,Release)(THIS) override;

    static IDirect3DSwapChain9* GetSwapChain(::LPDIRECT3DSWAPCHAIN9 pSwapChain, D3D9Wrapper::IDirect3DDevice9* pDevice);
	inline ::LPDIRECT3DSWAPCHAIN9 GetSwapChain9() { return (::LPDIRECT3DSWAPCHAIN9)m_pUnk; }
	inline ::LPDIRECT3DSWAPCHAIN9EX GetSwapChain9Ex() { return (::LPDIRECT3DSWAPCHAIN9EX)m_pUnk; }

    /*** IDirect3DSwapChain9 methods ***/
    STDMETHOD(Present)(THIS_ CONST RECT* pSourceRect,CONST RECT* pDestRect,HWND hDestWindowOverride,CONST RGNDATA* pDirtyRegion,DWORD dwFlags);
    STDMETHOD(GetFrontBufferData)(THIS_ D3D9Wrapper::IDirect3DSurface9 *pDestSurface);
    STDMETHOD(GetBackBuffer)(THIS_ UINT iBackBuffer,::D3DBACKBUFFER_TYPE Type, D3D9Wrapper::IDirect3DSurface9 **ppBackBuffer);
    STDMETHOD(GetRasterStatus)(THIS_ ::D3DRASTER_STATUS* pRasterStatus);
    STDMETHOD(GetDisplayMode)(THIS_ ::D3DDISPLAYMODE* pMode);
    STDMETHOD(GetDevice)(THIS_ D3D9Wrapper::IDirect3DDevice9** ppDevice);
    STDMETHOD(GetPresentParameters)(THIS_ ::D3DPRESENT_PARAMETERS* pPresentationParameters);

	/*** IDirect3DSwapChain9Ex methods ***/
	STDMETHOD(GetDisplayModeEx)(THIS_ ::D3DDISPLAYMODEEX* pMode, ::D3DDISPLAYROTATION* pRotation);
	STDMETHOD(GetLastPresentCount)(THIS_ UINT *pLastPresentCount);
	STDMETHOD(GetPresentStats)(::D3DPRESENTSTATS *pPresentationStatistics);

};

class IDirect3DResource9 : public D3D9Wrapper::IDirect3DUnknown
{
protected:
	D3D9Wrapper::IDirect3DDevice9 *hackerDevice;
public:
	IDirect3DResource9(::LPDIRECT3DRESOURCE9 pResource, D3D9Wrapper::IDirect3DDevice9 *hackerDevice);
	__forceinline ::LPDIRECT3DRESOURCE9 GetD3DResource9() { return (::LPDIRECT3DRESOURCE9)m_pUnk; }
	ResourceHandleInfo resourceHandleInfo;
	LockedResourceInfo lockedResourceInfo;
	void Delete();
};

struct D3DLockedRect {
	::D3DLOCKED_RECT lockedRect;
	RECT rect;
	DWORD flags;
};

enum SurfaceContainerOwnerType {
	Device,
	SwapChain,
	Texture,
	CubeTexture,
	Unknown
};

class IDirect3DSurface9 : public D3D9Wrapper::IDirect3DResource9
{
private:
	::IDirect3DSurface9* m_pDirectSurfaceRight;
	::IDirect3DTexture9* m_pDirectLockableSysMemTexture;
	std::mutex m_mtx;
	bool DirectFullSurface;
	::D3DLOCKED_RECT DirectLockedRect;
	DWORD DirectLockedFlags;
	vector<D3D9Wrapper::D3DLockedRect> DirectLockedRects;
	bool DirectLockRectNoMemTex;
public:
	void Delete();
	bool bound;
	bool zero_d3d_ref_count;
	void Bound();
	void Unbound();
	bool IsDirectStereoSurface();
	::IDirect3DSurface9* DirectModeGetMono();
	::IDirect3DSurface9* DirectModeGetLeft();
	::IDirect3DSurface9* DirectModeGetRight();

	bool possibleDepthBuffer;
	bool depthstencil_replacement_multisampled;
	bool depthstencil_replacement_resolvedAA;
	bool depthstencil_replacement_direct_sample;
	bool depthstencil_replacement_nvapi_registered;
	DepthSourceInfo depthSourceInfo;
	::IDirect3DTexture9 *depthstencil_replacement_texture;
	::IDirect3DSurface9 *depthstencil_replacement_surface;
	::IDirect3DSurface9 *depthstencil_multisampled_rt_surface;
	HRESULT resolveDepthReplacement();
	HRESULT copyDepthSurfaceToTexture();
    static ThreadSafePointerSet m_List;
	// Delayed creation parameters from device.
	int magic;
	UINT _Width;
	UINT _Height;
	::D3DFORMAT _Format;
	::D3DMULTISAMPLE_TYPE _MultiSample;
	DWORD _MultisampleQuality;
	BOOL _Discard;
    D3D9Wrapper::IDirect3DDevice9 *_Device;
	// Delayed creation parameters from texture.
	UINT _Level;
	D3D9Wrapper::IDirect3DTexture9 *_Texture;
	bool pendingGetSurfaceLevel;
	D3D9Wrapper::IDirect3DCubeTexture9 *_CubeTexture;
	bool pendingGetCubeMapSurface;
	::D3DCUBEMAP_FACES _FaceType;
	DWORD _Usage;

	void HookSurface();

	IDirect3DSurface9(::LPDIRECT3DSURFACE9 pSurface, D3D9Wrapper::IDirect3DDevice9 *hackerDevice, ::LPDIRECT3DSURFACE9 pDirectModeRightSurface);
	IDirect3DSurface9(::LPDIRECT3DSURFACE9 pSurface, D3D9Wrapper::IDirect3DDevice9 *hackerDevice, ::LPDIRECT3DSURFACE9 pDirectModeRightSurface, D3D9Wrapper::IDirect3DSwapChain9 *owningContainer);
	IDirect3DSurface9(::LPDIRECT3DSURFACE9 pSurface, D3D9Wrapper::IDirect3DDevice9 *hackerDevice, ::LPDIRECT3DSURFACE9 pDirectModeRightSurface, D3D9Wrapper::IDirect3DTexture9 *owningContainer);
	IDirect3DSurface9(::LPDIRECT3DSURFACE9 pSurface, D3D9Wrapper::IDirect3DDevice9 *hackerDevice, ::LPDIRECT3DSURFACE9 pDirectModeRightSurface, D3D9Wrapper::IDirect3DCubeTexture9 *owningContainer);
	static IDirect3DSurface9* GetDirect3DSurface9(::LPDIRECT3DSURFACE9 pSurface, D3D9Wrapper::IDirect3DDevice9 *hackerDevice, ::LPDIRECT3DSURFACE9 pDirectModeRightSurface);
	static IDirect3DSurface9* GetDirect3DSurface9(::LPDIRECT3DSURFACE9 pSurface, D3D9Wrapper::IDirect3DDevice9 *hackerDevice, ::LPDIRECT3DSURFACE9 pDirectModeRightSurface, D3D9Wrapper::IDirect3DSwapChain9 *owningContainer);
	static IDirect3DSurface9* GetDirect3DSurface9(::LPDIRECT3DSURFACE9 pSurface, D3D9Wrapper::IDirect3DDevice9 *hackerDevice, ::LPDIRECT3DSURFACE9 pDirectModeRightSurface, D3D9Wrapper::IDirect3DTexture9 *owningContainer);
	static IDirect3DSurface9* GetDirect3DSurface9(::LPDIRECT3DSURFACE9 pSurface, D3D9Wrapper::IDirect3DDevice9 *hackerDevice, ::LPDIRECT3DSURFACE9 pDirectModeRightSurface, D3D9Wrapper::IDirect3DCubeTexture9 *owningContainer);

	D3D9Wrapper::IDirect3DSwapChain9 *m_OwningSwapChain;
	D3D9Wrapper::IDirect3DTexture9 *m_OwningTexture;
	D3D9Wrapper::IDirect3DCubeTexture9 *m_OwningCubeTexture;
	D3D9Wrapper::SurfaceContainerOwnerType m_OwningContainerType;

	__forceinline ::LPDIRECT3DSURFACE9 GetD3DSurface9() { return (::LPDIRECT3DSURFACE9)m_pUnk; }

    /*** IUnknown methods ***/
	STDMETHOD(QueryInterface)(THIS_ REFIID riid, void** ppvObj) override;
	STDMETHOD_(ULONG,AddRef)(THIS) override;
    STDMETHOD_(ULONG,Release)(THIS) override;

    /*** IDirect3DResource9 methods ***/
    STDMETHOD(GetDevice)(THIS_ D3D9Wrapper::IDirect3DDevice9** ppDevice);
    STDMETHOD(SetPrivateData)(THIS_ REFGUID refguid,CONST void* pData,DWORD SizeOfData,DWORD Flags);
    STDMETHOD(GetPrivateData)(THIS_ REFGUID refguid,void* pData,DWORD* pSizeOfData);
    STDMETHOD(FreePrivateData)(THIS_ REFGUID refguid);
    STDMETHOD_(DWORD, SetPriority)(THIS_ DWORD PriorityNew);
    STDMETHOD_(DWORD, GetPriority)(THIS);
    STDMETHOD_(void, PreLoad)(THIS);
    STDMETHOD_(::D3DRESOURCETYPE, GetType)(THIS);

    /*** IDirect3DSurface9 methods ***/
    STDMETHOD(GetContainer)(THIS_ REFIID riid,void** ppContainer);
    STDMETHOD(GetDesc)(THIS_ ::D3DSURFACE_DESC *pDesc);
    STDMETHOD(LockRect)(THIS_ ::D3DLOCKED_RECT* pLockedRect,CONST RECT* pRect,DWORD Flags);
    STDMETHOD(UnlockRect)(THIS);
    STDMETHOD(GetDC)(THIS_ HDC *phdc);
    STDMETHOD(ReleaseDC)(THIS_ HDC hdc);
};

class IDirect3DVertexDeclaration9 : public D3D9Wrapper::IDirect3DUnknown
{
private:
	D3D9Wrapper::IDirect3DDevice9 *hackerDevice;
public:
	void Delete();
	bool bound;
	bool zero_d3d_ref_count;
	void Bound() {
		bound = true;
	};
	void Unbound() {
		bound = false;
		if (zero_d3d_ref_count)
			Delete();
	};

    static ThreadSafePointerSet m_List;
	int magic;
	// Delayed creation parameters.
	::D3DVERTEXELEMENT9 _VertexElements;
	D3D9Wrapper::IDirect3DDevice9 *pendingDevice;
	bool pendingCreateVertexDeclaration;

	IDirect3DVertexDeclaration9(::LPDIRECT3DVERTEXDECLARATION9 pVertexDeclaration, D3D9Wrapper::IDirect3DDevice9 *hackerDevice);
    static IDirect3DVertexDeclaration9* GetDirect3DVertexDeclaration9(::LPDIRECT3DVERTEXDECLARATION9 pVertexDeclaration, D3D9Wrapper::IDirect3DDevice9 *hackerDevice);
	__forceinline ::LPDIRECT3DVERTEXDECLARATION9 GetD3DVertexDeclaration9() { return (::LPDIRECT3DVERTEXDECLARATION9) m_pUnk; }

	void HookVertexDeclaration();

    /*** IUnknown methods ***/
	STDMETHOD(QueryInterface)(THIS_ REFIID riid, void** ppvObj) override;
    STDMETHOD_(ULONG,AddRef)(THIS) override;
    STDMETHOD_(ULONG,Release)(THIS) override;

    /*** IDirect3DVertexDeclaration9 methods ***/
    STDMETHOD(GetDevice)(THIS_ D3D9Wrapper::IDirect3DDevice9** ppDevice);
    STDMETHOD(GetDeclaration)(THIS_ ::D3DVERTEXELEMENT9* pElement,UINT* pNumElements);
};
enum class TextureType {
	Unknown,
	Texture2D,
	Cube,
	Volume
};

class IDirect3DBaseTexture9 : public D3D9Wrapper::IDirect3DResource9
{
public:
	void Delete();
	ULONG shared_ref_count;
	set<DWORD> bound;
	bool zero_d3d_ref_count;
	void Bound(DWORD Stage);
	void Unbound(DWORD Stage);

	TextureType texType;
	// Delayed creation parameters.
	int magic;
	UINT _Levels;
	DWORD _Usage;
	::D3DFORMAT _Format;
	::D3DPOOL _Pool;
	D3D9Wrapper::IDirect3DDevice9 *_Device;

	bool pendingCreateTexture;
	// Delayed lock/unlock.
	DWORD _Flags;
	UINT _Level;
	char *_Buffer;
	bool pendingLockUnlock;

	IDirect3DBaseTexture9(::LPDIRECT3DBASETEXTURE9 pTexture, D3D9Wrapper::IDirect3DDevice9 *hackerDevice, TextureType type);
	__forceinline ::LPDIRECT3DBASETEXTURE9 GetD3DBaseTexture9() { return (::LPDIRECT3DBASETEXTURE9) m_pUnk; }
};

class IDirect3DTexture9 : public D3D9Wrapper::IDirect3DBaseTexture9
{
private:
	std::unordered_map<UINT, D3D9Wrapper::IDirect3DSurface9*> m_wrappedSurfaceLevels;
	::IDirect3DTexture9* m_pDirectTextureRight;
	std::mutex m_mtx;
	std::unordered_map<UINT, std::vector<D3D9Wrapper::D3DLockedRect>> DirectLockedRects;
	std::unordered_map<UINT, bool> DirectFullSurfaces;
	std::unordered_map<UINT, bool> DirectNewSurface;
	std::unordered_map<UINT, ::IDirect3DTexture9*> m_pDirectLockableSysMemTextures;
	bool DirectLockRectNoMemTex;

	DWORD DirectLockedFlags;
	::D3DLOCKED_RECT DirectLockedRect;

public:
	void Delete();
	bool IsDirectStereoTexture();
	::IDirect3DTexture9* DirectModeGetMono();
	::IDirect3DTexture9* DirectModeGetLeft();
	::IDirect3DTexture9* DirectModeGetRight();
    static ThreadSafePointerSet m_List;
	// Delayed creation parameters.
	//int magic;
	UINT _Width;
	UINT _Height;
	void HookTexture();

	IDirect3DTexture9(::LPDIRECT3DTEXTURE9 pTexture, D3D9Wrapper::IDirect3DDevice9 *hackerDevice, ::LPDIRECT3DTEXTURE9 pDirectModeRightSurface);
    static IDirect3DTexture9* GetDirect3DTexture9(::LPDIRECT3DTEXTURE9 pTexture, D3D9Wrapper::IDirect3DDevice9 *hackerDevice, ::LPDIRECT3DTEXTURE9 pDirectModeRightSurface);
	__forceinline ::LPDIRECT3DTEXTURE9 GetD3DTexture9() { return (::LPDIRECT3DTEXTURE9) m_pUnk; }

    /*** IUnknown methods ***/
	STDMETHOD(QueryInterface)(THIS_ REFIID riid, void** ppvObj) override;
    STDMETHOD_(ULONG,AddRef)(THIS) override;
    STDMETHOD_(ULONG,Release)(THIS) override;

    /*** IDirect3DBaseTexture9 methods ***/
    STDMETHOD(GetDevice)(THIS_ D3D9Wrapper::IDirect3DDevice9** ppDevice);
    STDMETHOD(SetPrivateData)(THIS_ REFGUID refguid,CONST void* pData,DWORD SizeOfData,DWORD Flags);
    STDMETHOD(GetPrivateData)(THIS_ REFGUID refguid,void* pData,DWORD* pSizeOfData);
    STDMETHOD(FreePrivateData)(THIS_ REFGUID refguid);
    STDMETHOD_(DWORD, SetPriority)(THIS_ DWORD PriorityNew);
    STDMETHOD_(DWORD, GetPriority)(THIS);
    STDMETHOD_(void, PreLoad)(THIS);
    STDMETHOD_(::D3DRESOURCETYPE, GetType)(THIS);
    STDMETHOD_(DWORD, SetLOD)(THIS_ DWORD LODNew);
    STDMETHOD_(DWORD, GetLOD)(THIS);
    STDMETHOD_(DWORD, GetLevelCount)(THIS);
    STDMETHOD(SetAutoGenFilterType)(THIS_ ::D3DTEXTUREFILTERTYPE FilterType);
    STDMETHOD_(::D3DTEXTUREFILTERTYPE, GetAutoGenFilterType)(THIS);
    STDMETHOD_(void, GenerateMipSubLevels)(THIS);
    STDMETHOD(GetLevelDesc)(THIS_ UINT Level,::D3DSURFACE_DESC *pDesc);
    STDMETHOD(GetSurfaceLevel)(THIS_ UINT Level, D3D9Wrapper::IDirect3DSurface9** ppSurfaceLevel);
    STDMETHOD(LockRect)(THIS_ UINT Level,::D3DLOCKED_RECT* pLockedRect,CONST RECT* pRect,DWORD Flags);
    STDMETHOD(UnlockRect)(THIS_ UINT Level);
    STDMETHOD(AddDirtyRect)(THIS_ CONST RECT* pDirtyRect);
};


struct LockedMem {
	VOID* updatedMem;
	UINT OffsetToLock;
	UINT SizeToLock;
	DWORD Flags;
};

struct pLockedMem {
	void **ppbData;
	UINT OffsetToLock;
	UINT SizeToLock;
	DWORD Flags;
};

class IDirect3DVertexBuffer9 : public D3D9Wrapper::IDirect3DResource9
{
public:
	void Delete();
	bool bound;
	bool zero_d3d_ref_count;
	void Bound() {
		bound = true;
	};
	void Unbound() {
		bound = false;
		if (zero_d3d_ref_count)
			Delete();
	};

	static ThreadSafePointerSet m_List;
	int magic;
	// Delayed creation parameters.
	UINT _Length;
	DWORD _Usage;
	DWORD _FVF;
	::D3DPOOL _Pool;
	D3D9Wrapper::IDirect3DDevice9 *_Device;
	bool pendingCreateVertexBuffer;
	// Delayed buffer lock/unlock.
	char *_Buffer;
	DWORD _Flags;
	bool pendingLockUnlock;

	void HookVertexBuffer();

	IDirect3DVertexBuffer9(::LPDIRECT3DVERTEXBUFFER9 pVertexBuffer, D3D9Wrapper::IDirect3DDevice9 *hackerDevice);
    static IDirect3DVertexBuffer9* GetDirect3DVertexBuffer9(::LPDIRECT3DVERTEXBUFFER9 pVertexBuffer, D3D9Wrapper::IDirect3DDevice9 *hackerDevice);
	__forceinline ::LPDIRECT3DVERTEXBUFFER9 GetD3DVertexBuffer9() { return (::LPDIRECT3DVERTEXBUFFER9)m_pUnk; }

    /*** IUnknown methods ***/
	STDMETHOD(QueryInterface)(THIS_ REFIID riid, void** ppvObj) override;
    STDMETHOD_(ULONG,AddRef)(THIS) override;
    STDMETHOD_(ULONG,Release)(THIS) override;

    /*** IDirect3DResource9 methods ***/
    STDMETHOD(GetDevice)(THIS_ D3D9Wrapper::IDirect3DDevice9** ppDevice);
    STDMETHOD(SetPrivateData)(THIS_ REFGUID refguid,CONST void* pData,DWORD SizeOfData,DWORD Flags);
    STDMETHOD(GetPrivateData)(THIS_ REFGUID refguid,void* pData,DWORD* pSizeOfData);
    STDMETHOD(FreePrivateData)(THIS_ REFGUID refguid);
    STDMETHOD_(DWORD, SetPriority)(THIS_ DWORD PriorityNew);
    STDMETHOD_(DWORD, GetPriority)(THIS);
    STDMETHOD_(void, PreLoad)(THIS);
    STDMETHOD_(::D3DRESOURCETYPE, GetType)(THIS);

    /*** IDirect3DResource9 methods ***/
    STDMETHOD(Lock)(THIS_ UINT OffsetToLock,UINT SizeToLock,void** ppbData,DWORD Flags);
    STDMETHOD(Unlock)(THIS);
    STDMETHOD(GetDesc)(THIS_ ::D3DVERTEXBUFFER_DESC *pDesc);
};

class IDirect3DIndexBuffer9 : public D3D9Wrapper::IDirect3DResource9
{
public:
	void Delete();
	bool bound;
	bool zero_d3d_ref_count;
	void Bound() {
		bound = true;
	};
	void Unbound() {
		bound = false;
		if (zero_d3d_ref_count)
			Delete();
	};

    static ThreadSafePointerSet m_List;
	int magic;
	// Delayed creation parameters.
	UINT _Length;
	DWORD _Usage;
	::D3DFORMAT _Format;
	::D3DPOOL _Pool;
	D3D9Wrapper::IDirect3DDevice9 *_Device;
	bool pendingCreateIndexBuffer;
	// Delayed buffer lock/unlock.
	char *_Buffer;
	DWORD _Flags;
	bool pendingLockUnlock;

	void HookIndexBuffer();

	IDirect3DIndexBuffer9(::LPDIRECT3DINDEXBUFFER9 pIndexBuffer, D3D9Wrapper::IDirect3DDevice9 *hackerDevice);
    static IDirect3DIndexBuffer9* GetDirect3DIndexBuffer9(::LPDIRECT3DINDEXBUFFER9 pIndexBuffer, D3D9Wrapper::IDirect3DDevice9 *hackerDevice);
	__forceinline ::LPDIRECT3DINDEXBUFFER9 GetD3DIndexBuffer9() { return (::LPDIRECT3DINDEXBUFFER9)m_pUnk; }

    /*** IUnknown methods ***/
	STDMETHOD(QueryInterface)(THIS_ REFIID riid, void** ppvObj) override;
    STDMETHOD_(ULONG,AddRef)(THIS) override;
    STDMETHOD_(ULONG,Release)(THIS) override;

    /*** IDirect3DResource9 methods ***/
    STDMETHOD(GetDevice)(THIS_ D3D9Wrapper::IDirect3DDevice9** ppDevice);
    STDMETHOD(SetPrivateData)(THIS_ REFGUID refguid,CONST void* pData,DWORD SizeOfData,DWORD Flags);
    STDMETHOD(GetPrivateData)(THIS_ REFGUID refguid,void* pData,DWORD* pSizeOfData);
    STDMETHOD(FreePrivateData)(THIS_ REFGUID refguid);
    STDMETHOD_(DWORD, SetPriority)(THIS_ DWORD PriorityNew);
    STDMETHOD_(DWORD, GetPriority)(THIS);
    STDMETHOD_(void, PreLoad)(THIS);
    STDMETHOD_(::D3DRESOURCETYPE, GetType)(THIS);

    /*** IDirect3DResource9 methods ***/
    STDMETHOD(Lock)(THIS_ UINT OffsetToLock,UINT SizeToLock,void** ppbData,DWORD Flags);
    STDMETHOD(Unlock)(THIS);
    STDMETHOD(GetDesc)(THIS_ ::D3DINDEXBUFFER_DESC *pDesc);
};

class IDirect3DQuery9 : public D3D9Wrapper::IDirect3DUnknown
{
private:
	D3D9Wrapper::IDirect3DDevice9 *hackerDevice;
public:
	void Delete();
    static ThreadSafePointerSet m_List;
	int magic;

	void HookQuery();

	IDirect3DQuery9(::LPDIRECT3DQUERY9 pQuery, D3D9Wrapper::IDirect3DDevice9 *hackerDevice);
    static IDirect3DQuery9* GetDirect3DQuery9(::LPDIRECT3DQUERY9 pQuery, D3D9Wrapper::IDirect3DDevice9 *hackerDevice);
	__forceinline ::LPDIRECT3DQUERY9 GetD3DQuery9() { return (::LPDIRECT3DQUERY9)m_pUnk; }

    /*** IUnknown methods ***/
	STDMETHOD(QueryInterface)(THIS_ REFIID riid, void** ppvObj) override;
    STDMETHOD_(ULONG,AddRef)(THIS) override;
    STDMETHOD_(ULONG,Release)(THIS) override;

    /*** IDirect3DQuery9 methods ***/
    STDMETHOD(GetDevice)(THIS_ D3D9Wrapper::IDirect3DDevice9** ppDevice);
    STDMETHOD_(::D3DQUERYTYPE, GetType)(THIS);
    STDMETHOD_(DWORD, GetDataSize)(THIS);
    STDMETHOD(Issue)(THIS_ DWORD dwIssueFlags);
    STDMETHOD(GetData)(THIS_ void* pData,DWORD dwSize,DWORD dwGetDataFlags);
};
enum ShaderType
{
	Vertex = 1,
	Pixel = 2
};
class IDirect3DShader9 : public D3D9Wrapper::IDirect3DUnknown
{
public:
	D3D9Wrapper::IDirect3DDevice9 *hackerDevice;
	int magic;
	ShaderType shaderType;
	UINT64 hash;
	OriginalShaderInfo originalShaderInfo;
	IUnknown *zeroShader;
	IUnknown *originalShader;
	ShaderInfoData *shaderInfo;
	ShaderOverride *shaderOverride;
	string *compiledShader;

	__forceinline IUnknown* GetD3DShader9() { return m_pUnk; };
	__forceinline ::LPDIRECT3DPIXELSHADER9 GetD3DPixelShader9() { return (::LPDIRECT3DPIXELSHADER9)m_pUnk; };
	__forceinline ::LPDIRECT3DVERTEXSHADER9 GetD3DVertexShader9() { return (::LPDIRECT3DVERTEXSHADER9)m_pUnk; };
	__forceinline ::LPDIRECT3DPIXELSHADER9 GetZeroPixelShader() { return (::LPDIRECT3DPIXELSHADER9)zeroShader; };
	__forceinline ::LPDIRECT3DVERTEXSHADER9 GetZeroVertexShader() { return (::LPDIRECT3DVERTEXSHADER9)zeroShader; };
	__forceinline IDirect3DShader9* GetShader() { return this;};
	IDirect3DShader9(IUnknown *shader, D3D9Wrapper::IDirect3DDevice9 *hackerDevice, ShaderType shaderType);
	void Delete();
};


class IDirect3DVertexShader9 : public D3D9Wrapper::IDirect3DShader9
{
public:
	void Delete();
	bool bound;
	bool zero_d3d_ref_count;
	void Bound() {
		bound = true;
	};
	void Unbound() {
		bound = false;
		if (zero_d3d_ref_count)
			Delete();
	};
    static ThreadSafePointerSet m_List;
	IDirect3DVertexShader9(::LPDIRECT3DVERTEXSHADER9 pVS, D3D9Wrapper::IDirect3DDevice9 *hackerDevice);
    static IDirect3DVertexShader9* GetDirect3DVertexShader9(::LPDIRECT3DVERTEXSHADER9 pVS, D3D9Wrapper::IDirect3DDevice9 *hackerDevice);

	__forceinline ::LPDIRECT3DVERTEXSHADER9 GetD3DVertexShader9() { return (::LPDIRECT3DVERTEXSHADER9)m_pUnk; }
	void HookVertexShader();

    /*** IUnknown methods ***/
	STDMETHOD(QueryInterface)(THIS_ REFIID riid, void** ppvObj) override;
    STDMETHOD_(ULONG,AddRef)(THIS) override;
    STDMETHOD_(ULONG,Release)(THIS) override;

    /*** IDirect3DVertexShader9 methods ***/
    STDMETHOD(GetDevice)(THIS_ D3D9Wrapper::IDirect3DDevice9** ppDevice);
    STDMETHOD(GetFunction)(THIS_ void*,UINT* pSizeOfData);
};

class IDirect3DPixelShader9 : public D3D9Wrapper::IDirect3DShader9
{
public:
	void Delete();
	bool bound;
	bool zero_d3d_ref_count;
	void Bound() {
		bound = true;
	};
	void Unbound() {
		bound = false;
		if (zero_d3d_ref_count)
			Delete();
	};
    static ThreadSafePointerSet m_List;
	void HookPixelShader();

	IDirect3DPixelShader9(::LPDIRECT3DPIXELSHADER9 pPS, D3D9Wrapper::IDirect3DDevice9 *hackerDevice);
    static IDirect3DPixelShader9* GetDirect3DPixelShader9(::LPDIRECT3DPIXELSHADER9 pPS, D3D9Wrapper::IDirect3DDevice9 *hackerDevice);
	__forceinline ::LPDIRECT3DPIXELSHADER9 GetD3DPixelShader9() { return (::LPDIRECT3DPIXELSHADER9)m_pUnk; };

	/*** IUnknown methods ***/
	STDMETHOD(QueryInterface)(THIS_ REFIID riid, void** ppvObj) override;
    STDMETHOD_(ULONG,AddRef)(THIS) override;
    STDMETHOD_(ULONG,Release)(THIS) override;

    /*** IDirect3DPixelShader9 methods ***/
    STDMETHOD(GetDevice)(THIS_ D3D9Wrapper::IDirect3DDevice9** ppDevice);
    STDMETHOD(GetFunction)(THIS_ void*,UINT* pSizeOfData);
};
class IDirect3DCubeTexture9 : public D3D9Wrapper::IDirect3DBaseTexture9
{
private:
	std::unordered_map<UINT, D3D9Wrapper::IDirect3DSurface9*> m_wrappedSurfaceLevels;
	::IDirect3DCubeTexture9* m_pDirectCubeTextureRight;
	std::mutex m_mtx;
	std::unordered_map<UINT, std::vector<D3DLockedRect>> DirectLockedRects;
	std::unordered_map<UINT, bool> DirectFullSurfaces;
	std::unordered_map<UINT, bool> DirectNewSurface;
	std::unordered_map<UINT, ::IDirect3DCubeTexture9*> m_pDirectLockableSysMemTextures;
	bool DirectLockRectNoMemTex;

	DWORD DirectLockedFlags;
	::D3DLOCKED_RECT DirectLockedRect;

public:
	void Delete();
	bool IsDirectStereoCubeTexture();
	::IDirect3DCubeTexture9* DirectModeGetMono();
	::IDirect3DCubeTexture9* DirectModeGetLeft();
	::IDirect3DCubeTexture9* DirectModeGetRight();

	static ThreadSafePointerSet m_List;
	// Delayed creation parameters.
	UINT _EdgeLength;
	::D3DCUBEMAP_FACES _FaceType;
	void HookCubeTexture();

	IDirect3DCubeTexture9(::LPDIRECT3DCUBETEXTURE9 pCubeTexture, D3D9Wrapper::IDirect3DDevice9 *hackerDevice, ::LPDIRECT3DCUBETEXTURE9 pDirectModeRightCubeTexture);
	static IDirect3DCubeTexture9* GetDirect3DCubeTexture9(::LPDIRECT3DCUBETEXTURE9 pCubeTexture, D3D9Wrapper::IDirect3DDevice9 *hackerDevice, ::LPDIRECT3DCUBETEXTURE9 pDirectModeRightCubeTexture);
	__forceinline ::LPDIRECT3DCUBETEXTURE9 GetD3DCubeTexture9() { return (::LPDIRECT3DCUBETEXTURE9) m_pUnk; }

	/*** IUnknown methods ***/
	STDMETHOD(QueryInterface)(THIS_ REFIID riid, void** ppvObj) override;
	STDMETHOD_(ULONG, AddRef)(THIS) override;
	STDMETHOD_(ULONG, Release)(THIS) override;

	/*** IDirect3DResource9 methods ***/
	STDMETHOD(GetDevice)(THIS_ D3D9Wrapper::IDirect3DDevice9** ppDevice);
	STDMETHOD(SetPrivateData)(THIS_ REFGUID refguid, CONST void* pData, DWORD SizeOfData, DWORD Flags);
	STDMETHOD(GetPrivateData)(THIS_ REFGUID refguid, void* pData, DWORD* pSizeOfData);
	STDMETHOD(FreePrivateData)(THIS_ REFGUID refguid);
	STDMETHOD_(DWORD, SetPriority)(THIS_ DWORD PriorityNew);
	STDMETHOD_(DWORD, GetPriority)(THIS);
	STDMETHOD_(void, PreLoad)(THIS);
	STDMETHOD_(::D3DRESOURCETYPE, GetType)(THIS);
	/*** IDirect3DBaseTexture9 methods ***/
	STDMETHOD_(DWORD, SetLOD)(THIS_ DWORD LODNew);
	STDMETHOD_(DWORD, GetLOD)(THIS);
	STDMETHOD_(DWORD, GetLevelCount)(THIS);
	STDMETHOD(SetAutoGenFilterType)(THIS_ ::D3DTEXTUREFILTERTYPE FilterType);
	STDMETHOD_(::D3DTEXTUREFILTERTYPE, GetAutoGenFilterType)(THIS);
	STDMETHOD_(void, GenerateMipSubLevels)(THIS);
	/*** IDirect3DCubeTexture9 methods ***/
	STDMETHOD(GetLevelDesc)(THIS_ UINT Level, ::D3DSURFACE_DESC *pDesc);
	STDMETHOD(GetCubeMapSurface)(THIS_ ::D3DCUBEMAP_FACES FaceType, UINT Level, D3D9Wrapper::IDirect3DSurface9 **ppCubeMapSurface);
	STDMETHOD(LockRect)(THIS_ ::D3DCUBEMAP_FACES FaceType, UINT Level, ::D3DLOCKED_RECT* pLockedRect, CONST RECT* pRect, DWORD Flags);
	STDMETHOD(UnlockRect)(THIS_ ::D3DCUBEMAP_FACES FaceType, UINT Level);
	STDMETHOD(AddDirtyRect)(THIS_ ::D3DCUBEMAP_FACES FaceType, CONST RECT* pDirtyRect);
};

class IDirect3DVolumeTexture9 : public D3D9Wrapper::IDirect3DBaseTexture9
{
public:
	void Delete();
	static ThreadSafePointerSet m_List;
	// Delayed creation parameters.
	UINT _Width;
	UINT _Height;
	UINT _Depth;
	unordered_map<UINT, D3D9Wrapper::IDirect3DVolume9*> m_wrappedVolumeLevels;
	void HookVolumeTexture();

	IDirect3DVolumeTexture9(::LPDIRECT3DVOLUMETEXTURE9 pVolumeTexture, D3D9Wrapper::IDirect3DDevice9 *hackerDevice);
	static IDirect3DVolumeTexture9* GetDirect3DVolumeTexture9(::LPDIRECT3DVOLUMETEXTURE9 pVolumeTexture, D3D9Wrapper::IDirect3DDevice9 *hackerDevice);
	__forceinline ::LPDIRECT3DVOLUMETEXTURE9 GetD3DVolumeTexture9() { return (::LPDIRECT3DVOLUMETEXTURE9) m_pUnk; }

	/*** IUnknown methods ***/
	STDMETHOD(QueryInterface)(THIS_ REFIID riid, void** ppvObj) override;
	STDMETHOD_(ULONG, AddRef)(THIS) override;
	STDMETHOD_(ULONG, Release)(THIS) override;

	/*** IDirect3DResource9 methods ***/
	STDMETHOD(GetDevice)(THIS_ D3D9Wrapper::IDirect3DDevice9** ppDevice);
	STDMETHOD(SetPrivateData)(THIS_ REFGUID refguid, CONST void* pData, DWORD SizeOfData, DWORD Flags);
	STDMETHOD(GetPrivateData)(THIS_ REFGUID refguid, void* pData, DWORD* pSizeOfData);
	STDMETHOD(FreePrivateData)(THIS_ REFGUID refguid);
	STDMETHOD_(DWORD, SetPriority)(THIS_ DWORD PriorityNew);
	STDMETHOD_(DWORD, GetPriority)(THIS);
	STDMETHOD_(void, PreLoad)(THIS);
	STDMETHOD_(::D3DRESOURCETYPE, GetType)(THIS);
	/*** IDirect3DBaseTexture9 methods ***/
	STDMETHOD_(DWORD, SetLOD)(THIS_ DWORD LODNew);
	STDMETHOD_(DWORD, GetLOD)(THIS);
	STDMETHOD_(DWORD, GetLevelCount)(THIS);
	STDMETHOD(SetAutoGenFilterType)(THIS_ ::D3DTEXTUREFILTERTYPE FilterType);
	STDMETHOD_(::D3DTEXTUREFILTERTYPE, GetAutoGenFilterType)(THIS);
	STDMETHOD_(void, GenerateMipSubLevels)(THIS);
	/*** IDirect3DVolumeTexture9 methods ***/
	STDMETHOD(GetLevelDesc)(THIS_ UINT Level, ::D3DVOLUME_DESC *pDesc);
	STDMETHOD(GetVolumeLevel)(THIS_ UINT Level, D3D9Wrapper::IDirect3DVolume9 **ppVolumeLevel);
	STDMETHOD(LockBox)(THIS_ UINT Level, ::D3DLOCKED_BOX *pLockedVolume, CONST ::D3DBOX *pBox, DWORD Flags);
	STDMETHOD(UnlockBox)(THIS_ UINT Level);
	STDMETHOD(AddDirtyBox)(THIS_ CONST ::D3DBOX *pDirtyBox);
};

class IDirect3DVolume9 : public D3D9Wrapper::IDirect3DUnknown
{
private:
	D3D9Wrapper::IDirect3DDevice9 *hackerDevice;
public:
	void Delete();
	bool zero_d3d_ref_count;
	static ThreadSafePointerSet m_List;
	// Delayed creation parameters from device.
	int magic;
	UINT _Width;
	UINT _Height;
	UINT _Depth;
	::D3DFORMAT _Format;
	D3D9Wrapper::IDirect3DDevice9 *_Device;
	// Delayed creation parameters from texture.
	UINT _Level;
	D3D9Wrapper::IDirect3DVolumeTexture9 *_VolumeTexture;
	bool pendingGetVolumeLevel;
	void HookVolume();

	IDirect3DVolume9(::LPDIRECT3DVOLUME9 pVolume, D3D9Wrapper::IDirect3DDevice9 *hackerDevice, D3D9Wrapper::IDirect3DVolumeTexture9 *owningContainer);
	static IDirect3DVolume9* GetDirect3DVolume9(::LPDIRECT3DVOLUME9 pVolume, D3D9Wrapper::IDirect3DDevice9 *hackerDevice, D3D9Wrapper::IDirect3DVolumeTexture9 *owningContainer);

	D3D9Wrapper::IDirect3DVolumeTexture9 *m_OwningContainer;
	__forceinline ::LPDIRECT3DVOLUME9 GetD3DVolume9() { return (::LPDIRECT3DVOLUME9)m_pUnk; }

	/*** IUnknown methods ***/
	STDMETHOD(QueryInterface)(THIS_ REFIID riid, void** ppvObj) override;
	STDMETHOD_(ULONG, AddRef)(THIS) override;
	STDMETHOD_(ULONG, Release)(THIS) override;

	/*** IDirect3DVolume9 methods ***/
	STDMETHOD(GetDevice)(THIS_ D3D9Wrapper::IDirect3DDevice9** ppDevice);
	STDMETHOD(SetPrivateData)(THIS_ REFGUID refguid, CONST void* pData, DWORD SizeOfData, DWORD Flags);
	STDMETHOD(GetPrivateData)(THIS_ REFGUID refguid, void* pData, DWORD* pSizeOfData);
	STDMETHOD(FreePrivateData)(THIS_ REFGUID refguid);

	STDMETHOD(GetContainer)(THIS_ REFIID riid, void** ppContainer);
	STDMETHOD(GetDesc)(THIS_ ::D3DVOLUME_DESC *pDesc);
	STDMETHOD(LockBox)(THIS_ ::D3DLOCKED_BOX *pLockedVolume, CONST ::D3DBOX *pBox, DWORD Flags);
	STDMETHOD(UnlockBox)(THIS);
};

class IDirect3DStateBlock9 : public D3D9Wrapper::IDirect3DUnknown
{
private:
	D3D9Wrapper::IDirect3DDevice9 *hackerDevice;
public:
	void Delete();
	static ThreadSafePointerSet m_List;
	::IDirect3DStateBlock9 *mDirectModeStateBlockDuplication;
	void HookStateBlock();

	IDirect3DStateBlock9(::LPDIRECT3DSTATEBLOCK9 pStateBlock, D3D9Wrapper::IDirect3DDevice9 *hackerDevice);
	static IDirect3DStateBlock9* GetDirect3DStateBlock9(::LPDIRECT3DSTATEBLOCK9 pStateBlock, D3D9Wrapper::IDirect3DDevice9 *hackerDevice);
	__forceinline ::LPDIRECT3DSTATEBLOCK9 GetD3DStateBlock9() { return (::LPDIRECT3DSTATEBLOCK9)m_pUnk; }

	/*** IUnknown methods ***/
	STDMETHOD(QueryInterface)(THIS_ REFIID riid, void** ppvObj) override;
	STDMETHOD_(ULONG, AddRef)(THIS) override;
	STDMETHOD_(ULONG, Release)(THIS) override;

	/*** IDirect3DStateBlock9 methods ***/
	STDMETHOD(GetDevice)(THIS_ D3D9Wrapper::IDirect3DDevice9 **ppDevice);
	STDMETHOD(Capture)(THIS_);
	STDMETHOD(Apply)(THIS_);
};
#include "FrameAnalysis.h"
