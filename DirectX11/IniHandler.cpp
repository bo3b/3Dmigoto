#include "IniHandler.h"

#include <algorithm>
#include <string>
#include <strsafe.h>
#include <fstream>

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
	{L"Preset", true},
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


// Case insensitive version of the wstring hashing and equality functions for
// case insensitive maps that we can use to look up ini sections and keys:
struct WStringInsensitiveHash {
	size_t operator()(const wstring &s) const
	{
		std::wstring l;
		std::hash<std::wstring> whash;

		l.resize(s.size());
		std::transform(s.begin(), s.end(), l.begin(), ::towlower);
		return whash(l);
	}
};
struct WStringInsensitiveEquality {
	size_t operator()(const wstring &x, const wstring &y) const
	{
		return _wcsicmp(x.c_str(), y.c_str()) == 0;
	}
};

// Whereas settings within a section are in the same order they were in the ini
// file. This will become more important as shader overrides gains more
// functionality and dependencies between different features form:
typedef std::vector<std::pair<wstring, wstring>> IniSectionVector;

// Unsorted maps for fast case insensitive key lookups by name
typedef std::unordered_map<wstring, wstring, WStringInsensitiveHash, WStringInsensitiveEquality> IniSectionMap;
typedef std::unordered_set<wstring, WStringInsensitiveHash, WStringInsensitiveEquality> IniSectionSet;

struct IniSection {
	IniSectionMap kv_map;
	IniSectionVector kv_vec;
};

// std::map is used so this is sorted for iterating over a prefix:
typedef std::map<wstring, IniSection, WStringInsensitiveLess> IniSections;

IniSections ini_sections;

// Returns an iterator to the first element in a set that does not begin with
// prefix in a case insensitive way. Combined with set::lower_bound, this can
// be used to iterate over all elements in the sections set that begin with a
// given prefix.
static IniSections::iterator prefix_upper_bound(IniSections &sections, wstring &prefix)
{
	IniSections::iterator i;

	for (i = sections.lower_bound(prefix); i != sections.end(); i++) {
		if (_wcsnicmp(i->first.c_str(), prefix.c_str(), prefix.length()) > 0)
			return i;
	}

	return sections.end();
}

static void ParseIniSectionLine(wstring *wline, wstring *section,
		bool *warn_duplicates, bool *warn_lines_without_equals,
		IniSectionVector **section_vector)
{
	size_t first, last;
	bool inserted;

	// To match the behaviour of GetPrivateProfileString, we use up until
	// the first ] as the section name. If there is no ] character, we use
	// the rest of the line.
	last = wline->find(L']');
	if (last == wline->npos)
		last = wline->length();

	// Strip whitespace:
	first = wline->find_first_not_of(L" \t", 1);
	last = wline->find_last_not_of(L" \t", last - 1);
	*section = wline->substr(first, last - first + 1);

	// If we find a duplicate section we only parse the first one to match
	// the behaviour of GetPrivateProfileString. We might actually want to
	// think about forgetting conforming to the old API as there is some
	// advantage to being able to append additional values to a previous
	// section to help organise the d3dx.ini into functional sections, and
	// we did get a feature request to that effect at one point. If we do
	// that though, we risk confusion if the user didn't realise they
	// already had a second section of the same name, which isn't ideal -
	// my thinking was that the user would have to e.g.  explicitly mark a
	// section as continued elsewhere.  Also, if we do that keep in mind
	// that some types of sections are considered duplicates if their hash
	// key matches, which would have to be handled elsewhere.  For now,
	// continue warning about duplicate sections and match the old
	// behaviour.
	inserted = ini_sections.emplace(*section, IniSection{}).second;
	if (!inserted) {
		LogInfo("WARNING: Duplicate section found in d3dx.ini: [%S]\n",
				section->c_str());
		BeepFailure2();
		section->clear();
		*section_vector = NULL;
		return;
	}

	*section_vector = &ini_sections[*section].kv_vec;

	// Some of the code below has been moved from the old GetIniSection()
	*warn_duplicates = true;
	*warn_lines_without_equals = true;

	// Sections that utilise a command list are allowed to have duplicate
	// keys, while other sections are not. The command list parser will
	// still check for duplicate keys that are not part of the command
	// list.
	if (IsCommandListSection(section->c_str()))
		*warn_duplicates = false;
	else if (!IsRegularSection(section->c_str())) {
		LogInfo("WARNING: Unknown section in d3dx.ini: [%S]\n", section->c_str());
		BeepFailure2();
	}

	if (DoesSectionAllowLinesWithoutEquals(section->c_str()))
		*warn_lines_without_equals = false;
}

static void ParseIniKeyValLine(wstring *wline, wstring *section,
		bool warn_duplicates, bool warn_lines_without_equals,
		IniSectionVector *section_vector)
{
	size_t first, last, delim;
	wstring key, val;
	bool inserted;

	if (section->empty() || section_vector == NULL) {
		LogInfo("WARNING: d3dx.ini entry outside of section: %S\n",
				wline->c_str());
		BeepFailure2();
		return;
	}

	// Key / Val pair
	delim = wline->find(L"=");
	if (delim != wline->npos) {
		// Strip whitespace around delimiter:
		last = wline->find_last_not_of(L" \t", delim - 1);
		key = wline->substr(0, last + 1);
		first = wline->find_first_not_of(L" \t", delim + 1);
		if (first != wline->npos)
			val = wline->substr(first);

		// We use "at" on the sections to access an existing
		// section (alternatively we could use the [] operator
		// to permit it to be created if it doesn't exist), but
		// we use emplace within the section so that only the
		// first item with a given key is inserted to match the
		// behaviour of GetPrivateProfileString for duplicate
		// keys within a single section:
		inserted = ini_sections.at(*section).kv_map.emplace(key, val).second;
		if (warn_duplicates && !inserted) {
			LogInfo("WARNING: Duplicate key found in d3dx.ini: [%S] %S\n",
					section->c_str(), key.c_str());
			BeepFailure2();
		}
	} else {
		// No = on line, don't store in key lookup maps to
		// match the behaviour of GetPrivateProfileString, but
		// we will store it in the section vector structure for the
		// profile parser to process.
		if (warn_lines_without_equals) {
			LogInfo("WARNING: Malformed line in d3dx.ini: [%S] \"%S\"\n",
					section->c_str(), wline->c_str());
			BeepFailure2();
			return;
		}
	}

	section_vector->emplace_back(key, val);
}


// Parse the ini file into data structures. We used to use the
// GetPrivateProfile family of Windows API calls to parse the ini file, but
// they have the disadvantage that they open and parse the whole ini file every
// time they are called, which can lead to lengthy ini files taking a long time
// to parse (e.g. Dreamfall Chapters takes around 1 minute 45). By reading the
// ini file once we can drastically reduce that time.
//
// I considered using a third party library to provide this, but eventually
// decided against it - ini files are relatively simple and easy to parse
// ourselves, and we don't strictly adhere to the ini spec since we allow for
// repeated keys and lines without equals signs, and the order of lines is
// important in some sections. We could rely on the Windows APIs to provide
// these guarantees because Microsoft is highly unlikely to change their
// behaviour, but the same cannot be said of a third party library. Therefore,
// let's just do it ourselves to be sure it meets our requirements.
//
// NOTE: If adding any debugging / logging into this routine and expect to see
// it, make sure you delay calling it until after the log file has been opened!
static void ParseIni(const wchar_t *ini)
{
	string aline;
	wstring wline, section, key, val;
	size_t first, last;
	IniSectionVector *section_vector = NULL;
	bool warn_duplicates = true;
	bool warn_lines_without_equals = true;

	ini_sections.clear();

	ifstream f(ini, ios::in, _SH_DENYNO);
	if (!f) {
		LogInfo("  Error opening d3dx.ini\n");
		return;
	}

	while (std::getline(f, aline)) {
		// Convert to wstring for compatibility with GetPrivateProfile*
		// APIs. If we assume the d3dx.ini is always ASCII we could
		// drop this, but that would require us to change a great many
		// types throughout 3DMigoto, so leave that for another day.
		wline = wstring(aline.begin(), aline.end());

		// Strip preceding and trailing whitespace:
		first = wline.find_first_not_of(L" \t");
		last = wline.find_last_not_of(L" \t");
		if (first != wline.npos)
			wline = wline.substr(first, last - first + 1);

		if (wline.empty())
			continue;

		// Comments are lines that start with a semicolon as the first
		// non-whitespace character that we want to skip over (note
		// that a semicolon appearing in the middle of a line is *NOT*
		// a comment in an ini file. It might be tempting to treat them
		// as comments since a lot of people do seem to try to do that,
		// but there may be cases where a semicolon is part of valid
		// syntax and I am hesitant to change that underlying handling
		// here, at least not without auditing most of the d3dx.ini
		// files already in the wild. Let's at least try not to add any
		// new syntax that includes semicolons anyway!)
		if (wline[0] == L';')
			continue;

		// Section?
		if (wline[0] == L'[') {
			ParseIniSectionLine(&wline, &section, &warn_duplicates,
					    &warn_lines_without_equals,
					    &section_vector);
			continue;
		}

		ParseIniKeyValLine(&wline, &section, warn_duplicates,
				   warn_lines_without_equals, section_vector);
	}
}

