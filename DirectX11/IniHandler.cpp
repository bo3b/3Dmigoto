#include "IniHandler.h"

#include <string>

#include "log.h"
#include "Globals.h"
#include "Override.h"
#include "Hunting.h"


// Case insensitive version of less comparitor. This is used to create case
// insensitive sets of section names in the ini so we can detect duplicate
// sections that vary only by case, e.g. [Key1] and [KEY1], as these are
// treated equivelent by the GetPrivateProfileXXX APIs. It also means that the
// set will be sorted in a case insensitive manner making it easy to iterate
// over all section names starting with a given case insensitive prefix.
struct WStringInsensitiveLess {
	bool operator() (const wstring &x, const wstring &y) const
	{
		return _wcsicmp(x.c_str(), y.c_str()) < 0;
	}
};

// std::set is used so this is sorted for iterating over a prefix:
typedef std::set<wstring, WStringInsensitiveLess> IniSections;

// Returns an iterator to the first element in a set that does not begin with
// prefix in a case insensitive way. Combined with set::lower_bound, this can
// be used to iterate over all elements in the sections set that begin with a
// given prefix.
IniSections::iterator prefix_upper_bound(IniSections &sections, wstring &prefix)
{
	IniSections::iterator i;

	for (i = sections.lower_bound(prefix); i != sections.end(); i++) {
		if (_wcsnicmp(i->c_str(), prefix.c_str(), prefix.length()) > 0)
			return i;
	}

	return sections.end();
}

void RegisterPresetKeyBindings(IniSections &sections, LPCWSTR iniFile)
{
	KeyOverrideType type;
	wchar_t key[MAX_PATH];
	wchar_t buf[MAX_PATH];
	KeyOverrideBase *preset;
	int delay, release_delay;
	IniSections::iterator lower, upper, i;

	lower = sections.lower_bound(wstring(L"Key"));
	upper = prefix_upper_bound(sections, wstring(L"Key"));

	for (i = lower; i != upper; i++) {
		const wchar_t *id = i->c_str();

		LogInfoW(L"[%s]\n", id);

		if (!GetPrivateProfileString(id, L"Key", 0, key, MAX_PATH, iniFile))
			break;

		type = KeyOverrideType::ACTIVATE;

		if (GetPrivateProfileString(id, L"type", 0, buf, MAX_PATH, iniFile)) {
			// XXX: hold & toggle types will restore the previous
			// settings on release - there's possibly also another
			// use case for setting a specific profile instead.
			type = lookup_enum_val<wchar_t *, KeyOverrideType>
				(KeyOverrideTypeNames, buf, KeyOverrideType::INVALID);
			if (type == KeyOverrideType::INVALID) {
				LogInfoW(L"  WARNING: UNKNOWN KEY BINDING TYPE %s\n", buf);
				BeepFailure2();
			}
			else {
				LogInfoW(L"  type=%s\n", buf);
			}
		}

		delay = GetPrivateProfileInt(id, L"delay", 0, iniFile);
		release_delay = GetPrivateProfileInt(id, L"release_delay", 0, iniFile);

		if (type == KeyOverrideType::CYCLE)
			preset = new KeyOverrideCycle();
		else
			preset = new KeyOverride(type);
		preset->ParseIniSection(id, iniFile);

		RegisterKeyBinding(L"Key", key, preset, 0, delay, release_delay);
	}
}


