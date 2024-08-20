#include <d3d11_1.h>
#include <D3D11ShaderTracing.h>
#include <Shlobj.h>
#include <algorithm>
#include <ctime>

#include "util.h"

#define COMPILER_DLL_VERSION "46"
#define COMPILER_DLL_VERSIONL L"46"

// Standalone DLL needs its own log file reference, as log.h just exports them.
bool gLogDebug = false;
FILE *LogFile = 0;

static bool gInitialized = false;

static bool EXPORT_SHADERS = false;
static wchar_t SHADER_PATH[MAX_PATH] = { 0 };

using namespace std;



void InitializeDLL()
{
	if (!gInitialized)
	{
		gInitialized = true;
		wchar_t dir[MAX_PATH];
		GetModuleFileName(0, dir, MAX_PATH);
		wcsrchr(dir, L'\\')[1] = 0;
		wcscat(dir, L"d3dx.ini");
		LogFile = GetPrivateProfileInt(L"Logging", L"calls", 0, dir) ? (FILE *)-1 : 0;
		if (LogFile) fopen_s(&LogFile, "D3DCompiler_" COMPILER_DLL_VERSION "_log.txt", "w");

		LogInfo("\nD3DCompiler_" COMPILER_DLL_VERSION " starting init  -  %s\n\n", LogTime().c_str());

		// Unbuffered logging to remove need for fflush calls, and r/w access to make it easy
		// to open active files.
		int unbuffered = -1;
		if (GetPrivateProfileInt(L"Logging", L"unbuffered", 0, dir))
		{
			unbuffered = setvbuf(LogFile, NULL, _IONBF, 0);
			LogInfo("  unbuffered=1  return: %d\n", unbuffered);
		}

		// Set the CPU affinity based upon d3dx.ini setting.  Useful for debugging and shader hunting in AC3.
		if (GetPrivateProfileInt(L"Logging", L"force_cpu_affinity", 0, dir))
		{
			DWORD one = 0x01;
			BOOL result = SetProcessAffinityMask(GetCurrentProcess(), one);
			LogInfo("CPU Affinity forced to 1- no multithreading: %s\n", result ? "true" : "false");
		}
		
		wchar_t val[MAX_PATH];
		GetPrivateProfileString(L"Rendering", L"storage_directory", 0, SHADER_PATH, MAX_PATH, dir);
		if (SHADER_PATH[0])
		{
			GetModuleFileName(0, val, MAX_PATH);
			wcsrchr(val, L'\\')[1] = 0;
			wcscat(val, SHADER_PATH);
			wcscpy(SHADER_PATH, val);
			// Create directory?
			CreateDirectory(SHADER_PATH, 0);
		}
		EXPORT_SHADERS = GetPrivateProfileInt(L"Rendering", L"export_shaders", 0, dir) == 1;
	}

	LogInfo("  DLL initialized.\n");
}

void DestroyDLL()
{
	if (LogFile)
	{
		LogInfo("Destroying DLL...\n");
		fclose(LogFile);
	}
}


static HMODULE hC46 = 0;
static HMODULE hD3D11 = 0;

typedef HRESULT (WINAPI *tD3DCompileFromMemory)(_In_reads_bytes_(SrcDataSize) LPCVOID pSrcData,
           _In_ SIZE_T SrcDataSize,
           _In_opt_ LPCSTR pSourceName,
           _In_reads_opt_(_Inexpressible_(pDefines->Name != NULL)) CONST D3D10_SHADER_MACRO* pDefines,
           _In_opt_ ID3D10Include* pInclude,
           _In_ LPCSTR pEntrypoint,
           _In_ LPCSTR pTarget,
           _In_ UINT Flags1,
           _In_ UINT Flags2,
           _Out_ ID3D10Blob** ppCode,
           _Out_opt_ ID3D10Blob** ppErrorMsgs);
static tD3DCompileFromMemory _D3DCompileFromMemory;
struct SOutputContext
{
	// Unknown.
};
typedef HRESULT (WINAPI *tD3DDisassembleCode)(_In_reads_bytes_(SrcDataSize) LPCVOID pSrcData,
               _In_ SIZE_T SrcDataSize,
               _In_ UINT Flags,
               _Out_ SOutputContext *context,
               _Out_ ID3D10Blob** ppDisassembly);
static tD3DDisassembleCode _D3DDisassembleCode;
typedef HRESULT (WINAPI *tD3DDisassembleEffect)(_In_ interface ID3D10Effect *pEffect, 
                       _In_ UINT Flags,
                       _Out_ ID3D10Blob** ppDisassembly);
static tD3DDisassembleEffect _D3DDisassembleEffect;
typedef HRESULT (WINAPI *tD3DGetCodeDebugInfo)(_In_reads_bytes_(SrcDataSize) LPCVOID pSrcData,
                _In_ SIZE_T SrcDataSize,
                _Out_ ID3D10Blob** ppDebugInfo);
static tD3DGetCodeDebugInfo _D3DGetCodeDebugInfo;
typedef HRESULT (WINAPI *tD3DPreprocessFromMemory)(_In_reads_bytes_(SrcDataSize) LPCVOID pSrcData,
              _In_ SIZE_T SrcDataSize,
              _In_opt_ LPCSTR pSourceName,
              _In_opt_ CONST D3D10_SHADER_MACRO* pDefines,
              _In_opt_ ID3D10Include* pInclude,
              _Out_ ID3D10Blob** ppCodeText,
              _Out_opt_ ID3D10Blob** ppErrorMsgs);
static tD3DPreprocessFromMemory _D3DPreprocessFromMemory;
typedef HRESULT (WINAPI *tD3DReflectCode)(GUID *interfaceId,
           _In_ INT unknown,
		   _In_reads_bytes_(SrcDataSize) LPCVOID pSrcData,
           _In_ SIZE_T SrcDataSize);
