#pragma once

#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <forward_list>
#include "util.h"
#include "DrawCallInfo.h"
#include "ResourceHash.h"
#include <chrono>
#include <nvapi.h>

#include <d3d9.h>
#include <D3dx9math.h>
namespace D3D9Wrapper{
	class IDirect3DDevice9;
	class IDirect3DBaseTexture9;
	class IDirect3DSurface9;
}
struct CachedStereoValues {
	float KnownConvergence = -1.0f;
	float KnownSeparation = -1.0f;
	float KnownEyeSeparation = -1.0f;
	bool StereoActiveIsKnown = false;
	bool KnownStereoActive = false;
	bool StereoEnabledIsKnown = false;
	bool KnownStereoEnabled = false;
};
NvAPI_Status SetConvergence(D3D9Wrapper::IDirect3DDevice9 *device, CachedStereoValues *cachedStereoValues, float convergence);
NvAPI_Status SetSeparation(D3D9Wrapper::IDirect3DDevice9 *device, CachedStereoValues *cachedStereoValues, float seperation);
NvAPI_Status GetConvergence(D3D9Wrapper::IDirect3DDevice9 *device, CachedStereoValues *cachedStereoValues, float * convergence);
NvAPI_Status GetSeparation(D3D9Wrapper::IDirect3DDevice9 *device, CachedStereoValues *cachedStereoValues, float * seperation);
NvAPI_Status GetEyeSeparation(D3D9Wrapper::IDirect3DDevice9 *device, CachedStereoValues *cachedStereoValues, float * eyeseperation);
NvAPI_Status GetStereoActive(D3D9Wrapper::IDirect3DDevice9 *device, CachedStereoValues *cachedStereoValues, bool * active);
NvAPI_Status GetStereoEnabled(CachedStereoValues *cachedStereoValues, bool *enabled);
enum class FrameAnalysisOptions;
// Used to prevent typos leading to infinite recursion (or at least overflowing
// the real stack) due to a section running itself or a circular reference. 64
// should be more than generous - I don't want it to be too low and stifle
// people's imagination, but I'd be very surprised if anyone ever has a
// legitimate need to exceed this:
#define MAX_COMMAND_LIST_RECURSION 64

typedef HRESULT(*CopyLevelSur)(D3D9Wrapper::IDirect3DDevice9 *mHackerDevice, ::IDirect3DSurface9 *srcLev, ::IDirect3DSurface9 *dstLev, D3D2DTEXTURE_DESC *srcDesc, D3D2DTEXTURE_DESC *dstDesc, RECT *srcRect, RECT *dstRect);
typedef HRESULT(*CopyLevelVol)(D3D9Wrapper::IDirect3DDevice9 *mHackerDevice, ::IDirect3DVolume9 *srcLev, ::IDirect3DVolume9 *dstLev, D3D3DTEXTURE_DESC *srcDesc, D3D3DTEXTURE_DESC *dstDesc, ::D3DBOX *srcRect, ::D3DBOX *dstRect);

template <CopyLevelSur c>
struct CopyLevelSurface { static HRESULT copyLevel(D3D9Wrapper::IDirect3DDevice9 *mHackerDevice, ::IDirect3DSurface9 *srcLev, ::IDirect3DSurface9 *dstLev, D3D2DTEXTURE_DESC *srcDesc, D3D2DTEXTURE_DESC *dstDesc, RECT *srcRect, RECT *dstRect); };
template <CopyLevelVol c>
struct CopyLevelVolume { static HRESULT copyLevel(D3D9Wrapper::IDirect3DDevice9 *mHackerDevice, ::IDirect3DVolume9 *srcLev, ::IDirect3DVolume9 *dstLev, D3D3DTEXTURE_DESC *srcDesc, D3D3DTEXTURE_DESC *dstDesc, ::D3DBOX *srcRect, ::D3DBOX *dstRect); };

typedef struct D3D9_ALPHATEST_DESC {
	DWORD alpha_ref;
	BOOL alpha_test_enable;
	::D3DCMPFUNC alpha_func;
}D3D9_ALPHATEST_DESC;

typedef struct D3D9_RASTERIZER_DESC {
	::D3DFILLMODE fill_mode;
	::D3DCULL cull_mode;
	DWORD depth_bias;
	DWORD slope_scale_depth_bias;
	DWORD clipping;
	DWORD scissor_test_enable;
	DWORD anti_aliased_line_enable;
	DWORD clip_plane_enable;
	BOOL multisample_antialias;
}D3D9_RASTERIZER_DESC;

typedef struct D3D9_DEPTH_STENCIL_DESC {
	BOOL stencil_enable;
	::D3DSTENCILOP stencil_fail;
	::D3DSTENCILOP stencil_z_fail;
	::D3DSTENCILOP stencil_pass;
	::D3DCMPFUNC stencil_func;
	UINT stencil_mask;
	UINT stencil_write_mask;
	UINT stencil_ref;
	BOOL z_enable;
	BOOL z_write_enable;
	::D3DCMPFUNC z_func;
	BOOL two_sided_stencil_mode;
	::D3DSTENCILOP ccw_stencil_fail;
	::D3DSTENCILOP ccw_stencil_z_fail;
	::D3DSTENCILOP ccw_stencil_pass;
	::D3DCMPFUNC ccw_stencil_func;
	UINT depth_bias;

}D3D9_DEPTH_STENCIL_DESC;

typedef struct D3D9_BLEND_DESC {
	DWORD alpha_blend_enable;
	::D3DBLEND src_blend;
	::D3DBLEND dest_blend;
	::D3DBLENDOP blend_op;
	DWORD seperate_alpha_blend_enable;
	::D3DBLEND src_blend_alpha;
	::D3DBLEND dest_blend_alpha;
	::D3DBLENDOP blend_op_alpha;
	DWORD color_write_enable;
	::D3DCOLOR blend_factor;
	::D3DCOLOR texture_factor;
	DWORD color_write_enable1;
	DWORD color_write_enable2;
	DWORD color_write_enable3;
	DWORD multisample_mask;
}D3D9_BLEND_DESC;

typedef struct D3D9_SAMPLER_DESC {
	::D3DTEXTUREADDRESS address_u;
	::D3DTEXTUREADDRESS address_v;
	::D3DTEXTUREADDRESS address_w;
	DWORD mip_map_lod_bias;
	UINT max_anisotropy;
	::D3DCOLOR border_colour;
	DWORD max_mip_level;
	::D3DTEXTUREFILTERTYPE mag_filter;
	::D3DTEXTUREFILTERTYPE min_filter;
	::D3DTEXTUREFILTERTYPE mip_filter;
	DWORD srgb_texture;
	DWORD element_index;
	DWORD dmap_offset;
}D3D9_SAMPLER_DESC;

class ID3D9AlphaTestState
{
private:
	D3D9_ALPHATEST_DESC m_pDesc;
public:

	ID3D9AlphaTestState(D3D9_ALPHATEST_DESC pDesc)
	{
		m_pDesc = pDesc;
	}
	~ID3D9AlphaTestState() {}

	void GetDesc(
		D3D9_ALPHATEST_DESC *pDesc
	) {
		*pDesc = m_pDesc;
	}
};
class ID3D9DepthStencilState
{
private:
	D3D9_DEPTH_STENCIL_DESC m_pDesc;
public:
	ID3D9DepthStencilState(D3D9_DEPTH_STENCIL_DESC pDesc)
	{
		m_pDesc = pDesc;
	}
	~ID3D9DepthStencilState() {}
	void GetDesc(
		D3D9_DEPTH_STENCIL_DESC *pDesc
	) {
		*pDesc = m_pDesc;
	}
};
class ID3D9BlendState
{
private:
	D3D9_BLEND_DESC m_pDesc;
public:

	ID3D9BlendState(D3D9_BLEND_DESC pDesc)
	{
		m_pDesc = pDesc;
	}
	~ID3D9BlendState() {}
	void GetDesc(
		D3D9_BLEND_DESC *pDesc
	) {
		*pDesc = m_pDesc;
	}
};
class ID3D9RasterizerState
{
private:
	D3D9_RASTERIZER_DESC m_pDesc;
public:
	ID3D9RasterizerState(D3D9_RASTERIZER_DESC pDesc)
	{
		m_pDesc = pDesc;
	}

	~ID3D9RasterizerState() {}

	void GetDesc(
		D3D9_RASTERIZER_DESC *pDesc
	) {
		*pDesc = m_pDesc;
	}
};
class ID3D9SamplerState
{
private:
	D3D9_SAMPLER_DESC m_pDesc;
public:
	ID3D9SamplerState(D3D9_SAMPLER_DESC pDesc)
	{
		m_pDesc = pDesc;
	}

	~ID3D9SamplerState() {}

	void GetDesc(
		D3D9_SAMPLER_DESC *pDesc
	) {
		*pDesc = m_pDesc;
	}
};

class ResourceCopyTarget;
class CommandListState {
public:
	D3D9Wrapper::IDirect3DDevice9 *mHackerDevice;
	::IDirect3DDevice9 *mOrigDevice;
	// Used to avoid querying the render target dimensions twice in the
	// common case we are going to store both width & height in separate
	// ini params:
	float rt_width, rt_height;
	DrawCallInfo *call_info;
	bool post;
	bool aborted;

	bool scissor_valid;
	RECT scissor_rect;

	// If set this resource is in some way related to the command list
	// invocation - a constant buffer we are analysing, a render target
	// being cleared, etc.
	ResourceCopyTarget *this_target;
	::IDirect3DResource9 *resource;
	// TODO: Cursor info and resources would be better off being cached
	// somewhere that is updated at most once per frame rather than once
	// per command list execution, and we would ideally skip the resource
	// creation if the cursor is unchanged.
	CURSORINFO cursor_info;
	POINT cursor_window_coords;
	ICONINFO cursor_info_ex;
	RECT window_rect;
	int recursion;
	int extra_indent;
	LARGE_INTEGER profiling_time_recursive;

