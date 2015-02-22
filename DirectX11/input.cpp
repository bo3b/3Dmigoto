#include "input.h"

#include "Main.h"
#include "../log.h"
#include "../util.h"
#include <vector>
#include "vkeys.h"
#include <Xinput.h>
#include <algorithm>
#include <ctime>


void InputListener::UpEvent(D3D11Base::ID3D11Device *device)
{
}

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

DelayedInputAction::DelayedInputAction(int delay_down, int delay_up, InputListener *listener) :
	delay_down(delay_down),
	delay_up(delay_up),
	effective_state(false),
	state_change_time(0),
	InputAction(listener)
{}

bool DelayedInputAction::Dispatch(D3D11Base::ID3D11Device *device)
{
	ULONGLONG now = GetTickCount64();
	bool state = CheckState();

	if (state != last_state)
		state_change_time = now;
	last_state = state;

	if (state != effective_state) {
		if (state && ((now - state_change_time) >= delay_down)) {
			effective_state = state;
			listener->DownEvent(device);
			return true;
		} else if (!state && ((now - state_change_time) >= delay_up)) {
			effective_state = state;
			listener->UpEvent(device);
			return true;
		}
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

VKDelayedInputAction::VKDelayedInputAction(int vkey, int down_delay, int up_delay, InputListener *listener) :
	DelayedInputAction(down_delay, up_delay, listener),
	InputAction(listener),
	VKInputAction(vkey, listener)
{}

bool VKDelayedInputAction::Dispatch(D3D11Base::ID3D11Device *device)
{
	return DelayedInputAction::Dispatch(device);
}

// Only necessary to silence an MSVC warning - there is only one CheckState
// implementation in the class heirachy
bool VKDelayedInputAction::CheckState()
{
	return VKInputAction::CheckState();
}

XDelayedInputAction::XDelayedInputAction(int controller, WORD button, BYTE left_trigger,
		BYTE right_trigger, int down_delay, int up_delay,
		InputListener *listener) :
	DelayedInputAction(down_delay, up_delay, listener),
	InputAction(listener),
	XInputAction(controller, button, left_trigger, right_trigger, listener)
{}

bool XDelayedInputAction::Dispatch(D3D11Base::ID3D11Device *device)
{
	return DelayedInputAction::Dispatch(device);
}

// Only necessary to silence an MSVC warning - there is only one CheckState
// implementation in the class heirachy
bool XDelayedInputAction::CheckState()
{
	return XInputAction::CheckState();
}


// -----------------------------------------------------------------------------

VKInputAction *NewVKInputAction(wchar_t *keyName, InputListener *listener,
		int auto_repeat, int down_delay, int up_delay)
{
	int vkey;

	vkey = ParseVKey(keyName);
	if (vkey < 0)
		return NULL;

	if (auto_repeat)
		return new VKRepeatingInputAction(vkey, auto_repeat, listener);

	if (down_delay || up_delay)
		return new VKDelayedInputAction(vkey, down_delay, up_delay, listener);

	return new VKInputAction(vkey, listener);
}

struct XInputState_t {
	XINPUT_STATE state;
	bool connected;
};
static XInputState_t XInputState[4];

bool XInputAction::_CheckState(int controller)
{
	XINPUT_GAMEPAD *gamepad = &XInputState[controller].state.Gamepad;

	if (!XInputState[controller].connected)
		return false;

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

static EnumName_t<wchar_t *, WORD> XInputButtons[] = {
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
// XB_DPAD_DOWN           - dpad down on any controller
// xb1_left_trigger > 128 - left trigger half way on 1st controller
//
// I originally wrote this using regular expressions rather than C style
// pointer arithmetic, but it looks like MSVC's implementation may be buggy
// (either that or I have a very precise 100% reproducable memory corruption
// issue), which isn't that surprising given that regular expressions are
// uncommon in the Windows world. Feel free to rewrite this in a cleaner way.
XInputAction *NewXInputAction(wchar_t *keyName, InputListener *listener,
		int auto_repeat, int down_delay, int up_delay)
{
	int i, controller = -1, button = 0, threshold = XINPUT_GAMEPAD_TRIGGER_THRESHOLD;
	BYTE left_trigger = 0, right_trigger = 0;
	BYTE *trigger;

	if (_wcsnicmp(keyName, L"XB", 2))
		return NULL;
	keyName += 2;

	if (*keyName >= L'1' && *keyName <= L'4') {
		controller = *keyName - L'1';
		keyName++;
	}

	if (*keyName != L'_')
		return NULL;
	keyName++;

	for (i = 0; i < ARRAYSIZE(XInputButtons); i++) {
		if (!_wcsicmp(keyName, XInputButtons[i].name)) {
			button = XInputButtons[i].val;
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

	if (down_delay || up_delay)
		return new XDelayedInputAction(controller, button, left_trigger, right_trigger, down_delay, up_delay, listener);

	return new XInputAction(controller, button, left_trigger, right_trigger, listener);
}

static std::vector<class InputAction *> actions;

void RegisterKeyBinding(LPCWSTR iniKey, wchar_t *keyName,
		InputListener *listener, int auto_repeat, int down_delay,
		int up_delay)
{
	class InputAction *action;

	RightStripW(keyName);

	action = NewVKInputAction(keyName, listener, auto_repeat, down_delay, up_delay);
	if (!action)
		action = NewXInputAction(keyName, listener, auto_repeat, down_delay, up_delay);

	if (action) {
		LogInfoW(L"  %s=%s\n", iniKey, keyName);
		actions.push_back(action);
	} else {
		LogInfoW(L"  WARNING: UNABLE TO PARSE KEY BINDING %s=%s\n", iniKey, keyName);
	}
}

bool RegisterIniKeyBinding(LPCWSTR app, LPCWSTR iniKey, LPCWSTR ini,
		InputCallback down_cb, InputCallback up_cb, int auto_repeat,
		void *private_data)
{
	InputCallbacks *callbacks = new InputCallbacks(down_cb, up_cb, private_data);
	wchar_t keyName[MAX_PATH];

	if (!GetPrivateProfileString(app, iniKey, 0, keyName, MAX_PATH, ini))
		return false;

	RegisterKeyBinding(iniKey, keyName, callbacks, auto_repeat, 0, 0);
	return true;
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
	static time_t last_time = 0;
	time_t now = time(NULL);
	int j;

	for (j = 0; j < 4; j++) {
		// Stagger polling controllers that were not connected last
		// frame over four seconds to minimise performance impact,
		// which has been observed to be extremely significant.
		if (!XInputState[j].connected && ((now == last_time) || (now % 4 != j)))
			continue;

		// TODO: Use undocumented XInputGetStateEx so we can also read the guide button
		XInputState[j].connected =
			(XInputGetState(j, &XInputState[j].state) == ERROR_SUCCESS);
	}

	last_time = now;

	for (i = actions.begin(); i != actions.end(); i++) {
		action = *i;

		input_processed |= action->Dispatch(device);
	}

	return input_processed;
}
