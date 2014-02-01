#include "Main.h"
#include <Shlobj.h>
#include <Winuser.h>
#include "../DirectInput.h"
#include <map>
#include <vector>
#include <set>
#include <iterator>

FILE *LogFile = 0;
static wchar_t DLL_PATH[MAX_PATH] = { 0 };
static bool gInitialized = false;
const int MARKING_MODE_SKIP = 0;
const int MARKING_MODE_MONO = 1;
const int MARKING_MODE_ORIGINAL = 2;
const int MARKING_MODE_ZERO = 3;
bool LogInput = false, LogDebug = false;

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

	bool take_screenshot;
	bool next_pixelshader, prev_pixelshader, mark_pixelshader;
	bool next_vertexshader, prev_vertexshader, mark_vertexshader;
	bool next_indexbuffer, prev_indexbuffer, mark_indexbuffer;
	bool next_rendertarget, prev_rendertarget, mark_rendertarget;
	bool EXPORT_ALL, EXPORT_HLSL, EXPORT_BINARY, EXPORT_FIXED, CACHE_SHADERS, PRELOAD_SHADERS, SCISSOR_DISABLE;
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

	SwapChainInfo mSwapChainInfo;

	ThreadSafePointerSet m_AdapterList;
	CRITICAL_SECTION mCriticalSection;
	bool ENABLE_CRITICAL_SECTION;

	DataBufferMap mDataBuffers;
	UINT64 mCurrentIndexBuffer;
	std::set<UINT64> mVisitedIndexBuffers;
	UINT64 mSelectedIndexBuffer;
	int mSelectedIndexBufferPos;
	std::set<UINT64> mSelectedIndexBuffer_VertexShader;
	std::set<UINT64> mSelectedIndexBuffer_PixelShader;

	CompiledShaderMap mCompiledShaderMap;

	PreloadVertexShaderMap mPreloadedVertexShaders;
	VertexShaderMap mVertexShaders;
	VertexShaderReplacementMap mOriginalVertexShaders;
	VertexShaderReplacementMap mZeroVertexShaders;
	UINT64 mCurrentVertexShader;
	std::set<UINT64> mVisitedVertexShaders;
	UINT64 mSelectedVertexShader;
	int mSelectedVertexShaderPos;
	std::set<UINT64> mSelectedVertexShader_IndexBuffer;

	PreloadPixelShaderMap mPreloadedPixelShaders;
	PixelShaderMap mPixelShaders;
	PixelShaderReplacementMap mOriginalPixelShaders;
	PixelShaderReplacementMap mZeroPixelShaders;
	UINT64 mCurrentPixelShader;
	std::set<UINT64> mVisitedPixelShaders;
	UINT64 mSelectedPixelShader;
	int mSelectedPixelShaderPos;
	std::set<UINT64> mSelectedPixelShader_IndexBuffer;

	GeometryShaderMap mGeometryShaders;
	ComputeShaderMap mComputeShaders;
	DomainShaderMap mDomainShaders;
	HullShaderMap mHullShaders;

	// Separation override for shader.
	ShaderSeparationMap mShaderSeparationMap;
	ShaderIterationMap mShaderIterationMap;
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
	int mSelectedRenderTargetPos;
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
		take_screenshot(false),
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
		EXPORT_ALL(false), 
		EXPORT_HLSL(false), 
		EXPORT_BINARY(false), 
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
		ENABLE_CRITICAL_SECTION(false),
		SCREEN_WIDTH(-1),
		SCREEN_HEIGHT(-1),
		SCREEN_REFRESH(-1),
		SCREEN_FULLSCREEN(-1),
		marking_mode(0),
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