void ParseShaderOverrideSections(IniSections &sections, LPCWSTR iniFile)
{
	IniSections::iterator lower, upper, i;
	wchar_t setting[MAX_PATH];
	const wchar_t *id;
	ShaderOverride *override;
	UINT64 hash, hash2;

	// Lock entire routine. This can be re-inited live.  These shaderoverrides
	// are unlikely to be changing much, but for consistency.
	if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);

	G->mShaderOverrideMap.clear();

	lower = sections.lower_bound(wstring(L"ShaderOverride"));
	upper = prefix_upper_bound(sections, wstring(L"ShaderOverride"));
	for (i = lower; i != upper; i++) {
		id = i->c_str();

		LogInfoW(L"[%s]\n", id);

		if (!GetPrivateProfileString(id, L"Hash", 0, setting, MAX_PATH, iniFile))
			break;
		swscanf_s(setting, L"%16llx", &hash);
		LogInfo("  Hash=%16llx\n", hash);

		if (G->mShaderOverrideMap.count(hash)) {
			LogInfo("  WARNING: Duplicate ShaderOverride hash: %16llx\n", hash);
			BeepFailure2();
		}
		override = &G->mShaderOverrideMap[hash];

		if (GetPrivateProfileString(id, L"Separation", 0, setting, MAX_PATH, iniFile))
		{
			swscanf_s(setting, L"%e", &override->separation);
			LogInfo("  Separation=%f\n", override->separation);
		}
		if (GetPrivateProfileString(id, L"Convergence", 0, setting, MAX_PATH, iniFile))
		{
			swscanf_s(setting, L"%e", &override->convergence);
			LogInfo("  Convergence=%f\n", override->convergence);
		}
		if (GetPrivateProfileString(id, L"Handling", 0, setting, MAX_PATH, iniFile)) {
			if (!wcscmp(setting, L"skip")) {
				override->skip = true;
				LogInfo("  Handling=skip\n");
			}
			else {
				LogInfoW(L"  WARNING: Unknown handling type \"%s\"\n", setting);
				BeepFailure2();
			}
		}
		if (GetPrivateProfileString(id, L"depth_filter", 0, setting, MAX_PATH, iniFile)) {
			override->depth_filter = lookup_enum_val<wchar_t *, DepthBufferFilter>
				(DepthBufferFilterNames, setting, DepthBufferFilter::INVALID);
			if (override->depth_filter == DepthBufferFilter::INVALID) {
				LogInfoW(L"  WARNING: Unknown depth_filter \"%s\"\n", setting);
				override->depth_filter = DepthBufferFilter::NONE;
				BeepFailure2();
			}
			else {
				LogInfoW(L"  depth_filter=%s\n", setting);
			}
		}

		// TODO: make this a list and/or add a way to pass in an
		// identifier for the partner shader
		if (GetPrivateProfileString(id, L"partner", 0, setting, MAX_PATH, iniFile)) {
			swscanf_s(setting, L"%16llx", &override->partner_hash);
			LogInfo("  partner=%16llx\n", override->partner_hash);
		}

#if 0 /* Iterations are broken since we no longer use present() */
		if (GetPrivateProfileString(id, L"Iteration", 0, setting, MAX_PATH, iniFile))
		{
			// XXX: This differs from the TextureOverride
			// iterations, in that there can only be one iteration
			// here - not sure why.
			int iteration;
			override->iterations.clear();
			override->iterations.push_back(0);
			swscanf_s(setting, L"%d", &iteration);
			LogInfo("  Iteration=%d\n", iteration);
			override->iterations.push_back(iteration);
		}
#endif
		if (GetPrivateProfileString(id, L"IndexBufferFilter", 0, setting, MAX_PATH, iniFile))
		{
			swscanf_s(setting, L"%16llx", &hash2);
			LogInfo("  IndexBufferFilter=%16llx\n", hash2);
			override->indexBufferFilter.push_back(hash2);
		}
	}
	if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
}



void ParseTextureOverrideSections(IniSections &sections, LPCWSTR iniFile)
{
	IniSections::iterator lower, upper, i;
	wchar_t setting[MAX_PATH];
	const wchar_t *id;
	TextureOverride *override;
	UINT64 hash;

	// Lock entire routine, this can be re-inited.  These shaderoverrides
	// are unlikely to be changing much, but for consistency.
	if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);

	G->mTextureOverrideMap.clear();

	lower = sections.lower_bound(wstring(L"TextureOverride"));
	upper = prefix_upper_bound(sections, wstring(L"TextureOverride"));

	for (i = lower; i != upper; i++) {
		id = i->c_str();

		LogInfoW(L"[%s]\n", id);

		if (!GetPrivateProfileString(id, L"Hash", 0, setting, MAX_PATH, iniFile))
			break;
		swscanf_s(setting, L"%16llx", &hash);
		LogInfo("  Hash=%16llx\n", hash);

		if (G->mTextureOverrideMap.count(hash)) {
			LogInfo("  WARNING: Duplicate TextureOverride hash: %16llx\n", hash);
			BeepFailure2();
		}
		override = &G->mTextureOverrideMap[hash];

		int stereoMode = GetPrivateProfileInt(id, L"StereoMode", -1, iniFile);
		if (stereoMode >= 0)
		{
			override->stereoMode = stereoMode;
			LogInfo("  StereoMode=%d\n", stereoMode);
		}
		int texFormat = GetPrivateProfileInt(id, L"Format", -1, iniFile);
		if (texFormat >= 0)
		{
			override->format = texFormat;
			LogInfo("  Format=%d\n", texFormat);
		}
#if 0 /* Iterations are broken since we no longer use present() */
		if (GetPrivateProfileString(id, L"Iteration", 0, setting, MAX_PATH, iniFile))
		{
			// TODO: This supports more iterations than the
			// ShaderOverride iteration parameter, and it's not
			// clear why there is a difference. This seems like the
			// better way, but should change it to use my list
			// parsing code rather than hard coding a maximum of 10
			// supported iterations.
			override->iterations.clear();
			override->iterations.push_back(0);
			int id[10] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
			swscanf_s(setting, L"%d,%d,%d,%d,%d,%d,%d,%d,%d,%d", id + 0, id + 1, id + 2, id + 3, id + 4, id + 5, id + 6, id + 7, id + 8, id + 9);
			for (int j = 0; j < 10; ++j)
			{
				if (id[j] <= 0) break;
				override->iterations.push_back(id[j]);
				LogInfo("  Iteration=%d\n", id[j]);
	}
}
#endif
	}
	if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
}

