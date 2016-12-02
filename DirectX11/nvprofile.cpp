#include "nvprofile.h"

#include "../util.h"
#include "Globals.h"

#include <unordered_set>
#include <fstream>

std::unordered_map<NvU32, NVDRS_SETTING> profile_settings;

// Recommended reading:
// NVIDIA Driver Settings Programming Guide
// And the nvapi reference

struct NVStereoSetting {
	unsigned id;
	// Setting a flag for known floating point types this since the parsing
	// of floats & ints can be ambiguous (whereas other types are not,
	// though we could use an enum and tag known string types as well).
	// Only setting this on fields identified as floats based on values
	// observed in existing profiles - there may be others:
	bool known_float;
	wchar_t *name;
};

static NVStereoSetting NVStereoSettingNames[] = {
	// Same name we decided to use in nvidia profile inspector:
	{0x701EB457, false, L"StereoProfile"},

	// Primary name table:
	{0x70ad05c8, false, L"Time"},
	{0x701a8be4, false, L"RunTimeName"},
	{0x70cb9168, false, L"EnableConsumerStereoSupport"},
	{0x704915a1, false, L"StereoViewer"},
	{0x708f9ef7, false, L"StereoViewerType"},
	{0x708e5cb4, false, L"ShowAllViewerTypes"},
	{0x70538ab1, false, L"StereoAdjustEnable"},
	{0x70633bd9, false, L"StereoDisableTnL"},
	{0x70c27e3c, false, L"StereoTransformationType"},
	{0x70933c00, false, L"StereoSeparation"},
	{0x7082555b, false, L"StereoSeparationStep"},
	{0x708db8c5,  true, L"StereoConvergence"},
	{0x708db8c6, false, L"StereoVRConvergenceBias"},
	{0x70efbb5b, false, L"StereoConvergenceMultiplier"},
	{0x708db8c8, false, L"StereoVRRefreshRateOverride"},
	{0x708db8c9, false, L"StereoVRVsync"},
	{0x7029432b, false, L"RHW2DDetectionMin"},
	{0x702c861a,  true, L"RHWGreaterAtScreen"},
	{0x70ab2e09, false, L"RHWEqualAtScreen"},
	{0x70381472,  true, L"RHWLessAtScreen"},
	{0x702a0ab2, false, L"AutoConvergence"},
	{0x70bf3c6b,  true, L"AutoConvergenceAdjustPace"},
	{0x70d76b8b, false, L"StereoToggle"},
	{0x70121853, false, L"SaveStereoImage"},
	{0x7087fe61, false, L"StereoVerticalAdjustMore"},
	{0x703acfc6, false, L"StereoVerticalAdjustLess"},
	{0x70062f07, false, L"StereoHorizontalAdjustMore"},
	{0x70871a39, false, L"StereoHorizontalAdjustLess"},
	{0x70ab8d32, false, L"StereoSeparationAdjustMore"},
	{0x705d1e02, false, L"StereoSeparationAdjustLess"},
	{0x701ed576, false, L"StereoConvergenceAdjustMore"},
	{0x70d4add7, false, L"StereoConvergenceAdjustLess"},
	{0x70d76b8c, false, L"StereoToggleMode"},
	{0x706315af, false, L"StereoSuggestSettings"},
	{0x7017861c, false, L"StereoUnsuggestSettings"},
	{0x700498b3, false, L"WriteConfig"},
	{0x70c73ba2, false, L"DeleteConfig"},
	{0x70b7bd1f, false, L"ToggleLaserSight"},
	{0x70d8bae6, false, L"LaserAdjustXPlus"},
	{0x7048b7dc, false, L"LaserAdjustXMinus"},
	{0x7024eda4, false, L"LaserAdjustYPlus"},
	{0x70fb9e1e, false, L"LaserAdjustYMinus"},
	{0x70085de3, false, L"ToggleAutoConvergence"},
	{0x703bc51e, false, L"ToggleAutoConvergenceRestore"},
	{0x7066a22e, false, L"RHWAtScreenMore"},
	{0x709139ad, false, L"RHWAtScreenLess"},
	{0x704e4bca, false, L"RHWLessAtScreenMore"},
	{0x70b378a1, false, L"RHWLessAtScreenLess"},
	{0x703f4521, false, L"GammaAdjustMore"},
	{0x70e8420c, false, L"GammaAdjustLess"},
	{0x701fc5b4, false, L"GlassesDelayPlus"},
	{0x70b8a743, false, L"GlassesDelayMinus"},
	{0x705faed7, false, L"FavorSZ"},
	{0x7058b6e1, false, L"LaserSight"},
	{0x707ac50d, false, L"LaserSightFile"},
	{0x7054837a, false, L"LaserSightEnabled"},
	{0x70da83c6, false, L"LaserSightIndex"},
	{0x7032243a, false, L"LaserSightProperty"},
	{0x70364596, false, L"StereoPointer"},
	{0x702244b7, false, L"GameSpecific0"},
	{0x70ab30a7, false, L"StereoDefaultOn"},
	{0x70a1411a, false, L"FrustumAdjustMode"},
	{0x7086ebe9, false, L"MonitorSize"},
	{0x7032022c, false, L"MaxMonitorSize"},
	{0x709e4a94, false, L"MaxVertexCount"},
	{0x709794cc, false, L"PartialClearMode"},
	{0x7057e831,  true, L"LaserXAdjust"},
	{0x70225308,  true, L"LaserYAdjust"},
	{0x7014fca2,  true, L"LaserZAdjust"},
	{0x702ba385, false, L"StereoRefreshDefaultOn"},
	{0x70bd11e0, false, L"MixedTnL"},
	{0x70c8b5d1, false, L"StereoGamma"},
	{0x70dc4a12, false, L"LineCodeColor"},
	{0x70d51cd1, false, L"LeftAnaglyphFilter"},
	{0x70f4a930, false, L"RightAnaglyphFilter"},
	{0x70b1c8cc, false, L"InterleavePattern0"},
	{0x7091a772, false, L"InterleavePattern1"},
	{0x70aae185, false, L"StereoForceVSync"},
	{0x70e5773b, false, L"StereoColorKey"},
	{0x70b17872, false, L"ZDirection"},
	{0x70a2000e, false, L"StereoCompatibility"},
	{0x70ac6888, false, L"LeftColorFilter0"},
	{0x7090b6ca, false, L"LeftColorFilter1"},
	{0x70b9a2f7, false, L"RightColorFilter0"},
	{0x70aca0cc, false, L"RightColorFilter1"},
	{0x706e0041, false, L"SharpVPI"},
	{0x701baa09, false, L"StereoMode"},
	{0x700a5654, false, L"Watchdog"},
	{0x70f455aa, false, L"StereoOSDEnable"},
	{0x703564f6, false, L"StereoOrthoEnable"},
	{0x70edb381, false, L"StereoTextureEnable"},
	{0x709aa171, false, L"StereoNotSupported"},
	{0x70969bb0, false, L"ModesetWarning"},
	{0x70af6400, false, L"StereoFirstTime"},
	{0x70ded3c0, false, L"StereoRefreshRate"},
	{0x704a905a, false, L"GameConfigs"},
	{0x70729e58, false, L"CompareEyes"},
	{0x70efb726, false, L"CompareFrom"},
	{0x7097906c, false, L"StereoImageType"},
	{0x7004e7a6, false, L"SnapShotQuality"},
	{0x7005ad16, false, L"NoLockSubstitute"},
	{0x7054fbf8, false, L"PushbufSubstituteSize"},
	{0x70175566, false, L"DiscardHotkeys"},
	{0x707cfb97, false, L"StereoLCDPatternType"},
	{0x70057bb6, false, L"GlassesSwitchDelay"},
	{0x7044d7a6, false, L"StartZBit"},
	{0x70c71508, false, L"DisableOnOutOfMemory"},
	{0x709b3484, false, L"StereoWindowedEnable"},
	{0x702c7709, false, L"AllowNonExclusiveStereo"},
	{0x706e1913,  true, L"Rhwinf"},
	{0x70a4995c,  true, L"Rhwscr"},
	{0x70fc13ad, false, L"Zinf"}, // float? Only appears once as 0
	{0x707f0e69,  true, L"Zscr"},
	{0x7064f0c2, false, L"InGameLaserSight"},
	{0x70d1bdb5, false, L"CutoffNearDepthLess"},
	{0x7020c991, false, L"CutoffNearDepthMore"},
	{0x704c9a46, false, L"CutoffFarDepthLess"},
	{0x70fbc04d, false, L"CutoffFarDepthMore"},
	{0x704b45c7, false, L"CutoffStepLess"},
	{0x700f2971, false, L"CutoffStepMore"},
	{0x7050e011,  true, L"StereoCutoffDepthNear"},
	{0x70add220,  true, L"StereoCutoffDepthFar"},
	{0x709a1ddf, false, L"StereoCutoff"},
	{0x702b8c95, false, L"EnableCE"},
	{0x70a8fc7f, false, L"MediaPlayer"},
	{0x70d10d2b, false, L"StereoDX9"},
	{0x70160ebf, false, L"StereoMsgVerticalOffset"},
	{0x70031b88, false, L"LaserSightTrigger"},
	{0x70bc864d, false, L"StereoLaserSightMaxCount"},
	{0x70077042, false, L"StereoLaserSightCount"},
	{0x70b6d6ed, false, L"StereoEasyZCheck"},
	{0x709bc378, false, L"StereoStrictLSCheck"},
	{0x70de5533, false, L"StereoDisableAsync"},
	{0x7096eced, false, L"EnablePartialStereoBlit"},
	{0x707f4b45, false, L"StereoMemoEnabled"},
	{0x709dea62, false, L"StereoNoDepthOverride"},
	{0x702442fc, false, L"StereoFlagsDX10"},
	{0x70e34a78, false, L"StereoUseMatrix"},
	{0x7044f8fb, false, L"StereoShaderMatrixCheck"},
	{0x7052bdd0, false, L"StereoLogShaders"},
	{0x70e5a749,  true, L"StereoEpsilon"},
	{0x7042eef1, false, L"DelayedStereoDesktop"},
	{0x70f8e408, false, L"DX10VSCBNumber"},
	{0x70092d4a, false, L"DX10DSCBNumber"},
	{0x706139ad, false, L"InGameLaserSightDX9States"},
	{0x70ccb5f0, false, L"StereoMiscFlags"},
	{0x70e46f20, false, L"StereoHiddenProfile"},
	{0x70e46f2a, false, L"StereoLinkDll"},
	{0x70e46f2b, false, L"EnableStereoCursor"},
	{0x70a7fc7f, false, L"CreateStereoDTAfterPresentNum"},
	{0x705fafec, false, L"Date_Rel"},
	{0x70c8d48e, false, L"Game"},
	{0x709cc5e0, false, L"Style"},
	{0x706c7030, false, L"Publisher"},
	{0x703c4026, false, L"Developer"},
	{0x70b5603f, false, L"API"},
	{0x7049c7ec, false, L"Value"},
	{0x7051e5f5, false, L"Compat"},
	{0x704cde5a, false, L"PF_Issues"},
	{0x704d456e, false, L"Comments"},
	{0x704f5928, false, L"Developer_Issues"},
	{0x70998683, false, L"P1SH0"},
	{0x70e6a3cf, false, L"V1SH0"},
	{0x7046516e, false, L"PSH0"},
	{0x708b7af8, false, L"VSH0"},
	{0x708b7af9, false, L"VSH1"},
	{0x708b7afa, false, L"VSH2"},
	{0x708b7afb, false, L"VSH3"},
	{0x708b7afc, false, L"VSH4"},
	{0x708b7afd, false, L"VSH5"},
	{0x708b7afe, false, L"VSH6"},
	{0x708b7aff, false, L"VSH7"},
	{0x708b7b00, false, L"VSH8"},
	{0x708b7b01, false, L"VSH9"},
	{0x708b7b02, false, L"VSH10"},
	{0x709adada, false, L"2DDHUDSettings"},
	{0x709adadb,  true, L"2DDConvergence"},
	{0x709adadd, false, L"Disable2DD"},
	{0x709adadc, false, L"2DD_Notes"},

	// Secondary name table:
	{0x7077bace,  true, L"StereoConvergence (Alternate 1)"},
	{0x7031a2e7, false, L"LaserSight (Alternate 1)"},
	{0x70ed1da7, false, L"FrustumAdjustMode (Alternate 1)"},
	{0x70e1518c, false, L"StereoTextureEnable (Alternate 1)"},
	{0x70cc286a,  true, L"Rhwinf (Alternate 1)"},
	{0x7030b071,  true, L"Rhwscr (Alternate 1)"},
	{0x70dd2585, false, L"InGameLaserSight (Alternate 1)"},
	{0x704ef483,  true, L"StereoCutoffDepthNear (Alternate 1)"},
	{0x704fcf5c, false, L"StereoCutoff (Alternate 1)"},
	{0x7084807e,  true, L"StereoConvergence (Alternate 2)"},
	{0x7045b752, false, L"LaserSight (Alternate 2)"},
	{0x70f475a0, false, L"FrustumAdjustMode (Alternate 2)"},
	{0x70c0125e, false, L"StereoTextureEnable (Alternate 2)"},
	{0x70a3fee6,  true, L"Rhwinf (Alternate 2)"},
	{0x70b57ed1,  true, L"Rhwscr (Alternate 2)"},
	{0x70e7adad, false, L"InGameLaserSight (Alternate 2)"},
	{0x7031de06,  true, L"StereoCutoffDepthNear (Alternate 2)"},
	{0x7053569a, false, L"StereoCutoff (Alternate 2)"},
	{0x70f64a32, false, L"DX10VSCBNumber (Alternate 2)"},
};

