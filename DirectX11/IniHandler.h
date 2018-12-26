#include "HackerDevice.h"

NvAPI_Status CheckStereo();
void FlagConfigReload(HackerDevice *device, void *private_data);
void LoadConfigFile();
void ReloadConfig(HackerDevice *device);
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

void GetIniSection(IniSectionVector **key_vals, const wchar_t *section);
int GetIniInt(const wchar_t *section, const wchar_t *key, int def, bool *found, bool warn=true);
bool GetIniBool(const wchar_t *section, const wchar_t *key, bool def, bool *found, bool warn=true);
float GetIniFloat(const wchar_t *section, const wchar_t *key, float def, bool *found);
int GetIniString(const wchar_t *section, const wchar_t *key, const wchar_t *def,
		 wchar_t *ret, unsigned size);
int GetIniStringAndLog(const wchar_t *section, const wchar_t *key, const wchar_t *def,
		 wchar_t *ret, unsigned size);
template <class T>
T GetIniEnumClass(const wchar_t *section, const wchar_t *key, T def, bool *found,
		struct EnumName_t<const wchar_t *, T> *enum_names);

bool get_namespaced_section_name_lower(const wstring *section, const wstring *ini_namespace, wstring *ret);
bool get_section_namespace(const wchar_t *section, wstring *ret);
wstring get_namespaced_var_name_lower(const wstring var, const wstring *ini_namespace);

// These functions will bypass our hooks *if* the option to do so has been enabled:
BOOL WINAPI CursorUpscalingBypass_GetClientRect(_In_ HWND hWnd, _Out_ LPRECT lpRect);
BOOL WINAPI CursorUpscalingBypass_GetCursorInfo(_Inout_ PCURSORINFO pci);
BOOL WINAPI CursorUpscalingBypass_ScreenToClient(_In_ HWND hWnd, LPPOINT lpPoint);

int InstallHook(HINSTANCE module, char *func, void **trampoline, void *hook, bool LogInfo_is_safe);
