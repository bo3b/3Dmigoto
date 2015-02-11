#include "input.h"

#include "Main.h"
#include "../log.h"
#include "../util.h"
#include <vector>
#include "vkeys.h"


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
// The VKRepeatAction is to allow for auto-repeat on hunting operations.
// Regular user inputs, and not all hunting operations are suitable for auto-
// repeat.  These are only created for operations that desire auto-repeat,
// otherwise the VKInputAction is used.

// For Dispatch, we have no need to be called as often as we are, that's just 
// an artifact of where we get processing time, from the Draw() calls made by the game.
// To trim this down to a sensible human-oriented, keyboard input type time, we'll
// use the GetTickCount64 to skip processing.  The reason to add this limiter is
// to make auto-repeat slow enough to be usable, and consistent.

// TODO: Determine if an alternate thread can properly provide time. That would make
// it possible to simply have the OS call us as desired.

VKRepeatingInputAction::VKRepeatingInputAction(int vkey, int repeat, InputListener *listener) :
VKInputAction(vkey, listener),
repeatRate(repeat)
{}

bool VKRepeatingInputAction::Dispatch(D3D11Base::ID3D11Device *device)
{
	int ms = (1000 / repeatRate);
	if (GetTickCount64() < (lastTick + ms))
		return false;
	lastTick = GetTickCount64();

	bool state = CheckState();

	// Only allow auto-repeat for down events.
	if (state || (state != last_state))
	{
		if (state)
			listener->DownEvent(device);
		else
			listener->UpEvent(device);

		last_state = state;

		return true;
	}

	return false;
}


// -----------------------------------------------------------------------------

static std::vector<class InputAction *> actions;

int ConvertKeyName(LPCWSTR iniKey, wchar_t * keyName)
{
	RightStripW(keyName);
	int vkey = ParseVKey(keyName);
	if (vkey < 0)
		LogInfoW(L"  WARNING: UNABLE TO PARSE KEY BINDING %s=%s\n", iniKey, keyName);

	return vkey;
}

void RegisterKeyBinding(LPCWSTR iniKey, wchar_t *keyName,
		InputListener *listener)
{
	class InputAction *action;
	int vkey;

	vkey = ConvertKeyName(iniKey, keyName);
	action = new VKInputAction(vkey, listener);
	actions.push_back(action);

	// Log key settings e.g. next_pixelshader=VK_NUMPAD2
	LogInfoW(L"  %s=%s\n", iniKey, keyName);
}

void RegisterIniKeyBinding(LPCWSTR app, LPCWSTR iniKey, LPCWSTR ini,
		InputCallback down_cb, InputCallback up_cb, int auto_repeat,
		void *private_data)
{
	InputCallbacks *callbacks = new InputCallbacks(down_cb, up_cb, private_data);
	wchar_t keyName[MAX_PATH];
	class InputAction *action;
	int vkey;

	if (!GetPrivateProfileString(app, iniKey, 0, keyName, MAX_PATH, ini))
		return;

	if (auto_repeat) {
		vkey = ConvertKeyName(iniKey, keyName);
		action = new VKRepeatingInputAction(vkey, auto_repeat, callbacks);
		actions.push_back(action);

		// Log key settings e.g. next_pixelshader=VK_NUMPAD2
		LogInfoW(L"  %s=%s\n", iniKey, keyName);
	}
	else {
		RegisterKeyBinding(iniKey, keyName, callbacks);
	}
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

	for (i = actions.begin(); i != actions.end(); i++) {
		action = *i;

		input_processed |= action->Dispatch(device);
	}

	return input_processed;
}
