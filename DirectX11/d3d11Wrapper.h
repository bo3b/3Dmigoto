#pragma once

// Forward declarations
class ID3D11DeviceContext;

class ID3D11Device : public IDirect3DUnknown
{
public:
    static ThreadSafePointerSet	 m_List;
	D3D11Base::StereoHandle mStereoHandle;
	D3D11Base::nv::stereo::ParamTextureManagerD3D11 mParamTextureManager;
	D3D11Base::ID3D11Texture2D *mStereoTexture;
	D3D11Base::ID3D11ShaderResourceView *mStereoResourceView;
	D3D11Base::ID3D11ShaderResourceView *mZBufferResourceView;
	D3D11Base::ID3D11Texture1D *mIniTexture;
	D3D11Base::ID3D11ShaderResourceView *mIniResourceView;

    ID3D11Device(D3D11Base::ID3D11Device *pDevice);
    static ID3D11Device* GetDirect3DDevice(D3D11Base::ID3D11Device *pDevice);
	__forceinline D3D11Base::ID3D11Device *GetD3D11Device() { return (D3D11Base::ID3D11Device*) m_pUnk; }

    /*** IDirect3DUnknown methods ***/
	STDMETHOD_(ULONG,AddRef)(THIS);
    STDMETHOD_(ULONG,Release)(THIS);

	/*** ID3D11Device methods ***/
	STDMETHOD(CreateBuffer)(THIS_
            /* [annotation] */ 
            __in  const D3D11Base::D3D11_BUFFER_DESC *pDesc,
            /* [annotation] */ 
            __in_opt  const D3D11Base::D3D11_SUBRESOURCE_DATA *pInitialData,
            /* [annotation] */ 
            __out_opt  D3D11Base::ID3D11Buffer **ppBuffer);
        
	STDMETHOD(CreateTexture1D)(THIS_
            /* [annotation] */ 
            __in  const D3D11Base::D3D11_TEXTURE1D_DESC *pDesc,
            /* [annotation] */ 
            __in_xcount_opt(pDesc->MipLevels * pDesc->ArraySize)  const D3D11Base::D3D11_SUBRESOURCE_DATA *pInitialData,
            /* [annotation] */ 
            __out_opt  D3D11Base::ID3D11Texture1D **ppTexture1D);
        
	STDMETHOD(CreateTexture2D)(THIS_
            /* [annotation] */ 
            __in  const D3D11Base::D3D11_TEXTURE2D_DESC *pDesc,
            /* [annotation] */ 
            __in_xcount_opt(pDesc->MipLevels * pDesc->ArraySize)  const D3D11Base::D3D11_SUBRESOURCE_DATA *pInitialData,
            /* [annotation] */ 
            __out_opt  D3D11Base::ID3D11Texture2D **ppTexture2D);
        
	STDMETHOD(CreateTexture3D)(THIS_
            /* [annotation] */ 
            __in  const D3D11Base::D3D11_TEXTURE3D_DESC *pDesc,
            /* [annotation] */ 
            __in_xcount_opt(pDesc->MipLevels)  const D3D11Base::D3D11_SUBRESOURCE_DATA *pInitialData,
            /* [annotation] */ 
            __out_opt  D3D11Base::ID3D11Texture3D **ppTexture3D);
        
	STDMETHOD(CreateShaderResourceView)(THIS_
            /* [annotation] */ 
            __in  D3D11Base::ID3D11Resource *pResource,
            /* [annotation] */ 
            __in_opt  const D3D11Base::D3D11_SHADER_RESOURCE_VIEW_DESC *pDesc,
            /* [annotation] */ 
            __out_opt  D3D11Base::ID3D11ShaderResourceView **ppSRView);
        
	STDMETHOD(CreateUnorderedAccessView)(THIS_
            /* [annotation] */ 
            __in  D3D11Base::ID3D11Resource *pResource,
            /* [annotation] */ 
            __in_opt  const D3D11Base::D3D11_UNORDERED_ACCESS_VIEW_DESC *pDesc,
            /* [annotation] */ 
            __out_opt  D3D11Base::ID3D11UnorderedAccessView **ppUAView);
        
	STDMETHOD(CreateRenderTargetView)(THIS_
            /* [annotation] */ 
            __in  D3D11Base::ID3D11Resource *pResource,
            /* [annotation] */ 
            __in_opt  const D3D11Base::D3D11_RENDER_TARGET_VIEW_DESC *pDesc,
            /* [annotation] */ 
            __out_opt  D3D11Base::ID3D11RenderTargetView **ppRTView);
        
	STDMETHOD(CreateDepthStencilView)(THIS_
            /* [annotation] */ 
            __in  D3D11Base::ID3D11Resource *pResource,
            /* [annotation] */ 
            __in_opt  const D3D11Base::D3D11_DEPTH_STENCIL_VIEW_DESC *pDesc,
            /* [annotation] */ 
            __out_opt  D3D11Base::ID3D11DepthStencilView **ppDepthStencilView);
        
	STDMETHOD(CreateInputLayout)(THIS_
            /* [annotation] */ 
            __in_ecount(NumElements)  const D3D11Base::D3D11_INPUT_ELEMENT_DESC *pInputElementDescs,
            /* [annotation] */ 
            __in_range( 0, D3D11_IA_VERTEX_INPUT_STRUCTURE_ELEMENT_COUNT )  UINT NumElements,
            /* [annotation] */ 
            __in  const void *pShaderBytecodeWithInputSignature,
            /* [annotation] */ 
            __in  SIZE_T BytecodeLength,
            /* [annotation] */ 
            __out_opt  D3D11Base::ID3D11InputLayout **ppInputLayout);
        
	STDMETHOD(CreateVertexShader)(THIS_
            /* [annotation] */ 
            __in  const void *pShaderBytecode,
            /* [annotation] */ 
            __in  SIZE_T BytecodeLength,
            /* [annotation] */ 
            __in_opt  D3D11Base::ID3D11ClassLinkage *pClassLinkage,
            /* [annotation] */ 
            __out_opt  D3D11Base::ID3D11VertexShader **ppVertexShader);
        
	STDMETHOD(CreateGeometryShader)(THIS_
            /* [annotation] */ 
            __in  const void *pShaderBytecode,
            /* [annotation] */ 
            __in  SIZE_T BytecodeLength,
            /* [annotation] */ 
            __in_opt  D3D11Base::ID3D11ClassLinkage *pClassLinkage,
            /* [annotation] */ 
            __out_opt  D3D11Base::ID3D11GeometryShader **ppGeometryShader);
        
