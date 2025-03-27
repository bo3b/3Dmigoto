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
// includes HackerDevice.hpp and HackerContext.hpp. As tempted as I am to just rip
// the vtable definition out into it's own header to get around that, but
// perhaps this will help decouple this from the rest of 3DMigoto.

#define CINTERFACE
#define D3D11_NO_HELPERS
#define COBJMACROS
#include <d3d11_1.h>
#undef COBJMACROS
#undef D3D11_NO_HELPERS
#undef CINTERFACE

#include "HookedDevice.h"

#include "DLLMainHook.h"
#include "Lock.h"
#include "log.h"

#include <unordered_map>
#include <Windows.h>

using std::unordered_map;

// -----------------------------------------------------------------------------

// Change this to 1 to enable debug logging of hooks and the trampolines back
// to the original device. Disabled by default as debug logging will already
// log most of these calls in the HackerDevice, so this would triple the noise
// and we probably won't need it very often:
#if 0
    #define HOOK_DEBUG LOG_DEBUG
#else
    #define HOOK_DEBUG(...) \
        do                  \
        {                   \
        } while (0)
#endif

// A map to look up the hacker device from the original device:
typedef unordered_map<ID3D11Device1*, ID3D11Device1*> DeviceMap;

static DeviceMap        device_map;
static CRITICAL_SECTION device_map_lock;

// Holds all the function pointers that we need to call into the real original
// device:
static struct ID3D11Device1Vtbl orig_vtable;

static bool hooks_installed = false;

// -----------------------------------------------------------------------------------------------
// IUnknown

static HRESULT STDMETHODCALLTYPE QueryInterface(
    ID3D11Device1* This,
    REFIID         riid,
    void**         ppvObject)
{
    ID3D11Device1* device = lookup_hooked_device(This);

    HOOK_DEBUG("HookedDevice::QueryInterface()\n");

    if (device)
        return ID3D11Device1_QueryInterface(device, riid, ppvObject);

    return orig_vtable.QueryInterface(This, riid, ppvObject);
}

static ULONG STDMETHODCALLTYPE AddRef(
    ID3D11Device1* This)
{
    ID3D11Device1* device = lookup_hooked_device(This);

    HOOK_DEBUG("HookedDevice::AddRef()\n");

    if (device)
        return ID3D11Device1_AddRef(device);

    return orig_vtable.AddRef(This);
}

static ULONG STDMETHODCALLTYPE Release(
    ID3D11Device1* This)
{
    DeviceMap::iterator i;
    ULONG               ref;

    HOOK_DEBUG("HookedDevice::Release()\n");

    ENTER_CRITICAL_SECTION(&device_map_lock);
    i = device_map.find(This);
    if (i != device_map.end())
    {
        ref = ID3D11Device1_Release(i->second);
        if (!ref)
            device_map.erase(i);
    }
    else
    {
        ref = orig_vtable.Release(This);
    }

    LEAVE_CRITICAL_SECTION(&device_map_lock);

    return ref;
}

// -----------------------------------------------------------------------------------------------
// ID3D11Device1

static HRESULT STDMETHODCALLTYPE CreateBuffer(
    ID3D11Device1*                This,
    const D3D11_BUFFER_DESC*      pDesc,
    const D3D11_SUBRESOURCE_DATA* pInitialData,
    ID3D11Buffer**                ppBuffer)
{
    ID3D11Device1* device = lookup_hooked_device(This);

    HOOK_DEBUG("HookedDevice::CreateBuffer()\n");

    if (device)
        return ID3D11Device1_CreateBuffer(device, pDesc, pInitialData, ppBuffer);

    return orig_vtable.CreateBuffer(This, pDesc, pInitialData, ppBuffer);
}

static HRESULT STDMETHODCALLTYPE CreateTexture1D(
    ID3D11Device1*                This,
    const D3D11_TEXTURE1D_DESC*   pDesc,
    const D3D11_SUBRESOURCE_DATA* pInitialData,
    ID3D11Texture1D**             ppTexture1D)
{
    ID3D11Device1* device = lookup_hooked_device(This);

    HOOK_DEBUG("HookedDevice::CreateTexture1D()\n");

    if (device)
        return ID3D11Device1_CreateTexture1D(device, pDesc, pInitialData, ppTexture1D);

    return orig_vtable.CreateTexture1D(This, pDesc, pInitialData, ppTexture1D);
}

static HRESULT STDMETHODCALLTYPE CreateTexture2D(
    ID3D11Device1*                This,
    const D3D11_TEXTURE2D_DESC*   pDesc,
    const D3D11_SUBRESOURCE_DATA* pInitialData,
    ID3D11Texture2D**             ppTexture2D)
{
    ID3D11Device1* device = lookup_hooked_device(This);

    HOOK_DEBUG("HookedDevice::CreateTexture2D()\n");

    if (device)
        return ID3D11Device1_CreateTexture2D(device, pDesc, pInitialData, ppTexture2D);

    return orig_vtable.CreateTexture2D(This, pDesc, pInitialData, ppTexture2D);
}

static HRESULT STDMETHODCALLTYPE CreateTexture3D(
    ID3D11Device1*                This,
    const D3D11_TEXTURE3D_DESC*   pDesc,
    const D3D11_SUBRESOURCE_DATA* pInitialData,
    ID3D11Texture3D**             ppTexture3D)
{
    ID3D11Device1* device = lookup_hooked_device(This);

    HOOK_DEBUG("HookedDevice::CreateTexture3D()\n");

    if (device)
        return ID3D11Device1_CreateTexture3D(device, pDesc, pInitialData, ppTexture3D);

    return orig_vtable.CreateTexture3D(This, pDesc, pInitialData, ppTexture3D);
}

static HRESULT STDMETHODCALLTYPE CreateShaderResourceView(
    ID3D11Device1*                         This,
    ID3D11Resource*                        pResource,
    const D3D11_SHADER_RESOURCE_VIEW_DESC* pDesc,
    ID3D11ShaderResourceView**             ppSRView)
{
    ID3D11Device1* device = lookup_hooked_device(This);

    HOOK_DEBUG("HookedDevice::CreateShaderResourceView()\n");

    if (device)
        return ID3D11Device1_CreateShaderResourceView(device, pResource, pDesc, ppSRView);

    return orig_vtable.CreateShaderResourceView(This, pResource, pDesc, ppSRView);
}

static HRESULT STDMETHODCALLTYPE CreateUnorderedAccessView(
    ID3D11Device1*                          This,
    ID3D11Resource*                         pResource,
    const D3D11_UNORDERED_ACCESS_VIEW_DESC* pDesc,
    ID3D11UnorderedAccessView**             ppUAView)
{
    ID3D11Device1* device = lookup_hooked_device(This);

    HOOK_DEBUG("HookedDevice::CreateUnorderedAccessView()\n");

    if (device)
        return ID3D11Device1_CreateUnorderedAccessView(device, pResource, pDesc, ppUAView);

    return orig_vtable.CreateUnorderedAccessView(This, pResource, pDesc, ppUAView);
}

static HRESULT STDMETHODCALLTYPE CreateRenderTargetView(
    ID3D11Device1*                       This,
    ID3D11Resource*                      pResource,
    const D3D11_RENDER_TARGET_VIEW_DESC* pDesc,
    ID3D11RenderTargetView**             ppRTView)
{
    ID3D11Device1* device = lookup_hooked_device(This);

    HOOK_DEBUG("HookedDevice::CreateRenderTargetView()\n");

    if (device)
        return ID3D11Device1_CreateRenderTargetView(device, pResource, pDesc, ppRTView);

    return orig_vtable.CreateRenderTargetView(This, pResource, pDesc, ppRTView);
}

