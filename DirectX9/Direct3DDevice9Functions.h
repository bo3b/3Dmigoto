#include <codecvt>
#include "../HLSLDecompiler/DecompileHLSL.h"
#include <D3Dcompiler.h>
#include "shader.h"
#include "ShaderRegex.h"
#include "HookedDeviceDX9.h"
#include "d3d9Wrapper.h"
#include <DirectXMath.h>

#define D3D_COMPILE_STANDARD_FILE_INCLUDE ((ID3DInclude*)(UINT_PTR)1)
inline void D3D9Wrapper::IDirect3DDevice9::Delete()
{
	if (G->enable_hooks & EnableHooksDX9::DEVICE)
		remove_hooked_device((::IDirect3DDevice9*)m_pRealUnk);

	if (mStereoHandle)
	{
		int result = NvAPI_Stereo_DestroyHandle(mStereoHandle);
		mStereoHandle = 0;
		LogInfo("  releasing NVAPI stereo handle, result = %d\n", result);
	}
	UnbindResources();
	ReleaseDeviceResources();

	if (frame_analysis_log)
		fclose(frame_analysis_log);

	if (m_pRealUnk) m_List.DeleteMember(m_pRealUnk);
	m_pUnk = 0;

	m_pRealUnk = 0;
	delete this;
}
HRESULT D3D9Wrapper::IDirect3DDevice9::NVAPIStretchRect(::IDirect3DResource9 *src, ::IDirect3DResource9 *dst, RECT *srcRect, RECT *dstRect)
{
	if (find(nvapi_registered_resources.begin(), nvapi_registered_resources.end(), src) == nvapi_registered_resources.end()) {
		if (!(NVAPI_OK == NvAPI_D3D9_RegisterResource(src)))
			return E_FAIL;
		else
			nvapi_registered_resources.emplace_back(src);
	}
	if (find(nvapi_registered_resources.begin(), nvapi_registered_resources.end(), dst) == nvapi_registered_resources.end()) {
		if (!(NVAPI_OK == NvAPI_D3D9_RegisterResource(dst)))
			return E_FAIL;
		else
			nvapi_registered_resources.emplace_back(dst);
	}
	if (NVAPI_OK == NvAPI_D3D9_StretchRectEx(GetD3D9Device(), src, srcRect, dst, dstRect, ::D3DTEXF_POINT)) {
		return S_OK;
	}
	else {
		return E_FAIL;
	}
}
inline bool D3D9Wrapper::IDirect3DDevice9::get_sli_enabled()
{
	NV_GET_CURRENT_SLI_STATE sli_state;
	sli_state.version = NV_GET_CURRENT_SLI_STATE_VER;
	NvAPI_Status status;

	status = Profiling::NvAPI_D3D_GetCurrentSLIState(this->GetD3D9Device(), &sli_state);
	if (status != NVAPI_OK) {
		LogInfo("Unable to retrieve SLI state from nvapi\n");
		sli = false;
		return false;
	}

	sli = sli_state.maxNumAFRGroups > 1;
	return sli;
}
bool D3D9Wrapper::IDirect3DDevice9::sli_enabled()
{
	return sli;
}
Overlay* D3D9Wrapper::IDirect3DDevice9::getOverlay()
{
	return mOverlay;
}

HRESULT D3D9Wrapper::IDirect3DDevice9::createOverlay()
{
	try {
		// Create Overlay class that will be responsible for drawing any text
		// info over the game.
		mOverlay = new Overlay(this);
	}
	catch (...) {
		LogInfo("  *** Failed to create Overlay. Exception caught.\n");
		mOverlay = NULL;
		return E_FAIL;
	}

	return S_OK;
}

inline HRESULT D3D9Wrapper::IDirect3DDevice9::ReleaseStereoTexture()
{
	// Release stereo parameter texture.
	LogInfo("  release stereo parameter texture.\n");
	if (this->mStereoTexture) {
		ULONG ret = this->mStereoTexture->Release();
		LogInfo("    stereo texture release, handle = %p\n", this->mStereoTexture);
		this->mStereoTexture = NULL;
		--migotoResourceCount;
	}
	return S_OK;
}
inline HRESULT D3D9Wrapper::IDirect3DDevice9::CreateStereoTexture()
{
	HRESULT hr;
	// Create stereo parameter texture.
	LogInfo("  creating stereo parameter texture.\n");
	hr = GetD3D9Device()->CreateTexture(nv::stereo::ParamTextureManagerD3D9::Parms::StereoTexWidth, nv::stereo::ParamTextureManagerD3D9::Parms::StereoTexHeight, 1, 0, nv::stereo::ParamTextureManagerD3D9::Parms::StereoTexFormat, ::D3DPOOL_DEFAULT, &this->mStereoTexture, NULL);

	if (FAILED(hr))
	{
		LogInfo("    call failed with result = %x.\n", hr);
		return hr;
	}
	LogInfo("    stereo texture created, handle = %p\n", this->mStereoTexture);
	++migotoResourceCount;
	return S_OK;
}
inline HRESULT D3D9Wrapper::IDirect3DDevice9::CreateStereoHandle() {
	NvAPI_Status nvret;
	// Todo: This call will fail if stereo is disabled. Proper notification?
	nvret = NvAPI_Stereo_CreateHandleFromIUnknown(m_pUnk, &this->mStereoHandle);
	if (nvret != NVAPI_OK)
	{
		this->mStereoHandle = 0;
		LogInfo("HackerDevice::CreateStereoParamResources NvAPI_Stereo_CreateHandleFromIUnknown failed: %d\n", nvret);
		return nvret;
	}
	this->mParamTextureManager.mStereoHandle = this->mStereoHandle;
	LogInfo("  created NVAPI stereo handle. Handle = %p\n", this->mStereoHandle);

	return InitStereoHandle();
}
inline HRESULT D3D9Wrapper::IDirect3DDevice9::InitStereoHandle() {
	NvAPI_Status nvret = NVAPI_OK;
	retreivedInitial3DSettings = false;
	if (G->stereoblit_control_set_once && G->gForceStereo != 2) {
		nvret = NvAPI_Stereo_ReverseStereoBlitControl(this->mStereoHandle, true);
		if (nvret != NVAPI_OK) {
			LogInfo("  Failed to enable reverse stereo blit\n");
		}
		else {
			LogInfo("  Enabled reverse stereo blit\n");
		}
	}
	return nvret;
}
inline void D3D9Wrapper::IDirect3DDevice9::CreatePinkHuntingResources()
{
	// Only create special pink mode PixelShader when requested.
	if (G->hunting && (G->marking_mode == MarkingMode::PINK || G->config_reloadable))
	{
		char* hlsl =
			"float4 pshader() : COLOR0"
			"{"
			"	return float4(1,0,1,1);"
			"}";

		ID3DBlob* blob = NULL;
		HRESULT hr = D3DCompile(hlsl, strlen(hlsl), "JustPink", NULL, NULL, "pshader", "ps_3_0", 0, 0, &blob, NULL);
		LogInfo("  Created pink mode pixel shader: %d\n", hr);
		if (SUCCEEDED(hr))
		{
			hr = GetD3D9Device()->CreatePixelShader((DWORD*)blob->GetBufferPointer(),  &G->mPinkingShader);
			if (FAILED(hr))
				LogInfo("  Failed to create pinking pixel shader: %d\n", hr);
			else
				++migotoResourceCount;
			blob->Release();
		}
	}
}
inline void D3D9Wrapper::IDirect3DDevice9::ReleasePinkHuntingResources()
{
	if (G->mPinkingShader)
	{
		LogInfo("  Releasing pinking shader\n");
		G->mPinkingShader->Release();
		G->mPinkingShader = NULL;
		--migotoResourceCount;
	}
}
inline HRESULT D3D9Wrapper::IDirect3DDevice9::SetGlobalNVSurfaceCreationMode()
{
	HRESULT hr;
	// Override custom settings.
	if (mStereoHandle && G->gSurfaceCreateMode >= 0)
	{
		NvAPIOverride();
		LogInfo("  setting custom surface creation mode.\n");

		hr = NvAPI_Stereo_SetSurfaceCreationMode(mStereoHandle, (NVAPI_STEREO_SURFACECREATEMODE)G->gSurfaceCreateMode);
		if (hr != NVAPI_OK)
		{
			LogInfo("    custom surface creation call failed: %d.\n", hr);
			return hr;
		}
	}
	return S_OK;
}

void ExportOrigBinary(UINT64 hash, const wchar_t *pShaderType, const void *pShaderBytecode, SIZE_T pBytecodeLength)
{
	wchar_t path[MAX_PATH];
	HANDLE f;
	bool exists = false;

	swprintf_s(path, MAX_PATH, L"%ls\\%016llx-%ls.bin", G->SHADER_CACHE_PATH, hash, pShaderType);
	f = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (f != INVALID_HANDLE_VALUE)
	{
		int cnt = 0;
		while (f != INVALID_HANDLE_VALUE)
		{
			// Check if same file.
			DWORD dataSize = GetFileSize(f, 0);
			char *buf = new char[dataSize];
			DWORD readSize;
			if (!ReadFile(f, buf, dataSize, &readSize, 0) || dataSize != readSize)
				LogInfo("  Error reading file.\n");
			CloseHandle(f);
			if (dataSize == pBytecodeLength && !memcmp(pShaderBytecode, buf, dataSize))
				exists = true;
			delete[] buf;
			if (exists)
				break;
			swprintf_s(path, MAX_PATH, L"%ls\\%016llx-%ls_%d.bin", G->SHADER_CACHE_PATH, hash, pShaderType, ++cnt);
			f = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
		}
	}
	if (!exists)
	{
		FILE *fw;
		wfopen_ensuring_access(&fw, path, L"wb");
		if (fw)
		{
			LogInfoW(L"    storing original binary shader to %s\n", path);
			fwrite(pShaderBytecode, 1, pBytecodeLength, fw);
			fclose(fw);
		}
		else
		{
			LogInfoW(L"    error storing original binary shader to %s\n", path);
		}
	}
}
static bool GetFileLastWriteTime(wchar_t *path, FILETIME *ftWrite)
{
	HANDLE f;

	f = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (f == INVALID_HANDLE_VALUE)
		return false;

	GetFileTime(f, NULL, NULL, ftWrite);
	CloseHandle(f);
	return true;
}

static bool CheckCacheTimestamp(HANDLE binHandle, wchar_t *binPath, FILETIME &pTimeStamp)
{
	FILETIME txtTime, binTime;
	wchar_t txtPath[MAX_PATH], *end = NULL;
	wcscpy_s(txtPath, MAX_PATH, binPath);
	end = wcsstr(txtPath, L".bin");
	wcscpy_s(end, sizeof(L".bin"), L".txt");
	if (GetFileLastWriteTime(txtPath, &txtTime) && GetFileTime(binHandle, NULL, NULL, &binTime)) {
		// We need to compare the timestamp on the .bin and .txt files.
		// This needs to be an exact match to ensure that the .bin file
		// corresponds to this .txt file (and we need to explicitly set
		// this timestamp when creating the .bin file). Just checking
		// for newer modification time is not enough, since the .txt
		// files in the zip files that fixes are distributed in contain
		// a timestamp that may be older than .bin files generated on
		// an end-users system.
		if (CompareFileTime(&binTime, &txtTime))
			return false;

		// It no longer matters which timestamp we save for later
		// comparison, since they need to match, but we save the .txt
		// file's timestamp since that is the one we are comparing
		// against later.
		pTimeStamp = txtTime;
		return true;
	}
	// If we couldn't get the timestamps it probably means the
	// corresponding .txt file no longer exists. This is actually a bit of
	// an odd (but not impossible) situation to be in - if a user used
	// uninstall.bat when updating a fix they should have removed any stale
	// .bin files as well, and if they didn't use uninstall.bat then they
	// should only be adding new files... so how did a shader that used to
	// be present disappear but leave the cache next to it?
	//
	// A shaderhacker might hit this if they removed the .txt file but not
	// the .bin file, but we could consider that to be user error, so it's
	// not clear any policy here would be correct. Alternatively, a fix
	// might have been shipped with only .bin files - historically we have
	// allowed (but discouraged) that scenario, so for now we issue a
	// warning but allow it.
	LogInfo("    WARNING: Unable to validate timestamp of %S"
		" - no corresponding .txt file?\n", binPath);
	return true;
}

