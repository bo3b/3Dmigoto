#include <nvapi.h>
#include "../util.h"
#include "Globals.h"

// Recommended reading:
// NVIDIA Driver Settings Programming Guide
// And the nvapi reference

static EnumName_t<char*, unsigned> NVStereoSettingNames[] = {
	// Same name we decided to use in nvidia profile inspector:
	{"StereoProfile", 0x701EB457},

	// Primary name table:
	{"Time", 0x70ad05c8},
	{"RunTimeName", 0x701a8be4},
	{"EnableConsumerStereoSupport", 0x70cb9168},
	{"StereoViewer", 0x704915a1},
	{"StereoViewerType", 0x708f9ef7},
	{"ShowAllViewerTypes", 0x708e5cb4},
	{"StereoAdjustEnable", 0x70538ab1},
	{"StereoDisableTnL", 0x70633bd9},
	{"StereoTransformationType", 0x70c27e3c},
	{"StereoSeparation", 0x70933c00},
	{"StereoSeparationStep", 0x7082555b},
	{"StereoConvergence", 0x708db8c5},
	{"StereoVRConvergenceBias", 0x708db8c6},
	{"StereoConvergenceMultiplier", 0x70efbb5b},
	{"StereoVRRefreshRateOverride", 0x708db8c8},
	{"StereoVRVsync", 0x708db8c9},
	{"RHW2DDetectionMin", 0x7029432b},
	{"RHWGreaterAtScreen", 0x702c861a},
	{"RHWEqualAtScreen", 0x70ab2e09},
	{"RHWLessAtScreen", 0x70381472},
	{"AutoConvergence", 0x702a0ab2},
	{"AutoConvergenceAdjustPace", 0x70bf3c6b},
	{"StereoToggle", 0x70d76b8b},
	{"SaveStereoImage", 0x70121853},
	{"StereoVerticalAdjustMore", 0x7087fe61},
	{"StereoVerticalAdjustLess", 0x703acfc6},
	{"StereoHorizontalAdjustMore", 0x70062f07},
	{"StereoHorizontalAdjustLess", 0x70871a39},
	{"StereoSeparationAdjustMore", 0x70ab8d32},
	{"StereoSeparationAdjustLess", 0x705d1e02},
	{"StereoConvergenceAdjustMore", 0x701ed576},
	{"StereoConvergenceAdjustLess", 0x70d4add7},
	{"StereoToggleMode", 0x70d76b8c},
	{"StereoSuggestSettings", 0x706315af},
	{"StereoUnsuggestSettings", 0x7017861c},
	{"WriteConfig", 0x700498b3},
	{"DeleteConfig", 0x70c73ba2},
	{"ToggleLaserSight", 0x70b7bd1f},
	{"LaserAdjustXPlus", 0x70d8bae6},
	{"LaserAdjustXMinus", 0x7048b7dc},
	{"LaserAdjustYPlus", 0x7024eda4},
	{"LaserAdjustYMinus", 0x70fb9e1e},
	{"ToggleAutoConvergence", 0x70085de3},
	{"ToggleAutoConvergenceRestore", 0x703bc51e},
	{"RHWAtScreenMore", 0x7066a22e},
	{"RHWAtScreenLess", 0x709139ad},
	{"RHWLessAtScreenMore", 0x704e4bca},
	{"RHWLessAtScreenLess", 0x70b378a1},
	{"GammaAdjustMore", 0x703f4521},
	{"GammaAdjustLess", 0x70e8420c},
	{"GlassesDelayPlus", 0x701fc5b4},
	{"GlassesDelayMinus", 0x70b8a743},
	{"FavorSZ", 0x705faed7},
	{"LaserSight", 0x7058b6e1},
	{"LaserSightFile", 0x707ac50d},
	{"LaserSightEnabled", 0x7054837a},
	{"LaserSightIndex", 0x70da83c6},
	{"LaserSightProperty", 0x7032243a},
	{"StereoPointer", 0x70364596},
	{"GameSpecific0", 0x702244b7},
	{"StereoDefaultOn", 0x70ab30a7},
	{"FrustumAdjustMode", 0x70a1411a},
	{"MonitorSize", 0x7086ebe9},
	{"MaxMonitorSize", 0x7032022c},
	{"MaxVertexCount", 0x709e4a94},
	{"PartialClearMode", 0x709794cc},
	{"LaserXAdjust", 0x7057e831},
	{"LaserYAdjust", 0x70225308},
	{"LaserZAdjust", 0x7014fca2},
	{"StereoRefreshDefaultOn", 0x702ba385},
	{"MixedTnL", 0x70bd11e0},
	{"StereoGamma", 0x70c8b5d1},
	{"LineCodeColor", 0x70dc4a12},
	{"LeftAnaglyphFilter", 0x70d51cd1},
	{"RightAnaglyphFilter", 0x70f4a930},
	{"InterleavePattern0", 0x70b1c8cc},
	{"InterleavePattern1", 0x7091a772},
	{"StereoForceVSync", 0x70aae185},
	{"StereoColorKey", 0x70e5773b},
	{"ZDirection", 0x70b17872},
	{"StereoCompatibility", 0x70a2000e},
	{"LeftColorFilter0", 0x70ac6888},
	{"LeftColorFilter1", 0x7090b6ca},
	{"RightColorFilter0", 0x70b9a2f7},
	{"RightColorFilter1", 0x70aca0cc},
	{"SharpVPI", 0x706e0041},
	{"StereoMode", 0x701baa09},
	{"Watchdog", 0x700a5654},
	{"StereoOSDEnable", 0x70f455aa},
	{"StereoOrthoEnable", 0x703564f6},
	{"StereoTextureEnable", 0x70edb381},
	{"StereoNotSupported", 0x709aa171},
	{"ModesetWarning", 0x70969bb0},
	{"StereoFirstTime", 0x70af6400},
	{"StereoRefreshRate", 0x70ded3c0},
	{"GameConfigs", 0x704a905a},
	{"CompareEyes", 0x70729e58},
	{"CompareFrom", 0x70efb726},
	{"StereoImageType", 0x7097906c},
	{"SnapShotQuality", 0x7004e7a6},
	{"NoLockSubstitute", 0x7005ad16},
	{"PushbufSubstituteSize", 0x7054fbf8},
	{"DiscardHotkeys", 0x70175566},
	{"StereoLCDPatternType", 0x707cfb97},
	{"GlassesSwitchDelay", 0x70057bb6},
	{"StartZBit", 0x7044d7a6},
	{"DisableOnOutOfMemory", 0x70c71508},
	{"StereoWindowedEnable", 0x709b3484},
	{"AllowNonExclusiveStereo", 0x702c7709},
	{"Rhwinf", 0x706e1913},
	{"Rhwscr", 0x70a4995c},
	{"Zinf", 0x70fc13ad},
	{"Zscr", 0x707f0e69},
	{"InGameLaserSight", 0x7064f0c2},
	{"CutoffNearDepthLess", 0x70d1bdb5},
	{"CutoffNearDepthMore", 0x7020c991},
	{"CutoffFarDepthLess", 0x704c9a46},
	{"CutoffFarDepthMore", 0x70fbc04d},
	{"CutoffStepLess", 0x704b45c7},
	{"CutoffStepMore", 0x700f2971},
	{"StereoCutoffDepthNear", 0x7050e011},
	{"StereoCutoffDepthFar", 0x70add220},
	{"StereoCutoff", 0x709a1ddf},
	{"EnableCE", 0x702b8c95},
	{"MediaPlayer", 0x70a8fc7f},
	{"StereoDX9", 0x70d10d2b},
	{"StereoMsgVerticalOffset", 0x70160ebf},
	{"LaserSightTrigger", 0x70031b88},
	{"StereoLaserSightMaxCount", 0x70bc864d},
	{"StereoLaserSightCount", 0x70077042},
	{"StereoEasyZCheck", 0x70b6d6ed},
	{"StereoStrictLSCheck", 0x709bc378},
	{"StereoDisableAsync", 0x70de5533},
	{"EnablePartialStereoBlit", 0x7096eced},
	{"StereoMemoEnabled", 0x707f4b45},
	{"StereoNoDepthOverride", 0x709dea62},
	{"StereoFlagsDX10", 0x702442fc},
	{"StereoUseMatrix", 0x70e34a78},
	{"StereoShaderMatrixCheck", 0x7044f8fb},
	{"StereoLogShaders", 0x7052bdd0},
	{"StereoEpsilon", 0x70e5a749},
	{"DelayedStereoDesktop", 0x7042eef1},
	{"DX10VSCBNumber", 0x70f8e408},
	{"DX10DSCBNumber", 0x70092d4a},
	{"InGameLaserSightDX9States", 0x706139ad},
	{"StereoMiscFlags", 0x70ccb5f0},
	{"StereoHiddenProfile", 0x70e46f20},
	{"StereoLinkDll", 0x70e46f2a},
	{"EnableStereoCursor", 0x70e46f2b},
	{"CreateStereoDTAfterPresentNum", 0x70a7fc7f},
	{"Date_Rel", 0x705fafec},
	{"Game", 0x70c8d48e},
	{"Style", 0x709cc5e0},
	{"Publisher", 0x706c7030},
	{"Developer", 0x703c4026},
	{"API", 0x70b5603f},
	{"Value", 0x7049c7ec},
	{"Compat", 0x7051e5f5},
	{"PF_Issues", 0x704cde5a},
	{"Comments", 0x704d456e},
	{"Developer_Issues", 0x704f5928},
	{"P1SH0", 0x70998683},
	{"V1SH0", 0x70e6a3cf},
	{"PSH0", 0x7046516e},
	{"VSH0", 0x708b7af8},
	{"VSH1", 0x708b7af9},
	{"VSH2", 0x708b7afa},
	{"VSH3", 0x708b7afb},
	{"VSH4", 0x708b7afc},
	{"VSH5", 0x708b7afd},
	{"VSH6", 0x708b7afe},
	{"VSH7", 0x708b7aff},
	{"VSH8", 0x708b7b00},
	{"VSH9", 0x708b7b01},
	{"VSH10", 0x708b7b02},
	{"2DDHUDSettings", 0x709adada},
	{"2DDConvergence", 0x709adadb},
	{"Disable2DD", 0x709adadd},
	{"2DD_Notes", 0x709adadc},

	// Secondary name table:
	{"StereoConvergence (Alternate 1)", 0x7077bace},
	{"LaserSight (Alternate 1)", 0x7031a2e7},
	{"FrustumAdjustMode (Alternate 1)", 0x70ed1da7},
	{"StereoTextureEnable (Alternate 1)", 0x70e1518c},
	{"Rhwinf (Alternate 1)", 0x70cc286a},
	{"Rhwscr (Alternate 1)", 0x7030b071},
	{"InGameLaserSight (Alternate 1)", 0x70dd2585},
	{"StereoCutoffDepthNear (Alternate 1)", 0x704ef483},
	{"StereoCutoff (Alternate 1)", 0x704fcf5c},
	{"StereoConvergence (Alternate 2)", 0x7084807e},
	{"LaserSight (Alternate 2)", 0x7045b752},
	{"FrustumAdjustMode (Alternate 2)", 0x70f475a0},
	{"StereoTextureEnable (Alternate 2)", 0x70c0125e},
	{"Rhwinf (Alternate 2)", 0x70a3fee6},
	{"Rhwscr (Alternate 2)", 0x70b57ed1},
	{"InGameLaserSight (Alternate 2)", 0x70e7adad},
	{"StereoCutoffDepthNear (Alternate 2)", 0x7031de06},
	{"StereoCutoff (Alternate 2)", 0x7053569a},
	{"DX10VSCBNumber (Alternate 2)", 0x70f64a32},
};

