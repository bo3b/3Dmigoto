#include "Hunting.h"

#include <string>
#include <D3Dcompiler.h>

#include "wrl\client.h"
#include "ScreenGrab.h"
#include "wincodec.h"

#include "util.h"
#include "DecompileHLSL.h"
#include "Input.h"
#include "Override.h"
#include "Globals.h"
#include "IniHandler.h"
#include "Assembler.h"


static int StrRenderTarget2D(char *buf, size_t size, D3D11_TEXTURE2D_DESC *desc)
{
	return _snprintf_s(buf, size, size, "type=Texture2D Width=%u Height=%u MipLevels=%u "
		"ArraySize=%u RawFormat=%u Format=\"%s\" SampleDesc.Count=%u "
		"SampleDesc.Quality=%u Usage=%u BindFlags=%u "
		"CPUAccessFlags=%u MiscFlags=%u",
		desc->Width, desc->Height, desc->MipLevels,
		desc->ArraySize, desc->Format,
		TexFormatStr(desc->Format), desc->SampleDesc.Count,
		desc->SampleDesc.Quality, desc->Usage, desc->BindFlags,
		desc->CPUAccessFlags, desc->MiscFlags);
}

static int StrRenderTarget3D(char *buf, size_t size, D3D11_TEXTURE3D_DESC *desc)
{

	return _snprintf_s(buf, size, size, "type=Texture3D Width=%u Height=%u Depth=%u "
		"MipLevels=%u RawFormat=%u Format=\"%s\" Usage=%u BindFlags=%u "
		"CPUAccessFlags=%u MiscFlags=%u",
		desc->Width, desc->Height, desc->Depth,
		desc->MipLevels, desc->Format,
		TexFormatStr(desc->Format), desc->Usage, desc->BindFlags,
		desc->CPUAccessFlags, desc->MiscFlags);
}

static int StrRenderTarget(char *buf, size_t size, struct ResourceInfo &info)
{
	switch (info.type) {
		case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
			return StrRenderTarget2D(buf, size, &info.tex2d_desc);
		case D3D11_RESOURCE_DIMENSION_TEXTURE3D:
			return StrRenderTarget3D(buf, size, &info.tex3d_desc);
		default:
			return _snprintf_s(buf, size, size, "type=%i\n", info.type);
	}
}


// bo3b: For this routine, we have a lot of warnings in x64, from converting a size_t result into the needed
//  DWORD type for the Write calls.  These are writing 256 byte strings, so there is never a chance that it 
//  will lose data, so rather than do anything heroic here, I'm just doing type casts on the strlen function.

DWORD castStrLen(const char* string)
{
	return (DWORD)strlen(string);
}


static void DumpUsage()
{
	wchar_t dir[MAX_PATH];
	GetModuleFileName(0, dir, MAX_PATH);
	wcsrchr(dir, L'\\')[1] = 0;
	wcscat(dir, L"ShaderUsage.txt");
	HANDLE f = CreateFile(dir, GENERIC_WRITE, FILE_SHARE_READ, 0, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (f != INVALID_HANDLE_VALUE)
	{
		if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
		char buf[256];
		DWORD written;
		std::map<UINT64, ShaderInfoData>::iterator i;
		for (i = G->mVertexShaderInfo.begin(); i != G->mVertexShaderInfo.end(); ++i)
		{
			sprintf(buf, "<VertexShader hash=\"%016llx\">\n  <CalledPixelShaders>", i->first);
			WriteFile(f, buf, castStrLen(buf), &written, 0);
			std::set<UINT64>::iterator j;
			for (j = i->second.PartnerShader.begin(); j != i->second.PartnerShader.end(); ++j)
			{
				sprintf(buf, "%016llx ", *j);
				WriteFile(f, buf, castStrLen(buf), &written, 0);
			}
			const char *REG_HEADER = "</CalledPixelShaders>\n";
			WriteFile(f, REG_HEADER, castStrLen(REG_HEADER), &written, 0);
			std::map<int, void *>::iterator k;
			for (k = i->second.ResourceRegisters.begin(); k != i->second.ResourceRegisters.end(); ++k)
				if (k->second)
				{
					UINT64 id = G->mRenderTargets[k->second];
					sprintf(buf, "  <Register id=%d handle=%p>%016llx</Register>\n", k->first, k->second, id);
					WriteFile(f, buf, castStrLen(buf), &written, 0);
				}
			const char *FOOTER = "</VertexShader>\n";
			WriteFile(f, FOOTER, castStrLen(FOOTER), &written, 0);
		}
		for (i = G->mPixelShaderInfo.begin(); i != G->mPixelShaderInfo.end(); ++i)
		{
			sprintf(buf, "<PixelShader hash=\"%016llx\">\n"
				"  <ParentVertexShaders>", i->first);
			WriteFile(f, buf, castStrLen(buf), &written, 0);
			std::set<UINT64>::iterator j;
			for (j = i->second.PartnerShader.begin(); j != i->second.PartnerShader.end(); ++j)
			{
				sprintf(buf, "%016llx ", *j);
				WriteFile(f, buf, castStrLen(buf), &written, 0);
			}
			const char *REG_HEADER = "</ParentVertexShaders>\n";
			WriteFile(f, REG_HEADER, castStrLen(REG_HEADER), &written, 0);
			std::map<int, void *>::iterator k;
			for (k = i->second.ResourceRegisters.begin(); k != i->second.ResourceRegisters.end(); ++k)
				if (k->second)
				{
					UINT64 id = G->mRenderTargets[k->second];
					sprintf(buf, "  <Register id=%d handle=%p>%016llx</Register>\n", k->first, k->second, id);
					WriteFile(f, buf, castStrLen(buf), &written, 0);
				}
			std::vector<std::set<void *>>::iterator m;
			int pos = 0;
			for (m = i->second.RenderTargets.begin(); m != i->second.RenderTargets.end(); m++, pos++) {
				std::set<void *>::const_iterator o;
				for (o = (*m).begin(); o != (*m).end(); o++) {
					UINT64 id = G->mRenderTargets[*o];
					sprintf(buf, "  <RenderTarget id=%d handle=%p>%016llx</RenderTarget>\n", pos, *o, id);
					WriteFile(f, buf, castStrLen(buf), &written, 0);
				}
			}
			std::set<void *>::iterator n;
			for (n = i->second.DepthTargets.begin(); n != i->second.DepthTargets.end(); n++) {
				UINT64 id = G->mRenderTargets[*n];
				sprintf(buf, "  <DepthTarget handle=%p>%016llx</DepthTarget>\n", *n, id);
				WriteFile(f, buf, castStrLen(buf), &written, 0);
			}
			const char *FOOTER = "</PixelShader>\n";
			WriteFile(f, FOOTER, castStrLen(FOOTER), &written, 0);
		}
		std::map<UINT64, struct ResourceInfo>::iterator j;
		for (j = G->mRenderTargetInfo.begin(); j != G->mRenderTargetInfo.end(); j++) {
			_snprintf_s(buf, 256, 256, "<RenderTarget hash=%016llx ", j->first);
			WriteFile(f, buf, castStrLen(buf), &written, 0);
			StrRenderTarget(buf, 256, j->second);
			WriteFile(f, buf, castStrLen(buf), &written, 0);
			const char *FOOTER = "></RenderTarget>\n";
			WriteFile(f, FOOTER, castStrLen(FOOTER), &written, 0);
		}
		for (j = G->mDepthTargetInfo.begin(); j != G->mDepthTargetInfo.end(); j++) {
			_snprintf_s(buf, 256, 256, "<DepthTarget hash=%016llx ", j->first);
			WriteFile(f, buf, castStrLen(buf), &written, 0);
			StrRenderTarget(buf, 256, j->second);
			WriteFile(f, buf, castStrLen(buf), &written, 0);
			const char *FOOTER = "></DepthTarget>\n";
			WriteFile(f, FOOTER, castStrLen(FOOTER), &written, 0);
		}
		if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
		CloseHandle(f);
	}
	else
	{
		LogInfo("Error dumping ShaderUsage.txt\n");

	}
}


// Make a snapshot of the backbuffer, with the current shader disabled, as a good piece
// of documentation.  The name will include the hash code, making a direct shader reference.
//
// CoInitialize must be called for WIC to work.  We can call it multiple times, it will
// return the S_FALSE if it's already inited.

// Only makes a black screen dump in Mordor.  In Alien only does half screen.
// Can't easily use the nvapi version, because it dumps to hardcoded path.

static void SimpleScreenShot(HackerDevice *pDevice, UINT64 hash, wstring shaderType)
{
	wchar_t fullName[MAX_PATH];
	Microsoft::WRL::ComPtr<ID3D11Texture2D> backBuffer;

	HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
	if (FAILED(hr))
		LogInfo("*** Overlay call CoInitializeEx failed: %d \n", hr);

	hr = pDevice->GetOrigSwapChain()->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&backBuffer);
	if (SUCCEEDED(hr))
	{
		wsprintf(fullName, L"%ls\\%I64x-%ls.jps", G->SHADER_PATH, hash, shaderType.c_str());
		hr = DirectX::SaveWICTextureToFile(pDevice->GetOrigContext(), backBuffer.Get(), GUID_ContainerFormatJpeg, fullName);
	}

	LogInfoW(L"  SimpleScreenShot on Mark: %s, result: %d \n", fullName, hr);
}



