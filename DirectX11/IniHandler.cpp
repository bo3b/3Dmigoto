#include "IniHandler.h"

#include <algorithm>
#include <string>
#include <strsafe.h>

#include "log.h"
#include "Globals.h"
#include "Override.h"
#include "Hunting.h"
#include "nvprofile.h"

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
struct Section {
	wchar_t *section;
	bool prefix;
};
static Section CommandListSections[] = {
	{L"ShaderOverride", true},
	{L"TextureOverride", true},
	{L"CustomShader", true},
	{L"CommandList", true},
	{L"Present", false},
};

// List all remaining sections so we can verify that every section listed in
// the d3dx.ini is valid and warn about any typos. As above, the boolean value
// indicates that this is a prefix, false if it is an exact match. No need to
// list a section in both lists - put it above if it is a command list section,
// and in this list if it is not:
static Section RegularSections[] = {
	{L"Logging", false},
	{L"System", false},
	{L"Device", false},
	{L"Stereo", false},
	{L"Rendering", false},
	{L"Hunting", false},
	{L"Constants", false},
	{L"Profile", false},
	{L"ConvergenceMap", false}, // Only used in nvapi wrapper
	{L"Resource", true},
	{L"Key", true},
};

// List of sections that will not trigger a warning if they contain a line
// without an equals sign
static wchar_t *AllowLinesWithoutEquals[] = {
	L"Profile",
};

static bool SectionInList(const wchar_t *section, Section section_list[], int list_size)
{
	size_t len;
	int i;

	for (i = 0; i < list_size; i++) {
		if (section_list[i].prefix) {
			len = wcslen(section_list[i].section);
			if (!_wcsnicmp(section, section_list[i].section, len))
				return true;
		} else {
			if (!_wcsicmp(section, section_list[i].section))
				return true;
		}
	}

	return false;
}

static bool IsCommandListSection(const wchar_t *section)
{
	return SectionInList(section, CommandListSections, ARRAYSIZE(CommandListSections));
}

static bool IsRegularSection(const wchar_t *section)
{
	return SectionInList(section, RegularSections, ARRAYSIZE(RegularSections));
}

static bool DoesSectionAllowLinesWithoutEquals(const wchar_t *section)
{
	int i;

	for (i = 0; i < ARRAYSIZE(AllowLinesWithoutEquals); i++) {
		if (!_wcsicmp(section, AllowLinesWithoutEquals[i]))
			return true;
	}

	return false;
}

// Helper functions to parse common types and log their values. TODO: Convert
// more of this file to use these where appropriate
static float GetIniFloat(const wchar_t *section, const wchar_t *key, float def, const wchar_t *iniFile, bool *found)
{
	wchar_t val[32];
	float ret = def;
	int len;

	if (found)
		*found = false;

	if (GetPrivateProfileString(section, key, 0, val, 32, iniFile)) {
		swscanf_s(val, L"%f%n", &ret, &len);
		if (len != wcslen(val)) {
			LogInfo("  WARNING: Floating point parse error: %S=%S\n", key, val);
			BeepFailure2();
		} else {
			if (found)
				*found = true;
			LogInfo("  %S=%f\n", key, ret);
		}
	}

	return ret;
}

static int GetIniInt(const wchar_t *section, const wchar_t *key, int def, const wchar_t *iniFile, bool *found)
{
	wchar_t val[32];
	int ret = def;
	int len;

	if (found)
		*found = false;

	// Not using GetPrivateProfileInt as it doesn't tell us if the key existed
	if (GetPrivateProfileString(section, key, 0, val, 32, iniFile)) {
		swscanf_s(val, L"%d%n", &ret, &len);
		if (len != wcslen(val)) {
			LogInfo("  WARNING: Integer parse error: %S=%S\n", key, val);
			BeepFailure2();
		} else {
			if (found)
				*found = true;
			LogInfo("  %S=%d\n", key, ret);
		}
	}

	return ret;
}

static bool GetIniBool(const wchar_t *section, const wchar_t *key, bool def, const wchar_t *iniFile, bool *found)
{
	wchar_t val[32];
	bool ret = def;

	if (found)
		*found = false;

	// Not using GetPrivateProfileInt as it doesn't tell us if the key existed
	if (GetPrivateProfileString(section, key, 0, val, 32, iniFile)) {
		if (!_wcsicmp(val, L"1") || !_wcsicmp(val, L"true") || !_wcsicmp(val, L"yes") || !_wcsicmp(val, L"on")) {
			LogInfo("  %S=1\n", key);
			if (found)
				*found = true;
			return true;
		}
		if (!_wcsicmp(val, L"0") || !_wcsicmp(val, L"false") || !_wcsicmp(val, L"no") || !_wcsicmp(val, L"off")) {
			LogInfo("  %S=0\n", key);
			if (found)
				*found = true;
			return false;
		}

		LogInfo("  WARNING: Boolean parse error: %S=%S\n", key, val);
		BeepFailure2();
	}

	return ret;
}

class EnumParseError: public exception {} enumParseError;

static int ParseEnum(wchar_t *str, wchar_t *prefix, wchar_t *names[], int names_len, int first)
{
	size_t prefix_len;
	wchar_t *ptr = str;
	int i;

	if (prefix) {
		prefix_len = wcslen(prefix);
		if (!_wcsnicmp(ptr, prefix, prefix_len))
			ptr += prefix_len;
	}

	for (i = first; i < names_len; i++) {
		if (!_wcsicmp(ptr, names[i]))
			return i;
	}

	throw enumParseError;
}

