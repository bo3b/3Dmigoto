#pragma once
#include <d3d9.h>
#include <mutex>
#include <ctime>
#include <vector>
#include <set>
#include <map>
#include <unordered_map>
#include <unordered_set>

#include "profiling.h"
#include "CommandList.h"
#include "ResourceHash.h"
#include "DLLMainHookDX9.h"
#include <DirectXMath.h>
#include "DecompileHLSL.h"



namespace D3D9Wrapper {
	class IDirect3DShader9;
	class IDirect3DVertexShader9;
	class IDirect3DPixelShader9;
	class IDirect3DQuery9;
}
// Resolve circular include dependency between Globals.h ->
// CommandList.h -> HackerContext.h -> Globals.h
#define	D3D9_VERTEX_INPUT_TEXTURE_SLOT_COUNT	( 4 )
#define	D3D9_VERTEX_INPUT_START_REG	(D3DVERTEXTEXTURESAMPLER0)
#define	D3D9_PIXEL_INPUT_TEXTURE_SLOT_COUNT	( 16 )
#define	D3D9_SIMULTANEOUS_RENDER_TARGET_COUNT	( 16 )
#define D3D9_VERTEX_INPUT_RESOURCE_SLOT_COUNT ( 16 )
#define D3D9_MAX_PIXEL_FLOAT_CONSTANT_SLOT_COUNT ( 224 )
#define D3D9_MAX_VERTEX_FLOAT_CONSTANT_SLOT_COUNT ( 256 )
#define D3D9_MAX_INT_CONSTANT_SLOT_COUNT ( 16 )
#define D3D9_MAX_BOOL_CONSTANT_SLOT_COUNT ( 16 )

__declspec (align(8)) extern volatile LONG shared_game_internal_width;
__declspec (align(8)) extern volatile LONG shared_game_internal_height;
__declspec (align(8)) extern volatile LONG shared_cursor_update_required;
extern HWND shared_hWnd;
class CommandListCommand;
class CommandList;
enum HuntingMode {
	HUNTING_MODE_DISABLED = 0,
	HUNTING_MODE_ENABLED = 1,
	HUNTING_MODE_SOFT_DISABLED = 2,
};
enum class MarkingMode {
	SKIP,
	ORIGINAL,
	PINK,
	MONO,

	INVALID, // Must be last - used for next_marking_mode
};
static EnumName_t<const wchar_t *, MarkingMode> MarkingModeNames[] = {
	{ L"skip", MarkingMode::SKIP },
	{ L"mono", MarkingMode::MONO },
	{ L"original", MarkingMode::ORIGINAL },
	{ L"pink", MarkingMode::PINK },
	{ NULL, MarkingMode::INVALID } // End of list marker
};

enum class MarkingAction {
	INVALID = 0,
	CLIPBOARD = 0x0000001,
	HLSL = 0x0000002,
	ASM = 0x0000004,
	CONSTANT_TABLE = 0x0000008,
	DUMP_MASK = 0x000000E, // HLSL and/or Assembly and/or Constant_Table is selected
	MONO_SS = 0x0000010,
	STEREO_SS = 0x0000020,
	SS_IF_PINK = 0x0000040,

	DEFAULT = 0x0000003,
};
SENSIBLE_ENUM(MarkingAction);
static EnumName_t<const wchar_t *, MarkingAction> MarkingActionNames[] = {
	{ L"hlsl", MarkingAction::HLSL },
	{ L"asm", MarkingAction::ASM },
	{ L"constant_table", MarkingAction::CONSTANT_TABLE },
	{ L"assembly", MarkingAction::ASM },
	{ L"clipboard", MarkingAction::CLIPBOARD },
	{ L"mono_snapshot", MarkingAction::MONO_SS },
	{ L"stereo_snapshot", MarkingAction::STEREO_SS },
	{ L"snapshot_if_pink", MarkingAction::SS_IF_PINK },
	{ NULL, MarkingAction::INVALID } // End of list marker
};

enum class ShaderHashType {
	INVALID = -1,
	FNV,
	EMBEDDED,
	BYTECODE,
};
static EnumName_t<const wchar_t *, ShaderHashType> ShaderHashNames[] = {
	{ L"3dmigoto", ShaderHashType::FNV },
	{ L"embedded", ShaderHashType::EMBEDDED },
	{ L"bytecode", ShaderHashType::BYTECODE },
	{ NULL, ShaderHashType::INVALID } // End of list marker
};

// Key is index/vertex buffer, value is hash key.
typedef std::unordered_map<IUnknown *, uint32_t> DataBufferMap;

