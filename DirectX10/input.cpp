#include "input.h"

#include "Main.h"
#include "../util.h"
#include <vector>
#include "../DirectX11/vkeys.h"
#include <Xinput.h>
#include <algorithm>
#include <ctime>
#include <exception>

// VS2013 BUG WORKAROUND: Make sure this class has a unique type name!
class KeyParseError: public exception {} keyParseError;

void InputListener::UpEvent(D3D10Base::ID3D10Device *device)
{
}

// -----------------------------------------------------------------------------

InputCallbacks::InputCallbacks(InputCallback down_cb, InputCallback up_cb,
		void *private_data) :
	down_cb(down_cb),
	up_cb(up_cb),
	private_data(private_data)
{}

void InputCallbacks::DownEvent(D3D10Base::ID3D10Device *device)
{
	if (down_cb)
		return down_cb(device, private_data);
}

void InputCallbacks::UpEvent(D3D10Base::ID3D10Device *device)
{
	if (up_cb)
		return up_cb(device, private_data);
}


// -----------------------------------------------------------------------------

InputAction::InputAction(InputButton *button, InputListener *listener) :
		last_state(false),
		button(button),
		listener(listener)
	{}

InputAction::~InputAction()
{
	delete button;
	delete listener;
}

bool InputAction::Dispatch(D3D10Base::ID3D10Device *device)
{
	bool state = button->CheckState();

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

VKInputButton::VKInputButton(wchar_t *keyName)
{
	vkey = ParseVKey(keyName);
	if (vkey < 0)
		throw keyParseError;
}

// The check for < 0 is a little odd.  The reason to use this form is because
// the call can also set the low bit in different situations that can theoretically
// result in non-zero, but top bit not set. This form ensures we only test the
// actual key bit.

bool VKInputButton::CheckState()
{
	return (GetAsyncKeyState(vkey) < 0);
}


// -----------------------------------------------------------------------------
// The RepeatAction is to allow for auto-repeat on hunting operations.
// Regular user inputs, and not all hunting operations are suitable for auto-
// repeat.  These are only created for operations that desire auto-repeat,
// otherwise the VKInputButton or XInputButton is used.

// For Dispatch, we have no need to be called as often as we are, that's just
// an artifact of where we get processing time, from the Draw() calls made by the game.
// To trim this down to a sensible human-oriented, keyboard input type time, we'll
// use the GetTickCount64 to skip processing.  The reason to add this limiter is
// to make auto-repeat slow enough to be usable, and consistent.

// TODO: Determine if an alternate thread can properly provide time. That would make
// it possible to simply have the OS call us as desired.

RepeatingInputAction::RepeatingInputAction(InputButton *button, InputListener *listener, int repeat) :
	repeatRate(repeat),
	InputAction(button, listener)
{}

bool RepeatingInputAction::Dispatch(D3D10Base::ID3D10Device *device)
{
	int ms = (1000 / repeatRate);
	if (GetTickCount64() < (lastTick + ms))
		return false;

	bool state = button->CheckState();

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

DelayedInputAction::DelayedInputAction(InputButton *button, InputListener *listener, int delay_down, int delay_up) :
	delay_down(delay_down),
	delay_up(delay_up),
	effective_state(false),
	state_change_time(0),
	InputAction(button, listener)
{}

bool DelayedInputAction::Dispatch(D3D10Base::ID3D10Device *device)
{
	ULONGLONG now = GetTickCount64();
	bool state = button->CheckState();

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

// -----------------------------------------------------------------------------

struct XInputState_t {
	XINPUT_STATE state;
	bool connected;
};
static XInputState_t XInputState[4];

bool XInputButton::_CheckState(int controller)
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
XInputButton::XInputButton(wchar_t *keyName) :
	controller(-1),
	button(0),
	left_trigger(0),
	right_trigger(0)
{
	int i, threshold = XINPUT_GAMEPAD_TRIGGER_THRESHOLD;
	BYTE *trigger;

	if (_wcsnicmp(keyName, L"XB", 2))
		throw keyParseError;
	keyName += 2;

	if (*keyName >= L'1' && *keyName <= L'4') {
		controller = *keyName - L'1';
		keyName++;
	}

	if (*keyName != L'_')
		throw keyParseError;
	keyName++;

	for (i = 0; i < ARRAYSIZE(XInputButtons); i++) {
		if (!_wcsicmp(keyName, XInputButtons[i].name)) {
			button = XInputButtons[i].val;
			break;
		}
	}
	if (button)
		return;

	if (!_wcsnicmp(keyName, L"LEFT_TRIGGER", 11)) {
		trigger = &left_trigger;
		keyName += 12;
	} else if (!_wcsnicmp(keyName, L"RIGHT_TRIGGER", 12)) {
		trigger = &right_trigger;
		keyName += 13;
	} else
		throw keyParseError;

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

bool XInputButton::CheckState()
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

static std::vector<class InputAction *> actions;

void RegisterKeyBinding(LPCWSTR iniKey, wchar_t *keyName,
		InputListener *listener, int auto_repeat, int down_delay,
		int up_delay)
{
	class InputAction *action;
	class InputButton *button;

	RightStripW(keyName);

	try {
		button = new VKInputButton(keyName);
	} catch (KeyParseError) {
		try {
			button = new XInputButton(keyName);
		} catch (KeyParseError) {
			LogInfoW(L"  WARNING: UNABLE TO PARSE KEY BINDING %s=%s\n", iniKey, keyName);
			BeepFailure2();
			return;
		}
	}

	if (auto_repeat)
		action = new RepeatingInputAction(button, listener, auto_repeat);
	else if (down_delay || up_delay)
		action = new DelayedInputAction(button, listener, down_delay, up_delay);
	else
		action = new InputAction(button, listener);

	LogInfoW(L"  %s=%s\n", iniKey, keyName);
	actions.push_back(action);
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
	std::vector<class InputAction *>::iterator i;

	for (i = actions.begin(); i != actions.end(); i++)
		delete *i;

	actions.clear();
}


bool DispatchInputEvents(D3D10Base::ID3D10Device *device)
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
