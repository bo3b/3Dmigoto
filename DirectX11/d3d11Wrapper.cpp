#include "Main.h"
#include <Shlobj.h>
#include <Winuser.h>
#include "../DirectInput.h"
#include <map>
#include <vector>
#include <set>
#include <iterator>
#include <string>
#include <DirectXMath.h>
#include "../HLSLDecompiler/DecompileHLSL.h"

FILE *LogFile = 0;		// off by default.
bool LogInput = false, LogDebug = false;

static wchar_t DLL_PATH[MAX_PATH] = { 0 };
static bool gInitialized = false;

const int MARKING_MODE_SKIP = 0;
const int MARKING_MODE_MONO = 1;
const int MARKING_MODE_ORIGINAL = 2;
const int MARKING_MODE_ZERO = 3;

ThreadSafePointerSet D3D11Wrapper::ID3D11Device::m_List;
ThreadSafePointerSet D3D11Wrapper::ID3D11DeviceContext::m_List;
ThreadSafePointerSet D3D11Wrapper::IDXGIDevice2::m_List;
ThreadSafePointerSet D3D11Wrapper::ID3D10Device::m_List;
ThreadSafePointerSet D3D11Wrapper::ID3D10Multithread::m_List;
static wchar_t SHADER_PATH[MAX_PATH] = { 0 };
static wchar_t SHADER_CACHE_PATH[MAX_PATH] = { 0 };

// Key is index/vertex buffer, value is hash key.
typedef std::map<D3D11Base::ID3D11Buffer *, UINT64> DataBufferMap;

// Source compiled shaders.
typedef std::map<UINT64, std::string> CompiledShaderMap;

// Strategy: This OriginalShaderInfo record and associated map is to allow us to keep track of every
//	pixelshader and vertexshader that are compiled from hlsl text from the ShaderFixes
//	folder.  This keeps track of the original shader information using the ID3D11VertexShader*
//	or ID3D11PixelShader* as a master key to the key map.
//	We are using the base class of ID3D11DeviceChild* since both descend from that, and that allows
//	us to use the same structure for Pixel and Vertex shaders both.

// Info saved about originally overridden shaders passed in by the game in CreateVertexShader or 
// CreatePixelShader that have been loaded as HLSL
//	shaderType is "vs" or "ps" or maybe later "gs" (type wstring for file name use)
//	shaderModel is only filled in when a shader is replaced.  (type string for old D3 API use)
//	linkage is passed as a parameter, seems to be rarely if ever used.
//	byteCode is the original shader byte code passed in by game, or recompiled by override.
//	timeStamp allows reloading/recompiling only modified shaders
//	replacement is either ID3D11VertexShader or ID3D11PixelShader
struct OriginalShaderInfo
{
	UINT64 hash;
	std::wstring shaderType;
	std::string shaderModel;
	D3D11Base::ID3D11ClassLinkage* linkage;
	D3D11Base::ID3DBlob* byteCode;
	FILETIME timeStamp;
	D3D11Base::ID3D11DeviceChild* replacement;
};

// Key is the overridden shader that was given back to the game at CreateVertexShader (vs or ps)
typedef std::map<D3D11Base::ID3D11DeviceChild *, OriginalShaderInfo> ShaderReloadMap;

// Key is vertexshader, value is hash key.
typedef std::map<D3D11Base::ID3D11VertexShader *, UINT64> VertexShaderMap;
typedef std::map<UINT64, D3D11Base::ID3D11VertexShader *> PreloadVertexShaderMap;
typedef std::map<D3D11Base::ID3D11VertexShader *, D3D11Base::ID3D11VertexShader *> VertexShaderReplacementMap;

// Key is pixelshader, value is hash key.
typedef std::map<D3D11Base::ID3D11PixelShader *, UINT64> PixelShaderMap;
typedef std::map<UINT64, D3D11Base::ID3D11PixelShader *> PreloadPixelShaderMap;
typedef std::map<D3D11Base::ID3D11PixelShader *, D3D11Base::ID3D11PixelShader *> PixelShaderReplacementMap;

typedef std::map<D3D11Base::ID3D11HullShader *, UINT64> HullShaderMap;
typedef std::map<D3D11Base::ID3D11DomainShader *, UINT64> DomainShaderMap;
typedef std::map<D3D11Base::ID3D11ComputeShader *, UINT64> ComputeShaderMap;
typedef std::map<D3D11Base::ID3D11GeometryShader *, UINT64> GeometryShaderMap;

// Separation override for shader.
typedef std::map<UINT64, float> ShaderSeparationMap;
typedef std::map<UINT64, std::vector<int> > ShaderIterationMap;
typedef std::map<UINT64, std::vector<UINT64>> ShaderIndexBufferFilter;

// Texture override.
typedef std::map<UINT64, int> TextureStereoMap;
typedef std::map<UINT64, int> TextureTypeMap;
typedef std::map<UINT64, std::vector<int> > TextureIterationMap;

struct ShaderInfoData
{
	std::map<int, void *> ResourceRegisters;
	std::set<UINT64> PartnerShader;
	std::vector<void *> RenderTargets;
};
struct SwapChainInfo
{
	int width, height;
};

struct Globals
{
	int SCREEN_WIDTH;
	int SCREEN_HEIGHT;
	int SCREEN_REFRESH;
	int SCREEN_FULLSCREEN;
	int marking_mode;
	bool gForceStereo;
	bool gCreateStereoProfile;
	int gSurfaceCreateMode;
	int gSurfaceSquareCreateMode;

	bool hunting;
	time_t huntTime;
	bool take_screenshot;
	bool reload_fixes;
	bool next_pixelshader, prev_pixelshader, mark_pixelshader;
	bool next_vertexshader, prev_vertexshader, mark_vertexshader;
	bool next_indexbuffer, prev_indexbuffer, mark_indexbuffer;
	bool next_rendertarget, prev_rendertarget, mark_rendertarget;

	int EXPORT_HLSL;		// 0=off, 1=HLSL only, 2=HLSL+OriginalASM, 3= HLSL+OriginalASM+recompiledASM
	bool EXPORT_SHADERS, EXPORT_FIXED, CACHE_SHADERS, PRELOAD_SHADERS, SCISSOR_DISABLE;
	char ZRepair_DepthTextureReg1, ZRepair_DepthTextureReg2;
	std::string ZRepair_DepthTexture1, ZRepair_DepthTexture2;
	std::vector<std::string> ZRepair_Dependencies1, ZRepair_Dependencies2;
	std::string ZRepair_ZPosCalc1, ZRepair_ZPosCalc2;
	std::string ZRepair_PositionTexture, ZRepair_WorldPosCalc;
	std::vector<std::string> InvTransforms;
	std::string BackProject_Vector1, BackProject_Vector2;
	std::string ObjectPos_ID1, ObjectPos_ID2, ObjectPos_MUL1, ObjectPos_MUL2;
	std::string MatrixPos_ID1, MatrixPos_MUL1;
	UINT64 ZBufferHashToInject;
	bool FIX_SV_Position;
	bool FIX_Light_Position;
	bool FIX_Recompile_VS;
	bool DumpUsage;
	bool ENABLE_TUNE;
	float gTuneValue1, gTuneValue2, gTuneValue3, gTuneValue4, gTuneStep;

	DirectX::XMFLOAT4 iniParams;

	SwapChainInfo mSwapChainInfo;

	ThreadSafePointerSet m_AdapterList;
	CRITICAL_SECTION mCriticalSection;
	bool ENABLE_CRITICAL_SECTION;

	DataBufferMap mDataBuffers;
	UINT64 mCurrentIndexBuffer;
	std::set<UINT64> mVisitedIndexBuffers;
	UINT64 mSelectedIndexBuffer;
	unsigned int mSelectedIndexBufferPos;
	std::set<UINT64> mSelectedIndexBuffer_VertexShader;
	std::set<UINT64> mSelectedIndexBuffer_PixelShader;

	CompiledShaderMap mCompiledShaderMap;

	VertexShaderMap mVertexShaders;							// All shaders ever registered with CreateVertexShader
	PreloadVertexShaderMap mPreloadedVertexShaders;			// All shaders that were preloaded as .bin
	VertexShaderReplacementMap mOriginalVertexShaders;		// When MarkingMode=Original, switch to original
	VertexShaderReplacementMap mZeroVertexShaders;			// When MarkingMode=zero.
	UINT64 mCurrentVertexShader;							// Shader currently live in GPU pipeline.
	std::set<UINT64> mVisitedVertexShaders;					// Only shaders seen in latest frame
	UINT64 mSelectedVertexShader;							// Shader selected using XInput
	unsigned int mSelectedVertexShaderPos;
	std::set<UINT64> mSelectedVertexShader_IndexBuffer;

	PixelShaderMap mPixelShaders;							// All shaders ever registered with CreatePixelShader
	PreloadPixelShaderMap mPreloadedPixelShaders;
	PixelShaderReplacementMap mOriginalPixelShaders;
	PixelShaderReplacementMap mZeroPixelShaders;
	UINT64 mCurrentPixelShader;
	std::set<UINT64> mVisitedPixelShaders;
	UINT64 mSelectedPixelShader;
	unsigned int mSelectedPixelShaderPos;
	std::set<UINT64> mSelectedPixelShader_IndexBuffer;

	ShaderReloadMap mReloadedShaders;						// Shaders that were reloaded live from ShaderFixes

	GeometryShaderMap mGeometryShaders;
	ComputeShaderMap mComputeShaders;
	DomainShaderMap mDomainShaders;
	HullShaderMap mHullShaders;

	// Separation override for shader.
	ShaderSeparationMap mShaderSeparationMap;
	ShaderIterationMap mShaderIterationMap;					// Only for separation changes, not shaders.
	ShaderIndexBufferFilter mShaderIndexBufferFilter;

	// Texture override.
	TextureStereoMap mTextureStereoMap;
	TextureTypeMap mTextureTypeMap;
	ShaderIterationMap mTextureIterationMap;

	// Statistics
	std::map<void *, UINT64> mRenderTargets;
	std::set<void *> mVisitedRenderTargets;
	std::vector<void *> mCurrentRenderTargets;
	void *mSelectedRenderTarget;
	unsigned int mSelectedRenderTargetPos;
	// Snapshot of all targets for selection.
	void *mSelectedRenderTargetSnapshot;
	std::set<void *> mSelectedRenderTargetSnapshotList;
	// Relations
	std::map<D3D11Base::ID3D11Texture2D *, UINT64> mTexture2D_ID;
	std::map<D3D11Base::ID3D11Texture3D *, UINT64> mTexture3D_ID;
	std::map<UINT64, ShaderInfoData> mVertexShaderInfo;
	std::map<UINT64, ShaderInfoData> mPixelShaderInfo;

	bool mBlockingMode;

	Globals() :
		mBlockingMode(false),
		mSelectedRenderTargetSnapshot(0),
		mSelectedRenderTargetPos(0),
		mSelectedRenderTarget((void *)1),
		mCurrentPixelShader(0),
		mSelectedPixelShader(1),
		mSelectedPixelShaderPos(0),
		mCurrentVertexShader(0),
		mSelectedVertexShader(1),
		mSelectedVertexShaderPos(0),
		mCurrentIndexBuffer(0),
		mSelectedIndexBuffer(1),
		mSelectedIndexBufferPos(0),

		hunting(false),
		huntTime(0),
		take_screenshot(false),
		reload_fixes(false),
		next_pixelshader(false),
		prev_pixelshader(false),
		mark_pixelshader(false),
		next_vertexshader(false),
		prev_vertexshader(false),
		mark_vertexshader(false),
		next_indexbuffer(false),
		prev_indexbuffer(false),
		mark_indexbuffer(false),
		next_rendertarget(false),
		prev_rendertarget(false),
		mark_rendertarget(false),

		EXPORT_SHADERS(false),
		EXPORT_HLSL(0),
		EXPORT_FIXED(false),
		CACHE_SHADERS(false),
		PRELOAD_SHADERS(false),
		FIX_SV_Position(false),
		FIX_Light_Position(false),
		FIX_Recompile_VS(false),
		DumpUsage(false),
		ENABLE_TUNE(false),
		gTuneValue1(1.0f), gTuneValue2(1.0f), gTuneValue3(1.0f), gTuneValue4(1.0f),
		gTuneStep(0.001f),

		iniParams{ -1.0f, -1.0f, -1.0f, -1.0f },

		ENABLE_CRITICAL_SECTION(false),
		SCREEN_WIDTH(-1),
		SCREEN_HEIGHT(-1),
		SCREEN_REFRESH(-1),
		SCREEN_FULLSCREEN(-1),
		marking_mode(-1),
		gForceStereo(false),
		gCreateStereoProfile(false),
		gSurfaceCreateMode(-1),
		gSurfaceSquareCreateMode(-1),
		ZBufferHashToInject(0),
		SCISSOR_DISABLE(0)
	{
		mSwapChainInfo.width = -1;
		mSwapChainInfo.height = -1;
	}
};
static Globals *G;


static string LogTime()
{
	string timeStr;
	char cTime[32];
	tm timestruct;

	time_t ltime = time(0);
	localtime_s(&timestruct, &ltime);
	asctime_s(cTime, sizeof(cTime), &timestruct);

	timeStr = cTime;
	return timeStr;
}

static char *readStringParameter(wchar_t *val)
{
	static char buf[MAX_PATH];
	wcstombs(buf, val, MAX_PATH);
	char *end = buf + strlen(buf) - 1; while (end > buf && isspace(*end)) end--; *(end + 1) = 0;
	char *start = buf; while (isspace(*start)) start++;
	return start;
}

// During the initialize, we will also Log every setting that is enabled, so that the log
// has a complete list of active settings.  This should make it more accurate and clear.

