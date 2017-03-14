#pragma once

#include <memory>
#include <unordered_map>
#include <d3d11.h>
#include <DirectXMath.h>
#include <util.h>
#include "DrawCallInfo.h"
#include <nvapi.h>

// Used to prevent typos leading to infinite recursion (or at least overflowing
// the real stack) due to a section running itself or a circular reference. 64
// should be more than generous - I don't want it to be too low and stifle
// people's imagination, but I'd be very surprised if anyone ever has a
// legitimate need to exceed this:
#define MAX_COMMAND_LIST_RECURSION 64

// Forward declarations instead of #includes to resolve circular includes (we
// include Hacker*.h, which includes Globals.h, which includes us):
class HackerDevice;
class HackerContext;

struct CommandListState {
	// Used to avoid querying the render target dimensions twice in the
	// common case we are going to store both width & height in separate
	// ini params:
	float rt_width, rt_height;
	DrawCallInfo *call_info;
	bool post;
	CURSORINFO cursor_info;
	int recursion;

	// Anything that needs to be updated at the end of the command list:
	bool update_params;

	CommandListState() :
		rt_width(-1),
		rt_height(-1),
		call_info(NULL),
		post(false),
		update_params(false),
		cursor_info(),
		recursion(0)
	{}
};

class CommandListCommand {
public:
	virtual ~CommandListCommand() {};

	virtual void run(HackerDevice*, HackerContext*, ID3D11Device*, ID3D11DeviceContext*, CommandListState*) = 0;
};

// Using vector of pointers to allow mixed types, and shared_ptr to handle
// destruction of each object:
typedef std::vector<std::shared_ptr<CommandListCommand>> CommandList;

class CheckTextureOverrideCommand : public CommandListCommand {
public:
	wstring ini_val;
	// For processing command lists in TextureOverride sections:
	wchar_t shader_type;
	unsigned texture_slot;

	CheckTextureOverrideCommand() :
		shader_type(NULL),
		texture_slot(INT_MAX)
	{}

	void run(HackerDevice*, HackerContext*, ID3D11Device*, ID3D11DeviceContext*, CommandListState*) override;
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
	wstring ini_val;
	ExplicitCommandListSection *command_list_section;

	RunExplicitCommandList() :
		command_list_section(NULL)
	{}

	void run(HackerDevice*, HackerContext*, ID3D11Device*, ID3D11DeviceContext*, CommandListState*) override;
};

class CustomShader
{
public:
	bool vs_override, hs_override, ds_override, gs_override, ps_override, cs_override;
	ID3D11VertexShader *vs;
	ID3D11HullShader *hs;
	ID3D11DomainShader *ds;
	ID3D11GeometryShader *gs;
	ID3D11PixelShader *ps;
	ID3D11ComputeShader *cs;

	ID3DBlob *vs_bytecode, *hs_bytecode, *ds_bytecode;
	ID3DBlob *gs_bytecode, *ps_bytecode, *cs_bytecode;

	int blend_override;
	D3D11_BLEND_DESC blend_desc;
	ID3D11BlendState *blend_state;
	FLOAT blend_factor[4];
	UINT blend_sample_mask;

	int rs_override;
	D3D11_RASTERIZER_DESC rs_desc;
	ID3D11RasterizerState *rs_state;

	D3D11_PRIMITIVE_TOPOLOGY topology;

	CommandList command_list;
	CommandList post_command_list;

	bool substantiated;

	int max_executions_per_frame;
	unsigned frame_no;
	int executions_this_frame;

	CustomShader();
	~CustomShader();

	bool compile(char type, wchar_t *filename, const wstring *wname);
	void substantiate(ID3D11Device *mOrigDevice);
};

typedef std::unordered_map<std::wstring, class CustomShader> CustomShaders;
extern CustomShaders customShaders;

class RunCustomShaderCommand : public CommandListCommand {
public:
	wstring ini_val;
	CustomShader *custom_shader;

	RunCustomShaderCommand() :
		custom_shader(NULL)
	{}

	void run(HackerDevice*, HackerContext*, ID3D11Device*, ID3D11DeviceContext*, CommandListState*) override;
};

enum class DrawCommandType {
	INVALID,
	DRAW,
	DRAW_AUTO,
	DRAW_INDEXED,
	DRAW_INDEXED_INSTANCED,
	DRAW_INDEXED_INSTANCED_INDIRECT,
	DRAW_INSTANCED,
	DRAW_INSTANCED_INDIRECT,
	DISPATCH,
	DISPATCH_INDIRECT,
	FROM_CALLER,
};

class DrawCommand : public CommandListCommand {
public:
	DrawCommandType type;

	UINT args[5];

	DrawCommand::DrawCommand() :
		type(DrawCommandType::INVALID)
	{}

	void run(HackerDevice*, HackerContext*, ID3D11Device*, ID3D11DeviceContext*, CommandListState*) override;
};

