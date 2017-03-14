#include "Main.h"
#include <Shlobj.h>
#include <Winuser.h>

#include "../util.h"
#include "globals.h"
#include "../HLSLDecompiler/DecompileHLSL.h"
#include "Override.h"
#include "IniHandler.h"
#include "Hunting.h"


ThreadSafePointerSet D3D10Wrapper::ID3D10Device::m_List;
ThreadSafePointerSet D3D10Wrapper::ID3D10Multithread::m_List;

Globals *G;
FILE *gLogFile = 0;
bool gLogDebug = false;


void InitializeDLL()
{
	if (!gInitialized)
	{

		LoadConfigFile();

		// NVAPI
		D3D10Base::NvAPI_Initialize();

		InitializeCriticalSection(&G->mCriticalSection);

		LogInfo("D3D10 DLL initialized.\n");
	}
}

void DestroyDLL()
{
	if (gLogFile)
	{
		LogInfo("Destroying DLL...\n");
		fclose(gLogFile);
	}
}

// D3DCompiler bridge
struct D3D10BridgeData
{
	UINT64 BinaryHash;
	char *HLSLFileName;
};

typedef ULONG 	D3DKMT_HANDLE;
typedef int		KMTQUERYADAPTERINFOTYPE;

typedef struct _D3DKMT_QUERYADAPTERINFO 
{
  D3DKMT_HANDLE           hAdapter;
  KMTQUERYADAPTERINFOTYPE Type;
  VOID                    *pPrivateDriverData;
  UINT                    PrivateDriverDataSize;
} D3DKMT_QUERYADAPTERINFO;

typedef void *D3D10DDI_HRTADAPTER;
typedef void *D3D10DDI_HADAPTER;
typedef void D3DDDI_ADAPTERCALLBACKS;
typedef void D3D10DDI_ADAPTERFUNCS;
typedef void D3D10_2DDI_ADAPTERFUNCS;

typedef struct D3D10DDIARG_OPENADAPTER 
{
  D3D10DDI_HRTADAPTER           hRTAdapter;
  D3D10DDI_HADAPTER             hAdapter;
  UINT                          Interface;
  UINT                          Version;
  const D3DDDI_ADAPTERCALLBACKS *pAdapterCallbacks;
  union {
    D3D10DDI_ADAPTERFUNCS   *pAdapterFuncs;
    D3D10_2DDI_ADAPTERFUNCS *pAdapterFuncs_2;
  };
} D3D10DDIARG_OPENADAPTER;

static HMODULE hD3D10 = 0;
typedef HRESULT (WINAPI *tD3D10CompileEffectFromMemory)(void *pData, SIZE_T DataLength, LPCSTR pSrcFileName, 
	const D3D10Base::D3D10_SHADER_MACRO *pDefines, D3D10Base::ID3D10Include *pInclude, UINT HLSLFlags, UINT FXFlags, 
	D3D10Base::ID3D10Blob **ppCompiledEffect, D3D10Base::ID3D10Blob **ppErrors);
static tD3D10CompileEffectFromMemory _D3D10CompileEffectFromMemory;
typedef HRESULT (WINAPI *tD3D10CompileShader)(LPCSTR pSrcData, SIZE_T SrcDataLen, LPCSTR pFileName, 
	const D3D10Base::D3D10_SHADER_MACRO *pDefines, D3D10Base::LPD3D10INCLUDE pInclude, LPCSTR pFunctionName, 
	LPCSTR pProfile, UINT Flags, D3D10Base::ID3D10Blob **ppShader, D3D10Base::ID3D10Blob **ppErrorMsgs);
static tD3D10CompileShader _D3D10CompileShader;
typedef HRESULT (WINAPI *tD3D10CreateBlob)(SIZE_T NumBytes, D3D10Base::LPD3D10BLOB *ppBuffer);
static tD3D10CreateBlob _D3D10CreateBlob;
typedef HRESULT (WINAPI *tD3D10CreateDevice)(D3D10Base::IDXGIAdapter *pAdapter, D3D10Base::D3D10_DRIVER_TYPE DriverType, 
	HMODULE Software, UINT Flags, UINT SDKVersion, D3D10Base::ID3D10Device **ppDevice);
static tD3D10CreateDevice _D3D10CreateDevice;
typedef HRESULT (WINAPI *tD3D10CreateDeviceAndSwapChain)(D3D10Base::IDXGIAdapter *pAdapter, D3D10Base::D3D10_DRIVER_TYPE DriverType, 
	HMODULE Software, UINT Flags, UINT SDKVersion, D3D10Base::DXGI_SWAP_CHAIN_DESC *pSwapChainDesc, 
	D3D10Base::IDXGISwapChain **ppSwapChain, D3D10Base::ID3D10Device **ppDevice);
static tD3D10CreateDeviceAndSwapChain _D3D10CreateDeviceAndSwapChain;
typedef HRESULT (WINAPI *tD3D10CreateEffectFromMemory)(void *pData, SIZE_T DataLength, UINT FXFlags, 
	D3D10Base::ID3D10Device *pDevice, D3D10Base::ID3D10EffectPool *pEffectPool, D3D10Base::ID3D10Effect **ppEffect);
static tD3D10CreateEffectFromMemory _D3D10CreateEffectFromMemory;
typedef HRESULT (WINAPI *tD3D10CreateEffectPoolFromMemory)(void *pData, SIZE_T DataLength, UINT FXFlags, 
	D3D10Base::ID3D10Device *pDevice, D3D10Base::ID3D10EffectPool **ppEffectPool);
static tD3D10CreateEffectPoolFromMemory _D3D10CreateEffectPoolFromMemory;
typedef HRESULT (WINAPI *tD3D10CreateStateBlock)(D3D10Base::ID3D10Device *pDevice, D3D10Base::D3D10_STATE_BLOCK_MASK *pStateBlockMask,
	D3D10Base::ID3D10StateBlock **ppStateBlock);
static tD3D10CreateStateBlock _D3D10CreateStateBlock;
typedef HRESULT (WINAPI *tD3D10DisassembleEffect)(D3D10Base::ID3D10Effect *pEffect, BOOL EnableColorCode, D3D10Base::ID3D10Blob **ppDisassembly);
static tD3D10DisassembleEffect _D3D10DisassembleEffect;
typedef HRESULT (WINAPI *tD3D10DisassembleShader)(const void *pShader, SIZE_T BytecodeLength, BOOL EnableColorCode, LPCSTR pComments, 
	D3D10Base::ID3D10Blob **ppDisassembly);
