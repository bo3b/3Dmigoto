#pragma once

#include <d3d11_1.h>

ID3D11Device1* hook_device(ID3D11Device1 *orig_device, ID3D11Device1 *hacker_device);
ID3D11Device1* lookup_hooked_device(ID3D11Device1 *orig_device);
