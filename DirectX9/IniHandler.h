#pragma once
#include "Main.h"

NvAPI_Status CheckStereo();
void FlagConfigReload(D3D9Wrapper::IDirect3DDevice9 *device, void *private_data);
void LoadConfigFile();
void ReloadConfig(D3D9Wrapper::IDirect3DDevice9 *device);
void LoadProfileManagerConfig(const wchar_t *exe_path);
void SavePersistentSettings();

struct IniLine {
	// Same syntax as std::pair, whitespace stripped around each:
	wstring first;
	wstring second;

	// For when we don't want whitespace around the equals sign stripped,
	// or when there is no equals sign (whitespace at the start and end of
	// the whole line is still stripped):
	wstring raw_line;

	// Namespaced sections can determine the namespace from the section as
	// a whole, but global sections like [Present] can have lines from many
	// different namespaces, so each line stores the namespace it came from
	// to resolve references within the namespace:
	wstring ini_namespace;

	IniLine(wstring &key, wstring &val, wstring &line, const wstring &ini_namespace) :
		first(key),
		second(val),
		raw_line(line),
		ini_namespace(ini_namespace)
	{}
};
// Whereas settings within a section are in the same order they were in the ini
// file. This will become more important as shader overrides gains more
// functionality and dependencies between different features form:
typedef std::vector<IniLine> IniSectionVector;
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
typedef std::unordered_map<wstring, wstring, WStringInsensitiveHash, WStringInsensitiveEquality> IniSectionMap;
struct IniSection {
	IniSectionMap kv_map;
	IniSectionVector kv_vec;


	// Stores the ini namespace/path that this section came from. Note that
	// there is also an ini_namespace in the IniLine structure for global
	// sections where the namespacing can be per-line:
	wstring ini_namespace;
};
struct WStringInsensitiveLess {
	bool operator() (const wstring &x, const wstring &y) const
	{
		return _wcsicmp(x.c_str(), y.c_str()) < 0;
	}
};
typedef std::map<wstring, IniSection, WStringInsensitiveLess> IniSections;
class IniFile
{
public:
	wstring ini_name;
	IniSections ini_sections;
	virtual const wchar_t* SectionPrefix(const wchar_t *section) = 0;
	virtual bool DoesSectionAllowLinesWithoutEquals(const wchar_t *section) { return false; };
	virtual bool IsRegularSection(const wchar_t *section) = 0;
	virtual bool IsCommandListSection(const wchar_t *section) = 0;
};
class MigotoIniFile : public IniFile
{
public:
	const wchar_t* SectionPrefix(const wchar_t *section) override;
	bool DoesSectionAllowLinesWithoutEquals(const wchar_t *section) override;

	bool IsRegularSection(const wchar_t *section) override;
	bool IsCommandListSection(const wchar_t *section) override;
};
class HelixIniFile : public IniFile
{
public:
	const wchar_t* SectionPrefix(const wchar_t *section) override;
	bool IsRegularSection(const wchar_t *section) override;
	bool IsCommandListSection(const wchar_t *section) override;
};
extern MigotoIniFile migoto_ini;
extern HelixIniFile helix_ini;
void GetIniSection(IniSectionVector **key_vals, const wchar_t *section, IniFile *ini);
int GetIniInt(const wchar_t *section, const wchar_t *key, int def, bool *found, IniFile *ini, bool warn = true);
bool GetIniBool(const wchar_t *section, const wchar_t *key, bool def, bool *found, IniFile *ini, bool warn = true);
float GetIniFloat(const wchar_t *section, const wchar_t *key, float def, bool *found, IniFile *ini);
int GetIniString(const wchar_t *section, const wchar_t *key, const wchar_t *def,
	wchar_t *ret, unsigned size, IniFile *ini);
bool GetIniString(const wchar_t *section, const wchar_t *key, const wchar_t *def, string *ret, IniFile *ini);
int GetIniStringAndLog(const wchar_t *section, const wchar_t *key, const wchar_t *def,
	wchar_t *ret, unsigned size, IniFile *ini);
template <class T1, class T2>
T2 GetIniEnumClass(const wchar_t *section, const wchar_t *key, T2 def, bool *found,
	struct EnumName_t<T1, T2> *enum_names, IniFile *ini);

bool get_namespaced_section_name_lower(const wstring *section, const wstring *ini_namespace, wstring *ret, IniFile *ini);
bool get_section_namespace(const wchar_t *section, wstring *ret, IniFile *ini);
wstring get_namespaced_var_name_lower(const wstring var, const wstring *ini_namespace);

// These functions will bypass our hooks *if* the option to do so has been enabled:
BOOL WINAPI CursorUpscalingBypass_GetClientRect(_In_ HWND hWnd, _Out_ LPRECT lpRect);
BOOL WINAPI CursorUpscalingBypass_GetCursorInfo(_Inout_ PCURSORINFO pci);
BOOL WINAPI CursorUpscalingBypass_ScreenToClient(_In_ HWND hWnd, LPPOINT lpPoint);

int InstallHook(HINSTANCE module, char *func, void **trampoline, void *hook, bool LogInfo_is_safe);
