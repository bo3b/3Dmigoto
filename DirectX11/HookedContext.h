#pragma once

#include <d3d11_1.h>

#include "DLLMainHook.h"

ID3D11DeviceContext1* hook_context(ID3D11DeviceContext1 *orig_context, ID3D11DeviceContext1 *hacker_context);

ID3D11DeviceContext1* lookup_hooked_context(ID3D11DeviceContext1 *orig_context);