static tD3D10DisassembleShader _D3D10DisassembleShader;
typedef LPCSTR (WINAPI *tD3D10GetGeometryShaderProfile)(D3D10Base::ID3D10Device *pDevice);
static tD3D10GetGeometryShaderProfile _D3D10GetGeometryShaderProfile;
typedef HRESULT (WINAPI *tD3D10GetInputAndOutputSignatureBlob)(const void *pShaderBytecode, SIZE_T BytecodeLength, 
	D3D10Base::ID3D10Blob **ppSignatureBlob);
static tD3D10GetInputAndOutputSignatureBlob _D3D10GetInputAndOutputSignatureBlob;
typedef HRESULT (WINAPI *tD3D10GetInputSignatureBlob)(const void *pShaderBytecode, SIZE_T BytecodeLength, 
	D3D10Base::ID3D10Blob **ppSignatureBlob);
static tD3D10GetInputSignatureBlob _D3D10GetInputSignatureBlob;
typedef HRESULT (WINAPI *tD3D10GetOutputSignatureBlob)(const void *pShaderBytecode, SIZE_T BytecodeLength, 
	D3D10Base::ID3D10Blob **ppSignatureBlob);
static tD3D10GetOutputSignatureBlob _D3D10GetOutputSignatureBlob;
typedef LPCSTR (WINAPI *tD3D10GetPixelShaderProfile)(D3D10Base::ID3D10Device *pDevice);
static tD3D10GetPixelShaderProfile _D3D10GetPixelShaderProfile;
typedef HRESULT (WINAPI *tD3D10GetShaderDebugInfo)(const void *pShaderBytecode, SIZE_T BytecodeLength, D3D10Base::ID3D10Blob **ppDebugInfo);
static tD3D10GetShaderDebugInfo _D3D10GetShaderDebugInfo;
typedef int (WINAPI *tD3D10GetVersion)();
static tD3D10GetVersion _D3D10GetVersion;
typedef LPCSTR (WINAPI *tD3D10GetVertexShaderProfile)(D3D10Base::ID3D10Device *pDevice);
static tD3D10GetVertexShaderProfile _D3D10GetVertexShaderProfile;
typedef HRESULT (WINAPI *tD3D10PreprocessShader)(LPCSTR pSrcData, SIZE_T SrcDataSize, LPCSTR pFileName, 
	const D3D10Base::D3D10_SHADER_MACRO *pDefines, D3D10Base::LPD3D10INCLUDE pInclude, 
	D3D10Base::ID3D10Blob **ppShaderText, D3D10Base::ID3D10Blob **ppErrorMsgs);
static tD3D10PreprocessShader _D3D10PreprocessShader;
typedef HRESULT (WINAPI *tD3D10ReflectShader)(const void *pShaderBytecode, SIZE_T BytecodeLength, 
	D3D10Base::ID3D10ShaderReflection **ppReflector);
static tD3D10ReflectShader _D3D10ReflectShader;
typedef int (WINAPI *tD3D10RegisterLayers)();
static tD3D10RegisterLayers _D3D10RegisterLayers;
typedef HRESULT (WINAPI *tD3D10StateBlockMaskDifference)(D3D10Base::D3D10_STATE_BLOCK_MASK *pA, 
	D3D10Base::D3D10_STATE_BLOCK_MASK *pB, D3D10Base::D3D10_STATE_BLOCK_MASK *pResult);
static tD3D10StateBlockMaskDifference _D3D10StateBlockMaskDifference;
typedef HRESULT (WINAPI *tD3D10StateBlockMaskDisableAll)(D3D10Base::D3D10_STATE_BLOCK_MASK *pMask);
static tD3D10StateBlockMaskDisableAll _D3D10StateBlockMaskDisableAll;
typedef HRESULT (WINAPI *tD3D10StateBlockMaskDisableCapture)(D3D10Base::D3D10_STATE_BLOCK_MASK *pMask, 
	D3D10Base::D3D10_DEVICE_STATE_TYPES StateType, UINT RangeStart, UINT RangeLength);
static tD3D10StateBlockMaskDisableCapture _D3D10StateBlockMaskDisableCapture;
typedef HRESULT (WINAPI *tD3D10StateBlockMaskEnableAll)(D3D10Base::D3D10_STATE_BLOCK_MASK *pMask);
static tD3D10StateBlockMaskEnableAll _D3D10StateBlockMaskEnableAll;
typedef HRESULT (WINAPI *tD3D10StateBlockMaskEnableCapture)(D3D10Base::D3D10_STATE_BLOCK_MASK *pMask, 
	D3D10Base::D3D10_DEVICE_STATE_TYPES StateType, UINT RangeStart, UINT RangeLength);
static tD3D10StateBlockMaskEnableCapture _D3D10StateBlockMaskEnableCapture;
typedef BOOL (WINAPI *tD3D10StateBlockMaskGetSetting)(D3D10Base::D3D10_STATE_BLOCK_MASK *pMask, 
	D3D10Base::D3D10_DEVICE_STATE_TYPES StateType, UINT Entry);
static tD3D10StateBlockMaskGetSetting _D3D10StateBlockMaskGetSetting;
typedef HRESULT (WINAPI *tD3D10StateBlockMaskIntersect)(D3D10Base::D3D10_STATE_BLOCK_MASK *pA, 
	D3D10Base::D3D10_STATE_BLOCK_MASK *pB, D3D10Base::D3D10_STATE_BLOCK_MASK *pResult);
static tD3D10StateBlockMaskIntersect _D3D10StateBlockMaskIntersect;
typedef HRESULT (WINAPI *tD3D10StateBlockMaskUnion)(D3D10Base::D3D10_STATE_BLOCK_MASK *pA, 
	D3D10Base::D3D10_STATE_BLOCK_MASK *pB, D3D10Base::D3D10_STATE_BLOCK_MASK *pResult);
