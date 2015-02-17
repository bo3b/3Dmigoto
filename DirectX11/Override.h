#pragma once

#include <DirectXMath.h>
#include "input.h"
#include "Main.h"
#include <vector>

class OverrideBase
{
public:
	virtual void ParseIniSection(LPCWSTR section, LPCWSTR ini) = 0;
};

class Override : public virtual OverrideBase
{
private:
	bool active;
	int transition, release_transition;

public:
	DirectX::XMFLOAT4 mOverrideParams;
	float mOverrideSeparation;
	float mOverrideConvergence;

	DirectX::XMFLOAT4 mSavedParams;
	float mUserSeparation;
	float mUserConvergence;

	Override();
	Override(float x, float y, float z, float w, float separation,
		 float convergence, int transition, int release_transition) :
		mOverrideParams({x, y, z, w}),
		mOverrideSeparation(separation),
		mOverrideConvergence(convergence),
		transition(transition),
		release_transition(release_transition)
	{}

	void ParseIniSection(LPCWSTR section, LPCWSTR ini) override;

	void Activate(D3D11Base::ID3D11Device *device);
	void Deactivate(D3D11Base::ID3D11Device *device);
	void Toggle(D3D11Base::ID3D11Device *device);
};

enum KeyOverrideType {
	ACTIVATE,
	HOLD,
	TOGGLE,
	CYCLE,
};

class KeyOverrideBase : public virtual OverrideBase, public InputListener
{
};

class KeyOverride : public KeyOverrideBase, public Override
{
private:
	enum KeyOverrideType type;

public:
	KeyOverride(enum KeyOverrideType type) :
		Override(),
		type(type)
	{}
	KeyOverride(enum KeyOverrideType type, float x, float y, float z,
			float w, float separation, float convergence,
			int transition, int release_transition) :
		Override(x, y, z, w, separation, convergence, transition, release_transition),
		type(type)
	{}

	void DownEvent(D3D11Base::ID3D11Device *device);
	void UpEvent(D3D11Base::ID3D11Device *device);
#pragma warning(suppress : 4250) // Suppress ParseIniSection inheritance via dominance warning
};

class KeyOverrideCycle : public KeyOverrideBase
{
private:
	std::vector<class KeyOverride> presets;
	int current;
public:
	KeyOverrideCycle() :
		current(0)
	{}

	void ParseIniSection(LPCWSTR section, LPCWSTR ini) override;
	void DownEvent(D3D11Base::ID3D11Device *device);
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
public:
	OverrideTransitionParam x, y, z, w, separation, convergence;

	void ScheduleTransition(D3D11Base::ID3D11Device *device,
			float target_separation, float target_convergence,
			float target_x, float target_y, float target_z,
			float target_w, int time);
	void OverrideTransition::UpdateTransitions(D3D11Base::ID3D11Device *device);
};

// This struct + class provides a global save for each of the overridable
// parameters. It is used to ensure that after all toggle and hold type
// bindings are released that the final value that is restored matches the
// original value. The local saves in each individual override do not guarantee
// this.
class OverrideGlobalSaveParam
{
private:
	float save;
	int refcount;
public:
	void Reset();
	void Save(float val);
	void Restore(float *val);
};

class OverrideGlobalSave
{
public:
	OverrideGlobalSaveParam x, y, z, w, separation, convergence;
	OverrideGlobalSave();

	void Reset();
	void Save(D3D11Base::ID3D11Device *device, Override *preset);
	void Restore(Override *preset);
};

// We only use a single transition instance to simplify the edge cases and
// answer what happens when we have overlapping transitions - there can only be
// one active transition for each parameter.
extern OverrideTransition CurrentTransition;
extern OverrideGlobalSave OverrideSave;