	// Anything that needs to be updated at the end of the command list:
	bool update_params;
	unordered_map<DWORD, D3D9Wrapper::IDirect3DBaseTexture9*> m_activeStereoTextureStages;

	bool resolved_depth_replacement;
	bool copied_depth_replacement;

	CachedStereoValues *cachedStereoValues;
	CommandListState();
	~CommandListState();
};

class CommandListCommand {
public:
	wstring ini_line;

	// For performance metrics:
	LARGE_INTEGER pre_time_spent;
	LARGE_INTEGER post_time_spent;
	unsigned pre_executions;
	unsigned post_executions;

	virtual ~CommandListCommand() {};

	virtual void run(CommandListState*) = 0;
	virtual bool optimise(D3D9Wrapper::IDirect3DDevice9 *device) { return false; }
	virtual bool noop(bool post, bool ignore_cto_pre, bool ignore_cto_post) { return false; }
};

// Using vector of pointers to allow mixed types, and shared_ptr to handle
// destruction of each object:
//typedef std::vector<std::shared_ptr<CommandListCommand>> CommandList;

enum class VariableFlags {
	NONE = 0,
	GLOBAL = 0x00000001,
	PERSIST = 0x00000002,
	INVALID = (signed)0xffffffff,
};
SENSIBLE_ENUM(VariableFlags);
static EnumName_t<const wchar_t *, VariableFlags> VariableFlagNames[] = {
	{ L"global", VariableFlags::GLOBAL },
{ L"persist", VariableFlags::PERSIST },
{ NULL, VariableFlags::INVALID } // End of list marker
};

class CommandListVariable {
public:
	wstring name;
	VariableFlags flags;

	CommandListVariable(wstring name, VariableFlags flags) :
		name(name),flags(flags)
	{}
};
class CommandListVariableFloat : public CommandListVariable
{
public:
	float fval;
	CommandListVariableFloat(wstring name, float fval, VariableFlags flags) :
		CommandListVariable(name, flags), fval(fval)
	{}
};
class CommandListVariableArray : public CommandListVariable
{
public:
	vector<float> fvals;
	CommandListVariableArray(wstring name, vector<float> fvals, VariableFlags flags) :
		CommandListVariable(name, flags), fvals(fvals)
	{}
};
class CommandListMatrix : public CommandListVariable
{
public:
	::D3DXMATRIX fmatrix;
	CommandListMatrix(wstring name, ::D3DXMATRIX matrix, VariableFlags flags) :
		CommandListVariable(name, flags), fmatrix(matrix)
	{}
};
typedef std::unordered_map<std::wstring, class CommandListVariableFloat> CommandListVariableFloats;
extern CommandListVariableFloats command_list_globals;
extern std::vector<CommandListVariableFloat*> persistent_variables;

typedef std::unordered_map<std::wstring, class CommandListVariableArray> CommandListVariableArrays;
extern CommandListVariableArrays command_list_global_arrays;
extern std::vector<CommandListVariableArray*> persistent_variable_arrays;

typedef std::unordered_map<std::wstring, class CommandListMatrix> CommandListMatrices;
extern CommandListMatrices command_list_global_matrices;
extern std::vector<CommandListMatrix*> persistent_matrices;

// The scope object is used to declare local variables in a command list. The
// multiple levels are to isolate variables declared inside if blocks from
// being accessed in a parent or sibling scope, while allowing variables
// declared in a parent scope to be used in an if block. This scope object is
// only used during command list parsing - once allocated the commands and
// operands referencing these variables will point directly to the variable
// objects to avoid slow lookups at runtime, and the scope object will be
// cleared to save memory (with some refactoring we could potentially even
// remove it from the CommandList class altogether).
typedef std::forward_list<std::unordered_map<std::wstring, CommandListVariableFloat*>> CommandListScope;
typedef std::forward_list<std::unordered_map<std::wstring, CommandListVariableArray*>> CommandListScopeArrays;
typedef std::forward_list<std::unordered_map<std::wstring, CommandListMatrix*>> CommandListScopeMatrices;

class CommandList {
public:
	// Using vector of pointers to allow mixed types, and shared_ptr to handle
	// destruction of each object:
	typedef std::vector<std::shared_ptr<CommandListCommand>> Commands;
	Commands commands;

	// For local/static variables. These are only used in the main pre
	// command list as the post command list and any sub command lists (if
	// blocks, etc) shares the same local variables and scope object as the
	// pre list. static_vars used to hold the variables in the pre command
	// list so they can be freed along with the command list, but at
	// runtime they are accessed directly by pointer. forward_list is used
	// because it doesn't invalidate pointers on insertion like vectors do.
	std::forward_list<CommandListVariableFloat> static_vars;
	std::forward_list<CommandListVariableArray> static_var_arrays;
	std::forward_list<CommandListMatrix> static_matrices;
	CommandListScope *scope;
	CommandListScopeArrays *scope_arrays;
	CommandListScopeMatrices *scope_matrices;

	// For performance metrics:
	wstring ini_section;
	bool post;
	LARGE_INTEGER time_spent_inclusive;
	LARGE_INTEGER time_spent_exclusive;
	unsigned executions;
	void clear();

	CommandList() :
		post(false),
		scope(NULL),
		scope_arrays(NULL),
		scope_matrices(NULL)
	{}
};

extern vector<CommandList*> registered_command_lists;
extern unordered_set<CommandList*> command_lists_profiling;
extern unordered_set<CommandListCommand*> command_lists_cmd_profiling;


// Forward declaration to avoid circular reference since Override.h includes
// HackerDevice.h includes HackerContext.h includes CommandList.h
class PresetOverride;

class PresetCommand : public CommandListCommand {
public:
	PresetOverride *preset;
	bool exclude;

	PresetCommand() :
		preset(NULL),
		exclude(false)
	{}

	void run(CommandListState*) override;
};
class ExplicitCommandListSection
{
public:
	CommandList command_list;
	CommandList post_command_list;
};

typedef std::unordered_map<std::wstring, class ExplicitCommandListSection> ExplicitCommandListSections;
extern ExplicitCommandListSections explicitCommandListSections;

class RunExplicitCommandList : public CommandListCommand {
public:
	ExplicitCommandListSection *command_list_section;
	bool run_pre_and_post_together;

	RunExplicitCommandList() :
		command_list_section(NULL),
		run_pre_and_post_together(false)
	{}

	void run(CommandListState*) override;
	bool noop(bool post, bool ignore_cto_pre, bool ignore_cto_post) override;
};

class RunLinkedCommandList : public CommandListCommand {
public:
	CommandList *link;

	RunLinkedCommandList(CommandList *link) :
		link(link)
	{}

	void run(CommandListState*) override;
	bool noop(bool post, bool ignore_cto_pre, bool ignore_cto_post) override;
};

// https://msdn.microsoft.com/en-us/library/windows/desktop/gg615083(v=vs.85).aspx
enum class D3DCompileFlags {
	DEBUG = (1 << 0),
	SKIP_VALIDATION = (1 << 1),
	SKIP_OPTIMIZATION = (1 << 2),
	PACK_MATRIX_ROW_MAJOR = (1 << 3),
	PACK_MATRIX_COLUMN_MAJOR = (1 << 4),
	PARTIAL_PRECISION = (1 << 5),
	FORCE_VS_SOFTWARE_NO_OPT = (1 << 6),
	FORCE_PS_SOFTWARE_NO_OPT = (1 << 7),
	NO_PRESHADER = (1 << 8),
	AVOID_FLOW_CONTROL = (1 << 9),
	PREFER_FLOW_CONTROL = (1 << 10),
	ENABLE_STRICTNESS = (1 << 11),
	ENABLE_BACKWARDS_COMPATIBILITY = (1 << 12),
	IEEE_STRICTNESS = (1 << 13),
	OPTIMIZATION_LEVEL0 = (1 << 14),
	OPTIMIZATION_LEVEL1 = 0,
	OPTIMIZATION_LEVEL2 = ((1 << 14) | (1 << 15)),
	OPTIMIZATION_LEVEL3 = (1 << 15),
	WARNINGS_ARE_ERRORS = (1 << 18),
	RESOURCES_MAY_ALIAS = (1 << 19),
	ENABLE_UNBOUNDED_DESCRIPTOR_TABLES = (1 << 20),
	ALL_RESOURCES_BOUND = (1 << 21),
	INVALID = (signed)0xffffffff,
};
SENSIBLE_ENUM(D3DCompileFlags);
static EnumName_t<const wchar_t *, D3DCompileFlags> D3DCompileFlagNames[] = {
	{ L"debug", D3DCompileFlags::DEBUG },
{ L"skip_validation", D3DCompileFlags::SKIP_VALIDATION },
{ L"skip_optimization", D3DCompileFlags::SKIP_OPTIMIZATION },
{ L"pack_matrix_row_major", D3DCompileFlags::PACK_MATRIX_ROW_MAJOR },
{ L"pack_matrix_column_major", D3DCompileFlags::PACK_MATRIX_COLUMN_MAJOR },
{ L"partial_precision", D3DCompileFlags::PARTIAL_PRECISION },
{ L"force_vs_software_no_opt", D3DCompileFlags::FORCE_VS_SOFTWARE_NO_OPT },
{ L"force_ps_software_no_opt", D3DCompileFlags::FORCE_PS_SOFTWARE_NO_OPT },
{ L"no_preshader", D3DCompileFlags::NO_PRESHADER },
{ L"avoid_flow_control", D3DCompileFlags::AVOID_FLOW_CONTROL },
{ L"prefer_flow_control", D3DCompileFlags::PREFER_FLOW_CONTROL },
{ L"enable_strictness", D3DCompileFlags::ENABLE_STRICTNESS },
{ L"enable_backwards_compatibility", D3DCompileFlags::ENABLE_BACKWARDS_COMPATIBILITY },
{ L"ieee_strictness", D3DCompileFlags::IEEE_STRICTNESS },
{ L"optimization_level0", D3DCompileFlags::OPTIMIZATION_LEVEL0 },
{ L"optimization_level1", D3DCompileFlags::OPTIMIZATION_LEVEL1 },
{ L"optimization_level2", D3DCompileFlags::OPTIMIZATION_LEVEL2 },
{ L"optimization_level3", D3DCompileFlags::OPTIMIZATION_LEVEL3 },
{ L"warnings_are_errors", D3DCompileFlags::WARNINGS_ARE_ERRORS },
// d3dcompiler47 only, but they won't hurt and adding them now means we
// can use them when we do migrate later:
{ L"resources_may_alias", D3DCompileFlags::RESOURCES_MAY_ALIAS },
{ L"enable_unbounded_descriptor_tables", D3DCompileFlags::ENABLE_UNBOUNDED_DESCRIPTOR_TABLES },
{ L"all_resources_bound", D3DCompileFlags::ALL_RESOURCES_BOUND },
{ NULL, D3DCompileFlags::INVALID } // End of list marker
};

