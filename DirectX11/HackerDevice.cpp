// Wrapper for the ID3D11Device.
// This gives us access to every D3D11 call for a device, and override the pieces needed.

#include "HackerDevice.h"
#include "HackerDXGI.h"

#include <D3Dcompiler.h>

#include "nvapi.h"
#include "log.h"
#include "util.h"
#include "DecompileHLSL.h"
#include "HackerContext.h"
#include "Globals.h"
#include "D3D11Wrapper.h"
#include "SpriteFont.h"
#include "Assembler.h"

// -----------------------------------------------------------------------------------------------

HackerDevice::HackerDevice(ID3D11Device *pDevice, ID3D11DeviceContext *pContext)
	: ID3D11Device(),
	mStereoHandle(0), mStereoResourceView(0), mStereoTexture(0),
	mIniResourceView(0), mIniTexture(0),
	mZBufferResourceView(0), 
	mHackerContext(0), mHackerSwapChain(0)
{
	mOrigDevice = pDevice;
	mOrigContext = pContext;
}

// With the addition of full DXGI support, this init sequence is too dangerous
// to do at object creation time.  The NV CreateHandleFromIUnknown calls back
// into this device, so we need to have it set up and ready.

HRESULT HackerDevice::CreateStereoAndIniTextures()
{
	// Todo: This call will fail if stereo is disabled. Proper notification?
	NvAPI_Status hr = NvAPI_Stereo_CreateHandleFromIUnknown(this, &mStereoHandle);
	if (hr != NVAPI_OK)
	{
		mStereoHandle = 0;
		LogInfo("HackerDevice::HackerDevice NvAPI_Stereo_CreateHandleFromIUnknown failed: %d \n", hr);
		return hr;
	}
	mParamTextureManager.mStereoHandle = mStereoHandle;
	LogInfo("  created NVAPI stereo handle. Handle = %p \n", mStereoHandle);

	// Override custom settings.
	if (mStereoHandle && G->gSurfaceCreateMode >= 0)
	{
		NvAPIOverride();
		LogInfo("  setting custom surface creation mode.\n");

		hr = NvAPI_Stereo_SetSurfaceCreationMode(mStereoHandle,	(NVAPI_STEREO_SURFACECREATEMODE)G->gSurfaceCreateMode);
		if (hr != NVAPI_OK)
		{
			LogInfo("    custom surface creation call failed: %d. \n", hr);
			return hr;
		}
	}

	// Create stereo parameter texture.
	LogInfo("  creating stereo parameter texture. \n");

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
		LogInfo("    call failed with result = %x. \n", ret);
		return hr;
	}
	LogInfo("    stereo texture created, handle = %p \n", mStereoTexture);

	// Since we need to bind the texture to a shader input, we also need a resource view.
	LogInfo("  creating stereo parameter resource view. \n");

	D3D11_SHADER_RESOURCE_VIEW_DESC descRV;
	memset(&descRV, 0, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC));
	descRV.Format = desc.Format;
	descRV.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	descRV.Texture2D.MostDetailedMip = 0;
	descRV.Texture2D.MipLevels = -1;
	ret = mOrigDevice->CreateShaderResourceView(mStereoTexture, &descRV, &mStereoResourceView);
	if (FAILED(ret))
	{
		LogInfo("    call failed with result = %x. \n", ret);
		return ret;
	}
	LogInfo("    stereo texture resource view created, handle = %p.\n", mStereoResourceView);

	// If any constants are specified in the .ini file that need to be sent to shaders, we need 
	// to create the resource view in order to deliver them via SetShaderResources.
	// Check for depth buffer view.
	//
	// No longer making this conditional. We are pretty well dependent on
	// the ini params these days and not creating this view might cause
	// issues with config reload.
	// if ((G->iniParams.x != FLT_MAX) || (G->iniParams.y != FLT_MAX) || (G->iniParams.z != FLT_MAX) || (G->iniParams.w != FLT_MAX))
	{
		D3D11_TEXTURE1D_DESC desc;
		memset(&desc, 0, sizeof(D3D11_TEXTURE1D_DESC));
		D3D11_SUBRESOURCE_DATA initialData;

		LogInfo("  creating .ini constant parameter texture.\n");

		// Stuff the constants read from the .ini file into the subresource data structure, so 
		// we can init the texture with them.
		initialData.pSysMem = &G->iniParams;
		initialData.SysMemPitch = sizeof(DirectX::XMFLOAT4) * INI_PARAMS_SIZE;	// Ignored for Texture1D, but still recommended for debugging

		desc.Width = INI_PARAMS_SIZE;						// n texels, .rgba as a float4
		desc.MipLevels = 1;
		desc.ArraySize = 1;
		desc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;				// float4
		desc.Usage = D3D11_USAGE_DYNAMIC;							// Read/Write access from GPU and CPU
		desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;				// As resource view, access via t120
		desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;				// allow CPU access for hotkeys
		desc.MiscFlags = 0;
		ret = mOrigDevice->CreateTexture1D(&desc, &initialData, &mIniTexture);
		if (FAILED(ret))
		{
			LogInfo("    CreateTexture1D call failed with result = %x.\n", ret);
			return ret;
		}
		LogInfo("    IniParam texture created, handle = %p\n", mIniTexture);

		// Since we need to bind the texture to a shader input, we also need a resource view.
		// The pDesc is set to NULL so that it will simply use the desc format above.
		LogInfo("  creating IniParam resource view.\n");

		D3D11_SHADER_RESOURCE_VIEW_DESC descRV;
		memset(&descRV, 0, sizeof(D3D11_SHADER_RESOURCE_VIEW_DESC));

		ret = mOrigDevice->CreateShaderResourceView(mIniTexture, NULL, &mIniResourceView);
		if (FAILED(ret))
		{
			LogInfo("   CreateShaderResourceView call failed with result = %x.\n", ret);
			return ret;
		}

		LogInfo("    Iniparams resource view created, handle = %p.\n", mIniResourceView);
	}

	// Only create special pink mode PixelShader when requested.
	if (G->marking_mode == MARKING_MODE_PINK)
	{
		char* hlsl =
			"float4 pshader() : SV_Target0"
			"{"
			"	return float4(1,0,1,1);"
			"}";

		ID3D10Blob* blob = NULL;
		HRESULT hr = D3DCompile(hlsl, strlen(hlsl), "JustPink", NULL, NULL, "pshader", "ps_4_0", 0, 0, &blob, NULL);
		LogInfo("  Created pink mode pixel shader: %d \n", hr);
		if (SUCCEEDED(hr))
		{
			hr = mOrigDevice->CreatePixelShader((DWORD*)blob->GetBufferPointer(), blob->GetBufferSize(), NULL, &G->mPinkingShader);
			if (FAILED(hr))
				LogInfo("  Failed to create pinking pixel shader: %d \n", hr);
			blob->Release();
		}
	}

	return ret;
}