static tD3D10StateBlockMaskUnion _D3D10StateBlockMaskUnion;
typedef int (WINAPI *tD3DKMTQueryAdapterInfo)(_D3DKMT_QUERYADAPTERINFO *);
static tD3DKMTQueryAdapterInfo _D3DKMTQueryAdapterInfo;
typedef int (WINAPI *tOpenAdapter10)(D3D10DDIARG_OPENADAPTER *adapter);
static tOpenAdapter10 _OpenAdapter10;
typedef int (WINAPI *tOpenAdapter10_2)(D3D10DDIARG_OPENADAPTER *adapter);
static tOpenAdapter10_2 _OpenAdapter10_2;

typedef int (WINAPI *tD3D10CoreCreateDevice)(__int32, int, int, LPCSTR lpModuleName, int, int, int, int, int, int);
static tD3D10CoreCreateDevice _D3D10CoreCreateDevice;

//typedef int (WINAPI *tD3D10CoreCreateLayeredDevice)(int a, int b, int c, int d, int e);
//static tD3D10CoreCreateLayeredDevice _D3D10CoreCreateLayeredDevice;
typedef HRESULT(WINAPI *tD3D10CoreCreateLayeredDevice)(const void *unknown0, DWORD unknown1, const void *unknown2, REFIID riid, void **ppvObj);
static tD3D10CoreCreateLayeredDevice _D3D10CoreCreateLayeredDevice;

//typedef int (WINAPI *tD3D10CoreGetLayeredDeviceSize)(int a, int b);
//static tD3D10CoreGetLayeredDeviceSize _D3D10CoreGetLayeredDeviceSize;
typedef SIZE_T(WINAPI *tD3D10CoreGetLayeredDeviceSize)(const void *unknown0, DWORD unknown1);
static tD3D10CoreGetLayeredDeviceSize _D3D10CoreGetLayeredDeviceSize;

//typedef int (WINAPI *tD3D10CoreRegisterLayers)(int a, int b);
//static tD3D10CoreRegisterLayers _D3D10CoreRegisterLayers;
typedef HRESULT(WINAPI *tD3D10CoreRegisterLayers)(const void *unknown0, DWORD unknown1);
static tD3D10CoreRegisterLayers _D3D10CoreRegisterLayers;

static void InitD310()
{
	if (hD3D10) return;

	G = new Globals();

	InitializeDLL();
	wchar_t sysDir[MAX_PATH];
	SHGetFolderPath(0, CSIDL_SYSTEM, 0, SHGFP_TYPE_CURRENT, sysDir);
	wcscat(sysDir, L"\\d3d10.dll");
	hD3D10 = LoadLibrary(sysDir);	
    if (hD3D10 == NULL)
    {
        LogInfo("LoadLibrary on d3d10.dll failed\n");
        
        return;
    }

	_D3D10CompileEffectFromMemory = (tD3D10CompileEffectFromMemory) GetProcAddress(hD3D10, "D3D10CompileEffectFromMemory");
	_D3D10CompileShader = (tD3D10CompileShader) GetProcAddress(hD3D10, "D3D10CompileShader");
	_D3D10CreateBlob = (tD3D10CreateBlob) GetProcAddress(hD3D10, "D3D10CreateBlob");
	_D3D10CreateDevice = (tD3D10CreateDevice) GetProcAddress(hD3D10, "D3D10CreateDevice");
	_D3D10CreateDeviceAndSwapChain = (tD3D10CreateDeviceAndSwapChain) GetProcAddress(hD3D10, "D3D10CreateDeviceAndSwapChain");
	_D3D10CreateEffectFromMemory = (tD3D10CreateEffectFromMemory) GetProcAddress(hD3D10, "D3D10CreateEffectFromMemory");
	_D3D10CreateEffectPoolFromMemory = (tD3D10CreateEffectPoolFromMemory) GetProcAddress(hD3D10, "D3D10CreateEffectPoolFromMemory");
	_D3D10CreateStateBlock = (tD3D10CreateStateBlock) GetProcAddress(hD3D10, "D3D10CreateStateBlock");
	_D3D10DisassembleEffect = (tD3D10DisassembleEffect) GetProcAddress(hD3D10, "D3D10DisassembleEffect");
	_D3D10DisassembleShader = (tD3D10DisassembleShader) GetProcAddress(hD3D10, "D3D10DisassembleShader");
	_D3D10GetGeometryShaderProfile = (tD3D10GetGeometryShaderProfile) GetProcAddress(hD3D10, "D3D10GetGeometryShaderProfile");
	_D3D10GetInputAndOutputSignatureBlob = (tD3D10GetInputAndOutputSignatureBlob) GetProcAddress(hD3D10, "D3D10GetInputAndOutputSignatureBlob");
	_D3D10GetInputSignatureBlob = (tD3D10GetInputSignatureBlob) GetProcAddress(hD3D10, "D3D10GetInputSignatureBlob");
	_D3D10GetOutputSignatureBlob = (tD3D10GetOutputSignatureBlob) GetProcAddress(hD3D10, "D3D10GetOutputSignatureBlob");
	_D3D10GetPixelShaderProfile = (tD3D10GetPixelShaderProfile) GetProcAddress(hD3D10, "D3D10GetPixelShaderProfile");
	_D3D10GetShaderDebugInfo = (tD3D10GetShaderDebugInfo) GetProcAddress(hD3D10, "D3D10GetShaderDebugInfo");
	_D3D10GetVersion = (tD3D10GetVersion) GetProcAddress(hD3D10, "D3D10GetVersion");
	_D3D10GetVertexShaderProfile = (tD3D10GetVertexShaderProfile) GetProcAddress(hD3D10, "D3D10GetVertexShaderProfile");
	_D3D10PreprocessShader = (tD3D10PreprocessShader) GetProcAddress(hD3D10, "D3D10PreprocessShader");
	_D3D10ReflectShader = (tD3D10ReflectShader) GetProcAddress(hD3D10, "D3D10ReflectShader");
	_D3D10RegisterLayers = (tD3D10RegisterLayers) GetProcAddress(hD3D10, "D3D10RegisterLayers");
	_D3D10StateBlockMaskDifference = (tD3D10StateBlockMaskDifference) GetProcAddress(hD3D10, "D3D10StateBlockMaskDifference");
	_D3D10StateBlockMaskDisableAll = (tD3D10StateBlockMaskDisableAll) GetProcAddress(hD3D10, "D3D10StateBlockMaskDisableAll");
	_D3D10StateBlockMaskDisableCapture = (tD3D10StateBlockMaskDisableCapture) GetProcAddress(hD3D10, "D3D10StateBlockMaskDisableCapture");
	_D3D10StateBlockMaskEnableAll = (tD3D10StateBlockMaskEnableAll) GetProcAddress(hD3D10, "D3D10StateBlockMaskEnableAll");
	_D3D10StateBlockMaskEnableCapture = (tD3D10StateBlockMaskEnableCapture) GetProcAddress(hD3D10, "D3D10StateBlockMaskEnableCapture");
	_D3D10StateBlockMaskGetSetting = (tD3D10StateBlockMaskGetSetting) GetProcAddress(hD3D10, "D3D10StateBlockMaskGetSetting");
	_D3D10StateBlockMaskIntersect = (tD3D10StateBlockMaskIntersect) GetProcAddress(hD3D10, "D3D10StateBlockMaskIntersect");
	_D3D10StateBlockMaskUnion = (tD3D10StateBlockMaskUnion) GetProcAddress(hD3D10, "D3D10StateBlockMaskUnion");
	_D3DKMTQueryAdapterInfo = (tD3DKMTQueryAdapterInfo) GetProcAddress(hD3D10, "D3DKMTQueryAdapterInfo");
	_OpenAdapter10 = (tOpenAdapter10) GetProcAddress(hD3D10, "OpenAdapter10");
	_OpenAdapter10_2 = (tOpenAdapter10_2) GetProcAddress(hD3D10, "OpenAdapter10_2");
	_D3D10CoreCreateDevice = (tD3D10CoreCreateDevice) GetProcAddress(hD3D10, "D3D10CoreCreateDevice");
	_D3D10CoreCreateLayeredDevice = (tD3D10CoreCreateLayeredDevice) GetProcAddress(hD3D10, "D3D10CoreCreateLayeredDevice");
	_D3D10CoreGetLayeredDeviceSize = (tD3D10CoreGetLayeredDeviceSize) GetProcAddress(hD3D10, "D3D10CoreGetLayeredDeviceSize");
	_D3D10CoreRegisterLayers = (tD3D10CoreRegisterLayers) GetProcAddress(hD3D10, "D3D10CoreRegisterLayers");
}