// Source compiled shaders.
typedef std::unordered_map<UINT64, std::string> CompiledShaderMap;

// Strategy: This OriginalShaderInfo record and associated map is to allow us to keep track of every
//	pixelshader and vertexshader that are compiled from hlsl text from the ShaderFixes
//	folder.  This keeps track of the original shader information using the ID3D11VertexShader*
//	or ID3D11PixelShader* as a master key to the key map.
//	We are using the base class of ID3D11DeviceChild* since both descend from that, and that allows
//	us to use the same structure for Pixel and Vertex shaders both.

// Info saved about originally overridden shaders passed in by the game in CreateVertexShader or
// CreatePixelShader that have been loaded as HLSL
//	shaderType is "vs" or "ps" or maybe later "gs" (type wstring for file name use)
//	shaderModel is only filled in when a shader is replaced.  (type string for old D3 API use)
//	linkage is passed as a parameter, seems to be rarely if ever used.
//	byteCode is the original shader byte code passed in by game, or recompiled by override.
//	timeStamp allows reloading/recompiling only modified shaders
//	replacement is either ID3D11VertexShader or ID3D11PixelShader
//  found is used to revert shaders that are deleted from ShaderFixes
//  infoText is shown in the OSD when the shader is actively selected.
struct OriginalShaderInfo
{
	UINT64 hash;
	std::wstring shaderType;
	std::string shaderModel;
	ID3DBlob* byteCode;
	FILETIME timeStamp;
	IUnknown* replacement;
	bool found;
	bool deferred_replacement_candidate;
	bool deferred_replacement_processed;
	std::wstring infoText;
};

// Key is the overridden shader that was given back to the game at CreateVertexShader (vs or ps)
typedef unordered_set<D3D9Wrapper::IDirect3DShader9*> ShaderSet;
enum class FrameAnalysisOptions {
	INVALID = 0,

	// Bind selection:
	DUMP_RT = 0x00000001,
	DUMP_DEPTH = 0x00000002,
	DUMP_SRV = 0x00000004,
	DUMP_CB = 0x00000008,
	DUMP_VB = 0x00000010,
	DUMP_IB = 0x00000020,

	// Format selection:
	FMT_2D_AUTO = 0x00000040,
	FMT_2D_JPS = 0x00000080,
	FMT_2D_DDS = 0x00000100,
	FMT_BUF_BIN = 0x00000200,
	FMT_BUF_TXT = 0x00000400,
	FMT_DESC = 0x00000800,

	// Masks:
	DUMP_XB_MASK = 0x00000038, // CB+VB+IB, to check if a user specified any of these
	FMT_2D_MASK = 0x000009c0, // Mask of Texture2D formats
	FMT_BUF_MASK = 0x00000e00, // Mask of Buffer formats

							   // Legacy bind + format combo options:
							   DUMP_RT_JPS = 0x00000081,
							   DUMP_RT_DDS = 0x00000301,
							   DUMP_DEPTH_JPS = 0x00000082,
							   DUMP_DEPTH_DDS = 0x00000302,
							   DUMP_TEX_JPS = 0x00000084,
							   DUMP_TEX_DDS = 0x00000304,
							   DUMP_CB_TXT = 0x00000408,
							   DUMP_VB_TXT = 0x00000410,
							   DUMP_IB_TXT = 0x00000420,

