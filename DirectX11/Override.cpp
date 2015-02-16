#include "Override.h"

#include "Main.h"
#include "globals.h"

OverrideTransition CurrentTransition;

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

	releasetransition = GetPrivateProfileInt(section, L"releasetransition", 0, ini);
	if (releasetransition)
		LogInfo("  releasetransition=%ims\n", releasetransition);
}

void Override::Save(D3D11Base::ID3D11Device *device)
{
	D3D11Base::NvAPI_Status err;
	D3D11Wrapper::ID3D11Device *wrapper;

	wrapper = (D3D11Wrapper::ID3D11Device*)D3D11Wrapper::ID3D11Device::m_List.GetDataPtr(device);
	if (!wrapper)
		return;

	if (mOverrideSeparation != FLT_MAX) {
		err = D3D11Base::NvAPI_Stereo_GetSeparation(wrapper->mStereoHandle, &mUserSeparation);
		if (err != D3D11Base::NVAPI_OK) {
			LogDebug("    Stereo_GetSeparation failed: %#.2f\n", err);
		}
	}

	if (mOverrideConvergence != FLT_MAX) {
		err = D3D11Base::NvAPI_Stereo_GetConvergence(wrapper->mStereoHandle, &mUserConvergence);
		if (err != D3D11Base::NVAPI_OK) {
			LogDebug("    Stereo_GetConvergence failed: %#.2f\n", err);
		}
	}

	if (mOverrideParams.x != FLT_MAX)
		mSavedParams.x = G->iniParams.x;
	if (mOverrideParams.y != FLT_MAX)
		mSavedParams.y = G->iniParams.y;
	if (mOverrideParams.z != FLT_MAX)
		mSavedParams.z = G->iniParams.z;
	if (mOverrideParams.w != FLT_MAX)
		mSavedParams.w = G->iniParams.w;
}

// In order to change the iniParams, we need to map them back to system memory so that the CPU
// can change the values, then remap them back to the GPU where they can be accessed by shader code.
// This map/unmap code also requires that the texture be created with the D3D11_USAGE_DYNAMIC flag set.
// This map operation can also cause the GPU to stall, so this should be done as rarely as possible.

