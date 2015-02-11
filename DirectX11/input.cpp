#include "input.h"

#include "Main.h"
#include "../log.h"
#include "../util.h"
#include <vector>
#include "vkeys.h"
#include <Xinput.h>
#include <algorithm>


// -----------------------------------------------------------------------------

InputCallbacks::InputCallbacks(InputCallback down_cb, InputCallback up_cb,
		void *private_data) :
	down_cb(down_cb),
	up_cb(up_cb),
	private_data(private_data)
{}

void InputCallbacks::DownEvent(D3D11Base::ID3D11Device *device)
{
	if (down_cb)
		return down_cb(device, private_data);
}

void InputCallbacks::UpEvent(D3D11Base::ID3D11Device *device)
{
	if (up_cb)
		return up_cb(device, private_data);
}


// -----------------------------------------------------------------------------

InputAction::InputAction(InputListener *listener) :
		last_state(false),
		listener(listener)
	{}

InputAction::~InputAction()
{
	delete listener;
}

bool InputAction::Dispatch(D3D11Base::ID3D11Device *device)
{
	bool state = CheckState();

	if (state == last_state)
		return false;

	if (state)
		listener->DownEvent(device);
	else
		listener->UpEvent(device);

	last_state = state;

	return true;
}


// -----------------------------------------------------------------------------

VKInputAction::VKInputAction(int vkey, InputListener *listener) :
	InputAction(listener),
	vkey(vkey)
{}

// The check for < 0 is a little odd.  The reason to use this form is because
// the call can also set the low bit in different situations that can theoretically
// result in non-zero, but top bit not set. This form ensures we only test the
// actual key bit.

bool VKInputAction::CheckState()
{
	return (GetAsyncKeyState(vkey) < 0);
}


// -----------------------------------------------------------------------------
// The RepeatAction is to allow for auto-repeat on hunting operations.
// Regular user inputs, and not all hunting operations are suitable for auto-
// repeat.  These are only created for operations that desire auto-repeat,
// otherwise the VKInputAction or XInputAction is used.

// For Dispatch, we have no need to be called as often as we are, that's just
// an artifact of where we get processing time, from the Draw() calls made by the game.
// To trim this down to a sensible human-oriented, keyboard input type time, we'll
// use the GetTickCount64 to skip processing.  The reason to add this limiter is
// to make auto-repeat slow enough to be usable, and consistent.

// TODO: Determine if an alternate thread can properly provide time. That would make
// it possible to simply have the OS call us as desired.

RepeatingInputAction::RepeatingInputAction(int repeat, InputListener *listener) :
	repeatRate(repeat),
	InputAction(listener)
{}

bool RepeatingInputAction::Dispatch(D3D11Base::ID3D11Device *device)
{
	int ms = (1000 / repeatRate);
	if (GetTickCount64() < (lastTick + ms))
		return false;

	bool state = CheckState();

	// Only allow auto-repeat for down events.
	if (state || (state != last_state))
	{
		if (state)
			listener->DownEvent(device);
		else
			listener->UpEvent(device);

		lastTick = GetTickCount64();
		last_state = state;

		return true;
	}

	return false;
}

VKRepeatingInputAction::VKRepeatingInputAction(int vkey, int repeat, InputListener *listener) :
	RepeatingInputAction(repeat, listener),
	InputAction(listener),
	VKInputAction(vkey, listener)
{}

bool VKRepeatingInputAction::Dispatch(D3D11Base::ID3D11Device *device)
{
	return RepeatingInputAction::Dispatch(device);
}

// Only necessary to silence an MSVC warning - there is only one CheckState
// implementation in the class heirachy
bool VKRepeatingInputAction::CheckState()
{
	return VKInputAction::CheckState();
}

XRepeatingInputAction::XRepeatingInputAction(int controller, WORD button, BYTE left_trigger,
		BYTE right_trigger, int repeat,
		InputListener *listener) :
	RepeatingInputAction(repeat, listener),
	InputAction(listener),
	XInputAction(controller, button, left_trigger, right_trigger, listener)
{}

bool XRepeatingInputAction::Dispatch(D3D11Base::ID3D11Device *device)
{
	return RepeatingInputAction::Dispatch(device);
}

// Only necessary to silence an MSVC warning - there is only one CheckState
// implementation in the class heirachy
bool XRepeatingInputAction::CheckState()
{
	return XInputAction::CheckState();
}