	STDMETHOD(CreateGeometryShaderWithStreamOutput)(THIS_
            /* [annotation] */ 
            __in  const void *pShaderBytecode,
            /* [annotation] */ 
            __in  SIZE_T BytecodeLength,
            /* [annotation] */ 
            __in_ecount_opt(NumEntries)  const D3D11Base::D3D11_SO_DECLARATION_ENTRY *pSODeclaration,
            /* [annotation] */ 
            __in_range( 0, D3D11_SO_STREAM_COUNT * D3D11_SO_OUTPUT_COMPONENT_COUNT )  UINT NumEntries,
            /* [annotation] */ 
            __in_ecount_opt(NumStrides)  const UINT *pBufferStrides,
            /* [annotation] */ 
            __in_range( 0, D3D11_SO_BUFFER_SLOT_COUNT )  UINT NumStrides,
            /* [annotation] */ 
            __in  UINT RasterizedStream,
            /* [annotation] */ 
            __in_opt  D3D11Base::ID3D11ClassLinkage *pClassLinkage,
            /* [annotation] */ 
            __out_opt  D3D11Base::ID3D11GeometryShader **ppGeometryShader);
        
	STDMETHOD(CreatePixelShader)(THIS_
            /* [annotation] */ 
            __in  const void *pShaderBytecode,
            /* [annotation] */ 
            __in  SIZE_T BytecodeLength,
            /* [annotation] */ 
            __in_opt  D3D11Base::ID3D11ClassLinkage *pClassLinkage,
            /* [annotation] */ 
            __out_opt  D3D11Base::ID3D11PixelShader **ppPixelShader);
        
	STDMETHOD(CreateHullShader)(THIS_
            /* [annotation] */ 
            __in  const void *pShaderBytecode,
            /* [annotation] */ 
            __in  SIZE_T BytecodeLength,
            /* [annotation] */ 
            __in_opt  D3D11Base::ID3D11ClassLinkage *pClassLinkage,
            /* [annotation] */ 
            __out_opt  D3D11Base::ID3D11HullShader **ppHullShader);
        
	STDMETHOD(CreateDomainShader)(THIS_
            /* [annotation] */ 
            __in  const void *pShaderBytecode,
            /* [annotation] */ 
            __in  SIZE_T BytecodeLength,
            /* [annotation] */ 
            __in_opt  D3D11Base::ID3D11ClassLinkage *pClassLinkage,
            /* [annotation] */ 
            __out_opt  D3D11Base::ID3D11DomainShader **ppDomainShader);
        
	STDMETHOD(CreateComputeShader)(THIS_
            /* [annotation] */ 
            __in  const void *pShaderBytecode,
            /* [annotation] */ 
            __in  SIZE_T BytecodeLength,
            /* [annotation] */ 
            __in_opt  D3D11Base::ID3D11ClassLinkage *pClassLinkage,
            /* [annotation] */ 
            __out_opt  D3D11Base::ID3D11ComputeShader **ppComputeShader);
        
	STDMETHOD(CreateClassLinkage)(THIS_
            /* [annotation] */ 
            __out  D3D11Base::ID3D11ClassLinkage **ppLinkage);
        
	STDMETHOD(CreateBlendState)(THIS_
            /* [annotation] */ 
            __in  const D3D11Base::D3D11_BLEND_DESC *pBlendStateDesc,
            /* [annotation] */ 
            __out_opt  D3D11Base::ID3D11BlendState **ppBlendState);
        
	STDMETHOD(CreateDepthStencilState)(THIS_
            /* [annotation] */ 
            __in  const D3D11Base::D3D11_DEPTH_STENCIL_DESC *pDepthStencilDesc,
            /* [annotation] */ 
            __out_opt  D3D11Base::ID3D11DepthStencilState **ppDepthStencilState);
        
	STDMETHOD(CreateRasterizerState)(THIS_
            /* [annotation] */ 
            __in  D3D11Base::D3D11_RASTERIZER_DESC *pRasterizerDesc,
            /* [annotation] */ 
            __out_opt  D3D11Base::ID3D11RasterizerState **ppRasterizerState);
        
	STDMETHOD(CreateSamplerState)(THIS_
            /* [annotation] */ 
            __in  const D3D11Base::D3D11_SAMPLER_DESC *pSamplerDesc,
            /* [annotation] */ 
            __out_opt  D3D11Base::ID3D11SamplerState **ppSamplerState);
        
	STDMETHOD(CreateQuery)(THIS_
            /* [annotation] */ 
            __in  const D3D11Base::D3D11_QUERY_DESC *pQueryDesc,
            /* [annotation] */ 
            __out_opt  D3D11Base::ID3D11Query **ppQuery);
        
	STDMETHOD(CreatePredicate)(THIS_
            /* [annotation] */ 
            __in  const D3D11Base::D3D11_QUERY_DESC *pPredicateDesc,
            /* [annotation] */ 
            __out_opt  D3D11Base::ID3D11Predicate **ppPredicate);
        
	STDMETHOD(CreateCounter)(THIS_
            /* [annotation] */ 
            __in  const D3D11Base::D3D11_COUNTER_DESC *pCounterDesc,
            /* [annotation] */ 
            __out_opt  D3D11Base::ID3D11Counter **ppCounter);
        
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
            __in  D3D11Base::DXGI_FORMAT Format,
            /* [annotation] */ 
            __out  UINT *pFormatSupport);
        
	STDMETHOD(CheckMultisampleQualityLevels)(THIS_
            /* [annotation] */ 
            __in  D3D11Base::DXGI_FORMAT Format,
            /* [annotation] */ 
            __in  UINT SampleCount,
            /* [annotation] */ 
            __out  UINT *pNumQualityLevels);
        
	STDMETHOD_(void, CheckCounterInfo)(THIS_
            /* [annotation] */ 
            __out  D3D11Base::D3D11_COUNTER_INFO *pCounterInfo);
        
    STDMETHOD(CheckCounter)(THIS_
            /* [annotation] */ 
            __in  const D3D11Base::D3D11_COUNTER_DESC *pDesc,
            /* [annotation] */ 
            __out  D3D11Base::D3D11_COUNTER_TYPE *pType,
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
            D3D11Base::D3D11_FEATURE Feature,
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
        
	STDMETHOD_(D3D11Base::D3D_FEATURE_LEVEL, GetFeatureLevel)(THIS);
        
    STDMETHOD_(UINT, GetCreationFlags)(THIS);
        
    STDMETHOD(GetDeviceRemovedReason)(THIS);
        
    STDMETHOD_(void, GetImmediateContext)(THIS_ 
            /* [annotation] */ 
            __out ID3D11DeviceContext **ppImmediateContext);
        
    STDMETHOD(SetExceptionMode)(THIS_ 
            UINT RaiseFlags);
        
    STDMETHOD_(UINT, GetExceptionMode)(THIS);

};