static int GetIniEnum(const wchar_t *section, const wchar_t *key, int def, const wchar_t *iniFile, bool *found,
		wchar_t *prefix, wchar_t *names[], int names_len, int first)
{
	wchar_t val[MAX_PATH];
	int ret = def;

	if (found)
		*found = false;

	if (GetPrivateProfileString(section, key, 0, val, MAX_PATH, iniFile)) {
		try {
			ret = ParseEnum(val, prefix, names, names_len, first);
			if (found)
				*found = true;
			LogInfo("  %S=%S\n", key, val);
		} catch (EnumParseError) {
			LogInfo("  WARNING: Unrecognised %S=%S\n", key, val);
			BeepFailure2();
		}
	}

	return ret;
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
	bool warn_lines_without_equals = true;

	// Sections that utilise a command list are allowed to have duplicate
	// keys, while other sections are not. The command list parser will
	// still check for duplicate keys that are not part of the command
	// list.
	if (IsCommandListSection(section))
		warn_duplicates = false;
	else if (!IsRegularSection(section)) {
		LogInfoW(L"WARNING: Unknown section in d3dx.ini: [%s]\n", section);
		BeepFailure2();
	}

	if (DoesSectionAllowLinesWithoutEquals(section))
		warn_lines_without_equals = false;

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
			if (warn_lines_without_equals) {
				LogInfoW(L"WARNING: Malformed line in d3dx.ini: [%s] \"%s\"\n", section, kptr);
				BeepFailure2();
				kptr = vptr;
				continue;
			}
		} else {
			*vptr = L'\0';
			vptr++;
		}

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

		LogInfo("[%S]\n", id);

		if (!GetPrivateProfileString(id, L"Key", 0, key, MAX_PATH, iniFile)) {
			LogInfo("  WARNING: [%S] missing Key=\n", id);
			BeepFailure2();
			continue;
		}

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

		delay = GetIniInt(id, L"delay", 0, iniFile, NULL);
		release_delay = GetIniInt(id, L"release_delay", 0, iniFile, NULL);

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
			GetIniInt(i->c_str(), L"max_copies_per_frame", 0, iniFile, NULL);

		if (GetPrivateProfileString(i->c_str(), L"filename", 0, setting, MAX_PATH, iniFile)) {
			LogInfoW(L"  filename=%s\n", setting);
			GetModuleFileName(0, path, MAX_PATH);
			wcsrchr(path, L'\\')[1] = 0;
			wcscat(path, setting);
			custom_resource->filename = path;
		}

		if (GetPrivateProfileString(i->c_str(), L"type", 0, setting, MAX_PATH, iniFile)) {
			custom_resource->override_type = lookup_enum_val<const wchar_t *, CustomResourceType>
				(CustomResourceTypeNames, setting, CustomResourceType::INVALID);
			if (custom_resource->override_type == CustomResourceType::INVALID) {
				LogInfo("  WARNING: Unknown type \"%S\"\n", setting);
				BeepFailure2();
			} else {
				LogInfo("  type=%S\n", setting);
			}
		}

		if (GetPrivateProfileString(i->c_str(), L"mode", 0, setting, MAX_PATH, iniFile)) {
			custom_resource->override_mode = lookup_enum_val<const wchar_t *, CustomResourceMode>
				(CustomResourceModeNames, setting, CustomResourceMode::DEFAULT);
			if (custom_resource->override_mode == CustomResourceMode::DEFAULT) {
				LogInfo("  WARNING: Unknown mode \"%S\"\n", setting);
				BeepFailure2();
			} else {
				LogInfo("  mode=%S\n", setting);
			}
		}

		if (GetPrivateProfileString(i->c_str(), L"format", 0, setting, MAX_PATH, iniFile)) {
			custom_resource->override_format = ParseFormatString(setting);
			if (custom_resource->override_format == (DXGI_FORMAT)-1) {
				LogInfo("  WARNING: Unknown format \"%S\"\n", setting);
				BeepFailure2();
			} else {
				LogInfo("  format=%s\n", TexFormatStr(custom_resource->override_format));
			}
		}

		custom_resource->override_width = GetIniInt(i->c_str(), L"width", -1, iniFile, NULL);
		custom_resource->override_height = GetIniInt(i->c_str(), L"height", -1, iniFile, NULL);
		custom_resource->override_depth = GetIniInt(i->c_str(), L"depth", -1, iniFile, NULL);
		custom_resource->override_mips = GetIniInt(i->c_str(), L"mips", -1, iniFile, NULL);
		custom_resource->override_array = GetIniInt(i->c_str(), L"array", -1, iniFile, NULL);
		custom_resource->override_msaa = GetIniInt(i->c_str(), L"msaa", -1, iniFile, NULL);
		custom_resource->override_msaa_quality = GetIniInt(i->c_str(), L"msaa_quality", -1, iniFile, NULL);
		custom_resource->override_byte_width = GetIniInt(i->c_str(), L"byte_width", -1, iniFile, NULL);
		custom_resource->override_stride = GetIniInt(i->c_str(), L"stride", -1, iniFile, NULL);

		custom_resource->width_multiply = GetIniFloat(i->c_str(), L"width_multiply", 1.0f, iniFile, NULL);
		custom_resource->height_multiply = GetIniFloat(i->c_str(), L"height_multiply", 1.0f, iniFile, NULL);

		if (GetPrivateProfileString(i->c_str(), L"bind_flags", 0, setting, MAX_PATH, iniFile)) {
			custom_resource->override_bind_flags = parse_enum_option_string<wchar_t *, CustomResourceBindFlags>
				(CustomResourceBindFlagNames, setting, NULL);
		}

		// TODO: Overrides for misc flags, etc
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
	CommandList *command_list, *explicit_command_list;
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

		// Convert key + val to lower case since ini files are supposed
		// to be case insensitive:
		std::transform(key->begin(), key->end(), key->begin(), ::towlower);
		std::transform(val->begin(), val->end(), val->begin(), ::towlower);

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
		explicit_command_list = NULL;
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
		continue;
log_continue:
		LogInfoW(L"  %ls=%s\n", key->c_str(), val->c_str());
	}
}

