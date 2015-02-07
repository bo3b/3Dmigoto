#pragma once

#include "Main.h"

// InputListener defines an interface for receiving key events. Subclass it and
// override the DownEvent and/or UpEvent methods to add input handling to
// another object, and register that object to a key binding with
// RegisterKeyBinding(). e.g. the KeyOverride class implements this interface.
// Once registered, the input subsystem will be responsible for freeing it
// (though at the moment the input events are never freed).

class InputListener {
public:
	virtual void DownEvent(D3D11Base::ID3D11Device *device) {}
	virtual void UpEvent(D3D11Base::ID3D11Device *device) {}
};

// For cases where implementing InputListener is overkill (e.g. when a key
// binding's handler does not need any instance specific information), a
// callback function may be used with this type signature and registered via
// RegisterIniKeyBinding:

typedef void (*InputCallback)(D3D11Base::ID3D11Device *device, void *private_data);

// At the moment RegisterKeyBinding takes a class implementing InputListener,
// while RegisterIniKeyBinding takes a pair of callbacks and private_data.
// Right now these are the only two combinations we need, but free to add more
// variants as needed.

void RegisterKeyBinding(LPCWSTR iniKey, wchar_t *keyName,
		InputListener *listener);
void RegisterIniKeyBinding(LPCWSTR app, LPCWSTR key, LPCWSTR ini,
		InputCallback down_cb, InputCallback up_cb,
		void *private_data);

// Clears all current key bindings in preparation for reloading the config.
// Note - this is not safe to call from within an input callback!
void ClearKeyBindings();

bool DispatchInputEvents(D3D11Base::ID3D11Device *device);