// devicechild: MIDL_INTERFACE("1841e5c8-16b0-489b-bcc8-44cfb0d5deae")
// MIDL_INTERFACE("c0bfa96c-e089-44fb-8eaf-26f8796190da")
class ID3D11DeviceContext : public IDirect3DUnknown
{
public:
    static ThreadSafePointerSet	m_List;

    ID3D11DeviceContext(D3D11Base::ID3D11DeviceContext *pContext);
    static ID3D11DeviceContext* GetDirect3DDeviceContext(D3D11Base::ID3D11DeviceContext *pContext);
	__forceinline D3D11Base::ID3D11DeviceContext *GetD3D11DeviceContext() { return (D3D11Base::ID3D11DeviceContext*) m_pUnk; }

    /*** IDirect3DUnknown methods ***/
	STDMETHOD_(ULONG,AddRef)(THIS);
    STDMETHOD_(ULONG,Release)(THIS);

	// ******************* ID3D11DeviceChild interface

	STDMETHOD_(void, GetDevice)(THIS_
        /* [annotation] */ 
        __out  ID3D11Device **ppDevice);
        
    STDMETHOD(GetPrivateData)(THIS_
        /* [annotation] */ 
        __in  REFGUID guid,
        /* [annotation] */ 
        __inout  UINT *pDataSize,
        /* [annotation] */ 
        __out_bcount_opt( *pDataSize )  void *pData);
        
    STDMETHOD(SetPrivateData)(THIS_
        /* [annotation] */ 
        __in  REFGUID guid,
        /* [annotation] */ 
        __in  UINT DataSize,
        /* [annotation] */ 
        __in_bcount_opt( DataSize )  const void *pData);
        
    STDMETHOD(SetPrivateDataInterface)(THIS_
        /* [annotation] */ 
        __in  REFGUID guid,
        /* [annotation] */ 
        __in_opt  const IUnknown *pData);

	// ******************* ID3D11DeviceContext interface

    STDMETHOD_(void, VSSetConstantBuffers)(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
        /* [annotation] */ 
        __in_ecount(NumBuffers) D3D11Base::ID3D11Buffer *const *ppConstantBuffers);
        
    STDMETHOD_(void, PSSetShaderResources)(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
        /* [annotation] */ 
        __in_ecount(NumViews) D3D11Base::ID3D11ShaderResourceView *const *ppShaderResourceViews);
        
    STDMETHOD_(void, PSSetShader)(THIS_
        /* [annotation] */ 
        __in_opt D3D11Base::ID3D11PixelShader *pPixelShader,
        /* [annotation] */ 
        __in_ecount_opt(NumClassInstances) D3D11Base::ID3D11ClassInstance *const *ppClassInstances,
        UINT NumClassInstances);
        
    STDMETHOD_(void, PSSetSamplers)(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
        /* [annotation] */ 
        __in_ecount(NumSamplers) D3D11Base::ID3D11SamplerState *const *ppSamplers);
        
    STDMETHOD_(void, VSSetShader)(THIS_
        /* [annotation] */ 
        __in_opt D3D11Base::ID3D11VertexShader *pVertexShader,
        /* [annotation] */ 
        __in_ecount_opt(NumClassInstances) D3D11Base::ID3D11ClassInstance *const *ppClassInstances,
        UINT NumClassInstances);
        
    STDMETHOD_(void, DrawIndexed)(THIS_
        /* [annotation] */ 
        __in  UINT IndexCount,
        /* [annotation] */ 
        __in  UINT StartIndexLocation,
        /* [annotation] */ 
        __in  INT BaseVertexLocation);
        
    STDMETHOD_(void, Draw)(THIS_
        /* [annotation] */ 
        __in  UINT VertexCount,
        /* [annotation] */ 
        __in  UINT StartVertexLocation);
        
    STDMETHOD(Map)(THIS_
        /* [annotation] */ 
        __in  D3D11Base::ID3D11Resource *pResource,
        /* [annotation] */ 
        __in  UINT Subresource,
        /* [annotation] */ 
        __in  D3D11Base::D3D11_MAP MapType,
        /* [annotation] */ 
        __in  UINT MapFlags,
        /* [annotation] */ 
        __out D3D11Base::D3D11_MAPPED_SUBRESOURCE *pMappedResource);
        
    STDMETHOD_(void, Unmap)(THIS_
        /* [annotation] */ 
        __in D3D11Base::ID3D11Resource *pResource,
        /* [annotation] */ 
        __in  UINT Subresource);
        
    STDMETHOD_(void, PSSetConstantBuffers)(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
        /* [annotation] */ 
        __in_ecount(NumBuffers) D3D11Base::ID3D11Buffer *const *ppConstantBuffers);
        
    STDMETHOD_(void, IASetInputLayout)(THIS_
        /* [annotation] */ 
        __in_opt D3D11Base::ID3D11InputLayout *pInputLayout);
        
