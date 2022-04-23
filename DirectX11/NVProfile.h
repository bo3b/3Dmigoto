#pragma once

#include <d3d11.h>
#include <string>
#include <unordered_map>

// Only include this after d3d11.h so that the pre-processor
// define for __d3d11_h__ will be added.
#include "nvapi.h"

typedef std::unordered_map<NvU32, NVDRS_SETTING> ProfileSettings;
extern ProfileSettings profile_settings;

void log_nv_driver_version();
void log_check_and_update_nv_profiles();
int parse_ini_profile_line(std::wstring *lhs, std::wstring *rhs);
