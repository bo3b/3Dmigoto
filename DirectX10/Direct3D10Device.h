
D3D10Wrapper::ID3D10Device::ID3D10Device(D3D10Base::ID3D10Device *pDevice)
    : D3D10Wrapper::IDirect3DUnknown((IUnknown*) pDevice)
{
    m_pDevice = pDevice;
}

D3D10Wrapper::ID3D10Device* D3D10Wrapper::ID3D10Device::GetDirect3DDevice(D3D10Base::ID3D10Device *pOrig)
{
    D3D10Wrapper::ID3D10Device* p = (D3D10Wrapper::ID3D10Device*) m_List.GetDataPtr(pOrig);
    if (!p)
    {
        p = new D3D10Wrapper::ID3D10Device(pOrig);
        if (pOrig) m_List.AddMember(pOrig,p);
    }
    return p;
}

STDMETHODIMP_(ULONG) D3D10Wrapper::ID3D10Device::AddRef(THIS)
{
	m_pUnk->AddRef();
    return ++m_ulRef;
}

STDMETHODIMP_(ULONG) D3D10Wrapper::ID3D10Device::Release(THIS)
{
	LogInfo("ID3D10Device::Release handle=%p, counter=%d\n", m_pUnk, m_ulRef);
	
    m_pUnk->Release();

    ULONG ulRef = --m_ulRef;

    if(ulRef <= 0)
    {
		LogInfo("  deleting self\n");
		
        if (m_pUnk) m_List.DeleteMember(m_pUnk); m_pUnk = 0;
        delete this;
        return 0L;
    }
    return ulRef;
}

/*
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::GetDevice(THIS_
            __out  D3D10Wrapper::ID3D10Device **ppDevice)
{
	LogInfo("ID3D10Device::GetDevice called\n");
	
	*ppDevice = this;
	AddRef();
}
*/
   
STDMETHODIMP D3D10Wrapper::ID3D10Device::GetPrivateData(THIS_
            /* [annotation] */ 
            __in  REFGUID guid,
            /* [annotation] */ 
            __inout  UINT *pDataSize,
            /* [annotation] */ 
            __out_bcount_opt(*pDataSize)  void *pData)
{
	LogInfo("ID3D10Device::GetPrivateData called with GUID = %08lx-%04hx-%04hx-%02hhx%02hhx-%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx\n", 
		guid.Data1, guid.Data2, guid.Data3, guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3], 
		guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
	
	HRESULT hr = m_pDevice->GetPrivateData(guid, pDataSize, pData);
	LogInfo("  returns result = %x, DataSize = %d\n", hr, *pDataSize);
	
	return hr;
}
        
STDMETHODIMP D3D10Wrapper::ID3D10Device::SetPrivateData(THIS_
            /* [annotation] */ 
            __in  REFGUID guid,
            /* [annotation] */ 
            __in  UINT DataSize,
            /* [annotation] */ 
            __in_bcount_opt(DataSize)  const void *pData)
{
	LogInfo("ID3D10Device::SetPrivateData called with GUID = %08lx-%04hx-%04hx-%02hhx%02hhx-%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx\n", 
		guid.Data1, guid.Data2, guid.Data3, guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3], 
		guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
	LogInfo("  DataSize = %d\n", DataSize);
	
	HRESULT hr = m_pDevice->SetPrivateData(guid, DataSize, pData);
	LogInfo("  returns result = %x\n", hr);
	
	return hr;
}
        