// Save reference to corresponding HackerContext during CreateDevice, needed for GetImmediateContext.

void HackerDevice::SetHackerContext(HackerContext *pHackerContext)
{
	mHackerContext = pHackerContext;
}

HackerContext* HackerDevice::GetHackerContext()
{
	LogInfo("HackerDevice::GetHackerContext returns %p \n", mHackerContext);
	return mHackerContext;
}

void HackerDevice::SetHackerSwapChain(HackerDXGISwapChain *pHackerSwapChain)
{
	mHackerSwapChain = pHackerSwapChain;
}


ID3D11Device* HackerDevice::GetOrigDevice()
{
	return mOrigDevice;
}

ID3D11DeviceContext* HackerDevice::GetOrigContext()
{
	return mOrigContext;
}

IDXGISwapChain* HackerDevice::GetOrigSwapChain()
{
	return mHackerSwapChain->GetOrigSwapChain();
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

// If called with IDXGIDevice, that's the game trying to access the original DXGIFactory to 
// get access to the swap chain.  We need to return a HackerDXGIDevice so that we can get 
// access to that swap chain.
// 
// This is the 'secret' path to getting the DXGIFactory and thus the swap chain, without
// having to go direct to DXGI calls. As described:
// https://msdn.microsoft.com/en-us/library/windows/desktop/bb174535(v=vs.85).aspx
//
// This technique is used in Mordor for sure, and very likely others.
//
// New addition, we need to also look for QueryInterface casts to different types.
// In Dragon Age, it seems clear that they are upcasting their ID3D11Device to an
// ID3D11Device1, and if we don't wrap that, we have an object leak where they can bypass us.
//
// Next up, it seems that we also need to handle a QueryInterface(IDXGIDevice1), as
// WatchDogs uses that call.  Another oddity: this device is called to return the
// same device. ID3D11Device->QueryInterface(ID3D11Device).  No idea why, but we
// need to return our wrapped version.
// 
// Initial call needs to be LogDebug, because this is otherwise far to chatty in the
// log.  That can be kind of misleading, so careful with missing log info. To
// keep it consistent, all normal cases will be LogDebug, error states are LogInfo.

HRESULT STDMETHODCALLTYPE HackerDevice::QueryInterface(
	/* [in] */ REFIID riid,
	/* [iid_is][out] */ _COM_Outptr_ void __RPC_FAR *__RPC_FAR *ppvObject)
{
	LogDebug("HackerDevice::QueryInterface(%s@%p) called with IID: %s \n", typeid(*this).name(), this, NameFromIID(riid).c_str());

	HRESULT hr = mOrigDevice->QueryInterface(riid, ppvObject);
	if (FAILED(hr))
	{
		LogInfo("  failed result = %x for %p \n", hr, ppvObject);
		return hr;
	}

	// No need for further checks of null ppvObject, as it could not have successfully
	// called the original in that case.

	if (riid == __uuidof(IDXGIDevice))
	{
		IDXGIDevice *origDXGIDevice = static_cast<IDXGIDevice*>(*ppvObject);
		HackerDXGIDevice *dxgiDeviceWrap = new HackerDXGIDevice(origDXGIDevice, this);
		*ppvObject = dxgiDeviceWrap;
		LogDebug("  created HackerDXGIDevice(%s@%p) wrapper of %p \n", typeid(*dxgiDeviceWrap).name(), dxgiDeviceWrap, origDXGIDevice);
	}
	else if (riid == __uuidof(IDXGIDevice1))
	{
		IDXGIDevice1 *origDXGIDevice1 = static_cast<IDXGIDevice1*>(*ppvObject);
		HackerDXGIDevice1 *dxgiDeviceWrap1 = new HackerDXGIDevice1(origDXGIDevice1, this);
		*ppvObject = dxgiDeviceWrap1;
		LogDebug("  created HackerDXGIDevice1(%s@%p) wrapper of %p \n", typeid(*dxgiDeviceWrap1).name(), dxgiDeviceWrap1, origDXGIDevice1);
	}
	else if (riid == __uuidof(IDXGIDevice2))
	{
		// an IDXGIDevice2 can only be created on platform update or above, so let's 
		// continue the philosophy of returning errors for anything optional.
		LogDebug("  returns E_NOINTERFACE as error for IDXGIDevice2. \n");
		*ppvObject = NULL;
		return E_NOINTERFACE;
	}
	else if (riid == __uuidof(ID3D11Device))
	{
		*ppvObject = this;
		LogDebug("  return HackerDevice(%s@%p) wrapper of %p \n", typeid(*this).name(), this, mOrigDevice);
	}
	else if (riid == __uuidof(ID3D11Device1))
	{
		// Well, bizarrely, this approach to upcasting to a ID3D11Device1 is supported on Win7, 
		// but only if you have the 'evil update', the platform update installed.  Since that
		// is an optional update, that certainly means that numerous people do not have it 
		// installed. Ergo, a game developer cannot in good faith just assume that it's there,
		// and it's very unlikely they would require it. No performance advantage on Win8.
		// So, that means that a game developer must support a fallback path, even if they
		// actually want Device1 for some reason.
		//
		// Sooo... Current plan is to return an error here, and pretend that the platform
		// update is not installed, or missing feature on Win8.1.  This will force the game
		// to use a more compatible path and make our job easier.
		// This worked in DragonAge, to avoid a crash. Wrapping Device1 also progressed but
		// adds a ton of undesirable complexity, so let's keep it simpler since we don't 
		// seem to lose anything. Not features, not performance.
		
		LogInfo("  returns E_NOINTERFACE as error for ID3D11Device1. \n");
		*ppvObject = NULL;
		return E_NOINTERFACE;

		// Maybe later:
		//ID3D11Device1 *origDevice1;
		//hr = mOrigDevice->QueryInterface(riid, (void**)(&origDevice1));

		//ID3D11DeviceContext1 *origContext1;
		//origDevice1->GetImmediateContext1(&origContext1);
		//HackerContext1 *hackerContextWrap1 = new HackerContext1(origDevice1, origContext1);

		//LogInfo("  created HackerContext1(%s@%p) wrapper of %p \n", typeid(*hackerContextWrap1).name(), hackerContextWrap1, origContext1);

		//HackerDevice1 *hackerDeviceWrap1 = new HackerDevice1(origDevice1, origContext1);
		//hackerDeviceWrap1->SetHackerContext1(hackerContextWrap1);

		//LogInfo("  created HackerDevice1(%s@%p) wrapper of %p \n", typeid(*hackerDeviceWrap1).name(), hackerDeviceWrap1, origDevice1);

		//// ToDo: Handle memory allocation exceptions

		//if (ppvObject)
		//	*ppvObject = hackerDeviceWrap1;
	}

	LogDebug("  returns result = %x for %p \n", hr, *ppvObject);
	return hr;
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
		//if (G->hunting)
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
		//if (G->hunting)
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

// Only used in CreateXXXShader (Vertex, Pixel, Compute, Geometry, Hull, Domain)

// This whole function is in need of major refactoring. At a quick glance I can
// see several code paths that will leak objects, and in general it is far,
// too long and complex - the human brain has between 5 an 9 (typically 7)
// general purpose registers, but this function requires far more than that to
// understand. I've added comments to a few objects that can leak, but there's
// little value in fixing one or two problems without tackling the whole thing,
// but I need to understand it better before I'm willing to start refactoring
// it. -DarkStarSword
//
// Chapter 6 of the Linux coding style guidelines is worth a read:
//   https://www.kernel.org/doc/Documentation/CodingStyle

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
		if (G->EXPORT_BINARY) {
			wsprintf(val, L"%ls\\%08lx%08lx-%ls.bin", G->SHADER_CACHE_PATH, (UINT32)(hash >> 32), (UINT32)(hash), shaderType);
			HANDLE f = CreateFile(val, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
			bool exists = false;
			if (f != INVALID_HANDLE_VALUE) {
				int cnt = 0;
				while (f != INVALID_HANDLE_VALUE) {
					// Check if same file.
					DWORD dataSize = GetFileSize(f, 0);
					char *buf = new char[dataSize];
					DWORD readSize;
					if (!ReadFile(f, buf, dataSize, &readSize, 0) || dataSize != readSize)
						LogInfo("  Error reading file.\n");
					CloseHandle(f);
					if (dataSize == BytecodeLength && !memcmp(pShaderBytecode, buf, dataSize))
						exists = true;
					delete [] buf;
					if (exists)
						break;
					wsprintf(val, L"%ls\\%08lx%08lx-%ls_%d.bin", G->SHADER_CACHE_PATH, (UINT32)(hash >> 32), (UINT32)(hash), shaderType, ++cnt);
					f = CreateFile(val, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
				}
			}
			if (!exists) {
				FILE *fw;
				_wfopen_s(&fw, val, L"wb");
				if (fw) {
					LogInfoW(L"    storing original binary shader to %s\n", val);
					fwrite(pShaderBytecode, 1, BytecodeLength, fw);
					fclose(fw);
				} else {
					LogInfoW(L"    error storing original binary shader to %s\n", val);
				}
			}
		}

		// Export every shader seen as an ASM text file.
		if (G->EXPORT_SHADERS)
		{
			CreateAsmTextFile(G->SHADER_CACHE_PATH, hash, shaderType, pShaderBytecode, BytecodeLength);
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
				string shaderModel = GetShaderModel(pShaderBytecode, BytecodeLength);
				if (shaderModel.empty())
				{
					LogInfo("    disassembly of original shader failed.\n");

					delete srcData;
				}
				else
				{
					// Any HLSL compiled shaders are reloading candidates, if moved to ShaderFixes
					foundShaderModel = shaderModel;
					timeStamp = ftWrite;

					// Compile replacement.
					LogInfo("    compiling replacement HLSL code with shader model %s\n", shaderModel.c_str());

					ID3DBlob *pErrorMsgs; // FIXME: This can leak
					ID3DBlob *pCompiledOutput = 0;
					HRESULT ret = D3DCompile(srcData, srcDataSize, "wrapper1349", 0, ((ID3DInclude*)(UINT_PTR)1),
						"main", shaderModel.c_str(), D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &pCompiledOutput, &pErrorMsgs);
					delete srcData; srcData = 0;
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
			else	// No HLSL replacement, how about ASM?
			{
				swprintf_s(val, MAX_PATH, L"%ls\\%016llx-%ls.txt", G->SHADER_PATH, hash, shaderType);
				f = CreateFile(val, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
				if (f != INVALID_HANDLE_VALUE)
				{
					LogInfo("    Replacement ASM shader found. Assembling replacement ASM code. \n");
					
					DWORD srcDataSize = GetFileSize(f, 0);
					vector<char> asmTextBytes(srcDataSize);
					DWORD readSize;
					FILETIME ftWrite;
					if (!ReadFile(f, asmTextBytes.data(), srcDataSize, &readSize, 0)
						|| !GetFileTime(f, NULL, NULL, &ftWrite)
						|| srcDataSize != readSize)
						LogInfo("    Error reading file. \n");
					CloseHandle(f);
					LogInfo("    Asm source code loaded. Size = %d \n", srcDataSize);
					
					// Disassemble old shader to get shader model.
					string shaderModel = GetShaderModel(pShaderBytecode, BytecodeLength);
					if (shaderModel.empty())
					{
						LogInfo("    disassembly of original shader failed.\n");
					}
					else
					{
						// Any ASM shaders are reloading candidates, if moved to ShaderFixes
						foundShaderModel = shaderModel;
						timeStamp = ftWrite;

						vector<byte> byteCode(BytecodeLength);
						memcpy(byteCode.data(), pShaderBytecode, BytecodeLength);
						byteCode = assembler(*reinterpret_cast<vector<byte>*>(&asmTextBytes), byteCode);

						// Write reassembly binary output for comparison. ToDo: remove after we have
						// resolved the disassembler precision issue and validated everything works.
						FILE *fw;
						swprintf_s(val, MAX_PATH, L"%ls\\%016llx-%ls_reasm.bin", G->SHADER_PATH, hash, shaderType);
						_wfopen_s(&fw, val, L"wb");
						if (fw)
						{
							LogInfoW(L"    storing reassembled binary to %s\n", val);
							fwrite(byteCode.data(), 1, byteCode.size(), fw);
							fclose(fw);
						}
						else 
						{
							LogInfoW(L"    error storing reassembled binary to %s\n", val);
						}

						// With that cso object of reassembly, let's re-dissassemble it and write output.
						// ToDo: remove this after it's all working.  This is just testing, validation.
						string asmText = BinaryToAsmText(byteCode.data(), byteCode.size());
						if (asmText.empty())
						{
							LogInfo("  re-disassembly failed. \n");
						}
						else
						{
							// Write reassembly output for comparison.
							swprintf_s(val, MAX_PATH, L"%ls\\%016llx-%ls_reasm.txt", G->SHADER_PATH, hash, shaderType);
							HRESULT hr = CreateTextFile(val, asmText, true);
							if (FAILED(hr)) {
								LogInfoW(L"    error storing reassembly to %s \n", val);
							}
							else {
								LogInfoW(L"    storing reassembly to %s \n", val);
							}

							// Since the re-assembly worked, let's make it the active shader code.
							pCodeSize = byteCode.size();
							pCode = new char[pCodeSize];
							memcpy(pCode, byteCode.data(), pCodeSize);
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
			ID3DBlob *disassembly = 0; // FIXME: This can leak
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

			// Stop-gap measure to avoid decompiling geometry, hull
			// and domain shaders as they will almost certainly
			// cause a crash in the decompiler. Remove this once fixed.
			if (!wcscmp(shaderType, L"gs") || !wcscmp(shaderType, L"hs") || !wcscmp(shaderType, L"ds")) {
				LogInfoW(L"    Skipping decompilation of unsupported shader type %ls.\n", shaderType);
				return 0;
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

					ID3DBlob *pErrorMsgs; // FIXME: This can leak
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
						string asmText = BinaryToAsmText(pCompiledOutput->GetBufferPointer(), pCompiledOutput->GetBufferSize());
						if (asmText.empty())
						{
							LogInfo("    disassembly of recompiled shader failed.\n");
						}
						else
						{
							fprintf_s(fw, "\n\n/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Recompiled ASM ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
							fwrite(asmText.data(), 1, asmText.size(), fw);
							fprintf_s(fw, "\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/\n");
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
		string asmText = BinaryToAsmText(pShaderBytecode, BytecodeLength);
		if (asmText.empty())
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
			p.decompiled = asmText.c_str();
			p.decompiledSize = asmText.size();
			p.recompileVs = G->FIX_Recompile_VS;
			p.fixSvPosition = false;
			p.ZeroOutput = true;
			const string decompiledCode = DecompileBinaryHLSL(p, patched, shaderModel, errorOccurred);
			if (!decompiledCode.size())
			{
				LogInfo("    error while decompiling.\n");

				return 0;
			}
			if (!errorOccurred)
			{
				// Compile replacement.
				LogInfo("    compiling zero HLSL code with shader model %s, size = %Iu\n", shaderModel.c_str(), decompiledCode.size());

				ID3DBlob *pErrorMsgs; // FIXME: This can leak
				ID3DBlob *pCompiledOutput = 0;
				HRESULT ret = D3DCompile(decompiledCode.c_str(), decompiledCode.size(), "wrapper1349", 0, ((ID3DInclude*)(UINT_PTR)1),
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

	// if (G->hunting && ... - hunting may now be soft-enabled via key binding
	if (G->marking_mode == MARKING_MODE_ORIGINAL || G->config_reloadable || G->show_original_enabled)
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
void HackerDevice::KeepOriginalShader(UINT64 hash, wchar_t *shaderType, ID3D11DeviceChild *pShader,
	const void *pShaderBytecode, SIZE_T BytecodeLength, ID3D11ClassLinkage *pClassLinkage)
{
	if (!NeedOriginalShader(hash))
		return;

	LogInfoW(L"    keeping original shader for filtering: %016llx-%ls\n", hash, shaderType);

	if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
		if (!wcsncmp(shaderType, L"vs", 2)) {
			ID3D11VertexShader *originalShader;
			mOrigDevice->CreateVertexShader(pShaderBytecode, BytecodeLength, pClassLinkage, &originalShader);
			G->mOriginalVertexShaders[(ID3D11VertexShader*)pShader] = originalShader;
		} else if (!wcsncmp(shaderType, L"ps", 2)) {
			ID3D11PixelShader *originalShader;
			mOrigDevice->CreatePixelShader(pShaderBytecode, BytecodeLength, pClassLinkage, &originalShader);
			G->mOriginalPixelShaders[(ID3D11PixelShader*)pShader] = originalShader;
		} else if (!wcsncmp(shaderType, L"cs", 2)) {
			ID3D11ComputeShader *originalShader;
			mOrigDevice->CreateComputeShader(pShaderBytecode, BytecodeLength, pClassLinkage, &originalShader);
			G->mOriginalComputeShaders[(ID3D11ComputeShader*)pShader] = originalShader;
		} else if (!wcsncmp(shaderType, L"gs", 2)) {
			ID3D11GeometryShader *originalShader;
			mOrigDevice->CreateGeometryShader(pShaderBytecode, BytecodeLength, pClassLinkage, &originalShader);
			G->mOriginalGeometryShaders[(ID3D11GeometryShader*)pShader] = originalShader;
		} else if (!wcsncmp(shaderType, L"hs", 2)) {
			ID3D11HullShader *originalShader;
			mOrigDevice->CreateHullShader(pShaderBytecode, BytecodeLength, pClassLinkage, &originalShader);
			G->mOriginalHullShaders[(ID3D11HullShader*)pShader] = originalShader;
		} else if (!wcsncmp(shaderType, L"ds", 2)) {
			ID3D11DomainShader *originalShader;
			mOrigDevice->CreateDomainShader(pShaderBytecode, BytecodeLength, pClassLinkage, &originalShader);
			G->mOriginalDomainShaders[(ID3D11DomainShader*)pShader] = originalShader;
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
	LogInfo("HackerDevice::SetPrivateDataInterface(%s@%p) called with IID: %s \n", typeid(*this).name(), this, NameFromIID(guid).c_str());

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

	// Create hash code.  Wrapped in try/catch because it can crash in Dirt Rally,
	// because of noncontiguous or non-mapped memory for the texture.  Not sure this
	// is the best strategy.
	UINT64 hash = 0;
	if (pInitialData && pInitialData->pSysMem && pDesc)
		try
		{
			hash = fnv_64_buf(pInitialData->pSysMem, pDesc->Width / 2 * pDesc->Height * pDesc->ArraySize);
		}
		catch (...)
		{
			// Fatal error, but catch it and return null for hash.
			LogInfo("   ******* Exception caught while calculating Texture2D hash ****** \n");
			hash = 0;
		}
	
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

	// Actual creation:
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


// C++ function template of common code shared by all CreateXXXShader functions:
template <class ID3D11Shader,
	 HRESULT (__stdcall ID3D11Device::*OrigCreateShader)(THIS_
			 const void *pShaderBytecode,
			 SIZE_T BytecodeLength,
			 ID3D11ClassLinkage *pClassLinkage,
			 ID3D11Shader **ppShader)
	 >
STDMETHODIMP HackerDevice::CreateShader(THIS_
	/* [annotation] */
	__in  const void *pShaderBytecode,
	/* [annotation] */
	__in  SIZE_T BytecodeLength,
	/* [annotation] */
	__in_opt  ID3D11ClassLinkage *pClassLinkage,
	/* [annotation] */
	__out_opt  ID3D11Shader **ppShader,
	wchar_t *shaderType,
	std::unordered_map<ID3D11Shader *, UINT64> *shaders,
	std::unordered_map<UINT64, ID3D11Shader *> *preloadedShaders,
	std::unordered_map<ID3D11Shader *, ID3D11Shader *> *originalShaders,
	std::unordered_map<ID3D11Shader *, ID3D11Shader *> *zeroShaders
	)
{
	HRESULT hr = -1;
	UINT64 hash;
	string shaderModel;
	SIZE_T replaceShaderSize;
	FILETIME ftWrite;
	ID3D11Shader *zeroShader = 0;

	if (pShaderBytecode && ppShader)
	{
		// Calculate hash
		hash = fnv_64_buf(pShaderBytecode, BytecodeLength);
		LogInfo("  bytecode hash = %016I64x\n", hash);

		// Preloaded shader? (Can't use preloaded shaders with class linkage).
		if (preloadedShaders && !pClassLinkage)
		{
			std::unordered_map<UINT64, ID3D11Shader *>::iterator i = preloadedShaders->find(hash);
			if (i != preloadedShaders->end())
			{
				*ppShader = i->second;
				ULONG cnt = (*ppShader)->AddRef();
				hr = S_OK;
				LogInfo("    shader assigned by preloaded version. ref counter = %d\n", cnt);

				if (G->marking_mode == MARKING_MODE_ZERO)
				{
					char *replaceShader = ReplaceShader(hash, shaderType, pShaderBytecode, BytecodeLength, replaceShaderSize,
						shaderModel, ftWrite, (void **)&zeroShader);
					delete replaceShader;
				}
				KeepOriginalShader(hash, shaderType, *ppShader, pShaderBytecode, BytecodeLength, pClassLinkage);
			}
		}
	}
	if (hr != S_OK && ppShader && pShaderBytecode)
	{
		char *replaceShader = ReplaceShader(hash, shaderType, pShaderBytecode, BytecodeLength, replaceShaderSize,
			shaderModel, ftWrite, (void **)&zeroShader);
		if (replaceShader)
		{
			// Create the new shader.
			LogDebug("    HackerDevice::Create%lsShader.  Device: %p\n", shaderType, this);

			hr = (mOrigDevice->*OrigCreateShader)(replaceShader, replaceShaderSize, pClassLinkage, ppShader);
			if (SUCCEEDED(hr))
			{
				LogInfo("    shader successfully replaced.\n");

				//if (G->hunting)
				{
					// Hunting mode:  keep byteCode around for possible replacement or marking
					ID3DBlob* blob;
					D3DCreateBlob(replaceShaderSize, &blob);
					memcpy(blob->GetBufferPointer(), replaceShader, replaceShaderSize);
					if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
						RegisterForReload(*ppShader, hash, shaderType, shaderModel, pClassLinkage, blob, ftWrite);
					if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
				}
				KeepOriginalShader(hash, shaderType, *ppShader, pShaderBytecode, BytecodeLength, pClassLinkage);
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
		hr = (mOrigDevice->*OrigCreateShader)(pShaderBytecode, BytecodeLength, pClassLinkage, ppShader);

		// When in hunting mode, make a copy of the original binary, regardless.  This can be replaced, but we'll at least
		// have a copy for every shader seen.
		//if (G->hunting)
		{
			if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
				ID3DBlob* blob;
				D3DCreateBlob(BytecodeLength, &blob);
				memcpy(blob->GetBufferPointer(), pShaderBytecode, blob->GetBufferSize());
				RegisterForReload(*ppShader, hash, shaderType, "bin", pClassLinkage, blob, ftWrite);

				// Also add the original shader to the original shaders
				// map so that if it is later replaced marking_mode =
				// original and depth buffer filtering will work:
				if (originalShaders->count(*ppShader) == 0)
					(*originalShaders)[*ppShader] = *ppShader;
			if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
		}
	}
	if (hr == S_OK && ppShader && pShaderBytecode)
	{
		if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
			(*shaders)[*ppShader] = hash;
			LogDebugW(L"    %ls: handle = %p, hash = %016I64x\n", shaderType, *ppShader, hash);

			if ((G->marking_mode == MARKING_MODE_ZERO) && zeroShader && zeroShaders)
			{
				(*zeroShaders)[*ppShader] = zeroShader;
			}

			CompiledShaderMap::iterator i = G->mCompiledShaderMap.find(hash);
			if (i != G->mCompiledShaderMap.end())
			{
				LogInfo("  shader was compiled from source code %s\n", i->second.c_str());
			}
		if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
	}

	LogInfo("  returns result = %x, handle = %p\n", hr, *ppShader);

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

	return CreateShader<ID3D11VertexShader, &ID3D11Device::CreateVertexShader>
			(pShaderBytecode, BytecodeLength, pClassLinkage, ppVertexShader,
			 L"vs", &G->mVertexShaders, &G->mPreloadedVertexShaders, &G->mOriginalVertexShaders, &G->mZeroVertexShaders);
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

	return CreateShader<ID3D11GeometryShader, &ID3D11Device::CreateGeometryShader>
			(pShaderBytecode, BytecodeLength, pClassLinkage, ppGeometryShader,
			 L"gs",
			 &G->mGeometryShaders,
			 NULL /* TODO: &G->mPreloadedGeometryShaders */,
			 &G->mOriginalGeometryShaders,
			 NULL /* TODO: &G->mZeroGeometryShaders */);
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

	return CreateShader<ID3D11PixelShader, &ID3D11Device::CreatePixelShader>
			(pShaderBytecode, BytecodeLength, pClassLinkage, ppPixelShader,
			 L"ps", &G->mPixelShaders, &G->mPreloadedPixelShaders, &G->mOriginalPixelShaders, &G->mZeroPixelShaders);
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

	return CreateShader<ID3D11HullShader, &ID3D11Device::CreateHullShader>
			(pShaderBytecode, BytecodeLength, pClassLinkage, ppHullShader,
			 L"hs",
			 &G->mHullShaders,
			 NULL /* TODO: &G->mPreloadedHullShaders */,
			 &G->mOriginalHullShaders,
			 NULL /* TODO: &G->mZeroHullShaders */);
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

	return CreateShader<ID3D11DomainShader, &ID3D11Device::CreateDomainShader>
			(pShaderBytecode, BytecodeLength, pClassLinkage, ppDomainShader,
			 L"ds",
			 &G->mDomainShaders,
			 NULL /* TODO: &G->mPreloadedDomainShaders */,
			 &G->mOriginalDomainShaders,
			 NULL /* TODO: &G->mZeroDomainShaders */);
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

	return CreateShader<ID3D11ComputeShader, &ID3D11Device::CreateComputeShader>
			(pShaderBytecode, BytecodeLength, pClassLinkage, ppComputeShader,
			 L"cs",
			 &G->mComputeShaders,
			 NULL /* TODO: &G->mPreloadedComputeShaders */,
			 &G->mOriginalComputeShaders,
			 NULL /* TODO (if this even makes sense?): &G->mZeroComputeShaders */);
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
		const_cast<D3D11_RASTERIZER_DESC*>(pRasterizerDesc)->ScissorEnable = FALSE;
	}
	hr = mOrigDevice->CreateRasterizerState(pRasterizerDesc, ppRasterizerState);

	LogDebug("  returns result = %x\n", hr);
	return hr;
}

// This method creates a Context, and we want to return a wrapped/hacker
// version as the result. The method signature requires an 
// ID3D11DeviceContext, but we return our HackerContext.

// A deferred context is for multithreading part of the drawing. 

STDMETHODIMP HackerDevice::CreateDeferredContext(THIS_
	UINT ContextFlags,
	/* [annotation] */
	__out_opt  ID3D11DeviceContext **ppDeferredContext)
{
	LogInfo("HackerDevice::CreateDeferredContext(%s@%p) called with flags = %#x, ptr:%p \n", 
		typeid(*this).name(), this, ContextFlags, ppDeferredContext);

	HRESULT hr = mOrigDevice->CreateDeferredContext(ContextFlags, ppDeferredContext);
	if (FAILED(hr))
	{
		LogDebug("  failed result = %x for %p \n", hr, ppDeferredContext);
		return hr;
	}

	if (ppDeferredContext)
	{
		ID3D11DeviceContext *origContext = static_cast<ID3D11DeviceContext*>(*ppDeferredContext);
		HackerContext *hackerContext = new HackerContext(mOrigDevice, origContext);
		hackerContext->SetHackerDevice(this);
		*ppDeferredContext = hackerContext;
		LogInfo("  created HackerContext(%s@%p) wrapper of %p \n", typeid(*hackerContext).name(), hackerContext, origContext);
	}

	LogDebug("  returns result = %x for %p \n", hr, *ppDeferredContext);

	return hr;
}

// A variant where we want to return a HackerContext instead of the
// real one.  Creating a new HackerContext is not correct here, because we 
// need to provide the one created originally with the device.

// This is a main way to get the context when you only have the device.
// There is only one immediate context per device, so if they are requesting
// it, we need to return them the HackerContext.
// 
// It is apparently possible for poorly written games to call this function
// with null as the ppImmediateContext. This not an optional parameter, and
// that call makes no sense, but apparently happens if they pass null to
// CreateDeviceAndSwapChain for ImmediateContext.  A bug in an older SDK.
// WatchDogs seems to do this. 
// 
// Also worth noting here is that by not calling through to GetImmediateContext
// we did not properly account for references.
// "The GetImmediateContext method increments the reference count of the immediate context by one. "
//
// Fairly common to see this called all the time, so switching to LogDebug for
// this as a way to trim down normal log.

STDMETHODIMP_(void) HackerDevice::GetImmediateContext(THIS_
	/* [annotation] */
	__out  ID3D11DeviceContext **ppImmediateContext)
{
	LogDebug("HackerDevice::GetImmediateContext(%s@%p) called with:%p \n", 
		typeid(*this).name(), this, ppImmediateContext);

	if (ppImmediateContext == nullptr)
	{
		LogInfo("  *** no return possible, nullptr input. \n");
		return;
	}

	// We still need to call the original function to make sure the reference counts are correct:
	mOrigDevice->GetImmediateContext(ppImmediateContext);

	// It should no longer be possible, but we can conceivably arrive here with
	// no mHackerContext created.  Based on the original code, it's not a bad
	// idea to create that HackerContext here.
	if (mHackerContext == nullptr)
	{
		LogInfo("*** HackerContext missing at HackerDevice::GetImmediateContext \n");

		mHackerContext = new HackerContext(mOrigDevice, *ppImmediateContext);
		LogInfo("  HackerContext %p created to wrap %p \n", mHackerContext, *ppImmediateContext);
	}

	*ppImmediateContext = mHackerContext;
	LogDebug("  returns handle = %p  \n", *ppImmediateContext);
}

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

// -----------------------------------------------------------------------------
// HackerDevice1 methods.  All other subclassed methods will use HackerDevice methods.

HackerDevice1::HackerDevice1(ID3D11Device1 *pDevice1, ID3D11DeviceContext1 *pContext)
	: HackerDevice(pDevice1, pContext)
{
	mOrigDevice1 = pDevice1;
	mOrigContext1 = pContext;
}

// Save reference to corresponding HackerContext during CreateDevice, needed for GetImmediateContext.

void HackerDevice1::SetHackerContext1(HackerContext1 *pHackerContext)
{
	mHackerContext1 = pHackerContext;
}


// Follow the lead for GetImmediateContext and return the wrapped version.

STDMETHODIMP_(void) HackerDevice1::GetImmediateContext1(
	/* [annotation] */
	_Out_  ID3D11DeviceContext1 **ppImmediateContext)
{
	LogInfo("HackerDevice1::GetImmediateContext1(%s@%p) called with:%p \n",
		typeid(*this).name(), this, ppImmediateContext);

	if (ppImmediateContext == nullptr)
	{
		LogInfo("  *** no return possible, nullptr input. \n");
		return;
	}

	// We still need to call the original function to make sure the reference counts are correct:
	mOrigDevice1->GetImmediateContext1(ppImmediateContext);

	// It should no longer be possible, but we can conceivably arrive here with
	// no mHackerContext created.  Based on the original code, it's not a bad
	// idea to create that HackerContext here.
	if (mHackerContext1 == nullptr)
	{
		LogInfo("*** HackerContext1 missing at HackerDevice1::GetImmediateContext1 \n");

		mHackerContext1 = new HackerContext1(mOrigDevice1, *ppImmediateContext);
		LogInfo("  mHackerContext1 %p created to wrap %p \n", mHackerContext1, *ppImmediateContext);
	}

	*ppImmediateContext = reinterpret_cast<ID3D11DeviceContext1*>(mHackerContext1);
	LogInfo("  returns handle = %p  \n", *ppImmediateContext);
}


// Pretty sure we don't need to wrap DeferredContexts at all, but we'll 
// still log to see when it's used.

STDMETHODIMP HackerDevice1::CreateDeferredContext1(
	UINT ContextFlags,
	/* [annotation] */
	_Out_opt_  ID3D11DeviceContext1 **ppDeferredContext)
{
	LogInfo("HackerDevice1::CreateDeferredContext1(%s@%p) called with flags = %x \n", typeid(*this).name(), this, ContextFlags);
	HRESULT hr = mOrigDevice1->CreateDeferredContext1(ContextFlags, ppDeferredContext);
	LogDebug("  returns result = %x\n", hr);
	return hr;
}

STDMETHODIMP HackerDevice1::CreateBlendState1(
	/* [annotation] */
	_In_  const D3D11_BLEND_DESC1 *pBlendStateDesc,
	/* [annotation] */
	_Out_opt_  ID3D11BlendState1 **ppBlendState)
{
	return mOrigDevice1->CreateBlendState1(pBlendStateDesc, ppBlendState);
}

STDMETHODIMP HackerDevice1::CreateRasterizerState1(
	/* [annotation] */
	_In_  const D3D11_RASTERIZER_DESC1 *pRasterizerDesc,
	/* [annotation] */
	_Out_opt_  ID3D11RasterizerState1 **ppRasterizerState)
{
	return mOrigDevice1->CreateRasterizerState1(pRasterizerDesc, ppRasterizerState);
}

STDMETHODIMP HackerDevice1::CreateDeviceContextState(
	UINT Flags,
	/* [annotation] */
	_In_reads_(FeatureLevels)  const D3D_FEATURE_LEVEL *pFeatureLevels,
	UINT FeatureLevels,
	UINT SDKVersion,
	REFIID EmulatedInterface,
	/* [annotation] */
	_Out_opt_  D3D_FEATURE_LEVEL *pChosenFeatureLevel,
	/* [annotation] */
	_Out_opt_  ID3DDeviceContextState **ppContextState)
{
	return mOrigDevice1->CreateDeviceContextState(Flags, pFeatureLevels, FeatureLevels, SDKVersion, EmulatedInterface, pChosenFeatureLevel, ppContextState);
}

STDMETHODIMP HackerDevice1::OpenSharedResource1(
	/* [annotation] */
	_In_  HANDLE hResource,
	/* [annotation] */
	_In_  REFIID returnedInterface,
	/* [annotation] */
	_Out_  void **ppResource)
{
	return mOrigDevice1->OpenSharedResource1(hResource, returnedInterface, ppResource);
}

STDMETHODIMP HackerDevice1::OpenSharedResourceByName(
	/* [annotation] */
	_In_  LPCWSTR lpName,
	/* [annotation] */
	_In_  DWORD dwDesiredAccess,
	/* [annotation] */
	_In_  REFIID returnedInterface,
	/* [annotation] */
	_Out_  void **ppResource)
{
	return mOrigDevice1->OpenSharedResourceByName(lpName, dwDesiredAccess, returnedInterface, ppResource);
}

