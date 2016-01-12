#pragma once

#include <D3D11.h>

ID3D11Device* hook_device(ID3D11Device *orig_device, ID3D11Device *hacker_device);
