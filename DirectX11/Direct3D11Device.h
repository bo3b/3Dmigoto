
D3D11Wrapper::ID3D11Device::ID3D11Device(D3D11Base::ID3D11Device *pDevice)
	: D3D11Wrapper::IDirect3DUnknown((IUnknown*)pDevice),
	mStereoHandle(0), mStereoResourceView(0), mStereoTexture(0), mIniResourceView(0), mIniTexture(0), mZBufferResourceView(0)
{
	if (D3D11Base::NVAPI_OK != D3D11Base::NvAPI_Stereo_CreateHandleFromIUnknown(pDevice, &mStereoHandle))
		mStereoHandle = 0;
	mParamTextureManager.mStereoHandle = mStereoHandle;
	if (LogFile) fprintf(LogFile, "  created NVAPI stereo handle. Handle = %p\n", mStereoHandle);

	// Override custom settings.
	if (mStereoHandle && G->gSurfaceCreateMode >= 0)
	{
		NvAPIOverride();
		if (LogFile) fprintf(LogFile, "  setting custom surface creation mode.\n");

		if (D3D11Base::NVAPI_OK != D3D11Base::NvAPI_Stereo_SetSurfaceCreationMode(mStereoHandle,
			(D3D11Base::NVAPI_STEREO_SURFACECREATEMODE) G->gSurfaceCreateMode))
		{
			if (LogFile) fprintf(LogFile, "    call failed.\n");
		}
	}
	// Create stereo parameter texture.
	if (mStereoHandle)
	{
		if (LogFile) fprintf(LogFile, "  creating stereo parameter texture.\n");

		D3D11Base::D3D11_TEXTURE2D_DESC desc;
		memset(&desc, 0, sizeof(D3D11Base::D3D11_TEXTURE2D_DESC));
		desc.Width = D3D11Base::nv::stereo::ParamTextureManagerD3D11::Parms::StereoTexWidth;
		desc.Height = D3D11Base::nv::stereo::ParamTextureManagerD3D11::Parms::StereoTexHeight;
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.Format = D3D11Base::nv::stereo::ParamTextureManagerD3D11::Parms::StereoTexFormat;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.Usage = D3D11Base::D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11Base::D3D11_BIND_SHADER_RESOURCE;
		desc.CPUAccessFlags = 0;
		desc.MiscFlags = 0;
		HRESULT ret = pDevice->CreateTexture2D(&desc, 0, &mStereoTexture);
		if (FAILED(ret))
		{
			if (LogFile) fprintf(LogFile, "    call failed with result = %x.\n", ret);
		}
		else
		{
			if (LogFile) fprintf(LogFile, "    stereo texture created, handle = %p\n", mStereoTexture);
			if (LogFile) fprintf(LogFile, "  creating stereo parameter resource view.\n");

			// Since we need to bind the texture to a shader input, we also need a resource view.
			D3D11Base::D3D11_SHADER_RESOURCE_VIEW_DESC descRV;
			memset(&descRV, 0, sizeof(D3D11Base::D3D11_SHADER_RESOURCE_VIEW_DESC));
			descRV.Format = desc.Format;
			descRV.ViewDimension = D3D11Base::D3D11_SRV_DIMENSION_TEXTURE2D;
			descRV.Texture2D.MostDetailedMip = 0;
			descRV.Texture2D.MipLevels = -1;
			ret = pDevice->CreateShaderResourceView(mStereoTexture, &descRV, &mStereoResourceView);
			if (FAILED(ret))
			{
				if (LogFile) fprintf(LogFile, "    call failed with result = %x.\n", ret);
			}
			if (LogFile) fprintf(LogFile, "    stereo texture resource view created, handle = %p.\n", mStereoResourceView);
		}
	}

	// If constants are specified in the .ini file that need to be sent to shaders, we need to create
	// the resource view in order to deliver them via SetShaderResources.
	// Check for depth buffer view.
	if (G->iniParams.x != -1.0f)
	{
		D3D11Base::D3D11_TEXTURE1D_DESC desc;
		memset(&desc, 0, sizeof(D3D11Base::D3D11_TEXTURE1D_DESC));
		D3D11Base::D3D11_SUBRESOURCE_DATA initialData;

		if (LogFile) fprintf(LogFile, "  creating .ini constant parameter texture.\n");

		// Stuff the constants read from the .ini file into the subresource data structure, so 
		// we can init the texture with them.
		initialData.pSysMem = &G->iniParams;
		initialData.SysMemPitch = sizeof(DirectX::XMFLOAT4) * 1;	// only one 4 element struct 

		desc.Width = 1;												// 1 texel, .rgba as a float4
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.Format = D3D11Base::DXGI_FORMAT_R32G32B32A32_FLOAT;	// float4
		desc.Usage = D3D11Base::D3D11_USAGE_DEFAULT;				// Read/Write access from GPU
		desc.BindFlags = D3D11Base::D3D11_BIND_SHADER_RESOURCE;		// As resource view, access via t120
		desc.CPUAccessFlags = 0;									// no CPU access after init
		desc.MiscFlags = 0;
		HRESULT ret = pDevice->CreateTexture1D(&desc, &initialData, &mIniTexture);
		if (FAILED(ret))
		{
			if (LogFile) fprintf(LogFile, "    CreateTexture1D call failed with result = %x.\n", ret);
		}
		else
		{
			if (LogFile) fprintf(LogFile, "    IniParam texture created, handle = %p\n", mIniTexture);
			if (LogFile) fprintf(LogFile, "  creating IniParam resource view.\n");

			// Since we need to bind the texture to a shader input, we also need a resource view.
			// The pDesc is set to NULL so that it will simply use the desc format above.
			D3D11Base::D3D11_SHADER_RESOURCE_VIEW_DESC descRV;
			memset(&descRV, 0, sizeof(D3D11Base::D3D11_SHADER_RESOURCE_VIEW_DESC));

			ret = pDevice->CreateShaderResourceView(mIniTexture, NULL, &mIniResourceView);
			if (FAILED(ret))
			{
				if (LogFile) fprintf(LogFile, "   CreateShaderResourceView call failed with result = %x.\n", ret);
			}

			if (LogFile) fprintf(LogFile, "    Iniparams resource view created, handle = %p.\n", mIniResourceView);
		}
	}

}

D3D11Wrapper::ID3D11Device* D3D11Wrapper::ID3D11Device::GetDirect3DDevice(D3D11Base::ID3D11Device *pOrig)
{
	D3D11Wrapper::ID3D11Device* p = (D3D11Wrapper::ID3D11Device*) m_List.GetDataPtr(pOrig);
	if (!p)
	{
		p = new D3D11Wrapper::ID3D11Device(pOrig);
		if (pOrig) m_List.AddMember(pOrig, p);
	}
	return p;
}

STDMETHODIMP_(ULONG) D3D11Wrapper::ID3D11Device::AddRef(THIS)
{
	++m_ulRef;
	return m_pUnk->AddRef();
}

