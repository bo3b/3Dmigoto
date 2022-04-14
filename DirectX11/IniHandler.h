#pragma once

#include "HackerDevice.hpp"

NvAPI_Status check_stereo();
void         flag_config_reload(HackerDevice* device, void* private_data);
void         load_config_file();
void         reload_config(HackerDevice* device);
void         load_profile_manager_config(const wchar_t* config_dir);
void         save_persistent_settings();

struct ini_line
{
    // Same syntax as std::pair, whitespace stripped around each:
    std::wstring first;
    std::wstring second;

    // For when we don't want whitespace around the equals sign stripped,
    // or when there is no equals sign (whitespace at the start and end of
    // the whole line is still stripped):
    std::wstring raw_line;

    // Namespaced sections can determine the namespace from the section as
    // a whole, but global sections like [Present] can have lines from many
    // different namespaces, so each line stores the namespace it came from
    // to resolve references within the namespace:
    std::wstring ini_namespace;

    ini_line(std::wstring& key, std::wstring& val, std::wstring& line, const std::wstring& ini_namespace) :
        first(key),
        second(val),
        raw_line(line),
        ini_namespace(ini_namespace)
    {}
};

// Whereas settings within a section are in the same order they were in the ini
// file. This will become more important as shader overrides gains more
// functionality and dependencies between different features form:
typedef std::vector<ini_line> IniSectionVector;

void  get_ini_section(IniSectionVector** key_vals, const wchar_t* section);
int   get_ini_int(const wchar_t* section, const wchar_t* key, int def, bool* found, bool warn = true);
bool  get_ini_bool(const wchar_t* section, const wchar_t* key, bool def, bool* found, bool warn = true);
float get_ini_float(const wchar_t* section, const wchar_t* key, float def, bool* found);
int   get_ini_string(const wchar_t* section, const wchar_t* key, const wchar_t* def, wchar_t* ret, unsigned size);
bool  get_ini_string(const wchar_t* section, const wchar_t* key, const wchar_t* def, std::string* ret);
int   get_ini_string_and_log(const wchar_t* section, const wchar_t* key, const wchar_t* def, wchar_t* ret, unsigned size);

template <class T1, class T2>
T2 get_ini_enum_class(const wchar_t* section, const wchar_t* key, T2 def, bool* found, struct Enum_Name_t<T1, T2>* enum_names);

bool         get_namespaced_section_name_lower(const std::wstring* section, const std::wstring* ini_namespace, std::wstring* ret);
bool         get_section_namespace(const wchar_t* section, std::wstring* ret);
std::wstring get_namespaced_var_name_lower(const std::wstring var, const std::wstring* ini_namespace);
