#include "Main.h"
#include <Shlobj.h>
#include <Winuser.h>
#include "input.h"
#include <ctime>
#include <map>
#include <vector>
#include <set>
#include <iterator>
#include <string>
#include <DirectXMath.h>
#include "../HLSLDecompiler/DecompileHLSL.h"
#include "../util.h"
#include "../log.h"
#include "Override.h"
#include "globals.h"

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

static bool ReloadConfigPending = false;

Globals *G;


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
	RightStripA(buf);
	char *start = buf; while (isspace(*start)) start++;
	return start;
}

void RegisterPresetKeyBindings(LPCWSTR iniFile)
{
	enum KeyOverrideType type;
	wchar_t key[MAX_PATH];
	wchar_t buf[MAX_PATH];
	KeyOverride *preset;
	int i;

	// TODO: Use GetPrivateProfileSectionNames() to enumerate all [Key*]
	// sections, rather than requiring them to start at 1 and increment
	// consecutively. Same thing also applies to [ShaderOverride*] and
	// [TextureOverride*] sections. Also, I'm considering dropping the
	// requirement for these to end in a number - after all, these are not
	// referenced by anything and the only real reason there is a number
	// here at all is to work around the limitation that they must be
	// unique names since we are using a .ini file to store configuration.

	for (i = 1;; i++) {
		LogDebug("Find [Key] i=%i\n", i);
		wchar_t id[] = L"Keyxxx";
		_itow_s(i, id + 3, 3, 10);
		if (!GetPrivateProfileString(id, L"Key", 0, key, MAX_PATH, iniFile))
			break;

		LogInfoW(L"[%s]\n", id);

		type = KeyOverrideType::ACTIVATE;

		if (GetPrivateProfileString(id, L"type", 0, buf, MAX_PATH, iniFile)) {
			// XXX: hold & toggle types will restore the previous
			// settings on release - there's possibly also another
			// use case for setting a specific profile instead.
			if (!_wcsicmp(buf, L"hold")) {
				type = KeyOverrideType::HOLD;
				LogInfo("  type=hold\n");
			} else if (!_wcsicmp(buf, L"toggle")) {
				type = KeyOverrideType::TOGGLE;
				LogInfo("  type=toggle\n");
			} else {
				LogInfoW(L"  WARNING: UNKNOWN KEY BINDING TYPE %s\n", buf);
			}
		}

		// TODO: Alternatively allow the [Key] section to contain a
		// reference to other preset sections, to allow a single key to
		// be used to cycle between several presets. In this case we
		// would use that list of presets rather than the settings
		// directly in the Key section.

		preset = new KeyOverride(type);
		preset->ParseIniSection(id, iniFile);

		RegisterKeyBinding(L"Key", key, (InputListener*)preset);
	}
}

// TODO: Reorder functions in this file to remove the need for this prototype:
void RegisterHuntingKeyBindings(wchar_t *iniFile);

