#include "nvprofile.h"

#include <nvapi.h>
#include "../util.h"
#include "Globals.h"

#include <unordered_set>
#include <fstream>
#include <string>


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

static HMODULE nvDLL;
typedef NvAPI_Status *(__cdecl *nvapi_QueryInterfaceType)(unsigned int offset);
static nvapi_QueryInterfaceType nvapi_QueryInterfacePtr;
typedef NvAPI_Status(__cdecl *tNvAPI_DRS_SaveSettingsToFileEx)(NvDRSSessionHandle hSession, NvAPI_UnicodeString fileName);
static tNvAPI_DRS_SaveSettingsToFileEx _NvAPI_DRS_SaveSettingsToFileEx;

static NvAPI_Status NvAPI_DRS_SaveSettingsToFileEx(NvDRSSessionHandle hSession, NvAPI_UnicodeString fileName)
{
	if (!nvDLL) {
		nvDLL = GetModuleHandle(L"nvapi64.dll");
		if (!nvDLL) {
			nvDLL = GetModuleHandle(L"nvapi.dll");
		}
		if (!nvDLL) {
			LogInfo("Can't get nvapi handle\n");
			return NVAPI_ERROR;
		}
	}
	if (!nvapi_QueryInterfacePtr) {
		nvapi_QueryInterfacePtr = (nvapi_QueryInterfaceType)GetProcAddress(nvDLL, "nvapi_QueryInterface");
		LogDebug("nvapi_QueryInterfacePtr @ 0x%p\n", nvapi_QueryInterfacePtr);
		if (!nvapi_QueryInterfacePtr) {
			LogInfo("Unable to call NvAPI_QueryInterface\n");
			return NVAPI_ERROR;
		}
	}
	if (!_NvAPI_DRS_SaveSettingsToFileEx) {
		_NvAPI_DRS_SaveSettingsToFileEx	= (tNvAPI_DRS_SaveSettingsToFileEx)nvapi_QueryInterfacePtr(0x1267818E);
		LogDebug("NvAPI_DRS_SaveSettingsToFileEx @ 0x%p\n", _NvAPI_DRS_SaveSettingsToFileEx);
		if (!_NvAPI_DRS_SaveSettingsToFileEx) {
			LogInfo("Unable to call NvAPI_DRS_SaveSettingsToFileEx\n");
			return NVAPI_ERROR;
		}
	}
	return (*_NvAPI_DRS_SaveSettingsToFileEx)(hSession, fileName);
}

static bool next_broken_utf16_line(std::ifstream *fp, std::wstring *line)
{
	char c[2];

	*line = L"";

	while (fp->good()) {
		fp->read(c, 2);
		if (c[0] == '\r' && c[1] == '\0') {
			fp->read(c, 2);
			if (c[0] == '\n' && c[1] == '\0')
				return true;
			else
				line->push_back(L'\r');
		}
		line->push_back(*(wchar_t*)&c);
	}

	return false;
}