HRESULT WINAPI D3D10CompileEffectFromMemory(void *pData, SIZE_T DataLength, LPCSTR pSrcFileName, 
	const D3D10Base::D3D10_SHADER_MACRO *pDefines, D3D10Base::ID3D10Include *pInclude, UINT HLSLFlags, UINT FXFlags, 
	D3D10Base::ID3D10Blob **ppCompiledEffect, D3D10Base::ID3D10Blob **ppErrors)
{
	InitD310();
	LogInfo("D3D10CompileEffectFromMemory called.\n");
	
	return (*_D3D10CompileEffectFromMemory)(pData, DataLength, pSrcFileName, 
		pDefines, pInclude, HLSLFlags, FXFlags, 
		ppCompiledEffect, ppErrors);
}

HRESULT WINAPI D3D10CompileShader(LPCSTR pSrcData, SIZE_T SrcDataLen, LPCSTR pFileName, 
	const D3D10Base::D3D10_SHADER_MACRO *pDefines, D3D10Base::LPD3D10INCLUDE pInclude, LPCSTR pFunctionName, 
	LPCSTR pProfile, UINT Flags, D3D10Base::ID3D10Blob **ppShader, D3D10Base::ID3D10Blob **ppErrorMsgs)
{
	InitD310();
	LogInfo("D3D10CompileShader called.\n");
	
	return (*_D3D10CompileShader)(pSrcData, SrcDataLen, pFileName, 
		pDefines, pInclude, pFunctionName, 
		pProfile, Flags, ppShader, ppErrorMsgs);
}

HRESULT WINAPI D3D10CreateBlob(SIZE_T NumBytes, D3D10Base::LPD3D10BLOB *ppBuffer)
{
	InitD310();
	LogInfo("D3D10CreateBlob called.\n");
	
	return (*_D3D10CreateBlob)(NumBytes, ppBuffer);
}

static void EnableStereo()
{
	if (!G->gForceStereo) return;

	// Prepare NVAPI for use in this application
	D3D10Base::NvAPI_Status status;
	status = D3D10Base::NvAPI_Initialize();
	if (status != D3D10Base::NVAPI_OK)
	{
		D3D10Base::NvAPI_ShortString errorMessage;
		NvAPI_GetErrorMessage(status, errorMessage);
		LogInfo("  stereo init failed: %s\n", errorMessage);		
	}
	else
	{
		// Check the Stereo availability
		D3D10Base::NvU8 isStereoEnabled;
		status = D3D10Base::NvAPI_Stereo_IsEnabled(&isStereoEnabled);
		// Stereo status report an error
		if ( status != D3D10Base::NVAPI_OK)
		{
			// GeForce Stereoscopic 3D driver is not installed on the system
			LogInfo("  stereo init failed: no stereo driver detected.\n");		
		}
		// Stereo is available but not enabled, let's enable it
		else if(D3D10Base::NVAPI_OK == status && !isStereoEnabled)
		{
			LogInfo("  stereo available and disabled. Enabling stereo.\n");		
			status = D3D10Base::NvAPI_Stereo_Enable();
			if (status != D3D10Base::NVAPI_OK)
				LogInfo("    enabling stereo failed.\n");		
		}

		if (G->gCreateStereoProfile)
		{
			LogInfo("  enabling registry profile.\n");		
			
			D3D10Base::NvAPI_Stereo_CreateConfigurationProfileRegistryKey(D3D10Base::NVAPI_STEREO_DEFAULT_REGISTRY_PROFILE);
		}
	}
}

static D3D10Base::IDXGIAdapter *ReplaceAdapter(D3D10Base::IDXGIAdapter *wrapper)
{
	if (!wrapper)
		return wrapper;
	LogInfo("  checking for adapter wrapper, handle = %x\n", wrapper);
	IID marker = { 0x017b2e72ul, 0xbcde, 0x9f15, { 0xa1, 0x2b, 0x3c, 0x4d, 0x5e, 0x6f, 0x70, 0x00 } };
	D3D10Base::IDXGIAdapter *realAdapter;
	if (wrapper->GetParent(marker, (void **) &realAdapter) == 0x13bc7e32)
	{
		LogInfo("    wrapper found. replacing with original handle = %x\n", realAdapter);
		
		return realAdapter;
	}
	return wrapper;
}

