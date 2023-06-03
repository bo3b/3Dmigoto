#pragma once

#include <d3d11_1.h>
#include <ctime>
#include <vector>
#include <set>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <memory>

#include "DLLMainHook.h"
#include "DirectXMath.h"
#include "util.h"
#include "DecompileHLSL.h"

#include "ResourceHash.h"
#include "CommandList.h"
#include "profiling.h"
#include "lock.h"

extern HINSTANCE migoto_handle;

// Resolve circular include dependency between Globals.h ->
// CommandList.h -> HackerContext.h -> Globals.h
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
	{L"skip", MarkingMode::SKIP},
	{L"mono", MarkingMode::MONO},
	{L"original", MarkingMode::ORIGINAL},
	{L"pink", MarkingMode::PINK},
	{NULL, MarkingMode::INVALID} // End of list marker
};

enum class MarkingAction {
	INVALID    = 0,
	CLIPBOARD  = 0x0000001,
	HLSL       = 0x0000002,
	ASM        = 0x0000004,
	REGEX      = 0x0000008,
	DUMP_MASK  = 0x000000e, // HLSL, Assembly and/or ShaderRegex is selected
	MONO_SS    = 0x0000010,
	STEREO_SS  = 0x0000020,
	SS_IF_PINK = 0x0000040,

	DEFAULT    = 0x0000003,
};
SENSIBLE_ENUM(MarkingAction);
static EnumName_t<const wchar_t *, MarkingAction> MarkingActionNames[] = {
	{L"hlsl", MarkingAction::HLSL},
	{L"asm", MarkingAction::ASM},
	{L"assembly", MarkingAction::ASM},
	{L"regex", MarkingAction::REGEX},
	{L"ShaderRegex", MarkingAction::REGEX},
	{L"clipboard", MarkingAction::CLIPBOARD},
	{L"mono_snapshot", MarkingAction::MONO_SS},
	{L"stereo_snapshot", MarkingAction::STEREO_SS},
	{L"snapshot_if_pink", MarkingAction::SS_IF_PINK},
	{NULL, MarkingAction::INVALID} // End of list marker
};

enum class ShaderHashType {
	INVALID = -1,
	FNV,
	EMBEDDED,
	BYTECODE,
};
static EnumName_t<const wchar_t *, ShaderHashType> ShaderHashNames[] = {
	{L"3dmigoto", ShaderHashType::FNV},
	{L"embedded", ShaderHashType::EMBEDDED},
	{L"bytecode", ShaderHashType::BYTECODE},
	{NULL, ShaderHashType::INVALID} // End of list marker
};

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
	ID3D11ClassLinkage* linkage;
	ID3DBlob* byteCode;
	FILETIME timeStamp;
	ID3D11DeviceChild* replacement;
	bool found;
	bool deferred_replacement_candidate;
	bool deferred_replacement_processed;
	std::wstring infoText;
};

// Call this after any CreateXXXShader call to ensure that all references to
// the handle have been removed in the event that it has been reused:
void CleanupShaderMaps(ID3D11DeviceChild *handle);

// Key is the overridden shader that was given back to the game at CreateVertexShader (vs or ps)
typedef std::unordered_map<ID3D11DeviceChild *, OriginalShaderInfo> ShaderReloadMap;

// TODO: We can probably merge this into ShaderReloadMap
typedef std::unordered_map<ID3D11DeviceChild *, ID3D11DeviceChild *> ShaderReplacementMap;

// Key is shader, value is hash key.
typedef std::unordered_map<ID3D11DeviceChild *, UINT64> ShaderMap;

enum class FrameAnalysisOptions {
	INVALID         = 0,

	// Bind selection:
	DUMP_RT         = 0x00000001,
	DUMP_DEPTH      = 0x00000002,
	DUMP_SRV        = 0x00000004,
	DUMP_CB         = 0x00000008,
	DUMP_VB         = 0x00000010,
	DUMP_IB         = 0x00000020,

