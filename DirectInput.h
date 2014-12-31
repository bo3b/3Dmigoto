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

typedef void (*input_callback)(void *device, void *private_data);

void register_ini_key_binding(LPCWSTR app, LPCWSTR key, LPCWSTR ini,
		input_callback down_cb, input_callback up_cb,
		void *private_data, FILE *log_file);
void dispatch_input_events(void *device);


extern wchar_t InputDevice[MAX_PATH];
extern int InputDeviceId;
extern int XInputDeviceId;

extern FILE *LogFile;
extern bool LogInput;

HRESULT InitDirectInput();
VOID FreeDirectInput();
bool UpdateInputState();
