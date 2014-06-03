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

const int NUM_ACTIONS = 22;
extern wchar_t InputDevice[MAX_PATH];
extern wchar_t InputAction[NUM_ACTIONS][MAX_PATH];
extern int InputDeviceId;
extern DWORD ActionButton[NUM_ACTIONS];
extern bool Action[NUM_ACTIONS];
extern int XInputDeviceId;

extern FILE *LogFile;
extern bool LogInput;

HRESULT InitDirectInput();
VOID FreeDirectInput();
bool UpdateInputState();
