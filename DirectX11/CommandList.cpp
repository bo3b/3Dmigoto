#include "CommandList.h"

#include <DDSTextureLoader.h>
#include <WICTextureLoader.h>
#include <algorithm>
#include "HackerDevice.h"
#include "HackerContext.h"

#include <D3DCompiler.h>

CustomResources customResources;
CustomShaders customShaders;
ExplicitCommandListSections explicitCommandListSections;

void _RunCommandList(HackerDevice *mHackerDevice,
		HackerContext *mHackerContext,
		ID3D11Device *mOrigDevice,
		ID3D11DeviceContext *mOrigContext,
		CommandList *command_list,
		CommandListState *state)
{
	CommandList::iterator i;

	if (state->recursion > MAX_COMMAND_LIST_RECURSION) {
		LogInfo("WARNING: Command list recursion limit exceeded! Circular reference?\n");
		return;
	}

	state->recursion++;
	for (i = command_list->begin(); i < command_list->end(); i++) {
		(*i)->run(mHackerDevice, mHackerContext, mOrigDevice, mOrigContext, state);
	}
	state->recursion--;
}

static void CommandListFlushState(HackerDevice *mHackerDevice,
		ID3D11DeviceContext *mOrigContext,
		CommandListState *state)
{
	D3D11_MAPPED_SUBRESOURCE mappedResource;
	HRESULT hr;

	if (state->update_params) {
		hr = mOrigContext->Map(mHackerDevice->mIniTexture, 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
		if (FAILED(hr)) {
			LogInfo("CommandListFlushState: Map failed\n");
			return;
		}
		memcpy(mappedResource.pData, &G->iniParams, sizeof(G->iniParams));
		mOrigContext->Unmap(mHackerDevice->mIniTexture, 0);
		state->update_params = false;
	}
}

void RunCommandList(HackerDevice *mHackerDevice,
		HackerContext *mHackerContext,
		CommandList *command_list,
		DrawCallInfo *call_info, bool post)
{
	CommandListState state;
	ID3D11Device *mOrigDevice = mHackerDevice->GetOrigDevice();
	ID3D11DeviceContext *mOrigContext = mHackerContext->GetOrigContext();

	state.call_info = call_info;
	state.post = post;

	_RunCommandList(mHackerDevice, mHackerContext, mOrigDevice, mOrigContext, command_list, &state);
	CommandListFlushState(mHackerDevice, mOrigContext, &state);
}

static void AddCommandToList(CommandListCommand *command,
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
}

static bool ParseCheckTextureOverride(wstring *val, CommandList *explicit_command_list,
		CommandList *pre_command_list, CommandList *post_command_list)
{
	int ret, len1;

	CheckTextureOverrideCommand *operation = new CheckTextureOverrideCommand();

	// Parse value as "<shader type>s-t<testure slot>", consistent
	// with texture filtering and resource copying
	ret = swscanf_s(val->c_str(), L"%lcs-t%u%n", &operation->shader_type, 1, &operation->texture_slot, &len1);
	if (ret == 2 && len1 == val->length() &&
			operation->texture_slot < D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT) {
		switch(operation->shader_type) {
			case L'v': case L'h': case L'd': case L'g': case L'p': case L'c':
				operation->ini_val = *val;
				AddCommandToList(operation, explicit_command_list, NULL, pre_command_list, post_command_list);
				return true;
		}
	}

	delete operation;
	return false;
}

static bool ParseRunShader(wstring *val, CommandList *explicit_command_list,
		CommandList *pre_command_list, CommandList *post_command_list)
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

	operation->ini_val = *val;
	operation->custom_shader = &shader->second;
	AddCommandToList(operation, explicit_command_list, pre_command_list, NULL, NULL);
	return true;

bail:
	delete operation;
	return false;
}

static bool ParseRunExplicitCommandList(wstring *val, CommandList *explicit_command_list,
		CommandList *pre_command_list, CommandList *post_command_list)
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

	operation->ini_val = *val;
	operation->command_list_section = &shader->second;
	// This function is nearly identical to ParseRunShader, but in case we
	// later refactor these together note that here we do not specify a
	// sensible command list, so it will be added to both pre and post
	// command lists:
	AddCommandToList(operation, explicit_command_list, NULL, pre_command_list, post_command_list);
	return true;

bail:
	delete operation;
	return false;
}

static bool ParseDrawCommand(const wchar_t *key, wstring *val,
		CommandList *explicit_command_list,
		CommandList *pre_command_list, CommandList *post_command_list)
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

	AddCommandToList(operation, explicit_command_list, pre_command_list, NULL, NULL);
	return true;

bail:
	delete operation;
	return false;
}

bool ParseCommandListGeneralCommands(const wchar_t *key, wstring *val,
		CommandList *explicit_command_list,
		CommandList *pre_command_list, CommandList *post_command_list)
{
	if (!wcscmp(key, L"checktextureoverride"))
		return ParseCheckTextureOverride(val, explicit_command_list, pre_command_list, post_command_list);

	if (!wcscmp(key, L"run")) {
		if (!wcsncmp(val->c_str(), L"customshader", 12))
			return ParseRunShader(val, explicit_command_list, pre_command_list, post_command_list);

		if (!wcsncmp(val->c_str(), L"commandlist", 11))
			return ParseRunExplicitCommandList(val, explicit_command_list, pre_command_list, post_command_list);
	}

	return ParseDrawCommand(key, val, explicit_command_list, pre_command_list, post_command_list);
}

static TextureOverride* FindTextureOverrideBySlot(HackerContext
		*mHackerContext, ID3D11DeviceContext *mOrigContext,
		wchar_t shader_type, unsigned texture_slot)
{
	D3D11_SHADER_RESOURCE_VIEW_DESC desc;
	ID3D11ShaderResourceView *view;
	ID3D11Resource *resource = NULL;
	TextureOverrideMap::iterator i;
	TextureOverride *ret = NULL;
	uint32_t hash = 0;

	switch (shader_type) {
		case L'v':
			mOrigContext->VSGetShaderResources(texture_slot, 1, &view);
			break;
		case L'h':
			mOrigContext->HSGetShaderResources(texture_slot, 1, &view);
			break;
		case L'd':
			mOrigContext->DSGetShaderResources(texture_slot, 1, &view);
			break;
		case L'g':
			mOrigContext->GSGetShaderResources(texture_slot, 1, &view);
			break;
		case L'p':
			mOrigContext->PSGetShaderResources(texture_slot, 1, &view);
			break;
		case L'c':
			mOrigContext->CSGetShaderResources(texture_slot, 1, &view);
			break;
		default:
			// Should not happen
			return NULL;
	}
	if (!view)
		return NULL;

	view->GetResource(&resource);
	if (!resource)
		goto out_release_view;

	view->GetDesc(&desc);

	if (G->ENABLE_CRITICAL_SECTION) EnterCriticalSection(&G->mCriticalSection);
		hash = GetResourceHash(resource);
	if (G->ENABLE_CRITICAL_SECTION) LeaveCriticalSection(&G->mCriticalSection);
	if (!hash)
		goto out_release_resource;

	mHackerContext->FrameAnalysisLog(" hash=%08llx", hash);

	i = G->mTextureOverrideMap.find(hash);
	if (i == G->mTextureOverrideMap.end())
		goto out_release_resource;

	ret = &i->second;

out_release_resource:
	resource->Release();
out_release_view:
	view->Release();
	return ret;
}

void CheckTextureOverrideCommand::run(HackerDevice *mHackerDevice, HackerContext *mHackerContext,
		ID3D11Device *mOrigDevice, ID3D11DeviceContext *mOrigContext,
		CommandListState *state)
{
	mHackerContext->FrameAnalysisLog("3DMigoto checktextureoverride = %S", ini_val.c_str());

	TextureOverride *override = FindTextureOverrideBySlot(mHackerContext,
			mOrigContext, shader_type, texture_slot);

	mHackerContext->FrameAnalysisLog(" found=%s\n", override ? "true" : "false");

	if (!override)
		return;

	if (state->post)
		_RunCommandList(mHackerDevice, mHackerContext, mOrigDevice, mOrigContext, &override->post_command_list, state);
	else
		_RunCommandList(mHackerDevice, mHackerContext, mOrigDevice, mOrigContext, &override->command_list, state);
}

