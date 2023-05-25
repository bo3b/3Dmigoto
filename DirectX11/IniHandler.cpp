#include "IniHandler.h"

#include <algorithm>
#include <iterator>
#include <string>
#include <strsafe.h>
#include <fstream>
#include <sstream>
#include <memory>
#include <pcre2.h>
#include <codecvt>

#include "log.h"
#include "Globals.h"
#include "Override.h"
#include "Hunting.h"
#include "nvprofile.h"
#include "ShaderRegex.h"
#include "cursor.h"

#define INI_FILENAME L"d3dx.ini"

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
	{L"ShaderRegex", true},
	{L"TextureOverride", true},
	{L"CustomShader", true},
	{L"CommandList", true},
	{L"BuiltInCustomShader", true},
	{L"BuiltInCommandList", true},
	{L"Present", false},
	{L"ClearRenderTargetView", false},
	{L"ClearDepthStencilView", false},
	{L"ClearUnorderedAccessViewUint", false},
	{L"ClearUnorderedAccessViewFloat", false},
	{L"Constants", false},
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
	{L"Profile", false},
	{L"ConvergenceMap", false}, // Only used in nvapi wrapper
	{L"Resource", true},
	{L"Key", true},
	{L"Preset", true},
	{L"Include", true}, // Prefix so that it may be namespaced to allow included files to include more files with relative paths
	{L"Loader", false},
};

// List of sections that will not trigger a warning if they contain a line
// without an equals sign. All command lists are also permitted this privilege
// to allow for cleaner flow control syntax (if/else/endif)
static Section AllowLinesWithoutEquals[] = {
	{L"Profile", false},
	{L"ShaderRegex", true},
};

static bool whitelisted_duplicate_key(const wchar_t *section, const wchar_t *key)
{
	// FIXME: Make this declarative
	if (!_wcsnicmp(section, L"key", 3)) {
		if (!_wcsicmp(key, L"key") || !_wcsicmp(key, L"back"))
			return true;
	}

	if (!_wcsicmp(section, L"include"))
		return true;

	return false;
}

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
	return SectionInList(section, AllowLinesWithoutEquals, ARRAYSIZE(AllowLinesWithoutEquals))
		|| IsCommandListSection(section);
}

static const wchar_t* SectionPrefixFromList(const wchar_t *section, Section section_list[], int list_size)
{
	size_t len;
	int i;

	for (i = 0; i < list_size; i++) {
		if (section_list[i].prefix) {
			len = wcslen(section_list[i].section);
			if (!_wcsnicmp(section, section_list[i].section, len))
				return section_list[i].section;
		}
	}

	return false;
}

static const wchar_t* SectionPrefix(const wchar_t *section)
{
	const wchar_t *ret;

	ret = SectionPrefixFromList(section, CommandListSections, ARRAYSIZE(CommandListSections));
	if (!ret)
		ret = SectionPrefixFromList(section, RegularSections, ARRAYSIZE(RegularSections));
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

// Unsorted maps for fast case insensitive key lookups by name
typedef std::unordered_map<wstring, wstring, WStringInsensitiveHash, WStringInsensitiveEquality> IniSectionMap;
typedef std::unordered_set<wstring, WStringInsensitiveHash, WStringInsensitiveEquality> IniSectionSet;

struct IniSection {
	IniSectionMap kv_map;
	IniSectionVector kv_vec;

	// Stores the ini namespace/path that this section came from. ini_path
	// is only set if a shader overrides it's namespace so we can still
	// find shaders & resources loaded from disk next to the ini. Note that
	// there is also an ini_namespace in the IniLine structure for global
	// sections where the namespacing can be per-line:
	wstring ini_namespace;
	wstring ini_path;
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

// We now emit a single warning tone after the config file is [re]loaded to get
// the shaderhackers attention if something needs to be addressed, since their
// eyes may be focussed elsewhere and may miss the notification message[s].
static bool ini_warned = false;
#define IniWarning(fmt, ...) do { \
	ini_warned = true; \
	LogOverlay(LOG_WARNING, fmt, __VA_ARGS__); \
} while (0)
#define IniWarningW(fmt, ...) do { \
	ini_warned = true; \
	LogOverlayW(LOG_WARNING, fmt, __VA_ARGS__); \
} while (0)
#define IniWarningBeep() do { \
	ini_warned = true; \
} while (0)

static void emit_ini_warning_tone()
{
	if (!ini_warned)
		return;
	ini_warned = false;
	BeepFailure();
}

static bool get_namespaced_section_name(const wstring *section, const wstring *ini_namespace, wstring *ret)
{
	const wchar_t *section_prefix = SectionPrefix(section->c_str());
	if (!section_prefix)
		return false;

	*ret = wstring(section_prefix) + wstring(L"\\") + *ini_namespace +
		wstring(L"\\") + section->substr(wcslen(section_prefix));
	return true;
}

bool get_namespaced_section_name_lower(const wstring *section, const wstring *ini_namespace, wstring *ret)
{
	bool rc;

	rc = get_namespaced_section_name(section, ini_namespace, ret);
	if (rc)
		std::transform(ret->begin(), ret->end(), ret->begin(), ::towlower);
	return rc;
}

wstring get_namespaced_var_name_lower(const wstring var, const wstring *ini_namespace)
{
	wstring ret = wstring(L"$\\") + *ini_namespace + wstring(L"\\") + var.substr(1);
	std::transform(ret.begin(), ret.end(), ret.begin(), ::towlower);
	return ret;
}

static bool _get_section_namespace(IniSections *custom_ini_sections, const wchar_t *section, wstring *ret)
{
	try {
		*ret = custom_ini_sections->at(wstring(section)).ini_namespace;
	} catch (std::out_of_range) {
		return false;
	}
	return (!ret->empty());
}

bool get_section_namespace(const wchar_t *section, wstring *ret)
{
	return _get_section_namespace(&ini_sections, section, ret);
}

static bool _get_section_path(IniSections *custom_ini_sections, const wchar_t *section, wstring *ret)
{
	IniSection *entry;

	try {
		entry = &custom_ini_sections->at(wstring(section));
	} catch (std::out_of_range) {
		return false;
	}

	if (entry->ini_path.empty())
		*ret = entry->ini_namespace;
	else
		*ret = entry->ini_path;

	return (!ret->empty());
}

static size_t get_section_namespace_endpos(const wchar_t *section)
{
	const wchar_t *section_prefix;
	wstring ini_namespace;

	section_prefix = SectionPrefix(section);
	if (!section_prefix)
		return 0;

	if (!get_section_namespace(section, &ini_namespace))
		return wcslen(section_prefix);

	return wcslen(section_prefix) + ini_namespace.length() + 2;
}

static bool _get_namespaced_section_path(IniSections *custom_ini_sections, const wchar_t *section, wstring *ret)
{
	wstring::size_type pos;

	if (!_get_section_path(custom_ini_sections, section, ret))
		return false;

	// Strip the ini name from the end of the namespace leaving the relative path:
	pos = ret->rfind(L"\\");
	if (pos != ret->npos)
		ret->resize(pos + 1);
	else
		*ret = L"";
	return true;
}

static bool get_namespaced_section_path(const wchar_t *section, wstring *ret)
{
	return _get_namespaced_section_path(&ini_sections, section, ret);
}

static void ParseIniSectionLine(wstring *wline, wstring *section,
		int *warn_duplicates, bool *warn_lines_without_equals,
		IniSectionVector **section_vector, const wstring *ini_namespace,
		const wstring *ini_path)
{
	bool allow_duplicate_sections = false;
	size_t first, last;
	bool inserted;
	bool namespaced_section = false;

	*warn_duplicates = 1;
	*warn_lines_without_equals = true;

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

	// Config files aside from the main one are namespaced to reduce the
	// potential of mod conflicts. Only sections that have prefixes can be
	// namespaced, since global sections are always global, but we do allow
	// these config files to append/override values in global sections:
	if (!ini_namespace->empty()) {
		if (get_namespaced_section_name(section, ini_namespace, section)) {
			namespaced_section = true;
		} else {
			allow_duplicate_sections = true;
			*warn_duplicates = 2;
		}
	}

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
	if (!inserted && !allow_duplicate_sections) {
		IniWarning("WARNING: Duplicate section found in d3dx.ini: [%S]\n",
				section->c_str());
		section->clear();
		*section_vector = NULL;
		return;
	}

	*section_vector = &ini_sections[*section].kv_vec;

	// Record the namespace so we can use it later when looking up any
	// referenced sections. Only for namespaced sections, not global
	// sections:
	if (namespaced_section) {
		ini_sections[*section].ini_namespace = *ini_namespace;
		if (*ini_path != *ini_namespace)
			ini_sections[*section].ini_path = *ini_path;
	}

	// Sections that utilise a command list are allowed to have duplicate
	// keys, while other sections are not. The command list parser will
	// still check for duplicate keys that are not part of the command
	// list.
	if (IsCommandListSection(section->c_str())) {
		if (*warn_duplicates == 1)
			*warn_duplicates = 0;
	} else if (!IsRegularSection(section->c_str())) {
		IniWarning("WARNING: Unknown section in d3dx.ini: [%S]\n", section->c_str());
	}

	if (DoesSectionAllowLinesWithoutEquals(section->c_str()))
		*warn_lines_without_equals = false;
}

bool check_include_condition(wstring *val, const wstring *ini_namespace)
{
	CommandListExpression condition;
	wstring sbuf(*val);
	float ret;

	// Expressions are case insensitive:
	std::transform(sbuf.begin(), sbuf.end(), sbuf.begin(), ::towlower);

	if (!condition.parse(&sbuf, ini_namespace, NULL)) {
		IniWarning("WARNING: Unable to parse include condition: %S\n", val->c_str());
		return false;
	}

	if (!condition.static_evaluate(&ret, NULL)) {
		IniWarning("WARNING: Include condition could not be statically evaluated: %S\n", val->c_str());
		return false;
	}

	if (!ret)
		LogInfo("        condition = false, skipping \"%S\"\n", ini_namespace->c_str());

	return !!ret;
}

static bool ParseIniPreamble(wstring *wline, wstring *ini_namespace)
{
	size_t first, last, delim;
	wstring key, val;

	LogInfo("      %S\n", wline->c_str());

	// Key / Val pair
	delim = wline->find(L"=");
	if (delim != wline->npos) {
		// Strip whitespace around delimiter:
		last = wline->find_last_not_of(L" \t", delim - 1);
		key = wline->substr(0, last + 1);
		first = wline->find_first_not_of(L" \t", delim + 1);
		if (first != wline->npos)
			val = wline->substr(first);

		if (!_wcsicmp(key.c_str(), L"condition")) {
			return check_include_condition(&val, ini_namespace);
		}

		if (!_wcsicmp(key.c_str(), L"namespace")) {
			LogInfo("        Renaming namespace \"%S\" -> \"%S\"\n", ini_namespace->c_str(), val.c_str());
			*ini_namespace = val;
			return true;
		}
	}

	IniWarning("WARNING: d3dx.ini entry outside of section: %S\n",
			wline->c_str());
	return true;
}

static void ParseIniKeyValLine(wstring *wline, wstring *section,
		int warn_duplicates, bool warn_lines_without_equals,
		IniSectionVector *section_vector, const wstring *ini_namespace)
{
	size_t first, last, delim;
	wstring key, val;
	bool inserted;

	if (section->empty() || section_vector == NULL) {
		IniWarning("WARNING: d3dx.ini entry outside of section: %S\n",
				wline->c_str());
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

		if (warn_duplicates == 2) {
			// Recursively loaded config files are permitted to
			// override values from the main d3dx.ini:
			ini_sections.at(*section).kv_map[key] = val;
		} else {
			// We use "at" on the sections to access an existing
			// section (alternatively we could use the [] operator
			// to permit it to be created if it doesn't exist), but
			// we use emplace within the section so that only the
			// first item with a given key is inserted to match the
			// behaviour of GetPrivateProfileString for duplicate
			// keys within a single section:
			inserted = ini_sections.at(*section).kv_map.emplace(key, val).second;
			if ((warn_duplicates == 1) && !inserted && !whitelisted_duplicate_key(section->c_str(), key.c_str())) {
				IniWarning("WARNING: Duplicate key found in d3dx.ini: [%S] %S\n",
						section->c_str(), key.c_str());
			}
		}
	} else {
		// No = on line, don't store in key lookup maps to
		// match the behaviour of GetPrivateProfileString, but
		// we will store it in the section vector structure for the
		// profile parser to process.
		if (warn_lines_without_equals) {
			IniWarning("WARNING: Malformed line in d3dx.ini: [%S] \"%S\"\n",
					section->c_str(), wline->c_str());
			return;
		}
	}

	section_vector->emplace_back(key, val, *wline, *ini_namespace);
}

static void ParseIniStream(istream *stream, const wstring *_ini_namespace)
{
	string aline;
	wstring wline, section, ini_path;
	size_t first, last;
	IniSectionVector *section_vector = NULL;
	int warn_duplicates = 1;
	bool warn_lines_without_equals = true;
	wstring ini_namespace;
	bool preamble = true;

	// Simplify code further on by translating NULL to "" here:
	if (_ini_namespace)
		ini_namespace = *_ini_namespace;
	else
		ini_namespace = L"";
	ini_path = ini_namespace;

	while (std::getline(*stream, aline)) {
		// Convert to wstring for compatibility with GetPrivateProfile*
		// APIs. If we assume the d3dx.ini is always ASCII we could
		// drop this, but that would require us to change a great many
		// types throughout 3DMigoto, so leave that for another day.
		wline = wstring(aline.begin(), aline.end());

		// Strip preceding and trailing whitespace:
		first = wline.find_first_not_of(L" \t");
		last = wline.find_last_not_of(L" \t");

		if (first == wline.npos)
			continue;

		wline = wline.substr(first, last - first + 1);

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
			preamble = false;
			ParseIniSectionLine(&wline, &section, &warn_duplicates,
					    &warn_lines_without_equals,
					    &section_vector, &ini_namespace,
					    &ini_path);
			continue;
		}

		if (preamble) {
			if (!ParseIniPreamble(&wline, &ini_namespace))
				return;
			continue;
		}

		ParseIniKeyValLine(&wline, &section, warn_duplicates,
				   warn_lines_without_equals, section_vector,
				   &ini_namespace);
	}
}