HRESULT WINAPI D3D10CreateDevice(D3D10Base::IDXGIAdapter *pAdapter, D3D10Base::D3D10_DRIVER_TYPE DriverType, 
	HMODULE Software, UINT Flags, UINT SDKVersion, D3D10Wrapper::ID3D10Device **ppDevice)
{
	InitD310();
	LogInfo("D3D10CreateDevice called with adapter = %x\n", pAdapter);
	
	D3D10Base::ID3D10Device *origDevice = 0;
	EnableStereo();
	HRESULT ret = (*_D3D10CreateDevice)(ReplaceAdapter(pAdapter), DriverType, Software, Flags, SDKVersion, &origDevice);
	if (ret != S_OK)
	{
		LogInfo("  failed with HRESULT=%x\n", ret);
		
		return ret;
	}

	D3D10Wrapper::ID3D10Device *wrapper = D3D10Wrapper::ID3D10Device::GetDirect3DDevice(origDevice);
	if (!wrapper)
	{
		LogInfo("  error allocating wrapper.\n");
		
		origDevice->Release();
		return E_OUTOFMEMORY;
	}
	*ppDevice = wrapper;

	LogInfo("  returns result = %x, handle = %x, wrapper = %x\n", ret, origDevice, wrapper);
	
	return ret;
}

HRESULT WINAPI D3D10CreateDeviceAndSwapChain(D3D10Base::IDXGIAdapter *pAdapter, D3D10Base::D3D10_DRIVER_TYPE DriverType, 
	HMODULE Software, UINT Flags, UINT SDKVersion, D3D10Base::DXGI_SWAP_CHAIN_DESC *pSwapChainDesc, 
	D3D10Base::IDXGISwapChain **ppSwapChain, D3D10Wrapper::ID3D10Device **ppDevice)
{
	InitD310();
	LogInfo("D3D10CreateDeviceAndSwapChain called with adapter = %x\n", pAdapter);
	if (pSwapChainDesc) LogInfo(" Windowed = %d\n", pSwapChainDesc->Windowed);
	if (pSwapChainDesc) LogInfo(" Width = %d\n", pSwapChainDesc->BufferDesc.Width);
	if (pSwapChainDesc) LogInfo(" Height = %d\n", pSwapChainDesc->BufferDesc.Height);
	if (pSwapChainDesc) LogInfo(" Refresh rate = %f\n", 
		(float) pSwapChainDesc->BufferDesc.RefreshRate.Numerator / (float) pSwapChainDesc->BufferDesc.RefreshRate.Denominator);

	if (G->SCREEN_FULLSCREEN >= 0 && pSwapChainDesc) pSwapChainDesc->Windowed = !G->SCREEN_FULLSCREEN;
	if (G->SCREEN_REFRESH >= 0 && pSwapChainDesc && !pSwapChainDesc->Windowed)
	{
		pSwapChainDesc->BufferDesc.RefreshRate.Numerator = G->SCREEN_REFRESH;
		pSwapChainDesc->BufferDesc.RefreshRate.Denominator = 1;
	}
	if (G->SCREEN_WIDTH >= 0 && pSwapChainDesc) pSwapChainDesc->BufferDesc.Width = G->SCREEN_WIDTH;
	if (G->SCREEN_HEIGHT >= 0 && pSwapChainDesc) pSwapChainDesc->BufferDesc.Height = G->SCREEN_HEIGHT;
	if (pSwapChainDesc) LogInfo("  calling with parameters width = %d, height = %d, refresh rate = %f, windowed = %d\n", 
		pSwapChainDesc->BufferDesc.Width, pSwapChainDesc->BufferDesc.Height, 
		(float) pSwapChainDesc->BufferDesc.RefreshRate.Numerator / (float) pSwapChainDesc->BufferDesc.RefreshRate.Denominator,
		pSwapChainDesc->Windowed);

	EnableStereo();

	D3D10Base::ID3D10Device *origDevice = 0;
	HRESULT ret = (*_D3D10CreateDeviceAndSwapChain)(ReplaceAdapter(pAdapter), DriverType, Software, Flags, SDKVersion, 
		pSwapChainDesc, ppSwapChain, &origDevice);
	
	// Changed to recognize that >0 DXGISTATUS values are possible, not just S_OK.
	if (FAILED(ret))
	{
		LogInfo("  failed with HRESULT=%x\n", ret);
		return ret;
	}

	D3D10Wrapper::ID3D10Device *wrapper = D3D10Wrapper::ID3D10Device::GetDirect3DDevice(origDevice);
	if (wrapper == NULL)
	{
		LogInfo("  error allocating wrapper.\n");
		
		origDevice->Release();
		return E_OUTOFMEMORY;
	}
	*ppDevice = wrapper;

	LogInfo("  returns result = %x, handle = %x, wrapper = %x\n", ret, origDevice, wrapper);
	
	return ret;
}

HRESULT WINAPI D3D10CreateEffectFromMemory(void *pData, SIZE_T DataLength, UINT FXFlags, 
	D3D10Base::ID3D10Device *pDevice, D3D10Base::ID3D10EffectPool *pEffectPool, D3D10Base::ID3D10Effect **ppEffect)
{
	InitD310();
	LogInfo("D3D10CreateEffectFromMemory called.\n");
	
	return (*_D3D10CreateEffectFromMemory)(pData, DataLength, FXFlags, 
		pDevice, pEffectPool, ppEffect);
}

HRESULT WINAPI D3D10CreateEffectPoolFromMemory(void *pData, SIZE_T DataLength, UINT FXFlags, 
	D3D10Base::ID3D10Device *pDevice, D3D10Base::ID3D10EffectPool **ppEffectPool)
{
	InitD310();
	LogInfo("D3D10CreateEffectPoolFromMemory called.\n");
	
	return (*_D3D10CreateEffectPoolFromMemory)(pData, DataLength, FXFlags, 
		pDevice, ppEffectPool);
}

HRESULT WINAPI D3D10CreateStateBlock(D3D10Base::ID3D10Device *pDevice, D3D10Base::D3D10_STATE_BLOCK_MASK *pStateBlockMask,
	D3D10Base::ID3D10StateBlock **ppStateBlock)
{
	InitD310();
	LogInfo("D3D10CreateStateBlock called.\n");
	
	return (*_D3D10CreateStateBlock)(pDevice, pStateBlockMask,
		ppStateBlock);
}