							   // Misc options:
							   CLEAR_RT = 0x00001000,
							   FILENAME_REG = 0x00002000,
							   FILENAME_HANDLE = 0x00004000,
							   PERSIST = 0x00008000, // Used by shader/texture triggers
							   STEREO = 0x00010000,
							   MONO = 0x00020000,
							   STEREO_MASK = 0x00030000,
							   HOLD = 0x00040000,
							   DUMP_ON_UNMAP = 0x00080000,
							   DUMP_ON_UPDATE = 0x00100000,
							   SHARE_DEDUPED = 0x00200000,
							   SYMLINK = 0x01000000,
							   DEPRECATED = (signed)0x80000000,
};
SENSIBLE_ENUM(FrameAnalysisOptions);
static EnumName_t<wchar_t *, FrameAnalysisOptions> FrameAnalysisOptionNames[] = {
	// Bind flag selection:
	{ L"dump_rt", FrameAnalysisOptions::DUMP_RT },
	{ L"dump_depth", FrameAnalysisOptions::DUMP_DEPTH },
	{ L"dump_tex", FrameAnalysisOptions::DUMP_SRV },
	{ L"dump_cb", FrameAnalysisOptions::DUMP_CB },
	{ L"dump_vb", FrameAnalysisOptions::DUMP_VB },
	{ L"dump_ib", FrameAnalysisOptions::DUMP_IB },

	// Texture2D format selection:
	{ L"jps", FrameAnalysisOptions::FMT_2D_JPS },
	{ L"jpg", FrameAnalysisOptions::FMT_2D_JPS },
	{ L"jpeg", FrameAnalysisOptions::FMT_2D_JPS },
	{ L"dds", FrameAnalysisOptions::FMT_2D_DDS },
	{ L"jps_dds", FrameAnalysisOptions::FMT_2D_AUTO },
	{ L"jpg_dds", FrameAnalysisOptions::FMT_2D_AUTO },
	{ L"jpeg_dds", FrameAnalysisOptions::FMT_2D_AUTO },

	// Buffer format selection:
	{ L"buf", FrameAnalysisOptions::FMT_BUF_BIN },
	{ L"txt", FrameAnalysisOptions::FMT_BUF_TXT },

	{ L"desc", FrameAnalysisOptions::FMT_DESC },

	// Misc options:
	{ L"clear_rt", FrameAnalysisOptions::CLEAR_RT },
	{ L"persist", FrameAnalysisOptions::PERSIST },
	{ L"stereo", FrameAnalysisOptions::STEREO },
	{ L"mono", FrameAnalysisOptions::MONO },
	{ L"filename_reg", FrameAnalysisOptions::FILENAME_REG },
	{ L"filename_handle", FrameAnalysisOptions::FILENAME_HANDLE },
	{ L"log", FrameAnalysisOptions::DEPRECATED }, // Left in the list for backwards compatibility, but this is now always enabled
	{ L"hold", FrameAnalysisOptions::HOLD },
	{ L"dump_on_unmap", FrameAnalysisOptions::DUMP_ON_UNMAP },
	{ L"dump_on_update", FrameAnalysisOptions::DUMP_ON_UPDATE },
	{ L"share_dupes", FrameAnalysisOptions::SHARE_DEDUPED },
	{ L"symlink", FrameAnalysisOptions::SYMLINK },

	// Legacy combo options:
	{ L"dump_rt_jps", FrameAnalysisOptions::DUMP_RT_JPS },
	{ L"dump_rt_dds", FrameAnalysisOptions::DUMP_RT_DDS },
	{ L"dump_depth_jps", FrameAnalysisOptions::DUMP_DEPTH_JPS }, // Doesn't work yet
	{ L"dump_depth_dds", FrameAnalysisOptions::DUMP_DEPTH_DDS },
	{ L"dump_tex_jps", FrameAnalysisOptions::DUMP_TEX_JPS },
	{ L"dump_tex_dds", FrameAnalysisOptions::DUMP_TEX_DDS },
	{ L"dump_cb_txt", FrameAnalysisOptions::DUMP_CB_TXT },
	{ L"dump_vb_txt", FrameAnalysisOptions::DUMP_VB_TXT },
	{ L"dump_ib_txt", FrameAnalysisOptions::DUMP_IB_TXT },

	{ NULL, FrameAnalysisOptions::INVALID } // End of list marker
};

enum class DepthBufferFilter {
	INVALID = -1,
	NONE,
	DEPTH_ACTIVE,
	DEPTH_INACTIVE,
};
static EnumName_t<const wchar_t *, DepthBufferFilter> DepthBufferFilterNames[] = {
	{ L"none", DepthBufferFilter::NONE },
	{ L"depth_active", DepthBufferFilter::DEPTH_ACTIVE },
	{ L"depth_inactive", DepthBufferFilter::DEPTH_INACTIVE },
	{ NULL, DepthBufferFilter::INVALID } // End of list marker
};

struct ShaderOverride {
	std::wstring first_ini_section;
	DepthBufferFilter depth_filter;
	UINT64 partner_hash;
	FrameAnalysisOptions analyse_options;
	char model[20]; // More than long enough for even ps_4_0_level_9_0
	int allow_duplicate_hashes;

	bool per_frame;
	unsigned frame_no;

	CommandList command_list;
	CommandList post_command_list;

	ShaderOverride() :
		depth_filter(DepthBufferFilter::NONE),
		partner_hash(0),
		analyse_options(FrameAnalysisOptions::INVALID),
		allow_duplicate_hashes(1),
		per_frame(false),
		frame_no(0)
	{
		model[0] = '\0';
	}
};
typedef std::unordered_map<UINT64, struct ShaderOverride> ShaderOverrideMap;

