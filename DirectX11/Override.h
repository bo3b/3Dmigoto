#pragma once

#include <DirectXMath.h>
#include "input.h"
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
};

enum KeyOverrideType {
	ACTIVATE,
	HOLD,
	TOGGLE,
};

class KeyOverride : public InputListener, public Override
{
private:
	enum KeyOverrideType type;

public:
	KeyOverride(enum KeyOverrideType type) :
		Override(),
		type(type)
	{}

	void DownEvent(D3D11Base::ID3D11Device *device);
	void UpEvent(D3D11Base::ID3D11Device *device);
};