static void LoadConfigFile()
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
		if (!LogFile)
			LogFile = _fsopen("d3d11_log.txt", "w", _SH_DENYNO);
		LogInfo("\nD3D11 DLL starting init  -  %s\n\n", LogTime().c_str());
		LogInfo("----------- d3dx.ini settings -----------\n");
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
		LogInfo("[Logging]\n");
		LogInfo("  calls=1\n");
		if (LogInput) LogInfo("  input=1\n");
		LogDebug("  debug=1\n");
		if (unbuffered != -1) LogInfo("  unbuffered=1  return: %d\n", unbuffered);
		if (affinity != -1) LogInfo("  force_cpu_affinity=1  return: %s\n", affinity ? "true" : "false");
		if (waitfordebugger) LogInfo("  waitfordebugger=1\n");
	}

	// [System]
	GetPrivateProfileString(L"System", L"proxy_d3d11", 0, DLL_PATH, MAX_PATH, iniFile);
	if (LogFile)
	{
		LogInfo("[System]\n");
		if (!DLL_PATH) LogInfo("  proxy_d3d11=%s\n", DLL_PATH);
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
		LogInfo("[Device]\n");
		if (G->SCREEN_WIDTH != -1) LogInfo("  width=%d\n", G->SCREEN_WIDTH);
		if (G->SCREEN_HEIGHT != -1) LogInfo("  height=%d\n", G->SCREEN_HEIGHT);
		if (G->SCREEN_REFRESH != -1) LogInfo("  refresh_rate=%d\n", G->SCREEN_REFRESH);
		if (refresh[0]) LogInfoW(L"  filter_refresh_rate=%s\n", refresh);
		if (G->SCREEN_FULLSCREEN) LogInfo("  full_screen=1\n");
		if (G->gForceStereo) LogInfo("  force_stereo=1\n");
		if (allowWindowCommands) LogInfo("  allow_windowcommands=1\n");
	}

	// [Stereo]
	bool automaticMode = GetPrivateProfileInt(L"Stereo", L"automatic_mode", 0, iniFile) == 1;				// in NVapi dll
	G->gCreateStereoProfile = GetPrivateProfileInt(L"Stereo", L"create_profile", 0, iniFile) == 1;
	G->gSurfaceCreateMode = GetPrivateProfileInt(L"Stereo", L"surface_createmode", -1, iniFile);
	G->gSurfaceSquareCreateMode = GetPrivateProfileInt(L"Stereo", L"surface_square_createmode", -1, iniFile);

	if (LogFile)
	{
		LogInfo("[Stereo]\n");
		if (automaticMode) LogInfo("  automatic_mode=1\n");
		if (G->gCreateStereoProfile) LogInfo("  create_profile=1\n");
		if (G->gSurfaceCreateMode != -1) LogInfo("  surface_createmode=%d\n", G->gSurfaceCreateMode);
		if (G->gSurfaceSquareCreateMode != -1) LogInfo("  surface_square_createmode=%d\n", G->gSurfaceSquareCreateMode);
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
		LogInfo("[Rendering]\n");
		if (SHADER_PATH[0])
			LogInfoW(L"  override_directory=%s\n", SHADER_PATH);
		if (SHADER_CACHE_PATH[0])
			LogInfoW(L"  cache_directory=%s\n", SHADER_CACHE_PATH);

		if (G->CACHE_SHADERS) LogInfo("  cache_shaders=1\n");
		if (G->PRELOAD_SHADERS) LogInfo("  preload_shaders=1\n");
		if (G->ENABLE_CRITICAL_SECTION) LogInfo("  use_criticalsection=1\n");
		if (G->SCISSOR_DISABLE) LogInfo("  rasterizer_disable_scissor=1\n");

		if (G->EXPORT_FIXED) LogInfo("  export_fixed=1\n");
		if (G->EXPORT_SHADERS) LogInfo("  export_shaders=1\n");
		if (G->EXPORT_HLSL != 0) LogInfo("  export_hlsl=%d\n", G->EXPORT_HLSL);
		if (G->DumpUsage) LogInfo("  dump_usage=1\n");
	}


	// Automatic section 
	G->FIX_SV_Position = GetPrivateProfileInt(L"Rendering", L"fix_sv_position", 0, iniFile) == 1;
	G->FIX_Light_Position = GetPrivateProfileInt(L"Rendering", L"fix_light_position", 0, iniFile) == 1;
	G->FIX_Recompile_VS = GetPrivateProfileInt(L"Rendering", L"recompile_all_vs", 0, iniFile) == 1;
	if (GetPrivateProfileString(L"Rendering", L"fix_ZRepair_DepthTexture1", 0, setting, MAX_PATH, iniFile))
	{
		char buf[MAX_PATH];
		wcstombs(buf, setting, MAX_PATH);
		char *end = RightStripA(buf);
		G->ZRepair_DepthTextureReg1 = *end; *(end - 1) = 0;
		char *start = buf; while (isspace(*start)) start++;
		G->ZRepair_DepthTexture1 = start;
	}
	if (GetPrivateProfileString(L"Rendering", L"fix_ZRepair_DepthTexture2", 0, setting, MAX_PATH, iniFile))
	{
		char buf[MAX_PATH];
		wcstombs(buf, setting, MAX_PATH);
		char *end = RightStripA(buf);
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
	LogInfo("  ... missing automatic ini section\n");


	// [Hunting]
	LogInfo("[Hunting]\n");
	G->hunting = GetPrivateProfileInt(L"Hunting", L"hunting", 0, iniFile) == 1;
	if (G->hunting)
		LogInfo("  hunting=1\n");

	G->marking_mode = MARKING_MODE_SKIP;
	if (GetPrivateProfileString(L"Hunting", L"marking_mode", 0, setting, MAX_PATH, iniFile)) {
		if (!wcscmp(setting, L"skip")) G->marking_mode = MARKING_MODE_SKIP;
		if (!wcscmp(setting, L"mono")) G->marking_mode = MARKING_MODE_MONO;
		if (!wcscmp(setting, L"original")) G->marking_mode = MARKING_MODE_ORIGINAL;
		if (!wcscmp(setting, L"zero")) G->marking_mode = MARKING_MODE_ZERO;
		LogInfoW(L"  marking_mode=%d\n", G->marking_mode);
	}

	RegisterHuntingKeyBindings(iniFile);

	RegisterPresetKeyBindings(iniFile);


	// Todo: Not sure this is best spot.
	G->ENABLE_TUNE = GetPrivateProfileInt(L"Hunting", L"tune_enable", 0, iniFile) == 1;
	if (GetPrivateProfileString(L"Hunting", L"tune_step", 0, setting, MAX_PATH, iniFile))
		swscanf_s(setting, L"%f", &G->gTuneStep);


	// Shader separation overrides.
	G->mShaderSeparationMap.clear();
	G->mShaderIterationMap.clear();
	G->mShaderIndexBufferFilter.clear();
	for (int i = 1;; ++i)
	{
		LogDebug("Find [ShaderOverride] i=%i\n", i);
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
			LogDebug(" [ShaderOverride] Shader = %08lx%08lx, separation = %f\n", hashHi, hashLo, separation);
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
	LogInfo("  ... missing shader override ini section\n");

	// Texture overrides.
	G->mTextureStereoMap.clear();
	G->mTextureTypeMap.clear();
	G->mTextureIterationMap.clear();
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
			LogDebug("[TextureOverride] Texture = %08lx%08lx, stereo mode = %d\n", hashHi, hashLo, stereoMode);
		}
		int texFormat = GetPrivateProfileInt(id, L"Format", -1, iniFile);
		if (texFormat >= 0)
		{
			G->mTextureTypeMap[(UINT64(hashHi) << 32) | UINT64(hashLo)] = texFormat;
			LogDebug("[TextureOverride] Texture = %08lx%08lx, format = %d\n", hashHi, hashLo, texFormat);
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
			LogDebug("[TextureOverride] Texture = %08lx%08lx, iterations = %d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n", hashHi, hashLo,
				id[0], id[1], id[2], id[3], id[4], id[5], id[6], id[7], id[8], id[9]);
		}
	}

	// Todo: finish logging all input parameters.
	LogInfo("  ... missing texture override ini section\n");

	// Todo: finish logging all input parameters.
	LogInfo("  ... missing mouse OverrideSettings ini section\n");
	LogInfo("  ... missing convergence map ini section\n");
	LogInfo("-----------------------------------------\n");

	// Read in any constants defined in the ini, for use as shader parameters
	// Any result of the default FLT_MAX means the parameter is not in use.
	// stof will crash if passed FLT_MAX, hence the extra check.
	// We use FLT_MAX instead of the more logical INFINITY, because Microsoft *always* generates 
	// warnings, even for simple comparisons. And NaN comparisons are similarly broken.
	G->iniParams.x = 0;
	G->iniParams.y = 0;
	G->iniParams.z = 0;
	G->iniParams.w = 0;
	if (GetPrivateProfileString(L"Constants", L"x", L"FLT_MAX", setting, MAX_PATH, iniFile))
	{
		if (wcscmp(setting, L"FLT_MAX") != 0)
			G->iniParams.x = stof(setting);
	}
	if (GetPrivateProfileString(L"Constants", L"y", L"FLT_MAX", setting, MAX_PATH, iniFile))
	{
		if (wcscmp(setting, L"FLT_MAX") != 0)
			G->iniParams.y = stof(setting);
	}
	if (GetPrivateProfileString(L"Constants", L"z", L"FLT_MAX", setting, MAX_PATH, iniFile))
	{
		if (wcscmp(setting, L"FLT_MAX") != 0)
			G->iniParams.z = stof(setting);
	}
	if (GetPrivateProfileString(L"Constants", L"w", L"FLT_MAX", setting, MAX_PATH, iniFile))
	{
		if (wcscmp(setting, L"FLT_MAX") != 0)
			G->iniParams.w = stof(setting);
	}

	if (LogFile && 
		(G->iniParams.x != FLT_MAX) || (G->iniParams.y != FLT_MAX) || (G->iniParams.z != FLT_MAX) || (G->iniParams.w != FLT_MAX))
	{
		LogInfo("[Constants]\n");
		LogInfo("  x=%#.2g\n", G->iniParams.x);
		LogInfo("  y=%#.2g\n", G->iniParams.y);
		LogInfo("  z=%#.2g\n", G->iniParams.z);
		LogInfo("  w=%#.2g\n", G->iniParams.w);
	}
}

static void ReloadConfig(D3D11Base::ID3D11Device *device)
{
	D3D11Wrapper::ID3D11Device* wrapper = (D3D11Wrapper::ID3D11Device*) D3D11Wrapper::ID3D11Device::m_List.GetDataPtr(device);
	D3D11Base::ID3D11DeviceContext* realContext; device->GetImmediateContext(&realContext);
	D3D11Base::D3D11_MAPPED_SUBRESOURCE mappedResource;

	LogInfo("Reloading d3dx.ini (EXPERIMENTAL)...\n");

	ReloadConfigPending = false;

	if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);

	// Clear the key bindings. There may be other things that need to be
	// cleared as well, but for the sake of clarity I'd rather clear as
	// many as possible inside LoadConfigFile() where they are set.
	ClearKeyBindings();

	LoadConfigFile();

	if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);

	// Update the iniParams resource from the config file:
	realContext->Map(wrapper->mIniTexture, 0, D3D11Base::D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	memcpy(mappedResource.pData, &G->iniParams, sizeof(G->iniParams));
	realContext->Unmap(wrapper->mIniTexture, 0);
}

static void FlagConfigReload(D3D11Base::ID3D11Device *device, void *private_data)
{
	// When we reload the configuration, we are going to clear the existing
	// key bindings and reassign them. Naturally this is not a safe thing
	// to do from inside a key binding callback, so we just set a flag and
	// do this after the input subsystem has finished dispatching calls.
	ReloadConfigPending = true;
}

// During the initialize, we will also Log every setting that is enabled, so that the log
// has a complete list of active settings.  This should make it more accurate and clear.