static tD3DReflectCode _D3DReflectCode;
typedef HRESULT (WINAPI *tD3DReturnFailure1)(int a, int b, int c);
static tD3DReturnFailure1 _D3DReturnFailure1;
typedef void (WINAPI *tDebugSetMute)();
static tDebugSetMute _DebugSetMute;
typedef HRESULT (WINAPI *tD3DAssemble)(LPCVOID data, SIZE_T datasize, LPCSTR filename,
                           const D3D_SHADER_MACRO *defines, ID3DInclude *include,
                           UINT flags,
                           ID3DBlob **shader, ID3DBlob **error_messages);
static tD3DAssemble _D3DAssemble;
typedef HRESULT (WINAPI *tD3DDisassemble11Trace)(_In_reads_bytes_(SrcDataSize) LPCVOID pSrcData,
                      _In_ SIZE_T SrcDataSize,
                      _In_ ID3D11ShaderTrace* pTrace,
                      _In_ UINT StartStep,
                      _In_ UINT NumSteps,
                      _In_ UINT Flags,
                      _Out_ interface ID3D10Blob** ppDisassembly);
static tD3DDisassemble11Trace _D3DDisassemble11Trace;
typedef HRESULT (WINAPI *tD3DReadFileToBlob)(_In_ LPCWSTR pFileName,
                  _Out_ ID3DBlob** ppContents);
static tD3DReadFileToBlob _D3DReadFileToBlob;
typedef HRESULT (WINAPI *tD3DWriteBlobToFile)(_In_ ID3DBlob* pBlob,
                   _In_ LPCWSTR pFileName,
                   _In_ BOOL bOverwrite);
static tD3DWriteBlobToFile _D3DWriteBlobToFile;
typedef HRESULT (WINAPI *tD3DCompile)(_In_reads_bytes_(SrcDataSize) LPCVOID pSrcData,
           _In_ SIZE_T SrcDataSize,
           _In_opt_ LPCSTR pSourceName,
           _In_reads_opt_(_Inexpressible_(pDefines->Name != NULL)) CONST D3D_SHADER_MACRO* pDefines,
           _In_opt_ ID3DInclude* pInclude,
           _In_ LPCSTR pEntrypoint,
           _In_ LPCSTR pTarget,
           _In_ UINT Flags1,
           _In_ UINT Flags2,
           _Out_ ID3DBlob** ppCode,
           _Out_opt_ ID3DBlob** ppErrorMsgs);
static tD3DCompile _D3DCompile;
typedef HRESULT (WINAPI *tD3DCompile2)(_In_reads_bytes_(SrcDataSize) LPCVOID pSrcData,
            _In_ SIZE_T SrcDataSize,
            _In_opt_ LPCSTR pSourceName,
            _In_reads_opt_(_Inexpressible_(pDefines->Name != NULL)) CONST D3D_SHADER_MACRO* pDefines,
            _In_opt_ ID3DInclude* pInclude,
            _In_ LPCSTR pEntrypoint,
            _In_ LPCSTR pTarget,
            _In_ UINT Flags1,
            _In_ UINT Flags2,
            _In_ UINT SecondaryDataFlags,
            _In_reads_bytes_opt_(SecondaryDataSize) LPCVOID pSecondaryData,
            _In_ SIZE_T SecondaryDataSize,
            _Out_ ID3DBlob** ppCode,
            _Out_opt_ ID3DBlob** ppErrorMsgs);
static tD3DCompile2 _D3DCompile2;
typedef HRESULT (WINAPI *tD3DCompileFromFile)(_In_ LPCWSTR pFileName,
                   _In_reads_opt_(_Inexpressible_(pDefines->Name != NULL)) CONST D3D_SHADER_MACRO* pDefines,
                   _In_opt_ ID3DInclude* pInclude,
                   _In_ LPCSTR pEntrypoint,
                   _In_ LPCSTR pTarget,
                   _In_ UINT Flags1,
                   _In_ UINT Flags2,
                   _Out_ ID3DBlob** ppCode,
                   _Out_opt_ ID3DBlob** ppErrorMsgs);
static tD3DCompileFromFile _D3DCompileFromFile;
typedef HRESULT (WINAPI *tD3DPreprocess)(_In_reads_bytes_(SrcDataSize) LPCVOID pSrcData,
              _In_ SIZE_T SrcDataSize,
              _In_opt_ LPCSTR pSourceName,
              _In_opt_ CONST D3D_SHADER_MACRO* pDefines,
              _In_opt_ ID3DInclude* pInclude,
              _Out_ ID3DBlob** ppCodeText,
              _Out_opt_ ID3DBlob** ppErrorMsgs);
static tD3DPreprocess _D3DPreprocess;
typedef HRESULT (WINAPI *tD3DGetDebugInfo)(_In_reads_bytes_(SrcDataSize) LPCVOID pSrcData,
                _In_ SIZE_T SrcDataSize,
                _Out_ ID3DBlob** ppDebugInfo);
static tD3DGetDebugInfo _D3DGetDebugInfo;
typedef HRESULT (WINAPI *tD3DReflect)(_In_reads_bytes_(SrcDataSize) LPCVOID pSrcData,
           _In_ SIZE_T SrcDataSize,
	   _In_ REFIID pInterface,
           _Out_ void** ppReflector);
static tD3DReflect _D3DReflect;
typedef HRESULT (WINAPI *tD3DDisassemble)(_In_reads_bytes_(SrcDataSize) LPCVOID pSrcData,
               _In_ SIZE_T SrcDataSize,
               _In_ UINT Flags,
               _In_opt_ LPCSTR szComments,
               _Out_ ID3DBlob** ppDisassembly);