static void UpdateIniParams(D3D11Base::ID3D11Device *device,
		D3D11Wrapper::ID3D11Device* wrapper,
		DirectX::XMFLOAT4 *params)
{
	D3D11Base::ID3D11DeviceContext* realContext; device->GetImmediateContext(&realContext);
	D3D11Base::D3D11_MAPPED_SUBRESOURCE mappedResource;

	if (params->x == FLT_MAX && params->y == FLT_MAX &&
	    params->z == FLT_MAX && params->w == FLT_MAX) {
		return;
	}

	if (params->x != FLT_MAX)
		G->iniParams.x = params->x;
	if (params->y != FLT_MAX)
		G->iniParams.y = params->y;
	if (params->z != FLT_MAX)
		G->iniParams.z = params->z;
	if (params->w != FLT_MAX)
		G->iniParams.w = params->w;

	realContext->Map(wrapper->mIniTexture, 0, D3D11Base::D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
	memcpy(mappedResource.pData, &G->iniParams, sizeof(G->iniParams));
	realContext->Unmap(wrapper->mIniTexture, 0);

	LogDebug(" IniParams remapped to %#.2g, %#.2g, %#.2g, %#.2g\n", params->x, params->y, params->z, params->w);
}

void Override::Activate(D3D11Base::ID3D11Device *device)
{
	LogInfo("User key activation -->\n");

	// FIXME: Don't save any values that are currently being transitioned
	// This is another case of a more general problem where
	// multiple type=hold/toggle keys change the same parameters -
	// we need a global save rather than an override specific one
	// to resolve it.
	Save(device);
	CurrentTransition.ScheduleTransition(device,
			mOverrideSeparation,
			mOverrideConvergence,
			mOverrideParams.x,
			mOverrideParams.y,
			mOverrideParams.z,
			mOverrideParams.w,
			transition);
}

void Override::Deactivate(D3D11Base::ID3D11Device *device)
{
	LogInfo("User key deactivation <--\n");

	CurrentTransition.ScheduleTransition(device,
			mUserSeparation,
			mUserConvergence,
			mSavedParams.x,
			mSavedParams.y,
			mSavedParams.z,
			mSavedParams.w,
			releasetransition);
}

void Override::Toggle(D3D11Base::ID3D11Device *device)
{
	active = !active;
	if (!active)
		return Deactivate(device);
	return Activate(device);
}

void KeyOverride::DownEvent(D3D11Base::ID3D11Device *device)
{
	if (type == KeyOverrideType::TOGGLE)
		return Toggle(device);
	return Activate(device);
}

void KeyOverride::UpEvent(D3D11Base::ID3D11Device *device)
{
	if (type == KeyOverrideType::HOLD)
		return Deactivate(device);
}

static void _ScheduleTransition(struct OverrideTransitionParam *transition,
		char *name, float current, float val, ULONGLONG now, int time)
{
	LogInfo(" %s: %#.2g -> %#.2g", name, current, val);
	transition->start = current;
	transition->target = val;
	transition->activation_time = now;
	transition->time = time;
}

void OverrideTransition::ScheduleTransition(D3D11Base::ID3D11Device *device,
		float target_separation, float target_convergence, float
		target_x, float target_y, float target_z, float target_w,
		int time)
{
	ULONGLONG now = GetTickCount64();
	D3D11Base::NvAPI_Status err;
	float current;
	D3D11Wrapper::ID3D11Device *wrapper;

	wrapper = (D3D11Wrapper::ID3D11Device*) D3D11Wrapper::ID3D11Device::m_List.GetDataPtr(device);
	if (!wrapper)
		return;

	LogInfo(" Override");
	if (time)
		LogInfo(" transition: %ims", time);

	if (target_separation != FLT_MAX) {
		err = D3D11Base::NvAPI_Stereo_GetSeparation(wrapper->mStereoHandle, &current);
		if (err != D3D11Base::NVAPI_OK)
			LogDebug("    Stereo_GetSeparation failed: %#.2f\n", err);
		_ScheduleTransition(&separation, "separation", current, target_separation, now, time);
	}
	if (target_convergence != FLT_MAX) {
		err = D3D11Base::NvAPI_Stereo_GetConvergence(wrapper->mStereoHandle, &current);
		if (err != D3D11Base::NVAPI_OK)
			LogDebug("    Stereo_GetSeparation failed: %#.2f\n", err);
		_ScheduleTransition(&convergence, "convergence", current, target_convergence, now, time);
	}
	if (target_x != FLT_MAX)
		_ScheduleTransition(&x, "x", G->iniParams.x, target_x, now, time);
	if (target_y != FLT_MAX)
		_ScheduleTransition(&y, "y", G->iniParams.y, target_y, now, time);
	if (target_z != FLT_MAX)
		_ScheduleTransition(&z, "z", G->iniParams.z, target_z, now, time);
	if (target_w != FLT_MAX)
		_ScheduleTransition(&w, "w", G->iniParams.w, target_w, now, time);
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

	return transition->target * percent + transition->start * (1.0f - percent);
}

void OverrideTransition::UpdateTransitions(D3D11Base::ID3D11Device *device)
{
	D3D11Wrapper::ID3D11Device *wrapper;
	ULONGLONG now = GetTickCount64();
	DirectX::XMFLOAT4 params = {FLT_MAX, FLT_MAX, FLT_MAX, FLT_MAX};
	D3D11Base::NvAPI_Status err;
	float val;

	wrapper = (D3D11Wrapper::ID3D11Device*) D3D11Wrapper::ID3D11Device::m_List.GetDataPtr(device);
	if (!wrapper)
		return;

	val = _UpdateTransition(&separation, now);
	if (val != FLT_MAX) {
		LogInfo(" Transitioning separation to %#.2f\n", val);

		D3D11Wrapper::NvAPIOverride();
		err = D3D11Base::NvAPI_Stereo_SetSeparation(wrapper->mStereoHandle, val);
		if (err != D3D11Base::NVAPI_OK)
			LogDebug("    Stereo_SetSeparation failed: %#.2f\n", err);
	}

	val = _UpdateTransition(&convergence, now);
	if (val != FLT_MAX) {
		LogInfo(" Transitioning convergence to %#.2f\n", val);

		D3D11Wrapper::NvAPIOverride();
		err = D3D11Base::NvAPI_Stereo_SetConvergence(wrapper->mStereoHandle, val);
		if (err != D3D11Base::NVAPI_OK)
			LogDebug("    Stereo_SetConvergence failed: %#.2f\n", err);
	}

	params.x = _UpdateTransition(&x, now);
	params.y = _UpdateTransition(&y, now);
	params.z = _UpdateTransition(&z, now);
	params.w = _UpdateTransition(&w, now);

	UpdateIniParams(device, wrapper, &params);
}
