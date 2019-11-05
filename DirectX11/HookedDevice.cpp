// Hooked version of D3D11Device. The goal in this file is just to manage the
// business of hooking the calls and calling the original functions.  We still
// call into the HackerDevice for all the hard work - so as not to duplicate
// any functional code.
//
// Since the HackerDevice will need to call into the original device and not
// have those calls hooked we take care of that too - we create a trampoline
// interface that will call the original functions with the original this
// pointer and pass that back to the HackerDevice for it to use in place of the
// original device.


// We are defining CINTERFACE here to get the C declarations of the
// ID3D11Device1, which uses a struct with function pointers for the vtable.
// That avoids some nasty casting which we would have to do with the C++
// interface and as an added bonus gives us a convenient structure to keep the
// pointers of the original functions.
//
// We also then have to define D3D11_NO_HELPERS (or undef __cplusplus)
// otherwise the header will try to define a bunch of helper functions that
// depend on the C++ API and will fail to compile
//
// The downside of this is it makes it basically impossible to include any
// header files that directly or indirectly depend on the C++ interface, which
// includes HackerDevice.h and HackerContext.h. As tempted as I am to just rip
// the vtable definition out into it's own header to get around that, but
// perhaps this will help decouple this from the rest of 3DMigoto.

#define CINTERFACE
#define D3D11_NO_HELPERS
#define COBJMACROS
#include <d3d11_1.h>
#undef COBJMACROS
#undef D3D11_NO_HELPERS
#undef CINTERFACE

#include <unordered_map>

#include "HookedDevice.h"
#include "DLLMainHook.h"
#include "log.h"
#include "lock.h"


// Change this to 1 to enable debug logging of hooks and the trampolines back
// to the original device. Disabled by default as debug logging will already
// log most of these calls in the HackerDevice, so this would triple the noise
// and we probably won't need it very often:
#if 0
#define HookDebug LogDebug
#else
#define HookDebug(...) do { } while (0)
#endif


// A map to look up the hacker device from the original device:
typedef std::unordered_map<ID3D11Device1 *, ID3D11Device1 *> DeviceMap;
static DeviceMap device_map;
static CRITICAL_SECTION device_map_lock;

// Holds all the function pointers that we need to call into the real original
// device:
static struct ID3D11Device1Vtbl orig_vtable;

static bool hooks_installed = false;


ID3D11Device1* lookup_hooked_device(ID3D11Device1 *orig_device)
{
	DeviceMap::iterator i;

	if (!hooks_installed)
		return NULL;

	EnterCriticalSectionPretty(&device_map_lock);
	i = device_map.find(orig_device);
	if (i == device_map.end()) {
		LeaveCriticalSection(&device_map_lock);
		return NULL;
	}
	LeaveCriticalSection(&device_map_lock);

	return i->second;
}


// IUnknown
static HRESULT STDMETHODCALLTYPE QueryInterface(
		ID3D11Device1 * This,
		/* [in] */ REFIID riid,
		/* [annotation][iid_is][out] */
		__RPC__deref_out  void **ppvObject)
{
	ID3D11Device1 *device = lookup_hooked_device(This);

	HookDebug("HookedDevice::QueryInterface()\n");

	if (device)
		return ID3D11Device1_QueryInterface(device, riid, ppvObject);

	return orig_vtable.QueryInterface(This, riid, ppvObject);
}

static ULONG STDMETHODCALLTYPE AddRef(
		ID3D11Device1 * This)
{
	ID3D11Device1 *device = lookup_hooked_device(This);

	HookDebug("HookedDevice::AddRef()\n");

	if (device)
		return ID3D11Device1_AddRef(device);

	return orig_vtable.AddRef(This);
}

static ULONG STDMETHODCALLTYPE Release(
		ID3D11Device1 * This)
{
	DeviceMap::iterator i;
	ULONG ref;

	HookDebug("HookedDevice::Release()\n");

	EnterCriticalSectionPretty(&device_map_lock);
	i = device_map.find(This);
	if (i != device_map.end()) {
		ref = ID3D11Device1_Release(i->second);
		if (!ref)
			device_map.erase(i);
	} else
		ref = orig_vtable.Release(This);

	LeaveCriticalSection(&device_map_lock);

	return ref;
}

// ID3D11Device1
static HRESULT STDMETHODCALLTYPE CreateBuffer(
		ID3D11Device1 * This,
		/* [annotation] */
		__in  const D3D11_BUFFER_DESC *pDesc,
		/* [annotation] */
		__in_opt  const D3D11_SUBRESOURCE_DATA *pInitialData,
		/* [annotation] */
		__out_opt  ID3D11Buffer **ppBuffer)

{
	ID3D11Device1 *device = lookup_hooked_device(This);

	HookDebug("HookedDevice::CreateBuffer()\n");

	if (device)
		return ID3D11Device1_CreateBuffer(device, pDesc, pInitialData, ppBuffer);

	return orig_vtable.CreateBuffer(This, pDesc, pInitialData, ppBuffer);
}

static HRESULT STDMETHODCALLTYPE CreateTexture1D(
		ID3D11Device1 * This,
		/* [annotation] */
		__in  const D3D11_TEXTURE1D_DESC *pDesc,
		/* [annotation] */
		__in_xcount_opt(pDesc->MipLevels * pDesc->ArraySize)  const D3D11_SUBRESOURCE_DATA *pInitialData,
		/* [annotation] */
		__out_opt  ID3D11Texture1D **ppTexture1D)

{
	ID3D11Device1 *device = lookup_hooked_device(This);

	HookDebug("HookedDevice::CreateTexture1D()\n");

	if (device)
		return ID3D11Device1_CreateTexture1D(device, pDesc, pInitialData, ppTexture1D);

	return orig_vtable.CreateTexture1D(This, pDesc, pInitialData, ppTexture1D);
}


static HRESULT STDMETHODCALLTYPE CreateTexture2D(
		ID3D11Device1 * This,
		/* [annotation] */
		__in  const D3D11_TEXTURE2D_DESC *pDesc,
		/* [annotation] */
		__in_xcount_opt(pDesc->MipLevels * pDesc->ArraySize)  const D3D11_SUBRESOURCE_DATA *pInitialData,
		/* [annotation] */
		__out_opt  ID3D11Texture2D **ppTexture2D)

{
	ID3D11Device1 *device = lookup_hooked_device(This);

	HookDebug("HookedDevice::CreateTexture2D()\n");

	if (device)
		return ID3D11Device1_CreateTexture2D(device, pDesc, pInitialData, ppTexture2D);

	return orig_vtable.CreateTexture2D(This, pDesc, pInitialData, ppTexture2D);
}


static HRESULT STDMETHODCALLTYPE CreateTexture3D(
		ID3D11Device1 * This,
		/* [annotation] */
		__in  const D3D11_TEXTURE3D_DESC *pDesc,
		/* [annotation] */
		__in_xcount_opt(pDesc->MipLevels)  const D3D11_SUBRESOURCE_DATA *pInitialData,
		/* [annotation] */
		__out_opt  ID3D11Texture3D **ppTexture3D)

{
	ID3D11Device1 *device = lookup_hooked_device(This);

	HookDebug("HookedDevice::CreateTexture3D()\n");

	if (device)
		return ID3D11Device1_CreateTexture3D(device, pDesc, pInitialData, ppTexture3D);

	return orig_vtable.CreateTexture3D(This, pDesc, pInitialData, ppTexture3D);
}

static HRESULT STDMETHODCALLTYPE CreateShaderResourceView(
		ID3D11Device1 * This,
		/* [annotation] */
		__in  ID3D11Resource *pResource,
		/* [annotation] */
		__in_opt  const D3D11_SHADER_RESOURCE_VIEW_DESC *pDesc,
		/* [annotation] */
		__out_opt  ID3D11ShaderResourceView **ppSRView)

{
	ID3D11Device1 *device = lookup_hooked_device(This);

	HookDebug("HookedDevice::CreateShaderResourceView()\n");

	if (device)
		return ID3D11Device1_CreateShaderResourceView(device, pResource, pDesc, ppSRView);

	return orig_vtable.CreateShaderResourceView(This, pResource, pDesc, ppSRView);
}