STDMETHODIMP D3D10Wrapper::ID3D10Device::SetPrivateDataInterface(THIS_
            /* [annotation] */ 
            __in  REFGUID guid,
            /* [annotation] */ 
            __in_opt  const IUnknown *pData)
{
	LogInfo("ID3D10Device::SetPrivateDataInterface called with GUID=%08lx-%04hx-%04hx-%02hhx%02hhx-%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx\n", 
		guid.Data1, guid.Data2, guid.Data3, guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3], 
		guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
	
	HRESULT hr = m_pDevice->SetPrivateDataInterface(guid, pData);
	LogInfo("  returns result = %x\n", hr);
	
	return hr;
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::VSSetConstantBuffers(THIS_
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
            /* [annotation] */ 
            __in_ecount(NumBuffers)  D3D10Base::ID3D10Buffer *const *ppConstantBuffers)
{
	//LogInfo("ID3D10Device::VSSetConstantBuffers called with StartSlot = %d, NumBuffers = %d\n", StartSlot, NumBuffers);
	//
	m_pDevice->VSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::PSSetShaderResources(THIS_
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
            /* [annotation] */ 
            __in_ecount(NumViews)  D3D10Base::ID3D10ShaderResourceView *const *ppShaderResourceViews)
{
	//LogInfo("ID3D10Device::PSSetShaderResources called\n");
	//
	m_pDevice->PSSetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::PSSetShader(THIS_
            /* [annotation] */ 
            __in_opt  D3D10Base::ID3D10PixelShader *pPixelShader)
{
	LogInfo("ID3D10Device::PSSetShader called with pixelshader handle = %p\n", pPixelShader);

	m_pDevice->PSSetShader(pPixelShader);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::PSSetSamplers(THIS_
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
            /* [annotation] */ 
            __in_ecount(NumSamplers)  D3D10Base::ID3D10SamplerState *const *ppSamplers)
{
	//LogInfo("ID3D10Device::PSSetSamplers called\n");
	//
	m_pDevice->PSSetSamplers(StartSlot, NumSamplers, ppSamplers);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::VSSetShader(THIS_
            /* [annotation] */ 
            __in_opt  D3D10Base::ID3D10VertexShader *pVertexShader)
{
	// :todo: intercept here
	//LogInfo("ID3D10Device::VSSetShader called\n");
	//
	m_pDevice->VSSetShader(pVertexShader);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::DrawIndexed(THIS_
            /* [annotation] */ 
            __in  UINT IndexCount,
            /* [annotation] */ 
            __in  UINT StartIndexLocation,
            /* [annotation] */ 
            __in  INT BaseVertexLocation)
{
	LogInfo("ID3D10Device::DrawIndexed called with IndexCount = %d, StartIndexLocation = %d, BaseVertexLocation = %d\n",
		IndexCount, StartIndexLocation, BaseVertexLocation);
	
	m_pDevice->DrawIndexed(IndexCount, StartIndexLocation, BaseVertexLocation);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::Draw(THIS_
            /* [annotation] */ 
            __in  UINT VertexCount,
            /* [annotation] */ 
            __in  UINT StartVertexLocation)
{
	LogInfo("ID3D10Device::Draw called with VertexCount = %d, StartVertexLocation = %d\n", 
		VertexCount, StartVertexLocation);
	
	m_pDevice->Draw(VertexCount, StartVertexLocation);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::PSSetConstantBuffers(THIS_ 
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
            /* [annotation] */ 
            __in_ecount(NumBuffers)  D3D10Base::ID3D10Buffer *const *ppConstantBuffers)
{
	//LogInfo("ID3D10Device::PSSetConstantBuffers called\n");
	//
	m_pDevice->PSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::IASetInputLayout(THIS_
            /* [annotation] */ 
            __in_opt  D3D10Base::ID3D10InputLayout *pInputLayout)
{
	//LogInfo("ID3D10Device::IASetInputLayout called\n");
	//
	m_pDevice->IASetInputLayout(pInputLayout);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::IASetVertexBuffers(THIS_
            /* [annotation] */ 
            __in_range( 0, D3D10_1_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
            /* [annotation] */ 
            __in_range( 0, D3D10_1_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumBuffers,
            /* [annotation] */ 
            __in_ecount(NumBuffers)  D3D10Base::ID3D10Buffer *const *ppVertexBuffers,
            /* [annotation] */ 
            __in_ecount(NumBuffers)  const UINT *pStrides,
            /* [annotation] */ 
            __in_ecount(NumBuffers)  const UINT *pOffsets)
{
	//LogInfo("ID3D10Device::IASetVertexBuffers called\n");
	//
	m_pDevice->IASetVertexBuffers(StartSlot, NumBuffers, ppVertexBuffers, pStrides, pOffsets);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::IASetIndexBuffer(THIS_
            /* [annotation] */ 
            __in_opt  D3D10Base::ID3D10Buffer *pIndexBuffer,
            /* [annotation] */ 
            __in  D3D10Base::DXGI_FORMAT Format,
            /* [annotation] */ 
            __in  UINT Offset)
{
	//LogInfo("ID3D10Device::IASetIndexBuffer called\n");
	//
	m_pDevice->IASetIndexBuffer(pIndexBuffer, Format, Offset);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::DrawIndexedInstanced(THIS_
            /* [annotation] */ 
            __in  UINT IndexCountPerInstance,
            /* [annotation] */ 
            __in  UINT InstanceCount,
            /* [annotation] */ 
            __in  UINT StartIndexLocation,
            /* [annotation] */ 
            __in  INT BaseVertexLocation,
            /* [annotation] */ 
            __in  UINT StartInstanceLocation)
{
	LogInfo("ID3D10Device::DrawIndexedInstanced called\n");
	
	m_pDevice->DrawIndexedInstanced(IndexCountPerInstance, InstanceCount, StartIndexLocation, 
		BaseVertexLocation, StartInstanceLocation);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::DrawInstanced(THIS_
            /* [annotation] */ 
            __in  UINT VertexCountPerInstance,
            /* [annotation] */ 
            __in  UINT InstanceCount,
            /* [annotation] */ 
            __in  UINT StartVertexLocation,
            /* [annotation] */ 
            __in  UINT StartInstanceLocation)
{
	LogInfo("ID3D10Device::DrawInstanced called\n");
	
	m_pDevice->DrawInstanced(VertexCountPerInstance, InstanceCount, StartVertexLocation,
		StartInstanceLocation);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::GSSetConstantBuffers(THIS_
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
            /* [annotation] */ 
            __in_ecount(NumBuffers)  D3D10Base::ID3D10Buffer *const *ppConstantBuffers)
{
	LogInfo("ID3D10Device::GSSetConstantBuffers called\n");
	
	m_pDevice->GSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::GSSetShader(THIS_
            /* [annotation] */ 
            __in_opt  D3D10Base::ID3D10GeometryShader *pShader)
{
	LogInfo("ID3D10Device::GSSetShader called\n");
	
	m_pDevice->GSSetShader(pShader);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::IASetPrimitiveTopology(THIS_
            /* [annotation] */ 
            __in  D3D10Base::D3D10_PRIMITIVE_TOPOLOGY Topology)
{
	LogInfo("ID3D10Device::IASetPrimitiveTopology called\n");
	
	m_pDevice->IASetPrimitiveTopology(Topology);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::VSSetShaderResources(THIS_
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
            /* [annotation] */ 
            __in_ecount(NumViews)  D3D10Base::ID3D10ShaderResourceView *const *ppShaderResourceViews)
{
	//LogInfo("ID3D10Device::VSSetShaderResources called with StartSlot = %d, NumViews = %d\n",
	//	StartSlot, NumViews);
	//
	m_pDevice->VSSetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::VSSetSamplers(THIS_
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
            /* [annotation] */ 
            __in_ecount(NumSamplers)  D3D10Base::ID3D10SamplerState *const *ppSamplers)
{
	LogInfo("ID3D10Device::VSSetSamplers called with StartSlot = %d, NumSamplers = %d\n",
		StartSlot, NumSamplers);
	
	m_pDevice->VSSetSamplers(StartSlot, NumSamplers, ppSamplers);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::SetPredication(THIS_
            /* [annotation] */ 
            __in_opt  D3D10Base::ID3D10Predicate *pPredicate,
            /* [annotation] */ 
            __in  BOOL PredicateValue)
{
	LogInfo("ID3D10Device::SetPredication called\n");
	
	m_pDevice->SetPredication(pPredicate, PredicateValue);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::GSSetShaderResources(THIS_
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
            /* [annotation] */ 
            __in_ecount(NumViews)  D3D10Base::ID3D10ShaderResourceView *const *ppShaderResourceViews)
{
	LogInfo("ID3D10Device::GSSetShaderResources called\n");
	
	m_pDevice->GSSetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::GSSetSamplers(THIS_
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
            /* [annotation] */ 
            __in_ecount(NumSamplers)  D3D10Base::ID3D10SamplerState *const *ppSamplers)
{
	LogInfo("ID3D10Device::GSSetSamplers called with StartSlod = %d, NumSamplers = %d\n",
		StartSlot, NumSamplers);
	
	m_pDevice->GSSetSamplers(StartSlot, NumSamplers, ppSamplers);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::OMSetRenderTargets(THIS_
            /* [annotation] */ 
            __in_range( 0, D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT )  UINT NumViews,
            /* [annotation] */ 
            __in_ecount_opt(NumViews)  D3D10Base::ID3D10RenderTargetView *const *ppRenderTargetViews,
            /* [annotation] */ 
            __in_opt  D3D10Base::ID3D10DepthStencilView *pDepthStencilView)
{
	//LogInfo("ID3D10Device::OMSetRenderTargets called with NumViews = %d\n", NumViews);
	//
	m_pDevice->OMSetRenderTargets(NumViews, ppRenderTargetViews, pDepthStencilView);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::OMSetBlendState(THIS_
            /* [annotation] */ 
            __in_opt  D3D10Base::ID3D10BlendState *pBlendState,
            /* [annotation] */ 
            __in  const FLOAT BlendFactor[ 4 ],
            /* [annotation] */ 
            __in  UINT SampleMask)
{
	//LogInfo("ID3D10Device::OMSetBlendState called\n");
	//
	m_pDevice->OMSetBlendState(pBlendState, BlendFactor, SampleMask);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::OMSetDepthStencilState(THIS_
            /* [annotation] */ 
            __in_opt  D3D10Base::ID3D10DepthStencilState *pDepthStencilState,
            /* [annotation] */ 
            __in  UINT StencilRef)
{
	LogInfo("ID3D10Device::OMSetDepthStencilState called\n");
	
	m_pDevice->OMSetDepthStencilState(pDepthStencilState, StencilRef);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::SOSetTargets(THIS_
            /* [annotation] */ 
            __in_range( 0, D3D10_SO_BUFFER_SLOT_COUNT)  UINT NumBuffers,
            /* [annotation] */ 
            __in_ecount_opt(NumBuffers)  D3D10Base::ID3D10Buffer *const *ppSOTargets,
            /* [annotation] */ 
            __in_ecount_opt(NumBuffers)  const UINT *pOffsets)
{
	LogInfo("ID3D10Device::SOSetTargets called\n");
	
	m_pDevice->SOSetTargets(NumBuffers, ppSOTargets, pOffsets);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::DrawAuto(THIS)
{
	LogInfo("ID3D10Device::DrawAuto called\n");
	
	m_pDevice->DrawAuto();
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::RSSetState(THIS_
            /* [annotation] */ 
            __in_opt  D3D10Base::ID3D10RasterizerState *pRasterizerState)
{
	LogInfo("ID3D10Device::RSSetState called\n");
	
	m_pDevice->RSSetState(pRasterizerState);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::RSSetViewports(THIS_
            /* [annotation] */ 
            __in_range(0, D3D10_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE)  UINT NumViewports,
            /* [annotation] */ 
            __in_ecount_opt(NumViewports)  const D3D10Base::D3D10_VIEWPORT *pViewports)
{
	LogInfo("ID3D10Device::RSSetViewports called with NumViewports = %d\n", NumViewports);
	
	m_pDevice->RSSetViewports(NumViewports, pViewports);
	if (LogFile)
	{
		if (pViewports)
		{
			for (UINT i = 0; i < NumViewports; ++i)
			{
				LogInfo("  viewport #%d: TopLeft=(%d,%d), Width=%d, Height=%d, MinDepth=%f, MaxDepth=%f\n", i,
					pViewports[i].TopLeftX, pViewports[i].TopLeftY, pViewports[i].Width,
					pViewports[i].Height, pViewports[i].MinDepth, pViewports[i].MaxDepth);
			}
		}
	}
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::RSSetScissorRects(THIS_
            /* [annotation] */ 
            __in_range(0, D3D10_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE)  UINT NumRects,
            /* [annotation] */ 
            __in_ecount_opt(NumRects)  const D3D10Base::D3D10_RECT *pRects)
{
	LogInfo("ID3D10Device::RSSetScissorRects called\n");
	
	m_pDevice->RSSetScissorRects(NumRects, pRects);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::CopySubresourceRegion(THIS_
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
            __in_opt  const D3D10Base::D3D10_BOX *pSrcBox)
{
	LogInfo("ID3D10Device::CopySubresourceRegion called\n");
	
	m_pDevice->CopySubresourceRegion(pDstResource, DstSubresource, DstX, DstY, DstZ,
		pSrcResource, SrcSubresource, pSrcBox);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::CopyResource(THIS_
            /* [annotation] */ 
            __in  D3D10Base::ID3D10Resource *pDstResource,
            /* [annotation] */ 
            __in  D3D10Base::ID3D10Resource *pSrcResource)
{
	LogInfo("ID3D10Device::CopyResource called\n");
	
	m_pDevice->CopyResource(pDstResource, pSrcResource);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::UpdateSubresource(THIS_
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
            __in  UINT SrcDepthPitch)
{
	//LogInfo("ID3D10Device::UpdateSubresource called\n");
	//
	m_pDevice->UpdateSubresource(pDstResource, DstSubresource, pDstBox, pSrcData, SrcRowPitch, SrcDepthPitch);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::ClearRenderTargetView(THIS_
            /* [annotation] */ 
            __in  D3D10Base::ID3D10RenderTargetView *pRenderTargetView,
            /* [annotation] */ 
            __in  const FLOAT ColorRGBA[ 4 ])
{
	LogInfo("ID3D10Device::ClearRenderTargetView called with handle %p, color=(rgba)(%f,%f,%f,%f)\n", 
		pRenderTargetView, ColorRGBA[0], ColorRGBA[1], ColorRGBA[2], ColorRGBA[3]);
	
	m_pDevice->ClearRenderTargetView(pRenderTargetView, ColorRGBA);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::ClearDepthStencilView(THIS_
            /* [annotation] */ 
            __in  D3D10Base::ID3D10DepthStencilView *pDepthStencilView,
            /* [annotation] */ 
            __in  UINT ClearFlags,
            /* [annotation] */ 
            __in  FLOAT Depth,
            /* [annotation] */ 
            __in  UINT8 Stencil)
{
	LogInfo("ID3D10Device::ClearDepthStencilView called\n");
	
	m_pDevice->ClearDepthStencilView(pDepthStencilView, ClearFlags, Depth, Stencil);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::GenerateMips(THIS_
            /* [annotation] */ 
            __in  D3D10Base::ID3D10ShaderResourceView *pShaderResourceView)
{
	LogInfo("ID3D10Device::GenerateMips called\n");
	
	m_pDevice->GenerateMips(pShaderResourceView);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::ResolveSubresource(THIS_
            /* [annotation] */ 
            __in  D3D10Base::ID3D10Resource *pDstResource,
            /* [annotation] */ 
            __in  UINT DstSubresource,
            /* [annotation] */ 
            __in  D3D10Base::ID3D10Resource *pSrcResource,
            /* [annotation] */ 
            __in  UINT SrcSubresource,
            /* [annotation] */ 
            __in  D3D10Base::DXGI_FORMAT Format)
{
	LogInfo("ID3D10Device::ResolveSubresource called\n");
	
	m_pDevice->ResolveSubresource(pDstResource, DstSubresource, pSrcResource, SrcSubresource, Format);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::VSGetConstantBuffers(THIS_
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
            /* [annotation] */ 
            __out_ecount(NumBuffers)  D3D10Base::ID3D10Buffer **ppConstantBuffers)
{
	LogInfo("ID3D10Device::VSGetConstantBuffers called\n");
	
	m_pDevice->VSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::PSGetShaderResources(THIS_
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
            /* [annotation] */ 
            __out_ecount(NumViews)  D3D10Base::ID3D10ShaderResourceView **ppShaderResourceViews)
{
	LogInfo("ID3D10Device::PSGetShaderResources called\n");
	
	m_pDevice->PSGetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::PSGetShader(THIS_
            /* [annotation] */ 
            __out  D3D10Base::ID3D10PixelShader **ppPixelShader)
{
	LogInfo("ID3D10Device::PSGetShader called\n");
	
	m_pDevice->PSGetShader(ppPixelShader);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::PSGetSamplers(THIS_
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
            /* [annotation] */ 
            __out_ecount(NumSamplers)  D3D10Base::ID3D10SamplerState **ppSamplers)
{
	LogInfo("ID3D10Device::PSGetSamplers called\n");
	
	m_pDevice->PSGetSamplers(StartSlot, NumSamplers, ppSamplers);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::VSGetShader(THIS_
            /* [annotation] */ 
            __out  D3D10Base::ID3D10VertexShader **ppVertexShader)
{
	LogInfo("ID3D10Device::VSGetShader called\n");
	
	m_pDevice->VSGetShader(ppVertexShader);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::PSGetConstantBuffers(THIS_
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
            /* [annotation] */ 
            __out_ecount(NumBuffers)  D3D10Base::ID3D10Buffer **ppConstantBuffers)
{
	LogInfo("ID3D10Device::PSGetConstantBuffers called\n");
	
	m_pDevice->PSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::IAGetInputLayout(THIS_
            /* [annotation] */ 
            __out  D3D10Base::ID3D10InputLayout **ppInputLayout)
{
	LogInfo("ID3D10Device::IAGetInputLayout called\n");
	
	m_pDevice->IAGetInputLayout(ppInputLayout);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::IAGetVertexBuffers(THIS_
            /* [annotation] */ 
            __in_range( 0, D3D10_1_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
            /* [annotation] */ 
            __in_range( 0, D3D10_1_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumBuffers,
            /* [annotation] */ 
            __out_ecount_opt(NumBuffers)  D3D10Base::ID3D10Buffer **ppVertexBuffers,
            /* [annotation] */ 
            __out_ecount_opt(NumBuffers)  UINT *pStrides,
            /* [annotation] */ 
            __out_ecount_opt(NumBuffers)  UINT *pOffsets)
{
	LogInfo("ID3D10Device::IAGetVertexBuffers called\n");
	
	m_pDevice->IAGetVertexBuffers(StartSlot, NumBuffers, ppVertexBuffers, pStrides, pOffsets);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::IAGetIndexBuffer(THIS_
            /* [annotation] */ 
            __out_opt  D3D10Base::ID3D10Buffer **pIndexBuffer,
            /* [annotation] */ 
            __out_opt  D3D10Base::DXGI_FORMAT *Format,
            /* [annotation] */ 
            __out_opt  UINT *Offset)
{
	LogInfo("ID3D10Device::IAGetIndexBuffer called\n");
	
	m_pDevice->IAGetIndexBuffer(pIndexBuffer, Format, Offset);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::GSGetConstantBuffers(THIS_
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
            /* [annotation] */ 
            __out_ecount(NumBuffers)  D3D10Base::ID3D10Buffer **ppConstantBuffers)
{
	LogInfo("ID3D10Device::GSGetConstantBuffers called\n");
	
	m_pDevice->GSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::GSGetShader(THIS_
            /* [annotation] */ 
            __out  D3D10Base::ID3D10GeometryShader **ppGeometryShader)
{
	LogInfo("ID3D10Device::GSGetShader called\n");
	
	m_pDevice->GSGetShader(ppGeometryShader);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::IAGetPrimitiveTopology(THIS_
            /* [annotation] */ 
            __out  D3D10Base::D3D10_PRIMITIVE_TOPOLOGY *pTopology)
{
	LogInfo("ID3D10Device::IAGetPrimitiveTopology called\n");
	
	m_pDevice->IAGetPrimitiveTopology(pTopology);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::VSGetShaderResources(THIS_
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
            /* [annotation] */ 
            __out_ecount(NumViews)  D3D10Base::ID3D10ShaderResourceView **ppShaderResourceViews)
{
	LogInfo("ID3D10Device::VSGetShaderResources called\n");
	
	m_pDevice->VSGetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::VSGetSamplers(THIS_
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
            /* [annotation] */ 
            __out_ecount(NumSamplers)  D3D10Base::ID3D10SamplerState **ppSamplers)
{
	LogInfo("ID3D10Device::VSGetSamplers called\n");
	
	m_pDevice->VSGetSamplers(StartSlot, NumSamplers, ppSamplers);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::GetPredication(THIS_
            /* [annotation] */ 
            __out_opt  D3D10Base::ID3D10Predicate **ppPredicate,
            /* [annotation] */ 
            __out_opt  BOOL *pPredicateValue)
{
	LogInfo("ID3D10Device::GetPredication called\n");
	
	m_pDevice->GetPredication(ppPredicate, pPredicateValue);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::GSGetShaderResources(THIS_
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
            /* [annotation] */ 
            __out_ecount(NumViews)  D3D10Base::ID3D10ShaderResourceView **ppShaderResourceViews)
{
	LogInfo("ID3D10Device::GSGetShaderResources called\n");
	
	m_pDevice->GSGetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::GSGetSamplers(THIS_
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
            /* [annotation] */ 
            __out_ecount(NumSamplers)  D3D10Base::ID3D10SamplerState **ppSamplers)
{
	LogInfo("ID3D10Device::GSGetSamplers called\n");
	
	m_pDevice->GSGetSamplers(StartSlot, NumSamplers, ppSamplers);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::OMGetRenderTargets(THIS_
            /* [annotation] */ 
            __in_range( 0, D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT )  UINT NumViews,
            /* [annotation] */ 
            __out_ecount_opt(NumViews)  D3D10Base::ID3D10RenderTargetView **ppRenderTargetViews,
            /* [annotation] */ 
            __out_opt  D3D10Base::ID3D10DepthStencilView **ppDepthStencilView)
{
	LogInfo("ID3D10Device::OMGetRenderTargets called\n");
	
	m_pDevice->OMGetRenderTargets(NumViews, ppRenderTargetViews, ppDepthStencilView);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::OMGetBlendState(THIS_
            /* [annotation] */ 
            __out_opt  D3D10Base::ID3D10BlendState **ppBlendState,
            /* [annotation] */ 
            __out_opt  FLOAT BlendFactor[ 4 ],
            /* [annotation] */ 
            __out_opt  UINT *pSampleMask)
{
	LogInfo("ID3D10Device::OMGetBlendState called\n");
	
	m_pDevice->OMGetBlendState(ppBlendState, BlendFactor, pSampleMask);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::OMGetDepthStencilState(THIS_
            /* [annotation] */ 
            __out_opt  D3D10Base::ID3D10DepthStencilState **ppDepthStencilState,
            /* [annotation] */ 
            __out_opt  UINT *pStencilRef)
{
	LogInfo("ID3D10Device::OMGetDepthStencilState called\n");
	
	m_pDevice->OMGetDepthStencilState(ppDepthStencilState, pStencilRef);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::SOGetTargets(THIS_
            /* [annotation] */ 
            __in_range( 0, D3D10_SO_BUFFER_SLOT_COUNT )  UINT NumBuffers,
            /* [annotation] */ 
            __out_ecount_opt(NumBuffers)  D3D10Base::ID3D10Buffer **ppSOTargets,
            /* [annotation] */ 
            __out_ecount_opt(NumBuffers)  UINT *pOffsets)
{
	LogInfo("ID3D10Device::SOGetTargets called\n");
	
	m_pDevice->SOGetTargets(NumBuffers, ppSOTargets, pOffsets);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::RSGetState(THIS_
            /* [annotation] */ 
            __out  D3D10Base::ID3D10RasterizerState **ppRasterizerState)
{
	LogInfo("ID3D10Device::RSGetState called\n");
	
	m_pDevice->RSGetState(ppRasterizerState);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::RSGetViewports(THIS_
            /* [annotation] */ 
            __inout /*_range(0, D3D10_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE )*/   UINT *NumViewports,
            /* [annotation] */ 
            __out_ecount_opt(*NumViewports)  D3D10Base::D3D10_VIEWPORT *pViewports)
{
	LogInfo("ID3D10Device::RSGetViewports called\n");
	
	m_pDevice->RSGetViewports(NumViewports, pViewports);
	if (LogFile)
	{
		if (pViewports)
		{
			for (UINT i = 0; i < *NumViewports; ++i)
			{
				LogInfo("  viewport #%d: TopLeft=(%d,%d), Width=%d, Height=%d, MinDepth=%f, MaxDepth=%f\n", i,
					pViewports[i].TopLeftX, pViewports[i].TopLeftY, pViewports[i].Width,
					pViewports[i].Height, pViewports[i].MinDepth, pViewports[i].MaxDepth);
			}
		}
		LogInfo("  returns NumViewports = %d\n", *NumViewports);
	}
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::RSGetScissorRects(THIS_
            /* [annotation] */ 
            __inout /*_range(0, D3D10_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE )*/   UINT *NumRects,
            /* [annotation] */ 
            __out_ecount_opt(*NumRects)  D3D10Base::D3D10_RECT *pRects)
{
	LogInfo("ID3D10Device::RSGetScissorRects called\n");
	
	m_pDevice->RSGetScissorRects(NumRects, pRects);
}
        
STDMETHODIMP D3D10Wrapper::ID3D10Device::GetDeviceRemovedReason(THIS)
{
	LogInfo("ID3D10Device::GetDeviceRemovedReason called\n");
	
	HRESULT hr = m_pDevice->GetDeviceRemovedReason();
	LogInfo("  returns result = %x\n", hr);
	
	return hr;
}
        
STDMETHODIMP D3D10Wrapper::ID3D10Device::SetExceptionMode(THIS_
            UINT RaiseFlags)
{
	LogInfo("ID3D10Device::SetExceptionMode called with RaiseFlags=%x\n", RaiseFlags);
	
	return m_pDevice->SetExceptionMode(RaiseFlags);
}
        
STDMETHODIMP_(UINT) D3D10Wrapper::ID3D10Device::GetExceptionMode(THIS)
{
	LogInfo("ID3D10Device::GetExceptionMode called\n");
	
	return m_pDevice->GetExceptionMode();
}
                
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::ClearState(THIS)
{
	LogInfo("ID3D10Device::ClearState called\n");
	
	m_pDevice->ClearState();
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::Flush(THIS)
{
	LogInfo("ID3D10Device::Flush called\n");
	
	m_pDevice->Flush();
}
        
STDMETHODIMP D3D10Wrapper::ID3D10Device::CreateBuffer(THIS_
            /* [annotation] */ 
            __in  const D3D10Base::D3D10_BUFFER_DESC *pDesc,
            /* [annotation] */ 
            __in_opt  const D3D10Base::D3D10_SUBRESOURCE_DATA *pInitialData,
            /* [annotation] */ 
            __out_opt  D3D10Base::ID3D10Buffer **ppBuffer)
{
	LogInfo("ID3D10Device::CreateBuffer called\n");
	
	return m_pDevice->CreateBuffer(pDesc, pInitialData, ppBuffer);
}
        
STDMETHODIMP D3D10Wrapper::ID3D10Device::CreateTexture1D(THIS_
            /* [annotation] */ 
            __in  const D3D10Base::D3D10_TEXTURE1D_DESC *pDesc,
            /* [annotation] */ 
            __in_xcount_opt(pDesc->MipLevels * pDesc->ArraySize)  const D3D10Base::D3D10_SUBRESOURCE_DATA *pInitialData,
            /* [annotation] */ 
            __out  D3D10Base::ID3D10Texture1D **ppTexture1D)
{
	LogInfo("ID3D10Device::CreateTexture1D called with\n");
	if (pDesc) LogInfo("  Width = %d\n", pDesc->Width);
	if (pDesc) LogInfo("  MipLevels = %d, ArraySize = %d\n", pDesc->MipLevels, pDesc->ArraySize);
	if (pDesc) LogInfo("  Format = %x\n", pDesc->Format);
	
	return m_pDevice->CreateTexture1D(pDesc, pInitialData, ppTexture1D);
}
        
STDMETHODIMP D3D10Wrapper::ID3D10Device::CreateTexture2D(THIS_
            /* [annotation] */ 
            __in  const D3D10Base::D3D10_TEXTURE2D_DESC *pDesc,
            /* [annotation] */ 
            __in_xcount_opt(pDesc->MipLevels * pDesc->ArraySize)  const D3D10Base::D3D10_SUBRESOURCE_DATA *pInitialData,
            /* [annotation] */ 
            __out  D3D10Base::ID3D10Texture2D **ppTexture2D)
{
	LogInfo("ID3D10Device::CreateTexture2D called with\n");
	LogInfo("  Width = %d, Height = %d\n", pDesc->Width, pDesc->Height);
	LogInfo("  MipLevels = %d, ArraySize = %d\n", pDesc->MipLevels, pDesc->ArraySize);
	LogInfo("  Format = %x, SampleDesc.Count = %u, SampleDesc.Quality = %u\n",
			pDesc->Format, pDesc->SampleDesc.Count, pDesc->SampleDesc.Quality);
	
	return m_pDevice->CreateTexture2D(pDesc, pInitialData, ppTexture2D);
}
        
STDMETHODIMP D3D10Wrapper::ID3D10Device::CreateTexture3D(THIS_
            /* [annotation] */ 
            __in  const D3D10Base::D3D10_TEXTURE3D_DESC *pDesc,
            /* [annotation] */ 
            __in_xcount_opt(pDesc->MipLevels)  const D3D10Base::D3D10_SUBRESOURCE_DATA *pInitialData,
            /* [annotation] */ 
            __out  D3D10Base::ID3D10Texture3D **ppTexture3D)
{
	LogInfo("ID3D10Device::CreateTexture3D called\n");
	
	return m_pDevice->CreateTexture3D(pDesc, pInitialData, ppTexture3D);
}
        
STDMETHODIMP D3D10Wrapper::ID3D10Device::CreateShaderResourceView(THIS_
            /* [annotation] */ 
            __in  D3D10Base::ID3D10Resource *pResource,
            /* [annotation] */ 
            __in_opt  const D3D10Base::D3D10_SHADER_RESOURCE_VIEW_DESC *pDesc,
            /* [annotation] */ 
            __out_opt  D3D10Base::ID3D10ShaderResourceView **ppSRView)
{
	//LogInfo("ID3D10Device::CreateShaderResourceView called\n");
	//
	return m_pDevice->CreateShaderResourceView(pResource, pDesc, ppSRView);
}
        
STDMETHODIMP D3D10Wrapper::ID3D10Device::CreateRenderTargetView(THIS_
            /* [annotation] */ 
            __in  D3D10Base::ID3D10Resource *pResource,
            /* [annotation] */ 
            __in_opt  const D3D10Base::D3D10_RENDER_TARGET_VIEW_DESC *pDesc,
            /* [annotation] */ 
            __out_opt  D3D10Base::ID3D10RenderTargetView **ppRTView)
{
	LogInfo("ID3D10Device::CreateRenderTargetView called\n");
	
	return m_pDevice->CreateRenderTargetView(pResource, pDesc, ppRTView);
}
        
STDMETHODIMP D3D10Wrapper::ID3D10Device::CreateDepthStencilView(THIS_
            /* [annotation] */ 
            __in  D3D10Base::ID3D10Resource *pResource,
            /* [annotation] */ 
            __in_opt  const D3D10Base::D3D10_DEPTH_STENCIL_VIEW_DESC *pDesc,
            /* [annotation] */ 
            __out_opt  D3D10Base::ID3D10DepthStencilView **ppDepthStencilView)
{
	LogInfo("ID3D10Device::CreateDepthStencilView called\n");
	
	return m_pDevice->CreateDepthStencilView(pResource, pDesc, ppDepthStencilView);
}
        
STDMETHODIMP D3D10Wrapper::ID3D10Device::CreateInputLayout(THIS_
            /* [annotation] */ 
            __in_ecount(NumElements)  const D3D10Base::D3D10_INPUT_ELEMENT_DESC *pInputElementDescs,
            /* [annotation] */ 
            __in_range( 0, D3D10_1_IA_VERTEX_INPUT_STRUCTURE_ELEMENT_COUNT )  UINT NumElements,
            /* [annotation] */ 
            __in  const void *pShaderBytecodeWithInputSignature,
            /* [annotation] */ 
            __in  SIZE_T BytecodeLength,
            /* [annotation] */ 
            __out_opt  D3D10Base::ID3D10InputLayout **ppInputLayout)
{
	LogInfo("ID3D10Device::CreateInputLayout called\n");
	
	return m_pDevice->CreateInputLayout(pInputElementDescs, NumElements, pShaderBytecodeWithInputSignature,
		BytecodeLength, ppInputLayout);
}
        
STDMETHODIMP D3D10Wrapper::ID3D10Device::CreateVertexShader(THIS_
            /* [annotation] */ 
            __in  const void *pShaderBytecode,
            /* [annotation] */ 
            __in  SIZE_T BytecodeLength,
            /* [annotation] */ 
            __out_opt  D3D10Base::ID3D10VertexShader **ppVertexShader)
{
	LogInfo("ID3D11Device::CreateVertexShader called.\n");
	
	return m_pDevice->CreateVertexShader(pShaderBytecode, BytecodeLength, ppVertexShader);
}
        
STDMETHODIMP D3D10Wrapper::ID3D10Device::CreateGeometryShader(THIS_
            /* [annotation] */ 
            __in  const void *pShaderBytecode,
            /* [annotation] */ 
            __in  SIZE_T BytecodeLength,
            /* [annotation] */ 
            __out_opt  D3D10Base::ID3D10GeometryShader **ppGeometryShader)
{
	LogInfo("ID3D11Device::CreateGeometryShader called.\n");
	
	return m_pDevice->CreateGeometryShader(pShaderBytecode, BytecodeLength, ppGeometryShader);
}
        
STDMETHODIMP D3D10Wrapper::ID3D10Device::CreateGeometryShaderWithStreamOutput(THIS_
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
            __out_opt  D3D10Base::ID3D10GeometryShader **ppGeometryShader)
{
	LogInfo("ID3D10Device::CreateGeometryShaderWithStreamOutput called\n");
	
	return m_pDevice->CreateGeometryShaderWithStreamOutput(pShaderBytecode, BytecodeLength, pSODeclaration,
		NumEntries, OutputStreamStride, ppGeometryShader);
}
        
STDMETHODIMP D3D10Wrapper::ID3D10Device::CreatePixelShader(THIS_
            /* [annotation] */ 
            __in  const void *pShaderBytecode,
            /* [annotation] */ 
            __in  SIZE_T BytecodeLength,
            /* [annotation] */ 
            __out_opt  D3D10Base::ID3D10PixelShader **ppPixelShader)
{
	LogInfo("ID3D11Device::CreatePixelShader called.\n");
	
	return m_pDevice->CreatePixelShader(pShaderBytecode, BytecodeLength, ppPixelShader);
}
        
STDMETHODIMP D3D10Wrapper::ID3D10Device::CreateBlendState(THIS_
            /* [annotation] */ 
            __in  const D3D10Base::D3D10_BLEND_DESC *pBlendStateDesc,
            /* [annotation] */ 
            __out_opt  D3D10Base::ID3D10BlendState **ppBlendState)
{
	LogInfo("ID3D10Device::CreateBlendState called\n");
	
	return m_pDevice->CreateBlendState(pBlendStateDesc, ppBlendState);
}
        
STDMETHODIMP D3D10Wrapper::ID3D10Device::CreateDepthStencilState(THIS_
            /* [annotation] */ 
            __in  const D3D10Base::D3D10_DEPTH_STENCIL_DESC *pDepthStencilDesc,
            /* [annotation] */ 
            __out_opt  D3D10Base::ID3D10DepthStencilState **ppDepthStencilState)
{
	LogInfo("ID3D10Device::CreateDepthStencilState called\n");
	
	return m_pDevice->CreateDepthStencilState(pDepthStencilDesc, ppDepthStencilState);
}
        
STDMETHODIMP D3D10Wrapper::ID3D10Device::CreateRasterizerState(THIS_
            /* [annotation] */ 
            __in  const D3D10Base::D3D10_RASTERIZER_DESC *pRasterizerDesc,
            /* [annotation] */ 
            __out_opt  D3D10Base::ID3D10RasterizerState **ppRasterizerState)
{
	LogInfo("ID3D10Device::CreateRasterizerState called\n");
	
	return m_pDevice->CreateRasterizerState(pRasterizerDesc, ppRasterizerState);
}
        
STDMETHODIMP D3D10Wrapper::ID3D10Device::CreateSamplerState(THIS_
            /* [annotation] */ 
            __in  const D3D10Base::D3D10_SAMPLER_DESC *pSamplerDesc,
            /* [annotation] */ 
            __out_opt  D3D10Base::ID3D10SamplerState **ppSamplerState)
{
	LogInfo("ID3D10Device::CreateSamplerState called\n");
	
	return m_pDevice->CreateSamplerState(pSamplerDesc, ppSamplerState);
}
        
STDMETHODIMP D3D10Wrapper::ID3D10Device::CreateQuery(THIS_
            /* [annotation] */ 
            __in  const D3D10Base::D3D10_QUERY_DESC *pQueryDesc,
            /* [annotation] */ 
            __out_opt  D3D10Base::ID3D10Query **ppQuery)
{
	if (LogFile) 
	{
		LogInfo("ID3D10Device::CreateQuery called with parameters\n");
		switch (pQueryDesc->Query)
		{
			case D3D10Base::D3D10_QUERY_EVENT: LogInfo("  query = Event\n"); break;
			case D3D10Base::D3D10_QUERY_OCCLUSION: LogInfo("  query = Occlusion\n"); break;
			case D3D10Base::D3D10_QUERY_TIMESTAMP: LogInfo("  query = Timestamp\n"); break;
			case D3D10Base::D3D10_QUERY_TIMESTAMP_DISJOINT: LogInfo("  query = Timestamp disjoint\n"); break;
			case D3D10Base::D3D10_QUERY_PIPELINE_STATISTICS: LogInfo("  query = Pipeline statistics\n"); break;
			case D3D10Base::D3D10_QUERY_OCCLUSION_PREDICATE: LogInfo("  query = Occlusion predicate\n"); break;
			case D3D10Base::D3D10_QUERY_SO_STATISTICS: LogInfo("  query = Streaming output statistics\n"); break;
			case D3D10Base::D3D10_QUERY_SO_OVERFLOW_PREDICATE: LogInfo("  query = Streaming output overflow predicate\n"); break;
			default: LogInfo("  query = unknown/invalid\n"); break;
		}
		LogInfo("  Flags = %x\n", pQueryDesc->MiscFlags);
	}
	HRESULT ret = m_pDevice->CreateQuery(pQueryDesc, ppQuery);
	LogInfo("  returned result = %x\n", ret);
	
	return ret;
}
        
STDMETHODIMP D3D10Wrapper::ID3D10Device::CreatePredicate(THIS_
            /* [annotation] */ 
            __in  const D3D10Base::D3D10_QUERY_DESC *pPredicateDesc,
            /* [annotation] */ 
            __out_opt  D3D10Base::ID3D10Predicate **ppPredicate)
{
	LogInfo("ID3D10Device::CreatePredicate called\n");
	
	return m_pDevice->CreatePredicate(pPredicateDesc, ppPredicate);
}
        
STDMETHODIMP D3D10Wrapper::ID3D10Device::CreateCounter(THIS_
            /* [annotation] */ 
            __in  const D3D10Base::D3D10_COUNTER_DESC *pCounterDesc,
            /* [annotation] */ 
            __out_opt  D3D10Base::ID3D10Counter **ppCounter)
{
	LogInfo("ID3D10Device::CreateCounter called\n");
	
	return m_pDevice->CreateCounter(pCounterDesc, ppCounter);
}
        
STDMETHODIMP D3D10Wrapper::ID3D10Device::CheckFormatSupport(THIS_
            /* [annotation] */ 
            __in  D3D10Base::DXGI_FORMAT Format,
            /* [annotation] */ 
            __out  UINT *pFormatSupport)
{
	LogInfo("ID3D10Device::CheckFormatSupport called\n");
	
	return m_pDevice->CheckFormatSupport(Format, pFormatSupport);
}
        
STDMETHODIMP D3D10Wrapper::ID3D10Device::CheckMultisampleQualityLevels(THIS_
            /* [annotation] */ 
            __in  D3D10Base::DXGI_FORMAT Format,
            /* [annotation] */ 
            __in  UINT SampleCount,
            /* [annotation] */ 
            __out  UINT *pNumQualityLevels)
{
	LogInfo("ID3D10Device::CheckMultisampleQualityLevels called with Format = %d, SampleCount = %d\n", Format, SampleCount);
	
	HRESULT hr = m_pDevice->CheckMultisampleQualityLevels(Format, SampleCount, pNumQualityLevels);
	LogInfo("  returns result = %x, NumQualityLevels = %d\n", hr, *pNumQualityLevels);
	
	return hr;
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::CheckCounterInfo(THIS_
            /* [annotation] */ 
            __out  D3D10Base::D3D10_COUNTER_INFO *pCounterInfo)
{
	LogInfo("ID3D10Device::CheckCounterInfo called\n");
	
	m_pDevice->CheckCounterInfo(pCounterInfo);
}
        
STDMETHODIMP D3D10Wrapper::ID3D10Device::CheckCounter(THIS_
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
            __inout_opt  UINT *pDescriptionLength)
{
	LogInfo("ID3D10Device::CheckCounter called\n");
	
	return m_pDevice->CheckCounter(pDesc, pType, pActiveCounters, szName, pNameLength, szUnits, pUnitsLength,
		szDescription, pDescriptionLength);
}
        
STDMETHODIMP_(UINT) D3D10Wrapper::ID3D10Device::GetCreationFlags(THIS)
{
	LogInfo("ID3D10Device::GetCreationFlags called\n");
	
	UINT ret = m_pDevice->GetCreationFlags();
	LogInfo("  returns Flags = %x\n", ret);
	
	return ret;
}
        
STDMETHODIMP D3D10Wrapper::ID3D10Device::OpenSharedResource(THIS_
            /* [annotation] */ 
            __in  HANDLE hResource,
            /* [annotation] */ 
            __in  REFIID ReturnedInterface,
            /* [annotation] */ 
            __out_opt  void **ppResource)
{
	LogInfo("ID3D10Device::OpenSharedResource called\n");
	
	return m_pDevice->OpenSharedResource(hResource, ReturnedInterface, ppResource);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::SetTextFilterSize(THIS_
            /* [annotation] */ 
            __in  UINT Width,
            /* [annotation] */ 
            __in  UINT Height)
{
	LogInfo("ID3D10Device::SetTextFilterSize called\n");
	
	m_pDevice->SetTextFilterSize(Width, Height);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::GetTextFilterSize(THIS_
            /* [annotation] */ 
            __out_opt  UINT *pWidth,
            /* [annotation] */ 
            __out_opt  UINT *pHeight)
{
	LogInfo("ID3D10Device::GetTextFilterSize called\n");
	
	m_pDevice->GetTextFilterSize(pWidth, pHeight);
}

D3D10Wrapper::ID3D10Multithread::ID3D10Multithread(D3D10Base::ID3D10Multithread *pDevice)
    : D3D10Wrapper::IDirect3DUnknown((IUnknown*) pDevice)
{
    m_pDevice = pDevice;
}

D3D10Wrapper::ID3D10Multithread* D3D10Wrapper::ID3D10Multithread::GetDirect3DMultithread(D3D10Base::ID3D10Multithread *pOrig)
{
    D3D10Wrapper::ID3D10Multithread* p = (D3D10Wrapper::ID3D10Multithread*) m_List.GetDataPtr(pOrig);
    if (!p)
    {
        p = new D3D10Wrapper::ID3D10Multithread(pOrig);
        if (pOrig) m_List.AddMember(pOrig,p);
    }
    return p;
}

STDMETHODIMP_(ULONG) D3D10Wrapper::ID3D10Multithread::AddRef(THIS)
{
	m_pUnk->AddRef();
    return ++m_ulRef;
}

STDMETHODIMP_(ULONG) D3D10Wrapper::ID3D10Multithread::Release(THIS)
{
	LogInfo("ID3D10Multithread::Release handle=%p, counter=%d\n", m_pUnk, m_ulRef);
	
    m_pUnk->Release();

    ULONG ulRef = --m_ulRef;

    if(ulRef <= 0)
    {
		LogInfo("  deleting self\n");
		
        if (m_pUnk) m_List.DeleteMember(m_pUnk); m_pUnk = 0;
        delete this;
        return 0L;
    }
    return ulRef;
}

STDMETHODIMP_(void) D3D10Wrapper::ID3D10Multithread::Enter(THIS)
{
	LogInfo("ID3D10Multithread::Enter called\n");
	
	m_pDevice->Enter(); 
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Multithread::Leave(THIS)
{
	LogInfo("ID3D10Multithread::Leave called\n");
	
	m_pDevice->Leave(); 
}
        
STDMETHODIMP_(BOOL) D3D10Wrapper::ID3D10Multithread::SetMultithreadProtected(THIS_
            /* [annotation] */ 
            __in  BOOL bMTProtect)
{
	LogInfo("ID3D10Multithread::SetMultithreadProtected called with bMTProtect = %d\n", bMTProtect);
	
	BOOL ret = m_pDevice->SetMultithreadProtected(bMTProtect); 
	LogInfo("  returns %d\n", ret);
	
	return ret;
}
        
STDMETHODIMP_(BOOL) D3D10Wrapper::ID3D10Multithread::GetMultithreadProtected(THIS)
{
	LogInfo("ID3D10Multithread::GetMultithreadProtected called\n");
	
	BOOL ret = m_pDevice->GetMultithreadProtected(); 
	LogInfo("  returns %d\n", ret);
	
	return ret;
}