//--------------------------------------------------------------------------------------------------

// Write the decompiled text as HLSL source code to the txt file.
// Now also writing the ASM text to the bottom of the file, commented out.
// This keeps the ASM with the HLSL for reference and should be more convenient.
//
// This will not overwrite any file that is already there. 
// The assumption is that the shaderByteCode that we have here is always the most up to date,
// and thus is not different than the file on disk.
// If a file was already extant in the ShaderFixes, it will be picked up at game launch as the master shaderByteCode.

static bool WriteHLSL(string hlslText, string asmText, UINT64 hash, wstring shaderType)
{
	wchar_t fullName[MAX_PATH];
	FILE *fw;

	wsprintf(fullName, L"%ls\\%08lx%08lx-%ls_replace.txt", G->SHADER_PATH, (UINT32)(hash >> 32), (UINT32)hash, shaderType.c_str());
	_wfopen_s(&fw, fullName, L"rb");
	if (fw)
	{
		LogInfoW(L"    marked shader file already exists: %s\n", fullName);
		fclose(fw);
		_wfopen_s(&fw, fullName, L"ab");
		if (fw) {
			fprintf_s(fw, " ");					// Touch file to update mod date as a convenience.
			fclose(fw);
		}
		BeepShort();						// Short High beep for for double beep that it's already there.
		return true;
	}

	_wfopen_s(&fw, fullName, L"wb");
	if (!fw)
	{
		LogInfoW(L"    error storing marked shader to %s\n", fullName);
		return false;
	}

	LogInfoW(L"    storing patched shader to %s\n", fullName);

	fwrite(hlslText.c_str(), 1, hlslText.size(), fw);

	fprintf_s(fw, "\n\n/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
	fwrite(asmText.data(), 1, asmText.size(), fw);
	fprintf_s(fw, "\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/\n");

	fclose(fw);
	return true;
}

// This is pretty heavyweight obviously, so it is only being done during Mark operations.
// Todo: another copy/paste job, we really need some subroutines, utility library.

static string Decompile(ID3DBlob* pShaderByteCode, string asmText)
{
	LogInfo("    creating HLSL representation.\n");

	bool patched = false;
	string shaderModel;
	bool errorOccurred = false;
	ParseParameters p;
	p.bytecode = pShaderByteCode->GetBufferPointer();
	p.decompiled = asmText.c_str();
	p.decompiledSize = asmText.size();
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
	}

	return decompiledCode;
}



// Compile a new shader from  HLSL text input, and report on errors if any.
// Return the binary blob of pCode to be activated with CreateVertexShader or CreatePixelShader.
// If the timeStamp has not changed from when it was loaded, skip the recompile, and return false as not an 
// error, but skipped.  On actual errors, return true so that we bail out.

// Compile example taken from: http://msdn.microsoft.com/en-us/library/windows/desktop/hh968107(v=vs.85).aspx