HRESULT WINAPI D3D10DisassembleEffect(D3D10Base::ID3D10Effect *pEffect, BOOL EnableColorCode, D3D10Base::ID3D10Blob **ppDisassembly)
{
	InitD310();
	LogInfo("D3D10DisassembleEffect called.\n");
	
	return (*_D3D10DisassembleEffect)(pEffect, EnableColorCode, ppDisassembly);
}

HRESULT WINAPI D3D10DisassembleShader(const void *pShader, SIZE_T BytecodeLength, BOOL EnableColorCode, LPCSTR pComments, 
	D3D10Base::ID3D10Blob **ppDisassembly)
{
	InitD310();
	LogInfo("D3D10DisassembleShader called.\n");
	
	return (*_D3D10DisassembleShader)(pShader, BytecodeLength, EnableColorCode, pComments, 
		ppDisassembly);
}

LPCSTR WINAPI D3D10GetGeometryShaderProfile(D3D10Base::ID3D10Device *pDevice)
{
	InitD310();
	LogInfo("D3D10GetGeometryShaderProfile called.\n");
	
	return (*_D3D10GetGeometryShaderProfile)(pDevice);
}

HRESULT WINAPI D3D10GetInputAndOutputSignatureBlob(const void *pShaderBytecode, SIZE_T BytecodeLength, 
	D3D10Base::ID3D10Blob **ppSignatureBlob)
{
	InitD310();
	LogInfo("D3D10GetInputAndOutputSignatureBlob called.\n");
	
	return (*_D3D10GetInputAndOutputSignatureBlob)(pShaderBytecode, BytecodeLength, 
		ppSignatureBlob);
}

HRESULT WINAPI D3D10GetInputSignatureBlob(const void *pShaderBytecode, SIZE_T BytecodeLength, 
	D3D10Base::ID3D10Blob **ppSignatureBlob)
{
	InitD310();
	LogInfo("D3D10GetInputSignatureBlob called.\n");
	
	return (*_D3D10GetInputSignatureBlob)(pShaderBytecode, BytecodeLength, 
		ppSignatureBlob);
}

HRESULT WINAPI D3D10GetOutputSignatureBlob(const void *pShaderBytecode, SIZE_T BytecodeLength, 
	D3D10Base::ID3D10Blob **ppSignatureBlob)
{
	InitD310();
	LogInfo("D3D10GetOutputSignatureBlob called.\n");
	
	return (*_D3D10GetOutputSignatureBlob)(pShaderBytecode, BytecodeLength, 
		ppSignatureBlob);
}

LPCSTR WINAPI D3D10GetPixelShaderProfile(D3D10Base::ID3D10Device *pDevice)
{
	InitD310();
	LogInfo("D3D10GetPixelShaderProfile called.\n");
	
	return (*_D3D10GetPixelShaderProfile)(pDevice);
}

HRESULT WINAPI D3D10GetShaderDebugInfo(const void *pShaderBytecode, SIZE_T BytecodeLength, D3D10Base::ID3D10Blob **ppDebugInfo)
{
	InitD310();
	LogInfo("D3D10GetShaderDebugInfo called.\n");
	
	return (*_D3D10GetShaderDebugInfo)(pShaderBytecode, BytecodeLength, ppDebugInfo);
}

int WINAPI D3D10GetVersion()
{
	InitD310();
	LogInfo("D3D10GetVersion called.\n");
	
	return (*_D3D10GetVersion)();
}

LPCSTR WINAPI D3D10GetVertexShaderProfile(D3D10Base::ID3D10Device *pDevice)
{
	InitD310();
	LogInfo("D3D10GetVertexShaderProfile called.\n");
	
	return (*_D3D10GetVertexShaderProfile)(pDevice);
}

HRESULT WINAPI D3D10PreprocessShader(LPCSTR pSrcData, SIZE_T SrcDataSize, LPCSTR pFileName, 
	const D3D10Base::D3D10_SHADER_MACRO *pDefines, D3D10Base::LPD3D10INCLUDE pInclude, 
	D3D10Base::ID3D10Blob **ppShaderText, D3D10Base::ID3D10Blob **ppErrorMsgs)
{
	InitD310();
	LogInfo("D3D10PreprocessShader called.\n");
	
	return (*_D3D10PreprocessShader)(pSrcData, SrcDataSize, pFileName, 
		pDefines, pInclude, 
		ppShaderText, ppErrorMsgs);
}

HRESULT WINAPI D3D10ReflectShader(const void *pShaderBytecode, SIZE_T BytecodeLength, 
	D3D10Base::ID3D10ShaderReflection **ppReflector)
{
	InitD310();
	LogInfo("D3D10ReflectShader called.\n");
	
	return (*_D3D10ReflectShader)(pShaderBytecode, BytecodeLength, 
		ppReflector);
}

int WINAPI D3D10RegisterLayers()
{
	InitD310();
	LogInfo("D3D10RegisterLayers called.\n");
	
	return (*_D3D10RegisterLayers)();
}

HRESULT WINAPI D3D10StateBlockMaskDifference(D3D10Base::D3D10_STATE_BLOCK_MASK *pA, 
	D3D10Base::D3D10_STATE_BLOCK_MASK *pB, D3D10Base::D3D10_STATE_BLOCK_MASK *pResult)
{
	InitD310();
	LogInfo("D3D10StateBlockMaskDifference called.\n");
	
	return (*_D3D10StateBlockMaskDifference)(pA, 
		pB, pResult);
}

HRESULT WINAPI D3D10StateBlockMaskDisableAll(D3D10Base::D3D10_STATE_BLOCK_MASK *pMask)
{
	InitD310();
	LogInfo("D3D10StateBlockMaskDisableAll called.\n");
	
	return (*_D3D10StateBlockMaskDisableAll)(pMask);
}

HRESULT WINAPI D3D10StateBlockMaskDisableCapture(D3D10Base::D3D10_STATE_BLOCK_MASK *pMask, 
	D3D10Base::D3D10_DEVICE_STATE_TYPES StateType, UINT RangeStart, UINT RangeLength)
{
	InitD310();
	LogInfo("D3D10StateBlockMaskDisableCapture called.\n");
	
	return (*_D3D10StateBlockMaskDisableCapture)(pMask, 
		StateType, RangeStart, RangeLength);
}

