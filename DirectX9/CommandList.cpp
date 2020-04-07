#include "CommandList.h"
#include "IniHandler.h"
#include <algorithm>
#include <sstream>
#include "Override.h"
#include <D3DCompiler.h>
#define getA(c) (((c)&0xff000000)>>24)
#define getR(c) (((c)&0x00ff0000)>>16)
#define getG(c) (((c)&0x0000ff00)>>8)
#define getB(c) ((c)&0x000000ff)
#define D3D_COMPILE_STANDARD_FILE_INCLUDE ((ID3DInclude*)(UINT_PTR)1)

CustomResources customResources;
CustomShaders customShaders;
ExplicitCommandListSections explicitCommandListSections;
CustomFunctions customFunctions;
InternalFuntionFloats internalFunctionFloats = {
	{ InternalFunctionFloat::ACSTATE_LAST_SET_CONVERGENCE , 0.0f },
	{ InternalFunctionFloat::ACSTATE_USER_POPOUT_BIAS ,  0.0f },
	{ InternalFunctionFloat::ACSTATE_JUDDER ,  0.0f },
	{ InternalFunctionFloat::ACSTATE_JUDDER_TIME ,  0.0f },
	{ InternalFunctionFloat::ACSTATE_LAST_CONVERGENCE1 ,  0.0f },
	{ InternalFunctionFloat::ACSTATE_LAST_CONVERGENCE2 ,  0.0f },
	{ InternalFunctionFloat::ACSTATE_LAST_CONVERGENCE3 ,  0.0f },
	{ InternalFunctionFloat::ACSTATE_LAST_CONVERGENCE4 ,  0.0f },
	{ InternalFunctionFloat::ACSTATE_TIME ,  0.0f },
	{ InternalFunctionFloat::ACSTATE_PREV_TIME ,  0.0f },
	{ InternalFunctionFloat::ACSTATE_CORRECTION_START ,  0.0f},
	{ InternalFunctionFloat::ACSTATE_LAST_CALCULATED_CONVERGENCE ,  0.0f},
};
unordered_map<BuiltInFunctionName, wstring> builtInFunctionDesc = {
	{ BuiltInFunctionName::AUTOCONVERGENCE , L"insert desc here, including parameter def" }
};
BuiltInFunctions builtInFunctions = {
	{ BuiltInFunctionName::AUTOCONVERGENCE , new FunctionAutoConvergence() }
};
ResourceCopyOperations resourceCopyOperations;
CommandListVariableFloats command_list_globals;
std::vector<CommandListVariableFloat*> persistent_variables;

CommandListVariableArrays command_list_global_arrays;
std::vector<CommandListVariableArray*> persistent_variable_arrays;
CommandListMatrices command_list_global_matrices;
std::vector<CommandListMatrix*> persistent_matrices;

std::vector<CommandList*> registered_command_lists;
std::unordered_set<CommandList*> command_lists_profiling;
std::unordered_set<CommandListCommand*> command_lists_cmd_profiling;
std::vector<std::shared_ptr<CommandList>> dynamically_allocated_command_lists;
NvAPI_Status SetConvergence(D3D9Wrapper::IDirect3DDevice9 *device, CachedStereoValues *cachedStereoValues, float convergence)
{
	cachedStereoValues->KnownConvergence = convergence;
	return Profiling::NvAPI_Stereo_SetConvergence(device->mStereoHandle, convergence);
}
NvAPI_Status SetSeparation(D3D9Wrapper::IDirect3DDevice9 *device, CachedStereoValues *cachedStereoValues, float seperation)
{
	cachedStereoValues->KnownSeparation = seperation;
	return Profiling::NvAPI_Stereo_SetSeparation(device->mStereoHandle, seperation);
}

NvAPI_Status GetConvergence(D3D9Wrapper::IDirect3DDevice9 *device, CachedStereoValues *cachedStereoValues, float * convergence)
{
	NvAPI_Status nret;
	if (cachedStereoValues->KnownConvergence != -1) {
		*convergence = cachedStereoValues->KnownConvergence;
		nret = NvAPI_Status::NVAPI_OK;
	}
	else {
		nret = Profiling::NvAPI_Stereo_GetConvergence(device->mStereoHandle, convergence);
		cachedStereoValues->KnownConvergence = *convergence;
	}
	return nret;
}

NvAPI_Status GetSeparation(D3D9Wrapper::IDirect3DDevice9 *device, CachedStereoValues *cachedStereoValues, float * seperation)
{
	NvAPI_Status nret;
	if (cachedStereoValues->KnownSeparation != -1) {
		*seperation = cachedStereoValues->KnownSeparation;
		nret = NvAPI_Status::NVAPI_OK;
	}
	else {
		nret = Profiling::NvAPI_Stereo_GetSeparation(device->mStereoHandle, seperation);
		cachedStereoValues->KnownSeparation = *seperation;
	}
	return nret;
}

NvAPI_Status GetEyeSeparation(D3D9Wrapper::IDirect3DDevice9 *device, CachedStereoValues *cachedStereoValues, float * eyeseperation)
{
	NvAPI_Status nret;
	if (cachedStereoValues->KnownEyeSeparation != -1) {
		*eyeseperation = cachedStereoValues->KnownEyeSeparation;
		nret = NvAPI_Status::NVAPI_OK;
	}
	else {
		nret = Profiling::NvAPI_Stereo_GetEyeSeparation(device->mStereoHandle, eyeseperation);
		cachedStereoValues->KnownEyeSeparation = *eyeseperation;
	}
	return nret;
}

NvAPI_Status GetStereoActive(D3D9Wrapper::IDirect3DDevice9 *device, CachedStereoValues *cachedStereoValues, bool * active)
{
	NvAPI_Status nret;
	if (cachedStereoValues->StereoActiveIsKnown) {
		*active = cachedStereoValues->KnownStereoActive;
		nret = NvAPI_Status::NVAPI_OK;
	}
	else {
		NvU8 _active;
		nret = Profiling::NvAPI_Stereo_IsActivated(device->mStereoHandle, &_active);
		*active = !!_active;
		cachedStereoValues->KnownStereoActive = *active;
		cachedStereoValues->StereoActiveIsKnown = true;
	}
	return nret;
}
NvAPI_Status GetStereoEnabled(CachedStereoValues *cachedStereoValues, bool *enabled)
{
	NvAPI_Status nret;
	if (cachedStereoValues->StereoEnabledIsKnown) {
		*enabled = cachedStereoValues->KnownStereoEnabled;
		nret = NvAPI_Status::NVAPI_OK;
	}
	else {
		NvU8 _enabled;
		nret = Profiling::NvAPI_Stereo_IsEnabled(&_enabled);
		*enabled = !!_enabled;
		cachedStereoValues->KnownStereoEnabled = *enabled;
		cachedStereoValues->StereoEnabledIsKnown = true;
	}
	return nret;
}
// Adds consistent "3DMigoto" prefix to frame analysis log with appropriate
// level of indentation for the current recursion level. Using a
// macro instead of a function for this to concatenate static strings:
#define COMMAND_LIST_LOG(state, fmt, ...) \
	do { \
		(state)->mHackerDevice->FrameAnalysisLog("3DMigoto%*s " fmt, state->recursion, "", __VA_ARGS__); \
	} while (0)
struct command_list_profiling_state {
	LARGE_INTEGER list_start_time;
	LARGE_INTEGER cmd_start_time;
	LARGE_INTEGER saved_recursive_time;
};

static inline void profile_command_list_start(CommandList *command_list, CommandListState *state,
	command_list_profiling_state *profiling_state)
{
	bool inserted;

	if ((Profiling::mode != Profiling::Mode::SUMMARY)
		&& (Profiling::mode != Profiling::Mode::TOP_COMMAND_LISTS))
		return;

	inserted = command_lists_profiling.insert(command_list).second;
	if (inserted) {
		command_list->time_spent_inclusive.QuadPart = 0;
		command_list->time_spent_exclusive.QuadPart = 0;
		command_list->executions = 0;
	}

	profiling_state->saved_recursive_time = state->profiling_time_recursive;
	state->profiling_time_recursive.QuadPart = 0;

	QueryPerformanceCounter(&profiling_state->list_start_time);
}

static inline void profile_command_list_end(CommandList *command_list, CommandListState *state,
	command_list_profiling_state *profiling_state)
{
	LARGE_INTEGER list_end_time, duration;

	if ((Profiling::mode != Profiling::Mode::SUMMARY)
		&& (Profiling::mode != Profiling::Mode::TOP_COMMAND_LISTS))
		return;

	QueryPerformanceCounter(&list_end_time);
	duration.QuadPart = list_end_time.QuadPart - profiling_state->list_start_time.QuadPart;
	command_list->time_spent_inclusive.QuadPart += duration.QuadPart;
	command_list->time_spent_exclusive.QuadPart += duration.QuadPart - state->profiling_time_recursive.QuadPart;
	command_list->executions++;
	state->profiling_time_recursive.QuadPart = profiling_state->saved_recursive_time.QuadPart + duration.QuadPart;
}

static inline void profile_command_list_cmd_start(CommandListCommand *cmd,
	command_list_profiling_state *profiling_state)
{
	bool inserted;

	if (Profiling::mode != Profiling::Mode::TOP_COMMANDS)
		return;

	inserted = command_lists_cmd_profiling.insert(cmd).second;
	if (inserted) {
		cmd->pre_time_spent.QuadPart = 0;
		cmd->post_time_spent.QuadPart = 0;
		cmd->pre_executions = 0;
		cmd->post_executions = 0;
	}

	QueryPerformanceCounter(&profiling_state->cmd_start_time);
}

static inline void profile_command_list_cmd_end(CommandListCommand *cmd, CommandListState *state,
	command_list_profiling_state *profiling_state)
{
	LARGE_INTEGER end_time;

	if (Profiling::mode != Profiling::Mode::TOP_COMMANDS)
		return;

	QueryPerformanceCounter(&end_time);
	if (state->post) {
		cmd->post_time_spent.QuadPart += end_time.QuadPart - profiling_state->cmd_start_time.QuadPart;
		cmd->post_executions++;
	}
	else {
		cmd->pre_time_spent.QuadPart += end_time.QuadPart - profiling_state->cmd_start_time.QuadPart;
		cmd->pre_executions++;
	}
}
static void _RunCommandList(CommandList *command_list, CommandListState *state, bool recursive = true)
{
	CommandList::Commands::iterator i;
	command_list_profiling_state profiling_state;

	if (state->recursion > MAX_COMMAND_LIST_RECURSION) {
		LogOverlay(LOG_WARNING, "WARNING: Command list recursion limit exceeded! Circular reference?\n");
		return;
	}

	if (command_list->commands.empty())
		return;

	if (recursive) {
		COMMAND_LIST_LOG(state, "%s {\n", state->post ? "post" : "pre");
		state->recursion++;
	}

	profile_command_list_start(command_list, state, &profiling_state);

	for (i = command_list->commands.begin(); i < command_list->commands.end() && !state->aborted; i++) {
		profile_command_list_cmd_start(i->get(), &profiling_state);
		(*i)->run(state);
		profile_command_list_cmd_end(i->get(), state, &profiling_state);
	}

	profile_command_list_end(command_list, state, &profiling_state);

	if (recursive) {
		state->recursion--;
		COMMAND_LIST_LOG(state, "}\n");
	}
}

static void CommandListFlushState(CommandListState *state)
{
	if (state->update_params && G->IniConstants.size() > 0) {

		for (std::map<int, DirectX::XMFLOAT4>::iterator it = G->IniConstants.begin(); it != G->IniConstants.end(); ++it) {
			float pConstants[4] = { it->second.x, it->second.y, it->second.z, it->second.w };
			state->mOrigDevice->SetVertexShaderConstantF(it->first, pConstants, 1);
			state->mOrigDevice->SetPixelShaderConstantF(it->first, pConstants, 1);

		}
		state->update_params = false;
		Profiling::iniparams_updates++;
	}
}

static void RunCommandListComplete(D3D9Wrapper::IDirect3DDevice9 *mHackerDevice,
	CommandList *command_list,
	DrawCallInfo *call_info,
	IDirect3DResource9 *resource,
	bool post, CachedStereoValues *cachedStereoValues)
{
	CommandListState state;
	state.mHackerDevice = mHackerDevice;
	state.mOrigDevice = mHackerDevice->GetD3D9Device();
	state.call_info = call_info;
	state.resource = resource;
	state.post = post;
	CachedStereoValues _cachedStereoValues;
	if (cachedStereoValues)
		state.cachedStereoValues = cachedStereoValues;
	else
		state.cachedStereoValues = &_cachedStereoValues;
	_RunCommandList(command_list, &state);
	CommandListFlushState(&state);
}

void RunCommandList(D3D9Wrapper::IDirect3DDevice9 *mHackerDevice,
	CommandList *command_list,
	DrawCallInfo *call_info,
	bool post, CachedStereoValues *cachedStereoValues)
{
	RunCommandListComplete(mHackerDevice, command_list,
		call_info, NULL, post, cachedStereoValues);
}

void RunResourceCommandList(D3D9Wrapper::IDirect3DDevice9 *mHackerDevice,
	CommandList *command_list,
	IDirect3DResource9 *resource,
	bool post, CachedStereoValues *cachedStereoValues)
{
	RunCommandListComplete(mHackerDevice, command_list,
		NULL, resource, post, cachedStereoValues);
}
void optimise_command_lists(D3D9Wrapper::IDirect3DDevice9 *device)
{
	bool making_progress;
	bool ignore_cto_pre, ignore_cto_post;
	size_t i;
	CommandList::Commands::iterator new_end;
	DWORD start;

	LogInfo("Optimising command lists...\n");
	start = GetTickCount();

	for (CommandList *command_list : registered_command_lists) {
		for (i = 0; i < command_list->commands.size(); i++)
			command_list->commands[i]->optimise(device);
	}

	do {
		making_progress = false;
		ignore_cto_pre = true;
		ignore_cto_post = true;

		// If all TextureOverride sections have empty command lists of
		// either pre or post, we can treat checktextureoverride as a
		// noop. This is intended to catch the case where we only have
		// "pre" commands in the TextureOverride sections to optimise
		// out the implicit "post checktextureoverride" commands, as
		// these can add up if they are used on very common shaders
		// (e.g. in DOAXVV this can easily save 0.2fps on a 4GHz CPU,
		// more on a slower CPU since this command is used in the
		// shadow map shaders)
		//
		// FIXME: This should itself ignore any checktextureoverrides
		// inside these command lists
		for (auto &tolkv : G->mTextureOverrideMap) {
			for (TextureOverride &to : tolkv.second) {
				ignore_cto_pre = ignore_cto_pre && to.command_list.commands.empty();
				ignore_cto_post = ignore_cto_post && to.post_command_list.commands.empty();
			}
		}
		for (auto &tof : G->mFuzzyTextureOverrides) {
			ignore_cto_pre = ignore_cto_pre && tof->texture_override->command_list.commands.empty();
			ignore_cto_post = ignore_cto_post && tof->texture_override->post_command_list.commands.empty();
		}

		// Go through each registered command list and remove any
		// commands that are noops to eliminate the runtime overhead of
		// processing these
		for (CommandList *command_list : registered_command_lists) {
			for (i = 0; i < command_list->commands.size(); ) {
				if (command_list->commands[i]->noop(command_list->post, ignore_cto_pre, ignore_cto_post)) {
					LogInfo("Optimised out %s %S\n",
						command_list->post ? "post" : "pre",
						command_list->commands[i]->ini_line.c_str());
					command_list->commands.erase(command_list->commands.begin() + i);
					making_progress = true;
					continue;
				}
				i++;
			}
		}

		// TODO: Merge adjacent commands if possible, e.g. all the
		// commands in BuiltInCommandListUnbindAllRenderTargets would
		// be good candidates to merge into a single command. We could
		// add a special command for that particular case, but would be
		// nice if this sort of thing worked more generally.
	} while (making_progress);

	Profiling::update_cto_warning(!ignore_cto_post);

	LogInfo("Command List Optimiser finished after %ums\n", GetTickCount() - start);
	registered_command_lists.clear();
	dynamically_allocated_command_lists.clear();
}
static bool AddCommandToList(CommandListCommand *command,
	CommandList *explicit_command_list,
	CommandList *sensible_command_list,
	CommandList *pre_command_list,
	CommandList *post_command_list,
	const wchar_t *section,
	const wchar_t *key, wstring *val)
{
	if (section && key) {
		command->ini_line = L"[" + wstring(section) + L"] " + wstring(key);
		if (val)
			command->ini_line += L" = " + *val;
	}

	if (explicit_command_list) {
		// User explicitly specified "pre" or "post", so only add the
		// command to that list
		explicit_command_list->commands.push_back(std::shared_ptr<CommandListCommand>(command));
	}
	else if (sensible_command_list) {
		// User did not specify which command list to add it to, but
		// the command they specified has a sensible default, so add it
		// to that list:
		sensible_command_list->commands.push_back(std::shared_ptr<CommandListCommand>(command));
	}
	else {
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
		pre_command_list->commands.push_back(p);
		if (post_command_list)
			post_command_list->commands.push_back(p);
	}

	return true;
}
static bool ParseCheckTextureOverride(const wchar_t *section,
	const wchar_t *key, wstring *val,
	CommandList *explicit_command_list,
	CommandList *pre_command_list,
	CommandList *post_command_list,
	const wstring *ini_namespace)
{
	int ret;

	CheckTextureOverrideCommand *operation = new CheckTextureOverrideCommand();

	// Parse value as consistent with texture filtering and resource copying
	ret = operation->target.ParseTarget(val->c_str(), true, ini_namespace);
	if (ret) {
		// If the user indicated an explicit command list we will run the pre
		// and post lists of the target list together.
		if (explicit_command_list)
			operation->run_pre_and_post_together = true;
		else if (post_command_list)
			G->implicit_post_checktextureoverride_used = true;

		return AddCommandToList(operation, explicit_command_list, NULL, pre_command_list, post_command_list, section, key, val);
	}

	delete operation;
	return false;
}
static bool ParseResetPerFrameLimits(const wchar_t *section,
	const wchar_t *key, wstring *val,
	CommandList *explicit_command_list,
	CommandList *pre_command_list,
	CommandList *post_command_list,
	const wstring *ini_namespace)
{
	CustomResources::iterator res;
	CustomShaders::iterator shader;
	wstring namespaced_section;

	ResetPerFrameLimitsCommand *operation = new ResetPerFrameLimitsCommand();

	if (!wcsncmp(val->c_str(), L"resource", 8)) {
		wstring resource_id(val->c_str());

		res = customResources.end();
		if (get_namespaced_section_name_lower(&resource_id, ini_namespace, &namespaced_section, &migoto_ini))
			res = customResources.find(namespaced_section);
		if (res == customResources.end())
			res = customResources.find(resource_id);
		if (res == customResources.end())
			goto bail;

		operation->resource = &res->second;
	}

	if (!wcsncmp(val->c_str(), L"customshader", 12) || !wcsncmp(val->c_str(), L"builtincustomshader", 19)) {
		wstring shader_id(val->c_str());

		shader = customShaders.end();
		if (get_namespaced_section_name_lower(&shader_id, ini_namespace, &namespaced_section, &migoto_ini))
			shader = customShaders.find(namespaced_section);
		if (shader == customShaders.end())
			shader = customShaders.find(shader_id);
		if (shader == customShaders.end())
			goto bail;

		operation->shader = &shader->second;
	}

	return AddCommandToList(operation, explicit_command_list, pre_command_list, NULL, NULL, section, key, val);

bail:
	delete operation;
	return false;
}
static bool ParseClearSurface(const wchar_t *section,
	const wchar_t *key, wstring *val,
	CommandList *explicit_command_list,
	CommandList *pre_command_list,
	CommandList *post_command_list,
	const wstring *ini_namespace)
{
	CustomResources::iterator res;
	CustomShaders::iterator shader;
	wistringstream token_stream(*val);
	wstring token;
	int ret, len1;
	int idx = 0;
	unsigned uval;
	float fval;

	ClearSurfaceCommand *operation = new ClearSurfaceCommand();

	while (getline(token_stream, token, L' ')) {
		if (operation->target.type == ResourceCopyTargetType::INVALID) {
			ret = operation->target.ParseTarget(token.c_str(), true, ini_namespace);
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
				//operation->clear_uav_uint = true;
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

	return AddCommandToList(operation, explicit_command_list, pre_command_list, NULL, NULL, section, key, val);

bail:
	delete operation;
	return false;
}


static bool ParseRunShader(const wchar_t *section,
	const wchar_t *key, wstring *val,
	CommandList *explicit_command_list,
	CommandList *pre_command_list,
	CommandList *post_command_list,
	const wstring *ini_namespace)
{
	RunCustomShaderCommand *operation = new RunCustomShaderCommand();
	CustomShaders::iterator shader;
	wstring namespaced_section;

	// Value should already have been transformed to lower case from
	// ParseCommandList, so our keys will be consistent in the
	// unordered_map:
	wstring shader_id(val->c_str());

	shader = customShaders.end();
	if (get_namespaced_section_name_lower(&shader_id, ini_namespace, &namespaced_section, &migoto_ini))
		shader = customShaders.find(namespaced_section);
	if (shader == customShaders.end())
		shader = customShaders.find(shader_id);
	if (shader == customShaders.end())
		goto bail;

	operation->custom_shader = &shader->second;
	return AddCommandToList(operation, explicit_command_list, pre_command_list, NULL, NULL, section, key, val);

bail:
	delete operation;
	return false;
}

bool ParseRunExplicitCommandList(const wchar_t *section,
	const wchar_t *key, wstring *val,
	CommandList *explicit_command_list,
	CommandList *pre_command_list,
	CommandList *post_command_list,
	const wstring *ini_namespace)
{
	RunExplicitCommandList *operation = new RunExplicitCommandList();
	ExplicitCommandListSections::iterator shader;
	wstring namespaced_section;

	// We need value in lower case so our keys will be consistent in the
	// unordered_map. ParseCommandList will have already done this, but the
	// Key/Preset parsing code will not have, and rather than require it to
	// we do it here:
	wstring section_id(val->c_str());
	std::transform(section_id.begin(), section_id.end(), section_id.begin(), ::towlower);

	shader = explicitCommandListSections.end();
	if (get_namespaced_section_name_lower(&section_id, ini_namespace, &namespaced_section, &migoto_ini))
		shader = explicitCommandListSections.find(namespaced_section);
	if (shader == explicitCommandListSections.end())
		shader = explicitCommandListSections.find(section_id);
	if (shader == explicitCommandListSections.end())
		goto bail;

	// If the user indicated an explicit command list we will run the pre
	// and post lists of the target list together. This tends to make
	// things a little less surprising for "post run = CommandListFoo"
	if (explicit_command_list)
		operation->run_pre_and_post_together = true;

	operation->command_list_section = &shader->second;
	// This function is nearly identical to ParseRunShader, but in case we
	// later refactor these together note that here we do not specify a
	// sensible command list, so it will be added to both pre and post
	// command lists:
	return AddCommandToList(operation, explicit_command_list, NULL, pre_command_list, post_command_list, section, key, val);

bail:
	delete operation;
	return false;
}
static bool ParsePreset(const wchar_t *section,
	const wchar_t *key, wstring *val,
	CommandList *explicit_command_list,
	CommandList *pre_command_list,
	CommandList *post_command_list,
	bool exclude, const wstring *ini_namespace)
{
	PresetCommand *operation = new PresetCommand();
	wstring prefixed_section, namespaced_section;

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
	prefixed_section = wstring(L"preset") + preset_id;

	// And now with namespacing, we have four permutations to try...
	i = presetOverrides.end();
	// First, add the 'Preset' (i.e. the user did not) and try namespaced:
	if (get_namespaced_section_name_lower(&prefixed_section, ini_namespace, &namespaced_section, &migoto_ini))
		i = presetOverrides.find(namespaced_section);
	// Second, try namespaced without adding the prefix:
	if (i == presetOverrides.end()) {
		if (get_namespaced_section_name_lower(&preset_id, ini_namespace, &namespaced_section, &migoto_ini))
			i = presetOverrides.find(namespaced_section);
	}
	// Third, add the 'Preset' and try global:
	if (i == presetOverrides.end())
		i = presetOverrides.find(prefixed_section);
	// Finally, don't add the prefix and try global:
	if (i == presetOverrides.end())
		i = presetOverrides.find(preset_id);
	if (i == presetOverrides.end())
		goto bail;

	operation->preset = &i->second;
	operation->exclude = exclude;

	return AddCommandToList(operation, explicit_command_list, pre_command_list, NULL, NULL, section, key, val);

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
		}
		else {
			operation->type = DrawCommandType::DRAW;
			nargs = swscanf_s(val->c_str(), L"%u, %u%n", &operation->args[0], &operation->args[1], &end);
			if (nargs != 2)
				goto bail;


		}
	}
	else if (!wcscmp(key, L"drawprimitive")) {
		operation->type = DrawCommandType::DRAWPRIMITIVE;
		nargs = swscanf_s(val->c_str(), L"%u, %u, %u%n", (DWORD)&operation->args[0], &operation->args[1], &operation->args[2], &end);
		if (nargs != 3)
			goto bail;


	}
	else if (!wcscmp(key, L"drawindexed")) {
		operation->type = DrawCommandType::DRAWINDEXED;
		nargs = swscanf_s(val->c_str(), L"%u, %u, %i%n", &operation->args[0], &operation->args[1], (INT*)&operation->args[2], &end);
		if (nargs != 3)
			goto bail;


	}
	else if (!wcscmp(key, L"drawindexedprimitive")) {
		operation->type = DrawCommandType::DRAWINDEXEDPRIMITIVE;
		nargs = swscanf_s(val->c_str(), L"%u, %i, %u, %u, %u, %u%n", (DWORD)&operation->args[0], (INT*)&operation->args[1], &operation->args[2], &operation->args[3], &operation->args[4], &operation->args[5], &end);
		if (nargs != 6)
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
	return AddCommandToList(operation, explicit_command_list, pre_command_list, NULL, NULL, section, key, val);

bail:
	delete operation;
	return false;
}
static bool ParseDirectModeSetActiveEyeCommand(const wchar_t *section,
	const wchar_t *key, wstring *val,
	CommandList *explicit_command_list,
	CommandList *pre_command_list,
	CommandList *post_command_list)
{
	DirectModeSetActiveEyeCommand *operation = new DirectModeSetActiveEyeCommand();

	if (!wcscmp(val->c_str(), L"mono")) {
		operation->eye = NVAPI_STEREO_EYE_MONO;
		goto success;
	}

	if (!wcscmp(val->c_str(), L"left")) {
		operation->eye = NVAPI_STEREO_EYE_LEFT;
		goto success;
	}

	if (!wcscmp(val->c_str(), L"right")) {
		operation->eye = NVAPI_STEREO_EYE_RIGHT;
		goto success;
	}

	goto bail;

success:
	return AddCommandToList(operation, explicit_command_list, pre_command_list, NULL, NULL, section, key, val);

bail:
	delete operation;
	return false;
}
static bool ParsePerDrawStereoOverride(const wchar_t *section,
	const wchar_t *key, wstring *val,
	CommandList *explicit_command_list,
	CommandList *pre_command_list,
	CommandList *post_command_list,
	bool is_separation,
	const wstring *ini_namespace)
{
	bool restore_on_post = !explicit_command_list && pre_command_list && post_command_list;
	PerDrawStereoOverrideCommand *operation = NULL;

	if (is_separation)
		operation = new PerDrawSeparationOverrideCommand(restore_on_post);
	else
		operation = new PerDrawConvergenceOverrideCommand(restore_on_post);

	// Try parsing value as a resource target for staging auto-convergence
	// Do this first, because the operand parsing would treat these as for
	// texture filtering
	if (operation->staging_op.src.ParseTarget(val->c_str(), true, ini_namespace)) {
		operation->staging_type = true;
		goto success;
	}

	// The scope is shared between pre & post, we use pre here since it is never NULL
	if (!operation->expression.parse(val, ini_namespace, pre_command_list))
		goto bail;

success:
	// Add to both command lists by default - the pre command list will set
	// the value, and the post command list will restore the original. If
	// an explicit command list is specified then the value will only be
	// set, not restored (regardless of whether that is pre or post)
	return AddCommandToList(operation, explicit_command_list, NULL, pre_command_list, post_command_list, section, key, val);

bail:
	delete operation;
	return false;
}
static bool ParseFrameAnalysisDump(const wchar_t *section,
	const wchar_t *key, wstring *val,
	CommandList *explicit_command_list,
	CommandList *pre_command_list,
	CommandList *post_command_list,
	const wstring *ini_namespace)
{
	FrameAnalysisDumpCommand *operation = new FrameAnalysisDumpCommand();
	wchar_t *buf;
	size_t size = val->size() + 1;
	wchar_t *target = NULL;

	// parse_enum_option_string replaces spaces with NULLs, so it can't
	// operate on the buffer in the wstring directly. I could potentially
	// change it to work without modifying the string, but for now it's
	// easier to just make a copy of the string:
	buf = new wchar_t[size];
	wcscpy_s(buf, size, val->c_str());

	operation->analyse_options = parse_enum_option_string<wchar_t *, FrameAnalysisOptions>
		(FrameAnalysisOptionNames, buf, &target);

	if (!target)
		goto bail;

	if (!operation->target.ParseTarget(target, true, ini_namespace))
		goto bail;

	operation->target_name = L"[" + wstring(section) + L"]-" + wstring(target);
	// target_name will be used in the filenames, so replace any reserved characters:
	std::replace(operation->target_name.begin(), operation->target_name.end(), L'<', L'_');
	std::replace(operation->target_name.begin(), operation->target_name.end(), L'>', L'_');
	std::replace(operation->target_name.begin(), operation->target_name.end(), L':', L'_');
	std::replace(operation->target_name.begin(), operation->target_name.end(), L'"', L'_');
	std::replace(operation->target_name.begin(), operation->target_name.end(), L'/', L'_');
	std::replace(operation->target_name.begin(), operation->target_name.end(), L'\\', L'_');
	std::replace(operation->target_name.begin(), operation->target_name.end(), L'|', L'_');
	std::replace(operation->target_name.begin(), operation->target_name.end(), L'?', L'_');
	std::replace(operation->target_name.begin(), operation->target_name.end(), L'*', L'_');

	delete[] buf;
	return AddCommandToList(operation, explicit_command_list, pre_command_list, NULL, NULL, section, key, val);

bail:
	delete[] buf;
	delete operation;
	return false;
}
static bool ParseShaderConstant(const wchar_t * target, wchar_t *shader_type, ConstantType *constant_type, int *idx, UINT *vector4Count)
{
	int ret, len;
	unsigned slot;
	unsigned const_num;
	wchar_t type;
	wchar_t shader;
	size_t length = wcslen(target);
	bool parsed = false;
	ret = swscanf_s(target, L"%lcs%lc_%u_%u%n", &shader, 1, &type, 1, &slot, &const_num, &len);
	if (ret == 4 && len == length) {
		parsed = true;
		*vector4Count = const_num;
	}
	else {
		ret = swscanf_s(target, L"%lcs%lc_%u%n", &shader, 1, &type, 1, &slot, &len);
		if (ret == 3 && len == length) {
			parsed = true;
			*vector4Count = 1;
		}
	}
	if (parsed) {
		*idx = slot;
		switch (shader) {
		case L'v': case L'p':
			*shader_type = shader;
			break;
		default:
			return false;
		}
		switch (type) {
		case L'f':
			*constant_type = ConstantType::FLOAT;
			break;
		case L'i':
			*constant_type = ConstantType::INT;
			break;
		case L'b':
			*constant_type = ConstantType::BOOL;
			break;
		default:
			return false;
		}
		return true;
	}
	return false;
}

static bool ParseFrameAnalysisDumpConstants(const wchar_t *section,
	const wchar_t *key, wstring *val,
	CommandList *explicit_command_list,
	CommandList *pre_command_list,
	CommandList *post_command_list,
	const wstring *ini_namespace)
{
	FrameAnalysisDumpConstantsCommand *operation = new FrameAnalysisDumpConstantsCommand();
	wchar_t shader_type;
	ConstantType const_type;
	int idx;

	if (!ParseShaderConstant(val->c_str(), &shader_type, &const_type, &idx, &operation->num_slots))
		goto bail;

	switch (const_type) {
	case ConstantType::FLOAT:
		operation->constant_type = 'f';
		break;
	case ConstantType::INT:
		operation->constant_type = 'i';
		break;
	case ConstantType::BOOL:
		operation->constant_type = 'b';
		break;
	default:
		goto bail;
	}
	operation->shader_type = (char)shader_type;
	operation->start_slot = idx;

	return AddCommandToList(operation, explicit_command_list, pre_command_list, NULL, NULL, section, key, val);

bail:
	delete operation;
	return false;
}
bool ParseCommandListGeneralCommands(const wchar_t *section,
	const wchar_t *key, wstring *val,
	CommandList *explicit_command_list,
	CommandList *pre_command_list, CommandList *post_command_list,
	const wstring *ini_namespace)
{
	if (!wcscmp(key, L"checktextureoverride"))
		return ParseCheckTextureOverride(section, key, val, explicit_command_list, pre_command_list, post_command_list, ini_namespace);

	if (!wcscmp(key, L"run")) {
		if (!wcsncmp(val->c_str(), L"customshader", 12) || !wcsncmp(val->c_str(), L"builtincustomshader", 19))
			return ParseRunShader(section, key, val, explicit_command_list, pre_command_list, post_command_list, ini_namespace);

		if (!wcsncmp(val->c_str(), L"commandlist", 11) || !wcsncmp(val->c_str(), L"builtincommandlist", 18))
			return ParseRunExplicitCommandList(section, key, val, explicit_command_list, pre_command_list, post_command_list, ini_namespace);

		if (!wcsncmp(val->c_str(), L"function", 8) || !wcsncmp(val->c_str(), L"builtinfunction", 15))
			return ParseRunFunction(section, key, val, explicit_command_list, pre_command_list, post_command_list, ini_namespace);
	}

	if (!wcscmp(key, L"preset"))
		return ParsePreset(section, key, val, explicit_command_list, pre_command_list, post_command_list, false, ini_namespace);
	if (!wcscmp(key, L"exclude_preset"))
		return ParsePreset(section, key, val, explicit_command_list, pre_command_list, post_command_list, true, ini_namespace);

	if (!wcscmp(key, L"handling")) {
		// skip only makes sense in pre command lists, since it needs
		// to run before the original draw call:
		if (!wcscmp(val->c_str(), L"skip"))
			return AddCommandToList(new SkipCommand(section), explicit_command_list, pre_command_list, NULL, NULL, section, key, val);

		// abort defaults to both command lists, to abort command list
		// execution both before and after the draw call:
		if (!wcscmp(val->c_str(), L"abort"))
			return AddCommandToList(new AbortCommand(section), explicit_command_list, NULL, pre_command_list, post_command_list, section, key, val);
	}

	if (!wcscmp(key, L"reset_per_frame_limits"))
		return ParseResetPerFrameLimits(section, key, val, explicit_command_list, pre_command_list, post_command_list, ini_namespace);

	if (!wcscmp(key, L"clear"))
		return ParseClearSurface(section, key, val, explicit_command_list, pre_command_list, post_command_list, ini_namespace);

	if (!wcscmp(key, L"separation"))
		return ParsePerDrawStereoOverride(section, key, val, explicit_command_list, pre_command_list, post_command_list, true, ini_namespace);

	if (!wcscmp(key, L"convergence"))
		return ParsePerDrawStereoOverride(section, key, val, explicit_command_list, pre_command_list, post_command_list, false, ini_namespace);

	if (!wcscmp(key, L"direct_mode_eye"))
		return ParseDirectModeSetActiveEyeCommand(section, key, val, explicit_command_list, pre_command_list, post_command_list);

	if (!wcscmp(key, L"analyse_options"))
		return AddCommandToList(new FrameAnalysisChangeOptionsCommand(val), explicit_command_list, pre_command_list, NULL, NULL, section, key, val);

	if (!wcscmp(key, L"dump"))
		return ParseFrameAnalysisDumpConstants(section, key, val, explicit_command_list, pre_command_list, post_command_list, ini_namespace);

	if (!wcscmp(key, L"dump"))
		return ParseFrameAnalysisDump(section, key, val, explicit_command_list, pre_command_list, post_command_list, ini_namespace);

	if (!wcscmp(key, L"special")) {
		if (!wcscmp(val->c_str(), L"upscaling_switch_bb"))
			return AddCommandToList(new UpscalingFlipBBCommand(section), explicit_command_list, pre_command_list, NULL, NULL, section, key, val);

		if (!wcscmp(val->c_str(), L"draw_3dmigoto_overlay"))
			return AddCommandToList(new Draw3DMigotoOverlayCommand(section), explicit_command_list, pre_command_list, NULL, NULL, section, key, val);
	}

	return ParseDrawCommand(section, key, val, explicit_command_list, pre_command_list, post_command_list);
}

void CheckTextureOverrideCommand::run(CommandListState *state)
{
	TextureOverrideMatches matches;
	ResourceCopyTarget *saved_this = NULL;
	bool saved_post;
	unsigned i;

	COMMAND_LIST_LOG(state, "%S\n", ini_line.c_str());

	target.FindTextureOverrides(state, NULL, &matches);

	saved_this = state->this_target;
	state->this_target = &target;
	if (run_pre_and_post_together) {
		saved_post = state->post;
		state->post = false;
		for (i = 0; i < matches.size(); i++)
			_RunCommandList(&matches[i]->command_list, state);
		state->post = true;
		for (i = 0; i < matches.size(); i++)
			_RunCommandList(&matches[i]->post_command_list, state);
		state->post = saved_post;
	}
	else {
		for (i = 0; i < matches.size(); i++) {
			if (state->post)
				_RunCommandList(&matches[i]->post_command_list, state);
			else
				_RunCommandList(&matches[i]->command_list, state);
		}
	}
	state->this_target = saved_this;
}
bool CheckTextureOverrideCommand::noop(bool post, bool ignore_cto_pre, bool ignore_cto_post)
{
	if (run_pre_and_post_together)
		return (ignore_cto_pre && ignore_cto_post);

	if (post)
		return ignore_cto_post;
	return ignore_cto_pre;
}
ClearSurfaceCommand::ClearSurfaceCommand() :
	dsv_depth(0.0),
	dsv_stencil(0),
	clear_depth(false),
	clear_stencil(false)
{
	memset(fval, 0, sizeof(fval));
	memset(uval, 0, sizeof(uval));
}
void ResetPerFrameLimitsCommand::run(CommandListState *state)
{
	COMMAND_LIST_LOG(state, "%S\n", ini_line.c_str());

	if (shader)
		shader->executions_this_frame = 0;

	if (resource)
		resource->copies_this_frame = 0;
}
void PresetCommand::run(CommandListState *state)
{
	COMMAND_LIST_LOG(state, "%S\n", ini_line.c_str());

	if (exclude)
		preset->Exclude();
	else
		preset->Trigger(this);
}

static UINT get_index_count_from_current_ib(D3D9Wrapper::IDirect3DDevice9 *mHackerDevice)
{
	::IDirect3DIndexBuffer9 *ib;
	::D3DINDEXBUFFER_DESC desc;

	mHackerDevice->GetD3D9Device()->GetIndices(&ib);
	if (!ib)
		return 0;
	ib->GetDesc(&desc);
	ib->Release();
	switch (desc.Format) {
	case ::D3DFORMAT::D3DFMT_INDEX16:
		return desc.Size / 2;
	case ::D3DFORMAT::D3DFMT_INDEX32:
		return desc.Size / 4;
	}

	return 0;
}

struct CUSTOMVERTEX_DRAWCOMMAND
{
	float vertexID;
};
HRESULT DrawCommandVertexBufferPrepare(CommandListState *state, UINT vertexCount) {
	HRESULT hr;
	bool recreateVertexBuffer = false;
	if (state->mHackerDevice->mClDrawVertexBuffer == NULL) {
		recreateVertexBuffer = true;
	}
	else {
		D3DVERTEXBUFFER_DESC vDesc;// = NULL;
		state->mHackerDevice->mClDrawVertexBuffer->GetDesc(&vDesc);
		if (vDesc.Size < vertexCount*(sizeof(CUSTOMVERTEX_DRAWCOMMAND))) {
			recreateVertexBuffer = true;
		}
	}

	if (recreateVertexBuffer) {
		if (state->mHackerDevice->mClDrawVertexBuffer != NULL) {
			--state->mHackerDevice->migotoResourceCount;
			state->mHackerDevice->mClDrawVertexBuffer->Release();
			state->mHackerDevice->mClDrawVertexBuffer = NULL;
		}
		hr = state->mOrigDevice->CreateVertexBuffer(
			vertexCount*(sizeof(CUSTOMVERTEX_DRAWCOMMAND)),
			0,
			0,
			D3DPOOL_DEFAULT,
			&state->mHackerDevice->mClDrawVertexBuffer,
			NULL);
		if (FAILED(hr))
			return hr;
		else
			++state->mHackerDevice->migotoResourceCount;

		vector<CUSTOMVERTEX_DRAWCOMMAND> DrawVertices(vertexCount);
		float i = 0;
		do {
			DrawVertices[(UINT)i] = { i };
			i += 1.0f;
		} while (i < vertexCount);
		VOID* pVoid;
		hr = state->mHackerDevice->mClDrawVertexBuffer->Lock(0, 0, (void**)&pVoid, 0);
		if (FAILED(hr))
			return hr;
		std::memcpy(pVoid, &DrawVertices[0], sizeof(CUSTOMVERTEX_DRAWCOMMAND) * vertexCount);
		hr = state->mHackerDevice->mClDrawVertexBuffer->Unlock();
		if (FAILED(hr))
			return hr;
	}

	if (state->mHackerDevice->mClDrawVertexDecl == NULL) {
		D3DVERTEXELEMENT9 index;
		index.Usage = D3DDECLUSAGE_TEXCOORD;
		index.UsageIndex = 0;
		index.Stream = 0;
		index.Type = D3DDECLTYPE_FLOAT1;
		index.Offset = 0;
		index.Method = D3DDECLMETHOD_DEFAULT;
		D3DVERTEXELEMENT9 vertexElements[] =
		{
			index,
			D3DDECL_END()
		};
		hr = state->mOrigDevice->CreateVertexDeclaration(vertexElements, &state->mHackerDevice->mClDrawVertexDecl);
		if (FAILED(hr))
			return hr;
		else
			++state->mHackerDevice->migotoResourceCount;
	}
	hr = state->mOrigDevice->SetVertexDeclaration(state->mHackerDevice->mClDrawVertexDecl);
	if (FAILED(hr))
		return hr;
	hr = state->mOrigDevice->SetStreamSourceFreq(0, 1);
	if (FAILED(hr))
		return hr;
	hr = state->mOrigDevice->SetStreamSource(0, state->mHackerDevice->mClDrawVertexBuffer, 0,  sizeof(CUSTOMVERTEX_DRAWCOMMAND));
	if (FAILED(hr))
		return hr;
	return D3D_OK;
}
inline bool SetDrawingSide(CommandListState *state, D3D9Wrapper::RenderPosition side)
{
	LogDebug("Command List Direct Mode, SetDrawingSide");
	// Already on the correct eye
	if (side == state->mHackerDevice->currentRenderingSide) {
		return true;
	}
	// should never try and render for the right eye if there is no render target for the main render targets right side
	if (!state->mHackerDevice->m_activeRenderTargets[0]->IsDirectStereoSurface() && (side == D3D9Wrapper::RenderPosition::Right)) {
		return false;
	}
	// Everything hasn't changed yet but we set this first so we don't accidentally use the member instead of the local and break
	// things, as I have already managed twice.
	state->mHackerDevice->currentRenderingSide = side;
	// switch render targets to new side
	bool renderTargetChanged = false;
	HRESULT result;
	D3D9Wrapper::IDirect3DSurface9* pCurrentRT;
	for (std::vector<D3D9Wrapper::IDirect3DSurface9*>::size_type i = 0; i != state->mHackerDevice->m_activeRenderTargets.size(); i++)
	{
		if ((pCurrentRT = state->mHackerDevice->m_activeRenderTargets[i]) != NULL) {

			if (side == D3D9Wrapper::RenderPosition::Left)
				result = state->mOrigDevice->SetRenderTarget((DWORD)i, pCurrentRT->DirectModeGetLeft());
			else
				result = state->mOrigDevice->SetRenderTarget((DWORD)i, pCurrentRT->DirectModeGetRight());

			if (result != D3D_OK) {
				LogDebug("Command List Direct Mode, - Error trying to set one of the Render Targets while switching between active eyes for drawing.\n");
			}
			else {
				renderTargetChanged = true;
			}
		}
	}
	// switch depth stencil to new side
	if (state->mHackerDevice->m_pActiveDepthStencil != NULL) {
		if (side == D3D9Wrapper::RenderPosition::Left)
			result = state->mOrigDevice->SetDepthStencilSurface(state->mHackerDevice->m_pActiveDepthStencil->DirectModeGetLeft());
		else
			result = state->mOrigDevice->SetDepthStencilSurface(state->mHackerDevice->m_pActiveDepthStencil->DirectModeGetRight());
	}
	// switch textures to new side
	::IDirect3DBaseTexture9* pActualLeftTexture = NULL;
	::IDirect3DBaseTexture9* pActualRightTexture = NULL;

	for (auto it = state->m_activeStereoTextureStages.begin(); it != state->m_activeStereoTextureStages.end(); ++it)
	{
		pActualLeftTexture = NULL;
		pActualRightTexture = NULL;
		state->mHackerDevice->HackerDeviceUnWrapTexture(it->second, &pActualLeftTexture, &pActualRightTexture);
		if (side == D3D9Wrapper::RenderPosition::Left)
			result = state->mOrigDevice->SetTexture(it->first, pActualLeftTexture);
		else
			result = state->mOrigDevice->SetTexture(it->first, pActualRightTexture);

		if (result != D3D_OK)
			LogDebug("Command List Direct Mode, Error trying to set one of the textures while switching between active eyes for drawing.\n");
	}
	if (state->mHackerDevice->DirectModeGameProjectionIsSet) {
		if (side == D3D9Wrapper::RenderPosition::Left)
			state->mOrigDevice->SetTransform(::D3DTS_PROJECTION, &state->mHackerDevice->m_leftProjection);
		else
			state->mOrigDevice->SetTransform(::D3DTS_PROJECTION, &state->mHackerDevice->m_rightProjection);
	}
	return true;
}
inline bool SwitchDrawingSide(CommandListState *state)
{
	bool switched = false;
	if (state->mHackerDevice->currentRenderingSide == D3D9Wrapper::RenderPosition::Left) {
		switched = SetDrawingSide(state, D3D9Wrapper::RenderPosition::Right);
	}
	else if (state->mHackerDevice->currentRenderingSide == D3D9Wrapper::RenderPosition::Right) {
		switched = SetDrawingSide(state, D3D9Wrapper::RenderPosition::Left);
	}
	return switched;
}

void DrawCommand::draw(CommandListState* state, DrawCommandType type) {

	IDirect3DDevice9 *mOrigDevice = state->mOrigDevice;
	DrawCallInfo *info = state->call_info;

	HRESULT hr;
	IDirect3DVertexBuffer9 *saved_vertex_buffer;
	UINT saved_stream_source_freq_divider;
	UINT saved_stream_source_offset;
	UINT saved_stream_source_stride;
	IDirect3DVertexDeclaration9 *saved_vertex_declaration;
	mOrigDevice->GetVertexDeclaration(&saved_vertex_declaration);
	mOrigDevice->GetStreamSource(0, &saved_vertex_buffer, &saved_stream_source_offset, &saved_stream_source_stride);
	mOrigDevice->GetStreamSourceFreq(0, &saved_stream_source_freq_divider);
	D3DVERTEXBUFFER_DESC pDesc;
	UINT primitiveCount;
	UINT NumVertices;
	switch (type) {
	case DrawCommandType::DRAW:
		COMMAND_LIST_LOG(state, "[%S] Draw(%u,%u)\n", ini_section.c_str(), args[0], args[1]);
		if (info->primitive_type == D3DPRIMITIVETYPE(-1)) {
			COMMAND_LIST_LOG(state, "  Draw failure: Unknown topology\n");
			break;
		}
		hr = DrawCommandVertexBufferPrepare(state, args[0]);
		if (FAILED(hr))
			COMMAND_LIST_LOG(state, "  Draw vertex buffer prepare failure: %d\n", hr);

		primitiveCount = DrawVerticesCountToPrimitiveCount(args[0], info->primitive_type);
		hr = mOrigDevice->DrawPrimitive(info->primitive_type, args[1], primitiveCount);
		if (FAILED(hr)) {
			COMMAND_LIST_LOG(state, "  Draw failure: %d\n", hr);
		}
		else if (G->gForceStereo == 2){
			if (SwitchDrawingSide(state)) {
				hr = mOrigDevice->DrawPrimitive(info->primitive_type, args[1], primitiveCount);
				if (FAILED(hr))
					COMMAND_LIST_LOG(state, "  Direct Mode Draw failure after side switch: %d\n", hr);
			}
		}
		break;
	case DrawCommandType::DRAWPRIMITIVE:
		COMMAND_LIST_LOG(state, "[%S] DrawPrimitive(%u,%u,%i)\n", ini_section.c_str(), (DWORD)args[0], args[1], args[2]);
		hr = DrawCommandVertexBufferPrepare(state, DrawPrimitiveCountToVerticesCount(args[2], info->primitive_type));
		if (FAILED(hr))
			COMMAND_LIST_LOG(state, "  Draw vertex buffer prepare failure: %d\n", hr);
		hr = mOrigDevice->DrawPrimitive(D3DPRIMITIVETYPE(args[0]), args[1], args[2]);
		if (FAILED(hr)) {
			COMMAND_LIST_LOG(state, "  Draw Primitive failure: %d\n", hr);
		}
		else if (G->gForceStereo == 2) {
			if (SwitchDrawingSide(state)) {
				hr = mOrigDevice->DrawPrimitive(D3DPRIMITIVETYPE(args[0]), args[1], args[2]);
				if (FAILED(hr))
					COMMAND_LIST_LOG(state, "  Direct Mode Draw Primitive failure after side switch: %d\n", hr);
			}
		}
		break;
	case DrawCommandType::DRAWINDEXED:
		COMMAND_LIST_LOG(state, "[%S] Draw(%u,%u)\n", ini_section.c_str(), args[0], args[1]);
		if (info->primitive_type == D3DPRIMITIVETYPE(-1)) {
			COMMAND_LIST_LOG(state, "  Draw failure: Unknown topology\n");
			break;
		}
		hr = DrawCommandVertexBufferPrepare(state, args[0]);
		if (FAILED(hr))
			COMMAND_LIST_LOG(state, "  Draw vertex buffer prepare failure: %d\n", hr);
		state->mHackerDevice->mClDrawVertexBuffer->GetDesc(&pDesc);
		NumVertices = (pDesc.Size / sizeof(CUSTOMVERTEX_DRAWCOMMAND));
		primitiveCount = DrawVerticesCountToPrimitiveCount(args[0], info->primitive_type);
		hr = mOrigDevice->DrawIndexedPrimitive(info->primitive_type, (INT)args[2], 0, NumVertices, args[1], primitiveCount);
		if (FAILED(hr)) {
			COMMAND_LIST_LOG(state, "  Draw Indexed failure: %d\n", hr);
		}
		else if (G->gForceStereo == 2) {
			if (SwitchDrawingSide(state)) {
				hr = mOrigDevice->DrawIndexedPrimitive(info->primitive_type, (INT)args[2], 0, NumVertices, args[1], primitiveCount);
				if (FAILED(hr))
					COMMAND_LIST_LOG(state, "  Direct Mode Draw Indexed failure after side switch: %d\n", hr);
			}
		}
		break;
	case DrawCommandType::DRAWINDEXEDPRIMITIVE:
		COMMAND_LIST_LOG(state, "[%S] DrawIndexed(%u, %i, %u, %u, %u, %u)\n", ini_section.c_str(), args[0], (INT)args[1], args[2], args[3], args[4], args[5]);
		hr = DrawCommandVertexBufferPrepare(state, DrawPrimitiveCountToVerticesCount(args[5], info->primitive_type));
		if (FAILED(hr))
			COMMAND_LIST_LOG(state, "  Draw vertex buffer prepare failure: %d\n", hr);
		hr = mOrigDevice->DrawIndexedPrimitive(D3DPRIMITIVETYPE(args[0]), (INT)args[1], args[2], args[3], args[4], args[5]);
		if (FAILED(hr)) {
			COMMAND_LIST_LOG(state, "  Draw Indexed Primitive failure: %d\n", hr);
		}
		else if (G->gForceStereo == 2) {
			if (SwitchDrawingSide(state)) {
				hr = mOrigDevice->DrawIndexedPrimitive(D3DPRIMITIVETYPE(args[0]), (INT)args[1], args[2], args[3], args[4], args[5]);
				if (FAILED(hr))
					COMMAND_LIST_LOG(state, "  Direct Mode Draw Indexed Primitive failure after side switch: %d\n", hr);
			}
		}
		break;
	}

	mOrigDevice->SetVertexDeclaration(saved_vertex_declaration);
	mOrigDevice->SetStreamSourceFreq(0, saved_stream_source_freq_divider);
	mOrigDevice->SetStreamSource(0, saved_vertex_buffer, saved_stream_source_offset, saved_stream_source_stride);

	if(saved_vertex_declaration)
		saved_vertex_declaration->Release();

	if (saved_vertex_buffer)
		saved_vertex_buffer->Release();

}

void DrawCommand::run(CommandListState *state)
{
	D3D9Wrapper::IDirect3DDevice9 *mHackerDevice = state->mHackerDevice;
	IDirect3DDevice9 *mOrigDevice = state->mOrigDevice;
	DrawCallInfo *info = state->call_info;
	UINT auto_count = 0;

	// If this command list was triggered from something currently skipped
	// due to hunting, we also skip any custom draw calls, so that if we
	// are replacing the original draw call we will still be able to see
	// the object being hunted.
	if (info && info->hunting_skip) {
		COMMAND_LIST_LOG(state, "[%S] Draw -> SKIPPED DUE TO HUNTING\n", ini_section.c_str());
		return;
	}

	// Ensure IniParams are visible:
	CommandListFlushState(state);

	Profiling::injected_draw_calls++;
	switch (type) {
	case DrawCommandType::DRAW:
		draw(state, DrawCommandType::DRAW);
		break;
	case DrawCommandType::DRAWPRIMITIVE:
		draw(state, DrawCommandType::DRAWPRIMITIVE);
		break;
	case DrawCommandType::DRAWINDEXED:
		draw(state, DrawCommandType::DRAWINDEXED);
		break;
	case DrawCommandType::DRAWINDEXEDPRIMITIVE:
		draw(state, DrawCommandType::DRAWINDEXEDPRIMITIVE);
		break;
	case DrawCommandType::FROM_CALLER:
		if (!info) {
			COMMAND_LIST_LOG(state, "[%S] Draw = from_caller -> NO ACTIVE DRAW CALL\n", ini_section.c_str());
			break;
		}
		switch (info->type) {
		case DrawCall::DrawIndexedUP:
			COMMAND_LIST_LOG(state, "[%S] Draw = from_caller -> DrawIndexedPrimitiveUP (%u, %u, %u, %u, %u, %p, %u, %p, %u)\n", ini_section.c_str(), info->primitive_type, info->MinVertexIndex, info->NumVertices, info->PrimitiveCount, info->pIndexData, info->IndexDataFormat, info->pVertexStreamZeroData, info->VertexStreamZeroStride);
			mOrigDevice->DrawIndexedPrimitiveUP(info->primitive_type, info->MinVertexIndex, info->NumVertices, info->PrimitiveCount, info->pIndexData, info->IndexDataFormat, info->pVertexStreamZeroData, info->VertexStreamZeroStride);
			break;
		case DrawCall::DrawUP:
			COMMAND_LIST_LOG(state, "[%S] Draw = from_caller -> DrawPrimitiveUP (%u, %u, %u, %p, %u)\n", ini_section.c_str(), info->primitive_type, info->PrimitiveCount, info->pVertexStreamZeroData, info->VertexStreamZeroStride);
			mOrigDevice->DrawPrimitiveUP(info->primitive_type, info->PrimitiveCount, info->pVertexStreamZeroData, info->VertexStreamZeroStride);
			break;
		case DrawCall::DrawIndexed:
			COMMAND_LIST_LOG(state, "[%S] Draw = from_caller -> DrawIndexedPrimitive (%u, %u, %u, %u, %u, %u, %u)\n", ini_section.c_str(), info->primitive_type, info->StartVertex, info->MinVertexIndex, info->NumVertices, info->StartIndex, info->PrimitiveCount);
			mOrigDevice->DrawIndexedPrimitive(info->primitive_type, (INT)info->StartVertex, info->MinVertexIndex, info->NumVertices, info->StartIndex, info->PrimitiveCount);
			break;
		case DrawCall::Draw:
			COMMAND_LIST_LOG(state, "[%S] Draw = from_caller -> DrawPrimitive  (%u, %u, %u, %u)\n", ini_section.c_str(), info->primitive_type, info->StartVertex, info->PrimitiveCount);
			mOrigDevice->DrawPrimitive(info->primitive_type, info->StartVertex, info->PrimitiveCount);
			break;
		case DrawCall::DrawTriPatch:
			COMMAND_LIST_LOG(state, "[%S] Draw = from_caller -> DrawTriPatch(%u, %u, %f, %p)\n", ini_section.c_str(), info->Handle, info->pNumSegs, info->pTriPatchInfo);
			mOrigDevice->DrawTriPatch(info->Handle, info->pNumSegs, info->pTriPatchInfo);
			break;
		case DrawCall::DrawRectPatch:
			COMMAND_LIST_LOG(state, "[%S] Draw = from_caller -> DrawRectPatch (%u, %u, %f, %p)\n", ini_section.c_str(), info->Handle, info->pNumSegs, info->pRectPatchInfo);
			mOrigDevice->DrawRectPatch(info->Handle, info->pNumSegs, info->pRectPatchInfo);
			break;
		default:
			LogOverlay(LOG_DIRE, "BUG: draw = from_caller -> unknown draw call type\n");
			break;
		}
		break;
	case DrawCommandType::AUTO_INDEX_COUNT:
		auto_count = get_index_count_from_current_ib(mHackerDevice);
		COMMAND_LIST_LOG(state, "[%S] drawindexed = auto -> DrawIndexed(%u, 0, 0)\n", ini_section.c_str(), auto_count);
		if (auto_count)
			mHackerDevice->DrawIndexedPrimitive(info->primitive_type, 0, 0, auto_count, 0, DrawVerticesCountToPrimitiveCount(auto_count, info->primitive_type));
		else
			COMMAND_LIST_LOG(state, "  Unable to determine index count\n");
		break;
	}
}

void SkipCommand::run(CommandListState *state)
{
	COMMAND_LIST_LOG(state, "[%S] handling = skip\n", ini_section.c_str());

	if (state->call_info)
		state->call_info->skip = true;
	else
		COMMAND_LIST_LOG(state, "  No active draw call to skip\n");
}

void AbortCommand::run(CommandListState *state)
{
	COMMAND_LIST_LOG(state, "[%S] handling = abort\n", ini_section.c_str());

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
	void *mapping = NULL;
	HRESULT hr;
	float tmp;
	bool ret = false;

	if (!staging_type)
		return true;
	if (staging_op.staging) {
		hr = staging_op.map(state, &mapping);
		if (FAILED(hr)) {
			if (hr == D3DERR_WASSTILLDRAWING)
				COMMAND_LIST_LOG(state, "  Transfer in progress...\n");
			else
				COMMAND_LIST_LOG(state, "  Unknown error: 0x%x\n", hr);
			return false;
		}

		// FIXME: Check if resource is at least 4 bytes (maybe we can
		// use RowPitch, but MSDN contradicts itself so I'm not sure.
		// Otherwise we can refer to the resource description)
		//tmp = ((float*)mapping.pData)[0];
		tmp = ((float*)mapping)[0];
		staging_op.unmap(state);

		if (isnan(tmp)) {
			COMMAND_LIST_LOG(state, "  Disregarding NAN\n");
		}
		else {
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
	COMMAND_LIST_LOG(state, "%S\n", ini_line.c_str());

	if (!state->mHackerDevice->mStereoHandle) {
		COMMAND_LIST_LOG(state, "  No Stereo Handle\n");
		return;
	}

	if (restore_on_post) {
		if (state->post) {
			if (!did_set_value_on_pre)
				return;
			did_set_value_on_pre = false;

			COMMAND_LIST_LOG(state, "  Restoring %s = %f\n", stereo_param_name(), saved);
			set_stereo_value(state, saved);
		}
		else {
			if (staging_type) {
				if (!(did_set_value_on_pre = update_val(state)))
					return;
			}
			else {
				val = expression.evaluate(state);
				did_set_value_on_pre = true;
			}

			saved = get_stereo_value(state);

			COMMAND_LIST_LOG(state, "  Setting per-draw call %s = %f * %f = %f\n",
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
	}
	else {
		if (staging_type) {
			if (!update_val(state))
				return;
		}
		else
			val = expression.evaluate(state);

		COMMAND_LIST_LOG(state, "  Setting %s = %f\n", stereo_param_name(), val);
		set_stereo_value(state, val);
	}
}

bool PerDrawStereoOverrideCommand::optimise(D3D9Wrapper::IDirect3DDevice9 *device)
{
	if (staging_type)
		return false;
	return expression.optimise(device);
}

bool PerDrawStereoOverrideCommand::noop(bool post, bool ignore_cto_pre, bool ignore_cto_post)
{
	NvU8 enabled = false;

	NvAPIOverride();
	Profiling::NvAPI_Stereo_IsEnabled(&enabled);
	return !enabled;
}

void DirectModeSetActiveEyeCommand::run(CommandListState *state)
{
	COMMAND_LIST_LOG(state, "%S\n", ini_line.c_str());

	if (NVAPI_OK != Profiling::NvAPI_Stereo_SetActiveEye(state->mHackerDevice->mStereoHandle, eye))
		COMMAND_LIST_LOG(state, "  Stereo_SetActiveEye failed\n");
}

bool DirectModeSetActiveEyeCommand::noop(bool post, bool ignore_cto_pre, bool ignore_cto_post)
{
	NvU8 enabled = false;

	NvAPIOverride();
	Profiling::NvAPI_Stereo_IsEnabled(&enabled);
	return !enabled;

	// FIXME: Should also return false if direct mode is disabled...
	// if only nvapi provided a GetDriverMode() API to determine that
}

float PerDrawSeparationOverrideCommand::get_stereo_value(CommandListState *state)
{
	float ret = 0.0f;

	if (NVAPI_OK != GetSeparation(state->mHackerDevice, state->cachedStereoValues, &ret))//		state->mHackerDevice->GetSeparation(&ret))//Profiling::NvAPI_Stereo_GetSeparation(state->mHackerDevice->mStereoHandle, &ret))
		COMMAND_LIST_LOG(state, "  Stereo_GetSeparation failed\n");

	return ret;
}

void PerDrawSeparationOverrideCommand::set_stereo_value(CommandListState *state, float val)
{
	NvAPIOverride();
	if (NVAPI_OK != SetSeparation(state->mHackerDevice, state->cachedStereoValues, val))//state->mHackerDevice->SetSeparation(val))//Profiling::NvAPI_Stereo_SetSeparation(state->mHackerDevice->mStereoHandle, val))
		COMMAND_LIST_LOG(state, "  Stereo_SetSeparation failed\n");
}

float PerDrawConvergenceOverrideCommand::get_stereo_value(CommandListState *state)
{
	float ret = 0.0f;

	if (NVAPI_OK != GetConvergence(state->mHackerDevice, state->cachedStereoValues, &ret))//state->mHackerDevice->GetConvergence(&ret))//Profiling::NvAPI_Stereo_GetConvergence(state->mHackerDevice->mStereoHandle, &ret))
		COMMAND_LIST_LOG(state, "  Stereo_GetConvergence failed\n");

	return ret;
}

void PerDrawConvergenceOverrideCommand::set_stereo_value(CommandListState *state, float val)
{
	NvAPIOverride();
	if (NVAPI_OK != SetConvergence(state->mHackerDevice, state->cachedStereoValues, val))//state->mHackerDevice->SetConvergence(val))//Profiling::NvAPI_Stereo_SetConvergence(state->mHackerDevice->mStereoHandle, val))
		COMMAND_LIST_LOG(state, "  Stereo_SetConvergence failed\n");
}

FrameAnalysisChangeOptionsCommand::FrameAnalysisChangeOptionsCommand(wstring *val)
{
	wchar_t *buf;
	size_t size = val->size() + 1;

	// parse_enum_option_string replaces spaces with NULLs, so it can't
	// operate on the buffer in the wstring directly. I could potentially
	// change it to work without modifying the string, but for now it's
	// easier to just make a copy of the string:
	buf = new wchar_t[size];
	wcscpy_s(buf, size, val->c_str());

	analyse_options = parse_enum_option_string<wchar_t *, FrameAnalysisOptions>
		(FrameAnalysisOptionNames, buf, NULL);

	delete[] buf;
}

void FrameAnalysisChangeOptionsCommand::run(CommandListState *state)
{
	COMMAND_LIST_LOG(state, "%S\n", ini_line.c_str());

	state->mHackerDevice->FrameAnalysisTrigger(analyse_options);
}

bool FrameAnalysisChangeOptionsCommand::noop(bool post, bool ignore_cto_pre, bool ignore_cto_post)
{
	return (G->hunting == HUNTING_MODE_DISABLED || G->frame_analysis_registered == false);
}
HRESULT strideForVertexDeclaration(IDirect3DVertexDeclaration9 *vd, UINT *totalBytes) {

	D3DVERTEXELEMENT9 decl[MAXD3DDECLLENGTH];
	UINT numElements;
	HRESULT hr = vd->GetDeclaration(decl, &numElements);
	if (FAILED(hr))
		return hr;
	*totalBytes = 0;
	for (UINT x = 0; x < (numElements - 1); x++) {
		D3DVERTEXELEMENT9 element = decl[x];
		*totalBytes += byteSizeFromD3DType((D3DDECLTYPE)element.Type);
	}

	return hr;
}
static void FillInMissingInfo(CommandListState *state, IDirect3DResource9 *resource,// IDirect3DSurface9 *surface, //ID3D11View *view,
	UINT *stride, UINT *offset, UINT *buf_size, D3DFORMAT *format)
{
	D3DRESOURCETYPE res_type;
	D3DVERTEXBUFFER_DESC vbuf_desc;
	D3DINDEXBUFFER_DESC ibuf_desc;
	IDirect3DVertexBuffer9 *vbuffer;
	IDirect3DIndexBuffer9 *ibuffer;
	IDirect3DSurface9 *sur;
	IDirect3DTexture9 *tex2d;
	IDirect3DVolumeTexture9 *tex3d;
	IDirect3DCubeTexture9 *texCube;
	D3DSURFACE_DESC sur_desc;
	D3DVOLUME_DESC vol_desc;
	res_type = resource->GetType();

	switch (res_type) {
	case D3DRTYPE_VERTEXBUFFER:
		vbuffer = (IDirect3DVertexBuffer9*)resource;
		vbuffer->GetDesc(&vbuf_desc);
		if (*buf_size)
			*buf_size = min(*buf_size, vbuf_desc.Size);
		else
			*buf_size = vbuf_desc.Size;
		if (!*stride) {
			if (vbuf_desc.FVF == 0) {
				IDirect3DVertexDeclaration9 *vd = NULL;
				state->mOrigDevice->GetVertexDeclaration(&vd);
				if (vd)
					strideForVertexDeclaration(vd, stride);
			}
			else {
				*stride = strideForFVF(vbuf_desc.FVF);
			}
		}
		break;
	case D3DRTYPE_INDEXBUFFER:
		ibuffer = (IDirect3DIndexBuffer9*)resource;
		ibuffer->GetDesc(&ibuf_desc);
		if (*buf_size)
			*buf_size = min(*buf_size, ibuf_desc.Size);
		else
			*buf_size = ibuf_desc.Size;
		if (*format == D3DFMT_UNKNOWN)
			*format = ibuf_desc.Format;
		if (!*stride)
			*stride = d3d_format_bytes(*format);
		break;
	case D3DRTYPE_SURFACE:
		sur = (IDirect3DSurface9*)resource;
		sur->GetDesc(&sur_desc);
		if (*format == D3DFMT_UNKNOWN)
			*format = sur_desc.Format;
		if (!*stride)
			*stride = d3d_format_bytes(*format);
		break;
	case D3DRTYPE_TEXTURE:
		tex2d = (IDirect3DTexture9*)resource;
		tex2d->GetLevelDesc(0, &sur_desc);
		if (*format == D3DFMT_UNKNOWN)
			*format = sur_desc.Format;
		if (!*stride)
			*stride = d3d_format_bytes(*format);
		break;
	case  D3DRTYPE_VOLUMETEXTURE:
		tex3d = (IDirect3DVolumeTexture9*)resource;
		tex3d->GetLevelDesc(0, &vol_desc);
		if (*format == D3DFMT_UNKNOWN)
			*format = vol_desc.Format;
		if (!*stride)
			*stride = d3d_format_bytes(*format);
		break;
	case D3DRTYPE_CUBETEXTURE:
		texCube = (IDirect3DCubeTexture9*)resource;
		texCube->GetLevelDesc(0, &sur_desc);
		if (*format == D3DFMT_UNKNOWN)
			*format = sur_desc.Format;
		if (!*stride)
			*stride = d3d_format_bytes(*format);
	}
}
void FrameAnalysisDumpCommand::run(CommandListState *state)
{
	IDirect3DResource9 *resource = NULL;
	UINT stride = 0;
	UINT offset = 0;
	UINT buf_size = 0;
	D3DFORMAT format = D3DFMT_UNKNOWN;
	ResourceHandleInfo *info = NULL;

	// Fast exit if frame analysis is currently inactive:
	if (!G->analyse_frame)
		return;

	COMMAND_LIST_LOG(state, "%S\n", ini_line.c_str());
	D3D9Wrapper::IDirect3DResource9 *wrapper = NULL;
	resource = target.GetResource(state, &stride, &offset, &format, &buf_size, NULL, &wrapper);
	if (!resource) {
		COMMAND_LIST_LOG(state, "  No resource to dump\n");
		return;
	}

	// Fill in any missing info before handing it to frame analysis. The
	// format is particularly important to try to avoid saving TYPELESS
	// resources:
	FillInMissingInfo(state, resource, &stride, &offset, &buf_size, &format);
	if (wrapper)
		info = &wrapper->resourceHandleInfo;
	state->mHackerDevice->FrameAnalysisDump(resource, analyse_options, target_name.c_str(), format, stride, offset, info);

	if (resource)
		resource->Release();
}

bool FrameAnalysisDumpCommand::noop(bool post, bool ignore_cto_pre, bool ignore_cto_post)
{
	return (G->hunting == HUNTING_MODE_DISABLED || G->frame_analysis_registered == false);
}

UpscalingFlipBBCommand::UpscalingFlipBBCommand(wstring section) :
	ini_section(section)
{
	G->upscaling_command_list_using_explicit_bb_flip = true;
}

UpscalingFlipBBCommand::~UpscalingFlipBBCommand()
{
	G->upscaling_command_list_using_explicit_bb_flip = false;
}

void UpscalingFlipBBCommand::run(CommandListState *state)
{
	COMMAND_LIST_LOG(state, "[%S] special = upscaling_switch_bb\n", ini_section.c_str());
	G->bb_is_upscaling_bb = false;
}

void Draw3DMigotoOverlayCommand::run(CommandListState *state)
{
	COMMAND_LIST_LOG(state, "[%S] special = draw_3dmigoto_overlay\n", ini_section.c_str());
	if (state->mHackerDevice->getOverlay()) {
		state->mHackerDevice->getOverlay()->DrawOverlay();
		G->suppress_overlay = true;
	}
}

CustomShader::CustomShader() :
	vs_override(false),
	ps_override(false),
	vs(NULL),
	ps(NULL),
	vs_bytecode(NULL),
	ps_bytecode(NULL),
	blend_override(0), blend_state(NULL),
	blend_sample_mask_merge_mask(0xffffffff),
	depth_stencil_override(0), depth_stencil_state(NULL),
	stencil_ref_mask(~0),
	rs_override(0), rs_state(NULL),
	alpha_test_override(0),
	alpha_test_state(NULL),
	primitive_type(D3DPRIMITIVETYPE(-1)),
	substantiated(false),
	max_executions_per_frame(0),
	frame_no(0),
	executions_this_frame(0),
	sampler_override(0),
	sampler_states(),
	mHackerDevice(NULL),
	enable_timer(false),
	run_interval(chrono::milliseconds(0)),
	last_time_run(chrono::high_resolution_clock::now()),
	compile_flags(D3DCompileFlags::OPTIMIZATION_LEVEL3)
{
	int i;

	for (i = 0; i < 4; i++) {
		blend_factor[i] = 1.0f;
		blend_factor_merge_mask[i] = ~0;
	}
}

CustomShader::~CustomShader()
{
	EnterCriticalSection(&G->mCriticalSection);
	if (vs) {
		vs->Release();
		vs = NULL;
		if (mHackerDevice)
			mHackerDevice->migotoResourceCount--;
	}
	if (ps) {
		ps->Release();
		ps = NULL;
		if (mHackerDevice)
			mHackerDevice->migotoResourceCount--;
	}
	LeaveCriticalSection(&G->mCriticalSection);
	if (blend_state)
		blend_state->~ID3D9BlendState();

	if (depth_stencil_state)
		depth_stencil_state->~ID3D9DepthStencilState();
	if (rs_state)
		rs_state->~ID3D9RasterizerState();

	if (vs_bytecode)
		vs_bytecode->Release();
	if (ps_bytecode)
		ps_bytecode->Release();
	map<UINT, ID3D9SamplerState*>::iterator it;
	for (it = sampler_states.begin(); it != sampler_states.end(); it++)
	{
		it->second->~ID3D9SamplerState();
	}
	sampler_states.clear();
}
static bool load_cached_shader(FILETIME hlsl_timestamp, wchar_t *cache_path, ID3DBlob **ppBytecode)
{
	FILETIME cache_timestamp;
	HANDLE f_cache;
	DWORD filesize, readsize;

	f_cache = CreateFile(cache_path, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (f_cache == INVALID_HANDLE_VALUE)
		return false;

	if (!GetFileTime(f_cache, NULL, NULL, &cache_timestamp)
		|| CompareFileTime(&hlsl_timestamp, &cache_timestamp)) {
		LogInfo("    Discarding stale cached shader: %S\n", cache_path);
		goto err_close;
	}

	filesize = GetFileSize(f_cache, 0);
	if (FAILED(D3DCreateBlob(filesize, ppBytecode))) {
		LogInfo("    D3DCreateBlob failed\n");
		goto err_close;
	}

	if (!ReadFile(f_cache, (*ppBytecode)->GetBufferPointer(), (DWORD)(*ppBytecode)->GetBufferSize(), &readsize, 0)
		|| readsize != filesize) {
		LogInfo("    Error reading cached shader\n");
		goto err_free;
	}

	LogInfo("    Loaded cached shader: %S\n", cache_path);
	CloseHandle(f_cache);
	return true;

err_free:
	(*ppBytecode)->Release();
	*ppBytecode = NULL;
err_close:
	CloseHandle(f_cache);
	return false;
}

static const D3D_SHADER_MACRO vs_macros[] = { "VERTEX_SHADER", "", NULL, NULL };
static const D3D_SHADER_MACRO ps_macros[] = { "PIXEL_SHADER", "", NULL, NULL };

// This is similar to the other compile routines, but still distinct enough to
// get it's own function for now - TODO: Refactor out the common code
bool CustomShader::compile(char type, wchar_t *filename, const wstring *wname, const wstring *namespace_path)
{
	wchar_t wpath[MAX_PATH], cache_path[MAX_PATH];
	char apath[MAX_PATH];
	HANDLE f;
	DWORD srcDataSize, readSize;
	vector<char> srcData;
	HRESULT hr;
	char shaderModel[7];
	ID3DBlob **ppBytecode = NULL;
	ID3DBlob *pErrorMsgs = NULL;
	const D3D_SHADER_MACRO *macros = NULL;
	bool found = false;
	FILETIME timestamp;

	LogInfo("  %cs=%S\n", type, filename);

	switch (type) {
	case 'v':
		ppBytecode = &vs_bytecode;
		macros = vs_macros;
		vs_override = true;
		break;
	case 'p':
		ppBytecode = &ps_bytecode;
		macros = ps_macros;
		ps_override = true;
		break;
	default:
		// Should not happen
		LogOverlay(LOG_DIRE, "CustomShader::compile: invalid shader type\n");
		goto err;
	}

	// Special value to unbind the shader instead:
	if (!_wcsicmp(filename, L"null"))
		return false;
	// If this section was not in the main d3dx.ini, look
	// for a file relative to the config it came from
	// first, then try relative to the 3DMigoto directory:
	found = false;
	if (!namespace_path->empty()) {
		GetModuleFileName(0, wpath, MAX_PATH);
		wcsrchr(wpath, L'\\')[1] = 0;
		wcscat(wpath, namespace_path->c_str());
		wcscat(wpath, filename);
		if (GetFileAttributes(wpath) != INVALID_FILE_ATTRIBUTES)
			found = true;
	}
	if (!found) {
		if (!GetModuleFileName(0, wpath, MAX_PATH)) {
			LogOverlay(LOG_DIRE, "CustomShader::compile: GetModuleFileName failed\n");
			goto err;
		}
		wcsrchr(wpath, L'\\')[1] = 0;
		wcscat(wpath, filename);
	}

	f = CreateFile(wpath, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (f == INVALID_HANDLE_VALUE) {
		LogOverlay(LOG_WARNING, "Shader not found: %S\n", wpath);
		goto err;
	}
	// Currently always using shader model 5, could allow this to be
	// overridden in the future:
	//_snprintf_s(shaderModel, 7, 7, "%cs_5_0", type);

	_snprintf_s(shaderModel, 7, 7, "%cs_3_0", type);

	// XXX: If we allow the compilation to be customised further (e.g. with
	// addition preprocessor defines), make the cache filename unique for
	// each possible combination
	wchar_t *ext = wcsrchr(wpath, L'.');
	if (ext > wcsrchr(wpath, L'\\'))
		swprintf_s(cache_path, MAX_PATH, L"%.*s.%S.%x.bin", (int)(ext - wpath), wpath, shaderModel, (UINT)compile_flags);
	else
		swprintf_s(cache_path, MAX_PATH, L"%s.%S.%x.bin", wpath, shaderModel, (UINT)compile_flags);

	GetFileTime(f, NULL, NULL, &timestamp);
	if (load_cached_shader(timestamp, cache_path, ppBytecode)) {
		CloseHandle(f);
		return false;
	}

	srcDataSize = GetFileSize(f, 0);
	srcData.resize(srcDataSize);

	if (!ReadFile(f, srcData.data(), srcDataSize, &readSize, 0)
		|| srcDataSize != readSize) {
		LogInfo("    Error reading HLSL file\n");
		goto err_close;
	}
	CloseHandle(f);

	// TODO: Add #defines for StereoParams and IniParams. Define a macro
	// for the type of shader, and maybe allow more defines to be specified
	// in the ini

	// Pass the real filename and use the standard include handler so that
	// #include will work with a relative path from the shader itself.
	// Later we could add a custom include handler to track dependencies so
	// that we can make reloading work better when using includes:
	wcstombs(apath, wpath, MAX_PATH);
	hr = D3DCompile(srcData.data(), srcDataSize, apath, macros, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"main", shaderModel, (UINT)compile_flags, 0, ppBytecode, &pErrorMsgs);

	if (pErrorMsgs) {
		LPVOID errMsg = pErrorMsgs->GetBufferPointer();
		SIZE_T errSize = pErrorMsgs->GetBufferSize();
		LogInfo("--------------------------------------------- BEGIN ---------------------------------------------\n");
		LogOverlay(LOG_NOTICE, "%*s\n", errSize, errMsg);
		LogInfo("---------------------------------------------- END ----------------------------------------------\n");
		pErrorMsgs->Release();
	}

	if (FAILED(hr)) {
		LogOverlay(LOG_WARNING, "Error compiling custom shader\n");
		goto err;
	}

	if (G->CACHE_SHADERS) {
		FILE *fw;

		wfopen_ensuring_access(&fw, cache_path, L"wb");
		if (fw) {
			LogInfo("    Storing compiled shader to %S\n", cache_path);
			fwrite((*ppBytecode)->GetBufferPointer(), 1, (*ppBytecode)->GetBufferSize(), fw);
			fclose(fw);

			set_file_last_write_time(cache_path, &timestamp);
		}
		else
			LogInfo("    Error writing compiled shader to %S\n", cache_path);
	}

	return false;
err_close:
	CloseHandle(f);
err:
	return true;
}

void CreateBlendState(D3D9_BLEND_DESC pBlendDesc, ID3D9BlendState ** ppBlendState)
{
	*ppBlendState = new ID3D9BlendState(pBlendDesc);
};
void GetAlphaTestState(IDirect3DDevice9 * mOrigDevice, ID3D9AlphaTestState ** ppAlphaTestState)
{
	D3D9_ALPHATEST_DESC pDesc;
	DWORD pValue;
	mOrigDevice->GetRenderState(D3DRS_ALPHAFUNC, &pValue);
	pDesc.alpha_func = (D3DCMPFUNC)pValue;
	mOrigDevice->GetRenderState(D3DRS_ALPHAREF, &pValue);
	pDesc.alpha_ref = (DWORD)pValue;
	mOrigDevice->GetRenderState(D3DRS_ALPHATESTENABLE, &pValue);
	pDesc.alpha_test_enable = (BOOL)pValue;

	*ppAlphaTestState = new ID3D9AlphaTestState(pDesc);

};
void GetDepthStencilState(IDirect3DDevice9 * mOrigDevice, ID3D9DepthStencilState ** ppDepthStencilState)
{
	D3D9_DEPTH_STENCIL_DESC pDesc;
	DWORD pValue;
	mOrigDevice->GetRenderState(D3DRS_STENCILENABLE, &pValue);
	BOOL stencil_enable = (pValue != 0);
	pDesc.stencil_enable = stencil_enable;
	mOrigDevice->GetRenderState(D3DRS_STENCILFAIL, &pValue);
	pDesc.stencil_fail = (D3DSTENCILOP)pValue;
	mOrigDevice->GetRenderState(D3DRS_STENCILZFAIL, &pValue);
	pDesc.stencil_z_fail = (D3DSTENCILOP)pValue;
	mOrigDevice->GetRenderState(D3DRS_STENCILPASS, &pValue);
	pDesc.stencil_pass = (D3DSTENCILOP)pValue;
	mOrigDevice->GetRenderState(D3DRS_STENCILFUNC, &pValue);
	pDesc.stencil_func = (D3DCMPFUNC)pValue;
	mOrigDevice->GetRenderState(D3DRS_STENCILREF, &pValue);
	pDesc.stencil_ref = (UINT)pValue;
	mOrigDevice->GetRenderState(D3DRS_STENCILMASK, &pValue);
	pDesc.stencil_mask = pValue;
	mOrigDevice->GetRenderState(D3DRS_STENCILWRITEMASK, &pValue);
	pDesc.stencil_write_mask = pValue;

	mOrigDevice->GetRenderState(D3DRS_ZENABLE, &pValue);
	BOOL z_enable = (pValue != 0);
	pDesc.z_enable = z_enable;
	mOrigDevice->GetRenderState(D3DRS_ZWRITEENABLE, &pValue);
	BOOL z_write_enable = (pValue != 0);
	pDesc.z_write_enable = z_write_enable;
	mOrigDevice->GetRenderState(D3DRS_ZFUNC, &pValue);
	pDesc.z_func = (D3DCMPFUNC)pValue;
	mOrigDevice->GetRenderState(D3DRS_TWOSIDEDSTENCILMODE, &pValue);
	BOOL two_sided_stencil_mode = (pValue != 0);
	pDesc.two_sided_stencil_mode = two_sided_stencil_mode;
	mOrigDevice->GetRenderState(D3DRS_CCW_STENCILFAIL, &pValue);
	pDesc.ccw_stencil_fail = (D3DSTENCILOP)pValue;
	mOrigDevice->GetRenderState(D3DRS_CCW_STENCILZFAIL, &pValue);
	pDesc.ccw_stencil_z_fail = (D3DSTENCILOP)pValue;
	mOrigDevice->GetRenderState(D3DRS_CCW_STENCILPASS, &pValue);
	pDesc.ccw_stencil_pass = (D3DSTENCILOP)pValue;
	mOrigDevice->GetRenderState(D3DRS_CCW_STENCILFUNC, &pValue);
	pDesc.ccw_stencil_func = (D3DCMPFUNC)pValue;
	mOrigDevice->GetRenderState(D3DRS_DEPTHBIAS, &pValue);
	pDesc.depth_bias = (UINT)pValue;

	*ppDepthStencilState = new ID3D9DepthStencilState(pDesc);

};
void SetRSState(IDirect3DDevice9 * mOrigDevice, ID3D9RasterizerState * pRasterizerStateState)
{

	D3D9_RASTERIZER_DESC pDesc;
	pRasterizerStateState->GetDesc(&pDesc);
	mOrigDevice->SetRenderState(D3DRS_FILLMODE, pDesc.fill_mode);
	mOrigDevice->SetRenderState(D3DRS_ANTIALIASEDLINEENABLE, pDesc.anti_aliased_line_enable);
	mOrigDevice->SetRenderState(D3DRS_CLIPPING, pDesc.clipping);
	mOrigDevice->SetRenderState(D3DRS_CULLMODE, pDesc.cull_mode);
	mOrigDevice->SetRenderState(D3DRS_DEPTHBIAS, pDesc.depth_bias);
	mOrigDevice->SetRenderState(D3DRS_SCISSORTESTENABLE, pDesc.scissor_test_enable);
	mOrigDevice->SetRenderState(D3DRS_SLOPESCALEDEPTHBIAS, pDesc.slope_scale_depth_bias);
	mOrigDevice->SetRenderState(D3DRS_CLIPPLANEENABLE, pDesc.clip_plane_enable);
	mOrigDevice->SetRenderState(D3DRS_MULTISAMPLEANTIALIAS, pDesc.multisample_antialias);
}
void SetAlphaTestState(IDirect3DDevice9 * mOrigDevice, ID3D9AlphaTestState * pAlphaTestState)
{
	D3D9_ALPHATEST_DESC pDesc;
	pAlphaTestState->GetDesc(&pDesc);
	mOrigDevice->SetRenderState(D3DRS_ALPHAREF, pDesc.alpha_ref);
	mOrigDevice->SetRenderState(D3DRS_ALPHATESTENABLE, pDesc.alpha_test_enable);
	mOrigDevice->SetRenderState(D3DRS_ALPHAFUNC, pDesc.alpha_func);
}
void SetDepthStencilState(IDirect3DDevice9 * mOrigDevice, ID3D9DepthStencilState * pDepthStencilState)
{

	D3D9_DEPTH_STENCIL_DESC pDesc;
	pDepthStencilState->GetDesc(&pDesc);
	mOrigDevice->SetRenderState(D3DRS_STENCILENABLE, pDesc.stencil_enable);
	mOrigDevice->SetRenderState(D3DRS_STENCILFAIL, pDesc.stencil_fail);
	mOrigDevice->SetRenderState(D3DRS_STENCILZFAIL, pDesc.stencil_z_fail);
	mOrigDevice->SetRenderState(D3DRS_STENCILPASS, pDesc.stencil_pass);
	mOrigDevice->SetRenderState(D3DRS_STENCILFUNC, pDesc.stencil_func);
	mOrigDevice->SetRenderState(D3DRS_STENCILREF, pDesc.stencil_ref);
	mOrigDevice->SetRenderState(D3DRS_STENCILMASK, pDesc.stencil_mask);
	mOrigDevice->SetRenderState(D3DRS_STENCILWRITEMASK, pDesc.stencil_write_mask);

	mOrigDevice->SetRenderState(D3DRS_ZENABLE, pDesc.z_enable);
	mOrigDevice->SetRenderState(D3DRS_ZWRITEENABLE, pDesc.z_write_enable);
	mOrigDevice->SetRenderState(D3DRS_ZFUNC, pDesc.z_func);
	mOrigDevice->SetRenderState(D3DRS_TWOSIDEDSTENCILMODE, pDesc.two_sided_stencil_mode);
	mOrigDevice->SetRenderState(D3DRS_CCW_STENCILFAIL, pDesc.ccw_stencil_fail);
	mOrigDevice->SetRenderState(D3DRS_CCW_STENCILZFAIL, pDesc.ccw_stencil_z_fail);
	mOrigDevice->SetRenderState(D3DRS_CCW_STENCILPASS, pDesc.ccw_stencil_pass);
	mOrigDevice->SetRenderState(D3DRS_CCW_STENCILFUNC, pDesc.ccw_stencil_func);
	mOrigDevice->SetRenderState(D3DRS_DEPTHBIAS, pDesc.depth_bias);
}
void CreateDepthStencilState(D3D9_DEPTH_STENCIL_DESC pDepthStencilDesc, ID3D9DepthStencilState ** pDepthStencilState)
{
	*pDepthStencilState = new ID3D9DepthStencilState(pDepthStencilDesc);
};
void CreateAlphaTestState(D3D9_ALPHATEST_DESC pAlphaTestDesc, ID3D9AlphaTestState ** pAlphaTestState)
{
	*pAlphaTestState = new ID3D9AlphaTestState(pAlphaTestDesc);
};
void CreateRasterizerState(D3D9_RASTERIZER_DESC desc, ID3D9RasterizerState **state) {

	*state = new ID3D9RasterizerState(desc);
}
void CustomShader::substantiate(IDirect3DDevice9 *mOrigDevice, D3D9Wrapper::IDirect3DDevice9 *mDevice)
{
	if (substantiated)
		return;
	substantiated = true;

	mHackerDevice = mDevice;

	if (vs_bytecode) {
		mOrigDevice->CreateVertexShader((DWORD*)vs_bytecode->GetBufferPointer(), &vs);
		if (vs && mHackerDevice)
			mHackerDevice->migotoResourceCount++;
	}
	if (ps_bytecode) {
		mOrigDevice->CreatePixelShader((DWORD*)ps_bytecode->GetBufferPointer(), &ps);
		if (ps && mHackerDevice)
			mHackerDevice->migotoResourceCount++;
	}
	if (blend_override == 1) // 2 will merge the blend state at draw time
		CreateBlendState(blend_desc, &blend_state);

	if (depth_stencil_override == 1) // 2 will merge depth/stencil state at draw time
		CreateDepthStencilState(depth_stencil_desc, &depth_stencil_state);

	if (rs_override == 1) // 2 will merge rasterizer state at draw time
		CreateRasterizerState(rs_desc, &rs_state);

	if (alpha_test_override == 1)
		CreateAlphaTestState(alpha_test_desc, &alpha_test_state);
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
FLOAT getRGBAComponent(D3DCOLOR colour, int component) {
	switch (component) {
	case 0:
		return (FLOAT)getR(colour);
	case 1:
		return (FLOAT)getG(colour);
	case 2:
		return (FLOAT)getB(colour);
	case 3:
		return (FLOAT)getA(colour);
	default:
		return -1;
	}
}
void CustomShader::merge_blend_states(ID3D9BlendState *src_state, IDirect3DDevice9 *mOrigDevice)
{
	D3D9_BLEND_DESC src_desc;
	int i;

	if (blend_override != 2)
		return;

	if (blend_state)
		blend_state->~ID3D9BlendState();
	blend_state = NULL;

	if (src_state) {
		src_state->GetDesc(&src_desc);
	}
	else {
		// There is no state set, so DX will be using defaults. Set the
		// source description to the defaults so the merge will still
		// work as expected:
		src_desc.alpha_blend_enable = FALSE;
		src_desc.src_blend = D3DBLEND_ONE;
		src_desc.dest_blend = D3DBLEND_ZERO;
		src_desc.blend_op = D3DBLENDOP_ADD;
		src_desc.seperate_alpha_blend_enable = FALSE;
		src_desc.src_blend_alpha = D3DBLEND_ONE;
		src_desc.dest_blend_alpha = D3DBLEND_ZERO;
		src_desc.blend_op_alpha = D3DBLENDOP_ADD;
		src_desc.color_write_enable = 0x0000000F;
		src_desc.blend_factor = 0xffffffff;
		src_desc.texture_factor = 0xFFFFFFFF;
		src_desc.color_write_enable1 = 0x0000000f;
		src_desc.color_write_enable2 = 0x0000000f;
		src_desc.color_write_enable3 = 0x0000000f;
		src_desc.multisample_mask = 0xFFFFFFFF;
	}

	memcpy_masked_merge(&blend_desc, &src_desc, &blend_mask, sizeof(D3D9_BLEND_DESC));
	for (i = 0; i < 4; i++) {
		if (blend_factor_merge_mask[i])
			blend_factor[i] = getRGBAComponent(src_desc.blend_factor, i);
	}
	blend_desc.multisample_mask = blend_desc.multisample_mask & ~blend_sample_mask_merge_mask | src_desc.multisample_mask & blend_sample_mask_merge_mask;
	CreateBlendState(blend_desc, &blend_state);
}

void CustomShader::merge_depth_stencil_states(ID3D9DepthStencilState *src_state)
{
	D3D9_DEPTH_STENCIL_DESC src_desc;

	if (depth_stencil_override != 2)
		return;

	if (depth_stencil_state)
		depth_stencil_state->~ID3D9DepthStencilState();
	depth_stencil_state = NULL;

	if (src_state) {
		src_state->GetDesc(&src_desc);
	}
	else {
		// There is no state set, so DX will be using defaults. Set the
		// source description to the defaults so the merge will still
		// work as expected:

		src_desc.stencil_enable = FALSE;
		src_desc.stencil_write_mask = 0xFFFFFFFF;
		src_desc.stencil_mask = 0;
		src_desc.stencil_ref = 0;
		src_desc.stencil_func = D3DCMP_ALWAYS;
		src_desc.stencil_fail = D3DSTENCILOP_KEEP;
		src_desc.stencil_pass = D3DSTENCILOP_KEEP;
		src_desc.stencil_z_fail = D3DSTENCILOP_KEEP;
		src_desc.z_enable = TRUE;
		src_desc.z_write_enable = TRUE;
		src_desc.z_func = D3DCMP_LESSEQUAL;
		src_desc.two_sided_stencil_mode = FALSE;
		src_desc.ccw_stencil_func = D3DCMP_ALWAYS;
		src_desc.ccw_stencil_fail = D3DSTENCILOP_KEEP;
		src_desc.ccw_stencil_pass = D3DSTENCILOP_KEEP;
		src_desc.ccw_stencil_z_fail = D3DSTENCILOP_KEEP;
		src_desc.depth_bias = 0;
	}

	memcpy_masked_merge(&depth_stencil_desc, &src_desc, &depth_stencil_mask, sizeof(D3D9_DEPTH_STENCIL_DESC));
	depth_stencil_desc.stencil_ref = depth_stencil_desc.stencil_ref & ~stencil_ref_mask | src_desc.stencil_ref & stencil_ref_mask;

	CreateDepthStencilState(depth_stencil_desc, &depth_stencil_state);
}

void CustomShader::merge_rasterizer_states(ID3D9RasterizerState *src_state, IDirect3DDevice9 *mOrigDevice)
{
	D3D9_RASTERIZER_DESC src_desc;

	if (rs_override != 2)
		return;

	if (rs_state)
		rs_state->~ID3D9RasterizerState();
	rs_state = NULL;

	if (src_state) {
		src_state->GetDesc(&src_desc);
	}
	else {
		// There is no state set, so DX will be using defaults. Set the
		// source description to the defaults so the merge will still
		// work as expected:
		src_desc.fill_mode = D3DFILL_SOLID;
		src_desc.cull_mode = D3DCULL_CCW;
		src_desc.depth_bias = 0;
		src_desc.slope_scale_depth_bias = 0;
		src_desc.clipping = TRUE;
		src_desc.scissor_test_enable = FALSE;
		src_desc.anti_aliased_line_enable = FALSE;
		src_desc.clip_plane_enable = 0;
		src_desc.multisample_antialias = TRUE;
	}

	memcpy_masked_merge(&rs_desc, &src_desc, &rs_mask, sizeof(D3D9_RASTERIZER_DESC));
	CreateRasterizerState(rs_desc, &rs_state);
}
void _SetSamplerStates(IDirect3DDevice9 * mOrigDevice, D3D9_SAMPLER_DESC pDesc, UINT slot) {
	mOrigDevice->SetSamplerState(slot, D3DSAMP_ADDRESSU, pDesc.address_u);
	mOrigDevice->SetSamplerState(slot, D3DSAMP_ADDRESSV, pDesc.address_v);
	mOrigDevice->SetSamplerState(slot, D3DSAMP_ADDRESSW, pDesc.address_w);
	mOrigDevice->SetSamplerState(slot, D3DSAMP_BORDERCOLOR, pDesc.border_colour);
	mOrigDevice->SetSamplerState(slot, D3DSAMP_MAXANISOTROPY, pDesc.max_anisotropy);
	mOrigDevice->SetSamplerState(slot, D3DSAMP_MAXMIPLEVEL, pDesc.max_mip_level);
	mOrigDevice->SetSamplerState(slot, D3DSAMP_MIPMAPLODBIAS, pDesc.mip_map_lod_bias);
	mOrigDevice->SetSamplerState(slot, D3DSAMP_MINFILTER, pDesc.min_filter);
	mOrigDevice->SetSamplerState(slot, D3DSAMP_MIPFILTER, pDesc.mip_filter);
	mOrigDevice->SetSamplerState(slot, D3DSAMP_MAGFILTER, pDesc.mag_filter);
	mOrigDevice->SetSamplerState(slot, D3DSAMP_SRGBTEXTURE, pDesc.srgb_texture);
	mOrigDevice->SetSamplerState(slot, D3DSAMP_ELEMENTINDEX, pDesc.element_index);
	mOrigDevice->SetSamplerState(slot, D3DSAMP_DMAPOFFSET, pDesc.dmap_offset);
}
void RunCustomShaderCommand::SetSamplerStates(::IDirect3DDevice9 * mOrigDevice, std::map<UINT, ID3D9SamplerState*> ss)
{
	map<UINT, ID3D9SamplerState*>::iterator it;
	for (it = ss.begin(); it != ss.end(); it++)
	{
		D3D9_SAMPLER_DESC pDesc;
		it->second->GetDesc(&pDesc);
		_SetSamplerStates(mOrigDevice, pDesc, it->first);
	}
}
void _GetSamplerStates(IDirect3DDevice9 * mOrigDevice, D3D9_SAMPLER_DESC *pDesc, UINT reg) {
	DWORD pValue;
	mOrigDevice->GetSamplerState(reg, D3DSAMP_ADDRESSU, &pValue);
	pDesc->address_u = (D3DTEXTUREADDRESS)pValue;
	mOrigDevice->GetSamplerState(reg, D3DSAMP_ADDRESSV, &pValue);
	pDesc->address_v = (D3DTEXTUREADDRESS)pValue;
	mOrigDevice->GetSamplerState(reg, D3DSAMP_ADDRESSW, &pValue);
	pDesc->address_w = (D3DTEXTUREADDRESS)pValue;
	mOrigDevice->GetSamplerState(reg, D3DSAMP_BORDERCOLOR, &pValue);
	pDesc->border_colour = (D3DCOLOR)pValue;
	mOrigDevice->GetSamplerState(reg, D3DSAMP_MAXANISOTROPY, &pValue);
	pDesc->max_anisotropy = pValue;
	mOrigDevice->GetSamplerState(reg, D3DSAMP_MAXMIPLEVEL, &pValue);
	pDesc->max_mip_level = pValue;
	mOrigDevice->GetSamplerState(reg, D3DSAMP_MIPMAPLODBIAS, &pValue);
	pDesc->mip_map_lod_bias = pValue;
	mOrigDevice->GetSamplerState(reg, D3DSAMP_MINFILTER, &pValue);
	pDesc->min_filter = (D3DTEXTUREFILTERTYPE)pValue;
	mOrigDevice->GetSamplerState(reg, D3DSAMP_MIPFILTER, &pValue);
	pDesc->mip_filter = (D3DTEXTUREFILTERTYPE)pValue;
	mOrigDevice->GetSamplerState(reg, D3DSAMP_MAGFILTER, &pValue);
	pDesc->mag_filter = (D3DTEXTUREFILTERTYPE)pValue;
	mOrigDevice->GetSamplerState(reg, D3DSAMP_SRGBTEXTURE, &pValue);
	pDesc->srgb_texture = pValue;
	mOrigDevice->GetSamplerState(reg, D3DSAMP_ELEMENTINDEX, &pValue);
	pDesc->element_index = pValue;
	mOrigDevice->GetSamplerState(reg, D3DSAMP_DMAPOFFSET, &pValue);
	pDesc->dmap_offset = pValue;

}
void RunCustomShaderCommand::GetSamplerStates(::IDirect3DDevice9 * mOrigDevice, std::map<UINT, ID3D9SamplerState*> *saved_sampler_states) {

	map<UINT, ID3D9SamplerState*>::iterator it;

	for (it = custom_shader->sampler_states.begin(); it != custom_shader->sampler_states.end(); it++) {
		D3D9_SAMPLER_DESC pDesc;
		_GetSamplerStates(mOrigDevice, &pDesc, it->first);
		saved_sampler_states->insert(std::pair<UINT, ID3D9SamplerState*>(it->first, new ID3D9SamplerState(pDesc)));
	}
}


void GetRSState(IDirect3DDevice9 * mOrigDevice, ID3D9RasterizerState ** ppRSState)
{
	D3D9_RASTERIZER_DESC pDesc;
	DWORD value;
	mOrigDevice->GetRenderState(D3DRS_FILLMODE, &value);
	pDesc.fill_mode = (D3DFILLMODE)value;
	mOrigDevice->GetRenderState(D3DRS_ANTIALIASEDLINEENABLE, &value);
	pDesc.anti_aliased_line_enable = value;
	mOrigDevice->GetRenderState(D3DRS_CLIPPING, &value);
	pDesc.clipping = value;
	mOrigDevice->GetRenderState(D3DRS_CULLMODE, &value);
	pDesc.cull_mode = (D3DCULL)value;
	mOrigDevice->GetRenderState(D3DRS_DEPTHBIAS, &value);
	pDesc.depth_bias = value;
	mOrigDevice->GetRenderState(D3DRS_SCISSORTESTENABLE, &value);
	pDesc.scissor_test_enable = value;
	mOrigDevice->GetRenderState(D3DRS_SLOPESCALEDEPTHBIAS, &value);
	pDesc.slope_scale_depth_bias = value;
	mOrigDevice->GetRenderState(D3DRS_CLIPPLANEENABLE, &value);
	pDesc.clip_plane_enable = value;
	mOrigDevice->GetRenderState(D3DRS_MULTISAMPLEANTIALIAS, &value);
	pDesc.multisample_antialias = (BOOL)value;

	*ppRSState = new ID3D9RasterizerState(pDesc);

};

void GetBlendState(IDirect3DDevice9 * mOrigDevice, ID3D9BlendState **ppBlendState, FLOAT* pBlendFactor)
{
	D3D9_BLEND_DESC pDesc;
	DWORD pValue;
	mOrigDevice->GetRenderState(D3DRS_ALPHABLENDENABLE, &pValue);
	pDesc.alpha_blend_enable = pValue;
	mOrigDevice->GetRenderState(D3DRS_BLENDOP, &pValue);
	pDesc.blend_op = (D3DBLENDOP)pValue;
	mOrigDevice->GetRenderState(D3DRS_BLENDOPALPHA, &pValue);
	pDesc.blend_op_alpha = (D3DBLENDOP)pValue;
	mOrigDevice->GetRenderState(D3DRS_COLORWRITEENABLE, &pValue);
	pDesc.color_write_enable = (DWORD)pValue;
	mOrigDevice->GetRenderState(D3DRS_DESTBLENDALPHA, &pValue);
	pDesc.dest_blend_alpha = (D3DBLEND)pValue;
	mOrigDevice->GetRenderState(D3DRS_DESTBLEND, &pValue);
	pDesc.dest_blend = (D3DBLEND)pValue;
	mOrigDevice->GetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, &pValue);
	pDesc.seperate_alpha_blend_enable = pValue;
	mOrigDevice->GetRenderState(D3DRS_SRCBLEND, &pValue);
	pDesc.src_blend = (D3DBLEND)pValue;
	mOrigDevice->GetRenderState(D3DRS_SRCBLENDALPHA, &pValue);
	pDesc.src_blend_alpha = (D3DBLEND)pValue;
	mOrigDevice->GetRenderState(D3DRS_BLENDFACTOR, &pValue);
	pDesc.blend_factor = (D3DCOLOR)pValue;
	for (int i = 0; i < 4; i++) {
		pBlendFactor[i] = getRGBAComponent(pDesc.blend_factor, i);

	}
	mOrigDevice->GetRenderState(D3DRS_TEXTUREFACTOR, &pValue);
	pDesc.texture_factor = (D3DCOLOR)pValue;
	mOrigDevice->GetRenderState(D3DRS_COLORWRITEENABLE1, &pValue);
	pDesc.color_write_enable1 = (DWORD)pValue;
	mOrigDevice->GetRenderState(D3DRS_COLORWRITEENABLE2, &pValue);
	pDesc.color_write_enable2 = (DWORD)pValue;
	mOrigDevice->GetRenderState(D3DRS_COLORWRITEENABLE3, &pValue);
	pDesc.color_write_enable3 = (DWORD)pValue;
	mOrigDevice->GetRenderState(D3DRS_MULTISAMPLEMASK, &pValue);
	pDesc.multisample_mask = (DWORD)pValue;
	*ppBlendState = new ID3D9BlendState(pDesc);

};

void SetBlendState(IDirect3DDevice9 * mOrigDevice, ID3D9BlendState *pBlendState, FLOAT BlendFactor[4])
{
	D3D9_BLEND_DESC pDesc;
	pBlendState->GetDesc(&pDesc);
	mOrigDevice->SetRenderState(D3DRS_ALPHABLENDENABLE, pDesc.alpha_blend_enable);
	mOrigDevice->SetRenderState(D3DRS_BLENDOP, pDesc.blend_op);
	mOrigDevice->SetRenderState(D3DRS_BLENDOPALPHA, pDesc.blend_op_alpha);
	mOrigDevice->SetRenderState(D3DRS_COLORWRITEENABLE, pDesc.color_write_enable);
	mOrigDevice->SetRenderState(D3DRS_DESTBLEND, pDesc.dest_blend);
	mOrigDevice->SetRenderState(D3DRS_DESTBLENDALPHA, pDesc.dest_blend_alpha);
	mOrigDevice->SetRenderState(D3DRS_SEPARATEALPHABLENDENABLE, pDesc.seperate_alpha_blend_enable);
	mOrigDevice->SetRenderState(D3DRS_SRCBLEND, pDesc.src_blend);
	mOrigDevice->SetRenderState(D3DRS_SRCBLENDALPHA, pDesc.src_blend_alpha);
	mOrigDevice->SetRenderState(D3DRS_BLENDFACTOR, D3DCOLOR_RGBA((int)BlendFactor[0], (int)BlendFactor[1], (int)BlendFactor[2], (int)BlendFactor[3]));
	mOrigDevice->SetRenderState(D3DRS_MULTISAMPLEMASK, pDesc.multisample_mask);

	mOrigDevice->SetRenderState(D3DRS_TEXTUREFACTOR, pDesc.texture_factor);
	mOrigDevice->SetRenderState(D3DRS_COLORWRITEENABLE1, pDesc.color_write_enable1);
	mOrigDevice->SetRenderState(D3DRS_COLORWRITEENABLE2, pDesc.color_write_enable2);
	mOrigDevice->SetRenderState(D3DRS_COLORWRITEENABLE3, pDesc.color_write_enable3);
	mOrigDevice->SetRenderState(D3DRS_MULTISAMPLEMASK, pDesc.multisample_mask);
}

void RunCustomShaderCommand::run(CommandListState *state)
{
	IDirect3DDevice9 *mOrigDevice = state->mOrigDevice;
	IDirect3DVertexShader9 *saved_vs = NULL;
	IDirect3DPixelShader9 *saved_ps = NULL;
	ID3D9BlendState *saved_blend = NULL;
	ID3D9DepthStencilState *saved_depth_stencil = NULL;
	ID3D9RasterizerState *saved_rs = NULL;
	ID3D9AlphaTestState *saved_ats = NULL;
	D3DVIEWPORT9 saved_viewport;
	FLOAT saved_blend_factor[4];
	bool saved_post;
	struct OMState om_state;
	D3DPRIMITIVETYPE saved_primitive_type;
	map<UINT, ID3D9SamplerState*> saved_sampler_states;
	vector<D3D9Wrapper::IDirect3DSurface9*> saved_rtss_wrappers;
	D3D9Wrapper::IDirect3DSurface9 *saved_dss_wrapper = NULL;
	COMMAND_LIST_LOG(state, "%S\n", ini_line.c_str());

	if (custom_shader->enable_timer == true) {
		if ((chrono::high_resolution_clock::now() - custom_shader->last_time_run) < custom_shader->run_interval) {
			COMMAND_LIST_LOG(state, "  shader skipped awaiting interval\n");
			return;
		}
		else {
			custom_shader->last_time_run += custom_shader->run_interval;
		}
	}
	if (custom_shader->max_executions_per_frame) {
		if (custom_shader->frame_no != G->frame_no) {
			custom_shader->frame_no = G->frame_no;
			custom_shader->executions_this_frame = 1;
		}
		else if (custom_shader->executions_this_frame++ >= custom_shader->max_executions_per_frame) {
			COMMAND_LIST_LOG(state, "  max_executions_per_frame exceeded\n");
			return;
		}
	}

	custom_shader->substantiate(mOrigDevice, state->mHackerDevice);
	state->m_activeStereoTextureStages.clear();
	// Assign custom shaders first before running the command lists, and
	// restore them last. This is so that if someone was injecting a
	// sequence of pixel shaders that all shared a common vertex shader
	// we can avoid having to repeatedly save & restore the vertex shader
	// by calling the next shader in sequence from the command list after
	// the draw call.

	if (custom_shader->vs_override) {
		mOrigDevice->GetVertexShader(&saved_vs);
		mOrigDevice->SetVertexShader(custom_shader->vs);
	}
	if (custom_shader->ps_override) {
		mOrigDevice->GetPixelShader(&saved_ps);
		mOrigDevice->SetPixelShader(custom_shader->ps);
	}
	if (custom_shader->blend_override) {
		GetBlendState(mOrigDevice, &saved_blend, saved_blend_factor);
		custom_shader->merge_blend_states(saved_blend, mOrigDevice);
		SetBlendState(mOrigDevice, custom_shader->blend_state, custom_shader->blend_factor);
	}

	if (custom_shader->alpha_test_override) {
		GetAlphaTestState(mOrigDevice, &saved_ats);
		SetAlphaTestState(mOrigDevice, custom_shader->alpha_test_state);
	}
	if (custom_shader->depth_stencil_override) {
		GetDepthStencilState(mOrigDevice, &saved_depth_stencil);
		custom_shader->merge_depth_stencil_states(saved_depth_stencil);
		SetDepthStencilState(mOrigDevice, custom_shader->depth_stencil_state);
	}
	if (custom_shader->rs_override) {
		GetRSState(mOrigDevice, &saved_rs);
		custom_shader->merge_rasterizer_states(saved_rs, mOrigDevice);
		SetRSState(mOrigDevice, custom_shader->rs_state);
	}
	DrawCallInfo c = DrawCallInfo(DrawCall::Invalid, custom_shader->primitive_type, 0, 0, 0, 0, 0, 0, NULL, 0, NULL, D3DFMT_UNKNOWN, 0, NULL, NULL, NULL);
	if (state->call_info == NULL) {
		saved_primitive_type = D3DPRIMITIVETYPE(-1);
		state->call_info = &c;
	}
	else {
		saved_primitive_type = state->call_info->primitive_type;
		if (custom_shader->primitive_type != D3DPRIMITIVETYPE(-1))
			state->call_info->primitive_type = custom_shader->primitive_type;
	}
	if (custom_shader->sampler_override) {
		GetSamplerStates(mOrigDevice, &saved_sampler_states);
		SetSamplerStates(mOrigDevice, custom_shader->sampler_states);
	}
	// We save off the viewports unconditionally for now. We could
	// potentially skip this by flagging if a command list may alter them,
	// but that probably wouldn't buy us anything:
	mOrigDevice->GetViewport(&saved_viewport);
	DWORD i;
	if (G->gForceStereo == 2) {
		saved_dss_wrapper = state->mHackerDevice->m_pActiveDepthStencil;
		saved_rtss_wrappers = state->mHackerDevice->m_activeRenderTargets;
	}
	else
		save_om_state(state->mOrigDevice, &om_state);
	saved_post = state->post;
	state->post = false;
	_RunCommandList(&custom_shader->command_list, state);
	state->post = true;
	_RunCommandList(&custom_shader->post_command_list, state);
	state->post = saved_post;
	// Finally restore the original shaders
	if (custom_shader->vs_override)
		mOrigDevice->SetVertexShader(saved_vs);
	if (custom_shader->ps_override)
		mOrigDevice->SetPixelShader(saved_ps);
	if (custom_shader->blend_override)
		SetBlendState(mOrigDevice, saved_blend, saved_blend_factor);
	if (custom_shader->depth_stencil_override)
		SetDepthStencilState(mOrigDevice, saved_depth_stencil);
	if (custom_shader->rs_override)
		SetRSState(mOrigDevice, saved_rs);
	if (custom_shader->alpha_test_override)
		SetAlphaTestState(mOrigDevice, saved_ats);

	if (saved_primitive_type != D3DPRIMITIVETYPE(-1))
		state->call_info->primitive_type = saved_primitive_type;
	else
		state->call_info = NULL;
	if (custom_shader->sampler_override)
		SetSamplerStates(mOrigDevice, saved_sampler_states);
	mOrigDevice->SetViewport(&saved_viewport);
	if (G->gForceStereo == 2) {
		if (saved_dss_wrapper) {
			state->mHackerDevice->m_pActiveDepthStencil = saved_dss_wrapper;
			if (state->mHackerDevice->currentRenderingSide == D3D9Wrapper::RenderPosition::Left)
				mOrigDevice->SetDepthStencilSurface(saved_dss_wrapper->DirectModeGetLeft());
			else
				mOrigDevice->SetDepthStencilSurface(saved_dss_wrapper->DirectModeGetRight());
		}
		state->mHackerDevice->m_activeRenderTargets = saved_rtss_wrappers;
		for (i = 0; i < saved_rtss_wrappers.size(); i++) {
			D3D9Wrapper::IDirect3DSurface9 *rts = saved_rtss_wrappers.at(i);
			if (rts) {
				if (state->mHackerDevice->currentRenderingSide == D3D9Wrapper::RenderPosition::Left)
					mOrigDevice->SetRenderTarget(i, rts->DirectModeGetLeft());
				else
					mOrigDevice->SetRenderTarget(i, rts->DirectModeGetRight());
			}
		}
	}
	else
		restore_om_state(state->mOrigDevice, &om_state);
	if (saved_vs)
		saved_vs->Release();
	if (saved_ps)
		saved_ps->Release();
	if (saved_blend)
		saved_blend->~ID3D9BlendState();
	if (saved_depth_stencil)
		saved_depth_stencil->~ID3D9DepthStencilState();
	if (saved_rs)
		saved_rs->~ID3D9RasterizerState();
	map<UINT, ID3D9SamplerState*>::iterator it;
	for (it = saved_sampler_states.begin(); it != saved_sampler_states.end(); it++) {
		it->second->~ID3D9SamplerState();
	}
	saved_sampler_states.clear();
}

bool RunCustomShaderCommand::noop(bool post, bool ignore_cto_pre, bool ignore_cto_post)
{
	return (custom_shader->command_list.commands.empty() && custom_shader->post_command_list.commands.empty());
}
void RunExplicitCommandList::run(CommandListState *state)
{
	bool saved_post;

	COMMAND_LIST_LOG(state, "%S\n", ini_line.c_str());

	if (run_pre_and_post_together) {
		saved_post = state->post;
		state->post = false;
		_RunCommandList(&command_list_section->command_list, state);
		state->post = true;
		_RunCommandList(&command_list_section->post_command_list, state);
		state->post = saved_post;
	}
	else if (state->post)
		_RunCommandList(&command_list_section->post_command_list, state);
	else
		_RunCommandList(&command_list_section->command_list, state);
}

bool RunExplicitCommandList::noop(bool post, bool ignore_cto_pre, bool ignore_cto_post)
{
	if (run_pre_and_post_together)
		return (command_list_section->command_list.commands.empty() && command_list_section->post_command_list.commands.empty());

	if (post)
		return command_list_section->post_command_list.commands.empty();
	return command_list_section->command_list.commands.empty();
}

void LinkCommandLists(CommandList *dst, CommandList *link, const wstring *ini_line)
{
	RunLinkedCommandList *operation = new RunLinkedCommandList(link);
	operation->ini_line = *ini_line;
	dst->commands.push_back(std::shared_ptr<CommandListCommand>(operation));
}

void RunLinkedCommandList::run(CommandListState *state)
{
	_RunCommandList(link, state, false);
}

bool RunLinkedCommandList::noop(bool post, bool ignore_cto_pre, bool ignore_cto_post)
{
	return link->commands.empty();
}

void ReleaseCommandListDeviceResources(D3D9Wrapper::IDirect3DDevice9 * mHackerDevice)
{
	EnterCriticalSection(&G->mCriticalSection);
	for (auto const& copyOp : resourceCopyOperations) {
		for (auto i = copyOp->resource_pool.cache.begin(); i != copyOp->resource_pool.cache.end(); /* not hoisted */ /* no increment */)
		{
			if (i->first.second == mHackerDevice) {
				i->first.second->migotoResourceCount--;
				if (i->second.resource) {
					ULONG result = i->second.resource->Release();
					LogDebug("Releasing Command List Device Resource, Copy Op Pool Resource, result = %d\n", result);
				}
				i = copyOp->resource_pool.cache.erase(i);
			}
			else {
				i++;
			}
		}
		if (copyOp->cached_resource) {
			ULONG result = copyOp->cached_resource->Release();
			LogDebug("Releasing Command List Device Resource, Copy Op Cached Resource, result = %d\n", result);
			copyOp->cached_resource = NULL;
		}
		if (copyOp->stereo2mono_intermediate) {
			ULONG result = copyOp->stereo2mono_intermediate->Release();
			LogDebug("Releasing Command List Device Resource, stereo2mono_intermediate Resource, result = %d\n", result);
			copyOp->stereo2mono_intermediate = NULL;
		}
	}

	for (CustomResources::iterator it = customResources.begin(); it != customResources.end(); ++it) {

		for (auto i = it->second.resource_pool.cache.begin(); i != it->second.resource_pool.cache.end(); /* not hoisted */ /* no increment */)
		{
			if (i->first.second == mHackerDevice) {
				i->first.second->migotoResourceCount--;
				if (i->second.resource) {
					ULONG result = i->second.resource->Release();
					LogDebug("Releasing Command List Device Resource, Custom Resource Pool Resource, result = %d\n", result);
				}
				i = it->second.resource_pool.cache.erase(i);
			}
			else {
				i++;
			}
		}
		if (it->second.resource) {
			ULONG result = it->second.resource->Release();
			LogDebug("Releasing Command List Device Resource,  Custom Resource, result = %d\n", result);
			it->second.resource = NULL;
			it->second.substantiated = false;
		}
	}
	for (CustomShaders::iterator it = customShaders.begin(); it != customShaders.end(); ++it) {
		if (it->second.mHackerDevice == mHackerDevice) {
			if (it->second.ps) {
				ULONG result = it->second.ps->Release();
				LogDebug("Releasing Command List Device Resource, Pixel Shader, result = %d\n", result);
				it->second.mHackerDevice->migotoResourceCount--;
				it->second.ps = NULL;
			}
			if (it->second.vs) {
				ULONG result = it->second.vs->Release();
				LogDebug("Releasing Command List Device Resource, Vertex Shader, result = %d\n", result);
				it->second.mHackerDevice->migotoResourceCount--;
				it->second.vs = NULL;
			}
		}
	}
	LeaveCriticalSection(&G->mCriticalSection);
}

void RecreateCommandListCustomShaders(D3D9Wrapper::IDirect3DDevice9 * mHackerDevice)
{
	for (CustomShaders::iterator it = customShaders.begin(); it != customShaders.end(); ++it) {
		if (it->second.mHackerDevice == mHackerDevice) {
			HRESULT hr;
			if (it->second.vs_bytecode) {
				hr = it->second.mHackerDevice->GetD3D9Device()->CreateVertexShader((DWORD*)it->second.vs_bytecode->GetBufferPointer(), &it->second.vs);
				if (FAILED(hr))
					LogDebug("Recreate Command List Vertex Shader failed:= %d\n", hr);
				if (it->second.vs && it->second.mHackerDevice)
					it->second.mHackerDevice->migotoResourceCount++;
			}
			if (it->second.ps_bytecode) {
				hr = it->second.mHackerDevice->GetD3D9Device()->CreatePixelShader((DWORD*)it->second.ps_bytecode->GetBufferPointer(), &it->second.ps);
				if (FAILED(hr))
					LogDebug("Recreate Command List Pixel Shader failed:= %d\n", hr);
				if (it->second.ps && it->second.mHackerDevice)
					it->second.mHackerDevice->migotoResourceCount++;
			}
		}
	}
}

static void ProcessRTSize(CommandListState *state)
{
	D3DSURFACE_DESC surface_desc;
	IDirect3DSurface9 *surface= NULL;
	if (state->rt_width != -1)
		return;

	state->mOrigDevice->GetRenderTarget(0, &surface);

	if (!surface)
		return;

	surface->GetDesc(&surface_desc);

	state->rt_width = (float)surface_desc.Width;
	state->rt_height = (float)surface_desc.Height;

	surface->Release();
}
static void UpdateScissorInfo(CommandListState *state)
{
	if (state->scissor_valid)
		return;

	state->mOrigDevice->GetScissorRect(&state->scissor_rect);

	state->scissor_valid = true;
}

float CommandListOperandFloat::process_texture_filter(CommandListState *state)
{
	TextureOverrideMatches matches;
	TextureOverrideMatches::reverse_iterator rit;
	bool resource_found;

	texture_filter_target.FindTextureOverrides(state, &resource_found, &matches);

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
	if (matches.empty())
		return 0;

	// If there are multiple matches, we want the filter_index with the
	// highest priority, which will be the last in the list that has a
	// filter index. In the future we may also want a namespaced version of
	// this (and checktextureoverride) to limit the check to sections
	// appearing in the same namespace or with a given prefix (but we don't
	// want to do string processing on the namespace here - the candidates
	// should already be narrowed down during ini parsing):
	for (rit = matches.rbegin(); rit != matches.rend(); rit++) {
		if ((*rit)->filter_index != FLT_MAX)
			return (*rit)->filter_index;
	}

	// No match had a filter_index, but there was at least one match:
	return 1.0;
}
//static UINT depth = 0;
float CommandListOperandFloat::staging_op_val(CommandListState * state)
{
	void *mapping = NULL;
	HRESULT hr;
	float tmp;
	float ret = numeric_limits<float>::quiet_NaN();

	if (staging_op.staging) {
		hr = staging_op.map(state, &mapping);
		if (FAILED(hr)) {
			if (hr == D3DERR_WASSTILLDRAWING)
				COMMAND_LIST_LOG(state, "  Transfer in progress...\n");
			else
				COMMAND_LIST_LOG(state, "  Unknown error: 0x%x\n", hr);
			return ret;
		}

		// FIXME: Check if resource is at least 4 bytes (maybe we can
		// use RowPitch, but MSDN contradicts itself so I'm not sure.
		// Otherwise we can refer to the resource description)
		//tmp = ((float*)mapping.pData)[0];
		tmp = ((float*)mapping)[0];
		/*if (depth >= 20) {
			LogOverlay(LOG_WARNING, "Depth = %f\n", tmp);
			depth = 0;
		}
		++depth;*/
		staging_op.unmap(state);

		if (isnan(tmp)) {
			COMMAND_LIST_LOG(state, "  Disregarding NAN\n");
		}
		else {
			ret = tmp;
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

void CommandList::clear()
{
	commands.clear();
	static_vars.clear();
}

CommandListState::CommandListState() :
	mHackerDevice(NULL),
	mOrigDevice(NULL),
	rt_width(-1),
	rt_height(-1),
	call_info(NULL),
	this_target(NULL),
	resource(NULL),
	post(false),
	update_params(false),
	recursion(0),
	extra_indent(0),
	aborted(false),
	scissor_valid(false),
	m_activeStereoTextureStages(NULL),
	resolved_depth_replacement(false),
	copied_depth_replacement(false)
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
	m_activeStereoTextureStages.clear();

}

static void UpdateWindowInfo(CommandListState *state)
{
	if (state->window_rect.right)
		return;

	if (G->hWnd())
		CursorUpscalingBypass_GetClientRect(G->hWnd(), &state->window_rect);
	else
		LogDebug("UpdateWindowInfo: No hWnd\n");
}
static void UpdateCursorInfo(CommandListState *state)
{
	if (state->cursor_info.cbSize)
		return;

	state->cursor_info.cbSize = sizeof(CURSORINFO);
	CursorUpscalingBypass_GetCursorInfo(&state->cursor_info);
	std::memcpy(&state->cursor_window_coords, &state->cursor_info.ptScreenPos, sizeof(POINT));

	if (G->hWnd())
		CursorUpscalingBypass_ScreenToClient(G->hWnd(), &state->cursor_window_coords);
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
	HBITMAP hbitmap,
	CommandListState *state,
	IDirect3DTexture9 **tex)
{
	BITMAPINFOHEADER bmp_info;
	D3DLOCKED_RECT data;

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
	data.Pitch = ((bitmap_obj->bmWidth * bmp_info.biBitCount + 31) / 32) * 4;

	data.pBits = new char[data.Pitch * bitmap_obj->bmHeight];

	if (!GetDIBits(dc, hbitmap, 0, bmp_info.biHeight,
		(LPVOID)data.pBits, (BITMAPINFO*)&bmp_info, DIB_RGB_COLORS)) {
		LogInfo("Software Mouse: GetDIBits() failed\n");
		goto err_free;
	}
	hr = state->mOrigDevice->CreateTexture(bitmap_obj->bmWidth, bitmap_obj->bmHeight, 1, D3DUSAGE_DYNAMIC, D3DFMT_A8R8G8B8, D3DPOOL_DEFAULT, tex, NULL);
	if (FAILED(hr)) {
		LogInfo("Software Mouse: CreateTexture Failed: 0x%x\n", hr);
		goto err_free;
	}
	D3DLOCKED_RECT rect;
	hr = (*tex)->LockRect(0, &rect, NULL, D3DLOCK_DISCARD);
	if (FAILED(hr))
		goto err_release_tex;
	unsigned char* dest = static_cast<unsigned char*>(rect.pBits);
	std::memcpy(dest, data.pBits, sizeof(unsigned char) * bitmap_obj->bmWidth * bitmap_obj->bmHeight * 4);
	hr = (*tex)->UnlockRect(0);
	if (FAILED(hr))
		goto err_release_tex;
	delete[] data.pBits;

	return;
err_release_tex:
	(*tex)->Release();
	*tex = NULL;
err_free:
	delete[] data.pBits;
}

static void CreateTextureFromBitmap(HDC dc, HBITMAP hbitmap, CommandListState *state,
	IDirect3DTexture9 **tex)
{
	BITMAP bitmap_obj;

	if (!GetObject(hbitmap, sizeof(BITMAP), &bitmap_obj)) {
		LogInfo("Software Mouse: GetObject() failed\n");
		return;
	}

	_CreateTextureFromBitmap(dc, &bitmap_obj, hbitmap, state, tex);
}

static void CreateTextureFromAnimatedCursor(
	HDC dc,
	HCURSOR cursor,
	UINT flags,
	HBITMAP static_bitmap,
	CommandListState *state,
	IDirect3DTexture9 **tex
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
		_CreateTextureFromBitmap(dc, &bitmap_obj, static_bitmap, state, tex);
		goto out_delete_ani_bitmap;
	}

	_CreateTextureFromBitmap(dc, &bitmap_obj, ani_bitmap, state, tex);

out_delete_ani_bitmap:
	DeleteObject(ani_bitmap);
out_delete_mem_dc:
	DeleteDC(dc_mem);
}

static void UpdateCursorResources(CommandListState *state)
{
	HDC dc;
	Profiling::State profiling_state;

	//I was having performance issues when trying to copy the cursor texture every frame. After trying
	//a variety of flags to cirucmvent this, I am currently trying to only update the software cursor when
	//it changes. Naturally this won't work with animated cursors (although possibly software already), and
	//perhaps a better approach would be to use a resource pool to capture all possible states of the cursor instead.

	if (!G->CURSOR_UPDATE_REQUIRED() && (state->mHackerDevice->cursor_mask_tex || state->mHackerDevice->cursor_color_tex))
		return;
	G->SET_CURSOR_UPDATE_REQUIRED(0);
	if (state->mHackerDevice->cursor_mask_tex) {
		state->mHackerDevice->cursor_mask_tex->Release();
		state->mHackerDevice->cursor_mask_tex = NULL;
	}
	if (state->mHackerDevice->cursor_color_tex) {
		state->mHackerDevice->cursor_color_tex->Release();
		state->mHackerDevice->cursor_color_tex = NULL;
	}

	if (Profiling::mode == Profiling::Mode::SUMMARY)
		Profiling::start(&profiling_state);

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
			&state->mHackerDevice->cursor_color_tex);

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
				&state->mHackerDevice->cursor_mask_tex);
		}
	}
	else if (state->cursor_info_ex.hbmMask) {
		// Black and white cursor, which means the hbmMask bitmap is
		// double height and won't work with the animated cursor
		// routines, so just turn the bitmap into a texture directly:
		CreateTextureFromBitmap(
			dc,
			state->cursor_info_ex.hbmMask,
			state,
			&state->mHackerDevice->cursor_mask_tex);
	}

	ReleaseDC(NULL, dc);
	if (Profiling::mode == Profiling::Mode::SUMMARY)
		Profiling::end(&profiling_state, &Profiling::cursor_overhead);
}
static float ShaderFloat(D3D9Wrapper::IDirect3DDevice9 *device, wchar_t shader_type, int idx, float DirectX::XMFLOAT4::*component) {
	float pConstantsF[4];
	switch (shader_type) {
	case L'v':
		device->GetD3D9Device()->GetVertexShaderConstantF(idx, pConstantsF, 1);
		break;
	case L'p':
		device->GetD3D9Device()->GetPixelShaderConstantF(idx, pConstantsF, 1);
	}
	if (component == &DirectX::XMFLOAT4::x) {
		return pConstantsF[0];
	}
	else if (component == &DirectX::XMFLOAT4::y) {
		return pConstantsF[1];
	}
	else if (component == &DirectX::XMFLOAT4::z) {
		return pConstantsF[2];
	}
	else if (component == &DirectX::XMFLOAT4::w) {
		return pConstantsF[3];
	}
	return 0.0f;
}
static int ShaderInt(D3D9Wrapper::IDirect3DDevice9 *device, wchar_t shader_type, int idx, float DirectX::XMFLOAT4::*component) {
	int pConstantsI[4];
	switch (shader_type) {
	case L'v':
		device->GetD3D9Device()->GetVertexShaderConstantI(idx, pConstantsI, 1);
	case L'p':
		device->GetD3D9Device()->GetPixelShaderConstantI(idx, pConstantsI, 1);
	}
	if (component == &DirectX::XMFLOAT4::x) {
		return pConstantsI[0];
	}
	else if (component == &DirectX::XMFLOAT4::y) {
		return pConstantsI[1];
	}
	else if (component == &DirectX::XMFLOAT4::z) {
		return pConstantsI[2];
	}
	else if (component == &DirectX::XMFLOAT4::w) {
		return pConstantsI[3];
	}
	return 0;
}
static BOOL ShaderBool(D3D9Wrapper::IDirect3DDevice9 *device, wchar_t shader_type, int idx, float DirectX::XMFLOAT4::*component) {
	BOOL pConstantsB[4];
	switch (shader_type) {
	case L'v':
		device->GetD3D9Device()->GetVertexShaderConstantB(idx, pConstantsB, 1);
	case L'p':
		device->GetD3D9Device()->GetPixelShaderConstantB(idx, pConstantsB, 1);
	}
	if (component == &DirectX::XMFLOAT4::x) {
		return pConstantsB[0];
	}
	else if (component == &DirectX::XMFLOAT4::y) {
		return pConstantsB[1];
	}
	else if (component == &DirectX::XMFLOAT4::z) {
		return pConstantsB[2];
	}
	else if (component == &DirectX::XMFLOAT4::w) {
		return pConstantsB[3];
	}
	return false;
}
void GetSurfaceWidth(CommandListState * state, ::IDirect3DSurface9 *pSurface, float *val)
{
	if (pSurface == nullptr)
		return;
	D3DSURFACE_DESC desc;
	pSurface->GetDesc(&desc);
	pSurface->Release();
	*val = (float)desc.Width;
}

void GetSurfaceHeight(CommandListState * state, ::IDirect3DSurface9 *pSurface, float *val)
{
	if (pSurface == nullptr)
		return;
	D3DSURFACE_DESC desc;
	pSurface->GetDesc(&desc);
	pSurface->Release();
	*val = (float)desc.Height;

}
void GetTextureWidth(::IDirect3DBaseTexture9 *pTexture, float *val)
{
	if (pTexture == nullptr)
		return;
	D3DSURFACE_DESC surDesc;
	D3DVOLUME_DESC volDesc;
	switch (pTexture->GetType()) {
	case D3DRTYPE_TEXTURE:
		((IDirect3DTexture9*)pTexture)->GetLevelDesc(0, &surDesc);
		*val = (float)surDesc.Width;
		return;
	case D3DRTYPE_CUBETEXTURE:
		((IDirect3DCubeTexture9*)pTexture)->GetLevelDesc(0, &surDesc);
		*val = (float)surDesc.Width;
		return;
	case D3DRTYPE_VOLUMETEXTURE:
		((IDirect3DVolumeTexture9*)pTexture)->GetLevelDesc(0, &volDesc);
		*val = (float)volDesc.Width;
		return;
	default:
		val = nullptr;
		return;
	}

}

void GetTextureHeight(::IDirect3DBaseTexture9 *pTexture, float *val)
{
	if (pTexture == nullptr)
		return;
	D3DSURFACE_DESC surDesc;
	D3DVOLUME_DESC volDesc;

	switch (pTexture->GetType()) {
	case D3DRTYPE_TEXTURE:
		((IDirect3DTexture9*)pTexture)->GetLevelDesc(0, &surDesc);
		*val = (float)surDesc.Height;
		return;
	case D3DRTYPE_CUBETEXTURE:
		((IDirect3DCubeTexture9*)pTexture)->GetLevelDesc(0, &surDesc);
		*val = (float)surDesc.Height;
		return;
	case D3DRTYPE_VOLUMETEXTURE:
		((IDirect3DVolumeTexture9*)pTexture)->GetLevelDesc(0, &volDesc);
		*val = (float)volDesc.Height;
		return;
	default:
		val = nullptr;
		return;
	}
}
float CommandListOperandFloat::evaluate(CommandListState *state, D3D9Wrapper::IDirect3DDevice9 *device)
{
	NvU8 _stereo = false;
	bool stereo = false;
	float fret;
	D3DVIEWPORT9 vport;
	IDirect3DSurface9 *surface = NULL;
	IDirect3DBaseTexture9 *texture = NULL;

	if (state)
		device = state->mHackerDevice;
	else if (!device) {
		LogOverlay(LOG_DIRE, "BUG: CommandListOperand::evaluate called with neither state nor device\n");
		return 0;
	}

	// XXX: If updating this list, be sure to also update
	// XXX: operand_allowed_in_context()
	switch (type) {
	case ParamOverrideType::VALUE:
		return val;
	case ParamOverrideType::ISNAN:
		return numeric_limits<float>::quiet_NaN();
	case ParamOverrideType::INI_PARAM:
		return G->IniConstants[idx].*component;
	case ParamOverrideType::VARIABLE:
		return *var_ftarget;
	case ParamOverrideType::RES_WIDTH:
		return (float)G->mResolutionInfo.width;
	case ParamOverrideType::RES_HEIGHT:
		return (float)G->mResolutionInfo.height;
	case ParamOverrideType::TIME:
		return (float)(GetTickCount() - G->ticks_at_launch) / 1000.0f;
	case ParamOverrideType::RAW_SEPARATION:
		if (state)
			GetSeparation(device, state->cachedStereoValues, &fret);
		else
			Profiling::NvAPI_Stereo_GetSeparation(device->mStereoHandle, &fret);
		return fret;
	case ParamOverrideType::CONVERGENCE:
		if (state)
			GetConvergence(device, state->cachedStereoValues, &fret);
		else
			Profiling::NvAPI_Stereo_GetConvergence(device->mStereoHandle, &fret);
		return fret;
	case ParamOverrideType::EYE_SEPARATION:
		if (state)
			GetEyeSeparation(device, state->cachedStereoValues, &fret);
		else
			Profiling::NvAPI_Stereo_GetEyeSeparation(device->mStereoHandle, &fret);
		return fret;
	case ParamOverrideType::STEREO_ACTIVE:
		if (state) {
			GetStereoActive(device, state->cachedStereoValues, &stereo);
			return stereo;
		}
		else {
			Profiling::NvAPI_Stereo_IsActivated(device->mStereoHandle, &_stereo);
			return !!_stereo;
		}
	case ParamOverrideType::SLI:
		return device->sli_enabled();
	case ParamOverrideType::HUNTING:
		return (float)G->hunting;
	case ParamOverrideType::FRAME_ANALYSIS:
		return G->analyse_frame;
	case ParamOverrideType::BACK_BUFFER_WIDTH:
		if (G->SCREEN_UPSCALING > 0) {
			return (float)G->SCREEN_WIDTH;
		}
		else {
			return (float)G->GAME_INTERNAL_WIDTH();
		}
	case ParamOverrideType::BACK_BUFFER_HEIGHT:
		if (G->SCREEN_UPSCALING > 0) {
			return (float)G->SCREEN_HEIGHT;
		}
		else {
			return (float)G->GAME_INTERNAL_HEIGHT();
		}
	case ParamOverrideType::PIXEL_SHADER_TEXTURE_WIDTH:
		device->GetD3D9Device()->GetTexture(idx, &texture);
		GetTextureWidth(texture, &fret);
		return fret;
	case ParamOverrideType::PIXEL_SHADER_TEXTURE_HEIGHT:
		device->GetD3D9Device()->GetTexture(idx, &texture);
		GetTextureHeight(texture, &fret);
		return fret;
	case ParamOverrideType::VERTEX_SHADER_TEXTURE_WIDTH:
		device->GetD3D9Device()->GetTexture(((D3DDMAPSAMPLER + 1) + idx), &texture);
		GetTextureWidth(texture, &fret);
		return fret;
	case ParamOverrideType::VERTEX_SHADER_TEXTURE_HEIGHT:
		device->GetD3D9Device()->GetTexture(((D3DDMAPSAMPLER + 1) + idx), &texture);
		GetTextureHeight(texture, &fret);
		return fret;
	case ParamOverrideType::CURSOR_COLOR_WIDTH:
		GetTextureWidth(device->cursor_color_tex, &fret);
		return fret;
	case ParamOverrideType::CURSOR_COLOR_HEIGHT:
		GetTextureHeight(device->cursor_color_tex, &fret);
		return fret;
	case ParamOverrideType::CURSOR_MASK_WIDTH:
		GetTextureWidth(device->cursor_mask_tex, &fret);
		return fret;
	case ParamOverrideType::CURSOR_MASK_HEIGHT:
		GetTextureHeight(device->cursor_mask_tex, &fret);
		return fret;
	case ParamOverrideType::VIEWPORT_WIDTH:
		device->GetD3D9Device()->GetViewport(&vport);
		return (float)vport.Width;
	case ParamOverrideType::VIEWPORT_HEIGHT:
		device->GetD3D9Device()->GetViewport(&vport);
		return (float)vport.Height;
	case ParamOverrideType::VERTEX_SHADER_FLOAT:
		return ShaderFloat(device, L'v', idx, component);
	case ParamOverrideType::VERTEX_SHADER_INT:
		return (float)ShaderInt(device, L'v', idx, component);
	case ParamOverrideType::VERTEX_SHADER_BOOL:
		return (float)ShaderBool(device, L'v', idx, component);
	case ParamOverrideType::PIXEL_SHADER_FLOAT:
		return ShaderFloat(device, L'p', idx, component);
	case ParamOverrideType::PIXEL_SHADER_INT:
		return (float)ShaderInt(device, L'p', idx, component);
	case ParamOverrideType::PIXEL_SHADER_BOOL:
		return (float)ShaderBool(device, L'p', idx, component);
	case ParamOverrideType::DEPTH_BUFFER_CMPFUNC_GREATER:
		if (!device->depthstencil_replacement)
			return 0.0f;
		else
			if (device->depthstencil_replacement->depthSourceInfo.last_cmp_func == ::D3DCMP_GREATER)
				return 1.0f;
			else
				return 0.0f;
	case ParamOverrideType::DEPTH_BUFFER_CMPFUNC_GREATEREQUAL:
		if (!device->depthstencil_replacement)
			return 0.0f;
		else
			if (device->depthstencil_replacement->depthSourceInfo.last_cmp_func == ::D3DCMP_GREATEREQUAL)
				return 1.0f;
			else
				return 0.0f;
	case ParamOverrideType::INTERNAL_FUNCTION_FLOAT:
		return internalFunctionFloats[iff];
	case ParamOverrideType::STEREO_PARAMS_UPDATED:
		return device->stereo_params_updated_this_frame;
		// XXX: If updating this list, be sure to also update
		// XXX: operand_allowed_in_context()
	}

	if (!state) {
		// FIXME: Some of these only use the state object for cache,
		// and could still be evaluated if we forgo the cache
		LogOverlay(LOG_WARNING, "BUG: Operand type %i cannot be evaluated outside of a command list\n", type);
		return 0;
	}

	switch (type) {
	case ParamOverrideType::RT_WIDTH:
		if (idx == 0)
			ProcessRTSize(state);
		else {
			device->GetD3D9Device()->GetRenderTarget(idx, &surface);
			GetSurfaceWidth(state, surface, &fret);
			return fret;
		}
		return state->rt_width;
	case ParamOverrideType::RT_HEIGHT:
		if (idx == 0)
			ProcessRTSize(state);
		else {
			device->GetD3D9Device()->GetRenderTarget(idx, &surface);
			GetSurfaceHeight(state, surface, &fret);
			return fret;
		}
		return state->rt_height;
	case ParamOverrideType::WINDOW_WIDTH:
		UpdateWindowInfo(state);
		return (float)state->window_rect.right;
	case ParamOverrideType::WINDOW_HEIGHT:
		UpdateWindowInfo(state);
		return (float)state->window_rect.bottom;
	case ParamOverrideType::TEXTURE:
		return process_texture_filter(state);
	case ParamOverrideType::VERTEX_COUNT:
		if (state->call_info)
			return (float)DrawPrimitiveCountToVerticesCount(state->call_info->PrimitiveCount, state->call_info->primitive_type);
		return 0;
	case ParamOverrideType::INDEX_COUNT:
		if (state->call_info)
			return (float)DrawPrimitiveCountToVerticesCount(state->call_info->PrimitiveCount, state->call_info->primitive_type);
		return 0;
	case ParamOverrideType::CURSOR_VISIBLE:
		UpdateCursorInfo(state);
		return !!(state->cursor_info.flags & CURSOR_SHOWING);
	case ParamOverrideType::CURSOR_SCREEN_X:
		UpdateCursorInfo(state);
		return (float)state->cursor_info.ptScreenPos.x;
	case ParamOverrideType::CURSOR_SCREEN_Y:
		UpdateCursorInfo(state);
		return (float)state->cursor_info.ptScreenPos.y;
	case ParamOverrideType::CURSOR_WINDOW_X:
		UpdateCursorInfo(state);
		return (float)state->cursor_window_coords.x;
	case ParamOverrideType::CURSOR_WINDOW_Y:
		UpdateCursorInfo(state);
		return (float)state->cursor_window_coords.y;
	case ParamOverrideType::CURSOR_X:
		UpdateCursorInfo(state);
		UpdateWindowInfo(state);
		return (float)state->cursor_window_coords.x / (float)state->window_rect.right;
	case ParamOverrideType::CURSOR_Y:
		UpdateCursorInfo(state);
		UpdateWindowInfo(state);
		return (float)state->cursor_window_coords.y / (float)state->window_rect.bottom;
	case ParamOverrideType::CURSOR_HOTSPOT_X:
		UpdateCursorInfoEx(state);
		return (float)state->cursor_info_ex.xHotspot;
	case ParamOverrideType::CURSOR_HOTSPOT_Y:
		UpdateCursorInfoEx(state);
		return (float)state->cursor_info_ex.yHotspot;
	case ParamOverrideType::SCISSOR_LEFT:
		UpdateScissorInfo(state);
		return (float)state->scissor_rect.left;
	case ParamOverrideType::SCISSOR_TOP:
		UpdateScissorInfo(state);
		return (float)state->scissor_rect.top;
	case ParamOverrideType::SCISSOR_RIGHT:
		UpdateScissorInfo(state);
		return (float)state->scissor_rect.right;
	case ParamOverrideType::SCISSOR_BOTTOM:
		UpdateScissorInfo(state);
		return (float)state->scissor_rect.bottom;
	case ParamOverrideType::STAGING_OP:
		return staging_op_val(state);
	}

	LogOverlay(LOG_DIRE, "BUG: Unhandled operand type %i\n", type);
	return 0;
}

bool CommandListOperandFloat::static_evaluate(float *ret, D3D9Wrapper::IDirect3DDevice9 *device)
{
	NvU8 stereo = false;

	switch (type) {
	case ParamOverrideType::VALUE:
		*ret = val;
		return true;
	case ParamOverrideType::ISNAN:
		*ret = numeric_limits<float>::quiet_NaN();
		return true;
	case ParamOverrideType::RAW_SEPARATION:
	case ParamOverrideType::CONVERGENCE:
	case ParamOverrideType::EYE_SEPARATION:
	case ParamOverrideType::STEREO_ACTIVE:
		NvAPIOverride();
		Profiling::NvAPI_Stereo_IsEnabled(&stereo);
		if (!stereo) {
			*ret = 0.0;
			return true;
		}
		break;
	case ParamOverrideType::SLI:
		if (device) {
			*ret = device->sli_enabled();
			return true;
		}
		break;
	case ParamOverrideType::HUNTING:
	case ParamOverrideType::FRAME_ANALYSIS:
		if (G->hunting == HUNTING_MODE_DISABLED) {
			*ret = 0;
			return true;
		}
		break;
	}
	return false;
}

bool CommandListOperandFloat::optimise(D3D9Wrapper::IDirect3DDevice9 *device, std::shared_ptr<CommandListEvaluatable> *replacement)
{
	if (type == ParamOverrideType::VALUE)
		return false;

	if (!static_evaluate(&val, device))
		return false;

	LogInfo("Statically evaluated %S as %f\n",
		lookup_enum_name(ParamOverrideTypeNames, type), val);

	type = ParamOverrideType::VALUE;
	return true;
}

static const wchar_t *operator_tokens[] = {
	// Three character tokens first:
	L"===", L"!==",
	// Two character tokens next:
	L"==", L"!=", L"//", L"<=", L">=", L"&&", L"||", L"**",
	// Single character tokens last:
	L"(", L")", L"!", L"*", L"/", L"%", L"+", L"-", L"<", L">",
};
static const wchar_t *matrix_operator_tokens[] = {
	// Single character tokens last:
	L"(", L")", L"*", L"+", L"-", L"i", L"t",
};
class CommandListSyntaxError : public exception
{
public:
	wstring msg;
	size_t pos;

	CommandListSyntaxError(wstring msg, size_t pos) :
		msg(msg), pos(pos)
	{}
};

static void tokenise(const wstring *expression, CommandListSyntaxTree<CommandListEvaluatable> *tree, const wstring *ini_namespace, CommandList *command_list)//CommandListScope *scope)
{
	wstring remain = *expression;
	ResourceCopyTarget texture_filter_target;
	shared_ptr<CommandListOperandFloat> operand;
	wstring token;
	size_t pos = 0;
	int ipos = 0;
	size_t friendly_pos = 0;
	float fval;
	int ret;
	int i;
	bool last_was_operand = false;

	LogDebug("    Tokenising \"%S\"\n", expression->c_str());

	while (true) {
	next_token:
		// Skip whitespace:
		pos = remain.find_first_not_of(L" \t", pos);
		if (pos == wstring::npos)
			return;
		remain = remain.substr(pos);
		friendly_pos += pos;

		// Operators:
		for (i = 0; i < ARRAYSIZE(operator_tokens); i++) {
			if (!remain.compare(0, wcslen(operator_tokens[i]), operator_tokens[i])) {
				pos = wcslen(operator_tokens[i]);
				tree->tokens.emplace_back(make_shared<CommandListOperatorToken>(friendly_pos, remain.substr(0, pos)));
				LogDebug("      Operator: \"%S\"\n", tree->tokens.back()->token.c_str());
				last_was_operand = false;
				goto next_token; // continue would continue wrong loop
			}
		}

		// Texture Filtering / Resource Slots:
		// - Many of these slots include a hyphen character, which
		//   conflicts with the subtraction/negation operators,
		//   potentially making something like "x = ps-t0" ambiguous as
		//   to whether it is referring to pixel shader texture slot 0,
		//   or subtracting "t0" from "ps", but in practice this should
		//   be generally be fine since we don't have anything called
		//   "ps", "t0" or similar, and if we did simply adding
		//   whitespace around the subtraction would disambiguate it.
		// - The characters we check for here preclude some arbitrary
		//   custom Resource names, including namespaced resources, but
		//   that's ok since this is only for texture filtering, which
		//   doesn't work if custom resources are checked. If we need
		//   to match these for some other reason, we could add \ and .
		//   to this list, which will cover most namespaced resources.
		pos = remain.find_first_not_of(L"abcdefghijklmnopqrstuvwxyz_-0123456789");
		if (pos) {
			token = remain.substr(0, pos);
			ret = texture_filter_target.ParseTarget(token.c_str(), true, ini_namespace);
			if (ret) {
				operand = make_shared<CommandListOperandFloat>(friendly_pos, token);
				if (operand->parse(&token, ini_namespace, command_list)) {
					tree->tokens.emplace_back(std::move(operand));
					LogDebug("      Resource Slot: \"%S\"\n", tree->tokens.back()->token.c_str());
					if (last_was_operand)
						throw CommandListSyntaxError(L"Unexpected identifier", friendly_pos);
					last_was_operand = true;
					continue;
				}
				else {
					LogOverlay(LOG_DIRE, "BUG: Token parsed as resource slot, but not as operand: \"%S\"\n", token.c_str());
					throw CommandListSyntaxError(L"BUG", friendly_pos);
				}
			}
		}

		// Identifiers:
		// - Parse this before floats to make sure that the special
		//   cases "inf" and "nan" are identifiers by themselves, not
		//   the start of some other identifier. Only applies to
		//   vs2015+ as older toolchains lack parsing for these.
		// - Identifiers cannot start with a number
		// - Variable identifiers start with a $, and these may be
		//   namespaced, so we allow backslash and . as well
		//   TODO: Be more specific with namespaces to allow exactly
		//   the set of actual namespaces. Would allow for namespaces
		//   to have spaces or other unusual characters while freeing
		//   up . \ and $ for potential use as operators in the future.
		if (remain[0] < '0' || remain[0] > '9') {
			pos = remain.find_first_not_of(L"abcdefghijklmnopqrstuvwxyz_0123456789$\\.[]");
			if (pos) {
				token = remain.substr(0, pos);
				operand = make_shared<CommandListOperandFloat>(friendly_pos, token);
				if (operand->parse(&token, ini_namespace, command_list)) {
					tree->tokens.emplace_back(std::move(operand));
					LogDebug("      Identifier: \"%S\"\n", tree->tokens.back()->token.c_str());
					if (last_was_operand)
						throw CommandListSyntaxError(L"Unexpected identifier", friendly_pos);
					last_was_operand = true;
					continue;
				}
				throw CommandListSyntaxError(L"Unrecognised identifier: " + token, friendly_pos);
			}
		}

		// Floats:
		// - Must tokenise subtraction operation first
		//   - Static optimisation will merge unary negation
		// - Identifier match will catch "nan" and "inf" special cases
		//   if the toolchain supports them
		ret = swscanf_s(remain.c_str(), L"%f%n", &fval, &ipos);
		if (ret != 0 && ret != EOF) {
			// VS2013 Issue: size_t z/I modifiers do not work with %n
			// We could make pos an int and cast it everywhere it is used
			// as a size_t, but this way highlights the toolchain issue.
			pos = ipos;

			token = remain.substr(0, ipos);
			operand = make_shared<CommandListOperandFloat>(friendly_pos, token);
			if (operand->parse(&token, ini_namespace, command_list)) {
				tree->tokens.emplace_back(std::move(operand));
				LogDebug("      Float: \"%S\"\n", tree->tokens.back()->token.c_str());
				if (last_was_operand)
					throw CommandListSyntaxError(L"Unexpected identifier", friendly_pos);
				last_was_operand = true;
				continue;
			}
			else {
				LogOverlay(LOG_DIRE, "BUG: Token parsed as float, but not as operand: \"%S\"\n", token.c_str());
				throw CommandListSyntaxError(L"BUG", friendly_pos);
			}
		}

		throw CommandListSyntaxError(L"Parse error", friendly_pos);
	}
}
int find_local_matrix(const wstring &name, CommandListScopeMatrices *scope, CommandListMatrix **matrix, UINT *startSlot = NULL, UINT *numSlots = NULL)
{
	CommandListScopeMatrices::iterator it;
	int ret, len1, slot, num;
	wchar_t variable[MAX_PATH];
	bool parsed = false;
	if (!scope)
		return false;
	if (name.length() < 2 || name[0] != L'$')
		return false;

	if (startSlot)
		*startSlot = 0;
	if (numSlots)
		*numSlots = 16;
	ret = swscanf_s(name.c_str(), L"%[^[][%u]%u%n", variable, _countof(variable), &slot, &num, &len1);
	if (ret != 0 && ret != EOF && len1 == name.length()) {
		parsed = true;
		if (startSlot) {
			if (slot < 16)
				*startSlot = slot;
			if (numSlots) {
				if ((*startSlot + num) < 16)
					*numSlots = num;
				else
					*numSlots = 16 - *startSlot;
			}
		}
		else if (numSlots && (num < 16))
			*numSlots = num;
	}
	else {
		ret = swscanf_s(name.c_str(), L"%[^[][%u]%n", variable, _countof(variable), &slot, &len1);
		if (ret != 0 && ret != EOF && len1 == name.length()) {
			parsed = true;
			if (startSlot) {
				if (slot < 16)
					*startSlot = slot;
			}
			if (numSlots)
				*numSlots = 1;
		}
		else
		{
			ret = swscanf_s(name.c_str(), L"%ls%n", variable, _countof(variable), &len1);
			if (ret != 0 && ret != EOF && len1 == name.length())
				parsed = true;
		}
	}
	if (parsed){
		for (it = scope->begin(); it != scope->end(); it++) {
			auto match = it->find(variable);
			if (match != it->end()) {
				*matrix = match->second;
				return true;
			}
		}
	}
	return false;
}
bool parse_command_list_matrix_name(const wstring &name, const wstring *ini_namespace, CommandListMatrix **target, UINT *startSlot = NULL, UINT *numSlots = NULL)
{
	CommandListMatrices::iterator matrix = command_list_global_matrices.end();
	int ret, len1, slot, num;
	wchar_t variable[MAX_PATH];
	bool parsed = false;

	if (name.length() < 2 || name[0] != L'$')
		return false;
	// We need value in lower case so our keys will be consistent in the
	// unordered_map. ParseCommandList will have already done this, but the
	// Key/Preset parsing code will not have, and rather than require it to
	// we do it here:
	if (startSlot)
		*startSlot = 0;
	if (numSlots)
		*numSlots = 16;
	wstring low_name(name);
	std::transform(low_name.begin(), low_name.end(), low_name.begin(), ::towlower);
	ret = swscanf_s(low_name.c_str(), L"%[^[][%u]%u%n", variable, _countof(variable), &slot, &num, &len1);
	if (ret != 0 && ret != EOF && len1 == low_name.length()) {
		parsed = true;
		if (startSlot) {
			if (slot < 16)
				*startSlot = slot;
			if (numSlots) {
				if ((*startSlot + num) < 16)
					*numSlots = num;
				else
					*numSlots = 16 - *startSlot;
			}
		}
		else if (numSlots && (num < 16))
			*numSlots = num;

	}
	else {
		ret = swscanf_s(name.c_str(), L"%[^[][%u]%n", variable, _countof(variable), &slot, &len1);
		if (ret != 0 && ret != EOF && len1 == name.length()) {
			parsed = true;
			if (startSlot) {
				if (slot < 16)
					*startSlot = slot;
			}
			if (numSlots)
				*numSlots = 1;
		}
		else
		{
			ret = swscanf_s(name.c_str(), L"%ls%n", variable, _countof(variable), &len1);
			if (ret != 0 && ret != EOF && len1 == low_name.length())
				parsed = true;
		}
	}
	if (parsed) {
		matrix = command_list_global_matrices.end();
		if (!ini_namespace->empty())
			matrix = command_list_global_matrices.find(get_namespaced_var_name_lower(variable, ini_namespace));
		if (matrix == command_list_global_matrices.end())
			matrix = command_list_global_matrices.find(variable);
		if (matrix == command_list_global_matrices.end())
			return false;
		*target = &matrix->second;
		return true;
	}
	return false;
}
static void matrix_tokenise(const wstring *expression, CommandListSyntaxTree<CommandListMatrixEvaluatable> *tree, const wstring *ini_namespace, CommandListScopeMatrices *scope)
{
	wstring remain = *expression;
	shared_ptr<CommandListMatrixOperand> operand;
	wstring token;
	size_t pos = 0;
	size_t friendly_pos = 0;
	int i;
	bool last_was_operand = false;

	LogDebug("    Tokenising \"%S\"\n", expression->c_str());

	while (true) {
	next_token:
		// Skip whitespace:
		pos = remain.find_first_not_of(L" \t", pos);
		if (pos == wstring::npos)
			return;
		remain = remain.substr(pos);
		friendly_pos += pos;

		// Operators:
		for (i = 0; i < ARRAYSIZE(matrix_operator_tokens); i++) {
			if (!remain.compare(0, wcslen(matrix_operator_tokens[i]), matrix_operator_tokens[i])) {
				pos = wcslen(matrix_operator_tokens[i]);
				tree->tokens.emplace_back(make_shared<CommandListOperatorToken>(friendly_pos, remain.substr(0, pos)));
				LogDebug("      Operator: \"%S\"\n", tree->tokens.back()->token.c_str());
				last_was_operand = false;
				goto next_token; // continue would continue wrong loop
			}
		}
		if (remain[0] < '0' || remain[0] > '9') {
			pos = remain.find_first_not_of(L"abcdefghijklmnopqrstuvwxyz_0123456789$\\.[]");
			if (pos) {
				token = remain.substr(0, pos);
				CommandListMatrix *matrix;
				if (find_local_matrix(token, scope, &matrix) ||
					parse_command_list_matrix_name(token, ini_namespace, &matrix)) {
					//if (vars.size() != 16)
						//throw CommandListSyntaxError(L"Unexpected non matrix: " + token, friendly_pos);
					operand = make_shared<CommandListMatrixOperand>(matrix, friendly_pos, token);
					tree->tokens.emplace_back(std::move(operand));
					LogDebug("      Identifier: \"%S\"\n", tree->tokens.back()->token.c_str());
					if (last_was_operand)
						throw CommandListSyntaxError(L"Unexpected identifier", friendly_pos);
					last_was_operand = true;
					continue;
				}
				throw CommandListSyntaxError(L"Unrecognised identifier: " + token, friendly_pos);
			}
		}

		throw CommandListSyntaxError(L"Parse error", friendly_pos);
	}
};
template <class Evaluatable>
static void group_parenthesis(CommandListSyntaxTree<Evaluatable> *tree)
{
	CommandListSyntaxTree<Evaluatable>::Tokens::iterator i;
	CommandListSyntaxTree<Evaluatable>::Tokens::reverse_iterator rit;
	CommandListOperatorToken *rbracket, *lbracket;
	std::shared_ptr<CommandListSyntaxTree<Evaluatable>> inner;

	for (i = tree->tokens.begin(); i != tree->tokens.end(); i++) {
		rbracket = dynamic_cast<CommandListOperatorToken*>(i->get());
		if (rbracket && !rbracket->token.compare(L")")) {
			for (rit = std::reverse_iterator<CommandListSyntaxTree<Evaluatable>::Tokens::iterator>(i); rit != tree->tokens.rend(); rit++) {
				lbracket = dynamic_cast<CommandListOperatorToken*>(rit->get());
				if (lbracket && !lbracket->token.compare(L"(")) {
					inner = std::make_shared<CommandListSyntaxTree<Evaluatable>>(lbracket->token_pos);
					// XXX: Double check bounds are right:
					inner->tokens.assign(rit.base(), i);
					i = tree->tokens.erase(rit.base() - 1, i + 1);
					i = tree->tokens.insert(i, std::move(inner));
					goto continue_rbracket_search; // continue would continue wrong loop
				}
			}
			throw CommandListSyntaxError(L"Unmatched )", rbracket->token_pos);
		}
	continue_rbracket_search: false;
	}

	for (i = tree->tokens.begin(); i != tree->tokens.end(); i++) {
		lbracket = dynamic_cast<CommandListOperatorToken*>(i->get());
		if (lbracket && !lbracket->token.compare(L"("))
			throw CommandListSyntaxError(L"Unmatched (", lbracket->token_pos);
	}
}

// Expression operator definitions:
#define DEFINE_OPERATOR(name, operator_pattern, fn) \
class name##T : public CommandListOperatorFloat { \
public: \
	name##T( \
			std::shared_ptr<CommandListToken> lhs, \
			CommandListOperatorToken &t, \
			std::shared_ptr<CommandListToken> rhs \
		) : CommandListOperatorFloat(lhs, t, rhs) \
	{} \
	static const wchar_t* pattern() { return L##operator_pattern; } \
	float evaluate(float lhs, float rhs) override { return (fn); } \
}; \
static CommandListOperatorFactory<name##T, CommandListEvaluatable> name;
#define DEFINE_MATRIX_OPERATOR(matrix_operator_name, operator_pattern, fn) \
class matrix_operator_name##T : public CommandListMatrixOperator { \
public: \
	matrix_operator_name##T( \
			std::shared_ptr<CommandListToken> lhs, \
			CommandListOperatorToken &t, \
			std::shared_ptr<CommandListToken> rhs \
		) : CommandListMatrixOperator(lhs, t, rhs) \
	{} \
	static const wchar_t* pattern() { return L##operator_pattern; } \
	::D3DXMATRIX evaluate(::D3DXMATRIX lhs, ::D3DXMATRIX rhs) override{D3DXMATRIX ret; return (fn); } \
}; \
static CommandListOperatorFactory<matrix_operator_name##T, CommandListMatrixEvaluatable> matrix_operator_name;
DEFINE_MATRIX_OPERATOR(matrix_transpose_operator, "t", (*(::D3DXMatrixTranspose(&D3DXMATRIX(), &rhs))));
DEFINE_MATRIX_OPERATOR(matrix_inverse_operator, "i", (::D3DXMatrixInverse(&ret, NULL, &rhs) ? (ret) : *(D3DXMatrixIdentity(&D3DXMATRIX()))));
DEFINE_MATRIX_OPERATOR(matrix_multiplication_operator, "*", (lhs * rhs));
DEFINE_MATRIX_OPERATOR(matrix_addition_operator, "+", (lhs + rhs));
DEFINE_MATRIX_OPERATOR(matrix_subtraction_operator, "-", (lhs - rhs));
// Highest level of precedence, allows for negative numbers
DEFINE_OPERATOR(unary_not_operator, "!", (!rhs));
DEFINE_OPERATOR(unary_plus_operator, "+", (+rhs));
DEFINE_OPERATOR(unary_negate_operator, "-", (-rhs));

// High level of precedence, right-associative. Lower than unary operators, so
// that 4**-2 works for square root
DEFINE_OPERATOR(exponent_operator, "**", (pow(lhs, rhs)));

DEFINE_OPERATOR(multiplication_operator, "*", (lhs * rhs));
DEFINE_OPERATOR(division_operator, "/", (lhs / rhs));
DEFINE_OPERATOR(floor_division_operator, "//", (floor(lhs / rhs)));
DEFINE_OPERATOR(modulus_operator, "%", (fmod(lhs, rhs)));

DEFINE_OPERATOR(addition_operator, "+", (lhs + rhs));
DEFINE_OPERATOR(subtraction_operator, "-", (lhs - rhs));

DEFINE_OPERATOR(less_operator, "<", (lhs < rhs));
DEFINE_OPERATOR(less_equal_operator, "<=", (lhs <= rhs));
DEFINE_OPERATOR(greater_operator, ">", (lhs > rhs));
DEFINE_OPERATOR(greater_equal_operator, ">=", (lhs >= rhs));

// The triple equals operator tests for binary equivalence - in particular,
// this allows us to test for negative zero, used in texture filtering to
// signify that nothing is bound to a given slot. Negative zero cannot be
// tested for using the regular equals operator, since -0.0 == +0.0. This
// operator could also test for specific cases of NAN (though, without the
// vs2015 toolchain "nan" won't parse as such).
DEFINE_OPERATOR(equality_operator, "==", (lhs == rhs));
DEFINE_OPERATOR(inequality_operator, "!=", (lhs != rhs));
DEFINE_OPERATOR(identical_operator, "===", (*(uint32_t*)&lhs == *(uint32_t*)&rhs));
DEFINE_OPERATOR(not_identical_operator, "!==", (*(uint32_t*)&lhs != *(uint32_t*)&rhs));

DEFINE_OPERATOR(and_operator, "&&", (lhs && rhs));

DEFINE_OPERATOR(or_operator, "||", (lhs || rhs));

// TODO: Ternary if operator
static CommandListOperatorFactoryBase<CommandListMatrixEvaluatable> *matrix_unary_operators[] = {
	&matrix_transpose_operator,
	&matrix_inverse_operator,
};
static CommandListOperatorFactoryBase<CommandListMatrixEvaluatable> *matrix_multi_operators[] = {
	&matrix_multiplication_operator,
};
static CommandListOperatorFactoryBase<CommandListMatrixEvaluatable> *matrix_add_subtract_operators[] = {
	&matrix_addition_operator,
	&matrix_subtraction_operator,
};
static CommandListOperatorFactoryBase<CommandListEvaluatable> *unary_operators[] = {
	&unary_not_operator,
	&unary_negate_operator,
	&unary_plus_operator,
};
static CommandListOperatorFactoryBase<CommandListEvaluatable> *exponent_operators[] = {
	&exponent_operator,
};
static CommandListOperatorFactoryBase<CommandListEvaluatable> *multi_division_operators[] = {
	&multiplication_operator,
	&division_operator,
	&floor_division_operator,
	&modulus_operator,
};
static CommandListOperatorFactoryBase<CommandListEvaluatable> *add_subtract_operators[] = {
	&addition_operator,
	&subtraction_operator,
};
static CommandListOperatorFactoryBase<CommandListEvaluatable> *relational_operators[] = {
	&less_operator,
	&less_equal_operator,
	&greater_operator,
	&greater_equal_operator,
};
static CommandListOperatorFactoryBase<CommandListEvaluatable> *equality_operators[] = {
	&equality_operator,
	&inequality_operator,
	&identical_operator,
	&not_identical_operator,
};
static CommandListOperatorFactoryBase<CommandListEvaluatable> *and_operators[] = {
	&and_operator,
};
static CommandListOperatorFactoryBase<CommandListEvaluatable> *or_operators[] = {
	&or_operator,
};
template <class Evaluatable>
static CommandListSyntaxTree<Evaluatable>::Tokens::iterator transform_operators_token(
	CommandListSyntaxTree<Evaluatable> *tree,
	std::vector<std::shared_ptr<CommandListToken>>::iterator i, // VS2017 could not evaluate "CommandListSyntaxTree<Evaluatable>::Tokens::iterator" here
	CommandListOperatorFactoryBase<Evaluatable> *factories[], int num_factories,
	bool unary)
{
	std::shared_ptr<CommandListOperatorToken> token;
	std::shared_ptr<CommandListOperator<Evaluatable>> op;
	std::shared_ptr<CommandListOperandBase> lhs;
	std::shared_ptr<CommandListOperandBase> rhs;
	int f;

	token = dynamic_pointer_cast<CommandListOperatorToken>(*i);
	if (!token)
		return i;

	for (f = 0; f < num_factories; f++) {
		if (token->token.compare(factories[f]->pattern()))
			continue;

		lhs = nullptr;
		rhs = nullptr;
		if (i > tree->tokens.begin())
			lhs = dynamic_pointer_cast<CommandListOperandBase>(*(i - 1));
		if (i < tree->tokens.end() - 1)
			rhs = dynamic_pointer_cast<CommandListOperandBase>(*(i + 1));

		if (unary) {
			// It is particularly important that we check that the
			// LHS is *not* an operand so the unary +/- operators
			// don't trump the binary addition/subtraction operators:
			if (rhs && !lhs) {
				op = factories[f]->create(nullptr, *token, *(i + 1));
				i = tree->tokens.erase(i, i + 2);
				i = tree->tokens.insert(i, std::move(op));
				break;
			}
		}
		else {
			if (lhs && rhs) {
				op = factories[f]->create(*(i - 1), *token, *(i + 1));
				i = tree->tokens.erase(i - 1, i + 2);
				i = tree->tokens.insert(i, std::move(op));
				break;
			}
		}
	}

	return i;
}

// Transforms operator tokens in the syntax tree into actual operators
template <class Evaluatable>
static void transform_operators_visit(CommandListSyntaxTree<Evaluatable> *tree,
	CommandListOperatorFactoryBase<Evaluatable> *factories[], int num_factories,
	bool right_associative, bool unary)
{
	CommandListSyntaxTree<Evaluatable>::Tokens::iterator i;
	CommandListSyntaxTree<Evaluatable>::Tokens::reverse_iterator rit;

	if (!tree)
		return;

	if (right_associative) {
		if (unary) {
			// Start at the second from the right
			for (rit = tree->tokens.rbegin() + 1; rit != tree->tokens.rend(); rit++) {
				// C++ gotcha: reverse_iterator::base() points to the *next* element
				i = transform_operators_token<Evaluatable>(tree, rit.base() - 1, factories, num_factories, unary);
				rit = std::reverse_iterator<CommandListSyntaxTree<Evaluatable>::Tokens::iterator>(i + 1);
			}
		}
		else {
			for (rit = tree->tokens.rbegin() + 1; rit < tree->tokens.rend() - 1; rit++) {
				// C++ gotcha: reverse_iterator::base() points to the *next* element
				i = transform_operators_token<Evaluatable>(tree, rit.base() - 1, factories, num_factories, unary);
				rit = std::reverse_iterator<CommandListSyntaxTree<Evaluatable>::Tokens::iterator>(i + 1);
			}
		}
	}
	else {
		if (unary) {
			throw CommandListSyntaxError(L"FIXME: Implement left-associative unary operators", 0);
		}
		else {
			// Since this is binary operators, skip the first and last
			// nodes as they must be operands, and this way I don't have to
			// worry about bounds checks.
			for (i = tree->tokens.begin() + 1; i < tree->tokens.end() - 1; i++)
				i = transform_operators_token<Evaluatable>(tree, i, factories, num_factories, unary);
		}
	}
}
template <class Evaluatable>
static void transform_operators_recursive(CommandListWalkable *tree,
	CommandListOperatorFactoryBase<Evaluatable> *factories[], int num_factories,
	bool right_associative, bool unary)
{
	// Depth first to ensure that we have visited all sub-trees before
	// transforming operators in this level, since that may add new
	// sub-trees
	for (auto &inner : tree->walk()) {
		transform_operators_recursive<Evaluatable>(dynamic_cast<CommandListWalkable*>(inner.get()),
			factories, num_factories, right_associative, unary);
	}

	transform_operators_visit<Evaluatable>(dynamic_cast<CommandListSyntaxTree<Evaluatable>*>(tree),
		factories, num_factories, right_associative, unary);
}

// Using raw pointers here so that ::optimise() can call it with "this"
template <class Evaluatable>
static void _log_syntax_tree(CommandListSyntaxTree<Evaluatable> *tree);
template <class Evaluatable>
static void _log_token(CommandListToken *token)
{
	CommandListSyntaxTree<Evaluatable> *inner;
	CommandListOperator<Evaluatable> *op;
	CommandListOperatorToken *op_tok;
	CommandListOperand *operand;

	if (!token)
		return;

	// Can't use CommandListWalkable here, because it only walks over inner
	// syntax trees and this debug dumper needs to walk over everything

	inner = dynamic_cast<CommandListSyntaxTree<Evaluatable>*>(token);
	op = dynamic_cast<CommandListOperator<Evaluatable>*>(token);
	op_tok = dynamic_cast<CommandListOperatorToken*>(token);
	operand = dynamic_cast<CommandListOperand*>(token);
	if (inner) {
		_log_syntax_tree<Evaluatable>(inner);
	}
	else if (op) {
		LogInfoNoNL("Operator \"%S\"[ ", token->token.c_str());
		if (op->lhs_tree)
			_log_token<Evaluatable>(op->lhs_tree.get());
		else if (op->lhs)
			_log_token<Evaluatable>(dynamic_cast<CommandListToken*>(op->lhs.get()));
		if ((op->lhs_tree || op->lhs) && (op->rhs_tree || op->rhs))
			LogInfoNoNL(", ");
		if (op->rhs_tree)
			_log_token<Evaluatable>(op->rhs_tree.get());
		else if (op->rhs)
			_log_token<Evaluatable>(dynamic_cast<CommandListToken*>(op->rhs.get()));
		LogInfoNoNL(" ]");
	}
	else if (op_tok) {
		LogInfoNoNL("OperatorToken \"%S\"", token->token.c_str());
	}
	else if (operand) {
		LogInfoNoNL("Operand \"%S\"", token->token.c_str());
	}
	else {
		LogInfoNoNL("Token \"%S\"", token->token.c_str());
	}
}
template <class Evaluatable>
static void _log_syntax_tree(CommandListSyntaxTree<Evaluatable> *tree)
{
	CommandListSyntaxTree<Evaluatable>::Tokens::iterator i;

	LogInfoNoNL("SyntaxTree[ ");
	for (i = tree->tokens.begin(); i != tree->tokens.end(); i++) {
		_log_token<Evaluatable>((*i).get());
		if (i != tree->tokens.end() - 1)
			LogInfoNoNL(", ");
	}
	LogInfoNoNL(" ]");
}
template <class Evaluatable>
static void log_syntax_tree(CommandListSyntaxTree<Evaluatable> *tree, const char *msg)
{
	if (!gLogDebug)
		return;

	LogInfo(msg);
	_log_syntax_tree<Evaluatable>(tree);
	LogInfo("\n");
}

template<class T, class Evaluatable>
static void log_syntax_tree(T token, const char *msg)
{
	if (!gLogDebug)
		return;

	LogInfo(msg);
	_log_token<Evaluatable>(dynamic_cast<CommandListToken*>(token.get()));
	LogInfo("\n");
}

bool CommandListExpression::parse(const wstring *expression, const wstring *ini_namespace, CommandList *command_list)//CommandListScope *scope)
{
	CommandListSyntaxTree<CommandListEvaluatable> tree(0);

	try {
		tokenise(expression, &tree, ini_namespace, command_list);

		group_parenthesis(&tree);

		transform_operators_recursive(&tree, unary_operators, ARRAYSIZE(unary_operators), true, true);
		transform_operators_recursive(&tree, exponent_operators, ARRAYSIZE(exponent_operators), true, false);
		transform_operators_recursive(&tree, multi_division_operators, ARRAYSIZE(multi_division_operators), false, false);
		transform_operators_recursive(&tree, add_subtract_operators, ARRAYSIZE(add_subtract_operators), false, false);
		transform_operators_recursive(&tree, relational_operators, ARRAYSIZE(relational_operators), false, false);
		transform_operators_recursive(&tree, equality_operators, ARRAYSIZE(equality_operators), false, false);
		transform_operators_recursive(&tree, and_operators, ARRAYSIZE(and_operators), false, false);
		transform_operators_recursive(&tree, or_operators, ARRAYSIZE(or_operators), false, false);

		evaluatable = tree.finalise();
		log_syntax_tree<shared_ptr<CommandListEvaluatable>, CommandListEvaluatable>(evaluatable, "Final syntax tree:\n");
		return true;
	}
	catch (const CommandListSyntaxError &e) {
		LogOverlay(LOG_WARNING_MONOSPACE,
			"Syntax Error: %S\n"
			"              %*s: %S\n",
			expression->c_str(), (int)e.pos + 1, "^", e.msg.c_str());
		return false;
	}
}
bool CommandListMatrixExpression::parse(const wstring * expression, const wstring * ini_namespace, CommandListScopeMatrices * scope)
{
	CommandListSyntaxTree<CommandListMatrixEvaluatable> tree(0);
	try {
		matrix_tokenise(expression, &tree, ini_namespace, scope);

		group_parenthesis(&tree);
		transform_operators_recursive(&tree, matrix_unary_operators, ARRAYSIZE(matrix_unary_operators), true, true);
		transform_operators_recursive(&tree, matrix_multi_operators, ARRAYSIZE(matrix_multi_operators), false, false);
		transform_operators_recursive(&tree, matrix_add_subtract_operators, ARRAYSIZE(matrix_add_subtract_operators), false, false);

		evaluatable = tree.finalise();
		log_syntax_tree<shared_ptr<CommandListMatrixEvaluatable>, CommandListMatrixEvaluatable>(evaluatable, "Final syntax tree:\n");
		return true;
	}
	catch (const CommandListSyntaxError &e) {
		LogOverlay(LOG_WARNING_MONOSPACE,
			"Syntax Error: %S\n"
			"              %*s: %S\n",
			expression->c_str(), (int)e.pos + 1, "^", e.msg.c_str());
		return false;
	}
}
::D3DXMATRIX CommandListMatrixExpression::evaluate(CommandListState * state, D3D9Wrapper::IDirect3DDevice9 * device)
{
	return evaluatable->evaluate(state, device);
}
float CommandListExpression::evaluate(CommandListState *state, D3D9Wrapper::IDirect3DDevice9 *device)
{
	return evaluatable->evaluate(state, device);
}

bool CommandListExpression::static_evaluate(float *ret, D3D9Wrapper::IDirect3DDevice9 *device)
{
	return evaluatable->static_evaluate(ret, device);
}

bool CommandListExpression::optimise(D3D9Wrapper::IDirect3DDevice9 *device)
{
	std::shared_ptr<CommandListEvaluatable> replacement;
	bool ret;

	if (!evaluatable) {
		LogOverlay(LOG_DIRE, "BUG: Non-evaluatable expression, please report this and provide your d3dx.ini\n");
		evaluatable = std::make_shared<CommandListOperandFloat>(0, L"<BUG>");
		return false;
	}

	ret = evaluatable->optimise(device, &replacement);

	if (replacement)
		evaluatable = replacement;

	return ret;
}

// Finalises the syntax trees in the operator into evaluatable operands,
// thereby making this operator also evaluatable.
template <class Evaluatable>
std::shared_ptr<Evaluatable> CommandListOperator<Evaluatable>::finalise()
{
	auto lhs_finalisable = dynamic_pointer_cast<CommandListFinalisable<Evaluatable>>(lhs_tree);
	auto rhs_finalisable = dynamic_pointer_cast<CommandListFinalisable<Evaluatable>>(rhs_tree);
	auto lhs_evaluatable = dynamic_pointer_cast<Evaluatable>(lhs_tree);
	auto rhs_evaluatable = dynamic_pointer_cast<Evaluatable>(rhs_tree);

	if (lhs || rhs) {
		LogInfo("BUG: Attempted to finalise already final operator\n");
		throw CommandListSyntaxError(L"BUG", token_pos);
	}

	if (lhs_tree) { // Binary operators only
		if (!lhs && lhs_finalisable)
			lhs = lhs_finalisable->finalise();
		if (!lhs && lhs_evaluatable)
			lhs = lhs_evaluatable;
		if (!lhs)
			throw CommandListSyntaxError(L"BUG: LHS operand invalid", token_pos);
		lhs_tree = nullptr;
	}

	if (!rhs && rhs_finalisable)
		rhs = rhs_finalisable->finalise();
	if (!rhs && rhs_evaluatable)
		rhs = rhs_evaluatable;
	if (!rhs)
		throw CommandListSyntaxError(L"BUG: RHS operand invalid", token_pos);
	rhs_tree = nullptr;

	// Can't return "this", because that is an unmanaged version of the
	// pointer which is already managed elsewhere - if we were to create a
	// new managed pointer from that, we would have undefined behaviour.
	// Instead we just return nullptr to signify that this node does not
	// need to be replaced.
	return nullptr;
}

// Recursively finalises every node in the syntax tree. If the expression is
// valid the tree should be left with a single evaluatable node, which will be
// returned to the caller so that it can replace this tree with just the node.
// Throws a syntax error if the finalised nodes are not right.
template <class Evaluatable>
std::shared_ptr<Evaluatable> CommandListSyntaxTree<Evaluatable>::finalise()
{
	std::shared_ptr<CommandListFinalisable<Evaluatable>> finalisable;
	std::shared_ptr<Evaluatable> evaluatable;
	std::shared_ptr<CommandListToken> token;
	Tokens::iterator i;

	for (i = tokens.begin(); i != tokens.end(); i++) {
		finalisable = dynamic_pointer_cast<CommandListFinalisable<Evaluatable>>(*i);
		if (finalisable) {
			evaluatable = finalisable->finalise();
			if (evaluatable) {
				// A recursive syntax tree has been finalised
				// and we replace it with its sole evaluatable
				// contents:
				token = dynamic_pointer_cast<CommandListToken>(evaluatable);
				if (!token) {
					LogInfo("BUG: finalised token did not cast back\n");
					throw CommandListSyntaxError(L"BUG", token_pos);
				}
				i = tokens.erase(i);
				i = tokens.insert(i, std::move(token));
			}
		}
	}

	// A finalised syntax tree should be reduced to a single evaluatable
	// operator/operand, which we pass back up the stack to replace this
	// tree
	if (tokens.empty())
		throw CommandListSyntaxError(L"Empty expression", 0);

	if (tokens.size() > 1)
		throw CommandListSyntaxError(L"Unexpected", tokens[1]->token_pos);

	evaluatable = dynamic_pointer_cast<Evaluatable>(tokens[0]);
	if (!evaluatable)
		throw CommandListSyntaxError(L"Non-evaluatable", tokens[0]->token_pos);

	return evaluatable;
}
template <class Evaluatable>
vector<std::shared_ptr<CommandListWalkable>> CommandListSyntaxTree<Evaluatable>::walk()
{
	Walk ret;
	std::shared_ptr<CommandListWalkable> inner;
	Tokens::iterator i;

	for (i = tokens.begin(); i != tokens.end(); i++) {
		inner = dynamic_pointer_cast<CommandListWalkable>(*i);
		if (inner)
			ret.push_back(std::move(inner));
	}

	return ret;
}

float CommandListOperatorFloat::evaluate(CommandListState *state, D3D9Wrapper::IDirect3DDevice9 *device)
{
	if (lhs) // Binary operator
		return evaluate(lhs->evaluate(state, device), rhs->evaluate(state, device));
	return evaluate(std::numeric_limits<float>::quiet_NaN(), rhs->evaluate(state, device));
}

bool CommandListOperatorFloat::static_evaluate(float *ret, D3D9Wrapper::IDirect3DDevice9 *device)
{
	float lhs_static = std::numeric_limits<float>::quiet_NaN(), rhs_static;
	bool is_static;

	is_static = rhs->static_evaluate(&rhs_static, device);
	if (lhs) // Binary operator
		is_static = lhs->static_evaluate(&lhs_static, device) && is_static;

	if (is_static) {
		if (ret)
			*ret = evaluate(lhs_static, rhs_static);
		return true;
	}

	return false;
}

bool CommandListOperatorFloat::optimise(D3D9Wrapper::IDirect3DDevice9 *device, std::shared_ptr<CommandListEvaluatable> *replacement)
{
	std::shared_ptr<CommandListEvaluatable> lhs_replacement;
	std::shared_ptr<CommandListEvaluatable> rhs_replacement;
	shared_ptr<CommandListOperandFloat> operand;
	bool making_progress = false;
	float static_val;
	wstring static_val_str;

	if (lhs)
		making_progress = lhs->optimise(device, &lhs_replacement) || making_progress;
	if (rhs)
		making_progress = rhs->optimise(device, &rhs_replacement) || making_progress;

	if (lhs_replacement)
		lhs = lhs_replacement;
	if (rhs_replacement)
		rhs = rhs_replacement;

	if (!static_evaluate(&static_val, device))
		return making_progress;

	// FIXME: Pretty print rather than dumping syntax tree
	LogInfoNoNL("Statically evaluated \"");
	_log_token<CommandListEvaluatable>(dynamic_cast<CommandListToken*>(this));
	LogInfo("\" as %f\n", static_val);
	static_val_str = std::to_wstring(static_val);

	operand = make_shared<CommandListOperandFloat>(token_pos, static_val_str.c_str());
	operand->type = ParamOverrideType::VALUE;
	operand->val = static_val;
	*replacement = dynamic_pointer_cast<CommandListEvaluatable>(operand);
	return true;
}
template <class Evaluatable>
vector<std::shared_ptr<CommandListWalkable>> CommandListOperator<Evaluatable>::walk()
{
	Walk ret;
	std::shared_ptr<CommandListWalkable> lhs;
	std::shared_ptr<CommandListWalkable> rhs;

	lhs = dynamic_pointer_cast<CommandListWalkable>(lhs_tree);
	rhs = dynamic_pointer_cast<CommandListWalkable>(rhs_tree);

	if (lhs)
		ret.push_back(std::move(lhs));
	if (rhs)
		ret.push_back(std::move(rhs));

	return ret;
}

void ParamOverride::run(CommandListState *state)
{
	float *dest = &(G->IniConstants[param_idx].*param_component);
	float orig = *dest;

	COMMAND_LIST_LOG(state, "%S\n", ini_line.c_str());

	*dest = expression.evaluate(state);

	COMMAND_LIST_LOG(state, "  ini param override = %f\n", *dest);

	state->update_params |= (*dest != orig);
}

void VariableAssignment::run(CommandListState *state)
{
	float orig = var->fval;

	COMMAND_LIST_LOG(state, "%S\n", ini_line.c_str());

	var->fval = expression.evaluate(state);

	COMMAND_LIST_LOG(state, "  = %f\n", var->fval);

	if (var->flags & VariableFlags::PERSIST)
		G->user_config_dirty |= (var->fval != orig);
}

bool AssignmentCommand::optimise(D3D9Wrapper::IDirect3DDevice9 *device)
{
	return expression.optimise(device);
}

static bool operand_allowed_in_context(ParamOverrideType type, CommandListScope *scope)
{
	if (scope)
		return true;

	// List of operand types allowed outside of a command list, e.g. in a
	// [Key] / [Preset] section
	switch (type) {
	case ParamOverrideType::VALUE:
	case ParamOverrideType::ISNAN:
	case ParamOverrideType::INI_PARAM:
	case ParamOverrideType::VARIABLE:
	case ParamOverrideType::RES_WIDTH:
	case ParamOverrideType::RES_HEIGHT:
	case ParamOverrideType::BACK_BUFFER_WIDTH:
	case ParamOverrideType::BACK_BUFFER_HEIGHT:
	case ParamOverrideType::TIME:
	case ParamOverrideType::RAW_SEPARATION:
	case ParamOverrideType::CONVERGENCE:
	case ParamOverrideType::EYE_SEPARATION:
	case ParamOverrideType::STEREO_ACTIVE:
	case ParamOverrideType::SLI:
	case ParamOverrideType::HUNTING:
	case ParamOverrideType::CURSOR_COLOR_HEIGHT:
	case ParamOverrideType::CURSOR_COLOR_WIDTH:
	case ParamOverrideType::CURSOR_MASK_HEIGHT:
	case ParamOverrideType::CURSOR_MASK_WIDTH:
	case ParamOverrideType::PIXEL_SHADER_BOOL:
	case ParamOverrideType::PIXEL_SHADER_FLOAT:
	case ParamOverrideType::PIXEL_SHADER_INT:
	case ParamOverrideType::VERTEX_SHADER_BOOL:
	case ParamOverrideType::VERTEX_SHADER_FLOAT:
	case ParamOverrideType::VERTEX_SHADER_INT:
	case ParamOverrideType::PIXEL_SHADER_TEXTURE_HEIGHT:
	case ParamOverrideType::PIXEL_SHADER_TEXTURE_WIDTH:
	case ParamOverrideType::VERTEX_SHADER_TEXTURE_HEIGHT:
	case ParamOverrideType::VERTEX_SHADER_TEXTURE_WIDTH:
	case ParamOverrideType::VIEWPORT_HEIGHT:
	case ParamOverrideType::VIEWPORT_WIDTH:
	case ParamOverrideType::DEPTH_BUFFER_CMPFUNC_GREATER:
	case ParamOverrideType::DEPTH_BUFFER_CMPFUNC_GREATEREQUAL:
	case ParamOverrideType::INTERNAL_FUNCTION_FLOAT:
	case ParamOverrideType::STEREO_PARAMS_UPDATED:
		return true;
	}
	return false;
}

bool valid_variable_name(const wstring &name)
{
	if (name.length() < 2)
		return false;

	// Variable names begin with a $
	if (name[0] != L'$')
		return false;

	// First character must be a letter or underscore ($1, $2, etc reserved for future arguments):
	if ((name[1] < L'a' || name[1] > L'z') && name[1] != L'_')
		return false;

	// Subsequent characters must be in this list:
	return (name.find_first_not_of(L"abcdefghijklmnopqrstuvwxyz_0123456789", 2) == wstring::npos);
}
bool is_matrix(const wstring & declaration, wstring * name)
{
	UINT size;
	if (!(is_variable_array(declaration, name, &size)))
		return false;
	if (size != 16)
		return false;
	return true;
}
bool is_variable_array(const wstring &declaration, wstring *name, UINT *size)
{
	int ret, len1;
	wchar_t variable[MAX_PATH];
	ret = swscanf_s(declaration.c_str(), L"%[^[][%u]%n", variable, _countof(variable), size, &len1);
	if (ret != 0 && ret != EOF && len1 == declaration.length()) {
		if (!valid_variable_name(variable))
			return false;
		*name = wstring(variable);
		return true;
	}
	return false;
}
bool global_variable_exists(const wstring & name)
{
	// We need value in lower case so our keys will be consistent in the
	// unordered_map. ParseCommandList will have already done this, but the
	// Key/Preset parsing code will not have, and rather than require it to
	// we do it here:
	wstring low_name(name);
	std::transform(low_name.begin(), low_name.end(), low_name.begin(), ::towlower);
	if (command_list_globals.find(low_name) != command_list_globals.end())
		return true;
	if (command_list_global_arrays.find(low_name) != command_list_global_arrays.end())
		return true;
	if (command_list_global_matrices.find(low_name) != command_list_global_matrices.end())
		return true;
	return false;
}
bool local_variable_exists(const wstring & name, CommandList *commandList)
{
	CommandListScope::iterator it;
	CommandListScopeArrays::iterator it_a;
	CommandListScopeMatrices::iterator it_m;
	for (it = commandList->scope->begin(); it != commandList->scope->end(); it++) {
		auto match = it->find(name);
		if (match != it->end()) {
			return true;
		}
	}
	for (it_a = commandList->scope_arrays->begin(); it_a != commandList->scope_arrays->end(); it_a++) {
		auto match = it_a->find(name);
		if (match != it_a->end()) {
			return true;
		}
	}
	for (it_m = commandList->scope_matrices->begin(); it_m != commandList->scope_matrices->end(); it_m++) {
		auto match = it_m->find(name);
		if (match != it_m->end()) {
			return true;
		}
	}
	return false;
}
bool parse_command_list_array_name(const wstring &name, const wstring *ini_namespace, CommandListVariableArray **target, UINT *startSlot, UINT *numSlots)
{
	CommandListVariableArrays::iterator var = command_list_global_arrays.end();
	UINT ret, len1, slot, num;
	wchar_t variable[MAX_PATH];
	bool parsed = false;

	if (name.length() < 2 || name[0] != L'$')
		return false;

	// We need value in lower case so our keys will be consistent in the
	// unordered_map. ParseCommandList will have already done this, but the
	// Key/Preset parsing code will not have, and rather than require it to
	// we do it here:

	slot = 0;
	num = 0;

	wstring low_name(name);
	std::transform(low_name.begin(), low_name.end(), low_name.begin(), ::towlower);
	ret = swscanf_s(low_name.c_str(), L"%[^[][%u]%u%n", variable, _countof(variable), &slot, &num, &len1);
	if (ret != 0 && ret != EOF && len1 == low_name.length()) {
		parsed = true;
	}
	else {
		ret = swscanf_s(name.c_str(), L"%[^[][%u]%n", variable, _countof(variable), &slot, &len1);
		if (ret != 0 && ret != EOF && len1 == name.length()) {
			parsed = true;
			num = 1;
		}
		else
		{
			ret = swscanf_s(name.c_str(), L"%ls%n", variable, _countof(variable), &len1);
			if (ret != 0 && ret != EOF && len1 == low_name.length()) {
				parsed = true;
			}
		}
	}

	if (parsed) {
		var = command_list_global_arrays.end();
		if (!ini_namespace->empty())
			var = command_list_global_arrays.find(get_namespaced_var_name_lower(variable, ini_namespace));
		if (var == command_list_global_arrays.end())
			var = command_list_global_arrays.find(variable);
		if (var == command_list_global_arrays.end())
			return false;
		*target = &var->second;
		if (startSlot) {
			if (slot < (*target)->fvals.size())
				*startSlot = slot;
			else
				*startSlot = 0;
		}
		if (numSlots) {
			if (num == 0)
				*numSlots = (UINT)(*target)->fvals.size();
			else
			{
				if ((*startSlot + num) < (*target)->fvals.size())
					*numSlots = num;
				else
					*numSlots = (UINT)(*target)->fvals.size() - *startSlot;
			}
		}
		return true;
	}
	return false;
}
bool parse_command_list_var_name(const wstring &name, const wstring *ini_namespace, CommandListVariableFloat **target)
{
	CommandListVariableFloats::iterator var = command_list_globals.end();

	if (name.length() < 2 || name[0] != L'$')
		return false;

	// We need value in lower case so our keys will be consistent in the
	// unordered_map. ParseCommandList will have already done this, but the
	// Key/Preset parsing code will not have, and rather than require it to
	// we do it here:
	wstring low_name(name);
	std::transform(low_name.begin(), low_name.end(), low_name.begin(), ::towlower);

	var = command_list_globals.end();
	if (!ini_namespace->empty())
		var = command_list_globals.find(get_namespaced_var_name_lower(low_name, ini_namespace));
	if (var == command_list_globals.end())
		var = command_list_globals.find(low_name);
	if (var == command_list_globals.end())
		return false;

	*target = &var->second;
	return true;
}

int find_local_variable_array(const wstring &name, CommandListScopeArrays *scope, CommandListVariableArray **arr, UINT *startSlot, UINT *numSlots)
{
	CommandListScopeArrays::iterator it;
	UINT ret, len1, slot, num;
	wchar_t variable[MAX_PATH];
	bool parsed = false;

	if (!scope)
		return false;
	if (name.length() < 2 || name[0] != L'$')
		return false;

	slot = 0;
	num = 0;

	ret = swscanf_s(name.c_str(), L"%[^[][%u]%u%n", variable, _countof(variable), &slot, &num, &len1);
	if (ret != 0 && ret != EOF && len1 == name.length()) {
		parsed = true;
	}
	else {
		ret = swscanf_s(name.c_str(), L"%[^[][%u]%n", variable, _countof(variable), &slot, &len1);
		if (ret != 0 && ret != EOF && len1 == name.length()) {
			parsed = true;
			num = 0;
		}
		else
		{
			ret = swscanf_s(name.c_str(), L"%ls%n", variable, _countof(variable), &len1);
			if (ret != 0 && ret != EOF && len1 == name.length())
				parsed = true;
		}
	}
	if (parsed) {
		for (it = scope->begin(); it != scope->end(); it++) {
			auto match = it->find(variable);
			if (match != it->end()) {
				*arr = match->second;
				if (startSlot) {
					if (slot < (*arr)->fvals.size())
						*startSlot = slot;
					else
						*startSlot = 0;
				}
				if (numSlots) {
					if (num == 0)
						*numSlots = (UINT)(*arr)->fvals.size();
					else
					{
						if ((*startSlot + num) < (*arr)->fvals.size())
							*numSlots = num;
						else
							*numSlots = (UINT)(*arr)->fvals.size() - *startSlot;

					}
				}
				return true;
			}
		}
	}
	return false;
}
int find_local_variable(const wstring &name, CommandListScope *scope, CommandListVariableFloat **var)
{
	CommandListScope::iterator it;

	if (!scope)
		return false;

	if (name.length() < 2 || name[0] != L'$')
		return false;

	for (it = scope->begin(); it != scope->end(); it++) {
		auto match = it->find(name);
		if (match != it->end()) {
			*var = match->second;
			return true;
		}
	}
	return false;
}
bool valid_local_variable(const wchar_t *section, wstring &name,
	CommandList *pre_command_list, const wstring *ini_namespace)
{
	CommandListVariableFloat *var = NULL;
	if (find_local_variable(name, pre_command_list->scope, &var)) {
		// Could allow this at different scope levels, but... no.
		// You can declare local variables of the same name in
		// independent scopes (if {local $tmp} else {local $tmp}), but
		// we won't allow masking a local variable from a parent scope,
		// because that's usually a bug. Choose a different name son.
		LogOverlay(LOG_WARNING, "WARNING: Illegal redeclaration of local variable [%S] %S\n", section, name.c_str());
		return false;
	}

	if (parse_command_list_var_name(name, ini_namespace, &var)) {
		// Not making this fatal since this could clash between say a
		// global in the d3dx.ini and a local variable in another ini.
		// Just issue a notice in hunting mode and carry on.
		LogOverlay(LOG_NOTICE, "WARNING: [%S] local %S masks a global variable with the same name\n", section, name.c_str());
	}

	pre_command_list->static_vars.emplace_front(name, 0.0f, VariableFlags::NONE);
	pre_command_list->scope->front()[name] = &pre_command_list->static_vars.front();

	return true;
}
bool declare_local_matrix(const wchar_t *section, wstring &decl,
	CommandList *pre_command_list, const wstring *ini_namespace, wstring *name)
{
	wstring _name;
	if (!is_matrix(decl, &_name)) {
		//LogOverlay(LOG_WARNING, "WARNING: Illegal local variable name: [%S] \"%S\"\n", section, decl.c_str());
		return false;
	}
	else {
		if (local_variable_exists(_name, pre_command_list)) {
			// Could allow this at different scope levels, but... no.
			// You can declare local variables of the same name in
			// independent scopes (if {local $tmp} else {local $tmp}), but
			// we won't allow masking a local variable from a parent scope,
			// because that's usually a bug. Choose a different name son.
			LogOverlay(LOG_WARNING, "WARNING: Illegal redeclaration of local variable [%S] %S\n", section, _name.c_str());
			return false;
		}
		if (global_variable_exists(_name)) {
			// Not making this fatal since this could clash between say a
			// global in the d3dx.ini and a local variable in another ini.
			// Just issue a notice in hunting mode and carry on.
			LogOverlay(LOG_NOTICE, "WARNING: [%S] local %S masks a global variable with the same name\n", section, _name.c_str());
		}
		pre_command_list->static_matrices.emplace_front(_name, (*::D3DXMatrixIdentity(&::D3DXMATRIX())), VariableFlags::NONE);
		pre_command_list->scope_matrices->front()[_name] = &pre_command_list->static_matrices.front();

		return true;
	}
}
bool declare_local_variable_array(const wchar_t *section, wstring &decl,
	CommandList *pre_command_list, const wstring *ini_namespace, wstring *name)
{
	UINT size;
	//wstring _name;
	if (!is_variable_array(decl, name, &size)) {
		//LogOverlay(LOG_WARNING, "WARNING: Illegal local variable name: [%S] \"%S\"\n", section, decl.c_str());
		return false;
	}
	else {
		if (local_variable_exists(*name, pre_command_list)) {
			// Could allow this at different scope levels, but... no.
			// You can declare local variables of the same name in
			// independent scopes (if {local $tmp} else {local $tmp}), but
			// we won't allow masking a local variable from a parent scope,
			// because that's usually a bug. Choose a different name son.
			LogOverlay(LOG_WARNING, "WARNING: Illegal redeclaration of local variable [%S] %S\n", section, (*name).c_str());
			return false;
		}
		if (global_variable_exists(*name)) {
			// Not making this fatal since this could clash between say a
			// global in the d3dx.ini and a local variable in another ini.
			// Just issue a notice in hunting mode and carry on.
			LogOverlay(LOG_NOTICE, "WARNING: [%S] local %S masks a global variable with the same name\n", section, (*name).c_str());
		}

		pre_command_list->static_var_arrays.emplace_front(*name, vector<float>(), VariableFlags::NONE);
		pre_command_list->scope_arrays->front()[*name] = &pre_command_list->static_var_arrays.front();

		return true;
	}
}
bool declare_local_variable(const wchar_t *section, wstring &name,
	CommandList *pre_command_list, const wstring *ini_namespace)
{
	if (!valid_variable_name(name)) {
		LogOverlay(LOG_WARNING, "WARNING: Illegal local variable name: [%S] \"%S\"\n", section, name.c_str());
		return false;
	}
	return valid_local_variable(section, name, pre_command_list, ini_namespace);

}
static bool ParseShaderConstant(const wchar_t * target, wchar_t *shader_type, ConstantType *constant_type, int *idx, float DirectX::XMFLOAT4::**component)
{
	int ret, len;
	wchar_t component_chr;
	unsigned slot;
	wchar_t type;
	wchar_t shader;
	size_t length = wcslen(target);
	ret = swscanf_s(target, L"%lcs%lc_%u%lc%n", &shader, 1, &type, 1, &slot, &component_chr, 1, &len);
	if (ret == 4 && len == length) {
		*idx = slot;
		switch (shader) {
		case L'v': case L'p':
			*shader_type = shader;
			break;
		default:
			return false;
		}
		switch (type) {
		case L'f':
			*constant_type = ConstantType::FLOAT;
			break;
		case L'i':
			*constant_type = ConstantType::INT;
			break;
		case L'b':
			*constant_type = ConstantType::BOOL;
			break;
		default:
			return false;
		}
		switch (component_chr) {
		case L'x':
			*component = &DirectX::XMFLOAT4::x;
			return true;
		case L'y':
			*component = &DirectX::XMFLOAT4::y;
			return true;
		case L'z':
			*component = &DirectX::XMFLOAT4::z;
			return true;
		case L'w':
			*component = &DirectX::XMFLOAT4::w;
			return true;
		}
	}
	return false;
}

bool CommandListOperandFloat::parse(const wstring *operand, const wstring *ini_namespace, CommandList *command_list)//CommandListScope *scope)
{
	CommandListVariableFloat *var = NULL;
	int ret, len1;
	CommandListScope *scope = NULL;
	if (command_list)
		scope = command_list->scope;
	// Try parsing value as a float
	ret = swscanf_s(operand->c_str(), L"%f%n", &val, &len1);
	if (ret != 0 && ret != EOF && len1 == operand->length()) {
		type = ParamOverrideType::VALUE;
		return operand_allowed_in_context(type, scope);
	}

	// Try parsing operand as an ini param:
	if (ParseIniParamName(operand->c_str(), &idx, &component)) {
		type = ParamOverrideType::INI_PARAM;
		return operand_allowed_in_context(type, scope);
	}

	// Try parsing operand as an shader constant:
	ConstantType const_type;
	wchar_t shader_type;
	if (ParseShaderConstant(operand->c_str(), &shader_type, &const_type, &idx, &component)) {
		switch (shader_type) {
		case L'v':
			switch (const_type) {
			case ConstantType::FLOAT:
				type = ParamOverrideType::VERTEX_SHADER_FLOAT;
				break;
			case ConstantType::BOOL:
				type = ParamOverrideType::VERTEX_SHADER_BOOL;
				break;
			case ConstantType::INT:
				type = ParamOverrideType::VERTEX_SHADER_INT;
			}
			break;
		case L'p':
			switch (const_type) {
			case ConstantType::FLOAT:
				type = ParamOverrideType::PIXEL_SHADER_FLOAT;
				break;
			case ConstantType::BOOL:
				type = ParamOverrideType::PIXEL_SHADER_BOOL;
				break;
			case ConstantType::INT:
				type = ParamOverrideType::PIXEL_SHADER_INT;
				break;
			}
			break;
		}
		return operand_allowed_in_context(type, scope);
	}

	// Try parsing value as a render target width
	ret = swscanf_s(operand->c_str(), L"o%u_width%n", &idx, &len1);
	if (ret != 0 && ret != EOF && len1 == operand->length()) {
		type = ParamOverrideType::RT_WIDTH;
		return operand_allowed_in_context(type, scope);
	}

	// Try parsing value as a render target width
	ret = swscanf_s(operand->c_str(), L"o%u_height%n", &idx, &len1);
	if (ret != 0 && ret != EOF && len1 == operand->length()) {
		type = ParamOverrideType::RT_HEIGHT;
		return operand_allowed_in_context(type, scope);
	}

	// Try parsing value as a pixel sampler width
	ret = swscanf_s(operand->c_str(), L"ps%u_width%n", &idx, &len1);
	if (ret != 0 && ret != EOF && len1 == operand->length()) {
		type = ParamOverrideType::PIXEL_SHADER_TEXTURE_WIDTH;
		return operand_allowed_in_context(type, scope);
	}

	// Try parsing value as a pixel sampler width
	ret = swscanf_s(operand->c_str(), L"ps%u_height%n", &idx, &len1);
	if (ret != 0 && ret != EOF && len1 == operand->length()) {
		type = ParamOverrideType::PIXEL_SHADER_TEXTURE_HEIGHT;
		return operand_allowed_in_context(type, scope);
	}

	// Try parsing value as a pixel sampler width
	ret = swscanf_s(operand->c_str(), L"vs%u_width%n", &idx, &len1);
	if (ret != 0 && ret != EOF && len1 == operand->length()) {
		type = ParamOverrideType::VERTEX_SHADER_TEXTURE_WIDTH;
		return operand_allowed_in_context(type, scope);
	}

	// Try parsing value as a pixel sampler width
	ret = swscanf_s(operand->c_str(), L"vs%u_height%n", &idx, &len1);
	if (ret != 0 && ret != EOF && len1 == operand->length()) {
		type = ParamOverrideType::VERTEX_SHADER_TEXTURE_HEIGHT;
		return operand_allowed_in_context(type, scope);
	}

	// Try parsing operand as a variable array member:
	wchar_t variable[MAX_PATH];
	ret = swscanf_s(operand->c_str(), L"%[^[][%u]%n", variable, _countof(variable), &idx, &len1);
	if (ret != 0 && ret != EOF && len1 == operand->length()) {
		if (command_list && command_list->scope_matrices) {
			CommandListScopeMatrices::iterator it_m;
			for (it_m = command_list->scope_matrices->begin(); it_m != command_list->scope_matrices->end(); it_m++) {
				auto match = it_m->find(variable);
				if (match != it_m->end()) {
					type = ParamOverrideType::VARIABLE;
					var_ftarget = &match->second->fmatrix[idx];
					return true;
				}
			}
		}
		CommandListMatrices::iterator matrix = command_list_global_matrices.end();
		matrix = command_list_global_matrices.end();
		if (!ini_namespace->empty())
			matrix = command_list_global_matrices.find(get_namespaced_var_name_lower(variable, ini_namespace));
		if (matrix == command_list_global_matrices.end())
			matrix = command_list_global_matrices.find(variable);
		if (matrix != command_list_global_matrices.end()) {
			type = ParamOverrideType::VARIABLE;
			var_ftarget = &matrix->second.fmatrix[idx];
			return true;
		}
		if (command_list && command_list->scope_arrays) {
			CommandListScopeArrays::iterator it_a;
			for (it_a = command_list->scope_arrays->begin(); it_a != command_list->scope_arrays->end(); it_a++) {
				auto match = it_a->find(variable);
				if (match != it_a->end()) {
					type = ParamOverrideType::VARIABLE;
					var_ftarget = &match->second->fvals[idx];
					return true;
				}
			}
		}
		CommandListVariableArrays::iterator arr = command_list_global_arrays.end();
		arr = command_list_global_arrays.end();
		if (!ini_namespace->empty())
			arr = command_list_global_arrays.find(get_namespaced_var_name_lower(variable, ini_namespace));
		if (arr == command_list_global_arrays.end())
			arr = command_list_global_arrays.find(variable);
		if (arr != command_list_global_arrays.end()) {
			type = ParamOverrideType::VARIABLE;
			var_ftarget = &arr->second.fvals[idx];
			return true;
		}
	}
	// Try parsing operand as a variable:
	if (find_local_variable(*operand, scope, &var) ||
		parse_command_list_var_name(*operand, ini_namespace, &var)) {
		type = ParamOverrideType::VARIABLE;
		var_ftarget = &var->fval;
		return operand_allowed_in_context(type, scope);
	}
	wchar_t resource[MAX_PATH];
	ret = swscanf_s(operand->c_str(), L"s_%ls%n", resource, _countof(resource), &len1);
	if (ret != 0 && ret != EOF && len1 == operand->length()) {
		if (staging_op.src.ParseTarget(resource, true, ini_namespace)) {
			type = ParamOverrideType::STAGING_OP;
			return operand_allowed_in_context(type, scope);
		}
	}
	// Try parsing value as a resource target for texture filtering
	ret = texture_filter_target.ParseTarget(operand->c_str(), true, ini_namespace);
	if (ret) {
		type = ParamOverrideType::TEXTURE;
		return operand_allowed_in_context(type, scope);
	}
	ret = swscanf_s(operand->c_str(), L"scissor_%n", &len1);
	if (ret == 1){
		if (!wcscmp(operand->c_str() + len1, L"left"))
			type = ParamOverrideType::SCISSOR_LEFT;
		else if (!wcscmp(operand->c_str() + len1, L"top"))
			type = ParamOverrideType::SCISSOR_TOP;
		else if (!wcscmp(operand->c_str() + len1, L"right"))
			type = ParamOverrideType::SCISSOR_RIGHT;
		else if (!wcscmp(operand->c_str() + len1, L"bottom"))
			type = ParamOverrideType::SCISSOR_BOTTOM;
		else
			return false;
		return operand_allowed_in_context(type, scope);
	}
	ret = swscanf_s(operand->c_str(), L"r_dss_cmp_%n", &len1);
	if (len1 == 10) {
		if (!wcscmp(operand->c_str() + len1, L"greater"))
			type = ParamOverrideType::DEPTH_BUFFER_CMPFUNC_GREATER;
		else if (!wcscmp(operand->c_str() + len1, L"greaterequal"))
			type = ParamOverrideType::DEPTH_BUFFER_CMPFUNC_GREATEREQUAL;
		else
			return false;
		return operand_allowed_in_context(type, scope);
	}
	// Check interal function float
	iff = lookup_enum_val<const wchar_t *, InternalFunctionFloat>
		(InternalFuntionFloatNames, operand->c_str(), InternalFunctionFloat::INVALID);
	if (iff != InternalFunctionFloat::INVALID) {
		type = ParamOverrideType::INTERNAL_FUNCTION_FLOAT;
		return operand_allowed_in_context(type, scope);
	}
	// Check special keywords
	type = lookup_enum_val<const wchar_t *, ParamOverrideType>
		(ParamOverrideTypeNames, operand->c_str(), ParamOverrideType::INVALID);
	if (type != ParamOverrideType::INVALID)
		return operand_allowed_in_context(type, scope);

	return false;
}
// Parse IniParams overrides, in forms such as
// x = 0.3 (set parameter to specific value, e.g. for shader partner filtering)
// y2 = ps-t0 (use parameter for texture filtering based on texture slot of shader type)
// z3 = rt_width / rt_height (set parameter to render target width/height)
// w4 = res_width / res_height (set parameter to resolution width/height)
bool ParseCommandListIniParamOverride(const wchar_t *section,
	const wchar_t *key, wstring *val, CommandList *command_list,
	const wstring *ini_namespace)
{
	ParamOverride *param = new ParamOverride();

	if (!ParseIniParamName(key, &param->param_idx, &param->param_component))
		goto bail;

	if (!param->expression.parse(val, ini_namespace, command_list))
		goto bail;

	param->ini_line = L"[" + wstring(section) + L"] " + wstring(key) + L" = " + *val;
	command_list->commands.push_back(std::shared_ptr<CommandListCommand>(param));
	return true;
bail:
	delete param;
	return false;
}
wchar_t *trimwhitespace(wchar_t *str)
{
	wchar_t *end;

	// Trim leading space
	while (isspace((wchar_t)*str)) str++;

	if (*str == 0)  // All spaces?
		return str;

	// Trim trailing space
	end = str + wcslen(str) - 1;
	while (end > str && isspace((wchar_t)*end)) end--;

	// Write new null terminator character
	end[1] = '\0';

	return str;
}
template<typename T> T* getPointer(T& t) { return &t; }
bool val_to_variable_array_assignment(wstring * val, vector<float> *fvals, UINT start_slot, UINT num_slots, CommandList * command_list, const wstring * ini_namespace, VariableArrayAssignment **assignment_command)
{
	vector<wstring> vals;
	wchar_t * wcs = const_cast<wchar_t*>(val->c_str());
	wchar_t * pwc;
	wchar_t *buffer;
	pwc = wcstok_s(wcs, L",", &buffer);
	while (pwc != NULL)
	{
		vals.push_back(trimwhitespace(pwc));
		pwc = wcstok_s(NULL, L",", &buffer);
	}
	ConstantType const_type;
	wchar_t shader_type;
	int idx;
	UINT vector4Count;
	UINT i = start_slot;
	if (vals.size() == 0)
		return false;
	UINT _start_slot, _num_slots;
	VariableArrayAssignment *command = new VariableArrayAssignment();
	for (auto &_val : vals)
	{
		if (i >= (num_slots + start_slot))
			break;
		int ret, len1, slot, num;
		wchar_t exp[MAX_PATH];
		wstring strExp;
		VariableArrayFromMatrixExpressionAssignment matrix_command = VariableArrayFromMatrixExpressionAssignment();
		UINT num_slots;
		ret = swscanf_s(_val.c_str(), L"(%ls)[%u]%u%n", exp, _countof(exp), &slot, &num, &len1);
		if (ret == 3 && ret != EOF && len1 == _val.length()) {
			matrix_command.start_slot = slot;
			num_slots = num;
			strExp = exp;
		}
		else {
			num_slots = 16;
			strExp = _val;
		}
		CommandListMatrixExpression expression;
		if (expression.parse(&strExp, ini_namespace, command_list->scope_matrices)) {
			matrix_command.expression = expression;
			auto first = (*fvals).begin() + start_slot + i;
			int end;
			if (start_slot + i + num_slots > (*fvals).size()) {
				end = (UINT)(*fvals).size();
			}
			else {
				end = start_slot + i + num_slots;
			}
			auto last = (*fvals).begin() + end;
			vector<float*> matrix_vars;
			matrix_vars.resize((end - (start_slot + i)));
			transform(first, last, matrix_vars.begin(), getPointer<float>);
			matrix_command.vars = matrix_vars;
			i += (end - (start_slot + i));
			command->matrix_expression_assignments.push_back(matrix_command);
			continue;
		}
		CommandListMatrix *value_matrix;
		if (find_local_matrix(_val, command_list->scope_matrices, &value_matrix, &_start_slot, &_num_slots) || parse_command_list_matrix_name(_val, ini_namespace, &value_matrix, &_start_slot, &_num_slots)) {
			VariableArrayFromMatrixAssignment matrix_command = VariableArrayFromMatrixAssignment();
			matrix_command.matrix = &value_matrix->fmatrix;
			matrix_command.start_slot = _start_slot;
			auto first = (*fvals).begin() + start_slot + i;
			int end;
			if (_num_slots >= (num_slots - i))
				end = _start_slot + (num_slots - i);
			else
				end = _start_slot + _num_slots;
			auto last = (*fvals).begin() + end;
			vector<float*> matrix_vars;
			matrix_vars.resize((end - (start_slot + i)));
			transform(first, last, matrix_vars.begin(), getPointer<float>);
			matrix_command.vars = matrix_vars;
			i += (end - (start_slot + i));
			command->matrix_assignments.push_back(matrix_command);
			continue;
		}
		if (ParseShaderConstant(_val.c_str(), &shader_type, &const_type, &idx, &vector4Count)) {
			VariableArrayFromShaderConstantAssignment shader_command = VariableArrayFromShaderConstantAssignment();
			shader_command.shader_type = shader_type;
			shader_command.constant_type = const_type;
			shader_command.slot = idx;
			auto first = (*fvals).begin() + start_slot + i;
			int end;
			if (start_slot + i + (vector4Count * 4) > (*fvals).size()) {
				end = (UINT)(*fvals).size();
			}
			else {
				end = start_slot + i + (vector4Count * 4);
			}
			auto last = (*fvals).begin() + end;
			vector<float*> shader_vars;
			shader_vars.resize((end - (start_slot + i)));
			transform(first, last, shader_vars.begin(), getPointer<float>);
			shader_command.vars = shader_vars;
			i += (end - (start_slot + i));
			command->shader_constant_assignments.push_back(shader_command);
			continue;
		}
		CommandListVariableArray *value_arr;
		UINT _start_slot, _num_slots;
		if (find_local_variable_array(_val, command_list->scope_arrays, &value_arr, &_start_slot, &_num_slots) || parse_command_list_array_name(_val, ini_namespace, &value_arr, &_start_slot, &_num_slots)) {
			VariableArrayFromArrayAssignment array_command = VariableArrayFromArrayAssignment();
			int end;
			if (_num_slots >= (num_slots - i))
				end = _start_slot + (num_slots - i);
			else
				end = _start_slot + _num_slots;
			for (int x = _start_slot; x < end; ++x)
			{
				array_command.map.emplace(&(*fvals)[i], &value_arr->fvals[x]);
				++i;
			}
			command->array_assignments.push_back(array_command);
			continue;
		}
		VariableArrayFromExpressionAssignment expression_command = VariableArrayFromExpressionAssignment();
		if (expression_command.expression.parse(&_val, ini_namespace, command_list)) {
			expression_command.fval = &(*fvals)[i];
			command->expression_assignments.push_back(expression_command);
			i++;
			continue;
		}
		goto bail;
	}
	*assignment_command = command;
	return true;
bail:
	delete command;
	return false;
}
bool val_to_matrix_assignment(wstring * val, ::D3DXMATRIX *matrix, UINT start_slot, UINT num_slots, CommandList * command_list, const wstring * ini_namespace, MatrixAssignment **assignment_command)
{
	vector<wstring> vals;
	wchar_t * wcs = const_cast<wchar_t*>(val->c_str());
	wchar_t * pwc;
	wchar_t *buffer;
	pwc = wcstok_s(wcs, L",", &buffer);
	while (pwc != NULL)
	{
		vals.push_back(trimwhitespace(pwc));
		pwc = wcstok_s(NULL, L",", &buffer);
	}
	ConstantType const_type;
	wchar_t shader_type;
	int idx;
	UINT vector4Count;
	UINT i = start_slot;
	int end;
	UINT _start_slot, _num_slots;
	if (vals.size() == 0)
		return false;

	MatrixAssignment *command = new MatrixAssignment();
	for (auto &_val : vals)
	{
		if (i >= (num_slots + start_slot))
			break;
		int ret, len1, slot, num;
		wchar_t exp[MAX_PATH];
		wstring strExp;
		MatrixFromMatrixExpressionAssignment matrix_exp_command = MatrixFromMatrixExpressionAssignment();
		matrix_exp_command.pMatrix = matrix;
		matrix_exp_command.dst_start = i;
		UINT num_slots;
		ret = swscanf_s(_val.c_str(), L"(%ls)[%u]%u%n", exp, _countof(exp), &slot, &num, &len1);
		if (ret == 3 && ret != EOF && len1 == _val.length()) {
			matrix_exp_command.src_start = slot;
			num_slots = num;
			strExp = exp;
		}
		else {
			num_slots = 16;
			strExp = _val;
		}
		CommandListMatrixExpression expression;
		if (expression.parse(&strExp, ini_namespace, command_list->scope_matrices)) {
			matrix_exp_command.expression = expression;
			if (start_slot + i + num_slots > 16) {
				end = 16;
			}
			else {
				end = start_slot + i + num_slots;
			}
			matrix_exp_command.num_slots = (end - (start_slot + i));
			i += (end - (start_slot + i));
			command->matrix_expression_assignments.push_back(matrix_exp_command);
			continue;
		}
		CommandListMatrix *value_matrix;
		if (find_local_matrix(_val, command_list->scope_matrices, &value_matrix, &_start_slot, &_num_slots) || parse_command_list_matrix_name(_val, ini_namespace, &value_matrix, &_start_slot, &_num_slots)) {
			MatrixFromMatrixAssignment matrix_command = MatrixFromMatrixAssignment();
			matrix_command.dst_matrix = matrix;
			matrix_command.dst_start = i;
			matrix_command.src_matrix = &value_matrix->fmatrix;
			matrix_command.src_start = _start_slot;
			if (start_slot + i + _num_slots > 16) {
				end = 16;
			}
			else {
				end = start_slot + i + _num_slots;
			}
			matrix_command.num_slots = (end - (start_slot + i));
			i += (end - (start_slot + i));
			command->matrix_assignments.push_back(matrix_command);
			continue;
		}
		if (ParseShaderConstant(_val.c_str(), &shader_type, &const_type, &idx, &vector4Count)) {
			MatrixFromShaderConstantAssignment shader_command = MatrixFromShaderConstantAssignment();
			shader_command.shader_type = shader_type;
			shader_command.constant_type = const_type;
			shader_command.slot = idx;
			shader_command.num_slots = vector4Count;

			shader_command.pMatrix = matrix;
			shader_command.dst_start = i;
			if (start_slot + i + (vector4Count * 4) > 16) {
				end = 16;
			}
			else {
				end = start_slot + i + (vector4Count * 4);
			}
			shader_command.num_slots = (end - (start_slot + i));
			i += (end - (start_slot + i));
			command->shader_constant_assignments.push_back(shader_command);
			continue;
		}
		CommandListVariableArray *value_arr;
		if (find_local_variable_array(_val, command_list->scope_arrays, &value_arr, &_start_slot, &_num_slots) || parse_command_list_array_name(_val, ini_namespace, &value_arr, &_start_slot, &_num_slots)) {
			MatrixFromArrayAssignment array_command = MatrixFromArrayAssignment();
			array_command.pMatrix = matrix;
			array_command.dst_start = i;
			auto first = value_arr->fvals.begin() + _start_slot;
			if (_num_slots >= (num_slots - i))
				end = _start_slot + (num_slots - i);
			else
				end = _start_slot + _num_slots;
			auto last = value_arr->fvals.begin() + end;
			vector<float*> arr;
			arr.resize(end - _start_slot);
			transform(first, last, arr.begin(), getPointer<float>);
			array_command.arr = arr;
			command->array_assignments.push_back(array_command);
			i += (end - _start_slot);
			continue;
		}
		MatrixFromExpressionAssignment expression_command = MatrixFromExpressionAssignment();
		if (expression_command.expression.parse(&_val, ini_namespace, command_list)) {
			expression_command.pMatrix = matrix;
			expression_command.dst_start = i;
			command->expression_assignments.push_back(expression_command);
			i++;
			continue;
		}
		goto bail;
	}
	*assignment_command = command;
	return true;
bail:
	delete command;
	return false;
}
bool ParseCommandListMatrixAssignment(const wchar_t * section, const wchar_t * key, wstring * val, const wstring * raw_line, CommandList * command_list, CommandList * pre_command_list, CommandList * post_command_list, const wstring * ini_namespace)
{
	CommandListMatrix* matrix;
	wstring name = key;

	// Declaration without assignment?
	if (name.empty() && raw_line)
		name = *raw_line;

	if (!name.compare(0, 6, L"local ")) {
		name = name.substr(name.find_first_not_of(L" \t", 6));
		// Local variables are shared between pre and post command lists.
		wstring _name;
		if (!declare_local_matrix(section, name, pre_command_list, ini_namespace, &_name))
			return false;

		// Declaration without assignment?
		if (val->empty())
			return true;
		name = _name;
	}
	UINT startSlot, numSlots;
	if (!find_local_matrix(name, pre_command_list->scope_matrices, &matrix, &startSlot, &numSlots) &&
		!parse_command_list_matrix_name(name, ini_namespace, &matrix, &startSlot, &numSlots))
		return false;
	MatrixAssignment *command;
	if (!(val_to_matrix_assignment(val, &matrix->fmatrix, startSlot, numSlots, pre_command_list, ini_namespace, &command)))
		return false;

	command->ini_line = L"[" + wstring(section) + L"] " + wstring(key) + L" = " + *val;
	command_list->commands.push_back(std::shared_ptr<CommandListCommand>(command));
	return true;
}
bool ParseCommandListVariableArrayAssignment(const wchar_t * section, const wchar_t * key, wstring * val, const wstring * raw_line, CommandList * command_list, CommandList * pre_command_list, CommandList * post_command_list, const wstring * ini_namespace)
{
	CommandListVariableArray* vars;
	wstring name = key;

	// Declaration without assignment?
	if (name.empty() && raw_line)
		name = *raw_line;

	if (!name.compare(0, 6, L"local ")) {
		name = name.substr(name.find_first_not_of(L" \t", 6));
		// Local variables are shared between pre and post command lists.
		wstring _name;
		if (!declare_local_variable_array(section, name, pre_command_list, ini_namespace, &_name))
			return false;

		// Declaration without assignment?
		if (val->empty())
			return true;
		name = _name;
	}
	UINT startSlot, numSlots;
	if (!find_local_variable_array(name, pre_command_list->scope_arrays, &vars, &startSlot, &numSlots) &&
		!parse_command_list_array_name(name, ini_namespace, &vars, &startSlot, &numSlots))
		return false;
	VariableArrayAssignment *command;
	if (!(val_to_variable_array_assignment(val, &vars->fvals, startSlot, numSlots, pre_command_list, ini_namespace, &command)))
		return false;

	command->ini_line = L"[" + wstring(section) + L"] " + wstring(key) + L" = " + *val;
	command_list->commands.push_back(std::shared_ptr<CommandListCommand>(command));
	return true;
}

bool ParseCommandListVariableAssignment(const wchar_t *section,
	const wchar_t *key, wstring *val, const wstring *raw_line,
	CommandList *command_list, CommandList *pre_command_list, CommandList *post_command_list,
	const wstring *ini_namespace)
{
	VariableAssignment *command = NULL;
	CommandListVariableFloat *var = NULL;
	wstring name = key;

	// Declaration without assignment?
	if (name.empty() && raw_line)
		name = *raw_line;

	if (!name.compare(0, 6, L"local ")) {
		name = name.substr(name.find_first_not_of(L" \t", 6));
		// Local variables are shared between pre and post command lists.
		if (!declare_local_variable(section, name, pre_command_list, ini_namespace))
			return false;

		// Declaration without assignment?
		if (val->empty())
			return true;
	}

	if (!find_local_variable(name, pre_command_list->scope, &var) &&
		!parse_command_list_var_name(name, ini_namespace, &var))
		return false;

	command = new VariableAssignment();
	command->var = var;

	if (!command->expression.parse(val, ini_namespace, command_list))
		goto bail;

	command->ini_line = L"[" + wstring(section) + L"] " + wstring(key) + L" = " + *val;
	command_list->commands.push_back(std::shared_ptr<CommandListCommand>(command));
	return true;
bail:
	delete command;
	return false;
}
bool ParseHelixShaderOverrideSetSampler(const wchar_t *section,
	const wchar_t *key, wstring *val, const wstring *raw_line,
	CommandList *command_list,
	const wstring *ini_namespace, wchar_t shader_type)
{
	UINT ret, len1, reg;
	wchar_t samp;
	wstring name = key;
	CustomResources::iterator res;
	ResourceCopyOperation *command = new ResourceCopyOperation();
	ret = swscanf_s(name.c_str(), L"setsampler%lctoreg", &samp, 1, 16);
	if (ret != 0 && ret != EOF && 16 == name.length()) {
		ret = swscanf_s(val->c_str(), L"%u%n", &reg, &len1);
		if (ret != 0 && ret != EOF && len1 == val->length()) {
			command->options = ResourceCopyOptions::REFERENCE;
			command->dst.shader_type = shader_type;
			command->dst.type = ResourceCopyTargetType::SHADER_RESOURCE;
			if (shader_type == L'v')
				command->dst.slot = reg - 257;
			else
				command->dst.slot = reg;
			wstring resource_id(L"resourcehelixsampler" + wstring(1, samp));
			wstring namespaced_section;

			res = customResources.end();
			if (get_namespaced_section_name_lower(&resource_id, ini_namespace, &namespaced_section, &helix_ini))
				res = customResources.find(namespaced_section);
			if (res == customResources.end())
				res = customResources.find(resource_id);
			if (res == customResources.end()) {
				command->src.custom_resource = &customResources[resource_id];
				command->src.custom_resource->name = resource_id;
			}
			else {
				command->src.custom_resource = &res->second;
			}
			command->src.type = ResourceCopyTargetType::CUSTOM_RESOURCE;
			command->ini_line = L"[" + wstring(section) + L"] " + wstring(key) + L" = " + *val;
			command_list->commands.push_back(std::shared_ptr<CommandListCommand>(command));
			return true;
		}
	}
	delete command;
	return false;
}
bool ParseHelixShaderOverrideGetSampler(const wchar_t * section, const wchar_t * key, wstring * val, const wstring * raw_line, CommandList * command_list, const wstring * ini_namespace, wchar_t shader_type)
{
	UINT ret, len1, reg;
	wchar_t samp;
	wstring name = key;
	CustomResources::iterator res;
	ResourceCopyOperation *command = new ResourceCopyOperation();
	ret = swscanf_s(name.c_str(), L"getsampler%lcfromreg", &samp, 1, 18);
	if (ret != 0 && ret != EOF && 18 == name.length()) {
		ret = swscanf_s(val->c_str(), L"%u%n", &reg, &len1);
		if (ret != 0 && ret != EOF && len1 == val->length()) {
			command->options = ResourceCopyOptions::REFERENCE;
			command->src.shader_type = shader_type;
			command->src.type = ResourceCopyTargetType::SHADER_RESOURCE;
			if (shader_type == L'v')
				command->src.slot = reg - 257;
			else
				command->src.slot = reg;
			wstring resource_id(L"resourcehelixsampler" + wstring(1, samp));
			wstring namespaced_section;

			res = customResources.end();
			if (get_namespaced_section_name_lower(&resource_id, ini_namespace, &namespaced_section, &helix_ini))
				res = customResources.find(namespaced_section);
			if (res == customResources.end())
				res = customResources.find(resource_id);
			if (res == customResources.end()) {
				command->dst.custom_resource = &customResources[resource_id];
				command->dst.custom_resource->name = resource_id;
			}
			else {
				command->dst.custom_resource = &res->second;
			}
			command->dst.type = ResourceCopyTargetType::CUSTOM_RESOURCE;
			command->ini_line = L"[" + wstring(section) + L"] " + wstring(key) + L" = " + *val;
			command_list->commands.push_back(std::shared_ptr<CommandListCommand>(command));
			return true;
		}
	}
	delete command;
	return false;
}
bool ParseHelixShaderOverrideGetConstant(const wchar_t * section, const wchar_t * key, wstring * val, const wstring * raw_line, CommandList * command_list, const wstring * ini_namespace, wchar_t shader_type)
{
	UINT ret, len1, reg;
	wchar_t con;
	wstring name = key;

	VariableArrayFromShaderConstantAssignment *command = new VariableArrayFromShaderConstantAssignment();
	ret = swscanf_s(name.c_str(), L"getconst%lcfromreg", &con, 1, 16);
	if (ret != 0 && ret != EOF && 16 == name.length()) {
		command->shader_type = shader_type;
		command->constant_type = ConstantType::FLOAT;
		ret = swscanf_s(val->c_str(), L"%u%n", &reg, &len1);
		if (ret != 0 && ret != EOF && len1 == val->length()) {
			command->slot = reg;
			command->vars.assign(4, 0);
			wstring const_id(L"$helix_const" + wstring(1, con));
			CommandListVariableArrays::iterator it = command_list_global_arrays.find(const_id);
			if (it == command_list_global_arrays.end()) {
				std::pair<CommandListVariableArrays::iterator, bool> inserted_array = command_list_global_arrays.emplace(const_id, CommandListVariableArray{ const_id, vector<float>(), VariableFlags() });
				if (!(inserted_array.second))
					goto bail;
				else {
					inserted_array.first->second.fvals.assign(4, 0);
					transform(inserted_array.first->second.fvals.begin(), inserted_array.first->second.fvals.end(), command->vars.begin(), getPointer<float>);
				}
			}
			else {
				transform(it->second.fvals.begin(), it->second.fvals.end(), command->vars.begin(), getPointer<float>);
			}
			command->ini_line = L"[" + wstring(section) + L"] " + wstring(key) + L" = " + *val;
			command_list->commands.push_back(std::shared_ptr<CommandListCommand>(command));
			return true;
		}
	}
bail:
	delete command;
	return false;
}
bool ParseHelixShaderOverrideSetConstant(const wchar_t * section, const wchar_t * key, wstring * val, const wstring * raw_line, CommandList * command_list, const wstring * ini_namespace, wchar_t shader_type)
{
	UINT ret, len1, reg;
	wchar_t con;
	wstring name = key;

	SetShaderConstant *command = new SetShaderConstant();
	VariableArrayAssignment *assignment = new VariableArrayAssignment();
	ret = swscanf_s(name.c_str(), L"setconst%lctoreg", &con, 1, 14);
	if (ret != 0 && ret != EOF && 14 == name.length()) {
		command->shader_type = shader_type;
		command->constant_type = ConstantType::FLOAT;
		ret = swscanf_s(val->c_str(), L"%u%n", &reg, &len1);
		if (ret != 0 && ret != EOF && len1 == val->length()) {
			command->slot = reg;
			command->vars.assign(4, 0);
			VariableArrayFromArrayAssignment array_to_array;
			wstring const_id(L"$helix_const" + wstring(1, con));
			CommandListVariableArrays::iterator it = command_list_global_arrays.find(const_id);
			if (it == command_list_global_arrays.end()) {
				std::pair<CommandListVariableArrays::iterator, bool> inserted_array = command_list_global_arrays.emplace(const_id, CommandListVariableArray{ const_id, vector<float>(), VariableFlags() });
				if (!(inserted_array.second))
					goto bail;
				else {
					inserted_array.first->second.fvals.assign(4, 0);
					int i = 0;
					for (auto &val : inserted_array.first->second.fvals) {
						array_to_array.map[&val] = &command->vars[i];
						++i;
					}
				}
			}
			else {
				int i = 0;
				for (auto &val : it->second.fvals) {
					array_to_array.map[&val] = &command->vars[i];
					++i;
				}
			}
			assignment->array_assignments.push_back(array_to_array);
			command->assign = assignment;
			command->ini_line = L"[" + wstring(section) + L"] " + wstring(key) + L" = " + *val;
			command_list->commands.push_back(std::shared_ptr<CommandListCommand>(command));
			return true;
		}
	}
bail:
	delete command;
	delete assignment;
	return false;
}
ResourcePool::ResourcePool()
{

}

ResourcePool::~ResourcePool()
{
	unordered_map<
		pair<uint32_t, D3D9Wrapper::IDirect3DDevice9*>,
		HashedResource,
		pair_hash
	>::iterator i;

	for (i = cache.begin(); i != cache.end(); i++) {
		if (i->second.resource) {
			if (i->first.second)
				i->first.second->migotoResourceCount--;
			i->second.resource->Release();
		}
	}
	cache.clear();
}

void ResourcePool::emplace(uint32_t hash, HashedResource hashedResource, D3D9Wrapper::IDirect3DDevice9 *mHackerDevice)
{
	if (hashedResource.resource){
		hashedResource.resource->AddRef();
		if (mHackerDevice) {
			mHackerDevice->migotoResourceCount++;
		}
		else {
			LogInfo("Hacker Device was null when emplacing cached resource\n");
		}
	}
	cache.emplace(make_pair(hash, mHackerDevice), hashedResource);
}
template <typename DescType>
	static bool GetResourceFromPool(
		IDirect3DResource9 *dst_resource,
		ResourcePool *resource_pool,
		DescType *desc,
		D3D9Wrapper::IDirect3DDevice9 *hackerDevice,
		IDirect3DResource9 **cached_resource,
		uint32_t *hash,
		ResourceCreationInfo *info = &ResourceCreationInfo())
{
	ResourcePoolCache::iterator pool_i;
	HashedResource hashedResource;
	pair<DescType, ResourceCreationInfo> hashPair;
	hashPair = make_pair(*desc, *info);
	// We don't want to use the CalTexture2D/3DDescHash functions because
	// the resolution override could produce the same hash for distinct
	// texture descriptions. This hash isn't exposed to the user, so
	// doesn't matter what we use - just has to be fast.
	*hash = crc32c_hw(0, &hashPair, sizeof(pair<DescType, ResourceCreationInfo>));
	pool_i = Profiling::lookup_map(resource_pool->cache, make_pair(*hash, hackerDevice), &Profiling::resource_pool_lookup_overhead);
	if (pool_i != resource_pool->cache.end()) {
		hashedResource = pool_i->second;
		if (hashedResource.resource == dst_resource) {
			*cached_resource = NULL;
		}
		if (hashedResource.resource) {
			LogDebug("Switching cached resource \n");
			Profiling::resource_pool_swaps++;
			hashedResource.resource->AddRef();
			*cached_resource = hashedResource.resource;
		}
	}
	else {
		*cached_resource = NULL;
		return true;
	}
	return false;
}
template <typename SourceResourceType, typename DestResourceType, typename DescType>
static DestResourceType* GetResourceFromPool(
	wstring *ini_line,
	SourceResourceType *src_resource,
	DestResourceType *dst_resource,
	ResourcePool *resource_pool,
	CommandListState *state,
	DescType *desc,
	bool override_create_mode = false,
	StereoHandle stereo_handle = NULL,
	HANDLE *pHandle = NULL,
	ResourceCreationInfo *info = &ResourceCreationInfo())
{
	DestResourceType *resource = NULL;
	DescType old_desc;
	uint32_t hash;
	size_t size;
	HRESULT hr;

	bool newResource = GetResourceFromPool<DescType>(dst_resource, resource_pool, desc, state->mHackerDevice, reinterpret_cast<IDirect3DResource9**>(&resource), &hash, info);
	if (!newResource) {
		if (resource)
			LogDebug(" %S\n", ini_line->c_str());
	}else{
		Profiling::resources_created++;
		LogInfo("Creating cached resource %S\n", ini_line->c_str());
		bool restore_create_mode = false;
		HashedResource newHashedResource;
		NVAPI_STEREO_SURFACECREATEMODE orig_mode;
		if (override_create_mode && G->gForceStereo != 2) {
			Profiling::NvAPI_Stereo_GetSurfaceCreationMode(stereo_handle, &orig_mode);
			if (orig_mode != info->mode) {
				Profiling::NvAPI_Stereo_SetSurfaceCreationMode(stereo_handle, info->mode);
				restore_create_mode = true;
			}
		}
		hr = CreateResource(state->mOrigDevice, desc, pHandle, &resource);
		if (G->gForceStereo == 2 && !FAILED(hr)) {
			HRESULT dmHR = DirectModeStereoizeResource(state, desc, pHandle, &resource, info->mode, &newHashedResource);
			if (FAILED(dmHR)) {
				LogInfo("Direct Mode resource not stereoized %S: 0x%x\n", ini_line->c_str(), dmHR);
			}
		}
		if (restore_create_mode && G->gForceStereo != 2) {
			Profiling::NvAPI_Stereo_SetSurfaceCreationMode(stereo_handle, orig_mode);
		}
		if (FAILED(hr)) {
			LogInfo("Resource copy failed %S: 0x%x\n", ini_line->c_str(), hr);
			LogResourceDesc(desc);
			GetResourceDesc(src_resource, &old_desc);
			LogInfo("Original resource was:\n");
			LogResourceDesc(&old_desc);
			// Prevent further attempts:
			newHashedResource.resource = NULL;
			resource_pool->emplace(hash, newHashedResource, state->mHackerDevice);
			return NULL;
		}
		newHashedResource.resource = resource;
		resource_pool->emplace(hash, newHashedResource, state->mHackerDevice);
		size = resource_pool->cache.size();
		if (size > 1)
			LogInfo("  NOTICE: cache now contains %Ii resources\n", size);
		LogDebugResourceDesc(desc);
	}
	return resource;
}
CustomResource::CustomResource() :
	resource(NULL),
	is_null(true),
	substantiated(false),
	usage_flags(0),
	stride(0),
	offset(0),
	buf_size(0),
	format(D3DFMT_UNKNOWN),
	max_copies_per_frame(0),
	frame_no(0),
	copies_this_frame(0),
	override_type(CustomResourceType::INVALID),
	override_mode(CustomResourceMode::DEFAULT),
	override_usage_flags(CustomResourceUsageFlags::INVALID),
	override_format((D3DFORMAT)-1),
	override_width(-1),
	override_height(-1),
	override_width_expression(NULL),
	override_depth_expression(NULL),
	override_height_expression(NULL),
	override_depth(-1),
	override_mips(-1),
	override_msaa(-1),
	override_msaa_quality(-1),
	override_byte_width(-1),
	override_stride(-1),
	width_multiply(1.0f),
	height_multiply(1.0f),
	initial_data(NULL),
	initial_data_size(0)
{
}

CustomResource::~CustomResource()
{
	if (override_height_expression)
		override_height_expression->~CommandListExpression();
	if (override_width_expression)
		override_width_expression->~CommandListExpression();
	if (override_depth_expression)
		override_depth_expression->~CommandListExpression();

	EnterCriticalSection(&G->mCriticalSection);
	if (resource)
		resource->Release();
	LeaveCriticalSection(&G->mCriticalSection);
	free(initial_data);
}
bool CustomResource::OverrideSurfaceCreationMode(StereoHandle mStereoHandle, NVAPI_STEREO_SURFACECREATEMODE *new_mode)
{

	if (override_mode == CustomResourceMode::DEFAULT)
		return false;
	switch (override_mode) {
	case CustomResourceMode::STEREO:
		*new_mode = NVAPI_STEREO_SURFACECREATEMODE_FORCESTEREO;
		return true;
	case CustomResourceMode::MONO:
		*new_mode = NVAPI_STEREO_SURFACECREATEMODE_FORCEMONO;
		return true;
	case CustomResourceMode::AUTO:
		*new_mode = NVAPI_STEREO_SURFACECREATEMODE_AUTO;
		return true;
	}

	return false;
}

void CustomResource::Substantiate(CommandListState *state, StereoHandle mStereoHandle, DWORD usage_flags)
{
	NVAPI_STEREO_SURFACECREATEMODE new_mode = NVAPI_STEREO_SURFACECREATEMODE_AUTO;
	NVAPI_STEREO_SURFACECREATEMODE orig_mode = NVAPI_STEREO_SURFACECREATEMODE_AUTO;
	bool override_mode = false;
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
	if (resource)
		return;

	Profiling::resources_created++;

	// Add any extra bind flags necessary for the current assignment. This
	// covers many (but not all) of the cases 3DMigoto cannot deduce
	// earlier during parsing - it will cover custom resources copied by
	// reference to other custom resources when the "this" resource target
	// is assigned. There are some complicated cases that could still need
	// bind_flags to be manually specified - where multiple bind flags are
	// required and cannot be deduced at parse time.
	this->usage_flags = (DWORD)(this->usage_flags | usage_flags);

	// If the resource section has enough information to create a resource
	// we do so the first time it is loaded from. The reason we do it this
	// late is to make sure we know which device is actually being used to
	// render the game - FC4 creates about a dozen devices with different
	// parameters while probing the hardware before it settles on the one
	// it will actually use.

	override_mode = OverrideSurfaceCreationMode(mStereoHandle, &new_mode);
	if (override_mode) {
		Profiling::NvAPI_Stereo_GetSurfaceCreationMode(mStereoHandle, &orig_mode);
		if (orig_mode != new_mode) {
			Profiling::NvAPI_Stereo_SetSurfaceCreationMode(mStereoHandle, new_mode);
			restore_create_mode = true;
		}
	}
	if (!filename.empty()) {
		LoadFromFile(state->mOrigDevice);
	}
	else {
		switch (override_type) {
		case CustomResourceType::VERTEXBUFFER:
			SubstantiateBuffer<IDirect3DVertexBuffer9, D3DVERTEXBUFFER_DESC>(state->mOrigDevice, NULL, 0);
		case CustomResourceType::INDEXBUFFER:
			SubstantiateBuffer<IDirect3DIndexBuffer9, D3DINDEXBUFFER_DESC>(state->mOrigDevice, NULL, 0);
			break;
		case CustomResourceType::TEXTURE2D:
			SubstantiateTexture2D(state);
		case CustomResourceType::CUBE:
			SubstantiateTextureCube(state);
			break;
		case CustomResourceType::TEXTURE3D:
			SubstantiateTexture3D(state);
			break;
		case CustomResourceType::RENDERTARGET:
			SubstantiateRenderTarget(state);
			break;
		case CustomResourceType::OFFSCREENPLAIN:
			SubstantiateOffscreenPlain(state);
			break;
		case CustomResourceType::DEPTHSTENCILSURFACE:
			SubstantiateDepthStencilSurface(state);
			break;
		}
	}

	if (restore_create_mode)
		Profiling::NvAPI_Stereo_SetSurfaceCreationMode(mStereoHandle, orig_mode);
}
template<typename ID3D9Buffer, typename ID3D9BufferDesc>
void CustomResource::LoadBufferFromFile(IDirect3DDevice9 *mOrigDevice)
{
	DWORD size, read_size;
	void *buf = NULL;
	HANDLE f;

	f = CreateFile(filename.c_str(), GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (f == INVALID_HANDLE_VALUE) {
		LogOverlay(LOG_WARNING, "Failed to load custom buffer resource %S: %d\n", filename.c_str(), GetLastError());
		return;
	}

	size = GetFileSize(f, 0);
	buf = malloc(size); // malloc to allow realloc to resize it if the user overrode the size
	if (!buf) {
		LogOverlay(LOG_DIRE, "Out of memory loading %S\n", filename.c_str());
		goto out_close;
	}

	if (!ReadFile(f, buf, size, &read_size, 0) || size != read_size) {
		LogOverlay(LOG_WARNING, "Error reading custom buffer from file %S\n", filename.c_str());
		goto out_delete;
	}

	SubstantiateBuffer<ID3D9Buffer, ID3D9BufferDesc>(mOrigDevice, &buf, size);

out_delete:
	free(buf);
out_close:
	CloseHandle(f);
}
void CustomResource::LoadFromFile(IDirect3DDevice9 *mOrigDevice)
{
	HRESULT hr;
	D3DXIMAGE_INFO image_info;
	if (override_type == CustomResourceType::VERTEXBUFFER)
		return LoadBufferFromFile<IDirect3DVertexBuffer9, D3DVERTEXBUFFER_DESC>(mOrigDevice);

	if (override_type == CustomResourceType::INDEXBUFFER)
		return LoadBufferFromFile<IDirect3DIndexBuffer9, D3DINDEXBUFFER_DESC>(mOrigDevice);

	if (override_usage_flags != CustomResourceUsageFlags::INVALID)
		usage_flags |= (DWORD)override_usage_flags;

	LogInfoW(L"Loading custom resource %s\n", filename.c_str());
	switch (override_type) {
	case CustomResourceType::TEXTURE2D:
		hr = D3DXCreateTextureFromFileEx(mOrigDevice, filename.c_str(), D3DX_DEFAULT, D3DX_DEFAULT, D3DX_FROM_FILE, usage_flags, D3DFMT_UNKNOWN, override_pool, D3DX_DEFAULT, D3DX_DEFAULT, 0, NULL, NULL, (LPDIRECT3DTEXTURE9*)&resource);
		break;
	case CustomResourceType::TEXTURE3D:
		hr = D3DXCreateVolumeTextureFromFileEx(mOrigDevice, filename.c_str(), D3DX_DEFAULT, D3DX_DEFAULT, D3DX_DEFAULT, D3DX_FROM_FILE, usage_flags, D3DFMT_UNKNOWN, override_pool, D3DX_DEFAULT, D3DX_DEFAULT, 0, NULL, NULL, (LPDIRECT3DVOLUMETEXTURE9*)&resource);
		break;
	case CustomResourceType::CUBE:
		hr = D3DXCreateCubeTextureFromFileEx(mOrigDevice, filename.c_str(), D3DX_DEFAULT, D3DX_FROM_FILE, usage_flags, D3DFMT_UNKNOWN, override_pool, D3DX_DEFAULT, D3DX_DEFAULT, 0, NULL, NULL, (LPDIRECT3DCUBETEXTURE9*)&resource);
		break;
	case CustomResourceType::DEPTHSTENCILSURFACE:
		hr = D3DXGetImageInfoFromFile(filename.c_str(), &image_info);
		if (!FAILED(hr)) {
			hr = mOrigDevice->CreateDepthStencilSurface(image_info.Width, image_info.Height, image_info.Format, D3DMULTISAMPLE_NONE, 0, (usage_flags & D3DUSAGE_DYNAMIC), (LPDIRECT3DSURFACE9*)&resource, NULL);
			if (!FAILED(hr))
				hr = D3DXLoadSurfaceFromFile((LPDIRECT3DSURFACE9)resource, NULL, NULL, filename.c_str(), NULL, D3DX_DEFAULT, 0, NULL);
		}
		break;
	case CustomResourceType::RENDERTARGET:
		hr = D3DXGetImageInfoFromFile(filename.c_str(), &image_info);
		if (!FAILED(hr)) {
			hr = mOrigDevice->CreateRenderTarget(image_info.Width, image_info.Height, image_info.Format, D3DMULTISAMPLE_NONE, 0, (usage_flags & D3DUSAGE_DYNAMIC), (LPDIRECT3DSURFACE9*)&resource, NULL);
			if (!FAILED(hr))
				hr = D3DXLoadSurfaceFromFile((LPDIRECT3DSURFACE9)resource, NULL, NULL, filename.c_str(), NULL, D3DX_DEFAULT, 0, NULL);
		}
		break;
	case CustomResourceType::OFFSCREENPLAIN:
		hr = D3DXGetImageInfoFromFile(filename.c_str(), &image_info);
		if (!FAILED(hr)) {
			hr = mOrigDevice->CreateOffscreenPlainSurface(image_info.Width, image_info.Height, image_info.Format, override_pool, (LPDIRECT3DSURFACE9*)&resource, NULL);
			if (!FAILED(hr))
				hr = D3DXLoadSurfaceFromFile((LPDIRECT3DSURFACE9)resource, NULL, NULL, filename.c_str(), NULL, D3DX_DEFAULT, 0, NULL);
		}
	}

	if (SUCCEEDED(hr)) {
		is_null = false;
		// TODO:
		// format = ...
	}
	else
		LogOverlay(LOG_WARNING, "Failed to load custom resource %S: 0x%x\n", filename.c_str(), hr);
}

static HRESULT _CreateCustomBuffer(IDirect3DVertexBuffer9 *buffer, D3DVERTEXBUFFER_DESC *desc, IDirect3DDevice9 *device) {
	return device->CreateVertexBuffer(desc->Size, desc->Usage, 0, desc->Pool, &buffer, NULL);
}
static HRESULT _CreateCustomBuffer(IDirect3DIndexBuffer9 *buffer, D3DINDEXBUFFER_DESC *desc, IDirect3DDevice9 *device) {
	return device->CreateIndexBuffer(desc->Size, desc->Usage, desc->Format, desc->Pool, &buffer, NULL);
}
template<typename ID3D9Buffer, typename ID3D9BufferDesc>
void CustomResource::SubstantiateBuffer(IDirect3DDevice9 *mOrigDevice, void **buf, DWORD size)
{
	void* data = { 0 }, *pInitialData = NULL;
	ID3D9Buffer *buffer = NULL;
	ID3D9BufferDesc desc;
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
	desc.Usage = usage_flags;
	desc.Format = D3DFMT_UNKNOWN;
	desc.Pool = D3DPOOL_DEFAULT;

	// Allow the buffer size to be set from the file / initial data size,
	// but it can be overridden if specified explicitly. If it's a
	// structured buffer, we assume just a single structure by default, but
	// again this can be overridden. The reason for doing this here, and
	// not in OverrideBufferDesc, is that this only applies if we are
	// substantiating the resource from scratch, not when copying a resource.
	if (size) {
		desc.Size = size;
	}

	OverrideBufferDesc(&desc);

	if (desc.Size > 0) {
		// Fill in size from the file/initial data, allowing for an
		// override to make it larger or smaller, which may involve
		// reallocating the buffer from the caller.
		if (desc.Size > size) {
			void *new_buf = realloc(*buf, desc.Size);
			if (!new_buf) {
				LogInfo("Out of memory enlarging buffer: [%S]\n", name.c_str());
				return;
			}
			memset((char*)new_buf + size, 0, desc.Size - size);
			*buf = new_buf;
		}

		data = *buf;
		pInitialData = &data;
	}

	hr = _CreateCustomBuffer(buffer, &desc, mOrigDevice);
	VOID* pVoid;
	HRESULT buffLock = buffer->Lock(0, 0, (void**)&pVoid, 0);
	if (FAILED(buffLock)) {
		LogInfo("Failed to lock buffer for initial data %S [%S]: 0x%x\n",
			lookup_enum_name(CustomResourceTypeNames, override_type), name.c_str(), hr);
		LogResourceDesc(&desc);

	}
	std::memcpy(pVoid, pInitialData, sizeof(pInitialData));
	HRESULT buffUnlock = buffer->Unlock();
	if (FAILED(buffUnlock)) {

		LogInfo("Failed to unlock buffer for initial data %S [%S]: 0x%x\n",
			lookup_enum_name(CustomResourceTypeNames, override_type), name.c_str(), hr);
		LogResourceDesc(&desc);
	}

	if (SUCCEEDED(hr)) {
		LogInfo("Substantiated custom %S [%S], usage_flags=0x%03x\n",
			lookup_enum_name(CustomResourceTypeNames, override_type), name.c_str(), desc.Usage);
		LogDebugResourceDesc(&desc);
		resource = (ID3D9Buffer*)buffer;
		is_null = false;
		OverrideOutOfBandInfo(&format, NULL);
	}
	else {
		LogOverlay(LOG_NOTICE, "Failed to substantiate custom %S [%S]: 0x%x\n",
			lookup_enum_name(CustomResourceTypeNames, override_type), name.c_str(), hr);
		LogResourceDesc(&desc);
	}
}
void CustomResource::SubstantiateRenderTarget(CommandListState *state)
{
	IDirect3DSurface9 *renderTarget;
	D3D2DTEXTURE_DESC desc;
	HRESULT hr;
	memset(&desc, 0, sizeof(desc));
	desc.Format = D3DFMT_UNKNOWN;
	OverrideSurfacesDesc(&desc, state);
	hr = state->mOrigDevice->CreateRenderTarget(desc.Width, desc.Height, desc.Format, desc.MultiSampleType, desc.MultiSampleQuality, (desc.Usage & D3DUSAGE_DYNAMIC), &renderTarget, NULL);
	if (SUCCEEDED(hr)) {
		LogInfo("Substantiated custom %S [%S], usage_flags=0x%03x\n",
			lookup_enum_name(CustomResourceTypeNames, override_type), name.c_str(), desc.Usage);
		LogDebugResourceDesc(&desc);
		resource = (IDirect3DSurface9*)renderTarget;
		is_null = false;
	}
	else {
		LogOverlay(LOG_NOTICE, "Failed to substantiate custom %S [%S]: 0x%x\n",
			lookup_enum_name(CustomResourceTypeNames, override_type), name.c_str(), hr);
		LogResourceDesc(&desc);
	}
}
void CustomResource::SubstantiateDepthStencilSurface(CommandListState *state)
{
	IDirect3DSurface9 *depthStencil;
	D3D2DTEXTURE_DESC desc;
	HRESULT hr;
	memset(&desc, 0, sizeof(desc));
	desc.Format = D3DFMT_UNKNOWN;
	OverrideSurfacesDesc(&desc, state);
	hr = state->mOrigDevice->CreateDepthStencilSurface(desc.Width, desc.Height, desc.Format, desc.MultiSampleType, desc.MultiSampleQuality, (desc.Usage & D3DUSAGE_DYNAMIC), &depthStencil, NULL);
	if (SUCCEEDED(hr)) {
		LogInfo("Substantiated custom %S [%S], usage_flags=0x%03x\n",
			lookup_enum_name(CustomResourceTypeNames, override_type), name.c_str(), desc.Usage);
		LogDebugResourceDesc(&desc);
		resource = (IDirect3DSurface9*)depthStencil;
		is_null = false;
	}
	else {
		LogOverlay(LOG_NOTICE, "Failed to substantiate custom %S [%S]: 0x%x\n",
			lookup_enum_name(CustomResourceTypeNames, override_type), name.c_str(), hr);
		LogResourceDesc(&desc);
	}
}
void CustomResource::SubstantiateOffscreenPlain(CommandListState *state)
{
	IDirect3DSurface9 *offscreenPlain;
	D3D2DTEXTURE_DESC desc;
	HRESULT hr;
	memset(&desc, 0, sizeof(desc));
	desc.Pool = D3DPOOL_DEFAULT;
	desc.Format = D3DFMT_UNKNOWN;
	OverrideSurfacesDesc(&desc, state);
	hr = state->mOrigDevice->CreateOffscreenPlainSurface(desc.Width, desc.Height, desc.Format, desc.Pool, &offscreenPlain, NULL);
	if (SUCCEEDED(hr)) {
		LogInfo("Substantiated custom %S [%S], usage_flags=0x%03x\n",
			lookup_enum_name(CustomResourceTypeNames, override_type), name.c_str(), desc.Usage);
		LogDebugResourceDesc(&desc);
		resource = (IDirect3DSurface9*)offscreenPlain;
		is_null = false;
	}
	else {
		LogOverlay(LOG_NOTICE, "Failed to substantiate custom %S [%S]: 0x%x\n",
			lookup_enum_name(CustomResourceTypeNames, override_type), name.c_str(), hr);
		LogResourceDesc(&desc);
	}
}
void CustomResource::SubstantiateTexture2D(CommandListState *state)
{
	IDirect3DTexture9 *tex2d;
	D3D2DTEXTURE_DESC desc;
	HRESULT hr;

	memset(&desc, 0, sizeof(desc));
	desc.Usage = usage_flags;
	desc.Pool = D3DPOOL_DEFAULT;
	desc.Format = D3DFMT_UNKNOWN;
	desc.Levels = 1;
	OverrideSurfacesDesc(&desc, state);
	hr = state->mOrigDevice->CreateTexture(desc.Width, desc.Height, desc.Levels, desc.Usage, desc.Format, desc.Pool, &tex2d, NULL);
	if (SUCCEEDED(hr)) {
		LogInfo("Substantiated custom %S [%S], usage_flags=0x%03x\n",
			lookup_enum_name(CustomResourceTypeNames, override_type), name.c_str(), desc.Usage);
		LogDebugResourceDesc(&desc);
		resource = (IDirect3DTexture9*)tex2d;
		is_null = false;
	}
	else {
		LogOverlay(LOG_NOTICE, "Failed to substantiate custom %S [%S]: 0x%x\n",
			lookup_enum_name(CustomResourceTypeNames, override_type), name.c_str(), hr);
		LogResourceDesc(&desc);
	}
}
void CustomResource::SubstantiateTexture3D(CommandListState *state)
{
	IDirect3DVolumeTexture9 *tex3d;
	D3D3DTEXTURE_DESC desc;
	HRESULT hr;

	memset(&desc, 0, sizeof(desc));
	desc.Usage = usage_flags;
	desc.Pool = D3DPOOL_DEFAULT;
	desc.Format = D3DFMT_UNKNOWN;
	desc.Levels = 1;
	OverrideSurfacesDesc(&desc, state);

	hr = state->mOrigDevice->CreateVolumeTexture(desc.Width, desc.Height, desc.Depth, desc.Levels, desc.Usage, desc.Format, desc.Pool, &tex3d, NULL);
	if (SUCCEEDED(hr)) {
		LogInfo("Substantiated custom %S [%S], usage_flags=0x%03x\n",
			lookup_enum_name(CustomResourceTypeNames, override_type), name.c_str(), desc.Usage);
		LogDebugResourceDesc(&desc);
		resource = (IDirect3DVolumeTexture9*)tex3d;
		is_null = false;
	}
	else {
		LogOverlay(LOG_NOTICE, "Failed to substantiate custom %S [%S]: 0x%x\n",
			lookup_enum_name(CustomResourceTypeNames, override_type), name.c_str(), hr);
		LogResourceDesc(&desc);
	}
}
void CustomResource::SubstantiateTextureCube(CommandListState *state)
{
	IDirect3DCubeTexture9 *texCube;
	D3D2DTEXTURE_DESC desc;
	HRESULT hr;

	memset(&desc, 0, sizeof(desc));
	desc.Usage = usage_flags;
	desc.Pool = D3DPOOL_DEFAULT;
	desc.Format = D3DFMT_UNKNOWN;
	desc.Levels = 1;
	OverrideSurfacesDesc(&desc, state);

	hr = state->mOrigDevice->CreateCubeTexture(desc.Width, desc.Levels, desc.Usage, desc.Format, desc.Pool, &texCube, NULL);
	if (SUCCEEDED(hr)) {
		LogInfo("Substantiated custom %S [%S], usage_flags=0x%03x\n",
			lookup_enum_name(CustomResourceTypeNames, override_type), name.c_str(), desc.Usage);
		LogDebugResourceDesc(&desc);
		resource = (IDirect3DCubeTexture9*)texCube;
		is_null = false;
	}
	else {
		LogOverlay(LOG_NOTICE, "Failed to substantiate custom %S [%S]: 0x%x\n",
			lookup_enum_name(CustomResourceTypeNames, override_type), name.c_str(), hr);
		LogResourceDesc(&desc);
	}
}
HRESULT CreateResource(IDirect3DDevice9 *device, D3DVERTEXBUFFER_DESC *desc, HANDLE *pSharedHandle, IDirect3DVertexBuffer9 **ppResource) {
	return device->CreateVertexBuffer(desc->Size, desc->Usage, desc->FVF, desc->Pool, ppResource, pSharedHandle);
}
HRESULT CreateResource(IDirect3DDevice9 *device, D3DINDEXBUFFER_DESC *desc, HANDLE *pSharedHandle, IDirect3DIndexBuffer9 **ppResource) {
	return device->CreateIndexBuffer(desc->Size, desc->Usage, desc->Format, desc->Pool, ppResource, pSharedHandle);
}
HRESULT CreateResource(IDirect3DDevice9 *device, D3D2DTEXTURE_DESC *desc, HANDLE *pSharedHandle, IDirect3DTexture9 **ppResource) {
	return device->CreateTexture(desc->Width, desc->Height, desc->Levels, desc->Usage, desc->Format, desc->Pool, ppResource, pSharedHandle);
}
HRESULT CreateResource(IDirect3DDevice9 *device, D3D3DTEXTURE_DESC *desc, HANDLE *pSharedHandle, IDirect3DVolumeTexture9 **ppResource) {
	return  device->CreateVolumeTexture(desc->Width, desc->Height, desc->Depth, desc->Levels, desc->Usage, desc->Format, desc->Pool, ppResource, pSharedHandle);
}
HRESULT CreateResource(IDirect3DDevice9 *device, D3D2DTEXTURE_DESC *desc, HANDLE *pSharedHandle, IDirect3DCubeTexture9 **ppResource) {
	return  device->CreateCubeTexture(desc->Width, desc->Levels, desc->Usage, desc->Format, desc->Pool, ppResource, pSharedHandle);
}
HRESULT CreateResource(IDirect3DDevice9 *device, D3D2DTEXTURE_DESC *desc, HANDLE *pSharedHandle, IDirect3DSurface9 **ppResource) {

	if (desc->Usage & D3DUSAGE_RENDERTARGET)
		return device->CreateRenderTarget(desc->Width, desc->Height, desc->Format, desc->MultiSampleType, desc->MultiSampleQuality, (desc->Usage & D3DUSAGE_DYNAMIC), ppResource, pSharedHandle);
	else if (desc->Usage & D3DUSAGE_DEPTHSTENCIL)
		return device->CreateDepthStencilSurface(desc->Width, desc->Height, desc->Format, desc->MultiSampleType, desc->MultiSampleQuality, (desc->Usage & D3DUSAGE_DYNAMIC), ppResource, pSharedHandle);
	else
		return device->CreateOffscreenPlainSurface(desc->Width, desc->Height, desc->Format, desc->Pool, ppResource, pSharedHandle);
}
HRESULT DirectModeStereoizeResource(CommandListState *state, D3D2DTEXTURE_DESC *pDesc, HANDLE *pHandle, IDirect3DSurface9 **wrappedResource, NVAPI_STEREO_SURFACECREATEMODE createMode, HashedResource *hashedResource) {
	HRESULT hr = D3D_OK;
	if (createMode != NVAPI_STEREO_SURFACECREATEMODE::NVAPI_STEREO_SURFACECREATEMODE_FORCEMONO && state->mHackerDevice->HackerDeviceShouldDuplicateSurface(pDesc)) {
		IDirect3DSurface9 *rightSurface;
		hr = CreateResource(state->mOrigDevice, pDesc, pHandle, &rightSurface);
		D3D9Wrapper::IDirect3DSurface9 *wrappedStereoSurface;
		if (!FAILED(hr)) {
			wrappedStereoSurface = D3D9Wrapper::IDirect3DSurface9::GetDirect3DSurface9(*wrappedResource, state->mHackerDevice, rightSurface);
		}
		else {
			wrappedStereoSurface = D3D9Wrapper::IDirect3DSurface9::GetDirect3DSurface9(*wrappedResource, state->mHackerDevice, NULL);
		}
		*wrappedResource = (IDirect3DSurface9*)wrappedStereoSurface;
	}
	else {
		D3D9Wrapper::IDirect3DSurface9 *wrappedStereoSurface = D3D9Wrapper::IDirect3DSurface9::GetDirect3DSurface9(*wrappedResource, state->mHackerDevice, NULL);
		*wrappedResource = (IDirect3DSurface9*)wrappedStereoSurface;
	}
	return hr;
}
HRESULT DirectModeStereoizeResource(CommandListState *state, D3D2DTEXTURE_DESC *pDesc, HANDLE *pHandle, IDirect3DTexture9 **wrappedResource, NVAPI_STEREO_SURFACECREATEMODE createMode, HashedResource *hashedResource) {

	if (createMode != NVAPI_STEREO_SURFACECREATEMODE::NVAPI_STEREO_SURFACECREATEMODE_FORCEMONO && state->mHackerDevice->HackerDeviceShouldDuplicateSurface(pDesc)) {
		HRESULT hr;
		IDirect3DTexture9 *rightTexture;
		hr = CreateResource(state->mOrigDevice, pDesc, pHandle, &rightTexture);
		D3D9Wrapper::IDirect3DTexture9 *wrappedStereoTexture;
		if (!FAILED(hr)) {
			wrappedStereoTexture = D3D9Wrapper::IDirect3DTexture9::GetDirect3DTexture9(*wrappedResource, state->mHackerDevice, rightTexture);
		}
		else {
			wrappedStereoTexture = D3D9Wrapper::IDirect3DTexture9::GetDirect3DTexture9(*wrappedResource, state->mHackerDevice, NULL);
		}
		*wrappedResource = (IDirect3DTexture9*)wrappedStereoTexture;
		return hr;
	}
	else {
		return E_NOTIMPL;
	}

}
HRESULT DirectModeStereoizeResource(CommandListState *state, D3D2DTEXTURE_DESC *pDesc, HANDLE *pHandle, IDirect3DCubeTexture9 **wrappedResource, NVAPI_STEREO_SURFACECREATEMODE createMode, HashedResource *hashedResource) {
	if (createMode != NVAPI_STEREO_SURFACECREATEMODE::NVAPI_STEREO_SURFACECREATEMODE_FORCEMONO && state->mHackerDevice->HackerDeviceShouldDuplicateSurface(pDesc)) {
		HRESULT hr;
		IDirect3DCubeTexture9 *rightTexture;
		hr = CreateResource(state->mOrigDevice, pDesc, pHandle, &rightTexture);
		D3D9Wrapper::IDirect3DCubeTexture9 *wrappedStereoTexture;
		if (!FAILED(hr)) {
			wrappedStereoTexture = D3D9Wrapper::IDirect3DCubeTexture9::GetDirect3DCubeTexture9(*wrappedResource, state->mHackerDevice, rightTexture);
		}
		else {
			wrappedStereoTexture = D3D9Wrapper::IDirect3DCubeTexture9::GetDirect3DCubeTexture9(*wrappedResource, state->mHackerDevice, NULL);
		}
		*wrappedResource = (IDirect3DCubeTexture9*)wrappedStereoTexture;
		return hr;
	}
	else {
		return E_NOTIMPL;
	}
}
HRESULT DirectModeStereoizeResource(CommandListState *state, D3D3DTEXTURE_DESC *pDesc, HANDLE *pHandle, IDirect3DVolumeTexture9 **wrappedResource, NVAPI_STEREO_SURFACECREATEMODE createMode, HashedResource *hashedResource) {
	return E_NOTIMPL;
}
HRESULT DirectModeStereoizeResource(CommandListState *state, D3DVERTEXBUFFER_DESC *pDesc, HANDLE *pHandle, IDirect3DVertexBuffer9 **wrappedResource, NVAPI_STEREO_SURFACECREATEMODE createMode, HashedResource *hashedResource) {
	return E_NOTIMPL;
}
HRESULT DirectModeStereoizeResource(CommandListState *state, D3DINDEXBUFFER_DESC *pDesc, HANDLE *pHandle, IDirect3DIndexBuffer9 **wrappedResource, NVAPI_STEREO_SURFACECREATEMODE createMode, HashedResource *hashedResource) {
	return E_NOTIMPL;
}
HRESULT GetResourceDesc(IDirect3DTexture9 *tex, D3D2DTEXTURE_DESC *desc) {
	*desc = D3D2DTEXTURE_DESC(tex);
	return D3D_OK;
}

HRESULT GetResourceDesc(IDirect3DCubeTexture9 *tex, D3D2DTEXTURE_DESC *desc) {
	*desc = D3D2DTEXTURE_DESC(tex);
	return D3D_OK;
}
HRESULT GetResourceDesc(IDirect3DVolumeTexture9 *tex, D3D3DTEXTURE_DESC *desc) {
	*desc = D3D3DTEXTURE_DESC(tex);
	return D3D_OK;
}
HRESULT GetResourceDesc(IDirect3DSurface9 *tex, D3D2DTEXTURE_DESC *desc) {
	*desc = D3D2DTEXTURE_DESC(tex);
	return D3D_OK;
}
HRESULT GetResourceDesc(IDirect3DVertexBuffer9 *vb, D3DVERTEXBUFFER_DESC *desc) {
	return vb->GetDesc(desc);
}

HRESULT GetResourceDesc(IDirect3DIndexBuffer9 *ib, D3DINDEXBUFFER_DESC *desc) {
	return ib->GetDesc(desc);
}
void CustomResource::OverrideBufferDesc(D3DVERTEXBUFFER_DESC * desc)
{
	if (override_byte_width != -1)
		desc->Size = override_byte_width;
	if (override_usage_flags != CustomResourceUsageFlags::INVALID)
		desc->Usage |= (DWORD)override_usage_flags;
	if (override_pool != D3DPOOL_DEFAULT)
		desc->Pool = override_pool;
}
void CustomResource::OverrideBufferDesc(D3DINDEXBUFFER_DESC * desc)
{
	if (override_byte_width != -1)
		desc->Size = override_byte_width;

	if (override_format != (D3DFORMAT)-1 && override_format != D3DFMT_UNKNOWN)
		desc->Format = override_format;

	if (override_usage_flags != CustomResourceUsageFlags::INVALID)
		desc->Usage |= (DWORD)override_usage_flags;

	if (override_pool != D3DPOOL_DEFAULT)
		desc->Pool = override_pool;
}
void CustomResource::OverrideSurfacesDesc(D3D2DTEXTURE_DESC *desc, CommandListState *state)
{
	if (override_height_expression != NULL) {
		float val = override_height_expression->evaluate(state);
		override_height = (int)val;
	}
	if (override_width_expression != NULL) {
		float val = override_width_expression->evaluate(state);
		override_width = (int)val;
	}

	if (override_width != -1)
		desc->Width = override_width;
	if (override_height != -1)
		desc->Height = override_height;
	if (override_format != (D3DFORMAT)-1 && override_format != D3DFMT_UNKNOWN)
		desc->Format = override_format;
	if (override_msaa != -1)
		desc->MultiSampleType = D3DMULTISAMPLE_TYPE(override_msaa);
	if (override_msaa_quality != -1)
		desc->MultiSampleQuality = override_msaa;
	if (override_mips != -1)
		desc->Levels = override_mips;

	desc->Width = (UINT)(desc->Width * width_multiply);
	desc->Height = (UINT)(desc->Height * height_multiply);

	if (override_usage_flags != CustomResourceUsageFlags::INVALID) {
		desc->Usage |= (DWORD)override_usage_flags;
		if ((DWORD)override_usage_flags & D3DUSAGE_RENDERTARGET)
			desc->Usage &= ~D3DUSAGE_DEPTHSTENCIL;
		if ((DWORD)override_usage_flags & D3DUSAGE_DEPTHSTENCIL)
			desc->Usage &= ~D3DUSAGE_RENDERTARGET;
	}

	if (override_type != CustomResourceType::INVALID) {
		switch (override_type)
		{
		case CustomResourceType::RENDERTARGET:
			desc->Type = D3DRTYPE_SURFACE;
			desc->Usage |= D3DUSAGE_RENDERTARGET;
			desc->Usage &= ~D3DUSAGE_DEPTHSTENCIL;
			break;
		case CustomResourceType::DEPTHSTENCILSURFACE:
			desc->Type = D3DRTYPE_SURFACE;
			desc->Usage |= D3DUSAGE_DEPTHSTENCIL;
			desc->Usage &= ~D3DUSAGE_RENDERTARGET;
			break;
		case CustomResourceType::OFFSCREENPLAIN:
			desc->Usage &= ~D3DUSAGE_RENDERTARGET;
			desc->Usage &= ~D3DUSAGE_DEPTHSTENCIL;
			desc->Type = D3DRTYPE_SURFACE;
			break;
		case CustomResourceType::TEXTURE2D:
			desc->Type = D3DRTYPE_TEXTURE;
			break;
		case CustomResourceType::CUBE:
			desc->Type = D3DRTYPE_CUBETEXTURE;
			break;
		default:
			break;
		}
	}

	if (override_pool != D3DPOOL_DEFAULT)
		desc->Pool = override_pool;
}
void CustomResource::OverrideSurfacesDesc(D3D3DTEXTURE_DESC *desc, CommandListState *state)
{
	if (override_height_expression != NULL) {
		float val = override_height_expression->evaluate(state);
		override_height = (int)val;
	}
	if (override_width_expression != NULL) {
		float val = override_width_expression->evaluate(state);
		override_width = (int)val;
	}
	if (override_depth_expression != NULL) {
		float val = override_depth_expression->evaluate(state);
		override_depth = (int)val;
	}

	if (override_width != -1)
		desc->Width = override_width;
	if (override_height != -1)
		desc->Height = override_height;
	if (override_depth != -1)
		desc->Depth = override_depth;
	if (override_format != (D3DFORMAT)-1 && override_format != D3DFMT_UNKNOWN)
		desc->Format = override_format;
	if (override_mips != -1)
		desc->Levels = override_mips;

	desc->Width = (UINT)(desc->Width * width_multiply);
	desc->Height = (UINT)(desc->Height * height_multiply);
	desc->Depth = (UINT)(desc->Depth * depth_multiply);

	if (override_usage_flags != CustomResourceUsageFlags::INVALID) {
		desc->Usage |= (DWORD)override_usage_flags;
		if ((DWORD)override_usage_flags & D3DUSAGE_RENDERTARGET)
			desc->Usage &= ~D3DUSAGE_DEPTHSTENCIL;
		if ((DWORD)override_usage_flags & D3DUSAGE_DEPTHSTENCIL)
			desc->Usage &= ~D3DUSAGE_RENDERTARGET;
	}

	if (override_pool != D3DPOOL_DEFAULT)
		desc->Pool = override_pool;
}
void CustomResource::OverrideOutOfBandInfo(D3DFORMAT *format, UINT *stride)
{
	if (override_format != (D3DFORMAT)-1 && override_format != D3DFMT_UNKNOWN)
		*format = override_format;
	if (override_stride != -1)
		*stride = override_stride;
}


bool ResourceCopyTarget::ParseTarget(const wchar_t *target, bool is_source, const wstring *ini_namespace)
{
	int ret, len;
	size_t length = wcslen(target);
	CustomResources::iterator res;
	ret = swscanf_s(target, L"%lcs-s%u%n", &shader_type, 1, &slot, &len);
	if (ret == 2 && len == length && ((slot < D3D9_PIXEL_INPUT_TEXTURE_SLOT_COUNT) || (slot >= D3D9_VERTEX_INPUT_START_REG && slot < (D3D9_VERTEX_INPUT_START_REG + D3D9_VERTEX_INPUT_TEXTURE_SLOT_COUNT)))) {
		type = ResourceCopyTargetType::SHADER_RESOURCE;
		goto check_shader_type;
	}

	// TODO: ret = swscanf_s(target, L"%lcs-s%u%n", &shader_type, 1, &slot, &len);
	// TODO: if (ret == 2 && len == length && slot < D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT) {
	// TODO: 	type = ResourceCopyTargetType::SAMPLER;
	// TODO:	goto check_shader_type;
	// TODO: }

	ret = swscanf_s(target, L"o%u%n", &slot, &len);
	if (ret == 1 && len == length && slot < D3D9_SIMULTANEOUS_RENDER_TARGET_COUNT) {
		type = ResourceCopyTargetType::RENDER_TARGET;
		return true;
	}

	if (!wcscmp(target, L"od")) {
		type = ResourceCopyTargetType::DEPTH_STENCIL_TARGET;
		return true;
	}
	ret = swscanf_s(target, L"vb%u%n", &slot, &len);
	if (ret == 1 && len == length && slot < D3D9_VERTEX_INPUT_RESOURCE_SLOT_COUNT) {
		type = ResourceCopyTargetType::VERTEX_BUFFER;
		return true;
	}

	if (!wcscmp(target, L"ib")) {
		type = ResourceCopyTargetType::INDEX_BUFFER;
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
		wstring namespaced_section;

		res = customResources.end();
		if (get_namespaced_section_name_lower(&resource_id, ini_namespace, &namespaced_section, &migoto_ini))
			res = customResources.find(namespaced_section);
		if (res == customResources.end())
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
		return true;
	}
	if (is_source && !wcscmp(target, L"r_bb")) {
		type = ResourceCopyTargetType::REAL_SWAP_CHAIN;
		// Holding a reference on the back buffer will prevent
		// ResizeBuffers() from working, so forbid caching any views of
		// the back buffer. Leaving it bound could also be a problem,
		// but since this is usually only used from custom shader
		// sections they will take care of unbinding it automatically:
		return true;
	}

	if (is_source && !wcscmp(target, L"f_bb")) {
		type = ResourceCopyTargetType::FAKE_SWAP_CHAIN;
		// Holding a reference on the back buffer will prevent
		// ResizeBuffers() from working, so forbid caching any views of
		// the back buffer. Leaving it bound could also be a problem,
		// but since this is usually only used from custom shader
		// sections they will take care of unbinding it automatically:
		return true;
	}


	if (is_source && !wcscmp(target, L"r_dst")) {
		type = ResourceCopyTargetType::REPLACEMENT_DEPTH_TEXTURE;
		return true;
	}
	if (is_source && !wcscmp(target, L"r_dss")) {
		type = ResourceCopyTargetType::REPLACEMENT_DEPTH_SURFACE;
		return true;
	}
	return false;

check_shader_type:
	switch (shader_type) {
	case L'v': case L'p':
		return true;
	}
	return false;
}
bool ParseCommandListSetShaderConstant(const wchar_t *section,
	const wchar_t *key, wstring *val, CommandList *command_list,
	const wstring *ini_namespace)
{
	SetShaderConstant *setShaderConstant = new SetShaderConstant();
	UINT vector4count = 0;
	if (ParseShaderConstant(key, &setShaderConstant->shader_type, &setShaderConstant->constant_type, &setShaderConstant->slot, &vector4count)) {
		setShaderConstant->vars.assign((vector4count * 4), 0);
		VariableArrayAssignment *command;
		if (!(val_to_variable_array_assignment(val, &setShaderConstant->vars, 0, (vector4count * 4), command_list, ini_namespace, &command)))
			goto bail;
		else
			setShaderConstant->assign = command;
	}
	else {
		if (!ParseShaderConstant(key, &setShaderConstant->shader_type, &setShaderConstant->constant_type, &setShaderConstant->slot, &setShaderConstant->component))
			goto bail;

		if (!setShaderConstant->expression.parse(val, ini_namespace, command_list))
			goto bail;
	}
	setShaderConstant->ini_line = L"[" + wstring(section) + L"] " + wstring(key) + L" = " + *val;
	command_list->commands.push_back(std::shared_ptr<CommandListCommand>(setShaderConstant));
	return true;
bail:
	delete setShaderConstant;
	return false;
}

bool ParseRunBuiltInFunction(const wchar_t * section, const wchar_t * key, wstring * val, CommandList *command_list, CommandList *pre_command_list, CommandList *post_command_list, const wstring * ini_namespace)
{
	BuiltInFunctions::iterator res;
	wchar_t * wcs = const_cast< wchar_t* >(val->c_str());
	wchar_t * pwc;
	wchar_t *buffer;
	pwc = wcstok_s(wcs, L" ", &buffer);
	if (!pwc)
		return false;
	wchar_t* func_name = pwc;
	vector<wchar_t*> params;
	pwc = wcstok_s(NULL, L", ", &buffer);
	while (pwc != NULL)
	{
		params.push_back(trimwhitespace(pwc));
		pwc = wcstok_s(NULL, L", ", &buffer);
	}

	BuiltInFunctionName func = parse_enum_option_string<const wchar_t *, BuiltInFunctionName, wchar_t*>
		(BuiltInFunctionsNames, func_name, NULL);

	if (func == BuiltInFunctionName::INVALID)
		return false;

	res = builtInFunctions.find(func);
	if (res == builtInFunctions.end())
		return false;

	CustomFunctionCommand *command = new CustomFunctionCommand();
	command->function = res->second;
	for (std::vector<wchar_t*>::iterator it = params.begin(); it != params.end(); ++it) {
		if (!(command->ParseParam((*it), pre_command_list, ini_namespace)))
			goto bail;
	}
	command->ini_line = L"[" + wstring(section) + L"] " + wstring(key) + L" = " + *val;
	pre_command_list->commands.push_back(std::shared_ptr<CommandListCommand>(command));
	return true;
bail:
	delete command;
	return false;
}

bool ParseRunCustomFunction(const wchar_t * section, const wchar_t * key, wstring * val, CommandList *command_list, CommandList *pre_command_list, CommandList *post_command_list, const wstring * ini_namespace)
{
	return false;
}
bool ParseRunFunction(const wchar_t * section, const wchar_t * key, wstring * val, CommandList *command_list, CommandList *pre_command_list, CommandList *post_command_list, const wstring * ini_namespace)
{
	if (ParseRunBuiltInFunction(section, key, val, command_list, pre_command_list, post_command_list, ini_namespace))
		return true;

	if (ParseRunCustomFunction(section, key, val, command_list, pre_command_list, post_command_list, ini_namespace))
		return true;

	return false;
}
bool ParseCommandListResourceCopyDirective(const wchar_t *section,
	const wchar_t *key, wstring *val, CommandList *command_list,
	const wstring *ini_namespace)
{
	ResourceCopyOperation *operation = new ResourceCopyOperation();
	wchar_t buf[MAX_PATH];
	wchar_t *src_ptr = NULL;

	if (!operation->dst.ParseTarget(key, false, ini_namespace))
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

	if (!operation->src.ParseTarget(src_ptr, true, ini_namespace))
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
				//|| operation->src.type == ResourceCopyTargetType::INI_PARAMS
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
	// be used in practice, so for now this will do. Update: We now
	// or in the bind flags in use when the custom resource is first
	// Substantiate()ed, which will cover most of these missing cases - the
	// remaining exceptions would be where differing bind flags are used at
	// different times and we still can't deduce it here
	// FIXME: The constant buffer bind flag can't be combined with others
	if (operation->src.type == ResourceCopyTargetType::CUSTOM_RESOURCE &&
		(operation->options & ResourceCopyOptions::REFERENCE)) {
		// Fucking C++ making this line 3x longer than it should be:
		operation->src.custom_resource->usage_flags = (DWORD)
		(operation->src.custom_resource->usage_flags | operation->dst.UsageFlags(NULL));
	}

	operation->ini_line = L"[" + wstring(section) + L"] " + wstring(key) + L" = " + *val;
	command_list->commands.push_back(std::shared_ptr<CommandListCommand>(operation));
	return true;
bail:
	delete operation;
	return false;
}
static bool ParseIfCommand(const wchar_t *section, const wstring *line,
	CommandList *pre_command_list, CommandList *post_command_list,
	const wstring *ini_namespace)
{
	IfCommand *operation = new IfCommand(section);
	wstring expression = line->substr(line->find_first_not_of(L" \t", 3));

	if (!operation->expression.parse(&expression, ini_namespace, pre_command_list))
		goto bail;

	// New scope level to isolate local variables:
	pre_command_list->scope->emplace_front();

	return AddCommandToList(operation, NULL, NULL, pre_command_list, post_command_list, section, line->c_str(), NULL);
bail:
	delete operation;
	return false;
}

static bool ParseElseIfCommand(const wchar_t *section, const wstring *line, int prefix,
	CommandList *pre_command_list, CommandList *post_command_list,
	const wstring *ini_namespace)
{
	ElseIfCommand *operation = new ElseIfCommand(section);
	wstring expression = line->substr(line->find_first_not_of(L" \t", prefix));

	if (!operation->expression.parse(&expression, ini_namespace, pre_command_list))
		goto bail;

	// Clear deepest scope level to isolate local variables:
	pre_command_list->scope->front().clear();

	// "else if" is implemented by nesting another if/endif inside the
	// parent if command's else clause. We add both an ElsePlaceholder and
	// an ElseIfCommand here, and will fix up the "endif" balance later.
	AddCommandToList(new ElsePlaceholder(), NULL, NULL, pre_command_list, post_command_list, section, line->c_str(), NULL);
	return AddCommandToList(operation, NULL, NULL, pre_command_list, post_command_list, section, line->c_str(), NULL);
bail:
	delete operation;
	return false;
}

static bool ParseElseCommand(const wchar_t *section,
	CommandList *pre_command_list, CommandList *post_command_list)
{
	// Clear deepest scope level to isolate local variables:
	pre_command_list->scope->front().clear();

	return AddCommandToList(new ElsePlaceholder(), NULL, NULL, pre_command_list, post_command_list, section, L"else", NULL);
}

static bool _ParseEndIfCommand(const wchar_t *section,
	CommandList *command_list, bool post, bool has_nested_else_if = false)
{
	CommandList::Commands::reverse_iterator rit;
	IfCommand *if_command;
	ElseIfCommand *else_if_command;
	ElsePlaceholder *else_command = NULL;
	CommandList::Commands::iterator else_pos = command_list->commands.end();

	for (rit = command_list->commands.rbegin(); rit != command_list->commands.rend(); rit++) {
		else_command = dynamic_cast<ElsePlaceholder*>(rit->get());
		if (else_command) {
			// C++ gotcha: reverse_iterator::base() points to the *next* element
			else_pos = rit.base() - 1;
		}

		if_command = dynamic_cast<IfCommand*>(rit->get());
		if (if_command) {
			// "else if" is treated as embedding another "if" block
			// in the else clause of the first "if" command. The
			// ElsePlaceholder was already inserted when the
			// ElseIfCommand was parsed, and this ElseIfCommand acts
			// as the nested IfCommand so the below code works as
			// is, but to balance the endifs we will need to repeat
			// this function until we find the original IfCommand:
			else_if_command = dynamic_cast<ElseIfCommand*>(rit->get());

			// Transfer the commands since the if command until the
			// endif into the if command's true/false lists
			if (post && !if_command->post_finalised) {
				// C++ gotcha: reverse_iterator::base() points to the *next* element
				if_command->true_commands_post->commands.assign(rit.base(), else_pos);
				if_command->true_commands_post->ini_section = if_command->ini_line;
				if (else_pos != command_list->commands.end()) {
					// Discard the else placeholder command:
					if_command->false_commands_post->commands.assign(else_pos + 1, command_list->commands.end());
					if_command->false_commands_post->ini_section = if_command->ini_line + L" <else>";
				}
				command_list->commands.erase(rit.base(), command_list->commands.end());
				if_command->post_finalised = true;
				if_command->has_nested_else_if = has_nested_else_if;
				if (else_if_command)
					return _ParseEndIfCommand(section, command_list, post, true);
				return true;
			}
			else if (!post && !if_command->pre_finalised) {
				// C++ gotcha: reverse_iterator::base() points to the *next* element
				if_command->true_commands_pre->commands.assign(rit.base(), else_pos);
				if_command->true_commands_pre->ini_section = if_command->ini_line;
				if (else_pos != command_list->commands.end()) {
					// Discard the else placeholder command:
					if_command->false_commands_pre->commands.assign(else_pos + 1, command_list->commands.end());
					if_command->false_commands_pre->ini_section = if_command->ini_line + L" <else>";
				}
				command_list->commands.erase(rit.base(), command_list->commands.end());
				if_command->pre_finalised = true;
				if_command->has_nested_else_if = has_nested_else_if;
				if (else_if_command)
					return _ParseEndIfCommand(section, command_list, post, true);
				return true;
			}
		}
	}

	LogOverlay(LOG_WARNING, "WARNING: [%S] endif missing if\n", section);
	return false;
}

static bool ParseEndIfCommand(const wchar_t *section,
	CommandList *pre_command_list, CommandList *post_command_list)
{
	bool ret;

	ret = _ParseEndIfCommand(section, pre_command_list, false);
	if (post_command_list)
		ret = ret && _ParseEndIfCommand(section, post_command_list, true);

	if (ret)
		pre_command_list->scope->pop_front();

	return ret;
}

bool ParseCommandListFlowControl(const wchar_t *section, const wstring *line,
	CommandList *pre_command_list, CommandList *post_command_list,
	const wstring *ini_namespace)
{
	if (!wcsncmp(line->c_str(), L"if ", 3))
		return ParseIfCommand(section, line, pre_command_list, post_command_list, ini_namespace);
	if (!wcsncmp(line->c_str(), L"elif ", 5))
		return ParseElseIfCommand(section, line, 5, pre_command_list, post_command_list, ini_namespace);
	if (!wcsncmp(line->c_str(), L"else if ", 8))
		return ParseElseIfCommand(section, line, 8, pre_command_list, post_command_list, ini_namespace);
	if (!wcscmp(line->c_str(), L"else"))
		return ParseElseCommand(section, pre_command_list, post_command_list);
	if (!wcscmp(line->c_str(), L"endif"))
		return ParseEndIfCommand(section, pre_command_list, post_command_list);

	return false;
}

IfCommand::IfCommand(const wchar_t *section) :
	pre_finalised(false),
	post_finalised(false),
	has_nested_else_if(false),
	section(section)
{
	true_commands_pre = std::make_shared<CommandList>();
	true_commands_post = std::make_shared<CommandList>();
	false_commands_pre = std::make_shared<CommandList>();
	false_commands_post = std::make_shared<CommandList>();
	true_commands_post->post = true;
	false_commands_post->post = true;

	// Placeholder names to be replaced by endif processing - we should
	// never see these, but in case they do show up somewhere these will
	// provide a clue as to what they are:
	true_commands_pre->ini_section = L"if placeholder";
	true_commands_post->ini_section = L"if placeholder";
	false_commands_pre->ini_section = L"else placeholder";
	false_commands_post->ini_section = L"else placeholder";

	// Place the dynamically allocated command lists in this data structure
	// to ensure they stay alive until after the optimisation stage, even
	// if the IfCommand is freed, e.g. by being optimised out:
	dynamically_allocated_command_lists.push_back(true_commands_pre);
	dynamically_allocated_command_lists.push_back(true_commands_post);
	dynamically_allocated_command_lists.push_back(false_commands_pre);
	dynamically_allocated_command_lists.push_back(false_commands_post);

	// And register these command lists for later optimisation:
	registered_command_lists.push_back(true_commands_pre.get());
	registered_command_lists.push_back(true_commands_post.get());
	registered_command_lists.push_back(false_commands_pre.get());
	registered_command_lists.push_back(false_commands_post.get());
}

void IfCommand::run(CommandListState *state)
{
	if (expression.evaluate(state)) {
		COMMAND_LIST_LOG(state, "%S: true {\n", ini_line.c_str());
		state->extra_indent++;
		if (state->post)
			_RunCommandList(true_commands_post.get(), state, false);
		else
			_RunCommandList(true_commands_pre.get(), state, false);
		state->extra_indent--;
		COMMAND_LIST_LOG(state, "} endif\n");
	}
	else {
		COMMAND_LIST_LOG(state, "%S: false\n", ini_line.c_str());
		if (!has_nested_else_if) {
			COMMAND_LIST_LOG(state, "[%S] else {\n", section.c_str());
			state->extra_indent++;
		}
		if (state->post)
			_RunCommandList(false_commands_post.get(), state, false);
		else
			_RunCommandList(false_commands_pre.get(), state, false);
		if (!has_nested_else_if) {
			state->extra_indent--;
			COMMAND_LIST_LOG(state, "} endif\n");
		}
	}
}

bool IfCommand::optimise(D3D9Wrapper::IDirect3DDevice9 *device)
{
	return expression.optimise(device);
}

bool IfCommand::noop(bool post, bool ignore_cto_pre, bool ignore_cto_post)
{
	float static_val;
	bool is_static;

	if ((post && !post_finalised) || (!post && !pre_finalised)) {
		LogOverlay(LOG_WARNING, "WARNING: If missing endif: %S\n", ini_line.c_str());
		return true;
	}

	is_static = expression.static_evaluate(&static_val);
	if (is_static) {
		if (static_val) {
			false_commands_pre->clear();
			false_commands_post->clear();
		}
		else {
			true_commands_pre->clear();
			true_commands_post->clear();
		}
	}

	if (post)
		return true_commands_post->commands.empty() && false_commands_post->commands.empty();
	return true_commands_pre->commands.empty() && false_commands_pre->commands.empty();
}

void CommandPlaceholder::run(CommandListState*)
{
	LogOverlay(LOG_DIRE, "BUG: Placeholder command executed: %S\n", ini_line.c_str());
}

bool CommandPlaceholder::noop(bool post, bool ignore_cto_pre, bool ignore_cto_post)
{
	LogOverlay(LOG_WARNING, "WARNING: Command not terminated: %S\n", ini_line.c_str());
	return true;
}
IDirect3DResource9 *ResourceCopyTarget::GetResource(
	CommandListState *state,
	UINT *stride,        // Used by vertex buffers
	UINT *offset,        // Used by vertex & index buffers
	D3DFORMAT *format, // Used by index buffers
	UINT *buf_size,
	ResourceCopyTarget *dst,
	D3D9Wrapper::IDirect3DResource9 **wrapper)
{
	D3D9Wrapper::IDirect3DDevice9 *mHackerDevice = state->mHackerDevice;
	IDirect3DDevice9 *mOrigDevice = state->mOrigDevice;

	::IDirect3DBaseTexture9 *tex = NULL;
	::IDirect3DSurface9 *sur = NULL;
	::IDirect3DVertexBuffer9 *vbuf = NULL;
	::IDirect3DIndexBuffer9 *ibuf = NULL;

	if (wrapper)
		*wrapper = NULL;
	void *pContainer = NULL;
	DWORD usage_flags = 0;

	switch (type) {
	case ResourceCopyTargetType::SHADER_RESOURCE:
	//	// FIXME: On win8 (or with evil update?), we should use
	//	// Get/SetConstantBuffers1 and copy the offset into the buffer as well

		switch (shader_type) {
		case L'v':
			mOrigDevice->GetTexture((D3DDMAPSAMPLER + 1 + slot), &tex);
			if (tex && wrapper) {
				auto it = mHackerDevice->m_activeTextureStages.find((D3DDMAPSAMPLER + 1 + slot));
				if (it != mHackerDevice->m_activeTextureStages.end()) {
					*wrapper = it->second;
				}
			}
			return tex;
		case L'p':
			mOrigDevice->GetTexture(slot, &tex);
			if (tex && wrapper) {
				auto it = mHackerDevice->m_activeTextureStages.find(slot);
				if (it != mHackerDevice->m_activeTextureStages.end()) {
					*wrapper = it->second;
				}
			}
			return tex;
		default:
			// Should not happen
			return NULL;
		}
		break;
	case ResourceCopyTargetType::VERTEX_BUFFER:
		// TODO: If copying this to a constant buffer, provide some
		// means to get the strides + offsets from within the shader.
		// Perhaps as an IniParam, or in another constant buffer?
		mOrigDevice->GetStreamSource(slot, &vbuf, offset, stride);
		if (state->call_info)
			*offset += state->call_info->StartVertex * *stride;
		if (vbuf && wrapper) {
			auto it = mHackerDevice->m_activeVertexBuffers.find(slot);
			if (it != mHackerDevice->m_activeVertexBuffers.end()){
				D3D9Wrapper::activeVertexBuffer avb = it->second;
				*wrapper = avb.m_vertexBuffer;
			}
		}
		return vbuf;
	case ResourceCopyTargetType::INDEX_BUFFER:
		// TODO: Similar comment as vertex buffers above, provide a
		// means for a shader to get format + offset.
		mOrigDevice->GetIndices(&ibuf);
		*stride = d3d_format_bytes(*format);
		if (state->call_info)
			*offset = state->call_info->StartIndex * *stride;
		if (ibuf && wrapper) {
			*wrapper = mHackerDevice->m_activeIndexBuffer;
		}
		return ibuf;

	case ResourceCopyTargetType::RENDER_TARGET:
		D3DCAPS9 pCaps;
		state->mOrigDevice->GetDeviceCaps(&pCaps);
		if (slot >= 0 && slot < pCaps.NumSimultaneousRTs)
			mOrigDevice->GetRenderTarget(slot, &sur);
		if (sur && wrapper && (slot < mHackerDevice->m_activeRenderTargets.size()) && (slot > 0)) {
			*wrapper = mHackerDevice->m_activeRenderTargets[slot];
		}
		return sur;

	case ResourceCopyTargetType::DEPTH_STENCIL_TARGET:
		mOrigDevice->GetDepthStencilSurface(&sur);
		if (sur && wrapper) {
			*wrapper = mHackerDevice->m_pActiveDepthStencil;
		}
		return sur;

	case ResourceCopyTargetType::CUSTOM_RESOURCE:
		if (dst)
			usage_flags = dst->UsageFlags(state);
		custom_resource->Substantiate(state, state->mHackerDevice->mStereoHandle, usage_flags);

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
			//*view = NULL;
			return NULL;
		}

		if (custom_resource->resource)
			custom_resource->resource->AddRef();
		return custom_resource->resource;

	case ResourceCopyTargetType::STEREO_PARAMS:
		if (mHackerDevice->mStereoTexture)
			mHackerDevice->mStereoTexture->AddRef();
		return mHackerDevice->mStereoTexture;

	case ResourceCopyTargetType::CURSOR_MASK:
		UpdateCursorResources(state);
		if (state->mHackerDevice->cursor_mask_tex)
			state->mHackerDevice->cursor_mask_tex->AddRef();
		return state->mHackerDevice->cursor_mask_tex;

	case ResourceCopyTargetType::CURSOR_COLOR:
		UpdateCursorResources(state);
		if (state->mHackerDevice->cursor_color_tex)
			state->mHackerDevice->cursor_color_tex->AddRef();
		return state->mHackerDevice->cursor_color_tex;

	case ResourceCopyTargetType::THIS_RESOURCE:
		if (state->this_target)
			return state->this_target->GetResource(state, stride, offset, format, buf_size, NULL, wrapper);

		if (state->resource)
			state->resource->AddRef();
		return state->resource;

	case ResourceCopyTargetType::SWAP_CHAIN:
		if (G->gForceStereo == 2) {
			if (G->bb_is_upscaling_bb) {
				D3D9Wrapper::IDirect3DSurface9 *wrappedBackBuffer;
				DirectModeGetFakeBackBuffer(state, 0, &wrappedBackBuffer);
				if (!wrappedBackBuffer)
					return NULL;
				if (wrapper)
					*wrapper = wrappedBackBuffer;
				return wrappedBackBuffer->GetD3DResource9();
			}
			else {
				D3D9Wrapper::IDirect3DSurface9 *wrappedBackBuffer;
				DirectModeGetRealBackBuffer(state, 0, &wrappedBackBuffer);
				if (!wrappedBackBuffer)
					return NULL;
				if (wrapper)
					*wrapper = wrappedBackBuffer;
				return wrappedBackBuffer->GetD3DResource9();
			}
		}else
		{
			if (G->bb_is_upscaling_bb && mHackerDevice->mFakeSwapChains.size() > 0 && mHackerDevice->mFakeSwapChains[0].mFakeBackBuffers.size() > 0) {
				D3D9Wrapper::IDirect3DSurface9 *wrappedBackBuffer = mHackerDevice->mFakeSwapChains[0].mFakeBackBuffers[0];
				if (wrapper)
					*wrapper = wrappedBackBuffer;
				wrappedBackBuffer->GetD3DResource9()->AddRef();
				return wrappedBackBuffer->GetD3DResource9();
			}
			else {
				if (G->SCREEN_UPSCALING > 0) {
					::IDirect3DSurface9 *realBB;
					mOrigDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, (IDirect3DSurface9**)&realBB);
					return realBB;
				}
				else {
					D3D9Wrapper::IDirect3DSurface9 *wrappedBackBuffer = mHackerDevice->deviceSwapChain->m_backBuffers[0];
					if (wrapper)
						*wrapper = wrappedBackBuffer;
					wrappedBackBuffer->GetD3DResource9()->AddRef();
					return wrappedBackBuffer->GetD3DResource9();
				}
			}
		}
	case ResourceCopyTargetType::REAL_SWAP_CHAIN:
		if (G->gForceStereo == 2) {
			D3D9Wrapper::IDirect3DSurface9 *wrappedBackBuffer;
			DirectModeGetRealBackBuffer(state, 0, &wrappedBackBuffer);
			if (!wrappedBackBuffer)
				return NULL;
			if (wrapper)
				*wrapper = wrappedBackBuffer;
			return wrappedBackBuffer->GetD3DResource9();
		}
		else
		{
			if (G->SCREEN_UPSCALING > 0) {
				::IDirect3DSurface9 *realBB;
				mOrigDevice->GetBackBuffer(0, 0, D3DBACKBUFFER_TYPE_MONO, (IDirect3DSurface9**)&realBB);
				return realBB;
			}
			else {
				D3D9Wrapper::IDirect3DSurface9 *wrappedBackBuffer = mHackerDevice->deviceSwapChain->m_backBuffers[0];
				if (wrapper)
					*wrapper = wrappedBackBuffer;
				wrappedBackBuffer->GetD3DResource9()->AddRef();
				return wrappedBackBuffer->GetD3DResource9();
			}
		}
	case ResourceCopyTargetType::FAKE_SWAP_CHAIN:
		if (G->gForceStereo == 2) {
			D3D9Wrapper::IDirect3DSurface9 *wrappedBackBuffer;
			DirectModeGetFakeBackBuffer(state, 0, &wrappedBackBuffer);
			if (!wrappedBackBuffer)
				return NULL;
			if (wrapper)
				*wrapper = wrappedBackBuffer;
			return wrappedBackBuffer->GetD3DResource9();
		}else
		{
			D3D9Wrapper::IDirect3DSurface9 *wrappedBackBuffer = NULL;
			if (mHackerDevice->mFakeSwapChains.size() > 0 && mHackerDevice->mFakeSwapChains[0].mFakeBackBuffers.size() > 0)
				wrappedBackBuffer = mHackerDevice->mFakeSwapChains[0].mFakeBackBuffers[0];
			else
				return NULL;
			if (wrapper)
				*wrapper = wrappedBackBuffer;
			wrappedBackBuffer->GetD3DResource9()->AddRef();
			return wrappedBackBuffer->GetD3DResource9();
		}
	case ResourceCopyTargetType::REPLACEMENT_DEPTH_TEXTURE:
		if (!mHackerDevice->depthstencil_replacement || !(mHackerDevice->depthstencil_replacement->depthstencil_replacement_texture))
			return NULL;
		if (!state->resolved_depth_replacement && mHackerDevice->depthstencil_replacement->depthstencil_replacement_multisampled) {
			mHackerDevice->depthstencil_replacement->resolveDepthReplacement();
			state->resolved_depth_replacement = true;
		}
		else if (!state->copied_depth_replacement && mHackerDevice->depthstencil_replacement->depthstencil_replacement_resolvedAA) {
			mHackerDevice->depthstencil_replacement->copyDepthSurfaceToTexture();
			state->copied_depth_replacement = true;
		}
		mHackerDevice->depthstencil_replacement->depthstencil_replacement_texture->AddRef();
		if (wrapper)
			*wrapper = mHackerDevice->depthstencil_replacement;
		return mHackerDevice->depthstencil_replacement->depthstencil_replacement_texture;
	case ResourceCopyTargetType::REPLACEMENT_DEPTH_SURFACE:
		if (!mHackerDevice->depthstencil_replacement || !(mHackerDevice->depthstencil_replacement->depthstencil_replacement_surface))
			return NULL;
		mHackerDevice->depthstencil_replacement->depthstencil_replacement_surface->AddRef();
		if (wrapper)
			*wrapper = mHackerDevice->depthstencil_replacement;
		return mHackerDevice->depthstencil_replacement->depthstencil_replacement_surface;
	}

	return NULL;
}

void ResourceCopyTarget::SetResource(
	CommandListState *state,
	IDirect3DResource9 *res,
	UINT stride,
	UINT offset,
	D3DFORMAT format,
	UINT buf_size,
	D3D9Wrapper::IDirect3DResource9 **wrapper)
{
	IDirect3DDevice9 *mOrigDevice = state->mOrigDevice;
	D3D9Wrapper::IDirect3DDevice9 *mHackerDevice = state->mHackerDevice;
	IDirect3DVertexBuffer9 *vb;
	IDirect3DIndexBuffer9 *ib;

	switch (type) {
	case ResourceCopyTargetType::SHADER_RESOURCE:
	//	// FIXME: On win8 (or with evil update?), we should use
	//	// Get/SetConstantBuffers1 and copy the offset into the buffer as well
	//	buf = (ID3D11Buffer*)res;
		IDirect3DBaseTexture9 * tex;
		switch (shader_type) {
		case L'v':
			if (!res) {
				mOrigDevice->SetTexture((D3DDMAPSAMPLER + 1 + slot), NULL);
				if (wrapper) {
					auto it = mHackerDevice->m_activeTextureStages.find((D3DDMAPSAMPLER + 1 + slot));
					if (it != mHackerDevice->m_activeTextureStages.end()) {
						it->second->Unbound(it->first);
						mHackerDevice->m_activeTextureStages.erase(it);
					}
				}
				return;
			}
			tex = reinterpret_cast<IDirect3DBaseTexture9*>(res);
			mOrigDevice->SetTexture((D3DDMAPSAMPLER + 1 + slot), tex);
			if (wrapper) {
				if (*wrapper) {
					D3D9Wrapper::IDirect3DBaseTexture9 *wrappedTexture = reinterpret_cast<D3D9Wrapper::IDirect3DBaseTexture9*>(*wrapper);
					auto it = mHackerDevice->m_activeTextureStages.find((D3DDMAPSAMPLER + 1 + slot));
					if (it != mHackerDevice->m_activeTextureStages.end()) {
						if (wrappedTexture != it->second) {
							wrappedTexture->Bound(it->first);
							it->second->Unbound(it->first);
							it->second = wrappedTexture;
						}
					}
					else {
						wrappedTexture->Bound(D3DDMAPSAMPLER + 1 + slot);
						mHackerDevice->m_activeTextureStages.emplace((D3DDMAPSAMPLER + 1 + slot), wrappedTexture);
					}
				}
				else {
					auto it = mHackerDevice->m_activeTextureStages.find((D3DDMAPSAMPLER + 1 + slot));
					if (it != mHackerDevice->m_activeTextureStages.end()) {
						it->second->Unbound(it->first);
						mHackerDevice->m_activeTextureStages.erase(it);
					}
				}
			}
			return;

		case L'p':

			if (!res) {
				mOrigDevice->SetTexture(slot, NULL);
				if (wrapper) {
					auto it = mHackerDevice->m_activeTextureStages.find(slot);
					if (it != mHackerDevice->m_activeTextureStages.end()) {
						it->second->Unbound(it->first);
						mHackerDevice->m_activeTextureStages.erase(it);
					}
				}
				return;
			}
			tex = reinterpret_cast<IDirect3DBaseTexture9*>(res);
			mOrigDevice->SetTexture((slot), tex);
			if (wrapper) {
				if (*wrapper) {
					D3D9Wrapper::IDirect3DBaseTexture9 *wrappedTexture = reinterpret_cast<D3D9Wrapper::IDirect3DBaseTexture9*>(*wrapper);
					auto it = mHackerDevice->m_activeTextureStages.find(slot);
					if (it != mHackerDevice->m_activeTextureStages.end()) {
						if (wrappedTexture != it->second) {
							wrappedTexture->Bound(slot);
							it->second->Unbound(slot);
							it->second = wrappedTexture;
						}
					}
					else {
						wrappedTexture->Bound(slot);
						mHackerDevice->m_activeTextureStages.emplace(slot, wrappedTexture);
					}
				}
				else {
					auto it = mHackerDevice->m_activeTextureStages.find(slot);
					if (it != mHackerDevice->m_activeTextureStages.end()) {
						it->second->Unbound(it->first);
						mHackerDevice->m_activeTextureStages.erase(it);
					}
				}
			}
			return;
		default:
	//		// Should not happen
			return;
		}
		break;
	case ResourceCopyTargetType::VERTEX_BUFFER:
		if (!res) {
			mOrigDevice->SetStreamSource(slot, NULL, offset, stride);
			if (wrapper) {
				auto it = mHackerDevice->m_activeVertexBuffers.find(slot);
				if (it != mHackerDevice->m_activeVertexBuffers.end()) {
					it->second.m_vertexBuffer->Unbound();
					mHackerDevice->m_activeVertexBuffers.erase(it);
				}
			}
			return;
		}

		vb = reinterpret_cast<IDirect3DVertexBuffer9*>(res);
		mOrigDevice->SetStreamSource(slot, vb, offset, stride);
		if (wrapper) {
			if (*wrapper) {
				D3D9Wrapper::IDirect3DVertexBuffer9 *wrappedVertexBuffer = reinterpret_cast<D3D9Wrapper::IDirect3DVertexBuffer9*>(*wrapper);
				auto it = mHackerDevice->m_activeVertexBuffers.find(slot);
				if (it != mHackerDevice->m_activeVertexBuffers.end()) {
					if (wrappedVertexBuffer != it->second.m_vertexBuffer) {
						D3D9Wrapper::activeVertexBuffer avb;
						avb.m_offsetInBytes = offset;
						avb.m_pStride = stride;
						avb.m_vertexBuffer = wrappedVertexBuffer;
						wrappedVertexBuffer->Bound();
						it->second.m_vertexBuffer->Unbound();
						it->second = avb;
					}
					else {
						it->second.m_pStride = stride;
						it->second.m_offsetInBytes = offset;
					}
				}
				else {
					D3D9Wrapper::activeVertexBuffer avb;
					avb.m_offsetInBytes = offset;
					avb.m_pStride = offset;
					avb.m_vertexBuffer = wrappedVertexBuffer;
					wrappedVertexBuffer->Bound();
					mHackerDevice->m_activeVertexBuffers.emplace(slot, avb);
				}
			}
			else {
				auto it = mHackerDevice->m_activeVertexBuffers.find(slot);
				if (it != mHackerDevice->m_activeVertexBuffers.end()) {
					it->second.m_vertexBuffer->Unbound();
					mHackerDevice->m_activeVertexBuffers.erase(it);
				}
			}
		}
		return;
	case ResourceCopyTargetType::INDEX_BUFFER:

		if (!res) {
			mOrigDevice->SetIndices(NULL);
			if (wrapper) {
				if (mHackerDevice->m_activeIndexBuffer)
					mHackerDevice->m_activeIndexBuffer->Unbound();
				mHackerDevice->m_activeIndexBuffer = NULL;
			}
			return;
		}
		ib = reinterpret_cast<IDirect3DIndexBuffer9*>(res);
		mOrigDevice->SetIndices(ib);
		if (wrapper) {
			D3D9Wrapper::IDirect3DIndexBuffer9 *wrappedIndexBuffer = reinterpret_cast<D3D9Wrapper::IDirect3DIndexBuffer9*>(*wrapper);
			if (wrappedIndexBuffer != mHackerDevice->m_activeIndexBuffer) {
				if (wrappedIndexBuffer)
					wrappedIndexBuffer->Bound();
				if (mHackerDevice->m_activeIndexBuffer)
					mHackerDevice->m_activeIndexBuffer->Unbound();
				mHackerDevice->m_activeIndexBuffer = wrappedIndexBuffer;
			}
		}
		return;
	case ResourceCopyTargetType::RENDER_TARGET:
		D3DCAPS9 pCaps;
		state->mOrigDevice->GetDeviceCaps(&pCaps);
		if (!res) {
			if (slot > 0 && slot < pCaps.NumSimultaneousRTs)
				mOrigDevice->SetRenderTarget(slot, NULL);
			if (wrapper && (slot < mHackerDevice->m_activeRenderTargets.size()) && (slot > 0)){
				D3D9Wrapper::IDirect3DSurface9 *existingRT = mHackerDevice->m_activeRenderTargets[slot];
				if (existingRT)
					existingRT->Unbound();
				mHackerDevice->m_activeRenderTargets[slot] = NULL;
			}
			return;
		}
		switch (res->GetType()) {
			IDirect3DSurface9 *pSurfaceLevel;
		case D3DRESOURCETYPE::D3DRTYPE_SURFACE:
			if (slot >= 0 && slot < pCaps.NumSimultaneousRTs) {
				mOrigDevice->SetRenderTarget(slot, reinterpret_cast<IDirect3DSurface9*>(res));
			}
			if (wrapper && (slot < mHackerDevice->m_activeRenderTargets.size()) && (slot > 0)) {
				D3D9Wrapper::IDirect3DSurface9 *wrappedRT = reinterpret_cast<D3D9Wrapper::IDirect3DSurface9*>(*wrapper);
				D3D9Wrapper::IDirect3DSurface9 *existingRT = mHackerDevice->m_activeRenderTargets[slot];
				if (wrappedRT != existingRT) {
					if (wrappedRT)
						wrappedRT->Bound();
					if (existingRT)
						existingRT->Unbound();
					mHackerDevice->m_activeRenderTargets[slot] = wrappedRT;
				}
			}
			return;
		case D3DRESOURCETYPE::D3DRTYPE_CUBETEXTURE:
			if (slot >= 0 && slot < pCaps.NumSimultaneousRTs) {
				(reinterpret_cast<IDirect3DCubeTexture9*>(res))->GetCubeMapSurface(D3DCUBEMAP_FACE_POSITIVE_X, 0, &pSurfaceLevel);
				mOrigDevice->SetRenderTarget(slot, pSurfaceLevel);
				pSurfaceLevel->Release();
			}
			if (wrapper && (slot < mHackerDevice->m_activeRenderTargets.size()) && (slot > 0)) {
				D3D9Wrapper::IDirect3DCubeTexture9 *wrappedCubeRT = reinterpret_cast<D3D9Wrapper::IDirect3DCubeTexture9*>(*wrapper);
				D3D9Wrapper::IDirect3DSurface9 *wrappedRT = NULL;
				if (wrappedCubeRT)
					wrappedCubeRT->GetCubeMapSurface(D3DCUBEMAP_FACE_POSITIVE_X, 0, &wrappedRT);
				D3D9Wrapper::IDirect3DSurface9 *existingRT = mHackerDevice->m_activeRenderTargets[slot];
				if (wrappedRT != existingRT) {
					if (wrappedRT)
						wrappedRT->Bound();
					if (existingRT)
						existingRT->Unbound();
					mHackerDevice->m_activeRenderTargets[slot] = wrappedRT;
				}
				if (wrappedRT)
					wrappedRT->Release();
			}
			return;
		case D3DRESOURCETYPE::D3DRTYPE_TEXTURE:
			if (slot >= 0 && slot < pCaps.NumSimultaneousRTs) {
				(reinterpret_cast<IDirect3DTexture9*>(res))->GetSurfaceLevel(0, &pSurfaceLevel);
				mOrigDevice->SetRenderTarget(slot, pSurfaceLevel);
				pSurfaceLevel->Release();
			}
			if (wrapper && (slot < mHackerDevice->m_activeRenderTargets.size()) && (slot > 0)) {
				D3D9Wrapper::IDirect3DTexture9 *wrappedTexRT = reinterpret_cast<D3D9Wrapper::IDirect3DTexture9*>(*wrapper);
				D3D9Wrapper::IDirect3DSurface9 *wrappedRT = NULL;
				if (wrappedTexRT)
					wrappedTexRT->GetSurfaceLevel(0, &wrappedRT);
				D3D9Wrapper::IDirect3DSurface9 *existingRT = mHackerDevice->m_activeRenderTargets[slot];
				if (wrappedRT != existingRT) {
					if (wrappedRT)
						wrappedRT->Bound();
					if (existingRT)
						existingRT->Unbound();
					mHackerDevice->m_activeRenderTargets[slot] = wrappedRT;
				}
				if (wrappedRT)
					wrappedRT->Release();
			}
			return;
		default:
			return;
		}
	case ResourceCopyTargetType::DEPTH_STENCIL_TARGET:
		IDirect3DSurface9 *pSurfaceLevel;
		if (!res) {
			mOrigDevice->SetDepthStencilSurface(NULL);
			if (wrapper) {
				if (mHackerDevice->m_pActiveDepthStencil)
					mHackerDevice->m_pActiveDepthStencil->Unbound();
				mHackerDevice->m_pActiveDepthStencil = NULL;
			}
			return;
		}

		switch (res->GetType()) {
		case D3DRESOURCETYPE::D3DRTYPE_SURFACE:
			mOrigDevice->SetDepthStencilSurface(reinterpret_cast<IDirect3DSurface9*>(res));
			if (wrapper) {
				D3D9Wrapper::IDirect3DSurface9 *wrappedDS = reinterpret_cast<D3D9Wrapper::IDirect3DSurface9*>(*wrapper);
				if (wrappedDS != mHackerDevice->m_pActiveDepthStencil) {
					if (wrappedDS)
						wrappedDS->Bound();
					if (mHackerDevice->m_pActiveDepthStencil)
						mHackerDevice->m_pActiveDepthStencil->Unbound();
					mHackerDevice->m_pActiveDepthStencil = wrappedDS;
				}
			}
			return;
		case D3DRESOURCETYPE::D3DRTYPE_CUBETEXTURE:
			(reinterpret_cast<IDirect3DCubeTexture9*>(res))->GetCubeMapSurface(D3DCUBEMAP_FACE_POSITIVE_X, 0, &pSurfaceLevel);
			mOrigDevice->SetDepthStencilSurface(pSurfaceLevel);
			pSurfaceLevel->Release();

			if (wrapper) {
				D3D9Wrapper::IDirect3DCubeTexture9 *wrappedCubeDS = reinterpret_cast<D3D9Wrapper::IDirect3DCubeTexture9*>(*wrapper);
				D3D9Wrapper::IDirect3DSurface9 *wrappedDS = NULL;
				if (wrappedCubeDS)
					wrappedCubeDS->GetCubeMapSurface(D3DCUBEMAP_FACE_POSITIVE_X, 0, &wrappedDS);
				if (wrappedDS != mHackerDevice->m_pActiveDepthStencil) {
					if (wrappedDS)
						wrappedDS->Bound();
					if (mHackerDevice->m_pActiveDepthStencil)
						mHackerDevice->m_pActiveDepthStencil->Unbound();
					mHackerDevice->m_pActiveDepthStencil = wrappedDS;
				}
				if (wrappedDS)
					wrappedDS->Release();
			}
			return;
		case D3DRESOURCETYPE::D3DRTYPE_TEXTURE:
			(reinterpret_cast<IDirect3DTexture9*>(res))->GetSurfaceLevel(0, &pSurfaceLevel);
			mOrigDevice->SetDepthStencilSurface(pSurfaceLevel);
			pSurfaceLevel->Release();
			if (wrapper) {
				D3D9Wrapper::IDirect3DTexture9 *wrappedTexDS = reinterpret_cast<D3D9Wrapper::IDirect3DTexture9*>(*wrapper);
				D3D9Wrapper::IDirect3DSurface9 *wrappedDS = NULL;
				if (wrappedTexDS)
					wrappedTexDS->GetSurfaceLevel(0, &wrappedDS);
				if (wrappedDS != mHackerDevice->m_pActiveDepthStencil) {
					if (wrappedDS)
						wrappedDS->Bound();
					if (mHackerDevice->m_pActiveDepthStencil)
						mHackerDevice->m_pActiveDepthStencil->Unbound();
					mHackerDevice->m_pActiveDepthStencil = wrappedDS;
				}
				if (wrappedDS)
					wrappedDS->Release();
			}
			return;
		default:
			return;
		}
	case ResourceCopyTargetType::CUSTOM_RESOURCE:
		custom_resource->stride = stride;
		custom_resource->offset = offset;
		custom_resource->format = format;
		custom_resource->buf_size = buf_size;


		if (res == NULL) {
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

		if (custom_resource->resource != res) {
			if (custom_resource->resource)
				custom_resource->resource->Release();
			custom_resource->resource = res;
			if (custom_resource->resource)
				custom_resource->resource->AddRef();
		}
		break;

	case ResourceCopyTargetType::THIS_RESOURCE:
		if (state->this_target)
			return state->this_target->SetResource(state, res, stride, offset, format, buf_size, wrapper);
		COMMAND_LIST_LOG(state, "  \"this\" target cannot be set outside of a checktextureoverride context\n");
		break;
	case ResourceCopyTargetType::STEREO_PARAMS:
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
DWORD ResourceCopyTarget::UsageFlags(CommandListState *state)
{
	switch (type) {
	case ResourceCopyTargetType::RENDER_TARGET:
		return D3DUSAGE_RENDERTARGET;
	case ResourceCopyTargetType::REPLACEMENT_DEPTH_TEXTURE:
	case ResourceCopyTargetType::REPLACEMENT_DEPTH_SURFACE:
	case ResourceCopyTargetType::DEPTH_STENCIL_TARGET:
		return D3DUSAGE_DEPTHSTENCIL;
	case ResourceCopyTargetType::CUSTOM_RESOURCE:
		return custom_resource->usage_flags;
	case ResourceCopyTargetType::THIS_RESOURCE:
		if (state && state->this_target)
			return state->this_target->UsageFlags(state);
		// Bind flags are unknown since this cannot be resolved
		// until runtime:
		return (DWORD)0;
	case ResourceCopyTargetType::STEREO_PARAMS:
	//case ResourceCopyTargetType::INI_PARAMS:
	case ResourceCopyTargetType::SWAP_CHAIN:
	case ResourceCopyTargetType::CPU:
	case ResourceCopyTargetType::SHADER_RESOURCE:
	case ResourceCopyTargetType::VERTEX_BUFFER:
	case ResourceCopyTargetType::INDEX_BUFFER:
		// N/A since swap chain can't be set as a destination
		return (DWORD)0;
	}

	// Shouldn't happen. No return value makes sense, so raise an exception
	throw(std::range_error("Bad 3DMigoto ResourceCopyTarget"));
}
void ResourceCopyTarget::FindTextureOverrides(CommandListState *state, bool *resource_found, TextureOverrideMatches *matches)
{
	TextureOverrideMap::iterator i;
	IDirect3DResource9 *resource = NULL;
	uint32_t hash = 0;
	D3D9Wrapper::IDirect3DResource9 *wrapper = NULL;
	ResourceHandleInfo *info = NULL;
	resource = GetResource(state, NULL, NULL, NULL, NULL, NULL, &wrapper);

	if (resource_found)
		*resource_found = !!resource;

	if (!resource)
		return;

	if (wrapper)
		info = &wrapper->resourceHandleInfo;

	find_texture_overrides_for_resource(resource, info, matches, state->call_info);
	resource->Release();
}
template <typename ID3D9Buffer, typename ID3D9BufferDesc>
static ID3D9Buffer *RecreateCompatibleBuffer(
	wstring *ini_line,
	ResourceCopyTarget *dst, // May be NULL
	ID3D9Buffer *src_resource,
	ID3D9Buffer *dst_resource,
	ResourcePool *resource_pool,
	DWORD usage_flags,
	CommandListState *state,
	UINT stride,
	UINT offset,
	D3DFORMAT format,
	UINT *buf_dst_size)
{
	ID3D9BufferDesc new_desc;
	ID3D9Buffer *buffer = NULL;
	GetResourceDesc(src_resource, &new_desc);
	if (!(new_desc.Usage & usage_flags))
		new_desc.Usage |= usage_flags;

	if (dst && dst->type == ResourceCopyTargetType::CPU) {
		new_desc.Pool = D3DPOOL_SYSTEMMEM;
	}
	else {
		new_desc.Pool = D3DPOOL_DEFAULT;
	}
	if (offset) {
		*buf_dst_size = new_desc.Size;
	}

	if (dst && dst->type == ResourceCopyTargetType::CUSTOM_RESOURCE)
		dst->custom_resource->OverrideBufferDesc(&new_desc);

	return GetResourceFromPool<ID3D9Buffer, ID3D9Buffer, ID3D9BufferDesc>
		(ini_line, src_resource, dst_resource, resource_pool, state, &new_desc);
}

static D3DFORMAT MakeDSVFormat(D3DFORMAT fmt, bool onlyLockable)
{
	if (!onlyLockable) {
		switch (fmt)
		{
		case D3DFMT_A32B32G32R32F:
		case D3DFMT_A16B16G16R16:
		case D3DFMT_Q16W16V16U16:
		case D3DFMT_A16B16G16R16F:
		case D3DFMT_G32R32F:
			return D3DFMT_D32;
		case D3DFMT_A2B10G10R10:
		case D3DFMT_A8B8G8R8:
		case D3DFMT_X8B8G8R8:
		case D3DFMT_G16R16:
		case D3DFMT_A2R10G10B10:
		case D3DFMT_V16U16:
		case D3DFMT_A2W10V10U10:
		case D3DFMT_A8R8G8B8:
		case D3DFMT_X8R8G8B8:
		case D3DFMT_X8L8V8U8:
		case D3DFMT_Q8W8V8U8:
		case D3DFMT_INDEX32:
		case D3DFMT_A2B10G10R10_XR_BIAS:
			return D3DFMT_D32;
		case D3DFMT_G16R16F:
		case D3DFMT_R32F:
			return D3DFMT_D32F_LOCKABLE;
		case D3DFMT_R5G6B5:
		case D3DFMT_X1R5G5B5:
		case D3DFMT_A1R5G5B5:
		case D3DFMT_A4R4G4B4:
		case D3DFMT_A8R3G3B2:
		case D3DFMT_X4R4G4B4:
		case D3DFMT_A8P8:
		case D3DFMT_A8L8:
		case D3DFMT_V8U8:
		case D3DFMT_L6V5U5:
		case D3DFMT_D15S1:
		case D3DFMT_L16:
		case D3DFMT_INDEX16:
		case D3DFMT_R16F:
		case D3DFMT_CxV8U8:
		case D3DFMT_R8G8_B8G8:
		case D3DFMT_G8R8_G8B8:
			return D3DFMT_D16;
		case D3DFMT_R3G3B2:
		case D3DFMT_A8:
		case D3DFMT_P8:
		case D3DFMT_L8:
		case D3DFMT_A4L4:
			return D3DFMT_S8_LOCKABLE;
		default:
			return fmt;
		}
	}
	else {
		switch (fmt)
		{
		case D3DFMT_A2B10G10R10:
		case D3DFMT_A8B8G8R8:
		case D3DFMT_X8B8G8R8:
		case D3DFMT_G16R16:
		case D3DFMT_A2R10G10B10:
		case D3DFMT_V16U16:
		case D3DFMT_A2W10V10U10:
		case D3DFMT_A8R8G8B8:
		case D3DFMT_X8R8G8B8:
		case D3DFMT_X8L8V8U8:
		case D3DFMT_Q8W8V8U8:
		case D3DFMT_INDEX32:
		case D3DFMT_A2B10G10R10_XR_BIAS:
		case D3DFMT_D32:
			return D3DFMT_D32_LOCKABLE;
		case D3DFMT_A32B32G32R32F:
		case D3DFMT_A16B16G16R16:
		case D3DFMT_Q16W16V16U16:
		case D3DFMT_A16B16G16R16F:
		case D3DFMT_G32R32F:
		case D3DFMT_G16R16F:
		case D3DFMT_R32F:
			return D3DFMT_D32F_LOCKABLE;
		case D3DFMT_R5G6B5:
		case D3DFMT_X1R5G5B5:
		case D3DFMT_A1R5G5B5:
		case D3DFMT_A4R4G4B4:
		case D3DFMT_A8R3G3B2:
		case D3DFMT_X4R4G4B4:
		case D3DFMT_A8P8:
		case D3DFMT_A8L8:
		case D3DFMT_V8U8:
		case D3DFMT_L6V5U5:
		case D3DFMT_D15S1:
		case D3DFMT_L16:
		case D3DFMT_INDEX16:
		case D3DFMT_R16F:
		case D3DFMT_CxV8U8:
		case D3DFMT_R8G8_B8G8:
		case D3DFMT_G8R8_G8B8:
		case D3DFMT_D16:
			return D3DFMT_D16_LOCKABLE;
		case D3DFMT_R3G3B2:
		case D3DFMT_A8:
		case D3DFMT_P8:
		case D3DFMT_L8:
		case D3DFMT_A4L4:
			return D3DFMT_S8_LOCKABLE;
		default:
			return fmt;
		}
	}

}

static D3DFORMAT MakeNonDSVFormat(D3DFORMAT fmt)
{
	// TODO: Add a keyword to return the stencil side of a combined
	// depth/stencil resource instead of the depth side
	switch (fmt)
	{
	case D3DFMT_D32:
	case D3DFMT_D24S8:
	case D3DFMT_D32_LOCKABLE:
	case D3DFMT_D24X8:
	case D3DFMT_D24X4S4:
	case D3DFMT_D24FS8:
		return D3DFMT_A8R8G8B8;
	case D3DFMT_D32F_LOCKABLE:
		return D3DFMT_G16R16F;
	case D3DFMT_D16:
		return D3DFMT_A4R4G4B4;
	case D3DFMT_S8_LOCKABLE:
		return D3DFMT_R3G3B2;
	default:
		return fmt;
	}
}
template <typename DescType>
static void Texture2DDescResolveMSAA(DescType *desc) {}
template <>
static void Texture2DDescResolveMSAA(D3DSURFACE_DESC *desc)
{
	desc->MultiSampleType = D3DMULTISAMPLE_NONE;
	desc->MultiSampleQuality = 0;
}
template <typename SourceResourceType>
static IDirect3DResource9* RecreateCompatibleSurfaces(
	wstring *ini_line,
	ResourceCopyTarget *dst, // May be NULL
	SourceResourceType *src_resource,
	IDirect3DResource9 *dst_resource,
	ResourcePool *resource_pool,
	DWORD usage_flags,
	CommandListState *state,
	StereoHandle mStereoHandle,
	ResourceCopyOptions options,
	bool override_create_mode = false,
	NVAPI_STEREO_SURFACECREATEMODE surfaceCreationMode = NVAPI_STEREO_SURFACECREATEMODE::NVAPI_STEREO_SURFACECREATEMODE_AUTO
)
{
	HANDLE sharedHandle = NULL;
	HANDLE *pSharedHandle = NULL;
	D3D2DTEXTURE_DESC new_desc;
	GetResourceDesc(src_resource, &new_desc);
	if (!(new_desc.Usage & usage_flags)) {
		new_desc.Usage |= usage_flags;
		if (usage_flags & D3DUSAGE_RENDERTARGET)
			new_desc.Usage &= ~D3DUSAGE_DEPTHSTENCIL;
		if (usage_flags & D3DUSAGE_DEPTHSTENCIL)
			new_desc.Usage &= ~D3DUSAGE_RENDERTARGET;
	}

	if (dst) {
		switch (dst->type) {
		case ResourceCopyTargetType::DEPTH_STENCIL_TARGET:
			new_desc.Levels = 1;
			new_desc.Pool = D3DPOOL_DEFAULT;
			new_desc.Format = MakeDSVFormat(new_desc.Format, false);
			break;
		case ResourceCopyTargetType::RENDER_TARGET:
			new_desc.Levels = 1;
			new_desc.Pool = D3DPOOL_DEFAULT;
			new_desc.Format = MakeNonDSVFormat(new_desc.Format);
			break;
		case ResourceCopyTargetType::SHADER_RESOURCE:
			if (new_desc.Type != D3DRTYPE_TEXTURE && new_desc.Type != D3DRTYPE_CUBETEXTURE)
				new_desc.Type = D3DRTYPE_TEXTURE;
			break;
		case ResourceCopyTargetType::CPU:
			if (new_desc.Usage & D3DUSAGE_DEPTHSTENCIL)
				new_desc.Format = MakeDSVFormat(new_desc.Format, true);
			new_desc.Usage |= D3DUSAGE_DYNAMIC;
			new_desc.Type = D3DRTYPE_SURFACE;
		}
	}
	if (options & ResourceCopyOptions::STEREO2MONO)
	{
		if (!(options & ResourceCopyOptions::STEREO))
			new_desc.Width *= 2;
		else
			new_desc.Type = D3DRTYPE_TEXTURE;
	}

	// TODO: reverse_blit might need to imply resolve_msaa:
	if (options & ResourceCopyOptions::RESOLVE_MSAA)
		Texture2DDescResolveMSAA(&new_desc);

	// XXX: Any changes needed in new_desc->MiscFlags?
	//
	// D3D11_RESOURCE_MISC_GENERATE_MIPS requires specific bind flags (both
	// shader resource AND render target must be set) and might prevent us
	// from creating the resource otherwise. Since we don't need to
	// generate mip-maps just clear it out:
	//new_desc->MiscFlags &= ~D3D11_RESOURCE_MISC_GENERATE_MIPS;
	new_desc.Usage &= ~D3DUSAGE_AUTOGENMIPMAP;

	if (dst && dst->type == ResourceCopyTargetType::CUSTOM_RESOURCE)
		dst->custom_resource->OverrideSurfacesDesc(&new_desc, state);
	ResourceCreationInfo info;
	info.mode = surfaceCreationMode;

	IDirect3DResource9 *resourceFromPool = NULL;
	switch (new_desc.Type) {
	case D3DRTYPE_SURFACE:
		resourceFromPool = GetResourceFromPool<SourceResourceType, IDirect3DSurface9, D3D2DTEXTURE_DESC>
			(ini_line, src_resource, (IDirect3DSurface9*)dst_resource, resource_pool, state, &new_desc, override_create_mode, mStereoHandle, pSharedHandle, &info);
		break;
	case D3DRTYPE_TEXTURE:
		resourceFromPool = GetResourceFromPool<SourceResourceType, IDirect3DTexture9, D3D2DTEXTURE_DESC>
			(ini_line, src_resource, (IDirect3DTexture9*)dst_resource, resource_pool, state, &new_desc, override_create_mode, mStereoHandle, pSharedHandle, &info);
		break;
	case D3DRTYPE_CUBETEXTURE:
		resourceFromPool = GetResourceFromPool<SourceResourceType, IDirect3DCubeTexture9, D3D2DTEXTURE_DESC>
			(ini_line, src_resource, (IDirect3DCubeTexture9*)dst_resource, resource_pool, state, &new_desc, override_create_mode, mStereoHandle, pSharedHandle, &info);
		break;
	}
	return resourceFromPool;
}
static IDirect3DVolumeTexture9* RecreateCompatibleVolumeTexture(
	wstring *ini_line,
	ResourceCopyTarget *dst, // May be NULL
	IDirect3DVolumeTexture9 *src_resource,
	IDirect3DVolumeTexture9 *dst_resource,
	ResourcePool *resource_pool,
	DWORD usage_flags,
	CommandListState *state,
	StereoHandle mStereoHandle,
	ResourceCopyOptions options,
	bool override_create_mode = false,
	NVAPI_STEREO_SURFACECREATEMODE surfaceCreationMode = NVAPI_STEREO_SURFACECREATEMODE::NVAPI_STEREO_SURFACECREATEMODE_AUTO
)
{
	HANDLE sharedHandle = NULL;
	HANDLE *pSharedHandle = NULL;
	D3D3DTEXTURE_DESC new_desc;
	GetResourceDesc(src_resource, &new_desc);

	if (dst && dst->type == ResourceCopyTargetType::CPU)
		new_desc.Usage |= D3DUSAGE_DYNAMIC;

	if (options & ResourceCopyOptions::STEREO2MONO && !(options & ResourceCopyOptions::STEREO))
		new_desc.Width *= 2;

	// TODO: reverse_blit might need to imply resolve_msaa:
	if (options & ResourceCopyOptions::RESOLVE_MSAA)
		Texture2DDescResolveMSAA(&new_desc);

	// XXX: Any changes needed in new_desc->MiscFlags?
	//
	// D3D11_RESOURCE_MISC_GENERATE_MIPS requires specific bind flags (both
	// shader resource AND render target must be set) and might prevent us
	// from creating the resource otherwise. Since we don't need to
	// generate mip-maps just clear it out:
	//new_desc->MiscFlags &= ~D3D11_RESOURCE_MISC_GENERATE_MIPS;
	new_desc.Usage &= ~D3DUSAGE_AUTOGENMIPMAP;

	if (dst && dst->type == ResourceCopyTargetType::CUSTOM_RESOURCE)
		dst->custom_resource->OverrideSurfacesDesc(&new_desc, state);
	ResourceCreationInfo info;
	info.mode = surfaceCreationMode;

	return GetResourceFromPool<IDirect3DVolumeTexture9, IDirect3DVolumeTexture9, D3D3DTEXTURE_DESC>
		(ini_line, src_resource, dst_resource, resource_pool, state, &new_desc, override_create_mode, mStereoHandle, pSharedHandle, &info);
}
static void RecreateCompatibleResource(
	wstring *ini_line,
	ResourceCopyTarget *dst, // May be NULL
	IDirect3DResource9 *src_resource,
	IDirect3DResource9 **dst_resource,
	ResourcePool *resource_pool,
	CommandListState *state,
	StereoHandle mStereoHandle,
	ResourceCopyOptions options,
	UINT stride,
	UINT offset,
	D3DFORMAT format,
	UINT *buf_dst_size)
{
	NVAPI_STEREO_SURFACECREATEMODE new_mode = NVAPI_STEREO_SURFACECREATEMODE_AUTO;
	D3DRESOURCETYPE src_type;
	D3DRESOURCETYPE dst_type;
	DWORD usage_flags = (DWORD)0;
	IDirect3DResource9 *res = NULL;
	bool override_create_mode = false;

	if (dst)
		usage_flags = dst->UsageFlags(state);

	src_type = src_resource->GetType();
	if (*dst_resource) {
		dst_type = (*dst_resource)->GetType();
	}
	if (mStereoHandle || G->gForceStereo == 2) {
		if (options & ResourceCopyOptions::CREATEMODE_MASK) {
			override_create_mode = true;

			// STEREO2MONO will force the final destination to mono since
			// it is in the CREATEMODE_MASK, but is not STEREO. It also
			// creates an additional intermediate resource that will be
			// forced to STEREO.

			if (options & ResourceCopyOptions::STEREO) {
				new_mode = NVAPI_STEREO_SURFACECREATEMODE_FORCESTEREO;
			}
			else {
				new_mode = NVAPI_STEREO_SURFACECREATEMODE_FORCEMONO;
			}
		}
		else if (dst && dst->type == ResourceCopyTargetType::CUSTOM_RESOURCE) {
			override_create_mode = dst->custom_resource->OverrideSurfaceCreationMode(mStereoHandle, &new_mode);
		}
	}

	switch (src_type) {
	case D3DRTYPE_VERTEXBUFFER:
		res = RecreateCompatibleBuffer<IDirect3DVertexBuffer9, D3DVERTEXBUFFER_DESC>(ini_line, dst, (IDirect3DVertexBuffer9*)src_resource, (IDirect3DVertexBuffer9*)*dst_resource, resource_pool, usage_flags, state, stride, offset, format, buf_dst_size);
		break;
	case D3DRTYPE_INDEXBUFFER:
		res = RecreateCompatibleBuffer<IDirect3DIndexBuffer9, D3DINDEXBUFFER_DESC>(ini_line, dst, (IDirect3DIndexBuffer9*)src_resource, (IDirect3DIndexBuffer9*)*dst_resource, resource_pool, usage_flags, state, stride, offset, format, buf_dst_size);
		break;
	case D3DRTYPE_SURFACE:
		res = RecreateCompatibleSurfaces<IDirect3DSurface9>
		(ini_line, dst, (IDirect3DSurface9*)src_resource, *dst_resource, resource_pool, usage_flags,
			state, mStereoHandle, options,
			override_create_mode, new_mode);
		break;
	case D3DRTYPE_TEXTURE:
		res = RecreateCompatibleSurfaces<IDirect3DTexture9>
		(ini_line, dst, (IDirect3DTexture9*)src_resource, *dst_resource, resource_pool, usage_flags,
			state, mStereoHandle, options,
			override_create_mode, new_mode);
		break;
	case D3DRTYPE_CUBETEXTURE:
		res = RecreateCompatibleSurfaces<IDirect3DCubeTexture9>
		(ini_line, dst, (IDirect3DCubeTexture9*)src_resource, *dst_resource, resource_pool, usage_flags,
			state, mStereoHandle, options,
			override_create_mode, new_mode);
		break;
	case D3DRTYPE_VOLUMETEXTURE:
		res = RecreateCompatibleVolumeTexture
		(ini_line, dst, (IDirect3DVolumeTexture9*)src_resource, (IDirect3DVolumeTexture9*)*dst_resource, resource_pool, usage_flags,
			state, mStereoHandle, options,
			override_create_mode, new_mode);
		break;
	}
	if (res) {
		if (*dst_resource)
			(*dst_resource)->Release();

		*dst_resource = res;
	}
}
static void SetViewportFromResource(CommandListState *state, IDirect3DResource9 *resource)
{
	D3DRESOURCETYPE type;
	IDirect3DTexture9 *tex2d;
	IDirect3DCubeTexture9 *texCube;
	IDirect3DSurface9 *sur;
	D3DSURFACE_DESC sur_desc;

	// TODO: Could handle mip-maps from a view like the CD3D11_VIEWPORT
	// constructor, but we aren't using them elsewhere so don't care yet.

	D3DVIEWPORT9 viewport = { 0, 0, 0, 0, 0.0f, 1.0f };

	type = resource->GetType();
	switch (type) {
	case D3DRTYPE_TEXTURE:
		tex2d = (IDirect3DTexture9*)resource;
		tex2d->GetLevelDesc(0, &sur_desc);
		viewport.Width = sur_desc.Width;
		viewport.Height = sur_desc.Height;
		break;
	case D3DRTYPE_CUBETEXTURE:
		texCube = (IDirect3DCubeTexture9*)resource;
		texCube->GetLevelDesc(0, &sur_desc);
		viewport.Width = sur_desc.Width;
		viewport.Height = sur_desc.Height;
		break;
	case D3DRTYPE_SURFACE:
		sur = (IDirect3DSurface9*)resource;
		sur->GetDesc(&sur_desc);
		viewport.Width = sur_desc.Width;
		viewport.Height = sur_desc.Height;
	}
	state->mOrigDevice->SetViewport(&viewport);
}

ResourceCopyOperation::ResourceCopyOperation() :
	options(ResourceCopyOptions::INVALID),
	cached_resource(NULL),
	stereo2mono_intermediate(NULL)
{
	resourceCopyOperations.push_back(this);

}

ResourceCopyOperation::~ResourceCopyOperation()
{
	EnterCriticalSection(&G->mCriticalSection);
	if (cached_resource)// {
		cached_resource->Release();
	if (stereo2mono_intermediate)
		stereo2mono_intermediate->Release();

	if (resourceCopyOperations.size() > 0)
		if (std::find(resourceCopyOperations.begin(), resourceCopyOperations.end(), this) != resourceCopyOperations.end())
			resourceCopyOperations.erase(std::remove(resourceCopyOperations.begin(), resourceCopyOperations.end(), this), resourceCopyOperations.end());
	LeaveCriticalSection(&G->mCriticalSection);
}

ResourceStagingOperation::ResourceStagingOperation()
{
	dst.type = ResourceCopyTargetType::CPU;
	options = ResourceCopyOptions::COPY;
	staging = false;
	ini_line = L"  Beginning transfer to CPU...";
	mapped_resource_type = D3DRESOURCETYPE(-1);
}

HRESULT mapGetSourceSurface(IDirect3DSurface9 *container, IDirect3DSurface9 *sur) {
	sur = container;
	return D3D_OK;
}
HRESULT mapGetSourceSurface(IDirect3DVolume9 *container, IDirect3DVolume9 *sur) {
	sur = container;
	return D3D_OK;
}
HRESULT mapGetSourceSurface(IDirect3DTexture9 *container, IDirect3DSurface9 *sur) {
	return container->GetSurfaceLevel(0, &sur);
}
HRESULT mapGetSourceSurface(IDirect3DCubeTexture9 *container, IDirect3DSurface9 *sur) {
	return container->GetCubeMapSurface(D3DCUBEMAP_FACE_POSITIVE_X, 0, &sur);
}

HRESULT mapGetSourceSurface(IDirect3DVolumeTexture9 *container, IDirect3DVolume9 *sur) {
	return container->GetVolumeLevel(0, &sur);
}

HRESULT mapUpdateDestResource(CommandListState *state, D3D2DTEXTURE_DESC *desc, IDirect3DSurface9 *srcSur, IDirect3DSurface9 *dstSur) {
	if ((desc->Usage & D3DUSAGE_RENDERTARGET)){
		if (desc->MultiSampleType == D3DMULTISAMPLE_NONE)
			return state->mOrigDevice->GetRenderTargetData(srcSur, dstSur);
		else {
			IDirect3DSurface9 *resolve_aa;
			state->mOrigDevice->CreateRenderTarget(desc->Width, desc->Height, desc->Format, D3DMULTISAMPLE_NONE, 0, false, &resolve_aa, NULL);
			state->mOrigDevice->StretchRect(srcSur, NULL, resolve_aa, NULL, D3DTEXTUREFILTERTYPE::D3DTEXF_POINT);
			return state->mOrigDevice->GetRenderTargetData(resolve_aa, dstSur);
		}
	}
	else if ((desc->Usage & D3DUSAGE_DEPTHSTENCIL)){
			IDirect3DSurface9 *intermediate;
			state->mOrigDevice->CreateRenderTarget(desc->Width, desc->Height, desc->Format, D3DMULTISAMPLE_NONE, 0, false, &intermediate, NULL);
			if (D3DXLoadSurfaceFromSurface(intermediate, NULL, NULL, srcSur, NULL, NULL, D3DX_DEFAULT, 0 == E_FAIL))
				state->mHackerDevice->NVAPIStretchRect(srcSur, intermediate, NULL, NULL);
			return state->mOrigDevice->GetRenderTargetData(intermediate, dstSur);
	}
	else {
		return D3DXLoadSurfaceFromSurface(dstSur, NULL, NULL, srcSur, NULL, NULL, D3DX_DEFAULT, 0);
	}
}
HRESULT mapUpdateDestResource(CommandListState *state, D3D3DTEXTURE_DESC *desc, IDirect3DVolumeTexture9 *srcRs, IDirect3DVolumeTexture9 *dstRs) {
	IDirect3DVolume9 *srcSur;
	srcRs->GetVolumeLevel(0, &srcSur);
	IDirect3DVolume9 *dstSur;
	dstRs->GetVolumeLevel(0, &dstSur);
	return D3DXLoadVolumeFromVolume(dstSur, NULL, NULL, srcSur, NULL, NULL, D3DX_DEFAULT, 0);
}
HRESULT mapUpdateDestResource(CommandListState *state, D3D2DTEXTURE_DESC *desc, IDirect3DSurface9 *srcSur, IDirect3DTexture9 *dstRs) {
	IDirect3DSurface9 *dstSur;
	dstRs->GetSurfaceLevel(0, &dstSur);
	return mapUpdateDestResource(state, desc, srcSur, dstSur);
}
HRESULT mapUpdateDestResource(CommandListState *state, D3D2DTEXTURE_DESC *desc, IDirect3DTexture9 *srcRs, IDirect3DTexture9 *dstRs) {
	IDirect3DSurface9 *srcSur;
	srcRs->GetSurfaceLevel(0, &srcSur);
	IDirect3DSurface9 *dstSur;
	dstRs->GetSurfaceLevel(0, &dstSur);
	return mapUpdateDestResource(state, desc, srcSur, dstSur);
}
HRESULT mapUpdateDestResource(CommandListState *state, D3D2DTEXTURE_DESC *desc, IDirect3DSurface9 *srcSur, IDirect3DCubeTexture9 *dstRs) {
	IDirect3DSurface9 *dstSur;
	dstRs->GetCubeMapSurface(D3DCUBEMAP_FACE_POSITIVE_X, 0, &dstSur);
	return mapUpdateDestResource(state, desc, srcSur, dstSur);
}
HRESULT mapUpdateDestResource(CommandListState *state, D3D2DTEXTURE_DESC *desc, IDirect3DCubeTexture9 *srcRs, IDirect3DCubeTexture9 *dstRs) {
	IDirect3DSurface9 *srcSur;
	srcRs->GetCubeMapSurface(D3DCUBEMAP_FACE_POSITIVE_X, 0, &srcSur);
	IDirect3DSurface9 *dstSur;
	dstRs->GetCubeMapSurface(D3DCUBEMAP_FACE_POSITIVE_X, 0, &dstSur);
	return mapUpdateDestResource(state, desc, srcSur, dstSur);
}
HRESULT mapCreateDestResource(CommandListState *state, D3D2DTEXTURE_DESC *desc, IDirect3DSurface9 *sur) {
	return state->mOrigDevice->CreateOffscreenPlainSurface(desc->Width, desc->Height, desc->Format, D3DPOOL::D3DPOOL_SYSTEMMEM, &sur, NULL);
}
HRESULT mapCreateDestResource(CommandListState *state, D3D2DTEXTURE_DESC *desc, IDirect3DTexture9 *sur) {
	return state->mOrigDevice->CreateTexture(desc->Width, desc->Height, 1, 0, desc->Format, D3DPOOL::D3DPOOL_SYSTEMMEM, &sur, NULL);
}

HRESULT mapCreateDestResource(CommandListState *state, D3D3DTEXTURE_DESC *desc, IDirect3DVolumeTexture9 *sur) {
	return state->mOrigDevice->CreateVolumeTexture(desc->Width, desc->Height, desc->Depth, 1, 0, desc->Format, D3DPOOL::D3DPOOL_SYSTEMMEM, &sur, NULL);
}

HRESULT mapCreateDestResource(CommandListState *state, D3D2DTEXTURE_DESC *desc, IDirect3DCubeTexture9 *sur) {
	return state->mOrigDevice->CreateCubeTexture(desc->Width, 1, 0, desc->Format, D3DPOOL::D3DPOOL_SYSTEMMEM, &sur, NULL);
}

template<typename SourceSurface, typename DestSurface, typename Desc, typename LockedRect>
inline HRESULT ResourceStagingOperation::mapSurface(CommandListState * state, SourceSurface * cached_surface, void ** mapping, DWORD flags)
{
	LockedRect pLockedRect;
	Desc desc;
	bool lockable = true;
	HRESULT hr;
	GetResourceDesc(cached_surface, &desc);
	mapped_resource_type = desc.Type;
	mapped_resource = cached_surface;
	hr = mapLock((SourceSurface*)mapped_resource, &pLockedRect, mapping, flags);
	return hr;
}

template<typename BufferType, typename Desc>
HRESULT ResourceStagingOperation::mapBuffer(CommandListState * state, BufferType * cached_buffer, void ** mapping, DWORD flags)
{
	Desc desc;
	HRESULT hr;
	GetResourceDesc(cached_buffer, &desc);
	mapped_resource_type = desc.Type;
	mapped_resource = cached_buffer;
	hr = ((BufferType*)cached_buffer)->Lock(0, desc.Size, mapping, flags);
	return hr;
}

HRESULT mapLock(IDirect3DSurface9 *sur, D3DLOCKED_RECT *pRect, void **mapping, DWORD flags = NULL) {
	HRESULT hr;
	hr =  sur->LockRect(pRect, NULL, flags);
	*mapping = pRect->pBits;
	return hr;
}

HRESULT mapLock(IDirect3DTexture9 *tex, D3DLOCKED_RECT *pRect, void **mapping, DWORD flags = NULL) {
	HRESULT hr;
	hr =  tex->LockRect(0, pRect, NULL, flags);
	*mapping = pRect->pBits;
	return hr;
}

HRESULT mapLock(IDirect3DCubeTexture9 *tex, D3DLOCKED_RECT *pRect, void **mapping, DWORD flags = NULL) {
	HRESULT hr;
	hr = tex->LockRect(D3DCUBEMAP_FACE_POSITIVE_X, 0, pRect, NULL, flags);
	*mapping = pRect->pBits;
	return hr;
}

HRESULT mapLock(IDirect3DVolumeTexture9 *tex, D3DLOCKED_BOX *pBox, void **mapping, DWORD flags = NULL) {

	HRESULT hr;
	hr= tex->LockBox(0, pBox, NULL, flags);
	*mapping = pBox->pBits;
	return hr;

}

HRESULT mapLock(IDirect3DVolume9 *vol, D3DLOCKED_BOX *pBox, void **mapping, DWORD flags = NULL) {

	HRESULT hr;
	hr = vol->LockBox(pBox, NULL, flags);
	*mapping = pBox->pBits;
	return hr;

}
HRESULT ResourceStagingOperation::map(CommandListState *state, void **mapping)
{
	if (!cached_resource)
		return E_FAIL;
	DWORD flags = D3DLOCK_READONLY | D3DLOCK_DONOTWAIT;
	HRESULT hr;
	switch (cached_resource->GetType()) {
	case D3DRTYPE_SURFACE:
		return mapSurface<IDirect3DSurface9, IDirect3DSurface9, D3D2DTEXTURE_DESC, D3DLOCKED_RECT>(state, (IDirect3DSurface9*)cached_resource, mapping, flags);
		case D3DRTYPE_TEXTURE:
		return mapSurface<IDirect3DTexture9, IDirect3DTexture9, D3D2DTEXTURE_DESC, D3DLOCKED_RECT>(state, (IDirect3DTexture9*)cached_resource, mapping, flags);
	case D3DRTYPE_CUBETEXTURE:
		return mapSurface<IDirect3DCubeTexture9, IDirect3DCubeTexture9, D3D2DTEXTURE_DESC, D3DLOCKED_RECT>(state, (IDirect3DCubeTexture9*)cached_resource, mapping, flags);
	case D3DRTYPE_VOLUMETEXTURE:
		return mapSurface<IDirect3DVolumeTexture9, IDirect3DVolumeTexture9, D3D3DTEXTURE_DESC, D3DLOCKED_BOX>(state, (IDirect3DVolumeTexture9*)cached_resource, mapping,flags);
	case D3DRTYPE_VERTEXBUFFER:
		return mapBuffer<IDirect3DVertexBuffer9, D3DVERTEXBUFFER_DESC>(state, (IDirect3DVertexBuffer9*)cached_resource, mapping, flags);
	case D3DRTYPE_INDEXBUFFER:
		return mapBuffer<IDirect3DIndexBuffer9, D3DINDEXBUFFER_DESC>(state, (IDirect3DIndexBuffer9*)cached_resource, mapping, flags);
	default:
		return E_NOTIMPL;
	}
	return hr;
}

void ResourceStagingOperation::unmap(CommandListState *state)
{
	if (mapped_resource)
		switch (mapped_resource_type) {
		case D3DRTYPE_SURFACE:
			((IDirect3DSurface9*)mapped_resource)->UnlockRect();
			break;
		case D3DRTYPE_VOLUME:
			((IDirect3DVolume9*)mapped_resource)->UnlockBox();
			break;
		case D3DRTYPE_TEXTURE:
			((IDirect3DTexture9*)mapped_resource)->UnlockRect(0);
			break;
		case D3DRTYPE_VOLUMETEXTURE:
			((IDirect3DVolumeTexture9*)mapped_resource)->UnlockBox(0);
			break;
		case D3DRTYPE_CUBETEXTURE:
			((IDirect3DCubeTexture9*)mapped_resource)->UnlockRect(D3DCUBEMAP_FACE_POSITIVE_X, 0);
			break;
		case D3DRTYPE_VERTEXBUFFER:
			((IDirect3DVertexBuffer9*)mapped_resource)->Unlock();
			break;
		case D3DRTYPE_INDEXBUFFER:
			((IDirect3DIndexBuffer9*)mapped_resource)->Unlock();
			break;
		}

	if (cached_resource != mapped_resource)
		mapped_resource->Release();
}

HRESULT AddDirtyRect(IDirect3DTexture9 *tex, RECT *rect) {
	return tex->AddDirtyRect(rect);
}

HRESULT AddDirtyRect(IDirect3DVolumeTexture9 *tex, D3DBOX *box) {
	return tex->AddDirtyBox(box);
}

HRESULT AddDirtyRect(IDirect3DCubeTexture9 *tex, RECT *rect) {
	HRESULT hr;
	hr = tex->AddDirtyRect(D3DCUBEMAP_FACE_POSITIVE_X, rect);
	if (FAILED(hr))
		return hr;
	hr = tex->AddDirtyRect(D3DCUBEMAP_FACE_POSITIVE_Y, rect);
	if (FAILED(hr))
		return hr;
	hr = tex->AddDirtyRect(D3DCUBEMAP_FACE_POSITIVE_Z, rect);
	if (FAILED(hr))
		return hr;
	hr = tex->AddDirtyRect(D3DCUBEMAP_FACE_NEGATIVE_X, rect);
	if (FAILED(hr))
		return hr;
	hr = tex->AddDirtyRect(D3DCUBEMAP_FACE_NEGATIVE_Y, rect);
	if (FAILED(hr))
		return hr;
	hr = tex->AddDirtyRect(D3DCUBEMAP_FACE_NEGATIVE_Z, rect);
	return hr;
}
HRESULT GetLevel(IDirect3DTexture9 *tex, UINT level, IDirect3DSurface9 **ppLev) {
	return tex->GetSurfaceLevel(level, ppLev);
}

HRESULT GetLevel(IDirect3DVolumeTexture9 *tex, UINT level, IDirect3DVolume9 **ppLev) {
	return tex->GetVolumeLevel(0, ppLev);
}
HRESULT GetLevel(IDirect3DCubeTexture9 *tex, UINT face, UINT level, IDirect3DSurface9 **ppLev) {
	return tex->GetCubeMapSurface((D3DCUBEMAP_FACES)face, level, ppLev);
}
template<CopyLevelSur c>
HRESULT CopyLevelSurface<c>::copyLevel(D3D9Wrapper::IDirect3DDevice9 * mHackerDevice, IDirect3DSurface9 * srcLev, IDirect3DSurface9 * dstLev, D3D2DTEXTURE_DESC * srcDesc, D3D2DTEXTURE_DESC * dstDesc, RECT * srcRect, RECT * dstRect)
{
	return c(mHackerDevice, srcLev, dstLev, srcDesc, dstDesc, srcRect, dstRect);
};
template<CopyLevelVol c>
HRESULT CopyLevelVolume<c>::copyLevel(D3D9Wrapper::IDirect3DDevice9 * mHackerDevice, IDirect3DVolume9 * srcLev, IDirect3DVolume9 * dstLev, D3D3DTEXTURE_DESC * srcDesc, D3D3DTEXTURE_DESC * dstDesc, D3DBOX * srcRect, D3DBOX * dstRect)
{
	return c(mHackerDevice, srcLev, dstLev, srcDesc, dstDesc, srcRect, dstRect);
};
bool isCompressedFormat(D3DFORMAT fmt) {

	switch (fmt) {
	case D3DFMT_DXT1:
	case D3DFMT_DXT2:
	case D3DFMT_DXT3:
	case D3DFMT_DXT4:
	case D3DFMT_DXT5:
	case D3DFMT_G8R8_G8B8:
	case D3DFMT_R8G8_B8G8:
	case D3DFMT_UYVY:
	case D3DFMT_YUY2:
		return true;
	default:
		return false;
	}
}

D3DFORMAT ensureNonCompressedFormat(D3DFORMAT fmt) {

	switch (fmt) {
	case D3DFMT_DXT1:
	case D3DFMT_DXT2:
	case D3DFMT_DXT3:
	case D3DFMT_DXT4:
	case D3DFMT_DXT5:
	case D3DFMT_G8R8_G8B8:
	case D3DFMT_R8G8_B8G8:
	case D3DFMT_UYVY:
	case D3DFMT_YUY2:
		return D3DFMT_A8R8G8B8;
	default:
		return fmt;
	}
}

bool d3dformat_stretchRect_target(D3DFORMAT fmt) {

	switch (fmt) {
	case D3DFMT_X1R5G5B5:
	case D3DFMT_R8G8B8:
	case D3DFMT_A2R10G10B10:
	case D3DFMT_A8B8G8R8:
	case D3DFMT_A32B32G32R32F:
	case D3DFMT_A1R5G5B5:
	case D3DFMT_X8R8G8B8:
	case D3DFMT_A16B16G16R16:
	case D3DFMT_X8B8G8R8:
	case D3DFMT_R5G6B5:
	case D3DFMT_A8R8G8B8:
	case D3DFMT_A2B10G10R10:
	case D3DFMT_A16B16G16R16F:
		return true;
	default:
		return false;
	}
}
bool d3dformat_stretchRect_source(D3DFORMAT fmt) {

	switch (fmt) {
	case D3DFMT_MULTI2_ARGB8:
	case D3DFMT_G8R8_G8B8:
	case D3DFMT_R8G8_B8G8:
	case D3DFMT_UYVY:
	case D3DFMT_YUY2:
		//A2R10G10B10
	case D3DFORMAT(35):
		//A8R8G8B8
	case D3DFORMAT(21):
		//X8R8G8B8
	case D3DFORMAT(22):
		//A1R5G5B5
	case D3DFORMAT(25):
		//X1R5G5B5
	case D3DFORMAT(24):
		//R5G6B5
	case D3DFORMAT(23):
		return true;
	default:
		return false;
	}
}
bool CanGetRenderTarget(IDirect3DDevice9 * mOrigDevice, D3D2DTEXTURE_DESC * srcDesc, D3D2DTEXTURE_DESC * dstDesc)
{
	if (srcDesc->Format != dstDesc->Format)
		return false;
	if (srcDesc->Width != dstDesc->Width || srcDesc->Height != dstDesc->Height)
		return false;
	if ((dstDesc->Usage & D3DUSAGE_RENDERTARGET) || (dstDesc->Usage & D3DUSAGE_DEPTHSTENCIL))
		return false;
	if (!(srcDesc->Usage & D3DUSAGE_RENDERTARGET))
		return false;
	return true;
}
bool CanDirectXStretchRectDS(IDirect3DDevice9 * mOrigDevice, D3D2DTEXTURE_DESC * srcDesc, D3D2DTEXTURE_DESC * dstDesc, RECT *srcRect, RECT *dstRect)
{
	if (srcRect || dstRect)
		return false;
	if (srcDesc->Width != dstDesc->Width || srcDesc->Height != dstDesc->Height)
		return false;
	if (srcDesc->Format != dstDesc->Format)
		return false;
	if (dstDesc->Type == D3DRTYPE_CUBETEXTURE || dstDesc->Type == D3DRTYPE_TEXTURE)
	{
		return false;
	}
	if (srcDesc->Type == D3DRTYPE_CUBETEXTURE || srcDesc->Type == D3DRTYPE_TEXTURE)
	{
		return false;
	}
	return true;
}
bool CanDirectXStretchRectRT(IDirect3DDevice9 * mOrigDevice, D3D2DTEXTURE_DESC * srcDesc, D3D2DTEXTURE_DESC * dstDesc)
{
	if (isCompressedFormat(srcDesc->Format) || isCompressedFormat(dstDesc->Format)) {
		return false;
	}
	if (srcDesc->Format != dstDesc->Format) {
		if (!(d3dformat_stretchRect_target(dstDesc->Format)) || !(d3dformat_stretchRect_source(srcDesc->Format))) {
			return false;
		}
		else {
			IDirect3D9 *pD3D9;
			mOrigDevice->GetDirect3D(&pD3D9);
			HRESULT hr = pD3D9->CheckDeviceFormatConversion(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, srcDesc->Format, dstDesc->Format);
			if (FAILED(hr)) {
				return false;
			}
			else if (hr == D3DERR_NOTAVAILABLE) {
				return false;
			}
			else {
				return true;
			}

		}
	}
	D3DCAPS9 d3dCaps;
	mOrigDevice->GetDeviceCaps(&d3dCaps);
	if (dstDesc->Type == D3DRTYPE_CUBETEXTURE || dstDesc->Type == D3DRTYPE_TEXTURE)
	{
		if (!(d3dCaps.DevCaps2 & D3DDEVCAPS2_CAN_STRETCHRECT_FROM_TEXTURES)) {
			return false;
		}
		if ((dstDesc->Usage & D3DUSAGE_RENDERTARGET)) {
			return true;
		}
		else {

			return false;
		}

	}

	if (srcDesc->Type == D3DRTYPE_CUBETEXTURE || srcDesc->Type == D3DRTYPE_TEXTURE)
	{
		if (!(d3dCaps.DevCaps2 & D3DDEVCAPS2_CAN_STRETCHRECT_FROM_TEXTURES)) {
			return false;
		}
		if (dstDesc->Usage & D3DUSAGE_RENDERTARGET) {
			return true;
		}
		else {
			return false;
		}
	}
	if ((srcDesc->Usage & D3DUSAGE_RENDERTARGET) && !(dstDesc->Usage & D3DUSAGE_RENDERTARGET)) {
		return false;
	}
	else if (!(srcDesc->Usage & D3DUSAGE_RENDERTARGET) && !(dstDesc->Usage & D3DUSAGE_RENDERTARGET)) {
		if ((srcDesc->Width != dstDesc->Width) || (srcDesc->Height != dstDesc->Height)) {
			return false;
		}
		else {
			return true;
		}
	}
	else {
		return true;
	}
}
template <typename Texture, typename Surface, typename Desc, typename Rect, typename CopyLevel>
HRESULT CopyAllLevelsOfTexture(D3D9Wrapper::IDirect3DDevice9 *mHackerDevice, IDirect3DCubeTexture9 *src, IDirect3DCubeTexture9 *dst, D3D2DTEXTURE_DESC *srcDesc, D3D2DTEXTURE_DESC *dstDesc = NULL, RECT *srcRect = NULL, RECT *dstRect = NULL) {
	HRESULT outputHR = S_OK;
	for (int face = 0; face < 6; face++) {
		for (UINT x = 0; x < srcDesc->Levels; x++) {
			HRESULT hr;
			IDirect3DSurface9 *srcLev = NULL;
			hr = GetLevel(src, face, x, &srcLev);
			if (FAILED(hr))
				outputHR = hr;
			IDirect3DSurface9 *dstLev = NULL;
			hr = GetLevel(dst, face, x, &dstLev);
			if (FAILED(hr))
				outputHR = hr;
			Rect *levelSrcRect = NULL;
			Rect *levelDstRect = NULL;
			Rect calcSrcRect;
			Rect calcDstRect;
			if (srcRect && x != 0) {
				LevelRect(srcRect, &calcSrcRect, x);
				levelSrcRect = &calcSrcRect;
			}
			if (dstRect && x != 0) {
				LevelRect(dstRect, &calcDstRect, x);
				levelDstRect = &calcDstRect;
			}
			hr = CopyLevel::copyLevel(mHackerDevice, srcLev, dstLev, srcDesc, dstDesc, levelSrcRect, levelDstRect);
			if (FAILED(hr))
				outputHR = hr;
			srcLev->Release();
			dstLev->Release();
		}
	}
	return outputHR;
}

template <typename Texture, typename Surface, typename Desc, typename Rect, typename CopyLevel>
HRESULT CopyAllLevelsOfTexture(D3D9Wrapper::IDirect3DDevice9 *mHackerDevice, Texture *src, Texture *dst, Desc *srcDesc, Desc *dstDesc = NULL, Rect *srcRect = NULL, Rect *dstRect = NULL) {
	HRESULT outputHR = S_OK;
	for (UINT x = 0; x < srcDesc->Levels; x++) {
		HRESULT hr;
		Surface *srcLev = NULL;
		hr = GetLevel(src, x, &srcLev);
		if (FAILED(hr))
			outputHR = hr;
		Surface *dstLev = NULL;
		hr = GetLevel(dst, x, &dstLev);
		if (FAILED(hr))
			outputHR = hr;
		Rect *levelSrcRect = NULL;
		Rect *levelDstRect = NULL;
		Rect calcSrcRect;
		Rect calcDstRect;
		if (srcRect && x != 0) {
			LevelRect(srcRect, &calcSrcRect, x);
			levelSrcRect = &calcSrcRect;
		}
		if (dstRect && x != 0) {
			LevelRect(dstRect, &calcDstRect, x);
			levelDstRect = &calcDstRect;
		}
		hr = CopyLevel::copyLevel(mHackerDevice, srcLev, dstLev, srcDesc, dstDesc, levelSrcRect, levelDstRect);
		if (FAILED(hr))
			outputHR = hr;
		srcLev->Release();
		dstLev->Release();
	}

	return outputHR;
}
void LevelRect(RECT *inputRect, RECT *outputRect, UINT level) {

	RECT oRect;
	oRect.left = inputRect->left >> level;
	oRect.right = inputRect->right >> level;
	oRect.top = inputRect->top >> level;
	oRect.bottom = inputRect->bottom >> level;
	*outputRect = oRect;
}
void LevelRect(D3DBOX *inputRect, D3DBOX *outputRect, UINT level) {
	D3DBOX oBox;
	oBox.Left = inputRect->Left >> level;
	oBox.Right = inputRect->Right >> level;
	oBox.Top = inputRect->Top >> level;
	oBox.Bottom = inputRect->Bottom >> level;
	oBox.Front = inputRect->Front >> level;
	oBox.Bottom = inputRect->Bottom >> level;
	*outputRect = oBox;
}
HRESULT _ResolveMSAA(D3D9Wrapper::IDirect3DDevice9 *mHackerDevice, IDirect3DSurface9 *src, IDirect3DSurface9 *dst, D3D2DTEXTURE_DESC *srcDesc, D3D2DTEXTURE_DESC *dstDesc, RECT *srcRect, RECT *dstRect) {
	HRESULT hr;
	if (srcDesc->Pool == D3DPOOL::D3DPOOL_DEFAULT) {
		if (CanDirectXStretchRectRT(mHackerDevice->GetD3D9Device(), srcDesc, dstDesc)) {
			return mHackerDevice->GetD3D9Device()->StretchRect(src, srcRect, dst, NULL, D3DTEXF_POINT);
		}
		else {
			hr = mHackerDevice->NVAPIStretchRect(src, dst, srcRect, NULL);
			if (FAILED(hr))
				hr = D3DXLoadSurfaceFromSurface(dst, NULL, NULL, src, NULL, srcRect, D3DX_DEFAULT, 0);
			return hr;
		}
	}
	else {
		UINT width;
		UINT height;
		if (srcRect != NULL) {
			width = srcRect->right - srcRect->left;
			height = srcRect->bottom - srcRect->top;
		}
		else {
			width = srcDesc->Width;
			height = srcDesc->Height;
		}
		IDirect3DSurface9 *rmLev;
		hr = mHackerDevice->GetD3D9Device()->CreateRenderTarget(width, height, srcDesc->Format, srcDesc->MultiSampleType, srcDesc->MultiSampleQuality, false, &rmLev, NULL);
		if (FAILED(hr))
			return hr;
		hr = D3DXLoadSurfaceFromSurface(rmLev, NULL, NULL, src, NULL, srcRect, D3DX_DEFAULT, 0);
		if (FAILED(hr)) {
			rmLev->Release();
			return hr;
		}
		D3D2DTEXTURE_DESC rmDesc;
		rmDesc.Format = srcDesc->Format;
		rmDesc.Height = height;
		rmDesc.Width = width;
		rmDesc.MultiSampleQuality = srcDesc->MultiSampleQuality;
		rmDesc.MultiSampleType = srcDesc->MultiSampleType;
		rmDesc.Pool = D3DPOOL_DEFAULT;
		rmDesc.Type = D3DRTYPE_SURFACE;
		rmDesc.Usage = D3DUSAGE_RENDERTARGET;
		rmDesc.Levels = 1;

		if (CanDirectXStretchRectRT(mHackerDevice->GetD3D9Device(), &rmDesc, dstDesc)) {
			hr = mHackerDevice->GetD3D9Device()->StretchRect(rmLev, NULL, dst, NULL, D3DTEXF_POINT);
		}
		else {
			hr = D3DXLoadSurfaceFromSurface(dst, NULL, NULL, rmLev, NULL, NULL, D3DX_DEFAULT, 0);
			if (FAILED(hr))
				hr = mHackerDevice->NVAPIStretchRect(rmLev, dst, NULL, NULL);
		}
		rmLev->Release();
	}
	return hr;
}

HRESULT _IntermediateResolveMSAA(D3D9Wrapper::IDirect3DDevice9 *mHackerDevice, IDirect3DSurface9 *src, IDirect3DSurface9 *dst, D3D2DTEXTURE_DESC *srcDesc, D3D2DTEXTURE_DESC *dstDesc, RECT *srcRect, RECT *dstRect) {
	HRESULT hr;
	UINT width;
	UINT height;
	if (srcRect != NULL) {
		width = srcRect->right - srcRect->left;
		height = srcRect->bottom - srcRect->top;
	}
	else {
		width = srcDesc->Width;
		height = srcDesc->Height;
	}
	hr = mHackerDevice->GetD3D9Device()->CreateRenderTarget(width, height, srcDesc->Format, D3DMULTISAMPLE_NONE, 0, false, &dst, NULL);
	if (FAILED(hr)) {
		return hr;
	}

	if (srcDesc->Pool == D3DPOOL::D3DPOOL_DEFAULT) {
		D3D2DTEXTURE_DESC dstDesc;
		dstDesc.Format = srcDesc->Format;
		dstDesc.Height = height;
		dstDesc.MultiSampleQuality = 0;
		dstDesc.MultiSampleType = D3DMULTISAMPLE_NONE;
		dstDesc.Pool = D3DPOOL_DEFAULT;
		dstDesc.Type = D3DRTYPE_SURFACE;
		dstDesc.Usage = D3DUSAGE_RENDERTARGET;
		dstDesc.Levels = 1;
		if (CanDirectXStretchRectRT(mHackerDevice->GetD3D9Device(), srcDesc, &dstDesc)) {
			return mHackerDevice->GetD3D9Device()->StretchRect(src, srcRect, dst, NULL, D3DTEXF_POINT);
		}
		else {
			hr = D3DXLoadSurfaceFromSurface(dst, NULL, NULL, src, NULL, srcRect, D3DX_DEFAULT, 0);
			if (FAILED(hr))
				hr = mHackerDevice->NVAPIStretchRect(src, dst, srcRect, NULL);

			return hr;
		}
	}
	else {

		IDirect3DSurface9 *rmLev;
		hr = mHackerDevice->GetD3D9Device()->CreateRenderTarget(width, height, srcDesc->Format, srcDesc->MultiSampleType, srcDesc->MultiSampleQuality, false, &rmLev, NULL);
		if (FAILED(hr))
			return hr;
		hr = D3DXLoadSurfaceFromSurface(rmLev, NULL, NULL, src, NULL, srcRect, D3DX_DEFAULT, 0);
		if (FAILED(hr)) {
			rmLev->Release();
			return hr;
		}
		D3D2DTEXTURE_DESC iDesc;
		iDesc.Format = srcDesc->Format;
		iDesc.Height = height;
		iDesc.Width = width;
		iDesc.MultiSampleQuality = srcDesc->MultiSampleQuality;
		iDesc.MultiSampleType = srcDesc->MultiSampleType;
		iDesc.Pool = D3DPOOL_DEFAULT;
		iDesc.Type = D3DRTYPE_SURFACE;
		iDesc.Usage = D3DUSAGE_RENDERTARGET;
		iDesc.Levels = 1;
		D3D2DTEXTURE_DESC dstDesc;
		dstDesc.Format = srcDesc->Format;
		dstDesc.Height = height;
		dstDesc.MultiSampleQuality = 0;
		dstDesc.MultiSampleType = D3DMULTISAMPLE_NONE;
		dstDesc.Pool = D3DPOOL_DEFAULT;
		dstDesc.Type = D3DRTYPE_SURFACE;
		dstDesc.Usage = D3DUSAGE_RENDERTARGET;
		dstDesc.Levels = 1;
		if (CanDirectXStretchRectRT(mHackerDevice->GetD3D9Device(), &iDesc, &dstDesc)) {
			hr = mHackerDevice->GetD3D9Device()->StretchRect(rmLev, NULL, dst, NULL, D3DTEXF_POINT);
		}
		else {
			hr = D3DXLoadSurfaceFromSurface(dst, NULL, NULL, rmLev, NULL, srcRect, D3DX_DEFAULT, 0);
			if (FAILED(hr))
				hr = mHackerDevice->NVAPIStretchRect(rmLev, dst, NULL, NULL);

		}
		rmLev->Release();
	}
	return hr;
}
HRESULT IntermediateResolveMSAA(D3D9Wrapper::IDirect3DDevice9 *mHackerDevice, IDirect3DSurface9 *src,  IDirect3DSurface9 *dst, D3D2DTEXTURE_DESC *srcDesc, D3D2DTEXTURE_DESC *dstDesc, RECT *srcRect, RECT *dstRect) {
	return _IntermediateResolveMSAA(mHackerDevice, src, dst, srcDesc, dstDesc, srcRect, dstRect);
}

HRESULT IntermediateResolveMSAA(D3D9Wrapper::IDirect3DDevice9 *mHackerDevice,  IDirect3DSurface9 *src, IDirect3DTexture9 *dst, D3D2DTEXTURE_DESC *srcDesc, D3D2DTEXTURE_DESC *dstDesc, RECT *srcRect, RECT *dstRect) {
	HRESULT hr;
	IDirect3DSurface9 *dstSur = NULL;
	GetLevel(dst, 0, &dstSur);
	hr = _IntermediateResolveMSAA(mHackerDevice, src, dstSur, srcDesc, dstDesc, srcRect, dstRect);
	dstSur->Release();
	return hr;
}

HRESULT IntermediateResolveMSAA(D3D9Wrapper::IDirect3DDevice9 *mHackerDevice, IDirect3DSurface9 *src, IDirect3DCubeTexture9 *dst, D3D2DTEXTURE_DESC *srcDesc, D3D2DTEXTURE_DESC *dstDesc, RECT *srcRect, RECT *dstRect) {
	HRESULT hr;
	IDirect3DSurface9 *dstSur = NULL;
	GetLevel(dst, 0, 0, &dstSur);
	hr = _IntermediateResolveMSAA(mHackerDevice, src, dstSur, srcDesc, dstDesc, srcRect, dstRect);
	dstSur->Release();
	return hr;
}

HRESULT IntermediateResolveMSAA(D3D9Wrapper::IDirect3DDevice9 *mHackerDevice,  IDirect3DTexture9 *src,IDirect3DSurface9 *dst, D3D2DTEXTURE_DESC *srcDesc, D3D2DTEXTURE_DESC *dstDesc, RECT *srcRect, RECT *dstRect) {
	HRESULT hr;
	IDirect3DSurface9 *srcSur = NULL;
	GetLevel(src, 0, &srcSur);
	hr = _IntermediateResolveMSAA(mHackerDevice, srcSur, dst, srcDesc, dstDesc, srcRect, dstRect);
	srcSur->Release();
	return hr;
}

HRESULT IntermediateResolveMSAA(D3D9Wrapper::IDirect3DDevice9 *mHackerDevice,  IDirect3DCubeTexture9 *src, IDirect3DSurface9 *dst, D3D2DTEXTURE_DESC *srcDesc, D3D2DTEXTURE_DESC *dstDesc, RECT *srcRect, RECT *dstRect) {
	HRESULT hr;
	IDirect3DSurface9 *srcSur = NULL;
	GetLevel(src, 0, 0, &srcSur);
	hr = _IntermediateResolveMSAA(mHackerDevice, srcSur, dst, srcDesc, dstDesc, srcRect, dstRect);
	srcSur->Release();
	return hr;
}

HRESULT IntermediateResolveMSAA(D3D9Wrapper::IDirect3DDevice9 *mHackerDevice, IDirect3DCubeTexture9 *src, IDirect3DCubeTexture9 *dst, D3D2DTEXTURE_DESC *srcDesc, D3D2DTEXTURE_DESC *dstDesc, RECT *srcRect, RECT *dstRect) {
	return CopyAllLevelsOfTexture<IDirect3DCubeTexture9, IDirect3DSurface9, D3D2DTEXTURE_DESC, RECT, CopyLevelSurface<_IntermediateResolveMSAA>>(mHackerDevice, src, dst, srcDesc, dstDesc, srcRect, dstRect);
}

HRESULT IntermediateResolveMSAA(D3D9Wrapper::IDirect3DDevice9 *mHackerDevice, IDirect3DTexture9 *src, IDirect3DTexture9 *dst, D3D2DTEXTURE_DESC *srcDesc, D3D2DTEXTURE_DESC *dstDesc, RECT *srcRect, RECT *dstRect) {
	return CopyAllLevelsOfTexture<IDirect3DTexture9, IDirect3DSurface9, D3D2DTEXTURE_DESC, RECT, CopyLevelSurface<_IntermediateResolveMSAA>>(mHackerDevice, src, dst, srcDesc, dstDesc, srcRect, dstRect);
}
HRESULT ResolveMSAA(D3D9Wrapper::IDirect3DDevice9 *mHackerDevice, IDirect3DTexture9 *src, IDirect3DTexture9 *dst, D3D2DTEXTURE_DESC *srcDesc, D3D2DTEXTURE_DESC *dstDesc, RECT *srcRect, RECT *dstRect) {
	return CopyAllLevelsOfTexture<IDirect3DTexture9, IDirect3DSurface9, D3D2DTEXTURE_DESC, RECT, CopyLevelSurface<_ResolveMSAA>>(mHackerDevice, src, dst, srcDesc, dstDesc, srcRect, dstRect);
}
HRESULT ResolveMSAA(D3D9Wrapper::IDirect3DDevice9 *mHackerDevice, IDirect3DCubeTexture9 *src, IDirect3DCubeTexture9 *dst, D3D2DTEXTURE_DESC *srcDesc, D3D2DTEXTURE_DESC *dstDesc, RECT *srcRect, RECT *dstRect) {
	return CopyAllLevelsOfTexture<IDirect3DCubeTexture9, IDirect3DSurface9, D3D2DTEXTURE_DESC, RECT, CopyLevelSurface<_ResolveMSAA>>(mHackerDevice, src,  dst, srcDesc, dstDesc, srcRect, dstRect);
}
HRESULT ResolveMSAA(D3D9Wrapper::IDirect3DDevice9 *mHackerDevice, IDirect3DSurface9 *src, IDirect3DSurface9 *dst, D3D2DTEXTURE_DESC *srcDesc, D3D2DTEXTURE_DESC *dstDesc, RECT *srcRect, RECT *dstRect) {
	return _ResolveMSAA(mHackerDevice, src,  dst, srcDesc, dstDesc, srcRect, dstRect);
}
HRESULT ResolveMSAA(D3D9Wrapper::IDirect3DDevice9 *mHackerDevice, IDirect3DSurface9 *src, IDirect3DTexture9 *dst, D3D2DTEXTURE_DESC *srcDesc, D3D2DTEXTURE_DESC *dstDesc, RECT *srcRect, RECT *dstRect) {
	HRESULT hr;
	IDirect3DSurface9 *dstSur = NULL;
	GetLevel(dst, 0, &dstSur);
	hr =  _ResolveMSAA(mHackerDevice, src,  dstSur, srcDesc, dstDesc, srcRect, dstRect);
	dstSur->Release();
	return hr;
}

HRESULT ResolveMSAA(D3D9Wrapper::IDirect3DDevice9 *mHackerDevice, IDirect3DSurface9 *src, IDirect3DCubeTexture9 *dst, D3D2DTEXTURE_DESC *srcDesc, D3D2DTEXTURE_DESC *dstDesc, RECT *srcRect, RECT *dstRect) {
	HRESULT hr;
	IDirect3DSurface9 *dstSur = NULL;
	GetLevel(dst, 0, 0, &dstSur);
	hr = _ResolveMSAA(mHackerDevice, src,  dstSur, srcDesc, dstDesc, srcRect, dstRect);
	dstSur->Release();
	return hr;
}
HRESULT ResolveMSAA(D3D9Wrapper::IDirect3DDevice9 *mHackerDevice, IDirect3DTexture9 *src,  IDirect3DSurface9 *dst, D3D2DTEXTURE_DESC *srcDesc, D3D2DTEXTURE_DESC *dstDesc, RECT *srcRect, RECT *dstRect) {
	HRESULT hr;
	IDirect3DSurface9 *srcSur = NULL;
	GetLevel(src, 0, &srcSur);
	hr = _ResolveMSAA(mHackerDevice,  srcSur,  dst, srcDesc, dstDesc, srcRect, dstRect);
	srcSur->Release();
	return hr;
}

HRESULT ResolveMSAA(D3D9Wrapper::IDirect3DDevice9 *mHackerDevice,  IDirect3DCubeTexture9 *src,  IDirect3DSurface9 *dst, D3D2DTEXTURE_DESC *srcDesc, D3D2DTEXTURE_DESC *dstDesc, RECT *srcRect, RECT *dstRect) {
	HRESULT hr;
	IDirect3DSurface9 *srcSur = NULL;
	GetLevel(src, 0, 0, &srcSur);
	hr = _ResolveMSAA(mHackerDevice,  srcSur,  dst, srcDesc, dstDesc, srcRect, dstRect);
	srcSur->Release();
	return hr;
}
static HRESULT ResolveMSAA(IDirect3DResource9 *dst_resource, IDirect3DResource9 *src_resource, CommandListState *state)
{
	D3DRESOURCETYPE srcType;
	D3DRESOURCETYPE dstType;
	HRESULT hr;
	IDirect3DSurface9 *srcSur, *dstSur;
	IDirect3DTexture9 *srcTex, *dstTex;
	IDirect3DCubeTexture9 *srcCube, *dstCube;
	D3D2DTEXTURE_DESC src_desc, dst_desc;
	srcType = src_resource->GetType();
	dstType = dst_resource->GetType();
	switch (srcType) {
	case D3DRTYPE_SURFACE:
		srcSur = (IDirect3DSurface9*)src_resource;
		src_desc = D3D2DTEXTURE_DESC(srcSur);
		switch (dstType) {
		case D3DRTYPE_SURFACE:
			dstSur = (IDirect3DSurface9*)src_resource;
			dst_desc = D3D2DTEXTURE_DESC(dstSur);
			hr = ResolveMSAA(state->mHackerDevice, srcSur, dstSur, &src_desc, &dst_desc, NULL, NULL);
			break;
		case D3DRTYPE_TEXTURE:
			dstTex = (IDirect3DTexture9*)src_resource;
			dst_desc = D3D2DTEXTURE_DESC(dstTex);
			hr = ResolveMSAA(state->mHackerDevice, srcSur, dstTex, &src_desc, &dst_desc, NULL, NULL);
			break;
		case D3DRTYPE_CUBETEXTURE:
			dstCube = (IDirect3DCubeTexture9*)src_resource;
			dst_desc = D3D2DTEXTURE_DESC(dstCube);
			hr = ResolveMSAA(state->mHackerDevice, srcSur, dstCube, &src_desc, &dst_desc, NULL, NULL);
			break;
		}
		break;
	case D3DRTYPE_TEXTURE:
		srcTex = (IDirect3DTexture9*)src_resource;
		src_desc = D3D2DTEXTURE_DESC(srcTex);
		switch (dstType) {
		case D3DRTYPE_SURFACE:
			dstSur = (IDirect3DSurface9*)src_resource;
			dst_desc = D3D2DTEXTURE_DESC(dstSur);
			hr = ResolveMSAA(state->mHackerDevice, srcTex, dstSur, &src_desc, &dst_desc, NULL, NULL);
			break;
		case D3DRTYPE_TEXTURE:
			dstTex = (IDirect3DTexture9*)src_resource;
			dst_desc = D3D2DTEXTURE_DESC(dstTex);
			hr = ResolveMSAA(state->mHackerDevice, srcTex, dstTex, &src_desc, &dst_desc, NULL, NULL);
			break;
		}
		break;
	case D3DRTYPE_CUBETEXTURE:
		srcCube = (IDirect3DCubeTexture9*)src_resource;
		src_desc = D3D2DTEXTURE_DESC(srcCube);
		switch (dstType) {
		case D3DRTYPE_SURFACE:
			dstSur = (IDirect3DSurface9*)src_resource;
			dst_desc = D3D2DTEXTURE_DESC(dstSur);
			hr = ResolveMSAA(state->mHackerDevice, srcCube, dstSur, &src_desc, &dst_desc, NULL, NULL);
			break;
		case D3DRTYPE_CUBETEXTURE:
			dstCube = (IDirect3DCubeTexture9*)src_resource;
			dst_desc = D3D2DTEXTURE_DESC(dstCube);
			hr = ResolveMSAA(state->mHackerDevice, srcCube, dstCube, &src_desc, &dst_desc, NULL, NULL);
			break;
		}
		break;
	}
	return hr;
}
HRESULT D3DXSurfaceToSurface(D3D9Wrapper::IDirect3DDevice9 *mHackerDevice,  IDirect3DSurface9 *src,  IDirect3DSurface9 *dst, D3D2DTEXTURE_DESC *srcDesc, D3D2DTEXTURE_DESC *dstDesc, RECT *srcRect, RECT *dstRect) {
	return D3DXLoadSurfaceFromSurface(dst, NULL, dstRect, src, NULL, srcRect, D3DX_DEFAULT, 0);
}

template <typename Texture>
HRESULT D3DXSurfaceToSurface(D3D9Wrapper::IDirect3DDevice9 *mHackerDevice, Texture *src, Texture *dst, D3D2DTEXTURE_DESC *srcDesc, D3D2DTEXTURE_DESC *dstDesc, RECT *srcRect, RECT *dstRect) {
	return CopyAllLevelsOfTexture<Texture, IDirect3DSurface9, D3D2DTEXTURE_DESC, RECT, CopyLevelSurface<D3DXSurfaceToSurface>>(mHackerDevice, src, dst, srcDesc, dstDesc, srcRect, dstRect);
}

HRESULT D3DXVolumeToVolume(D3D9Wrapper::IDirect3DDevice9 *mHackerDevice, IDirect3DVolume9 *src, IDirect3DVolume9 *dst, D3D3DTEXTURE_DESC *srcDesc, D3D3DTEXTURE_DESC *dstDesc, D3DBOX *srcRect, D3DBOX *dstRect) {
	return D3DXLoadVolumeFromVolume(dst, NULL, dstRect, src, NULL, srcRect, D3DTEXF_POINT, 0);
}

HRESULT D3DXSurfaceToSurface(D3D9Wrapper::IDirect3DDevice9 *mHackerDevice, IDirect3DSurface9 *src, IDirect3DTexture9 *dst, D3D2DTEXTURE_DESC *srcDesc, D3D2DTEXTURE_DESC *dstDesc, RECT *srcRect, RECT *dstRect) {
	HRESULT hr;
	IDirect3DSurface9 *dstSur = NULL;
	GetLevel(dst, 0, &dstSur);
	hr = D3DXLoadSurfaceFromSurface(dstSur, NULL, dstRect, src, NULL, srcRect, D3DX_DEFAULT, 0);
	dstSur->Release();
	return hr;
}

HRESULT D3DXSurfaceToSurface(D3D9Wrapper::IDirect3DDevice9 *mHackerDevice, IDirect3DSurface9 *src, IDirect3DCubeTexture9 *dst, D3D2DTEXTURE_DESC *srcDesc, D3D2DTEXTURE_DESC *dstDesc, RECT *srcRect, RECT *dstRect) {
	HRESULT hr;
	IDirect3DSurface9 *dstSur = NULL;
	GetLevel(dst, 0, 0, &dstSur);
	hr = D3DXLoadSurfaceFromSurface(dstSur, NULL, dstRect, src, NULL, srcRect, D3DX_DEFAULT, 0);
	dstSur->Release();
	return hr;
}
HRESULT D3DXSurfaceToSurface(D3D9Wrapper::IDirect3DDevice9 *mHackerDevice, IDirect3DTexture9 *src, IDirect3DSurface9 *dst, D3D2DTEXTURE_DESC *srcDesc, D3D2DTEXTURE_DESC *dstDesc, RECT *srcRect, RECT *dstRect) {
	HRESULT hr;
	IDirect3DSurface9 *srcSur = NULL;
	GetLevel(src, 0, &srcSur);
	hr = D3DXLoadSurfaceFromSurface(dst, NULL, dstRect, srcSur, NULL, srcRect, D3DX_DEFAULT, 0);
	srcSur->Release();
	return hr;
}

HRESULT D3DXSurfaceToSurface(D3D9Wrapper::IDirect3DDevice9 *mHackerDevice, IDirect3DCubeTexture9 *src, IDirect3DSurface9 *dst, D3D2DTEXTURE_DESC *srcDesc, D3D2DTEXTURE_DESC *dstDesc, RECT *srcRect, RECT *dstRect) {
	HRESULT hr;
	IDirect3DSurface9 *srcSur = NULL;
	GetLevel(src, 0, 0, &srcSur);
	hr = D3DXLoadSurfaceFromSurface(dst, NULL, dstRect, srcSur, NULL, srcRect, D3DX_DEFAULT, 0);
	srcSur->Release();
	return hr;
}
HRESULT GetRenderTargetData(D3D9Wrapper::IDirect3DDevice9 *mHackerDevice, IDirect3DSurface9 *src, IDirect3DSurface9 *dst, D3D2DTEXTURE_DESC *srcDesc, D3D2DTEXTURE_DESC *dstDesc, RECT *srcRect, RECT *dstRect) {
	return mHackerDevice->GetD3D9Device()->GetRenderTargetData(src, dst);
}

HRESULT GetRenderTargetData(D3D9Wrapper::IDirect3DDevice9 *mHackerDevice, IDirect3DSurface9 *src, IDirect3DTexture9 *dst, D3D2DTEXTURE_DESC *srcDesc, D3D2DTEXTURE_DESC *dstDesc, RECT *srcRect, RECT *dstRect) {
	HRESULT hr;
	IDirect3DSurface9 *dstSur = NULL;
	GetLevel(dst, 0, &dstSur);
	hr = mHackerDevice->GetD3D9Device()->GetRenderTargetData(src, dstSur);
	dstSur->Release();
	return hr;
}
HRESULT GetRenderTargetData(D3D9Wrapper::IDirect3DDevice9 *mHackerDevice, IDirect3DSurface9 *src, IDirect3DCubeTexture9 *dst, D3D2DTEXTURE_DESC *srcDesc, D3D2DTEXTURE_DESC *dstDesc, RECT *srcRect, RECT *dstRect) {
	HRESULT hr;
	IDirect3DSurface9 *dstSur = NULL;
	GetLevel(dst, 0, 0, &dstSur);
	hr = mHackerDevice->GetD3D9Device()->GetRenderTargetData(src, dstSur);
	dstSur->Release();
	return hr;
}
HRESULT GetRenderTargetData(D3D9Wrapper::IDirect3DDevice9 *mHackerDevice,  IDirect3DTexture9 *src, IDirect3DSurface9 *dst, D3D2DTEXTURE_DESC *srcDesc, D3D2DTEXTURE_DESC *dstDesc, RECT *srcRect, RECT *dstRect) {
	HRESULT hr;
	IDirect3DSurface9 *srcSur = NULL;
	GetLevel(src, 0, &srcSur);
	hr = mHackerDevice->GetD3D9Device()->GetRenderTargetData(srcSur, dst);
	srcSur->Release();
	return hr;
}
HRESULT GetRenderTargetData(D3D9Wrapper::IDirect3DDevice9 *mHackerDevice, IDirect3DCubeTexture9 *src, IDirect3DSurface9 *dst, D3D2DTEXTURE_DESC *srcDesc, D3D2DTEXTURE_DESC *dstDesc, RECT *srcRect, RECT *dstRect) {
	HRESULT hr;
	IDirect3DSurface9 *srcSur = NULL;
	GetLevel(src, 0, 0, &srcSur);
	hr = mHackerDevice->GetD3D9Device()->GetRenderTargetData(srcSur, dst);
	srcSur->Release();
	return hr;
}

HRESULT GetRenderTargetData(D3D9Wrapper::IDirect3DDevice9 *mHackerDevice,  IDirect3DCubeTexture9 *src, IDirect3DCubeTexture9 *dst, D3D2DTEXTURE_DESC *srcDesc, D3D2DTEXTURE_DESC *dstDesc, RECT *srcRect, RECT *dstRect ) {
	return CopyAllLevelsOfTexture<IDirect3DCubeTexture9, IDirect3DSurface9, D3D2DTEXTURE_DESC, RECT, CopyLevelSurface<GetRenderTargetData>>(mHackerDevice,  src, dst, srcDesc, dstDesc, srcRect, dstRect );
}
HRESULT GetRenderTargetData(D3D9Wrapper::IDirect3DDevice9 *mHackerDevice, IDirect3DTexture9 *src, IDirect3DTexture9 *dst, D3D2DTEXTURE_DESC *srcDesc, D3D2DTEXTURE_DESC *dstDesc, RECT *srcRect, RECT *dstRect) {
	return CopyAllLevelsOfTexture<IDirect3DTexture9, IDirect3DSurface9, D3D2DTEXTURE_DESC, RECT, CopyLevelSurface<GetRenderTargetData>>(mHackerDevice, src, dst, srcDesc, dstDesc, srcRect, dstRect);
}
HRESULT StretchRect(D3D9Wrapper::IDirect3DDevice9 *mHackerDevice, IDirect3DSurface9 *src, IDirect3DSurface9 *dst, D3D2DTEXTURE_DESC *srcDesc, D3D2DTEXTURE_DESC *dstDesc,  RECT *srcRect, RECT *dstRect) {
	return mHackerDevice->GetD3D9Device()->StretchRect(src, srcRect, dst, dstRect, D3DTEXF_POINT);
}

HRESULT StretchRect(D3D9Wrapper::IDirect3DDevice9 *mHackerDevice, IDirect3DSurface9 *src, IDirect3DTexture9 *dst, D3D2DTEXTURE_DESC *srcDesc, D3D2DTEXTURE_DESC *dstDesc, RECT *srcRect, RECT *dstRect) {
	HRESULT hr;
	IDirect3DSurface9 *dstSur = NULL;
	GetLevel(dst, 0, &dstSur);
	hr = mHackerDevice->GetD3D9Device()->StretchRect(src, srcRect, dstSur, dstRect, D3DTEXF_POINT);
	dstSur->Release();
	return hr;
}

HRESULT StretchRect(D3D9Wrapper::IDirect3DDevice9 *mHackerDevice,IDirect3DSurface9 *src, IDirect3DCubeTexture9 *dst, D3D2DTEXTURE_DESC *srcDesc, D3D2DTEXTURE_DESC *dstDesc, RECT *srcRect, RECT *dstRect) {
	HRESULT hr;
	IDirect3DSurface9 *dstSur = NULL;
	GetLevel(dst, 0, 0, &dstSur);
	hr = mHackerDevice->GetD3D9Device()->StretchRect(src, srcRect, dstSur, dstRect, D3DTEXF_POINT);
	dstSur->Release();
	return hr;
}
HRESULT StretchRect(D3D9Wrapper::IDirect3DDevice9 *mHackerDevice, IDirect3DTexture9 *src, IDirect3DSurface9 *dst, D3D2DTEXTURE_DESC *srcDesc, D3D2DTEXTURE_DESC *dstDesc, RECT *srcRect, RECT *dstRect) {
	HRESULT hr;
	IDirect3DSurface9 *srcSur = NULL;
	GetLevel(src, 0, &srcSur);
	hr = mHackerDevice->GetD3D9Device()->StretchRect(srcSur, srcRect, dst, dstRect, D3DTEXF_POINT);
	srcSur->Release();
	return hr;
}

HRESULT StretchRect(D3D9Wrapper::IDirect3DDevice9 *mHackerDevice, IDirect3DCubeTexture9 *src, IDirect3DSurface9 *dst, D3D2DTEXTURE_DESC *srcDesc, D3D2DTEXTURE_DESC *dstDesc,  RECT *srcRect, RECT *dstRect) {
	HRESULT hr;
	IDirect3DSurface9 *srcSur = NULL;
	GetLevel(src, 0, 0, &srcSur);
	hr = mHackerDevice->GetD3D9Device()->StretchRect(srcSur, srcRect, dst, dstRect, D3DTEXF_POINT);
	srcSur->Release();
	return hr;
}

template <typename Texture>
HRESULT StretchRect(D3D9Wrapper::IDirect3DDevice9 *mHackerDevice, Texture *src, Texture *dst, D3D2DTEXTURE_DESC *srcDesc, D3D2DTEXTURE_DESC *dstDesc,  RECT *srcRect, RECT *dstRect) {
	return CopyAllLevelsOfTexture<Texture, IDirect3DSurface9, D3D2DTEXTURE_DESC, RECT, CopyLevelSurface<StretchRect>>(mHackerDevice,  src, dst, srcDesc,dstDesc,  srcRect, dstRect);
}

HRESULT UpdateSurface(D3D9Wrapper::IDirect3DDevice9 *mHackerDevice, IDirect3DSurface9 *src, IDirect3DSurface9 *dst, D3D2DTEXTURE_DESC *srcDesc, D3D2DTEXTURE_DESC *dstDesc, RECT *srcRect = NULL, RECT *dstRect = NULL) {
	if (dstRect != NULL) {
		POINT dPoint = { dstRect->left, dstRect->top };
		return mHackerDevice->GetD3D9Device()->UpdateSurface(src, srcRect, dst, &dPoint);
	}
	else {
		return mHackerDevice->GetD3D9Device()->UpdateSurface(src, srcRect, dst, NULL);
	}
}
HRESULT UpdateSurface(D3D9Wrapper::IDirect3DDevice9 *mHackerDevice, IDirect3DSurface9 *src, IDirect3DTexture9 *dst, D3D2DTEXTURE_DESC *srcDesc, D3D2DTEXTURE_DESC *dstDesc, RECT *srcRect = NULL, RECT *dstRect = NULL) {
	IDirect3DSurface9 *dstSur = NULL;
	GetLevel(dst, 0, &dstSur);
	if (dstRect != NULL) {
		POINT dPoint = { dstRect->left, dstRect->top };
		return mHackerDevice->GetD3D9Device()->UpdateSurface(src, srcRect, dstSur, &dPoint);
	}
	else {
		return mHackerDevice->GetD3D9Device()->UpdateSurface(src, srcRect, dstSur, NULL);
	}
}
HRESULT UpdateSurface(D3D9Wrapper::IDirect3DDevice9 *mHackerDevice, IDirect3DSurface9 *src, IDirect3DCubeTexture9 *dst, D3D2DTEXTURE_DESC *srcDesc, D3D2DTEXTURE_DESC *dstDesc, RECT *srcRect = NULL, RECT *dstRect = NULL) {
	IDirect3DSurface9 *dstSur = NULL;
	GetLevel(dst, 0, 0, &dstSur);
	if (dstRect != NULL) {
		POINT dPoint = { dstRect->left, dstRect->top };
		return mHackerDevice->GetD3D9Device()->UpdateSurface(src, srcRect, dstSur, &dPoint);
	}
	else {
		return mHackerDevice->GetD3D9Device()->UpdateSurface(src, srcRect, dstSur, NULL);
	}
}
HRESULT UpdateSurface(D3D9Wrapper::IDirect3DDevice9 *mHackerDevice, IDirect3DTexture9 *src, IDirect3DSurface9 *dst, D3D2DTEXTURE_DESC *srcDesc, D3D2DTEXTURE_DESC *dstDesc, RECT *srcRect = NULL, RECT *dstRect = NULL) {
	IDirect3DSurface9 *srcSur = NULL;
	GetLevel(src, 0, &srcSur);
	if (dstRect != NULL) {
		POINT dPoint = { dstRect->left, dstRect->top };
		return mHackerDevice->GetD3D9Device()->UpdateSurface(srcSur, srcRect, dst, &dPoint);
	}
	else {
		return mHackerDevice->GetD3D9Device()->UpdateSurface(srcSur, srcRect, dst, NULL);
	}
}

HRESULT UpdateSurface(D3D9Wrapper::IDirect3DDevice9 *mHackerDevice, IDirect3DCubeTexture9 *src, IDirect3DSurface9 *dst, D3D2DTEXTURE_DESC *srcDesc, D3D2DTEXTURE_DESC *dstDesc, RECT *srcRect = NULL, RECT *dstRect = NULL) {
	IDirect3DSurface9 *srcSur = NULL;
	GetLevel(src, 0, 0, &srcSur);
	if (dstRect != NULL) {
		POINT dPoint = { dstRect->left, dstRect->top };
		return mHackerDevice->GetD3D9Device()->UpdateSurface(srcSur, srcRect, dst, &dPoint);
	}
	else {
		return mHackerDevice->GetD3D9Device()->UpdateSurface(srcSur, srcRect, dst, NULL);
	}
}

HRESULT UpdateSurface(D3D9Wrapper::IDirect3DDevice9 *mHackerDevice, IDirect3DCubeTexture9 *src, IDirect3DCubeTexture9 *dst, D3D2DTEXTURE_DESC *srcDesc, D3D2DTEXTURE_DESC *dstDesc, RECT *srcRect = NULL, RECT *dstRect = NULL) {
	if (srcRect != NULL)
		AddDirtyRect(src, srcRect);
	if (dstRect != NULL)
		AddDirtyRect(dst, dstRect);
	return mHackerDevice->GetD3D9Device()->UpdateTexture(src, dst);
}

HRESULT UpdateSurface(D3D9Wrapper::IDirect3DDevice9 *mHackerDevice, IDirect3DTexture9 *src, IDirect3DTexture9 *dst, D3D2DTEXTURE_DESC *srcDesc, D3D2DTEXTURE_DESC *dstDesc, RECT *srcRect = NULL, RECT *dstRect = NULL) {
	if (srcRect != NULL)
		AddDirtyRect(src, srcRect);
	if (dstRect != NULL)
		AddDirtyRect(dst, dstRect);
	return mHackerDevice->GetD3D9Device()->UpdateTexture(src, dst);
}
template <typename SrcSurface, typename DstSurface>
static HRESULT _CopyResource(CommandListState *state, SrcSurface *srcSurface, DstSurface *dstSurface, RECT *srcRect = NULL, RECT *dstRect = NULL) {
	D3D2DTEXTURE_DESC srcDesc;
	GetResourceDesc(srcSurface, &srcDesc);
	D3D2DTEXTURE_DESC dstDesc;
	GetResourceDesc(dstSurface, &dstDesc);
	HRESULT hr;
	if (dstDesc.Usage & D3DUSAGE_DEPTHSTENCIL || srcDesc.Usage & D3DUSAGE_DEPTHSTENCIL) {
			if ((dstDesc.Usage & D3DUSAGE_DEPTHSTENCIL && srcDesc.Usage & D3DUSAGE_DEPTHSTENCIL) && CanDirectXStretchRectDS(state->mOrigDevice, &srcDesc, &dstDesc, srcRect, dstRect))
				hr = StretchRect(state->mHackerDevice, srcSurface, dstSurface, &srcDesc, &dstDesc, srcRect, dstRect);
			else
				hr = D3DXSurfaceToSurface(state->mHackerDevice, srcSurface, dstSurface, &srcDesc, &dstDesc, srcRect, dstRect);
			if (FAILED(hr))
				hr = state->mHackerDevice->NVAPIStretchRect(srcSurface, dstSurface, srcRect, dstRect);
			return hr;
	}
	else {
		if (srcDesc.Pool == D3DPOOL::D3DPOOL_SYSTEMMEM) {
			if (dstDesc.Pool == D3DPOOL_SYSTEMMEM || dstDesc.MultiSampleType != D3DMULTISAMPLE_NONE)
				return D3DXSurfaceToSurface(state->mHackerDevice, srcSurface, dstSurface, &srcDesc, &dstDesc, srcRect, dstRect);
			else
				return UpdateSurface(state->mHackerDevice, srcSurface, dstSurface, &srcDesc, &dstDesc, srcRect, dstRect);
		}
		else {
			if (dstDesc.Pool == D3DPOOL_SYSTEMMEM) {
				if (CanGetRenderTarget(state->mOrigDevice, &srcDesc, &dstDesc)) {
					if (srcDesc.MultiSampleType == D3DMULTISAMPLE_NONE) {
						return GetRenderTargetData(state->mHackerDevice, srcSurface, dstSurface, &srcDesc, &dstDesc, srcRect, dstRect);
					}
					else {
						COMMAND_LIST_LOG(state, "  resolving MSAA\n");
						SrcSurface *intermediateSur = NULL;
						hr = IntermediateResolveMSAA(state->mHackerDevice, srcSurface, intermediateSur, &srcDesc, &dstDesc, srcRect, dstRect);
						if (FAILED(hr))
							return hr;
						return GetRenderTargetData(state->mHackerDevice, intermediateSur, dstSurface, &srcDesc, &dstDesc, srcRect, dstRect);
					}
				}
				else {
					return D3DXSurfaceToSurface(state->mHackerDevice, srcSurface, dstSurface, &srcDesc, &dstDesc, srcRect, dstRect);
				}
			}
			else {
				if (CanDirectXStretchRectRT(state->mOrigDevice, &srcDesc, &dstDesc)) {
					return StretchRect(state->mHackerDevice, srcSurface, dstSurface, &srcDesc, &dstDesc, srcRect, dstRect);
				}
				else {
					hr = D3DXSurfaceToSurface(state->mHackerDevice, srcSurface, dstSurface, &srcDesc, &dstDesc, srcRect, dstRect);
					if (FAILED(hr))
						hr = state->mHackerDevice->NVAPIStretchRect(srcSurface, dstSurface, srcRect, dstRect);
					return hr;
				}
			}
		}
	}
}

static HRESULT CopyResource(CommandListState *state,IDirect3DSurface9 *srcResource, IDirect3DSurface9 *dstResource, RECT *srcRect = NULL, RECT *dstRect = NULL){
	return	_CopyResource<IDirect3DSurface9, IDirect3DSurface9>(state, srcResource, dstResource,  srcRect, dstRect);
}
static HRESULT CopyResource(CommandListState *state, IDirect3DVolumeTexture9 *srcResource, IDirect3DVolumeTexture9 *dstResource, D3DBOX *srcRect = NULL, D3DBOX *dstRect = NULL) {
	D3D3DTEXTURE_DESC src_desc;
	GetResourceDesc(srcResource, &src_desc);
	D3D3DTEXTURE_DESC dst_desc;
	GetResourceDesc(dstResource, &dst_desc);
	if (!(src_desc.Pool == D3DPOOL::D3DPOOL_SYSTEMMEM) || !(dst_desc.Pool == D3DPOOL::D3DPOOL_DEFAULT)) {
		return CopyAllLevelsOfTexture<IDirect3DVolumeTexture9, IDirect3DVolume9, D3D3DTEXTURE_DESC, D3DBOX, CopyLevelVolume<D3DXVolumeToVolume>>(state->mHackerDevice, srcResource, dstResource, &src_desc, &dst_desc, srcRect, dstRect);
	}
	else {
		if (srcRect != NULL)
			AddDirtyRect(srcResource, srcRect);
		if (dstRect != NULL)
			AddDirtyRect(dstResource, dstRect);
		return state->mOrigDevice->UpdateTexture(srcResource, dstResource);
	}
}
static HRESULT CopyResource(CommandListState *state, IDirect3DCubeTexture9 *srcResource, IDirect3DCubeTexture9 *dstResource, RECT *srcRect = NULL, RECT *dstRect = NULL) {
	return _CopyResource<IDirect3DCubeTexture9, IDirect3DCubeTexture9>(state, srcResource, dstResource, srcRect, dstRect);
}
static HRESULT CopyResource(CommandListState *state, IDirect3DTexture9 *srcResource, IDirect3DTexture9 *dstResource, RECT *srcRect = NULL, RECT *dstRect = NULL) {

	return _CopyResource<IDirect3DTexture9, IDirect3DTexture9>(state, srcResource, dstResource, srcRect, dstRect);
}
static HRESULT CopyResource(CommandListState *state,  IDirect3DSurface9 *srcResource, IDirect3DCubeTexture9 *dstResource, RECT *srcRect = NULL, RECT *dstRect = NULL) {
	return _CopyResource<IDirect3DSurface9, IDirect3DCubeTexture9>(state, srcResource, dstResource, srcRect, dstRect);
}
static HRESULT CopyResource(CommandListState *state,  IDirect3DSurface9 *srcResource, IDirect3DTexture9 *dstResource, RECT *srcRect = NULL, RECT *dstRect = NULL) {
	return _CopyResource<IDirect3DSurface9, IDirect3DTexture9>(state,  srcResource, dstResource, srcRect, dstRect);
}
static HRESULT CopyResource(CommandListState *state, IDirect3DCubeTexture9 *srcResource, IDirect3DSurface9 *dstResource, RECT *srcRect = NULL, RECT *dstRect = NULL) {
	return _CopyResource<IDirect3DCubeTexture9, IDirect3DSurface9>(state,  srcResource, dstResource, srcRect, dstRect);
}
static HRESULT CopyResource(CommandListState *state, IDirect3DTexture9 *srcResource, IDirect3DSurface9 *dstResource, RECT *srcRect = NULL, RECT *dstRect = NULL) {
	return _CopyResource<IDirect3DTexture9, IDirect3DSurface9>(state, srcResource, dstResource, srcRect, dstRect);
}
template<typename ID3D9SourceBuffer, typename ID3D9DestBuffer>
static HRESULT _CopyResource(CommandListState *state, ID3D9SourceBuffer *src, ID3D9DestBuffer *dst, UINT *offset, UINT size) {
	HRESULT hr;
	VOID* pVoidSrc;
	VOID* pVoidDst;
	hr = src->Lock(*offset, size, (void**)&pVoidSrc, D3DLOCK_READONLY);
	if FAILED(hr)
		return hr;
	hr = dst->Lock(*offset, size, (void**)&pVoidDst, 0);
	if FAILED(hr)
		return hr;
	memcpy(pVoidDst, pVoidSrc, sizeof(pVoidSrc));
	hr = src->Unlock();
	if FAILED(hr)
		return hr;
	hr = dst->Unlock();
	if FAILED(hr)
		return hr;
	return hr;
}
static HRESULT CopyResource(CommandListState *state, IDirect3DVertexBuffer9 *src, IDirect3DVertexBuffer9 *dst) {
	D3DVERTEXBUFFER_DESC vDesc;
	src->GetDesc(&vDesc);
	return _CopyResource<IDirect3DVertexBuffer9, IDirect3DVertexBuffer9>(state, src, dst, 0, vDesc.Size);
}
static HRESULT CopyResource(CommandListState *state, IDirect3DIndexBuffer9 *src, IDirect3DIndexBuffer9 *dst) {
	D3DINDEXBUFFER_DESC iDesc;
	src->GetDesc(&iDesc);
	return _CopyResource<IDirect3DIndexBuffer9, IDirect3DIndexBuffer9>(state, src, dst, 0, iDesc.Size);
}
static HRESULT CopyResource(CommandListState *state,  IDirect3DResource9 *srcResource, IDirect3DResource9 *dstResource, RECT *srcRect = NULL, RECT *dstRect = NULL) {
	D3DRESOURCETYPE srcType;
	D3DRESOURCETYPE dstType;
	srcType = srcResource->GetType();
	dstType = dstResource->GetType();
	switch (srcType){
		case D3DRTYPE_SURFACE:
			switch (dstType) {
			case D3DRTYPE_SURFACE:
				return CopyResource(state, (IDirect3DSurface9*)srcResource, (IDirect3DSurface9*)dstResource, srcRect, dstRect);
			case D3DRTYPE_TEXTURE:
				return CopyResource(state, (IDirect3DSurface9*)srcResource, (IDirect3DTexture9*)dstResource, srcRect, dstRect);
			case D3DRTYPE_CUBETEXTURE:
				return CopyResource(state, (IDirect3DSurface9*)srcResource, (IDirect3DCubeTexture9*)dstResource, srcRect, dstRect);
			default:
				return E_NOTIMPL;
			}
			break;
		case D3DRTYPE_TEXTURE:
			switch (dstType) {
			case D3DRTYPE_SURFACE:
				return CopyResource(state, (IDirect3DTexture9*)srcResource, (IDirect3DSurface9*)dstResource, srcRect, dstRect);
			case D3DRTYPE_TEXTURE:
				return CopyResource(state, (IDirect3DTexture9*)srcResource, (IDirect3DTexture9*)dstResource, srcRect, dstRect);
			default:
				return E_NOTIMPL;
			}
			break;
		case D3DRTYPE_CUBETEXTURE:
			switch (dstType) {
			case D3DRTYPE_SURFACE:
				return CopyResource(state, (IDirect3DCubeTexture9*)srcResource, (IDirect3DSurface9*)dstResource, srcRect, dstRect);
			case D3DRTYPE_CUBETEXTURE:
				return CopyResource(state, (IDirect3DCubeTexture9*)srcResource, (IDirect3DCubeTexture9*)dstResource, srcRect, dstRect);
			default:
				return E_NOTIMPL;
			}
			break;
		case D3DRTYPE_VOLUMETEXTURE:
			if (dstType == D3DRTYPE_VOLUMETEXTURE)
				return CopyResource(state, (IDirect3DVolumeTexture9*)srcResource, (IDirect3DVolumeTexture9*)dstResource, srcRect, dstRect);
			return E_NOTIMPL;
		case D3DRTYPE_VERTEXBUFFER:
			if (dstType == D3DRTYPE_VERTEXBUFFER)
				return CopyResource(state, (IDirect3DVertexBuffer9*)srcResource, (IDirect3DVertexBuffer9*)dstResource);
			return E_NOTIMPL;
		case D3DRTYPE_INDEXBUFFER:
			if (dstType == D3DRTYPE_INDEXBUFFER)
				return CopyResource(state, (IDirect3DIndexBuffer9*)srcResource, (IDirect3DIndexBuffer9*)dstResource);
			return E_NOTIMPL;
		default:
			return E_NOTIMPL;
	}
}
static void ReverseStereoBlits(vector<IDirect3DTexture9*>dst_resources, vector<IDirect3DTexture9*>src_resources, CommandListState *state, LONG fallback, size_t numberOfBlits)
{
	NvAPI_Status nvret;
	D3DSURFACE_DESC srcDesc;
	UINT width, height;
	RECT srcRect;
	RECT dstRect;
	LONG fallbackside = 0;
	if (!fallback && !G->stereoblit_control_set_once){
		nvret = Profiling::NvAPI_Stereo_ReverseStereoBlitControl(state->mHackerDevice->mStereoHandle, true);
		if (nvret != NVAPI_OK) {
			LogInfo("Resource copying failed to enable reverse stereo blit\n");
			// Fallback path: Copy 2D resource to both sides of the 2x
			// width destination
			fallback = 1;
		}
	}
	for (size_t x = 0; x < numberOfBlits; x++) {
		IDirect3DTexture9 *src_texture;
		IDirect3DTexture9 *dst_texture;
		src_texture = src_resources.at(x);
		dst_texture = dst_resources.at(x);
		IDirect3DSurface9 *src_surface;
		IDirect3DSurface9 *dst_surface;
		src_texture->GetSurfaceLevel(0, &src_surface);
		dst_texture->GetSurfaceLevel(0, &dst_surface);
		src_surface->GetDesc(&srcDesc);
		for (fallbackside = 0; fallbackside < 1 + fallback; fallbackside++) {
			if (fallback) {
				srcRect.left = 0;
				srcRect.top = 0;
				srcRect.right = width = srcDesc.Width;
				srcRect.bottom = height = srcDesc.Height;
				LONG upperX = fallbackside * srcRect.right;
				dstRect.left = upperX;
				dstRect.top = 0;
				dstRect.right = upperX + width;
				dstRect.bottom = height;
			}
			else {
				srcRect.left = 0;
				srcRect.top = 0;
				srcRect.right = width = srcDesc.Width;
				srcRect.bottom = height = srcDesc.Height;
				dstRect.left = 0;
				dstRect.top = 0;
				dstRect.right = width * 2;
				dstRect.bottom = height;
			}
			state->mOrigDevice->StretchRect(src_surface, &srcRect, dst_surface, &dstRect, D3DTEXF_POINT);
		}

		src_surface->Release();
		dst_surface->Release();
	}
	if (!fallback && !G->stereoblit_control_set_once)
		Profiling::NvAPI_Stereo_ReverseStereoBlitControl(state->mHackerDevice->mStereoHandle, false);
}
static void DirectModeReverseStereoBlitSurfaceToSurface(IDirect3DSurface9 *left_surface, IDirect3DSurface9 *right_surface, IDirect3DSurface9 *dst_surface, CommandListState *state) {
	if (state->mHackerDevice->sli_enabled()) {
		IDirect3DSurface9 *origRT;
		state->mOrigDevice->GetRenderTarget(0, &origRT);
		state->mOrigDevice->SetRenderTarget(0, dst_surface);
		state->mOrigDevice->Clear(0L, NULL, D3DCLEAR_TARGET, 0x00000000, 1.0f, 0L);
		state->mOrigDevice->SetRenderTarget(0, origRT);
	}
	D3DSURFACE_DESC srcDesc;
	left_surface->GetDesc(&srcDesc);
	for (LONG side = 0; side < 2; side++) {
		RECT dstRect;
		LONG upperX = side * srcDesc.Width;
		dstRect.left = upperX;
		dstRect.top = 0;
		dstRect.right = upperX + srcDesc.Width;
		dstRect.bottom = srcDesc.Height;
		if (side == 0)
			state->mOrigDevice->StretchRect(left_surface, NULL, dst_surface, &dstRect, D3DTEXF_POINT);
		else
			state->mOrigDevice->StretchRect(right_surface, NULL, dst_surface, &dstRect, D3DTEXF_POINT);
	}
}


static void DirectModeReverseStereoBlit(IDirect3DResource9 *src_resource, IDirect3DResource9 *dst_resource, CommandListState *state, bool srcIsWrapped, bool dstIsWrapped) {
	D3DRESOURCETYPE src_type = src_resource->GetType();
	D3DRESOURCETYPE dst_type = dst_resource->GetType();
	IDirect3DSurface9 *dst_surface = NULL;
	IDirect3DSurface9 *left_surface = NULL;
	IDirect3DSurface9 *right_surface = NULL;
	switch (src_type) {
	case D3DRTYPE_SURFACE:
		if (srcIsWrapped) {
			D3D9Wrapper::IDirect3DSurface9 *src_surface_wrapper = reinterpret_cast<D3D9Wrapper::IDirect3DSurface9*>(src_resource);
			left_surface = src_surface_wrapper->DirectModeGetLeft();
			right_surface = src_surface_wrapper->DirectModeGetRight();
		}
		else {
			IDirect3DSurface9 *src_surface = reinterpret_cast<IDirect3DSurface9*>(src_resource);
			left_surface = src_surface;
			right_surface = src_surface;
		}
		break;
	case D3DRTYPE_TEXTURE:
		if (srcIsWrapped) {
			D3D9Wrapper::IDirect3DTexture9 *src_texture_wrapper = reinterpret_cast<D3D9Wrapper::IDirect3DTexture9*>(src_resource);
			src_texture_wrapper->DirectModeGetLeft()->GetSurfaceLevel(0, &left_surface);
			src_texture_wrapper->DirectModeGetRight()->GetSurfaceLevel(0, &right_surface);
		}
		else {
			IDirect3DTexture9 *src_texture = reinterpret_cast<IDirect3DTexture9*>(src_resource);
			src_texture->GetSurfaceLevel(0, &left_surface);
			src_texture->GetSurfaceLevel(0, &right_surface);
		}
		left_surface->Release();
		right_surface->Release();
		break;
	case D3DRTYPE_CUBETEXTURE:
		if (srcIsWrapped) {
			D3D9Wrapper::IDirect3DCubeTexture9 *src_cubetexture_wrapper = reinterpret_cast<D3D9Wrapper::IDirect3DCubeTexture9*>(src_resource);
			src_cubetexture_wrapper->DirectModeGetLeft()->GetCubeMapSurface(D3DCUBEMAP_FACES::D3DCUBEMAP_FACE_POSITIVE_X, 0, &left_surface);
			src_cubetexture_wrapper->DirectModeGetRight()->GetCubeMapSurface(D3DCUBEMAP_FACES::D3DCUBEMAP_FACE_POSITIVE_X, 0, &right_surface);
		}
		else {
			IDirect3DCubeTexture9 *src_texture = reinterpret_cast<IDirect3DCubeTexture9*>(src_resource);
			src_texture->GetCubeMapSurface(D3DCUBEMAP_FACES::D3DCUBEMAP_FACE_POSITIVE_X, 0, &left_surface);
			src_texture->GetCubeMapSurface(D3DCUBEMAP_FACES::D3DCUBEMAP_FACE_POSITIVE_X, 0, &right_surface);
		}
		left_surface->Release();
		right_surface->Release();
		break;
	}
	switch (dst_type) {
	case D3DRTYPE_SURFACE:
		if (dstIsWrapped) {
			D3D9Wrapper::IDirect3DSurface9 *dst_surface_wrapper = reinterpret_cast<D3D9Wrapper::IDirect3DSurface9*>(dst_resource);
			dst_surface = dst_surface_wrapper->DirectModeGetLeft();
		}
		else {
			dst_surface = reinterpret_cast<IDirect3DSurface9*>(dst_resource);
		}
		break;
	case D3DRTYPE_TEXTURE:
		if (dstIsWrapped) {
			D3D9Wrapper::IDirect3DTexture9 *dst_texture_wrapper = reinterpret_cast<D3D9Wrapper::IDirect3DTexture9*>(dst_resource);
			dst_texture_wrapper->DirectModeGetLeft()->GetSurfaceLevel(0, &dst_surface);
		}
		else {
			IDirect3DTexture9 *dst_texture = reinterpret_cast<IDirect3DTexture9*>(dst_resource);
			dst_texture->GetSurfaceLevel(0, &dst_surface);
		}
		dst_surface->Release();
		break;
	case D3DRTYPE_CUBETEXTURE:
		if (dstIsWrapped) {
			D3D9Wrapper::IDirect3DCubeTexture9 *dst_cubetexture_wrapper = reinterpret_cast<D3D9Wrapper::IDirect3DCubeTexture9*>(dst_resource);
			dst_cubetexture_wrapper->DirectModeGetLeft()->GetCubeMapSurface(D3DCUBEMAP_FACES::D3DCUBEMAP_FACE_POSITIVE_X, 0, &dst_surface);
		}
		else {
			IDirect3DCubeTexture9 *dst_cubetexture = reinterpret_cast<IDirect3DCubeTexture9*>(dst_resource);
			dst_cubetexture->GetCubeMapSurface(D3DCUBEMAP_FACES::D3DCUBEMAP_FACE_POSITIVE_X, 0, &dst_surface);
		}
		dst_surface->Release();
		break;
	}
	DirectModeReverseStereoBlitSurfaceToSurface(left_surface, right_surface, dst_surface, state);
}
static void ReverseStereoBlit(IDirect3DSurface9 *dst_resource, IDirect3DSurface9 *src_resource, CommandListState *state, LONG fallback)
{

	LONG fallbackside = 0;
	if (!fallback && !G->stereoblit_control_set_once) {
		NvAPI_Status nvret = Profiling::NvAPI_Stereo_ReverseStereoBlitControl(state->mHackerDevice->mStereoHandle, true);
		if (nvret != NVAPI_OK) {
			LogInfo("Resource copying failed to enable reverse stereo blit\n");
			// Fallback path: Copy 2D resource to both sides of the 2x
			// width destination
			fallback = 1;
		}
	}
	IDirect3DSurface9 *origRT;
	state->mOrigDevice->GetRenderTarget(0, &origRT);
	state->mOrigDevice->SetRenderTarget(0, dst_resource);
	state->mOrigDevice->Clear(0L, NULL, D3DCLEAR_TARGET, 0x00000000, 1.0f, 0L);
	state->mOrigDevice->SetRenderTarget(0, origRT);
	origRT->Release();
	for (fallbackside = 0; fallbackside < 1 + fallback; fallbackside++) {
		if (fallback) {
			D3DSURFACE_DESC srcDesc;
			src_resource->GetDesc(&srcDesc);
			RECT dstRect;
			LONG upperX = fallbackside * srcDesc.Width;
			dstRect.left = upperX;
			dstRect.top = 0;
			dstRect.right = upperX + srcDesc.Width;
			dstRect.bottom = srcDesc.Height;
			state->mOrigDevice->StretchRect(src_resource, NULL, dst_resource, &dstRect, D3DTEXF_POINT);
		}else{
			state->mOrigDevice->StretchRect(src_resource, NULL, dst_resource, NULL, D3DTEXF_POINT);
		}
	}
	if (!fallback && !G->stereoblit_control_set_once)
		Profiling::NvAPI_Stereo_ReverseStereoBlitControl(state->mHackerDevice->mStereoHandle, false);
}
template<typename ID3D9SourceBuffer, typename ID3D9DestBuffer>
static HRESULT SpecialCopyBufferRegion(ID3D9DestBuffer *dst_resource, ID3D9SourceBuffer *src_resource,
	CommandListState *state, UINT stride, UINT *offset,
	UINT buf_src_size, UINT buf_dst_size)
{
	HRESULT hr;
	UINT size = 0;
	UINT left = 0;
	UINT right = 0;

	// We want to copy from the offset to the end of the source buffer, but
	// cap it to the destination size to avoid "undefined behaviour". Keep
	// in mind that this is "right", not "size":
	left = *offset;
	right = min(buf_src_size, *offset + buf_dst_size);

	if (stride) {
		// If we are copying to a structured resource, the source box
		// must be a multiple of the stride, so round it down:
		right = (right - left) / stride * stride + left;
	}

	size = right - left;

	hr = _CopyResource<ID3D9SourceBuffer, ID3D9DestBuffer>(state, src_resource, dst_resource, offset, size);
	// We have effectively removed the offset during the region copy, so
	// set it to 0 to make sure nothing will try to use it again elsewhere:
	*offset = 0;
	return hr;
}
static HRESULT SpecialCopyBufferRegion(IDirect3DResource9* dst_resource, IDirect3DResource9 *src_resource,
	CommandListState *state, UINT stride, UINT *offset,
	UINT buf_src_size, UINT buf_dst_size) {
	D3DRESOURCETYPE src_type;
	D3DRESOURCETYPE dst_type;
	src_type = src_resource->GetType();
	dst_type = dst_resource->GetType();
	switch (src_type) {
	case D3DRTYPE_VERTEXBUFFER:
		switch (dst_type) {
		case D3DRTYPE_VERTEXBUFFER:
			return SpecialCopyBufferRegion<IDirect3DVertexBuffer9, IDirect3DVertexBuffer9>((IDirect3DVertexBuffer9*)dst_resource, (IDirect3DVertexBuffer9*)src_resource, state, stride, offset, buf_src_size, buf_dst_size);
		case D3DRTYPE_INDEXBUFFER:
			return SpecialCopyBufferRegion<IDirect3DVertexBuffer9, IDirect3DIndexBuffer9>((IDirect3DIndexBuffer9*)dst_resource, (IDirect3DVertexBuffer9*)src_resource, state, stride, offset, buf_src_size, buf_dst_size);
		}
		break;
	case D3DRTYPE_INDEXBUFFER:
		switch (dst_type) {
		case D3DRTYPE_VERTEXBUFFER:
			return SpecialCopyBufferRegion<IDirect3DIndexBuffer9, IDirect3DIndexBuffer9>((IDirect3DIndexBuffer9*)dst_resource, (IDirect3DIndexBuffer9*)src_resource, state, stride, offset, buf_src_size, buf_dst_size);
		case D3DRTYPE_INDEXBUFFER:
			return SpecialCopyBufferRegion<IDirect3DIndexBuffer9, IDirect3DVertexBuffer9>((IDirect3DVertexBuffer9*)dst_resource, (IDirect3DIndexBuffer9*)src_resource, state, stride, offset, buf_src_size, buf_dst_size);
		}
	}
	return E_NOTIMPL;
}
void ClearSurfaceCommand::clear_surface(IDirect3DResource9 *resource, CommandListState *state)
{
	IDirect3DSurface9 *surface = NULL;
	D3DRESOURCETYPE type;
	type = resource->GetType();
	switch (type) {
	case D3DRTYPE_SURFACE:
		surface = (IDirect3DSurface9*)resource;
		break;
	case D3DRTYPE_TEXTURE:
		IDirect3DTexture9 *tex;
		tex = (IDirect3DTexture9*)resource;
		tex->GetSurfaceLevel(0, &surface);
		break;
	case D3DRTYPE_CUBETEXTURE:
		IDirect3DCubeTexture9 *texCube;
		texCube = (IDirect3DCubeTexture9*)resource;
		texCube->GetCubeMapSurface(D3DCUBEMAP_FACES(0), 0, &surface);
		break;
	}
	D3DSURFACE_DESC desc;
	surface->GetDesc(&desc);
	if (desc.Usage & D3DUSAGE_RENDERTARGET) {
		COMMAND_LIST_LOG(state, "  clearing RTV\n");
		Profiling::surfaces_cleared++;
		IDirect3DSurface9 *origRT;
		state->mOrigDevice->GetRenderTarget(0, &origRT);
		state->mOrigDevice->SetRenderTarget(0, surface);
		state->mOrigDevice->Clear(0, NULL, D3DCLEAR_TARGET, D3DCOLOR_RGBA((int)fval[0], (int)fval[1], (int)fval[2], (int)fval[3]), NULL, NULL);
		state->mOrigDevice->SetRenderTarget(0, origRT);
		origRT->Release();
	}
	if (desc.Usage & D3DUSAGE_DEPTHSTENCIL) {
		COMMAND_LIST_LOG(state, "  clearing DSV\n");
		Profiling::surfaces_cleared++;
		DWORD flags = (DWORD)0;
		if (!clear_depth && !clear_stencil)
			flags = (D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL);
		else if (clear_depth)
			flags = D3DCLEAR_ZBUFFER;
		else if (clear_stencil)
			flags = D3DCLEAR_STENCIL;
		IDirect3DSurface9 *origDS;
		state->mOrigDevice->GetDepthStencilSurface(&origDS);
		state->mOrigDevice->SetDepthStencilSurface(surface);
		state->mOrigDevice->Clear(0, NULL, flags, NULL, dsv_depth, dsv_stencil);
		state->mOrigDevice->SetDepthStencilSurface(origDS);
		origDS->Release();
	}
	if (surface)
		surface->Release();
}

void ClearSurfaceCommand::run(CommandListState *state)
{
	IDirect3DResource9 *resource = NULL;
	UINT stride = 0;
	UINT offset = 0;
	D3DFORMAT format = D3DFMT_UNKNOWN;
	UINT buf_src_size = 0;

	COMMAND_LIST_LOG(state, "%S\n", ini_line.c_str());

	resource = target.GetResource(state, &stride, &offset, &format, &buf_src_size);
	if (!resource) {
		COMMAND_LIST_LOG(state, "  No resource to clear\n");
		return;
	}
	clear_surface(resource, state);

	if (resource)
		resource->Release();
}

void ResourceCopyOperation::DirectModeCopyResource(CommandListState *state, ::IDirect3DResource9 *src_resource, ::IDirect3DResource9 *dst_resource, bool direct_mode_wrapped_resource_source, bool direct_mode_wrapped_resource_dest) {
	if (!direct_mode_wrapped_resource_source && !direct_mode_wrapped_resource_dest) {
		HRESULT hr = CopyResource(state, src_resource, dst_resource, NULL, NULL);
		if (FAILED(hr))
			COMMAND_LIST_LOG(state, "  Direct Mode, non-wrapped resources, full copy failed with hr=%x\n", hr);
	}
	else {
		D3DRESOURCETYPE src_type;
		src_type = src_resource->GetType();
		::IDirect3DResource9* pSourceResourceLeft = NULL;
		::IDirect3DResource9* pSourceResourceRight = NULL;
		::IDirect3DResource9* pDestResourceLeft = NULL;
		::IDirect3DResource9* pDestResourceRight = NULL;
		D3DRESOURCETYPE dst_type;
		dst_type = dst_resource->GetType();
		D3D9Wrapper::IDirect3DSurface9 *src_sur = NULL;
		D3D9Wrapper::IDirect3DTexture9 *src_tex = NULL;
		D3D9Wrapper::IDirect3DCubeTexture9 *src_cube_tex = NULL;
		switch (src_type) {
		case D3DRTYPE_SURFACE:
			src_sur = reinterpret_cast<D3D9Wrapper::IDirect3DSurface9*>(src_resource);
			pSourceResourceLeft = src_sur->DirectModeGetLeft();
			pSourceResourceRight = src_sur->DirectModeGetRight();
			break;
		case D3DRTYPE_TEXTURE:
			if (direct_mode_wrapped_resource_source) {
				src_tex = reinterpret_cast<D3D9Wrapper::IDirect3DTexture9*>(src_resource);
				pSourceResourceLeft = src_tex->DirectModeGetLeft();
				pSourceResourceRight = src_tex->DirectModeGetRight();
			}
			else {
				pSourceResourceLeft = reinterpret_cast<IDirect3DTexture9*>(src_resource);
			}
			break;
		case D3DRTYPE_CUBETEXTURE:
			if (direct_mode_wrapped_resource_source) {
				src_cube_tex = reinterpret_cast<D3D9Wrapper::IDirect3DCubeTexture9*>(src_resource);
				pSourceResourceLeft = src_cube_tex->DirectModeGetLeft();
				pSourceResourceRight = src_cube_tex->DirectModeGetRight();
			}
			else {
				pSourceResourceLeft = reinterpret_cast<IDirect3DCubeTexture9*>(src_resource);
			}
			break;
		}
		D3D9Wrapper::IDirect3DSurface9 *dst_sur = NULL;
		D3D9Wrapper::IDirect3DTexture9 *dst_tex = NULL;
		D3D9Wrapper::IDirect3DCubeTexture9 *dst_cube_tex = NULL;
		switch (dst_type) {
		case D3DRTYPE_SURFACE:
			dst_sur = reinterpret_cast<D3D9Wrapper::IDirect3DSurface9*>(dst_resource);
			pDestResourceLeft = dst_sur->DirectModeGetLeft();
			pDestResourceRight = dst_sur->DirectModeGetRight();
			break;
		case D3DRTYPE_TEXTURE:
			if (direct_mode_wrapped_resource_dest){
				dst_tex = reinterpret_cast<D3D9Wrapper::IDirect3DTexture9*>(dst_resource);
				pDestResourceLeft = dst_tex->DirectModeGetLeft();
				pDestResourceRight = dst_tex->DirectModeGetRight();
			}
			else {
				pDestResourceLeft = reinterpret_cast<IDirect3DTexture9*>(dst_resource);
			}

			break;
		case D3DRTYPE_CUBETEXTURE:
			if (direct_mode_wrapped_resource_dest) {
				dst_cube_tex = reinterpret_cast<D3D9Wrapper::IDirect3DCubeTexture9*>(dst_resource);
				pDestResourceLeft = dst_cube_tex->DirectModeGetLeft();
				pDestResourceRight = dst_cube_tex->DirectModeGetRight();
			}
			else {
				pDestResourceLeft = reinterpret_cast<IDirect3DCubeTexture9*>(dst_resource);
			}
			break;
		}
		HRESULT hr = CopyResource(state, pSourceResourceLeft, pDestResourceLeft, NULL, NULL);
		if (FAILED(hr))
			COMMAND_LIST_LOG(state, "  Direct Mode full copy of left side failed with hr=%x\n", hr);

		if (!pSourceResourceRight) {
			COMMAND_LIST_LOG(state, "  Direct Mode, source resource is not stereo, copying left side to both eyes \n");
			HRESULT hr = CopyResource(state, pSourceResourceLeft, pDestResourceRight, NULL, NULL);
			if (FAILED(hr))
				COMMAND_LIST_LOG(state, "  Direct Mode full copy of source left to dest right failed with hr=%x\n", hr);
		}
		else if (!pDestResourceRight) {
			COMMAND_LIST_LOG(state, "  Direct Mode, source and dest resources are not stereo, copied left side only \n");
		}
		else {
			HRESULT hr = CopyResource(state, pSourceResourceRight, pDestResourceRight, NULL, NULL);
			if (FAILED(hr))
				COMMAND_LIST_LOG(state, "  Direct Mode full copy of right side failed with hr=%x\n", hr);
		}
	}
}

void ResourceCopyOperation::run(CommandListState *state)
{
	D3D9Wrapper::IDirect3DDevice9 *mHackerDevice = state->mHackerDevice;
	IDirect3DResource9 *src_resource = NULL;
	IDirect3DResource9 *dst_resource = NULL;

	IDirect3DResource9 **pp_cached_resource = &cached_resource;
	ResourcePool *p_resource_pool = &resource_pool;

	D3D9Wrapper::IDirect3DResource9 *wrapper = NULL;
	UINT stride = 0;
	UINT offset = 0;
	D3DFORMAT format = D3DFMT_UNKNOWN;
	UINT buf_src_size = 0, buf_dst_size = 0;

	COMMAND_LIST_LOG(state, "%S\n", ini_line.c_str());

	if (src.type == ResourceCopyTargetType::EMPTY) {
		dst.SetResource(state, NULL, 0, 0, D3DFMT_UNKNOWN, 0, NULL);
		return;
	}

	src_resource = src.GetResource(state, &stride, &offset, &format, &buf_src_size,
		((options & ResourceCopyOptions::REFERENCE) ? &dst : NULL), &wrapper);

	if (!src_resource) {
		COMMAND_LIST_LOG(state, "  Copy source was NULL\n");
		if (!(options & ResourceCopyOptions::UNLESS_NULL)) {
			// Still set destination to NULL - if we are copying a
			// resource we generally expect it to be there, and
			// this will make errors more obvious if we copy
			// something that doesn't exist. This behaviour can be
			// overridden with the unless_null keyword.
			dst.SetResource(state, NULL, 0, 0, D3DFMT_UNKNOWN, 0, NULL);
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

		if (dst.custom_resource->max_copies_per_frame) {
			if (dst.custom_resource->frame_no != G->frame_no) {
				dst.custom_resource->frame_no = G->frame_no;
				dst.custom_resource->copies_this_frame = 1;
			}
			else if (dst.custom_resource->copies_this_frame++ >= dst.custom_resource->max_copies_per_frame) {
				COMMAND_LIST_LOG(state, "  max_copies_per_frame exceeded\n");
				return;
			}
		}

		dst.custom_resource->OverrideOutOfBandInfo(&format, &stride);
	}

	FillInMissingInfo(state, src_resource, &stride, &offset, &buf_src_size, &format);

	if (options & ResourceCopyOptions::COPY_MASK) {
		RecreateCompatibleResource(&ini_line, &dst, src_resource,
			pp_cached_resource, p_resource_pool,
			state, mHackerDevice->mStereoHandle,
			options, stride, offset, format, &buf_dst_size);

		if (!*pp_cached_resource) {
			LogDebug("Resource copy error: Could not create/update destination resource\n");
			goto out_release;
		}
		dst_resource = *pp_cached_resource;

		if (options & ResourceCopyOptions::COPY_DESC) {
			// RecreateCompatibleResource has already done the work
			COMMAND_LIST_LOG(state, "  copying resource description\n");
		}
		else if (options & ResourceCopyOptions::STEREO2MONO) {
			COMMAND_LIST_LOG(state, "  performing reverse stereo blit\n");
			Profiling::stereo2mono_copies++;
			if (G->gForceStereo == 2) {
				//DirectModeReverseStereoBlit(src_resource, dst_resource, state, direct_mode_wrapped_resource_source, direct_mode_wrapped_resource_dest);
			}
			else {
				D3DRESOURCETYPE src_type;
				D3DRESOURCETYPE dst_type;
				src_type = src_resource->GetType();
				dst_type = dst_resource->GetType();
				if ((src_type != D3DRTYPE_SURFACE && src_type != D3DRTYPE_TEXTURE) || (dst_type != D3DRTYPE_TEXTURE && dst_type != D3DRTYPE_SURFACE)) {
					// TODO: I think it should be possible to do this with all
					// resource types (possibly including buffers from the
					// discovery of the stereo parameters in the cb12 slot), but I
					// need to test it and make sure it works first
					LogInfo("Resource copy: Reverse stereo blit not supported on resource source type %d, resource dest type %d\n", src_type, dst_type);
					goto out_release;
				}
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
				IDirect3DSurface9 *src_surface;
				IDirect3DSurface9 *dst_surface;
				switch (src_type) {
				case D3DRTYPE_SURFACE:
					src_surface = (IDirect3DSurface9*)src_resource;
					break;
				default:
					((IDirect3DTexture9*)src_resource)->GetSurfaceLevel(0, &src_surface);
				}
				switch (dst_type) {
				case D3DRTYPE_SURFACE:
					dst_surface = (IDirect3DSurface9*)dst_resource;
					break;
				default:
					((IDirect3DTexture9*)dst_resource)->GetSurfaceLevel(0, &dst_surface);
				}
				LONG fallback = state->mHackerDevice->mParamTextureManager.mActive ? 0 : 1;
				bool sli_enabled = state->mHackerDevice->sli_enabled();
				if (!fallback && sli_enabled && src_type != D3DRTYPE_TEXTURE) {
					RecreateCompatibleResource(&(ini_line + L" (intermediate)"),
						NULL, src_resource, (IDirect3DResource9**)&stereo2mono_intermediate,
						p_resource_pool,
						state, mHackerDevice->mStereoHandle,
						(ResourceCopyOptions)(options | ResourceCopyOptions::STEREO),
						stride, offset, format, NULL);
					IDirect3DSurface9 *stereo2mono_intermediate_surface = NULL;
					stereo2mono_intermediate->GetSurfaceLevel(0, &stereo2mono_intermediate_surface);
					IDirect3DSurface9 *origRT;
					state->mOrigDevice->GetRenderTarget(0, &origRT);
					state->mOrigDevice->SetRenderTarget(0, stereo2mono_intermediate_surface);
					state->mOrigDevice->Clear(0L, NULL, D3DCLEAR_TARGET, 0x00000000, 1.0f, 0L);
					state->mOrigDevice->SetRenderTarget(0, origRT);
					origRT->Release();
					state->mOrigDevice->StretchRect(src_surface, NULL, stereo2mono_intermediate_surface, NULL, D3DTEXF_POINT);
					ReverseStereoBlit(dst_surface, stereo2mono_intermediate_surface, state, fallback);
				}
				else {
					ReverseStereoBlit(dst_surface, src_surface, state, fallback);
				}
				if (dst_type == D3DRTYPE_TEXTURE)
					dst_surface->Release();
				if (src_type == D3DRTYPE_TEXTURE)
					src_surface->Release();
			}
		}
		else if (options & ResourceCopyOptions::RESOLVE_MSAA) {
			COMMAND_LIST_LOG(state, "  resolving MSAA\n");
			Profiling::msaa_resolutions++;
			ResolveMSAA(dst_resource, src_resource, state);
		}
		else if (buf_dst_size) {
			COMMAND_LIST_LOG(state, "  performing region copy\n");
			Profiling::buffer_region_copies++;
			SpecialCopyBufferRegion(dst_resource, src_resource,
				state, stride, &offset,
				buf_src_size, buf_dst_size);
		}
		else {
			COMMAND_LIST_LOG(state, "  performing full copy\n");
			Profiling::resource_full_copies++;
			if (G->gForceStereo == 2) {
				//DirectModeCopyResource(state, src_resource, dst_resource, direct_mode_wrapped_resource_source, direct_mode_wrapped_resource_dest);
			}
			else {
				HRESULT hr = CopyResource(state, src_resource, dst_resource, NULL, NULL);
				if (FAILED(hr))
					COMMAND_LIST_LOG(state, "  full copy failed with hr=%x\n", hr);
			}
		}
	}
	else {
		COMMAND_LIST_LOG(state, "  copying by reference\n");
		Profiling::resource_reference_copies++;
		dst_resource = src_resource;
	}
	dst.SetResource(state, dst_resource, stride, offset, format, buf_dst_size, &wrapper);

	if (options & ResourceCopyOptions::SET_VIEWPORT)
		SetViewportFromResource(state, dst_resource);

out_release:
	if (src_resource)
		src_resource->Release();
}
HRESULT ResourceCopyTarget::DirectModeGetRealBackBuffer(CommandListState *state, UINT iBackBuffer, D3D9Wrapper::IDirect3DSurface9 ** ppBackBuffer)
{
	D3D9Wrapper::FakeSwapChain *swapChain = &state->mHackerDevice->mFakeSwapChains[0];
	if (!swapChain)
		return D3DERR_INVALIDCALL;
	D3D9Wrapper::IDirect3DSurface9 *wrappedBackBuffer;
	if (G->SCREEN_UPSCALING) {
		if (iBackBuffer > (swapChain->mDirectModeUpscalingBackBuffers.size() - 1))
			return D3DERR_INVALIDCALL;
		wrappedBackBuffer = swapChain->mDirectModeUpscalingBackBuffers.at(iBackBuffer);
		if (!wrappedBackBuffer)
			return D3DERR_INVALIDCALL;
		*ppBackBuffer = wrappedBackBuffer;
	}
	else {
		if (iBackBuffer > (swapChain->mFakeBackBuffers.size() - 1))
			return D3DERR_INVALIDCALL;
		wrappedBackBuffer = swapChain->mFakeBackBuffers.at(iBackBuffer);
		if (!wrappedBackBuffer)
			return D3DERR_INVALIDCALL;
		*ppBackBuffer = wrappedBackBuffer;
	}
	wrappedBackBuffer->DirectModeGetLeft()->AddRef();
	return D3D_OK;
}
HRESULT ResourceCopyTarget::DirectModeGetFakeBackBuffer(CommandListState *state, UINT iBackBuffer, D3D9Wrapper::IDirect3DSurface9 ** ppBackBuffer)
{
	D3D9Wrapper::FakeSwapChain *swapChain = &state->mHackerDevice->mFakeSwapChains[0];
	if (!swapChain)
		return D3DERR_INVALIDCALL;
	if (iBackBuffer > (swapChain->mFakeBackBuffers.size() - 1))
		return D3DERR_INVALIDCALL;
	D3D9Wrapper::IDirect3DSurface9 *wrappedBackBuffer = swapChain->mFakeBackBuffers.at(iBackBuffer);
	if (!wrappedBackBuffer)
		return D3DERR_INVALIDCALL;
	*ppBackBuffer = wrappedBackBuffer;
	wrappedBackBuffer->DirectModeGetLeft()->AddRef();
	return D3D_OK;
}
bool FunctionAutoConvergence::run(CommandListState *state, vector<CommandListVariableFloat*> *params)
{
	if (params->size() != 19)
		return false;
	float *last_set_convergence = &internalFunctionFloats[InternalFunctionFloat::ACSTATE_LAST_SET_CONVERGENCE];
	float *user_popout_bias = &internalFunctionFloats[InternalFunctionFloat::ACSTATE_USER_POPOUT_BIAS];
	float *judder = &internalFunctionFloats[InternalFunctionFloat::ACSTATE_JUDDER];
	float *judder_time = &internalFunctionFloats[InternalFunctionFloat::ACSTATE_JUDDER_TIME];
	float *last_convergence1 = &internalFunctionFloats[InternalFunctionFloat::ACSTATE_LAST_CONVERGENCE1];
	float *last_convergence2 = &internalFunctionFloats[InternalFunctionFloat::ACSTATE_LAST_CONVERGENCE2];
	float *last_convergence3 = &internalFunctionFloats[InternalFunctionFloat::ACSTATE_LAST_CONVERGENCE3];
	float *last_convergence4 = &internalFunctionFloats[InternalFunctionFloat::ACSTATE_LAST_CONVERGENCE4];

	float *time = &internalFunctionFloats[InternalFunctionFloat::ACSTATE_TIME];
	float *prev_time = &internalFunctionFloats[InternalFunctionFloat::ACSTATE_PREV_TIME];
	float *correction_start = &internalFunctionFloats[InternalFunctionFloat::ACSTATE_CORRECTION_START];
	float *last_calculated_convergence = &internalFunctionFloats[InternalFunctionFloat::ACSTATE_LAST_CALCULATED_CONVERGENCE];

	*time = (float)(GetTickCount() - G->ticks_at_launch) / 1000.0f;

	float* auto_convergence = &params->at(0)->fval;
	float depth = params->at(1)->fval;

	float min_convergence = params->at(2)->fval;
	float max_convergence_soft = params->at(3)->fval;
	float max_convergence_hard = params->at(4)->fval;
	float ini_popout_bias = params->at(5)->fval;
	float slow_convergence_rate = params->at(6)->fval;
	float slow_convergence_threshold_near = params->at(7)->fval;
	float slow_convergence_threshold_far = params->at(8)->fval;
	float instant_convergence_threshold = params->at(9)->fval;
	float anti_judder_threshold = params->at(10)->fval;
	float prev_auto_convergence_enabled = params->at(11)->fval;

	float correction_rate_threshold = params->at(12)->fval;
	float correction_period = params->at(13)->fval;
	float *correcting = &params->at(14)->fval;
	float get = params->at(15)->fval;

	float convergence = params->at(16)->fval;
	float raw_sep = params->at(17)->fval;
	float separation = params->at(18)->fval;

	float target_convergence, convergence_difference;
	float current_convergence;
	float new_convergence;

	float qnan = numeric_limits<float>::quiet_NaN();
	if (get != 1.0) {
		current_convergence = *last_set_convergence;
		new_convergence = *last_calculated_convergence;
		if (isnan(current_convergence) || isnan(new_convergence)) {
			*auto_convergence = qnan;
			return false;
		}
		convergence_difference = new_convergence - current_convergence;
		if (*correcting == 1.0) {
			float remaining_time = (correction_period - (*time - *correction_start));
			if (remaining_time <= 0) {
				*auto_convergence = new_convergence;
				*correcting = 0.0f;
			}
			else {
				float correction_rate = (abs(convergence_difference) / remaining_time);
				float diff = correction_rate * (*time - *prev_time) * 2;
				if (convergence_difference > 0) {
					if (new_convergence <= (current_convergence + diff)) {
						*auto_convergence = new_convergence;
						*correcting = 0.0f;
					}
					else {
						*auto_convergence = current_convergence + diff;
					}
				}
				else {
					if (new_convergence >= (current_convergence - diff)) {
						*auto_convergence = new_convergence;
						*correcting = 0.0f;
					}
					else {
						*auto_convergence = current_convergence - diff;
					}
				}
			}
		}
		else if (-convergence_difference > slow_convergence_threshold_near) {
			float diff = slow_convergence_rate * (*time - *prev_time) * 2;
			*auto_convergence = max(new_convergence, current_convergence - diff);
		}
		else if (convergence_difference > slow_convergence_threshold_far) {
			float diff = slow_convergence_rate * (*time - *prev_time) * 2;
			*auto_convergence = min(new_convergence, current_convergence + diff);
		}
		else {
			//*auto_convergence = qnan;
		}
		*last_convergence4 = *last_convergence3;
		*last_convergence3 = *last_convergence2;
		*last_convergence2 = *last_convergence1;
		*last_convergence1 = current_convergence;
		*last_set_convergence = *auto_convergence;
		*prev_time = *time;
		return true;
	}
	else
	{
		if (isnan(convergence))
			GetConvergence(state->mHackerDevice, state->cachedStereoValues, &convergence);
		if (isnan(raw_sep))
			GetSeparation(state->mHackerDevice, state->cachedStereoValues, &raw_sep);
		if (isnan(separation))
			GetEyeSeparation(state->mHackerDevice, state->cachedStereoValues, &separation);

		if (prev_auto_convergence_enabled == 1.0f) {
			current_convergence = *last_set_convergence;
			if (isnan(current_convergence))
				current_convergence = convergence;
			else
				*last_set_convergence = qnan;
		}
		else {
			current_convergence = convergence;
			*last_set_convergence = qnan;
			*last_convergence1 = qnan;
			*last_convergence2 = qnan;
			*last_convergence3 = qnan;
			*last_convergence4 = qnan;
		}
		if (isinf(depth)) {
			// No depth buffer, auto-convergence cannot work. Bail now,
			// otherwise we would set the hard maximum convergence limit
			*auto_convergence = qnan;
			*judder = 0.0f;
			return true;
		}

		// A lot of the maths below is experimental to try to find a good
		// auto-convergence algorithm that works well with a wide variety of
		// screen sizes, seating distances, and varying scenes in the game.

		// Apply the max convergence now, before we apply the popout bias, on
		// the theory that the max suitable convergence is going to vary based
		// on screen size
		target_convergence = min(depth, max_convergence_soft);

		// Disabling this for now since we want the user popout bias to be
		// saved in the d3dx_user.ini, meaning we can't calculate it on the
		// GPU. TODO: Maybe bring this back once able to stage arbitrary buffers.
		//if (user_convergence_delta) {
		//	// User adjusted the convergence. Convert this to an equivalent
		//	// popout bias for auto-convergence that we save in a buffer on
		//	// the GPU. This is the below formula re-arranged:
		//	target_popout_bias = (separation*(convergence - target_convergence)/(raw_sep*w));
		//	target_popout_bias = min(max(target_popout_bias, -1), 1) - ini_popout_bias;
		//	state[0].user_popout_bias = target_popout_bias;
		//}

		// Apply the popout bias. This experimental formula is derived by
		// taking the nvidia formula with the perspective divide and the
		// original x=0:
		//
		//   x' = x + separation * (depth - convergence) / depth
		//   x' = separation * (depth - convergence) / depth
		//
		// That gives us our original stereo corrected X value using a
		// convergence that would place the closest object at screen depth
		// (barring the result of capping the convergence). We want to find a
		// new convergence value that would instead position the object
		// slightly popped out - to do so in a way that is comfortable
		// regardless of scene, screen size, and player distance from the
		// screen we apply the popout bias to the x', call it x''. Because we
		// are modifying this post-separation, we will need to multiply by the
		// raw separation value so that it scales with separation (otherwise we
		// would always end with the full pop-out regardless of separation -
		// which is actually kind of cool - people who like toyification might
		// appreciate it, but we do want turning the stereo effect down to
		// reduce the stereo effect):
		//
		//   x'' = x' - (popout_bias * raw_separation)
		//
		// Then we rearrange the nvidia formula and substitute in the two
		// previous formulas to find the new convergence:
		//
		//   x'' = separation * (depth - convergence') / depth
		//   convergence' = depth * (1 - (x'' / separation))
		//   convergence' = depth * (((popout_bias * raw_separation) - x') / separation + 1)
		//   convergence' = depth * popout * raw_separation / separation + convergence
		//
		float new_convergence = depth * (ini_popout_bias + *user_popout_bias) * raw_sep / separation + target_convergence;

		// Apply the minimum convergence now to ensure we can't go negative
		// regardless of what the popout bias did, and a hard maximum
		// convergence to prevent us going near infinity:
		new_convergence = min(max(new_convergence, min_convergence), max_convergence_hard);
		*last_calculated_convergence = new_convergence;
		convergence_difference = new_convergence - current_convergence;
		if (abs(convergence_difference) >= instant_convergence_threshold) {

			// The anti-judder countermeasure aims to detect situations
			// where the auto-convergence makes an adjustment that moves
			// something on or off screen, that in turn triggers another
			// auto-convergence correction causing an oscillation between
			// two or more convergence values. In this situation we want to
			// stop trying to set the convergence back to a value it
			// recently was, but we also have to choose which state we stop
			// in. To try to avoid the camera being obscured, we try to
			// stop in the lower convergence state
			bool any = false;
			if (abs(new_convergence - *last_convergence1) < anti_judder_threshold)
				any = true;
			if (abs(new_convergence - *last_convergence2) < anti_judder_threshold)
				any = true;
			if (abs(new_convergence - *last_convergence3) < anti_judder_threshold)
				any = true;
			if (abs(new_convergence - *last_convergence4) < anti_judder_threshold)
				any = true;
			if (any) {
				*judder = 1.0f;
				*judder_time = *time;
				if (new_convergence < current_convergence) {
					*auto_convergence = new_convergence;
					*last_set_convergence = *auto_convergence;
					//*last_convergence.xyzw = float4(current_convergence, *last_convergence.xyz);
					*last_convergence1 = current_convergence;
					*last_convergence2 = *last_convergence1;
					*last_convergence3 = *last_convergence2;
					*last_convergence4 = *last_convergence3;
					//*last_calculated_convergence = *auto_convergence;
					return true;
				}
				// *last_calculated_convergence = *auto_convergence;
				*auto_convergence = qnan;
				return true;
			}

			*auto_convergence = new_convergence;
		}
		else if (*correcting == 0.0 && abs(convergence_difference) >= correction_rate_threshold) {

			bool any = false;
			if (abs(new_convergence - *last_convergence1) < anti_judder_threshold)
				any = true;
			if (abs(new_convergence - *last_convergence2) < anti_judder_threshold)
				any = true;
			if (abs(new_convergence - *last_convergence3) < anti_judder_threshold)
				any = true;
			if (abs(new_convergence - *last_convergence4) < anti_judder_threshold)
				any = true;
			if (any) {
				*judder = 1.0f;
				*judder_time = *time;
				if (new_convergence < current_convergence) {
					*auto_convergence = new_convergence;
					*last_set_convergence = *auto_convergence;
					*last_convergence1 = current_convergence;
					*last_convergence2 = *last_convergence1;
					*last_convergence3 = *last_convergence2;
					*last_convergence4 = *last_convergence3;
					//*last_calculated_convergence = *auto_convergence;
					return true;
				}
				//*last_calculated_convergence = *auto_convergence;
				*auto_convergence = qnan;
				return true;
			}
			*correction_start = *prev_time;
			*correcting = 1.0f;
			float remaining_time = (correction_period - (*time - *correction_start));
			if (remaining_time <= 0) {
				*auto_convergence = new_convergence;
				*correcting = 0.0f;
			}
			else {
				float correction_rate = (abs(convergence_difference) / remaining_time);
				float diff = correction_rate * (*time - *prev_time) * 2;
				if (convergence_difference > 0) {
					if (new_convergence <= (current_convergence + diff)) {
						*auto_convergence = new_convergence;
						*correcting = 0.0f;
					}
					else {
						*auto_convergence = current_convergence + diff;
					}
				}
				else {
					if (new_convergence >= (current_convergence - diff)) {
						*auto_convergence = new_convergence;
						*correcting = 0.0f;
					}
					else {
						*auto_convergence = current_convergence - diff;
					}
				}
			}
		}
		else if (*correcting == 1.0) {
			float remaining_time = (correction_period - (*time - *correction_start));
			if (remaining_time <= 0) {
				*auto_convergence = new_convergence;
				*correcting = 0.0f;
			}
			else {
				float correction_rate = (abs(convergence_difference) / remaining_time);
				float diff = correction_rate * (*time - *prev_time) * 2;
				if (convergence_difference > 0) {
					if (new_convergence <= (current_convergence + diff)) {
						*auto_convergence = new_convergence;
						*correcting = 0.0f;
					}
					else {
						*auto_convergence = current_convergence + diff;
					}
				}
				else {
					if (new_convergence >= (current_convergence - diff)) {
						*auto_convergence = new_convergence;
						*correcting = 0.0f;
					}
					else {
						*auto_convergence = current_convergence - diff;
					}
				}
			}
		}
		else if (-convergence_difference > slow_convergence_threshold_near) {
			// The *2 here is to compensate for the lag in setting the
			// convergence due to the asynchronous transfer.
			float diff = slow_convergence_rate * (*time - *prev_time) * 2;
			*auto_convergence = max(new_convergence, current_convergence - diff);
		}
		else if (convergence_difference > slow_convergence_threshold_far) {
			// The *2 here is to compensate for the lag in setting the
			// convergence due to the asynchronous transfer.
			float diff = slow_convergence_rate * (*time - *prev_time) * 2;
			*auto_convergence = min(new_convergence, current_convergence + diff);
		}
		else {
			*auto_convergence = qnan;
		}

		//state[0].last_convergence.xyzw = float4(current_convergence, state[0].last_convergence.xyz);
		*last_convergence4 = *last_convergence3;
		*last_convergence3 = *last_convergence2;
		*last_convergence2 = *last_convergence1;
		*last_convergence1 = current_convergence;
		*last_set_convergence = *auto_convergence;
		//*last_calculated_convergence = *auto_convergence;
		*judder = 0.0f;
		*prev_time = *time;
		return true;
	}
}

CustomFunction::CustomFunction()
{
}

CustomFunction::~CustomFunction()
{
}

bool CustomFunction::run(CommandListState *, vector<CommandListVariableFloat*>* params)
{
	return false;
}

bool CustomFunctionCommand::ParseParam(const wchar_t *name, CommandList *pre_command_list, const wstring *ini_namespace)
{
	CommandListVariableFloat *var = NULL;
	if (!find_local_variable(name, pre_command_list->scope, &var) &&
		!parse_command_list_var_name(name, ini_namespace, &var))
		return false;
	params.push_back(var);
	return true;
}

CustomFunctionCommand::CustomFunctionCommand()
{
}

CustomFunctionCommand::~CustomFunctionCommand()
{
}

void CustomFunctionCommand::run(CommandListState *state)
{
	function->run(state, &params);
}
void SetShaderConstant::run(CommandListState *state)
{

	COMMAND_LIST_LOG(state, "%S\n", ini_line.c_str());

	COMMAND_LIST_LOG(state, "  set shader constant, shader_type = %lc, constant_type = %u, slot = %u \n", shader_type, constant_type, slot);

	if (vars.size()) {
		assign->run(state);
		UINT count = (UINT)ceil(vars.size() / 4);
		vector<float> pConstantsF;
		vector<int> pConstantsI;
		vector<BOOL> pConstantsB;
		COMMAND_LIST_LOG(state, "  vector count = %u \n", count);
		switch (constant_type) {
		case ConstantType::FLOAT:
			pConstantsF.resize(count * 4);
			if (vars.size() % 4 != 0) {
				switch (shader_type) {
				case L'v':
					state->mOrigDevice->GetVertexShaderConstantF(slot + count - 1, &pConstantsF[((slot + count - 1) * 4)], 1);
					break;
				case L'p':
					state->mOrigDevice->GetPixelShaderConstantF(slot + count - 1, &pConstantsF[((slot + count - 1) * 4)], 1);
					break;
				default:
					return;
				}
			}
			copy(vars.begin(), vars.end(), pConstantsF.begin());
			switch (shader_type) {
			case L'v':
				state->mOrigDevice->SetVertexShaderConstantF(slot, &pConstantsF[0], count);
				return;
			case L'p':
				state->mOrigDevice->SetPixelShaderConstantF(slot, &pConstantsF[0], count);
				return;
			}
			return;
		case ConstantType::INT:
			pConstantsI.resize(count * 4);
			if (vars.size() % 4 != 0) {
				switch (shader_type) {
				case L'v':
					state->mOrigDevice->GetVertexShaderConstantI(slot + count - 1, &pConstantsI[((slot + count - 1) * 4)], 1);
					break;
				case L'p':
					state->mOrigDevice->GetPixelShaderConstantI(slot + count - 1, &pConstantsI[((slot + count - 1) * 4)], 1);
					break;
				default:
					return;
				}
			}
			std::transform(vars.begin(), vars.end(), pConstantsI.begin(), [](float v) -> int { return (int)v; });
			switch (shader_type) {
			case L'v':
				state->mOrigDevice->SetVertexShaderConstantI(slot, &pConstantsI[0], count);
				return;
			case L'p':
				state->mOrigDevice->SetPixelShaderConstantI(slot, &pConstantsI[0], count);
				return;
			}
			return;
		case ConstantType::BOOL:
			pConstantsB.resize(count * 4);
			if (vars.size() % 4 != 0) {
				switch (shader_type) {
				case L'v':
					state->mOrigDevice->GetVertexShaderConstantB(slot + count - 1, &pConstantsB[((slot + count - 1) * 4)], 1);
					break;
				case L'p':
					state->mOrigDevice->GetPixelShaderConstantB(slot + count - 1, &pConstantsB[((slot + count - 1) * 4)], 1);
					break;
				default:
					return;
				}
			}
			std::transform(vars.begin(), vars.end(), pConstantsB.begin(), [](float v) -> BOOL { return !!v; });
			switch (shader_type) {
			case L'v':
				state->mOrigDevice->SetVertexShaderConstantB(slot, &pConstantsB[0], count);
				return;
			case L'p':
				state->mOrigDevice->SetPixelShaderConstantB(slot, &pConstantsB[0], count);
				return;
			}
			return;
		}
	}
	switch (constant_type) {
		float pConstantsF[4];
		int pConstantsI[4];
		BOOL pConstantsB[4];
		COMMAND_LIST_LOG(state, "  component = %f \n", component);
	case ConstantType::FLOAT:
		switch (shader_type) {
		case L'v':
			state->mOrigDevice->GetVertexShaderConstantF(slot, pConstantsF, 1);
			break;
		case L'p':
			state->mOrigDevice->GetPixelShaderConstantF(slot, pConstantsF, 1);
			break;
		default:
			return;
		}
		if (component == &DirectX::XMFLOAT4::x) {
			pConstantsF[0] = expression.evaluate(state);
		}
		else if (component == &DirectX::XMFLOAT4::y) {
			pConstantsF[1] = expression.evaluate(state);
		}
		else if (component == &DirectX::XMFLOAT4::z) {
			pConstantsF[2] = expression.evaluate(state);
		}
		else if (component == &DirectX::XMFLOAT4::w) {
			pConstantsF[3] = expression.evaluate(state);
		}
		switch (shader_type) {
		case L'v':
			state->mOrigDevice->SetVertexShaderConstantF(slot, pConstantsF, 1);
			return;
		case L'p':
			state->mOrigDevice->SetPixelShaderConstantF(slot, pConstantsF, 1);
			return;
		}
		return;
	case ConstantType::INT:
		switch (shader_type) {
		case L'v':
			state->mOrigDevice->GetVertexShaderConstantI(slot, pConstantsI, 1);
			break;
		case L'p':
			state->mOrigDevice->GetPixelShaderConstantI(slot, pConstantsI, 1);
			break;
		default:
			return;
		}
		if (component == &DirectX::XMFLOAT4::x) {
			pConstantsI[0] = (int)expression.evaluate(state);
		}
		else if (component == &DirectX::XMFLOAT4::y) {
			pConstantsI[1] = (int)expression.evaluate(state);
		}
		else if (component == &DirectX::XMFLOAT4::z) {
			pConstantsI[2] = (int)expression.evaluate(state);
		}
		else if (component == &DirectX::XMFLOAT4::w) {
			pConstantsI[3] = (int)expression.evaluate(state);
		}
		switch (shader_type) {
		case L'v':
			state->mOrigDevice->SetVertexShaderConstantI(slot, pConstantsI, 1);
			return;
		case L'p':
			state->mOrigDevice->SetPixelShaderConstantI(slot, pConstantsI, 1);
			return;
		}
		return;
	case ConstantType::BOOL:
		switch (shader_type) {
		case L'v':
			state->mOrigDevice->GetVertexShaderConstantB(slot, pConstantsB, 1);
			break;
		case L'p':
			state->mOrigDevice->GetPixelShaderConstantB(slot, pConstantsB, 1);
			break;
		default:
			return;
		}
		if (component == &DirectX::XMFLOAT4::x) {
			pConstantsB[0] = (BOOL)expression.evaluate(state);
		}
		else if (component == &DirectX::XMFLOAT4::y) {
			pConstantsB[1] = (BOOL)expression.evaluate(state);
		}
		else if (component == &DirectX::XMFLOAT4::z) {
			pConstantsB[2] = (BOOL)expression.evaluate(state);
		}
		else if (component == &DirectX::XMFLOAT4::w) {
			pConstantsB[3] = (BOOL)expression.evaluate(state);
		}
		switch (shader_type) {
		case L'v':
			state->mOrigDevice->SetVertexShaderConstantB(slot, pConstantsB, 1);
			return;
		case L'p':
			state->mOrigDevice->SetPixelShaderConstantB(slot, pConstantsB, 1);
			return;
		}
	}
}
void VariableArrayFromShaderConstantAssignment::run(CommandListState *state)
{
	COMMAND_LIST_LOG(state, "%S\n", ini_line.c_str());

	COMMAND_LIST_LOG(state, "  get shader constant array, shader_type = %lc, constant_type = %u, slot = %u \n", shader_type, constant_type, slot);

	UINT count = (UINT)ceil(vars.size() / 4);
	vector<float> pConstantsF;
	vector<int> pConstantsI;
	vector<BOOL> pConstantsB;
	int i = 0;

	COMMAND_LIST_LOG(state, "  vector count = %u \n", count);

	switch (constant_type) {
	case ConstantType::FLOAT:
		pConstantsF.resize(count * 4);
		switch (shader_type) {
		case L'v':
			state->mOrigDevice->GetVertexShaderConstantF(slot, &pConstantsF[0], count);
			break;
		case L'p':
			state->mOrigDevice->GetPixelShaderConstantF(slot, &pConstantsF[0], count);
			break;
		default:
			return;
		}
		for (auto it = vars.begin(); it != vars.end(); ++it)
		{
			*(*it) = pConstantsF[i];
			++i;
		}
		return;
	case ConstantType::INT:
		pConstantsI.resize(count * 4);
		switch (shader_type) {
		case L'v':
			state->mOrigDevice->GetVertexShaderConstantI(slot, &pConstantsI[0], count);
			break;
		case L'p':
			state->mOrigDevice->GetPixelShaderConstantI(slot, &pConstantsI[0], count);
			break;
		default:
			return;
		}
		for (auto it = vars.begin(); it != vars.end(); ++it)
		{
			*(*it) = (float)pConstantsI[i];
			++i;
		}
		return;
	case ConstantType::BOOL:
		pConstantsB.resize(count * 4);
		switch (shader_type) {
		case L'v':
			state->mOrigDevice->GetVertexShaderConstantB(slot, &pConstantsB[0], count);
			break;
		case L'p':
			state->mOrigDevice->GetPixelShaderConstantB(slot, &pConstantsB[0], count);
			break;
		default:
			return;
		}
		for (auto it = vars.begin(); it != vars.end(); ++it)
		{
			*(*it) = (float)pConstantsB[i];
			++i;
		}
		return;
	}
}

void VariableArrayAssignment::run(CommandListState *state)
{
	for (auto it = shader_constant_assignments.begin(); it != shader_constant_assignments.end();)
	{
		(*it).run(state);
		++it;
	}
	for (auto it = array_assignments.begin(); it != array_assignments.end();)
	{
		(*it).run(state);
		++it;
	}
	for (auto it = matrix_assignments.begin(); it != matrix_assignments.end();)
	{
		(*it).run(state);
		++it;
	}
	for (auto it = matrix_expression_assignments.begin(); it != matrix_expression_assignments.end();)
	{
		(*it).run(state);
		++it;
	}
	for (auto it = expression_assignments.begin(); it != expression_assignments.end();)
	{
		(*it).run(state);
		++it;
	}
}

void VariableArrayFromArrayAssignment::run(CommandListState *)
{
	for (auto it = map.cbegin(); it != map.cend();)
	{
		*(it->first) = *(it->second);
		++it;
	}
}

void VariableArrayFromExpressionAssignment::run(CommandListState *state)
{
	*fval = expression.evaluate(state);
}

::D3DXMATRIX CommandListMatrixOperator::evaluate(CommandListState * state, D3D9Wrapper::IDirect3DDevice9 * device)
{
	if (lhs) // Binary operator
		return evaluate(lhs->evaluate(state, device), rhs->evaluate(state, device));
	return evaluate((*::D3DXMatrixIdentity(&::D3DXMATRIX())), rhs->evaluate(state, device));
}

::D3DXMATRIX CommandListMatrixOperand::evaluate(CommandListState * state, D3D9Wrapper::IDirect3DDevice9 * device)
{
	return m_pMatrix->fmatrix;
}

void VariableArrayFromMatrixAssignment::run(CommandListState *)
{
	UINT m = start_slot;
	for (UINT i = 0; i < vars.size() && m < 16; ++i)
	{
		*(vars[i]) = (*matrix)[m];
		++m;
	}
}

void VariableArrayFromMatrixExpressionAssignment::run(CommandListState *state)
{
	::D3DXMATRIX matrix = expression.evaluate(state);
	UINT m = start_slot;
	for (UINT i = 0; i < vars.size() && m < 16; ++i)
	{
		*(vars[i]) = matrix[m];
		++m;
	}
}

void MatrixAssignment::run(CommandListState *state)
{
	for (auto it = shader_constant_assignments.begin(); it != shader_constant_assignments.end();)
	{
		(*it).run(state);
		++it;
	}
	for (auto it = array_assignments.begin(); it != array_assignments.end();)
	{
		(*it).run(state);
		++it;
	}
	for (auto it = matrix_assignments.begin(); it != matrix_assignments.end();)
	{
		(*it).run(state);
		++it;
	}
	for (auto it = matrix_expression_assignments.begin(); it != matrix_expression_assignments.end();)
	{
		(*it).run(state);
		++it;
	}
	for (auto it = expression_assignments.begin(); it != expression_assignments.end();)
	{
		(*it).run(state);
		++it;
	}
}

void MatrixFromShaderConstantAssignment::run(CommandListState *state)
{
	COMMAND_LIST_LOG(state, "%S\n", ini_line.c_str());

	COMMAND_LIST_LOG(state, "  get shader constant matrix, shader_type = %lc, constant_type = %u, slot = %u \n", shader_type, constant_type, slot);

	vector<float> pConstantsF;
	vector<int> pConstantsI;
	vector<BOOL> pConstantsB;
	UINT vectorCount = num_slots / 4;
	switch (constant_type) {
	case ConstantType::FLOAT:
		pConstantsF.assign(16, 0);
		switch (shader_type) {
		case L'v':
			state->mOrigDevice->GetVertexShaderConstantF(slot, &pConstantsF[0], vectorCount);
			break;
		case L'p':
			state->mOrigDevice->GetPixelShaderConstantF(slot, &pConstantsF[0], vectorCount);
			break;
		default:
			return;
		}
		*pMatrix = pConstantsF.data();
		return;
	case ConstantType::INT:
		pConstantsI.assign(16, 0);
		switch (shader_type) {
		case L'v':
			state->mOrigDevice->GetVertexShaderConstantI(slot, &pConstantsI[0], vectorCount);
			break;
		case L'p':
			state->mOrigDevice->GetPixelShaderConstantI(slot, &pConstantsI[0], vectorCount);
			break;
		default:
			return;
		}
		*pMatrix = (float*)pConstantsI.data();
		return;
	case ConstantType::BOOL:
		pConstantsB.assign(16, 0);
		switch (shader_type) {
		case L'v':
			state->mOrigDevice->GetVertexShaderConstantB(slot, &pConstantsB[0], vectorCount);
			break;
		case L'p':
			state->mOrigDevice->GetPixelShaderConstantB(slot, &pConstantsB[0], vectorCount);
			break;
		default:
			return;
		}
		*pMatrix = (float*)pConstantsB.data();
		return;
	}
}

void MatrixFromArrayAssignment::run(CommandListState *)
{
	UINT i = 0;
	for (UINT m = dst_start; m < 16 && i < arr.size(); ++m)
	{
		(*pMatrix)[m] = *(arr[i]);
		++i;
	}
}

void MatrixFromMatrixAssignment::run(CommandListState *)
{
	for (UINT i = 0; i < num_slots; ++i)
	{
		(*dst_matrix)[dst_start + i] = *(src_matrix)[src_start + i];
	}
}

void MatrixFromMatrixExpressionAssignment::run(CommandListState *state)
{
	::D3DXMATRIX src_matrix = expression.evaluate(state);
	for (UINT i = 0; i < num_slots; ++i)
	{
		(*pMatrix)[dst_start + i] = src_matrix[src_start + i];
	}
}

void MatrixFromExpressionAssignment::run(CommandListState *state)
{
	float val = expression.evaluate(state);
	(*pMatrix)[dst_start] = val;
}

void FrameAnalysisDumpConstantsCommand::run(CommandListState *state)
{
	// Fast exit if frame analysis is currently inactive:
	if (!G->analyse_frame)
		return;

	COMMAND_LIST_LOG(state, "%S\n", ini_line.c_str());
	state->mHackerDevice->DumpCB(shader_type, constant_type, start_slot, num_slots);
}

bool FrameAnalysisDumpConstantsCommand::noop(bool post, bool ignore_cto_pre, bool ignore_cto_post)
{
	return (G->hunting == HUNTING_MODE_DISABLED || G->frame_analysis_registered == false);
}
