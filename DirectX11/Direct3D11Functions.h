
D3D11Wrapper::ID3D11Device::ID3D11Device(D3D11Base::ID3D11Device *pDevice)
    : D3D11Wrapper::IDirect3DUnknown((IUnknown*) pDevice)
{
    m_pDevice = pDevice;
}

D3D11Wrapper::ID3D11Device* D3D11Wrapper::ID3D11Device::GetDirect3DDevice(D3D11Base::ID3D11Device *pOrig)
{
    D3D11Wrapper::ID3D11Device* p = (D3D11Wrapper::ID3D11Device*) m_List.GetDataPtr(pOrig);
    if( p == NULL )
    {
        p = new D3D11Wrapper::ID3D11Device(pOrig);
        m_List.AddMember(pOrig,p);
        return p;
    }
    
    p->m_ulRef++;
    return p;
}

STDMETHODIMP_(ULONG) D3D11Wrapper::ID3D11Device::AddRef(THIS)
{
	m_pUnk->AddRef();
    return ++m_ulRef;
}

STDMETHODIMP_(ULONG) D3D11Wrapper::ID3D11Device::Release(THIS)
{
	if (LogFile) fprintf(LogFile, "Release handle=%x, counter=%d\n", m_pUnk, m_ulRef);
	//if (LogFile) fprintf(LogFile, "  ignoring call\n");
	if (LogFile) fflush(LogFile);
    m_pUnk->Release();

    ULONG ulRef = --m_ulRef;

    if(ulRef <= 0)
    {
		if (LogFile) fprintf(LogFile, "  deleting self\n");
		if (LogFile) fflush(LogFile);
        m_List.DeleteMember(GetD3D11Device());
        delete this;
        return 0L;
    }
    return ulRef;
}

STDMETHODIMP D3D11Wrapper::ID3D11Device::CreateBuffer(THIS_
            /* [annotation] */ 
            __in  const D3D11Base::D3D11_BUFFER_DESC *pDesc,
            /* [annotation] */ 
            __in_opt  const D3D11Base::D3D11_SUBRESOURCE_DATA *pInitialData,
            /* [annotation] */ 
            __out_opt  D3D11Base::ID3D11Buffer **ppBuffer)
{
	return m_pDevice->CreateBuffer(pDesc, pInitialData, ppBuffer);
}

STDMETHODIMP D3D11Wrapper::ID3D11Device::CreateTexture1D(THIS_
            /* [annotation] */ 
            __in  const D3D11Base::D3D11_TEXTURE1D_DESC *pDesc,
            /* [annotation] */ 
            __in_xcount_opt(pDesc->MipLevels * pDesc->ArraySize)  const D3D11Base::D3D11_SUBRESOURCE_DATA *pInitialData,
            /* [annotation] */ 
            __out_opt  D3D11Base::ID3D11Texture1D **ppTexture1D)
{
	return m_pDevice->CreateTexture1D(pDesc, pInitialData, ppTexture1D);
}

STDMETHODIMP D3D11Wrapper::ID3D11Device::CreateTexture2D(THIS_
            /* [annotation] */ 
            __in  const D3D11Base::D3D11_TEXTURE2D_DESC *pDesc,
            /* [annotation] */ 
            __in_xcount_opt(pDesc->MipLevels * pDesc->ArraySize)  const D3D11Base::D3D11_SUBRESOURCE_DATA *pInitialData,
            /* [annotation] */ 
            __out_opt  D3D11Base::ID3D11Texture2D **ppTexture2D)
{
	return m_pDevice->CreateTexture2D(pDesc, pInitialData, ppTexture2D);
}

STDMETHODIMP D3D11Wrapper::ID3D11Device::CreateTexture3D(THIS_
            /* [annotation] */ 
            __in  const D3D11Base::D3D11_TEXTURE3D_DESC *pDesc,
            /* [annotation] */ 
            __in_xcount_opt(pDesc->MipLevels)  const D3D11Base::D3D11_SUBRESOURCE_DATA *pInitialData,
            /* [annotation] */ 
            __out_opt  D3D11Base::ID3D11Texture3D **ppTexture3D)
{
	return m_pDevice->CreateTexture3D(pDesc, pInitialData, ppTexture3D);
}

