#include <nvapi.h>
#include "../util.h"
#include "Globals.h"

// Recommended reading:
// NVIDIA Driver Settings Programming Guide
// And the nvapi reference

static void log_nv_error(NvAPI_Status status)
{
	NvAPI_ShortString desc = {0};

	if (status == NVAPI_OK)
		return;

	NvAPI_GetErrorMessage(status, desc);
	LogInfo(" NVAPI error: %s\n", desc);
}

// We aim to make the output of this similar to Geforce Profile Manager so that
// it is familiar, and so that perhaps it could even be copied from one to the
// other, but we definitely do NOT want it to be identical - we want to show
// real setting names, to decode the internal settings, and we definitely don't
// want that horrible encoding nightmare that prevents the file being easily
// opened in anything more sophisticated than notepad!
void _log_nv_profile(NvDRSSessionHandle session, NvDRSProfileHandle profile, NVDRS_PROFILE *info)
{
	NvAPI_Status status = NVAPI_OK;
	NVDRS_APPLICATION *apps = NULL;
	NVDRS_SETTING *settings = NULL;
	unsigned len;
	NvU32 i;

	info->version = NVDRS_PROFILE_VER;
	status = NvAPI_DRS_GetProfileInfo(session, profile, info);
	if (status != NVAPI_OK)
		goto bail;

	LogInfo("Profile \"%S\"%s\n", info->profileName,
			info->isPredefined ? "" : " UserSpecified=true");

	if (info->gpuSupport.geforce && info->gpuSupport.quadro) // CHECKME
		LogInfo("    ShowOn All\n");
	else if (info->gpuSupport.geforce)
		LogInfo("    ShowOn GeForce\n");
	else if (info->gpuSupport.quadro)
		LogInfo("    ShowOn Quadro\n");
	if (info->gpuSupport.nvs) // What is this?
		LogInfo("    ShowOn NVS\n");

	// XXX: Geforce Profile Manager says "ProfileType Application/Global"...
	// not sure where that comes from

	if (info->numOfApps > 0) {
		apps = new NVDRS_APPLICATION[info->numOfApps];
		memset(apps, 0, sizeof(NVDRS_APPLICATION) * info->numOfApps);
		apps[0].version = NVDRS_APPLICATION_VER;
		status = NvAPI_DRS_EnumApplications(session, profile, 0, &info->numOfApps, apps);
		if (status != NVAPI_OK)
			goto bail;
	}

	for (i = 0; i < info->numOfApps; i++) {
		LogInfo("    Executable \"%S\"", apps[i].appName);
		if (apps[i].userFriendlyName[0])
			LogInfo(" Name=\"%S\"", apps[i].userFriendlyName);
		if (!apps[i].isPredefined)
			LogInfo(" UserSpecified=true");
		LogInfo("\n");
		// XXX There's several other pieces of info here we might need
		// to worry about:
		// - launcher
		// - fileInFolder
		// - isMetro
	}

	if (info->numOfSettings > 0) {
		settings = new NVDRS_SETTING[info->numOfSettings];
		memset(settings, 0, sizeof(NVDRS_SETTING) * info->numOfSettings);
		settings[0].version = NVDRS_SETTING_VER;
		status = NvAPI_DRS_EnumSettings(session, profile, 0, &info->numOfSettings, settings);
		if (status != NVAPI_OK)
			goto bail;
	}

	for (i = 0; i < info->numOfSettings; i++) {
		if (settings[i].settingLocation != NVDRS_CURRENT_PROFILE_LOCATION) {
			// Inherited from base or global profiles, not this one
			continue;
		}

		switch (settings[i].settingType) {
		case NVDRS_DWORD_TYPE:
			// FIXME: Decode
			LogInfo("    Setting ID_0x%08x = 0x%08x",
					settings[i].settingId,
					settings[i].u32CurrentValue);
			break;
		case NVDRS_BINARY_TYPE:
			LogInfo(" XXX Setting Binary XXX (length=%d) :", settings[i].binaryCurrentValue.valueLength);
			for (len = 0; len < settings[i].binaryCurrentValue.valueLength; len++)
				LogInfo(" %02x", settings[i].binaryCurrentValue.valueData[len]);
			break;
		case NVDRS_WSTRING_TYPE:
			// FIXME: Decode
			LogInfo("    SettingString ID_0x%08x = \"%S\"",
					settings[i].settingId,
					settings[i].wszCurrentValue);
			break;
		}
		if (!settings[i].isCurrentPredefined)
			LogInfo(" UserSpecified=true");
		if (settings[i].settingName[0])
			LogInfo(" // %S", settings[i].settingName);
		LogInfo("\n");
	}

	LogInfo("EndProfile\n\n");

bail:
	log_nv_error(status);
	delete [] apps;
	delete [] settings;
}

void log_nv_profile(NvDRSSessionHandle session, NvDRSProfileHandle profile)
{
	NVDRS_PROFILE info = {0};
	NvAPI_Status status = NVAPI_OK;

	info.version = NVDRS_PROFILE_VER;
	status = NvAPI_DRS_GetProfileInfo(session, profile, &info);
	if (status != NVAPI_OK)
		goto bail;

	return _log_nv_profile(session, profile, &info);

bail:
	log_nv_error(status);
}