static wchar_t* lookup_setting_name(unsigned id)
{
	for (int i = 0; i < ARRAYSIZE(NVStereoSettingNames); i++) {
		if (NVStereoSettingNames[i].id == id) {
			return NVStereoSettingNames[i].name;
		}
	}

	return NULL;
}

static int lookup_setting_id(const wchar_t *name, NvU32 *id)
{
	NvAPI_Status status;

	status = NvAPI_DRS_GetSettingIdFromName((NvU16*)name, (NvU32*)id);
	if (status == NVAPI_OK)
		return 0;

	for (int i = 0; i < ARRAYSIZE(NVStereoSettingNames); i++) {
		if (!_wcsicmp(NVStereoSettingNames[i].name, name)) {
			*id = NVStereoSettingNames[i].id;
			return 0;
		}
	}

	return -1;
}

static bool is_known_float_setting(unsigned id)
{
	for (int i = 0; i < ARRAYSIZE(NVStereoSettingNames); i++) {
		if (NVStereoSettingNames[i].id == id) {
			return NVStereoSettingNames[i].known_float;
		}
	}

	return false;
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

typedef std::unordered_map<wstring, std::unordered_set<unsigned>> internal_setting_map_type;
internal_setting_map_type internal_setting_map;

// Any settings with the internal flag are specially encoded, so we have to
// decode them to be able to make sense of them... but the DRS API does not
// pass this flag to us and they differ for each profile so it seems our only
// option is to ask the driver to save the profiles to disk, and read that back
// in to identify these settings. This is the same approach used by nvidia
// profile inspector.
static int create_internal_setting_map(NvDRSSessionHandle session)
{
	wchar_t tmp[MAX_PATH], path[MAX_PATH];
	NvAPI_Status status = NVAPI_OK;
	wstring line, profile, sid;
	std::ifstream fp;
	unsigned id;

	if (!GetTempPath(MAX_PATH, tmp))
		goto err;

	if (!GetTempFileName(tmp, L"3DM", 0, path))
		goto err;

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

	while (next_broken_utf16_line(&fp, &line)) {
		if (!line.compare(0, 9, L"Profile \"")) {
			profile = line.substr(9, line.find_last_of(L'\"') - 9);
		}

		if (!line.compare(L"EndProfile")) {
			profile.clear();
		}

		if (!profile.empty() && line.length() > 22 && !line.compare(line.length() - 22, 22, L"InternalSettingFlag=V0")) {
			sid = line.substr(line.find(L" ID_0x") + 6, 8);
			swscanf_s(sid.c_str(), L"%08x", &id);
			// LogInfo("Identified Internal Setting ID %S 0x%08x\n", line.c_str(), id);
			internal_setting_map[profile].insert(id);
		}
	}

	fp.close();

	DeleteFile(path);
	return 0;

err_rm:
	DeleteFile(path);
err:
	log_nv_error(status);
	LogInfo("WARNING: Unable to determine which settings are internal - some settings will be garbage\n");
	return -1;
}

static void destroy_internal_setting_map()
{
	internal_setting_map.clear();
}

static void identify_internal_settings(NvDRSSessionHandle session,
		wchar_t *profile_name,
		std::unordered_set<unsigned> **internal_settings)
{
	internal_setting_map_type::iterator i;

	*internal_settings = NULL;

	i = internal_setting_map.find(wstring(profile_name));
	if (i == internal_setting_map.end())
		return;

	*internal_settings = &i->second;
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
	std::unordered_set<unsigned> *internal_settings = NULL;
	NvAPI_Status status = NVAPI_OK;
	NVDRS_APPLICATION *apps = NULL;
	NVDRS_SETTING *settings = NULL;
	unsigned len, dval = 0;
	wchar_t *name;
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
					&& internal_settings && internal_settings->count(settings[i].settingId)) {
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
					&& internal_settings && internal_settings->count(settings[i].settingId)) {
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
			if (name) {
				LogInfo(" // %S", name);
				// Floating point settings are only in our known table.
				// If we change it to allow floats not in our
				// table, make sure that we have added the //
				// comment character already.
				if (is_known_float_setting(settings[i].settingId))
					LogInfo(" = %.9g", *(float*)&dval);
			}
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

	if (create_internal_setting_map(session))
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
	destroy_internal_setting_map();
	NvAPI_DRS_DestroySession(session);
err_no_session:
	log_nv_error(status);
	LogInfo("----------- End driver profile settings -----------\n");
}

static int parse_ini_profile_lhs(wstring *lhs, NVDRS_SETTING *setting)
{
	int ret, len, off = 0;
	const wchar_t *val = lhs->c_str();

	// Check if the line contains a generic hex setting ID:
	if (!_wcsnicmp(val, L"Setting ID_0x", 13))
		off = 13;
	else if (!_wcsnicmp(val, L"SettingString ID_0x", 19))
		off = 19;
	else if (!_wcsnicmp(val, L"ID_0x", 5))
		off = 5;
	else if (!_wcsnicmp(val, L"0x", 2))
		off = 2;

	if (off) {
		ret = swscanf_s(&val[off], L"%x%n", &setting->settingId, &len);
		if (ret != 0 && ret != EOF && (off + len == lhs->length())) {
			return 0;
		}
	}

	// Finally, try asking the driver and looking up the setting name in
	// our table of Stereo settings:
	return lookup_setting_id(lhs->c_str(), &setting->settingId);
}

static int parse_ini_profile_rhs(wstring *rhs, NVDRS_SETTING *setting)
{
	wstring val(*rhs); // Copy so we still have the original for logging
	bool internal = false;
	wstring::size_type pos;
	unsigned dval;
	int ival, ret, len;
	float fval;

	// XXX: Not sure of best way to handle intermixed quotes and comment
	// characters... blah = "foo // "bar" baz" // "barf". Escape characters
	// are one solution, but I don't want to use them. A regular expression
	// match would make it fairly robust, though the above example is still
	// ambiguous and my experience is that Microsoft's implementation of
	// regexp is buggy, so for now I'll just do something simple and people
	// who use double shashes can break the parsing at their own peril.

	// Strip comments:
	pos = val.rfind(L"//");
	if (pos != val.npos)
		val.resize(pos);

	// Check if it is encrypted:
	pos = val.rfind(L" InternalSettingFlag=V0");
	if (pos != val.npos) {
		val.resize(pos);
		internal = true;
	}

	// Strip the UserSpecified flag (we always set it):
	pos = val.rfind(L" UserSpecified=true");
	if (pos != val.npos)
		val.resize(pos);

	// Strip remaining whitespace
	pos = val.find_last_not_of(L"\t ");
	if (pos + 1 != val.npos)
		val.resize(pos + 1);

	// Check if it looks like a string:
	if (val[0] == L'\"' && val[val.length() - 1] == L'\"') {
		if (val.length() - 2 < NVAPI_UNICODE_STRING_MAX) {
			if (internal) {
				// Ignore encrypted strings, as there is a high
				// probability that they have been corrupted
				// thanks to the bogus encoding they use.
				return -1;
			}
			setting->settingType = NVDRS_WSTRING_TYPE;
			wcscpy_s((wchar_t*)setting->wszCurrentValue, NVAPI_UNICODE_STRING_MAX, val.substr(1, val.length() - 2).c_str());
			return 0;
		}
	}

	// Try parsing value as a hex string:
	ret = swscanf_s(val.c_str(), L"0x%x%n", &dval, &len);
	if (ret != 0 && ret != EOF && len == val.length()) {
		if (internal)
			dval = decode_internal_dword(setting->settingId, dval);
		setting->settingType = NVDRS_DWORD_TYPE;
		setting->u32CurrentValue = dval;
		return 0;
	}

	// Try parsing value as an int:
	ret = swscanf_s(val.c_str(), L"%i%n", &ival, &len);
	if (ret != 0 && ret != EOF && len == val.length()) {
		setting->settingType = NVDRS_DWORD_TYPE;
		if (is_known_float_setting(setting->settingId)) {
			fval = (float)ival;
			setting->u32CurrentValue = *(unsigned*)&fval;
		} else {
			setting->u32CurrentValue = *(unsigned*)&ival;
		}
		return 0;
	}

	// Try parsing value as a float:
	ret = swscanf_s(val.c_str(), L"%f%n", &fval, &len);
	if (ret != 0 && ret != EOF && len == val.length()) {
		setting->settingType = NVDRS_DWORD_TYPE;
		setting->u32CurrentValue = *(unsigned*)&fval;
		return 0;
	}

	return -1;
}

// We want to allow the [Profile] section to be formatted in two ways (and we
// don't care if they are intermixed). We prefer entries like the following:
//
// StereoFlagsDX10 = 0x00004000
// StereoConvergence = 0.5
// Comments = "Boomshakalaka"
//
// But, since people already have profiles saved from Geforce Profile
// Manager, we also would like to accept profiles in that format - even
// decrypting any internal DWORD settings they specify (maybe not encrypted
// strings though - the broken encoding might do horrible things to the ini
// file).
int parse_ini_profile_line(wstring *lhs, wstring *rhs)
{
	NvAPI_Status status = NVAPI_OK;
	NVDRS_SETTING setting = {0};

	setting.version = NVDRS_SETTING_VER;

	// Check if it is a line from a Geforce Profile Manager paste we want
	// to ignore:
	if (rhs->empty() && (
			!_wcsnicmp(lhs->c_str(), L"Profile ", 8) ||
			!_wcsnicmp(lhs->c_str(), L"EndProfile", 10) ||
			!_wcsnicmp(lhs->c_str(), L"Executable ", 11) ||
			!_wcsnicmp(lhs->c_str(), L"ProfileType ", 12) ||
			!_wcsnicmp(lhs->c_str(), L"ShowOn ", 7))) {
		LogInfo("  Ignoring Line: %S\n", lhs->c_str());
		return 0;
	}

	if (parse_ini_profile_lhs(lhs, &setting)) {
		LogInfo("  WARNING: Unrecognised line (bad setting): %S = %S", lhs->c_str(), rhs->c_str());
		LogInfo("\n"); // In case of utf16 parse error
		BeepFailure2();
		return -1;
	}

	if (parse_ini_profile_rhs(rhs, &setting)) {
		LogInfo("  WARNING: Unrecognised line (bad value): %S = %S", lhs->c_str(), rhs->c_str());
		LogInfo("\n"); // In case of utf16 parse error
		BeepFailure2();
		return -1;
	}

	LogInfo("  %S (0x%08x) = %S", lhs->c_str(), setting.settingId, rhs->c_str());
	if (setting.settingType == NVDRS_DWORD_TYPE)
		LogInfo(" (0x%08x)", setting.u32CurrentValue);
	LogInfo("\n");

	// Since the same setting may have several names, duplicates could be
	// missed by the generic ini parsing code. Perform an extra duplicate
	// check here on the setting ID:
	if (profile_settings.count(setting.settingId)) {
		LogInfoW(L"WARNING: Duplicate driver profile setting ID found in d3dx.ini: 0x%08x\n", setting.settingId);
		BeepFailure2();
	}

	profile_settings[setting.settingId] = setting;

	return 0;
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

// Raise privileges to allow us to write any changes we need into the driver
// profile.
//
// In order to run some code with admin privileges we need an entire task
// running as admin. This presents a dilemma:
//
// We are a dll loaded by another process, so we cannot ourselves require that
// we are run with admin privileges as that is outside of our control.
//
// We could request the user run the game as admin, but we don't want to do
// that as we don't know what consequences that might have. e.g. the game could
// create a config or save file while running as admin and find itself unable
// to modify (or perhaps even open) that file later when running as a limited
// user.
//
// We could install a privileged service, background task or COM object that we
// communicate with... but we would need to be privileged at some point in
// order to install that. Since we are just dropped in a game directory we
// don't have any opportunity to install such a service.
//
// We could distribute a helper program alongside 3DMigoto that requires admin.
// This is not a bad option, and has the advantage that we should be able to
// set a sensible description that would appear in the UAC prompt, but on the
// downside it would increase the amount of cruft we pollute the game directory
// with, which we want to minimise.
//
// We could distribute the same helper program embedded as a resource inside
// the 3DMigoto dll, which we write to disk and execute only when we need it.
// This might end up being the best option, but at the cost of complexity.
//
// The below is an attempt to use rundll to achieve this - we call rundll with
// runas to request it be run as admin, then have it load us and call back into
// us. One disadvantage of this approach is the description in the UAC dialog
// has no indication of why we need admin, but we could always show our own
// dialog first to explain what we are about to do.
//     MAJOR PROBLEM WITH THIS APPROACH: The full path of the DLL cannot have
//     any spaces or commas in the filename. The documentation recommends using
//     the short path to guarantee this, but if short paths are disabled on a
//     drive this will not work, as seems to be the case when not using drive
//     C. As a workaround we could perhaps copy ourselves to another location,
//     such as the temporary directory - that is less likely to have a space in
//     the path and even if it does it is more likely to have a short filename.
static void spawn_privileged_profile_helper_task()
{
	wchar_t rundll_path[MAX_PATH], migoto_long_path[MAX_PATH], migoto_short_path[MAX_PATH], game_path[MAX_PATH];
	wstring params;
	SHELLEXECUTEINFO info = {0};
	HMODULE module;
	DWORD rc;

	if (!GetSystemDirectory(rundll_path, MAX_PATH))
		goto err;
	wcscat(rundll_path, L"\\rundll32.exe");

	// Get a handle to ourselves. Use an address to ensure we get us, not
	// some other d3d11.dll
	if (!GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
			| GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
			(LPCWSTR)spawn_privileged_profile_helper_task, &module)) {
		goto err;
	}

	if (!GetModuleFileName(module, migoto_long_path, MAX_PATH))
		goto err;

	// rundll requires the short path to ensure there are no spaces or
	// other punctuation:
	if (!GetShortPathName(migoto_long_path, migoto_short_path, MAX_PATH))
		goto err;

	// Now we no longer need the long path, turn it into the directory so
	// that the new process starts in the right directory to find d3dx.ini,
	// even if the game has changed to a different directory:
	wcsrchr(migoto_long_path, L'\\')[1] = 0;

	// Since the new process will be rundll, pass in the full path to the
	// game's executable so that we can find the right profile, etc:
	GetModuleFileName(0, game_path, MAX_PATH);

	// Since the parameters include two paths, it could conceivably be
	// longer than MAX_PATH, so use a wstring so we don't need to care:
	params = migoto_short_path;
	params += L",Install3DMigotoDriverProfile ";
	params += game_path;

	info.cbSize = sizeof(info);
	info.fMask = SEE_MASK_NOCLOSEPROCESS | SEE_MASK_NOASYNC;
	info.lpVerb = L"runas";
	info.lpFile = rundll_path;
	info.lpDirectory = migoto_long_path;
	info.lpParameters = params.c_str();
	info.nShow = SW_HIDE;

	// ShellExecuteEx may require COM, though will probably be ok without it:
	if (FAILED(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE)))
		LogInfo("spawn_privileged_profile_helper_task: CoInitializeEx failed\n");

	LogInfo("Spawning helper task to install driver profile...\n");
	LogInfo("  Program: %S\n", rundll_path);
	LogInfo("  Arguments: %S\n", params.c_str());
	LogInfo("  Directory: %S\n", info.lpDirectory);
	if (!ShellExecuteEx(&info))
		goto err;

	if (!info.hProcess)
		goto err;

	if (WaitForSingleObject(info.hProcess, INFINITE) == WAIT_FAILED)
		goto err_close;

	if (!GetExitCodeProcess(info.hProcess, &rc))
		goto err_close;

	LogInfo("Helper process terminated with code %u\n", rc);

	CloseHandle(info.hProcess);

	return;
err_close:
	CloseHandle(info.hProcess);
err:
	LogInfo("Error while requesting admin privileges to install driver profile\n");
}

// rundll entry point - when we come here we should be running in a new process
// with admin privileges, so that we can write any changes we need to the
// driver profile.
void CALLBACK Install3DMigotoDriverProfileW(HWND hwnd, HINSTANCE hinst, LPWSTR lpszCmdLine, int nCmdShow)
{
	MessageBox(hwnd, lpszCmdLine, L"3DMigoto profile installer", MB_OK);
}

void install_driver_profile()
{
	// TODO: If we are already privileged we should just write the profile
	// directly instead of calling code paths with lots of points of failure
	spawn_privileged_profile_helper_task();

	// TODO: Actually install the driver profile...
}
