#pragma once

#include "EnumNames.hpp"

#include <d3d11_1.h>
#include <string>
#include <vector>

// We include this specifically after d3d11.h so that it can define
// the __d3d11_h__ preprocessor and pick up extra calls.
#include "nvapi.h"

class HackerDevice;

//--------------------------------------------------------------------------------------------------

// We now emit a single warning tone after the config file is [re]loaded to get
// the shaderhackers attention if something needs to be addressed, since their
// eyes may be focussed elsewhere and may miss the notification message[s].

static bool ini_warned = false;
#define INI_WARNING(fmt, ...)                        \
    do                                               \
    {                                                \
        ini_warned = true;                           \
        log_overlay(overlay::log::warning, fmt, __VA_ARGS__); \
    } while (0)
#define INI_WARNING_W(fmt, ...)                        \
    do                                                 \
    {                                                  \
        ini_warned = true;                             \
        log_overlay_w(overlay::log::warning, fmt, __VA_ARGS__); \
    } while (0)
#define INI_WARNING_BEEP() \
    do                     \
    {                      \
        ini_warned = true; \
    } while (0)


// Since these are found via d3dx.ini parameters, we've moved the declaration
// here to IniHandler.h.  There is no more obvious client of these. In
// general we want these Enum_Name_t to be defined by their owner.

enum class EnableHooks
{
    INVALID           = 0,
    DEFERRED_CONTEXTS = 0x00000001,
    IMMEDIATE_CONTEXT = 0x00000002,
    CONTEXT           = 0x00000003,
    DEVICE            = 0x00000004,
    ALL               = 0x0000ffff,
    DEPRECATED        = 0x00010000,

    // All recommended hooks and workarounds. Does not include
    // skip_dxgi_factory as that could lead to us missing the present call:
    // No longer enables workarounds - now fixed in updated Deviare library
    RECOMMENDED = 0x00000007,
};
SENSIBLE_ENUM(EnableHooks);
static Enum_Name_t<wchar_t*, EnableHooks> EnableHooksNames[] = {
    { L"deferred_contexts", EnableHooks::DEFERRED_CONTEXTS },
    { L"immediate_context", EnableHooks::IMMEDIATE_CONTEXT },
    { L"context", EnableHooks::IMMEDIATE_CONTEXT },
    { L"device", EnableHooks::DEVICE },
    { L"all", EnableHooks::ALL },
    { L"recommended", EnableHooks::RECOMMENDED },

    // These options are no longer necessary, but kept here to avoid beep
    // notifications when using new DLLs with old d3dx.ini files.
    { L"except_set_shader_resources", EnableHooks::DEPRECATED },
    { L"except_set_samplers", EnableHooks::DEPRECATED },
    { L"except_set_rasterizer_state", EnableHooks::DEPRECATED },
    { L"skip_dxgi_factory", EnableHooks::DEPRECATED },
    { L"skip_dxgi_device", EnableHooks::DEPRECATED },

    { NULL, EnableHooks::INVALID }  // End of list marker
};

enum class ShaderHashType
{
    INVALID = -1,
    FNV,
    EMBEDDED,
    BYTECODE,
};
static Enum_Name_t<const wchar_t*, ShaderHashType> ShaderHashNames[] = {
    { L"3dmigoto", ShaderHashType::FNV },
    { L"embedded", ShaderHashType::EMBEDDED },
    { L"bytecode", ShaderHashType::BYTECODE },
    { NULL, ShaderHashType::INVALID }  // End of list marker
};

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

bool         get_namespaced_section_name_lower(const std::wstring* section, const std::wstring* ini_namespace, std::wstring* ret);
bool         get_section_namespace(const wchar_t* section, std::wstring* ret);
std::wstring get_namespaced_var_name_lower(const std::wstring var, const std::wstring* ini_namespace);