void log_all_nv_profiles(NvDRSSessionHandle session)
{
	NvAPI_Status status = NVAPI_OK;
	NvDRSProfileHandle profile = 0;
	unsigned i;

	for (i = 0; (status = NvAPI_DRS_EnumProfiles(session, i, &profile)) == NVAPI_OK; i++)
		log_nv_profile(session, profile);

	if (status != NVAPI_END_ENUMERATION)
		log_nv_error(status);
}

// This function logs the contents of all profiles that may have an influence
// on the current game - the base profile, global default profile (if different
// to the base profile), the default stereo profile (or rather will once we
// update our nvapi headers), and
void log_relevant_nv_profiles()
{
	NvDRSSessionHandle session = 0;
	NvDRSProfileHandle base_profile = 0;
	NvDRSProfileHandle global_profile = 0;
	NvDRSProfileHandle stereo_profile = 0;
	NvDRSProfileHandle profile = 0;
	NVDRS_PROFILE base_info = {0};
	NVDRS_PROFILE global_info = {0};
	NvAPI_Status status = NVAPI_OK;
	NvU32 len = 0;
	char *default_stereo_profile = NULL;
	wchar_t path[MAX_PATH];
	NVDRS_APPLICATION app = {0};

	if (!GetModuleFileName(0, path, MAX_PATH)) {
		LogInfo("GetModuleFileName failed\n");
		goto bail;
	}
	LogInfo("\nLooking up profiles related to %S\n", path);
	LogInfo("----------- Driver profile settings -----------\n");

	// NvAPI_Initialize() should have already been called

	status = NvAPI_DRS_CreateSession(&session);
	if (status != NVAPI_OK)
		goto err_no_session;

	status = NvAPI_DRS_LoadSettings(session);
	if (status != NVAPI_OK)
		goto bail;

	status = NvAPI_DRS_GetBaseProfile(session, &base_profile);
	if (status != NVAPI_OK)
		goto bail;

	base_info.version = NVDRS_PROFILE_VER;
	status = NvAPI_DRS_GetProfileInfo(session, base_profile, &base_info);
	if (status != NVAPI_OK)
		goto bail;

	LogInfo("BaseProfile \"%S\"\n", base_info.profileName);

	status = NvAPI_DRS_GetCurrentGlobalProfile(session, &global_profile);
	if (status != NVAPI_OK)
		goto bail;

	global_info.version = NVDRS_PROFILE_VER;
	status = NvAPI_DRS_GetProfileInfo(session, global_profile, &global_info);
	if (status != NVAPI_OK)
		goto bail;

	LogInfo("SelectedGlobalProfile \"%S\"\n", global_info.profileName);

	// TODO: Log current stereo profile
	// FIXME: Update nvapi headers to latest public version to get this function:
	// status = NvAPI_Stereo_GetDefaultProfile(0, 0, &len);
	// if (status != NVAPI_OK)
	// 	goto bail;
	// if (len) {
	// 	default_stereo_profile = new char[len];
	// 	status = NvAPI_Stereo_GetDefaultProfile(len, default_stereo_profile, &len);
	// 	if (status != NVAPI_OK)
	// 		goto bail;
	// 	// Making this look like a comment since Geforce Profile
	// 	// Manager does not output it:
	// 	LogInfo("// Default stereo profile: %s\n", default_stereo_profile);

	// } else {
	// 	LogInfo("// No default stereo profile set\n");
	// }

	LogInfo("\n");

	if (G->dump_all_profiles) {
		log_all_nv_profiles(session);
	} else {
		_log_nv_profile(session, base_profile, &base_info);
		if (base_profile != global_profile)
			_log_nv_profile(session, global_profile, &global_info);

		app.version = NVDRS_APPLICATION_VER;
		status = NvAPI_DRS_FindApplicationByName(session, (NvU16*)path, &profile, &app);
		if (status == NVAPI_OK) {
			log_nv_profile(session, profile);
		} else {
			LogInfo("Cannot locate application profile: ");
			// Not necessarily an error, since the application may not have
			// a profile. Will still print the nvapi error in the common
			// cleanup/error paths below.
		}
	}

bail:
	delete default_stereo_profile;
	NvAPI_DRS_DestroySession(session);
err_no_session:
	log_nv_error(status);
	LogInfo("----------- End driver profile settings -----------\n");
}

void log_nv_driver_version()
{
	NvU32 version;
	NvAPI_ShortString branch;
	NvAPI_Status status = NVAPI_OK;

	status = NvAPI_SYS_GetDriverAndBranchVersion(&version, branch);
	if (status == NVAPI_OK) {
		LogInfo("NVIDIA driver version %u.%u (branch %s)\n", version / 100, version % 100, branch);
	} else {
		LogInfo("Error getting NVIDIA driver version:");
		log_nv_error(status);
	}
}
