#pragma once

#include <unordered_map>

#include <d3d11_1.h>

#include "nvstereo.h"
#include "HackerContext.h"
#include "HackerDXGI.h"


// Forward declaration to allow circular reference between HackerContext and HackerDevice. 
// We need this to allow each to reference the other as needed.

class HackerContext;
class HackerContext1;
class HackerDXGISwapChain;
class HackerDXGIDevice1;

class HackerDevice : public ID3D11Device
{
private:
	ID3D11Device *mOrigDevice;
	ID3D11Device *mRealOrigDevice;
	ID3D11DeviceContext *mOrigContext;

	HackerDXGISwapChain *mHackerSwapChain;
	HackerContext *mHackerContext;
	HackerDXGIDevice1 *mHackerDXGIDevice1;

	// Utility routines
	void RegisterForReload(ID3D11DeviceChild* ppShader, UINT64 hash, wstring shaderType, string shaderModel,
		ID3D11ClassLinkage* pClassLinkage, ID3DBlob* byteCode, FILETIME timeStamp, wstring text);
	char *ReplaceShader(UINT64 hash, const wchar_t *shaderType, const void *pShaderBytecode,
		SIZE_T BytecodeLength, SIZE_T &pCodeSize, string &foundShaderModel, FILETIME &timeStamp, 
		void **zeroShader, wstring &headerLine, const char *overrideShaderModel);
	bool NeedOriginalShader(UINT64 hash);
	void KeepOriginalShader(UINT64 hash, wchar_t *shaderType, ID3D11DeviceChild *pShader,
		const void *pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage *pClassLinkage);
	HRESULT CreateStereoParamResources();
	HRESULT CreateIniParamResources();
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
		wchar_t *shaderType,
		std::unordered_map<ID3D11Shader *, UINT64> *shaders,
		std::unordered_map<ID3D11Shader *, ID3D11Shader *> *originalShaders,
		std::unordered_map<ID3D11Shader *, ID3D11Shader *> *zeroShaders
		);

