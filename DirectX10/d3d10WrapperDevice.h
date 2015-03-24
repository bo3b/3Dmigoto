#pragma once

// 9B7E4C00-342C-4106-A19F-4F2704F689F0 ID3D10DeviceChild
// 9B7E4C0F-342C-4106-A19F-4F2704F689F0 ID3D10Device

class ID3D10Device : public IDirect3DUnknown
{
public:
	static ThreadSafePointerSet	 m_List;
	D3D10Base::StereoHandle mStereoHandle;
	D3D10Base::nv::stereo::ParamTextureManagerD3D10 mParamTextureManager;
	D3D10Base::ID3D10Texture2D *mStereoTexture;
	D3D10Base::ID3D10ShaderResourceView *mStereoResourceView;
	D3D10Base::ID3D10ShaderResourceView *mZBufferResourceView;
	D3D10Base::ID3D10Texture1D *mIniTexture;
	D3D10Base::ID3D10ShaderResourceView *mIniResourceView;

	ID3D10Device(D3D10Base::ID3D10Device *pDevice);
	static ID3D10Device* GetDirect3DDevice(D3D10Base::ID3D10Device *pDevice);
	__forceinline D3D10Base::ID3D10Device *GetD3D10Device() { return (D3D10Base::ID3D10Device*) m_pUnk; }

    // *** IDirect3DUnknown methods 
	STDMETHOD_(ULONG,AddRef)(THIS);
    STDMETHOD_(ULONG,Release)(THIS);

	// *** ID3D10DeviceChild methods
	/*
	STDMETHOD_(void, GetDevice)(THIS_
            __out  ID3D10Device **ppDevice);
        
    STDMETHOD(GetPrivateData)(THIS_
            __in  REFGUID guid,
            __inout  UINT *pDataSize,
            __out_bcount_opt(*pDataSize)  void *pData);
        
    STDMETHOD(SetPrivateData)(THIS_
            __in  REFGUID guid,
            __in  UINT DataSize,
            __in_bcount_opt(DataSize)  const void *pData);
        
	STDMETHOD(SetPrivateDataInterface)(THIS_
            __in  REFGUID guid,
            __in_opt  const IUnknown *pData);
	*/
        
	/*** ID3D10Device methods ***/
	STDMETHOD_(void, VSSetConstantBuffers)(THIS_
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
            /* [annotation] */ 
            __in_ecount(NumBuffers)  D3D10Base::ID3D10Buffer *const *ppConstantBuffers);
        
	STDMETHOD_(void, PSSetShaderResources)(THIS_
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
            /* [annotation] */ 
            __in_ecount(NumViews)  D3D10Base::ID3D10ShaderResourceView *const *ppShaderResourceViews);
        
	STDMETHOD_(void, PSSetShader)(THIS_
            /* [annotation] */ 
            __in_opt  D3D10Base::ID3D10PixelShader *pPixelShader);
        
	STDMETHOD_(void, PSSetSamplers)(THIS_
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
            /* [annotation] */ 
            __in_ecount(NumSamplers)  D3D10Base::ID3D10SamplerState *const *ppSamplers);
        
	STDMETHOD_(void, VSSetShader)(THIS_
            /* [annotation] */ 
            __in_opt  D3D10Base::ID3D10VertexShader *pVertexShader);
        
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
        
	STDMETHOD_(void, PSSetConstantBuffers)(THIS_ 
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
            /* [annotation] */ 
            __in_ecount(NumBuffers)  D3D10Base::ID3D10Buffer *const *ppConstantBuffers);
        
	STDMETHOD_(void, IASetInputLayout)(THIS_
            /* [annotation] */ 
            __in_opt  D3D10Base::ID3D10InputLayout *pInputLayout);
        