enum class ParamOverrideType {
	INVALID,
	VALUE,
	RT_WIDTH,
	RT_HEIGHT,
	RES_WIDTH,
	RES_HEIGHT,
	TEXTURE,	// Needs shader type and slot number specified in
			// [ShaderOverride]. [TextureOverride] sections can
			// specify filter_index=N to define the value passed in
			// here. Special values for no [TextureOverride]
			// section = 0.0, or [TextureOverride] with no
			// filter_index = 1.0
	VERTEX_COUNT,
	INDEX_COUNT,
	INSTANCE_COUNT,
	CURSOR_VISIBLE,  // If we later suppress this we may need an 'intent to show'
	CURSOR_SCREEN_X, // This may not be the best units for windowed games, etc.
	CURSOR_SCREEN_Y, // and not sure about multi-monitor, but it will do for now.
	// TODO:
	// DEPTH_ACTIVE
	// etc.
};
static EnumName_t<const wchar_t *, ParamOverrideType> ParamOverrideTypeNames[] = {
	{L"rt_width", ParamOverrideType::RT_WIDTH},
	{L"rt_height", ParamOverrideType::RT_HEIGHT},
	{L"res_width", ParamOverrideType::RES_WIDTH},
	{L"res_height", ParamOverrideType::RES_HEIGHT},
	{L"vertex_count", ParamOverrideType::VERTEX_COUNT},
	{L"index_count", ParamOverrideType::INDEX_COUNT},
	{L"instance_count", ParamOverrideType::INSTANCE_COUNT},
	{L"cursor_showing", ParamOverrideType::CURSOR_VISIBLE},
	{L"cursor_screen_x", ParamOverrideType::CURSOR_SCREEN_X},
	{L"cursor_screen_y", ParamOverrideType::CURSOR_SCREEN_Y},
	{NULL, ParamOverrideType::INVALID} // End of list marker
};
class ParamOverride : public CommandListCommand {
public:
	wstring ini_key, ini_val;

	int param_idx;
	float DirectX::XMFLOAT4::*param_component;

	ParamOverrideType type;
	float val;

	// For texture filters:
	wchar_t shader_type;
	unsigned texture_slot;

	// TODO: Ability to override value until:
	// a) From now on
	// b) Single draw call only
	// c) Until end of this frame (e.g. mark when post processing starts)
	// d) Until end of next frame (e.g. for scene detection)
	// Since the duration of the convergence and separation settings are
	// not currently consistent between [ShaderOverride] and [Key] sections
	// we could also make this apply to them to make it consistent, but
	// still allow for the existing behaviour for the fixes that depend on
	// it (like DG2).

	ParamOverride() :
		param_idx(-1),
		param_component(NULL),
		type(ParamOverrideType::INVALID),
		val(FLT_MAX),
		shader_type(NULL),
		texture_slot(INT_MAX)
	{}

	void run(HackerDevice*, HackerContext*, ID3D11Device*, ID3D11DeviceContext*, CommandListState*) override;
};

enum class CustomResourceType {
	INVALID,
	BUFFER,
	STRUCTURED_BUFFER,
	RAW_BUFFER,
	TEXTURE1D,
	TEXTURE2D,
	TEXTURE3D,
	CUBE,
};
static EnumName_t<const wchar_t *, CustomResourceType> CustomResourceTypeNames[] = {
	// Use the same names as HLSL here since they are what shaderhackers
	// will see in the shaders, even if some of these have no distinction
	// from our point of view, or are just a misc flag:
	{L"Buffer", CustomResourceType::BUFFER},
	{L"StructuredBuffer", CustomResourceType::STRUCTURED_BUFFER},
	{L"AppendStructuredBuffer", CustomResourceType::STRUCTURED_BUFFER},
	{L"ConsumeStructuredBuffer", CustomResourceType::STRUCTURED_BUFFER},
	{L"ByteAddressBuffer", CustomResourceType::RAW_BUFFER},
	{L"Texture1D", CustomResourceType::TEXTURE1D},
	{L"Texture2D", CustomResourceType::TEXTURE2D},
	{L"Texture3D", CustomResourceType::TEXTURE3D},
	{L"TextureCube", CustomResourceType::CUBE},
	// RW variants are identical to the above (it's the usage that counts):
	{L"RWBuffer", CustomResourceType::BUFFER},
	{L"RWStructuredBuffer", CustomResourceType::STRUCTURED_BUFFER},
	{L"RWByteAddressBuffer", CustomResourceType::RAW_BUFFER},
	{L"RWTexture1D", CustomResourceType::TEXTURE1D},
	{L"RWTexture2D", CustomResourceType::TEXTURE2D},
	{L"RWTexture3D", CustomResourceType::TEXTURE3D},

	{NULL, CustomResourceType::INVALID} // End of list marker
};
enum class CustomResourceMode {
	DEFAULT,
	AUTO,
	STEREO,
	MONO,
};
static EnumName_t<const wchar_t *, CustomResourceMode> CustomResourceModeNames[] = {
	{L"auto", CustomResourceMode::AUTO},
	{L"stereo", CustomResourceMode::STEREO},
	{L"mono", CustomResourceMode::MONO},
	{NULL, CustomResourceMode::DEFAULT} // End of list marker
};