static HRESULT STDMETHODCALLTYPE CreateUnorderedAccessView(
		ID3D11Device1 * This,
		/* [annotation] */
		__in  ID3D11Resource *pResource,
		/* [annotation] */
		__in_opt  const D3D11_UNORDERED_ACCESS_VIEW_DESC *pDesc,
		/* [annotation] */
		__out_opt  ID3D11UnorderedAccessView **ppUAView)

{
	ID3D11Device1 *device = lookup_hooked_device(This);

	HookDebug("HookedDevice::CreateUnorderedAccessView()\n");

	if (device)
		return ID3D11Device1_CreateUnorderedAccessView(device, pResource, pDesc, ppUAView);

	return orig_vtable.CreateUnorderedAccessView(This, pResource, pDesc, ppUAView);
}

static HRESULT STDMETHODCALLTYPE CreateRenderTargetView(
		ID3D11Device1 * This,
		/* [annotation] */
		__in  ID3D11Resource *pResource,
		/* [annotation] */
		__in_opt  const D3D11_RENDER_TARGET_VIEW_DESC *pDesc,
		/* [annotation] */
		__out_opt  ID3D11RenderTargetView **ppRTView)

{
	ID3D11Device1 *device = lookup_hooked_device(This);

	HookDebug("HookedDevice::CreateRenderTargetView()\n");

	if (device)
		return ID3D11Device1_CreateRenderTargetView(device, pResource, pDesc, ppRTView);

	return orig_vtable.CreateRenderTargetView(This, pResource, pDesc, ppRTView);
}

static HRESULT STDMETHODCALLTYPE CreateDepthStencilView(
		ID3D11Device1 * This,
		/* [annotation] */
		__in  ID3D11Resource *pResource,
		/* [annotation] */
		__in_opt  const D3D11_DEPTH_STENCIL_VIEW_DESC *pDesc,
		/* [annotation] */
		__out_opt  ID3D11DepthStencilView **ppDepthStencilView)

{
	ID3D11Device1 *device = lookup_hooked_device(This);

	HookDebug("HookedDevice::CreateDepthStencilView()\n");

	if (device)
		return ID3D11Device1_CreateDepthStencilView(device, pResource, pDesc, ppDepthStencilView);

	return orig_vtable.CreateDepthStencilView(This, pResource, pDesc, ppDepthStencilView);
}

static HRESULT STDMETHODCALLTYPE CreateInputLayout(
		ID3D11Device1 * This,
		/* [annotation] */
		__in_ecount(NumElements)  const D3D11_INPUT_ELEMENT_DESC *pInputElementDescs,
		/* [annotation] */
		__in_range( 0, D3D11_IA_VERTEX_INPUT_STRUCTURE_ELEMENT_COUNT )  UINT NumElements,
		/* [annotation] */
		__in  const void *pShaderBytecodeWithInputSignature,
		/* [annotation] */
		__in  SIZE_T BytecodeLength,
		/* [annotation] */
		__out_opt  ID3D11InputLayout **ppInputLayout)

{
	ID3D11Device1 *device = lookup_hooked_device(This);

	HookDebug("HookedDevice::CreateInputLayout()\n");

	if (device)
		return ID3D11Device1_CreateInputLayout(device, pInputElementDescs, NumElements, pShaderBytecodeWithInputSignature, BytecodeLength, ppInputLayout);

	return orig_vtable.CreateInputLayout(This, pInputElementDescs, NumElements, pShaderBytecodeWithInputSignature, BytecodeLength, ppInputLayout);
}

static HRESULT STDMETHODCALLTYPE CreateVertexShader(
		ID3D11Device1 * This,
		/* [annotation] */
		__in  const void *pShaderBytecode,
		/* [annotation] */
		__in  SIZE_T BytecodeLength,
		/* [annotation] */
		__in_opt  ID3D11ClassLinkage *pClassLinkage,
		/* [annotation] */
		__out_opt  ID3D11VertexShader **ppVertexShader)

{
	ID3D11Device1 *device = lookup_hooked_device(This);

	HookDebug("HookedDevice::CreateVertexShader()\n");

	if (device)
		return ID3D11Device1_CreateVertexShader(device, pShaderBytecode, BytecodeLength, pClassLinkage, ppVertexShader);

	return orig_vtable.CreateVertexShader(This, pShaderBytecode, BytecodeLength, pClassLinkage, ppVertexShader);
}

static HRESULT STDMETHODCALLTYPE CreateGeometryShader(
		ID3D11Device1 * This,
		/* [annotation] */
		__in  const void *pShaderBytecode,
		/* [annotation] */
		__in  SIZE_T BytecodeLength,
		/* [annotation] */
		__in_opt  ID3D11ClassLinkage *pClassLinkage,
		/* [annotation] */
		__out_opt  ID3D11GeometryShader **ppGeometryShader)

{
	ID3D11Device1 *device = lookup_hooked_device(This);

	HookDebug("HookedDevice::CreateGeometryShader()\n");

	if (device)
		return ID3D11Device1_CreateGeometryShader(device, pShaderBytecode, BytecodeLength, pClassLinkage, ppGeometryShader);

	return orig_vtable.CreateGeometryShader(This, pShaderBytecode, BytecodeLength, pClassLinkage, ppGeometryShader);
}

static HRESULT STDMETHODCALLTYPE CreateGeometryShaderWithStreamOutput(
		ID3D11Device1 * This,
		/* [annotation] */
		__in  const void *pShaderBytecode,
		/* [annotation] */
		__in  SIZE_T BytecodeLength,
		/* [annotation] */
		__in_ecount_opt(NumEntries)  const D3D11_SO_DECLARATION_ENTRY *pSODeclaration,
		/* [annotation] */
		__in_range( 0, D3D11_SO_STREAM_COUNT * D3D11_SO_OUTPUT_COMPONENT_COUNT )  UINT NumEntries,
		/* [annotation] */
		__in_ecount_opt(NumStrides)  const UINT *pBufferStrides,
		/* [annotation] */
		__in_range( 0, D3D11_SO_BUFFER_SLOT_COUNT )  UINT NumStrides,
		/* [annotation] */
		__in  UINT RasterizedStream,
		/* [annotation] */
		__in_opt  ID3D11ClassLinkage *pClassLinkage,
		/* [annotation] */
		__out_opt  ID3D11GeometryShader **ppGeometryShader)

{
	ID3D11Device1 *device = lookup_hooked_device(This);

	HookDebug("HookedDevice::CreateGeometryShaderWithStreamOutput()\n");

	if (device)
		return ID3D11Device1_CreateGeometryShaderWithStreamOutput(device, pShaderBytecode, BytecodeLength, pSODeclaration, NumEntries, pBufferStrides, NumStrides, RasterizedStream, pClassLinkage, ppGeometryShader);

	return orig_vtable.CreateGeometryShaderWithStreamOutput(This, pShaderBytecode, BytecodeLength, pSODeclaration, NumEntries, pBufferStrides, NumStrides, RasterizedStream, pClassLinkage, ppGeometryShader);
}

static HRESULT STDMETHODCALLTYPE CreatePixelShader(
		ID3D11Device1 * This,
		/* [annotation] */
		__in  const void *pShaderBytecode,
		/* [annotation] */
		__in  SIZE_T BytecodeLength,
		/* [annotation] */
		__in_opt  ID3D11ClassLinkage *pClassLinkage,
		/* [annotation] */
		__out_opt  ID3D11PixelShader **ppPixelShader)

{
	ID3D11Device1 *device = lookup_hooked_device(This);

	HookDebug("HookedDevice::CreatePixelShader()\n");

	if (device)
		return ID3D11Device1_CreatePixelShader(device, pShaderBytecode, BytecodeLength, pClassLinkage, ppPixelShader);

	return orig_vtable.CreatePixelShader(This, pShaderBytecode, BytecodeLength, pClassLinkage, ppPixelShader);
}