// This emulates the behaviour of the old GetPrivateProfileString API to
// facilitate switching to our own ini parser. Later we might consider changing
// the return values (e.g. return found/not found instead of string length),
// but we need to check that we don't depend on the existing behaviour first.
// Note that it is the only GetIni...() function that does not perform any
// automatic logging of present values
int GetIniString(const wchar_t *section, const wchar_t *key, const wchar_t *def,
		 wchar_t *ret, unsigned size)
{
	int rc;

	try {
		wstring &val = ini_sections.at(section).kv_map.at(key);
		if (wcscpy_s(ret, size, val.c_str())) {
			// Funky return code of GetPrivateProfileString Not
			// sure if we depend on this - if we don't I'd like a
			// nicer return code or to raise an exception. Note
			// that wcscpy_s will have returned an empty string,
			// while the original GetPrivateProfileString would
			// have only truncated the string.
			LogInfo("  WARNING: [%S]%S=%S too long\n",
					section, key, val.c_str());
			BeepFailure2();
			rc = size - 1;
		} else {
			// I'd also rather not have to calculate the string
			// length if we don't use it
			rc = (int)wcslen(ret);
		}
	} catch (std::out_of_range) {
		if (def) {
			if (wcscpy_s(ret, size, def)) {
				// If someone passed in a default value that is
				// too long, treat it as a programming error
				// and terminate:
				DoubleBeepExit();
			} else
				rc = (int)wcslen(ret);
		} else {
			// Return an empty string
			ret[0] = L'\0';
			rc = 0;
		}
	}

	return rc;
}

// Helper functions to parse common types and log their values. TODO: Convert
// more of this file to use these where appropriate
static int GetIniStringAndLog(const wchar_t *section, const wchar_t *key,
		const wchar_t *def, wchar_t *ret, unsigned size)
{
	int rc = GetIniString(section, key, def, ret, size);

	if (rc)
		LogInfo("  %S=%S\n", key, ret);

	return rc;
}