HRESULT WINAPI D3D10StateBlockMaskEnableAll(D3D10Base::D3D10_STATE_BLOCK_MASK *pMask)
{
	InitD310();
	LogInfo("D3D10StateBlockMaskEnableAll called.\n");
	
	return (*_D3D10StateBlockMaskEnableAll)(pMask);
}

HRESULT WINAPI D3D10StateBlockMaskEnableCapture(D3D10Base::D3D10_STATE_BLOCK_MASK *pMask, 
	D3D10Base::D3D10_DEVICE_STATE_TYPES StateType, UINT RangeStart, UINT RangeLength)
{
	InitD310();
	LogInfo("D3D10StateBlockMaskEnableCapture called.\n");
	
	return (*_D3D10StateBlockMaskEnableCapture)(pMask, 
		StateType, RangeStart, RangeLength);
}

BOOL WINAPI D3D10StateBlockMaskGetSetting(D3D10Base::D3D10_STATE_BLOCK_MASK *pMask, 
	D3D10Base::D3D10_DEVICE_STATE_TYPES StateType, UINT Entry)
{
	InitD310();
	LogInfo("D3D10StateBlockMaskGetSetting called.\n");
	
	return (*_D3D10StateBlockMaskGetSetting)(pMask, 
		StateType, Entry);
}

HRESULT WINAPI D3D10StateBlockMaskIntersect(D3D10Base::D3D10_STATE_BLOCK_MASK *pA, 
	D3D10Base::D3D10_STATE_BLOCK_MASK *pB, D3D10Base::D3D10_STATE_BLOCK_MASK *pResult)
{
	InitD310();
	LogInfo("D3D10StateBlockMaskIntersect called.\n");
	
	return (*_D3D10StateBlockMaskIntersect)(pA, 
		pB, pResult);
}

HRESULT WINAPI D3D10StateBlockMaskUnion(D3D10Base::D3D10_STATE_BLOCK_MASK *pA, 
	D3D10Base::D3D10_STATE_BLOCK_MASK *pB, D3D10Base::D3D10_STATE_BLOCK_MASK *pResult)
{
	InitD310();
	LogInfo("D3D10StateBlockMaskUnion called.\n");
	
	return (*_D3D10StateBlockMaskUnion)(pA, 
		pB, pResult);
}

// Todo: Not real sure that D3D10Wrapper is the right name space for the D3D10Wrapper

