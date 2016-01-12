#pragma once

#include <D3D11.h>

ID3D11DeviceContext* hook_context(ID3D11DeviceContext *orig_context, ID3D11DeviceContext *hacker_context);