static tD3DDisassemble _D3DDisassemble;
typedef HRESULT (WINAPI *tD3DDisassembleRegion)(_In_reads_bytes_(SrcDataSize) LPCVOID pSrcData,
                     _In_ SIZE_T SrcDataSize,
                     _In_ UINT Flags,
                     _In_opt_ LPCSTR szComments,
                     _In_ SIZE_T StartByteOffset,
                     _In_ SIZE_T NumInsts,
                     _Out_opt_ SIZE_T* pFinishByteOffset,
                     _Out_ ID3DBlob** ppDisassembly);
static tD3DDisassembleRegion _D3DDisassembleRegion;
typedef HRESULT (WINAPI *tD3DDisassemble10Effect)(_In_ interface ID3D10Effect *pEffect, 
                       _In_ UINT Flags,
                       _Out_ ID3DBlob** ppDisassembly);
static tD3DDisassemble10Effect _D3DDisassemble10Effect;
typedef HRESULT (WINAPI *tD3DGetTraceInstructionOffsets)(_In_reads_bytes_(SrcDataSize) LPCVOID pSrcData,
                              _In_ SIZE_T SrcDataSize,
                              _In_ UINT Flags,
                              _In_ SIZE_T StartInstIndex,
                              _In_ SIZE_T NumInsts,
                              _Out_writes_to_opt_(NumInsts, min(NumInsts, *pTotalInsts)) SIZE_T* pOffsets,
                              _Out_opt_ SIZE_T* pTotalInsts);
static tD3DGetTraceInstructionOffsets _D3DGetTraceInstructionOffsets;
typedef HRESULT (WINAPI *tD3DGetInputSignatureBlob)(_In_reads_bytes_(SrcDataSize) LPCVOID pSrcData,
                         _In_ SIZE_T SrcDataSize,
                         _Out_ ID3DBlob** ppSignatureBlob);
static tD3DGetInputSignatureBlob _D3DGetInputSignatureBlob;
typedef HRESULT (WINAPI *tD3DGetOutputSignatureBlob)(_In_reads_bytes_(SrcDataSize) LPCVOID pSrcData,
                          _In_ SIZE_T SrcDataSize,
                          _Out_ ID3DBlob** ppSignatureBlob);
static tD3DGetOutputSignatureBlob _D3DGetOutputSignatureBlob;
typedef HRESULT (WINAPI *tD3DGetInputAndOutputSignatureBlob)(_In_reads_bytes_(SrcDataSize) LPCVOID pSrcData,
                                  _In_ SIZE_T SrcDataSize,
                                  _Out_ ID3DBlob** ppSignatureBlob);
static tD3DGetInputAndOutputSignatureBlob _D3DGetInputAndOutputSignatureBlob;
typedef HRESULT (WINAPI *tD3DStripShader)(_In_reads_bytes_(BytecodeLength) LPCVOID pShaderBytecode,
               _In_ SIZE_T BytecodeLength,
               _In_ UINT uStripFlags,
               _Out_ ID3DBlob** ppStrippedBlob);
static tD3DStripShader _D3DStripShader;
typedef HRESULT (WINAPI *tD3DGetBlobPart)(_In_reads_bytes_(SrcDataSize) LPCVOID pSrcData,
               _In_ SIZE_T SrcDataSize,
               _In_ D3D_BLOB_PART Part,
               _In_ UINT Flags,
               _Out_ ID3DBlob** ppPart);
static tD3DGetBlobPart _D3DGetBlobPart;
typedef HRESULT (WINAPI *tD3DSetBlobPart)(_In_reads_bytes_(SrcDataSize) LPCVOID pSrcData,
               _In_ SIZE_T SrcDataSize,
               _In_ D3D_BLOB_PART Part,
               _In_ UINT Flags,
	       _In_reads_bytes_(PartSize) LPCVOID pPart,
               _In_ SIZE_T PartSize,
               _Out_ ID3DBlob** ppNewShader);
static tD3DSetBlobPart _D3DSetBlobPart;
typedef HRESULT (WINAPI *tD3DCompressShaders)(_In_ UINT uNumShaders,
                   _In_reads_(uNumShaders) D3D_SHADER_DATA* pShaderData,
                   _In_ UINT uFlags,
                   _Out_ ID3DBlob** ppCompressedData);
static tD3DCompressShaders _D3DCompressShaders;
typedef HRESULT (WINAPI *tD3DDecompressShaders)(_In_reads_bytes_(SrcDataSize) LPCVOID pSrcData,
                     _In_ SIZE_T SrcDataSize,
                     _In_ UINT uNumShaders,	      
                     _In_ UINT uStartIndex,
                     _In_reads_opt_(uNumShaders) UINT* pIndices,
                     _In_ UINT uFlags,
                     _Out_writes_(uNumShaders) ID3DBlob** ppShaders,
		     _Out_opt_ UINT* pTotalShaders);
static tD3DDecompressShaders _D3DDecompressShaders;
typedef HRESULT (WINAPI *tD3DCreateBlob)(_In_ SIZE_T Size,
              _Out_ ID3DBlob** ppBlob);
static tD3DCreateBlob _D3DCreateBlob;

// DirectX 11 bridge.
typedef int (WINAPI *tD3D11CoreGetLayeredDeviceSize)(int a, int b);
static tD3D11CoreGetLayeredDeviceSize _D3D11CoreGetLayeredDeviceSize;
struct D3D11BridgeData
{
	UINT64 BinaryHash;
	char *HLSLFileName;
};

