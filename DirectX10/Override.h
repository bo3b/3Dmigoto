#pragma once

#include <DirectXMath.h>
#include "input.h"
#include "Main.h"
#include <vector>
#include "../util.h"

enum class KeyOverrideType {
	INVALID = -1,
	ACTIVATE,
	HOLD,
	TOGGLE,
	CYCLE,
};
static EnumName_t<wchar_t *, KeyOverrideType> KeyOverrideTypeNames[] = {
	{L"activate", KeyOverrideType::ACTIVATE},
	{L"hold", KeyOverrideType::HOLD},
	{L"toggle", KeyOverrideType::TOGGLE},
	{L"cycle", KeyOverrideType::CYCLE},
	{NULL, KeyOverrideType::INVALID} // End of list marker
};

enum class TransitionType {
	INVALID = -1,
	LINEAR,
	COSINE,
};
static EnumName_t<wchar_t *, TransitionType> TransitionTypeNames[] = {
	{L"linear", TransitionType::LINEAR},
	{L"cosine", TransitionType::COSINE},
	{NULL, TransitionType::INVALID} // End of list marker
};

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
	TransitionType transition_type, release_transition_type;

public:
	DirectX::XMFLOAT4 mOverrideParams;
	float mOverrideSeparation;
	float mOverrideConvergence;

	DirectX::XMFLOAT4 mSavedParams;
	float mUserSeparation;
	float mUserConvergence;

	Override();
	Override(float x, float y, float z, float w, float separation,
		 float convergence, int transition, int release_transition,
		 TransitionType transition_type,
		 TransitionType release_transition_type) :
		mOverrideParams({x, y, z, w}),
		mOverrideSeparation(separation),
		mOverrideConvergence(convergence),
		transition(transition),
		release_transition(release_transition),
		transition_type(transition_type),
		release_transition_type(release_transition_type)
	{}

	void ParseIniSection(LPCWSTR section, LPCWSTR ini) override;

	void Activate(D3D10Base::ID3D10Device *device);
	void Deactivate(D3D10Base::ID3D10Device *device);
	void Toggle(D3D10Base::ID3D10Device *device);
};

class KeyOverrideBase : public virtual OverrideBase, public InputListener
{
};

class KeyOverride : public KeyOverrideBase, public Override
{
private:
	KeyOverrideType type;

public:
	KeyOverride(KeyOverrideType type) :
		Override(),
		type(type)
	{}
	KeyOverride(KeyOverrideType type, float x, float y, float z,
			float w, float separation, float convergence,
			int transition, int release_transition,
			TransitionType transition_type,
			TransitionType release_transition_type) :
		Override(x, y, z, w, separation, convergence, transition,
				release_transition, transition_type,
				release_transition_type),
		type(type)
	{}

	void DownEvent(D3D10Base::ID3D10Device *device);
	void UpEvent(D3D10Base::ID3D10Device *device);
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
	void DownEvent(D3D10Base::ID3D10Device *device);
};

struct OverrideTransitionParam
{
	float start;
	float target;
	ULONGLONG activation_time;
	int time;
	TransitionType transition_type;

	OverrideTransitionParam() :
		start(FLT_MAX),
		target(FLT_MAX),
		activation_time(0),
		time(-1),
		transition_type(TransitionType::LINEAR)
	{}
};

class OverrideTransition
{
public:
	OverrideTransitionParam x, y, z, w, separation, convergence;

	void ScheduleTransition(D3D10Base::ID3D10Device *device,
			float target_separation, float target_convergence,
			float target_x, float target_y, float target_z,
			float target_w, int time, TransitionType transition_type);
	void OverrideTransition::UpdateTransitions(D3D10Base::ID3D10Device *device);
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
	OverrideGlobalSaveParam();

	float Reset();
	void Save(float val);
	void Restore(float *val);
};

class OverrideGlobalSave
{
public:
	OverrideGlobalSaveParam x, y, z, w, separation, convergence;

	void Reset(D3D10Wrapper::ID3D10Device* wrapper);
	void Save(D3D10Base::ID3D10Device *device, Override *preset);
	void Restore(Override *preset);
};

// We only use a single transition instance to simplify the edge cases and
// answer what happens when we have overlapping transitions - there can only be
// one active transition for each parameter.
extern OverrideTransition CurrentTransition;
extern OverrideGlobalSave OverrideSave;