class CustomShader
{
public:
	D3DCompileFlags compile_flags;
	D3D9Wrapper::IDirect3DDevice9 *mHackerDevice;
	bool vs_override, ps_override;
	::IDirect3DVertexShader9 *vs;
	::IDirect3DPixelShader9 *ps;
	bool enable_timer;
	chrono::high_resolution_clock::duration run_interval;
	chrono::high_resolution_clock::time_point last_time_run;

	ID3DBlob *vs_bytecode;
	ID3DBlob *ps_bytecode;

	int blend_override;
	int alpha_test_override;
	D3D9_BLEND_DESC blend_desc;
	D3D9_BLEND_DESC blend_mask;
	ID3D9BlendState *blend_state;
	FLOAT blend_factor[4];
	FLOAT blend_factor_merge_mask[4];
	UINT blend_sample_mask_merge_mask;

	int depth_stencil_override;

	D3D9_DEPTH_STENCIL_DESC depth_stencil_desc;
	D3D9_DEPTH_STENCIL_DESC depth_stencil_mask;
	ID3D9DepthStencilState *depth_stencil_state;
	UINT stencil_ref_mask;

	D3D9_ALPHATEST_DESC alpha_test_desc;
	ID3D9AlphaTestState *alpha_test_state;

	int rs_override;

	D3D9_RASTERIZER_DESC rs_desc;
	D3D9_RASTERIZER_DESC rs_mask;
	ID3D9RasterizerState *rs_state;

	int sampler_override;
	map<UINT, ID3D9SamplerState*> sampler_states;

	::D3DPRIMITIVETYPE primitive_type;
	CommandList command_list;
	CommandList post_command_list;

	bool substantiated;

	int max_executions_per_frame;
	unsigned frame_no;
	int executions_this_frame;

	CustomShader();
	~CustomShader();

	bool compile(char type, wchar_t *filename, const wstring *wname, const wstring *namespace_path);
	void substantiate(::IDirect3DDevice9 *mOrigDevice, D3D9Wrapper::IDirect3DDevice9 *mDevice);

	void merge_blend_states(ID3D9BlendState *src_state, ::IDirect3DDevice9 *mOrigDevice);
	void merge_depth_stencil_states(ID3D9DepthStencilState *state);
	void merge_rasterizer_states(ID3D9RasterizerState *state, ::IDirect3DDevice9 *mOrigDevice);
};

typedef std::unordered_map<std::wstring, class CustomShader> CustomShaders;
extern CustomShaders customShaders;

class RunCustomShaderCommand : public CommandListCommand {
public:
	CustomShader *custom_shader;

	RunCustomShaderCommand() :
		custom_shader(NULL)
	{}
	void GetSamplerStates(::IDirect3DDevice9 * mOrigDevice, std::map<UINT, ID3D9SamplerState*> *saved_sampler_states);
	void SetSamplerStates(::IDirect3DDevice9 * mOrigDevice, std::map<UINT, ID3D9SamplerState*> ss);
	void run(CommandListState*) override;
	bool noop(bool post, bool ignore_cto_pre, bool ignore_cto_post) override;
};

enum class DrawCommandType {
	INVALID,
	DRAW,
	DRAWPRIMITIVE,
	DRAWINDEXED,
	DRAWINDEXEDPRIMITIVE,
	DRAWPRIMITIVEUP,
	DRAWINDEXEDPRIMITIVEUP,
	DRAWRECTPATCH,
	DRAWTRIPATCH,
	// 3DMigoto special draw commands:
	FROM_CALLER,
	AUTO_INDEX_COUNT,
};

class DrawCommand : public CommandListCommand {
public:
	wstring ini_section;
	DrawCommandType type;

	UINT args[5];
	DrawCommand::DrawCommand() :
		type(DrawCommandType::INVALID)
	{}

	void run(CommandListState*) override;
	void draw(CommandListState*, DrawCommandType);
};

class SkipCommand : public CommandListCommand {
public:
	wstring ini_section;

	SkipCommand(wstring section) :
		ini_section(section)
	{}

	void run(CommandListState*) override;
};

// Handling=abort aborts the current command list, and any command lists that
// called it. e.g. it can be used in conjunction with checktextureoverride = oD
// to abort command list execution if a specific depth target is in use.
class AbortCommand : public CommandListCommand {
public:
	wstring ini_section;

	AbortCommand(wstring section) :
		ini_section(section)
	{}

	void run(CommandListState*) override;
};


enum class CustomResourceType {
	INVALID,
	VERTEXBUFFER,
	INDEXBUFFER,
	RENDERTARGET,
	DEPTHSTENCILSURFACE,
	OFFSCREENPLAIN,
	TEXTURE2D,
	TEXTURE3D,
	CUBE

};
static EnumName_t<const wchar_t *, CustomResourceType> CustomResourceTypeNames[] = {
	// Use the same names as HLSL here since they are what shaderhackers
	// will see in the shaders, even if some of these have no distinction
	// from our point of view, or are just a misc flag:
	{ L"VertexBuffer", CustomResourceType::VERTEXBUFFER },
	{ L"IndexBuffer", CustomResourceType::INDEXBUFFER },
	{ L"RenderTarget", CustomResourceType::RENDERTARGET },
	{ L"DepthStencilSurface", CustomResourceType::DEPTHSTENCILSURFACE },
	{ L"OffscreenPlain", CustomResourceType::OFFSCREENPLAIN },
	{ L"Texture2D", CustomResourceType::TEXTURE2D },
	{ L"Texture3D", CustomResourceType::TEXTURE3D },
	{ L"TextureCube", CustomResourceType::CUBE },
	{ NULL, CustomResourceType::INVALID } // End of list marker
};
enum class CustomResourceMode {
	DEFAULT,
	AUTO,
	STEREO,
	MONO,
};
static EnumName_t<const wchar_t *, CustomResourceMode> CustomResourceModeNames[] = {
	{ L"auto", CustomResourceMode::AUTO },
	{ L"stereo", CustomResourceMode::STEREO },
	{ L"mono", CustomResourceMode::MONO },
	{ NULL, CustomResourceMode::DEFAULT } // End of list marker
};
// The bind flags are usually set automatically, but there are cases where
// these can be used to influence driver heuristics (e.g. a buffer that
// includes a render target or UAV bind flag may be stereoised), so we allow
// them to be set manually as well. If specified these will *replace* the bind
// flags 3DMigoto sets automatically - if you use these, you presumably know
// what you are doing. This enumeration is essentially the same as
// D3D11_BIND_FLAG, but this allows us to use parse_enum_option_string.
enum class CustomResourceUsageFlags {
	INVALID = 0x00000000,
	AUTOGENMIPMAP = 0x00000400L,
	DEPTHSTENCIL = 0x00000002L,
	DMAP = 0x00004000L,
	DONOTCLIP = 0x00000020L,
	DYNAMIC = 0x00000200L,
	NONSECURE = 0x00800000L,
	NPATCHES = 0x00000100L,
	POINTS = 0x00000040L,
	RENDERTARGET = 0x00000001L,
	RTPATCHES = 0x00000080L,
	SOFTWAREPROCESSING = 0x00000010L,
	TEXTAPI = 0x10000000L,
	WRITEONLY = 0x00000008L,
	RESTRICTED_CONTENT = 0x00000800L,
	RESTRICT_SHARED_RESOURCE = 0x00002000L,
	RESTRICT_SHARED_RESOURCE_DRIVER = 0x00001000L,
};
SENSIBLE_ENUM(CustomResourceUsageFlags);
static EnumName_t<const wchar_t *, CustomResourceUsageFlags> CustomResourceUsageFlagNames[] = {
	{ L"auto_gen_mip_map", CustomResourceUsageFlags::AUTOGENMIPMAP },
	{ L"depth_stencil", CustomResourceUsageFlags::DEPTHSTENCIL },
	{ L"d_map", CustomResourceUsageFlags::DMAP },
	{ L"do_not_clip", CustomResourceUsageFlags::DONOTCLIP },
	{ L"dynamic", CustomResourceUsageFlags::DYNAMIC },
	{ L"non_secure", CustomResourceUsageFlags::NONSECURE },
	{ L"n_patches", CustomResourceUsageFlags::NPATCHES },
	{ L"points", CustomResourceUsageFlags::POINTS },
	{ L"render_target", CustomResourceUsageFlags::RENDERTARGET },
	{ L"rt_patches", CustomResourceUsageFlags::RTPATCHES },
	{ L"software_processing", CustomResourceUsageFlags::SOFTWAREPROCESSING },
	{ L"text_api", CustomResourceUsageFlags::TEXTAPI },
	{ L"write_only", CustomResourceUsageFlags::WRITEONLY },
	{ L"restricted_content", CustomResourceUsageFlags::RESTRICTED_CONTENT },
	{ L"restrict_shared_resource", CustomResourceUsageFlags::RESTRICT_SHARED_RESOURCE },
	{ L"restrict_shared_resource_driver", CustomResourceUsageFlags::RESTRICT_SHARED_RESOURCE_DRIVER },
	{ NULL, CustomResourceUsageFlags::INVALID } // End of list marker
};

