#include "CommandList.h"

#include <DDSTextureLoader.h>
#include <WICTextureLoader.h>
#include <algorithm>
#include <sstream>
#include "HackerDevice.h"
#include "HackerContext.h"
#include "Override.h"
#include "D3D11Wrapper.h"
#include "IniHandler.h"

#include <D3DCompiler.h>

CustomResources customResources;
CustomShaders customShaders;
ExplicitCommandListSections explicitCommandListSections;

static void _RunCommandList(CommandList *command_list, CommandListState *state)
{
	CommandList::iterator i;

	if (state->recursion > MAX_COMMAND_LIST_RECURSION) {
		LogInfo("WARNING: Command list recursion limit exceeded! Circular reference?\n");
		return;
	}

	state->recursion++;
	for (i = command_list->begin(); i < command_list->end() && !state->aborted; i++) {
		(*i)->run(state);
	}
	state->recursion--;
}

static void CommandListFlushState(CommandListState *state)
{
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	HRESULT hr;

	if (state->update_params) {
		hr = state->mOrigContext1->Map(state->mHackerDevice->mIniTexture, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
		if (FAILED(hr)) {
			LogInfo("CommandListFlushState: Map failed\n");
			return;
		}
		memcpy(mappedResource.pData, &G->iniParams, sizeof(G->iniParams));
		state->mOrigContext1->Unmap(state->mHackerDevice->mIniTexture, 0);
		state->update_params = false;
	}
}

static void RunCommandListComplete(HackerDevice *mHackerDevice,
		HackerContext *mHackerContext,
		CommandList *command_list,
		DrawCallInfo *call_info,
		ID3D11Resource *resource,
		ID3D11View *view,
		bool post)
{
	CommandListState state;
	state.mHackerDevice = mHackerDevice;
	state.mHackerContext = mHackerContext;
	state.mOrigDevice1 = mHackerDevice->GetPassThroughOrigDevice1();
	state.mOrigContext1 = mHackerContext->GetPassThroughOrigContext1();

	state.call_info = call_info;
	state.resource = resource;
	state.view = view;
	state.post = post;

	_RunCommandList(command_list, &state);
	CommandListFlushState(&state);
}

void RunCommandList(HackerDevice *mHackerDevice,
		HackerContext *mHackerContext,
		CommandList *command_list,
		DrawCallInfo *call_info,
		bool post)
{
	ID3D11Resource *resource = NULL;
	if (call_info)
		resource = call_info->indirect_buffer;

	RunCommandListComplete(mHackerDevice, mHackerContext, command_list,
			call_info, resource, NULL, post);
}

void RunResourceCommandList(HackerDevice *mHackerDevice,
		HackerContext *mHackerContext,
		CommandList *command_list,
		ID3D11Resource *resource,
		bool post)
{
	RunCommandListComplete(mHackerDevice, mHackerContext, command_list,
			NULL, resource, NULL, post);
}

void RunViewCommandList(HackerDevice *mHackerDevice,
		HackerContext *mHackerContext,
		CommandList *command_list,
		ID3D11View *view,
		bool post)
{
	ID3D11Resource *res = NULL;

	if (view)
		view->GetResource(&res);

	RunCommandListComplete(mHackerDevice, mHackerContext, command_list,
			NULL, res, view, post);

	if (res)
		res->Release();
}

static bool AddCommandToList(CommandListCommand *command,
		CommandList *explicit_command_list,
		CommandList *sensible_command_list,
		CommandList *pre_command_list,
		CommandList *post_command_list)
{
	if (explicit_command_list) {
		// User explicitly specified "pre" or "post", so only add the
		// command to that list
		explicit_command_list->push_back(std::shared_ptr<CommandListCommand>(command));
	} else if (sensible_command_list) {
		// User did not specify which command list to add it to, but
		// the command they specified has a sensible default, so add it
		// to that list:
		sensible_command_list->push_back(std::shared_ptr<CommandListCommand>(command));
	} else {
		// The command's default is to add it to both lists (e.g. the
		// checktextureoverride directive will call command lists in
		// another ini section with both pre and post lists, so the
		// principal of least unexpected behaviour says we add it to
		// both so that both those command lists will be called)
		//
		// Using a std::shared_ptr here to handle adding the same
		// pointer to two lists and have it garbage collected only once
		// both are destroyed:
		std::shared_ptr<CommandListCommand> p(command);
		pre_command_list->push_back(p);
		if (post_command_list)
			post_command_list->push_back(p);
	}

	return true;
}

static bool ParseCheckTextureOverride(const wchar_t *section,
		const wchar_t *key, wstring *val,
		CommandList *explicit_command_list,
		CommandList *pre_command_list,
		CommandList *post_command_list)
{
	int ret;

	CheckTextureOverrideCommand *operation = new CheckTextureOverrideCommand();

	// Parse value as consistent with texture filtering and resource copying
	ret = operation->target.ParseTarget(val->c_str(), true);
	if (ret) {
		operation->ini_line = L"[" + wstring(section) + L"] " + wstring(key) + L" = " + *val;
		return AddCommandToList(operation, explicit_command_list, NULL, pre_command_list, post_command_list);
	}

	delete operation;
	return false;
}

static bool ParseResetPerFrameLimits(const wchar_t *section,
		const wchar_t *key, wstring *val,
		CommandList *explicit_command_list,
		CommandList *pre_command_list,
		CommandList *post_command_list)
{
	CustomResources::iterator res;
	CustomShaders::iterator shader;

	ResetPerFrameLimitsCommand *operation = new ResetPerFrameLimitsCommand();

	if (!wcsncmp(val->c_str(), L"resource", 8)) {
		wstring resource_id(val->c_str());

		res = customResources.find(resource_id);
		if (res == customResources.end())
			goto bail;

		operation->resource = &res->second;
	}

	if (!wcsncmp(val->c_str(), L"customshader", 12) || !wcsncmp(val->c_str(), L"builtincustomshader", 19)) {
		wstring shader_id(val->c_str());

		shader = customShaders.find(shader_id);
		if (shader == customShaders.end())
			goto bail;

		operation->shader = &shader->second;
	}

	operation->ini_line = L"[" + wstring(section) + L"] " + wstring(key) + L" = " + *val;
	return AddCommandToList(operation, explicit_command_list, pre_command_list, NULL, NULL);

bail:
	delete operation;
	return false;
}

static bool ParseClearView(const wchar_t *section,
		const wchar_t *key, wstring *val,
		CommandList *explicit_command_list,
		CommandList *pre_command_list,
		CommandList *post_command_list)
{
	CustomResources::iterator res;
	CustomShaders::iterator shader;
	wistringstream token_stream(*val);
	wstring token;
	int ret, len1;
	int idx = 0;
	unsigned uval;
	float fval;

	ClearViewCommand *operation = new ClearViewCommand();

	while (getline(token_stream, token, L' ')) {
		if (operation->target.type == ResourceCopyTargetType::INVALID) {
			ret = operation->target.ParseTarget(token.c_str(), true);
			if (ret)
				continue;
		}

		if (idx < 4) {
			// Try parsing value as a hex string. If this matches
			// we know the user didn't intend to use floats. This
			// is necessary to allow integer values that require
			// more than 24 significant bits to be used, which
			// would be lost if we only parsed the string as a
			// float, e.g. 0xffffffff cannot be stored as a float
			ret = swscanf_s(token.c_str(), L"0x%x%n", &uval, &len1);
			if (ret != 0 && ret != EOF && len1 == token.length()) {
				operation->uval[idx] = uval;
				operation->fval[idx] = *(float*)&uval;
				operation->clear_uav_uint = true;
				idx++;
				continue;
			}

			// On the other hand, if parsing the value as a float
			// matches the user might have intended it to be a
			// float or an integer. We will assume they want floats
			// by default, but store it in both arrays in case we
			// later determine that we need to use an integer clear.
			ret = swscanf_s(token.c_str(), L"%f%n", &fval, &len1);
			if (ret != 0 && ret != EOF && len1 == token.length()) {
				operation->fval[idx] = fval;
				operation->uval[idx] = (UINT)fval;
				idx++;
				continue;
			}
		}
		if (!wcscmp(token.c_str(), L"int")) {
			operation->clear_uav_uint = true;
			continue;
		}
		if (!wcscmp(token.c_str(), L"depth")) {
			operation->clear_depth = true;
			continue;
		}
		if (!wcscmp(token.c_str(), L"stencil")) {
			operation->clear_stencil = true;
			continue;
		}

		goto bail;
	}

	if (operation->target.type == ResourceCopyTargetType::INVALID)
		goto bail;

	// Use the first value specified as the depth value when clearing a
	// DSV, and the second as the stencil value, unless we are only
	// clearing the stencil side, in which case use the first:
	operation->dsv_depth = operation->fval[0];
	operation->dsv_stencil = operation->uval[1];
	if (operation->clear_stencil && !operation->clear_depth)
		operation->dsv_stencil = operation->uval[0];

	// Propagate the final specified value to the remaining channels. This
	// allows a single value to be specified to clear all channels in RTVs
	// and UAVs. Note that this is done after noting the DSV values because
	// we never want to propagate the depth value to the stencil value:
	for (idx++; idx < 4; idx++) {
		operation->uval[idx] = operation->uval[idx - 1];
		operation->fval[idx] = operation->fval[idx - 1];
	}

	operation->ini_line = L"[" + wstring(section) + L"] " + wstring(key) + L" = " + *val;
	return AddCommandToList(operation, explicit_command_list, pre_command_list, NULL, NULL);

bail:
	delete operation;
	return false;
}


static bool ParseRunShader(const wchar_t *section,
		const wchar_t *key, wstring *val,
		CommandList *explicit_command_list,
		CommandList *pre_command_list,
		CommandList *post_command_list)
{
	RunCustomShaderCommand *operation = new RunCustomShaderCommand();
	CustomShaders::iterator shader;

	// Value should already have been transformed to lower case from
	// ParseCommandList, so our keys will be consistent in the
	// unordered_map:
	wstring shader_id(val->c_str());

	shader = customShaders.find(shader_id);
	if (shader == customShaders.end())
		goto bail;

	operation->ini_line = L"[" + wstring(section) + L"] " + wstring(key) + L" = " + *val;
	operation->custom_shader = &shader->second;
	return AddCommandToList(operation, explicit_command_list, pre_command_list, NULL, NULL);

bail:
	delete operation;
	return false;
}

static bool ParseRunExplicitCommandList(const wchar_t *section,
		const wchar_t *key, wstring *val,
		CommandList *explicit_command_list,
		CommandList *pre_command_list,
		CommandList *post_command_list)
{
	RunExplicitCommandList *operation = new RunExplicitCommandList();
	ExplicitCommandListSections::iterator shader;

	// Value should already have been transformed to lower case from
	// ParseCommandList, so our keys will be consistent in the
	// unordered_map:
	wstring section_id(val->c_str());

	shader = explicitCommandListSections.find(section_id);
	if (shader == explicitCommandListSections.end())
		goto bail;

	operation->ini_line = L"[" + wstring(section) + L"] " + wstring(key) + L" = " + *val;
	operation->command_list_section = &shader->second;
	// This function is nearly identical to ParseRunShader, but in case we
	// later refactor these together note that here we do not specify a
	// sensible command list, so it will be added to both pre and post
	// command lists:
	return AddCommandToList(operation, explicit_command_list, NULL, pre_command_list, post_command_list);

bail:
	delete operation;
	return false;
}

static bool ParsePreset(const wchar_t *section,
		const wchar_t *key, wstring *val,
		CommandList *explicit_command_list,
		CommandList *pre_command_list,
		CommandList *post_command_list)
{
	PresetCommand *operation = new PresetCommand();

	PresetOverrideMap::iterator i;

	// Value should already have been transformed to lower case from
	// ParseCommandList, so our keys will be consistent in the
	// unordered_map:
	wstring preset_id(val->c_str());

	// The original preset code did not accept the "Preset" prefix on the
	// prefix command, as in it would only accept 'preset = Foo', not
	// 'preset = PresetFoo'. While I agree that the later is redundant
	// since the word preset now appears twice, it is more consistent with
	// the way we have referenced other sections in the command list (ps-t0
	// = ResourceBar, run = CustomShaderBaz, etc), and it makes it easier
	// to search for 'PresetFoo' to find both where it is used and where it
	// is referenced, so it is good to support here... but for backwards
	// compatibility and less redundancy for those that prefer not to say
	// "preset" twice we support both ways.

	// First, try without the prefix:
	i = presetOverrides.find(preset_id);
	if (i == presetOverrides.end()) {
		// If the 'Preset' prefix was specified, strip it and try again:
		if (!wcsncmp(val->c_str(), L"preset", 6)) {
			preset_id = val->c_str() + 6;

			i = presetOverrides.find(preset_id);
			if (i == presetOverrides.end())
				goto bail;
		}
	}

	operation->ini_line = L"[" + wstring(section) + L"] " + wstring(key) + L" = " + *val;
	operation->preset = &i->second;

	return AddCommandToList(operation, explicit_command_list, pre_command_list, NULL, NULL);

bail:
	delete operation;
	return false;
}

static bool ParseDrawCommand(const wchar_t *section,
		const wchar_t *key, wstring *val,
		CommandList *explicit_command_list,
		CommandList *pre_command_list,
		CommandList *post_command_list)
{
	DrawCommand *operation = new DrawCommand();
	int nargs, end = 0;

	if (!wcscmp(key, L"draw")) {
		if (!wcscmp(val->c_str(), L"from_caller")) {
			operation->type = DrawCommandType::FROM_CALLER;
			end = (int)val->length();
		} else {
			operation->type = DrawCommandType::DRAW;
			nargs = swscanf_s(val->c_str(), L"%u, %u%n", &operation->args[0], &operation->args[1], &end);
			if (nargs != 2)
				goto bail;
		}
	} else if (!wcscmp(key, L"drawauto")) {
		operation->type = DrawCommandType::DRAW_AUTO;
	} else if (!wcscmp(key, L"drawindexed")) {
		operation->type = DrawCommandType::DRAW_INDEXED;
		nargs = swscanf_s(val->c_str(), L"%u, %u, %i%n", &operation->args[0], &operation->args[1], (INT*)&operation->args[2], &end);
		if (nargs != 3)
			goto bail;
	} else if (!wcscmp(key, L"drawindexedinstanced")) {
		operation->type = DrawCommandType::DRAW_INDEXED_INSTANCED;
		nargs = swscanf_s(val->c_str(), L"%u, %u, %u, %i, %u%n",
				&operation->args[0], &operation->args[1], &operation->args[2], (INT*)&operation->args[3], &operation->args[4], &end);
		if (nargs != 5)
			goto bail;
	} else if (!wcscmp(key, L"drawinstanced")) {
		operation->type = DrawCommandType::DRAW_INSTANCED;
		nargs = swscanf_s(val->c_str(), L"%u, %u, %u, %u%n",
				&operation->args[0], &operation->args[1], &operation->args[2], &operation->args[3], &end);
		if (nargs != 5)
			goto bail;
	} else if (!wcscmp(key, L"dispatch")) {
		operation->type = DrawCommandType::DISPATCH;
		nargs = swscanf_s(val->c_str(), L"%u, %u, %u%n", &operation->args[0], &operation->args[1], &operation->args[2], &end);
		if (nargs != 3)
			goto bail;
	}

	// TODO: } else if (!wcscmp(key, L"drawindexedinstancedindirect")) {
	// TODO: 	operation->type = DrawCommandType::DRAW_INDEXED_INSTANCED_INDIRECT;
	// TODO: } else if (!wcscmp(key, L"drawinstancedindirect")) {
	// TODO: 	operation->type = DrawCommandType::DRAW_INSTANCED_INDIRECT;
	// TODO: } else if (!wcscmp(key, L"dispatchindirect")) {
	// TODO: 	operation->type = DrawCommandType::DISPATCH_INDIRECT;
	// TODO: }


	if (operation->type == DrawCommandType::INVALID)
		goto bail;

	if (end != val->length())
		goto bail;

	operation->ini_section = section;
	return AddCommandToList(operation, explicit_command_list, pre_command_list, NULL, NULL);

bail:
	delete operation;
	return false;
}

static bool ParsePerDrawStereoOverride(const wchar_t *section,
		const wchar_t *key, wstring *val,
		CommandList *explicit_command_list,
		CommandList *pre_command_list,
		CommandList *post_command_list,
		bool is_separation)
{
	PerDrawStereoOverrideCommand *operation = NULL;
	int ret, len1;

	if (is_separation)
		operation = new PerDrawSeparationOverrideCommand(!explicit_command_list);
	else
		operation = new PerDrawConvergenceOverrideCommand(!explicit_command_list);

	// Try parsing value as a float
	ret = swscanf_s(val->c_str(), L"%f%n", &operation->val, &len1);
	if (ret != 0 && ret != EOF && len1 == val->length())
		goto success;

	// Try parsing value as a resource target for staging auto-convergence
	if (operation->staging_op.src.ParseTarget(val->c_str(), true)) {
		operation->staging_type = true;
		goto success;
	}

	goto bail;

success:
	// Add to both command lists by default - the pre command list will set
	// the value, and the post command list will restore the original. If
	// an explicit command list is specified then the value will only be
	// set, not restored (regardless of whether that is pre or post)
	operation->ini_line = L"[" + wstring(section) + L"] " + wstring(key) + L" = " + *val;
	return AddCommandToList(operation, explicit_command_list, NULL, pre_command_list, post_command_list);

bail:
	delete operation;
	return false;
}

bool ParseCommandListGeneralCommands(const wchar_t *section,
		const wchar_t *key, wstring *val,
		CommandList *explicit_command_list,
		CommandList *pre_command_list, CommandList *post_command_list)
{
	if (!wcscmp(key, L"checktextureoverride"))
		return ParseCheckTextureOverride(section, key, val, explicit_command_list, pre_command_list, post_command_list);

	if (!wcscmp(key, L"run")) {
		if (!wcsncmp(val->c_str(), L"customshader", 12) || !wcsncmp(val->c_str(), L"builtincustomshader", 19))
			return ParseRunShader(section, key, val, explicit_command_list, pre_command_list, post_command_list);

		if (!wcsncmp(val->c_str(), L"commandlist", 11) || !wcsncmp(val->c_str(), L"builtincommandlist", 18))
			return ParseRunExplicitCommandList(section, key, val, explicit_command_list, pre_command_list, post_command_list);
	}

	if (!wcscmp(key, L"preset"))
		return ParsePreset(section, key, val, explicit_command_list, pre_command_list, post_command_list);

	if (!wcscmp(key, L"handling")) {
		// skip only makes sense in pre command lists, since it needs
		// to run before the original draw call:
		if (!wcscmp(val->c_str(), L"skip"))
			return AddCommandToList(new SkipCommand(section), explicit_command_list, pre_command_list, NULL, NULL);

		// abort defaults to both command lists, to abort command list
		// execution both before and after the draw call:
		if (!wcscmp(val->c_str(), L"abort"))
			return AddCommandToList(new AbortCommand(section), explicit_command_list, NULL, pre_command_list, post_command_list);
	}

	if (!wcscmp(key, L"reset_per_frame_limits"))
		return ParseResetPerFrameLimits(section, key, val, explicit_command_list, pre_command_list, post_command_list);

	if (!wcscmp(key, L"clear"))
		return ParseClearView(section, key, val, explicit_command_list, pre_command_list, post_command_list);

	if (!wcscmp(key, L"separation"))
		return ParsePerDrawStereoOverride(section, key, val, explicit_command_list, pre_command_list, post_command_list, true);

	if (!wcscmp(key, L"convergence"))
		return ParsePerDrawStereoOverride(section, key, val, explicit_command_list, pre_command_list, post_command_list, false);

	return ParseDrawCommand(section, key, val, explicit_command_list, pre_command_list, post_command_list);
}

void CheckTextureOverrideCommand::run(CommandListState *state)
{
	state->mHackerContext->FrameAnalysisLog("3DMigoto %S\n", ini_line.c_str());

	TextureOverride *override = target.FindTextureOverride(state, NULL);

	if (!override)
		return;

	if (state->post)
		_RunCommandList(&override->post_command_list, state);
	else
		_RunCommandList(&override->command_list, state);
}

ClearViewCommand::ClearViewCommand() :
	dsv_depth(0.0),
	dsv_stencil(0),
	clear_depth(false),
	clear_stencil(false),
	clear_uav_uint(false)
{
	memset(fval, 0, sizeof(fval));
	memset(uval, 0, sizeof(uval));
}

static bool UAVSupportsFloatClear(ID3D11UnorderedAccessView *uav)
{
	D3D11_UNORDERED_ACCESS_VIEW_DESC desc;

	// UAVs can be cleared as a float if their format is a float, snorm or
	// unorm, otherwise the clear will fail silently. I didn't include
	// partially typed or block compressed formats in the below list
	// because I doubt they would work (but haven't checked).

	uav->GetDesc(&desc);

	switch (desc.Format) {
		case DXGI_FORMAT_UNKNOWN:
			// Common case
			return false;
		case DXGI_FORMAT_R32G32B32A32_FLOAT:
		case DXGI_FORMAT_R32G32B32_FLOAT:
		case DXGI_FORMAT_R16G16B16A16_FLOAT:
		case DXGI_FORMAT_R16G16B16A16_UNORM:
		case DXGI_FORMAT_R16G16B16A16_SNORM:
		case DXGI_FORMAT_R32G32_FLOAT:
		case DXGI_FORMAT_R10G10B10A2_UNORM:
		case DXGI_FORMAT_R11G11B10_FLOAT:
		case DXGI_FORMAT_R8G8B8A8_UNORM:
		case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
		case DXGI_FORMAT_R8G8B8A8_SNORM:
		case DXGI_FORMAT_R16G16_FLOAT:
		case DXGI_FORMAT_R16G16_UNORM:
		case DXGI_FORMAT_R16G16_SNORM:
		case DXGI_FORMAT_D32_FLOAT:
		case DXGI_FORMAT_R32_FLOAT:
		case DXGI_FORMAT_R8G8_UNORM:
		case DXGI_FORMAT_R8G8_SNORM:
		case DXGI_FORMAT_R16_FLOAT:
		case DXGI_FORMAT_D16_UNORM:
		case DXGI_FORMAT_R16_UNORM:
		case DXGI_FORMAT_R16_SNORM:
		case DXGI_FORMAT_R8_UNORM:
		case DXGI_FORMAT_R8_SNORM:
		case DXGI_FORMAT_A8_UNORM:
		case DXGI_FORMAT_R1_UNORM:
		case DXGI_FORMAT_R8G8_B8G8_UNORM:
		case DXGI_FORMAT_G8R8_G8B8_UNORM:
		case DXGI_FORMAT_B5G6R5_UNORM:
		case DXGI_FORMAT_B5G5R5A1_UNORM:
		case DXGI_FORMAT_B8G8R8A8_UNORM:
		case DXGI_FORMAT_B8G8R8X8_UNORM:
		case DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM:
		case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
		case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
		case DXGI_FORMAT_B4G4R4A4_UNORM:
			return true;
		default:
			return false;
	}
}

void ResetPerFrameLimitsCommand::run(CommandListState *state)
{
	state->mHackerContext->FrameAnalysisLog("3DMigoto %S\n", ini_line.c_str());

	if (shader)
		shader->executions_this_frame = 0;

	if (resource)
		resource->copies_this_frame = 0;
}

void PresetCommand::run(CommandListState *state)
{
	state->mHackerContext->FrameAnalysisLog("3DMigoto %S\n", ini_line.c_str());

	preset->Activate(state->mHackerDevice);
}

void DrawCommand::run(CommandListState *state)
{
	HackerContext *mHackerContext = state->mHackerContext;
	ID3D11DeviceContext *mOrigContext1 = state->mOrigContext1;

	// Ensure IniParams are visible:
	CommandListFlushState(state);

	switch (type) {
		case DrawCommandType::DRAW:
			mHackerContext->FrameAnalysisLog("3DMigoto [%S] Draw(%u, %u)\n", ini_section.c_str(), args[0], args[1]);
			mOrigContext1->Draw(args[0], args[1]);
			break;
		case DrawCommandType::DRAW_AUTO:
			mHackerContext->FrameAnalysisLog("3DMigoto [%S] DrawAuto()\n", ini_section.c_str());
			mOrigContext1->DrawAuto();
			break;
		case DrawCommandType::DRAW_INDEXED:
			mHackerContext->FrameAnalysisLog("3DMigoto [%S] DrawIndexed(%u, %u, %i)\n", ini_section.c_str(), args[0], args[1], (INT)args[2]);
			mOrigContext1->DrawIndexed(args[0], args[1], (INT)args[2]);
			break;
		case DrawCommandType::DRAW_INDEXED_INSTANCED:
			mHackerContext->FrameAnalysisLog("3DMigoto [%S] DrawIndexedInstanced(%u, %u, %u, %i, %u)\n", ini_section.c_str(), args[0], args[1], args[2], (INT)args[3], args[4]);
			mOrigContext1->DrawIndexedInstanced(args[0], args[1], args[2], (INT)args[3], args[4]);
			break;
		// TODO: case DrawCommandType::DRAW_INDEXED_INSTANCED_INDIRECT:
		// TODO: 	break;
		case DrawCommandType::DRAW_INSTANCED:
			mHackerContext->FrameAnalysisLog("3DMigoto [%S] DrawInstanced(%u, %u, %u, %u)\n", ini_section.c_str(), args[0], args[1], args[2], args[3]);
			mOrigContext1->DrawInstanced(args[0], args[1], args[2], args[3]);
			break;
		// TODO: case DrawCommandType::DRAW_INSTANCED_INDIRECT:
		// TODO: 	break;
		case DrawCommandType::DISPATCH:
			mHackerContext->FrameAnalysisLog("3DMigoto [%S] Dispatch(%u, %u, %u)\n", ini_section.c_str(), args[0], args[1], args[2]);
			mOrigContext1->Dispatch(args[0], args[1], args[2]);
			break;
		// TODO: case DrawCommandType::DISPATCH_INDIRECT:
		// TODO: 	break;
		case DrawCommandType::FROM_CALLER:
			DrawCallInfo *info = state->call_info;
			if (!info) {
				mHackerContext->FrameAnalysisLog("3DMigoto [%S] Draw = from_caller -> NO ACTIVE DRAW CALL\n", ini_section.c_str());
				break;
			}
			if (info->hunting_skip) {
				mHackerContext->FrameAnalysisLog("3DMigoto [%S] Draw = from_caller -> SKIPPED DUE TO HUNTING\n", ini_section.c_str());
				break;
			}
			if (info->InstanceCount) {
				if (info->IndexCount) {
					mHackerContext->FrameAnalysisLog("3DMigoto [%S] Draw = from_caller -> DrawIndexedInstanced(%u, %u, %u, %i, %u)\n", ini_section.c_str(), info->IndexCount, info->InstanceCount, info->FirstIndex, info->FirstVertex, info->FirstInstance);
					mOrigContext1->DrawIndexedInstanced(info->IndexCount, info->InstanceCount, info->FirstIndex, info->FirstVertex, info->FirstInstance);
				} else {
					mHackerContext->FrameAnalysisLog("3DMigoto [%S] Draw = from_caller -> DrawInstanced(%u, %u, %u, %u)\n", ini_section.c_str(), info->VertexCount, info->InstanceCount, info->FirstVertex, info->FirstInstance);
					mOrigContext1->DrawInstanced(info->VertexCount, info->InstanceCount, info->FirstVertex, info->FirstInstance);
				}
			} else if (info->IndexCount) {
				mHackerContext->FrameAnalysisLog("3DMigoto [%S] Draw = from_caller -> DrawIndexed(%u, %u, %i)\n", ini_section.c_str(), info->IndexCount, info->FirstIndex, info->FirstVertex);
				mOrigContext1->DrawIndexed(info->IndexCount, info->FirstIndex, info->FirstVertex);
			} else if (info->VertexCount) {
				mHackerContext->FrameAnalysisLog("3DMigoto [%S] Draw = from_caller -> Draw(%u, %u)\n", ini_section.c_str(), info->VertexCount, info->FirstVertex);
				mOrigContext1->Draw(info->VertexCount, info->FirstVertex);
			} else if (info->indirect_buffer) {
				if (info->DrawInstancedIndirect) {
					mHackerContext->FrameAnalysisLog("3DMigoto [%S] Draw = from_caller -> DrawInstancedIndirect(0x%p, %u)\n", ini_section.c_str(), info->indirect_buffer, info->args_offset);
					mOrigContext1->DrawInstancedIndirect(info->indirect_buffer, info->args_offset);
				} else {
					mHackerContext->FrameAnalysisLog("3DMigoto [%S] Draw = from_caller -> DrawIndexedInstancedIndirect(0x%p, %u)\n", ini_section.c_str(), info->indirect_buffer, info->args_offset);
					mOrigContext1->DrawIndexedInstancedIndirect(info->indirect_buffer, info->args_offset);
				}
			} else {
				mHackerContext->FrameAnalysisLog("3DMigoto [%S] Draw = from_caller -> DrawAuto()\n", ini_section.c_str());
				mHackerContext->DrawAuto();
			}
			// TODO: dispatch = from_caller
			break;
	}
}

void SkipCommand::run(CommandListState *state)
{
	state->mHackerContext->FrameAnalysisLog("3DMigoto [%S] handling = skip\n", ini_section.c_str());

	if (state->call_info)
		state->call_info->skip = true;
	else
		state->mHackerContext->FrameAnalysisLog("3DMigoto   No active draw call to skip\n");
}

void AbortCommand::run(CommandListState *state)
{
	state->mHackerContext->FrameAnalysisLog("3DMigoto [%S] handling = abort\n", ini_section.c_str());

	state->aborted = true;
}

PerDrawStereoOverrideCommand::PerDrawStereoOverrideCommand(bool restore_on_post) :
		staging_type(false),
		val(FLT_MAX),
		saved(FLT_MAX),
		restore_on_post(restore_on_post),
		did_set_value_on_pre(false)
{}

bool PerDrawStereoOverrideCommand::update_val(CommandListState *state)
{
	D3D11_MAPPED_SUBRESOURCE mapping;
	HRESULT hr;
	float tmp;
	bool ret = false;

	if (!staging_type)
		return true;

	if (staging_op.staging) {
		hr = staging_op.map(state, &mapping);
		if (FAILED(hr)) {
			if (hr == DXGI_ERROR_WAS_STILL_DRAWING)
				state->mHackerContext->FrameAnalysisLog("3DMigoto   Transfer in progress...\n");
			else
				state->mHackerContext->FrameAnalysisLog("3DMigoto   Unknown error: 0x%x\n", hr);
			return false;
		}

		// FIXME: Check if resource is at least 4 bytes (maybe we can
		// use RowPitch, but MSDN contradicts itself so I'm not sure.
		// Otherwise we can refer to the resource description)
		tmp = ((float*)mapping.pData)[0];
		staging_op.unmap(state);

		if (isnan(tmp)) {
			state->mHackerContext->FrameAnalysisLog("3DMigoto   Disregarding NAN\n");
		} else {
			val = tmp;
			ret = true;
		}

		// To make auto-convergence as responsive as possible, we start
		// the next transfer as soon as we have retrieved the value
		// from the previous transfer. This should minimise the number
		// of frames displayed with wrong convergence on scene changes.
	}

	staging_op.staging = true;
	staging_op.run(state);
	return ret;
}

void PerDrawStereoOverrideCommand::run(CommandListState *state)
{
	state->mHackerContext->FrameAnalysisLog("3DMigoto %S\n", ini_line.c_str());

	if (!state->mHackerDevice->mStereoHandle) {
		state->mHackerContext->FrameAnalysisLog("3DMigoto   No Stereo Handle\n");
		return;
	}

	if (restore_on_post) {
		if (state->post) {
			if (!did_set_value_on_pre)
				return;
			did_set_value_on_pre = false;

			state->mHackerContext->FrameAnalysisLog("3DMigoto   Restoring %s = %f\n", stereo_param_name(), saved);
			set_stereo_value(state, saved);
		} else {
			if (!(did_set_value_on_pre = update_val(state)))
				return;

			saved = get_stereo_value(state);

			state->mHackerContext->FrameAnalysisLog("3DMigoto   Setting per-draw call %s = %f * %f = %f\n",
					stereo_param_name(), val, saved, val * saved);

			// The original ShaderOverride code multiplied the new
			// separation and convergence by the old ones, so I'm
			// doing that as well, but while that makes sense for
			// separation, I'm not really convinced it makes sense
			// for convergence. Still, the convergence override is
			// generally only useful to use convergence=0 to move
			// something to infinity, and in that case it won't
			// matter.
			set_stereo_value(state, val * saved);
		}
	} else {
		if (!update_val(state))
			return;

		state->mHackerContext->FrameAnalysisLog("3DMigoto   Setting %s = %f\n", stereo_param_name(), val);
		set_stereo_value(state, val);
	}
}

float PerDrawSeparationOverrideCommand::get_stereo_value(CommandListState *state)
{
	float ret = 0.0f;

	if (NVAPI_OK != NvAPI_Stereo_GetSeparation(state->mHackerDevice->mStereoHandle, &ret))
		state->mHackerContext->FrameAnalysisLog("3DMigoto   Stereo_GetSeparation failed\n");

	return ret;
}

void PerDrawSeparationOverrideCommand::set_stereo_value(CommandListState *state, float val)
{
	NvAPIOverride();
	if (NVAPI_OK != NvAPI_Stereo_SetSeparation(state->mHackerDevice->mStereoHandle, val))
		state->mHackerContext->FrameAnalysisLog("3DMigoto   Stereo_SetSeparation failed\n");
}

float PerDrawConvergenceOverrideCommand::get_stereo_value(CommandListState *state)
{
	float ret = 0.0f;

	if (NVAPI_OK != NvAPI_Stereo_GetConvergence(state->mHackerDevice->mStereoHandle, &ret))
		state->mHackerContext->FrameAnalysisLog("3DMigoto   Stereo_GetConvergence failed\n");

	return ret;
}

void PerDrawConvergenceOverrideCommand::set_stereo_value(CommandListState *state, float val)
{
	NvAPIOverride();
	if (NVAPI_OK != NvAPI_Stereo_SetConvergence(state->mHackerDevice->mStereoHandle, val))
		state->mHackerContext->FrameAnalysisLog("3DMigoto   Stereo_SetConvergence failed\n");
}

CustomShader::CustomShader() :
	vs_override(false), hs_override(false), ds_override(false),
	gs_override(false), ps_override(false), cs_override(false),
	vs(NULL), hs(NULL), ds(NULL), gs(NULL), ps(NULL), cs(NULL),
	vs_bytecode(NULL), hs_bytecode(NULL), ds_bytecode(NULL),
	gs_bytecode(NULL), ps_bytecode(NULL), cs_bytecode(NULL),
	blend_override(0), blend_state(NULL),
	blend_sample_mask(0xffffffff), blend_sample_mask_merge_mask(0xffffffff),
	depth_stencil_override(0), depth_stencil_state(NULL),
	stencil_ref(0), stencil_ref_mask(~0),
	rs_override(0), rs_state(NULL),
	topology(D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED),
	substantiated(false),
	max_executions_per_frame(0),
	frame_no(0),
	executions_this_frame(0),
	sampler_override(0),
	sampler_state(nullptr)
{
	int i;

	for (i = 0; i < 4; i++) {
		blend_factor[i] = 1.0f;
		blend_factor_merge_mask[i] = ~0;
	}
}

CustomShader::~CustomShader()
{
	if (vs)
		vs->Release();
	if (hs)
		hs->Release();
	if (ds)
		ds->Release();
	if (gs)
		gs->Release();
	if (ps)
		ps->Release();
	if (cs)
		cs->Release();

	if (blend_state)
		blend_state->Release();
	if (depth_stencil_state)
		depth_stencil_state->Release();
	if (rs_state)
		rs_state->Release();

	if (vs_bytecode)
		vs_bytecode->Release();
	if (hs_bytecode)
		hs_bytecode->Release();
	if (ds_bytecode)
		ds_bytecode->Release();
	if (gs_bytecode)
		gs_bytecode->Release();
	if (ps_bytecode)
		ps_bytecode->Release();
	if (cs_bytecode)
		cs_bytecode->Release();
	if (sampler_state)
		sampler_state->Release();
}

// This is similar to the other compile routines, but still distinct enough to
// get it's own function for now - TODO: Refactor out the common code
bool CustomShader::compile(char type, wchar_t *filename, const wstring *wname)
{
	wchar_t wpath[MAX_PATH];
	char apath[MAX_PATH];
	HANDLE f;
	DWORD srcDataSize, readSize;
	vector<char> srcData;
	HRESULT hr;
	char shaderModel[7];
	ID3DBlob **ppBytecode = NULL;
	ID3DBlob *pErrorMsgs = NULL;

	LogInfo("  %cs=%S\n", type, filename);

	switch(type) {
		case 'v':
			ppBytecode = &vs_bytecode;
			vs_override = true;
			break;
		case 'h':
			ppBytecode = &hs_bytecode;
			hs_override = true;
			break;
		case 'd':
			ppBytecode = &ds_bytecode;
			ds_override = true;
			break;
		case 'g':
			ppBytecode = &gs_bytecode;
			gs_override = true;
			break;
		case 'p':
			ppBytecode = &ps_bytecode;
			ps_override = true;
			break;
		case 'c':
			ppBytecode = &cs_bytecode;
			cs_override = true;
			break;
		default:
			// Should not happen
			goto err;
	}

	// Special value to unbind the shader instead:
	if (!_wcsicmp(filename, L"null"))
		return false;

	if (!GetModuleFileName(0, wpath, MAX_PATH)) {
		LogInfo("GetModuleFileName failed\n");
		goto err;
	}
	wcsrchr(wpath, L'\\')[1] = 0;
	wcscat(wpath, filename);

	f = CreateFile(wpath, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (f == INVALID_HANDLE_VALUE) {
		LogInfo("    Shader not found: %S\n", wpath);
		goto err;
	}

	srcDataSize = GetFileSize(f, 0);
	srcData.resize(srcDataSize);

	if (!ReadFile(f, srcData.data(), srcDataSize, &readSize, 0)
			|| srcDataSize != readSize) {
		LogInfo("    Error reading HLSL file\n");
		goto err_close;
	}
	CloseHandle(f);

	// Currently always using shader model 5, could allow this to be
	// overridden in the future:
	_snprintf_s(shaderModel, 7, 7, "%cs_5_0", type);

	// TODO: Add #defines for StereoParams and IniParams. Define a macro
	// for the type of shader, and maybe allow more defines to be specified
	// in the ini

	// Pass the real filename and use the standard include handler so that
	// #include will work with a relative path from the shader itself.
	// Later we could add a custom include handler to track dependencies so
	// that we can make reloading work better when using includes:
	wcstombs(apath, wpath, MAX_PATH);
	hr = D3DCompile(srcData.data(), srcDataSize, apath, 0, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"main", shaderModel, D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, ppBytecode, &pErrorMsgs);

	if (pErrorMsgs && LogFile) { // Check LogFile so the fwrite doesn't crash
		LPVOID errMsg = pErrorMsgs->GetBufferPointer();
		SIZE_T errSize = pErrorMsgs->GetBufferSize();
		LogInfo("--------------------------------------------- BEGIN ---------------------------------------------\n");
		fwrite(errMsg, 1, errSize - 1, LogFile);
		LogInfo("---------------------------------------------- END ----------------------------------------------\n");
		pErrorMsgs->Release();
	}

	if (FAILED(hr))
		goto err;

	// TODO: Cache bytecode

	return false;
err_close:
	CloseHandle(f);
err:
	BeepFailure();
	return true;
}

void CustomShader::substantiate(ID3D11Device *mOrigDevice1)
{
	if (substantiated)
		return;
	substantiated = true;

	if (vs_bytecode) {
		mOrigDevice1->CreateVertexShader(vs_bytecode->GetBufferPointer(), vs_bytecode->GetBufferSize(), NULL, &vs);
		CleanupShaderMaps(vs);
		vs_bytecode->Release();
		vs_bytecode = NULL;
	}
	if (hs_bytecode) {
		mOrigDevice1->CreateHullShader(hs_bytecode->GetBufferPointer(), hs_bytecode->GetBufferSize(), NULL, &hs);
		CleanupShaderMaps(hs);
		hs_bytecode->Release();
		hs_bytecode = NULL;
	}
	if (ds_bytecode) {
		mOrigDevice1->CreateDomainShader(ds_bytecode->GetBufferPointer(), ds_bytecode->GetBufferSize(), NULL, &ds);
		CleanupShaderMaps(ds);
		ds_bytecode->Release();
		ds_bytecode = NULL;
	}
	if (gs_bytecode) {
		mOrigDevice1->CreateGeometryShader(gs_bytecode->GetBufferPointer(), gs_bytecode->GetBufferSize(), NULL, &gs);
		CleanupShaderMaps(gs);
		gs_bytecode->Release();
		gs_bytecode = NULL;
	}
	if (ps_bytecode) {
		mOrigDevice1->CreatePixelShader(ps_bytecode->GetBufferPointer(), ps_bytecode->GetBufferSize(), NULL, &ps);
		CleanupShaderMaps(ps);
		ps_bytecode->Release();
		ps_bytecode = NULL;
	}
	if (cs_bytecode) {
		mOrigDevice1->CreateComputeShader(cs_bytecode->GetBufferPointer(), cs_bytecode->GetBufferSize(), NULL, &cs);
		CleanupShaderMaps(cs);
		cs_bytecode->Release();
		cs_bytecode = NULL;
	}

	if (blend_override == 1) // 2 will merge the blend state at draw time
		mOrigDevice1->CreateBlendState(&blend_desc, &blend_state);

	if (depth_stencil_override == 1) // 2 will merge depth/stencil state at draw time
		mOrigDevice1->CreateDepthStencilState(&depth_stencil_desc, &depth_stencil_state);

	if (rs_override == 1) // 2 will merge rasterizer state at draw time
		mOrigDevice1->CreateRasterizerState(&rs_desc, &rs_state);

	if (sampler_override == 1)
		mOrigDevice1->CreateSamplerState(&sampler_desc, &sampler_state);
}

// Similar to memcpy, but also takes a mask. Any bits in the mask that are set
// to 0 will be unchanged in the destination, while bits that are set to 1 will
// be copied from the source buffer.
static void memcpy_masked_merge(void *dest, void *src, void *mask, size_t n)
{
	char *c_dest = (char*)dest;
	char *c_src = (char*)src;
	char *c_mask = (char*)mask;
	size_t i;

	for (i = 0; i < n; i++)
		c_dest[i] = c_dest[i] & ~c_mask[i] | c_src[i] & c_mask[i];
}

void CustomShader::merge_blend_states(ID3D11BlendState *src_state, FLOAT src_blend_factor[4], UINT src_sample_mask, ID3D11Device *mOrigDevice1)
{
	D3D11_BLEND_DESC src_desc;
	int i;

	if (blend_override != 2)
		return;

	if (blend_state)
		blend_state->Release();
	blend_state = NULL;

	if (src_state) {
		src_state->GetDesc(&src_desc);
	} else {
		// There is no state set, so DX will be using defaults. Set the
		// source description to the defaults so the merge will still
		// work as expected:
		src_desc.AlphaToCoverageEnable = FALSE;
		src_desc.IndependentBlendEnable = FALSE;
		for (i = 0; i < 8; i++) {
			src_desc.RenderTarget[i].BlendEnable = FALSE;
			src_desc.RenderTarget[i].SrcBlend = D3D11_BLEND_ONE;
			src_desc.RenderTarget[i].DestBlend = D3D11_BLEND_ZERO;
			src_desc.RenderTarget[i].BlendOp = D3D11_BLEND_OP_ADD;
			src_desc.RenderTarget[i].SrcBlendAlpha = D3D11_BLEND_ONE;
			src_desc.RenderTarget[i].DestBlendAlpha = D3D11_BLEND_ZERO;
			src_desc.RenderTarget[i].BlendOpAlpha = D3D11_BLEND_OP_ADD;
			src_desc.RenderTarget[i].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		}
	}

	memcpy_masked_merge(&blend_desc, &src_desc, &blend_mask, sizeof(D3D11_BLEND_DESC));

	for (i = 0; i < 4; i++) {
		if (blend_factor_merge_mask[i])
			blend_factor[i] = src_blend_factor[i];
	}
	blend_sample_mask = blend_sample_mask & ~blend_sample_mask_merge_mask | src_sample_mask & blend_sample_mask_merge_mask;

	mOrigDevice1->CreateBlendState(&blend_desc, &blend_state);
}

void CustomShader::merge_depth_stencil_states(ID3D11DepthStencilState *src_state, UINT src_stencil_ref, ID3D11Device *mOrigDevice1)
{
	D3D11_DEPTH_STENCIL_DESC src_desc;

	if (depth_stencil_override != 2)
		return;

	if (depth_stencil_state)
		depth_stencil_state->Release();
	depth_stencil_state = NULL;

	if (src_state) {
		src_state->GetDesc(&src_desc);
	} else {
		// There is no state set, so DX will be using defaults. Set the
		// source description to the defaults so the merge will still
		// work as expected:
		src_desc.DepthEnable = TRUE;
		src_desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		src_desc.DepthFunc = D3D11_COMPARISON_LESS;
		src_desc.StencilEnable = FALSE;
		src_desc.StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK;
		src_desc.StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK;
		src_desc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
		src_desc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
		src_desc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		src_desc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		src_desc.BackFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
		src_desc.BackFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
		src_desc.BackFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		src_desc.BackFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
	}

	memcpy_masked_merge(&depth_stencil_desc, &src_desc, &depth_stencil_mask, sizeof(D3D11_DEPTH_STENCIL_DESC));
	stencil_ref = stencil_ref & ~stencil_ref_mask | src_stencil_ref & stencil_ref_mask;

	mOrigDevice1->CreateDepthStencilState(&depth_stencil_desc, &depth_stencil_state);
}

void CustomShader::merge_rasterizer_states(ID3D11RasterizerState *src_state, ID3D11Device *mOrigDevice1)
{
	D3D11_RASTERIZER_DESC src_desc;

	if (rs_override != 2)
		return;

	if (rs_state)
		rs_state->Release();
	rs_state = NULL;

	if (src_state) {
		src_state->GetDesc(&src_desc);
	} else {
		// There is no state set, so DX will be using defaults. Set the
		// source description to the defaults so the merge will still
		// work as expected:
		src_desc.FillMode = D3D11_FILL_SOLID;
		src_desc.CullMode = D3D11_CULL_BACK;
		src_desc.FrontCounterClockwise = FALSE;
		src_desc.DepthBias = 0;
		src_desc.SlopeScaledDepthBias = 0.0f;
		src_desc.DepthBiasClamp = 0.0f;
		src_desc.DepthClipEnable = TRUE;
		src_desc.ScissorEnable = FALSE;
		src_desc.MultisampleEnable = FALSE;
		src_desc.AntialiasedLineEnable = FALSE;
	}

	memcpy_masked_merge(&rs_desc, &src_desc, &rs_mask, sizeof(D3D11_RASTERIZER_DESC));

	mOrigDevice1->CreateRasterizerState(&rs_desc, &rs_state);
}

struct saved_shader_inst
{
	ID3D11ClassInstance *instances[256];
	UINT num_instances;

	saved_shader_inst() :
		num_instances(0)
	{}

	~saved_shader_inst()
	{
		UINT i;

		for (i = 0; i < num_instances; i++) {
			if (instances[i])
				instances[i]->Release();
		}
	}
};

static void get_all_rts_dsv_uavs(CommandListState *state,
	UINT *NumRTVs,
	ID3D11RenderTargetView *rtvs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT],
	ID3D11DepthStencilView **dsv,
	UINT *UAVStartSlot,
	UINT *NumUAVs,
	ID3D11UnorderedAccessView *uavs[D3D11_PS_CS_UAV_REGISTER_COUNT])

{
	int i;

	// OMGetRenderTargetAndUnorderedAccessViews is a poorly designed API as
	// to use it properly to get all RTVs and UAVs we need to pass it some
	// information that we don't know. So, we have to do a few extra steps
	// to find that info.

	state->mOrigContext1->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, rtvs, dsv);

	*NumRTVs = 0;
	if (rtvs) {
		for (i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++) {
			if (rtvs[i])
				*NumRTVs = i + 1;
		}
	}

	*UAVStartSlot = *NumRTVs;
	// Set NumUAVs to the max to retrieve them all now, and so that later
	// when rebinding them we will unbind any others that the command list
	// bound in the meantime
	*NumUAVs = D3D11_PS_CS_UAV_REGISTER_COUNT - *UAVStartSlot;

	// Finally get all the UAVs. Since we already retrieved the RTVs and
	// DSV we can skip getting them:
	state->mOrigContext1->OMGetRenderTargetsAndUnorderedAccessViews(0, NULL, NULL, *UAVStartSlot, *NumUAVs, uavs);
}