	// Format selection:
	FMT_2D_AUTO     = 0x00000040,
	FMT_2D_JPS      = 0x00000080,
	FMT_2D_DDS      = 0x00000100,
	FMT_BUF_BIN     = 0x00000200,
	FMT_BUF_TXT     = 0x00000400,
	FMT_DESC        = 0x00000800,

	// Masks:
	DUMP_XB_MASK    = 0x00000038, // CB+VB+IB, to check if a user specified any of these
	FMT_2D_MASK     = 0x000009c0, // Mask of Texture2D formats
	FMT_BUF_MASK    = 0x00000e00, // Mask of Buffer formats

	// Legacy bind + format combo options:
	DUMP_RT_JPS     = 0x00000081,
	DUMP_RT_DDS     = 0x00000301,
	DUMP_DEPTH_JPS  = 0x00000082,
	DUMP_DEPTH_DDS  = 0x00000302,
	DUMP_TEX_JPS    = 0x00000084,
	DUMP_TEX_DDS    = 0x00000304,
	DUMP_CB_TXT     = 0x00000408,
	DUMP_VB_TXT     = 0x00000410,
	DUMP_IB_TXT     = 0x00000420,

	// Misc options:
	CLEAR_RT        = 0x00001000,
	FILENAME_REG    = 0x00002000,
	FILENAME_HANDLE = 0x00004000,
	PERSIST         = 0x00008000, // Used by shader/texture triggers
	STEREO          = 0x00010000,
	MONO            = 0x00020000,
	STEREO_MASK     = 0x00030000,
	HOLD            = 0x00040000,
	DUMP_ON_UNMAP   = 0x00080000,
	DUMP_ON_UPDATE  = 0x00100000,
	SHARE_DEDUPED   = 0x00200000,
	DEFRD_CTX_IMM   = 0x00400000,
	DEFRD_CTX_DELAY = 0x00800000,
	DEFRD_CTX_MASK  = 0x00c00000,
	SYMLINK         = 0x01000000,
	DEPRECATED      = (signed)0x80000000,
};
SENSIBLE_ENUM(FrameAnalysisOptions);
static EnumName_t<wchar_t *, FrameAnalysisOptions> FrameAnalysisOptionNames[] = {
	// Bind flag selection:
	{L"dump_rt", FrameAnalysisOptions::DUMP_RT},
	{L"dump_depth", FrameAnalysisOptions::DUMP_DEPTH},
	{L"dump_tex", FrameAnalysisOptions::DUMP_SRV},
	{L"dump_cb", FrameAnalysisOptions::DUMP_CB},
	{L"dump_vb", FrameAnalysisOptions::DUMP_VB},
	{L"dump_ib", FrameAnalysisOptions::DUMP_IB},

	// Texture2D format selection:
	{L"jps", FrameAnalysisOptions::FMT_2D_JPS},
	{L"jpg", FrameAnalysisOptions::FMT_2D_JPS},
	{L"jpeg", FrameAnalysisOptions::FMT_2D_JPS},
	{L"dds", FrameAnalysisOptions::FMT_2D_DDS},
	{L"jps_dds", FrameAnalysisOptions::FMT_2D_AUTO},
	{L"jpg_dds", FrameAnalysisOptions::FMT_2D_AUTO},
	{L"jpeg_dds", FrameAnalysisOptions::FMT_2D_AUTO},

	// Buffer format selection:
	{L"buf", FrameAnalysisOptions::FMT_BUF_BIN},
	{L"txt", FrameAnalysisOptions::FMT_BUF_TXT},

	{L"desc", FrameAnalysisOptions::FMT_DESC},

	// Misc options:
	{L"clear_rt", FrameAnalysisOptions::CLEAR_RT},
	{L"persist", FrameAnalysisOptions::PERSIST},
	{L"stereo", FrameAnalysisOptions::STEREO},
	{L"mono", FrameAnalysisOptions::MONO},
	{L"filename_reg", FrameAnalysisOptions::FILENAME_REG},
	{L"filename_handle", FrameAnalysisOptions::FILENAME_HANDLE},
	{L"log", FrameAnalysisOptions::DEPRECATED}, // Left in the list for backwards compatibility, but this is now always enabled
	{L"hold", FrameAnalysisOptions::HOLD},
	{L"dump_on_unmap", FrameAnalysisOptions::DUMP_ON_UNMAP},
	{L"dump_on_update", FrameAnalysisOptions::DUMP_ON_UPDATE},
	{L"deferred_ctx_immediate", FrameAnalysisOptions::DEFRD_CTX_IMM},
	{L"deferred_ctx_accurate", FrameAnalysisOptions::DEFRD_CTX_DELAY},
	{L"share_dupes", FrameAnalysisOptions::SHARE_DEDUPED},
	{L"symlink", FrameAnalysisOptions::SYMLINK},

	// Legacy combo options:
	{L"dump_rt_jps", FrameAnalysisOptions::DUMP_RT_JPS},
	{L"dump_rt_dds", FrameAnalysisOptions::DUMP_RT_DDS},
	{L"dump_depth_jps", FrameAnalysisOptions::DUMP_DEPTH_JPS}, // Doesn't work yet
	{L"dump_depth_dds", FrameAnalysisOptions::DUMP_DEPTH_DDS},
	{L"dump_tex_jps", FrameAnalysisOptions::DUMP_TEX_JPS},
	{L"dump_tex_dds", FrameAnalysisOptions::DUMP_TEX_DDS},
	{L"dump_cb_txt", FrameAnalysisOptions::DUMP_CB_TXT},
	{L"dump_vb_txt", FrameAnalysisOptions::DUMP_VB_TXT},
	{L"dump_ib_txt", FrameAnalysisOptions::DUMP_IB_TXT},

	{NULL, FrameAnalysisOptions::INVALID} // End of list marker
};