// The ResourcePool holds a pool of cached resources for when a single copy
// operation or a custom resource may be copied to from multiple distinct
// incompatible resources (e.g. they may have differing sizes). This saves us
// from having to destroy the old cache and create a new one any time the game
// switches.
//
// The hash we are using is crc32c for the moment, which I think should (though
// I have not verified) produce distinct hashes for all distinct permutations
// of resource types and descriptions. We don't explicitly introduce any
// variations for different resource types, instead relying on the fact that
// the description size of each resource type is unique - and it would be
// highly unusual (though not forbidden) to mix different resource types in a
// single pool anyway.
struct pair_hash {
	template <class T1, class T2>
	std::size_t operator () (const std::pair<T1, T2> &p) const {
		auto h1 = std::hash<T1>{}(p.first);
		auto h2 = std::hash<T2>{}(p.second);
		return h1 ^ h2;
	}
};
struct ResourceCreationInfo {
	NVAPI_STEREO_SURFACECREATEMODE mode;
	ResourceCreationInfo() : mode(NVAPI_STEREO_SURFACECREATEMODE::NVAPI_STEREO_SURFACECREATEMODE_AUTO) {}//, stereo2mono_index(0) {}
};
struct HashedResource {
	::IDirect3DResource9 *resource = NULL;
};

// The ResourcePool holds a pool of cached resources for when a single copy
// operation or a custom resource may be copied to from multiple distinct
// incompatible resources (e.g. they may have differing sizes). This saves us
// from having to destroy the old cache and create a new one any time the game
// switches.
//
// The hash we are using is crc32c for the moment, which I think should (though
// I have not verified) produce distinct hashes for all distinct permutations
// of resource types and descriptions. We don't explicitly introduce any
// variations for different resource types, instead relying on the fact that
// the description size of each resource type is unique - and it would be
// highly unusual (though not forbidden) to mix different resource types in a
// single pool anyway.
typedef unordered_map<pair<uint32_t, D3D9Wrapper::IDirect3DDevice9*>, HashedResource, pair_hash> ResourcePoolCache;
class ResourcePool
{
public:
	ResourcePool();

	ResourcePoolCache cache;
	~ResourcePool();

	void emplace(uint32_t hash, HashedResource hashedResource, D3D9Wrapper::IDirect3DDevice9 *mHackerDevice);
};
class CommandListExpression;
class CustomResource
{
public:
	wstring name;

	::IDirect3DResource9 *resource;
	ResourcePool resource_pool;
	bool is_null;
	DWORD resource_use;
	DWORD usage_flags;
	UINT stride;
	UINT offset;
	UINT buf_size;
	::D3DFORMAT format;

	int max_copies_per_frame;
	unsigned frame_no;
	int copies_this_frame;

	wstring filename;
	bool substantiated;

	// Used to override description when copying or synthesise resources
	// from scratch:
	CustomResourceType override_type;
	CustomResourceMode override_mode;
	CustomResourceUsageFlags override_usage_flags;
	::D3DPOOL override_pool;
	::D3DFORMAT override_format;
	int override_width;
	int override_height;

	CommandListExpression *override_width_expression;
	CommandListExpression *override_height_expression;
	CommandListExpression *override_depth_expression;

	int override_depth;
	int override_mips;
	int override_msaa;
	int override_msaa_quality;
	int override_byte_width;
	int override_stride;
	float width_multiply;
	float height_multiply;
	float depth_multiply;

	void *initial_data;
	size_t initial_data_size;

	CustomResource();
	~CustomResource();

	void Substantiate (CommandListState *state, StereoHandle mStereoHandle, DWORD usage_flags);
	bool OverrideSurfaceCreationMode(StereoHandle mStereoHandle, NVAPI_STEREO_SURFACECREATEMODE *new_mode);

	void OverrideBufferDesc(::D3DVERTEXBUFFER_DESC *desc);
	void OverrideBufferDesc(::D3DINDEXBUFFER_DESC * desc);
	void OverrideSurfacesDesc(D3D2DTEXTURE_DESC *desc, CommandListState *state);
	void OverrideSurfacesDesc(D3D3DTEXTURE_DESC *desc, CommandListState *state);
	void OverrideOutOfBandInfo(::D3DFORMAT *format, UINT *stride);

private:
	void LoadFromFile(::IDirect3DDevice9 *mOrigDevice);
	template<typename ID3D9Buffer, typename ID3D9BufferDesc>
	void LoadBufferFromFile(::IDirect3DDevice9 *mOrigDevice);
	template<typename ID3D9Buffer, typename ID3D9BufferDesc>
	void SubstantiateBuffer(::IDirect3DDevice9 *mOrigDevice, void **buf, DWORD size);
	void SubstantiateTexture2D(CommandListState *state);
	void SubstantiateTexture3D(CommandListState *state);
	void SubstantiateTextureCube(CommandListState *state);
	void SubstantiateRenderTarget(CommandListState *state);
	void SubstantiateDepthStencilSurface(CommandListState *state);
	void SubstantiateOffscreenPlain(CommandListState *state);
};

typedef std::unordered_map<std::wstring, class CustomResource> CustomResources;
extern CustomResources customResources;

// Forward declaration since TextureOverride also contains a command list
struct TextureOverride;

enum class ResourceCopyTargetType {
	INVALID,
	EMPTY,
	SHADER_RESOURCE,
	// TODO: SAMPLER, // Not really a resource, but might still be useful
	VERTEX_BUFFER,
	INDEX_BUFFER,
	RENDER_TARGET,
	DEPTH_STENCIL_TARGET,
	CUSTOM_RESOURCE,
	STEREO_PARAMS,
	CURSOR_MASK,
	CURSOR_COLOR,
	THIS_RESOURCE, // For constant buffer analysis & render/depth target clearing
	SWAP_CHAIN,
	REAL_SWAP_CHAIN,
	FAKE_SWAP_CHAIN, // need this for upscaling used with "f_bb" flag in  the .ini file
	CPU, // For staging resources to the CPU, e.g. for auto-convergence
	REPLACEMENT_DEPTH_TEXTURE,
	REPLACEMENT_DEPTH_SURFACE
};
enum class ResourceCopyOptions {
	INVALID = 0,
	COPY = 0x00000001,
	REFERENCE = 0x00000002,
	UNLESS_NULL = 0x00000004,
	RESOLVE_MSAA = 0x00000008,
	STEREO = 0x00000010,
	MONO = 0x00000020,
	STEREO2MONO = 0x00000040,
	COPY_DESC = 0x00000080,
	SET_VIEWPORT = 0x00000100,
	COPY_MASK = 0x000000c9, // Anything that implies a copy
	COPY_TYPE_MASK = 0x000000cb, // Anything that implies a copy or a reference
	CREATEMODE_MASK = 0x00000070,
};
SENSIBLE_ENUM(ResourceCopyOptions);
static EnumName_t<wchar_t *, ResourceCopyOptions> ResourceCopyOptionNames[] = {
	{ L"copy", ResourceCopyOptions::COPY },
{ L"ref", ResourceCopyOptions::REFERENCE },
{ L"reference", ResourceCopyOptions::REFERENCE },
{ L"copy_desc", ResourceCopyOptions::COPY_DESC },
{ L"copy_description", ResourceCopyOptions::COPY_DESC },
{ L"unless_null", ResourceCopyOptions::UNLESS_NULL },
{ L"stereo", ResourceCopyOptions::STEREO },
{ L"mono", ResourceCopyOptions::MONO },
{ L"stereo2mono", ResourceCopyOptions::STEREO2MONO },
{ L"set_viewport", ResourceCopyOptions::SET_VIEWPORT },

// This one currently depends on device support for resolving the
// given texture format (D3D11_FORMAT_SUPPORT_MULTISAMPLE_RESOLVE), and
// currently has no fallback, so we can't rely on it - don't encourage
// people to use it and don't document it. TODO: Implement a fallback
// using a shader to resolve any unsupported formats.
{ L"resolve_msaa", ResourceCopyOptions::RESOLVE_MSAA },

{ NULL, ResourceCopyOptions::INVALID } // End of list marker
};
// TODO: Add support for more behaviour modifiers, here's a few ideas
// off the top of my head - I don't intend to implement all these
// unless we have a proven need for them or maybe if they are trivial
// and have real potential to be useful later. For now they are just
// food for thought:
//
// res_format= - override DXGI Format when creating a resource
// view_format= - override DXGI Format when creating a view
// if_dest_is_null - only perform the operation if the destination is not currently assigned
// if_dest_is_compatible - only perform the operation if the destination exists, and is compatible with the source
// if_dest_is_null_or_incompatible - only perform the operation if the destination is not currently assigned, or is incompatible
// copy_subresource_region=... - Use copy_subresource_region instead of copy_resource
// mip_map, array, etc. - create a view that exposes only part of the resource
// overwrite - instead of creating a new resource for a copy operation, overwrite the resource already assigned to the destination (if it exists and is compatible)
class ResourceCopyTarget {
public:
	ResourceCopyTargetType type;
	wchar_t shader_type;
	unsigned slot;
	CustomResource *custom_resource;
	ResourceCopyTarget() :
		type(ResourceCopyTargetType::INVALID),
		shader_type(L'\0'),
		slot(0),
		custom_resource(NULL)
	{}

	bool ParseTarget(const wchar_t *target, bool is_source, const wstring *ini_namespace);
	::IDirect3DResource9 *GetResource(CommandListState *state,
		UINT *stride,
		UINT *offset,
		::D3DFORMAT *format,
		UINT *buf_size,
		ResourceCopyTarget *dst = NULL,
		D3D9Wrapper::IDirect3DResource9 **wrapper = NULL);
	void SetResource(CommandListState *state,
		::IDirect3DResource9 *res,
		UINT stride,
		UINT offset,
		::D3DFORMAT format,
		UINT buf_size,
		D3D9Wrapper::IDirect3DResource9 **wrapper = NULL);
	void FindTextureOverrides(
		CommandListState *state,
		bool *resource_found,
		TextureOverrideMatches *matches);
	HRESULT DirectModeGetRealBackBuffer(CommandListState *state, UINT iBackBuffer, D3D9Wrapper::IDirect3DSurface9 **ppBackBuffer);
	HRESULT DirectModeGetFakeBackBuffer(CommandListState *state, UINT iBackBuffer, D3D9Wrapper::IDirect3DSurface9 **ppBackBuffer);
	DWORD UsageFlags(CommandListState *state);
};

