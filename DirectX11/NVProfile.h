#pragma once

#include <nvapi.h>
#include <string>
#include <unordered_map>

typedef std::unordered_map<NvU32, NVDRS_SETTING> ProfileSettings;
extern ProfileSettings profile_settings;

void log_nv_driver_version();
void log_check_and_update_nv_profiles();
int parse_ini_profile_line(std::wstring *lhs, std::wstring *rhs);