void RunCustomShaderCommand::run(CommandListState *state)
{
	ID3D11Device *mOrigDevice1 = state->mOrigDevice1;
	ID3D11DeviceContext *mOrigContext1 = state->mOrigContext1;
	ID3D11VertexShader *saved_vs = NULL;
	ID3D11HullShader *saved_hs = NULL;
	ID3D11DomainShader *saved_ds = NULL;
	ID3D11GeometryShader *saved_gs = NULL;
	ID3D11PixelShader *saved_ps = NULL;
	ID3D11ComputeShader *saved_cs = NULL;
	ID3D11BlendState *saved_blend = NULL;
	ID3D11DepthStencilState *saved_depth_stencil = NULL;
	ID3D11RasterizerState *saved_rs = NULL;
	UINT num_viewports = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
	D3D11_VIEWPORT saved_viewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
	FLOAT saved_blend_factor[4];
	UINT saved_sample_mask;
	UINT saved_stencil_ref;
	bool saved_post;
	UINT NumRTVs, UAVStartSlot, NumUAVs;
	ID3D11RenderTargetView *saved_rtvs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
	ID3D11DepthStencilView *saved_dsv;
	ID3D11UnorderedAccessView *saved_uavs[D3D11_PS_CS_UAV_REGISTER_COUNT];
	UINT uav_counts[D3D11_PS_CS_UAV_REGISTER_COUNT] = {(UINT)-1, (UINT)-1, (UINT)-1, (UINT)-1, (UINT)-1, (UINT)-1, (UINT)-1, (UINT)-1};
	UINT i;
	D3D11_PRIMITIVE_TOPOLOGY saved_topology;
	UINT num_sampler = D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT;
	ID3D11SamplerState* saved_sampler_states[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT];

	for (UINT i = 0; i < D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT; ++i)
	{
		saved_sampler_states[i] = nullptr;
	}

	state->mHackerContext->FrameAnalysisLog("3DMigoto %S\n", ini_line.c_str());

	if (custom_shader->max_executions_per_frame) {
		if (custom_shader->frame_no != G->frame_no) {
			custom_shader->frame_no = G->frame_no;
			custom_shader->executions_this_frame = 1;
		} else if (custom_shader->executions_this_frame++ >= custom_shader->max_executions_per_frame) {
			state->mHackerContext->FrameAnalysisLog("3DMigoto   max_executions_per_frame exceeded\n");
			return;
		}
	}

	custom_shader->substantiate(mOrigDevice1);

	saved_shader_inst vs_inst, hs_inst, ds_inst, gs_inst, ps_inst, cs_inst;

	// Assign custom shaders first before running the command lists, and
	// restore them last. This is so that if someone was injecting a
	// sequence of pixel shaders that all shared a common vertex shader
	// we can avoid having to repeatedly save & restore the vertex shader
	// by calling the next shader in sequence from the command list after
	// the draw call.

	if (custom_shader->vs_override) {
		mOrigContext1->VSGetShader(&saved_vs, vs_inst.instances, &vs_inst.num_instances);
		mOrigContext1->VSSetShader(custom_shader->vs, NULL, 0);
	}
	if (custom_shader->hs_override) {
		mOrigContext1->HSGetShader(&saved_hs, hs_inst.instances, &hs_inst.num_instances);
		mOrigContext1->HSSetShader(custom_shader->hs, NULL, 0);
	}
	if (custom_shader->ds_override) {
		mOrigContext1->DSGetShader(&saved_ds, ds_inst.instances, &ds_inst.num_instances);
		mOrigContext1->DSSetShader(custom_shader->ds, NULL, 0);
	}
	if (custom_shader->gs_override) {
		mOrigContext1->GSGetShader(&saved_gs, gs_inst.instances, &gs_inst.num_instances);
		mOrigContext1->GSSetShader(custom_shader->gs, NULL, 0);
	}
	if (custom_shader->ps_override) {
		mOrigContext1->PSGetShader(&saved_ps, ps_inst.instances, &ps_inst.num_instances);
		mOrigContext1->PSSetShader(custom_shader->ps, NULL, 0);
	}
	if (custom_shader->cs_override) {
		mOrigContext1->CSGetShader(&saved_cs, cs_inst.instances, &cs_inst.num_instances);
		mOrigContext1->CSSetShader(custom_shader->cs, NULL, 0);
	}
	if (custom_shader->blend_override) {
		mOrigContext1->OMGetBlendState(&saved_blend, saved_blend_factor, &saved_sample_mask);
		custom_shader->merge_blend_states(saved_blend, saved_blend_factor, saved_sample_mask, mOrigDevice1);
		mOrigContext1->OMSetBlendState(custom_shader->blend_state, custom_shader->blend_factor, custom_shader->blend_sample_mask);
	}
	if (custom_shader->depth_stencil_override) {
		mOrigContext1->OMGetDepthStencilState(&saved_depth_stencil, &saved_stencil_ref);
		custom_shader->merge_depth_stencil_states(saved_depth_stencil, saved_stencil_ref, mOrigDevice1);
		mOrigContext1->OMSetDepthStencilState(custom_shader->depth_stencil_state, custom_shader->stencil_ref);
	}
	if (custom_shader->rs_override) {
		mOrigContext1->RSGetState(&saved_rs);
		custom_shader->merge_rasterizer_states(saved_rs, mOrigDevice1);
		mOrigContext1->RSSetState(custom_shader->rs_state);
	}
	if (custom_shader->topology != D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED) {
		mOrigContext1->IAGetPrimitiveTopology(&saved_topology);
		mOrigContext1->IASetPrimitiveTopology(custom_shader->topology);
	}
	if (custom_shader->sampler_override) {
		mOrigContext1->PSGetSamplers(0, num_sampler, saved_sampler_states);
		mOrigContext1->PSSetSamplers(0, 1, &custom_shader->sampler_state); //just one slot for the moment TODO: allow more via *.ini file
	}
	// We save off the viewports unconditionally for now. We could
	// potentially skip this by flagging if a command list may alter them,
	// but that probably wouldn't buy us anything:
	mOrigContext1->RSGetViewports(&num_viewports, saved_viewports);
	// Likewise, save off all RTVs, UAVs and DSVs unconditionally:
	get_all_rts_dsv_uavs(state, &NumRTVs, saved_rtvs, &saved_dsv, &UAVStartSlot, &NumUAVs, saved_uavs);

	// Run the command lists. This should generally include a draw or
	// dispatch call, or call out to another command list which does.
	// The reason for having a post command list is so that people can
	// write 'ps-t100 = ResourceFoo; post ps-t100 = null' and have it work.
	saved_post = state->post;
	state->post = false;
	_RunCommandList(&custom_shader->command_list, state);
	state->post = true;
	_RunCommandList(&custom_shader->post_command_list, state);
	state->post = saved_post;

	// Finally restore the original shaders
	if (custom_shader->vs_override)
		mOrigContext1->VSSetShader(saved_vs, vs_inst.instances, vs_inst.num_instances);
	if (custom_shader->hs_override)
		mOrigContext1->HSSetShader(saved_hs, hs_inst.instances, hs_inst.num_instances);
	if (custom_shader->ds_override)
		mOrigContext1->DSSetShader(saved_ds, ds_inst.instances, ds_inst.num_instances);
	if (custom_shader->gs_override)
		mOrigContext1->GSSetShader(saved_gs, gs_inst.instances, gs_inst.num_instances);
	if (custom_shader->ps_override)
		mOrigContext1->PSSetShader(saved_ps, ps_inst.instances, ps_inst.num_instances);
	if (custom_shader->cs_override)
		mOrigContext1->CSSetShader(saved_cs, cs_inst.instances, cs_inst.num_instances);
	if (custom_shader->blend_override)
		mOrigContext1->OMSetBlendState(saved_blend, saved_blend_factor, saved_sample_mask);
	if (custom_shader->depth_stencil_override)
		mOrigContext1->OMSetDepthStencilState(saved_depth_stencil, saved_stencil_ref);
	if (custom_shader->rs_override)
		mOrigContext1->RSSetState(saved_rs);
	if (custom_shader->topology != D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED)
		mOrigContext1->IASetPrimitiveTopology(saved_topology);
	if (custom_shader->sampler_override)
		mOrigContext1->PSSetSamplers(0, num_sampler, saved_sampler_states);

	mOrigContext1->RSSetViewports(num_viewports, saved_viewports);
	mOrigContext1->OMSetRenderTargetsAndUnorderedAccessViews(NumRTVs, saved_rtvs, saved_dsv, UAVStartSlot, NumUAVs, saved_uavs, uav_counts);

	if (saved_vs)
		saved_vs->Release();
	if (saved_hs)
		saved_hs->Release();
	if (saved_ds)
		saved_ds->Release();
	if (saved_gs)
		saved_gs->Release();
	if (saved_ps)
		saved_ps->Release();
	if (saved_cs)
		saved_cs->Release();
	if (saved_blend)
		saved_blend->Release();
	if (saved_depth_stencil)
		saved_depth_stencil->Release();
	if (saved_rs)
		saved_rs->Release();

	for (i = 0; i < num_sampler; ++i) {
		if (saved_sampler_states[i])
			saved_sampler_states[i]->Release();
	}
	for (i = 0; i < NumRTVs; i++) {
		if (saved_rtvs[i])
			saved_rtvs[i]->Release();
	}
	for (i = 0; i < NumUAVs; i++) {
		if (saved_uavs[i])
			saved_uavs[i]->Release();
	}
}