static void ParseDriverProfile(wchar_t *iniFile)
{
	IniSection section;
	IniSection::iterator entry;
	wstring *lhs, *rhs;

	// Arguably we should only parse this section the first time since the
	// settings will only be applied on startup.
	profile_settings.clear();

	GetIniSection(section, L"Profile", iniFile);
	for (entry = section.begin(); entry < section.end(); entry++) {
		lhs = &entry->first;
		rhs = &entry->second;

		parse_ini_profile_line(lhs, rhs);
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
	L"model",
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

		LogInfo("[%S]\n", id);

		if (!GetPrivateProfileString(id, L"Hash", 0, setting, MAX_PATH, iniFile)) {
			LogInfo("  WARNING: [%S] missing Hash=\n", id);
			BeepFailure2();
			continue;
		}
		swscanf_s(setting, L"%16llx", &hash);
		LogInfo("  Hash=%16llx\n", hash);

		if (G->mShaderOverrideMap.count(hash)) {
			LogInfo("  WARNING: Duplicate ShaderOverride hash: %16llx\n", hash);
			BeepFailure2();
		}
		override = &G->mShaderOverrideMap[hash];

		override->separation = GetIniFloat(id, L"Separation", FLT_MAX, iniFile, NULL);
		override->convergence = GetIniFloat(id, L"Convergence", FLT_MAX, iniFile, NULL);

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

		if (GetPrivateProfileString(id, L"model", 0, setting, MAX_PATH, iniFile)) {
			wcstombs(override->model, setting, ARRAYSIZE(override->model));
			override->model[ARRAYSIZE(override->model) - 1] = '\0';
			LogInfo("  model=%s\n", override->model);
		}

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
	L"width",
	L"height",
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

		LogInfo("[%S]\n", id);

		if (!GetPrivateProfileString(id, L"Hash", 0, setting, MAX_PATH, iniFile)) {
			LogInfo("  WARNING: [%S] missing Hash=\n", id);
			BeepFailure2();
			continue;
		}

		swscanf_s(setting, L"%8lx", &hash);
		LogInfo("  Hash=%08lx\n", hash);

		if (G->mTextureOverrideMap.count(hash)) {
			LogInfo("  WARNING: Duplicate TextureOverride hash: %08lx\n", hash);
			BeepFailure2();
		}
		override = &G->mTextureOverrideMap[hash];

		override->stereoMode = GetIniInt(id, L"StereoMode", -1, iniFile, NULL);
		override->format = GetIniInt(id, L"Format", -1, iniFile, NULL);
		override->width = GetIniInt(id, L"Width", -1, iniFile, NULL);
		override->height = GetIniInt(id, L"Height", -1, iniFile, NULL);

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

		override->filter_index = GetIniFloat(id, L"filter_index", 1.0f, iniFile, NULL);

		override->expand_region_copy = GetIniBool(id, L"expand_region_copy", false, iniFile, NULL);
		override->deny_cpu_read = GetIniBool(id, L"deny_cpu_read", false, iniFile, NULL);

		ParseCommandList(id, iniFile, &override->command_list, &override->post_command_list, TextureOverrideIniKeys);
	}
	if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
}

// https://msdn.microsoft.com/en-us/library/windows/desktop/ff476088(v=vs.85).aspx
static wchar_t *BlendOPs[] = {
	L"",
	L"ADD",
	L"SUBTRACT",
	L"REV_SUBTRACT",
	L"MIN",
	L"MAX",
};

// https://msdn.microsoft.com/en-us/library/windows/desktop/ff476086(v=vs.85).aspx
static wchar_t *BlendFactors[] = {
	L"",
	L"ZERO",
	L"ONE",
	L"SRC_COLOR",
	L"INV_SRC_COLOR",
	L"SRC_ALPHA",
	L"INV_SRC_ALPHA",
	L"DEST_ALPHA",
	L"INV_DEST_ALPHA",
	L"DEST_COLOR",
	L"INV_DEST_COLOR",
	L"SRC_ALPHA_SAT",
	L"",
	L"",
	L"BLEND_FACTOR",
	L"INV_BLEND_FACTOR",
	L"SRC1_COLOR",
	L"INV_SRC1_COLOR",
	L"SRC1_ALPHA",
	L"INV_SRC1_ALPHA",
};

static void ParseBlendOp(wchar_t *key, wchar_t *val, D3D11_BLEND_OP *op, D3D11_BLEND *src, D3D11_BLEND *dst)
{
	wchar_t op_buf[32], src_buf[32], dst_buf[32];
	int i;

	i = swscanf_s(val, L"%s %s %s",
			op_buf, (unsigned)ARRAYSIZE(op_buf),
			src_buf, (unsigned)ARRAYSIZE(src_buf),
			dst_buf, (unsigned)ARRAYSIZE(dst_buf));
	if (i != 3) {
		LogInfo("  WARNING: Unrecognised %S=%S\n", key, val);
		BeepFailure2();
		return;
	}
	LogInfo("  %S=%S\n", key, val);

	try {
		*op = (D3D11_BLEND_OP)ParseEnum(op_buf, L"D3D11_BLEND_OP_", BlendOPs, ARRAYSIZE(BlendOPs), 1);
	} catch (EnumParseError) {
		LogInfo("  WARNING: Unrecognised blend operation %S\n", op_buf);
		BeepFailure2();
	}

	try {
		*src = (D3D11_BLEND)ParseEnum(src_buf, L"D3D11_BLEND_", BlendFactors, ARRAYSIZE(BlendFactors), 1);
	} catch (EnumParseError) {
		LogInfo("  WARNING: Unrecognised blend source factor %S\n", src_buf);
		BeepFailure2();
	}

	try {
		*dst = (D3D11_BLEND)ParseEnum(dst_buf, L"D3D11_BLEND_", BlendFactors, ARRAYSIZE(BlendFactors), 1);
	} catch (EnumParseError) {
		LogInfo("  WARNING: Unrecognised blend destination factor %S\n", dst_buf);
		BeepFailure2();
	}
}