struct TextureOverride {
	std::wstring ini_section;
	int stereoMode;
	int format;
	int width;
	int height;
	float width_multiply;
	float height_multiply;
	std::vector<int> iterations;
	bool expand_region_copy;
	bool deny_cpu_read;
	float filter_index;

	bool has_draw_context_match;
	bool has_match_priority;
	int priority;
	FuzzyMatch match_first_vertex;
	FuzzyMatch match_first_index;
	FuzzyMatch match_vertex_count;
	FuzzyMatch match_index_count;

	CommandList command_list;
	CommandList post_command_list;

	TextureOverride() :
		stereoMode(-1),
		format(-1),
		width(-1),
		height(-1),
		width_multiply(1.0),
		height_multiply(1.0),
		expand_region_copy(false),
		deny_cpu_read(false),
		filter_index(FLT_MAX),
		has_draw_context_match(false),
		has_match_priority(false),
		priority(0)
	{}
};
//typedef std::unordered_map<::IDirect3DResource9*, ResourceHandleInfo> ResourceMap;
// The TextureOverrideList will be sorted because we want multiple
// [TextureOverrides] that share the same hash (differentiated by draw context
// matching) to always be processed in the same order for consistent results.
// We can't use a std::set to enforce this ordering, as that makes the
// TextureOverrides const, but there are a few places we modify it. Instead, we
// will sort it in the ini parser when we create the list.
typedef std::vector<struct TextureOverride> TextureOverrideList;
typedef std::unordered_map<uint32_t, TextureOverrideList> TextureOverrideMap;
//typedef std::unordered_map<uint32_t, struct TextureOverride> TextureOverrideMap;

// We use this when collecting resource info for ShaderUsage.txt to take a
// snapshot of the resource handle, hash and original hash. We used to just
// save the resource handle, but that was problematic since handles can get
// reused, and so we could record the wrong hash in the ShaderUsage.txt
struct ResourceSnapshot
{
	D3D9Wrapper::IDirect3DResource9 *handle;
	uint32_t hash;
	uint32_t orig_hash;

	ResourceSnapshot(D3D9Wrapper::IDirect3DResource9 *handle, uint32_t hash, uint32_t orig_hash) :
		handle(handle), hash(hash), orig_hash(orig_hash)
	{}
};
static inline bool operator<(const ResourceSnapshot &lhs, const ResourceSnapshot &rhs)
{
	if (lhs.orig_hash != rhs.orig_hash)
		return (lhs.orig_hash < rhs.orig_hash);
	if (lhs.hash != rhs.hash)
		return (lhs.hash < rhs.hash);
	return (lhs.handle < rhs.handle);
}

struct ShaderInfoData
{
	// All are std::map or std::set so that ShaderUsage.txt is sorted - lookup time is O(log N)
	map<int, std::set<ResourceSnapshot>> ResourceRegisters;
	set<UINT64> PeerShaders;
	vector<set<ResourceSnapshot>> RenderTargets;
	set<ResourceSnapshot> DepthTargets;
};

enum class GetResolutionFrom {
	INVALID = -1,
	SWAP_CHAIN,
	DEPTH_STENCIL,
};
static EnumName_t<const wchar_t *, GetResolutionFrom> GetResolutionFromNames[] = {
	{ L"swap_chain", GetResolutionFrom::SWAP_CHAIN },
	{ L"depth_stencil", GetResolutionFrom::DEPTH_STENCIL },
	{ NULL, GetResolutionFrom::INVALID } // End of list marker
};

struct ResolutionInfo
{
	int width, height;
	GetResolutionFrom from;

	ResolutionInfo() :
		from(GetResolutionFrom::INVALID),
		width(-1),
		height(-1)
	{}
};

struct Globals
{
private:
	HWND local_hWnd; // To translate mouse coordinates to the window
public:
	HWND hWnd(){
		HWND rethWnd;
		if (!multi_process_share_globals)
			rethWnd = local_hWnd;
		else
			rethWnd = shared_hWnd;

		return rethWnd;
	}
	void sethWnd(HWND hWnd) {
		if (!multi_process_share_globals)
			local_hWnd = hWnd;
		else
			shared_hWnd = hWnd;
	}
	LONG SET_CURSOR_UPDATE_REQUIRED(LONG cursor_update_required) {
		LONG initialValue;
		if (!multi_process_share_globals) {
			initialValue = LOCAL_CURSOR_UPDATE_REQUIRED;
			LOCAL_CURSOR_UPDATE_REQUIRED = cursor_update_required;
		}
		else {
			initialValue = InterlockedExchange(&shared_cursor_update_required, cursor_update_required);
		}
		LogDebug("  SET_CURSOR_UPDATE_REQUIRED called with %d, initial value = %d \n", cursor_update_required, initialValue);
		return initialValue;
	}