static void ParseIniExcerpt(const char *excerpt)
{
	std::istringstream stream(excerpt);

	ParseIniStream(&stream, NULL);
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
static void ParseNamespacedIniFile(const wchar_t *ini, const wstring *ini_namespace)
{
	ifstream f(ini, ios::in, _SH_DENYNO);
	if (!f) {
		LogOverlay(LOG_WARNING, "  Error opening %S\n", ini);
		return;
	}

	ParseIniStream(&f, ini_namespace);
}

static void ParseIniFile(const wchar_t *ini)
{
	ini_sections.clear();

	return ParseNamespacedIniFile(ini, NULL);
}

static void InsertBuiltInIniSections()
{
	static const char text[] =
		"[BuiltInCustomShaderDisableScissorClipping]\n"
		"scissor_enable = false\n"
		"rasterizer_state_merge = true\n"
		"draw = from_caller\n"
		"handling = skip\n"

		"[BuiltInCustomShaderEnableScissorClipping]\n"
		"scissor_enable = true\n"
		"rasterizer_state_merge = true\n"
		"draw = from_caller\n"
		"handling = skip\n"

		"[BuiltInCommandListUnbindAllRenderTargets]\n"
		"o0 = null\n"
		"o1 = null\n"
		"o2 = null\n"
		"o3 = null\n"
		"o4 = null\n"
		"o5 = null\n"
		"o6 = null\n"
		"o7 = null\n"
		"oD = null\n"
	;

	ParseIniExcerpt(text);
}

static pcre2_code* glob_to_regex(wstring &pattern)
{
	PCRE2_UCHAR *converted = NULL;
	PCRE2_SIZE blength = 0;
	pcre2_code *regex = NULL;
	string apattern(pattern.begin(), pattern.end());
	PCRE2_SIZE err_off;
	int err;

	if (pcre2_pattern_convert((PCRE2_SPTR)apattern.c_str(),
				apattern.length(), PCRE2_CONVERT_GLOB,
				&converted, &blength, NULL)) {
		LogInfo("Bad pattern: exclude_recursive=%S\n", pattern.c_str());
		return NULL;
	}

	regex = pcre2_compile(converted, blength, PCRE2_CASELESS, &err, &err_off, NULL);
	if (!regex)
		LogInfo("WARNING: exclude_recursive PCRE2 regex compilation failed");

	pcre2_converted_pattern_free(converted);
	return regex;
}

static vector<pcre2_code*> globbing_vector_to_regex(vector<wstring> &globbing_patterns)
{
	vector<pcre2_code*> ret;
	pcre2_code *regex;

	for (wstring pattern : globbing_patterns) {
		regex = glob_to_regex(pattern);
		if (regex)
			ret.push_back(regex);
	}

	return ret;
}

static void free_globbing_vector(vector<pcre2_code*> &patterns) {
	for (pcre2_code *regex : patterns)
		pcre2_code_free(regex);
}

static bool matches_globbing_vector(wchar_t *filename, vector<pcre2_code*> &patterns) {
	string afilename;
	pcre2_match_data *md;
	int rc;

	// In a lot of cases we just use fake conversion to/from wstring,
	// because we assume the d3dx.ini is ASCII (at some point we should
	// eliminate all unecessary uses of wchar_t/wstring). Since this is a
	// filename, it can contain legitimate unicode characters, so we should
	// convert it properly to UTF8:
	std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> codec;
	afilename = codec.to_bytes(filename); // to_bytes = to utf8

	for (pcre2_code *regex : patterns) {
		md = pcre2_match_data_create_from_pattern(regex, NULL);
		rc = pcre2_match(regex, (PCRE2_SPTR)afilename.c_str(), PCRE2_ZERO_TERMINATED, 0, 0, md, NULL);
		pcre2_match_data_free(md);
		if (rc > 0)
			return true;
	}

	return false;
}

static void ParseIniFilesRecursive(wchar_t *migoto_path, const wstring &rel_path, vector<pcre2_code*> &exclude)
{
	std::set<wstring, WStringInsensitiveLess> ini_files, directories;
	WIN32_FIND_DATA find_data;
	HANDLE hFind;
	wstring search_path, ini_path, ini_namespace;

	search_path = wstring(migoto_path) + rel_path + L"\\*";
	LogInfo("    Searching \"%S\"\n", search_path.c_str());

	// We want to make sure the order will be consistent in case of any
	// interactions between mods, so we read the entire directory, sort it
	// in a case insensitive manner, then process the matching files &
	// directories in the same order every time

	hFind = FindFirstFile(search_path.c_str(), &find_data);
	if (hFind == INVALID_HANDLE_VALUE) {
		LogInfo("    Recursive include path \"%S\" not found\n", search_path.c_str());
		return;
	}

	do {
		if (matches_globbing_vector(find_data.cFileName, exclude)) {
			LogInfo("    Excluding \"%S\"\n", find_data.cFileName);
			continue;
		}

		if (find_data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
			if (wcscmp(find_data.cFileName, L".") && wcscmp(find_data.cFileName, L".."))
				directories.insert(wstring(find_data.cFileName));
		} else if (!wcscmp(find_data.cFileName + wcslen(find_data.cFileName) - 4, L".ini")) {
			ini_files.insert(wstring(find_data.cFileName));
		} else {
			LogDebug("    Not a directory or ini file: \"%S\"\n", find_data.cFileName);
		}
	} while (FindNextFile(hFind, &find_data));

	FindClose(hFind);

	for (wstring i: ini_files) {
		ini_namespace = rel_path + wstring(L"\\") + i;
		ini_path = wstring(migoto_path) + ini_namespace;
		LogInfo("    Processing \"%S\"\n", ini_path.c_str());
		ParseNamespacedIniFile(ini_path.c_str(), &ini_namespace);
	}

	for (wstring i: directories) {
		ini_namespace = rel_path + wstring(L"\\") + i;
		ParseIniFilesRecursive(migoto_path, ini_namespace, exclude);
	}
}

static bool IniHasKey(const wchar_t *section, const wchar_t *key)
{
	try {
		return !!ini_sections.at(section).kv_map.count(key);
	} catch (std::out_of_range) {
		return false;
	}
}

static void _GetIniSection(IniSections *custom_ini_sections, IniSectionVector **key_vals, const wchar_t *section)
{
	static IniSectionVector empty_section_vector;

	try {
		*key_vals = &custom_ini_sections->at(section).kv_vec;
	} catch (std::out_of_range) {
		LogDebug("WARNING: GetIniSection() called on a section not in the ini_sections map: %S\n", section);
		*key_vals = &empty_section_vector;
	}
}

void GetIniSection(IniSectionVector **key_vals, const wchar_t *section)
{
	return _GetIniSection(&ini_sections, key_vals, section);
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
		// Note that we now use wcsncpy_s here with _TRUNCATE rather
		// than wcscpy_s, because it turns out the later may just kill
		// us immediately on overflow depending on the invalid
		// parameter handler (refer to issue #84), and this way we more
		// closely match the behaviour of GetPrivateProfileString.
		if (wcsncpy_s(ret, size, val.c_str(), _TRUNCATE)) {
			// Funky return code of GetPrivateProfileString Not
			// sure if we depend on this - if we don't I'd like a
			// nicer return code or to raise an exception.
			IniWarning("WARNING: [%S] \"%S=%S\" too long\n",
					section, key, val.c_str());
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

// Variant of the above that fills out a std::string, and doesn't bother about
// all that size nonsense. There is no std::wstring variant of this because I
// want to refactor out all our uses of wide characters that came from the ini
// file courtesy of the old ini parsing API, and adding a new function that
// returns wide characters would be counter-productive to that goal.
bool GetIniString(const wchar_t *section, const wchar_t *key, const wchar_t *def, std::string *ret)
{
	std::wstring wret;
	bool found = false;

	if (!ret) {
		LogInfo("BUG: Misuse of GetIniString()\n");
		DoubleBeepExit();
	}

	try {
		wret = ini_sections.at(section).kv_map.at(key);
		found = true;
	} catch (std::out_of_range) {
		if (def)
			wret = def;
		else
			wret = L"";
	}

	// TODO: Get rid of all the wide character strings that the old ini
	// parsing API forced on us so we don't need this re-conversion:
	*ret = std::string(wret.begin(), wret.end());
	return found;
}

// For sections that allow the same key to be used multiple times with
// different values, fills out a vector with all values of the key
static std::vector<std::wstring> GetIniStringMultipleKeys(const wchar_t *section, const wchar_t *key)
{
	std::vector<std::wstring> ret;
	IniSectionVector *sv = NULL;
	IniSectionVector::iterator entry;

	GetIniSection(&sv, section);
	for (entry = sv->begin(); entry < sv->end(); entry++) {
		if (!_wcsicmp(key, entry->first.c_str()))
			ret.push_back(entry->second);
	}

	return ret;
}

// Helper functions to parse common types and log their values. TODO: Convert
// more of this file to use these where appropriate
int GetIniStringAndLog(const wchar_t *section, const wchar_t *key,
		const wchar_t *def, wchar_t *ret, unsigned size)
{
	int rc = GetIniString(section, key, def, ret, size);

	if (rc)
		LogInfo("  %S=%S\n", key, ret);

	return rc;
}
static bool GetIniStringAndLog(const wchar_t *section, const wchar_t *key,
		const wchar_t *def, std::string *ret)
{
	bool rc = GetIniString(section, key, def, ret);

	if (rc)
		LogInfo("  %S=%s\n", key, ret->c_str());

	return rc;
}


float GetIniFloat(const wchar_t *section, const wchar_t *key, float def, bool *found)
{
	wchar_t val[32];
	float ret = def;
	int len;

	if (found)
		*found = false;

	if (GetIniString(section, key, 0, val, 32)) {
		swscanf_s(val, L"%f%n", &ret, &len);
		if (len != wcslen(val)) {
			IniWarning("WARNING: Floating point parse error: %S=%S\n", key, val);
		} else {
			if (found)
				*found = true;
			LogInfo("  %S=%f\n", key, ret);
		}
	}

	return ret;
}

int GetIniInt(const wchar_t *section, const wchar_t *key, int def, bool *found, bool warn)
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
			if (warn)
				IniWarning("WARNING: Integer parse error: %S=%S\n", key, val);
		} else {
			if (found)
				*found = true;
			LogInfo("  %S=%d\n", key, ret);
		}
	}

	return ret;
}

bool GetIniBool(const wchar_t *section, const wchar_t *key, bool def, bool *found, bool warn)
{
	wchar_t val[32];
	bool ret = def;

	if (found)
		*found = false;

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

		if (warn)
			IniWarning("WARNING: Boolean parse error: %S=%S\n", key, val);
	}

	return ret;
}

static UINT64 GetIniHash(const wchar_t *section, const wchar_t *key, UINT64 def, bool *found)
{
	std::string val;
	UINT64 ret = def;
	int len;

	if (found)
		*found = false;

	if (GetIniString(section, key, NULL, &val)) {
		sscanf_s(val.c_str(), "%16llx%n", &ret, &len);
		if (len != val.length()) {
			IniWarning("WARNING: Hash parse error: %S=%s\n", key, val.c_str());
		} else {
			if (found)
				*found = true;
			LogInfo("  %S=%016llx\n", key, ret);
		}
	}

	return ret;
}

static int GetIniHexString(const wchar_t *section, const wchar_t *key, int def, bool *found)
{
	std::string val;
	int ret = def;
	int len;

	if (found)
		*found = false;

	if (GetIniString(section, key, NULL, &val)) {
		sscanf_s(val.c_str(), "%x%n", &ret, &len);
		if (len != val.length()) {
			IniWarning("WARNING: Hex string parse error: %S=%s\n", key, val.c_str());
		} else {
			if (found)
				*found = true;
			LogInfo("  %S=%x\n", key, ret);
		}
	}

	return ret;
}

// VS2013 BUG WORKAROUND: Make sure this class has a unique type name!
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
			IniWarning("WARNING: Unrecognised %S=%S\n", key, val);
		}
	}

	return ret;
}

// wchar_t* specialisation. Has character limit
// Want to remove this eventually, though since MarkingMode uses it and
// the DirectXTK API uses wide characters we might keep it around.
template <class T1, class T>
T GetIniEnumClass(const wchar_t *section, const wchar_t *key, T def, bool *found,
		struct EnumName_t<const wchar_t *, T> *enum_names)
{
	wchar_t val[MAX_PATH];
	T ret = def;
	bool tmp_found;

	if (found)
		*found = false;

	if (GetIniString(section, key, 0, val, MAX_PATH)) {
		ret = lookup_enum_val<const wchar_t *, T>(enum_names, val, def, &tmp_found);
		if (tmp_found) {
			if (found)
				*found = tmp_found;
			LogInfo("  %S=%S\n", key, val);
		} else {
			IniWarning("WARNING: Unknown %S=%S\n", key, val);
		}
	}

	return ret;
}

template <class T>
T GetIniEnumClass(const wchar_t *section, const wchar_t *key, T def, bool *found,
		struct EnumName_t<const wchar_t *, T> *enum_names)
{
	return GetIniEnumClass<const wchar_t *, T>(section, key, def, found, enum_names);
}

// char* specialisation of the above. No character limit
template <class T1, class T>
T GetIniEnumClass(const wchar_t *section, const wchar_t *key, T def, bool *found,
		struct EnumName_t<const char *, T> *enum_names)
{
	string val;
	T ret = def;
	bool tmp_found;

	if (found)
		*found = false;

	if (GetIniString(section, key, 0, &val)) {
		ret = lookup_enum_val<const char *, T>(enum_names, val.c_str(), def, &tmp_found);
		if (tmp_found) {
			if (found)
				*found = tmp_found;
			LogInfo("  %S=%s\n", key, val.c_str());
		} else {
			IniWarning("WARNING: Unknown %S=%s\n", key, val.c_str());
		}
	}

	return ret;
}

// Explicit template expansion is necessary to generate these functions for
// the compiler to generate them so they can be used from other source files:
template TransitionType GetIniEnumClass<const char *, TransitionType>(const wchar_t *section, const wchar_t *key, TransitionType def, bool *found,
		struct EnumName_t<const char *, TransitionType> *enum_names);
template MarkingMode GetIniEnumClass<const wchar_t *, MarkingMode>(const wchar_t *section, const wchar_t *key, MarkingMode def, bool *found,
		struct EnumName_t<const wchar_t *, MarkingMode> *enum_names);

// For options that used to be booleans and are now integers. Boolean values
// (0/1/true/false/yes/no/on/off) will continue retuning 0/1 for backwards
// compatibility and integers will return the integer value
static int GetIniBoolOrInt(const wchar_t *section, const wchar_t *key, int def, bool *found)
{
	int ret;
	bool tmp_found;

	ret = GetIniBool(section, key, !!def, &tmp_found, false);
	if (tmp_found) {
		if (found)
			*found = tmp_found;
		return ret;
	}

	return GetIniInt(section, key, def, found, false);
}

// For options that used to be booleans or integers and are now enums. Boolean
// values (0/1/true/false/yes/no/on/off) will continue retuning 0/1 for
// backwards compatibility, integers will return the integer value (provided it
// is within the range of the enum), otherwise the enum will be used.
static int GetIniBoolIntOrEnum(const wchar_t *section, const wchar_t *key, int def, bool *found,
		wchar_t *prefix, wchar_t *names[], int names_len, int first)
{
	int ret;
	bool tmp_found;

	ret = GetIniBoolOrInt(section, key, def, &tmp_found);
	if (tmp_found && ret >= 0 && ret < names_len) {
		if (found)
			*found = tmp_found;
		return ret;
	}

	return GetIniEnum(section, key, def, found, prefix, names, names_len, first);
}

static void GetUserConfigPath(const wchar_t *migoto_path)
{
	std::string tmp;
	wstring rel_path;

	GetIniString(L"Include", L"user_config", L"d3dx_user.ini", &tmp);
	rel_path = wstring(tmp.begin(), tmp.end()); // TODO: Sort out wide character mess
	if (tmp[1] != ':' && tmp[0] != '\\')
		G->user_config = wstring(migoto_path) + rel_path;
	else
		G->user_config = rel_path;
}

static void ParseIncludedIniFiles()
{
	IniSections include_sections;
	IniSections::iterator lower, upper, i;
	const wchar_t *section_id;
	IniSectionVector *section = NULL;
	IniSectionVector::iterator entry;
	wstring *key, *val;
	std::unordered_set<wstring> seen;
	wstring namespace_path, rel_path, ini_path;
	wchar_t migoto_path[MAX_PATH];
	vector<pcre2_code*> exclude;
	DWORD attrib;

	GetModuleFileName(migoto_handle, migoto_path, MAX_PATH);
	wcsrchr(migoto_path, L'\\')[1] = 0;

	// Grab the user_config path before the below code removes it from the
	// ini_sections data structure:
	GetUserConfigPath(migoto_path);

	// Do this before removing [Include] from ini_sections. TODO: Allow
	// recursively included files to modify the exclude mid-recursion:
	exclude = globbing_vector_to_regex(GetIniStringMultipleKeys(L"Include", L"exclude_recursive"));

	do {
		// To safely allow included files to include more files, we
		// transfer the includes we currently know about into a
		// separate data structure and remove them from the global
		// ini_sections data structure. Then, after parsing more
		// included files anything new in the ini_sections data
		// will be included from one of the newly parsed files. We
		// repeat this process until no more include files appear.
		lower = ini_sections.lower_bound(wstring(L"Include"));
		upper = prefix_upper_bound(ini_sections, wstring(L"Include"));
		include_sections.clear();
		include_sections.insert(lower, upper);
		ini_sections.erase(lower, upper);

		for (i = include_sections.begin(); i != include_sections.end(); i++) {
			section_id = i->first.c_str();
			LogInfo("[%S]\n", section_id);

			_get_namespaced_section_path(&include_sections, i->first.c_str(), &namespace_path);

			_GetIniSection(&include_sections, &section, section_id);
			for (entry = section->begin(); entry < section->end(); entry++) {
				key = &entry->first;
				val = &entry->second;
				LogInfo("  %S=%S\n", key->c_str(), val->c_str());

				rel_path = namespace_path + *val;

				// This is not a strong protection against including the same file multiple times,
				// but it is intended to ensure that this do while loop will eventually terminate.
				if (seen.count(rel_path)) {
					IniWarning("WARNING: File included multiple times: %S\n", rel_path.c_str());
					continue;
				}
				seen.insert(rel_path);

				if (!wcscmp(key->c_str(), L"include")) {
					ini_path = wstring(migoto_path) + rel_path;
					ParseNamespacedIniFile(ini_path.c_str(), &rel_path);
				} else if (!wcscmp(key->c_str(), L"include_recursive")) {
					ParseIniFilesRecursive(migoto_path, rel_path, exclude);
				} else if (!wcscmp(key->c_str(), L"exclude_recursive")) {
					// Handled above
				} else if (!wcscmp(key->c_str(), L"user_config")) {
					// Handled below
				} else {
					IniWarning("WARNING: Unrecognised entry: %S=%S\n", key->c_str(), rel_path.c_str());
				}
			}
		}
	} while (!include_sections.empty());

	free_globbing_vector(exclude);

	// User config is loaded very last to allow it to override all other
	// ini files.
	attrib = GetFileAttributes(G->user_config.c_str());
	if (attrib != INVALID_FILE_ATTRIBUTES)
		ParseNamespacedIniFile(G->user_config.c_str(), &G->user_config);
}

