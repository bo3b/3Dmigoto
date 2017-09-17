#include "Override.h"

#include "Main.h"
#include "globals.h"

#define _USE_MATH_DEFINES
#include <math.h>

OverrideTransition CurrentTransition;
OverrideGlobalSave OverrideSave;

Override::Override()
{
	// It's important for us to know if any are actively in use or not, so setting them
	// to FloatMax by default allows us to know when they are unused or invalid.
	// We are using FloatMax now instead of infinity, to avoid compiler warnings.
	mOverrideParams.x = FLT_MAX;
	mOverrideParams.y = FLT_MAX;
	mOverrideParams.z = FLT_MAX;
	mOverrideParams.w = FLT_MAX;
	mOverrideSeparation = FLT_MAX;
	mOverrideConvergence = FLT_MAX;

	mSavedParams.x = FLT_MAX;
	mSavedParams.y = FLT_MAX;
	mSavedParams.z = FLT_MAX;
	mSavedParams.w = FLT_MAX;
	mUserSeparation = FLT_MAX;
	mUserConvergence = FLT_MAX;

	active = false;
}

void Override::ParseIniSection(LPCWSTR section, LPCWSTR ini)
{
	wchar_t buf[MAX_PATH];

	if (GetPrivateProfileString(section, L"separation", 0, buf, MAX_PATH, ini)) {
		swscanf_s(buf, L"%f", &mOverrideSeparation);
		LogInfo("  separation=%#.2f\n", mOverrideSeparation);
	}

	if (GetPrivateProfileString(section, L"convergence", 0, buf, MAX_PATH, ini)) {
		swscanf_s(buf, L"%f", &mOverrideConvergence);
		LogInfo("  convergence=%#.2f\n", mOverrideConvergence);
	}

	if (GetPrivateProfileString(section, L"x", 0, buf, MAX_PATH, ini)) {
		swscanf_s(buf, L"%f", &mOverrideParams.x);
		LogInfo("  x=%#.2g\n", mOverrideParams.x);
	}

	if (GetPrivateProfileString(section, L"y", 0, buf, MAX_PATH, ini)) {
		swscanf_s(buf, L"%f", &mOverrideParams.y);
		LogInfo("  y=%#.2g\n", mOverrideParams.y);
	}

	if (GetPrivateProfileString(section, L"z", 0, buf, MAX_PATH, ini)) {
		swscanf_s(buf, L"%f", &mOverrideParams.z);
		LogInfo("  z=%#.2g\n", mOverrideParams.z);
	}

	if (GetPrivateProfileString(section, L"w", 0, buf, MAX_PATH, ini)) {
		swscanf_s(buf, L"%f", &mOverrideParams.w);
		LogInfo("  w=%#.2g\n", mOverrideParams.w);
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
			LogInfoWNoNL(L" %s=%s", name, cur);
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
};

void KeyOverrideCycle::ParseIniSection(LPCWSTR section, LPCWSTR ini)
{
	struct KeyOverrideCycleParam x, y, z, w, separation, convergence;
	struct KeyOverrideCycleParam transition, release_transition;
	struct KeyOverrideCycleParam transition_type, release_transition_type;
	bool not_done = true;
	int i;

	GetPrivateProfileString(section, L"x", 0, x.buf, MAX_PATH, ini);
	GetPrivateProfileString(section, L"y", 0, y.buf, MAX_PATH, ini);
	GetPrivateProfileString(section, L"z", 0, z.buf, MAX_PATH, ini);
	GetPrivateProfileString(section, L"w", 0, w.buf, MAX_PATH, ini);
	GetPrivateProfileString(section, L"separation", 0, separation.buf, MAX_PATH, ini);
	GetPrivateProfileString(section, L"convergence", 0, convergence.buf, MAX_PATH, ini);
	GetPrivateProfileString(section, L"transition", 0, transition.buf, MAX_PATH, ini);
	GetPrivateProfileString(section, L"release_transition", 0, release_transition.buf, MAX_PATH, ini);
	GetPrivateProfileString(section, L"transition_type", 0, transition_type.buf, MAX_PATH, ini);
	GetPrivateProfileString(section, L"release_transition_type", 0, release_transition_type.buf, MAX_PATH, ini);

	for (i = 1; not_done; i++) {
		not_done = false;

		not_done = x.next() || not_done;
		not_done = y.next() || not_done;
		not_done = z.next() || not_done;
		not_done = w.next() || not_done;
		not_done = separation.next() || not_done;
		not_done = convergence.next() || not_done;
		not_done = transition.next() || not_done;
		not_done = release_transition.next() || not_done;
		not_done = transition_type.next() || not_done;
		not_done = release_transition_type.next() || not_done;

		if (!not_done)
			break;

		LogInfoNoNL("  Cycle %i:", i);
		x.log(L"x");
		y.log(L"y");
		z.log(L"z");
		w.log(L"w");
		separation.log(L"separation");
		convergence.log(L"convergence");
		transition.log(L"transition");
		release_transition.log(L"release_transition");
		transition_type.log(L"transition_type");
		release_transition_type.log(L"release_transition_type");
		LogInfo("\n");

		presets.push_back(KeyOverride(KeyOverrideType::CYCLE,
			x.as_float(FLT_MAX), y.as_float(FLT_MAX),
			z.as_float(FLT_MAX), w.as_float(FLT_MAX),
			separation.as_float(FLT_MAX), convergence.as_float(FLT_MAX),
			transition.as_int(0), release_transition.as_int(0),
			transition_type.as_enum<wchar_t *, TransitionType>(TransitionTypeNames, TransitionType::LINEAR),
			release_transition_type.as_enum<wchar_t *, TransitionType>(TransitionTypeNames, TransitionType::LINEAR)));
	}
}

void KeyOverrideCycle::DownEvent(D3D10Base::ID3D10Device *device)
{
	if (presets.empty())
		return;

	presets[current].Activate(device);
	current = (current + 1) % presets.size();
}

// In order to change the iniParams, we need to map them back to system memory so that the CPU
// can change the values, then remap them back to the GPU where they can be accessed by shader code.
// This map/unmap code also requires that the texture be created with the D3D10_USAGE_DYNAMIC flag set.
// This map operation can also cause the GPU to stall, so this should be done as rarely as possible.

static void UpdateIniParams(D3D10Base::ID3D10Device *device,
		D3D10Wrapper::ID3D10Device* wrapper,
		DirectX::XMFLOAT4 *params)
{
	LogDebug("UpdateIniParams for DX10- unimplemented\n");

	//D3D10Base::ID3D10DeviceContext* realContext; device->GetImmediateContext(&realContext);
	//D3D10Base::D3D10_MAPPED_SUBRESOURCE mappedResource;

	//if (params->x == FLT_MAX && params->y == FLT_MAX &&
	//    params->z == FLT_MAX && params->w == FLT_MAX) {
	//	return;
	//}

	//if (params->x != FLT_MAX)
	//	G->iniParams.x = params->x;
	//if (params->y != FLT_MAX)
	//	G->iniParams.y = params->y;
	//if (params->z != FLT_MAX)
	//	G->iniParams.z = params->z;
	//if (params->w != FLT_MAX)
	//	G->iniParams.w = params->w;

	//realContext->Map(wrapper->mIniTexture, 0, D3D10Base::D3D10_MAP_WRITE_DISCARD, 0, &mappedResource);
	//memcpy(mappedResource.pData, &G->iniParams, sizeof(G->iniParams));
	//realContext->Unmap(wrapper->mIniTexture, 0);

	//LogDebug(" IniParams remapped to %#.2g, %#.2g, %#.2g, %#.2g\n", params->x, params->y, params->z, params->w);
}

void Override::Activate(D3D10Base::ID3D10Device *device)
{
	LogInfo("User key activation -->\n");

	CurrentTransition.ScheduleTransition(device,
			mOverrideSeparation,
			mOverrideConvergence,
			mOverrideParams.x,
			mOverrideParams.y,
			mOverrideParams.z,
			mOverrideParams.w,
			transition,
			transition_type);
}

void Override::Deactivate(D3D10Base::ID3D10Device *device)
{
	LogInfo("User key deactivation <--\n");

	CurrentTransition.ScheduleTransition(device,
			mUserSeparation,
			mUserConvergence,
			mSavedParams.x,
			mSavedParams.y,
			mSavedParams.z,
			mSavedParams.w,
			release_transition,
			release_transition_type);
}

void Override::Toggle(D3D10Base::ID3D10Device *device)
{
	active = !active;
	if (!active) {
		OverrideSave.Restore(this);
		return Deactivate(device);
	}
	OverrideSave.Save(device, this);
	return Activate(device);
}

void KeyOverride::DownEvent(D3D10Base::ID3D10Device *device)
{
	if (type == KeyOverrideType::TOGGLE)
		return Toggle(device);
	if (type == KeyOverrideType::HOLD)
		OverrideSave.Save(device, this);
	return Activate(device);
}

void KeyOverride::UpEvent(D3D10Base::ID3D10Device *device)
{
	if (type == KeyOverrideType::HOLD) {
		OverrideSave.Restore(this);
		return Deactivate(device);
	}
}

static void _ScheduleTransition(struct OverrideTransitionParam *transition,
		char *name, float current, float val, ULONGLONG now, int time,
		TransitionType transition_type)
{
	LogInfoNoNL(" %s: %#.2g -> %#.2g", name, current, val);
	transition->start = current;
	transition->target = val;
	transition->activation_time = now;
	transition->time = time;
	transition->transition_type = transition_type;
}

void OverrideTransition::ScheduleTransition(D3D10Base::ID3D10Device *device,
		float target_separation, float target_convergence, float
		target_x, float target_y, float target_z, float target_w,
		int time, TransitionType transition_type)
{
	ULONGLONG now = GetTickCount64();
	D3D10Base::NvAPI_Status err;
	float current;
	D3D10Wrapper::ID3D10Device *wrapper;

	wrapper = (D3D10Wrapper::ID3D10Device*) D3D10Wrapper::ID3D10Device::m_List.GetDataPtr(device);
	if (!wrapper)
		return;

	LogInfoNoNL(" Override");
	if (time) {
		LogInfoNoNL(" transition: %ims", time);
		LogInfoWNoNL(L" transition_type: %s",
			lookup_enum_name<wchar_t *, TransitionType>(TransitionTypeNames, transition_type));
	}

	if (target_separation != FLT_MAX) {
		err = D3D10Base::NvAPI_Stereo_GetSeparation(wrapper->mStereoHandle, &current);
		if (err != D3D10Base::NVAPI_OK)
			LogDebug("    Stereo_GetSeparation failed: %i\n", err);
		_ScheduleTransition(&separation, "separation", current, target_separation, now, time, transition_type);
	}
	if (target_convergence != FLT_MAX) {
		err = D3D10Base::NvAPI_Stereo_GetConvergence(wrapper->mStereoHandle, &current);
		if (err != D3D10Base::NVAPI_OK)
			LogDebug("    Stereo_GetConvergence failed: %i\n", err);
		_ScheduleTransition(&convergence, "convergence", current, target_convergence, now, time, transition_type);
	}
	if (target_x != FLT_MAX)
		_ScheduleTransition(&x, "x", G->iniParams.x, target_x, now, time, transition_type);
	if (target_y != FLT_MAX)
		_ScheduleTransition(&y, "y", G->iniParams.y, target_y, now, time, transition_type);
	if (target_z != FLT_MAX)
		_ScheduleTransition(&z, "z", G->iniParams.z, target_z, now, time, transition_type);
	if (target_w != FLT_MAX)
		_ScheduleTransition(&w, "w", G->iniParams.w, target_w, now, time, transition_type);
	LogInfo("\n");
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

void OverrideTransition::UpdateTransitions(D3D10Base::ID3D10Device *device)
{
	D3D10Wrapper::ID3D10Device *wrapper;
	ULONGLONG now = GetTickCount64();
	DirectX::XMFLOAT4 params = {FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX};
	D3D10Base::NvAPI_Status err;
	float val;

	wrapper = (D3D10Wrapper::ID3D10Device*) D3D10Wrapper::ID3D10Device::m_List.GetDataPtr(device);
	if (!wrapper)
		return;

	val = _UpdateTransition(&separation, now);
	if (val != FLT_MAX) {
		LogInfo(" Transitioning separation to %#.2f\n", val);

//		D3D10Wrapper::NvAPIOverride();
		err = D3D10Base::NvAPI_Stereo_SetSeparation(wrapper->mStereoHandle, val);
		if (err != D3D10Base::NVAPI_OK)
			LogDebug("    Stereo_SetSeparation failed: %i\n", err);
	}

	val = _UpdateTransition(&convergence, now);
	if (val != FLT_MAX) {
		LogInfo(" Transitioning convergence to %#.2f\n", val);

		//D3D10Wrapper::NvAPIOverride();
		err = D3D10Base::NvAPI_Stereo_SetConvergence(wrapper->mStereoHandle, val);
		if (err != D3D10Base::NVAPI_OK)
			LogDebug("    Stereo_SetConvergence failed: %i\n", err);
	}

	params.x = _UpdateTransition(&x, now);
	params.y = _UpdateTransition(&y, now);
	params.z = _UpdateTransition(&z, now);
	params.w = _UpdateTransition(&w, now);

	UpdateIniParams(device, wrapper, &params);
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

void OverrideGlobalSave::Reset(D3D10Wrapper::ID3D10Device* wrapper)
{
	D3D10Base::NvAPI_Status err;
	float val;

	x.Reset();
	y.Reset();
	z.Reset();
	w.Reset();

	// Restore any saved separation & convergence settings to prevent a
	// currently active preset from becoming the default on config reload.
	// Don't worry about the ini params since the config reload will reset
	// them anyway.
	val = separation.Reset();
	if (val != FLT_MAX) {
		LogInfo(" Restoring separation to %#.2f\n", val);

//		D3D10Wrapper::NvAPIOverride();
		err = D3D10Base::NvAPI_Stereo_SetSeparation(wrapper->mStereoHandle, val);
		if (err != D3D10Base::NVAPI_OK)
			LogDebug("    Stereo_SetSeparation failed: %i\n", err);
	}

	val = convergence.Reset();
	if (val != FLT_MAX) {
		LogInfo(" Restoring convergence to %#.2f\n", val);

	//	D3D10Wrapper::NvAPIOverride();
		err = D3D10Base::NvAPI_Stereo_SetConvergence(wrapper->mStereoHandle, val);
		if (err != D3D10Base::NVAPI_OK)
			LogDebug("    Stereo_SetConvergence failed: %i\n", err);
	}
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

void OverrideGlobalSave::Save(D3D10Base::ID3D10Device *device, Override *preset)
{
	D3D10Base::NvAPI_Status err;
	D3D10Wrapper::ID3D10Device *wrapper;
	float val;

	wrapper = (D3D10Wrapper::ID3D10Device*)D3D10Wrapper::ID3D10Device::m_List.GetDataPtr(device);
	if (!wrapper)
		return;

	if (preset->mOverrideSeparation != FLT_MAX) {
		if (CurrentTransition.separation.time != -1) {
			val = CurrentTransition.separation.target;
		} else {
			err = D3D10Base::NvAPI_Stereo_GetSeparation(wrapper->mStereoHandle, &val);
			if (err != D3D10Base::NVAPI_OK) {
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
			err = D3D10Base::NvAPI_Stereo_GetConvergence(wrapper->mStereoHandle, &val);
			if (err != D3D10Base::NVAPI_OK) {
				LogDebug("    Stereo_GetConvergence failed: %i\n", err);
			}
		}

		preset->mUserConvergence = val;
		convergence.Save(val);
	}

	if (preset->mOverrideParams.x != FLT_MAX) {
		if (CurrentTransition.x.time != -1)
			val = CurrentTransition.x.target;
		else
			val = G->iniParams.x;

		preset->mSavedParams.x = val;
		x.Save(val);
	}
	if (preset->mOverrideParams.y != FLT_MAX) {
		if (CurrentTransition.y.time != -1)
			val = CurrentTransition.y.target;
		else
			val = G->iniParams.y;

		preset->mSavedParams.y = val;
		y.Save(val);
	}
	if (preset->mOverrideParams.z != FLT_MAX) {
		if (CurrentTransition.z.time != -1)
			val = CurrentTransition.z.target;
		else
			val = G->iniParams.z;

		preset->mSavedParams.z = val;
		z.Save(val);
	}
	if (preset->mOverrideParams.w != FLT_MAX) {
		if (CurrentTransition.w.time != -1)
			val = CurrentTransition.w.target;
		else
			val = G->iniParams.w;

		preset->mSavedParams.w = val;
		w.Save(val);
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
	// This replaces the local saved values in the override with the global
	// ones for any parameters where this is the last override being
	// deactivated. This ensures that we will finally restore the original
	// value, even if keys were held and released in a bad order, or a
	// local value was saved in the middle of a transition.

	if (preset->mOverrideSeparation != FLT_MAX)
		separation.Restore(&preset->mUserSeparation);
	if (preset->mOverrideConvergence != FLT_MAX)
		convergence.Restore(&preset->mUserConvergence);
	if (preset->mOverrideParams.x != FLT_MAX)
		x.Restore(&preset->mSavedParams.x);
	if (preset->mOverrideParams.y != FLT_MAX)
		y.Restore(&preset->mSavedParams.y);
	if (preset->mOverrideParams.z != FLT_MAX)
		z.Restore(&preset->mSavedParams.z);
	if (preset->mOverrideParams.w != FLT_MAX)
		w.Restore(&preset->mSavedParams.w);
}
