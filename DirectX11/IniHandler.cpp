#include "IniHandler.h"

#include <algorithm>
#include <string>
#include <strsafe.h>

#include "log.h"
#include "Globals.h"
#include "Override.h"
#include "Hunting.h"

// List all the section prefixes which may contain a command list here and
// whether they are a prefix or an exact match. Listing a section here will not
// automatically treat it as a command list (call ParseCommandList on it to do
// that), but will mean that it will not be checked for duplicate keys (since
// it is legal for a command list to contain duplicate keys).
//
// Keys within these sections that are not part of the command list must be
// explicitly whitelisted, and these keys will be checked for duplicates by
// ParseCommandList.
//
// ParseCommandList will terminate the program if it is called on a section not
// listed here to make sure we never forget to update this.
struct CommandListSection {
	wchar_t *section;
	bool prefix;
};
static CommandListSection CommandListSections[] = {
	{L"ShaderOverride", true},
	{L"TextureOverride", true},
	{L"Present", false},
};

bool IsCommandListSection(const wchar_t *section)
{
	size_t len;
	int i;

	for (i = 0; i < ARRAYSIZE(CommandListSections); i++) {
		if (CommandListSections[i].prefix) {
			len = wcslen(CommandListSections[i].section);
			if (!_wcsnicmp(section, CommandListSections[i].section, len))
				return true;
		} else {
			if (!_wcsicmp(section, CommandListSections[i].section))
				return true;
		}
	}

	return false;
}

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

// Whereas settings within a section are in the same order they were in the ini
// file. This will become more important as shader overrides gains more
// functionality and dependencies between different features form:
typedef std::vector<std::pair<wstring, wstring>> IniSection;

// Returns an iterator to the first element in a set that does not begin with
// prefix in a case insensitive way. Combined with set::lower_bound, this can
// be used to iterate over all elements in the sections set that begin with a
// given prefix.
static IniSections::iterator prefix_upper_bound(IniSections &sections, wstring &prefix)
{
	IniSections::iterator i;

	for (i = sections.lower_bound(prefix); i != sections.end(); i++) {
		if (_wcsnicmp(i->c_str(), prefix.c_str(), prefix.length()) > 0)
			return i;
	}

	return sections.end();
}

static void GetIniSection(IniSection &key_vals, const wchar_t *section, wchar_t *iniFile)
{
	wchar_t *buf, *kptr, *vptr;
	// Don't set initial buffer size too low (< 2?) - GetPrivateProfileSection
	// returns 0 instead of the documented (buf_size - 2) in that case.
	int buf_size = 256;
	DWORD result;
	IniSections keys;
	bool warn_duplicates = true;

	// Sections that utilise a command list are allowed to have duplicate
	// keys, while other sections are not. The command list parser will
	// still check for duplicate keys that are not part of the command
	// list.
	if (IsCommandListSection(section))
		warn_duplicates = false;

	key_vals.clear();

	while (true) {
		buf = new wchar_t[buf_size];
		if (!buf)
			return;

		result = GetPrivateProfileSection(section, buf, buf_size, iniFile);
		if (result != buf_size - 2)
			break;

		delete[] buf;
		buf_size <<= 1;
	}

	for (kptr = buf; *kptr; kptr++) {
		for (vptr = kptr; *vptr && *vptr != L'='; vptr++) {}
		if (*vptr != L'=') {
			LogInfoW(L"WARNING: Malformed line in d3dx.ini: [%s] \"%s\"\n", section, kptr);
			BeepFailure2();
			kptr = vptr;
			continue;
		}

		*vptr = L'\0';
		vptr++;

		if (warn_duplicates) {
			if (keys.count(kptr)) {
				LogInfoW(L"WARNING: Duplicate key found in d3dx.ini: [%s] %s\n", section, kptr);
				BeepFailure2();
			}
			keys.insert(kptr);
		}
		key_vals.emplace_back(kptr, vptr);
		for (kptr = vptr; *kptr; kptr++) {}
	}

	delete[] buf;
}