static bool RegenerateShader(wchar_t *shaderFixPath, wchar_t *fileName, const char *shaderModel, 
	UINT64 hash, wstring shaderType, ID3DBlob *origByteCode,
	__out FILETIME* timeStamp, _Outptr_ ID3DBlob** pCode)
{
	*pCode = nullptr;
	wchar_t fullName[MAX_PATH];
	wsprintf(fullName, L"%s\\%s", shaderFixPath, fileName);

	HANDLE f = CreateFile(fullName, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (f == INVALID_HANDLE_VALUE)
	{
		LogInfo("    ReloadShader shader not found: %ls\n", fullName);

		return true;
	}


	DWORD srcDataSize = GetFileSize(f, 0);
	vector<char> srcData(srcDataSize);
	DWORD readSize;
	FILETIME curFileTime;

	if (!ReadFile(f, srcData.data(), srcDataSize, &readSize, 0)
		|| !GetFileTime(f, NULL, NULL, &curFileTime)
		|| srcDataSize != readSize)
	{
		LogInfo("    Error reading txt file.\n");

		return true;
	}
	CloseHandle(f);

	// Check file time stamp, and only recompile shaders that have been edited since they were loaded.
	// This dramatically improves the F10 reload speed.
	if (!CompareFileTime(timeStamp, &curFileTime))
	{
		return false;
	}
	*timeStamp = curFileTime;

	// Now that we are sure to be reloading, let's see if it's an ASM file and assemble instead.
	ID3DBlob* pByteCode = nullptr;
	
	if (wcsstr(fileName, L"_replace"))
	{
		LogInfo("   >Replacement shader found. Re-Loading replacement HLSL code from %ls \n", fileName);
		LogInfo("    Reload source code loaded. Size = %d \n", srcDataSize);
		LogInfo("    compiling replacement HLSL code with shader model %s \n", shaderModel);


		ID3DBlob* pErrorMsgs = nullptr;
		HRESULT ret = D3DCompile(srcData.data(), srcDataSize, "wrapper1349", 0, ((ID3DInclude*)(UINT_PTR)1),
			"main", shaderModel, D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &pByteCode, &pErrorMsgs);

		LogInfo("    compile result for replacement HLSL shader: %x\n", ret);

		if (LogFile && pErrorMsgs)
		{
			LPVOID errMsg = pErrorMsgs->GetBufferPointer();
			SIZE_T errSize = pErrorMsgs->GetBufferSize();
			LogInfo("--------------------------------------------- BEGIN ---------------------------------------------\n");
			fwrite(errMsg, 1, errSize - 1, LogFile);
			LogInfo("---------------------------------------------- END ----------------------------------------------\n");
			pErrorMsgs->Release();
		}

		if (FAILED(ret))
		{
			if (pByteCode)
			{
				pByteCode->Release();
				pByteCode = 0;
			}
			return true;
		}
	}
	else if (wcsstr(fileName, L"_reasm"))
	{
		return false;	// Don't reassemble these, and don't error out.
	} 
	else
	{
		LogInfo("   >Replacement shader found. Re-Loading replacement ASM code from %ls \n", fileName);
		LogInfo("    Reload source code loaded. Size = %d \n", srcDataSize);
		LogInfo("    assembling replacement ASM code with shader model %s \n", shaderModel);

		// We need original byte code unchanged, so make a copy.
		vector<byte> byteCode(origByteCode->GetBufferSize());
		memcpy(byteCode.data(), origByteCode->GetBufferPointer(), origByteCode->GetBufferSize());
		byteCode = assembler(srcData, byteCode);

		// ToDo: How we do know when it fails? Error handling. Do we really have to re-disassemble?
		string asmText = BinaryToAsmText(byteCode.data(), byteCode.size());
		if (asmText.empty())
		{
			LogInfo("  *** assembler failed. \n");
			return true;
		}
		else
		{
			// Write reassembly output for comparison. ToDo: how long do we need this?
			swprintf_s(fullName, MAX_PATH, L"%ls\\%016llx-%ls_reasm.txt", G->SHADER_PATH, hash, shaderType.c_str());
			HRESULT hr = CreateTextFile(fullName, asmText, true);
			if (FAILED(hr)) {
				LogInfoW(L"    *** error storing reassembly to %s \n", fullName);
			}
			else {
				LogInfoW(L"    storing reassembly to %s \n", fullName);
			}

			// Since the re-assembly worked, let's make it the active shader code.
			HRESULT ret = D3DCreateBlob(byteCode.size(), &pByteCode);
			if (SUCCEEDED(ret)) {
				memcpy(pByteCode->GetBufferPointer(), byteCode.data(), byteCode.size());
			} else {
				LogInfo("    *** failed to allocate new Blob for assemble. \n");
				return true;
			}
		}
	}


	// Write replacement .bin if necessary
	if (G->CACHE_SHADERS && pByteCode)
	{
		wchar_t val[MAX_PATH];
		wsprintf(val, L"%ls\\%08lx%08lx-%ls_replace.bin", shaderFixPath, (UINT32)(hash >> 32), (UINT32)(hash), shaderType.c_str());
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
			fwrite(pByteCode->GetBufferPointer(), 1, pByteCode->GetBufferSize(), fw);
			fclose(fw);
		}
	}

	// pCode on return == NULL for error cases, valid if made it this far.
	*pCode = pByteCode;

	return true;
}


// Strategy: When the user hits F10 as the reload key, we want to reload all of the hand-patched shaders in
//	the ShaderFixes folder, and make them live in game.  That will allow the user to test out fixes on the 
//	HLSL .txt files and do live experiments with fixes.  This makes it easier to figure things out.
//	To do that, we need to patch what is being sent to VSSetShader and PSSetShader, as they get activated
//	in game.  Since the original shader is already on the GPU from CreateVertexShader and CreatePixelShader,
//	we need to override it on the fly. We cannot change what the game originally sent as the shader request,
//	nor their storage location, because we cannot guarantee that they didn't make a copy and use it. So, the
//	item to go on is the ID3D11VertexShader* that the game received back from CreateVertexShader and will thus
//	use to activate it with VSSetShader.
//	To do the override, we've made a new Map that maps the original ID3D11VertexShader* to the new one, and
//	in VSSetShader, we will look for a map match, and if we find it, we'll apply the override of the newly
//	loaded shader.
//	Here in ReloadShader, we need to set up to make that possible, by filling in the mReloadedVertexShaders
//	map with <old,new>. In this spot, we have been notified by the user via F10 or whatever input that they
//	want the shaders reloaded. We need to take each shader hlsl.txt file, recompile it, call CreateVertexShader
//	on it to make it available in the GPU, and save the new ID3D11VertexShader* in our <old,new> map. We get the
//	old ID3D11VertexShader* reference by looking that up in the complete mVertexShaders map, by using the hash
//	number we can get from the file name itself.
//	Notably, if the user does multiple iterations, we'll still only use the same number of overrides, because
//	the map will replace the last one. This probably does leak vertex shaders on the GPU though.

// Todo: this is not a particularly good spot for the routine.  Need to move these compile/dissassemble routines
//	including those in Direct3D11Device.h into a separate file and include a .h file.
//	This routine plagarized from ReplaceShaders.

// Reload all of the patched shaders from the ShaderFixes folder, and add them to the override map, so that the
// new version will be used at VSSetShader and PSSetShader.
// File names are uniform in the form: 3c69e169edc8cd5f-ps_replace.txt

