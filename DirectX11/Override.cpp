#include "Override.h"

#include "Globals.h"
#include "D3D11Wrapper.h"

#define _USE_MATH_DEFINES
#include <math.h>
#include <strsafe.h>

PresetOverrideMap presetOverrides;

OverrideTransition CurrentTransition;
OverrideGlobalSave OverrideSave;

Override::Override()
{
	int i;

	// It's important for us to know if any are actively in use or not, so setting them
	// to FloatMax by default allows us to know when they are unused or invalid.
	// We are using FloatMax now instead of infinity, to avoid compiler warnings.
	for (i = 0; i < INI_PARAMS_SIZE; i++) {
		mOverrideParams[i].x = FLT_MAX;
		mOverrideParams[i].y = FLT_MAX;
		mOverrideParams[i].z = FLT_MAX;
		mOverrideParams[i].w = FLT_MAX;
		mSavedParams[i].x = FLT_MAX;
		mSavedParams[i].y = FLT_MAX;
		mSavedParams[i].z = FLT_MAX;
		mSavedParams[i].w = FLT_MAX;
	}
	mOverrideSeparation = FLT_MAX;
	mOverrideConvergence = FLT_MAX;

	mUserSeparation = FLT_MAX;
	mUserConvergence = FLT_MAX;

	is_conditional = false;
	condition_param_idx = 0;
	condition_param_component = NULL;

	active = false;
}

void Override::ParseIniSection(LPCWSTR section, LPCWSTR ini)
{
	wchar_t buf[MAX_PATH], param_name[8];
	int i;

	if (GetPrivateProfileString(section, L"separation", 0, buf, MAX_PATH, ini)) {
		swscanf_s(buf, L"%f", &mOverrideSeparation);
		LogInfo("  separation=%#.2f\n", mOverrideSeparation);
	}

	if (GetPrivateProfileString(section, L"convergence", 0, buf, MAX_PATH, ini)) {
		swscanf_s(buf, L"%f", &mOverrideConvergence);
		LogInfo("  convergence=%#.2f\n", mOverrideConvergence);
	}

	for (i = 0; i < INI_PARAMS_SIZE; i++) {
		StringCchPrintf(param_name, 8, L"x%.0i", i);
		if (GetPrivateProfileString(section, param_name, 0, buf, MAX_PATH, ini)) {
			swscanf_s(buf, L"%f", &mOverrideParams[i].x);
			LogInfoW(L"  %ls=%#.2g\n", param_name, mOverrideParams[i].x);
		}

		StringCchPrintf(param_name, 8, L"y%.0i", i);
		if (GetPrivateProfileString(section, param_name, 0, buf, MAX_PATH, ini)) {
			swscanf_s(buf, L"%f", &mOverrideParams[i].y);
			LogInfoW(L"  %ls=%#.2g\n", param_name, mOverrideParams[i].y);
		}

		StringCchPrintf(param_name, 8, L"z%.0i", i);
		if (GetPrivateProfileString(section, param_name, 0, buf, MAX_PATH, ini)) {
			swscanf_s(buf, L"%f", &mOverrideParams[i].z);
			LogInfoW(L"  %ls=%#.2g\n", param_name, mOverrideParams[i].z);
		}

		StringCchPrintf(param_name, 8, L"w%.0i", i);
		if (GetPrivateProfileString(section, param_name, 0, buf, MAX_PATH, ini)) {
			swscanf_s(buf, L"%f", &mOverrideParams[i].w);
			LogInfoW(L"  %ls=%#.2g\n", param_name, mOverrideParams[i].w);
		}
	}

	transition = GetPrivateProfileInt(section, L"transition", 0, ini);
	if (transition)
		LogInfo("  transition=%ims\n", transition);

	release_transition = GetPrivateProfileInt(section, L"release_transition", 0, ini);
	if (release_transition)
		LogInfo("  release_transition=%ims\n", release_transition);

	if (GetPrivateProfileString(section, L"transition_type", 0, buf, MAX_PATH, ini)) {
		transition_type = lookup_enum_val<wchar_t *, TransitionType>(TransitionTypeNames, buf, TransitionType::INVALID);
		if (transition_type == TransitionType::INVALID) {
			LogInfoW(L"WARNING: Invalid transition_type=\"%s\"\n", buf);
			transition_type = TransitionType::LINEAR;
			BeepFailure2();
		} else {
			LogInfoW(L"  transition_type=%s\n", buf);
		}
	}

	if (GetPrivateProfileString(section, L"release_transition_type", 0, buf, MAX_PATH, ini)) {
		release_transition_type = lookup_enum_val<wchar_t *, TransitionType>(TransitionTypeNames, buf, TransitionType::INVALID);
		if (release_transition_type == TransitionType::INVALID) {
			LogInfoW(L"WARNING: Invalid release_transition_type=\"%s\"\n", buf);
			release_transition_type = TransitionType::LINEAR;
			BeepFailure2();
		} else {
			LogInfoW(L"  release_transition_type=%s\n", buf);
		}
	}

	if (GetPrivateProfileString(section, L"condition", 0, buf, MAX_PATH, ini)) {
		// For now these conditions are just an IniParam being
		// non-zero. In the future we could implement more complicated
		// conditionals, perhaps even all the way up to a full logic
		// expression parser... but for now that's overkill, and
		// potentially something that might prove more useful to
		// implement in the command list along with flow control.

		if (!ParseIniParamName(buf, &condition_param_idx, &condition_param_component)) {
			LogInfoW(L"WARNING: Invalid condition=\"%s\"\n", buf);
			BeepFailure2();
		} else {
			is_conditional = true;
			LogInfoW(L"  condition=%s\n", buf);
		}
	}
}

