#pragma once

#include <unordered_map>
#include <nvapi.h>
#include <string>

extern std::unordered_map<NvU32, NVDRS_SETTING> profile_settings;

void log_nv_driver_version();
void log_relevant_nv_profiles();
int parse_ini_profile_line(std::wstring *lhs, std::wstring *rhs);
void install_driver_profile();