static bool ReloadShader(wchar_t *shaderPath, wchar_t *fileName, HackerDevice *device)
{
	UINT64 hash;
	ID3D11DeviceChild* oldShader = NULL;
	ID3D11DeviceChild* replacement = NULL;
	ID3D11ClassLinkage* classLinkage;
	ID3DBlob* shaderCode;
	string shaderModel;
	wstring shaderType;		// "vs" or "ps" maybe "gs"
	FILETIME timeStamp;
	HRESULT hr = E_FAIL;

	// Extract hash from first 16 characters of file name so we can look up details by hash
	wstring ws = fileName;
	hash = stoull(ws.substr(0, 16), NULL, 16);

	// This is probably unnecessary, because we modify already existing map entries, but
	// for consistency, we'll wrap this.
	if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);

	// Find the original shader bytecode in the mReloadedShaders Map. This map contains entries for all
	// shaders from the ShaderFixes and ShaderCache folder, and can also include .bin files that were loaded directly.
	// We include ShaderCache because that allows moving files into ShaderFixes as they are identified.
	// This needs to use the value to find the key, so a linear search.
	// It's notable that the map can contain multiple copies of the same hash, used for different visual
	// items, but with same original code.  We need to update all copies.
	for each (pair<ID3D11DeviceChild *, OriginalShaderInfo> iter in G->mReloadedShaders)
	{
		if (iter.second.hash == hash)
		{
			oldShader = iter.first;
			classLinkage = iter.second.linkage;
			shaderModel = iter.second.shaderModel;
			shaderType = iter.second.shaderType;
			timeStamp = iter.second.timeStamp;
			shaderCode = iter.second.byteCode;

			// If we didn't find an original shader, that is OK, because it might not have been loaded yet.
			// Just skip it in that case, because the new version will be loaded when it is used.
			if (oldShader == NULL)
			{
				LogInfo("> failed to find original shader in mReloadedShaders: %ls\n", fileName);
				continue;
			}

			// Is there a good reason we are operating on a copy of the map and not the original?
			// Took me a while to work out why this wasn't working: iter.second.found = true;
			//   -DarkStarSword
			// Not a _good_ reason, but I was worried about breaking something I didn't understand, 
			// if I were to modify the original. Too many moving parts for me, no good way to test regressions.
			//   -bo3b
			G->mReloadedShaders[oldShader].found = true;

			// If shaderModel is "bin", that means the original was loaded as a binary object, and thus shaderModel is unknown.
			// Disassemble the binary to get that string.
			if (shaderModel.compare("bin") == 0)
			{
				shaderModel = GetShaderModel(shaderCode->GetBufferPointer(), shaderCode->GetBufferSize());
				if (shaderModel.empty())
					return false;
				G->mReloadedShaders[oldShader].shaderModel = shaderModel;
			}

			// Compile anew. If timestamp is unchanged, the code is unchanged, continue to next shader.
			ID3DBlob *pShaderBytecode = NULL;
			if (!RegenerateShader(shaderPath, fileName, shaderModel.c_str(), hash, shaderType, shaderCode, &timeStamp, &pShaderBytecode))
				continue;

			// If we compiled but got nothing, that's a fatal error we need to report.
			if (pShaderBytecode == NULL)
				return false;

			// Update timestamp, since we have an edited file.
			G->mReloadedShaders[oldShader].timeStamp = timeStamp;

			// This needs to call the real CreateVertexShader, not our wrapped version
			if (shaderType.compare(L"vs") == 0)
			{
				hr = device->GetOrigDevice()->CreateVertexShader(pShaderBytecode->GetBufferPointer(), pShaderBytecode->GetBufferSize(), classLinkage,
					(ID3D11VertexShader**)&replacement);
			}
			else if (shaderType.compare(L"ps") == 0)
			{
				hr = device->GetOrigDevice()->CreatePixelShader(pShaderBytecode->GetBufferPointer(), pShaderBytecode->GetBufferSize(), classLinkage,
					(ID3D11PixelShader**)&replacement);
			}
			if (FAILED(hr))
				return false;


			// If we have an older reloaded shader, let's release it to avoid a memory leak.  This only happens after 1st reload.
			// New shader is loaded on GPU and ready to be used as override in VSSetShader or PSSetShader
			if (G->mReloadedShaders[oldShader].replacement != NULL)
				G->mReloadedShaders[oldShader].replacement->Release();
			G->mReloadedShaders[oldShader].replacement = replacement;

			// New binary shader code, to replace the prior loaded shader byte code. 
			shaderCode->Release();
			G->mReloadedShaders[oldShader].byteCode = pShaderBytecode;

			LogInfo("> successfully reloaded shader: %ls\n", fileName);
		}
	}	// for every registered shader in mReloadedShaders 

	if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);

	return true;
}



// When a shader is marked by the user, we want to automatically move it to the ShaderFixes folder
// The universal way to do this is to keep the shaderByteCode around, and when mark happens, use that as
// the replacement and build code to match.  This handles all the variants of preload, cache, hlsl 
// or not, and allows creating new files on a first run.  Should be handy.

static void CopyToFixes(UINT64 hash, HackerDevice *device)
{
	bool success = false;
	string asmText;
	string decompiled;

	// The key of the map is the actual shader, we thus need to do a linear search to find our marked hash.
	for each (pair<ID3D11DeviceChild *, OriginalShaderInfo> iter in G->mReloadedShaders)
	{
		if (iter.second.hash == hash)
		{
			// Whether we succeed or fail on decompile, let's now make a screen shot of the backbuffer
			// as a good way to remember what the HLSL affects. This will be with it disabled in the picture.
			// ToDo, broken at moment, doesn't make 3D shots.
			//SimpleScreenShot(device, hash, iter.second.shaderType);

			asmText = BinaryToAsmText(iter.second.byteCode->GetBufferPointer(), iter.second.byteCode->GetBufferSize());
			if (asmText.empty())
				break;

			// Disassembly file is written, now decompile the current byte code into HLSL.
			decompiled = Decompile(iter.second.byteCode, asmText);
			if (decompiled.empty())
				break;

			// Save the decompiled text, and ASM text into the .txt source file.
			if (!WriteHLSL(decompiled, asmText, hash, iter.second.shaderType))
				break;


			// Lastly, reload the shader generated, to check for decompile errors, set it as the active 
			// shader code, in case there are visual errors, and make it the match the code in the file.
			wchar_t fileName[MAX_PATH];
			wsprintf(fileName, L"%08lx%08lx-%ls_replace.txt", (UINT32)(hash >> 32), (UINT32)(hash), iter.second.shaderType.c_str());
			if (!ReloadShader(G->SHADER_PATH, fileName, device))
				break;

			// There can be more than one in the map with the same hash, but we only need a single copy to
			// make the hlsl file output, so exit with success.
			success = true;
			break;
		}
	}

	if (success)
	{
		BeepSuccess();			// High beep for success, to notify it's running fresh fixes.
		LogInfo("> successfully copied Marked shader to ShaderFixes\n");
	}
	else
	{
		BeepFailure();			// Bonk sound for failure.
		LogInfo("> FAILED to copy Marked shader to ShaderFixes\n");
	}
}

static void TakeScreenShot(HackerDevice *wrapped, void *private_data)
{
	LogInfo("> capturing screenshot\n");

	if (wrapped->mStereoHandle)
	{
		NvAPI_Status err;
		err = NvAPI_Stereo_CapturePngImage(wrapped->mStereoHandle);
		if (err != NVAPI_OK)
		{
			LogInfo("> screenshot failed, error:%d\n", err);
			BeepFailure2();		// Brnk, dunk sound for failure.
		}
	}
}