void InitializeDLL()
{
	if (!gInitialized)
	{
		wchar_t iniFile[MAX_PATH];
		wchar_t setting[MAX_PATH];

		gInitialized = true;

		GetModuleFileName(0, iniFile, MAX_PATH);
		wcsrchr(iniFile, L'\\')[1] = 0;
		wcscat(iniFile, L"d3dx.ini");

		// Log all settings that are _enabled_, in order, 
		// so that there is no question what settings we are using.

		// [Logging]
		if (GetPrivateProfileInt(L"Logging", L"calls", 1, iniFile))
		{
			LogFile = _fsopen("d3d11_log.txt", "w", _SH_DENYNO);
			if (LogFile) fprintf(LogFile, "\nD3D11 DLL starting init  -  %s\n\n", LogTime().c_str());
			fprintf(LogFile, "----------- d3dx.ini settings -----------\n");
		}

		LogInput = GetPrivateProfileInt(L"Logging", L"input", 0, iniFile) == 1;
		LogDebug = GetPrivateProfileInt(L"Logging", L"debug", 0, iniFile) == 1;

		// Unbuffered logging to remove need for fflush calls, and r/w access to make it easy
		// to open active files.
		int unbuffered = -1;
		if (GetPrivateProfileInt(L"Logging", L"unbuffered", 0, iniFile))
		{
			unbuffered = setvbuf(LogFile, NULL, _IONBF, 0);
		}

		// Set the CPU affinity based upon d3dx.ini setting.  Useful for debugging and shader hunting in AC3.
		BOOL affinity = -1;
		if (GetPrivateProfileInt(L"Logging", L"force_cpu_affinity", 0, iniFile))
		{
			DWORD one = 0x01;
			affinity = SetProcessAffinityMask(GetCurrentProcess(), one);
		}

		// If specified in Logging section, wait for Attach to Debugger.
		bool waitfordebugger = false;
		int debugger = GetPrivateProfileInt(L"Logging", L"waitfordebugger", 0, iniFile);
		if (debugger > 0)
		{
			waitfordebugger = true;
			do
			{
				Sleep(250);
			} while (!IsDebuggerPresent());
			if (debugger > 1)
				__debugbreak();
		}

		if (LogFile)
		{
			fprintf(LogFile, "[Logging]\n");
			fprintf(LogFile, "  calls=1\n");
			if (LogInput) fprintf(LogFile, "  input=1\n");
			if (LogDebug) fprintf(LogFile, "  debug=1\n");
			if (unbuffered != -1) fprintf(LogFile, "  unbuffered=1  return: %d\n", unbuffered);
			if (affinity != -1) fprintf(LogFile, "  force_cpu_affinity=1  return: %s\n", affinity ? "true" : "false");
			if (waitfordebugger) fprintf(LogFile, "  waitfordebugger=1\n");
		}

		// [System]
		GetPrivateProfileString(L"System", L"proxy_d3d11", 0, DLL_PATH, MAX_PATH, iniFile);
		if (LogFile)
		{
			fprintf(LogFile, "[System]\n");
			if (!DLL_PATH) fprintf(LogFile, "  proxy_d3d11=%s\n", DLL_PATH);
		}

		// [Device]
		wchar_t refresh[MAX_PATH] = { 0 };

		if (GetPrivateProfileString(L"Device", L"width", 0, setting, MAX_PATH, iniFile))
			swscanf_s(setting, L"%d", &G->SCREEN_WIDTH);
		if (GetPrivateProfileString(L"Device", L"height", 0, setting, MAX_PATH, iniFile))
			swscanf_s(setting, L"%d", &G->SCREEN_HEIGHT);
		if (GetPrivateProfileString(L"Device", L"refresh_rate", 0, setting, MAX_PATH, iniFile))
			swscanf_s(setting, L"%d", &G->SCREEN_REFRESH);
		GetPrivateProfileString(L"Device", L"filter_refresh_rate", 0, refresh, MAX_PATH, iniFile);	 // in DXGI dll
		G->SCREEN_FULLSCREEN = GetPrivateProfileInt(L"Device", L"full_screen", 0, iniFile);
		G->gForceStereo = GetPrivateProfileInt(L"Device", L"force_stereo", 0, iniFile) == 1;
		bool allowWindowCommands = GetPrivateProfileInt(L"Device", L"allow_windowcommands", 0, iniFile) == 1; // in DXGI dll

		if (LogFile)
		{
			fprintf(LogFile, "[Device]\n");
			if (G->SCREEN_WIDTH != -1) fprintf(LogFile, "  width=%d\n", G->SCREEN_WIDTH);
			if (G->SCREEN_HEIGHT != -1) fprintf(LogFile, "  height=%d\n", G->SCREEN_HEIGHT);
			if (G->SCREEN_REFRESH != -1) fprintf(LogFile, "  refresh_rate=%d\n", G->SCREEN_REFRESH);
			if (refresh[0]) fwprintf(LogFile, L"  filter_refresh_rate=%s\n", refresh);
			if (G->SCREEN_FULLSCREEN) fprintf(LogFile, "  full_screen=1\n");
			if (G->gForceStereo) fprintf(LogFile, "  force_stereo=1\n");
			if (allowWindowCommands) fprintf(LogFile, "  allow_windowcommands=1\n");
		}

		// [Stereo]
		bool automaticMode = GetPrivateProfileInt(L"Stereo", L"automatic_mode", 0, iniFile) == 1;				// in NVapi dll
		G->gCreateStereoProfile = GetPrivateProfileInt(L"Stereo", L"create_profile", 0, iniFile) == 1;
		G->gSurfaceCreateMode = GetPrivateProfileInt(L"Stereo", L"surface_createmode", -1, iniFile);
		G->gSurfaceSquareCreateMode = GetPrivateProfileInt(L"Stereo", L"surface_square_createmode", -1, iniFile);

		if (LogFile)
		{
			fprintf(LogFile, "[Stereo]\n");
			if (automaticMode) fprintf(LogFile, "  automatic_mode=1\n");
			if (G->gCreateStereoProfile) fprintf(LogFile, "  create_profile=1\n");
			if (G->gSurfaceCreateMode != -1) fprintf(LogFile, "  surface_createmode=%d\n", G->gSurfaceCreateMode);
			if (G->gSurfaceSquareCreateMode != -1) fprintf(LogFile, "  surface_square_createmode=%d\n", G->gSurfaceSquareCreateMode);
		}

		// [Rendering]
		GetPrivateProfileString(L"Rendering", L"override_directory", 0, SHADER_PATH, MAX_PATH, iniFile);
		if (SHADER_PATH[0])
		{
			while (SHADER_PATH[wcslen(SHADER_PATH) - 1] == L' ')
				SHADER_PATH[wcslen(SHADER_PATH) - 1] = 0;
			if (SHADER_PATH[1] != ':' && SHADER_PATH[0] != '\\')
			{
				GetModuleFileName(0, setting, MAX_PATH);
				wcsrchr(setting, L'\\')[1] = 0;
				wcscat(setting, SHADER_PATH);
				wcscpy(SHADER_PATH, setting);
			}
			// Create directory?
			CreateDirectory(SHADER_PATH, 0);
		}
		GetPrivateProfileString(L"Rendering", L"cache_directory", 0, SHADER_CACHE_PATH, MAX_PATH, iniFile);
		if (SHADER_CACHE_PATH[0])
		{
			while (SHADER_CACHE_PATH[wcslen(SHADER_CACHE_PATH) - 1] == L' ')
				SHADER_CACHE_PATH[wcslen(SHADER_CACHE_PATH) - 1] = 0;
			if (SHADER_CACHE_PATH[1] != ':' && SHADER_CACHE_PATH[0] != '\\')
			{
				GetModuleFileName(0, setting, MAX_PATH);
				wcsrchr(setting, L'\\')[1] = 0;
				wcscat(setting, SHADER_CACHE_PATH);
				wcscpy(SHADER_CACHE_PATH, setting);
			}
			// Create directory?
			CreateDirectory(SHADER_CACHE_PATH, 0);
		}

		G->CACHE_SHADERS = GetPrivateProfileInt(L"Rendering", L"cache_shaders", 0, iniFile) == 1;
		G->PRELOAD_SHADERS = GetPrivateProfileInt(L"Rendering", L"preload_shaders", 0, iniFile) == 1;
		G->ENABLE_CRITICAL_SECTION = GetPrivateProfileInt(L"Rendering", L"use_criticalsection", 0, iniFile) == 1;
		G->SCISSOR_DISABLE = GetPrivateProfileInt(L"Rendering", L"rasterizer_disable_scissor", 0, iniFile) == 1;

		G->EXPORT_FIXED = GetPrivateProfileInt(L"Rendering", L"export_fixed", 0, iniFile) == 1;
		G->EXPORT_SHADERS = GetPrivateProfileInt(L"Rendering", L"export_shaders", 0, iniFile) == 1;
		G->EXPORT_HLSL = GetPrivateProfileInt(L"Rendering", L"export_hlsl", 0, iniFile);
		G->DumpUsage = GetPrivateProfileInt(L"Rendering", L"dump_usage", 0, iniFile) == 1;

		if (LogFile)
		{
			fprintf(LogFile, "[Rendering]\n");
			if (SHADER_PATH[0])
				fwprintf(LogFile, L"  override_directory=%s\n", SHADER_PATH);
			if (SHADER_CACHE_PATH[0])
				fwprintf(LogFile, L"  cache_directory=%s\n", SHADER_CACHE_PATH);

			if (G->CACHE_SHADERS) fprintf(LogFile, "  cache_shaders=1\n");
			if (G->PRELOAD_SHADERS) fprintf(LogFile, "  preload_shaders=1\n");
			if (G->ENABLE_CRITICAL_SECTION) fprintf(LogFile, "  use_criticalsection=1\n");
			if (G->SCISSOR_DISABLE) fprintf(LogFile, "  rasterizer_disable_scissor=1\n");

			if (G->EXPORT_FIXED) fprintf(LogFile, "  export_fixed=1\n");
			if (G->EXPORT_SHADERS) fprintf(LogFile, "  export_shaders=1\n");
			if (G->EXPORT_HLSL != 0) fprintf(LogFile, "  export_hlsl=%d\n", G->EXPORT_HLSL);
			if (G->DumpUsage) fprintf(LogFile, "  dump_usage=1\n");
		}


		// Automatic section 
		G->FIX_SV_Position = GetPrivateProfileInt(L"Rendering", L"fix_sv_position", 0, iniFile) == 1;
		G->FIX_Light_Position = GetPrivateProfileInt(L"Rendering", L"fix_light_position", 0, iniFile) == 1;
		G->FIX_Recompile_VS = GetPrivateProfileInt(L"Rendering", L"recompile_all_vs", 0, iniFile) == 1;
		if (GetPrivateProfileString(L"Rendering", L"fix_ZRepair_DepthTexture1", 0, setting, MAX_PATH, iniFile))
		{
			char buf[MAX_PATH];
			wcstombs(buf, setting, MAX_PATH);
			char *end = buf + strlen(buf) - 1; while (end > buf && isspace(*end)) end--; *(end + 1) = 0;
			G->ZRepair_DepthTextureReg1 = *end; *(end - 1) = 0;
			char *start = buf; while (isspace(*start)) start++;
			G->ZRepair_DepthTexture1 = start;
		}
		if (GetPrivateProfileString(L"Rendering", L"fix_ZRepair_DepthTexture2", 0, setting, MAX_PATH, iniFile))
		{
			char buf[MAX_PATH];
			wcstombs(buf, setting, MAX_PATH);
			char *end = buf + strlen(buf) - 1; while (end > buf && isspace(*end)) end--; *(end + 1) = 0;
			G->ZRepair_DepthTextureReg2 = *end; *(end - 1) = 0;
			char *start = buf; while (isspace(*start)) start++;
			G->ZRepair_DepthTexture2 = start;
		}
		if (GetPrivateProfileString(L"Rendering", L"fix_ZRepair_ZPosCalc1", 0, setting, MAX_PATH, iniFile))
			G->ZRepair_ZPosCalc1 = readStringParameter(setting);
		if (GetPrivateProfileString(L"Rendering", L"fix_ZRepair_ZPosCalc2", 0, setting, MAX_PATH, iniFile))
			G->ZRepair_ZPosCalc2 = readStringParameter(setting);
		if (GetPrivateProfileString(L"Rendering", L"fix_ZRepair_PositionTexture", 0, setting, MAX_PATH, iniFile))
			G->ZRepair_PositionTexture = readStringParameter(setting);
		if (GetPrivateProfileString(L"Rendering", L"fix_ZRepair_PositionCalc", 0, setting, MAX_PATH, iniFile))
			G->ZRepair_WorldPosCalc = readStringParameter(setting);
		if (GetPrivateProfileString(L"Rendering", L"fix_ZRepair_Dependencies1", 0, setting, MAX_PATH, iniFile))
		{
			char buf[MAX_PATH];
			wcstombs(buf, setting, MAX_PATH);
			char *start = buf; while (isspace(*start)) ++start;
			while (*start)
			{
				char *end = start; while (*end != ',' && *end && *end != ' ') ++end;
				G->ZRepair_Dependencies1.push_back(string(start, end));
				start = end; if (*start == ',') ++start;
			}
		}
		if (GetPrivateProfileString(L"Rendering", L"fix_ZRepair_Dependencies2", 0, setting, MAX_PATH, iniFile))
		{
			char buf[MAX_PATH];
			wcstombs(buf, setting, MAX_PATH);
			char *start = buf; while (isspace(*start)) ++start;
			while (*start)
			{
				char *end = start; while (*end != ',' && *end && *end != ' ') ++end;
				G->ZRepair_Dependencies2.push_back(string(start, end));
				start = end; if (*start == ',') ++start;
			}
		}
		if (GetPrivateProfileString(L"Rendering", L"fix_InvTransform", 0, setting, MAX_PATH, iniFile))
		{
			char buf[MAX_PATH];
			wcstombs(buf, setting, MAX_PATH);
			char *start = buf; while (isspace(*start)) ++start;
			while (*start)
			{
				char *end = start; while (*end != ',' && *end && *end != ' ') ++end;
				G->InvTransforms.push_back(string(start, end));
				start = end; if (*start == ',') ++start;
			}
		}
		if (GetPrivateProfileString(L"Rendering", L"fix_ZRepair_DepthTextureHash", 0, setting, MAX_PATH, iniFile))
		{
			unsigned long hashHi, hashLo;
			swscanf_s(setting, L"%08lx%08lx", &hashHi, &hashLo);
			G->ZBufferHashToInject = (UINT64(hashHi) << 32) | UINT64(hashLo);
		}
		if (GetPrivateProfileString(L"Rendering", L"fix_BackProjectionTransform1", 0, setting, MAX_PATH, iniFile))
			G->BackProject_Vector1 = readStringParameter(setting);
		if (GetPrivateProfileString(L"Rendering", L"fix_BackProjectionTransform2", 0, setting, MAX_PATH, iniFile))
			G->BackProject_Vector2 = readStringParameter(setting);
		if (GetPrivateProfileString(L"Rendering", L"fix_ObjectPosition1", 0, setting, MAX_PATH, iniFile))
			G->ObjectPos_ID1 = readStringParameter(setting);
		if (GetPrivateProfileString(L"Rendering", L"fix_ObjectPosition2", 0, setting, MAX_PATH, iniFile))
			G->ObjectPos_ID2 = readStringParameter(setting);
		if (GetPrivateProfileString(L"Rendering", L"fix_ObjectPosition1Multiplier", 0, setting, MAX_PATH, iniFile))
			G->ObjectPos_MUL1 = readStringParameter(setting);
		if (GetPrivateProfileString(L"Rendering", L"fix_ObjectPosition2Multiplier", 0, setting, MAX_PATH, iniFile))
			G->ObjectPos_MUL2 = readStringParameter(setting);
		if (GetPrivateProfileString(L"Rendering", L"fix_MatrixOperand1", 0, setting, MAX_PATH, iniFile))
			G->MatrixPos_ID1 = readStringParameter(setting);
		if (GetPrivateProfileString(L"Rendering", L"fix_MatrixOperand1Multiplier", 0, setting, MAX_PATH, iniFile))
			G->MatrixPos_MUL1 = readStringParameter(setting);

		// Todo: finish logging all these settings
		if (LogFile) fprintf(LogFile, "  ... missing automatic ini section\n");


		// [Hunting]
		G->hunting = GetPrivateProfileInt(L"Hunting", L"hunting", 0, iniFile) == 1;

		// DirectInput
		InputDevice[0] = 0;
		GetPrivateProfileString(L"Hunting", L"Input", 0, InputDevice, MAX_PATH, iniFile);
		wchar_t *end = InputDevice + wcslen(InputDevice) - 1; while (end > InputDevice && iswspace(*end)) end--; *(end + 1) = 0;

		if (GetPrivateProfileString(L"Hunting", L"marking_mode", 0, setting, MAX_PATH, iniFile)) {
			if (!wcscmp(setting, L"skip")) G->marking_mode = MARKING_MODE_SKIP;
			if (!wcscmp(setting, L"mono")) G->marking_mode = MARKING_MODE_MONO;
			if (!wcscmp(setting, L"original")) G->marking_mode = MARKING_MODE_ORIGINAL;
			if (!wcscmp(setting, L"zero")) G->marking_mode = MARKING_MODE_ZERO;
		}

		InputDeviceId = GetPrivateProfileInt(L"Hunting", L"DeviceNr", -1, iniFile);
		// Todo: This deviceNr is in wrong section- actually found in NVapi dll

		// Actions
		GetPrivateProfileString(L"Hunting", L"next_pixelshader", 0, InputAction[0], MAX_PATH, iniFile);
		end = InputAction[0] + wcslen(InputAction[0]) - 1; while (end > InputAction[0] && iswspace(*end)) end--; *(end + 1) = 0;
		GetPrivateProfileString(L"Hunting", L"previous_pixelshader", 0, InputAction[1], MAX_PATH, iniFile);
		end = InputAction[1] + wcslen(InputAction[1]) - 1; while (end > InputAction[1] && iswspace(*end)) end--; *(end + 1) = 0;
		GetPrivateProfileString(L"Hunting", L"mark_pixelshader", 0, InputAction[2], MAX_PATH, iniFile);
		end = InputAction[2] + wcslen(InputAction[2]) - 1; while (end > InputAction[2] && iswspace(*end)) end--; *(end + 1) = 0;

		GetPrivateProfileString(L"Hunting", L"take_screenshot", 0, InputAction[3], MAX_PATH, iniFile);
		end = InputAction[3] + wcslen(InputAction[3]) - 1; while (end > InputAction[3] && iswspace(*end)) end--; *(end + 1) = 0;

		GetPrivateProfileString(L"Hunting", L"next_indexbuffer", 0, InputAction[4], MAX_PATH, iniFile);
		end = InputAction[4] + wcslen(InputAction[4]) - 1; while (end > InputAction[4] && iswspace(*end)) end--; *(end + 1) = 0;
		GetPrivateProfileString(L"Hunting", L"previous_indexbuffer", 0, InputAction[5], MAX_PATH, iniFile);
		end = InputAction[5] + wcslen(InputAction[5]) - 1; while (end > InputAction[5] && iswspace(*end)) end--; *(end + 1) = 0;
		GetPrivateProfileString(L"Hunting", L"mark_indexbuffer", 0, InputAction[6], MAX_PATH, iniFile);
		end = InputAction[6] + wcslen(InputAction[6]) - 1; while (end > InputAction[6] && iswspace(*end)) end--; *(end + 1) = 0;

		GetPrivateProfileString(L"Hunting", L"next_vertexshader", 0, InputAction[7], MAX_PATH, iniFile);
		end = InputAction[7] + wcslen(InputAction[7]) - 1; while (end > InputAction[7] && iswspace(*end)) end--; *(end + 1) = 0;
		GetPrivateProfileString(L"Hunting", L"previous_vertexshader", 0, InputAction[8], MAX_PATH, iniFile);
		end = InputAction[8] + wcslen(InputAction[8]) - 1; while (end > InputAction[8] && iswspace(*end)) end--; *(end + 1) = 0;
		GetPrivateProfileString(L"Hunting", L"mark_vertexshader", 0, InputAction[9], MAX_PATH, iniFile);
		end = InputAction[9] + wcslen(InputAction[9]) - 1; while (end > InputAction[9] && iswspace(*end)) end--; *(end + 1) = 0;

		GetPrivateProfileString(L"Hunting", L"tune1_up", 0, InputAction[10], MAX_PATH, iniFile);
		end = InputAction[10] + wcslen(InputAction[10]) - 1; while (end > InputAction[10] && iswspace(*end)) end--; *(end + 1) = 0;
		GetPrivateProfileString(L"Hunting", L"tune1_down", 0, InputAction[11], MAX_PATH, iniFile);
		end = InputAction[11] + wcslen(InputAction[11]) - 1; while (end > InputAction[11] && iswspace(*end)) end--; *(end + 1) = 0;

		GetPrivateProfileString(L"Hunting", L"next_rendertarget", 0, InputAction[12], MAX_PATH, iniFile);
		end = InputAction[12] + wcslen(InputAction[12]) - 1; while (end > InputAction[12] && iswspace(*end)) end--; *(end + 1) = 0;
		GetPrivateProfileString(L"Hunting", L"previous_rendertarget", 0, InputAction[13], MAX_PATH, iniFile);
		end = InputAction[13] + wcslen(InputAction[13]) - 1; while (end > InputAction[13] && iswspace(*end)) end--; *(end + 1) = 0;
		GetPrivateProfileString(L"Hunting", L"mark_rendertarget", 0, InputAction[14], MAX_PATH, iniFile);
		end = InputAction[14] + wcslen(InputAction[14]) - 1; while (end > InputAction[14] && iswspace(*end)) end--; *(end + 1) = 0;

		GetPrivateProfileString(L"Hunting", L"tune2_up", 0, InputAction[15], MAX_PATH, iniFile);
		end = InputAction[15] + wcslen(InputAction[15]) - 1; while (end > InputAction[15] && iswspace(*end)) end--; *(end + 1) = 0;
		GetPrivateProfileString(L"Hunting", L"tune2_down", 0, InputAction[16], MAX_PATH, iniFile);
		end = InputAction[16] + wcslen(InputAction[16]) - 1; while (end > InputAction[16] && iswspace(*end)) end--; *(end + 1) = 0;
		GetPrivateProfileString(L"Hunting", L"tune3_up", 0, InputAction[17], MAX_PATH, iniFile);
		end = InputAction[17] + wcslen(InputAction[17]) - 1; while (end > InputAction[17] && iswspace(*end)) end--; *(end + 1) = 0;
		GetPrivateProfileString(L"Hunting", L"tune3_down", 0, InputAction[18], MAX_PATH, iniFile);
		end = InputAction[18] + wcslen(InputAction[18]) - 1; while (end > InputAction[18] && iswspace(*end)) end--; *(end + 1) = 0;
		GetPrivateProfileString(L"Hunting", L"tune4_up", 0, InputAction[19], MAX_PATH, iniFile);
		end = InputAction[19] + wcslen(InputAction[19]) - 1; while (end > InputAction[19] && iswspace(*end)) end--; *(end + 1) = 0;
		GetPrivateProfileString(L"Hunting", L"tune4_down", 0, InputAction[20], MAX_PATH, iniFile);
		end = InputAction[20] + wcslen(InputAction[20]) - 1; while (end > InputAction[20] && iswspace(*end)) end--; *(end + 1) = 0;

		GetPrivateProfileString(L"Hunting", L"reload_fixes", 0, InputAction[21], MAX_PATH, iniFile);
		end = InputAction[21] + wcslen(InputAction[21]) - 1; while (end > InputAction[21] && iswspace(*end)) end--; *(end + 1) = 0;
		// XInput
		XInputDeviceId = GetPrivateProfileInt(L"Hunting", L"XInputDevice", -1, iniFile);

		// Todo: Not sure this is best spot.
		G->ENABLE_TUNE = GetPrivateProfileInt(L"Hunting", L"tune_enable", 0, iniFile) == 1;
		if (GetPrivateProfileString(L"Hunting", L"tune_step", 0, setting, MAX_PATH, iniFile))
			swscanf_s(setting, L"%f", &G->gTuneStep);


		if (LogFile)
		{
			fprintf(LogFile, "[Hunting]\n");
			if (G->hunting)
			{
				fprintf(LogFile, "  hunting=1\n");
				if (InputDevice) fwprintf(LogFile, L"  Input=%s\n", InputDevice);
				if (G->marking_mode != -1) fwprintf(LogFile, L"  marking_mode=%d\n", G->marking_mode);

				if (InputAction[0][0]) fwprintf(LogFile, L"  next_pixelshader=%s\n", InputAction[0]);
				if (InputAction[1][0]) fwprintf(LogFile, L"  previous_pixelshader=%s\n", InputAction[1]);
				if (InputAction[2][0]) fwprintf(LogFile, L"  mark_pixelshader=%s\n", InputAction[2]);

				if (InputAction[7][0]) fwprintf(LogFile, L"  next_vertexshader=%s\n", InputAction[7]);
				if (InputAction[8][0]) fwprintf(LogFile, L"  previous_vertexshader=%s\n", InputAction[8]);
				if (InputAction[9][0]) fwprintf(LogFile, L"  mark_vertexshader=%s\n", InputAction[9]);

				if (InputAction[4][0]) fwprintf(LogFile, L"  next_indexbuffer=%s\n", InputAction[4]);
				if (InputAction[5][0]) fwprintf(LogFile, L"  previous_indexbuffer=%s\n", InputAction[5]);
				if (InputAction[6][0]) fwprintf(LogFile, L"  mark_indexbuffer=%s\n", InputAction[6]);

				if (InputAction[12][0]) fwprintf(LogFile, L"  next_rendertarget=%s\n", InputAction[12]);
				if (InputAction[13][0]) fwprintf(LogFile, L"  previous_rendertarget=%s\n", InputAction[13]);
				if (InputAction[14][0]) fwprintf(LogFile, L"  mark_rendertarget=%s\n", InputAction[14]);

				if (InputAction[3][0]) fwprintf(LogFile, L"  take_screenshot=%s\n", InputAction[3]);
				if (InputAction[21][0]) fwprintf(LogFile, L"  reload_fixes=%s\n", InputAction[21]);
			}
			fprintf(LogFile, "  ... missing tuning ini section\n");
		}

		// Shader separation overrides.
		for (int i = 1;; ++i)
		{
			if (LogFile && LogDebug) fprintf(LogFile, "Find [ShaderOverride] i=%i\n", i);
			wchar_t id[] = L"ShaderOverridexxx";
			_itow_s(i, id + 14, 3, 10);
			if (!GetPrivateProfileString(id, L"Hash", 0, setting, MAX_PATH, iniFile))
				break;
			unsigned long hashHi, hashLo;
			swscanf_s(setting, L"%08lx%08lx", &hashHi, &hashLo);
			if (GetPrivateProfileString(id, L"Separation", 0, setting, MAX_PATH, iniFile))
			{
				float separation;
				swscanf_s(setting, L"%e", &separation);
				G->mShaderSeparationMap[(UINT64(hashHi) << 32) | UINT64(hashLo)] = separation;
				if (LogFile && LogDebug) fprintf(LogFile, " [ShaderOverride] Shader = %08lx%08lx, separation = %f\n", hashHi, hashLo, separation);
			}
			if (GetPrivateProfileString(id, L"Handling", 0, setting, MAX_PATH, iniFile)) {
				if (!wcscmp(setting, L"skip"))
					G->mShaderSeparationMap[(UINT64(hashHi) << 32) | UINT64(hashLo)] = 10000;
			}
			if (GetPrivateProfileString(id, L"Iteration", 0, setting, MAX_PATH, iniFile))
			{
				int iteration;
				std::vector<int> iterations;
				iterations.push_back(0);
				swscanf_s(setting, L"%d", &iteration);
				iterations.push_back(iteration);
				G->mShaderIterationMap[(UINT64(hashHi) << 32) | UINT64(hashLo)] = iterations;
			}
			if (GetPrivateProfileString(id, L"IndexBufferFilter", 0, setting, MAX_PATH, iniFile))
			{
				unsigned long hashHi2, hashLo2;
				swscanf_s(setting, L"%08lx%08lx", &hashHi2, &hashLo2);
				G->mShaderIndexBufferFilter[(UINT64(hashHi) << 32) | UINT64(hashLo)].push_back((UINT64(hashHi2) << 32) | UINT64(hashLo2));
			}
		}

		// Todo: finish logging all input parameters.
		if (LogFile) fprintf(LogFile, "  ... missing shader override ini section\n");

		// Texture overrides.
		for (int i = 1;; ++i)
		{
			wchar_t id[] = L"TextureOverridexxx";
			_itow_s(i, id + 15, 3, 10);
			if (!GetPrivateProfileString(id, L"Hash", 0, setting, MAX_PATH, iniFile))
				break;
			unsigned long hashHi, hashLo;
			swscanf_s(setting, L"%08lx%08lx", &hashHi, &hashLo);
			int stereoMode = GetPrivateProfileInt(id, L"StereoMode", -1, iniFile);
			if (stereoMode >= 0)
			{
				G->mTextureStereoMap[(UINT64(hashHi) << 32) | UINT64(hashLo)] = stereoMode;
				if (LogFile && LogDebug) fprintf(LogFile, "[TextureOverride] Texture = %08lx%08lx, stereo mode = %d\n", hashHi, hashLo, stereoMode);
			}
			int texFormat = GetPrivateProfileInt(id, L"Format", -1, iniFile);
			if (texFormat >= 0)
			{
				G->mTextureTypeMap[(UINT64(hashHi) << 32) | UINT64(hashLo)] = texFormat;
				if (LogFile && LogDebug) fprintf(LogFile, "[TextureOverride] Texture = %08lx%08lx, format = %d\n", hashHi, hashLo, texFormat);
			}
			if (GetPrivateProfileString(id, L"Iteration", 0, setting, MAX_PATH, iniFile))
			{
				std::vector<int> iterations;
				iterations.push_back(0);
				int id[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
				swscanf_s(setting, L"%d,%d,%d,%d,%d,%d,%d,%d,%d,%d", id + 0, id + 1, id + 2, id + 3, id + 4, id + 5, id + 6, id + 7, id + 8, id + 9);
				for (int j = 0; j < 10; ++j)
				{
					if (id[j] <= 0) break;
					iterations.push_back(id[j]);
				}
				G->mTextureIterationMap[(UINT64(hashHi) << 32) | UINT64(hashLo)] = iterations;
				if (LogFile && LogDebug) fprintf(LogFile, "[TextureOverride] Texture = %08lx%08lx, iterations = %d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n", hashHi, hashLo,
					id[0], id[1], id[2], id[3], id[4], id[5], id[6], id[7], id[8], id[9]);
			}
		}

		// Todo: finish logging all input parameters.
		if (LogFile) fprintf(LogFile, "  ... missing texture override ini section\n");

		// Todo: finish logging all input parameters.
		if (LogFile) fprintf(LogFile, "  ... missing mouse OverrideSettings ini section\n");
		if (LogFile) fprintf(LogFile, "  ... missing convergence map ini section\n");
		if (LogFile) fprintf(LogFile, "-----------------------------------------\n");

		// Read in any constants defined in the ini, for use as shader parameters
		if (GetPrivateProfileString(L"Constants", L"x", 0, setting, MAX_PATH, iniFile))
			G->iniParams.x = stof(setting);
		if (GetPrivateProfileString(L"Constants", L"y", 0, setting, MAX_PATH, iniFile))
			G->iniParams.y = stof(setting);
		if (GetPrivateProfileString(L"Constants", L"z", 0, setting, MAX_PATH, iniFile))
			G->iniParams.z = stof(setting);
		if (GetPrivateProfileString(L"Constants", L"w", 0, setting, MAX_PATH, iniFile))
			G->iniParams.w = stof(setting);

		if (LogFile && G->iniParams.x != -1.0f)
		{
			fprintf(LogFile, "[Constants]\n");
			fprintf(LogFile, "  x=%f\n", G->iniParams.x);
			fprintf(LogFile, "  y=%f\n", G->iniParams.y);
			fprintf(LogFile, "  z=%f\n", G->iniParams.z);
			fprintf(LogFile, "  w=%f\n", G->iniParams.w);
		}

		// Fire up the keyboards and controllers
		InitDirectInput();

		// NVAPI
		D3D11Base::NvAPI_Initialize();

		InitializeCriticalSection(&G->mCriticalSection);

		// Everything set up, let's make another thread for watching the keyboard for shader hunting.
		// It's all multi-threaded rendering now anyway, so it has to be ready for that.
		//HuntingThread = CreateThread(
		//	NULL,                   // default security attributes
		//	0,                      // use default stack size  
		//	Hunting,				// thread function name
		//	&realDevice,			// argument to thread function 
		//	CREATE_SUSPENDED,		// Run late, to avoid window conflicts in DInput
		//	NULL);					// returns the thread identifier 


		//// If we can't init, just log it, not a fatal error.
		//if (HuntingThread == NULL)
		//{
		//	DWORD dw = GetLastError();
		//	if (LogFile) fprintf(LogFile, "Error while creating hunting thread: %x\n", dw);
		//}

		if (LogFile) fprintf(LogFile, "D3D11 DLL initialized.\n");
		if (LogFile && LogDebug) fprintf(LogFile, "[Rendering] XInputDevice = %d\n", XInputDeviceId);
	}
}

void DestroyDLL()
{
	//TerminateThread(
	//	_Inout_  HuntingThread,
	//	_In_     0
	//	);

	//// Should be the normal 100ms wait.
	//DWORD err =  WaitForSingleObject(
	//	_In_  HuntingThread,
	//	_In_  INFINITE
	//	);
	//BOOL ok = CloseHandle(HuntingThread);
	//if (LogFile) fprintf(LogFile, "Hunting thread closed: %x, %d\n", err, ok);

	if (LogFile)
	{
		if (LogFile) fprintf(LogFile, "Destroying DLL...\n");
		fclose(LogFile);
	}
}

// D3DCompiler bridge
struct D3D11BridgeData
{
	UINT64 BinaryHash;
	char *HLSLFileName;
};

int WINAPI D3DKMTCloseAdapter()
{
	if (LogFile && LogDebug) fprintf(LogFile, "D3DKMTCloseAdapter called.\n");

	return 0;
}
int WINAPI D3DKMTDestroyAllocation()
{
	if (LogFile && LogDebug) fprintf(LogFile, "D3DKMTDestroyAllocation called.\n");

	return 0;
}
int WINAPI D3DKMTDestroyContext()
{
	if (LogFile && LogDebug) fprintf(LogFile, "D3DKMTDestroyContext called.\n");

	return 0;
}
int WINAPI D3DKMTDestroyDevice()
{
	if (LogFile && LogDebug) fprintf(LogFile, "D3DKMTDestroyDevice called.\n");

	return 0;
}
int WINAPI D3DKMTDestroySynchronizationObject()
{
	if (LogFile && LogDebug) fprintf(LogFile, "D3DKMTDestroySynchronizationObject called.\n");

	return 0;
}
int WINAPI D3DKMTSetDisplayPrivateDriverFormat()
{
	if (LogFile && LogDebug) fprintf(LogFile, "D3DKMTSetDisplayPrivateDriverFormat called.\n");

	return 0;
}
int WINAPI D3DKMTSignalSynchronizationObject()
{
	if (LogFile && LogDebug) fprintf(LogFile, "D3DKMTSignalSynchronizationObject called.\n");

	return 0;
}
int WINAPI D3DKMTUnlock()
{
	if (LogFile && LogDebug) fprintf(LogFile, "D3DKMTUnlock called.\n");

	return 0;
}
int WINAPI D3DKMTWaitForSynchronizationObject()
{
	if (LogFile && LogDebug) fprintf(LogFile, "D3DKMTWaitForSynchronizationObject called.\n");

	return 0;
}
int WINAPI D3DKMTCreateAllocation()
{
	if (LogFile && LogDebug) fprintf(LogFile, "D3DKMTCreateAllocation called.\n");

	return 0;
}
int WINAPI D3DKMTCreateContext()
{
	if (LogFile && LogDebug) fprintf(LogFile, "D3DKMTCreateContext called.\n");

	return 0;
}
int WINAPI D3DKMTCreateDevice()
{
	if (LogFile && LogDebug) fprintf(LogFile, "D3DKMTCreateDevice called.\n");

	return 0;
}
int WINAPI D3DKMTCreateSynchronizationObject()
{
	if (LogFile && LogDebug) fprintf(LogFile, "D3DKMTCreateSynchronizationObject called.\n");

	return 0;
}
int WINAPI D3DKMTEscape()
{
	if (LogFile && LogDebug) fprintf(LogFile, "D3DKMTEscape called.\n");

	return 0;
}
int WINAPI D3DKMTGetContextSchedulingPriority()
{
	if (LogFile && LogDebug) fprintf(LogFile, "D3DKMTGetContextSchedulingPriority called.\n");

	return 0;
}
int WINAPI D3DKMTGetDisplayModeList()
{
	if (LogFile && LogDebug) fprintf(LogFile, "D3DKMTGetDisplayModeList called.\n");

	return 0;
}
int WINAPI D3DKMTGetMultisampleMethodList()
{
	if (LogFile && LogDebug) fprintf(LogFile, "D3DKMTGetMultisampleMethodList called.\n");

	return 0;
}
int WINAPI D3DKMTGetRuntimeData()
{
	if (LogFile && LogDebug) fprintf(LogFile, "D3DKMTGetRuntimeData called.\n");

	return 0;
}
int WINAPI D3DKMTGetSharedPrimaryHandle()
{
	if (LogFile && LogDebug) fprintf(LogFile, "D3DKMTGetRuntimeData called.\n");

	return 0;
}
int WINAPI D3DKMTLock()
{
	if (LogFile && LogDebug) fprintf(LogFile, "D3DKMTLock called.\n");

	return 0;
}
int WINAPI D3DKMTPresent()
{
	if (LogFile && LogDebug) fprintf(LogFile, "D3DKMTPresent called.\n");

	return 0;
}
int WINAPI D3DKMTQueryAllocationResidency()
{
	if (LogFile && LogDebug) fprintf(LogFile, "D3DKMTQueryAllocationResidency called.\n");

	return 0;
}
int WINAPI D3DKMTRender()
{
	if (LogFile && LogDebug) fprintf(LogFile, "D3DKMTRender called.\n");

	return 0;
}
int WINAPI D3DKMTSetAllocationPriority()
{
	if (LogFile && LogDebug) fprintf(LogFile, "D3DKMTSetAllocationPriority called.\n");

	return 0;
}
int WINAPI D3DKMTSetContextSchedulingPriority()
{
	if (LogFile && LogDebug) fprintf(LogFile, "D3DKMTSetContextSchedulingPriority called.\n");

	return 0;
}
int WINAPI D3DKMTSetDisplayMode()
{
	if (LogFile && LogDebug) fprintf(LogFile, "D3DKMTSetDisplayMode called.\n");

	return 0;
}
int WINAPI D3DKMTSetGammaRamp()
{
	if (LogFile && LogDebug) fprintf(LogFile, "D3DKMTSetGammaRamp called.\n");

	return 0;
}
int WINAPI D3DKMTSetVidPnSourceOwner()
{
	if (LogFile && LogDebug) fprintf(LogFile, "D3DKMTSetVidPnSourceOwner called.\n");

	return 0;
}

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

static HMODULE hD3D11 = 0;

typedef int (WINAPI *tD3DKMTQueryAdapterInfo)(_D3DKMT_QUERYADAPTERINFO *);
static tD3DKMTQueryAdapterInfo _D3DKMTQueryAdapterInfo;

typedef int (WINAPI *tOpenAdapter10)(D3D10DDIARG_OPENADAPTER *adapter);
static tOpenAdapter10 _OpenAdapter10;

typedef int (WINAPI *tOpenAdapter10_2)(D3D10DDIARG_OPENADAPTER *adapter);
static tOpenAdapter10_2 _OpenAdapter10_2;

typedef int (WINAPI *tD3D11CoreCreateDevice)(__int32, int, int, LPCSTR lpModuleName, int, int, int, int, int, int);
static tD3D11CoreCreateDevice _D3D11CoreCreateDevice;

typedef HRESULT(WINAPI *tD3D11CoreCreateLayeredDevice)(const void *unknown0, DWORD unknown1, const void *unknown2, REFIID riid, void **ppvObj);
static tD3D11CoreCreateLayeredDevice _D3D11CoreCreateLayeredDevice;

typedef SIZE_T(WINAPI *tD3D11CoreGetLayeredDeviceSize)(const void *unknown0, DWORD unknown1);
static tD3D11CoreGetLayeredDeviceSize _D3D11CoreGetLayeredDeviceSize;

typedef HRESULT(WINAPI *tD3D11CoreRegisterLayers)(const void *unknown0, DWORD unknown1);
static tD3D11CoreRegisterLayers _D3D11CoreRegisterLayers;

typedef HRESULT(WINAPI *tD3D11CreateDevice)(
	D3D11Base::IDXGIAdapter *pAdapter,
	D3D11Base::D3D_DRIVER_TYPE DriverType,
	HMODULE Software,
	UINT Flags,
	const D3D11Base::D3D_FEATURE_LEVEL *pFeatureLevels,
	UINT FeatureLevels,
	UINT SDKVersion,
	D3D11Base::ID3D11Device **ppDevice,
	D3D11Base::D3D_FEATURE_LEVEL *pFeatureLevel,
	D3D11Base::ID3D11DeviceContext **ppImmediateContext);
static tD3D11CreateDevice _D3D11CreateDevice;
typedef HRESULT(WINAPI *tD3D11CreateDeviceAndSwapChain)(
	D3D11Base::IDXGIAdapter *pAdapter,
	D3D11Base::D3D_DRIVER_TYPE DriverType,
	HMODULE Software,
	UINT Flags,
	const D3D11Base::D3D_FEATURE_LEVEL *pFeatureLevels,
	UINT FeatureLevels,
	UINT SDKVersion,
	D3D11Base::DXGI_SWAP_CHAIN_DESC *pSwapChainDesc,
	D3D11Base::IDXGISwapChain **ppSwapChain,
	D3D11Base::ID3D11Device **ppDevice,
	D3D11Base::D3D_FEATURE_LEVEL *pFeatureLevel,
	D3D11Base::ID3D11DeviceContext **ppImmediateContext);
static tD3D11CreateDeviceAndSwapChain _D3D11CreateDeviceAndSwapChain;

typedef int (WINAPI *tD3DKMTGetDeviceState)(int a);
static tD3DKMTGetDeviceState _D3DKMTGetDeviceState;
typedef int (WINAPI *tD3DKMTOpenAdapterFromHdc)(int a);
static tD3DKMTOpenAdapterFromHdc _D3DKMTOpenAdapterFromHdc;
typedef int (WINAPI *tD3DKMTOpenResource)(int a);
static tD3DKMTOpenResource _D3DKMTOpenResource;
typedef int (WINAPI *tD3DKMTQueryResourceInfo)(int a);
static tD3DKMTQueryResourceInfo _D3DKMTQueryResourceInfo;


static void InitD311()
{
	if (hD3D11) return;
	G = new Globals();
	InitializeDLL();
	if (DLL_PATH[0])
	{
		wchar_t sysDir[MAX_PATH];
		GetModuleFileName(0, sysDir, MAX_PATH);
		wcsrchr(sysDir, L'\\')[1] = 0;
		wcscat(sysDir, DLL_PATH);
		if (LogFile)
		{
			char path[MAX_PATH];
			wcstombs(path, sysDir, MAX_PATH);
			fprintf(LogFile, "trying to load %s\n", path);
		}
		hD3D11 = LoadLibrary(sysDir);
		if (!hD3D11)
		{
			if (LogFile)
			{
				char path[MAX_PATH];
				wcstombs(path, DLL_PATH, MAX_PATH);
				fprintf(LogFile, "load failed. Trying to load %s\n", path);
			}
			hD3D11 = LoadLibrary(DLL_PATH);
		}
	}
	else
	{
		wchar_t sysDir[MAX_PATH];
		SHGetFolderPath(0, CSIDL_SYSTEM, 0, SHGFP_TYPE_CURRENT, sysDir);
#if (_WIN64 && HOOK_SYSTEM32)
		// We'll look for this in MainHook to avoid callback to self.		
		// Must remain all lower case to be matched in MainHook.
		wcscat(sysDir, L"\\original_d3d11.dll");
#else
		wcscat(sysDir, L"\\d3d11.dll");
#endif

		if (LogFile)
		{
			char path[MAX_PATH];
			wcstombs(path, sysDir, MAX_PATH);
			fprintf(LogFile, "trying to load %s\n", path);
		}
		hD3D11 = LoadLibrary(sysDir);
	}
	if (hD3D11 == NULL)
	{
		if (LogFile) fprintf(LogFile, "LoadLibrary on d3d11.dll failed\n");

		return;
	}

	_D3DKMTQueryAdapterInfo = (tD3DKMTQueryAdapterInfo)GetProcAddress(hD3D11, "D3DKMTQueryAdapterInfo");
	_OpenAdapter10 = (tOpenAdapter10)GetProcAddress(hD3D11, "OpenAdapter10");
	_OpenAdapter10_2 = (tOpenAdapter10_2)GetProcAddress(hD3D11, "OpenAdapter10_2");
	_D3D11CoreCreateDevice = (tD3D11CoreCreateDevice)GetProcAddress(hD3D11, "D3D11CoreCreateDevice");
	_D3D11CoreCreateLayeredDevice = (tD3D11CoreCreateLayeredDevice)GetProcAddress(hD3D11, "D3D11CoreCreateLayeredDevice");
	_D3D11CoreGetLayeredDeviceSize = (tD3D11CoreGetLayeredDeviceSize)GetProcAddress(hD3D11, "D3D11CoreGetLayeredDeviceSize");
	_D3D11CoreRegisterLayers = (tD3D11CoreRegisterLayers)GetProcAddress(hD3D11, "D3D11CoreRegisterLayers");
	_D3D11CreateDevice = (tD3D11CreateDevice)GetProcAddress(hD3D11, "D3D11CreateDevice");
	_D3D11CreateDeviceAndSwapChain = (tD3D11CreateDeviceAndSwapChain)GetProcAddress(hD3D11, "D3D11CreateDeviceAndSwapChain");
	_D3DKMTGetDeviceState = (tD3DKMTGetDeviceState)GetProcAddress(hD3D11, "D3DKMTGetDeviceState");
	_D3DKMTOpenAdapterFromHdc = (tD3DKMTOpenAdapterFromHdc)GetProcAddress(hD3D11, "D3DKMTOpenAdapterFromHdc");
	_D3DKMTOpenResource = (tD3DKMTOpenResource)GetProcAddress(hD3D11, "D3DKMTOpenResource");
	_D3DKMTQueryResourceInfo = (tD3DKMTQueryResourceInfo)GetProcAddress(hD3D11, "D3DKMTQueryResourceInfo");
}

int WINAPI D3DKMTQueryAdapterInfo(_D3DKMT_QUERYADAPTERINFO *info)
{
	InitD311();
	if (LogFile) fprintf(LogFile, "D3DKMTQueryAdapterInfo called.\n");

	return (*_D3DKMTQueryAdapterInfo)(info);
}

int WINAPI OpenAdapter10(struct D3D10DDIARG_OPENADAPTER *adapter)
{
	InitD311();
	if (LogFile) fprintf(LogFile, "OpenAdapter10 called.\n");

	return (*_OpenAdapter10)(adapter);
}

int WINAPI OpenAdapter10_2(struct D3D10DDIARG_OPENADAPTER *adapter)
{
	InitD311();
	if (LogFile) fprintf(LogFile, "OpenAdapter10_2 called.\n");

	return (*_OpenAdapter10_2)(adapter);
}

int WINAPI D3D11CoreCreateDevice(__int32 a, int b, int c, LPCSTR lpModuleName, int e, int f, int g, int h, int i, int j)
{
	InitD311();
	if (LogFile) fprintf(LogFile, "D3D11CoreCreateDevice called.\n");

	return (*_D3D11CoreCreateDevice)(a, b, c, lpModuleName, e, f, g, h, i, j);
}


HRESULT WINAPI D3D11CoreCreateLayeredDevice(const void *unknown0, DWORD unknown1, const void *unknown2, REFIID riid, void **ppvObj)
{
	InitD311();
	if (LogFile) fprintf(LogFile, "D3D11CoreCreateLayeredDevice called.\n");

	return (*_D3D11CoreCreateLayeredDevice)(unknown0, unknown1, unknown2, riid, ppvObj);
}

SIZE_T WINAPI D3D11CoreGetLayeredDeviceSize(const void *unknown0, DWORD unknown1)
{
	InitD311();
	// Call from D3DCompiler (magic number from there) ?
	if ((intptr_t)unknown0 == 0x77aa128b)
	{
		if (LogFile) fprintf(LogFile, "Shader code info from D3DCompiler_xx.dll wrapper received:\n");

		D3D11BridgeData *data = (D3D11BridgeData *)unknown1;
		if (LogFile) fprintf(LogFile, "  Bytecode hash = %08lx%08lx\n", (UINT32)(data->BinaryHash >> 32), (UINT32)data->BinaryHash);
		if (LogFile) fprintf(LogFile, "  Filename = %s\n", data->HLSLFileName);

		G->mCompiledShaderMap[data->BinaryHash] = data->HLSLFileName;
		return 0xaa77125b;
	}
	if (LogFile) fprintf(LogFile, "D3D11CoreGetLayeredDeviceSize called.\n");

	return (*_D3D11CoreGetLayeredDeviceSize)(unknown0, unknown1);
}

HRESULT WINAPI D3D11CoreRegisterLayers(const void *unknown0, DWORD unknown1)
{
	InitD311();
	if (LogFile) fprintf(LogFile, "D3D11CoreRegisterLayers called.\n");

	return (*_D3D11CoreRegisterLayers)(unknown0, unknown1);
}

static void EnableStereo()
{
	if (!G->gForceStereo) return;

	// Prepare NVAPI for use in this application
	D3D11Base::NvAPI_Status status;
	status = D3D11Base::NvAPI_Initialize();
	if (status != D3D11Base::NVAPI_OK)
	{
		D3D11Base::NvAPI_ShortString errorMessage;
		NvAPI_GetErrorMessage(status, errorMessage);
		if (LogFile) fprintf(LogFile, "  stereo init failed: %s\n", errorMessage);
	}
	else
	{
		// Check the Stereo availability
		D3D11Base::NvU8 isStereoEnabled;
		status = D3D11Base::NvAPI_Stereo_IsEnabled(&isStereoEnabled);
		// Stereo status report an error
		if (status != D3D11Base::NVAPI_OK)
		{
			// GeForce Stereoscopic 3D driver is not installed on the system
			if (LogFile) fprintf(LogFile, "  stereo init failed: no stereo driver detected.\n");
		}
		// Stereo is available but not enabled, let's enable it
		else if (D3D11Base::NVAPI_OK == status && !isStereoEnabled)
		{
			if (LogFile) fprintf(LogFile, "  stereo available and disabled. Enabling stereo.\n");
			status = D3D11Base::NvAPI_Stereo_Enable();
			if (status != D3D11Base::NVAPI_OK)
				if (LogFile) fprintf(LogFile, "    enabling stereo failed.\n");
		}

		if (G->gCreateStereoProfile)
		{
			if (LogFile) fprintf(LogFile, "  enabling registry profile.\n");

			D3D11Base::NvAPI_Stereo_CreateConfigurationProfileRegistryKey(D3D11Base::NVAPI_STEREO_DEFAULT_REGISTRY_PROFILE);
		}
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
			std::vector<void *>::iterator m;
			int pos = 0;
			for (m = i->second.RenderTargets.begin(); m != i->second.RenderTargets.end(); ++m)
			{
				UINT64 id = G->mRenderTargets[*m];
				sprintf(buf, "  <RenderTarget id=%d handle=%p>%016llx</RenderTarget>\n", pos, *m, id);
				WriteFile(f, buf, castStrLen(buf), &written, 0);
				++pos;
			}
			const char *FOOTER = "</PixelShader>\n";
			WriteFile(f, FOOTER, castStrLen(FOOTER), &written, 0);
		}
		if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
		CloseHandle(f);
	}
	else
	{
		if (LogFile) fprintf(LogFile, "Error dumping ShaderUsage.txt\n");

	}
}

