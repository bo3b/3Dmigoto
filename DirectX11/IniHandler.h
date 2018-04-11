#include "HackerDevice.h"

NvAPI_Status CheckStereo();
void FlagConfigReload(HackerDevice *device, void *private_data);
void LoadConfigFile();
void ReloadConfig(HackerDevice *device);
void LoadProfileManagerConfig(const wchar_t *exe_path);

int GetIniInt(const wchar_t *section, const wchar_t *key, int def, bool *found);
bool GetIniBool(const wchar_t *section, const wchar_t *key, bool def, bool *found);
float GetIniFloat(const wchar_t *section, const wchar_t *key, float def, bool *found);
int GetIniString(const wchar_t *section, const wchar_t *key, const wchar_t *def,
		 wchar_t *ret, unsigned size);
int GetIniStringAndLog(const wchar_t *section, const wchar_t *key, const wchar_t *def,
		 wchar_t *ret, unsigned size);

bool get_namespaced_section_name_lower(const wstring *section, const wstring *ini_namespace, wstring *ret);
bool get_section_namespace(const wchar_t *section, wstring *ret);

// These functions will bypass our hooks *if* the option to do so has been enabled:
BOOL WINAPI CursorUpscalingBypass_GetClientRect(_In_ HWND hWnd, _Out_ LPRECT lpRect);
BOOL WINAPI CursorUpscalingBypass_GetCursorInfo(_Inout_ PCURSORINFO pci);
BOOL WINAPI CursorUpscalingBypass_ScreenToClient(_In_ HWND hWnd, LPPOINT lpPoint);

int InstallHook(HINSTANCE module, char *func, void **trampoline, void *hook, bool LogInfo_is_safe);
