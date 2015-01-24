#include "Override.h"

#include "Main.h"
#include "globals.h"

Override::Override()
{
	// It's important for us to know if any are actively in use or not, so setting them
	// to infinity by default allows us to know when they are valid.
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

static void UpdateIniParams(D3D11Base::ID3D11Device *device,
		D3D11Wrapper::ID3D11Device* wrapper,
		DirectX::XMFLOAT4 *params, DirectX::XMFLOAT4 *save)
{
	D3D11Base::ID3D11DeviceContext* realContext; device->GetImmediateContext(&realContext);
	D3D11Base::D3D11_MAPPED_SUBRESOURCE mappedResource;

	if (params->x == FLT_MAX && params->y == FLT_MAX &&
	    params->z == FLT_MAX && params->w == FLT_MAX) {
		return;
	}

	if (save)
		memcpy(save, &G->iniParams, sizeof(G->iniParams));

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

	if (mOverrideSeparation != FLT_MAX) {
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
	if (mOverrideConvergence != FLT_MAX) {
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

	UpdateIniParams(device, wrapper, &mOverrideParams, &mSavedParams);

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

	if (mUserSeparation != FLT_MAX) {
		D3D11Wrapper::NvAPIOverride();
		err = NvAPI_Stereo_SetSeparation(wrapper->mStereoHandle, mUserSeparation);
		if (err != NVAPI_OK) {
			LogDebug("    Stereo_SetSeparation failed: %d\n", err);
		}
		mUserSeparation = FLT_MAX;
	}
	if (mUserConvergence != FLT_MAX) {
		D3D11Wrapper::NvAPIOverride();
		err = NvAPI_Stereo_SetConvergence(wrapper->mStereoHandle, mUserConvergence);
		if (err != NVAPI_OK) {
			LogDebug("    Stereo_SetConvergence failed: %d\n", err);
		}
		mUserConvergence = FLT_MAX;
	}

	UpdateIniParams(device, wrapper, &mSavedParams, NULL);

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