//--------------------------------------------------------------------------------------------------

// Convenience class to avoid passing wrong objects all of Blob type.
// For strong type checking.  Already had a couple of bugs with generic ID3DBlobs.

class AsmTextBlob : public D3D11Base::ID3DBlob
{
};



// Write the decompiled text as HLSL source code to the txt file.
// Now also writing the ASM text to the bottom of the file, commented out.
// This keeps the ASM with the HLSL for reference and should be more convenient.
//
// This will not overwrite any file that is already there. 
// The assumption is that the shaderByteCode that we have here is always the most up to date,
// and thus is not different than the file on disk.
// If a file was already extant in the ShaderFixes, it will be picked up at game launch as the master shaderByteCode.

static bool WriteHLSL(string hlslText, AsmTextBlob* asmTextBlob, UINT64 hash, wstring shaderType)
{
	wchar_t fullName[MAX_PATH];
	FILE *fw;

	wsprintf(fullName, L"%ls\\%08lx%08lx-%ls_replace.txt", SHADER_PATH, (UINT32)(hash >> 32), (UINT32)hash, shaderType.c_str());
	_wfopen_s(&fw, fullName, L"rb");
	if (fw)
	{
		if (LogFile) fwprintf(LogFile, L"    marked shader file already exists: %s\n", fullName);
		fclose(fw);
		_wfopen_s(&fw, fullName, L"ab");
		fprintf_s(fw, " ");					// Touch file to update mod date as a convenience.
		fclose(fw);
		Beep(1800, 100);					// Short High beep for for double beep that it's already there.
		return true;
	}

	_wfopen_s(&fw, fullName, L"wb");
	if (!fw)
	{
		if (LogFile) fwprintf(LogFile, L"    error storing marked shader to %s\n", fullName);
		return false;
	}

	if (LogFile) fwprintf(LogFile, L"    storing patched shader to %s\n", fullName);

	fwrite(hlslText.c_str(), 1, hlslText.size(), fw);

	fprintf_s(fw, "\n\n/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
	fwrite(asmTextBlob->GetBufferPointer(), 1, asmTextBlob->GetBufferSize(), fw);
	fprintf_s(fw, "\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/\n");

	fclose(fw);
	return true;
}


// Decompile code passed in as assembly text, and shader byte code.
// This is pretty heavyweight obviously, so it is only being done during Mark operations.
// Todo: another copy/paste job, we really need some subroutines, utility library.

static string Decompile(D3D11Base::ID3DBlob* pShaderByteCode, AsmTextBlob* disassembly)
{
	if (LogFile) fprintf(LogFile, "    creating HLSL representation.\n");

	bool patched = false;
	string shaderModel;
	bool errorOccurred = false;
	ParseParameters p;
	p.bytecode = pShaderByteCode->GetBufferPointer();
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
		if (LogFile) fprintf(LogFile, "    error while decompiling.\n");
	}

	return decompiledCode;
}


