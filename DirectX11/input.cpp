#include "input.h"

#include "Main.h"
#include "../log.h"
#include "../util.h"
#include <vector>
#include "vkeys.h"

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
// input backend specific code (we would still need to add an abstraction of
// the backends themselves). One reason we might want to considder this is to
// re-add the ability to use gamepads or other input devices which was removed
// when we pulled out the problematic DirectInput support. I'm not concerned
// about supporting gamepads for hunting, but we might want it for e.g.
// convergence overrides with the aim button on a controller in a FPS.

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
