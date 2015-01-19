#include "input.h"

#include "Main.h"
#include "../log.h"
#include "../util.h"
#include <vector>
#include "vkeys.h"

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
	InputCallback down_cb;
	InputCallback up_cb;
	void *private_data;

	InputAction(InputCallback down_cb,
			InputCallback up_cb,
			void *private_data) :
		last_state(false),
		down_cb(down_cb),
		up_cb(up_cb),
		private_data(private_data)
	{}

	virtual bool CheckState()
	{
		return false;
	}

	bool Dispatch(D3D11Base::ID3D11Device *device)
	{
		bool state = CheckState();

		if (state == last_state)
			return false;

		if (state) {
			if (down_cb)
				down_cb(device, private_data);
		} else {
			if (up_cb)
				up_cb(device, private_data);
		}

		last_state = state;

		return true;
	}
};

class VKInputAction : public InputAction {
public:
	int vkey;

	VKInputAction(int vkey,
			InputCallback down_cb,
			InputCallback up_cb,
			void *private_data) :
		InputAction(down_cb, up_cb, private_data),
		vkey(vkey)
	{}

	bool CheckState()
	{
		return (!!GetAsyncKeyState(vkey));
	}
};

static std::vector<class InputAction *> actions;

void RegisterIniKeyBinding(LPCWSTR app, LPCWSTR key, LPCWSTR ini,
		InputCallback down_cb, InputCallback up_cb,
		void *private_data, FILE *log_file)
{
	class InputAction *action;
	wchar_t buf[MAX_PATH];
	int vkey;

	if (!GetPrivateProfileString(app, key, 0, buf, MAX_PATH, ini))
		return;

	RightStripW(buf);
	vkey = ParseVKey(buf);
	if (vkey < 0) {
		LogInfoW(L"  WARNING: UNABLE TO PARSE KEY BINDING %s=%s\n", key, buf);
		return;
	}
	action = new VKInputAction(vkey, down_cb, up_cb, private_data);
	actions.push_back(action);

	LogInfoW(L"  %s=%s\n", key, buf);
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
