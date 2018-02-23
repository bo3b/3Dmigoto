#pragma once

#include "Nektra/NktHookLib.h"
#include "util_min.h"

// We can have only one of these hook libraries for the entire process.
// This is exported so that we can install hooks later during runtime, as
// well as at launch.

extern CNktHookLib cHookMgr;


// Don't really want these here, but otherwise hooking infrastructure needs
// changing.

enum class EnableHooks {
	INVALID                     = 0,
	DEFERRED_CONTEXTS           = 0x00000001,
	IMMEDIATE_CONTEXT           = 0x00000002,
	CONTEXT                     = 0x00000003,
	DEVICE                      = 0x00000004,
	ALL                         = 0x0000ffff,
	DEPRECATED                  = 0x00010000,

	// All recommended hooks and workarounds. Does not include
	// skip_dxgi_factory as that could lead to us missing the present call:
	// No longer enables workarounds - now fixed in updated Deviare library
	RECOMMENDED                 = 0x00000007,
};
SENSIBLE_ENUM(EnableHooks);
static EnumName_t<wchar_t *, EnableHooks> EnableHooksNames[] = {
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

	{ NULL, EnableHooks::INVALID } // End of list marker
};