void InitializeDLL()
{
	if (!gInitialized)
	{
		LoadConfigFile();

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
		//	LogInfo("Error while creating hunting thread: %x\n", dw);
		//}

		LogInfo("D3D11 DLL initialized.\n");
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
	//LogInfo("Hunting thread closed: %x, %d\n", err, ok);

	if (LogFile)
	{
		LogInfo("Destroying DLL...\n");
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
	LogDebug("D3DKMTCloseAdapter called.\n");

	return 0;
}
int WINAPI D3DKMTDestroyAllocation()
{
	LogDebug("D3DKMTDestroyAllocation called.\n");

	return 0;
}
int WINAPI D3DKMTDestroyContext()
{
	LogDebug("D3DKMTDestroyContext called.\n");

	return 0;
}
int WINAPI D3DKMTDestroyDevice()
{
	LogDebug("D3DKMTDestroyDevice called.\n");

	return 0;
}
int WINAPI D3DKMTDestroySynchronizationObject()
{
	LogDebug("D3DKMTDestroySynchronizationObject called.\n");

	return 0;
}
int WINAPI D3DKMTSetDisplayPrivateDriverFormat()
{
	LogDebug("D3DKMTSetDisplayPrivateDriverFormat called.\n");

	return 0;
}
int WINAPI D3DKMTSignalSynchronizationObject()
{
	LogDebug("D3DKMTSignalSynchronizationObject called.\n");

	return 0;
}
int WINAPI D3DKMTUnlock()
{
	LogDebug("D3DKMTUnlock called.\n");

	return 0;
}
int WINAPI D3DKMTWaitForSynchronizationObject()
{
	LogDebug("D3DKMTWaitForSynchronizationObject called.\n");

	return 0;
}
int WINAPI D3DKMTCreateAllocation()
{
	LogDebug("D3DKMTCreateAllocation called.\n");

	return 0;
}
int WINAPI D3DKMTCreateContext()
{
	LogDebug("D3DKMTCreateContext called.\n");

	return 0;
}
int WINAPI D3DKMTCreateDevice()
{
	LogDebug("D3DKMTCreateDevice called.\n");

	return 0;
}
int WINAPI D3DKMTCreateSynchronizationObject()
{
	LogDebug("D3DKMTCreateSynchronizationObject called.\n");

	return 0;
}
int WINAPI D3DKMTEscape()
{
	LogDebug("D3DKMTEscape called.\n");

	return 0;
}
int WINAPI D3DKMTGetContextSchedulingPriority()
{
	LogDebug("D3DKMTGetContextSchedulingPriority called.\n");

	return 0;
}
int WINAPI D3DKMTGetDisplayModeList()
{
	LogDebug("D3DKMTGetDisplayModeList called.\n");

	return 0;
}
int WINAPI D3DKMTGetMultisampleMethodList()
{
	LogDebug("D3DKMTGetMultisampleMethodList called.\n");

	return 0;
}
int WINAPI D3DKMTGetRuntimeData()
{
	LogDebug("D3DKMTGetRuntimeData called.\n");

	return 0;
}
int WINAPI D3DKMTGetSharedPrimaryHandle()
{
	LogDebug("D3DKMTGetRuntimeData called.\n");

	return 0;
}
int WINAPI D3DKMTLock()
{
	LogDebug("D3DKMTLock called.\n");

	return 0;
}
int WINAPI D3DKMTPresent()
{
	LogDebug("D3DKMTPresent called.\n");

	return 0;
}
int WINAPI D3DKMTQueryAllocationResidency()
{
	LogDebug("D3DKMTQueryAllocationResidency called.\n");

	return 0;
}
int WINAPI D3DKMTRender()
{
	LogDebug("D3DKMTRender called.\n");

	return 0;
}
int WINAPI D3DKMTSetAllocationPriority()
{
	LogDebug("D3DKMTSetAllocationPriority called.\n");

	return 0;
}
int WINAPI D3DKMTSetContextSchedulingPriority()
{
	LogDebug("D3DKMTSetContextSchedulingPriority called.\n");

	return 0;
}
int WINAPI D3DKMTSetDisplayMode()
{
	LogDebug("D3DKMTSetDisplayMode called.\n");

	return 0;
}
int WINAPI D3DKMTSetGammaRamp()
{
	LogDebug("D3DKMTSetGammaRamp called.\n");

	return 0;
}
int WINAPI D3DKMTSetVidPnSourceOwner()
{
	LogDebug("D3DKMTSetVidPnSourceOwner called.\n");

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
			LogInfo("trying to load %s\n", path);
		}
		hD3D11 = LoadLibrary(sysDir);
		if (!hD3D11)
		{
			if (LogFile)
			{
				char path[MAX_PATH];
				wcstombs(path, DLL_PATH, MAX_PATH);
				LogInfo("load failed. Trying to load %s\n", path);
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
			LogInfo("trying to load %s\n", path);
		}
		hD3D11 = LoadLibrary(sysDir);
	}
	if (hD3D11 == NULL)
	{
		LogInfo("LoadLibrary on d3d11.dll failed\n");

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
	LogInfo("D3DKMTQueryAdapterInfo called.\n");

	return (*_D3DKMTQueryAdapterInfo)(info);
}

int WINAPI OpenAdapter10(struct D3D10DDIARG_OPENADAPTER *adapter)
{
	InitD311();
	LogInfo("OpenAdapter10 called.\n");

	return (*_OpenAdapter10)(adapter);
}

int WINAPI OpenAdapter10_2(struct D3D10DDIARG_OPENADAPTER *adapter)
{
	InitD311();
	LogInfo("OpenAdapter10_2 called.\n");

	return (*_OpenAdapter10_2)(adapter);
}

int WINAPI D3D11CoreCreateDevice(__int32 a, int b, int c, LPCSTR lpModuleName, int e, int f, int g, int h, int i, int j)
{
	InitD311();
	LogInfo("D3D11CoreCreateDevice called.\n");

	return (*_D3D11CoreCreateDevice)(a, b, c, lpModuleName, e, f, g, h, i, j);
}


HRESULT WINAPI D3D11CoreCreateLayeredDevice(const void *unknown0, DWORD unknown1, const void *unknown2, REFIID riid, void **ppvObj)
{
	InitD311();
	LogInfo("D3D11CoreCreateLayeredDevice called.\n");

	return (*_D3D11CoreCreateLayeredDevice)(unknown0, unknown1, unknown2, riid, ppvObj);
}

SIZE_T WINAPI D3D11CoreGetLayeredDeviceSize(const void *unknown0, DWORD unknown1)
{
	InitD311();
	// Call from D3DCompiler (magic number from there) ?
	if ((intptr_t)unknown0 == 0x77aa128b)
	{
		LogInfo("Shader code info from D3DCompiler_xx.dll wrapper received:\n");

		D3D11BridgeData *data = (D3D11BridgeData *)unknown1;
		LogInfo("  Bytecode hash = %08lx%08lx\n", (UINT32)(data->BinaryHash >> 32), (UINT32)data->BinaryHash);
		LogInfo("  Filename = %s\n", data->HLSLFileName);

		G->mCompiledShaderMap[data->BinaryHash] = data->HLSLFileName;
		return 0xaa77125b;
	}
	LogInfo("D3D11CoreGetLayeredDeviceSize called.\n");

	return (*_D3D11CoreGetLayeredDeviceSize)(unknown0, unknown1);
}

HRESULT WINAPI D3D11CoreRegisterLayers(const void *unknown0, DWORD unknown1)
{
	InitD311();
	LogInfo("D3D11CoreRegisterLayers called.\n");

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
		LogInfo("  stereo init failed: %s\n", errorMessage);
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
			LogInfo("  stereo init failed: no stereo driver detected.\n");
		}
		// Stereo is available but not enabled, let's enable it
		else if (D3D11Base::NVAPI_OK == status && !isStereoEnabled)
		{
			LogInfo("  stereo available and disabled. Enabling stereo.\n");
			status = D3D11Base::NvAPI_Stereo_Enable();
			if (status != D3D11Base::NVAPI_OK)
				LogInfo("    enabling stereo failed.\n");
		}

		if (G->gCreateStereoProfile)
		{
			LogInfo("  enabling registry profile.\n");

			D3D11Base::NvAPI_Stereo_CreateConfigurationProfileRegistryKey(D3D11Base::NVAPI_STEREO_DEFAULT_REGISTRY_PROFILE);
		}
	}
}

static int StrRenderTarget2D(char *buf, size_t size, D3D11Base::D3D11_TEXTURE2D_DESC *desc)
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

