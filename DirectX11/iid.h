#pragma once

#include <d3d11_1.h>

bool check_interface_supported(IUnknown* unknown, REFIID riid);
void analyse_iunknown(IUnknown* unknown);

// TODO: For the time being, since we are not setup to use the Win10 SDK, we'll add
// these manually. Some games under Win10 are requesting these.

struct _declspec(uuid("9d06dffa-d1e5-4d07-83a8-1bb123f2f841")) ID3D11Device2;
struct _declspec(uuid("420d5b32-b90c-4da4-bef0-359f6a24a83a")) ID3D11DeviceContext2;
struct _declspec(uuid("A8BE2AC4-199F-4946-B331-79599FB98DE7")) IDXGISwapChain2;
struct _declspec(uuid("94D99BDB-F1F8-4AB0-B236-7DA0170EDAB1")) IDXGISwapChain3;
struct _declspec(uuid("3D585D5A-BD4A-489E-B1F4-3DBCB6452FFB")) IDXGISwapChain4;

std::string name_from_IID(IID id);