class ResourceCopyOperation : public CommandListCommand {
public:
	ResourceCopyTarget src;
	ResourceCopyTarget dst;
	ResourceCopyOptions options;
	::IDirect3DResource9 *cached_resource;
	ResourcePool resource_pool;
	// Additional intermediate resources required for certain operations
	// (TODO: add alternate cache in CustomResource to cut down on extra
	// copies when copying to a single resource from many sources)
	::IDirect3DTexture9 *stereo2mono_intermediate;

	ResourceCopyOperation();
	~ResourceCopyOperation();
	void DirectModeCopyResource(CommandListState *state, ::IDirect3DResource9 *src_resource, ::IDirect3DResource9 *dst_resource, bool direct_mode_wrapped_resource_source, bool direct_mode_wrapped_resource_dest);
	void run(CommandListState*) override;
};

typedef std::vector<ResourceCopyOperation*> ResourceCopyOperations;
extern ResourceCopyOperations resourceCopyOperations;

class ResourceStagingOperation : public ResourceCopyOperation {
public:
	bool staging;
	IUnknown *mapped_resource;
	::D3DRESOURCETYPE mapped_resource_type;

	ResourceStagingOperation();

	HRESULT map(CommandListState *state, void **mapping);
	void unmap(CommandListState *state);
	template<typename SourceSurface, typename DestSurface, typename Desc, typename LockedRect>
	HRESULT mapSurface(CommandListState * state, SourceSurface * cached_surface, void ** mapping, DWORD flags = NULL);
	template<typename BufferType, typename Desc>
	HRESULT mapBuffer(CommandListState * state, BufferType * cached_buffer, void **mapping, DWORD flags = NULL);
};
class CommandListToken {
public:
	wstring token;
	size_t token_pos;

	CommandListToken(size_t token_pos, wstring token = L"") :
		token_pos(token_pos), token(token)
	{}
	virtual ~CommandListToken() {}; // Because C++
};

// Expression nodes that are evaluatable - nodes start off as non-evaluatable
// tokens and are later transformed into evaluatable operators and operands
// that inherit from this class.
class CommandListEvaluatable {
public:
	virtual ~CommandListEvaluatable() {}; // Because C++

	virtual float evaluate(CommandListState *state, D3D9Wrapper::IDirect3DDevice9 *device = NULL) = 0;
	virtual bool static_evaluate(float *ret, D3D9Wrapper::IDirect3DDevice9 *device = NULL) = 0;
	virtual bool optimise(D3D9Wrapper::IDirect3DDevice9 *device, std::shared_ptr<CommandListEvaluatable> *replacement) = 0;
};
class CommandListMatrixEvaluatable {
public:
	virtual ~CommandListMatrixEvaluatable() {}; // Because C++
	virtual ::D3DXMATRIX evaluate(CommandListState *state, D3D9Wrapper::IDirect3DDevice9 *device = NULL) = 0;
};
// Indicates that this node can be used as an operand, checked when
// transforming operators to bind adjacent operands. Includes groups of syntax
// that as a whole could be an operand, such as everything between parenthesis,
// and even (transformed) operators. Essentially, everything that is or will
// eventually become evaluatable
class CommandListOperandBase {
public:
};

// Indicates this node requires finalisation - replaces syntax parsing trees
// with evaluatable operands and operators
template <class Evaluatable>
class CommandListFinalisable {
public:
	virtual std::shared_ptr<Evaluatable> finalise() = 0;
};

// Signifies that a node contains other syntax trees that can be walked over.
// Keeping things simple by just returning a vector of contained syntax trees
// that the caller can iterate over - note that this vector will not iterate
// over all the tokens in those syntax trees, just the trees themselves.
// Alternatives are implementing our own iterator, or the visitor pattern.
class CommandListWalkable {
public:
	typedef std::vector<std::shared_ptr<CommandListWalkable>> Walk;
	virtual Walk walk() = 0;
};
template <class Evaluatable>
class CommandListSyntaxTree :
	public CommandListToken,
	public CommandListOperandBase,
	public CommandListFinalisable<Evaluatable>,
	public CommandListWalkable {
public:
	typedef std::vector<std::shared_ptr<CommandListToken>> Tokens;
	Tokens tokens;

	CommandListSyntaxTree(size_t token_pos) :
		CommandListToken(token_pos)
	{}
	std::shared_ptr<Evaluatable> finalise() override;
	Walk walk() override;
};

// Placeholder for operator tokens from the tokenisation stage. These will all
// be transformed into proper operators later in the expression parsing, and
// there should be none left in the final tree.
class CommandListOperatorToken : public CommandListToken {
public:
	CommandListOperatorToken(size_t token_pos, wstring token = L"") :
		CommandListToken(token_pos, token)
	{}
};

// Base class for operators. Subclass this and provide a static pattern and
// concrete evaluate function to implement an operator, then use the factory
// template below to transform matching operator tokens into these.
template <class Evaluatable>
class CommandListOperator :
	public CommandListOperatorToken,
	public CommandListFinalisable<Evaluatable>,
	public CommandListOperandBase,
	public CommandListWalkable {
public:
	std::shared_ptr<CommandListToken> lhs_tree;
	std::shared_ptr<CommandListToken> rhs_tree;
	std::shared_ptr<Evaluatable> lhs;
	std::shared_ptr<Evaluatable> rhs;

	CommandListOperator(
		std::shared_ptr<CommandListToken> lhs,
		CommandListOperatorToken &t,
		std::shared_ptr<CommandListToken> rhs
	) : CommandListOperatorToken(t), lhs_tree(lhs), rhs_tree(rhs)
	{}

	std::shared_ptr<Evaluatable> finalise() override;
	Walk walk() override;

	static const wchar_t* pattern() { return L"<IMPLEMENT ME>"; }
};
class CommandListOperatorFloat :
	public CommandListOperator<CommandListEvaluatable>,
	public CommandListEvaluatable
{
public:

	CommandListOperatorFloat(
		std::shared_ptr<CommandListToken> lhs,
		CommandListOperatorToken &t,
		std::shared_ptr<CommandListToken> rhs
	) : CommandListOperator(lhs, t, rhs)
	{}

	float evaluate(CommandListState *state, D3D9Wrapper::IDirect3DDevice9 *device = NULL) override;
	bool static_evaluate(float *ret, D3D9Wrapper::IDirect3DDevice9 *device = NULL) override;
	bool optimise(D3D9Wrapper::IDirect3DDevice9 *device, std::shared_ptr<CommandListEvaluatable> *replacement) override;

	virtual float evaluate(float lhs, float rhs) = 0;
};
class CommandListMatrixOperator :
	public CommandListOperator<CommandListMatrixEvaluatable>,
	public CommandListMatrixEvaluatable
{
public:

	CommandListMatrixOperator(
		std::shared_ptr<CommandListToken> lhs,
		CommandListOperatorToken &t,
		std::shared_ptr<CommandListToken> rhs
	) : CommandListOperator(lhs, t, rhs)
	{}

	::D3DXMATRIX evaluate(CommandListState *state, D3D9Wrapper::IDirect3DDevice9 *device = NULL) override;
	virtual ::D3DXMATRIX evaluate(::D3DXMATRIX lhs, ::D3DXMATRIX rhs) = 0;
};
// Abstract base factory class for defining operators. Statically instantiate
// the template below for each implemented operator.
template <class Evaluatable>
class CommandListOperatorFactoryBase {
public:
	virtual const wchar_t* pattern() = 0;
	virtual std::shared_ptr<CommandListOperator<Evaluatable>> create(
		std::shared_ptr<CommandListToken> lhs,
		CommandListOperatorToken &t,
		std::shared_ptr<CommandListToken> rhs) = 0;
};

// Template factory class for defining operators. Statically instantiate this
// and pass it in a list to transform_operators to transform all matching
// operator tokens in the syntax tree into fully fledged operators
template <class T, class Evaluatable>
class CommandListOperatorFactory : public CommandListOperatorFactoryBase<Evaluatable>{
public:
	const wchar_t* pattern() override {
		return T::pattern();
	}