STDMETHODIMP D3D11Wrapper::ID3D11Device::CreateShaderResourceView(THIS_
            /* [annotation] */ 
            __in  D3D11Base::ID3D11Resource *pResource,
            /* [annotation] */ 
            __in_opt  const D3D11Base::D3D11_SHADER_RESOURCE_VIEW_DESC *pDesc,
            /* [annotation] */ 
            __out_opt  D3D11Base::ID3D11ShaderResourceView **ppSRView)
{
	return m_pDevice->CreateShaderResourceView(pResource, pDesc, ppSRView);
}

STDMETHODIMP D3D11Wrapper::ID3D11Device::CreateUnorderedAccessView(THIS_
            /* [annotation] */ 
            __in  D3D11Base::ID3D11Resource *pResource,
            /* [annotation] */ 
            __in_opt  const D3D11Base::D3D11_UNORDERED_ACCESS_VIEW_DESC *pDesc,
            /* [annotation] */ 
            __out_opt  D3D11Base::ID3D11UnorderedAccessView **ppUAView)
{
	return m_pDevice->CreateUnorderedAccessView(pResource, pDesc, ppUAView);
}

STDMETHODIMP D3D11Wrapper::ID3D11Device::CreateRenderTargetView(THIS_
            /* [annotation] */ 
            __in  D3D11Base::ID3D11Resource *pResource,
            /* [annotation] */ 
            __in_opt  const D3D11Base::D3D11_RENDER_TARGET_VIEW_DESC *pDesc,
            /* [annotation] */ 
            __out_opt  D3D11Base::ID3D11RenderTargetView **ppRTView)
{
	return m_pDevice->CreateRenderTargetView(pResource, pDesc, ppRTView);
}

STDMETHODIMP D3D11Wrapper::ID3D11Device::CreateDepthStencilView(THIS_
            /* [annotation] */ 
            __in  D3D11Base::ID3D11Resource *pResource,
            /* [annotation] */ 
            __in_opt  const D3D11Base::D3D11_DEPTH_STENCIL_VIEW_DESC *pDesc,
            /* [annotation] */ 
            __out_opt  D3D11Base::ID3D11DepthStencilView **ppDepthStencilView)
{
	return m_pDevice->CreateDepthStencilView(pResource, pDesc, ppDepthStencilView);
}

STDMETHODIMP D3D11Wrapper::ID3D11Device::CreateInputLayout(THIS_
            /* [annotation] */ 
            __in_ecount(NumElements)  const D3D11Base::D3D11_INPUT_ELEMENT_DESC *pInputElementDescs,
            /* [annotation] */ 
            __in_range( 0, D3D11_IA_VERTEX_INPUT_STRUCTURE_ELEMENT_COUNT )  UINT NumElements,
            /* [annotation] */ 
            __in  const void *pShaderBytecodeWithInputSignature,
            /* [annotation] */ 
            __in  SIZE_T BytecodeLength,
            /* [annotation] */ 
            __out_opt  D3D11Base::ID3D11InputLayout **ppInputLayout)
{
	return m_pDevice->CreateInputLayout(pInputElementDescs, NumElements, pShaderBytecodeWithInputSignature,
		BytecodeLength, ppInputLayout);
}

STDMETHODIMP D3D11Wrapper::ID3D11Device::CreateVertexShader(THIS_
            /* [annotation] */ 
            __in  const void *pShaderBytecode,
            /* [annotation] */ 
            __in  SIZE_T BytecodeLength,
            /* [annotation] */ 
            __in_opt  D3D11Base::ID3D11ClassLinkage *pClassLinkage,
            /* [annotation] */ 
            __out_opt  D3D11Base::ID3D11VertexShader **ppVertexShader)
{
	return m_pDevice->CreateVertexShader(pShaderBytecode, BytecodeLength, pClassLinkage, ppVertexShader);
}