static bool ParseBlendRenderTarget(D3D11_RENDER_TARGET_BLEND_DESC *desc, const wchar_t *section, int index, wchar_t *iniFile)
{
	wchar_t setting[MAX_PATH];
	bool override = false;
	wchar_t key[32];
	int ival;

	wcscpy(key, L"blend");
	if (index >= 0)
		swprintf_s(key, ARRAYSIZE(key), L"blend[%i]", index);
	if (GetPrivateProfileString(section, key, 0, setting, MAX_PATH, iniFile)) {
		override = true;

		// Special value to disable blending:
		if (!wcscmp(setting, L"disable")) {
			LogInfo("  %S=disable\n", key);
			desc->BlendEnable = false;
			return true;
		}

		ParseBlendOp(key, setting,
				&desc->BlendOp,
				&desc->SrcBlend,
				&desc->DestBlend);
	}

	wcscpy(key, L"alpha");
	if (index >= 0)
		swprintf_s(key, ARRAYSIZE(key), L"alpha[%i]", index);
	if (GetPrivateProfileString(section, key, 0, setting, MAX_PATH, iniFile)) {
		override = true;
		ParseBlendOp(key, setting,
				&desc->BlendOpAlpha,
				&desc->SrcBlendAlpha,
				&desc->DestBlendAlpha);
	}

	wcscpy(key, L"mask");
	if (index >= 0)
		swprintf_s(key, ARRAYSIZE(key), L"mask[%i]", index);
	if (GetPrivateProfileString(section, key, 0, setting, MAX_PATH, iniFile)) {
		override = true;
		swscanf_s(setting, L"%x", &ival); // No suitable format string w/o overflow?
		desc->RenderTargetWriteMask = ival; // Use an intermediate to be safe
		LogInfo("  %S=0x%x\n", key, desc->RenderTargetWriteMask);
	}

	if (override)
		desc->BlendEnable = true;

	return override;
}

static void ParseBlendState(CustomShader *shader, const wchar_t *section, wchar_t *iniFile)
{
	D3D11_BLEND_DESC *desc = &shader->blend_desc;
	wchar_t setting[MAX_PATH];
	wchar_t key[32];
	int i;
	bool found;

	memset(desc, 0, sizeof(D3D11_BLEND_DESC));

	// Set a default blend state for any missing values:
	desc->IndependentBlendEnable = false;
	desc->RenderTarget[0].BlendEnable = false;
	desc->RenderTarget[0].SrcBlend = D3D11_BLEND_ONE;
	desc->RenderTarget[0].DestBlend = D3D11_BLEND_ZERO;
	desc->RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
	desc->RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
	desc->RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
	desc->RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
	desc->RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

	// Any blend states that are specified without a render target index
	// are propagated to all render targets:
	if (ParseBlendRenderTarget(&desc->RenderTarget[0], section, -1, iniFile))
		shader->blend_override = 1;
	for (i = 1; i < 8; i++)
		memcpy(&desc->RenderTarget[i], &desc->RenderTarget[0], sizeof(D3D11_RENDER_TARGET_BLEND_DESC));

	// We check all render targets again with the [%i] syntax. We do the
	// first one again since the last time was for default, while this is
	// for the specific target:
	for (i = 0; i < 8; i++) {
		if (ParseBlendRenderTarget(&desc->RenderTarget[i], section, i, iniFile)) {
			shader->blend_override = 1;
			desc->IndependentBlendEnable = true;
		}
	}

	desc->AlphaToCoverageEnable = GetIniBool(section, L"alpha_to_coverage", false, iniFile, &found);
	if (found)
		shader->blend_override = 1;

	for (i = 0; i < 4; i++) {
		swprintf_s(key, ARRAYSIZE(key), L"blend_factor[%i]", i);
		shader->blend_factor[i] = GetIniFloat(section, key, 0.0f, iniFile, &found);
		if (found)
			shader->blend_override = 1;
	}

	if (GetPrivateProfileString(section, L"sample_mask", 0, setting, MAX_PATH, iniFile)) {
		shader->blend_override = 1;
		swscanf_s(setting, L"%x", &shader->blend_sample_mask);
		LogInfo("  sample_mask=0x%x\n", shader->blend_sample_mask);
	}
}

// https://msdn.microsoft.com/en-us/library/windows/desktop/ff476131(v=vs.85).aspx
static wchar_t *FillModes[] = {
	L"",
	L"",
	L"WIREFRAME",
	L"SOLID",
};

// https://msdn.microsoft.com/en-us/library/windows/desktop/ff476108(v=vs.85).aspx
static wchar_t *CullModes[] = {
	L"",
	L"NONE",
	L"FRONT",
	L"BACK",
};

// Actually a bool
static wchar_t *FrontDirection[] = {
	L"Clockwise",
	L"CounterClockwise",
};

static void ParseRSState(CustomShader *shader, const wchar_t *section, wchar_t *iniFile)
{
	D3D11_RASTERIZER_DESC *desc = &shader->rs_desc;
	bool found;

	desc->FillMode = (D3D11_FILL_MODE)GetIniEnum(section, L"fill", D3D11_FILL_SOLID, iniFile, &found,
			L"D3D11_FILL_", FillModes, ARRAYSIZE(FillModes), 2);
	if (found)
		shader->rs_override = 1;

	desc->CullMode = (D3D11_CULL_MODE)GetIniEnum(section, L"cull", D3D11_CULL_BACK,  iniFile, &found,
			L"D3D11_CULL_", CullModes, ARRAYSIZE(CullModes), 1);
	if (found)
		shader->rs_override = 1;

	desc->FrontCounterClockwise = (BOOL)GetIniEnum(section, L"front", 0, iniFile, &found,
			NULL, FrontDirection, ARRAYSIZE(FrontDirection), 0);
	if (found)
		shader->rs_override = 1;

	desc->DepthBias = GetIniInt(section, L"depth_bias", 0, iniFile, &found);
	if (found)
		shader->rs_override = 1;

	desc->DepthBiasClamp = GetIniFloat(section, L"depth_bias_clamp", 0, iniFile, &found);
	if (found)
		shader->rs_override = 1;

	desc->SlopeScaledDepthBias = GetIniFloat(section, L"slope_scaled_depth_bias", 0, iniFile, &found);
	if (found)
		shader->rs_override = 1;

	desc->DepthClipEnable = GetIniBool(section, L"depth_clip_enable", true, iniFile, &found);
	if (found)
		shader->rs_override = 1;

	desc->ScissorEnable = GetIniBool(section, L"scissor_enable", false, iniFile, &found);
	if (found)
		shader->rs_override = 1;

	desc->MultisampleEnable = GetIniBool(section, L"multisample_enable", false, iniFile, &found);
	if (found)
		shader->rs_override = 1;

	desc->AntialiasedLineEnable = GetIniBool(section, L"antialiased_line_enable", false, iniFile, &found);
	if (found)
		shader->rs_override = 1;
}

