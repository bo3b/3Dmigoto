#include "Override.h"

#include "Main.h"

Override::Override(D3D11Wrapper::ID3D11Device *device)
{
	mWrappedDevice = device;

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
}


void Override::Activate()
{
	// In general we want to be calling the _real_ d3d11 device here.
	using namespace D3D11Base;

	NvAPI_Status err;

	if (mOverrideSeparation != INFINITY) {
		err = NvAPI_Stereo_GetSeparation(mWrappedDevice->mStereoHandle, &mUserSeparation);
		if (err != NVAPI_OK) {
			LogDebug("    Stereo_GetSeparation failed: %d\n", err);
		}

		D3D11Wrapper::NvAPIOverride();
		err = NvAPI_Stereo_SetSeparation(mWrappedDevice->mStereoHandle, mOverrideSeparation);
		if (err != NVAPI_OK) {
			LogDebug("    Stereo_SetSeparation failed: %d\n", err);
		}
	}
	if (mOverrideConvergence != INFINITY) {
		err = NvAPI_Stereo_GetConvergence(mWrappedDevice->mStereoHandle, &mUserConvergence);
		if (err != NVAPI_OK) {
			LogDebug("    Stereo_GetConvergence failed: %d\n", err);
		}

		D3D11Wrapper::NvAPIOverride();
		err = NvAPI_Stereo_SetConvergence(mWrappedDevice->mStereoHandle, mOverrideConvergence);
		if (err != NVAPI_OK) {
			LogDebug("    Stereo_SetConvergence failed: %d\n", err);
		}
	}
}

void Override::Deactivate()
{
	using namespace D3D11Base;

	NvAPI_Status err;

	if (mUserSeparation != INFINITY) {
		D3D11Wrapper::NvAPIOverride();
		err = NvAPI_Stereo_SetSeparation(mWrappedDevice->mStereoHandle, mUserSeparation);
		if (err != NVAPI_OK) {
			LogDebug("    Stereo_SetSeparation failed: %d\n", err);
		}
		mUserSeparation = INFINITY;
	}
	if (mUserConvergence != INFINITY) {
		D3D11Wrapper::NvAPIOverride();
		err = NvAPI_Stereo_SetConvergence(mWrappedDevice->mStereoHandle, mUserConvergence);
		if (err != NVAPI_OK) {
			LogDebug("    Stereo_SetConvergence failed: %d\n", err);
		}
		mUserConvergence = INFINITY;
	}
}
