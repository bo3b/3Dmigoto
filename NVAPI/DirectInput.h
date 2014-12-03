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

extern wchar_t InputDevice[MAX_PATH], InputAction[MAX_PATH], ToggleAction[MAX_PATH];
extern int InputDeviceId;
extern DWORD ActionButton;
extern bool Action;

// Four states of toggle, with key presses.  
enum tState
{
	offDown, offUp, onDown, onUp
};
extern tState Toggle;

extern int XInputDeviceId;

extern FILE *LogFile;
extern bool LogInput;

HRESULT InitDirectInput();
VOID FreeDirectInput();
void UpdateInputState();