static HRESULT STDMETHODCALLTYPE CreateHullShader(
		ID3D11Device1 * This,
		/* [annotation] */
		__in  const void *pShaderBytecode,
		/* [annotation] */
		__in  SIZE_T BytecodeLength,
		/* [annotation] */
		__in_opt  ID3D11ClassLinkage *pClassLinkage,
		/* [annotation] */
		__out_opt  ID3D11HullShader **ppHullShader)

{
	ID3D11Device1 *device = lookup_hooked_device(This);

	HookDebug("HookedDevice::CreateHullShader()\n");

	if (device)
		return ID3D11Device1_CreateHullShader(device, pShaderBytecode, BytecodeLength, pClassLinkage, ppHullShader);

	return orig_vtable.CreateHullShader(This, pShaderBytecode, BytecodeLength, pClassLinkage, ppHullShader);
}

static HRESULT STDMETHODCALLTYPE CreateDomainShader(
		ID3D11Device1 * This,
		/* [annotation] */
		__in  const void *pShaderBytecode,
		/* [annotation] */
		__in  SIZE_T BytecodeLength,
		/* [annotation] */
		__in_opt  ID3D11ClassLinkage *pClassLinkage,
		/* [annotation] */
		__out_opt  ID3D11DomainShader **ppDomainShader)

{
	ID3D11Device1 *device = lookup_hooked_device(This);

	HookDebug("HookedDevice::CreateDomainShader()\n");

	if (device)
		return ID3D11Device1_CreateDomainShader(device, pShaderBytecode, BytecodeLength, pClassLinkage, ppDomainShader);

	return orig_vtable.CreateDomainShader(This, pShaderBytecode, BytecodeLength, pClassLinkage, ppDomainShader);
}

static HRESULT STDMETHODCALLTYPE CreateComputeShader(
		ID3D11Device1 * This,
		/* [annotation] */
		__in  const void *pShaderBytecode,
		/* [annotation] */
		__in  SIZE_T BytecodeLength,
		/* [annotation] */
		__in_opt  ID3D11ClassLinkage *pClassLinkage,
		/* [annotation] */
		__out_opt  ID3D11ComputeShader **ppComputeShader)

{
	ID3D11Device1 *device = lookup_hooked_device(This);

	HookDebug("HookedDevice::CreateComputeShader()\n");

	if (device)
		return ID3D11Device1_CreateComputeShader(device, pShaderBytecode, BytecodeLength, pClassLinkage, ppComputeShader);

	return orig_vtable.CreateComputeShader(This, pShaderBytecode, BytecodeLength, pClassLinkage, ppComputeShader);
}

static HRESULT STDMETHODCALLTYPE CreateClassLinkage(
		ID3D11Device1 * This,
		/* [annotation] */
		__out  ID3D11ClassLinkage **ppLinkage)

{
	ID3D11Device1 *device = lookup_hooked_device(This);

	HookDebug("HookedDevice::CreateClassLinkage()\n");

	if (device)
		return ID3D11Device1_CreateClassLinkage(device, ppLinkage);

	return orig_vtable.CreateClassLinkage(This, ppLinkage);
}

static HRESULT STDMETHODCALLTYPE CreateBlendState(
		ID3D11Device1 * This,
		/* [annotation] */
		__in  const D3D11_BLEND_DESC *pBlendStateDesc,
		/* [annotation] */
		__out_opt  ID3D11BlendState **ppBlendState)

{
	ID3D11Device1 *device = lookup_hooked_device(This);

	HookDebug("HookedDevice::CreateBlendState()\n");

	if (device)
		return ID3D11Device1_CreateBlendState(device, pBlendStateDesc, ppBlendState);

	return orig_vtable.CreateBlendState(This, pBlendStateDesc, ppBlendState);
}

static HRESULT STDMETHODCALLTYPE CreateDepthStencilState(
		ID3D11Device1 * This,
		/* [annotation] */
		__in  const D3D11_DEPTH_STENCIL_DESC *pDepthStencilDesc,
		/* [annotation] */
		__out_opt  ID3D11DepthStencilState **ppDepthStencilState)

{
	ID3D11Device1 *device = lookup_hooked_device(This);

	HookDebug("HookedDevice::CreateDepthStencilState()\n");

	if (device)
		return ID3D11Device1_CreateDepthStencilState(device, pDepthStencilDesc, ppDepthStencilState);

	return orig_vtable.CreateDepthStencilState(This, pDepthStencilDesc, ppDepthStencilState);
}

static HRESULT STDMETHODCALLTYPE CreateRasterizerState(
		ID3D11Device1 * This,
		/* [annotation] */
		__in  const D3D11_RASTERIZER_DESC *pRasterizerDesc,
		/* [annotation] */
		__out_opt  ID3D11RasterizerState **ppRasterizerState)

{
	ID3D11Device1 *device = lookup_hooked_device(This);

	HookDebug("HookedDevice::CreateRasterizerState()\n");

	if (device)
		return ID3D11Device1_CreateRasterizerState(device, pRasterizerDesc, ppRasterizerState);

	return orig_vtable.CreateRasterizerState(This, pRasterizerDesc, ppRasterizerState);
}

static HRESULT STDMETHODCALLTYPE CreateSamplerState(
		ID3D11Device1 * This,
		/* [annotation] */
		__in  const D3D11_SAMPLER_DESC *pSamplerDesc,
		/* [annotation] */
		__out_opt  ID3D11SamplerState **ppSamplerState)

{
	ID3D11Device1 *device = lookup_hooked_device(This);

	HookDebug("HookedDevice::CreateSamplerState()\n");

	if (device)
		return ID3D11Device1_CreateSamplerState(device, pSamplerDesc, ppSamplerState);

	return orig_vtable.CreateSamplerState(This, pSamplerDesc, ppSamplerState);
}

static HRESULT STDMETHODCALLTYPE CreateQuery(
		ID3D11Device1 * This,
		/* [annotation] */
		__in  const D3D11_QUERY_DESC *pQueryDesc,
		/* [annotation] */
		__out_opt  ID3D11Query **ppQuery)

{
	ID3D11Device1 *device = lookup_hooked_device(This);

	HookDebug("HookedDevice::CreateQuery()\n");

	if (device)
		return ID3D11Device1_CreateQuery(device, pQueryDesc, ppQuery);

	return orig_vtable.CreateQuery(This, pQueryDesc, ppQuery);
}

static HRESULT STDMETHODCALLTYPE CreatePredicate(
		ID3D11Device1 * This,
		/* [annotation] */
		__in  const D3D11_QUERY_DESC *pPredicateDesc,
		/* [annotation] */
		__out_opt  ID3D11Predicate **ppPredicate)

{
	ID3D11Device1 *device = lookup_hooked_device(This);

	HookDebug("HookedDevice::CreatePredicate()\n");

	if (device)
		return ID3D11Device1_CreatePredicate(device, pPredicateDesc, ppPredicate);

	return orig_vtable.CreatePredicate(This, pPredicateDesc, ppPredicate);
}

static HRESULT STDMETHODCALLTYPE CreateCounter(
		ID3D11Device1 * This,
		/* [annotation] */
		__in  const D3D11_COUNTER_DESC *pCounterDesc,
		/* [annotation] */
		__out_opt  ID3D11Counter **ppCounter)

{
	ID3D11Device1 *device = lookup_hooked_device(This);

	HookDebug("HookedDevice::CreateCounter()\n");

	if (device)
		return ID3D11Device1_CreateCounter(device, pCounterDesc, ppCounter);

	return orig_vtable.CreateCounter(This, pCounterDesc, ppCounter);
}

static HRESULT STDMETHODCALLTYPE CreateDeferredContext(
		ID3D11Device1 * This,
		UINT ContextFlags,
		/* [annotation] */
		__out_opt  ID3D11DeviceContext **ppDeferredContext)

{
	ID3D11Device1 *device = lookup_hooked_device(This);

	HookDebug("HookedDevice::CreateDeferredContext()\n");

	if (device)
		return ID3D11Device1_CreateDeferredContext(device, ContextFlags, ppDeferredContext);

	return orig_vtable.CreateDeferredContext(This, ContextFlags, ppDeferredContext);
}