struct PrimitiveTopology {
	wchar_t *name;
	int val;
};

static struct PrimitiveTopology PrimitiveTopologies[] = {
	{ L"UNDEFINED", 0},
	{ L"POINT_LIST", 1},
	{ L"LINE_LIST", 2},
	{ L"LINE_STRIP", 3},
	{ L"TRIANGLE_LIST", 4},
	{ L"TRIANGLE_STRIP", 5},
	{ L"LINE_LIST_ADJ", 10},
	{ L"LINE_STRIP_ADJ", 11},
	{ L"TRIANGLE_LIST_ADJ", 12},
	{ L"TRIANGLE_STRIP_ADJ", 13},
	{ L"1_CONTROL_POINT_PATCH_LIST", 33},
	{ L"2_CONTROL_POINT_PATCH_LIST", 34},
	{ L"3_CONTROL_POINT_PATCH_LIST", 35},
	{ L"4_CONTROL_POINT_PATCH_LIST", 36},
	{ L"5_CONTROL_POINT_PATCH_LIST", 37},
	{ L"6_CONTROL_POINT_PATCH_LIST", 38},
	{ L"7_CONTROL_POINT_PATCH_LIST", 39},
	{ L"8_CONTROL_POINT_PATCH_LIST", 40},
	{ L"9_CONTROL_POINT_PATCH_LIST", 41},
	{ L"10_CONTROL_POINT_PATCH_LIST", 42},
	{ L"11_CONTROL_POINT_PATCH_LIST", 43},
	{ L"12_CONTROL_POINT_PATCH_LIST", 44},
	{ L"13_CONTROL_POINT_PATCH_LIST", 45},
	{ L"14_CONTROL_POINT_PATCH_LIST", 46},
	{ L"15_CONTROL_POINT_PATCH_LIST", 47},
	{ L"16_CONTROL_POINT_PATCH_LIST", 48},
	{ L"17_CONTROL_POINT_PATCH_LIST", 49},
	{ L"18_CONTROL_POINT_PATCH_LIST", 50},
	{ L"19_CONTROL_POINT_PATCH_LIST", 51},
	{ L"20_CONTROL_POINT_PATCH_LIST", 52},
	{ L"21_CONTROL_POINT_PATCH_LIST", 53},
	{ L"22_CONTROL_POINT_PATCH_LIST", 54},
	{ L"23_CONTROL_POINT_PATCH_LIST", 55},
	{ L"24_CONTROL_POINT_PATCH_LIST", 56},
	{ L"25_CONTROL_POINT_PATCH_LIST", 57},
	{ L"26_CONTROL_POINT_PATCH_LIST", 58},
	{ L"27_CONTROL_POINT_PATCH_LIST", 59},
	{ L"28_CONTROL_POINT_PATCH_LIST", 60},
	{ L"29_CONTROL_POINT_PATCH_LIST", 61},
	{ L"30_CONTROL_POINT_PATCH_LIST", 62},
	{ L"31_CONTROL_POINT_PATCH_LIST", 63},
	{ L"32_CONTROL_POINT_PATCH_LIST", 64},
};

static void ParseTopology(CustomShader *shader, const wchar_t *section, wchar_t *iniFile)
{
	wchar_t *prefix = L"D3D11_PRIMITIVE_TOPOLOGY_";
	size_t prefix_len;
	wchar_t val[MAX_PATH];
	wchar_t *ptr;
	int i;

	shader->topology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;

	if (!GetPrivateProfileString(section, L"topology", 0, val, MAX_PATH, iniFile))
		return;

	prefix_len = wcslen(prefix);
	ptr = val;
	if (!_wcsnicmp(ptr, prefix, prefix_len))
		ptr += prefix_len;


	for (i = 1; i < ARRAYSIZE(PrimitiveTopologies); i++) {
		if (!_wcsicmp(ptr, PrimitiveTopologies[i].name)) {
			shader->topology = (D3D11_PRIMITIVE_TOPOLOGY)PrimitiveTopologies[i].val;
			LogInfo("  topology=%S\n", val);
			return;
		}

	}

	LogInfo("  WARNING: Unrecognised primitive topology=%S\n", val);
	BeepFailure2();
}