struct KeyOverrideCycleParam
{
	wchar_t *cur;
	wchar_t buf[MAX_PATH];
	wchar_t *ptr;
	bool done;

	KeyOverrideCycleParam() :
		cur(L""),
		ptr(buf),
		done(false)
	{}

	bool next()
	{
		wchar_t *tmp;

		if (done)
			return false;

		// Skip over whitespace:
		for (; *ptr == L' '; ptr++) {}

		// Mark start of current entry:
		cur = ptr;

		// Scan until the next comma or end of string:
		for (; *ptr && *ptr != L','; ptr++) {}

		// Scan backwards over any trailing whitespace in this entry:
		for (tmp = ptr - 1; ptr >= cur && *ptr == L' '; ptr--) {}

		// If it's a comma advance to the next item, otherwise mark us as done:
		if (*ptr == L',')
			ptr++;
		else
			done = true;

		// NULL terminate this entry:
		*(tmp + 1) = L'\0';

		return true;
	}

	void log(wchar_t *name)
	{
		if (*cur)
			LogInfoW(L" %s=%s", name, cur);
	}

	float as_float(float default)
	{
		float val;
		int n;

		n = swscanf_s(cur, L"%f", &val);
		if (!n || n == EOF) {
			// Blank entry
			return default;
		}
		return val;
	}

	int as_int(int default)
	{
		int val;
		int n;

		n = swscanf_s(cur, L"%i", &val);
		if (!n || n == EOF) {
			// Blank entry
			return default;
		}
		return val;
	}

	template <class T1, class T2>
	T2 as_enum(EnumName_t<T1, T2> *enum_names, T2 default)
	{
		T2 val;

		if (*cur == L'\0') {
			// Blank entry
			return default;
		}

		val = lookup_enum_val<wchar_t *, TransitionType>(enum_names, cur, (T2)-1);
		if (val == (T2)-1) {
			LogInfoW(L"WARNING: Unmatched value \"%s\"\n", cur);
			BeepFailure2();
			return default;
		}

		return val;
	}

	bool as_ini_param(int *idx, float DirectX::XMFLOAT4::**component)
	{
		if (*cur == L'\0') {
			// Blank entry
			return false;
		}

		if (!ParseIniParamName(buf, idx, component)) {
			LogInfoW(L"WARNING: Invalid condition=\"%s\"\n", buf);
			BeepFailure2();
			return false;
		}

		return true;
	}
};