// The bind flags are usually set automatically, but there are cases where
// these can be used to influence driver heuristics (e.g. a buffer that
// includes a render target or UAV bind flag may be stereoised), so we allow
// them to be set manually as well. If specified these will *replace* the bind
// flags 3DMigoto sets automatically - if you use these, you presumably know
// what you are doing. This enumeration is essentially the same as
// D3D11_BIND_FLAG, but this allows us to use parse_enum_option_string.
enum class CustomResourceBindFlags {
	INVALID         = 0x00000000,
	VERTEX_BUFFER   = 0x00000001,
	INDEX_BUFFER    = 0x00000002,
	CONSTANT_BUFFER = 0x00000004,
	SHADER_RESOURCE = 0x00000008,
	STREAM_OUTPUT   = 0x00000010,
	RENDER_TARGET   = 0x00000020,
	DEPTH_STENCIL   = 0x00000040,
	UNORDERED_ACCESS= 0x00000080,
	DECODER         = 0x00000200,
	VIDEO_ENCODER   = 0x00000400,
};
SENSIBLE_ENUM(CustomResourceBindFlags);
static EnumName_t<wchar_t *, CustomResourceBindFlags> CustomResourceBindFlagNames[] = {
	{L"vertex_buffer", CustomResourceBindFlags::VERTEX_BUFFER},
	{L"index_buffer", CustomResourceBindFlags::INDEX_BUFFER},
	{L"constant_buffer", CustomResourceBindFlags::CONSTANT_BUFFER},
	{L"shader_resource", CustomResourceBindFlags::SHADER_RESOURCE},
	{L"stream_output", CustomResourceBindFlags::STREAM_OUTPUT},
	{L"render_target", CustomResourceBindFlags::RENDER_TARGET},
	{L"depth_stencil", CustomResourceBindFlags::DEPTH_STENCIL},
	{L"unordered_access", CustomResourceBindFlags::UNORDERED_ACCESS},
	{L"decoder", CustomResourceBindFlags::DECODER},
	{L"video_encoder", CustomResourceBindFlags::VIDEO_ENCODER},
	{NULL, CustomResourceBindFlags::INVALID} // End of list marker
};

class CustomResource
{
public:
	ID3D11Resource *resource;
	ID3D11View *view;
	bool is_null;

	D3D11_BIND_FLAG bind_flags;

	UINT stride;
	UINT offset;
	UINT buf_size;
	DXGI_FORMAT format;

	int max_copies_per_frame;
	unsigned frame_no;
	int copies_this_frame;

	wstring filename;
	bool substantiated;

	// Used to override description when copying or synthesise resources
	// from scratch:
	CustomResourceType override_type;
	CustomResourceMode override_mode;
	CustomResourceBindFlags override_bind_flags;
	DXGI_FORMAT override_format;
	int override_width;
	int override_height;
	int override_depth;
	int override_mips;
	int override_array;
	int override_msaa;
	int override_msaa_quality;
	int override_byte_width;
	int override_stride;

	float width_multiply;
	float height_multiply;

	CustomResource();
	~CustomResource();

	void Substantiate(ID3D11Device *mOrigDevice, StereoHandle mStereoHandle);
	bool OverrideSurfaceCreationMode(StereoHandle mStereoHandle, NVAPI_STEREO_SURFACECREATEMODE *orig_mode);
	void OverrideBufferDesc(D3D11_BUFFER_DESC *desc);
	void OverrideTexDesc(D3D11_TEXTURE1D_DESC *desc);
	void OverrideTexDesc(D3D11_TEXTURE2D_DESC *desc);
	void OverrideTexDesc(D3D11_TEXTURE3D_DESC *desc);
	void OverrideOutOfBandInfo(DXGI_FORMAT *format, UINT *stride);

private:
	void LoadFromFile(ID3D11Device *mOrigDevice);
	void LoadBufferFromFile(ID3D11Device *mOrigDevice);
	void SubstantiateBuffer(ID3D11Device *mOrigDevice, void **buf, DWORD size);
	void SubstantiateTexture1D(ID3D11Device *mOrigDevice);
	void SubstantiateTexture2D(ID3D11Device *mOrigDevice);
	void SubstantiateTexture3D(ID3D11Device *mOrigDevice);
};

typedef std::unordered_map<std::wstring, class CustomResource> CustomResources;
extern CustomResources customResources;