static HRESULT STDMETHODCALLTYPE CreateDepthStencilView(
    ID3D11Device1*                       This,
    ID3D11Resource*                      pResource,
    const D3D11_DEPTH_STENCIL_VIEW_DESC* pDesc,
    ID3D11DepthStencilView**             ppDepthStencilView)
{
    ID3D11Device1* device = lookup_hooked_device(This);

    HOOK_DEBUG("HookedDevice::CreateDepthStencilView()\n");

    if (device)
        return ID3D11Device1_CreateDepthStencilView(device, pResource, pDesc, ppDepthStencilView);

    return orig_vtable.CreateDepthStencilView(This, pResource, pDesc, ppDepthStencilView);
}

static HRESULT STDMETHODCALLTYPE CreateInputLayout(
    ID3D11Device1*                  This,
    const D3D11_INPUT_ELEMENT_DESC* pInputElementDescs,
    UINT                            NumElements,
    const void*                     pShaderBytecodeWithInputSignature,
    SIZE_T                          BytecodeLength,
    ID3D11InputLayout**             ppInputLayout)
{
    ID3D11Device1* device = lookup_hooked_device(This);

    HOOK_DEBUG("HookedDevice::CreateInputLayout()\n");

    if (device)
        return ID3D11Device1_CreateInputLayout(device, pInputElementDescs, NumElements, pShaderBytecodeWithInputSignature, BytecodeLength, ppInputLayout);

    return orig_vtable.CreateInputLayout(This, pInputElementDescs, NumElements, pShaderBytecodeWithInputSignature, BytecodeLength, ppInputLayout);
}

static HRESULT STDMETHODCALLTYPE CreateVertexShader(
    ID3D11Device1*       This,
    const void*          pShaderBytecode,
    SIZE_T               BytecodeLength,
    ID3D11ClassLinkage*  pClassLinkage,
    ID3D11VertexShader** ppVertexShader)
{
    ID3D11Device1* device = lookup_hooked_device(This);

    HOOK_DEBUG("HookedDevice::CreateVertexShader()\n");

    if (device)
        return ID3D11Device1_CreateVertexShader(device, pShaderBytecode, BytecodeLength, pClassLinkage, ppVertexShader);

    return orig_vtable.CreateVertexShader(This, pShaderBytecode, BytecodeLength, pClassLinkage, ppVertexShader);
}

static HRESULT STDMETHODCALLTYPE CreateGeometryShader(
    ID3D11Device1*         This,
    const void*            pShaderBytecode,
    SIZE_T                 BytecodeLength,
    ID3D11ClassLinkage*    pClassLinkage,
    ID3D11GeometryShader** ppGeometryShader)
{
    ID3D11Device1* device = lookup_hooked_device(This);

    HOOK_DEBUG("HookedDevice::CreateGeometryShader()\n");

    if (device)
        return ID3D11Device1_CreateGeometryShader(device, pShaderBytecode, BytecodeLength, pClassLinkage, ppGeometryShader);

    return orig_vtable.CreateGeometryShader(This, pShaderBytecode, BytecodeLength, pClassLinkage, ppGeometryShader);
}

static HRESULT STDMETHODCALLTYPE CreateGeometryShaderWithStreamOutput(
    ID3D11Device1*                    This,
    const void*                       pShaderBytecode,
    SIZE_T                            BytecodeLength,
    const D3D11_SO_DECLARATION_ENTRY* pSODeclaration,
    UINT                              NumEntries,
    const UINT*                       pBufferStrides,
    UINT                              NumStrides,
    UINT                              RasterizedStream,
    ID3D11ClassLinkage*               pClassLinkage,
    ID3D11GeometryShader**            ppGeometryShader)
{
    ID3D11Device1* device = lookup_hooked_device(This);

    HOOK_DEBUG("HookedDevice::CreateGeometryShaderWithStreamOutput()\n");

    if (device)
        return ID3D11Device1_CreateGeometryShaderWithStreamOutput(device, pShaderBytecode, BytecodeLength, pSODeclaration, NumEntries, pBufferStrides, NumStrides, RasterizedStream, pClassLinkage, ppGeometryShader);

    return orig_vtable.CreateGeometryShaderWithStreamOutput(This, pShaderBytecode, BytecodeLength, pSODeclaration, NumEntries, pBufferStrides, NumStrides, RasterizedStream, pClassLinkage, ppGeometryShader);
}

static HRESULT STDMETHODCALLTYPE CreatePixelShader(
    ID3D11Device1*      This,
    const void*         pShaderBytecode,
    SIZE_T              BytecodeLength,
    ID3D11ClassLinkage* pClassLinkage,
    ID3D11PixelShader** ppPixelShader)
{
    ID3D11Device1* device = lookup_hooked_device(This);

    HOOK_DEBUG("HookedDevice::CreatePixelShader()\n");

    if (device)
        return ID3D11Device1_CreatePixelShader(device, pShaderBytecode, BytecodeLength, pClassLinkage, ppPixelShader);

    return orig_vtable.CreatePixelShader(This, pShaderBytecode, BytecodeLength, pClassLinkage, ppPixelShader);
}

static HRESULT STDMETHODCALLTYPE CreateHullShader(
    ID3D11Device1*      This,
    const void*         pShaderBytecode,
    SIZE_T              BytecodeLength,
    ID3D11ClassLinkage* pClassLinkage,
    ID3D11HullShader**  ppHullShader)
{
    ID3D11Device1* device = lookup_hooked_device(This);

    HOOK_DEBUG("HookedDevice::CreateHullShader()\n");

    if (device)
        return ID3D11Device1_CreateHullShader(device, pShaderBytecode, BytecodeLength, pClassLinkage, ppHullShader);

    return orig_vtable.CreateHullShader(This, pShaderBytecode, BytecodeLength, pClassLinkage, ppHullShader);
}

static HRESULT STDMETHODCALLTYPE CreateDomainShader(
    ID3D11Device1*       This,
    const void*          pShaderBytecode,
    SIZE_T               BytecodeLength,
    ID3D11ClassLinkage*  pClassLinkage,
    ID3D11DomainShader** ppDomainShader)
{
    ID3D11Device1* device = lookup_hooked_device(This);

    HOOK_DEBUG("HookedDevice::CreateDomainShader()\n");

    if (device)
        return ID3D11Device1_CreateDomainShader(device, pShaderBytecode, BytecodeLength, pClassLinkage, ppDomainShader);

    return orig_vtable.CreateDomainShader(This, pShaderBytecode, BytecodeLength, pClassLinkage, ppDomainShader);
}

static HRESULT STDMETHODCALLTYPE CreateComputeShader(
    ID3D11Device1*        This,
    const void*           pShaderBytecode,
    SIZE_T                BytecodeLength,
    ID3D11ClassLinkage*   pClassLinkage,
    ID3D11ComputeShader** ppComputeShader)
{
    ID3D11Device1* device = lookup_hooked_device(This);

    HOOK_DEBUG("HookedDevice::CreateComputeShader()\n");

    if (device)
        return ID3D11Device1_CreateComputeShader(device, pShaderBytecode, BytecodeLength, pClassLinkage, ppComputeShader);

    return orig_vtable.CreateComputeShader(This, pShaderBytecode, BytecodeLength, pClassLinkage, ppComputeShader);
}

static HRESULT STDMETHODCALLTYPE CreateClassLinkage(
    ID3D11Device1*       This,
    ID3D11ClassLinkage** ppLinkage)
{
    ID3D11Device1* device = lookup_hooked_device(This);

    HOOK_DEBUG("HookedDevice::CreateClassLinkage()\n");

    if (device)
        return ID3D11Device1_CreateClassLinkage(device, ppLinkage);

    return orig_vtable.CreateClassLinkage(This, ppLinkage);
}

static HRESULT STDMETHODCALLTYPE CreateBlendState(
    ID3D11Device1*          This,
    const D3D11_BLEND_DESC* pBlendStateDesc,
    ID3D11BlendState**      ppBlendState)
{
    ID3D11Device1* device = lookup_hooked_device(This);

    HOOK_DEBUG("HookedDevice::CreateBlendState()\n");

    if (device)
        return ID3D11Device1_CreateBlendState(device, pBlendStateDesc, ppBlendState);

    return orig_vtable.CreateBlendState(This, pBlendStateDesc, ppBlendState);
}

