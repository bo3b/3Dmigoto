// Wrapper for the ID3D11Device.
// This gives us access to every D3D11 call for a device, and override the pieces needed.

#include "HackerDevice.h"

#include <D3Dcompiler.h>

#include "nvapi.h"
#include "log.h"
#include "util.h"
#include "DecompileHLSL.h"
#include "HackerContext.h"
#include "Globals.h"
#include "D3D11Wrapper.h"
#include "SpriteFont.h"

// -----------------------------------------------------------------------------------------------

HackerDevice::HackerDevice(ID3D11Device *pDevice, ID3D11DeviceContext *pContext)
	: ID3D11Device(),
	mStereoHandle(0), mStereoResourceView(0), mStereoTexture(0), 
	mIniResourceView(0), mIniTexture(0), 
	mZBufferResourceView(0)
{
	mOrigDevice = pDevice;
	mOrigContext = pContext;

	// Todo: This call will fail if stereo is disabled. Proper notification?
	NvAPI_Status hr = NvAPI_Stereo_CreateHandleFromIUnknown(this, &mStereoHandle);
	if (hr != NVAPI_OK)
	{
		mStereoHandle = 0;
		LogInfo("HackerDevice::HackerDevice NvAPI_Stereo_CreateHandleFromIUnknown failed: %d\n", hr);
	}
	mParamTextureManager.mStereoHandle = mStereoHandle;
	LogInfo("  created NVAPI stereo handle. Handle = %p\n", mStereoHandle);

	// Override custom settings.
	if (mStereoHandle && G->gSurfaceCreateMode >= 0)
	{
		NvAPIOverride();
		LogInfo("  setting custom surface creation mode.\n");

		if (NVAPI_OK != NvAPI_Stereo_SetSurfaceCreationMode(mStereoHandle,
			(NVAPI_STEREO_SURFACECREATEMODE) G->gSurfaceCreateMode))
		{
			LogInfo("    call failed.\n");
		}
	}
	// Create stereo parameter texture.
	if (mStereoHandle)
	{
		LogInfo("  creating stereo parameter texture.\n");

		D3D11_TEXTURE2D_DESC desc;
		memset(&desc, 0, sizeof(D3D11_TEXTURE2D_DESC));
		desc.Width = nv::stereo::ParamTextureManagerD3D11::Parms::StereoTexWidth;
		desc.Height = nv::stereo::ParamTextureManagerD3D11::Parms::StereoTexHeight;
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.Format = nv::stereo::ParamTextureManagerD3D11::Parms::StereoTexFormat;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.Usage = D3D11_USAGE_DEFAULT;
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		desc.CPUAccessFlags = 0;
		desc.MiscFlags = 0;
		HRESULT ret = mOrigDevice->CreateTexture2D(&desc, 0, &mStereoTexture);
		if (FAILED(ret))
		{
			LogInfo("    call failed with result = %x.\n", ret);
		}
		else
		{
			LogInfo("    stereo texture created, handle = %p\n", mStereoTexture);
			LogInfo("  creating stereo parameter resource view.\n");

			// Since we need to bind the texture to a shader input, we also need a resource view.
			D3D11_SHADER_RESOURCE_VIEW_DESC descRV;
			memset(&descRV, 0, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC));
			descRV.Format = desc.Format;
			descRV.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			descRV.Texture2D.MostDetailedMip = 0;
			descRV.Texture2D.MipLevels = -1;
			ret = mOrigDevice->CreateShaderResourceView(mStereoTexture, &descRV, &mStereoResourceView);
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
		D3D11_TEXTURE1D_DESC desc;
		memset(&desc, 0, sizeof(D3D11_TEXTURE1D_DESC));
		D3D11_SUBRESOURCE_DATA initialData;

		LogInfo("  creating .ini constant parameter texture.\n");

		// Stuff the constants read from the .ini file into the subresource data structure, so 
		// we can init the texture with them.
		initialData.pSysMem = &G->iniParams;
		initialData.SysMemPitch = sizeof(DirectX::XMFLOAT4) * 1;	// only one 4 element struct 

		desc.Width = 1;												// 1 texel, .rgba as a float4
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;	// float4
		desc.Usage = D3D11_USAGE_DYNAMIC;				// Read/Write access from GPU and CPU
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;		// As resource view, access via t120
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;				// allow CPU access for hotkeys
		desc.MiscFlags = 0;
		HRESULT ret = mOrigDevice->CreateTexture1D(&desc, &initialData, &mIniTexture);
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
			D3D11_SHADER_RESOURCE_VIEW_DESC descRV;
			memset(&descRV, 0, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC));

			ret = mOrigDevice->CreateShaderResourceView(mIniTexture, NULL, &mIniResourceView);
			if (FAILED(ret))
			{
				LogInfo("   CreateShaderResourceView call failed with result = %x.\n", ret);
			}

			LogInfo("    Iniparams resource view created, handle = %p.\n", mIniResourceView);
		}
	}

}

// Save reference to corresponding HackerContext during CreateDevice, needed for GetImmediateContext.

void HackerDevice::SetHackerContext(HackerContext *pHackerContext)
{
	mHackerContext = pHackerContext;
}

ID3D11Device* HackerDevice::GetOrigDevice()
{
	return mOrigDevice;
}

ID3D11DeviceContext* HackerDevice::GetOrigContext()
{
	return mOrigContext;
}


// No longer need this routine, we are storing Device and Context in the object

//HackerDevice::ID3D11Device* __cdecl HackerDevice::GetDirect3DDevice(ID3D11Device *pOrig)
//{
//	HackerDevice::ID3D11Device* p = (ID3D11Device*)m_List.GetDataPtr(pOrig);
//	if (!p)
//	{
//		p = new HackerDevice::ID3D11Device(pOrig);
//		if (pOrig) m_List.AddMember(pOrig, p);
//	}
//	return p;
//}


// -----------------------------------------------------------------------------------------------

/*** IUnknown methods ***/

STDMETHODIMP_(ULONG) HackerDevice::AddRef(THIS)
{
	return mOrigDevice->AddRef();
}

STDMETHODIMP_(ULONG) HackerDevice::Release(THIS)
{
	ULONG ulRef = mOrigDevice->Release();
	LogDebug("HackerDevice::Release counter=%d, this=%p\n", ulRef, this);
	
	if (ulRef <= 0)
	{
		LogInfo("HackerDevice::Release counter=%d, this=%p\n", ulRef, this);
		LogInfo("  deleting self\n");

		if (mStereoHandle)
		{
			int result = NvAPI_Stereo_DestroyHandle(mStereoHandle);
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
			G->mPreloadedPixelShaders.clear();		// No critical wrap for exiting.
		}
		if (!G->mPreloadedVertexShaders.empty())
		{
			LogInfo("  releasing preloaded vertex shaders\n");

			for (PreloadVertexShaderMap::iterator i = G->mPreloadedVertexShaders.begin(); i != G->mPreloadedVertexShaders.end(); ++i)
				i->second->Release();
			G->mPreloadedVertexShaders.clear();		// No critical wrap for exiting.
		}
		delete this;
		return 0L;
	}
	return ulRef;
}

HRESULT STDMETHODCALLTYPE HackerDevice::QueryInterface(
	/* [in] */ REFIID riid,
	/* [iid_is][out] */ _COM_Outptr_ void __RPC_FAR *__RPC_FAR *ppvObject)
{
	return mOrigDevice->QueryInterface(riid, ppvObject);
}



// -----------------------------------------------------------------------------------------------

// For any given vertex or pixel shader from the ShaderFixes folder, we need to track them at load time so
// that we can associate a given active shader with an override file.  This allows us to reload the shaders
// dynamically, and do on-the-fly fix testing.
// ShaderModel is usually something like "vs_5_0", but "bin" is a valid ShaderModel string, and tells the 
// reloader to disassemble the .bin file to determine the shader model.

// Currently, critical lock must be taken BEFORE this is called.