STDMETHODIMP D3D11Wrapper::ID3D11Device::CreateGeometryShader(THIS_
            /* [annotation] */ 
            __in  const void *pShaderBytecode,
            /* [annotation] */ 
            __in  SIZE_T BytecodeLength,
            /* [annotation] */ 
            __in_opt  D3D11Base::ID3D11ClassLinkage *pClassLinkage,
            /* [annotation] */ 
            __out_opt  D3D11Base::ID3D11GeometryShader **ppGeometryShader)
{
	return m_pDevice->CreateGeometryShader(pShaderBytecode, BytecodeLength, pClassLinkage, ppGeometryShader);
}

STDMETHODIMP D3D11Wrapper::ID3D11Device::CreateGeometryShaderWithStreamOutput(THIS_
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
            __out_opt  D3D11Base::ID3D11GeometryShader **ppGeometryShader)
{
	return m_pDevice->CreateGeometryShaderWithStreamOutput(pShaderBytecode, BytecodeLength, pSODeclaration,
		NumEntries, pBufferStrides, NumStrides, RasterizedStream, pClassLinkage, ppGeometryShader);
}

STDMETHODIMP D3D11Wrapper::ID3D11Device::CreatePixelShader(THIS_
            /* [annotation] */ 
            __in  const void *pShaderBytecode,
            /* [annotation] */ 
            __in  SIZE_T BytecodeLength,
            /* [annotation] */ 
            __in_opt  D3D11Base::ID3D11ClassLinkage *pClassLinkage,
            /* [annotation] */ 
            __out_opt  D3D11Base::ID3D11PixelShader **ppPixelShader)
{
	return m_pDevice->CreatePixelShader(pShaderBytecode, BytecodeLength, pClassLinkage, ppPixelShader);
}

STDMETHODIMP D3D11Wrapper::ID3D11Device::CreateHullShader(THIS_
            /* [annotation] */ 
            __in  const void *pShaderBytecode,
            /* [annotation] */ 
            __in  SIZE_T BytecodeLength,
            /* [annotation] */ 
            __in_opt  D3D11Base::ID3D11ClassLinkage *pClassLinkage,
            /* [annotation] */ 
            __out_opt  D3D11Base::ID3D11HullShader **ppHullShader)
{
	return m_pDevice->CreateHullShader(pShaderBytecode, BytecodeLength, pClassLinkage, ppHullShader);
}

STDMETHODIMP D3D11Wrapper::ID3D11Device::CreateDomainShader(THIS_
            /* [annotation] */ 
            __in  const void *pShaderBytecode,
            /* [annotation] */ 
            __in  SIZE_T BytecodeLength,
            /* [annotation] */ 
            __in_opt  D3D11Base::ID3D11ClassLinkage *pClassLinkage,
            /* [annotation] */ 
            __out_opt  D3D11Base::ID3D11DomainShader **ppDomainShader)
{
	return m_pDevice->CreateDomainShader(pShaderBytecode, BytecodeLength, pClassLinkage, ppDomainShader);
}

STDMETHODIMP D3D11Wrapper::ID3D11Device::CreateComputeShader(THIS_
            /* [annotation] */ 
            __in  const void *pShaderBytecode,
            /* [annotation] */ 
            __in  SIZE_T BytecodeLength,
            /* [annotation] */ 
            __in_opt  D3D11Base::ID3D11ClassLinkage *pClassLinkage,
            /* [annotation] */ 
            __out_opt  D3D11Base::ID3D11ComputeShader **ppComputeShader)
{
	return m_pDevice->CreateComputeShader(pShaderBytecode, BytecodeLength, pClassLinkage, ppComputeShader);
}

STDMETHODIMP D3D11Wrapper::ID3D11Device::CreateClassLinkage(THIS_
            /* [annotation] */ 
            __out  D3D11Base::ID3D11ClassLinkage **ppLinkage)
{
	return m_pDevice->CreateClassLinkage(ppLinkage);
}