static int StrRenderTarget3D(char *buf, size_t size, D3D11Base::D3D11_TEXTURE3D_DESC *desc)
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
	switch(info.type) {
		case D3D11Base::D3D11_RESOURCE_DIMENSION_TEXTURE2D:
			return StrRenderTarget2D(buf, size, &info.tex2d_desc);
		case D3D11Base::D3D11_RESOURCE_DIMENSION_TEXTURE3D:
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
		LogInfoW(L"    marked shader file already exists: %s\n", fullName);
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
		LogInfoW(L"    error storing marked shader to %s\n", fullName);
		return false;
	}

	LogInfoW(L"    storing patched shader to %s\n", fullName);

	fwrite(hlslText.c_str(), 1, hlslText.size(), fw);

	fprintf_s(fw, "\n\n/*~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
	// Size - 1 to strip NULL terminator
	fwrite(asmTextBlob->GetBufferPointer(), 1, asmTextBlob->GetBufferSize() - 1, fw);
	fprintf_s(fw, "\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~*/\n");

	fclose(fw);
	return true;
}


// Decompile code passed in as assembly text, and shader byte code.
// This is pretty heavyweight obviously, so it is only being done during Mark operations.
// Todo: another copy/paste job, we really need some subroutines, utility library.

static string Decompile(D3D11Base::ID3DBlob* pShaderByteCode, AsmTextBlob* disassembly)
{
	LogInfo("    creating HLSL representation.\n");

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
		LogInfo("    error while decompiling.\n");
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
		LogInfo("    disassembly of original shader failed: \n");
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
		LogInfoW(L"    Shader Mark .bin file already exists: %s\n", fullName);
		fclose(f);
		return false;
	}

	_wfopen_s(&f, fullName, L"wb");
	if (!f)
	{
		LogInfoW(L"    Shader Mark could not write asm text file: %s\n", fullName);
		return false;
	}

	// Size - 1 to strip NULL terminator
	fwrite(asmTextBlob->GetBufferPointer(), 1, asmTextBlob->GetBufferSize() - 1, f);
	fclose(f);
	LogInfoW(L"    storing disassembly to %s\n", fullName);

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
		LogInfo("    ReloadShader shader not found: %ls\n", fullName);

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

	LogInfo("   >Replacement shader found. Re-Loading replacement HLSL code from %ls\n", fileName);
	LogInfo("    Reload source code loaded. Size = %d\n", srcDataSize);
	LogInfo("    compiling replacement HLSL code with shader model %s\n", shaderModel);


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
				LogInfo("> failed to find original shader in mReloadedShaders: %ls\n", fileName);
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

			LogInfo("> successfully reloaded shader: %ls\n", fileName);
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
		LogInfo("> successfully copied Marked shader to ShaderFixes\n");
	}
	else
	{
		Beep(200, 150);			// Bonk sound for failure.
		LogInfo("> FAILED to copy Marked shader to ShaderFixes\n");
	}
}


// Key binding callbacks
static void TakeScreenShot(D3D11Base::ID3D11Device *device, void *private_data)
{
	LogInfo("> capturing screenshot\n");

	D3D11Wrapper::ID3D11Device* wrapped = (D3D11Wrapper::ID3D11Device*) D3D11Wrapper::ID3D11Device::m_List.GetDataPtr(device);
	if (wrapped->mStereoHandle)
	{
		D3D11Base::NvAPI_Status err;
		err = D3D11Base::NvAPI_Stereo_CapturePngImage(wrapped->mStereoHandle);
		if (err != D3D11Base::NVAPI_OK)
		{
			LogInfo("> screenshot failed, error:%d\n", err);
			Beep(300, 200); Beep(200, 150);		// Brnk, dunk sound for failure.
		}
	}
}

static void ReloadFixes(D3D11Base::ID3D11Device *device, void *private_data)
{
	LogInfo("> reloading *_replace.txt fixes from ShaderFixes\n");

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
			LogInfo("> successfully reloaded shaders from ShaderFixes\n");
		}
		else
		{
			Beep(200, 150);			// Bonk sound for failure.
			LogInfo("> FAILED to reload shaders from ShaderFixes\n");
		}
	}
}

static void NextIndexBuffer(D3D11Base::ID3D11Device *device, void *private_data)
{
	if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
	std::set<UINT64>::iterator i = G->mVisitedIndexBuffers.find(G->mSelectedIndexBuffer);
	if (i != G->mVisitedIndexBuffers.end() && ++i != G->mVisitedIndexBuffers.end())
	{
		G->mSelectedIndexBuffer = *i;
		G->mSelectedIndexBufferPos++;
		LogInfo("> traversing to next index buffer #%d. Number of index buffers in frame: %d\n", G->mSelectedIndexBufferPos, G->mVisitedIndexBuffers.size());
	}
	if (i == G->mVisitedIndexBuffers.end() && ++G->mSelectedIndexBufferPos < G->mVisitedIndexBuffers.size() && G->mSelectedIndexBufferPos >= 0)
	{
		i = G->mVisitedIndexBuffers.begin();
		std::advance(i, G->mSelectedIndexBufferPos);
		G->mSelectedIndexBuffer = *i;
		LogInfo("> last index buffer lost. traversing to next index buffer #%d. Number of index buffers in frame: %d\n", G->mSelectedIndexBufferPos, G->mVisitedIndexBuffers.size());
	}
	if (i == G->mVisitedIndexBuffers.end() && G->mVisitedIndexBuffers.size() != 0)
	{
		G->mSelectedIndexBufferPos = 0;
		LogInfo("> traversing to index buffer #0. Number of index buffers in frame: %d\n", G->mVisitedIndexBuffers.size());

		G->mSelectedIndexBuffer = *G->mVisitedIndexBuffers.begin();
	}
	if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
}

static void PrevIndexBuffer(D3D11Base::ID3D11Device *device, void *private_data)
{
	if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
	std::set<UINT64>::iterator i = G->mVisitedIndexBuffers.find(G->mSelectedIndexBuffer);
	if (i != G->mVisitedIndexBuffers.end() && i != G->mVisitedIndexBuffers.begin())
	{
		--i;
		G->mSelectedIndexBuffer = *i;
		G->mSelectedIndexBufferPos--;
		LogInfo("> traversing to previous index buffer #%d. Number of index buffers in frame: %d\n", G->mSelectedIndexBufferPos, G->mVisitedIndexBuffers.size());
	}
	if (i == G->mVisitedIndexBuffers.end() && --G->mSelectedIndexBufferPos < G->mVisitedIndexBuffers.size() && G->mSelectedIndexBufferPos >= 0)
	{
		i = G->mVisitedIndexBuffers.begin();
		std::advance(i, G->mSelectedIndexBufferPos);
		G->mSelectedIndexBuffer = *i;
		LogInfo("> last index buffer lost. traversing to previous index buffer #%d. Number of index buffers in frame: %d\n", G->mSelectedIndexBufferPos, G->mVisitedIndexBuffers.size());
	}
	if (i == G->mVisitedIndexBuffers.end() && G->mVisitedIndexBuffers.size() != 0)
	{
		G->mSelectedIndexBufferPos = 0;
		LogInfo("> traversing to index buffer #0. Number of index buffers in frame: %d\n", G->mVisitedIndexBuffers.size());

		G->mSelectedIndexBuffer = *G->mVisitedIndexBuffers.begin();
	}
	if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
}

static void MarkIndexBuffer(D3D11Base::ID3D11Device *device, void *private_data)
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

static void NextPixelShader(D3D11Base::ID3D11Device *device, void *private_data)
{
	if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
	std::set<UINT64>::const_iterator i = G->mVisitedPixelShaders.find(G->mSelectedPixelShader);
	if (i != G->mVisitedPixelShaders.end() && ++i != G->mVisitedPixelShaders.end())
	{
		G->mSelectedPixelShader = *i;
		G->mSelectedPixelShaderPos++;
		LogInfo("> traversing to next pixel shader #%d. Number of pixel shaders in frame: %d\n", G->mSelectedPixelShaderPos, G->mVisitedPixelShaders.size());
	}
	if (i == G->mVisitedPixelShaders.end() && ++G->mSelectedPixelShaderPos < G->mVisitedPixelShaders.size() && G->mSelectedPixelShaderPos >= 0)
	{
		i = G->mVisitedPixelShaders.begin();
		std::advance(i, G->mSelectedPixelShaderPos);
		G->mSelectedPixelShader = *i;
		LogInfo("> last pixel shader lost. traversing to next pixel shader #%d. Number of pixel shaders in frame: %d\n", G->mSelectedPixelShaderPos, G->mVisitedPixelShaders.size());
	}
	if (i == G->mVisitedPixelShaders.end() && G->mVisitedPixelShaders.size() != 0)
	{
		G->mSelectedPixelShaderPos = 0;
		LogInfo("> traversing to pixel shader #0. Number of pixel shaders in frame: %d\n", G->mVisitedPixelShaders.size());

		G->mSelectedPixelShader = *G->mVisitedPixelShaders.begin();
	}
	if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
}