static void InitC46()
{
	if (hC46) return;
	InitializeDLL();

	wchar_t workingDir[MAX_PATH];
	GetModuleFileName(0, workingDir, MAX_PATH);
	wcsrchr(workingDir, L'\\')[1] = 0;
	wcscat(workingDir, L"D3DCompiler_" COMPILER_DLL_VERSIONL L"_org.dll");
	hC46 = LoadLibrary(workingDir);	
    if (!hC46)
    {
        LogInfo("LoadLibrary on D3DCompiler_" COMPILER_DLL_VERSION "_org.dll failed\n");
        
        return;
    }
	GetModuleFileName(0, workingDir, MAX_PATH);
	wcsrchr(workingDir, L'\\')[1] = 0;
	wcscat(workingDir, L"d3d11.dll");
	hD3D11 = LoadLibrary(workingDir);	
    if (!hD3D11)
    {
        LogInfo("LoadLibrary on d3d11.dll wrapper failed\n");
    }

	// d3d11 bridge
	_D3D11CoreGetLayeredDeviceSize = (tD3D11CoreGetLayeredDeviceSize) GetProcAddress(hD3D11, "D3D11CoreGetLayeredDeviceSize");
	// Version 39
	_D3DCompileFromMemory = (tD3DCompileFromMemory) GetProcAddress(hC46, "D3DCompileFromMemory");
	_D3DDisassembleCode = (tD3DDisassembleCode) GetProcAddress(hC46, "D3DDisassembleCode");
	_D3DDisassembleEffect = (tD3DDisassembleEffect) GetProcAddress(hC46, "D3DDisassembleEffect");
	_D3DGetCodeDebugInfo = (tD3DGetCodeDebugInfo) GetProcAddress(hC46, "D3DGetCodeDebugInfo");
	_D3DPreprocessFromMemory = (tD3DPreprocessFromMemory) GetProcAddress(hC46, "D3DPreprocessFromMemory");
	_D3DReflectCode = (tD3DReflectCode) GetProcAddress(hC46, "D3DReflectCode");
	// Version 46
	_D3DAssemble = (tD3DAssemble) GetProcAddress(hC46, "D3DAssemble");
	_D3DCompile = (tD3DCompile) GetProcAddress(hC46, "D3DCompile");
	_D3DCompile2 = (tD3DCompile2) GetProcAddress(hC46, "D3DCompile2");
	_D3DCompileFromFile = (tD3DCompileFromFile) GetProcAddress(hC46, "D3DCompileFromFile");
	_D3DCompressShaders = (tD3DCompressShaders) GetProcAddress(hC46, "D3DCompressShaders");
	_D3DCreateBlob = (tD3DCreateBlob) GetProcAddress(hC46, "D3DCreateBlob");
	_D3DDecompressShaders = (tD3DDecompressShaders) GetProcAddress(hC46, "D3DDecompressShaders");
	_D3DDisassemble = (tD3DDisassemble) GetProcAddress(hC46, "D3DDisassemble");
	_D3DDisassemble10Effect = (tD3DDisassemble10Effect) GetProcAddress(hC46, "D3DDisassemble10Effect");
	_D3DDisassemble11Trace = (tD3DDisassemble11Trace) GetProcAddress(hC46, "D3DDisassemble11Trace");
	_D3DDisassembleRegion = (tD3DDisassembleRegion) GetProcAddress(hC46, "D3DDisassembleRegion");
	_D3DGetBlobPart = (tD3DGetBlobPart) GetProcAddress(hC46, "D3DGetBlobPart");
	_D3DGetDebugInfo = (tD3DGetDebugInfo) GetProcAddress(hC46, "D3DGetDebugInfo");
	_D3DGetInputAndOutputSignatureBlob = (tD3DGetInputAndOutputSignatureBlob) GetProcAddress(hC46, "D3DGetInputAndOutputSignatureBlob");
	_D3DGetInputSignatureBlob = (tD3DGetInputSignatureBlob) GetProcAddress(hC46, "D3DGetInputSignatureBlob");
	_D3DGetOutputSignatureBlob = (tD3DGetOutputSignatureBlob) GetProcAddress(hC46, "D3DGetOutputSignatureBlob");
	_D3DGetTraceInstructionOffsets = (tD3DGetTraceInstructionOffsets) GetProcAddress(hC46, "D3DGetTraceInstructionOffsets");
	_D3DPreprocess = (tD3DPreprocess) GetProcAddress(hC46, "D3DPreprocess");
	_D3DReadFileToBlob = (tD3DReadFileToBlob) GetProcAddress(hC46, "D3DReadFileToBlob");
	_D3DReflect = (tD3DReflect) GetProcAddress(hC46, "D3DReflect");
	_D3DReturnFailure1 = (tD3DReturnFailure1) GetProcAddress(hC46, "D3DReturnFailure1");
	_D3DSetBlobPart = (tD3DSetBlobPart) GetProcAddress(hC46, "D3DSetBlobPart");
	_D3DStripShader = (tD3DStripShader) GetProcAddress(hC46, "D3DStripShader");
	_D3DWriteBlobToFile = (tD3DWriteBlobToFile) GetProcAddress(hC46, "D3DWriteBlobToFile");
	_DebugSetMute = (tDebugSetMute) GetProcAddress(hC46, "DebugSetMute");
}

