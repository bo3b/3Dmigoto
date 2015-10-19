
D3D10Wrapper::ID3D10Device::ID3D10Device(D3D10Base::ID3D10Device *pDevice)
    : D3D10Wrapper::IDirect3DUnknown((IUnknown*) pDevice),
	mStereoHandle(0), mStereoResourceView(0), mStereoTexture(0), mIniResourceView(0), mIniTexture(0), mZBufferResourceView(0)
{
	if (D3D10Base::NVAPI_OK != D3D10Base::NvAPI_Stereo_CreateHandleFromIUnknown(pDevice, &mStereoHandle))
		mStereoHandle = 0;
	
	// This reassignment is not valid because it's a private member.  Not sure why this
	// was being done- maybe for Tune support.
	//mParamTextureManager.mStereoHandle = mStereoHandle;

	LogInfo("  created NVAPI stereo handle. Handle = %p\n", mStereoHandle);

	// Override custom settings.
	if (mStereoHandle && G->gSurfaceCreateMode >= 0)
	{
		//NvAPIOverride();
		LogInfo("  setting custom surface creation mode.\n");

		if (D3D10Base::NVAPI_OK != D3D10Base::NvAPI_Stereo_SetSurfaceCreationMode(mStereoHandle,
			(D3D10Base::NVAPI_STEREO_SURFACECREATEMODE) G->gSurfaceCreateMode))
		{
			LogInfo("    call failed.\n");
		}
	}
	// Create stereo parameter texture.
	if (mStereoHandle)
	{
		LogInfo("  creating stereo parameter texture.\n");

		D3D10Base::D3D10_TEXTURE2D_DESC desc;
		memset(&desc, 0, sizeof(D3D10Base::D3D10_TEXTURE2D_DESC));
		desc.Width = D3D10Base::nv::stereo::ParamTextureManagerD3D10::Parms::StereoTexWidth;
		desc.Height = D3D10Base::nv::stereo::ParamTextureManagerD3D10::Parms::StereoTexHeight;
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.Format = D3D10Base::nv::stereo::ParamTextureManagerD3D10::Parms::StereoTexFormat;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.Usage = D3D10Base::D3D10_USAGE_DEFAULT;
		desc.BindFlags = D3D10Base::D3D10_BIND_SHADER_RESOURCE;
		desc.CPUAccessFlags = 0;
		desc.MiscFlags = 0;
		HRESULT ret = pDevice->CreateTexture2D(&desc, 0, &mStereoTexture);
		if (FAILED(ret))
		{
			LogInfo("    call failed with result = %x.\n", ret);
		}
		else
		{
			LogInfo("    stereo texture created, handle = %p\n", mStereoTexture);
			LogInfo("  creating stereo parameter resource view.\n");

			// Since we need to bind the texture to a shader input, we also need a resource view.
			D3D10Base::D3D10_SHADER_RESOURCE_VIEW_DESC descRV;
			memset(&descRV, 0, sizeof(D3D10Base::D3D10_SHADER_RESOURCE_VIEW_DESC));
			descRV.Format = desc.Format;
			descRV.ViewDimension = D3D10Base::D3D10_SRV_DIMENSION_TEXTURE2D;
			descRV.Texture2D.MostDetailedMip = 0;
			descRV.Texture2D.MipLevels = -1;
			ret = pDevice->CreateShaderResourceView(mStereoTexture, &descRV, &mStereoResourceView);
			if (FAILED(ret))
			{
				LogInfo("    call failed with result = %x.\n", ret);
			}
			LogInfo("    stereo texture resource view created, handle = %p.\n", mStereoResourceView);
		}
	}

	// If constants are specified in the .ini file that need to be sent to shaders, we need to create
	// the resource view in order to deliver them via SetShaderResources.
	// Check for depth buffer view.
	if ((G->iniParams.x != FLT_MAX) || (G->iniParams.y != FLT_MAX) || (G->iniParams.z != FLT_MAX) || (G->iniParams.w != FLT_MAX))
	{
		D3D10Base::D3D10_TEXTURE1D_DESC desc;
		memset(&desc, 0, sizeof(D3D10Base::D3D10_TEXTURE1D_DESC));
		D3D10Base::D3D10_SUBRESOURCE_DATA initialData;

		LogInfo("  creating .ini constant parameter texture.\n");

		// Stuff the constants read from the .ini file into the subresource data structure, so 
		// we can init the texture with them.
		initialData.pSysMem = &G->iniParams;
		initialData.SysMemPitch = sizeof(DirectX::XMFLOAT4) * 1;	// only one 4 element struct 

		desc.Width = 1;												// 1 texel, .rgba as a float4
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.Format = D3D10Base::DXGI_FORMAT_R32G32B32A32_FLOAT;	// float4
		desc.Usage = D3D10Base::D3D10_USAGE_DYNAMIC;				// Read/Write access from GPU and CPU
		desc.BindFlags = D3D10Base::D3D10_BIND_SHADER_RESOURCE;		// As resource view, access via t120
		desc.CPUAccessFlags = D3D10Base::D3D10_CPU_ACCESS_WRITE;				// allow CPU access for hotkeys
		desc.MiscFlags = 0;
		HRESULT ret = pDevice->CreateTexture1D(&desc, &initialData, &mIniTexture);
		if (FAILED(ret))
		{
			LogInfo("    CreateTexture1D call failed with result = %x.\n", ret);
		}
		else
		{
			LogInfo("    IniParam texture created, handle = %p\n", mIniTexture);
			LogInfo("  creating IniParam resource view.\n");

			// Since we need to bind the texture to a shader input, we also need a resource view.
			// The pDesc is set to NULL so that it will simply use the desc format above.
			D3D10Base::D3D10_SHADER_RESOURCE_VIEW_DESC descRV;
			memset(&descRV, 0, sizeof(D3D10Base::D3D10_SHADER_RESOURCE_VIEW_DESC));

			ret = pDevice->CreateShaderResourceView(mIniTexture, NULL, &mIniResourceView);
			if (FAILED(ret))
			{
				LogInfo("   CreateShaderResourceView call failed with result = %x.\n", ret);
			}

			LogInfo("    Iniparams resource view created, handle = %p.\n", mIniResourceView);
		}
	}
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

	ULONG ulRef = m_pUnk ? m_pUnk->Release() : 0;
	LogDebug("  internal counter = %d\n", ulRef);

	--m_ulRef;

	if (ulRef == 0)
	{
		if (!gLogDebug) LogInfo("ID3D10Device::Release handle=%p, counter=%d, internal counter = %d, this=%p\n", m_pUnk, m_ulRef, ulRef, this);
		LogInfo("  deleting self\n");

		if (m_pUnk) m_List.DeleteMember(m_pUnk); m_pUnk = 0;
		if (mStereoHandle)
		{
			int result = D3D10Base::NvAPI_Stereo_DestroyHandle(mStereoHandle);
			mStereoHandle = 0;
			LogInfo("  releasing NVAPI stereo handle, result = %d\n", result);
		}
		if (mStereoResourceView)
		{
			long result = mStereoResourceView->Release();
			mStereoResourceView = 0;
			LogInfo("  releasing stereo parameters resource view, result = %d\n", result);
		}
		if (mStereoTexture)
		{
			long result = mStereoTexture->Release();
			mStereoTexture = 0;
			LogInfo("  releasing stereo texture, result = %d\n", result);
		}
		if (mIniResourceView)
		{
			long result = mIniResourceView->Release();
			mIniResourceView = 0;
			LogInfo("  releasing ini parameters resource view, result = %d\n", result);
		}
		if (mIniTexture)
		{
			long result = mIniTexture->Release();
			mIniTexture = 0;
			LogInfo("  releasing iniparams texture, result = %d\n", result);
		}
		if (!G->mPreloadedPixelShaders.empty())
		{
			LogInfo("  releasing preloaded pixel shaders\n");

			for (PreloadPixelShaderMap::iterator i = G->mPreloadedPixelShaders.begin(); i != G->mPreloadedPixelShaders.end(); ++i)
				i->second->Release();
			G->mPreloadedPixelShaders.clear();
		}
		if (!G->mPreloadedVertexShaders.empty())
		{
			LogInfo("  releasing preloaded vertex shaders\n");

			for (PreloadVertexShaderMap::iterator i = G->mPreloadedVertexShaders.begin(); i != G->mPreloadedVertexShaders.end(); ++i)
				i->second->Release();
			G->mPreloadedVertexShaders.clear();
		}
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
	LogDebug("ID3D10Device::GetPrivateData called with GUID = %08lx-%04hx-%04hx-%02hhx%02hhx-%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx\n",
		guid.Data1, guid.Data2, guid.Data3, guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3], 
		guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
	
	HRESULT hr = GetD3D10Device()->GetPrivateData(guid, pDataSize, pData);
	LogDebug("  returns result = %x, DataSize = %d\n", hr, *pDataSize);
	
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
	LogDebug("ID3D10Device::SetPrivateData called with GUID = %08lx-%04hx-%04hx-%02hhx%02hhx-%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx\n",
		guid.Data1, guid.Data2, guid.Data3, guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3], 
		guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
	LogDebug("  DataSize = %d\n", DataSize);
	
	HRESULT hr = GetD3D10Device()->SetPrivateData(guid, DataSize, pData);
	LogDebug("  returns result = %x\n", hr);
	
	return hr;
}
        
STDMETHODIMP D3D10Wrapper::ID3D10Device::SetPrivateDataInterface(THIS_
            /* [annotation] */ 
            __in  REFGUID guid,
            /* [annotation] */ 
            __in_opt  const IUnknown *pData)
{
	LogDebug("ID3D10Device::SetPrivateDataInterface called with GUID=%08lx-%04hx-%04hx-%02hhx%02hhx-%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx\n",
		guid.Data1, guid.Data2, guid.Data3, guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3], 
		guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);
	
	HRESULT hr = GetD3D10Device()->SetPrivateDataInterface(guid, pData);
	LogDebug("  returns result = %x\n", hr);
	
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
	LogDebug("ID3D10Device::VSSetConstantBuffers called with StartSlot = %d, NumBuffers = %d\n", StartSlot, NumBuffers);
	
	GetD3D10Device()->VSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::PSSetShaderResources(THIS_
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
            /* [annotation] */ 
            __in_ecount(NumViews)  D3D10Base::ID3D10ShaderResourceView *const *ppShaderResourceViews)
{
	LogDebug("ID3D10Device::PSSetShaderResources called\n");
	
	GetD3D10Device()->PSSetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::PSSetShader(THIS_
            /* [annotation] */ 
            __in_opt  D3D10Base::ID3D10PixelShader *pPixelShader)
{
	LogDebug("ID3D10Device::PSSetShader called with pixelshader handle = %p\n", pPixelShader);

	bool patchedShader = false;
	if (pPixelShader)
	{
		// Store as current pixel shader. Need to do this even while
		// not hunting for ShaderOverride sections.
		if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
		PixelShaderMap::iterator i = G->mPixelShaders.find(pPixelShader);
		if (i != G->mPixelShaders.end())
		{
			G->mCurrentPixelShader = i->second;
			G->mCurrentPixelShaderHandle = pPixelShader;
			LogDebug("  pixel shader found: handle = %p, hash = %08lx%08lx\n", pPixelShader, (UINT32)(G->mCurrentPixelShader >> 32), (UINT32)G->mCurrentPixelShader);

			if (G->hunting) {
				// Add to visited pixel shaders.
				G->mVisitedPixelShaders.insert(i->second);
				patchedShader = true;
			}

			// second try to hide index buffer.
			// if (mCurrentIndexBuffer == mSelectedIndexBuffer)
			//	pIndexBuffer = 0;
		}
		else
		{
			LogInfo("  pixel shader %p not found\n", pPixelShader);
		}
	}

	if (G->hunting && pPixelShader)
	{
		// Replacement map.
		if (G->marking_mode == MARKING_MODE_ORIGINAL || !G->fix_enabled) {
			PixelShaderReplacementMap::iterator j = G->mOriginalPixelShaders.find(pPixelShader);
			if ((G->mSelectedPixelShader == G->mCurrentPixelShader || !G->fix_enabled) && j != G->mOriginalPixelShaders.end())
			{
				D3D10Base::ID3D10PixelShader *shader = j->second;
				if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
				GetD3D10Device()->PSSetShader(shader);
				return;
			}
		}
		if (G->marking_mode == MARKING_MODE_ZERO) {
			PixelShaderReplacementMap::iterator j = G->mZeroPixelShaders.find(pPixelShader);
			if (G->mSelectedPixelShader == G->mCurrentPixelShader && j != G->mZeroPixelShaders.end())
			{
				D3D10Base::ID3D10PixelShader *shader = j->second;
				if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
				GetD3D10Device()->PSSetShader(shader);
				return;
			}
		}

		// If the shader has been live reloaded from ShaderFixes, use the new one
		ShaderReloadMap::iterator it = G->mReloadedShaders.find(pPixelShader);
		if (it != G->mReloadedShaders.end() && it->second.replacement != NULL)
		{
			LogDebug("  pixel shader replaced by: %p\n", it->second.replacement);

			// Todo: It might make sense to Release() the original shader, to recover memory on GPU
			D3D10Base::ID3D10PixelShader *shader = (D3D10Base::ID3D10PixelShader*) it->second.replacement;
			if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
			GetD3D10Device()->PSSetShader(shader);
			return;
		}
	}

	if (pPixelShader) {
		if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
	}

	GetD3D10Device()->PSSetShader(pPixelShader);

	// When hunting is off, send stereo texture to all shaders, as any might need it.
	// Maybe a bit of a waste of GPU resource, but optimizes CPU use.
	if (!G->hunting || patchedShader)
	{
		// Device is THIS here, not context like DX11
		D3D10Wrapper::ID3D10Device *device = this;
		//GetDevice(&device);
		if (device)
		{
			// Set NVidia stereo texture.
			if (device->mStereoResourceView)
			{
				LogDebug("  adding NVidia stereo parameter texture to shader resources in slot 125.\n");

				GetD3D10Device()->PSSetShaderResources(125, 1, &device->mStereoResourceView);
			}
			// Set constants from ini file if they exist
			if (device->mIniResourceView)
			{
				LogDebug("  adding ini constants as texture to shader resources in slot 120.\n");

				GetD3D10Device()->PSSetShaderResources(120, 1, &device->mIniResourceView);
			}
			// Set custom depth texture.
			if (device->mZBufferResourceView)
			{
				LogDebug("  adding Z buffer to shader resources in slot 126.\n");

				GetD3D10Device()->PSSetShaderResources(126, 1, &device->mZBufferResourceView);
			}
			//device->Release();
		}
		else
		{
			LogInfo("  error querying device. Can't set NVidia stereo parameter texture.\n");
		}
	}
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::PSSetSamplers(THIS_
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
            /* [annotation] */ 
            __in_ecount(NumSamplers)  D3D10Base::ID3D10SamplerState *const *ppSamplers)
{
	LogDebug("ID3D10Device::PSSetSamplers called\n");
	
	GetD3D10Device()->PSSetSamplers(StartSlot, NumSamplers, ppSamplers);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::VSSetShader(THIS_
            /* [annotation] */ 
            __in_opt  D3D10Base::ID3D10VertexShader *pVertexShader)
{
	LogDebug("ID3D10DeviceContext::VSSetShader called with vertexshader handle = %p\n", pVertexShader);

	bool patchedShader = false;
	if (pVertexShader)
	{
		// Store as current vertex shader. Need to do this even while
		// not hunting for ShaderOverride sections.
		if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
		VertexShaderMap::iterator i = G->mVertexShaders.find(pVertexShader);
		if (i != G->mVertexShaders.end())
		{
			G->mCurrentVertexShader = i->second;
			G->mCurrentVertexShaderHandle = pVertexShader;
			LogDebug("  vertex shader found: handle = %p, hash = %08lx%08lx\n", pVertexShader, (UINT32)(G->mCurrentVertexShader >> 32), (UINT32)G->mCurrentVertexShader);

			if (G->hunting) {
				// Add to visited vertex shaders.
				G->mVisitedVertexShaders.insert(i->second);
				patchedShader = true;
			}

			// second try to hide index buffer.
			// if (mCurrentIndexBuffer == mSelectedIndexBuffer)
			//	pIndexBuffer = 0;
		}
		else
		{
			LogInfo("  vertex shader %p not found\n", pVertexShader);
			// G->mCurrentVertexShader = 0;
		}
	}

	if (G->hunting && pVertexShader)
	{
		// Replacement map.
		if (G->marking_mode == MARKING_MODE_ORIGINAL || !G->fix_enabled) {
			VertexShaderReplacementMap::iterator j = G->mOriginalVertexShaders.find(pVertexShader);
			if ((G->mSelectedVertexShader == G->mCurrentVertexShader || !G->fix_enabled) && j != G->mOriginalVertexShaders.end())
			{
				D3D10Base::ID3D10VertexShader *shader = j->second;
				if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
				GetD3D10Device()->VSSetShader(shader);
				return;
			}
		}
		if (G->marking_mode == MARKING_MODE_ZERO) {
			VertexShaderReplacementMap::iterator j = G->mZeroVertexShaders.find(pVertexShader);
			if (G->mSelectedVertexShader == G->mCurrentVertexShader && j != G->mZeroVertexShaders.end())
			{
				D3D10Base::ID3D10VertexShader *shader = j->second;
				if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
				GetD3D10Device()->VSSetShader(shader);
				return;
			}
		}

		// If the shader has been live reloaded from ShaderFixes, use the new one
		ShaderReloadMap::iterator it = G->mReloadedShaders.find(pVertexShader);
		if (it != G->mReloadedShaders.end() && it->second.replacement != NULL)
		{
			LogDebug("  vertex shader replaced by: %p\n", it->second.replacement);

			D3D10Base::ID3D10VertexShader *shader = (D3D10Base::ID3D10VertexShader*) it->second.replacement;
			if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
			GetD3D10Device()->VSSetShader(shader);
			return;
		}
	}

	if (pVertexShader) {
		if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
	}

	GetD3D10Device()->VSSetShader(pVertexShader);

	// When hunting is off, send stereo texture to all shaders, as any might need it.
	// Maybe a bit of a waste of GPU resource, but optimizes CPU use.
	if (!G->hunting || patchedShader)
	{
		D3D10Wrapper::ID3D10Device *device = this;
		//GetDevice(&device);  not context based, like DX11
		if (device)
		{
			// Set NVidia stereo texture.
			if (device->mStereoResourceView)
			{
				LogDebug("  adding NVidia stereo parameter texture to shader resources in slot 125.\n");

				GetD3D10Device()->VSSetShaderResources(125, 1, &device->mStereoResourceView);
			}

			// Set constants from ini file if they exist
			if (device->mIniResourceView)
			{
				LogDebug("  adding ini constants as texture to shader resources in slot 120.\n");

				GetD3D10Device()->VSSetShaderResources(120, 1, &device->mIniResourceView);
			}
			//device->Release();
		}
		else
		{
			LogInfo("  error querying device. Can't set NVidia stereo parameter texture.\n");
		}
	}
}
        
UINT64 CalcTexture2DDescHash(const D3D10Base::D3D10_TEXTURE2D_DESC *desc,
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

UINT64 CalcTexture3DDescHash(const D3D10Base::D3D10_TEXTURE3D_DESC *desc,
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

static UINT64 GetTexture2DHash(D3D10Base::ID3D10Texture2D *texture,
	bool log_new, struct ResourceInfo *resource_info)
{

	D3D10Base::D3D10_TEXTURE2D_DESC desc;
	std::unordered_map<D3D10Base::ID3D10Texture2D *, UINT64>::iterator j;

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

	return CalcTexture2DDescHash(&desc, 0, 0, 0);
}

static UINT64 GetTexture3DHash(D3D10Base::ID3D10Texture3D *texture,
	bool log_new, struct ResourceInfo *resource_info)
{

	D3D10Base::D3D10_TEXTURE3D_DESC desc;
	std::unordered_map<D3D10Base::ID3D10Texture3D *, UINT64>::iterator j;

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

	return CalcTexture3DDescHash(&desc, 0, 0, 0);
}

// Records the hash of this shader resource view for later lookup. Returns the
// handle to the resource, but be aware that it no longer has a reference and
// should only be used for map lookups.
static void *RecordResourceViewStats(D3D10Base::ID3D10ShaderResourceView *view)
{
	D3D10Base::D3D10_SHADER_RESOURCE_VIEW_DESC desc;
	D3D10Base::ID3D10Resource *resource = NULL;
	UINT64 hash = 0;

	if (!view)
		return NULL;

	view->GetResource(&resource);
	if (!resource)
		return NULL;

	view->GetDesc(&desc);

	switch (desc.ViewDimension) {
		case D3D10Base::D3D10_SRV_DIMENSION_TEXTURE2D:
		case D3D10Base::D3D10_SRV_DIMENSION_TEXTURE2DMS:
		case D3D10Base::D3D10_SRV_DIMENSION_TEXTURE2DMSARRAY:
			hash = GetTexture2DHash((D3D10Base::ID3D10Texture2D *)resource, false, NULL);
			break;
		case D3D10Base::D3D10_SRV_DIMENSION_TEXTURE3D:
			hash = GetTexture3DHash((D3D10Base::ID3D10Texture3D *)resource, false, NULL);
			break;
	}

	resource->Release();

	if (hash)
		G->mRenderTargets[resource] = hash;

	return resource;
}

static void RecordShaderResourceUsage(D3D10Wrapper::ID3D10Device *context)
{
	D3D10Base::ID3D10ShaderResourceView *ps_views[D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
	D3D10Base::ID3D10ShaderResourceView *vs_views[D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT];
	void *resource;
	int i;

	context->PSGetShaderResources(0, D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, ps_views);
	context->VSGetShaderResources(0, D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT, vs_views);

	for (i = 0; i < D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT; i++) {
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

static void RecordRenderTargetInfo(D3D10Base::ID3D10RenderTargetView *target, UINT view_num)
{
	D3D10Base::D3D10_RENDER_TARGET_VIEW_DESC desc;
	D3D10Base::ID3D10Resource *resource = NULL;
	struct ResourceInfo resource_info;
	UINT64 hash = 0;

	if (!target)
		return;

	target->GetDesc(&desc);

	LogDebug("  View #%d, Format = %d, Is2D = %d\n",
		view_num, desc.Format, D3D10Base::D3D10_RTV_DIMENSION_TEXTURE2D == desc.ViewDimension);

	switch (desc.ViewDimension) {
		case D3D10Base::D3D10_RTV_DIMENSION_TEXTURE2D:
		case D3D10Base::D3D10_RTV_DIMENSION_TEXTURE2DMS:
			target->GetResource(&resource);
			if (!resource)
				return;
			hash = GetTexture2DHash((D3D10Base::ID3D10Texture2D *)resource,
				gLogDebug, &resource_info);
			resource->Release();
			break;
		case D3D10Base::D3D10_RTV_DIMENSION_TEXTURE3D:
			target->GetResource(&resource);
			if (!resource)
				return;
			hash = GetTexture3DHash((D3D10Base::ID3D10Texture3D *)resource,
				gLogDebug, &resource_info);
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

static void RecordDepthStencil(D3D10Base::ID3D10DepthStencilView *target)
{
	D3D10Base::D3D10_DEPTH_STENCIL_VIEW_DESC desc;
	D3D10Base::D3D10_TEXTURE2D_DESC tex_desc;
	D3D10Base::ID3D10Resource *resource = NULL;
	D3D10Base::ID3D10Texture2D *texture;
	struct ResourceInfo resource_info;
	UINT64 hash = 0;

	if (!target)
		return;

	target->GetResource(&resource);
	if (!resource)
		return;

	target->GetDesc(&desc);

	switch (desc.ViewDimension) {
		// TODO: Is it worth recording the type of 2D texture view?
		// TODO: Maybe for array variants, record all resources in array?
		case D3D10Base::D3D10_DSV_DIMENSION_TEXTURE2D:
		case D3D10Base::D3D10_DSV_DIMENSION_TEXTURE2DARRAY:
		case D3D10Base::D3D10_DSV_DIMENSION_TEXTURE2DMS:
		case D3D10Base::D3D10_DSV_DIMENSION_TEXTURE2DMSARRAY:
			texture = (D3D10Base::ID3D10Texture2D *)resource;
			hash = GetTexture2DHash(texture, false, NULL);
			texture->GetDesc(&tex_desc);
			resource_info = tex_desc;
			break;
	}

	resource->Release();

	if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
	G->mRenderTargets[resource] = hash;
	G->mCurrentDepthTarget = resource;
	G->mDepthTargetInfo[hash] = resource_info;
	if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
}

static D3D10Base::ID3D10VertexShader *SwitchVSShader(
	D3D10Wrapper::ID3D10Device *context,
	D3D10Base::ID3D10VertexShader *shader)
{

	D3D10Base::ID3D10VertexShader *pVertexShader;
	//D3D10Base::ID3D10ClassInstance *pClassInstances;
	//UINT NumClassInstances = 0, i;

	// We can possibly save the need to get the current shader by saving the ClassInstances
	context->GetD3D10Device()->VSGetShader(&pVertexShader);
	context->GetD3D10Device()->VSSetShader(shader);

	//for (i = 0; i < NumClassInstances; i++)
	//	pClassInstances[i].Release();

	return pVertexShader;
}

static D3D10Base::ID3D10PixelShader *SwitchPSShader(
	D3D10Wrapper::ID3D10Device *context,
	D3D10Base::ID3D10PixelShader *shader)
{

	D3D10Base::ID3D10PixelShader *pPixelShader;
	//D3D10Base::ID3D10ClassInstance *pClassInstances;
	//UINT NumClassInstances = 0, i;

	// We can possibly save the need to get the current shader by saving the ClassInstances
	context->GetD3D10Device()->PSGetShader(&pPixelShader);
	context->GetD3D10Device()->PSSetShader(shader);

	//for (i = 0; i < NumClassInstances; i++)
	//	pClassInstances[i].Release();

	return pPixelShader;
}

struct DrawContext
{
	bool skip;
	bool override;
	float oldSeparation;
	float oldConvergence;
	D3D10Base::ID3D10PixelShader *oldPixelShader;
	D3D10Base::ID3D10VertexShader *oldVertexShader;

	DrawContext() :
		skip(false),
		override(false),
		oldSeparation(FLT_MAX),
		oldConvergence(FLT_MAX),
		oldVertexShader(NULL),
		oldPixelShader(NULL)
	{}
};

static void ProcessShaderOverride(D3D10Wrapper::ID3D10Device *context,
	ShaderOverride *shaderOverride,
	bool isPixelShader,
	struct DrawContext *data,
	float *separationValue, float *convergenceValue)
{
	bool use_orig = false;

	LogDebug("  override found for shader\n");

	*separationValue = shaderOverride->separation;
	if (*separationValue != FLT_MAX)
		data->override = true;
	*convergenceValue = shaderOverride->convergence;
	if (*convergenceValue != FLT_MAX)
		data->override = true;
	data->skip = shaderOverride->skip;

#if 0 /* Iterations are broken since we no longer use present() */
	// Check iteration.
	if (!shaderOverride->iterations.empty()) {
		std::vector<int>::iterator k = shaderOverride->iterations.begin();
		int currentiterations = *k = *k + 1;
		LogDebug("  current iterations = %d\n", currentiterations);

		data->override = false;
		while (++k != shaderOverride->iterations.end())
		{
			if (currentiterations == *k)
			{
				data->override = true;
				break;
			}
		}
		if (!data->override)
		{
			LogDebug("  override skipped\n");
		}
	}
#endif
	// Check index buffer filter.
	if (!shaderOverride->indexBufferFilter.empty()) {
		bool found = false;
		for (vector<UINT64>::iterator l = shaderOverride->indexBufferFilter.begin(); l != shaderOverride->indexBufferFilter.end(); ++l)
			if (G->mCurrentIndexBuffer == *l)
			{
				found = true;
				break;
			}
		if (!found)
		{
			data->override = false;
			data->skip = false;
		}

		// TODO: This filter currently seems pretty limited as it only
		// applies to handling=skip and per-draw separation/convergence.
	}

	if (shaderOverride->depth_filter != DepthBufferFilter::NONE) {
		D3D10Base::ID3D10DepthStencilView *pDepthStencilView = NULL;

		context->GetD3D10Device()->OMGetRenderTargets(0, NULL, &pDepthStencilView);

		// Remember - we are NOT switching to the original shader when the condition is true
		if (shaderOverride->depth_filter == DepthBufferFilter::DEPTH_ACTIVE && !pDepthStencilView) {
			use_orig = true;
		}
		else if (shaderOverride->depth_filter == DepthBufferFilter::DEPTH_INACTIVE && pDepthStencilView) {
			use_orig = true;
		}

		if (pDepthStencilView)
			pDepthStencilView->Release();

		// TODO: Add alternate filter type where the depth
		// buffer state is passed as an input to the shader
	}

	if (shaderOverride->partner_hash) {
		if (isPixelShader) {
			if (G->mCurrentVertexShader != shaderOverride->partner_hash)
				use_orig = true;
		}
		else {
			if (G->mCurrentPixelShader != shaderOverride->partner_hash)
				use_orig = true;
		}
	}

	// TODO: Add render target filters, texture filters, etc.

	if (use_orig) {
		if (isPixelShader) {
			PixelShaderReplacementMap::iterator i = G->mOriginalPixelShaders.find(G->mCurrentPixelShaderHandle);
			if (i != G->mOriginalPixelShaders.end())
				data->oldPixelShader = SwitchPSShader(context, i->second);
		}
		else {
			VertexShaderReplacementMap::iterator i = G->mOriginalVertexShaders.find(G->mCurrentVertexShaderHandle);
			if (i != G->mOriginalVertexShaders.end())
				data->oldVertexShader = SwitchVSShader(context, i->second);
		}
	}
}

static DrawContext BeforeDraw(D3D10Wrapper::ID3D10Device *device)
{
	DrawContext data;
	float separationValue = FLT_MAX, convergenceValue = FLT_MAX;

	// Skip?
	data.skip = false;

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
				for (selectedRenderTargetPos = 0; selectedRenderTargetPos < G->mCurrentRenderTargets.size(); ++selectedRenderTargetPos) {
					std::vector<std::set<void *>> &targets = G->mPixelShaderInfo[G->mCurrentPixelShader].RenderTargets;

					if (selectedRenderTargetPos >= targets.size())
						targets.push_back(std::set<void *>());

					targets[selectedRenderTargetPos].insert(G->mCurrentRenderTargets[selectedRenderTargetPos]);
				}
				if (G->mCurrentDepthTarget)
					G->mPixelShaderInfo[G->mCurrentPixelShader].DepthTargets.insert(G->mCurrentDepthTarget);
			}

			// Maybe make this optional if it turns out to have a
			// significant performance impact:
			//RecordShaderResourceUsage(device);

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

	if (!G->fix_enabled)
		return data;

	// Override settings?
	ShaderOverrideMap::iterator iVertex = G->mShaderOverrideMap.find(G->mCurrentVertexShader);
	ShaderOverrideMap::iterator iPixel = G->mShaderOverrideMap.find(G->mCurrentPixelShader);

	if (iVertex != G->mShaderOverrideMap.end())
		ProcessShaderOverride(device, &iVertex->second, false, &data, &separationValue, &convergenceValue);
	if (iPixel != G->mShaderOverrideMap.end())
		ProcessShaderOverride(device, &iPixel->second, true, &data, &separationValue, &convergenceValue);

	if (data.override) {
		//D3D10Wrapper::ID3D10Device *device;
		//context->GetDevice(&device);
		if (device->mStereoHandle) {
			if (separationValue != FLT_MAX) {
				LogDebug("  setting custom separation value\n");

				if (D3D10Base::NVAPI_OK != D3D10Base::NvAPI_Stereo_GetSeparation(device->mStereoHandle, &data.oldSeparation))
				{
					LogInfo("    Stereo_GetSeparation failed.\n");
				}
				//D3D10Wrapper::NvAPIOverride();
				if (D3D10Base::NVAPI_OK != D3D10Base::NvAPI_Stereo_SetSeparation(device->mStereoHandle, separationValue * data.oldSeparation))
				{
					LogInfo("    Stereo_SetSeparation failed.\n");
				}
			}

			if (convergenceValue != FLT_MAX) {
				LogDebug("  setting custom convergence value\n");

				if (D3D10Base::NVAPI_OK != D3D10Base::NvAPI_Stereo_GetConvergence(device->mStereoHandle, &data.oldConvergence)) {
					LogInfo("    Stereo_GetConvergence failed.\n");
				}
				//D3D10Wrapper::NvAPIOverride();
				if (D3D10Base::NVAPI_OK != D3D10Base::NvAPI_Stereo_SetConvergence(device->mStereoHandle, convergenceValue * data.oldConvergence)) {
					LogInfo("    Stereo_SetConvergence failed.\n");
				}
			}
		}
		device->Release();
	}
	return data;
}

static void AfterDraw(DrawContext &data, D3D10Wrapper::ID3D10Device *device)
{
	if (data.skip)
		return;

	if (data.override) {
		//D3D10Wrapper::ID3D10Device *device;
		//context->GetDevice(&device);
		if (device->mStereoHandle) {
			if (data.oldSeparation != FLT_MAX) {
				//D3D10Wrapper::NvAPIOverride();
				if (D3D10Base::NVAPI_OK != D3D10Base::NvAPI_Stereo_SetSeparation(device->mStereoHandle, data.oldSeparation)) {
					LogInfo("    Stereo_SetSeparation failed.\n");
				}
			}

			if (data.oldConvergence != FLT_MAX) {
				//D3D10Wrapper::NvAPIOverride();
				if (D3D10Base::NVAPI_OK != D3D10Base::NvAPI_Stereo_SetConvergence(device->mStereoHandle, data.oldConvergence)) {
					LogInfo("    Stereo_SetConvergence failed.\n");
				}
			}
		}

		//device->Release();
	}

	if (data.oldVertexShader) {
		D3D10Base::ID3D10VertexShader *ret;
		ret = SwitchVSShader(device, data.oldVertexShader);
		data.oldVertexShader->Release();
		if (ret)
			ret->Release();
	}
	if (data.oldPixelShader) {
		D3D10Base::ID3D10PixelShader *ret;
		ret = SwitchPSShader(device, data.oldPixelShader);
		data.oldPixelShader->Release();
		if (ret)
			ret->Release();
	}

	// When in hunting mode, we need to get time to run the UI for stepping through shaders.
	// This gets called for every Draw, and is a definitely overkill, but is a convenient spot
	// where we are absolutely certain that everyone is set up correctly.  And where we can
	// get the original ID3D10Device.  This used to be done through the DXGI Present interface,
	// but that had a number of problems.
	RunFrameActions(device->GetD3D10Device());
}

STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::DrawIndexed(THIS_
            /* [annotation] */ 
            __in  UINT IndexCount,
            /* [annotation] */ 
            __in  UINT StartIndexLocation,
            /* [annotation] */ 
            __in  INT BaseVertexLocation)
{
	LogDebug("ID3D10Device::DrawIndexed called with IndexCount = %d, StartIndexLocation = %d, BaseVertexLocation = %d\n",
		IndexCount, StartIndexLocation, BaseVertexLocation);
	
	DrawContext c = BeforeDraw(this);
	if (!c.skip)
		GetD3D10Device()->DrawIndexed(IndexCount, StartIndexLocation, BaseVertexLocation);
	AfterDraw(c, this);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::Draw(THIS_
            /* [annotation] */ 
            __in  UINT VertexCount,
            /* [annotation] */ 
            __in  UINT StartVertexLocation)
{
	LogDebug("ID3D10Device::Draw called with VertexCount = %d, StartVertexLocation = %d\n",
		VertexCount, StartVertexLocation);
	
	DrawContext c = BeforeDraw(this);
	if (!c.skip)
		GetD3D10Device()->Draw(VertexCount, StartVertexLocation);
	AfterDraw(c, this);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::PSSetConstantBuffers(THIS_ 
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
            /* [annotation] */ 
            __in_ecount(NumBuffers)  D3D10Base::ID3D10Buffer *const *ppConstantBuffers)
{
	LogDebug("ID3D10Device::PSSetConstantBuffers called\n");
	
	GetD3D10Device()->PSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::IASetInputLayout(THIS_
            /* [annotation] */ 
            __in_opt  D3D10Base::ID3D10InputLayout *pInputLayout)
{
	LogDebug("ID3D10Device::IASetInputLayout called\n");
	
	GetD3D10Device()->IASetInputLayout(pInputLayout);
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
	LogDebug("ID3D10Device::IASetVertexBuffers called\n");
	
	GetD3D10Device()->IASetVertexBuffers(StartSlot, NumBuffers, ppVertexBuffers, pStrides, pOffsets);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::IASetIndexBuffer(THIS_
            /* [annotation] */ 
            __in_opt  D3D10Base::ID3D10Buffer *pIndexBuffer,
            /* [annotation] */ 
            __in  D3D10Base::DXGI_FORMAT Format,
            /* [annotation] */ 
            __in  UINT Offset)
{
	LogDebug("ID3D10Device::IASetIndexBuffer called\n");
	
	GetD3D10Device()->IASetIndexBuffer(pIndexBuffer, Format, Offset);
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
	LogDebug("ID3D10Device::DrawIndexedInstanced called\n");
	
	DrawContext c = BeforeDraw(this);
	if (!c.skip)
		GetD3D10Device()->DrawIndexedInstanced(IndexCountPerInstance, InstanceCount, StartIndexLocation,
		BaseVertexLocation, StartInstanceLocation);
	AfterDraw(c, this);
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
	LogDebug("ID3D10Device::DrawInstanced called\n");
	
	DrawContext c = BeforeDraw(this);
	if (!c.skip)
		GetD3D10Device()->DrawInstanced(VertexCountPerInstance, InstanceCount, StartVertexLocation,
		StartInstanceLocation);
	AfterDraw(c, this);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::GSSetConstantBuffers(THIS_
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
            /* [annotation] */ 
            __in_ecount(NumBuffers)  D3D10Base::ID3D10Buffer *const *ppConstantBuffers)
{
	LogDebug("ID3D10Device::GSSetConstantBuffers called\n");
	
	GetD3D10Device()->GSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::GSSetShader(THIS_
            /* [annotation] */ 
            __in_opt  D3D10Base::ID3D10GeometryShader *pShader)
{
	LogDebug("ID3D10Device::GSSetShader called\n");
	
	GetD3D10Device()->GSSetShader(pShader);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::IASetPrimitiveTopology(THIS_
            /* [annotation] */ 
            __in  D3D10Base::D3D10_PRIMITIVE_TOPOLOGY Topology)
{
	LogDebug("ID3D10Device::IASetPrimitiveTopology called\n");
	
	GetD3D10Device()->IASetPrimitiveTopology(Topology);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::VSSetShaderResources(THIS_
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
            /* [annotation] */ 
            __in_ecount(NumViews)  D3D10Base::ID3D10ShaderResourceView *const *ppShaderResourceViews)
{
	LogDebug("ID3D10Device::VSSetShaderResources called with StartSlot = %d, NumViews = %d\n",
		StartSlot, NumViews);
	
	GetD3D10Device()->VSSetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::VSSetSamplers(THIS_
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
            /* [annotation] */ 
            __in_ecount(NumSamplers)  D3D10Base::ID3D10SamplerState *const *ppSamplers)
{
	LogDebug("ID3D10Device::VSSetSamplers called with StartSlot = %d, NumSamplers = %d\n",
		StartSlot, NumSamplers);
	
	GetD3D10Device()->VSSetSamplers(StartSlot, NumSamplers, ppSamplers);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::SetPredication(THIS_
            /* [annotation] */ 
            __in_opt  D3D10Base::ID3D10Predicate *pPredicate,
            /* [annotation] */ 
            __in  BOOL PredicateValue)
{
	LogDebug("ID3D10Device::SetPredication called\n");
	
	GetD3D10Device()->SetPredication(pPredicate, PredicateValue);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::GSSetShaderResources(THIS_
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
            /* [annotation] */ 
            __in_ecount(NumViews)  D3D10Base::ID3D10ShaderResourceView *const *ppShaderResourceViews)
{
	LogDebug("ID3D10Device::GSSetShaderResources called\n");
	
	GetD3D10Device()->GSSetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::GSSetSamplers(THIS_
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
            /* [annotation] */ 
            __in_ecount(NumSamplers)  D3D10Base::ID3D10SamplerState *const *ppSamplers)
{
	LogDebug("ID3D10Device::GSSetSamplers called with StartSlod = %d, NumSamplers = %d\n",
		StartSlot, NumSamplers);
	
	GetD3D10Device()->GSSetSamplers(StartSlot, NumSamplers, ppSamplers);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::OMSetRenderTargets(THIS_
            /* [annotation] */ 
            __in_range( 0, D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT )  UINT NumViews,
            /* [annotation] */ 
            __in_ecount_opt(NumViews)  D3D10Base::ID3D10RenderTargetView *const *ppRenderTargetViews,
            /* [annotation] */ 
            __in_opt  D3D10Base::ID3D10DepthStencilView *pDepthStencilView)
{
	LogDebug("ID3D10Device::OMSetRenderTargets called with NumViews = %d\n", NumViews);
	
	GetD3D10Device()->OMSetRenderTargets(NumViews, ppRenderTargetViews, pDepthStencilView);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::OMSetBlendState(THIS_
            /* [annotation] */ 
            __in_opt  D3D10Base::ID3D10BlendState *pBlendState,
            /* [annotation] */ 
            __in  const FLOAT BlendFactor[ 4 ],
            /* [annotation] */ 
            __in  UINT SampleMask)
{
	LogDebug("ID3D10Device::OMSetBlendState called\n");
	
	GetD3D10Device()->OMSetBlendState(pBlendState, BlendFactor, SampleMask);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::OMSetDepthStencilState(THIS_
            /* [annotation] */ 
            __in_opt  D3D10Base::ID3D10DepthStencilState *pDepthStencilState,
            /* [annotation] */ 
            __in  UINT StencilRef)
{
	LogDebug("ID3D10Device::OMSetDepthStencilState called\n");
	
	GetD3D10Device()->OMSetDepthStencilState(pDepthStencilState, StencilRef);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::SOSetTargets(THIS_
            /* [annotation] */ 
            __in_range( 0, D3D10_SO_BUFFER_SLOT_COUNT)  UINT NumBuffers,
            /* [annotation] */ 
            __in_ecount_opt(NumBuffers)  D3D10Base::ID3D10Buffer *const *ppSOTargets,
            /* [annotation] */ 
            __in_ecount_opt(NumBuffers)  const UINT *pOffsets)
{
	LogDebug("ID3D10Device::SOSetTargets called\n");
	
	GetD3D10Device()->SOSetTargets(NumBuffers, ppSOTargets, pOffsets);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::DrawAuto(THIS)
{
	LogDebug("ID3D10Device::DrawAuto called\n");
	
	DrawContext c = BeforeDraw(this);
	if (!c.skip)
		GetD3D10Device()->DrawAuto();
	AfterDraw(c, this);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::RSSetState(THIS_
            /* [annotation] */ 
            __in_opt  D3D10Base::ID3D10RasterizerState *pRasterizerState)
{
	LogDebug("ID3D10Device::RSSetState called\n");
	
	GetD3D10Device()->RSSetState(pRasterizerState);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::RSSetViewports(THIS_
            /* [annotation] */ 
            __in_range(0, D3D10_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE)  UINT NumViewports,
            /* [annotation] */ 
            __in_ecount_opt(NumViewports)  const D3D10Base::D3D10_VIEWPORT *pViewports)
{
	LogDebug("ID3D10Device::RSSetViewports called with NumViewports = %d\n", NumViewports);
	
	GetD3D10Device()->RSSetViewports(NumViewports, pViewports);
	if (gLogFile)
	{
		if (pViewports)
		{
			for (UINT i = 0; i < NumViewports; ++i)
			{
				LogDebug("  viewport #%d: TopLeft=(%d,%d), Width=%d, Height=%d, MinDepth=%f, MaxDepth=%f\n", i,
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
	LogDebug("ID3D10Device::RSSetScissorRects called\n");
	
	GetD3D10Device()->RSSetScissorRects(NumRects, pRects);
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
	LogDebug("ID3D10Device::CopySubresourceRegion called\n");
	
	GetD3D10Device()->CopySubresourceRegion(pDstResource, DstSubresource, DstX, DstY, DstZ,
		pSrcResource, SrcSubresource, pSrcBox);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::CopyResource(THIS_
            /* [annotation] */ 
            __in  D3D10Base::ID3D10Resource *pDstResource,
            /* [annotation] */ 
            __in  D3D10Base::ID3D10Resource *pSrcResource)
{
	LogDebug("ID3D10Device::CopyResource called\n");
	
	GetD3D10Device()->CopyResource(pDstResource, pSrcResource);
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
	LogDebug("ID3D10Device::UpdateSubresource called\n");
	
	GetD3D10Device()->UpdateSubresource(pDstResource, DstSubresource, pDstBox, pSrcData, SrcRowPitch, SrcDepthPitch);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::ClearRenderTargetView(THIS_
            /* [annotation] */ 
            __in  D3D10Base::ID3D10RenderTargetView *pRenderTargetView,
            /* [annotation] */ 
            __in  const FLOAT ColorRGBA[ 4 ])
{
	LogDebug("ID3D11DeviceContext::ClearRenderTargetView called with RenderTargetView=%p, color=[%f,%f,%f,%f]\n", pRenderTargetView,
		ColorRGBA[0], ColorRGBA[1], ColorRGBA[2], ColorRGBA[3]);

	//if (G->hunting)
	{
		// Update stereo parameter texture.
		LogDebug("  updating stereo parameter texture.\n");

		ID3D10Device *device = this;
		//GetDevice(&device);

		// Todo: This variant has no Tune support.

		//device->mParamTextureManager.mScreenWidth = (float)G->mSwapChainInfo.width;
		//device->mParamTextureManager.mScreenHeight = (float)G->mSwapChainInfo.height;
		//if (G->ENABLE_TUNE)
		//{
		//	//device->mParamTextureManager.mSeparationModifier = gTuneValue;
		//	device->mParamTextureManager.mTuneVariable1 = G->gTuneValue[0];
		//	device->mParamTextureManager.mTuneVariable2 = G->gTuneValue[1];
		//	device->mParamTextureManager.mTuneVariable3 = G->gTuneValue[2];
		//	device->mParamTextureManager.mTuneVariable4 = G->gTuneValue[3];
		//	static int counter = 0;
		//	if (counter-- < 0)
		//	{
		//		counter = 30;
		//		device->mParamTextureManager.mForceUpdate = true;
		//	}
		//}

		device->mParamTextureManager.UpdateStereoTexture(device->GetD3D10Device(), device->mStereoTexture, false);
		//device->Release();
	}

	GetD3D10Device()->ClearRenderTargetView(pRenderTargetView, ColorRGBA);
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
	LogDebug("ID3D10Device::ClearDepthStencilView called\n");
	
	GetD3D10Device()->ClearDepthStencilView(pDepthStencilView, ClearFlags, Depth, Stencil);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::GenerateMips(THIS_
            /* [annotation] */ 
            __in  D3D10Base::ID3D10ShaderResourceView *pShaderResourceView)
{
	LogDebug("ID3D10Device::GenerateMips called\n");
	
	GetD3D10Device()->GenerateMips(pShaderResourceView);
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
	LogDebug("ID3D10Device::ResolveSubresource called\n");
	
	GetD3D10Device()->ResolveSubresource(pDstResource, DstSubresource, pSrcResource, SrcSubresource, Format);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::VSGetConstantBuffers(THIS_
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
            /* [annotation] */ 
            __out_ecount(NumBuffers)  D3D10Base::ID3D10Buffer **ppConstantBuffers)
{
	LogDebug("ID3D10Device::VSGetConstantBuffers called\n");
	
	GetD3D10Device()->VSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::PSGetShaderResources(THIS_
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
            /* [annotation] */ 
            __out_ecount(NumViews)  D3D10Base::ID3D10ShaderResourceView **ppShaderResourceViews)
{
	LogDebug("ID3D10Device::PSGetShaderResources called\n");
	
	GetD3D10Device()->PSGetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::PSGetShader(THIS_
            /* [annotation] */ 
            __out  D3D10Base::ID3D10PixelShader **ppPixelShader)
{
	LogDebug("ID3D10Device::PSGetShader called\n");
	
	GetD3D10Device()->PSGetShader(ppPixelShader);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::PSGetSamplers(THIS_
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
            /* [annotation] */ 
            __out_ecount(NumSamplers)  D3D10Base::ID3D10SamplerState **ppSamplers)
{
	LogDebug("ID3D10Device::PSGetSamplers called\n");
	
	GetD3D10Device()->PSGetSamplers(StartSlot, NumSamplers, ppSamplers);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::VSGetShader(THIS_
            /* [annotation] */ 
            __out  D3D10Base::ID3D10VertexShader **ppVertexShader)
{
	LogDebug("ID3D10Device::VSGetShader called\n");
	
	GetD3D10Device()->VSGetShader(ppVertexShader);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::PSGetConstantBuffers(THIS_
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
            /* [annotation] */ 
            __out_ecount(NumBuffers)  D3D10Base::ID3D10Buffer **ppConstantBuffers)
{
	LogDebug("ID3D10Device::PSGetConstantBuffers called\n");
	
	GetD3D10Device()->PSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::IAGetInputLayout(THIS_
            /* [annotation] */ 
            __out  D3D10Base::ID3D10InputLayout **ppInputLayout)
{
	LogDebug("ID3D10Device::IAGetInputLayout called\n");
	
	GetD3D10Device()->IAGetInputLayout(ppInputLayout);
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
	LogDebug("ID3D10Device::IAGetVertexBuffers called\n");
	
	GetD3D10Device()->IAGetVertexBuffers(StartSlot, NumBuffers, ppVertexBuffers, pStrides, pOffsets);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::IAGetIndexBuffer(THIS_
            /* [annotation] */ 
            __out_opt  D3D10Base::ID3D10Buffer **pIndexBuffer,
            /* [annotation] */ 
            __out_opt  D3D10Base::DXGI_FORMAT *Format,
            /* [annotation] */ 
            __out_opt  UINT *Offset)
{
	LogDebug("ID3D10Device::IAGetIndexBuffer called\n");
	
	GetD3D10Device()->IAGetIndexBuffer(pIndexBuffer, Format, Offset);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::GSGetConstantBuffers(THIS_
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - 1 )  UINT StartSlot,
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT - StartSlot )  UINT NumBuffers,
            /* [annotation] */ 
            __out_ecount(NumBuffers)  D3D10Base::ID3D10Buffer **ppConstantBuffers)
{
	LogDebug("ID3D10Device::GSGetConstantBuffers called\n");
	
	GetD3D10Device()->GSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::GSGetShader(THIS_
            /* [annotation] */ 
            __out  D3D10Base::ID3D10GeometryShader **ppGeometryShader)
{
	LogDebug("ID3D10Device::GSGetShader called\n");
	
	GetD3D10Device()->GSGetShader(ppGeometryShader);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::IAGetPrimitiveTopology(THIS_
            /* [annotation] */ 
            __out  D3D10Base::D3D10_PRIMITIVE_TOPOLOGY *pTopology)
{
	LogDebug("ID3D10Device::IAGetPrimitiveTopology called\n");
	
	GetD3D10Device()->IAGetPrimitiveTopology(pTopology);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::VSGetShaderResources(THIS_
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
            /* [annotation] */ 
            __out_ecount(NumViews)  D3D10Base::ID3D10ShaderResourceView **ppShaderResourceViews)
{
	LogDebug("ID3D10Device::VSGetShaderResources called\n");
	
	GetD3D10Device()->VSGetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::VSGetSamplers(THIS_
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
            /* [annotation] */ 
            __out_ecount(NumSamplers)  D3D10Base::ID3D10SamplerState **ppSamplers)
{
	LogDebug("ID3D10Device::VSGetSamplers called\n");
	
	GetD3D10Device()->VSGetSamplers(StartSlot, NumSamplers, ppSamplers);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::GetPredication(THIS_
            /* [annotation] */ 
            __out_opt  D3D10Base::ID3D10Predicate **ppPredicate,
            /* [annotation] */ 
            __out_opt  BOOL *pPredicateValue)
{
	LogDebug("ID3D10Device::GetPredication called\n");
	
	GetD3D10Device()->GetPredication(ppPredicate, pPredicateValue);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::GSGetShaderResources(THIS_
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - 1 )  UINT StartSlot,
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT - StartSlot )  UINT NumViews,
            /* [annotation] */ 
            __out_ecount(NumViews)  D3D10Base::ID3D10ShaderResourceView **ppShaderResourceViews)
{
	LogDebug("ID3D10Device::GSGetShaderResources called\n");
	
	GetD3D10Device()->GSGetShaderResources(StartSlot, NumViews, ppShaderResourceViews);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::GSGetSamplers(THIS_
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT - 1 )  UINT StartSlot,
            /* [annotation] */ 
            __in_range( 0, D3D10_COMMONSHADER_SAMPLER_SLOT_COUNT - StartSlot )  UINT NumSamplers,
            /* [annotation] */ 
            __out_ecount(NumSamplers)  D3D10Base::ID3D10SamplerState **ppSamplers)
{
	LogDebug("ID3D10Device::GSGetSamplers called\n");
	
	GetD3D10Device()->GSGetSamplers(StartSlot, NumSamplers, ppSamplers);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::OMGetRenderTargets(THIS_
            /* [annotation] */ 
            __in_range( 0, D3D10_SIMULTANEOUS_RENDER_TARGET_COUNT )  UINT NumViews,
            /* [annotation] */ 
            __out_ecount_opt(NumViews)  D3D10Base::ID3D10RenderTargetView **ppRenderTargetViews,
            /* [annotation] */ 
            __out_opt  D3D10Base::ID3D10DepthStencilView **ppDepthStencilView)
{
	LogDebug("ID3D10Device::OMGetRenderTargets called\n");
	
	GetD3D10Device()->OMGetRenderTargets(NumViews, ppRenderTargetViews, ppDepthStencilView);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::OMGetBlendState(THIS_
            /* [annotation] */ 
            __out_opt  D3D10Base::ID3D10BlendState **ppBlendState,
            /* [annotation] */ 
            __out_opt  FLOAT BlendFactor[ 4 ],
            /* [annotation] */ 
            __out_opt  UINT *pSampleMask)
{
	LogDebug("ID3D10Device::OMGetBlendState called\n");
	
	GetD3D10Device()->OMGetBlendState(ppBlendState, BlendFactor, pSampleMask);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::OMGetDepthStencilState(THIS_
            /* [annotation] */ 
            __out_opt  D3D10Base::ID3D10DepthStencilState **ppDepthStencilState,
            /* [annotation] */ 
            __out_opt  UINT *pStencilRef)
{
	LogDebug("ID3D10Device::OMGetDepthStencilState called\n");
	
	GetD3D10Device()->OMGetDepthStencilState(ppDepthStencilState, pStencilRef);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::SOGetTargets(THIS_
            /* [annotation] */ 
            __in_range( 0, D3D10_SO_BUFFER_SLOT_COUNT )  UINT NumBuffers,
            /* [annotation] */ 
            __out_ecount_opt(NumBuffers)  D3D10Base::ID3D10Buffer **ppSOTargets,
            /* [annotation] */ 
            __out_ecount_opt(NumBuffers)  UINT *pOffsets)
{
	LogDebug("ID3D10Device::SOGetTargets called\n");
	
	GetD3D10Device()->SOGetTargets(NumBuffers, ppSOTargets, pOffsets);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::RSGetState(THIS_
            /* [annotation] */ 
            __out  D3D10Base::ID3D10RasterizerState **ppRasterizerState)
{
	LogDebug("ID3D10Device::RSGetState called\n");
	
	GetD3D10Device()->RSGetState(ppRasterizerState);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::RSGetViewports(THIS_
            /* [annotation] */ 
            __inout /*_range(0, D3D10_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE )*/   UINT *NumViewports,
            /* [annotation] */ 
            __out_ecount_opt(*NumViewports)  D3D10Base::D3D10_VIEWPORT *pViewports)
{
	LogDebug("ID3D10Device::RSGetViewports called\n");
	
	GetD3D10Device()->RSGetViewports(NumViewports, pViewports);
	if (gLogFile)
	{
		if (pViewports)
		{
			for (UINT i = 0; i < *NumViewports; ++i)
			{
				LogDebug("  viewport #%d: TopLeft=(%d,%d), Width=%d, Height=%d, MinDepth=%f, MaxDepth=%f\n", i,
					pViewports[i].TopLeftX, pViewports[i].TopLeftY, pViewports[i].Width,
					pViewports[i].Height, pViewports[i].MinDepth, pViewports[i].MaxDepth);
			}
		}
		LogDebug("  returns NumViewports = %d\n", *NumViewports);
	}
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::RSGetScissorRects(THIS_
            /* [annotation] */ 
            __inout /*_range(0, D3D10_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE )*/   UINT *NumRects,
            /* [annotation] */ 
            __out_ecount_opt(*NumRects)  D3D10Base::D3D10_RECT *pRects)
{
	LogDebug("ID3D10Device::RSGetScissorRects called\n");
	
	GetD3D10Device()->RSGetScissorRects(NumRects, pRects);
}
        
STDMETHODIMP D3D10Wrapper::ID3D10Device::GetDeviceRemovedReason(THIS)
{
	LogDebug("ID3D10Device::GetDeviceRemovedReason called\n");
	
	HRESULT hr = GetD3D10Device()->GetDeviceRemovedReason();
	LogDebug("  returns result = %x\n", hr);
	
	return hr;
}
        
STDMETHODIMP D3D10Wrapper::ID3D10Device::SetExceptionMode(THIS_
            UINT RaiseFlags)
{
	LogDebug("ID3D10Device::SetExceptionMode called with RaiseFlags=%x\n", RaiseFlags);
	
	return GetD3D10Device()->SetExceptionMode(RaiseFlags);
}
        
STDMETHODIMP_(UINT) D3D10Wrapper::ID3D10Device::GetExceptionMode(THIS)
{
	LogDebug("ID3D10Device::GetExceptionMode called\n");
	
	return GetD3D10Device()->GetExceptionMode();
}
                
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::ClearState(THIS)
{
	LogDebug("ID3D10Device::ClearState called\n");
	
	GetD3D10Device()->ClearState();
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::Flush(THIS)
{
	LogDebug("ID3D10Device::Flush called\n");
	
	GetD3D10Device()->Flush();
}
        
STDMETHODIMP D3D10Wrapper::ID3D10Device::CreateBuffer(THIS_
            /* [annotation] */ 
            __in  const D3D10Base::D3D10_BUFFER_DESC *pDesc,
            /* [annotation] */ 
            __in_opt  const D3D10Base::D3D10_SUBRESOURCE_DATA *pInitialData,
            /* [annotation] */ 
            __out_opt  D3D10Base::ID3D10Buffer **ppBuffer)
{
	LogDebug("ID3D10Device::CreateBuffer called\n");
	
	return GetD3D10Device()->CreateBuffer(pDesc, pInitialData, ppBuffer);
}
        
STDMETHODIMP D3D10Wrapper::ID3D10Device::CreateTexture1D(THIS_
            /* [annotation] */ 
            __in  const D3D10Base::D3D10_TEXTURE1D_DESC *pDesc,
            /* [annotation] */ 
            __in_xcount_opt(pDesc->MipLevels * pDesc->ArraySize)  const D3D10Base::D3D10_SUBRESOURCE_DATA *pInitialData,
            /* [annotation] */ 
            __out  D3D10Base::ID3D10Texture1D **ppTexture1D)
{
	LogDebug("ID3D10Device::CreateTexture1D called with\n");
	if (pDesc) LogDebug("  Width = %d\n", pDesc->Width);
	if (pDesc) LogDebug("  MipLevels = %d, ArraySize = %d\n", pDesc->MipLevels, pDesc->ArraySize);
	if (pDesc) LogDebug("  Format = %x\n", pDesc->Format);
	
	return GetD3D10Device()->CreateTexture1D(pDesc, pInitialData, ppTexture1D);
}
        
STDMETHODIMP D3D10Wrapper::ID3D10Device::CreateTexture2D(THIS_
            /* [annotation] */ 
            __in  const D3D10Base::D3D10_TEXTURE2D_DESC *pDesc,
            /* [annotation] */ 
            __in_xcount_opt(pDesc->MipLevels * pDesc->ArraySize)  const D3D10Base::D3D10_SUBRESOURCE_DATA *pInitialData,
            /* [annotation] */ 
            __out  D3D10Base::ID3D10Texture2D **ppTexture2D)
{
	LogDebug("ID3D10Device::CreateTexture2D called with\n");
	LogDebug("  Width = %d, Height = %d\n", pDesc->Width, pDesc->Height);
	LogDebug("  MipLevels = %d, ArraySize = %d\n", pDesc->MipLevels, pDesc->ArraySize);
	LogDebug("  Format = %x, SampleDesc.Count = %u, SampleDesc.Quality = %u\n",
			pDesc->Format, pDesc->SampleDesc.Count, pDesc->SampleDesc.Quality);
	
	return GetD3D10Device()->CreateTexture2D(pDesc, pInitialData, ppTexture2D);
}
        
STDMETHODIMP D3D10Wrapper::ID3D10Device::CreateTexture3D(THIS_
            /* [annotation] */ 
            __in  const D3D10Base::D3D10_TEXTURE3D_DESC *pDesc,
            /* [annotation] */ 
            __in_xcount_opt(pDesc->MipLevels)  const D3D10Base::D3D10_SUBRESOURCE_DATA *pInitialData,
            /* [annotation] */ 
            __out  D3D10Base::ID3D10Texture3D **ppTexture3D)
{
	LogDebug("ID3D10Device::CreateTexture3D called\n");
	
	return GetD3D10Device()->CreateTexture3D(pDesc, pInitialData, ppTexture3D);
}
        
STDMETHODIMP D3D10Wrapper::ID3D10Device::CreateShaderResourceView(THIS_
            /* [annotation] */ 
            __in  D3D10Base::ID3D10Resource *pResource,
            /* [annotation] */ 
            __in_opt  const D3D10Base::D3D10_SHADER_RESOURCE_VIEW_DESC *pDesc,
            /* [annotation] */ 
            __out_opt  D3D10Base::ID3D10ShaderResourceView **ppSRView)
{
	LogDebug("ID3D10Device::CreateShaderResourceView called\n");
	
	return GetD3D10Device()->CreateShaderResourceView(pResource, pDesc, ppSRView);
}
        
STDMETHODIMP D3D10Wrapper::ID3D10Device::CreateRenderTargetView(THIS_
            /* [annotation] */ 
            __in  D3D10Base::ID3D10Resource *pResource,
            /* [annotation] */ 
            __in_opt  const D3D10Base::D3D10_RENDER_TARGET_VIEW_DESC *pDesc,
            /* [annotation] */ 
            __out_opt  D3D10Base::ID3D10RenderTargetView **ppRTView)
{
	LogDebug("ID3D10Device::CreateRenderTargetView called\n");
	
	return GetD3D10Device()->CreateRenderTargetView(pResource, pDesc, ppRTView);
}
        
STDMETHODIMP D3D10Wrapper::ID3D10Device::CreateDepthStencilView(THIS_
            /* [annotation] */ 
            __in  D3D10Base::ID3D10Resource *pResource,
            /* [annotation] */ 
            __in_opt  const D3D10Base::D3D10_DEPTH_STENCIL_VIEW_DESC *pDesc,
            /* [annotation] */ 
            __out_opt  D3D10Base::ID3D10DepthStencilView **ppDepthStencilView)
{
	LogDebug("ID3D10Device::CreateDepthStencilView called\n");
	
	return GetD3D10Device()->CreateDepthStencilView(pResource, pDesc, ppDepthStencilView);
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
	LogDebug("ID3D10Device::CreateInputLayout called\n");
	
	return GetD3D10Device()->CreateInputLayout(pInputElementDescs, NumElements, pShaderBytecodeWithInputSignature,
		BytecodeLength, ppInputLayout);
}
        
// For any given vertex or pixel shader from the ShaderFixes folder, we need to track them at load time so
// that we can associate a given active shader with an override file.  This allows us to reload the shaders
// dynamically, and do on-the-fly fix testing.
// ShaderModel is usually something like "vs_5_0", but "bin" is a valid ShaderModel string, and tells the 
// reloader to disassemble the .bin file to determine the shader model.

static void RegisterForReload(D3D10Base::ID3D10DeviceChild* ppShader,
	UINT64 hash, wstring shaderType, string shaderModel, D3D10Base::ID3DBlob* byteCode, FILETIME timeStamp)
{
	LogInfo("    shader registered for possible reloading: %016llx_%ls as %s\n", hash, shaderType.c_str(), shaderModel.c_str());

	G->mReloadedShaders[ppShader].hash = hash;
	G->mReloadedShaders[ppShader].shaderType = shaderType;
	G->mReloadedShaders[ppShader].shaderModel = shaderModel;
	G->mReloadedShaders[ppShader].byteCode = byteCode;
	G->mReloadedShaders[ppShader].timeStamp = timeStamp;
	G->mReloadedShaders[ppShader].replacement = NULL;
}

// Fairly bold new strategy here for ReplaceShader. 
// This is called at launch to replace any shaders that we might want patched to fix problems.
// It would previously use both ShaderCache, and ShaderFixes both to fix shaders, but this is
// problematic in that broken shaders dumped as part of universal cache could be buggy, and generated
// visual anomolies.  Moreover, we don't really want every file to patched, just the ones we need.

// I'm moving to a model where only stuff in ShaderFixes is active, and stuff in ShaderCache is for reference.
// This will allow us to dump and use the ShaderCache for offline fixes, looking for similar fix patterns, and
// also make them live by moving them to ShaderFixes.
// For auto-fixed shaders- rather than leave them in ShaderCache, when they are fixed, we'll move them into 
// ShaderFixes as being live.  

// Only used in CreateVertexShader and CreatePixelShader

static char *ReplaceShader(D3D10Base::ID3D10Device *realDevice, UINT64 hash, const wchar_t *shaderType, const void *pShaderBytecode,
	SIZE_T BytecodeLength, SIZE_T &pCodeSize, string &foundShaderModel, FILETIME &timeStamp, void **zeroShader)
{
	foundShaderModel = "";
	timeStamp = { 0 };

	*zeroShader = 0;
	char *pCode = 0;
	wchar_t val[MAX_PATH];

	if (G->SHADER_PATH[0] && G->SHADER_CACHE_PATH[0])
	{
		// Export every shader seen as an ASM file.
		if (G->EXPORT_SHADERS)
		{
			D3D10Base::ID3DBlob *disassembly;
			HRESULT r = D3D10Base::D3DDisassemble(pShaderBytecode, BytecodeLength,
				D3D_DISASM_ENABLE_DEFAULT_VALUE_PRINTS,
				0, &disassembly);
			if (r != S_OK)
			{
				LogInfo("  disassembly failed.\n");
			}
			else
			{
				wsprintf(val, L"%ls\\%08lx%08lx-%ls.txt", G->SHADER_CACHE_PATH, (UINT32)(hash >> 32), (UINT32)(hash), shaderType);
				FILE *f = _wfsopen(val, L"rb", _SH_DENYNO);
				bool exists = false;
				if (f)
				{
					int cnt = 0;
					while (f)
					{
						// Check if same file.
						fseek(f, 0, SEEK_END);
						long dataSize = ftell(f);
						rewind(f);
						char *buf = new char[dataSize];
						fread(buf, 1, dataSize, f);
						fclose(f);
						// Considder same file regardless of whether it has a NULL terminator or not
						// to avoid creating identical asm files if an older version of 3Dmigoto has
						// previously dumped out the asm file with a NULL terminator.
						if ((dataSize == disassembly->GetBufferSize() || dataSize == (disassembly->GetBufferSize() - 1))
							&& !memcmp(disassembly->GetBufferPointer(), buf, disassembly->GetBufferSize() - 1))
							exists = true;
						delete buf;
						if (exists) break;
						wsprintf(val, L"%ls\\%08lx%08lx-%ls_%d.txt", G->SHADER_CACHE_PATH, (UINT32)(hash >> 32), (UINT32)hash, shaderType, ++cnt);
						f = _wfsopen(val, L"rb", _SH_DENYNO);
					}
				}
				if (!exists)
				{
					FILE *f;
					_wfopen_s(&f, val, L"wb");
					if (gLogFile)
					{
						char fileName[MAX_PATH];
						wcstombs(fileName, val, MAX_PATH);
						if (f)
							LogInfo("    storing disassembly to %s\n", fileName);
						else
							LogInfo("    error storing disassembly to %s\n", fileName);
					}
					if (f)
					{
						// Size - 1 to strip NULL terminator
						fwrite(disassembly->GetBufferPointer(), 1, (disassembly->GetBufferSize() - 1), f);
						fclose(f);
					}
				}
				disassembly->Release();
			}
		}

		// Read binary compiled shader.
		wsprintf(val, L"%ls\\%08lx%08lx-%ls_replace.bin", G->SHADER_PATH, (UINT32)(hash >> 32), (UINT32)(hash), shaderType);
		HANDLE f = CreateFile(val, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (f != INVALID_HANDLE_VALUE)
		{
			LogInfo("    Replacement binary shader found.\n");

			DWORD codeSize = GetFileSize(f, 0);
			pCode = new char[codeSize];
			DWORD readSize;
			FILETIME ftWrite;
			if (!ReadFile(f, pCode, codeSize, &readSize, 0)
				|| !GetFileTime(f, NULL, NULL, &ftWrite)
				|| codeSize != readSize)
			{
				LogInfo("    Error reading file.\n");
				delete pCode; pCode = 0;
				CloseHandle(f);
			}
			else
			{
				pCodeSize = codeSize;
				LogInfo("    Bytecode loaded. Size = %Iu\n", pCodeSize);
				CloseHandle(f);

				foundShaderModel = "bin";		// tag it as reload candidate, but needing disassemble

				// For timestamp, we need the time stamp on the .txt file for comparison, not this .bin file.
				wchar_t *end = wcsstr(val, L".bin");
				wcscpy_s(end, sizeof(L".bin"), L".txt");
				f = CreateFile(val, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
				if ((f != INVALID_HANDLE_VALUE)
					&& GetFileTime(f, NULL, NULL, &ftWrite))
				{
					timeStamp = ftWrite;
					CloseHandle(f);
				}
			}
		}

		// Load previously created HLSL shaders, but only from ShaderFixes
		if (!pCode)
		{
			wsprintf(val, L"%ls\\%08lx%08lx-%ls_replace.txt", G->SHADER_PATH, (UINT32)(hash >> 32), (UINT32)(hash), shaderType);
			f = CreateFile(val, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
			if (f != INVALID_HANDLE_VALUE)
			{
				LogInfo("    Replacement shader found. Loading replacement HLSL code.\n");

				DWORD srcDataSize = GetFileSize(f, 0);
				char *srcData = new char[srcDataSize];
				DWORD readSize;
				FILETIME ftWrite;
				if (!ReadFile(f, srcData, srcDataSize, &readSize, 0)
					|| !GetFileTime(f, NULL, NULL, &ftWrite)
					|| srcDataSize != readSize)
					LogInfo("    Error reading file.\n");
				CloseHandle(f);
				LogInfo("    Source code loaded. Size = %d\n", srcDataSize);

				// Disassemble old shader to get shader model.
				D3D10Base::ID3DBlob *disassembly;
				HRESULT ret = D3D10Base::D3DDisassemble(pShaderBytecode, BytecodeLength,
					D3D_DISASM_ENABLE_DEFAULT_VALUE_PRINTS, 0, &disassembly);
				if (ret != S_OK)
				{
					LogInfo("    disassembly of original shader failed.\n");

					delete srcData;
				}
				else
				{
					// Read shader model. This is the first not commented line.
					char *pos = (char *)disassembly->GetBufferPointer();
					char *end = pos + disassembly->GetBufferSize();
					while (pos[0] == '/' && pos < end)
					{
						while (pos[0] != 0x0a && pos < end) pos++;
						pos++;
					}
					// Extract model.
					char *eol = pos;
					while (eol[0] != 0x0a && pos < end) eol++;
					string shaderModel(pos, eol);

					// Any HLSL compiled shaders are reloading candidates, if moved to ShaderFixes
					foundShaderModel = shaderModel;
					timeStamp = ftWrite;

					// Compile replacement.
					LogInfo("    compiling replacement HLSL code with shader model %s\n", shaderModel.c_str());

					D3D10Base::ID3DBlob *pErrorMsgs;
					D3D10Base::ID3DBlob *pCompiledOutput = 0;
					ret = D3D10Base::D3DCompile(srcData, srcDataSize, "wrapper1349", 0, ((D3D10Base::ID3DInclude*)(UINT_PTR)1),
						"main", shaderModel.c_str(), D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &pCompiledOutput, &pErrorMsgs);
					delete srcData; srcData = 0;
					disassembly->Release();
					if (pCompiledOutput)
					{
						pCodeSize = pCompiledOutput->GetBufferSize();
						pCode = new char[pCodeSize];
						memcpy(pCode, pCompiledOutput->GetBufferPointer(), pCodeSize);
						pCompiledOutput->Release(); pCompiledOutput = 0;
					}

					LogInfo("    compile result of replacement HLSL shader: %x\n", ret);

					if (gLogFile && pErrorMsgs)
					{
						LPVOID errMsg = pErrorMsgs->GetBufferPointer();
						SIZE_T errSize = pErrorMsgs->GetBufferSize();
						LogInfo("--------------------------------------------- BEGIN ---------------------------------------------\n");
						fwrite(errMsg, 1, errSize - 1, gLogFile);
						LogInfo("---------------------------------------------- END ----------------------------------------------\n");
						pErrorMsgs->Release();
					}

					// Cache binary replacement.
					if (G->CACHE_SHADERS && pCode)
					{
						wsprintf(val, L"%ls\\%08lx%08lx-%ls_replace.bin", G->SHADER_PATH, (UINT32)(hash >> 32), (UINT32)(hash), shaderType);
						FILE *fw;
						_wfopen_s(&fw, val, L"wb");
						if (gLogFile)
						{
							char fileName[MAX_PATH];
							wcstombs(fileName, val, MAX_PATH);
							if (fw)
								LogInfo("    storing compiled shader to %s\n", fileName);
							else
								LogInfo("    error writing compiled shader to %s\n", fileName);
						}
						if (fw)
						{
							fwrite(pCode, 1, pCodeSize, fw);
							fclose(fw);
						}
					}
				}
			}
		}
	}

	// Shader hacking?
	if (G->SHADER_PATH[0] && G->SHADER_CACHE_PATH[0] && ((G->EXPORT_HLSL >= 1) || G->FIX_SV_Position || G->FIX_Light_Position || G->FIX_Recompile_VS) && !pCode)
	{
		// Skip?
		wsprintf(val, L"%ls\\%08lx%08lx-%ls_bad.txt", G->SHADER_PATH, (UINT32)(hash >> 32), (UINT32)(hash), shaderType);
		HANDLE hFind = CreateFile(val, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hFind != INVALID_HANDLE_VALUE)
		{
			char fileName[MAX_PATH];
			wcstombs(fileName, val, MAX_PATH);
			LogInfo("    skipping shader marked bad. %s\n", fileName);
			CloseHandle(hFind);
		}
		else
		{
			D3D10Base::ID3DBlob *disassembly = 0;
			FILE *fw = 0;
			string shaderModel = "";

			// Store HLSL export files in ShaderCache, auto-Fixed shaders in ShaderFixes
			if (G->EXPORT_HLSL >= 1)
				wsprintf(val, L"%ls\\%08lx%08lx-%ls_replace.txt", G->SHADER_CACHE_PATH, (UINT32)(hash >> 32), (UINT32)hash, shaderType);
			else
				wsprintf(val, L"%ls\\%08lx%08lx-%ls_replace.txt", G->SHADER_PATH, (UINT32)(hash >> 32), (UINT32)hash, shaderType);

			// If we can open the file already, it exists, and thus we should skip doing this slow operation again.
			errno_t err = _wfopen_s(&fw, val, L"rb");
			if (err == 0)
			{
				fclose(fw);
				return 0;	// Todo: what about zero shader section?
			}

			// Disassemble old shader for fixing.
			HRESULT ret = D3D10Base::D3DDisassemble(pShaderBytecode, BytecodeLength,
				D3D_DISASM_ENABLE_DEFAULT_VALUE_PRINTS, 0, &disassembly);
			if (ret != S_OK)
			{
				LogInfo("    disassembly of original shader failed.\n");
			}
			else
			{
				// Decompile code.
				LogInfo("    creating HLSL representation.\n");

				bool patched = false;
				bool errorOccurred = false;
				ParseParameters p;
				p.bytecode = pShaderBytecode;
				p.decompiled = (const char *)disassembly->GetBufferPointer();
				p.decompiledSize = disassembly->GetBufferSize();
				p.recompileVs = G->FIX_Recompile_VS;
				p.fixSvPosition = G->FIX_SV_Position;
				p.ZRepair_Dependencies1 = G->ZRepair_Dependencies1;
				p.ZRepair_Dependencies2 = G->ZRepair_Dependencies2;
				p.ZRepair_DepthTexture1 = G->ZRepair_DepthTexture1;
				p.ZRepair_DepthTexture2 = G->ZRepair_DepthTexture2;
				p.ZRepair_DepthTextureReg1 = G->ZRepair_DepthTextureReg1;
				p.ZRepair_DepthTextureReg2 = G->ZRepair_DepthTextureReg2;
				p.ZRepair_ZPosCalc1 = G->ZRepair_ZPosCalc1;
				p.ZRepair_ZPosCalc2 = G->ZRepair_ZPosCalc2;
				p.ZRepair_PositionTexture = G->ZRepair_PositionTexture;
				p.ZRepair_DepthBuffer = (G->ZBufferHashToInject != 0);
				p.ZRepair_WorldPosCalc = G->ZRepair_WorldPosCalc;
				p.BackProject_Vector1 = G->BackProject_Vector1;
				p.BackProject_Vector2 = G->BackProject_Vector2;
				p.ObjectPos_ID1 = G->ObjectPos_ID1;
				p.ObjectPos_ID2 = G->ObjectPos_ID2;
				p.ObjectPos_MUL1 = G->ObjectPos_MUL1;
				p.ObjectPos_MUL2 = G->ObjectPos_MUL2;
				p.MatrixPos_ID1 = G->MatrixPos_ID1;
				p.MatrixPos_MUL1 = G->MatrixPos_MUL1;
				p.InvTransforms = G->InvTransforms;
				p.fixLightPosition = G->FIX_Light_Position;
				p.ZeroOutput = false;
				const string decompiledCode = DecompileBinaryHLSL(p, patched, shaderModel, errorOccurred);
				if (!decompiledCode.size())
				{
					LogInfo("    error while decompiling.\n");

					return 0;
				}

				if (!errorOccurred && ((G->EXPORT_HLSL >= 1) || (G->EXPORT_FIXED && patched)))
				{
					_wfopen_s(&fw, val, L"wb");
					if (gLogFile)
					{
						char fileName[MAX_PATH];
						wcstombs(fileName, val, MAX_PATH);
						if (fw)
							LogInfo("    storing patched shader to %s\n", fileName);
						else
							LogInfo("    error storing patched shader to %s\n", fileName);
					}
					if (fw)
					{
						// Save decompiled HLSL code to that new file.
						fwrite(decompiledCode.c_str(), 1, decompiledCode.size(), fw);

						// Now also write the ASM text to the shader file as a set of comments at the bottom.
						// That will make the ASM code the master reference for fixing shaders, and should be more 
						// convenient, especially in light of the numerous decompiler bugs we see.
						if (G->EXPORT_HLSL >= 2)
						{
							fprintf_s(fw, "\n\n/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Original ASM ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
							// Size - 1 to strip NULL terminator
							fwrite(disassembly->GetBufferPointer(), 1, disassembly->GetBufferSize() - 1, fw);
							fprintf_s(fw, "\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/\n");

						}

						if (disassembly) disassembly->Release(); disassembly = 0;
					}
				}

				// Let's re-compile every time we create a new one, regardless.  Previously this would only re-compile
				// after auto-fixing shaders. This makes shader Decompiler errors more obvious.
				if (!errorOccurred)
				{
					LogInfo("    compiling fixed HLSL code with shader model %s, size = %Iu\n", shaderModel.c_str(), decompiledCode.size());

					D3D10Base::ID3DBlob *pErrorMsgs;
					D3D10Base::ID3DBlob *pCompiledOutput = 0;
					ret = D3D10Base::D3DCompile(decompiledCode.c_str(), decompiledCode.size(), "wrapper1349", 0, ((D3D10Base::ID3DInclude*)(UINT_PTR)1),
						"main", shaderModel.c_str(), D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &pCompiledOutput, &pErrorMsgs);
					LogInfo("    compile result of fixed HLSL shader: %x\n", ret);

					if (gLogFile && pErrorMsgs)
					{
						LPVOID errMsg = pErrorMsgs->GetBufferPointer();
						SIZE_T errSize = pErrorMsgs->GetBufferSize();
						LogInfo("--------------------------------------------- BEGIN ---------------------------------------------\n");
						fwrite(errMsg, 1, errSize - 1, gLogFile);
						LogInfo("------------------------------------------- HLSL code -------------------------------------------\n");
						fwrite(decompiledCode.c_str(), 1, decompiledCode.size(), gLogFile);
						LogInfo("\n---------------------------------------------- END ----------------------------------------------\n");

						// And write the errors to the HLSL file as comments too, as a more convenient spot to see them.
						fprintf_s(fw, "\n\n/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~ HLSL errors ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
						fwrite(errMsg, 1, errSize - 1, fw);
						fprintf_s(fw, "~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/\n");

						pErrorMsgs->Release();
					}

					// If requested by .ini, also write the newly re-compiled assembly code to the file.  This gives a direct
					// comparison between original ASM, and recompiled ASM.
					if ((G->EXPORT_HLSL >= 3) && pCompiledOutput)
					{
						HRESULT ret = D3D10Base::D3DDisassemble(pCompiledOutput->GetBufferPointer(), pCompiledOutput->GetBufferSize(),
							D3D_DISASM_ENABLE_DEFAULT_VALUE_PRINTS, 0, &disassembly);
						if (ret != S_OK)
						{
							LogInfo("    disassembly of recompiled shader failed.\n");
						}
						else
						{
							fprintf_s(fw, "\n\n/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Recompiled ASM ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
							// Size - 1 to strip NULL terminator
							fwrite(disassembly->GetBufferPointer(), 1, disassembly->GetBufferSize() - 1, fw);
							fprintf_s(fw, "\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/\n");
							disassembly->Release(); disassembly = 0;
						}
					}

					if (pCompiledOutput)
					{
						// If the shader has been auto-fixed, return it as the live shader.  
						// For just caching shaders, we return zero so it won't affect game visuals.
						if (patched)
						{
							pCodeSize = pCompiledOutput->GetBufferSize();
							pCode = new char[pCodeSize];
							memcpy(pCode, pCompiledOutput->GetBufferPointer(), pCodeSize);
						}
						pCompiledOutput->Release(); pCompiledOutput = 0;
					}
				}
			}

			if (fw)
			{
				// Any HLSL compiled shaders are reloading candidates, if moved to ShaderFixes
				FILETIME ftWrite;
				GetFileTime(fw, NULL, NULL, &ftWrite);
				foundShaderModel = shaderModel;
				timeStamp = ftWrite;

				fclose(fw);
			}
		}
	}

	// Zero shader?
	if (G->marking_mode == MARKING_MODE_ZERO)
	{
		// Disassemble old shader for fixing.
		D3D10Base::ID3DBlob *disassembly;
		HRESULT ret = D3D10Base::D3DDisassemble(pShaderBytecode, BytecodeLength,
			D3D_DISASM_ENABLE_DEFAULT_VALUE_PRINTS, 0, &disassembly);
		if (ret != S_OK)
		{
			LogInfo("    disassembly of original shader failed.\n");
		}
		else
		{
			// Decompile code.
			LogInfo("    creating HLSL representation of zero output shader.\n");

			bool patched = false;
			string shaderModel;
			bool errorOccurred = false;
			ParseParameters p;
			p.bytecode = pShaderBytecode;
			p.decompiled = (const char *)disassembly->GetBufferPointer();
			p.decompiledSize = disassembly->GetBufferSize();
			p.recompileVs = G->FIX_Recompile_VS;
			p.fixSvPosition = false;
			p.ZeroOutput = true;
			const string decompiledCode = DecompileBinaryHLSL(p, patched, shaderModel, errorOccurred);
			disassembly->Release();
			if (!decompiledCode.size())
			{
				LogInfo("    error while decompiling.\n");

				return 0;
			}
			if (!errorOccurred)
			{
				// Compile replacement.
				LogInfo("    compiling zero HLSL code with shader model %s, size = %Iu\n", shaderModel.c_str(), decompiledCode.size());

				D3D10Base::ID3DBlob *pErrorMsgs;
				D3D10Base::ID3DBlob *pCompiledOutput = 0;
				ret = D3D10Base::D3DCompile(decompiledCode.c_str(), decompiledCode.size(), "wrapper1349", 0, ((D3D10Base::ID3DInclude*)(UINT_PTR)1),
					"main", shaderModel.c_str(), D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &pCompiledOutput, &pErrorMsgs);
				LogInfo("    compile result of zero HLSL shader: %x\n", ret);

				if (pCompiledOutput)
				{
					SIZE_T codeSize = pCompiledOutput->GetBufferSize();
					char *code = new char[codeSize];
					memcpy(code, pCompiledOutput->GetBufferPointer(), codeSize);
					pCompiledOutput->Release(); pCompiledOutput = 0;
					if (!wcscmp(shaderType, L"vs"))
					{
						D3D10Base::ID3D10VertexShader *zeroVertexShader;
						HRESULT hr = realDevice->CreateVertexShader(code, codeSize, &zeroVertexShader);
						if (hr == S_OK)
							*zeroShader = zeroVertexShader;
					}
					else if (!wcscmp(shaderType, L"ps"))
					{
						D3D10Base::ID3D10PixelShader *zeroPixelShader;
						HRESULT hr = realDevice->CreatePixelShader(code, codeSize, &zeroPixelShader);
						if (hr == S_OK)
							*zeroShader = zeroPixelShader;
					}
					delete code;
				}

				if (gLogFile && pErrorMsgs)
				{
					LPVOID errMsg = pErrorMsgs->GetBufferPointer();
					SIZE_T errSize = pErrorMsgs->GetBufferSize();
					LogInfo("--------------------------------------------- BEGIN ---------------------------------------------\n");
					fwrite(errMsg, 1, errSize - 1, gLogFile);
					LogInfo("------------------------------------------- HLSL code -------------------------------------------\n");
					fwrite(decompiledCode.c_str(), 1, decompiledCode.size(), gLogFile);
					LogInfo("\n---------------------------------------------- END ----------------------------------------------\n");
					pErrorMsgs->Release();
				}
			}
		}
	}

	return pCode;
}

static bool NeedOriginalShader(UINT64 hash)
{
	ShaderOverride *shaderOverride;
	ShaderOverrideMap::iterator i;

	if (G->hunting && (G->marking_mode == MARKING_MODE_ORIGINAL || G->config_reloadable))
		return true;

	i = G->mShaderOverrideMap.find(hash);
	if (i == G->mShaderOverrideMap.end())
		return false;
	shaderOverride = &i->second;

	if ((shaderOverride->depth_filter == DepthBufferFilter::DEPTH_ACTIVE) ||
		(shaderOverride->depth_filter == DepthBufferFilter::DEPTH_INACTIVE)) {
		return true;
	}

	return false;
}

// Keep the original shader around if it may be needed by a filter in a
// [ShaderOverride] section, or if hunting is enabled and either the
// marking_mode=original, or reload_config support is enabled
static void KeepOriginalShader(D3D10Wrapper::ID3D10Device *device, UINT64 hash,
	D3D10Base::ID3D10VertexShader *pVertexShader,
	D3D10Base::ID3D10PixelShader *pPixelShader,
	const void *pShaderBytecode,
	SIZE_T BytecodeLength)
{
	if (!NeedOriginalShader(hash))
		return;

	LogInfo("    keeping original shader for filtering: %016llx\n", hash);

	if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
	if (pVertexShader) {
		D3D10Base::ID3D10VertexShader *originalShader;
		device->GetD3D10Device()->CreateVertexShader(pShaderBytecode, BytecodeLength, &originalShader);
		G->mOriginalVertexShaders[pVertexShader] = originalShader;
	}
	else if (pPixelShader) {
		D3D10Base::ID3D10PixelShader *originalShader;
		device->GetD3D10Device()->CreatePixelShader(pShaderBytecode, BytecodeLength, &originalShader);
		G->mOriginalPixelShaders[pPixelShader] = originalShader;
	}
	if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
}

STDMETHODIMP D3D10Wrapper::ID3D10Device::CreateVertexShader(THIS_
            /* [annotation] */ 
            __in  const void *pShaderBytecode,
            /* [annotation] */ 
            __in  SIZE_T BytecodeLength,
            /* [annotation] */ 
            __out_opt  D3D10Base::ID3D10VertexShader **ppVertexShader)
{
	LogInfo("ID3D10Device::CreateVertexShader called with BytecodeLength = %Iu, handle = %p\n", BytecodeLength, pShaderBytecode);

	HRESULT hr = -1;
	UINT64 hash;
	string shaderModel;
	SIZE_T replaceShaderSize;
	FILETIME ftWrite;
	D3D10Base::ID3D10VertexShader *zeroShader = 0;

	if (pShaderBytecode && ppVertexShader)
	{
		// Calculate hash
		hash = fnv_64_buf(pShaderBytecode, BytecodeLength);
		LogInfo("  bytecode hash = %016llx\n", hash);
		LogInfo("  bytecode hash = %08lx%08lx\n", (UINT32)(hash >> 32), (UINT32)hash);

		// Preloaded shader? 
		{
			PreloadVertexShaderMap::iterator i = G->mPreloadedVertexShaders.find(hash);
			if (i != G->mPreloadedVertexShaders.end())
			{
				*ppVertexShader = i->second;
				ULONG cnt = (*ppVertexShader)->AddRef();
				hr = S_OK;
				LogInfo("    shader assigned by preloaded version. ref counter = %d\n", cnt);

				if (G->marking_mode == MARKING_MODE_ZERO)
				{
					char *replaceShader = ReplaceShader(GetD3D10Device(), hash, L"vs", pShaderBytecode, BytecodeLength, replaceShaderSize,
						shaderModel, ftWrite, (void **)&zeroShader);
					delete replaceShader;
				}
				KeepOriginalShader(this, hash, *ppVertexShader, NULL, pShaderBytecode, BytecodeLength);
			}
		}
	}
	if (hr != S_OK && ppVertexShader && pShaderBytecode)
	{
		D3D10Base::ID3D10VertexShader *zeroShader = 0;
		// Not sure why, but blocking the Decompiler from multi-threading prevents a crash.
		// This is just a patch for now.
		if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
		char *replaceShader = ReplaceShader(GetD3D10Device(), hash, L"vs", pShaderBytecode, BytecodeLength, replaceShaderSize,
			shaderModel, ftWrite, (void **)&zeroShader);
		if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
		if (replaceShader)
		{
			// Create the new shader.
			LogDebug("    D3D10Wrapper::ID3D10Device::CreateVertexShader.  Device: %p\n", GetD3D10Device());

			hr = GetD3D10Device()->CreateVertexShader(pShaderBytecode, BytecodeLength, ppVertexShader);
			if (SUCCEEDED(hr))
			{
				LogInfo("    shader successfully replaced.\n");

				if (G->hunting)
				{
					// Hunting mode:  keep byteCode around for possible replacement or marking
					D3D10Base::ID3DBlob* blob;
					D3DCreateBlob(replaceShaderSize, &blob);
					memcpy(blob->GetBufferPointer(), replaceShader, replaceShaderSize);
					RegisterForReload(*ppVertexShader, hash, L"vs", shaderModel, blob, ftWrite);
				}
				KeepOriginalShader(this, hash, *ppVertexShader, NULL, pShaderBytecode, BytecodeLength);
			}
			else
			{
				LogInfo("    error replacing shader.\n");
			}
			delete replaceShader; replaceShader = 0;
		}
	}
	if (hr != S_OK)
	{
		hr = GetD3D10Device()->CreateVertexShader(pShaderBytecode, BytecodeLength, ppVertexShader);

		// When in hunting mode, make a copy of the original binary, regardless.  This can be replaced, but we'll at least
		// have a copy for every shader seen.
		if (G->hunting)
		{
			D3D10Base::ID3DBlob* blob;
			D3DCreateBlob(BytecodeLength, &blob);
			memcpy(blob->GetBufferPointer(), pShaderBytecode, blob->GetBufferSize());
			RegisterForReload(*ppVertexShader, hash, L"vs", "bin", blob, ftWrite);

			// Also add the original shader to the original shaders
			// map so that if it is later replaced marking_mode =
			// original and depth buffer filtering will work:
			if (G->mOriginalVertexShaders.count(*ppVertexShader) == 0)
				G->mOriginalVertexShaders[*ppVertexShader] = *ppVertexShader;
		}
	}
	if (hr == S_OK && ppVertexShader && pShaderBytecode)
	{
		if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
		G->mVertexShaders[*ppVertexShader] = hash;
		LogDebug("    Vertex shader registered: handle = %p, hash = %08lx%08lx\n", *ppVertexShader, (UINT32)(hash >> 32), (UINT32)hash);

		if ((G->marking_mode == MARKING_MODE_ZERO) && zeroShader)
		{
			G->mZeroVertexShaders[*ppVertexShader] = zeroShader;
		}

		CompiledShaderMap::iterator i = G->mCompiledShaderMap.find(hash);
		if (i != G->mCompiledShaderMap.end())
		{
			LogInfo("  shader was compiled from source code %s\n", i->second.c_str());
		}
		if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
	}

	LogInfo("  returns result = %x, handle = %p\n", hr, *ppVertexShader);

	return hr;
}
        
STDMETHODIMP D3D10Wrapper::ID3D10Device::CreateGeometryShader(THIS_
            /* [annotation] */ 
            __in  const void *pShaderBytecode,
            /* [annotation] */ 
            __in  SIZE_T BytecodeLength,
            /* [annotation] */ 
            __out_opt  D3D10Base::ID3D10GeometryShader **ppGeometryShader)
{
	LogDebug("ID3D10Device::CreateGeometryShader called.\n");
	
	return GetD3D10Device()->CreateGeometryShader(pShaderBytecode, BytecodeLength, ppGeometryShader);
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
	LogDebug("ID3D10Device::CreateGeometryShaderWithStreamOutput called\n");
	
	return GetD3D10Device()->CreateGeometryShaderWithStreamOutput(pShaderBytecode, BytecodeLength, pSODeclaration,
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
	// Create the new shader.
	LogInfo("ID3D10Device::CreatePixelShader called with BytecodeLength = %Iu, handle = %p\n", BytecodeLength, pShaderBytecode);

	HRESULT hr = -1;
	UINT64 hash;
	string shaderModel;
	SIZE_T replaceShaderSize;
	FILETIME ftWrite;
	D3D10Base::ID3D10PixelShader *zeroShader = 0;

	if (pShaderBytecode && ppPixelShader)
	{
		// Calculate hash
		hash = fnv_64_buf(pShaderBytecode, BytecodeLength);
		LogInfo("  bytecode hash = %08lx%08lx\n", (UINT32)(hash >> 32), (UINT32)hash);

		// Preloaded shader? 
		{
			PreloadPixelShaderMap::iterator i = G->mPreloadedPixelShaders.find(hash);
			if (i != G->mPreloadedPixelShaders.end())
			{
				*ppPixelShader = i->second;
				ULONG cnt = (*ppPixelShader)->AddRef();
				hr = S_OK;
				LogInfo("    shader assigned by preloaded version. ref counter = %d\n", cnt);

				if (G->marking_mode == MARKING_MODE_ZERO)
				{
					char *replaceShader = ReplaceShader(GetD3D10Device(), hash, L"ps", pShaderBytecode, BytecodeLength, replaceShaderSize,
						shaderModel, ftWrite, (void **)&zeroShader);
					delete replaceShader;
				}
				KeepOriginalShader(this, hash, NULL, *ppPixelShader, pShaderBytecode, BytecodeLength);
			}
		}
	}
	if (hr != S_OK && ppPixelShader && pShaderBytecode)
	{
		// TODO: shouldn't require critical section
		if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
		char *replaceShader = ReplaceShader(GetD3D10Device(), hash, L"ps", pShaderBytecode, BytecodeLength, replaceShaderSize,
			shaderModel, ftWrite, (void **)&zeroShader);
		if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
		if (replaceShader)
		{
			hr = GetD3D10Device()->CreatePixelShader(replaceShader, replaceShaderSize, ppPixelShader);
			if (SUCCEEDED(hr))
			{
				LogInfo("    shader successfully replaced.\n");

				if (G->hunting)
				{
					// Hunting mode:  keep byteCode around for possible replacement or marking
					D3D10Base::ID3DBlob* blob;
					D3DCreateBlob(replaceShaderSize, &blob);
					memcpy(blob->GetBufferPointer(), replaceShader, replaceShaderSize);
					RegisterForReload(*ppPixelShader, hash, L"ps", shaderModel, blob, ftWrite);
				}
				KeepOriginalShader(this, hash, NULL, *ppPixelShader, pShaderBytecode, BytecodeLength);
			}
			else
			{
				LogInfo("    error replacing shader.\n");
			}
			delete replaceShader; replaceShader = 0;
		}
	}
	if (hr != S_OK)
	{
		hr = GetD3D10Device()->CreatePixelShader(pShaderBytecode, BytecodeLength, ppPixelShader);

		// When in hunting mode, make a copy of the original binary, regardless.  This can be replaced, but we'll at least
		// have a copy for every shader seen.
		if (G->hunting)
		{
			D3D10Base::ID3DBlob* blob;
			D3DCreateBlob(BytecodeLength, &blob);
			memcpy(blob->GetBufferPointer(), pShaderBytecode, blob->GetBufferSize());
			RegisterForReload(*ppPixelShader, hash, L"ps", "bin", blob, ftWrite);

			// Also add the original shader to the original shaders
			// map so that if it is later replaced marking_mode =
			// original and depth buffer filtering will work:
			if (G->mOriginalPixelShaders.count(*ppPixelShader) == 0)
				G->mOriginalPixelShaders[*ppPixelShader] = *ppPixelShader;
		}
	}
	if (hr == S_OK && ppPixelShader && pShaderBytecode)
	{
		if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
		G->mPixelShaders[*ppPixelShader] = hash;
		LogDebug("    Pixel shader: handle = %p, hash = %08lx%08lx\n", *ppPixelShader, (UINT32)(hash >> 32), (UINT32)hash);

		if ((G->marking_mode == MARKING_MODE_ZERO) && zeroShader)
		{
			G->mZeroPixelShaders[*ppPixelShader] = zeroShader;
		}

		CompiledShaderMap::iterator i = G->mCompiledShaderMap.find(hash);
		if (i != G->mCompiledShaderMap.end())
		{
			LogInfo("  shader was compiled from source code %s\n", i->second.c_str());
		}
		if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
	}

	LogInfo("  returns result = %x, handle = %p\n", hr, *ppPixelShader);

	return hr;
}
        
STDMETHODIMP D3D10Wrapper::ID3D10Device::CreateBlendState(THIS_
            /* [annotation] */ 
            __in  const D3D10Base::D3D10_BLEND_DESC *pBlendStateDesc,
            /* [annotation] */ 
            __out_opt  D3D10Base::ID3D10BlendState **ppBlendState)
{
	LogDebug("ID3D10Device::CreateBlendState called\n");
	
	return GetD3D10Device()->CreateBlendState(pBlendStateDesc, ppBlendState);
}
        
STDMETHODIMP D3D10Wrapper::ID3D10Device::CreateDepthStencilState(THIS_
            /* [annotation] */ 
            __in  const D3D10Base::D3D10_DEPTH_STENCIL_DESC *pDepthStencilDesc,
            /* [annotation] */ 
            __out_opt  D3D10Base::ID3D10DepthStencilState **ppDepthStencilState)
{
	LogDebug("ID3D10Device::CreateDepthStencilState called\n");
	
	return GetD3D10Device()->CreateDepthStencilState(pDepthStencilDesc, ppDepthStencilState);
}
        
STDMETHODIMP D3D10Wrapper::ID3D10Device::CreateRasterizerState(THIS_
            /* [annotation] */ 
            __in  const D3D10Base::D3D10_RASTERIZER_DESC *pRasterizerDesc,
            /* [annotation] */ 
            __out_opt  D3D10Base::ID3D10RasterizerState **ppRasterizerState)
{
	LogDebug("ID3D10Device::CreateRasterizerState called\n");
	
	return GetD3D10Device()->CreateRasterizerState(pRasterizerDesc, ppRasterizerState);
}
        
STDMETHODIMP D3D10Wrapper::ID3D10Device::CreateSamplerState(THIS_
            /* [annotation] */ 
            __in  const D3D10Base::D3D10_SAMPLER_DESC *pSamplerDesc,
            /* [annotation] */ 
            __out_opt  D3D10Base::ID3D10SamplerState **ppSamplerState)
{
	LogDebug("ID3D10Device::CreateSamplerState called\n");
	
	return GetD3D10Device()->CreateSamplerState(pSamplerDesc, ppSamplerState);
}
        
STDMETHODIMP D3D10Wrapper::ID3D10Device::CreateQuery(THIS_
            /* [annotation] */ 
            __in  const D3D10Base::D3D10_QUERY_DESC *pQueryDesc,
            /* [annotation] */ 
            __out_opt  D3D10Base::ID3D10Query **ppQuery)
{
	if (gLogFile) 
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
	HRESULT ret = GetD3D10Device()->CreateQuery(pQueryDesc, ppQuery);
	LogInfo("  returned result = %x\n", ret);
	
	return ret;
}
        
STDMETHODIMP D3D10Wrapper::ID3D10Device::CreatePredicate(THIS_
            /* [annotation] */ 
            __in  const D3D10Base::D3D10_QUERY_DESC *pPredicateDesc,
            /* [annotation] */ 
            __out_opt  D3D10Base::ID3D10Predicate **ppPredicate)
{
	LogDebug("ID3D10Device::CreatePredicate called\n");
	
	return GetD3D10Device()->CreatePredicate(pPredicateDesc, ppPredicate);
}
        
STDMETHODIMP D3D10Wrapper::ID3D10Device::CreateCounter(THIS_
            /* [annotation] */ 
            __in  const D3D10Base::D3D10_COUNTER_DESC *pCounterDesc,
            /* [annotation] */ 
            __out_opt  D3D10Base::ID3D10Counter **ppCounter)
{
	LogDebug("ID3D10Device::CreateCounter called\n");
	
	return GetD3D10Device()->CreateCounter(pCounterDesc, ppCounter);
}
        
STDMETHODIMP D3D10Wrapper::ID3D10Device::CheckFormatSupport(THIS_
            /* [annotation] */ 
            __in  D3D10Base::DXGI_FORMAT Format,
            /* [annotation] */ 
            __out  UINT *pFormatSupport)
{
	LogDebug("ID3D10Device::CheckFormatSupport called\n");
	
	return GetD3D10Device()->CheckFormatSupport(Format, pFormatSupport);
}
        
STDMETHODIMP D3D10Wrapper::ID3D10Device::CheckMultisampleQualityLevels(THIS_
            /* [annotation] */ 
            __in  D3D10Base::DXGI_FORMAT Format,
            /* [annotation] */ 
            __in  UINT SampleCount,
            /* [annotation] */ 
            __out  UINT *pNumQualityLevels)
{
	LogDebug("ID3D10Device::CheckMultisampleQualityLevels called with Format = %d, SampleCount = %d\n", Format, SampleCount);
	
	HRESULT hr = GetD3D10Device()->CheckMultisampleQualityLevels(Format, SampleCount, pNumQualityLevels);
	LogDebug("  returns result = %x, NumQualityLevels = %d\n", hr, *pNumQualityLevels);
	
	return hr;
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::CheckCounterInfo(THIS_
            /* [annotation] */ 
            __out  D3D10Base::D3D10_COUNTER_INFO *pCounterInfo)
{
	LogDebug("ID3D10Device::CheckCounterInfo called\n");
	
	GetD3D10Device()->CheckCounterInfo(pCounterInfo);
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
	LogDebug("ID3D10Device::CheckCounter called\n");
	
	return GetD3D10Device()->CheckCounter(pDesc, pType, pActiveCounters, szName, pNameLength, szUnits, pUnitsLength,
		szDescription, pDescriptionLength);
}
        
STDMETHODIMP_(UINT) D3D10Wrapper::ID3D10Device::GetCreationFlags(THIS)
{
	LogDebug("ID3D10Device::GetCreationFlags called\n");
	
	UINT ret = GetD3D10Device()->GetCreationFlags();
	LogDebug("  returns Flags = %x\n", ret);
	
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
	LogDebug("ID3D10Device::OpenSharedResource called\n");
	
	return GetD3D10Device()->OpenSharedResource(hResource, ReturnedInterface, ppResource);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::SetTextFilterSize(THIS_
            /* [annotation] */ 
            __in  UINT Width,
            /* [annotation] */ 
            __in  UINT Height)
{
	LogDebug("ID3D10Device::SetTextFilterSize called\n");
	
	GetD3D10Device()->SetTextFilterSize(Width, Height);
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Device::GetTextFilterSize(THIS_
            /* [annotation] */ 
            __out_opt  UINT *pWidth,
            /* [annotation] */ 
            __out_opt  UINT *pHeight)
{
	LogDebug("ID3D10Device::GetTextFilterSize called\n");
	
	GetD3D10Device()->GetTextFilterSize(pWidth, pHeight);
}

/*------------------------------------------------------------------*/

// Todo: Might need to flesh this out with stereo texture and all too, if we ever see these
// multithread variants.

D3D10Wrapper::ID3D10Multithread::ID3D10Multithread(D3D10Base::ID3D10Multithread *pDevice)
    : D3D10Wrapper::IDirect3DUnknown((IUnknown*) pDevice)
{

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
	LogDebug("ID3D10Multithread::Release handle=%p, counter=%d\n", m_pUnk, m_ulRef);
	
    m_pUnk->Release();

    ULONG ulRef = --m_ulRef;

    if(ulRef <= 0)
    {
		LogDebug("  deleting self\n");
		
        if (m_pUnk) m_List.DeleteMember(m_pUnk); m_pUnk = 0;
        delete this;
        return 0L;
    }
    return ulRef;
}

STDMETHODIMP_(void) D3D10Wrapper::ID3D10Multithread::Enter(THIS)
{
	LogDebug("ID3D10Multithread::Enter called\n");
	
	GetD3D10MultithreadDevice()->Enter();
}
        
STDMETHODIMP_(void) D3D10Wrapper::ID3D10Multithread::Leave(THIS)
{
	LogDebug("ID3D10Multithread::Leave called\n");
	
	GetD3D10MultithreadDevice()->Leave();
}
        
STDMETHODIMP_(BOOL) D3D10Wrapper::ID3D10Multithread::SetMultithreadProtected(THIS_
            /* [annotation] */ 
            __in  BOOL bMTProtect)
{
	LogDebug("ID3D10Multithread::SetMultithreadProtected called with bMTProtect = %d\n", bMTProtect);
	
	BOOL ret = GetD3D10MultithreadDevice()->SetMultithreadProtected(bMTProtect);
	LogDebug("  returns %d\n", ret);
	
	return ret;
}
        
STDMETHODIMP_(BOOL) D3D10Wrapper::ID3D10Multithread::GetMultithreadProtected(THIS)
{
	LogDebug("ID3D10Multithread::GetMultithreadProtected called\n");
	
	BOOL ret = GetD3D10MultithreadDevice()->GetMultithreadProtected();
	LogDebug("  returns %d\n", ret);
	
	return ret;
}