static void PrevPixelShader(D3D11Base::ID3D11Device *device, void *private_data)
{
	if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
	std::set<UINT64>::iterator i = G->mVisitedPixelShaders.find(G->mSelectedPixelShader);
	if (i != G->mVisitedPixelShaders.end() && i != G->mVisitedPixelShaders.begin())
	{
		--i;
		G->mSelectedPixelShader = *i;
		G->mSelectedPixelShaderPos--;
		LogInfo("> traversing to previous pixel shader #%d. Number of pixel shaders in frame: %d\n", G->mSelectedPixelShaderPos, G->mVisitedPixelShaders.size());
	}
	if (i == G->mVisitedPixelShaders.end() && --G->mSelectedPixelShaderPos < G->mVisitedPixelShaders.size() && G->mSelectedPixelShaderPos >= 0)
	{
		i = G->mVisitedPixelShaders.begin();
		std::advance(i, G->mSelectedPixelShaderPos);
		G->mSelectedPixelShader = *i;
		LogInfo("> last pixel shader lost. traversing to previous pixel shader #%d. Number of pixel shaders in frame: %d\n", G->mSelectedPixelShaderPos, G->mVisitedPixelShaders.size());
	}
	if (i == G->mVisitedPixelShaders.end() && G->mVisitedPixelShaders.size() != 0)
	{
		G->mSelectedPixelShaderPos = 0;
		LogInfo("> traversing to pixel shader #0. Number of pixel shaders in frame: %d\n", G->mVisitedPixelShaders.size());

		G->mSelectedPixelShader = *G->mVisitedPixelShaders.begin();
	}
	if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
}

static void MarkPixelShader(D3D11Base::ID3D11Device *device, void *private_data)
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
		LogInfo("       pixel shader was compiled from source code %s\n", i->second);
	}
	i = G->mCompiledShaderMap.find(G->mSelectedVertexShader);
	if (i != G->mCompiledShaderMap.end())
	{
		LogInfo("       vertex shader was compiled from source code %s\n", i->second);
	}
	// Copy marked shader to ShaderFixes
	CopyToFixes(G->mSelectedPixelShader, device);
	if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
	if (G->DumpUsage) DumpUsage();
}

static void NextVertexShader(D3D11Base::ID3D11Device *device, void *private_data)
{
	if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
	std::set<UINT64>::iterator i = G->mVisitedVertexShaders.find(G->mSelectedVertexShader);
	if (i != G->mVisitedVertexShaders.end() && ++i != G->mVisitedVertexShaders.end())
	{
		G->mSelectedVertexShader = *i;
		G->mSelectedVertexShaderPos++;
		LogInfo("> traversing to next vertex shader #%d. Number of vertex shaders in frame: %d\n", G->mSelectedVertexShaderPos, G->mVisitedVertexShaders.size());
	}
	if (i == G->mVisitedVertexShaders.end() && ++G->mSelectedVertexShaderPos < G->mVisitedVertexShaders.size() && G->mSelectedVertexShaderPos >= 0)
	{
		i = G->mVisitedVertexShaders.begin();
		std::advance(i, G->mSelectedVertexShaderPos);
		G->mSelectedVertexShader = *i;
		LogInfo("> last vertex shader lost. traversing to previous vertex shader #%d. Number of vertex shaders in frame: %d\n", G->mSelectedVertexShaderPos, G->mVisitedVertexShaders.size());
	}
	if (i == G->mVisitedVertexShaders.end() && G->mVisitedVertexShaders.size() != 0)
	{
		G->mSelectedVertexShaderPos = 0;
		LogInfo("> traversing to vertex shader #0. Number of vertex shaders in frame: %d\n", G->mVisitedVertexShaders.size());

		G->mSelectedVertexShader = *G->mVisitedVertexShaders.begin();
	}
	if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
}

static void PrevVertexShader(D3D11Base::ID3D11Device *device, void *private_data)
{
	if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
	std::set<UINT64>::iterator i = G->mVisitedVertexShaders.find(G->mSelectedVertexShader);
	if (i != G->mVisitedVertexShaders.end() && i != G->mVisitedVertexShaders.begin())
	{
		--i;
		G->mSelectedVertexShader = *i;
		G->mSelectedVertexShaderPos--;
		LogInfo("> traversing to previous vertex shader #%d. Number of vertex shaders in frame: %d\n", G->mSelectedVertexShaderPos, G->mVisitedVertexShaders.size());
	}
	if (i == G->mVisitedVertexShaders.end() && --G->mSelectedVertexShaderPos < G->mVisitedVertexShaders.size() && G->mSelectedVertexShaderPos >= 0)
	{
		i = G->mVisitedVertexShaders.begin();
		std::advance(i, G->mSelectedVertexShaderPos);
		G->mSelectedVertexShader = *i;
		LogInfo("> last vertex shader lost. traversing to previous vertex shader #%d. Number of vertex shaders in frame: %d\n", G->mSelectedVertexShaderPos, G->mVisitedVertexShaders.size());
	}
	if (i == G->mVisitedVertexShaders.end() && G->mVisitedVertexShaders.size() != 0)
	{
		G->mSelectedVertexShaderPos = 0;
		LogInfo("> traversing to vertex shader #0. Number of vertex shaders in frame: %d\n", G->mVisitedVertexShaders.size());

		G->mSelectedVertexShader = *G->mVisitedVertexShaders.begin();
	}
	if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
}

static void MarkVertexShader(D3D11Base::ID3D11Device *device, void *private_data)
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
		LogInfo("       shader was compiled from source code %s\n", i->second);
	}
	// Copy marked shader to ShaderFixes
	CopyToFixes(G->mSelectedVertexShader, device);
	if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
	if (G->DumpUsage) DumpUsage();
}

static void NextRenderTarget(D3D11Base::ID3D11Device *device, void *private_data)
{
	if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
	std::set<void *>::iterator i = G->mVisitedRenderTargets.find(G->mSelectedRenderTarget);
	if (i != G->mVisitedRenderTargets.end() && ++i != G->mVisitedRenderTargets.end())
	{
		G->mSelectedRenderTarget = *i;
		G->mSelectedRenderTargetPos++;
		LogInfo("> traversing to next render target #%d. Number of render targets in frame: %d\n", G->mSelectedRenderTargetPos, G->mVisitedRenderTargets.size());
	}
	if (i == G->mVisitedRenderTargets.end() && ++G->mSelectedRenderTargetPos < G->mVisitedRenderTargets.size() && G->mSelectedRenderTargetPos >= 0)
	{
		i = G->mVisitedRenderTargets.begin();
		std::advance(i, G->mSelectedRenderTargetPos);
		G->mSelectedRenderTarget = *i;
		LogInfo("> last render target lost. traversing to next render target #%d. Number of render targets frame: %d\n", G->mSelectedRenderTargetPos, G->mVisitedRenderTargets.size());
	}
	if (i == G->mVisitedRenderTargets.end() && G->mVisitedRenderTargets.size() != 0)
	{
		G->mSelectedRenderTargetPos = 0;
		LogInfo("> traversing to render target #0. Number of render targets in frame: %d\n", G->mVisitedRenderTargets.size());

		G->mSelectedRenderTarget = *G->mVisitedRenderTargets.begin();
	}
	if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
}