    STDMETHOD_(void, IASetVertexBuffers)(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        __in_range( 0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumBuffers,
        /* [annotation] */ 
        __in_ecount(NumBuffers)  D3D11Base::ID3D11Buffer *const *ppVertexBuffers,
        /* [annotation] */ 
        __in_ecount(NumBuffers)  const UINT *pStrides,
        /* [annotation] */ 
        __in_ecount(NumBuffers)  const UINT *pOffsets);
        
    STDMETHOD_(void, IASetIndexBuffer)(THIS_
        /* [annotation] */ 
        __in_opt D3D11Base::ID3D11Buffer *pIndexBuffer,
        /* [annotation] */ 
        __in D3D11Base::DXGI_FORMAT Format,
        /* [annotation] */ 
        __in  UINT Offset);
        
    STDMETHOD_(void, DrawIndexedInstanced)(THIS_
        /* [annotation] */ 
        __in  UINT IndexCountPerInstance,
        /* [annotation] */ 
        __in  UINT InstanceCount,
        /* [annotation] */ 
        __in  UINT StartIndexLocation,
        /* [annotation] */ 
        __in  INT BaseVertexLocation,
        /* [annotation] */ 
        __in  UINT StartInstanceLocation);
        
    STDMETHOD_(void, DrawInstanced)(THIS_
        /* [annotation] */ 
        __in  UINT VertexCountPerInstance,
        /* [annotation] */ 
        __in  UINT InstanceCount,
        /* [annotation] */ 
        __in  UINT StartVertexLocation,
        /* [annotation] */ 
        __in  UINT StartInstanceLocation);
        
    STDMETHOD_(void, GSSetConstantBuffers)(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
        /* [annotation] */ 
        __in_ecount(NumBuffers) D3D11Base::ID3D11Buffer *const *ppConstantBuffers);
        
    STDMETHOD_(void, GSSetShader)(THIS_
        /* [annotation] */ 
        __in_opt D3D11Base::ID3D11GeometryShader *pShader,
        /* [annotation] */ 
        __in_ecount_opt(NumClassInstances) D3D11Base::ID3D11ClassInstance *const *ppClassInstances,
        UINT NumClassInstances);
        
    STDMETHOD_(void, IASetPrimitiveTopology)(THIS_
        /* [annotation] */ 
        __in D3D11Base::D3D11_PRIMITIVE_TOPOLOGY Topology);
        
    STDMETHOD_(void, VSSetShaderResources)(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
        /* [annotation] */ 
        __in_ecount(NumViews) D3D11Base::ID3D11ShaderResourceView *const *ppShaderResourceViews);
        
    STDMETHOD_(void, VSSetSamplers)(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
        /* [annotation] */ 
        __in_ecount(NumSamplers) D3D11Base::ID3D11SamplerState *const *ppSamplers);
        
    STDMETHOD_(void, Begin)(THIS_
        /* [annotation] */ 
        __in  D3D11Base::ID3D11Asynchronous *pAsync);
        
    STDMETHOD_(void, End)(THIS_
        /* [annotation] */ 
        __in  D3D11Base::ID3D11Asynchronous *pAsync);
        
    STDMETHOD(GetData)(THIS_
        /* [annotation] */ 
        __in  D3D11Base::ID3D11Asynchronous *pAsync,
        /* [annotation] */ 
        __out_bcount_opt( DataSize )  void *pData,
        /* [annotation] */ 
        __in  UINT DataSize,
        /* [annotation] */ 
        __in  UINT GetDataFlags);
        
    STDMETHOD_(void, SetPredication)(THIS_
        /* [annotation] */ 
        __in_opt D3D11Base::ID3D11Predicate *pPredicate,
        /* [annotation] */ 
        __in  BOOL PredicateValue);
        
    STDMETHOD_(void, GSSetShaderResources)(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
        /* [annotation] */ 
        __in_ecount(NumViews) D3D11Base::ID3D11ShaderResourceView *const *ppShaderResourceViews);
        
    STDMETHOD_(void, GSSetSamplers)(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
        /* [annotation] */ 
        __in_ecount(NumSamplers) D3D11Base::ID3D11SamplerState *const *ppSamplers);
        
    STDMETHOD_(void, OMSetRenderTargets)(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT )  UINT NumViews,
        /* [annotation] */ 
        __in_ecount_opt(NumViews) D3D11Base::ID3D11RenderTargetView *const *ppRenderTargetViews,
        /* [annotation] */ 
        __in_opt D3D11Base::ID3D11DepthStencilView *pDepthStencilView);
        
    STDMETHOD_(void, OMSetRenderTargetsAndUnorderedAccessViews)(THIS_
        /* [annotation] */ 
        __in  UINT NumRTVs,
        /* [annotation] */ 
        __in_ecount_opt(NumRTVs) D3D11Base::ID3D11RenderTargetView *const *ppRenderTargetViews,
        /* [annotation] */ 
        __in_opt D3D11Base::ID3D11DepthStencilView *pDepthStencilView,
        /* [annotation] */ 
        __in_range( 0, D3D11_PS_CS_UAV_REGISTER_COUNT - 1 )  UINT UAVStartSlot,
        /* [annotation] */ 
        __in  UINT NumUAVs,
        /* [annotation] */ 
        __in_ecount_opt(NumUAVs) D3D11Base::ID3D11UnorderedAccessView *const *ppUnorderedAccessViews,
        /* [annotation] */ 
        __in_ecount_opt(NumUAVs)  const UINT *pUAVInitialCounts);
        
    STDMETHOD_(void, OMSetBlendState)(THIS_
        /* [annotation] */ 
        __in_opt  D3D11Base::ID3D11BlendState *pBlendState,
        /* [annotation] */ 
        __in_opt  const FLOAT BlendFactor[ 4 ],
        /* [annotation] */ 
        __in  UINT SampleMask);
        
    STDMETHOD_(void, OMSetDepthStencilState)(THIS_
        /* [annotation] */ 
        __in_opt  D3D11Base::ID3D11DepthStencilState *pDepthStencilState,
        /* [annotation] */ 
        __in  UINT StencilRef);
        