STDMETHODIMP D3D11Wrapper::ID3D11Device::CreateBlendState(THIS_
            /* [annotation] */ 
            __in  const D3D11Base::D3D11_BLEND_DESC *pBlendStateDesc,
            /* [annotation] */ 
            __out_opt  D3D11Base::ID3D11BlendState **ppBlendState)
{
	return m_pDevice->CreateBlendState(pBlendStateDesc, ppBlendState);
}

STDMETHODIMP D3D11Wrapper::ID3D11Device::CreateDepthStencilState(THIS_
            /* [annotation] */ 
            __in  const D3D11Base::D3D11_DEPTH_STENCIL_DESC *pDepthStencilDesc,
            /* [annotation] */ 
            __out_opt  D3D11Base::ID3D11DepthStencilState **ppDepthStencilState)
{
	return m_pDevice->CreateDepthStencilState(pDepthStencilDesc, ppDepthStencilState);
}

STDMETHODIMP D3D11Wrapper::ID3D11Device::CreateRasterizerState(THIS_
            /* [annotation] */ 
            __in  const D3D11Base::D3D11_RASTERIZER_DESC *pRasterizerDesc,
            /* [annotation] */ 
            __out_opt  D3D11Base::ID3D11RasterizerState **ppRasterizerState)
{
	return m_pDevice->CreateRasterizerState(pRasterizerDesc, ppRasterizerState);
}

STDMETHODIMP D3D11Wrapper::ID3D11Device::CreateSamplerState(THIS_
            /* [annotation] */ 
            __in  const D3D11Base::D3D11_SAMPLER_DESC *pSamplerDesc,
            /* [annotation] */ 
            __out_opt  D3D11Base::ID3D11SamplerState **ppSamplerState)
{
	return m_pDevice->CreateSamplerState(pSamplerDesc, ppSamplerState);
}

STDMETHODIMP D3D11Wrapper::ID3D11Device::CreateQuery(THIS_
            /* [annotation] */ 
            __in  const D3D11Base::D3D11_QUERY_DESC *pQueryDesc,
            /* [annotation] */ 
            __out_opt  D3D11Base::ID3D11Query **ppQuery)
{
	return m_pDevice->CreateQuery(pQueryDesc, ppQuery);
}

STDMETHODIMP D3D11Wrapper::ID3D11Device::CreatePredicate(THIS_
            /* [annotation] */ 
            __in  const D3D11Base::D3D11_QUERY_DESC *pPredicateDesc,
            /* [annotation] */ 
            __out_opt  D3D11Base::ID3D11Predicate **ppPredicate)
{
	return m_pDevice->CreatePredicate(pPredicateDesc, ppPredicate);
}

STDMETHODIMP D3D11Wrapper::ID3D11Device::CreateCounter(THIS_
            /* [annotation] */ 
            __in  const D3D11Base::D3D11_COUNTER_DESC *pCounterDesc,
            /* [annotation] */ 
            __out_opt  D3D11Base::ID3D11Counter **ppCounter) 
{
	return m_pDevice->CreateCounter(pCounterDesc, ppCounter);
}

STDMETHODIMP D3D11Wrapper::ID3D11Device::CreateDeferredContext(THIS_
            UINT ContextFlags,
            /* [annotation] */ 
            __out_opt  D3D11Base::ID3D11DeviceContext **ppDeferredContext)
{
	return m_pDevice->CreateDeferredContext(ContextFlags, ppDeferredContext);
}

STDMETHODIMP D3D11Wrapper::ID3D11Device::OpenSharedResource(THIS_
            /* [annotation] */ 
            __in  HANDLE hResource,
            /* [annotation] */ 
            __in  REFIID ReturnedInterface,
            /* [annotation] */ 
            __out_opt  void **ppResource)
{
	return m_pDevice->OpenSharedResource(hResource, ReturnedInterface, ppResource);
}

STDMETHODIMP D3D11Wrapper::ID3D11Device::CheckFormatSupport(THIS_
            /* [annotation] */ 
            __in  D3D11Base::DXGI_FORMAT Format,
            /* [annotation] */ 
            __out  UINT *pFormatSupport)
{
	return m_pDevice->CheckFormatSupport(Format, pFormatSupport);
}