static void PrevRenderTarget(D3D11Base::ID3D11Device *device, void *private_data)
{
	if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
	std::set<void *>::iterator i = G->mVisitedRenderTargets.find(G->mSelectedRenderTarget);
	if (i != G->mVisitedRenderTargets.end() && i != G->mVisitedRenderTargets.begin())
	{
		--i;
		G->mSelectedRenderTarget = *i;
		G->mSelectedRenderTargetPos--;
		LogInfo("> traversing to previous render target #%d. Number of render targets in frame: %d\n", G->mSelectedRenderTargetPos, G->mVisitedRenderTargets.size());
	}
	if (i == G->mVisitedRenderTargets.end() && --G->mSelectedRenderTargetPos < G->mVisitedRenderTargets.size() && G->mSelectedRenderTargetPos >= 0)
	{
		i = G->mVisitedRenderTargets.begin();
		std::advance(i, G->mSelectedRenderTargetPos);
		G->mSelectedRenderTarget = *i;
		LogInfo("> last render target lost. traversing to previous render target #%d. Number of render targets in frame: %d\n", G->mSelectedRenderTargetPos, G->mVisitedRenderTargets.size());
	}
	if (i == G->mVisitedRenderTargets.end() && G->mVisitedRenderTargets.size() != 0)
	{
		G->mSelectedRenderTargetPos = 0;
		LogInfo("> traversing to render target #0. Number of render targets in frame: %d\n", G->mVisitedRenderTargets.size());

		G->mSelectedRenderTarget = *G->mVisitedRenderTargets.begin();
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

static void MarkRenderTarget(D3D11Base::ID3D11Device *device, void *private_data)
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

static void TuneUp(D3D11Base::ID3D11Device *device, void *private_data)
{
	int index = (int)private_data;

	G->gTuneValue[index] += G->gTuneStep;
	LogInfo("> Value %i tuned to %f\n", index+1, G->gTuneValue[index]);
}

static void TuneDown(D3D11Base::ID3D11Device *device, void *private_data)
{
	int index = (int)private_data;

	G->gTuneValue[index] -= G->gTuneStep;
	LogInfo("> Value %i tuned to %f\n", index+1, G->gTuneValue[index]);
}

// Start with a fresh set of shaders in the scene - either called explicitly
// via keypress, or after no hunting for 1 minute (see comment in RunFrameActions)
// Caller must have taken G->mCriticalSection (if enabled)
static void TimeoutHuntingBuffers()
{
	G->mVisitedIndexBuffers.clear();
	G->mVisitedVertexShaders.clear();
	G->mVisitedPixelShaders.clear();
	G->mSelectedPixelShader_IndexBuffer.clear();
	G->mSelectedVertexShader_IndexBuffer.clear();
	G->mSelectedIndexBuffer_PixelShader.clear();
	G->mSelectedIndexBuffer_VertexShader.clear();
	for (ShaderIterationMap::iterator i = G->mShaderIterationMap.begin(); i != G->mShaderIterationMap.end(); ++i)
		i->second[0] = 0;
}

// User has requested all shaders be re-enabled
static void DoneHunting(D3D11Base::ID3D11Device *device, void *private_data)
{
	TimeoutHuntingBuffers();

	G->mSelectedRenderTargetPos = 0;
	G->mSelectedRenderTarget = ((void *)1),
	G->mSelectedPixelShader = 1;
	G->mSelectedPixelShaderPos = 0;
	G->mSelectedVertexShader = 1;
	G->mSelectedVertexShaderPos = 0;
	G->mSelectedIndexBuffer = 1;
	G->mSelectedIndexBufferPos = 0;

	if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
}

static void RegisterHuntingKeyBindings(wchar_t *iniFile)
{
	int i;
	wchar_t buf[16];

	// reload_config is registered even if not hunting - this allows us to
	// turn on hunting in the ini dynamically without having to relaunch
	// the game. This can be useful in games that receive a significant
	// performance hit with hunting on, or where a broken effect is
	// discovered while playing normally where it may not be easy/fast to
	// find the effect again later.
	RegisterIniKeyBinding(L"Hunting", L"reload_config", iniFile, FlagConfigReload, NULL, NULL);

	if (!G->hunting)
		return;

	RegisterIniKeyBinding(L"Hunting", L"next_pixelshader", iniFile, NextPixelShader, NULL, NULL);
	RegisterIniKeyBinding(L"Hunting", L"previous_pixelshader", iniFile, PrevPixelShader, NULL, NULL);
	RegisterIniKeyBinding(L"Hunting", L"mark_pixelshader", iniFile, MarkPixelShader, NULL, NULL);

	RegisterIniKeyBinding(L"Hunting", L"take_screenshot", iniFile, TakeScreenShot, NULL, NULL);

	RegisterIniKeyBinding(L"Hunting", L"next_indexbuffer", iniFile, NextIndexBuffer, NULL, NULL);
	RegisterIniKeyBinding(L"Hunting", L"previous_indexbuffer", iniFile, PrevIndexBuffer, NULL, NULL);
	RegisterIniKeyBinding(L"Hunting", L"mark_indexbuffer", iniFile, MarkIndexBuffer, NULL, NULL);

	RegisterIniKeyBinding(L"Hunting", L"next_vertexshader", iniFile, NextVertexShader, NULL, NULL);
	RegisterIniKeyBinding(L"Hunting", L"previous_vertexshader", iniFile, PrevVertexShader, NULL, NULL);
	RegisterIniKeyBinding(L"Hunting", L"mark_vertexshader", iniFile, MarkVertexShader, NULL, NULL);

	RegisterIniKeyBinding(L"Hunting", L"next_rendertarget", iniFile, NextRenderTarget, NULL, NULL);
	RegisterIniKeyBinding(L"Hunting", L"previous_rendertarget", iniFile, PrevRenderTarget, NULL, NULL);
	RegisterIniKeyBinding(L"Hunting", L"mark_rendertarget", iniFile, MarkRenderTarget, NULL, NULL);

	RegisterIniKeyBinding(L"Hunting", L"done_hunting", iniFile, DoneHunting, NULL, NULL);

	RegisterIniKeyBinding(L"Hunting", L"reload_fixes", iniFile, ReloadFixes, NULL, NULL);

	for (i = 0; i < 4; i++) {
		_snwprintf(buf, 16, L"tune%i_up", i + 1);
		RegisterIniKeyBinding(L"Hunting", buf, iniFile, TuneUp, NULL, (void*)i);

		_snwprintf(buf, 16, L"tune%i_down", i + 1);
		RegisterIniKeyBinding(L"Hunting", buf, iniFile, TuneDown, NULL, (void*)i);
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
	LogDebug("Running frame actions.  Device: %p\n", device);

	// Regardless of log settings, since this runs every frame, let's flush the log
	// so that the most lost will be one frame worth.  Tradeoff of performance to accuracy
	if (LogFile) fflush(LogFile);

	bool newEvent = DispatchInputEvents(device);

	// The config file is not safe to reload from within the input handler
	// since it needs to change the key bindings, so it sets this flag
	// instead and we handle it now.
	if (ReloadConfigPending)
		ReloadConfig(device);

	// When not hunting most keybindings won't have been registered, but
	// still skip the below logic that only applies while hunting.
	if (!G->hunting)
		return;

	// Update the huntTime whenever we get fresh user input.
	if (newEvent)
		G->huntTime = time(NULL);

	// Clear buffers after some user idle time.  This allows the buffers to be
	// stable during a hunt, and cleared after one minute of idle time.  The idea
	// is to make the arrays of shaders stable so that hunting up and down the arrays
	// is consistent, while the user is engaged.  After 1 minute, they are likely onto
	// some other spot, and we should start with a fresh set, to keep the arrays and
	// active shader list small for easier hunting.  Until the first keypress, the arrays
	// are cleared at each thread wake, just like before. 
	// The arrays will be continually filled by the SetShader sections, but should 
	// rapidly converge upon all active shaders.

	if (difftime(time(NULL), G->huntTime) > 60) {
		if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
		TimeoutHuntingBuffers();
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
	LogDebug("D3D11Wrapper::IDirect3DUnknown::QueryInterface called at 'this': %s\n", typeid(*this).name());

	IID m1 = { 0x017b2e72ul, 0xbcde, 0x9f15, { 0xa1, 0x2b, 0x3c, 0x4d, 0x5e, 0x6f, 0x70, 0x01 } };
	IID m2 = { 0x017b2e72ul, 0xbcde, 0x9f15, { 0xa1, 0x2b, 0x3c, 0x4d, 0x5e, 0x6f, 0x70, 0x02 } };
	IID m3 = { 0x017b2e72ul, 0xbcde, 0x9f15, { 0xa1, 0x2b, 0x3c, 0x4d, 0x5e, 0x6f, 0x70, 0x03 } };
	if (riid.Data1 == m1.Data1 && riid.Data2 == m1.Data2 && riid.Data3 == m1.Data3 &&
		riid.Data4[0] == m1.Data4[0] && riid.Data4[1] == m1.Data4[1] && riid.Data4[2] == m1.Data4[2] && riid.Data4[3] == m1.Data4[3] &&
		riid.Data4[4] == m1.Data4[4] && riid.Data4[5] == m1.Data4[5] && riid.Data4[6] == m1.Data4[6] && riid.Data4[7] == m1.Data4[7])
	{
		LogInfo("Callback from dxgi.dll wrapper: requesting real ID3D11Device handle from %p\n", *ppvObj);

		D3D11Wrapper::ID3D11Device *p = (D3D11Wrapper::ID3D11Device*) D3D11Wrapper::ID3D11Device::m_List.GetDataPtr(*ppvObj);
		if (p)
		{
			LogInfo("  given pointer was already the real device.\n");
		}
		else
		{
			*ppvObj = ((D3D11Wrapper::ID3D11Device *)*ppvObj)->m_pUnk;
		}
		LogInfo("  returning handle = %p\n", *ppvObj);

		return 0x13bc7e31;
	}
	else if (riid.Data1 == m2.Data1 && riid.Data2 == m2.Data2 && riid.Data3 == m2.Data3 &&
		riid.Data4[0] == m2.Data4[0] && riid.Data4[1] == m2.Data4[1] && riid.Data4[2] == m2.Data4[2] && riid.Data4[3] == m2.Data4[3] &&
		riid.Data4[4] == m2.Data4[4] && riid.Data4[5] == m2.Data4[5] && riid.Data4[6] == m2.Data4[6] && riid.Data4[7] == m2.Data4[7])
	{
		LogDebug("Callback from dxgi.dll wrapper: notification %s received\n", typeid(*ppvObj).name());

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
		LogInfo("Callback from dxgi.dll wrapper: screen resolution width=%d, height=%d received\n",
			info->width, info->height);

		G->mSwapChainInfo = *info;
		return 0x13bc7e31;
	}

	LogDebug("QueryInterface request for %08lx-%04hx-%04hx-%02hhx%02hhx-%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx on %p\n",
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
	if (d3d10device) LogDebug("  9b7e4c0f-342c-4106-a19f-4f2704f689f0 = ID3D10Device\n");
	if (d3d10multithread) LogDebug("  9b7e4e00-342c-4106-a19f-4f2704f689f0 = ID3D10Multithread\n");
	if (dxgidevice) LogDebug("  54ec77fa-1377-44e6-8c32-88fd5f44c84c = IDXGIDevice\n");
	if (dxgidevice1) LogDebug("  77db970f-6276-48ba-ba28-070143b4392c = IDXGIDevice1\n");
	if (dxgidevice2) LogDebug("  05008617-fbfd-4051-a790-144884b4f6a9 = IDXGIDevice2\n");
	/*
	if (unknown1) LogInfo("  7abb6563-02bc-47c4-8ef9-acc4795edbcf = undocumented. Forcing fail.\n");
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
			LogDebug("  interface replaced with ID3D11Device wrapper. Interface counter=%d, wrapper counter=%d, wrapper internal counter = %d\n", cnt, p1->m_ulRef, cnt2);
		}
		D3D11Wrapper::ID3D11DeviceContext *p2 = (D3D11Wrapper::ID3D11DeviceContext*) D3D11Wrapper::ID3D11DeviceContext::m_List.GetDataPtr(*ppvObj);
		if (p2)
		{
			unsigned long cnt = ((IDirect3DUnknown*)*ppvObj)->Release();
			*ppvObj = p2;
			unsigned long cnt2 = p2->AddRef();
			LogDebug("  interface replaced with ID3D11DeviceContext wrapper. Interface counter=%d, wrapper counter=%d, wrapper internal counter = %d\n", cnt, p2->m_ulRef, cnt2);
		}
		D3D11Wrapper::IDXGIDevice2 *p3 = (D3D11Wrapper::IDXGIDevice2*) D3D11Wrapper::IDXGIDevice2::m_List.GetDataPtr(*ppvObj);
		if (p3)
		{
			unsigned long cnt = ((IDirect3DUnknown*)*ppvObj)->Release();
			*ppvObj = p3;
			unsigned long cnt2 = p3->AddRef();
			LogDebug("  interface replaced with IDXGIDevice2 wrapper. Interface counter=%d, wrapper counter=%d, wrapper internal counter = %d\n", cnt, p3->m_ulRef, cnt2);
		}
		D3D11Wrapper::ID3D10Device *p4 = (D3D11Wrapper::ID3D10Device*) D3D11Wrapper::ID3D10Device::m_List.GetDataPtr(*ppvObj);
		if (p4)
		{
			unsigned long cnt = ((IDirect3DUnknown*)*ppvObj)->Release();
			*ppvObj = p4;
			unsigned long cnt2 = p4->AddRef();
			LogDebug("  interface replaced with ID3D10Device wrapper. Interface counter=%d, wrapper counter=%d, wrapper internal counter = %d\n", cnt, p4->m_ulRef, cnt2);
		}
		D3D11Wrapper::ID3D10Multithread *p5 = (D3D11Wrapper::ID3D10Multithread*) D3D11Wrapper::ID3D10Multithread::m_List.GetDataPtr(*ppvObj);
		if (p5)
		{
			unsigned long cnt = ((IDirect3DUnknown*)*ppvObj)->Release();
			*ppvObj = p5;
			unsigned long cnt2 = p5->AddRef();
			LogDebug("  interface replaced with ID3D10Multithread wrapper. Interface counter=%d, wrapper counter=%d\n", cnt, p5->m_ulRef);
		}
		if (!p1 && !p2 && !p3 && !p4 && !p5)
		{
			// Check for IDXGIDevice, IDXGIDevice1 or IDXGIDevice2 cast.
			if (dxgidevice || dxgidevice1 || dxgidevice2)
			{
				// Cast again, but always use IDXGIDevice2 interface.
				D3D11Base::IDXGIDevice *oldDevice = (D3D11Base::IDXGIDevice *)*ppvObj;
				LogDebug("  releasing received IDXGIDevice, handle=%p. Querying IDXGIDevice2 interface.\n", *ppvObj);

				oldDevice->Release();
				const IID IID_IGreet = { 0x7A5E6E81, 0x3DF8, 0x11D3, { 0x90, 0x3D, 0x00, 0x10, 0x5A, 0xA4, 0x5B, 0xDC } };
				const IID IDXGIDevice2 = { 0x05008617, 0xfbfd, 0x4051, { 0xa7, 0x90, 0x14, 0x48, 0x84, 0xb4, 0xf6, 0xa9 } };
				hr = m_pUnk->QueryInterface(IDXGIDevice2, ppvObj);
				if (hr != S_OK)
				{
					LogInfo("  error querying IDXGIDevice2 interface: %x. Trying IDXGIDevice1.\n", hr);

					const IID IDXGIDevice1 = { 0x77db970f, 0x6276, 0x48ba, { 0xba, 0x28, 0x07, 0x01, 0x43, 0xb4, 0x39, 0x2c } };
					hr = m_pUnk->QueryInterface(IDXGIDevice1, ppvObj);
					if (hr != S_OK)
					{
						LogInfo("  error querying IDXGIDevice1 interface: %x. Trying IDXGIDevice.\n", hr);

						const IID IDXGIDevice = { 0x54ec77fa, 0x1377, 0x44e6, { 0x8c, 0x32, 0x88, 0xfd, 0x5f, 0x44, 0xc8, 0x4c } };
						hr = m_pUnk->QueryInterface(IDXGIDevice, ppvObj);
						if (hr != S_OK)
						{
							LogInfo("  fatal error querying IDXGIDevice interface: %x.\n", hr);

							return E_OUTOFMEMORY;
						}
					}
				}
				D3D11Base::IDXGIDevice2 *origDevice = (D3D11Base::IDXGIDevice2 *)*ppvObj;
				D3D11Wrapper::IDXGIDevice2 *wrapper = D3D11Wrapper::IDXGIDevice2::GetDirectDevice2(origDevice);
				if (wrapper == NULL)
				{
					LogInfo("  error allocating IDXGIDevice2 wrapper.\n");

					origDevice->Release();
					return E_OUTOFMEMORY;
				}
				*ppvObj = wrapper;
				LogDebug("  interface replaced with IDXGIDevice2 wrapper, original device handle=%p. Wrapper counter=%d\n",
					origDevice, wrapper->m_ulRef);
			}
			// Check for DirectX10 cast.
			if (d3d10device)
			{
				D3D11Base::ID3D10Device *origDevice = (D3D11Base::ID3D10Device *)*ppvObj;
				D3D11Wrapper::ID3D10Device *wrapper = D3D11Wrapper::ID3D10Device::GetDirect3DDevice(origDevice);
				if (wrapper == NULL)
				{
					LogInfo("  error allocating ID3D10Device wrapper.\n");

					origDevice->Release();
					return E_OUTOFMEMORY;
				}
				*ppvObj = wrapper;
				LogDebug("  interface replaced with ID3D10Device wrapper. Wrapper counter=%d\n", wrapper->m_ulRef);
			}
			if (d3d10multithread)
			{
				D3D11Base::ID3D10Multithread *origDevice = (D3D11Base::ID3D10Multithread *)*ppvObj;
				D3D11Wrapper::ID3D10Multithread *wrapper = D3D11Wrapper::ID3D10Multithread::GetDirect3DMultithread(origDevice);
				if (wrapper == NULL)
				{
					LogInfo("  error allocating ID3D10Multithread wrapper.\n");

					origDevice->Release();
					return E_OUTOFMEMORY;
				}
				*ppvObj = wrapper;
				LogDebug("  interface replaced with ID3D10Multithread wrapper. Wrapper counter=%d\n", wrapper->m_ulRef);
			}
		}
	}
	LogDebug("  result = %x, handle = %p\n", hr, *ppvObj);

	return hr;
}

static D3D11Base::IDXGIAdapter *ReplaceAdapter(D3D11Base::IDXGIAdapter *wrapper)
{
	if (!wrapper)
		return wrapper;
	LogInfo("  checking for adapter wrapper, handle = %p\n", wrapper);
	IID marker = { 0x017b2e72ul, 0xbcde, 0x9f15, { 0xa1, 0x2b, 0x3c, 0x4d, 0x5e, 0x6f, 0x70, 0x00 } };
	D3D11Base::IDXGIAdapter *realAdapter;
	if (wrapper->GetParent(marker, (void **)&realAdapter) == 0x13bc7e32)
	{
		LogInfo("    wrapper found. replacing with original handle = %p\n", realAdapter);

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
	LogInfo("D3D11CreateDevice called with adapter = %p\n", pAdapter);

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
		LogInfo("  failed with HRESULT=%x\n", ret);

		return ret;
	}

	D3D11Wrapper::ID3D11Device *wrapper = D3D11Wrapper::ID3D11Device::GetDirect3DDevice(origDevice);
	if (wrapper == NULL)
	{
		LogInfo("  error allocating wrapper.\n");

		origDevice->Release();
		origContext->Release();

		return E_OUTOFMEMORY;
	}
	if (ppDevice)
		*ppDevice = wrapper;

	D3D11Wrapper::ID3D11DeviceContext *wrapper2 = D3D11Wrapper::ID3D11DeviceContext::GetDirect3DDeviceContext(origContext);
	if (wrapper2 == NULL)
	{
		LogInfo("  error allocating wrapper2.\n");

		origDevice->Release();
		origContext->Release();
		return E_OUTOFMEMORY;
	}
	if (ppImmediateContext)
		*ppImmediateContext = wrapper2;

	LogInfo("  returns result = %x, device handle = %p, device wrapper = %p, context handle = %p, context wrapper = %p\n", ret, origDevice, wrapper, origContext, wrapper2);

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
	LogInfo("D3D11CreateDeviceAndSwapChain called with adapter = %p\n", pAdapter);
	if (pSwapChainDesc) LogInfo("  Windowed = %d\n", pSwapChainDesc->Windowed);
	if (pSwapChainDesc) LogInfo("  Width = %d\n", pSwapChainDesc->BufferDesc.Width);
	if (pSwapChainDesc) LogInfo("  Height = %d\n", pSwapChainDesc->BufferDesc.Height);
	if (pSwapChainDesc) LogInfo("  Refresh rate = %f\n",
		(float)pSwapChainDesc->BufferDesc.RefreshRate.Numerator / (float)pSwapChainDesc->BufferDesc.RefreshRate.Denominator);

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
		LogInfo("  failed with HRESULT=%x\n", ret);
		return ret;
	}

	LogInfo("  CreateDeviceAndSwapChain returned device handle = %p, context handle = %p\n", origDevice, origContext);

	if (!origDevice || !origContext)
		return ret;

	D3D11Wrapper::ID3D11Device *wrapper = D3D11Wrapper::ID3D11Device::GetDirect3DDevice(origDevice);
	if (wrapper == NULL)
	{
		LogInfo("  error allocating wrapper.\n");

		origDevice->Release();
		origContext->Release();
		return E_OUTOFMEMORY;
	}
	if (ppDevice)
		*ppDevice = wrapper;

	D3D11Wrapper::ID3D11DeviceContext *wrapper2 = D3D11Wrapper::ID3D11DeviceContext::GetDirect3DDeviceContext(origContext);
	if (wrapper2 == NULL)
	{
		LogInfo("  error allocating wrapper2.\n");

		origDevice->Release();
		origContext->Release();
		return E_OUTOFMEMORY;
	}
	if (ppImmediateContext)
		*ppImmediateContext = wrapper2;

	LogInfo("  returns result = %x, device handle = %p, device wrapper = %p, context handle = %p, context wrapper = %p\n", ret, origDevice, wrapper, origContext, wrapper2);

	return ret;
}

int WINAPI D3DKMTGetDeviceState(int a)
{
	InitD311();
	LogInfo("D3DKMTGetDeviceState called.\n");

	return (*_D3DKMTGetDeviceState)(a);
}

int WINAPI D3DKMTOpenAdapterFromHdc(int a)
{
	InitD311();
	LogInfo("D3DKMTOpenAdapterFromHdc called.\n");

	return (*_D3DKMTOpenAdapterFromHdc)(a);
}

int WINAPI D3DKMTOpenResource(int a)
{
	InitD311();
	LogInfo("D3DKMTOpenResource called.\n");

	return (*_D3DKMTOpenResource)(a);
}

int WINAPI D3DKMTQueryResourceInfo(int a)
{
	InitD311();
	LogInfo("D3DKMTQueryResourceInfo called.\n");

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

void D3D11Wrapper::NvAPIOverride()
{
	// Override custom settings.
	const D3D11Base::StereoHandle id1 = (D3D11Base::StereoHandle)0x77aa8ebc;
	float id2 = 1.23f;
	if (D3D11Base::NvAPI_Stereo_GetConvergence(id1, &id2) != 0xeecc34ab)
	{
		LogDebug("  overriding NVAPI wrapper failed.\n");
	}
}

#include "Direct3D11Device.h"
#include "Direct3D11Context.h"
#include "DirectDXGIDevice.h"
#include "../DirectX10/Direct3D10Device.h"