void RunExplicitCommandList::run(CommandListState *state)
{
	state->mHackerContext->FrameAnalysisLog("3DMigoto %S\n", ini_line.c_str());

	if (state->post)
		_RunCommandList(&command_list_section->post_command_list, state);
	else
		_RunCommandList(&command_list_section->command_list, state);
}

void LinkCommandLists(CommandList *dst, CommandList *link)
{
	RunLinkedCommandList *operation = new RunLinkedCommandList(link);
	dst->push_back(std::shared_ptr<CommandListCommand>(operation));
}

void RunLinkedCommandList::run(CommandListState *state)
{
	_RunCommandList(link, state);
}

static void ProcessParamRTSize(CommandListState *state)
{
	D3D11_RENDER_TARGET_VIEW_DESC view_desc;
	D3D11_TEXTURE2D_DESC res_desc;
	ID3D11RenderTargetView *view = NULL;
	ID3D11Resource *res = NULL;
	ID3D11Texture2D *tex = NULL;

	if (state->rt_width != -1)
		return;

	state->mOrigContext1->OMGetRenderTargets(1, &view, NULL);
	if (!view)
		return;

	view->GetDesc(&view_desc);

	if (view_desc.ViewDimension != D3D11_RTV_DIMENSION_TEXTURE2D &&
	    view_desc.ViewDimension != D3D11_RTV_DIMENSION_TEXTURE2DMS)
		goto out_release_view;

	view->GetResource(&res);
	if (!res)
		goto out_release_view;

	tex = (ID3D11Texture2D *)res;
	tex->GetDesc(&res_desc);

	state->rt_width = (float)res_desc.Width;
	state->rt_height = (float)res_desc.Height;

	tex->Release();
out_release_view:
	view->Release();
}

float ParamOverride::process_texture_filter(CommandListState *state)
{
	bool resource_found;

	TextureOverride *texture_override = texture_filter_target.FindTextureOverride(state, &resource_found);

	// If there is no resource bound we want to return a special value that
	// is distinct from simply not finding a texture override section. For
	// backwards compatibility we use negative zero -0.0, because any
	// existing fixes that test for zero/non-zero to check if a matching
	// [TextureOverride] is present would expect an unbound texture to
	// never have a hash and therefore be equal to 0, and -0.0 *is* equal
	// to +0, so these will continue to work. To explicitly test for an
	// unassigned resource, use this HLSL to reinterpret the values as
	// integers and check the sign bit:
	//
	// if (asint(IniParams[0].x) == asint(-0.0)) { ... }
	//
	if (!resource_found)
		return -0.0;

	// A resource was bound, but no matching texture override was found:
	if (!texture_override)
		return 0;

	return texture_override->filter_index;
}


CommandListState::CommandListState() :
	mHackerDevice(NULL),
	mHackerContext(NULL),
	mOrigDevice1(NULL),
	mOrigContext1(NULL),
	rt_width(-1),
	rt_height(-1),
	call_info(NULL),
	resource(NULL),
	view(NULL),
	post(false),
	update_params(false),
	cursor_mask_tex(NULL),
	cursor_mask_view(NULL),
	cursor_color_tex(NULL),
	cursor_color_view(NULL),
	recursion(0),
	aborted(false)
{
	memset(&cursor_info, 0, sizeof(CURSORINFO));
	memset(&cursor_info_ex, 0, sizeof(ICONINFO));
	memset(&window_rect, 0, sizeof(RECT));
}

CommandListState::~CommandListState()
{
	if (cursor_info_ex.hbmMask)
		DeleteObject(cursor_info_ex.hbmMask);
	if (cursor_info_ex.hbmColor)
		DeleteObject(cursor_info_ex.hbmColor);
	if (cursor_mask_view)
		cursor_mask_view->Release();
	if (cursor_mask_tex)
		cursor_mask_tex->Release();
	if (cursor_color_view)
		cursor_color_view->Release();
	if (cursor_color_tex)
		cursor_color_tex->Release();
}

static void UpdateWindowInfo(CommandListState *state)
{
	if (state->window_rect.right)
		return;

	if (G->hWnd)
		CursorUpscalingBypass_GetClientRect(G->hWnd, &state->window_rect);
	else
		LogDebug("UpdateWindowInfo: No hWnd\n");
}

static void UpdateCursorInfo(CommandListState *state)
{
	if (state->cursor_info.cbSize)
		return;

	state->cursor_info.cbSize = sizeof(CURSORINFO);
	CursorUpscalingBypass_GetCursorInfo(&state->cursor_info);
	memcpy(&state->cursor_window_coords, &state->cursor_info.ptScreenPos, sizeof(POINT));

	if (G->hWnd)
		CursorUpscalingBypass_ScreenToClient(G->hWnd, &state->cursor_window_coords);
	else
		LogDebug("UpdateCursorInfo: No hWnd\n");
}

static void UpdateCursorInfoEx(CommandListState *state)
{
	if (state->cursor_info_ex.hbmMask)
		return;

	UpdateCursorInfo(state);

	GetIconInfo(state->cursor_info.hCursor, &state->cursor_info_ex);
}

// Uses an undocumented Windows API to get info about animated cursors and
// calculate the current frame based on the global tick count
// https://stackoverflow.com/questions/6969801/how-do-i-determine-if-the-current-mouse-cursor-is-animated
static unsigned GetCursorFrame(HCURSOR cursor)
{
	typedef HCURSOR(WINAPI* GET_CURSOR_FRAME_INFO)(HCURSOR, LPCWSTR, DWORD, DWORD*, DWORD*);
	static GET_CURSOR_FRAME_INFO fnGetCursorFrameInfo = NULL;
	HMODULE libUser32 = NULL;
	DWORD period = 6, frames = 1;

	if (!fnGetCursorFrameInfo) {
		libUser32 = LoadLibraryA("user32.dll");
		if (!libUser32)
			return 0;

		fnGetCursorFrameInfo = (GET_CURSOR_FRAME_INFO)GetProcAddress(libUser32, "GetCursorFrameInfo");
		if (!fnGetCursorFrameInfo)
			return 0;
	}

	fnGetCursorFrameInfo(cursor, L"", 0, &period, &frames);

	// Avoid divide by zero if not an animated cursor:
	if (!period || !frames)
		return 0;

	// period is a multiple of 1/60 seconds. We should really use the ms
	// since this cursor was most recently displayed, but the global tick
	// count works well enough and means we have less state to track:
	return (GetTickCount() * 6) / (period * 100) % frames;
}

static void _CreateTextureFromBitmap(HDC dc, BITMAP *bitmap_obj,
		HBITMAP hbitmap, CommandListState *state,
		ID3D11Texture2D **tex, ID3D11ShaderResourceView **view)
{
	D3D11_SHADER_RESOURCE_VIEW_DESC rv_desc;
	BITMAPINFOHEADER bmp_info;
	D3D11_SUBRESOURCE_DATA data;
	D3D11_TEXTURE2D_DESC desc;
	HRESULT hr;

	bmp_info.biSize = sizeof(BITMAPINFOHEADER);
	bmp_info.biWidth = bitmap_obj->bmWidth;
	bmp_info.biHeight = bitmap_obj->bmHeight;
	// Requesting 32bpp here to simplify the conversion process - the
	// R1_UNORM format can't be used for the 1bpp mask because that format
	// has a special purpose, and requesting 8 or 16bpp would require an
	// array of RGBQUADs after the BITMAPINFO structure for the pallette
	// that I don't want to deal with, and there is no DXGI_FORMAT for
	// 24bpp... 32bpp should work for both the mask and palette:
	bmp_info.biBitCount = 32;
	bmp_info.biPlanes = 1;
	bmp_info.biCompression = BI_RGB;
	// Pretty sure these are ignored / output only in GetDIBits:
	bmp_info.biSizeImage = 0;
	bmp_info.biXPelsPerMeter = 0;
	bmp_info.biYPelsPerMeter = 0;
	bmp_info.biClrUsed = 0;
	bmp_info.biClrImportant = 0;

	// This padding came from an example on MSDN, but I can't find
	// the documentation that indicates exactly what it is supposed
	// to be. Since we're using 32bpp, this shouldn't matter anyway:
	data.SysMemPitch = ((bitmap_obj->bmWidth * bmp_info.biBitCount + 31) / 32) * 4;

	data.pSysMem = new char[data.SysMemPitch * bitmap_obj->bmHeight];

	if (!GetDIBits(dc, hbitmap, 0, bmp_info.biHeight,
			(LPVOID)data.pSysMem, (BITMAPINFO*)&bmp_info, DIB_RGB_COLORS)) {
		LogInfo("Software Mouse: GetDIBits() failed\n");
		goto err_free;
	}

	desc.Width = bitmap_obj->bmWidth;
	desc.Height = bitmap_obj->bmHeight;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	// FIXME: Use DXGI_FORMAT_B8G8R8X8_UNORM_SRGB if no alpha channel (there is no API to check)
	desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	desc.CPUAccessFlags = 0;
	desc.MiscFlags = 0;

	hr = state->mOrigDevice1->CreateTexture2D(&desc, &data, tex);
	if (FAILED(hr)) {
		LogInfo("Software Mouse: CreateTexture2D Failed: 0x%x\n", hr);
		goto err_free;
	}

	rv_desc.Format = desc.Format;
	rv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	rv_desc.Texture2D.MostDetailedMip = 0;
	rv_desc.Texture2D.MipLevels = 1;

	hr = state->mOrigDevice1->CreateShaderResourceView(*tex, &rv_desc, view);
	if (FAILED(hr)) {
		LogInfo("Software Mouse: CreateShaderResourceView Failed: 0x%x\n", hr);
		goto err_release_tex;
	}

	delete [] data.pSysMem;

	return;
err_release_tex:
	(*tex)->Release();
	*tex = NULL;
err_free:
	delete [] data.pSysMem;
}

static void CreateTextureFromBitmap(HDC dc, HBITMAP hbitmap, CommandListState *state,
		ID3D11Texture2D **tex, ID3D11ShaderResourceView **view)
{
	BITMAP bitmap_obj;

	if (!GetObject(hbitmap, sizeof(BITMAP), &bitmap_obj)) {
		LogInfo("Software Mouse: GetObject() failed\n");
		return;
	}

	_CreateTextureFromBitmap(dc, &bitmap_obj, hbitmap, state, tex, view);
}

static void CreateTextureFromAnimatedCursor(
		HDC dc,
		HCURSOR cursor,
		UINT flags,
		HBITMAP static_bitmap,
		CommandListState *state,
		ID3D11Texture2D **tex,
		ID3D11ShaderResourceView **view
		)
{
	BITMAP bitmap_obj;
	HDC dc_mem;
	HBITMAP ani_bitmap;
	unsigned frame;

	if (!GetObject(static_bitmap, sizeof(BITMAP), &bitmap_obj)) {
		LogInfo("Software Mouse: GetObject() failed\n");
		return;
	}

	dc_mem = CreateCompatibleDC(dc);
	if (!dc_mem) {
		LogInfo("Software Mouse: CreateCompatibleDC() failed\n");
		return;
	}

	ani_bitmap = CreateCompatibleBitmap(dc, bitmap_obj.bmWidth, bitmap_obj.bmHeight);
	if (!ani_bitmap) {
		LogInfo("Software Mouse: CreateCompatibleBitmap() failed\n");
		goto out_delete_mem_dc;
	}

	frame = GetCursorFrame(cursor);

	// To get a frame from an animated cursor we have to use DrawIconEx to
	// draw it to another bitmap, then we can create a texture from that
	// bitmap:
	SelectObject(dc_mem, ani_bitmap);
	if (!DrawIconEx(dc_mem, 0, 0, cursor, bitmap_obj.bmWidth, bitmap_obj.bmHeight, frame, NULL, flags)) {
		LogInfo("Software Mouse: DrawIconEx failed\n");
		// Fall back to getting the first frame from the static_bitmap we already have:
		_CreateTextureFromBitmap(dc, &bitmap_obj, static_bitmap, state, tex, view);
		goto out_delete_ani_bitmap;
	}

	_CreateTextureFromBitmap(dc, &bitmap_obj, ani_bitmap, state, tex, view);

out_delete_ani_bitmap:
	DeleteObject(ani_bitmap);
out_delete_mem_dc:
	DeleteDC(dc_mem);
}

static void UpdateCursorResources(CommandListState *state)
{
	HDC dc;

	if (state->cursor_mask_tex || state->cursor_color_tex)
		return;

	UpdateCursorInfoEx(state);

	// XXX: Should maybe be the device context for the window?
	dc = GetDC(NULL);
	if (!dc) {
		LogInfo("Software Mouse: GetDC() failed\n");
		return;
	}

	if (state->cursor_info_ex.hbmColor) {
		// Colour cursor, which may or may not be animated, but the
		// animated routine will work either way:
		CreateTextureFromAnimatedCursor(
				dc,
				state->cursor_info.hCursor,
				DI_IMAGE,
				state->cursor_info_ex.hbmColor,
				state,
				&state->cursor_color_tex,
				&state->cursor_color_view);

		if (state->cursor_info_ex.hbmMask) {
			// Since it's a colour cursor the mask bitmap will be
			// the regular height, which will work with the
			// animated routine:
			CreateTextureFromAnimatedCursor(
					dc,
					state->cursor_info.hCursor,
					DI_MASK,
					state->cursor_info_ex.hbmMask,
					state,
					&state->cursor_mask_tex,
					&state->cursor_mask_view);
		}
	} else if (state->cursor_info_ex.hbmMask) {
		// Black and white cursor, which means the hbmMask bitmap is
		// double height and won't work with the animated cursor
		// routines, so just turn the bitmap into a texture directly:
		CreateTextureFromBitmap(
				dc,
				state->cursor_info_ex.hbmMask,
				state,
				&state->cursor_mask_tex,
				&state->cursor_mask_view);
	}

	ReleaseDC(NULL, dc);
}