enum class DepthBufferFilter {
	INVALID = -1,
	NONE,
	DEPTH_ACTIVE,
	DEPTH_INACTIVE,
};
static EnumName_t<const wchar_t *, DepthBufferFilter> DepthBufferFilterNames[] = {
	{L"none", DepthBufferFilter::NONE},
	{L"depth_active", DepthBufferFilter::DEPTH_ACTIVE},
	{L"depth_inactive", DepthBufferFilter::DEPTH_INACTIVE},
	{NULL, DepthBufferFilter::INVALID} // End of list marker
};

struct ShaderOverride {
	std::wstring first_ini_section;
	DepthBufferFilter depth_filter;
	UINT64 partner_hash;
	char model[20]; // More than long enough for even ps_4_0_level_9_0
	int allow_duplicate_hashes;
	float filter_index, backup_filter_index;

	CommandList command_list;
	CommandList post_command_list;

	ShaderOverride() :
		depth_filter(DepthBufferFilter::NONE),
		partner_hash(0),
		allow_duplicate_hashes(1),
		filter_index(FLT_MAX),
		backup_filter_index(FLT_MAX)
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
	float backup_filter_index;

	bool has_draw_context_match;
	bool has_match_priority;
	int priority;
	FuzzyMatch match_first_vertex;
	FuzzyMatch match_first_index;
	FuzzyMatch match_first_instance;
	FuzzyMatch match_vertex_count;
	FuzzyMatch match_index_count;
	FuzzyMatch match_instance_count;

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

typedef std::unordered_map<ID3D11Resource *, ResourceHandleInfo> ResourceMap;

// The TextureOverrideList will be sorted because we want multiple
// [TextureOverrides] that share the same hash (differentiated by draw context
// matching) to always be processed in the same order for consistent results.
// We can't use a std::set to enforce this ordering, as that makes the
// TextureOverrides const, but there are a few places we modify it. Instead, we
// will sort it in the ini parser when we create the list.
typedef std::vector<struct TextureOverride> TextureOverrideList;
typedef std::unordered_map<uint32_t, TextureOverrideList> TextureOverrideMap;

// We use this when collecting resource info for ShaderUsage.txt to take a
// snapshot of the resource handle, hash and original hash. We used to just
// save the resource handle, but that was problematic since handles can get
// reused, and so we could record the wrong hash in the ShaderUsage.txt
struct ResourceSnapshot
{
	ID3D11Resource *handle;
	uint32_t hash;
	uint32_t orig_hash;