	LONG CURSOR_UPDATE_REQUIRED() {
		LONG required;
		if (!multi_process_share_globals) {
			required = LOCAL_CURSOR_UPDATE_REQUIRED;
		}
		else {
			required = InterlockedCompareExchange(&shared_cursor_update_required, 0, 0);
		}
		LogDebug("  CURSOR_UPDATE_REQUIRED called = %d \n", required);
		return required;
	}
	LONG SET_GAME_INTERNAL_WIDTH(LONG game_internal_width) {
		LONG initialValue;
		if (!multi_process_share_globals) {
			initialValue = LOCAL_GAME_INTERNAL_WIDTH;
			LOCAL_GAME_INTERNAL_WIDTH = game_internal_width;
		}
		else {
			initialValue = InterlockedExchange(&shared_game_internal_width, game_internal_width);
		}
		LogDebug("  SET_GAME_INTERNAL_WIDTH called with %d, initial value = %d \n", game_internal_width, initialValue);
		return initialValue;
	}

	LONG GAME_INTERNAL_WIDTH() {
		LONG width;
		if (!multi_process_share_globals) {
			width = LOCAL_GAME_INTERNAL_WIDTH;
		}
		else {
			width = InterlockedCompareExchange(&shared_game_internal_width, 0, 0);
		}
		LogDebug("  GAME_INTERNAL_WIDTH called = %d \n", width);
		return width;
	}
	LONG SET_GAME_INTERNAL_HEIGHT(LONG game_internal_height) {
		LONG initialValue;
		if (!multi_process_share_globals) {
			initialValue = LOCAL_GAME_INTERNAL_HEIGHT;
			LOCAL_GAME_INTERNAL_HEIGHT = game_internal_height;
		}
		else {
			initialValue = InterlockedExchange(&shared_game_internal_height, game_internal_height);
		}

		LogDebug("  SET_GAME_INTERNAL_HEIGHT called with %d, initial value = %d \n", game_internal_height, initialValue);
		return initialValue;
	}

	LONG GAME_INTERNAL_HEIGHT() {
		LONG height;
		if (!multi_process_share_globals) {
			height = LOCAL_GAME_INTERNAL_HEIGHT;
		}
		else {
			height = InterlockedCompareExchange(&shared_game_internal_height, 0, 0);
		}
		LogDebug("  GAME_INTERNAL_HEIGHT called = %d \n", height);
		return height;
	}

	bool gDelayDeviceCreation;
	LONG process_index;

	bool gInitialized;
	bool gReloadConfigPending;
	bool gWipeUserConfig;
	bool gLogInput;
	bool dump_all_profiles;
	DWORD ticks_at_launch;

	bool helix_fix;
	wchar_t helix_ini[MAX_PATH];
	wchar_t HELIX_SHADER_PATH_VERTEX[MAX_PATH];
	wchar_t HELIX_SHADER_PATH_PIXEL[MAX_PATH];
	bool helix_skip_set_scissor_rect;
	int helix_StereoParamsVertexReg;
	int helix_StereoParamsPixelReg;

	wchar_t SHADER_PATH[MAX_PATH];
	wchar_t SHADER_CACHE_PATH[MAX_PATH];
	wchar_t CHAIN_DLL_PATH[MAX_PATH];
	int load_library_redirect;

	std::wstring user_config;
	bool user_config_dirty;

	EnableHooksDX9 enable_hooks;

	bool enable_check_interface;
	int enable_dxgi1_2;
	int enable_create_device;
	bool enable_platform_update;

	int SCREEN_WIDTH_DELAY;
	int	SCREEN_HEIGHT_DELAY;
	int	SCREEN_REFRESH_DELAY;

	int LOCAL_CURSOR_UPDATE_REQUIRED;

	int LOCAL_GAME_INTERNAL_WIDTH; // this variable stores the resolution width provided by the game (required for the upscaling feature)
	int LOCAL_GAME_INTERNAL_HEIGHT; // this variable stores the resolution height provided by the game (required for the upscaling feature)
	int SCREEN_WIDTH;
	int SCREEN_HEIGHT;
	int SCREEN_REFRESH;
	int SCREEN_FULLSCREEN;
	int SCREEN_UPSCALING;
	int FILTER_REFRESH[11];
	bool SCREEN_ALLOW_COMMANDS;
	bool upscaling_hooks_armed;
	bool upscaling_command_list_using_explicit_bb_flip;
	bool bb_is_upscaling_bb;
	bool implicit_post_checktextureoverride_used;