static bool LoadCachedShader(wchar_t *binPath, const wchar_t *pShaderType,
	__out char* &pCode, SIZE_T &pCodeSize, string &pShaderModel, FILETIME &pTimeStamp)
{
	HANDLE f;
	DWORD codeSize, readSize;

	f = CreateFile(binPath, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (f == INVALID_HANDLE_VALUE)
		return false;

	if (!CheckCacheTimestamp(f, binPath, pTimeStamp)) {
		LogInfoW(L"    Discarding stale cached shader: %s\n", binPath);
		goto bail_close_handle;
	}

	LogInfoW(L"    Replacement binary shader found: %s\n", binPath);
	WarnIfConflictingShaderExists(binPath, end_user_conflicting_shader_msg);

	codeSize = GetFileSize(f, 0);
	pCode = new char[codeSize];
	if (!ReadFile(f, pCode, codeSize, &readSize, 0)
		|| codeSize != readSize)
	{
		LogInfo("    Error reading binary file.\n");
		goto err_free_code;
	}

	pCodeSize = codeSize;
	LogInfo("    Bytecode loaded. Size = %Iu\n", pCodeSize);
	CloseHandle(f);

	pShaderModel = "bin";		// tag it as reload candidate, but needing disassemble

	return true;

err_free_code:
	delete[] pCode;
	pCode = NULL;
bail_close_handle:
	CloseHandle(f);
	return false;
}

// Load .bin shaders from the ShaderFixes folder as cached shaders.
// This will load either *_replace.bin, or *.bin variants.

static void LoadBinaryShaders(__in UINT64 hash, const wchar_t *pShaderType,
	__out char* &pCode, SIZE_T &pCodeSize, string &pShaderModel, FILETIME &pTimeStamp)
{
	wchar_t path[MAX_PATH];

	swprintf_s(path, MAX_PATH, L"%ls\\%016llx-%ls_replace.bin", G->SHADER_PATH, hash, pShaderType);
	if (LoadCachedShader(path, pShaderType, pCode, pCodeSize, pShaderModel, pTimeStamp))
		return;

	// If we can't find an HLSL compiled version, look for ASM assembled one.
	swprintf_s(path, MAX_PATH, L"%ls\\%016llx-%ls.bin", G->SHADER_PATH, hash, pShaderType);
	LoadCachedShader(path, pShaderType, pCode, pCodeSize, pShaderModel, pTimeStamp);
}
// Load an HLSL text file as the replacement shader.  Recompile it using D3DCompile.
// If caching is enabled, save a .bin replacement for this new shader.

static void ReplaceHLSLShader(__in UINT64 hash, const wchar_t *pShaderType,
	__in const void *pShaderBytecode, SIZE_T pBytecodeLength, const char *pOverrideShaderModel,
	__out char* &pCode, SIZE_T &pCodeSize, string &pShaderModel, FILETIME &pTimeStamp, wstring &pHeaderLine)
{
	wchar_t path[MAX_PATH];
	HANDLE f;
	string shaderModel;

	swprintf_s(path, MAX_PATH, L"%ls\\%016llx-%ls_replace.txt", G->SHADER_PATH, hash, pShaderType);
	f = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (f != INVALID_HANDLE_VALUE)
	{
		LogInfo("    Replacement shader found. Loading replacement HLSL code.\n");
		WarnIfConflictingShaderExists(path, end_user_conflicting_shader_msg);

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
		shaderModel = GetShaderModel(pShaderBytecode, pBytecodeLength);
		if (shaderModel.empty())
		{
			LogInfo("    disassembly of original shader failed.\n");

			delete[] srcData;
		}
		else
		{
			// Any HLSL compiled shaders are reloading candidates, if moved to ShaderFixes
			pShaderModel = shaderModel;
			pTimeStamp = ftWrite;
			std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> utf8_to_utf16;
			pHeaderLine = utf8_to_utf16.from_bytes(srcData, strchr(srcData, '\n'));

			// Way too many obscure interractions in this function, using another
			// temporary variable to not modify anything already here and reduce
			// the risk of breaking it in some subtle way:
			const char *tmpShaderModel;
			char apath[MAX_PATH];

			if (pOverrideShaderModel)
				tmpShaderModel = pOverrideShaderModel;
			else
				tmpShaderModel = shaderModel.c_str();

			// Compile replacement.
			LogInfo("    compiling replacement HLSL code with shader model %s\n", tmpShaderModel);

			// TODO: Add #defines for StereoParams and IniParams

			ID3DBlob *errorMsgs; // FIXME: This can leak
			ID3DBlob *compiledOutput = 0;
			// Pass the real filename and use the standard include handler so that
			// #include will work with a relative path from the shader itself.
			// Later we could add a custom include handler to track dependencies so
			// that we can make reloading work better when using includes:
			wcstombs(apath, path, MAX_PATH);
			HRESULT ret = D3DCompile(srcData, srcDataSize, apath, 0, D3D_COMPILE_STANDARD_FILE_INCLUDE,
				"main", tmpShaderModel, D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &compiledOutput, &errorMsgs);
			delete[] srcData; srcData = 0;
			if (compiledOutput)
			{
				pCodeSize = compiledOutput->GetBufferSize();
				pCode = new char[pCodeSize];
				memcpy(pCode, compiledOutput->GetBufferPointer(), pCodeSize);
				compiledOutput->Release(); compiledOutput = 0;
			}

			LogInfo("    compile result of replacement HLSL shader: %x\n", ret);

			if (LogFile && errorMsgs)
			{
				LPVOID errMsg = errorMsgs->GetBufferPointer();
				SIZE_T errSize = errorMsgs->GetBufferSize();
				LogInfo("--------------------------------------------- BEGIN ---------------------------------------------\n");
				fwrite(errMsg, 1, errSize - 1, LogFile);
				LogInfo("---------------------------------------------- END ----------------------------------------------\n");
				errorMsgs->Release();
			}

			// Cache binary replacement.
			if (G->CACHE_SHADERS && pCode)
			{
				swprintf_s(path, MAX_PATH, L"%ls\\%016llx-%ls_replace.bin", G->SHADER_PATH, hash, pShaderType);
				FILE *fw;
				wfopen_ensuring_access(&fw, path, L"wb");
				if (fw)
				{
					LogInfo("    storing compiled shader to %S\n", path);
					fwrite(pCode, 1, pCodeSize, fw);
					fclose(fw);

					// Set the last modified timestamp on the cached shader to match the
					// .txt file it is created from, so we can later check its validity:
					set_file_last_write_time(path, &ftWrite);
				}
				else
					LogInfo("    error writing compiled shader to %S\n", path);
			}
		}
	}
}
static void ReplaceASMShader(__in UINT64 hash, const wchar_t *pShaderType, const void *pShaderBytecode, SIZE_T pBytecodeLength,
	__out char* &pCode, SIZE_T &pCodeSize, string &pShaderModel, FILETIME &pTimeStamp, wstring &pHeaderLine, wchar_t *shader_path, bool helix)
{
	wchar_t path[MAX_PATH];
	HANDLE f;
	string shaderModel;
	if (helix)
		swprintf_s(path, MAX_PATH, L"%ls\\%08X.txt", shader_path, hash);
	else
		swprintf_s(path, MAX_PATH, L"%ls\\%016llx-%ls.txt", shader_path, hash, pShaderType);
	f = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (f != INVALID_HANDLE_VALUE)
	{
		LogInfo("    Replacement ASM shader found. Assembling replacement ASM code.\n");
		WarnIfConflictingShaderExists(path, end_user_conflicting_shader_msg);

		DWORD srcDataSize = GetFileSize(f, 0);
		vector<char> asmTextBytes(srcDataSize);
		DWORD readSize;
		FILETIME ftWrite;
		if (!ReadFile(f, asmTextBytes.data(), srcDataSize, &readSize, 0)
			|| !GetFileTime(f, NULL, NULL, &ftWrite)
			|| srcDataSize != readSize)
			LogInfo("    Error reading file.\n");
		CloseHandle(f);
		LogInfo("    Asm source code loaded. Size = %d\n", srcDataSize);

		// Disassemble old shader to get shader model.
		shaderModel = GetShaderModel(pShaderBytecode, pBytecodeLength);
		if (shaderModel.empty())
		{
			LogInfo("    disassembly of original shader failed.\n");
		}
		else
		{
			// Any ASM shaders are reloading candidates, if moved to ShaderFixes
			pShaderModel = shaderModel;
			pTimeStamp = ftWrite;
			std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> utf8_to_utf16;
			pHeaderLine = utf8_to_utf16.from_bytes(asmTextBytes.data(), strchr(asmTextBytes.data(), '\n'));

			vector<byte> byteCode(pBytecodeLength);
			memcpy(byteCode.data(), pShaderBytecode, pBytecodeLength);

			// Re-assemble the ASM text back to binary
			try
			{
				byteCode = assemblerDX9(&asmTextBytes);

				// ToDo: Any errors to check?  When it fails, throw an exception.

				// Assuming the re-assembly worked, let's make it the active shader code.
				pCodeSize = byteCode.size();
				pCode = new char[pCodeSize];
				memcpy(pCode, byteCode.data(), pCodeSize);

				// Cache binary replacement.
				if (G->CACHE_SHADERS && pCode)
				{
					// Write reassembled binary output as a cached shader.
					FILE *fw;
					swprintf_s(path, MAX_PATH, L"%ls\\%016llx-%ls.bin", shader_path, hash, pShaderType);
					wfopen_ensuring_access(&fw, path, L"wb");
					if (fw)
					{
						LogInfoW(L"    storing reassembled binary to %s\n", path);
						fwrite(byteCode.data(), 1, byteCode.size(), fw);
						fclose(fw);

						// Set the last modified timestamp on the cached shader to match the
						// .txt file it is created from, so we can later check its validity:
						set_file_last_write_time(path, &ftWrite);
					}
					else
					{
						LogInfoW(L"    error storing reassembled binary to %s\n", path);
					}
				}
			}
			catch (...)
			{
				LogInfo("    reassembly of ASM shader text failed.\n");
			}
		}
	}
}
inline char * D3D9Wrapper::IDirect3DDevice9::ReplaceShader(UINT64 hash, const wchar_t * shaderType, const void * pShaderBytecode, SIZE_T BytecodeLength, SIZE_T & pCodeSize, string & foundShaderModel, FILETIME & timeStamp, wstring & headerLine, const char * overrideShaderModel)
{
	foundShaderModel = "";
	timeStamp = { 0 };

	char *pCode = 0;
	wchar_t val[MAX_PATH];

	if (G->SHADER_PATH[0] && G->SHADER_CACHE_PATH[0])
	{
		// Export every original game shader as a .bin file.
		if (G->EXPORT_BINARY)
		{
			ExportOrigBinary(hash, shaderType, pShaderBytecode, BytecodeLength);
		}

		// Export every shader seen as an ASM text file.
		if (G->EXPORT_SHADERS)
		{
			CreateAsmTextFile(G->SHADER_CACHE_PATH, hash, shaderType, pShaderBytecode, BytecodeLength, false);
		}


		// Read the binary compiled shaders, as previously cached shaders.  This is how
		// fixes normally ship, so that we just load previously compiled/assembled shaders.
		LoadBinaryShaders(hash, shaderType, pCode, pCodeSize, foundShaderModel, timeStamp);

		// Load previously created HLSL shaders, but only from ShaderFixes.
		if (!pCode)
		{
			ReplaceHLSLShader(hash, shaderType, pShaderBytecode, BytecodeLength, overrideShaderModel,
				pCode, pCodeSize, foundShaderModel, timeStamp, headerLine);
		}

		// If still not found, look for replacement ASM text shaders.
		if (!pCode)
		{
			ReplaceASMShader(hash, shaderType, pShaderBytecode, BytecodeLength,
				pCode, pCodeSize, foundShaderModel, timeStamp, headerLine, G->SHADER_PATH, false);
		}

		//helix fixes?
		if (!pCode && G->helix_fix)
		{
			if (!wcscmp(shaderType, L"vs") && (G->HELIX_SHADER_PATH_VERTEX[0]))
				ReplaceASMShader(hash, shaderType, pShaderBytecode, BytecodeLength, pCode, pCodeSize, foundShaderModel, timeStamp, headerLine, G->HELIX_SHADER_PATH_VERTEX, true);
			else if (!wcscmp(shaderType, L"ps") && (G->HELIX_SHADER_PATH_PIXEL[0]))
				ReplaceASMShader(hash, shaderType, pShaderBytecode, BytecodeLength, pCode, pCodeSize, foundShaderModel, timeStamp, headerLine, G->HELIX_SHADER_PATH_PIXEL, true);
		}
	}

	// Shader hacking?
	if (G->SHADER_PATH[0] && G->SHADER_CACHE_PATH[0] && ((G->EXPORT_HLSL >= 1)) && !pCode)
	{
		// Skip?
		swprintf_s(val, MAX_PATH, L"%ls\\%016llx-%ls_bad.txt", G->SHADER_PATH, hash, shaderType);
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
				swprintf_s(val, MAX_PATH, L"%ls\\%016llx-%ls_replace.txt", G->SHADER_CACHE_PATH, hash, shaderType);
			else
				swprintf_s(val, MAX_PATH, L"%ls\\%016llx-%ls_replace.txt", G->SHADER_PATH, hash, shaderType);

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
				p.StereoParamsVertexReg = G->StereoParamsVertexReg;
				p.StereoParamsPixelReg = G->StereoParamsPixelReg;
				p.ZeroOutput = false;
				p.G = &G->decompiler_settings;
				const string decompiledCode = DecompileBinaryHLSL(p, patched, shaderModel, errorOccurred);
				if (!decompiledCode.size())
				{
					LogInfo("    error while decompiling.\n");

					return 0;
				}

				if (!errorOccurred && ((G->EXPORT_HLSL >= 1) || (G->EXPORT_FIXED && patched)))
				{
					errno_t err = wfopen_ensuring_access(&fw, val, L"wb");
					if (err != 0)
					{
						LogInfo("    !!! Fail to open replace.txt file: 0x%x\n", err);
						return 0;
					}

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
					// Way too many obscure interractions in this function, using another
					// temporary variable to not modify anything already here and reduce
					// the risk of breaking it in some subtle way:
					const char *tmpShaderModel;
					char apath[MAX_PATH];

					if (overrideShaderModel)
						tmpShaderModel = overrideShaderModel;
					else
						tmpShaderModel = shaderModel.c_str();

					LogInfo("    compiling fixed HLSL code with shader model %s, size = %Iu\n", tmpShaderModel, decompiledCode.size());

					// TODO: Add #defines for StereoParams and IniParams

					ID3DBlob *pErrorMsgs; // FIXME: This can leak
					ID3DBlob *pCompiledOutput = 0;
					// Probably unecessary here since this shader is one we freshly decompiled,
					// but for consistency pass the path here as well so that the standard
					// include handler can correctly handle includes with paths relative to the
					// shader itself:
					wcstombs(apath, val, MAX_PATH);
					ret = D3DCompile(decompiledCode.c_str(), decompiledCode.size(), apath, 0, D3D_COMPILE_STANDARD_FILE_INCLUDE,
						"main", tmpShaderModel, D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &pCompiledOutput, &pErrorMsgs);
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
						string asmText = BinaryToAsmText(pCompiledOutput->GetBufferPointer(), pCompiledOutput->GetBufferSize(), false);
						if (asmText.empty())
						{
							LogInfo("    disassembly of recompiled shader failed.\n");
						}
						else
						{
							fprintf_s(fw, "\n\n/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~ Recompiled ASM ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
							fwrite(asmText.c_str(), 1, asmText.size(), fw);
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

	return pCode;
}
inline bool D3D9Wrapper::IDirect3DDevice9::NeedOriginalShader(D3D9Wrapper::IDirect3DShader9 *wrapper)
{
	ShaderOverride *shaderOverride;

	if (G->hunting && (G->marking_mode == MarkingMode::ORIGINAL || G->config_reloadable || G->show_original_enabled))
		return true;

	if (!wrapper->shaderOverride)
		return false;
	shaderOverride = wrapper->shaderOverride;

	if ((shaderOverride->depth_filter == DepthBufferFilter::DEPTH_ACTIVE) ||
		(shaderOverride->depth_filter == DepthBufferFilter::DEPTH_INACTIVE)) {
		return true;
	}

	if (shaderOverride->partner_hash)
		return true;

	return false;
}

template <class ID3D9ShaderWrapper, class ID3D9Shader,
	HRESULT(__stdcall ::IDirect3DDevice9::*OrigCreateShader)(THIS_
		__in const DWORD *pFunction,
		__out_opt ID3D9Shader **ppShader)>
void D3D9Wrapper::IDirect3DDevice9::KeepOriginalShader(wchar_t * shaderType, ID3D9ShaderWrapper * pShader, const DWORD * pFunction)
{
	ID3D9Shader *originalShader = NULL;
	HRESULT hr;

	if (!NeedOriginalShader(pShader))
		return;

	LogInfoW(L"    keeping original shader for filtering: %016llx-%ls\n", pShader->hash, shaderType);
	hr = (GetD3D9Device()->*OrigCreateShader)(pFunction, &originalShader);

	if (SUCCEEDED(hr)) {
		++migotoResourceCount;
		pShader->originalShader = originalShader;
	}
}

template <class ID3D9ShaderWrapper, class ID3D9Shader,
	ID3D9ShaderWrapper* (*GetDirect3DShader9)(ID3D9Shader *pS, D3D9Wrapper::IDirect3DDevice9 *hackerDevice),
		HRESULT(__stdcall ::IDirect3DDevice9::*OrigCreateShader)(THIS_
			__in const DWORD *pFunction,
			__out_opt ID3D9Shader **ppShader)>
HRESULT D3D9Wrapper::IDirect3DDevice9::CreateShader(const DWORD *pFunction, ID3D9Shader **ppShader, wchar_t * shaderType, ID3D9ShaderWrapper **ppShaderWrapper)
{
	HRESULT hr = E_FAIL;
	UINT64 hash;
	string shaderModel;
	SIZE_T replaceShaderSize;
	FILETIME ftWrite;
	wstring headerLine = L"";
	ShaderOverrideMap::iterator override;
	const char *overrideShaderModel = NULL;
	char *replaceShader;
	ShaderOverride *shaderOverride = NULL;
	SIZE_T BytecodeLength = 0;

	if (pFunction && ppShader)
	{
		int i;
		for (i = 0; pFunction[i] != 0xFFFF; i++) {}
		i++;
		BytecodeLength = i * 4;
		hash = hash_shader(pFunction, BytecodeLength);
		ShaderOverrideMap::iterator override = lookup_shaderoverride(hash);
		if (override != G->mShaderOverrideMap.end()) {
			if (override->second.model[0])
				overrideShaderModel = override->second.model;
			shaderOverride = &override->second;
		}
	}

	if (hr != S_OK && ppShader && pFunction)
	{
		replaceShader = ReplaceShader(hash, shaderType, pFunction, BytecodeLength, replaceShaderSize,
			shaderModel, ftWrite, headerLine, overrideShaderModel);
		if (replaceShader)
		{
			LogDebug("    HackerDevice::Create%lsShader.  Device: %p\n", shaderType, this);
			*ppShader = NULL;
			hr = (GetD3D9Device()->*OrigCreateShader)((DWORD*)replaceShader, ppShader);
			if (SUCCEEDED(hr))
			{
				LogInfo("    shader successfully replaced.\n");
				*ppShaderWrapper = (*GetDirect3DShader9)(*ppShader, this);
				(*ppShaderWrapper)->hash = hash;
				if (shaderOverride) {
					(*ppShaderWrapper)->shaderOverride = shaderOverride;
				}
				if (G->hunting)
				{
					ID3DBlob* blob;
					hr = D3DCreateBlob(BytecodeLength, &blob);
					if (SUCCEEDED(hr)) {
						memcpy(blob->GetBufferPointer(), pFunction, blob->GetBufferSize());
						EnterCriticalSection(&G->mCriticalSection);
						RegisterForReload(*ppShaderWrapper, hash, shaderType, shaderModel, blob, ftWrite, headerLine, false);
						LeaveCriticalSection(&G->mCriticalSection);
					}
				}

				KeepOriginalShader<ID3D9ShaderWrapper, ID3D9Shader, OrigCreateShader>
					(shaderType, *ppShaderWrapper, pFunction);
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
		if (ppShader)
			*ppShader = NULL;
		hr = (GetD3D9Device()->*OrigCreateShader)(pFunction, ppShader);
		if (SUCCEEDED(hr)) {
			*ppShaderWrapper = (*GetDirect3DShader9)(*ppShader, this);
			(*ppShaderWrapper)->hash = hash;
			if (shaderOverride) {
				(*ppShaderWrapper)->shaderOverride = shaderOverride;
			}
			if (G->hunting || !shader_regex_groups.empty())
			{
				EnterCriticalSection(&G->mCriticalSection);
				ID3DBlob* blob;
				hr = D3DCreateBlob(BytecodeLength, &blob);
				if (SUCCEEDED(hr)) {
					memcpy(blob->GetBufferPointer(), pFunction, blob->GetBufferSize());
					RegisterForReload(*ppShaderWrapper, hash, shaderType, "bin", blob, ftWrite, headerLine, true);
					(*ppShaderWrapper)->originalShader = *ppShader;
					(*ppShader)->AddRef();
				}
				LeaveCriticalSection(&G->mCriticalSection);
			}
		}
	}

	if (hr == S_OK && ppShader && pFunction)
	{
		LogDebugW(L"    %ls: handle = %p, hash = %016I64x\n", shaderType, *ppShader, hash);

		EnterCriticalSection(&G->mCriticalSection);
		CompiledShaderMap::iterator i = G->mCompiledShaderMap.find(hash);
		if (i != G->mCompiledShaderMap.end())
		{
			LogInfo("  shader was compiled from source code %s\n", i->second.c_str());
		}
		LeaveCriticalSection(&G->mCriticalSection);
	}

	LogInfo("  returns result = %x, handle = %p\n", hr, *ppShader);

	return hr;
}

// -----------------------------------------------------------------------------------------------

// For any given vertex or pixel shader from the ShaderFixes folder, we need to track them at load time so
// that we can associate a given active shader with an override file.  This allows us to reload the shaders
// dynamically, and do on-the-fly fix testing.
// ShaderModel is usually something like "vs_5_0", but "bin" is a valid ShaderModel string, and tells the
// reloader to disassemble the .bin file to determine the shader model.

// Currently, critical lock must be taken BEFORE this is called.
static void RegisterForReload(D3D9Wrapper::IDirect3DShader9* ppShader, UINT64 hash, wstring shaderType, string shaderModel,
	ID3DBlob* byteCode, FILETIME timeStamp, wstring text, bool deferred_replacement_candidate)
{
	LogInfo("    shader registered for possible reloading: %016llx_%ls as %s - %ls\n", hash, shaderType.c_str(), shaderModel.c_str(), text.c_str());
	ppShader->originalShaderInfo.hash = hash;
	ppShader->originalShaderInfo.shaderType = shaderType;
	ppShader->originalShaderInfo.shaderModel = shaderModel;
	ppShader->originalShaderInfo.byteCode = byteCode;
	ppShader->originalShaderInfo.timeStamp = timeStamp;
	ppShader->originalShaderInfo.replacement = NULL;
	ppShader->originalShaderInfo.infoText = text;
	ppShader->originalShaderInfo.deferred_replacement_candidate = deferred_replacement_candidate;
	ppShader->originalShaderInfo.deferred_replacement_processed = false;

	G->mReloadedShaders.emplace(ppShader);
}

static char* hash_whitelisted_sections[] = {
	"SHDR", "SHEX",         // Bytecode
	"ISGN",         "ISG1", // Input signature
	"PCSG",         "PSG1", // Patch constant signature
	"OSGN", "OSG5", "OSG1", // Output signature
};

static uint32_t hash_shader_bytecode(struct dxbc_header *header, SIZE_T BytecodeLength)
{
	uint32_t *offsets = (uint32_t*)((char*)header + sizeof(struct dxbc_header));
	struct section_header *section;
	unsigned i, j;
	uint32_t hash = 0;

	if (BytecodeLength < sizeof(struct dxbc_header) + header->num_sections * sizeof(uint32_t))
		return 0;

	for (i = 0; i < header->num_sections; i++) {
		section = (struct section_header*)((char*)header + offsets[i]);
		if (BytecodeLength < (char*)section - (char*)header + sizeof(struct section_header) + section->size)
			return 0;

		for (j = 0; j < ARRAYSIZE(hash_whitelisted_sections); j++) {
			if (!strncmp(section->signature, hash_whitelisted_sections[j], 4))
				hash = crc32c_hw(hash, (char*)section + sizeof(struct section_header), section->size);
		}
	}

	return hash;
}
static UINT64 hash_shader(const void *pShaderBytecode, SIZE_T BytecodeLength)
{
	if (G->helix_fix) {
		return crc32_fast(pShaderBytecode, BytecodeLength, 0);
	}

	UINT64 hash = 0;

	struct dxbc_header *header = (struct dxbc_header *)pShaderBytecode;

	if (BytecodeLength < sizeof(struct dxbc_header))
		goto fnv;

	switch (G->shader_hash_type) {
	case ShaderHashType::FNV:
	fnv:
		hash = fnv_64_buf(pShaderBytecode, BytecodeLength);
		LogInfo("       FNV hash = %016I64x\n", hash);
		break;

	case ShaderHashType::EMBEDDED:
		// Confirmed with dx11shaderanalyse that the hash
		// embedded in the file is as md5sum would have printed
		// it (that is - if md5sum used the same obfuscated
		// message size padding), so read it as big-endian so
		// that we print it the same way for consistency.
		//
		// Endian bug: _byteswap_uint64 is unconditional, but I
		// don't want to pull in all of winsock just for ntohl,
		// and since we are only targetting x86... meh.
		hash = _byteswap_uint64(header->hash[0] | (UINT64)header->hash[1] << 32);
		LogInfo("  Embedded hash = %016I64x\n", hash);
		break;

	case ShaderHashType::BYTECODE:
		hash = hash_shader_bytecode(header, BytecodeLength);
		if (!hash)
			goto fnv;
		LogInfo("  Bytecode hash = %016I64x\n", hash);
		break;
	}

	return hash;
}
inline void D3D9Wrapper::IDirect3DDevice9::Create3DMigotoResources()
{
	LogInfo("HackerDevice::Create3DMigotoResources(%s@%p) called.\n", type_name_dx9((IUnknown*)this), this);

	// XXX: Ignoring the return values for now because so do our callers.
	// If we want to change this, keep in mind that failures in
	// CreateStereoParamResources and SetGlobalNVSurfaceCreationMode should
	// be considdered non-fatal, as stereo could be disabled in the control
	// panel, or we could be on an AMD or Intel card.

	CreateStereoTexture();
	CreateStereoHandle();
	CreatePinkHuntingResources();
	SetGlobalNVSurfaceCreationMode();

	optimise_command_lists(this);
}
inline ::IDirect3DDevice9 * D3D9Wrapper::IDirect3DDevice9::GetPassThroughOrigDevice()
{
	return GetD3D9Device();
}

inline void D3D9Wrapper::IDirect3DDevice9::HookDevice()
{

	// This will install hooks in the original device (if they have not
	// already been installed from a prior device) which will call the
	// equivalent function in this HackerDevice. It returns a trampoline
	// interface which we use in place of GetD3D9Device() to call the real
	// original device, thereby side stepping the problem that calling the
	// old GetD3D9Device() would be hooked and call back into us endlessly:
	if (_ex)
		m_pUnk = hook_device(GetD3D9DeviceEx(), reinterpret_cast<::IDirect3DDevice9Ex*>(this), G->enable_hooks);
	else
		m_pUnk = hook_device(GetD3D9Device(), reinterpret_cast<::IDirect3DDevice9*>(this), G->enable_hooks);
}

inline void D3D9Wrapper::IDirect3DDevice9::Bind3DMigotoResources()
{
	HRESULT hr;
	// Set NVidia stereo texture.
	if (this->mStereoTexture && ((G->helix_fix && G->helix_StereoParamsVertexReg > -1) || G->StereoParamsVertexReg > -1)) {
		if (G->helix_fix && G->helix_StereoParamsVertexReg > -1) {
			LogDebug("  adding NVidia stereo parameter texture to vertex shader resources in slot %i.\n", G->helix_StereoParamsVertexReg);
			hr = GetD3D9Device()->SetTexture(G->helix_StereoParamsVertexReg, this->mStereoTexture);
			if (FAILED(hr))
				LogInfo("  failed to add NVidia stereo parameter texture to vertex shader resources in slot %i.\n", G->helix_StereoParamsVertexReg);
		}
		else {
			LogDebug("  adding NVidia stereo parameter texture to vertex shader resources in slot %i.\n", G->StereoParamsVertexReg);
			hr = GetD3D9Device()->SetTexture(G->StereoParamsVertexReg, this->mStereoTexture);
			if (FAILED(hr))
				LogInfo("  failed to add NVidia stereo parameter texture to vertex shader resources in slot %i.\n", G->StereoParamsVertexReg);
		}
	}
	if (this->mStereoTexture && ((G->helix_fix && G->helix_StereoParamsPixelReg > -1) || G->StereoParamsPixelReg > -1)) {
		if (G->helix_fix && G->helix_StereoParamsPixelReg > -1) {
			LogDebug("  adding NVidia stereo parameter texture to pixel shader resources in slot %i.\n", G->helix_StereoParamsPixelReg);
			hr = GetD3D9Device()->SetTexture(G->helix_StereoParamsPixelReg, this->mStereoTexture);
			if (FAILED(hr))
				LogInfo("  failed to add NVidia stereo parameter texture to pixel shader resources in slot %i.\n", G->helix_StereoParamsPixelReg);
		}
		else {
			LogDebug("  adding NVidia stereo parameter texture to pixel shader resources in slot %i.\n", G->StereoParamsPixelReg);
			hr = GetD3D9Device()->SetTexture(G->StereoParamsPixelReg, this->mStereoTexture);
			if (FAILED(hr))
				LogInfo("  failed to add NVidia stereo parameter texture to pixel shader resources in slot %i.\n", G->StereoParamsPixelReg);
		}
	}
	for (map<int, DirectX::XMFLOAT4>::iterator it = G->IniConstants.begin(); it != G->IniConstants.end(); ++it) {
		LogDebug("  setting ini constants in vertex and pixel registers.\n");
		float pConstants[4] = { it->second.x, it->second.y, it->second.z, it->second.w };
		hr = GetD3D9Device()->SetVertexShaderConstantF(it->first, pConstants, 1);
		if (FAILED(hr))
			LogInfo("  failed to set ini constants for vertex shader in slot %i.\n", it->first);
		hr = GetD3D9Device()->SetPixelShaderConstantF(it->first, pConstants, 1);
		if (FAILED(hr))
			LogInfo("  failed to set ini constants for pixel shader in slot %i.\n", it->first);
	}
}

HRESULT D3D9Wrapper::IDirect3DDevice9::_SetTexture(DWORD Sampler, ::IDirect3DBaseTexture9 * pTexture)
{
	bool overrideStereo = false;
	if (this->mStereoTexture){

		if (G->helix_fix && (G->helix_StereoParamsVertexReg == Sampler)) {
			LogInfo("  Game attempted to unbind vertex StereoParams, pinning in slot %i\n", G->helix_StereoParamsVertexReg);
			overrideStereo = true;
		}
		else if (G->helix_fix && (G->helix_StereoParamsPixelReg == Sampler)) {
			LogInfo("  Game attempted to unbind pixel StereoParams, pinning in slot %i\n", G->helix_StereoParamsVertexReg);
			overrideStereo = true;
		}
		else if (G->StereoParamsVertexReg == Sampler) {
			LogInfo("  Game attempted to unbind vertex StereoParams, pinning in slot %i\n", G->StereoParamsVertexReg);
			overrideStereo = true;
		}else if (G->StereoParamsPixelReg == Sampler) {
			LogInfo("  Game attempted to unbind pixel StereoParams, pinning in slot %i\n", G->StereoParamsPixelReg);
			overrideStereo = true;
		}
	}
	if (!overrideStereo) {
		return GetD3D9Device()->SetTexture(Sampler, pTexture);
	}
	else {

		return D3D_OK;
	}
}


bool LockDenyCPURead(
	D3D9Wrapper::IDirect3DResource9 *pResource,
	DWORD MapFlags, UINT Level = 0)
{
	uint32_t hash;
	TextureOverrideMap::iterator i;

	// Currently only replacing first subresource to simplify map type, and
	// only on read access as it is unclear how to handle a read/write access.
	if (Level != 0)
		return false;

	if (G->mTextureOverrideMap.empty())
		return false;

	hash = GetResourceHash(pResource);

	i = lookup_textureoverride(hash);
	if (i == G->mTextureOverrideMap.end())
		return false;

	return i->second.begin()->deny_cpu_read;
}
template <typename Surface>
void D3D9Wrapper::IDirect3DDevice9::TrackAndDivertLock(HRESULT lock_hr, Surface *pResource,
	 ::D3DLOCKED_RECT *pLockedRect, DWORD MapFlags, UINT Level)
{
	::D3DSURFACE_DESC sur_desc;
	LockedResourceInfo *locked_info = NULL;

	::D3DRESOURCETYPE type = pResource->GetD3DResource9()->GetType();

	void *replace = NULL;
	bool divertable = false, divert = false, track = false;
	bool write = false, read = false, deny = false;
	Profiling::State profiling_state;

	if (Profiling::mode == Profiling::Mode::SUMMARY)
		Profiling::start(&profiling_state);

	if (FAILED(lock_hr) || !pResource || !pLockedRect || !pLockedRect->pBits)
		goto out_profile;

	if (!(MapFlags & D3DLOCK_READONLY))
		write = true;
	if (MapFlags & D3DLOCK_NOOVERWRITE)
		divert = track = LockTrackResourceHashUpdate(pResource, Level);
	read = divertable = true;
	divert = deny = LockDenyCPURead(pResource, MapFlags, Level);


	if (!track && !divert)
		goto out_profile;

	locked_info = &pResource->lockedResourceInfo;
	locked_info->locked_writable = write;

	::D3DLOCKED_BOX lockedBox;
	lockedBox.pBits = pLockedRect->pBits;
	lockedBox.RowPitch = pLockedRect->Pitch;
	lockedBox.SlicePitch = 0;

	memcpy(&locked_info->lockedBox, &lockedBox, sizeof(::D3DLOCKED_BOX));

	if (!divertable || !divert)
		goto out_profile;

	switch (type) {
		case ::D3DRTYPE_SURFACE:
			((::IDirect3DSurface9*)pResource->GetD3DResource9())->GetDesc(&sur_desc);
			break;
		case ::D3DRTYPE_TEXTURE:
			((::IDirect3DTexture9*)pResource->GetD3DResource9())->GetLevelDesc(0, &sur_desc);
			break;
		case ::D3DRTYPE_CUBETEXTURE:
			((::IDirect3DCubeTexture9*)pResource->GetD3DResource9())->GetLevelDesc(0, &sur_desc);
			break;
	}

	locked_info->size = pLockedRect->Pitch * sur_desc.Height;
	replace = malloc(locked_info->size);
	if (!replace) {
		LogInfo("LockAndDivertMap out of memory\n");
		goto out_profile;
	}

	if (read && !deny)
		memcpy(replace, pLockedRect->pBits, locked_info->size);
	else
		memset(replace, 0, locked_info->size);

	locked_info->orig_pData = pLockedRect->pBits;
	locked_info->lockedBox.pBits = replace;
	pLockedRect->pBits = replace;

out_profile:
	if (Profiling::mode == Profiling::Mode::SUMMARY)
		Profiling::end(&profiling_state, &Profiling::map_overhead);
}
void D3D9Wrapper::IDirect3DDevice9::TrackAndDivertLock(HRESULT lock_hr, D3D9Wrapper::IDirect3DVolumeTexture9 *pResource,
	 ::D3DLOCKED_BOX *pLockedBox, DWORD MapFlags, UINT Level)
{
	::D3DVOLUME_DESC vol_desc;
	LockedResourceInfo *locked_info = NULL;

	void *replace = NULL;
	bool divertable = false, divert = false, track = false;
	bool write = false, read = false, deny = false;

	Profiling::State profiling_state;

	if (Profiling::mode == Profiling::Mode::SUMMARY)
		Profiling::start(&profiling_state);

	if (FAILED(lock_hr) || !pResource || !pLockedBox || !pLockedBox->pBits)
		goto out_profile;

	if (!(MapFlags & D3DLOCK_READONLY))
		write = true;
	if (MapFlags & D3DLOCK_NOOVERWRITE)
		divert = track = LockTrackResourceHashUpdate(pResource, Level);
	read = divertable = true;
	divert = deny = LockDenyCPURead(pResource, MapFlags, Level);

	if (!track && !divert)
		goto out_profile;

	locked_info = &pResource->lockedResourceInfo;
	locked_info->locked_writable = write;
	memcpy(&locked_info->lockedBox, pLockedBox, sizeof(::D3DLOCKED_BOX));

	if (!divertable || !divert)
		goto out_profile;

	pResource->GetD3DVolumeTexture9()->GetLevelDesc(0, &vol_desc);
	locked_info->size = pLockedBox->SlicePitch * vol_desc.Depth;

	replace = malloc(locked_info->size);
	if (!replace) {
		LogInfo("LockAndDivertMap out of memory\n");
		goto out_profile;
	}

	if (read && !deny)
		memcpy(replace, pLockedBox->pBits, locked_info->size);
	else
		memset(replace, 0, locked_info->size);

	locked_info->orig_pData = pLockedBox->pBits;
	locked_info->lockedBox.pBits = replace;
	pLockedBox->pBits = replace;

out_profile:
	if (Profiling::mode == Profiling::Mode::SUMMARY)
		Profiling::end(&profiling_state, &Profiling::map_overhead);
}
template<typename Buffer, typename Desc>
void D3D9Wrapper::IDirect3DDevice9::TrackAndDivertLock(HRESULT lock_hr, Buffer * pResource, UINT SizeToLock, void *ppbData, DWORD MapFlags)
{
	void *replace = NULL;
	bool divertable = false, divert = false, track = false;
	bool write = false, read = false, deny = false;
	LockedResourceInfo *locked_info = NULL;
	Profiling::State profiling_state;

	if (Profiling::mode == Profiling::Mode::SUMMARY)
		Profiling::start(&profiling_state);

	if (FAILED(lock_hr) || !pResource || !ppbData)
		goto out_profile;

	if (!(MapFlags & D3DLOCK_READONLY))
		write = true;
	if (MapFlags & D3DLOCK_NOOVERWRITE)
		divert = track = LockTrackResourceHashUpdate(pResource);
	read = divertable = true;
	divert = deny = LockDenyCPURead(pResource, MapFlags);

	if (!track && !divert)
		goto out_profile;

	locked_info = &pResource->lockedResourceInfo;
	locked_info->locked_writable = write;

	::D3DLOCKED_BOX lockedBox;
	lockedBox.pBits = ppbData;
	lockedBox.RowPitch = 0;
	lockedBox.SlicePitch = 0;

	memcpy(&locked_info->lockedBox, &lockedBox, sizeof(::D3DLOCKED_BOX));

	if (!divertable || !divert)
		goto out_profile;

	if (SizeToLock == 0) {
		Desc pDesc;
		pResource->GetDesc(&pDesc);
		locked_info->size = pDesc.Size;
	}
	else {
		locked_info->size = SizeToLock;
	}
	replace = malloc(locked_info->size);
	if (!replace) {
		LogInfo("LockAndDivertMap out of memory\n");
		goto out_profile;
	}

	if (read && !deny)
		memcpy(replace, ppbData, locked_info->size);
	else
		memset(replace, 0, locked_info->size);

	locked_info->orig_pData = ppbData;
	locked_info->lockedBox.pBits = replace;
	ppbData = replace;

out_profile:
	if (Profiling::mode == Profiling::Mode::SUMMARY)
		Profiling::end(&profiling_state, &Profiling::map_overhead);
}
void D3D9Wrapper::IDirect3DDevice9::TrackAndDivertUnlock(D3D9Wrapper::IDirect3DResource9 *pResource, UINT Level)
{
	LockedResourceInfo *lock_info = NULL;
	Profiling::State profiling_state;

	if (Profiling::mode == Profiling::Mode::SUMMARY)
		Profiling::start(&profiling_state);


	lock_info = &pResource->lockedResourceInfo;

	if (G->track_texture_updates && Level == 0 && lock_info->locked_writable)
		UpdateResourceHashFromCPU(pResource, &lock_info->lockedBox);

	if (lock_info->orig_pData) {
		// TODO: Measure performance vs. not diverting:
		if (lock_info->locked_writable)
			memcpy(lock_info->orig_pData, lock_info->lockedBox.pBits, lock_info->size);

		free(lock_info->lockedBox.pBits);
	}

	if (Profiling::mode == Profiling::Mode::SUMMARY)
		Profiling::end(&profiling_state, &Profiling::map_overhead);
}
static ResourceSnapshot SnapshotResource(D3D9Wrapper::IDirect3DResource9 *handle)
{
	uint32_t hash = 0, orig_hash = 0;

	ResourceHandleInfo *info = GetResourceHandleInfo(handle);
	if (info) {
		hash = info->hash;
		orig_hash = info->orig_hash;
	}
	return ResourceSnapshot(handle , hash, orig_hash);
}
void D3D9Wrapper::IDirect3DDevice9::RecordPixelShaderResourceUsage(ShaderInfoData *shader_info)
{
	for (int i = 0; i < 16; i++) {
		D3D9Wrapper::IDirect3DBaseTexture9 *tex;
		if (m_activeStereoTextureStages.count(i) == 1) {
			tex = m_activeStereoTextureStages[i];
			if (tex) {
				RecordResourceStats(tex, &G->mShaderResourceInfo);
				shader_info->ResourceRegisters[i].insert(SnapshotResource(tex));
				tex->Release();
			}
		}
	}
}
void D3D9Wrapper::IDirect3DDevice9::RecordVertexShaderResourceUsage(ShaderInfoData *shader_info)
{
	for (int i = 0; i < 4; i++) {
		D3D9Wrapper::IDirect3DBaseTexture9 *tex;
		if (m_activeStereoTextureStages.count(D3DDMAPSAMPLER + (i + 1)) == 1) {
			tex = m_activeStereoTextureStages[D3DDMAPSAMPLER + (i + 1)];
			if (tex) {
				RecordResourceStats(tex, &G->mShaderResourceInfo);
				shader_info->ResourceRegisters[D3DDMAPSAMPLER + (i + 1)].insert(SnapshotResource(tex));
				tex->Release();
			}
		}
	}
	}
void D3D9Wrapper::IDirect3DDevice9::RecordResourceStats(D3D9Wrapper::IDirect3DResource9 *resource, std::set<uint32_t> *resource_info)
{
	uint32_t orig_hash = 0;
	if (!resource)
		return;

	// We are using the original resource hash for stat collection - things
	// get tricky otherwise
	orig_hash = GetOrigResourceHash(resource);

	if (orig_hash)
		resource_info->insert(orig_hash);
}

void D3D9Wrapper::IDirect3DDevice9::RecordPeerShaders(set<UINT64> *PeerShaders, D3D9Wrapper::IDirect3DShader9 *this_shader)
{
	if (mCurrentVertexShaderHandle && mCurrentVertexShaderHandle->hash && mCurrentVertexShaderHandle->hash != this_shader->hash)
		PeerShaders->insert(mCurrentVertexShaderHandle->hash);

	if (mCurrentPixelShaderHandle && mCurrentPixelShaderHandle->hash && mCurrentPixelShaderHandle->hash != this_shader->hash)
		PeerShaders->insert(mCurrentPixelShaderHandle->hash);
}


void D3D9Wrapper::IDirect3DDevice9::RecordGraphicsShaderStats()
{
	UINT selectedRenderTargetPos;
	ShaderInfoData *info;
	Profiling::State profiling_state;

	if (Profiling::mode == Profiling::Mode::SUMMARY)
		Profiling::start(&profiling_state);

	if (mCurrentVertexShaderHandle) {
		info = &G->mVertexShaderInfo[mCurrentVertexShaderHandle->hash];
		mCurrentVertexShaderHandle->shaderInfo = info;
		RecordVertexShaderResourceUsage(info);
		RecordPeerShaders(&info->PeerShaders, mCurrentVertexShaderHandle);
	}

	if (mCurrentPixelShaderHandle) {
		info = &G->mPixelShaderInfo[mCurrentPixelShaderHandle->hash];
		mCurrentPixelShaderHandle->shaderInfo = info;
		RecordPixelShaderResourceUsage(info);
		RecordPeerShaders(&info->PeerShaders, mCurrentPixelShaderHandle);

		for (selectedRenderTargetPos = 0; selectedRenderTargetPos < mCurrentRenderTargets.size(); ++selectedRenderTargetPos) {
			if (selectedRenderTargetPos >= info->RenderTargets.size())
				info->RenderTargets.push_back(set<ResourceSnapshot>());

			info->RenderTargets[selectedRenderTargetPos].insert(SnapshotResource(mCurrentRenderTargets[selectedRenderTargetPos]));
		}

		if (m_pActiveDepthStencil)
			info->DepthTargets.insert(SnapshotResource(m_pActiveDepthStencil));
	}
	if (Profiling::mode == Profiling::Mode::SUMMARY)
		Profiling::end(&profiling_state, &Profiling::stat_overhead);
}

void D3D9Wrapper::IDirect3DDevice9::BeforeDraw(DrawContext & data)
{
	Profiling::State profiling_state;

	if (Profiling::mode == Profiling::Mode::SUMMARY)
		Profiling::start(&profiling_state);
	if (G->gAutoDetectDepthBuffer) {
		UINT _vertices = DrawPrimitiveCountToVerticesCount(data.call_info.PrimitiveCount, data.call_info.primitive_type);
		vertices += _vertices;
		drawCalls += 1;

		if (m_pActiveDepthStencil != nullptr)
		{
			if (m_pActiveDepthStencil->possibleDepthBuffer) {
				m_pActiveDepthStencil->depthSourceInfo.drawcall_count = drawCalls;
				m_pActiveDepthStencil->depthSourceInfo.vertices_count += _vertices;
				m_pActiveDepthStencil->depthSourceInfo.last_cmp_func = current_zfunc;
			}
		}

	}
	// If we are not hunting shaders, we should skip all of this shader management for a performance bump.
	if (G->hunting == HUNTING_MODE_ENABLED)
	{
		UINT selectedVertexBufferPos;
		UINT selectedRenderTargetPos;
		UINT i;

		EnterCriticalSection(&G->mCriticalSection);
		{
			// In some cases stat collection can have a significant
			// performance impact or may result in a runaway
			// memory leak, so only do it if dump_usage is enabled:
			if (G->DumpUsage)
				RecordGraphicsShaderStats();

			// Selection

			UINT64 vertexHash = -1;
			UINT64 pixelHash = -1;

			if (mCurrentVertexShaderHandle)
				vertexHash = mCurrentVertexShaderHandle->hash;

			if (mCurrentPixelShaderHandle)
				pixelHash = mCurrentPixelShaderHandle->hash;
			// Selection
			for (selectedVertexBufferPos = 0; selectedVertexBufferPos < D3D9_VERTEX_INPUT_RESOURCE_SLOT_COUNT; ++selectedVertexBufferPos) {
				if (mCurrentVertexBuffers[selectedVertexBufferPos] == G->mSelectedVertexBuffer)
					break;
			}
			for (selectedRenderTargetPos = 0; selectedRenderTargetPos < mCurrentRenderTargets.size(); ++selectedRenderTargetPos) {
				if (mCurrentRenderTargets[selectedRenderTargetPos] == G->mSelectedRenderTarget)
					break;
			}

			if (mCurrentIndexBuffer == G->mSelectedIndexBuffer ||
				vertexHash == G->mSelectedVertexShader ||
				pixelHash == G->mSelectedPixelShader ||
				selectedVertexBufferPos < D3D9_VERTEX_INPUT_RESOURCE_SLOT_COUNT ||
				selectedRenderTargetPos < mCurrentRenderTargets.size())
			{
				LogDebug("  Skipping selected operation. CurrentIndexBuffer = %08lx, CurrentVertexShader = %016I64x, CurrentPixelShader = %016I64x\n",
					mCurrentIndexBuffer, vertexHash, pixelHash);

				// Snapshot render target list.
				if (G->mSelectedRenderTargetSnapshot != G->mSelectedRenderTarget)
				{
					G->mSelectedRenderTargetSnapshotList.clear();
					G->mSelectedRenderTargetSnapshot = G->mSelectedRenderTarget;
				}
				G->mSelectedRenderTargetSnapshotList.insert(mCurrentRenderTargets.begin(), mCurrentRenderTargets.end());
				// Snapshot info.
				for (i = 0; i < D3D9_VERTEX_INPUT_RESOURCE_SLOT_COUNT; i++) {
					if (mCurrentVertexBuffers[i] == G->mSelectedVertexBuffer) {
						G->mSelectedVertexBuffer_VertexShader.insert(vertexHash);
						G->mSelectedVertexBuffer_PixelShader.insert(pixelHash);
					}
				}
				if (mCurrentIndexBuffer == G->mSelectedIndexBuffer)
				{
					G->mSelectedIndexBuffer_VertexShader.insert(vertexHash);
					G->mSelectedIndexBuffer_PixelShader.insert(pixelHash);
				}
				if (vertexHash == G->mSelectedVertexShader) {
					for (i = 0; i < D3D9_VERTEX_INPUT_RESOURCE_SLOT_COUNT; i++) {
						if (mCurrentVertexBuffers[i])
							G->mSelectedVertexShader_VertexBuffer.insert(mCurrentVertexBuffers[i]);
					}
					if (mCurrentIndexBuffer)
						G->mSelectedVertexShader_IndexBuffer.insert(mCurrentIndexBuffer);
				}
				if (pixelHash == G->mSelectedPixelShader) {
					for (i = 0; i < D3D9_VERTEX_INPUT_RESOURCE_SLOT_COUNT; i++) {
						if (mCurrentVertexBuffers[i])
							G->mSelectedVertexShader_VertexBuffer.insert(mCurrentVertexBuffers[i]);
					}
					if (mCurrentIndexBuffer)
						G->mSelectedPixelShader_IndexBuffer.insert(mCurrentIndexBuffer);
				}
				if (G->marking_mode == MarkingMode::MONO && mStereoHandle)
				{
					LogDebug("  setting separation=0 for hunting\n");

					if (NVAPI_OK != GetSeparation(this, &data.cachedStereoValues, &data.oldSeparation)) //Profiling::NvAPI_Stereo_GetSeparation(this->mStereoHandle, &data.oldSeparation))
						LogDebug("    Stereo_GetSeparation failed.\n");

					NvAPIOverride();
					if (NVAPI_OK != SetSeparation(this, &data.cachedStereoValues, 0))//Profiling::NvAPI_Stereo_SetSeparation(this->mStereoHandle, 0))
						LogDebug("    Stereo_SetSeparation failed.\n");
				}
				else if (G->marking_mode == MarkingMode::SKIP)
				{
					data.call_info.skip = true;

					// If we have transferred the draw call to a custom shader via "handling =
					// skip" and "draw = from_caller" we still want a way to skip it for hunting.
					// We can't reuse call_info.skip for that, as that is also set by
					// "handling=skip", which may happen before the "draw=from_caller", so we
					// use a second skip flag specifically for hunting:
					data.call_info.hunting_skip = true;
				}
				else if (G->marking_mode == MarkingMode::PINK)
				{
					if (G->mPinkingShader) {
						data.oldPixelShader = mCurrentPixelShaderHandle;
						SwitchPSShader(G->mPinkingShader);
					}

				}
			}
		}
		LeaveCriticalSection(&G->mCriticalSection);
	}

	if (!G->fix_enabled)
		goto out_profile;

	DeferredShaderReplacementBeforeDraw();

	// Override settings?
	if (mCurrentVertexShaderHandle && mCurrentVertexShaderHandle->shaderOverride) {
		if (mCurrentVertexShaderHandle->shaderOverride->per_frame) {
			if (mCurrentVertexShaderHandle->shaderOverride->frame_no != G->frame_no) {
				mCurrentVertexShaderHandle->shaderOverride->frame_no = G->frame_no;
				data.post_commands[0] = &mCurrentVertexShaderHandle->shaderOverride->post_command_list;
				ProcessShaderOverride(mCurrentVertexShaderHandle->shaderOverride, false, &data);
			}
		}
		else {
			data.post_commands[0] = &mCurrentVertexShaderHandle->shaderOverride->post_command_list;
			ProcessShaderOverride(mCurrentVertexShaderHandle->shaderOverride, false, &data);
		}
	}
	if (mCurrentPixelShaderHandle && mCurrentPixelShaderHandle->shaderOverride) {
		if (mCurrentPixelShaderHandle->shaderOverride->per_frame) {
			if (mCurrentPixelShaderHandle->shaderOverride->frame_no != G->frame_no) {
				mCurrentPixelShaderHandle->shaderOverride->frame_no = G->frame_no;
				data.post_commands[0] = &mCurrentPixelShaderHandle->shaderOverride->post_command_list;
				ProcessShaderOverride(mCurrentPixelShaderHandle->shaderOverride, true, &data);
			}
		}
		else {
			data.post_commands[0] = &mCurrentPixelShaderHandle->shaderOverride->post_command_list;
			ProcessShaderOverride(mCurrentPixelShaderHandle->shaderOverride, true, &data);
		}
	}
out_profile:
	if (Profiling::mode == Profiling::Mode::SUMMARY)
		Profiling::end(&profiling_state, &Profiling::draw_overhead);
}

#define ENABLE_LEGACY_FILTERS 1
void D3D9Wrapper::IDirect3DDevice9::ProcessShaderOverride(ShaderOverride *shaderOverride, bool isPixelShader, DrawContext *data)
{
	bool use_orig = false;

	LogDebug("  override found for shader\n");

	// We really want to start deprecating all the old filters and switch
	// to using the command list for much greater flexibility. This if()
	// will be optimised out by the compiler, but is here to remind anyone
	// looking at this that we don't want to extend this code further.
	if (ENABLE_LEGACY_FILTERS) {
		// Deprecated: The texture filtering support in the command
		// list can match oD for the depth buffer, which will return
		// negative zero -0.0 if no depth buffer is assigned.
		if (shaderOverride->depth_filter != DepthBufferFilter::NONE) {
			::IDirect3DSurface9 *pDepthStencilSurface = NULL;
			GetD3D9Device()->GetDepthStencilSurface(&pDepthStencilSurface);
			// Remember - we are NOT switching to the original shader when the condition is true
			if (shaderOverride->depth_filter == DepthBufferFilter::DEPTH_ACTIVE && !pDepthStencilSurface) {
				use_orig = true;
			}
			else if (shaderOverride->depth_filter == DepthBufferFilter::DEPTH_INACTIVE && pDepthStencilSurface) {
				use_orig = true;
			}

			if (pDepthStencilSurface)
				pDepthStencilSurface->Release();

			// TODO: Add alternate filter type where the depth
			// buffer state is passed as an input to the shader
		}

		// Deprecated: Partner filtering can already be achieved with
		// the command list with far more flexibility than this allows
		if (shaderOverride->partner_hash) {
			if (isPixelShader) {
				if (mCurrentVertexShaderHandle->hash != shaderOverride->partner_hash)
					use_orig = true;
			}
			else {
				if (mCurrentPixelShaderHandle->hash != shaderOverride->partner_hash)
					use_orig = true;
			}
		}
	}

	RunCommandList(this, &shaderOverride->command_list, &data->call_info, false, &data->cachedStereoValues);

	if (ENABLE_LEGACY_FILTERS) {
		// Deprecated since the logic can be moved into the shaders with far more flexibility
		if (use_orig) {
			if (isPixelShader) {
				if (mCurrentPixelShaderHandle->originalShader) {
					data->oldPixelShader = mCurrentPixelShaderHandle;
					SwitchPSShader((::IDirect3DPixelShader9*)mCurrentPixelShaderHandle->originalShader);
				}
			}
			else {
				if (mCurrentVertexShaderHandle->originalShader) {
					data->oldVertexShader = mCurrentVertexShaderHandle;
					SwitchVSShader((::IDirect3DVertexShader9*)mCurrentVertexShaderHandle->originalShader);
				}
			}
		}
	}
}

template<typename ID3D9Shader, HRESULT (__stdcall ::IDirect3DDevice9::*_CreateShader)(const DWORD*, ID3D9Shader**)>
void D3D9Wrapper::IDirect3DDevice9::DeferredShaderReplacement(D3D9Wrapper::IDirect3DShader9 *shader, wchar_t *shader_type)
{
	ID3D9Shader	*patched_shader = NULL;
	UINT64 hash = shader->hash;
	OriginalShaderInfo *orig_info = NULL;
	UINT num_instances = 0;
	string asm_text;
	bool patch_regex = false;
	HRESULT hr;
	wstring tagline(L"//");

	orig_info = &shader->originalShaderInfo;

	if (!orig_info->deferred_replacement_candidate || orig_info->deferred_replacement_processed)
		return;

	LogInfo("Performing deferred shader analysis on %S %016I64x...\n", shader_type, hash);

	// Remember that we have analysed this one so we don't check it again
	// (until config reload) regardless of whether we patch it or not:
	orig_info->deferred_replacement_processed = true;

	asm_text = BinaryToAsmText(orig_info->byteCode->GetBufferPointer(),
		orig_info->byteCode->GetBufferSize(), false);
	if (asm_text.empty())
		return;

	try {
		patch_regex = apply_shader_regex_groups(&asm_text, &orig_info->shaderModel, hash, &tagline);
	}
	catch (...) {
		LogInfo("    *** Exception while patching shader\n");
		return;
	}

	if (!patch_regex) {
		LogInfo("Patch did not apply\n");
		return;
	}

	LogInfo("Patched Shader:\n%s\n", asm_text.c_str());

	vector<char> asm_vector(asm_text.begin(), asm_text.end());
	vector<byte> patched_bytecode;

	try {
		patched_bytecode = assemblerDX9(&asm_vector);
	} catch (...) {
		LogInfo("    *** Assembling patched shader failed\n");
		return;
	}

	hr = (GetD3D9Device()->*_CreateShader)((DWORD*)patched_bytecode.data(), &patched_shader);

	if (FAILED(hr)) {
		LogInfo("    *** Creating replacement shader failed\n");
		return;
	}

	// Update replacement map so we don't have to repeat this process.
	// Not updating the bytecode in the replaced shader map - we do that
	// elsewhere, but I think that is a bug. Need to untangle that first.
	if (orig_info->replacement)
		orig_info->replacement->Release();
	else
		++migotoResourceCount;
	orig_info->replacement = patched_shader;
	orig_info->infoText = tagline;

	// And bind the replaced shader in time for this draw call:
	// VSBUGWORKAROUND: VS2013 toolchain has a bug that mistakes a member
	// pointer called "SetShader" for the SetShader we have in
	// HackerContext, even though the member pointer we were passed very
	// clearly points to a member function of ID3D11DeviceContext. VS2015
	// toolchain does not suffer from this bug.
}

void D3D9Wrapper::IDirect3DDevice9::DeferredShaderReplacementBeforeDraw()
{
	Profiling::State profiling_state;

	if (shader_regex_groups.empty())
		return;

	if (Profiling::mode == Profiling::Mode::SUMMARY)
		Profiling::start(&profiling_state);

	EnterCriticalSection(&G->mCriticalSection);

	if (mCurrentVertexShaderHandle) {
		DeferredShaderReplacement<::IDirect3DVertexShader9, &::IDirect3DDevice9::CreateVertexShader>
			(mCurrentVertexShaderHandle, L"vs");
	}
	if (mCurrentPixelShaderHandle) {
		DeferredShaderReplacement<::IDirect3DPixelShader9, &::IDirect3DDevice9::CreatePixelShader>
			(mCurrentPixelShaderHandle, L"ps");
	}

	LeaveCriticalSection(&G->mCriticalSection);

	if (Profiling::mode == Profiling::Mode::SUMMARY)
		Profiling::end(&profiling_state, &Profiling::shaderregex_overhead);
}

void D3D9Wrapper::IDirect3DDevice9::SwitchVSShader(::IDirect3DVertexShader9 *shader)
{
	GetD3D9Device()->SetVertexShader(shader);
}


void D3D9Wrapper::IDirect3DDevice9::SwitchPSShader(::IDirect3DPixelShader9 *shader)
{
	GetD3D9Device()->SetPixelShader(shader);
}
void D3D9Wrapper::IDirect3DDevice9::AfterDraw(DrawContext & data)
{
	int i;
	Profiling::State profiling_state;

	if (Profiling::mode == Profiling::Mode::SUMMARY)
		Profiling::start(&profiling_state);

	if (data.call_info.skip)
		Profiling::skipped_draw_calls++;

	for (i = 0; i < 5; i++) {
		if (data.post_commands[i]) {
			RunCommandList(this, data.post_commands[i], &data.call_info, true, &data.cachedStereoValues);
		}
	}
	if (this->mStereoHandle && data.oldSeparation != FLT_MAX) {
		NvAPIOverride();
		if (NVAPI_OK != SetSeparation(this, &data.cachedStereoValues, data.oldSeparation))//Profiling::NvAPI_Stereo_SetSeparation(this->mStereoHandle, data.oldSeparation))
			LogDebug("    Stereo_SetSeparation failed.\n");
	}

	if (data.oldVertexShader) {
		SwitchVSShader(data.oldVertexShader->GetD3DVertexShader9());
	}
	if (data.oldPixelShader) {
		SwitchPSShader(data.oldPixelShader->GetD3DPixelShader9());
	}
	if (Profiling::mode == Profiling::Mode::SUMMARY)
		Profiling::end(&profiling_state, &Profiling::draw_overhead);
}

void D3D9Wrapper::IDirect3DDevice9::RecordRenderTargetInfo(D3D9Wrapper::IDirect3DSurface9 *target, DWORD RenderTargetIndex)
{
	::D3DSURFACE_DESC desc;
	uint32_t orig_hash = 0;

	if (!target)
		return;

	target->GetDesc(&desc);

	LogDebug(" RenderTargetIndex = #%u, Format = %d",
		RenderTargetIndex, desc.Format);

	// We are using the original resource hash for stat collection - things
	// get tricky otherwise
	orig_hash = GetOrigResourceHash(target);

	mCurrentRenderTargets.push_back(target);
	G->mVisitedRenderTargets.insert(target);
	G->mRenderTargetInfo.insert(orig_hash);
}

void D3D9Wrapper::IDirect3DDevice9::RecordDepthStencil(D3D9Wrapper::IDirect3DSurface9 *target)
{
	::D3DSURFACE_DESC desc;
	uint32_t orig_hash = 0;

	if (!target)
		return;

	target->GetDesc(&desc);

	// We are using the original resource hash for stat collection - things
	// get tricky otherwise
	orig_hash = GetOrigResourceHash(target);
	G->mDepthTargetInfo.insert(orig_hash);
}

// C++ function template of common code shared by all XXSetShader functions:
template <class ID3D9ShaderWrapper, class ID3D9Shader,
	HRESULT(__stdcall ::IDirect3DDevice9::*OrigSetShader)(THIS_
		ID3D9Shader *pShader)
>
HRESULT D3D9Wrapper::IDirect3DDevice9::SetShader(ID3D9ShaderWrapper *pShader,
	set<UINT64> *visitedShaders,
	UINT64 selectedShader,
	ID3D9ShaderWrapper **currentShaderHandle)
{
	ID3D9Shader *repl_shader = NULL;
	HRESULT hr;

	// Always update the current shader handle no matter what so we can
	// reliably check if a shader of a given type is bound and for certain
	// types of old style filtering:
	if (pShader) {
		repl_shader = (ID3D9Shader*)pShader->GetRealOrig();
		// Store as current shader. Need to do this even while
		// not hunting for ShaderOverride section in BeforeDraw
		// We also set the current shader hash, but as an optimization,
		// we skip the lookup if there are no ShaderOverride The
		// lookup/find takes measurable amounts of CPU time.
		//
		LogDebug("  set shader: handle = %p, hash = %016I64x\n", pShader, pShader->hash);

		if ((G->hunting == HUNTING_MODE_ENABLED) && visitedShaders) {
			EnterCriticalSection(&G->mCriticalSection);
			visitedShaders->insert(pShader->hash);
			LeaveCriticalSection(&G->mCriticalSection);
		}
		// If the shader has been live reloaded from ShaderFixes, use the new one
		// No longer conditional on G->hunting now that hunting may be soft enabled via key binding
		//ShaderReloadMap::iterator it = G->mReloadedShaders.find(pShader);
		//if (it != G->mReloadedShaders.end() && it->second.replacement != NULL) {
		if (pShader->originalShaderInfo.replacement) {
			LogDebug("  shader replaced by: %p\n", pShader->originalShaderInfo.replacement);

			// It might make sense to Release() the original shader, to recover memory on GPU
			//   -Bo3b
			// No - we're already not incrementing the refcount since we don't bind it, and if we
			// released the original it would mean the game has an invalid pointer and can crash.
			// I wouldn't worry too much about GPU memory usage beyond leaks - the driver has a
			// full virtual memory system and can swap rarely used resources out to system memory.
			// If we did want to do better here we could return a wrapper object when the game
			// creates the original shader, and manage original/replaced/reverted/etc from there.
			//   -DSS
			repl_shader = (ID3D9Shader*)pShader->originalShaderInfo.replacement;
		}
		if (G->hunting == HUNTING_MODE_ENABLED) {
			// Replacement map.
			if (G->marking_mode == MarkingMode::ORIGINAL || !G->fix_enabled) {
				if ((selectedShader == pShader->hash || !G->fix_enabled) && pShader->originalShader) {
					repl_shader = reinterpret_cast<ID3D9Shader*>(pShader->originalShader);
				}
			}
		}

	}
	// Call through to original XXSetShader, but pShader may have been replaced.
	hr = (GetD3D9Device()->*OrigSetShader)(repl_shader);
	if (!FAILED(hr)){
		if (pShader != (*currentShaderHandle)) {
			if (pShader)
				pShader->Bound();
			if ((*currentShaderHandle))
				(*currentShaderHandle)->Unbound();
			*currentShaderHandle = pShader;
		}
	}
	return hr;
}
inline void D3D9Wrapper::IDirect3DDevice9::OnCreateOrRestore(::D3DPRESENT_PARAMETERS* pOrigParams, ::D3DPRESENT_PARAMETERS* pNewParams)
{
	deviceSwapChain->origPresentationParameters = *pOrigParams;
	if (G->SCREEN_UPSCALING > 0 || G->gForceStereo == 2) {
		string ex;
		FakeSwapChain *newFakeSwapChain = NULL;
		HRESULT hrUp = CreateFakeSwapChain(this, &newFakeSwapChain, pOrigParams, pNewParams, &ex);
		if (FAILED(hrUp)) {
			LogInfo("Creation of Upscaling Swapchain failed. Error: %s\n", ex.c_str());
			// Something went wrong inform the user with double beep and end!;
			DoubleBeepExit();
		}
		deviceSwapChain->mFakeSwapChain = newFakeSwapChain;
	}
	else {
		for (UINT i = 0; i < pOrigParams->BackBufferCount; i++) {
			::IDirect3DSurface9 *realBackBuffer = NULL;
			deviceSwapChain->GetSwapChain9()->GetBackBuffer(i, ::D3DBACKBUFFER_TYPE_MONO, &realBackBuffer);
			D3D9Wrapper::IDirect3DSurface9 *wrappedBackBuffer = IDirect3DSurface9::GetDirect3DSurface9(realBackBuffer, this, NULL, deviceSwapChain);
			deviceSwapChain->m_backBuffers.push_back(wrappedBackBuffer);
			if (i == 0) {
				m_activeRenderTargets[0] = wrappedBackBuffer;
			}
			wrappedBackBuffer->Release();
		}
		if (pOrigParams->EnableAutoDepthStencil) {
			::IDirect3DSurface9 *realDepthStencil;
			HRESULT hr = GetD3D9Device()->GetDepthStencilSurface(&realDepthStencil);
			if (!FAILED(hr)) {
				m_pActiveDepthStencil = D3D9Wrapper::IDirect3DSurface9::GetDirect3DSurface9(realDepthStencil, this, NULL);
				m_pActiveDepthStencil->Bound();
				m_pActiveDepthStencil->Release();
				if (G->gAutoDetectDepthBuffer) {
					if (depth_sources.find(m_pActiveDepthStencil) == depth_sources.end())
					{
						depth_sources.emplace(m_pActiveDepthStencil);
					}
					::D3DSURFACE_DESC desc;
					m_pActiveDepthStencil->GetD3DSurface9()->GetDesc(&desc);
					m_pActiveDepthStencil->possibleDepthBuffer = true;
					// Begin tracking
					const DepthSourceInfo info = { desc.Width, desc.Height};
					m_pActiveDepthStencil->depthSourceInfo = info;
				}
			}
		}
	}
	if (G->gForceStereo == 2) {
		::D3DXMatrixIdentity(&m_currentProjection);
		::D3DXMatrixIdentity(&m_gameProjection);
		::D3DXMatrixIdentity(&m_leftProjection);
		::D3DXMatrixIdentity(&m_rightProjection);
		GetD3D9Device()->GetViewport(&m_LastViewportSet);
	}
}
D3D9Wrapper::IDirect3DDevice9::IDirect3DDevice9(::LPDIRECT3DDEVICE9 pDevice, D3D9Wrapper::IDirect3D9 *pD3D, bool ex)
    : IDirect3DUnknown((IUnknown*) pDevice),
	m_pD3D(pD3D),
	pendingCreateDepthStencilSurface(0),
	pendingCreateDepthStencilSurfaceEx(0),
	pendingSetDepthStencilSurface(0), mStereoHandle(0),  mStereoTexture(0),
	mClDrawVertexBuffer(NULL), mClDrawVertexDecl(NULL),
	mFakeDepthSurface(NULL),
	migotoResourceCount(0),
	retreivedInitial3DSettings(false),
	m_activeRenderTargets(1, NULL),
	m_bActiveViewportIsDefault(true),
	m_pActiveDepthStencil(NULL),
	m_activeStereoTextureStages(),
	m_activeTextureStages(),
	m_activeVertexBuffers(),
	DirectModeProjectionNeedsUpdate(false),
	DirectModeIntermediateRT(NULL),
	DirectModeGameProjectionIsSet(false),
	m_activeIndexBuffer(NULL),
	currentRenderingSide(D3D9Wrapper::RenderPosition::Left),
	currentRenderPass(NULL),
	sli(false),
	recordDeviceStateBlock(NULL),
	mOverlay(NULL),
	cursor_mask_tex(NULL),
	cursor_color_tex(NULL),
	m_pActiveVertexDeclaration(NULL),
	depthstencil_replacement(NULL),
	current_zfunc(::D3DCMP_LESSEQUAL),
	update_stereo_params_interval(chrono::milliseconds(0)),
	update_stereo_params_last_run(chrono::high_resolution_clock::now()),
	_ex(ex),
	stereo_params_updated_this_frame(false)
{
	update_stereo_params_interval = chrono::milliseconds((long)(G->update_stereo_params_freq * 1000));
	mCurrentIndexBuffer = 0;
	memset(mCurrentVertexBuffers, 0, sizeof(mCurrentVertexBuffers));
	mCurrentVertexShaderHandle = NULL;
	mCurrentPixelShaderHandle = NULL;
	analyse_options = FrameAnalysisOptions::INVALID;
	frame_analysis_log = NULL;
	get_sli_enabled();
	if ((G->enable_hooks & EnableHooksDX9::DEVICE) && pDevice)
		this->HookDevice();

	if (pDevice) {
		createOverlay();
	}
	::D3DCAPS9 pCaps;
	GetD3D9Device()->GetDeviceCaps(&pCaps);
	DWORD maxRenderTargets = pCaps.NumSimultaneousRTs;
	m_activeRenderTargets.resize(maxRenderTargets, NULL);

	::IDirect3DSwapChain9 *realDeviceSwapChain = NULL;
	HRESULT hr = GetD3D9Device()->GetSwapChain(0, &realDeviceSwapChain);
	deviceSwapChain = D3D9Wrapper::IDirect3DSwapChain9::GetSwapChain(realDeviceSwapChain, this);
	deviceSwapChain->_SwapChain = 0;
	mWrappedSwapChains.push_back(deviceSwapChain);
	deviceSwapChain->Bound();
	deviceSwapChain->Release();

}
D3D9Wrapper::IDirect3DDevice9* D3D9Wrapper::IDirect3DDevice9::GetDirect3DDevice(::LPDIRECT3DDEVICE9 pDevice, D3D9Wrapper::IDirect3D9 *pD3D, bool ex)
{
	D3D9Wrapper::IDirect3DDevice9* p = new D3D9Wrapper::IDirect3DDevice9(pDevice, pD3D, ex);
	if (pDevice) m_List.AddMember(pDevice, p);

    return p;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::QueryInterface(THIS_ REFIID riid, void ** ppvObj)
{
	LogDebug("D3D9Wrapper::IDirect3DDevice9::QueryInterface called\n");// at 'this': %s\n", type_name_dx9((IUnknown*)this));
	HRESULT hr = NULL;
	if (QueryInterface_DXGI_Callback(riid, ppvObj, &hr))
		return hr;
	LogInfo("QueryInterface request for %s on %p\n", NameFromIID(riid), this);
	hr = m_pUnk->QueryInterface(riid, ppvObj);
	if (hr == S_OK) {
		if ((*ppvObj) == GetRealOrig()) {
			if (!(G->enable_hooks & EnableHooksDX9::DEVICE)) {
				*ppvObj = this;
				++m_ulRef;
				LogInfo("  interface replaced with IDirect3DDevice9 wrapper.\n");
				LogInfo("  result = %x, handle = %p\n", hr, *ppvObj);
				return hr;
			}
		}
		D3D9Wrapper::IDirect3DUnknown *unk = QueryInterface_Find_Wrapper(*ppvObj);
		if (unk)
			*ppvObj = unk;
	}
	LogInfo("  result = %x, handle = %p\n", hr, *ppvObj);
	return hr;
}

STDMETHODIMP_(ULONG) D3D9Wrapper::IDirect3DDevice9::AddRef(THIS)
{
	++m_ulRef;
	return m_pUnk->AddRef();
}
void D3D9Wrapper::IDirect3DDevice9::UnbindResources()
{
	if (mCurrentVertexShaderHandle) {
		mCurrentVertexShaderHandle->Unbound();
		mCurrentVertexShaderHandle = NULL;
	}
	if (mCurrentPixelShaderHandle) {
		mCurrentPixelShaderHandle->Unbound();
		mCurrentPixelShaderHandle = NULL;
	}
	if (m_pActiveVertexDeclaration) {
		m_pActiveVertexDeclaration->Unbound();
		m_pActiveVertexDeclaration = NULL;
	}
	if (m_pActiveDepthStencil) {
		m_pActiveDepthStencil->Unbound();
		m_pActiveDepthStencil = NULL;
	}
	if (m_activeIndexBuffer) {
		m_activeIndexBuffer->Unbound();
		m_activeIndexBuffer = NULL;
	}
	for (auto it = m_activeTextureStages.cbegin(); it != m_activeTextureStages.cend();)
	{
		it->second->Unbound(it->first);
		m_activeTextureStages.erase(it++);
	}
	for (auto it = m_activeVertexBuffers.cbegin(); it != m_activeVertexBuffers.cend();)
	{
		it->second.m_vertexBuffer->Unbound();
		m_activeVertexBuffers.erase(it++);
	}
	int x = 0;
	for (auto const& rt : m_activeRenderTargets) {
		if (rt) {
			rt->Unbound();
		}
		m_activeRenderTargets[x] = NULL;
		x++;
	}
}
void D3D9Wrapper::IDirect3DDevice9::ReleaseDeviceResources()
{
	if (nvapi_registered_resources.size() > 0) {
		for (auto i = nvapi_registered_resources.begin(); i != nvapi_registered_resources.end(); /* not hoisted */ /* no increment */)
		{
			NvAPI_D3D9_UnregisterResource((*i));
			i = nvapi_registered_resources.erase(i);
		}
	}
	if (mOverlay) {
		delete mOverlay;
		mOverlay = NULL;
	}
	ReleaseStereoTexture();
	ReleasePinkHuntingResources();

	if (mFakeSwapChains.size() > 0) {
		for (UINT x = 0; x < mFakeSwapChains.size(); x++) {
			for (UINT y = 0; y < mFakeSwapChains.at(x).mFakeBackBuffers.size(); y++) {
				if (G->gForceStereo == 2)
					m_activeRenderTargets.erase(std::remove(m_activeRenderTargets.begin(), m_activeRenderTargets.end(), mFakeSwapChains.at(x).mFakeBackBuffers.at(y)), m_activeRenderTargets.end());
				long backBufferResult = mFakeSwapChains.at(x).mFakeBackBuffers.at(y)->Release();
				LogInfo("  releasing fake swap chain, backBuffer result = %d\n", backBufferResult);
			}
			mFakeSwapChains.at(x).mFakeBackBuffers.clear();
			for (UINT y = 0; y < mFakeSwapChains.at(x).mDirectModeUpscalingBackBuffers.size(); y++) {
				if (G->gForceStereo == 2)
					m_activeRenderTargets.erase(std::remove(m_activeRenderTargets.begin(), m_activeRenderTargets.end(), mFakeSwapChains.at(x).mDirectModeUpscalingBackBuffers.at(y)), m_activeRenderTargets.end());
				long backBufferResult = mFakeSwapChains.at(x).mDirectModeUpscalingBackBuffers.at(y)->Release();
				LogInfo("  releasing fake swap chain, backBuffer result = %d\n", backBufferResult);
			}
			mFakeSwapChains.at(x).mDirectModeUpscalingBackBuffers.clear();
		}

	}
	mFakeSwapChains.clear();

	if (mFakeDepthSurface) {
		if (G->gForceStereo == 2)
			m_activeRenderTargets.erase(std::remove(m_activeRenderTargets.begin(), m_activeRenderTargets.end(), mFakeDepthSurface), m_activeRenderTargets.end());
		long detphSurfaceResult = mFakeDepthSurface->Release();
		LogInfo("  releasing fake swap chain, depth surface result = %d\n", detphSurfaceResult);
		mFakeDepthSurface = NULL;
	}

	if (mClDrawVertexBuffer) {
		long result = mClDrawVertexBuffer->Release();
		LogInfo("  releasing command list draw vertex buffer, result = %d\n", result);
		mClDrawVertexBuffer = NULL;
	}

	if (mClDrawVertexDecl) {
		long result = mClDrawVertexDecl->Release();
		LogInfo("  releasing command list vertex declaration, result = %d\n", result);
		mClDrawVertexDecl = NULL;
	}

	if (G->gForceStereo == 2) {
		for (std::vector<D3D9Wrapper::IDirect3DSurface9*>::size_type i = 0; i != m_activeRenderTargets.size(); i++)
		{
			if (m_activeRenderTargets[i] != NULL) {
				if (currentRenderingSide == RenderPosition::Right)
					m_activeRenderTargets[i]->DirectModeGetLeft()->Release();
				else
					m_activeRenderTargets[i]->DirectModeGetRight()->Release();
				m_activeRenderTargets[i] = NULL;
			}
		}
		m_activeRenderTargets.clear();
		auto it = m_activeStereoTextureStages.begin();
		while (it != m_activeStereoTextureStages.end()) {
			if (it->second) {
				switch (it->second->texType) {
				case TextureType::Texture2D:
					if (currentRenderingSide == RenderPosition::Right)
						((D3D9Wrapper::IDirect3DTexture9*)it->second)->DirectModeGetLeft()->Release();
					else
						((D3D9Wrapper::IDirect3DTexture9*)it->second)->DirectModeGetRight()->Release();
					break;
				case TextureType::Cube:
					if (currentRenderingSide == RenderPosition::Right)
						((D3D9Wrapper::IDirect3DCubeTexture9*)it->second)->DirectModeGetLeft()->Release();
					else
						((D3D9Wrapper::IDirect3DCubeTexture9*)it->second)->DirectModeGetRight()->Release();
					break;
				}
			}
			it = m_activeStereoTextureStages.erase(it);
		}
		m_activeStereoTextureStages.clear();

		auto itStages = m_activeTextureStages.begin();
		while (itStages != m_activeTextureStages.end()) {
			if (itStages->second) {
				switch (itStages->second->texType) {
				case TextureType::Texture2D:
					if (reinterpret_cast<D3D9Wrapper::IDirect3DTexture9*>(itStages->second)->IsDirectStereoTexture()) {
						if (currentRenderingSide == RenderPosition::Right)
							((D3D9Wrapper::IDirect3DTexture9*)itStages->second)->DirectModeGetLeft()->Release();
						else
							((D3D9Wrapper::IDirect3DTexture9*)itStages->second)->DirectModeGetRight()->Release();
					}
					break;
				case TextureType::Cube:
					if (reinterpret_cast<D3D9Wrapper::IDirect3DCubeTexture9*>(itStages->second)->IsDirectStereoCubeTexture()) {
						if (currentRenderingSide == RenderPosition::Right)
							((D3D9Wrapper::IDirect3DCubeTexture9*)itStages->second)->DirectModeGetLeft()->Release();
						else
							((D3D9Wrapper::IDirect3DCubeTexture9*)itStages->second)->DirectModeGetRight()->Release();
					}
					break;
				}
			}
			itStages = m_activeTextureStages.erase(itStages);
		}
		m_activeTextureStages.clear();

		if (DirectModeIntermediateRT)
			DirectModeIntermediateRT->Release();
	}
	if (cursor_mask_tex) {
		cursor_mask_tex->Release();
		cursor_mask_tex = NULL;
	}
	if (cursor_color_tex) {
		cursor_color_tex->Release();
		cursor_color_tex = NULL;
	}
	ReleaseCommandListDeviceResources(this);
	for (auto it = deviceSwapChain->m_backBuffers.begin(); it != deviceSwapChain->m_backBuffers.end();)
	{
		(*it)->Delete();
		it = deviceSwapChain->m_backBuffers.erase(it);
	}
}

STDMETHODIMP_(ULONG) D3D9Wrapper::IDirect3DDevice9::Release(THIS)
{

	LogDebug("IDirect3DDevice9::Release handle=%p, counter=%lu, this=%p\n", m_pUnk, m_ulRef, this);

	ULONG ulRef = m_pUnk ? m_pUnk->Release() : 0;
	LogDebug("  internal counter = %d\n", ulRef);
	ulRef -= migotoResourceCount;
	ulRef -= mOverlay->ReferenceCount();
	if (ulRef == 0)
    {
		if (!gLogDebug) LogInfo("IDirect3DDevice9::Release handle=%p, counter=%lu, internal counter = %lu\n", m_pUnk, m_ulRef, ulRef);
		LogInfo("  deleting self\n");
		Delete();
    }
    return ulRef;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::TestCooperativeLevel(THIS)
{
	LogInfo("IDirect3DDevice9::TestCooperativeLevel called\n");

	CheckDevice(this);
	HRESULT hr = GetD3D9Device()->TestCooperativeLevel();
	switch (hr) {
	case D3DERR_DEVICELOST:
		LogInfo("  returns D3DERR_DEVICELOST\n");
		break;
	case D3DERR_DEVICENOTRESET:
		LogInfo("  returns D3DERR_DEVICENOTRESET\n");
		break;
	case D3DERR_DRIVERINTERNALERROR:
		LogInfo("  returns D3DERR_DRIVERINTERNALERROR\n");
		break;
	default:
		LogInfo("  returns result=%x\n", hr);
	}
	return hr;
}

STDMETHODIMP_(UINT) D3D9Wrapper::IDirect3DDevice9::GetAvailableTextureMem(THIS)
{
	LogInfo("IDirect3DDevice9::GetAvailableTextureMem called\n");

	CheckDevice(this);
	return GetD3D9Device()->GetAvailableTextureMem();
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::EvictManagedResources(THIS)
{
	LogInfo("IDirect3DDevice9::EvictManagedResources called\n");

	CheckDevice(this);
	return GetD3D9Device()->EvictManagedResources();
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::GetDirect3D(THIS_ D3D9Wrapper::IDirect3D9** ppD3D9)
{
	LogInfo("IDirect3DDevice9::GetDirect3D called\n");

	CheckDevice(this);
	if (m_pD3D) {
		m_pD3D->AddRef();
	}
	else {
		return D3DERR_NOTFOUND;
	}
	if (!(G->enable_hooks & EnableHooksDX9::DEVICE)) {
		*ppD3D9 = m_pD3D;
	}
	else {
		*ppD3D9 = reinterpret_cast<D3D9Wrapper::IDirect3D9*>(m_pD3D->GetRealOrig());
	}
	LogInfo("  returns result=%x, handle=%p, wrapper=%p\n", D3D_OK, m_pD3D->GetDirect3D9(), m_pD3D);
    return D3D_OK;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::GetDeviceCaps(THIS_ ::D3DCAPS9* pCaps)
{
	LogInfo("IDirect3DDevice9::GetDeviceCaps called\n");

	CheckDevice(this);
	HRESULT hr = GetD3D9Device()->GetDeviceCaps(pCaps);
	LogInfo("  returns result=%x\n", hr);

	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::GetDisplayMode(THIS_ UINT iSwapChain,::D3DDISPLAYMODE* pMode)
{
	LogInfo("IDirect3DDevice9::GetDisplayMode called\n");

	CheckDevice(this);
	HRESULT hr = GetD3D9Device()->GetDisplayMode(iSwapChain, pMode);
	if (hr == S_OK && pMode)
	{
		//if (G->UPSCALE_MODE > 0) {
		//	if (G->GAME_INTERNAL_WIDTH() > 1)
		//		pMode->Width = G->GAME_INTERNAL_WIDTH();
		//	if (G->GAME_INTERNAL_HEIGHT() > 1)
		//		pMode->Height = G->GAME_INTERNAL_HEIGHT();
		//}
		//if (G->SCREEN_REFRESH != 1 && pMode->RefreshRate != _pOrigPresentationParameters.FullScreen_RefreshRateInHz)
		//	pMode->RefreshRate = _pOrigPresentationParameters.FullScreen_RefreshRateInHz;

//		if (G->SCREEN_REFRESH != -1 && pMode->RefreshRate != G->SCREEN_REFRESH)
//		{
//			LogInfo("  overriding refresh rate %d with %d\n", pMode->RefreshRate, G->SCREEN_REFRESH);
//
//			pMode->RefreshRate = G->SCREEN_REFRESH;
//		}
//		if (G->GAME_INTERNAL_WIDTH() > 1 && G->FORCE_REPORT_GAME_RES) {
//			pMode->Width = G->GAME_INTERNAL_WIDTH();
//		}
///*		else if (G->FORCE_REPORT_WIDTH > 0) {
//			pMode->Width = G->FORCE_REPORT_WIDTH;
//		}*/
//		if (G->GAME_INTERNAL_HEIGHT() > 1 && G->FORCE_REPORT_GAME_RES) {
//			pMode->Height = G->GAME_INTERNAL_HEIGHT();
//		}
		//else if (G->FORCE_REPORT_HEIGHT > 0) {
		//	pMode->Height = G->FORCE_REPORT_HEIGHT;
		//}
	}
	if (!pMode) LogInfo("  returns result=%x\n", hr);
	if (pMode) LogInfo("  returns result=%x, Width=%d, Height=%d, RefreshRate=%d, Format=%d\n", hr,
		pMode->Width, pMode->Height, pMode->RefreshRate, pMode->Format);

	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::GetCreationParameters(THIS_ ::D3DDEVICE_CREATION_PARAMETERS *pParameters)
{
	LogInfo("IDirect3DDevice9::GetCreationParameters called\n");

	CheckDevice(this);
	HRESULT hr = GetD3D9Device()->GetCreationParameters(pParameters);
	if (!pParameters) LogInfo("  returns result=%x\n", hr);
	if (pParameters) LogInfo("  returns result=%x, AdapterOrdinal=%d, DeviceType=%d, FocusWindow=%p, BehaviorFlags=%x\n", hr,
		pParameters->AdapterOrdinal, pParameters->DeviceType, pParameters->hFocusWindow, pParameters->BehaviorFlags);

	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::SetCursorProperties(THIS_ UINT XHotSpot,UINT YHotSpot, D3D9Wrapper::IDirect3DSurface9 *pCursorBitmap)
{
	LogInfo("IDirect3DDevice9::SetCursorProperties called\n");

	CheckDevice(this);
	HRESULT hr = GetD3D9Device()->SetCursorProperties(XHotSpot, YHotSpot, baseSurface9(pCursorBitmap));
	LogInfo("  returns result=%x\n", hr);

	return hr;
}

STDMETHODIMP_(void) D3D9Wrapper::IDirect3DDevice9::SetCursorPosition(THIS_ int X,int Y,DWORD Flags)
{
	LogInfo("IDirect3DDevice9::SetCursorPosition called\n");

	CheckDevice(this);
	if (G->SCREEN_UPSCALING > 0 && G->GAME_INTERNAL_WIDTH() > 1 && G->GAME_INTERNAL_HEIGHT() > 1) {
		X = X * G->SCREEN_WIDTH / G->GAME_INTERNAL_WIDTH();
		Y = Y * G->SCREEN_HEIGHT / G->GAME_INTERNAL_HEIGHT();
	}
	GetD3D9Device()->SetCursorPosition(X, Y, Flags);
}

STDMETHODIMP_(BOOL) D3D9Wrapper::IDirect3DDevice9::ShowCursor(THIS_ BOOL bShow)
{
	LogInfo("IDirect3DDevice9::ShowCursor called\n");

	CheckDevice(this);
	BOOL hr = GetD3D9Device()->ShowCursor(bShow);
	LogInfo("  returns result=%x\n", hr);

	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::CreateAdditionalSwapChain(THIS_ ::D3DPRESENT_PARAMETERS* pPresentationParameters, D3D9Wrapper::IDirect3DSwapChain9** pSwapChain)
{
	LogInfo("IDirect3DDevice9::CreateAdditionalSwapChain called\n");

	CheckDevice(this);
	::D3DPRESENT_PARAMETERS originalPresentParams;

	if (pPresentationParameters != nullptr) {
		// Save off the window handle so we can translate mouse cursor
		// coordinates to the window:
		if (pPresentationParameters->hDeviceWindow != NULL)
			G->sethWnd(pPresentationParameters->hDeviceWindow);

		memcpy(&originalPresentParams, pPresentationParameters, sizeof(::D3DPRESENT_PARAMETERS));
		// Require in case the software mouse and upscaling are on at the same time
		// TODO: Use a helper class to track *all* different resolutions
		G->SET_GAME_INTERNAL_WIDTH(pPresentationParameters->BackBufferWidth);
		G->SET_GAME_INTERNAL_HEIGHT(pPresentationParameters->BackBufferHeight);

		if (G->mResolutionInfo.from == GetResolutionFrom::SWAP_CHAIN) {
			// TODO: Use a helper class to track *all* different resolutions
			G->mResolutionInfo.width = pPresentationParameters->BackBufferWidth;
			G->mResolutionInfo.height = pPresentationParameters->BackBufferHeight;
			LogInfo("Got resolution from swap chain: %ix%i\n",
				G->mResolutionInfo.width, G->mResolutionInfo.height);
		}
	}

	ForceDisplayParams(pPresentationParameters);

	::IDirect3DSwapChain9 *baseSwapChain;
	HRESULT hr = GetD3D9Device()->CreateAdditionalSwapChain(pPresentationParameters, &baseSwapChain);
    if (FAILED(hr) || baseSwapChain == NULL)
    {
		LogInfo("  failed with hr=%x\n", hr);

        return hr;
    }
    D3D9Wrapper::IDirect3DSwapChain9* NewSwapChain = D3D9Wrapper::IDirect3DSwapChain9::GetSwapChain(baseSwapChain, this);
    if (NewSwapChain == NULL)
    {
		LogInfo("  error creating wrapper\n", hr);

		baseSwapChain->Release();
        return E_OUTOFMEMORY;
    }
	NewSwapChain->origPresentationParameters = originalPresentParams;
	NewSwapChain->_SwapChain = (UINT)mWrappedSwapChains.size();
	mWrappedSwapChains.push_back(NewSwapChain);

	if (G->SCREEN_UPSCALING > 0) {
		string ex;
		FakeSwapChain *newFakeSwapChain = NULL;
		HRESULT upHr = CreateFakeSwapChain(this, &newFakeSwapChain, &originalPresentParams, pPresentationParameters, &ex);
		if (FAILED(upHr)) {
			LogInfo("HackerDevice:CreateAdditionalSwapChain(): Creation of Upscaling Swapchain failed. Error: %s\n", ex.c_str());
			// Something went wrong inform the user with double beep and end!;
			DoubleBeepExit();
		}
		else {
			NewSwapChain->mFakeSwapChain = newFakeSwapChain;
		}
		pPresentationParameters->BackBufferWidth = originalPresentParams.BackBufferWidth;
		pPresentationParameters->BackBufferHeight = originalPresentParams.BackBufferHeight;
	}
	else {
		for (UINT i = 0; i < originalPresentParams.BackBufferCount; i++) {
			::IDirect3DSurface9 *realBackBuffer = NULL;
			baseSwapChain->GetBackBuffer(i, ::D3DBACKBUFFER_TYPE_MONO, &realBackBuffer);
			D3D9Wrapper::IDirect3DSurface9 *wrappedBackBuffer = IDirect3DSurface9::GetDirect3DSurface9(realBackBuffer, this, NULL, NewSwapChain);
			NewSwapChain->m_backBuffers.push_back(wrappedBackBuffer);
			wrappedBackBuffer->Release();
		}
	}
	if (pSwapChain) {
		if (!(G->enable_hooks >= EnableHooksDX9::ALL)) {
			*pSwapChain = NewSwapChain;
		}else if (baseSwapChain != NULL) {
			*pSwapChain = (D3D9Wrapper::IDirect3DSwapChain9*)baseSwapChain;
		}
	}
	LogInfo("  returns result=%x, handle=%p\n", hr, NewSwapChain);

    return hr;
}
STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::GetSwapChain(THIS_ UINT iSwapChain, D3D9Wrapper::IDirect3DSwapChain9** pSwapChain)
{
	LogInfo("IDirect3DDevice9::GetSwapChain called with SwapChain=%d\n", iSwapChain);

	HRESULT hr;
	if (!GetD3D9Device())
	{
		LogInfo("  postponing call because device was not created yet.\n");

		D3D9Wrapper::IDirect3DSwapChain9 *wrapper = D3D9Wrapper::IDirect3DSwapChain9::GetSwapChain((::LPDIRECT3DSWAPCHAIN9) 0, this);
		wrapper->_SwapChain = iSwapChain;
		wrapper->pendingGetSwapChain = true;
		wrapper->pendingDevice = this;
		*pSwapChain = wrapper;
		LogInfo("  returns handle=%p\n", wrapper);

		return S_OK;
	}
	D3D9Wrapper::IDirect3DSwapChain9* wrappedSwapChain = NULL;
	::LPDIRECT3DSWAPCHAIN9 baseSwapChain = NULL;
    *pSwapChain = NULL;
	if (iSwapChain >= mWrappedSwapChains.size())
		hr = D3DERR_INVALIDCALL;
	else {
		wrappedSwapChain = mWrappedSwapChains[iSwapChain];
		baseSwapChain = wrappedSwapChain->GetSwapChain9();
		wrappedSwapChain->AddRef();
		hr = S_OK;
		if (pSwapChain) {
			if (!(G->enable_hooks >= EnableHooksDX9::ALL)) {
				*pSwapChain = wrappedSwapChain;
			}
			else {
				*pSwapChain = (D3D9Wrapper::IDirect3DSwapChain9*)wrappedSwapChain->GetRealOrig();
			}
		}
	}
	LogInfo("  returns result=%x, handle=%p, wrapper=%p\n", hr, baseSwapChain, wrappedSwapChain);
    return hr;
}

STDMETHODIMP_(UINT) D3D9Wrapper::IDirect3DDevice9::GetNumberOfSwapChains(THIS)
{
	LogInfo("GetNumberOfSwapChains\n");

	CheckDevice(this);
	UINT hr;
	if (G->SCREEN_UPSCALING > 0){
		hr = GetD3D9Device()->GetNumberOfSwapChains() / 2;
	}
	else {
		hr = GetD3D9Device()->GetNumberOfSwapChains();
	}

	LogInfo("  returns NumberOfSwapChains=%d\n", hr);

	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::Reset(THIS_ ::D3DPRESENT_PARAMETERS* pPresentationParameters)
{
	LogInfo("IDirect3DDevice9::Reset called on handle=%p\n", GetD3D9Device());
	if (G->gForwardToEx) {
		LogInfo("  forwarding to ResetEx.\n");

		::D3DDISPLAYMODEEX fullScreenDisplayMode;
		fullScreenDisplayMode.Size = sizeof(::D3DDISPLAYMODEEX);
		if (pPresentationParameters)
		{
			fullScreenDisplayMode.Format = pPresentationParameters->BackBufferFormat;
			fullScreenDisplayMode.Height = pPresentationParameters->BackBufferHeight;
			fullScreenDisplayMode.Width = pPresentationParameters->BackBufferWidth;
			fullScreenDisplayMode.RefreshRate = pPresentationParameters->FullScreen_RefreshRateInHz;
			fullScreenDisplayMode.ScanLineOrdering = ::D3DSCANLINEORDERING_PROGRESSIVE;
		}
		::D3DDISPLAYMODEEX *pFullScreenDisplayMode = &fullScreenDisplayMode;
		if (pPresentationParameters && pPresentationParameters->Windowed)
			pFullScreenDisplayMode = 0;
		return ResetEx(pPresentationParameters, pFullScreenDisplayMode);
	}
	CheckDevice(this);
	LogInfo("  BackBufferWidth %d\n", pPresentationParameters->BackBufferWidth);
	LogInfo("  BackBufferHeight %d\n", pPresentationParameters->BackBufferHeight);
	LogInfo("  BackBufferFormat %d\n", pPresentationParameters->BackBufferFormat);
	LogInfo("  BackBufferCount %d\n", pPresentationParameters->BackBufferCount);
	LogInfo("  SwapEffect %x\n", pPresentationParameters->SwapEffect);
	LogInfo("  Flags %x\n", pPresentationParameters->Flags);
	LogInfo("  FullScreen_RefreshRateInHz %d\n", pPresentationParameters->FullScreen_RefreshRateInHz);
	LogInfo("  PresentationInterval %d\n", pPresentationParameters->PresentationInterval);
	LogInfo("  Windowed %d\n", pPresentationParameters->Windowed);
	LogInfo("  EnableAutoDepthStencil %d\n", pPresentationParameters->EnableAutoDepthStencil);
	LogInfo("  AutoDepthStencilFormat %d\n", pPresentationParameters->AutoDepthStencilFormat);
	LogInfo("  MultiSampleType %d\n", pPresentationParameters->MultiSampleType);
	LogInfo("  MultiSampleQuality %d\n", pPresentationParameters->MultiSampleQuality);
	::D3DPRESENT_PARAMETERS originalPresentParams;
	if (pPresentationParameters != nullptr) {
		// Save off the window handle so we can translate mouse cursor
		// coordinates to the window:
		if (pPresentationParameters->hDeviceWindow != NULL)
			G->sethWnd(pPresentationParameters->hDeviceWindow);
		memcpy(&originalPresentParams, pPresentationParameters, sizeof(::D3DPRESENT_PARAMETERS));
		// Require in case the software mouse and upscaling are on at the same time
		// TODO: Use a helper class to track *all* different resolutions
		G->SET_GAME_INTERNAL_WIDTH(pPresentationParameters->BackBufferWidth);
		G->SET_GAME_INTERNAL_HEIGHT(pPresentationParameters->BackBufferHeight);
		// TODO: Use a helper class to track *all* different resolutions
		G->mResolutionInfo.width = pPresentationParameters->BackBufferWidth;
		G->mResolutionInfo.height = pPresentationParameters->BackBufferHeight;
		LogInfo("Got resolution from device reset: %ix%i\n",
			G->mResolutionInfo.width, G->mResolutionInfo.height);
	}
	ForceDisplayParams(pPresentationParameters);
	LogDebug("Overridden Presentation Params\n");
	if (pPresentationParameters)
	{
		LogDebug("  BackBufferWidth = %d\n", pPresentationParameters->BackBufferWidth);
		LogDebug("  BackBufferHeight = %d\n", pPresentationParameters->BackBufferHeight);
		LogDebug("  BackBufferFormat = %d\n", pPresentationParameters->BackBufferFormat);
		LogDebug("  BackBufferCount = %d\n", pPresentationParameters->BackBufferCount);
		LogDebug("  SwapEffect = %x\n", pPresentationParameters->SwapEffect);
		LogDebug("  Flags = %x\n", pPresentationParameters->Flags);
		LogDebug("  FullScreen_RefreshRateInHz = %d\n", pPresentationParameters->FullScreen_RefreshRateInHz);
		LogDebug("  PresentationInterval = %d\n", pPresentationParameters->PresentationInterval);
		LogDebug("  Windowed = %d\n", pPresentationParameters->Windowed);
		LogDebug("  EnableAutoDepthStencil = %d\n", pPresentationParameters->EnableAutoDepthStencil);
		LogDebug("  AutoDepthStencilFormat = %d\n", pPresentationParameters->AutoDepthStencilFormat);
		LogInfo("  MultiSampleType %d\n", pPresentationParameters->MultiSampleType);
		LogInfo("  MultiSampleQuality %d\n", pPresentationParameters->MultiSampleQuality);
	}
	UnbindResources();
	ReleaseDeviceResources();
	HRESULT hr = GetD3D9Device()->Reset(pPresentationParameters);
	LogInfo("  returns result=%x\n", hr);
	InitStereoHandle();
	this->_pOrigPresentationParameters = originalPresentParams;
	this->_pPresentationParameters = *pPresentationParameters;
	createOverlay();
	CreateStereoTexture();
	CreatePinkHuntingResources();
	this->Bind3DMigotoResources();
	this->OnCreateOrRestore(&originalPresentParams, pPresentationParameters);
	RecreateCommandListCustomShaders(this);
	pPresentationParameters->BackBufferWidth = originalPresentParams.BackBufferWidth;
	pPresentationParameters->BackBufferHeight = originalPresentParams.BackBufferHeight;

	return hr;
}
STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::ResetEx(THIS_ ::D3DPRESENT_PARAMETERS* pPresentationParameters, ::D3DDISPLAYMODEEX *pFullscreenDisplayMode)
{
	LogInfo("IDirect3DDevice9Ex::ResetEx called on handle=%p\n", GetD3D9Device());

	CheckDevice(this);
	LogInfo("  BackBufferWidth %d\n", pPresentationParameters->BackBufferWidth);
	LogInfo("  BackBufferHeight %d\n", pPresentationParameters->BackBufferHeight);
	LogInfo("  BackBufferFormat %d\n", pPresentationParameters->BackBufferFormat);
	LogInfo("  BackBufferCount %d\n", pPresentationParameters->BackBufferCount);
	LogInfo("  SwapEffect %x\n", pPresentationParameters->SwapEffect);
	LogInfo("  Flags %x\n", pPresentationParameters->Flags);
	LogInfo("  FullScreen_RefreshRateInHz %d\n", pPresentationParameters->FullScreen_RefreshRateInHz);
	LogInfo("  PresentationInterval %d\n", pPresentationParameters->PresentationInterval);
	LogInfo("  Windowed %d\n", pPresentationParameters->Windowed);
	LogInfo("  EnableAutoDepthStencil %d\n", pPresentationParameters->EnableAutoDepthStencil);
	LogInfo("  AutoDepthStencilFormat %d\n", pPresentationParameters->AutoDepthStencilFormat);
	LogInfo("  MultiSampleType %d\n", pPresentationParameters->MultiSampleType);
	LogInfo("  MultiSampleQuality %d\n", pPresentationParameters->MultiSampleQuality);
	if (pFullscreenDisplayMode)
	{
		LogInfo("  FullscreenDisplayFormat = %d\n", pFullscreenDisplayMode->Format);
		LogInfo("  FullscreenDisplayWidth = %d\n", pFullscreenDisplayMode->Width);
		LogInfo("  FullscreenDisplayHeight = %d\n", pFullscreenDisplayMode->Height);
		LogInfo("  FullscreenRefreshRate = %d\n", pFullscreenDisplayMode->RefreshRate);
		LogInfo("  ScanLineOrdering = %d\n", pFullscreenDisplayMode->ScanLineOrdering);
	}
	::D3DPRESENT_PARAMETERS originalPresentParams;
	if (pPresentationParameters != nullptr) {
		// Save off the window handle so we can translate mouse cursor
		// coordinates to the window:
		if (pPresentationParameters->hDeviceWindow != NULL)
			G->sethWnd(pPresentationParameters->hDeviceWindow);
		memcpy(&originalPresentParams, pPresentationParameters, sizeof(::D3DPRESENT_PARAMETERS));
		// Require in case the software mouse and upscaling are on at the same time
		// TODO: Use a helper class to track *all* different resolutions
		G->SET_GAME_INTERNAL_WIDTH(pPresentationParameters->BackBufferWidth);
		G->SET_GAME_INTERNAL_HEIGHT(pPresentationParameters->BackBufferHeight);
		// TODO: Use a helper class to track *all* different resolutions
		G->mResolutionInfo.width = pPresentationParameters->BackBufferWidth;
		G->mResolutionInfo.height = pPresentationParameters->BackBufferHeight;
		LogInfo("Got resolution from device reset: %ix%i\n",
			G->mResolutionInfo.width, G->mResolutionInfo.height);
	}
	ForceDisplayParams(pPresentationParameters, pFullscreenDisplayMode);
	::D3DDISPLAYMODEEX fullScreenDisplayMode;
	if (pPresentationParameters && !(pPresentationParameters->Windowed) && !pFullscreenDisplayMode)
	{
		LogInfo("    creating full screen parameter structure.\n");
		fullScreenDisplayMode.Size = sizeof(::D3DDISPLAYMODEEX);
		fullScreenDisplayMode.Format = pPresentationParameters->BackBufferFormat;
		fullScreenDisplayMode.Height = pPresentationParameters->BackBufferHeight;
		fullScreenDisplayMode.Width = pPresentationParameters->BackBufferWidth;
		fullScreenDisplayMode.RefreshRate = pPresentationParameters->FullScreen_RefreshRateInHz;
		fullScreenDisplayMode.ScanLineOrdering = ::D3DSCANLINEORDERING_PROGRESSIVE;
		pFullscreenDisplayMode = &fullScreenDisplayMode;
	}
	LogDebug("Overridden Presentation Params\n");
	if (pPresentationParameters)
	{
		LogDebug("  BackBufferWidth = %d\n", pPresentationParameters->BackBufferWidth);
		LogDebug("  BackBufferHeight = %d\n", pPresentationParameters->BackBufferHeight);
		LogDebug("  BackBufferFormat = %d\n", pPresentationParameters->BackBufferFormat);
		LogDebug("  BackBufferCount = %d\n", pPresentationParameters->BackBufferCount);
		LogDebug("  SwapEffect = %x\n", pPresentationParameters->SwapEffect);
		LogDebug("  Flags = %x\n", pPresentationParameters->Flags);
		LogDebug("  FullScreen_RefreshRateInHz = %d\n", pPresentationParameters->FullScreen_RefreshRateInHz);
		LogDebug("  PresentationInterval = %d\n", pPresentationParameters->PresentationInterval);
		LogDebug("  Windowed = %d\n", pPresentationParameters->Windowed);
		LogDebug("  EnableAutoDepthStencil = %d\n", pPresentationParameters->EnableAutoDepthStencil);
		LogDebug("  AutoDepthStencilFormat = %d\n", pPresentationParameters->AutoDepthStencilFormat);
		LogInfo("  MultiSampleType %d\n", pPresentationParameters->MultiSampleType);
		LogInfo("  MultiSampleQuality %d\n", pPresentationParameters->MultiSampleQuality);
	}
	if (pFullscreenDisplayMode)
	{
		LogDebug("  FullscreenDisplayFormat = %d\n", pFullscreenDisplayMode->Format);
		LogDebug("  FullscreenDisplayWidth = %d\n", pFullscreenDisplayMode->Width);
		LogDebug("  FullscreenDisplayHeight = %d\n", pFullscreenDisplayMode->Height);
		LogDebug("  FullscreenRefreshRate = %d\n", pFullscreenDisplayMode->RefreshRate);
		LogDebug("  ScanLineOrdering = %d\n", pFullscreenDisplayMode->ScanLineOrdering);
	}
	UnbindResources();
	HRESULT hr = GetD3D9DeviceEx()->ResetEx(pPresentationParameters, pFullscreenDisplayMode);
	this->_pOrigPresentationParameters = originalPresentParams;
	this->_pPresentationParameters = *pPresentationParameters;
	this->OnCreateOrRestore(&originalPresentParams, pPresentationParameters);
	pPresentationParameters->BackBufferWidth = originalPresentParams.BackBufferWidth;
	pPresentationParameters->BackBufferHeight = originalPresentParams.BackBufferHeight;
	LogInfo("  returns result=%x\n", hr);
	return hr;
}

UINT FrameIndex = 0;
STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::Present(THIS_ CONST RECT* pSourceRect,CONST RECT* pDestRect,HWND hDestWindowOverride,CONST RGNDATA* pDirtyRegion)
{
	LogDebug("IDirect3DDevice9::Present called.\n");

	LogDebug("HackerDevice::Present(%s@%p) called\n", type_name_dx9((IUnknown*)this), this);
	LogDebug("  pSourceRect = %p\n", pSourceRect);
	LogDebug("  pDestRect = %p\n", pDestRect);
	LogDebug("  hDestWindowOverride = %p\n", hDestWindowOverride);
	LogDebug("  pDirtyRegion = %p\n", pDirtyRegion);
	CheckDevice(this);
	Profiling::State profiling_state = { 0 };
	bool profiling = false;
	if (G->SCREEN_FULLSCREEN == 2)
	{
		if (!gLogDebug) LogInfo("IDirect3DDevice9::Present called.\n");
		LogInfo("  initiating reset to switch to full screen.\n");

		G->SCREEN_FULLSCREEN = 1;
		G->SCREEN_WIDTH = G->SCREEN_WIDTH_DELAY;
		G->SCREEN_HEIGHT = G->SCREEN_HEIGHT_DELAY;
		G->SCREEN_REFRESH = G->SCREEN_REFRESH_DELAY;
		return D3DERR_DEVICELOST;
	}
	// Profiling::mode may change below, so make a copy
	profiling = Profiling::mode == Profiling::Mode::SUMMARY;
	if (profiling)
		Profiling::start(&profiling_state);

	// Every presented frame, we want to take some CPU time to run our actions,
	// which enables hunting, and snapshots, and aiming overrides and other inputs
	CachedStereoValues prePresentCachedStereoValues;
	RunFrameActions(this, &prePresentCachedStereoValues);

	if (profiling)
		Profiling::end(&profiling_state, &Profiling::present_overhead);

	HRESULT hr = GetD3D9Device()->Present(pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);

	if (profiling)
		Profiling::start(&profiling_state);

	if (G->gTrackNvAPIStereoActive && !G->gTrackNvAPIStereoActiveDisableReset)
		NvAPIResetStereoActiveTracking();
	if (G->gTrackNvAPIConvergence && !G->gTrackNvAPIConvergenceDisableReset)
		NvAPIResetConvergenceTracking();
	if (G->gTrackNvAPISeparation && !G->gTrackNvAPISeparationDisableReset)
		NvAPIResetSeparationTracking();
	if (G->gTrackNvAPIEyeSeparation && !G->gTrackNvAPIEyeSeparationDisableReset)
		NvAPIResetEyeSeparationTracking();
	// Update the stereo params texture just after the present so that
	// shaders get the new values for the current frame:
	this->stereo_params_updated_this_frame = false;
	CachedStereoValues postPresentCachedStereoValues;
	this->UpdateStereoParams(false, &postPresentCachedStereoValues);
	G->bb_is_upscaling_bb = !!G->SCREEN_UPSCALING && G->upscaling_command_list_using_explicit_bb_flip;

	// Run the post present command list now, which can be used to restore
	// state changed in the pre-present command list, or to perform some
	// action at the start of a frame:
	GetD3D9Device()->BeginScene();
	RunCommandList(this, &G->post_present_command_list, NULL, true, &postPresentCachedStereoValues);
	GetD3D9Device()->EndScene();
	if (G->gAutoDetectDepthBuffer) {
		DetectDepthSource();
		drawCalls = vertices = 0;
	}
	if (profiling)
		Profiling::end(&profiling_state, &Profiling::present_overhead);

	if (hr == D3DERR_DEVICELOST)
	{
		LogInfo("  returns D3DERR_DEVICELOST\n");
	}
	else
	{
		LogDebug("  returns result=%x\n", hr);
	}
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::GetBackBuffer(THIS_ UINT iSwapChain,UINT iBackBuffer,::D3DBACKBUFFER_TYPE Type, D3D9Wrapper::IDirect3DSurface9 **ppBackBuffer)
{
	LogInfo("IDirect3DDevice9::GetBackBuffer called\n");

	CheckDevice(this);
	HRESULT hr;

	::LPDIRECT3DSURFACE9 baseSurface = 0;
	FakeSwapChain *fakeSwapChain = NULL;
	D3D9Wrapper::IDirect3DSurface9 * wrappedSurface = NULL;
	if (G->SCREEN_UPSCALING > 0 || G->gForceStereo == 2) {
		LogDebug("HackerFakeDeviceSwapChain::GetBuffer(%s@%p)\n", type_name_dx9((IUnknown*)this), this);
		fakeSwapChain = &mFakeSwapChains[iSwapChain];
		if (fakeSwapChain && fakeSwapChain->mFakeBackBuffers.size() > iBackBuffer){
			wrappedSurface = fakeSwapChain->mFakeBackBuffers.at(iBackBuffer);
			wrappedSurface->AddRef();
			hr = S_OK;
		}
		else {
			hr = D3DERR_INVALIDCALL;
			LogInfo("BUG: HackerFakeSwapChain::GetBuffer(): Missing fake back buffer\n");
			if (ppBackBuffer) *ppBackBuffer = 0;
			return hr;
		}
		if (ppBackBuffer) {
			if (!(G->enable_hooks >= EnableHooksDX9::ALL)) {
				*ppBackBuffer = wrappedSurface;
			}
			else {
				*ppBackBuffer = reinterpret_cast<D3D9Wrapper::IDirect3DSurface9*>(wrappedSurface->GetRealOrig());
			}
		}
	}
	else {
		D3D9Wrapper::IDirect3DSwapChain9 *wrappedSwapChain = NULL;
		wrappedSwapChain = mWrappedSwapChains[iSwapChain];
		if (wrappedSwapChain) {
			wrappedSurface = mWrappedSwapChains[iSwapChain]->m_backBuffers[iBackBuffer];
			if (wrappedSurface) {
				wrappedSurface->AddRef();
				hr = S_OK;
			}
			else {
				hr = D3DERR_INVALIDCALL;
				LogInfo("  failed with hr=%x\n", hr);
				if (ppBackBuffer) *ppBackBuffer = 0;
				return hr;
			}
		}else{
			hr = D3DERR_INVALIDCALL;
			LogInfo("  failed with hr=%x\n", hr);
			if (ppBackBuffer) *ppBackBuffer = 0;
			return hr;
		}
		if (ppBackBuffer) {
			if (!(G->enable_hooks >= EnableHooksDX9::ALL)) {
				*ppBackBuffer = wrappedSurface;
			}
			else {
				*ppBackBuffer = reinterpret_cast<D3D9Wrapper::IDirect3DSurface9*>(wrappedSurface->GetRealOrig());
			}
		}
	}

	if (ppBackBuffer) LogInfo("  returns result=%x, handle=%p, wrapper=%p\n", hr, baseSurface, *ppBackBuffer);
    return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::GetRasterStatus(THIS_ UINT iSwapChain,::D3DRASTER_STATUS* pRasterStatus)
{
	LogDebug("IDirect3DDevice9::GetRasterStatus called.\n");

	CheckDevice(this);
	HRESULT hr = GetD3D9Device()->GetRasterStatus(iSwapChain, pRasterStatus);
	LogInfo("  returns result=%x\n", hr);

	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::SetDialogBoxMode(THIS_ BOOL bEnableDialogs)
{
	LogDebug("IDirect3DDevice9::SetDialogBoxMode called.\n");

	CheckDevice(this);
	HRESULT hr = GetD3D9Device()->SetDialogBoxMode(bEnableDialogs);
	LogInfo("  returns result=%x\n", hr);

	return hr;
}

STDMETHODIMP_(void) D3D9Wrapper::IDirect3DDevice9::SetGammaRamp(THIS_ UINT iSwapChain,DWORD Flags,CONST ::D3DGAMMARAMP* pRamp)
{
	LogDebug("IDirect3DDevice9::SetGammaRamp called.\n");

	CheckDevice(this);
	GetD3D9Device()->SetGammaRamp(iSwapChain, Flags, pRamp);
}

STDMETHODIMP_(void) D3D9Wrapper::IDirect3DDevice9::GetGammaRamp(THIS_ UINT iSwapChain,::D3DGAMMARAMP* pRamp)
{
	LogDebug("IDirect3DDevice9::GetGammaRamp called.\n");

	CheckDevice(this);
	GetD3D9Device()->GetGammaRamp(iSwapChain, pRamp);
}

enum SurfaceCreation {
	Texture,
	CubeTexture,
	VolumeTexture,
	RenderTarget,
	RenderTargetEx,
	DepthStencil,
	DepthStencilEx,
	OffscreenPlain,
	OffscreenPlainEx
};

static HRESULT _CreateSurface(::IDirect3DTexture9 **baseSurface, const D3D2DTEXTURE_DESC *pDesc, D3D9Wrapper::IDirect3DDevice9 *me, HANDLE *pSharedHandle, SurfaceCreation type) {
	return me->GetD3D9Device()->CreateTexture(pDesc->Width, pDesc->Height, pDesc->Levels, pDesc->Usage, pDesc->Format, pDesc->Pool, baseSurface, pSharedHandle);
}

static HRESULT _CreateSurface(::IDirect3DVolumeTexture9 **baseSurface, const D3D3DTEXTURE_DESC *pDesc, D3D9Wrapper::IDirect3DDevice9 *me, HANDLE *pSharedHandle, SurfaceCreation type) {
	return me->GetD3D9Device()->CreateVolumeTexture(pDesc->Width, pDesc->Height, pDesc->Depth, pDesc->Levels, pDesc->Usage, pDesc->Format, pDesc->Pool, baseSurface, pSharedHandle);
}

static HRESULT _CreateSurface(::IDirect3DCubeTexture9 **baseSurface, const D3D2DTEXTURE_DESC *pDesc, D3D9Wrapper::IDirect3DDevice9 *me, HANDLE *pSharedHandle, SurfaceCreation type) {
	return me->GetD3D9Device()->CreateCubeTexture(pDesc->Width, pDesc->Levels, pDesc->Usage, pDesc->Format, pDesc->Pool, baseSurface, pSharedHandle);
}

static HRESULT _CreateSurface(::IDirect3DSurface9 **baseSurface, const D3D2DTEXTURE_DESC *pDesc, D3D9Wrapper::IDirect3DDevice9 *me, HANDLE *pSharedHandle, SurfaceCreation type) {
	switch (type) {
	case SurfaceCreation::RenderTarget:
		return me->GetD3D9Device()->CreateRenderTarget(pDesc->Width, pDesc->Height, pDesc->Format, pDesc->MultiSampleType, pDesc->MultiSampleQuality, (pDesc->Usage & D3DUSAGE_DYNAMIC), baseSurface, pSharedHandle);
	case SurfaceCreation::DepthStencil:
		return me->GetD3D9Device()->CreateDepthStencilSurface(pDesc->Width, pDesc->Height, pDesc->Format, pDesc->MultiSampleType, pDesc->MultiSampleQuality, (pDesc->Usage & D3DUSAGE_DYNAMIC), baseSurface, pSharedHandle);
	case SurfaceCreation::OffscreenPlain:
		return me->GetD3D9Device()->CreateOffscreenPlainSurface(pDesc->Width, pDesc->Height, pDesc->Format, pDesc->Pool, baseSurface, pSharedHandle);
	case SurfaceCreation::RenderTargetEx:
		return me->GetD3D9DeviceEx()->CreateRenderTargetEx(pDesc->Width, pDesc->Height, pDesc->Format, pDesc->MultiSampleType, pDesc->MultiSampleQuality, (pDesc->Usage & D3DUSAGE_DYNAMIC), baseSurface, pSharedHandle, pDesc->Usage);
	case SurfaceCreation::DepthStencilEx:
		return me->GetD3D9DeviceEx()->CreateDepthStencilSurfaceEx(pDesc->Width, pDesc->Height, pDesc->Format, pDesc->MultiSampleType, pDesc->MultiSampleQuality, (pDesc->Usage & D3DUSAGE_DYNAMIC), baseSurface, pSharedHandle, pDesc->Usage);
	case SurfaceCreation::OffscreenPlainEx:
		return me->GetD3D9DeviceEx()->CreateOffscreenPlainSurfaceEx(pDesc->Width, pDesc->Height, pDesc->Format, pDesc->Pool, baseSurface, pSharedHandle, pDesc->Usage);
	default:
		return E_NOTIMPL;
	}
}

template <typename Desc>
static void check_surface_resolution(const Desc *pDesc) {
	// Rectangular depth stencil textures of at least 640x480 may indicate
	// the game's resolution, for games that upscale to their swap chains:
	if (pDesc &&
		(pDesc->Usage & D3DUSAGE_DEPTHSTENCIL) &&
		G->mResolutionInfo.from == GetResolutionFrom::DEPTH_STENCIL &&
		heuristic_could_be_possible_resolution(pDesc))
	{
		G->mResolutionInfo.width = pDesc->Width;
		G->mResolutionInfo.height = pDesc->Height;
		LogInfo("Got resolution from depth/stencil buffer: %ix%i\n",
			G->mResolutionInfo.width, G->mResolutionInfo.height);
	}
}

template <typename Desc>
static UINT get_surface_width(const Desc *pDesc) {
	return pDesc->Width;

}

template <typename Desc>
static void set_surface_width_direct_mode(const Desc *pDesc) {
	const_cast<Desc *>(pDesc)->Width *= 2;
}


template <class Surface, typename Desc>
static HRESULT CreateSurface(Surface **baseSurface, ResourceHandleInfo *handle_info, const Desc *pDesc, D3D9Wrapper::IDirect3DDevice9 *me, HANDLE *pSharedHandle, SurfaceCreation type)
{
	Desc newDesc;
	const Desc *pNewDesc = NULL;
	NVAPI_STEREO_SURFACECREATEMODE oldMode;

	LogDebug("HackerDevice::CreateSurface called with parameters\n");
	if (pDesc)
		LogDebugResourceDesc(pDesc);
	uint32_t hash = 0;
	if (pDesc)
		hash = CalcDescHash(hash, pDesc);
	LogDebug("  hash = %08lx\n", hash);

	// Override custom settings?
	pNewDesc = process_texture_override<Desc>(hash, me->mStereoHandle, pDesc, &newDesc, &oldMode);

	// Actual creation:
	HRESULT hr = _CreateSurface(baseSurface, pNewDesc, me, pSharedHandle, type);

	restore_old_surface_create_mode(oldMode, me->mStereoHandle);
	if (baseSurface) LogDebug("  returns result = %x, handle = %p\n", hr, *baseSurface);

	// Register texture. Every one seen.
	if (hr == S_OK && baseSurface && *baseSurface)
	{
		handle_info->type = pDesc->Type;
		handle_info->hash = hash;
		handle_info->orig_hash = hash;
		if (pDesc)
			copy_surface_desc_to_handle(handle_info, pDesc);
		if (G->hunting && pDesc) {
			EnterCriticalSection(&G->mCriticalSection);
			G->mResourceInfo[hash] = *pDesc;
			LeaveCriticalSection(&G->mCriticalSection);
		}
	}
	return hr;
}

static void copy_surface_desc_to_handle(ResourceHandleInfo *handle_info, const D3D2DTEXTURE_DESC *pDesc) {
	memcpy(&handle_info->desc2D, pDesc, sizeof(D3D2DTEXTURE_DESC));
}
static void copy_surface_desc_to_handle(ResourceHandleInfo *handle_info, const D3D3DTEXTURE_DESC *pDesc) {
	memcpy(&handle_info->desc3D, pDesc, sizeof(D3D3DTEXTURE_DESC));
}
template <typename Desc>
static bool heuristic_could_be_possible_resolution(Desc *pDesc) {

	if (pDesc->Width < 640 || pDesc->Height < 480)
		return false;

	// Assume square textures are not a resolution, like 3D Vision:
	if (pDesc->Width == pDesc->Height)
		return false;

	// Special case for WATCH_DOGS2 1.09.154 update, which creates 16384 x 4096
	// shadow maps on ultra that are mistaken for the resolution. I don't
	// think that 4 is ever a valid aspect radio, so exclude it:
	if (pDesc->Width == pDesc->Height * 4)
		return false;

	return true;
}
static void AdjustForConstResolution(UINT *hashWidth, UINT *hashHeight)
{
	int width = *hashWidth;
	int height = *hashHeight;

	if (G->mResolutionInfo.from == GetResolutionFrom::INVALID)
		return;

	if (width == G->mResolutionInfo.width && height == G->mResolutionInfo.height) {
		*hashWidth = 'SRES';
		*hashHeight = 'SRES';
	}
	else if (width == G->mResolutionInfo.width * 2 && height == G->mResolutionInfo.height * 2) {
		*hashWidth = 'SR*2';
		*hashHeight = 'SR*2';
	}
	else if (width == G->mResolutionInfo.width * 4 && height == G->mResolutionInfo.height * 4) {
		*hashWidth = 'SR*4';
		*hashHeight = 'SR*4';
	}
	else if (width == G->mResolutionInfo.width * 8 && height == G->mResolutionInfo.height * 8) {
		*hashWidth = 'SR*8';
		*hashHeight = 'SR*8';
	}
	else if (width == G->mResolutionInfo.width / 2 && height == G->mResolutionInfo.height / 2) {
		*hashWidth = 'SR/2';
		*hashHeight = 'SR/2';
	}
}
static void restore_old_surface_create_mode(NVAPI_STEREO_SURFACECREATEMODE oldMode, StereoHandle mStereoHandle)
{
	if (oldMode == (NVAPI_STEREO_SURFACECREATEMODE) - 1)
		return;

	if (NVAPI_OK != Profiling::NvAPI_Stereo_SetSurfaceCreationMode(mStereoHandle, oldMode))
		LogInfo("    restore call failed.\n");
}

template <typename DescType>
static const DescType* process_texture_override(uint32_t hash,
	StereoHandle mStereoHandle,
	const DescType *origDesc,
	DescType *newDesc,
	NVAPI_STEREO_SURFACECREATEMODE *oldMode)
{
	NVAPI_STEREO_SURFACECREATEMODE newMode = (NVAPI_STEREO_SURFACECREATEMODE) - 1;
	TextureOverrideMatches matches;
	TextureOverride *textureOverride = NULL;
	const DescType* ret = origDesc;
	unsigned i;

	*oldMode = (NVAPI_STEREO_SURFACECREATEMODE) - 1;

	// Check for square surfaces. We used to do this after processing the
	// StereoMode in TextureOverrides, but realistically we always want the
	// TextureOverrides to be able to override this since they are more
	// specific, so now we do this first.
	if (G->gForceStereo != 2 && is_square_surface(origDesc))
		newMode = (NVAPI_STEREO_SURFACECREATEMODE)G->gSurfaceSquareCreateMode;

	find_texture_overrides(hash, origDesc, &matches, NULL);

	if (origDesc && !matches.empty()) {
		// There is at least one matching texture override, which means
		// we may possibly be altering the resource description. Make a
		// copy of it and adjust the return pointer to the copy:
		*newDesc = *origDesc;
		ret = newDesc;

		// We go through each matching texture override applying any
		// resource description and stereo mode overrides. The texture
		// overrides with higher priorities come later in the list, so
		// if there are any conflicts they will override the earlier
		// lower priority ones.
		for (i = 0; i < matches.size(); i++) {
			textureOverride = matches[i];

			LogInfo("  %S matched resource with hash=%08x\n", textureOverride->ini_section.c_str(), hash);

			if (!check_texture_override_iteration(textureOverride))
				continue;

			if (G->gForceStereo != 2 && textureOverride->stereoMode != -1)
				newMode = (NVAPI_STEREO_SURFACECREATEMODE)textureOverride->stereoMode;

			override_resource_desc(newDesc, textureOverride);
		}
	}

	if (G->gForceStereo != 2 && newMode != (NVAPI_STEREO_SURFACECREATEMODE) - 1) {
		Profiling::NvAPI_Stereo_GetSurfaceCreationMode(mStereoHandle, oldMode);
		NvAPIOverride();
		LogInfo("  setting custom surface creation mode.\n");

		if (NVAPI_OK != Profiling::NvAPI_Stereo_SetSurfaceCreationMode(mStereoHandle, newMode))
			LogInfo("    call failed.\n");
	}

	return ret;
}

static bool check_texture_override_iteration(TextureOverride *textureOverride)
{
	if (textureOverride->iterations.empty())
		return true;

	std::vector<int>::iterator k = textureOverride->iterations.begin();
	int currentIteration = textureOverride->iterations[0] = textureOverride->iterations[0] + 1;
	LogInfo("  current iteration = %d\n", currentIteration);

	while (++k != textureOverride->iterations.end()) {
		if (currentIteration == *k)
			return true;
	}

	LogInfo("  override skipped\n");
	return false;
}
template <typename DescType>
static void override_resource_desc_common_2d_3d(DescType *desc, TextureOverride *textureOverride)
{
	if (textureOverride->format != -1) {
		LogInfo("  setting custom format to %d\n", textureOverride->format);
		desc->Format = (::D3DFORMAT)textureOverride->format;
	}

	if (textureOverride->width != -1) {
		LogInfo("  setting custom width to %d\n", textureOverride->width);
		desc->Width = textureOverride->width;
	}

	if (textureOverride->width_multiply != 1.0f) {
		desc->Width = (UINT)(desc->Width * textureOverride->width_multiply);
		LogInfo("  multiplying custom width by %f to %d\n", textureOverride->width_multiply, desc->Width);
	}

	if (textureOverride->height != -1) {
		LogInfo("  setting custom height to %d\n", textureOverride->height);
		desc->Height = textureOverride->height;
	}

	if (textureOverride->height_multiply != 1.0f) {
		desc->Height = (UINT)(desc->Height * textureOverride->height_multiply);
		LogInfo("  multiplying custom height by %f to %d\n", textureOverride->height_multiply, desc->Height);
	}
}

static void override_resource_desc(::D3DVERTEXBUFFER_DESC *desc, TextureOverride *textureOverride) {}
static void override_resource_desc(::D3DINDEXBUFFER_DESC *desc, TextureOverride *textureOverride) {}
static void override_resource_desc(D3D2DTEXTURE_DESC *desc, TextureOverride *textureOverride)
{
	override_resource_desc_common_2d_3d(desc, textureOverride);
}
static void override_resource_desc(D3D3DTEXTURE_DESC *desc, TextureOverride *textureOverride)
{
	override_resource_desc_common_2d_3d(desc, textureOverride);
}
bool D3D9Wrapper::IDirect3DDevice9::HackerDeviceShouldDuplicateSurface(D3D2DTEXTURE_DESC * pDesc)
{
	return ShouldDuplicate(pDesc);
}
STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::CreateTexture(THIS_ UINT Width,UINT Height,UINT Levels,DWORD Usage,::D3DFORMAT Format,::D3DPOOL Pool, D3D9Wrapper::IDirect3DTexture9 **ppTexture,HANDLE* pSharedHandle)
{
	LogDebug("IDirect3DDevice9::CreateTexture called with Width=%d, Height=%d, Levels=%d, Usage=%x, Format=%d, Pool=%d, SharedHandle=%p\n",
		Width, Height, Levels, Usage, Format, Pool, pSharedHandle);

	if (!GetD3D9Device())
	{
		LogInfo("  postponing call because device was not created yet.\n");
		D3D9Wrapper::IDirect3DTexture9 *wrapper = IDirect3DTexture9::GetDirect3DTexture9((::LPDIRECT3DTEXTURE9) 0, this, NULL);
		wrapper->_Width = Width;
		wrapper->_Height = Height;
		wrapper->_Levels = Levels;
		wrapper->_Usage = Usage;
		wrapper->_Format = Format;
		wrapper->_Pool = Pool;
		wrapper->_Device = this;
		wrapper->pendingCreateTexture = true;
		*ppTexture = wrapper;
		LogInfo("  returns handle=%p\n", wrapper);

		return S_OK;
	}

	::LPDIRECT3DTEXTURE9 baseTexture = 0;
	if (G->gForwardToEx && Pool == ::D3DPOOL_MANAGED)
	{
		LogDebug("  Pool changed from MANAGED to DEFAULT because of DirectX9Ex migration.\n");

		Pool = ::D3DPOOL_DEFAULT;
		if (!(Usage & D3DUSAGE_DYNAMIC))
			Usage = Usage | D3DUSAGE_DYNAMIC;
	}
	D3D2DTEXTURE_DESC pDesc;// = {};
	pDesc.Width = Width;
	pDesc.Height = Height;
	pDesc.Levels = Levels;
	pDesc.Usage = Usage;
	pDesc.Format = Format;
	pDesc.Pool = Pool;
	HRESULT hr;
	D3D9Wrapper::IDirect3DTexture9 *wrapper;
	ResourceHandleInfo info;
	if (G->gForceStereo == 2) {
		::IDirect3DTexture9* pRightTexture = NULL;
		hr = CreateSurface<::IDirect3DTexture9, D3D2DTEXTURE_DESC>(&baseTexture, &info, &pDesc, this, pSharedHandle, SurfaceCreation::Texture);
		if (!FAILED(hr)){
			if (ShouldDuplicate(&pDesc)) {
				hr = CreateSurface<::IDirect3DTexture9, D3D2DTEXTURE_DESC>(&pRightTexture, &info, &pDesc, this, pSharedHandle, SurfaceCreation::Texture);
				if (!FAILED(hr)) {
					wrapper = IDirect3DTexture9::GetDirect3DTexture9(baseTexture, this, pRightTexture);
				}
				else {
					LogDebug("IDirect3DDevice9::CreateTexture Direct Mode, failed to create right texture, falling back to mono, hr = %d", hr);
						wrapper = IDirect3DTexture9::GetDirect3DTexture9(baseTexture, this, NULL);
				}
			}else{
				LogDebug("IDirect3DDevice9::CreateTexture Direct Mode, non stereo texture");
				wrapper = IDirect3DTexture9::GetDirect3DTexture9(baseTexture, this, NULL);
			}
		}
		else {
			LogDebug("IDirect3DDevice9::CreateTexture Direct Mode, failed to create left surface, hr = %d ", hr);
		}
	}
	else {
		check_surface_resolution(&pDesc);
		hr = CreateSurface<::IDirect3DTexture9, D3D2DTEXTURE_DESC>(&baseTexture, &info, &pDesc, this, pSharedHandle, SurfaceCreation::Texture);
		wrapper = IDirect3DTexture9::GetDirect3DTexture9(baseTexture, this, NULL);
	}

	if (ppTexture) {
		if (!(G->enable_hooks >= EnableHooksDX9::ALL)) {
			*ppTexture = wrapper;
		}
		else {
			*ppTexture = reinterpret_cast<D3D9Wrapper::IDirect3DTexture9*>(baseTexture);
		}
	}
	if (ppTexture) LogDebug("  returns result=%x, handle=%p, wrapper=%p\n", hr, baseTexture, *ppTexture);

	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::CreateVolumeTexture(THIS_ UINT Width,UINT Height,UINT Depth,UINT Levels,DWORD Usage,::D3DFORMAT Format,::D3DPOOL Pool, D3D9Wrapper::IDirect3DVolumeTexture9** ppVolumeTexture,HANDLE* pSharedHandle)
{
	LogInfo("IDirect3DDevice9::CreateVolumeTexture Width=%d Height=%d Format=%d\n", Width, Height, Format);
	if (!GetD3D9Device())
	{
		LogInfo("  postponing call because device was not created yet.\n");

		D3D9Wrapper::IDirect3DVolumeTexture9 *wrapper = IDirect3DVolumeTexture9::GetDirect3DVolumeTexture9((::LPDIRECT3DVOLUMETEXTURE9) 0, this);
		wrapper->_Height = Height;
		wrapper->_Width = Width;
		wrapper->_Depth = Depth;
		wrapper->_Levels = Levels;
		wrapper->_Usage = Usage;
		wrapper->_Format = Format;
		wrapper->_Pool = Pool;
		wrapper->_Device = this;
		wrapper->pendingCreateTexture = true;
		*ppVolumeTexture = wrapper;
		LogInfo("  returns handle=%p\n", wrapper);

		return S_OK;
	}

	if (G->gForwardToEx && Pool == ::D3DPOOL_MANAGED)
	{
		LogDebug("  Pool changed from MANAGED to DEFAULT because of DirectX9Ex migration.\n");

		Pool = ::D3DPOOL_DEFAULT;
		if (!(Usage & D3DUSAGE_DYNAMIC))
			Usage = Usage | D3DUSAGE_DYNAMIC;
	}

	D3D3DTEXTURE_DESC pDesc;// = {};
	pDesc.Width = Width;
	pDesc.Height = Height;
	pDesc.Depth = Depth;
	pDesc.Levels = Levels;
	pDesc.Usage = Usage;
	pDesc.Format = Format;
	pDesc.Pool = Pool;
	check_surface_resolution(&pDesc);
	::LPDIRECT3DVOLUMETEXTURE9 baseTexture = 0;
	ResourceHandleInfo info;
	HRESULT hr = CreateSurface<::IDirect3DVolumeTexture9, D3D3DTEXTURE_DESC>(&baseTexture, &info, &pDesc, this, pSharedHandle, SurfaceCreation::VolumeTexture);
	if (baseTexture) {
		D3D9Wrapper::IDirect3DVolumeTexture9 *wrapper;
		wrapper = IDirect3DVolumeTexture9::GetDirect3DVolumeTexture9(baseTexture, this);
		wrapper->resourceHandleInfo = info;
		if (ppVolumeTexture) {
			if (!(G->enable_hooks >= EnableHooksDX9::ALL)) {
				*ppVolumeTexture = wrapper;
			}
			else {
				*ppVolumeTexture = reinterpret_cast<D3D9Wrapper::IDirect3DVolumeTexture9*>(baseTexture);
			}
		}

	}

	if (ppVolumeTexture) LogDebug("  returns result=%x, handle=%p, wrapper=%p\n", hr, baseTexture, *ppVolumeTexture);
	LogInfo("  returns result=%x\n", hr);

	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::CreateCubeTexture(THIS_ UINT EdgeLength,UINT Levels,DWORD Usage,::D3DFORMAT Format,::D3DPOOL Pool, D3D9Wrapper::IDirect3DCubeTexture9** ppCubeTexture,HANDLE* pSharedHandle)
{
	LogInfo("IDirect3DDevice9::CreateCubeTexture EdgeLength=%d Format=%d\n", EdgeLength, Format);
	if (!GetD3D9Device())
	{
		LogInfo("  postponing call because device was not created yet.\n");

		D3D9Wrapper::IDirect3DCubeTexture9 *wrapper = IDirect3DCubeTexture9::GetDirect3DCubeTexture9((::LPDIRECT3DCUBETEXTURE9) 0, this, NULL);
		wrapper->_EdgeLength = EdgeLength;
		wrapper->_Levels = Levels;
		wrapper->_Usage = Usage;
		wrapper->_Format = Format;
		wrapper->_Pool = Pool;
		wrapper->_Device = this;
		wrapper->pendingCreateTexture = true;
		*ppCubeTexture = wrapper;
		LogInfo("  returns handle=%p\n", wrapper);

		return S_OK;
	}

	if (G->gForwardToEx && Pool == ::D3DPOOL_MANAGED)
	{
		LogDebug("  Pool changed from MANAGED to DEFAULT because of DirectX9Ex migration.\n");

		Pool = ::D3DPOOL_DEFAULT;
		if (!(Usage & D3DUSAGE_DYNAMIC))
			Usage = Usage | D3DUSAGE_DYNAMIC;
	}

	D3D2DTEXTURE_DESC pDesc;
	pDesc.Width = EdgeLength;
	pDesc.Height = EdgeLength;
	pDesc.Levels = Levels;
	pDesc.Usage = Usage;
	pDesc.Format = Format;
	pDesc.Pool = Pool;
	::LPDIRECT3DCUBETEXTURE9 baseTexture = 0;
	HRESULT hr;
	D3D9Wrapper::IDirect3DCubeTexture9 *wrapper = NULL;
	ResourceHandleInfo info;
	if (G->gForceStereo == 2) {
		::IDirect3DCubeTexture9* pRightTexture = NULL;
		hr = CreateSurface<::IDirect3DCubeTexture9, D3D2DTEXTURE_DESC>(&baseTexture, &info, &pDesc, this, pSharedHandle, SurfaceCreation::CubeTexture);
		if (!FAILED(hr)){
			if (ShouldDuplicate(&pDesc)) {
				hr = CreateSurface<::IDirect3DCubeTexture9, D3D2DTEXTURE_DESC>(&pRightTexture, &info, &pDesc, this, pSharedHandle, SurfaceCreation::CubeTexture);
				if (!FAILED(hr)) {
					wrapper = IDirect3DCubeTexture9::GetDirect3DCubeTexture9(baseTexture, this, pRightTexture);
					wrapper->resourceHandleInfo = info;
				}
				else {
					LogDebug("IDirect3DDevice9::CreateCubeTexture Direct Mode, failed to create right texture, falling back to mono, hr = %d", hr);
					wrapper = IDirect3DCubeTexture9::GetDirect3DCubeTexture9(baseTexture, this, NULL);
					wrapper->resourceHandleInfo = info;
				}
			}
			else {
				LogDebug("IDirect3DDevice9::CreateCubeTexture Direct Mode, non stereo cube texture");
				wrapper = IDirect3DCubeTexture9::GetDirect3DCubeTexture9(baseTexture, this, NULL);
				wrapper->resourceHandleInfo = info;
			}
		}
		else {
			LogDebug("IDirect3DDevice9::CreateCubeTexture Direct Mode, failed to create left surface, hr = %d ", hr);
		}
	}
	else {
		check_surface_resolution(&pDesc);
		hr = CreateSurface<::IDirect3DCubeTexture9, D3D2DTEXTURE_DESC>(&baseTexture, &info, &pDesc, this, pSharedHandle, SurfaceCreation::CubeTexture);
		wrapper = IDirect3DCubeTexture9::GetDirect3DCubeTexture9(baseTexture, this, NULL);
		wrapper->resourceHandleInfo = info;
	}

	if (ppCubeTexture) {
		if (!(G->enable_hooks >= EnableHooksDX9::ALL)) {
			*ppCubeTexture = wrapper;
		}
		else {
			*ppCubeTexture = reinterpret_cast<D3D9Wrapper::IDirect3DCubeTexture9*>(baseTexture);
		}
	}

	if (ppCubeTexture) LogDebug("  returns result=%x, handle=%p, wrapper=%p\n", hr, baseTexture, *ppCubeTexture);
	LogInfo("  returns result=%x\n", hr);

	return hr;
}


static HRESULT _CreateBuffer(::IDirect3DVertexBuffer9 **baseBuffer, const ::D3DVERTEXBUFFER_DESC *pDesc, D3D9Wrapper::IDirect3DDevice9* me, HANDLE *pSharedHandle) {

	return me->GetD3D9Device()->CreateVertexBuffer(pDesc->Size, pDesc->Usage, pDesc->FVF, pDesc->Pool, baseBuffer, pSharedHandle);
}

static HRESULT _CreateBuffer(::IDirect3DIndexBuffer9 **baseBuffer, const ::D3DINDEXBUFFER_DESC *pDesc, D3D9Wrapper::IDirect3DDevice9* me, HANDLE *pSharedHandle) {

	return me->GetD3D9Device()->CreateIndexBuffer(pDesc->Size, pDesc->Usage, pDesc->Format, pDesc->Pool, baseBuffer, pSharedHandle);
}

template <typename DescType, class BufferType>
static HRESULT CreateBuffer(BufferType** baseBuffer, ResourceHandleInfo *info, const DescType *pDesc, D3D9Wrapper::IDirect3DDevice9* me, HANDLE *pSharedHandle) {

	DescType  newDesc;
	const DescType *pNewDesc = NULL;
	::NVAPI_STEREO_SURFACECREATEMODE oldMode;

	LogDebug("HackerDevice::CreateBuffer called\n");
	if (pDesc)
		LogDebugResourceDesc(pDesc);

	// Create hash from the raw buffer data if available, but also include
	// the pDesc data as a unique fingerprint for a buffer.
	uint32_t data_hash = 0, hash = 0;
	if (pDesc)
		hash = crc32c_hw(hash, pDesc, sizeof(DescType));

	// Override custom settings?
	pNewDesc = process_texture_override<DescType>(hash, me->mStereoHandle, pDesc, &newDesc, &oldMode);

	HRESULT hr = _CreateBuffer(baseBuffer, pNewDesc, me, pSharedHandle);
	restore_old_surface_create_mode(oldMode, me->mStereoHandle);
	if (hr == S_OK && baseBuffer && *baseBuffer)
	{
		info->type = pDesc->Type;
		info->hash = hash;
		info->orig_hash = hash;
		info->data_hash = data_hash;

		// XXX: This is only used for hash tracking, which we
		// don't enable for buffers for performance reasons:
		// if (pDesc)
		//	memcpy(&handle_info->descBuf, pDesc, sizeof(D3D11_BUFFER_DESC));

		// For stat collection and hash contamination tracking:
		if (G->hunting && pDesc) {
			EnterCriticalSection(&G->mCriticalSection);
			G->mResourceInfo[hash] = *pDesc;
			LeaveCriticalSection(&G->mCriticalSection);
		}
	}
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::CreateVertexBuffer(THIS_ UINT Length,DWORD Usage,DWORD FVF,::D3DPOOL Pool, D3D9Wrapper::IDirect3DVertexBuffer9 **ppVertexBuffer, HANDLE* pSharedHandle)
{
	LogDebug("IDirect3DDevice9::CreateVertexBuffer called with Length=%d\n", Length);

	if (!GetD3D9Device())
	{
		LogInfo("  postponing call because device was not created yet.\n");

		D3D9Wrapper::IDirect3DVertexBuffer9 *wrapper = D3D9Wrapper::IDirect3DVertexBuffer9::GetDirect3DVertexBuffer9((::LPDIRECT3DVERTEXBUFFER9) 0, this);
		wrapper->_Length = Length;
		wrapper->_Usage = Usage;
		wrapper->_FVF = FVF;
		wrapper->_Pool = Pool;
		wrapper->_Device = this;
		wrapper->pendingCreateVertexBuffer = true;
		if (ppVertexBuffer) *ppVertexBuffer = wrapper;
		LogInfo("  returns handle=%p\n", wrapper);

		return S_OK;
	}

	::LPDIRECT3DVERTEXBUFFER9 baseVB = 0;
	if (G->gForwardToEx && Pool == ::D3DPOOL_MANAGED)
	{
		LogDebug("  Pool changed from MANAGED to DEFAULT because of DirectX9Ex migration.\n");

		Pool = ::D3DPOOL_DEFAULT;
	}
    HRESULT hr = GetD3D9Device()->CreateVertexBuffer(Length, Usage, FVF, Pool, &baseVB, pSharedHandle);
	if (baseVB) {
		D3D9Wrapper::IDirect3DVertexBuffer9 *wrapper = D3D9Wrapper::IDirect3DVertexBuffer9::GetDirect3DVertexBuffer9(baseVB, this);
		if (ppVertexBuffer) {
			if (!(G->enable_hooks >= EnableHooksDX9::ALL)) {
				*ppVertexBuffer = wrapper;
			}
			else {
				*ppVertexBuffer = reinterpret_cast<D3D9Wrapper::IDirect3DVertexBuffer9*>(baseVB);
			}
		}

	}

	if (ppVertexBuffer) LogDebug("  returns result=%x, handle=%p, wrapper=%p\n", hr, baseVB, *ppVertexBuffer);

    return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::CreateIndexBuffer(THIS_ UINT Length,DWORD Usage,::D3DFORMAT Format,::D3DPOOL Pool,
	D3D9Wrapper::IDirect3DIndexBuffer9 **ppIndexBuffer,HANDLE* pSharedHandle)
{
	LogDebug("IDirect3DDevice9::CreateIndexBuffer called with Length=%d, Usage=%x, Format=%d, Pool=%d, Sharedhandle=%p, IndexBufferPtr=%p\n",
		Length, Usage, Format, Pool, pSharedHandle, ppIndexBuffer);

	if (!GetD3D9Device())
	{
		LogInfo("  postponing call because device was not created yet.\n");
		D3D9Wrapper::IDirect3DIndexBuffer9 *wrapper = D3D9Wrapper::IDirect3DIndexBuffer9::GetDirect3DIndexBuffer9((::LPDIRECT3DINDEXBUFFER9) 0, this);
		wrapper->_Length = Length;
		wrapper->_Usage = Usage;
		wrapper->_Format = Format;
		wrapper->_Pool = Pool;
		wrapper->_Device = this;
		wrapper->pendingCreateIndexBuffer = true;
		if (ppIndexBuffer) *ppIndexBuffer = wrapper;
		LogInfo("  returns handle=%p\n", wrapper);

		return S_OK;
	}

	::LPDIRECT3DINDEXBUFFER9 baseIB = 0;
	if (G->gForwardToEx && Pool == ::D3DPOOL_MANAGED)
	{
		LogDebug("  Pool changed from MANAGED to DEFAULT because of DirectX9Ex migration.\n");

		Pool = ::D3DPOOL_DEFAULT;
	}
    HRESULT hr = GetD3D9Device()->CreateIndexBuffer(Length, Usage, Format, Pool, &baseIB, pSharedHandle);
	if (baseIB) {
		D3D9Wrapper::IDirect3DIndexBuffer9 *wrapper = D3D9Wrapper::IDirect3DIndexBuffer9::GetDirect3DIndexBuffer9(baseIB, this);
		if (ppIndexBuffer) {
			if (!(G->enable_hooks >= EnableHooksDX9::ALL)) {
				*ppIndexBuffer = wrapper;
			}
			else {
				*ppIndexBuffer = reinterpret_cast<D3D9Wrapper::IDirect3DIndexBuffer9*>(baseIB);
			}
		}
	}

	if (ppIndexBuffer) LogDebug("  returns result=%x, handle=%p, wrapper=%p\n", hr, baseIB, *ppIndexBuffer);

    return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::CreateRenderTarget(THIS_ UINT Width,UINT Height,::D3DFORMAT Format,::D3DMULTISAMPLE_TYPE MultiSample,DWORD MultisampleQuality,BOOL Lockable, D3D9Wrapper::IDirect3DSurface9 **ppSurface,HANDLE* pSharedHandle)
{
	LogInfo("IDirect3DDevice9::CreateRenderTarget called with Width=%d Height=%d Format=%d\n", Width, Height, Format);

	CheckDevice(this);
	::LPDIRECT3DSURFACE9 baseSurface = NULL;

	D3D2DTEXTURE_DESC pDesc;
	pDesc.Usage = D3DUSAGE_RENDERTARGET;
	if (Lockable)
		pDesc.Usage |= D3DUSAGE_DYNAMIC;
	pDesc.Format = Format;
	pDesc.Height = Height;
	pDesc.Width = Width;
	pDesc.MultiSampleType = MultiSample;
	pDesc.MultiSampleQuality = MultisampleQuality;
	pDesc.Levels = 1;
	HRESULT hr;
	D3D9Wrapper::IDirect3DSurface9 *wrapper;
	ResourceHandleInfo info;
	if (G->gForceStereo == 2) {
		::IDirect3DSurface9* pRightRenderTarget = NULL;
		// create left/mono
		hr = CreateSurface<::IDirect3DSurface9, D3D2DTEXTURE_DESC>(&baseSurface, &info, &pDesc, this, pSharedHandle, SurfaceCreation::RenderTarget);
		if (!FAILED(hr)){
			if (ShouldDuplicate(&pDesc)) {
				hr = CreateSurface<::IDirect3DSurface9, D3D2DTEXTURE_DESC>(&pRightRenderTarget, &info, &pDesc, this, pSharedHandle, SurfaceCreation::RenderTarget);
				if (!FAILED(hr)) {
					wrapper = IDirect3DSurface9::GetDirect3DSurface9(baseSurface, this, pRightRenderTarget);
					wrapper->resourceHandleInfo = info;
				}
				else {
					LogDebug("IDirect3DDevice9::CreateRenderTarget Direct Mode, failed to create right surface, falling back to mono, hr = %d", hr);
					wrapper = IDirect3DSurface9::GetDirect3DSurface9(baseSurface, this, NULL);
					wrapper->resourceHandleInfo = info;
				}
			}
			else {
				LogDebug("IDirect3DDevice9::CreateRenderTarget Direct Mode, non stereo render target");
				wrapper = IDirect3DSurface9::GetDirect3DSurface9(baseSurface, this, NULL);
				wrapper->resourceHandleInfo = info;
			}
		}
		else {
			LogDebug("IDirect3DDevice9::CreateRenderTarget Direct Mode, failed to create left surface, hr = %d ", hr);
		}
	}
	else {
		hr = CreateSurface<::IDirect3DSurface9, D3D2DTEXTURE_DESC>(&baseSurface, &info, &pDesc, this, pSharedHandle, SurfaceCreation::RenderTarget);
		wrapper = IDirect3DSurface9::GetDirect3DSurface9(baseSurface, this, NULL);
		wrapper->resourceHandleInfo = info;
	}
	if (ppSurface) {
		if (!(G->enable_hooks >= EnableHooksDX9::ALL)) {
			*ppSurface = wrapper;
		}
		else {
			*ppSurface = reinterpret_cast<D3D9Wrapper::IDirect3DSurface9*>(baseSurface);
		}
	}
	if (ppSurface) LogInfo("  returns result=%x, handle=%p, wrapper=%p\n", hr, baseSurface, *ppSurface);

    return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::CreateDepthStencilSurface(THIS_ UINT Width,UINT Height,::D3DFORMAT Format,::D3DMULTISAMPLE_TYPE MultiSample,DWORD MultisampleQuality,BOOL Discard,
																	  D3D9Wrapper::IDirect3DSurface9** ppSurface,HANDLE* pSharedHandle)
{
	LogInfo("IDirect3DDevice9::CreateDepthStencilSurface Width=%d Height=%d Format=%d\n", Width, Height, Format);
	D3D9Wrapper::IDirect3DSurface9 * wrapper;
	if (!GetD3D9Device())
	{
		LogInfo("  postponing call because device was not created yet.\n");
		wrapper = IDirect3DSurface9::GetDirect3DSurface9((::LPDIRECT3DSURFACE9) 0, this, NULL);
		wrapper->_Width = Width;
		wrapper->_Height = Height;
		wrapper->_Format = Format;
		wrapper->_MultiSample = MultiSample;
		wrapper->_MultisampleQuality = MultisampleQuality;
		wrapper->_Discard = Discard;
		wrapper->_Device = this;
		pendingCreateDepthStencilSurface = wrapper;
		*ppSurface = wrapper;
		LogInfo("  returns handle=%p\n", wrapper);

		return S_OK;
	}

	::LPDIRECT3DSURFACE9 baseSurface = NULL;

	D3D2DTEXTURE_DESC pDesc;
	pDesc.Usage = D3DUSAGE_DEPTHSTENCIL;
	if (Discard)
		pDesc.Usage |= D3DUSAGE_DYNAMIC;
	pDesc.Format = Format;
	pDesc.Height = Height;
	pDesc.Width = Width;
	pDesc.MultiSampleType = MultiSample;
	pDesc.MultiSampleQuality = MultisampleQuality;
	pDesc.Levels = 1;
	HRESULT hr;
	ResourceHandleInfo info;
	if (G->gForceStereo == 2) {
		::IDirect3DSurface9* pRightDepthStencil = NULL;
		// create left/mono
		hr = CreateSurface<::IDirect3DSurface9, D3D2DTEXTURE_DESC>(&baseSurface, &info, &pDesc, this, pSharedHandle, SurfaceCreation::DepthStencil);
		if (!FAILED(hr)){
			if (ShouldDuplicate(&pDesc)) {
				hr = CreateSurface<::IDirect3DSurface9, D3D2DTEXTURE_DESC>(&pRightDepthStencil, &info, &pDesc, this, pSharedHandle, SurfaceCreation::DepthStencil);
				if (!FAILED(hr)) {
					wrapper = IDirect3DSurface9::GetDirect3DSurface9(baseSurface, this, pRightDepthStencil);
					wrapper->resourceHandleInfo = info;
				}
				else {
					LogDebug("IDirect3DDevice9::CreateDepthStencilSurface Direct Mode, failed to create right surface, falling back to mono, hr = %d", hr);
					wrapper = IDirect3DSurface9::GetDirect3DSurface9(baseSurface, this, NULL);
					wrapper->resourceHandleInfo = info;
				}
			}
			else {
				LogDebug("IDirect3DDevice9::CreateDepthStencilSurface Direct Mode, non stereo depth stencil");
				wrapper = IDirect3DSurface9::GetDirect3DSurface9(baseSurface, this, NULL);
				wrapper->resourceHandleInfo = info;
			}
		}
		else {
			LogDebug("IDirect3DDevice9::CreateDepthStencilSurface Direct Mode, failed to create left surface, hr = %d ", hr);
		}
	}
	else {
		check_surface_resolution(&pDesc);
		hr = CreateSurface<::IDirect3DSurface9, D3D2DTEXTURE_DESC>(&baseSurface, &info, &pDesc, this, pSharedHandle, SurfaceCreation::DepthStencil);
		wrapper = IDirect3DSurface9::GetDirect3DSurface9(baseSurface, this, NULL);
		wrapper->resourceHandleInfo = info;
	}
	if (ppSurface) {
		if (!(G->enable_hooks >= EnableHooksDX9::ALL)) {
			*ppSurface = wrapper;
		}
		else {
			*ppSurface = reinterpret_cast<D3D9Wrapper::IDirect3DSurface9*>(baseSurface);
		}
	}
	if (ppSurface) LogInfo("  returns result=%x, handle=%p, wrapper=%p\n", hr, baseSurface, *ppSurface);
    return hr;
}
HRESULT DirectModeUpdateSurfaceRight(::IDirect3DDevice9 *origDevice, ::IDirect3DSurface9 *pSourceSurfaceLeft, ::IDirect3DSurface9 *pSourceSurfaceRight, ::IDirect3DSurface9 *pDestSurfaceRight, CONST RECT *pSourceRect, CONST POINT* pDestPoint) {
	HRESULT hr = D3D_OK;
	if (!pSourceSurfaceRight && pDestSurfaceRight) {
		LogDebug("IDirect3DDevice9::UpdateSurface Direct Mode, INFO: UpdateSurface - Source is not stereo, destination is stereo. Copying source to both sides of destination.\n");
		hr = origDevice->UpdateSurface(pSourceSurfaceLeft, pSourceRect, pDestSurfaceRight, pDestPoint);
		if (FAILED(hr))
			LogDebug("ERROR: UpdateSurface - Failed to copy source left to destination right.\n");
	}
	else if (pSourceSurfaceRight && !pDestSurfaceRight) {
		LogDebug("IDirect3DDevice9::UpdateSurface Direct Mode, INFO: UpdateSurface - Source is stereo, destination is not stereo. Copied Left side only.\n");
	}
	else if (pSourceSurfaceRight && pDestSurfaceRight) {
		hr = origDevice->UpdateSurface(pSourceSurfaceRight, pSourceRect, pDestSurfaceRight, pDestPoint);
		if (FAILED(hr))
			LogDebug("IDirect3DDevice9::UpdateSurface Direct Mode, ERROR: UpdateSurface - Failed to copy source right to destination right.\n");
	}
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::UpdateSurface(THIS_ D3D9Wrapper::IDirect3DSurface9 *pSourceSurface,CONST RECT* pSourceRect,
														  D3D9Wrapper::IDirect3DSurface9 *pDestinationSurface,CONST POINT* pDestPoint)
{
	LogDebug("IDirect3DDevice9::UpdateSurface called with SourceSurface=%p, DestinationSurface=%p\n", pSourceSurface, pDestinationSurface);
	CheckDevice(this);
	::LPDIRECT3DSURFACE9 baseSourceSurface = baseSurface9(pSourceSurface);
	::LPDIRECT3DSURFACE9 baseDestinationSurface = baseSurface9(pDestinationSurface);
	D3D9Wrapper::IDirect3DSurface9 *wrappedSource = wrappedSurface9(pSourceSurface);
	D3D9Wrapper::IDirect3DSurface9 *wrappedDest = wrappedSurface9(pDestinationSurface);
	if (G->hunting && G->track_texture_updates != 2) { // Any hunting mode - want to catch hash contamination even while soft disabled
		if (pSourceRect || pDestPoint) {
			::D3DBOX srcBox;
			::D3DBOX dstBox;
			::D3DSURFACE_DESC srcDesc;
			RECT srcRect;
			if (pSourceRect != NULL) {
				srcBox.Top = pSourceRect->top;
				srcBox.Left = pSourceRect->left;
				srcBox.Right = pSourceRect->right;
				srcBox.Bottom = pSourceRect->bottom;
				srcBox.Back = 0;
				srcBox.Front = 0;
			}
			if (pDestPoint != NULL) {
				if (pSourceRect != NULL) {
					srcRect.left = pSourceRect->left;
					srcRect.top = pSourceRect->top;
					srcRect.right = pSourceRect->right;
					srcRect.bottom = pSourceRect->bottom;
				}
				else {
					pSourceSurface->GetDesc(&srcDesc);
					srcRect.left = 0;
					srcRect.top = 0;
					srcRect.right = srcDesc.Width;
					srcRect.bottom = srcDesc.Height;
				}
				dstBox.Top = pDestPoint->y;
				dstBox.Left = pDestPoint->x;
				dstBox.Right = pDestPoint->x + (srcRect.right - srcRect.left);
				dstBox.Bottom = pDestPoint->y + (srcRect.bottom - srcRect.top);
				dstBox.Back = 0;
				dstBox.Front = 0;
			}
			MarkResourceHashContaminated(wrappedDest, 0, wrappedSource, 0, 'R', &dstBox, &srcBox);
		}
		else {
			MarkResourceHashContaminated(wrappedDest, 0, wrappedSource, 0, 'C', NULL, NULL);
		}
	}
	HRESULT hr;
	if (G->gForceStereo == 2) {
		::IDirect3DSurface9* pSourceSurfaceLeft = wrappedSource->DirectModeGetLeft();
		::IDirect3DSurface9* pSourceSurfaceRight = wrappedSource->DirectModeGetRight();
		::IDirect3DSurface9* pDestSurfaceLeft = wrappedDest->DirectModeGetLeft();
		::IDirect3DSurface9* pDestSurfaceRight = wrappedDest->DirectModeGetRight();
		hr = GetD3D9Device()->UpdateSurface(pSourceSurfaceLeft, pSourceRect, pDestSurfaceLeft, pDestPoint);
		if (SUCCEEDED(hr))
			hr = DirectModeUpdateSurfaceRight(GetD3D9Device(), pSourceSurfaceLeft, pSourceSurfaceRight, pDestSurfaceRight, pSourceRect, pDestPoint);
	}
	else {
		hr = GetD3D9Device()->UpdateSurface(baseSourceSurface, pSourceRect, baseDestinationSurface, pDestPoint);
	}
	if (G->track_texture_updates == 1 && pSourceRect == NULL && pDestPoint == NULL)
		PropagateResourceHash(wrappedDest, wrappedSource);
	LogInfo("  returns result=%x\n", hr);

	return hr;
}
void D3D9Wrapper::IDirect3DDevice9::HackerDeviceUnWrapTexture(D3D9Wrapper::IDirect3DBaseTexture9 * pWrappedTexture, ::IDirect3DBaseTexture9 ** ppActualLeftTexture, ::IDirect3DBaseTexture9 ** ppActualRightTexture)
{
	UnWrapTexture(pWrappedTexture, ppActualLeftTexture, ppActualRightTexture);
}
HRESULT DirectModeUpdateTextureRight(::IDirect3DDevice9 *origDevice, ::IDirect3DBaseTexture9 *pSourceTextureLeft, ::IDirect3DBaseTexture9 *pSourceTextureRight, ::IDirect3DBaseTexture9 *pDestTextureRight) {

	HRESULT hr = D3D_OK;
	if (!pSourceTextureRight && pDestTextureRight) {
		LogDebug("IDirect3DDevice9::UpdateTexture Direct Mode, INFO: UpdateTexture - Source is not stereo, destination is stereo. Copying source to both sides of destination.\n");
		hr = origDevice->UpdateTexture(pSourceTextureLeft, pDestTextureRight);
		if (FAILED(hr))
			LogDebug("ERROR: UpdateTexture - Failed to copy source left to destination right.\n");
	}
	else if (pSourceTextureRight && !pDestTextureRight) {
		LogDebug("IDirect3DDevice9::UpdateTexture Direct Mode, INFO: UpdateTexture - Source is stereo, destination is not stereo. Copied Left side only.\n");
	}
	else if (pSourceTextureRight && pDestTextureRight) {
		hr = origDevice->UpdateTexture(pSourceTextureRight, pDestTextureRight);
		if (FAILED(hr))
			LogDebug("IDirect3DDevice9::UpdateTexture Direct Mode, ERROR: UpdateTexture - Failed to copy source right to destination right.\n");
	}
	return hr;
}
STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::UpdateTexture(THIS_ D3D9Wrapper::IDirect3DBaseTexture9 *pSourceTexture, D3D9Wrapper::IDirect3DBaseTexture9 *pDestinationTexture)
{
    LogInfo("IDirect3DDevice9::UpdateTexture called with SourceTexture=%p, DestinationTexture=%p\n", pSourceTexture, pDestinationTexture);

	if (simulateTextureUpdate(pSourceTexture, pDestinationTexture))
		return S_OK;
	CheckDevice(this);
	::IDirect3DBaseTexture9 *baseSourceTexture = baseTexture9(pSourceTexture);
	::IDirect3DBaseTexture9 *baseDestTexture = baseTexture9(pDestinationTexture);
	D3D9Wrapper::IDirect3DBaseTexture9 *wrappedSource = wrappedTexture9(pSourceTexture);
	D3D9Wrapper::IDirect3DBaseTexture9 *wrappedDest = wrappedTexture9(pDestinationTexture);
	if (G->hunting && G->track_texture_updates != 2) { // Any hunting mode - want to catch hash contamination even while soft disabled
		MarkResourceHashContaminated(wrappedDest, 0, wrappedSource, 0, 'C', NULL, NULL);
	}
	HRESULT hr;
	if (G->gForceStereo == 2) {
		::IDirect3DBaseTexture9* pSourceTextureLeft = NULL;
		::IDirect3DBaseTexture9* pSourceTextureRight = NULL;
		::IDirect3DBaseTexture9* pDestTextureLeft = NULL;
		::IDirect3DBaseTexture9* pDestTextureRight = NULL;
		UnWrapTexture(wrappedSource, &pSourceTextureLeft, &pSourceTextureRight);
		UnWrapTexture(wrappedDest, &pDestTextureLeft, &pDestTextureRight);
		hr = GetD3D9Device()->UpdateTexture(pSourceTextureLeft, pDestTextureLeft);
		if (SUCCEEDED(hr))
			hr = DirectModeUpdateTextureRight(GetD3D9Device(), pSourceTextureLeft, pSourceTextureRight, pDestTextureRight);
	}
	else {
		hr = GetD3D9Device()->UpdateTexture(baseSourceTexture, baseDestTexture);
	}
	if (G->track_texture_updates == 1)
		PropagateResourceHash(wrappedDest, wrappedSource);
	LogInfo("  returns result=%x\n", hr);

	return hr;
}
HRESULT DirectModeGetRenderTargetDataRight(::IDirect3DDevice9 *origDevice, ::IDirect3DSurface9 *pRenderTargetLeft, ::IDirect3DSurface9 *pRenderTargetRight, ::IDirect3DSurface9 *pDestSurfaceRight) {
	HRESULT hr = D3D_OK;
	if (!pRenderTargetRight && pDestSurfaceRight) {
		LogDebug("IDirect3DDevice9::GetRenderTargetData Direct Mode, INFO: GetRenderTargetData - Source is not stereo, destination is stereo. Copying source to both sides of destination.\n");
		hr = origDevice->GetRenderTargetData(pRenderTargetLeft, pDestSurfaceRight);
		if (FAILED(hr))
			LogDebug("ERROR: GetRenderTargetData - Failed to copy source left to destination right.\n");
	}
	else if (pRenderTargetRight && !pDestSurfaceRight) {
		LogDebug("IDirect3DDevice9::GetRenderTargetData Direct Mode, INFO: GetRenderTargetData - Source is stereo, destination is not stereo. Copied Left side only.\n");
	}
	else if (pRenderTargetRight && pDestSurfaceRight) {
		hr = origDevice->GetRenderTargetData(pRenderTargetRight, pDestSurfaceRight);
		if (FAILED(hr))
			LogDebug("IDirect3DDevice9::GetRenderTargetData Direct Mode, ERROR: GetRenderTargetData - Failed to copy source right to destination right.\n");
	}
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::GetRenderTargetData(THIS_ D3D9Wrapper::IDirect3DSurface9 *pRenderTarget,
																D3D9Wrapper::IDirect3DSurface9 *pDestSurface)
{
	LogInfo("IDirect3DDevice9::GetRenderTargetData called with RenderTarget=%p, DestSurface=%p\n", pRenderTarget, pDestSurface);
	CheckDevice(this);
	::LPDIRECT3DSURFACE9 baseRenderTarget = baseSurface9(pRenderTarget);
	::LPDIRECT3DSURFACE9 baseDestSurface = baseSurface9(pDestSurface);
	D3D9Wrapper::IDirect3DSurface9* pWrappedRenderTarget = wrappedSurface9(pRenderTarget);
	D3D9Wrapper::IDirect3DSurface9* pWrappedDest = wrappedSurface9(pDestSurface);
	if (G->hunting && G->track_texture_updates != 2) { // Any hunting mode - want to catch hash contamination even while soft disabled
		MarkResourceHashContaminated(pWrappedDest, 0, pWrappedRenderTarget, 0, 'C', NULL, NULL);
	}

	HRESULT hr;
	if (G->gForceStereo == 2) {
		D3D9Wrapper::IDirect3DSurface9* pWrappedRenderTarget = wrappedSurface9(pRenderTarget);
		D3D9Wrapper::IDirect3DSurface9* pWrappedDest = wrappedSurface9(pDestSurface);
		::IDirect3DSurface9* pRenderTargetLeft = pWrappedRenderTarget->DirectModeGetLeft();
		::IDirect3DSurface9* pRenderTargetRight = pWrappedRenderTarget->DirectModeGetRight();
		::IDirect3DSurface9* pDestSurfaceLeft = pWrappedDest->DirectModeGetLeft();
		::IDirect3DSurface9* pDestSurfaceRight = pWrappedDest->DirectModeGetRight();
		hr = GetD3D9Device()->GetRenderTargetData(pRenderTargetLeft, pDestSurfaceLeft);
		if (SUCCEEDED(hr))
			hr = DirectModeGetRenderTargetDataRight(GetD3D9Device(), pRenderTargetLeft, pRenderTargetRight, pDestSurfaceRight);
	}
	else {
		hr = GetD3D9Device()->GetRenderTargetData(baseRenderTarget, baseDestSurface);
	}

	if (G->track_texture_updates == 1)
		PropagateResourceHash(pWrappedDest, pWrappedRenderTarget);
	LogInfo("  returns result=%x\n", hr);

	return hr;

}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::GetFrontBufferData(THIS_ UINT iSwapChain, D3D9Wrapper::IDirect3DSurface9 *pDestSurface)
{
	LogInfo("IDirect3DDevice9::GetFrontBufferData called with SwapChain=%d, DestSurface=%p\n", iSwapChain, pDestSurface);
	CheckDevice(this);

	::LPDIRECT3DSURFACE9 baseDestSurface = baseSurface9(pDestSurface);
	HRESULT hr = GetD3D9Device()->GetFrontBufferData(iSwapChain, baseDestSurface);
	LogInfo("  returns result=%x\n", hr);

	return hr;
}
HRESULT DirectModeStretchRectRight(::IDirect3DDevice9 *origDevice, ::IDirect3DSurface9 *pSourceSurfaceLeft, ::IDirect3DSurface9 *pSourceSurfaceRight, ::IDirect3DSurface9 *pDestSurfaceRight, CONST RECT* pSourceRect, CONST RECT* pDestRect, ::D3DTEXTUREFILTERTYPE Filter) {
	HRESULT hr = D3D_OK;
	if (!pSourceSurfaceRight && pDestSurfaceRight) {
		LogDebug("IDirect3DDevice9::StretchRect, Direct Mode, - Source is not stereo, destination is stereo. Copying source to both sides of destination.\n");
			hr = origDevice->StretchRect(pSourceSurfaceLeft, pSourceRect, pDestSurfaceRight, pDestRect, Filter);
		if (FAILED(hr))
			LogDebug("ERROR: StretchRect - Failed to copy source left to destination right.\n");
	}
	else if (pSourceSurfaceRight && !pDestSurfaceRight) {
		LogDebug("INFO: StretchRect - Source is stereo, destination is not stereo. Copied Left side only.\n");
	}
	else if (pSourceSurfaceRight && pDestSurfaceRight) {
			hr = origDevice->StretchRect(pSourceSurfaceRight, pSourceRect, pDestSurfaceRight, pDestRect, Filter);
		if (FAILED(hr))
			LogDebug("ERROR: StretchRect - Failed to copy source right to destination right.\n");
	}
	return hr;
}
STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::StretchRect(THIS_ D3D9Wrapper::IDirect3DSurface9 *pSourceSurface,CONST RECT* pSourceRect,
														D3D9Wrapper::IDirect3DSurface9 *pDestSurface,CONST RECT* pDestRect,::D3DTEXTUREFILTERTYPE Filter)
{
	LogDebug("IDirect3DDevice9::StretchRect called using SourceSurface=%p, DestSurface=%p\n", pSourceSurface, pDestSurface);

	CheckDevice(this);
	::LPDIRECT3DSURFACE9 baseSourceSurface = baseSurface9(pSourceSurface);
	::LPDIRECT3DSURFACE9 baseDestSurface = baseSurface9(pDestSurface);
	D3D9Wrapper::IDirect3DSurface9* pWrappedSource = wrappedSurface9(pSourceSurface);
	D3D9Wrapper::IDirect3DSurface9* pWrappedDest = wrappedSurface9(pDestSurface);
	if (G->gAutoDetectDepthBuffer && pWrappedSource && pWrappedDest && pWrappedSource->possibleDepthBuffer && pWrappedDest->possibleDepthBuffer) {
		::D3DSURFACE_DESC src_desc;
		pWrappedSource->GetD3DSurface9()->GetDesc(&src_desc);
		::D3DSURFACE_DESC dst_desc;
		pWrappedDest->GetD3DSurface9()->GetDesc(&dst_desc);
		if (src_desc.MultiSampleType != ::D3DMULTISAMPLE_NONE && dst_desc.MultiSampleType == ::D3DMULTISAMPLE_NONE)
		{
			pWrappedSource->depthSourceInfo.resolvedAA_dest = pWrappedDest;
			pWrappedDest->depthSourceInfo.resolvedAA_source = pWrappedSource;
			if (pWrappedSource == depthstencil_replacement) {
				depthstencil_replacement = pWrappedDest;
				if (!pWrappedDest->depthstencil_replacement_surface)
					CreateDepthStencilReplacement(pWrappedDest, true);
				else {
					if (pWrappedDest->depthstencil_replacement_surface != pWrappedDest->GetRealOrig()) {
						pWrappedDest->depthstencil_replacement_resolvedAA = true;
						pWrappedDest->depthstencil_replacement_surface = reinterpret_cast<::IDirect3DSurface9*>(pWrappedDest->GetRealOrig());
						if (!pWrappedDest->depthstencil_replacement_nvapi_registered) {
							NvAPI_D3D9_RegisterResource(pWrappedDest->depthstencil_replacement_surface);
							NvAPI_D3D9_RegisterResource(pWrappedDest->depthstencil_replacement_texture);
							pWrappedDest->depthstencil_replacement_nvapi_registered = true;
						}
					}
				}
			}else if (pWrappedDest == depthstencil_replacement){
				if (pWrappedDest->depthstencil_replacement_surface != pWrappedDest->GetRealOrig()) {
					pWrappedDest->depthstencil_replacement_resolvedAA = true;
					pWrappedDest->depthstencil_replacement_surface = reinterpret_cast<::IDirect3DSurface9*>(pWrappedDest->GetRealOrig());
					if (!pWrappedDest->depthstencil_replacement_nvapi_registered) {
						NvAPI_D3D9_RegisterResource(pWrappedDest->depthstencil_replacement_surface);
						NvAPI_D3D9_RegisterResource(pWrappedDest->depthstencil_replacement_texture);
						pWrappedDest->depthstencil_replacement_nvapi_registered = true;
					}
					if (m_pActiveDepthStencil != nullptr && m_pActiveDepthStencil == depthstencil_replacement)
					{
						GetD3D9Device()->SetDepthStencilSurface(depthstencil_replacement->depthstencil_replacement_surface);
					}
				}
			}
		}
	}
	if (G->hunting && G->track_texture_updates != 2){ // Any hunting mode - want to catch hash contamination even while soft disabled
		if (pSourceRect || pDestRect) {
			::D3DBOX srcBox;
			::D3DBOX dstBox;
			if (pSourceRect != NULL) {
				srcBox.Top = pSourceRect->top;
				srcBox.Left = pSourceRect->left;
				srcBox.Right = pSourceRect->right;
				srcBox.Bottom = pSourceRect->bottom;
				srcBox.Back = 0;
				srcBox.Front = 0;
			}

			if (pDestRect != NULL) {
				dstBox.Top = pDestRect->top;
				dstBox.Left = pDestRect->left;
				dstBox.Right = pDestRect->right;
				dstBox.Bottom = pDestRect->bottom;
				dstBox.Back = 0;
				dstBox.Front = 0;
			}


			MarkResourceHashContaminated(pWrappedDest, 0, pWrappedSource, 0, 'R', &dstBox, &srcBox);
		}
		else {
			MarkResourceHashContaminated(pWrappedDest, 0, pWrappedSource, 0, 'C', NULL, NULL);
		}

	}
	HRESULT hr;
	if (G->gForceStereo == 2) {
		::IDirect3DSurface9* pSourceSurfaceLeft = pWrappedSource->DirectModeGetLeft();
		::IDirect3DSurface9* pSourceSurfaceRight = pWrappedSource->DirectModeGetRight();
		::IDirect3DSurface9* pDestSurfaceLeft = pWrappedDest->DirectModeGetLeft();
		::IDirect3DSurface9* pDestSurfaceRight = pWrappedDest->DirectModeGetRight();
		hr = GetD3D9Device()->StretchRect(pSourceSurfaceLeft, pSourceRect, pDestSurfaceLeft, pDestRect, Filter);
		if (SUCCEEDED(hr))
			hr = DirectModeStretchRectRight(GetD3D9Device(), pSourceSurfaceLeft, pSourceSurfaceRight, pDestSurfaceRight, pSourceRect, pDestRect, Filter);
	}
	else {
		hr = GetD3D9Device()->StretchRect(baseSourceSurface, pSourceRect, baseDestSurface, pDestRect, Filter);
	}

	// We only update the destination resource hash when the entire
	// subresource 0 is updated and pSrcBox is NULL. We could check if the
	// pSrcBox fills the entire resource, but if the game is using pSrcBox
	// it stands to reason that it won't always fill the entire resource
	// and the hashes might be less predictable. Possibly something to
	// enable as an option in the future if there is a proven need.
	if (G->track_texture_updates == 1 && pSourceRect == NULL && pDestRect == NULL)
		PropagateResourceHash(pWrappedDest, pWrappedSource);
	LogInfo("  returns result=%x\n", hr);
	return hr;
}


STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::ColorFill(THIS_ D3D9Wrapper::IDirect3DSurface9 *pSurface,CONST RECT* pRect,::D3DCOLOR color)
{
	LogDebug("IDirect3DDevice9::ColorFill called with Surface=%p\n", pSurface);
	CheckDevice(this);
	::LPDIRECT3DSURFACE9 baseSurface = baseSurface9(pSurface);
	HRESULT hr;
	if (G->gForceStereo == 2) {
		D3D9Wrapper::IDirect3DSurface9 *pDerivedSurface = wrappedSurface9(pSurface);
		if (pDerivedSurface->IsDirectStereoSurface()) {
			hr = GetD3D9Device()->ColorFill(baseSurface, pRect, color);
			hr = GetD3D9Device()->ColorFill(pDerivedSurface->DirectModeGetRight(), pRect, color);
		}
		else {
			hr = GetD3D9Device()->ColorFill(baseSurface, pRect, color);
		}
	}
	else {
		hr = GetD3D9Device()->ColorFill(baseSurface, pRect, color);
	}
	LogDebug("  returns result=%x\n", hr);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::CreateOffscreenPlainSurface(THIS_ UINT Width,UINT Height,::D3DFORMAT Format,::D3DPOOL Pool, D3D9Wrapper::IDirect3DSurface9 **ppSurface,HANDLE* pSharedHandle)
{
	LogInfo("IDirect3DDevice9::CreateOffscreenPlainSurface called with Width=%d Height=%d\n", Width, Height);
	CheckDevice(this);
	::LPDIRECT3DSURFACE9 baseSurface = NULL;
	if (G->gForwardToEx && Pool == ::D3DPOOL_MANAGED)
	{
		LogDebug("  Pool changed from MANAGED to DEFAULT because of DirectX9Ex migration.\n");
		Pool = ::D3DPOOL_DEFAULT;
	}
	D3D2DTEXTURE_DESC pDesc;
	pDesc.Usage = 0;
	pDesc.Format = Format;
	pDesc.Height = Height;
	pDesc.Width = Width;
	pDesc.Pool = Pool;
	pDesc.Levels = 1;
	ResourceHandleInfo info;
	HRESULT hr = CreateSurface<::IDirect3DSurface9, D3D2DTEXTURE_DESC>(&baseSurface, &info, &pDesc, this, pSharedHandle, SurfaceCreation::OffscreenPlain);
	if (baseSurface) {
		D3D9Wrapper::IDirect3DSurface9 * wrapper;
		wrapper = IDirect3DSurface9::GetDirect3DSurface9(baseSurface, this, NULL);
		wrapper->resourceHandleInfo = info;
		if (ppSurface) {
			if (!(G->enable_hooks >= EnableHooksDX9::ALL)) {
				*ppSurface = wrapper;
			}
			else {
				*ppSurface = reinterpret_cast<D3D9Wrapper::IDirect3DSurface9*>(baseSurface);
			}
		}
	}
	if (ppSurface) LogInfo("  returns result=%x, handle=%p, wrapper=%p\n", hr, baseSurface, *ppSurface);
    return hr;
}
#define SMALL_FLOAT 0.001f
#define	SLIGHTLY_LESS_THAN_ONE 0.999f
inline bool D3D9Wrapper::IDirect3DDevice9::isViewportDefaultForMainRT(CONST ::D3DVIEWPORT9* pViewport)
{
	LogDebug("isViewportDefaultForMainRT\n");
	D3D9Wrapper::IDirect3DSurface9* pPrimaryRenderTarget = m_activeRenderTargets[0];
	::D3DSURFACE_DESC pRTDesc;
	pPrimaryRenderTarget->GetDesc(&pRTDesc);

	return  ((pViewport->Height == pRTDesc.Height) && (pViewport->Width == pRTDesc.Width) &&
		(pViewport->MinZ <= SMALL_FLOAT) && (pViewport->MaxZ >= SLIGHTLY_LESS_THAN_ONE));
}
STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::SetRenderTarget(THIS_ DWORD RenderTargetIndex, D3D9Wrapper::IDirect3DSurface9 *pRenderTarget)
{
	LogDebug("IDirect3DDevice9::SetRenderTarget called with RenderTargetIndex=%d, pRenderTarget=%p.\n", RenderTargetIndex, pRenderTarget);
	CheckDevice(this);
	::LPDIRECT3DSURFACE9 baseRenderTarget = baseSurface9(pRenderTarget);
	D3D9Wrapper::IDirect3DSurface9* newRenderTarget = wrappedSurface9(pRenderTarget);
	Profiling::State profiling_state;
	HRESULT hr;
	if (pRenderTarget == NULL) {
		if (RenderTargetIndex == 0) {
			// main render target should never be set to NULL
			hr = D3DERR_INVALIDCALL;
		}
		else {
			hr = GetD3D9Device()->SetRenderTarget(RenderTargetIndex, NULL);
		}
	}else{
		if (G->gForceStereo == 2) {
			if (currentRenderingSide == RenderPosition::Left) {
				hr = GetD3D9Device()->SetRenderTarget(RenderTargetIndex, newRenderTarget->DirectModeGetLeft());
			}
			else {
				hr = GetD3D9Device()->SetRenderTarget(RenderTargetIndex, newRenderTarget->DirectModeGetRight());
			}
		}
		else {
			hr = GetD3D9Device()->SetRenderTarget(RenderTargetIndex, baseRenderTarget);
		}
	}
	if (!FAILED(hr)) {
		// changing rendertarget resets viewport to fullsurface
		m_bActiveViewportIsDefault = true;
		// replace with new render target (may be NULL)
		D3D9Wrapper::IDirect3DSurface9 *existingRT = m_activeRenderTargets[RenderTargetIndex];
		if (newRenderTarget != existingRT) {
			if (newRenderTarget)
				newRenderTarget->Bound();
			if (existingRT)
				existingRT->Unbound();
			m_activeRenderTargets[RenderTargetIndex] = newRenderTarget;
		}
	}
	if (G->hunting == HUNTING_MODE_ENABLED) {
		EnterCriticalSection(&G->mCriticalSection);
		mCurrentRenderTargets.clear();
		LeaveCriticalSection(&G->mCriticalSection);
		if (G->DumpUsage) {
			if (Profiling::mode == Profiling::Mode::SUMMARY)
				Profiling::start(&profiling_state);
			for (UINT i = 0; i < m_activeRenderTargets.size(); ++i) {
				if (!m_activeRenderTargets[i])
					continue;
				RecordRenderTargetInfo(m_activeRenderTargets[i], i);
			}
			if (Profiling::mode == Profiling::Mode::SUMMARY)
				Profiling::end(&profiling_state, &Profiling::stat_overhead);
		}
	}
	LogInfo("  returns result=%x\n", hr);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::GetRenderTarget(THIS_ DWORD RenderTargetIndex, D3D9Wrapper::IDirect3DSurface9 **ppRenderTarget)
{
	LogDebug("IDirect3DDevice9::GetRenderTarget called with RenderTargetIndex=%d\n", RenderTargetIndex);

	CheckDevice(this);
	::LPDIRECT3DSURFACE9 baseSurface = 0;
	HRESULT hr;
	if ((RenderTargetIndex >= m_activeRenderTargets.size()) || (RenderTargetIndex < 0) || !ppRenderTarget) {
		hr = D3DERR_INVALIDCALL;
	}
	else {
		D3D9Wrapper::IDirect3DSurface9* targetToReturn = m_activeRenderTargets[RenderTargetIndex];
		if (!targetToReturn) {
			hr = D3DERR_NOTFOUND;
		}
		else {
				baseSurface = targetToReturn->GetD3DSurface9();
				targetToReturn->AddRef();
				hr = S_OK;
				if (!(G->enable_hooks >= EnableHooksDX9::ALL)) {
					*ppRenderTarget = targetToReturn;
				}
				else {
					*ppRenderTarget = reinterpret_cast<D3D9Wrapper::IDirect3DSurface9*>(targetToReturn->GetRealOrig());
				}
		}
	}
	if (FAILED(hr) || !baseSurface)
	{
		if (!gLogDebug) LogInfo("IDirect3DDevice9::GetRenderTarget called.\n");
		LogInfo("  failed with hr=%x\n", hr);

		if (ppRenderTarget) *ppRenderTarget = 0;
		return hr;
	}
	if (ppRenderTarget) LogDebug("  returns result=%x, handle=%p, wrapper=%p\n", hr, baseSurface, *ppRenderTarget);
    return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::SetDepthStencilSurface(THIS_ D3D9Wrapper::IDirect3DSurface9 *pNewZStencil)
{
	LogDebug("IDirect3DDevice9::SetDepthStencilSurface called with NewZStencil=%p\n", pNewZStencil);

	if (!GetD3D9Device())
	{
		LogInfo("  postponing call because device was not created yet.\n");

		pendingSetDepthStencilSurface = pNewZStencil;
		return S_OK;
	}

	::LPDIRECT3DSURFACE9 baseStencil = baseSurface9(pNewZStencil);
	D3D9Wrapper::IDirect3DSurface9* pNewDepthStencil = wrappedSurface9(pNewZStencil);
	Profiling::State profiling_state;
	if (G->hunting == HUNTING_MODE_ENABLED) {
		if (G->DumpUsage) {
			if (Profiling::mode == Profiling::Mode::SUMMARY)
				Profiling::start(&profiling_state);
			RecordDepthStencil(pNewDepthStencil);
			if (Profiling::mode == Profiling::Mode::SUMMARY)
				Profiling::end(&profiling_state, &Profiling::stat_overhead);
		}
	}
	if (G->gAutoDetectDepthBuffer && pNewDepthStencil) {
		if (depth_sources.find(pNewDepthStencil) == depth_sources.end())
		{
			::D3DSURFACE_DESC desc;
			pNewDepthStencil->GetD3DSurface9()->GetDesc(&desc);

			// Early rejection
			if (!((desc.Width < G->mResolutionInfo.width * 0.95 || desc.Width > G->mResolutionInfo.width * 1.05) ||
				(desc.Height < G->mResolutionInfo.height * 0.95 || desc.Height > G->mResolutionInfo.height * 1.05)))
			{
				pNewDepthStencil->possibleDepthBuffer = true;

				// Begin tracking
				const DepthSourceInfo info = { desc.Width, desc.Height};
				pNewDepthStencil->depthSourceInfo = info;
				depth_sources.emplace(pNewDepthStencil);
			}
		}
	}
	HRESULT hr;
	if (G->gForceStereo == 2) {
		::IDirect3DSurface9* pActualStencilForCurrentSide = NULL;
		if (pNewDepthStencil) {
			if (currentRenderingSide == RenderPosition::Left){
				pActualStencilForCurrentSide = pNewDepthStencil->DirectModeGetLeft();
			}
			else {
				pActualStencilForCurrentSide = pNewDepthStencil->DirectModeGetRight();
			}
		}
		// Update actual depth stencil
		hr = GetD3D9Device()->SetDepthStencilSurface(pActualStencilForCurrentSide);
	}
	else {
		if (depthstencil_replacement != nullptr && pNewDepthStencil == depthstencil_replacement)
			hr = GetD3D9Device()->SetDepthStencilSurface(pNewDepthStencil->depthstencil_replacement_surface);
		else
			hr = GetD3D9Device()->SetDepthStencilSurface(baseStencil);
	}
	// Update stored proxy depth stencil
	if (SUCCEEDED(hr)){
		if (pNewDepthStencil != m_pActiveDepthStencil) {
			if (pNewDepthStencil)
				pNewDepthStencil->Bound();
			if (m_pActiveDepthStencil)
				m_pActiveDepthStencil->Unbound();
			m_pActiveDepthStencil = pNewDepthStencil;
		}
	}
	LogDebug("  returns result=%x\n", hr);

	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::GetDepthStencilSurface(THIS_ D3D9Wrapper::IDirect3DSurface9 **ppZStencilSurface)
{
	LogDebug("IDirect3DDevice9::GetDepthStencilSurface called.\n");

	CheckDevice(this);

	::LPDIRECT3DSURFACE9 baseSurface = 0;
	HRESULT hr;
	if (!ppZStencilSurface) {
		hr = D3DERR_INVALIDCALL;
	}
	else {
		if (!m_pActiveDepthStencil) {
			hr = D3DERR_NOTFOUND;
		}
		else {
			baseSurface = m_pActiveDepthStencil->GetD3DSurface9();
			m_pActiveDepthStencil->AddRef();
			hr = S_OK;
			if (!(G->enable_hooks >= EnableHooksDX9::ALL)) {
				*ppZStencilSurface = m_pActiveDepthStencil;
			}
			else {
				*ppZStencilSurface = reinterpret_cast<D3D9Wrapper::IDirect3DSurface9*>(m_pActiveDepthStencil->GetRealOrig());
			}
		}
	}

	if (FAILED(hr) || !baseSurface)
	{
		if (!gLogDebug) LogInfo("IDirect3DDevice9::GetDepthStencilSurface called.\n");
		LogInfo("  failed with hr=%x\n", hr);

		if (ppZStencilSurface) *ppZStencilSurface = 0;
		return hr;
	}
	if (ppZStencilSurface) LogDebug("  returns hr=%x, handle=%p, wrapper=%p\n", hr, baseSurface, *ppZStencilSurface);
    return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::BeginScene(THIS)
{
	LogDebug("IDirect3DDevice9::BeginScene called.\n");

	CheckDevice(this);
    HRESULT hr = GetD3D9Device()->BeginScene();
	LogDebug("  returns result=%x\n", hr);

	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::EndScene(THIS)
{
	LogDebug("IDirect3DDevice9::EndScene called.\n");
	CheckDevice(this);
    HRESULT hr = GetD3D9Device()->EndScene();
	LogDebug("  returns result=%x\n", hr);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::Clear(THIS_ DWORD Count,CONST ::D3DRECT* pRects,DWORD Flags,::D3DCOLOR Color, float Z,DWORD Stencil)
{
	LogDebug("IDirect3DDevice9::Clear called.\n");

	CheckDevice(this);

	::IDirect3DSurface9 *dss = NULL;
	if ((G->clear_dsv_command_list.commands.size() > 0) && ((Flags & D3DCLEAR_STENCIL) || (Flags & D3DCLEAR_ZBUFFER))) {
		GetD3D9Device()->GetDepthStencilSurface(&dss);
		RunResourceCommandList(this, &G->clear_dsv_command_list, dss, false);
	}
	vector<::IDirect3DSurface9*> renderTargets;
	if ((G->clear_rtv_command_list.commands.size() > 0) && (Flags & D3DCLEAR_TARGET)) {
		::D3DCAPS9 pCaps;// = NULL;
		GetD3D9Device()->GetDeviceCaps(&pCaps);
		for (UINT x = 0; x < pCaps.NumSimultaneousRTs; x++) {
			::IDirect3DSurface9 *rts;
			GetD3D9Device()->GetRenderTarget(x, &rts);
			if (rts)
				renderTargets.push_back(rts);
				RunResourceCommandList(this, &G->clear_rtv_command_list, rts, false);
		}
	}
	HRESULT hr = GetD3D9Device()->Clear(Count, pRects, Flags, Color, Z, Stencil);
	if (G->gForceStereo == 2) {
		if (SwitchDrawingSide()) {
			hr = GetD3D9Device()->Clear(Count, pRects, Flags, Color, Z, Stencil);
			if (FAILED(hr))
				LogDebug(" Direct Mode failed to clear other eye, hr = ", hr);
		}
	}
	LogDebug("  returns result=%x\n", hr);

	if (dss) {
		RunResourceCommandList(this, &G->clear_dsv_command_list, dss, true);
		dss->Release();
		dss = NULL;
	}
	if (renderTargets.size() > 0) {
		for (UINT x = 0; x < renderTargets.size(); x++) {
			::IDirect3DSurface9 *rts = renderTargets.at(x);
			RunResourceCommandList(this, &G->clear_rtv_command_list, rts, true);
			rts->Release();
		}
		renderTargets.clear();
	}

	return hr;

}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::SetTransform(THIS_ ::D3DTRANSFORMSTATETYPE State,CONST ::D3DMATRIX* pMatrix)
{
	LogDebug("IDirect3DDevice9::SetTransform called.\n");
	CheckDevice(this);
	HRESULT hr;
	hr = GetD3D9Device()->SetTransform(State, pMatrix);
	LogDebug("  returns result=%x\n", hr);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::GetTransform(THIS_ ::D3DTRANSFORMSTATETYPE State,::D3DMATRIX* pMatrix)
{
	LogDebug("IDirect3DDevice9::GetTransform called.\n");

	CheckDevice(this);
	HRESULT hr = GetD3D9Device()->GetTransform(State, pMatrix);
	LogDebug("  returns result=%x\n", hr);

	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::MultiplyTransform(THIS_ ::D3DTRANSFORMSTATETYPE a,CONST ::D3DMATRIX *b)
{
	LogDebug("IDirect3DDevice9::MultiplyTransform called.\n");

	CheckDevice(this);
	HRESULT hr = GetD3D9Device()->MultiplyTransform(a, b);
	LogDebug("  returns result=%x\n", hr);

	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::SetViewport(THIS_ CONST ::D3DVIEWPORT9* pViewport)
{
	LogDebug("IDirect3DDevice9::SetViewport called.\n");
	LogDebug("  width = %d, height = %d\n", pViewport->Width, pViewport->Height);
	CheckDevice(this);
	// In the 3D Vision Direct Mode, we need to double the width of any ViewPorts
	// We specifically modify the input, so that the game is using full 2x width.
	// Modifying every ViewPort rect seems wrong, so we are only doing those that
	// match the screen resolution.
	if (G->gForceStereo == 2)
	{
		m_bActiveViewportIsDefault = isViewportDefaultForMainRT(pViewport);
		m_LastViewportSet = *pViewport;
	}

	HRESULT hr = GetD3D9Device()->SetViewport(pViewport);
	LogDebug("  returns result=%x\n", hr);

	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::GetViewport(THIS_ ::D3DVIEWPORT9* pViewport)
{
	LogDebug("IDirect3DDevice9::GetViewport called.\n");

	CheckDevice(this);
	HRESULT hr = GetD3D9Device()->GetViewport(pViewport);
	LogDebug("  width = %d, height = %d\n", pViewport->Width, pViewport->Height);
	LogDebug("  returns result=%x\n", hr);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::SetMaterial(THIS_ CONST ::D3DMATERIAL9* pMaterial)
{
	LogDebug("IDirect3DDevice9::SetMaterial called.\n");

	CheckDevice(this);
	HRESULT hr = GetD3D9Device()->SetMaterial(pMaterial);
	LogDebug("  returns result=%x\n", hr);

	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::GetMaterial(THIS_ ::D3DMATERIAL9* pMaterial)
{
	LogDebug("IDirect3DDevice9::GetMaterial called.\n");

	CheckDevice(this);
	HRESULT hr = GetD3D9Device()->GetMaterial(pMaterial);
	LogDebug("  returns result=%x\n", hr);

	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::SetLight(THIS_ DWORD Index,CONST ::D3DLIGHT9 *Light)
{
	LogDebug("IDirect3DDevice9::SetLight called.\n");

	CheckDevice(this);
	HRESULT hr = GetD3D9Device()->SetLight(Index, Light);
	LogDebug("  returns result=%x\n", hr);

	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::GetLight(THIS_ DWORD Index,::D3DLIGHT9* Light)
{
	LogDebug("IDirect3DDevice9::GetLight called.\n");

	CheckDevice(this);
	HRESULT hr = GetD3D9Device()->GetLight(Index, Light);
	LogDebug("  returns result=%x\n", hr);

	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::LightEnable(THIS_ DWORD Index,BOOL Enable)
{
	LogDebug("IDirect3DDevice9::LightEnable called.\n");

	CheckDevice(this);
	HRESULT hr = GetD3D9Device()->LightEnable(Index, Enable);
	LogDebug("  returns result=%x\n", hr);

	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::GetLightEnable(THIS_ DWORD Index,BOOL* pEnable)
{
	LogDebug("IDirect3DDevice9::GetLightEnable called.\n");

	CheckDevice(this);
	HRESULT hr = GetD3D9Device()->GetLightEnable(Index, pEnable);
	LogDebug("  returns result=%x\n", hr);

	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::SetClipPlane(THIS_ DWORD Index,CONST float* pPlane)
{
	LogDebug("IDirect3DDevice9::SetClipPlane called.\n");

	CheckDevice(this);
	HRESULT hr = GetD3D9Device()->SetClipPlane(Index, pPlane);
	LogDebug("  returns result=%x\n", hr);

	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::GetClipPlane(THIS_ DWORD Index,float* pPlane)
{
	LogDebug("IDirect3DDevice9::GetClipPlane called.\n");

	CheckDevice(this);
	HRESULT hr = GetD3D9Device()->GetClipPlane(Index, pPlane);
	LogDebug("  returns result=%x\n", hr);

	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::SetRenderState(THIS_ ::D3DRENDERSTATETYPE State,DWORD Value)
{
	LogDebug("IDirect3DDevice9::SetRenderState called with State=%d, Value=%d\n", State, Value);

	if (!GetD3D9Device())
	{
		if (!gLogDebug) LogInfo("IDirect3DDevice9::SetRenderState called.\n");
		LogInfo("  ignoring call because device was not created yet.\n");
		return S_OK;
	}
	HRESULT hr;
	if ((G->SCISSOR_DISABLE || (G->helix_fix && G->helix_skip_set_scissor_rect)) && State == ::D3DRS_SCISSORTESTENABLE && Value == TRUE)
	{
		LogDebug("  disabling scissor mode.\n");
		hr = D3D_OK;
	}
	else {
		hr = GetD3D9Device()->SetRenderState(State, Value);
		if (!FAILED(hr)){
			if (G->gAutoDetectDepthBuffer && State == ::D3DRS_ZFUNC) {
				current_zfunc = (::D3DCMPFUNC)Value;
			}
		}
	}
	LogDebug("  returns result=%x\n", hr);

	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::GetRenderState(THIS_ ::D3DRENDERSTATETYPE State,DWORD* pValue)
{
	LogDebug("IDirect3DDevice9::GetRenderState called.\n");

	CheckDevice(this);
	HRESULT hr = GetD3D9Device()->GetRenderState(State, pValue);
	LogDebug("  returns result=%x\n", hr);

	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::CreateStateBlock(THIS_ ::D3DSTATEBLOCKTYPE Type, D3D9Wrapper::IDirect3DStateBlock9** ppSB)
{
	LogDebug("IDirect3DDevice9::CreateStateBlock called.\n");
	::LPDIRECT3DSTATEBLOCK9 baseStateBlock = NULL;
	CheckDevice(this);
	HRESULT hr = GetD3D9Device()->CreateStateBlock(Type, &baseStateBlock);
	if (baseStateBlock) {
		D3D9Wrapper::IDirect3DStateBlock9 *wrapper = IDirect3DStateBlock9::GetDirect3DStateBlock9(baseStateBlock, this);
		if (!(G->enable_hooks >= EnableHooksDX9::ALL)) {
			*ppSB = wrapper;
		}
		else {
			*ppSB = reinterpret_cast<D3D9Wrapper::IDirect3DStateBlock9*>(baseStateBlock);
		}
	}
	LogDebug("  returns result=%x\n", hr);

	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::BeginStateBlock(THIS)
{
	LogDebug("IDirect3DDevice9::BeginStateBlock called.\n");

	CheckDevice(this);
	HRESULT hr = GetD3D9Device()->BeginStateBlock();
	LogDebug("  returns result=%x\n", hr);

	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::EndStateBlock(THIS_ D3D9Wrapper::IDirect3DStateBlock9** ppSB)
{
	LogDebug("IDirect3DDevice9::EndStateBlock called.\n");
	::LPDIRECT3DSTATEBLOCK9 baseStateBlock = NULL;
	CheckDevice(this);
	HRESULT hr = GetD3D9Device()->EndStateBlock(&baseStateBlock);
	if (baseStateBlock) {
		D3D9Wrapper::IDirect3DStateBlock9 *wrapper = IDirect3DStateBlock9::GetDirect3DStateBlock9(baseStateBlock, this);
		if (!(G->enable_hooks >= EnableHooksDX9::ALL)) {
			*ppSB = wrapper;
		}
		else {
			*ppSB = reinterpret_cast<D3D9Wrapper::IDirect3DStateBlock9*>(baseStateBlock);
		}
	}
	LogDebug("  returns result=%x\n", hr);

	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::SetClipStatus(THIS_ CONST ::D3DCLIPSTATUS9* pClipStatus)
{
	LogDebug("IDirect3DDevice9::SetClipStatus called.\n");

	CheckDevice(this);
	HRESULT hr = GetD3D9Device()->SetClipStatus(pClipStatus);
	LogDebug("  returns result=%x\n", hr);

	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::GetClipStatus(THIS_ ::D3DCLIPSTATUS9* pClipStatus)
{
	LogDebug("IDirect3DDevice9::GetClipStatus called.\n");

	CheckDevice(this);
	HRESULT hr = GetD3D9Device()->GetClipStatus(pClipStatus);
	LogDebug("  returns result=%x\n", hr);

	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::GetTexture(THIS_ DWORD Stage, D3D9Wrapper::IDirect3DBaseTexture9** ppTexture)
{
	LogDebug("IDirect3DDevice9::GetTexture called.\n");

	CheckDevice(this);
	HRESULT hr;
	::LPDIRECT3DBASETEXTURE9 baseTexture = 0;
	if (!ppTexture) {
		hr = D3DERR_INVALIDCALL;
	}else{
		auto it = m_activeTextureStages.find(Stage);
		if (it == m_activeTextureStages.end()) {
			*ppTexture = NULL;
			hr = S_OK;
		}
		else {
			D3D9Wrapper::IDirect3DBaseTexture9 *wrapper = it->second;
			baseTexture = wrapper->GetD3DBaseTexture9();
			wrapper->AddRef();
			hr = S_OK;
			if (!(G->enable_hooks >= EnableHooksDX9::ALL)) {
				*ppTexture = wrapper;
			}
			else {
				*ppTexture = reinterpret_cast<D3D9Wrapper::IDirect3DBaseTexture9*>(wrapper->GetRealOrig());
			}
		}
	}
	LogDebug("  returns result=%x\n", hr);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::SetTexture(THIS_ DWORD Stage,D3D9Wrapper::IDirect3DBaseTexture9 *pTexture)
{
	LogDebug("IDirect3DDevice9::SetTexture called.\n");

	CheckDevice(this);
	::IDirect3DBaseTexture9 *baseTexture = baseTexture9(pTexture);
	D3D9Wrapper::IDirect3DBaseTexture9 *wrappedTexture = wrappedTexture9(pTexture);
	HRESULT hr;
	if (!pTexture) {
		hr = _SetTexture(Stage, NULL);
		if (!FAILED(hr)) {
			auto it = m_activeTextureStages.find(Stage);
			if (it != m_activeTextureStages.end()) {
				it->second->Unbound(Stage);
				m_activeTextureStages.erase(it);
			}
			if (G->gForceStereo == 2)
				m_activeStereoTextureStages.erase(Stage);
		}
	}
	else {
		if (G->gForceStereo == 2) {
			::IDirect3DBaseTexture9* pActualLeftTexture = NULL;
			::IDirect3DBaseTexture9* pActualRightTexture = NULL;
			UnWrapTexture(wrappedTexture, &pActualLeftTexture, &pActualRightTexture);
			if ((pActualRightTexture == NULL) || (currentRenderingSide == RenderPosition::Left)) { // use left (mono) if not stereo or one left side
				hr = _SetTexture(Stage, pActualLeftTexture);
			}
			else {
				hr = _SetTexture(Stage, pActualRightTexture);
			}
			if (!FAILED(hr)) {
				if (pActualRightTexture)
					m_activeStereoTextureStages[Stage] = wrappedTexture;
				else
					m_activeStereoTextureStages.erase(Stage);
			}
		}
		else {
			hr = _SetTexture(Stage, baseTexture);
		}
		if (!FAILED(hr)) {
			auto it = m_activeTextureStages.find(Stage);
			if (it != m_activeTextureStages.end()) {
				if (wrappedTexture != it->second) {
					wrappedTexture->Bound(Stage);
					it->second->Unbound(Stage);
					it->second = wrappedTexture;
				}
			}
			else {
				wrappedTexture->Bound(Stage);
				m_activeTextureStages.emplace(Stage, wrappedTexture);
			}
		}
	}
	LogDebug("  returns result=%x\n", hr);

	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::GetTextureStageState(THIS_ DWORD Stage,::D3DTEXTURESTAGESTATETYPE Type,DWORD* pValue)
{
	LogDebug("IDirect3DDevice9::GetTextureStageState called.\n");

	CheckDevice(this);
	HRESULT hr = GetD3D9Device()->GetTextureStageState(Stage, Type, pValue);
	LogDebug("  returns result=%x\n", hr);

	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::SetTextureStageState(THIS_ DWORD Stage,::D3DTEXTURESTAGESTATETYPE Type, DWORD Value)
{
	LogDebug("IDirect3DDevice9::SetTextureStageState called.\n");

	CheckDevice(this);
	HRESULT hr = GetD3D9Device()->SetTextureStageState(Stage, Type, Value);
	LogDebug("  returns result=%x\n", hr);

	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::GetSamplerState(THIS_ DWORD Sampler,::D3DSAMPLERSTATETYPE Type,DWORD* pValue)
{
	LogDebug("IDirect3DDevice9::GetSamplerState called.\n");

	CheckDevice(this);
	HRESULT hr = GetD3D9Device()->GetSamplerState(Sampler, Type, pValue);
	LogDebug("  returns result=%x\n", hr);

	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::SetSamplerState(THIS_ DWORD Sampler,::D3DSAMPLERSTATETYPE Type,DWORD Value)
{
	LogDebug("IDirect3DDevice9::SetSamplerState called with Sampler=%d, StateType=%d, Value=%d\n", Sampler, Type, Value);

	if (!GetD3D9Device())
	{
		if (!gLogDebug) LogInfo("IDirect3DDevice9::SetSamplerState called.\n");
		LogInfo("  ignoring call because device was not created yet.\n");

		return S_OK;
	}

	HRESULT hr = GetD3D9Device()->SetSamplerState(Sampler, Type, Value);
	LogDebug("  returns result=%x\n", hr);

	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::ValidateDevice(THIS_ DWORD* pNumPasses)
{
	LogDebug("IDirect3DDevice9::ValidateDevice called.\n");

	CheckDevice(this);
	HRESULT hr = GetD3D9Device()->ValidateDevice(pNumPasses);
	LogDebug("  returns result=%x\n", hr);

	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::SetPaletteEntries(THIS_ UINT PaletteNumber,CONST PALETTEENTRY* pEntries)
{
	LogDebug("IDirect3DDevice9::SetPaletteEntries called.\n");

	CheckDevice(this);
	HRESULT hr = GetD3D9Device()->SetPaletteEntries(PaletteNumber, pEntries);
	LogDebug("  returns result=%x\n", hr);

	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::GetPaletteEntries(THIS_ UINT PaletteNumber,PALETTEENTRY* pEntries)
{
	LogDebug("IDirect3DDevice9::GetPaletteEntries called.\n");

	CheckDevice(this);
	HRESULT hr = GetD3D9Device()->GetPaletteEntries(PaletteNumber, pEntries);
	LogDebug("  returns result=%x\n", hr);

	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::SetCurrentTexturePalette(THIS_ UINT PaletteNumber)
{
	LogDebug("IDirect3DDevice9::SetCurrentTexturePalette called.\n");

	CheckDevice(this);
	HRESULT hr = GetD3D9Device()->SetCurrentTexturePalette(PaletteNumber);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::GetCurrentTexturePalette(THIS_ UINT *PaletteNumber)
{
	LogDebug("IDirect3DDevice9::GetCurrentTexturePalette called.\n");

	CheckDevice(this);
	return GetD3D9Device()->GetCurrentTexturePalette( PaletteNumber);
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::SetScissorRect(THIS_ CONST RECT* pRect)
{
	LogDebug("IDirect3DDevice9::SetScissorRect called.\n");

	CheckDevice(this);
	HRESULT hr = GetD3D9Device()->SetScissorRect(pRect);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::GetScissorRect(THIS_ RECT* pRect)
{
	LogDebug("IDirect3DDevice9::GetScissorRect called.\n");

	CheckDevice(this);
	return GetD3D9Device()->GetScissorRect(pRect);
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::SetSoftwareVertexProcessing(THIS_ BOOL bSoftware)
{
	LogDebug("IDirect3DDevice9::SetSoftwareVertexProcessing called.\n");

	CheckDevice(this);
	HRESULT hr = GetD3D9Device()->SetSoftwareVertexProcessing(bSoftware);
	return hr;
}

STDMETHODIMP_(BOOL) D3D9Wrapper::IDirect3DDevice9::GetSoftwareVertexProcessing(THIS)
{
	LogDebug("IDirect3DDevice9::GetSoftwareVertexProcessing called.\n");

	CheckDevice(this);
	return GetD3D9Device()->GetSoftwareVertexProcessing();
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::SetNPatchMode(THIS_ float nSegments)
{
	LogDebug("IDirect3DDevice9::SetNPatchMode called.\n");

	CheckDevice(this);
	HRESULT hr = GetD3D9Device()->SetNPatchMode(nSegments);
	return hr;
}

STDMETHODIMP_(float) D3D9Wrapper::IDirect3DDevice9::GetNPatchMode(THIS)
{
	LogDebug("IDirect3DDevice9::GetNPatchMode called.\n");

	CheckDevice(this);
	return GetD3D9Device()->GetNPatchMode();
}
inline bool D3D9Wrapper::IDirect3DDevice9::DirectModeUpdateTransforms()
{
	if (!DirectModeProjectionNeedsUpdate)
		return false;
	if (::D3DXMatrixIsIdentity(&m_gameProjection)) {
		DirectModeGameProjectionIsSet = false;
		m_leftProjection = m_gameProjection;
		m_rightProjection = m_gameProjection;
	}
	else {
		DirectModeGameProjectionIsSet = true;
		float pConvergence;
		float pSeparationPercentage;
		float pEyeSeparation;
		pConvergence = mParamTextureManager.mConvergence;
		pSeparationPercentage = mParamTextureManager.mSeparation;
		pEyeSeparation = mParamTextureManager.mEyeSeparation;
		float separation = pEyeSeparation * pSeparationPercentage / 100;
		float convergence = pEyeSeparation * pSeparationPercentage / 100 * pConvergence;
		::D3DXMATRIX leftprojection = ::D3DXMATRIX(m_gameProjection);
		::D3DXMATRIX rightprojection = ::D3DXMATRIX(m_gameProjection);

		leftprojection._31 -= separation;
		leftprojection._41 = convergence;
		::D3DXMatrixTranspose(&m_leftProjection, &leftprojection);
		rightprojection._31 += separation;
		rightprojection._41 = -convergence;
		::D3DXMatrixTranspose(&m_rightProjection, &rightprojection);
	}
	DirectModeProjectionNeedsUpdate = false;
	return true;
}
inline bool D3D9Wrapper::IDirect3DDevice9::SetDrawingSide(RenderPosition side)
{
	LogDebug("SetDrawingSide \n");
	// Already on the correct eye
	if (side == currentRenderingSide) {
		return true;
	}
	// should never try and render for the right eye if there is no render target for the main render targets right side
	if (!m_activeRenderTargets[0]->IsDirectStereoSurface() && (side == RenderPosition::Right)) {
		return false;
	}
	// Everything hasn't changed yet but we set this first so we don't accidentally use the member instead of the local and break
	// things, as I have already managed twice.
	currentRenderingSide = side;
	// switch render targets to new side
	bool renderTargetChanged = false;
	HRESULT result;
	D3D9Wrapper::IDirect3DSurface9* pCurrentRT;
	for (std::vector<D3D9Wrapper::IDirect3DSurface9*>::size_type i = 0; i != m_activeRenderTargets.size(); i++)
	{
		if ((pCurrentRT = m_activeRenderTargets[i]) != NULL) {

			if (side == RenderPosition::Left)
				result = GetD3D9Device()->SetRenderTarget((DWORD)i, pCurrentRT->DirectModeGetLeft());
			else
				result = GetD3D9Device()->SetRenderTarget((DWORD)i, pCurrentRT->DirectModeGetRight());

			if (result != D3D_OK) {
				LogDebug("Direct Mode - Error trying to set one of the Render Targets while switching between active eyes for drawing.\n");
			}
			else {
				renderTargetChanged = true;
			}
		}
	}
	// if a non-fullsurface viewport is active and a rendertarget changed we need to reapply the viewport
	if (renderTargetChanged && !m_bActiveViewportIsDefault) {
		GetD3D9Device()->SetViewport(&m_LastViewportSet);
	}
	// switch depth stencil to new side
	if (m_pActiveDepthStencil != NULL) {
		if (side == RenderPosition::Left)
			result = GetD3D9Device()->SetDepthStencilSurface(m_pActiveDepthStencil->DirectModeGetLeft());
		else
			result = GetD3D9Device()->SetDepthStencilSurface(m_pActiveDepthStencil->DirectModeGetRight());
	}
	// switch textures to new side
	::IDirect3DBaseTexture9* pActualLeftTexture = NULL;
	::IDirect3DBaseTexture9* pActualRightTexture = NULL;

	for (auto it = m_activeStereoTextureStages.begin(); it != m_activeStereoTextureStages.end(); ++it)
	{
		pActualLeftTexture = NULL;
		pActualRightTexture = NULL;
		UnWrapTexture(it->second, &pActualLeftTexture, &pActualRightTexture);
		if (side == RenderPosition::Left)
			result = GetD3D9Device()->SetTexture(it->first, pActualLeftTexture);
		else
			result = GetD3D9Device()->SetTexture(it->first, pActualRightTexture);

		if (result != D3D_OK)
			LogDebug("Error trying to set one of the textures while switching between active eyes for drawing.\n");
	}

	if (DirectModeGameProjectionIsSet) {
		if (side == RenderPosition::Left)
			GetD3D9Device()->SetTransform(::D3DTS_PROJECTION, &m_leftProjection);
		else
			GetD3D9Device()->SetTransform(::D3DTS_PROJECTION, &m_rightProjection);
	}
	return true;
}
static bool AllCachedStereoValuesKnownForUpdate(CachedStereoValues *cachedStereoValues) {
	if (!cachedStereoValues)
		return false;
	return (cachedStereoValues->KnownConvergence != -1 && cachedStereoValues->KnownEyeSeparation != -1 && cachedStereoValues->KnownSeparation != -1 && cachedStereoValues->StereoActiveIsKnown == true);
}
inline void D3D9Wrapper::IDirect3DDevice9::UpdateStereoParams(bool forceUpdate, CachedStereoValues *cachedStereoValues)
{
	bool allValuesKnown = false;
	if (retreivedInitial3DSettings) {
		if (G->update_stereo_params_freq < 0.0f)
			return;
		allValuesKnown = AllCachedStereoValuesKnownForUpdate(cachedStereoValues);
		if (G->update_stereo_params_freq != 0.0f && !allValuesKnown && ((chrono::high_resolution_clock::now() - update_stereo_params_last_run) < update_stereo_params_interval))
			return;
	}
	else {
		allValuesKnown = AllCachedStereoValuesKnownForUpdate(cachedStereoValues);
	}
	update_stereo_params_last_run += update_stereo_params_interval;

	if (G->ENABLE_TUNE)
	{
		mParamTextureManager.mTuneVariable1 = G->gTuneValue[0];
		mParamTextureManager.mTuneVariable2 = G->gTuneValue[1];
		mParamTextureManager.mTuneVariable3 = G->gTuneValue[2];
		mParamTextureManager.mTuneVariable4 = G->gTuneValue[3];
		int counter = 0;
		if (counter-- < 0)
		{
			counter = 30;
			mParamTextureManager.mForceUpdate = true;
		}
	}

	// Update stereo parameter texture. It's possible to arrive here with no texture available though,
	// so we need to check first. Added check on mStereoHandle to fix crash when stereo disabled in
	// DX9 port since mStereoTexture is created unconditionally.
	if (mStereoTexture && mStereoHandle)
	{
		LogDebug("  updating stereo parameter texture.\n");
		if (allValuesKnown) {
			forceUpdate = true;
			mParamTextureManager.mConvergence = cachedStereoValues->KnownConvergence;
			mParamTextureManager.mSeparation = cachedStereoValues->KnownSeparation;
			mParamTextureManager.mEyeSeparation = cachedStereoValues->KnownEyeSeparation;
			mParamTextureManager.mActive = cachedStereoValues->KnownStereoActive;
		}
		else if (forceUpdate) {
			if (cachedStereoValues->KnownConvergence != -1)
				mParamTextureManager.mConvergence = cachedStereoValues->KnownConvergence;
			if (cachedStereoValues->KnownSeparation != -1)
				mParamTextureManager.mSeparation = cachedStereoValues->KnownSeparation;
			if (cachedStereoValues->KnownEyeSeparation != -1)
				mParamTextureManager.mEyeSeparation = cachedStereoValues->KnownEyeSeparation;
			if (cachedStereoValues->StereoActiveIsKnown)
				mParamTextureManager.mActive = cachedStereoValues->KnownStereoActive;
		}
		else if (cachedStereoValues) {
			mParamTextureManager.mKnownConvergence = cachedStereoValues->KnownConvergence;
			mParamTextureManager.mKnownSeparation = cachedStereoValues->KnownSeparation;
			mParamTextureManager.mKnownEyeSeparation = cachedStereoValues->KnownEyeSeparation;
			mParamTextureManager.mKnownStereoActive = cachedStereoValues->KnownStereoActive;
			mParamTextureManager.mStereoActiveIsKnown = cachedStereoValues->StereoActiveIsKnown;
		}
		if (forceUpdate)
			mParamTextureManager.mForceUpdate = true;
		bool updated = mParamTextureManager.UpdateStereoTexture(GetD3D9Device(), GetD3D9Device(), mStereoTexture, false);
		if (updated) {

		}
		if (G->gForceStereo == 2)
			DirectModeProjectionNeedsUpdate = true;
		if (cachedStereoValues) {
			cachedStereoValues->KnownConvergence = mParamTextureManager.mConvergence;
			cachedStereoValues->KnownSeparation = mParamTextureManager.mSeparation;
			cachedStereoValues->KnownEyeSeparation = mParamTextureManager.mEyeSeparation;
			cachedStereoValues->KnownStereoActive = mParamTextureManager.mActive;
			cachedStereoValues->StereoActiveIsKnown = true;
		}
		if (mParamTextureManager.mActive)
			retreivedInitial3DSettings = true;
		this->stereo_params_updated_this_frame = true;
	}
	else
	{
		LogDebug("  stereo parameter texture missing.\n");
	}
}

inline void D3D9Wrapper::IDirect3DDevice9::DetectDepthSource()
{
	if (G->frame_no % 30 || depth_sources.empty())
		return;
	DepthSourceInfo best_info = { 0, 0 };
	D3D9Wrapper::IDirect3DSurface9 *best_match = nullptr;

	for (auto it = depth_sources.begin(); it != depth_sources.end();)
	{
		const auto depthstencil = (*it);
		auto &depthstencil_info = depthstencil->depthSourceInfo;
		++it;
		if (depthstencil_info.drawcall_count == 0)
		{
			continue;
		}

		if ((depthstencil_info.vertices_count * (1.2f - float(depthstencil_info.drawcall_count) / drawCalls)) >= (best_info.vertices_count * (1.2f - float(best_info.drawcall_count) / drawCalls)))
		{
			if (best_match) {
				best_match->depthSourceInfo.resolvedAA_source = nullptr;
				best_match->depthSourceInfo.resolvedAA_dest = nullptr;
				best_match->depthSourceInfo.drawcall_count = best_match->depthSourceInfo.vertices_count = 0;
			}
			best_match = depthstencil;
			best_info = depthstencil_info;
		}
		else {
			depthstencil_info.resolvedAA_source = nullptr;
			depthstencil_info.resolvedAA_dest = nullptr;
			depthstencil_info.drawcall_count = depthstencil_info.vertices_count = 0;
		}
	}

	if (best_match != nullptr)
	{
		if (best_match->depthSourceInfo.resolvedAA_dest)
			best_match = best_match->depthSourceInfo.resolvedAA_dest;
		if (best_match->depthstencil_replacement_resolvedAA && !((best_match->depthSourceInfo.resolvedAA_dest || best_match->depthSourceInfo.resolvedAA_source)))
		{
			best_match->depthstencil_replacement_resolvedAA = false;
			best_match->depthstencil_replacement_texture->GetSurfaceLevel(0, &best_match->depthstencil_replacement_surface);
		}
		if (depthstencil_replacement != best_match)
		{
			if (!best_match->depthstencil_replacement_surface)
				CreateDepthStencilReplacement(best_match, (best_match->depthSourceInfo.resolvedAA_dest || best_match->depthSourceInfo.resolvedAA_source));
			else {
				depthstencil_replacement = best_match;
			}
		}
	}
	best_match->depthSourceInfo.resolvedAA_source = nullptr;
	best_match->depthSourceInfo.resolvedAA_dest = nullptr;
	best_match->depthSourceInfo.drawcall_count = best_match->depthSourceInfo.vertices_count = 0;
}

inline bool D3D9Wrapper::IDirect3DDevice9::CreateDepthStencilReplacement(D3D9Wrapper::IDirect3DSurface9 * depthStencil, bool resolvedAA)
{
	if (depthStencil != nullptr)
	{
		::D3DSURFACE_DESC desc;
		depthStencil->GetD3DSurface9()->GetDesc(&desc);

		depthstencil_replacement = depthStencil;
		if (desc.Format == D3DFMT_INTZ){
			depthstencil_replacement->depthstencil_replacement_direct_sample = true;
			depthstencil_replacement->depthstencil_replacement_surface = reinterpret_cast<::IDirect3DSurface9*>(depthStencil->GetRealOrig());
			return true;
		}
		HRESULT hr = GetD3D9Device()->CreateTexture(desc.Width, desc.Height, 1, D3DUSAGE_DEPTHSTENCIL, D3DFMT_INTZ, ::D3DPOOL_DEFAULT, &depthstencil_replacement->depthstencil_replacement_texture, nullptr);
		if (SUCCEEDED(hr))
		{
			++migotoResourceCount;
			if (desc.MultiSampleType != ::D3DMULTISAMPLE_NONE) {
				depthstencil_replacement->depthstencil_replacement_multisampled = true;
				depthstencil_replacement->depthstencil_replacement_surface = reinterpret_cast<::IDirect3DSurface9*>(depthstencil_replacement->GetRealOrig());
				if (m_pD3D->m_isRESZ) {
					hr = GetD3D9Device()->CreateRenderTarget(desc.Width, desc.Height, NULL_CODE, desc.MultiSampleType, desc.MultiSampleQuality, false, &depthstencil_replacement->depthstencil_multisampled_rt_surface, nullptr);
					if (!FAILED(hr))
						++migotoResourceCount;
				}
				else {
					if (!(NVAPI_OK == NvAPI_D3D9_RegisterResource(depthstencil_replacement->depthstencil_replacement_surface)))
						hr = E_FAIL;
					if (!(NVAPI_OK == NvAPI_D3D9_RegisterResource(depthstencil_replacement->depthstencil_replacement_texture)))
						hr = E_FAIL;
					if (!FAILED(hr))
						depthstencil_replacement->depthstencil_replacement_nvapi_registered = true;
				}
			}
			else if (resolvedAA) {
				depthstencil_replacement->depthstencil_replacement_resolvedAA = true;
				depthstencil_replacement->depthstencil_replacement_surface = reinterpret_cast<::IDirect3DSurface9*>(depthstencil_replacement->GetRealOrig());
				if (!(NVAPI_OK == NvAPI_D3D9_RegisterResource(depthstencil_replacement->depthstencil_replacement_surface)))
					hr = E_FAIL;
				if (!(NVAPI_OK == NvAPI_D3D9_RegisterResource(depthstencil_replacement->depthstencil_replacement_texture)))
					hr = E_FAIL;
				if (!FAILED(hr))
					depthstencil_replacement->depthstencil_replacement_nvapi_registered = true;
			}
			else {
				hr = depthstencil_replacement->depthstencil_replacement_texture->GetSurfaceLevel(0, &depthstencil_replacement->depthstencil_replacement_surface);
			}
			if (m_pActiveDepthStencil != nullptr)
			{
				if (m_pActiveDepthStencil == depthstencil_replacement)
				{
					GetD3D9Device()->SetDepthStencilSurface(depthstencil_replacement->depthstencil_replacement_surface);
				}
			}
			if (!depthstencil_replacement->depthstencil_replacement_multisampled && !depthstencil_replacement->depthstencil_replacement_resolvedAA)
				depthstencil_replacement->depthstencil_replacement_surface->Release();
		}
		else
		{
			LogDebug("Failed to create depth replacement texture! HRESULT is=%d \n",
				hr);
			return false;
		}
	}
	return true;
}
template<class It>
It myadvance(It it, size_t n) {
	std::advance(it, n);
	return it;
}
template<class Cont>
void resize_container(Cont & cont, size_t n) {
	cont.erase(myadvance(cont.begin(), min(n, cont.size())),
		cont.end());
}
void D3D9Wrapper::IDirect3DDevice9::InitIniParams()
{
	// The command list only changes ini params that are defined, but for
	// consistency we want all other ini params to be initialised as well:
	//memset(G->IniConstants.data(), 0, sizeof(float4) * G->IniConstants.size());
	for (map<int, DirectX::XMFLOAT4>::iterator it = G->IniConstants.begin(); it != G->IniConstants.end(); ++it) {
		it->second = DirectX::XMFLOAT4();
	}

	// The command list will take care of initialising any non-zero values:
	CachedStereoValues cachedStereoValues;
	RunCommandList(this, &G->constants_command_list, NULL, false, &cachedStereoValues);
	// We don't consider persistent globals set in the [Constants] pre
	// command list as making the user config file dirty, because this
	// command list includes the user config file's [Constants] itself:
	G->user_config_dirty = false;
	RunCommandList(this, &G->post_constants_command_list, NULL, true, &cachedStereoValues);

}

inline bool D3D9Wrapper::IDirect3DDevice9::SwitchDrawingSide()
{
	bool switched = false;
	if (currentRenderingSide == RenderPosition::Left) {
		switched = SetDrawingSide(RenderPosition::Right);
	}
	else if (currentRenderingSide == RenderPosition::Right) {
		switched = SetDrawingSide(RenderPosition::Left);
	}
	return switched;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::DrawPrimitive(THIS_ ::D3DPRIMITIVETYPE PrimitiveType, UINT StartVertex, UINT PrimitiveCount)
{
	LogDebug("IDirect3DDevice9::DrawPrimitive called with PrimitiveType=%d, StartVertex=%d, PrimitiveCount=%d\n",
		PrimitiveType, StartVertex, PrimitiveCount);

	CheckDevice(this);
	DrawContext c = DrawContext(DrawCall::Draw, PrimitiveType, PrimitiveCount, StartVertex, 0, 0, 0, 0, NULL, 0, NULL, ::D3DFORMAT(-1), 0, NULL, NULL, NULL);
	BeforeDraw(c);

	HRESULT hr;
	if (!c.call_info.skip) {
		hr = GetD3D9Device()->DrawPrimitive(PrimitiveType, StartVertex, PrimitiveCount);
		if (G->gForceStereo == 2 && !FAILED(hr)){
			if (SwitchDrawingSide()) {
				hr = GetD3D9Device()->DrawPrimitive(PrimitiveType, StartVertex, PrimitiveCount);
				if (FAILED(hr))
					LogDebug("IDirect3DDevice9::DrawPrimitive Direct Mode, other side draw failed = %d", hr);
			}
		}
		LogDebug("  returns result=%x\n", hr);
	}
	else {
		hr = D3D_OK;
	}

	AfterDraw(c);

	return hr;

}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::DrawIndexedPrimitive(THIS_ ::D3DPRIMITIVETYPE Type,INT BaseVertexIndex,UINT MinVertexIndex,UINT NumVertices,UINT startIndex,UINT primCount)
{
	LogDebug("IDirect3DDevice9::DrawIndexedPrimitive called with Type=%d, BaseVertexIndex=%d, MinVertexIndex=%d, NumVertices=%d, startIndex=%d, primCount=%d\n",
		Type, BaseVertexIndex, MinVertexIndex, NumVertices, startIndex, primCount);

	CheckDevice(this);

	DrawContext c = DrawContext(DrawCall::DrawIndexed, Type, primCount, 0, BaseVertexIndex, MinVertexIndex, NumVertices, startIndex, NULL, 0, NULL, ::D3DFORMAT(-1), 0, NULL, NULL, NULL);
	BeforeDraw(c);
	HRESULT hr;
	if (!c.call_info.skip) {
		hr = GetD3D9Device()->DrawIndexedPrimitive(Type, BaseVertexIndex, MinVertexIndex, NumVertices, startIndex, primCount);
		if (G->gForceStereo == 2 && !FAILED(hr)){
			if (SwitchDrawingSide()) {
				hr = GetD3D9Device()->DrawIndexedPrimitive(Type, BaseVertexIndex, MinVertexIndex, NumVertices, startIndex, primCount);
				if (FAILED(hr))
					LogDebug("IDirect3DDevice9::DrawIndexedPrimitive Direct Mode, other side draw failed = %d", hr);
			}
		}
	}
	else {

		hr = D3D_OK;
	}

	AfterDraw(c);
	LogDebug("  returns result=%x\n", hr);

	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::DrawPrimitiveUP(THIS_ ::D3DPRIMITIVETYPE PrimitiveType,UINT PrimitiveCount,CONST void* pVertexStreamZeroData,UINT VertexStreamZeroStride)
{
	LogDebug("IDirect3DDevice9::DrawPrimitiveUP called.\n");

	CheckDevice(this);
	DrawContext c = DrawContext(DrawCall::DrawUP, PrimitiveType, PrimitiveCount, 0, 0, 0, 0, 0, pVertexStreamZeroData, VertexStreamZeroStride, NULL, ::D3DFORMAT(-1), 0, NULL, NULL, NULL);
	BeforeDraw(c);

	HRESULT hr;
	if (!c.call_info.skip) {
		hr = GetD3D9Device()->DrawPrimitiveUP(PrimitiveType, PrimitiveCount, pVertexStreamZeroData, VertexStreamZeroStride);
		if (G->gForceStereo == 2 && !FAILED(hr)){
			if (SwitchDrawingSide()) {
				hr = GetD3D9Device()->DrawPrimitiveUP(PrimitiveType, PrimitiveCount, pVertexStreamZeroData, VertexStreamZeroStride);
				if (FAILED(hr))
					LogDebug("IDirect3DDevice9::DrawPrimitiveUP Direct Mode, other side draw failed = %d", hr);
			}
		}
		LogDebug("  returns result=%x\n", hr);
	}
	else {
		hr = D3D_OK;
	}

	AfterDraw(c);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::DrawIndexedPrimitiveUP(THIS_ ::D3DPRIMITIVETYPE PrimitiveType,UINT MinVertexIndex,UINT NumVertices,UINT PrimitiveCount,CONST void* pIndexData,::D3DFORMAT IndexDataFormat,CONST void* pVertexStreamZeroData,UINT VertexStreamZeroStride)
{
	LogDebug("IDirect3DDevice9::DrawIndexedPrimitiveUP called with PrimitiveType=%d, MinVertexIndex=%d, NumVertices=%d, PrimitiveCount=%d, IndexDataFormat=%d\n",
		PrimitiveType, MinVertexIndex, NumVertices, PrimitiveCount, IndexDataFormat);

	CheckDevice(this);
	DrawContext c = DrawContext(DrawCall::DrawIndexedUP, PrimitiveType, PrimitiveCount, 0, 0, MinVertexIndex, NumVertices, 0, pVertexStreamZeroData, VertexStreamZeroStride, pIndexData, IndexDataFormat, 0, NULL, NULL, NULL);
	BeforeDraw(c);

	HRESULT hr;
	if (!c.call_info.skip) {
		hr = GetD3D9Device()->DrawIndexedPrimitiveUP(PrimitiveType, MinVertexIndex, NumVertices, PrimitiveCount, pIndexData, IndexDataFormat, pVertexStreamZeroData, VertexStreamZeroStride);
		if (G->gForceStereo == 2 && !FAILED(hr)) {
			if (SwitchDrawingSide()) {
				hr = GetD3D9Device()->DrawIndexedPrimitiveUP(PrimitiveType, MinVertexIndex, NumVertices, PrimitiveCount, pIndexData, IndexDataFormat, pVertexStreamZeroData, VertexStreamZeroStride);
				if (FAILED(hr))
					LogDebug("IDirect3DDevice9::DrawIndexedPrimitiveUP Direct Mode, other side draw failed = %d", hr);
			}
		}
		LogDebug("  returns result=%x\n", hr);
	}
	else {
		hr = D3D_OK;
	}
	AfterDraw(c);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::ProcessVertices(THIS_ UINT SrcStartIndex,UINT DestIndex,UINT VertexCount, D3D9Wrapper::IDirect3DVertexBuffer9 *pDestBuffer, D3D9Wrapper::IDirect3DVertexDeclaration9* pVertexDecl,DWORD Flags)
{
	LogDebug("IDirect3DDevice9::ProcessVertices called with SrcStartIndex=%d, DestIndex=%d, VertexCount=%d, Flags=%x\n",
		SrcStartIndex, DestIndex, VertexCount, Flags);

	CheckDevice(this);
	CheckVertexBuffer9(pDestBuffer);
	HRESULT hr = GetD3D9Device()->ProcessVertices(SrcStartIndex, DestIndex, VertexCount, baseVertexBuffer9(pDestBuffer), baseVertexDeclaration9(pVertexDecl), Flags);
	LogDebug("  returns result=%x\n", hr);

	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::CreateVertexDeclaration(THIS_ CONST ::D3DVERTEXELEMENT9* pVertexElements, D3D9Wrapper::IDirect3DVertexDeclaration9** ppDecl)
{
	LogDebug("IDirect3DDevice9::CreateVertexDeclaration called.\n");

	if (!GetD3D9Device())
	{
		LogInfo("  postponing call because device was not created yet.\n");

		D3D9Wrapper::IDirect3DVertexDeclaration9 *wrapper = D3D9Wrapper::IDirect3DVertexDeclaration9::GetDirect3DVertexDeclaration9((::LPDIRECT3DVERTEXDECLARATION9) 0, this);
		if (pVertexElements)
			wrapper->_VertexElements = *pVertexElements;
		wrapper->pendingDevice = this;
		LogInfo("  returns handle=%p\n", wrapper);

		return S_OK;
	}

	::LPDIRECT3DVERTEXDECLARATION9 baseVertexDeclaration = 0;
	HRESULT hr = GetD3D9Device()->CreateVertexDeclaration(pVertexElements, &baseVertexDeclaration);

	if (baseVertexDeclaration) {
		D3D9Wrapper::IDirect3DVertexDeclaration9 * wrappedVertexBuffer = IDirect3DVertexDeclaration9::GetDirect3DVertexDeclaration9(baseVertexDeclaration, this);
		if (ppDecl) {
			if (!(G->enable_hooks >= EnableHooksDX9::ALL)) {
				*ppDecl = wrappedVertexBuffer;
			}
			else {
				*ppDecl = reinterpret_cast<D3D9Wrapper::IDirect3DVertexDeclaration9*>(baseVertexDeclaration);
			}
		}
	}
	if (ppDecl) LogDebug("  returns result=%x, handle=%p, wrapper=%p\n", hr, baseVertexDeclaration, *ppDecl);

	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::SetVertexDeclaration(THIS_ D3D9Wrapper::IDirect3DVertexDeclaration9* pDecl)
{
	LogDebug("IDirect3DDevice9::SetVertexDeclaration called.\n");

	CheckDevice(this);
	HRESULT hr;
	::IDirect3DVertexDeclaration9 *baseVDeclaration = baseVertexDeclaration9(pDecl);
	D3D9Wrapper::IDirect3DVertexDeclaration9 *wrappedVDeclaration = wrappedVertexDeclaration9(pDecl);
	hr = GetD3D9Device()->SetVertexDeclaration(baseVDeclaration);
	if (!FAILED(hr)) {
		if (wrappedVDeclaration != m_pActiveVertexDeclaration) {
			if (wrappedVDeclaration)
				wrappedVDeclaration->Bound();
			if (m_pActiveVertexDeclaration)
				m_pActiveVertexDeclaration->Unbound();
			m_pActiveVertexDeclaration = wrappedVDeclaration;
		}
	}
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::GetVertexDeclaration(THIS_ D3D9Wrapper::IDirect3DVertexDeclaration9** ppDecl)
{
	LogDebug("IDirect3DDevice9::GetVertexDeclaration called.\n");

	CheckDevice(this);
	HRESULT hr;
	::LPDIRECT3DVERTEXDECLARATION9 baseVertexDeclaration = 0;
	if (!ppDecl) {
		hr = D3DERR_INVALIDCALL;
	}
	else {
		if (m_pActiveVertexDeclaration) {
			baseVertexDeclaration = m_pActiveVertexDeclaration->GetD3DVertexDeclaration9();
			m_pActiveVertexDeclaration->AddRef();
			hr = S_OK;
			if (!(G->enable_hooks >= EnableHooksDX9::ALL)) {
				*ppDecl = m_pActiveVertexDeclaration;
			}
			else {
				*ppDecl = reinterpret_cast<D3D9Wrapper::IDirect3DVertexDeclaration9*>(m_pActiveVertexDeclaration->GetRealOrig());
			}
		}
		else {
			*ppDecl = NULL;
			hr = S_OK;
		}
	}
	if (ppDecl) LogDebug("  returns result=%x, handle=%p, wrapper=%p\n", hr, baseVertexDeclaration, *ppDecl);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::SetFVF(THIS_ DWORD FVF)
{
	LogDebug("IDirect3DDevice9::SetFVF called.\n");

	CheckDevice(this);
	HRESULT hr = GetD3D9Device()->SetFVF(FVF);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::GetFVF(THIS_ DWORD* pFVF)
{
	LogDebug("IDirect3DDevice9::GetFVF called.\n");

	CheckDevice(this);
	return GetD3D9Device()->GetFVF(pFVF);
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::CreateVertexShader(THIS_ CONST DWORD* pFunction, D3D9Wrapper::IDirect3DVertexShader9 **ppShader)
{
	LogDebug("IDirect3DDevice9::CreateVertexShader called.\n");

	CheckDevice(this);

	::IDirect3DVertexShader9 *baseShader = 0;
	D3D9Wrapper::IDirect3DVertexShader9 * wrappedVertexShader = 0;
	HRESULT hr = CreateShader<D3D9Wrapper::IDirect3DVertexShader9, ::IDirect3DVertexShader9, &IDirect3DVertexShader9::GetDirect3DVertexShader9, &::IDirect3DDevice9::CreateVertexShader>
		(pFunction, &baseShader, L"vs", &wrappedVertexShader);
	if (ppShader) {
		if (!(G->enable_hooks >= EnableHooksDX9::ALL)) {
			*ppShader = wrappedVertexShader;
		}
		else {
			*ppShader = reinterpret_cast<D3D9Wrapper::IDirect3DVertexShader9*>(baseShader);
		}
	}
	if (ppShader) LogDebug("  returns result=%x, handle=%p, wrapper=%p\n", hr, baseShader, *ppShader);

	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::SetVertexShader(THIS_ D3D9Wrapper::IDirect3DVertexShader9 *pShader)
{
	LogDebug("IDirect3DDevice9::SetVertexShader called.\n");

	CheckDevice(this);
	::IDirect3DVertexShader9 *baseShader = baseVertexShader9(pShader);
	D3D9Wrapper::IDirect3DVertexShader9 *wrappedShader = wrappedVertexShader9(pShader);
	HRESULT hr = SetShader<D3D9Wrapper::IDirect3DVertexShader9, ::IDirect3DVertexShader9, &::IDirect3DDevice9::SetVertexShader>
		(wrappedShader,
			&G->mVisitedVertexShaders,
			G->mSelectedVertexShader,
			&mCurrentVertexShaderHandle);
	LogDebug("  returns result=%x\n", hr);

	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::GetVertexShader(THIS_ D3D9Wrapper::IDirect3DVertexShader9** ppShader)
{
	LogDebug("IDirect3DDevice9::GetVertexShader called.\n");

	CheckDevice(this);
	::IDirect3DVertexShader9 *baseShader = 0;
	HRESULT hr;
	if (!ppShader) {
		hr = D3DERR_INVALIDCALL;
	}
	else {
		if (mCurrentVertexShaderHandle) {
			baseShader = mCurrentVertexShaderHandle->GetD3DVertexShader9();
			mCurrentVertexShaderHandle->AddRef();
			hr = S_OK;
			if (!(G->enable_hooks >= EnableHooksDX9::ALL)) {
				*ppShader = mCurrentVertexShaderHandle;
			}
			else {
				*ppShader = reinterpret_cast<D3D9Wrapper::IDirect3DVertexShader9*>(mCurrentVertexShaderHandle->GetRealOrig());
			}
		}
		else {
			*ppShader = NULL;
			hr = S_OK;
		}
	}
	if (ppShader) LogDebug("  returns result=%x, handle=%p, wrapper=%p\n", hr, baseShader, *ppShader);

	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::SetVertexShaderConstantF(THIS_ UINT StartRegister,CONST float* pConstantData,UINT Vector4fCount)
{
	LogDebug("IDirect3DDevice9::SetVertexShaderConstantF called.\n");
	CheckDevice(this);
	map<int, DirectX::XMFLOAT4>::iterator it;
	for (it = G->IniConstants.begin(); it != G->IniConstants.end(); it++)
	{
		if ((UINT)it->first >= StartRegister && (UINT)it->first < (StartRegister + Vector4fCount)) {
			LogInfo("  set vertex float overriding ini params, constant reg: %i\n", it->first);
		}
	}
	HRESULT hr = GetD3D9Device()->SetVertexShaderConstantF(StartRegister, pConstantData, Vector4fCount);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::GetVertexShaderConstantF(THIS_ UINT StartRegister,float* pConstantData,UINT Vector4fCount)
{
	LogDebug("IDirect3DDevice9::GetVertexShaderConstantF called.\n");
	CheckDevice(this);
	return GetD3D9Device()->GetVertexShaderConstantF(StartRegister, pConstantData, Vector4fCount);
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::SetVertexShaderConstantI(THIS_ UINT StartRegister,CONST int* pConstantData,UINT Vector4iCount)
{
	LogDebug("IDirect3DDevice9::SetVertexShaderConstantI called.\n");
	CheckDevice(this);
	HRESULT hr = GetD3D9Device()->SetVertexShaderConstantI(StartRegister, pConstantData, Vector4iCount);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::GetVertexShaderConstantI(THIS_ UINT StartRegister,int* pConstantData,UINT Vector4iCount)
{
	LogDebug("IDirect3DDevice9::GetVertexShaderConstantI called.\n");
	CheckDevice(this);
	return GetD3D9Device()->GetVertexShaderConstantI(StartRegister, pConstantData, Vector4iCount);
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::SetVertexShaderConstantB(THIS_ UINT StartRegister,CONST BOOL* pConstantData, UINT BoolCount)
{
	LogDebug("IDirect3DDevice9::SetVertexShaderConstantB called.\n");
	CheckDevice(this);
	HRESULT hr = GetD3D9Device()->SetVertexShaderConstantB(StartRegister, pConstantData, BoolCount);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::GetVertexShaderConstantB(THIS_ UINT StartRegister,BOOL* pConstantData,UINT BoolCount)
{
	LogDebug("IDirect3DDevice9::GetVertexShaderConstantB called.\n");
	CheckDevice(this);
	return GetD3D9Device()->GetVertexShaderConstantB(StartRegister, pConstantData, BoolCount);
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::SetStreamSource(THIS_ UINT StreamNumber, D3D9Wrapper::IDirect3DVertexBuffer9 *pStreamData, UINT OffsetInBytes,UINT Stride)
{
	LogDebug("IDirect3DDevice9::SetStreamSource called.\n");

	CheckDevice(this);

	::IDirect3DVertexBuffer9 *baseVertexBuffer = baseVertexBuffer9(pStreamData);
	D3D9Wrapper::IDirect3DVertexBuffer9 *wrappedVertexBuffer = wrappedVertexBuffer9(pStreamData);
	HRESULT hr = GetD3D9Device()->SetStreamSource(StreamNumber, baseVertexBuffer, OffsetInBytes, Stride);
	if (SUCCEEDED(hr)) {
		if (wrappedVertexBuffer) {
			auto it = m_activeVertexBuffers.find(StreamNumber);
			if (it != m_activeVertexBuffers.end()) {
				if (wrappedVertexBuffer != it->second.m_vertexBuffer) {
					D3D9Wrapper::activeVertexBuffer avb;
					avb.m_offsetInBytes = OffsetInBytes;
					avb.m_pStride = Stride;
					avb.m_vertexBuffer = wrappedVertexBuffer;
					wrappedVertexBuffer->Bound();
					it->second.m_vertexBuffer->Unbound();
					it->second = avb;
				}
				else {
					it->second.m_pStride = Stride;
					it->second.m_offsetInBytes = OffsetInBytes;
				}
			}
			else {
				D3D9Wrapper::activeVertexBuffer avb;
				avb.m_offsetInBytes = OffsetInBytes;
				avb.m_pStride = Stride;
				avb.m_vertexBuffer = wrappedVertexBuffer;
				wrappedVertexBuffer->Bound();
				m_activeVertexBuffers.emplace(StreamNumber, avb);
			}
		}
		else {
			auto it = m_activeVertexBuffers.find(StreamNumber);
			if (it != m_activeVertexBuffers.end()) {
				it->second.m_vertexBuffer->Unbound();
				m_activeVertexBuffers.erase(it);
			}
		}
	}

	if (G->hunting == HUNTING_MODE_ENABLED) {
		EnterCriticalSection(&G->mCriticalSection);
		if (baseVertexBuffer) {
			mCurrentVertexBuffers[StreamNumber] = GetResourceHash(wrappedVertexBuffer);
			G->mVisitedVertexBuffers.insert(mCurrentVertexBuffers[StreamNumber]);
		}
		else
			mCurrentVertexBuffers[StreamNumber] = 0;
		LeaveCriticalSection(&G->mCriticalSection);
	}
	LogDebug("  returns result=%x\n", hr);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::GetStreamSource(THIS_ UINT StreamNumber, D3D9Wrapper::IDirect3DVertexBuffer9 **ppStreamData,UINT* pOffsetInBytes,UINT* pStride)
{
	LogDebug("IDirect3DDevice9::GetStreamSource called.\n");

	CheckDevice(this);
	::LPDIRECT3DVERTEXBUFFER9 baseVB = 0;
	HRESULT hr;
	if (!ppStreamData || !pOffsetInBytes || !pStride) {
		hr = D3DERR_INVALIDCALL;
	}
	else {
		auto it = m_activeVertexBuffers.find(StreamNumber);
		if (it == m_activeVertexBuffers.end()) {
			hr = S_OK;
			*ppStreamData = NULL;
			*pOffsetInBytes = 0;
			*pStride = 0;
		}
		else {
			activeVertexBuffer avb = it->second;
			baseVB = avb.m_vertexBuffer->GetD3DVertexBuffer9();
			avb.m_vertexBuffer->AddRef();
			hr = S_OK;
			*pOffsetInBytes = avb.m_offsetInBytes;
			*pStride = avb.m_pStride;
			if (!(G->enable_hooks >= EnableHooksDX9::ALL)) {
				*ppStreamData = avb.m_vertexBuffer;
			}
			else {
				*ppStreamData = reinterpret_cast<D3D9Wrapper::IDirect3DVertexBuffer9*>(avb.m_vertexBuffer->GetRealOrig());
			}
		}
	}
	if (ppStreamData) LogDebug("  returns hr=%x, handle=%p, wrapper=%p\n", hr, baseVB, *ppStreamData);
    return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::SetStreamSourceFreq(THIS_ UINT StreamNumber, UINT FrequencyParameter)
{
	LogDebug("IDirect3DDevice9::SetStreamSourceFreq called.\n");

	CheckDevice(this);
	HRESULT hr = GetD3D9Device()->SetStreamSourceFreq(StreamNumber, FrequencyParameter);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::GetStreamSourceFreq(THIS_ UINT StreamNumber,UINT* pSetting)
{
	LogDebug("IDirect3DDevice9::GetStreamSourceFreq called.\n");

	CheckDevice(this);
	return GetD3D9Device()->GetStreamSourceFreq(StreamNumber, pSetting);
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::SetIndices(THIS_ IDirect3DIndexBuffer9 *pIndexData)
{
	LogDebug("IDirect3DDevice9::SetIndices called.\n");

	::IDirect3DIndexBuffer9 *baseIndexBuffer = baseIndexBuffer9(pIndexData);
	D3D9Wrapper::IDirect3DIndexBuffer9 *pIndexBuffer = wrappedIndexBuffer9(pIndexData);
	CheckDevice(this);
	HRESULT hr = GetD3D9Device()->SetIndices(baseIndexBuffer);
	if (SUCCEEDED(hr)) {
		if (pIndexBuffer != m_activeIndexBuffer) {
			if (pIndexBuffer)
				pIndexBuffer->Bound();
			if (m_activeIndexBuffer)
				m_activeIndexBuffer->Unbound();
			m_activeIndexBuffer = pIndexBuffer;
		}
	}
	LogDebug("  returns result=%x\n", hr);

	// This is only used for index buffer hunting nowadays since the
	// command list checks the hash on demand only when it is needed
	mCurrentIndexBuffer = 0;
	if (baseIndexBuffer && G->hunting == HUNTING_MODE_ENABLED) {
		mCurrentIndexBuffer = GetResourceHash(pIndexBuffer);
		if (mCurrentIndexBuffer) {
			// When hunting, save this as a visited index buffer to cycle through.
			EnterCriticalSection(&G->mCriticalSection);
			G->mVisitedIndexBuffers.insert(mCurrentIndexBuffer);
			LeaveCriticalSection(&G->mCriticalSection);
		}
	}
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::GetIndices(THIS_ D3D9Wrapper::IDirect3DIndexBuffer9 **ppIndexData)
{
	LogDebug("IDirect3DDevice9::GetIndices called.\n");

	CheckDevice(this);
	::LPDIRECT3DINDEXBUFFER9 baseIB = 0;
	HRESULT hr;
	if (!ppIndexData) {
		hr = D3DERR_INVALIDCALL;
	}
	else {
		if (!m_activeIndexBuffer) {
			*ppIndexData = NULL;
			hr = S_OK;
		}
		else {
			baseIB = m_activeIndexBuffer->GetD3DIndexBuffer9();
			m_activeIndexBuffer->AddRef();
			hr = S_OK;
			if (!(G->enable_hooks >= EnableHooksDX9::ALL)) {
				*ppIndexData = m_activeIndexBuffer;
			}
			else {
				*ppIndexData = reinterpret_cast<D3D9Wrapper::IDirect3DIndexBuffer9*>(m_activeIndexBuffer->GetRealOrig());

			}
		}
	}
	if (ppIndexData) LogDebug("  returns result=%x, handle=%p, wrapper=%p\n", hr, baseIB, *ppIndexData);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::CreatePixelShader(THIS_ CONST DWORD* pFunction, D3D9Wrapper::IDirect3DPixelShader9 **ppShader)
{
	LogDebug("IDirect3DDevice9::CreatePixelShader called.\n");

	CheckDevice(this);

	::IDirect3DPixelShader9 *baseShader = 0;
	D3D9Wrapper::IDirect3DPixelShader9 *wrappedShader = 0;

	HRESULT hr = CreateShader<D3D9Wrapper::IDirect3DPixelShader9, ::IDirect3DPixelShader9, &IDirect3DPixelShader9::GetDirect3DPixelShader9, &::IDirect3DDevice9::CreatePixelShader>
		(pFunction, &baseShader, L"ps", &wrappedShader);
	if (ppShader) {
		if (!(G->enable_hooks >= EnableHooksDX9::ALL)) {
			*ppShader = wrappedShader;
		}
		else {
			*ppShader = reinterpret_cast<D3D9Wrapper::IDirect3DPixelShader9*>(baseShader);
		}

	}
	if (ppShader) LogDebug("  returns result=%x, handle=%p, wrapper=%p\n", hr, baseShader, *ppShader);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::SetPixelShader(THIS_ IDirect3DPixelShader9 *pShader)
{
	LogDebug("IDirect3DDevice9::SetPixelShader called.\n");

	CheckDevice(this);
	::IDirect3DPixelShader9 *baseShader = basePixelShader9(pShader);
	D3D9Wrapper::IDirect3DPixelShader9 *wrappedShader = wrappedPixelShader9(pShader);
	HRESULT hr = SetShader<D3D9Wrapper::IDirect3DPixelShader9, ::IDirect3DPixelShader9, &::IDirect3DDevice9::SetPixelShader>
		(wrappedShader,
			&G->mVisitedPixelShaders,
			G->mSelectedPixelShader,
			&mCurrentPixelShaderHandle);
	LogDebug("  returns result=%x\n", hr);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::GetPixelShader(THIS_ D3D9Wrapper::IDirect3DPixelShader9 **ppShader)
{
	LogDebug("IDirect3DDevice9::GetPixelShader called.\n");

	CheckDevice(this);
	::IDirect3DPixelShader9 *baseShader = 0;
	HRESULT hr;

	if (!ppShader) {
		hr = D3DERR_INVALIDCALL;
	}
	else {
		if (mCurrentPixelShaderHandle) {
			baseShader = mCurrentPixelShaderHandle->GetD3DPixelShader9();
			mCurrentPixelShaderHandle->AddRef();
			hr = S_OK;
			if (!(G->enable_hooks >= EnableHooksDX9::ALL)) {
				*ppShader = mCurrentPixelShaderHandle;
			}
			else {
				*ppShader = reinterpret_cast<D3D9Wrapper::IDirect3DPixelShader9*>(mCurrentPixelShaderHandle->GetRealOrig());
			}
		}
		else {
			*ppShader = NULL;
			hr = S_OK;
		}
	}
	if (ppShader) LogDebug("  returns result=%x, handle=%p, wrapper=%p\n", hr, baseShader, *ppShader);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::SetPixelShaderConstantF(THIS_ UINT StartRegister,CONST float* pConstantData,UINT Vector4fCount)
{
	LogDebug("IDirect3DDevice9::SetPixelShaderConstantF called.\n");
	CheckDevice(this);
	map<int, DirectX::XMFLOAT4>::iterator it;

	for (it = G->IniConstants.begin(); it != G->IniConstants.end(); it++)
	{
		if ((UINT)it->first >= StartRegister && (UINT)it->first < (StartRegister + Vector4fCount)) {
			LogInfo("  set pixel float overriding ini params, constant reg: %i\n", it->first);
		}

	}
	HRESULT hr = GetD3D9Device()->SetPixelShaderConstantF(StartRegister, pConstantData, Vector4fCount);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::GetPixelShaderConstantF(THIS_ UINT StartRegister,float* pConstantData,UINT Vector4fCount)
{
	LogDebug("IDirect3DDevice9::GetPixelShaderConstantF called.\n");
	CheckDevice(this);
	return GetD3D9Device()->GetPixelShaderConstantF(StartRegister, pConstantData, Vector4fCount);
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::SetPixelShaderConstantI(THIS_ UINT StartRegister,CONST int* pConstantData,UINT Vector4iCount)
{
	LogDebug("IDirect3DDevice9::SetPixelShaderConstantI called.\n");
	CheckDevice(this);
	HRESULT hr = GetD3D9Device()->SetPixelShaderConstantI(StartRegister, pConstantData, Vector4iCount);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::GetPixelShaderConstantI(THIS_ UINT StartRegister,int* pConstantData,UINT Vector4iCount)
{
	LogDebug("IDirect3DDevice9::GetPixelShaderConstantI called.\n");
	CheckDevice(this);
	return GetD3D9Device()->GetPixelShaderConstantI(StartRegister, pConstantData, Vector4iCount);
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::SetPixelShaderConstantB(THIS_ UINT StartRegister,CONST BOOL* pConstantData, UINT BoolCount)
{
	LogDebug("IDirect3DDevice9::SetPixelShaderConstantB called.\n");
	CheckDevice(this);
	HRESULT hr = GetD3D9Device()->SetPixelShaderConstantB(StartRegister, pConstantData, BoolCount);
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::GetPixelShaderConstantB(THIS_ UINT StartRegister,BOOL* pConstantData,UINT BoolCount)
{
	LogDebug("IDirect3DDevice9::GetPixelShaderConstantB called.\n");
	CheckDevice(this);
	return GetD3D9Device()->GetPixelShaderConstantB(StartRegister, pConstantData, BoolCount);
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::DrawRectPatch(THIS_ UINT Handle,CONST float* pNumSegs,CONST ::D3DRECTPATCH_INFO* pRectPatchInfo)
{
	LogDebug("IDirect3DDevice9::DrawRectPatch called.\n");

	CheckDevice(this);
	DrawContext c = DrawContext(DrawCall::DrawRectPatch, ::D3DPRIMITIVETYPE(-1), 0, 0, 0, 0, 0, 0, NULL, 0, NULL, ::D3DFORMAT(-1), Handle, pNumSegs, pRectPatchInfo, NULL);
	BeforeDraw(c);
	HRESULT hr;
	if (!c.call_info.skip) {
		hr = GetD3D9Device()->DrawRectPatch(Handle, pNumSegs, pRectPatchInfo);
		if (G->gForceStereo == 2 && !FAILED(hr)){
			if (SwitchDrawingSide()) {
				hr = GetD3D9Device()->DrawRectPatch(Handle, pNumSegs, pRectPatchInfo);
				if (FAILED(hr))
					LogDebug("IDirect3DDevice9::DrawRectPatch Direct Mode, other side draw failed = %d", hr);
			}
		}
		LogDebug("  returns result=%x\n", hr);
	}
	else {
		hr = D3D_OK;
	}
	AfterDraw(c);

	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::DrawTriPatch(THIS_ UINT Handle,CONST float* pNumSegs,CONST ::D3DTRIPATCH_INFO* pTriPatchInfo)
{
	LogDebug("IDirect3DDevice9::DrawTriPatch called.\n");

	CheckDevice(this);
	DrawContext c = DrawContext(DrawCall::DrawTriPatch, ::D3DPRIMITIVETYPE(-1), 0, 0, 0, 0, 0, 0, NULL, 0, NULL, ::D3DFORMAT(-1), Handle, pNumSegs, NULL, pTriPatchInfo);
	BeforeDraw(c);
	HRESULT hr;
	if (!c.call_info.skip) {
		hr = GetD3D9Device()->DrawTriPatch(Handle, pNumSegs, pTriPatchInfo);
		if (G->gForceStereo == 2 && !FAILED(hr)){
			if (SwitchDrawingSide()) {
				hr = GetD3D9Device()->DrawTriPatch(Handle, pNumSegs, pTriPatchInfo);
				if (FAILED(hr))
					LogDebug("IDirect3DDevice9::DrawTriPatch Direct Mode, other side draw failed = %d", hr);
			}
		}
		LogDebug("  returns result=%x\n", hr);
	}
	else {
		hr = D3D_OK;
	}
	AfterDraw(c);

	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::DeletePatch(THIS_ UINT Handle)
{
	LogDebug("IDirect3DDevice9::DeletePatch called.\n");

	CheckDevice(this);
	return GetD3D9Device()->DeletePatch(Handle);
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::CreateQuery(THIS_ ::D3DQUERYTYPE Type, ::IDirect3DQuery9** ppQuery)
{
	LogDebug("IDirect3DDevice9::CreateQuery called with Type=%d, ppQuery=%p\n", Type, ppQuery);

	CheckDevice(this);

	::LPDIRECT3DQUERY9 baseQuery = 0;
	HRESULT hr = GetD3D9Device()->CreateQuery(Type, &baseQuery);
    if (FAILED(hr) || !baseQuery)
    {
		if (!gLogDebug) LogInfo("IDirect3DDevice9::CreateQuery called.\n");
		LogInfo("  failed with hr=%x\n", hr);

		if (ppQuery) *ppQuery = 0;
        return hr;
    }
	D3D9Wrapper::IDirect3DQuery9 * wrappedQuery = NULL;
	if (baseQuery) {
		wrappedQuery = IDirect3DQuery9::GetDirect3DQuery9(baseQuery, this);
		if (ppQuery) {
			if (!(G->enable_hooks >= EnableHooksDX9::ALL) && wrappedQuery) {
				*ppQuery = (::IDirect3DQuery9*)wrappedQuery;
			}
			else {
				*ppQuery = baseQuery;
			}
		}
	}
	if (ppQuery) LogDebug("  returns result=%x, handle=%p, wrapper=%p\n", hr, baseQuery, *ppQuery);
	if (!ppQuery) LogDebug("  returns result=%x, handle=%p\n", hr, baseQuery);

	if (G->hunting && SUCCEEDED(hr) && wrappedQuery)
		G->mQueries.push_back(wrappedQuery);

	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::GetDisplayModeEx(THIS_ UINT iSwapChain,::D3DDISPLAYMODEEX* pMode,::D3DDISPLAYROTATION* pRotation)
{
	LogDebug("IDirect3DDevice9::GetDisplayModeEx called.\n");

	CheckDevice(this);
	HRESULT hr = GetD3D9DeviceEx()->GetDisplayModeEx(iSwapChain, pMode, pRotation);
	if (hr == S_OK && pMode)
	{
		//if (G->UPSCALE_MODE > 0) {
		//	if (G->GAME_INTERNAL_WIDTH() > 1)
		//		pMode->Width = G->GAME_INTERNAL_WIDTH();
		//	if (G->GAME_INTERNAL_HEIGHT() > 1)
		//		pMode->Height = G->GAME_INTERNAL_HEIGHT();
		//}
		//if (G->SCREEN_REFRESH != 1 && pMode->RefreshRate != _pOrigPresentationParameters.FullScreen_RefreshRateInHz)
		//	pMode->RefreshRate = _pOrigPresentationParameters.FullScreen_RefreshRateInHz;
		//if (G->SCREEN_REFRESH != -1 && pMode->RefreshRate != G->SCREEN_REFRESH)
		//{
		//	LogInfo("  overriding refresh rate %d with %d\n", pMode->RefreshRate, G->SCREEN_REFRESH);

		//	pMode->RefreshRate = G->SCREEN_REFRESH;
		//}
		//if (G->GAME_INTERNAL_WIDTH() > 1 && G->FORCE_REPORT_GAME_RES) {
		//	pMode->Width = G->GAME_INTERNAL_WIDTH();
		//}
		////else if (G->FORCE_REPORT_WIDTH > 0) {
		////	pMode->Width = G->FORCE_REPORT_WIDTH;
		////}
		//if (G->GAME_INTERNAL_HEIGHT() > 1 && G->FORCE_REPORT_GAME_RES) {
		//	pMode->Height = G->GAME_INTERNAL_HEIGHT();
		//}
		////else if (G->FORCE_REPORT_HEIGHT > 0) {
		////	pMode->Height = G->FORCE_REPORT_HEIGHT;
		////}
	}
	if (!pMode) LogInfo("  returns result=%x\n", hr);
	if (pMode) LogInfo("  returns result=%x, Width=%d, Height=%d, RefreshRate=%d, Format=%d\n", hr,
		pMode->Width, pMode->Height, pMode->RefreshRate, pMode->Format);

	return hr;
}


STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::CheckDeviceState(HWND hWindow)
{
	LogDebug("IDirect3DDevice9::CheckDeviceState called.\n");

	CheckDevice(this);
	return GetD3D9DeviceEx()->CheckDeviceState(hWindow);
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::CheckResourceResidency(::IDirect3DResource9 ** pResourceArray, UINT32 NumResources)
{
	LogDebug("IDirect3DDevice9::CheckResourceResidency called.\n");

	CheckDevice(this);
	return GetD3D9DeviceEx()->CheckResourceResidency(pResourceArray, NumResources);
}
HRESULT DirectModeComposeRectsRight(::IDirect3DDevice9Ex *origDevice, D3D9Wrapper::IDirect3DSurface9* wrappedSrcSur, D3D9Wrapper::IDirect3DSurface9* wrappedDstSur, ::IDirect3DVertexBuffer9 *SrcVB, ::IDirect3DVertexBuffer9 *DstVB, UINT NumRects, ::D3DCOMPOSERECTSOP Operation, INT XOffset, INT YOffset) {
	HRESULT hr = D3D_OK;
	if (!wrappedSrcSur->DirectModeGetRight() && wrappedDstSur->DirectModeGetRight()) {
		LogDebug("IDirect3DDevice9::ComposeRects, Direct Mode, - Source is not stereo, destination is stereo. Copying source to both sides of destination.\n");
		hr = origDevice->ComposeRects(wrappedSrcSur->DirectModeGetLeft(), wrappedDstSur->DirectModeGetRight(), SrcVB, NumRects, DstVB, Operation, XOffset, YOffset);
		if (FAILED(hr))
			LogDebug("ERROR: ComposeRects - Failed to copy source left to destination right.\n");
	}
	else if (wrappedSrcSur->DirectModeGetRight() && !wrappedDstSur->DirectModeGetRight()) {
		LogDebug("INFO: ComposeRects - Source is stereo, destination is not stereo. Copied Left side only.\n");
	}
	else if (wrappedSrcSur->DirectModeGetRight() && wrappedDstSur->DirectModeGetRight()) {
		hr = origDevice->ComposeRects(wrappedSrcSur->DirectModeGetRight(), wrappedDstSur->DirectModeGetRight(), SrcVB, NumRects, DstVB, Operation, XOffset, YOffset);
		if (FAILED(hr))
			LogDebug("ERROR: ComposeRects - Failed to copy source right to destination right.\n");
	}
	return hr;
}
STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::ComposeRects(D3D9Wrapper::IDirect3DSurface9 * pSource, D3D9Wrapper::IDirect3DSurface9 * pDestination, D3D9Wrapper::IDirect3DVertexBuffer9 * pSrcRectDescriptors, UINT NumRects, D3D9Wrapper::IDirect3DVertexBuffer9 * pDstRectDescriptors, ::D3DCOMPOSERECTSOP Operation, INT XOffset, INT YOffset)
{
	LogDebug("IDirect3DDevice9::ComposeRects called.\n");
	::LPDIRECT3DSURFACE9 baseSourceSurface = baseSurface9(pSource);
	::LPDIRECT3DSURFACE9 baseDestinationSurface = baseSurface9(pDestination);
	::LPDIRECT3DVERTEXBUFFER9 baseSourceVertexBuffer = baseVertexBuffer9(pSrcRectDescriptors);
	::LPDIRECT3DVERTEXBUFFER9 baseDestinationVertexBuffer = baseVertexBuffer9(pDstRectDescriptors);
	CheckDevice(this);
	HRESULT hr;
	if (G->gForceStereo == 2){
		D3D9Wrapper::IDirect3DSurface9 *wrappedSrcSur = wrappedSurface9(pSource);
		D3D9Wrapper::IDirect3DSurface9 *wrappedDstSur = wrappedSurface9(pDestination);
		hr = GetD3D9DeviceEx()->ComposeRects(baseSourceSurface, baseDestinationSurface, baseSourceVertexBuffer, NumRects, baseDestinationVertexBuffer, Operation, XOffset, YOffset);
		hr = DirectModeComposeRectsRight(this->GetD3D9DeviceEx(), wrappedSrcSur, wrappedDstSur, baseSourceVertexBuffer, baseDestinationVertexBuffer, NumRects, Operation, XOffset, YOffset);
	}
	else {
		hr = GetD3D9DeviceEx()->ComposeRects(baseSourceSurface, baseDestinationSurface, baseSourceVertexBuffer, NumRects, baseDestinationVertexBuffer, Operation, XOffset, YOffset);
	}
	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::CreateDepthStencilSurfaceEx(UINT Width, UINT Height, ::D3DFORMAT Format, ::D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Discard, D3D9Wrapper::IDirect3DSurface9 ** ppSurface, HANDLE * pSharedHandle, DWORD Usage)
{
	LogInfo("IDirect3DDevice9::CreateDepthStencilSurfaceEx Width=%d Height=%d Format=%d Usage=%d\n", Width, Height, Format, Usage);
	D3D9Wrapper::IDirect3DSurface9 * wrapper;
	if (!GetD3D9Device())
	{
		LogInfo("  postponing call because device was not created yet.\n");
		wrapper = IDirect3DSurface9::GetDirect3DSurface9((::LPDIRECT3DSURFACE9) 0, this, NULL);
		wrapper->_Width = Width;
		wrapper->_Height = Height;
		wrapper->_Format = Format;
		wrapper->_MultiSample = MultiSample;
		wrapper->_MultisampleQuality = MultisampleQuality;
		wrapper->_Discard = Discard;
		wrapper->_Device = this;

		wrapper->_Usage = Usage;

		pendingCreateDepthStencilSurfaceEx = wrapper;
		*ppSurface = wrapper;
		LogInfo("  returns handle=%p\n", wrapper);

		return S_OK;
	}

	::LPDIRECT3DSURFACE9 baseSurface = NULL;

	D3D2DTEXTURE_DESC pDesc;
	pDesc.Usage = Usage;
	if (!(pDesc.Usage & D3DUSAGE_DEPTHSTENCIL))
		pDesc.Usage |= D3DUSAGE_DEPTHSTENCIL;
	if (Discard)
		pDesc.Usage |= D3DUSAGE_DYNAMIC;
	pDesc.Format = Format;
	pDesc.Height = Height;
	pDesc.Width = Width;
	pDesc.MultiSampleType = MultiSample;
	pDesc.MultiSampleQuality = MultisampleQuality;
	pDesc.Levels = 1;
	HRESULT hr;
	ResourceHandleInfo info;
	if (G->gForceStereo == 2) {
		::IDirect3DSurface9* pRightDepthStencil = NULL;
		// create left/mono
		hr = CreateSurface<::IDirect3DSurface9, D3D2DTEXTURE_DESC>(&baseSurface, &info, &pDesc, this, pSharedHandle, SurfaceCreation::DepthStencilEx);
		if (!FAILED(hr)) {
			if (ShouldDuplicate(&pDesc)) {
				hr = CreateSurface<::IDirect3DSurface9, D3D2DTEXTURE_DESC>(&pRightDepthStencil, &info, &pDesc, this, pSharedHandle, SurfaceCreation::DepthStencilEx);
				if (!FAILED(hr)) {
					wrapper = IDirect3DSurface9::GetDirect3DSurface9(baseSurface, this, pRightDepthStencil);
					wrapper->resourceHandleInfo = info;
				}
				else {
					LogDebug("IDirect3DDevice9::CreateDepthStencilSurfaceEx Direct Mode, failed to create right surface, falling back to mono, hr = %d", hr);
					wrapper = IDirect3DSurface9::GetDirect3DSurface9(baseSurface, this, NULL);
					wrapper->resourceHandleInfo = info;
				}
			}
			else {
				LogDebug("IDirect3DDevice9::CreateDepthStencilSurfaceEx Direct Mode, non stereo depth stencil");
				wrapper = IDirect3DSurface9::GetDirect3DSurface9(baseSurface, this, NULL);
				wrapper->resourceHandleInfo = info;
			}
		}
		else {
			LogDebug("IDirect3DDevice9::CreateDepthStencilSurfaceEx Direct Mode, failed to create left surface, hr = %d ", hr);
		}
	}
	else {
		hr = CreateSurface<::IDirect3DSurface9, D3D2DTEXTURE_DESC>(&baseSurface, &info, &pDesc, this, pSharedHandle, SurfaceCreation::DepthStencilEx);
		wrapper = IDirect3DSurface9::GetDirect3DSurface9(baseSurface, this, NULL);
		wrapper->resourceHandleInfo = info;
	}
	if (ppSurface) {
		if (!(G->enable_hooks >= EnableHooksDX9::ALL)) {
			*ppSurface = wrapper;
		}
		else {
			*ppSurface = reinterpret_cast<D3D9Wrapper::IDirect3DSurface9*>(baseSurface);
		}
	}
	if (ppSurface) LogInfo("  returns result=%x, handle=%p, wrapper=%p\n", hr, baseSurface, *ppSurface);

	return hr;
}
STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::CreateOffscreenPlainSurfaceEx(UINT Width, UINT Height, ::D3DFORMAT Format, ::D3DPOOL Pool, D3D9Wrapper::IDirect3DSurface9 ** ppSurface, HANDLE * pSharedHandle, DWORD Usage)
{
	LogInfo("IDirect3DDevice9::CreateOffscreenPlainSurfaceEx called with Width=%d Height=%d Usage=%d\n", Width, Height, Usage);

	CheckDevice(this);

	::LPDIRECT3DSURFACE9 baseSurface = NULL;
	if (G->gForwardToEx && Pool == ::D3DPOOL_MANAGED)
	{
		LogDebug("  Pool changed from MANAGED to DEFAULT because of DirectX9Ex migration.\n");

		Pool = ::D3DPOOL_DEFAULT;
	}

	D3D2DTEXTURE_DESC pDesc;
	pDesc.Usage = Usage;
	pDesc.Format = Format;
	pDesc.Height = Height;
	pDesc.Width = Width;
	pDesc.Pool = Pool;
	pDesc.Levels = 1;
	ResourceHandleInfo info;
	HRESULT hr = CreateSurface<::IDirect3DSurface9, D3D2DTEXTURE_DESC>(&baseSurface, &info, &pDesc, this, pSharedHandle, SurfaceCreation::OffscreenPlainEx);
	if (baseSurface) {
		D3D9Wrapper::IDirect3DSurface9 * wrapper;
		wrapper = IDirect3DSurface9::GetDirect3DSurface9(baseSurface, this, NULL);
		wrapper->resourceHandleInfo = info;
		if (ppSurface) {
			if (!(G->enable_hooks >= EnableHooksDX9::ALL)) {
				*ppSurface = wrapper;
			}
			else {
				*ppSurface = reinterpret_cast<D3D9Wrapper::IDirect3DSurface9*>(baseSurface);
			}
		}
	}

	if (ppSurface) LogInfo("  returns result=%x, handle=%p, wrapper=%p\n", hr, baseSurface, *ppSurface);
	return hr;

}
STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::CreateRenderTargetEx(UINT Width, UINT Height, ::D3DFORMAT Format, ::D3DMULTISAMPLE_TYPE MultiSample, DWORD MultisampleQuality, BOOL Lockable, D3D9Wrapper::IDirect3DSurface9 ** ppSurface, HANDLE * pSharedHandle, DWORD Usage)
{
	LogInfo("IDirect3DDevice9::CreateRenderTargetEx called with Width=%d Height=%d Format=%d Usage=%d\n", Width, Height, Format, Usage);

	CheckDevice(this);

	::LPDIRECT3DSURFACE9 baseSurface = NULL;

	D3D2DTEXTURE_DESC pDesc;
	pDesc.Usage = Usage;
	if (!(pDesc.Usage & D3DUSAGE_RENDERTARGET))
		pDesc.Usage |= D3DUSAGE_RENDERTARGET;
	if (Lockable)
		pDesc.Usage |= D3DUSAGE_DYNAMIC;
	pDesc.Format = Format;
	pDesc.Height = Height;
	pDesc.Width = Width;
	pDesc.MultiSampleType = MultiSample;
	pDesc.MultiSampleQuality = MultisampleQuality;
	pDesc.Levels = 1;
	HRESULT hr;
	D3D9Wrapper::IDirect3DSurface9 *wrapper;
	ResourceHandleInfo info;
	if (G->gForceStereo == 2) {
		::IDirect3DSurface9* pRightRenderTarget = NULL;
		// create left/mono
		hr = CreateSurface<::IDirect3DSurface9, D3D2DTEXTURE_DESC>(&baseSurface, &info, &pDesc, this, pSharedHandle, SurfaceCreation::RenderTargetEx);
		if (!FAILED(hr)) {
			if (ShouldDuplicate(&pDesc)) {
				hr = CreateSurface<::IDirect3DSurface9, D3D2DTEXTURE_DESC>(&pRightRenderTarget, &info, &pDesc, this, pSharedHandle, SurfaceCreation::RenderTargetEx);
				if (!FAILED(hr)) {
					wrapper = IDirect3DSurface9::GetDirect3DSurface9(baseSurface, this, pRightRenderTarget);
					wrapper->resourceHandleInfo = info;
				}
				else {
					LogDebug("IDirect3DDevice9::CreateRenderTargetEx Direct Mode, failed to create right surface, falling back to mono, hr = %d", hr);
					wrapper = IDirect3DSurface9::GetDirect3DSurface9(baseSurface, this, NULL);
					wrapper->resourceHandleInfo = info;
				}
			}
			else {
				LogDebug("IDirect3DDevice9::CreateRenderTargetEx Direct Mode, non stereo render target");
				wrapper = IDirect3DSurface9::GetDirect3DSurface9(baseSurface, this, NULL);
				wrapper->resourceHandleInfo = info;
			}
		}
		else {
			LogDebug("IDirect3DDevice9::CreateRenderTargetEx Direct Mode, failed to create left surface, hr = %d ", hr);
		}
	}
	else {
		hr = CreateSurface<::IDirect3DSurface9, D3D2DTEXTURE_DESC>(&baseSurface, &info, &pDesc, this, pSharedHandle, SurfaceCreation::RenderTargetEx);
		wrapper = IDirect3DSurface9::GetDirect3DSurface9(baseSurface, this, NULL);
		wrapper->resourceHandleInfo = info;
	}
	if (ppSurface) {
		if (!(G->enable_hooks >= EnableHooksDX9::ALL)) {
			*ppSurface = wrapper;
		}
		else {
			*ppSurface = reinterpret_cast<D3D9Wrapper::IDirect3DSurface9*>(baseSurface);
		}
	}
	if (ppSurface) LogInfo("  returns result=%x, handle=%p, wrapper=%p\n", hr, baseSurface, *ppSurface);

	return hr;
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::GetGPUThreadPriority(INT * pPriority)
{
	LogDebug("IDirect3DDevice9::GetGPUThreadPriority called.\n");

	CheckDevice(this);
	return GetD3D9DeviceEx()->GetGPUThreadPriority(pPriority);
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::GetMaximumFrameLatency(UINT * pMaxLatency)
{
	LogDebug("IDirect3DDevice9::GetMaximumFrameLatency called.\n");

	CheckDevice(this);
	return GetD3D9DeviceEx()->GetMaximumFrameLatency(pMaxLatency);
}

STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::PresentEx(RECT * pSourceRect, const RECT * pDestRect, HWND hDestWindowOverride, const RGNDATA * pDirtyRegion, DWORD dwFlags)
{
	LogDebug("IDirect3DDevice9::PresentEx called.\n");

	LogDebug("HackerDevice::PresentEx(%s@%p) called\n", type_name_dx9((IUnknown*)this), this);
	LogDebug("  pSourceRect = %p\n", pSourceRect);
	LogDebug("  pDestRect = %p\n", pDestRect);
	LogDebug("  hDestWindowOverride = %p\n", hDestWindowOverride);
	LogDebug("  pDirtyRegion = %p\n", pDirtyRegion);
	LogDebug("  dwFlags = %d\n", dwFlags);
	CheckDevice(this);
	Profiling::State profiling_state = { 0 };
	bool profiling = false;
	if (G->SCREEN_FULLSCREEN == 2)
	{
		if (!gLogDebug) LogInfo("IDirect3DDevice9::Present called.\n");
		LogInfo("  initiating reset to switch to full screen.\n");

		G->SCREEN_FULLSCREEN = 1;
		G->SCREEN_WIDTH = G->SCREEN_WIDTH_DELAY;
		G->SCREEN_HEIGHT = G->SCREEN_HEIGHT_DELAY;
		G->SCREEN_REFRESH = G->SCREEN_REFRESH_DELAY;
		return D3DERR_DEVICELOST;
	}
	// Profiling::mode may change below, so make a copy
	profiling = Profiling::mode == Profiling::Mode::SUMMARY;
	if (profiling)
		Profiling::start(&profiling_state);

	// Every presented frame, we want to take some CPU time to run our actions,
	// which enables hunting, and snapshots, and aiming overrides and other inputs
	CachedStereoValues prePresentcachedStereoValues;
	RunFrameActions(this, &prePresentcachedStereoValues);

	if (profiling)
		Profiling::end(&profiling_state, &Profiling::present_overhead);

	HRESULT hr = GetD3D9DeviceEx()->PresentEx(pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion, dwFlags);

	if (profiling)
		Profiling::start(&profiling_state);

	if (G->gTrackNvAPIStereoActive && !G->gTrackNvAPIStereoActiveDisableReset)
		NvAPIResetStereoActiveTracking();
	if (G->gTrackNvAPIConvergence && !G->gTrackNvAPIConvergenceDisableReset)
		NvAPIResetConvergenceTracking();
	if (G->gTrackNvAPISeparation && !G->gTrackNvAPISeparationDisableReset)
		NvAPIResetSeparationTracking();
	if (G->gTrackNvAPIEyeSeparation && !G->gTrackNvAPIEyeSeparationDisableReset)
		NvAPIResetEyeSeparationTracking();

	// Update the stereo params texture just after the present so that
	// shaders get the new values for the current frame:
	this->stereo_params_updated_this_frame = false;
	CachedStereoValues postPresentcachedStereoValues;
	UpdateStereoParams(false, &postPresentcachedStereoValues);
	G->bb_is_upscaling_bb = !!G->SCREEN_UPSCALING && G->upscaling_command_list_using_explicit_bb_flip;

	// Run the post present command list now, which can be used to restore
	// state changed in the pre-present command list, or to perform some
	// action at the start of a frame:
	GetD3D9Device()->BeginScene();
	RunCommandList(this, &G->post_present_command_list, NULL, true, &postPresentcachedStereoValues);
	GetD3D9Device()->EndScene();
	if (G->gAutoDetectDepthBuffer) {
		DetectDepthSource();
		drawCalls = vertices = 0;
	}
	if (profiling)
		Profiling::end(&profiling_state, &Profiling::present_overhead);
	if (hr == D3DERR_DEVICELOST)
	{
		LogInfo("  returns D3DERR_DEVICELOST\n");
	}
	else
	{
		LogDebug("  returns result=%x\n", hr);
	}
	return hr;
}

inline STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::SetConvolutionMonoKernel(UINT Width, UINT Height, float * RowWeights, float * ColumnWeights)
{
	LogDebug("IDirect3DDevice9::SetConvolutionMonoKernel called.\n");

	CheckDevice(this);
	return GetD3D9DeviceEx()->SetConvolutionMonoKernel(Width, Height, RowWeights, ColumnWeights);
}

inline STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::SetGPUThreadPriority(INT pPriority)
{
	LogDebug("IDirect3DDevice9::SetGPUThreadPriority called.\n");

	CheckDevice(this);
	return GetD3D9DeviceEx()->SetGPUThreadPriority(pPriority);
}

inline STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::SetMaximumFrameLatency(UINT pMaxLatency)
{
	LogDebug("IDirect3DDevice9::SetMaximumFrameLatency called.\n");

	CheckDevice(this);
	return GetD3D9DeviceEx()->SetMaximumFrameLatency(pMaxLatency);
}

inline STDMETHODIMP D3D9Wrapper::IDirect3DDevice9::WaitForVBlank(UINT SwapChainIndex)
{
	LogDebug("IDirect3DDevice9::WaitForVBlank called.\n");

	CheckDevice(this);
	return GetD3D9DeviceEx()->WaitForVBlank(SwapChainIndex);
}
