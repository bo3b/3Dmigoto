#pragma once

#include "Main.h"

typedef void (*InputCallback)(D3D11Base::ID3D11Device *device, void *private_data);

void RegisterKeyBinding(LPCWSTR iniKey, wchar_t *keyName,
		InputCallback down_cb, InputCallback up_cb,
		void *private_data);
void RegisterIniKeyBinding(LPCWSTR app, LPCWSTR key, LPCWSTR ini,
		InputCallback down_cb, InputCallback up_cb,
		void *private_data);
bool DispatchInputEvents(D3D11Base::ID3D11Device *device);