static void GetIniSections(IniSections &sections, wchar_t *iniFile)
{
	wchar_t *buf, *ptr;
	// Don't set initial buffer size too low (< 2?) - GetPrivateProfileSectionNames
	// returns 0 instead of the documented (buf_size - 2) in that case.
	int buf_size = 256;
	DWORD result;

	sections.clear();

	while (true) {
		buf = new wchar_t[buf_size];
		if (!buf)
			return;

		result = GetPrivateProfileSectionNames(buf, buf_size, iniFile);
		if (result != buf_size - 2)
			break;

		delete[] buf;
		buf_size <<= 1;
	}

	for (ptr = buf; *ptr; ptr++) {
		if (sections.count(ptr)) {
			LogInfoW(L"WARNING: Duplicate section found in d3dx.ini: [%s]\n", ptr);
			BeepFailure2();
		}
		sections.insert(ptr);
		for (; *ptr; ptr++) {}
	}

	delete[] buf;
}

// Check the Stereo availability. If stereo is disabled we otherwise will crash 
// when trying to create stereo texture.  This should be more graceful now.

NvAPI_Status CheckStereo()
{
	NvU8 isStereoEnabled;
	NvAPI_Status status = NvAPI_Stereo_IsEnabled(&isStereoEnabled);
	if (status != NVAPI_OK)
	{
		// GeForce Stereoscopic 3D driver is not installed on the system
		NvAPI_ShortString nvDescription;
		NvAPI_GetErrorMessage(status, nvDescription);
		LogInfo("  stereo init failed: no stereo driver detected- %s\n", nvDescription);
		return status;
	}

	// Stereo is available but not enabled, let's enable it if specified.
	if (!isStereoEnabled)
	{
		LogInfo("  stereo available but disabled.\n");

		if (!G->gForceStereo)
			return NVAPI_STEREO_NOT_ENABLED;

		status = NvAPI_Stereo_Enable();
		if (status != NVAPI_OK)
		{
			NvAPI_ShortString nvDescription;
			NvAPI_GetErrorMessage(status, nvDescription);
			LogInfo("   force enabling stereo failed- %s\n", nvDescription);
			return status;
		}
	}

	if (G->gCreateStereoProfile)
	{
		LogInfo("  enabling registry profile.\n");

		NvAPI_Stereo_CreateConfigurationProfileRegistryKey(NVAPI_STEREO_DEFAULT_REGISTRY_PROFILE);
	}

	return NVAPI_OK;
}



void FlagConfigReload(HackerDevice *device, void *private_data)
{
	// When we reload the configuration, we are going to clear the existing
	// key bindings and reassign them. Naturally this is not a safe thing
	// to do from inside a key binding callback, so we just set a flag and
	// do this after the input subsystem has finished dispatching calls.
	G->gReloadConfigPending = true;
}