void ParamOverride::run(CommandListState *state)
{
	float *dest = &(G->iniParams[param_idx].*param_component);
	float orig = *dest;

	state->mHackerContext->FrameAnalysisLog("3DMigoto %S\n", ini_line.c_str());

	switch (type) {
		case ParamOverrideType::VALUE:
			*dest = val;
			break;
		case ParamOverrideType::RT_WIDTH:
			ProcessParamRTSize(state);
			*dest = state->rt_width;
			break;
		case ParamOverrideType::RT_HEIGHT:
			ProcessParamRTSize(state);
			*dest = state->rt_height;
			break;
		case ParamOverrideType::RES_WIDTH:
			*dest = (float)G->mResolutionInfo.width;
			break;
		case ParamOverrideType::RES_HEIGHT:
			*dest = (float)G->mResolutionInfo.height;
			break;
		case ParamOverrideType::WINDOW_WIDTH:
			UpdateWindowInfo(state);
			*dest = (float)state->window_rect.right;
			break;
		case ParamOverrideType::WINDOW_HEIGHT:
			UpdateWindowInfo(state);
			*dest = (float)state->window_rect.bottom;
			break;
		case ParamOverrideType::TEXTURE:
			*dest = process_texture_filter(state);
			break;
		case ParamOverrideType::VERTEX_COUNT:
			if (state->call_info)
				*dest = (float)state->call_info->VertexCount;
			else
				*dest = 0;
			break;
		case ParamOverrideType::INDEX_COUNT:
			if (state->call_info)
				*dest = (float)state->call_info->IndexCount;
			else
				*dest = 0;
			break;
		case ParamOverrideType::INSTANCE_COUNT:
			if (state->call_info)
				*dest = (float)state->call_info->InstanceCount;
			else
				*dest = 0;
			break;
		case ParamOverrideType::CURSOR_VISIBLE:
			UpdateCursorInfo(state);
			*dest = !!(state->cursor_info.flags & CURSOR_SHOWING);
			break;
		case ParamOverrideType::CURSOR_SCREEN_X:
			UpdateCursorInfo(state);
			*dest = (float)state->cursor_info.ptScreenPos.x;
			break;
		case ParamOverrideType::CURSOR_SCREEN_Y:
			UpdateCursorInfo(state);
			*dest = (float)state->cursor_info.ptScreenPos.y;
			break;
		case ParamOverrideType::CURSOR_WINDOW_X:
			UpdateCursorInfo(state);
			*dest = (float)state->cursor_window_coords.x;
			break;
		case ParamOverrideType::CURSOR_WINDOW_Y:
			UpdateCursorInfo(state);
			*dest = (float)state->cursor_window_coords.y;
			break;
		case ParamOverrideType::CURSOR_X:
			UpdateCursorInfo(state);
			UpdateWindowInfo(state);
			*dest = (float)state->cursor_window_coords.x / (float)state->window_rect.right;
			break;
		case ParamOverrideType::CURSOR_Y:
			UpdateCursorInfo(state);
			UpdateWindowInfo(state);
			*dest = (float)state->cursor_window_coords.y / (float)state->window_rect.bottom;
			break;
		case ParamOverrideType::CURSOR_HOTSPOT_X:
			UpdateCursorInfoEx(state);
			*dest = (float)state->cursor_info_ex.xHotspot;
			break;
		case ParamOverrideType::CURSOR_HOTSPOT_Y:
			UpdateCursorInfoEx(state);
			*dest = (float)state->cursor_info_ex.yHotspot;
			break;
		case ParamOverrideType::TIME:
			*dest = (float)(GetTickCount() - G->ticks_at_launch) / 1000.0f;
			break;
		case ParamOverrideType::RAW_SEPARATION:
			// We could use cached values of these (nvapi is known
			// to become a bottleneck with too many calls / frame),
			// but they need to be up to date, taking into account
			// any changes made via the command list already this
			// frame (this is used for snapshots and getting the
			// current convergence regardless of whether an
			// asynchronous transfer from the GPU has or has not
			// completed) - StereoParams is currently unsuitable
			// for this as it is only updated once / frame... We
			// could change it so that StereoParams is always up to
			// date - it would differ from the historical
			// behaviour, but I doubt it would break anything.
			// Otherwise we could have a separate cache. Whatever -
			// this is rarely used, so let's just go with this for
			// now and worry about optimisations only if it proves
			// to be a bottleneck in practice:
			NvAPI_Stereo_GetSeparation(state->mHackerDevice->mStereoHandle, dest);
			break;
		case ParamOverrideType::CONVERGENCE:
			NvAPI_Stereo_GetConvergence(state->mHackerDevice->mStereoHandle, dest);
			break;
		case ParamOverrideType::EYE_SEPARATION:
			NvAPI_Stereo_GetEyeSeparation(state->mHackerDevice->mStereoHandle, dest);
			break;
		default:
			return;
	}

	state->mHackerContext->FrameAnalysisLog("3DMigoto   ini param override = %f\n", *dest);

	state->update_params |= (*dest != orig);
}

// Parse IniParams overrides, in forms such as
// x = 0.3 (set parameter to specific value, e.g. for shader partner filtering)
// y2 = ps-t0 (use parameter for texture filtering based on texture slot of shader type)
// z3 = rt_width / rt_height (set parameter to render target width/height)
// w4 = res_width / res_height (set parameter to resolution width/height)
bool ParseCommandListIniParamOverride(const wchar_t *section,
		const wchar_t *key, wstring *val, CommandList *command_list)
{
	int ret, len1;
	ParamOverride *param = new ParamOverride();

	if (!ParseIniParamName(key, &param->param_idx, &param->param_component))
		goto bail;

	// Try parsing value as a float
	ret = swscanf_s(val->c_str(), L"%f%n", &param->val, &len1);
	if (ret != 0 && ret != EOF && len1 == val->length()) {
		param->type = ParamOverrideType::VALUE;
		goto success;
	}

	// Try parsing value as a resource target for texture filtering
	ret = param->texture_filter_target.ParseTarget(val->c_str(), true);
	if (ret) {
		param->type = ParamOverrideType::TEXTURE;
		goto success;
	}

	// Check special keywords
	param->type = lookup_enum_val<const wchar_t *, ParamOverrideType>
		(ParamOverrideTypeNames, val->c_str(), ParamOverrideType::INVALID);
	if (param->type == ParamOverrideType::INVALID)
		goto bail;

success:
	param->ini_line = L"[" + wstring(section) + L"] " + wstring(key) + L" = " + *val;
	command_list->push_back(std::shared_ptr<CommandListCommand>(param));
	return true;
bail:
	delete param;
	return false;
}

ResourcePool::~ResourcePool()
{
	unordered_map<uint32_t, ID3D11Resource*>::iterator i;

	for (i = cache.begin(); i != cache.end(); i++) {
		if (i->second)
			i->second->Release();
	}
	cache.clear();
}

void ResourcePool::emplace(uint32_t hash, ID3D11Resource *resource)
{
	if (resource)
		resource->AddRef();
	cache.emplace(hash, resource);
}

template <typename ResourceType,
	 typename DescType,
	HRESULT (__stdcall ID3D11Device::*CreateResource)(THIS_
	      const DescType *pDesc,
	      const D3D11_SUBRESOURCE_DATA *pInitialData,
	      ResourceType **ppTexture)
	>
static ResourceType* GetResourceFromPool(
		wstring *ini_line,
		ResourceType *src_resource,
		ResourceType *dst_resource,
		ResourcePool *resource_pool,
		CommandListState *state,
		DescType *desc)
{
	ResourceType *resource = NULL;
	DescType old_desc;
	uint32_t hash;
	size_t size;
	HRESULT hr;

	// We don't want to use the CalTexture2D/3DDescHash functions because
	// the resolution override could produce the same hash for distinct
	// texture descriptions. This hash isn't exposed to the user, so
	// doesn't matter what we use - just has to be fast.
	hash = crc32c_hw(0, &desc, sizeof(DescType));

	try {
		resource = (ResourceType*)resource_pool->cache.at(hash);
		if (resource == dst_resource)
			return NULL;
		if (resource) {
			LogDebug("Switching cached resource %S\n", ini_line->c_str());
			resource->AddRef();
		}
	} catch (std::out_of_range) {
		LogInfo("Creating cached resource %S\n", ini_line->c_str());

		hr = (state->mOrigDevice1->*CreateResource)(desc, NULL, &resource);
		if (FAILED(hr)) {
			LogInfo("Resource copy failed %S: 0x%x\n", ini_line->c_str(), hr);
			LogResourceDesc(desc);
			src_resource->GetDesc(&old_desc);
			LogInfo("Original resource was:\n");
			LogResourceDesc(&old_desc);

			// Prevent further attempts:
			resource_pool->emplace(hash, NULL);

			return NULL;
		}
		resource_pool->emplace(hash, resource);
		size = resource_pool->cache.size();
		if (size > 1)
			LogInfo("  NOTICE: cache now contains %Ii resources\n", size);

		LogDebugResourceDesc(desc);
	}

	return resource;
}

CustomResource::CustomResource() :
	resource(NULL),
	view(NULL),
	is_null(true),
	substantiated(false),
	bind_flags((D3D11_BIND_FLAG)0),
	stride(0),
	offset(0),
	buf_size(0),
	format(DXGI_FORMAT_UNKNOWN),
	max_copies_per_frame(0),
	frame_no(0),
	copies_this_frame(0),
	override_type(CustomResourceType::INVALID),
	override_mode(CustomResourceMode::DEFAULT),
	override_bind_flags(CustomResourceBindFlags::INVALID),
	override_format((DXGI_FORMAT)-1),
	override_width(-1),
	override_height(-1),
	override_depth(-1),
	override_mips(-1),
	override_array(-1),
	override_msaa(-1),
	override_msaa_quality(-1),
	override_byte_width(-1),
	override_stride(-1),
	width_multiply(1.0f),
	height_multiply(1.0f),
	initial_data(NULL),
	initial_data_size(0)
{}

CustomResource::~CustomResource()
{
	if (resource)
		resource->Release();
	if (view)
		view->Release();
	free(initial_data);
}

bool CustomResource::OverrideSurfaceCreationMode(StereoHandle mStereoHandle, NVAPI_STEREO_SURFACECREATEMODE *orig_mode)
{

	if (override_mode == CustomResourceMode::DEFAULT)
		return false;

	NvAPI_Stereo_GetSurfaceCreationMode(mStereoHandle, orig_mode);

	switch (override_mode) {
		case CustomResourceMode::STEREO:
			NvAPI_Stereo_SetSurfaceCreationMode(mStereoHandle,
					NVAPI_STEREO_SURFACECREATEMODE_FORCESTEREO);
			return true;
		case CustomResourceMode::MONO:
			NvAPI_Stereo_SetSurfaceCreationMode(mStereoHandle,
					NVAPI_STEREO_SURFACECREATEMODE_FORCEMONO);
			return true;
		case CustomResourceMode::AUTO:
			NvAPI_Stereo_SetSurfaceCreationMode(mStereoHandle,
					NVAPI_STEREO_SURFACECREATEMODE_AUTO);
			return true;
	}

	return false;
}

void CustomResource::Substantiate(ID3D11Device *mOrigDevice1, StereoHandle mStereoHandle)
{
	NVAPI_STEREO_SURFACECREATEMODE orig_mode = NVAPI_STEREO_SURFACECREATEMODE_AUTO;
	bool restore_create_mode = false;

	// We only allow a custom resource to be substantiated once. Otherwise
	// we could end up reloading it again if it is later set to null. Also
	// prevents us from endlessly retrying to load a custom resource from a
	// file that doesn't exist:
	if (substantiated)
		return;
	substantiated = true;

	// If this custom resource has already been set through other means we
	// won't overwrite it:
	if (resource || view)
		return;

	// If the resource section has enough information to create a resource
	// we do so the first time it is loaded from. The reason we do it this
	// late is to make sure we know which device is actually being used to
	// render the game - FC4 creates about a dozen devices with different
	// parameters while probing the hardware before it settles on the one
	// it will actually use.

	restore_create_mode = OverrideSurfaceCreationMode(mStereoHandle, &orig_mode);

	if (!filename.empty()) {
		LoadFromFile(mOrigDevice1);
	} else {
		switch (override_type) {
			case CustomResourceType::BUFFER:
			case CustomResourceType::STRUCTURED_BUFFER:
			case CustomResourceType::RAW_BUFFER:
				SubstantiateBuffer(mOrigDevice1, NULL, 0);
				break;
			case CustomResourceType::TEXTURE1D:
				SubstantiateTexture1D(mOrigDevice1);
				break;
			case CustomResourceType::TEXTURE2D:
			case CustomResourceType::CUBE:
				SubstantiateTexture2D(mOrigDevice1);
				break;
			case CustomResourceType::TEXTURE3D:
				SubstantiateTexture3D(mOrigDevice1);
				break;
		}
	}

	if (restore_create_mode)
		NvAPI_Stereo_SetSurfaceCreationMode(mStereoHandle, orig_mode);
}

