#pragma once

#include "EnumNames.hpp"

#include <d3dcommon.h>
#include <string>
#include <vector>

// Hunting
//  This code is to implement the Hunting mechanism as a separate compilation from the main wrapper code.
//  It implements all the shader management based on user input via key presses from Input.

// Custom #include handler used to track which shaders need to be reloaded after an included file is modified
class MigotoIncludeHandler : public ID3DInclude
{
    std::vector<std::string> dirStack;

    void PushDir(const char* path);

public:
    MigotoIncludeHandler(const char* path);

    virtual COM_DECLSPEC_NOTHROW HRESULT STDMETHODCALLTYPE Open(D3D_INCLUDE_TYPE include_type, LPCSTR file_name, LPCVOID parent_data, LPCVOID* data, UINT* bytes);
    virtual COM_DECLSPEC_NOTHROW HRESULT STDMETHODCALLTYPE Close(LPCVOID data);
};

// -----------------------------------------------------------------------------------------------

enum class Hunting_Mode : int
{
    disabled      = 0,
    enabled       = 1,
    soft_disabled = 2,
};

bool hunting_enabled();

void timeout_hunting_buffers();
void parse_hunting_section();
void dump_usage(wchar_t* dir);

// -----------------------------------------------------------------------------------------------

enum class MarkingMode
{
    SKIP,
    ORIGINAL,
    PINK,
    MONO,

    INVALID,  // Must be last - used for next_marking_mode
};
static Enum_Name_t<const wchar_t*, MarkingMode> MarkingModeNames[] = {
    { L"skip", MarkingMode::SKIP },
    { L"mono", MarkingMode::MONO },
    { L"original", MarkingMode::ORIGINAL },
    { L"pink", MarkingMode::PINK },
    { NULL, MarkingMode::INVALID }  // End of list marker
};

enum class MarkingAction
{
    INVALID    = 0,
    CLIPBOARD  = 0x0000001,
    HLSL       = 0x0000002,
    ASM        = 0x0000004,
    REGEX      = 0x0000008,
    DUMP_MASK  = 0x000000e,  // HLSL, Assembly and/or ShaderRegex is selected
    MONO_SS    = 0x0000010,
    STEREO_SS  = 0x0000020,
    SS_IF_PINK = 0x0000040,

    DEFAULT = 0x0000003,
};
SENSIBLE_ENUM(MarkingAction);
static Enum_Name_t<const wchar_t*, MarkingAction> MarkingActionNames[] = {
    { L"hlsl", MarkingAction::HLSL },
    { L"asm", MarkingAction::ASM },
    { L"assembly", MarkingAction::ASM },
    { L"regex", MarkingAction::REGEX },
    { L"ShaderRegex", MarkingAction::REGEX },
    { L"clipboard", MarkingAction::CLIPBOARD },
    { L"mono_snapshot", MarkingAction::MONO_SS },
    { L"stereo_snapshot", MarkingAction::STEREO_SS },
    { L"snapshot_if_pink", MarkingAction::SS_IF_PINK },
    { NULL, MarkingAction::INVALID }  // End of list marker
};
