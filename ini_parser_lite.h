#pragma once

const char* find_ini_section_lite(const char* buf, const char* section_name);
bool        find_ini_setting_lite(const char* buf, const char* setting, char* ret, size_t n);
bool        find_ini_bool_lite(const char* buf, const char* setting, bool def);
int         find_ini_int_lite(const char* buf, const char* setting, int def);