static HRESULT STDMETHODCALLTYPE OpenSharedResource(
		ID3D11Device1 * This,
		/* [annotation] */
		__in  HANDLE hResource,
		/* [annotation] */
		__in  REFIID ReturnedInterface,
		/* [annotation] */
		__out_opt  void **ppResource)
{
	ID3D11Device1 *device = lookup_hooked_device(This);

	HookDebug("HookedDevice::OpenSharedResource()\n");

	if (device)
		return ID3D11Device1_OpenSharedResource(device, hResource, ReturnedInterface, ppResource);

	return orig_vtable.OpenSharedResource(This, hResource, ReturnedInterface, ppResource);
}

static HRESULT STDMETHODCALLTYPE CheckFormatSupport(
	ID3D11Device1 * This,
	/* [annotation] */
	__in  DXGI_FORMAT Format,
	/* [annotation] */
	__out  UINT *pFormatSupport)

{
	ID3D11Device1 *device = lookup_hooked_device(This);

	HookDebug("HookedDevice::CheckFormatSupport()\n");

	if (device)
		return ID3D11Device1_CheckFormatSupport(device, Format, pFormatSupport);

	return orig_vtable.CheckFormatSupport(This, Format, pFormatSupport);
}

static HRESULT STDMETHODCALLTYPE CheckMultisampleQualityLevels(
		ID3D11Device1 * This,
		/* [annotation] */
		__in  DXGI_FORMAT Format,
		/* [annotation] */
		__in  UINT SampleCount,
		/* [annotation] */
		__out  UINT *pNumQualityLevels)

{
	ID3D11Device1 *device = lookup_hooked_device(This);

	HookDebug("HookedDevice::CheckMultisampleQualityLevels()\n");

	if (device)
		return ID3D11Device1_CheckMultisampleQualityLevels(device, Format, SampleCount, pNumQualityLevels);

	return orig_vtable.CheckMultisampleQualityLevels(This, Format, SampleCount, pNumQualityLevels);
}

static void STDMETHODCALLTYPE CheckCounterInfo(
		ID3D11Device1 * This,
		/* [annotation] */
		__out  D3D11_COUNTER_INFO *pCounterInfo)

{
	ID3D11Device1 *device = lookup_hooked_device(This);

	HookDebug("HookedDevice::CheckCounterInfo()\n");

	if (device)
		return ID3D11Device1_CheckCounterInfo(device, pCounterInfo);

	return orig_vtable.CheckCounterInfo(This, pCounterInfo);
}

static HRESULT STDMETHODCALLTYPE CheckCounter(
		ID3D11Device1 * This,
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
		__inout_opt  UINT *pDescriptionLength)

{
	ID3D11Device1 *device = lookup_hooked_device(This);

	HookDebug("HookedDevice::CheckCounter()\n");

	if (device)
		return ID3D11Device1_CheckCounter(device, pDesc, pType, pActiveCounters, szName, pNameLength, szUnits, pUnitsLength, szDescription, pDescriptionLength);

	return orig_vtable.CheckCounter(This, pDesc, pType, pActiveCounters, szName, pNameLength, szUnits, pUnitsLength, szDescription, pDescriptionLength);
}

static HRESULT STDMETHODCALLTYPE CheckFeatureSupport(
		ID3D11Device1 * This,
		D3D11_FEATURE Feature,
		/* [annotation] */
		__out_bcount(FeatureSupportDataSize)  void *pFeatureSupportData,
		UINT FeatureSupportDataSize)

{
	ID3D11Device1 *device = lookup_hooked_device(This);

	HookDebug("HookedDevice::CheckFeatureSupport()\n");

	if (device)
		return ID3D11Device1_CheckFeatureSupport(device, Feature, pFeatureSupportData, FeatureSupportDataSize);

	return orig_vtable.CheckFeatureSupport(This, Feature, pFeatureSupportData, FeatureSupportDataSize);
}

static HRESULT STDMETHODCALLTYPE GetPrivateData(
		ID3D11Device1 * This,
		/* [annotation] */
		__in  REFGUID guid,
		/* [annotation] */
		__inout  UINT *pDataSize,
		/* [annotation] */
		__out_bcount_opt(*pDataSize)  void *pData)

{
	ID3D11Device1 *device = lookup_hooked_device(This);

	HookDebug("HookedDevice::GetPrivateData()\n");

	if (device)
		return ID3D11Device1_GetPrivateData(device, guid, pDataSize, pData);

	return orig_vtable.GetPrivateData(This, guid, pDataSize, pData);
}

static HRESULT STDMETHODCALLTYPE SetPrivateData(
		ID3D11Device1 * This,
		/* [annotation] */
		__in  REFGUID guid,
		/* [annotation] */
		__in  UINT DataSize,
		/* [annotation] */
		__in_bcount_opt(DataSize)  const void *pData)

{
	ID3D11Device1 *device = lookup_hooked_device(This);

	HookDebug("HookedDevice::SetPrivateData()\n");

	if (device)
		return ID3D11Device1_SetPrivateData(device, guid, DataSize, pData);

	return orig_vtable.SetPrivateData(This, guid, DataSize, pData);
}

static HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(
		ID3D11Device1 * This,
		/* [annotation] */
		__in  REFGUID guid,
		/* [annotation] */
		__in_opt  const IUnknown *pData)

{
	ID3D11Device1 *device = lookup_hooked_device(This);

	HookDebug("HookedDevice::SetPrivateDataInterface()\n");

	if (device)
		return ID3D11Device1_SetPrivateDataInterface(device, guid, pData);

	return orig_vtable.SetPrivateDataInterface(This, guid, pData);
}

static D3D_FEATURE_LEVEL STDMETHODCALLTYPE GetFeatureLevel(
		ID3D11Device1 * This)

{
	ID3D11Device1 *device = lookup_hooked_device(This);

	HookDebug("HookedDevice::GetFeatureLevel()\n");

	if (device)
		return ID3D11Device1_GetFeatureLevel(device);

	return orig_vtable.GetFeatureLevel(This);
}

static UINT STDMETHODCALLTYPE GetCreationFlags(
		ID3D11Device1 * This)

{
	ID3D11Device1 *device = lookup_hooked_device(This);

	HookDebug("HookedDevice::GetCreationFlags()\n");

	if (device)
		return ID3D11Device1_GetCreationFlags(device);

	return orig_vtable.GetCreationFlags(This);
}

static HRESULT STDMETHODCALLTYPE GetDeviceRemovedReason(
		ID3D11Device1 * This)

{
	ID3D11Device1 *device = lookup_hooked_device(This);

	HookDebug("HookedDevice::GetDeviceRemovedReason()\n");

	if (device)
		return ID3D11Device1_GetDeviceRemovedReason(device);

	return orig_vtable.GetDeviceRemovedReason(This);
}

static void STDMETHODCALLTYPE GetImmediateContext(
		ID3D11Device1 * This,
		/* [annotation] */
		__out  ID3D11DeviceContext **ppImmediateContext)

{
	ID3D11Device1 *device = lookup_hooked_device(This);

	HookDebug("HookedDevice::GetImmediateContext()\n");

	if (device)
		return ID3D11Device1_GetImmediateContext(device, ppImmediateContext);

	return orig_vtable.GetImmediateContext(This, ppImmediateContext);
}

static HRESULT STDMETHODCALLTYPE SetExceptionMode(
		ID3D11Device1 * This,
		UINT RaiseFlags)

{
	ID3D11Device1 *device = lookup_hooked_device(This);

	HookDebug("HookedDevice::SetExceptionMode()\n");

	if (device)
		return ID3D11Device1_SetExceptionMode(device, RaiseFlags);

	return orig_vtable.SetExceptionMode(This, RaiseFlags);
}

static UINT STDMETHODCALLTYPE GetExceptionMode(
		ID3D11Device1 * This)

{
	ID3D11Device1 *device = lookup_hooked_device(This);

	HookDebug("HookedDevice::GetExceptionMode()\n");

	if (device)
		return ID3D11Device1_GetExceptionMode(device);

	return orig_vtable.GetExceptionMode(This);
}

