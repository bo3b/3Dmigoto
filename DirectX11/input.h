#pragma once

#include "Main.h"

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
	virtual void DownEvent(D3D11Base::ID3D11Device *device) =0;
	virtual void UpEvent(D3D11Base::ID3D11Device *device) =0;
};


// -----------------------------------------------------------------------------
// For cases where implementing InputListener is overkill (e.g. when a key
// binding's handler does not need any instance specific information), a
// callback function may be used with this type signature and registered via
// RegisterIniKeyBinding:

typedef void(*InputCallback)(D3D11Base::ID3D11Device *device, void *private_data);

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

	void DownEvent(D3D11Base::ID3D11Device *device) override;
	void UpEvent(D3D11Base::ID3D11Device *device) override;
};


// -----------------------------------------------------------------------------
// I'm using inheritance here because if we wanted to add another input backend
// in the future this is where I see the logical split between common code and
// input backend specific code (we would still need to add an abstraction of
// the backends themselves). 
//
// One reason we might want to considder this is to
// re-add the ability to use gamepads or other input devices which was removed
// when we pulled out the problematic DirectInput support. I'm not concerned
// about supporting gamepads for hunting, but we might want it for e.g.
// convergence overrides with the aim button on a controller in a FPS.
// At present we can require users to use XPadder to generate keypresses.

class InputAction {
public:
	bool last_state;
	InputListener *listener;

	InputAction(InputListener *listener);
	~InputAction();

	virtual bool CheckState() = 0;
	virtual bool Dispatch(D3D11Base::ID3D11Device *device);
};


// -----------------------------------------------------------------------------
// VKInputAction is the primary object used for game input from the users, for
// changing separation/convergence/iniParams.
//
// The keybindings are oriented around the use of GetAsyncKeyState, and numerous
// convenience aliases are defined in vkeys.h.

class VKInputAction : public virtual InputAction {
public:
	int vkey;

	VKInputAction(int vkey, InputListener *listener);
	bool CheckState() override;
};

// -----------------------------------------------------------------------------
// XInputAction serves much the same purpose as VKInputAction, but implements
// XInput to support xbox controllers
class XInputAction : public virtual InputAction {
private:
	int controller;
	WORD button;
	BYTE left_trigger;
	BYTE right_trigger;

	bool _CheckState(int controller);
public:
	XInputAction(int controller, WORD button, BYTE left_trigger,
			BYTE right_trigger, InputListener *listener);
	bool CheckState() override;
};


// -----------------------------------------------------------------------------
// RepeatingInputAction is used to provide auto-repeating functionality to
// other key bindings.

class RepeatingInputAction : public virtual InputAction {
private:
	int repeatRate = 8;			// repeats per second
	ULONGLONG lastTick = 0;

public:
	RepeatingInputAction(int repeat, InputListener *listener);
	bool Dispatch(D3D11Base::ID3D11Device *device) override;
};

// -----------------------------------------------------------------------------
// Classes using multiple inheritance to add auto-repeat functionality to
// VKInputAction and XInputAction
class VKRepeatingInputAction : public VKInputAction, public RepeatingInputAction {
public:
	VKRepeatingInputAction(int vkey, int repeat, InputListener *listener);
	bool Dispatch(D3D11Base::ID3D11Device *device);
	bool CheckState(); // Only necessary to silence warning from MSVC bug
};
class XRepeatingInputAction : public XInputAction, public RepeatingInputAction {
public:
	XRepeatingInputAction(int controller, WORD button, BYTE left_trigger,
		BYTE right_trigger, int repeat, InputListener *listener);
	bool Dispatch(D3D11Base::ID3D11Device *device);
	bool CheckState(); // Only necessary to silence warning from MSVC bug
};


// -----------------------------------------------------------------------------
// At the moment RegisterKeyBinding takes a class implementing InputListener,
// while RegisterIniKeyBinding takes a pair of callbacks and private_data.
// Right now these are the only two combinations we need, but feel free to add 
// more variants as needed.

void RegisterKeyBinding(LPCWSTR iniKey, wchar_t *keyName,
		InputListener *listener, int auto_repeat);
void RegisterIniKeyBinding(LPCWSTR app, LPCWSTR key, LPCWSTR ini,
		InputCallback down_cb, InputCallback up_cb, int auto_repeat,
		void *private_data);

// Clears all current key bindings in preparation for reloading the config.
// Note - this is not safe to call from within an input callback!
void ClearKeyBindings();

bool DispatchInputEvents(D3D11Base::ID3D11Device *device);