// If a shader no longer exists in ShaderFixes, point it back to the original
// shader so that the replaced shaders are consistent with those in
// ShaderFixes. Especially useful if the decompiler creates a rendering issue
// in a shader we actually don't need so we don't need to restart the game.
static void RevertMissingShaders()
{
	ID3D11DeviceChild* replacement = NULL;
	ShaderReloadMap::iterator i;

	for (i = G->mReloadedShaders.begin(); i != G->mReloadedShaders.end(); i++) {
		if (i->second.found)
			continue;

		if (i->second.shaderType.compare(L"vs") == 0) {
			VertexShaderReplacementMap::iterator j = G->mOriginalVertexShaders.find((ID3D11VertexShader*)i->first);
			if (j == G->mOriginalVertexShaders.end())
				continue;
			replacement = j->second;
		}
		else if (i->second.shaderType.compare(L"ps") == 0) {
			PixelShaderReplacementMap::iterator j = G->mOriginalPixelShaders.find((ID3D11PixelShader*)i->first);
			if (j == G->mOriginalPixelShaders.end())
				continue;
			replacement = j->second;
		}
		else {
			continue;
		}

		if ((i->second.replacement == NULL && i->first == replacement)
			|| replacement == i->second.replacement) {
			continue;
		}

		LogInfo("Reverting %016llx not found in ShaderFixes\n", i->second.hash);

		if (i->second.replacement)
			i->second.replacement->Release();

		replacement->AddRef();
		i->second.replacement = replacement;
		i->second.timeStamp = { 0 };
	}
}

// Now that we are adding ASM files to the mix, we need to decide who gets precedence.
// Given that an ASM file is possibly being used as the more sure approach for a given
// file, it makes sense to give ASM files precedence, if both files are there.
// Using the '*' for file match will allow anything with _replace or with no tail to
// be found.  Since these will use name order, the no tail asm file will come after
// the hlsl file and replace it.  This dual file scenario is expected to be rare, so
// not doing anything heroic here to avoid that double load.

static void ReloadFixes(HackerDevice *device, void *private_data)
{
	LogInfo("> reloading *_replace.txt fixes from ShaderFixes\n");

	if (G->SHADER_PATH[0])
	{
		bool success = true;
		WIN32_FIND_DATA findFileData;
		wchar_t fileName[MAX_PATH];

		for (ShaderReloadMap::iterator iter = G->mReloadedShaders.begin(); iter != G->mReloadedShaders.end(); iter++)
			iter->second.found = false;

		// Strict file name format, to allow renaming out of the way. 
		// "00aa7fa12bbf66b3-ps_replace.txt" or "00aa7fa12bbf66b3-vs.txt"
		// Will still blow up if the first characters are not hex.
		wsprintf(fileName, L"%ls\\????????????????-??*.txt", G->SHADER_PATH);
		HANDLE hFind = FindFirstFile(fileName, &findFileData);
		if (hFind != INVALID_HANDLE_VALUE)
		{
			do
			{
				success = ReloadShader(G->SHADER_PATH, findFileData.cFileName, device);
			} while (FindNextFile(hFind, &findFileData) && success);
			FindClose(hFind);
		}

		if (success)
		{
			// Any shaders in the map not visited, we want to revert back to original.
			RevertMissingShaders();

			BeepSuccess();		// High beep for success, to notify it's running fresh fixes.
			LogInfo("> successfully reloaded shaders from ShaderFixes\n");
		}
		else
		{
			BeepFailure();			// Bonk sound for failure.
			LogInfo("> FAILED to reload shaders from ShaderFixes\n");
		}
	}
}

static void DisableFix(HackerDevice *device, void *private_data)
{
	LogInfo("show_original pressed - switching to original shaders\n");
	G->fix_enabled = false;
}

static void EnableFix(HackerDevice *device, void *private_data)
{
	LogInfo("show_original released - switching to replaced shaders\n");
	G->fix_enabled = true;
}




static void NextIndexBuffer(HackerDevice *device, void *private_data)
{
	if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
	std::set<UINT64>::iterator i = G->mVisitedIndexBuffers.find(G->mSelectedIndexBuffer);
	if (i != G->mVisitedIndexBuffers.end() && ++i != G->mVisitedIndexBuffers.end())
	{
		G->mSelectedIndexBuffer = *i;
		G->mSelectedIndexBufferPos++;
		LogInfo("> traversing to next index buffer #%d. Number of index buffers in frame: %Iu\n", G->mSelectedIndexBufferPos, G->mVisitedIndexBuffers.size());
	}
	if (i == G->mVisitedIndexBuffers.end() && ++G->mSelectedIndexBufferPos < G->mVisitedIndexBuffers.size() && G->mSelectedIndexBufferPos >= 0)
	{
		i = G->mVisitedIndexBuffers.begin();
		std::advance(i, G->mSelectedIndexBufferPos);
		G->mSelectedIndexBuffer = *i;
		LogInfo("> last index buffer lost. traversing to next index buffer #%d. Number of index buffers in frame: %Iu\n", G->mSelectedIndexBufferPos, G->mVisitedIndexBuffers.size());
	}
	if (i == G->mVisitedIndexBuffers.end() && G->mVisitedIndexBuffers.size() != 0)
	{
		G->mSelectedIndexBufferPos = 0;
		LogInfo("> traversing to index buffer #0. Number of index buffers in frame: %Iu\n", G->mVisitedIndexBuffers.size());

		G->mSelectedIndexBuffer = *G->mVisitedIndexBuffers.begin();
	}
	if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
}

static void PrevIndexBuffer(HackerDevice *device, void *private_data)
{
	if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
	std::set<UINT64>::iterator i = G->mVisitedIndexBuffers.find(G->mSelectedIndexBuffer);
	if ((i == G->mVisitedIndexBuffers.begin() || G->mSelectedIndexBuffer == 1) && G->mVisitedIndexBuffers.size() != 0) {
		i = std::prev(G->mVisitedIndexBuffers.end());
		G->mSelectedIndexBuffer = *i;
		G->mSelectedIndexBufferPos = (unsigned)G->mVisitedIndexBuffers.size() - 1;
		LogInfo("> traversing to index buffer #%d. Number of index buffers in frame: %Iu\n", G->mSelectedIndexBufferPos, G->mVisitedIndexBuffers.size());
	}
	else if (i != G->mVisitedIndexBuffers.end() && i != G->mVisitedIndexBuffers.begin())
	{
		--i;
		G->mSelectedIndexBuffer = *i;
		G->mSelectedIndexBufferPos--;
		LogInfo("> traversing to previous index buffer #%d. Number of index buffers in frame: %Iu\n", G->mSelectedIndexBufferPos, G->mVisitedIndexBuffers.size());
	}
	if (i == G->mVisitedIndexBuffers.end() && --G->mSelectedIndexBufferPos < G->mVisitedIndexBuffers.size() && G->mSelectedIndexBufferPos >= 0)
	{
		i = G->mVisitedIndexBuffers.begin();
		std::advance(i, G->mSelectedIndexBufferPos);
		G->mSelectedIndexBuffer = *i;
		LogInfo("> last index buffer lost. traversing to previous index buffer #%d. Number of index buffers in frame: %Iu\n", G->mSelectedIndexBufferPos, G->mVisitedIndexBuffers.size());
	}
	if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
}