public:
	//static ThreadSafePointerSet	 m_List; ToDo: These should all be private.
	StereoHandle mStereoHandle;
	nv::stereo::ParamTextureManagerD3D11 mParamTextureManager;
	ID3D11Texture2D *mStereoTexture;
	ID3D11ShaderResourceView *mStereoResourceView;
	ID3D11ShaderResourceView *mZBufferResourceView;
	ID3D11Texture1D *mIniTexture;
	ID3D11ShaderResourceView *mIniResourceView;

	HackerDevice(ID3D11Device *pDevice, ID3D11DeviceContext *pContext);

	void Create3DMigotoResources();
	void SetHackerContext(HackerContext *pHackerContext);
	void SetHackerSwapChain(HackerDXGISwapChain *pHackerSwapChain);

	HackerContext* GetHackerContext();
	ID3D11Device* GetOrigDevice();
	ID3D11DeviceContext* GetOrigContext();
	IDXGISwapChain* GetOrigSwapChain();
	void HackerDevice::HookDevice();


	//static ID3D11Device* GetDirect3DDevice(ID3D11Device *pDevice);
	//static __forceinline ID3D11Device *GetD3D11Device() { return (ID3D11Device*) m_pUnk; }
	// GetD3D11Device is just using super

	HRESULT STDMETHODCALLTYPE QueryInterface(
		/* [in] */ REFIID riid,
		/* [iid_is][out] */ _COM_Outptr_ void __RPC_FAR *__RPC_FAR *ppvObject);

	ULONG STDMETHODCALLTYPE AddRef(void);

	ULONG STDMETHODCALLTYPE Release(void);

	/*** IDirect3DUnknown methods ***/
	//STDMETHOD_(ULONG, AddRef)(THIS);
	//STDMETHOD_(ULONG, Release)(THIS);

	/*** ID3D11Device methods ***/
	STDMETHOD(CreateBuffer)(THIS_
		/* [annotation] */
		__in  const D3D11_BUFFER_DESC *pDesc,
		/* [annotation] */
		__in_opt  const D3D11_SUBRESOURCE_DATA *pInitialData,
		/* [annotation] */
		__out_opt  ID3D11Buffer **ppBuffer);

	STDMETHOD(CreateTexture1D)(THIS_
		/* [annotation] */
		__in  const D3D11_TEXTURE1D_DESC *pDesc,
		/* [annotation] */
		__in_xcount_opt(pDesc->MipLevels * pDesc->ArraySize)  const D3D11_SUBRESOURCE_DATA *pInitialData,
		/* [annotation] */
		__out_opt  ID3D11Texture1D **ppTexture1D);

	STDMETHOD(CreateTexture2D)(THIS_
		/* [annotation] */
		__in  const D3D11_TEXTURE2D_DESC *pDesc,
		/* [annotation] */
		__in_xcount_opt(pDesc->MipLevels * pDesc->ArraySize)  const D3D11_SUBRESOURCE_DATA *pInitialData,
		/* [annotation] */
		__out_opt  ID3D11Texture2D **ppTexture2D);

	STDMETHOD(CreateTexture3D)(THIS_
		/* [annotation] */
		__in  const D3D11_TEXTURE3D_DESC *pDesc,
		/* [annotation] */
		__in_xcount_opt(pDesc->MipLevels)  const D3D11_SUBRESOURCE_DATA *pInitialData,
		/* [annotation] */
		__out_opt  ID3D11Texture3D **ppTexture3D);

	STDMETHOD(CreateShaderResourceView)(THIS_
		/* [annotation] */
		__in  ID3D11Resource *pResource,
		/* [annotation] */
		__in_opt  const D3D11_SHADER_RESOURCE_VIEW_DESC *pDesc,
		/* [annotation] */
		__out_opt  ID3D11ShaderResourceView **ppSRView);

	STDMETHOD(CreateUnorderedAccessView)(THIS_
		/* [annotation] */
		__in  ID3D11Resource *pResource,
		/* [annotation] */
		__in_opt  const D3D11_UNORDERED_ACCESS_VIEW_DESC *pDesc,
		/* [annotation] */
		__out_opt  ID3D11UnorderedAccessView **ppUAView);

	STDMETHOD(CreateRenderTargetView)(THIS_
		/* [annotation] */
		__in  ID3D11Resource *pResource,
		/* [annotation] */
		__in_opt  const D3D11_RENDER_TARGET_VIEW_DESC *pDesc,
		/* [annotation] */
		__out_opt  ID3D11RenderTargetView **ppRTView);

	STDMETHOD(CreateDepthStencilView)(THIS_
		/* [annotation] */
		__in  ID3D11Resource *pResource,
		/* [annotation] */
		__in_opt  const D3D11_DEPTH_STENCIL_VIEW_DESC *pDesc,
		/* [annotation] */
		__out_opt  ID3D11DepthStencilView **ppDepthStencilView);

	STDMETHOD(CreateInputLayout)(THIS_
		/* [annotation] */
		__in_ecount(NumElements)  const D3D11_INPUT_ELEMENT_DESC *pInputElementDescs,
		/* [annotation] */
		__in_range(0, D3D11_IA_VERTEX_INPUT_STRUCTURE_ELEMENT_COUNT)  UINT NumElements,
		/* [annotation] */
		__in  const void *pShaderBytecodeWithInputSignature,
		/* [annotation] */
		__in  SIZE_T BytecodeLength,
		/* [annotation] */
		__out_opt  ID3D11InputLayout **ppInputLayout);

	STDMETHOD(CreateVertexShader)(THIS_
		/* [annotation] */
		__in  const void *pShaderBytecode,
		/* [annotation] */
		__in  SIZE_T BytecodeLength,
		/* [annotation] */
		__in_opt  ID3D11ClassLinkage *pClassLinkage,
		/* [annotation] */
		__out_opt  ID3D11VertexShader **ppVertexShader);

	STDMETHOD(CreateGeometryShader)(THIS_
		/* [annotation] */
		__in  const void *pShaderBytecode,
		/* [annotation] */
		__in  SIZE_T BytecodeLength,
		/* [annotation] */
		__in_opt  ID3D11ClassLinkage *pClassLinkage,
		/* [annotation] */
		__out_opt  ID3D11GeometryShader **ppGeometryShader);

	STDMETHOD(CreateGeometryShaderWithStreamOutput)(THIS_
		/* [annotation] */
		__in  const void *pShaderBytecode,
		/* [annotation] */
		__in  SIZE_T BytecodeLength,
		/* [annotation] */
		__in_ecount_opt(NumEntries)  const D3D11_SO_DECLARATION_ENTRY *pSODeclaration,
		/* [annotation] */
		__in_range(0, D3D11_SO_STREAM_COUNT * D3D11_SO_OUTPUT_COMPONENT_COUNT)  UINT NumEntries,
		/* [annotation] */
		__in_ecount_opt(NumStrides)  const UINT *pBufferStrides,
		/* [annotation] */
		__in_range(0, D3D11_SO_BUFFER_SLOT_COUNT)  UINT NumStrides,
		/* [annotation] */
		__in  UINT RasterizedStream,
		/* [annotation] */
		__in_opt  ID3D11ClassLinkage *pClassLinkage,
		/* [annotation] */
		__out_opt  ID3D11GeometryShader **ppGeometryShader);

	STDMETHOD(CreatePixelShader)(THIS_
		/* [annotation] */
		__in  const void *pShaderBytecode,
		/* [annotation] */
		__in  SIZE_T BytecodeLength,
		/* [annotation] */
		__in_opt  ID3D11ClassLinkage *pClassLinkage,
		/* [annotation] */
		__out_opt  ID3D11PixelShader **ppPixelShader);

	STDMETHOD(CreateHullShader)(THIS_
		/* [annotation] */
		__in  const void *pShaderBytecode,
		/* [annotation] */
		__in  SIZE_T BytecodeLength,
		/* [annotation] */
		__in_opt  ID3D11ClassLinkage *pClassLinkage,
		/* [annotation] */
		__out_opt  ID3D11HullShader **ppHullShader);

	STDMETHOD(CreateDomainShader)(THIS_
		/* [annotation] */
		__in  const void *pShaderBytecode,
		/* [annotation] */
		__in  SIZE_T BytecodeLength,
		/* [annotation] */
		__in_opt  ID3D11ClassLinkage *pClassLinkage,
		/* [annotation] */
		__out_opt  ID3D11DomainShader **ppDomainShader);

	STDMETHOD(CreateComputeShader)(THIS_
		/* [annotation] */
		__in  const void *pShaderBytecode,
		/* [annotation] */
		__in  SIZE_T BytecodeLength,
		/* [annotation] */
		__in_opt  ID3D11ClassLinkage *pClassLinkage,
		/* [annotation] */
		__out_opt  ID3D11ComputeShader **ppComputeShader);

	STDMETHOD(CreateClassLinkage)(THIS_
		/* [annotation] */
		__out  ID3D11ClassLinkage **ppLinkage);

	STDMETHOD(CreateBlendState)(THIS_
		/* [annotation] */
		__in  const D3D11_BLEND_DESC *pBlendStateDesc,
		/* [annotation] */
		__out_opt  ID3D11BlendState **ppBlendState);

	STDMETHOD(CreateDepthStencilState)(THIS_
		/* [annotation] */
		__in  const D3D11_DEPTH_STENCIL_DESC *pDepthStencilDesc,
		/* [annotation] */
		__out_opt  ID3D11DepthStencilState **ppDepthStencilState);

	STDMETHOD(CreateRasterizerState)(THIS_
		/* [annotation] */
		__in  const D3D11_RASTERIZER_DESC *pRasterizerDesc,
		/* [annotation] */
		__out_opt  ID3D11RasterizerState **ppRasterizerState);

	STDMETHOD(CreateSamplerState)(THIS_
		/* [annotation] */
		__in  const D3D11_SAMPLER_DESC *pSamplerDesc,
		/* [annotation] */
		__out_opt  ID3D11SamplerState **ppSamplerState);

	STDMETHOD(CreateQuery)(THIS_
		/* [annotation] */
		__in  const D3D11_QUERY_DESC *pQueryDesc,
		/* [annotation] */
		__out_opt  ID3D11Query **ppQuery);

	STDMETHOD(CreatePredicate)(THIS_
		/* [annotation] */
		__in  const D3D11_QUERY_DESC *pPredicateDesc,
		/* [annotation] */
		__out_opt  ID3D11Predicate **ppPredicate);

	STDMETHOD(CreateCounter)(THIS_
		/* [annotation] */
		__in  const D3D11_COUNTER_DESC *pCounterDesc,
		/* [annotation] */
		__out_opt  ID3D11Counter **ppCounter);

	STDMETHOD(CreateDeferredContext)(THIS_
		UINT ContextFlags,
		/* [annotation] */
		__out_opt ID3D11DeviceContext **ppDeferredContext);

	STDMETHOD(OpenSharedResource)(THIS_
		/* [annotation] */
		__in  HANDLE hResource,
		/* [annotation] */
		__in  REFIID ReturnedInterface,
		/* [annotation] */
		__out_opt  void **ppResource);

	STDMETHOD(CheckFormatSupport)(THIS_
		/* [annotation] */
		__in  DXGI_FORMAT Format,
		/* [annotation] */
		__out  UINT *pFormatSupport);

	STDMETHOD(CheckMultisampleQualityLevels)(THIS_
		/* [annotation] */
		__in  DXGI_FORMAT Format,
		/* [annotation] */
		__in  UINT SampleCount,
		/* [annotation] */
		__out  UINT *pNumQualityLevels);

	STDMETHOD_(void, CheckCounterInfo)(THIS_
		/* [annotation] */
		__out  D3D11_COUNTER_INFO *pCounterInfo);

	STDMETHOD(CheckCounter)(THIS_
		/* [annotation] */
		__in  const D3D11_COUNTER_DESC *pDesc,
		/* [annotation] */
		__out  D3D11_COUNTER_TYPE *pType,
		/* [annotation] */
		__out  UINT *pActiveCounters,
		/* [annotation] */
		__out_ecount_opt(*pNameLength)  LPSTR szName,
		/* [annotation] */
		__inout_opt  UINT *pNameLength,
		/* [annotation] */
		__out_ecount_opt(*pUnitsLength)  LPSTR szUnits,
		/* [annotation] */
		__inout_opt  UINT *pUnitsLength,
		/* [annotation] */
		__out_ecount_opt(*pDescriptionLength)  LPSTR szDescription,
		/* [annotation] */
		__inout_opt  UINT *pDescriptionLength);

	STDMETHOD(CheckFeatureSupport)(THIS_
		D3D11_FEATURE Feature,
		/* [annotation] */
		__out_bcount(FeatureSupportDataSize)  void *pFeatureSupportData,
		UINT FeatureSupportDataSize);

	STDMETHOD(GetPrivateData)(THIS_
		/* [annotation] */
		__in  REFGUID guid,
		/* [annotation] */
		__inout  UINT *pDataSize,
		/* [annotation] */
		__out_bcount_opt(*pDataSize)  void *pData);

	STDMETHOD(SetPrivateData)(THIS_
		/* [annotation] */
		__in  REFGUID guid,
		/* [annotation] */
		__in  UINT DataSize,
		/* [annotation] */
		__in_bcount_opt(DataSize)  const void *pData);

	STDMETHOD(SetPrivateDataInterface)(THIS_
		/* [annotation] */
		__in  REFGUID guid,
		/* [annotation] */
		__in_opt  const IUnknown *pData);

	STDMETHOD_(D3D_FEATURE_LEVEL, GetFeatureLevel)(THIS);

	STDMETHOD_(UINT, GetCreationFlags)(THIS);

	STDMETHOD(GetDeviceRemovedReason)(THIS);

	STDMETHOD_(void, GetImmediateContext)(THIS_
		/* [annotation] */
		__out ID3D11DeviceContext **ppImmediateContext);

	STDMETHOD(SetExceptionMode)(THIS_
		UINT RaiseFlags);

	STDMETHOD_(UINT, GetExceptionMode)(THIS);

};


