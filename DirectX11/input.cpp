#include "input.h"

#include "Main.h"
#include "../log.h"
#include "../util.h"
#include <vector>
#include "vkeys.h"
#include <Xinput.h>
#include <algorithm>

class InputCallbacks : public InputListener {
private:
	InputCallback down_cb;
	InputCallback up_cb;
	void *private_data;

public:
	InputCallbacks(InputCallback down_cb, InputCallback up_cb,
			void *private_data) :
		down_cb(down_cb),
		up_cb(up_cb),
		private_data(private_data)
	{}

	void DownEvent(D3D11Base::ID3D11Device *device)
	{
		if (down_cb)
			return down_cb(device, private_data);
	}

	void UpEvent(D3D11Base::ID3D11Device *device)
	{
		if (up_cb)
			return up_cb(device, private_data);
	}
};

// I'm using inheritance here because if we wanted to add another input backend
// in the future this is where I see the logical split between common code and
// input backend specific code.

class InputAction {
public:
	bool last_state;
	InputListener *listener;

	InputAction(InputListener *listener) :
		last_state(false),
		listener(listener)
	{}

	~InputAction()
	{
		delete listener;
	}

	virtual bool CheckState()
	{
		return false;
	}

	bool Dispatch(D3D11Base::ID3D11Device *device)
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
};

class VKInputAction : public InputAction {
public:
	int vkey;

	VKInputAction(int vkey, InputListener *listener) :
		InputAction(listener),
		vkey(vkey)
	{}

	bool CheckState()
	{
		return (GetAsyncKeyState(vkey) < 0);
	}
};

VKInputAction *NewVKInputAction(wchar_t *keyName, InputListener *listener)
{
	int vkey;

	vkey = ParseVKey(keyName);
	if (vkey < 0)
		return NULL;

	return new VKInputAction(vkey, listener);
}

static XINPUT_STATE XInputState[4];

class XInputAction : public InputAction {
private:
	int controller;
	WORD button;
	BYTE left_trigger;
	BYTE right_trigger;

	bool _CheckState(int controller)
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

public:
	XInputAction(int controller, WORD button, BYTE left_trigger,
			BYTE right_trigger, InputListener *listener) :
		InputAction(listener),
		controller(controller),
		button(button),
		left_trigger(left_trigger),
		right_trigger(right_trigger)
	{}

	bool CheckState()
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
};

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
XInputAction *NewXInputAction(wchar_t *keyName, InputListener *listener)
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
		InputListener *listener)
{
	class InputAction *action;

	RightStripW(keyName);

	action = NewVKInputAction(keyName, listener);
	if (!action)
		action = NewXInputAction(keyName, listener);

	if (action) {
		LogInfoW(L"  %s=%s\n", iniKey, keyName);
		actions.push_back(action);
	} else {
		LogInfoW(L"  WARNING: UNABLE TO PARSE KEY BINDING %s=%s\n", iniKey, keyName);
	}
}

void RegisterIniKeyBinding(LPCWSTR app, LPCWSTR key, LPCWSTR ini,
		InputCallback down_cb, InputCallback up_cb,
		void *private_data)
{
	InputCallbacks *callbacks = new InputCallbacks(down_cb, up_cb, private_data);
	wchar_t buf[MAX_PATH];

	if (!GetPrivateProfileString(app, key, 0, buf, MAX_PATH, ini))
		return;

	return RegisterKeyBinding(key, buf, callbacks);
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