	std::shared_ptr<CommandListOperator<Evaluatable>> create(
		std::shared_ptr<CommandListToken> lhs,
		CommandListOperatorToken &t,
		std::shared_ptr<CommandListToken> rhs) override
	{
		return std::make_shared<T>(lhs, t, rhs);
	}
};
enum class ParamOverrideType {
	INVALID,
	VALUE,
	INI_PARAM,
	VARIABLE,
	RT_WIDTH,
	RT_HEIGHT,
	RES_WIDTH,
	RES_HEIGHT,
	WINDOW_WIDTH,
	WINDOW_HEIGHT,
	TEXTURE,	// Needs shader type and slot number specified in
				// [ShaderOverride]. [TextureOverride] sections can
				// specify filter_index=N to define the value passed in
				// here. Special values for no [TextureOverride]
				// section = 0.0, or [TextureOverride] with no
				// filter_index = 1.0
				VERTEX_COUNT,
				INDEX_COUNT,
				CURSOR_VISIBLE,
				CURSOR_SCREEN_X, // Cursor in screen coordinates in pixels
				CURSOR_SCREEN_Y,
				CURSOR_WINDOW_X, // Cursor in window client area coordinates in pixels
				CURSOR_WINDOW_Y,
				CURSOR_X,        // Cursor position scaled so that client area is the range [0:1]
				CURSOR_Y,
				CURSOR_HOTSPOT_X,
				CURSOR_HOTSPOT_Y,
				TIME,
				SCISSOR_LEFT,   // May have an optional scissor rectangle index
				SCISSOR_TOP,    // specified, which is parsed in code, or not -
				SCISSOR_RIGHT,  // in which case it will match the keyword list
				SCISSOR_BOTTOM,
				RAW_SEPARATION, // These get the values as they are right now -
				EYE_SEPARATION, // StereoParams is only updated at the start of each
				CONVERGENCE,    // frame. Intended for use if the convergence may have
				STEREO_ACTIVE,	// been changed during the frame (e.g. if staged from
								// the GPU and it is unknown whether the operation has
								// completed). Comparing these immediately before and
								// after present can be useful to determine if the user
								// is currently adjusting them, which is used for the
								// auto-convergence in Life is Strange: Before the
								// Storm to convert user convergence adjustments into
								// equivalent popout adjustments. stereo_active is used
								// for auto-convergence to remember if stereo was
								// enabled last frame, since it cannot note this itself
								// because if stereo was disabled it would not have run
								// in both eyes to be able to update its state buffer.
								SLI,
								HUNTING,
								FRAME_ANALYSIS,
								VERTEX_SHADER_FLOAT,
								VERTEX_SHADER_INT,
								VERTEX_SHADER_BOOL,
								PIXEL_SHADER_FLOAT,
								PIXEL_SHADER_INT,
								PIXEL_SHADER_BOOL,
								PIXEL_SHADER_TEXTURE_WIDTH,
								PIXEL_SHADER_TEXTURE_HEIGHT,
								VERTEX_SHADER_TEXTURE_WIDTH,
								VERTEX_SHADER_TEXTURE_HEIGHT,
								BACK_BUFFER_WIDTH,
								BACK_BUFFER_HEIGHT,
								VIEWPORT_WIDTH,
								VIEWPORT_HEIGHT,
								CURSOR_COLOR_WIDTH,
								CURSOR_COLOR_HEIGHT,
								CURSOR_MASK_WIDTH,
								CURSOR_MASK_HEIGHT,
								STAGING_OP,
								ISNAN,
								DEPTH_BUFFER_CMPFUNC_GREATER,
								DEPTH_BUFFER_CMPFUNC_GREATEREQUAL,
								INTERNAL_FUNCTION_FLOAT,
								STEREO_PARAMS_UPDATED
};
static EnumName_t<const wchar_t *, ParamOverrideType> ParamOverrideTypeNames[] = {
	{ L"rt_width", ParamOverrideType::RT_WIDTH },
	{ L"rt_height", ParamOverrideType::RT_HEIGHT },
	{ L"res_width", ParamOverrideType::RES_WIDTH },
	{ L"res_height", ParamOverrideType::RES_HEIGHT },
	{ L"window_width", ParamOverrideType::WINDOW_WIDTH },
	{ L"window_height", ParamOverrideType::WINDOW_HEIGHT },
	{ L"vertex_count", ParamOverrideType::VERTEX_COUNT },
	{ L"index_count", ParamOverrideType::INDEX_COUNT },
	{ L"cursor_showing", ParamOverrideType::CURSOR_VISIBLE },
	{ L"cursor_screen_x", ParamOverrideType::CURSOR_SCREEN_X },
	{ L"cursor_screen_y", ParamOverrideType::CURSOR_SCREEN_Y },
	{ L"cursor_window_x", ParamOverrideType::CURSOR_WINDOW_X },
	{ L"cursor_window_y", ParamOverrideType::CURSOR_WINDOW_Y },
	{ L"cursor_x", ParamOverrideType::CURSOR_X },
	{ L"cursor_y", ParamOverrideType::CURSOR_Y },
	{ L"cursor_hotspot_x", ParamOverrideType::CURSOR_HOTSPOT_X },
	{ L"cursor_hotspot_y", ParamOverrideType::CURSOR_HOTSPOT_Y },
	{ L"time", ParamOverrideType::TIME },
	{ L"scissor_left", ParamOverrideType::SCISSOR_LEFT },
	{ L"scissor_top", ParamOverrideType::SCISSOR_TOP },
	{ L"scissor_right", ParamOverrideType::SCISSOR_RIGHT },
	{ L"scissor_bottom", ParamOverrideType::SCISSOR_BOTTOM },
	{ L"separation", ParamOverrideType::RAW_SEPARATION },
	{ L"raw_separation", ParamOverrideType::RAW_SEPARATION },
	{ L"eye_separation", ParamOverrideType::EYE_SEPARATION },
	{ L"convergence", ParamOverrideType::CONVERGENCE },
	{ L"stereo_active", ParamOverrideType::STEREO_ACTIVE },
	{ L"sli", ParamOverrideType::SLI },
	{ L"hunting", ParamOverrideType::HUNTING },
	{ L"frame_analysis", ParamOverrideType::FRAME_ANALYSIS },
	{ L"bb_width", ParamOverrideType::BACK_BUFFER_WIDTH },
	{ L"bb_height", ParamOverrideType::BACK_BUFFER_HEIGHT },
	{ L"viewport_width", ParamOverrideType::VIEWPORT_WIDTH },
	{ L"viewport_height", ParamOverrideType::VIEWPORT_HEIGHT },
	{ L"cursor_color_width", ParamOverrideType::CURSOR_COLOR_WIDTH },
	{ L"cursor_color_height", ParamOverrideType::CURSOR_COLOR_HEIGHT },
	{ L"cursor_mask_width", ParamOverrideType::CURSOR_MASK_WIDTH },
	{ L"cursor_mask_height", ParamOverrideType::CURSOR_MASK_HEIGHT },
	{ L"isnan", ParamOverrideType::ISNAN },
	{ L"stereo_params_updated", ParamOverrideType::STEREO_PARAMS_UPDATED },
	{ NULL, ParamOverrideType::INVALID } // End of list marker
};
class CommandListOperand :
	public CommandListToken,
	public CommandListOperandBase
{

public:
	CommandListOperand(size_t pos, wstring token = L"") :
		CommandListToken(pos, token)
	{}
};
enum class InternalFunctionFloat {
	ACSTATE_LAST_SET_CONVERGENCE,
	ACSTATE_USER_POPOUT_BIAS,
	ACSTATE_JUDDER,
	ACSTATE_JUDDER_TIME,
	ACSTATE_LAST_CONVERGENCE1,
	ACSTATE_LAST_CONVERGENCE2,
	ACSTATE_LAST_CONVERGENCE3,
	ACSTATE_LAST_CONVERGENCE4,
	ACSTATE_TIME,
	ACSTATE_PREV_TIME,
	ACSTATE_CORRECTION_START,
	ACSTATE_LAST_CALCULATED_CONVERGENCE,
	INVALID = (signed)0xffffffff,
};
SENSIBLE_ENUM(InternalFunctionFloat);
static EnumName_t<const wchar_t *, InternalFunctionFloat> InternalFuntionFloatNames[] = {
	{ L"last_set_convergence", InternalFunctionFloat::ACSTATE_LAST_SET_CONVERGENCE },
	{ L"user_popout_bias", InternalFunctionFloat::ACSTATE_USER_POPOUT_BIAS },
	{ L"judder", InternalFunctionFloat::ACSTATE_JUDDER },
	{ L"judder_time", InternalFunctionFloat::ACSTATE_JUDDER_TIME },
	{ L"last_convergence_1", InternalFunctionFloat::ACSTATE_LAST_CONVERGENCE1 },
	{ L"last_convergence_2", InternalFunctionFloat::ACSTATE_LAST_CONVERGENCE2 },
	{ L"last_convergence_3", InternalFunctionFloat::ACSTATE_LAST_CONVERGENCE3 },
	{ L"last_convergence_4", InternalFunctionFloat::ACSTATE_LAST_CONVERGENCE4 },
	{ L"acstate_time", InternalFunctionFloat::ACSTATE_TIME },
	{ L"prev_time", InternalFunctionFloat::ACSTATE_PREV_TIME },
	{ L"correction_start", InternalFunctionFloat::ACSTATE_CORRECTION_START },
	{ L"last_calculated_convergence", InternalFunctionFloat::ACSTATE_LAST_CALCULATED_CONVERGENCE },
	{ NULL, InternalFunctionFloat::INVALID } // End of list marker
};
class CommandListOperandFloat :
	public CommandListOperand,
	public CommandListEvaluatable {
	float process_texture_filter(CommandListState*);
	float staging_op_val(CommandListState *state);
public:
	// TODO: Break up into separate classes for each operand type
	ParamOverrideType type;
	float val;
	float DirectX::XMFLOAT4::*component;
	int idx;
	float *var_ftarget;

	//For mapping from a resource
	ResourceStagingOperation staging_op;

	// For texture filters:
	ResourceCopyTarget texture_filter_target;

	// For scissor rectangle:
	unsigned scissor;

	InternalFunctionFloat iff;

	CommandListOperandFloat(size_t pos, wstring token = L"") :
		CommandListOperand(pos, token),
		type(ParamOverrideType::INVALID),
		val(FLT_MAX),
		component(NULL),
		idx(0),
		var_ftarget(NULL),
		scissor(0),
		iff(InternalFunctionFloat::INVALID)
	{}

	bool parse(const wstring *operand, const wstring *ini_namespace, CommandList *command_list);// CommandListScope *scope);
	float evaluate(CommandListState *state, D3D9Wrapper::IDirect3DDevice9 *device = NULL) override;
	bool static_evaluate(float *ret, D3D9Wrapper::IDirect3DDevice9 *device = NULL) override;
	bool optimise(D3D9Wrapper::IDirect3DDevice9 *device, std::shared_ptr<CommandListEvaluatable> *replacement) override;
};
class CommandListMatrixOperand :
	public CommandListOperand,
	public CommandListMatrixEvaluatable
{
public:
	CommandListMatrixOperand(CommandListMatrix *pMatrix, size_t pos, wstring token = L"") :
		CommandListOperand(pos, token),
		m_pMatrix(pMatrix)
	{}
	CommandListMatrix *m_pMatrix;
	::D3DXMATRIX evaluate(CommandListState *state, D3D9Wrapper::IDirect3DDevice9 *device = NULL) override;
};
class CommandListExpression {
public:
	std::shared_ptr<CommandListEvaluatable> evaluatable;