static char *readStringParameter(wchar_t *val)
{
	static char buf[MAX_PATH];
	wcstombs(buf, val, MAX_PATH);
	char *end = buf + strlen(buf) - 1; while (end > buf && isspace(*end)) end--; *(end+1) = 0;
	char *start = buf; while (isspace(*start)) start++;
	return start;
}

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
		if (LogFile) LogFile = fopen("d3d11_log.txt", "w");
		LogInput = GetPrivateProfileInt(L"Logging", L"input", 0, dir);
		LogDebug = GetPrivateProfileInt(L"Logging", L"debug", 0, dir);
		wchar_t val[MAX_PATH];
		int read = GetPrivateProfileString(L"Device", L"width", 0, val, MAX_PATH, dir);
		if (read) swscanf(val, L"%d", &G->SCREEN_WIDTH);
		read = GetPrivateProfileString(L"Device", L"height", 0, val, MAX_PATH, dir);
		if (read) swscanf(val, L"%d", &G->SCREEN_HEIGHT);
		read = GetPrivateProfileString(L"Device", L"refresh_rate", 0, val, MAX_PATH, dir);
		if (read) swscanf(val, L"%d", &G->SCREEN_REFRESH);
		read = GetPrivateProfileString(L"Device", L"full_screen", 0, val, MAX_PATH, dir);
		if (read) swscanf(val, L"%d", &G->SCREEN_FULLSCREEN);
		G->gForceStereo = GetPrivateProfileInt(L"Device", L"force_stereo", 0, dir);
		G->gCreateStereoProfile = GetPrivateProfileInt(L"Stereo", L"create_profile", 0, dir);
		G->gSurfaceCreateMode = GetPrivateProfileInt(L"Stereo", L"surface_createmode", -1, dir);
		G->gSurfaceSquareCreateMode = GetPrivateProfileInt(L"Stereo", L"surface_square_createmode", -1, dir);
		read = GetPrivateProfileString(L"System", L"proxy_d3d11", 0, DLL_PATH, MAX_PATH, dir);

		// Shader
		read = GetPrivateProfileString(L"Rendering", L"override_directory", 0, SHADER_PATH, MAX_PATH, dir);
		if (SHADER_PATH[0])
		{
			while (SHADER_PATH[wcslen(SHADER_PATH)-1] == L' ')
				SHADER_PATH[wcslen(SHADER_PATH)-1] = 0;
			if (SHADER_PATH[1] != ':' && SHADER_PATH[0] != '\\')
			{
				GetModuleFileName(0, val, MAX_PATH);
				wcsrchr(val, L'\\')[1] = 0;
				wcscat(val, SHADER_PATH);
				wcscpy(SHADER_PATH, val);
			}
			// Create directory?
			CreateDirectory(SHADER_PATH, 0);
		}
		read = GetPrivateProfileString(L"Rendering", L"cache_directory", 0, SHADER_CACHE_PATH, MAX_PATH, dir);
		if (SHADER_CACHE_PATH[0])
		{
			while (SHADER_CACHE_PATH[wcslen(SHADER_CACHE_PATH)-1] == L' ')
				SHADER_CACHE_PATH[wcslen(SHADER_CACHE_PATH)-1] = 0;
			if (SHADER_CACHE_PATH[1] != ':' && SHADER_CACHE_PATH[0] != '\\')
			{
				GetModuleFileName(0, val, MAX_PATH);
				wcsrchr(val, L'\\')[1] = 0;
				wcscat(val, SHADER_CACHE_PATH);
				wcscpy(SHADER_CACHE_PATH, val);
			}
			// Create directory?
			CreateDirectory(SHADER_CACHE_PATH, 0);
		}
		G->ENABLE_CRITICAL_SECTION = GetPrivateProfileInt(L"Rendering", L"use_criticalsection", 0, dir);
		G->EXPORT_ALL = GetPrivateProfileInt(L"Rendering", L"export_shaders", 0, dir);
		G->CACHE_SHADERS = GetPrivateProfileInt(L"Rendering", L"cache_shaders", 0, dir);
		G->PRELOAD_SHADERS = GetPrivateProfileInt(L"Rendering", L"preload_shaders", 0, dir);
		G->EXPORT_HLSL = GetPrivateProfileInt(L"Rendering", L"export_hlsl", 0, dir);
		G->EXPORT_BINARY = GetPrivateProfileInt(L"Rendering", L"export_binary", 0, dir);
		G->EXPORT_FIXED = GetPrivateProfileInt(L"Rendering", L"export_fixed", 0, dir);
		G->FIX_SV_Position = GetPrivateProfileInt(L"Rendering", L"fix_sv_position", 0, dir);
		G->FIX_Light_Position = GetPrivateProfileInt(L"Rendering", L"fix_light_position", 0, dir);
		G->FIX_Recompile_VS = GetPrivateProfileInt(L"Rendering", L"recompile_all_vs", 0, dir);
		G->DumpUsage = GetPrivateProfileInt(L"Rendering", L"dump_usage", 0, dir);
		G->SCISSOR_DISABLE = GetPrivateProfileInt(L"Rendering", L"rasterizer_disable_scissor", 0, dir);
		G->ENABLE_TUNE = GetPrivateProfileInt(L"Rendering", L"tune_enable", 0, dir);
		read = GetPrivateProfileString(L"Rendering", L"tune_step", 0, val, MAX_PATH, dir);
		if (read) swscanf(val, L"%f", &G->gTuneStep);
		read = GetPrivateProfileString(L"Rendering", L"marking_mode", 0, val, MAX_PATH, dir);
		if (read && !wcscmp(val, L"skip")) G->marking_mode = MARKING_MODE_SKIP;
		if (read && !wcscmp(val, L"mono")) G->marking_mode = MARKING_MODE_MONO;
		if (read && !wcscmp(val, L"original")) G->marking_mode = MARKING_MODE_ORIGINAL;
		if (read && !wcscmp(val, L"zero")) G->marking_mode = MARKING_MODE_ZERO;
		read = GetPrivateProfileString(L"Rendering", L"fix_ZRepair_DepthTexture1", 0, val, MAX_PATH, dir);
		if (read)
		{
			char buf[MAX_PATH];
			wcstombs(buf, val, MAX_PATH);
			char *end = buf + strlen(buf) - 1; while (end > buf && isspace(*end)) end--; *(end+1) = 0;
			G->ZRepair_DepthTextureReg1 = *end; *(end-1) = 0;
			char *start = buf; while (isspace(*start)) start++;
			G->ZRepair_DepthTexture1 = start;
		}
		read = GetPrivateProfileString(L"Rendering", L"fix_ZRepair_DepthTexture2", 0, val, MAX_PATH, dir);
		if (read)
		{
			char buf[MAX_PATH];
			wcstombs(buf, val, MAX_PATH);
			char *end = buf + strlen(buf) - 1; while (end > buf && isspace(*end)) end--; *(end+1) = 0;
			G->ZRepair_DepthTextureReg2 = *end; *(end-1) = 0;
			char *start = buf; while (isspace(*start)) start++;
			G->ZRepair_DepthTexture2 = start;
		}
		read = GetPrivateProfileString(L"Rendering", L"fix_ZRepair_ZPosCalc1", 0, val, MAX_PATH, dir);
		if (read) G->ZRepair_ZPosCalc1 = readStringParameter(val);
		read = GetPrivateProfileString(L"Rendering", L"fix_ZRepair_ZPosCalc2", 0, val, MAX_PATH, dir);
		if (read) G->ZRepair_ZPosCalc2 = readStringParameter(val);
		read = GetPrivateProfileString(L"Rendering", L"fix_ZRepair_PositionTexture", 0, val, MAX_PATH, dir);
		if (read) G->ZRepair_PositionTexture = readStringParameter(val);
		read = GetPrivateProfileString(L"Rendering", L"fix_ZRepair_PositionCalc", 0, val, MAX_PATH, dir);
		if (read) G->ZRepair_WorldPosCalc = readStringParameter(val);
		read = GetPrivateProfileString(L"Rendering", L"fix_ZRepair_Dependencies1", 0, val, MAX_PATH, dir);
		if (read)
		{
			char buf[MAX_PATH];
			wcstombs(buf, val, MAX_PATH);
			char *start = buf; while (isspace(*start)) ++start;
			while (*start)
			{
				char *end = start; while (*end != ',' && *end && *end != ' ') ++end;
				G->ZRepair_Dependencies1.push_back(string(start, end));
				start = end; if (*start == ',') ++start;
			}
		}
		read = GetPrivateProfileString(L"Rendering", L"fix_ZRepair_Dependencies2", 0, val, MAX_PATH, dir);
		if (read)
		{
			char buf[MAX_PATH];
			wcstombs(buf, val, MAX_PATH);
			char *start = buf; while (isspace(*start)) ++start;
			while (*start)
			{
				char *end = start; while (*end != ',' && *end && *end != ' ') ++end;
				G->ZRepair_Dependencies2.push_back(string(start, end));
				start = end; if (*start == ',') ++start;
			}
		}
		read = GetPrivateProfileString(L"Rendering", L"fix_InvTransform", 0, val, MAX_PATH, dir);
		if (read)
		{
			char buf[MAX_PATH];
			wcstombs(buf, val, MAX_PATH);
			char *start = buf; while (isspace(*start)) ++start;
			while (*start)
			{
				char *end = start; while (*end != ',' && *end && *end != ' ') ++end;
				G->InvTransforms.push_back(string(start, end));
				start = end; if (*start == ',') ++start;
			}
		}
		read = GetPrivateProfileString(L"Rendering", L"fix_ZRepair_DepthTextureHash", 0, val, MAX_PATH, dir);
		if (read)
		{
			unsigned long hashHi, hashLo;
			swscanf(val, L"%08lx%08lx", &hashHi, &hashLo);
			G->ZBufferHashToInject = (UINT64(hashHi) << 32) | UINT64(hashLo);
		}
		read = GetPrivateProfileString(L"Rendering", L"fix_BackProjectionTransform1", 0, val, MAX_PATH, dir);
		if (read) G->BackProject_Vector1 = readStringParameter(val);
		read = GetPrivateProfileString(L"Rendering", L"fix_BackProjectionTransform2", 0, val, MAX_PATH, dir);
		if (read) G->BackProject_Vector2 = readStringParameter(val);
		read = GetPrivateProfileString(L"Rendering", L"fix_ObjectPosition1", 0, val, MAX_PATH, dir);
		if (read) G->ObjectPos_ID1 = readStringParameter(val);
		read = GetPrivateProfileString(L"Rendering", L"fix_ObjectPosition2", 0, val, MAX_PATH, dir);
		if (read) G->ObjectPos_ID2 = readStringParameter(val);
		read = GetPrivateProfileString(L"Rendering", L"fix_ObjectPosition1Multiplier", 0, val, MAX_PATH, dir);
		if (read) G->ObjectPos_MUL1 = readStringParameter(val);
		read = GetPrivateProfileString(L"Rendering", L"fix_ObjectPosition2Multiplier", 0, val, MAX_PATH, dir);
		if (read) G->ObjectPos_MUL2 = readStringParameter(val);
		read = GetPrivateProfileString(L"Rendering", L"fix_MatrixOperand1", 0, val, MAX_PATH, dir);
		if (read) G->MatrixPos_ID1 = readStringParameter(val);
		read = GetPrivateProfileString(L"Rendering", L"fix_MatrixOperand1Multiplier", 0, val, MAX_PATH, dir);
		if (read) G->MatrixPos_MUL1 = readStringParameter(val);

		// DirectInput
		InputDevice[0] = 0;
		GetPrivateProfileString(L"Rendering", L"Input", 0, InputDevice, MAX_PATH, dir);
		wchar_t *end = InputDevice + wcslen(InputDevice) - 1; while (end > InputDevice && isspace(*end)) end--; *(end+1) = 0;
		InputDeviceId = GetPrivateProfileInt(L"Rendering", L"DeviceNr", -1, dir);
		// Actions
		GetPrivateProfileString(L"Rendering", L"next_pixelshader", 0, InputAction[0], MAX_PATH, dir);
		end = InputAction[0] + wcslen(InputAction[0]) - 1; while (end > InputAction[0] && isspace(*end)) end--; *(end+1) = 0;
		GetPrivateProfileString(L"Rendering", L"previous_pixelshader", 0, InputAction[1], MAX_PATH, dir);
		end = InputAction[1] + wcslen(InputAction[1]) - 1; while (end > InputAction[1] && isspace(*end)) end--; *(end+1) = 0;
		GetPrivateProfileString(L"Rendering", L"mark_pixelshader", 0, InputAction[2], MAX_PATH, dir);
		end = InputAction[2] + wcslen(InputAction[2]) - 1; while (end > InputAction[2] && isspace(*end)) end--; *(end+1) = 0;
		GetPrivateProfileString(L"Rendering", L"take_screenshot", 0, InputAction[3], MAX_PATH, dir);
		end = InputAction[3] + wcslen(InputAction[3]) - 1; while (end > InputAction[3] && isspace(*end)) end--; *(end+1) = 0;
		GetPrivateProfileString(L"Rendering", L"next_indexbuffer", 0, InputAction[4], MAX_PATH, dir);
		end = InputAction[4] + wcslen(InputAction[4]) - 1; while (end > InputAction[4] && isspace(*end)) end--; *(end+1) = 0;
		GetPrivateProfileString(L"Rendering", L"previous_indexbuffer", 0, InputAction[5], MAX_PATH, dir);
		end = InputAction[5] + wcslen(InputAction[5]) - 1; while (end > InputAction[5] && isspace(*end)) end--; *(end+1) = 0;
		GetPrivateProfileString(L"Rendering", L"mark_indexbuffer", 0, InputAction[6], MAX_PATH, dir);
		end = InputAction[6] + wcslen(InputAction[6]) - 1; while (end > InputAction[6] && isspace(*end)) end--; *(end+1) = 0;
		GetPrivateProfileString(L"Rendering", L"next_vertexshader", 0, InputAction[7], MAX_PATH, dir);
		end = InputAction[7] + wcslen(InputAction[7]) - 1; while (end > InputAction[7] && isspace(*end)) end--; *(end+1) = 0;
		GetPrivateProfileString(L"Rendering", L"previous_vertexshader", 0, InputAction[8], MAX_PATH, dir);
		end = InputAction[8] + wcslen(InputAction[8]) - 1; while (end > InputAction[8] && isspace(*end)) end--; *(end+1) = 0;
		GetPrivateProfileString(L"Rendering", L"mark_vertexshader", 0, InputAction[9], MAX_PATH, dir);
		end = InputAction[9] + wcslen(InputAction[9]) - 1; while (end > InputAction[9] && isspace(*end)) end--; *(end+1) = 0;
		GetPrivateProfileString(L"Rendering", L"tune1_up", 0, InputAction[10], MAX_PATH, dir);
		end = InputAction[10] + wcslen(InputAction[10]) - 1; while (end > InputAction[10] && isspace(*end)) end--; *(end+1) = 0;
		GetPrivateProfileString(L"Rendering", L"tune1_down", 0, InputAction[11], MAX_PATH, dir);
		end = InputAction[11] + wcslen(InputAction[11]) - 1; while (end > InputAction[11] && isspace(*end)) end--; *(end+1) = 0;
		GetPrivateProfileString(L"Rendering", L"next_rendertarget", 0, InputAction[12], MAX_PATH, dir);
		end = InputAction[12] + wcslen(InputAction[12]) - 1; while (end > InputAction[12] && isspace(*end)) end--; *(end+1) = 0;
		GetPrivateProfileString(L"Rendering", L"previous_rendertarget", 0, InputAction[13], MAX_PATH, dir);
		end = InputAction[13] + wcslen(InputAction[13]) - 1; while (end > InputAction[13] && isspace(*end)) end--; *(end+1) = 0;
		GetPrivateProfileString(L"Rendering", L"mark_rendertarget", 0, InputAction[14], MAX_PATH, dir);
		end = InputAction[14] + wcslen(InputAction[14]) - 1; while (end > InputAction[14] && isspace(*end)) end--; *(end+1) = 0;
		GetPrivateProfileString(L"Rendering", L"tune2_up", 0, InputAction[15], MAX_PATH, dir);
		end = InputAction[15] + wcslen(InputAction[15]) - 1; while (end > InputAction[15] && isspace(*end)) end--; *(end+1) = 0;
		GetPrivateProfileString(L"Rendering", L"tune2_down", 0, InputAction[16], MAX_PATH, dir);
		end = InputAction[16] + wcslen(InputAction[16]) - 1; while (end > InputAction[16] && isspace(*end)) end--; *(end+1) = 0;
		GetPrivateProfileString(L"Rendering", L"tune3_up", 0, InputAction[17], MAX_PATH, dir);
		end = InputAction[17] + wcslen(InputAction[17]) - 1; while (end > InputAction[17] && isspace(*end)) end--; *(end+1) = 0;
		GetPrivateProfileString(L"Rendering", L"tune3_down", 0, InputAction[18], MAX_PATH, dir);
		end = InputAction[18] + wcslen(InputAction[18]) - 1; while (end > InputAction[18] && isspace(*end)) end--; *(end+1) = 0;
		GetPrivateProfileString(L"Rendering", L"tune4_up", 0, InputAction[19], MAX_PATH, dir);
		end = InputAction[19] + wcslen(InputAction[19]) - 1; while (end > InputAction[19] && isspace(*end)) end--; *(end+1) = 0;
		GetPrivateProfileString(L"Rendering", L"tune4_down", 0, InputAction[20], MAX_PATH, dir);
		end = InputAction[20] + wcslen(InputAction[20]) - 1; while (end > InputAction[20] && isspace(*end)) end--; *(end+1) = 0;
		InitDirectInput();
		// XInput
		XInputDeviceId = GetPrivateProfileInt(L"Rendering", L"XInputDevice", -1, dir);				
		// Shader separation overrides.
		for (int i = 1;; ++i)
		{
			wchar_t id[] = L"ShaderOverridexxx", val[MAX_PATH];
			_itow(i, id+14, 10);
			int read = GetPrivateProfileString(id, L"Hash", 0, val, MAX_PATH, dir);
			if (!read) break;
			unsigned long hashHi, hashLo;
			swscanf(val, L"%08lx%08lx", &hashHi, &hashLo);
			read = GetPrivateProfileString(id, L"Separation", 0, val, MAX_PATH, dir);
			if (read)
			{
				float separation;
				swscanf(val, L"%e", &separation);
				G->mShaderSeparationMap[(UINT64(hashHi) << 32) | UINT64(hashLo)] = separation;
				if (LogFile && LogDebug) fprintf(LogFile, "[ShaderOverride] Shader = %08lx%08lx, separation = %f\n", hashHi, hashLo, separation);
			}
			read = GetPrivateProfileString(id, L"Handling", 0, val, MAX_PATH, dir);
			if (read && !wcscmp(val, L"skip"))
				G->mShaderSeparationMap[(UINT64(hashHi) << 32) | UINT64(hashLo)] = 10000;
			read = GetPrivateProfileString(id, L"Iteration", 0, val, MAX_PATH, dir);
			if (read)
			{
				int iteration;
				std::vector<int> iterations;
				iterations.push_back(0);
				swscanf(val, L"%d", &iteration);
				iterations.push_back(iteration);
				G->mShaderIterationMap[(UINT64(hashHi) << 32) | UINT64(hashLo)] = iterations;
			}
			read = GetPrivateProfileString(id, L"IndexBufferFilter", 0, val, MAX_PATH, dir);
			if (read)
			{
				unsigned long hashHi2, hashLo2;
				swscanf(val, L"%08lx%08lx", &hashHi2, &hashLo2);
				G->mShaderIndexBufferFilter[(UINT64(hashHi) << 32) | UINT64(hashLo)].push_back((UINT64(hashHi2) << 32) | UINT64(hashLo2));
			}
		}
		// Texture overrides.
		for (int i = 1;; ++i)
		{
			wchar_t id[] = L"TextureOverridexxx", val[MAX_PATH];
			_itow(i, id+15, 10);
			int read = GetPrivateProfileString(id, L"Hash", 0, val, MAX_PATH, dir);
			if (!read) break;
			unsigned long hashHi, hashLo;
			swscanf(val, L"%08lx%08lx", &hashHi, &hashLo);
			int stereoMode = GetPrivateProfileInt(id, L"StereoMode", -1, dir);
			if (stereoMode >= 0)
			{
				G->mTextureStereoMap[(UINT64(hashHi) << 32) | UINT64(hashLo)] = stereoMode;
				if (LogFile && LogDebug) fprintf(LogFile, "[TextureOverride] Texture = %08lx%08lx, stereo mode = %d\n", hashHi, hashLo, stereoMode);
			}
			int texFormat = GetPrivateProfileInt(id, L"Format", -1, dir);
			if (texFormat >= 0)
			{
				G->mTextureTypeMap[(UINT64(hashHi) << 32) | UINT64(hashLo)] = texFormat;
				if (LogFile && LogDebug) fprintf(LogFile, "[TextureOverride] Texture = %08lx%08lx, format = %d\n", hashHi, hashLo, texFormat);
			}
			read = GetPrivateProfileString(id, L"Iteration", 0, val, MAX_PATH, dir);
			if (read)
			{
				std::vector<int> iterations;
				iterations.push_back(0);
				int id[10] = { 0,0,0,0,0,0,0,0,0,0 };
				swscanf(val, L"%d,%d,%d,%d,%d,%d,%d,%d,%d,%d", id+0,id+1,id+2,id+3,id+4,id+5,id+6,id+7,id+8,id+9);
				for (int j = 0; j < 10; ++j)
				{
					if (id[j] <= 0) break;
					iterations.push_back(id[j]);
				}
				G->mTextureIterationMap[(UINT64(hashHi) << 32) | UINT64(hashLo)] = iterations;
				if (LogFile && LogDebug) fprintf(LogFile, "[TextureOverride] Texture = %08lx%08lx, iterations = %d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n", hashHi, hashLo,
					id[0],id[1],id[2],id[3],id[4],id[5],id[6],id[7],id[8],id[9]);
			}
		}
		// NVAPI
		D3D11Base::NvAPI_Initialize();
		InitializeCriticalSection(&G->mCriticalSection);

		if (LogFile) fprintf(LogFile, "DLL initialized.\n");
		if (LogFile && LogDebug) fprintf(LogFile, "[Rendering] XInputDevice = %d\n", XInputDeviceId);
		if (LogFile) fflush(LogFile);
	}
}

