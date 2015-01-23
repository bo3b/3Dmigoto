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

	D3D11Wrapper::ID3D11Device *mWrappedDevice;

public:
	Override(D3D11Wrapper::ID3D11Device *device);

	void Activate();
	void Deactivate();
};

