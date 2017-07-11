#pragma once

#include <DirectXMath.h>
#include <d3d11.h>
#include <vector>

#include "util.h"
#include "Input.h"
#include "HackerDevice.h"

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

	bool is_conditional;
	int condition_param_idx;
	float DirectX::XMFLOAT4::*condition_param_component;

public:
	DirectX::XMFLOAT4 mOverrideParams[INI_PARAMS_SIZE];
	float mOverrideSeparation;
	float mOverrideConvergence;

	DirectX::XMFLOAT4 mSavedParams[INI_PARAMS_SIZE];
	float mUserSeparation;
	float mUserConvergence;

	Override();
	Override(DirectX::XMFLOAT4 *params, float separation,
		 float convergence, int transition, int release_transition,
		 TransitionType transition_type,
		 TransitionType release_transition_type,
		 bool is_conditional, int condition_param_idx,
		 float DirectX::XMFLOAT4::*condition_param_component) :
		mOverrideSeparation(separation),
		mOverrideConvergence(convergence),
		transition(transition),
		release_transition(release_transition),
		transition_type(transition_type),
		release_transition_type(release_transition_type),
		is_conditional(is_conditional),
		condition_param_idx(condition_param_idx),
		condition_param_component(condition_param_component)
	{
		memcpy(&mOverrideParams, params, sizeof(DirectX::XMFLOAT4[INI_PARAMS_SIZE]));
	}

	void ParseIniSection(LPCWSTR section, LPCWSTR ini) override;

	void Activate(HackerDevice *device);
	void Deactivate(HackerDevice *device);
	void Toggle(HackerDevice *device);
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
	KeyOverride(KeyOverrideType type, DirectX::XMFLOAT4 *params,
			float separation, float convergence,
			int transition, int release_transition,
			TransitionType transition_type,
			TransitionType release_transition_type,
			bool is_conditional, int condition_param_idx,
			float DirectX::XMFLOAT4::*condition_param_component) :
		Override(params, separation, convergence, transition,
				release_transition, transition_type,
				release_transition_type, is_conditional,
				condition_param_idx,
				condition_param_component),
		type(type)
	{}

	void DownEvent(HackerDevice *device);
	void UpEvent(HackerDevice *device);
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
	void DownEvent(HackerDevice *device);
};

class PresetOverride : public Override
{
private:
	bool activated;
public:
	PresetOverride() :
		Override(),
		activated(false)
	{}
	PresetOverride(DirectX::XMFLOAT4 *params,
			float separation, float convergence,
			int transition, int release_transition,
			TransitionType transition_type,
			TransitionType release_transition_type,
			bool is_conditional, int condition_param_idx,
			float DirectX::XMFLOAT4::*condition_param_component) :
		Override(params, separation, convergence, transition,
				release_transition, transition_type,
				release_transition_type, is_conditional,
				condition_param_idx,
				condition_param_component),
		activated(false)
	{}

	void Activate(HackerDevice *device, PresetOverride *prev);
	void Deactivate(HackerDevice *device);
	bool IsActivated();
};
typedef std::unordered_map<std::wstring, class PresetOverride> PresetOverrideMap;
extern PresetOverrideMap presetOverrides;

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
	OverrideTransitionParam x[INI_PARAMS_SIZE], y[INI_PARAMS_SIZE];
	OverrideTransitionParam z[INI_PARAMS_SIZE], w[INI_PARAMS_SIZE];
	OverrideTransitionParam separation, convergence;
	std::wstring active_preset;

	void ScheduleTransition(HackerDevice *wrapper,
			float target_separation, float target_convergence,
			DirectX::XMFLOAT4 *targets,
			int time, TransitionType transition_type);
	void UpdatePresets(HackerDevice *wrapper);
	void OverrideTransition::UpdateTransitions(HackerDevice *wrapper);
	void Stop();
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
	OverrideGlobalSaveParam x[INI_PARAMS_SIZE], y[INI_PARAMS_SIZE];
	OverrideGlobalSaveParam z[INI_PARAMS_SIZE], w[INI_PARAMS_SIZE];
	OverrideGlobalSaveParam separation, convergence;

	void Reset(HackerDevice* wrapper);
	void Save(HackerDevice *wrapper, Override *preset);
	void Restore(Override *preset);
};

// We only use a single transition instance to simplify the edge cases and
// answer what happens when we have overlapping transitions - there can only be
// one active transition for each parameter.
extern OverrideTransition CurrentTransition;
extern OverrideGlobalSave OverrideSave;