void LoadConfigFile()
{
	wchar_t iniFile[MAX_PATH];
	wchar_t setting[MAX_PATH];
	IniSections sections;
	IniSections::iterator lower, upper, i;

	G->gInitialized = true;

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

	GetIniSections(sections, iniFile);

	G->gLogInput = GetPrivateProfileInt(L"Logging", L"input", 0, iniFile) == 1;
	gLogDebug = GetPrivateProfileInt(L"Logging", L"debug", 0, iniFile) == 1;

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
		if (G->gLogInput) LogInfo("  input=1\n");
		LogDebug("  debug=1\n");
		if (unbuffered != -1) LogInfo("  unbuffered=1  return: %d\n", unbuffered);
		if (affinity != -1) LogInfo("  force_cpu_affinity=1  return: %s\n", affinity ? "true" : "false");
		if (waitfordebugger) LogInfo("  waitfordebugger=1\n");
	}

	// [System]
	GetPrivateProfileString(L"System", L"proxy_d3d11", 0, G->CHAIN_DLL_PATH, MAX_PATH, iniFile);
	if (LogFile)
	{
		LogInfo("[System]\n");
		if (!G->CHAIN_DLL_PATH) LogInfoW(L"  proxy_d3d11=%s\n", G->CHAIN_DLL_PATH);
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
	GetPrivateProfileString(L"Rendering", L"override_directory", 0, G->SHADER_PATH, MAX_PATH, iniFile);
	if (G->SHADER_PATH[0])
	{
		while (G->SHADER_PATH[wcslen(G->SHADER_PATH) - 1] == L' ')
			G->SHADER_PATH[wcslen(G->SHADER_PATH) - 1] = 0;
		if (G->SHADER_PATH[1] != ':' && G->SHADER_PATH[0] != '\\')
		{
			GetModuleFileName(0, setting, MAX_PATH);
			wcsrchr(setting, L'\\')[1] = 0;
			wcscat(setting, G->SHADER_PATH);
			wcscpy(G->SHADER_PATH, setting);
		}
		// Create directory?
		CreateDirectory(G->SHADER_PATH, 0);
	}
	GetPrivateProfileString(L"Rendering", L"cache_directory", 0, G->SHADER_CACHE_PATH, MAX_PATH, iniFile);
	if (G->SHADER_CACHE_PATH[0])
	{
		while (G->SHADER_CACHE_PATH[wcslen(G->SHADER_CACHE_PATH) - 1] == L' ')
			G->SHADER_CACHE_PATH[wcslen(G->SHADER_CACHE_PATH) - 1] = 0;
		if (G->SHADER_CACHE_PATH[1] != ':' && G->SHADER_CACHE_PATH[0] != '\\')
		{
			GetModuleFileName(0, setting, MAX_PATH);
			wcsrchr(setting, L'\\')[1] = 0;
			wcscat(setting, G->SHADER_CACHE_PATH);
			wcscpy(G->SHADER_CACHE_PATH, setting);
		}
		// Create directory?
		CreateDirectory(G->SHADER_CACHE_PATH, 0);
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
		if (G->SHADER_PATH[0])
			LogInfoW(L"  override_directory=%s\n", G->SHADER_PATH);
		if (G->SHADER_CACHE_PATH[0])
			LogInfoW(L"  cache_directory=%s\n", G->SHADER_CACHE_PATH);

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

	RegisterPresetKeyBindings(sections, iniFile);


	// Todo: Not sure this is best spot.
	G->ENABLE_TUNE = GetPrivateProfileInt(L"Hunting", L"tune_enable", 0, iniFile) == 1;
	if (GetPrivateProfileString(L"Hunting", L"tune_step", 0, setting, MAX_PATH, iniFile))
		swscanf_s(setting, L"%f", &G->gTuneStep);

	ParseShaderOverrideSections(sections, iniFile);
	ParseTextureOverrideSections(sections, iniFile);

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


void ReloadConfig(HackerDevice *device)
{
	ID3D11DeviceContext* realContext; device->GetImmediateContext(&realContext);
	D3D11_MAPPED_SUBRESOURCE mappedResource;

	LogInfo("Reloading d3dx.ini (EXPERIMENTAL)...\n");

	G->gReloadConfigPending = false;

	if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);

	// Clear the key bindings. There may be other things that need to be
	// cleared as well, but for the sake of clarity I'd rather clear as
	// many as possible inside LoadConfigFile() where they are set.
	ClearKeyBindings();

	// Reset the counters on the global parameter save area:
	OverrideSave.Reset(device);

	LoadConfigFile();

	if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);

	// Update the iniParams resource from the config file:
	realContext->Map(device->mIniTexture, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	memcpy(mappedResource.pData, &G->iniParams, sizeof(G->iniParams));
	realContext->Unmap(device->mIniTexture, 0);
}