    STDMETHOD_(void, SOSetTargets)(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_SO_BUFFER_SLOT_COUNT)  UINT NumBuffers,
        /* [annotation] */ 
        __in_ecount_opt(NumBuffers)  D3D11Base::ID3D11Buffer *const *ppSOTargets,
        /* [annotation] */ 
        __in_ecount_opt(NumBuffers)  const UINT *pOffsets);
        
    STDMETHOD_(void, DrawAuto)(THIS);
        
    STDMETHOD_(void, DrawIndexedInstancedIndirect)(THIS_
        /* [annotation] */ 
        __in  D3D11Base::ID3D11Buffer *pBufferForArgs,
        /* [annotation] */ 
        __in  UINT AlignedByteOffsetForArgs);
        
    STDMETHOD_(void, DrawInstancedIndirect)(THIS_
        /* [annotation] */ 
        __in  D3D11Base::ID3D11Buffer *pBufferForArgs,
        /* [annotation] */ 
        __in  UINT AlignedByteOffsetForArgs);
        
    STDMETHOD_(void, Dispatch)(THIS_
        /* [annotation] */ 
        __in  UINT ThreadGroupCountX,
        /* [annotation] */ 
        __in  UINT ThreadGroupCountY,
        /* [annotation] */ 
        __in  UINT ThreadGroupCountZ);
        
    STDMETHOD_(void, DispatchIndirect)(THIS_
        /* [annotation] */ 
        __in  D3D11Base::ID3D11Buffer *pBufferForArgs,
        /* [annotation] */ 
        __in  UINT AlignedByteOffsetForArgs);
        
    STDMETHOD_(void, RSSetState)(THIS_
        /* [annotation] */ 
        __in_opt  D3D11Base::ID3D11RasterizerState *pRasterizerState);
        
    STDMETHOD_(void, RSSetViewports)(THIS_
        /* [annotation] */ 
        __in_range(0, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE)  UINT NumViewports,
        /* [annotation] */ 
        __in_ecount_opt(NumViewports)  const D3D11Base::D3D11_VIEWPORT *pViewports);
        
    STDMETHOD_(void, RSSetScissorRects)(THIS_
        /* [annotation] */ 
        __in_range(0, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE)  UINT NumRects,
        /* [annotation] */ 
        __in_ecount_opt(NumRects)  const D3D11Base::D3D11_RECT *pRects);
        
    STDMETHOD_(void, CopySubresourceRegion)(THIS_
        /* [annotation] */ 
        __in  D3D11Base::ID3D11Resource *pDstResource,
        /* [annotation] */ 
        __in  UINT DstSubresource,
        /* [annotation] */ 
        __in  UINT DstX,
        /* [annotation] */ 
        __in  UINT DstY,
        /* [annotation] */ 
        __in  UINT DstZ,
        /* [annotation] */ 
        __in  D3D11Base::ID3D11Resource *pSrcResource,
        /* [annotation] */ 
        __in  UINT SrcSubresource,
        /* [annotation] */ 
        __in_opt  const D3D11Base::D3D11_BOX *pSrcBox);
        
    STDMETHOD_(void, CopyResource)(THIS_
        /* [annotation] */ 
        __in  D3D11Base::ID3D11Resource *pDstResource,
        /* [annotation] */ 
        __in  D3D11Base::ID3D11Resource *pSrcResource);
        
    STDMETHOD_(void, UpdateSubresource)(THIS_
        /* [annotation] */ 
        __in  D3D11Base::ID3D11Resource *pDstResource,
        /* [annotation] */ 
        __in  UINT DstSubresource,
        /* [annotation] */ 
        __in_opt  const D3D11Base::D3D11_BOX *pDstBox,
        /* [annotation] */ 
        __in  const void *pSrcData,
        /* [annotation] */ 
        __in  UINT SrcRowPitch,
        /* [annotation] */ 
        __in  UINT SrcDepthPitch);
        
    STDMETHOD_(void, CopyStructureCount)(THIS_
        /* [annotation] */ 
        __in  D3D11Base::ID3D11Buffer *pDstBuffer,
        /* [annotation] */ 
        __in  UINT DstAlignedByteOffset,
        /* [annotation] */ 
        __in  D3D11Base::ID3D11UnorderedAccessView *pSrcView);
        
    STDMETHOD_(void, ClearRenderTargetView)(THIS_
        /* [annotation] */ 
        __in  D3D11Base::ID3D11RenderTargetView *pRenderTargetView,
        /* [annotation] */ 
        __in  const FLOAT ColorRGBA[ 4 ]);
        
    STDMETHOD_(void, ClearUnorderedAccessViewUint)(THIS_
        /* [annotation] */ 
        __in  D3D11Base::ID3D11UnorderedAccessView *pUnorderedAccessView,
        /* [annotation] */ 
        __in  const UINT Values[ 4 ]);
        
    STDMETHOD_(void, ClearUnorderedAccessViewFloat)(THIS_
        /* [annotation] */ 
        __in  D3D11Base::ID3D11UnorderedAccessView *pUnorderedAccessView,
        /* [annotation] */ 
        __in  const FLOAT Values[ 4 ]) ;
        
    STDMETHOD_(void, ClearDepthStencilView)(THIS_
        /* [annotation] */ 
        __in  D3D11Base::ID3D11DepthStencilView *pDepthStencilView,
        /* [annotation] */ 
        __in  UINT ClearFlags,
        /* [annotation] */ 
        __in  FLOAT Depth,
        /* [annotation] */ 
        __in  UINT8 Stencil) ;
        
    STDMETHOD_(void, GenerateMips)(THIS_
        /* [annotation] */ 
        __in  D3D11Base::ID3D11ShaderResourceView *pShaderResourceView) ;
        
    STDMETHOD_(void, SetResourceMinLOD)(THIS_
        /* [annotation] */ 
        __in  D3D11Base::ID3D11Resource *pResource,
        FLOAT MinLOD) ;
        
    STDMETHOD_(FLOAT, GetResourceMinLOD)(THIS_
        /* [annotation] */ 
        __in  D3D11Base::ID3D11Resource *pResource) ;
        
    STDMETHOD_(void, ResolveSubresource)(THIS_
        /* [annotation] */ 
        __in  D3D11Base::ID3D11Resource *pDstResource,
        /* [annotation] */ 
        __in  UINT DstSubresource,
        /* [annotation] */ 
        __in  D3D11Base::ID3D11Resource *pSrcResource,
        /* [annotation] */ 
        __in  UINT SrcSubresource,
        /* [annotation] */ 
        __in  D3D11Base::DXGI_FORMAT Format) ;
        
    STDMETHOD_(void, ExecuteCommandList)(THIS_
        /* [annotation] */ 
        __in  D3D11Base::ID3D11CommandList *pCommandList,
        BOOL RestoreContextState) ;
        
    STDMETHOD_(void, HSSetShaderResources)(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
        /* [annotation] */ 
        __in_ecount(NumViews)  D3D11Base::ID3D11ShaderResourceView *const *ppShaderResourceViews) ;
        
    STDMETHOD_(void, HSSetShader)(THIS_
        /* [annotation] */ 
        __in_opt  D3D11Base::ID3D11HullShader *pHullShader,
        /* [annotation] */ 
        __in_ecount_opt(NumClassInstances)  D3D11Base::ID3D11ClassInstance *const *ppClassInstances,
        UINT NumClassInstances) ;
        
    STDMETHOD_(void, HSSetSamplers)(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
        /* [annotation] */ 
        __in_ecount(NumSamplers)  D3D11Base::ID3D11SamplerState *const *ppSamplers) ;
        
    STDMETHOD_(void, HSSetConstantBuffers)(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
        /* [annotation] */ 
        __in_ecount(NumBuffers)  D3D11Base::ID3D11Buffer *const *ppConstantBuffers) ;
        
    STDMETHOD_(void, DSSetShaderResources)(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
        /* [annotation] */ 
        __in_ecount(NumViews)  D3D11Base::ID3D11ShaderResourceView *const *ppShaderResourceViews) ;
        
    STDMETHOD_(void, DSSetShader)(THIS_
        /* [annotation] */ 
        __in_opt  D3D11Base::ID3D11DomainShader *pDomainShader,
        /* [annotation] */ 
        __in_ecount_opt(NumClassInstances)  D3D11Base::ID3D11ClassInstance *const *ppClassInstances,
        UINT NumClassInstances) ;
        
    STDMETHOD_(void, DSSetSamplers)(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
        /* [annotation] */ 
        __in_ecount(NumSamplers)  D3D11Base::ID3D11SamplerState *const *ppSamplers) ;
        
    STDMETHOD_(void, DSSetConstantBuffers)(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
        /* [annotation] */ 
        __in_ecount(NumBuffers)  D3D11Base::ID3D11Buffer *const *ppConstantBuffers) ;
        
    STDMETHOD_(void, CSSetShaderResources)(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
        /* [annotation] */ 
        __in_ecount(NumViews)  D3D11Base::ID3D11ShaderResourceView *const *ppShaderResourceViews) ;
        
    STDMETHOD_(void, CSSetUnorderedAccessViews)(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_PS_CS_UAV_REGISTER_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        __in_range( 0, D3D11_PS_CS_UAV_REGISTER_COUNT - StartSlot )  UINT NumUAVs,
        /* [annotation] */ 
        __in_ecount(NumUAVs)  D3D11Base::ID3D11UnorderedAccessView *const *ppUnorderedAccessViews,
        /* [annotation] */ 
        __in_ecount(NumUAVs)  const UINT *pUAVInitialCounts) ;
        
    STDMETHOD_(void, CSSetShader)(THIS_
        /* [annotation] */ 
        __in_opt  D3D11Base::ID3D11ComputeShader *pComputeShader,
        /* [annotation] */ 
        __in_ecount_opt(NumClassInstances)  D3D11Base::ID3D11ClassInstance *const *ppClassInstances,
        UINT NumClassInstances) ;
        
    STDMETHOD_(void, CSSetSamplers)(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
        /* [annotation] */ 
        __in_ecount(NumSamplers)  D3D11Base::ID3D11SamplerState *const *ppSamplers) ;
        
    STDMETHOD_(void, CSSetConstantBuffers)(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
        /* [annotation] */ 
        __in_ecount(NumBuffers)  D3D11Base::ID3D11Buffer *const *ppConstantBuffers) ;
        
    STDMETHOD_(void, VSGetConstantBuffers)(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
        /* [annotation] */ 
        __out_ecount(NumBuffers)  D3D11Base::ID3D11Buffer **ppConstantBuffers) ;
        
    STDMETHOD_(void, PSGetShaderResources)(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
        /* [annotation] */ 
        __out_ecount(NumViews)  D3D11Base::ID3D11ShaderResourceView **ppShaderResourceViews) ;
        
    STDMETHOD_(void, PSGetShader)(THIS_
        /* [annotation] */ 
        __out  D3D11Base::ID3D11PixelShader **ppPixelShader,
        /* [annotation] */ 
        __out_ecount_opt(*pNumClassInstances)  D3D11Base::ID3D11ClassInstance **ppClassInstances,
        /* [annotation] */ 
        __inout_opt  UINT *pNumClassInstances) ;
        
    STDMETHOD_(void, PSGetSamplers)(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
        /* [annotation] */ 
        __out_ecount(NumSamplers)  D3D11Base::ID3D11SamplerState **ppSamplers) ;
        
    STDMETHOD_(void, VSGetShader)(THIS_
        /* [annotation] */ 
        __out  D3D11Base::ID3D11VertexShader **ppVertexShader,
        /* [annotation] */ 
        __out_ecount_opt(*pNumClassInstances)  D3D11Base::ID3D11ClassInstance **ppClassInstances,
        /* [annotation] */ 
        __inout_opt  UINT *pNumClassInstances) ;
        
    STDMETHOD_(void, PSGetConstantBuffers)(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
        /* [annotation] */ 
        __out_ecount(NumBuffers)  D3D11Base::ID3D11Buffer **ppConstantBuffers) ;
        
    STDMETHOD_(void, IAGetInputLayout)(THIS_
        /* [annotation] */ 
        __out  D3D11Base::ID3D11InputLayout **ppInputLayout) ;
        
    STDMETHOD_(void, IAGetVertexBuffers)(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        __in_range( 0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumBuffers,
        /* [annotation] */ 
        __out_ecount_opt(NumBuffers)  D3D11Base::ID3D11Buffer **ppVertexBuffers,
        /* [annotation] */ 
        __out_ecount_opt(NumBuffers)  UINT *pStrides,
        /* [annotation] */ 
        __out_ecount_opt(NumBuffers)  UINT *pOffsets) ;
        
    STDMETHOD_(void, IAGetIndexBuffer)(THIS_
        /* [annotation] */ 
        __out_opt  D3D11Base::ID3D11Buffer **pIndexBuffer,
        /* [annotation] */ 
        __out_opt  D3D11Base::DXGI_FORMAT *Format,
        /* [annotation] */ 
        __out_opt  UINT *Offset) ;
        
    STDMETHOD_(void, GSGetConstantBuffers)(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
        /* [annotation] */ 
        __out_ecount(NumBuffers)  D3D11Base::ID3D11Buffer **ppConstantBuffers) ;
        
    STDMETHOD_(void, GSGetShader)(THIS_
        /* [annotation] */ 
        __out  D3D11Base::ID3D11GeometryShader **ppGeometryShader,
        /* [annotation] */ 
        __out_ecount_opt(*pNumClassInstances)  D3D11Base::ID3D11ClassInstance **ppClassInstances,
        /* [annotation] */ 
        __inout_opt  UINT *pNumClassInstances) ;
        
    STDMETHOD_(void, IAGetPrimitiveTopology)(THIS_
        /* [annotation] */ 
        __out  D3D11Base::D3D11_PRIMITIVE_TOPOLOGY *pTopology) ;
        
    STDMETHOD_(void, VSGetShaderResources)(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
        /* [annotation] */ 
        __out_ecount(NumViews)  D3D11Base::ID3D11ShaderResourceView **ppShaderResourceViews) ;
        
    STDMETHOD_(void, VSGetSamplers)(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
        /* [annotation] */ 
        __out_ecount(NumSamplers)  D3D11Base::ID3D11SamplerState **ppSamplers) ;
        
    STDMETHOD_(void, GetPredication)(THIS_
        /* [annotation] */ 
        __out_opt  D3D11Base::ID3D11Predicate **ppPredicate,
        /* [annotation] */ 
        __out_opt  BOOL *pPredicateValue) ;
        
    STDMETHOD_(void, GSGetShaderResources)(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
        /* [annotation] */ 
        __out_ecount(NumViews)  D3D11Base::ID3D11ShaderResourceView **ppShaderResourceViews) ;
        
    STDMETHOD_(void, GSGetSamplers)(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
        /* [annotation] */ 
        __out_ecount(NumSamplers)  D3D11Base::ID3D11SamplerState **ppSamplers) ;
        
    STDMETHOD_(void, OMGetRenderTargets)(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT )  UINT NumViews,
        /* [annotation] */ 
        __out_ecount_opt(NumViews)  D3D11Base::ID3D11RenderTargetView **ppRenderTargetViews,
        /* [annotation] */ 
        __out_opt  D3D11Base::ID3D11DepthStencilView **ppDepthStencilView) ;
        
    STDMETHOD_(void, OMGetRenderTargetsAndUnorderedAccessViews)(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT )  UINT NumRTVs,
        /* [annotation] */ 
        __out_ecount_opt(NumRTVs)  D3D11Base::ID3D11RenderTargetView **ppRenderTargetViews,
        /* [annotation] */ 
        __out_opt  D3D11Base::ID3D11DepthStencilView **ppDepthStencilView,
        /* [annotation] */ 
        __in_range( 0, D3D11_PS_CS_UAV_REGISTER_COUNT - 1 )  UINT UAVStartSlot,
        /* [annotation] */ 
        __in_range( 0, D3D11_PS_CS_UAV_REGISTER_COUNT - UAVStartSlot )  UINT NumUAVs,
        /* [annotation] */ 
        __out_ecount_opt(NumUAVs)  D3D11Base::ID3D11UnorderedAccessView **ppUnorderedAccessViews) ;
        
    STDMETHOD_(void, OMGetBlendState)(THIS_
        /* [annotation] */ 
        __out_opt  D3D11Base::ID3D11BlendState **ppBlendState,
        /* [annotation] */ 
        __out_opt  FLOAT BlendFactor[ 4 ],
        /* [annotation] */ 
        __out_opt  UINT *pSampleMask) ;
        
    STDMETHOD_(void, OMGetDepthStencilState)(THIS_
        /* [annotation] */ 
        __out_opt  D3D11Base::ID3D11DepthStencilState **ppDepthStencilState,
        /* [annotation] */ 
        __out_opt  UINT *pStencilRef) ;
        
    STDMETHOD_(void, SOGetTargets)(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_SO_BUFFER_SLOT_COUNT )  UINT NumBuffers,
        /* [annotation] */ 
        __out_ecount(NumBuffers)  D3D11Base::ID3D11Buffer **ppSOTargets) ;
        
    STDMETHOD_(void, RSGetState)(THIS_
        /* [annotation] */ 
        __out  D3D11Base::ID3D11RasterizerState **ppRasterizerState) ;
        
    STDMETHOD_(void, RSGetViewports)(THIS_
        /* [annotation] */ 
        __inout /*_range(0, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE )*/   UINT *pNumViewports,
        /* [annotation] */ 
        __out_ecount_opt(*pNumViewports)  D3D11Base::D3D11_VIEWPORT *pViewports) ;
        
    STDMETHOD_(void, RSGetScissorRects)(THIS_
        /* [annotation] */ 
        __inout /*_range(0, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE )*/   UINT *pNumRects,
        /* [annotation] */ 
        __out_ecount_opt(*pNumRects)  D3D11Base::D3D11_RECT *pRects) ;
        
    STDMETHOD_(void, HSGetShaderResources)(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
        /* [annotation] */ 
        __out_ecount(NumViews)  D3D11Base::ID3D11ShaderResourceView **ppShaderResourceViews) ;
        
    STDMETHOD_(void, HSGetShader)(THIS_
        /* [annotation] */ 
        __out  D3D11Base::ID3D11HullShader **ppHullShader,
        /* [annotation] */ 
        __out_ecount_opt(*pNumClassInstances)  D3D11Base::ID3D11ClassInstance **ppClassInstances,
        /* [annotation] */ 
        __inout_opt  UINT *pNumClassInstances) ;
        
    STDMETHOD_(void, HSGetSamplers)(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
        /* [annotation] */ 
        __out_ecount(NumSamplers)  D3D11Base::ID3D11SamplerState **ppSamplers) ;
        
    STDMETHOD_(void, HSGetConstantBuffers)(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
        /* [annotation] */ 
        __out_ecount(NumBuffers)  D3D11Base::ID3D11Buffer **ppConstantBuffers) ;
        
    STDMETHOD_(void, DSGetShaderResources)(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
        /* [annotation] */ 
        __out_ecount(NumViews)  D3D11Base::ID3D11ShaderResourceView **ppShaderResourceViews) ;
        
    STDMETHOD_(void, DSGetShader)(THIS_
        /* [annotation] */ 
        __out  D3D11Base::ID3D11DomainShader **ppDomainShader,
        /* [annotation] */ 
        __out_ecount_opt(*pNumClassInstances)  D3D11Base::ID3D11ClassInstance **ppClassInstances,
        /* [annotation] */ 
        __inout_opt  UINT *pNumClassInstances) ;
        
    STDMETHOD_(void, DSGetSamplers)(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
        /* [annotation] */ 
        __out_ecount(NumSamplers)  D3D11Base::ID3D11SamplerState **ppSamplers) ;
        
    STDMETHOD_(void, DSGetConstantBuffers)(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
        /* [annotation] */ 
        __out_ecount(NumBuffers)  D3D11Base::ID3D11Buffer **ppConstantBuffers) ;
        
    STDMETHOD_(void, CSGetShaderResources)(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
        /* [annotation] */ 
        __out_ecount(NumViews)  D3D11Base::ID3D11ShaderResourceView **ppShaderResourceViews) ;
        
    STDMETHOD_(void, CSGetUnorderedAccessViews)(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_PS_CS_UAV_REGISTER_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        __in_range( 0, D3D11_PS_CS_UAV_REGISTER_COUNT - StartSlot )  UINT NumUAVs,
        /* [annotation] */ 
        __out_ecount(NumUAVs)  D3D11Base::ID3D11UnorderedAccessView **ppUnorderedAccessViews) ;
        
    STDMETHOD_(void, CSGetShader)(THIS_
        /* [annotation] */ 
        __out  D3D11Base::ID3D11ComputeShader **ppComputeShader,
        /* [annotation] */ 
        __out_ecount_opt(*pNumClassInstances)  D3D11Base::ID3D11ClassInstance **ppClassInstances,
        /* [annotation] */ 
        __inout_opt  UINT *pNumClassInstances) ;
        
    STDMETHOD_(void, CSGetSamplers)(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
        /* [annotation] */ 
        __out_ecount(NumSamplers)  D3D11Base::ID3D11SamplerState **ppSamplers) ;
        
    STDMETHOD_(void, CSGetConstantBuffers)(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
        /* [annotation] */ 
        __out_ecount(NumBuffers)  D3D11Base::ID3D11Buffer **ppConstantBuffers) ;
        
    STDMETHOD_(void, ClearState)(THIS);
        
    STDMETHOD_(void, Flush)(THIS);
        
    STDMETHOD_(D3D11Base::D3D11_DEVICE_CONTEXT_TYPE, GetType)(THIS);
        
    STDMETHOD_(UINT, GetContextFlags)(THIS);
        
    STDMETHOD(FinishCommandList)(THIS_ 
        BOOL RestoreDeferredContextState,
        /* [annotation] */ 
        __out_opt  D3D11Base::ID3D11CommandList **ppCommandList);
        
};