void DrawCommand::run(HackerDevice *mHackerDevice, HackerContext *mHackerContext,
		ID3D11Device *mOrigDevice, ID3D11DeviceContext *mOrigContext,
		CommandListState *state)
{
	// Ensure IniParams are visible:
	CommandListFlushState(mHackerDevice, mOrigContext, state);

	switch (type) {
		case DrawCommandType::DRAW:
			mHackerContext->FrameAnalysisLog("3DMigoto Draw(%u, %u)\n", args[0], args[1]);
			mOrigContext->Draw(args[0], args[1]);
			break;
		case DrawCommandType::DRAW_AUTO:
			mHackerContext->FrameAnalysisLog("3DMigoto DrawAuto()\n");
			mOrigContext->DrawAuto();
			break;
		case DrawCommandType::DRAW_INDEXED:
			mHackerContext->FrameAnalysisLog("3DMigoto DrawIndexed(%u, %u, %i)\n", args[0], args[1], (INT)args[2]);
			mOrigContext->DrawIndexed(args[0], args[1], (INT)args[2]);
			break;
		case DrawCommandType::DRAW_INDEXED_INSTANCED:
			mHackerContext->FrameAnalysisLog("3DMigoto DrawIndexedInstanced(%u, %u, %u, %i, %u)\n", args[0], args[1], args[2], (INT)args[3], args[4]);
			mOrigContext->DrawIndexedInstanced(args[0], args[1], args[2], (INT)args[3], args[4]);
			break;
		// TODO: case DrawCommandType::DRAW_INDEXED_INSTANCED_INDIRECT:
		// TODO: 	break;
		case DrawCommandType::DRAW_INSTANCED:
			mHackerContext->FrameAnalysisLog("3DMigoto DrawInstanced(%u, %u, %u, %u)\n", args[0], args[1], args[2], args[3]);
			mOrigContext->DrawInstanced(args[0], args[1], args[2], args[3]);
			break;
		// TODO: case DrawCommandType::DRAW_INSTANCED_INDIRECT:
		// TODO: 	break;
		case DrawCommandType::DISPATCH:
			mHackerContext->FrameAnalysisLog("3DMigoto Dispatch(%u, %u, %u)\n", args[0], args[1], args[2]);
			mOrigContext->Dispatch(args[0], args[1], args[2]);
			break;
		// TODO: case DrawCommandType::DISPATCH_INDIRECT:
		// TODO: 	break;
		case DrawCommandType::FROM_CALLER:
			DrawCallInfo *info = state->call_info;
			if (info->InstanceCount) {
				if (info->IndexCount) {
					mHackerContext->FrameAnalysisLog("3DMigoto Draw = from_caller -> DrawIndexedInstanced(%u, %u, %u, %i, %u)\n", info->IndexCount, info->InstanceCount, info->FirstIndex, info->FirstVertex, info->FirstInstance);
					mOrigContext->DrawIndexedInstanced(info->IndexCount, info->InstanceCount, info->FirstIndex, info->FirstVertex, info->FirstInstance);
				} else {
					mHackerContext->FrameAnalysisLog("3DMigoto Draw = from_caller -> DrawInstanced(%u, %u, %u, %u)\n", info->VertexCount, info->InstanceCount, info->FirstVertex, info->FirstInstance);
					mOrigContext->DrawInstanced(info->VertexCount, info->InstanceCount, info->FirstVertex, info->FirstInstance);
				}
			} else if (info->IndexCount) {
				mHackerContext->FrameAnalysisLog("3DMigoto Draw = from_caller -> DrawIndexed(%u, %u, %i)\n", info->IndexCount, info->FirstIndex, info->FirstVertex);
				mOrigContext->DrawIndexed(info->IndexCount, info->FirstIndex, info->FirstVertex);
			} else if (info->VertexCount) {
				mHackerContext->FrameAnalysisLog("3DMigoto Draw from_caller -> Draw(%u, %u)\n", info->VertexCount, info->FirstVertex);
				mOrigContext->Draw(info->VertexCount, info->FirstVertex);
			}
			// TODO: Save enough state to know if it's DrawAuto or
			// an Indirect draw call (and the buffer)
			break;
	}
}

CustomShader::CustomShader() :
	vs_override(false), hs_override(false), ds_override(false),
	gs_override(false), ps_override(false), cs_override(false),
	vs(NULL), hs(NULL), ds(NULL), gs(NULL), ps(NULL), cs(NULL),
	vs_bytecode(NULL), hs_bytecode(NULL), ds_bytecode(NULL),
	gs_bytecode(NULL), ps_bytecode(NULL), cs_bytecode(NULL),
	blend_override(0), blend_state(NULL), blend_sample_mask(0xffffffff),
	rs_override(0), rs_state(NULL),
	topology(D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED),
	substantiated(false),
	max_executions_per_frame(0),
	frame_no(0),
	executions_this_frame(0)
{
	int i;

	for (i = 0; i < 4; i++)
		blend_factor[i] = 1.0f;
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
}