	//this will surely have just one use case, SWTOR, in which two processes are spawned for some reason
	//(32 bit memory limitation?). This means two of us, and two globals, are loaded. This wasn't an issue
	//until upscaling was attempted, but the window handle and game res needed to be shared between the
	//two processes, otherwise the game would just infinitly reset itself on launch.
	bool multi_process_share_globals;
	bool stereoblit_control_set_once;
	float update_stereo_params_freq;

	int stereo_format;
	bool intercept_window_proc;
	bool adjust_message_pt;

	bool adjust_display_settings;
	bool adjust_monitor_info;
	bool adjust_system_metrics;

	bool adjust_map_window_points;
	bool adjust_get_window_rect;
	bool adjust_clip_cursor;
	bool adjust_window_from_point;

	bool adjust_cursor_pos;

	MarkingMode marking_mode;
	MarkingAction marking_actions;

	int gForceStereo;
	bool gCreateStereoProfile;
	int gSurfaceCreateMode;
	int gSurfaceSquareCreateMode;
	bool gForceNoNvAPI;
	bool gTrackNvAPIStereoActive;
	bool gTrackNvAPIConvergence;
	bool gTrackNvAPISeparation;
	bool gTrackNvAPIEyeSeparation;
	bool gTrackNvAPIStereoActiveDisableReset;
	bool gTrackNvAPIConvergenceDisableReset;
	bool gTrackNvAPISeparationDisableReset;
	bool gTrackNvAPIEyeSeparationDisableReset;

	bool gAutoDetectDepthBuffer;

	bool gForwardToEx;

	bool gDirectModeStereoLargeSurfacesOnly;
	bool gDirectModeStereoSmallerThanBackBuffer;

	int gDirectModeStereoMinSurfaceArea;

	UINT hunting;
	bool fix_enabled;
	bool config_reloadable;
	bool show_original_enabled;
	time_t huntTime;
	bool verbose_overlay;
	bool suppress_overlay;

	bool deferred_contexts_enabled;

	bool frame_analysis_registered;
	bool analyse_frame;
	unsigned analyse_frame_no;
	wchar_t ANALYSIS_PATH[MAX_PATH];
	FrameAnalysisOptions def_analyse_options, cur_analyse_options;
	std::unordered_set<void*> frame_analysis_seen_rts;

	ShaderHashType shader_hash_type;
	int texture_hash_version;
	int EXPORT_HLSL;		// 0=off, 1=HLSL only, 2=HLSL+OriginalASM, 3= HLSL+OriginalASM+recompiledASM
	bool EXPORT_SHADERS, EXPORT_FIXED, EXPORT_BINARY, CACHE_SHADERS, SCISSOR_DISABLE;
	int track_texture_updates;
	uint32_t ZBufferHashToInject;
	DecompilerSettings decompiler_settings;
	bool DumpUsage;
	bool ENABLE_TUNE;
	float gTuneValue[4], gTuneStep;

	int StereoParamsVertexReg;
	int StereoParamsPixelReg;

	std::map<int, DirectX::XMFLOAT4> IniConstants;

	ResolutionInfo mResolutionInfo;
	CommandList present_command_list;
	CommandList post_present_command_list;
	CommandList clear_rtv_command_list;
	CommandList post_clear_rtv_command_list;
	CommandList clear_dsv_command_list;
	CommandList post_clear_dsv_command_list;
	CommandList constants_command_list;
	CommandList post_constants_command_list;
	unsigned frame_no;
	bool hide_cursor;
	bool cursor_upscaling_bypass;

	bool check_foreground_window;

	CRITICAL_SECTION mCriticalSection;

	std::set<uint32_t> mVisitedIndexBuffers;				// std::set is sorted for consistent order while hunting
	uint32_t mSelectedIndexBuffer;
	int mSelectedIndexBufferPos;
	std::set<UINT64> mSelectedIndexBuffer_VertexShader;		// std::set so that shaders used with an index buffer will be sorted in log when marked
	std::set<UINT64> mSelectedIndexBuffer_PixelShader;		// std::set so that shaders used with an index buffer will be sorted in log when marked

	std::set<uint32_t> mVisitedVertexBuffers;				// std::set is sorted for consistent order while hunting
	uint32_t mSelectedVertexBuffer;
	int mSelectedVertexBufferPos;
	std::set<UINT64> mSelectedVertexBuffer_VertexShader;		// std::set so that shaders used with an index buffer will be sorted in log when marked
	std::set<UINT64> mSelectedVertexBuffer_PixelShader;		// std::set so that shaders used with an index buffer will be sorted in log when marked