STDMETHODIMP_(ULONG) D3D11Wrapper::ID3D11Device::Release(THIS)
{
	if (LogFile && LogDebug) fprintf(LogFile, "ID3D11Device::Release handle=%p, counter=%d, this=%p\n", m_pUnk, m_ulRef, this);

	ULONG ulRef = m_pUnk ? m_pUnk->Release() : 0;
	if (LogFile && LogDebug) fprintf(LogFile, "  internal counter = %d\n", ulRef);

	--m_ulRef;

	if (ulRef == 0)
	{
		if (LogFile && !LogDebug) fprintf(LogFile, "ID3D11Device::Release handle=%p, counter=%d, internal counter = %d, this=%p\n", m_pUnk, m_ulRef, ulRef, this);
		if (LogFile) fprintf(LogFile, "  deleting self\n");

		if (m_pUnk) m_List.DeleteMember(m_pUnk); m_pUnk = 0;
		if (mStereoHandle)
		{
			int result = D3D11Base::NvAPI_Stereo_DestroyHandle(mStereoHandle);
			mStereoHandle = 0;
			if (LogFile) fprintf(LogFile, "  releasing NVAPI stereo handle, result = %d\n", result);
		}
		if (mStereoResourceView)
		{
			long result = mStereoResourceView->Release();
			mStereoResourceView = 0;
			if (LogFile) fprintf(LogFile, "  releasing stereo parameters resource view, result = %d\n", result);
		}
		if (mStereoTexture)
		{
			long result = mStereoTexture->Release();
			mStereoTexture = 0;
			if (LogFile) fprintf(LogFile, "  releasing stereo texture, result = %d\n", result);
		}
		if (mIniResourceView)
		{
			long result = mIniResourceView->Release();
			mIniResourceView = 0;
			if (LogFile) fprintf(LogFile, "  releasing ini parameters resource view, result = %d\n", result);
		}
		if (mIniTexture)
		{
			long result = mIniTexture->Release();
			mIniTexture = 0;
			if (LogFile) fprintf(LogFile, "  releasing iniparams texture, result = %d\n", result);
		}
		if (!G->mPreloadedPixelShaders.empty())
		{
			if (LogFile) fprintf(LogFile, "  releasing preloaded pixel shaders\n");

			for (PreloadPixelShaderMap::iterator i = G->mPreloadedPixelShaders.begin(); i != G->mPreloadedPixelShaders.end(); ++i)
				i->second->Release();
			G->mPreloadedPixelShaders.clear();
		}
		if (!G->mPreloadedVertexShaders.empty())
		{
			if (LogFile) fprintf(LogFile, "  releasing preloaded vertex shaders\n");

			for (PreloadVertexShaderMap::iterator i = G->mPreloadedVertexShaders.begin(); i != G->mPreloadedVertexShaders.end(); ++i)
				i->second->Release();
			G->mPreloadedVertexShaders.clear();
		}
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
	/*
	if (LogFile) fprintf(LogFile, "ID3D11Device::CreateBuffer called\n");
	if (LogFile) fprintf(LogFile, "  ByteWidth = %d\n", pDesc->ByteWidth);
	if (LogFile) fprintf(LogFile, "  Usage = %d\n", pDesc->Usage);
	if (LogFile) fprintf(LogFile, "  BindFlags = %x\n", pDesc->BindFlags);
	if (LogFile) fprintf(LogFile, "  CPUAccessFlags = %x\n", pDesc->CPUAccessFlags);
	if (LogFile) fprintf(LogFile, "  MiscFlags = %x\n", pDesc->MiscFlags);
	if (LogFile) fprintf(LogFile, "  StructureByteStride = %d\n", pDesc->StructureByteStride);
	if (LogFile) fprintf(LogFile, "  InitialData = %p\n", pInitialData);
	*/
	HRESULT hr = GetD3D11Device()->CreateBuffer(pDesc, pInitialData, ppBuffer);
	if (hr == S_OK && ppBuffer && G->hunting)
	{
		UINT64 hash = 0;
		if (pInitialData && pInitialData->pSysMem)
			hash = fnv_64_buf(pInitialData->pSysMem, pDesc->ByteWidth);
		hash ^= pDesc->ByteWidth; hash *= FNV_64_PRIME;
		hash ^= pDesc->Usage; hash *= FNV_64_PRIME;
		hash ^= pDesc->BindFlags; hash *= FNV_64_PRIME;
		hash ^= pDesc->CPUAccessFlags; hash *= FNV_64_PRIME;
		hash ^= pDesc->MiscFlags; hash *= FNV_64_PRIME;
		hash ^= pDesc->StructureByteStride;
		G->mDataBuffers[*ppBuffer] = hash;
		if (LogFile && LogDebug) fprintf(LogFile, "    Buffer registered: handle = %p, hash = %08lx%08lx\n", *ppBuffer, (UINT32)(hash >> 32), (UINT32)hash);
	}
	return hr;
}

STDMETHODIMP D3D11Wrapper::ID3D11Device::CreateTexture1D(THIS_
	/* [annotation] */
	__in  const D3D11Base::D3D11_TEXTURE1D_DESC *pDesc,
	/* [annotation] */
	__in_xcount_opt(pDesc->MipLevels * pDesc->ArraySize)  const D3D11Base::D3D11_SUBRESOURCE_DATA *pInitialData,
	/* [annotation] */
	__out_opt  D3D11Base::ID3D11Texture1D **ppTexture1D)
{
	return GetD3D11Device()->CreateTexture1D(pDesc, pInitialData, ppTexture1D);
}

// For any given vertex or pixel shader from the ShaderFixes folder, we need to track them at load time so
// that we can associate a given active shader with an override file.  This allows us to reload the shaders
// dynamically, and do on-the-fly fix testing.
// ShaderModel is usually something like "vs_5_0", but "bin" is a valid ShaderModel string, and tells the 
// reloader to disassemble the .bin file to determine the shader model.

static void RegisterForReload(D3D11Base::ID3D11DeviceChild* ppShader,
	UINT64 hash, wstring shaderType, string shaderModel, D3D11Base::ID3D11ClassLinkage* pClassLinkage, D3D11Base::ID3DBlob* byteCode, FILETIME timeStamp)
{
	if (LogFile) fprintf(LogFile, "    shader registered for possible reloading: %016llx_%ls as %s\n", hash, shaderType.c_str(), shaderModel.c_str());

	G->mReloadedShaders[ppShader].hash = hash;
	G->mReloadedShaders[ppShader].shaderType = shaderType;
	G->mReloadedShaders[ppShader].shaderModel = shaderModel;
	G->mReloadedShaders[ppShader].linkage = pClassLinkage;
	G->mReloadedShaders[ppShader].byteCode = byteCode;
	G->mReloadedShaders[ppShader].timeStamp = timeStamp;
	G->mReloadedShaders[ppShader].replacement = NULL;
}

static void PreloadVertexShader(wchar_t *shader_path, WIN32_FIND_DATA &findFileData, D3D11Base::ID3D11Device *m_pDevice)
{
	wchar_t fileName[MAX_PATH];
	wsprintf(fileName, L"%ls\\%ls", shader_path, findFileData.cFileName);
	char cFileName[MAX_PATH];

	if (LogFile) wcstombs(cFileName, findFileData.cFileName, MAX_PATH);
	HANDLE f = CreateFile(fileName, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (f == INVALID_HANDLE_VALUE)
	{
		if (LogFile) fprintf(LogFile, "  error reading binary vertex shader %s.\n", cFileName);
		return;
	}
	DWORD bytecodeLength = GetFileSize(f, 0);
	char *pShaderBytecode = new char[bytecodeLength];
	DWORD readSize;
	FILETIME ftWrite;
	if (!ReadFile(f, pShaderBytecode, bytecodeLength, &readSize, 0)
		|| !GetFileTime(f, NULL, NULL, &ftWrite)
		|| bytecodeLength != readSize)
	{
		if (LogFile) fprintf(LogFile, "  Error reading binary vertex shader %s.\n", cFileName);
		CloseHandle(f);
		return;
	}
	CloseHandle(f);

	if (LogFile) fprintf(LogFile, "  preloading vertex shader %s\n", cFileName);

	UINT64 hash = fnv_64_buf(pShaderBytecode, bytecodeLength);
	UINT64 keyHash = 0;
	for (int i = 0; i < 16; ++i)
	{
		UINT64 digit = findFileData.cFileName[i] > L'9' ? towupper(findFileData.cFileName[i]) - L'A' + 10 : findFileData.cFileName[i] - L'0';
		keyHash += digit << (60 - i * 4);
	}
	if (LogFile) fprintf(LogFile, "    key hash = %08lx%08lx, bytecode hash = %08lx%08lx\n",
		(UINT32)(keyHash >> 32), (UINT32)(keyHash),
		(UINT32)(hash >> 32), (UINT32)(hash));

	// Create the new shader.
	D3D11Base::ID3D11VertexShader *pVertexShader;
	HRESULT hr = m_pDevice->CreateVertexShader(pShaderBytecode, bytecodeLength, 0, &pVertexShader);
	if (FAILED(hr))
	{
		if (LogFile) fprintf(LogFile, "    error creating shader.\n");

		delete pShaderBytecode; pShaderBytecode = 0;
		return;
	}

	if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
	G->mPreloadedVertexShaders[keyHash] = pVertexShader;
	if (G->hunting)
	{
		D3D11Base::ID3DBlob* blob;
		D3DCreateBlob(bytecodeLength, &blob);
		memcpy(blob->GetBufferPointer(), pShaderBytecode, bytecodeLength);
		RegisterForReload(pVertexShader, keyHash, L"vs", "bin", NULL, blob, ftWrite);
	}
	delete pShaderBytecode; pShaderBytecode = 0;
	if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
}

static void PreloadPixelShader(wchar_t *shader_path, WIN32_FIND_DATA &findFileData, D3D11Base::ID3D11Device *m_pDevice)
{
	wchar_t fileName[MAX_PATH];
	wsprintf(fileName, L"%ls\\%ls", shader_path, findFileData.cFileName);
	char cFileName[MAX_PATH];

	if (LogFile) wcstombs(cFileName, findFileData.cFileName, MAX_PATH);
	HANDLE f = CreateFile(fileName, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (f == INVALID_HANDLE_VALUE)
	{
		if (LogFile) fprintf(LogFile, "  error reading binary pixel shader %s.\n", cFileName);
		return;
	}
	DWORD bytecodeLength = GetFileSize(f, 0);
	char *pShaderBytecode = new char[bytecodeLength];
	DWORD readSize;
	FILETIME ftWrite;
	if (!ReadFile(f, pShaderBytecode, bytecodeLength, &readSize, 0)
		|| !GetFileTime(f, NULL, NULL, &ftWrite)
		|| bytecodeLength != readSize)
	{
		if (LogFile) fprintf(LogFile, "  Error reading binary pixel shader %s.\n", cFileName);
		CloseHandle(f);
		return;
	}
	CloseHandle(f);

	if (LogFile) fprintf(LogFile, "  preloading pixel shader %s\n", cFileName);

	UINT64 hash = fnv_64_buf(pShaderBytecode, bytecodeLength);
	UINT64 keyHash = 0;
	for (int i = 0; i < 16; ++i)
	{
		UINT64 digit = findFileData.cFileName[i] > L'9' ? towupper(findFileData.cFileName[i]) - L'A' + 10 : findFileData.cFileName[i] - L'0';
		keyHash += digit << (60 - i * 4);
	}
	if (LogFile) fprintf(LogFile, "    key hash = %08lx%08lx, bytecode hash = %08lx%08lx\n",
		(UINT32)(keyHash >> 32), (UINT32)(keyHash),
		(UINT32)(hash >> 32), (UINT32)(hash));

	// Create the new shader.
	D3D11Base::ID3D11PixelShader *pPixelShader;
	HRESULT hr = m_pDevice->CreatePixelShader(pShaderBytecode, bytecodeLength, 0, &pPixelShader);
	if (FAILED(hr))
	{
		if (LogFile) fprintf(LogFile, "    error creating shader.\n");

		delete pShaderBytecode; pShaderBytecode = 0;
		return;
	}

	if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
	G->mPreloadedPixelShaders[hash] = pPixelShader;
	if (G->hunting)
	{
		D3D11Base::ID3DBlob* blob;
		D3DCreateBlob(bytecodeLength, &blob);
		memcpy(blob->GetBufferPointer(), pShaderBytecode, bytecodeLength);
		RegisterForReload(pPixelShader, keyHash, L"ps", "bin", NULL, blob, ftWrite);
	}
	delete pShaderBytecode; pShaderBytecode = 0;
	if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
}

STDMETHODIMP D3D11Wrapper::ID3D11Device::CreateTexture2D(THIS_
	/* [annotation] */
	__in  const D3D11Base::D3D11_TEXTURE2D_DESC *pDesc,
	/* [annotation] */
	__in_xcount_opt(pDesc->MipLevels * pDesc->ArraySize)  const D3D11Base::D3D11_SUBRESOURCE_DATA *pInitialData,
	/* [annotation] */
	__out_opt  D3D11Base::ID3D11Texture2D **ppTexture2D)
{
	if (LogFile && LogDebug) fprintf(LogFile, "ID3D11Device::CreateTexture2D called with parameters\n");
	if (pDesc && LogFile && LogDebug) fprintf(LogFile, "  Width = %d, Height = %d, MipLevels = %d, ArraySize = %d\n",
		pDesc->Width, pDesc->Height, pDesc->MipLevels, pDesc->ArraySize);
	if (pDesc && LogFile && LogDebug) fprintf(LogFile, "  Format = %d, Usage = %x, BindFlags = %x, CPUAccessFlags = %x, MiscFlags = %x\n",
		pDesc->Format, pDesc->Usage, pDesc->BindFlags, pDesc->CPUAccessFlags, pDesc->MiscFlags);

	// Preload shaders?
	if (G->PRELOAD_SHADERS && G->mPreloadedVertexShaders.empty() && G->mPreloadedPixelShaders.empty())
	{
		if (LogFile) fprintf(LogFile, "  preloading custom shaders.\n");

		wchar_t fileName[MAX_PATH];
		if (SHADER_PATH[0])
		{
			wsprintf(fileName, L"%ls\\*-vs_replace.bin", SHADER_PATH);
			WIN32_FIND_DATA findFileData;
			HANDLE hFind = FindFirstFile(fileName, &findFileData);
			if (hFind != INVALID_HANDLE_VALUE)
			{
				BOOL found = true;
				while (found)
				{
					PreloadVertexShader(SHADER_PATH, findFileData, GetD3D11Device());
					found = FindNextFile(hFind, &findFileData);
				}
				FindClose(hFind);
			}
		}
		if (SHADER_CACHE_PATH[0])
		{
			wsprintf(fileName, L"%ls\\*-vs_replace.bin", SHADER_CACHE_PATH);
			WIN32_FIND_DATA findFileData;
			HANDLE hFind = FindFirstFile(fileName, &findFileData);
			if (hFind != INVALID_HANDLE_VALUE)
			{
				BOOL found = true;
				while (found)
				{
					PreloadVertexShader(SHADER_CACHE_PATH, findFileData, GetD3D11Device());
					found = FindNextFile(hFind, &findFileData);
				}
				FindClose(hFind);
			}
		}
		if (SHADER_PATH[0])
		{
			wsprintf(fileName, L"%ls\\*-ps_replace.bin", SHADER_PATH);
			WIN32_FIND_DATA findFileData;
			HANDLE hFind = FindFirstFile(fileName, &findFileData);
			if (hFind != INVALID_HANDLE_VALUE)
			{
				BOOL found = true;
				while (found)
				{
					PreloadPixelShader(SHADER_PATH, findFileData, GetD3D11Device());
					found = FindNextFile(hFind, &findFileData);
				}
				FindClose(hFind);
			}
		}
		if (SHADER_CACHE_PATH[0])
		{
			wsprintf(fileName, L"%ls\\*-ps_replace.bin", SHADER_CACHE_PATH);
			WIN32_FIND_DATA findFileData;
			HANDLE hFind = FindFirstFile(fileName, &findFileData);
			if (hFind != INVALID_HANDLE_VALUE)
			{
				BOOL found = true;
				while (found)
				{
					PreloadPixelShader(SHADER_CACHE_PATH, findFileData, GetD3D11Device());
					found = FindNextFile(hFind, &findFileData);
				}
				FindClose(hFind);
			}
		}
	}

	// Get screen resolution.
	int hashWidth = 0; if (pDesc) hashWidth = pDesc->Width;
	int hashHeight = 0; if (pDesc) hashHeight = pDesc->Height;
	if (hashWidth == G->mSwapChainInfo.width && hashHeight == G->mSwapChainInfo.height)
	{
		hashWidth = 1386492276;
		hashHeight = 1386492276;
	}

	// Create hash code.
	UINT64 hash = 0;
	if (pInitialData && pInitialData->pSysMem && pDesc)
		hash = fnv_64_buf(pInitialData->pSysMem, pDesc->Width / 2 * pDesc->Height * pDesc->ArraySize);
	if (pDesc)
	{
		hash ^= hashWidth; hash *= FNV_64_PRIME;
		hash ^= hashHeight; hash *= FNV_64_PRIME;
		hash ^= pDesc->MipLevels; hash *= FNV_64_PRIME;
		hash ^= pDesc->ArraySize; hash *= FNV_64_PRIME;
		hash ^= pDesc->Format; hash *= FNV_64_PRIME;
		hash ^= pDesc->Usage; hash *= FNV_64_PRIME;
		hash ^= pDesc->BindFlags; hash *= FNV_64_PRIME;
		hash ^= pDesc->CPUAccessFlags; hash *= FNV_64_PRIME;
		hash ^= pDesc->MiscFlags;
	}
	if (LogFile && LogDebug) fprintf(LogFile, "  InitialData = %p, hash = %08lx%08lx\n", pInitialData, (UINT32)(hash >> 32), (UINT32)hash);

	// Override custom settings?
	bool override = false;
	TextureStereoMap::iterator istereo = G->mTextureStereoMap.find(hash);
	TextureTypeMap::iterator itype = G->mTextureTypeMap.find(hash);
	D3D11Base::NVAPI_STEREO_SURFACECREATEMODE oldMode = (D3D11Base::NVAPI_STEREO_SURFACECREATEMODE) - 1, newMode = (D3D11Base::NVAPI_STEREO_SURFACECREATEMODE) - 1;
	D3D11Base::D3D11_TEXTURE2D_DESC newDesc = *pDesc;
	if (istereo != G->mTextureStereoMap.end() || itype != G->mTextureTypeMap.end())
	{
		override = true;
		if (istereo != G->mTextureStereoMap.end())
			newMode = (D3D11Base::NVAPI_STEREO_SURFACECREATEMODE) istereo->second;
		// Check iteration.
		TextureIterationMap::iterator j = G->mTextureIterationMap.find(hash);
		if (j != G->mTextureIterationMap.end())
		{
			std::vector<int>::iterator k = j->second.begin();
			int currentIteration = j->second[0] = j->second[0] + 1;
			if (LogFile) fprintf(LogFile, "  current iteration = %d\n", currentIteration);

			override = false;
			while (++k != j->second.end())
			{
				if (currentIteration == *k)
				{
					override = true;
					break;
				}
			}
			if (!override)
			{
				if (LogFile) fprintf(LogFile, "  override skipped\n");
			}
		}
	}
	if (pDesc && G->gSurfaceSquareCreateMode >= 0 && pDesc->Width == pDesc->Height && (pDesc->Usage & D3D11Base::D3D11_USAGE_IMMUTABLE) == 0)
	{
		override = true;
		newMode = (D3D11Base::NVAPI_STEREO_SURFACECREATEMODE) G->gSurfaceSquareCreateMode;
	}
	if (override)
	{
		if (newMode != (D3D11Base::NVAPI_STEREO_SURFACECREATEMODE) - 1)
		{
			D3D11Base::NvAPI_Stereo_GetSurfaceCreationMode(mStereoHandle, &oldMode);
			NvAPIOverride();
			if (LogFile) fprintf(LogFile, "  setting custom surface creation mode.\n");

			if (D3D11Base::NVAPI_OK != D3D11Base::NvAPI_Stereo_SetSurfaceCreationMode(mStereoHandle, newMode))
			{
				if (LogFile) fprintf(LogFile, "    call failed.\n");
			}
		}
		if (itype != G->mTextureTypeMap.end())
		{
			if (LogFile) fprintf(LogFile, "  setting custom format to %d\n", itype->second);

			newDesc.Format = (D3D11Base::DXGI_FORMAT) itype->second;
		}
	}
	HRESULT hr = GetD3D11Device()->CreateTexture2D(&newDesc, pInitialData, ppTexture2D);
	if (oldMode != (D3D11Base::NVAPI_STEREO_SURFACECREATEMODE) - 1)
	{
		if (D3D11Base::NVAPI_OK != D3D11Base::NvAPI_Stereo_SetSurfaceCreationMode(mStereoHandle, oldMode))
		{
			if (LogFile) fprintf(LogFile, "    restore call failed.\n");
		}
	}
	if (LogFile && LogDebug && ppTexture2D) fprintf(LogFile, "  returns result = %x, handle = %p\n", hr, *ppTexture2D);

	// Register texture.
	if (ppTexture2D)
		G->mTexture2D_ID[*ppTexture2D] = hash;

	return hr;
}

STDMETHODIMP D3D11Wrapper::ID3D11Device::CreateTexture3D(THIS_
	/* [annotation] */
	__in  const D3D11Base::D3D11_TEXTURE3D_DESC *pDesc,
	/* [annotation] */
	__in_xcount_opt(pDesc->MipLevels)  const D3D11Base::D3D11_SUBRESOURCE_DATA *pInitialData,
	/* [annotation] */
	__out_opt  D3D11Base::ID3D11Texture3D **ppTexture3D)
{
	if (LogFile) fprintf(LogFile, "ID3D11Device::CreateTexture3D called with parameters\n");
	if (pDesc && LogFile) fprintf(LogFile, "  Width = %d, Height = %d, Depth = %d, MipLevels = %d, InitialData = %p\n",
		pDesc->Width, pDesc->Height, pDesc->Depth, pDesc->MipLevels, pInitialData);
	if (pDesc && LogFile) fprintf(LogFile, "  Format = %d, Usage = %x, BindFlags = %x, CPUAccessFlags = %x, MiscFlags = %x\n",
		pDesc->Format, pDesc->Usage, pDesc->BindFlags, pDesc->CPUAccessFlags, pDesc->MiscFlags);

	// Get screen resolution.
	int hashWidth = 0; if (pDesc) hashWidth = pDesc->Width;
	int hashHeight = 0; if (pDesc) hashHeight = pDesc->Height;
	if (hashWidth == G->mSwapChainInfo.width && hashHeight == G->mSwapChainInfo.height)
	{
		hashWidth = 1386492276;
		hashHeight = 1386492276;
	}

	// Create hash code.
	UINT64 hash = 0;
	if (pInitialData && pInitialData->pSysMem)
		hash = fnv_64_buf(pInitialData->pSysMem, pDesc->Width / 2 * pDesc->Height * pDesc->Depth);
	if (pDesc)
	{
		hash ^= hashWidth; hash *= FNV_64_PRIME;
		hash ^= hashHeight; hash *= FNV_64_PRIME;
		hash ^= pDesc->Height; hash *= FNV_64_PRIME;
		hash ^= pDesc->MipLevels; hash *= FNV_64_PRIME;
		hash ^= pDesc->Format; hash *= FNV_64_PRIME;
		hash ^= pDesc->Usage; hash *= FNV_64_PRIME;
		hash ^= pDesc->BindFlags; hash *= FNV_64_PRIME;
		hash ^= pDesc->CPUAccessFlags; hash *= FNV_64_PRIME;
		hash ^= pDesc->MiscFlags;
	}
	if (LogFile) fprintf(LogFile, "  InitialData = %p, hash = %08lx%08lx\n", pInitialData, (UINT32)(hash >> 32), (UINT32)hash);

	HRESULT hr = GetD3D11Device()->CreateTexture3D(pDesc, pInitialData, ppTexture3D);

	// Register texture.
	if (hr == S_OK && ppTexture3D)
		G->mTexture3D_ID[*ppTexture3D] = hash;

	if (LogFile) fprintf(LogFile, "  returns result = %x\n", hr);

	return hr;
}

STDMETHODIMP D3D11Wrapper::ID3D11Device::CreateShaderResourceView(THIS_
	/* [annotation] */
	__in  D3D11Base::ID3D11Resource *pResource,
	/* [annotation] */
	__in_opt  const D3D11Base::D3D11_SHADER_RESOURCE_VIEW_DESC *pDesc,
	/* [annotation] */
	__out_opt  D3D11Base::ID3D11ShaderResourceView **ppSRView)
{
	if (LogFile && LogDebug) fprintf(LogFile, "ID3D11Device::CreateShaderResourceView called\n");

	HRESULT hr = GetD3D11Device()->CreateShaderResourceView(pResource, pDesc, ppSRView);

	// Check for depth buffer view.
	if (hr == S_OK && G->ZBufferHashToInject)
	{
		map<D3D11Base::ID3D11Texture2D *, UINT64>::iterator i = G->mTexture2D_ID.find((D3D11Base::ID3D11Texture2D *) pResource);
		if (i != G->mTexture2D_ID.end() && i->second == G->ZBufferHashToInject)
		{
			if (LogFile) fprintf(LogFile, "  resource view of z buffer found: handle = %p, hash = %08lx%08lx\n", *ppSRView, (UINT32)(i->second >> 32), (UINT32)i->second);

			mZBufferResourceView = *ppSRView;
		}
	}

	if (LogFile && LogDebug) fprintf(LogFile, "  returns result = %x\n", hr);

	return hr;
}

STDMETHODIMP D3D11Wrapper::ID3D11Device::CreateUnorderedAccessView(THIS_
	/* [annotation] */
	__in  D3D11Base::ID3D11Resource *pResource,
	/* [annotation] */
	__in_opt  const D3D11Base::D3D11_UNORDERED_ACCESS_VIEW_DESC *pDesc,
	/* [annotation] */
	__out_opt  D3D11Base::ID3D11UnorderedAccessView **ppUAView)
{
	return GetD3D11Device()->CreateUnorderedAccessView(pResource, pDesc, ppUAView);
}

STDMETHODIMP D3D11Wrapper::ID3D11Device::CreateRenderTargetView(THIS_
	/* [annotation] */
	__in  D3D11Base::ID3D11Resource *pResource,
	/* [annotation] */
	__in_opt  const D3D11Base::D3D11_RENDER_TARGET_VIEW_DESC *pDesc,
	/* [annotation] */
	__out_opt  D3D11Base::ID3D11RenderTargetView **ppRTView)
{
	return GetD3D11Device()->CreateRenderTargetView(pResource, pDesc, ppRTView);
}

STDMETHODIMP D3D11Wrapper::ID3D11Device::CreateDepthStencilView(THIS_
	/* [annotation] */
	__in  D3D11Base::ID3D11Resource *pResource,
	/* [annotation] */
	__in_opt  const D3D11Base::D3D11_DEPTH_STENCIL_VIEW_DESC *pDesc,
	/* [annotation] */
	__out_opt  D3D11Base::ID3D11DepthStencilView **ppDepthStencilView)
{
	return GetD3D11Device()->CreateDepthStencilView(pResource, pDesc, ppDepthStencilView);
}

STDMETHODIMP D3D11Wrapper::ID3D11Device::CreateInputLayout(THIS_
	/* [annotation] */
	__in_ecount(NumElements)  const D3D11Base::D3D11_INPUT_ELEMENT_DESC *pInputElementDescs,
	/* [annotation] */
	__in_range(0, D3D11_IA_VERTEX_INPUT_STRUCTURE_ELEMENT_COUNT)  UINT NumElements,
	/* [annotation] */
	__in  const void *pShaderBytecodeWithInputSignature,
	/* [annotation] */
	__in  SIZE_T BytecodeLength,
	/* [annotation] */
	__out_opt  D3D11Base::ID3D11InputLayout **ppInputLayout)
{
	return GetD3D11Device()->CreateInputLayout(pInputElementDescs, NumElements, pShaderBytecodeWithInputSignature,
		BytecodeLength, ppInputLayout);
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

static char *ReplaceShader(D3D11Base::ID3D11Device *realDevice, UINT64 hash, const wchar_t *shaderType, const void *pShaderBytecode,
	SIZE_T BytecodeLength, SIZE_T &pCodeSize, string &foundShaderModel, FILETIME &timeStamp, void **zeroShader)
{
	if (G->mBlockingMode)
		return 0;

	foundShaderModel = "";
	timeStamp = { 0 };

	*zeroShader = 0;
	char *pCode = 0;
	wchar_t val[MAX_PATH];

	if (SHADER_PATH[0] && SHADER_CACHE_PATH[0])
	{
		if (G->EXPORT_ALL)
		{
			D3D11Base::ID3DBlob *disassembly;
			HRESULT r = D3D11Base::D3DDisassemble(pShaderBytecode, BytecodeLength,
				D3D_DISASM_ENABLE_DEFAULT_VALUE_PRINTS,
				0, &disassembly);
			if (r != S_OK)
			{
				if (LogFile) fprintf(LogFile, "  disassembly failed.\n");
			}
			else
			{
				wsprintf(val, L"%ls\\%08lx%08lx-%ls.txt", SHADER_CACHE_PATH, (UINT32)(hash >> 32), (UINT32)(hash), shaderType);
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
						if (dataSize == disassembly->GetBufferSize() && !memcmp(disassembly->GetBufferPointer(), buf, dataSize)) exists = true;
						delete buf;
						if (exists) break;
						wsprintf(val, L"%ls\\%08lx%08lx-%ls_%d.txt", SHADER_CACHE_PATH, (UINT32)(hash >> 32), (UINT32)hash, shaderType, ++cnt);
						f = _wfsopen(val, L"rb", _SH_DENYNO);
					}
				}
				if (!exists)
				{
					FILE *f;
					_wfopen_s(&f, val, L"wb");
					if (LogFile)
					{
						char fileName[MAX_PATH];
						wcstombs(fileName, val, MAX_PATH);
						if (f)
							fprintf(LogFile, "    storing disassembly to %s\n", fileName);
						else
							fprintf(LogFile, "    error storing disassembly to %s\n", fileName);
					}
					if (f)
					{
						fwrite(disassembly->GetBufferPointer(), 1, disassembly->GetBufferSize(), f);
						fclose(f);
					}
				}
				disassembly->Release();
			}
		}

		// Read binary compiled shader.
		wsprintf(val, L"%ls\\%08lx%08lx-%ls_replace.bin", SHADER_PATH, (UINT32)(hash >> 32), (UINT32)(hash), shaderType);
		HANDLE f = CreateFile(val, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (f == INVALID_HANDLE_VALUE)
		{
			wsprintf(val, L"%ls\\%08lx%08lx-%ls_replace.bin", SHADER_CACHE_PATH, (UINT32)(hash >> 32), (UINT32)(hash), shaderType);
			f = CreateFile(val, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		}
		if (f != INVALID_HANDLE_VALUE)
		{
			if (LogFile) fprintf(LogFile, "    Replacement binary shader found.\n");

			DWORD codeSize = GetFileSize(f, 0);
			pCode = new char[codeSize];
			DWORD readSize;
			FILETIME ftWrite;
			if (!ReadFile(f, pCode, codeSize, &readSize, 0)
				|| !GetFileTime(f, NULL, NULL, &ftWrite)
				|| codeSize != readSize)
			{
				if (LogFile) fprintf(LogFile, "    Error reading file.\n");
				delete pCode; pCode = 0;
				CloseHandle(f);
			}
			else
			{
				pCodeSize = codeSize;
				if (LogFile) fprintf(LogFile, "    Bytecode loaded. Size = %d\n", pCodeSize);
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

		if (!pCode)
		{
			// Read HLSL shader.
			wsprintf(val, L"%ls\\%08lx%08lx-%ls_replace.txt", SHADER_PATH, (UINT32)(hash >> 32), (UINT32)(hash), shaderType);
			f = CreateFile(val, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
			wchar_t *shader_path = SHADER_PATH;
			if (f == INVALID_HANDLE_VALUE)
			{
				wsprintf(val, L"%ls\\%08lx%08lx-%ls_replace.txt", SHADER_CACHE_PATH, (UINT32)(hash >> 32), (UINT32)(hash), shaderType);
				f = CreateFile(val, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
				shader_path = SHADER_CACHE_PATH;
			}
			if (f != INVALID_HANDLE_VALUE)
			{
				if (LogFile) fprintf(LogFile, "    Replacement shader found. Loading replacement HLSL code.\n");

				DWORD srcDataSize = GetFileSize(f, 0);
				char *srcData = new char[srcDataSize];
				DWORD readSize;
				FILETIME ftWrite;
				if (!ReadFile(f, srcData, srcDataSize, &readSize, 0)
					|| !GetFileTime(f, NULL, NULL, &ftWrite)
					|| srcDataSize != readSize)
					if (LogFile) fprintf(LogFile, "    Error reading file.\n");
				CloseHandle(f);
				if (LogFile) fprintf(LogFile, "    Source code loaded. Size = %d\n", srcDataSize);

				// Disassemble old shader to get shader model.
				D3D11Base::ID3DBlob *disassembly;
				HRESULT ret = D3D11Base::D3DDisassemble(pShaderBytecode, BytecodeLength,
					D3D_DISASM_ENABLE_DEFAULT_VALUE_PRINTS, 0, &disassembly);
				if (ret != S_OK)
				{
					if (LogFile) fprintf(LogFile, "    disassembly of original shader failed.\n");

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
					if (LogFile) fprintf(LogFile, "    compiling replacement HLSL code with shader model %s\n", shaderModel.c_str());

					D3D11Base::ID3DBlob *pErrorMsgs;
					D3D11Base::ID3DBlob *pCompiledOutput = 0;
					ret = D3D11Base::D3DCompile(srcData, srcDataSize, "wrapper1349", 0, ((D3D11Base::ID3DInclude*)(UINT_PTR)1),
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

					if (LogFile) fprintf(LogFile, "    compile result of replacement HLSL shader: %x\n", ret);

					if (LogFile && pErrorMsgs)
					{
						LPVOID errMsg = pErrorMsgs->GetBufferPointer();
						SIZE_T errSize = pErrorMsgs->GetBufferSize();
						fprintf(LogFile, "--------------------------------------------- BEGIN ---------------------------------------------\n");
						fwrite(errMsg, 1, errSize - 1, LogFile);
						fprintf(LogFile, "---------------------------------------------- END ----------------------------------------------\n");
						pErrorMsgs->Release();
					}

					// Write replacement.
					if (G->CACHE_SHADERS && pCode)
					{
						wsprintf(val, L"%ls\\%08lx%08lx-%ls_replace.bin", shader_path, (UINT32)(hash >> 32), (UINT32)(hash), shaderType);
						FILE *fw;
						_wfopen_s(&fw, val, L"wb");
						if (LogFile)
						{
							char fileName[MAX_PATH];
							wcstombs(fileName, val, MAX_PATH);
							if (fw)
								fprintf(LogFile, "    storing compiled shader to %s\n", fileName);
							else
								fprintf(LogFile, "    error writing compiled shader to %s\n", fileName);
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
	if (SHADER_PATH[0] && SHADER_CACHE_PATH[0] && ((G->EXPORT_HLSL >= 1) || G->FIX_SV_Position || G->FIX_Light_Position || G->FIX_Recompile_VS) && !pCode)
	{
		// Skip?
		wsprintf(val, L"%ls\\%08lx%08lx-%ls_bad.txt", SHADER_PATH, (UINT32)(hash >> 32), (UINT32)(hash), shaderType);
		HANDLE hFind = CreateFile(val, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		if (hFind != INVALID_HANDLE_VALUE)
		{
			char fileName[MAX_PATH];
			wcstombs(fileName, val, MAX_PATH);
			if (LogFile) fprintf(LogFile, "    skipping shader marked bad. %s\n", fileName);
			CloseHandle(hFind);
		}
		else
		{
			D3D11Base::ID3DBlob *disassembly = 0;
			FILE *fw = 0;
			string shaderModel = "";

			// Disassemble old shader for fixing.
			HRESULT ret = D3D11Base::D3DDisassemble(pShaderBytecode, BytecodeLength,
				D3D_DISASM_ENABLE_DEFAULT_VALUE_PRINTS, 0, &disassembly);
			if (ret != S_OK)
			{
				if (LogFile) fprintf(LogFile, "    disassembly of original shader failed.\n");
			}
			else
			{
				// Decompile code.
				if (LogFile) fprintf(LogFile, "    creating HLSL representation.\n");

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
			// Not sure why, but blocking the Decompiler from multi-threading prevents a crash.
			// This is just a patch for now.
			if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
				const string decompiledCode = DecompileBinaryHLSL(p, patched, shaderModel, errorOccurred);
			if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
				if (!decompiledCode.size())
				{
					if (LogFile) fprintf(LogFile, "    error while decompiling.\n");

					return 0;
				}

				if (!errorOccurred && ((G->EXPORT_HLSL >= 1) || (G->EXPORT_FIXED && patched)))
				{
					wsprintf(val, L"%ls\\%08lx%08lx-%ls_replace.txt", SHADER_CACHE_PATH, (UINT32)(hash >> 32), (UINT32)hash, shaderType);
					_wfopen_s(&fw, val, L"wb");
					if (LogFile)
					{
						char fileName[MAX_PATH];
						wcstombs(fileName, val, MAX_PATH);
						if (fw)
							fprintf(LogFile, "    storing patched shader to %s\n", fileName);
						else
							fprintf(LogFile, "    error storing patched shader to %s\n", fileName);
					}
					if (fw)
					{
						// Save decompiled HLSL code to new file.
						fwrite(decompiledCode.c_str(), 1, decompiledCode.size(), fw);

						// Now also write the ASM text to the shader file as a set of comments at the bottom.
						// That will make the ASM code the master reference for fixing shaders, and should be more 
						// convenient, especially in light of the numerous decompiler bugs we see.
						if (G->EXPORT_HLSL >= 2)
						{
							fprintf_s(fw, "\n\n/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Original ASM ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
							fwrite(disassembly->GetBufferPointer(), 1, disassembly->GetBufferSize(), fw);
							fprintf_s(fw, "\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/\n");

						}

						if (disassembly) disassembly->Release(); disassembly = 0;
					}
				}

				// Let's re-compile every time we create a new one, regardless.  Previously this would only re-compile
				// after auto-fixing shaders. This makes shader Decompiler errors more obvious.
				if (!errorOccurred)
				{
					// Compile replacement.
					if (LogFile) fprintf(LogFile, "    compiling fixed HLSL code with shader model %s, size = %d\n", shaderModel.c_str(), decompiledCode.size());

					D3D11Base::ID3DBlob *pErrorMsgs;
					D3D11Base::ID3DBlob *pCompiledOutput = 0;
					ret = D3D11Base::D3DCompile(decompiledCode.c_str(), decompiledCode.size(), "wrapper1349", 0, ((D3D11Base::ID3DInclude*)(UINT_PTR)1),
						"main", shaderModel.c_str(), D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &pCompiledOutput, &pErrorMsgs);
					if (LogFile) fprintf(LogFile, "    compile result of fixed HLSL shader: %x\n", ret);

					if (pCompiledOutput)
					{
						pCodeSize = pCompiledOutput->GetBufferSize();
						pCode = new char[pCodeSize];
						memcpy(pCode, pCompiledOutput->GetBufferPointer(), pCodeSize);
					}

					if (LogFile && pErrorMsgs)
					{
						LPVOID errMsg = pErrorMsgs->GetBufferPointer();
						SIZE_T errSize = pErrorMsgs->GetBufferSize();
						fprintf(LogFile, "--------------------------------------------- BEGIN ---------------------------------------------\n");
						fwrite(errMsg, 1, errSize - 1, LogFile);
						fprintf(LogFile, "------------------------------------------- HLSL code -------------------------------------------\n");
						fwrite(decompiledCode.c_str(), 1, decompiledCode.size(), LogFile);
						fprintf(LogFile, "\n---------------------------------------------- END ----------------------------------------------\n");

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
						HRESULT ret = D3D11Base::D3DDisassemble(pCompiledOutput->GetBufferPointer(), pCompiledOutput->GetBufferSize(),
							D3D_DISASM_ENABLE_DEFAULT_VALUE_PRINTS, 0, &disassembly);
						if (ret != S_OK)
						{
							if (LogFile) fprintf(LogFile, "    disassembly of recompiled shader failed.\n");
						}
						else
						{
							fprintf_s(fw, "\n\n/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Recompiled ASM ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
							fwrite(disassembly->GetBufferPointer(), 1, disassembly->GetBufferSize(), fw);
							fprintf_s(fw, "\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/\n");
							disassembly->Release(); disassembly = 0;
						}
					}
	
					if (pCompiledOutput)
					{
						pCompiledOutput->Release(); pCompiledOutput = 0;
					}
				}

				// Write replacement.
				if (G->CACHE_SHADERS && pCode)
				{
					wsprintf(val, L"%ls\\%08lx%08lx-%ls_replace.bin", SHADER_CACHE_PATH, (UINT32)(hash >> 32), (UINT32)hash, shaderType);
					FILE *bin;
					_wfopen_s(&bin, val, L"wb");
					if (bin)
					{
						fwrite(pCode, 1, pCodeSize, bin);
						fclose(bin);
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
		D3D11Base::ID3DBlob *disassembly;
		HRESULT ret = D3D11Base::D3DDisassemble(pShaderBytecode, BytecodeLength,
			D3D_DISASM_ENABLE_DEFAULT_VALUE_PRINTS, 0, &disassembly);
		if (ret != S_OK)
		{
			if (LogFile) fprintf(LogFile, "    disassembly of original shader failed.\n");
		}
		else
		{
			// Decompile code.
			if (LogFile) fprintf(LogFile, "    creating HLSL representation of zero output shader.\n");

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
				if (LogFile) fprintf(LogFile, "    error while decompiling.\n");

				return 0;
			}
			if (!errorOccurred)
			{
				// Compile replacement.
				if (LogFile) fprintf(LogFile, "    compiling zero HLSL code with shader model %s, size = %d\n", shaderModel.c_str(), decompiledCode.size());

				D3D11Base::ID3DBlob *pErrorMsgs;
				D3D11Base::ID3DBlob *pCompiledOutput = 0;
				ret = D3D11Base::D3DCompile(decompiledCode.c_str(), decompiledCode.size(), "wrapper1349", 0, ((D3D11Base::ID3DInclude*)(UINT_PTR)1),
					"main", shaderModel.c_str(), D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &pCompiledOutput, &pErrorMsgs);
				if (LogFile) fprintf(LogFile, "    compile result of zero HLSL shader: %x\n", ret);

				if (pCompiledOutput)
				{
					SIZE_T codeSize = pCompiledOutput->GetBufferSize();
					char *code = new char[codeSize];
					memcpy(code, pCompiledOutput->GetBufferPointer(), codeSize);
					pCompiledOutput->Release(); pCompiledOutput = 0;
					if (!wcscmp(shaderType, L"vs"))
					{
						D3D11Base::ID3D11VertexShader *zeroVertexShader;
						HRESULT hr = realDevice->CreateVertexShader(code, codeSize, 0, &zeroVertexShader);
						if (hr == S_OK)
							*zeroShader = zeroVertexShader;
					}
					else if (!wcscmp(shaderType, L"ps"))
					{
						D3D11Base::ID3D11PixelShader *zeroPixelShader;
						HRESULT hr = realDevice->CreatePixelShader(code, codeSize, 0, &zeroPixelShader);
						if (hr == S_OK)
							*zeroShader = zeroPixelShader;
					}
					delete code;
				}

				if (LogFile && pErrorMsgs)
				{
					LPVOID errMsg = pErrorMsgs->GetBufferPointer();
					SIZE_T errSize = pErrorMsgs->GetBufferSize();
					fprintf(LogFile, "--------------------------------------------- BEGIN ---------------------------------------------\n");
					fwrite(errMsg, 1, errSize - 1, LogFile);
					fprintf(LogFile, "------------------------------------------- HLSL code -------------------------------------------\n");
					fwrite(decompiledCode.c_str(), 1, decompiledCode.size(), LogFile);
					fprintf(LogFile, "\n---------------------------------------------- END ----------------------------------------------\n");
					pErrorMsgs->Release();
				}
			}
		}
	}

	return pCode;
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
	if (LogFile) fprintf(LogFile, "ID3D11Device::CreateVertexShader called with BytecodeLength = %d, handle = %p, ClassLinkage = %p\n", BytecodeLength, pShaderBytecode, pClassLinkage);

	HRESULT hr = -1;
	UINT64 hash;
	string shaderModel;
	SIZE_T replaceShaderSize;
	FILETIME ftWrite;
	D3D11Base::ID3D11VertexShader *zeroShader = 0;

	if (pShaderBytecode && ppVertexShader)
	{
		// Calculate hash
		hash = fnv_64_buf(pShaderBytecode, BytecodeLength);
		if (LogFile) fprintf(LogFile, "  bytecode hash = %016llx\n", hash);
		if (LogFile) fprintf(LogFile, "  bytecode hash = %08lx%08lx\n", (UINT32)(hash >> 32), (UINT32)hash);

		// Preloaded shader? (Can't use preloaded shaders with class linkage).
		if (!pClassLinkage)
		{
			PreloadVertexShaderMap::iterator i = G->mPreloadedVertexShaders.find(hash);
			if (i != G->mPreloadedVertexShaders.end())
			{
				*ppVertexShader = i->second;
				ULONG cnt = (*ppVertexShader)->AddRef();
				hr = S_OK;
				if (LogFile) fprintf(LogFile, "    shader assigned by preloaded version. ref counter = %d\n", cnt);

				if (G->marking_mode == MARKING_MODE_ZERO)
				{
					char *replaceShader = ReplaceShader(GetD3D11Device(), hash, L"vs", pShaderBytecode, BytecodeLength, replaceShaderSize,
						shaderModel, ftWrite, (void **)&zeroShader);
					delete replaceShader;
				}
				if (G->marking_mode == MARKING_MODE_ORIGINAL)
				{
					// Compile original shader.
					D3D11Base::ID3D11VertexShader *originalShader;
					GetD3D11Device()->CreateVertexShader(pShaderBytecode, BytecodeLength, pClassLinkage, &originalShader);
					G->mOriginalVertexShaders[*ppVertexShader] = originalShader;
				}
			}
		}
	}
	if (hr != S_OK && ppVertexShader && pShaderBytecode)
	{
		D3D11Base::ID3D11VertexShader *zeroShader = 0;
		char *replaceShader = ReplaceShader(GetD3D11Device(), hash, L"vs", pShaderBytecode, BytecodeLength, replaceShaderSize,
			shaderModel, ftWrite, (void **)&zeroShader);
		if (replaceShader)
		{
			// Create the new shader.
			if (LogFile && LogDebug) fprintf(LogFile, "    D3D11Wrapper::ID3D11Device::CreateVertexShader.  Device: %p\n", GetD3D11Device());

			hr = GetD3D11Device()->CreateVertexShader(replaceShader, replaceShaderSize, pClassLinkage, ppVertexShader);
			if (SUCCEEDED(hr))
			{
				if (LogFile) fprintf(LogFile, "    shader successfully replaced.\n");

				if (G->hunting)
				{
					// Hunting mode:  keep byteCode around for possible replacement or marking
					D3D11Base::ID3DBlob* blob;
					D3DCreateBlob(replaceShaderSize, &blob);
					memcpy(blob->GetBufferPointer(), replaceShader, replaceShaderSize);
					RegisterForReload(*ppVertexShader, hash, L"vs", shaderModel, pClassLinkage, blob, ftWrite);

					if (G->marking_mode == MARKING_MODE_ORIGINAL)
					{
						// Compile original shader.
						D3D11Base::ID3D11VertexShader *originalShader;
						GetD3D11Device()->CreateVertexShader(pShaderBytecode, BytecodeLength, pClassLinkage, &originalShader);
						G->mOriginalVertexShaders[*ppVertexShader] = originalShader;
					}
				}
			}
			else
			{
				if (LogFile) fprintf(LogFile, "    error replacing shader.\n");
			}
			delete replaceShader; replaceShader = 0;
		}
	}
	if (hr != S_OK)
	{
		hr = GetD3D11Device()->CreateVertexShader(pShaderBytecode, BytecodeLength, pClassLinkage, ppVertexShader);

		// When in hunting mode, make a copy of the original binary, regardless.  This can be replaced, but we'll at least
		// have a copy for every shader seen.
		if (G->hunting)
		{
			D3D11Base::ID3DBlob* blob;
			D3DCreateBlob(BytecodeLength, &blob);
			memcpy(blob->GetBufferPointer(), pShaderBytecode, blob->GetBufferSize());
			RegisterForReload(*ppVertexShader, hash, L"vs", "bin", pClassLinkage, blob, ftWrite);
		}
	}
	if (hr == S_OK && ppVertexShader && pShaderBytecode)
	{
		if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
		G->mVertexShaders[*ppVertexShader] = hash;
		if (LogFile && LogDebug) fprintf(LogFile, "    Vertex shader registered: handle = %p, hash = %08lx%08lx\n", *ppVertexShader, (UINT32)(hash >> 32), (UINT32)hash);

		if ((G->marking_mode == MARKING_MODE_ZERO) && zeroShader)
		{
			G->mZeroVertexShaders[*ppVertexShader] = zeroShader;
		}

		CompiledShaderMap::iterator i = G->mCompiledShaderMap.find(hash);
		if (i != G->mCompiledShaderMap.end())
		{
			if (LogFile) fprintf(LogFile, "  shader was compiled from source code %s\n", i->second);
		}
		if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
	}

	if (LogFile) fprintf(LogFile, "  returns result = %x, handle = %p\n", hr, *ppVertexShader);

	return hr;
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
	if (LogFile) fprintf(LogFile, "ID3D11Device::CreateGeometryShader called with BytecodeLength = %d, handle = %p\n", BytecodeLength, pShaderBytecode);

	HRESULT hr = -1;
	UINT64 hash;
	if (pShaderBytecode && ppGeometryShader)
	{
		// Calculate hash
		hash = fnv_64_buf(pShaderBytecode, BytecodeLength);
		if (LogFile) fprintf(LogFile, "  bytecode hash = %08lx%08lx\n", (UINT32)(hash >> 32), (UINT32)hash);

		// :todo: Geometry shader
		/*
		D3D11Base::ID3DBlob *replaceShader = ReplaceShader(hash, L"gs", pShaderBytecode, BytecodeLength);
		if (replaceShader)
		{
		// Create the new shader.
		hr = m_pDevice->CreateGeometryShader(replaceShader->GetBufferPointer(),
		replaceShader->GetBufferSize(), pClassLinkage, ppGeometryShader);
		replaceShader->Release();
		if (hr == S_OK)
		{
		if (LogFile) fprintf(LogFile, "    shader successfully replaced.\n");
		}
		else
		{
		if (LogFile) fprintf(LogFile, "    error replacing shader.\n");
		}
		}
		*/
	}
	if (hr != S_OK)
	{
		hr = GetD3D11Device()->CreateGeometryShader(pShaderBytecode, BytecodeLength, pClassLinkage, ppGeometryShader);
	}
	if (hr == S_OK && ppGeometryShader && pShaderBytecode)
	{
		if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
		G->mGeometryShaders[*ppGeometryShader] = hash;
		if (LogFile && LogDebug) fprintf(LogFile, "    Geometry shader registered: handle = %p, hash = %08lx%08lx\n",
			*ppGeometryShader, (UINT32)(hash >> 32), (UINT32)hash);

		CompiledShaderMap::iterator i = G->mCompiledShaderMap.find(hash);
		if (i != G->mCompiledShaderMap.end())
		{
			if (LogFile) fprintf(LogFile, "  shader was compiled from source code %s\n", i->second);
		}
		if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
	}

	if (LogFile) fprintf(LogFile, "  returns result = %x, handle = %p\n", hr, *ppGeometryShader);

	return hr;
}

STDMETHODIMP D3D11Wrapper::ID3D11Device::CreateGeometryShaderWithStreamOutput(THIS_
	/* [annotation] */
	__in  const void *pShaderBytecode,
	/* [annotation] */
	__in  SIZE_T BytecodeLength,
	/* [annotation] */
	__in_ecount_opt(NumEntries)  const D3D11Base::D3D11_SO_DECLARATION_ENTRY *pSODeclaration,
	/* [annotation] */
	__in_range(0, D3D11_SO_STREAM_COUNT * D3D11_SO_OUTPUT_COMPONENT_COUNT)  UINT NumEntries,
	/* [annotation] */
	__in_ecount_opt(NumStrides)  const UINT *pBufferStrides,
	/* [annotation] */
	__in_range(0, D3D11_SO_BUFFER_SLOT_COUNT)  UINT NumStrides,
	/* [annotation] */
	__in  UINT RasterizedStream,
	/* [annotation] */
	__in_opt  D3D11Base::ID3D11ClassLinkage *pClassLinkage,
	/* [annotation] */
	__out_opt  D3D11Base::ID3D11GeometryShader **ppGeometryShader)
{
	if (LogFile) fprintf(LogFile, "ID3D11Device::CreateGeometryShaderWithStreamOutput called.\n");

	HRESULT hr = GetD3D11Device()->CreateGeometryShaderWithStreamOutput(pShaderBytecode, BytecodeLength, pSODeclaration,
		NumEntries, pBufferStrides, NumStrides, RasterizedStream, pClassLinkage, ppGeometryShader);
	if (LogFile) fprintf(LogFile, "  returns result = %x, handle = %p\n", hr, *ppGeometryShader);

	return hr;
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
	if (LogFile) fprintf(LogFile, "ID3D11Device::CreatePixelShader called with BytecodeLength = %d, handle = %p, ClassLinkage = %p\n", BytecodeLength, pShaderBytecode, pClassLinkage);

	HRESULT hr = -1;
	UINT64 hash;
	string shaderModel;
	SIZE_T replaceShaderSize;
	FILETIME ftWrite;
	D3D11Base::ID3D11PixelShader *zeroShader = 0;

	if (pShaderBytecode && ppPixelShader)
	{
		// Calculate hash
		hash = fnv_64_buf(pShaderBytecode, BytecodeLength);
		if (LogFile) fprintf(LogFile, "  bytecode hash = %08lx%08lx\n", (UINT32)(hash >> 32), (UINT32)hash);

		// Preloaded shader? (Can't use preloaded shaders with class linkage).
		if (!pClassLinkage)
		{
			PreloadPixelShaderMap::iterator i = G->mPreloadedPixelShaders.find(hash);
			if (i != G->mPreloadedPixelShaders.end())
			{
				*ppPixelShader = i->second;
				ULONG cnt = (*ppPixelShader)->AddRef();
				hr = S_OK;
				if (LogFile) fprintf(LogFile, "    shader assigned by preloaded version. ref counter = %d\n", cnt);

				if (G->marking_mode == MARKING_MODE_ZERO)
				{
					char *replaceShader = ReplaceShader(GetD3D11Device(), hash, L"ps", pShaderBytecode, BytecodeLength, replaceShaderSize,
						shaderModel, ftWrite, (void **)&zeroShader);
					delete replaceShader;
				}
				if (G->marking_mode == MARKING_MODE_ORIGINAL)
				{
					// Compile original shader.
					D3D11Base::ID3D11PixelShader *originalShader;
					GetD3D11Device()->CreatePixelShader(pShaderBytecode, BytecodeLength, pClassLinkage, &originalShader);
					G->mOriginalPixelShaders[*ppPixelShader] = originalShader;
				}
			}
		}
	}
	if (hr != S_OK && ppPixelShader && pShaderBytecode)
	{
		char *replaceShader = ReplaceShader(GetD3D11Device(), hash, L"ps", pShaderBytecode, BytecodeLength, replaceShaderSize,
			shaderModel, ftWrite, (void **)&zeroShader);
		if (replaceShader)
		{
			// Create the new shader.
			if (LogFile && LogDebug) fprintf(LogFile, "    D3D11Wrapper::ID3D11Device::CreatePixelShader.  Device: %p\n", GetD3D11Device());

			hr = GetD3D11Device()->CreatePixelShader(replaceShader, replaceShaderSize, pClassLinkage, ppPixelShader);
			if (SUCCEEDED(hr))
			{
				if (LogFile) fprintf(LogFile, "    shader successfully replaced.\n");

				if (G->hunting)
				{
					// Hunting mode:  keep byteCode around for possible replacement or marking
					D3D11Base::ID3DBlob* blob;
					D3DCreateBlob(replaceShaderSize, &blob);
					memcpy(blob->GetBufferPointer(), replaceShader, replaceShaderSize);
					RegisterForReload(*ppPixelShader, hash, L"ps", shaderModel, pClassLinkage, blob, ftWrite);

					if (G->marking_mode == MARKING_MODE_ORIGINAL)
					{
						// Compile original shader.
						D3D11Base::ID3D11PixelShader *originalShader;
						GetD3D11Device()->CreatePixelShader(pShaderBytecode, BytecodeLength, pClassLinkage, &originalShader);
						G->mOriginalPixelShaders[*ppPixelShader] = originalShader;
					}
				}
			}
			else
			{
				if (LogFile) fprintf(LogFile, "    error replacing shader.\n");
			}
			delete replaceShader; replaceShader = 0;
		}
	}
	if (hr != S_OK)
	{
		hr = GetD3D11Device()->CreatePixelShader(pShaderBytecode, BytecodeLength, pClassLinkage, ppPixelShader);

		// When in hunting mode, make a copy of the original binary, regardless.  This can be replaced, but we'll at least
		// have a copy for every shader seen.
		if (G->hunting)
		{
			D3D11Base::ID3DBlob* blob;
			D3DCreateBlob(BytecodeLength, &blob);
			memcpy(blob->GetBufferPointer(), pShaderBytecode, blob->GetBufferSize());
			RegisterForReload(*ppPixelShader, hash, L"ps", "bin", pClassLinkage, blob, ftWrite);
		}
	}
	if (hr == S_OK && ppPixelShader && pShaderBytecode)
	{
		if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
		G->mPixelShaders[*ppPixelShader] = hash;
		if (LogFile && LogDebug) fprintf(LogFile, "    Pixel shader: handle = %p, hash = %08lx%08lx\n", *ppPixelShader, (UINT32)(hash >> 32), (UINT32)hash);

		if ((G->marking_mode == MARKING_MODE_ZERO) && zeroShader)
		{
			G->mZeroPixelShaders[*ppPixelShader] = zeroShader;
		}

		CompiledShaderMap::iterator i = G->mCompiledShaderMap.find(hash);
		if (i != G->mCompiledShaderMap.end())
		{
			if (LogFile) fprintf(LogFile, "  shader was compiled from source code %s\n", i->second);
		}
		if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
	}

	if (LogFile) fprintf(LogFile, "  returns result = %x, handle = %p\n", hr, *ppPixelShader);

	return hr;
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
	if (LogFile) fprintf(LogFile, "ID3D11Device::CreateHullShader called with BytecodeLength = %d, handle = %p\n", BytecodeLength, pShaderBytecode);

	HRESULT hr = -1;
	UINT64 hash;
	if (pShaderBytecode && ppHullShader)
	{
		// Calculate hash
		hash = fnv_64_buf(pShaderBytecode, BytecodeLength);
		if (LogFile) fprintf(LogFile, "  bytecode hash = %08lx%08lx\n", (UINT32)(hash >> 32), (UINT32)hash);

		// :todo: Hull Shader
		/*
		D3D11Base::ID3DBlob *replaceShader = ReplaceShader(hash, L"hs", pShaderBytecode, BytecodeLength);
		if (replaceShader)
		{
		// Create the new shader.
		hr = m_pDevice->CreateHullShader(replaceShader->GetBufferPointer(),
		replaceShader->GetBufferSize(), pClassLinkage, ppHullShader);
		replaceShader->Release();
		if (hr == S_OK)
		{
		if (LogFile) fprintf(LogFile, "    shader successfully replaced.\n");
		}
		else
		{
		if (LogFile) fprintf(LogFile, "    error replacing shader.\n");
		}
		}
		*/
	}
	if (hr != S_OK)
	{
		hr = GetD3D11Device()->CreateHullShader(pShaderBytecode, BytecodeLength, pClassLinkage, ppHullShader);
	}
	if (hr == S_OK && ppHullShader && pShaderBytecode)
	{
		if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
		G->mHullShaders[*ppHullShader] = hash;
		if (LogFile && LogDebug) fprintf(LogFile, "    Hull shader: handle = %p, hash = %08lx%08lx\n",
			*ppHullShader, (UINT32)(hash >> 32), (UINT32)hash);

		CompiledShaderMap::iterator i = G->mCompiledShaderMap.find(hash);
		if (i != G->mCompiledShaderMap.end())
		{
			if (LogFile) fprintf(LogFile, "  shader was compiled from source code %s\n", i->second);
		}
		if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
	}

	if (LogFile) fprintf(LogFile, "  returns result = %x, handle = %p\n", hr, *ppHullShader);

	return hr;
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
	if (LogFile) fprintf(LogFile, "ID3D11Device::CreateDomainShader called with BytecodeLength = %d, handle = %p\n", BytecodeLength, pShaderBytecode);

	HRESULT hr = -1;
	UINT64 hash;
	if (pShaderBytecode && ppDomainShader)
	{
		// Calculate hash
		hash = fnv_64_buf(pShaderBytecode, BytecodeLength);
		if (LogFile) fprintf(LogFile, "  bytecode hash = %08lx%08lx\n", (UINT32)(hash >> 32), (UINT32)hash);

		// :todo: create domain shader
		/*
		D3D11Base::ID3DBlob *replaceShader = ReplaceShader(hash, L"ds", pShaderBytecode, BytecodeLength);
		if (replaceShader)
		{
		// Create the new shader.
		hr = m_pDevice->CreateDomainShader(replaceShader->GetBufferPointer(),
		replaceShader->GetBufferSize(), pClassLinkage, ppDomainShader);
		replaceShader->Release();
		if (hr == S_OK)
		{
		if (LogFile) fprintf(LogFile, "    shader successfully replaced.\n");
		}
		else
		{
		if (LogFile) fprintf(LogFile, "    error replacing shader.\n");
		}
		}
		*/
	}
	if (hr != S_OK)
	{
		hr = GetD3D11Device()->CreateDomainShader(pShaderBytecode, BytecodeLength, pClassLinkage, ppDomainShader);
	}
	if (hr == S_OK && ppDomainShader && pShaderBytecode)
	{
		if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
		G->mDomainShaders[*ppDomainShader] = hash;
		if (LogFile && LogDebug) fprintf(LogFile, "    Domain shader: handle = %p, hash = %08lx%08lx\n",
			*ppDomainShader, (UINT32)(hash >> 32), (UINT32)hash);

		CompiledShaderMap::iterator i = G->mCompiledShaderMap.find(hash);
		if (i != G->mCompiledShaderMap.end())
		{
			if (LogFile) fprintf(LogFile, "  shader was compiled from source code %s\n", i->second);
		}
		if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
	}

	if (LogFile) fprintf(LogFile, "  returns result = %x, handle = %p\n", hr, *ppDomainShader);

	return hr;
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
	if (LogFile) fprintf(LogFile, "ID3D11Device::CreateComputeShader called with BytecodeLength = %d, handle = %p\n", BytecodeLength, pShaderBytecode);

	HRESULT hr = -1;
	UINT64 hash;
	if (pShaderBytecode && ppComputeShader)
	{
		// Calculate hash
		hash = fnv_64_buf(pShaderBytecode, BytecodeLength);
		if (LogFile) fprintf(LogFile, "  bytecode hash = %08lx%08lx\n", (UINT32)(hash >> 32), (UINT32)hash);

		// :todo: Compute shader
		/*
		D3D11Base::ID3DBlob *replaceShader = ReplaceShader(hash, L"cs", pShaderBytecode, BytecodeLength);
		if (replaceShader)
		{
		// Create the new shader.
		hr = m_pDevice->CreateComputeShader(replaceShader->GetBufferPointer(),
		replaceShader->GetBufferSize(), pClassLinkage, ppComputeShader);
		replaceShader->Release();
		if (hr == S_OK)
		{
		if (LogFile) fprintf(LogFile, "    shader successfully replaced.\n");
		}
		else
		{
		if (LogFile) fprintf(LogFile, "    error replacing shader.\n");
		}
		}
		*/
	}
	if (hr != S_OK)
	{
		hr = GetD3D11Device()->CreateComputeShader(pShaderBytecode, BytecodeLength, pClassLinkage, ppComputeShader);
	}
	if (hr == S_OK && ppComputeShader && pShaderBytecode)
	{
		if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
		G->mComputeShaders[*ppComputeShader] = hash;
		if (LogFile && LogDebug) fprintf(LogFile, "    Compute shader: handle = %p, hash = %08lx%08lx\n",
			*ppComputeShader, (UINT32)(hash >> 32), (UINT32)hash);

		CompiledShaderMap::iterator i = G->mCompiledShaderMap.find(hash);
		if (i != G->mCompiledShaderMap.end())
		{
			if (LogFile) fprintf(LogFile, "  shader was compiled from source code %s\n", i->second);
		}
		if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
	}

	if (LogFile) fprintf(LogFile, "  returns result = %x, handle = %p\n", hr, *ppComputeShader);

	return hr;
}

STDMETHODIMP D3D11Wrapper::ID3D11Device::CreateClassLinkage(THIS_
	/* [annotation] */
	__out  D3D11Base::ID3D11ClassLinkage **ppLinkage)
{
	return GetD3D11Device()->CreateClassLinkage(ppLinkage);
}

STDMETHODIMP D3D11Wrapper::ID3D11Device::CreateBlendState(THIS_
	/* [annotation] */
	__in  const D3D11Base::D3D11_BLEND_DESC *pBlendStateDesc,
	/* [annotation] */
	__out_opt  D3D11Base::ID3D11BlendState **ppBlendState)
{
	return GetD3D11Device()->CreateBlendState(pBlendStateDesc, ppBlendState);
}

STDMETHODIMP D3D11Wrapper::ID3D11Device::CreateDepthStencilState(THIS_
	/* [annotation] */
	__in  const D3D11Base::D3D11_DEPTH_STENCIL_DESC *pDepthStencilDesc,
	/* [annotation] */
	__out_opt  D3D11Base::ID3D11DepthStencilState **ppDepthStencilState)
{
	return GetD3D11Device()->CreateDepthStencilState(pDepthStencilDesc, ppDepthStencilState);
}

STDMETHODIMP D3D11Wrapper::ID3D11Device::CreateRasterizerState(THIS_
	/* [annotation] */
	__in  D3D11Base::D3D11_RASTERIZER_DESC *pRasterizerDesc,
	/* [annotation] */
	__out_opt  D3D11Base::ID3D11RasterizerState **ppRasterizerState)
{
	if (LogFile && LogDebug && pRasterizerDesc) fprintf(LogFile, "ID3D11Device::CreateRasterizerState called with \n"
		"  FillMode = %d, CullMode = %d, DepthBias = %d, DepthBiasClamp = %f, SlopeScaledDepthBias = %f,\n"
		"  DepthClipEnable = %d, ScissorEnable = %d, MultisampleEnable = %d, AntialiasedLineEnable = %d\n",
		pRasterizerDesc->FillMode, pRasterizerDesc->CullMode, pRasterizerDesc->DepthBias, pRasterizerDesc->DepthBiasClamp,
		pRasterizerDesc->SlopeScaledDepthBias, pRasterizerDesc->DepthClipEnable, pRasterizerDesc->ScissorEnable,
		pRasterizerDesc->MultisampleEnable, pRasterizerDesc->AntialiasedLineEnable);

	if (G->SCISSOR_DISABLE && pRasterizerDesc && pRasterizerDesc->ScissorEnable)
	{
		if (LogFile && LogDebug) fprintf(LogFile, "  disabling scissor mode.\n");

		pRasterizerDesc->ScissorEnable = FALSE;
	}
	HRESULT hr = GetD3D11Device()->CreateRasterizerState(pRasterizerDesc, ppRasterizerState);
	if (LogFile && LogDebug) fprintf(LogFile, "  returns result = %x\n", hr);

	return hr;
}

STDMETHODIMP D3D11Wrapper::ID3D11Device::CreateSamplerState(THIS_
	/* [annotation] */
	__in  const D3D11Base::D3D11_SAMPLER_DESC *pSamplerDesc,
	/* [annotation] */
	__out_opt  D3D11Base::ID3D11SamplerState **ppSamplerState)
{
	return GetD3D11Device()->CreateSamplerState(pSamplerDesc, ppSamplerState);
}

STDMETHODIMP D3D11Wrapper::ID3D11Device::CreateQuery(THIS_
	/* [annotation] */
	__in  const D3D11Base::D3D11_QUERY_DESC *pQueryDesc,
	/* [annotation] */
	__out_opt  D3D11Base::ID3D11Query **ppQuery)
{
	return GetD3D11Device()->CreateQuery(pQueryDesc, ppQuery);
}

STDMETHODIMP D3D11Wrapper::ID3D11Device::CreatePredicate(THIS_
	/* [annotation] */
	__in  const D3D11Base::D3D11_QUERY_DESC *pPredicateDesc,
	/* [annotation] */
	__out_opt  D3D11Base::ID3D11Predicate **ppPredicate)
{
	return GetD3D11Device()->CreatePredicate(pPredicateDesc, ppPredicate);
}

STDMETHODIMP D3D11Wrapper::ID3D11Device::CreateCounter(THIS_
	/* [annotation] */
	__in  const D3D11Base::D3D11_COUNTER_DESC *pCounterDesc,
	/* [annotation] */
	__out_opt  D3D11Base::ID3D11Counter **ppCounter)
{
	return GetD3D11Device()->CreateCounter(pCounterDesc, ppCounter);
}

STDMETHODIMP D3D11Wrapper::ID3D11Device::CreateDeferredContext(THIS_
	UINT ContextFlags,
	/* [annotation] */
	__out_opt  D3D11Wrapper::ID3D11DeviceContext **ppDeferredContext)
{
	if (LogFile) fprintf(LogFile, "ID3D11Device::CreateDeferredContext called with flags = %x\n", ContextFlags);

	D3D11Base::ID3D11DeviceContext *origContext = 0;
	HRESULT ret = GetD3D11Device()->CreateDeferredContext(ContextFlags, &origContext);

	D3D11Wrapper::ID3D11DeviceContext *wrapper = D3D11Wrapper::ID3D11DeviceContext::GetDirect3DDeviceContext(origContext);
	if (wrapper == NULL)
	{
		if (LogFile) fprintf(LogFile, "  error allocating wrapper.\n");

		origContext->Release();
		return E_OUTOFMEMORY;
	}
	*ppDeferredContext = wrapper;

	if (LogFile) fprintf(LogFile, "  returns result = %x, handle = %p, wrapper = %p\n", ret, origContext, wrapper);

	return ret;
}

STDMETHODIMP D3D11Wrapper::ID3D11Device::OpenSharedResource(THIS_
	/* [annotation] */
	__in  HANDLE hResource,
	/* [annotation] */
	__in  REFIID ReturnedInterface,
	/* [annotation] */
	__out_opt  void **ppResource)
{
	return GetD3D11Device()->OpenSharedResource(hResource, ReturnedInterface, ppResource);
}

STDMETHODIMP D3D11Wrapper::ID3D11Device::CheckFormatSupport(THIS_
	/* [annotation] */
	__in  D3D11Base::DXGI_FORMAT Format,
	/* [annotation] */
	__out  UINT *pFormatSupport)
{
	return GetD3D11Device()->CheckFormatSupport(Format, pFormatSupport);
}

STDMETHODIMP D3D11Wrapper::ID3D11Device::CheckMultisampleQualityLevels(THIS_
	/* [annotation] */
	__in  D3D11Base::DXGI_FORMAT Format,
	/* [annotation] */
	__in  UINT SampleCount,
	/* [annotation] */
	__out  UINT *pNumQualityLevels)
{
	return GetD3D11Device()->CheckMultisampleQualityLevels(Format, SampleCount, pNumQualityLevels);
}

STDMETHODIMP_(void) D3D11Wrapper::ID3D11Device::CheckCounterInfo(THIS_
	/* [annotation] */
	__out  D3D11Base::D3D11_COUNTER_INFO *pCounterInfo)
{
	return GetD3D11Device()->CheckCounterInfo(pCounterInfo);
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
	return GetD3D11Device()->CheckCounter(pDesc, pType, pActiveCounters, szName, pNameLength, szUnits,
		pUnitsLength, szDescription, pDescriptionLength);
}

STDMETHODIMP D3D11Wrapper::ID3D11Device::CheckFeatureSupport(THIS_
	D3D11Base::D3D11_FEATURE Feature,
	/* [annotation] */
	__out_bcount(FeatureSupportDataSize)  void *pFeatureSupportData,
	UINT FeatureSupportDataSize)
{
	return GetD3D11Device()->CheckFeatureSupport(Feature, pFeatureSupportData, FeatureSupportDataSize);
}

STDMETHODIMP D3D11Wrapper::ID3D11Device::GetPrivateData(THIS_
	/* [annotation] */
	__in  REFGUID guid,
	/* [annotation] */
	__inout  UINT *pDataSize,
	/* [annotation] */
	__out_bcount_opt(*pDataSize)  void *pData)
{
	return GetD3D11Device()->GetPrivateData(guid, pDataSize, pData);
}

STDMETHODIMP D3D11Wrapper::ID3D11Device::SetPrivateData(THIS_
	/* [annotation] */
	__in  REFGUID guid,
	/* [annotation] */
	__in  UINT DataSize,
	/* [annotation] */
	__in_bcount_opt(DataSize)  const void *pData)
{
	return GetD3D11Device()->SetPrivateData(guid, DataSize, pData);
}

STDMETHODIMP D3D11Wrapper::ID3D11Device::SetPrivateDataInterface(THIS_
	/* [annotation] */
	__in  REFGUID guid,
	/* [annotation] */
	__in_opt  const IUnknown *pData)
{
	if (LogFile) fprintf(LogFile, "ID3D11Device::SetPrivateDataInterface called with GUID=%08lx-%04hx-%04hx-%02hhx%02hhx-%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx\n",
		guid.Data1, guid.Data2, guid.Data3, guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
		guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);

	return GetD3D11Device()->SetPrivateDataInterface(guid, pData);
}

STDMETHODIMP_(D3D11Base::D3D_FEATURE_LEVEL) D3D11Wrapper::ID3D11Device::GetFeatureLevel(THIS)
{
	return GetD3D11Device()->GetFeatureLevel();
}

STDMETHODIMP_(UINT) D3D11Wrapper::ID3D11Device::GetCreationFlags(THIS)
{
	return GetD3D11Device()->GetCreationFlags();
}

STDMETHODIMP D3D11Wrapper::ID3D11Device::GetDeviceRemovedReason(THIS)
{
	return GetD3D11Device()->GetDeviceRemovedReason();
}

STDMETHODIMP_(void) D3D11Wrapper::ID3D11Device::GetImmediateContext(THIS_
	/* [annotation] */
	__out  D3D11Wrapper::ID3D11DeviceContext **ppImmediateContext)
{
	D3D11Base::ID3D11DeviceContext *origContext = 0;
	GetD3D11Device()->GetImmediateContext(&origContext);
	// Check if wrapper exists.
	D3D11Wrapper::ID3D11DeviceContext *wrapper = (D3D11Wrapper::ID3D11DeviceContext*) D3D11Wrapper::ID3D11DeviceContext::m_List.GetDataPtr(origContext);
	if (wrapper)
	{
		*ppImmediateContext = wrapper;
		if (LogFile && LogDebug) fprintf(LogFile, "  returns handle = %p, wrapper = %p\n", origContext, wrapper);

		return;
	}
	if (LogFile) fprintf(LogFile, "ID3D11Device::GetImmediateContext called.\n");

	// Create wrapper.
	wrapper = D3D11Wrapper::ID3D11DeviceContext::GetDirect3DDeviceContext(origContext);
	if (wrapper == NULL)
	{
		if (LogFile) fprintf(LogFile, "  error allocating wrapper.\n");

		origContext->Release();
	}
	*ppImmediateContext = wrapper;
	if (LogFile) fprintf(LogFile, "  returns handle = %p, wrapper = %p\n", origContext, wrapper);

}

STDMETHODIMP D3D11Wrapper::ID3D11Device::SetExceptionMode(THIS_
	UINT RaiseFlags)
{
	return GetD3D11Device()->SetExceptionMode(RaiseFlags);
}

STDMETHODIMP_(UINT) D3D11Wrapper::ID3D11Device::GetExceptionMode(THIS)
{
	return GetD3D11Device()->GetExceptionMode();
}