static HRESULT STDMETHODCALLTYPE CreateDepthStencilState(
    ID3D11Device1*                  This,
    const D3D11_DEPTH_STENCIL_DESC* pDepthStencilDesc,
    ID3D11DepthStencilState**       ppDepthStencilState)
{
    ID3D11Device1* device = lookup_hooked_device(This);

    HOOK_DEBUG("HookedDevice::CreateDepthStencilState()\n");

    if (device)
        return ID3D11Device1_CreateDepthStencilState(device, pDepthStencilDesc, ppDepthStencilState);

    return orig_vtable.CreateDepthStencilState(This, pDepthStencilDesc, ppDepthStencilState);
}

static HRESULT STDMETHODCALLTYPE CreateRasterizerState(
    ID3D11Device1*               This,
    const D3D11_RASTERIZER_DESC* pRasterizerDesc,
    ID3D11RasterizerState**      ppRasterizerState)
{
    ID3D11Device1* device = lookup_hooked_device(This);

    HOOK_DEBUG("HookedDevice::CreateRasterizerState()\n");

    if (device)
        return ID3D11Device1_CreateRasterizerState(device, pRasterizerDesc, ppRasterizerState);

    return orig_vtable.CreateRasterizerState(This, pRasterizerDesc, ppRasterizerState);
}

static HRESULT STDMETHODCALLTYPE CreateSamplerState(
    ID3D11Device1*            This,
    const D3D11_SAMPLER_DESC* pSamplerDesc,
    ID3D11SamplerState**      ppSamplerState)
{
    ID3D11Device1* device = lookup_hooked_device(This);

    HOOK_DEBUG("HookedDevice::CreateSamplerState()\n");

    if (device)
        return ID3D11Device1_CreateSamplerState(device, pSamplerDesc, ppSamplerState);

    return orig_vtable.CreateSamplerState(This, pSamplerDesc, ppSamplerState);
}

static HRESULT STDMETHODCALLTYPE CreateQuery(
    ID3D11Device1*          This,
    const D3D11_QUERY_DESC* pQueryDesc,
    ID3D11Query**           ppQuery)
{
    ID3D11Device1* device = lookup_hooked_device(This);

    HOOK_DEBUG("HookedDevice::CreateQuery()\n");

    if (device)
        return ID3D11Device1_CreateQuery(device, pQueryDesc, ppQuery);

    return orig_vtable.CreateQuery(This, pQueryDesc, ppQuery);
}

static HRESULT STDMETHODCALLTYPE CreatePredicate(
    ID3D11Device1*          This,
    const D3D11_QUERY_DESC* pPredicateDesc,
    ID3D11Predicate**       ppPredicate)
{
    ID3D11Device1* device = lookup_hooked_device(This);

    HOOK_DEBUG("HookedDevice::CreatePredicate()\n");

    if (device)
        return ID3D11Device1_CreatePredicate(device, pPredicateDesc, ppPredicate);

    return orig_vtable.CreatePredicate(This, pPredicateDesc, ppPredicate);
}

static HRESULT STDMETHODCALLTYPE CreateCounter(
    ID3D11Device1*            This,
    const D3D11_COUNTER_DESC* pCounterDesc,
    ID3D11Counter**           ppCounter)
{
    ID3D11Device1* device = lookup_hooked_device(This);

    HOOK_DEBUG("HookedDevice::CreateCounter()\n");

    if (device)
        return ID3D11Device1_CreateCounter(device, pCounterDesc, ppCounter);

    return orig_vtable.CreateCounter(This, pCounterDesc, ppCounter);
}

static HRESULT STDMETHODCALLTYPE CreateDeferredContext(
    ID3D11Device1*        This,
    UINT                  ContextFlags,
    ID3D11DeviceContext** ppDeferredContext)
{
    ID3D11Device1* device = lookup_hooked_device(This);

    HOOK_DEBUG("HookedDevice::CreateDeferredContext()\n");

    if (device)
        return ID3D11Device1_CreateDeferredContext(device, ContextFlags, ppDeferredContext);

    return orig_vtable.CreateDeferredContext(This, ContextFlags, ppDeferredContext);
}

static HRESULT STDMETHODCALLTYPE OpenSharedResource(
    ID3D11Device1* This,
    HANDLE         hResource,
    REFIID         ReturnedInterface,
    void**         ppResource)
{
    ID3D11Device1* device = lookup_hooked_device(This);

    HOOK_DEBUG("HookedDevice::OpenSharedResource()\n");

    if (device)
        return ID3D11Device1_OpenSharedResource(device, hResource, ReturnedInterface, ppResource);

    return orig_vtable.OpenSharedResource(This, hResource, ReturnedInterface, ppResource);
}

static HRESULT STDMETHODCALLTYPE CheckFormatSupport(
    ID3D11Device1* This,
    DXGI_FORMAT    Format,
    UINT*          pFormatSupport)
{
    ID3D11Device1* device = lookup_hooked_device(This);

    HOOK_DEBUG("HookedDevice::CheckFormatSupport()\n");

    if (device)
        return ID3D11Device1_CheckFormatSupport(device, Format, pFormatSupport);

    return orig_vtable.CheckFormatSupport(This, Format, pFormatSupport);
}

static HRESULT STDMETHODCALLTYPE CheckMultisampleQualityLevels(
    ID3D11Device1* This,
    DXGI_FORMAT    Format,
    UINT           SampleCount,
    UINT*          pNumQualityLevels)
{
    ID3D11Device1* device = lookup_hooked_device(This);

    HOOK_DEBUG("HookedDevice::CheckMultisampleQualityLevels()\n");

    if (device)
        return ID3D11Device1_CheckMultisampleQualityLevels(device, Format, SampleCount, pNumQualityLevels);

    return orig_vtable.CheckMultisampleQualityLevels(This, Format, SampleCount, pNumQualityLevels);
}

static void STDMETHODCALLTYPE CheckCounterInfo(
    ID3D11Device1*      This,
    D3D11_COUNTER_INFO* pCounterInfo)
{
    ID3D11Device1* device = lookup_hooked_device(This);

    HOOK_DEBUG("HookedDevice::CheckCounterInfo()\n");

    if (device)
        return ID3D11Device1_CheckCounterInfo(device, pCounterInfo);

    return orig_vtable.CheckCounterInfo(This, pCounterInfo);
}

static HRESULT STDMETHODCALLTYPE CheckCounter(
    ID3D11Device1*            This,
    const D3D11_COUNTER_DESC* pDesc,
    D3D11_COUNTER_TYPE*       pType,
    UINT*                     pActiveCounters,
    LPSTR                     szName,
    UINT*                     pNameLength,
    LPSTR                     szUnits,
    UINT*                     pUnitsLength,
    LPSTR                     szDescription,
    UINT*                     pDescriptionLength)
{
    ID3D11Device1* device = lookup_hooked_device(This);

    HOOK_DEBUG("HookedDevice::CheckCounter()\n");

    if (device)
        return ID3D11Device1_CheckCounter(device, pDesc, pType, pActiveCounters, szName, pNameLength, szUnits, pUnitsLength, szDescription, pDescriptionLength);

    return orig_vtable.CheckCounter(This, pDesc, pType, pActiveCounters, szName, pNameLength, szUnits, pUnitsLength, szDescription, pDescriptionLength);
}

static HRESULT STDMETHODCALLTYPE CheckFeatureSupport(
    ID3D11Device1* This,
    D3D11_FEATURE  Feature,
    void*          pFeatureSupportData,
    UINT           FeatureSupportDataSize)
{
    ID3D11Device1* device = lookup_hooked_device(This);

    HOOK_DEBUG("HookedDevice::CheckFeatureSupport()\n");

    if (device)
        return ID3D11Device1_CheckFeatureSupport(device, Feature, pFeatureSupportData, FeatureSupportDataSize);

    return orig_vtable.CheckFeatureSupport(This, Feature, pFeatureSupportData, FeatureSupportDataSize);
}

