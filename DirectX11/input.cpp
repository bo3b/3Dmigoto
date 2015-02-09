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

bool VKInputAction::CheckState()
{
	return (GetAsyncKeyState(vkey) < 0);
}


// -----------------------------------------------------------------------------

static std::vector<class InputAction *> actions;

void RegisterKeyBinding(LPCWSTR iniKey, wchar_t *keyName,
		InputListener *listener)
{
	class InputAction *action;
	int vkey;

	RightStripW(keyName);
	vkey = ParseVKey(keyName);
	if (vkey < 0) {
		LogInfoW(L"  WARNING: UNABLE TO PARSE KEY BINDING %s=%s\n", iniKey, keyName);
		return;
	}
	action = new VKInputAction(vkey, listener);
	actions.push_back(action);

	LogInfoW(L"  %s=%s\n", iniKey, keyName);
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

	for (i = actions.begin(); i != actions.end(); i++) {
		action = *i;

		input_processed |= action->Dispatch(device);
	}

	return input_processed;
}