	STDMETHOD_(void, IASetVertexBuffers)(THIS_
            /* [annotation] */ 
            __in_range( 0, D3D10_1_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
            /* [annotation] */ 
            __in_range( 0, D3D10_1_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumBuffers,
            /* [annotation] */ 
            __in_ecount(NumBuffers)  D3D10Base::ID3D10Buffer *const *ppVertexBuffers,
            /* [annotation] */ 
            __in_ecount(NumBuffers)  const UINT *pStrides,
            /* [annotation] */ 
            __in_ecount(NumBuffers)  const UINT *pOffsets);
        
	STDMETHOD_(void, IASetIndexBuffer)(THIS_
            /* [annotation] */ 
            __in_opt  D3D10Base::ID3D10Buffer *pIndexBuffer,
            /* [annotation] */ 
            __in  D3D10Base::DXGI_FORMAT Format,
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
            __in_range( 0, D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
            /* [annotation] */ 
            __in_ecount(NumBuffers)  D3D10Base::ID3D10Buffer *const *ppConstantBuffers);
        
	STDMETHOD_(void, GSSetShader)(THIS_
            /* [annotation] */ 
            __in_opt  D3D10Base::ID3D10GeometryShader *pShader);
        
	STDMETHOD_(void, IASetPrimitiveTopology)(THIS_
            /* [annotation] */ 
            __in  D3D10Base::D3D10_PRIMITIVE_TOPOLOGY Topology);
        
	STDMETHOD_(void, VSSetShaderResources)(THIS_
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
            /* [annotation] */ 
            __in_ecount(NumViews)  D3D10Base::ID3D10ShaderResourceView *const *ppShaderResourceViews);
        
	STDMETHOD_(void, VSSetSamplers)(THIS_
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
            /* [annotation] */ 
            __in_ecount(NumSamplers)  D3D10Base::ID3D10SamplerState *const *ppSamplers);
        
	STDMETHOD_(void, SetPredication)(THIS_
            /* [annotation] */ 
            __in_opt  D3D10Base::ID3D10Predicate *pPredicate,
            /* [annotation] */ 
            __in  BOOL PredicateValue);
        
	STDMETHOD_(void, GSSetShaderResources)(THIS_
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
            /* [annotation] */ 
            __in_ecount(NumViews)  D3D10Base::ID3D10ShaderResourceView *const *ppShaderResourceViews);
        
	STDMETHOD_(void, GSSetSamplers)(THIS_
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
            /* [annotation] */ 
            __in_ecount(NumSamplers)  D3D10Base::ID3D10SamplerState *const *ppSamplers);
        
	STDMETHOD_(void, OMSetRenderTargets)(THIS_
            /* [annotation] */ 
            __in_range( 0, D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT )  UINT NumViews,
            /* [annotation] */ 
            __in_ecount_opt(NumViews)  D3D10Base::ID3D10RenderTargetView *const *ppRenderTargetViews,
            /* [annotation] */ 
            __in_opt  D3D10Base::ID3D10DepthStencilView *pDepthStencilView);
        
	STDMETHOD_(void, OMSetBlendState)(THIS_
            /* [annotation] */ 
            __in_opt  D3D10Base::ID3D10BlendState *pBlendState,
            /* [annotation] */ 
            __in  const FLOAT BlendFactor[ 4 ],
            /* [annotation] */ 
            __in  UINT SampleMask);
        
	STDMETHOD_(void, OMSetDepthStencilState)(THIS_
            /* [annotation] */ 
            __in_opt  D3D10Base::ID3D10DepthStencilState *pDepthStencilState,
            /* [annotation] */ 
            __in  UINT StencilRef);
        
	STDMETHOD_(void, SOSetTargets)(THIS_
            /* [annotation] */ 
            __in_range( 0, D3D10_SO_BUFFER_SLOT_COUNT)  UINT NumBuffers,
            /* [annotation] */ 
            __in_ecount_opt(NumBuffers)  D3D10Base::ID3D10Buffer *const *ppSOTargets,
            /* [annotation] */ 
            __in_ecount_opt(NumBuffers)  const UINT *pOffsets);
        
	STDMETHOD_(void, DrawAuto)(THIS);
        
	STDMETHOD_(void, RSSetState)(THIS_
            /* [annotation] */ 
            __in_opt  D3D10Base::ID3D10RasterizerState *pRasterizerState);
        
	STDMETHOD_(void, RSSetViewports)(THIS_
            /* [annotation] */ 
            __in_range(0, D3D10_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE)  UINT NumViewports,
            /* [annotation] */ 
            __in_ecount_opt(NumViewports)  const D3D10Base::D3D10_VIEWPORT *pViewports);
        
	STDMETHOD_(void, RSSetScissorRects)(THIS_
            /* [annotation] */ 
            __in_range(0, D3D10_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE)  UINT NumRects,
            /* [annotation] */ 
            __in_ecount_opt(NumRects)  const D3D10Base::D3D10_RECT *pRects);
        
	STDMETHOD_(void, CopySubresourceRegion)(THIS_
            /* [annotation] */ 
            __in  D3D10Base::ID3D10Resource *pDstResource,
            /* [annotation] */ 
            __in  UINT DstSubresource,
            /* [annotation] */ 
            __in  UINT DstX,
            /* [annotation] */ 
            __in  UINT DstY,
            /* [annotation] */ 
            __in  UINT DstZ,
            /* [annotation] */ 
            __in  D3D10Base::ID3D10Resource *pSrcResource,
            /* [annotation] */ 
            __in  UINT SrcSubresource,
            /* [annotation] */ 
            __in_opt  const D3D10Base::D3D10_BOX *pSrcBox);
        
	STDMETHOD_(void, CopyResource)(THIS_
            /* [annotation] */ 
            __in  D3D10Base::ID3D10Resource *pDstResource,
            /* [annotation] */ 
            __in  D3D10Base::ID3D10Resource *pSrcResource);
        
	STDMETHOD_(void, UpdateSubresource)(THIS_
            /* [annotation] */ 
            __in  D3D10Base::ID3D10Resource *pDstResource,
            /* [annotation] */ 
            __in  UINT DstSubresource,
            /* [annotation] */ 
            __in_opt  const D3D10Base::D3D10_BOX *pDstBox,
            /* [annotation] */ 
            __in  const void *pSrcData,
            /* [annotation] */ 
            __in  UINT SrcRowPitch,
            /* [annotation] */ 
            __in  UINT SrcDepthPitch);
        
	STDMETHOD_(void, ClearRenderTargetView)(THIS_
            /* [annotation] */ 
            __in  D3D10Base::ID3D10RenderTargetView *pRenderTargetView,
            /* [annotation] */ 
            __in  const FLOAT ColorRGBA[ 4 ]);
        
	STDMETHOD_(void, ClearDepthStencilView)(THIS_
            /* [annotation] */ 
            __in  D3D10Base::ID3D10DepthStencilView *pDepthStencilView,
            /* [annotation] */ 
            __in  UINT ClearFlags,
            /* [annotation] */ 
            __in  FLOAT Depth,
            /* [annotation] */ 
            __in  UINT8 Stencil);
        
	STDMETHOD_(void, GenerateMips)(THIS_
            /* [annotation] */ 
            __in  D3D10Base::ID3D10ShaderResourceView *pShaderResourceView);
        
	STDMETHOD_(void, ResolveSubresource)(THIS_
            /* [annotation] */ 
            __in  D3D10Base::ID3D10Resource *pDstResource,
            /* [annotation] */ 
            __in  UINT DstSubresource,
            /* [annotation] */ 
            __in  D3D10Base::ID3D10Resource *pSrcResource,
            /* [annotation] */ 
            __in  UINT SrcSubresource,
            /* [annotation] */ 
            __in  D3D10Base::DXGI_FORMAT Format);
        
	STDMETHOD_(void, VSGetConstantBuffers)(THIS_
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
            /* [annotation] */ 
            __out_ecount(NumBuffers)  D3D10Base::ID3D10Buffer **ppConstantBuffers);
        
	STDMETHOD_(void, PSGetShaderResources)(THIS_
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
            /* [annotation] */ 
            __out_ecount(NumViews)  D3D10Base::ID3D10ShaderResourceView **ppShaderResourceViews);
        
	STDMETHOD_(void, PSGetShader)(THIS_
            /* [annotation] */ 
            __out  D3D10Base::ID3D10PixelShader **ppPixelShader);
        
	STDMETHOD_(void, PSGetSamplers)(THIS_
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
            /* [annotation] */ 
            __out_ecount(NumSamplers)  D3D10Base::ID3D10SamplerState **ppSamplers);
        
	STDMETHOD_(void, VSGetShader)(THIS_
            /* [annotation] */ 
            __out  D3D10Base::ID3D10VertexShader **ppVertexShader);
        
	STDMETHOD_(void, PSGetConstantBuffers)(THIS_
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
            /* [annotation] */ 
            __out_ecount(NumBuffers)  D3D10Base::ID3D10Buffer **ppConstantBuffers);
        
	STDMETHOD_(void, IAGetInputLayout)(THIS_
            /* [annotation] */ 
            __out  D3D10Base::ID3D10InputLayout **ppInputLayout);
        
	STDMETHOD_(void, IAGetVertexBuffers)(THIS_
            /* [annotation] */ 
            __in_range( 0, D3D10_1_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
            /* [annotation] */ 
            __in_range( 0, D3D10_1_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumBuffers,
            /* [annotation] */ 
            __out_ecount_opt(NumBuffers)  D3D10Base::ID3D10Buffer **ppVertexBuffers,
            /* [annotation] */ 
            __out_ecount_opt(NumBuffers)  UINT *pStrides,
            /* [annotation] */ 
            __out_ecount_opt(NumBuffers)  UINT *pOffsets);
        
	STDMETHOD_(void, IAGetIndexBuffer)(THIS_
            /* [annotation] */ 
            __out_opt  D3D10Base::ID3D10Buffer **pIndexBuffer,
            /* [annotation] */ 
            __out_opt  D3D10Base::DXGI_FORMAT *Format,
            /* [annotation] */ 
            __out_opt  UINT *Offset);
        
	STDMETHOD_(void, GSGetConstantBuffers)(THIS_
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
            /* [annotation] */ 
            __out_ecount(NumBuffers)  D3D10Base::ID3D10Buffer **ppConstantBuffers);
        
	STDMETHOD_(void, GSGetShader)(THIS_
            /* [annotation] */ 
            __out  D3D10Base::ID3D10GeometryShader **ppGeometryShader);
        
	STDMETHOD_(void, IAGetPrimitiveTopology)(THIS_
            /* [annotation] */ 
            __out  D3D10Base::D3D10_PRIMITIVE_TOPOLOGY *pTopology);
        
	STDMETHOD_(void, VSGetShaderResources)(THIS_
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
            /* [annotation] */ 
            __out_ecount(NumViews)  D3D10Base::ID3D10ShaderResourceView **ppShaderResourceViews);
        
	STDMETHOD_(void, VSGetSamplers)(THIS_
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
            /* [annotation] */ 
            __out_ecount(NumSamplers)  D3D10Base::ID3D10SamplerState **ppSamplers);
        
	STDMETHOD_(void, GetPredication)(THIS_
            /* [annotation] */ 
            __out_opt  D3D10Base::ID3D10Predicate **ppPredicate,
            /* [annotation] */ 
            __out_opt  BOOL *pPredicateValue);
        
	STDMETHOD_(void, GSGetShaderResources)(THIS_
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
            /* [annotation] */ 
            __out_ecount(NumViews)  D3D10Base::ID3D10ShaderResourceView **ppShaderResourceViews);
        
	STDMETHOD_(void, GSGetSamplers)(THIS_
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
            /* [annotation] */ 
            __out_ecount(NumSamplers)  D3D10Base::ID3D10SamplerState **ppSamplers);
        
	STDMETHOD_(void, OMGetRenderTargets)(THIS_
            /* [annotation] */ 
            __in_range( 0, D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT )  UINT NumViews,
            /* [annotation] */ 
            __out_ecount_opt(NumViews)  D3D10Base::ID3D10RenderTargetView **ppRenderTargetViews,
            /* [annotation] */ 
            __out_opt  D3D10Base::ID3D10DepthStencilView **ppDepthStencilView);
        
	STDMETHOD_(void, OMGetBlendState)(THIS_
            /* [annotation] */ 
            __out_opt  D3D10Base::ID3D10BlendState **ppBlendState,
            /* [annotation] */ 
            __out_opt  FLOAT BlendFactor[ 4 ],
            /* [annotation] */ 
            __out_opt  UINT *pSampleMask);
        
	STDMETHOD_(void, OMGetDepthStencilState)(THIS_
            /* [annotation] */ 
            __out_opt  D3D10Base::ID3D10DepthStencilState **ppDepthStencilState,
            /* [annotation] */ 
            __out_opt  UINT *pStencilRef);
        
	STDMETHOD_(void, SOGetTargets)(THIS_
            /* [annotation] */ 
            __in_range( 0, D3D10_SO_BUFFER_SLOT_COUNT )  UINT NumBuffers,
            /* [annotation] */ 
            __out_ecount_opt(NumBuffers)  D3D10Base::ID3D10Buffer **ppSOTargets,
            /* [annotation] */ 
            __out_ecount_opt(NumBuffers)  UINT *pOffsets);
        
	STDMETHOD_(void, RSGetState)(THIS_
            /* [annotation] */ 
            __out  D3D10Base::ID3D10RasterizerState **ppRasterizerState);
        
	STDMETHOD_(void, RSGetViewports)(THIS_
            /* [annotation] */ 
            __inout /*_range(0, D3D10_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE )*/   UINT *NumViewports,
            /* [annotation] */ 
            __out_ecount_opt(*NumViewports)  D3D10Base::D3D10_VIEWPORT *pViewports);
        
	STDMETHOD_(void, RSGetScissorRects)(THIS_
            /* [annotation] */ 
            __inout /*_range(0, D3D10_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE )*/   UINT *NumRects,
            /* [annotation] */ 
            __out_ecount_opt(*NumRects)  D3D10Base::D3D10_RECT *pRects);
        
	STDMETHOD(GetDeviceRemovedReason)(THIS);
        
    STDMETHOD(SetExceptionMode)(THIS_
            UINT RaiseFlags);
        
    STDMETHOD_(UINT, GetExceptionMode)(THIS);
        
    STDMETHOD(GetPrivateData)(THIS_
            __in  REFGUID guid,
            __inout  UINT *pDataSize,
            __out_bcount_opt(*pDataSize)  void *pData);
        
	STDMETHOD(SetPrivateData)(THIS_
            __in  REFGUID guid,
            __in  UINT DataSize,
            __in_bcount_opt(DataSize)  const void *pData);
        
    STDMETHOD(SetPrivateDataInterface)(THIS_
            __in  REFGUID guid,
            __in_opt  const IUnknown *pData);
        
	STDMETHOD_(void, ClearState)(THIS);
        
	STDMETHOD_(void, Flush)(THIS);
        
    STDMETHOD(CreateBuffer)(THIS_
            /* [annotation] */ 
            __in  const D3D10Base::D3D10_BUFFER_DESC *pDesc,
            /* [annotation] */ 
            __in_opt  const D3D10Base::D3D10_SUBRESOURCE_DATA *pInitialData,
            /* [annotation] */ 
            __out_opt  D3D10Base::ID3D10Buffer **ppBuffer);
        
    STDMETHOD(CreateTexture1D)(THIS_
            /* [annotation] */ 
            __in  const D3D10Base::D3D10_TEXTURE1D_DESC *pDesc,
            /* [annotation] */ 
            __in_xcount_opt(pDesc->MipLevels * pDesc->ArraySize)  const D3D10Base::D3D10_SUBRESOURCE_DATA *pInitialData,
            /* [annotation] */ 
            __out  D3D10Base::ID3D10Texture1D **ppTexture1D);
        
	STDMETHOD(CreateTexture2D)(THIS_
            /* [annotation] */ 
            __in  const D3D10Base::D3D10_TEXTURE2D_DESC *pDesc,
            /* [annotation] */ 
            __in_xcount_opt(pDesc->MipLevels * pDesc->ArraySize)  const D3D10Base::D3D10_SUBRESOURCE_DATA *pInitialData,
            /* [annotation] */ 
            __out  D3D10Base::ID3D10Texture2D **ppTexture2D);
        
	STDMETHOD(CreateTexture3D)(THIS_
            /* [annotation] */ 
            __in  const D3D10Base::D3D10_TEXTURE3D_DESC *pDesc,
            /* [annotation] */ 
            __in_xcount_opt(pDesc->MipLevels)  const D3D10Base::D3D10_SUBRESOURCE_DATA *pInitialData,
            /* [annotation] */ 
            __out  D3D10Base::ID3D10Texture3D **ppTexture3D);
        
	STDMETHOD(CreateShaderResourceView)(THIS_
            /* [annotation] */ 
            __in  D3D10Base::ID3D10Resource *pResource,
            /* [annotation] */ 
            __in_opt  const D3D10Base::D3D10_SHADER_RESOURCE_VIEW_DESC *pDesc,
            /* [annotation] */ 
            __out_opt  D3D10Base::ID3D10ShaderResourceView **ppSRView);
        
	STDMETHOD(CreateRenderTargetView)(THIS_
            /* [annotation] */ 
            __in  D3D10Base::ID3D10Resource *pResource,
            /* [annotation] */ 
            __in_opt  const D3D10Base::D3D10_RENDER_TARGET_VIEW_DESC *pDesc,
            /* [annotation] */ 
            __out_opt  D3D10Base::ID3D10RenderTargetView **ppRTView);
        
	STDMETHOD(CreateDepthStencilView)(THIS_
            /* [annotation] */ 
            __in  D3D10Base::ID3D10Resource *pResource,
            /* [annotation] */ 
            __in_opt  const D3D10Base::D3D10_DEPTH_STENCIL_VIEW_DESC *pDesc,
            /* [annotation] */ 
            __out_opt  D3D10Base::ID3D10DepthStencilView **ppDepthStencilView);
        
	STDMETHOD(CreateInputLayout)(THIS_
            /* [annotation] */ 
            __in_ecount(NumElements)  const D3D10Base::D3D10_INPUT_ELEMENT_DESC *pInputElementDescs,
            /* [annotation] */ 
            __in_range( 0, D3D10_1_IA_VERTEX_INPUT_STRUCTURE_ELEMENT_COUNT )  UINT NumElements,
            /* [annotation] */ 
            __in  const void *pShaderBytecodeWithInputSignature,
            /* [annotation] */ 
            __in  SIZE_T BytecodeLength,
            /* [annotation] */ 
            __out_opt  D3D10Base::ID3D10InputLayout **ppInputLayout);
        
	STDMETHOD(CreateVertexShader)(THIS_
            /* [annotation] */ 
            __in  const void *pShaderBytecode,
            /* [annotation] */ 
            __in  SIZE_T BytecodeLength,
            /* [annotation] */ 
            __out_opt  D3D10Base::ID3D10VertexShader **ppVertexShader);
        
	STDMETHOD(CreateGeometryShader)(THIS_
            /* [annotation] */ 
            __in  const void *pShaderBytecode,
            /* [annotation] */ 
            __in  SIZE_T BytecodeLength,
            /* [annotation] */ 
            __out_opt  D3D10Base::ID3D10GeometryShader **ppGeometryShader);
        
	STDMETHOD(CreateGeometryShaderWithStreamOutput)(THIS_
            /* [annotation] */ 
            __in  const void *pShaderBytecode,
            /* [annotation] */ 
            __in  SIZE_T BytecodeLength,
            /* [annotation] */ 
            __in_ecount_opt(NumEntries)  const D3D10Base::D3D10_SO_DECLARATION_ENTRY *pSODeclaration,
            /* [annotation] */ 
            __in_range( 0, D3D10_SO_SINGLE_BUFFER_COMPONENT_LIMIT )  UINT NumEntries,
            /* [annotation] */ 
            __in  UINT OutputStreamStride,
            /* [annotation] */ 
            __out_opt  D3D10Base::ID3D10GeometryShader **ppGeometryShader);
        
	STDMETHOD(CreatePixelShader)(THIS_
            /* [annotation] */ 
            __in  const void *pShaderBytecode,
            /* [annotation] */ 
            __in  SIZE_T BytecodeLength,
            /* [annotation] */ 
            __out_opt  D3D10Base::ID3D10PixelShader **ppPixelShader);
        
	STDMETHOD(CreateBlendState)(THIS_
            /* [annotation] */ 
            __in  const D3D10Base::D3D10_BLEND_DESC *pBlendStateDesc,
            /* [annotation] */ 
            __out_opt  D3D10Base::ID3D10BlendState **ppBlendState);
        
	STDMETHOD(CreateDepthStencilState)(THIS_
            /* [annotation] */ 
            __in  const D3D10Base::D3D10_DEPTH_STENCIL_DESC *pDepthStencilDesc,
            /* [annotation] */ 
            __out_opt  D3D10Base::ID3D10DepthStencilState **ppDepthStencilState);
        
	STDMETHOD(CreateRasterizerState)(THIS_
            /* [annotation] */ 
            __in  const D3D10Base::D3D10_RASTERIZER_DESC *pRasterizerDesc,
            /* [annotation] */ 
            __out_opt  D3D10Base::ID3D10RasterizerState **ppRasterizerState);
        
	STDMETHOD(CreateSamplerState)(THIS_
            /* [annotation] */ 
            __in  const D3D10Base::D3D10_SAMPLER_DESC *pSamplerDesc,
            /* [annotation] */ 
            __out_opt  D3D10Base::ID3D10SamplerState **ppSamplerState);
        
	STDMETHOD(CreateQuery)(THIS_
            /* [annotation] */ 
            __in  const D3D10Base::D3D10_QUERY_DESC *pQueryDesc,
            /* [annotation] */ 
            __out_opt  D3D10Base::ID3D10Query **ppQuery);
        
	STDMETHOD(CreatePredicate)(THIS_
            /* [annotation] */ 
            __in  const D3D10Base::D3D10_QUERY_DESC *pPredicateDesc,
            /* [annotation] */ 
            __out_opt  D3D10Base::ID3D10Predicate **ppPredicate);
        
	STDMETHOD(CreateCounter)(THIS_
            /* [annotation] */ 
            __in  const D3D10Base::D3D10_COUNTER_DESC *pCounterDesc,
            /* [annotation] */ 
            __out_opt  D3D10Base::ID3D10Counter **ppCounter);
        
	STDMETHOD(CheckFormatSupport)(THIS_
            /* [annotation] */ 
            __in  D3D10Base::DXGI_FORMAT Format,
            /* [annotation] */ 
            __out  UINT *pFormatSupport);
        
	STDMETHOD(CheckMultisampleQualityLevels)(THIS_
            /* [annotation] */ 
            __in  D3D10Base::DXGI_FORMAT Format,
            /* [annotation] */ 
            __in  UINT SampleCount,
            /* [annotation] */ 
            __out  UINT *pNumQualityLevels);
        
    STDMETHOD_(void, CheckCounterInfo)(THIS_
            /* [annotation] */ 
            __out  D3D10Base::D3D10_COUNTER_INFO *pCounterInfo);
        
	STDMETHOD(CheckCounter)(THIS_
            /* [annotation] */ 
            __in  const D3D10Base::D3D10_COUNTER_DESC *pDesc,
            /* [annotation] */ 
            __out  D3D10Base::D3D10_COUNTER_TYPE *pType,
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
        
    STDMETHOD_(UINT, GetCreationFlags)(THIS);
        
	STDMETHOD(OpenSharedResource)(THIS_
            /* [annotation] */ 
            __in  HANDLE hResource,
            /* [annotation] */ 
            __in  REFIID ReturnedInterface,
            /* [annotation] */ 
            __out_opt  void **ppResource);
        
	STDMETHOD_(void, SetTextFilterSize)(THIS_
            /* [annotation] */ 
            __in  UINT Width,
            /* [annotation] */ 
            __in  UINT Height);
        
	STDMETHOD_(void, GetTextFilterSize)(THIS_
            /* [annotation] */ 
            __out_opt  UINT *pWidth,
            /* [annotation] */ 
            __out_opt  UINT *pHeight);
        
};

/*------------------------------------------------------------------*/

// MIDL_INTERFACE("9B7E4E00-342C-4106-A19F-4F2704F689F0")
class ID3D10Multithread : public IDirect3DUnknown
{
public:
	static ThreadSafePointerSet	 m_List;

	ID3D10Multithread(D3D10Base::ID3D10Multithread *pDevice);
	static ID3D10Multithread* GetDirect3DDevice(D3D10Base::ID3D10Multithread *pDevice);
	__forceinline D3D10Base::ID3D10Multithread *GetD3D10MultithreadDevice() { return (D3D10Base::ID3D10Multithread*) m_pUnk; }

	static ID3D10Multithread* GetDirect3DMultithread(D3D10Base::ID3D10Multithread *pDevice);

    // *** IDirect3DUnknown methods 
	STDMETHOD_(ULONG,AddRef)(THIS);
    STDMETHOD_(ULONG,Release)(THIS);

	// *** ID3D10Multithread methods
	STDMETHOD_(void, Enter)(THIS);
        
    STDMETHOD_(void, Leave)(THIS);
        
    STDMETHOD_(BOOL, SetMultithreadProtected)(THIS_
            /* [annotation] */ 
            __in  BOOL bMTProtect);
        
    STDMETHOD_(BOOL, GetMultithreadProtected)(THIS);
};    