static void install_hooks(ID3D11Device1 *device)
{
	SIZE_T hook_id;

	// Hooks should only be installed once as they will affect all contexts
	if (hooks_installed)
		return;
	InitializeCriticalSectionPretty(&device_map_lock);
	hooks_installed = true;

	// Make sure that everything in the orig_vtable is filled in just in
	// case we miss one of the hooks below:
	memcpy(&orig_vtable, device->lpVtbl, sizeof(struct ID3D11Device1Vtbl));

	// At the moment we are just throwing away the hook IDs - we should
	// probably hold on to them incase we need to remove the hooks later:
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.QueryInterface,                       device->lpVtbl->QueryInterface,                       QueryInterface);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.AddRef,                               device->lpVtbl->AddRef,                               AddRef);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.Release,                              device->lpVtbl->Release,                              Release);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.CreateBuffer,                         device->lpVtbl->CreateBuffer,                         CreateBuffer);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.CreateTexture1D,                      device->lpVtbl->CreateTexture1D,                      CreateTexture1D);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.CreateTexture2D,                      device->lpVtbl->CreateTexture2D,                      CreateTexture2D);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.CreateTexture3D,                      device->lpVtbl->CreateTexture3D,                      CreateTexture3D);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.CreateShaderResourceView,             device->lpVtbl->CreateShaderResourceView,             CreateShaderResourceView);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.CreateUnorderedAccessView,            device->lpVtbl->CreateUnorderedAccessView,            CreateUnorderedAccessView);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.CreateRenderTargetView,               device->lpVtbl->CreateRenderTargetView,               CreateRenderTargetView);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.CreateDepthStencilView,               device->lpVtbl->CreateDepthStencilView,               CreateDepthStencilView);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.CreateInputLayout,                    device->lpVtbl->CreateInputLayout,                    CreateInputLayout);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.CreateVertexShader,                   device->lpVtbl->CreateVertexShader,                   CreateVertexShader);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.CreateGeometryShader,                 device->lpVtbl->CreateGeometryShader,                 CreateGeometryShader);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.CreateGeometryShaderWithStreamOutput, device->lpVtbl->CreateGeometryShaderWithStreamOutput, CreateGeometryShaderWithStreamOutput);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.CreatePixelShader,                    device->lpVtbl->CreatePixelShader,                    CreatePixelShader);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.CreateHullShader,                     device->lpVtbl->CreateHullShader,                     CreateHullShader);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.CreateDomainShader,                   device->lpVtbl->CreateDomainShader,                   CreateDomainShader);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.CreateComputeShader,                  device->lpVtbl->CreateComputeShader,                  CreateComputeShader);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.CreateClassLinkage,                   device->lpVtbl->CreateClassLinkage,                   CreateClassLinkage);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.CreateBlendState,                     device->lpVtbl->CreateBlendState,                     CreateBlendState);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.CreateDepthStencilState,              device->lpVtbl->CreateDepthStencilState,              CreateDepthStencilState);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.CreateRasterizerState,                device->lpVtbl->CreateRasterizerState,                CreateRasterizerState);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.CreateSamplerState,                   device->lpVtbl->CreateSamplerState,                   CreateSamplerState);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.CreateQuery,                          device->lpVtbl->CreateQuery,                          CreateQuery);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.CreatePredicate,                      device->lpVtbl->CreatePredicate,                      CreatePredicate);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.CreateCounter,                        device->lpVtbl->CreateCounter,                        CreateCounter);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.CreateDeferredContext,                device->lpVtbl->CreateDeferredContext,                CreateDeferredContext);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.OpenSharedResource,                   device->lpVtbl->OpenSharedResource,                   OpenSharedResource);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.CheckFormatSupport,                   device->lpVtbl->CheckFormatSupport,                   CheckFormatSupport);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.CheckMultisampleQualityLevels,        device->lpVtbl->CheckMultisampleQualityLevels,        CheckMultisampleQualityLevels);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.CheckCounterInfo,                     device->lpVtbl->CheckCounterInfo,                     CheckCounterInfo);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.CheckCounter,                         device->lpVtbl->CheckCounter,                         CheckCounter);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.CheckFeatureSupport,                  device->lpVtbl->CheckFeatureSupport,                  CheckFeatureSupport);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetPrivateData,                       device->lpVtbl->GetPrivateData,                       GetPrivateData);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.SetPrivateData,                       device->lpVtbl->SetPrivateData,                       SetPrivateData);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.SetPrivateDataInterface,              device->lpVtbl->SetPrivateDataInterface,              SetPrivateDataInterface);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetFeatureLevel,                      device->lpVtbl->GetFeatureLevel,                      GetFeatureLevel);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetCreationFlags,                     device->lpVtbl->GetCreationFlags,                     GetCreationFlags);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetDeviceRemovedReason,               device->lpVtbl->GetDeviceRemovedReason,               GetDeviceRemovedReason);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetImmediateContext,                  device->lpVtbl->GetImmediateContext,                  GetImmediateContext);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.SetExceptionMode,                     device->lpVtbl->SetExceptionMode,                     SetExceptionMode);
	cHookMgr.Hook(&hook_id, (void**)&orig_vtable.GetExceptionMode,                     device->lpVtbl->GetExceptionMode,                     GetExceptionMode);
}

// This provides another ID3D11Device1 interface for calling the original
// functions in orig_vtable. This replaces mOrigDevice in the HackerDevice and
// elsewhere and gives us a way to call back into the game with minimal code
// changes.
typedef struct ID3D11Device1Trampoline {
	CONST_VTBL struct ID3D11Device1Vtbl *lpVtbl;
	ID3D11Device1 *orig_this;
} ID3D11Device1Trampoline;