static HRESULT STDMETHODCALLTYPE GetPrivateData(
    ID3D11Device1* This,
    REFGUID        guid,
    UINT*          pDataSize,
    void*          pData)
{
    ID3D11Device1* device = lookup_hooked_device(This);

    HOOK_DEBUG("HookedDevice::GetPrivateData()\n");

    if (device)
        return ID3D11Device1_GetPrivateData(device, guid, pDataSize, pData);

    return orig_vtable.GetPrivateData(This, guid, pDataSize, pData);
}

static HRESULT STDMETHODCALLTYPE SetPrivateData(
    ID3D11Device1* This,
    REFGUID        guid,
    UINT           DataSize,
    const void*    pData)
{
    ID3D11Device1* device = lookup_hooked_device(This);

    HOOK_DEBUG("HookedDevice::SetPrivateData()\n");

    if (device)
        return ID3D11Device1_SetPrivateData(device, guid, DataSize, pData);

    return orig_vtable.SetPrivateData(This, guid, DataSize, pData);
}

static HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(
    ID3D11Device1*  This,
    REFGUID         guid,
    const IUnknown* pData)
{
    ID3D11Device1* device = lookup_hooked_device(This);

    HOOK_DEBUG("HookedDevice::SetPrivateDataInterface()\n");

    if (device)
        return ID3D11Device1_SetPrivateDataInterface(device, guid, pData);

    return orig_vtable.SetPrivateDataInterface(This, guid, pData);
}

static D3D_FEATURE_LEVEL STDMETHODCALLTYPE GetFeatureLevel(
    ID3D11Device1* This)
{
    ID3D11Device1* device = lookup_hooked_device(This);

    HOOK_DEBUG("HookedDevice::GetFeatureLevel()\n");

    if (device)
        return ID3D11Device1_GetFeatureLevel(device);

    return orig_vtable.GetFeatureLevel(This);
}

static UINT STDMETHODCALLTYPE GetCreationFlags(
    ID3D11Device1* This)
{
    ID3D11Device1* device = lookup_hooked_device(This);

    HOOK_DEBUG("HookedDevice::GetCreationFlags()\n");

    if (device)
        return ID3D11Device1_GetCreationFlags(device);

    return orig_vtable.GetCreationFlags(This);
}

static HRESULT STDMETHODCALLTYPE GetDeviceRemovedReason(
    ID3D11Device1* This)
{
    ID3D11Device1* device = lookup_hooked_device(This);

    HOOK_DEBUG("HookedDevice::GetDeviceRemovedReason()\n");

    if (device)
        return ID3D11Device1_GetDeviceRemovedReason(device);

    return orig_vtable.GetDeviceRemovedReason(This);
}

static void STDMETHODCALLTYPE GetImmediateContext(
    ID3D11Device1*        This,
    ID3D11DeviceContext** ppImmediateContext)
{
    ID3D11Device1* device = lookup_hooked_device(This);

    HOOK_DEBUG("HookedDevice::GetImmediateContext()\n");

    if (device)
        return ID3D11Device1_GetImmediateContext(device, ppImmediateContext);

    return orig_vtable.GetImmediateContext(This, ppImmediateContext);
}

static HRESULT STDMETHODCALLTYPE SetExceptionMode(
    ID3D11Device1* This,
    UINT           RaiseFlags)
{
    ID3D11Device1* device = lookup_hooked_device(This);

    HOOK_DEBUG("HookedDevice::SetExceptionMode()\n");

    if (device)
        return ID3D11Device1_SetExceptionMode(device, RaiseFlags);

    return orig_vtable.SetExceptionMode(This, RaiseFlags);
}

static UINT STDMETHODCALLTYPE GetExceptionMode(
    ID3D11Device1* This)
{
    ID3D11Device1* device = lookup_hooked_device(This);

    HOOK_DEBUG("HookedDevice::GetExceptionMode()\n");

    if (device)
        return ID3D11Device1_GetExceptionMode(device);

    return orig_vtable.GetExceptionMode(This);
}