HRESULT WINAPI D3DReturnFailure1(int a, int b, int c)
{
	InitC46();
	return (*_D3DReturnFailure1)(a, b, c);
}
void WINAPI DebugSetMute()
{
	InitC46();
	return (*_DebugSetMute)();
}
HRESULT WINAPI D3DAssemble(LPCVOID data, SIZE_T datasize, LPCSTR filename,
                           const D3D_SHADER_MACRO *defines, ID3DInclude *include,
                           UINT flags,
                           ID3DBlob **shader, ID3DBlob **error_messages)
{
	InitC46();
	LogInfo("D3DAssemble called\n"); 
	
	return (*_D3DAssemble)(data, datasize, filename, defines, include, flags, shader, error_messages);
}
HRESULT WINAPI D3DDisassemble11Trace(_In_reads_bytes_(SrcDataSize) LPCVOID pSrcData,
                      _In_ SIZE_T SrcDataSize,
                      _In_ ID3D11ShaderTrace* pTrace,
                      _In_ UINT StartStep,
                      _In_ UINT NumSteps,
                      _In_ UINT Flags,
                      _Out_ interface ID3D10Blob** ppDisassembly)
{
	InitC46();
	LogInfo("D3DDisassemble11Trace called\n"); 
	
	return (*_D3DDisassemble11Trace)(pSrcData, SrcDataSize, pTrace, StartStep, NumSteps, Flags, ppDisassembly);
}
HRESULT WINAPI D3DReadFileToBlob(_In_ LPCWSTR pFileName,
                  _Out_ ID3DBlob** ppContents)
{
	InitC46();
	LogInfo("D3DReadFileToBlob called\n"); 
	
	return (*_D3DReadFileToBlob)(pFileName, ppContents);
}
HRESULT WINAPI D3DWriteBlobToFile(_In_ ID3DBlob* pBlob,
                   _In_ LPCWSTR pFileName,
                   _In_ BOOL bOverwrite)
{
	InitC46();
	LogInfo("D3DWriteBlobToFile called\n"); 
	
	return (*_D3DWriteBlobToFile)(pBlob, pFileName, bOverwrite);
}
HRESULT WINAPI D3DCompile(_In_reads_bytes_(SrcDataSize) LPCVOID pSrcData,
           _In_ SIZE_T SrcDataSize,
           _In_opt_ LPCSTR pSourceName,
           _In_reads_opt_(_Inexpressible_(pDefines->Name != NULL)) CONST D3D_SHADER_MACRO* pDefines,
           _In_opt_ ID3DInclude* pInclude,
           _In_ LPCSTR pEntrypoint,
           _In_ LPCSTR pTarget,
           _In_ UINT Flags1,
           _In_ UINT Flags2,
           _Out_ ID3DBlob** ppCode,
           _Out_opt_ ID3DBlob** ppErrorMsgs)
{
	InitC46();
	bool wrapperCall = (pSourceName && !strcmp(pSourceName, "wrapper1349"));
	if (LogFile && !wrapperCall) 
	{
		LogInfo("D3DCompile called with\n"); 
		LogInfo("  SourceName = %s\n", pSourceName);
		LogInfo("  Entrypoint = %s\n", pEntrypoint);
		LogInfo("  Target = %s\n", pTarget);
		LogInfo("  ppErrorMsgs = %p\n", ppErrorMsgs);
		
	}

	// Compile original shader.
	ID3DBlob *errorBlob = 0;
	if (!ppErrorMsgs)
	{
		ppErrorMsgs = &errorBlob;
	}
	HRESULT ret = (*_D3DCompile)(pSrcData, SrcDataSize, pSourceName, pDefines, pInclude, pEntrypoint,
		pTarget, Flags1, Flags2, ppCode, ppErrorMsgs);
	UINT64 binaryHash;
	char shaderName[MAX_PATH];
	if (ret == S_OK && ppCode && *ppCode && !wrapperCall)
	{
		// Calculate CRC and name.
		UINT64 sourceHash = fnv_64_buf(pSrcData, SrcDataSize);
		binaryHash = fnv_64_buf((*ppCode)->GetBufferPointer(), (*ppCode)->GetBufferSize());
		sprintf(shaderName, "%08lx%08lx-%s_%08lx%08lx.txt", (UINT32)(binaryHash >> 32), (UINT32)binaryHash, pTarget, (UINT32)(sourceHash >> 32), (UINT32)sourceHash);
		LogInfo("    Filename = %s\n", shaderName);
		LogInfo("    Compiled bytecode size = %llu, bytecode handle = %p\n", (*ppCode)->GetBufferSize(), (*ppCode)->GetBufferPointer());
		
		if (SHADER_PATH[0])
		{
			wchar_t val[MAX_PATH], entrypoint[MAX_PATH], target[MAX_PATH];
			mbstowcs(entrypoint, pEntrypoint, MAX_PATH);
			mbstowcs(target, pTarget, MAX_PATH);
			wsprintf(val, L"%ls\\%08lx%08lx-%ls_%08lx%08lx.txt", SHADER_PATH, 
				(UINT32)(binaryHash >> 32), (UINT32)binaryHash, target, (UINT32)(sourceHash >> 32), (UINT32)sourceHash);
			if (EXPORT_SHADERS)
			{
				FILE *f;
				_wfopen_s(&f, val, L"wb");
				if (f)
				{
					fwrite(pSrcData, 1, SrcDataSize, f);
					fclose(f);
				}
			}
			else
			{
				wsprintf(val, L"%ls\\%08lx%08lx-%ls_%08lx%08lx*replace.txt", SHADER_PATH, (UINT32)(binaryHash >> 32), (UINT32)binaryHash, target, (UINT32)(sourceHash >> 32), (UINT32)sourceHash);
				WIN32_FIND_DATA findFileData;
				HANDLE hFind = FindFirstFile(val, &findFileData);
				if (hFind != INVALID_HANDLE_VALUE)
				{
					wsprintf(val, L"%ls\\%ls", SHADER_PATH, findFileData.cFileName);
					FindClose(hFind);
					FILE *f;
					_wfopen_s(&f, val, L"rb");
					if (LogFile)
					{
						char path[MAX_PATH];
						wcstombs(path, val, MAX_PATH);
						LogInfo("    Replacement shader found. Loading replacement HLSL code from file \"%s\".\n", path);
						
					}
					if (f)
					{
						fseek(f, 0, SEEK_END);
						SrcDataSize = ftell(f);
						fseek(f, 0, SEEK_SET);
						pSrcData = new char[SrcDataSize];
						fread((void *) pSrcData, 1, SrcDataSize, f);
						fclose(f);
						LogInfo("    Source code loaded. Size = %llu\n", SrcDataSize);
						

						// Compile replacement.
						if (*ppErrorMsgs) (*ppErrorMsgs)->Release();
						if (*ppCode) (*ppCode)->Release();
						ret = (*_D3DCompile)(pSrcData, SrcDataSize, pSourceName, pDefines, pInclude, pEntrypoint,
							pTarget, Flags1, Flags2, ppCode, ppErrorMsgs);
						delete pSrcData;
					}
				}
			}
		}
	}
	if (LogFile && !wrapperCall)
	{
		LogInfo("  Result = %x\n", ret);
		if (*ppErrorMsgs)
		{
			LPVOID errMsg = (*ppErrorMsgs)->GetBufferPointer();
			SIZE_T errSize = (*ppErrorMsgs)->GetBufferSize();
			LogInfo("  Compile errors:\n");
			LogInfo("--------------------------------------------- BEGIN ---------------------------------------------\n");
			fwrite(errMsg, 1, errSize, LogFile);
			LogInfo("\n---------------------------------------------- END ----------------------------------------------\n");
		}
	}
	if (errorBlob) errorBlob->Release();

	// ToDo: No longer connected to d3d11 here
	// Send to DirectX 
	//if (ret == S_OK && _D3D11CoreGetLayeredDeviceSize && !wrapperCall)
	//{
	//	D3D11BridgeData data;
	//	data.BinaryHash = binaryHash;
	//	data.HLSLFileName = shaderName;
	//	int success = (*_D3D11CoreGetLayeredDeviceSize)(0x77aa128b, (int) &data);
	//	if (LogFile && success != 0xaa77125b)
	//	{
	//		LogInfo("    sending code hash to d3d11.dll wrapper failed\n");
	//	}
	//}
	return ret;
}
HRESULT WINAPI D3DCompile2(_In_reads_bytes_(SrcDataSize) LPCVOID pSrcData,
            _In_ SIZE_T SrcDataSize,
            _In_opt_ LPCSTR pSourceName,
            _In_reads_opt_(_Inexpressible_(pDefines->Name != NULL)) CONST D3D_SHADER_MACRO* pDefines,
            _In_opt_ ID3DInclude* pInclude,
            _In_ LPCSTR pEntrypoint,
            _In_ LPCSTR pTarget,
            _In_ UINT Flags1,
            _In_ UINT Flags2,
            _In_ UINT SecondaryDataFlags,
            _In_reads_bytes_opt_(SecondaryDataSize) LPCVOID pSecondaryData,
            _In_ SIZE_T SecondaryDataSize,
            _Out_ ID3DBlob** ppCode,
            _Out_opt_ ID3DBlob** ppErrorMsgs)
{
	InitC46();
	LogInfo("D3DCompile2 called\n"); 
	
	return (*_D3DCompile2)(pSrcData, SrcDataSize, pSourceName, pDefines, pInclude, pEntrypoint,
		pTarget, Flags1, Flags2, SecondaryDataFlags, pSecondaryData, SecondaryDataSize, ppCode, ppErrorMsgs);
}
HRESULT WINAPI D3DCompileFromFile(_In_ LPCWSTR pFileName,
                   _In_reads_opt_(_Inexpressible_(pDefines->Name != NULL)) CONST D3D_SHADER_MACRO* pDefines,
                   _In_opt_ ID3DInclude* pInclude,
                   _In_ LPCSTR pEntrypoint,
                   _In_ LPCSTR pTarget,
                   _In_ UINT Flags1,
                   _In_ UINT Flags2,
                   _Out_ ID3DBlob** ppCode,
                   _Out_opt_ ID3DBlob** ppErrorMsgs)
{
	InitC46();
	LogInfo("D3DCompileFromFile called\n"); 
	
	return (*_D3DCompileFromFile)(pFileName, pDefines, pInclude, pEntrypoint, pTarget, Flags1, Flags2,
		ppCode, ppErrorMsgs);
}
HRESULT WINAPI D3DPreprocess(_In_reads_bytes_(SrcDataSize) LPCVOID pSrcData,
              _In_ SIZE_T SrcDataSize,
              _In_opt_ LPCSTR pSourceName,
              _In_opt_ CONST D3D_SHADER_MACRO* pDefines,
              _In_opt_ ID3DInclude* pInclude,
              _Out_ ID3DBlob** ppCodeText,
              _Out_opt_ ID3DBlob** ppErrorMsgs)
{
	InitC46();
	LogInfo("D3DPreprocess called\n"); 
	
	return (*_D3DPreprocess)(pSrcData, SrcDataSize, pSourceName, pDefines, pInclude, ppCodeText, ppErrorMsgs);
}
HRESULT WINAPI D3DGetDebugInfo(_In_reads_bytes_(SrcDataSize) LPCVOID pSrcData,
                _In_ SIZE_T SrcDataSize,
                _Out_ ID3DBlob** ppDebugInfo)
{
	InitC46();
	return (*_D3DGetDebugInfo)(pSrcData, SrcDataSize, ppDebugInfo);
}
HRESULT WINAPI D3DReflect(_In_reads_bytes_(SrcDataSize) LPCVOID pSrcData,
           _In_ SIZE_T SrcDataSize,
	   _In_ REFIID pInterface,
           _Out_ void** ppReflector)
{
	InitC46();
	//LogInfo("D3DReflect called\n"); 
	//
	return (*_D3DReflect)(pSrcData, SrcDataSize, pInterface, ppReflector);
}
HRESULT WINAPI D3DDisassemble(_In_reads_bytes_(SrcDataSize) LPCVOID pSrcData,
               _In_ SIZE_T SrcDataSize,
               _In_ UINT Flags,
               _In_opt_ LPCSTR szComments,
               _Out_ ID3DBlob** ppDisassembly)
{
	InitC46();
	//LogInfo("D3DDisassemble called\n"); 
	//
	return (*_D3DDisassemble)(pSrcData, SrcDataSize, Flags, szComments, ppDisassembly);
}
HRESULT WINAPI D3DDisassembleRegion(_In_reads_bytes_(SrcDataSize) LPCVOID pSrcData,
                     _In_ SIZE_T SrcDataSize,
                     _In_ UINT Flags,
                     _In_opt_ LPCSTR szComments,
                     _In_ SIZE_T StartByteOffset,
                     _In_ SIZE_T NumInsts,
                     _Out_opt_ SIZE_T* pFinishByteOffset,
                     _Out_ ID3DBlob** ppDisassembly)
{
	InitC46();
	LogInfo("D3DDisassembleRegion called\n"); 
	
	return (*_D3DDisassembleRegion)(pSrcData, SrcDataSize, Flags, szComments, StartByteOffset,
		NumInsts, pFinishByteOffset, ppDisassembly);
}
HRESULT WINAPI D3DDisassemble10Effect(_In_ interface ID3D10Effect *pEffect, 
                       _In_ UINT Flags,
                       _Out_ ID3DBlob** ppDisassembly)
{
	InitC46();
	LogInfo("D3DDisassemble10Effect called\n"); 
	
	return (*_D3DDisassemble10Effect)(pEffect, Flags, ppDisassembly);
}
HRESULT WINAPI D3DGetTraceInstructionOffsets(_In_reads_bytes_(SrcDataSize) LPCVOID pSrcData,
                              _In_ SIZE_T SrcDataSize,
                              _In_ UINT Flags,
                              _In_ SIZE_T StartInstIndex,
                              _In_ SIZE_T NumInsts,
                              _Out_writes_to_opt_(NumInsts, min(NumInsts, *pTotalInsts)) SIZE_T* pOffsets,
                              _Out_opt_ SIZE_T* pTotalInsts)
{
	InitC46();
	LogInfo("D3DGetTraceInstructionOffsets called\n"); 
	
	return (*_D3DGetTraceInstructionOffsets)(pSrcData, SrcDataSize, Flags, StartInstIndex, NumInsts,
		pOffsets, pTotalInsts);
}
HRESULT WINAPI D3DGetInputSignatureBlob(_In_reads_bytes_(SrcDataSize) LPCVOID pSrcData,
                         _In_ SIZE_T SrcDataSize,
                         _Out_ ID3DBlob** ppSignatureBlob)
{
	InitC46();
	//LogInfo("D3DGetInputSignatureBlob called\n"); 
	//
	return (*_D3DGetInputSignatureBlob)(pSrcData, SrcDataSize, ppSignatureBlob);
}
HRESULT WINAPI D3DGetOutputSignatureBlob(_In_reads_bytes_(SrcDataSize) LPCVOID pSrcData,
                          _In_ SIZE_T SrcDataSize,
                          _Out_ ID3DBlob** ppSignatureBlob)
{
	InitC46();
	LogInfo("D3DGetOutputSignatureBlob called\n"); 
	
	return (*_D3DGetOutputSignatureBlob)(pSrcData, SrcDataSize, ppSignatureBlob);
}
HRESULT WINAPI D3DGetInputAndOutputSignatureBlob(_In_reads_bytes_(SrcDataSize) LPCVOID pSrcData,
                                  _In_ SIZE_T SrcDataSize,
                                  _Out_ ID3DBlob** ppSignatureBlob)
{
	InitC46();
	LogInfo("D3DGetInputAndOutputSignatureBlob called\n"); 
	
	return (*_D3DGetInputAndOutputSignatureBlob)(pSrcData, SrcDataSize, ppSignatureBlob);
}
HRESULT WINAPI D3DStripShader(_In_reads_bytes_(BytecodeLength) LPCVOID pShaderBytecode,
               _In_ SIZE_T BytecodeLength,
               _In_ UINT uStripFlags,
               _Out_ ID3DBlob** ppStrippedBlob)
{
	InitC46();
	LogInfo("D3DStripShader called\n"); 
	
	return (*_D3DStripShader)(pShaderBytecode, BytecodeLength, uStripFlags, ppStrippedBlob);
}
HRESULT WINAPI D3DGetBlobPart(_In_reads_bytes_(SrcDataSize) LPCVOID pSrcData,
               _In_ SIZE_T SrcDataSize,
               _In_ D3D_BLOB_PART Part,
               _In_ UINT Flags,
               _Out_ ID3DBlob** ppPart)
{
	InitC46();
	LogInfo("D3DGetBlobPart called\n"); 
	
	return (*_D3DGetBlobPart)(pSrcData, SrcDataSize, Part, Flags, ppPart);
}
HRESULT WINAPI D3DSetBlobPart(_In_reads_bytes_(SrcDataSize) LPCVOID pSrcData,
               _In_ SIZE_T SrcDataSize,
               _In_ D3D_BLOB_PART Part,
               _In_ UINT Flags,
	       _In_reads_bytes_(PartSize) LPCVOID pPart,
               _In_ SIZE_T PartSize,
               _Out_ ID3DBlob** ppNewShader)
{
	InitC46();
	LogInfo("D3DSetBlobPart called\n"); 
	
	return (*_D3DSetBlobPart)(pSrcData, SrcDataSize, Part, Flags, pPart, PartSize, ppNewShader);
}
HRESULT WINAPI D3DCompressShaders(_In_ UINT uNumShaders,
                   _In_reads_(uNumShaders) D3D_SHADER_DATA* pShaderData,
                   _In_ UINT uFlags,
                   _Out_ ID3DBlob** ppCompressedData)
{
	InitC46();
	LogInfo("D3DCompressShaders called\n"); 
	
	return (*_D3DCompressShaders)(uNumShaders, pShaderData, uFlags, ppCompressedData);
}
HRESULT WINAPI D3DDecompressShaders(_In_reads_bytes_(SrcDataSize) LPCVOID pSrcData,
                     _In_ SIZE_T SrcDataSize,
                     _In_ UINT uNumShaders,	      
                     _In_ UINT uStartIndex,
                     _In_reads_opt_(uNumShaders) UINT* pIndices,
                     _In_ UINT uFlags,
                     _Out_writes_(uNumShaders) ID3DBlob** ppShaders,
		     _Out_opt_ UINT* pTotalShaders)
{
	InitC46();
	LogInfo("D3DDecompressShaders called\n"); 
	
	return (*_D3DDecompressShaders)(pSrcData, SrcDataSize, uNumShaders, uStartIndex, pIndices,
		uFlags, ppShaders, pTotalShaders);
}
HRESULT WINAPI D3DCreateBlob(_In_ SIZE_T Size,
              _Out_ ID3DBlob** ppBlob)
{
	InitC46();
	LogDebug("D3DCreateBlob called\n");	// Too chatty for LogInfo
	
	return (*_D3DCreateBlob)(Size, ppBlob);
}

