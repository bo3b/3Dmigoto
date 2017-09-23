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