void CustomResource::LoadBufferFromFile(ID3D11Device *mOrigDevice1)
{
	DWORD size, read_size;
	void *buf = NULL;
	HANDLE f;

	f = CreateFile(filename.c_str(), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (f == INVALID_HANDLE_VALUE) {
		LogInfo("Failed to load custom buffer resource %S: %d\n", filename.c_str(), GetLastError());
		return;
	}

	size = GetFileSize(f, 0);
	buf = malloc(size); // malloc to allow realloc to resize it if the user overrode the size
	if (!buf) {
		LogInfo("Out of memory loading %S\n", filename.c_str());
		goto out_close;
	}

	if (!ReadFile(f, buf, size, &read_size, 0) || size != read_size) {
		LogInfo("Error reading custom buffer from file %S\n", filename.c_str());
		goto out_delete;
	}

	SubstantiateBuffer(mOrigDevice1, &buf, size);

out_delete:
	free(buf);
out_close:
	CloseHandle(f);
}

void CustomResource::LoadFromFile(ID3D11Device *mOrigDevice1)
{
	wstring ext;
	HRESULT hr;

	switch (override_type) {
		case CustomResourceType::BUFFER:
		case CustomResourceType::STRUCTURED_BUFFER:
		case CustomResourceType::RAW_BUFFER:
			return LoadBufferFromFile(mOrigDevice1);
	}

	// XXX: We are not creating a view with DirecXTK because
	// 1) it assumes we want a shader resource view, which is an
	//    assumption that doesn't fit with the goal of this code to
	//    allow for arbitrary resource copying, and
	// 2) we currently won't use the view in a source custom
	//    resource, even if we are referencing it into a compatible
	//    slot. We might improve this, and if we do, I don't want
	//    any surprises caused by a view of the wrong type we
	//    happen to have created here and forgotten about.
	// If we do start using the source custom resource's view, we
	// could do something smart here, like only using it if the
	// bind_flags indicate it will be used as a shader resource.

	ext = filename.substr(filename.rfind(L"."));
	if (!_wcsicmp(ext.c_str(), L".dds")) {
		LogInfoW(L"Loading custom resource %s as DDS\n", filename.c_str());
		hr = DirectX::CreateDDSTextureFromFileEx(mOrigDevice1,
				filename.c_str(), 0,
				D3D11_USAGE_DEFAULT, bind_flags, 0, 0,
				false, &resource, NULL, NULL);
	} else {
		LogInfoW(L"Loading custom resource %s as WIC\n", filename.c_str());
		hr = DirectX::CreateWICTextureFromFileEx(mOrigDevice1,
				filename.c_str(), 0,
				D3D11_USAGE_DEFAULT, bind_flags, 0, 0,
				false, &resource, NULL);
	}
	if (SUCCEEDED(hr)) {
		is_null = false;
		// TODO:
		// format = ...
	} else
		LogInfoW(L"Failed to load custom texture resource %s: 0x%x\n", filename.c_str(), hr);
}

void CustomResource::SubstantiateBuffer(ID3D11Device *mOrigDevice1, void **buf, DWORD size)
{
	D3D11_SUBRESOURCE_DATA data = {0}, *pInitialData = NULL;
	ID3D11Buffer *buffer;
	D3D11_BUFFER_DESC desc;
	HRESULT hr;

	if (!buf) {
		// If no file is passed in, we use the optional initial data to
		// initialise the buffer. We do this even if no initial data
		// has been specified, so that the buffer will be initialised
		// with zeroes for safety.
		buf = &initial_data;
		size = (DWORD)initial_data_size;
	}

	memset(&desc, 0, sizeof(desc));
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = bind_flags;

	// Allow the buffer size to be set from the file / initial data size,
	// but it can be overridden if specified explicitly. If it's a
	// structured buffer, we assume just a single structure by default, but
	// again this can be overridden. The reason for doing this here, and
	// not in OverrideBufferDesc, is that this only applies if we are
	// substantiating the resource from scratch, not when copying a resource.
	if (size) {
		desc.ByteWidth = size;
		if (override_type == CustomResourceType::STRUCTURED_BUFFER)
			desc.StructureByteStride = size;
	}

	OverrideBufferDesc(&desc);

	if (desc.ByteWidth > 0) {
		// Fill in size from the file/initial data, allowing for an
		// override to make it larger or smaller, which may involve
		// reallocating the buffer from the caller.
		if (desc.ByteWidth > size) {
			void *new_buf = realloc(*buf, desc.ByteWidth);
			if (!new_buf) {
				LogInfo("Out of memory enlarging buffer: [%S]\n", name.c_str());
				return;
			}
			memset((char*)new_buf + size, 0, desc.ByteWidth - size);
			*buf = new_buf;
		}

		data.pSysMem = *buf;
		pInitialData = &data;
	}

	hr = mOrigDevice1->CreateBuffer(&desc, pInitialData, &buffer);
	if (SUCCEEDED(hr)) {
		LogInfo("Substantiated custom %S [%S]\n",
				lookup_enum_name(CustomResourceTypeNames, override_type), name.c_str());
		LogDebugResourceDesc(&desc);
		resource = (ID3D11Resource*)buffer;
		is_null = false;
		if (override_format != (DXGI_FORMAT)-1)
			format = override_format;
	} else {
		LogInfo("Failed to substantiate custom %S [%S]: 0x%x\n",
				lookup_enum_name(CustomResourceTypeNames, override_type), name.c_str(), hr);
		LogResourceDesc(&desc);
		BeepFailure();
	}
}
void CustomResource::SubstantiateTexture1D(ID3D11Device *mOrigDevice1)
{
	ID3D11Texture1D *tex1d;
	D3D11_TEXTURE1D_DESC desc;
	HRESULT hr;

	memset(&desc, 0, sizeof(desc));
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = bind_flags;
	OverrideTexDesc(&desc);

	hr = mOrigDevice1->CreateTexture1D(&desc, NULL, &tex1d);
	if (SUCCEEDED(hr)) {
		LogInfo("Substantiated custom %S [%S]\n",
				lookup_enum_name(CustomResourceTypeNames, override_type), name.c_str());
		LogDebugResourceDesc(&desc);
		resource = (ID3D11Resource*)tex1d;
		is_null = false;
	} else {
		LogInfo("Failed to substantiate custom %S [%S]: 0x%x\n",
				lookup_enum_name(CustomResourceTypeNames, override_type), name.c_str(), hr);
		LogResourceDesc(&desc);
		BeepFailure();
	}
}
void CustomResource::SubstantiateTexture2D(ID3D11Device *mOrigDevice1)
{
	ID3D11Texture2D *tex2d;
	D3D11_TEXTURE2D_DESC desc;
	HRESULT hr;

	memset(&desc, 0, sizeof(desc));
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = bind_flags;
	OverrideTexDesc(&desc);

	hr = mOrigDevice1->CreateTexture2D(&desc, NULL, &tex2d);
	if (SUCCEEDED(hr)) {
		LogInfo("Substantiated custom %S [%S]\n",
				lookup_enum_name(CustomResourceTypeNames, override_type), name.c_str());
		LogDebugResourceDesc(&desc);
		resource = (ID3D11Resource*)tex2d;
		is_null = false;
	} else {
		LogInfo("Failed to substantiate custom %S [%S]: 0x%x\n",
				lookup_enum_name(CustomResourceTypeNames, override_type), name.c_str(), hr);
		LogResourceDesc(&desc);
		BeepFailure();
	}
}
void CustomResource::SubstantiateTexture3D(ID3D11Device *mOrigDevice1)
{
	ID3D11Texture3D *tex3d;
	D3D11_TEXTURE3D_DESC desc;
	HRESULT hr;

	memset(&desc, 0, sizeof(desc));
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = bind_flags;
	OverrideTexDesc(&desc);

	hr = mOrigDevice1->CreateTexture3D(&desc, NULL, &tex3d);
	if (SUCCEEDED(hr)) {
		LogInfo("Substantiated custom %S [%S]\n",
				lookup_enum_name(CustomResourceTypeNames, override_type), name.c_str());
		LogDebugResourceDesc(&desc);
		resource = (ID3D11Resource*)tex3d;
		is_null = false;
	} else {
		LogInfo("Failed to substantiate custom %S [%S]: 0x%x\n",
				lookup_enum_name(CustomResourceTypeNames, override_type), name.c_str(), hr);
		LogResourceDesc(&desc);
		BeepFailure();
	}
}

void CustomResource::OverrideBufferDesc(D3D11_BUFFER_DESC *desc)
{
	switch (override_type) {
		case CustomResourceType::STRUCTURED_BUFFER:
			desc->MiscFlags |= D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
			break;
		case CustomResourceType::RAW_BUFFER:
			desc->MiscFlags |= D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
			break;
	}

	if (override_stride != -1)
		desc->StructureByteStride = override_stride;
	else if (override_format != (DXGI_FORMAT)-1 && override_format != DXGI_FORMAT_UNKNOWN)
		desc->StructureByteStride = dxgi_format_size(override_format);

	if (override_byte_width != -1)
		desc->ByteWidth = override_byte_width;
	else if (override_array != -1)
		desc->ByteWidth = desc->StructureByteStride * override_array;

	if (override_bind_flags != CustomResourceBindFlags::INVALID)
		desc->BindFlags = (D3D11_BIND_FLAG)override_bind_flags;

	// TODO: Add more overrides for misc flags
}

void CustomResource::OverrideTexDesc(D3D11_TEXTURE1D_DESC *desc)
{
	if (override_width != -1)
		desc->Width = override_width;
	if (override_mips != -1)
		desc->MipLevels = override_mips;
	if (override_array != -1)
		desc->ArraySize = override_array;
	if (override_format != (DXGI_FORMAT)-1)
		desc->Format = override_format;

	desc->Width = (UINT)(desc->Width * width_multiply);

	if (override_bind_flags != CustomResourceBindFlags::INVALID)
		desc->BindFlags = (D3D11_BIND_FLAG)override_bind_flags;

	// TODO: Add more overrides for misc flags
}

void CustomResource::OverrideTexDesc(D3D11_TEXTURE2D_DESC *desc)
{
	if (override_width != -1)
		desc->Width = override_width;
	if (override_height != -1)
		desc->Height = override_height;
	if (override_mips != -1)
		desc->MipLevels = override_mips;
	if (override_format != (DXGI_FORMAT)-1)
		desc->Format = override_format;
	if (override_array != -1)
		desc->ArraySize = override_array;
	if (override_msaa != -1)
		desc->SampleDesc.Count = override_msaa;
	if (override_msaa_quality != -1)
		desc->SampleDesc.Quality = override_msaa_quality;

	if (override_type == CustomResourceType::CUBE) {
		desc->MiscFlags |= D3D11_RESOURCE_MISC_TEXTURECUBE;
		if (override_array != -1)
			desc->ArraySize = override_array * 6;
	}

	desc->Width = (UINT)(desc->Width * width_multiply);
	desc->Height = (UINT)(desc->Height * height_multiply);

	if (override_bind_flags != CustomResourceBindFlags::INVALID)
		desc->BindFlags = (D3D11_BIND_FLAG)override_bind_flags;

	// TODO: Add more overrides for misc flags
}

void CustomResource::OverrideTexDesc(D3D11_TEXTURE3D_DESC *desc)
{
	if (override_width != -1)
		desc->Width = override_width;
	if (override_height != -1)
		desc->Height = override_height;
	if (override_depth != -1)
		desc->Height = override_depth;
	if (override_mips != -1)
		desc->MipLevels = override_mips;
	if (override_format != (DXGI_FORMAT)-1)
		desc->Format = override_format;

	desc->Width = (UINT)(desc->Width * width_multiply);
	desc->Height = (UINT)(desc->Height * height_multiply);

	if (override_bind_flags != CustomResourceBindFlags::INVALID)
		desc->BindFlags = (D3D11_BIND_FLAG)override_bind_flags;

	// TODO: Add more overrides for misc flags
}

void CustomResource::OverrideOutOfBandInfo(DXGI_FORMAT *format, UINT *stride)
{
	if (override_format != (DXGI_FORMAT)-1)
		*format = override_format;
	if (override_stride != -1)
		*stride = override_stride;
}


bool ResourceCopyTarget::ParseTarget(const wchar_t *target, bool is_source)
{
	int ret, len;
	size_t length = wcslen(target);
	CustomResources::iterator res;

	ret = swscanf_s(target, L"%lcs-cb%u%n", &shader_type, 1, &slot, &len);
	if (ret == 2 && len == length && slot < D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT) {
		type = ResourceCopyTargetType::CONSTANT_BUFFER;
		goto check_shader_type;
	}

	ret = swscanf_s(target, L"%lcs-t%u%n", &shader_type, 1, &slot, &len);
	if (ret == 2 && len == length && slot < D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT) {
		type = ResourceCopyTargetType::SHADER_RESOURCE;
	       goto check_shader_type;
	}

	// TODO: ret = swscanf_s(target, L"%lcs-s%u%n", &shader_type, 1, &slot, &len);
	// TODO: if (ret == 2 && len == length && slot < D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT) {
	// TODO: 	type = ResourceCopyTargetType::SAMPLER;
	// TODO:	goto check_shader_type;
	// TODO: }

	ret = swscanf_s(target, L"o%u%n", &slot, &len);
	if (ret == 1 && len == length && slot < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT) {
		type = ResourceCopyTargetType::RENDER_TARGET;
		return true;
	}

	if (!wcscmp(target, L"od")) {
		type = ResourceCopyTargetType::DEPTH_STENCIL_TARGET;
		return true;
	}

	ret = swscanf_s(target, L"%lcs-u%u%n", &shader_type, 1, &slot, &len);
	// XXX: On Win8 D3D11_1_UAV_SLOT_COUNT (64) is the limit instead. Use
	// the lower amount for now to enforce compatibility.
	if (ret == 2 && len == length && slot < D3D11_PS_CS_UAV_REGISTER_COUNT) {
		// These views are only valid for pixel and compute shaders:
		if (shader_type == L'p' || shader_type == L'c') {
			type = ResourceCopyTargetType::UNORDERED_ACCESS_VIEW;
			return true;
		}
		return false;
	}

	ret = swscanf_s(target, L"vb%u%n", &slot, &len);
	if (ret == 1 && len == length && slot < D3D11_IA_VERTEX_INPUT_RESOURCE_SLOT_COUNT) {
		type = ResourceCopyTargetType::VERTEX_BUFFER;
		return true;
	}

	if (!wcscmp(target, L"ib")) {
		type = ResourceCopyTargetType::INDEX_BUFFER;
		return true;
	}

	ret = swscanf_s(target, L"so%u%n", &slot, &len);
	if (ret == 1 && len == length && slot < D3D11_SO_STREAM_COUNT) {
		type = ResourceCopyTargetType::STREAM_OUTPUT;
		return true;
	}

	if (is_source && !wcscmp(target, L"null")) {
		type = ResourceCopyTargetType::EMPTY;
		return true;
	}

	if (length >= 9 && !wcsncmp(target, L"resource", 8)) {
		// section name should already have been transformed to lower
		// case from ParseCommandList, so our keys will be consistent
		// in the unordered_map:
		wstring resource_id(target);

		res = customResources.find(resource_id);
		if (res == customResources.end())
			return false;

		custom_resource = &res->second;
		type = ResourceCopyTargetType::CUSTOM_RESOURCE;
		return true;
	}

	// Alternate means to assign StereoParams and IniParams
	if (is_source && !wcscmp(target, L"stereoparams")) {
		type = ResourceCopyTargetType::STEREO_PARAMS;
		return true;
	}

	if (is_source && !wcscmp(target, L"iniparams")) {
		type = ResourceCopyTargetType::INI_PARAMS;
		return true;
	}

	if (is_source && !wcscmp(target, L"cursor_mask")) {
		type = ResourceCopyTargetType::CURSOR_MASK;
		return true;
	}

	if (is_source && !wcscmp(target, L"cursor_color")) {
		type = ResourceCopyTargetType::CURSOR_COLOR;
		return true;
	}

	if (is_source && !wcscmp(target, L"this")) {
		type = ResourceCopyTargetType::THIS_RESOURCE;
		return true;
	}

	// XXX: Any reason to allow access to sequential swap chains? Given
	// they either won't exist or are read only I can't think of one.
	if (is_source && !wcscmp(target, L"bb")) { // Back Buffer
		type = ResourceCopyTargetType::SWAP_CHAIN;
		// Holding a reference on the back buffer will prevent
		// ResizeBuffers() from working, so forbid caching any views of
		// the back buffer. Leaving it bound could also be a problem,
		// but since this is usually only used from custom shader
		// sections they will take care of unbinding it automatically:
		forbid_view_cache = true;
		return true;
	}

	if (is_source && !wcscmp(target, L"f_bb")) {
		type = ResourceCopyTargetType::FAKE_SWAP_CHAIN;
		// Holding a reference on the back buffer will prevent
		// ResizeBuffers() from working, so forbid caching any views of
		// the back buffer. Leaving it bound could also be a problem,
		// but since this is usually only used from custom shader
		// sections they will take care of unbinding it automatically:
		forbid_view_cache = true;
		return true;
	}

	return false;

check_shader_type:
	switch(shader_type) {
		case L'v': case L'h': case L'd': case L'g': case L'p': case L'c':
			return true;
	}
	return false;
}


bool ParseCommandListResourceCopyDirective(const wchar_t *section,
		const wchar_t *key, wstring *val, CommandList *command_list)
{
	ResourceCopyOperation *operation = new ResourceCopyOperation();
	wchar_t buf[MAX_PATH];
	wchar_t *src_ptr = NULL;

	if (!operation->dst.ParseTarget(key, false))
		goto bail;

	// parse_enum_option_string replaces spaces with NULLs, so it can't
	// operate on the buffer in the wstring directly. I could potentially
	// change it to work without modifying the string, but for now it's
	// easier to just make a copy of the string:
	if (val->length() >= MAX_PATH)
		goto bail;
	wcsncpy_s(buf, val->c_str(), MAX_PATH);

	operation->options = parse_enum_option_string<wchar_t *, ResourceCopyOptions>
		(ResourceCopyOptionNames, buf, &src_ptr);

	if (!src_ptr)
		goto bail;

	if (!operation->src.ParseTarget(src_ptr, true))
		goto bail;

	if (!(operation->options & ResourceCopyOptions::COPY_TYPE_MASK)) {
		// If the copy method was not speficied make a guess.
		// References aren't always safe (e.g. a resource can't be both
		// an input and an output), and a resource may not have been
		// created with the right usage flags, so we'll err on the side
		// of doing a full copy if we aren't fairly sure.
		//
		// If we're merely copying a resource from one shader to
		// another without changnig the usage (e.g. giving the vertex
		// shader access to a constant buffer or texture from the pixel
		// shader) a reference is probably safe (unless the game
		// reassigns it to a different usage later and doesn't know
		// that our reference is still bound somewhere), but it would
		// not be safe to give a vertex shader access to the depth
		// buffer of the output merger stage, for example.
		//
		// If we are copying a resource into a custom resource (e.g.
		// for use from another draw call), do a full copy by default
		// in case the game alters the original.
		//
		// If we are assigning a render target, do so by reference
		// since we probably want the result reflected in the resource
		// we assigned to it. Mostly this would already work due to the
		// custom resource rules, but adding this rule should make
		// assigning the back buffer to a render target work.
		if (operation->dst.type == ResourceCopyTargetType::CUSTOM_RESOURCE)
			operation->options |= ResourceCopyOptions::COPY;
		else if (operation->dst.type == ResourceCopyTargetType::RENDER_TARGET)
			operation->options |= ResourceCopyOptions::REFERENCE;
		else if (operation->src.type == ResourceCopyTargetType::CUSTOM_RESOURCE)
			operation->options |= ResourceCopyOptions::REFERENCE;
		else if (operation->src.type == operation->dst.type)
			operation->options |= ResourceCopyOptions::REFERENCE;
		else if (operation->dst.type == ResourceCopyTargetType::SHADER_RESOURCE
				&& (operation->src.type == ResourceCopyTargetType::STEREO_PARAMS
				|| operation->src.type == ResourceCopyTargetType::INI_PARAMS
				|| operation->src.type == ResourceCopyTargetType::CURSOR_MASK
				|| operation->src.type == ResourceCopyTargetType::CURSOR_COLOR))
			operation->options |= ResourceCopyOptions::REFERENCE;
		else
			operation->options |= ResourceCopyOptions::COPY;
	}

	// FIXME: If custom resources are copied to other custom resources by
	// reference that are in turn bound to the pipeline we may not
	// propagate all the bind flags correctly depending on the order
	// everything is parsed. We'd need to construct a dependency graph
	// to fix this, but it's not clear that this combination would really
	// be used in practice, so for now this will do.
	// FIXME: The constant buffer bind flag can't be combined with others
	if (operation->src.type == ResourceCopyTargetType::CUSTOM_RESOURCE &&
			(operation->options & ResourceCopyOptions::REFERENCE)) {
		// Fucking C++ making this line 3x longer than it should be:
		operation->src.custom_resource->bind_flags = (D3D11_BIND_FLAG)
			(operation->src.custom_resource->bind_flags | operation->dst.BindFlags());
	}

	operation->ini_line = L"[" + wstring(section) + L"] " + wstring(key) + L" = " + *val;
	command_list->push_back(std::shared_ptr<CommandListCommand>(operation));
	return true;
bail:
	delete operation;
	return false;
}

ID3D11Resource *ResourceCopyTarget::GetResource(
		CommandListState *state,
		ID3D11View **view,   // Used by textures, render targets, depth/stencil buffers & UAVs
		UINT *stride,        // Used by vertex buffers
		UINT *offset,        // Used by vertex & index buffers
		DXGI_FORMAT *format, // Used by index buffers
		UINT *buf_size)      // Used when creating a view of the buffer
{
	HackerDevice *mHackerDevice = state->mHackerDevice;
	ID3D11Device *mOrigDevice1 = state->mOrigDevice1;
	ID3D11DeviceContext *mOrigContext1 = state->mOrigContext1;
	ID3D11Resource *res = NULL;
	ID3D11Buffer *buf = NULL;
	ID3D11Buffer *so_bufs[D3D11_SO_STREAM_COUNT];
	ID3D11ShaderResourceView *resource_view = NULL;
	ID3D11RenderTargetView *render_view[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
	ID3D11DepthStencilView *depth_view = NULL;
	ID3D11UnorderedAccessView *unordered_view = NULL;
	unsigned i;

	switch(type) {
	case ResourceCopyTargetType::CONSTANT_BUFFER:
		// FIXME: On win8 (or with evil update?), we should use
		// Get/SetConstantBuffers1 and copy the offset into the buffer as well
		switch(shader_type) {
		case L'v':
			mOrigContext1->VSGetConstantBuffers(slot, 1, &buf);
			return buf;
		case L'h':
			mOrigContext1->HSGetConstantBuffers(slot, 1, &buf);
			return buf;
		case L'd':
			mOrigContext1->DSGetConstantBuffers(slot, 1, &buf);
			return buf;
		case L'g':
			mOrigContext1->GSGetConstantBuffers(slot, 1, &buf);
			return buf;
		case L'p':
			mOrigContext1->PSGetConstantBuffers(slot, 1, &buf);
			return buf;
		case L'c':
			mOrigContext1->CSGetConstantBuffers(slot, 1, &buf);
			return buf;
		default:
			// Should not happen
			return NULL;
		}
		break;

	case ResourceCopyTargetType::SHADER_RESOURCE:
		switch(shader_type) {
		case L'v':
			mOrigContext1->VSGetShaderResources(slot, 1, &resource_view);
			break;
		case L'h':
			mOrigContext1->HSGetShaderResources(slot, 1, &resource_view);
			break;
		case L'd':
			mOrigContext1->DSGetShaderResources(slot, 1, &resource_view);
			break;
		case L'g':
			mOrigContext1->GSGetShaderResources(slot, 1, &resource_view);
			break;
		case L'p':
			mOrigContext1->PSGetShaderResources(slot, 1, &resource_view);
			break;
		case L'c':
			mOrigContext1->CSGetShaderResources(slot, 1, &resource_view);
			break;
		default:
			// Should not happen
			return NULL;
		}

		if (!resource_view)
			return NULL;

		resource_view->GetResource(&res);
		if (!res) {
			resource_view->Release();
			return NULL;
		}

		*view = resource_view;
		return res;

	// TODO: case ResourceCopyTargetType::SAMPLER: // Not an ID3D11Resource, need to think about this one
	// TODO: 	break;

	case ResourceCopyTargetType::VERTEX_BUFFER:
		// TODO: If copying this to a constant buffer, provide some
		// means to get the strides + offsets from within the shader.
		// Perhaps as an IniParam, or in another constant buffer?
		mOrigContext1->IAGetVertexBuffers(slot, 1, &buf, stride, offset);

		// To simplify things we just copy the part of the buffer
		// referred to by this call, so adjust the offset with the
		// call-specific first vertex. Do NOT set the buffer size here
		// as if it's too small it will disable the region copy later.
		// TODO: Add a keyword to ignore offsets in case we want the
		// whole buffer regardless
		if (state->call_info && stride && offset)
			*offset += state->call_info->FirstVertex * *stride;
		return buf;

	case ResourceCopyTargetType::INDEX_BUFFER:
		// TODO: Similar comment as vertex buffers above, provide a
		// means for a shader to get format + offset.
		mOrigContext1->IAGetIndexBuffer(&buf, format, offset);
		if (stride && format)
			*stride = dxgi_format_size(*format);

		// To simplify things we just copy the part of the buffer
		// referred to by this call, so adjust the offset with the
		// call-specific first index. Do NOT set the buffer size here
		// as if it's too small it will disable the region copy later.
		// TODO: Add a keyword to ignore offsets in case we want the
		// whole buffer regardless
		if (state->call_info && stride && offset)
			*offset += state->call_info->FirstIndex * *stride;
		return buf;

	case ResourceCopyTargetType::STREAM_OUTPUT:
		// XXX: Does not give us the offset
		mOrigContext1->SOGetTargets(slot + 1, so_bufs);

		// Release any buffers we aren't after:
		for (i = 0; i < slot; i++) {
			if (so_bufs[i]) {
				so_bufs[i]->Release();
				so_bufs[i] = NULL;
			}
		}

		return so_bufs[slot];

	case ResourceCopyTargetType::RENDER_TARGET:
		mOrigContext1->OMGetRenderTargets(slot + 1, render_view, NULL);

		// Release any views we aren't after:
		for (i = 0; i < slot; i++) {
			if (render_view[i]) {
				render_view[i]->Release();
				render_view[i] = NULL;
			}
		}

		if (!render_view[slot])
			return NULL;

		render_view[slot]->GetResource(&res);
		if (!res) {
			render_view[slot]->Release();
			return NULL;
		}

		*view = render_view[slot];
		return res;

	case ResourceCopyTargetType::DEPTH_STENCIL_TARGET:
		mOrigContext1->OMGetRenderTargets(0, NULL, &depth_view);
		if (!depth_view)
			return NULL;

		depth_view->GetResource(&res);
		if (!res) {
			depth_view->Release();
			return NULL;
		}

		// Depth buffers can't be buffers

		*view = depth_view;
		return res;

	case ResourceCopyTargetType::UNORDERED_ACCESS_VIEW:
		switch(shader_type) {
		case L'p':
			// XXX: Not clear if the start slot is ok like this from the docs?
			// Particularly, what happens if we retrieve a subsequent UAV?
			mOrigContext1->OMGetRenderTargetsAndUnorderedAccessViews(0, NULL, NULL, slot, 1, &unordered_view);
			break;
		case L'c':
			mOrigContext1->CSGetUnorderedAccessViews(slot, 1, &unordered_view);
			break;
		default:
			// Should not happen
			return NULL;
		}

		if (!unordered_view)
			return NULL;

		unordered_view->GetResource(&res);
		if (!res) {
			unordered_view->Release();
			return NULL;
		}

		*view = unordered_view;
		return res;

	case ResourceCopyTargetType::CUSTOM_RESOURCE:
		custom_resource->Substantiate(mOrigDevice1, mHackerDevice->mStereoHandle);

		if (stride)
			*stride = custom_resource->stride;
		if (offset)
			*offset = custom_resource->offset;
		if (format)
			*format = custom_resource->format;
		if (buf_size)
			*buf_size = custom_resource->buf_size;

		if (custom_resource->is_null) {
			// Optimisation to allow the resource to be set to null
			// without throwing away the cache so we don't
			// endlessly create & destroy temporary resources.
			*view = NULL;
			return NULL;
		}

		if (custom_resource->view)
			custom_resource->view->AddRef();
		*view = custom_resource->view;
		if (custom_resource->resource)
			custom_resource->resource->AddRef();
		return custom_resource->resource;

	case ResourceCopyTargetType::STEREO_PARAMS:
		if (mHackerDevice->mStereoResourceView)
			mHackerDevice->mStereoResourceView->AddRef();
		*view = mHackerDevice->mStereoResourceView;
		if (mHackerDevice->mStereoTexture)
			mHackerDevice->mStereoTexture->AddRef();
		return mHackerDevice->mStereoTexture;

	case ResourceCopyTargetType::INI_PARAMS:
		if (mHackerDevice->mIniResourceView)
			mHackerDevice->mIniResourceView->AddRef();
		*view = mHackerDevice->mIniResourceView;
		if (mHackerDevice->mIniTexture)
			mHackerDevice->mIniTexture->AddRef();
		return mHackerDevice->mIniTexture;

	case ResourceCopyTargetType::CURSOR_MASK:
		UpdateCursorResources(state);
		if (state->cursor_mask_view)
			state->cursor_mask_view->AddRef();
		*view = state->cursor_mask_view;
		if (state->cursor_mask_tex)
			state->cursor_mask_tex->AddRef();
		return state->cursor_mask_tex;

	case ResourceCopyTargetType::CURSOR_COLOR:
		UpdateCursorResources(state);
		if (state->cursor_color_view)
			state->cursor_color_view->AddRef();
		*view = state->cursor_color_view;
		if (state->cursor_color_tex)
			state->cursor_color_tex->AddRef();
		return state->cursor_color_tex;

	case ResourceCopyTargetType::THIS_RESOURCE:
		if (state->view)
			state->view->AddRef();
		*view = state->view;
		if (state->resource)
			state->resource->AddRef();
		return state->resource;

	case ResourceCopyTargetType::SWAP_CHAIN:
		G->gHackerSwapChain->GetBuffer(0, __uuidof(ID3D11Resource), (void**)&res);
		return res;

	case ResourceCopyTargetType::FAKE_SWAP_CHAIN:
		G->gHackerSwapChain->GetBuffer(0, __uuidof(ID3D11Resource), (void**)&res);
		return res;
	}

	return NULL;
}

void ResourceCopyTarget::SetResource(
		CommandListState *state,
		ID3D11Resource *res,
		ID3D11View *view,
		UINT stride,
		UINT offset,
		DXGI_FORMAT format,
		UINT buf_size)
{
	ID3D11DeviceContext *mOrigContext1 = state->mOrigContext1;
	ID3D11Buffer *buf = NULL;
	ID3D11Buffer *so_bufs[D3D11_SO_STREAM_COUNT];
	ID3D11ShaderResourceView *resource_view = NULL;
	ID3D11RenderTargetView *render_view[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
	ID3D11DepthStencilView *depth_view = NULL;
	ID3D11UnorderedAccessView *unordered_view = NULL;
	UINT uav_counter = -1; // TODO: Allow this to be set
	int i;

	switch(type) {
	case ResourceCopyTargetType::CONSTANT_BUFFER:
		// FIXME: On win8 (or with evil update?), we should use
		// Get/SetConstantBuffers1 and copy the offset into the buffer as well
		buf = (ID3D11Buffer*)res;
		switch(shader_type) {
		case L'v':
			mOrigContext1->VSSetConstantBuffers(slot, 1, &buf);
			return;
		case L'h':
			mOrigContext1->HSSetConstantBuffers(slot, 1, &buf);
			return;
		case L'd':
			mOrigContext1->DSSetConstantBuffers(slot, 1, &buf);
			return;
		case L'g':
			mOrigContext1->GSSetConstantBuffers(slot, 1, &buf);
			return;
		case L'p':
			mOrigContext1->PSSetConstantBuffers(slot, 1, &buf);
			return;
		case L'c':
			mOrigContext1->CSSetConstantBuffers(slot, 1, &buf);
			return;
		default:
			// Should not happen
			return;
		}
		break;

	case ResourceCopyTargetType::SHADER_RESOURCE:
		resource_view = (ID3D11ShaderResourceView*)view;
		switch(shader_type) {
		case L'v':
			mOrigContext1->VSSetShaderResources(slot, 1, &resource_view);
			break;
		case L'h':
			mOrigContext1->HSSetShaderResources(slot, 1, &resource_view);
			break;
		case L'd':
			mOrigContext1->DSSetShaderResources(slot, 1, &resource_view);
			break;
		case L'g':
			mOrigContext1->GSSetShaderResources(slot, 1, &resource_view);
			break;
		case L'p':
			mOrigContext1->PSSetShaderResources(slot, 1, &resource_view);
			break;
		case L'c':
			mOrigContext1->CSSetShaderResources(slot, 1, &resource_view);
			break;
		default:
			// Should not happen
			return;
		}
		break;

	// TODO: case ResourceCopyTargetType::SAMPLER: // Not an ID3D11Resource, need to think about this one
	// TODO: 	break;

	case ResourceCopyTargetType::VERTEX_BUFFER:
		buf = (ID3D11Buffer*)res;
		mOrigContext1->IASetVertexBuffers(slot, 1, &buf, &stride, &offset);
		return;

	case ResourceCopyTargetType::INDEX_BUFFER:
		buf = (ID3D11Buffer*)res;
		mOrigContext1->IASetIndexBuffer(buf, format, offset);
		break;

	case ResourceCopyTargetType::STREAM_OUTPUT:
		// XXX: HERE BE UNTESTED CODE PATHS!
		buf = (ID3D11Buffer*)res;
		mOrigContext1->SOGetTargets(D3D11_SO_STREAM_COUNT, so_bufs);
		if (so_bufs[slot])
			so_bufs[slot]->Release();
		so_bufs[slot] = buf;
		// XXX: We set offsets to NULL here. We should really preserve
		// them, but I'm not sure how to get their original values,
		// so... too bad. Probably will never even use this anyway.
		mOrigContext1->SOSetTargets(D3D11_SO_STREAM_COUNT, so_bufs, NULL);

		for (i = 0; i < D3D11_SO_STREAM_COUNT; i++) {
			if (so_bufs[i])
				so_bufs[i]->Release();
		}

		break;

	case ResourceCopyTargetType::RENDER_TARGET:
		mOrigContext1->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, render_view, &depth_view);

		if (render_view[slot])
			render_view[slot]->Release();
		render_view[slot] = (ID3D11RenderTargetView*)view;

		mOrigContext1->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, render_view, depth_view);

		for (i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++) {
			if (i != slot && render_view[i])
				render_view[i]->Release();
		}
		if (depth_view)
			depth_view->Release();

		break;

	case ResourceCopyTargetType::DEPTH_STENCIL_TARGET:
		mOrigContext1->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, render_view, &depth_view);

		if (depth_view)
			depth_view->Release();
		depth_view = (ID3D11DepthStencilView*)view;

		mOrigContext1->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, render_view, depth_view);

		for (i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++) {
			if (render_view[i])
				render_view[i]->Release();
		}
		break;

	case ResourceCopyTargetType::UNORDERED_ACCESS_VIEW:
		// XXX: HERE BE UNTESTED CODE PATHS!
		unordered_view = (ID3D11UnorderedAccessView*)view;
		switch(shader_type) {
		case L'p':
			// XXX: Not clear if this will unbind other UAVs or not?
			// TODO: Allow pUAVInitialCounts to optionally be set
			mOrigContext1->OMSetRenderTargetsAndUnorderedAccessViews(D3D11_KEEP_RENDER_TARGETS_AND_DEPTH_STENCIL,
				NULL, NULL, slot, 1, &unordered_view, &uav_counter);
			return;
		case L'c':
			// TODO: Allow pUAVInitialCounts to optionally be set
			mOrigContext1->CSSetUnorderedAccessViews(slot, 1, &unordered_view, &uav_counter);
			return;
		default:
			// Should not happen
			return;
		}
		break;

	case ResourceCopyTargetType::CUSTOM_RESOURCE:
		custom_resource->stride = stride;
		custom_resource->offset = offset;
		custom_resource->format = format;
		custom_resource->buf_size = buf_size;


		if (res == NULL && view == NULL) {
			// Optimisation to allow the resource to be set to null
			// without throwing away the cache so we don't
			// endlessly create & destroy temporary resources.
			custom_resource->is_null = true;
			return;
		}
		custom_resource->is_null = false;

		// If we are passed our own resource (might happen if the
		// resource is used directly in the run() function, or if
		// someone assigned a resource to itself), don't needlessly
		// AddRef() and Release(), and definitely don't Release()
		// before AddRef()
		if (custom_resource->view != view) {
			if (custom_resource->view)
				custom_resource->view->Release();
			custom_resource->view = view;
			if (custom_resource->view)
				custom_resource->view->AddRef();
		}

		if (custom_resource->resource != res) {
			if (custom_resource->resource)
				custom_resource->resource->Release();
			custom_resource->resource = res;
			if (custom_resource->resource)
				custom_resource->resource->AddRef();
		}
		break;

	case ResourceCopyTargetType::STEREO_PARAMS:
	case ResourceCopyTargetType::INI_PARAMS:
	case ResourceCopyTargetType::SWAP_CHAIN:
	case ResourceCopyTargetType::FAKE_SWAP_CHAIN:
	case ResourceCopyTargetType::CPU:
		// Only way we could "set" a resource to the (fake) back buffer is by
		// copying to it. Might implement overwrites later, but no
		// pressing need. To write something to the back buffer, assign
		// it as a render target instead.
		//
		// We can't set values on the CPU directly from here, since the
		// values won't have finished transferring yet. These will be
		// set from elsewhere.
		break;
	}
}

