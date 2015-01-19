#pragma once

#include <Windows.h>
#include <Shlobj.h>
#include <dinput.h>
#include <dinputd.h>
#include <assert.h>
#include <oleauto.h>
#include <shellapi.h>
#include <stdio.h>
#include <map>
#include <ctime>
#include <XInput.h>

typedef void (*InputCallback)(void *device, void *private_data);

void RegisterIniKeyBinding(LPCWSTR app, LPCWSTR key, LPCWSTR ini,
		InputCallback down_cb, InputCallback up_cb,
		void *private_data, FILE *log_file);
void DispatchInputEvents(void *device);


extern wchar_t InputDevice[MAX_PATH];
extern int InputDeviceId;
extern int XInputDeviceId;

extern FILE *LogFile;
extern bool LogInput;

HRESULT InitDirectInput();
VOID FreeDirectInput();
bool UpdateInputState();