// IUnknown
static HRESULT STDMETHODCALLTYPE TrampolineQueryInterface(
		ID3D11Device1 * This,
		/* [in] */ REFIID riid,
		/* [annotation][iid_is][out] */
		__RPC__deref_out  void **ppvObject)
{
	HookDebug("TrampolineDevice::QueryInterface()\n");
	return orig_vtable.QueryInterface(((ID3D11Device1Trampoline*)This)->orig_this, riid, ppvObject);
}
static ULONG STDMETHODCALLTYPE TrampolineAddRef(
		ID3D11Device1 *This)
{
	HookDebug("TrampolineDevice::AddRef()\n");
	return orig_vtable.AddRef(((ID3D11Device1Trampoline*)This)->orig_this);
}
static ULONG STDMETHODCALLTYPE TrampolineRelease(
		ID3D11Device1 *This)
{
	ULONG ref;

	HookDebug("TrampolineDevice::Release()\n");
	ref = orig_vtable.Release(((ID3D11Device1Trampoline*)This)->orig_this);

	if (!ref)
		delete This;

	return ref;
}
// ID3D11Device1
static HRESULT STDMETHODCALLTYPE TrampolineCreateBuffer(
		ID3D11Device1 * This,
		/* [annotation] */
		__in  const D3D11_BUFFER_DESC *pDesc,
		/* [annotation] */
		__in_opt  const D3D11_SUBRESOURCE_DATA *pInitialData,
		/* [annotation] */
		__out_opt  ID3D11Buffer **ppBuffer)
{
	HookDebug("TrampolineDevice::CreateBuffer()\n");
	return orig_vtable.CreateBuffer(((ID3D11Device1Trampoline*)This)->orig_this, pDesc, pInitialData, ppBuffer);
}
static HRESULT STDMETHODCALLTYPE TrampolineCreateTexture1D(
		ID3D11Device1 * This,
		/* [annotation] */
		__in  const D3D11_TEXTURE1D_DESC *pDesc,
		/* [annotation] */
		__in_xcount_opt(pDesc->MipLevels * pDesc->ArraySize)  const D3D11_SUBRESOURCE_DATA *pInitialData,
		/* [annotation] */
		__out_opt  ID3D11Texture1D **ppTexture1D)
{
	HookDebug("TrampolineDevice::CreateTexture1D()\n");
	return orig_vtable.CreateTexture1D(((ID3D11Device1Trampoline*)This)->orig_this, pDesc, pInitialData, ppTexture1D);
}
static HRESULT STDMETHODCALLTYPE TrampolineCreateTexture2D(
		ID3D11Device1 * This,
		/* [annotation] */
		__in  const D3D11_TEXTURE2D_DESC *pDesc,
		/* [annotation] */
		__in_xcount_opt(pDesc->MipLevels * pDesc->ArraySize)  const D3D11_SUBRESOURCE_DATA *pInitialData,
		/* [annotation] */
		__out_opt  ID3D11Texture2D **ppTexture2D)
{
	HookDebug("TrampolineDevice::CreateTexture2D()\n");
	return orig_vtable.CreateTexture2D(((ID3D11Device1Trampoline*)This)->orig_this, pDesc, pInitialData, ppTexture2D);
}
static HRESULT STDMETHODCALLTYPE TrampolineCreateTexture3D(
		ID3D11Device1 * This,
		/* [annotation] */
		__in  const D3D11_TEXTURE3D_DESC *pDesc,
		/* [annotation] */
		__in_xcount_opt(pDesc->MipLevels)  const D3D11_SUBRESOURCE_DATA *pInitialData,
		/* [annotation] */
		__out_opt  ID3D11Texture3D **ppTexture3D)
{
	HookDebug("TrampolineDevice::CreateTexture3D()\n");
	return orig_vtable.CreateTexture3D(((ID3D11Device1Trampoline*)This)->orig_this, pDesc, pInitialData, ppTexture3D);
}
static HRESULT STDMETHODCALLTYPE TrampolineCreateShaderResourceView(
		ID3D11Device1 * This,
		/* [annotation] */
		__in  ID3D11Resource *pResource,
		/* [annotation] */
		__in_opt  const D3D11_SHADER_RESOURCE_VIEW_DESC *pDesc,
		/* [annotation] */
		__out_opt  ID3D11ShaderResourceView **ppSRView)
{
	HookDebug("TrampolineDevice::CreateShaderResourceView()\n");
	return orig_vtable.CreateShaderResourceView(((ID3D11Device1Trampoline*)This)->orig_this, pResource, pDesc, ppSRView);
}
static HRESULT STDMETHODCALLTYPE TrampolineCreateUnorderedAccessView(
		ID3D11Device1 * This,
		/* [annotation] */
		__in  ID3D11Resource *pResource,
		/* [annotation] */
		__in_opt  const D3D11_UNORDERED_ACCESS_VIEW_DESC *pDesc,
		/* [annotation] */
		__out_opt  ID3D11UnorderedAccessView **ppUAView)
{
	HookDebug("TrampolineDevice::CreateUnorderedAccessView()\n");
	return orig_vtable.CreateUnorderedAccessView(((ID3D11Device1Trampoline*)This)->orig_this, pResource, pDesc, ppUAView);
}
static HRESULT STDMETHODCALLTYPE TrampolineCreateRenderTargetView(
		ID3D11Device1 * This,
		/* [annotation] */
		__in  ID3D11Resource *pResource,
		/* [annotation] */
		__in_opt  const D3D11_RENDER_TARGET_VIEW_DESC *pDesc,
		/* [annotation] */
		__out_opt  ID3D11RenderTargetView **ppRTView)
{
	HookDebug("TrampolineDevice::CreateRenderTargetView()\n");
	return orig_vtable.CreateRenderTargetView(((ID3D11Device1Trampoline*)This)->orig_this, pResource, pDesc, ppRTView);
}
static HRESULT STDMETHODCALLTYPE TrampolineCreateDepthStencilView(
		ID3D11Device1 * This,
		/* [annotation] */
		__in  ID3D11Resource *pResource,
		/* [annotation] */
		__in_opt  const D3D11_DEPTH_STENCIL_VIEW_DESC *pDesc,
		/* [annotation] */
		__out_opt  ID3D11DepthStencilView **ppDepthStencilView)
{
	HookDebug("TrampolineDevice::CreateDepthStencilView()\n");
	return orig_vtable.CreateDepthStencilView(((ID3D11Device1Trampoline*)This)->orig_this, pResource, pDesc, ppDepthStencilView);
}
static HRESULT STDMETHODCALLTYPE TrampolineCreateInputLayout(
		ID3D11Device1 * This,
		/* [annotation] */
		__in_ecount(NumElements)  const D3D11_INPUT_ELEMENT_DESC *pInputElementDescs,
		/* [annotation] */
		__in_range( 0, D3D11_IA_VERTEX_INPUT_STRUCTURE_ELEMENT_COUNT )  UINT NumElements,
		/* [annotation] */
		__in  const void *pShaderBytecodeWithInputSignature,
		/* [annotation] */
		__in  SIZE_T BytecodeLength,
		/* [annotation] */
		__out_opt  ID3D11InputLayout **ppInputLayout)
{
	HookDebug("TrampolineDevice::CreateInputLayout()\n");
	return orig_vtable.CreateInputLayout(((ID3D11Device1Trampoline*)This)->orig_this, pInputElementDescs, NumElements, pShaderBytecodeWithInputSignature, BytecodeLength, ppInputLayout);
}
static HRESULT STDMETHODCALLTYPE TrampolineCreateVertexShader(
		ID3D11Device1 * This,
		/* [annotation] */
		__in  const void *pShaderBytecode,
		/* [annotation] */
		__in  SIZE_T BytecodeLength,
		/* [annotation] */
		__in_opt  ID3D11ClassLinkage *pClassLinkage,
		/* [annotation] */
		__out_opt  ID3D11VertexShader **ppVertexShader)
{
	HookDebug("TrampolineDevice::CreateVertexShader()\n");
	return orig_vtable.CreateVertexShader(((ID3D11Device1Trampoline*)This)->orig_this, pShaderBytecode, BytecodeLength, pClassLinkage, ppVertexShader);
}
static HRESULT STDMETHODCALLTYPE TrampolineCreateGeometryShader(
		ID3D11Device1 * This,
		/* [annotation] */
		__in  const void *pShaderBytecode,
		/* [annotation] */
		__in  SIZE_T BytecodeLength,
		/* [annotation] */
		__in_opt  ID3D11ClassLinkage *pClassLinkage,
		/* [annotation] */
		__out_opt  ID3D11GeometryShader **ppGeometryShader)
{
	HookDebug("TrampolineDevice::CreateGeometryShader()\n");
	return orig_vtable.CreateGeometryShader(((ID3D11Device1Trampoline*)This)->orig_this, pShaderBytecode, BytecodeLength, pClassLinkage, ppGeometryShader);
}
static HRESULT STDMETHODCALLTYPE TrampolineCreateGeometryShaderWithStreamOutput(
		ID3D11Device1 * This,
		/* [annotation] */
		__in  const void *pShaderBytecode,
		/* [annotation] */
		__in  SIZE_T BytecodeLength,
		/* [annotation] */
		__in_ecount_opt(NumEntries)  const D3D11_SO_DECLARATION_ENTRY *pSODeclaration,
		/* [annotation] */
		__in_range( 0, D3D11_SO_STREAM_COUNT * D3D11_SO_OUTPUT_COMPONENT_COUNT )  UINT NumEntries,
		/* [annotation] */
		__in_ecount_opt(NumStrides)  const UINT *pBufferStrides,
		/* [annotation] */
		__in_range( 0, D3D11_SO_BUFFER_SLOT_COUNT )  UINT NumStrides,
		/* [annotation] */
		__in  UINT RasterizedStream,
		/* [annotation] */
		__in_opt  ID3D11ClassLinkage *pClassLinkage,
		/* [annotation] */
		__out_opt  ID3D11GeometryShader **ppGeometryShader)
{
	HookDebug("TrampolineDevice::CreateGeometryShaderWithStreamOutput()\n");
	return orig_vtable.CreateGeometryShaderWithStreamOutput(((ID3D11Device1Trampoline*)This)->orig_this, pShaderBytecode, BytecodeLength, pSODeclaration, NumEntries, pBufferStrides, NumStrides, RasterizedStream, pClassLinkage, ppGeometryShader);
}
static HRESULT STDMETHODCALLTYPE TrampolineCreatePixelShader(
		ID3D11Device1 * This,
		/* [annotation] */
		__in  const void *pShaderBytecode,
		/* [annotation] */
		__in  SIZE_T BytecodeLength,
		/* [annotation] */
		__in_opt  ID3D11ClassLinkage *pClassLinkage,
		/* [annotation] */
		__out_opt  ID3D11PixelShader **ppPixelShader)
{
	HookDebug("TrampolineDevice::CreatePixelShader()\n");
	return orig_vtable.CreatePixelShader(((ID3D11Device1Trampoline*)This)->orig_this, pShaderBytecode, BytecodeLength, pClassLinkage, ppPixelShader);
}
static HRESULT STDMETHODCALLTYPE TrampolineCreateHullShader(
		ID3D11Device1 * This,
		/* [annotation] */
		__in  const void *pShaderBytecode,
		/* [annotation] */
		__in  SIZE_T BytecodeLength,
		/* [annotation] */
		__in_opt  ID3D11ClassLinkage *pClassLinkage,
		/* [annotation] */
		__out_opt  ID3D11HullShader **ppHullShader)
{
	HookDebug("TrampolineDevice::CreateHullShader()\n");
	return orig_vtable.CreateHullShader(((ID3D11Device1Trampoline*)This)->orig_this, pShaderBytecode, BytecodeLength, pClassLinkage, ppHullShader);
}
static HRESULT STDMETHODCALLTYPE TrampolineCreateDomainShader(
		ID3D11Device1 * This,
		/* [annotation] */
		__in  const void *pShaderBytecode,
		/* [annotation] */
		__in  SIZE_T BytecodeLength,
		/* [annotation] */
		__in_opt  ID3D11ClassLinkage *pClassLinkage,
		/* [annotation] */
		__out_opt  ID3D11DomainShader **ppDomainShader)
{
	HookDebug("TrampolineDevice::CreateDomainShader()\n");
	return orig_vtable.CreateDomainShader(((ID3D11Device1Trampoline*)This)->orig_this, pShaderBytecode, BytecodeLength, pClassLinkage, ppDomainShader);
}
static HRESULT STDMETHODCALLTYPE TrampolineCreateComputeShader(
		ID3D11Device1 * This,
		/* [annotation] */
		__in  const void *pShaderBytecode,
		/* [annotation] */
		__in  SIZE_T BytecodeLength,
		/* [annotation] */
		__in_opt  ID3D11ClassLinkage *pClassLinkage,
		/* [annotation] */
		__out_opt  ID3D11ComputeShader **ppComputeShader)
{
	HookDebug("TrampolineDevice::CreateComputeShader()\n");
	return orig_vtable.CreateComputeShader(((ID3D11Device1Trampoline*)This)->orig_this, pShaderBytecode, BytecodeLength, pClassLinkage, ppComputeShader);
}
static HRESULT STDMETHODCALLTYPE TrampolineCreateClassLinkage(
		ID3D11Device1 * This,
		/* [annotation] */
		__out  ID3D11ClassLinkage **ppLinkage)
{
	HookDebug("TrampolineDevice::CreateClassLinkage()\n");
	return orig_vtable.CreateClassLinkage(((ID3D11Device1Trampoline*)This)->orig_this, ppLinkage);
}
static HRESULT STDMETHODCALLTYPE TrampolineCreateBlendState(
		ID3D11Device1 * This,
		/* [annotation] */
		__in  const D3D11_BLEND_DESC *pBlendStateDesc,
		/* [annotation] */
		__out_opt  ID3D11BlendState **ppBlendState)
{
	HookDebug("TrampolineDevice::CreateBlendState()\n");
	return orig_vtable.CreateBlendState(((ID3D11Device1Trampoline*)This)->orig_this, pBlendStateDesc, ppBlendState);
}
static HRESULT STDMETHODCALLTYPE TrampolineCreateDepthStencilState(
		ID3D11Device1 * This,
		/* [annotation] */
		__in  const D3D11_DEPTH_STENCIL_DESC *pDepthStencilDesc,
		/* [annotation] */
		__out_opt  ID3D11DepthStencilState **ppDepthStencilState)
{
	HookDebug("TrampolineDevice::CreateDepthStencilState()\n");
	return orig_vtable.CreateDepthStencilState(((ID3D11Device1Trampoline*)This)->orig_this, pDepthStencilDesc, ppDepthStencilState);
}
static HRESULT STDMETHODCALLTYPE TrampolineCreateRasterizerState(
		ID3D11Device1 * This,
		/* [annotation] */
		__in  const D3D11_RASTERIZER_DESC *pRasterizerDesc,
		/* [annotation] */
		__out_opt  ID3D11RasterizerState **ppRasterizerState)
{
	HookDebug("TrampolineDevice::CreateRasterizerState()\n");
	return orig_vtable.CreateRasterizerState(((ID3D11Device1Trampoline*)This)->orig_this, pRasterizerDesc, ppRasterizerState);
}
static HRESULT STDMETHODCALLTYPE TrampolineCreateSamplerState(
		ID3D11Device1 * This,
		/* [annotation] */
		__in  const D3D11_SAMPLER_DESC *pSamplerDesc,
		/* [annotation] */
		__out_opt  ID3D11SamplerState **ppSamplerState)
{
	HookDebug("TrampolineDevice::CreateSamplerState()\n");
	return orig_vtable.CreateSamplerState(((ID3D11Device1Trampoline*)This)->orig_this, pSamplerDesc, ppSamplerState);
}
static HRESULT STDMETHODCALLTYPE TrampolineCreateQuery(
		ID3D11Device1 * This,
		/* [annotation] */
		__in  const D3D11_QUERY_DESC *pQueryDesc,
		/* [annotation] */
		__out_opt  ID3D11Query **ppQuery)
{
	HookDebug("TrampolineDevice::CreateQuery()\n");
	return orig_vtable.CreateQuery(((ID3D11Device1Trampoline*)This)->orig_this, pQueryDesc, ppQuery);
}
static HRESULT STDMETHODCALLTYPE TrampolineCreatePredicate(
		ID3D11Device1 * This,
		/* [annotation] */
		__in  const D3D11_QUERY_DESC *pPredicateDesc,
		/* [annotation] */
		__out_opt  ID3D11Predicate **ppPredicate)
{
	HookDebug("TrampolineDevice::CreatePredicate()\n");
	return orig_vtable.CreatePredicate(((ID3D11Device1Trampoline*)This)->orig_this, pPredicateDesc, ppPredicate);
}
static HRESULT STDMETHODCALLTYPE TrampolineCreateCounter(
		ID3D11Device1 * This,
		/* [annotation] */
		__in  const D3D11_COUNTER_DESC *pCounterDesc,
		/* [annotation] */
		__out_opt  ID3D11Counter **ppCounter)
{
	HookDebug("TrampolineDevice::CreateCounter()\n");
	return orig_vtable.CreateCounter(((ID3D11Device1Trampoline*)This)->orig_this, pCounterDesc, ppCounter);
}
static HRESULT STDMETHODCALLTYPE TrampolineCreateDeferredContext(
		ID3D11Device1 * This,
		UINT ContextFlags,
		/* [annotation] */
		__out_opt  ID3D11DeviceContext **ppDeferredContext)
{
	HookDebug("TrampolineDevice::CreateDeferredContext()\n");
	return orig_vtable.CreateDeferredContext(((ID3D11Device1Trampoline*)This)->orig_this, ContextFlags, ppDeferredContext);
}
static HRESULT STDMETHODCALLTYPE TrampolineOpenSharedResource(
		ID3D11Device1 * This,
		/* [annotation] */
		__in  HANDLE hResource,
		/* [annotation] */
		__in  REFIID ReturnedInterface,
		/* [annotation] */
		__out_opt  void **ppResource)
{
	HookDebug("TrampolineDevice::OpenSharedResource()\n");
	return orig_vtable.OpenSharedResource(((ID3D11Device1Trampoline*)This)->orig_this, hResource, ReturnedInterface, ppResource);
}
static HRESULT STDMETHODCALLTYPE TrampolineCheckFormatSupport(
	ID3D11Device1 * This,
	/* [annotation] */
	__in  DXGI_FORMAT Format,
	/* [annotation] */
	__out  UINT *pFormatSupport)
{
	HookDebug("TrampolineDevice::CheckFormatSupport()\n");
	return orig_vtable.CheckFormatSupport(((ID3D11Device1Trampoline*)This)->orig_this, Format, pFormatSupport);
}
static HRESULT STDMETHODCALLTYPE TrampolineCheckMultisampleQualityLevels(
		ID3D11Device1 * This,
		/* [annotation] */
		__in  DXGI_FORMAT Format,
		/* [annotation] */
		__in  UINT SampleCount,
		/* [annotation] */
		__out  UINT *pNumQualityLevels)
{
	HookDebug("TrampolineDevice::CheckMultisampleQualityLevels()\n");
	return orig_vtable.CheckMultisampleQualityLevels(((ID3D11Device1Trampoline*)This)->orig_this, Format, SampleCount, pNumQualityLevels);
}
static void STDMETHODCALLTYPE TrampolineCheckCounterInfo(
		ID3D11Device1 * This,
		/* [annotation] */
		__out  D3D11_COUNTER_INFO *pCounterInfo)
{
	HookDebug("TrampolineDevice::CheckCounterInfo()\n");
	return orig_vtable.CheckCounterInfo(((ID3D11Device1Trampoline*)This)->orig_this, pCounterInfo);
}
static HRESULT STDMETHODCALLTYPE TrampolineCheckCounter(
		ID3D11Device1 * This,
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
		__inout_opt  UINT *pDescriptionLength)
{
	HookDebug("TrampolineDevice::CheckCounter()\n");
	return orig_vtable.CheckCounter(((ID3D11Device1Trampoline*)This)->orig_this, pDesc, pType, pActiveCounters, szName, pNameLength, szUnits, pUnitsLength, szDescription, pDescriptionLength);
}
static HRESULT STDMETHODCALLTYPE TrampolineCheckFeatureSupport(
		ID3D11Device1 * This,
		D3D11_FEATURE Feature,
		/* [annotation] */
		__out_bcount(FeatureSupportDataSize)  void *pFeatureSupportData,
		UINT FeatureSupportDataSize)
{
	HookDebug("TrampolineDevice::CheckFeatureSupport()\n");
	return orig_vtable.CheckFeatureSupport(((ID3D11Device1Trampoline*)This)->orig_this, Feature, pFeatureSupportData, FeatureSupportDataSize);
}
static HRESULT STDMETHODCALLTYPE TrampolineGetPrivateData(
		ID3D11Device1 * This,
		/* [annotation] */
		__in  REFGUID guid,
		/* [annotation] */
		__inout  UINT *pDataSize,
		/* [annotation] */
		__out_bcount_opt(*pDataSize)  void *pData)
{
	HookDebug("TrampolineDevice::GetPrivateData()\n");
	return orig_vtable.GetPrivateData(((ID3D11Device1Trampoline*)This)->orig_this, guid, pDataSize, pData);
}
static HRESULT STDMETHODCALLTYPE TrampolineSetPrivateData(
		ID3D11Device1 * This,
		/* [annotation] */
		__in  REFGUID guid,
		/* [annotation] */
		__in  UINT DataSize,
		/* [annotation] */
		__in_bcount_opt(DataSize)  const void *pData)
{
	HookDebug("TrampolineDevice::SetPrivateData()\n");
	return orig_vtable.SetPrivateData(((ID3D11Device1Trampoline*)This)->orig_this, guid, DataSize, pData);
}
static HRESULT STDMETHODCALLTYPE TrampolineSetPrivateDataInterface(
		ID3D11Device1 * This,
		/* [annotation] */
		__in  REFGUID guid,
		/* [annotation] */
		__in_opt  const IUnknown *pData)
{
	HookDebug("TrampolineDevice::SetPrivateDataInterface()\n");
	return orig_vtable.SetPrivateDataInterface(((ID3D11Device1Trampoline*)This)->orig_this, guid, pData);
}
static D3D_FEATURE_LEVEL STDMETHODCALLTYPE TrampolineGetFeatureLevel(
		ID3D11Device1 * This)
{
	HookDebug("TrampolineDevice::GetFeatureLevel()\n");
	return orig_vtable.GetFeatureLevel(((ID3D11Device1Trampoline*)This)->orig_this);
}
static UINT STDMETHODCALLTYPE TrampolineGetCreationFlags(
		ID3D11Device1 * This)
{
	HookDebug("TrampolineDevice::GetCreationFlags()\n");
	return orig_vtable.GetCreationFlags(((ID3D11Device1Trampoline*)This)->orig_this);
}
static HRESULT STDMETHODCALLTYPE TrampolineGetDeviceRemovedReason(
		ID3D11Device1 * This)
{
	HookDebug("TrampolineDevice::GetDeviceRemovedReason()\n");
	return orig_vtable.GetDeviceRemovedReason(((ID3D11Device1Trampoline*)This)->orig_this);
}
static void STDMETHODCALLTYPE TrampolineGetImmediateContext(
		ID3D11Device1 * This,
		/* [annotation] */
		__out  ID3D11DeviceContext **ppImmediateContext)
{
	HookDebug("TrampolineDevice::GetImmediateContext()\n");
	return orig_vtable.GetImmediateContext(((ID3D11Device1Trampoline*)This)->orig_this, ppImmediateContext);
}
static HRESULT STDMETHODCALLTYPE TrampolineSetExceptionMode(
		ID3D11Device1 * This,
		UINT RaiseFlags)
{
	HookDebug("TrampolineDevice::SetExceptionMode()\n");
	return orig_vtable.SetExceptionMode(((ID3D11Device1Trampoline*)This)->orig_this, RaiseFlags);
}
static UINT STDMETHODCALLTYPE TrampolineGetExceptionMode(
		ID3D11Device1 * This)
{
	HookDebug("TrampolineDevice::GetExceptionMode()\n");
	return orig_vtable.GetExceptionMode(((ID3D11Device1Trampoline*)This)->orig_this);
}