	ResourceSnapshot(ID3D11Resource *handle, uint32_t hash, uint32_t orig_hash):
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
	std::map<int, std::set<ResourceSnapshot>> ResourceRegisters;
	std::set<UINT64> PeerShaders;
	std::vector<std::set<ResourceSnapshot>> RenderTargets;
	std::map<int, std::set<ResourceSnapshot>> UAVs;
	std::set<ResourceSnapshot> DepthTargets;
};

enum class GetResolutionFrom {
	INVALID       = -1,
	SWAP_CHAIN,
	DEPTH_STENCIL,
};
static EnumName_t<const wchar_t *, GetResolutionFrom> GetResolutionFromNames[] = {
	{L"swap_chain", GetResolutionFrom::SWAP_CHAIN},
	{L"depth_stencil", GetResolutionFrom::DEPTH_STENCIL},
	{NULL, GetResolutionFrom::INVALID} // End of list marker
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

enum class AsyncQueryType
{
	QUERY,
	PREDICATE,
	COUNTER,
};

struct Globals
{
	bool gInitialized;
	bool bIntendedTargetExe;
	bool gReloadConfigPending;
	bool gWipeUserConfig;
	bool gLogInput;
	bool dump_all_profiles;
	DWORD ticks_at_launch;

	wchar_t SHADER_PATH[MAX_PATH];
	wchar_t SHADER_CACHE_PATH[MAX_PATH];
	wchar_t CHAIN_DLL_PATH[MAX_PATH];
	int load_library_redirect;

	std::wstring user_config;
	int user_config_dirty;

	EnableHooks enable_hooks;
	
	bool enable_check_interface;
	int enable_create_device;
	bool enable_platform_update;

	int GAME_INTERNAL_WIDTH; // this variable stores the resolution width provided by the game (required for the upscaling feature)
	int GAME_INTERNAL_HEIGHT; // this variable stores the resolution height provided by the game (required for the upscaling feature)
	int SCREEN_WIDTH;
	int SCREEN_HEIGHT;
	int SCREEN_REFRESH;
	int SCREEN_FULLSCREEN;
	int SCREEN_UPSCALING;
	int UPSCALE_MODE;
	int FILTER_REFRESH[11];
	bool SCREEN_ALLOW_COMMANDS;
	bool upscaling_hooks_armed;
	bool upscaling_command_list_using_explicit_bb_flip;
	bool bb_is_upscaling_bb;
	bool implicit_post_checktextureoverride_used;

	MarkingMode marking_mode;
	MarkingAction marking_actions;
	int gForceStereo;
	bool gCreateStereoProfile;
	int gSurfaceCreateMode;
	int gSurfaceSquareCreateMode;
	bool gForceNoNvAPI;

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
	bool assemble_signature_comments;
	bool disassemble_undecipherable_custom_data;
	bool patch_cb_offsets;
	int recursive_include;
	uint32_t ZBufferHashToInject;
	DecompilerSettings decompiler_settings;
	bool DumpUsage;
	bool ENABLE_TUNE;
	float gTuneValue[4], gTuneStep;

	std::vector<DirectX::XMFLOAT4> iniParams;
	int iniParamsReserved;
	int StereoParamsReg;
	int IniParamsReg;

	ResolutionInfo mResolutionInfo;
	CommandList present_command_list;
	CommandList post_present_command_list;
	CommandList clear_rtv_command_list;
	CommandList post_clear_rtv_command_list;
	CommandList clear_dsv_command_list;
	CommandList post_clear_dsv_command_list;
	CommandList clear_uav_float_command_list;
	CommandList post_clear_uav_float_command_list;
	CommandList clear_uav_uint_command_list;
	CommandList post_clear_uav_uint_command_list;
	CommandList constants_command_list;
	CommandList post_constants_command_list;
	bool constants_run;
	unsigned frame_no;
	HWND hWnd; // To translate mouse coordinates to the window
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

	std::set<UINT64> mVisitedVertexShaders;					// Only shaders seen since last hunting timeout; std::set for consistent order while hunting
	UINT64 mSelectedVertexShader;				 			// Hash.  -1 now for unselected state. The shader selected using Input object.
	int mSelectedVertexShaderPos;							// -1 for unselected state.
	std::set<uint32_t> mSelectedVertexShader_IndexBuffer;	// std::set so that index buffers used with a shader will be sorted in log when marked
	std::set<uint32_t> mSelectedVertexShader_VertexBuffer;	// std::set so that index buffers used with a shader will be sorted in log when marked

	std::set<UINT64> mVisitedPixelShaders;					// std::set is sorted for consistent order while hunting
	UINT64 mSelectedPixelShader;							// Hash.  -1 now for unselected state.
	int mSelectedPixelShaderPos;							// -1 for unselected state.
	std::set<uint32_t> mSelectedPixelShader_IndexBuffer;	// std::set so that index buffers used with a shader will be sorted in log when marked
	std::set<uint32_t> mSelectedPixelShader_VertexBuffer;	// std::set so that index buffers used with a shader will be sorted in log when marked
	ID3D11PixelShader* mPinkingShader;						// Special pixels shader to mark a selection with hot pink.

	ShaderMap mShaders;										// All shaders ever registered with CreateXXXShader
	ShaderReloadMap mReloadedShaders;						// Shaders that were reloaded live from ShaderFixes
	ShaderReplacementMap mOriginalShaders;					// When MarkingMode=Original, switch to original. Also used for show_original and shader reversion

	std::set<UINT64> mVisitedComputeShaders;
	UINT64 mSelectedComputeShader;
	int mSelectedComputeShaderPos;

	std::set<UINT64> mVisitedGeometryShaders;
	UINT64 mSelectedGeometryShader;
	int mSelectedGeometryShaderPos;

	std::set<UINT64> mVisitedDomainShaders;
	UINT64 mSelectedDomainShader;
	int mSelectedDomainShaderPos;

	std::set<UINT64> mVisitedHullShaders;
	UINT64 mSelectedHullShader;
	int mSelectedHullShaderPos;

	ShaderOverrideMap mShaderOverrideMap;
	TextureOverrideMap mTextureOverrideMap;
	FuzzyTextureOverrides mFuzzyTextureOverrides;

	// Statistics
	///////////////////////////////////////////////////////////////////////
	//                                                                   //
	//                  <==============================>                 //
	//                  < AB-BA TYPE DEADLOCK WARNING! >                 //
	//                  <==============================>                 //
	//                                                                   //
	// mResources is now protected by its own lock.                      //
	//                                                                   //
	// Never call into DirectX while holding g->mResourcesLock. DirectX  //
	// can take a lock of it's own, introducing a locking dependency. At //
	// other times DirectX can call into our resource release tracker    //
	// with their lock held, and we take the G->mResourcesLock, which    //
	// is another locking order dependency in the other direction,       //
	// leading to an AB-BA type deadlock.                                //
	//                                                                   //
	// If you ever need to obtain both mCriticalSection and              //
	// mResourcesLock simultaneously, be sure to take mCriticalSection   //
	// first so as not to introduce a three way AB-BC-CA deadlock.       //
	//                                                                   //
	// It's recommended to enable debug_locks=1 when working on any code //
	// dealing with these locks to detect ordering violations that have  //
	// the potential to lead to a deadlock if the timing is unfortunate. //
	//                                                                   //
	///////////////////////////////////////////////////////////////////////
	CRITICAL_SECTION mResourcesLock;
	ResourceMap mResources;

	std::unordered_map<ID3D11Asynchronous*, AsyncQueryType> mQueryTypes;

	// These five items work with the *original* resource hash:
	ResourceInfoMap mResourceInfo;
	std::set<uint32_t> mRenderTargetInfo;					// std::set so that ShaderUsage.txt is sorted - lookup time is O(log N)
	std::set<uint32_t> mUnorderedAccessInfo;				// std::set so that ShaderUsage.txt is sorted - lookup time is O(log N)
	std::set<uint32_t> mDepthTargetInfo;					// std::set so that ShaderUsage.txt is sorted - lookup time is O(log N)
	std::set<uint32_t> mShaderResourceInfo;					// std::set so that ShaderUsage.txt is sorted - lookup time is O(log N)
	std::set<uint32_t> mCopiedResourceInfo;					// std::set so that ShaderUsage.txt is sorted - lookup time is O(log N)

	std::set<ID3D11Resource *> mVisitedRenderTargets;						// std::set is sorted for consistent order while hunting
	ID3D11Resource *mSelectedRenderTarget;
	int mSelectedRenderTargetPos;
	// Snapshot of all targets for selection.
	ID3D11Resource *mSelectedRenderTargetSnapshot;
	std::set<ID3D11Resource *> mSelectedRenderTargetSnapshotList;			// std::set so that render targets will be sorted in log when marked
	// Relations
	std::map<UINT64, ShaderInfoData> mVertexShaderInfo;			// std::map so that ShaderUsage.txt is sorted - lookup time is O(log N)
	std::map<UINT64, ShaderInfoData> mHullShaderInfo;			// std::map so that ShaderUsage.txt is sorted - lookup time is O(log N)
	std::map<UINT64, ShaderInfoData> mDomainShaderInfo;			// std::map so that ShaderUsage.txt is sorted - lookup time is O(log N)
	std::map<UINT64, ShaderInfoData> mGeometryShaderInfo;		// std::map so that ShaderUsage.txt is sorted - lookup time is O(log N)
	std::map<UINT64, ShaderInfoData> mPixelShaderInfo;			// std::map so that ShaderUsage.txt is sorted - lookup time is O(log N)
	std::map<UINT64, ShaderInfoData> mComputeShaderInfo;		// std::map so that ShaderUsage.txt is sorted - lookup time is O(log N)

	Globals() :

		mSelectedRenderTargetSnapshot(0),
		mSelectedRenderTargetPos(-1),
		mSelectedRenderTarget((ID3D11Resource *)-1),
		mSelectedPixelShader(-1),
		mSelectedPixelShaderPos(-1),
		mSelectedVertexShader(-1),
		mSelectedVertexShaderPos(-1),
		mSelectedIndexBuffer(-1),
		mSelectedIndexBufferPos(-1),
		mSelectedVertexBuffer(-1),
		mSelectedVertexBufferPos(-1),
		mSelectedComputeShader(-1),
		mSelectedComputeShaderPos(-1),
		mSelectedGeometryShader(-1),
		mSelectedGeometryShaderPos(-1),
		mSelectedDomainShader(-1),
		mSelectedDomainShaderPos(-1),
		mSelectedHullShader(-1),
		mSelectedHullShaderPos(-1),
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

		iniParamsReserved(0),

		constants_run(false),
		frame_no(0),
		hWnd(NULL),
		hide_cursor(false),
		cursor_upscaling_bypass(true),
		check_foreground_window(false),

		GAME_INTERNAL_WIDTH(1), // it gonna be used by mouse pos hook in case of softwaremouse is on and it can be called before
		GAME_INTERNAL_HEIGHT(1),//  the swap chain is created and the proper data set to avoid errors in the hooked winapi functions
		SCREEN_WIDTH(-1),
		SCREEN_HEIGHT(-1),
		SCREEN_REFRESH(-1),
		SCREEN_FULLSCREEN(0),
		SCREEN_ALLOW_COMMANDS(false),
		upscaling_hooks_armed(true),
		upscaling_command_list_using_explicit_bb_flip(false),
		bb_is_upscaling_bb(false),
		implicit_post_checktextureoverride_used(false),

		marking_mode(MarkingMode::INVALID),
		marking_actions(MarkingAction::INVALID),
		gForceStereo(0),
		gCreateStereoProfile(false),
		gSurfaceCreateMode(-1),
		gSurfaceSquareCreateMode(-1),
		gForceNoNvAPI(false),
		ZBufferHashToInject(0),
		SCISSOR_DISABLE(0),

		load_library_redirect(2),
		enable_hooks(EnableHooks::INVALID),
		enable_check_interface(false),
		enable_create_device(0),
		enable_platform_update(false),
		gInitialized(false),
		bIntendedTargetExe(false),
		gReloadConfigPending(false),
		gWipeUserConfig(false),
		user_config_dirty(0),
		gLogInput(false),
		dump_all_profiles(false)
	{
		int i;

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

// Everything in this struct has a unique copy per thread. It would be vastly
// simpler to just use the "thread_local" keyword, but MSDN warns that it can
// interfere with delay loading DLLs (without any detail as to what it means by
// that), so to err on the side of caution I'm using the old Win32 TLS API. We
// are using a structure to ensure we only consume a single TLS slot since they
// are limited, regardless of how many thread local variables we might want in
// the future. Use the below accessor function to get a pointer to this
// structure for the current thread.
struct TLS
{
	// This is set before calling into a DirectX function known to be
	// problematic if hooks are in use that can lead to one of our
	// functions being called unexpectedly if DirectX (or a third party
	// tool sitting between us and DirectX) has implemented the function we
	// are calling in terms of other hooked functions. We check if it is
	// set from any function known to be one called by DirectX and call
	// straight through to the original function if it is set.
	//
	// This is very much a band-aid solution to one of the fundamental
	// problems associated with hooking, but unfortunately hooking is a
	// reality we cannot avoid and in many cases a necessary evil to solve
	// certain problems. This is not a complete solution - it protects
	// against known cases where a function we call can manage to call back
	// into us, but does not protect against unknown cases of the same
	// problem, or cases where we call a function that has been hooked by a
	// third party tool (which we can use other strategies to avoid, such
	// as the unhookable UnhookableCreateDevice).
	bool hooking_quirk_protection;

	LockStack locks_held;

	TLS() :
		hooking_quirk_protection(false)
	{}
};

extern DWORD tls_idx;
static struct TLS* get_tls()
{
	TLS *tls;

	tls = (TLS*)TlsGetValue(tls_idx);
	if (!tls) {
		tls = new TLS();
		TlsSetValue(tls_idx, tls);
	}

	return tls;
}

extern Globals *G;

static inline ShaderMap::iterator lookup_shader_hash(ID3D11DeviceChild *shader)
{
	return Profiling::lookup_map(G->mShaders, shader, &Profiling::shader_hash_lookup_overhead);
}

static inline ShaderReloadMap::iterator lookup_reloaded_shader(ID3D11DeviceChild *shader)
{
	return Profiling::lookup_map(G->mReloadedShaders, shader, &Profiling::shader_reload_lookup_overhead);
}

static inline ShaderReplacementMap::iterator lookup_original_shader(ID3D11DeviceChild *shader)
{
	return Profiling::lookup_map(G->mOriginalShaders, shader, &Profiling::shader_original_lookup_overhead);
}

static inline ShaderOverrideMap::iterator lookup_shaderoverride(UINT64 hash)
{
	return Profiling::lookup_map(G->mShaderOverrideMap, hash, &Profiling::shaderoverride_lookup_overhead);
}

static inline ResourceMap::iterator lookup_resource_handle_info(ID3D11Resource *resource)
{
	return Profiling::lookup_map(G->mResources, resource, &Profiling::texture_handle_info_lookup_overhead);
}

static inline TextureOverrideMap::iterator lookup_textureoverride(uint32_t hash)
{
	return Profiling::lookup_map(G->mTextureOverrideMap, hash, &Profiling::textureoverride_lookup_overhead);
}
