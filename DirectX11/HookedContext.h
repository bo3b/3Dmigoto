#pragma once

#include <D3D11_1.h>
#include "DLLMainHook.h"

ID3D11DeviceContext* hook_context(ID3D11DeviceContext *orig_context, ID3D11DeviceContext *hacker_context, EnableHooks enable_hooks);

ID3D11DeviceContext* lookup_hooked_context(ID3D11DeviceContext *orig_context);