// List of keys in [CustomShader] sections that are processed in this
// function. Used by ParseCommandList to find any unrecognised lines.
wchar_t *CustomShaderIniKeys[] = {
	L"vs", L"hs", L"ds", L"gs", L"ps", L"cs",
	L"max_executions_per_frame",
	// OM Blend State overrides:
	L"blend", L"alpha", L"mask",
	L"blend[0]", L"blend[1]", L"blend[2]", L"blend[3]",
	L"blend[4]", L"blend[5]", L"blend[6]", L"blend[7]",
	L"alpha[0]", L"alpha[1]", L"alpha[2]", L"alpha[3]",
	L"alpha[4]", L"alpha[5]", L"alpha[6]", L"alpha[7]",
	L"mask[0]", L"mask[1]", L"mask[2]", L"mask[3]",
	L"mask[4]", L"mask[5]", L"mask[6]", L"mask[7]",
	L"alpha_to_coverage", L"sample_mask",
	L"blend_factor[0]", L"blend_factor[1]",
	L"blend_factor[2]", L"blend_factor[3]",
	// RS State overrides:
	L"fill", L"cull", L"front", L"depth_bias", L"depth_bias_clamp",
	L"slope_scaled_depth_bias", L"depth_clip_enable", L"scissor_enable",
	L"multisample_enable", L"antialiased_line_enable",
	// IA State overrides:
	L"topology",
	NULL
};
static void EnumerateCustomShaderSections(IniSections &sections, wchar_t *iniFile)
{
	IniSections::iterator lower, upper, i;
	wstring shader_id;

	customShaders.clear();

	lower = sections.lower_bound(wstring(L"CustomShader"));
	upper = prefix_upper_bound(sections, wstring(L"CustomShader"));
	for (i = lower; i != upper; i++) {
		// Convert section name to lower case so our keys will be
		// consistent in the unordered_map:
		shader_id = *i;
		std::transform(shader_id.begin(), shader_id.end(), shader_id.begin(), ::towlower);

		// Construct a custom shader in the global list:
		customShaders[shader_id];
	}
}
static void ParseCustomShaderSections(wchar_t *iniFile)
{
	CustomShaders::iterator i;
	const wstring *shader_id;
	CustomShader *custom_shader;
	wchar_t setting[MAX_PATH];
	bool failed;

	for (i = customShaders.begin(); i != customShaders.end(); i++) {
		shader_id = &i->first;
		custom_shader = &i->second;

		// FIXME: This will be logged in lower case. It would be better
		// to use the original case, but not a big deal:
		LogInfoW(L"[%s]\n", shader_id->c_str());

		failed = false;

		if (GetPrivateProfileString(shader_id->c_str(), L"vs", 0, setting, MAX_PATH, iniFile))
			failed |= custom_shader->compile('v', setting, shader_id);
		if (GetPrivateProfileString(shader_id->c_str(), L"hs", 0, setting, MAX_PATH, iniFile))
			failed |= custom_shader->compile('h', setting, shader_id);
		if (GetPrivateProfileString(shader_id->c_str(), L"ds", 0, setting, MAX_PATH, iniFile))
			failed |= custom_shader->compile('d', setting, shader_id);
		if (GetPrivateProfileString(shader_id->c_str(), L"gs", 0, setting, MAX_PATH, iniFile))
			failed |= custom_shader->compile('g', setting, shader_id);
		if (GetPrivateProfileString(shader_id->c_str(), L"ps", 0, setting, MAX_PATH, iniFile))
			failed |= custom_shader->compile('p', setting, shader_id);
		if (GetPrivateProfileString(shader_id->c_str(), L"cs", 0, setting, MAX_PATH, iniFile))
			failed |= custom_shader->compile('c', setting, shader_id);


		ParseBlendState(custom_shader, shader_id->c_str(), iniFile);
		ParseRSState(custom_shader, shader_id->c_str(), iniFile);
		ParseTopology(custom_shader, shader_id->c_str(), iniFile);

		custom_shader->max_executions_per_frame =
			GetIniInt(shader_id->c_str(), L"max_executions_per_frame", 0, iniFile, NULL);

		if (failed) {
			// Don't want to allow a shader to be run if it had an
			// error since we are likely to call Draw or Dispatch
			customShaders.erase(*shader_id);
			continue;
		}

		ParseCommandList(shader_id->c_str(), iniFile, &custom_shader->command_list, &custom_shader->post_command_list, CustomShaderIniKeys);
	}
}

// "Explicit" means that this parses command lists sections that are
// *explicitly* called [CommandList*], as opposed to other sections that are
// implicitly command lists (such as ShaderOverride, Present, etc).
static void EnumerateExplicitCommandListSections(IniSections &sections, wchar_t *iniFile)
{
	IniSections::iterator lower, upper, i;
	wstring section_id;

	explicitCommandListSections.clear();

	lower = sections.lower_bound(wstring(L"CommandList"));
	upper = prefix_upper_bound(sections, wstring(L"CommandList"));
	for (i = lower; i != upper; i++) {
		// Convert section name to lower case so our keys will be
		// consistent in the unordered_map:
		section_id = *i;
		std::transform(section_id.begin(), section_id.end(), section_id.begin(), ::towlower);

		// Construct an explicit command list section in the global list:
		explicitCommandListSections[section_id];
	}
}