static void MarkIndexBuffer(HackerDevice *device, void *private_data)
{
	if (LogFile)
	{
		LogInfo(">>>> Index buffer marked: index buffer hash = %08lx%08lx\n", (UINT32)(G->mSelectedIndexBuffer >> 32), (UINT32)G->mSelectedIndexBuffer);
		for (std::set<UINT64>::iterator i = G->mSelectedIndexBuffer_PixelShader.begin(); i != G->mSelectedIndexBuffer_PixelShader.end(); ++i)
			LogInfo("     visited pixel shader hash = %08lx%08lx\n", (UINT32)(*i >> 32), (UINT32)*i);
		for (std::set<UINT64>::iterator i = G->mSelectedIndexBuffer_VertexShader.begin(); i != G->mSelectedIndexBuffer_VertexShader.end(); ++i)
			LogInfo("     visited vertex shader hash = %08lx%08lx\n", (UINT32)(*i >> 32), (UINT32)*i);
	}
	if (G->DumpUsage) DumpUsage();
}

static void NextPixelShader(HackerDevice *device, void *private_data)
{
	if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
	{
		std::set<UINT64>::iterator loc = G->mVisitedPixelShaders.find(G->mSelectedPixelShader);
		std::set<UINT64>::iterator end = G->mVisitedPixelShaders.end();
		bool found = (loc != end);
		int size = (int) G->mVisitedPixelShaders.size();

		if (!found && size > 0)
		{
			G->mSelectedPixelShaderPos = 0;
			G->mSelectedPixelShader = *G->mVisitedPixelShaders.begin();
			LogInfo("> starting at pixel shader #%d. Number of pixel shaders in frame: %Iu \n", G->mSelectedPixelShaderPos, size);
		}
		else {
			loc++;
			if (loc != end)
			{
				G->mSelectedPixelShaderPos++;
				G->mSelectedPixelShader = *loc;
			}
			else {
				G->mSelectedPixelShaderPos = 0;
				G->mSelectedPixelShader = *G->mVisitedPixelShaders.begin();
			}
			LogInfo("> traversing to next pixel shader #%d. Number of pixel shaders in frame: %Iu \n", G->mSelectedPixelShaderPos, size);
		}
	}
	if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
}

static void PrevPixelShader(HackerDevice *device, void *private_data)
{
	if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
	{
		std::set<UINT64>::iterator loc = G->mVisitedPixelShaders.find(G->mSelectedPixelShader);
		std::set<UINT64>::iterator end = G->mVisitedPixelShaders.end();
		std::set<UINT64>::iterator front = G->mVisitedPixelShaders.begin();
		bool found = (loc != end);
		int size = (int) G->mVisitedPixelShaders.size();

		if (!found && size > 0)
		{
			G->mSelectedPixelShaderPos = size - 1;
			G->mSelectedPixelShader = *std::prev(end);
			LogInfo("> starting at pixel shader #%d. Number of pixel shaders in frame: %Iu \n", G->mSelectedPixelShaderPos, size);
		}
		else {
			if (loc != front)
			{
				G->mSelectedPixelShaderPos--;
				loc--;
				G->mSelectedPixelShader = *loc;
			}
			else {
				G->mSelectedPixelShaderPos = size - 1;
				G->mSelectedPixelShader = *std::prev(end);
			}
			LogInfo("> traversing to previous pixel shader #%d. Number of pixel shaders in frame: %Iu \n", G->mSelectedPixelShaderPos, size);
		}
	}
	if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
}

static void MarkPixelShader(HackerDevice *device, void *private_data)
{
	if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
	if (LogFile)
	{
		LogInfo(">>>> Pixel shader marked: pixel shader hash = %08lx%08lx\n", (UINT32)(G->mSelectedPixelShader >> 32), (UINT32)G->mSelectedPixelShader);
		for (std::set<UINT64>::iterator i = G->mSelectedPixelShader_IndexBuffer.begin(); i != G->mSelectedPixelShader_IndexBuffer.end(); ++i)
			LogInfo("     visited index buffer hash = %08lx%08lx\n", (UINT32)(*i >> 32), (UINT32)*i);
		for (std::set<UINT64>::iterator i = G->mPixelShaderInfo[G->mSelectedPixelShader].PartnerShader.begin(); i != G->mPixelShaderInfo[G->mSelectedPixelShader].PartnerShader.end(); ++i)
			LogInfo("     visited vertex shader hash = %08lx%08lx\n", (UINT32)(*i >> 32), (UINT32)*i);
	}
	CompiledShaderMap::iterator i = G->mCompiledShaderMap.find(G->mSelectedPixelShader);
	if (i != G->mCompiledShaderMap.end())
	{
		LogInfo("       pixel shader was compiled from source code %s\n", i->second.c_str());
	}
	i = G->mCompiledShaderMap.find(G->mSelectedVertexShader);
	if (i != G->mCompiledShaderMap.end())
	{
		LogInfo("       vertex shader was compiled from source code %s\n", i->second.c_str());
	}
	// Copy marked shader to ShaderFixes
	CopyToFixes(G->mSelectedPixelShader, device);
	if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
	if (G->DumpUsage) DumpUsage();
}

static void NextVertexShader(HackerDevice *device, void *private_data)
{
	if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
	{
		std::set<UINT64>::iterator loc = G->mVisitedVertexShaders.find(G->mSelectedVertexShader);
		std::set<UINT64>::iterator end = G->mVisitedVertexShaders.end();
		bool found = (loc != end);
		int size = (int) G->mVisitedVertexShaders.size();

		if (!found && size > 0)
		{
			G->mSelectedVertexShaderPos = 0;
			G->mSelectedVertexShader = *G->mVisitedVertexShaders.begin();
			LogInfo("> starting at vertex shader #%d. Number of vertex shaders in frame: %Iu \n", G->mSelectedVertexShaderPos, size);
		} else {
			loc++;
			if (loc != end)
			{
				G->mSelectedVertexShaderPos++;
				G->mSelectedVertexShader = *loc;
			} else {
				G->mSelectedVertexShaderPos = 0;
				G->mSelectedVertexShader = *G->mVisitedVertexShaders.begin();
			}
			LogInfo("> traversing to next vertex shader #%d. Number of vertex shaders in frame: %Iu \n", G->mSelectedVertexShaderPos, size);
		}
	}
	if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
}