	CompiledShaderMap mCompiledShaderMap;

	set<UINT64> mVisitedVertexShaders;					// Only shaders seen since last hunting timeout; std::set for consistent order while hunting
	UINT64 mSelectedVertexShader;				 			// Hash.  -1 now for unselected state. The shader selected using Input object.
	int mSelectedVertexShaderPos;							// -1 for unselected state.

	set<uint32_t> mSelectedVertexShader_IndexBuffer;	// std::set so that index buffers used with a shader will be sorted in log when marked
	set<uint32_t> mSelectedVertexShader_VertexBuffer;

	set<UINT64> mVisitedPixelShaders;					// std::set is sorted for consistent order while hunting
	UINT64 mSelectedPixelShader;							// Hash.  -1 now for unselected state.
	int mSelectedPixelShaderPos;							// -1 for unselected state.
	set<uint32_t> mSelectedPixelShader_IndexBuffer;	// std::set so that index buffers used with a shader will be sorted in log when marked
	set<uint32_t> mSelectedPixelShader_VertexBuffer;
	::IDirect3DPixelShader9* mPinkingShader;						// Special pixels shader to mark a selection with hot pink.

	ShaderSet mReloadedShaders;						// Shaders that were reloaded live from ShaderFixes

	ShaderOverrideMap mShaderOverrideMap;
	TextureOverrideMap mTextureOverrideMap;
	FuzzyTextureOverrides mFuzzyTextureOverrides;

	std::vector<D3D9Wrapper::IDirect3DQuery9*> mQueries;

	// These five items work with the *original* resource hash:
	std::unordered_map<uint32_t, struct ResourceHashInfo> mResourceInfo;
	std::set<uint32_t> mRenderTargetInfo;					// std::set so that ShaderUsage.txt is sorted - lookup time is O(log N)
	std::set<uint32_t> mDepthTargetInfo;					// std::set so that ShaderUsage.txt is sorted - lookup time is O(log N)
	std::set<uint32_t> mShaderResourceInfo;					// std::set so that ShaderUsage.txt is sorted - lookup time is O(log N)
	std::set<uint32_t> mCopiedResourceInfo;					// std::set so that ShaderUsage.txt is sorted - lookup time is O(log N)

	std::set<D3D9Wrapper::IDirect3DSurface9 *> mVisitedRenderTargets;						// std::set is sorted for consistent order while hunting
	D3D9Wrapper::IDirect3DSurface9 *mSelectedRenderTarget;
	int mSelectedRenderTargetPos;
	// Snapshot of all targets for selection.
	D3D9Wrapper::IDirect3DSurface9 *mSelectedRenderTargetSnapshot;
	std::set<D3D9Wrapper::IDirect3DSurface9 *> mSelectedRenderTargetSnapshotList;			// std::set so that render targets will be sorted in log when marked

	map<UINT64, ShaderInfoData> mVertexShaderInfo;			// std::map so that ShaderUsage.txt is sorted - lookup time is O(log N)
	map<UINT64, ShaderInfoData> mPixelShaderInfo;			// std::map so that ShaderUsage.txt is sorted - lookup time is O(log N)

	Globals() :
		gDelayDeviceCreation(false),
		process_index(0),

		mSelectedRenderTargetSnapshot(0),
		mSelectedRenderTargetPos(-1),
		mSelectedRenderTarget((D3D9Wrapper::IDirect3DSurface9 *)-1),
		mSelectedPixelShader(NULL),
		mSelectedPixelShaderPos(-1),
		mSelectedVertexShader(NULL),
		mSelectedVertexShaderPos(-1),
		mSelectedIndexBuffer(-1),
		mSelectedIndexBufferPos(-1),
		mSelectedVertexBuffer(-1),
		mSelectedVertexBufferPos(-1),
		mPinkingShader(0),

		hunting(HUNTING_MODE_DISABLED),
		fix_enabled(true),
		config_reloadable(false),
		show_original_enabled(false),
		huntTime(0),
		verbose_overlay(false),
		suppress_overlay(false),

		deferred_contexts_enabled(true),
		frame_analysis_registered(false),
		analyse_frame(false),
		analyse_frame_no(0),
		def_analyse_options(FrameAnalysisOptions::INVALID),
		cur_analyse_options(FrameAnalysisOptions::INVALID),