// Get the text disassembly of the shader byte code specified.

static AsmTextBlob* GetDisassembly(D3D11Base::ID3DBlob* pCode)
{
	D3D11Base::ID3DBlob *disassembly;

	HRESULT ret = D3D11Base::D3DDisassemble(pCode->GetBufferPointer(), pCode->GetBufferSize(), D3D_DISASM_ENABLE_DEFAULT_VALUE_PRINTS, 0,
		&disassembly);
	if (FAILED(ret))
	{
		if (LogFile) fprintf(LogFile, "    disassembly of original shader failed: \n");
		return NULL;
	}

	return (AsmTextBlob*)disassembly;
}

// Write the disassembly to the text file.
// If the file already exists, return an error, to avoid overwrite.  
// Generally if the file is already there, the code we would write on Mark is the same anyway.

static bool WriteDisassembly(UINT64 hash, wstring shaderType, AsmTextBlob* asmTextBlob)
{
	wchar_t fullName[MAX_PATH];
	FILE *f;

	wsprintf(fullName, L"%ls\\%08lx%08lx-%ls.txt", SHADER_PATH, (UINT32)(hash >> 32), (UINT32)(hash), shaderType.c_str());

	// Check if the file already exists.
	_wfopen_s(&f, fullName, L"rb");
	if (f)
	{
		if (LogFile) fwprintf(LogFile, L"    Shader Mark .bin file already exists: %s\n", fullName);
		fclose(f);
		return false;
	}

	_wfopen_s(&f, fullName, L"wb");
	if (!f)
	{
		if (LogFile) fwprintf(LogFile, L"    Shader Mark could not write asm text file: %s\n", fullName);
		return false;
	}

	fwrite(asmTextBlob->GetBufferPointer(), 1, asmTextBlob->GetBufferSize(), f);
	fclose(f);
	if (LogFile) fwprintf(LogFile, L"    storing disassembly to %s\n", fullName);

	return true;
}