static void RegisterPresetKeyBindings()
{
	KeyOverrideType type;
	shared_ptr<KeyOverrideBase> preset;
	int delay, release_delay;
	IniSections::iterator lower, upper, i;
	vector<wstring> keys;
	vector<wstring> back;

	lower = ini_sections.lower_bound(wstring(L"Key"));
	upper = prefix_upper_bound(ini_sections, wstring(L"Key"));

	for (i = lower; i != upper; i++) {
		const wchar_t *id = i->first.c_str();

		LogInfo("[%S]\n", id);

		keys = GetIniStringMultipleKeys(id, L"Key");
		back = GetIniStringMultipleKeys(id, L"Back");
		if (keys.empty() && back.empty()) {
			IniWarning("WARNING: [%S] missing Key=\n", id);
			continue;
		}

		type = GetIniEnumClass(id, L"type", KeyOverrideType::ACTIVATE, NULL, KeyOverrideTypeNames);

		delay = GetIniInt(id, L"delay", 0, NULL);
		release_delay = GetIniInt(id, L"release_delay", 0, NULL);

		if (type == KeyOverrideType::CYCLE) {
			shared_ptr<KeyOverrideCycle> cycle_preset = make_shared<KeyOverrideCycle>();
			shared_ptr<KeyOverrideCycleBack> cycle_back = make_shared<KeyOverrideCycleBack>(cycle_preset);
			preset = cycle_preset;
			for (wstring key : back)
				RegisterKeyBinding(L"Back", key.c_str(), cycle_back, 0, delay, release_delay);
		} else {
			preset = make_shared<KeyOverride>(type);
		}

		preset->ParseIniSection(id);

		for (wstring key : keys)
			RegisterKeyBinding(L"Key", key.c_str(), preset, 0, delay, release_delay);
	}
}

static void EnumeratePresetOverrideSections()
{
	wstring preset_id;
	IniSections::iterator lower, upper, i;

	presetOverrides.clear();

	lower = ini_sections.lower_bound(wstring(L"Preset"));
	upper = prefix_upper_bound(ini_sections, wstring(L"Preset"));

	for (i = lower; i != upper; i++) {
		const wchar_t *id = i->first.c_str();

		// Convert to lower case
		preset_id = id;
		std::transform(preset_id.begin(), preset_id.end(), preset_id.begin(), ::towlower);

		// Construct a preset in the global list:
		presetOverrides[preset_id];
	}
}

static void ParsePresetOverrideSections()
{
	PresetOverrideMap::iterator i;
	PresetOverride *preset;

	for (i = begin(presetOverrides); i != end(presetOverrides); i++) {
		const wchar_t *id = i->first.c_str();
		preset = &i->second;

		LogInfo("[%S]\n", id);

		// Read parameters from ini
		preset->ParseIniSection(id);
		preset->unique_triggers_required = GetIniInt(id, L"unique_triggers_required", 0, NULL);
	}
}

static char* type_to_format(float type)
{
	return "%f%n";
}

static char* type_to_format(unsigned int type)
{
	return "%u%n";
}

static char* type_to_format(signed int type)
{
	return "%i%n";
}

static char* type_to_format(unsigned short type)
{
	return "%hu%n";
}

static char* type_to_format(signed short type)
{
	return "%hi%n";
}

static char* type_to_format(unsigned char type)
{
	return "%hhu%n";
}

static char* type_to_format(signed char type)
{
	return "%hhi%n";
}

template <typename T>
static std::vector<T> string_to_typed_array(std::istringstream *tokens)
{
	std::string token;
	std::vector<T> list;
	T val = 0;
	int ret, len;
	unsigned uval;

	while (std::getline(*tokens, token, ' ')) {
		if (token.empty())
			continue;

		ret = sscanf_s(token.c_str(), "0x%x%n", &uval, &len);
		if (ret != 0 && ret != EOF && len == token.length()) {
			// Reinterpret the 32bit unsigned integer as whatever
			// type we are supposed to be returning.
			// Classic endian bug: This conversion only works in
			// little-endian when converting to a smaller type
			list.push_back(*(T*)&uval);
			continue;
		}

		ret = sscanf_s(token.c_str(), type_to_format(val), &val, &len);
		if (ret != 0 && ret != EOF && len == token.length()) {
			list.push_back(val);
			continue;
		}

		IniWarning("WARNING: Parse error: %s\n", token.c_str());
	}

	return list;
}

template <typename T>
static void ConstructInitialData(CustomResource *custom_resource, std::istringstream *tokens)
{
	std::vector<T> vals;

	vals = string_to_typed_array<T>(tokens);

	// We use malloc() here because the custom resource may realloc() the
	// buffer to the correct size when substantiating:
	custom_resource->initial_data_size = sizeof(T) * vals.size();
	custom_resource->initial_data = malloc(custom_resource->initial_data_size);
	if (!custom_resource->initial_data) {
		IniWarning("ERROR allocating initial data\n");
		return;
	}

	memcpy(custom_resource->initial_data, vals.data(), custom_resource->initial_data_size);
}


static void ConstructInitialDataNorm(CustomResource *custom_resource, std::istringstream *tokens, int bytes, bool snorm)
{
	std::vector<float> vals;
	union {
		void *union_buf;
		unsigned short *unorm16_buf;
		signed short *snorm16_buf;
		unsigned char *unorm8_buf;
		signed char *snorm8_buf;
	};
	unsigned i;
	float val;

	vals = string_to_typed_array<float>(tokens);

	// We use malloc() here because the custom resource may realloc() the
	// buffer to the correct size when substantiating:
	custom_resource->initial_data_size = bytes * vals.size();
	custom_resource->initial_data = malloc(custom_resource->initial_data_size);
	if (!custom_resource->initial_data) {
		IniWarning("ERROR allocating initial data\n");
		return;
	}

	union_buf = custom_resource->initial_data;

	for (i = 0; i < vals.size(); i++) {
		val = vals[i];

		if (isnan(val)) {
			IniWarning("WARNING: Special value unsupported as normalized integer: %f\n", val);
			val = 0;
		} else if (snorm) {
			if (val < -1.0 || val > 1.0)
				IniWarning("WARNING: Value out of [-1, +1] range: %f\n", val);
			val = max(min(val, 1.0f), -1.0f);
		} else {
			if (val < 0.0 || val > 1.0)
				IniWarning("WARNING: Value out of [0, +1] range: %f\n", val);
			val = max(min(val, 1.0f), 0.0f);
		}

		if (bytes == 2) {
			if (snorm)
				snorm16_buf[i] = (signed short)(val * 0x7fff);
			else
				unorm16_buf[i] = (unsigned short)(val * 0xffff);
		} else {
			if (snorm)
				snorm8_buf[i] = (signed char)(val * 0x7f);
			else
				unorm8_buf[i] = (unsigned char)(val * 0xff);
		}
	}
}

static void ConstructInitialDataString(CustomResource *custom_resource, std::string *data)
{
	// Currently this requires a byte array format, though we could
	// possibly add utf32 (... or the worst-of-all-worlds utf16) in the
	// future to support text shaders with international character support.
	// The format cannot currently be specified inline, though we will
	// allow it to be implied if not specified.
	switch(custom_resource->override_format) {
	case (DXGI_FORMAT)-1:
		custom_resource->format = DXGI_FORMAT_R8_UINT;
		// Fall through
	case DXGI_FORMAT_R8_UINT:
	case DXGI_FORMAT_R8_SINT:
		custom_resource->initial_data_size = data->length() - 2;
		custom_resource->initial_data = malloc(custom_resource->initial_data_size);
		if (!custom_resource->initial_data) {
			IniWarning("ERROR allocating initial data\n");
			return;
		}
		memcpy(custom_resource->initial_data, data->c_str() + 1, custom_resource->initial_data_size);
		return;
	default:
		IniWarning("WARNING: unsupported format for specifying initial data as text\n");
		return;
	}
}

static void ParseResourceInitialData(CustomResource *custom_resource, const wchar_t *section)
{
	std::string setting, token;
	int format_size = 0;
	int format_type = 0;
	DXGI_FORMAT format;

	if (!GetIniStringAndLog(section, L"data", NULL, &setting))
		return;

	std::istringstream tokens(setting);

	switch (custom_resource->override_type) {
		case CustomResourceType::BUFFER:
		case CustomResourceType::STRUCTURED_BUFFER:
		case CustomResourceType::RAW_BUFFER:
			break;
		default:
			IniWarning("WARNING: initial data currently only supported on buffers\n");
			// TODO: Support Textures as well (remember to fill out row/depth pitch)
			return;
	}

	if (!custom_resource->filename.empty()) {
		IniWarning("WARNING: initial data and filename cannot be used together\n");
		return;
	}

	// Check for text data:
	if (setting.length() >= 2 && setting.front() == '"' && setting.back() == '"')
		return ConstructInitialDataString(custom_resource, &setting);

	// The format can be specified inline as the first entry in the data
	// line, or separately as its own setting. Specifying it inline is
	// mostly intended for structured buffers, where the resource doesn't
	// have one format, but we might still want to specify initial data,
	// and we will need a format for that. Later we might expand this to
	// allow formats to be specified elsewhere in the data line to switch
	// parsing formats on the fly for more complex structured buffers.
	// e.g. data = R32_FLOAT 1 2 3 4
	std::getline(tokens, token, ' ');
	format = ParseFormatString(token.c_str(), false);
	if (format == (DXGI_FORMAT)-1) {
		format = custom_resource->override_format;
		tokens.seekg(0);
	}

	switch (format) {
	case DXGI_FORMAT_R32G32B32A32_FLOAT:
	case DXGI_FORMAT_R32G32B32_FLOAT:
	case DXGI_FORMAT_R32G32_FLOAT:
	case DXGI_FORMAT_D32_FLOAT:
	case DXGI_FORMAT_R32_FLOAT:
		ConstructInitialData<float>(custom_resource, &tokens);
		break;

	case DXGI_FORMAT_R32G32B32A32_UINT:
	case DXGI_FORMAT_R32G32B32_UINT:
	case DXGI_FORMAT_R32G32_UINT:
	case DXGI_FORMAT_R32_UINT:
		ConstructInitialData<unsigned int>(custom_resource, &tokens);
		break;

	case DXGI_FORMAT_R32G32B32A32_SINT:
	case DXGI_FORMAT_R32G32B32_SINT:
	case DXGI_FORMAT_R32G32_SINT:
	case DXGI_FORMAT_R32_SINT:
		ConstructInitialData<signed int>(custom_resource, &tokens);
		break;

	// TODO: 16-bit floats:
	// case DXGI_FORMAT_R16G16B16A16_FLOAT:
	// case DXGI_FORMAT_R16G16_FLOAT:
	// case DXGI_FORMAT_R16_FLOAT:
	// 	break;

	case DXGI_FORMAT_R16G16B16A16_UNORM:
	case DXGI_FORMAT_R16G16_UNORM:
	case DXGI_FORMAT_D16_UNORM:
	case DXGI_FORMAT_R16_UNORM:
		ConstructInitialDataNorm(custom_resource, &tokens, 2, false);
		break;

	case DXGI_FORMAT_R16G16B16A16_SNORM:
	case DXGI_FORMAT_R16G16_SNORM:
	case DXGI_FORMAT_R16_SNORM:
		ConstructInitialDataNorm(custom_resource, &tokens, 2, true);
		break;

	case DXGI_FORMAT_R16G16B16A16_UINT:
	case DXGI_FORMAT_R16G16_UINT:
	case DXGI_FORMAT_R16_UINT:
		ConstructInitialData<unsigned short>(custom_resource, &tokens);
		break;

	case DXGI_FORMAT_R16G16B16A16_SINT:
	case DXGI_FORMAT_R16G16_SINT:
	case DXGI_FORMAT_R16_SINT:
		ConstructInitialData<signed short>(custom_resource, &tokens);
		break;

	case DXGI_FORMAT_R8G8B8A8_UNORM:
	case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
	case DXGI_FORMAT_R8G8_UNORM:
	case DXGI_FORMAT_R8_UNORM:
	case DXGI_FORMAT_A8_UNORM:
	case DXGI_FORMAT_R8G8_B8G8_UNORM:
	case DXGI_FORMAT_G8R8_G8B8_UNORM:
	case DXGI_FORMAT_B8G8R8A8_UNORM:
	case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
	// TODO: Not positive if I want to auto-expand the unused field to 0,
	// or parse it like the A8 versions. Putting off the decision:
	//	case DXGI_FORMAT_B8G8R8X8_UNORM:
	//	case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
		ConstructInitialDataNorm(custom_resource, &tokens, 1, false);
		break;

	case DXGI_FORMAT_R8G8B8A8_SNORM:
	case DXGI_FORMAT_R8G8_SNORM:
	case DXGI_FORMAT_R8_SNORM:
		ConstructInitialDataNorm(custom_resource, &tokens, 1, true);
		break;

	case DXGI_FORMAT_R8G8B8A8_UINT:
	case DXGI_FORMAT_R8G8_UINT:
	case DXGI_FORMAT_R8_UINT:
		ConstructInitialData<unsigned char>(custom_resource, &tokens);
		break;

	case DXGI_FORMAT_R8G8B8A8_SINT:
	case DXGI_FORMAT_R8G8_SINT:
	case DXGI_FORMAT_R8_SINT:
		ConstructInitialData<signed char>(custom_resource, &tokens);
		break;

	// TODO: case DXGI_FORMAT_R1_UNORM:

	default:
		IniWarning("WARNING: unsupported format for specifying initial data\n");
		return;
	}
}

static void ParseResourceSections()
{
	IniSections::iterator lower, upper, i;
	wstring resource_id;
	CustomResource *custom_resource;
	wchar_t setting[MAX_PATH], path[MAX_PATH];
	wstring namespace_path;
	bool found;

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

		if (GetIniStringAndLog(i->first.c_str(), L"filename", 0, setting, MAX_PATH)) {
			// If this section was not in the main d3dx.ini, look
			// for a file relative to the config it came from
			// first, then try relative to the 3DMigoto directory:
			get_namespaced_section_path(i->first.c_str(), &namespace_path);
			found = false;
			if (!namespace_path.empty()) {
				GetModuleFileName(migoto_handle, path, MAX_PATH);
				wcsrchr(path, L'\\')[1] = 0;
				wcscat(path, namespace_path.c_str());
				wcscat(path, setting);
				if (GetFileAttributes(path) != INVALID_FILE_ATTRIBUTES)
					found = true;
			}
			if (!found) {
				GetModuleFileName(migoto_handle, path, MAX_PATH);
				wcsrchr(path, L'\\')[1] = 0;
				wcscat(path, setting);
			}
			custom_resource->filename = path;
		}

		custom_resource->override_type = GetIniEnumClass(i->first.c_str(), L"type", CustomResourceType::INVALID, NULL, CustomResourceTypeNames);
		custom_resource->override_mode = GetIniEnumClass(i->first.c_str(), L"mode", CustomResourceMode::DEFAULT, NULL, CustomResourceModeNames);

		if (GetIniString(i->first.c_str(), L"format", 0, setting, MAX_PATH)) {
			custom_resource->override_format = ParseFormatString(setting, true);
			if (custom_resource->override_format == (DXGI_FORMAT)-1) {
				IniWarning("WARNING: Unknown format \"%S\"\n", setting);
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
			custom_resource->override_bind_flags = parse_enum_option_string<const wchar_t *, CustomResourceBindFlags, wchar_t*>
				(CustomResourceBindFlagNames, setting, NULL);
		}

		if (GetIniStringAndLog(i->first.c_str(), L"misc_flags", 0, setting, MAX_PATH)) {
			custom_resource->override_misc_flags = parse_enum_option_string<const wchar_t *, ResourceMiscFlags, wchar_t*>
				(ResourceMiscFlagNames, setting, NULL);
		}

		ParseResourceInitialData(custom_resource, i->first.c_str());
	}
}

static bool ParseCommandListLine(const wchar_t *ini_section,
		const wchar_t *lhs, wstring *rhs, wstring *raw_line,
		CommandList *command_list,
		CommandList *explicit_command_list,
		CommandList *pre_command_list,
		CommandList *post_command_list,
		const wstring *ini_namespace)
{
	if (ParseCommandListGeneralCommands(ini_section, lhs, rhs, explicit_command_list, pre_command_list, post_command_list, ini_namespace))
		return true;

	if (ParseCommandListIniParamOverride(ini_section, lhs, rhs, command_list, ini_namespace))
		return true;

	if (ParseCommandListVariableAssignment(ini_section, lhs, rhs, raw_line, command_list, pre_command_list, post_command_list, ini_namespace))
		return true;

	if (ParseCommandListResourceCopyDirective(ini_section, lhs, rhs, command_list, ini_namespace))
		return true;

	if (raw_line && !explicit_command_list &&
			ParseCommandListFlowControl(ini_section, raw_line, pre_command_list, post_command_list, ini_namespace))
		return true;

	return false;
}

static bool ParseCommandListLine(const wchar_t *ini_section,
		const wchar_t *lhs, const wchar_t *rhs, wstring *raw_line,
		CommandList *command_list,
		const wstring *ini_namespace)
{
	wstring srhs = wstring(rhs);

	return ParseCommandListLine(ini_section, lhs, &srhs, raw_line, command_list, command_list, NULL, NULL, ini_namespace);
}

