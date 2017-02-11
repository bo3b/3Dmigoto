#pragma once

#include "HackerDevice.h"

// The "input" files are a set of objects to handle user input for both gaming 
// purposes and for tool purposes, like hunting for shaders.
//
// The hunting usage is presently built using a call-back mechanism, to call
// the older routines in the d3d11Wrapper.cpp to actually track and disable shaders.
//
// For gameplay purposes, like aiming override, HUD movement, menu adjustments
// based on keys- the VKInputAction objects are used.


// -----------------------------------------------------------------------------
// InputListener defines an abstract interface for receiving key events. Subclass 
// it and override the DownEvent and/or UpEvent methods to add input handling to
// another object, and register that object to a key binding with
// RegisterKeyBinding(). e.g. the KeyOverride class implements this interface.
// Once registered, the input subsystem will be responsible for freeing it
// (though at the moment the input events are never freed).

class InputListener {
public:
	virtual void DownEvent(HackerDevice *device) = 0;
	virtual void UpEvent(HackerDevice *device);
};


// -----------------------------------------------------------------------------
// For cases where implementing InputListener is overkill (e.g. when a key
// binding's handler does not need any instance specific information), a
// callback function may be used with this type signature and registered via
// RegisterIniKeyBinding:

typedef void(*InputCallback)(HackerDevice *device, void *private_data);

// -----------------------------------------------------------------------------
// InputCallbacks is a key descendant of InputListener, and is used primarily
// for the shader hunting mechanism.  
//
// TODO: remove use of callbacks by making them InputAction subclasses.

class InputCallbacks : public InputListener {
private:
	InputCallback down_cb;
	InputCallback up_cb;
	void *private_data;

public:
	InputCallbacks(InputCallback down_cb, InputCallback up_cb, void *private_data);

	void DownEvent(HackerDevice *device) override;
	void UpEvent(HackerDevice *device) override;
};


// -----------------------------------------------------------------------------
// Abstract base class of all input backend button classes
class InputButton {
public:
	virtual bool CheckState() = 0;
};

// -----------------------------------------------------------------------------
// VKInputButton is the primary object used for game input from the users, for
// changing separation/convergence/iniParams.
//
// The keybindings are oriented around the use of GetAsyncKeyState, and numerous
// convenience aliases are defined in vkeys.h.

class VKInputButton : public InputButton {
public:
	int vkey;
	bool invert;

	VKInputButton(const wchar_t *keyName);
	bool CheckState() override;
};

// -----------------------------------------------------------------------------
// XInputButton serves much the same purpose as VKInputButton, but implements
// XInputButton to support xbox controllers
class XInputButton : public InputButton {
private:
	int controller;
	WORD button;
	BYTE left_trigger;
	BYTE right_trigger;
	bool invert;

	bool _CheckState(int controller);
public:
	XInputButton(const wchar_t *keyName);
	bool CheckState() override;
};

// -----------------------------------------------------------------------------
// InputButtonList allows multiple InputButtons to be combined:
class InputButtonList : public InputButton {
	vector<InputButton*> buttons;
	void clear();

public:
	InputButtonList(wchar_t *keyName);
	~InputButtonList();
	bool CheckState() override;
};


// -----------------------------------------------------------------------------
// InputAction combines an InputButton and an InputListener together to create
// an action.

class InputAction {
public:
	bool last_state;
	InputButton *button;
	InputListener *listener;

	InputAction(InputButton *button, InputListener *listener);
	virtual ~InputAction();

	virtual bool Dispatch(HackerDevice *device);
};

// -----------------------------------------------------------------------------
// RepeatingInputAction is used to provide auto-repeating functionality to
// other key bindings.

class RepeatingInputAction : public virtual InputAction {
private:
	int repeatRate = 8;			// repeats per second
	ULONGLONG lastTick = 0;

public:
	RepeatingInputAction(InputButton *button, InputListener *listener, int repeat);
	bool Dispatch(HackerDevice *device) override;
};

// -----------------------------------------------------------------------------
// DelayedInputAction is used to add delays to the activation of other key
// bindings.
class DelayedInputAction : public virtual InputAction {
private:
	int delay_down, delay_up;
	bool effective_state;
	ULONGLONG state_change_time;
public:
	DelayedInputAction(InputButton *button, InputListener *listener, int delayDown, int delayUp);
	bool Dispatch(HackerDevice *device) override;
};


// -----------------------------------------------------------------------------
// At the moment RegisterKeyBinding takes a class implementing InputListener,
// while RegisterIniKeyBinding takes a pair of callbacks and private_data.
// Right now these are the only two combinations we need, but feel free to add 
// more variants as needed.

void RegisterKeyBinding(LPCWSTR iniKey, wchar_t *keyName,
		InputListener *listener, int auto_repeat, int down_delay,
		int up_delay);
bool RegisterIniKeyBinding(LPCWSTR app, LPCWSTR key, LPCWSTR ini,
		InputCallback down_cb, InputCallback up_cb, int auto_repeat,
		void *private_data);

// Clears all current key bindings in preparation for reloading the config.
// Note - this is not safe to call from within an input callback!
void ClearKeyBindings();

bool DispatchInputEvents(HackerDevice *device);
