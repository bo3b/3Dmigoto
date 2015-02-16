#pragma once

#include <DirectXMath.h>
#include "input.h"
#include "Main.h"

class Override
{
private:
	bool active;
	int transition, releasetransition;

public:
	DirectX::XMFLOAT4 mOverrideParams;
	float mOverrideSeparation;
	float mOverrideConvergence;

	DirectX::XMFLOAT4 mSavedParams;
	float mUserSeparation;
	float mUserConvergence;

	Override();

	void ParseIniSection(LPCWSTR section, LPCWSTR ini);

	void Activate(D3D11Base::ID3D11Device *device);
	void Deactivate(D3D11Base::ID3D11Device *device);
	void Toggle(D3D11Base::ID3D11Device *device);
	void Save(D3D11Base::ID3D11Device *device);
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

struct OverrideTransitionParam
{
	float start;
	float target;
	ULONGLONG activation_time;
	int time;

	OverrideTransitionParam() :
		start(FLT_MAX),
		target(FLT_MAX),
		activation_time(0),
		time(-1)
	{}
};

class OverrideTransition
{
private:
	OverrideTransitionParam x, y, z, w, separation, convergence;
public:
	void ScheduleTransition(D3D11Base::ID3D11Device *device,
			float target_separation, float target_convergence,
			float target_x, float target_y, float target_z,
			float target_w, int time);
	void OverrideTransition::UpdateTransitions(D3D11Base::ID3D11Device *device);
};

// We only use a single transition instance to simplify the edge cases and
// answer what happens when we have overlapping transitions - there can only be
// one active transition for each parameter.
extern OverrideTransition CurrentTransition;
