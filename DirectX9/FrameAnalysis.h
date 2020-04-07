#pragma once
// FIXME: This GUID is not properly implemented in QueryInterface
DEFINE_GUID(IID_FrameAnalysisDevice9,
	0xbdd9edf9, 0x2389, 0x4ffa, 0x9c, 0x81, 0x76, 0x9a, 0x4b, 0x68, 0x86, 0x3b);
class FrameAnalysisDevice : public D3D9Wrapper::IDirect3DDevice9
{
private:
	void FrameAnalysisLogShaderHash(D3D9Wrapper::IDirect3DShader9 *shader);
	void FrameAnalysisLogResourceHash(D3D9Wrapper::IDirect3DResource9 *resource);
	void FrameAnalysisLogResource(int slot, char *slot_name, D3D9Wrapper::IDirect3DResource9 *resource);
	void FrameAnalysisLogResourceArray(UINT start, UINT len, D3D9Wrapper::IDirect3DResource9 *const *ppResources);
	void FrameAnalysisLogMiscArray(UINT start, UINT len, void *const *array);
	void FrameAnalysisLogData(void *buf, UINT size);
	FILE *frame_analysis_log;
	unsigned draw_call;
	unsigned non_draw_call_dump_counter;
	::IDirect3DDevice9* GetDumpingDevice();
	void FrameAnalysisLogQuery(::IDirect3DQuery9 *query);
	template<typename SurfaceType>
	void DumpSurface(SurfaceType *resource, wstring filename, bool stereo, D3D2DTEXTURE_DESC *orig_desc, ::D3DFORMAT format);
	template<typename Surface>HRESULT StageResource(Surface *src,
		D3D2DTEXTURE_DESC *srcDesc, Surface **dst, ::D3DFORMAT format);
	template<typename Surface>
	HRESULT CreateStagingResource(Surface **resource,
		D3D2DTEXTURE_DESC desc, bool stereo, bool msaa, ::D3DFORMAT format);
	template <typename Surface> void DumpStereoResource(Surface *resource, wchar_t *filename,
		::D3DFORMAT format);
	void DumpVBTxt(wchar_t *filename, void *pData,
		UINT size, int idx, UINT stride, UINT offset, UINT first, UINT count, ::IDirect3DVertexDeclaration9 *layout,
		::D3DPRIMITIVETYPE topology, DrawCallInfo *call_info);
	void DumpIBTxt(wchar_t *filename, void *map,
		UINT size, ::D3DFORMAT format, UINT offset, UINT first, UINT count,
		::D3DPRIMITIVETYPE topology);
	void DumpResource(::IDirect3DResource9 *resource, wchar_t *filename,
		FrameAnalysisOptions buf_type_mask, int idx, ::D3DFORMAT format,
		UINT stride, UINT offset);
	HRESULT FrameAnalysisFilename(wchar_t *filename, size_t size,
		wchar_t *reg, char shader_type, int idx, D3D9Wrapper::IDirect3DResource9 *handle);
	HRESULT FrameAnalysisFilenameResource(wchar_t *filename, size_t size, const wchar_t *type, ::IDirect3DResource9 *handle, bool force_filename_handle, ResourceHandleInfo *handle_info);
	void _DumpTextures(char shader_type);
	void _DumpCBs(char shader_type, wchar_t constant_type, void *buffer, UINT size, UINT start_slot, UINT num_slots);
	void DumpCBs();
	void DumpCB(char shader_type, wchar_t constant_type, UINT start_slot, UINT num_slots) override;
	void DumpVBs(DrawCallInfo *call_info, ::IDirect3DIndexBuffer9 *staged_ib, ::D3DFORMAT ib_fmt, UINT ib_off);
	void DumpIB(DrawCallInfo *call_info, ::IDirect3DIndexBuffer9  **staged_ib, ::D3DFORMAT *format, UINT *offset);
	void DumpTextures();
	void DumpRenderTargets();
	void DumpDepthStencilTargets();
	void FrameAnalysisClearRT(::IDirect3DSurface9 *target);
	void FrameAnalysisAfterDraw(DrawCallInfo *call_info);
	void FrameAnalysisAfterUpdate(D3D9Wrapper::IDirect3DResource9 *resource);
	void _FrameAnalysisAfterUpdate(D3D9Wrapper::IDirect3DResource9 * resource,
		FrameAnalysisOptions type_mask, wchar_t *type);
	template<typename ID3D9Buffer, typename ID3D9BufferDesc>
	void DumpBufferImmediateCtx(ID3D9Buffer *staging, ID3D9BufferDesc *orig_desc,
		wstring filename, FrameAnalysisOptions buf_type_mask, int idx,
		::D3DFORMAT ib_fmt, UINT stride, UINT offset, UINT first, UINT count, ::IDirect3DVertexDeclaration9 *layout,
		::D3DPRIMITIVETYPE topology, DrawCallInfo *call_info,
		::IDirect3DIndexBuffer9 *staged_ib_for_vb, UINT ib_off_for_vb);
	template<typename ID3D9Buffer, typename ID3D9BufferDesc>
	void DumpBuffer(ID3D9Buffer *buffer, wchar_t *filename,
		FrameAnalysisOptions buf_type_mask, int idx, ::D3DFORMAT ib_fmt,
		UINT stride, UINT offset, UINT first, UINT count, ::IDirect3DVertexDeclaration9 *layout,
		::D3DPRIMITIVETYPE topology, DrawCallInfo *call_info,
		ID3D9Buffer **staged_b_ret, ::IDirect3DIndexBuffer9 *staged_ib_for_vb, UINT ib_off_for_vb);
	template<typename SurfaceType>
	HRESULT ResolveMSAA(SurfaceType *src, D3D2DTEXTURE_DESC *srcDesc,
		SurfaceType **dst, ::D3DFORMAT format);
	template<typename SurfaceType>
	void Dump2DSurface(SurfaceType *resource, wchar_t *filename,
		bool stereo, D3D2DTEXTURE_DESC *orig_desc, ::D3DFORMAT format);
	void DumpBufferTxt(wchar_t *filename, void *map,
		UINT size, char type, int idx, UINT stride, UINT offset);
	void DumpMesh(DrawCallInfo *call_info);
	template <typename DescType>
	void DumpDesc(DescType *desc, const wchar_t *filename);
	template<typename Surface>
	const wchar_t* dedupe_sur2d_filename(Surface *resource,
		D3D2DTEXTURE_DESC *orig_desc, wchar_t *dedupe_filename,
		size_t size, const wchar_t *traditional_filename, ::D3DFORMAT format);
	template<typename ID3D9Buffer, typename BufferDesc>
	void dedupe_buf_filename(ID3D9Buffer *resource,
		BufferDesc *orig_desc,
		void *map,
		wchar_t *dedupe_filename, size_t size);
	void dedupe_buf_filename_txt(const wchar_t *bin_filename,
		wchar_t *txt_filename, size_t size, char type, int idx,
		UINT stride, UINT offset);
	void dedupe_buf_filename_vb_txt(const wchar_t *bin_filename,
		wchar_t *txt_filename, size_t size, int idx,
		UINT stride, UINT offset, UINT first, UINT count, ::IDirect3DVertexDeclaration9 *layout,
		::D3DPRIMITIVETYPE topology, DrawCallInfo *call_info);
	void dedupe_buf_filename_ib_txt(const wchar_t *bin_filename,
		wchar_t *txt_filename, size_t size, ::D3DFORMAT ib_fmt,
		UINT offset, UINT first, UINT count, ::D3DPRIMITIVETYPE topology);
	void link_deduplicated_files(const wchar_t *filename, const wchar_t *dedupe_filename);
	void rotate_when_nearing_hard_link_limit(const wchar_t *dedupe_filename);
	void rotate_deduped_file(const wchar_t *dedupe_filename);
	void get_deduped_dir(wchar_t *path, size_t size);
	void determine_vb_count(UINT *count, ::IDirect3DIndexBuffer9 *staged_ib_for_vb,
		DrawCallInfo *call_info, UINT ib_off_for_vb, ::D3DFORMAT ib_fmt);
	void update_per_draw_analyse_options();
	void FrameAnalysisAfterUnmap(D3D9Wrapper::IDirect3DResource9 *pResource);
	void update_stereo_dumping_mode();
	void set_default_dump_formats(bool draw);
	FrameAnalysisOptions analyse_options;
	FrameAnalysisOptions oneshot_analyse_options;
	bool oneshot_valid;
public:

	FrameAnalysisDevice(::LPDIRECT3DDEVICE9 pDevice, D3D9Wrapper::IDirect3D9 *pD3D, bool ex);
	static FrameAnalysisDevice* GetDirect3DDevice(::LPDIRECT3DDEVICE9 pDevice, D3D9Wrapper::IDirect3D9 *pD3D, bool ex);
	~FrameAnalysisDevice();

	// public to allow CommandList access
	void FrameAnalysisLog(char *fmt, ...) override;
	void vFrameAnalysisLog(char *fmt, va_list ap);
	// An alias for the above function that we use to denote that omitting
	// the newline was done intentionally. For now this is just for our
	// reference, but later we might actually make the default function
	// insert a newline:
#define FrameAnalysisLogNoNL FrameAnalysisLog

	void FrameAnalysisTrigger(FrameAnalysisOptions new_options) override;
	void FrameAnalysisDump(::IDirect3DResource9 *resource, FrameAnalysisOptions options,
		const wchar_t *target, ::D3DFORMAT format, UINT stride, UINT offset, ResourceHandleInfo *info);
	/*** IDirect3DUnknown methods ***/
	STDMETHOD_(ULONG, AddRef)(THIS);
	STDMETHOD_(ULONG, Release)(THIS);
	STDMETHOD(QueryInterface)(THIS_ REFIID riid, void** ppvObj);

