#pragma once

#include <unordered_map>

#include <d3d11_1.h>
#include <INITGUID.h>

#include "nvstereo.h"
#include "HackerContext.h"
#include "HackerDXGI.h"

// {83FFD841-A5C9-46F4-8109-BC259558FEF4}
DEFINE_GUID(IID_HackerDevice,
0x83ffd841, 0xa5c9, 0x46f4, 0x81, 0x9, 0xbc, 0x25, 0x95, 0x58, 0xfe, 0xf4);

// Hack to get around vertex limits in genshin buffers
// Apologies for the silly way of doing this, am currently under time pressure
// I will come back and do it properly once my time frees up
#ifndef GENSHIN_VERTEX
#define GENSHIN_VERTEX
extern std::unordered_set<UINT64> genshin_character_vb_draw_hashes;
#endif

// Forward declaration to allow circular reference between HackerContext and HackerDevice. 
// We need this to allow each to reference the other as needed.

class HackerContext;
class HackerSwapChain;


// 1-6-18:  Current approach will be to only create one level of wrapping,
// specifically HackerDevice and HackerContext, based on the ID3D11Device1,
// and ID3D11DeviceContext1.  ID3D11Device1/ID3D11DeviceContext1 is supported
// on Win7+platform_update, and thus is a superset of what we need.  By
// using the highest level object supported, we can kill off a lot of conditional
// code that just complicates things. 
//
// The ID3D11Device1 will be supported on all OS except Win7 minus the 
// platform_update.  In that scenario, we will save a reference to the 
// ID3D11Device object instead, but store it and wrap it in HackerDevice.
// 
// Specifically decided to not name everything *1, because frankly that is 
// was an awful choice on Microsoft's part to begin with.  Meaningless number
// completely unrelated to version/revision or functionality.  Bad.
// We will use the *1 notation for object names that are specific types,
// like the mOrigDevice1 to avoid misleading types.
//
// Any HackerDevice will be the superset object ID3D11Device1 in all cases
// except for Win7 missing the evil platform_update.

// Hierarchy:
//  HackerDevice <- ID3D11Device1 <- ID3D11Device <- IUnknown

class HackerDevice : public ID3D11Device1
{
private:
	ID3D11Device1 *mOrigDevice1;
	ID3D11Device1 *mRealOrigDevice1;
	ID3D11DeviceContext1 *mOrigContext1;
	IUnknown *mUnknown;

	HackerContext *mHackerContext;
	HackerSwapChain *mHackerSwapChain;

	// Utility routines
	char *_ReplaceShaderFromShaderFixes(UINT64 hash, const wchar_t *shaderType, const void *pShaderBytecode,
		SIZE_T BytecodeLength, SIZE_T &pCodeSize, string &foundShaderModel, FILETIME &timeStamp,
		wstring &headerLine, const char *overrideShaderModel);

	template <class ID3D11Shader,
		 HRESULT (__stdcall ID3D11Device::*OrigCreateShader)(THIS_
				 __in const void *pShaderBytecode,
				 __in SIZE_T BytecodeLength,
				 __in_opt ID3D11ClassLinkage *pClassLinkage,
				 __out_opt ID3D11Shader **ppShader)
		 >
	HRESULT ReplaceShaderFromShaderFixes(UINT64 hash, const void *pShaderBytecode, SIZE_T BytecodeLength,
		ID3D11ClassLinkage *pClassLinkage, ID3D11Shader **ppShader, wchar_t *shaderType);

	template <class ID3D11Shader,
		 HRESULT (__stdcall ID3D11Device::*OrigCreateShader)(THIS_
				 __in const void *pShaderBytecode,
				 __in SIZE_T BytecodeLength,
				 __in_opt ID3D11ClassLinkage *pClassLinkage,
				 __out_opt ID3D11Shader **ppShader)
		 >
	HRESULT ProcessShaderNotFoundInShaderFixes(UINT64 hash, const void *pShaderBytecode, SIZE_T BytecodeLength,
			ID3D11ClassLinkage *pClassLinkage, ID3D11Shader **ppShader, wchar_t *shaderType);

	bool NeedOriginalShader(UINT64 hash);

	template <class ID3D11Shader,
		 HRESULT (__stdcall ID3D11Device::*OrigCreateShader)(THIS_
				 __in const void *pShaderBytecode,
				 __in SIZE_T BytecodeLength,
				 __in_opt ID3D11ClassLinkage *pClassLinkage,
				 __out_opt ID3D11Shader **ppShader)
			 >
	void KeepOriginalShader(UINT64 hash, wchar_t *shaderType, ID3D11Shader *pShader,
		const void *pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage *pClassLinkage);

