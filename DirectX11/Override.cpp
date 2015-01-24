#include "Override.h"

#include "Main.h"

Override::Override()
{
	// It's important for us to know if any are actively in use or not, so setting them
	// to infinity by default allows us to know when they are valid.
	mOverrideParams.x = INFINITY;
	mOverrideParams.y = INFINITY;
	mOverrideParams.z = INFINITY;
	mOverrideParams.w = INFINITY;
	mOverrideSeparation = INFINITY;
	mOverrideConvergence = INFINITY;

	mUserSeparation = INFINITY;
	mUserConvergence = INFINITY;

	active = false;
}

void Override::ParseIniSection(LPCWSTR section, LPCWSTR ini)
{
	wchar_t buf[MAX_PATH];

	if (GetPrivateProfileString(section, L"separation", 0, buf, MAX_PATH, ini)) {
		swscanf_s(buf, L"%f", &mOverrideSeparation);
		LogInfo("  separation=%f\n", mOverrideSeparation);
	}

	if (GetPrivateProfileString(section, L"convergence", 0, buf, MAX_PATH, ini)) {
		swscanf_s(buf, L"%f", &mOverrideConvergence);
		LogInfo("  convergence=%f\n", mOverrideConvergence);
	}

	if (GetPrivateProfileString(section, L"x", 0, buf, MAX_PATH, ini)) {
		swscanf_s(buf, L"%f", &mOverrideParams.x);
		LogInfo("  x=%f\n", mOverrideParams.x);
	}

	if (GetPrivateProfileString(section, L"y", 0, buf, MAX_PATH, ini)) {
		swscanf_s(buf, L"%f", &mOverrideParams.y);
		LogInfo("  y=%f\n", mOverrideParams.y);
	}

	if (GetPrivateProfileString(section, L"z", 0, buf, MAX_PATH, ini)) {
		swscanf_s(buf, L"%f", &mOverrideParams.z);
		LogInfo("  z=%f\n", mOverrideParams.z);
	}

	if (GetPrivateProfileString(section, L"w", 0, buf, MAX_PATH, ini)) {
		swscanf_s(buf, L"%f", &mOverrideParams.w);
		LogInfo("  w=%f\n", mOverrideParams.w);
	}
}

void Override::Activate(D3D11Base::ID3D11Device *device)
{
	// In general we want to be calling the _real_ d3d11 device here.
	using namespace D3D11Base;

	NvAPI_Status err;

	D3D11Wrapper::ID3D11Device* wrapper =
		(D3D11Wrapper::ID3D11Device*) D3D11Wrapper::ID3D11Device::m_List.GetDataPtr(device);
	if (!wrapper)
		return;

	if (mOverrideSeparation != INFINITY) {
		err = NvAPI_Stereo_GetSeparation(wrapper->mStereoHandle, &mUserSeparation);
		if (err != NVAPI_OK) {
			LogDebug("    Stereo_GetSeparation failed: %d\n", err);
		}

		LogInfo("Changing separation from %f to %f\n", mUserSeparation, mOverrideSeparation);

		D3D11Wrapper::NvAPIOverride();
		err = NvAPI_Stereo_SetSeparation(wrapper->mStereoHandle, mOverrideSeparation);
		if (err != NVAPI_OK) {
			LogDebug("    Stereo_SetSeparation failed: %d\n", err);
		}
	}
	if (mOverrideConvergence != INFINITY) {
		err = NvAPI_Stereo_GetConvergence(wrapper->mStereoHandle, &mUserConvergence);
		if (err != NVAPI_OK) {
			LogDebug("    Stereo_GetConvergence failed: %d\n", err);
		}

		LogInfo("Changing convergence from %f to %f\n", mUserConvergence, mOverrideConvergence);

		D3D11Wrapper::NvAPIOverride();
		err = NvAPI_Stereo_SetConvergence(wrapper->mStereoHandle, mOverrideConvergence);
		if (err != NVAPI_OK) {
			LogDebug("    Stereo_SetConvergence failed: %d\n", err);
		}
	}

	active = true;
}

void Override::Deactivate(D3D11Base::ID3D11Device *device)
{
	using namespace D3D11Base;

	NvAPI_Status err;

	D3D11Wrapper::ID3D11Device* wrapper =
		(D3D11Wrapper::ID3D11Device*) D3D11Wrapper::ID3D11Device::m_List.GetDataPtr(device);
	if (!wrapper)
		return;

	if (mUserSeparation != INFINITY) {
		D3D11Wrapper::NvAPIOverride();
		err = NvAPI_Stereo_SetSeparation(wrapper->mStereoHandle, mUserSeparation);
		if (err != NVAPI_OK) {
			LogDebug("    Stereo_SetSeparation failed: %d\n", err);
		}
		mUserSeparation = INFINITY;
	}
	if (mUserConvergence != INFINITY) {
		D3D11Wrapper::NvAPIOverride();
		err = NvAPI_Stereo_SetConvergence(wrapper->mStereoHandle, mUserConvergence);
		if (err != NVAPI_OK) {
			LogDebug("    Stereo_SetConvergence failed: %d\n", err);
		}
		mUserConvergence = INFINITY;
	}

	active = false;
}

void Override::Toggle(D3D11Base::ID3D11Device *device)
{
	if (active)
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