// 54ec77fa-1377-44e6-8c32-88fd5f44c84c = IDXGIDevice
// 77db970f-6276-48ba-ba28-070143b4392c = IDXGIDevice1
// 05008617-fbfd-4051-a790-144884b4f6a9 = IDXGIDevice2
class IDXGIDevice2 : public IDirect3DUnknown
{
public:
	static ThreadSafePointerSet	m_List;

	IDXGIDevice2(D3D11Base::IDXGIDevice2 *pDevice);
    static IDXGIDevice2* GetDirectDevice2(D3D11Base::IDXGIDevice2 *pOrig);
    inline D3D11Base::IDXGIDevice2 *GetDevice2() { return (D3D11Base::IDXGIDevice2*) m_pUnk; }

    // *** IDirect3DUnknown methods 
	STDMETHOD_(ULONG,AddRef)(THIS);
    STDMETHOD_(ULONG,Release)(THIS);

	// *** IDXGIObject methods
    STDMETHOD(SetPrivateData)(THIS_ 
            /* [annotation][in] */ 
            __in  REFGUID Name,
            /* [in] */ UINT DataSize,
            /* [annotation][in] */ 
            __in_bcount(DataSize)  const void *pData);
        
    STDMETHOD(SetPrivateDataInterface)(THIS_ 
            /* [annotation][in] */ 
            __in  REFGUID Name,
            /* [annotation][in] */ 
            __in  const IUnknown *pUnknown);
        
