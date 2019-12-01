#pragma once

#include "Nektra/NktHookLib.h"
#include "util_min.h"

// We can have only one of these hook libraries for the entire process.
// This is exported so that we can install hooks later during runtime, as
// well as at launch.

extern CNktHookLib cHookMgr;


enum class EnableHooksDX9 {
	INVALID                     = 0,
	IMMEDIATE_CONTEXT           = 0x00000002,
	CONTEXT                     = 0x00000003,
	DEVICE                      = 0x00000004,
	ALL                         = 0x0000ffff,
	// All recommended hooks and workarounds. Does not include
	// skip_dxgi_factory as that could lead to us missing the present call:
	// No longer enables workarounds - now fixed in updated Deviare library
	RECOMMENDED                 = 0x00000007,
};
SENSIBLE_ENUM(EnableHooksDX9);
static EnumName_t<wchar_t *, EnableHooksDX9> EnableHooksDX9Names[] = {
	{ L"immediate_context", EnableHooksDX9::IMMEDIATE_CONTEXT },
	{ L"context", EnableHooksDX9::IMMEDIATE_CONTEXT },
	{ L"device", EnableHooksDX9::DEVICE },
	{ L"all", EnableHooksDX9::ALL },
	{ L"recommended", EnableHooksDX9::RECOMMENDED },

	{ NULL, EnableHooksDX9::INVALID } // End of list marker
};