static float GetIniFloat(const wchar_t *section, const wchar_t *key, float def, bool *found)
{
	wchar_t val[32];
	float ret = def;
	int len;

	if (found)
		*found = false;

	if (GetIniString(section, key, 0, val, 32)) {
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

int GetIniInt(const wchar_t *section, const wchar_t *key, int def, bool *found)
{
	wchar_t val[32];
	int ret = def;
	int len;

	if (found)
		*found = false;

	// Not using GetPrivateProfileInt as it doesn't tell us if the key existed
	if (GetIniString(section, key, 0, val, 32)) {
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

static bool GetIniBool(const wchar_t *section, const wchar_t *key, bool def, bool *found)
{
	wchar_t val[32];
	bool ret = def;

	if (found)
		*found = false;

	// Not using GetPrivateProfileInt as it doesn't tell us if the key existed
	if (GetIniString(section, key, 0, val, 32)) {
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

static int GetIniEnum(const wchar_t *section, const wchar_t *key, int def, bool *found,
		wchar_t *prefix, wchar_t *names[], int names_len, int first)
{
	wchar_t val[MAX_PATH];
	int ret = def;

	if (found)
		*found = false;

	if (GetIniString(section, key, 0, val, MAX_PATH)) {
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

static void GetIniSection(IniSectionVector **key_vals, const wchar_t *section)
{
	static IniSectionVector empty_section_vector;

	try {
		*key_vals = &ini_sections.at(section).kv_vec;
	} catch (std::out_of_range) {
		LogInfo("WARNING: GetIniSection() called on a section not in the ini_sections map: %S\n", section);
		*key_vals = &empty_section_vector;
	}
}

static void RegisterPresetKeyBindings()
{
	KeyOverrideType type;
	wchar_t key[MAX_PATH];
	wchar_t buf[MAX_PATH];
	KeyOverrideBase *preset;
	int delay, release_delay;
	IniSections::iterator lower, upper, i;

	lower = ini_sections.lower_bound(wstring(L"Key"));
	upper = prefix_upper_bound(ini_sections, wstring(L"Key"));

	for (i = lower; i != upper; i++) {
		const wchar_t *id = i->first.c_str();

		LogInfo("[%S]\n", id);

		if (!GetIniString(id, L"Key", 0, key, MAX_PATH)) {
			LogInfo("  WARNING: [%S] missing Key=\n", id);
			BeepFailure2();
			continue;
		}

		type = KeyOverrideType::ACTIVATE;

		if (GetIniString(id, L"type", 0, buf, MAX_PATH)) {
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

		delay = GetIniInt(id, L"delay", 0, NULL);
		release_delay = GetIniInt(id, L"release_delay", 0, NULL);

		if (type == KeyOverrideType::CYCLE)
			preset = new KeyOverrideCycle();
		else
			preset = new KeyOverride(type);
		preset->ParseIniSection(id);

		RegisterKeyBinding(L"Key", key, preset, 0, delay, release_delay);
	}
}

static void ParsePresetOverrideSections()
{
	wstring preset_id;
	PresetOverride *preset;
	IniSections::iterator lower, upper, i;

	presetOverrides.clear();

	lower = ini_sections.lower_bound(wstring(L"Preset"));
	upper = prefix_upper_bound(ini_sections, wstring(L"Preset"));

	for (i = lower; i != upper; i++) {
		const wchar_t *id = i->first.c_str();

		LogInfo("[%S]\n", id);

		// Remove prefix and convert to lower case
		preset_id = id + 6;
		std::transform(preset_id.begin(), preset_id.end(), preset_id.begin(), ::towlower);

		// Read parameters from ini
		presetOverrides[preset_id];
		preset = &presetOverrides[preset_id];
		preset->ParseIniSection(id);
	}
}

static void ParseResourceSections()
{
	IniSections::iterator lower, upper, i;
	wstring resource_id;
	CustomResource *custom_resource;
	wchar_t setting[MAX_PATH], path[MAX_PATH];

	customResources.clear();

	lower = ini_sections.lower_bound(wstring(L"Resource"));
	upper = prefix_upper_bound(ini_sections, wstring(L"Resource"));
	for (i = lower; i != upper; i++) {
		LogInfoW(L"[%s]\n", i->first.c_str());

		// Convert section name to lower case so our keys will be
		// consistent in the unordered_map:
		resource_id = i->first;
		std::transform(resource_id.begin(), resource_id.end(), resource_id.begin(), ::towlower);

		// Empty Resource sections are valid (think of them as a
		// sort of variable declaration), so explicitly construct a
		// CustomResource for each one. Use the [] operator so the
		// default constructor will be used:
		custom_resource = &customResources[resource_id];
		custom_resource->name = i->first;

		custom_resource->max_copies_per_frame =
			GetIniInt(i->first.c_str(), L"max_copies_per_frame", 0, NULL);

		if (GetIniString(i->first.c_str(), L"filename", 0, setting, MAX_PATH)) {
			LogInfoW(L"  filename=%s\n", setting);
			GetModuleFileName(0, path, MAX_PATH);
			wcsrchr(path, L'\\')[1] = 0;
			wcscat(path, setting);
			custom_resource->filename = path;
		}

		if (GetIniString(i->first.c_str(), L"type", 0, setting, MAX_PATH)) {
			custom_resource->override_type = lookup_enum_val<const wchar_t *, CustomResourceType>
				(CustomResourceTypeNames, setting, CustomResourceType::INVALID);
			if (custom_resource->override_type == CustomResourceType::INVALID) {
				LogInfo("  WARNING: Unknown type \"%S\"\n", setting);
				BeepFailure2();
			} else {
				LogInfo("  type=%S\n", setting);
			}
		}

		if (GetIniString(i->first.c_str(), L"mode", 0, setting, MAX_PATH)) {
			custom_resource->override_mode = lookup_enum_val<const wchar_t *, CustomResourceMode>
				(CustomResourceModeNames, setting, CustomResourceMode::DEFAULT);
			if (custom_resource->override_mode == CustomResourceMode::DEFAULT) {
				LogInfo("  WARNING: Unknown mode \"%S\"\n", setting);
				BeepFailure2();
			} else {
				LogInfo("  mode=%S\n", setting);
			}
		}

		if (GetIniString(i->first.c_str(), L"format", 0, setting, MAX_PATH)) {
			custom_resource->override_format = ParseFormatString(setting);
			if (custom_resource->override_format == (DXGI_FORMAT)-1) {
				LogInfo("  WARNING: Unknown format \"%S\"\n", setting);
				BeepFailure2();
			} else {
				LogInfo("  format=%s\n", TexFormatStr(custom_resource->override_format));
			}
		}

		custom_resource->override_width = GetIniInt(i->first.c_str(), L"width", -1, NULL);
		custom_resource->override_height = GetIniInt(i->first.c_str(), L"height", -1, NULL);
		custom_resource->override_depth = GetIniInt(i->first.c_str(), L"depth", -1, NULL);
		custom_resource->override_mips = GetIniInt(i->first.c_str(), L"mips", -1, NULL);
		custom_resource->override_array = GetIniInt(i->first.c_str(), L"array", -1, NULL);
		custom_resource->override_msaa = GetIniInt(i->first.c_str(), L"msaa", -1, NULL);
		custom_resource->override_msaa_quality = GetIniInt(i->first.c_str(), L"msaa_quality", -1, NULL);
		custom_resource->override_byte_width = GetIniInt(i->first.c_str(), L"byte_width", -1, NULL);
		custom_resource->override_stride = GetIniInt(i->first.c_str(), L"stride", -1, NULL);

		custom_resource->width_multiply = GetIniFloat(i->first.c_str(), L"width_multiply", 1.0f, NULL);
		custom_resource->height_multiply = GetIniFloat(i->first.c_str(), L"height_multiply", 1.0f, NULL);

		if (GetIniStringAndLog(i->first.c_str(), L"bind_flags", 0, setting, MAX_PATH)) {
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
static void ParseCommandList(const wchar_t *id,
		CommandList *pre_command_list, CommandList *post_command_list,
		wchar_t *whitelist[])
{
	IniSectionVector *section = NULL;
	IniSectionVector::iterator entry;
	wstring *key, *val;
	const wchar_t *key_ptr;
	CommandList *command_list, *explicit_command_list;
	IniSectionSet whitelisted_keys;
	int i;

	// Safety check to make sure we are keeping the command list section
	// list up to date:
	if (!IsCommandListSection(id)) {
		LogInfoW(L"BUG: ParseCommandList() called on a section not in the CommandListSections list: %s\n", id);
		DoubleBeepExit();
	}

	GetIniSection(&section, id);
	for (entry = section->begin(); entry < section->end(); entry++) {
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

		if (ParseCommandListGeneralCommands(id, key_ptr, val, explicit_command_list, pre_command_list, post_command_list))
			goto log_continue;

		if (ParseCommandListIniParamOverride(id, key_ptr, val, command_list))
			goto log_continue;

		if (ParseCommandListResourceCopyDirective(id, key_ptr, val, command_list))
			goto log_continue;

		LogInfoW(L"  WARNING: Unrecognised entry: %ls=%ls\n", key->c_str(), val->c_str());
		BeepFailure2();
		continue;
log_continue:
		LogInfoW(L"  %ls=%s\n", key->c_str(), val->c_str());
	}
}

static void ParseDriverProfile()
{
	IniSectionVector *section = NULL;
	IniSectionVector::iterator entry;
	wstring *lhs, *rhs;

	// Arguably we should only parse this section the first time since the
	// settings will only be applied on startup.
	profile_settings.clear();

	GetIniSection(&section, L"Profile");
	for (entry = section->begin(); entry < section->end(); entry++) {
		lhs = &entry->first;
		rhs = &entry->second;

		parse_ini_profile_line(lhs, rhs);
	}
}

// List of keys in [ShaderOverride] sections that are processed in this
// function. Used by ParseCommandList to find any unrecognised lines.
wchar_t *ShaderOverrideIniKeys[] = {
	L"hash",
	L"allow_duplicate_hash",
	L"separation",
	L"convergence",
	L"depth_filter",
	L"partner",
	L"iteration",
	L"analyse_options",
	L"model",
	L"disable_scissor",
	NULL
};
static void ParseShaderOverrideSections()
{
	IniSections::iterator lower, upper, i;
	wchar_t setting[MAX_PATH];
	const wchar_t *id;
	ShaderOverride *override;
	UINT64 hash;
	bool duplicate, allow_duplicates;

	// Lock entire routine. This can be re-inited live.  These shaderoverrides
	// are unlikely to be changing much, but for consistency.
	if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);

	G->mShaderOverrideMap.clear();

	lower = ini_sections.lower_bound(wstring(L"ShaderOverride"));
	upper = prefix_upper_bound(ini_sections, wstring(L"ShaderOverride"));
	for (i = lower; i != upper; i++) {
		id = i->first.c_str();

		LogInfo("[%S]\n", id);

		if (!GetIniString(id, L"Hash", 0, setting, MAX_PATH)) {
			LogInfo("  WARNING: [%S] missing Hash=\n", id);
			BeepFailure2();
			continue;
		}
		swscanf_s(setting, L"%16llx", &hash);
		LogInfo("  Hash=%016llx\n", hash);

		duplicate = !!G->mShaderOverrideMap.count(hash);
		override = &G->mShaderOverrideMap[hash];

		// We permit hash= to be duplicate, but only if every section
		// indicates they are ok with it, and the section names still
		// have to be distinct. This is intended that scripts will set
		// this flag on any sections they create so that if a user
		// creates a shaderoverride with the same hash they will get a
		// warning at first, but can choose to allow it so that they
		// can add their own commands without having to merge them with
		// the section from the script, allowing all the auto generated
		// sections to be grouped together. The section names still
		// have to be distinct, which offers protection against scripts
		// adding multiple identical sections if run multiple times.
		// Note that you won't get warnings of duplicate settings
		// between the sections, but at least we try not to clobber
		// their values from earlier sections with the defaults.
		allow_duplicates = GetIniBool(id, L"allow_duplicate_hash", false, NULL)
				   && override->allow_duplicate_hashes;

		if (duplicate && !allow_duplicates) {
			LogInfo("  WARNING: Duplicate ShaderOverride hash: %016llx\n", hash);
			BeepFailure2();
		}

		override->allow_duplicate_hashes = allow_duplicates;

		override->separation = GetIniFloat(id, L"Separation", override->separation, NULL);
		override->convergence = GetIniFloat(id, L"Convergence", override->convergence, NULL);

		if (GetIniString(id, L"depth_filter", 0, setting, MAX_PATH)) {
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
		if (GetIniString(id, L"partner", 0, setting, MAX_PATH)) {
			swscanf_s(setting, L"%16llx", &override->partner_hash);
			LogInfo("  partner=%016llx\n", override->partner_hash);
		}

		if (GetIniString(id, L"Iteration", 0, setting, MAX_PATH))
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

		if (GetIniString(id, L"analyse_options", 0, setting, MAX_PATH)) {
			LogInfoW(L"  analyse_options=%s\n", setting);
			override->analyse_options = parse_enum_option_string<wchar_t *, FrameAnalysisOptions>
				(FrameAnalysisOptionNames, setting, NULL);
		}

		if (GetIniString(id, L"model", 0, setting, MAX_PATH)) {
			wcstombs(override->model, setting, ARRAYSIZE(override->model));
			override->model[ARRAYSIZE(override->model) - 1] = '\0';
			LogInfo("  model=%s\n", override->model);
		}

		override->disable_scissor = GetIniInt(id, L"disable_scissor", override->disable_scissor, NULL);

		ParseCommandList(id, &override->command_list, &override->post_command_list, ShaderOverrideIniKeys);
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
static void ParseTextureOverrideSections()
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

	lower = ini_sections.lower_bound(wstring(L"TextureOverride"));
	upper = prefix_upper_bound(ini_sections, wstring(L"TextureOverride"));

	for (i = lower; i != upper; i++) 
	{
		id = i->first.c_str();

		LogInfo("[%S]\n", id);

		if (!GetIniString(id, L"Hash", 0, setting, MAX_PATH)) {
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

		override->stereoMode = GetIniInt(id, L"StereoMode", -1, NULL);
		override->format = GetIniInt(id, L"Format", -1, NULL);
		override->width = GetIniInt(id, L"Width", -1, NULL);
		override->height = GetIniInt(id, L"Height", -1, NULL);

		if (GetIniString(id, L"Iteration", 0, setting, MAX_PATH))
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

		if (GetIniString(id, L"analyse_options", 0, setting, MAX_PATH)) {
			LogInfoW(L"  analyse_options=%s\n", setting);
			override->analyse_options = parse_enum_option_string<wchar_t *, FrameAnalysisOptions>
				(FrameAnalysisOptionNames, setting, NULL);
		}

		override->filter_index = GetIniFloat(id, L"filter_index", 1.0f, NULL);

		override->expand_region_copy = GetIniBool(id, L"expand_region_copy", false, NULL);
		override->deny_cpu_read = GetIniBool(id, L"deny_cpu_read", false, NULL);

		ParseCommandList(id, &override->command_list, &override->post_command_list, TextureOverrideIniKeys);
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

static bool ParseBlendRenderTarget(
		D3D11_RENDER_TARGET_BLEND_DESC *desc,
		D3D11_RENDER_TARGET_BLEND_DESC *mask,
		const wchar_t *section, int index)
{
	wchar_t setting[MAX_PATH];
	bool override = false;
	wchar_t key[32];
	int ival;

	wcscpy(key, L"blend");
	if (index >= 0)
		swprintf_s(key, ARRAYSIZE(key), L"blend[%i]", index);
	if (GetIniString(section, key, 0, setting, MAX_PATH)) {
		override = true;

		// Special value to disable blending:
		if (!_wcsicmp(setting, L"disable")) {
			LogInfo("  %S=disable\n", key);
			desc->BlendEnable = false;
			mask->BlendEnable = 0;
			return true;
		}

		ParseBlendOp(key, setting,
				&desc->BlendOp,
				&desc->SrcBlend,
				&desc->DestBlend);
		mask->BlendOp = (D3D11_BLEND_OP)0;
		mask->SrcBlend = (D3D11_BLEND)0;
		mask->DestBlend = (D3D11_BLEND)0;
	}

	wcscpy(key, L"alpha");
	if (index >= 0)
		swprintf_s(key, ARRAYSIZE(key), L"alpha[%i]", index);
	if (GetIniString(section, key, 0, setting, MAX_PATH)) {
		override = true;
		ParseBlendOp(key, setting,
				&desc->BlendOpAlpha,
				&desc->SrcBlendAlpha,
				&desc->DestBlendAlpha);
		mask->BlendOpAlpha = (D3D11_BLEND_OP)0;
		mask->SrcBlendAlpha = (D3D11_BLEND)0;
		mask->DestBlendAlpha = (D3D11_BLEND)0;
	}

	wcscpy(key, L"mask");
	if (index >= 0)
		swprintf_s(key, ARRAYSIZE(key), L"mask[%i]", index);
	if (GetIniString(section, key, 0, setting, MAX_PATH)) {
		override = true;
		swscanf_s(setting, L"%x", &ival); // No suitable format string w/o overflow?
		desc->RenderTargetWriteMask = ival; // Use an intermediate to be safe
		mask->RenderTargetWriteMask = 0;
		LogInfo("  %S=0x%x\n", key, desc->RenderTargetWriteMask);
	}

	if (override) {
		desc->BlendEnable = true;
		mask->BlendEnable = 0;
	}

	return override;
}

static void ParseBlendState(CustomShader *shader, const wchar_t *section)
{
	D3D11_BLEND_DESC *desc = &shader->blend_desc;
	D3D11_BLEND_DESC *mask = &shader->blend_mask;
	wchar_t setting[MAX_PATH];
	wchar_t key[32];
	int i;
	bool found;

	memset(desc, 0, sizeof(D3D11_BLEND_DESC));
	memset(mask, 0xff, sizeof(D3D11_BLEND_DESC));

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
	if (ParseBlendRenderTarget(&desc->RenderTarget[0], &mask->RenderTarget[0], section, -1))
		shader->blend_override = 1;
	for (i = 1; i < 8; i++) {
		memcpy(&desc->RenderTarget[i], &desc->RenderTarget[0], sizeof(D3D11_RENDER_TARGET_BLEND_DESC));
		memcpy(&mask->RenderTarget[i], &mask->RenderTarget[0], sizeof(D3D11_RENDER_TARGET_BLEND_DESC));
	}

	// We check all render targets again with the [%i] syntax. We do the
	// first one again since the last time was for default, while this is
	// for the specific target:
	for (i = 0; i < 8; i++) {
		if (ParseBlendRenderTarget(&desc->RenderTarget[i], &mask->RenderTarget[i], section, i)) {
			shader->blend_override = 1;
			desc->IndependentBlendEnable = true;
			mask->IndependentBlendEnable = 0;
		}
	}

	desc->AlphaToCoverageEnable = GetIniBool(section, L"alpha_to_coverage", false, &found);
	if (found) {
		shader->blend_override = 1;
		mask->AlphaToCoverageEnable = 0;
	}

	for (i = 0; i < 4; i++) {
		swprintf_s(key, ARRAYSIZE(key), L"blend_factor[%i]", i);
		shader->blend_factor[i] = GetIniFloat(section, key, 0.0f, &found);
		if (found) {
			shader->blend_override = 1;
			shader->blend_factor_merge_mask[i] = 0;
		}
	}

	if (GetIniString(section, L"sample_mask", 0, setting, MAX_PATH)) {
		shader->blend_override = 1;
		swscanf_s(setting, L"%x", &shader->blend_sample_mask);
		LogInfo("  sample_mask=0x%x\n", shader->blend_sample_mask);
		shader->blend_sample_mask_merge_mask = 0;
	}

	if (GetIniBool(section, L"blend_state_merge", false, NULL))
		shader->blend_override = 2;
}

// https://msdn.microsoft.com/en-us/library/windows/desktop/ff476113(v=vs.85).aspx
static wchar_t *DepthWriteMasks[] = {
	L"ZERO",
	L"ALL",
};


// https://msdn.microsoft.com/en-us/library/windows/desktop/ff476101(v=vs.85).aspx
static wchar_t *ComparisonFuncs[] = {
	L"",
	L"NEVER",
	L"LESS",
	L"EQUAL",
	L"LESS_EQUAL",
	L"GREATER",
	L"NOT_EQUAL",
	L"GREATER_EQUAL",
	L"ALWAYS",
};

// https://msdn.microsoft.com/en-us/library/windows/desktop/ff476219(v=vs.85).aspx
static wchar_t *StencilOps[] = {
	L"",
	L"KEEP",
	L"ZERO",
	L"REPLACE",
	L"INCR_SAT",
	L"DECR_SAT",
	L"INVERT",
	L"INCR",
	L"DECR",
};

static void ParseStencilOp(wchar_t *key, wchar_t *val, D3D11_DEPTH_STENCILOP_DESC *desc)
{
	wchar_t func_buf[32], both_pass_buf[32], depth_fail_buf[32], stencil_fail_buf[32];
	int i;

	i = swscanf_s(val, L"%s %s %s %s",
			func_buf, (unsigned)ARRAYSIZE(func_buf),
			both_pass_buf, (unsigned)ARRAYSIZE(both_pass_buf),
			depth_fail_buf, (unsigned)ARRAYSIZE(depth_fail_buf),
			stencil_fail_buf, (unsigned)ARRAYSIZE(stencil_fail_buf));
	if (i != 4) {
		LogInfo("  WARNING: Unrecognised %S=%S\n", key, val);
		BeepFailure2();
		return;
	}
	LogInfo("  %S=%S\n", key, val);

	try {
		desc->StencilFunc = (D3D11_COMPARISON_FUNC)ParseEnum(func_buf, L"D3D11_COMPARISON_", ComparisonFuncs, ARRAYSIZE(ComparisonFuncs), 1);
	} catch (EnumParseError) {
		LogInfo("  WARNING: Unrecognised stencil function %S\n", func_buf);
		BeepFailure2();
	}

	try {
		desc->StencilPassOp = (D3D11_STENCIL_OP)ParseEnum(both_pass_buf, L"D3D11_STENCIL_OP_", StencilOps, ARRAYSIZE(StencilOps), 1);
	} catch (EnumParseError) {
		LogInfo("  WARNING: Unrecognised stencil + depth pass operation %S\n", both_pass_buf);
		BeepFailure2();
	}

	try {
		desc->StencilDepthFailOp = (D3D11_STENCIL_OP)ParseEnum(depth_fail_buf, L"D3D11_STENCIL_OP_", StencilOps, ARRAYSIZE(StencilOps), 1);
	} catch (EnumParseError) {
		LogInfo("  WARNING: Unrecognised stencil pass / depth fail operation %S\n", depth_fail_buf);
		BeepFailure2();
	}

	try {
		desc->StencilFailOp = (D3D11_STENCIL_OP)ParseEnum(stencil_fail_buf, L"D3D11_STENCIL_OP_", StencilOps, ARRAYSIZE(StencilOps), 1);
	} catch (EnumParseError) {
		LogInfo("  WARNING: Unrecognised stencil fail operation %S\n", stencil_fail_buf);
		BeepFailure2();
	}
}

static void ParseDepthStencilState(CustomShader *shader, const wchar_t *section)
{
	D3D11_DEPTH_STENCIL_DESC *desc = &shader->depth_stencil_desc;
	D3D11_DEPTH_STENCIL_DESC *mask = &shader->depth_stencil_mask;
	wchar_t setting[MAX_PATH];
	wchar_t key[32];
	int ival;
	bool found;

	memset(desc, 0, sizeof(D3D11_DEPTH_STENCIL_DESC));
	memset(mask, 0xff, sizeof(D3D11_DEPTH_STENCIL_DESC));

	// Set a default stencil state for any missing values:
	desc->StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
	desc->StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;
	desc->FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
	desc->FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	desc->FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	desc->BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
	desc->BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
	desc->BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;

	desc->DepthEnable = GetIniBool(section, L"depth_enable", true, &found);
	if (found) {
		shader->depth_stencil_override = 1;
		mask->DepthEnable = 0;
	}

	desc->DepthWriteMask = (D3D11_DEPTH_WRITE_MASK)GetIniEnum(section, L"depth_write_mask", D3D11_DEPTH_WRITE_MASK_ALL, &found,
			L"D3D11_DEPTH_WRITE_MASK_", DepthWriteMasks, ARRAYSIZE(DepthWriteMasks), 0);
	if (found) {
		shader->depth_stencil_override = 1;
		mask->DepthWriteMask = (D3D11_DEPTH_WRITE_MASK)0;
	}

	desc->DepthFunc = (D3D11_COMPARISON_FUNC)GetIniEnum(section, L"depth_func", D3D11_COMPARISON_LESS, &found,
			L"D3D11_COMPARISON_", ComparisonFuncs, ARRAYSIZE(ComparisonFuncs), 1);
	if (found) {
		shader->depth_stencil_override = 1;
		mask->DepthFunc = (D3D11_COMPARISON_FUNC)0;
	}

	desc->StencilEnable = GetIniBool(section, L"stencil_enable", false, &found);
	if (found) {
		shader->depth_stencil_override = 1;
		mask->StencilEnable = 0;
	}

	if (GetIniString(section, L"stencil_read_mask", 0, setting, MAX_PATH)) {
		shader->depth_stencil_override = 1;
		swscanf_s(setting, L"%x", &ival); // No suitable format string w/o overflow?
		desc->StencilReadMask = ival; // Use an intermediate to be safe
		mask->StencilReadMask = 0;
		LogInfo("  stencil_read_mask=0x%x\n", desc->StencilReadMask);
	}

	if (GetIniString(section, L"stencil_write_mask", 0, setting, MAX_PATH)) {
		shader->depth_stencil_override = 1;
		swscanf_s(setting, L"%x", &ival); // No suitable format string w/o overflow?
		desc->StencilWriteMask = ival; // Use an intermediate to be safe
		mask->StencilWriteMask = 0;
		LogInfo("  stencil_write_mask=0x%x\n", desc->StencilWriteMask);
	}

	if (GetIniString(section, L"stencil_front", 0, setting, MAX_PATH)) {
		shader->depth_stencil_override = 1;
		ParseStencilOp(key, setting, &desc->FrontFace);
		memset(&mask->FrontFace, 0, sizeof(D3D11_DEPTH_STENCILOP_DESC));
	}

	if (GetIniString(section, L"stencil_back", 0, setting, MAX_PATH)) {
		shader->depth_stencil_override = 1;
		ParseStencilOp(key, setting, &desc->BackFace);
		memset(&mask->BackFace, 0, sizeof(D3D11_DEPTH_STENCILOP_DESC));
	}

	shader->stencil_ref = GetIniInt(section, L"stencil_ref", 0, &found);
	if (found) {
		shader->depth_stencil_override = 1;
		shader->stencil_ref_mask = 0;
	}

	if (GetIniBool(section, L"depth_stencil_state_merge", false, NULL))
		shader->depth_stencil_override = 2;
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

static void ParseRSState(CustomShader *shader, const wchar_t *section)
{
	D3D11_RASTERIZER_DESC *desc = &shader->rs_desc;
	D3D11_RASTERIZER_DESC *mask = &shader->rs_mask;
	bool found;

	memset(mask, 0xff, sizeof(D3D11_RASTERIZER_DESC));

	desc->FillMode = (D3D11_FILL_MODE)GetIniEnum(section, L"fill", D3D11_FILL_SOLID, &found,
			L"D3D11_FILL_", FillModes, ARRAYSIZE(FillModes), 2);
	if (found) {
		shader->rs_override = 1;
		mask->FillMode = (D3D11_FILL_MODE)0;
	}

	desc->CullMode = (D3D11_CULL_MODE)GetIniEnum(section, L"cull", D3D11_CULL_BACK, &found,
			L"D3D11_CULL_", CullModes, ARRAYSIZE(CullModes), 1);
	if (found) {
		shader->rs_override = 1;
		mask->CullMode = (D3D11_CULL_MODE)0;
	}

	desc->FrontCounterClockwise = (BOOL)GetIniEnum(section, L"front", 0, &found,
			NULL, FrontDirection, ARRAYSIZE(FrontDirection), 0);
	if (found) {
		shader->rs_override = 1;
		mask->FrontCounterClockwise = 0;
	}

	desc->DepthBias = GetIniInt(section, L"depth_bias", 0, &found);
	if (found) {
		shader->rs_override = 1;
		mask->DepthBias = 0;
	}

	desc->DepthBiasClamp = GetIniFloat(section, L"depth_bias_clamp", 0, &found);
	if (found) {
		shader->rs_override = 1;
		mask->DepthBiasClamp = 0;
	}

	desc->SlopeScaledDepthBias = GetIniFloat(section, L"slope_scaled_depth_bias", 0, &found);
	if (found) {
		shader->rs_override = 1;
		mask->SlopeScaledDepthBias = 0;
	}

	desc->DepthClipEnable = GetIniBool(section, L"depth_clip_enable", true, &found);
	if (found) {
		shader->rs_override = 1;
		mask->DepthClipEnable = 0;
	}

	desc->ScissorEnable = GetIniBool(section, L"scissor_enable", false, &found);
	if (found) {
		shader->rs_override = 1;
		mask->ScissorEnable = 0;
	}

	desc->MultisampleEnable = GetIniBool(section, L"multisample_enable", false, &found);
	if (found) {
		shader->rs_override = 1;
		mask->MultisampleEnable = 0;
	}

	desc->AntialiasedLineEnable = GetIniBool(section, L"antialiased_line_enable", false, &found);
	if (found) {
		shader->rs_override = 1;
		mask->AntialiasedLineEnable = 0;
	}

	if (GetIniBool(section, L"rasterizer_state_merge", false, NULL))
		shader->rs_override = 2;
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

static void ParseTopology(CustomShader *shader, const wchar_t *section)
{
	wchar_t *prefix = L"D3D11_PRIMITIVE_TOPOLOGY_";
	size_t prefix_len;
	wchar_t val[MAX_PATH];
	wchar_t *ptr;
	int i;

	shader->topology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;

	if (!GetIniString(section, L"topology", 0, val, MAX_PATH))
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

static void ParseSamplerState(CustomShader *shader, const wchar_t *section)
{
	D3D11_SAMPLER_DESC* desc = &shader->sampler_desc;
	wchar_t setting[MAX_PATH];

	memset(desc, 0, sizeof(D3D11_SAMPLER_DESC));

	//TODO: do not really understand the difference between normal and comparison filter 
	// and how they are depending on the comparison func. 
	// just used one ==> need further reconsideration
	desc->Filter = D3D11_FILTER_COMPARISON_ANISOTROPIC;
	desc->AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
	desc->AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
	desc->AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
	desc->MipLODBias = 0.0f;
	desc->MaxAnisotropy = 1;
	desc->ComparisonFunc = D3D11_COMPARISON_ALWAYS;
	desc->BorderColor[0] = 0;
	desc->BorderColor[1] = 0;
	desc->BorderColor[2] = 0;
	desc->BorderColor[3] = 0;
	desc->MinLOD = 0;
	desc->MaxLOD = 1;

	if (GetIniString(section, L"sampler", 0, setting, MAX_PATH))
	{
		if (!_wcsicmp(setting, L"null"))
			return;

		if (!_wcsicmp(setting, L"point_filter"))
		{
			desc->Filter = D3D11_FILTER_COMPARISON_MIN_LINEAR_MAG_MIP_POINT;
			shader->sampler_override = 1;
			LogInfo("  sampler=point_filter\n");
			return;
		}

		if (!_wcsicmp(setting, L"linear_filter"))
		{
			desc->Filter = D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
			shader->sampler_override = 1;
			LogInfo("  sampler=linear_filter\n");
			return;
		}

		if (!_wcsicmp(setting, L"anisotropic_filter"))
		{
			desc->Filter = D3D11_FILTER_COMPARISON_ANISOTROPIC;
			desc->MaxAnisotropy = 16; // TODO: is 16 necessary or maybe it should be provided by the config ini?
			shader->sampler_override = 1;
			LogInfo("  sampler=anisotropic_filter\n");
			return;
		}

		LogInfo("  WARNING: Unknown sampler \"%S\"\n", setting);
		BeepFailure2();
	}
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
	L"blend_state_merge",
	// OM Depth Stencil State overrides:
	L"depth_enable", L"depth_write_mask", L"depth_func",
	L"stencil_enable", L"stencil_read_mask", L"stencil_write_mask",
	L"stencil_front", L"stencil_back", L"stencil_ref",
	L"depth_stencil_state_merge",
	// RS State overrides:
	L"fill", L"cull", L"front", L"depth_bias", L"depth_bias_clamp",
	L"slope_scaled_depth_bias", L"depth_clip_enable", L"scissor_enable",
	L"multisample_enable", L"antialiased_line_enable",
	L"rasterizer_state_merge",
	// IA State overrides:
	L"topology",
	// Sampler State overrides
	L"sampler", // TODO: add additional sampler parameter 
				// For now due to the lack of sampler as a custom resource only filtering is added no further parameter are implemented
	NULL
};
static void EnumerateCustomShaderSections()
{
	IniSections::iterator lower, upper, i;
	wstring shader_id;

	customShaders.clear();

	lower = ini_sections.lower_bound(wstring(L"CustomShader"));
	upper = prefix_upper_bound(ini_sections, wstring(L"CustomShader"));
	for (i = lower; i != upper; i++) {
		// Convert section name to lower case so our keys will be
		// consistent in the unordered_map:
		shader_id = i->first;
		std::transform(shader_id.begin(), shader_id.end(), shader_id.begin(), ::towlower);

		// Construct a custom shader in the global list:
		customShaders[shader_id];
	}
}
static void ParseCustomShaderSections()
{
	CustomShaders::iterator i;
	const wstring *shader_id;
	CustomShader *custom_shader;
	wchar_t setting[MAX_PATH];
	bool failed;

	for (i = customShaders.begin(); i != customShaders.end();) {
		shader_id = &i->first;
		custom_shader = &i->second;

		// FIXME: This will be logged in lower case. It would be better
		// to use the original case, but not a big deal:
		LogInfoW(L"[%s]\n", shader_id->c_str());

		failed = false;

		if (GetIniString(shader_id->c_str(), L"vs", 0, setting, MAX_PATH))
			failed |= custom_shader->compile('v', setting, shader_id);
		if (GetIniString(shader_id->c_str(), L"hs", 0, setting, MAX_PATH))
			failed |= custom_shader->compile('h', setting, shader_id);
		if (GetIniString(shader_id->c_str(), L"ds", 0, setting, MAX_PATH))
			failed |= custom_shader->compile('d', setting, shader_id);
		if (GetIniString(shader_id->c_str(), L"gs", 0, setting, MAX_PATH))
			failed |= custom_shader->compile('g', setting, shader_id);
		if (GetIniString(shader_id->c_str(), L"ps", 0, setting, MAX_PATH))
			failed |= custom_shader->compile('p', setting, shader_id);
		if (GetIniString(shader_id->c_str(), L"cs", 0, setting, MAX_PATH))
			failed |= custom_shader->compile('c', setting, shader_id);


		ParseBlendState(custom_shader, shader_id->c_str());
		ParseDepthStencilState(custom_shader, shader_id->c_str());
		ParseRSState(custom_shader, shader_id->c_str());
		ParseTopology(custom_shader, shader_id->c_str());
		ParseSamplerState(custom_shader, shader_id->c_str());

		custom_shader->max_executions_per_frame =
			GetIniInt(shader_id->c_str(), L"max_executions_per_frame", 0, NULL);

		if (failed) {
			// Don't want to allow a shader to be run if it had an
			// error since we are likely to call Draw or Dispatch
			i = customShaders.erase(i);
			continue;
		} else
			i++;

		ParseCommandList(shader_id->c_str(), &custom_shader->command_list, &custom_shader->post_command_list, CustomShaderIniKeys);
	}
}

// "Explicit" means that this parses command lists sections that are
// *explicitly* called [CommandList*], as opposed to other sections that are
// implicitly command lists (such as ShaderOverride, Present, etc).
static void EnumerateExplicitCommandListSections()
{
	IniSections::iterator lower, upper, i;
	wstring section_id;

	explicitCommandListSections.clear();

	lower = ini_sections.lower_bound(wstring(L"CommandList"));
	upper = prefix_upper_bound(ini_sections, wstring(L"CommandList"));
	for (i = lower; i != upper; i++) {
		// Convert section name to lower case so our keys will be
		// consistent in the unordered_map:
		section_id = i->first;
		std::transform(section_id.begin(), section_id.end(), section_id.begin(), ::towlower);

		// Construct an explicit command list section in the global list:
		explicitCommandListSections[section_id];
	}
}

static void ParseExplicitCommandListSections()
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
		ParseCommandList(section_id->c_str(), &command_list_section->command_list, &command_list_section->post_command_list, NULL);
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

	ParseIni(iniFile);

	G->gLogInput = GetIniBool(L"Logging", L"input", false, NULL);
	gLogDebug = GetIniBool(L"Logging", L"debug", false, NULL);

	// Unbuffered logging to remove need for fflush calls, and r/w access to make it easy
	// to open active files.
	if (LogFile && GetIniBool(L"Logging", L"unbuffered", false, NULL))
	{
		int unbuffered = setvbuf(LogFile, NULL, _IONBF, 0);
		LogInfo("    unbuffered return: %d\n", unbuffered);
	}

	// Set the CPU affinity based upon d3dx.ini setting.  Useful for debugging and shader hunting in AC3.
	if (GetIniBool(L"Logging", L"force_cpu_affinity", false, NULL))
	{
		DWORD one = 0x01;
		BOOL affinity = SetProcessAffinityMask(GetCurrentProcess(), one);
		LogInfo("    force_cpu_affinity return: %s\n", affinity ? "true" : "false");
	}

	// If specified in Logging section, wait for Attach to Debugger.
	int debugger = GetIniInt(L"Logging", L"waitfordebugger", 0, NULL);
	if (debugger > 0)
	{
		do
		{
			Sleep(250);
		} while (!IsDebuggerPresent());
		if (debugger > 1)
			__debugbreak();
	}

	G->dump_all_profiles = GetIniBool(L"Logging", L"dump_all_profiles", false, NULL);

	// [System]
	LogInfo("[System]\n");
	GetIniString(L"System", L"proxy_d3d11", 0, G->CHAIN_DLL_PATH, MAX_PATH);
	if (G->CHAIN_DLL_PATH[0])
		LogInfoW(L"  proxy_d3d11=%s\n", G->CHAIN_DLL_PATH);
	if (GetIniString(L"System", L"hook", 0, setting, MAX_PATH))
	{
		LogInfoW(L"  hook=%s\n", setting);
		G->enable_hooks = parse_enum_option_string<wchar_t *, EnableHooks>
			(EnableHooksNames, setting, NULL);
	}
	G->enable_dxgi1_2 = GetIniInt(L"System", L"allow_dxgi1_2", 0, NULL);
	G->enable_check_interface = GetIniBool(L"System", L"allow_check_interface", false, NULL);
	G->enable_create_device = GetIniInt(L"System", L"allow_create_device", 0, NULL);
	G->enable_platform_update = GetIniBool(L"System", L"allow_platform_update", false, NULL);

	// [Device] (DXGI parameters)
	LogInfo("[Device]\n");
	G->SCREEN_WIDTH = GetIniInt(L"Device", L"width", -1, NULL);
	G->SCREEN_HEIGHT = GetIniInt(L"Device", L"height", -1, NULL);
	G->SCREEN_REFRESH = GetIniInt(L"Device", L"refresh_rate", -1, NULL);
	G->SCREEN_UPSCALING = GetIniInt(L"Device", L"upscaling", 0, NULL);
	G->UPSCALE_MODE = GetIniInt(L"Device", L"upscale_mode", 0, NULL);

	if (GetIniString(L"Device", L"filter_refresh_rate", 0, setting, MAX_PATH))
	{
		swscanf_s(setting, L"%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
			G->FILTER_REFRESH + 0, G->FILTER_REFRESH + 1, G->FILTER_REFRESH + 2, G->FILTER_REFRESH + 3, 
			G->FILTER_REFRESH + 4, G->FILTER_REFRESH + 5, G->FILTER_REFRESH + 6, G->FILTER_REFRESH + 7, 
			G->FILTER_REFRESH + 8, G->FILTER_REFRESH + 9);
		LogInfoW(L"  filter_refresh_rate=%s\n", setting);
	}

	G->SCREEN_FULLSCREEN = GetIniInt(L"Device", L"full_screen", -1, NULL);
	RegisterIniKeyBinding(L"Device", L"toggle_full_screen", ToggleFullScreen, NULL, 0, NULL);
	G->gForceStereo = GetIniInt(L"Device", L"force_stereo", 0, NULL);
	G->SCREEN_ALLOW_COMMANDS = GetIniBool(L"Device", L"allow_windowcommands", false, NULL);

	if (GetIniString(L"Device", L"get_resolution_from", 0, setting, MAX_PATH)) {
		G->mResolutionInfo.from = lookup_enum_val<wchar_t *, GetResolutionFrom>
			(GetResolutionFromNames, setting, GetResolutionFrom::INVALID);
		if (G->mResolutionInfo.from == GetResolutionFrom::INVALID) {
			LogInfoW(L"  WARNING: Unknown get_resolution_from %s\n", setting);
			BeepFailure2();
		} else
			LogInfoW(L"  get_resolution_from=%s\n", setting);
	} else
		G->mResolutionInfo.from = GetResolutionFrom::INVALID;

	G->hide_cursor = GetIniBool(L"Device", L"hide_cursor", false, NULL);
	G->cursor_upscaling_bypass = GetIniBool(L"Device", L"cursor_upscaling_bypass", true, NULL);

	// [Stereo]
	LogInfo("[Stereo]\n");
	bool automaticMode = GetIniBool(L"Stereo", L"automatic_mode", false, NULL);				// in NVapi dll
	G->gCreateStereoProfile = GetIniBool(L"Stereo", L"create_profile", false, NULL);
	G->gSurfaceCreateMode = GetIniInt(L"Stereo", L"surface_createmode", -1, NULL);
	G->gSurfaceSquareCreateMode = GetIniInt(L"Stereo", L"surface_square_createmode", -1, NULL);
	G->gForceNoNvAPI = GetIniBool(L"Stereo", L"force_no_nvapi", false, NULL);

	// [Rendering]
	LogInfo("[Rendering]\n");

	G->shader_hash_type = ShaderHashType::FNV;
	if (GetIniString(L"Rendering", L"shader_hash", 0, setting, MAX_PATH)) {
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

	GetIniString(L"Rendering", L"override_directory", 0, G->SHADER_PATH, MAX_PATH);
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
	GetIniString(L"Rendering", L"cache_directory", 0, G->SHADER_CACHE_PATH, MAX_PATH);
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

	G->CACHE_SHADERS = GetIniBool(L"Rendering", L"cache_shaders", false, NULL);
	G->ENABLE_CRITICAL_SECTION = GetIniBool(L"Rendering", L"use_criticalsection", false, NULL);
	G->SCISSOR_DISABLE = GetIniBool(L"Rendering", L"rasterizer_disable_scissor", false, NULL);
	G->track_texture_updates = GetIniBool(L"Rendering", L"track_texture_updates", false, NULL);
	G->assemble_signature_comments = GetIniBool(L"Rendering", L"assemble_signature_comments", false, NULL);

	G->EXPORT_FIXED = GetIniBool(L"Rendering", L"export_fixed", false, NULL);
	G->EXPORT_SHADERS = GetIniBool(L"Rendering", L"export_shaders", false, NULL);
	G->EXPORT_HLSL = GetIniInt(L"Rendering", L"export_hlsl", 0, NULL);
	G->EXPORT_BINARY = GetIniBool(L"Rendering", L"export_binary", false, NULL);
	G->DumpUsage = GetIniBool(L"Rendering", L"dump_usage", false, NULL);

	G->StereoParamsReg = GetIniInt(L"Rendering", L"stereo_params", 125, NULL);
	G->IniParamsReg = GetIniInt(L"Rendering", L"ini_params", 120, NULL);
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
	G->FIX_SV_Position = GetIniBool(L"Rendering", L"fix_sv_position", false, NULL);
	G->FIX_Light_Position = GetIniBool(L"Rendering", L"fix_light_position", false, NULL);
	G->FIX_Recompile_VS = GetIniBool(L"Rendering", L"recompile_all_vs", false, NULL);
	if (GetIniStringAndLog(L"Rendering", L"fix_ZRepair_DepthTexture1", 0, setting, MAX_PATH))
	{
		char buf[MAX_PATH];
		wcstombs(buf, setting, MAX_PATH);
		char *end = RightStripA(buf);
		G->ZRepair_DepthTextureReg1 = *end; *(end - 1) = 0;
		char *start = buf; while (isspace(*start)) start++;
		G->ZRepair_DepthTexture1 = start;
	}
	if (GetIniStringAndLog(L"Rendering", L"fix_ZRepair_DepthTexture2", 0, setting, MAX_PATH))
	{
		char buf[MAX_PATH];
		wcstombs(buf, setting, MAX_PATH);
		char *end = RightStripA(buf);
		G->ZRepair_DepthTextureReg2 = *end; *(end - 1) = 0;
		char *start = buf; while (isspace(*start)) start++;
		G->ZRepair_DepthTexture2 = start;
	}
	if (GetIniStringAndLog(L"Rendering", L"fix_ZRepair_ZPosCalc1", 0, setting, MAX_PATH))
		G->ZRepair_ZPosCalc1 = readStringParameter(setting);
	if (GetIniStringAndLog(L"Rendering", L"fix_ZRepair_ZPosCalc2", 0, setting, MAX_PATH))
		G->ZRepair_ZPosCalc2 = readStringParameter(setting);
	if (GetIniStringAndLog(L"Rendering", L"fix_ZRepair_PositionTexture", 0, setting, MAX_PATH))
		G->ZRepair_PositionTexture = readStringParameter(setting);
	if (GetIniStringAndLog(L"Rendering", L"fix_ZRepair_PositionCalc", 0, setting, MAX_PATH))
		G->ZRepair_WorldPosCalc = readStringParameter(setting);
	if (GetIniStringAndLog(L"Rendering", L"fix_ZRepair_Dependencies1", 0, setting, MAX_PATH))
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
	if (GetIniStringAndLog(L"Rendering", L"fix_ZRepair_Dependencies2", 0, setting, MAX_PATH))
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
	if (GetIniStringAndLog(L"Rendering", L"fix_InvTransform", 0, setting, MAX_PATH))
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
	if (GetIniStringAndLog(L"Rendering", L"fix_ZRepair_DepthTextureHash", 0, setting, MAX_PATH))
	{
		uint32_t hash;
		swscanf_s(setting, L"%08lx", &hash);
		G->ZBufferHashToInject = hash;
	}
	if (GetIniStringAndLog(L"Rendering", L"fix_BackProjectionTransform1", 0, setting, MAX_PATH))
		G->BackProject_Vector1 = readStringParameter(setting);
	if (GetIniStringAndLog(L"Rendering", L"fix_BackProjectionTransform2", 0, setting, MAX_PATH))
		G->BackProject_Vector2 = readStringParameter(setting);
	if (GetIniStringAndLog(L"Rendering", L"fix_ObjectPosition1", 0, setting, MAX_PATH))
		G->ObjectPos_ID1 = readStringParameter(setting);
	if (GetIniStringAndLog(L"Rendering", L"fix_ObjectPosition2", 0, setting, MAX_PATH))
		G->ObjectPos_ID2 = readStringParameter(setting);
	if (GetIniStringAndLog(L"Rendering", L"fix_ObjectPosition1Multiplier", 0, setting, MAX_PATH))
		G->ObjectPos_MUL1 = readStringParameter(setting);
	if (GetIniStringAndLog(L"Rendering", L"fix_ObjectPosition2Multiplier", 0, setting, MAX_PATH))
		G->ObjectPos_MUL2 = readStringParameter(setting);
	if (GetIniStringAndLog(L"Rendering", L"fix_MatrixOperand1", 0, setting, MAX_PATH))
		G->MatrixPos_ID1 = readStringParameter(setting);
	if (GetIniStringAndLog(L"Rendering", L"fix_MatrixOperand1Multiplier", 0, setting, MAX_PATH))
		G->MatrixPos_MUL1 = readStringParameter(setting);


	// [Hunting]
	LogInfo("[Hunting]\n");
	G->hunting = GetIniInt(L"Hunting", L"hunting", 0, NULL);

	G->marking_mode = MARKING_MODE_SKIP;
	if (GetIniString(L"Hunting", L"marking_mode", 0, setting, MAX_PATH)) {
		if (!_wcsicmp(setting, L"skip")) G->marking_mode = MARKING_MODE_SKIP;
		if (!_wcsicmp(setting, L"mono")) G->marking_mode = MARKING_MODE_MONO;
		if (!_wcsicmp(setting, L"original")) G->marking_mode = MARKING_MODE_ORIGINAL;
		if (!_wcsicmp(setting, L"zero")) G->marking_mode = MARKING_MODE_ZERO;
		if (!_wcsicmp(setting, L"pink")) G->marking_mode = MARKING_MODE_PINK;
		LogInfoW(L"  marking_mode=%d\n", G->marking_mode);
	}

	G->mark_snapshot = GetIniInt(L"Hunting", L"mark_snapshot", 0, NULL);

	RegisterHuntingKeyBindings();
	RegisterPresetKeyBindings();

	ParsePresetOverrideSections();
	ParseResourceSections();

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
	EnumerateCustomShaderSections();
	EnumerateExplicitCommandListSections();

	ParseCustomShaderSections();
	ParseExplicitCommandListSections();

	ParseShaderOverrideSections();
	ParseTextureOverrideSections();

	LogInfo("[Present]\n");
	G->present_command_list.clear();
	G->post_present_command_list.clear();
	ParseCommandList(L"Present", &G->present_command_list, &G->post_present_command_list, NULL);

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
		if (GetIniString(L"Constants", buf, L"FLT_MAX", setting, MAX_PATH))
		{
			if (wcscmp(setting, L"FLT_MAX") != 0) {
				G->iniParams[i].x = stof(setting);
				LogInfoW(L"  %ls=%#.2g\n", buf, G->iniParams[i].x);
			}
		}
		StringCchPrintf(buf, 8, L"y%.0i", i);
		if (GetIniString(L"Constants", buf, L"FLT_MAX", setting, MAX_PATH))
		{
			if (wcscmp(setting, L"FLT_MAX") != 0) {
				G->iniParams[i].y = stof(setting);
				LogInfoW(L"  %ls=%#.2g\n", buf, G->iniParams[i].y);
			}
		}
		StringCchPrintf(buf, 8, L"z%.0i", i);
		if (GetIniString(L"Constants", buf, L"FLT_MAX", setting, MAX_PATH))
		{
			if (wcscmp(setting, L"FLT_MAX") != 0) {
				G->iniParams[i].z = stof(setting);
				LogInfoW(L"  %ls=%#.2g\n", buf, G->iniParams[i].z);
			}
		}
		StringCchPrintf(buf, 8, L"w%.0i", i);
		if (GetIniString(L"Constants", buf, L"FLT_MAX", setting, MAX_PATH))
		{
			if (wcscmp(setting, L"FLT_MAX") != 0) {
				G->iniParams[i].w = stof(setting);
				LogInfoW(L"  %ls=%#.2g\n", buf, G->iniParams[i].w);
			}
		}
	}

	LogInfo("[Profile]\n");
	ParseDriverProfile();

	LogInfo("\n");

	if (G->hide_cursor || G->SCREEN_UPSCALING)
		InstallMouseHooks(G->hide_cursor);
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

	ParseIni(iniFile);

	gLogDebug = GetIniBool(L"Logging", L"debug", false, NULL);

	// Unbuffered logging to remove need for fflush calls, and r/w access to make it easy
	// to open active files.
	if (LogFile && GetIniBool(L"Logging", L"unbuffered", false, NULL))
	{
		int unbuffered = setvbuf(LogFile, NULL, _IONBF, 0);
		LogInfo("    unbuffered return: %d\n", unbuffered);
	}

	LogInfo("[Profile]\n");
	ParseDriverProfile();

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