// This tries to parse each line in a section in order as part of a command
// list. A list of keys that may be parsed elsewhere can be passed in so that
// it can warn about unrecognised keys and detect duplicate keys that aren't
// part of the command list.
static void ParseCommandList(const wchar_t *id,
		CommandList *pre_command_list, CommandList *post_command_list,
		wchar_t *whitelist[], bool register_command_lists=true)
{
	IniSectionVector *section = NULL;
	IniSectionVector::iterator entry;
	wstring *key, *val, *raw_line;
	const wchar_t *key_ptr;
	CommandList *command_list, *explicit_command_list;
	IniSectionSet whitelisted_keys;
	CommandListScope scope;
	int i;

	// Safety check to make sure we are keeping the command list section
	// list up to date:
	if (!IsCommandListSection(id)) {
		LogInfoW(L"BUG: ParseCommandList() called on a section not in the CommandListSections list: %s\n", id);
		DoubleBeepExit();
	}

	scope.emplace_front();

	LogDebug("Registering command list: %S\n", id);
	pre_command_list->ini_section = id;
	pre_command_list->post = false;
	pre_command_list->scope = &scope;
	if (register_command_lists)
		registered_command_lists.push_back(pre_command_list);
	if (post_command_list) {
		post_command_list->ini_section = id;
		post_command_list->post = true;
		post_command_list->scope = &scope;
		if (register_command_lists)
			registered_command_lists.push_back(post_command_list);
	}

	GetIniSection(&section, id);
	for (entry = section->begin(); entry < section->end(); entry++) {
		key = &entry->first;
		val = &entry->second;
		raw_line = &entry->raw_line;

		// Convert key + val to lower case since ini files are supposed
		// to be case insensitive:
		std::transform(key->begin(), key->end(), key->begin(), ::towlower);
		std::transform(val->begin(), val->end(), val->begin(), ::towlower);
		std::transform(raw_line->begin(), raw_line->end(), raw_line->begin(), ::towlower);

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
					IniWarningW(L"WARNING: Duplicate non-command list key found in " INI_FILENAME L": [%ls] %ls\n", id, key->c_str());
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

		if (ParseCommandListLine(id, key_ptr, val, raw_line, command_list, explicit_command_list, pre_command_list, post_command_list, &entry->ini_namespace)) {
			LogInfo("  %S\n", raw_line->c_str());
			continue;
		}

		if (entry->ini_namespace == G->user_config && !G->user_config.empty()) {
			// Invalid command, but it is in the user config, which may happen
			// if the user recently uninstalled/upgraded/etc a mod. We will flag
			// the user config to be updated at the next save, but won't do this
			// immediately just in case. Inform the user of what is happening.
			if (!G->user_config_dirty) {
				LogOverlay(LOG_WARNING,
					"NOTICE: Unknown user settings will be removed from d3dx_user.ini\n"
					" This is normal if you recently removed/changed any mods\n"
					" Press %S to update the config now, or %S to reset all settings to default\n"
					" The first unrecognised entry was: \"%S\"\n",
					user_friendly_ini_key_binding(L"Hunting", L"reload_config").c_str(),
					user_friendly_ini_key_binding(L"Hunting", L"wipe_user_config").c_str(),
					raw_line->c_str());
				// Once the [Constants] command list has finished running the
				// low bit will be cleared to ensure that loading the user config
				// itself cannot mark the user config as dirty. Set the second
				// bit to indicate that it should be updated regardless:
				G->user_config_dirty |= 2;
			}
			// There might be a lot of entries if a large mod was just
			// uninstalled, so we only show the first bad setting on the
			// overlay and log all other invalid settings to the log file:
			LogInfo("WARNING: Unrecognised entry in %S: %S\n", G->user_config.c_str(), raw_line->c_str());
			continue;
		}

		IniWarning("WARNING: Unrecognised entry: %S\n", raw_line->c_str());
	}

	// Don't need the scope objects once parsing is complete. If all
	// if/endifs were balanced correctly we should be back to the initial
	// scope, so warn if we aren't:
	if (std::distance(begin(scope), end(scope)) != 1)
		IniWarning("WARNING: [%S] scope unbalanced\n", id);

	pre_command_list->scope = NULL;
	if (post_command_list)
		post_command_list->scope = NULL;
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

static void ParseConstantsSection()
{
	VariableFlags flags;
	IniSectionVector *section = NULL;
	IniSectionVector::iterator entry, next;
	wstring *key, *val, name;
	const wchar_t *name_pos;
	const wstring *ini_namespace;
	std::pair<CommandListVariables::iterator, bool> inserted;
	float fval;
	int len;

	// The naming on this one is historical - [Constants] used to define
	// iniParams that couldn't change, then later we allowed them to be
	// changed by key inputs and this became the initial state, and now
	// this is implemented as a command list run on immediate context
	// creation & config reload, which allows it to be used for any one
	// time initialisation.
	LogInfo("[Constants]\n");

	// We pass this section in two stages - the first pass is only looking
	// for global variable declarations, and the second pass is as any
	// other command list (with one extra flag to tell it not to warn about
	// the "global" keyword). The reason for this is so that a global
	// variable defined in an included config file can be set from the
	// [Constants] section in the main d3dx.ini or potentially another
	// config file (d3dx_user.ini?). This covers cases such as setting the
	// 3dvision2sbs mode, but still ensures that setting the variable will
	// throw an error if 3dvision2sbs.ini was not included.

	command_list_globals.clear();
	persistent_variables.clear();
	GetIniSection(&section, L"Constants");
	for (next = section->begin(), entry = next; entry < section->end(); entry = next) {
		next++;
		key = &entry->first;
		val = &entry->second;
		ini_namespace = &entry->ini_namespace;

		// The variable name will either be in the key if this line
		// also includes an assignment, or in raw_line if it does not:
		if (!key->empty())
			name = *key;
		else
			name = entry->raw_line;

		// Convert variable name to lower case since ini files are
		// supposed to be case insensitive:
		std::transform(name.begin(), name.end(), name.begin(), ::towlower);

		// Globals do not support pre/post since they are declarations
		// with static initialisers where pre/post doesn't make sense
		// (and [Constants] doesn't support them as yet either)

		flags = parse_enum_option_string_prefix<const wchar_t *, VariableFlags>
			(VariableFlagNames, name.c_str(), &name_pos);
		if (!(flags & VariableFlags::GLOBAL))
			continue;
		name = name_pos;

		if (!valid_variable_name(name)) {
			IniWarning("WARNING: Illegal global variable name: \"%S\"\n", name.c_str());
			continue;
		}

		if (!ini_namespace->empty())
			name = get_namespaced_var_name_lower(name, ini_namespace);

		// Initialisation is optional and deferred until the command
		// list is run
		// If the initialiser is present and simple
		fval = 0.0f;
		if (!val->empty()) {
			swscanf_s(val->c_str(), L"%f%n", &fval, &len);
			if (len != val->length()) {
				IniWarning("WARNING: Floating point parse error: %S=%S\n", key->c_str(), val->c_str());
				continue;
			}
		}

		inserted = command_list_globals.emplace(name, CommandListVariable{name, fval, flags});
		if (!inserted.second) {
			IniWarning("WARNING: Redeclaration of %S\n", name.c_str());
			continue;
		}

		if (flags & VariableFlags::PERSIST)
			persistent_variables.emplace_back(&inserted.first->second);

		if (val->empty())
			LogInfo("  global %S\n", name.c_str());
		else
			LogInfo("  global %S=%f\n", name.c_str(), fval);

		// Remove this line from the ini section data structures so the
		// command list won't consider it in the 2nd pass:
		next = section->erase(entry);
	}

	// Second pass for the command list:
	G->constants_command_list.clear();
	G->post_constants_command_list.clear();
	ParseCommandList(L"Constants", &G->constants_command_list, &G->post_constants_command_list, NULL);
}

static wchar_t *true_false_overrule[] = {
	L"false", // GetIniBoolIntOrEnum will also accept 0/false/no/off
	L"true", // GetIniBoolIntOrEnum will also accept 1/true/yes/on
	L"overrule", // GetIniBoolIntOrEnum will also accept 2
};

static void check_shaderoverride_duplicates(bool duplicate, const wchar_t *id, ShaderOverride *override, UINT64 hash)
{
	int allow_duplicates;

	// Options to permit ShaderOverride sections with duplicate hashes.
	// This has to be explicitly opted in to and the section names still
	// have to be unique (or namespaced), and Note that you won't get
	// warnings of duplicate settings between the sections, but at least we
	// try not to clobber their values from earlier sections with the
	// defaults.
	allow_duplicates = GetIniBoolIntOrEnum(id, L"allow_duplicate_hash", 0, NULL,
			NULL, true_false_overrule, ARRAYSIZE(true_false_overrule), 0);

	if (allow_duplicates == 2 || override->allow_duplicate_hashes == 2) {
		// Overrule - one section said it doesn't care if any other
		// sections have the same hash. Mostly for use with third party
		// mods where a mod author may not be able to change another
		// mod directly, but has confirmed that the two are ok to work
		// together. Far from perfect since it might allow other actual
		// conflicts to go through unchecked, but a reasonable
		// compromise.
		allow_duplicates = 2;
	} else {
		// Cooperative - all sections sharing the same hash must opt in
		// and will warn if even one section does not. This is intended
		// that scripts will set this flag on any sections they create
		// so that if a user creates a ShaderOverride with the same
		// hash they will get a warning at first, but can choose to
		// allow it so that they can add their own commands without
		// having to merge them with the section from the script,
		// allowing all the auto generated sections to be grouped
		// together. The section names still have to be distinct, which
		// offers protection against scripts adding multiple identical
		// sections if run multiple times.
		allow_duplicates = allow_duplicates && override->allow_duplicate_hashes;
	}

	if (duplicate && !allow_duplicates) {
		IniWarning("WARNING: Possible Mod Conflict: Duplicate ShaderOverride hash=%16llx\n"
			   "[%S]\n"
			   "[%S]\n"
			   "If this is intentional, add allow_duplicate_hash=true or allow_duplicate_hash=overrule to suppress warning\n",
			   hash, override->first_ini_section.c_str(), id);
	}

	override->allow_duplicate_hashes = allow_duplicates;
}

static void warn_deprecated_shaderoverride_options(const wchar_t *id, ShaderOverride *override)
{
	// I've seen several shaderhackers attempt to use the deprecated
	// partner= in a way that won't work recently. Detect, warn and
	// suggest an alternative. TODO: Add a way to check ps/vs/etc hashes
	// directly to simplify this.
	// TODO: Once we have a good simple alternative to the actual use case
	// of partner=, issue a non-conditional deprecation warning. This might
	// be something like if ps == ... ; handling=original ; endif
	if (override->partner_hash && (!override->command_list.commands.empty() || !override->post_command_list.commands.empty())) {
	        LogOverlay(LOG_NOTICE, "WARNING: [%S] tried to combine the deprecated partner= option with a command list.\n"
	                               "This almost certainly won't do what you want. Try something like this instead:\n"
	                               "\n"
	                               "[%S_VERTEX_SHADER]\n"
	                               "hash = <vertex shader hash>\n"
	                               "filter_index = 5\n"
	                               "\n"
	                               "[%S_PIXEL_SHADER]\n"
	                               "hash = <pixel shader hash>\n"
	                               "x = vs\n"
	                               "\n"
	                               , id, id, id);
	}

	if (override->depth_filter != DepthBufferFilter::NONE) {
	        LogOverlay(LOG_NOTICE, "NOTICE: [%S] used deprecated depth_filter option. Consider texture filtering for more flexibility:\n"
	                               "\n"
	                               "[%S]\n"
	                               "x = oD\n"
	                               "\n"
	                               "In the shader:\n"
	                               "if (asint(IniParams[0].x) == asint(-0.0)) {\n"
	                               "    // No depth buffer bound\n"
	                               "} else {\n"
	                               "    // Depth buffer bound\n"
	                               "}\n"
	                               "\n"
	                               "Or in assembly:\n"
	                               "dcl_resource_texture1d (float,float,float,float) t120\n"
	                               "ld_indexable(texture1d)(float,float,float,float) r0.x, l(0, 0, 0, 0), t120.xyzw\n"
	                               "ieq r0.x, r0.x, l(0x80000000)\n"
	                               "if_nz r0.x\n"
	                               "    // No depth buffer bound\n"
	                               "else\n"
	                               "    // Depth buffer bound\n"
	                               "endif\n"
	                        , id, id);
	}
}

// List of keys in [ShaderOverride] sections that are processed in this
// function. Used by ParseCommandList to find any unrecognised lines.
wchar_t *ShaderOverrideIniKeys[] = {
	L"hash",
	L"allow_duplicate_hash",
	L"depth_filter",
	L"partner",
	L"model",
	L"disable_scissor",
	L"filter_index",
	NULL
};
static void ParseShaderOverrideSections()
{
	IniSections::iterator lower, upper, i;
	wchar_t setting[MAX_PATH];
	const wchar_t *id;
	ShaderOverride *override;
	UINT64 hash;
	bool duplicate, found;
	bool disable_scissor;

	// Lock entire routine. This can be re-inited live.  These shaderoverrides
	// are unlikely to be changing much, but for consistency.
	//  We actually already lock the entire config reload, so this is redundant -DSS
	EnterCriticalSectionPretty(&G->mCriticalSection);

	G->mShaderOverrideMap.clear();

	lower = ini_sections.lower_bound(wstring(L"ShaderOverride"));
	upper = prefix_upper_bound(ini_sections, wstring(L"ShaderOverride"));
	for (i = lower; i != upper; i++) {
		id = i->first.c_str();

		LogInfo("[%S]\n", id);

		hash = GetIniHash(id, L"Hash", 0, &found);
		if (!found) {
			IniWarning("WARNING: [%S] missing Hash=\n", id);
			continue;
		}

		duplicate = !!G->mShaderOverrideMap.count(hash);
		override = &G->mShaderOverrideMap[hash];
		if (!duplicate)
			override->first_ini_section = id;

		check_shaderoverride_duplicates(duplicate, id, override, hash);

		override->depth_filter = GetIniEnumClass(id, L"depth_filter", DepthBufferFilter::NONE, NULL, DepthBufferFilterNames);

		// Simple partner shader filtering. Deprecated - more advanced
		// filtering can be achieved by setting an ini param in the
		// partner's [ShaderOverride] section, or the below filter_index
		override->partner_hash = GetIniHash(id, L"partner", 0, NULL);

		// Superior partner shader filtering that also supports a bound/unbound case
		override->filter_index = GetIniFloat(id, L"filter_index", FLT_MAX, NULL);
		// Backup version not affected by ShaderRegex:
		override->backup_filter_index = override->filter_index;

		if (GetIniStringAndLog(id, L"model", 0, setting, MAX_PATH)) {
			wcstombs(override->model, setting, ARRAYSIZE(override->model));
			override->model[ARRAYSIZE(override->model) - 1] = '\0';
		}

		ParseCommandList(id, &override->command_list, &override->post_command_list, ShaderOverrideIniKeys);

		// For backwards compatibility with Nier Automata fix,
		// translate disable_scissor into an equivalent command list:
		disable_scissor = GetIniBool(id, L"disable_scissor", false, &found);
		if (found) {
			wstring ini_namespace;
			get_section_namespace(id, &ini_namespace);

			if (disable_scissor)
				ParseCommandListLine(id, L"run", L"builtincustomshaderdisablescissorclipping", NULL, &override->command_list, &ini_namespace);
			else
				ParseCommandListLine(id, L"run", L"builtincustomshaderenablescissorclipping", NULL, &override->command_list, &ini_namespace);
		}

		warn_deprecated_shaderoverride_options(id, override);
	}
	LeaveCriticalSection(&G->mCriticalSection);
}

// Oh C++, do you really not have a .split() in your standard library?
static std::vector<std::wstring> split_string(const std::wstring *str, wchar_t sep)
{
	std::wistringstream tokens(*str);
	std::wstring token;
	std::vector<std::wstring> list;

	while (std::getline(tokens, token, sep))
		list.push_back(token);

	return list;
}
static std::vector<std::string> split_string(const std::string *str, char sep)
{
	std::istringstream tokens(*str);
	std::string token;
	std::vector<std::string> list;

	while (std::getline(tokens, token, sep))
		list.push_back(token);

	return list;
}

template <typename T>
static std::set<T> vec_to_set(std::vector<T> &v)
{
	return std::set<T>(v.begin(), v.end());
}

static uint32_t hash_ini_section(uint32_t hash, const wstring *sname)
{
	IniSectionVector *svec = NULL;
	IniSectionVector::iterator entry;

	hash = crc32c_hw(hash, sname->c_str(), sname->size());

	GetIniSection(&svec, sname->c_str());
	for (entry = svec->begin(); entry < svec->end(); entry++) {
		hash = crc32c_hw(hash, entry->raw_line.c_str(), entry->raw_line.size());
	}

	return hash;
}

// List of keys in [ShaderRegex] sections that are processed in this
// function. Used by ParseCommandList to find any unrecognised lines.
wchar_t *ShaderRegexIniKeys[] = {
	L"shader_model",
	L"temps",
	L"filter_index",
	// L"type" =asm/hlsl? I'd rather not encourage autofixes on HLSL
	//         shaders, because there is too much potential for trouble
	NULL
};
static bool parse_shader_regex_section_main(const std::wstring *section_id, ShaderRegexGroup *regex_group)
{
	std::string setting;
	std::vector<std::string> items;

	if (!GetIniStringAndLog(section_id->c_str(), L"shader_model", NULL, &setting)) {
		IniWarning("WARNING: [%S] missing shader_model\n", section_id->c_str());
		return false;
	}
	regex_group->shader_models = vec_to_set(split_string(&setting, ' '));

	if (GetIniStringAndLog(section_id->c_str(), L"temps", NULL, &setting))
		regex_group->temp_regs = vec_to_set(split_string(&setting, ' '));

	regex_group->ini_section = *section_id;

	regex_group->filter_index = GetIniFloat(section_id->c_str(), L"filter_index", FLT_MAX, NULL);

	ParseCommandList(section_id->c_str(), &regex_group->command_list, &regex_group->post_command_list, ShaderRegexIniKeys);
	return true;
}

static bool parse_shader_regex_section_pattern(const std::wstring *section_id, const std::wstring *pattern_id, ShaderRegexGroup *regex_group)
{
	IniSectionVector *section = NULL;
	IniSectionVector::iterator entry;
	ShaderRegexPattern *regex_pattern;
	std::wstring *wline;
	std::string aline, pattern;

	GetIniSection(&section, section_id->c_str());
	for (entry = section->begin(); entry < section->end(); entry++) {
		// FIXME: ini parser shouldn't be converting to wide characters
		// in the first place, but we have to change types all over the
		// place to fix that, which is a large and risky refactoring
		// job for another day
		wline = &entry->raw_line;
		aline = std::string(wline->begin(), wline->end());
		LogInfo("  %s\n", aline.c_str());
		pattern.append(aline);
	}

	// We also want to show the final pattern used for the regex with all
	// the newlines, blank lines, initial whitespace and ini file comments
	// stripped, so that if there is a problem the user can see exactly
	// what we used. This will look ugly, but will make errors like missing
	// \n or \s+ easier to spot.
	LogInfo("--------- final pcre2 regex pattern used after ini parsing ---------\n");
	LogInfo("%s\n", pattern.c_str());
	LogInfo("--------------------------------------------------------------------\n");

	regex_pattern = &regex_group->patterns[*pattern_id];
	if (!regex_pattern->compile(&pattern))
		return false;

	if (regex_pattern->named_group_overlaps(regex_group->temp_regs)) {
		IniWarning("WARNING: Named capture group overlaps with temp regs!\n");
		return false;
	}

	// TODO: Also check for overlapping named capture groups between
	// patterns in a single regex group.

	// TODO: Log the final computed value of PCRE2_INFO_ALLOPTIONS

	return true;
}

static bool parse_shader_regex_section_declarations(const std::wstring *section_id, const std::wstring *pattern_id, ShaderRegexGroup *regex_group)
{
	IniSectionVector *section = NULL;
	IniSectionVector::iterator entry;
	std::wstring *wline;
	std::string aline;

	GetIniSection(&section, section_id->c_str());
	for (entry = section->begin(); entry < section->end(); entry++) {
		// FIXME: ini parser shouldn't be converting to wide characters
		// in the first place, but we have to change types all over the
		// place to fix that, which is a large and risky refactoring
		// job for another day
		wline = &entry->raw_line;
		aline = std::string(wline->begin(), wline->end());
		LogInfo("  %s\n", aline.c_str());
		regex_group->declarations.push_back(aline);
	}

	return true;
}

static bool parse_shader_regex_section_replace(const std::wstring *section_id, const std::wstring *pattern_id, ShaderRegexGroup *regex_group)
{
	IniSectionVector *section = NULL;
	IniSectionVector::iterator entry;
	ShaderRegexPattern *regex_pattern;
	std::wstring *wline;
	std::string aline;

	try {
		regex_pattern = &regex_group->patterns.at(*pattern_id);
	} catch (std::out_of_range) {
		IniWarning("WARNING: Missing corresponding pattern section for %S\n", section_id->c_str());
		return false;
	}

	GetIniSection(&section, section_id->c_str());
	for (entry = section->begin(); entry < section->end(); entry++) {
		// FIXME: ini parser shouldn't be converting to wide characters
		// in the first place, but we have to change types all over the
		// place to fix that, which is a large and risky refactoring
		// job for another day
		wline = &entry->raw_line;
		aline = std::string(wline->begin(), wline->end());
		LogInfo("  %s\n", aline.c_str());
		regex_pattern->replace.append(aline);
	}

	// Similar to above we want to see the final substitution string after
	// ini parsing, especially to help spot missing newlines. TODO: Add an
	// option to automatically add newlines after every ini line.
	LogInfo("--------- final pcre2 replace string used after ini parsing ---------\n");
	LogInfo("%s\n", regex_pattern->replace.c_str());
	LogInfo("---------------------------------------------------------------------\n");

	regex_pattern->do_replace = true;
	return true;
}

static ShaderRegexGroup* get_regex_group(std::wstring *regex_id, bool allow_creation)
{
	if (allow_creation)
		return &shader_regex_groups[*regex_id];

	try {
		return &shader_regex_groups.at(*regex_id);
	} catch (std::out_of_range) {
		IniWarning("WARNING: Missing [%S] section\n", regex_id->c_str());
		return NULL;
	}
}

static void delete_regex_group(std::wstring *regex_id)
{
	ShaderRegexGroups::iterator i;

	i = shader_regex_groups.find(*regex_id);
	shader_regex_groups.erase(i);
}

static void ParseShaderRegexSections()
{
	IniSections::iterator lower, upper, i;
	const std::wstring *section_id;
	std::wstring section_prefix, section_suffix;
	std::vector<std::wstring> subsection_names;
	ShaderRegexGroup *regex_group;
	ShaderRegexGroups::iterator j;
	size_t namespace_endpos = 0;
	uint32_t hash = 0;

	shader_regex_group_index.clear();
	shader_regex_groups.clear();

	// Hash any settings that may alter assembly or otherwise have an
	// effect on ShaderRegex to invalidate the cache if these change:
	hash = crc32c_hw(hash, &G->assemble_signature_comments, sizeof(G->assemble_signature_comments));
	hash = crc32c_hw(hash, &G->disassemble_undecipherable_custom_data, sizeof(G->disassemble_undecipherable_custom_data));
	hash = crc32c_hw(hash, &G->patch_cb_offsets, sizeof(G->patch_cb_offsets));

	lower = ini_sections.lower_bound(wstring(L"ShaderRegex"));
	upper = prefix_upper_bound(ini_sections, wstring(L"ShaderRegex"));
	for (i = lower; i != upper; i++) {
		section_id = &i->first;
		LogInfo("[%S]\n", section_id->c_str());

		hash = hash_ini_section(hash, section_id);

		// namespaced sections may have a dot in the namespace, so we
		// only split the string after the namespace text
		namespace_endpos = get_section_namespace_endpos(section_id->c_str());
		section_prefix = section_id->substr(0, namespace_endpos);
		section_suffix = section_id->substr(namespace_endpos);
		subsection_names = split_string(&section_suffix, L'.');
		if (subsection_names.size())
			subsection_names[0] = section_prefix + subsection_names[0];
		else
			subsection_names.push_back(section_prefix);

		regex_group = get_regex_group(&subsection_names[0], subsection_names.size() == 1);
		if (!regex_group)
			continue;

		switch (subsection_names.size()) {
			case 1:
				if (parse_shader_regex_section_main(section_id, regex_group))
					continue;
				break;
			case 2:
				if (!_wcsicmp(subsection_names[1].c_str(), L"Pattern")) {
					// TODO: Allow multiple patterns per regex group, but not before
					// our custom substitution logic is implemented to allow named capture
					// groups matched in one pattern to be substituted into another, and
					// ensure that identically named groups match in all patterns.
					//
					// Until then, the user will just have to write longer regex patterns
					// and substitutions to match everything they need in one go.
					if (parse_shader_regex_section_pattern(section_id, &subsection_names[1], regex_group))
						continue;
				} else if (!_wcsicmp(subsection_names[1].c_str(), L"InsertDeclarations")) {
					if (parse_shader_regex_section_declarations(section_id, &subsection_names[1], regex_group))
						continue;
				}
				break;
			case 3:
				if (!_wcsnicmp(subsection_names[1].c_str(), L"Pattern", 7)
				 && !_wcsicmp(subsection_names[2].c_str(), L"Replace")) {
					if (parse_shader_regex_section_replace(section_id, &subsection_names[1], regex_group))
						continue;
				}
				break;
		}


		// We delete the whole regex data structure if any of the subsections
		// are not present, or fail to parse or compile so that we don't end up
		// applying an incomplete regex to any shaders.
		IniWarning("WARNING: disabling entire shader regex group [%S]\n", subsection_names[0].c_str());
		delete_regex_group(&subsection_names[0]);
	}

	// When we load ShaderRegex metadata from the cache we need to look up
	// the command lists and filter_index from the data structures. The
	// shader_regex_hash means we know the data structures should be
	// identical to when the shader was first cached, and since
	// shader_regex_groups is sorted the order should be the same too.
	// Copy pointers to each of the groups to a vector so we can look them
	// up directly without iterating over the map:
	shader_regex_hash = hash;
	LogInfo("ShaderRegex hash: %08x\n", shader_regex_hash);
	for (j = shader_regex_groups.begin(); j != shader_regex_groups.end(); j++)
		shader_regex_group_index.push_back(&j->second);
}

// For fuzzy matching instead of using hash. Using terms consistent
// with [Resource] section. TODO: Consider providing MS naming aliases.
// If any of these appear in a section that also contains a hash= the parser
// will issue an error, since hash is always a specific match they cannot be
// mixed. Macro so this can be included in multiple string lists.
#define TEXTURE_OVERRIDE_FUZZY_MATCHES \
	L"match_type", \
	L"match_usage", \
	L"match_bind_flags", \
	L"match_cpu_access_flags", \
	L"match_misc_flags", \
	L"match_byte_width", \
	L"match_stride", \
	L"match_mips", \
	L"match_format", \
	L"match_width", \
	L"match_height", \
	L"match_depth", \
	L"match_array", \
	L"match_msaa", \
	L"match_msaa_quality"

// These match the draw context, and may be used in conjunction with either
// hash or fuzzy description matching:
#define TEXTURE_OVERRIDE_DRAW_CALL_MATCHES \
	L"match_first_vertex", \
	L"match_first_index", \
	L"match_first_instance", \
	L"match_vertex_count", \
	L"match_index_count", \
	L"match_instance_count"

// List of keys in [TextureOverride] sections that are processed in this
// function. Used by ParseCommandList to find any unrecognised lines.
wchar_t *TextureOverrideIniKeys[] = {
	L"hash",
	L"stereomode",
	L"format",
	L"width",
	L"height",
	L"width_multiply",
	L"height_multiply",
	L"iteration",
	L"filter_index",
	L"expand_region_copy",
	L"deny_cpu_read",
	L"match_priority",
	TEXTURE_OVERRIDE_FUZZY_MATCHES,
	TEXTURE_OVERRIDE_DRAW_CALL_MATCHES,
	NULL
};
// List of keys for fuzzy matching that cannot be used together with hash:
wchar_t *TextureOverrideFuzzyMatchesIniKeys[] = {
	TEXTURE_OVERRIDE_FUZZY_MATCHES,
	NULL
};

static void parse_fuzzy_numeric_match_expression_error(const wchar_t *text)
{
	IniWarning("WARNING: Unable to parse expression - must be in the simple form:\n"
	           "    [ operator ] value | field_name [ * field_name ] [ * multiplier ] [ / divider ]\n"
	           "    Parse error on text: \"%S\"\n", text);
}

static bool parse_fuzzy_field_name(const wchar_t **ptr, FuzzyMatchOperandType *field_type)
{
	bool ret;

	// whitespace
	for (; **ptr == L' '; ++*ptr);

	if (!wcsncmp(*ptr, L"width", 5)) {
		*field_type = FuzzyMatchOperandType::WIDTH;
		*ptr += 5;
	} else if (!wcsncmp(*ptr, L"height", 6)) {
		*field_type = FuzzyMatchOperandType::HEIGHT;
		*ptr += 6;
	} else if (!wcsncmp(*ptr, L"depth", 5)) {
		*field_type = FuzzyMatchOperandType::DEPTH;
		*ptr += 5;
	} else if (!wcsncmp(*ptr, L"array", 5)) {
		*field_type = FuzzyMatchOperandType::ARRAY;
		*ptr += 5;
	} else if (!wcsncmp(*ptr, L"res_width", 9)) {
		*field_type = FuzzyMatchOperandType::RES_WIDTH;
		*ptr += 9;
	} else if (!wcsncmp(*ptr, L"res_height", 10)) {
		*field_type = FuzzyMatchOperandType::RES_HEIGHT;
		*ptr += 10;
	}

	// Check field name terminated by whitespace
	ret = (**ptr == L'\0' || **ptr == L' ');

	// whitespace
	for (; **ptr == L' '; ++*ptr);

	return ret;
}

static void parse_fuzzy_numeric_match_expression(const wchar_t *setting, FuzzyMatch *matcher)
{
	const wchar_t *ptr = setting;
	int ret, len;

	// For now we're just supporting fairly simple expressions in the form:
	//
	//   [ operator ] ( value | ( field_name [ * multiplier ] [ / divider ] ) )
	//
	//     operator   =   "=" | "!" | "<" | ">" | "<=" | ">="
	//     field_name =   "width" | "height" | "depth" | "array" | "res_width" | "res_height"
	//     value, multiplier and divider are integers.
	//
	// That should be enough to match most things we need, including aspect
	// ratios, downsampled resources, etc. We can add a full expression
	// parser later if we really want.

	// operator. Make sure to check <= before < because of overlapping prefix:
	if (!wcsncmp(ptr, L"<=", 2)) {
		matcher->op = FuzzyMatchOp::LESS_EQUAL;
		ptr += 2;
	} else if (!wcsncmp(ptr, L">=", 2)) {
		matcher->op = FuzzyMatchOp::GREATER_EQUAL;
		ptr += 2;
	} else if (!wcsncmp(ptr, L"=", 1)) {
		matcher->op = FuzzyMatchOp::EQUAL;
		ptr++;
	} else if (!wcsncmp(ptr, L"!", 1)) {
		matcher->op = FuzzyMatchOp::NOT_EQUAL;
		ptr++;
	} else if (!wcsncmp(ptr, L"<", 1)) {
		matcher->op = FuzzyMatchOp::LESS;
		ptr++;
	} else if (!wcsncmp(ptr, L">", 1)) {
		matcher->op = FuzzyMatchOp::GREATER;
		ptr++;
	} else {
		matcher->op = FuzzyMatchOp::EQUAL;
	}

	// whitespace
	for (; *ptr == L' '; ptr++);

	// Try parsing remaining string as integer. Has to reach end of string.
	ret = swscanf_s(ptr, L"%u%n", &matcher->val, &len);
	if (ret != 0 && ret != EOF && len == wcslen(ptr))
		return;

	// field_name
	if (!parse_fuzzy_field_name(&ptr, &matcher->rhs_type1))
		return parse_fuzzy_numeric_match_expression_error(ptr);

	// numerator
	if (*ptr == L'*') {
		ret = swscanf_s(++ptr, L"%u%n", &matcher->numerator, &len);
		if (ret != 0 && ret != EOF) {
			ptr += len;
		} else {
			// No numerator (yet?). Check for 2nd named field? In
			// RE7: 'match_byte_width = res_width * res_height'
			if (!parse_fuzzy_field_name(&ptr, &matcher->rhs_type2))
				return parse_fuzzy_numeric_match_expression_error(ptr);

			// numerator?
			if (*ptr == L'*') {
				ret = swscanf_s(++ptr, L"%u%n", &matcher->numerator, &len);
				if (ret == 0 || ret == EOF)
					return parse_fuzzy_numeric_match_expression_error(ptr);
				ptr += len;
			}
		}
	}

	// whitespace
	for (; *ptr == L' '; ptr++);

	// denominator
	if (*ptr == L'/') {
		ret = swscanf_s(++ptr, L"%u%n", &matcher->denominator, &len);
		if (ret == 0 || ret == EOF)
			return parse_fuzzy_numeric_match_expression_error(ptr);
		if (matcher->denominator == 0) {
			matcher->denominator = 1;
			IniWarning("WARNING: Denominator is zero: %S\n", ptr);
			return;
		}
		ptr += len;
	}

	if (*ptr)
		return parse_fuzzy_numeric_match_expression_error(ptr);
}

static void parse_texture_override_common(const wchar_t *id, TextureOverride *override, bool register_command_lists)
{
	wchar_t setting[MAX_PATH];
	bool found;

	// Priority can be used for both fuzzy resource description matching
	// and draw context matching. It can also indicate that a duplicate
	// hash is intentional, since it defines an order between the sections.
	override->priority = GetIniInt(id, L"match_priority", 0, &found);
	if (found)
		override->has_match_priority = true;

	override->stereoMode = GetIniInt(id, L"StereoMode", -1, NULL);
	override->format = GetIniInt(id, L"Format", -1, NULL);
	override->width = GetIniInt(id, L"Width", -1, NULL);
	override->height = GetIniInt(id, L"Height", -1, NULL);
	override->width_multiply = GetIniFloat(id, L"width_multiply", 1.0f, NULL);
	override->height_multiply = GetIniFloat(id, L"height_multiply", 1.0f, NULL);

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

	override->filter_index = GetIniFloat(id, L"filter_index", FLT_MAX, NULL);

	override->expand_region_copy = GetIniBool(id, L"expand_region_copy", false, NULL);
	override->deny_cpu_read = GetIniBool(id, L"deny_cpu_read", false, NULL);

	// Draw call context matching:
	if (GetIniStringAndLog(id, L"match_first_vertex", 0, setting, MAX_PATH)) {
		parse_fuzzy_numeric_match_expression(setting, &override->match_first_vertex);
		override->has_draw_context_match = true;
	}
	if (GetIniStringAndLog(id, L"match_first_index", 0, setting, MAX_PATH)) {
		parse_fuzzy_numeric_match_expression(setting, &override->match_first_index);
		override->has_draw_context_match = true;
	}
	if (GetIniStringAndLog(id, L"match_first_instance", 0, setting, MAX_PATH)) {
		parse_fuzzy_numeric_match_expression(setting, &override->match_first_instance);
		override->has_draw_context_match = true;
	}
	if (GetIniStringAndLog(id, L"match_vertex_count", 0, setting, MAX_PATH)) {
		parse_fuzzy_numeric_match_expression(setting, &override->match_vertex_count);
		override->has_draw_context_match = true;
	}
	if (GetIniStringAndLog(id, L"match_index_count", 0, setting, MAX_PATH)) {
		parse_fuzzy_numeric_match_expression(setting, &override->match_index_count);
		override->has_draw_context_match = true;
	}
	if (GetIniStringAndLog(id, L"match_instance_count", 0, setting, MAX_PATH)) {
		parse_fuzzy_numeric_match_expression(setting, &override->match_instance_count);
		override->has_draw_context_match = true;
	}

	ParseCommandList(id, &override->command_list, &override->post_command_list, TextureOverrideIniKeys, register_command_lists);
}

static bool texture_override_section_has_fuzzy_match_keys(const wchar_t *section)
{
	int i;

	for (i = 0; TextureOverrideFuzzyMatchesIniKeys[i]; i++) {
		if (IniHasKey(section, TextureOverrideFuzzyMatchesIniKeys[i]))
			return true;
	}

	return false;
}

template <class T>
static bool parse_masked_flags_field(const wstring setting, unsigned *val, unsigned *mask,
		struct EnumName_t<const wchar_t *, T> *enum_names)
{
	std::vector<std::wstring> tokens;
	std::wstring token;
	int ret, len1, len2;
	unsigned i;
	bool use_mask = false;
	bool set;
	unsigned tmp;

	// Allow empty strings and 0 to indicate it matches 0 / 0xffffffff:
	if (!setting.size() || !setting.compare(L"0")) {
		*val = 0;
		*mask = 0xffffffff;
		LogInfo("    Using: 0x%08x / 0x%08x\n", *val, *mask);
		return true;
	}

	// Try parsing the field as a hex string with an optional mask:
	ret = swscanf_s(setting.c_str(), L"0x%x%n / 0x%x%n", val, &len1, mask, &len2);
	if (ret != 0 && ret != EOF && (len1 == setting.length() || len2 == setting.length())) {
		if (ret == 2)
			*mask = 0xffffffff;
		LogInfo("    Using: 0x%08x / 0x%08x\n", *val, *mask);
		return true;
	}

	tokens = split_string(&setting, L' ');
	*val = 0;
	*mask = 0;

	for (i = 0; i < tokens.size(); i++) {
		if (tokens[i][0] == L'+') {
			token = tokens[i].substr(1);
			use_mask = true;
			set = true;
		} else if (tokens[i][0] == L'-') {
			token = tokens[i].substr(1);
			use_mask = true;
			set = false;
		} else {
			token = tokens[i];
			set = true;
		}

		tmp = (unsigned)lookup_enum_val<const wchar_t*, T>
			(enum_names, token.c_str(), (T)0);

		if (!tmp) {
			IniWarning("WARNING: Invalid flag %S\n", token.c_str());
			return false;
		}

		if ((*mask & tmp) == tmp) {
			IniWarning("WARNING: Duplicate flag %S\n", token.c_str());
			return false;
		}

		*mask |= tmp;
		if (set)
			*val |= tmp;
	}

	if (!use_mask)
		*mask = 0xffffffff;
	LogInfo("    Using: 0x%08x / 0x%08x\n", *val, *mask);

	return true;
}

static void parse_texture_override_fuzzy_match(const wchar_t *section)
{
	FuzzyMatchResourceDesc *fuzzy;
	wchar_t setting[MAX_PATH];
	bool found;
	int ival;

	fuzzy = new FuzzyMatchResourceDesc(section);

	ival = GetIniEnum(section, L"match_type",
			D3D11_RESOURCE_DIMENSION_UNKNOWN, &found,
			L"D3D11_RESOURCE_DIMENSION_", ResourceDimensions,
			ARRAYSIZE(ResourceDimensions), 1);
	fuzzy->set_resource_type((D3D11_RESOURCE_DIMENSION)ival);

	// We always use match_usage=default if it is not explicitly specified,
	// since forcing the stereo mode doesn't make much sense for other
	// usage types and forcing immutable resources to mono/stereo is
	// suspected, though not confirmed of possibly contributing to some
	// driver crashes, and this shouldn't hurt if that is not the case:
	// https://forums.geforce.com/default/topic/1029242/3d-vision/mass-effect-andromeda-100-plus-10-3d-vision-ready-fix/post/5279617/#5279617
	//
	// If someone needs to match a different usage type they can always
	// explicitly specify it, or match by hash.
	ival = GetIniEnum(section, L"match_usage",
			D3D11_USAGE_DEFAULT, &found, L"D3D11_USAGE_",
			ResourceUsage, ARRAYSIZE(ResourceUsage), 0);
	fuzzy->Usage.op = FuzzyMatchOp::EQUAL;
	fuzzy->Usage.val = ival;

	// Flags
	if (GetIniStringAndLog(section, L"match_bind_flags", 0, setting, MAX_PATH)) {
		if (parse_masked_flags_field(setting, &fuzzy->BindFlags.val, &fuzzy->BindFlags.mask, CustomResourceBindFlagNames)) {
			fuzzy->BindFlags.op = FuzzyMatchOp::EQUAL;
		}
	}
	if (GetIniStringAndLog(section, L"match_cpu_access_flags", 0, setting, MAX_PATH)) {
		if (parse_masked_flags_field(setting, &fuzzy->CPUAccessFlags.val, &fuzzy->CPUAccessFlags.mask, ResourceCPUAccessFlagNames)) {
			fuzzy->CPUAccessFlags.op = FuzzyMatchOp::EQUAL;
		}
	}
	if (GetIniStringAndLog(section, L"match_misc_flags", 0, setting, MAX_PATH)) {
		if (parse_masked_flags_field(setting, &fuzzy->MiscFlags.val, &fuzzy->MiscFlags.mask, ResourceMiscFlagNames)) {
			fuzzy->MiscFlags.op = FuzzyMatchOp::EQUAL;
		}
	}

	// Format string
	if (GetIniStringAndLog(section, L"match_format", 0, setting, MAX_PATH)) {
		fuzzy->Format.val = ParseFormatString(setting, true);
		if (fuzzy->Format.val == (DXGI_FORMAT)-1)
			IniWarning("WARNING: Unknown format \"%S\"\n", setting);
		else
			fuzzy->Format.op = FuzzyMatchOp::EQUAL;
	}

	// Simple numeric expressions:
	if (GetIniStringAndLog(section, L"match_byte_width", 0, setting, MAX_PATH))
		parse_fuzzy_numeric_match_expression(setting, &fuzzy->ByteWidth);
	if (GetIniStringAndLog(section, L"match_stride", 0, setting, MAX_PATH))
		parse_fuzzy_numeric_match_expression(setting, &fuzzy->StructureByteStride);
	if (GetIniStringAndLog(section, L"match_mips", 0, setting, MAX_PATH))
		parse_fuzzy_numeric_match_expression(setting, &fuzzy->MipLevels);
	if (GetIniStringAndLog(section, L"match_width", 0, setting, MAX_PATH))
		parse_fuzzy_numeric_match_expression(setting, &fuzzy->Width);
	if (GetIniStringAndLog(section, L"match_height", 0, setting, MAX_PATH))
		parse_fuzzy_numeric_match_expression(setting, &fuzzy->Height);
	if (GetIniStringAndLog(section, L"match_depth", 0, setting, MAX_PATH))
		parse_fuzzy_numeric_match_expression(setting, &fuzzy->Depth);
	if (GetIniStringAndLog(section, L"match_array", 0, setting, MAX_PATH))
		parse_fuzzy_numeric_match_expression(setting, &fuzzy->ArraySize);
	if (GetIniStringAndLog(section, L"match_msaa", 0, setting, MAX_PATH))
		parse_fuzzy_numeric_match_expression(setting, &fuzzy->SampleDesc_Count);
	if (GetIniStringAndLog(section, L"match_msaa_quality", 0, setting, MAX_PATH))
		parse_fuzzy_numeric_match_expression(setting, &fuzzy->SampleDesc_Quality);

	if (!fuzzy->update_types_matched()) {
		IniWarning("WARNING: [%S] can never match any resources\n", section);
		delete fuzzy;
		return;
	}

	parse_texture_override_common(section, fuzzy->texture_override, true);

	if (!G->mFuzzyTextureOverrides.insert(std::shared_ptr<FuzzyMatchResourceDesc>(fuzzy)).second) {
		IniWarning("BUG: Unexpected error inserting fuzzy texture override\n");
		DoubleBeepExit();
	}
}

static void warn_if_duplicate_texture_hash(TextureOverride *override, uint32_t hash)
{
	TextureOverrideMap::iterator i;
	TextureOverrideList::iterator j;

	if (override->has_draw_context_match || override->has_match_priority)
		return;

	i = lookup_textureoverride(hash);
	if (i == G->mTextureOverrideMap.end())
		return;

	for (j = i->second.begin(); j != i->second.end(); j++) {
		if (&(*j) == override)
			continue;

		// Duplicate hashes are permitted (or at least not warned about) for:
		// 1. Fuzzy resource description matching (no hash - will have bailed above)
		// 2. Draw context matching
		// 3. Whenever a match_priority has been specified
		if (j->has_draw_context_match || j->has_match_priority)
			continue;

		IniWarning("WARNING: Possible Mod Conflict: Duplicate TextureOverride hash=%08lx\n"
			   "[%S]\n"
			   "[%S]\n"
			   "If this is intentional, add a match_priority=n to suppress warning and disambiguate order\n",
			   hash, j->ini_section.c_str(), override->ini_section.c_str());
	}
}

static void ParseTextureOverrideSections()
{
	IniSections::iterator lower, upper, i;
	const wchar_t *id;
	TextureOverride *override;
	uint32_t hash;
	bool found;

	// Lock entire routine, this can be re-inited.  These shaderoverrides
	// are unlikely to be changing much, but for consistency.
	//  We actually already lock the entire config reload, so this is redundant -DSS
	EnterCriticalSectionPretty(&G->mCriticalSection);

	G->mTextureOverrideMap.clear();
	G->mFuzzyTextureOverrides.clear();

	lower = ini_sections.lower_bound(wstring(L"TextureOverride"));
	upper = prefix_upper_bound(ini_sections, wstring(L"TextureOverride"));

	for (i = lower; i != upper; i++) {
		id = i->first.c_str();

		LogInfo("[%S]\n", id);

		hash = (uint32_t)GetIniHash(id, L"Hash", 0, &found);
		if (!found) {
			if (texture_override_section_has_fuzzy_match_keys(id)) {
				parse_texture_override_fuzzy_match(id);
				continue;
			}

			IniWarning("WARNING: [%S] missing Hash= or valid match options\n", id);
			continue;
		}

		if (texture_override_section_has_fuzzy_match_keys(id))
			IniWarning("WARNING: [%S] Cannot use hash= and match options together!\n", id);

		G->mTextureOverrideMap[hash].emplace_back(); // C++ gotcha: invalidates pointers into the vector
		override = &G->mTextureOverrideMap[hash].back();
		override->ini_section = id;

		// Important that we do *not* register the command lists yet:
		parse_texture_override_common(id, override, false);

		// Warn if same hash is used two or more times in sections that
		// do not have a draw context match or match_priority:
		warn_if_duplicate_texture_hash(override, hash);
	}

	for (auto &tolkv : G->mTextureOverrideMap) {
		// Sort the TextureOverride sections sharing the same hash to
		// ensure we get consistent results when processing them.
		// TextureOverrideLess will sort by priority first and ini
		// section name second. We can't use a std::set to keep this
		// sorted, because std::set makes it const, but the
		// TextureOverride will be mutated later and that just becomes
		// a horrible mess. We could do a more efficient insertion
		// sort, but given this cost is only paid on launch and config
		// reload I'd rather keep the sorting down here at the end:
		std::sort(tolkv.second.begin(), tolkv.second.end(), TextureOverrideLess);

		// We cannot register the non-fuzzy TextureOverride command
		// lists automatically when parsing them like we do for other
		// command lists, because the command lists will move around in
		// memory as more TextureOverride sections are added to the
		// vector, and again when the vector is sorted... Thanks C++
		//
		// Might be worthwhile considering changing the data structure
		// to hold pointers so it can rearrange the pointers however it
		// likes without changing the TextureOverrides they point to,
		// similar to how the CommandList data structures work.
		for (TextureOverride &to : tolkv.second) {
			registered_command_lists.push_back(&to.command_list);
			registered_command_lists.push_back(&to.post_command_list);
		}
	}

	LeaveCriticalSection(&G->mCriticalSection);
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
		IniWarning("WARNING: Unrecognised %S=%S\n", key, val);
		return;
	}

	try {
		*op = (D3D11_BLEND_OP)ParseEnum(op_buf, L"D3D11_BLEND_OP_", BlendOPs, ARRAYSIZE(BlendOPs), 1);
	} catch (EnumParseError) {
		IniWarning("WARNING: Unrecognised blend operation %S\n", op_buf);
	}

	try {
		*src = (D3D11_BLEND)ParseEnum(src_buf, L"D3D11_BLEND_", BlendFactors, ARRAYSIZE(BlendFactors), 1);
	} catch (EnumParseError) {
		IniWarning("WARNING: Unrecognised blend source factor %S\n", src_buf);
	}

	try {
		*dst = (D3D11_BLEND)ParseEnum(dst_buf, L"D3D11_BLEND_", BlendFactors, ARRAYSIZE(BlendFactors), 1);
	} catch (EnumParseError) {
		IniWarning("WARNING: Unrecognised blend destination factor %S\n", dst_buf);
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
	bool found;

	wcscpy(key, L"blend");
	if (index >= 0)
		swprintf_s(key, ARRAYSIZE(key), L"blend[%i]", index);
	if (GetIniStringAndLog(section, key, 0, setting, MAX_PATH)) {
		override = true;

		// Special value to disable blending:
		if (!_wcsicmp(setting, L"disable")) {
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
	if (GetIniStringAndLog(section, key, 0, setting, MAX_PATH)) {
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
	desc->RenderTargetWriteMask = GetIniHexString(section, key, D3D11_COLOR_WRITE_ENABLE_ALL, &found);
	if (found) {
		override = true;
		mask->RenderTargetWriteMask = 0;
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

	shader->blend_sample_mask = GetIniHexString(section, L"sample_mask", 0xffffffff, &found);
	if (found) {
		shader->blend_override = 1;
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
		IniWarning("WARNING: Unrecognised %S=%S\n", key, val);
		return;
	}

	try {
		desc->StencilFunc = (D3D11_COMPARISON_FUNC)ParseEnum(func_buf, L"D3D11_COMPARISON_", ComparisonFuncs, ARRAYSIZE(ComparisonFuncs), 1);
	} catch (EnumParseError) {
		IniWarning("WARNING: Unrecognised stencil function %S\n", func_buf);
	}

	try {
		desc->StencilPassOp = (D3D11_STENCIL_OP)ParseEnum(both_pass_buf, L"D3D11_STENCIL_OP_", StencilOps, ARRAYSIZE(StencilOps), 1);
	} catch (EnumParseError) {
		IniWarning("WARNING: Unrecognised stencil + depth pass operation %S\n", both_pass_buf);
	}

	try {
		desc->StencilDepthFailOp = (D3D11_STENCIL_OP)ParseEnum(depth_fail_buf, L"D3D11_STENCIL_OP_", StencilOps, ARRAYSIZE(StencilOps), 1);
	} catch (EnumParseError) {
		IniWarning("WARNING: Unrecognised stencil pass / depth fail operation %S\n", depth_fail_buf);
	}

	try {
		desc->StencilFailOp = (D3D11_STENCIL_OP)ParseEnum(stencil_fail_buf, L"D3D11_STENCIL_OP_", StencilOps, ARRAYSIZE(StencilOps), 1);
	} catch (EnumParseError) {
		IniWarning("WARNING: Unrecognised stencil fail operation %S\n", stencil_fail_buf);
	}
}

static void ParseDepthStencilState(CustomShader *shader, const wchar_t *section)
{
	D3D11_DEPTH_STENCIL_DESC *desc = &shader->depth_stencil_desc;
	D3D11_DEPTH_STENCIL_DESC *mask = &shader->depth_stencil_mask;
	wchar_t setting[MAX_PATH];
	wchar_t key[32];
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

	desc->StencilReadMask = GetIniHexString(section, L"stencil_read_mask", D3D11_DEFAULT_STENCIL_READ_MASK, &found);
	if (found) {
		shader->depth_stencil_override = 1;
		mask->StencilReadMask = 0;
	}

	desc->StencilWriteMask = GetIniHexString(section, L"stencil_write_mask", D3D11_DEFAULT_STENCIL_WRITE_MASK, &found);
	if (found) {
		shader->depth_stencil_override = 1;
		mask->StencilWriteMask = 0;
	}

	if (GetIniStringAndLog(section, L"stencil_front", 0, setting, MAX_PATH)) {
		shader->depth_stencil_override = 1;
		ParseStencilOp(key, setting, &desc->FrontFace);
		memset(&mask->FrontFace, 0, sizeof(D3D11_DEPTH_STENCILOP_DESC));
	}

	if (GetIniStringAndLog(section, L"stencil_back", 0, setting, MAX_PATH)) {
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

	if (!GetIniStringAndLog(section, L"topology", 0, val, MAX_PATH))
		return;

	prefix_len = wcslen(prefix);
	ptr = val;
	if (!_wcsnicmp(ptr, prefix, prefix_len))
		ptr += prefix_len;


	for (i = 1; i < ARRAYSIZE(PrimitiveTopologies); i++) {
		if (!_wcsicmp(ptr, PrimitiveTopologies[i].name)) {
			shader->topology = (D3D11_PRIMITIVE_TOPOLOGY)PrimitiveTopologies[i].val;
			return;
		}

	}

	IniWarning("WARNING: Unrecognised primitive topology=%S\n", val);
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

	if (GetIniStringAndLog(section, L"sampler", 0, setting, MAX_PATH))
	{
		if (!_wcsicmp(setting, L"null"))
			return;

		if (!_wcsicmp(setting, L"point_filter"))
		{
			desc->Filter = D3D11_FILTER_COMPARISON_MIN_LINEAR_MAG_MIP_POINT;
			shader->sampler_override = 1;
			return;
		}

		if (!_wcsicmp(setting, L"linear_filter"))
		{
			desc->Filter = D3D11_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR;
			shader->sampler_override = 1;
			return;
		}

		if (!_wcsicmp(setting, L"anisotropic_filter"))
		{
			desc->Filter = D3D11_FILTER_COMPARISON_ANISOTROPIC;
			desc->MaxAnisotropy = 16; // TODO: is 16 necessary or maybe it should be provided by the config ini?
			shader->sampler_override = 1;
			return;
		}

		IniWarning("WARNING: Unknown sampler \"%S\"\n", setting);
	}
}


// List of keys in [CustomShader] sections that are processed in this
// function. Used by ParseCommandList to find any unrecognised lines.
wchar_t *CustomShaderIniKeys[] = {
	L"vs", L"hs", L"ds", L"gs", L"ps", L"cs",
	L"max_executions_per_frame", L"flags",
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
static void _EnumerateCustomShaderSections(IniSections::iterator lower, IniSections::iterator upper)
{
	IniSections::iterator i;
	wstring shader_id;

	for (i = lower; i != upper; i++) {
		// Convert section name to lower case so our keys will be
		// consistent in the unordered_map:
		shader_id = i->first;
		std::transform(shader_id.begin(), shader_id.end(), shader_id.begin(), ::towlower);

		// Construct a custom shader in the global list:
		customShaders[shader_id];
	}
}
static void EnumerateCustomShaderSections()
{
	IniSections::iterator lower, upper;

	customShaders.clear();

	lower = ini_sections.lower_bound(wstring(L"BuiltInCustomShader"));
	upper = prefix_upper_bound(ini_sections, wstring(L"BuiltInCustomShader"));
	_EnumerateCustomShaderSections(lower, upper);

	lower = ini_sections.lower_bound(wstring(L"CustomShader"));
	upper = prefix_upper_bound(ini_sections, wstring(L"CustomShader"));
	_EnumerateCustomShaderSections(lower, upper);
}
static void ParseCustomShaderSections()
{
	CustomShaders::iterator i;
	const wstring *shader_id;
	CustomShader *custom_shader;
	wchar_t setting[MAX_PATH];
	bool failed;
	wstring namespace_path;

	for (i = customShaders.begin(); i != customShaders.end(); i++) {
		shader_id = &i->first;
		custom_shader = &i->second;

		// FIXME: This will be logged in lower case. It would be better
		// to use the original case, but not a big deal:
		LogInfoW(L"[%s]\n", shader_id->c_str());

		failed = false;

		// Flags is currently just applied to every shader in the chain
		// because it's so rarely needed and it doesn't really matter.
		// We can add vs_flags and so on later if we really need to.
		if (GetIniStringAndLog(shader_id->c_str(), L"flags", 0, setting, MAX_PATH)) {
			custom_shader->compile_flags = parse_enum_option_string<const wchar_t *, D3DCompileFlags, wchar_t*>
				(D3DCompileFlagNames, setting, NULL);
		}

		get_namespaced_section_path(i->first.c_str(), &namespace_path);

		if (GetIniString(shader_id->c_str(), L"vs", 0, setting, MAX_PATH))
			failed |= custom_shader->compile('v', setting, shader_id, &namespace_path);
		if (GetIniString(shader_id->c_str(), L"hs", 0, setting, MAX_PATH))
			failed |= custom_shader->compile('h', setting, shader_id, &namespace_path);
		if (GetIniString(shader_id->c_str(), L"ds", 0, setting, MAX_PATH))
			failed |= custom_shader->compile('d', setting, shader_id, &namespace_path);
		if (GetIniString(shader_id->c_str(), L"gs", 0, setting, MAX_PATH))
			failed |= custom_shader->compile('g', setting, shader_id, &namespace_path);
		if (GetIniString(shader_id->c_str(), L"ps", 0, setting, MAX_PATH))
			failed |= custom_shader->compile('p', setting, shader_id, &namespace_path);
		if (GetIniString(shader_id->c_str(), L"cs", 0, setting, MAX_PATH))
			failed |= custom_shader->compile('c', setting, shader_id, &namespace_path);

		if (failed) {
			// Don't want to allow a shader to be run if it had an
			// error since we are likely to call Draw or Dispatch.
			// We used to erase this from the customShaders map, but
			// now that the command list in [Constants] is parsed
			// first there could still be a pointer to the erased
			// section. Just skip further processing so the command
			// list in this section is empty, and it will be
			// removed during the optimise_command_lists() call.
			IniWarningBeep();
			continue;
		}

		ParseBlendState(custom_shader, shader_id->c_str());
		ParseDepthStencilState(custom_shader, shader_id->c_str());
		ParseRSState(custom_shader, shader_id->c_str());
		ParseTopology(custom_shader, shader_id->c_str());
		ParseSamplerState(custom_shader, shader_id->c_str());

		custom_shader->max_executions_per_frame =
			GetIniInt(shader_id->c_str(), L"max_executions_per_frame", 0, NULL);

		ParseCommandList(shader_id->c_str(), &custom_shader->command_list, &custom_shader->post_command_list, CustomShaderIniKeys);
	}
}

// "Explicit" means that this parses command lists sections that are
// *explicitly* called [CommandList*], as opposed to other sections that are
// implicitly command lists (such as ShaderOverride, Present, etc).
static void _EnumerateExplicitCommandListSections(IniSections::iterator lower, IniSections::iterator upper)
{
	IniSections::iterator i;
	wstring section_id;

	for (i = lower; i != upper; i++) {
		// Convert section name to lower case so our keys will be
		// consistent in the unordered_map:
		section_id = i->first;
		std::transform(section_id.begin(), section_id.end(), section_id.begin(), ::towlower);

		// Construct an explicit command list section in the global list:
		explicitCommandListSections[section_id];
	}
}
static void EnumerateExplicitCommandListSections()
{
	IniSections::iterator lower, upper;

	explicitCommandListSections.clear();

	lower = ini_sections.lower_bound(wstring(L"BuiltInCommandList"));
	upper = prefix_upper_bound(ini_sections, wstring(L"BuiltInCommandList"));
	_EnumerateExplicitCommandListSections(lower, upper);

	lower = ini_sections.lower_bound(wstring(L"CommandList"));
	upper = prefix_upper_bound(ini_sections, wstring(L"CommandList"));
	_EnumerateExplicitCommandListSections(lower, upper);
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

	// We defer wiping the user config (if requested) until the reload in
	// case something marks the user config as dirty between now and then:
	G->gWipeUserConfig = !!private_data;
}

static void ToggleFullScreen(HackerDevice *device, void *private_data)
{
	// SCREEN_FULLSCREEN has several options now, so to preserve the
	// current setting when toggled off we negate it:
	G->SCREEN_FULLSCREEN = -G->SCREEN_FULLSCREEN;
	LogInfo("> full screen forcing toggled to %d (will not take effect until next mode switch)\n", G->SCREEN_FULLSCREEN);
}

static void ForceFullScreen(HackerDevice *device, void *private_data)
{
	HackerSwapChain *mHackerSwapChain = device->GetHackerSwapChain();
	IDXGISwapChain1 *swap_chain;

	LogInfo("> Switching to exclusive full screen mode\n");

	if (!mHackerSwapChain) {
		LogOverlay(LOG_DIRE, "force_full_screen_on_key: Unable to find swap chain\n");
		return;
	}

	swap_chain = mHackerSwapChain->GetOrigSwapChain1();

	swap_chain->SetFullscreenState(TRUE, NULL);
	swap_chain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH);
}

static void warn_of_conflicting_d3dx(wchar_t *dll_ini_path)
{
	wchar_t exe_ini_path[MAX_PATH];
	DWORD attrib;

	if (!GetModuleFileName(NULL, exe_ini_path, MAX_PATH))
		return;
	wcsrchr(exe_ini_path, L'\\')[1] = 0;
	wcscat(exe_ini_path, L"d3dx.ini");

	if (!wcscmp(dll_ini_path, exe_ini_path))
		return;

	attrib = GetFileAttributes(exe_ini_path);
	if (attrib == INVALID_FILE_ATTRIBUTES)
		return;

	LogOverlay(LOG_WARNING, "Detected a conflicting d3dx.ini in the game directory that is not being used.\n"
			"Using this configuration: %S\n", dll_ini_path);
}

void LoadConfigFile()
{
	wchar_t iniFile[MAX_PATH], logFilename[MAX_PATH];
	wchar_t setting[MAX_PATH];

	G->gInitialized = true;

	if (!GetModuleFileName(migoto_handle, iniFile, MAX_PATH))
		DoubleBeepExit();
	wcsrchr(iniFile, L'\\')[1] = 0;
	wcscpy(logFilename, iniFile);
	wcscat(iniFile, INI_FILENAME);
	wcscat(logFilename, L"d3d11_log.txt");
	warn_of_conflicting_d3dx(iniFile);

	// Log all settings that are _enabled_, in order, 
	// so that there is no question what settings we are using.

	// [Logging]
	// Not using the helper function for this one since logging isn't enabled yet
	if (GetPrivateProfileInt(L"Logging", L"calls", 1, iniFile))
	{
		if (!LogFile)
			LogFile = _wfsopen(logFilename, L"w", _SH_DENYNO);
		LogInfo("\nD3D11 DLL starting init - v %s - %s\n", VER_FILE_VERSION_STR, LogTime().c_str());

		wchar_t our_path[MAX_PATH], exe_path[MAX_PATH];
		GetModuleFileName(migoto_handle, our_path, MAX_PATH);
		GetModuleFileName(NULL, exe_path, MAX_PATH);
		LogInfo("Game path: %S\n"
			"3DMigoto path: %S\n\n",
			exe_path, our_path);

		LogInfoW(L"----------- " INI_FILENAME L" settings -----------\n");
	}
	LogInfo("[Logging]\n");
	LogInfo("  calls=1\n");

	ParseIniFile(iniFile);
	InsertBuiltInIniSections();

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

	debugger = GetIniInt(L"Logging", L"crash", false, NULL);
	if (debugger)
		install_crash_handler(debugger);

	G->dump_all_profiles = GetIniBool(L"Logging", L"dump_all_profiles", false, NULL);

	if (GetIniBool(L"Logging", L"debug_locks", false, NULL))
		enable_lock_dependency_checks();

	// [Include]
	ParseIncludedIniFiles();

	// [System]
	LogInfo("[System]\n");
	GetIniStringAndLog(L"System", L"proxy_d3d11", 0, G->CHAIN_DLL_PATH, MAX_PATH);	
	G->load_library_redirect = GetIniInt(L"System", L"load_library_redirect", 2, NULL);

	if (GetIniStringAndLog(L"System", L"hook", 0, setting, MAX_PATH))
	{
		G->enable_hooks = parse_enum_option_string<wchar_t *, EnableHooks>
			(EnableHooksNames, setting, NULL);

		if (G->enable_hooks & EnableHooks::DEPRECATED)
			LogOverlay(LOG_NOTICE, "Deprecated hook options: Please remove \"except\" and \"skip\" options\n");
	}
	G->enable_check_interface = GetIniBool(L"System", L"allow_check_interface", false, NULL);
	G->enable_create_device = GetIniInt(L"System", L"allow_create_device", 0, NULL);
	G->enable_platform_update = GetIniBool(L"System", L"allow_platform_update", false, NULL);
	// TODO: Enable this by default if wider testing goes well:
	G->check_foreground_window = GetIniBool(L"System", L"check_foreground_window", false, NULL);

	// [Device] (DXGI parameters)
	LogInfo("[Device]\n");
	G->SCREEN_WIDTH = GetIniInt(L"Device", L"width", -1, NULL);
	G->SCREEN_HEIGHT = GetIniInt(L"Device", L"height", -1, NULL);
	G->SCREEN_REFRESH = GetIniInt(L"Device", L"refresh_rate", -1, NULL);
	G->SCREEN_UPSCALING = GetIniInt(L"Device", L"upscaling", 0, NULL);
	G->UPSCALE_MODE = GetIniInt(L"Device", L"upscale_mode", 0, NULL);

	if (GetIniStringAndLog(L"Device", L"filter_refresh_rate", 0, setting, MAX_PATH))
	{
		swscanf_s(setting, L"%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
			G->FILTER_REFRESH + 0, G->FILTER_REFRESH + 1, G->FILTER_REFRESH + 2, G->FILTER_REFRESH + 3, 
			G->FILTER_REFRESH + 4, G->FILTER_REFRESH + 5, G->FILTER_REFRESH + 6, G->FILTER_REFRESH + 7, 
			G->FILTER_REFRESH + 8, G->FILTER_REFRESH + 9);
	}

	G->SCREEN_FULLSCREEN = GetIniInt(L"Device", L"full_screen", -1, NULL);
	RegisterIniKeyBinding(L"Device", L"toggle_full_screen", ToggleFullScreen, NULL, 0, NULL);
	RegisterIniKeyBinding(L"Device", L"force_full_screen_on_key", ForceFullScreen, NULL, 0, NULL);
	G->gForceStereo = GetIniInt(L"Device", L"force_stereo", 0, NULL);
	G->SCREEN_ALLOW_COMMANDS = GetIniBool(L"Device", L"allow_windowcommands", false, NULL);

	G->mResolutionInfo.from = GetIniEnumClass(L"Device", L"get_resolution_from", GetResolutionFrom::INVALID, NULL, GetResolutionFromNames);

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

	G->shader_hash_type = GetIniEnumClass(L"Rendering", L"shader_hash", ShaderHashType::FNV, NULL, ShaderHashNames);
	G->texture_hash_version = GetIniInt(L"Rendering", L"texture_hash", 0, NULL);

	if (GetIniStringAndLog(L"Rendering", L"override_directory", 0, G->SHADER_PATH, MAX_PATH))
	{
		while (G->SHADER_PATH[wcslen(G->SHADER_PATH) - 1] == L' ')
			G->SHADER_PATH[wcslen(G->SHADER_PATH) - 1] = 0;
		if (G->SHADER_PATH[1] != ':' && G->SHADER_PATH[0] != '\\')
		{
			GetModuleFileName(migoto_handle, setting, MAX_PATH);
			wcsrchr(setting, L'\\')[1] = 0;
			wcscat(setting, G->SHADER_PATH);
			wcscpy(G->SHADER_PATH, setting);
		}
		// Create directory?
		CreateDirectoryEnsuringAccess(G->SHADER_PATH);
	}
	if (GetIniStringAndLog(L"Rendering", L"cache_directory", 0, G->SHADER_CACHE_PATH, MAX_PATH))
	{
		while (G->SHADER_CACHE_PATH[wcslen(G->SHADER_CACHE_PATH) - 1] == L' ')
			G->SHADER_CACHE_PATH[wcslen(G->SHADER_CACHE_PATH) - 1] = 0;
		if (G->SHADER_CACHE_PATH[1] != ':' && G->SHADER_CACHE_PATH[0] != '\\')
		{
			GetModuleFileName(migoto_handle, setting, MAX_PATH);
			wcsrchr(setting, L'\\')[1] = 0;
			wcscat(setting, G->SHADER_CACHE_PATH);
			wcscpy(G->SHADER_CACHE_PATH, setting);
		}
		// Create directory?
		CreateDirectoryEnsuringAccess(G->SHADER_CACHE_PATH);
	}

	G->CACHE_SHADERS = GetIniBool(L"Rendering", L"cache_shaders", false, NULL);
	G->SCISSOR_DISABLE = GetIniBool(L"Rendering", L"rasterizer_disable_scissor", false, NULL);
	G->track_texture_updates = GetIniBoolOrInt(L"Rendering", L"track_texture_updates", 0, NULL);
	G->assemble_signature_comments = GetIniBool(L"Rendering", L"assemble_signature_comments", false, NULL);
	G->disassemble_undecipherable_custom_data = GetIniBool(L"Rendering", L"disassemble_undecipherable_custom_data", false, NULL);
	G->patch_cb_offsets = GetIniBool(L"Rendering", L"patch_assembly_cb_offsets", false, NULL);
	G->recursive_include = GetIniBoolOrInt(L"Rendering", L"recursive_include", false, NULL);

	G->EXPORT_FIXED = GetIniBool(L"Rendering", L"export_fixed", false, NULL);
	G->EXPORT_SHADERS = GetIniBool(L"Rendering", L"export_shaders", false, NULL);
	G->EXPORT_HLSL = GetIniInt(L"Rendering", L"export_hlsl", 0, NULL);
	G->EXPORT_BINARY = GetIniBool(L"Rendering", L"export_binary", false, NULL);
	G->DumpUsage = GetIniBool(L"Rendering", L"dump_usage", false, NULL);

	G->StereoParamsReg = GetIniInt(L"Rendering", L"stereo_params", 125, NULL);
	G->IniParamsReg = GetIniInt(L"Rendering", L"ini_params", 120, NULL);
	G->decompiler_settings.StereoParamsReg = G->StereoParamsReg;
	G->decompiler_settings.IniParamsReg = G->IniParamsReg;
	if (G->StereoParamsReg >= D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT) {
		IniWarning("WARNING: stereo_params=%i out of range\n", G->StereoParamsReg);
		G->StereoParamsReg = -1;
	}
	if (G->IniParamsReg >= D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT) {
		IniWarning("WARNING: ini_params=%i out of range\n", G->IniParamsReg);
		G->IniParamsReg = -1;
	}


	// Automatic section
	G->decompiler_settings.fixSvPosition = GetIniBool(L"Rendering", L"fix_sv_position", false, NULL);
	G->decompiler_settings.recompileVs = GetIniBool(L"Rendering", L"recompile_all_vs", false, NULL);
	if (GetIniStringAndLog(L"Rendering", L"fix_ZRepair_DepthTexture1", 0, setting, MAX_PATH))
	{
		char buf[MAX_PATH];
		wcstombs(buf, setting, MAX_PATH);
		char *end = RightStripA(buf);
		G->decompiler_settings.ZRepair_DepthTextureReg1 = *end; *(end - 1) = 0;
		char *start = buf; while (isspace(*start)) start++;
		G->decompiler_settings.ZRepair_DepthTexture1 = start;
	}
	if (GetIniStringAndLog(L"Rendering", L"fix_ZRepair_DepthTexture2", 0, setting, MAX_PATH))
	{
		char buf[MAX_PATH];
		wcstombs(buf, setting, MAX_PATH);
		char *end = RightStripA(buf);
		G->decompiler_settings.ZRepair_DepthTextureReg2 = *end; *(end - 1) = 0;
		char *start = buf; while (isspace(*start)) start++;
		G->decompiler_settings.ZRepair_DepthTexture2 = start;
	}
	if (GetIniStringAndLog(L"Rendering", L"fix_ZRepair_ZPosCalc1", 0, setting, MAX_PATH))
		G->decompiler_settings.ZRepair_ZPosCalc1 = readStringParameter(setting);
	if (GetIniStringAndLog(L"Rendering", L"fix_ZRepair_ZPosCalc2", 0, setting, MAX_PATH))
		G->decompiler_settings.ZRepair_ZPosCalc2 = readStringParameter(setting);
	if (GetIniStringAndLog(L"Rendering", L"fix_ZRepair_PositionTexture", 0, setting, MAX_PATH))
		G->decompiler_settings.ZRepair_PositionTexture = readStringParameter(setting);
	if (GetIniStringAndLog(L"Rendering", L"fix_ZRepair_PositionCalc", 0, setting, MAX_PATH))
		G->decompiler_settings.ZRepair_WorldPosCalc = readStringParameter(setting);
	if (GetIniStringAndLog(L"Rendering", L"fix_ZRepair_Dependencies1", 0, setting, MAX_PATH))
	{
		char buf[MAX_PATH];
		wcstombs(buf, setting, MAX_PATH);
		char *start = buf; while (isspace(*start)) ++start;
		while (*start)
		{
			char *end = start; while (*end != ',' && *end && *end != ' ') ++end;
			G->decompiler_settings.ZRepair_Dependencies1.push_back(string(start, end));
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
			G->decompiler_settings.ZRepair_Dependencies2.push_back(string(start, end));
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
			G->decompiler_settings.InvTransforms.push_back(string(start, end));
			start = end; if (*start == ',') ++start;
		}
	}
	if (GetIniStringAndLog(L"Rendering", L"fix_ZRepair_DepthTextureHash", 0, setting, MAX_PATH))
	{
		uint32_t hash;
		swscanf_s(setting, L"%08lx", &hash);
		G->ZBufferHashToInject = hash;
		G->decompiler_settings.ZRepair_DepthBuffer = !!G->ZBufferHashToInject;
	}
	if (GetIniStringAndLog(L"Rendering", L"fix_BackProjectionTransform1", 0, setting, MAX_PATH))
		G->decompiler_settings.BackProject_Vector1 = readStringParameter(setting);
	if (GetIniStringAndLog(L"Rendering", L"fix_BackProjectionTransform2", 0, setting, MAX_PATH))
		G->decompiler_settings.BackProject_Vector2 = readStringParameter(setting);
	if (GetIniStringAndLog(L"Rendering", L"fix_ObjectPosition1", 0, setting, MAX_PATH))
		G->decompiler_settings.ObjectPos_ID1 = readStringParameter(setting);
	if (GetIniStringAndLog(L"Rendering", L"fix_ObjectPosition2", 0, setting, MAX_PATH))
		G->decompiler_settings.ObjectPos_ID2 = readStringParameter(setting);
	if (GetIniStringAndLog(L"Rendering", L"fix_ObjectPosition1Multiplier", 0, setting, MAX_PATH))
		G->decompiler_settings.ObjectPos_MUL1 = readStringParameter(setting);
	if (GetIniStringAndLog(L"Rendering", L"fix_ObjectPosition2Multiplier", 0, setting, MAX_PATH))
		G->decompiler_settings.ObjectPos_MUL2 = readStringParameter(setting);
	if (GetIniStringAndLog(L"Rendering", L"fix_MatrixOperand1", 0, setting, MAX_PATH))
		G->decompiler_settings.MatrixPos_ID1 = readStringParameter(setting);
	if (GetIniStringAndLog(L"Rendering", L"fix_MatrixOperand1Multiplier", 0, setting, MAX_PATH))
		G->decompiler_settings.MatrixPos_MUL1 = readStringParameter(setting);

	// [Hunting]
	ParseHuntingSection();

	// Must be done prior to parsing any command list sections, as every
	// section registered in this set will be a candidate for optimisation:
	registered_command_lists.clear();
	G->implicit_post_checktextureoverride_used = false;

	// Splitting the enumeration of these sections out from parsing them as
	// they can be referenced from other command list sections, keys and
	// presets (via the run command), including sections of the same type.
	// Most of the other sections don't need this so long as we parse them
	// in an appropriate order so that sections that can be referred to are
	// parsed before sections that can refer to them (e.g. Resource
	// sections are parsed before all command list sections for this
	// reason), but these are special since they can both refer to other
	// sections and be referred to by other sections, and we don't want the
	// parse order to determine if the reference will work or not.
	EnumerateCustomShaderSections();
	EnumerateExplicitCommandListSections();
	// Splitting enumeration of presets out, because [Constants] could
	// theoretically use a preset (however unlikely), but needs to be
	// parsed before presets to allocate any global variables that
	// [Preset]s may refer to:
	EnumeratePresetOverrideSections();

	// Must be done before any command lists that may refer to them:
	ParseResourceSections();

	// This is the only command list we permit to allocate global variables,
	// so we parse it before all other command lists, key bindings and
	// presets that may use those variables.
	ParseConstantsSection();

	// Must be done after [Constants] has allocated global variables:
	RegisterPresetKeyBindings();
	ParsePresetOverrideSections();

	// Used to have to do CustomShaders before other command lists in case
	// any failed and had their sections erased, but no longer matters.
	ParseCustomShaderSections();
	ParseExplicitCommandListSections();

	ParseShaderOverrideSections();
	ParseShaderRegexSections();
	ParseTextureOverrideSections();

	LogInfo("[Present]\n");
	G->present_command_list.clear();
	G->post_present_command_list.clear();
	ParseCommandList(L"Present", &G->present_command_list, &G->post_present_command_list, NULL);

	LogInfo("[ClearRenderTargetView]\n");
	G->clear_rtv_command_list.clear();
	G->post_clear_rtv_command_list.clear();
	ParseCommandList(L"ClearRenderTargetView", &G->clear_rtv_command_list, &G->post_clear_rtv_command_list, NULL);

	LogInfo("[ClearDepthStencilView]\n");
	G->clear_dsv_command_list.clear();
	G->post_clear_dsv_command_list.clear();
	ParseCommandList(L"ClearDepthStencilView", &G->clear_dsv_command_list, &G->post_clear_dsv_command_list, NULL);

	LogInfo("[ClearUnorderedAccessViewUint]\n");
	G->clear_uav_uint_command_list.clear();
	G->post_clear_uav_uint_command_list.clear();
	ParseCommandList(L"ClearUnorderedAccessViewUint", &G->clear_uav_uint_command_list, &G->post_clear_uav_uint_command_list, NULL);

	LogInfo("[ClearUnorderedAccessViewFloat]\n");
	G->clear_uav_float_command_list.clear();
	G->post_clear_uav_float_command_list.clear();
	ParseCommandList(L"ClearUnorderedAccessViewFloat", &G->clear_uav_float_command_list, &G->post_clear_uav_float_command_list, NULL);

	LogInfo("[Profile]\n");
	ParseDriverProfile();

	LogInfo("\n");

	if (G->hide_cursor || G->SCREEN_UPSCALING)
		InstallMouseHooks(G->hide_cursor);

	emit_ini_warning_tone();
}

// This variant is called by the profile manager helper with the path to the
// game's executable passed in. It doesn't need to parse most of the config,
// only the [Profile] section and some of the logging. It uses a separate log
// file from the main DLL.
void LoadProfileManagerConfig(const wchar_t *config_dir)
{
	wchar_t iniFile[MAX_PATH], logFilename[MAX_PATH];

	G->gInitialized = true;

	if (wcscpy_s(iniFile, MAX_PATH, config_dir))
		DoubleBeepExit();
	wcsrchr(iniFile, L'\\')[1] = 0;
	wcscpy(logFilename, iniFile);
	wcscat(iniFile, INI_FILENAME);
	wcscat(logFilename, L"d3d11_profile_log.txt");

	// [Logging]
	// Not using the helper function for this one since logging isn't enabled yet
	if (GetPrivateProfileInt(L"Logging", L"calls", 1, iniFile))
	{
		if (!LogFile)
			LogFile = _wfsopen(logFilename, L"w", _SH_DENYNO);
		LogInfo("\n3DMigoto profile helper starting init - v %s - %s\n\n", VER_FILE_VERSION_STR, LogTime().c_str());
		LogInfoW(L"----------- " INI_FILENAME L" settings -----------\n");
	}
	LogInfo("[Logging]\n");
	LogInfo("  calls=1\n");

	ParseIniFile(iniFile);

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

void SavePersistentSettings()
{
	FILE *f;

	if (!G->user_config_dirty)
		return;
	G->user_config_dirty = 0;

	// TODO: Ability to update existing file rather than overwriting:
	//wfopen_ensuring_access(&f, G->user_config.c_str(), L"r+");
	//if (!f)
	wfopen_ensuring_access(&f, G->user_config.c_str(), L"w");
	if (!f) {
		LogInfo("Unable to save settings in %S\n", G->user_config.c_str());
		return;
	}

	LogInfo("Saving user settings to %S\n", G->user_config.c_str());

	fputs("; AUTOMATICALLY GENERATED FILE - DO NOT EDIT\n"
	      ";\n"
	      "; 3DMigoto will overwrite this file whenever any persistent settings are\n"
	      "; altered by hot key or command list. Tag global variables with the \"persist\"\n"
	      "; keyword to save them in this file. Use the post keyword in the [Constants]\n"
	      "; command list if you need to do any intialisation after this file is loaded.\n"
	      ";\n"
	      "[Constants]\n", f);

	for (auto global : persistent_variables)
		fprintf_s(f, "%S = %.9g\n", global->name.c_str(), global->fval);

	fclose(f);
}

static void WipeUserConfig()
{
	G->gWipeUserConfig = false;
	G->user_config_dirty = 0;

	DeleteFile(G->user_config.c_str());
}

static void MarkAllShadersDeferredUnprocessed()
{
	ShaderReloadMap::iterator i;

	for (i = G->mReloadedShaders.begin(); i != G->mReloadedShaders.end(); i++) {
		// Whenever we reload the config we clear the processed flag on
		// all auto patched shaders to ensure that they will be
		// re-patched using the current patterns in the d3dx.ini. This
		// is separate from the deferred_replacement_candidate flag,
		// which will be set in the shader reload routine for any
		// shaders that have been removed from disk, and removed from
		// any that are loaded from disk:
		i->second.deferred_replacement_processed = false;
	}

	// TODO: If ShaderRegex hash is unchanged leave these shaders in place
	// and just update the ShaderOverrides & filter_index
}

void ReloadConfig(HackerDevice *device)
{
	HackerContext *mHackerContext = device->GetHackerContext();

	if (G->gWipeUserConfig)
		WipeUserConfig();

	SavePersistentSettings();

	LogInfoW(L"Reloading " INI_FILENAME L" (EXPERIMENTAL)...\n");

	G->gReloadConfigPending = false;
	G->iniParamsReserved = 0;

	// Lock the entire config reload as it touches many global structures
	// that could potentially be accessed from other threads (e.g. deferred
	// contexts) while we do this
	EnterCriticalSectionPretty(&G->mCriticalSection);

	// Clears any notices currently displayed on the overlay. This ensures
	// that any notices that haven't timed out yet (e.g. from a previous
	// failed reload attempt) are removed so that the only messages
	// displayed will be relevant to the current reload attempt.
	//
	// The shader reload is separate and will also attempt to clear old
	// notices - ClearNotices() itself will ensure that only the first one
	// of these actually takes effect in the current frame.
	ClearNotices();

	// Clear the key bindings. There may be other things that need to be
	// cleared as well, but for the sake of clarity I'd rather clear as
	// many as possible inside LoadConfigFile() where they are set.
	ClearKeyBindings();

	// Clear active command lists set, as the pointers in this set will
	// become invalid as the config is reloaded:
	command_lists_profiling.clear();
	command_lists_cmd_profiling.clear();

	// Reset the counters on the global parameter save area:
	OverrideSave.Reset(device);

	LoadConfigFile();
	optimise_command_lists(device);

	MarkAllShadersDeferredUnprocessed();

	LeaveCriticalSection(&G->mCriticalSection);

	// Execute the [Constants] command list in the immediate context to
	// initialise iniParams and perform any other custom initialisation the
	// user may have defined:
	if (mHackerContext) {
		if (G->iniParams.size() != G->iniParamsReserved) {
			LogInfo("  Resizing IniParams from %Ii to %d\n", G->iniParams.size(), G->iniParamsReserved);
			device->CreateIniParamResources();
			mHackerContext->Bind3DMigotoResources();
		}

		mHackerContext->InitIniParams();
	} else {
		// We used to use GetImmediateContext here, which would ensure
		// that the HackerContext had been created if it didn't exist
		// for some reason, but that doesn't work in the case of
		// hooking. I'm not positive we actually needed that (and if we
		// did the [Present] command list would also be broken), so
		// rather than continue to use it, issue a warning if the
		// HackerContext doesn't exist.
		LogOverlay(LOG_DIRE, "BUG: No HackerContext at ReloadConfig - please report this\n");
	}

	LogOverlayW(LOG_INFO, L"> " INI_FILENAME L" reloaded\n");
}
