#include "Input.h"

#include <Xinput.h>
#include <vector>
#include <algorithm>

#include "log.h"
#include "util.h"
#include "vkeys.h"

// Set a function pointer to the xinput get state call. By default, set it to
// XInputGetState() in whichever xinput we are linked to (xinput9_1_0.dll). If
// the d3dx.ini is using the guide button we will try to switch to either
// xinput 1.3 or 1.4 to get access to the undocumented XInputGetStateEx() call.
// We can't rely on these existing on Win7 though, so if we fail to load them
// don't treat it as fatal and continue using the original one.
static HMODULE xinput_lib;
typedef DWORD (WINAPI *tXInputGetState)(DWORD dwUserIndex, XINPUT_STATE* pState);
static tXInputGetState _XInputGetState = XInputGetState;

static void SwitchToXinpuGetStateEx()
{
	tXInputGetState XInputGetStateEx;

	if (xinput_lib)
		return;

	// 3DMigoto is linked against xinput9_1_0.dll, but that version does
	// not export XInputGetStateEx to get the guide button. Try loading
	// xinput 1.3 and 1.4, which both support this functionality.
	xinput_lib = LoadLibrary(L"xinput1_3.dll");
	if (xinput_lib) {
		LogInfo("Loaded xinput1_3.dll for guide button support\n");
	} else {
		xinput_lib = LoadLibrary(L"xinput1_4.dll");
		if (xinput_lib) {
			LogInfo("Loaded xinput1_4.dll for guide button support\n");
		} else {
			LogInfo("ERROR: Unable to load xinput 1.3 or 1.4: Guide button will not be available\n");
			return;
		}
	}

	// Unnamed and undocumented exports FTW
	LPCSTR XInputGetStateExOrdinal = (LPCSTR)100;
	XInputGetStateEx = (tXInputGetState)GetProcAddress(xinput_lib, XInputGetStateExOrdinal);
	if (!XInputGetStateEx) {
		LogInfo("ERROR: Unable to get XInputGetStateEx: Guide button will not be available\n");
		return;
	}

	_XInputGetState = XInputGetStateEx;
}

class KeyParseError: public exception {} keyParseError;

void InputListener::UpEvent(HackerDevice *device)
{
}

// -----------------------------------------------------------------------------

InputCallbacks::InputCallbacks(InputCallback down_cb, InputCallback up_cb,
		void *private_data) :
	down_cb(down_cb),
	up_cb(up_cb),
	private_data(private_data)
{}

void InputCallbacks::DownEvent(HackerDevice *device)
{
	if (down_cb)
		return down_cb(device, private_data);
}

void InputCallbacks::UpEvent(HackerDevice *device)
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

bool InputAction::Dispatch(HackerDevice *device)
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