static CONST_VTBL struct ID3D11Device1Vtbl trampoline_vtable = {
	TrampolineQueryInterface,
	TrampolineAddRef,
	TrampolineRelease,
	TrampolineCreateBuffer,
	TrampolineCreateTexture1D,
	TrampolineCreateTexture2D,
	TrampolineCreateTexture3D,
	TrampolineCreateShaderResourceView,
	TrampolineCreateUnorderedAccessView,
	TrampolineCreateRenderTargetView,
	TrampolineCreateDepthStencilView,
	TrampolineCreateInputLayout,
	TrampolineCreateVertexShader,
	TrampolineCreateGeometryShader,
	TrampolineCreateGeometryShaderWithStreamOutput,
	TrampolineCreatePixelShader,
	TrampolineCreateHullShader,
	TrampolineCreateDomainShader,
	TrampolineCreateComputeShader,
	TrampolineCreateClassLinkage,
	TrampolineCreateBlendState,
	TrampolineCreateDepthStencilState,
	TrampolineCreateRasterizerState,
	TrampolineCreateSamplerState,
	TrampolineCreateQuery,
	TrampolineCreatePredicate,
	TrampolineCreateCounter,
	TrampolineCreateDeferredContext,
	TrampolineOpenSharedResource,
	TrampolineCheckFormatSupport,
	TrampolineCheckMultisampleQualityLevels,
	TrampolineCheckCounterInfo,
	TrampolineCheckCounter,
	TrampolineCheckFeatureSupport,
	TrampolineGetPrivateData,
	TrampolineSetPrivateData,
	TrampolineSetPrivateDataInterface,
	TrampolineGetFeatureLevel,
	TrampolineGetCreationFlags,
	TrampolineGetDeviceRemovedReason,
	TrampolineGetImmediateContext,
	TrampolineSetExceptionMode,
	TrampolineGetExceptionMode,
};

ID3D11Device1* hook_device(ID3D11Device1 *orig_device, ID3D11Device1 *hacker_device)
{
	ID3D11Device1Trampoline *trampoline_device = new ID3D11Device1Trampoline();
	trampoline_device->lpVtbl = &trampoline_vtable;
	trampoline_device->orig_this = orig_device;

	install_hooks(orig_device);
	EnterCriticalSectionPretty(&device_map_lock);
	device_map[orig_device] = hacker_device;
	LeaveCriticalSection(&device_map_lock);

	return (ID3D11Device1*)trampoline_device;
}