static void install_hooks(
    ID3D11Device1* device)
{
    SIZE_T hook_id;

    // Hooks should only be installed once as they will affect all contexts
    if (hooks_installed)
        return;
    INIT_CRITICAL_SECTION(&device_map_lock);
    hooks_installed = true;

    // Make sure that everything in the orig_vtable is filled in just in
    // case we miss one of the hooks below:
    memcpy(&orig_vtable, device->lpVtbl, sizeof(struct ID3D11Device1Vtbl));

    // clang-format off
    // At the moment we are just throwing away the hook IDs - we should
    // probably hold on to them incase we need to remove the hooks later:
    cHookMgr.Hook(&hook_id, reinterpret_cast<void**>(&orig_vtable.QueryInterface),                       device->lpVtbl->QueryInterface,                       QueryInterface);
    cHookMgr.Hook(&hook_id, reinterpret_cast<void**>(&orig_vtable.AddRef),                               device->lpVtbl->AddRef,                               AddRef);
    cHookMgr.Hook(&hook_id, reinterpret_cast<void**>(&orig_vtable.Release),                              device->lpVtbl->Release,                              Release);
    cHookMgr.Hook(&hook_id, reinterpret_cast<void**>(&orig_vtable.CreateBuffer),                         device->lpVtbl->CreateBuffer,                         CreateBuffer);
    cHookMgr.Hook(&hook_id, reinterpret_cast<void**>(&orig_vtable.CreateTexture1D),                      device->lpVtbl->CreateTexture1D,                      CreateTexture1D);
    cHookMgr.Hook(&hook_id, reinterpret_cast<void**>(&orig_vtable.CreateTexture2D),                      device->lpVtbl->CreateTexture2D,                      CreateTexture2D);
    cHookMgr.Hook(&hook_id, reinterpret_cast<void**>(&orig_vtable.CreateTexture3D),                      device->lpVtbl->CreateTexture3D,                      CreateTexture3D);
    cHookMgr.Hook(&hook_id, reinterpret_cast<void**>(&orig_vtable.CreateShaderResourceView),             device->lpVtbl->CreateShaderResourceView,             CreateShaderResourceView);
    cHookMgr.Hook(&hook_id, reinterpret_cast<void**>(&orig_vtable.CreateUnorderedAccessView),            device->lpVtbl->CreateUnorderedAccessView,            CreateUnorderedAccessView);
    cHookMgr.Hook(&hook_id, reinterpret_cast<void**>(&orig_vtable.CreateRenderTargetView),               device->lpVtbl->CreateRenderTargetView,               CreateRenderTargetView);
    cHookMgr.Hook(&hook_id, reinterpret_cast<void**>(&orig_vtable.CreateDepthStencilView),               device->lpVtbl->CreateDepthStencilView,               CreateDepthStencilView);
    cHookMgr.Hook(&hook_id, reinterpret_cast<void**>(&orig_vtable.CreateInputLayout),                    device->lpVtbl->CreateInputLayout,                    CreateInputLayout);
    cHookMgr.Hook(&hook_id, reinterpret_cast<void**>(&orig_vtable.CreateVertexShader),                   device->lpVtbl->CreateVertexShader,                   CreateVertexShader);
    cHookMgr.Hook(&hook_id, reinterpret_cast<void**>(&orig_vtable.CreateGeometryShader),                 device->lpVtbl->CreateGeometryShader,                 CreateGeometryShader);
    cHookMgr.Hook(&hook_id, reinterpret_cast<void**>(&orig_vtable.CreateGeometryShaderWithStreamOutput), device->lpVtbl->CreateGeometryShaderWithStreamOutput, CreateGeometryShaderWithStreamOutput);
    cHookMgr.Hook(&hook_id, reinterpret_cast<void**>(&orig_vtable.CreatePixelShader),                    device->lpVtbl->CreatePixelShader,                    CreatePixelShader);
    cHookMgr.Hook(&hook_id, reinterpret_cast<void**>(&orig_vtable.CreateHullShader),                     device->lpVtbl->CreateHullShader,                     CreateHullShader);
    cHookMgr.Hook(&hook_id, reinterpret_cast<void**>(&orig_vtable.CreateDomainShader),                   device->lpVtbl->CreateDomainShader,                   CreateDomainShader);
    cHookMgr.Hook(&hook_id, reinterpret_cast<void**>(&orig_vtable.CreateComputeShader),                  device->lpVtbl->CreateComputeShader,                  CreateComputeShader);
    cHookMgr.Hook(&hook_id, reinterpret_cast<void**>(&orig_vtable.CreateClassLinkage),                   device->lpVtbl->CreateClassLinkage,                   CreateClassLinkage);
    cHookMgr.Hook(&hook_id, reinterpret_cast<void**>(&orig_vtable.CreateBlendState),                     device->lpVtbl->CreateBlendState,                     CreateBlendState);
    cHookMgr.Hook(&hook_id, reinterpret_cast<void**>(&orig_vtable.CreateDepthStencilState),              device->lpVtbl->CreateDepthStencilState,              CreateDepthStencilState);
    cHookMgr.Hook(&hook_id, reinterpret_cast<void**>(&orig_vtable.CreateRasterizerState),                device->lpVtbl->CreateRasterizerState,                CreateRasterizerState);
    cHookMgr.Hook(&hook_id, reinterpret_cast<void**>(&orig_vtable.CreateSamplerState),                   device->lpVtbl->CreateSamplerState,                   CreateSamplerState);
    cHookMgr.Hook(&hook_id, reinterpret_cast<void**>(&orig_vtable.CreateQuery),                          device->lpVtbl->CreateQuery,                          CreateQuery);
    cHookMgr.Hook(&hook_id, reinterpret_cast<void**>(&orig_vtable.CreatePredicate),                      device->lpVtbl->CreatePredicate,                      CreatePredicate);
    cHookMgr.Hook(&hook_id, reinterpret_cast<void**>(&orig_vtable.CreateCounter),                        device->lpVtbl->CreateCounter,                        CreateCounter);
    cHookMgr.Hook(&hook_id, reinterpret_cast<void**>(&orig_vtable.CreateDeferredContext),                device->lpVtbl->CreateDeferredContext,                CreateDeferredContext);
    cHookMgr.Hook(&hook_id, reinterpret_cast<void**>(&orig_vtable.OpenSharedResource),                   device->lpVtbl->OpenSharedResource,                   OpenSharedResource);
    cHookMgr.Hook(&hook_id, reinterpret_cast<void**>(&orig_vtable.CheckFormatSupport),                   device->lpVtbl->CheckFormatSupport,                   CheckFormatSupport);
    cHookMgr.Hook(&hook_id, reinterpret_cast<void**>(&orig_vtable.CheckMultisampleQualityLevels),        device->lpVtbl->CheckMultisampleQualityLevels,        CheckMultisampleQualityLevels);
    cHookMgr.Hook(&hook_id, reinterpret_cast<void**>(&orig_vtable.CheckCounterInfo),                     device->lpVtbl->CheckCounterInfo,                     CheckCounterInfo);
    cHookMgr.Hook(&hook_id, reinterpret_cast<void**>(&orig_vtable.CheckCounter),                         device->lpVtbl->CheckCounter,                         CheckCounter);
    cHookMgr.Hook(&hook_id, reinterpret_cast<void**>(&orig_vtable.CheckFeatureSupport),                  device->lpVtbl->CheckFeatureSupport,                  CheckFeatureSupport);
    cHookMgr.Hook(&hook_id, reinterpret_cast<void**>(&orig_vtable.GetPrivateData),                       device->lpVtbl->GetPrivateData,                       GetPrivateData);
    cHookMgr.Hook(&hook_id, reinterpret_cast<void**>(&orig_vtable.SetPrivateData),                       device->lpVtbl->SetPrivateData,                       SetPrivateData);
    cHookMgr.Hook(&hook_id, reinterpret_cast<void**>(&orig_vtable.SetPrivateDataInterface),              device->lpVtbl->SetPrivateDataInterface,              SetPrivateDataInterface);
    cHookMgr.Hook(&hook_id, reinterpret_cast<void**>(&orig_vtable.GetFeatureLevel),                      device->lpVtbl->GetFeatureLevel,                      GetFeatureLevel);
    cHookMgr.Hook(&hook_id, reinterpret_cast<void**>(&orig_vtable.GetCreationFlags),                     device->lpVtbl->GetCreationFlags,                     GetCreationFlags);
    cHookMgr.Hook(&hook_id, reinterpret_cast<void**>(&orig_vtable.GetDeviceRemovedReason),               device->lpVtbl->GetDeviceRemovedReason,               GetDeviceRemovedReason);
    cHookMgr.Hook(&hook_id, reinterpret_cast<void**>(&orig_vtable.GetImmediateContext),                  device->lpVtbl->GetImmediateContext,                  GetImmediateContext);
    cHookMgr.Hook(&hook_id, reinterpret_cast<void**>(&orig_vtable.SetExceptionMode),                     device->lpVtbl->SetExceptionMode,                     SetExceptionMode);
    cHookMgr.Hook(&hook_id, reinterpret_cast<void**>(&orig_vtable.GetExceptionMode),                     device->lpVtbl->GetExceptionMode,                     GetExceptionMode);
    // clang-format on
}

// This provides another ID3D11Device1 interface for calling the original
// functions in orig_vtable. This replaces mOrigDevice in the HackerDevice and
// elsewhere and gives us a way to call back into the game with minimal code
// changes.
typedef struct
{
    CONST_VTBL struct ID3D11Device1Vtbl* lpVtbl;
    ID3D11Device1*                       orig_this;
} ID3D11Device1_trampoline;

// -----------------------------------------------------------------------------------------------
// IUnknown

static HRESULT STDMETHODCALLTYPE trampoline_QueryInterface(
    ID3D11Device1* This,
    REFIID         riid,
    void**         ppvObject)
{
    HOOK_DEBUG("TrampolineDevice::QueryInterface()\n");
    return orig_vtable.QueryInterface(reinterpret_cast<ID3D11Device1_trampoline*>(This)->orig_this, riid, ppvObject);
}
static ULONG STDMETHODCALLTYPE trampoline_AddRef(
    ID3D11Device1* This)
{
    HOOK_DEBUG("TrampolineDevice::AddRef()\n");
    return orig_vtable.AddRef(reinterpret_cast<ID3D11Device1_trampoline*>(This)->orig_this);
}
static ULONG STDMETHODCALLTYPE trampoline_Release(
    ID3D11Device1* This)
{
    ULONG ref;

    HOOK_DEBUG("TrampolineDevice::Release()\n");
    ref = orig_vtable.Release(reinterpret_cast<ID3D11Device1_trampoline*>(This)->orig_this);

    if (!ref)
        delete This;

    return ref;
}

// -----------------------------------------------------------------------------------------------
// ID3D11Device1