VKInputButton::VKInputButton(const wchar_t *keyName) :
	invert(false)
{
	if (!_wcsnicmp(keyName, L"no_", 3)) {
		invert = true;
		keyName += 3;
	}

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
	return ((GetAsyncKeyState(vkey) < 0) ^ invert);
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

bool RepeatingInputAction::Dispatch(HackerDevice *device)
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

bool DelayedInputAction::Dispatch(HackerDevice *device)
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
		return false; // Don't invert if it's not connected

	if (button && (gamepad->wButtons & button))
		return true ^ invert;
	if (left_trigger && (gamepad->bLeftTrigger >= left_trigger))
		return true ^ invert;
	if (right_trigger && (gamepad->bRightTrigger >= right_trigger))
		return true ^ invert;

	return false ^ invert;
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
	{L"GUIDE", 0x400}, /* Requires undocumented XInputGetStateEx call in xinput 1.3 / 1.4 */
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
XInputButton::XInputButton(const wchar_t *keyName) :
	controller(-1),
	button(0),
	left_trigger(0),
	right_trigger(0),
	invert(false)
{
	int i, threshold = XINPUT_GAMEPAD_TRIGGER_THRESHOLD;
	BYTE *trigger;

	if (!_wcsnicmp(keyName, L"no_", 3)) {
		invert = true;
		keyName += 3;
	}

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

	if (!_wcsicmp(keyName, L"GUIDE"))
		SwitchToXinpuGetStateEx();

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

	*trigger = min(threshold + 1, 255);
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

InputButtonList::InputButtonList(wchar_t *keyName)
{
	wchar_t *ptr = keyName, *cur = NULL;
	wstring cur_key;

	while (*ptr) {
		// Skip over whitespace:
		for (; *ptr == L' '; ptr++) {}

		// Mark start of current entry:
		cur = ptr;

		// Scan until the next whitespace or end of string:
		for (; *ptr && *ptr != L' '; ptr++) {}

		// Copy the current entry to a new string (don't modify the
		// string passed from the caller so it can still log properly)
		// and advance pointer unless it is the end of string:
		cur_key = wstring(cur, ptr - cur);
		if (*ptr)
			ptr++;

		// Special case: "no_modifiers" is expanded to exclude all modifiers:
		if (!_wcsicmp(cur_key.c_str(), L"no_modifiers")) {
			buttons.push_back(new VKInputButton(L"NO_CTRL"));
			buttons.push_back(new VKInputButton(L"NO_ALT"));
			buttons.push_back(new VKInputButton(L"NO_SHIFT"));
			// Ctrl, Alt & Shift have left/right independent
			// variants, but Win does not, exclude both:
			buttons.push_back(new VKInputButton(L"NO_LWIN"));
			buttons.push_back(new VKInputButton(L"NO_RWIN"));
		} else {
			try {
				buttons.push_back(new VKInputButton(cur_key.c_str()));
			} catch (KeyParseError) {
				try {
					buttons.push_back(new XInputButton(cur_key.c_str()));
				} catch (KeyParseError) {
					goto fail;
				}
			}
		}
	}

	if (buttons.empty())
		throw keyParseError;

	return;
fail:
	clear();
	throw keyParseError;
}

void InputButtonList::clear()
{
	vector<InputButton*>::iterator i;

	for (i = buttons.begin(); i < buttons.end(); i++)
		delete *i;

	buttons.clear();
}

InputButtonList::~InputButtonList()
{
	clear();
}

bool InputButtonList::CheckState()
{
	vector<InputButton*>::iterator i;

	for (i = buttons.begin(); i < buttons.end(); i++) {
		if (!(*i)->CheckState())
			return false;
	}

	return true;
}

static std::vector<class InputAction *> actions;

void RegisterKeyBinding(LPCWSTR iniKey, wchar_t *keyName,
		InputListener *listener, int auto_repeat, int down_delay,
		int up_delay)
{
	class InputAction *action;
	class InputButton *button;

	RightStripW(keyName);

	// We could potentially only use the InputButtonList here, but that
	// does not work with some of our backwards compatibility key names
	// that contain spaces ("Num blah", "Prnt Scrn"), so we still try to
	// parse the keyName as a single key first.
	try {
		button = new VKInputButton(keyName);
	} catch (KeyParseError) {
		try {
			button = new XInputButton(keyName);
		} catch (KeyParseError) {
			try {
				button = new InputButtonList(keyName);
			} catch (KeyParseError) {
				LogInfoW(L"  WARNING: UNABLE TO PARSE KEY BINDING %s=%s\n",
						iniKey, keyName);
				BeepFailure2();
				return;
			}
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


bool DispatchInputEvents(HackerDevice *device)
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

		XInputState[j].connected =
			(_XInputGetState(j, &XInputState[j].state) == ERROR_SUCCESS);
	}

	last_time = now;

	for (i = actions.begin(); i != actions.end(); i++) {
		action = *i;

		input_processed |= action->Dispatch(device);
	}

	return input_processed;
}