STDMETHODIMP D3D11Wrapper::ID3D11Device::CheckMultisampleQualityLevels(THIS_
            /* [annotation] */ 
            __in  D3D11Base::DXGI_FORMAT Format,
            /* [annotation] */ 
            __in  UINT SampleCount,
            /* [annotation] */ 
            __out  UINT *pNumQualityLevels) 
{
	return m_pDevice->CheckMultisampleQualityLevels(Format, SampleCount, pNumQualityLevels);
}

STDMETHODIMP_(void) D3D11Wrapper::ID3D11Device::CheckCounterInfo(THIS_
            /* [annotation] */ 
            __out  D3D11Base::D3D11_COUNTER_INFO *pCounterInfo)
{
	return m_pDevice->CheckCounterInfo(pCounterInfo);
}

STDMETHODIMP D3D11Wrapper::ID3D11Device::CheckCounter(THIS_
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
            __inout_opt  UINT *pDescriptionLength)
{
	return m_pDevice->CheckCounter(pDesc, pType, pActiveCounters, szName, pNameLength, szUnits,
		pUnitsLength, szDescription, pDescriptionLength);
}

STDMETHODIMP D3D11Wrapper::ID3D11Device::CheckFeatureSupport(THIS_
            D3D11Base::D3D11_FEATURE Feature,
            /* [annotation] */ 
            __out_bcount(FeatureSupportDataSize)  void *pFeatureSupportData,
            UINT FeatureSupportDataSize)
{
	return m_pDevice->CheckFeatureSupport(Feature, pFeatureSupportData, FeatureSupportDataSize);
}

STDMETHODIMP D3D11Wrapper::ID3D11Device::GetPrivateData(THIS_
            /* [annotation] */ 
            __in  REFGUID guid,
            /* [annotation] */ 
            __inout  UINT *pDataSize,
            /* [annotation] */ 
            __out_bcount_opt(*pDataSize)  void *pData)
{
	return m_pDevice->GetPrivateData(guid, pDataSize, pData);
}
        
STDMETHODIMP D3D11Wrapper::ID3D11Device::SetPrivateData(THIS_
            /* [annotation] */ 
            __in  REFGUID guid,
            /* [annotation] */ 
            __in  UINT DataSize,
            /* [annotation] */ 
            __in_bcount_opt(DataSize)  const void *pData)
{
	return m_pDevice->SetPrivateData(guid, DataSize, pData);
}
        
STDMETHODIMP D3D11Wrapper::ID3D11Device::SetPrivateDataInterface(THIS_
            /* [annotation] */ 
            __in  REFGUID guid,
            /* [annotation] */ 
            __in_opt  const IUnknown *pData)
{
	return m_pDevice->SetPrivateDataInterface(guid, pData);
}

STDMETHODIMP_(D3D11Base::D3D_FEATURE_LEVEL) D3D11Wrapper::ID3D11Device::GetFeatureLevel(THIS)
{
	return m_pDevice->GetFeatureLevel();
}
        
STDMETHODIMP_(UINT) D3D11Wrapper::ID3D11Device::GetCreationFlags(THIS)
{
	return m_pDevice->GetCreationFlags();
}
        
STDMETHODIMP D3D11Wrapper::ID3D11Device::GetDeviceRemovedReason(THIS)
{
	return m_pDevice->GetDeviceRemovedReason();
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11Device::GetImmediateContext(THIS_ 
            /* [annotation] */ 
            __out  D3D11Base::ID3D11DeviceContext **ppImmediateContext)
{
	m_pDevice->GetImmediateContext(ppImmediateContext);
}
        
STDMETHODIMP D3D11Wrapper::ID3D11Device::SetExceptionMode(THIS_ 
            UINT RaiseFlags)
{
	return m_pDevice->SetExceptionMode(RaiseFlags);
}
        
STDMETHODIMP_(UINT) D3D11Wrapper::ID3D11Device::GetExceptionMode(THIS)
{
	return m_pDevice->GetExceptionMode();
}

