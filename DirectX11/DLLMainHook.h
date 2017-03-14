#pragma once

#include "Nektra/NktHookLib.h"
#include "util_min.h"

// We can have only one of these hook libraries for the entire process.

extern CNktHookLib cHookMgr;
extern bool bLog;

void InitD311();
void InstallSetWindowPosHook();

enum class EnableHooks {
	INVALID                     = 0,
	DEFERRED_CONTEXTS           = 0x00000001,
	IMMEDIATE_CONTEXT           = 0x00000002,
	CONTEXT                     = 0x00000003,
	DEVICE                      = 0x00000004,
	ALL                         = 0x0000ffff,
	EXCEPT_SET_SHADER_RESOURCES = 0x00010000, // Crashes in MGSV:TPP if NumViews=0
	EXCEPT_SET_SAMPLERS         = 0x00020000, // Crashes in MGSV:GZ on Win10, possibly also source of crashes in Witcher 3 on Win7
	EXCEPT_SET_RASTERIZER_STATE = 0x00040000, // Crashes in MGSV:GZ on Win 7 WITHOUT evil update
	SKIP_DXGI_FACTORY           = 0x00080000, // Specific hack for MGSV (both) on Win 10, which rejects the wrapped DXGIFactory (but not DXGIFactory1)
	SKIP_DXGI_DEVICE            = 0x00100000, // Specific hack for MGSV on Win 10 WITH the anniversary update, which rejects the wrapped DXGIDevice

	// All recommended hooks and workarounds. Does not include
	// skip_dxgi_factory as that could lead to us missing the present call:
	RECOMMENDED                 = 0x00070007,
};
SENSIBLE_ENUM(EnableHooks);
static EnumName_t<wchar_t *, EnableHooks> EnableHooksNames[] = {
	{L"deferred_contexts", EnableHooks::DEFERRED_CONTEXTS},
	{L"immediate_context", EnableHooks::IMMEDIATE_CONTEXT},
	{L"context", EnableHooks::IMMEDIATE_CONTEXT},
	{L"device", EnableHooks::DEVICE},
	{L"all", EnableHooks::ALL},
	{L"except_set_shader_resources", EnableHooks::EXCEPT_SET_SHADER_RESOURCES},
	{L"except_set_samplers", EnableHooks::EXCEPT_SET_SAMPLERS},
	{L"except_set_rasterizer_state", EnableHooks::EXCEPT_SET_RASTERIZER_STATE},
	{L"skip_dxgi_factory", EnableHooks::SKIP_DXGI_FACTORY},
	{L"skip_dxgi_device", EnableHooks::SKIP_DXGI_DEVICE},
	{L"recommended", EnableHooks::RECOMMENDED},
	{NULL, EnableHooks::INVALID} // End of list marker
};