// -----------------------------------------------------------------------------

VKInputAction *NewVKInputAction(wchar_t *keyName, InputListener *listener, int auto_repeat)
{
	int vkey;

	vkey = ParseVKey(keyName);
	if (vkey < 0)
		return NULL;

	if (auto_repeat)
		return new VKRepeatingInputAction(vkey, auto_repeat, listener);

	return new VKInputAction(vkey, listener);
}

static XINPUT_STATE XInputState[4];

bool XInputAction::_CheckState(int controller)
{
	XINPUT_GAMEPAD *gamepad = &XInputState[controller].Gamepad;

	if (button && (gamepad->wButtons & button))
		return true;
	if (left_trigger && (gamepad->bLeftTrigger >= left_trigger))
		return true;
	if (right_trigger && (gamepad->bRightTrigger >= right_trigger))
		return true;

	return false;
}

XInputAction::XInputAction(int controller, WORD button, BYTE left_trigger,
		BYTE right_trigger, InputListener *listener) :
	InputAction(listener),
	controller(controller),
	button(button),
	left_trigger(left_trigger),
	right_trigger(right_trigger)
{}

bool XInputAction::CheckState()
{
	int i;

	if (controller != -1)
		return _CheckState(controller);

	for (i = 0; i < 4; i++) {
		if (_CheckState(i))
			return true;
	}

	return false;
}

struct XInputMapping_t {
	wchar_t *name;
	WORD button;
};
static XInputMapping_t XInputButtons[] = {
	{L"DPAD_UP", XINPUT_GAMEPAD_DPAD_UP},
	{L"DPAD_DOWN", XINPUT_GAMEPAD_DPAD_DOWN},
	{L"DPAD_LEFT", XINPUT_GAMEPAD_DPAD_LEFT},
	{L"DPAD_RIGHT", XINPUT_GAMEPAD_DPAD_RIGHT},
	{L"START", XINPUT_GAMEPAD_START},
	{L"BACK", XINPUT_GAMEPAD_BACK},
	{L"LEFT_THUMB", XINPUT_GAMEPAD_LEFT_THUMB},
	{L"RIGHT_THUMB", XINPUT_GAMEPAD_RIGHT_THUMB},
	{L"LEFT_SHOULDER", XINPUT_GAMEPAD_LEFT_SHOULDER},
	{L"RIGHT_SHOULDER", XINPUT_GAMEPAD_RIGHT_SHOULDER},
	{L"A", XINPUT_GAMEPAD_A},
	{L"B", XINPUT_GAMEPAD_B},
	{L"X", XINPUT_GAMEPAD_X},
	{L"Y", XINPUT_GAMEPAD_Y},
	{L"GUIDE", 0x400}, /* Placeholder for now - need to use undocumented XInputGetStateEx call */
};

// This function is parsing strings with formats such as:
//
// XINPUT:DPAD_DOWN               - dpad down on any controller
// xinput 0 : left_trigger > 128  - left trigger half way on 1st controller
//
// I originally wrote this using regular expressions rather than C style
// pointer arithmetic, but it looks like MSVC's implementation may be buggy
// (either that or I have a very precise 100% reproducable memory corruption
// issue), which isn't that surprising given that regular expressions are
// uncommon in the Windows world. Feel free to rewrite this in a cleaner way.
XInputAction *NewXInputAction(wchar_t *keyName, InputListener *listener, int auto_repeat)
{
	int i, controller = -1, button = 0, threshold = 0;
	BYTE left_trigger = 0, right_trigger = 0;
	BYTE *trigger;

	if (_wcsnicmp(keyName, L"XINPUT", 6))
		return NULL;
	keyName += 6;

	while (*keyName == L' ')
		keyName++;

	if (*keyName >= L'0' && *keyName < L'4') {
		controller = *keyName - L'0';
		keyName++;
	}

	while (*keyName == L' ')
		keyName++;

	if (*keyName != L':')
		return NULL;
	keyName++;

	while (*keyName == L' ')
		keyName++;

	for (i = 0; i < ARRAYSIZE(XInputButtons); i++) {
		if (!_wcsicmp(keyName, XInputButtons[i].name)) {
			button = XInputButtons[i].button;
			break;
		}
	}

	if (!button) {
		if (!_wcsnicmp(keyName, L"LEFT_TRIGGER", 11)) {
			trigger = &left_trigger;
			keyName += 12;
		} else if (!_wcsnicmp(keyName, L"RIGHT_TRIGGER", 12)) {
			trigger = &right_trigger;
			keyName += 13;
		} else
			return NULL;

		while (*keyName == L' ')
			keyName++;

		if (*keyName == L'>') {
			keyName++;
			while (*keyName == L' ')
				keyName++;
			threshold = _wtoi(keyName);
		}

		*trigger = std::min(threshold + 1, 255);
	}

	if (auto_repeat)
		return new XRepeatingInputAction(controller, button, left_trigger, right_trigger, auto_repeat, listener);

	return new XInputAction(controller, button, left_trigger, right_trigger, listener);
}