HRESULT WINAPI D3DCompileFromMemory(_In_reads_bytes_(SrcDataSize) LPCVOID pSrcData,
           _In_ SIZE_T SrcDataSize,
           _In_opt_ LPCSTR pSourceName,
           _In_reads_opt_(_Inexpressible_(pDefines->Name != NULL)) CONST D3D10_SHADER_MACRO* pDefines,
           _In_opt_ ID3D10Include* pInclude,
           _In_ LPCSTR pEntrypoint,
           _In_ LPCSTR pTarget,
           _In_ UINT Flags1,
           _In_ UINT Flags2,
           _Out_ ID3D10Blob** ppCode,
           _Out_opt_ ID3D10Blob** ppErrorMsgs)
{
	InitC46();
	LogInfo("D3DCompileFromMemory called\n"); 
	
	return (*_D3DCompileFromMemory)(pSrcData, SrcDataSize, pSourceName, pDefines, pInclude,
		pEntrypoint, pTarget, Flags1, Flags2, ppCode, ppErrorMsgs);
}
HRESULT WINAPI D3DDisassembleCode(_In_reads_bytes_(SrcDataSize) LPCVOID pSrcData,
               _In_ SIZE_T SrcDataSize,
               _In_ UINT Flags,
               _Out_ SOutputContext *context,
               _Out_ ID3D10Blob** ppDisassembly)
{
	InitC46();
	LogInfo("D3DDisassembleCode called\n"); 
	
	return (*_D3DDisassembleCode)(pSrcData, SrcDataSize, Flags, context, ppDisassembly);
}
HRESULT WINAPI D3DDisassembleEffect(_In_ interface ID3D10Effect *pEffect, 
                       _In_ UINT Flags,
                       _Out_ ID3D10Blob** ppDisassembly)
{
	InitC46();
	LogInfo("D3DDisassembleEffect called\n"); 
	
	return (*_D3DDisassembleEffect)(pEffect, Flags, ppDisassembly);
}
HRESULT WINAPI D3DGetCodeDebugInfo(_In_reads_bytes_(SrcDataSize) LPCVOID pSrcData,
                _In_ SIZE_T SrcDataSize,
                _Out_ ID3D10Blob** ppDebugInfo)
{
	InitC46();
	LogInfo("D3DGetCodeDebugInfo called\n"); 
	
	return (*_D3DGetCodeDebugInfo)(pSrcData, SrcDataSize, ppDebugInfo);
}
HRESULT WINAPI D3DPreprocessFromMemory(_In_reads_bytes_(SrcDataSize) LPCVOID pSrcData,
              _In_ SIZE_T SrcDataSize,
              _In_opt_ LPCSTR pSourceName,
              _In_opt_ CONST D3D10_SHADER_MACRO* pDefines,
              _In_opt_ ID3D10Include* pInclude,
              _Out_ ID3D10Blob** ppCodeText,
              _Out_opt_ ID3D10Blob** ppErrorMsgs)
{
	InitC46();
	LogInfo("D3DPreprocessFromMemory called\n"); 
	
	return (*_D3DPreprocessFromMemory)(pSrcData, SrcDataSize, pSourceName, pDefines, pInclude,
		ppCodeText, ppErrorMsgs);
}
HRESULT WINAPI D3DReflectCode(GUID *interfaceId,
           _In_ INT unknown,
		   _In_reads_bytes_(SrcDataSize) LPCVOID pSrcData,
           _In_ SIZE_T SrcDataSize)
{
	InitC46();
	LogInfo("D3DReflectCode called\n"); 
	
	return (*_D3DReflectCode)(interfaceId, unknown, pSrcData, SrcDataSize);
}


// We need to get control as early as possible in order to pre-load our 
// d3d11 and nvapi64 dlls.  This will get called at static load time to
// get us hooked into place.  
//
// Be Careful of the restrictions on this entry point. No LogInfo e.g.

BOOL WINAPI DllMain(
	_In_  HINSTANCE hinstDLL,
	_In_  DWORD fdwReason,
	_In_  LPVOID lpvReserved)
{
	bool result = true;

	//bool waitfordebugger = false;
	//waitfordebugger = true;
	//do
	//{
	//	Sleep(250);
	//} while (!IsDebuggerPresent());
	//__debugbreak();

	switch (fdwReason)
	{
	case DLL_PROCESS_ATTACH:
	{
		wchar_t localPath[MAX_PATH] = L"d3d11.dll";
		hD3D11 = LoadLibrary(localPath);
		break;
	}

	case DLL_PROCESS_DETACH:
		break;

	case DLL_THREAD_ATTACH:
		// Do thread-specific initialization.
		break;

	case DLL_THREAD_DETACH:
		// Do thread-specific cleanup.
		break;
	}

	return result;
}