// This is similar to the other compile routines, but still distinct enough to
// get it's own function for now - TODO: Refactor out the common code
bool CustomShader::compile(char type, wchar_t *filename, const wstring *wname)
{
	wchar_t path[MAX_PATH];
	HANDLE f;
	DWORD srcDataSize, readSize;
	vector<char> srcData;
	HRESULT hr;
	char shaderModel[7];
	ID3DBlob **ppBytecode = NULL;
	ID3DBlob *pErrorMsgs = NULL;
	string name(wname->begin(), wname->end());

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

	if (!GetModuleFileName(0, path, MAX_PATH)) {
		LogInfo("GetModuleFileName failed\n");
		goto err;
	}
	wcsrchr(path, L'\\')[1] = 0;
	wcscat(path, filename);

	f = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (f == INVALID_HANDLE_VALUE) {
		LogInfo("    Shader not found: %S\n", path);
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

	hr = D3DCompile(srcData.data(), srcDataSize, name.c_str(), 0, D3D_COMPILE_STANDARD_FILE_INCLUDE,
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

void CustomShader::substantiate(ID3D11Device *mOrigDevice)
{
	if (substantiated)
		return;
	substantiated = true;

	if (vs_bytecode) {
		mOrigDevice->CreateVertexShader(vs_bytecode->GetBufferPointer(), vs_bytecode->GetBufferSize(), NULL, &vs);
		vs_bytecode->Release();
		vs_bytecode = NULL;
	}
	if (hs_bytecode) {
		mOrigDevice->CreateHullShader(hs_bytecode->GetBufferPointer(), hs_bytecode->GetBufferSize(), NULL, &hs);
		hs_bytecode->Release();
		hs_bytecode = NULL;
	}
	if (ds_bytecode) {
		mOrigDevice->CreateDomainShader(ds_bytecode->GetBufferPointer(), ds_bytecode->GetBufferSize(), NULL, &ds);
		ds_bytecode->Release();
		ds_bytecode = NULL;
	}
	if (gs_bytecode) {
		mOrigDevice->CreateGeometryShader(gs_bytecode->GetBufferPointer(), gs_bytecode->GetBufferSize(), NULL, &gs);
		gs_bytecode->Release();
		gs_bytecode = NULL;
	}
	if (ps_bytecode) {
		mOrigDevice->CreatePixelShader(ps_bytecode->GetBufferPointer(), ps_bytecode->GetBufferSize(), NULL, &ps);
		ps_bytecode->Release();
		ps_bytecode = NULL;
	}
	if (cs_bytecode) {
		mOrigDevice->CreateComputeShader(cs_bytecode->GetBufferPointer(), cs_bytecode->GetBufferSize(), NULL, &cs);
		cs_bytecode->Release();
		cs_bytecode = NULL;
	}

	if (blend_override == 1) // 2 will use default blend state
		mOrigDevice->CreateBlendState(&blend_desc, &blend_state);

	if (rs_override == 1) // 2 will use default blend state
		mOrigDevice->CreateRasterizerState(&rs_desc, &rs_state);
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

static void get_all_rts_dsv_uavs(ID3D11DeviceContext *mOrigContext,
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

	mOrigContext->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, rtvs, dsv);

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
	mOrigContext->OMGetRenderTargetsAndUnorderedAccessViews(0, NULL, NULL, *UAVStartSlot, *NumUAVs, uavs);
}

void RunCustomShaderCommand::run(HackerDevice *mHackerDevice, HackerContext *mHackerContext,
		ID3D11Device *mOrigDevice, ID3D11DeviceContext *mOrigContext,
		CommandListState *state)
{
	ID3D11VertexShader *saved_vs = NULL;
	ID3D11HullShader *saved_hs = NULL;
	ID3D11DomainShader *saved_ds = NULL;
	ID3D11GeometryShader *saved_gs = NULL;
	ID3D11PixelShader *saved_ps = NULL;
	ID3D11ComputeShader *saved_cs = NULL;
	ID3D11BlendState *saved_blend = NULL;
	ID3D11RasterizerState *saved_rs = NULL;
	UINT num_viewports = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
	D3D11_VIEWPORT saved_viewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE];
	FLOAT saved_blend_factor[4];
	UINT saved_sample_mask;
	bool saved_post;
	UINT NumRTVs, UAVStartSlot, NumUAVs;
	ID3D11RenderTargetView *saved_rtvs[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
	ID3D11DepthStencilView *saved_dsv;
	ID3D11UnorderedAccessView *saved_uavs[D3D11_PS_CS_UAV_REGISTER_COUNT];
	UINT uav_counts[D3D11_PS_CS_UAV_REGISTER_COUNT] = {(UINT)-1, (UINT)-1, (UINT)-1, (UINT)-1, (UINT)-1, (UINT)-1, (UINT)-1, (UINT)-1};
	UINT i;
	D3D11_PRIMITIVE_TOPOLOGY saved_topology;

	mHackerContext->FrameAnalysisLog("3DMigoto run %S\n", ini_val.c_str());

	if (custom_shader->max_executions_per_frame) {
		if (custom_shader->frame_no != G->frame_no) {
			custom_shader->frame_no = G->frame_no;
			custom_shader->executions_this_frame = 1;
		} else if (custom_shader->executions_this_frame++ >= custom_shader->max_executions_per_frame) {
			mHackerContext->FrameAnalysisLog("max_executions_per_frame exceeded\n");
			return;
		}
	}

	custom_shader->substantiate(mOrigDevice);

	saved_shader_inst vs_inst, hs_inst, ds_inst, gs_inst, ps_inst, cs_inst;

	// Assign custom shaders first before running the command lists, and
	// restore them last. This is so that if someone was injecting a
	// sequence of pixel shaders that all shared a common vertex shader
	// we can avoid having to repeatedly save & restore the vertex shader
	// by calling the next shader in sequence from the command list after
	// the draw call.

	if (custom_shader->vs_override) {
		mOrigContext->VSGetShader(&saved_vs, vs_inst.instances, &vs_inst.num_instances);
		mOrigContext->VSSetShader(custom_shader->vs, NULL, 0);
	}
	if (custom_shader->hs_override) {
		mOrigContext->HSGetShader(&saved_hs, hs_inst.instances, &hs_inst.num_instances);
		mOrigContext->HSSetShader(custom_shader->hs, NULL, 0);
	}
	if (custom_shader->ds_override) {
		mOrigContext->DSGetShader(&saved_ds, ds_inst.instances, &ds_inst.num_instances);
		mOrigContext->DSSetShader(custom_shader->ds, NULL, 0);
	}
	if (custom_shader->gs_override) {
		mOrigContext->GSGetShader(&saved_gs, gs_inst.instances, &gs_inst.num_instances);
		mOrigContext->GSSetShader(custom_shader->gs, NULL, 0);
	}
	if (custom_shader->ps_override) {
		mOrigContext->PSGetShader(&saved_ps, ps_inst.instances, &ps_inst.num_instances);
		mOrigContext->PSSetShader(custom_shader->ps, NULL, 0);
	}
	if (custom_shader->cs_override) {
		mOrigContext->CSGetShader(&saved_cs, cs_inst.instances, &cs_inst.num_instances);
		mOrigContext->CSSetShader(custom_shader->cs, NULL, 0);
	}
	if (custom_shader->blend_override) {
		mOrigContext->OMGetBlendState(&saved_blend, saved_blend_factor, &saved_sample_mask);
		mOrigContext->OMSetBlendState(custom_shader->blend_state, custom_shader->blend_factor, custom_shader->blend_sample_mask);
	}
	if (custom_shader->rs_override) {
		mOrigContext->RSGetState(&saved_rs);
		mOrigContext->RSSetState(custom_shader->rs_state);
	}
	if (custom_shader->topology != D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED) {
		mOrigContext->IAGetPrimitiveTopology(&saved_topology);
		mOrigContext->IASetPrimitiveTopology(custom_shader->topology);
	}

	// We save off the viewports unconditionally for now. We could
	// potentially skip this by flagging if a command list may alter them,
	// but that probably wouldn't buy us anything:
	mOrigContext->RSGetViewports(&num_viewports, saved_viewports);
	// Likewise, save off all RTVs, UAVs and DSVs unconditionally:
	get_all_rts_dsv_uavs(mOrigContext, &NumRTVs, saved_rtvs, &saved_dsv, &UAVStartSlot, &NumUAVs, saved_uavs);

	// Run the command lists. This should generally include a draw or
	// dispatch call, or call out to another command list which does.
	// The reason for having a post command list is so that people can
	// write 'ps-t100 = ResourceFoo; post ps-t100 = null' and have it work.
	saved_post = state->post;
	state->post = false;
	_RunCommandList(mHackerDevice, mHackerContext, mOrigDevice, mOrigContext, &custom_shader->command_list, state);
	state->post = true;
	_RunCommandList(mHackerDevice, mHackerContext, mOrigDevice, mOrigContext, &custom_shader->post_command_list, state);
	state->post = saved_post;

	// Finally restore the original shaders
	if (custom_shader->vs_override)
		mOrigContext->VSSetShader(saved_vs, vs_inst.instances, vs_inst.num_instances);
	if (custom_shader->hs_override)
		mOrigContext->HSSetShader(saved_hs, hs_inst.instances, hs_inst.num_instances);
	if (custom_shader->ds_override)
		mOrigContext->DSSetShader(saved_ds, ds_inst.instances, ds_inst.num_instances);
	if (custom_shader->gs_override)
		mOrigContext->GSSetShader(saved_gs, gs_inst.instances, gs_inst.num_instances);
	if (custom_shader->ps_override)
		mOrigContext->PSSetShader(saved_ps, ps_inst.instances, ps_inst.num_instances);
	if (custom_shader->cs_override)
		mOrigContext->CSSetShader(saved_cs, cs_inst.instances, cs_inst.num_instances);
	if (custom_shader->blend_override)
		mOrigContext->OMSetBlendState(saved_blend, saved_blend_factor, saved_sample_mask);
	if (custom_shader->rs_override)
		mOrigContext->RSSetState(saved_rs);
	if (custom_shader->topology != D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED)
		mOrigContext->IASetPrimitiveTopology(saved_topology);

	mOrigContext->RSSetViewports(num_viewports, saved_viewports);
	mOrigContext->OMSetRenderTargetsAndUnorderedAccessViews(NumRTVs, saved_rtvs, saved_dsv, UAVStartSlot, NumUAVs, saved_uavs, uav_counts);

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
	if (saved_rs)
		saved_rs->Release();

	for (i = 0; i < NumRTVs; i++) {
		if (saved_rtvs[i])
			saved_rtvs[i]->Release();
	}
	for (i = 0; i < NumUAVs; i++) {
		if (saved_uavs[i])
			saved_uavs[i]->Release();
	}
}

void RunExplicitCommandList::run(HackerDevice *mHackerDevice, HackerContext *mHackerContext,
		ID3D11Device *mOrigDevice, ID3D11DeviceContext *mOrigContext,
		CommandListState *state)
{
	mHackerContext->FrameAnalysisLog("3DMigoto run = %S", ini_val.c_str());

	if (state->post)
		_RunCommandList(mHackerDevice, mHackerContext, mOrigDevice, mOrigContext, &command_list_section->post_command_list, state);
	else
		_RunCommandList(mHackerDevice, mHackerContext, mOrigDevice, mOrigContext, &command_list_section->command_list, state);
}


static void ProcessParamRTSize(ID3D11DeviceContext *mOrigContext, CommandListState *state)
{
	D3D11_RENDER_TARGET_VIEW_DESC view_desc;
	D3D11_TEXTURE2D_DESC res_desc;
	ID3D11RenderTargetView *view = NULL;
	ID3D11Resource *res = NULL;
	ID3D11Texture2D *tex = NULL;

	if (state->rt_width != -1)
		return;

	mOrigContext->OMGetRenderTargets(1, &view, NULL);
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

static float ProcessParamTextureFilter(HackerContext *mHackerContext,
		ID3D11DeviceContext *mOrigContext, ParamOverride *override)
{
	TextureOverride *tex = FindTextureOverrideBySlot(mHackerContext,
			mOrigContext, override->shader_type, override->texture_slot);
	if (!tex)
		return 0;

	return tex->filter_index;
}

static void UpdateCursorInfo(CommandListState *state)
{
	if (state->cursor_info.cbSize)
		return;

	state->cursor_info.cbSize = sizeof(CURSORINFO);
	GetCursorInfo(&state->cursor_info);
}

void ParamOverride::run(HackerDevice *mHackerDevice, HackerContext *mHackerContext,
		ID3D11Device *mOrigDevice, ID3D11DeviceContext *mOrigContext,
		CommandListState *state)
{
	float *dest = &(G->iniParams[param_idx].*param_component);

	float orig = *dest;

	switch (type) {
		case ParamOverrideType::VALUE:
			*dest = val;
			break;
		case ParamOverrideType::RT_WIDTH:
			ProcessParamRTSize(mOrigContext, state);
			*dest = state->rt_width;
			break;
		case ParamOverrideType::RT_HEIGHT:
			ProcessParamRTSize(mOrigContext, state);
			*dest = state->rt_height;
			break;
		case ParamOverrideType::RES_WIDTH:
			*dest = (float)G->mResolutionInfo.width;
			break;
		case ParamOverrideType::RES_HEIGHT:
			*dest = (float)G->mResolutionInfo.height;
			break;
		case ParamOverrideType::TEXTURE:
			*dest = ProcessParamTextureFilter(mHackerContext,
					mOrigContext, this);
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
		default:
			return;
	}

	mHackerContext->FrameAnalysisLog("3DMigoto %S = %S (%f)\n", ini_key.c_str(), ini_val.c_str(), *dest);

	state->update_params |= (*dest != orig);
}

// Parse IniParams overrides, in forms such as
// x = 0.3 (set parameter to specific value, e.g. for shader partner filtering)
// y2 = ps-t0 (use parameter for texture filtering based on texture slot of shader type)
// z3 = rt_width / rt_height (set parameter to render target width/height)
// w4 = res_width / res_height (set parameter to resolution width/height)
bool ParseCommandListIniParamOverride(const wchar_t *key, wstring *val,
		CommandList *command_list)
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

	// Try parsing value as "<shader type>s-t<testure slot>" for texture filtering
	ret = swscanf_s(val->c_str(), L"%lcs-t%u%n", &param->shader_type, 1, &param->texture_slot, &len1);
	if (ret == 2 && len1 == val->length() &&
			param->texture_slot < D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT) {
		switch(param->shader_type) {
			case L'v': case L'h': case L'd': case L'g': case L'p': case L'c':
				param->type = ParamOverrideType::TEXTURE;
				goto success;
			default:
				goto bail;
		}
	}

	// Check special keywords
	param->type = lookup_enum_val<const wchar_t *, ParamOverrideType>
		(ParamOverrideTypeNames, val->c_str(), ParamOverrideType::INVALID);
	if (param->type == ParamOverrideType::INVALID)
		goto bail;

success:
	param->ini_key = key;
	param->ini_val = *val;
	command_list->push_back(std::shared_ptr<CommandListCommand>(param));
	return true;
bail:
	delete param;
	return false;
}


// Is there already a utility function that does this?
static UINT dxgi_format_size(DXGI_FORMAT format)
{
	switch (format) {
		case DXGI_FORMAT_R32G32B32A32_TYPELESS:
		case DXGI_FORMAT_R32G32B32A32_FLOAT:
		case DXGI_FORMAT_R32G32B32A32_UINT:
		case DXGI_FORMAT_R32G32B32A32_SINT:
			return 16;
		case DXGI_FORMAT_R32G32B32_TYPELESS:
		case DXGI_FORMAT_R32G32B32_FLOAT:
		case DXGI_FORMAT_R32G32B32_UINT:
		case DXGI_FORMAT_R32G32B32_SINT:
			return 12;
		case DXGI_FORMAT_R16G16B16A16_TYPELESS:
		case DXGI_FORMAT_R16G16B16A16_FLOAT:
		case DXGI_FORMAT_R16G16B16A16_UNORM:
		case DXGI_FORMAT_R16G16B16A16_UINT:
		case DXGI_FORMAT_R16G16B16A16_SNORM:
		case DXGI_FORMAT_R16G16B16A16_SINT:
		case DXGI_FORMAT_R32G32_TYPELESS:
		case DXGI_FORMAT_R32G32_FLOAT:
		case DXGI_FORMAT_R32G32_UINT:
		case DXGI_FORMAT_R32G32_SINT:
		case DXGI_FORMAT_R32G8X24_TYPELESS:
		case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
		case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
		case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
			return 8;
		case DXGI_FORMAT_R10G10B10A2_TYPELESS:
		case DXGI_FORMAT_R10G10B10A2_UNORM:
		case DXGI_FORMAT_R10G10B10A2_UINT:
		case DXGI_FORMAT_R11G11B10_FLOAT:
		case DXGI_FORMAT_R8G8B8A8_TYPELESS:
		case DXGI_FORMAT_R8G8B8A8_UNORM:
		case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
		case DXGI_FORMAT_R8G8B8A8_UINT:
		case DXGI_FORMAT_R8G8B8A8_SNORM:
		case DXGI_FORMAT_R8G8B8A8_SINT:
		case DXGI_FORMAT_R16G16_TYPELESS:
		case DXGI_FORMAT_R16G16_FLOAT:
		case DXGI_FORMAT_R16G16_UNORM:
		case DXGI_FORMAT_R16G16_UINT:
		case DXGI_FORMAT_R16G16_SNORM:
		case DXGI_FORMAT_R16G16_SINT:
		case DXGI_FORMAT_R32_TYPELESS:
		case DXGI_FORMAT_D32_FLOAT:
		case DXGI_FORMAT_R32_FLOAT:
		case DXGI_FORMAT_R32_UINT:
		case DXGI_FORMAT_R32_SINT:
		case DXGI_FORMAT_R24G8_TYPELESS:
		case DXGI_FORMAT_D24_UNORM_S8_UINT:
		case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
		case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
		case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:
		case DXGI_FORMAT_R8G8_B8G8_UNORM:
		case DXGI_FORMAT_G8R8_G8B8_UNORM:
		case DXGI_FORMAT_B8G8R8A8_UNORM:
		case DXGI_FORMAT_B8G8R8X8_UNORM:
		case DXGI_FORMAT_R10G10B10_XR_BIAS_A2_UNORM:
		case DXGI_FORMAT_B8G8R8A8_TYPELESS:
		case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
		case DXGI_FORMAT_B8G8R8X8_TYPELESS:
		case DXGI_FORMAT_B8G8R8X8_UNORM_SRGB:
			return 4;
		case DXGI_FORMAT_R8G8_TYPELESS:
		case DXGI_FORMAT_R8G8_UNORM:
		case DXGI_FORMAT_R8G8_UINT:
		case DXGI_FORMAT_R8G8_SNORM:
		case DXGI_FORMAT_R8G8_SINT:
		case DXGI_FORMAT_R16_TYPELESS:
		case DXGI_FORMAT_R16_FLOAT:
		case DXGI_FORMAT_D16_UNORM:
		case DXGI_FORMAT_R16_UNORM:
		case DXGI_FORMAT_R16_UINT:
		case DXGI_FORMAT_R16_SNORM:
		case DXGI_FORMAT_R16_SINT:
		case DXGI_FORMAT_B5G6R5_UNORM:
		case DXGI_FORMAT_B5G5R5A1_UNORM:
			return 2;
		case DXGI_FORMAT_R8_TYPELESS:
		case DXGI_FORMAT_R8_UNORM:
		case DXGI_FORMAT_R8_UINT:
		case DXGI_FORMAT_R8_SNORM:
		case DXGI_FORMAT_R8_SINT:
		case DXGI_FORMAT_A8_UNORM:
			return 1;
		default:
			return 0;
	}
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
	height_multiply(1.0f)
{}

CustomResource::~CustomResource()
{
	if (resource)
		resource->Release();
	if (view)
		view->Release();
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

void CustomResource::Substantiate(ID3D11Device *mOrigDevice, StereoHandle mStereoHandle)
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
		LoadFromFile(mOrigDevice);
	} else {
		switch (override_type) {
			case CustomResourceType::BUFFER:
			case CustomResourceType::STRUCTURED_BUFFER:
			case CustomResourceType::RAW_BUFFER:
				SubstantiateBuffer(mOrigDevice, NULL, 0);
				break;
			case CustomResourceType::TEXTURE1D:
				SubstantiateTexture1D(mOrigDevice);
				break;
			case CustomResourceType::TEXTURE2D:
			case CustomResourceType::CUBE:
				SubstantiateTexture2D(mOrigDevice);
				break;
			case CustomResourceType::TEXTURE3D:
				SubstantiateTexture3D(mOrigDevice);
				break;
		}
	}

	if (restore_create_mode)
		NvAPI_Stereo_SetSurfaceCreationMode(mStereoHandle, orig_mode);
}

void CustomResource::LoadBufferFromFile(ID3D11Device *mOrigDevice)
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

	SubstantiateBuffer(mOrigDevice, &buf, size);

out_delete:
	free(buf);
out_close:
	CloseHandle(f);
}

void CustomResource::LoadFromFile(ID3D11Device *mOrigDevice)
{
	wstring ext;
	HRESULT hr;

	switch (override_type) {
		case CustomResourceType::BUFFER:
		case CustomResourceType::STRUCTURED_BUFFER:
		case CustomResourceType::RAW_BUFFER:
			return LoadBufferFromFile(mOrigDevice);
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
		hr = DirectX::CreateDDSTextureFromFileEx(mOrigDevice,
				filename.c_str(), 0,
				D3D11_USAGE_DEFAULT, bind_flags, 0, 0,
				false, &resource, NULL, NULL);
	} else {
		LogInfoW(L"Loading custom resource %s as WIC\n", filename.c_str());
		hr = DirectX::CreateWICTextureFromFileEx(mOrigDevice,
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

void CustomResource::SubstantiateBuffer(ID3D11Device *mOrigDevice, void **buf, DWORD size)
{
	D3D11_SUBRESOURCE_DATA data = {0}, *pInitialData = NULL;
	ID3D11Buffer *buffer;
	D3D11_BUFFER_DESC desc;
	HRESULT hr;

	memset(&desc, 0, sizeof(desc));
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = bind_flags;
	OverrideBufferDesc(&desc);

	if (buf) {
		// Fill in size from the file, allowing for an override to make
		// it larger or smaller, which may involve reallocating the
		// buffer from the caller.
		if (desc.ByteWidth <= 0) {
			desc.ByteWidth = size;
		} else if (desc.ByteWidth > size) {
			void *new_buf = realloc(*buf, desc.ByteWidth);
			if (!new_buf) {
				LogInfo("Out of memory enlarging buffer\n");
				return;
			}
			*buf = new_buf;
		}

		data.pSysMem = *buf;
		pInitialData = &data;
	}

	hr = mOrigDevice->CreateBuffer(&desc, pInitialData, &buffer);
	if (SUCCEEDED(hr)) {
		LogInfo("Substantiated custom %S resource\n",
				lookup_enum_name(CustomResourceTypeNames, override_type));
		LogDebugResourceDesc(&desc);
		resource = (ID3D11Resource*)buffer;
		is_null = false;
		if (override_format != (DXGI_FORMAT)-1)
			format = override_format;
	} else {
		LogInfo("Failed to substantiate custom %S resource: 0x%x\n",
				lookup_enum_name(CustomResourceTypeNames, override_type), hr);
		LogResourceDesc(&desc);
		BeepFailure();
	}
}
void CustomResource::SubstantiateTexture1D(ID3D11Device *mOrigDevice)
{
	ID3D11Texture1D *tex1d;
	D3D11_TEXTURE1D_DESC desc;
	HRESULT hr;

	memset(&desc, 0, sizeof(desc));
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = bind_flags;
	OverrideTexDesc(&desc);

	hr = mOrigDevice->CreateTexture1D(&desc, NULL, &tex1d);
	if (SUCCEEDED(hr)) {
		LogInfo("Substantiated custom %S resource\n",
				lookup_enum_name(CustomResourceTypeNames, override_type));
		LogDebugResourceDesc(&desc);
		resource = (ID3D11Resource*)tex1d;
		is_null = false;
	} else {
		LogInfo("Failed to substantiate custom %S resource: 0x%x\n",
				lookup_enum_name(CustomResourceTypeNames, override_type), hr);
		LogResourceDesc(&desc);
		BeepFailure();
	}
}
void CustomResource::SubstantiateTexture2D(ID3D11Device *mOrigDevice)
{
	ID3D11Texture2D *tex2d;
	D3D11_TEXTURE2D_DESC desc;
	HRESULT hr;

	memset(&desc, 0, sizeof(desc));
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = bind_flags;
	OverrideTexDesc(&desc);

	hr = mOrigDevice->CreateTexture2D(&desc, NULL, &tex2d);
	if (SUCCEEDED(hr)) {
		LogInfo("Substantiated custom %S resource\n",
				lookup_enum_name(CustomResourceTypeNames, override_type));
		LogDebugResourceDesc(&desc);
		resource = (ID3D11Resource*)tex2d;
		is_null = false;
	} else {
		LogInfo("Failed to substantiate custom %S resource: 0x%x\n",
				lookup_enum_name(CustomResourceTypeNames, override_type), hr);
		LogResourceDesc(&desc);
		BeepFailure();
	}
}
void CustomResource::SubstantiateTexture3D(ID3D11Device *mOrigDevice)
{
	ID3D11Texture3D *tex3d;
	D3D11_TEXTURE3D_DESC desc;
	HRESULT hr;

	memset(&desc, 0, sizeof(desc));
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = bind_flags;
	OverrideTexDesc(&desc);

	hr = mOrigDevice->CreateTexture3D(&desc, NULL, &tex3d);
	if (SUCCEEDED(hr)) {
		LogInfo("Substantiated custom %S resource\n",
				lookup_enum_name(CustomResourceTypeNames, override_type));
		LogDebugResourceDesc(&desc);
		resource = (ID3D11Resource*)tex3d;
		is_null = false;
	} else {
		LogInfo("Failed to substantiate custom %S resource: 0x%x\n",
				lookup_enum_name(CustomResourceTypeNames, override_type), hr);
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

	// XXX: Any reason to allow access to sequential swap chains? Given
	// they either won't exist or are read only I can't think of one.
	if (is_source && !wcscmp(target, L"bb")) { // Back Buffer
		type = ResourceCopyTargetType::SWAP_CHAIN;
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


bool ParseCommandListResourceCopyDirective(const wchar_t *key, wstring *val,
		CommandList *command_list)
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
				|| operation->src.type == ResourceCopyTargetType::INI_PARAMS))
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

	operation->ini_key = key;
	operation->ini_val = *val;
	command_list->push_back(std::shared_ptr<CommandListCommand>(operation));
	return true;
bail:
	delete operation;
	return false;
}

ID3D11Resource *ResourceCopyTarget::GetResource(
		HackerDevice *mHackerDevice,
		ID3D11Device *mOrigDevice,
		ID3D11DeviceContext *mOrigContext,
		ID3D11View **view,   // Used by textures, render targets, depth/stencil buffers & UAVs
		UINT *stride,        // Used by vertex buffers
		UINT *offset,        // Used by vertex & index buffers
		DXGI_FORMAT *format, // Used by index buffers
		UINT *buf_size,      // Used when creating a view of the buffer
		DrawCallInfo *call_info)
{
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
			mOrigContext->VSGetConstantBuffers(slot, 1, &buf);
			return buf;
		case L'h':
			mOrigContext->HSGetConstantBuffers(slot, 1, &buf);
			return buf;
		case L'd':
			mOrigContext->DSGetConstantBuffers(slot, 1, &buf);
			return buf;
		case L'g':
			mOrigContext->GSGetConstantBuffers(slot, 1, &buf);
			return buf;
		case L'p':
			mOrigContext->PSGetConstantBuffers(slot, 1, &buf);
			return buf;
		case L'c':
			mOrigContext->CSGetConstantBuffers(slot, 1, &buf);
			return buf;
		default:
			// Should not happen
			return NULL;
		}
		break;

	case ResourceCopyTargetType::SHADER_RESOURCE:
		switch(shader_type) {
		case L'v':
			mOrigContext->VSGetShaderResources(slot, 1, &resource_view);
			break;
		case L'h':
			mOrigContext->HSGetShaderResources(slot, 1, &resource_view);
			break;
		case L'd':
			mOrigContext->DSGetShaderResources(slot, 1, &resource_view);
			break;
		case L'g':
			mOrigContext->GSGetShaderResources(slot, 1, &resource_view);
			break;
		case L'p':
			mOrigContext->PSGetShaderResources(slot, 1, &resource_view);
			break;
		case L'c':
			mOrigContext->CSGetShaderResources(slot, 1, &resource_view);
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
		mOrigContext->IAGetVertexBuffers(slot, 1, &buf, stride, offset);

		// To simplify things we just copy the part of the buffer
		// referred to by this call, so adjust the offset with the
		// call-specific first vertex. Do NOT set the buffer size here
		// as if it's too small it will disable the region copy later.
		// TODO: Add a keyword to ignore offsets in case we want the
		// whole buffer regardless
		if (call_info)
			*offset += call_info->FirstVertex * *stride;
		return buf;

	case ResourceCopyTargetType::INDEX_BUFFER:
		// TODO: Similar comment as vertex buffers above, provide a
		// means for a shader to get format + offset.
		mOrigContext->IAGetIndexBuffer(&buf, format, offset);
		*stride = dxgi_format_size(*format);

		// To simplify things we just copy the part of the buffer
		// referred to by this call, so adjust the offset with the
		// call-specific first index. Do NOT set the buffer size here
		// as if it's too small it will disable the region copy later.
		// TODO: Add a keyword to ignore offsets in case we want the
		// whole buffer regardless
		if (call_info)
			*offset += call_info->FirstIndex * *stride;
		return buf;

	case ResourceCopyTargetType::STREAM_OUTPUT:
		// XXX: Does not give us the offset
		mOrigContext->SOGetTargets(slot + 1, so_bufs);

		// Release any buffers we aren't after:
		for (i = 0; i < slot; i++) {
			if (so_bufs[i]) {
				so_bufs[i]->Release();
				so_bufs[i] = NULL;
			}
		}

		return so_bufs[slot];

	case ResourceCopyTargetType::RENDER_TARGET:
		mOrigContext->OMGetRenderTargets(slot + 1, render_view, NULL);

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
		mOrigContext->OMGetRenderTargets(0, NULL, &depth_view);
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
			mOrigContext->OMGetRenderTargetsAndUnorderedAccessViews(0, NULL, NULL, slot, 1, &unordered_view);
			break;
		case L'c':
			mOrigContext->CSGetUnorderedAccessViews(slot, 1, &unordered_view);
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
		custom_resource->Substantiate(mOrigDevice, mHackerDevice->mStereoHandle);

		*stride = custom_resource->stride;
		*offset = custom_resource->offset;
		*format = custom_resource->format;
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
		*view = mHackerDevice->mStereoResourceView;
		return mHackerDevice->mStereoTexture;

	case ResourceCopyTargetType::INI_PARAMS:
		*view = mHackerDevice->mIniResourceView;
		return mHackerDevice->mIniTexture;

	case ResourceCopyTargetType::SWAP_CHAIN:
		mHackerDevice->GetOrigSwapChain()->GetBuffer(0, __uuidof(ID3D11Resource), (void**)&res);
		return res;

	}

	return NULL;
}

void ResourceCopyTarget::SetResource(
		ID3D11DeviceContext *mOrigContext,
		ID3D11Resource *res,
		ID3D11View *view,
		UINT stride,
		UINT offset,
		DXGI_FORMAT format,
		UINT buf_size)
{
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
			mOrigContext->VSSetConstantBuffers(slot, 1, &buf);
			return;
		case L'h':
			mOrigContext->HSSetConstantBuffers(slot, 1, &buf);
			return;
		case L'd':
			mOrigContext->DSSetConstantBuffers(slot, 1, &buf);
			return;
		case L'g':
			mOrigContext->GSSetConstantBuffers(slot, 1, &buf);
			return;
		case L'p':
			mOrigContext->PSSetConstantBuffers(slot, 1, &buf);
			return;
		case L'c':
			mOrigContext->CSSetConstantBuffers(slot, 1, &buf);
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
			mOrigContext->VSSetShaderResources(slot, 1, &resource_view);
			break;
		case L'h':
			mOrigContext->HSSetShaderResources(slot, 1, &resource_view);
			break;
		case L'd':
			mOrigContext->DSSetShaderResources(slot, 1, &resource_view);
			break;
		case L'g':
			mOrigContext->GSSetShaderResources(slot, 1, &resource_view);
			break;
		case L'p':
			mOrigContext->PSSetShaderResources(slot, 1, &resource_view);
			break;
		case L'c':
			mOrigContext->CSSetShaderResources(slot, 1, &resource_view);
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
		mOrigContext->IASetVertexBuffers(slot, 1, &buf, &stride, &offset);
		return;

	case ResourceCopyTargetType::INDEX_BUFFER:
		buf = (ID3D11Buffer*)res;
		mOrigContext->IASetIndexBuffer(buf, format, offset);
		break;

	case ResourceCopyTargetType::STREAM_OUTPUT:
		// XXX: HERE BE UNTESTED CODE PATHS!
		buf = (ID3D11Buffer*)res;
		mOrigContext->SOGetTargets(D3D11_SO_STREAM_COUNT, so_bufs);
		if (so_bufs[slot])
			so_bufs[slot]->Release();
		so_bufs[slot] = buf;
		// XXX: We set offsets to NULL here. We should really preserve
		// them, but I'm not sure how to get their original values,
		// so... too bad. Probably will never even use this anyway.
		mOrigContext->SOSetTargets(D3D11_SO_STREAM_COUNT, so_bufs, NULL);

		for (i = 0; i < D3D11_SO_STREAM_COUNT; i++) {
			if (so_bufs[i])
				so_bufs[i]->Release();
		}

		break;

	case ResourceCopyTargetType::RENDER_TARGET:
		mOrigContext->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, render_view, &depth_view);

		if (render_view[slot])
			render_view[slot]->Release();
		render_view[slot] = (ID3D11RenderTargetView*)view;

		mOrigContext->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, render_view, depth_view);

		for (i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++) {
			if (i != slot && render_view[i])
				render_view[i]->Release();
		}
		if (depth_view)
			depth_view->Release();

		break;

	case ResourceCopyTargetType::DEPTH_STENCIL_TARGET:
		mOrigContext->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, render_view, &depth_view);

		if (depth_view)
			depth_view->Release();
		depth_view = (ID3D11DepthStencilView*)view;

		mOrigContext->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, render_view, depth_view);

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
			mOrigContext->OMSetRenderTargetsAndUnorderedAccessViews(D3D11_KEEP_RENDER_TARGETS_AND_DEPTH_STENCIL,
				NULL, NULL, slot, 1, &unordered_view, &uav_counter);
			return;
		case L'c':
			// TODO: Allow pUAVInitialCounts to optionally be set
			mOrigContext->CSSetUnorderedAccessViews(slot, 1, &unordered_view, &uav_counter);
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
		// Only way we could "set" a resource to the back buffer is by
		// copying to it. Might implement overwrites later, but no
		// pressing need. To write something to the back buffer, assign
		// it as a render target instead.
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
			// N/A since swap chain can't be set as a destination
			return (D3D11_BIND_FLAG)0;
	}

	// Shouldn't happen. No return value makes sense, so raise an exception
	throw(std::range_error("Bad 3DMigoto ResourceCopyTarget"));
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
		ResourceCopyTarget *dst,
		ID3D11Buffer *src_resource,
		ID3D11Buffer *dst_resource,
		ID3D11View *src_view,
		D3D11_BIND_FLAG bind_flags,
		ID3D11Device *device,
		UINT stride,
		UINT offset,
		DXGI_FORMAT format,
		UINT *buf_dst_size)
{
	HRESULT hr;
	D3D11_BUFFER_DESC old_desc;
	D3D11_BUFFER_DESC new_desc;
	ID3D11Buffer *buffer = NULL;
	UINT dst_size;

	src_resource->GetDesc(&new_desc);
	new_desc.Usage = D3D11_USAGE_DEFAULT;
	new_desc.BindFlags = bind_flags;
	new_desc.CPUAccessFlags = 0;

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

	if (dst_resource) {
		// If destination already exists and the description is
		// identical we don't need to recreate it.
		dst_resource->GetDesc(&old_desc);
		if (!memcmp(&old_desc, &new_desc, sizeof(D3D11_BUFFER_DESC)))
			return NULL;
		LogInfo("RecreateCompatibleBuffer: Recreating cached resource\n");
	} else
		LogInfo("RecreateCompatibleBuffer: Creating cached resource\n");

	hr = device->CreateBuffer(&new_desc, NULL, &buffer);
	if (FAILED(hr)) {
		LogInfo("Resource copy RecreateCompatibleBuffer failed: 0x%x\n", hr);
		LogResourceDesc(&new_desc);
		return NULL;
	}

	LogDebugResourceDesc(&new_desc);

	return buffer;
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
		ResourceCopyTarget *dst,
		ResourceType *src_resource,
		ResourceType *dst_resource,
		D3D11_BIND_FLAG bind_flags,
		ID3D11Device *device,
		StereoHandle mStereoHandle,
		ResourceCopyOptions options)
{
	HRESULT hr;
	DescType old_desc;
	DescType new_desc;
	ResourceType *tex = NULL;

	src_resource->GetDesc(&new_desc);
	new_desc.Usage = D3D11_USAGE_DEFAULT;
	new_desc.BindFlags = bind_flags;
	new_desc.CPUAccessFlags = 0;

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

	if (dst_resource) {
		// If destination already exists and the description is
		// identical we don't need to recreate it.
		dst_resource->GetDesc(&old_desc);
		if (!memcmp(&old_desc, &new_desc, sizeof(DescType)))
			return NULL;
		LogInfo("RecreateCompatibleTexture: Recreating cached resource\n");
	} else
		LogInfo("RecreateCompatibleTexture: Creating cached resource\n");

	hr = (device->*CreateTexture)(&new_desc, NULL, &tex);
	if (FAILED(hr)) {
		LogInfo("Resource copy RecreateCompatibleTexture failed: 0x%x\n", hr);
		LogResourceDesc(&new_desc);
		src_resource->GetDesc(&old_desc);
		LogInfo("Original resource was:\n");
		LogResourceDesc(&old_desc);
		return NULL;
	}

	LogDebugResourceDesc(&new_desc);

	return tex;
}