static HRESULT STDMETHODCALLTYPE trampoline_CreateBuffer(
    ID3D11Device1*                This,
    const D3D11_BUFFER_DESC*      pDesc,
    const D3D11_SUBRESOURCE_DATA* pInitialData,
    ID3D11Buffer**                ppBuffer)
{
    HOOK_DEBUG("TrampolineDevice::CreateBuffer()\n");
    return orig_vtable.CreateBuffer(reinterpret_cast<ID3D11Device1_trampoline*>(This)->orig_this, pDesc, pInitialData, ppBuffer);
}
static HRESULT STDMETHODCALLTYPE trampoline_CreateTexture1D(
    ID3D11Device1*                This,
    const D3D11_TEXTURE1D_DESC*   pDesc,
    const D3D11_SUBRESOURCE_DATA* pInitialData,
    ID3D11Texture1D**             ppTexture1D)
{
    HOOK_DEBUG("TrampolineDevice::CreateTexture1D()\n");
    return orig_vtable.CreateTexture1D(reinterpret_cast<ID3D11Device1_trampoline*>(This)->orig_this, pDesc, pInitialData, ppTexture1D);
}
static HRESULT STDMETHODCALLTYPE trampoline_CreateTexture2D(
    ID3D11Device1*                This,
    const D3D11_TEXTURE2D_DESC*   pDesc,
    const D3D11_SUBRESOURCE_DATA* pInitialData,
    ID3D11Texture2D**             ppTexture2D)
{
    HOOK_DEBUG("TrampolineDevice::CreateTexture2D()\n");
    return orig_vtable.CreateTexture2D(reinterpret_cast<ID3D11Device1_trampoline*>(This)->orig_this, pDesc, pInitialData, ppTexture2D);
}
static HRESULT STDMETHODCALLTYPE trampoline_CreateTexture3D(
    ID3D11Device1*                This,
    const D3D11_TEXTURE3D_DESC*   pDesc,
    const D3D11_SUBRESOURCE_DATA* pInitialData,
    ID3D11Texture3D**             ppTexture3D)
{
    HOOK_DEBUG("TrampolineDevice::CreateTexture3D()\n");
    return orig_vtable.CreateTexture3D(reinterpret_cast<ID3D11Device1_trampoline*>(This)->orig_this, pDesc, pInitialData, ppTexture3D);
}
static HRESULT STDMETHODCALLTYPE trampoline_CreateShaderResourceView(
    ID3D11Device1*                         This,
    ID3D11Resource*                        pResource,
    const D3D11_SHADER_RESOURCE_VIEW_DESC* pDesc,
    ID3D11ShaderResourceView**             ppSRView)
{
    HOOK_DEBUG("TrampolineDevice::CreateShaderResourceView()\n");
    return orig_vtable.CreateShaderResourceView(reinterpret_cast<ID3D11Device1_trampoline*>(This)->orig_this, pResource, pDesc, ppSRView);
}
static HRESULT STDMETHODCALLTYPE trampoline_CreateUnorderedAccessView(
    ID3D11Device1*                          This,
    ID3D11Resource*                         pResource,
    const D3D11_UNORDERED_ACCESS_VIEW_DESC* pDesc,
    ID3D11UnorderedAccessView**             ppUAView)
{
    HOOK_DEBUG("TrampolineDevice::CreateUnorderedAccessView()\n");
    return orig_vtable.CreateUnorderedAccessView(reinterpret_cast<ID3D11Device1_trampoline*>(This)->orig_this, pResource, pDesc, ppUAView);
}
static HRESULT STDMETHODCALLTYPE trampoline_CreateRenderTargetView(
    ID3D11Device1*                       This,
    ID3D11Resource*                      pResource,
    const D3D11_RENDER_TARGET_VIEW_DESC* pDesc,
    ID3D11RenderTargetView**             ppRTView)
{
    HOOK_DEBUG("TrampolineDevice::CreateRenderTargetView()\n");
    return orig_vtable.CreateRenderTargetView(reinterpret_cast<ID3D11Device1_trampoline*>(This)->orig_this, pResource, pDesc, ppRTView);
}
static HRESULT STDMETHODCALLTYPE trampoline_CreateDepthStencilView(
    ID3D11Device1*                       This,
    ID3D11Resource*                      pResource,
    const D3D11_DEPTH_STENCIL_VIEW_DESC* pDesc,
    ID3D11DepthStencilView**             ppDepthStencilView)
{
    HOOK_DEBUG("TrampolineDevice::CreateDepthStencilView()\n");
    return orig_vtable.CreateDepthStencilView(reinterpret_cast<ID3D11Device1_trampoline*>(This)->orig_this, pResource, pDesc, ppDepthStencilView);
}
static HRESULT STDMETHODCALLTYPE trampoline_CreateInputLayout(
    ID3D11Device1*                  This,
    const D3D11_INPUT_ELEMENT_DESC* pInputElementDescs,
    UINT                            NumElements,
    const void*                     pShaderBytecodeWithInputSignature,
    SIZE_T                          BytecodeLength,
    ID3D11InputLayout**             ppInputLayout)
{
    HOOK_DEBUG("TrampolineDevice::CreateInputLayout()\n");
    return orig_vtable.CreateInputLayout(reinterpret_cast<ID3D11Device1_trampoline*>(This)->orig_this, pInputElementDescs, NumElements, pShaderBytecodeWithInputSignature, BytecodeLength, ppInputLayout);
}
static HRESULT STDMETHODCALLTYPE trampoline_CreateVertexShader(
    ID3D11Device1*       This,
    const void*          pShaderBytecode,
    SIZE_T               BytecodeLength,
    ID3D11ClassLinkage*  pClassLinkage,
    ID3D11VertexShader** ppVertexShader)
{
    HOOK_DEBUG("TrampolineDevice::CreateVertexShader()\n");
    return orig_vtable.CreateVertexShader(reinterpret_cast<ID3D11Device1_trampoline*>(This)->orig_this, pShaderBytecode, BytecodeLength, pClassLinkage, ppVertexShader);
}
static HRESULT STDMETHODCALLTYPE trampoline_CreateGeometryShader(
    ID3D11Device1*         This,
    const void*            pShaderBytecode,
    SIZE_T                 BytecodeLength,
    ID3D11ClassLinkage*    pClassLinkage,
    ID3D11GeometryShader** ppGeometryShader)
{
    HOOK_DEBUG("TrampolineDevice::CreateGeometryShader()\n");
    return orig_vtable.CreateGeometryShader(reinterpret_cast<ID3D11Device1_trampoline*>(This)->orig_this, pShaderBytecode, BytecodeLength, pClassLinkage, ppGeometryShader);
}
static HRESULT STDMETHODCALLTYPE trampoline_CreateGeometryShaderWithStreamOutput(
    ID3D11Device1*                    This,
    const void*                       pShaderBytecode,
    SIZE_T                            BytecodeLength,
    const D3D11_SO_DECLARATION_ENTRY* pSODeclaration,
    UINT                              NumEntries,
    const UINT*                       pBufferStrides,
    UINT                              NumStrides,
    UINT                              RasterizedStream,
    ID3D11ClassLinkage*               pClassLinkage,
    ID3D11GeometryShader**            ppGeometryShader)
{
    HOOK_DEBUG("TrampolineDevice::CreateGeometryShaderWithStreamOutput()\n");
    return orig_vtable.CreateGeometryShaderWithStreamOutput(reinterpret_cast<ID3D11Device1_trampoline*>(This)->orig_this, pShaderBytecode, BytecodeLength, pSODeclaration, NumEntries, pBufferStrides, NumStrides, RasterizedStream, pClassLinkage, ppGeometryShader);
}
static HRESULT STDMETHODCALLTYPE trampoline_CreatePixelShader(
    ID3D11Device1*      This,
    const void*         pShaderBytecode,
    SIZE_T              BytecodeLength,
    ID3D11ClassLinkage* pClassLinkage,
    ID3D11PixelShader** ppPixelShader)
{
    HOOK_DEBUG("TrampolineDevice::CreatePixelShader()\n");
    return orig_vtable.CreatePixelShader(reinterpret_cast<ID3D11Device1_trampoline*>(This)->orig_this, pShaderBytecode, BytecodeLength, pClassLinkage, ppPixelShader);
}
static HRESULT STDMETHODCALLTYPE trampoline_CreateHullShader(
    ID3D11Device1*      This,
    const void*         pShaderBytecode,
    SIZE_T              BytecodeLength,
    ID3D11ClassLinkage* pClassLinkage,
    ID3D11HullShader**  ppHullShader)
{
    HOOK_DEBUG("TrampolineDevice::CreateHullShader()\n");
    return orig_vtable.CreateHullShader(reinterpret_cast<ID3D11Device1_trampoline*>(This)->orig_this, pShaderBytecode, BytecodeLength, pClassLinkage, ppHullShader);
}
static HRESULT STDMETHODCALLTYPE trampoline_CreateDomainShader(
    ID3D11Device1*       This,
    const void*          pShaderBytecode,
    SIZE_T               BytecodeLength,
    ID3D11ClassLinkage*  pClassLinkage,
    ID3D11DomainShader** ppDomainShader)
{
    HOOK_DEBUG("TrampolineDevice::CreateDomainShader()\n");
    return orig_vtable.CreateDomainShader(reinterpret_cast<ID3D11Device1_trampoline*>(This)->orig_this, pShaderBytecode, BytecodeLength, pClassLinkage, ppDomainShader);
}
static HRESULT STDMETHODCALLTYPE trampoline_CreateComputeShader(
    ID3D11Device1*        This,
    const void*           pShaderBytecode,
    SIZE_T                BytecodeLength,
    ID3D11ClassLinkage*   pClassLinkage,
    ID3D11ComputeShader** ppComputeShader)
{
    HOOK_DEBUG("TrampolineDevice::CreateComputeShader()\n");
    return orig_vtable.CreateComputeShader(reinterpret_cast<ID3D11Device1_trampoline*>(This)->orig_this, pShaderBytecode, BytecodeLength, pClassLinkage, ppComputeShader);
}
static HRESULT STDMETHODCALLTYPE trampoline_CreateClassLinkage(
    ID3D11Device1*       This,
    ID3D11ClassLinkage** ppLinkage)
{
    HOOK_DEBUG("TrampolineDevice::CreateClassLinkage()\n");
    return orig_vtable.CreateClassLinkage(reinterpret_cast<ID3D11Device1_trampoline*>(This)->orig_this, ppLinkage);
}
static HRESULT STDMETHODCALLTYPE trampoline_CreateBlendState(
    ID3D11Device1*          This,
    const D3D11_BLEND_DESC* pBlendStateDesc,
    ID3D11BlendState**      ppBlendState)
{
    HOOK_DEBUG("TrampolineDevice::CreateBlendState()\n");
    return orig_vtable.CreateBlendState(reinterpret_cast<ID3D11Device1_trampoline*>(This)->orig_this, pBlendStateDesc, ppBlendState);
}
static HRESULT STDMETHODCALLTYPE trampoline_CreateDepthStencilState(
    ID3D11Device1*                  This,
    const D3D11_DEPTH_STENCIL_DESC* pDepthStencilDesc,
    ID3D11DepthStencilState**       ppDepthStencilState)
{
    HOOK_DEBUG("TrampolineDevice::CreateDepthStencilState()\n");
    return orig_vtable.CreateDepthStencilState(reinterpret_cast<ID3D11Device1_trampoline*>(This)->orig_this, pDepthStencilDesc, ppDepthStencilState);
}
static HRESULT STDMETHODCALLTYPE trampoline_CreateRasterizerState(
    ID3D11Device1*               This,
    const D3D11_RASTERIZER_DESC* pRasterizerDesc,
    ID3D11RasterizerState**      ppRasterizerState)
{
    HOOK_DEBUG("TrampolineDevice::CreateRasterizerState()\n");
    return orig_vtable.CreateRasterizerState(reinterpret_cast<ID3D11Device1_trampoline*>(This)->orig_this, pRasterizerDesc, ppRasterizerState);
}
static HRESULT STDMETHODCALLTYPE trampoline_CreateSamplerState(
    ID3D11Device1*            This,
    const D3D11_SAMPLER_DESC* pSamplerDesc,
    ID3D11SamplerState**      ppSamplerState)
{
    HOOK_DEBUG("TrampolineDevice::CreateSamplerState()\n");
    return orig_vtable.CreateSamplerState(reinterpret_cast<ID3D11Device1_trampoline*>(This)->orig_this, pSamplerDesc, ppSamplerState);
}
static HRESULT STDMETHODCALLTYPE trampoline_CreateQuery(
    ID3D11Device1*          This,
    const D3D11_QUERY_DESC* pQueryDesc,
    ID3D11Query**           ppQuery)
{
    HOOK_DEBUG("TrampolineDevice::CreateQuery()\n");
    return orig_vtable.CreateQuery(reinterpret_cast<ID3D11Device1_trampoline*>(This)->orig_this, pQueryDesc, ppQuery);
}
static HRESULT STDMETHODCALLTYPE trampoline_CreatePredicate(
    ID3D11Device1*          This,
    const D3D11_QUERY_DESC* pPredicateDesc,
    ID3D11Predicate**       ppPredicate)
{
    HOOK_DEBUG("TrampolineDevice::CreatePredicate()\n");
    return orig_vtable.CreatePredicate(reinterpret_cast<ID3D11Device1_trampoline*>(This)->orig_this, pPredicateDesc, ppPredicate);
}
static HRESULT STDMETHODCALLTYPE trampoline_CreateCounter(
    ID3D11Device1*            This,
    const D3D11_COUNTER_DESC* pCounterDesc,
    ID3D11Counter**           ppCounter)
{
    HOOK_DEBUG("TrampolineDevice::CreateCounter()\n");
    return orig_vtable.CreateCounter(reinterpret_cast<ID3D11Device1_trampoline*>(This)->orig_this, pCounterDesc, ppCounter);
}
static HRESULT STDMETHODCALLTYPE trampoline_CreateDeferredContext(
    ID3D11Device1*        This,
    UINT                  ContextFlags,
    ID3D11DeviceContext** ppDeferredContext)
{
    HOOK_DEBUG("TrampolineDevice::CreateDeferredContext()\n");
    return orig_vtable.CreateDeferredContext(reinterpret_cast<ID3D11Device1_trampoline*>(This)->orig_this, ContextFlags, ppDeferredContext);
}
static HRESULT STDMETHODCALLTYPE trampoline_OpenSharedResource(
    ID3D11Device1* This,
    HANDLE         hResource,
    REFIID         ReturnedInterface,
    void**         ppResource)
{
    HOOK_DEBUG("TrampolineDevice::OpenSharedResource()\n");
    return orig_vtable.OpenSharedResource(reinterpret_cast<ID3D11Device1_trampoline*>(This)->orig_this, hResource, ReturnedInterface, ppResource);
}
static HRESULT STDMETHODCALLTYPE trampoline_CheckFormatSupport(
    ID3D11Device1* This,
    DXGI_FORMAT    Format,
    UINT*          pFormatSupport)
{
    HOOK_DEBUG("TrampolineDevice::CheckFormatSupport()\n");
    return orig_vtable.CheckFormatSupport(reinterpret_cast<ID3D11Device1_trampoline*>(This)->orig_this, Format, pFormatSupport);
}
static HRESULT STDMETHODCALLTYPE trampoline_CheckMultisampleQualityLevels(
    ID3D11Device1* This,
    DXGI_FORMAT    Format,
    UINT           SampleCount,
    UINT*          pNumQualityLevels)
{
    HOOK_DEBUG("TrampolineDevice::CheckMultisampleQualityLevels()\n");
    return orig_vtable.CheckMultisampleQualityLevels(reinterpret_cast<ID3D11Device1_trampoline*>(This)->orig_this, Format, SampleCount, pNumQualityLevels);
}
static void STDMETHODCALLTYPE trampoline_CheckCounterInfo(
    ID3D11Device1*      This,
    D3D11_COUNTER_INFO* pCounterInfo)
{
    HOOK_DEBUG("TrampolineDevice::CheckCounterInfo()\n");
    return orig_vtable.CheckCounterInfo(reinterpret_cast<ID3D11Device1_trampoline*>(This)->orig_this, pCounterInfo);
}
static HRESULT STDMETHODCALLTYPE trampoline_CheckCounter(
    ID3D11Device1*            This,
    const D3D11_COUNTER_DESC* pDesc,
    D3D11_COUNTER_TYPE*       pType,
    UINT*                     pActiveCounters,
    LPSTR                     szName,
    UINT*                     pNameLength,
    LPSTR                     szUnits,
    UINT*                     pUnitsLength,
    LPSTR                     szDescription,
    UINT*                     pDescriptionLength)
{
    HOOK_DEBUG("TrampolineDevice::CheckCounter()\n");
    return orig_vtable.CheckCounter(reinterpret_cast<ID3D11Device1_trampoline*>(This)->orig_this, pDesc, pType, pActiveCounters, szName, pNameLength, szUnits, pUnitsLength, szDescription, pDescriptionLength);
}
static HRESULT STDMETHODCALLTYPE trampoline_CheckFeatureSupport(
    ID3D11Device1* This,
    D3D11_FEATURE  Feature,
    void*          pFeatureSupportData,
    UINT           FeatureSupportDataSize)
{
    HOOK_DEBUG("TrampolineDevice::CheckFeatureSupport()\n");
    return orig_vtable.CheckFeatureSupport(reinterpret_cast<ID3D11Device1_trampoline*>(This)->orig_this, Feature, pFeatureSupportData, FeatureSupportDataSize);
}
static HRESULT STDMETHODCALLTYPE trampoline_GetPrivateData(
    ID3D11Device1* This,
    REFGUID        guid,
    UINT*          pDataSize,
    void*          pData)
{
    HOOK_DEBUG("TrampolineDevice::GetPrivateData()\n");
    return orig_vtable.GetPrivateData(reinterpret_cast<ID3D11Device1_trampoline*>(This)->orig_this, guid, pDataSize, pData);
}
static HRESULT STDMETHODCALLTYPE trampoline_SetPrivateData(
    ID3D11Device1* This,
    REFGUID        guid,
    UINT           DataSize,
    const void*    pData)
{
    HOOK_DEBUG("TrampolineDevice::SetPrivateData()\n");
    return orig_vtable.SetPrivateData(reinterpret_cast<ID3D11Device1_trampoline*>(This)->orig_this, guid, DataSize, pData);
}
static HRESULT STDMETHODCALLTYPE trampoline_SetPrivateDataInterface(
    ID3D11Device1*  This,
    REFGUID         guid,
    const IUnknown* pData)
{
    HOOK_DEBUG("TrampolineDevice::SetPrivateDataInterface()\n");
    return orig_vtable.SetPrivateDataInterface(reinterpret_cast<ID3D11Device1_trampoline*>(This)->orig_this, guid, pData);
}
static D3D_FEATURE_LEVEL STDMETHODCALLTYPE trampoline_GetFeatureLevel(
    ID3D11Device1* This)
{
    HOOK_DEBUG("TrampolineDevice::GetFeatureLevel()\n");
    return orig_vtable.GetFeatureLevel(reinterpret_cast<ID3D11Device1_trampoline*>(This)->orig_this);
}
static UINT STDMETHODCALLTYPE trampoline_GetCreationFlags(
    ID3D11Device1* This)
{
    HOOK_DEBUG("TrampolineDevice::GetCreationFlags()\n");
    return orig_vtable.GetCreationFlags(reinterpret_cast<ID3D11Device1_trampoline*>(This)->orig_this);
}
static HRESULT STDMETHODCALLTYPE trampoline_GetDeviceRemovedReason(
    ID3D11Device1* This)
{
    HOOK_DEBUG("TrampolineDevice::GetDeviceRemovedReason()\n");
    return orig_vtable.GetDeviceRemovedReason(reinterpret_cast<ID3D11Device1_trampoline*>(This)->orig_this);
}
static void STDMETHODCALLTYPE trampoline_GetImmediateContext(
    ID3D11Device1*        This,
    ID3D11DeviceContext** ppImmediateContext)
{
    HOOK_DEBUG("TrampolineDevice::GetImmediateContext()\n");
    return orig_vtable.GetImmediateContext(reinterpret_cast<ID3D11Device1_trampoline*>(This)->orig_this, ppImmediateContext);
}
static HRESULT STDMETHODCALLTYPE trampoline_SetExceptionMode(
    ID3D11Device1* This,
    UINT           RaiseFlags)
{
    HOOK_DEBUG("TrampolineDevice::SetExceptionMode()\n");
    return orig_vtable.SetExceptionMode(reinterpret_cast<ID3D11Device1_trampoline*>(This)->orig_this, RaiseFlags);
}
static UINT STDMETHODCALLTYPE trampoline_GetExceptionMode(
    ID3D11Device1* This)
{
    HOOK_DEBUG("TrampolineDevice::GetExceptionMode()\n");
    return orig_vtable.GetExceptionMode(reinterpret_cast<ID3D11Device1_trampoline*>(This)->orig_this);
}