D3D11_BIND_FLAG ResourceCopyTarget::BindFlags()
{
	switch(type) {
		case ResourceCopyTargetType::CONSTANT_BUFFER:
			return D3D11_BIND_CONSTANT_BUFFER;
		case ResourceCopyTargetType::SHADER_RESOURCE:
			return D3D11_BIND_SHADER_RESOURCE;
		case ResourceCopyTargetType::VERTEX_BUFFER:
			return D3D11_BIND_VERTEX_BUFFER;
		case ResourceCopyTargetType::INDEX_BUFFER:
			return D3D11_BIND_INDEX_BUFFER;
		case ResourceCopyTargetType::STREAM_OUTPUT:
			return D3D11_BIND_STREAM_OUTPUT;
		case ResourceCopyTargetType::RENDER_TARGET:
			return D3D11_BIND_RENDER_TARGET;
		case ResourceCopyTargetType::DEPTH_STENCIL_TARGET:
			return D3D11_BIND_DEPTH_STENCIL;
		case ResourceCopyTargetType::UNORDERED_ACCESS_VIEW:
			return D3D11_BIND_UNORDERED_ACCESS;
		case ResourceCopyTargetType::CUSTOM_RESOURCE:
			return custom_resource->bind_flags;
		case ResourceCopyTargetType::STEREO_PARAMS:
		case ResourceCopyTargetType::INI_PARAMS:
		case ResourceCopyTargetType::SWAP_CHAIN:
		case ResourceCopyTargetType::CPU:
			// N/A since swap chain can't be set as a destination
			return (D3D11_BIND_FLAG)0;
	}

	// Shouldn't happen. No return value makes sense, so raise an exception
	throw(std::range_error("Bad 3DMigoto ResourceCopyTarget"));
}

TextureOverride* ResourceCopyTarget::FindTextureOverride(CommandListState *state, bool *resource_found)
{
	TextureOverrideMap::iterator i;
	TextureOverride *ret = NULL;
	ID3D11Resource *resource = NULL;
	ID3D11View *view = NULL;
	uint32_t hash = 0;

	resource = GetResource(state, &view, NULL, NULL, NULL, NULL);

	if (resource_found)
		*resource_found = !!resource;

	if (!resource)
		return NULL;

	if (G->mTextureOverrideMap.empty())
		goto out_release_resource;

	EnterCriticalSection(&G->mCriticalSection);
		hash = GetResourceHash(resource);
	LeaveCriticalSection(&G->mCriticalSection);
	if (!hash)
		goto out_release_resource;

	state->mHackerContext->FrameAnalysisLog("3DMigoto   found texture hash = %08llx\n", hash);

	i = G->mTextureOverrideMap.find(hash);
	if (i == G->mTextureOverrideMap.end())
		goto out_release_resource;

	ret = &i->second;

out_release_resource:
	if (resource)
		resource->Release();
	if (view)
		view->Release();
	return ret;
}

static bool IsCoersionToStructuredBufferRequired(ID3D11View *view, UINT stride,
		UINT offset, DXGI_FORMAT format, D3D11_BIND_FLAG bind_flags)
{
	// If we are copying a vertex buffer into a shader resource we need to
	// convert it into a structured buffer, which requires a flag set when
	// creating the new resource as well as changes in the view.
	//
	// This function tries to detect this situation without explicitly
	// checking that the source was a vertex buffer - that way, similar
	// situations should work as well, such as when using an intermediate
	// resource.

	// If we are copying from a resource that had a view we will use it's
	// description to work out what we need to do (or we will, once I write
	// that code)
	if (view)
		return false;

	// If we know the format there's no need to be structured
	if (format != DXGI_FORMAT_UNKNOWN)
		return false;

	// We need to know the stride to be structured:
	if (stride == 0)
		return false;

	// Structured buffers only make sense for certain views:
	return !!(bind_flags & (D3D11_BIND_SHADER_RESOURCE |
			D3D11_BIND_RENDER_TARGET |
			D3D11_BIND_DEPTH_STENCIL |
			D3D11_BIND_UNORDERED_ACCESS));
}

static ID3D11Buffer *RecreateCompatibleBuffer(
		wstring *ini_line,
		ResourceCopyTarget *dst, // May be NULL
		ID3D11Buffer *src_resource,
		ID3D11Buffer *dst_resource,
		ResourcePool *resource_pool,
		ID3D11View *src_view,
		D3D11_BIND_FLAG bind_flags,
		CommandListState *state,
		UINT stride,
		UINT offset,
		DXGI_FORMAT format,
		UINT *buf_dst_size)
{
	D3D11_BUFFER_DESC new_desc;
	ID3D11Buffer *buffer = NULL;
	UINT dst_size;

	src_resource->GetDesc(&new_desc);
	new_desc.BindFlags = bind_flags;

	if (dst && dst->type == ResourceCopyTargetType::CPU) {
		new_desc.Usage = D3D11_USAGE_STAGING;
		new_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	} else {
		new_desc.Usage = D3D11_USAGE_DEFAULT;
		new_desc.CPUAccessFlags = 0;
	}

	// TODO: Add a keyword to allow raw views:
	// D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS

	if (bind_flags & D3D11_BIND_CONSTANT_BUFFER) {
		// Constant buffers have additional limitations. The size must
		// be a multiple of 16, so round up if necessary, and it cannot
		// be larger than 4096 x 4 component x 4 byte constants.
		dst_size = (new_desc.ByteWidth + 15) & ~0xf;
		dst_size = min(dst_size, D3D11_REQ_CONSTANT_BUFFER_ELEMENT_COUNT * 16);

		// Constant buffers cannot be structured, so clear that flag:
		new_desc.MiscFlags &= ~D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		// XXX: Should we clear StructureByteStride? Seems to work ok
		// without clearing that.

		// If the size of the new resource doesn't match the old or
		// there is an offset we will have to perform a region copy
		// instead of a regular copy:
		if (offset || dst_size != new_desc.ByteWidth) {
			// It might be temping to take the offset into account
			// here and make the buffer only as large as it need to
			// be, but it's possible that the source offset might
			// change much more often than the source buffer (just
			// a guess), which could potentially lead us to
			// constantly recreating the destination buffer.

			// Note down the size of the source and destination:
			*buf_dst_size = dst_size;
			new_desc.ByteWidth = dst_size;
		}
	} else if (IsCoersionToStructuredBufferRequired(src_view, stride, offset, format, bind_flags)) {
		new_desc.MiscFlags |= D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
		new_desc.StructureByteStride = stride;

		// A structured buffer needs to be a multiple of it's stride,
		// which may not be the case if we're converting a buffer to
		// one. Round it down:
		dst_size = new_desc.ByteWidth / stride * stride;
		// For now always using the region copy if there's an offset.
		// We might not need to do that if the offset is aligned to the
		// stride (although we would need to recreate the view every
		// time it changed), but for now it seems safest to use the
		// region copy method whenever there is an offset:
		if (offset || dst_size != new_desc.ByteWidth) {
			*buf_dst_size = dst_size;
			new_desc.ByteWidth = dst_size;
		}
	} else if (!src_view && offset) {
		// No source view but we do have an offset - use the region
		// copy to knock out the offset. We can probably assume the
		// original resource met all the size and alignment
		// constraints, so we shouldn't need to resize it.
		*buf_dst_size = new_desc.ByteWidth;
	}

	if (dst && dst->type == ResourceCopyTargetType::CUSTOM_RESOURCE)
		dst->custom_resource->OverrideBufferDesc(&new_desc);

	return GetResourceFromPool<ID3D11Buffer, D3D11_BUFFER_DESC, &ID3D11Device::CreateBuffer>
		(ini_line, src_resource, dst_resource, resource_pool, state, &new_desc);
}

static DXGI_FORMAT MakeTypeless(DXGI_FORMAT fmt)
{
	switch(fmt)
	{
		case DXGI_FORMAT_R32G32B32A32_FLOAT:
		case DXGI_FORMAT_R32G32B32A32_UINT:
		case DXGI_FORMAT_R32G32B32A32_SINT:
			return DXGI_FORMAT_R32G32B32A32_TYPELESS;

		case DXGI_FORMAT_R32G32B32_FLOAT:
		case DXGI_FORMAT_R32G32B32_UINT:
		case DXGI_FORMAT_R32G32B32_SINT:
			return DXGI_FORMAT_R32G32B32_TYPELESS;

		case DXGI_FORMAT_R16G16B16A16_FLOAT:
		case DXGI_FORMAT_R16G16B16A16_UNORM:
		case DXGI_FORMAT_R16G16B16A16_UINT:
		case DXGI_FORMAT_R16G16B16A16_SNORM:
		case DXGI_FORMAT_R16G16B16A16_SINT:
			return DXGI_FORMAT_R16G16B16A16_TYPELESS;

		case DXGI_FORMAT_R32G32_FLOAT:
		case DXGI_FORMAT_R32G32_UINT:
		case DXGI_FORMAT_R32G32_SINT:
			return DXGI_FORMAT_R32G32_TYPELESS;

		case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
		case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
		case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
			return DXGI_FORMAT_R32G8X24_TYPELESS;

		case DXGI_FORMAT_R10G10B10A2_UNORM:
		case DXGI_FORMAT_R10G10B10A2_UINT:
			return DXGI_FORMAT_R10G10B10A2_TYPELESS;

		case DXGI_FORMAT_R8G8B8A8_UNORM:
		case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
		case DXGI_FORMAT_R8G8B8A8_UINT:
		case DXGI_FORMAT_R8G8B8A8_SNORM:
		case DXGI_FORMAT_R8G8B8A8_SINT:
			return DXGI_FORMAT_R8G8B8A8_TYPELESS;

		case DXGI_FORMAT_R16G16_FLOAT:
		case DXGI_FORMAT_R16G16_UNORM:
		case DXGI_FORMAT_R16G16_UINT:
		case DXGI_FORMAT_R16G16_SNORM:
		case DXGI_FORMAT_R16G16_SINT:
			return DXGI_FORMAT_R16G16_TYPELESS;

		case DXGI_FORMAT_D32_FLOAT:
		case DXGI_FORMAT_R32_FLOAT:
		case DXGI_FORMAT_R32_UINT:
		case DXGI_FORMAT_R32_SINT:
			return DXGI_FORMAT_R32_TYPELESS;

		case DXGI_FORMAT_D24_UNORM_S8_UINT:
		case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
		case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
			return DXGI_FORMAT_R24G8_TYPELESS;

		case DXGI_FORMAT_R8G8_UNORM:
		case DXGI_FORMAT_R8G8_UINT:
		case DXGI_FORMAT_R8G8_SNORM:
		case DXGI_FORMAT_R8G8_SINT:
			return DXGI_FORMAT_R8G8_TYPELESS;

		case DXGI_FORMAT_R16_FLOAT:
		case DXGI_FORMAT_D16_UNORM:
		case DXGI_FORMAT_R16_UNORM:
		case DXGI_FORMAT_R16_UINT:
		case DXGI_FORMAT_R16_SNORM:
		case DXGI_FORMAT_R16_SINT:
			return DXGI_FORMAT_R16_TYPELESS;

		case DXGI_FORMAT_R8_UNORM:
		case DXGI_FORMAT_R8_UINT:
		case DXGI_FORMAT_R8_SNORM:
		case DXGI_FORMAT_R8_SINT:
			return DXGI_FORMAT_R8_TYPELESS;

		case DXGI_FORMAT_BC1_UNORM:
		case DXGI_FORMAT_BC1_UNORM_SRGB:
			return DXGI_FORMAT_BC1_TYPELESS;

		case DXGI_FORMAT_BC2_UNORM:
		case DXGI_FORMAT_BC2_UNORM_SRGB:
			return DXGI_FORMAT_BC2_TYPELESS;

		case DXGI_FORMAT_BC3_UNORM:
		case DXGI_FORMAT_BC3_UNORM_SRGB:
			return DXGI_FORMAT_BC3_TYPELESS;

		case DXGI_FORMAT_BC4_UNORM:
		case DXGI_FORMAT_BC4_SNORM:
			return DXGI_FORMAT_BC4_TYPELESS;

		case DXGI_FORMAT_BC5_UNORM:
		case DXGI_FORMAT_BC5_SNORM:
			return DXGI_FORMAT_BC5_TYPELESS;

		case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
			return DXGI_FORMAT_B8G8R8A8_TYPELESS;

		case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
			return DXGI_FORMAT_B8G8R8X8_TYPELESS;

		case DXGI_FORMAT_BC6H_UF16:
		case DXGI_FORMAT_BC6H_SF16:
			return DXGI_FORMAT_BC6H_TYPELESS;

		case DXGI_FORMAT_BC7_UNORM:
		case DXGI_FORMAT_BC7_UNORM_SRGB:
			return DXGI_FORMAT_BC7_TYPELESS;

		case DXGI_FORMAT_R11G11B10_FLOAT:
		default:
			return fmt;
	}
}

static DXGI_FORMAT MakeDSVFormat(DXGI_FORMAT fmt)
{
	switch(fmt)
	{
		case DXGI_FORMAT_R32G8X24_TYPELESS:
		case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
		case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
		case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
			return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;

		case DXGI_FORMAT_R32_TYPELESS:
		case DXGI_FORMAT_D32_FLOAT:
		case DXGI_FORMAT_R32_FLOAT:
			return DXGI_FORMAT_D32_FLOAT;

		case DXGI_FORMAT_R24G8_TYPELESS:
		case DXGI_FORMAT_D24_UNORM_S8_UINT:
		case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
		case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
			return DXGI_FORMAT_D24_UNORM_S8_UINT;

		case DXGI_FORMAT_R16_TYPELESS:
		case DXGI_FORMAT_D16_UNORM:
		case DXGI_FORMAT_R16_UNORM:
			return DXGI_FORMAT_D16_UNORM;

		default:
			return EnsureNotTypeless(fmt);
	}
}

static DXGI_FORMAT MakeNonDSVFormat(DXGI_FORMAT fmt)
{
	// TODO: Add a keyword to return the stencil side of a combined
	// depth/stencil resource instead of the depth side
	switch(fmt)
	{
		case DXGI_FORMAT_R32G8X24_TYPELESS:
		case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
		case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
		case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
			return DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS;

		case DXGI_FORMAT_R32_TYPELESS:
		case DXGI_FORMAT_D32_FLOAT:
		case DXGI_FORMAT_R32_FLOAT:
			return DXGI_FORMAT_R32_FLOAT;

		case DXGI_FORMAT_R24G8_TYPELESS:
		case DXGI_FORMAT_D24_UNORM_S8_UINT:
		case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
		case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
			return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;

		case DXGI_FORMAT_R16_TYPELESS:
		case DXGI_FORMAT_D16_UNORM:
		case DXGI_FORMAT_R16_UNORM:
			return DXGI_FORMAT_R16_UNORM;

		default:
			return EnsureNotTypeless(fmt);
	}
}

// MSAA resolving only makes sense for Texture2D types, and the SampleDesc
// entry only exists in those. Use template specialisation so we don't have to
// duplicate the entire RecreateCompatibleTexture() routine for such a small
// difference.
template <typename DescType>
static void Texture2DDescResolveMSAA(DescType *desc) {}
template <>
static void Texture2DDescResolveMSAA(D3D11_TEXTURE2D_DESC *desc)
{
	desc->SampleDesc.Count = 1;
	desc->SampleDesc.Quality = 0;
}

template <typename ResourceType,
	 typename DescType,
	HRESULT (__stdcall ID3D11Device::*CreateTexture)(THIS_
	      const DescType *pDesc,
	      const D3D11_SUBRESOURCE_DATA *pInitialData,
	      ResourceType **ppTexture)
	>
static ResourceType* RecreateCompatibleTexture(
		wstring *ini_line,
		ResourceCopyTarget *dst, // May be NULL
		ResourceType *src_resource,
		ResourceType *dst_resource,
		ResourcePool *resource_pool,
		D3D11_BIND_FLAG bind_flags,
		CommandListState *state,
		StereoHandle mStereoHandle,
		ResourceCopyOptions options)
{
	DescType new_desc;

	src_resource->GetDesc(&new_desc);
	new_desc.BindFlags = bind_flags;

	if (dst && dst->type == ResourceCopyTargetType::CPU) {
		new_desc.Usage = D3D11_USAGE_STAGING;
		new_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	} else {
		new_desc.Usage = D3D11_USAGE_DEFAULT;
		new_desc.CPUAccessFlags = 0;
	}

	// New strategy - we make the new resources typeless whenever possible
	// and will fill the type back in in the view instead. This gives us
	// more flexibility with depth/stencil formats which need different
	// types depending on where they are bound in the pipeline. This also
	// helps with certain MSAA resources that may not be possible to create
	// if we change the type to a R*X* format.
	new_desc.Format = MakeTypeless(new_desc.Format);

	if (options & ResourceCopyOptions::STEREO2MONO)
		new_desc.Width *= 2;

	// TODO: reverse_blit might need to imply resolve_msaa:
	if (options & ResourceCopyOptions::RESOLVE_MSAA)
		Texture2DDescResolveMSAA(&new_desc);

	// XXX: Any changes needed in new_desc.MiscFlags?
	//
	// D3D11_RESOURCE_MISC_GENERATE_MIPS requires specific bind flags (both
	// shader resource AND render target must be set) and might prevent us
	// from creating the resource otherwise. Since we don't need to
	// generate mip-maps just clear it out:
	new_desc.MiscFlags &= ~D3D11_RESOURCE_MISC_GENERATE_MIPS;

	if (dst && dst->type == ResourceCopyTargetType::CUSTOM_RESOURCE)
		dst->custom_resource->OverrideTexDesc(&new_desc);

	return GetResourceFromPool<ResourceType, DescType, CreateTexture>
		(ini_line, src_resource, dst_resource, resource_pool, state, &new_desc);
}

static void RecreateCompatibleResource(
		wstring *ini_line,
		ResourceCopyTarget *dst, // May be NULL
		ID3D11Resource *src_resource,
		ID3D11Resource **dst_resource,
		ResourcePool *resource_pool,
		ID3D11View *src_view,
		ID3D11View **dst_view,
		CommandListState *state,
		StereoHandle mStereoHandle,
		ResourceCopyOptions options,
		UINT stride,
		UINT offset,
		DXGI_FORMAT format,
		UINT *buf_dst_size)
{
	NVAPI_STEREO_SURFACECREATEMODE orig_mode = NVAPI_STEREO_SURFACECREATEMODE_AUTO;
	D3D11_RESOURCE_DIMENSION src_dimension;
	D3D11_RESOURCE_DIMENSION dst_dimension;
	D3D11_BIND_FLAG bind_flags = (D3D11_BIND_FLAG)0;
	ID3D11Resource *res = NULL;
	bool restore_create_mode = false;

	if (dst)
		bind_flags = dst->BindFlags();

	src_resource->GetType(&src_dimension);
	if (*dst_resource) {
		(*dst_resource)->GetType(&dst_dimension);
		if (src_dimension != dst_dimension) {
			LogInfo("Resource type changed %S\n", ini_line->c_str());

			(*dst_resource)->Release();
			if (dst_view && *dst_view)
				(*dst_view)->Release();

			*dst_resource = NULL;
			if (dst_view)
				*dst_view = NULL;
		}
	}

	if (options & ResourceCopyOptions::CREATEMODE_MASK) {
		NvAPI_Stereo_GetSurfaceCreationMode(mStereoHandle, &orig_mode);
		restore_create_mode = true;

		// STEREO2MONO will force the final destination to mono since
		// it is in the CREATEMODE_MASK, but is not STEREO. It also
		// creates an additional intermediate resource that will be
		// forced to STEREO.

		if (options & ResourceCopyOptions::STEREO) {
			NvAPI_Stereo_SetSurfaceCreationMode(mStereoHandle,
					NVAPI_STEREO_SURFACECREATEMODE_FORCESTEREO);
		} else {
			NvAPI_Stereo_SetSurfaceCreationMode(mStereoHandle,
					NVAPI_STEREO_SURFACECREATEMODE_FORCEMONO);
		}
	} else if (dst && dst->type == ResourceCopyTargetType::CUSTOM_RESOURCE) {
		restore_create_mode = dst->custom_resource->OverrideSurfaceCreationMode(mStereoHandle, &orig_mode);
	}

	switch (src_dimension) {
		case D3D11_RESOURCE_DIMENSION_BUFFER:
			res = RecreateCompatibleBuffer(ini_line, dst, (ID3D11Buffer*)src_resource, (ID3D11Buffer*)*dst_resource,
				resource_pool, src_view, bind_flags, state, stride, offset, format, buf_dst_size);
			break;
		case D3D11_RESOURCE_DIMENSION_TEXTURE1D:
			res = RecreateCompatibleTexture<ID3D11Texture1D, D3D11_TEXTURE1D_DESC, &ID3D11Device::CreateTexture1D>
				(ini_line, dst, (ID3D11Texture1D*)src_resource, (ID3D11Texture1D*)*dst_resource, resource_pool,
				 bind_flags, state, mStereoHandle, options);
			break;
		case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
			res = RecreateCompatibleTexture<ID3D11Texture2D, D3D11_TEXTURE2D_DESC, &ID3D11Device::CreateTexture2D>
				(ini_line, dst, (ID3D11Texture2D*)src_resource, (ID3D11Texture2D*)*dst_resource, resource_pool,
				 bind_flags, state, mStereoHandle, options);
			break;
		case D3D11_RESOURCE_DIMENSION_TEXTURE3D:
			res = RecreateCompatibleTexture<ID3D11Texture3D, D3D11_TEXTURE3D_DESC, &ID3D11Device::CreateTexture3D>
				(ini_line, dst, (ID3D11Texture3D*)src_resource, (ID3D11Texture3D*)*dst_resource, resource_pool,
				 bind_flags, state, mStereoHandle, options);
			break;
	}

	if (restore_create_mode)
		NvAPI_Stereo_SetSurfaceCreationMode(mStereoHandle, orig_mode);

	if (res) {
		if (*dst_resource)
			(*dst_resource)->Release();
		if (dst_view && *dst_view)
			(*dst_view)->Release();

		*dst_resource = res;
		if (dst_view)
			*dst_view = NULL;
	}
}