void KeyOverrideCycle::ParseIniSection(LPCWSTR section, LPCWSTR ini)
{
	struct KeyOverrideCycleParam x[INI_PARAMS_SIZE], y[INI_PARAMS_SIZE];
	struct KeyOverrideCycleParam z[INI_PARAMS_SIZE], w[INI_PARAMS_SIZE];
	struct KeyOverrideCycleParam separation, convergence;
	struct KeyOverrideCycleParam transition, release_transition;
	struct KeyOverrideCycleParam transition_type, release_transition_type;
	struct KeyOverrideCycleParam condition;
	bool not_done = true;
	int i, j;
	wchar_t buf[8];
	DirectX::XMFLOAT4 params[INI_PARAMS_SIZE];
	bool is_conditional;
	int condition_param_idx = 0;
	float DirectX::XMFLOAT4::*condition_param_component = NULL;


	for (j = 0; j < INI_PARAMS_SIZE; j++) {
		StringCchPrintf(buf, 8, L"x%.0i", j);
		GetPrivateProfileString(section, buf, 0, x[j].buf, MAX_PATH, ini);
		StringCchPrintf(buf, 8, L"y%.0i", j);
		GetPrivateProfileString(section, buf, 0, y[j].buf, MAX_PATH, ini);
		StringCchPrintf(buf, 8, L"z%.0i", j);
		GetPrivateProfileString(section, buf, 0, z[j].buf, MAX_PATH, ini);
		StringCchPrintf(buf, 8, L"w%.0i", j);
		GetPrivateProfileString(section, buf, 0, w[j].buf, MAX_PATH, ini);
	}
	GetPrivateProfileString(section, L"separation", 0, separation.buf, MAX_PATH, ini);
	GetPrivateProfileString(section, L"convergence", 0, convergence.buf, MAX_PATH, ini);
	GetPrivateProfileString(section, L"transition", 0, transition.buf, MAX_PATH, ini);
	GetPrivateProfileString(section, L"release_transition", 0, release_transition.buf, MAX_PATH, ini);
	GetPrivateProfileString(section, L"transition_type", 0, transition_type.buf, MAX_PATH, ini);
	GetPrivateProfileString(section, L"release_transition_type", 0, release_transition_type.buf, MAX_PATH, ini);
	GetPrivateProfileString(section, L"condition", 0, condition.buf, MAX_PATH, ini);

	for (i = 1; not_done; i++) {
		not_done = false;

		for (j = 0; j < INI_PARAMS_SIZE; j++) {
			not_done = x[j].next() || not_done;
			not_done = y[j].next() || not_done;
			not_done = z[j].next() || not_done;
			not_done = w[j].next() || not_done;
		}
		not_done = separation.next() || not_done;
		not_done = convergence.next() || not_done;
		not_done = transition.next() || not_done;
		not_done = release_transition.next() || not_done;
		not_done = transition_type.next() || not_done;
		not_done = release_transition_type.next() || not_done;
		not_done = condition.next() || not_done;

		if (!not_done)
			break;

		LogInfo("  Cycle %i:", i);
		for (j = 0; j < INI_PARAMS_SIZE; j++) {
			StringCchPrintf(buf, 8, L"x%.0i", j);
			x[j].log(buf);
			StringCchPrintf(buf, 8, L"y%.0i", j);
			y[j].log(buf);
			StringCchPrintf(buf, 8, L"z%.0i", j);
			z[j].log(buf);
			StringCchPrintf(buf, 8, L"w%.0i", j);
			w[j].log(buf);

			params[j].x = x[j].as_float(FLT_MAX);
			params[j].y = y[j].as_float(FLT_MAX);
			params[j].z = z[j].as_float(FLT_MAX);
			params[j].w = w[j].as_float(FLT_MAX);
		}

		is_conditional = condition.as_ini_param(&condition_param_idx, &condition_param_component);

		separation.log(L"separation");
		convergence.log(L"convergence");
		transition.log(L"transition");
		release_transition.log(L"release_transition");
		transition_type.log(L"transition_type");
		release_transition_type.log(L"release_transition_type");
		condition.log(L"condition");
		LogInfo("\n");

		presets.push_back(KeyOverride(KeyOverrideType::CYCLE, params,
			separation.as_float(FLT_MAX), convergence.as_float(FLT_MAX),
			transition.as_int(0), release_transition.as_int(0),
			transition_type.as_enum<wchar_t *, TransitionType>(TransitionTypeNames, TransitionType::LINEAR),
			release_transition_type.as_enum<wchar_t *, TransitionType>(TransitionTypeNames, TransitionType::LINEAR),
			is_conditional, condition_param_idx, condition_param_component));
	}
}

void KeyOverrideCycle::DownEvent(HackerDevice *device)
{
	if (presets.empty())
		return;

	presets[current].Activate(device);
	current = (current + 1) % presets.size();
}