static void GetIniSections(IniSections &sections, wchar_t *iniFile)
{
	wchar_t *buf, *ptr;
	// Don't set initial buffer size too low (< 2?) - GetPrivateProfileSectionNames
	// returns 0 instead of the documented (buf_size - 2) in that case.
	int buf_size = 256;
	DWORD result;
	IniSection section;

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

		// Call GetIniSection to warn about any malformed lines or
		// duplicate keys in the section, discarding the result.
		GetIniSection(section, ptr, iniFile);
		section.clear();

		for (; *ptr; ptr++) {}
	}

	delete[] buf;
}


static void RegisterPresetKeyBindings(IniSections &sections, LPCWSTR iniFile)
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

static void ParseResourceSections(IniSections &sections, LPCWSTR iniFile)
{
	IniSections::iterator lower, upper, i;
	wstring resource_id;
	CustomResource *custom_resource;
	wchar_t setting[MAX_PATH], path[MAX_PATH];

	customResources.clear();

	lower = sections.lower_bound(wstring(L"Resource"));
	upper = prefix_upper_bound(sections, wstring(L"Resource"));
	for (i = lower; i != upper; i++) {
		LogInfoW(L"[%s]\n", i->c_str());

		// Convert section name to lower case so our keys will be
		// consistent in the unordered_map:
		resource_id = *i;
		std::transform(resource_id.begin(), resource_id.end(), resource_id.begin(), ::towlower);

		// Empty Resource sections are valid (think of them as a
		// sort of variable declaration), so explicitly construct a
		// CustomResource for each one. Use the [] operator so the
		// default constructor will be used:
		custom_resource = &customResources[resource_id];

		custom_resource->max_copies_per_frame =
			GetPrivateProfileInt(i->c_str(), L"max_copies_per_frame", 0, iniFile);
		if (custom_resource->max_copies_per_frame)
			LogInfo("  max_copies_per_frame=%d\n", custom_resource->max_copies_per_frame);

		if (GetPrivateProfileString(i->c_str(), L"filename", 0, setting, MAX_PATH, iniFile)) {
			LogInfoW(L"  filename=%s\n", setting);
			GetModuleFileName(0, path, MAX_PATH);
			wcsrchr(path, L'\\')[1] = 0;
			wcscat(path, setting);
			custom_resource->filename = path;
		}
	}
}

// This tries to parse each line in a section in order as part of a command
// list. A list of keys that may be parsed elsewhere can be passed in so that
// it can warn about unrecognised keys and detect duplicate keys that aren't
// part of the command list.
static void ParseCommandList(const wchar_t *id, wchar_t *iniFile,
		CommandList *pre_command_list, CommandList *post_command_list,
		wchar_t *whitelist[])
{
	IniSection section;
	IniSection::iterator entry;
	wstring *key, *val;
	const wchar_t *key_ptr;
	CommandList *command_list, *explicit_command_list = NULL;
	IniSections whitelisted_keys;
	int i;

	// Safety check to make sure we are keeping the command list section
	// list up to date:
	if (!IsCommandListSection(id)) {
		LogInfoW(L"BUG: ParseCommandList() called on a section not in the CommandListSections list: %s\n", id);
		DoubleBeepExit();
	}

	GetIniSection(section, id, iniFile);
	for (entry = section.begin(); entry < section.end(); entry++) {
		key = &entry->first;
		val = &entry->second;

		// Convert key to lower case since ini files are supposed to be
		// case insensitive:
		std::transform(key->begin(), key->end(), key->begin(), ::towlower);

		// Skip any whitelisted entries that are parsed elsewhere.
		if (whitelist) {
			for (i = 0; whitelist[i]; i++) {
				if (!key->compare(whitelist[i]))
					break;
			}
			if (whitelist[i]) {
				// Entry is whitelisted and will be parsed
				// elsewhere. Sections with command lists are
				// allowed duplicate keys *except for these
				// whitelisted entries*, so check for
				// duplicates here:
				if (whitelisted_keys.count(key->c_str())) {
					LogInfoW(L"WARNING: Duplicate non-command list key found in d3dx.ini: [%s] %s\n", id, key->c_str());
					BeepFailure2();
				}
				whitelisted_keys.insert(key->c_str());

				continue;
			}
		}

		command_list = pre_command_list;
		key_ptr = key->c_str();
		if (post_command_list) {
			if (!key->compare(0, 5, L"post ")) {
				key_ptr += 5;
				command_list = post_command_list;
				explicit_command_list = post_command_list;
			} else if (!key->compare(0, 4, L"pre ")) {
				key_ptr += 4;
				explicit_command_list = pre_command_list;
			}
		}

		if (ParseCommandListGeneralCommands(key_ptr, val, explicit_command_list, pre_command_list, post_command_list))
			goto log_continue;

		if (ParseCommandListIniParamOverride(key_ptr, val, command_list))
			goto log_continue;

		if (ParseCommandListResourceCopyDirective(key_ptr, val, command_list))
			goto log_continue;

		LogInfoW(L"  WARNING: Unrecognised entry: %ls=%ls\n", key->c_str(), val->c_str());
		BeepFailure2();
		break;
log_continue:
		LogInfoW(L"  %ls=%s\n", key->c_str(), val->c_str());
	}
}