static void PrevVertexShader(HackerDevice *device, void *private_data)
{
	if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
	{
		std::set<UINT64>::iterator loc = G->mVisitedVertexShaders.find(G->mSelectedVertexShader);
		std::set<UINT64>::iterator end = G->mVisitedVertexShaders.end();
		std::set<UINT64>::iterator front = G->mVisitedVertexShaders.begin();
		bool found = (loc != end);
		int size = (int) G->mVisitedVertexShaders.size();

		if (!found && size > 0)
		{
			G->mSelectedVertexShaderPos = size - 1;
			G->mSelectedVertexShader = *std::prev(end);
			LogInfo("> starting at vertex shader #%d. Number of vertex shaders in frame: %Iu \n", G->mSelectedVertexShaderPos, size);
		}
		else {
			if (loc != front)
			{
				G->mSelectedVertexShaderPos--;
				loc--;
				G->mSelectedVertexShader = *loc;
			} else {
				G->mSelectedVertexShaderPos = size - 1;	
				G->mSelectedVertexShader = *std::prev(end);
			}
			LogInfo("> traversing to previous vertex shader #%d. Number of vertex shaders in frame: %Iu \n", G->mSelectedVertexShaderPos, size);
		}
	}
	if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
}

static void MarkVertexShader(HackerDevice *device, void *private_data)
{
	if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
	if (LogFile)
	{
		LogInfo(">>>> Vertex shader marked: vertex shader hash = %08lx%08lx\n", (UINT32)(G->mSelectedVertexShader >> 32), (UINT32)G->mSelectedVertexShader);
		for (std::set<UINT64>::iterator i = G->mVertexShaderInfo[G->mSelectedVertexShader].PartnerShader.begin(); i != G->mVertexShaderInfo[G->mSelectedVertexShader].PartnerShader.end(); ++i)
			LogInfo("     visited pixel shader hash = %08lx%08lx\n", (UINT32)(*i >> 32), (UINT32)*i);
		for (std::set<UINT64>::iterator i = G->mSelectedVertexShader_IndexBuffer.begin(); i != G->mSelectedVertexShader_IndexBuffer.end(); ++i)
			LogInfo("     visited index buffer hash = %08lx%08lx\n", (UINT32)(*i >> 32), (UINT32)*i);
	}

	CompiledShaderMap::iterator i = G->mCompiledShaderMap.find(G->mSelectedVertexShader);
	if (i != G->mCompiledShaderMap.end())
	{
		LogInfo("       shader was compiled from source code %s\n", i->second.c_str());
	}
	// Copy marked shader to ShaderFixes
	CopyToFixes(G->mSelectedVertexShader, device);
	if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
	if (G->DumpUsage) DumpUsage();
}

static void NextRenderTarget(HackerDevice *device, void *private_data)
{
	if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
	std::set<void *>::iterator i = G->mVisitedRenderTargets.find(G->mSelectedRenderTarget);
	if (i != G->mVisitedRenderTargets.end() && ++i != G->mVisitedRenderTargets.end())
	{
		G->mSelectedRenderTarget = *i;
		G->mSelectedRenderTargetPos++;
		LogInfo("> traversing to next render target #%d. Number of render targets in frame: %Iu\n", G->mSelectedRenderTargetPos, G->mVisitedRenderTargets.size());
	}
	if (i == G->mVisitedRenderTargets.end() && ++G->mSelectedRenderTargetPos < G->mVisitedRenderTargets.size() && G->mSelectedRenderTargetPos >= 0)
	{
		i = G->mVisitedRenderTargets.begin();
		std::advance(i, G->mSelectedRenderTargetPos);
		G->mSelectedRenderTarget = *i;
		LogInfo("> last render target lost. traversing to next render target #%d. Number of render targets frame: %Iu\n", G->mSelectedRenderTargetPos, G->mVisitedRenderTargets.size());
	}
	if (i == G->mVisitedRenderTargets.end() && G->mVisitedRenderTargets.size() != 0)
	{
		G->mSelectedRenderTargetPos = 0;
		LogInfo("> traversing to render target #0. Number of render targets in frame: %Iu\n", G->mVisitedRenderTargets.size());

		G->mSelectedRenderTarget = *G->mVisitedRenderTargets.begin();
	}
	if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
}

static void PrevRenderTarget(HackerDevice *device, void *private_data)
{
	if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
	std::set<void *>::iterator i = G->mVisitedRenderTargets.find(G->mSelectedRenderTarget);
	if ((i == G->mVisitedRenderTargets.begin() || G->mSelectedRenderTarget == (void *)1) && G->mVisitedRenderTargets.size() != 0) {
		i = std::prev(G->mVisitedRenderTargets.end());
		G->mSelectedRenderTarget = *i;
		G->mSelectedRenderTargetPos = (unsigned)G->mVisitedRenderTargets.size() - 1;
		LogInfo("> traversing to render targets #%d. Number of render targets in frame: %Iu\n", G->mSelectedRenderTargetPos, G->mVisitedRenderTargets.size());
	}
	else if (i != G->mVisitedRenderTargets.end() && i != G->mVisitedRenderTargets.begin())
	{
		--i;
		G->mSelectedRenderTarget = *i;
		G->mSelectedRenderTargetPos--;
		LogInfo("> traversing to previous render target #%d. Number of render targets in frame: %Iu\n", G->mSelectedRenderTargetPos, G->mVisitedRenderTargets.size());
	}
	if (i == G->mVisitedRenderTargets.end() && --G->mSelectedRenderTargetPos < G->mVisitedRenderTargets.size() && G->mSelectedRenderTargetPos >= 0)
	{
		i = G->mVisitedRenderTargets.begin();
		std::advance(i, G->mSelectedRenderTargetPos);
		G->mSelectedRenderTarget = *i;
		LogInfo("> last render target lost. traversing to previous render target #%d. Number of render targets in frame: %Iu\n", G->mSelectedRenderTargetPos, G->mVisitedRenderTargets.size());
	}
	if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
}

static void LogRenderTarget(void *target, char *log_prefix)
{
	char buf[256];

	if (!target || target == (void *)1) {
		LogInfo("No render target selected for marking\n");
		return;
	}

	UINT64 hash = G->mRenderTargets[target];
	struct ResourceInfo &info = G->mRenderTargetInfo[hash];
	StrRenderTarget(buf, 256, info);
	LogInfo("%srender target handle = %p, hash = %.16llx, %s\n",
		log_prefix, target, hash, buf);
}

static void MarkRenderTarget(HackerDevice *device, void *private_data)
{
	if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
	if (LogFile)
	{
		LogRenderTarget(G->mSelectedRenderTarget, ">>>> Render target marked: ");
		for (std::set<void *>::iterator i = G->mSelectedRenderTargetSnapshotList.begin(); i != G->mSelectedRenderTargetSnapshotList.end(); ++i)
		{
			LogRenderTarget(*i, "       ");
		}
	}
	if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);

	if (G->DumpUsage) DumpUsage();
}