void DestroyDLL()
{
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
	if (LogFile && LogDebug) fflush(LogFile);
	return 0; 
}
int WINAPI D3DKMTDestroyAllocation() 
{ 
	if (LogFile && LogDebug) fprintf(LogFile, "D3DKMTDestroyAllocation called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	return 0; 
}
int WINAPI D3DKMTDestroyContext() 
{ 
	if (LogFile && LogDebug) fprintf(LogFile, "D3DKMTDestroyContext called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	return 0; 
}
int WINAPI D3DKMTDestroyDevice() 
{ 
	if (LogFile && LogDebug) fprintf(LogFile, "D3DKMTDestroyDevice called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	return 0; 
}
int WINAPI D3DKMTDestroySynchronizationObject() 
{ 
	if (LogFile && LogDebug) fprintf(LogFile, "D3DKMTDestroySynchronizationObject called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	return 0; 
}
int WINAPI D3DKMTSetDisplayPrivateDriverFormat() 
{ 
	if (LogFile && LogDebug) fprintf(LogFile, "D3DKMTSetDisplayPrivateDriverFormat called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	return 0; 
}
int WINAPI D3DKMTSignalSynchronizationObject() 
{ 
	if (LogFile && LogDebug) fprintf(LogFile, "D3DKMTSignalSynchronizationObject called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	return 0; 
}
int WINAPI D3DKMTUnlock() 
{ 
	if (LogFile && LogDebug) fprintf(LogFile, "D3DKMTUnlock called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	return 0; 
}
int WINAPI D3DKMTWaitForSynchronizationObject() 
{ 
	if (LogFile && LogDebug) fprintf(LogFile, "D3DKMTWaitForSynchronizationObject called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	return 0; 
}
int WINAPI D3DKMTCreateAllocation() 
{ 
	if (LogFile && LogDebug) fprintf(LogFile, "D3DKMTCreateAllocation called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	return 0; 
}
int WINAPI D3DKMTCreateContext() 
{ 
	if (LogFile && LogDebug) fprintf(LogFile, "D3DKMTCreateContext called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	return 0; 
}
int WINAPI D3DKMTCreateDevice() 
{ 
	if (LogFile && LogDebug) fprintf(LogFile, "D3DKMTCreateDevice called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	return 0; 
}
int WINAPI D3DKMTCreateSynchronizationObject() 
{ 
	if (LogFile && LogDebug) fprintf(LogFile, "D3DKMTCreateSynchronizationObject called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	return 0; 
}
int WINAPI D3DKMTEscape() 
{ 
	if (LogFile && LogDebug) fprintf(LogFile, "D3DKMTEscape called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	return 0; 
}
int WINAPI D3DKMTGetContextSchedulingPriority() 
{ 
	if (LogFile && LogDebug) fprintf(LogFile, "D3DKMTGetContextSchedulingPriority called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	return 0; 
}
int WINAPI D3DKMTGetDisplayModeList() 
{ 
	if (LogFile && LogDebug) fprintf(LogFile, "D3DKMTGetDisplayModeList called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	return 0; 
}
int WINAPI D3DKMTGetMultisampleMethodList() 
{ 
	if (LogFile && LogDebug) fprintf(LogFile, "D3DKMTGetMultisampleMethodList called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	return 0; 
}
int WINAPI D3DKMTGetRuntimeData() 
{ 
	if (LogFile && LogDebug) fprintf(LogFile, "D3DKMTGetRuntimeData called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	return 0; 
}
int WINAPI D3DKMTGetSharedPrimaryHandle() 
{ 
	if (LogFile && LogDebug) fprintf(LogFile, "D3DKMTGetRuntimeData called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	return 0; 
}
int WINAPI D3DKMTLock() 
{ 
	if (LogFile && LogDebug) fprintf(LogFile, "D3DKMTLock called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	return 0; 
}
int WINAPI D3DKMTPresent() 
{ 
	if (LogFile && LogDebug) fprintf(LogFile, "D3DKMTPresent called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	return 0; 
}
int WINAPI D3DKMTQueryAllocationResidency() 
{ 
	if (LogFile && LogDebug) fprintf(LogFile, "D3DKMTQueryAllocationResidency called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	return 0; 
}
int WINAPI D3DKMTRender() 
{ 
	if (LogFile && LogDebug) fprintf(LogFile, "D3DKMTRender called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	return 0; 
}
int WINAPI D3DKMTSetAllocationPriority() 
{ 
	if (LogFile && LogDebug) fprintf(LogFile, "D3DKMTSetAllocationPriority called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	return 0; 
}
int WINAPI D3DKMTSetContextSchedulingPriority() 
{ 
	if (LogFile && LogDebug) fprintf(LogFile, "D3DKMTSetContextSchedulingPriority called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	return 0; 
}
int WINAPI D3DKMTSetDisplayMode() 
{ 
	if (LogFile && LogDebug) fprintf(LogFile, "D3DKMTSetDisplayMode called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	return 0; 
}
int WINAPI D3DKMTSetGammaRamp() 
{ 
	if (LogFile && LogDebug) fprintf(LogFile, "D3DKMTSetGammaRamp called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
	return 0; 
}
int WINAPI D3DKMTSetVidPnSourceOwner() 
{ 
	if (LogFile && LogDebug) fprintf(LogFile, "D3DKMTSetVidPnSourceOwner called.\n");
	if (LogFile && LogDebug) fflush(LogFile);
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
typedef int (WINAPI *tD3D11CoreCreateLayeredDevice)(int a, int b, int c, int d, int e);
static tD3D11CoreCreateLayeredDevice _D3D11CoreCreateLayeredDevice;
typedef int (WINAPI *tD3D11CoreGetLayeredDeviceSize)(int a, int b);
static tD3D11CoreGetLayeredDeviceSize _D3D11CoreGetLayeredDeviceSize;
typedef int (WINAPI *tD3D11CoreRegisterLayers)(int a, int b);
static tD3D11CoreRegisterLayers _D3D11CoreRegisterLayers;
typedef HRESULT (WINAPI *tD3D11CreateDevice)(
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
typedef HRESULT (WINAPI *tD3D11CreateDeviceAndSwapChain)(
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
		wcscat(sysDir, L"\\d3d11.dll");
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
		if (LogFile) fflush(LogFile);
		return;
	}

	_D3DKMTQueryAdapterInfo = (tD3DKMTQueryAdapterInfo) GetProcAddress(hD3D11, "D3DKMTQueryAdapterInfo");
	_OpenAdapter10 = (tOpenAdapter10) GetProcAddress(hD3D11, "OpenAdapter10");
	_OpenAdapter10_2 = (tOpenAdapter10_2) GetProcAddress(hD3D11, "OpenAdapter10_2");
	_D3D11CoreCreateDevice = (tD3D11CoreCreateDevice) GetProcAddress(hD3D11, "D3D11CoreCreateDevice");
	_D3D11CoreCreateLayeredDevice = (tD3D11CoreCreateLayeredDevice) GetProcAddress(hD3D11, "D3D11CoreCreateLayeredDevice");
	_D3D11CoreGetLayeredDeviceSize = (tD3D11CoreGetLayeredDeviceSize) GetProcAddress(hD3D11, "D3D11CoreGetLayeredDeviceSize");
	_D3D11CoreRegisterLayers = (tD3D11CoreRegisterLayers) GetProcAddress(hD3D11, "D3D11CoreRegisterLayers");
	_D3D11CreateDevice = (tD3D11CreateDevice) GetProcAddress(hD3D11, "D3D11CreateDevice");
	_D3D11CreateDeviceAndSwapChain = (tD3D11CreateDeviceAndSwapChain) GetProcAddress(hD3D11, "D3D11CreateDeviceAndSwapChain");
	_D3DKMTGetDeviceState = (tD3DKMTGetDeviceState) GetProcAddress(hD3D11, "D3DKMTGetDeviceState");
	_D3DKMTOpenAdapterFromHdc = (tD3DKMTOpenAdapterFromHdc) GetProcAddress(hD3D11, "D3DKMTOpenAdapterFromHdc");
	_D3DKMTOpenResource = (tD3DKMTOpenResource) GetProcAddress(hD3D11, "D3DKMTOpenResource");
	_D3DKMTQueryResourceInfo = (tD3DKMTQueryResourceInfo) GetProcAddress(hD3D11, "D3DKMTQueryResourceInfo");
}

int WINAPI D3DKMTQueryAdapterInfo(_D3DKMT_QUERYADAPTERINFO *info)
{
	InitD311();
    if (LogFile) fprintf(LogFile, "D3DKMTQueryAdapterInfo called.\n");
    if (LogFile) fflush(LogFile);
	return (*_D3DKMTQueryAdapterInfo)(info);
}

int WINAPI OpenAdapter10(struct D3D10DDIARG_OPENADAPTER *adapter)
{
	InitD311();
    if (LogFile) fprintf(LogFile, "OpenAdapter10 called.\n");
    if (LogFile) fflush(LogFile);
	return (*_OpenAdapter10)(adapter);
}

int WINAPI OpenAdapter10_2(struct D3D10DDIARG_OPENADAPTER *adapter)
{
	InitD311();
    if (LogFile) fprintf(LogFile, "OpenAdapter10_2 called.\n");
    if (LogFile) fflush(LogFile);
	return (*_OpenAdapter10_2)(adapter);
}

int WINAPI D3D11CoreCreateDevice(__int32 a, int b, int c, LPCSTR lpModuleName, int e, int f, int g, int h, int i, int j)
{
	InitD311();
    if (LogFile) fprintf(LogFile, "D3D11CoreCreateDevice called.\n");
    if (LogFile) fflush(LogFile);
	return (*_D3D11CoreCreateDevice)(a, b, c, lpModuleName, e, f, g, h, i, j);
}

int WINAPI D3D11CoreCreateLayeredDevice(int a, int b, int c, int d, int e)
{
	InitD311();
    if (LogFile) fprintf(LogFile, "D3D11CoreCreateLayeredDevice called.\n");
    if (LogFile) fflush(LogFile);
	return (*_D3D11CoreCreateLayeredDevice)(a, b, c, d, e);
}

int WINAPI D3D11CoreGetLayeredDeviceSize(int a, int b)
{
	InitD311();
	// Call from D3DCompiler ?
	if (a == 0x77aa128b)
	{
	    if (LogFile) fprintf(LogFile, "Shader code info from D3DCompiler_xx.dll wrapper received:\n");
	    if (LogFile) fflush(LogFile);
		D3D11BridgeData *data = (D3D11BridgeData *)b;
	    if (LogFile) fprintf(LogFile, "  Bytecode hash = %08lx%08lx\n", (UINT32)(data->BinaryHash >> 32), (UINT32)data->BinaryHash);
	    if (LogFile) fprintf(LogFile, "  Filename = %s\n", data->HLSLFileName);
	    if (LogFile) fflush(LogFile);
		G->mCompiledShaderMap[data->BinaryHash] = data->HLSLFileName;
		return 0xaa77125b;
	}
    if (LogFile) fprintf(LogFile, "D3D11CoreGetLayeredDeviceSize called.\n");
    if (LogFile) fflush(LogFile);
	return (*_D3D11CoreGetLayeredDeviceSize)(a, b);
}

int WINAPI D3D11CoreRegisterLayers(int a, int b)
{
	InitD311();
    if (LogFile) fprintf(LogFile, "D3D11CoreRegisterLayers called.\n");
    if (LogFile) fflush(LogFile);
	return (*_D3D11CoreRegisterLayers)(a, b);
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
		if ( status != D3D11Base::NVAPI_OK)
		{
			// GeForce Stereoscopic 3D driver is not installed on the system
			if (LogFile) fprintf(LogFile, "  stereo init failed: no stereo driver detected.\n");		
		}
		// Stereo is available but not enabled, let's enable it
		else if(D3D11Base::NVAPI_OK == status && !isStereoEnabled)
		{
			if (LogFile) fprintf(LogFile, "  stereo available and disabled. Enabling stereo.\n");		
			status = D3D11Base::NvAPI_Stereo_Enable();
			if (status != D3D11Base::NVAPI_OK)
				if (LogFile) fprintf(LogFile, "    enabling stereo failed.\n");		
		}

		if (G->gCreateStereoProfile)
		{
			if (LogFile) fprintf(LogFile, "  enabling registry profile.\n");		
			if (LogFile) fflush(LogFile);
			D3D11Base::NvAPI_Stereo_CreateConfigurationProfileRegistryKey(D3D11Base::NVAPI_STEREO_DEFAULT_REGISTRY_PROFILE);
		}
	}
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
			sprintf(buf, "<VertexShader hash=\"%08lx%08lx\">\n"
						 "  <CalledPixelShaders>", (UINT32)(i->first >> 32), (UINT32)i->first);
			WriteFile(f, buf, strlen(buf), &written, 0);
			std::set<UINT64>::iterator j;
			for (j = i->second.PartnerShader.begin(); j != i->second.PartnerShader.end(); ++j)
			{
				sprintf(buf, "%08lx%08lx ", (UINT32)(*j >> 32), (UINT32)*j);
				WriteFile(f, buf, strlen(buf), &written, 0);
			}
			const char *REG_HEADER = "</CalledPixelShaders>\n";
			WriteFile(f, REG_HEADER, strlen(REG_HEADER), &written, 0);
			std::map<int, void *>::iterator k;
			for (k = i->second.ResourceRegisters.begin(); k != i->second.ResourceRegisters.end(); ++k)
				if (k->second)
				{
					UINT64 id = G->mRenderTargets[k->second];
					sprintf(buf, "  <Register id=%d handle=%08lx>%08lx%08lx</Register>\n", k->first, k->second, (UINT32)(id >> 32), (UINT32)id);
					WriteFile(f, buf, strlen(buf), &written, 0);
				}
			const char *FOOTER = "</VertexShader>\n";
			WriteFile(f, FOOTER, strlen(FOOTER), &written, 0);
		}
		for (i = G->mPixelShaderInfo.begin(); i != G->mPixelShaderInfo.end(); ++i)
		{
			sprintf(buf, "<PixelShader hash=\"%08lx%08lx\">\n"
						 "  <ParentVertexShaders>", (UINT32)(i->first >> 32), (UINT32)i->first);
			WriteFile(f, buf, strlen(buf), &written, 0);
			std::set<UINT64>::iterator j;
			for (j = i->second.PartnerShader.begin(); j != i->second.PartnerShader.end(); ++j)
			{
				sprintf(buf, "%08lx%08lx ", (UINT32)(*j >> 32), (UINT32)*j);
				WriteFile(f, buf, strlen(buf), &written, 0);
			}
			const char *REG_HEADER = "</ParentVertexShaders>\n";
			WriteFile(f, REG_HEADER, strlen(REG_HEADER), &written, 0);
			std::map<int, void *>::iterator k;
			for (k = i->second.ResourceRegisters.begin(); k != i->second.ResourceRegisters.end(); ++k)
				if (k->second)
				{
					UINT64 id = G->mRenderTargets[k->second];
					sprintf(buf, "  <Register id=%d handle=%08lx>%08lx%08lx</Register>\n", k->first, k->second, (UINT32)(id >> 32), (UINT32)id);
					WriteFile(f, buf, strlen(buf), &written, 0);
				}
			std::vector<void *>::iterator m;
			int pos = 0;
			for (m = i->second.RenderTargets.begin(); m != i->second.RenderTargets.end(); ++m)
			{
				UINT64 id = G->mRenderTargets[*m];
				sprintf(buf, "  <RenderTarget id=%d handle=%08lx>%08lx%08lx</RenderTarget>\n", pos, *m, (UINT32)(id >> 32), (UINT32)id);
				WriteFile(f, buf, strlen(buf), &written, 0);
				++pos;
			}
			const char *FOOTER = "</PixelShader>\n";
			WriteFile(f, FOOTER, strlen(FOOTER), &written, 0);
		}
		if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
		CloseHandle(f);
	}
	else
	{
		if (LogFile) fprintf(LogFile, "Error dumping ShaderUsage.txt\n");
		if (LogFile) fflush(LogFile);
	}
}

static void RunFrameActions(D3D11Wrapper::ID3D11Device *device)
{
	if (LogFile && LogDebug) fprintf(LogFile, "Running frame actions\n");
	if (LogFile && LogDebug) fflush(LogFile);

	UpdateInputState();

	// Screenshot?
	if (Action[3] && !G->take_screenshot)
	{
		if (LogFile) fprintf(LogFile, "> capturing screenshot\n");
		if (LogFile) fflush(LogFile);
		G->take_screenshot = true;
		if (device->mStereoHandle)
			D3D11Base::NvAPI_Stereo_CapturePngImage(device->mStereoHandle);
	}
	if (!Action[3]) G->take_screenshot = false;

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
			if (LogFile) fflush(LogFile);
		}
		if (i == G->mVisitedIndexBuffers.end() && ++G->mSelectedIndexBufferPos < G->mVisitedIndexBuffers.size() && G->mSelectedIndexBufferPos >= 0)
		{
			i = G->mVisitedIndexBuffers.begin();
			std::advance(i, G->mSelectedIndexBufferPos);
			G->mSelectedIndexBuffer = *i;			
			if (LogFile) fprintf(LogFile, "> last index buffer lost. traversing to next index buffer #%d. Number of index buffers in frame: %d\n", G->mSelectedIndexBufferPos, G->mVisitedIndexBuffers.size());
			if (LogFile) fflush(LogFile);
		}
		if (i == G->mVisitedIndexBuffers.end() && G->mVisitedIndexBuffers.size() != 0)
		{
			G->mSelectedIndexBufferPos = 0;
			if (LogFile) fprintf(LogFile, "> traversing to index buffer #0. Number of index buffers in frame: %d\n", G->mVisitedIndexBuffers.size());
			if (LogFile) fflush(LogFile);
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
			if (LogFile) fflush(LogFile);
		}
		if (i == G->mVisitedIndexBuffers.end() && --G->mSelectedIndexBufferPos < G->mVisitedIndexBuffers.size() && G->mSelectedIndexBufferPos >= 0)
		{
			i = G->mVisitedIndexBuffers.begin();
			std::advance(i, G->mSelectedIndexBufferPos);
			G->mSelectedIndexBuffer = *i;			
			if (LogFile) fprintf(LogFile, "> last index buffer lost. traversing to previous index buffer #%d. Number of index buffers in frame: %d\n", G->mSelectedIndexBufferPos, G->mVisitedIndexBuffers.size());
			if (LogFile) fflush(LogFile);
		}
		if (i == G->mVisitedIndexBuffers.end() && G->mVisitedIndexBuffers.size() != 0)
		{
			G->mSelectedIndexBufferPos = 0;
			if (LogFile) fprintf(LogFile, "> traversing to index buffer #0. Number of index buffers in frame: %d\n", G->mVisitedIndexBuffers.size());
			if (LogFile) fflush(LogFile);
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
			fflush(LogFile);
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
			if (LogFile) fflush(LogFile);
		}
		if (i == G->mVisitedPixelShaders.end() && ++G->mSelectedPixelShaderPos < G->mVisitedPixelShaders.size() && G->mSelectedPixelShaderPos >= 0)
		{
			i = G->mVisitedPixelShaders.begin();
			std::advance(i, G->mSelectedPixelShaderPos);
			G->mSelectedPixelShader = *i;			
			if (LogFile) fprintf(LogFile, "> last pixel shader lost. traversing to next pixel shader #%d. Number of pixel shaders in frame: %d\n", G->mSelectedPixelShaderPos, G->mVisitedPixelShaders.size());
			if (LogFile) fflush(LogFile);
		}
		if (i == G->mVisitedPixelShaders.end() && G->mVisitedPixelShaders.size() != 0)
		{
			G->mSelectedPixelShaderPos = 0;
			if (LogFile) fprintf(LogFile, "> traversing to pixel shader #0. Number of pxiel shaders in frame: %d\n", G->mVisitedPixelShaders.size());
			if (LogFile) fflush(LogFile);
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
			if (LogFile) fflush(LogFile);
		}
		if (i == G->mVisitedPixelShaders.end() && --G->mSelectedPixelShaderPos < G->mVisitedPixelShaders.size() && G->mSelectedPixelShaderPos >= 0)
		{
			i = G->mVisitedPixelShaders.begin();
			std::advance(i, G->mSelectedPixelShaderPos);
			G->mSelectedPixelShader = *i;
			if (LogFile) fprintf(LogFile, "> last pixel shader lost. traversing to previous pixel shader #%d. Number of pixel shaders in frame: %d\n", G->mSelectedPixelShaderPos, G->mVisitedPixelShaders.size());
			if (LogFile) fflush(LogFile);
		}
		if (i == G->mVisitedPixelShaders.end() && G->mVisitedPixelShaders.size() != 0)
		{
			G->mSelectedPixelShaderPos = 0;
			if (LogFile) fprintf(LogFile, "> traversing to pixel shader #0. Number of pixel shaders in frame: %d\n", G->mVisitedPixelShaders.size());
			if (LogFile) fflush(LogFile);
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
			fflush(LogFile);
		}
		CompiledShaderMap::iterator i = G->mCompiledShaderMap.find(G->mSelectedPixelShader);
		if (i != G->mCompiledShaderMap.end())
		{
			fprintf(LogFile, "       pixel shader was compiled from source code %s\n", i->second);
			if (LogFile) fflush(LogFile);
		}
		i = G->mCompiledShaderMap.find(G->mSelectedVertexShader);
		if (i != G->mCompiledShaderMap.end())
		{
			fprintf(LogFile, "       vertex shader was compiled from source code %s\n", i->second);
			if (LogFile) fflush(LogFile);
		}
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
			if (LogFile) fflush(LogFile);
		}
		if (i == G->mVisitedVertexShaders.end() && ++G->mSelectedVertexShaderPos < G->mVisitedVertexShaders.size() && G->mSelectedVertexShaderPos >= 0)
		{
			i = G->mVisitedVertexShaders.begin();
			std::advance(i, G->mSelectedVertexShaderPos);
			G->mSelectedVertexShader = *i;
			if (LogFile) fprintf(LogFile, "> last vertex shader lost. traversing to previous vertex shader #%d. Number of vertex shaders in frame: %d\n", G->mSelectedVertexShaderPos, G->mVisitedVertexShaders.size());
			if (LogFile) fflush(LogFile);
		}
		if (i == G->mVisitedVertexShaders.end() && G->mVisitedVertexShaders.size() != 0)
		{
			G->mSelectedVertexShaderPos = 0;
			if (LogFile) fprintf(LogFile, "> traversing to vertex shader #0. Number of vertex shaders in frame: %d\n", G->mVisitedVertexShaders.size());
			if (LogFile) fflush(LogFile);
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
			if (LogFile) fflush(LogFile);
		}
		if (i == G->mVisitedVertexShaders.end() && --G->mSelectedVertexShaderPos < G->mVisitedVertexShaders.size() && G->mSelectedVertexShaderPos >= 0)
		{
			i = G->mVisitedVertexShaders.begin();
			std::advance(i, G->mSelectedVertexShaderPos);
			G->mSelectedVertexShader = *i;
			if (LogFile) fprintf(LogFile, "> last vertex shader lost. traversing to previous vertex shader #%d. Number of vertex shaders in frame: %d\n", G->mSelectedVertexShaderPos, G->mVisitedVertexShaders.size());
			if (LogFile) fflush(LogFile);
		}
		if (i == G->mVisitedVertexShaders.end() && G->mVisitedVertexShaders.size() != 0)
		{
			G->mSelectedVertexShaderPos = 0;
			if (LogFile) fprintf(LogFile, "> traversing to vertex shader #0. Number of vertex shaders in frame: %d\n", G->mVisitedVertexShaders.size());
			if (LogFile) fflush(LogFile);
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
			fflush(LogFile);
		}
		if (LogFile) fflush(LogFile);
		CompiledShaderMap::iterator i = G->mCompiledShaderMap.find(G->mSelectedVertexShader);
		if (i != G->mCompiledShaderMap.end())
		{
			fprintf(LogFile, "       shader was compiled from source code %s\n", i->second);
			if (LogFile) fflush(LogFile);
		}
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
			if (LogFile) fflush(LogFile);
		}
		if (i == G->mVisitedRenderTargets.end() && ++G->mSelectedRenderTargetPos < G->mVisitedRenderTargets.size() && G->mSelectedRenderTargetPos >= 0)
		{
			i = G->mVisitedRenderTargets.begin();
			std::advance(i, G->mSelectedRenderTargetPos);
			G->mSelectedRenderTarget = *i;
			if (LogFile) fprintf(LogFile, "> last render target lost. traversing to next render target #%d. Number of render targets frame: %d\n", G->mSelectedRenderTargetPos, G->mVisitedRenderTargets.size());
			if (LogFile) fflush(LogFile);
		}
		if (i == G->mVisitedRenderTargets.end() && G->mVisitedRenderTargets.size() != 0)
		{
			G->mSelectedRenderTargetPos = 0;
			if (LogFile) fprintf(LogFile, "> traversing to render target #0. Number of render targets in frame: %d\n", G->mVisitedRenderTargets.size());
			if (LogFile) fflush(LogFile);
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
			if (LogFile) fflush(LogFile);
		}
		if (i == G->mVisitedRenderTargets.end() && --G->mSelectedRenderTargetPos < G->mVisitedRenderTargets.size() && G->mSelectedRenderTargetPos >= 0)
		{
			i = G->mVisitedRenderTargets.begin();
			std::advance(i, G->mSelectedRenderTargetPos);
			G->mSelectedRenderTarget = *i;
			if (LogFile) fprintf(LogFile, "> last render target lost. traversing to previous render target #%d. Number of render targets in frame: %d\n", G->mSelectedRenderTargetPos, G->mVisitedRenderTargets.size());
			if (LogFile) fflush(LogFile);
		}
		if (i == G->mVisitedRenderTargets.end() && G->mVisitedRenderTargets.size() != 0)
		{
			G->mSelectedRenderTargetPos = 0;
			if (LogFile) fprintf(LogFile, "> traversing to render target #0. Number of render targets in frame: %d\n", G->mVisitedRenderTargets.size());
			if (LogFile) fflush(LogFile);
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
			fprintf(LogFile, ">>>> Render target marked: render target handle = %08lx, hash = %08lx%08lx\n", G->mSelectedRenderTarget, (UINT32)(id >> 32), (UINT32)id);
			for (std::set<void *>::iterator i = G->mSelectedRenderTargetSnapshotList.begin(); i != G->mSelectedRenderTargetSnapshotList.end(); ++i)
			{
				id = G->mRenderTargets[*i];
				fprintf(LogFile, "       render target handle = %08lx, hash = %08lx%08lx\n", *i, (UINT32)(id >> 32), (UINT32)id);
			}
		}
		if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
		if (LogFile) fflush(LogFile);
		if (G->DumpUsage) DumpUsage();
	}
	if (!Action[14]) G->mark_rendertarget = false;

	// Tune value?
	if (Action[10])
	{
		G->gTuneValue1 += G->gTuneStep;
		if (LogFile) fprintf(LogFile, "> Value 1 tuned to %f\n", G->gTuneValue1);
		if (LogFile) fflush(LogFile);
	}
	if (Action[11]) 
	{
		G->gTuneValue1 -= G->gTuneStep;
		if (LogFile) fprintf(LogFile, "> Value 1 tuned to %f\n", G->gTuneValue1);
		if (LogFile) fflush(LogFile);
	}
	if (Action[15])
	{
		G->gTuneValue2 += G->gTuneStep;
		if (LogFile) fprintf(LogFile, "> Value 2 tuned to %f\n", G->gTuneValue2);
		if (LogFile) fflush(LogFile);
	}
	if (Action[16]) 
	{
		G->gTuneValue2 -= G->gTuneStep;
		if (LogFile) fprintf(LogFile, "> Value 2 tuned to %f\n", G->gTuneValue2);
		if (LogFile) fflush(LogFile);
	}
	if (Action[17])
	{
		G->gTuneValue3 += G->gTuneStep;
		if (LogFile) fprintf(LogFile, "> Value 3 tuned to %f\n", G->gTuneValue3);
		if (LogFile) fflush(LogFile);
	}
	if (Action[18]) 
	{
		G->gTuneValue3 -= G->gTuneStep;
		if (LogFile) fprintf(LogFile, "> Value 3 tuned to %f\n", G->gTuneValue3);
		if (LogFile) fflush(LogFile);
	}
	if (Action[19])
	{
		G->gTuneValue4 += G->gTuneStep;
		if (LogFile) fprintf(LogFile, "> Value 4 tuned to %f\n", G->gTuneValue4);
		if (LogFile) fflush(LogFile);
	}
	if (Action[20]) 
	{
		G->gTuneValue4 -= G->gTuneStep;
		if (LogFile) fprintf(LogFile, "> Value 4 tuned to %f\n", G->gTuneValue4);
		if (LogFile) fflush(LogFile);
	}

	// Clear buffers.
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

STDMETHODIMP D3D11Wrapper::IDirect3DUnknown::QueryInterface(THIS_ REFIID riid, void** ppvObj)
{
	IID m1 = { 0x017b2e72ul, 0xbcde, 0x9f15, { 0xa1, 0x2b, 0x3c, 0x4d, 0x5e, 0x6f, 0x70, 0x01 } };
	IID m2 = { 0x017b2e72ul, 0xbcde, 0x9f15, { 0xa1, 0x2b, 0x3c, 0x4d, 0x5e, 0x6f, 0x70, 0x02 } };
	IID m3 = { 0x017b2e72ul, 0xbcde, 0x9f15, { 0xa1, 0x2b, 0x3c, 0x4d, 0x5e, 0x6f, 0x70, 0x03 } };
	if (riid.Data1 == m1.Data1 && riid.Data2 == m1.Data2 && riid.Data3 == m1.Data3 && 
		riid.Data4[0] == m1.Data4[0] && riid.Data4[1] == m1.Data4[1] && riid.Data4[2] == m1.Data4[2] && riid.Data4[3] == m1.Data4[3] && 
		riid.Data4[4] == m1.Data4[4] && riid.Data4[5] == m1.Data4[5] && riid.Data4[6] == m1.Data4[6] && riid.Data4[7] == m1.Data4[7])
	{
		if (LogFile) fprintf(LogFile, "Callback from dxgi.dll wrapper: requesting real ID3D11Device handle from %x\n", *ppvObj);
		if (LogFile) fflush(LogFile);
	    D3D11Wrapper::ID3D11Device *p = (D3D11Wrapper::ID3D11Device*) D3D11Wrapper::ID3D11Device::m_List.GetDataPtr(*ppvObj);
		if (p)
		{
			if (LogFile) fprintf(LogFile, "  given pointer was already the real device.\n");
			if (LogFile) fflush(LogFile);
		}
		else
		{
			*ppvObj = ((D3D11Wrapper::ID3D11Device *)*ppvObj)->m_pUnk;
		}
		if (LogFile) fprintf(LogFile, "  returning handle = %x\n", *ppvObj);
		if (LogFile) fflush(LogFile);
		return 0x13bc7e31;
	}
	else if (riid.Data1 == m2.Data1 && riid.Data2 == m2.Data2 && riid.Data3 == m2.Data3 && 
		riid.Data4[0] == m2.Data4[0] && riid.Data4[1] == m2.Data4[1] && riid.Data4[2] == m2.Data4[2] && riid.Data4[3] == m2.Data4[3] && 
		riid.Data4[4] == m2.Data4[4] && riid.Data4[5] == m2.Data4[5] && riid.Data4[6] == m2.Data4[6] && riid.Data4[7] == m2.Data4[7])
	{
		if (LogFile && LogDebug) fprintf(LogFile, "Callback from dxgi.dll wrapper: notification #%d received\n", (int) *ppvObj);
		if (LogFile && LogDebug) fflush(LogFile);
		switch ((int) *ppvObj)
		{
			case 0:
			{
				// Present received.
				ID3D11Device *device = (ID3D11Device *)this;
				RunFrameActions(device);
				break;
			}
		}
		return 0x13bc7e31;
	}
	else if (riid.Data1 == m3.Data1 && riid.Data2 == m3.Data2 && riid.Data3 == m3.Data3 && 
		riid.Data4[0] == m3.Data4[0] && riid.Data4[1] == m3.Data4[1] && riid.Data4[2] == m3.Data4[2] && riid.Data4[3] == m3.Data4[3] && 
		riid.Data4[4] == m3.Data4[4] && riid.Data4[5] == m3.Data4[5] && riid.Data4[6] == m3.Data4[6] && riid.Data4[7] == m3.Data4[7])
	{
		SwapChainInfo *info = (SwapChainInfo *)*ppvObj;
		if (LogFile) fprintf(LogFile, "Callback from dxgi.dll wrapper: screen resolution width=%d, height=%d received\n", 
			info->width, info->height);
		if (LogFile) fflush(LogFile);
		G->mSwapChainInfo = *info;
		return 0x13bc7e31;
	}

	if (LogFile) fprintf(LogFile, "QueryInterface request for %08lx-%04hx-%04hx-%02hhx%02hhx-%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx on %x\n", 
		riid.Data1, riid.Data2, riid.Data3, riid.Data4[0], riid.Data4[1], riid.Data4[2], riid.Data4[3], riid.Data4[4], riid.Data4[5], riid.Data4[6], riid.Data4[7], this);
	if (LogFile) fflush(LogFile);
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
	if (LogFile && d3d10device) fprintf(LogFile, "  9b7e4c0f-342c-4106-a19f-4f2704f689f0 = ID3D10Device\n");
	if (LogFile && d3d10multithread) fprintf(LogFile, "  9b7e4e00-342c-4106-a19f-4f2704f689f0 = ID3D10Multithread\n");
	if (LogFile && dxgidevice) fprintf(LogFile, "  54ec77fa-1377-44e6-8c32-88fd5f44c84c = IDXGIDevice\n");
	if (LogFile && dxgidevice1) fprintf(LogFile, "  77db970f-6276-48ba-ba28-070143b4392c = IDXGIDevice1\n");
	if (LogFile && dxgidevice2) fprintf(LogFile, "  05008617-fbfd-4051-a790-144884b4f6a9 = IDXGIDevice2\n");
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
			if (LogFile) fprintf(LogFile, "  interface replaced with ID3D11Device wrapper. Interface counter=%d, wrapper counter=%d, wrapper internal counter = %d\n", cnt, p1->m_ulRef, cnt2);
		}
		D3D11Wrapper::ID3D11DeviceContext *p2 = (D3D11Wrapper::ID3D11DeviceContext*) D3D11Wrapper::ID3D11DeviceContext::m_List.GetDataPtr(*ppvObj);
		if (p2)
		{
			unsigned long cnt = ((IDirect3DUnknown*)*ppvObj)->Release();
			*ppvObj = p2;
			unsigned long cnt2 = p2->AddRef();
			if (LogFile) fprintf(LogFile, "  interface replaced with ID3D11DeviceContext wrapper. Interface counter=%d, wrapper counter=%d, wrapper internal counter = %d\n", cnt, p2->m_ulRef, cnt2);
		}
		D3D11Wrapper::IDXGIDevice2 *p3 = (D3D11Wrapper::IDXGIDevice2*) D3D11Wrapper::IDXGIDevice2::m_List.GetDataPtr(*ppvObj);
		if (p3)
		{
			unsigned long cnt = ((IDirect3DUnknown*)*ppvObj)->Release();
			*ppvObj = p3;
			unsigned long cnt2 = p3->AddRef();
			if (LogFile) fprintf(LogFile, "  interface replaced with IDXGIDevice2 wrapper. Interface counter=%d, wrapper counter=%d, wrapper internal counter = %d\n", cnt, p3->m_ulRef, cnt2);
		}
		D3D11Wrapper::ID3D10Device *p4 = (D3D11Wrapper::ID3D10Device*) D3D11Wrapper::ID3D10Device::m_List.GetDataPtr(*ppvObj);
		if (p4)
		{
			unsigned long cnt = ((IDirect3DUnknown*)*ppvObj)->Release();
			*ppvObj = p4;
			unsigned long cnt2 = p4->AddRef();
			if (LogFile) fprintf(LogFile, "  interface replaced with ID3D10Device wrapper. Interface counter=%d, wrapper counter=%d, wrapper internal counter = %d\n", cnt, p4->m_ulRef, cnt2);
		}
		D3D11Wrapper::ID3D10Multithread *p5 = (D3D11Wrapper::ID3D10Multithread*) D3D11Wrapper::ID3D10Multithread::m_List.GetDataPtr(*ppvObj);
		if (p5)
		{
			unsigned long cnt = ((IDirect3DUnknown*)*ppvObj)->Release();
			*ppvObj = p5;
			unsigned long cnt2 = p5->AddRef();
			if (LogFile) fprintf(LogFile, "  interface replaced with ID3D10Multithread wrapper. Interface counter=%d, wrapper counter=%d\n", cnt, p5->m_ulRef);
		}
		if (!p1 && !p2 && !p3 && !p4 && !p5)
		{
			// Check for IDXGIDevice, IDXGIDevice1 or IDXGIDevice2 cast.
			if (dxgidevice || dxgidevice1 || dxgidevice2)
			{
				// Cast again, but always use IDXGIDevice2 interface.
				D3D11Base::IDXGIDevice *oldDevice = (D3D11Base::IDXGIDevice *)*ppvObj;
				if (LogFile) fprintf(LogFile, "  releasing received IDXGIDevice, handle=%x. Querying IDXGIDevice2 interface.\n", *ppvObj);
				if (LogFile) fflush(LogFile);
				oldDevice->Release();
				const IID IID_IGreet = {0x7A5E6E81,0x3DF8,0x11D3,{0x90,0x3D,0x00,0x10,0x5A,0xA4,0x5B,0xDC}};
				const IID IDXGIDevice2 = {0x05008617,0xfbfd,0x4051,{0xa7,0x90,0x14,0x48,0x84,0xb4,0xf6,0xa9}};
				hr = m_pUnk->QueryInterface(IDXGIDevice2, ppvObj);
				if (hr != S_OK)
				{
					if (LogFile) fprintf(LogFile, "  error querying IDXGIDevice2 interface. Trying IDXGIDevice1.\n");
					if (LogFile) fflush(LogFile);											    
					const IID IDXGIDevice1 = {0x77db970f,0x6276,0x48ba,{0xba,0x28,0x07,0x01,0x43,0xb4,0x39,0x2c}};
					hr = m_pUnk->QueryInterface(IDXGIDevice1, ppvObj);
					if (hr != S_OK)
					{
						if (LogFile) fprintf(LogFile, "  error querying IDXGIDevice1 interface. Trying IDXGIDevice.\n");
						if (LogFile) fflush(LogFile);
						const IID IDXGIDevice = {0x54ec77fa,0x1377,0x44e6,{0x8c,0x32,0x88,0xfd,0x5f,0x44,0xc8,0x4c}};
						hr = m_pUnk->QueryInterface(IDXGIDevice, ppvObj);
						if (hr != S_OK)
						{
							if (LogFile) fprintf(LogFile, "  error querying IDXGIDevice interface.\n");
							if (LogFile) fflush(LogFile);
							return E_OUTOFMEMORY;
						}
					}
				}
				D3D11Base::IDXGIDevice2 *origDevice = (D3D11Base::IDXGIDevice2 *)*ppvObj;
				D3D11Wrapper::IDXGIDevice2 *wrapper = D3D11Wrapper::IDXGIDevice2::GetDirectDevice2(origDevice);
				if(wrapper == NULL)
				{
					if (LogFile) fprintf(LogFile, "  error allocating IDXGIDevice2 wrapper.\n");
					if (LogFile) fflush(LogFile);
					origDevice->Release();
					return E_OUTOFMEMORY;
				}
				*ppvObj = wrapper;
				if (LogFile) fprintf(LogFile, "  interface replaced with IDXGIDevice2 wrapper, original device handle=%x. Wrapper counter=%d\n", 
					origDevice, wrapper->m_ulRef);
			}
			// Check for DirectX10 cast.
			if (d3d10device)
			{
				D3D11Base::ID3D10Device *origDevice = (D3D11Base::ID3D10Device *)*ppvObj;
				D3D11Wrapper::ID3D10Device *wrapper = D3D11Wrapper::ID3D10Device::GetDirect3DDevice(origDevice);
				if(wrapper == NULL)
				{
					if (LogFile) fprintf(LogFile, "  error allocating ID3D10Device wrapper.\n");
					if (LogFile) fflush(LogFile);
					origDevice->Release();
					return E_OUTOFMEMORY;
				}
				*ppvObj = wrapper;
				if (LogFile) fprintf(LogFile, "  interface replaced with ID3D10Device wrapper. Wrapper counter=%d\n", wrapper->m_ulRef);
			}
			if (d3d10multithread)
			{
				D3D11Base::ID3D10Multithread *origDevice = (D3D11Base::ID3D10Multithread *)*ppvObj;
				D3D11Wrapper::ID3D10Multithread *wrapper = D3D11Wrapper::ID3D10Multithread::GetDirect3DMultithread(origDevice);
				if(wrapper == NULL)
				{
					if (LogFile) fprintf(LogFile, "  error allocating ID3D10Multithread wrapper.\n");
					if (LogFile) fflush(LogFile);
					origDevice->Release();
					return E_OUTOFMEMORY;
				}
				*ppvObj = wrapper;
				if (LogFile) fprintf(LogFile, "  interface replaced with ID3D10Multithread wrapper. Wrapper counter=%d\n", wrapper->m_ulRef);
			}
		}
	}
	if (LogFile) fprintf(LogFile, "  result = %x, handle = %x\n", hr, *ppvObj);
	if (LogFile) fflush(LogFile);
	return hr;
}

static D3D11Base::IDXGIAdapter *ReplaceAdapter(D3D11Base::IDXGIAdapter *wrapper)
{
	if (!wrapper)
		return wrapper;
	if (LogFile) fprintf(LogFile, "  checking for adapter wrapper, handle = %x\n", wrapper);
	IID marker = { 0x017b2e72ul, 0xbcde, 0x9f15, { 0xa1, 0x2b, 0x3c, 0x4d, 0x5e, 0x6f, 0x70, 0x00 } };
	D3D11Base::IDXGIAdapter *realAdapter;
	if (wrapper->GetParent(marker, (void **) &realAdapter) == 0x13bc7e32)
	{
		if (LogFile) fprintf(LogFile, "    wrapper found. replacing with original handle = %x\n", realAdapter);
		if (LogFile) fflush(LogFile);
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
	if (LogFile) fprintf(LogFile, "D3D11CreateDevice called with adapter = %x\n", pAdapter);
	if (LogFile) fflush(LogFile);
	D3D11Base::ID3D11Device *origDevice = 0;
	D3D11Base::ID3D11DeviceContext *origContext = 0;
	EnableStereo();
	HRESULT ret = (*_D3D11CreateDevice)(ReplaceAdapter(pAdapter), DriverType, Software, Flags, pFeatureLevels,
		FeatureLevels, SDKVersion, &origDevice, pFeatureLevel, &origContext);
	if (ret != S_OK)
	{
		if (LogFile) fprintf(LogFile, "  failed with HRESULT=%x\n", ret);
		if (LogFile) fflush(LogFile);
		return ret;
	}

	D3D11Wrapper::ID3D11Device *wrapper = D3D11Wrapper::ID3D11Device::GetDirect3DDevice(origDevice);
	if(wrapper == NULL)
	{
		if (LogFile) fprintf(LogFile, "  error allocating wrapper.\n");
		if (LogFile) fflush(LogFile);
		origDevice->Release();
		origContext->Release();
		return E_OUTOFMEMORY;
	}
	if (ppDevice)
		*ppDevice = wrapper;

	D3D11Wrapper::ID3D11DeviceContext *wrapper2 = D3D11Wrapper::ID3D11DeviceContext::GetDirect3DDeviceContext(origContext);
	if(wrapper == NULL)
	{
		if (LogFile) fprintf(LogFile, "  error allocating wrapper2.\n");
		if (LogFile) fflush(LogFile);
		origDevice->Release();
		origContext->Release();
		return E_OUTOFMEMORY;
	}
	if (ppImmediateContext)
		*ppImmediateContext = wrapper2;

	if (LogFile) fprintf(LogFile, "  returns result = %x, device handle = %x, device wrapper = %x, context handle = %x, context wrapper = %x\n", ret, origDevice, wrapper, origContext, wrapper2);
	if (LogFile) fflush(LogFile);
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
	if (LogFile) fprintf(LogFile, "D3D11CreateDeviceAndSwapChain called with adapter = %x\n", pAdapter);
	if (LogFile && pSwapChainDesc) fprintf(LogFile, "  Windowed = %d\n", pSwapChainDesc->Windowed);
	if (LogFile && pSwapChainDesc) fprintf(LogFile, "  Width = %d\n", pSwapChainDesc->BufferDesc.Width);
	if (LogFile && pSwapChainDesc) fprintf(LogFile, "  Height = %d\n", pSwapChainDesc->BufferDesc.Height);
	if (LogFile && pSwapChainDesc) fprintf(LogFile, "  Refresh rate = %f\n", 
		(float) pSwapChainDesc->BufferDesc.RefreshRate.Numerator / (float) pSwapChainDesc->BufferDesc.RefreshRate.Denominator);

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
		(float) pSwapChainDesc->BufferDesc.RefreshRate.Numerator / (float) pSwapChainDesc->BufferDesc.RefreshRate.Denominator,
		pSwapChainDesc->Windowed);
	if (LogFile) fflush(LogFile);

	EnableStereo();
	D3D11Base::ID3D11Device *origDevice = 0;
	D3D11Base::ID3D11DeviceContext *origContext = 0;
	HRESULT ret = (*_D3D11CreateDeviceAndSwapChain)(ReplaceAdapter(pAdapter), DriverType, Software, Flags, pFeatureLevels,
		FeatureLevels, SDKVersion, pSwapChainDesc, ppSwapChain, &origDevice, pFeatureLevel, &origContext);
	if (ret != S_OK)
	{
		if (LogFile) fprintf(LogFile, "  failed with HRESULT=%x\n", ret);
		if (LogFile) fflush(LogFile);
		return ret;
	}

	if (LogFile) fprintf(LogFile, "  CreateDeviceAndSwapChain returned device handle = %x, context handle = %x\n", origDevice, origContext);
	if (LogFile) fflush(LogFile);
	if (!origDevice || !origContext)
		return ret;

	D3D11Wrapper::ID3D11Device *wrapper = D3D11Wrapper::ID3D11Device::GetDirect3DDevice(origDevice);
	if (wrapper == NULL)
	{
		if (LogFile) fprintf(LogFile, "  error allocating wrapper.\n");
		if (LogFile) fflush(LogFile);
		origDevice->Release();
		origContext->Release();
		return E_OUTOFMEMORY;
	}
	if (ppDevice)
		*ppDevice = wrapper;

	D3D11Wrapper::ID3D11DeviceContext *wrapper2 = D3D11Wrapper::ID3D11DeviceContext::GetDirect3DDeviceContext(origContext);
	if(wrapper == NULL)
	{
		if (LogFile) fprintf(LogFile, "  error allocating wrapper2.\n");
		if (LogFile) fflush(LogFile);
		origDevice->Release();
		origContext->Release();
		return E_OUTOFMEMORY;
	}
	if (ppImmediateContext)
		*ppImmediateContext = wrapper2;

	if (LogFile) fprintf(LogFile, "  returns result = %x, device handle = %x, device wrapper = %x, context handle = %x, context wrapper = %x\n", ret, origDevice, wrapper, origContext, wrapper2);
	if (LogFile) fflush(LogFile);
	return ret;
}

int WINAPI D3DKMTGetDeviceState(int a)
{
	InitD311();
	if (LogFile) fprintf(LogFile, "D3DKMTGetDeviceState called.\n");
	if (LogFile) fflush(LogFile);
	return (*_D3DKMTGetDeviceState)(a);
}

int WINAPI D3DKMTOpenAdapterFromHdc(int a)
{
	InitD311();
	if (LogFile) fprintf(LogFile, "D3DKMTOpenAdapterFromHdc called.\n");
	if (LogFile) fflush(LogFile);
	return (*_D3DKMTOpenAdapterFromHdc)(a);
}

int WINAPI D3DKMTOpenResource(int a)
{
	InitD311();
	if (LogFile) fprintf(LogFile, "D3DKMTOpenResource called.\n");
	if (LogFile) fflush(LogFile);
	return (*_D3DKMTOpenResource)(a);
}

int WINAPI D3DKMTQueryResourceInfo(int a)
{
	InitD311();
	if (LogFile) fprintf(LogFile, "D3DKMTQueryResourceInfo called.\n");
	if (LogFile) fflush(LogFile);
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
		if (LogFile && LogDebug) fflush(LogFile);
	}
}

#include "Direct3D11Device.h"
#include "Direct3D11Context.h"
#include "DirectDXGIDevice.h"
#include "../DirectX10/Direct3D10Device.h"