	bool parse(const wstring *expression, const wstring *ini_namespace, CommandList *command_list);
	float evaluate(CommandListState *state, D3D9Wrapper::IDirect3DDevice9 *device = NULL);
	bool static_evaluate(float *ret, D3D9Wrapper::IDirect3DDevice9 *device = NULL);
	bool optimise(D3D9Wrapper::IDirect3DDevice9 *device);
};
class CommandListMatrixExpression {
public:
	std::shared_ptr<CommandListMatrixEvaluatable> evaluatable;

	bool parse(const wstring *expression, const wstring *ini_namespace, CommandListScopeMatrices *scope);
	::D3DXMATRIX evaluate(CommandListState *state, D3D9Wrapper::IDirect3DDevice9 *device = NULL);
};
class AssignmentCommand : public CommandListCommand {
public:
	CommandListExpression expression;

	bool optimise(D3D9Wrapper::IDirect3DDevice9 *device) override;
};

class ParamOverride : public AssignmentCommand {
public:
	int param_idx;
	float DirectX::XMFLOAT4::*param_component;

	ParamOverride() :
		param_idx(-1),
		param_component(NULL)
	{};

	void run(CommandListState*) override;
};
enum class ConstantType {
	INVALID,
	FLOAT,
	INT,
	BOOL
};
class VariableArrayFromShaderConstantAssignment;
class VariableArrayFromArrayAssignment;
class VariableArrayFromMatrixAssignment;
class VariableArrayFromMatrixExpressionAssignment;
class VariableArrayFromExpressionAssignment;
class VariableArrayAssignment : public CommandListCommand {
public:
	vector<VariableArrayFromShaderConstantAssignment> shader_constant_assignments;
	vector<VariableArrayFromArrayAssignment> array_assignments;
	vector<VariableArrayFromMatrixAssignment> matrix_assignments;
	vector<VariableArrayFromMatrixExpressionAssignment> matrix_expression_assignments;
	vector<VariableArrayFromExpressionAssignment> expression_assignments;

	VariableArrayAssignment() {};
	~VariableArrayAssignment() {};

	void run(CommandListState*) override;
};
class SetShaderConstant : public AssignmentCommand {
public:
	wchar_t shader_type;
	ConstantType constant_type;
	int slot;
	float DirectX::XMFLOAT4::*component;
	vector<float> vars;
	VariableArrayAssignment *assign;
	SetShaderConstant() :
		shader_type(L'\0'),
		constant_type(ConstantType::INVALID),
		slot(-1),
		component(NULL),
		assign(NULL)
	{};
	~SetShaderConstant() {
		if (assign)
			delete assign;
	};
	void run(CommandListState*) override;
};
class VariableArrayFromShaderConstantAssignment : public CommandListCommand {
public:
	vector<float*> vars;
	wchar_t shader_type;
	ConstantType constant_type;
	int slot;

	VariableArrayFromShaderConstantAssignment() :
		shader_type(L'\0'),
		constant_type(ConstantType::INVALID),
		slot(-1)
	{};

	void run(CommandListState*) override;
};
class VariableArrayFromArrayAssignment : public CommandListCommand {
public:
	unordered_map<float*, float*> map;

	VariableArrayFromArrayAssignment(){};

	void run(CommandListState*) override;
};
class VariableArrayFromMatrixAssignment : public CommandListCommand {
public:
	vector<float*> vars;
	::D3DXMATRIX *matrix;
	UINT start_slot;

	VariableArrayFromMatrixAssignment() : matrix(NULL), start_slot(0) {};

	void run(CommandListState*) override;
};
class VariableArrayFromMatrixExpressionAssignment : public CommandListCommand {
public:
	vector<float*> vars;
	CommandListMatrixExpression expression;
	UINT start_slot;

	VariableArrayFromMatrixExpressionAssignment() : start_slot(0){};

	void run(CommandListState*) override;
};
class VariableArrayFromExpressionAssignment : public CommandListCommand {
public:
	float* fval;
	CommandListExpression expression;

	VariableArrayFromExpressionAssignment() {};

	void run(CommandListState*) override;
};

class MatrixFromShaderConstantAssignment : public CommandListCommand {
public:
	::D3DXMATRIX *pMatrix;
	UINT dst_start;
	UINT num_slots;

	UINT index;
	wchar_t shader_type;
	ConstantType constant_type;
	int slot;

	MatrixFromShaderConstantAssignment() :
		shader_type(L'\0'),
		constant_type(ConstantType::INVALID),
		slot(-1),
		pMatrix(NULL),
		dst_start(0),
		num_slots(16)
	{};

	void run(CommandListState*) override;
};
class MatrixFromArrayAssignment : public CommandListCommand {
public:
	::D3DXMATRIX *pMatrix;
	UINT dst_start;
	vector<float*> arr;

	MatrixFromArrayAssignment() : pMatrix(NULL), dst_start(0){};

	void run(CommandListState*) override;
};
class MatrixFromMatrixAssignment : public CommandListCommand {
public:
	::D3DXMATRIX *src_matrix;
	::D3DXMATRIX *dst_matrix;
	UINT src_start;
	UINT dst_start;
	UINT num_slots;

	MatrixFromMatrixAssignment() : src_matrix(NULL), dst_matrix(NULL), src_start(0), dst_start(0), num_slots(16){};

	void run(CommandListState*) override;
};
class MatrixFromMatrixExpressionAssignment : public CommandListCommand {
public:
	::D3DXMATRIX* pMatrix;
	CommandListMatrixExpression expression;
	UINT src_start;
	UINT dst_start;
	UINT num_slots;

	MatrixFromMatrixExpressionAssignment() : pMatrix(NULL), src_start(0), dst_start(0), num_slots(16){};

	void run(CommandListState*) override;
};
class MatrixFromExpressionAssignment : public CommandListCommand {
public:
	::D3DXMATRIX* pMatrix;
	UINT dst_start;
	CommandListExpression expression;

	MatrixFromExpressionAssignment() : pMatrix(NULL), dst_start(0){};

	void run(CommandListState*) override;
};
class MatrixAssignment : public CommandListCommand {
public:
	vector<MatrixFromShaderConstantAssignment> shader_constant_assignments;
	vector<MatrixFromArrayAssignment> array_assignments;
	vector<MatrixFromMatrixAssignment> matrix_assignments;
	vector<MatrixFromMatrixExpressionAssignment> matrix_expression_assignments;
	vector<MatrixFromExpressionAssignment> expression_assignments;

	MatrixAssignment() {};

	void run(CommandListState*) override;
};
class VariableAssignment : public AssignmentCommand {
public:
	CommandListVariableFloat * var;

	VariableAssignment() :
		var(NULL)
	{};

	void run(CommandListState*) override;
};

class IfCommand : public CommandListCommand {
public:
	CommandListExpression expression;
	bool pre_finalised, post_finalised;
	bool has_nested_else_if;
	wstring section;

	// Commands cannot statically contain command lists, because the
	// command may be optimised out and the command list freed while we are
	// still in the middle of optimisations. These are dynamically
	// allocated, and also stored in a second data structure until
	// optimisations are complete
	std::shared_ptr<CommandList> true_commands_pre;
	std::shared_ptr<CommandList> true_commands_post;
	std::shared_ptr<CommandList> false_commands_pre;
	std::shared_ptr<CommandList> false_commands_post;

	IfCommand(const wchar_t *section);

	void run(CommandListState*) override;
	bool optimise(D3D9Wrapper::IDirect3DDevice9 *device) override;
	bool noop(bool post, bool ignore_cto_pre, bool ignore_cto_post) override;
};

class ElseIfCommand : public IfCommand {
public:
	ElseIfCommand(const wchar_t *section) :
		IfCommand(section)
	{}
};

class CommandPlaceholder : public CommandListCommand {
public:
	void run(CommandListState*) override;
	bool noop(bool post, bool ignore_cto_pre, bool ignore_cto_post) override;
};
class ElsePlaceholder : public CommandPlaceholder {
};

class CheckTextureOverrideCommand : public CommandListCommand {
public:
	// For processing command lists in TextureOverride sections:
	ResourceCopyTarget target;
	bool run_pre_and_post_together;

	CheckTextureOverrideCommand() :
		run_pre_and_post_together(false)
	{}

	void run(CommandListState*) override;
	bool noop(bool post, bool ignore_cto_pre, bool ignore_cto_post) override;
};
class ClearSurfaceCommand : public CommandListCommand {
public:
	ResourceCopyTarget target;

	FLOAT dsv_depth;
	UINT8 dsv_stencil;

	// If neither "depth" or "stencil" are specified, both will be used:
	bool clear_depth;
	bool clear_stencil;

	// fval is used for RTV colours and UAVs when clearing them with
	// floating point values. uval is used for UAVs if nothing looked like
	// a float.
	FLOAT fval[4];
	UINT uval[4];
	ClearSurfaceCommand();
	void clear_surface(::IDirect3DResource9 *resource, CommandListState *state);

	void run(CommandListState*) override;
};

class ResetPerFrameLimitsCommand : public CommandListCommand {
public:
	CustomShader *shader;
	CustomResource *resource;

	ResetPerFrameLimitsCommand() :
		shader(NULL),
		resource(NULL)
	{}

	void run(CommandListState*) override;
};
class PerDrawStereoOverrideCommand : public CommandListCommand {
public:
	CommandListExpression expression;
	float val;
	float saved;
	bool restore_on_post;
	bool did_set_value_on_pre;
	bool staging_type;
	ResourceStagingOperation staging_op;

	PerDrawStereoOverrideCommand(bool restore_on_post);