    STDMETHOD(GetPrivateData)(THIS_
            /* [annotation][in] */ 
            __in  REFGUID Name,
            /* [annotation][out][in] */ 
            __inout  UINT *pDataSize,
            /* [annotation][out] */ 
            __out_bcount(*pDataSize)  void *pData);
        
    STDMETHOD(GetParent)(THIS_ 
            /* [annotation][in] */ 
            __in  REFIID riid,
            /* [annotation][retval][out] */ 
            __out  void **ppParent);

	// *** IDXGIDevice methods
	STDMETHOD(GetAdapter)(THIS_
            /* [annotation][out] */ 
            _Out_  D3D11Base::IDXGIAdapter **pAdapter);
        
    STDMETHOD(CreateSurface)(THIS_
            /* [annotation][in] */ 
            _In_  const D3D11Base::DXGI_SURFACE_DESC *pDesc,
            /* [in] */ UINT NumSurfaces,
            /* [in] */ D3D11Base::DXGI_USAGE Usage,
            /* [annotation][in] */ 
            _In_opt_  const D3D11Base::DXGI_SHARED_RESOURCE *pSharedResource,
            /* [annotation][out] */ 
            _Out_  D3D11Base::IDXGISurface **ppSurface);
        
	STDMETHOD(QueryResourceResidency)(THIS_
            /* [annotation][size_is][in] */ 
            _In_reads_(NumResources)  IUnknown *const *ppResources,
            /* [annotation][size_is][out] */ 
            _Out_writes_(NumResources)  D3D11Base::DXGI_RESIDENCY *pResidencyStatus,
            /* [in] */ UINT NumResources);
        
