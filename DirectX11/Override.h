#pragma once

#include <DirectXMath.h>
#include "Main.h"

class Override
{
private:
	DirectX::XMFLOAT4 mOverrideParams;
	float mOverrideSeparation;
	float mOverrideConvergence;

	float mUserSeparation;
	float mUserConvergence;

	bool active;

public:
	Override();

	void ParseIniSection(LPCWSTR section, LPCWSTR ini);

	void Activate(D3D11Base::ID3D11Device *device);
	void Deactivate(D3D11Base::ID3D11Device *device);
	void Toggle(D3D11Base::ID3D11Device *device);

	// Static proxy functions that can be called from the input subsystem
	// to get around the C++ limitation where a pointer to a bound member
	// function cannot be used for a callback directly - the instance is in
	// private_data. TODO: Consider a redesign using an OO interface
	// instead of callbacks.
	static void ActivateCallBack(D3D11Base::ID3D11Device *device, void *private_data);
	static void DeactivateCallBack(D3D11Base::ID3D11Device *device, void *private_data);
	static void ToggleCallBack(D3D11Base::ID3D11Device *device, void *private_data);
};