	HRESULT CreateStereoParamResources();
	void CreatePinkHuntingResources();
	HRESULT SetGlobalNVSurfaceCreationMode();

	// Templates of nearly identical functions
	template <class ID3D11Shader,
		 HRESULT (__stdcall ID3D11Device::*OrigCreateShader)(THIS_
				 __in const void *pShaderBytecode,
				 __in SIZE_T BytecodeLength,
				 __in_opt ID3D11ClassLinkage *pClassLinkage,
				 __out_opt ID3D11Shader **ppShader)
		 >
	STDMETHODIMP CreateShader(THIS_
		/* [annotation] */
		__in  const void *pShaderBytecode,
		/* [annotation] */
		__in  SIZE_T BytecodeLength,
		/* [annotation] */
		__in_opt  ID3D11ClassLinkage *pClassLinkage,
		/* [annotation] */
		__out_opt  ID3D11Shader **ppShader,
		wchar_t *shaderType);

public:
	StereoHandle mStereoHandle;
	nv::stereo::ParamTextureManagerD3D11 mParamTextureManager;
	ID3D11Texture2D *mStereoTexture;
	ID3D11ShaderResourceView *mStereoResourceView;
	ID3D11ShaderResourceView *mZBufferResourceView;
	ID3D11Texture1D *mIniTexture;
	ID3D11ShaderResourceView *mIniResourceView;

	HackerDevice(ID3D11Device1 *pDevice1, ID3D11DeviceContext1 *pContext1);

	HRESULT CreateIniParamResources();
	void Create3DMigotoResources();
	void SetHackerContext(HackerContext *pHackerContext);
	void SetHackerSwapChain(HackerSwapChain *pHackerSwapChain);

	HackerContext* GetHackerContext();
	HackerSwapChain* GetHackerSwapChain();
	ID3D11Device1* GetPossiblyHookedOrigDevice1();
	ID3D11Device1* GetPassThroughOrigDevice1();
	ID3D11DeviceContext1* GetPossiblyHookedOrigContext1();
	ID3D11DeviceContext1* GetPassThroughOrigContext1();
	IUnknown* GetIUnknown();
	void HookDevice();


	/*** IUnknown methods ***/

	HRESULT STDMETHODCALLTYPE QueryInterface(
		/* [in] */ REFIID riid,
		/* [iid_is][out] */ _COM_Outptr_ void __RPC_FAR *__RPC_FAR *ppvObject);

	ULONG STDMETHODCALLTYPE AddRef(void);

	ULONG STDMETHODCALLTYPE Release(void);


	/*** ID3D11Device methods ***/

	HRESULT STDMETHODCALLTYPE CreateBuffer(
		/* [annotation] */
		_In_  const D3D11_BUFFER_DESC *pDesc,
		/* [annotation] */
		_In_opt_  const D3D11_SUBRESOURCE_DATA *pInitialData,
		/* [annotation] */
		_Out_opt_  ID3D11Buffer **ppBuffer);

	HRESULT STDMETHODCALLTYPE CreateTexture1D(
		/* [annotation] */
		_In_  const D3D11_TEXTURE1D_DESC *pDesc,
		/* [annotation] */
		_In_reads_opt_(_Inexpressible_(pDesc->MipLevels * pDesc->ArraySize))  const D3D11_SUBRESOURCE_DATA *pInitialData,
		/* [annotation] */
		_Out_opt_  ID3D11Texture1D **ppTexture1D);

	HRESULT STDMETHODCALLTYPE CreateTexture2D(
		/* [annotation] */
		_In_  const D3D11_TEXTURE2D_DESC *pDesc,
		/* [annotation] */
		_In_reads_opt_(_Inexpressible_(pDesc->MipLevels * pDesc->ArraySize))  const D3D11_SUBRESOURCE_DATA *pInitialData,
		/* [annotation] */
		_Out_opt_  ID3D11Texture2D **ppTexture2D);

	HRESULT STDMETHODCALLTYPE CreateTexture3D(
		/* [annotation] */
		_In_  const D3D11_TEXTURE3D_DESC *pDesc,
		/* [annotation] */
		_In_reads_opt_(_Inexpressible_(pDesc->MipLevels))  const D3D11_SUBRESOURCE_DATA *pInitialData,
		/* [annotation] */
		_Out_opt_  ID3D11Texture3D **ppTexture3D);