	void run(CommandListState*) override;
	bool optimise(D3D9Wrapper::IDirect3DDevice9 *device) override;
	bool noop(bool post, bool ignore_cto_pre, bool ignore_cto_post) override;
	bool update_val(CommandListState *state);

	virtual const char* stereo_param_name() = 0;
	virtual float get_stereo_value(CommandListState*) = 0;
	virtual void set_stereo_value(CommandListState*, float val) = 0;
};
class PerDrawSeparationOverrideCommand : public PerDrawStereoOverrideCommand
{
public:
	PerDrawSeparationOverrideCommand(bool restore_on_post) :
		PerDrawStereoOverrideCommand(restore_on_post)
	{}

	const char* stereo_param_name() override { return "separation"; }
	float get_stereo_value(CommandListState*) override;
	void set_stereo_value(CommandListState*, float val) override;
};
class PerDrawConvergenceOverrideCommand : public PerDrawStereoOverrideCommand
{
public:
	PerDrawConvergenceOverrideCommand(bool restore_on_post) :
		PerDrawStereoOverrideCommand(restore_on_post)
	{}

	const char* stereo_param_name() override { return "convergence"; }
	float get_stereo_value(CommandListState*) override;
	void set_stereo_value(CommandListState*, float val) override;
};
class DirectModeSetActiveEyeCommand : public CommandListCommand {
public:
	NV_STEREO_ACTIVE_EYE eye;

	void run(CommandListState*) override;
	bool noop(bool post, bool ignore_cto_pre, bool ignore_cto_post) override;
};

class FrameAnalysisChangeOptionsCommand : public CommandListCommand {
public:
	FrameAnalysisOptions analyse_options;

	FrameAnalysisChangeOptionsCommand(wstring *val);

	void run(CommandListState*) override;
	bool noop(bool post, bool ignore_cto_pre, bool ignore_cto_post) override;
};

class FrameAnalysisDumpCommand : public CommandListCommand {
public:
	ResourceCopyTarget target;
	wstring target_name;
	FrameAnalysisOptions analyse_options;

	void run(CommandListState*) override;
	bool noop(bool post, bool ignore_cto_pre, bool ignore_cto_post) override;
};

class FrameAnalysisDumpConstantsCommand : public CommandListCommand {
public:
	char shader_type;
	wchar_t constant_type;
	UINT start_slot;
	UINT num_slots;
	void run(CommandListState*) override;
	bool noop(bool post, bool ignore_cto_pre, bool ignore_cto_post) override;
};

class UpscalingFlipBBCommand : public CommandListCommand {
public:
	wstring ini_section;

	UpscalingFlipBBCommand(wstring section);
	~UpscalingFlipBBCommand();

	void run(CommandListState*) override;
};

class Draw3DMigotoOverlayCommand : public CommandListCommand {
public:
	wstring ini_section;

	Draw3DMigotoOverlayCommand(const wchar_t *section) :
		ini_section(section)
	{}

	void run(CommandListState*) override;
};
enum class BuiltInFunctionName {
	AUTOCONVERGENCE = 0,
	INVALID = (signed)0xffffffff,
};
SENSIBLE_ENUM(BuiltInFunctionName);
static EnumName_t<const wchar_t *, BuiltInFunctionName> BuiltInFunctionsNames[] = {
	{ L"FunctionAutoConvergence", BuiltInFunctionName::AUTOCONVERGENCE },
{ NULL, BuiltInFunctionName::INVALID } // End of list marker
};
//For autoconvergence I implemented some function classes to handle to autoconvergence state tracking (without constant buffers I was unsure how this could be done in DX9 shaders)
//The eventual plan was to embed a python interpreter so that users can run custom scripts in the ini. Given performance should be better in c++
//I also supported built-in functions, which I use for auto-convergence.
extern unordered_map<BuiltInFunctionName, wstring> builtInFunctionDesc;
class Function {
public:
	Function() {};
	~Function() {};
	virtual bool run(CommandListState*, vector<CommandListVariableFloat*> *params) { return false; };
};
class BuiltInFunction : public Function {
public:
	BuiltInFunction() {}
	~BuiltInFunction() {};
};
typedef std::unordered_map<BuiltInFunctionName, BuiltInFunction*> BuiltInFunctions;
typedef std::unordered_map<InternalFunctionFloat, float> InternalFuntionFloats;
extern InternalFuntionFloats internalFunctionFloats;
class FunctionAutoConvergence : public BuiltInFunction {
public:
	FunctionAutoConvergence() : BuiltInFunction() {
	};
	~FunctionAutoConvergence() {};
	bool run(CommandListState*, vector<CommandListVariableFloat*> *params) override;
};
extern BuiltInFunctions builtInFunctions;
class CustomFunction : public Function {
public:
	wstring script;
	CustomFunction();
	~CustomFunction();
	bool run(CommandListState*, vector<CommandListVariableFloat*> *params) override;
};
typedef std::unordered_map<wstring, CustomFunction> CustomFunctions;
extern CustomFunctions customFunctions;
class CustomFunctionCommand : public CommandListCommand {
public:
	Function *function;
	vector<CommandListVariableFloat*> params;
	bool ParseParam(const wchar_t *name, CommandList *pre_command_list, const wstring *ini_namespace);
	CustomFunctionCommand();
	~CustomFunctionCommand();
	void run(CommandListState*) override;
};
void RunCommandList(D3D9Wrapper::IDirect3DDevice9 *mHackerDevice,
	CommandList *command_list, DrawCallInfo *call_info,
	bool post, CachedStereoValues *cachedStereoValues = NULL);
void RunResourceCommandList(D3D9Wrapper::IDirect3DDevice9 *mHackerDevice,
	CommandList *command_list, ::IDirect3DResource9 *resource,
	bool post, CachedStereoValues *cachedStereoValues = NULL);
bool ParseRunExplicitCommandList(const wchar_t *section,
	const wchar_t *key, wstring *val,
	CommandList *explicit_command_list,
	CommandList *pre_command_list,
	CommandList *post_command_list,
	const wstring *ini_namespace);
bool ParseCommandListGeneralCommands(const wchar_t *section,
	const wchar_t *key, wstring *val,
	CommandList *explicit_command_list,
	CommandList *pre_command_list, CommandList *post_command_list,
	const wstring *ini_namespace);
bool ParseCommandListIniParamOverride(const wchar_t *section,
	const wchar_t *key, wstring *val, CommandList *command_list,
	const wstring *ini_namespace);
bool ParseCommandListMatrixAssignment(const wchar_t *section,
	const wchar_t *key, wstring *val, const wstring *raw_line,
	CommandList *command_list, CommandList *pre_command_list, CommandList *post_command_list,
	const wstring *ini_namespace);
bool ParseCommandListVariableArrayAssignment(const wchar_t *section,
	const wchar_t *key, wstring *val, const wstring *raw_line,
	CommandList *command_list, CommandList *pre_command_list, CommandList *post_command_list,
	const wstring *ini_namespace);
bool ParseCommandListVariableAssignment(const wchar_t *section,
	const wchar_t *key, wstring *val, const wstring *raw_line,
	CommandList *command_list, CommandList *pre_command_list, CommandList *post_command_list,
	const wstring *ini_namespace);
bool ParseCommandListResourceCopyDirective(const wchar_t *section,
	const wchar_t *key, wstring *val, CommandList *command_list,
	const wstring *ini_namespace);
bool ParseCommandListFlowControl(const wchar_t *section, const wstring *line,
	CommandList *pre_command_list, CommandList *post_command_list,
	const wstring *ini_namespace);
void LinkCommandLists(CommandList *dst, CommandList *link, const wstring *ini_line);
void optimise_command_lists(D3D9Wrapper::IDirect3DDevice9 *device);
bool parse_command_list_var_name(const wstring &name, const wstring *ini_namespace, CommandListVariableFloat **target);
bool valid_variable_name(const wstring &name);
bool is_matrix(const wstring &declaration, wstring *name);
bool is_variable_array(const wstring &declaration, wstring *name, UINT *size);
bool global_variable_exists(const wstring &name);
bool local_variable_exists(const wstring &name, CommandList *commandList);
bool ParseCommandListSetShaderConstant(const wchar_t *section,
	const wchar_t *key, wstring *val, CommandList *command_list,
	const wstring *ini_namespace);
bool ParseRunBuiltInFunction(const wchar_t *section,
	const wchar_t *key, wstring *val, CommandList *command_list,
	CommandList *pre_command_list, CommandList *post_command_list,
	const wstring *ini_namespace);
bool ParseRunCustomFunction(const wchar_t *section,
	const wchar_t *key, wstring *val, CommandList *command_list,
	CommandList *pre_command_list, CommandList *post_command_list,
	const wstring *ini_namespace);
bool ParseRunFunction(const wchar_t *section,
	const wchar_t *key, wstring *val, CommandList *command_list,
	CommandList *pre_command_list, CommandList *post_command_list,
	const wstring *ini_namespace);
void ReleaseCommandListDeviceResources(D3D9Wrapper::IDirect3DDevice9 *mHackerDevice);
void RecreateCommandListCustomShaders(D3D9Wrapper::IDirect3DDevice9 *mHackerDevice);

bool ParseHelixShaderOverrideGetConstant(const wchar_t *section,
	const wchar_t *key, wstring *val, const wstring *raw_line,
	CommandList *command_list,
	const wstring *ini_namespace, wchar_t shader_type);
bool ParseHelixShaderOverrideSetConstant(const wchar_t *section,
	const wchar_t *key, wstring *val, const wstring *raw_line,
	CommandList *command_list,
	const wstring *ini_namespace, wchar_t shader_type);

bool ParseHelixShaderOverrideGetSampler(const wchar_t *section,
	const wchar_t *key, wstring *val, const wstring *raw_line,
	CommandList *command_list,
	const wstring *ini_namespace, wchar_t shader_type);
bool ParseHelixShaderOverrideSetSampler(const wchar_t *section,
	const wchar_t *key, wstring *val, const wstring *raw_line,
	CommandList *command_list,
	const wstring *ini_namespace, wchar_t shader_type);