		shader_hash_type(ShaderHashType::FNV),
		texture_hash_version(0),
		EXPORT_SHADERS(false),
		EXPORT_HLSL(0),
		EXPORT_FIXED(false),
		EXPORT_BINARY(false),
		CACHE_SHADERS(false),
		DumpUsage(false),
		ENABLE_TUNE(false),
		gTuneStep(0.001f),

		StereoParamsVertexReg(-1),
		StereoParamsPixelReg(-1),

		frame_no(0),
		local_hWnd(NULL),
		hide_cursor(false),
		cursor_upscaling_bypass(true),
		check_foreground_window(false),

		SCREEN_WIDTH_DELAY(-1),
		SCREEN_HEIGHT_DELAY(-1),
		SCREEN_REFRESH_DELAY(-1),

		SCREEN_WIDTH(-1),
		SCREEN_HEIGHT(-1),
		SCREEN_REFRESH(-1),
		SCREEN_FULLSCREEN(0),
		SCREEN_ALLOW_COMMANDS(false),
		upscaling_hooks_armed(true),
		upscaling_command_list_using_explicit_bb_flip(false),
		bb_is_upscaling_bb(false),

		multi_process_share_globals(false),
		stereoblit_control_set_once(false),
		update_stereo_params_freq(1.0),
		gForwardToEx(false),
		gAutoDetectDepthBuffer(false),

		stereo_format(0),

		adjust_cursor_pos(true),
		intercept_window_proc(true),

		adjust_message_pt(false),
		adjust_display_settings(false),
		adjust_monitor_info(false),
		adjust_system_metrics(false),
		adjust_map_window_points(false),
		adjust_get_window_rect(true),
		adjust_clip_cursor(true),
		adjust_window_from_point(false),

		implicit_post_checktextureoverride_used(false),

		marking_mode(MarkingMode::INVALID),
		marking_actions(MarkingAction::INVALID),

		gForceStereo(0),
		gCreateStereoProfile(false),
		gSurfaceCreateMode(-1),
		gSurfaceSquareCreateMode(-1),
		gForceNoNvAPI(false),
		gTrackNvAPIStereoActive(false),
		gTrackNvAPIConvergence(false),
		gTrackNvAPISeparation(false),
		gTrackNvAPIEyeSeparation(false),
		gTrackNvAPIStereoActiveDisableReset(false),
		gTrackNvAPIConvergenceDisableReset(false),
		gTrackNvAPISeparationDisableReset(false),
		gTrackNvAPIEyeSeparationDisableReset(false),
		ZBufferHashToInject(0),
		SCISSOR_DISABLE(0),

		gDirectModeStereoLargeSurfacesOnly(false),
		gDirectModeStereoSmallerThanBackBuffer(false),
		gDirectModeStereoMinSurfaceArea(-1),

		load_library_redirect(0),
		enable_hooks(EnableHooksDX9::INVALID),
		enable_check_interface(false),
		enable_dxgi1_2(0),
		enable_create_device(0),
		enable_platform_update(false),

		gInitialized(false),
		gReloadConfigPending(false),
		gWipeUserConfig(false),
		user_config_dirty(false),
		gLogInput(false),
		dump_all_profiles(false),
		LOCAL_CURSOR_UPDATE_REQUIRED(1),
		helix_fix(false),
		helix_skip_set_scissor_rect(false),
		helix_StereoParamsVertexReg(-1),
		helix_StereoParamsPixelReg(-1)

	{
		int i;

		helix_ini[0] = 0;
		HELIX_SHADER_PATH_VERTEX[0] = 0;
		HELIX_SHADER_PATH_PIXEL[0] = 0;

		SHADER_PATH[0] = 0;
		SHADER_CACHE_PATH[0] = 0;
		CHAIN_DLL_PATH[0] = 0;

		ANALYSIS_PATH[0] = 0;

		for (i = 0; i < 4; i++)
			gTuneValue[i] = 1.0f;

		for (i = 0; i < 11; i++)
			FILTER_REFRESH[i] = 0;
		ticks_at_launch = GetTickCount();
	}
};

extern Globals *G;

static inline ShaderOverrideMap::iterator lookup_shaderoverride(UINT64 hash)
{
	return Profiling::lookup_map(G->mShaderOverrideMap, hash, &Profiling::shaderoverride_lookup_overhead);
}
static inline TextureOverrideMap::iterator lookup_textureoverride(uint32_t hash)
{
	return Profiling::lookup_map(G->mTextureOverrideMap, hash, &Profiling::textureoverride_lookup_overhead);
}