void HackerDevice::RegisterForReload(ID3D11DeviceChild* ppShader, UINT64 hash, wstring shaderType, string shaderModel,
		ID3D11ClassLinkage* pClassLinkage, ID3DBlob* byteCode, FILETIME timeStamp)
{
	LogInfo("    shader registered for possible reloading: %016llx_%ls as %s\n", hash, shaderType.c_str(), shaderModel.c_str());

	G->mReloadedShaders[ppShader].hash = hash;
	G->mReloadedShaders[ppShader].shaderType = shaderType;
	G->mReloadedShaders[ppShader].shaderModel = shaderModel;
	G->mReloadedShaders[ppShader].linkage = pClassLinkage;
	G->mReloadedShaders[ppShader].byteCode = byteCode;
	G->mReloadedShaders[ppShader].timeStamp = timeStamp;
	G->mReloadedShaders[ppShader].replacement = NULL;
}

void HackerDevice::PreloadVertexShader(wchar_t *path, WIN32_FIND_DATA &findFileData)
{
	wchar_t fileName[MAX_PATH];
	wsprintf(fileName, L"%ls\\%ls", path, findFileData.cFileName);
	char cFileName[MAX_PATH];

	if (LogFile) wcstombs(cFileName, findFileData.cFileName, MAX_PATH);
	HANDLE f = CreateFile(fileName, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (f == INVALID_HANDLE_VALUE)
	{
		LogInfo("  error reading binary vertex shader %s.\n", cFileName);
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
		LogInfo("  Error reading binary vertex shader %s.\n", cFileName);
		CloseHandle(f);
		return;
	}
	CloseHandle(f);

	LogInfo("  preloading vertex shader %s\n", cFileName);

	UINT64 hash = fnv_64_buf(pShaderBytecode, bytecodeLength);
	UINT64 keyHash = 0;
	for (int i = 0; i < 16; ++i)
	{
		UINT64 digit = findFileData.cFileName[i] > L'9' ? towupper(findFileData.cFileName[i]) - L'A' + 10 : findFileData.cFileName[i] - L'0';
		keyHash += digit << (60 - i * 4);
	}
	LogInfo("    key hash = %08lx%08lx, bytecode hash = %08lx%08lx\n",
		(UINT32)(keyHash >> 32), (UINT32)(keyHash),
		(UINT32)(hash >> 32), (UINT32)(hash));

	// Create the new shader.
	ID3D11VertexShader *pVertexShader;
	HRESULT hr = mOrigDevice->CreateVertexShader(pShaderBytecode, bytecodeLength, 0, &pVertexShader);
	if (FAILED(hr))
	{
		LogInfo("    error creating shader.\n");

		delete pShaderBytecode; pShaderBytecode = 0;
		return;
	}

	if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
		G->mPreloadedVertexShaders[keyHash] = pVertexShader;
		if (G->hunting)
		{
			ID3DBlob* blob;
			D3DCreateBlob(bytecodeLength, &blob);
			memcpy(blob->GetBufferPointer(), pShaderBytecode, bytecodeLength);
			RegisterForReload(pVertexShader, keyHash, L"vs", "bin", NULL, blob, ftWrite);
		}
		delete pShaderBytecode; pShaderBytecode = 0;
	if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
}

void HackerDevice::PreloadPixelShader(wchar_t *path, WIN32_FIND_DATA &findFileData)
{
	wchar_t fileName[MAX_PATH];
	wsprintf(fileName, L"%ls\\%ls", path, findFileData.cFileName);
	char cFileName[MAX_PATH];

	if (LogFile) wcstombs(cFileName, findFileData.cFileName, MAX_PATH);
	HANDLE f = CreateFile(fileName, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (f == INVALID_HANDLE_VALUE)
	{
		LogInfo("  error reading binary pixel shader %s.\n", cFileName);
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
		LogInfo("  Error reading binary pixel shader %s.\n", cFileName);
		CloseHandle(f);
		return;
	}
	CloseHandle(f);

	LogInfo("  preloading pixel shader %s\n", cFileName);

	UINT64 hash = fnv_64_buf(pShaderBytecode, bytecodeLength);
	UINT64 keyHash = 0;
	for (int i = 0; i < 16; ++i)
	{
		UINT64 digit = findFileData.cFileName[i] > L'9' ? towupper(findFileData.cFileName[i]) - L'A' + 10 : findFileData.cFileName[i] - L'0';
		keyHash += digit << (60 - i * 4);
	}
	LogInfo("    key hash = %08lx%08lx, bytecode hash = %08lx%08lx\n",
		(UINT32)(keyHash >> 32), (UINT32)(keyHash),
		(UINT32)(hash >> 32), (UINT32)(hash));

	// Create the new shader.
	ID3D11PixelShader *pPixelShader;
	HRESULT hr = mOrigDevice->CreatePixelShader(pShaderBytecode, bytecodeLength, 0, &pPixelShader);
	if (FAILED(hr))
	{
		LogInfo("    error creating shader.\n");

		delete pShaderBytecode; pShaderBytecode = 0;
		return;
	}

	if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
		G->mPreloadedPixelShaders[hash] = pPixelShader;
		if (G->hunting)
		{
			ID3DBlob* blob;
			D3DCreateBlob(bytecodeLength, &blob);
			memcpy(blob->GetBufferPointer(), pShaderBytecode, bytecodeLength);
			RegisterForReload(pPixelShader, keyHash, L"ps", "bin", NULL, blob, ftWrite);
		}
		delete pShaderBytecode; pShaderBytecode = 0;
	if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
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

char* HackerDevice::ReplaceShader(UINT64 hash, const wchar_t *shaderType, const void *pShaderBytecode,
	SIZE_T BytecodeLength, SIZE_T &pCodeSize, string &foundShaderModel, FILETIME &timeStamp, void **zeroShader)
{
	if (G->mBlockingMode)
		return 0;

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
			ID3DBlob *disassembly;
			HRESULT r = D3DDisassemble(pShaderBytecode, BytecodeLength,
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
					if (LogFile)
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
				ID3DBlob *disassembly;
				HRESULT ret = D3DDisassemble(pShaderBytecode, BytecodeLength,
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

					ID3DBlob *pErrorMsgs;
					ID3DBlob *pCompiledOutput = 0;
					ret = D3DCompile(srcData, srcDataSize, "wrapper1349", 0, ((ID3DInclude*)(UINT_PTR)1),
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

					if (LogFile && pErrorMsgs)
					{
						LPVOID errMsg = pErrorMsgs->GetBufferPointer();
						SIZE_T errSize = pErrorMsgs->GetBufferSize();
						LogInfo("--------------------------------------------- BEGIN ---------------------------------------------\n");
						fwrite(errMsg, 1, errSize - 1, LogFile);
						LogInfo("---------------------------------------------- END ----------------------------------------------\n");
						pErrorMsgs->Release();
					}

					// Cache binary replacement.
					if (G->CACHE_SHADERS && pCode)
					{
						wsprintf(val, L"%ls\\%08lx%08lx-%ls_replace.bin", G->SHADER_PATH, (UINT32)(hash >> 32), (UINT32)(hash), shaderType);
						FILE *fw;
						_wfopen_s(&fw, val, L"wb");
						if (LogFile)
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
			ID3DBlob *disassembly = 0;
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
			HRESULT ret = D3DDisassemble(pShaderBytecode, BytecodeLength,
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
					if (LogFile)
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

					ID3DBlob *pErrorMsgs;
					ID3DBlob *pCompiledOutput = 0;
					ret = D3DCompile(decompiledCode.c_str(), decompiledCode.size(), "wrapper1349", 0, ((ID3DInclude*)(UINT_PTR)1),
						"main", shaderModel.c_str(), D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &pCompiledOutput, &pErrorMsgs);
					LogInfo("    compile result of fixed HLSL shader: %x\n", ret);

					if (LogFile && pErrorMsgs)
					{
						LPVOID errMsg = pErrorMsgs->GetBufferPointer();
						SIZE_T errSize = pErrorMsgs->GetBufferSize();
						LogInfo("--------------------------------------------- BEGIN ---------------------------------------------\n");
						fwrite(errMsg, 1, errSize - 1, LogFile);
						LogInfo("------------------------------------------- HLSL code -------------------------------------------\n");
						fwrite(decompiledCode.c_str(), 1, decompiledCode.size(), LogFile);
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
						HRESULT ret = D3DDisassemble(pCompiledOutput->GetBufferPointer(), pCompiledOutput->GetBufferSize(),
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
		ID3DBlob *disassembly;
		HRESULT ret = D3DDisassemble(pShaderBytecode, BytecodeLength,
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

				ID3DBlob *pErrorMsgs;
				ID3DBlob *pCompiledOutput = 0;
				ret = D3DCompile(decompiledCode.c_str(), decompiledCode.size(), "wrapper1349", 0, ((ID3DInclude*)(UINT_PTR)1),
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
						ID3D11VertexShader *zeroVertexShader;
						HRESULT hr = mOrigDevice->CreateVertexShader(code, codeSize, 0, &zeroVertexShader);
						if (hr == S_OK)
							*zeroShader = zeroVertexShader;
					}
					else if (!wcscmp(shaderType, L"ps"))
					{
						ID3D11PixelShader *zeroPixelShader;
						HRESULT hr = mOrigDevice->CreatePixelShader(code, codeSize, 0, &zeroPixelShader);
						if (hr == S_OK)
							*zeroShader = zeroPixelShader;
					}
					delete code;
				}

				if (LogFile && pErrorMsgs)
				{
					LPVOID errMsg = pErrorMsgs->GetBufferPointer();
					SIZE_T errSize = pErrorMsgs->GetBufferSize();
					LogInfo("--------------------------------------------- BEGIN ---------------------------------------------\n");
					fwrite(errMsg, 1, errSize - 1, LogFile);
					LogInfo("------------------------------------------- HLSL code -------------------------------------------\n");
					fwrite(decompiledCode.c_str(), 1, decompiledCode.size(), LogFile);
					LogInfo("\n---------------------------------------------- END ----------------------------------------------\n");
					pErrorMsgs->Release();
				}
			}
		}
	}

	return pCode;
}

bool HackerDevice::NeedOriginalShader(UINT64 hash)
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
void HackerDevice::KeepOriginalShader(UINT64 hash, ID3D11VertexShader *pVertexShader, ID3D11PixelShader *pPixelShader,
	const void *pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage *pClassLinkage)
{
	if (!NeedOriginalShader(hash))
		return;

	LogInfo("    keeping original shader for filtering: %016llx\n", hash);

	if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
		if (pVertexShader) {
			ID3D11VertexShader *originalShader;
			mOrigDevice->CreateVertexShader(pShaderBytecode, BytecodeLength, pClassLinkage, &originalShader);
			G->mOriginalVertexShaders[pVertexShader] = originalShader;
		}
		else if (pPixelShader) {
			ID3D11PixelShader *originalShader;
			mOrigDevice->CreatePixelShader(pShaderBytecode, BytecodeLength, pClassLinkage, &originalShader);
			G->mOriginalPixelShaders[pPixelShader] = originalShader;
		}
	if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
}


// -----------------------------------------------------------------------------------------------

// These are the boilerplate routines that are necessary to pass through any calls to these
// to Direct3D.  Since Direct3D does not have proper objects, we can't rely on super class calls.

STDMETHODIMP HackerDevice::CreateUnorderedAccessView(THIS_
	/* [annotation] */
	__in  ID3D11Resource *pResource,
	/* [annotation] */
	__in_opt  const D3D11_UNORDERED_ACCESS_VIEW_DESC *pDesc,
	/* [annotation] */
	__out_opt  ID3D11UnorderedAccessView **ppUAView)
{
	return mOrigDevice->CreateUnorderedAccessView(pResource, pDesc, ppUAView);
}

STDMETHODIMP HackerDevice::CreateRenderTargetView(THIS_
	/* [annotation] */
	__in  ID3D11Resource *pResource,
	/* [annotation] */
	__in_opt  const D3D11_RENDER_TARGET_VIEW_DESC *pDesc,
	/* [annotation] */
	__out_opt  ID3D11RenderTargetView **ppRTView)
{
	return mOrigDevice->CreateRenderTargetView(pResource, pDesc, ppRTView);
}

STDMETHODIMP HackerDevice::CreateDepthStencilView(THIS_
	/* [annotation] */
	__in  ID3D11Resource *pResource,
	/* [annotation] */
	__in_opt  const D3D11_DEPTH_STENCIL_VIEW_DESC *pDesc,
	/* [annotation] */
	__out_opt  ID3D11DepthStencilView **ppDepthStencilView)
{
	return mOrigDevice->CreateDepthStencilView(pResource, pDesc, ppDepthStencilView);
}

STDMETHODIMP HackerDevice::CreateInputLayout(THIS_
	/* [annotation] */
	__in_ecount(NumElements)  const D3D11_INPUT_ELEMENT_DESC *pInputElementDescs,
	/* [annotation] */
	__in_range(0, D3D11_IA_VERTEX_INPUT_STRUCTURE_ELEMENT_COUNT)  UINT NumElements,
	/* [annotation] */
	__in  const void *pShaderBytecodeWithInputSignature,
	/* [annotation] */
	__in  SIZE_T BytecodeLength,
	/* [annotation] */
	__out_opt  ID3D11InputLayout **ppInputLayout)
{
	return mOrigDevice->CreateInputLayout(pInputElementDescs, NumElements, pShaderBytecodeWithInputSignature,
		BytecodeLength, ppInputLayout);
}

STDMETHODIMP HackerDevice::CreateClassLinkage(THIS_
	/* [annotation] */
	__out  ID3D11ClassLinkage **ppLinkage)
{
	return mOrigDevice->CreateClassLinkage(ppLinkage);
}

STDMETHODIMP HackerDevice::CreateBlendState(THIS_
	/* [annotation] */
	__in  const D3D11_BLEND_DESC *pBlendStateDesc,
	/* [annotation] */
	__out_opt  ID3D11BlendState **ppBlendState)
{
	return mOrigDevice->CreateBlendState(pBlendStateDesc, ppBlendState);
}

STDMETHODIMP HackerDevice::CreateDepthStencilState(THIS_
	/* [annotation] */
	__in  const D3D11_DEPTH_STENCIL_DESC *pDepthStencilDesc,
	/* [annotation] */
	__out_opt  ID3D11DepthStencilState **ppDepthStencilState)
{
	return mOrigDevice->CreateDepthStencilState(pDepthStencilDesc, ppDepthStencilState);
}

STDMETHODIMP HackerDevice::CreateSamplerState(THIS_
	/* [annotation] */
	__in  const D3D11_SAMPLER_DESC *pSamplerDesc,
	/* [annotation] */
	__out_opt  ID3D11SamplerState **ppSamplerState)
{
	return mOrigDevice->CreateSamplerState(pSamplerDesc, ppSamplerState);
}

STDMETHODIMP HackerDevice::CreateQuery(THIS_
	/* [annotation] */
	__in  const D3D11_QUERY_DESC *pQueryDesc,
	/* [annotation] */
	__out_opt  ID3D11Query **ppQuery)
{
	return mOrigDevice->CreateQuery(pQueryDesc, ppQuery);
}

STDMETHODIMP HackerDevice::CreatePredicate(THIS_
	/* [annotation] */
	__in  const D3D11_QUERY_DESC *pPredicateDesc,
	/* [annotation] */
	__out_opt  ID3D11Predicate **ppPredicate)
{
	return mOrigDevice->CreatePredicate(pPredicateDesc, ppPredicate);
}

STDMETHODIMP HackerDevice::CreateCounter(THIS_
	/* [annotation] */
	__in  const D3D11_COUNTER_DESC *pCounterDesc,
	/* [annotation] */
	__out_opt  ID3D11Counter **ppCounter)
{
	return mOrigDevice->CreateCounter(pCounterDesc, ppCounter);
}

STDMETHODIMP HackerDevice::OpenSharedResource(THIS_
	/* [annotation] */
	__in  HANDLE hResource,
	/* [annotation] */
	__in  REFIID ReturnedInterface,
	/* [annotation] */
	__out_opt  void **ppResource)
{
	return mOrigDevice->OpenSharedResource(hResource, ReturnedInterface, ppResource);
}

STDMETHODIMP HackerDevice::CheckFormatSupport(THIS_
	/* [annotation] */
	__in  DXGI_FORMAT Format,
	/* [annotation] */
	__out  UINT *pFormatSupport)
{
	return mOrigDevice->CheckFormatSupport(Format, pFormatSupport);
}

STDMETHODIMP HackerDevice::CheckMultisampleQualityLevels(THIS_
	/* [annotation] */
	__in  DXGI_FORMAT Format,
	/* [annotation] */
	__in  UINT SampleCount,
	/* [annotation] */
	__out  UINT *pNumQualityLevels)
{
	return mOrigDevice->CheckMultisampleQualityLevels(Format, SampleCount, pNumQualityLevels);
}

STDMETHODIMP_(void) HackerDevice::CheckCounterInfo(THIS_
	/* [annotation] */
	__out  D3D11_COUNTER_INFO *pCounterInfo)
{
	return mOrigDevice->CheckCounterInfo(pCounterInfo);
}

STDMETHODIMP HackerDevice::CheckCounter(THIS_
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
	return mOrigDevice->CheckCounter(pDesc, pType, pActiveCounters, szName, pNameLength, szUnits,
		pUnitsLength, szDescription, pDescriptionLength);
}

STDMETHODIMP HackerDevice::CheckFeatureSupport(THIS_
	D3D11_FEATURE Feature,
	/* [annotation] */
	__out_bcount(FeatureSupportDataSize)  void *pFeatureSupportData,
	UINT FeatureSupportDataSize)
{
	return mOrigDevice->CheckFeatureSupport(Feature, pFeatureSupportData, FeatureSupportDataSize);
}

STDMETHODIMP HackerDevice::GetPrivateData(THIS_
	/* [annotation] */
	__in  REFGUID guid,
	/* [annotation] */
	__inout  UINT *pDataSize,
	/* [annotation] */
	__out_bcount_opt(*pDataSize)  void *pData)
{
	return mOrigDevice->GetPrivateData(guid, pDataSize, pData);
}

STDMETHODIMP HackerDevice::SetPrivateData(THIS_
	/* [annotation] */
	__in  REFGUID guid,
	/* [annotation] */
	__in  UINT DataSize,
	/* [annotation] */
	__in_bcount_opt(DataSize)  const void *pData)
{
	return mOrigDevice->SetPrivateData(guid, DataSize, pData);
}

STDMETHODIMP HackerDevice::SetPrivateDataInterface(THIS_
	/* [annotation] */
	__in  REFGUID guid,
	/* [annotation] */
	__in_opt  const IUnknown *pData)
{
	LogInfo("HackerDevice::SetPrivateDataInterface called with GUID=%08lx-%04hx-%04hx-%02hhx%02hhx-%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx\n",
		guid.Data1, guid.Data2, guid.Data3, guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
		guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]);

	return mOrigDevice->SetPrivateDataInterface(guid, pData);
}

STDMETHODIMP_(D3D_FEATURE_LEVEL) HackerDevice::GetFeatureLevel(THIS)
{
	return mOrigDevice->GetFeatureLevel();
}

STDMETHODIMP_(UINT) HackerDevice::GetCreationFlags(THIS)
{
	return mOrigDevice->GetCreationFlags();
}

STDMETHODIMP HackerDevice::GetDeviceRemovedReason(THIS)
{
	return mOrigDevice->GetDeviceRemovedReason();
}

STDMETHODIMP HackerDevice::SetExceptionMode(THIS_
	UINT RaiseFlags)
{
	return mOrigDevice->SetExceptionMode(RaiseFlags);
}

STDMETHODIMP_(UINT) HackerDevice::GetExceptionMode(THIS)
{
	return mOrigDevice->GetExceptionMode();
}



// -----------------------------------------------------------------------------------------------

STDMETHODIMP HackerDevice::CreateBuffer(THIS_
	/* [annotation] */
	__in  const D3D11_BUFFER_DESC *pDesc,
	/* [annotation] */
	__in_opt  const D3D11_SUBRESOURCE_DATA *pInitialData,
	/* [annotation] */
	__out_opt  ID3D11Buffer **ppBuffer)
{
	/*
	LogInfo("ID3D11Device::CreateBuffer called\n");
	LogInfo("  ByteWidth = %d\n", pDesc->ByteWidth);
	LogInfo("  Usage = %d\n", pDesc->Usage);
	LogInfo("  BindFlags = %x\n", pDesc->BindFlags);
	LogInfo("  CPUAccessFlags = %x\n", pDesc->CPUAccessFlags);
	LogInfo("  MiscFlags = %x\n", pDesc->MiscFlags);
	LogInfo("  StructureByteStride = %d\n", pDesc->StructureByteStride);
	LogInfo("  InitialData = %p\n", pInitialData);
	*/
	HRESULT hr = mOrigDevice->CreateBuffer(pDesc, pInitialData, ppBuffer);
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
		if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
			G->mDataBuffers[*ppBuffer] = hash;
		if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
		LogDebug("    Buffer registered: handle = %p, hash = %08lx%08lx\n", *ppBuffer, (UINT32)(hash >> 32), (UINT32)hash);
	}
	return hr;
}

STDMETHODIMP HackerDevice::CreateTexture1D(THIS_
	/* [annotation] */
	__in  const D3D11_TEXTURE1D_DESC *pDesc,
	/* [annotation] */
	__in_xcount_opt(pDesc->MipLevels * pDesc->ArraySize)  const D3D11_SUBRESOURCE_DATA *pInitialData,
	/* [annotation] */
	__out_opt  ID3D11Texture1D **ppTexture1D)
{
	return mOrigDevice->CreateTexture1D(pDesc, pInitialData, ppTexture1D);
}

STDMETHODIMP HackerDevice::CreateTexture2D(THIS_
	/* [annotation] */
	__in  const D3D11_TEXTURE2D_DESC *pDesc,
	/* [annotation] */
	__in_xcount_opt(pDesc->MipLevels * pDesc->ArraySize)  const D3D11_SUBRESOURCE_DATA *pInitialData,
	/* [annotation] */
	__out_opt  ID3D11Texture2D **ppTexture2D)
{
	TextureOverride *textureOverride = NULL;
	bool override = false;

	LogDebug("HackerDevice::CreateTexture2D called with parameters\n");
	if (pDesc) LogDebug("  Width = %d, Height = %d, MipLevels = %d, ArraySize = %d\n",
		pDesc->Width, pDesc->Height, pDesc->MipLevels, pDesc->ArraySize);
	if (pDesc) LogDebug("  Format = %d, Usage = %x, BindFlags = %x, CPUAccessFlags = %x, MiscFlags = %x\n",
		pDesc->Format, pDesc->Usage, pDesc->BindFlags, pDesc->CPUAccessFlags, pDesc->MiscFlags);

	// Preload shaders?
	if (G->PRELOAD_SHADERS && G->mPreloadedVertexShaders.empty() && G->mPreloadedPixelShaders.empty())
	{
		LogInfo("  preloading custom shaders.\n");

		wchar_t fileName[MAX_PATH];
		if (G->SHADER_PATH[0])
		{
			wsprintf(fileName, L"%ls\\*-vs_replace.bin", G->SHADER_PATH);
			WIN32_FIND_DATA findFileData;
			HANDLE hFind = FindFirstFile(fileName, &findFileData);
			if (hFind != INVALID_HANDLE_VALUE)
			{
				BOOL found = true;
				while (found)
				{
					PreloadVertexShader(G->SHADER_PATH, findFileData);
					found = FindNextFile(hFind, &findFileData);
				}
				FindClose(hFind);
			}
		}
		if (G->SHADER_CACHE_PATH[0])
		{
			wsprintf(fileName, L"%ls\\*-vs_replace.bin", G->SHADER_CACHE_PATH);
			WIN32_FIND_DATA findFileData;
			HANDLE hFind = FindFirstFile(fileName, &findFileData);
			if (hFind != INVALID_HANDLE_VALUE)
			{
				BOOL found = true;
				while (found)
				{
					PreloadVertexShader(G->SHADER_CACHE_PATH, findFileData);
					found = FindNextFile(hFind, &findFileData);
				}
				FindClose(hFind);
			}
		}
		if (G->SHADER_PATH[0])
		{
			wsprintf(fileName, L"%ls\\*-ps_replace.bin", G->SHADER_PATH);
			WIN32_FIND_DATA findFileData;
			HANDLE hFind = FindFirstFile(fileName, &findFileData);
			if (hFind != INVALID_HANDLE_VALUE)
			{
				BOOL found = true;
				while (found)
				{
					PreloadPixelShader(G->SHADER_PATH, findFileData);
					found = FindNextFile(hFind, &findFileData);
				}
				FindClose(hFind);
			}
		}
		if (G->SHADER_CACHE_PATH[0])
		{
			wsprintf(fileName, L"%ls\\*-ps_replace.bin", G->SHADER_CACHE_PATH);
			WIN32_FIND_DATA findFileData;
			HANDLE hFind = FindFirstFile(fileName, &findFileData);
			if (hFind != INVALID_HANDLE_VALUE)
			{
				BOOL found = true;
				while (found)
				{
					PreloadPixelShader(G->SHADER_CACHE_PATH, findFileData);
					found = FindNextFile(hFind, &findFileData);
				}
				FindClose(hFind);
			}
		}
	}

	// Get screen resolution.
	int hashWidth = 0;
	int hashHeight = 0;
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
		hash = CalcTexture2DDescHash(pDesc, hash, hashWidth, hashHeight);
	LogDebug("  InitialData = %p, hash = %08lx%08lx\n", pInitialData, (UINT32)(hash >> 32), (UINT32)hash);

	// Override custom settings?
	NVAPI_STEREO_SURFACECREATEMODE oldMode = (NVAPI_STEREO_SURFACECREATEMODE) - 1, newMode = (NVAPI_STEREO_SURFACECREATEMODE) - 1;
	D3D11_TEXTURE2D_DESC newDesc = *pDesc;

	TextureOverrideMap::iterator i = G->mTextureOverrideMap.find(hash);
	if (i != G->mTextureOverrideMap.end()) {
		textureOverride = &i->second;

		override = true;
		if (textureOverride->stereoMode != -1)
			newMode = (NVAPI_STEREO_SURFACECREATEMODE) textureOverride->stereoMode;
#if 0 /* Iterations are broken since we no longer use present() */
		// Check iteration.
		if (!textureOverride->iterations.empty()) {
			std::vector<int>::iterator k = textureOverride->iterations.begin();
			int currentIteration = textureOverride->iterations[0] = textureOverride->iterations[0] + 1;
			LogInfo("  current iteration = %d\n", currentIteration);

			override = false;
			while (++k != textureOverride->iterations.end())
			{
				if (currentIteration == *k)
				{
					override = true;
					break;
				}
			}
			if (!override)
			{
				LogInfo("  override skipped\n");
			}
		}
#endif
	}
	if (pDesc && G->gSurfaceSquareCreateMode >= 0 && pDesc->Width == pDesc->Height && (pDesc->Usage & D3D11_USAGE_IMMUTABLE) == 0)
	{
		override = true;
		newMode = (NVAPI_STEREO_SURFACECREATEMODE) G->gSurfaceSquareCreateMode;
	}
	if (override)
	{
		if (newMode != (NVAPI_STEREO_SURFACECREATEMODE) - 1)
		{
			NvAPI_Stereo_GetSurfaceCreationMode(mStereoHandle, &oldMode);
			NvAPIOverride();
			LogInfo("  setting custom surface creation mode.\n");

			if (NVAPI_OK != NvAPI_Stereo_SetSurfaceCreationMode(mStereoHandle, newMode))
			{
				LogInfo("    call failed.\n");
			}
		}
		if (textureOverride && textureOverride->format != -1)
		{
			LogInfo("  setting custom format to %d\n", textureOverride->format);

			newDesc.Format = (DXGI_FORMAT) textureOverride->format;
		}
	}
	HRESULT hr = mOrigDevice->CreateTexture2D(&newDesc, pInitialData, ppTexture2D);
	if (oldMode != (NVAPI_STEREO_SURFACECREATEMODE) - 1)
	{
		if (NVAPI_OK != NvAPI_Stereo_SetSurfaceCreationMode(mStereoHandle, oldMode))
		{
			LogInfo("    restore call failed.\n");
		}
	}
	if (ppTexture2D) LogDebug("  returns result = %x, handle = %p\n", hr, *ppTexture2D);

	// Register texture.
	if (ppTexture2D)
	{
		if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
			G->mTexture2D_ID[*ppTexture2D] = hash;
		if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
	}
	return hr;
}

STDMETHODIMP HackerDevice::CreateTexture3D(THIS_
	/* [annotation] */
	__in  const D3D11_TEXTURE3D_DESC *pDesc,
	/* [annotation] */
	__in_xcount_opt(pDesc->MipLevels)  const D3D11_SUBRESOURCE_DATA *pInitialData,
	/* [annotation] */
	__out_opt  ID3D11Texture3D **ppTexture3D)
{
	LogInfo("HackerDevice::CreateTexture3D called with parameters\n");
	if (pDesc) LogInfo("  Width = %d, Height = %d, Depth = %d, MipLevels = %d, InitialData = %p\n",
		pDesc->Width, pDesc->Height, pDesc->Depth, pDesc->MipLevels, pInitialData);
	if (pDesc) LogInfo("  Format = %d, Usage = %x, BindFlags = %x, CPUAccessFlags = %x, MiscFlags = %x\n",
		pDesc->Format, pDesc->Usage, pDesc->BindFlags, pDesc->CPUAccessFlags, pDesc->MiscFlags);

	// Get screen resolution.
	int hashWidth = 0;
	int hashHeight = 0;
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
		hash = CalcTexture3DDescHash(pDesc, hash, hashWidth, hashHeight);
	LogInfo("  InitialData = %p, hash = %08lx%08lx\n", pInitialData, (UINT32)(hash >> 32), (UINT32)hash);

	HRESULT hr = mOrigDevice->CreateTexture3D(pDesc, pInitialData, ppTexture3D);

	// Register texture.
	if (hr == S_OK && ppTexture3D)
	{
		if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
			G->mTexture3D_ID[*ppTexture3D] = hash;
		if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
	}

	LogInfo("  returns result = %x\n", hr);

	return hr;
}

STDMETHODIMP HackerDevice::CreateShaderResourceView(THIS_
	/* [annotation] */
	__in  ID3D11Resource *pResource,
	/* [annotation] */
	__in_opt  const D3D11_SHADER_RESOURCE_VIEW_DESC *pDesc,
	/* [annotation] */
	__out_opt  ID3D11ShaderResourceView **ppSRView)
{
	LogDebug("HackerDevice::CreateShaderResourceView called\n");

	HRESULT hr = mOrigDevice->CreateShaderResourceView(pResource, pDesc, ppSRView);

	// Check for depth buffer view.
	if (hr == S_OK && G->ZBufferHashToInject && ppSRView)
	{
		unordered_map<ID3D11Texture2D *, UINT64>::iterator i = G->mTexture2D_ID.find((ID3D11Texture2D *) pResource);
		if (i != G->mTexture2D_ID.end() && i->second == G->ZBufferHashToInject)
		{
			LogInfo("  resource view of z buffer found: handle = %p, hash = %08lx%08lx\n", *ppSRView, (UINT32)(i->second >> 32), (UINT32)i->second);

			mZBufferResourceView = *ppSRView;
		}
	}

	LogDebug("  returns result = %x\n", hr);

	return hr;
}

STDMETHODIMP HackerDevice::CreateVertexShader(THIS_
	/* [annotation] */
	__in  const void *pShaderBytecode,
	/* [annotation] */
	__in  SIZE_T BytecodeLength,
	/* [annotation] */
	__in_opt  ID3D11ClassLinkage *pClassLinkage,
	/* [annotation] */
	__out_opt  ID3D11VertexShader **ppVertexShader)
{
	LogInfo("HackerDevice::CreateVertexShader called with BytecodeLength = %Iu, handle = %p, ClassLinkage = %p\n", BytecodeLength, pShaderBytecode, pClassLinkage);

	HRESULT hr = -1;
	UINT64 hash;
	string shaderModel;
	SIZE_T replaceShaderSize;
	FILETIME ftWrite;
	ID3D11VertexShader *zeroShader = 0;

	if (pShaderBytecode && ppVertexShader)
	{
		// Calculate hash
		hash = fnv_64_buf(pShaderBytecode, BytecodeLength);
		LogInfo("  bytecode hash = %016llx\n", hash);
		LogInfo("  bytecode hash = %08lx%08lx\n", (UINT32)(hash >> 32), (UINT32)hash);

		// Preloaded shader? (Can't use preloaded shaders with class linkage).
		if (!pClassLinkage)
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
					char *replaceShader = ReplaceShader(hash, L"vs", pShaderBytecode, BytecodeLength, replaceShaderSize,
						shaderModel, ftWrite, (void **)&zeroShader);
					delete replaceShader;
				}
				KeepOriginalShader(hash, *ppVertexShader, NULL, pShaderBytecode, BytecodeLength, pClassLinkage);
			}
		}
	}
	if (hr != S_OK && ppVertexShader && pShaderBytecode)
	{
		ID3D11VertexShader *zeroShader = 0;
		char *replaceShader = ReplaceShader(hash, L"vs", pShaderBytecode, BytecodeLength, replaceShaderSize,
			shaderModel, ftWrite, (void **)&zeroShader);
		if (replaceShader)
		{
			// Create the new shader.
			LogDebug("    HackerDevice::CreateVertexShader.  Device: %p\n", this);

			hr = mOrigDevice->CreateVertexShader(replaceShader, replaceShaderSize, pClassLinkage, ppVertexShader);
			if (SUCCEEDED(hr))
			{
				LogInfo("    shader successfully replaced.\n");

				if (G->hunting)
				{
					// Hunting mode:  keep byteCode around for possible replacement or marking
					ID3DBlob* blob;
					D3DCreateBlob(replaceShaderSize, &blob);
					memcpy(blob->GetBufferPointer(), replaceShader, replaceShaderSize);
					if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
						RegisterForReload(*ppVertexShader, hash, L"vs", shaderModel, pClassLinkage, blob, ftWrite);
					if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
				}
				KeepOriginalShader(hash, *ppVertexShader, NULL, pShaderBytecode, BytecodeLength, pClassLinkage);
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
		hr = mOrigDevice->CreateVertexShader(pShaderBytecode, BytecodeLength, pClassLinkage, ppVertexShader);

		// When in hunting mode, make a copy of the original binary, regardless.  This can be replaced, but we'll at least
		// have a copy for every shader seen.
		if (G->hunting)
		{
			if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
				ID3DBlob* blob;
				D3DCreateBlob(BytecodeLength, &blob);
				memcpy(blob->GetBufferPointer(), pShaderBytecode, blob->GetBufferSize());
				RegisterForReload(*ppVertexShader, hash, L"vs", "bin", pClassLinkage, blob, ftWrite);

				// Also add the original shader to the original shaders
				// map so that if it is later replaced marking_mode =
				// original and depth buffer filtering will work:
				if (G->mOriginalVertexShaders.count(*ppVertexShader) == 0)
					G->mOriginalVertexShaders[*ppVertexShader] = *ppVertexShader;
			if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
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

STDMETHODIMP HackerDevice::CreateGeometryShader(THIS_
	/* [annotation] */
	__in  const void *pShaderBytecode,
	/* [annotation] */
	__in  SIZE_T BytecodeLength,
	/* [annotation] */
	__in_opt  ID3D11ClassLinkage *pClassLinkage,
	/* [annotation] */
	__out_opt  ID3D11GeometryShader **ppGeometryShader)
{
	LogInfo("HackerDevice::CreateGeometryShader called with BytecodeLength = %Iu, handle = %p\n", BytecodeLength, pShaderBytecode);

	HRESULT hr = -1;
	UINT64 hash;
	if (pShaderBytecode && ppGeometryShader)
	{
		// Calculate hash
		hash = fnv_64_buf(pShaderBytecode, BytecodeLength);
		LogInfo("  bytecode hash = %08lx%08lx\n", (UINT32)(hash >> 32), (UINT32)hash);

		// :todo: Geometry shader
		/*
		ID3DBlob *replaceShader = ReplaceShader(hash, L"gs", pShaderBytecode, BytecodeLength);
		if (replaceShader)
		{
		// Create the new shader.
		hr = m_pDevice->CreateGeometryShader(replaceShader->GetBufferPointer(),
		replaceShader->GetBufferSize(), pClassLinkage, ppGeometryShader);
		replaceShader->Release();
		if (hr == S_OK)
		{
		LogInfo("    shader successfully replaced.\n");
		}
		else
		{
		LogInfo("    error replacing shader.\n");
		}
		}
		*/
	}
	if (hr != S_OK)
	{
		hr = mOrigDevice->CreateGeometryShader(pShaderBytecode, BytecodeLength, pClassLinkage, ppGeometryShader);
	}
	if (hr == S_OK && ppGeometryShader && pShaderBytecode)
	{
		if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
			G->mGeometryShaders[*ppGeometryShader] = hash;
			LogDebug("    Geometry shader registered: handle = %p, hash = %08lx%08lx\n",
				*ppGeometryShader, (UINT32)(hash >> 32), (UINT32)hash);

			CompiledShaderMap::iterator i = G->mCompiledShaderMap.find(hash);
			if (i != G->mCompiledShaderMap.end())
			{
				LogInfo("  shader was compiled from source code %s\n", i->second.c_str());
			}
		if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
	}

	LogInfo("  returns result = %x, handle = %p\n", hr, (ppGeometryShader ? *ppGeometryShader : NULL));

	return hr;
}

STDMETHODIMP HackerDevice::CreateGeometryShaderWithStreamOutput(THIS_
	/* [annotation] */
	__in  const void *pShaderBytecode,
	/* [annotation] */
	__in  SIZE_T BytecodeLength,
	/* [annotation] */
	__in_ecount_opt(NumEntries)  const D3D11_SO_DECLARATION_ENTRY *pSODeclaration,
	/* [annotation] */
	__in_range(0, D3D11_SO_STREAM_COUNT * D3D11_SO_OUTPUT_COMPONENT_COUNT)  UINT NumEntries,
	/* [annotation] */
	__in_ecount_opt(NumStrides)  const UINT *pBufferStrides,
	/* [annotation] */
	__in_range(0, D3D11_SO_BUFFER_SLOT_COUNT)  UINT NumStrides,
	/* [annotation] */
	__in  UINT RasterizedStream,
	/* [annotation] */
	__in_opt  ID3D11ClassLinkage *pClassLinkage,
	/* [annotation] */
	__out_opt  ID3D11GeometryShader **ppGeometryShader)
{
	LogInfo("HackerDevice::CreateGeometryShaderWithStreamOutput called.\n");

	HRESULT hr = mOrigDevice->CreateGeometryShaderWithStreamOutput(pShaderBytecode, BytecodeLength, pSODeclaration,
		NumEntries, pBufferStrides, NumStrides, RasterizedStream, pClassLinkage, ppGeometryShader);
	LogInfo("  returns result = %x, handle = %p\n", hr, (ppGeometryShader ? *ppGeometryShader : NULL));

	return hr;
}

STDMETHODIMP HackerDevice::CreatePixelShader(THIS_
	/* [annotation] */
	__in  const void *pShaderBytecode,
	/* [annotation] */
	__in  SIZE_T BytecodeLength,
	/* [annotation] */
	__in_opt  ID3D11ClassLinkage *pClassLinkage,
	/* [annotation] */
	__out_opt  ID3D11PixelShader **ppPixelShader)
{
	LogInfo("HackerDevice::CreatePixelShader called with BytecodeLength = %Iu, handle = %p, ClassLinkage = %p\n", BytecodeLength, pShaderBytecode, pClassLinkage);

	HRESULT hr = -1;
	UINT64 hash;
	string shaderModel;
	SIZE_T replaceShaderSize;
	FILETIME ftWrite;
	ID3D11PixelShader *zeroShader = 0;

	if (pShaderBytecode && ppPixelShader)
	{
		// Calculate hash
		hash = fnv_64_buf(pShaderBytecode, BytecodeLength);
		LogInfo("  bytecode hash = %08lx%08lx\n", (UINT32)(hash >> 32), (UINT32)hash);

		// Preloaded shader? (Can't use preloaded shaders with class linkage).
		if (!pClassLinkage)
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
					char *replaceShader = ReplaceShader(hash, L"ps", pShaderBytecode, BytecodeLength, replaceShaderSize,
						shaderModel, ftWrite, (void **)&zeroShader);
					delete replaceShader;
				}
				KeepOriginalShader(hash, NULL, *ppPixelShader, pShaderBytecode, BytecodeLength, pClassLinkage);
			}
		}
	}
	if (hr != S_OK && ppPixelShader && pShaderBytecode)
	{
		char *replaceShader = ReplaceShader(hash, L"ps", pShaderBytecode, BytecodeLength, replaceShaderSize,
			shaderModel, ftWrite, (void **)&zeroShader);
		if (replaceShader)
		{
			// Create the new shader.
			LogDebug("    HackerDevice::CreatePixelShader.  Device: %p\n", this);

			hr = mOrigDevice->CreatePixelShader(replaceShader, replaceShaderSize, pClassLinkage, ppPixelShader);
			if (SUCCEEDED(hr))
			{
				LogInfo("    shader successfully replaced.\n");

				if (G->hunting)
				{
					// Hunting mode:  keep byteCode around for possible replacement or marking
					ID3DBlob* blob;
					D3DCreateBlob(replaceShaderSize, &blob);
					memcpy(blob->GetBufferPointer(), replaceShader, replaceShaderSize);
					if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
						RegisterForReload(*ppPixelShader, hash, L"ps", shaderModel, pClassLinkage, blob, ftWrite);
					if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
				}
				KeepOriginalShader(hash, NULL, *ppPixelShader, pShaderBytecode, BytecodeLength, pClassLinkage);
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
		hr = mOrigDevice->CreatePixelShader(pShaderBytecode, BytecodeLength, pClassLinkage, ppPixelShader);

		// When in hunting mode, make a copy of the original binary, regardless.  This can be replaced, but we'll at least
		// have a copy for every shader seen.
		if (G->hunting)
		{
			if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
				ID3DBlob* blob;
				D3DCreateBlob(BytecodeLength, &blob);
				memcpy(blob->GetBufferPointer(), pShaderBytecode, blob->GetBufferSize());
				RegisterForReload(*ppPixelShader, hash, L"ps", "bin", pClassLinkage, blob, ftWrite);

				// Also add the original shader to the original shaders
				// map so that if it is later replaced marking_mode =
				// original and depth buffer filtering will work:
				if (G->mOriginalPixelShaders.count(*ppPixelShader) == 0)
					G->mOriginalPixelShaders[*ppPixelShader] = *ppPixelShader;
			if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
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

STDMETHODIMP HackerDevice::CreateHullShader(THIS_
	/* [annotation] */
	__in  const void *pShaderBytecode,
	/* [annotation] */
	__in  SIZE_T BytecodeLength,
	/* [annotation] */
	__in_opt  ID3D11ClassLinkage *pClassLinkage,
	/* [annotation] */
	__out_opt  ID3D11HullShader **ppHullShader)
{
	LogInfo("HackerDevice::CreateHullShader called with BytecodeLength = %Iu, handle = %p\n", BytecodeLength, pShaderBytecode);

	HRESULT hr = -1;
	UINT64 hash;
	if (pShaderBytecode && ppHullShader)
	{
		// Calculate hash
		hash = fnv_64_buf(pShaderBytecode, BytecodeLength);
		LogInfo("  bytecode hash = %08lx%08lx\n", (UINT32)(hash >> 32), (UINT32)hash);

		// :todo: Hull Shader
		/*
		ID3DBlob *replaceShader = ReplaceShader(hash, L"hs", pShaderBytecode, BytecodeLength);
		if (replaceShader)
		{
		// Create the new shader.
		hr = m_pDevice->CreateHullShader(replaceShader->GetBufferPointer(),
		replaceShader->GetBufferSize(), pClassLinkage, ppHullShader);
		replaceShader->Release();
		if (hr == S_OK)
		{
		LogInfo("    shader successfully replaced.\n");
		}
		else
		{
		LogInfo("    error replacing shader.\n");
		}
		}
		*/
	}
	if (hr != S_OK)
	{
		hr = mOrigDevice->CreateHullShader(pShaderBytecode, BytecodeLength, pClassLinkage, ppHullShader);
	}
	if (hr == S_OK && ppHullShader && pShaderBytecode)
	{
		if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
			G->mHullShaders[*ppHullShader] = hash;
			LogDebug("    Hull shader: handle = %p, hash = %08lx%08lx\n",
				*ppHullShader, (UINT32)(hash >> 32), (UINT32)hash);

			CompiledShaderMap::iterator i = G->mCompiledShaderMap.find(hash);
			if (i != G->mCompiledShaderMap.end())
			{
				LogInfo("  shader was compiled from source code %s\n", i->second.c_str());
			}
		if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
	}

	LogInfo("  returns result = %x, handle = %p\n", hr, (ppHullShader ? *ppHullShader : NULL));

	return hr;
}

STDMETHODIMP HackerDevice::CreateDomainShader(THIS_
	/* [annotation] */
	__in  const void *pShaderBytecode,
	/* [annotation] */
	__in  SIZE_T BytecodeLength,
	/* [annotation] */
	__in_opt  ID3D11ClassLinkage *pClassLinkage,
	/* [annotation] */
	__out_opt  ID3D11DomainShader **ppDomainShader)
{
	LogInfo("HackerDevice::CreateDomainShader called with BytecodeLength = %Iu, handle = %p\n", BytecodeLength, pShaderBytecode);

	HRESULT hr = -1;
	UINT64 hash;
	if (pShaderBytecode && ppDomainShader)
	{
		// Calculate hash
		hash = fnv_64_buf(pShaderBytecode, BytecodeLength);
		LogInfo("  bytecode hash = %08lx%08lx\n", (UINT32)(hash >> 32), (UINT32)hash);

		// :todo: create domain shader
		/*
		ID3DBlob *replaceShader = ReplaceShader(hash, L"ds", pShaderBytecode, BytecodeLength);
		if (replaceShader)
		{
		// Create the new shader.
		hr = m_pDevice->CreateDomainShader(replaceShader->GetBufferPointer(),
		replaceShader->GetBufferSize(), pClassLinkage, ppDomainShader);
		replaceShader->Release();
		if (hr == S_OK)
		{
		LogInfo("    shader successfully replaced.\n");
		}
		else
		{
		LogInfo("    error replacing shader.\n");
		}
		}
		*/
	}
	if (hr != S_OK)
	{
		hr = mOrigDevice->CreateDomainShader(pShaderBytecode, BytecodeLength, pClassLinkage, ppDomainShader);
	}
	if (hr == S_OK && ppDomainShader && pShaderBytecode)
	{
		if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
			G->mDomainShaders[*ppDomainShader] = hash;
			LogDebug("    Domain shader: handle = %p, hash = %08lx%08lx\n",
				*ppDomainShader, (UINT32)(hash >> 32), (UINT32)hash);

			CompiledShaderMap::iterator i = G->mCompiledShaderMap.find(hash);
			if (i != G->mCompiledShaderMap.end())
			{
				LogInfo("  shader was compiled from source code %s\n", i->second.c_str());
			}
		if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
	}

	LogInfo("  returns result = %x, handle = %p\n", hr, (ppDomainShader ? *ppDomainShader : NULL));

	return hr;
}

STDMETHODIMP HackerDevice::CreateComputeShader(THIS_
	/* [annotation] */
	__in  const void *pShaderBytecode,
	/* [annotation] */
	__in  SIZE_T BytecodeLength,
	/* [annotation] */
	__in_opt  ID3D11ClassLinkage *pClassLinkage,
	/* [annotation] */
	__out_opt  ID3D11ComputeShader **ppComputeShader)
{
	LogInfo("HackerDevice::CreateComputeShader called with BytecodeLength = %Iu, handle = %p\n", BytecodeLength, pShaderBytecode);

	HRESULT hr = -1;
	UINT64 hash;
	if (pShaderBytecode && ppComputeShader)
	{
		// Calculate hash
		hash = fnv_64_buf(pShaderBytecode, BytecodeLength);
		LogInfo("  bytecode hash = %08lx%08lx\n", (UINT32)(hash >> 32), (UINT32)hash);

		// :todo: Compute shader
		/*
		ID3DBlob *replaceShader = ReplaceShader(hash, L"cs", pShaderBytecode, BytecodeLength);
		if (replaceShader)
		{
		// Create the new shader.
		hr = m_pDevice->CreateComputeShader(replaceShader->GetBufferPointer(),
		replaceShader->GetBufferSize(), pClassLinkage, ppComputeShader);
		replaceShader->Release();
		if (hr == S_OK)
		{
		LogInfo("    shader successfully replaced.\n");
		}
		else
		{
		LogInfo("    error replacing shader.\n");
		}
		}
		*/
	}
	if (hr != S_OK)
	{
		hr = mOrigDevice->CreateComputeShader(pShaderBytecode, BytecodeLength, pClassLinkage, ppComputeShader);
	}
	if (hr == S_OK && ppComputeShader && pShaderBytecode)
	{
		if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
			G->mComputeShaders[*ppComputeShader] = hash;
			LogDebug("    Compute shader: handle = %p, hash = %08lx%08lx\n",
				*ppComputeShader, (UINT32)(hash >> 32), (UINT32)hash);

			CompiledShaderMap::iterator i = G->mCompiledShaderMap.find(hash);
			if (i != G->mCompiledShaderMap.end())
			{
				LogInfo("  shader was compiled from source code %s\n", i->second.c_str());
			}
		if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
	}

	LogInfo("  returns result = %x, handle = %p\n", hr, (ppComputeShader ? *ppComputeShader : NULL));

	return hr;
}

STDMETHODIMP HackerDevice::CreateRasterizerState(THIS_
	/* [annotation] */
	__in const D3D11_RASTERIZER_DESC *pRasterizerDesc,
	/* [annotation] */
	__out_opt  ID3D11RasterizerState **ppRasterizerState)
{
	HRESULT hr;

	if (pRasterizerDesc) LogDebug("HackerDevice::CreateRasterizerState called with \n"
		"  FillMode = %d, CullMode = %d, DepthBias = %d, DepthBiasClamp = %f, SlopeScaledDepthBias = %f,\n"
		"  DepthClipEnable = %d, ScissorEnable = %d, MultisampleEnable = %d, AntialiasedLineEnable = %d\n",
		pRasterizerDesc->FillMode, pRasterizerDesc->CullMode, pRasterizerDesc->DepthBias, pRasterizerDesc->DepthBiasClamp,
		pRasterizerDesc->SlopeScaledDepthBias, pRasterizerDesc->DepthClipEnable, pRasterizerDesc->ScissorEnable,
		pRasterizerDesc->MultisampleEnable, pRasterizerDesc->AntialiasedLineEnable);

	if (G->SCISSOR_DISABLE && pRasterizerDesc && pRasterizerDesc->ScissorEnable)
	{
		LogDebug("  disabling scissor mode.\n");

		// input is const- so we need to make a copy to change.
		D3D11_RASTERIZER_DESC rasterizerDesc;
		memcpy(&pRasterizerDesc, pRasterizerDesc, sizeof(D3D11_RASTERIZER_DESC));

		rasterizerDesc.ScissorEnable = FALSE;
		hr = mOrigDevice->CreateRasterizerState(&rasterizerDesc, ppRasterizerState);
	}
	else
	{
		hr = mOrigDevice->CreateRasterizerState(pRasterizerDesc, ppRasterizerState);
	}

	LogDebug("  returns result = %x\n", hr);
	return hr;
}

// This method creates a Context, and we want to return a wrapped/hacker
// version as the result. The method signature requires an 
// ID3D11DeviceContext, but we return our HackerContext.
// In general, I do not believe this is how contexts are created- they are
// usually going to be part of CreateDevice and CreateDeviceAndSwapChain.

// A deferred context is for multithreading part of the drawing and is rarely
// if ever used.
// Not positive that the SetHackerDevice using 'this' is correct.

STDMETHODIMP HackerDevice::CreateDeferredContext(THIS_
	UINT ContextFlags,
	/* [annotation] */
	__out_opt  ID3D11DeviceContext **ppDeferredContext)
{
	LogInfo("*** Double check context is correct ****\n\n");
	LogInfo("HackerDevice::CreateDeferredContext called with flags = %x\n", ContextFlags);

	ID3D11DeviceContext *deferContext = 0;
	HRESULT ret = -1;

	if (*ppDeferredContext)
	{
		ret = mOrigDevice->CreateDeferredContext(ContextFlags, &deferContext);
		HackerContext *hackerContext = new HackerContext(mOrigDevice, deferContext);
		hackerContext->SetHackerDevice(this);
		*ppDeferredContext = hackerContext;

		LogInfo("  returns result = %x, handle = %p, wrapper = %s\n", ret, deferContext, typeid(*ppDeferredContext).name());
		LogInfo("\n*** Double check context is correct ****\n");
	}

	return ret;
}

// Another variant where we want to return a HackerContext instead of the
// real one.  Creating a new HackerContext is not correct here, because we 
// need to provide the one created originally with the device.

// This is a main way to get the context when you only have the device.
// There is only one context per device.

STDMETHODIMP_(void) HackerDevice::GetImmediateContext(THIS_
	/* [annotation] */
	__out  ID3D11DeviceContext **ppImmediateContext)
{
	LogInfo("*** Double check context is correct ****\n\n");
	LogInfo("HackerDevice::GetImmediateContext called.\n");

	*ppImmediateContext = mHackerContext;

	LogInfo("  returns handle = %p\n", typeid(*ppImmediateContext).name());
	LogInfo("\n*** Double check context is correct ****\n");

	// Original code for reference:
/*	D3D11Base::ID3D11DeviceContext *origContext = 0;
	GetD3D11Device()->GetImmediateContext(&origContext);
	// Check if wrapper exists.
	D3D11Wrapper::ID3D11DeviceContext *wrapper = (D3D11Wrapper::ID3D11DeviceContext*) D3D11Wrapper::ID3D11DeviceContext::m_List.GetDataPtr(origContext);
	if (wrapper)
	{
		*ppImmediateContext = wrapper;
		LogDebug("  returns handle = %p, wrapper = %p\n", origContext, wrapper);

		return;
	}
	LogInfo("ID3D11Device::GetImmediateContext called.\n");

	// Create wrapper.
	wrapper = D3D11Wrapper::ID3D11DeviceContext::GetDirect3DDeviceContext(origContext);
	if (wrapper == NULL)
	{
		LogInfo("  error allocating wrapper.\n");

		origContext->Release();
	}
	*ppImmediateContext = wrapper;
	LogInfo("  returns handle = %p, wrapper = %p\n", origContext, wrapper);
*/
}

