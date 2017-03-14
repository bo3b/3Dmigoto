#include "HackerDevice.h"

NvAPI_Status CheckStereo();
void FlagConfigReload(HackerDevice *device, void *private_data);
void LoadConfigFile();
void ReloadConfig(HackerDevice *device);
void LoadProfileManagerConfig(const wchar_t *exe_path);