static char* lookup_setting_name(unsigned id)
{
	for (int i = 0; i < ARRAYSIZE(NVStereoSettingNames); i++) {
		if (NVStereoSettingNames[i].val == id) {
			return NVStereoSettingNames[i].name;
		}
	}

	return NULL;
}


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
	char *name;

	info->version = NVDRS_PROFILE_VER;
	status = NvAPI_DRS_GetProfileInfo(session, profile, info);
	if (status != NVAPI_OK)
		goto bail;

	LogInfo("Profile \"%S\"%s\n", (wchar_t*)info->profileName,
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
		LogInfo("    Executable \"%S\"", (wchar_t*)apps[i].appName);
		if (apps[i].userFriendlyName[0])
			LogInfo(" Name=\"%S\"", (wchar_t*)apps[i].userFriendlyName);
		if (apps[i].launcher[0])
			LogInfo(" Launcher=\"%S\"", (wchar_t*)apps[i].launcher);
		if (apps[i].fileInFolder[0])
			LogInfo(" FindFile=\"%S\"", (wchar_t*)apps[i].fileInFolder);
		if (!apps[i].isPredefined)
			LogInfo(" UserSpecified=true");
		LogInfo("\n");
		// XXX There's one last piece of info here we might need to
		// output, but I don't see anything that looks like it in the
		// Geforce Profile Manager output: isMetro
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
					(wchar_t*)settings[i].wszCurrentValue);
			break;
		}
		if (!settings[i].isCurrentPredefined)
			LogInfo(" UserSpecified=true");
		if (settings[i].settingName[0]) {
			LogInfo(" // %S", (wchar_t*)settings[i].settingName);
		} else {
			name = lookup_setting_name(settings[i].settingId);
			if (name)
				LogInfo(" // %s", name);
		}
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

	LogInfo("BaseProfile \"%S\"\n", (wchar_t*)base_info.profileName);

	status = NvAPI_DRS_GetCurrentGlobalProfile(session, &global_profile);
	if (status != NVAPI_OK)
		goto bail;

	global_info.version = NVDRS_PROFILE_VER;
	status = NvAPI_DRS_GetProfileInfo(session, global_profile, &global_info);
	if (status != NVAPI_OK)
		goto bail;

	LogInfo("SelectedGlobalProfile \"%S\"\n", (wchar_t*)global_info.profileName);

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