	HRESULT STDMETHODCALLTYPE CreateShaderResourceView(
		/* [annotation] */
		_In_  ID3D11Resource *pResource,
		/* [annotation] */
		_In_opt_  const D3D11_SHADER_RESOURCE_VIEW_DESC *pDesc,
		/* [annotation] */
		_Out_opt_  ID3D11ShaderResourceView **ppSRView);

	HRESULT STDMETHODCALLTYPE CreateUnorderedAccessView(
		/* [annotation] */
		_In_  ID3D11Resource *pResource,
		/* [annotation] */
		_In_opt_  const D3D11_UNORDERED_ACCESS_VIEW_DESC *pDesc,
		/* [annotation] */
		_Out_opt_  ID3D11UnorderedAccessView **ppUAView);

	HRESULT STDMETHODCALLTYPE CreateRenderTargetView(
		/* [annotation] */
		_In_  ID3D11Resource *pResource,
		/* [annotation] */
		_In_opt_  const D3D11_RENDER_TARGET_VIEW_DESC *pDesc,
		/* [annotation] */
		_Out_opt_  ID3D11RenderTargetView **ppRTView);

	HRESULT STDMETHODCALLTYPE CreateDepthStencilView(
		/* [annotation] */
		_In_  ID3D11Resource *pResource,
		/* [annotation] */
		_In_opt_  const D3D11_DEPTH_STENCIL_VIEW_DESC *pDesc,
		/* [annotation] */
		_Out_opt_  ID3D11DepthStencilView **ppDepthStencilView);

	HRESULT STDMETHODCALLTYPE CreateInputLayout(
		/* [annotation] */
		_In_reads_(NumElements)  const D3D11_INPUT_ELEMENT_DESC *pInputElementDescs,
		/* [annotation] */
		_In_range_(0, D3D11_IA_VERTEX_INPUT_STRUCTURE_ELEMENT_COUNT)  UINT NumElements,
		/* [annotation] */
		_In_  const void *pShaderBytecodeWithInputSignature,
		/* [annotation] */
		_In_  SIZE_T BytecodeLength,
		/* [annotation] */
		_Out_opt_  ID3D11InputLayout **ppInputLayout);

	HRESULT STDMETHODCALLTYPE CreateVertexShader(
		/* [annotation] */
		_In_  const void *pShaderBytecode,
		/* [annotation] */
		_In_  SIZE_T BytecodeLength,
		/* [annotation] */
		_In_opt_  ID3D11ClassLinkage *pClassLinkage,
		/* [annotation] */
		_Out_opt_  ID3D11VertexShader **ppVertexShader);

	HRESULT STDMETHODCALLTYPE CreateGeometryShader(
		/* [annotation] */
		_In_  const void *pShaderBytecode,
		/* [annotation] */
		_In_  SIZE_T BytecodeLength,
		/* [annotation] */
		_In_opt_  ID3D11ClassLinkage *pClassLinkage,
		/* [annotation] */
		_Out_opt_  ID3D11GeometryShader **ppGeometryShader);

	HRESULT STDMETHODCALLTYPE CreateGeometryShaderWithStreamOutput(
		/* [annotation] */
		_In_  const void *pShaderBytecode,
		/* [annotation] */
		_In_  SIZE_T BytecodeLength,
		/* [annotation] */
		_In_reads_opt_(NumEntries)  const D3D11_SO_DECLARATION_ENTRY *pSODeclaration,
		/* [annotation] */
		_In_range_(0, D3D11_SO_STREAM_COUNT * D3D11_SO_OUTPUT_COMPONENT_COUNT)  UINT NumEntries,
		/* [annotation] */
		_In_reads_opt_(NumStrides)  const UINT *pBufferStrides,
		/* [annotation] */
		_In_range_(0, D3D11_SO_BUFFER_SLOT_COUNT)  UINT NumStrides,
		/* [annotation] */
		_In_  UINT RasterizedStream,
		/* [annotation] */
		_In_opt_  ID3D11ClassLinkage *pClassLinkage,
		/* [annotation] */
		_Out_opt_  ID3D11GeometryShader **ppGeometryShader);

	HRESULT STDMETHODCALLTYPE CreatePixelShader(
		/* [annotation] */
		_In_  const void *pShaderBytecode,
		/* [annotation] */
		_In_  SIZE_T BytecodeLength,
		/* [annotation] */
		_In_opt_  ID3D11ClassLinkage *pClassLinkage,
		/* [annotation] */
		_Out_opt_  ID3D11PixelShader **ppPixelShader);