enum class ResourceCopyTargetType {
	INVALID,
	EMPTY,
	CONSTANT_BUFFER,
	SHADER_RESOURCE,
	// TODO: SAMPLER, // Not really a resource, but might still be useful
	VERTEX_BUFFER,
	INDEX_BUFFER,
	STREAM_OUTPUT,
	RENDER_TARGET,
	DEPTH_STENCIL_TARGET,
	UNORDERED_ACCESS_VIEW,
	CUSTOM_RESOURCE,
	STEREO_PARAMS,
	INI_PARAMS,
	SWAP_CHAIN,
};

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

	bool ParseTarget(const wchar_t *target, bool is_source);
	ID3D11Resource *GetResource(
			HackerDevice *mHackerDevice,
			ID3D11Device *mOrigDevice,
			ID3D11DeviceContext *mOrigContext,
			ID3D11View **view,
			UINT *stride,
			UINT *offset,
			DXGI_FORMAT *format,
			UINT *buf_size,
			DrawCallInfo *call_info);
	void SetResource(
			ID3D11DeviceContext *mOrigContext,
			ID3D11Resource *res,
			ID3D11View *view,
			UINT stride,
			UINT offset,
			DXGI_FORMAT format,
			UINT buf_size);
	D3D11_BIND_FLAG BindFlags();
};

enum class ResourceCopyOptions {
	INVALID         = 0,
	COPY            = 0x00000001,
	REFERENCE       = 0x00000002,
	UNLESS_NULL     = 0x00000004,
	RESOLVE_MSAA    = 0x00000008,
	STEREO          = 0x00000010,
	MONO            = 0x00000020,
	STEREO2MONO     = 0x00000040,
	COPY_DESC       = 0x00000080,
	SET_VIEWPORT    = 0x00000100,
	NO_VIEW_CACHE   = 0x00000200,

	COPY_MASK       = 0x000000c9, // Anything that implies a copy
	COPY_TYPE_MASK  = 0x000000cb, // Anything that implies a copy or a reference
	CREATEMODE_MASK = 0x00000070,
};
SENSIBLE_ENUM(ResourceCopyOptions);
static EnumName_t<wchar_t *, ResourceCopyOptions> ResourceCopyOptionNames[] = {
	{L"copy", ResourceCopyOptions::COPY},
	{L"ref", ResourceCopyOptions::REFERENCE},
	{L"reference", ResourceCopyOptions::REFERENCE},
	{L"copy_desc", ResourceCopyOptions::COPY_DESC},
	{L"copy_description", ResourceCopyOptions::COPY_DESC},
	{L"unless_null", ResourceCopyOptions::UNLESS_NULL},
	{L"stereo", ResourceCopyOptions::STEREO},
	{L"mono", ResourceCopyOptions::MONO},
	{L"stereo2mono", ResourceCopyOptions::STEREO2MONO},
	{L"set_viewport", ResourceCopyOptions::SET_VIEWPORT},
	{L"no_view_cache", ResourceCopyOptions::NO_VIEW_CACHE},

	// This one currently depends on device support for resolving the
	// given texture format (D3D11_FORMAT_SUPPORT_MULTISAMPLE_RESOLVE), and
	// currently has no fallback, so we can't rely on it - don't encourage
	// people to use it and don't document it. TODO: Implement a fallback
	// using a shader to resolve any unsupported formats.
	{L"resolve_msaa", ResourceCopyOptions::RESOLVE_MSAA},

	{NULL, ResourceCopyOptions::INVALID} // End of list marker
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


class ResourceCopyOperation : public CommandListCommand {
public:
	wstring ini_key, ini_val;

	ResourceCopyTarget src;
	ResourceCopyTarget dst;
	ResourceCopyOptions options;

	ID3D11Resource *cached_resource;
	ID3D11View *cached_view;

	// Additional intermediate resources required for certain operations
	// (TODO: add alternate cache in CustomResource to cut down on extra
	// copies when copying to a single resource from many sources)
	ID3D11Resource *stereo2mono_intermediate;

	ResourceCopyOperation();
	~ResourceCopyOperation();

	void run(HackerDevice*, HackerContext*, ID3D11Device*, ID3D11DeviceContext*, CommandListState*) override;
};


void RunCommandList(HackerDevice *mHackerDevice,
		HackerContext *mHackerContext,
		CommandList *command_list, DrawCallInfo *call_info,
		bool post);

bool ParseCommandListGeneralCommands(const wchar_t *key, wstring *val,
		CommandList *explicit_command_list,
		CommandList *pre_command_list, CommandList *post_command_list);
bool ParseCommandListIniParamOverride(const wchar_t *key, wstring *val,
		CommandList *command_list);
bool ParseCommandListResourceCopyDirective(const wchar_t *key, wstring *val,
		CommandList *command_list);