#if 0
// Alternate implementation using regular expressions - gave up after hitting
// what appears to be a bug in MSVC (if I was able to hit one so easily with a
// relatively straight forward expression, who knows how many more are lurking
// in wait). Alternatively maybe I mucked up something I just can't spot.
XInputAction *NewXInputAction(wchar_t *keyName, InputListener *listener)
{
	std::wregex pattern(L"^XINPUT\\s*([0-3])?\\s*:\\s*([A-Z_]*)(?:\\s*>\\s*([0-9]+))?$",
			std::regex_constants::icase);
	std::wcmatch match;
	const wchar_t *button_name;
	int i, controller = -1, button = 0;
	BYTE left_trigger = 0, right_trigger = 0, threshold = 1;

	if (!std::regex_match(keyName, match, pattern))
		return NULL;

	if (match[1].length())
		controller = stoi(match[1].str());

	// Looks like an MSVC bug - the first character in this group is garbage :(
	button_name = match[2].str().c_str();
	LogInfoW(L"button_name: %s\n", button_name);

	for (i = 0; i < ARRAYSIZE(XInputButtons); i++) {
		if (!_wcsicmp(button_name, XInputButtons[i].name)) {
			button = XInputButtons[i].button;
			break;
		}
	}

	if (!button) {
		if (match[3].length())
			threshold = stoi(match[3].str()) + 1;

		if (!_wcsicmp(button_name, L"LEFT_TRIGGER"))
			left_trigger = threshold;
		else if (!_wcsicmp(button_name, L"RIGHT_TRIGGER"))
			right_trigger = threshold;
		else
			return NULL;
	}

	return new XInputAction(controller, button, left_trigger, right_trigger, listener);

}
#endif

static std::vector<class InputAction *> actions;

void RegisterKeyBinding(LPCWSTR iniKey, wchar_t *keyName,
		InputListener *listener, int auto_repeat)
{
	class InputAction *action;

	RightStripW(keyName);

	action = NewVKInputAction(keyName, listener, auto_repeat);
	if (!action)
		action = NewXInputAction(keyName, listener, auto_repeat);

	if (action) {
		LogInfoW(L"  %s=%s\n", iniKey, keyName);
		actions.push_back(action);
	} else {
		LogInfoW(L"  WARNING: UNABLE TO PARSE KEY BINDING %s=%s\n", iniKey, keyName);
	}
}

void RegisterIniKeyBinding(LPCWSTR app, LPCWSTR iniKey, LPCWSTR ini,
		InputCallback down_cb, InputCallback up_cb, int auto_repeat,
		void *private_data)
{
	InputCallbacks *callbacks = new InputCallbacks(down_cb, up_cb, private_data);
	wchar_t keyName[MAX_PATH];

	if (!GetPrivateProfileString(app, iniKey, 0, keyName, MAX_PATH, ini))
		return;

	RegisterKeyBinding(iniKey, keyName, callbacks, auto_repeat);
}

void ClearKeyBindings()
{
	actions.clear();
}


bool DispatchInputEvents(D3D11Base::ID3D11Device *device)
{
	std::vector<class InputAction *>::iterator i;
	class InputAction *action;
	bool input_processed = false;
	int j;

	for (j = 0; j < 4; j++) {
		// TODO: Use undocumented XInputGetStateEx so we can also read the guide button
		if (XInputGetState(j, &XInputState[j]) != ERROR_SUCCESS) {
			if (XInputState[j].dwPacketNumber)
				memset(&XInputState[j], 0, sizeof(XINPUT_STATE));
		}
	}

	for (i = actions.begin(); i != actions.end(); i++) {
		action = *i;

		input_processed |= action->Dispatch(device);
	}

	return input_processed;
}