	HRESULT STDMETHODCALLTYPE CreateHullShader(
		/* [annotation] */
		_In_  const void *pShaderBytecode,
		/* [annotation] */
		_In_  SIZE_T BytecodeLength,
		/* [annotation] */
		_In_opt_  ID3D11ClassLinkage *pClassLinkage,
		/* [annotation] */
		_Out_opt_  ID3D11HullShader **ppHullShader);

	HRESULT STDMETHODCALLTYPE CreateDomainShader(
		/* [annotation] */
		_In_  const void *pShaderBytecode,
		/* [annotation] */
		_In_  SIZE_T BytecodeLength,
		/* [annotation] */
		_In_opt_  ID3D11ClassLinkage *pClassLinkage,
		/* [annotation] */
		_Out_opt_  ID3D11DomainShader **ppDomainShader);

	HRESULT STDMETHODCALLTYPE CreateComputeShader(
		/* [annotation] */
		_In_  const void *pShaderBytecode,
		/* [annotation] */
		_In_  SIZE_T BytecodeLength,
		/* [annotation] */
		_In_opt_  ID3D11ClassLinkage *pClassLinkage,
		/* [annotation] */
		_Out_opt_  ID3D11ComputeShader **ppComputeShader);

	HRESULT STDMETHODCALLTYPE CreateClassLinkage(
		/* [annotation] */
		_Out_  ID3D11ClassLinkage **ppLinkage);

	HRESULT STDMETHODCALLTYPE CreateBlendState(
		/* [annotation] */
		_In_  const D3D11_BLEND_DESC *pBlendStateDesc,
		/* [annotation] */
		_Out_opt_  ID3D11BlendState **ppBlendState);

	HRESULT STDMETHODCALLTYPE CreateDepthStencilState(
		/* [annotation] */
		_In_  const D3D11_DEPTH_STENCIL_DESC *pDepthStencilDesc,
		/* [annotation] */
		_Out_opt_  ID3D11DepthStencilState **ppDepthStencilState);

	HRESULT STDMETHODCALLTYPE CreateRasterizerState(
		/* [annotation] */
		_In_  const D3D11_RASTERIZER_DESC *pRasterizerDesc,
		/* [annotation] */
		_Out_opt_  ID3D11RasterizerState **ppRasterizerState);

	HRESULT STDMETHODCALLTYPE CreateSamplerState(
		/* [annotation] */
		_In_  const D3D11_SAMPLER_DESC *pSamplerDesc,
		/* [annotation] */
		_Out_opt_  ID3D11SamplerState **ppSamplerState);

	HRESULT STDMETHODCALLTYPE CreateQuery(
		/* [annotation] */
		_In_  const D3D11_QUERY_DESC *pQueryDesc,
		/* [annotation] */
		_Out_opt_  ID3D11Query **ppQuery);

	HRESULT STDMETHODCALLTYPE CreatePredicate(
		/* [annotation] */
		_In_  const D3D11_QUERY_DESC *pPredicateDesc,
		/* [annotation] */
		_Out_opt_  ID3D11Predicate **ppPredicate);

	HRESULT STDMETHODCALLTYPE CreateCounter(
		/* [annotation] */
		_In_  const D3D11_COUNTER_DESC *pCounterDesc,
		/* [annotation] */
		_Out_opt_  ID3D11Counter **ppCounter);

	HRESULT STDMETHODCALLTYPE CreateDeferredContext(
		UINT ContextFlags,
		/* [annotation] */
		_Out_opt_  ID3D11DeviceContext **ppDeferredContext);

	HRESULT STDMETHODCALLTYPE OpenSharedResource(
		/* [annotation] */
		_In_  HANDLE hResource,
		/* [annotation] */
		_In_  REFIID ReturnedInterface,
		/* [annotation] */
		_Out_opt_  void **ppResource);

	HRESULT STDMETHODCALLTYPE CheckFormatSupport(
		/* [annotation] */
		_In_  DXGI_FORMAT Format,
		/* [annotation] */
		_Out_  UINT *pFormatSupport);

	HRESULT STDMETHODCALLTYPE CheckMultisampleQualityLevels(
		/* [annotation] */
		_In_  DXGI_FORMAT Format,
		/* [annotation] */
		_In_  UINT SampleCount,
		/* [annotation] */
		_Out_  UINT *pNumQualityLevels);

	void STDMETHODCALLTYPE CheckCounterInfo(
		/* [annotation] */
		_Out_  D3D11_COUNTER_INFO *pCounterInfo);