	/*** IDirect3DDevice9 methods ***/
	STDMETHOD(TestCooperativeLevel)(THIS);
	STDMETHOD_(UINT, GetAvailableTextureMem)(THIS);
	STDMETHOD(EvictManagedResources)(THIS);
	STDMETHOD(GetDirect3D)(THIS_ D3D9Wrapper::IDirect3D9** ppD3D9);
	STDMETHOD(GetDeviceCaps)(THIS_ ::D3DCAPS9* pCaps);
	STDMETHOD(GetDisplayMode)(THIS_ UINT iSwapChain, ::D3DDISPLAYMODE* pMode);
	STDMETHOD(GetCreationParameters)(THIS_ ::D3DDEVICE_CREATION_PARAMETERS *pParameters);
	STDMETHOD(SetCursorProperties)(THIS_ UINT XHotSpot, UINT YHotSpot, D3D9Wrapper::IDirect3DSurface9 *pCursorBitmap);
	STDMETHOD_(void, SetCursorPosition)(THIS_ int X, int Y, DWORD Flags);
	STDMETHOD_(BOOL, ShowCursor)(THIS_ BOOL bShow);
	STDMETHOD(CreateAdditionalSwapChain)(THIS_ ::D3DPRESENT_PARAMETERS* pPresentationParameters, D3D9Wrapper::IDirect3DSwapChain9** pSwapChain);
	STDMETHOD(GetSwapChain)(THIS_ UINT iSwapChain, D3D9Wrapper::IDirect3DSwapChain9** pSwapChain);
	STDMETHOD_(UINT, GetNumberOfSwapChains)(THIS);
	STDMETHOD(Reset)(THIS_ ::D3DPRESENT_PARAMETERS* pPresentationParameters);
	STDMETHOD(Present)(THIS_ CONST RECT* pSourceRect, CONST RECT* pDestRect, HWND hDestWindowOverride, CONST RGNDATA* pDirtyRegion);
	STDMETHOD(GetBackBuffer)(THIS_ UINT iSwapChain, UINT iBackBuffer, ::D3DBACKBUFFER_TYPE Type, D3D9Wrapper::IDirect3DSurface9 **ppBackBuffer);
	STDMETHOD(GetRasterStatus)(THIS_ UINT iSwapChain, ::D3DRASTER_STATUS* pRasterStatus);
	STDMETHOD(SetDialogBoxMode)(THIS_ BOOL bEnableDialogs);
	STDMETHOD_(void, SetGammaRamp)(THIS_ UINT iSwapChain, DWORD Flags, CONST ::D3DGAMMARAMP* pRamp);
	STDMETHOD_(void, GetGammaRamp)(THIS_ UINT iSwapChain, ::D3DGAMMARAMP* pRamp);
	STDMETHOD(CreateTexture)(THIS_ UINT Width, UINT Height, UINT Levels, DWORD Usage, ::D3DFORMAT Format, ::D3DPOOL Pool, D3D9Wrapper::IDirect3DTexture9** ppTexture, HANDLE* pSharedHandle);
	STDMETHOD(CreateVolumeTexture)(THIS_ UINT Width, UINT Height, UINT Depth, UINT Levels, DWORD Usage, ::D3DFORMAT Format, ::D3DPOOL Pool, D3D9Wrapper::IDirect3DVolumeTexture9** ppVolumeTexture, HANDLE* pSharedHandle);
	STDMETHOD(CreateCubeTexture)(THIS_ UINT EdgeLength, UINT Levels, DWORD Usage, ::D3DFORMAT Format, ::D3DPOOL Pool, D3D9Wrapper::IDirect3DCubeTexture9** ppCubeTexture, HANDLE* pSharedHandle);
	STDMETHOD(CreateVertexBuffer)(THIS_ UINT Length, DWORD Usage, DWORD FVF, ::D3DPOOL Pool, D3D9Wrapper::IDirect3DVertexBuffer9 **ppVertexBuffer, HANDLE* pSharedHandle);
	STDMETHOD(CreateIndexBuffer)(THIS_ UINT Length, DWORD Usage, ::D3DFORMAT Format, ::D3DPOOL Pool, D3D9Wrapper::IDirect3DIndexBuffer9 **ppIndexBuffer, HANDLE* pSharedHandle);
	STDMETHOD(CreateRenderTarget)(THIS_ UINT Width, UINT Height, ::D3DFORMAT Format, ::D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Lockable, D3D9Wrapper::IDirect3DSurface9 **ppSurface, HANDLE* pSharedHandle);
	STDMETHOD(CreateDepthStencilSurface)(THIS_ UINT Width, UINT Height, ::D3DFORMAT Format, ::D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Discard, D3D9Wrapper::IDirect3DSurface9 **ppSurface, HANDLE* pSharedHandle);
	STDMETHOD(UpdateSurface)(THIS_ D3D9Wrapper::IDirect3DSurface9 *pSourceSurface, CONST RECT* pSourceRect, D3D9Wrapper::IDirect3DSurface9 *pDestinationSurface, CONST POINT* pDestPoint);
	STDMETHOD(UpdateTexture)(THIS_ D3D9Wrapper::IDirect3DBaseTexture9 *pSourceTexture, D3D9Wrapper::IDirect3DBaseTexture9 *pDestinationTexture);
	STDMETHOD(GetRenderTargetData)(THIS_ D3D9Wrapper::IDirect3DSurface9 *pRenderTarget, D3D9Wrapper::IDirect3DSurface9 *pDestSurface);
	STDMETHOD(GetFrontBufferData)(THIS_ UINT iSwapChain, D3D9Wrapper::IDirect3DSurface9 *pDestSurface);
	STDMETHOD(StretchRect)(THIS_ D3D9Wrapper::IDirect3DSurface9 *pSourceSurface, CONST RECT* pSourceRect, D3D9Wrapper::IDirect3DSurface9 *pDestSurface, CONST RECT* pDestRect, ::D3DTEXTUREFILTERTYPE Filter);
	STDMETHOD(ColorFill)(THIS_ D3D9Wrapper::IDirect3DSurface9 *pSurface, CONST RECT* pRect, ::D3DCOLOR color);
	STDMETHOD(CreateOffscreenPlainSurface)(THIS_ UINT Width, UINT Height, ::D3DFORMAT Format, ::D3DPOOL Pool, D3D9Wrapper::IDirect3DSurface9 **ppSurface, HANDLE* pSharedHandle);
	STDMETHOD(SetRenderTarget)(THIS_ DWORD RenderTargetIndex, D3D9Wrapper::IDirect3DSurface9 *pRenderTarget);
	STDMETHOD(GetRenderTarget)(THIS_ DWORD RenderTargetIndex, D3D9Wrapper::IDirect3DSurface9 **ppRenderTarget);
	STDMETHOD(SetDepthStencilSurface)(THIS_ D3D9Wrapper::IDirect3DSurface9 *pNewZStencil);
	STDMETHOD(GetDepthStencilSurface)(THIS_ D3D9Wrapper::IDirect3DSurface9 **ppZStencilSurface);
	STDMETHOD(BeginScene)(THIS);
	STDMETHOD(EndScene)(THIS);
	STDMETHOD(Clear)(THIS_ DWORD Count, CONST ::D3DRECT* pRects, DWORD Flags, ::D3DCOLOR Color, float Z, DWORD Stencil);
	STDMETHOD(SetTransform)(THIS_ ::D3DTRANSFORMSTATETYPE State, CONST ::D3DMATRIX* pMatrix);
	STDMETHOD(GetTransform)(THIS_ ::D3DTRANSFORMSTATETYPE State, ::D3DMATRIX* pMatrix);
	STDMETHOD(MultiplyTransform)(THIS_ ::D3DTRANSFORMSTATETYPE a, CONST ::D3DMATRIX *b);
	STDMETHOD(SetViewport)(THIS_ CONST ::D3DVIEWPORT9* pViewport);
	STDMETHOD(GetViewport)(THIS_ ::D3DVIEWPORT9* pViewport);
	STDMETHOD(SetMaterial)(THIS_ CONST ::D3DMATERIAL9* pMaterial);
	STDMETHOD(GetMaterial)(THIS_ ::D3DMATERIAL9* pMaterial);
	STDMETHOD(SetLight)(THIS_ DWORD Index, CONST ::D3DLIGHT9 *Light);
	STDMETHOD(GetLight)(THIS_ DWORD Index, ::D3DLIGHT9*);
	STDMETHOD(LightEnable)(THIS_ DWORD Index, BOOL Enable);
	STDMETHOD(GetLightEnable)(THIS_ DWORD Index, BOOL* pEnable);
	STDMETHOD(SetClipPlane)(THIS_ DWORD Index, CONST float* pPlane);
	STDMETHOD(GetClipPlane)(THIS_ DWORD Index, float* pPlane);
	STDMETHOD(SetRenderState)(THIS_ ::D3DRENDERSTATETYPE State, DWORD Value);
	STDMETHOD(GetRenderState)(THIS_ ::D3DRENDERSTATETYPE State, DWORD* pValue);
	STDMETHOD(CreateStateBlock)(THIS_ ::D3DSTATEBLOCKTYPE Type, D3D9Wrapper::IDirect3DStateBlock9** ppSB);
	STDMETHOD(BeginStateBlock)(THIS);
	STDMETHOD(EndStateBlock)(THIS_ D3D9Wrapper::IDirect3DStateBlock9** ppSB);
	STDMETHOD(SetClipStatus)(THIS_ CONST ::D3DCLIPSTATUS9* pClipStatus);
	STDMETHOD(GetClipStatus)(THIS_ ::D3DCLIPSTATUS9* pClipStatus);
	STDMETHOD(GetTexture)(THIS_ DWORD Stage, D3D9Wrapper::IDirect3DBaseTexture9 **ppTexture);
	STDMETHOD(SetTexture)(THIS_ DWORD Stage, D3D9Wrapper::IDirect3DBaseTexture9 *pTexture);
	STDMETHOD(GetTextureStageState)(THIS_ DWORD Stage, ::D3DTEXTURESTAGESTATETYPE Type, DWORD* pValue);
	STDMETHOD(SetTextureStageState)(THIS_ DWORD Stage, ::D3DTEXTURESTAGESTATETYPE Type, DWORD Value);
	STDMETHOD(GetSamplerState)(THIS_ DWORD Sampler, ::D3DSAMPLERSTATETYPE Type, DWORD* pValue);
	STDMETHOD(SetSamplerState)(THIS_ DWORD Sampler, ::D3DSAMPLERSTATETYPE Type, DWORD Value);
	STDMETHOD(ValidateDevice)(THIS_ DWORD* pNumPasses);
	STDMETHOD(SetPaletteEntries)(THIS_ UINT PaletteNumber, CONST PALETTEENTRY* pEntries);
	STDMETHOD(GetPaletteEntries)(THIS_ UINT PaletteNumber, PALETTEENTRY* pEntries);
	STDMETHOD(SetCurrentTexturePalette)(THIS_ UINT PaletteNumber);
	STDMETHOD(GetCurrentTexturePalette)(THIS_ UINT *PaletteNumber);
	STDMETHOD(SetScissorRect)(THIS_ CONST RECT* pRect);
	STDMETHOD(GetScissorRect)(THIS_ RECT* pRect);
	STDMETHOD(SetSoftwareVertexProcessing)(THIS_ BOOL bSoftware);
	STDMETHOD_(BOOL, GetSoftwareVertexProcessing)(THIS);
	STDMETHOD(SetNPatchMode)(THIS_ float nSegments);
	STDMETHOD_(float, GetNPatchMode)(THIS);
	STDMETHOD(DrawPrimitive)(THIS_ ::D3DPRIMITIVETYPE PrimitiveType, UINT StartVertex, UINT PrimitiveCount);
	STDMETHOD(DrawIndexedPrimitive)(THIS_ ::D3DPRIMITIVETYPE, INT BaseVertexIndex, UINT MinVertexIndex, UINT NumVertices, UINT startIndex, UINT primCount);
	STDMETHOD(DrawPrimitiveUP)(THIS_ ::D3DPRIMITIVETYPE PrimitiveType, UINT PrimitiveCount, CONST void* pVertexStreamZeroData, UINT VertexStreamZeroStride);
	STDMETHOD(DrawIndexedPrimitiveUP)(THIS_ ::D3DPRIMITIVETYPE PrimitiveType, UINT MinVertexIndex, UINT NumVertices, UINT PrimitiveCount, CONST void* pIndexData, ::D3DFORMAT IndexDataFormat, CONST void* pVertexStreamZeroData, UINT VertexStreamZeroStride);
	STDMETHOD(ProcessVertices)(THIS_ UINT SrcStartIndex, UINT DestIndex, UINT VertexCount, D3D9Wrapper::IDirect3DVertexBuffer9 *pDestBuffer, D3D9Wrapper::IDirect3DVertexDeclaration9* pVertexDecl, DWORD Flags);
	STDMETHOD(CreateVertexDeclaration)(THIS_ CONST ::D3DVERTEXELEMENT9* pVertexElements, D3D9Wrapper::IDirect3DVertexDeclaration9** ppDecl);
	STDMETHOD(SetVertexDeclaration)(THIS_ D3D9Wrapper::IDirect3DVertexDeclaration9* pDecl);
	STDMETHOD(GetVertexDeclaration)(THIS_ D3D9Wrapper::IDirect3DVertexDeclaration9** ppDecl);
	STDMETHOD(SetFVF)(THIS_ DWORD FVF);
	STDMETHOD(GetFVF)(THIS_ DWORD* pFVF);
	STDMETHOD(CreateVertexShader)(THIS_ CONST DWORD* pFunction, D3D9Wrapper::IDirect3DVertexShader9 **ppShader);
	STDMETHOD(SetVertexShader)(THIS_ D3D9Wrapper::IDirect3DVertexShader9 *pShader);
	STDMETHOD(GetVertexShader)(THIS_ D3D9Wrapper::IDirect3DVertexShader9 **ppShader);
	STDMETHOD(SetVertexShaderConstantF)(THIS_ UINT StartRegister, CONST float* pConstantData, UINT Vector4fCount);
	STDMETHOD(GetVertexShaderConstantF)(THIS_ UINT StartRegister, float* pConstantData, UINT Vector4fCount);
	STDMETHOD(SetVertexShaderConstantI)(THIS_ UINT StartRegister, CONST int* pConstantData, UINT Vector4iCount);
	STDMETHOD(GetVertexShaderConstantI)(THIS_ UINT StartRegister, int* pConstantData, UINT Vector4iCount);
	STDMETHOD(SetVertexShaderConstantB)(THIS_ UINT StartRegister, CONST BOOL* pConstantData, UINT  BoolCount);
	STDMETHOD(GetVertexShaderConstantB)(THIS_ UINT StartRegister, BOOL* pConstantData, UINT BoolCount);
	STDMETHOD(SetStreamSource)(THIS_ UINT StreamNumber, D3D9Wrapper::IDirect3DVertexBuffer9 *pStreamData, UINT OffsetInBytes, UINT Stride);
	STDMETHOD(GetStreamSource)(THIS_ UINT StreamNumber, D3D9Wrapper::IDirect3DVertexBuffer9 **ppStreamData, UINT* pOffsetInBytes, UINT* pStride);
	STDMETHOD(SetStreamSourceFreq)(THIS_ UINT StreamNumber, UINT Setting);
	STDMETHOD(GetStreamSourceFreq)(THIS_ UINT StreamNumber, UINT* pSetting);
	STDMETHOD(SetIndices)(THIS_ D3D9Wrapper::IDirect3DIndexBuffer9 *pIndexData);
	STDMETHOD(GetIndices)(THIS_ D3D9Wrapper::IDirect3DIndexBuffer9 **ppIndexData);
	STDMETHOD(CreatePixelShader)(THIS_ CONST DWORD* pFunction, D3D9Wrapper::IDirect3DPixelShader9 **ppShader);
	STDMETHOD(SetPixelShader)(THIS_ D3D9Wrapper::IDirect3DPixelShader9 *pShader);
	STDMETHOD(GetPixelShader)(THIS_ D3D9Wrapper::IDirect3DPixelShader9 **ppShader);
	STDMETHOD(SetPixelShaderConstantF)(THIS_ UINT StartRegister, CONST float* pConstantData, UINT Vector4fCount);
	STDMETHOD(GetPixelShaderConstantF)(THIS_ UINT StartRegister, float* pConstantData, UINT Vector4fCount);
	STDMETHOD(SetPixelShaderConstantI)(THIS_ UINT StartRegister, CONST int* pConstantData, UINT Vector4iCount);
	STDMETHOD(GetPixelShaderConstantI)(THIS_ UINT StartRegister, int* pConstantData, UINT Vector4iCount);
	STDMETHOD(SetPixelShaderConstantB)(THIS_ UINT StartRegister, CONST BOOL* pConstantData, UINT  BoolCount);
	STDMETHOD(GetPixelShaderConstantB)(THIS_ UINT StartRegister, BOOL* pConstantData, UINT BoolCount);
	STDMETHOD(DrawRectPatch)(THIS_ UINT Handle, CONST float* pNumSegs, CONST ::D3DRECTPATCH_INFO* pRectPatchInfo);
	STDMETHOD(DrawTriPatch)(THIS_ UINT Handle, CONST float* pNumSegs, CONST ::D3DTRIPATCH_INFO* pTriPatchInfo);
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
};
