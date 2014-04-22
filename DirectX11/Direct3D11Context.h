
D3D11Wrapper::ID3D11DeviceContext::ID3D11DeviceContext(D3D11Base::ID3D11DeviceContext *pContext)
    : D3D11Wrapper::IDirect3DUnknown((IUnknown*) pContext)
{
}

D3D11Wrapper::ID3D11DeviceContext* D3D11Wrapper::ID3D11DeviceContext::GetDirect3DDeviceContext(D3D11Base::ID3D11DeviceContext *pOrig)
{
    D3D11Wrapper::ID3D11DeviceContext* p = (D3D11Wrapper::ID3D11DeviceContext*) m_List.GetDataPtr(pOrig);
    if (!p)
    {
        p = new D3D11Wrapper::ID3D11DeviceContext(pOrig);
        if (pOrig) m_List.AddMember(pOrig,p);
    }
    return p;
}

STDMETHODIMP_(ULONG) D3D11Wrapper::ID3D11DeviceContext::AddRef(THIS)
{
	++m_ulRef;
	return m_pUnk->AddRef();
}

STDMETHODIMP_(ULONG) D3D11Wrapper::ID3D11DeviceContext::Release(THIS)
{
	if (LogFile) fprintf(LogFile, "ID3D11DeviceContext::Release handle=%x, counter=%d, this=%x\n", m_pUnk, m_ulRef, this);
	
    ULONG ulRef = m_pUnk ? m_pUnk->Release() : 0;
	if (LogFile) fprintf(LogFile, "  internal counter = %d\n", ulRef);
	
	--m_ulRef;

    if (ulRef <= 0)
    {
		if (LogFile) fprintf(LogFile, "  deleting self\n");
		
        if (m_pUnk) m_List.DeleteMember(m_pUnk); m_pUnk = 0;
        delete this;
    }
    return ulRef;
}

STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::GetDevice(THIS_
        /* [annotation] */ 
        __out  ID3D11Device **ppDevice)
{
	// Get old device pointer.
	D3D11Base::ID3D11Device *origDevice;
	GetD3D11DeviceContext()->GetDevice(&origDevice);

	// Map device to wrapper.
	D3D11Wrapper::ID3D11Device *wrapper = (D3D11Wrapper::ID3D11Device*) D3D11Wrapper::ID3D11Device::GetDirect3DDevice(origDevice);
	if (!wrapper)
	{
		if (LogFile) fprintf(LogFile, "ID3D11DeviceContext::GetDevice called");
		if (LogFile) fprintf(LogFile, "  can't find wrapper for parent device. Returning original device handle = %x\n", origDevice);
		
		*ppDevice = (ID3D11Device *)origDevice;
		return;
	}

	*ppDevice = wrapper;
}
        
STDMETHODIMP D3D11Wrapper::ID3D11DeviceContext::GetPrivateData(THIS_
        /* [annotation] */ 
        __in  REFGUID guid,
        /* [annotation] */ 
        __inout  UINT *pDataSize,
        /* [annotation] */ 
        __out_bcount_opt( *pDataSize )  void *pData)
{
	if (LogFile) fprintf(LogFile, "ID3D11DeviceContext::GetPrivateData called with GUID = %08lx-%04hx-%04hx-%02hhx%02hhx-%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx\n", 
		guid.Data1, guid.Data2, guid.Data3, guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3], 
		guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
	
	HRESULT hr = GetD3D11DeviceContext()->GetPrivateData(guid, pDataSize, pData);
	if (LogFile) fprintf(LogFile, "  returns result = %x, DataSize = %d\n", hr, *pDataSize);
	
	return hr;
}
        
STDMETHODIMP D3D11Wrapper::ID3D11DeviceContext::SetPrivateData(THIS_
        /* [annotation] */ 
        __in  REFGUID guid,
        /* [annotation] */ 
        __in  UINT DataSize,
        /* [annotation] */ 
        __in_bcount_opt( DataSize )  const void *pData)
{
	if (LogFile) fprintf(LogFile, "ID3D11DeviceContext::SetPrivateData called with GUID = %08lx-%04hx-%04hx-%02hhx%02hhx-%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx\n", 
		guid.Data1, guid.Data2, guid.Data3, guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3], 
		guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
	if (LogFile) fprintf(LogFile, "  DataSize = %d\n", DataSize);
	
	HRESULT hr = GetD3D11DeviceContext()->SetPrivateData(guid, DataSize, pData);
	if (LogFile) fprintf(LogFile, "  returns result = %x\n", hr);
	
	return hr;
}
        
STDMETHODIMP D3D11Wrapper::ID3D11DeviceContext::SetPrivateDataInterface(THIS_
        /* [annotation] */ 
        __in  REFGUID guid,
        /* [annotation] */ 
        __in_opt  const IUnknown *pData)
{
	if (LogFile) fprintf(LogFile, "ID3D11DeviceContext::SetPrivateDataInterface called with GUID=%08lx-%04hx-%04hx-%02hhx%02hhx-%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx\n", 
		guid.Data1, guid.Data2, guid.Data3, guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3], 
		guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
	
	HRESULT hr = GetD3D11DeviceContext()->SetPrivateDataInterface(guid, pData);
	if (LogFile) fprintf(LogFile, "  returns result = %x\n", hr);
	
	return hr;
}

STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::VSSetConstantBuffers(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
        /* [annotation] */ 
        __in_ecount(NumBuffers) D3D11Base::ID3D11Buffer *const *ppConstantBuffers)
{
	GetD3D11DeviceContext()->VSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::PSSetShaderResources(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
        /* [annotation] */ 
        __in_ecount(NumViews) D3D11Base::ID3D11ShaderResourceView *const *ppShaderResourceViews)
{
	if (LogFile && LogDebug) fprintf(LogFile, "ID3D11DeviceContext::PSSetShaderResources called with StartSlot = %d, NumViews = %d\n", StartSlot, NumViews);
	if (ppShaderResourceViews && NumViews && LogFile && LogDebug) fprintf(LogFile, "  ShaderResourceView[0] handle = %x\n", *ppShaderResourceViews);	

	GetD3D11DeviceContext()->PSSetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
	
	// Resolve resource from resource view.
	if (G->hunting && ppShaderResourceViews)
	{
		if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
		for (int i = 0; i < NumViews; ++i)
		{
			int pos = StartSlot + i;
			if (!ppShaderResourceViews[i])
			{
				G->mPixelShaderInfo[G->mCurrentPixelShader].ResourceRegisters[pos] = 0;
				continue;
			}
			D3D11Base::D3D11_SHADER_RESOURCE_VIEW_DESC desc;
			ppShaderResourceViews[i]->GetDesc(&desc);
			if (desc.ViewDimension == D3D11Base::D3D11_SRV_DIMENSION_TEXTURE2D ||
				desc.ViewDimension == D3D11Base::D3D11_SRV_DIMENSION_TEXTURE2DMS ||
				desc.ViewDimension == D3D11Base::D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY)
			{
				D3D11Base::ID3D11Resource *pResource = 0;
				ppShaderResourceViews[i]->GetResource(&pResource);
				if (pResource)
				{
					D3D11Base::ID3D11Texture2D *texture = (D3D11Base::ID3D11Texture2D *)pResource;
					D3D11Base::D3D11_TEXTURE2D_DESC texDesc;
					texture->GetDesc(&texDesc);
					pResource->Release();
					std::map<D3D11Base::ID3D11Texture2D *, UINT64>::iterator j = G->mTexture2D_ID.find(texture);
					UINT64 hash = 0;
					if (j == G->mTexture2D_ID.end())
					{
						// Create hash again.
						hash ^= texDesc.Width; hash *= FNV_64_PRIME;
						hash ^= texDesc.Height; hash *= FNV_64_PRIME;
						hash ^= texDesc.MipLevels; hash *= FNV_64_PRIME;
						hash ^= texDesc.ArraySize; hash *= FNV_64_PRIME;
						hash ^= texDesc.Format; hash *= FNV_64_PRIME;
						hash ^= texDesc.Usage; hash *= FNV_64_PRIME;
						hash ^= texDesc.BindFlags; hash *= FNV_64_PRIME;
						hash ^= texDesc.CPUAccessFlags; hash *= FNV_64_PRIME;
						hash ^= texDesc.MiscFlags;
					}
					else
					{
						hash = j->second;
					}
					G->mRenderTargets[texture] = hash;
					G->mPixelShaderInfo[G->mCurrentPixelShader].ResourceRegisters[pos] = texture;
				}
			}
			else if (desc.ViewDimension == D3D11Base::D3D11_SRV_DIMENSION_TEXTURE3D)
			{
				D3D11Base::ID3D11Resource *pResource = 0;
				ppShaderResourceViews[i]->GetResource(&pResource);
				if (pResource)
				{
					D3D11Base::ID3D11Texture3D *texture = (D3D11Base::ID3D11Texture3D *)pResource;
					D3D11Base::D3D11_TEXTURE3D_DESC texDesc;
					texture->GetDesc(&texDesc);
					pResource->Release();
					std::map<D3D11Base::ID3D11Texture3D *, UINT64>::iterator j = G->mTexture3D_ID.find(texture);
					UINT64 hash = 0;
					if (j == G->mTexture3D_ID.end())
					{
						// Create hash again.
						hash ^= texDesc.Width; hash *= FNV_64_PRIME;
						hash ^= texDesc.Height; hash *= FNV_64_PRIME;
						hash ^= texDesc.Depth; hash *= FNV_64_PRIME;
						hash ^= texDesc.MipLevels; hash *= FNV_64_PRIME;
						hash ^= texDesc.Format; hash *= FNV_64_PRIME;
						hash ^= texDesc.Usage; hash *= FNV_64_PRIME;
						hash ^= texDesc.BindFlags; hash *= FNV_64_PRIME;
						hash ^= texDesc.CPUAccessFlags; hash *= FNV_64_PRIME;
						hash ^= texDesc.MiscFlags;
					}
					else
					{
						hash = j->second;
					}
					G->mRenderTargets[texture] = hash;
					G->mPixelShaderInfo[G->mCurrentPixelShader].ResourceRegisters[pos] = texture;
				}
			}
		}
		if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
	}

	/*
	// Map nvidia texture slot.
	if (ppShaderResourceViews && NumViews && ppShaderResourceViews[0])
	{
		ID3D11Device *device = 0;
		GetDevice(&device);
		if (device && device->mStereoResourceView)
		{
			if (LogFile && LogDebug) fprintf(LogFile, "  adding NVidia stereo parameter texture to shader resources in slot 125.\n");
			
			m_pContext->PSSetShaderResources(125, 1, &device->mStereoResourceView);
		}
		else
		{
			if (LogFile) fprintf(LogFile, "  error querying device. Can't set NVidia stereo parameter texture.\n");
		}
		device->Release();
	}
	*/
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::PSSetShader(THIS_
        /* [annotation] */ 
        __in_opt D3D11Base::ID3D11PixelShader *pPixelShader,
        /* [annotation] */ 
        __in_ecount_opt(NumClassInstances) D3D11Base::ID3D11ClassInstance *const *ppClassInstances,
        UINT NumClassInstances)
{
	if (LogFile && LogDebug) fprintf(LogFile, "ID3D11DeviceContext::PSSetShader called with pixelshader handle = %x\n", pPixelShader);
	
	bool patchedShader = false;
	if (G->hunting && pPixelShader)
	{
		// Store as current pixel shader.
		if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
		PixelShaderMap::iterator i = G->mPixelShaders.find(pPixelShader);
		if (i != G->mPixelShaders.end())
		{
			G->mCurrentPixelShader = i->second;
			if (LogFile && LogDebug) fprintf(LogFile, "  pixel shader found: handle = %x, hash = %08lx%08lx\n", pPixelShader, (UINT32)(G->mCurrentPixelShader >> 32), (UINT32)G->mCurrentPixelShader);

			// Add to visited pixel shaders.
			G->mVisitedPixelShaders.insert(i->second);
			patchedShader = true;

			// second try to hide index buffer.
			// if (mCurrentIndexBuffer == mSelectedIndexBuffer)
			//	pIndexBuffer = 0;
		}
		else 
		{
			if (LogFile && LogDebug) fprintf(LogFile, "  pixel shader %x not found\n", pPixelShader);
		}

		// Replacement map.
		PixelShaderReplacementMap::iterator j = G->mOriginalPixelShaders.find(pPixelShader);
		if (G->mSelectedPixelShader == G->mCurrentPixelShader && j != G->mOriginalPixelShaders.end())
		{
			D3D11Base::ID3D11PixelShader *shader = j->second;
			if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
			GetD3D11DeviceContext()->PSSetShader(shader, ppClassInstances, NumClassInstances);
			return;
		}
		j = G->mZeroPixelShaders.find(pPixelShader);
		if (G->mSelectedPixelShader == G->mCurrentPixelShader && j != G->mZeroPixelShaders.end())
		{
			D3D11Base::ID3D11PixelShader *shader = j->second;
			if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
			GetD3D11DeviceContext()->PSSetShader(shader, ppClassInstances, NumClassInstances);
			return;
		}

		// If the shader has been live reloaded from ShaderFixes, use the new one
		ShaderReloadMap::iterator it = G->mReloadedShaders.find(pPixelShader);
		if (it != G->mReloadedShaders.end() && it->second.newShader != NULL)
		{
			if (LogFile && LogDebug) fprintf(LogFile, "  pixel shader replaced by: %x\n", it->second.newShader);

			// Todo: It might make sense to Release() the original shader, to recover memory on GPU
			D3D11Base::ID3D11PixelShader *shader = (D3D11Base::ID3D11PixelShader*) it->second.newShader;
			if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
			GetD3D11DeviceContext()->PSSetShader(shader, ppClassInstances, NumClassInstances);
			return;
		}
		if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
	}

	GetD3D11DeviceContext()->PSSetShader(pPixelShader, ppClassInstances, NumClassInstances);

	// When hunting is off, send stereo texture to all shaders, as any might need it.
	// Maybe a bit of a waste of GPU resource, but optimizes CPU use.
	if (!G->hunting || patchedShader)
	{
		D3D11Wrapper::ID3D11Device *device = 0;
		GetDevice(&device);
		if (device)
		{
			// Set NVidia stereo texture.
			if (device->mStereoResourceView)
			{
				if (LogFile && LogDebug) fprintf(LogFile, "  adding NVidia stereo parameter texture to shader resources in slot 125.\n");
				
				GetD3D11DeviceContext()->PSSetShaderResources(125, 1, &device->mStereoResourceView);
			}
			// Set custom depth texture.
			if (device->mZBufferResourceView)
			{
				if (LogFile && LogDebug) fprintf(LogFile, "  adding Z buffer to shader resources in slot 126.\n");
				
				GetD3D11DeviceContext()->PSSetShaderResources(126, 1, &device->mZBufferResourceView);
			}
			device->Release();
		}
		else
		{
			if (LogFile) fprintf(LogFile, "  error querying device. Can't set NVidia stereo parameter texture.\n");
		}
	}
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::PSSetSamplers(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
        /* [annotation] */ 
        __in_ecount(NumSamplers) D3D11Base::ID3D11SamplerState *const *ppSamplers)
{
	GetD3D11DeviceContext()->PSSetSamplers(StartSlot, NumSamplers, ppSamplers);
}

struct DrawContext
{
	bool skip;
	bool override;
	float oldSeparation;
};
static DrawContext BeforeDraw(D3D11Wrapper::ID3D11DeviceContext *context)
{
	DrawContext data;

	// Skip?
	data.override = false;
	data.skip = G->mBlockingMode;

	// If we are not hunting shaders, we can skip all of this shader management for a performance bump.
	// ToDo: this also kills texture overrides (not used in AC3 fix)
	if (!G->hunting)
		return data;

	float separationValue;
	int selectedRenderTargetPos;
	if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
	// Stats
	if (G->mCurrentVertexShader && G->mCurrentPixelShader)
	{
		G->mVertexShaderInfo[G->mCurrentVertexShader].PartnerShader.insert(G->mCurrentPixelShader);
		G->mPixelShaderInfo[G->mCurrentPixelShader].PartnerShader.insert(G->mCurrentVertexShader);
	}
	if (G->mCurrentPixelShader)
		G->mPixelShaderInfo[G->mCurrentPixelShader].RenderTargets = G->mCurrentRenderTargets;
	// Selection
	for (selectedRenderTargetPos = 0; selectedRenderTargetPos < G->mCurrentRenderTargets.size(); ++selectedRenderTargetPos)
		if (G->mCurrentRenderTargets[selectedRenderTargetPos] == G->mSelectedRenderTarget) break;
	if (G->mCurrentIndexBuffer == G->mSelectedIndexBuffer ||
		G->mCurrentVertexShader == G->mSelectedVertexShader ||
		G->mCurrentPixelShader == G->mSelectedPixelShader ||
		selectedRenderTargetPos < G->mCurrentRenderTargets.size())
	{
		if (LogFile && LogDebug) fprintf(LogFile, "  Skipping selected operation. CurrentIndexBuffer = %08lx%08lx, CurrentVertexShader = %08lx%08lx, CurrentPixelShader = %08lx%08lx\n", 
			(UINT32)(G->mCurrentIndexBuffer >> 32), (UINT32)G->mCurrentIndexBuffer,
			(UINT32)(G->mCurrentVertexShader >> 32), (UINT32)G->mCurrentVertexShader,
			(UINT32)(G->mCurrentPixelShader >> 32), (UINT32)G->mCurrentPixelShader);
		
		// Snapshot render target list.
		if (G->mSelectedRenderTargetSnapshot != G->mSelectedRenderTarget)
		{
			G->mSelectedRenderTargetSnapshotList.clear();
			G->mSelectedRenderTargetSnapshot = G->mSelectedRenderTarget;
		}
		G->mSelectedRenderTargetSnapshotList.insert(G->mCurrentRenderTargets.begin(), G->mCurrentRenderTargets.end());
		// Snapshot info.
		if (G->mCurrentIndexBuffer == G->mSelectedIndexBuffer)
		{
			G->mSelectedIndexBuffer_VertexShader.insert(G->mCurrentVertexShader);
			G->mSelectedIndexBuffer_PixelShader.insert(G->mCurrentPixelShader);
		}
		if (G->mCurrentVertexShader == G->mSelectedVertexShader)
			G->mSelectedVertexShader_IndexBuffer.insert(G->mCurrentIndexBuffer);
		if (G->mCurrentPixelShader == G->mSelectedPixelShader)
			G->mSelectedPixelShader_IndexBuffer.insert(G->mCurrentIndexBuffer);
		if (G->marking_mode == MARKING_MODE_MONO)
		{
			data.override = true;
			separationValue = 0;
		}
		else if (G->marking_mode == MARKING_MODE_SKIP)
		{
			data.skip = true;
			if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
			return data;
		}
	}

	// Override settings?
	ShaderSeparationMap::iterator i = G->mShaderSeparationMap.find(G->mCurrentVertexShader);
	if (i == G->mShaderSeparationMap.end()) i = G->mShaderSeparationMap.find(G->mCurrentPixelShader);
	if (i != G->mShaderSeparationMap.end())
	{
		if (LogFile && LogDebug) fprintf(LogFile, "  seperation override found for shader\n");
		
		data.override = true;
		separationValue = i->second;
		if (separationValue == 10000)
			data.skip = true;
		// Check iteration.
		ShaderIterationMap::iterator j = G->mShaderIterationMap.find(i->first);
		if (j != G->mShaderIterationMap.end())
		{
			std::vector<int>::iterator k = j->second.begin();
			int currentIteration = *k = *k + 1;
			if (LogFile && LogDebug) fprintf(LogFile, "  current iteration = %d\n", currentIteration);
			
			data.override = false;
			while (++k != j->second.end())
			{
				if (currentIteration == *k)
				{
					data.override = true;
					break;
				}
			}
			if (!data.override)
			{
				if (LogFile && LogDebug) fprintf(LogFile, "  override skipped\n");
			}
		}
		// Check index buffer filter.
		ShaderIndexBufferFilter::iterator k = G->mShaderIndexBufferFilter.find(i->first);
		if (k != G->mShaderIndexBufferFilter.end())
		{
			bool found = false;
			for (vector<UINT64>::iterator l = k->second.begin(); l != k->second.end(); ++l)
				if (G->mCurrentIndexBuffer == *l)
				{
					found = true;
					break;
				}
			if (!found)
			{
				data.override = false;
				data.skip = false;
			}
		}
	}
	if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
	if (data.override)
	{
		D3D11Wrapper::ID3D11Device *device;
		context->GetDevice(&device);
		if (device->mStereoHandle)
		{
			if (LogFile && LogDebug) fprintf(LogFile, "  setting custom separation value\n");
			
			if (D3D11Base::NVAPI_OK != D3D11Base::NvAPI_Stereo_GetSeparation(device->mStereoHandle, &data.oldSeparation))
			{
				if (LogFile && LogDebug) fprintf(LogFile, "    Stereo_GetSeparation failed.\n");
			}
			NvAPIOverride();
			if (D3D11Base::NVAPI_OK != D3D11Base::NvAPI_Stereo_SetSeparation(device->mStereoHandle, separationValue * data.oldSeparation))
			{
				if (LogFile && LogDebug) fprintf(LogFile, "    Stereo_SetSeparation failed.\n");
			}
		}
		device->Release();
	}
	return data;
}

static void AfterDraw(DrawContext &data, D3D11Wrapper::ID3D11DeviceContext *context)
{
	if (data.skip)
		return;
	if (data.override)
	{
		D3D11Wrapper::ID3D11Device *device;
		context->GetDevice(&device);
		if (device->mStereoHandle)
		{
			NvAPIOverride();
			if (D3D11Base::NVAPI_OK != D3D11Base::NvAPI_Stereo_SetSeparation(device->mStereoHandle, data.oldSeparation))
			{
				if (LogFile && LogDebug) fprintf(LogFile, "    Stereo_SetSeparation failed.\n");
			}
		}
		device->Release();
	}
}

STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::VSSetShader(THIS_
        /* [annotation] */ 
        __in_opt D3D11Base::ID3D11VertexShader *pVertexShader,
        /* [annotation] */ 
        __in_ecount_opt(NumClassInstances) D3D11Base::ID3D11ClassInstance *const *ppClassInstances,
        UINT NumClassInstances)
{
	if (LogFile && LogDebug) fprintf(LogFile, "ID3D11DeviceContext::VSSetShader called with vertexshader handle = %x\n", pVertexShader);

	bool patchedShader = false;
	if (G->hunting && pVertexShader)
	{
		// Store as current vertex shader.
		if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
		VertexShaderMap::iterator i = G->mVertexShaders.find(pVertexShader);
		if (i != G->mVertexShaders.end())
		{
			G->mCurrentVertexShader = i->second;
			if (LogFile && LogDebug) fprintf(LogFile, "  vertex shader found: handle = %x, hash = %08lx%08lx\n", pVertexShader, (UINT32)(G->mCurrentVertexShader >> 32), (UINT32)G->mCurrentVertexShader);

			// Add to visited vertex shaders.
			G->mVisitedVertexShaders.insert(i->second);
			patchedShader = true;

			// second try to hide index buffer.
			// if (mCurrentIndexBuffer == mSelectedIndexBuffer)
			//	pIndexBuffer = 0;
		}
		else 
		{
			if (LogFile && LogDebug) fprintf(LogFile, "  vertex shader %x not found\n", pVertexShader);
			// G->mCurrentVertexShader = 0;
		}
		
		// Replacement map.
		VertexShaderReplacementMap::iterator j = G->mOriginalVertexShaders.find(pVertexShader);
		if (G->mSelectedVertexShader == G->mCurrentVertexShader && j != G->mOriginalVertexShaders.end())
		{
			D3D11Base::ID3D11VertexShader *shader = j->second;
			if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
			GetD3D11DeviceContext()->VSSetShader(shader, ppClassInstances, NumClassInstances);
			return;
		}
		j = G->mZeroVertexShaders.find(pVertexShader);
		if (G->mSelectedVertexShader == G->mCurrentVertexShader && j != G->mZeroVertexShaders.end())
		{
			D3D11Base::ID3D11VertexShader *shader = j->second;
			if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
			GetD3D11DeviceContext()->VSSetShader(shader, ppClassInstances, NumClassInstances);
			return;
		}

		// If the shader has been live reloaded from ShaderFixes, use the new one
		ShaderReloadMap::iterator it = G->mReloadedShaders.find(pVertexShader);
		if (it != G->mReloadedShaders.end() && it->second.newShader != NULL)
		{
			if (LogFile && LogDebug) fprintf(LogFile, "  vertex shader replaced by: %x\n", it->second.newShader);

			D3D11Base::ID3D11VertexShader *shader = (D3D11Base::ID3D11VertexShader*) it->second.newShader;
			if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
			GetD3D11DeviceContext()->VSSetShader(shader, ppClassInstances, NumClassInstances);
			return;
		}
		if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
	}

	GetD3D11DeviceContext()->VSSetShader(pVertexShader, ppClassInstances, NumClassInstances);

	// When hunting is off, send stereo texture to all shaders, as any might need it.
	// Maybe a bit of a waste of GPU resource, but optimizes CPU use.
	if (!G->hunting || patchedShader)
	{
		D3D11Wrapper::ID3D11Device *device = 0;
		GetDevice(&device);
		if (device)
		{
			// Set NVidia stereo texture.
			if (device->mStereoResourceView)
			{
				if (LogFile && LogDebug) fprintf(LogFile, "  adding NVidia stereo parameter texture to shader resources in slot 125.\n");
				
				GetD3D11DeviceContext()->VSSetShaderResources(125, 1, &device->mStereoResourceView);
			}
			device->Release();
		}
		else
		{
			if (LogFile) fprintf(LogFile, "  error querying device. Can't set NVidia stereo parameter texture.\n");
		}
	}
}
 
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::DrawIndexed(THIS_
        /* [annotation] */ 
        __in  UINT IndexCount,
        /* [annotation] */ 
        __in  UINT StartIndexLocation,
        /* [annotation] */ 
        __in  INT BaseVertexLocation)
{
	if (LogFile && LogDebug) fprintf(LogFile, "ID3D11DeviceContext::DrawIndexed called with IndexCount = %d, StartIndexLocation = %d, BaseVertexLocation = %d\n", 
		IndexCount, StartIndexLocation, BaseVertexLocation);

	DrawContext c = BeforeDraw(this);
	if (!c.skip)
		GetD3D11DeviceContext()->DrawIndexed(IndexCount, StartIndexLocation, BaseVertexLocation);
	AfterDraw(c, this);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::Draw(THIS_
        /* [annotation] */ 
        __in  UINT VertexCount,
        /* [annotation] */ 
        __in  UINT StartVertexLocation)
{
	if (LogFile && LogDebug) fprintf(LogFile, "ID3D11DeviceContext::Draw called with VertexCount = %d, StartVertexLocation = %d\n", 
		VertexCount, StartVertexLocation);

	DrawContext c = BeforeDraw(this);
	if (!c.skip)
		GetD3D11DeviceContext()->Draw(VertexCount, StartVertexLocation);
	AfterDraw(c, this);
}
        
STDMETHODIMP D3D11Wrapper::ID3D11DeviceContext::Map(THIS_
        /* [annotation] */ 
        __in  D3D11Base::ID3D11Resource *pResource,
        /* [annotation] */ 
        __in  UINT Subresource,
        /* [annotation] */ 
        __in  D3D11Base::D3D11_MAP MapType,
        /* [annotation] */ 
        __in  UINT MapFlags,
        /* [annotation] */ 
        __out D3D11Base::D3D11_MAPPED_SUBRESOURCE *pMappedResource)
{
	return GetD3D11DeviceContext()->Map(pResource, Subresource, MapType, MapFlags, pMappedResource);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::Unmap(THIS_
        /* [annotation] */ 
        __in D3D11Base::ID3D11Resource *pResource,
        /* [annotation] */ 
        __in  UINT Subresource)
{
	GetD3D11DeviceContext()->Unmap(pResource, Subresource);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::PSSetConstantBuffers(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
        /* [annotation] */ 
        __in_ecount(NumBuffers) D3D11Base::ID3D11Buffer *const *ppConstantBuffers)
{
	GetD3D11DeviceContext()->PSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::IASetInputLayout(THIS_
        /* [annotation] */ 
        __in_opt D3D11Base::ID3D11InputLayout *pInputLayout)
{
	GetD3D11DeviceContext()->IASetInputLayout(pInputLayout);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::IASetVertexBuffers(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        __in_range( 0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumBuffers,
        /* [annotation] */ 
        __in_ecount(NumBuffers)  D3D11Base::ID3D11Buffer *const *ppVertexBuffers,
        /* [annotation] */ 
        __in_ecount(NumBuffers)  const UINT *pStrides,
        /* [annotation] */ 
        __in_ecount(NumBuffers)  const UINT *pOffsets)
{
	GetD3D11DeviceContext()->IASetVertexBuffers(StartSlot, NumBuffers, ppVertexBuffers, pStrides, pOffsets);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::IASetIndexBuffer(THIS_
        /* [annotation] */ 
        __in_opt D3D11Base::ID3D11Buffer *pIndexBuffer,
        /* [annotation] */ 
        __in D3D11Base::DXGI_FORMAT Format,
        /* [annotation] */ 
        __in  UINT Offset)
{
	if (LogFile && LogDebug) fprintf(LogFile, "ID3D11DeviceContext::IASetIndexBuffer called\n");

	if (G->hunting && pIndexBuffer)
	{
		// Store as current index buffer.
		if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
		DataBufferMap::iterator i = G->mDataBuffers.find(pIndexBuffer);
		if (i != G->mDataBuffers.end())
		{
			G->mCurrentIndexBuffer = i->second;
			if (LogFile && LogDebug) fprintf(LogFile, "  index buffer found: handle = %x, hash = %08lx%08lx\n", pIndexBuffer, (UINT32)(G->mCurrentIndexBuffer >> 32), (UINT32)G->mCurrentIndexBuffer);

			// Add to visited index buffers.
			G->mVisitedIndexBuffers.insert(G->mCurrentIndexBuffer);

			// second try to hide index buffer.
			// if (mCurrentIndexBuffer == mSelectedIndexBuffer)
			//	pIndexBuffer = 0;
		}
		else if (LogFile && LogDebug) fprintf(LogFile, "  index buffer %x not found\n", pIndexBuffer);
		if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
	}

	GetD3D11DeviceContext()->IASetIndexBuffer(pIndexBuffer, Format, Offset);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::DrawIndexedInstanced(THIS_
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
	if (LogFile && LogDebug) fprintf(LogFile, "ID3D11DeviceContext::DrawIndexedInstanced called with IndexCountPerInstance = %d, InstanceCount = %d\n", 
		IndexCountPerInstance, InstanceCount);

	DrawContext c = BeforeDraw(this);
	if (!c.skip)
		GetD3D11DeviceContext()->DrawIndexedInstanced(IndexCountPerInstance, InstanceCount, StartIndexLocation, 
			BaseVertexLocation, StartInstanceLocation);
	AfterDraw(c, this);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::DrawInstanced(THIS_
        /* [annotation] */ 
        __in  UINT VertexCountPerInstance,
        /* [annotation] */ 
        __in  UINT InstanceCount,
        /* [annotation] */ 
        __in  UINT StartVertexLocation,
        /* [annotation] */ 
        __in  UINT StartInstanceLocation)
{
	if (LogFile && LogDebug) fprintf(LogFile, "ID3D11DeviceContext::DrawInstanced called with VertexCountPerInstance = %d, InstanceCount = %d\n", 
		VertexCountPerInstance, InstanceCount);

	DrawContext c = BeforeDraw(this);
	if (!c.skip)
		GetD3D11DeviceContext()->DrawInstanced(VertexCountPerInstance, InstanceCount, StartVertexLocation, StartInstanceLocation);
	AfterDraw(c, this);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::GSSetConstantBuffers(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
        /* [annotation] */ 
        __in_ecount(NumBuffers) D3D11Base::ID3D11Buffer *const *ppConstantBuffers)
{
	GetD3D11DeviceContext()->GSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::GSSetShader(THIS_
        /* [annotation] */ 
        __in_opt D3D11Base::ID3D11GeometryShader *pShader,
        /* [annotation] */ 
        __in_ecount_opt(NumClassInstances) D3D11Base::ID3D11ClassInstance *const *ppClassInstances,
        UINT NumClassInstances)
{
	GetD3D11DeviceContext()->GSSetShader(pShader, ppClassInstances, NumClassInstances);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::IASetPrimitiveTopology(THIS_
        /* [annotation] */ 
        __in D3D11Base::D3D11_PRIMITIVE_TOPOLOGY Topology)
{
	GetD3D11DeviceContext()->IASetPrimitiveTopology(Topology);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::VSSetShaderResources(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
        /* [annotation] */ 
        __in_ecount(NumViews) D3D11Base::ID3D11ShaderResourceView *const *ppShaderResourceViews)
{
	if (LogFile && LogDebug) fprintf(LogFile, "ID3D11DeviceContext::VSSetShaderResources called with StartSlot = %d, NumViews = %d\n", StartSlot, NumViews);
	if (ppShaderResourceViews && NumViews && LogFile && LogDebug) fprintf(LogFile, "  ShaderResourceView[0] handle = %x\n", *ppShaderResourceViews);	

	GetD3D11DeviceContext()->VSSetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
		
	// Resolve resource from resource view.
	if (G->hunting && ppShaderResourceViews)
	{
		if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
		for (int i = 0; i < NumViews; ++i)
		{
			int pos = StartSlot + i;
			if (!ppShaderResourceViews[i])
			{
				G->mVertexShaderInfo[G->mCurrentVertexShader].ResourceRegisters[pos] = 0;
				continue;
			}
			D3D11Base::D3D11_SHADER_RESOURCE_VIEW_DESC desc;
			ppShaderResourceViews[i]->GetDesc(&desc);
			if (desc.ViewDimension == D3D11Base::D3D11_SRV_DIMENSION_TEXTURE2D ||
				desc.ViewDimension == D3D11Base::D3D11_SRV_DIMENSION_TEXTURE2DMS ||
				desc.ViewDimension == D3D11Base::D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY)
			{
				D3D11Base::ID3D11Resource *pResource = 0;
				ppShaderResourceViews[i]->GetResource(&pResource);
				if (pResource)
				{
					D3D11Base::ID3D11Texture2D *texture = (D3D11Base::ID3D11Texture2D *)pResource;
					D3D11Base::D3D11_TEXTURE2D_DESC texDesc;
					texture->GetDesc(&texDesc);
					pResource->Release();
					std::map<D3D11Base::ID3D11Texture2D *, UINT64>::iterator j = G->mTexture2D_ID.find(texture);
					UINT64 hash = 0;
					if (j == G->mTexture2D_ID.end())
					{
						// Create hash again.
						hash ^= texDesc.Width; hash *= FNV_64_PRIME;
						hash ^= texDesc.Height; hash *= FNV_64_PRIME;
						hash ^= texDesc.MipLevels; hash *= FNV_64_PRIME;
						hash ^= texDesc.ArraySize; hash *= FNV_64_PRIME;
						hash ^= texDesc.Format; hash *= FNV_64_PRIME;
						hash ^= texDesc.Usage; hash *= FNV_64_PRIME;
						hash ^= texDesc.BindFlags; hash *= FNV_64_PRIME;
						hash ^= texDesc.CPUAccessFlags; hash *= FNV_64_PRIME;
						hash ^= texDesc.MiscFlags;
					}
					else
					{
						hash = j->second;
					}
					G->mRenderTargets[texture] = hash;
					G->mVertexShaderInfo[G->mCurrentVertexShader].ResourceRegisters[pos] = texture;
				}
			}
			else if (desc.ViewDimension == D3D11Base::D3D11_SRV_DIMENSION_TEXTURE3D)
			{
				D3D11Base::ID3D11Resource *pResource = 0;
				ppShaderResourceViews[i]->GetResource(&pResource);
				if (pResource)
				{
					D3D11Base::ID3D11Texture3D *texture = (D3D11Base::ID3D11Texture3D *)pResource;
					D3D11Base::D3D11_TEXTURE3D_DESC texDesc;
					texture->GetDesc(&texDesc);
					pResource->Release();
					std::map<D3D11Base::ID3D11Texture3D *, UINT64>::iterator j = G->mTexture3D_ID.find(texture);
					UINT64 hash = 0;
					if (j == G->mTexture3D_ID.end())
					{
						// Create hash again.
						hash ^= texDesc.Width; hash *= FNV_64_PRIME;
						hash ^= texDesc.Height; hash *= FNV_64_PRIME;
						hash ^= texDesc.Depth; hash *= FNV_64_PRIME;
						hash ^= texDesc.MipLevels; hash *= FNV_64_PRIME;
						hash ^= texDesc.Format; hash *= FNV_64_PRIME;
						hash ^= texDesc.Usage; hash *= FNV_64_PRIME;
						hash ^= texDesc.BindFlags; hash *= FNV_64_PRIME;
						hash ^= texDesc.CPUAccessFlags; hash *= FNV_64_PRIME;
						hash ^= texDesc.MiscFlags;
					}
					else
					{
						hash = j->second;
					}
					G->mRenderTargets[texture] = hash;
					G->mVertexShaderInfo[G->mCurrentVertexShader].ResourceRegisters[pos] = texture;
				}
			}
		}
		if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
	}

	/*
	// Map nvidia texture slot.
	if (ppShaderResourceViews && NumViews && ppShaderResourceViews[0])
	{
		ID3D11Device *device = 0;
		GetDevice(&device);
		if (device && device->mStereoResourceView)
		{
			if (LogFile && LogDebug) fprintf(LogFile, "  adding NVidia stereo parameter texture to shader resources in slot 125.\n");
			
			m_pContext->VSSetShaderResources(125, 1, &device->mStereoResourceView);
		}
		else
		{
			if (LogFile) fprintf(LogFile, "  error querying device. Can't set NVidia stereo parameter texture.\n");
		}
		device->Release();
	}
	*/
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::VSSetSamplers(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
        /* [annotation] */ 
        __in_ecount(NumSamplers) D3D11Base::ID3D11SamplerState *const *ppSamplers)
{
	GetD3D11DeviceContext()->VSSetSamplers(StartSlot, NumSamplers, ppSamplers);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::Begin(THIS_
        /* [annotation] */ 
        __in  D3D11Base::ID3D11Asynchronous *pAsync)
{
	GetD3D11DeviceContext()->Begin(pAsync);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::End(THIS_
        /* [annotation] */ 
        __in  D3D11Base::ID3D11Asynchronous *pAsync)
{
	if (LogFile && LogDebug) fprintf(LogFile, "ID3D11DeviceContext::End called\n");
	
	GetD3D11DeviceContext()->End(pAsync);
}
        
STDMETHODIMP D3D11Wrapper::ID3D11DeviceContext::GetData(THIS_
        /* [annotation] */ 
        __in  D3D11Base::ID3D11Asynchronous *pAsync,
        /* [annotation] */ 
        __out_bcount_opt( DataSize )  void *pData,
        /* [annotation] */ 
        __in  UINT DataSize,
        /* [annotation] */ 
        __in  UINT GetDataFlags)
{
	return GetD3D11DeviceContext()->GetData(pAsync, pData, DataSize, GetDataFlags);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::SetPredication(THIS_
        /* [annotation] */ 
        __in_opt D3D11Base::ID3D11Predicate *pPredicate,
        /* [annotation] */ 
        __in  BOOL PredicateValue)
{
	return GetD3D11DeviceContext()->SetPredication(pPredicate, PredicateValue);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::GSSetShaderResources(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
        /* [annotation] */ 
        __in_ecount(NumViews) D3D11Base::ID3D11ShaderResourceView *const *ppShaderResourceViews)
{
	GetD3D11DeviceContext()->GSSetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::GSSetSamplers(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
        /* [annotation] */ 
        __in_ecount(NumSamplers) D3D11Base::ID3D11SamplerState *const *ppSamplers)
{
	GetD3D11DeviceContext()->GSSetSamplers(StartSlot, NumSamplers, ppSamplers);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::OMSetRenderTargets(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT )  UINT NumViews,
        /* [annotation] */ 
        __in_ecount_opt(NumViews) D3D11Base::ID3D11RenderTargetView *const *ppRenderTargetViews,
        /* [annotation] */ 
        __in_opt D3D11Base::ID3D11DepthStencilView *pDepthStencilView)
{
	if (LogFile && LogDebug) fprintf(LogFile, "ID3D11DeviceContext::OMSetRenderTargets called with NumViews = %d\n", NumViews);
	
	if (G->hunting)
	{
		if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
		G->mCurrentRenderTargets.clear();
		if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
		for (int i = 0; i < NumViews; ++i)
		{
			if (ppRenderTargetViews[i] == 0) continue;
			D3D11Base::D3D11_RENDER_TARGET_VIEW_DESC desc;
			ppRenderTargetViews[i]->GetDesc(&desc);
			if (LogFile && LogDebug) fprintf(LogFile, "  View #%d, Format = %d, Is2D = %d\n", i, desc.Format, D3D11Base::D3D11_RTV_DIMENSION_TEXTURE2D == desc.ViewDimension);
			D3D11Base::ID3D11Resource *pResource;
			if (D3D11Base::D3D11_RTV_DIMENSION_TEXTURE2D == desc.ViewDimension ||
				D3D11Base::D3D11_RTV_DIMENSION_TEXTURE2DMS == desc.ViewDimension)
			{
				ppRenderTargetViews[i]->GetResource(&pResource);
				D3D11Base::ID3D11Texture2D *targetTexture = (D3D11Base::ID3D11Texture2D *)pResource;
				D3D11Base::D3D11_TEXTURE2D_DESC texDesc;
				targetTexture->GetDesc(&texDesc);
				pResource->Release();

				// Registered?
				std::map<D3D11Base::ID3D11Texture2D *, UINT64>::iterator tex = G->mTexture2D_ID.find(targetTexture);
				UINT64 hash = 0;
				if (tex == G->mTexture2D_ID.end())
				{
					if (LogFile && LogDebug) fprintf(LogFile, "    Unknown render target:\n");
					if (LogFile && LogDebug) fprintf(LogFile, "    Width = %d, Height = %d, MipLevels = %d, ArraySize = %d\n", texDesc.Width, texDesc.Height,
						texDesc.MipLevels, texDesc.ArraySize);
					if (LogFile && LogDebug) fprintf(LogFile, "    Format = %d, Usage = %x, BindFlags = %x, CPUAccessFlags = %x, MiscFlags = %x\n", texDesc.Format,
						texDesc.Usage, texDesc.BindFlags, texDesc.CPUAccessFlags, texDesc.MiscFlags);
					// Register current and visited targets.
					hash ^= texDesc.Width; hash *= FNV_64_PRIME;
					hash ^= texDesc.Height; hash *= FNV_64_PRIME;
					hash ^= texDesc.MipLevels; hash *= FNV_64_PRIME;
					hash ^= texDesc.ArraySize; hash *= FNV_64_PRIME;
					hash ^= texDesc.Format; hash *= FNV_64_PRIME;
					hash ^= texDesc.Usage; hash *= FNV_64_PRIME;
					hash ^= texDesc.BindFlags; hash *= FNV_64_PRIME;
					hash ^= texDesc.CPUAccessFlags; hash *= FNV_64_PRIME;
					hash ^= texDesc.MiscFlags;
				}
				else
				{
					hash = tex->second;
				}
				if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
				G->mRenderTargets[targetTexture] = hash;
				G->mCurrentRenderTargets.push_back(targetTexture);
				G->mVisitedRenderTargets.insert(targetTexture);
				if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
			}
			else if (D3D11Base::D3D11_RTV_DIMENSION_TEXTURE3D == desc.ViewDimension)
			{
				ppRenderTargetViews[i]->GetResource(&pResource);
				D3D11Base::ID3D11Texture3D *targetTexture = (D3D11Base::ID3D11Texture3D *)pResource;
				D3D11Base::D3D11_TEXTURE3D_DESC texDesc;
				targetTexture->GetDesc(&texDesc);
				pResource->Release();

				// Registered?
				std::map<D3D11Base::ID3D11Texture3D *, UINT64>::iterator tex = G->mTexture3D_ID.find(targetTexture);
				UINT64 hash = 0;
				if (tex == G->mTexture3D_ID.end())
				{
					if (LogFile && LogDebug) fprintf(LogFile, "    Unknown 3D render target:\n");
					if (LogFile && LogDebug) fprintf(LogFile, "    Width = %d, Height = %d, MipLevels = %d\n", texDesc.Width, texDesc.Height, texDesc.MipLevels);
					if (LogFile && LogDebug) fprintf(LogFile, "    Format = %d, Usage = %x, BindFlags = %x, CPUAccessFlags = %x, MiscFlags = %x\n", texDesc.Format,
						texDesc.Usage, texDesc.BindFlags, texDesc.CPUAccessFlags, texDesc.MiscFlags);
					// Register current and visited targets.
					hash ^= texDesc.Width; hash *= FNV_64_PRIME;
					hash ^= texDesc.Height; hash *= FNV_64_PRIME;
					hash ^= texDesc.Depth; hash *= FNV_64_PRIME;
					hash ^= texDesc.MipLevels; hash *= FNV_64_PRIME;
					hash ^= texDesc.Format; hash *= FNV_64_PRIME;
					hash ^= texDesc.Usage; hash *= FNV_64_PRIME;
					hash ^= texDesc.BindFlags; hash *= FNV_64_PRIME;
					hash ^= texDesc.CPUAccessFlags; hash *= FNV_64_PRIME;
					hash ^= texDesc.MiscFlags;
				}
				else
				{
					hash = tex->second;
				}
				if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
				G->mRenderTargets[targetTexture] = hash;
				G->mCurrentRenderTargets.push_back(targetTexture);
				G->mVisitedRenderTargets.insert(targetTexture);
				if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
			}
		}
	}

	GetD3D11DeviceContext()->OMSetRenderTargets(NumViews, ppRenderTargetViews, pDepthStencilView);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::OMSetRenderTargetsAndUnorderedAccessViews(THIS_
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
        __in_ecount_opt(NumUAVs)  const UINT *pUAVInitialCounts)
{
	if (LogFile) fprintf(LogFile, "ID3D11DeviceContext::OMSetRenderTargetsAndUnorderedAccessViews called with NumRTVs = %d, NumUAVs = %d\n", NumRTVs, NumUAVs);
	
	GetD3D11DeviceContext()->OMSetRenderTargetsAndUnorderedAccessViews(NumRTVs, ppRenderTargetViews, pDepthStencilView,
		UAVStartSlot, NumUAVs, ppUnorderedAccessViews, pUAVInitialCounts);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::OMSetBlendState(THIS_
        /* [annotation] */ 
        __in_opt  D3D11Base::ID3D11BlendState *pBlendState,
        /* [annotation] */ 
        __in_opt  const FLOAT BlendFactor[ 4 ],
        /* [annotation] */ 
        __in  UINT SampleMask)
{
	GetD3D11DeviceContext()->OMSetBlendState(pBlendState, BlendFactor, SampleMask);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::OMSetDepthStencilState(THIS_
        /* [annotation] */ 
        __in_opt  D3D11Base::ID3D11DepthStencilState *pDepthStencilState,
        /* [annotation] */ 
        __in  UINT StencilRef)
{
	GetD3D11DeviceContext()->OMSetDepthStencilState(pDepthStencilState, StencilRef);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::SOSetTargets(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_SO_BUFFER_SLOT_COUNT)  UINT NumBuffers,
        /* [annotation] */ 
        __in_ecount_opt(NumBuffers)  D3D11Base::ID3D11Buffer *const *ppSOTargets,
        /* [annotation] */ 
        __in_ecount_opt(NumBuffers)  const UINT *pOffsets)
{
	if (LogFile) fprintf(LogFile, "ID3D11DeviceContext::SOSetTargets called with NumBuffers = %d\n", NumBuffers);
	
	GetD3D11DeviceContext()->SOSetTargets(NumBuffers, ppSOTargets, pOffsets);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::DrawAuto(THIS)
{
	if (LogFile && LogDebug) fprintf(LogFile, "ID3D11DeviceContext::DrawAuto called\n");

	DrawContext c = BeforeDraw(this);
	if (!c.skip)
		GetD3D11DeviceContext()->DrawAuto();
	AfterDraw(c, this);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::DrawIndexedInstancedIndirect(THIS_
        /* [annotation] */ 
        __in  D3D11Base::ID3D11Buffer *pBufferForArgs,
        /* [annotation] */ 
        __in  UINT AlignedByteOffsetForArgs)
{
	if (LogFile && LogDebug) fprintf(LogFile, "ID3D11DeviceContext::DrawIndexedInstancedIndirect called\n");

	DrawContext c = BeforeDraw(this);
	if (!c.skip)
		GetD3D11DeviceContext()->DrawIndexedInstancedIndirect(pBufferForArgs, AlignedByteOffsetForArgs);
	AfterDraw(c, this);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::DrawInstancedIndirect(THIS_
        /* [annotation] */ 
        __in  D3D11Base::ID3D11Buffer *pBufferForArgs,
        /* [annotation] */ 
        __in  UINT AlignedByteOffsetForArgs)
{
	if (LogFile && LogDebug) fprintf(LogFile, "ID3D11DeviceContext::DrawInstancedIndirect called\n");

	DrawContext c = BeforeDraw(this);
	if (!c.skip)
		GetD3D11DeviceContext()->DrawInstancedIndirect(pBufferForArgs, AlignedByteOffsetForArgs);
	AfterDraw(c, this);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::Dispatch(THIS_
        /* [annotation] */ 
        __in  UINT ThreadGroupCountX,
        /* [annotation] */ 
        __in  UINT ThreadGroupCountY,
        /* [annotation] */ 
        __in  UINT ThreadGroupCountZ)
{
	GetD3D11DeviceContext()->Dispatch(ThreadGroupCountX, ThreadGroupCountY, ThreadGroupCountZ);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::DispatchIndirect(THIS_
        /* [annotation] */ 
        __in  D3D11Base::ID3D11Buffer *pBufferForArgs,
        /* [annotation] */ 
        __in  UINT AlignedByteOffsetForArgs)
{
	GetD3D11DeviceContext()->DispatchIndirect(pBufferForArgs, AlignedByteOffsetForArgs);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::RSSetState(THIS_
        /* [annotation] */ 
        __in_opt  D3D11Base::ID3D11RasterizerState *pRasterizerState)
{
	GetD3D11DeviceContext()->RSSetState(pRasterizerState);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::RSSetViewports(THIS_
        /* [annotation] */ 
        __in_range(0, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE)  UINT NumViewports,
        /* [annotation] */ 
        __in_ecount_opt(NumViewports)  const D3D11Base::D3D11_VIEWPORT *pViewports)
{
	GetD3D11DeviceContext()->RSSetViewports(NumViewports, pViewports);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::RSSetScissorRects(THIS_
        /* [annotation] */ 
        __in_range(0, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE)  UINT NumRects,
        /* [annotation] */ 
        __in_ecount_opt(NumRects)  const D3D11Base::D3D11_RECT *pRects)
{
	GetD3D11DeviceContext()->RSSetScissorRects(NumRects, pRects);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::CopySubresourceRegion(THIS_
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
        __in_opt  const D3D11Base::D3D11_BOX *pSrcBox)
{
	GetD3D11DeviceContext()->CopySubresourceRegion(pDstResource, DstSubresource, DstX, DstY, DstZ,
		pSrcResource, SrcSubresource, pSrcBox);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::CopyResource(THIS_
        /* [annotation] */ 
        __in  D3D11Base::ID3D11Resource *pDstResource,
        /* [annotation] */ 
        __in  D3D11Base::ID3D11Resource *pSrcResource)
{
	GetD3D11DeviceContext()->CopyResource(pDstResource, pSrcResource);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::UpdateSubresource(THIS_
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
        __in  UINT SrcDepthPitch)
{
	GetD3D11DeviceContext()->UpdateSubresource(pDstResource, DstSubresource, pDstBox, pSrcData, SrcRowPitch,
		SrcDepthPitch);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::CopyStructureCount(THIS_
        /* [annotation] */ 
        __in  D3D11Base::ID3D11Buffer *pDstBuffer,
        /* [annotation] */ 
        __in  UINT DstAlignedByteOffset,
        /* [annotation] */ 
        __in  D3D11Base::ID3D11UnorderedAccessView *pSrcView)
{
	GetD3D11DeviceContext()->CopyStructureCount(pDstBuffer, DstAlignedByteOffset, pSrcView);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::ClearRenderTargetView(THIS_
        /* [annotation] */ 
        __in  D3D11Base::ID3D11RenderTargetView *pRenderTargetView,
        /* [annotation] */ 
        __in  const FLOAT ColorRGBA[ 4 ])
{
	if (LogFile && LogDebug) fprintf(LogFile, "ID3D11DeviceContext::ClearRenderTargetView called with RenderTargetView=%x, color=[%f,%f,%f,%f]\n", pRenderTargetView, 
		ColorRGBA[0], ColorRGBA[1], ColorRGBA[2], ColorRGBA[3]);

	//if (G->hunting)
	{
		// Update stereo parameter texture.
		if (LogFile && LogDebug) fprintf(LogFile, "  updating stereo parameter texture.\n");

		ID3D11Device *device;
		GetDevice(&device);

		device->mParamTextureManager.mScreenWidth = G->mSwapChainInfo.width;
		device->mParamTextureManager.mScreenHeight = G->mSwapChainInfo.height;
		if (G->ENABLE_TUNE)
		{
			//device->mParamTextureManager.mSeparationModifier = gTuneValue;
			device->mParamTextureManager.mTuneVariable1 = G->gTuneValue1;
			device->mParamTextureManager.mTuneVariable2 = G->gTuneValue2;
			device->mParamTextureManager.mTuneVariable3 = G->gTuneValue3;
			device->mParamTextureManager.mTuneVariable4 = G->gTuneValue4;
			static int counter = 0;
			if (counter-- < 0)
			{
				counter = 30;
				device->mParamTextureManager.mForceUpdate = true;
			}
		}

		device->mParamTextureManager.UpdateStereoTexture(device->GetD3D11Device(), GetD3D11DeviceContext(), device->mStereoTexture, false);
		device->Release();
	}

	GetD3D11DeviceContext()->ClearRenderTargetView(pRenderTargetView, ColorRGBA);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::ClearUnorderedAccessViewUint(THIS_
        /* [annotation] */ 
        __in  D3D11Base::ID3D11UnorderedAccessView *pUnorderedAccessView,
        /* [annotation] */ 
        __in  const UINT Values[ 4 ])
{
	GetD3D11DeviceContext()->ClearUnorderedAccessViewUint(pUnorderedAccessView, Values);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::ClearUnorderedAccessViewFloat(THIS_
        /* [annotation] */ 
        __in  D3D11Base::ID3D11UnorderedAccessView *pUnorderedAccessView,
        /* [annotation] */ 
        __in  const FLOAT Values[ 4 ]) 
{
	GetD3D11DeviceContext()->ClearUnorderedAccessViewFloat(pUnorderedAccessView, Values);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::ClearDepthStencilView(THIS_
        /* [annotation] */ 
        __in  D3D11Base::ID3D11DepthStencilView *pDepthStencilView,
        /* [annotation] */ 
        __in  UINT ClearFlags,
        /* [annotation] */ 
        __in  FLOAT Depth,
        /* [annotation] */ 
        __in  UINT8 Stencil)
{
	GetD3D11DeviceContext()->ClearDepthStencilView(pDepthStencilView, ClearFlags, Depth, Stencil);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::GenerateMips(THIS_
        /* [annotation] */ 
        __in  D3D11Base::ID3D11ShaderResourceView *pShaderResourceView) 
{
	GetD3D11DeviceContext()->GenerateMips(pShaderResourceView);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::SetResourceMinLOD(THIS_
        /* [annotation] */ 
        __in  D3D11Base::ID3D11Resource *pResource,
        FLOAT MinLOD) 
{
	GetD3D11DeviceContext()->SetResourceMinLOD(pResource, MinLOD);
}
        
STDMETHODIMP_(FLOAT) D3D11Wrapper::ID3D11DeviceContext::GetResourceMinLOD(THIS_
        /* [annotation] */ 
        __in  D3D11Base::ID3D11Resource *pResource) 
{
	return GetD3D11DeviceContext()->GetResourceMinLOD(pResource);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::ResolveSubresource(THIS_
        /* [annotation] */ 
        __in  D3D11Base::ID3D11Resource *pDstResource,
        /* [annotation] */ 
        __in  UINT DstSubresource,
        /* [annotation] */ 
        __in  D3D11Base::ID3D11Resource *pSrcResource,
        /* [annotation] */ 
        __in  UINT SrcSubresource,
        /* [annotation] */ 
        __in  D3D11Base::DXGI_FORMAT Format) 
{
	GetD3D11DeviceContext()->ResolveSubresource(pDstResource, DstSubresource, pSrcResource, SrcSubresource,
		Format);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::ExecuteCommandList(THIS_
        /* [annotation] */ 
        __in  D3D11Base::ID3D11CommandList *pCommandList,
        BOOL RestoreContextState) 
{
	GetD3D11DeviceContext()->ExecuteCommandList(pCommandList, RestoreContextState);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::HSSetShaderResources(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
        /* [annotation] */ 
        __in_ecount(NumViews)  D3D11Base::ID3D11ShaderResourceView *const *ppShaderResourceViews) 
{
	GetD3D11DeviceContext()->HSSetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::HSSetShader(THIS_
        /* [annotation] */ 
        __in_opt  D3D11Base::ID3D11HullShader *pHullShader,
        /* [annotation] */ 
        __in_ecount_opt(NumClassInstances)  D3D11Base::ID3D11ClassInstance *const *ppClassInstances,
        UINT NumClassInstances) 
{
	if (LogFile && LogDebug) fprintf(LogFile, "ID3D11DeviceContext::HSSetShader called\n");
	
	GetD3D11DeviceContext()->HSSetShader(pHullShader, ppClassInstances, NumClassInstances);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::HSSetSamplers(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
        /* [annotation] */ 
        __in_ecount(NumSamplers)  D3D11Base::ID3D11SamplerState *const *ppSamplers) 
{
	GetD3D11DeviceContext()->HSSetSamplers(StartSlot, NumSamplers, ppSamplers);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::HSSetConstantBuffers(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
        /* [annotation] */ 
        __in_ecount(NumBuffers)  D3D11Base::ID3D11Buffer *const *ppConstantBuffers) 
{
	GetD3D11DeviceContext()->HSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::DSSetShaderResources(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
        /* [annotation] */ 
        __in_ecount(NumViews)  D3D11Base::ID3D11ShaderResourceView *const *ppShaderResourceViews) 
{
	GetD3D11DeviceContext()->DSSetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::DSSetShader(THIS_
        /* [annotation] */ 
        __in_opt  D3D11Base::ID3D11DomainShader *pDomainShader,
        /* [annotation] */ 
        __in_ecount_opt(NumClassInstances)  D3D11Base::ID3D11ClassInstance *const *ppClassInstances,
        UINT NumClassInstances) 
{
	if (LogFile && LogDebug) fprintf(LogFile, "ID3D11DeviceContext::DSSetShader called\n");
	
	GetD3D11DeviceContext()->DSSetShader(pDomainShader, ppClassInstances, NumClassInstances);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::DSSetSamplers(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
        /* [annotation] */ 
        __in_ecount(NumSamplers)  D3D11Base::ID3D11SamplerState *const *ppSamplers) 
{
	GetD3D11DeviceContext()->DSSetSamplers(StartSlot, NumSamplers, ppSamplers);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::DSSetConstantBuffers(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
        /* [annotation] */ 
        __in_ecount(NumBuffers)  D3D11Base::ID3D11Buffer *const *ppConstantBuffers) 
{
	GetD3D11DeviceContext()->DSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::CSSetShaderResources(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
        /* [annotation] */ 
        __in_ecount(NumViews)  D3D11Base::ID3D11ShaderResourceView *const *ppShaderResourceViews) 
{
	GetD3D11DeviceContext()->CSSetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::CSSetUnorderedAccessViews(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_PS_CS_UAV_REGISTER_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        __in_range( 0, D3D11_PS_CS_UAV_REGISTER_COUNT - StartSlot )  UINT NumUAVs,
        /* [annotation] */ 
        __in_ecount(NumUAVs)  D3D11Base::ID3D11UnorderedAccessView *const *ppUnorderedAccessViews,
        /* [annotation] */ 
        __in_ecount(NumUAVs)  const UINT *pUAVInitialCounts) 
{
	GetD3D11DeviceContext()->CSSetUnorderedAccessViews(StartSlot, NumUAVs, ppUnorderedAccessViews, pUAVInitialCounts);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::CSSetShader(THIS_
        /* [annotation] */ 
        __in_opt  D3D11Base::ID3D11ComputeShader *pComputeShader,
        /* [annotation] */ 
        __in_ecount_opt(NumClassInstances)  D3D11Base::ID3D11ClassInstance *const *ppClassInstances,
        UINT NumClassInstances) 
{
	if (LogFile && LogDebug) fprintf(LogFile, "ID3D11DeviceContext::CSSetShader called\n");
	
	GetD3D11DeviceContext()->CSSetShader(pComputeShader, ppClassInstances, NumClassInstances);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::CSSetSamplers(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
        /* [annotation] */ 
        __in_ecount(NumSamplers)  D3D11Base::ID3D11SamplerState *const *ppSamplers) 
{
	GetD3D11DeviceContext()->CSSetSamplers(StartSlot, NumSamplers, ppSamplers);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::CSSetConstantBuffers(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
        /* [annotation] */ 
        __in_ecount(NumBuffers)  D3D11Base::ID3D11Buffer *const *ppConstantBuffers) 
{
	GetD3D11DeviceContext()->CSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::VSGetConstantBuffers(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
        /* [annotation] */ 
        __out_ecount(NumBuffers)  D3D11Base::ID3D11Buffer **ppConstantBuffers) 
{
	GetD3D11DeviceContext()->VSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::PSGetShaderResources(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
        /* [annotation] */ 
        __out_ecount(NumViews)  D3D11Base::ID3D11ShaderResourceView **ppShaderResourceViews) 
{
	GetD3D11DeviceContext()->PSGetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::PSGetShader(THIS_
        /* [annotation] */ 
        __out  D3D11Base::ID3D11PixelShader **ppPixelShader,
        /* [annotation] */ 
        __out_ecount_opt(*pNumClassInstances)  D3D11Base::ID3D11ClassInstance **ppClassInstances,
        /* [annotation] */ 
        __inout_opt  UINT *pNumClassInstances) 
{
	GetD3D11DeviceContext()->PSGetShader(ppPixelShader, ppClassInstances, pNumClassInstances);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::PSGetSamplers(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
        /* [annotation] */ 
        __out_ecount(NumSamplers)  D3D11Base::ID3D11SamplerState **ppSamplers) 
{
	GetD3D11DeviceContext()->PSGetSamplers(StartSlot, NumSamplers, ppSamplers);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::VSGetShader(THIS_
        /* [annotation] */ 
        __out  D3D11Base::ID3D11VertexShader **ppVertexShader,
        /* [annotation] */ 
        __out_ecount_opt(*pNumClassInstances)  D3D11Base::ID3D11ClassInstance **ppClassInstances,
        /* [annotation] */ 
        __inout_opt  UINT *pNumClassInstances) 
{
	if (LogFile) fprintf(LogFile, "D3D11Wrapper::ID3D11DeviceContext::VSGetShader out: %x", ppVertexShader);

	GetD3D11DeviceContext()->VSGetShader(ppVertexShader, ppClassInstances, pNumClassInstances);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::PSGetConstantBuffers(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
        /* [annotation] */ 
        __out_ecount(NumBuffers)  D3D11Base::ID3D11Buffer **ppConstantBuffers) 
{
	GetD3D11DeviceContext()->PSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::IAGetInputLayout(THIS_
        /* [annotation] */ 
        __out  D3D11Base::ID3D11InputLayout **ppInputLayout) 
{
	GetD3D11DeviceContext()->IAGetInputLayout(ppInputLayout);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::IAGetVertexBuffers(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        __in_range( 0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumBuffers,
        /* [annotation] */ 
        __out_ecount_opt(NumBuffers)  D3D11Base::ID3D11Buffer **ppVertexBuffers,
        /* [annotation] */ 
        __out_ecount_opt(NumBuffers)  UINT *pStrides,
        /* [annotation] */ 
        __out_ecount_opt(NumBuffers)  UINT *pOffsets) 
{
	GetD3D11DeviceContext()->IAGetVertexBuffers(StartSlot, NumBuffers, ppVertexBuffers, pStrides, pOffsets);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::IAGetIndexBuffer(THIS_
        /* [annotation] */ 
        __out_opt  D3D11Base::ID3D11Buffer **pIndexBuffer,
        /* [annotation] */ 
        __out_opt  D3D11Base::DXGI_FORMAT *Format,
        /* [annotation] */ 
        __out_opt  UINT *Offset) 
{
	GetD3D11DeviceContext()->IAGetIndexBuffer(pIndexBuffer, Format, Offset);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::GSGetConstantBuffers(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
        /* [annotation] */ 
        __out_ecount(NumBuffers)  D3D11Base::ID3D11Buffer **ppConstantBuffers) 
{
	GetD3D11DeviceContext()->GSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::GSGetShader(THIS_
        /* [annotation] */ 
        __out  D3D11Base::ID3D11GeometryShader **ppGeometryShader,
        /* [annotation] */ 
        __out_ecount_opt(*pNumClassInstances)  D3D11Base::ID3D11ClassInstance **ppClassInstances,
        /* [annotation] */ 
        __inout_opt  UINT *pNumClassInstances) 
{
	GetD3D11DeviceContext()->GSGetShader(ppGeometryShader, ppClassInstances, pNumClassInstances);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::IAGetPrimitiveTopology(THIS_
        /* [annotation] */ 
        __out  D3D11Base::D3D11_PRIMITIVE_TOPOLOGY *pTopology) 
{
	GetD3D11DeviceContext()->IAGetPrimitiveTopology(pTopology);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::VSGetShaderResources(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
        /* [annotation] */ 
        __out_ecount(NumViews)  D3D11Base::ID3D11ShaderResourceView **ppShaderResourceViews) 
{
	GetD3D11DeviceContext()->VSGetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::VSGetSamplers(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
        /* [annotation] */ 
        __out_ecount(NumSamplers)  D3D11Base::ID3D11SamplerState **ppSamplers) 
{
	GetD3D11DeviceContext()->VSGetSamplers(StartSlot, NumSamplers, ppSamplers);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::GetPredication(THIS_
        /* [annotation] */ 
        __out_opt  D3D11Base::ID3D11Predicate **ppPredicate,
        /* [annotation] */ 
        __out_opt  BOOL *pPredicateValue) 
{
	GetD3D11DeviceContext()->GetPredication(ppPredicate, pPredicateValue);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::GSGetShaderResources(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
        /* [annotation] */ 
        __out_ecount(NumViews)  D3D11Base::ID3D11ShaderResourceView **ppShaderResourceViews) 
{
	GetD3D11DeviceContext()->GSGetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::GSGetSamplers(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
        /* [annotation] */ 
        __out_ecount(NumSamplers)  D3D11Base::ID3D11SamplerState **ppSamplers) 
{
	GetD3D11DeviceContext()->GSGetSamplers(StartSlot, NumSamplers, ppSamplers);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::OMGetRenderTargets(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT )  UINT NumViews,
        /* [annotation] */ 
        __out_ecount_opt(NumViews)  D3D11Base::ID3D11RenderTargetView **ppRenderTargetViews,
        /* [annotation] */ 
        __out_opt  D3D11Base::ID3D11DepthStencilView **ppDepthStencilView)
{
	GetD3D11DeviceContext()->OMGetRenderTargets(NumViews, ppRenderTargetViews, ppDepthStencilView);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::OMGetRenderTargetsAndUnorderedAccessViews(THIS_
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
        __out_ecount_opt(NumUAVs)  D3D11Base::ID3D11UnorderedAccessView **ppUnorderedAccessViews) 
{
	GetD3D11DeviceContext()->OMGetRenderTargetsAndUnorderedAccessViews(NumRTVs, ppRenderTargetViews, ppDepthStencilView,
		UAVStartSlot, NumUAVs, ppUnorderedAccessViews);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::OMGetBlendState(THIS_
        /* [annotation] */ 
        __out_opt  D3D11Base::ID3D11BlendState **ppBlendState,
        /* [annotation] */ 
        __out_opt  FLOAT BlendFactor[ 4 ],
        /* [annotation] */ 
        __out_opt  UINT *pSampleMask) 
{
	GetD3D11DeviceContext()->OMGetBlendState(ppBlendState, BlendFactor, pSampleMask);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::OMGetDepthStencilState(THIS_
        /* [annotation] */ 
        __out_opt  D3D11Base::ID3D11DepthStencilState **ppDepthStencilState,
        /* [annotation] */ 
        __out_opt  UINT *pStencilRef) 
{
	GetD3D11DeviceContext()->OMGetDepthStencilState(ppDepthStencilState, pStencilRef);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::SOGetTargets(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_SO_BUFFER_SLOT_COUNT )  UINT NumBuffers,
        /* [annotation] */ 
        __out_ecount(NumBuffers)  D3D11Base::ID3D11Buffer **ppSOTargets) 
{
	GetD3D11DeviceContext()->SOGetTargets(NumBuffers, ppSOTargets);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::RSGetState(THIS_
        /* [annotation] */ 
        __out  D3D11Base::ID3D11RasterizerState **ppRasterizerState) 
{
	GetD3D11DeviceContext()->RSGetState(ppRasterizerState);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::RSGetViewports(THIS_
        /* [annotation] */ 
        __inout /*_range(0, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE )*/   UINT *pNumViewports,
        /* [annotation] */ 
        __out_ecount_opt(*pNumViewports)  D3D11Base::D3D11_VIEWPORT *pViewports) 
{
	GetD3D11DeviceContext()->RSGetViewports(pNumViewports, pViewports);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::RSGetScissorRects(THIS_
        /* [annotation] */ 
        __inout /*_range(0, D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE )*/   UINT *pNumRects,
        /* [annotation] */ 
        __out_ecount_opt(*pNumRects)  D3D11Base::D3D11_RECT *pRects) 
{
	GetD3D11DeviceContext()->RSGetScissorRects(pNumRects, pRects);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::HSGetShaderResources(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
        /* [annotation] */ 
        __out_ecount(NumViews)  D3D11Base::ID3D11ShaderResourceView **ppShaderResourceViews) 
{
	GetD3D11DeviceContext()->HSGetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::HSGetShader(THIS_
        /* [annotation] */ 
        __out  D3D11Base::ID3D11HullShader **ppHullShader,
        /* [annotation] */ 
        __out_ecount_opt(*pNumClassInstances)  D3D11Base::ID3D11ClassInstance **ppClassInstances,
        /* [annotation] */ 
        __inout_opt  UINT *pNumClassInstances) 
{
	GetD3D11DeviceContext()->HSGetShader(ppHullShader, ppClassInstances, pNumClassInstances);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::HSGetSamplers(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
        /* [annotation] */ 
        __out_ecount(NumSamplers)  D3D11Base::ID3D11SamplerState **ppSamplers) 
{
	GetD3D11DeviceContext()->HSGetSamplers(StartSlot, NumSamplers, ppSamplers);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::HSGetConstantBuffers(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
        /* [annotation] */ 
        __out_ecount(NumBuffers)  D3D11Base::ID3D11Buffer **ppConstantBuffers) 
{
	GetD3D11DeviceContext()->HSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::DSGetShaderResources(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
        /* [annotation] */ 
        __out_ecount(NumViews)  D3D11Base::ID3D11ShaderResourceView **ppShaderResourceViews) 
{
	GetD3D11DeviceContext()->DSGetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::DSGetShader(THIS_
        /* [annotation] */ 
        __out  D3D11Base::ID3D11DomainShader **ppDomainShader,
        /* [annotation] */ 
        __out_ecount_opt(*pNumClassInstances)  D3D11Base::ID3D11ClassInstance **ppClassInstances,
        /* [annotation] */ 
        __inout_opt  UINT *pNumClassInstances) 
{
	GetD3D11DeviceContext()->DSGetShader(ppDomainShader, ppClassInstances, pNumClassInstances);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::DSGetSamplers(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
        /* [annotation] */ 
        __out_ecount(NumSamplers)  D3D11Base::ID3D11SamplerState **ppSamplers) 
{
	GetD3D11DeviceContext()->DSGetSamplers(StartSlot, NumSamplers, ppSamplers);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::DSGetConstantBuffers(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
        /* [annotation] */ 
        __out_ecount(NumBuffers)  D3D11Base::ID3D11Buffer **ppConstantBuffers) 
{
	GetD3D11DeviceContext()->DSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::CSGetShaderResources(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
        /* [annotation] */ 
        __out_ecount(NumViews)  D3D11Base::ID3D11ShaderResourceView **ppShaderResourceViews) 
{
	GetD3D11DeviceContext()->CSGetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::CSGetUnorderedAccessViews(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_PS_CS_UAV_REGISTER_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        __in_range( 0, D3D11_PS_CS_UAV_REGISTER_COUNT - StartSlot )  UINT NumUAVs,
        /* [annotation] */ 
        __out_ecount(NumUAVs)  D3D11Base::ID3D11UnorderedAccessView **ppUnorderedAccessViews) 
{
	GetD3D11DeviceContext()->CSGetUnorderedAccessViews(StartSlot, NumUAVs, ppUnorderedAccessViews);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::CSGetShader(THIS_
        /* [annotation] */ 
        __out  D3D11Base::ID3D11ComputeShader **ppComputeShader,
        /* [annotation] */ 
        __out_ecount_opt(*pNumClassInstances)  D3D11Base::ID3D11ClassInstance **ppClassInstances,
        /* [annotation] */ 
        __inout_opt  UINT *pNumClassInstances) 
{
	GetD3D11DeviceContext()->CSGetShader(ppComputeShader, ppClassInstances, pNumClassInstances);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::CSGetSamplers(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
        /* [annotation] */ 
        __out_ecount(NumSamplers)  D3D11Base::ID3D11SamplerState **ppSamplers) 
{
	GetD3D11DeviceContext()->CSGetSamplers(StartSlot, NumSamplers, ppSamplers);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::CSGetConstantBuffers(THIS_
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
        /* [annotation] */ 
        __in_range( 0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
        /* [annotation] */ 
        __out_ecount(NumBuffers)  D3D11Base::ID3D11Buffer **ppConstantBuffers) 
{
	GetD3D11DeviceContext()->CSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::ClearState(THIS)
{
	GetD3D11DeviceContext()->ClearState();
}
        
STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::Flush(THIS)
{
	GetD3D11DeviceContext()->Flush();
}
        
STDMETHODIMP_(D3D11Base::D3D11_DEVICE_CONTEXT_TYPE) D3D11Wrapper::ID3D11DeviceContext::GetType(THIS)
{
	return GetD3D11DeviceContext()->GetType();
}
        
STDMETHODIMP_(UINT) D3D11Wrapper::ID3D11DeviceContext::GetContextFlags(THIS)
{
	return GetD3D11DeviceContext()->GetContextFlags();
}
        
STDMETHODIMP D3D11Wrapper::ID3D11DeviceContext::FinishCommandList(THIS_ 
        BOOL RestoreDeferredContextState,
        /* [annotation] */ 
        __out_opt  D3D11Base::ID3D11CommandList **ppCommandList)
{
	return GetD3D11DeviceContext()->FinishCommandList(RestoreDeferredContextState, ppCommandList);
}