// Different version that takes asm text already.

static string GetShaderModel(AsmTextBlob* asmTextBlob)
{
	// Read shader model. This is the first not commented line.
	char *pos = (char *)asmTextBlob->GetBufferPointer();
	char *end = pos + asmTextBlob->GetBufferSize();
	while (pos[0] == '/' && pos < end)
	{
		while (pos[0] != 0x0a && pos < end) pos++;
		pos++;
	}
	// Extract model.
	char *eol = pos;
	while (eol[0] != 0x0a && pos < end) eol++;
	string shaderModel(pos, eol);

	return shaderModel;
}


// Compile a new shader from  HLSL text input, and report on errors if any.
// Return the binary blob of pCode to be activated with CreateVertexShader or CreatePixelShader.
// If the timeStamp has not changed from when it was loaded, skip the recompile, and return false as not an 
// error, but skipped.  On actual errors, return true so that we bail out.

static bool CompileShader(wchar_t *shaderFixPath, wchar_t *fileName, const char *shaderModel, UINT64 hash, wstring shaderType, FILETIME* timeStamp,
	_Outptr_ D3D11Base::ID3DBlob** pCode)
{
	*pCode = nullptr;
	wchar_t fullName[MAX_PATH];
	wsprintf(fullName, L"%s\\%s", shaderFixPath, fileName);

	HANDLE f = CreateFile(fullName, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (f == INVALID_HANDLE_VALUE)
	{
		if (LogFile) fprintf(LogFile, "    ReloadShader shader not found: %ls\n", fullName);

		return true;
	}


	DWORD srcDataSize = GetFileSize(f, 0);
	char *srcData = new char[srcDataSize];
	DWORD readSize;
	FILETIME curFileTime;

	if (!ReadFile(f, srcData, srcDataSize, &readSize, 0)
		|| !GetFileTime(f, NULL, NULL, &curFileTime)
		|| srcDataSize != readSize)
	{
		if (LogFile) fprintf(LogFile, "    Error reading txt file.\n");

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

	if (LogFile) fprintf(LogFile, "   >Replacement shader found. Re-Loading replacement HLSL code from %ls\n", fileName);
	if (LogFile) fprintf(LogFile, "    Reload source code loaded. Size = %d\n", srcDataSize);
	if (LogFile) fprintf(LogFile, "    compiling replacement HLSL code with shader model %s\n", shaderModel);


	D3D11Base::ID3DBlob* pByteCode = nullptr;
	D3D11Base::ID3DBlob* pErrorMsgs = nullptr;
	HRESULT ret = D3D11Base::D3DCompile(srcData, srcDataSize, "wrapper1349", 0, ((D3D11Base::ID3DInclude*)(UINT_PTR)1),
		"main", shaderModel, D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &pByteCode, &pErrorMsgs);

	delete srcData; srcData = 0;

	// bo3b: pretty sure that we do not need to copy the data. That data is what we need to pass to CreateVertexShader
	// Example taken from: http://msdn.microsoft.com/en-us/library/windows/desktop/hh968107(v=vs.85).aspx
	//char *pCode = 0;
	//SIZE_T pCodeSize;
	//if (pCompiledOutput)
	//{
	//	pCodeSize = pCompiledOutput->GetBufferSize();
	//	pCode = new char[pCodeSize];
	//	memcpy(pCode, pCompiledOutput->GetBufferPointer(), pCodeSize);
	//	pCompiledOutput->Release(); pCompiledOutput = 0;
	//}

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

	if (FAILED(ret))
	{
		if (pByteCode)
		{
			pByteCode->Release();
			pByteCode = 0;
		}
		return true;
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
				fprintf(LogFile, "    storing compiled shader to %s\n", fileName);
			else
				fprintf(LogFile, "    error writing compiled shader to %s\n", fileName);
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

static bool ReloadShader(wchar_t *shaderPath, wchar_t *fileName, D3D11Base::ID3D11Device *realDevice)
{
	UINT64 hash;
	D3D11Base::ID3D11DeviceChild* oldShader = NULL;
	D3D11Base::ID3D11DeviceChild* replacement = NULL;
	D3D11Base::ID3D11ClassLinkage* classLinkage;
	D3D11Base::ID3DBlob* shaderCode;
	string shaderModel;
	wstring shaderType;		// "vs" or "ps" maybe "gs"
	FILETIME timeStamp;
	HRESULT hr = E_FAIL;

	// Extract hash from first 16 characters of file name so we can look up details by hash
	wstring ws = fileName;
	hash = stoull(ws.substr(0, 16), NULL, 16);

	// Find the original shader bytecode in the mReloadedShaders Map. This map contains entries for all
	// shaders from the ShaderFixes and ShaderCache folder, and can also include .bin files that were loaded directly.
	// We include ShaderCache because that allows moving files into ShaderFixes as they are identified.
	// This needs to use the value to find the key, so a linear search.
	// It's notable that the map can contain multiple copies of the same hash, used for different visual
	// items, but with same original code.  We need to update all copies.
	for each (pair<D3D11Base::ID3D11DeviceChild *, OriginalShaderInfo> iter in G->mReloadedShaders)
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
				if (LogFile) fprintf(LogFile, "> failed to find original shader in mReloadedShaders: %ls\n", fileName);
				continue;
			}

			// If shaderModel is "bin", that means the original was loaded as a binary object, and thus shaderModel is unknown.
			// Disassemble the binary to get that string.
			if (shaderModel.compare("bin") == 0)
			{
				AsmTextBlob* asmTextBlob = GetDisassembly(shaderCode);
				if (!asmTextBlob)
					return false;
				shaderModel = GetShaderModel(asmTextBlob);
				if (shaderModel.empty())
					return false;
				G->mReloadedShaders[oldShader].shaderModel = shaderModel;
			}

			// Compile anew. If timestamp is unchanged, the code is unchanged, continue to next shader.
			D3D11Base::ID3DBlob *pShaderBytecode = NULL;
			if (!CompileShader(shaderPath, fileName, shaderModel.c_str(), hash, shaderType, &timeStamp, &pShaderBytecode))
				continue;

			// If we compiled but got nothing, that's a fatal error we need to report.
			if (pShaderBytecode == NULL)
				return false;

			// Update timestamp, since we have an edited file.
			G->mReloadedShaders[oldShader].timeStamp = timeStamp;

			// This needs to call the real CreateVertexShader, not our wrapped version
			if (shaderType.compare(L"vs") == 0)
			{
				hr = realDevice->CreateVertexShader(pShaderBytecode->GetBufferPointer(), pShaderBytecode->GetBufferSize(), classLinkage,
					(D3D11Base::ID3D11VertexShader**) &replacement);
			}
			else if (shaderType.compare(L"ps") == 0)
			{
				hr = realDevice->CreatePixelShader(pShaderBytecode->GetBufferPointer(), pShaderBytecode->GetBufferSize(), classLinkage,
					(D3D11Base::ID3D11PixelShader**) &replacement);
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

			if (LogFile) fprintf(LogFile, "> successfully reloaded shader: %ls\n", fileName);
		}
	}	// for every registered shader in mReloadedShaders 

	return true;
}


// When a shader is marked by the user, we want to automatically move it to the ShaderFixes folder
// The universal way to do this is to keep the shaderByteCode around, and when mark happens, use that as
// the replacement and build code to match.  This handles all the variants of preload, cache, hlsl 
// or not, and allows creating new files on a first run.  Should be handy.

static void CopyToFixes(UINT64 hash, D3D11Base::ID3D11Device *device)
{
	bool success = false;
	string shaderModel;
	AsmTextBlob* asmTextBlob;
	string decompiled;

	// The key of the map is the actual shader, we thus need to do a linear search to find our marked hash.
	for each (pair<D3D11Base::ID3D11DeviceChild *, OriginalShaderInfo> iter in G->mReloadedShaders)
	{
		if (iter.second.hash == hash)
		{
			asmTextBlob = GetDisassembly(iter.second.byteCode);
			if (!asmTextBlob)
				break;

			// Disassembly file is written, now decompile the current byte code into HLSL.
			shaderModel = GetShaderModel(asmTextBlob);
			decompiled = Decompile(iter.second.byteCode, asmTextBlob);
			if (decompiled.empty())
				break;

			// Save the decompiled text, and ASM text into the .txt source file.
			if (!WriteHLSL(decompiled, asmTextBlob, hash, iter.second.shaderType))
				break;

			asmTextBlob->Release();

			// Lastly, reload the shader generated, to check for decompile errors, set it as the active 
			// shader code, in case there are visual errors, and make it the match the code in the file.
			wchar_t fileName[MAX_PATH];
			wsprintf(fileName, L"%08lx%08lx-%ls_replace.txt", (UINT32)(hash >> 32), (UINT32)(hash), iter.second.shaderType.c_str());
			if (!ReloadShader(SHADER_PATH, fileName, device))
				break;

			// There can be more than one in the map with the same hash, but we only need a single copy to
			// make the hlsl file output, so exit with success.
			success = true;
			break;
		}
	}

	if (success)
	{
		Beep(1800, 400);		// High beep for success, to notify it's running fresh fixes.
		if (LogFile) fprintf(LogFile, "> successfully copied Marked shader to ShaderFixes\n");
	}
	else
	{
		Beep(200, 150);			// Bonk sound for failure.
		if (LogFile) fprintf(LogFile, "> FAILED to copy Marked shader to ShaderFixes\n");
	}
}


// Todo: I'm just hacking this in here at the moment, because I'm not positive it will work.
//	Once it's clearly a good path, we can move it out.

// Using the wrapped Device, we want to change the iniParams associated with the device.

void SetIniParams(D3D11Base::ID3D11Device *device, bool on)
{
	D3D11Wrapper::ID3D11Device* wrapped = (D3D11Wrapper::ID3D11Device*) D3D11Wrapper::ID3D11Device::m_List.GetDataPtr(device);
	D3D11Base::ID3D11DeviceContext* realContext; device->GetImmediateContext(&realContext);
	D3D11Base::D3D11_MAPPED_SUBRESOURCE mappedResource;
	memset(&mappedResource, 0, sizeof(D3D11Base::D3D11_MAPPED_SUBRESOURCE));

	if (on)
	{
		DirectX::XMFLOAT4 zeroed = {0, 0, 0, 0};

		//	Disable GPU access to the texture ini data so we can rewrite it.
		realContext->Map(wrapped->mIniTexture, 0, D3D11Base::D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
		memcpy(mappedResource.pData, &zeroed, sizeof(zeroed));
		realContext->Unmap(wrapped->mIniTexture, 0);
	}
	else
	{
		// Restore texture ini data to original values from d3dx.ini
		realContext->Map(wrapped->mIniTexture, 0, D3D11Base::D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
		memcpy(mappedResource.pData, &G->iniParams, sizeof(G->iniParams));
		realContext->Unmap(wrapped->mIniTexture, 0);
	}
}

// Check for keys being sent from the user to change behavior as a hotkey.  This will update
// the iniParams live, so that the shaders can use that keypress to change behavior.

// Four states of toggle, with key presses.  
enum TState
{
	offDown, offUp, onDown, onUp
};
TState toggleState = offUp;

void CheckForKeys(D3D11Base::ID3D11Device *device)
{
	bool escKey = (GetAsyncKeyState(VK_F2) < 0);
	TState lastState = toggleState;

	// Must cycle through different states based solely on user input.
	switch (toggleState)
	{
		case offUp:		if (escKey) toggleState = onDown;
			break;
		case onDown:	if (!escKey) toggleState = onUp;
			break;
		case onUp:		if (escKey) toggleState = offDown;
			break;
		case offDown:	if (!escKey) toggleState = offUp;
			break;
	}

	// Only operate on state changes, since this gets called multiple times per frame.
	if ((toggleState != lastState) && (toggleState == onUp))
	{
		if (LogFile) fprintf(LogFile, "ESC key activated.\n");
		SetIniParams(device, true);
	}
	if ((toggleState != lastState) && (toggleState == offUp))
	{
		if (LogFile) fprintf(LogFile, "ESC key deactivated.\n");
		SetIniParams(device, false);
	}
}


extern "C" int * __cdecl nvapi_QueryInterface(unsigned int offset);

// Called indirectly through the QueryInterface for every vertical blanking, based on calls to
// IDXGISwapChain1::Present1 and/or IDXGISwapChain::Present in the dxgi interface wrapper.
// This looks for user input for shader hunting.
// No longer called from Present, because we weren't getting calls from some games, probably because
// of a missing wrapper on some games.  
// This will do the same basic task, of giving time to the UI so that we can hunt shaders by selectively
// disabling them.  
// Another approach was to use an alternate Thread to give this time, but that still meant we needed to
// get the proper ID3D11Device object to pass in, and possibly introduces race conditions or threading
// problems. (Didn't see any, but.)
// Another problem from here is that the DirectInput code has some sort of bug where if you call for
// key events before the window has been created, that it locks out DirectInput from then on. To avoid
// that we can do a late-binding approach to start looking for events.  

// Rather than do all that, we now insert a RunFrameActions in the Draw method of the Context object,
// where it is absolutely certain that the game is fully loaded and ready to go, because it's actively
// drawing.  This gives us too many calls, maybe 5 per frame, but should not be a problem. The code
// is expecting to be called in a loop, and locks out auto-repeat using that looping.

// Draw is a very late binding for the game, and should solve all these problems, and allow us to retire
// the dxgi wrapper as unneeded.  The draw is caught at AfterDraw in the Context, which is called for
// every type of Draw, including DrawIndexed.

static void RunFrameActions(D3D11Base::ID3D11Device *device)
{
	if (LogFile && LogDebug) fprintf(LogFile, "Running frame actions.  Device: %p\n", device);

	// Regardless of log settings, since this runs every frame, let's flush the log
	// so that the most lost will be one frame worth.  Tradeoff of performance to accuracy
	if (LogFile) fflush(LogFile);

	// Send the secret callback to the nvapi.dll to give time to aiming override.
	// This is done here because this will be called at first game Draw call, and thus very
	// late from the init standpoint, which fixes DirectInput failures.  And avoids
	// crashes when we use a secondary thread to give time to aiming override.
	//nvapi_QueryInterface(0xb03bb03b);

	// Give time to our keyboard handling for hot keys that can change iniParams.
	CheckForKeys(device);

	// Optimize for game play by skipping all shader hunting, screenshots, reload shaders.
	if (!G->hunting)
		return;

	bool newEvent = UpdateInputState();

	// Update the huntTime whenever we get fresh user input.
	if (newEvent)
		G->huntTime = time(NULL);


	// Screenshot?
	if (Action[3] && !G->take_screenshot)
	{
		if (LogFile) fprintf(LogFile, "> capturing screenshot\n");

		G->take_screenshot = true;
		D3D11Wrapper::ID3D11Device* wrapped = (D3D11Wrapper::ID3D11Device*) D3D11Wrapper::ID3D11Device::m_List.GetDataPtr(device);
		if (wrapped->mStereoHandle)
		{
			D3D11Base::NvAPI_Status err;
			err = D3D11Base::NvAPI_Stereo_CapturePngImage(wrapped->mStereoHandle);
			if (err != D3D11Base::NVAPI_OK)
			{
				if (LogFile) fprintf(LogFile, "> screenshot failed, error:%d\n", err);
				Beep(300, 200); Beep(200, 150);		// Brnk, dunk sound for failure.
			}
		}
	}
	if (!Action[3]) G->take_screenshot = false;

	// Reload all fixes from ShaderFixes?
	if (Action[21] && !G->reload_fixes)
	{
		if (LogFile) fprintf(LogFile, "> reloading *_replace.txt fixes from ShaderFixes\n");
		G->reload_fixes = true;

		if (SHADER_PATH[0])
		{
			bool success = false;
			WIN32_FIND_DATA findFileData;
			wchar_t fileName[MAX_PATH];

			// Strict file name format, to allow renaming out of the way. "00aa7fa12bbf66b3-ps_replace.txt"
			// Will still blow up if the first characters are not hex.
			wsprintf(fileName, L"%ls\\????????????????-??_replace.txt", SHADER_PATH);
			HANDLE hFind = FindFirstFile(fileName, &findFileData);
			if (hFind != INVALID_HANDLE_VALUE)
			{
				do
				{
					success = ReloadShader(SHADER_PATH, findFileData.cFileName, device);
				} while (FindNextFile(hFind, &findFileData) && success);
				FindClose(hFind);
			}

			if (success)
			{
				Beep(1800, 400);		// High beep for success, to notify it's running fresh fixes.
				if (LogFile) fprintf(LogFile, "> successfully reloaded shaders from ShaderFixes\n");
			}
			else
			{
				Beep(200, 150);			// Bonk sound for failure.
				if (LogFile) fprintf(LogFile, "> FAILED to reload shaders from ShaderFixes\n");
			}
		}
	}
	if (!Action[21]) G->reload_fixes = false;

	// Traverse index buffers?
	if (Action[4] && !G->next_indexbuffer)
	{
		G->next_indexbuffer = true;
		if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
		std::set<UINT64>::iterator i = G->mVisitedIndexBuffers.find(G->mSelectedIndexBuffer);
		if (i != G->mVisitedIndexBuffers.end() && ++i != G->mVisitedIndexBuffers.end())
		{
			G->mSelectedIndexBuffer = *i;
			G->mSelectedIndexBufferPos++;
			if (LogFile) fprintf(LogFile, "> traversing to next index buffer #%d. Number of index buffers in frame: %d\n", G->mSelectedIndexBufferPos, G->mVisitedIndexBuffers.size());
		}
		if (i == G->mVisitedIndexBuffers.end() && ++G->mSelectedIndexBufferPos < G->mVisitedIndexBuffers.size() && G->mSelectedIndexBufferPos >= 0)
		{
			i = G->mVisitedIndexBuffers.begin();
			std::advance(i, G->mSelectedIndexBufferPos);
			G->mSelectedIndexBuffer = *i;
			if (LogFile) fprintf(LogFile, "> last index buffer lost. traversing to next index buffer #%d. Number of index buffers in frame: %d\n", G->mSelectedIndexBufferPos, G->mVisitedIndexBuffers.size());
		}
		if (i == G->mVisitedIndexBuffers.end() && G->mVisitedIndexBuffers.size() != 0)
		{
			G->mSelectedIndexBufferPos = 0;
			if (LogFile) fprintf(LogFile, "> traversing to index buffer #0. Number of index buffers in frame: %d\n", G->mVisitedIndexBuffers.size());

			G->mSelectedIndexBuffer = *G->mVisitedIndexBuffers.begin();
		}
		if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
	}
	if (!Action[4]) G->next_indexbuffer = false;
	if (Action[5] && !G->prev_indexbuffer)
	{
		G->prev_indexbuffer = true;
		if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
		std::set<UINT64>::iterator i = G->mVisitedIndexBuffers.find(G->mSelectedIndexBuffer);
		if (i != G->mVisitedIndexBuffers.end() && i != G->mVisitedIndexBuffers.begin())
		{
			--i;
			G->mSelectedIndexBuffer = *i;
			G->mSelectedIndexBufferPos--;
			if (LogFile) fprintf(LogFile, "> traversing to previous index buffer #%d. Number of index buffers in frame: %d\n", G->mSelectedIndexBufferPos, G->mVisitedIndexBuffers.size());
		}
		if (i == G->mVisitedIndexBuffers.end() && --G->mSelectedIndexBufferPos < G->mVisitedIndexBuffers.size() && G->mSelectedIndexBufferPos >= 0)
		{
			i = G->mVisitedIndexBuffers.begin();
			std::advance(i, G->mSelectedIndexBufferPos);
			G->mSelectedIndexBuffer = *i;
			if (LogFile) fprintf(LogFile, "> last index buffer lost. traversing to previous index buffer #%d. Number of index buffers in frame: %d\n", G->mSelectedIndexBufferPos, G->mVisitedIndexBuffers.size());
		}
		if (i == G->mVisitedIndexBuffers.end() && G->mVisitedIndexBuffers.size() != 0)
		{
			G->mSelectedIndexBufferPos = 0;
			if (LogFile) fprintf(LogFile, "> traversing to index buffer #0. Number of index buffers in frame: %d\n", G->mVisitedIndexBuffers.size());

			G->mSelectedIndexBuffer = *G->mVisitedIndexBuffers.begin();
		}
		if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
	}
	if (!Action[5]) G->prev_indexbuffer = false;
	if (Action[6] && !G->mark_indexbuffer)
	{
		G->mark_indexbuffer = true;
		if (LogFile)
		{
			fprintf(LogFile, ">>>> Index buffer marked: index buffer hash = %08lx%08lx\n", (UINT32)(G->mSelectedIndexBuffer >> 32), (UINT32)G->mSelectedIndexBuffer);
			for (std::set<UINT64>::iterator i = G->mSelectedIndexBuffer_PixelShader.begin(); i != G->mSelectedIndexBuffer_PixelShader.end(); ++i)
				fprintf(LogFile, "     visited pixel shader hash = %08lx%08lx\n", (UINT32)(*i >> 32), (UINT32)*i);
			for (std::set<UINT64>::iterator i = G->mSelectedIndexBuffer_VertexShader.begin(); i != G->mSelectedIndexBuffer_VertexShader.end(); ++i)
				fprintf(LogFile, "     visited vertex shader hash = %08lx%08lx\n", (UINT32)(*i >> 32), (UINT32)*i);
		}
		if (G->DumpUsage) DumpUsage();
	}
	if (!Action[6]) G->mark_indexbuffer = false;

	// Traverse pixel shaders?
	if (Action[0] && !G->next_pixelshader)
	{
		G->next_pixelshader = true;
		if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
		std::set<UINT64>::const_iterator i = G->mVisitedPixelShaders.find(G->mSelectedPixelShader);
		if (i != G->mVisitedPixelShaders.end() && ++i != G->mVisitedPixelShaders.end())
		{
			G->mSelectedPixelShader = *i;
			G->mSelectedPixelShaderPos++;
			if (LogFile) fprintf(LogFile, "> traversing to next pixel shader #%d. Number of pixel shaders in frame: %d\n", G->mSelectedPixelShaderPos, G->mVisitedPixelShaders.size());
		}
		if (i == G->mVisitedPixelShaders.end() && ++G->mSelectedPixelShaderPos < G->mVisitedPixelShaders.size() && G->mSelectedPixelShaderPos >= 0)
		{
			i = G->mVisitedPixelShaders.begin();
			std::advance(i, G->mSelectedPixelShaderPos);
			G->mSelectedPixelShader = *i;
			if (LogFile) fprintf(LogFile, "> last pixel shader lost. traversing to next pixel shader #%d. Number of pixel shaders in frame: %d\n", G->mSelectedPixelShaderPos, G->mVisitedPixelShaders.size());
		}
		if (i == G->mVisitedPixelShaders.end() && G->mVisitedPixelShaders.size() != 0)
		{
			G->mSelectedPixelShaderPos = 0;
			if (LogFile) fprintf(LogFile, "> traversing to pixel shader #0. Number of pixel shaders in frame: %d\n", G->mVisitedPixelShaders.size());

			G->mSelectedPixelShader = *G->mVisitedPixelShaders.begin();
		}
		if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
	}
	if (!Action[0]) G->next_pixelshader = false;
	if (Action[1] && !G->prev_pixelshader)
	{
		G->prev_pixelshader = true;
		if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
		std::set<UINT64>::iterator i = G->mVisitedPixelShaders.find(G->mSelectedPixelShader);
		if (i != G->mVisitedPixelShaders.end() && i != G->mVisitedPixelShaders.begin())
		{
			--i;
			G->mSelectedPixelShader = *i;
			G->mSelectedPixelShaderPos--;
			if (LogFile) fprintf(LogFile, "> traversing to previous pixel shader #%d. Number of pixel shaders in frame: %d\n", G->mSelectedPixelShaderPos, G->mVisitedPixelShaders.size());
		}
		if (i == G->mVisitedPixelShaders.end() && --G->mSelectedPixelShaderPos < G->mVisitedPixelShaders.size() && G->mSelectedPixelShaderPos >= 0)
		{
			i = G->mVisitedPixelShaders.begin();
			std::advance(i, G->mSelectedPixelShaderPos);
			G->mSelectedPixelShader = *i;
			if (LogFile) fprintf(LogFile, "> last pixel shader lost. traversing to previous pixel shader #%d. Number of pixel shaders in frame: %d\n", G->mSelectedPixelShaderPos, G->mVisitedPixelShaders.size());
		}
		if (i == G->mVisitedPixelShaders.end() && G->mVisitedPixelShaders.size() != 0)
		{
			G->mSelectedPixelShaderPos = 0;
			if (LogFile) fprintf(LogFile, "> traversing to pixel shader #0. Number of pixel shaders in frame: %d\n", G->mVisitedPixelShaders.size());

			G->mSelectedPixelShader = *G->mVisitedPixelShaders.begin();
		}
		if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
	}
	if (!Action[1]) G->prev_pixelshader = false;
	if (Action[2] && !G->mark_pixelshader)
	{
		G->mark_pixelshader = true;
		if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
		if (LogFile)
		{
			fprintf(LogFile, ">>>> Pixel shader marked: pixel shader hash = %08lx%08lx\n", (UINT32)(G->mSelectedPixelShader >> 32), (UINT32)G->mSelectedPixelShader);
			for (std::set<UINT64>::iterator i = G->mSelectedPixelShader_IndexBuffer.begin(); i != G->mSelectedPixelShader_IndexBuffer.end(); ++i)
				fprintf(LogFile, "     visited index buffer hash = %08lx%08lx\n", (UINT32)(*i >> 32), (UINT32)*i);
			for (std::set<UINT64>::iterator i = G->mPixelShaderInfo[G->mSelectedPixelShader].PartnerShader.begin(); i != G->mPixelShaderInfo[G->mSelectedPixelShader].PartnerShader.end(); ++i)
				fprintf(LogFile, "     visited vertex shader hash = %08lx%08lx\n", (UINT32)(*i >> 32), (UINT32)*i);
		}
		CompiledShaderMap::iterator i = G->mCompiledShaderMap.find(G->mSelectedPixelShader);
		if (i != G->mCompiledShaderMap.end())
		{
			fprintf(LogFile, "       pixel shader was compiled from source code %s\n", i->second);
		}
		i = G->mCompiledShaderMap.find(G->mSelectedVertexShader);
		if (i != G->mCompiledShaderMap.end())
		{
			fprintf(LogFile, "       vertex shader was compiled from source code %s\n", i->second);
		}
		// Copy marked shader to ShaderFixes
		CopyToFixes(G->mSelectedPixelShader, device);
		if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
		if (G->DumpUsage) DumpUsage();
	}
	if (!Action[2]) G->mark_pixelshader = false;

	// Traverse vertex shaders?
	if (Action[7] && !G->next_vertexshader)
	{
		G->next_vertexshader = true;
		if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
		std::set<UINT64>::iterator i = G->mVisitedVertexShaders.find(G->mSelectedVertexShader);
		if (i != G->mVisitedVertexShaders.end() && ++i != G->mVisitedVertexShaders.end())
		{
			G->mSelectedVertexShader = *i;
			G->mSelectedVertexShaderPos++;
			if (LogFile) fprintf(LogFile, "> traversing to next vertex shader #%d. Number of vertex shaders in frame: %d\n", G->mSelectedVertexShaderPos, G->mVisitedVertexShaders.size());
		}
		if (i == G->mVisitedVertexShaders.end() && ++G->mSelectedVertexShaderPos < G->mVisitedVertexShaders.size() && G->mSelectedVertexShaderPos >= 0)
		{
			i = G->mVisitedVertexShaders.begin();
			std::advance(i, G->mSelectedVertexShaderPos);
			G->mSelectedVertexShader = *i;
			if (LogFile) fprintf(LogFile, "> last vertex shader lost. traversing to previous vertex shader #%d. Number of vertex shaders in frame: %d\n", G->mSelectedVertexShaderPos, G->mVisitedVertexShaders.size());
		}
		if (i == G->mVisitedVertexShaders.end() && G->mVisitedVertexShaders.size() != 0)
		{
			G->mSelectedVertexShaderPos = 0;
			if (LogFile) fprintf(LogFile, "> traversing to vertex shader #0. Number of vertex shaders in frame: %d\n", G->mVisitedVertexShaders.size());

			G->mSelectedVertexShader = *G->mVisitedVertexShaders.begin();
		}
		if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
	}
	if (!Action[7]) G->next_vertexshader = false;
	if (Action[8] && !G->prev_vertexshader)
	{
		G->prev_vertexshader = true;
		if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
		std::set<UINT64>::iterator i = G->mVisitedVertexShaders.find(G->mSelectedVertexShader);
		if (i != G->mVisitedVertexShaders.end() && i != G->mVisitedVertexShaders.begin())
		{
			--i;
			G->mSelectedVertexShader = *i;
			G->mSelectedVertexShaderPos--;
			if (LogFile) fprintf(LogFile, "> traversing to previous vertex shader #%d. Number of vertex shaders in frame: %d\n", G->mSelectedVertexShaderPos, G->mVisitedVertexShaders.size());
		}
		if (i == G->mVisitedVertexShaders.end() && --G->mSelectedVertexShaderPos < G->mVisitedVertexShaders.size() && G->mSelectedVertexShaderPos >= 0)
		{
			i = G->mVisitedVertexShaders.begin();
			std::advance(i, G->mSelectedVertexShaderPos);
			G->mSelectedVertexShader = *i;
			if (LogFile) fprintf(LogFile, "> last vertex shader lost. traversing to previous vertex shader #%d. Number of vertex shaders in frame: %d\n", G->mSelectedVertexShaderPos, G->mVisitedVertexShaders.size());
		}
		if (i == G->mVisitedVertexShaders.end() && G->mVisitedVertexShaders.size() != 0)
		{
			G->mSelectedVertexShaderPos = 0;
			if (LogFile) fprintf(LogFile, "> traversing to vertex shader #0. Number of vertex shaders in frame: %d\n", G->mVisitedVertexShaders.size());

			G->mSelectedVertexShader = *G->mVisitedVertexShaders.begin();
		}
		if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
	}
	if (!Action[8]) G->prev_vertexshader = false;
	if (Action[9] && !G->mark_vertexshader)
	{
		G->mark_vertexshader = true;
		if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
		if (LogFile)
		{
			fprintf(LogFile, ">>>> Vertex shader marked: vertex shader hash = %08lx%08lx\n", (UINT32)(G->mSelectedVertexShader >> 32), (UINT32)G->mSelectedVertexShader);
			for (std::set<UINT64>::iterator i = G->mVertexShaderInfo[G->mSelectedVertexShader].PartnerShader.begin(); i != G->mVertexShaderInfo[G->mSelectedVertexShader].PartnerShader.end(); ++i)
				fprintf(LogFile, "     visited pixel shader hash = %08lx%08lx\n", (UINT32)(*i >> 32), (UINT32)*i);
			for (std::set<UINT64>::iterator i = G->mSelectedVertexShader_IndexBuffer.begin(); i != G->mSelectedVertexShader_IndexBuffer.end(); ++i)
				fprintf(LogFile, "     visited index buffer hash = %08lx%08lx\n", (UINT32)(*i >> 32), (UINT32)*i);
		}

		CompiledShaderMap::iterator i = G->mCompiledShaderMap.find(G->mSelectedVertexShader);
		if (i != G->mCompiledShaderMap.end())
		{
			fprintf(LogFile, "       shader was compiled from source code %s\n", i->second);
		}
		// Copy marked shader to ShaderFixes
		CopyToFixes(G->mSelectedVertexShader, device);
		if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
		if (G->DumpUsage) DumpUsage();
	}
	if (!Action[9]) G->mark_vertexshader = false;

	// Traverse render targets?
	if (Action[12] && !G->next_rendertarget)
	{
		G->next_rendertarget = true;
		if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
		std::set<void *>::iterator i = G->mVisitedRenderTargets.find(G->mSelectedRenderTarget);
		if (i != G->mVisitedRenderTargets.end() && ++i != G->mVisitedRenderTargets.end())
		{
			G->mSelectedRenderTarget = *i;
			G->mSelectedRenderTargetPos++;
			if (LogFile) fprintf(LogFile, "> traversing to next render target #%d. Number of render targets in frame: %d\n", G->mSelectedRenderTargetPos, G->mVisitedRenderTargets.size());
		}
		if (i == G->mVisitedRenderTargets.end() && ++G->mSelectedRenderTargetPos < G->mVisitedRenderTargets.size() && G->mSelectedRenderTargetPos >= 0)
		{
			i = G->mVisitedRenderTargets.begin();
			std::advance(i, G->mSelectedRenderTargetPos);
			G->mSelectedRenderTarget = *i;
			if (LogFile) fprintf(LogFile, "> last render target lost. traversing to next render target #%d. Number of render targets frame: %d\n", G->mSelectedRenderTargetPos, G->mVisitedRenderTargets.size());
		}
		if (i == G->mVisitedRenderTargets.end() && G->mVisitedRenderTargets.size() != 0)
		{
			G->mSelectedRenderTargetPos = 0;
			if (LogFile) fprintf(LogFile, "> traversing to render target #0. Number of render targets in frame: %d\n", G->mVisitedRenderTargets.size());

			G->mSelectedRenderTarget = *G->mVisitedRenderTargets.begin();
		}
		if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
	}
	if (!Action[12]) G->next_rendertarget = false;
	if (Action[13] && !G->prev_rendertarget)
	{
		G->prev_rendertarget = true;
		if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
		std::set<void *>::iterator i = G->mVisitedRenderTargets.find(G->mSelectedRenderTarget);
		if (i != G->mVisitedRenderTargets.end() && i != G->mVisitedRenderTargets.begin())
		{
			--i;
			G->mSelectedRenderTarget = *i;
			G->mSelectedRenderTargetPos--;
			if (LogFile) fprintf(LogFile, "> traversing to previous render target #%d. Number of render targets in frame: %d\n", G->mSelectedRenderTargetPos, G->mVisitedRenderTargets.size());
		}
		if (i == G->mVisitedRenderTargets.end() && --G->mSelectedRenderTargetPos < G->mVisitedRenderTargets.size() && G->mSelectedRenderTargetPos >= 0)
		{
			i = G->mVisitedRenderTargets.begin();
			std::advance(i, G->mSelectedRenderTargetPos);
			G->mSelectedRenderTarget = *i;
			if (LogFile) fprintf(LogFile, "> last render target lost. traversing to previous render target #%d. Number of render targets in frame: %d\n", G->mSelectedRenderTargetPos, G->mVisitedRenderTargets.size());
		}
		if (i == G->mVisitedRenderTargets.end() && G->mVisitedRenderTargets.size() != 0)
		{
			G->mSelectedRenderTargetPos = 0;
			if (LogFile) fprintf(LogFile, "> traversing to render target #0. Number of render targets in frame: %d\n", G->mVisitedRenderTargets.size());

			G->mSelectedRenderTarget = *G->mVisitedRenderTargets.begin();
		}
		if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
	}
	if (!Action[13]) G->prev_rendertarget = false;
	if (Action[14] && !G->mark_rendertarget)
	{
		G->mark_rendertarget = true;
		if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
		if (LogFile)
		{
			UINT64 id = G->mRenderTargets[G->mSelectedRenderTarget];
			fprintf(LogFile, ">>>> Render target marked: render target handle = %p, hash = %08lx%08lx\n", G->mSelectedRenderTarget, (UINT32)(id >> 32), (UINT32)id);
			for (std::set<void *>::iterator i = G->mSelectedRenderTargetSnapshotList.begin(); i != G->mSelectedRenderTargetSnapshotList.end(); ++i)
			{
				id = G->mRenderTargets[*i];
				fprintf(LogFile, "       render target handle = %p, hash = %08lx%08lx\n", *i, (UINT32)(id >> 32), (UINT32)id);
			}
		}
		if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);

		if (G->DumpUsage) DumpUsage();
	}
	if (!Action[14]) G->mark_rendertarget = false;

	// Tune value?
	if (Action[10])
	{
		G->gTuneValue1 += G->gTuneStep;
		if (LogFile) fprintf(LogFile, "> Value 1 tuned to %f\n", G->gTuneValue1);
	}
	if (Action[11])
	{
		G->gTuneValue1 -= G->gTuneStep;
		if (LogFile) fprintf(LogFile, "> Value 1 tuned to %f\n", G->gTuneValue1);
	}
	if (Action[15])
	{
		G->gTuneValue2 += G->gTuneStep;
		if (LogFile) fprintf(LogFile, "> Value 2 tuned to %f\n", G->gTuneValue2);
	}
	if (Action[16])
	{
		G->gTuneValue2 -= G->gTuneStep;
		if (LogFile) fprintf(LogFile, "> Value 2 tuned to %f\n", G->gTuneValue2);
	}
	if (Action[17])
	{
		G->gTuneValue3 += G->gTuneStep;
		if (LogFile) fprintf(LogFile, "> Value 3 tuned to %f\n", G->gTuneValue3);
	}
	if (Action[18])
	{
		G->gTuneValue3 -= G->gTuneStep;
		if (LogFile) fprintf(LogFile, "> Value 3 tuned to %f\n", G->gTuneValue3);
	}
	if (Action[19])
	{
		G->gTuneValue4 += G->gTuneStep;
		if (LogFile) fprintf(LogFile, "> Value 4 tuned to %f\n", G->gTuneValue4);
	}
	if (Action[20])
	{
		G->gTuneValue4 -= G->gTuneStep;
		if (LogFile) fprintf(LogFile, "> Value 4 tuned to %f\n", G->gTuneValue4);
	}

	// Clear buffers after some user idle time.  This allows the buffers to be
	// stable during a hunt, and cleared after one minute of idle time.  The idea
	// is to make the arrays of shaders stable so that hunting up and down the arrays
	// is consistent, while the user is engaged.  After 1 minute, they are likely onto
	// some other spot, and we should start with a fresh set, to keep the arrays and
	// active shader list small for easier hunting.  Until the first keypress, the arrays
	// are cleared at each thread wake, just like before. 
	// The arrays will be continually filled by the SetShader sections, but should 
	// rapidly converge upon all active shaders.

	if (difftime(time(NULL), G->huntTime) > 60)
	{
		if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
		G->mVisitedIndexBuffers.clear();
		G->mVisitedVertexShaders.clear();
		G->mVisitedPixelShaders.clear();
		G->mSelectedPixelShader_IndexBuffer.clear();
		G->mSelectedVertexShader_IndexBuffer.clear();
		G->mSelectedIndexBuffer_PixelShader.clear();
		G->mSelectedIndexBuffer_VertexShader.clear();
		for (ShaderIterationMap::iterator i = G->mShaderIterationMap.begin(); i != G->mShaderIterationMap.end(); ++i)
			i->second[0] = 0;
		if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
	}
}


// Todo: straighten out these includes and methods to make more sense.

// For reasons that I do not understand, this method is actually a part of the IDXGIDevice2 object. 
// I think that this is here so that it can get access to the RunFrameActions routine directly,
// because that is not a method, but simply a function.
// Whenever I arrive here in the debugger, the object type of 'this' is IDXGIDevice2, and using the
// expected GetD3D11Device off what I'd expect to be a D3D11Wrapper object, I also get another
// IDXGIDevice2 object as probably an expected original/wrapper.
// So... Pretty sure this method is actually part of IDXGIDevice2, NOT the ID3D11Device as I'd expect.
// This was a problem that took me a couple of days to figure out, because I need the original ID3D11Device
// in order to call CreateVertexShader here.
// As part of that I also looked at the piece here before, taking screen snapshots.  That also does
// not work for the same reason, because the object has a bad stereo texture, because the DXGIDevice2
// does not include that override, so it gets junk out of memory.  Pretty sure.
// More info.
// The way this is defined, it is used in two places.  One for the ID3D11Device and one for the IDXGIDevice2.
// These are not in conflict, but because of the way the includes are set up, this routine is assumed to be
// the base class implementation for IDirect3DUnknown, and thus it is used for both those objects.
// This is bad, because the ID3d11Device is not being accessed, we are only seeing this called from
// IDXGIDevice2. So, 'this' is always a IDXGIDevice2 object.  
// So, I'm fairly sure this is just a bug.
// The current fix is to make a global to store the real ID3D11Device and use that when RunFrameActions
// calls. The correct answer is to split these QueryInterface routines in two, make one for ID3D11Device
// and one for IDXGIDevice2, and make a callback where the IDXGIDevice2 can know how to Present to the
// proper object.

STDMETHODIMP D3D11Wrapper::IDirect3DUnknown::QueryInterface(THIS_ REFIID riid, void** ppvObj)
{
	if (LogFile && LogDebug) fprintf(LogFile, "D3D11Wrapper::IDirect3DUnknown::QueryInterface called at 'this': %s\n", typeid(*this).name());

	IID m1 = { 0x017b2e72ul, 0xbcde, 0x9f15, { 0xa1, 0x2b, 0x3c, 0x4d, 0x5e, 0x6f, 0x70, 0x01 } };
	IID m2 = { 0x017b2e72ul, 0xbcde, 0x9f15, { 0xa1, 0x2b, 0x3c, 0x4d, 0x5e, 0x6f, 0x70, 0x02 } };
	IID m3 = { 0x017b2e72ul, 0xbcde, 0x9f15, { 0xa1, 0x2b, 0x3c, 0x4d, 0x5e, 0x6f, 0x70, 0x03 } };
	if (riid.Data1 == m1.Data1 && riid.Data2 == m1.Data2 && riid.Data3 == m1.Data3 &&
		riid.Data4[0] == m1.Data4[0] && riid.Data4[1] == m1.Data4[1] && riid.Data4[2] == m1.Data4[2] && riid.Data4[3] == m1.Data4[3] &&
		riid.Data4[4] == m1.Data4[4] && riid.Data4[5] == m1.Data4[5] && riid.Data4[6] == m1.Data4[6] && riid.Data4[7] == m1.Data4[7])
	{
		if (LogFile) fprintf(LogFile, "Callback from dxgi.dll wrapper: requesting real ID3D11Device handle from %p\n", *ppvObj);

		D3D11Wrapper::ID3D11Device *p = (D3D11Wrapper::ID3D11Device*) D3D11Wrapper::ID3D11Device::m_List.GetDataPtr(*ppvObj);
		if (p)
		{
			if (LogFile) fprintf(LogFile, "  given pointer was already the real device.\n");
		}
		else
		{
			*ppvObj = ((D3D11Wrapper::ID3D11Device *)*ppvObj)->m_pUnk;
		}
		if (LogFile) fprintf(LogFile, "  returning handle = %p\n", *ppvObj);

		return 0x13bc7e31;
	}
	else if (riid.Data1 == m2.Data1 && riid.Data2 == m2.Data2 && riid.Data3 == m2.Data3 &&
		riid.Data4[0] == m2.Data4[0] && riid.Data4[1] == m2.Data4[1] && riid.Data4[2] == m2.Data4[2] && riid.Data4[3] == m2.Data4[3] &&
		riid.Data4[4] == m2.Data4[4] && riid.Data4[5] == m2.Data4[5] && riid.Data4[6] == m2.Data4[6] && riid.Data4[7] == m2.Data4[7])
	{
		if (LogFile && LogDebug) fprintf(LogFile, "Callback from dxgi.dll wrapper: notification %s received\n", typeid(*ppvObj).name());

		// This callback from DXGI has been disabled, because the DXGI interface does not get called in all games.
		// We have switched to using a similar callback, but from DeviceContext so that we don't need DXGI.
		//switch ((int) *ppvObj)
		//{
		//	case 0:
		//	{
		// Present received.
		// Todo: this cast is wrong. The object is always IDXGIDevice2.
		//				ID3D11Device *device = (ID3D11Device *)this;
		//		RunFrameActions((D3D11Base::ID3D11Device*) realDevice);
		//	break;
		//}
		//}
		return 0x13bc7e31;
	}
	else if (riid.Data1 == m3.Data1 && riid.Data2 == m3.Data2 && riid.Data3 == m3.Data3 &&
		riid.Data4[0] == m3.Data4[0] && riid.Data4[1] == m3.Data4[1] && riid.Data4[2] == m3.Data4[2] && riid.Data4[3] == m3.Data4[3] &&
		riid.Data4[4] == m3.Data4[4] && riid.Data4[5] == m3.Data4[5] && riid.Data4[6] == m3.Data4[6] && riid.Data4[7] == m3.Data4[7])
	{
		SwapChainInfo *info = (SwapChainInfo *)*ppvObj;
		if (LogFile) fprintf(LogFile, "Callback from dxgi.dll wrapper: screen resolution width=%d, height=%d received\n",
			info->width, info->height);

		G->mSwapChainInfo = *info;
		return 0x13bc7e31;
	}

	if (LogFile && LogDebug) fprintf(LogFile, "QueryInterface request for %08lx-%04hx-%04hx-%02hhx%02hhx-%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx on %p\n",
		riid.Data1, riid.Data2, riid.Data3, riid.Data4[0], riid.Data4[1], riid.Data4[2], riid.Data4[3], riid.Data4[4], riid.Data4[5], riid.Data4[6], riid.Data4[7], this);

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
	if (LogFile && LogDebug && d3d10device) fprintf(LogFile, "  9b7e4c0f-342c-4106-a19f-4f2704f689f0 = ID3D10Device\n");
	if (LogFile && LogDebug && d3d10multithread) fprintf(LogFile, "  9b7e4e00-342c-4106-a19f-4f2704f689f0 = ID3D10Multithread\n");
	if (LogFile && LogDebug && dxgidevice) fprintf(LogFile, "  54ec77fa-1377-44e6-8c32-88fd5f44c84c = IDXGIDevice\n");
	if (LogFile && LogDebug && dxgidevice1) fprintf(LogFile, "  77db970f-6276-48ba-ba28-070143b4392c = IDXGIDevice1\n");
	if (LogFile && LogDebug && dxgidevice2) fprintf(LogFile, "  05008617-fbfd-4051-a790-144884b4f6a9 = IDXGIDevice2\n");
	/*
	if (LogFile && unknown1) fprintf(LogFile, "  7abb6563-02bc-47c4-8ef9-acc4795edbcf = undocumented. Forcing fail.\n");
	if (unknown1)
	{
	*ppvObj = 0;
	return E_OUTOFMEMORY;
	}
	*/

	HRESULT hr = m_pUnk->QueryInterface(riid, ppvObj);
	if (hr == S_OK)
	{
		D3D11Wrapper::ID3D11Device *p1 = (D3D11Wrapper::ID3D11Device*) D3D11Wrapper::ID3D11Device::m_List.GetDataPtr(*ppvObj);
		if (p1)
		{
			unsigned long cnt = ((IDirect3DUnknown*)*ppvObj)->Release();
			*ppvObj = p1;
			unsigned long cnt2 = p1->AddRef();
			if (LogFile && LogDebug) fprintf(LogFile, "  interface replaced with ID3D11Device wrapper. Interface counter=%d, wrapper counter=%d, wrapper internal counter = %d\n", cnt, p1->m_ulRef, cnt2);
		}
		D3D11Wrapper::ID3D11DeviceContext *p2 = (D3D11Wrapper::ID3D11DeviceContext*) D3D11Wrapper::ID3D11DeviceContext::m_List.GetDataPtr(*ppvObj);
		if (p2)
		{
			unsigned long cnt = ((IDirect3DUnknown*)*ppvObj)->Release();
			*ppvObj = p2;
			unsigned long cnt2 = p2->AddRef();
			if (LogFile && LogDebug) fprintf(LogFile, "  interface replaced with ID3D11DeviceContext wrapper. Interface counter=%d, wrapper counter=%d, wrapper internal counter = %d\n", cnt, p2->m_ulRef, cnt2);
		}
		D3D11Wrapper::IDXGIDevice2 *p3 = (D3D11Wrapper::IDXGIDevice2*) D3D11Wrapper::IDXGIDevice2::m_List.GetDataPtr(*ppvObj);
		if (p3)
		{
			unsigned long cnt = ((IDirect3DUnknown*)*ppvObj)->Release();
			*ppvObj = p3;
			unsigned long cnt2 = p3->AddRef();
			if (LogFile && LogDebug) fprintf(LogFile, "  interface replaced with IDXGIDevice2 wrapper. Interface counter=%d, wrapper counter=%d, wrapper internal counter = %d\n", cnt, p3->m_ulRef, cnt2);
		}
		D3D11Wrapper::ID3D10Device *p4 = (D3D11Wrapper::ID3D10Device*) D3D11Wrapper::ID3D10Device::m_List.GetDataPtr(*ppvObj);
		if (p4)
		{
			unsigned long cnt = ((IDirect3DUnknown*)*ppvObj)->Release();
			*ppvObj = p4;
			unsigned long cnt2 = p4->AddRef();
			if (LogFile && LogDebug) fprintf(LogFile, "  interface replaced with ID3D10Device wrapper. Interface counter=%d, wrapper counter=%d, wrapper internal counter = %d\n", cnt, p4->m_ulRef, cnt2);
		}
		D3D11Wrapper::ID3D10Multithread *p5 = (D3D11Wrapper::ID3D10Multithread*) D3D11Wrapper::ID3D10Multithread::m_List.GetDataPtr(*ppvObj);
		if (p5)
		{
			unsigned long cnt = ((IDirect3DUnknown*)*ppvObj)->Release();
			*ppvObj = p5;
			unsigned long cnt2 = p5->AddRef();
			if (LogFile && LogDebug) fprintf(LogFile, "  interface replaced with ID3D10Multithread wrapper. Interface counter=%d, wrapper counter=%d\n", cnt, p5->m_ulRef);
		}
		if (!p1 && !p2 && !p3 && !p4 && !p5)
		{
			// Check for IDXGIDevice, IDXGIDevice1 or IDXGIDevice2 cast.
			if (dxgidevice || dxgidevice1 || dxgidevice2)
			{
				// Cast again, but always use IDXGIDevice2 interface.
				D3D11Base::IDXGIDevice *oldDevice = (D3D11Base::IDXGIDevice *)*ppvObj;
				if (LogFile && LogDebug) fprintf(LogFile, "  releasing received IDXGIDevice, handle=%p. Querying IDXGIDevice2 interface.\n", *ppvObj);

				oldDevice->Release();
				const IID IID_IGreet = { 0x7A5E6E81, 0x3DF8, 0x11D3, { 0x90, 0x3D, 0x00, 0x10, 0x5A, 0xA4, 0x5B, 0xDC } };
				const IID IDXGIDevice2 = { 0x05008617, 0xfbfd, 0x4051, { 0xa7, 0x90, 0x14, 0x48, 0x84, 0xb4, 0xf6, 0xa9 } };
				hr = m_pUnk->QueryInterface(IDXGIDevice2, ppvObj);
				if (hr != S_OK)
				{
					if (LogFile) fprintf(LogFile, "  error querying IDXGIDevice2 interface: %x. Trying IDXGIDevice1.\n", hr);

					const IID IDXGIDevice1 = { 0x77db970f, 0x6276, 0x48ba, { 0xba, 0x28, 0x07, 0x01, 0x43, 0xb4, 0x39, 0x2c } };
					hr = m_pUnk->QueryInterface(IDXGIDevice1, ppvObj);
					if (hr != S_OK)
					{
						if (LogFile) fprintf(LogFile, "  error querying IDXGIDevice1 interface: %x. Trying IDXGIDevice.\n", hr);

						const IID IDXGIDevice = { 0x54ec77fa, 0x1377, 0x44e6, { 0x8c, 0x32, 0x88, 0xfd, 0x5f, 0x44, 0xc8, 0x4c } };
						hr = m_pUnk->QueryInterface(IDXGIDevice, ppvObj);
						if (hr != S_OK)
						{
							if (LogFile) fprintf(LogFile, "  fatal error querying IDXGIDevice interface: %x.\n", hr);

							return E_OUTOFMEMORY;
						}
					}
				}
				D3D11Base::IDXGIDevice2 *origDevice = (D3D11Base::IDXGIDevice2 *)*ppvObj;
				D3D11Wrapper::IDXGIDevice2 *wrapper = D3D11Wrapper::IDXGIDevice2::GetDirectDevice2(origDevice);
				if (wrapper == NULL)
				{
					if (LogFile) fprintf(LogFile, "  error allocating IDXGIDevice2 wrapper.\n");

					origDevice->Release();
					return E_OUTOFMEMORY;
				}
				*ppvObj = wrapper;
				if (LogFile && LogDebug) fprintf(LogFile, "  interface replaced with IDXGIDevice2 wrapper, original device handle=%p. Wrapper counter=%d\n",
					origDevice, wrapper->m_ulRef);
			}
			// Check for DirectX10 cast.
			if (d3d10device)
			{
				D3D11Base::ID3D10Device *origDevice = (D3D11Base::ID3D10Device *)*ppvObj;
				D3D11Wrapper::ID3D10Device *wrapper = D3D11Wrapper::ID3D10Device::GetDirect3DDevice(origDevice);
				if (wrapper == NULL)
				{
					if (LogFile) fprintf(LogFile, "  error allocating ID3D10Device wrapper.\n");

					origDevice->Release();
					return E_OUTOFMEMORY;
				}
				*ppvObj = wrapper;
				if (LogFile && LogDebug) fprintf(LogFile, "  interface replaced with ID3D10Device wrapper. Wrapper counter=%d\n", wrapper->m_ulRef);
			}
			if (d3d10multithread)
			{
				D3D11Base::ID3D10Multithread *origDevice = (D3D11Base::ID3D10Multithread *)*ppvObj;
				D3D11Wrapper::ID3D10Multithread *wrapper = D3D11Wrapper::ID3D10Multithread::GetDirect3DMultithread(origDevice);
				if (wrapper == NULL)
				{
					if (LogFile) fprintf(LogFile, "  error allocating ID3D10Multithread wrapper.\n");

					origDevice->Release();
					return E_OUTOFMEMORY;
				}
				*ppvObj = wrapper;
				if (LogFile && LogDebug) fprintf(LogFile, "  interface replaced with ID3D10Multithread wrapper. Wrapper counter=%d\n", wrapper->m_ulRef);
			}
		}
	}
	if (LogFile && LogDebug) fprintf(LogFile, "  result = %x, handle = %p\n", hr, *ppvObj);

	return hr;
}

static D3D11Base::IDXGIAdapter *ReplaceAdapter(D3D11Base::IDXGIAdapter *wrapper)
{
	if (!wrapper)
		return wrapper;
	if (LogFile) fprintf(LogFile, "  checking for adapter wrapper, handle = %p\n", wrapper);
	IID marker = { 0x017b2e72ul, 0xbcde, 0x9f15, { 0xa1, 0x2b, 0x3c, 0x4d, 0x5e, 0x6f, 0x70, 0x00 } };
	D3D11Base::IDXGIAdapter *realAdapter;
	if (wrapper->GetParent(marker, (void **)&realAdapter) == 0x13bc7e32)
	{
		if (LogFile) fprintf(LogFile, "    wrapper found. replacing with original handle = %p\n", realAdapter);

		// Register adapter.
		G->m_AdapterList.AddMember(realAdapter, wrapper);
		return realAdapter;
	}
	return wrapper;
}

HRESULT WINAPI D3D11CreateDevice(
	D3D11Base::IDXGIAdapter *pAdapter,
	D3D11Base::D3D_DRIVER_TYPE DriverType,
	HMODULE Software,
	UINT Flags,
	const D3D11Base::D3D_FEATURE_LEVEL *pFeatureLevels,
	UINT FeatureLevels,
	UINT SDKVersion,
	D3D11Wrapper::ID3D11Device **ppDevice,
	D3D11Base::D3D_FEATURE_LEVEL *pFeatureLevel,
	D3D11Wrapper::ID3D11DeviceContext **ppImmediateContext)
{
	InitD311();
	if (LogFile) fprintf(LogFile, "D3D11CreateDevice called with adapter = %p\n", pAdapter);

#if defined(_DEBUG_LAYER)
	// If the project is in a debug build, enable the debug layer.
	Flags |= D3D11Base::D3D11_CREATE_DEVICE_DEBUG;
	Flags |= D3D11Base::D3D11_CREATE_DEVICE_PREVENT_INTERNAL_THREADING_OPTIMIZATIONS;
#endif

	D3D11Base::ID3D11Device *origDevice = 0;
	D3D11Base::ID3D11DeviceContext *origContext = 0;
	EnableStereo();
	HRESULT ret = (*_D3D11CreateDevice)(ReplaceAdapter(pAdapter), DriverType, Software, Flags, pFeatureLevels,
		FeatureLevels, SDKVersion, &origDevice, pFeatureLevel, &origContext);

	// ret from D3D11CreateDevice has the same problem as CreateDeviceAndSwapChain, in that it can return
	// a value that S_FALSE, which is a positive number.  It's not an error exactly, but it's not S_OK.
	// The best check here is for FAILED instead, to allow benign errors to continue.
	if (FAILED(ret))
	{
		if (LogFile) fprintf(LogFile, "  failed with HRESULT=%x\n", ret);

		return ret;
	}

	D3D11Wrapper::ID3D11Device *wrapper = D3D11Wrapper::ID3D11Device::GetDirect3DDevice(origDevice);
	if (wrapper == NULL)
	{
		if (LogFile) fprintf(LogFile, "  error allocating wrapper.\n");

		origDevice->Release();
		origContext->Release();

		return E_OUTOFMEMORY;
	}
	if (ppDevice)
		*ppDevice = wrapper;

	D3D11Wrapper::ID3D11DeviceContext *wrapper2 = D3D11Wrapper::ID3D11DeviceContext::GetDirect3DDeviceContext(origContext);
	if (wrapper2 == NULL)
	{
		if (LogFile) fprintf(LogFile, "  error allocating wrapper2.\n");

		origDevice->Release();
		origContext->Release();
		return E_OUTOFMEMORY;
	}
	if (ppImmediateContext)
		*ppImmediateContext = wrapper2;

	if (LogFile) fprintf(LogFile, "  returns result = %x, device handle = %p, device wrapper = %p, context handle = %p, context wrapper = %p\n", ret, origDevice, wrapper, origContext, wrapper2);

	return ret;
}

HRESULT WINAPI D3D11CreateDeviceAndSwapChain(
	D3D11Base::IDXGIAdapter *pAdapter,
	D3D11Base::D3D_DRIVER_TYPE DriverType,
	HMODULE Software,
	UINT Flags,
	const D3D11Base::D3D_FEATURE_LEVEL *pFeatureLevels,
	UINT FeatureLevels,
	UINT SDKVersion,
	D3D11Base::DXGI_SWAP_CHAIN_DESC *pSwapChainDesc,
	D3D11Base::IDXGISwapChain **ppSwapChain,
	D3D11Wrapper::ID3D11Device **ppDevice,
	D3D11Base::D3D_FEATURE_LEVEL *pFeatureLevel,
	D3D11Wrapper::ID3D11DeviceContext **ppImmediateContext)
{
	InitD311();
	if (LogFile) fprintf(LogFile, "D3D11CreateDeviceAndSwapChain called with adapter = %p\n", pAdapter);
	if (LogFile && pSwapChainDesc) fprintf(LogFile, "  Windowed = %d\n", pSwapChainDesc->Windowed);
	if (LogFile && pSwapChainDesc) fprintf(LogFile, "  Width = %d\n", pSwapChainDesc->BufferDesc.Width);
	if (LogFile && pSwapChainDesc) fprintf(LogFile, "  Height = %d\n", pSwapChainDesc->BufferDesc.Height);
	if (LogFile && pSwapChainDesc) fprintf(LogFile, "  Refresh rate = %f\n",
		(float)pSwapChainDesc->BufferDesc.RefreshRate.Numerator / (float)pSwapChainDesc->BufferDesc.RefreshRate.Denominator);

	if (G->SCREEN_FULLSCREEN >= 0 && pSwapChainDesc) pSwapChainDesc->Windowed = !G->SCREEN_FULLSCREEN;
	if (G->SCREEN_REFRESH >= 0 && pSwapChainDesc && !pSwapChainDesc->Windowed)
	{
		pSwapChainDesc->BufferDesc.RefreshRate.Numerator = G->SCREEN_REFRESH;
		pSwapChainDesc->BufferDesc.RefreshRate.Denominator = 1;
	}
	if (G->SCREEN_WIDTH >= 0 && pSwapChainDesc) pSwapChainDesc->BufferDesc.Width = G->SCREEN_WIDTH;
	if (G->SCREEN_HEIGHT >= 0 && pSwapChainDesc) pSwapChainDesc->BufferDesc.Height = G->SCREEN_HEIGHT;
	if (LogFile && pSwapChainDesc) fprintf(LogFile, "  calling with parameters width = %d, height = %d, refresh rate = %f, windowed = %d\n",
		pSwapChainDesc->BufferDesc.Width, pSwapChainDesc->BufferDesc.Height,
		(float)pSwapChainDesc->BufferDesc.RefreshRate.Numerator / (float)pSwapChainDesc->BufferDesc.RefreshRate.Denominator,
		pSwapChainDesc->Windowed);

	EnableStereo();

#if defined(_DEBUG_LAYER)
	// If the project is in a debug build, enable the debug layer.
	Flags |= D3D11Base::D3D11_CREATE_DEVICE_DEBUG;
	Flags |= D3D11Base::D3D11_CREATE_DEVICE_PREVENT_INTERNAL_THREADING_OPTIMIZATIONS;
#endif

	D3D11Base::ID3D11Device *origDevice = 0;
	D3D11Base::ID3D11DeviceContext *origContext = 0;
	HRESULT ret = (*_D3D11CreateDeviceAndSwapChain)(ReplaceAdapter(pAdapter), DriverType, Software, Flags, pFeatureLevels,
		FeatureLevels, SDKVersion, pSwapChainDesc, ppSwapChain, &origDevice, pFeatureLevel, &origContext);

	// Changed to recognize that >0 DXGISTATUS values are possible, not just S_OK.
	if (FAILED(ret))
	{
		if (LogFile) fprintf(LogFile, "  failed with HRESULT=%x\n", ret);
		return ret;
	}

	if (LogFile) fprintf(LogFile, "  CreateDeviceAndSwapChain returned device handle = %p, context handle = %p\n", origDevice, origContext);

	if (!origDevice || !origContext)
		return ret;

	D3D11Wrapper::ID3D11Device *wrapper = D3D11Wrapper::ID3D11Device::GetDirect3DDevice(origDevice);
	if (wrapper == NULL)
	{
		if (LogFile) fprintf(LogFile, "  error allocating wrapper.\n");

		origDevice->Release();
		origContext->Release();
		return E_OUTOFMEMORY;
	}
	if (ppDevice)
		*ppDevice = wrapper;

	D3D11Wrapper::ID3D11DeviceContext *wrapper2 = D3D11Wrapper::ID3D11DeviceContext::GetDirect3DDeviceContext(origContext);
	if (wrapper2 == NULL)
	{
		if (LogFile) fprintf(LogFile, "  error allocating wrapper2.\n");

		origDevice->Release();
		origContext->Release();
		return E_OUTOFMEMORY;
	}
	if (ppImmediateContext)
		*ppImmediateContext = wrapper2;

	if (LogFile) fprintf(LogFile, "  returns result = %x, device handle = %p, device wrapper = %p, context handle = %p, context wrapper = %p\n", ret, origDevice, wrapper, origContext, wrapper2);

	return ret;
}

int WINAPI D3DKMTGetDeviceState(int a)
{
	InitD311();
	if (LogFile) fprintf(LogFile, "D3DKMTGetDeviceState called.\n");

	return (*_D3DKMTGetDeviceState)(a);
}

int WINAPI D3DKMTOpenAdapterFromHdc(int a)
{
	InitD311();
	if (LogFile) fprintf(LogFile, "D3DKMTOpenAdapterFromHdc called.\n");

	return (*_D3DKMTOpenAdapterFromHdc)(a);
}

int WINAPI D3DKMTOpenResource(int a)
{
	InitD311();
	if (LogFile) fprintf(LogFile, "D3DKMTOpenResource called.\n");

	return (*_D3DKMTOpenResource)(a);
}

int WINAPI D3DKMTQueryResourceInfo(int a)
{
	InitD311();
	if (LogFile) fprintf(LogFile, "D3DKMTQueryResourceInfo called.\n");

	return (*_D3DKMTQueryResourceInfo)(a);
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

static void NvAPIOverride()
{
	// Override custom settings.
	const D3D11Base::StereoHandle id1 = (D3D11Base::StereoHandle)0x77aa8ebc;
	float id2 = 1.23f;
	if (D3D11Base::NvAPI_Stereo_GetConvergence(id1, &id2) != 0xeecc34ab)
	{
		if (LogFile && LogDebug) fprintf(LogFile, "  overriding NVAPI wrapper failed.\n");
	}
}

#include "Direct3D11Device.h"
#include "Direct3D11Context.h"
#include "DirectDXGIDevice.h"
#include "../DirectX10/Direct3D10Device.h"