	HRESULT STDMETHODCALLTYPE CheckCounter(
		/* [annotation] */
		_In_  const D3D11_COUNTER_DESC *pDesc,
		/* [annotation] */
		_Out_  D3D11_COUNTER_TYPE *pType,
		/* [annotation] */
		_Out_  UINT *pActiveCounters,
		/* [annotation] */
		_Out_writes_opt_(*pNameLength)  LPSTR szName,
		/* [annotation] */
		_Inout_opt_  UINT *pNameLength,
		/* [annotation] */
		_Out_writes_opt_(*pUnitsLength)  LPSTR szUnits,
		/* [annotation] */
		_Inout_opt_  UINT *pUnitsLength,
		/* [annotation] */
		_Out_writes_opt_(*pDescriptionLength)  LPSTR szDescription,
		/* [annotation] */
		_Inout_opt_  UINT *pDescriptionLength);

	HRESULT STDMETHODCALLTYPE CheckFeatureSupport(
		D3D11_FEATURE Feature,
		/* [annotation] */
		_Out_writes_bytes_(FeatureSupportDataSize)  void *pFeatureSupportData,
		UINT FeatureSupportDataSize);

	HRESULT STDMETHODCALLTYPE GetPrivateData(
		/* [annotation] */
		_In_  REFGUID guid,
		/* [annotation] */
		_Inout_  UINT *pDataSize,
		/* [annotation] */
		_Out_writes_bytes_opt_(*pDataSize)  void *pData);

	HRESULT STDMETHODCALLTYPE SetPrivateData(
		/* [annotation] */
		_In_  REFGUID guid,
		/* [annotation] */
		_In_  UINT DataSize,
		/* [annotation] */
		_In_reads_bytes_opt_(DataSize)  const void *pData);

	HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(
		/* [annotation] */
		_In_  REFGUID guid,
		/* [annotation] */
		_In_opt_  const IUnknown *pData);

	D3D_FEATURE_LEVEL STDMETHODCALLTYPE GetFeatureLevel(void);

	UINT STDMETHODCALLTYPE GetCreationFlags(void);

	HRESULT STDMETHODCALLTYPE GetDeviceRemovedReason(void);

	void STDMETHODCALLTYPE GetImmediateContext(
		/* [annotation] */
		_Out_  ID3D11DeviceContext **ppImmediateContext);

	HRESULT STDMETHODCALLTYPE SetExceptionMode(
		UINT RaiseFlags);

	UINT STDMETHODCALLTYPE GetExceptionMode(void);


	/*** ID3D11Device1 methods ***/

	void STDMETHODCALLTYPE GetImmediateContext1(
		/* [annotation] */
		_Out_  ID3D11DeviceContext1 **ppImmediateContext);

	HRESULT STDMETHODCALLTYPE CreateDeferredContext1(
		UINT ContextFlags,
		/* [annotation] */
		_Out_opt_  ID3D11DeviceContext1 **ppDeferredContext);

	HRESULT STDMETHODCALLTYPE CreateBlendState1(
		/* [annotation] */
		_In_  const D3D11_BLEND_DESC1 *pBlendStateDesc,
		/* [annotation] */
		_Out_opt_  ID3D11BlendState1 **ppBlendState);

	HRESULT STDMETHODCALLTYPE CreateRasterizerState1(
		/* [annotation] */
		_In_  const D3D11_RASTERIZER_DESC1 *pRasterizerDesc,
		/* [annotation] */
		_Out_opt_  ID3D11RasterizerState1 **ppRasterizerState);

	HRESULT STDMETHODCALLTYPE CreateDeviceContextState(
		UINT Flags,
		/* [annotation] */
		_In_reads_(FeatureLevels)  const D3D_FEATURE_LEVEL *pFeatureLevels,
		UINT FeatureLevels,
		UINT SDKVersion,
		REFIID EmulatedInterface,
		/* [annotation] */
		_Out_opt_  D3D_FEATURE_LEVEL *pChosenFeatureLevel,
		/* [annotation] */
		_Out_opt_  ID3DDeviceContextState **ppContextState);

	HRESULT STDMETHODCALLTYPE OpenSharedResource1(
		/* [annotation] */
		_In_  HANDLE hResource,
		/* [annotation] */
		_In_  REFIID returnedInterface,
		/* [annotation] */
		_Out_  void **ppResource);

	HRESULT STDMETHODCALLTYPE OpenSharedResourceByName(
		/* [annotation] */
		_In_  LPCWSTR lpName,
		/* [annotation] */
		_In_  DWORD dwDesiredAccess,
		/* [annotation] */
		_In_  REFIID returnedInterface,
		/* [annotation] */
		_Out_  void **ppResource); 
};

HackerDevice* lookup_hacker_device(IUnknown *unknown);