// Any settings with the internal flag are specially encoded, so we have to
// decode them to be able to make sense of them... but the DRS API does not
// pass this flag to us and they differ for each profile so it seems our only
// option is to ask the driver to save the profiles to disk, and read that back
// in to identify these settings. This is the same approach used by nvidia
// profile inspector.
static void identify_internal_settings(NvDRSSessionHandle session,
		wchar_t *profile_name,
		std::unordered_set<unsigned> *internal_settings)
{
	wchar_t tmp[MAX_PATH], path[MAX_PATH];
	NvAPI_Status status = NVAPI_OK;
	wstring line, search, sid;
	std::ifstream fp;
	int found = 0;
	unsigned id;

	if (!GetTempPath(MAX_PATH, tmp))
		goto err;

	if (!GetTempFileName(tmp, L"3DM", 0, path))
		goto err;

	// FIXME: If we are looking up multiple profiles we should do this only
	// once. If we are looking up all the profiles it might be worth
	// creating an unordered_map to map each profile to their internal
	// settings.
	status = NvAPI_DRS_SaveSettingsToFileEx(session, (NvU16*)path);
	if (status != NVAPI_OK)
		goto err_rm;

	// We can't treat the file as UTF16, since the encoding scheme they
	// used causes internal strings in the file to violate that encoding
	// and will confuse any parsers (they may stop reading prematurely).
	// Safest thing to do is treat it as a stream of bytes and find the
	// newline characters and strings we are looking for ourselves.
	// Notably, there are potentially edge cases that could screw even this
	// up, but the worst case should only be that we miss one internal
	// string setting... and in practice they fluked avoiding that so far.
	fp.open(path, std::ios::binary);

	search = L"Profile \"";
	search += profile_name;
	search += L"\"";

	while (next_broken_utf16_line(&fp, &line)) {
		if (found) {
			if (!line.compare(L"EndProfile")) {
				found = 2;
				break;
			}

			if (line.length() > 22 && !line.compare(line.length() - 22, 22, L"InternalSettingFlag=V0")) {
				sid = line.substr(line.find(L" ID_0x") + 6, 8);
				swscanf_s(sid.c_str(), L"%08x", &id);
				// LogDebug("Identified Internal Setting ID 0x%08x\n", line.c_str(), id);
				internal_settings->insert(id);
			}

		} else if (!line.compare(0, search.length(), search)) {
			found = 1;
		}
	}
	if (found != 2)
		goto err_close;

	fp.close();

	DeleteFile(path);
	return;

err_close:
	fp.close();
err_rm:
	DeleteFile(path);
err:
	log_nv_error(status);
	LogInfo("WARNING: Unable to determine which settings are internal - some settings will be garbage\n");
}

unsigned char internal_setting_key[] = {
	0x2f, 0x7c, 0x4f, 0x8b, 0x20, 0x24, 0x52, 0x8d, 0x26, 0x3c, 0x94, 0x77, 0xf3, 0x7c, 0x98, 0xa5,
	0xfa, 0x71, 0xb6, 0x80, 0xdd, 0x35, 0x84, 0xba, 0xfd, 0xb6, 0xa6, 0x1b, 0x39, 0xc4, 0xcc, 0xb0,
	0x7e, 0x95, 0xd9, 0xee, 0x18, 0x4b, 0x9c, 0xf5, 0x2d, 0x4e, 0xd0, 0xc1, 0x55, 0x17, 0xdf, 0x18,
	0x1e, 0x0b, 0x18, 0x8b, 0x88, 0x58, 0x86, 0x5a, 0x1e, 0x03, 0xed, 0x56, 0xfb, 0x16, 0xfe, 0x8a,
	0x01, 0x32, 0x9c, 0x8d, 0xf2, 0xe8, 0x4a, 0xe6, 0x90, 0x8e, 0x15, 0x68, 0xe8, 0x2d, 0xf4, 0x40,
	0x37, 0x9a, 0x72, 0xc7, 0x02, 0x0c, 0xd1, 0xd3, 0x58, 0xea, 0x62, 0xd1, 0x98, 0x36, 0x2b, 0xb2,
	0x16, 0xd5, 0xde, 0x93, 0xf1, 0xba, 0x74, 0xe3, 0x32, 0xc4, 0x9f, 0xf6, 0x12, 0xfe, 0x18, 0xc0,
	0xbb, 0x35, 0x79, 0x9c, 0x6b, 0x7a, 0x23, 0x7f, 0x2b, 0x15, 0x9b, 0x42, 0x07, 0x1a, 0xff, 0x69,
	0xfb, 0x9c, 0xbd, 0x23, 0x97, 0xa8, 0x22, 0x63, 0x8f, 0x32, 0xc8, 0xe9, 0x9b, 0x63, 0x1c, 0xee,
	0x2c, 0xd9, 0xed, 0x8d, 0x3a, 0x35, 0x9c, 0xb1, 0x60, 0xae, 0x5e, 0xf5, 0x97, 0x6b, 0x9f, 0x20,
	0x8c, 0xf7, 0x98, 0x2c, 0x43, 0x79, 0x95, 0x1d, 0xcd, 0x46, 0x36, 0x6c, 0xd9, 0x67, 0x20, 0xab,
	0x41, 0x22, 0x21, 0xe5, 0x55, 0x82, 0xf5, 0x27, 0x20, 0xf5, 0x08, 0x07, 0x3f, 0x6d, 0x69, 0xd9,
	0x1c, 0x4b, 0xf8, 0x26, 0x03, 0x6e, 0xb2, 0x3f, 0x1e, 0xe6, 0xca, 0x3d, 0x61, 0x44, 0xb0, 0x92,
	0xaf, 0xf0, 0x88, 0xca, 0xe0, 0x5f, 0x5d, 0xf4, 0xdf, 0xc6, 0x4c, 0xa4, 0xe0, 0xca, 0xb0, 0x20,
	0x5d, 0xc0, 0xfa, 0xdd, 0x9a, 0x34, 0x8f, 0x50, 0x79, 0x5a, 0x5f, 0x7c, 0x19, 0x9e, 0x40, 0x70,
	0x71, 0xb5, 0x45, 0x19, 0xb8, 0x53, 0xfc, 0xdf, 0x24, 0xbe, 0x22, 0x1c, 0x79, 0xbf, 0x42, 0x89,
};