static void RecreateCompatibleResource(
		ResourceCopyTarget *dst,
		ID3D11Resource *src_resource,
		ID3D11Resource **dst_resource,
		ID3D11View *src_view,
		ID3D11View **dst_view,
		ID3D11Device *device,
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
			LogInfo("RecreateCompatibleResource: Resource type changed\n");

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
	} else if (dst->type == ResourceCopyTargetType::CUSTOM_RESOURCE) {
		restore_create_mode = dst->custom_resource->OverrideSurfaceCreationMode(mStereoHandle, &orig_mode);
	}

	switch (src_dimension) {
		case D3D11_RESOURCE_DIMENSION_BUFFER:
			res = RecreateCompatibleBuffer(dst, (ID3D11Buffer*)src_resource, (ID3D11Buffer*)*dst_resource, src_view,
					bind_flags, device, stride, offset, format, buf_dst_size);
			break;
		case D3D11_RESOURCE_DIMENSION_TEXTURE1D:
			res = RecreateCompatibleTexture<ID3D11Texture1D, D3D11_TEXTURE1D_DESC, &ID3D11Device::CreateTexture1D>
				(dst, (ID3D11Texture1D*)src_resource, (ID3D11Texture1D*)*dst_resource, bind_flags,
				 device, mStereoHandle, options);
			break;
		case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
			res = RecreateCompatibleTexture<ID3D11Texture2D, D3D11_TEXTURE2D_DESC, &ID3D11Device::CreateTexture2D>
				(dst, (ID3D11Texture2D*)src_resource, (ID3D11Texture2D*)*dst_resource, bind_flags,
				 device, mStereoHandle, options);
			break;
		case D3D11_RESOURCE_DIMENSION_TEXTURE3D:
			res = RecreateCompatibleTexture<ID3D11Texture3D, D3D11_TEXTURE3D_DESC, &ID3D11Device::CreateTexture3D>
				(dst, (ID3D11Texture3D*)src_resource, (ID3D11Texture3D*)*dst_resource, bind_flags,
				 device, mStereoHandle, options);
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
	desc->Buffer.FirstElement = offset / stride;
	desc->Buffer.NumElements = (buf_src_size - offset) / stride;
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
		ID3D11Device *device,
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

	hr = (device->*CreateView)(resource, pDesc, &view);
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
		ID3D11Device *device,
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
				       (resource, device, stride, offset, format, buf_src_size);
		case ResourceCopyTargetType::RENDER_TARGET:
			return _CreateCompatibleView<ID3D11RenderTargetView,
			       D3D11_RENDER_TARGET_VIEW_DESC,
			       &ID3D11Device::CreateRenderTargetView>
				       (resource, device, stride, offset, format, buf_src_size);
		case ResourceCopyTargetType::DEPTH_STENCIL_TARGET:
			return _CreateCompatibleView<ID3D11DepthStencilView,
			       D3D11_DEPTH_STENCIL_VIEW_DESC,
			       &ID3D11Device::CreateDepthStencilView>
				       (resource, device, stride, offset, format, buf_src_size);
		case ResourceCopyTargetType::UNORDERED_ACCESS_VIEW:
			return _CreateCompatibleView<ID3D11UnorderedAccessView,
			       D3D11_UNORDERED_ACCESS_VIEW_DESC,
			       &ID3D11Device::CreateUnorderedAccessView>
				       (resource, device, stride, offset, format, buf_src_size);
	}
	return NULL;
}

