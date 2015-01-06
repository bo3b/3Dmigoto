
D3D11Wrapper::ID3D11DeviceContext::ID3D11DeviceContext(D3D11Base::ID3D11DeviceContext *pContext)
	: D3D11Wrapper::IDirect3DUnknown((IUnknown*)pContext)
{
}

D3D11Wrapper::ID3D11DeviceContext* D3D11Wrapper::ID3D11DeviceContext::GetDirect3DDeviceContext(D3D11Base::ID3D11DeviceContext *pOrig)
{
	D3D11Wrapper::ID3D11DeviceContext* p = (D3D11Wrapper::ID3D11DeviceContext*) m_List.GetDataPtr(pOrig);
	if (!p)
	{
		p = new D3D11Wrapper::ID3D11DeviceContext(pOrig);
		if (pOrig) m_List.AddMember(pOrig, p);
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
 	LogDebug("ID3D11DeviceContext::Release handle=%p, counter=%d, this=%p\n", m_pUnk, m_ulRef, this);

	ULONG ulRef = m_pUnk ? m_pUnk->Release() : 0;
	LogDebug("  internal counter = %d\n", ulRef);

	--m_ulRef;

	if (ulRef <= 0)
	{
		LogDebug("  deleting self\n");

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
		LogInfo("ID3D11DeviceContext::GetDevice called");
		LogInfo("  can't find wrapper for parent device. Returning original device handle = %p\n", origDevice);

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
	__out_bcount_opt(*pDataSize)  void *pData)
{
	LogInfo("ID3D11DeviceContext::GetPrivateData called with GUID = %08lx-%04hx-%04hx-%02hhx%02hhx-%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx\n",
		guid.Data1, guid.Data2, guid.Data3, guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
		guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);

	HRESULT hr = GetD3D11DeviceContext()->GetPrivateData(guid, pDataSize, pData);
	LogInfo("  returns result = %x, DataSize = %d\n", hr, *pDataSize);

	return hr;
}

STDMETHODIMP D3D11Wrapper::ID3D11DeviceContext::SetPrivateData(THIS_
	/* [annotation] */
	__in  REFGUID guid,
	/* [annotation] */
	__in  UINT DataSize,
	/* [annotation] */
	__in_bcount_opt(DataSize)  const void *pData)
{
	LogInfo("ID3D11DeviceContext::SetPrivateData called with GUID = %08lx-%04hx-%04hx-%02hhx%02hhx-%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx\n",
		guid.Data1, guid.Data2, guid.Data3, guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
		guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
	LogInfo("  DataSize = %d\n", DataSize);

	HRESULT hr = GetD3D11DeviceContext()->SetPrivateData(guid, DataSize, pData);
	LogInfo("  returns result = %x\n", hr);

	return hr;
}

STDMETHODIMP D3D11Wrapper::ID3D11DeviceContext::SetPrivateDataInterface(THIS_
	/* [annotation] */
	__in  REFGUID guid,
	/* [annotation] */
	__in_opt  const IUnknown *pData)
{
	LogInfo("ID3D11DeviceContext::SetPrivateDataInterface called with GUID=%08lx-%04hx-%04hx-%02hhx%02hhx-%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx\n",
		guid.Data1, guid.Data2, guid.Data3, guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
		guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);

	HRESULT hr = GetD3D11DeviceContext()->SetPrivateDataInterface(guid, pData);
	LogInfo("  returns result = %x\n", hr);

	return hr;
}

STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::VSSetConstantBuffers(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
	/* [annotation] */
	__in_ecount(NumBuffers) D3D11Base::ID3D11Buffer *const *ppConstantBuffers)
{
	GetD3D11DeviceContext()->VSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}

UINT64 D3D11Wrapper::CalcTexture2DDescHash(const D3D11Base::D3D11_TEXTURE2D_DESC *desc,
		UINT64 initial_hash, int override_width, int override_height)
{
	UINT64 hash = initial_hash;

	// It concerns me that CreateTextureND can use an override if it
	// matches screen resolution, but when we record render target / shader
	// resource stats we don't use the same override.
	//
	// For textures made with CreateTextureND and later used as a render
	// target it's probably fine since the hash will still be stored, but
	// it could be a problem if we need the hash of a render target not
	// created directly with that. I don't know enough about the DX11 API
	// to know if this is an issue, but it might be worth using the screen
	// resolution override in all cases. -DarkStarSword
	if (override_width)
		hash ^= override_width;
	else
		hash ^= desc->Width;
	hash *= FNV_64_PRIME;

	if (override_height)
		hash ^= override_height;
	else
		hash ^= desc->Height;
	hash *= FNV_64_PRIME;

	hash ^= desc->MipLevels; hash *= FNV_64_PRIME;
	hash ^= desc->ArraySize; hash *= FNV_64_PRIME;
	hash ^= desc->Format; hash *= FNV_64_PRIME;
	hash ^= desc->SampleDesc.Count;
	hash ^= desc->SampleDesc.Quality;
	hash ^= desc->Usage; hash *= FNV_64_PRIME;
	hash ^= desc->BindFlags; hash *= FNV_64_PRIME;
	hash ^= desc->CPUAccessFlags; hash *= FNV_64_PRIME;
	hash ^= desc->MiscFlags;

	return hash;
}

UINT64 D3D11Wrapper::CalcTexture3DDescHash(const D3D11Base::D3D11_TEXTURE3D_DESC *desc,
		UINT64 initial_hash, int override_width, int override_height)
{
	UINT64 hash = initial_hash;

	// Same comment as in CalcTexture2DDescHash above - concerned about
	// inconsistent use of these resolution overrides
	if (override_width)
		hash ^= override_width;
	else
		hash ^= desc->Width;
	hash *= FNV_64_PRIME;

	if (override_height)
		hash ^= override_height;
	else
		hash ^= desc->Height;
	hash *= FNV_64_PRIME;

	hash ^= desc->Depth; hash *= FNV_64_PRIME;
	hash ^= desc->MipLevels; hash *= FNV_64_PRIME;
	hash ^= desc->Format; hash *= FNV_64_PRIME;
	hash ^= desc->Usage; hash *= FNV_64_PRIME;
	hash ^= desc->BindFlags; hash *= FNV_64_PRIME;
	hash ^= desc->CPUAccessFlags; hash *= FNV_64_PRIME;
	hash ^= desc->MiscFlags;

	return hash;
}

static UINT64 GetTexture2DHash(D3D11Base::ID3D11Texture2D *texture,
		bool log_new, struct ResourceInfo *resource_info)
{

	D3D11Base::D3D11_TEXTURE2D_DESC desc;
	std::map<D3D11Base::ID3D11Texture2D *, UINT64>::iterator j;

	texture->GetDesc(&desc);

	if (resource_info)
		*resource_info = desc;

	j = G->mTexture2D_ID.find(texture);
	if (j != G->mTexture2D_ID.end())
		return j->second;

	if (log_new) {
		// TODO: Refactor with LogRenderTarget()
		LogDebug("    Unknown render target:\n");
		LogDebug("    Width = %d, Height = %d, MipLevels = %d, ArraySize = %d\n",
				desc.Width, desc.Height, desc.MipLevels, desc.ArraySize);
		LogDebug("    Format = %d, Usage = %x, BindFlags = %x, CPUAccessFlags = %x, MiscFlags = %x\n",
				desc.Format, desc.Usage, desc.BindFlags, desc.CPUAccessFlags, desc.MiscFlags);
	}

	return D3D11Wrapper::CalcTexture2DDescHash(&desc, 0, 0, 0);
}

static UINT64 GetTexture3DHash(D3D11Base::ID3D11Texture3D *texture,
		bool log_new, struct ResourceInfo *resource_info)
{

	D3D11Base::D3D11_TEXTURE3D_DESC desc;
	std::map<D3D11Base::ID3D11Texture3D *, UINT64>::iterator j;

	texture->GetDesc(&desc);

	if (resource_info)
		*resource_info = desc;

	j = G->mTexture3D_ID.find(texture);
	if (j != G->mTexture3D_ID.end())
		return j->second;

	if (log_new) {
		// TODO: Refactor with LogRenderTarget()
		LogDebug("    Unknown 3D render target:\n");
		LogDebug("    Width = %d, Height = %d, MipLevels = %d\n",
				desc.Width, desc.Height, desc.MipLevels);
		LogDebug("    Format = %d, Usage = %x, BindFlags = %x, CPUAccessFlags = %x, MiscFlags = %x\n",
				desc.Format, desc.Usage, desc.BindFlags, desc.CPUAccessFlags, desc.MiscFlags);
	}

	return D3D11Wrapper::CalcTexture3DDescHash(&desc, 0, 0, 0);
}

// Records the hash of this shader resource view for later lookup. Returns the
// handle to the resource, but be aware that it no longer has a reference and
// should only be used for map lookups.
static void *RecordResourceViewStats(D3D11Base::ID3D11ShaderResourceView *view)
{
	D3D11Base::D3D11_SHADER_RESOURCE_VIEW_DESC desc;
	D3D11Base::ID3D11Resource *resource = NULL;
	UINT64 hash = 0;

	if (!view)
		return NULL;

	view->GetResource(&resource);
	if (!resource)
		return NULL;

	view->GetDesc(&desc);

	switch (desc.ViewDimension) {
		case D3D11Base::D3D11_SRV_DIMENSION_TEXTURE2D:
		case D3D11Base::D3D11_SRV_DIMENSION_TEXTURE2DMS:
		case D3D11Base::D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY:
			hash = GetTexture2DHash((D3D11Base::ID3D11Texture2D *)resource, false, NULL);
			break;
		case D3D11Base::D3D11_SRV_DIMENSION_TEXTURE3D:
			hash = GetTexture3DHash((D3D11Base::ID3D11Texture3D *)resource, false, NULL);
			break;
	}

	resource->Release();

	if (hash)
		G->mRenderTargets[resource] = hash;

	return resource;
}

static void RecordShaderResourceUsage(D3D11Wrapper::ID3D11DeviceContext *context)
{
	D3D11Base::ID3D11ShaderResourceView *ps_views[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
	D3D11Base::ID3D11ShaderResourceView *vs_views[D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
	void *resource;
	int i;

	context->PSGetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, ps_views);
	context->VSGetShaderResources(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, vs_views);

	for (i = 0; i < D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT; i++) {
		resource = RecordResourceViewStats(ps_views[i]);
		if (resource) {
			// FIXME: Don't clobber these - it would be useful to
			// collect a set of all seen resources, e.g. for
			// matching all textures used by a shader.
			G->mPixelShaderInfo[G->mCurrentPixelShader].ResourceRegisters[i] = resource;
		}

		resource = RecordResourceViewStats(vs_views[i]);
		if (resource) {
			// FIXME: Don't clobber these - it would be useful to
			// collect a set of all seen resources, e.g. for
			// matching all textures used by a shader.
			G->mVertexShaderInfo[G->mCurrentVertexShader].ResourceRegisters[i] = resource;
		}
	}
}

static void RecordRenderTargetInfo(D3D11Base::ID3D11RenderTargetView *target, UINT view_num)
{
	D3D11Base::D3D11_RENDER_TARGET_VIEW_DESC desc;
	D3D11Base::ID3D11Resource *resource = NULL;
	struct ResourceInfo resource_info;
	UINT64 hash = 0;

	if (!target)
		return;

	target->GetDesc(&desc);

	LogDebug("  View #%d, Format = %d, Is2D = %d\n",
			view_num, desc.Format, D3D11Base::D3D11_RTV_DIMENSION_TEXTURE2D == desc.ViewDimension);

	switch(desc.ViewDimension) {
		case D3D11Base::D3D11_RTV_DIMENSION_TEXTURE2D:
		case D3D11Base::D3D11_RTV_DIMENSION_TEXTURE2DMS:
			target->GetResource(&resource);
			if (!resource)
				return;
			hash = GetTexture2DHash((D3D11Base::ID3D11Texture2D *)resource,
					LogDebug, &resource_info);
			resource->Release();
			break;
		case D3D11Base::D3D11_RTV_DIMENSION_TEXTURE3D:
			target->GetResource(&resource);
			if (!resource)
				return;
			hash = GetTexture3DHash((D3D11Base::ID3D11Texture3D *)resource,
					LogDebug, &resource_info);
			resource->Release();
			break;
	}

	if (!resource)
		return;

	if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
	G->mRenderTargets[resource] = hash;
	G->mCurrentRenderTargets.push_back(resource);
	G->mVisitedRenderTargets.insert(resource);
	G->mRenderTargetInfo[hash] = resource_info;
	if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
}

static void RecordDepthStencil(D3D11Base::ID3D11DepthStencilView *target)
{
	D3D11Base::D3D11_DEPTH_STENCIL_VIEW_DESC desc;
	D3D11Base::ID3D11Resource *resource = NULL;
	UINT64 hash = 0;

	if (!target)
		return;

	target->GetResource(&resource);
	if (!resource)
		return;

	target->GetDesc(&desc);

	switch(desc.ViewDimension) {
		// TODO: Is it worth recording the type of 2D texture view?
		// TODO: Maybe for array variants, record all resources in array?
		case D3D11Base::D3D11_DSV_DIMENSION_TEXTURE2D:
		case D3D11Base::D3D11_DSV_DIMENSION_TEXTURE2DARRAY:
		case D3D11Base::D3D11_DSV_DIMENSION_TEXTURE2DMS:
		case D3D11Base::D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY:
			hash = GetTexture2DHash((D3D11Base::ID3D11Texture2D *)resource, false, NULL);
			break;
	}

	resource->Release();

	if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
	G->mRenderTargets[resource] = hash;
	G->mCurrentDepthTarget = resource;
	if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
}

STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::PSSetShaderResources(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
	/* [annotation] */
	__in_ecount(NumViews) D3D11Base::ID3D11ShaderResourceView *const *ppShaderResourceViews)
{
	LogDebug("ID3D11DeviceContext::PSSetShaderResources called with StartSlot = %d, NumViews = %d\n", StartSlot, NumViews);
	if (ppShaderResourceViews && NumViews) LogDebug("  ShaderResourceView[0] handle = %p\n", *ppShaderResourceViews);

	GetD3D11DeviceContext()->PSSetShaderResources(StartSlot, NumViews, ppShaderResourceViews);

	// Resolve resource from resource view.
	// This is possibly no longer required as we collect stats on draw calls
	if (G->hunting && ppShaderResourceViews)
	{
		if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
		for (UINT i = 0; i < NumViews; ++i)
		{
			void *pResource;
			int pos = StartSlot + i;

			pResource = RecordResourceViewStats(ppShaderResourceViews[i]);
			if (pResource) {
				// FIXME: Don't clobber these - it would be useful to
				// collect a set of all seen resources, e.g. for
				// matching all textures used by a shader.
				G->mPixelShaderInfo[G->mCurrentPixelShader].ResourceRegisters[pos] = pResource;
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
	LogDebug("  adding NVidia stereo parameter texture to shader resources in slot 125.\n");

	m_pContext->PSSetShaderResources(125, 1, &device->mStereoResourceView);
	}
	else
	{
	LogInfo("  error querying device. Can't set NVidia stereo parameter texture.\n");
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
	LogDebug("ID3D11DeviceContext::PSSetShader called with pixelshader handle = %p\n", pPixelShader);

	bool patchedShader = false;
	if (G->hunting && pPixelShader)
	{
		// Store as current pixel shader.
		if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
		PixelShaderMap::iterator i = G->mPixelShaders.find(pPixelShader);
		if (i != G->mPixelShaders.end())
		{
			G->mCurrentPixelShader = i->second;
			LogDebug("  pixel shader found: handle = %p, hash = %08lx%08lx\n", pPixelShader, (UINT32)(G->mCurrentPixelShader >> 32), (UINT32)G->mCurrentPixelShader);

			// Add to visited pixel shaders.
			G->mVisitedPixelShaders.insert(i->second);
			patchedShader = true;

			// second try to hide index buffer.
			// if (mCurrentIndexBuffer == mSelectedIndexBuffer)
			//	pIndexBuffer = 0;
		}
		else
		{
			LogDebug("  pixel shader %p not found\n", pPixelShader);
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
		if (it != G->mReloadedShaders.end() && it->second.replacement != NULL)
		{
			LogDebug("  pixel shader replaced by: %p\n", it->second.replacement);

			// Todo: It might make sense to Release() the original shader, to recover memory on GPU
			D3D11Base::ID3D11PixelShader *shader = (D3D11Base::ID3D11PixelShader*) it->second.replacement;
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
				LogDebug("  adding NVidia stereo parameter texture to shader resources in slot 125.\n");

				GetD3D11DeviceContext()->PSSetShaderResources(125, 1, &device->mStereoResourceView);
			}
			// Set constants from ini file if they exist
			if (device->mIniResourceView)
			{
				LogDebug("  adding ini constants as texture to shader resources in slot 120.\n");

				GetD3D11DeviceContext()->PSSetShaderResources(120, 1, &device->mIniResourceView);
			}
			// Set custom depth texture.
			if (device->mZBufferResourceView)
			{
				LogDebug("  adding Z buffer to shader resources in slot 126.\n");

				GetD3D11DeviceContext()->PSSetShaderResources(126, 1, &device->mZBufferResourceView);
			}
			device->Release();
		}
		else
		{
			LogInfo("  error querying device. Can't set NVidia stereo parameter texture.\n");
		}
	}
}

STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::PSSetSamplers(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
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
	float separationValue;

	// Skip?
	data.override = false;
	data.skip = G->mBlockingMode;

	// If we are not hunting shaders, we should skip all of this shader management for a performance bump.
	if (G->hunting)
	{
		UINT selectedRenderTargetPos;
		if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
		{
			// Stats
			if (G->mCurrentVertexShader && G->mCurrentPixelShader)
			{
				G->mVertexShaderInfo[G->mCurrentVertexShader].PartnerShader.insert(G->mCurrentPixelShader);
				G->mPixelShaderInfo[G->mCurrentPixelShader].PartnerShader.insert(G->mCurrentVertexShader);
			}
			if (G->mCurrentPixelShader) {
				// FIXME: Don't clobber this - a shader may be used with different render targets at different times
				G->mPixelShaderInfo[G->mCurrentPixelShader].RenderTargets = G->mCurrentRenderTargets;
				if (G->mCurrentDepthTarget)
					G->mPixelShaderInfo[G->mCurrentPixelShader].DepthTargets.insert(G->mCurrentDepthTarget);
			}

			// Maybe make this optional if it turns out to have a
			// significant performance impact:
			RecordShaderResourceUsage(context);

			// Selection
			for (selectedRenderTargetPos = 0; selectedRenderTargetPos < G->mCurrentRenderTargets.size(); ++selectedRenderTargetPos)
				if (G->mCurrentRenderTargets[selectedRenderTargetPos] == G->mSelectedRenderTarget) break;
			if (G->mCurrentIndexBuffer == G->mSelectedIndexBuffer ||
				G->mCurrentVertexShader == G->mSelectedVertexShader ||
				G->mCurrentPixelShader == G->mSelectedPixelShader ||
				selectedRenderTargetPos < G->mCurrentRenderTargets.size())
			{
				LogDebug("  Skipping selected operation. CurrentIndexBuffer = %08lx%08lx, CurrentVertexShader = %08lx%08lx, CurrentPixelShader = %08lx%08lx\n",
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
				}
			}
		}
		if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
	}

	// Override settings?
	ShaderSeparationMap::iterator i = G->mShaderSeparationMap.find(G->mCurrentVertexShader);
	if (i == G->mShaderSeparationMap.end()) i = G->mShaderSeparationMap.find(G->mCurrentPixelShader);
	if (i != G->mShaderSeparationMap.end())
	{
		LogDebug("  seperation override found for shader\n");

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
			LogDebug("  current iteration = %d\n", currentIteration);

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
				LogDebug("  override skipped\n");
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

	if (data.override)
	{
		D3D11Wrapper::ID3D11Device *device;
		context->GetDevice(&device);
		if (device->mStereoHandle)
		{
			LogDebug("  setting custom separation value\n");

			if (D3D11Base::NVAPI_OK != D3D11Base::NvAPI_Stereo_GetSeparation(device->mStereoHandle, &data.oldSeparation))
			{
				LogDebug("    Stereo_GetSeparation failed.\n");
			}
			NvAPIOverride();
			if (D3D11Base::NVAPI_OK != D3D11Base::NvAPI_Stereo_SetSeparation(device->mStereoHandle, separationValue * data.oldSeparation))
			{
				LogDebug("    Stereo_SetSeparation failed.\n");
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
				LogDebug("    Stereo_SetSeparation failed.\n");
			}
		}
		device->Release();
	}

	// When in hunting mode, we need to get time to run the UI for stepping through shaders.
	// This gets called for every Draw, and is a definitely overkill, but is a convenient spot
	// where we are absolutely certain that everyone is set up correctly.  And where we can
	// get the original ID3D11Device.  This used to be done through the DXGI Present interface,
	// but that had a number of problems.
	D3D11Wrapper::ID3D11Device *device;
	context->GetDevice(&device);
	RunFrameActions((D3D11Base::ID3D11Device *)device->m_pUnk);
	device->Release();
}

STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::VSSetShader(THIS_
	/* [annotation] */
	__in_opt D3D11Base::ID3D11VertexShader *pVertexShader,
	/* [annotation] */
	__in_ecount_opt(NumClassInstances) D3D11Base::ID3D11ClassInstance *const *ppClassInstances,
	UINT NumClassInstances)
{
	LogDebug("ID3D11DeviceContext::VSSetShader called with vertexshader handle = %p\n", pVertexShader);

	bool patchedShader = false;
	if (G->hunting && pVertexShader)
	{
		// Store as current vertex shader.
		if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
		VertexShaderMap::iterator i = G->mVertexShaders.find(pVertexShader);
		if (i != G->mVertexShaders.end())
		{
			G->mCurrentVertexShader = i->second;
			LogDebug("  vertex shader found: handle = %p, hash = %08lx%08lx\n", pVertexShader, (UINT32)(G->mCurrentVertexShader >> 32), (UINT32)G->mCurrentVertexShader);

			// Add to visited vertex shaders.
			G->mVisitedVertexShaders.insert(i->second);
			patchedShader = true;

			// second try to hide index buffer.
			// if (mCurrentIndexBuffer == mSelectedIndexBuffer)
			//	pIndexBuffer = 0;
		}
		else
		{
			LogDebug("  vertex shader %p not found\n", pVertexShader);
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
		if (it != G->mReloadedShaders.end() && it->second.replacement != NULL)
		{
			LogDebug("  vertex shader replaced by: %p\n", it->second.replacement);

			D3D11Base::ID3D11VertexShader *shader = (D3D11Base::ID3D11VertexShader*) it->second.replacement;
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
				LogDebug("  adding NVidia stereo parameter texture to shader resources in slot 125.\n");

				GetD3D11DeviceContext()->VSSetShaderResources(125, 1, &device->mStereoResourceView);
			}

			// Set constants from ini file if they exist
			if (device->mIniResourceView)
			{
				LogDebug("  adding ini constants as texture to shader resources in slot 120.\n");

				GetD3D11DeviceContext()->VSSetShaderResources(120, 1, &device->mIniResourceView);
			}
			device->Release();
		}
		else
		{
			LogInfo("  error querying device. Can't set NVidia stereo parameter texture.\n");
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
	LogDebug("ID3D11DeviceContext::DrawIndexed called with IndexCount = %d, StartIndexLocation = %d, BaseVertexLocation = %d\n",
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
	LogDebug("ID3D11DeviceContext::Draw called with VertexCount = %d, StartVertexLocation = %d\n",
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
	__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
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
	__in_range(0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumBuffers,
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
	LogDebug("ID3D11DeviceContext::IASetIndexBuffer called\n");

	if (G->hunting && pIndexBuffer)
	{
		// Store as current index buffer.
		if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
		DataBufferMap::iterator i = G->mDataBuffers.find(pIndexBuffer);
		if (i != G->mDataBuffers.end())
		{
			G->mCurrentIndexBuffer = i->second;
			LogDebug("  index buffer found: handle = %p, hash = %08lx%08lx\n", pIndexBuffer, (UINT32)(G->mCurrentIndexBuffer >> 32), (UINT32)G->mCurrentIndexBuffer);

			// Add to visited index buffers.
			G->mVisitedIndexBuffers.insert(G->mCurrentIndexBuffer);

			// second try to hide index buffer.
			// if (mCurrentIndexBuffer == mSelectedIndexBuffer)
			//	pIndexBuffer = 0;
		}
		else LogDebug("  index buffer %p not found\n", pIndexBuffer);
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
	LogDebug("ID3D11DeviceContext::DrawIndexedInstanced called with IndexCountPerInstance = %d, InstanceCount = %d\n",
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
	LogDebug("ID3D11DeviceContext::DrawInstanced called with VertexCountPerInstance = %d, InstanceCount = %d\n",
		VertexCountPerInstance, InstanceCount);

	DrawContext c = BeforeDraw(this);
	if (!c.skip)
		GetD3D11DeviceContext()->DrawInstanced(VertexCountPerInstance, InstanceCount, StartVertexLocation, StartInstanceLocation);
	AfterDraw(c, this);
}

STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::GSSetConstantBuffers(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
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
	__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
	/* [annotation] */
	__in_ecount(NumViews) D3D11Base::ID3D11ShaderResourceView *const *ppShaderResourceViews)
{
	LogDebug("ID3D11DeviceContext::VSSetShaderResources called with StartSlot = %d, NumViews = %d\n", StartSlot, NumViews);
	if (ppShaderResourceViews && NumViews) LogDebug("  ShaderResourceView[0] handle = %p\n", *ppShaderResourceViews);

	GetD3D11DeviceContext()->VSSetShaderResources(StartSlot, NumViews, ppShaderResourceViews);

	// Resolve resource from resource view.
	// This is possibly no longer required as we collect stats on draw calls
	if (G->hunting && ppShaderResourceViews)
	{
		if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
		for (UINT i = 0; i < NumViews; ++i)
		{
			void *pResource;
			int pos = StartSlot + i;

			pResource = RecordResourceViewStats(ppShaderResourceViews[i]);
			if (pResource) {
				// FIXME: Don't clobber these - it would be useful to
				// collect a set of all seen resources, e.g. for
				// matching all textures used by a shader.
				G->mVertexShaderInfo[G->mCurrentVertexShader].ResourceRegisters[pos] = pResource;
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
	LogDebug("  adding NVidia stereo parameter texture to shader resources in slot 125.\n");

	m_pContext->VSSetShaderResources(125, 1, &device->mStereoResourceView);
	}
	else
	{
	LogInfo("  error querying device. Can't set NVidia stereo parameter texture.\n");
	}
	device->Release();
	}
	*/
}

STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::VSSetSamplers(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
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
	LogDebug("ID3D11DeviceContext::End called\n");

	GetD3D11DeviceContext()->End(pAsync);
}

STDMETHODIMP D3D11Wrapper::ID3D11DeviceContext::GetData(THIS_
	/* [annotation] */
	__in  D3D11Base::ID3D11Asynchronous *pAsync,
	/* [annotation] */
	__out_bcount_opt(DataSize)  void *pData,
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
	__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
	/* [annotation] */
	__in_ecount(NumViews) D3D11Base::ID3D11ShaderResourceView *const *ppShaderResourceViews)
{
	GetD3D11DeviceContext()->GSSetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
}

STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::GSSetSamplers(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
	/* [annotation] */
	__in_ecount(NumSamplers) D3D11Base::ID3D11SamplerState *const *ppSamplers)
{
	GetD3D11DeviceContext()->GSSetSamplers(StartSlot, NumSamplers, ppSamplers);
}

STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::OMSetRenderTargets(THIS_
	/* [annotation] */
	__in_range(0, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT)  UINT NumViews,
	/* [annotation] */
	__in_ecount_opt(NumViews) D3D11Base::ID3D11RenderTargetView *const *ppRenderTargetViews,
	/* [annotation] */
	__in_opt D3D11Base::ID3D11DepthStencilView *pDepthStencilView)
{
	LogDebug("ID3D11DeviceContext::OMSetRenderTargets called with NumViews = %d\n", NumViews);

	if (G->hunting)
	{
		if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
		G->mCurrentRenderTargets.clear();
		if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);

		for (UINT i = 0; i < NumViews; ++i)
			RecordRenderTargetInfo(ppRenderTargetViews[i], i);

		RecordDepthStencil(pDepthStencilView);
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
	__in_range(0, D3D11_PS_CS_UAV_REGISTER_COUNT - 1)  UINT UAVStartSlot,
	/* [annotation] */
	__in  UINT NumUAVs,
	/* [annotation] */
	__in_ecount_opt(NumUAVs) D3D11Base::ID3D11UnorderedAccessView *const *ppUnorderedAccessViews,
	/* [annotation] */
	__in_ecount_opt(NumUAVs)  const UINT *pUAVInitialCounts)
{
	LogDebug("ID3D11DeviceContext::OMSetRenderTargetsAndUnorderedAccessViews called with NumRTVs = %d, NumUAVs = %d\n", NumRTVs, NumUAVs);

	GetD3D11DeviceContext()->OMSetRenderTargetsAndUnorderedAccessViews(NumRTVs, ppRenderTargetViews, pDepthStencilView,
		UAVStartSlot, NumUAVs, ppUnorderedAccessViews, pUAVInitialCounts);
}

STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::OMSetBlendState(THIS_
	/* [annotation] */
	__in_opt  D3D11Base::ID3D11BlendState *pBlendState,
	/* [annotation] */
	__in_opt  const FLOAT BlendFactor[4],
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
	__in_range(0, D3D11_SO_BUFFER_SLOT_COUNT)  UINT NumBuffers,
	/* [annotation] */
	__in_ecount_opt(NumBuffers)  D3D11Base::ID3D11Buffer *const *ppSOTargets,
	/* [annotation] */
	__in_ecount_opt(NumBuffers)  const UINT *pOffsets)
{
	LogDebug("ID3D11DeviceContext::SOSetTargets called with NumBuffers = %d\n", NumBuffers);

	GetD3D11DeviceContext()->SOSetTargets(NumBuffers, ppSOTargets, pOffsets);
}

STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::DrawAuto(THIS)
{
	LogDebug("ID3D11DeviceContext::DrawAuto called\n");

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
	LogDebug("ID3D11DeviceContext::DrawIndexedInstancedIndirect called\n");

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
	LogDebug("ID3D11DeviceContext::DrawInstancedIndirect called\n");

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
	__in  const FLOAT ColorRGBA[4])
{
	LogDebug("ID3D11DeviceContext::ClearRenderTargetView called with RenderTargetView=%p, color=[%f,%f,%f,%f]\n", pRenderTargetView,
		ColorRGBA[0], ColorRGBA[1], ColorRGBA[2], ColorRGBA[3]);

	//if (G->hunting)
	{
		// Update stereo parameter texture.
		LogDebug("  updating stereo parameter texture.\n");

		ID3D11Device *device;
		GetDevice(&device);

		device->mParamTextureManager.mScreenWidth = (float)G->mSwapChainInfo.width;
		device->mParamTextureManager.mScreenHeight = (float)G->mSwapChainInfo.height;
		if (G->ENABLE_TUNE)
		{
			//device->mParamTextureManager.mSeparationModifier = gTuneValue;
			device->mParamTextureManager.mTuneVariable1 = G->gTuneValue[0];
			device->mParamTextureManager.mTuneVariable2 = G->gTuneValue[1];
			device->mParamTextureManager.mTuneVariable3 = G->gTuneValue[2];
			device->mParamTextureManager.mTuneVariable4 = G->gTuneValue[3];
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
	__in  const UINT Values[4])
{
	GetD3D11DeviceContext()->ClearUnorderedAccessViewUint(pUnorderedAccessView, Values);
}

STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::ClearUnorderedAccessViewFloat(THIS_
	/* [annotation] */
	__in  D3D11Base::ID3D11UnorderedAccessView *pUnorderedAccessView,
	/* [annotation] */
	__in  const FLOAT Values[4])
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
	__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
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
	LogDebug("ID3D11DeviceContext::HSSetShader called\n");

	GetD3D11DeviceContext()->HSSetShader(pHullShader, ppClassInstances, NumClassInstances);
}

STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::HSSetSamplers(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
	/* [annotation] */
	__in_ecount(NumSamplers)  D3D11Base::ID3D11SamplerState *const *ppSamplers)
{
	GetD3D11DeviceContext()->HSSetSamplers(StartSlot, NumSamplers, ppSamplers);
}

STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::HSSetConstantBuffers(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
	/* [annotation] */
	__in_ecount(NumBuffers)  D3D11Base::ID3D11Buffer *const *ppConstantBuffers)
{
	GetD3D11DeviceContext()->HSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}

STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::DSSetShaderResources(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
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
	LogDebug("ID3D11DeviceContext::DSSetShader called\n");

	GetD3D11DeviceContext()->DSSetShader(pDomainShader, ppClassInstances, NumClassInstances);
}

STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::DSSetSamplers(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
	/* [annotation] */
	__in_ecount(NumSamplers)  D3D11Base::ID3D11SamplerState *const *ppSamplers)
{
	GetD3D11DeviceContext()->DSSetSamplers(StartSlot, NumSamplers, ppSamplers);
}

STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::DSSetConstantBuffers(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
	/* [annotation] */
	__in_ecount(NumBuffers)  D3D11Base::ID3D11Buffer *const *ppConstantBuffers)
{
	GetD3D11DeviceContext()->DSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}

STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::CSSetShaderResources(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
	/* [annotation] */
	__in_ecount(NumViews)  D3D11Base::ID3D11ShaderResourceView *const *ppShaderResourceViews)
{
	GetD3D11DeviceContext()->CSSetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
}

STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::CSSetUnorderedAccessViews(THIS_
	/* [annotation] */
	__in_range(0, D3D11_PS_CS_UAV_REGISTER_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_PS_CS_UAV_REGISTER_COUNT - StartSlot)  UINT NumUAVs,
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
	LogDebug("ID3D11DeviceContext::CSSetShader called\n");

	GetD3D11DeviceContext()->CSSetShader(pComputeShader, ppClassInstances, NumClassInstances);
}

STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::CSSetSamplers(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
	/* [annotation] */
	__in_ecount(NumSamplers)  D3D11Base::ID3D11SamplerState *const *ppSamplers)
{
	GetD3D11DeviceContext()->CSSetSamplers(StartSlot, NumSamplers, ppSamplers);
}

STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::CSSetConstantBuffers(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
	/* [annotation] */
	__in_ecount(NumBuffers)  D3D11Base::ID3D11Buffer *const *ppConstantBuffers)
{
	GetD3D11DeviceContext()->CSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}

STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::VSGetConstantBuffers(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
	/* [annotation] */
	__out_ecount(NumBuffers)  D3D11Base::ID3D11Buffer **ppConstantBuffers)
{
	GetD3D11DeviceContext()->VSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}

STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::PSGetShaderResources(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
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

	LogDebug("D3D11Wrapper::ID3D11DeviceContext::PSGetShader out: %p\n", *ppPixelShader);
}

STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::PSGetSamplers(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
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
	GetD3D11DeviceContext()->VSGetShader(ppVertexShader, ppClassInstances, pNumClassInstances);

	// Todo: At GetShader, we need to return the original shader if it's been reloaded.
	LogDebug("D3D11Wrapper::ID3D11DeviceContext::VSGetShader out: %p\n", *ppVertexShader);
}

STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::PSGetConstantBuffers(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
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
	__in_range(0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumBuffers,
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
	__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
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
	__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
	/* [annotation] */
	__out_ecount(NumViews)  D3D11Base::ID3D11ShaderResourceView **ppShaderResourceViews)
{
	GetD3D11DeviceContext()->VSGetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
}

STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::VSGetSamplers(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
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
	__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
	/* [annotation] */
	__out_ecount(NumViews)  D3D11Base::ID3D11ShaderResourceView **ppShaderResourceViews)
{
	GetD3D11DeviceContext()->GSGetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
}

STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::GSGetSamplers(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
	/* [annotation] */
	__out_ecount(NumSamplers)  D3D11Base::ID3D11SamplerState **ppSamplers)
{
	GetD3D11DeviceContext()->GSGetSamplers(StartSlot, NumSamplers, ppSamplers);
}

STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::OMGetRenderTargets(THIS_
	/* [annotation] */
	__in_range(0, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT)  UINT NumViews,
	/* [annotation] */
	__out_ecount_opt(NumViews)  D3D11Base::ID3D11RenderTargetView **ppRenderTargetViews,
	/* [annotation] */
	__out_opt  D3D11Base::ID3D11DepthStencilView **ppDepthStencilView)
{
	GetD3D11DeviceContext()->OMGetRenderTargets(NumViews, ppRenderTargetViews, ppDepthStencilView);
}

STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::OMGetRenderTargetsAndUnorderedAccessViews(THIS_
	/* [annotation] */
	__in_range(0, D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT)  UINT NumRTVs,
	/* [annotation] */
	__out_ecount_opt(NumRTVs)  D3D11Base::ID3D11RenderTargetView **ppRenderTargetViews,
	/* [annotation] */
	__out_opt  D3D11Base::ID3D11DepthStencilView **ppDepthStencilView,
	/* [annotation] */
	__in_range(0, D3D11_PS_CS_UAV_REGISTER_COUNT - 1)  UINT UAVStartSlot,
	/* [annotation] */
	__in_range(0, D3D11_PS_CS_UAV_REGISTER_COUNT - UAVStartSlot)  UINT NumUAVs,
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
	__out_opt  FLOAT BlendFactor[4],
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
	__in_range(0, D3D11_SO_BUFFER_SLOT_COUNT)  UINT NumBuffers,
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
	__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
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
	__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
	/* [annotation] */
	__out_ecount(NumSamplers)  D3D11Base::ID3D11SamplerState **ppSamplers)
{
	GetD3D11DeviceContext()->HSGetSamplers(StartSlot, NumSamplers, ppSamplers);
}

STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::HSGetConstantBuffers(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
	/* [annotation] */
	__out_ecount(NumBuffers)  D3D11Base::ID3D11Buffer **ppConstantBuffers)
{
	GetD3D11DeviceContext()->HSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}

STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::DSGetShaderResources(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
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
	__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
	/* [annotation] */
	__out_ecount(NumSamplers)  D3D11Base::ID3D11SamplerState **ppSamplers)
{
	GetD3D11DeviceContext()->DSGetSamplers(StartSlot, NumSamplers, ppSamplers);
}

STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::DSGetConstantBuffers(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
	/* [annotation] */
	__out_ecount(NumBuffers)  D3D11Base::ID3D11Buffer **ppConstantBuffers)
{
	GetD3D11DeviceContext()->DSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}

STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::CSGetShaderResources(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot)  UINT NumViews,
	/* [annotation] */
	__out_ecount(NumViews)  D3D11Base::ID3D11ShaderResourceView **ppShaderResourceViews)
{
	GetD3D11DeviceContext()->CSGetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
}

STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::CSGetUnorderedAccessViews(THIS_
	/* [annotation] */
	__in_range(0, D3D11_PS_CS_UAV_REGISTER_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_PS_CS_UAV_REGISTER_COUNT - StartSlot)  UINT NumUAVs,
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
	__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot)  UINT NumSamplers,
	/* [annotation] */
	__out_ecount(NumSamplers)  D3D11Base::ID3D11SamplerState **ppSamplers)
{
	GetD3D11DeviceContext()->CSGetSamplers(StartSlot, NumSamplers, ppSamplers);
}

STDMETHODIMP_(void) D3D11Wrapper::ID3D11DeviceContext::CSGetConstantBuffers(THIS_
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1)  UINT StartSlot,
	/* [annotation] */
	__in_range(0, D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot)  UINT NumBuffers,
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