static CONST_VTBL struct ID3D11Device1Vtbl trampoline_vtable = {
    trampoline_QueryInterface,
    trampoline_AddRef,
    trampoline_Release,
    trampoline_CreateBuffer,
    trampoline_CreateTexture1D,
    trampoline_CreateTexture2D,
    trampoline_CreateTexture3D,
    trampoline_CreateShaderResourceView,
    trampoline_CreateUnorderedAccessView,
    trampoline_CreateRenderTargetView,
    trampoline_CreateDepthStencilView,
    trampoline_CreateInputLayout,
    trampoline_CreateVertexShader,
    trampoline_CreateGeometryShader,
    trampoline_CreateGeometryShaderWithStreamOutput,
    trampoline_CreatePixelShader,
    trampoline_CreateHullShader,
    trampoline_CreateDomainShader,
    trampoline_CreateComputeShader,
    trampoline_CreateClassLinkage,
    trampoline_CreateBlendState,
    trampoline_CreateDepthStencilState,
    trampoline_CreateRasterizerState,
    trampoline_CreateSamplerState,
    trampoline_CreateQuery,
    trampoline_CreatePredicate,
    trampoline_CreateCounter,
    trampoline_CreateDeferredContext,
    trampoline_OpenSharedResource,
    trampoline_CheckFormatSupport,
    trampoline_CheckMultisampleQualityLevels,
    trampoline_CheckCounterInfo,
    trampoline_CheckCounter,
    trampoline_CheckFeatureSupport,
    trampoline_GetPrivateData,
    trampoline_SetPrivateData,
    trampoline_SetPrivateDataInterface,
    trampoline_GetFeatureLevel,
    trampoline_GetCreationFlags,
    trampoline_GetDeviceRemovedReason,
    trampoline_GetImmediateContext,
    trampoline_SetExceptionMode,
    trampoline_GetExceptionMode,
};

// -----------------------------------------------------------------------------------------------

ID3D11Device1* lookup_hooked_device(
    ID3D11Device1* orig_device)
{
    DeviceMap::iterator i;

    if (!hooks_installed)
        return nullptr;

    ENTER_CRITICAL_SECTION(&device_map_lock);
    {
        i = device_map.find(orig_device);
        if (i == device_map.end())
        {
            LEAVE_CRITICAL_SECTION(&device_map_lock);
            return nullptr;
        }
    }
    LEAVE_CRITICAL_SECTION(&device_map_lock);

    return i->second;
}

ID3D11Device1* hook_device(
    ID3D11Device1* orig_device,
    ID3D11Device1* hacker_device)
{
    ID3D11Device1_trampoline* trampoline_device = new ID3D11Device1_trampoline();
    trampoline_device->lpVtbl                   = &trampoline_vtable;
    trampoline_device->orig_this                = orig_device;

    install_hooks(orig_device);
    ENTER_CRITICAL_SECTION(&device_map_lock);
    {
        device_map[orig_device] = hacker_device;
    }
    LEAVE_CRITICAL_SECTION(&device_map_lock);

    return reinterpret_cast<ID3D11Device1*>(trampoline_device);
}