unsigned decode_internal_dword(unsigned id, unsigned val)
{
	unsigned off, key;

	off = (id << 1);
	key = internal_setting_key[(off+3) % 256] << 24
	    | internal_setting_key[(off+2) % 256] << 16
	    | internal_setting_key[(off+1) % 256] << 8
	    | internal_setting_key[(off+0) % 256];
	// LogDebug("Decoded Setting ID=%08x Off=%u Key=%08x Enc=%08x Dec=%08x\n", id, off, key, val, key ^ val);
	return key ^ val;
}

void decode_internal_string(unsigned id, NvAPI_UnicodeString val)
{
	unsigned off, i;
	wchar_t key;

	for (i = 0; i < NVAPI_UNICODE_STRING_MAX; i++) {
		off = ((id << 1) + i*2);
		key = internal_setting_key[(off+1) % 256] << 8
		    | internal_setting_key[(off+0) % 256];
		// LogInfo("Decoded SettingString ID=%08x Off=%u Key=%04x Enc=%04x Dec=%04x\n", id, off, key, val[i], key ^ val[i]);
		val[i] = val[i] ^ key;
		if (!val[i]) // Decoded the NULL terminator
			break;
	}
}

// We aim to make the output of this similar to Geforce Profile Manager so that
// it is familiar, and so that perhaps it could even be copied from one to the
// other, but we definitely do NOT want it to be identical - we want to show
// real setting names, to decode the internal settings, and we definitely don't
// want that horrible encoding nightmare that prevents the file being easily
// opened in anything more sophisticated than notepad!
void _log_nv_profile(NvDRSSessionHandle session, NvDRSProfileHandle profile, NVDRS_PROFILE *info)
{
	std::unordered_set<unsigned> internal_settings;
	NvAPI_Status status = NVAPI_OK;
	NVDRS_APPLICATION *apps = NULL;
	NVDRS_SETTING *settings = NULL;
	unsigned len, dval;
	char *name;
	NvU32 i;

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

	identify_internal_settings(session, (wchar_t*)info->profileName, &internal_settings);

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
			dval = settings[i].u32CurrentValue;
			if (settings[i].isCurrentPredefined
					&& settings[i].isPredefinedValid
					&& internal_settings.count(settings[i].settingId)) {
				dval = decode_internal_dword(settings[i].settingId, dval);
			}
			LogInfo("    Setting ID_0x%08x = 0x%08x", settings[i].settingId, dval);
			break;
		case NVDRS_BINARY_TYPE:
			// Do these even exist?
			LogInfo(" XXX Setting Binary XXX (length=%d) :", settings[i].binaryCurrentValue.valueLength);
			for (len = 0; len < settings[i].binaryCurrentValue.valueLength; len++)
				LogInfo(" %02x", settings[i].binaryCurrentValue.valueData[len]);
			break;
		case NVDRS_WSTRING_TYPE:
			if (settings[i].isCurrentPredefined
					&& settings[i].isPredefinedValid
					&& internal_settings.count(settings[i].settingId)) {
				decode_internal_string(settings[i].settingId, settings[i].wszCurrentValue);
			}
			LogInfo("    SettingString ID_0x%08x = \"%S\"", settings[i].settingId, (wchar_t*)settings[i].wszCurrentValue);
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