// List of keys in [ShaderOverride] sections that are processed in this
// function. Used by ParseCommandList to find any unrecognised lines.
wchar_t *ShaderOverrideIniKeys[] = {
	L"hash",
	L"separation",
	L"convergence",
	L"handling",
	L"depth_filter",
	L"partner",
	L"iteration",
	L"indexbufferfilter",
	L"analyse_options",
	L"fake_o0",
	NULL
};
static void ParseShaderOverrideSections(IniSections &sections, wchar_t *iniFile)
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

		// Simple partner shader filtering. Deprecated - more advanced
		// filtering can be achieved by setting an ini param in the
		// partner's [ShaderOverride] section.
		if (GetPrivateProfileString(id, L"partner", 0, setting, MAX_PATH, iniFile)) {
			swscanf_s(setting, L"%16llx", &override->partner_hash);
			LogInfo("  partner=%16llx\n", override->partner_hash);
		}

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
		
		if (GetPrivateProfileString(id, L"IndexBufferFilter", 0, setting, MAX_PATH, iniFile))
		{
			swscanf_s(setting, L"%16llx", &hash2);
			LogInfo("  IndexBufferFilter=%16llx\n", hash2);
			override->indexBufferFilter.push_back(hash2);
		}

		if (GetPrivateProfileString(id, L"analyse_options", 0, setting, MAX_PATH, iniFile)) {
			LogInfoW(L"  analyse_options=%s\n", setting);
			override->analyse_options = parse_enum_option_string<wchar_t *, FrameAnalysisOptions>
				(FrameAnalysisOptionNames, setting, NULL);
		}

		override->fake_o0 = GetPrivateProfileInt(id, L"fake_o0", 0, iniFile) == 1;
		if (override->fake_o0)
			LogInfo("  fake_o0=1\n");

		ParseCommandList(id, iniFile, &override->command_list, &override->post_command_list, ShaderOverrideIniKeys);
	}
	if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
}