// -----------------------------------------------------------------------------

class HackerDevice1 : public HackerDevice
{
private:
	ID3D11Device1 *mOrigDevice1;
	ID3D11DeviceContext1 *mOrigContext1;
	HackerContext1 *mHackerContext1;

public:
	HackerDevice1(ID3D11Device1 *pDevice1, ID3D11DeviceContext1 *pContext);

	void SetHackerContext1(HackerContext1 *pHackerContext);


	STDMETHOD_(void, GetImmediateContext1)(
		/* [annotation] */
		_Out_  ID3D11DeviceContext1 **ppImmediateContext);

	STDMETHOD(CreateDeferredContext1)(
		UINT ContextFlags,
		/* [annotation] */
		_Out_opt_  ID3D11DeviceContext1 **ppDeferredContext);

	STDMETHOD(CreateBlendState1)(
		/* [annotation] */
		_In_  const D3D11_BLEND_DESC1 *pBlendStateDesc,
		/* [annotation] */
		_Out_opt_  ID3D11BlendState1 **ppBlendState);

	STDMETHOD(CreateRasterizerState1)(
		/* [annotation] */
		_In_  const D3D11_RASTERIZER_DESC1 *pRasterizerDesc,
		/* [annotation] */
		_Out_opt_  ID3D11RasterizerState1 **ppRasterizerState);

	STDMETHOD(CreateDeviceContextState)(
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

	STDMETHOD(OpenSharedResource1)(
		/* [annotation] */
		_In_  HANDLE hResource,
		/* [annotation] */
		_In_  REFIID returnedInterface,
		/* [annotation] */
		_Out_  void **ppResource);

	STDMETHOD(OpenSharedResourceByName)(
		/* [annotation] */
		_In_  LPCWSTR lpName,
		/* [annotation] */
		_In_  DWORD dwDesiredAccess,
		/* [annotation] */
		_In_  REFIID returnedInterface,
		/* [annotation] */
		_Out_  void **ppResource);
};