STDMETHODIMP D3D10Wrapper::IDirect3DUnknown::QueryInterface(THIS_ REFIID riid, void** ppvObj)
{
	LogDebug("D3D10Wrapper::IDirect3DUnknown::QueryInterface called at 'this': %s\n", type_name(this));

	IID marker = { 0x017b2e72ul, 0xbcde, 0x9f15, { 0xa1, 0x2b, 0x3c, 0x4d, 0x5e, 0x6f, 0x70, 0x01 } };
	if (riid.Data1 == marker.Data1 && riid.Data2 == marker.Data2 && riid.Data3 == marker.Data3 && 
		riid.Data4[0] == marker.Data4[0] && riid.Data4[1] == marker.Data4[1] && riid.Data4[2] == marker.Data4[2] && riid.Data4[3] == marker.Data4[3] && 
		riid.Data4[4] == marker.Data4[4] && riid.Data4[5] == marker.Data4[5] && riid.Data4[6] == marker.Data4[6] && riid.Data4[7] == marker.Data4[7])
	{
		LogInfo("Callback from dxgi.dll wrapper: requesting real ID3D10Device handle from %x\n", *ppvObj);
		
	    D3D10Wrapper::ID3D10Device *p = (D3D10Wrapper::ID3D10Device*) D3D10Wrapper::ID3D10Device::m_List.GetDataPtr(*ppvObj);
		if (p)
		{
			LogInfo("  given pointer was already the real device.\n");
		}
		else
		{
			*ppvObj = ((D3D10Wrapper::ID3D10Device *)*ppvObj)->m_pUnk;
		}
		LogInfo("  returning handle = %x\n", *ppvObj);
		
		return 0x13bc7e31;
	}

	LogDebug("QueryInterface request for %08lx-%04hx-%04hx-%02hhx%02hhx-%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx on %x\n", 
		riid.Data1, riid.Data2, riid.Data3, riid.Data4[0], riid.Data4[1], riid.Data4[2], riid.Data4[3], riid.Data4[4], riid.Data4[5], riid.Data4[6], riid.Data4[7], this);
	bool d3d9device = riid.Data1 == 0xd0223b96 && riid.Data2 == 0xbf7a && riid.Data3 == 0x43fd && riid.Data4[0] == 0x92 && 
		riid.Data4[1] == 0xbd && riid.Data4[2] == 0xa4 && riid.Data4[3] == 0x3b && riid.Data4[4] == 0x0d && 
		riid.Data4[5] == 0x82 && riid.Data4[6] == 0xb9 && riid.Data4[7] == 0xeb;
	bool d3d10device = riid.Data1 == 0x9b7e4c0f && riid.Data2 == 0x342c && riid.Data3 == 0x4106 && riid.Data4[0] == 0xa1 && 
		riid.Data4[1] == 0x9f && riid.Data4[2] == 0x4f && riid.Data4[3] == 0x27 && riid.Data4[4] == 0x04 && 
		riid.Data4[5] == 0xf6 && riid.Data4[6] == 0x89 && riid.Data4[7] == 0xf0;
	bool d3d10multithread = riid.Data1 == 0x9b7e4e00 && riid.Data2 == 0x342c && riid.Data3 == 0x4106 && riid.Data4[0] == 0xa1 && 
		riid.Data4[1] == 0x9f && riid.Data4[2] == 0x4f && riid.Data4[3] == 0x27 && riid.Data4[4] == 0x04 && 
		riid.Data4[5] == 0xf6 && riid.Data4[6] == 0x89 && riid.Data4[7] == 0xf0;
	bool dxgidevice = riid.Data1 == 0x54ec77fa && riid.Data2 == 0x1377 && riid.Data3 == 0x44e6 && riid.Data4[0] == 0x8c && 
			 riid.Data4[1] == 0x32 && riid.Data4[2] == 0x88 && riid.Data4[3] == 0xfd && riid.Data4[4] == 0x5f && 
			 riid.Data4[5] == 0x44 && riid.Data4[6] == 0xc8 && riid.Data4[7] == 0x4c;
    bool dxgidevice1 = riid.Data1 == 0x77db970f && riid.Data2 == 0x6276 && riid.Data3 == 0x48ba && riid.Data4[0] == 0xba && 
			 riid.Data4[1] == 0x28 && riid.Data4[2] == 0x07 && riid.Data4[3] == 0x01 && riid.Data4[4] == 0x43 && 
			 riid.Data4[5] == 0xb4 && riid.Data4[6] == 0x39 && riid.Data4[7] == 0x2c;
    bool dxgidevice2 = riid.Data1 == 0x05008617 && riid.Data2 == 0xfbfd && riid.Data3 == 0x4051 && riid.Data4[0] == 0xa7 && 
			 riid.Data4[1] == 0x90 && riid.Data4[2] == 0x14 && riid.Data4[3] == 0x48 && riid.Data4[4] == 0x84 && 
			 riid.Data4[5] == 0xb4 && riid.Data4[6] == 0xf6 && riid.Data4[7] == 0xa9;
    bool unknown1 = riid.Data1 == 0x7abb6563 && riid.Data2 == 0x02bc && riid.Data3 == 0x47c4 && riid.Data4[0] == 0x8e && 
			 riid.Data4[1] == 0xf9 && riid.Data4[2] == 0xac && riid.Data4[3] == 0xc4 && riid.Data4[4] == 0x79 && 
			 riid.Data4[5] == 0x5e && riid.Data4[6] == 0xdb && riid.Data4[7] == 0xcf;
	if (d3d9device) LogInfo("  d0223b96-bf7a-43fd-92bd-a43b0d82b9eb = IDirect3DDevice9\n");
	if (d3d10device) LogInfo("  9b7e4c0f-342c-4106-a19f-4f2704f689f0 = ID3D10Device\n");
	if (d3d10multithread) LogInfo("  9b7e4e00-342c-4106-a19f-4f2704f689f0 = ID3D10Multithread\n");
	if (dxgidevice) LogInfo("  54ec77fa-1377-44e6-8c32-88fd5f44c84c = IDXGIDevice\n");
	if (dxgidevice1) LogInfo("  77db970f-6276-48ba-ba28-070143b4392c = IDXGIDevice1\n");
	if (dxgidevice2) LogInfo("  05008617-fbfd-4051-a790-144884b4f6a9 = IDXGIDevice2\n");
	/*
	if (unknown1) LogInfo("  7abb6563-02bc-47c4-8ef9-acc4795edbcf = undocumented. Forcing fail.\n");
	if (unknown1)
	{
		*ppvObj = 0;
		return E_OUTOFMEMORY;
	}
	*/
	HRESULT hr = m_pUnk->QueryInterface(riid, ppvObj);
	D3D10Wrapper::ID3D10Device *p4 = (D3D10Wrapper::ID3D10Device*) D3D10Wrapper::ID3D10Device::m_List.GetDataPtr(*ppvObj);
    if (p4)
	{
		unsigned long cnt = ((IDirect3DUnknown*)*ppvObj)->Release();
		*ppvObj = p4;
		p4->AddRef();
		LogInfo("  interface replaced with ID3D10Device wrapper. Interface counter=%d, wrapper counter=%d\n", cnt, p4->m_ulRef);
	}
	D3D10Wrapper::ID3D10Multithread *p5 = (D3D10Wrapper::ID3D10Multithread*) D3D10Wrapper::ID3D10Multithread::m_List.GetDataPtr(*ppvObj);
    if (p5)
	{
		unsigned long cnt = ((IDirect3DUnknown*)*ppvObj)->Release();
		*ppvObj = p5;
		p5->AddRef();
		LogInfo("  interface replaced with ID3D10Multithread wrapper. Interface counter=%d, wrapper counter=%d\n", cnt, p5->m_ulRef);
	}
	if (!p4 && !p5)
	{
		// Check for DirectX10 cast.
		if (d3d10device)
		{
			D3D10Base::ID3D10Device *origDevice = (D3D10Base::ID3D10Device *)*ppvObj;
			D3D10Wrapper::ID3D10Device *wrapper = D3D10Wrapper::ID3D10Device::GetDirect3DDevice(origDevice);
			if(wrapper == NULL)
			{
				LogInfo("  error allocating ID3D10Device wrapper.\n");
				
				origDevice->Release();
				return E_OUTOFMEMORY;
			}
			*ppvObj = wrapper;
			LogInfo("  interface replaced with ID3D10Device wrapper. Wrapper counter=%d\n", wrapper->m_ulRef);
		}
		if (d3d10multithread)
		{
			D3D10Base::ID3D10Multithread *origDevice = (D3D10Base::ID3D10Multithread *)*ppvObj;
			D3D10Wrapper::ID3D10Multithread *wrapper = D3D10Wrapper::ID3D10Multithread::GetDirect3DMultithread(origDevice);
			if(wrapper == NULL)
			{
				LogInfo("  error allocating ID3D10Multithread wrapper.\n");
				
				origDevice->Release();
				return E_OUTOFMEMORY;
			}
			*ppvObj = wrapper;
			LogInfo("  interface replaced with ID3D10Multithread wrapper. Wrapper counter=%d\n", wrapper->m_ulRef);
		}
		// :todo: create d3d9 wrapper!
		if (d3d9device)
		{
			// create d3d9 wrapper!
		}
	}
	LogDebug("  result = %x, handle = %x\n", hr, *ppvObj);
	
	return hr;
}

// 64 bit magic FNV-0 and FNV-1 prime
#define FNV_64_PRIME ((UINT64)0x100000001b3ULL)
static UINT64 fnv_64_buf(const void *buf, size_t len)
{
	UINT64 hval = 0;
    unsigned const char *bp = (unsigned const char *)buf;	/* start of buffer */
    unsigned const char *be = bp + len;		/* beyond end of buffer */

    // FNV-1 hash each octet of the buffer
    while (bp < be) 
	{
		// multiply by the 64 bit FNV magic prime mod 2^64 */
		hval *= FNV_64_PRIME;
		// xor the bottom with the current octet
		hval ^= (UINT64)*bp++;
    }
	return hval;
}


#include "Direct3D10Device.h"