static void TuneUp(HackerDevice *device, void *private_data)
{
	int index = (int)private_data;

	G->gTuneValue[index] += G->gTuneStep;
	LogInfo("> Value %i tuned to %f\n", index + 1, G->gTuneValue[index]);
}

static void TuneDown(HackerDevice *device, void *private_data)
{
	int index = (int)private_data;

	G->gTuneValue[index] -= G->gTuneStep;
	LogInfo("> Value %i tuned to %f\n", index + 1, G->gTuneValue[index]);
}


// Start with a fresh set of shaders in the scene - either called explicitly
// via keypress, or after no hunting for 1 minute (see comment in RunFrameActions)
// Caller must have taken G->mCriticalSection (if enabled)
void TimeoutHuntingBuffers()
{
	G->mVisitedIndexBuffers.clear();
	G->mVisitedVertexShaders.clear();
	G->mVisitedPixelShaders.clear();

	// FIXME: Not sure this is the right place to clear these - I think
	// they should be cleared every frame as they appear to be aimed at
	// providing a single frame usage snapshot on mark:
	G->mSelectedPixelShader_IndexBuffer.clear();
	G->mSelectedVertexShader_IndexBuffer.clear();
	G->mSelectedIndexBuffer_PixelShader.clear();
	G->mSelectedIndexBuffer_VertexShader.clear();

#if 0 /* Iterations are broken since we no longer use present() */
	// This seems totally bogus - shouldn't we be resetting the iteration
	// on each new frame, not after hunting timeout? This probably worked
	// back when RunFrameActions() was called from present(), but I suspect
	// has been broken ever since that was changed to come from draw(), and
	// it's not related to hunting buffers so it doesn't belong here:
	for (ShaderOverrideMap::iterator i = G->mShaderOverrideMap.begin(); i != G->mShaderOverrideMap.end(); ++i)
		i->second.iterations[0] = 0;
#endif
}

// User has requested all shaders be re-enabled
static void DoneHunting(HackerDevice *device, void *private_data)
{
	if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);

	TimeoutHuntingBuffers();

	G->mSelectedPixelShader = -1;
	G->mSelectedPixelShaderPos = -1;
	G->mSelectedVertexShader = -1;
	G->mSelectedVertexShaderPos = -1;

	G->mSelectedRenderTargetPos = 0;
	G->mSelectedRenderTarget = ((void *)1);
	G->mSelectedIndexBuffer = 1;
	G->mSelectedIndexBufferPos = 0;

	if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
}

static void ToggleHunting(HackerDevice *device, void *private_data)
{
	G->hunting = !G->hunting;
	LogInfo("> Hunting toggled to %d \n", G->hunting);
}

void RegisterHuntingKeyBindings(wchar_t *iniFile)
{
	int i;
	wchar_t buf[16];
	int repeat = 8, noRepeat = 0;

	// reload_config is registered even if not hunting - this allows us to
	// turn on hunting in the ini dynamically without having to relaunch
	// the game. This can be useful in games that receive a significant
	// performance hit with hunting on, or where a broken effect is
	// discovered while playing normally where it may not be easy/fast to
	// find the effect again later.
	G->config_reloadable = RegisterIniKeyBinding(L"Hunting", L"reload_config", iniFile, FlagConfigReload, NULL, noRepeat, NULL);

	// Let's also allow an easy toggle of hunting itself, for speed and playability.
	RegisterIniKeyBinding(L"Hunting", L"toggle_hunting", iniFile, ToggleHunting, NULL, noRepeat, NULL);

	//if (!G->hunting)
	//	return;

	if (GetPrivateProfileString(L"Hunting", L"repeat_rate", 0, buf, 16, iniFile))
		repeat = _wtoi(buf);

	RegisterIniKeyBinding(L"Hunting", L"next_pixelshader", iniFile, NextPixelShader, NULL, repeat, NULL);
	RegisterIniKeyBinding(L"Hunting", L"previous_pixelshader", iniFile, PrevPixelShader, NULL, repeat, NULL);
	RegisterIniKeyBinding(L"Hunting", L"mark_pixelshader", iniFile, MarkPixelShader, NULL, noRepeat, NULL);

	RegisterIniKeyBinding(L"Hunting", L"take_screenshot", iniFile, TakeScreenShot, NULL, noRepeat, NULL);

	RegisterIniKeyBinding(L"Hunting", L"next_indexbuffer", iniFile, NextIndexBuffer, NULL, repeat, NULL);
	RegisterIniKeyBinding(L"Hunting", L"previous_indexbuffer", iniFile, PrevIndexBuffer, NULL, repeat, NULL);
	RegisterIniKeyBinding(L"Hunting", L"mark_indexbuffer", iniFile, MarkIndexBuffer, NULL, noRepeat, NULL);

	RegisterIniKeyBinding(L"Hunting", L"next_vertexshader", iniFile, NextVertexShader, NULL, repeat, NULL);
	RegisterIniKeyBinding(L"Hunting", L"previous_vertexshader", iniFile, PrevVertexShader, NULL, repeat, NULL);
	RegisterIniKeyBinding(L"Hunting", L"mark_vertexshader", iniFile, MarkVertexShader, NULL, noRepeat, NULL);

	RegisterIniKeyBinding(L"Hunting", L"next_rendertarget", iniFile, NextRenderTarget, NULL, repeat, NULL);
	RegisterIniKeyBinding(L"Hunting", L"previous_rendertarget", iniFile, PrevRenderTarget, NULL, repeat, NULL);
	RegisterIniKeyBinding(L"Hunting", L"mark_rendertarget", iniFile, MarkRenderTarget, NULL, noRepeat, NULL);

	RegisterIniKeyBinding(L"Hunting", L"done_hunting", iniFile, DoneHunting, NULL, noRepeat, NULL);

	RegisterIniKeyBinding(L"Hunting", L"reload_fixes", iniFile, ReloadFixes, NULL, noRepeat, NULL);

	RegisterIniKeyBinding(L"Hunting", L"show_original", iniFile, DisableFix, EnableFix, noRepeat, NULL);

	for (i = 0; i < 4; i++) {
		_snwprintf(buf, 16, L"tune%i_up", i + 1);
		RegisterIniKeyBinding(L"Hunting", buf, iniFile, TuneUp, NULL, repeat, (void*)i);

		_snwprintf(buf, 16, L"tune%i_down", i + 1);
		RegisterIniKeyBinding(L"Hunting", buf, iniFile, TuneDown, NULL, repeat, (void*)i);
	}

	LogInfoW(L"  repeat_rate=%d\n", repeat);
}