    STDMETHOD(SetGPUThreadPriority)(THIS_
            /* [in] */ INT Priority);
        
    STDMETHOD(GetGPUThreadPriority)(THIS_
            /* [annotation][retval][out] */ 
            _Out_  INT *pPriority);
    
	// *** IDXGIDevice1 methods
    STDMETHOD(SetMaximumFrameLatency)(THIS_
            /* [in] */ UINT MaxLatency);
        
    STDMETHOD(GetMaximumFrameLatency)(THIS_
            /* [annotation][out] */ 
            _Out_  UINT *pMaxLatency);

	// *** IDXGIDevice2 methods
    STDMETHOD(OfferResources)(THIS_
            /* [annotation][in] */ 
            _In_  UINT NumResources,
            /* [annotation][size_is][in] */ 
            _In_reads_(NumResources)  D3D11Base::IDXGIResource *const *ppResources,
            /* [annotation][in] */ 
            _In_  D3D11Base::DXGI_OFFER_RESOURCE_PRIORITY Priority);
        
    STDMETHOD(ReclaimResources)(THIS_
            /* [annotation][in] */ 
            _In_  UINT NumResources,
            /* [annotation][size_is][in] */ 
            _In_reads_(NumResources)  D3D11Base::IDXGIResource *const *ppResources,
            /* [annotation][size_is][out] */ 
            _Out_writes_all_opt_(NumResources)  BOOL *pDiscarded);
        
    STDMETHOD(EnqueueSetEvent)(THIS_
            /* [annotation][in] */ 
            _In_  HANDLE hEvent);
};


UINT64 CalcTexture2DDescHash(const D3D11Base::D3D11_TEXTURE2D_DESC *desc,
		UINT64 initial_hash, int override_width, int override_height);
UINT64 CalcTexture3DDescHash(const D3D11Base::D3D11_TEXTURE3D_DESC *desc,
		UINT64 initial_hash, int override_width, int override_height);

#include "../DirectX10/d3d10WrapperDevice.h"