template <typename DescType>
static void FillOutBufferDescCommon(DescType *desc, UINT stride,
		UINT offset, UINT buf_src_size)
{
	// The documentation on the buffer part of the description is
	// misleading.
	//
	// There are two unions with two possible parameters each which
	// are documented in MSDN, but DX11 never uses ElementWidth
	// (which is determined by either the format, or buffer's
	// StructureByteStride), only NumElements.
	//
	// My reading of FirstElement/ElementOffset sound like they are
	// the same thing, but one is in bytes and the other is in
	// elements - only the names seem backwards compared to the
	// description in the documentation. Research suggests DX11
	// only uses multiples of the element size (since it's a union,
	// it shouldn't matter which name we use).
	//
	// XXX: At the moment we are relying on the region copy to have
	// knocked out the offset for us. We could alternatively do it
	// here (and the below should work), but we would need to
	// create a new view every time the offset changes.
	if (stride) {
		desc->Buffer.FirstElement = offset / stride;
		desc->Buffer.NumElements = (buf_src_size - offset) / stride;
	} else {
		desc->Buffer.FirstElement = 0;
		desc->Buffer.NumElements = 1;
	}
}

static D3D11_SHADER_RESOURCE_VIEW_DESC* FillOutBufferDesc(
		D3D11_SHADER_RESOURCE_VIEW_DESC *desc, UINT stride,
		UINT offset, UINT buf_src_size)
{
	// TODO: Also handle BUFFEREX for raw buffers
	desc->ViewDimension = D3D11_SRV_DIMENSION_BUFFER;

	FillOutBufferDescCommon<D3D11_SHADER_RESOURCE_VIEW_DESC>(desc, stride, offset, buf_src_size);
	return desc;
}
static D3D11_RENDER_TARGET_VIEW_DESC* FillOutBufferDesc(
		D3D11_RENDER_TARGET_VIEW_DESC *desc, UINT stride,
		UINT offset, UINT buf_src_size)
{
	desc->ViewDimension = D3D11_RTV_DIMENSION_BUFFER;

	FillOutBufferDescCommon<D3D11_RENDER_TARGET_VIEW_DESC>(desc, stride, offset, buf_src_size);
	return desc;
}
static D3D11_UNORDERED_ACCESS_VIEW_DESC* FillOutBufferDesc(
		D3D11_UNORDERED_ACCESS_VIEW_DESC *desc, UINT stride,
		UINT offset, UINT buf_src_size)
{
	desc->ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
	// TODO Support buffer UAV flags for append, counter and raw buffers.
	desc->Buffer.Flags = 0;

	FillOutBufferDescCommon<D3D11_UNORDERED_ACCESS_VIEW_DESC>(desc, stride, offset, buf_src_size);
	return desc;
}
static D3D11_DEPTH_STENCIL_VIEW_DESC* FillOutBufferDesc(
		D3D11_DEPTH_STENCIL_VIEW_DESC *desc, UINT stride,
		UINT offset, UINT buf_src_size)
{
	// Depth views don't support buffers:
	return NULL;
}


// This is a hell of a lot of duplicated code, mostly thanks to DX using
// different names for the same thing in a slightly different type, and pretty
// much all this is only needed for depth/stencil format conversions. It would
// be nice to refactor this somehow. TODO: For now we are creating a view of
// the entire resource, but it would make sense to use information from the
// source view if available instead.
static D3D11_SHADER_RESOURCE_VIEW_DESC* FillOutTex1DDesc(
		D3D11_SHADER_RESOURCE_VIEW_DESC *view_desc,
		D3D11_TEXTURE1D_DESC *resource_desc, DXGI_FORMAT format)
{
	view_desc->Format = MakeNonDSVFormat(format);

	if (resource_desc->ArraySize == 1) {
		view_desc->ViewDimension = D3D11_SRV_DIMENSION_TEXTURE1D;
		view_desc->Texture1D.MostDetailedMip = 0;
		view_desc->Texture1D.MipLevels = -1;
	} else {
		view_desc->ViewDimension = D3D11_SRV_DIMENSION_TEXTURE1DARRAY;
		view_desc->Texture1DArray.MostDetailedMip = 0;
		view_desc->Texture1DArray.MipLevels = -1;
		view_desc->Texture1DArray.FirstArraySlice = 0;
		view_desc->Texture1DArray.ArraySize = resource_desc->ArraySize;
	}

	return view_desc;
}
static D3D11_RENDER_TARGET_VIEW_DESC* FillOutTex1DDesc(
		D3D11_RENDER_TARGET_VIEW_DESC *view_desc,
		D3D11_TEXTURE1D_DESC *resource_desc, DXGI_FORMAT format)
{
	view_desc->Format = MakeNonDSVFormat(format);

	if (resource_desc->ArraySize == 1) {
		view_desc->ViewDimension = D3D11_RTV_DIMENSION_TEXTURE1D;
		view_desc->Texture1D.MipSlice = 0;
	} else {
		view_desc->ViewDimension = D3D11_RTV_DIMENSION_TEXTURE1DARRAY;
		view_desc->Texture1DArray.MipSlice = 0;
		view_desc->Texture1DArray.FirstArraySlice = 0;
		view_desc->Texture1DArray.ArraySize = resource_desc->ArraySize;
	}

	return view_desc;
}
static D3D11_DEPTH_STENCIL_VIEW_DESC* FillOutTex1DDesc(
		D3D11_DEPTH_STENCIL_VIEW_DESC *view_desc,
		D3D11_TEXTURE1D_DESC *resource_desc, DXGI_FORMAT format)
{
	view_desc->Format = MakeDSVFormat(format);

	if (resource_desc->ArraySize == 1) {
		view_desc->ViewDimension = D3D11_DSV_DIMENSION_TEXTURE1D;
		view_desc->Texture1D.MipSlice = 0;
	} else {
		view_desc->ViewDimension = D3D11_DSV_DIMENSION_TEXTURE1DARRAY;
		view_desc->Texture1DArray.MipSlice = 0;
		view_desc->Texture1DArray.FirstArraySlice = 0;
		view_desc->Texture1DArray.ArraySize = resource_desc->ArraySize;
	}

	return view_desc;
}
static D3D11_UNORDERED_ACCESS_VIEW_DESC* FillOutTex1DDesc(
		D3D11_UNORDERED_ACCESS_VIEW_DESC *view_desc,
		D3D11_TEXTURE1D_DESC *resource_desc, DXGI_FORMAT format)
{
	view_desc->Format = MakeNonDSVFormat(format);

	if (resource_desc->ArraySize == 1) {
		view_desc->ViewDimension = D3D11_UAV_DIMENSION_TEXTURE1D;
		view_desc->Texture1D.MipSlice = 0;
	} else {
		view_desc->ViewDimension = D3D11_UAV_DIMENSION_TEXTURE1DARRAY;
		view_desc->Texture1DArray.MipSlice = 0;
		view_desc->Texture1DArray.FirstArraySlice = 0;
		view_desc->Texture1DArray.ArraySize = resource_desc->ArraySize;
	}

	return view_desc;
}
static D3D11_SHADER_RESOURCE_VIEW_DESC* FillOutTex2DDesc(
		D3D11_SHADER_RESOURCE_VIEW_DESC *view_desc,
		D3D11_TEXTURE2D_DESC *resource_desc, DXGI_FORMAT format)
{
	view_desc->Format = MakeNonDSVFormat(format);

	if (resource_desc->MiscFlags & D3D11_RESOURCE_MISC_TEXTURECUBE) {
		if (resource_desc->ArraySize == 1) {
			view_desc->ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
			view_desc->TextureCube.MostDetailedMip = 0;
			view_desc->TextureCube.MipLevels = -1;
		} else {
			view_desc->ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBEARRAY;
			view_desc->TextureCubeArray.MostDetailedMip = 0;
			view_desc->TextureCubeArray.MipLevels = -1;
			view_desc->TextureCubeArray.First2DArrayFace = 0; // FIXME: Get from original view
			view_desc->TextureCubeArray.NumCubes = resource_desc->ArraySize / 6;
		}
	} else if (resource_desc->SampleDesc.Count == 1) {
		if (resource_desc->ArraySize == 1) {
			view_desc->ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			view_desc->Texture2D.MostDetailedMip = 0;
			view_desc->Texture2D.MipLevels = -1;
		} else {
			view_desc->ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DARRAY;
			view_desc->Texture2DArray.MostDetailedMip = 0;
			view_desc->Texture2DArray.MipLevels = -1;
			view_desc->Texture2DArray.FirstArraySlice = 0;
			view_desc->Texture2DArray.ArraySize = resource_desc->ArraySize;
		}
	} else {
		if (resource_desc->ArraySize == 1) {
			view_desc->ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DMS;
		} else {
			view_desc->ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2DMSARRAY;
			view_desc->Texture2DMSArray.FirstArraySlice = 0;
			view_desc->Texture2DMSArray.ArraySize = resource_desc->ArraySize;
		}
	}

	return view_desc;
}
static D3D11_RENDER_TARGET_VIEW_DESC* FillOutTex2DDesc(
		D3D11_RENDER_TARGET_VIEW_DESC *view_desc,
		D3D11_TEXTURE2D_DESC *resource_desc, DXGI_FORMAT format)
{
	view_desc->Format = MakeNonDSVFormat(format);

	if (resource_desc->SampleDesc.Count == 1) {
		if (resource_desc->ArraySize == 1) {
			view_desc->ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
			view_desc->Texture2D.MipSlice = 0;
		} else {
			view_desc->ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DARRAY;
			view_desc->Texture2DArray.MipSlice = 0;
			view_desc->Texture2DArray.FirstArraySlice = 0;
			view_desc->Texture2DArray.ArraySize = resource_desc->ArraySize;
		}
	} else {
		if (resource_desc->ArraySize == 1) {
			view_desc->ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMS;
		} else {
			view_desc->ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2DMSARRAY;
			view_desc->Texture2DMSArray.FirstArraySlice = 0;
			view_desc->Texture2DMSArray.ArraySize = resource_desc->ArraySize;
		}
	}

	return view_desc;
}
static D3D11_DEPTH_STENCIL_VIEW_DESC* FillOutTex2DDesc(
		D3D11_DEPTH_STENCIL_VIEW_DESC *view_desc,
		D3D11_TEXTURE2D_DESC *resource_desc, DXGI_FORMAT format)
{
	view_desc->Format = MakeDSVFormat(format);
	view_desc->Flags = 0; // TODO: Fill in from old view, and add keyword to override

	if (resource_desc->SampleDesc.Count == 1) {
		if (resource_desc->ArraySize == 1) {
			view_desc->ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
			view_desc->Texture2D.MipSlice = 0;
		} else {
			view_desc->ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
			view_desc->Texture2DArray.MipSlice = 0;
			view_desc->Texture2DArray.FirstArraySlice = 0;
			view_desc->Texture2DArray.ArraySize = resource_desc->ArraySize;
		}
	} else {
		if (resource_desc->ArraySize == 1) {
			view_desc->ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMS;
		} else {
			view_desc->ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DMSARRAY;
			view_desc->Texture2DMSArray.FirstArraySlice = 0;
			view_desc->Texture2DMSArray.ArraySize = resource_desc->ArraySize;
		}
	}

	return view_desc;
}
static D3D11_UNORDERED_ACCESS_VIEW_DESC* FillOutTex2DDesc(
		D3D11_UNORDERED_ACCESS_VIEW_DESC *view_desc,
		D3D11_TEXTURE2D_DESC *resource_desc, DXGI_FORMAT format)
{
	view_desc->Format = MakeNonDSVFormat(format);

	if (resource_desc->ArraySize == 1) {
		view_desc->ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
		view_desc->Texture2D.MipSlice = 0;
	} else {
		view_desc->ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2DARRAY;
		view_desc->Texture2DArray.MipSlice = 0;
		view_desc->Texture2DArray.FirstArraySlice = 0;
		view_desc->Texture2DArray.ArraySize = resource_desc->ArraySize;
	}

	return view_desc;
}
static D3D11_SHADER_RESOURCE_VIEW_DESC* FillOutTex3DDesc(
		D3D11_SHADER_RESOURCE_VIEW_DESC *view_desc,
		D3D11_TEXTURE3D_DESC *resource_desc, DXGI_FORMAT format)
{
	view_desc->Format = MakeNonDSVFormat(format);

	view_desc->ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
	view_desc->Texture3D.MostDetailedMip = 0;
	view_desc->Texture3D.MipLevels = -1;

	return view_desc;
}
static D3D11_RENDER_TARGET_VIEW_DESC* FillOutTex3DDesc(
		D3D11_RENDER_TARGET_VIEW_DESC *view_desc,
		D3D11_TEXTURE3D_DESC *resource_desc, DXGI_FORMAT format)
{
	view_desc->Format = MakeNonDSVFormat(format);

	view_desc->ViewDimension = D3D11_RTV_DIMENSION_TEXTURE3D;
	view_desc->Texture3D.MipSlice = 0;
	view_desc->Texture3D.FirstWSlice = 0;
	view_desc->Texture3D.WSize = -1;

	return view_desc;
}
static D3D11_DEPTH_STENCIL_VIEW_DESC* FillOutTex3DDesc(
		D3D11_DEPTH_STENCIL_VIEW_DESC *view_desc,
		D3D11_TEXTURE3D_DESC *resource_desc, DXGI_FORMAT format)
{
	// DSV cannot be a Texture3D

	return NULL;
}
static D3D11_UNORDERED_ACCESS_VIEW_DESC* FillOutTex3DDesc(
		D3D11_UNORDERED_ACCESS_VIEW_DESC *view_desc,
		D3D11_TEXTURE3D_DESC *resource_desc, DXGI_FORMAT format)
{
	view_desc->Format = MakeNonDSVFormat(format);

	view_desc->ViewDimension = D3D11_UAV_DIMENSION_TEXTURE3D;
	view_desc->Texture3D.MipSlice = 0;
	view_desc->Texture3D.FirstWSlice = 0;
	view_desc->Texture3D.WSize = -1;

	return view_desc;
}


template <typename ViewType,
	 typename DescType,
	 HRESULT (__stdcall ID3D11Device::*CreateView)(THIS_
			 ID3D11Resource *pResource,
			 const DescType *pDesc,
			 ViewType **ppView)
	>
static ID3D11View* _CreateCompatibleView(
		ID3D11Resource *resource,
		CommandListState *state,
		UINT stride,
		UINT offset,
		DXGI_FORMAT format,
		UINT buf_src_size)
{
	D3D11_RESOURCE_DIMENSION dimension;
	ID3D11Texture1D *tex1d;
	ID3D11Texture2D *tex2d;
	ID3D11Texture3D *tex3d;
	D3D11_TEXTURE1D_DESC tex1d_desc;
	D3D11_TEXTURE2D_DESC tex2d_desc;
	D3D11_TEXTURE3D_DESC tex3d_desc;
	ViewType *view = NULL;
	DescType view_desc, *pDesc = NULL;
	HRESULT hr;

	resource->GetType(&dimension);
	switch(dimension) {
		case D3D11_RESOURCE_DIMENSION_BUFFER:
			// In the case of a buffer type resource we must specify the
			// description as DirectX doesn't have enough information from the
			// buffer alone to create a view.

			view_desc.Format = format;

			pDesc = FillOutBufferDesc(&view_desc, stride, offset, buf_src_size);

			// This should already handle things like:
			// - Copying a vertex buffer to a SRV or constant buffer
			// - Copying an index buffer to a SRV
			// - Copying structured buffers
			// - Copying regular buffers

			// TODO: Support UAV flags like append/consume and SRV BufferEx views
			break;

		// We now also fill out the view description for textures as
		// well. We used to create fully typed resources and leave this
		// up to DX, but there were some situations where that would
		// not work (depth buffers need different types depending on
		// where they are bound, some MSAA resources could not be
		// created), so we now create typeless resources and therefore
		// have to fill out the view description to set the type. We
		// could potentially do this for only the cases where we need
		// (i.e. depth buffer formats), but I want to do this for
		// everything because it's so damn overly complex that typos
		// are ensured so this way it will at least get more exposure
		// and I can find the bugs sooner:
		case D3D11_RESOURCE_DIMENSION_TEXTURE1D:
			tex1d = (ID3D11Texture1D*)resource;
			tex1d->GetDesc(&tex1d_desc);
			pDesc = FillOutTex1DDesc(&view_desc, &tex1d_desc, format);
			break;
		case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
			tex2d = (ID3D11Texture2D*)resource;
			tex2d->GetDesc(&tex2d_desc);
			pDesc = FillOutTex2DDesc(&view_desc, &tex2d_desc, format);
			break;
		case D3D11_RESOURCE_DIMENSION_TEXTURE3D:
			tex3d = (ID3D11Texture3D*)resource;
			tex3d->GetDesc(&tex3d_desc);
			pDesc = FillOutTex3DDesc(&view_desc, &tex3d_desc, format);
			break;
	}

	hr = (state->mOrigDevice1->*CreateView)(resource, pDesc, &view);
	if (FAILED(hr)) {
		LogInfo("Resource copy CreateCompatibleView failed: %x\n", hr);
		if (pDesc)
			LogViewDesc(pDesc);
		LogResourceDesc(resource);
		return NULL;
	}

	if (pDesc)
		LogDebugViewDesc(pDesc);

	return view;
}

static ID3D11View* CreateCompatibleView(
		ResourceCopyTarget *dst,
		ID3D11Resource *resource,
		CommandListState *state,
		UINT stride,
		UINT offset,
		DXGI_FORMAT format,
		UINT buf_src_size)
{
	switch (dst->type) {
		case ResourceCopyTargetType::SHADER_RESOURCE:
			return _CreateCompatibleView<ID3D11ShaderResourceView,
			       D3D11_SHADER_RESOURCE_VIEW_DESC,
			       &ID3D11Device::CreateShaderResourceView>
				       (resource, state, stride, offset, format, buf_src_size);
		case ResourceCopyTargetType::RENDER_TARGET:
			return _CreateCompatibleView<ID3D11RenderTargetView,
			       D3D11_RENDER_TARGET_VIEW_DESC,
			       &ID3D11Device::CreateRenderTargetView>
				       (resource, state, stride, offset, format, buf_src_size);
		case ResourceCopyTargetType::DEPTH_STENCIL_TARGET:
			return _CreateCompatibleView<ID3D11DepthStencilView,
			       D3D11_DEPTH_STENCIL_VIEW_DESC,
			       &ID3D11Device::CreateDepthStencilView>
				       (resource, state, stride, offset, format, buf_src_size);
		case ResourceCopyTargetType::UNORDERED_ACCESS_VIEW:
			return _CreateCompatibleView<ID3D11UnorderedAccessView,
			       D3D11_UNORDERED_ACCESS_VIEW_DESC,
			       &ID3D11Device::CreateUnorderedAccessView>
				       (resource, state, stride, offset, format, buf_src_size);
	}
	return NULL;
}

static void SetViewportFromResource(CommandListState *state, ID3D11Resource *resource)
{
	D3D11_RESOURCE_DIMENSION dimension;
	ID3D11Texture1D *tex1d;
	ID3D11Texture2D *tex2d;
	ID3D11Texture3D *tex3d;
	D3D11_TEXTURE1D_DESC tex1d_desc;
	D3D11_TEXTURE2D_DESC tex2d_desc;
	D3D11_TEXTURE3D_DESC tex3d_desc;
	D3D11_VIEWPORT viewport = {0, 0, 0, 0, D3D11_MIN_DEPTH, D3D11_MAX_DEPTH};

	// TODO: Could handle mip-maps from a view like the CD3D11_VIEWPORT
	// constructor, but we aren't using them elsewhere so don't care yet.
	resource->GetType(&dimension);
	switch(dimension) {
		case D3D11_RESOURCE_DIMENSION_BUFFER:
			// TODO: Width = NumElements
			return;
		case D3D11_RESOURCE_DIMENSION_TEXTURE1D:
			tex1d = (ID3D11Texture1D*)resource;
			tex1d->GetDesc(&tex1d_desc);
			viewport.Width = (float)tex1d_desc.Width;
			break;
		case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
			tex2d = (ID3D11Texture2D*)resource;
			tex2d->GetDesc(&tex2d_desc);
			viewport.Width = (float)tex2d_desc.Width;
			viewport.Height = (float)tex2d_desc.Height;
			break;
		case D3D11_RESOURCE_DIMENSION_TEXTURE3D:
			tex3d = (ID3D11Texture3D*)resource;
			tex3d->GetDesc(&tex3d_desc);
			viewport.Width = (float)tex3d_desc.Width;
			viewport.Height = (float)tex3d_desc.Height;
	}

	state->mOrigContext1->RSSetViewports(1, &viewport);
}

ResourceCopyOperation::ResourceCopyOperation() :
	options(ResourceCopyOptions::INVALID),
	cached_resource(NULL),
	cached_view(NULL),
	stereo2mono_intermediate(NULL)
{}

ResourceCopyOperation::~ResourceCopyOperation()
{
	if (cached_resource)
		cached_resource->Release();

	if (cached_view)
		cached_view->Release();
}

ResourceStagingOperation::ResourceStagingOperation()
{
	dst.type = ResourceCopyTargetType::CPU;
	options = ResourceCopyOptions::COPY;
	staging = false;
	ini_line = L"  Beginning transfer to CPU...";
}

HRESULT ResourceStagingOperation::map(CommandListState *state, D3D11_MAPPED_SUBRESOURCE *mapping)
{
	if (!cached_resource)
		return E_FAIL;

	return state->mOrigContext1->Map(cached_resource, 0, D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, mapping);
}

void ResourceStagingOperation::unmap(CommandListState *state)
{
	if (cached_resource)
		state->mOrigContext1->Unmap(cached_resource, 0);
}

static void ResolveMSAA(ID3D11Resource *dst_resource, ID3D11Resource *src_resource, CommandListState *state)
{
	UINT item, level, index, support;
	D3D11_RESOURCE_DIMENSION dst_dimension;
	ID3D11Texture2D *src, *dst;
	D3D11_TEXTURE2D_DESC desc;
	DXGI_FORMAT fmt;
	HRESULT hr;

	dst_resource->GetType(&dst_dimension);
	if (dst_dimension != D3D11_RESOURCE_DIMENSION_TEXTURE2D)
		return;

	src = (ID3D11Texture2D*)src_resource;
	dst = (ID3D11Texture2D*)dst_resource;

	dst->GetDesc(&desc);
	fmt = EnsureNotTypeless(desc.Format);

	hr = state->mOrigDevice1->CheckFormatSupport( fmt, &support );
	if (FAILED(hr) || !(support & D3D11_FORMAT_SUPPORT_MULTISAMPLE_RESOLVE)) {
		// TODO: Implement a fallback using a SM5 shader to resolve it
		LogInfo("Resource copy cannot resolve MSAA format %d\n", fmt);
		return;
	}

	for (item = 0; item < desc.ArraySize; item++) {
		for (level = 0; level < desc.MipLevels; level++) {
			index = D3D11CalcSubresource(level, item, max(desc.MipLevels, 1));
			state->mOrigContext1->ResolveSubresource(dst, index, src, index, fmt);
		}
	}
}