static void ParseExplicitCommandListSections(wchar_t *iniFile)
{
	ExplicitCommandListSections::iterator i;
	ExplicitCommandListSection *command_list_section;
	const wstring *section_id;

	for (i = explicitCommandListSections.begin(); i != explicitCommandListSections.end(); i++) {
		section_id = &i->first;
		command_list_section = &i->second;

		// FIXME: This will be logged in lower case. It would be better
		// to use the original case, but not a big deal:
		LogInfoW(L"[%s]\n", section_id->c_str());
		ParseCommandList(section_id->c_str(), iniFile, &command_list_section->command_list, &command_list_section->post_command_list, NULL);
	}
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

static void ToggleFullScreen(HackerDevice *device, void *private_data)
{
	// SCREEN_FULLSCREEN has several options now, so to preserve the
	// current setting when toggled off we negate it:
	G->SCREEN_FULLSCREEN = -G->SCREEN_FULLSCREEN;
	LogInfo("> full screen forcing toggled to %d (will not take effect until next mode switch)\n", G->SCREEN_FULLSCREEN);
}

void LoadConfigFile()
{
	wchar_t iniFile[MAX_PATH], logFilename[MAX_PATH];
	wchar_t setting[MAX_PATH];
	IniSections sections;
	int i;

	G->gInitialized = true;

	if (!GetModuleFileName(0, iniFile, MAX_PATH))
		DoubleBeepExit();
	wcsrchr(iniFile, L'\\')[1] = 0;
	wcscpy(logFilename, iniFile);
	wcscat(iniFile, L"d3dx.ini");
	wcscat(logFilename, L"d3d11_log.txt");

	// Log all settings that are _enabled_, in order, 
	// so that there is no question what settings we are using.

	// [Logging]
	// Not using the helper function for this one since logging isn't enabled yet
	if (GetPrivateProfileInt(L"Logging", L"calls", 1, iniFile))
	{
		if (!LogFile)
			LogFile = _wfsopen(logFilename, L"w", _SH_DENYNO);
		LogInfo("\nD3D11 DLL starting init - v %s - %s\n\n", VER_FILE_VERSION_STR, LogTime().c_str());
		LogInfo("----------- d3dx.ini settings -----------\n");
	}
	LogInfo("[Logging]\n");
	LogInfo("  calls=1\n");

	GetIniSections(sections, iniFile);

	G->gLogInput = GetIniBool(L"Logging", L"input", false, iniFile, NULL);
	gLogDebug = GetIniBool(L"Logging", L"debug", false, iniFile, NULL);

	// Unbuffered logging to remove need for fflush calls, and r/w access to make it easy
	// to open active files.
	if (LogFile && GetIniBool(L"Logging", L"unbuffered", false, iniFile, NULL))
	{
		int unbuffered = setvbuf(LogFile, NULL, _IONBF, 0);
		LogInfo("    unbuffered return: %d\n", unbuffered);
	}

	// Set the CPU affinity based upon d3dx.ini setting.  Useful for debugging and shader hunting in AC3.
	if (GetIniBool(L"Logging", L"force_cpu_affinity", false, iniFile, NULL))
	{
		DWORD one = 0x01;
		BOOL affinity = SetProcessAffinityMask(GetCurrentProcess(), one);
		LogInfo("    force_cpu_affinity return: %s\n", affinity ? "true" : "false");
	}

	// If specified in Logging section, wait for Attach to Debugger.
	int debugger = GetIniInt(L"Logging", L"waitfordebugger", 0, iniFile, NULL);
	if (debugger > 0)
	{
		do
		{
			Sleep(250);
		} while (!IsDebuggerPresent());
		if (debugger > 1)
			__debugbreak();
	}

	G->dump_all_profiles = GetIniBool(L"Logging", L"dump_all_profiles", false, iniFile, NULL);

	// [System]
	LogInfo("[System]\n");
	GetPrivateProfileString(L"System", L"proxy_d3d11", 0, G->CHAIN_DLL_PATH, MAX_PATH, iniFile);
	if (G->CHAIN_DLL_PATH[0])
		LogInfoW(L"  proxy_d3d11=%s\n", G->CHAIN_DLL_PATH);
	if (GetPrivateProfileString(L"System", L"hook", 0, setting, MAX_PATH, iniFile))
	{
		LogInfoW(L"  hook=%s\n", setting);
		G->enable_hooks = parse_enum_option_string<wchar_t *, EnableHooks>
			(EnableHooksNames, setting, NULL);
	}
	G->enable_dxgi1_2 = GetIniInt(L"System", L"allow_dxgi1_2", 0, iniFile, NULL);
	G->enable_check_interface = GetIniBool(L"System", L"allow_check_interface", false, iniFile, NULL);
	G->enable_create_device = GetIniInt(L"System", L"allow_create_device", 0, iniFile, NULL);
	G->enable_platform_update = GetIniBool(L"System", L"allow_platform_update", false, iniFile, NULL);

	// [Device] (DXGI parameters)
	LogInfo("[Device]\n");
	G->SCREEN_WIDTH = GetIniInt(L"Device", L"width", -1, iniFile, NULL);
	G->SCREEN_HEIGHT = GetIniInt(L"Device", L"height", -1, iniFile, NULL);
	G->SCREEN_REFRESH = GetIniInt(L"Device", L"refresh_rate", -1, iniFile, NULL);

	if (GetPrivateProfileString(L"Device", L"filter_refresh_rate", 0, setting, MAX_PATH, iniFile))
	{
		swscanf_s(setting, L"%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
			G->FILTER_REFRESH + 0, G->FILTER_REFRESH + 1, G->FILTER_REFRESH + 2, G->FILTER_REFRESH + 3, 
			G->FILTER_REFRESH + 4, G->FILTER_REFRESH + 5, G->FILTER_REFRESH + 6, G->FILTER_REFRESH + 7, 
			G->FILTER_REFRESH + 8, G->FILTER_REFRESH + 9);
		LogInfoW(L"  filter_refresh_rate=%s\n", setting);
	}

	G->SCREEN_FULLSCREEN = GetIniInt(L"Device", L"full_screen", -1, iniFile, NULL);
	RegisterIniKeyBinding(L"Device", L"toggle_full_screen", iniFile, ToggleFullScreen, NULL, 0, NULL);
	G->gForceStereo = GetIniInt(L"Device", L"force_stereo", 0, iniFile, NULL);
	G->SCREEN_ALLOW_COMMANDS = GetIniBool(L"Device", L"allow_windowcommands", false, iniFile, NULL);

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
	LogInfo("[Stereo]\n");
	bool automaticMode = GetIniBool(L"Stereo", L"automatic_mode", false, iniFile, NULL);				// in NVapi dll
	G->gCreateStereoProfile = GetIniBool(L"Stereo", L"create_profile", false, iniFile, NULL);
	G->gSurfaceCreateMode = GetIniInt(L"Stereo", L"surface_createmode", -1, iniFile, NULL);
	G->gSurfaceSquareCreateMode = GetIniInt(L"Stereo", L"surface_square_createmode", -1, iniFile, NULL);
	G->gForceNoNvAPI = GetIniBool(L"Stereo", L"force_no_nvapi", false, iniFile, NULL);

	// [Rendering]
	LogInfo("[Rendering]\n");

	G->shader_hash_type = ShaderHashType::FNV;
	if (GetPrivateProfileString(L"Rendering", L"shader_hash", 0, setting, MAX_PATH, iniFile)) {
		G->shader_hash_type = lookup_enum_val<wchar_t *, ShaderHashType>
			(ShaderHashNames, setting, ShaderHashType::INVALID);
		if (G->shader_hash_type == ShaderHashType::INVALID) {
			LogInfoW(L"  WARNING: Unknown shader_hash \"%s\"\n", setting);
			G->shader_hash_type = ShaderHashType::FNV;
			BeepFailure2();
		} else {
			LogInfoW(L"  shader_hash=%s\n", setting);
		}
	}

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

	G->CACHE_SHADERS = GetIniBool(L"Rendering", L"cache_shaders", false, iniFile, NULL);
	G->ENABLE_CRITICAL_SECTION = GetIniBool(L"Rendering", L"use_criticalsection", false, iniFile, NULL);
	G->SCISSOR_DISABLE = GetIniBool(L"Rendering", L"rasterizer_disable_scissor", false, iniFile, NULL);
	G->track_texture_updates = GetIniBool(L"Rendering", L"track_texture_updates", false, iniFile, NULL);

	G->EXPORT_FIXED = GetIniBool(L"Rendering", L"export_fixed", false, iniFile, NULL);
	G->EXPORT_SHADERS = GetIniBool(L"Rendering", L"export_shaders", false, iniFile, NULL);
	G->EXPORT_HLSL = GetIniInt(L"Rendering", L"export_hlsl", 0, iniFile, NULL);
	G->EXPORT_BINARY = GetIniBool(L"Rendering", L"export_binary", false, iniFile, NULL);
	G->DumpUsage = GetIniBool(L"Rendering", L"dump_usage", false, iniFile, NULL);

	G->StereoParamsReg = GetIniInt(L"Rendering", L"stereo_params", 125, iniFile, NULL);
	G->IniParamsReg = GetIniInt(L"Rendering", L"ini_params", 120, iniFile, NULL);
	if (G->StereoParamsReg >= D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT) {
		LogInfo("WARNING: stereo_params=%i out of range\n", G->StereoParamsReg);
		BeepFailure2();
		G->StereoParamsReg = -1;
	}
	if (G->IniParamsReg >= D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT) {
		LogInfo("WARNING: ini_params=%i out of range\n", G->IniParamsReg);
		BeepFailure2();
		G->IniParamsReg = -1;
	}

	if (G->SHADER_PATH[0])
		LogInfoW(L"  override_directory=%s\n", G->SHADER_PATH);
	if (G->SHADER_CACHE_PATH[0])
		LogInfoW(L"  cache_directory=%s\n", G->SHADER_CACHE_PATH);


	// Automatic section 
	G->FIX_SV_Position = GetIniBool(L"Rendering", L"fix_sv_position", false, iniFile, NULL);
	G->FIX_Light_Position = GetIniBool(L"Rendering", L"fix_light_position", false, iniFile, NULL);
	G->FIX_Recompile_VS = GetIniBool(L"Rendering", L"recompile_all_vs", false, iniFile, NULL);
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
	G->hunting = GetIniInt(L"Hunting", L"hunting", 0, iniFile, NULL);

	G->marking_mode = MARKING_MODE_SKIP;
	if (GetPrivateProfileString(L"Hunting", L"marking_mode", 0, setting, MAX_PATH, iniFile)) {
		if (!wcscmp(setting, L"skip")) G->marking_mode = MARKING_MODE_SKIP;
		if (!wcscmp(setting, L"mono")) G->marking_mode = MARKING_MODE_MONO;
		if (!wcscmp(setting, L"original")) G->marking_mode = MARKING_MODE_ORIGINAL;
		if (!wcscmp(setting, L"zero")) G->marking_mode = MARKING_MODE_ZERO;
		if (!wcscmp(setting, L"pink")) G->marking_mode = MARKING_MODE_PINK;
		LogInfoW(L"  marking_mode=%d\n", G->marking_mode);
	}

	G->mark_snapshot = GetIniInt(L"Hunting", L"mark_snapshot", 0, iniFile, NULL);

	RegisterHuntingKeyBindings(iniFile);
	RegisterPresetKeyBindings(sections, iniFile);

	ParseResourceSections(sections, iniFile);

	// Splitting the enumeration of these sections out from parsing them as
	// they can be referenced from other command list sections (via the run
	// command), including sections of the same type. Most of the other
	// sections don't need this so long as we parse them in an appropriate
	// order so that sections that can be referred to are parsed before
	// sections that can refer to them (e.g. Resource sections are parsed
	// before all command list sections for this reason), but these are
	// special since they can both refer to other sections and be referred
	// to by other sections, and we don't want the parse order to determine
	// if the reference will work or not.
	EnumerateCustomShaderSections(sections, iniFile);
	EnumerateExplicitCommandListSections(sections, iniFile);

	ParseCustomShaderSections(iniFile);
	ParseExplicitCommandListSections(iniFile);

	ParseShaderOverrideSections(sections, iniFile);
	ParseTextureOverrideSections(sections, iniFile);

	LogInfo("[Present]\n");
	G->present_command_list.clear();
	G->post_present_command_list.clear();
	ParseCommandList(L"Present", iniFile, &G->present_command_list, &G->post_present_command_list, NULL);

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

	LogInfo("[Profile]\n");
	ParseDriverProfile(iniFile);

	LogInfo("\n");
}

// This variant is called by the profile manager helper with the path to the
// game's executable passed in. It doesn't need to parse most of the config,
// only the [Profile] section and some of the logging. It uses a separate log
// file from the main DLL.
void LoadProfileManagerConfig(const wchar_t *exe_path)
{
	wchar_t iniFile[MAX_PATH], logFilename[MAX_PATH];

	G->gInitialized = true;

	if (wcscpy_s(iniFile, MAX_PATH, exe_path))
		DoubleBeepExit();
	wcsrchr(iniFile, L'\\')[1] = 0;
	wcscpy(logFilename, iniFile);
	wcscat(iniFile, L"d3dx.ini");
	wcscat(logFilename, L"d3d11_profile_log.txt");

	// [Logging]
	// Not using the helper function for this one since logging isn't enabled yet
	if (GetPrivateProfileInt(L"Logging", L"calls", 1, iniFile))
	{
		if (!LogFile)
			LogFile = _wfsopen(logFilename, L"w", _SH_DENYNO);
		LogInfo("\n3DMigoto profile helper starting init - v %s - %s\n\n", VER_FILE_VERSION_STR, LogTime().c_str());
		LogInfo("----------- d3dx.ini settings -----------\n");
	}
	LogInfo("[Logging]\n");
	LogInfo("  calls=1\n");

	gLogDebug = GetIniBool(L"Logging", L"debug", false, iniFile, NULL);

	// Unbuffered logging to remove need for fflush calls, and r/w access to make it easy
	// to open active files.
	if (LogFile && GetIniBool(L"Logging", L"unbuffered", false, iniFile, NULL))
	{
		int unbuffered = setvbuf(LogFile, NULL, _IONBF, 0);
		LogInfo("    unbuffered return: %d\n", unbuffered);
	}

	LogInfo("[Profile]\n");
	ParseDriverProfile(iniFile);

	LogInfo("\n");
}


void ReloadConfig(HackerDevice *device)
{
	ID3D11DeviceContext* realContext; device->GetImmediateContext(&realContext);
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	HRESULT hr;

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
	hr = realContext->Map(device->mIniTexture, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	if (FAILED(hr)) {
		LogInfo("Failed to update IniParams\n");
		return;
	}
	memcpy(mappedResource.pData, &G->iniParams, sizeof(G->iniParams));
	realContext->Unmap(device->mIniTexture, 0);
}