static void SetViewportFromResource(ID3D11DeviceContext *mOrigContext, ID3D11Resource *resource)
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

	mOrigContext->RSSetViewports(1, &viewport);
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

static void ResolveMSAA(ID3D11Resource *dst_resource, ID3D11Resource *src_resource,
		ID3D11Device *mOrigDevice, ID3D11DeviceContext *mOrigContext)
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

	hr = mOrigDevice->CheckFormatSupport( fmt, &support );
	if (FAILED(hr) || !(support & D3D11_FORMAT_SUPPORT_MULTISAMPLE_RESOLVE)) {
		// TODO: Implement a fallback using a SM5 shader to resolve it
		LogInfo("Resource copy cannot resolve MSAA format %d\n", fmt);
		return;
	}

	for (item = 0; item < desc.ArraySize; item++) {
		for (level = 0; level < desc.MipLevels; level++) {
			index = D3D11CalcSubresource(level, item, max(desc.MipLevels, 1));
			mOrigContext->ResolveSubresource(dst, index, src, index, fmt);
		}
	}
}

static void ReverseStereoBlit(ID3D11Resource *dst_resource, ID3D11Resource *src_resource,
		StereoHandle mStereoHandle, ID3D11DeviceContext *mOrigContext)
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

	nvret = NvAPI_Stereo_ReverseStereoBlitControl(mStereoHandle, true);
	if (nvret != NVAPI_OK) {
		LogInfo("Resource copying failed to enable reverse stereo blit\n");
		// Fallback path: Copy 2D resource to both sides of the 2x
		// width destination (TESTME)
		fallback = 1;
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
				mOrigContext->CopySubresourceRegion(dst_resource, index,
						fallbackside * srcBox.right, 0, 0,
						src, index, &srcBox);
			}
		}
	}

	NvAPI_Stereo_ReverseStereoBlitControl(mStereoHandle, false);
}