static void ReverseStereoBlit(ID3D11Resource *dst_resource, ID3D11Resource *src_resource, CommandListState *state)
{
	NvAPI_Status nvret;
	D3D11_RESOURCE_DIMENSION src_dimension;
	ID3D11Texture2D *src;
	D3D11_TEXTURE2D_DESC srcDesc;
	UINT item, level, index, width, height;
	D3D11_BOX srcBox;
	int fallbackside, fallback = 0;

	src_resource->GetType(&src_dimension);
	if (src_dimension != D3D11_RESOURCE_DIMENSION_TEXTURE2D) {
		// TODO: I think it should be possible to do this with all
		// resource types (possibly including buffers from the
		// discovery of the stereo parameters in the cb12 slot), but I
		// need to test it and make sure it works first
		LogInfo("Resource copy: Reverse stereo blit not supported on resource type %d\n", src_dimension);
		return;
	}

	src = (ID3D11Texture2D*)src_resource;
	src->GetDesc(&srcDesc);

	// TODO: Resolve MSAA
	// TODO: Use intermediate resource if copying from a texture with depth buffer bind flags

	// If stereo is disabled the reverse stereo blit won't work and we
	// would end up with the destination only updated on the left, which
	// may lead to shaders reading stale or 0 data if they read from the
	// right hand side. Use the fallback path to copy the source to both
	// sides of the destination so that the right side will be up to date:
	fallback = state->mHackerDevice->mParamTextureManager.mActive ? 0 : 1;

	if (!fallback) {
		nvret = NvAPI_Stereo_ReverseStereoBlitControl(state->mHackerDevice->mStereoHandle, true);
		if (nvret != NVAPI_OK) {
			LogInfo("Resource copying failed to enable reverse stereo blit\n");
			// Fallback path: Copy 2D resource to both sides of the 2x
			// width destination
			fallback = 1;
		}
	}

	for (fallbackside = 0; fallbackside < 1 + fallback; fallbackside++) {

		// Set the source box as per the nvapi documentation:
		srcBox.left = 0;
		srcBox.top = 0;
		srcBox.front = 0;
		srcBox.right = width = srcDesc.Width;
		srcBox.bottom = height = srcDesc.Height;
		srcBox.back = 1;

		// Perform the reverse stereo blit on all sub-resources and mip-maps:
		for (item = 0; item < srcDesc.ArraySize; item++) {
			for (level = 0; level < srcDesc.MipLevels; level++) {
				index = D3D11CalcSubresource(level, item, max(srcDesc.MipLevels, 1));
				srcBox.right = width >> level;
				srcBox.bottom = height >> level;
				state->mOrigContext1->CopySubresourceRegion(dst_resource, index,
						fallbackside * srcBox.right, 0, 0,
						src, index, &srcBox);
			}
		}
	}

	if (!fallback)
		NvAPI_Stereo_ReverseStereoBlitControl(state->mHackerDevice->mStereoHandle, false);
}

static void SpecialCopyBufferRegion(ID3D11Resource *dst_resource,ID3D11Resource *src_resource,
		CommandListState *state, UINT stride, UINT *offset,
		UINT buf_src_size, UINT buf_dst_size)
{
	// We are copying a buffer for use in a constant buffer and the size of
	// the original buffer did not meet the constraints of a constant
	// buffer.
	D3D11_BOX src_box;

	// We want to copy from the offset to the end of the source buffer, but
	// cap it to the destination size to avoid "undefined behaviour". Keep
	// in mind that this is "right", not "size":
	src_box.left = *offset;
	src_box.right = min(buf_src_size, *offset + buf_dst_size);

	if (stride) {
		// If we are copying to a structured resource, the source box
		// must be a multiple of the stride, so round it down:
		src_box.right = (src_box.right - src_box.left) / stride * stride + src_box.left;
	}

	src_box.top = 0;
	src_box.bottom = 1;
	src_box.front = 0;
	src_box.back = 1;

	state->mOrigContext1->CopySubresourceRegion(dst_resource, 0, 0, 0, 0, src_resource, 0, &src_box);

	// We have effectively removed the offset during the region copy, so
	// set it to 0 to make sure nothing will try to use it again elsewhere:
	*offset = 0;
}

static void FillInMissingInfo(ResourceCopyTargetType type, ID3D11Resource *resource, ID3D11View *view,
		UINT *stride, UINT *offset, UINT *buf_size, DXGI_FORMAT *format)
{
	D3D11_RESOURCE_DIMENSION dimension;
	D3D11_BUFFER_DESC buf_desc;
	ID3D11Buffer *buffer;

	ID3D11ShaderResourceView *resource_view = NULL;
	ID3D11RenderTargetView *render_view = NULL;
	ID3D11DepthStencilView *depth_view = NULL;
	ID3D11UnorderedAccessView *unordered_view = NULL;

	D3D11_SHADER_RESOURCE_VIEW_DESC resource_view_desc;
	D3D11_RENDER_TARGET_VIEW_DESC render_view_desc;
	D3D11_DEPTH_STENCIL_VIEW_DESC depth_view_desc;
	D3D11_UNORDERED_ACCESS_VIEW_DESC unordered_view_desc;

	ID3D11Texture1D *tex1d;
	ID3D11Texture2D *tex2d;
	ID3D11Texture3D *tex3d;
	D3D11_TEXTURE1D_DESC tex1d_desc;
	D3D11_TEXTURE2D_DESC tex2d_desc;
	D3D11_TEXTURE3D_DESC tex3d_desc;

	// Some of these may already be filled in when getting the resource
	// (either because it is stored in the pipeline state and retrieved
	// with the resource, or was stored in a custom resource). If they are
	// not we will try to fill them in here from either the resource or
	// view description as they may be necessary later to create a
	// compatible view or perform a region copy:

	resource->GetType(&dimension);
	if (dimension == D3D11_RESOURCE_DIMENSION_BUFFER) {
		buffer = (ID3D11Buffer*)resource;
		buffer->GetDesc(&buf_desc);
		if (*buf_size)
			*buf_size = min(*buf_size, buf_desc.ByteWidth);
		else
			*buf_size = buf_desc.ByteWidth;

		if (!*stride)
			*stride = buf_desc.StructureByteStride;
	}

	if (view) {
		switch (type) {
			case ResourceCopyTargetType::SHADER_RESOURCE:
				resource_view = (ID3D11ShaderResourceView*)view;
				resource_view->GetDesc(&resource_view_desc);
				if (*format == DXGI_FORMAT_UNKNOWN)
					*format = resource_view_desc.Format;
				if (!*stride)
					*stride = dxgi_format_size(*format);
				if (!*offset)
					*offset = resource_view_desc.Buffer.FirstElement * *stride;
				if (!*buf_size)
					*buf_size = resource_view_desc.Buffer.NumElements * *stride + *offset;
				break;
			case ResourceCopyTargetType::RENDER_TARGET:
				render_view = (ID3D11RenderTargetView*)view;
				render_view->GetDesc(&render_view_desc);
				if (*format == DXGI_FORMAT_UNKNOWN)
					*format = render_view_desc.Format;
				if (!*stride)
					*stride = dxgi_format_size(*format);
				if (!*offset)
					*offset = render_view_desc.Buffer.FirstElement * *stride;
				if (!*buf_size)
					*buf_size = render_view_desc.Buffer.NumElements * *stride + *offset;
				break;
			case ResourceCopyTargetType::DEPTH_STENCIL_TARGET:
				depth_view = (ID3D11DepthStencilView*)view;
				depth_view->GetDesc(&depth_view_desc);
				if (*format == DXGI_FORMAT_UNKNOWN)
					*format = depth_view_desc.Format;
				if (!*stride)
					*stride = dxgi_format_size(*format);
				// Depth stencil buffers cannot be buffers
				break;
			case ResourceCopyTargetType::UNORDERED_ACCESS_VIEW:
				unordered_view = (ID3D11UnorderedAccessView*)view;
				unordered_view->GetDesc(&unordered_view_desc);
				if (*format == DXGI_FORMAT_UNKNOWN)
					*format = unordered_view_desc.Format;
				if (!*stride)
					*stride = dxgi_format_size(*format);
				if (!*offset)
					*offset = unordered_view_desc.Buffer.FirstElement * *stride;
				if (!*buf_size)
					*buf_size = unordered_view_desc.Buffer.NumElements * *stride + *offset;
				break;
		}
	} else if (*format == DXGI_FORMAT_UNKNOWN) {
		// If we *still* don't know the format and it's a texture, get it from
		// the resource description. This will be the case for the back buffer
		// since that does not have a view.
		switch (dimension) {
			case D3D11_RESOURCE_DIMENSION_TEXTURE1D:
				tex1d = (ID3D11Texture1D*)resource;
				tex1d->GetDesc(&tex1d_desc);
				*format = tex1d_desc.Format;
				break;
			case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
				tex2d = (ID3D11Texture2D*)resource;
				tex2d->GetDesc(&tex2d_desc);
				*format = tex2d_desc.Format;
				break;
			case D3D11_RESOURCE_DIMENSION_TEXTURE3D:
				tex3d = (ID3D11Texture3D*)resource;
				tex3d->GetDesc(&tex3d_desc);
				*format = tex3d_desc.Format;
		}
	}

	if (!*stride) {
		// This will catch index buffers, which are not structured and
		// don't have a view, but they do have a format we can use:
		*stride = dxgi_format_size(*format);

		// This will catch constant buffers, which are not structured
		// and don't have either a view or format, so set the stride to
		// the size of the whole buffer:
		if (!*stride)
			*stride = *buf_size;
	}
}

static UINT get_resource_bind_flags(ID3D11Resource *resource)
{
	D3D11_RESOURCE_DIMENSION dimension;
	ID3D11Buffer *buf = NULL;
	ID3D11Texture1D *tex1d = NULL;
	ID3D11Texture2D *tex2d = NULL;
	ID3D11Texture3D *tex3d = NULL;
	D3D11_BUFFER_DESC buf_desc;
	D3D11_TEXTURE1D_DESC tex1d_desc;
	D3D11_TEXTURE2D_DESC tex2d_desc;
	D3D11_TEXTURE3D_DESC tex3d_desc;

	resource->GetType(&dimension);
	switch (dimension) {
		case D3D11_RESOURCE_DIMENSION_BUFFER:
			buf = (ID3D11Buffer*)resource;
			buf->GetDesc(&buf_desc);
			return buf_desc.BindFlags;
		case D3D11_RESOURCE_DIMENSION_TEXTURE1D:
			tex1d = (ID3D11Texture1D*)resource;
			tex1d->GetDesc(&tex1d_desc);
			return tex1d_desc.BindFlags;
		case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
			tex2d = (ID3D11Texture2D*)resource;
			tex2d->GetDesc(&tex2d_desc);
			return tex2d_desc.BindFlags;
		case D3D11_RESOURCE_DIMENSION_TEXTURE3D:
			tex3d = (ID3D11Texture3D*)resource;
			tex3d->GetDesc(&tex3d_desc);
			return tex3d_desc.BindFlags;
	}
	return 0;
}

ID3D11View* ClearViewCommand::create_best_view(
		ID3D11Resource *resource,
		CommandListState *state,
		UINT stride,
		UINT offset,
		DXGI_FORMAT format,
		UINT buf_src_size)
{
	UINT bind_flags;

	// We didn't get a view, so we will have to create one, but
	// which type? We will guess based on what the user specified
	// and what bind flags the resource has.

	FillInMissingInfo(target.type, resource, NULL, &stride, &offset,
			&buf_src_size, &format);

	// If the user specified "depth" and/or "stencil" they gave us
	// the answer:
	if (clear_depth || clear_stencil) {
		return _CreateCompatibleView<ID3D11DepthStencilView,
		       D3D11_DEPTH_STENCIL_VIEW_DESC,
		       &ID3D11Device::CreateDepthStencilView>
			       (resource, state, stride, offset, format, buf_src_size);
	}

	// If the user specified "int" or used a hex string then it
	// must be a UAV and we must be doing an int clear on it:
	if (clear_uav_uint) {
		return _CreateCompatibleView<ID3D11UnorderedAccessView,
		       D3D11_UNORDERED_ACCESS_VIEW_DESC,
		       &ID3D11Device::CreateUnorderedAccessView>
			       (resource, state, stride, offset, format, buf_src_size);
	}

	// Otherwise just make whatever view is compatible with the bind flags.
	// Since views may have multiple bind flags let's prioritise the more
	// esoteric DSV and UAV before RTV on the theory that if they are
	// available then we are more likely to want to use their clear
	// methods.
	bind_flags = get_resource_bind_flags(resource);
	if (bind_flags & D3D11_BIND_DEPTH_STENCIL) {
		return _CreateCompatibleView<ID3D11DepthStencilView,
		       D3D11_DEPTH_STENCIL_VIEW_DESC,
		       &ID3D11Device::CreateDepthStencilView>
			       (resource, state, stride, offset, format, buf_src_size);
	}
	if (bind_flags & D3D11_BIND_UNORDERED_ACCESS) {
		return _CreateCompatibleView<ID3D11UnorderedAccessView,
		       D3D11_UNORDERED_ACCESS_VIEW_DESC,
		       &ID3D11Device::CreateUnorderedAccessView>
			       (resource, state, stride, offset, format, buf_src_size);
	}
	if (bind_flags & D3D11_BIND_RENDER_TARGET) {
		return _CreateCompatibleView<ID3D11RenderTargetView,
		       D3D11_RENDER_TARGET_VIEW_DESC,
		       &ID3D11Device::CreateRenderTargetView>
			       (resource, state, stride, offset, format, buf_src_size);
	}
	// TODO: In DX 11.1 there is a generic clear routine, so SRVs might work?
	return NULL;
}

void ClearViewCommand::clear_unknown_view(ID3D11View *view, CommandListState *state)
{
	ID3D11RenderTargetView *rtv = NULL;
	ID3D11DepthStencilView *dsv = NULL;
	ID3D11UnorderedAccessView *uav = NULL;

	// We have a view, but we don't know what kind of view it is. We could
	// infer that from the target type, but in the future CustomResource
	// targets will return a cached view as well (they already have a view
	// today, but if you follow the logic closely you will realise we don't
	// have any code paths that will decide to use it), so to try to future
	// proof this let's use QueryInterface() to see which interfaces the
	// view supports to tell us what kind it is:
	view->QueryInterface(__uuidof(ID3D11RenderTargetView), (void**)&rtv);
	view->QueryInterface(__uuidof(ID3D11DepthStencilView), (void**)&dsv);
	view->QueryInterface(__uuidof(ID3D11UnorderedAccessView), (void**)&uav);

	if (rtv) {
		state->mHackerContext->FrameAnalysisLog("3DMigoto   clearing RTV\n");
		state->mOrigContext1->ClearRenderTargetView(rtv, fval);
	}
	if (dsv) {
		D3D11_CLEAR_FLAG flags = (D3D11_CLEAR_FLAG)0;
		state->mHackerContext->FrameAnalysisLog("3DMigoto   clearing DSV\n");

		if (!clear_depth && !clear_stencil)
			flags = (D3D11_CLEAR_FLAG)(D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL);
		else if (clear_depth)
			flags = D3D11_CLEAR_DEPTH;
		else if (clear_stencil)
			flags = D3D11_CLEAR_STENCIL;

		state->mOrigContext1->ClearDepthStencilView(dsv, flags, dsv_depth, dsv_stencil);
	}
	if (uav) {
		// We can clear UAVs with either floats or uints, but which
		// should we use? The API call doesn't let us know if it
		// failed, and floats will only work with specific view
		// formats, so we try to predict if the float clear will pass
		// unless the user specificially told us to use the int clear.
		if (clear_uav_uint || !UAVSupportsFloatClear(uav)) {
			state->mHackerContext->FrameAnalysisLog("3DMigoto   clearing UAV (uint)\n");
			state->mOrigContext1->ClearUnorderedAccessViewUint(uav, uval);
		} else {
			state->mHackerContext->FrameAnalysisLog("3DMigoto   clearing UAV (float)\n");
			state->mOrigContext1->ClearUnorderedAccessViewFloat(uav, fval);
		}
	}

	if (rtv)
		rtv->Release();
	if (dsv)
		dsv->Release();
	if (uav)
		uav->Release();
}

void ClearViewCommand::run(CommandListState *state)
{
	ID3D11Resource *resource = NULL;
	ID3D11View *view = NULL;
	UINT stride = 0;
	UINT offset = 0;
	DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
	UINT buf_src_size = 0;

	state->mHackerContext->FrameAnalysisLog("3DMigoto %S\n", ini_line.c_str());

	resource = target.GetResource(state, &view, &stride, &offset, &format, &buf_src_size);
	if (!resource) {
		state->mHackerContext->FrameAnalysisLog("3DMigoto   No resource to clear\n");
		return;
	}

	if (!view)
		view = create_best_view(resource, state, stride, offset, format, buf_src_size);

	if (view)
		clear_unknown_view(view, state);
	else
		state->mHackerContext->FrameAnalysisLog("3DMigoto   No view and unable to create view to clear resource\n");

	if (resource)
		resource->Release();
	if (view)
		view->Release();
}


static bool ViewMatchesResource(ID3D11View *view, ID3D11Resource *resource)
{
	ID3D11Resource *tmp_resource = NULL;

	view->GetResource(&tmp_resource);
	if (!tmp_resource)
		return false;
	tmp_resource->Release();

	return (tmp_resource == resource);
}

// Returns the equivelent target type of built in targets with pre-existing
// views, so that we don't go and create a view cache when we already have one
// we could use directly:
static ResourceCopyTargetType EquivTarget(ResourceCopyTargetType type)
{
	switch(type) {
		case ResourceCopyTargetType::STEREO_PARAMS:
		case ResourceCopyTargetType::INI_PARAMS:
		case ResourceCopyTargetType::CURSOR_MASK:
		case ResourceCopyTargetType::CURSOR_COLOR:
			return ResourceCopyTargetType::SHADER_RESOURCE;
	}
	return type;
}

void ResourceCopyOperation::run(CommandListState *state)
{
	HackerDevice *mHackerDevice = state->mHackerDevice;
	HackerContext *mHackerContext = state->mHackerContext;
	ID3D11DeviceContext *mOrigContext1 = state->mOrigContext1;
	ID3D11Resource *src_resource = NULL;
	ID3D11Resource *dst_resource = NULL;
	ID3D11Resource **pp_cached_resource = &cached_resource;
	ResourcePool *p_resource_pool = &resource_pool;
	ID3D11View *src_view = NULL;
	ID3D11View *dst_view = NULL;
	ID3D11View **pp_cached_view = &cached_view;
	UINT stride = 0;
	UINT offset = 0;
	DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
	UINT buf_src_size = 0, buf_dst_size = 0;

	mHackerContext->FrameAnalysisLog("3DMigoto %S\n", ini_line.c_str());

	if (src.type == ResourceCopyTargetType::EMPTY) {
		dst.SetResource(state, NULL, NULL, 0, 0, DXGI_FORMAT_UNKNOWN, 0);
		return;
	}

	src_resource = src.GetResource(state, &src_view, &stride, &offset, &format, &buf_src_size);
	if (!src_resource) {
		mHackerContext->FrameAnalysisLog("3DMigoto   Copy source was NULL\n");
		if (!(options & ResourceCopyOptions::UNLESS_NULL)) {
			// Still set destination to NULL - if we are copying a
			// resource we generally expect it to be there, and
			// this will make errors more obvious if we copy
			// something that doesn't exist. This behaviour can be
			// overridden with the unless_null keyword.
			dst.SetResource(state, NULL, NULL, 0, 0, DXGI_FORMAT_UNKNOWN, 0);
		}
		return;
	}

	if (dst.type == ResourceCopyTargetType::CUSTOM_RESOURCE) {
		// If we're copying to a custom resource, use the resource &
		// view in the CustomResource directly as the cache instead of
		// the cache in the ResourceCopyOperation. This will reduce the
		// number of extra resources we have floating around if copying
		// something to a single custom resource from multiple shaders.
		pp_cached_resource = &dst.custom_resource->resource;
		p_resource_pool = &dst.custom_resource->resource_pool;
		pp_cached_view = &dst.custom_resource->view;

		if (dst.custom_resource->max_copies_per_frame) {
			if (dst.custom_resource->frame_no != G->frame_no) {
				dst.custom_resource->frame_no = G->frame_no;
				dst.custom_resource->copies_this_frame = 1;
			} else if (dst.custom_resource->copies_this_frame++ >= dst.custom_resource->max_copies_per_frame) {
				mHackerContext->FrameAnalysisLog("3DMigoto   max_copies_per_frame exceeded\n");
				return;
			}
		}

		dst.custom_resource->OverrideOutOfBandInfo(&format, &stride);
	}

	FillInMissingInfo(src.type, src_resource, src_view, &stride, &offset, &buf_src_size, &format);

	if (options & ResourceCopyOptions::COPY_MASK) {
		RecreateCompatibleResource(&ini_line, &dst, src_resource,
			pp_cached_resource, p_resource_pool, src_view, pp_cached_view,
			state, mHackerDevice->mStereoHandle,
			options, stride, offset, format, &buf_dst_size);

		if (!*pp_cached_resource) {
			LogDebug("Resource copy error: Could not create/update destination resource\n");
			goto out_release;
		}
		dst_resource = *pp_cached_resource;
		dst_view = *pp_cached_view;

		if (options & ResourceCopyOptions::COPY_DESC) {
			// RecreateCompatibleResource has already done the work
			mHackerContext->FrameAnalysisLog("3DMigoto   copying resource description\n");
		} else if (options & ResourceCopyOptions::STEREO2MONO) {
			mHackerContext->FrameAnalysisLog("3DMigoto   performing reverse stereo blit\n");

			// TODO: Resolve MSAA to an intermediate resource first
			// if necessary (but keep in mind this may have
			// compatibility issues without a fallback path)

			// The reverse stereo blit seems to only work if the
			// destination resource is stereo. This is a bit
			// bizzare since the whole point of it is to create a
			// double width mono resource, but there you go.
			// We use a second intermediate resource that is forced
			// to stereo and the final destination is forced to
			// mono - once we have done the reverse blit we use an
			// ordinary copy to the final mono resource.

			RecreateCompatibleResource(&(ini_line + L" (intermediate)"),
				NULL, src_resource, &stereo2mono_intermediate,
				p_resource_pool, NULL, NULL,
				state, mHackerDevice->mStereoHandle,
				(ResourceCopyOptions)(options | ResourceCopyOptions::STEREO),
				stride, offset, format, NULL);

			ReverseStereoBlit(stereo2mono_intermediate, src_resource, state);

			mOrigContext1->CopyResource(dst_resource, stereo2mono_intermediate);

		} else if (options & ResourceCopyOptions::RESOLVE_MSAA) {
			mHackerContext->FrameAnalysisLog("3DMigoto   resolving MSAA\n");
			ResolveMSAA(dst_resource, src_resource, state);
		} else if (buf_dst_size) {
			mHackerContext->FrameAnalysisLog("3DMigoto   performing region copy\n");
			SpecialCopyBufferRegion(dst_resource, src_resource,
					state, stride, &offset,
					buf_src_size, buf_dst_size);
		} else {
			mHackerContext->FrameAnalysisLog("3DMigoto   performing full copy\n");
			mOrigContext1->CopyResource(dst_resource, src_resource);
		}
	} else {
		mHackerContext->FrameAnalysisLog("3DMigoto   copying by reference\n");
		dst_resource = src_resource;
		if (src_view && (EquivTarget(src.type) == EquivTarget(dst.type))) {
			dst_view = src_view;
		} else if (*pp_cached_view) {
			if (ViewMatchesResource(*pp_cached_view, dst_resource)) {
				dst_view = *pp_cached_view;
			} else {
				LogDebug("Resource copying: Releasing stale view cache\n");
				(*pp_cached_view)->Release();
				*pp_cached_view = NULL;
			}
		}
		// TODO: If we are referencing to/from a custom resource we
		// currently don't reference the view, but we could so long as
		// the bind flags from the original source are compatible with
		// the bind flags in the final destination. If we implement
		// this, go read the note in CustomResource::Substantiate()
	}

	if (!dst_view) {
		dst_view = CreateCompatibleView(&dst, dst_resource, state,
				stride, offset, format, buf_src_size);
		// Not checking for NULL return as view's are not applicable to
		// all types. Legitimate failures are logged.
		*pp_cached_view = dst_view;
	}

	dst.SetResource(state, dst_resource, dst_view, stride, offset, format, buf_dst_size);

	if (options & ResourceCopyOptions::SET_VIEWPORT)
		SetViewportFromResource(state, dst_resource);

out_release:

	if ((options & ResourceCopyOptions::NO_VIEW_CACHE || src.forbid_view_cache)
			&& *pp_cached_view)
	{
		(*pp_cached_view)->Release();
		*pp_cached_view = NULL;
	}

	if (src_view)
		src_view->Release();

	if (src_resource)
		src_resource->Release();
}