// List of keys in [TextureOverride] sections that are processed in this
// function. Used by ParseCommandList to find any unrecognised lines.
wchar_t *TextureOverrideIniKeys[] = {
	L"hash",
	L"stereomode",
	L"format",
	L"iteration",
	L"analyse_options",
	L"filter_index",
	L"expand_region_copy",
	L"deny_cpu_read",
	NULL
};
static void ParseTextureOverrideSections(IniSections &sections, wchar_t *iniFile)
{
	IniSections::iterator lower, upper, i;
	wchar_t setting[MAX_PATH];
	const wchar_t *id;
	TextureOverride *override;
	uint32_t hash;

	// Lock entire routine, this can be re-inited.  These shaderoverrides
	// are unlikely to be changing much, but for consistency.
	if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);

	G->mTextureOverrideMap.clear();

	lower = sections.lower_bound(wstring(L"TextureOverride"));
	upper = prefix_upper_bound(sections, wstring(L"TextureOverride"));

	for (i = lower; i != upper; i++) 
	{
		id = i->c_str();

		LogInfoW(L"[%s]\n", id);

		if (!GetPrivateProfileString(id, L"Hash", 0, setting, MAX_PATH, iniFile))
			break;
		swscanf_s(setting, L"%8lx", &hash);
		LogInfo("  Hash=%08lx\n", hash);

		if (G->mTextureOverrideMap.count(hash)) {
			LogInfo("  WARNING: Duplicate TextureOverride hash: %08lx\n", hash);
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

		if (GetPrivateProfileString(id, L"analyse_options", 0, setting, MAX_PATH, iniFile)) {
			LogInfoW(L"  analyse_options=%s\n", setting);
			override->analyse_options = parse_enum_option_string<wchar_t *, FrameAnalysisOptions>
				(FrameAnalysisOptionNames, setting, NULL);
		}

		if (GetPrivateProfileString(id, L"filter_index", 0, setting, MAX_PATH, iniFile)) {
			swscanf_s(setting, L"%f", &override->filter_index);
			LogInfo("  filter_index=%f\n", override->filter_index);
		}

		override->expand_region_copy = GetPrivateProfileInt(id, L"expand_region_copy", 0, iniFile) == 1;
		override->deny_cpu_read = GetPrivateProfileInt(id, L"deny_cpu_read", 0, iniFile) == 1;

		ParseCommandList(id, iniFile, &override->command_list, &override->post_command_list, TextureOverrideIniKeys);
	}
	if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
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
	wchar_t iniFile[MAX_PATH], logFilename[MAX_PATH];
	wchar_t setting[MAX_PATH];
	IniSections sections;
	int i;

	G->gInitialized = true;

	GetModuleFileName(0, iniFile, MAX_PATH);
	wcsrchr(iniFile, L'\\')[1] = 0;
	wcscpy(logFilename, iniFile);
	wcscat(iniFile, L"d3dx.ini");
	wcscat(logFilename, L"d3d11_log.txt");

	// Log all settings that are _enabled_, in order, 
	// so that there is no question what settings we are using.

	// [Logging]
	if (GetPrivateProfileInt(L"Logging", L"calls", 1, iniFile))
	{
		if (!LogFile)
			LogFile = _wfsopen(logFilename, L"w", _SH_DENYNO);
		LogInfo("\nD3D11 DLL starting init - v %s - %s\n\n", VER_FILE_VERSION_STR, LogTime().c_str());
		LogInfo("----------- d3dx.ini settings -----------\n");
	}

	GetIniSections(sections, iniFile);

	G->gLogInput = GetPrivateProfileInt(L"Logging", L"input", 0, iniFile) == 1;
	gLogDebug = GetPrivateProfileInt(L"Logging", L"debug", 0, iniFile) == 1;

	// Unbuffered logging to remove need for fflush calls, and r/w access to make it easy
	// to open active files.
	int unbuffered = -1;
	if (LogFile && GetPrivateProfileInt(L"Logging", L"unbuffered", 0, iniFile))
	{
		unbuffered = setvbuf(LogFile, NULL, _IONBF, 0);
	}

#if _DEBUG
	// Always force full logging by default in DEBUG builds
	gLogDebug = true;
	unbuffered = setvbuf(LogFile, NULL, _IONBF, 0); 
#endif

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

	// [Device] (DXGI parameters)
	if (GetPrivateProfileString(L"Device", L"width", 0, setting, MAX_PATH, iniFile))
		swscanf_s(setting, L"%d", &G->SCREEN_WIDTH);
	if (GetPrivateProfileString(L"Device", L"height", 0, setting, MAX_PATH, iniFile))
		swscanf_s(setting, L"%d", &G->SCREEN_HEIGHT);
	if (GetPrivateProfileString(L"Device", L"refresh_rate", 0, setting, MAX_PATH, iniFile))
		swscanf_s(setting, L"%d", &G->SCREEN_REFRESH);
	if (GetPrivateProfileString(L"Device", L"filter_refresh_rate", 0, setting, MAX_PATH, iniFile))
	{
		swscanf_s(setting, L"%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
			G->FILTER_REFRESH + 0, G->FILTER_REFRESH + 1, G->FILTER_REFRESH + 2, G->FILTER_REFRESH + 3, 
			G->FILTER_REFRESH + 4, G->FILTER_REFRESH + 5, G->FILTER_REFRESH + 6, G->FILTER_REFRESH + 7, 
			G->FILTER_REFRESH + 8, G->FILTER_REFRESH + 9);
	}
	G->SCREEN_FULLSCREEN = GetPrivateProfileInt(L"Device", L"full_screen", 0, iniFile) == 1;
	G->gForceStereo = GetPrivateProfileInt(L"Device", L"force_stereo", 0, iniFile) == 1;
	G->SCREEN_ALLOW_COMMANDS = GetPrivateProfileInt(L"Device", L"allow_windowcommands", 0, iniFile) == 1;

	if (LogFile)
	{
		LogInfo("[Device]\n");
		if (G->SCREEN_WIDTH != -1) LogInfo("  width=%d\n", G->SCREEN_WIDTH);
		if (G->SCREEN_HEIGHT != -1) LogInfo("  height=%d\n", G->SCREEN_HEIGHT);
		if (G->SCREEN_REFRESH != -1) LogInfo("  refresh_rate=%d\n", G->SCREEN_REFRESH);
		if (G->FILTER_REFRESH[0]) LogInfoW(L"  filter_refresh_rate=%d\n", G->FILTER_REFRESH[0]);
		if (G->SCREEN_FULLSCREEN) LogInfo("  full_screen=1\n");
		if (G->gForceStereo) LogInfo("  force_stereo=1\n");
		if (G->SCREEN_ALLOW_COMMANDS) LogInfo("  allow_windowcommands=1\n");
	}

	if (GetPrivateProfileString(L"Device", L"get_resolution_from", 0, setting, MAX_PATH, iniFile)) {
		G->mResolutionInfo.from = lookup_enum_val<wchar_t *, GetResolutionFrom>
			(GetResolutionFromNames, setting, GetResolutionFrom::INVALID);
		if (G->mResolutionInfo.from == GetResolutionFrom::INVALID) {
			LogInfoW(L"  WARNING: Unknown get_resolution_from %s\n", setting);
			BeepFailure2();
		} else
			LogInfoW(L"  get_resolution_from=%s\n", setting);
	} else
		G->mResolutionInfo.from = GetResolutionFrom::INVALID;

	// [Stereo]
	bool automaticMode = GetPrivateProfileInt(L"Stereo", L"automatic_mode", 0, iniFile) == 1;				// in NVapi dll
	G->gCreateStereoProfile = GetPrivateProfileInt(L"Stereo", L"create_profile", 0, iniFile) == 1;
	G->gSurfaceCreateMode = GetPrivateProfileInt(L"Stereo", L"surface_createmode", -1, iniFile);
	G->gSurfaceSquareCreateMode = GetPrivateProfileInt(L"Stereo", L"surface_square_createmode", -1, iniFile);
	G->gForceNoNvAPI = GetPrivateProfileInt(L"Stereo", L"force_no_nvapi", 0, iniFile) == 1;

	if (LogFile)
	{
		LogInfo("[Stereo]\n");
		if (automaticMode) LogInfo("  automatic_mode=1\n");
		if (G->gCreateStereoProfile) LogInfo("  create_profile=1\n");
		if (G->gSurfaceCreateMode != -1) LogInfo("  surface_createmode=%d\n", G->gSurfaceCreateMode);
		if (G->gSurfaceSquareCreateMode != -1) LogInfo("  surface_square_createmode=%d\n", G->gSurfaceSquareCreateMode);
		if (G->gForceNoNvAPI) LogInfo("  force_no_nvapi=1 \n");
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
	G->track_texture_updates = GetPrivateProfileInt(L"Rendering", L"track_texture_updates", 0, iniFile) == 1;

	G->EXPORT_FIXED = GetPrivateProfileInt(L"Rendering", L"export_fixed", 0, iniFile) == 1;
	G->EXPORT_SHADERS = GetPrivateProfileInt(L"Rendering", L"export_shaders", 0, iniFile) == 1;
	G->EXPORT_HLSL = GetPrivateProfileInt(L"Rendering", L"export_hlsl", 0, iniFile);
	G->EXPORT_BINARY = GetPrivateProfileInt(L"Rendering", L"export_binary", 0, iniFile) == 1;
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
		if (G->track_texture_updates) LogInfo("  track_texture_updates=1\n");

		if (G->EXPORT_FIXED) LogInfo("  export_fixed=1\n");
		if (G->EXPORT_SHADERS) LogInfo("  export_shaders=1\n");
		if (G->EXPORT_HLSL != 0) LogInfo("  export_hlsl=%d\n", G->EXPORT_HLSL);
		if (G->EXPORT_BINARY) LogInfo("  export_binary=1\n");
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
		uint32_t hash;
		swscanf_s(setting, L"%08lx", &hash);
		G->ZBufferHashToInject = hash;
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
	G->hunting = GetPrivateProfileInt(L"Hunting", L"hunting", 0, iniFile);
	if (G->hunting)
		LogInfo("  hunting=%u\n", G->hunting);

	G->marking_mode = MARKING_MODE_SKIP;
	if (GetPrivateProfileString(L"Hunting", L"marking_mode", 0, setting, MAX_PATH, iniFile)) {
		if (!wcscmp(setting, L"skip")) G->marking_mode = MARKING_MODE_SKIP;
		if (!wcscmp(setting, L"mono")) G->marking_mode = MARKING_MODE_MONO;
		if (!wcscmp(setting, L"original")) G->marking_mode = MARKING_MODE_ORIGINAL;
		if (!wcscmp(setting, L"zero")) G->marking_mode = MARKING_MODE_ZERO;
		if (!wcscmp(setting, L"pink")) G->marking_mode = MARKING_MODE_PINK;
		LogInfoW(L"  marking_mode=%d\n", G->marking_mode);
	}


	RegisterHuntingKeyBindings(iniFile);
	RegisterPresetKeyBindings(sections, iniFile);

	ParseResourceSections(sections, iniFile);
	ParseShaderOverrideSections(sections, iniFile);
	ParseTextureOverrideSections(sections, iniFile);

	LogInfo("[Present]\n");
	G->present_command_list.clear();
	ParseCommandList(L"Present", iniFile, &G->present_command_list, NULL, NULL);

	// Read in any constants defined in the ini, for use as shader parameters
	// Any result of the default FLT_MAX means the parameter is not in use.
	// stof will crash if passed FLT_MAX, hence the extra check.
	// We use FLT_MAX instead of the more logical INFINITY, because Microsoft *always* generates 
	// warnings, even for simple comparisons. And NaN comparisons are similarly broken.
	LogInfo("[Constants]\n");
	for (i = 0; i < INI_PARAMS_SIZE; i++) {
		wchar_t buf[8];

		G->iniParams[i].x = 0;
		G->iniParams[i].y = 0;
		G->iniParams[i].z = 0;
		G->iniParams[i].w = 0;
		StringCchPrintf(buf, 8, L"x%.0i", i);
		if (GetPrivateProfileString(L"Constants", buf, L"FLT_MAX", setting, MAX_PATH, iniFile))
		{
			if (wcscmp(setting, L"FLT_MAX") != 0) {
				G->iniParams[i].x = stof(setting);
				LogInfoW(L"  %ls=%#.2g\n", buf, G->iniParams[i].x);
			}
		}
		StringCchPrintf(buf, 8, L"y%.0i", i);
		if (GetPrivateProfileString(L"Constants", buf, L"FLT_MAX", setting, MAX_PATH, iniFile))
		{
			if (wcscmp(setting, L"FLT_MAX") != 0) {
				G->iniParams[i].y = stof(setting);
				LogInfoW(L"  %ls=%#.2g\n", buf, G->iniParams[i].y);
			}
		}
		StringCchPrintf(buf, 8, L"z%.0i", i);
		if (GetPrivateProfileString(L"Constants", buf, L"FLT_MAX", setting, MAX_PATH, iniFile))
		{
			if (wcscmp(setting, L"FLT_MAX") != 0) {
				G->iniParams[i].z = stof(setting);
				LogInfoW(L"  %ls=%#.2g\n", buf, G->iniParams[i].z);
			}
		}
		StringCchPrintf(buf, 8, L"w%.0i", i);
		if (GetPrivateProfileString(L"Constants", buf, L"FLT_MAX", setting, MAX_PATH, iniFile))
		{
			if (wcscmp(setting, L"FLT_MAX") != 0) {
				G->iniParams[i].w = stof(setting);
				LogInfoW(L"  %ls=%#.2g\n", buf, G->iniParams[i].w);
			}
		}
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
	// FIXME: THIS CRASHES IF 3D IS DISABLED (ROOT CAUSE LIKELY ELSEWHERE)
	realContext->Map(device->mIniTexture, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	memcpy(mappedResource.pData, &G->iniParams, sizeof(G->iniParams));
	realContext->Unmap(device->mIniTexture, 0);
}