static void SpecialCopyBufferRegion(ID3D11Resource *dst_resource,ID3D11Resource *src_resource,
		ID3D11DeviceContext *mOrigContext, UINT stride, UINT *offset,
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

	mOrigContext->CopySubresourceRegion(dst_resource, 0, 0, 0, 0, src_resource, 0, &src_box);

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

	if (!*stride)
		*stride = dxgi_format_size(*format);
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

void ResourceCopyOperation::run(HackerDevice *mHackerDevice, HackerContext *mHackerContext,
		ID3D11Device *mOrigDevice, ID3D11DeviceContext *mOrigContext,
		CommandListState *state)
{
	ID3D11Resource *src_resource = NULL;
	ID3D11Resource *dst_resource = NULL;
	ID3D11Resource **pp_cached_resource = &cached_resource;
	ID3D11View *src_view = NULL;
	ID3D11View *dst_view = NULL;
	ID3D11View **pp_cached_view = &cached_view;
	UINT stride = 0;
	UINT offset = 0;
	DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
	UINT buf_src_size = 0, buf_dst_size = 0;

	mHackerContext->FrameAnalysisLog("3DMigoto %S = %S\n", ini_key.c_str(), ini_val.c_str());

	if (src.type == ResourceCopyTargetType::EMPTY) {
		dst.SetResource(mOrigContext, NULL, NULL, 0, 0, DXGI_FORMAT_UNKNOWN, 0);
		return;
	}

	src_resource = src.GetResource(mHackerDevice, mOrigDevice, mOrigContext, &src_view, &stride, &offset, &format, &buf_src_size, state->call_info);
	if (!src_resource) {
		LogDebug("Resource copy: Source was NULL\n");
		if (!(options & ResourceCopyOptions::UNLESS_NULL)) {
			// Still set destination to NULL - if we are copying a
			// resource we generally expect it to be there, and
			// this will make errors more obvious if we copy
			// something that doesn't exist. This behaviour can be
			// overridden with the unless_null keyword.
			dst.SetResource(mOrigContext, NULL, NULL, 0, 0, DXGI_FORMAT_UNKNOWN, 0);
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
		pp_cached_view = &dst.custom_resource->view;

		if (dst.custom_resource->max_copies_per_frame) {
			if (dst.custom_resource->frame_no != G->frame_no) {
				dst.custom_resource->frame_no = G->frame_no;
				dst.custom_resource->copies_this_frame = 1;
			} else if (dst.custom_resource->copies_this_frame++ >= dst.custom_resource->max_copies_per_frame) {
				mHackerContext->FrameAnalysisLog("max_copies_per_frame exceeded\n");
				return;
			}
		}

		dst.custom_resource->OverrideOutOfBandInfo(&format, &stride);
	}

	FillInMissingInfo(src.type, src_resource, src_view, &stride, &offset, &buf_src_size, &format);

	if (options & ResourceCopyOptions::COPY_MASK) {
		RecreateCompatibleResource(&dst, src_resource,
			pp_cached_resource, src_view, pp_cached_view,
			mOrigDevice, mHackerDevice->mStereoHandle,
			options, stride, offset, format, &buf_dst_size);

		if (!*pp_cached_resource) {
			LogInfo("Resource copy error: Could not create/update destination resource\n");
			goto out_release;
		}
		dst_resource = *pp_cached_resource;
		dst_view = *pp_cached_view;

		if (options & ResourceCopyOptions::COPY_DESC) {
			// RecreateCompatibleResource has already done the work
			mHackerContext->FrameAnalysisLog("3DMigoto copying resource description\n");
		} else if (options & ResourceCopyOptions::STEREO2MONO) {
			mHackerContext->FrameAnalysisLog("3DMigoto performing reverse stereo blit\n");

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

			RecreateCompatibleResource(NULL, src_resource,
				&stereo2mono_intermediate, NULL, NULL,
				mOrigDevice, mHackerDevice->mStereoHandle,
				(ResourceCopyOptions)(options | ResourceCopyOptions::STEREO),
				stride, offset, format, NULL);

			ReverseStereoBlit(stereo2mono_intermediate, src_resource,
					mHackerDevice->mStereoHandle, mOrigContext);

			mOrigContext->CopyResource(dst_resource, stereo2mono_intermediate);

		} else if (options & ResourceCopyOptions::RESOLVE_MSAA) {
			mHackerContext->FrameAnalysisLog("3DMigoto resolving MSAA\n");
			ResolveMSAA(dst_resource, src_resource, mOrigDevice, mOrigContext);
		} else if (buf_dst_size) {
			mHackerContext->FrameAnalysisLog("3DMigoto performing region copy\n");
			SpecialCopyBufferRegion(dst_resource, src_resource,
					mOrigContext, stride, &offset,
					buf_src_size, buf_dst_size);
		} else {
			mHackerContext->FrameAnalysisLog("3DMigoto performing full copy\n");
			mOrigContext->CopyResource(dst_resource, src_resource);
		}
	} else {
		mHackerContext->FrameAnalysisLog("3DMigoto copying by reference\n");
		dst_resource = src_resource;
		if (src_view && (src.type == dst.type)) {
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
		dst_view = CreateCompatibleView(&dst, dst_resource, mOrigDevice,
				stride, offset, format, buf_src_size);
		// Not checking for NULL return as view's are not applicable to
		// all types. Legitimate failures are logged.
		*pp_cached_view = dst_view;
	}

	dst.SetResource(mOrigContext, dst_resource, dst_view, stride, offset, format, buf_dst_size);

	if (options & ResourceCopyOptions::SET_VIEWPORT)
		SetViewportFromResource(mOrigContext, dst_resource);

out_release:
	src_resource->Release();
	if (src_view)
		src_view->Release();
	if (options & ResourceCopyOptions::NO_VIEW_CACHE && *pp_cached_view) {
		(*pp_cached_view)->Release();
		*pp_cached_view = NULL;
	}
}