// In order to change the iniParams, we need to map them back to system memory so that the CPU
// can change the values, then remap them back to the GPU where they can be accessed by shader code.
// This map/unmap code also requires that the texture be created with the D3D11_USAGE_DYNAMIC flag set.
// This map operation can also cause the GPU to stall, so this should be done as rarely as possible.

static void UpdateIniParams(HackerDevice* wrapper,
		DirectX::XMFLOAT4 *params)
{
	ID3D11DeviceContext* realContext = wrapper->GetOrigContext();
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	int i;
	bool updated = false;

	for (i = 0; i < INI_PARAMS_SIZE; i++) {
		if (params[i].x != FLT_MAX) {
			G->iniParams[i].x = params[i].x;
			updated = true;
		}
		if (params[i].y != FLT_MAX) {
			G->iniParams[i].y = params[i].y;
			updated = true;
		}
		if (params[i].z != FLT_MAX) {
			G->iniParams[i].z = params[i].z;
			updated = true;
		}
		if (params[i].w != FLT_MAX) {
			G->iniParams[i].w = params[i].w;
			updated = true;
		}
	}

	if (!updated)
		return;

	realContext->Map(wrapper->mIniTexture, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	memcpy(mappedResource.pData, &G->iniParams, sizeof(G->iniParams));
	realContext->Unmap(wrapper->mIniTexture, 0);

	LogDebug(" IniParams remapped to ");
	for (i = 0; i < INI_PARAMS_SIZE; i++)
		LogDebug("%#.2g, %#.2g, %#.2g, %#.2g, ", params[i].x, params[i].y, params[i].z, params[i].w);
	LogDebug("\n");
}

void Override::Activate(HackerDevice *device)
{
	if (is_conditional && G->iniParams[condition_param_idx].*condition_param_component == 0) {
		LogInfo("Skipping override activation: condition not met\n");
		return;
	}

	LogInfo("User key activation -->\n");

	CurrentTransition.ScheduleTransition(device,
			mOverrideSeparation,
			mOverrideConvergence,
			mOverrideParams,
			transition,
			transition_type);
}

void Override::Deactivate(HackerDevice *device)
{
	LogInfo("User key deactivation <--\n");

	CurrentTransition.ScheduleTransition(device,
			mUserSeparation,
			mUserConvergence,
			mSavedParams,
			release_transition,
			release_transition_type);
}

void Override::Toggle(HackerDevice *device)
{
	if (is_conditional && G->iniParams[condition_param_idx].*condition_param_component == 0) {
		LogInfo("Skipping toggle override: condition not met\n");
		return;
	}

	active = !active;
	if (!active) {
		OverrideSave.Restore(this);
		return Deactivate(device);
	}
	OverrideSave.Save(device, this);
	return Activate(device);
}

void KeyOverride::DownEvent(HackerDevice *device)
{
	if (type == KeyOverrideType::TOGGLE)
		return Toggle(device);
	if (type == KeyOverrideType::HOLD)
		OverrideSave.Save(device, this);
	return Activate(device);
}

void KeyOverride::UpEvent(HackerDevice *device)
{
	if (type == KeyOverrideType::HOLD) {
		OverrideSave.Restore(this);
		return Deactivate(device);
	}
}

void PresetOverride::Activate(HackerDevice *device, PresetOverride *prev)
{
	NvAPI_Status err;
	int i;

	if (activated)
		return;

	if (prev == NULL) {
		// Store current values for deactivation
		err = NvAPI_Stereo_GetSeparation(device->mStereoHandle, &mUserSeparation);
		if (err != NVAPI_OK)
			LogDebug("Stereo_GetSeparation failed: %i\n", err);

		err = NvAPI_Stereo_GetConvergence(device->mStereoHandle, &mUserConvergence);
		if (err != NVAPI_OK)
			LogDebug("Stereo_GetConvergence failed: %i\n", err);

		for (i = 0; i < INI_PARAMS_SIZE; i++) {
			mSavedParams[i].x = CurrentTransition.x[i].target;
			mSavedParams[i].y = CurrentTransition.y[i].target;
			mSavedParams[i].z = CurrentTransition.z[i].target;
			mSavedParams[i].w = CurrentTransition.w[i].target;
		}

	} else {
		// Store values from the last activated preset which holds the real original data
		mUserSeparation = prev->mUserSeparation;
		mUserConvergence = prev->mUserConvergence;

		for (i = 0; i < INI_PARAMS_SIZE; i++) {
			mSavedParams[i].x = prev->mSavedParams[i].x;
			mSavedParams[i].y = prev->mSavedParams[i].y;
			mSavedParams[i].z = prev->mSavedParams[i].z;
			mSavedParams[i].w = prev->mSavedParams[i].w;
		}
	}

	Override::Activate(device);

	activated = true;
}

void PresetOverride::Deactivate(HackerDevice *device)
{
	if (!activated)
		return;

	Override::Deactivate(device);

	activated = false;
}

bool PresetOverride::IsActivated()
{
	return activated;
}

static void _ScheduleTransition(struct OverrideTransitionParam *transition,
		char *name, float current, float val, ULONGLONG now, int time,
		TransitionType transition_type)
{
	LogInfo(" %s: %#.2g -> %#.2g", name, current, val);
	transition->start = current;
	transition->target = val;
	transition->activation_time = now;
	transition->time = time;
	transition->transition_type = transition_type;
}

void OverrideTransition::ScheduleTransition(HackerDevice *wrapper,
		float target_separation, float target_convergence,
		DirectX::XMFLOAT4 *targets,
		int time, TransitionType transition_type)
{
	ULONGLONG now = GetTickCount64();
	NvAPI_Status err;
	float current;
	char buf[8];
	int i;

	LogInfo(" Override");
	if (time) {
		LogInfo(" transition: %ims", time);
		LogInfoW(L" transition_type: %s",
			lookup_enum_name<wchar_t *, TransitionType>(TransitionTypeNames, transition_type));
	}

	if (target_separation != FLT_MAX) {
		err = NvAPI_Stereo_GetSeparation(wrapper->mStereoHandle, &current);
		if (err != NVAPI_OK)
			LogDebug("    Stereo_GetSeparation failed: %i\n", err);
		_ScheduleTransition(&separation, "separation", current, target_separation, now, time, transition_type);
	}
	if (target_convergence != FLT_MAX) {
		err = NvAPI_Stereo_GetConvergence(wrapper->mStereoHandle, &current);
		if (err != NVAPI_OK)
			LogDebug("    Stereo_GetConvergence failed: %i\n", err);
		_ScheduleTransition(&convergence, "convergence", current, target_convergence, now, time, transition_type);
	}
	for (i = 0; i < INI_PARAMS_SIZE; i++) {
		if (targets[i].x != FLT_MAX) {
			StringCchPrintfA(buf, 8, "x%.0i", i);
			_ScheduleTransition(&x[i], buf, G->iniParams[i].x, targets[i].x, now, time, transition_type);
		}
		if (targets[i].y != FLT_MAX) {
			StringCchPrintfA(buf, 8, "y%.0i", i);
			_ScheduleTransition(&y[i], buf, G->iniParams[i].y, targets[i].y, now, time, transition_type);
		}
		if (targets[i].z != FLT_MAX) {
			StringCchPrintfA(buf, 8, "z%.0i", i);
			_ScheduleTransition(&z[i], buf, G->iniParams[i].z, targets[i].z, now, time, transition_type);
		}
		if (targets[i].w != FLT_MAX) {
			StringCchPrintfA(buf, 8, "w%.0i", i);
			_ScheduleTransition(&w[i], buf, G->iniParams[i].w, targets[i].w, now, time, transition_type);
		}
	}
	LogInfo("\n");
}

void OverrideTransition::UpdatePresets(HackerDevice *wrapper)
{
	PresetOverrideMap::iterator i;
	PresetOverride *prev = NULL;
	PresetOverride *preset;

	// Deactivate previously activated but not current preset
	for (i = presetOverrides.begin(); i != presetOverrides.end(); i++) {
		if (active_preset.empty() || i->first != active_preset) {
			preset = &i->second;
			if (preset->IsActivated()) {
				prev = preset;
				preset->Deactivate(wrapper);
			}
		}
	}

	// Activate current preset if any
	if (!active_preset.empty()) {
		preset = &presetOverrides[active_preset];
		preset->Activate(wrapper, prev);
	}

	// If next frame activates any preset the active_present will be set again.
	// Otherwise active_preset remains empty until next call, in which case we
	// deactivate the current active preset.
	active_preset.clear();
}

static float _UpdateTransition(struct OverrideTransitionParam *transition, ULONGLONG now)
{
	ULONGLONG time;
	float percent;

	if (transition->time == -1)
		return FLT_MAX;

	if (transition->time == 0) {
		transition->time = -1;
		return transition->target;
	}

	time = now - transition->activation_time;
	percent = (float)time / transition->time;

	if (percent >= 1.0f) {
		transition->time = -1;
		return transition->target;
	}

	if (transition->transition_type == TransitionType::COSINE)
		percent = (float)((1.0 - cos(percent * M_PI)) / 2.0);

	percent = transition->target * percent + transition->start * (1.0f - percent);

	return percent;
}

void OverrideTransition::UpdateTransitions(HackerDevice *wrapper)
{
	ULONGLONG now = GetTickCount64();
	DirectX::XMFLOAT4 params[INI_PARAMS_SIZE];
	NvAPI_Status err;
	float val;
	int i;

	val = _UpdateTransition(&separation, now);
	if (val != FLT_MAX) {
		LogInfo(" Transitioning separation to %#.2f\n", val);

		NvAPIOverride();
		err = NvAPI_Stereo_SetSeparation(wrapper->mStereoHandle, val);
		if (err != NVAPI_OK)
			LogDebug("    Stereo_SetSeparation failed: %i\n", err);
	}

	val = _UpdateTransition(&convergence, now);
	if (val != FLT_MAX) {
		LogInfo(" Transitioning convergence to %#.2f\n", val);

		NvAPIOverride();
		err = NvAPI_Stereo_SetConvergence(wrapper->mStereoHandle, val);
		if (err != NVAPI_OK)
			LogDebug("    Stereo_SetConvergence failed: %i\n", err);
	}

	for (i = 0; i < INI_PARAMS_SIZE; i++) {
		params[i].x = _UpdateTransition(&x[i], now);
		params[i].y = _UpdateTransition(&y[i], now);
		params[i].z = _UpdateTransition(&z[i], now);
		params[i].w = _UpdateTransition(&w[i], now);
	}

	UpdateIniParams(wrapper, params);
}

void OverrideTransition::Stop()
{
	int i;

	for (i = 0; i < INI_PARAMS_SIZE; i++) {
		x[i].time = -1;
		y[i].time = -1;
		z[i].time = -1;
		w[i].time = -1;
	}
	separation.time = -1;
	convergence.time = -1;
}

OverrideGlobalSaveParam::OverrideGlobalSaveParam() :
	save(FLT_MAX),
	refcount(0)
{
}

float OverrideGlobalSaveParam::Reset()
{
	float ret = save;

	save = FLT_MAX;
	refcount = 0;

	return ret;
}

void OverrideGlobalSave::Reset(HackerDevice* wrapper)
{
	NvAPI_Status err;
	float val;
	int i;

	for (i = 0; i < INI_PARAMS_SIZE; i++) {
		x[i].Reset();
		y[i].Reset();
		z[i].Reset();
		w[i].Reset();
	}

	// Restore any saved separation & convergence settings to prevent a
	// currently active preset from becoming the default on config reload.
	//
	// If there is no active preset, but there is a transition in progress,
	// use it's target to avoid an intermediate value becoming the default.
	//
	// Don't worry about the ini params since the config reload will reset
	// them anyway.
	val = separation.Reset();
	if (val == FLT_MAX && CurrentTransition.separation.time != -1)
		val = CurrentTransition.separation.target;
	if (val != FLT_MAX) {
		LogInfo(" Restoring separation to %#.2f\n", val);

		NvAPIOverride();
		err = NvAPI_Stereo_SetSeparation(wrapper->mStereoHandle, val);
		if (err != NVAPI_OK)
			LogDebug("    Stereo_SetSeparation failed: %i\n", err);
	}

	val = convergence.Reset();
	if (val == FLT_MAX && CurrentTransition.convergence.time != -1)
		val = CurrentTransition.convergence.target;
	if (val != FLT_MAX) {
		LogInfo(" Restoring convergence to %#.2f\n", val);

		NvAPIOverride();
		err = NvAPI_Stereo_SetConvergence(wrapper->mStereoHandle, val);
		if (err != NVAPI_OK)
			LogDebug("    Stereo_SetConvergence failed: %i\n", err);
	}

	// Make sure any current transition won't continue to change the
	// parameters after the reset:
	CurrentTransition.Stop();
}

void OverrideGlobalSaveParam::Save(float val)
{
	if (!refcount)
		save = val;
	refcount++;
}

// Saves the current values for each parameter to the override's local save
// area, and the global save area (if nothing is already saved there).
// If a parameter is currently in a transition, the target value of that
// transition is used instead of the current value. This prevents an
// intermediate transition value from being saved and restored later (e.g. if
// rapidly pressing RMB with a release_transition set).

void OverrideGlobalSave::Save(HackerDevice *wrapper, Override *preset)
{
	NvAPI_Status err;
	float val;
	int i;

	if (preset->mOverrideSeparation != FLT_MAX) {
		if (CurrentTransition.separation.time != -1) {
			val = CurrentTransition.separation.target;
		} else {
			err = NvAPI_Stereo_GetSeparation(wrapper->mStereoHandle, &val);
			if (err != NVAPI_OK) {
				LogDebug("    Stereo_GetSeparation failed: %i\n", err);
			}
		}

		preset->mUserSeparation = val;
		separation.Save(val);
	}

	if (preset->mOverrideConvergence != FLT_MAX) {
		if (CurrentTransition.convergence.time != -1) {
			val = CurrentTransition.convergence.target;
		} else {
			err = NvAPI_Stereo_GetConvergence(wrapper->mStereoHandle, &val);
			if (err != NVAPI_OK) {
				LogDebug("    Stereo_GetConvergence failed: %i\n", err);
			}
		}

		preset->mUserConvergence = val;
		convergence.Save(val);
	}

	for (i = 0; i < INI_PARAMS_SIZE; i++) {
		if (preset->mOverrideParams[i].x != FLT_MAX) {
			if (CurrentTransition.x[i].time != -1)
				val = CurrentTransition.x[i].target;
			else
				val = G->iniParams[i].x;

			preset->mSavedParams[i].x = val;
			x[i].Save(val);
		}
		if (preset->mOverrideParams[i].y != FLT_MAX) {
			if (CurrentTransition.y[i].time != -1)
				val = CurrentTransition.y[i].target;
			else
				val = G->iniParams[i].y;

			preset->mSavedParams[i].y = val;
			y[i].Save(val);
		}
		if (preset->mOverrideParams[i].z != FLT_MAX) {
			if (CurrentTransition.z[i].time != -1)
				val = CurrentTransition.z[i].target;
			else
				val = G->iniParams[i].z;

			preset->mSavedParams[i].z = val;
			z[i].Save(val);
		}
		if (preset->mOverrideParams[i].w != FLT_MAX) {
			if (CurrentTransition.w[i].time != -1)
				val = CurrentTransition.w[i].target;
			else
				val = G->iniParams[i].w;

			preset->mSavedParams[i].w = val;
			w[i].Save(val);
		}
	}
}

void OverrideGlobalSaveParam::Restore(float *val)
{
	refcount--;

	if (!refcount) {
		if (val)
			*val = save;
		save = FLT_MAX;
	} else if (refcount < 0) {
		LogInfo("BUG! OverrideGlobalSaveParam refcount < 0!\n");
	}
}

void OverrideGlobalSave::Restore(Override *preset)
{
	int i;

	// This replaces the local saved values in the override with the global
	// ones for any parameters where this is the last override being
	// deactivated. This ensures that we will finally restore the original
	// value, even if keys were held and released in a bad order, or a
	// local value was saved in the middle of a transition.

	if (preset->mOverrideSeparation != FLT_MAX)
		separation.Restore(&preset->mUserSeparation);
	if (preset->mOverrideConvergence != FLT_MAX)
		convergence.Restore(&preset->mUserConvergence);
	for (i = 0; i < INI_PARAMS_SIZE; i++) {
		if (preset->mOverrideParams[i].x != FLT_MAX)
			x[i].Restore(&preset->mSavedParams[i].x);
		if (preset->mOverrideParams[i].y != FLT_MAX)
			y[i].Restore(&preset->mSavedParams[i].y);
		if (preset->mOverrideParams[i].z != FLT_MAX)
			z[i].Restore(&preset->mSavedParams[i].z);
		if (preset->mOverrideParams[i].w != FLT_MAX)
			w[i].Restore(&preset->mSavedParams[i].w);
	}
}
