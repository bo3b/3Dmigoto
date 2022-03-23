// Include before util.h (or any header that includes util.h) to get pretty
// version of LOCK_RESOURCE_CREATION_MODE:
#include "lock.h"

#include "CommandList.hpp"

#include <DDSTextureLoader.h>
#include <WICTextureLoader.h>
#include <algorithm>
#include <sstream>
#include "HackerDevice.hpp"
#include "HackerContext.hpp"
#include "Override.hpp"
#include "D3D11Wrapper.h"
#include "IniHandler.h"
#include "profiling.hpp"
#include "Hunting.hpp"
#include "cursor.h"

#include <D3DCompiler.h>

using namespace std;


CustomResources custom_resources;
CustomShaders custom_shaders;
ExplicitCommandListSections explicit_command_list_sections;
CommandListVariables command_list_globals;
std::vector<CommandListVariable*> persistent_variables;
std::vector<CommandList*> registered_command_lists;
std::unordered_set<CommandList*> command_lists_profiling;
std::unordered_set<CommandListCommand*> command_lists_cmd_profiling;
std::vector<std::shared_ptr<CommandList>> dynamically_allocated_command_lists;


// Adds consistent "3DMigoto" prefix to frame analysis log with appropriate
// level of indentation for the current recursion level. Using a
// macro instead of a function for this to concatenate static strings:
#define COMMAND_LIST_LOG(state, fmt, ...) \
    do { \
        (state)->hackerContext->FrameAnalysisLog("3DMigoto%*s " fmt, state->recursion + state->extraIndent, "", __VA_ARGS__); \
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
        command_list->timeSpentInclusive.QuadPart = 0;
        command_list->timeSpentExclusive.QuadPart = 0;
        command_list->executions = 0;
    }

    profiling_state->saved_recursive_time = state->profilingTimeRecursive;
    state->profilingTimeRecursive.QuadPart = 0;

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
    command_list->timeSpentInclusive.QuadPart += duration.QuadPart;
    command_list->timeSpentExclusive.QuadPart += duration.QuadPart - state->profilingTimeRecursive.QuadPart;
    command_list->executions++;
    state->profilingTimeRecursive.QuadPart = profiling_state->saved_recursive_time.QuadPart + duration.QuadPart;
}

static inline void profile_command_list_cmd_start(CommandListCommand *cmd,
        command_list_profiling_state *profiling_state)
{
    bool inserted;

    if (Profiling::mode != Profiling::Mode::TOP_COMMANDS)
        return;

    inserted = command_lists_cmd_profiling.insert(cmd).second;
    if (inserted) {
        cmd->preTimeSpent.QuadPart = 0;
        cmd->postTimeSpent.QuadPart = 0;
        cmd->preExecutions = 0;
        cmd->postExecutions = 0;
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
        cmd->postTimeSpent.QuadPart += end_time.QuadPart - profiling_state->cmd_start_time.QuadPart;
        cmd->postExecutions++;
    } else {
        cmd->preTimeSpent.QuadPart += end_time.QuadPart - profiling_state->cmd_start_time.QuadPart;
        cmd->preExecutions++;
    }
}

static void _run_command_list(CommandList *command_list, CommandListState *state, bool recursive=true)
{
    CommandList::Commands::iterator i;
    command_list_profiling_state profiling_state;

    if (state->recursion > max_command_list_recursion) {
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
        (*i)->Run(state);
        profile_command_list_cmd_end(i->get(), state, &profiling_state);
    }

    profile_command_list_end(command_list, state, &profiling_state);

    if (recursive) {
        state->recursion--;
        COMMAND_LIST_LOG(state, "}\n");
    }
}

static void command_list_flush_state(CommandListState *state)
{
    D3D11_MAPPED_SUBRESOURCE mapped_subresource;
    HRESULT hr;

    if (state->updateParams && state->hackerDevice->iniTexture) {
        hr = state->origContext1->Map(state->hackerDevice->iniTexture, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped_subresource);
        if (FAILED(hr)) {
            LOG_INFO("command_list_flush_state: Map failed\n");
            return;
        }
        memcpy(mapped_subresource.pData, G->iniParams.data(), sizeof(DirectX::XMFLOAT4) * G->iniParams.size());
        state->origContext1->Unmap(state->hackerDevice->iniTexture, 0);
        state->updateParams = false;
        Profiling::iniparams_updates++;
    }
}

static void run_command_list_complete(HackerDevice *hacker_device,
        HackerContext *hacker_context,
        CommandList *command_list,
        DrawCallInfo *call_info,
        ID3D11Resource **resource,
        ID3D11View *view,
        bool post)
{
    CommandListState state;
    state.hackerDevice = hacker_device;
    state.hackerContext = hacker_context;
    state.origDevice1 = hacker_device->GetPassThroughOrigDevice1();
    state.origContext1 = hacker_context->GetPassThroughOrigContext1();

    state.callInfo = call_info;
    state.resource = resource;
    state.view = view;
    state.post = post;

    _run_command_list(command_list, &state);
    command_list_flush_state(&state);
}

void run_command_list(HackerDevice *hacker_device,
        HackerContext *hacker_context,
        CommandList *command_list,
        DrawCallInfo *call_info,
        bool post)
{
    ID3D11Resource **resource = NULL;
    if (call_info)
        resource = (ID3D11Resource**)call_info->indirect_buffer;

    run_command_list_complete(hacker_device, hacker_context, command_list,
            call_info, resource, NULL, post);
}

void run_resource_command_list(HackerDevice *hacker_device,
        HackerContext *hacker_context,
        CommandList *command_list,
        ID3D11Resource **resource,
        bool post)
{
    run_command_list_complete(hacker_device, hacker_context, command_list,
            NULL, resource, NULL, post);
}

void run_view_command_list(HackerDevice *hacker_device,
        HackerContext *hacker_context,
        CommandList *command_list,
        ID3D11View *view,
        bool post)
{
    ID3D11Resource *res = NULL;

    if (view)
        view->GetResource(&res);

    run_command_list_complete(hacker_device, hacker_context, command_list,
            NULL, &res, view, post);

    if (res)
        res->Release();
}

void optimise_command_lists(HackerDevice *device)
{
    bool making_progress;
    bool ignore_cto_pre, ignore_cto_post;
    size_t i;
    CommandList::Commands::iterator new_end;
    DWORD start;

    LOG_INFO("Optimising command lists...\n");
    start = GetTickCount();

    for (CommandList *command_list : registered_command_lists) {
        for (i = 0; i < command_list->commands.size(); i++)
            command_list->commands[i]->Optimise(device);
    }

    do {
        making_progress = false;
        ignore_cto_pre = true;
        ignore_cto_post = true;

        // If all TextureOverride sections have empty command lists of
        // either pre or post, we can treat checktextureoverride as a
        // Noop. This is intended to catch the case where we only have
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
            for (texture_override &to : tolkv.second) {
                ignore_cto_pre = ignore_cto_pre && to.command_list.commands.empty();
                ignore_cto_post = ignore_cto_post && to.post_command_list.commands.empty();
            }
        }
        for (auto &tof : G->mFuzzyTextureOverrides) {
            ignore_cto_pre = ignore_cto_pre && tof->textureOverride->command_list.commands.empty();
            ignore_cto_post = ignore_cto_post && tof->textureOverride->post_command_list.commands.empty();
        }

        // Go through each registered command list and remove any
        // commands that are noops to eliminate the runtime overhead of
        // processing these
        for (CommandList *command_list : registered_command_lists) {
            for (i = 0; i < command_list->commands.size(); ) {
                if (command_list->commands[i]->Noop(command_list->post, ignore_cto_pre, ignore_cto_post)) {
                    LOG_INFO("Optimised out %s %S\n",
                            command_list->post ? "post" : "pre",
                            command_list->commands[i]->iniLine.c_str());
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

    LOG_INFO("Command List Optimiser finished after %ums\n", GetTickCount() - start);
    registered_command_lists.clear();
    dynamically_allocated_command_lists.clear();
}

static bool add_command_to_list(CommandListCommand *command,
        CommandList *explicit_command_list,
        CommandList *sensible_command_list,
        CommandList *pre_command_list,
        CommandList *post_command_list,
        const wchar_t *section,
        const wchar_t *key, wstring *val)
{
    if (section && key) {
        command->iniLine = L"[" + wstring(section) + L"] " + wstring(key);
        if (val)
            command->iniLine += L" = " + *val;
    }

    if (explicit_command_list) {
        // User explicitly specified "pre" or "post", so only add the
        // command to that list
        explicit_command_list->commands.push_back(std::shared_ptr<CommandListCommand>(command));
    } else if (sensible_command_list) {
        // User did not specify which command list to add it to, but
        // the command they specified has a sensible default, so add it
        // to that list:
        sensible_command_list->commands.push_back(std::shared_ptr<CommandListCommand>(command));
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
        pre_command_list->commands.push_back(p);
        if (post_command_list)
            post_command_list->commands.push_back(p);
    }

    return true;
}

static bool parse_check_texture_override(const wchar_t *section,
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
            operation->runPreAndPostTogether = true;
        else if (post_command_list)
            G->implicit_post_checktextureoverride_used = true;

        return add_command_to_list(operation, explicit_command_list, NULL, pre_command_list, post_command_list, section, key, val);
    }

    delete operation;
    return false;
}

static bool parse_reset_per_frame_limits(const wchar_t *section,
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

        res = custom_resources.end();
        if (get_namespaced_section_name_lower(&resource_id, ini_namespace, &namespaced_section))
            res = custom_resources.find(namespaced_section);
        if (res == custom_resources.end())
            res = custom_resources.find(resource_id);
        if (res == custom_resources.end())
            goto bail;

        operation->resource = &res->second;
    }

    if (!wcsncmp(val->c_str(), L"customshader", 12) || !wcsncmp(val->c_str(), L"builtincustomshader", 19)) {
        wstring shader_id(val->c_str());

        shader = custom_shaders.end();
        if (get_namespaced_section_name_lower(&shader_id, ini_namespace, &namespaced_section))
            shader = custom_shaders.find(namespaced_section);
        if (shader == custom_shaders.end())
            shader = custom_shaders.find(shader_id);
        if (shader == custom_shaders.end())
            goto bail;

        operation->shader = &shader->second;
    }

    return add_command_to_list(operation, explicit_command_list, pre_command_list, NULL, NULL, section, key, val);

bail:
    delete operation;
    return false;
}

static bool parse_clear_view(const wchar_t *section,
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

    ClearViewCommand *operation = new ClearViewCommand();

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
                operation->clearUavUint = true;
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
            operation->clearUavUint = true;
            continue;
        }
        if (!wcscmp(token.c_str(), L"depth")) {
            operation->clearDepth = true;
            continue;
        }
        if (!wcscmp(token.c_str(), L"stencil")) {
            operation->clearStencil = true;
            continue;
        }

        goto bail;
    }

    if (operation->target.type == ResourceCopyTargetType::INVALID)
        goto bail;

    // Use the first value specified as the depth value when clearing a
    // DSV, and the second as the stencil value, unless we are only
    // clearing the stencil side, in which case use the first:
    operation->dsvDepth = operation->fval[0];
    operation->dsvStencil = operation->uval[1];
    if (operation->clearStencil && !operation->clearDepth)
        operation->dsvStencil = operation->uval[0];

    // Propagate the final specified value to the remaining channels. This
    // allows a single value to be specified to clear all channels in RTVs
    // and UAVs. Note that this is done after noting the DSV values because
    // we never want to propagate the depth value to the stencil value:
    for (idx = max(1, idx); idx < 4; idx++) {
        operation->uval[idx] = operation->uval[idx - 1];
        operation->fval[idx] = operation->fval[idx - 1];
    }

    return add_command_to_list(operation, explicit_command_list, pre_command_list, NULL, NULL, section, key, val);

bail:
    delete operation;
    return false;
}


static bool parse_run_shader(const wchar_t *section,
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

    shader = custom_shaders.end();
    if (get_namespaced_section_name_lower(&shader_id, ini_namespace, &namespaced_section))
        shader = custom_shaders.find(namespaced_section);
    if (shader == custom_shaders.end())
        shader = custom_shaders.find(shader_id);
    if (shader == custom_shaders.end())
        goto bail;

    operation->customShader = &shader->second;
    return add_command_to_list(operation, explicit_command_list, pre_command_list, NULL, NULL, section, key, val);

bail:
    delete operation;
    return false;
}

bool parse_run_explicit_command_list(const wchar_t *section,
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

    shader = explicit_command_list_sections.end();
    if (get_namespaced_section_name_lower(&section_id, ini_namespace, &namespaced_section))
        shader = explicit_command_list_sections.find(namespaced_section);
    if (shader == explicit_command_list_sections.end())
        shader = explicit_command_list_sections.find(section_id);
    if (shader == explicit_command_list_sections.end())
        goto bail;

    // If the user indicated an explicit command list we will run the pre
    // and post lists of the target list together. This tends to make
    // things a little less surprising for "post run = CommandListFoo"
    if (explicit_command_list)
        operation->runPreAndPostTogether = true;

    operation->commandListSection = &shader->second;
    // This function is nearly identical to parse_run_shader, but in case we
    // later refactor these together note that here we do not specify a
    // sensible command list, so it will be added to both pre and post
    // command lists:
    return add_command_to_list(operation, explicit_command_list, NULL, pre_command_list, post_command_list, section, key, val);

bail:
    delete operation;
    return false;
}

static bool parse_preset(const wchar_t *section,
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
    i = preset_overrides.end();
    // First, add the 'Preset' (i.e. the user did not) and try namespaced:
    if (get_namespaced_section_name_lower(&prefixed_section, ini_namespace, &namespaced_section))
        i = preset_overrides.find(namespaced_section);
    // Second, try namespaced without adding the prefix:
    if (i == preset_overrides.end()) {
        if (get_namespaced_section_name_lower(&preset_id, ini_namespace, &namespaced_section))
            i = preset_overrides.find(namespaced_section);
    }
    // Third, add the 'Preset' and try global:
    if (i == preset_overrides.end())
        i = preset_overrides.find(prefixed_section);
    // Finally, don't add the prefix and try global:
    if (i == preset_overrides.end())
        i = preset_overrides.find(preset_id);
    if (i == preset_overrides.end())
        goto bail;

    operation->preset = &i->second;
    operation->exclude = exclude;

    return add_command_to_list(operation, explicit_command_list, pre_command_list, NULL, NULL, section, key, val);

bail:
    delete operation;
    return false;
}

static bool parse_draw_command_args(wstring *val, DrawCommand *operation, bool indirect, int nargs, const wstring *ini_namespace, CommandListScope *scope)
{
    size_t start = 0, end;
    wstring sub;
    int i;

    if (indirect) {
        end = val->find(L',', start);
        if (end == wstring::npos)
            return false;

        sub = val->substr(start, end);
        if (!operation->indirectBuffer.ParseTarget(sub.c_str(), true, ini_namespace))
            return false;

        if (operation->indirectBuffer.type == ResourceCopyTargetType::CUSTOM_RESOURCE) {
            // Fucking C++ making this line 3x longer than it should be:
            operation->indirectBuffer.customResource->miscFlags = (D3D11_RESOURCE_MISC_FLAG)
                (operation->indirectBuffer.customResource->miscFlags
                 | D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS);
        }

        start = end + 1;
    }

    for (i = 0; i < nargs; i++) {
        end = val->find(L',', start);

        sub = val->substr(start, end - start);
        if (!operation->args[i].Parse(&sub, ini_namespace, scope))
            return false;

        if (end == wstring::npos)
            return (i+1 == nargs);
        start = end + 1;
    }

    return false;
}

static bool parse_draw_command(const wchar_t *section,
        const wchar_t *key, wstring *val,
        CommandList *explicit_command_list,
        CommandList *pre_command_list,
        CommandList *post_command_list,
        const wstring *ini_namespace)
{
    DrawCommand *operation = new DrawCommand();
    bool ok = true;

    if (!wcscmp(key, L"draw")) {
        if (!wcscmp(val->c_str(), L"from_caller")) {
            operation->type = DrawCommandType::FROM_CALLER;
        } else {
            operation->type = DrawCommandType::DRAW;
            ok = parse_draw_command_args(val, operation, false, 2, ini_namespace, pre_command_list->scope);
        }
    } else if (!wcscmp(key, L"drawauto")) {
        operation->type = DrawCommandType::DRAW_AUTO;
    } else if (!wcscmp(key, L"drawindexed")) {
        if (!wcscmp(val->c_str(), L"auto")) {
            operation->type = DrawCommandType::AUTO_INDEX_COUNT;
        } else {
            operation->type = DrawCommandType::DRAW_INDEXED;
            ok = parse_draw_command_args(val, operation, false, 3, ini_namespace, pre_command_list->scope);
        }
    } else if (!wcscmp(key, L"drawindexedinstanced")) {
        operation->type = DrawCommandType::DRAW_INDEXED_INSTANCED;
        ok = parse_draw_command_args(val, operation, false, 5, ini_namespace, pre_command_list->scope);
    } else if (!wcscmp(key, L"drawinstanced")) {
        operation->type = DrawCommandType::DRAW_INSTANCED;
        ok = parse_draw_command_args(val, operation, false, 4, ini_namespace, pre_command_list->scope);
    } else if (!wcscmp(key, L"dispatch")) {
        operation->type = DrawCommandType::DISPATCH;
        ok = parse_draw_command_args(val, operation, false, 3, ini_namespace, pre_command_list->scope);
    } else if (!wcscmp(key, L"drawindexedinstancedindirect")) {
        operation->type = DrawCommandType::DRAW_INDEXED_INSTANCED_INDIRECT;
        ok = parse_draw_command_args(val, operation, true, 1, ini_namespace, pre_command_list->scope);
    } else if (!wcscmp(key, L"drawinstancedindirect")) {
        operation->type = DrawCommandType::DRAW_INSTANCED_INDIRECT;
        ok = parse_draw_command_args(val, operation, true, 1, ini_namespace, pre_command_list->scope);
    } else if (!wcscmp(key, L"dispatchindirect")) {
        operation->type = DrawCommandType::DISPATCH_INDIRECT;
        ok = parse_draw_command_args(val, operation, true, 1, ini_namespace, pre_command_list->scope);
    }

    if (operation->type == DrawCommandType::INVALID || !ok)
        goto bail;

    operation->iniSection = section;
    return add_command_to_list(operation, explicit_command_list, pre_command_list, NULL, NULL, section, key, val);

bail:
    delete operation;
    return false;
}

static bool parse_direct_mode_set_active_eye_command(const wchar_t *section,
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
    return add_command_to_list(operation, explicit_command_list, pre_command_list, NULL, NULL, section, key, val);

bail:
    delete operation;
    return false;
}

static bool parse_per_draw_stereo_override(const wchar_t *section,
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
    if (operation->stagingOp.src.ParseTarget(val->c_str(), true, ini_namespace)) {
        operation->stagingType = true;
        goto success;
    }

    // The scope is shared between pre & post, we use pre here since it is never NULL
    if (!operation->expression.Parse(val, ini_namespace, pre_command_list->scope))
        goto bail;

success:
    // Add to both command lists by default - the pre command list will set
    // the value, and the post command list will restore the original. If
    // an explicit command list is specified then the value will only be
    // set, not restored (regardless of whether that is pre or post)
    return add_command_to_list(operation, explicit_command_list, NULL, pre_command_list, post_command_list, section, key, val);

bail:
    delete operation;
    return false;
}

static bool parse_frame_analysis_dump(const wchar_t *section,
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

    operation->analyseOptions = parse_enum_option_string<wchar_t *, FrameAnalysisOptions>
        (FrameAnalysisOptionNames, buf, &target);

    if (!target)
        goto bail;

    if (!operation->target.ParseTarget(target, true, ini_namespace))
        goto bail;

    operation->targetName = L"[" + wstring(section) + L"]-" + wstring(target);
    // target_name will be used in the filenames, so replace any reserved characters:
    std::replace(operation->targetName.begin(), operation->targetName.end(), L'<', L'_');
    std::replace(operation->targetName.begin(), operation->targetName.end(), L'>', L'_');
    std::replace(operation->targetName.begin(), operation->targetName.end(), L':', L'_');
    std::replace(operation->targetName.begin(), operation->targetName.end(), L'"', L'_');
    std::replace(operation->targetName.begin(), operation->targetName.end(), L'/', L'_');
    std::replace(operation->targetName.begin(), operation->targetName.end(), L'\\',L'_');
    std::replace(operation->targetName.begin(), operation->targetName.end(), L'|', L'_');
    std::replace(operation->targetName.begin(), operation->targetName.end(), L'?', L'_');
    std::replace(operation->targetName.begin(), operation->targetName.end(), L'*', L'_');

    delete [] buf;
    return add_command_to_list(operation, explicit_command_list, pre_command_list, NULL, NULL, section, key, val);

bail:
    delete [] buf;
    delete operation;
    return false;
}

bool parse_command_list_general_commands(const wchar_t *section,
        const wchar_t *key, wstring *val,
        CommandList *explicit_command_list,
        CommandList *pre_command_list, CommandList *post_command_list,
        const wstring *ini_namespace)
{
    if (!wcscmp(key, L"checktextureoverride"))
        return parse_check_texture_override(section, key, val, explicit_command_list, pre_command_list, post_command_list, ini_namespace);

    if (!wcscmp(key, L"run")) {
        if (!wcsncmp(val->c_str(), L"customshader", 12) || !wcsncmp(val->c_str(), L"builtincustomshader", 19))
            return parse_run_shader(section, key, val, explicit_command_list, pre_command_list, post_command_list, ini_namespace);

        if (!wcsncmp(val->c_str(), L"commandlist", 11) || !wcsncmp(val->c_str(), L"builtincommandlist", 18))
            return parse_run_explicit_command_list(section, key, val, explicit_command_list, pre_command_list, post_command_list, ini_namespace);
    }

    if (!wcscmp(key, L"preset"))
        return parse_preset(section, key, val, explicit_command_list, pre_command_list, post_command_list, false, ini_namespace);
    if (!wcscmp(key, L"exclude_preset"))
        return parse_preset(section, key, val, explicit_command_list, pre_command_list, post_command_list, true, ini_namespace);

    if (!wcscmp(key, L"handling")) {
        // skip only makes sense in pre command lists, since it needs
        // to run before the original draw call:
        if (!wcscmp(val->c_str(), L"skip"))
            return add_command_to_list(new SkipCommand(section), explicit_command_list, pre_command_list, NULL, NULL, section, key, val);

        // abort defaults to both command lists, to abort command list
        // execution both before and after the draw call:
        if (!wcscmp(val->c_str(), L"abort"))
            return add_command_to_list(new AbortCommand(section), explicit_command_list, NULL, pre_command_list, post_command_list, section, key, val);
    }

    if (!wcscmp(key, L"reset_per_frame_limits"))
        return parse_reset_per_frame_limits(section, key, val, explicit_command_list, pre_command_list, post_command_list, ini_namespace);

    if (!wcscmp(key, L"clear"))
        return parse_clear_view(section, key, val, explicit_command_list, pre_command_list, post_command_list, ini_namespace);

    if (!wcscmp(key, L"separation"))
        return parse_per_draw_stereo_override(section, key, val, explicit_command_list, pre_command_list, post_command_list, true, ini_namespace);

    if (!wcscmp(key, L"convergence"))
        return parse_per_draw_stereo_override(section, key, val, explicit_command_list, pre_command_list, post_command_list, false, ini_namespace);

    if (!wcscmp(key, L"direct_mode_eye"))
        return parse_direct_mode_set_active_eye_command(section, key, val, explicit_command_list, pre_command_list, post_command_list);

    if (!wcscmp(key, L"analyse_options"))
        return add_command_to_list(new FrameAnalysisChangeOptionsCommand(val), explicit_command_list, pre_command_list, NULL, NULL, section, key, val);

    if (!wcscmp(key, L"dump"))
        return parse_frame_analysis_dump(section, key, val, explicit_command_list, pre_command_list, post_command_list, ini_namespace);

    if (!wcscmp(key, L"special")) {
        if (!wcscmp(val->c_str(), L"upscaling_switch_bb"))
            return add_command_to_list(new UpscalingFlipBBCommand(section), explicit_command_list, pre_command_list, NULL, NULL, section, key, val);

        if (!wcscmp(val->c_str(), L"draw_3dmigoto_overlay"))
            return add_command_to_list(new Draw3DMigotoOverlayCommand(section), explicit_command_list, pre_command_list, NULL, NULL, section, key, val);
    }

    return parse_draw_command(section, key, val, explicit_command_list, pre_command_list, post_command_list, ini_namespace);
}

void CheckTextureOverrideCommand::Run(CommandListState *state)
{
    TextureOverrideMatches matches;
    ResourceCopyTarget *saved_this = NULL;
    bool saved_post;
    unsigned i;

    COMMAND_LIST_LOG(state, "%S\n", iniLine.c_str());

    target.FindTextureOverrides(state, NULL, &matches);

    saved_this = state->thisTarget;
    state->thisTarget = &target;
    if (runPreAndPostTogether) {
        saved_post = state->post;
        state->post = false;
        for (i = 0; i < matches.size(); i++)
            _run_command_list(&matches[i]->command_list, state);
        state->post = true;
        for (i = 0; i < matches.size(); i++)
            _run_command_list(&matches[i]->post_command_list, state);
        state->post = saved_post;
    } else {
        for (i = 0; i < matches.size(); i++) {
            if (state->post)
                _run_command_list(&matches[i]->post_command_list, state);
            else
                _run_command_list(&matches[i]->command_list, state);
        }
    }
    state->thisTarget = saved_this;
}

bool CheckTextureOverrideCommand::Noop(bool post, bool ignore_cto_pre, bool ignore_cto_post)
{
    if (runPreAndPostTogether)
        return (ignore_cto_pre && ignore_cto_post);

    if (post)
        return ignore_cto_post;
    return ignore_cto_pre;
}

ClearViewCommand::ClearViewCommand() :
    dsvDepth(0.0),
    dsvStencil(0),
    clearDepth(false),
    clearStencil(false),
    clearUavUint(false)
{
    memset(fval, 0, sizeof(fval));
    memset(uval, 0, sizeof(uval));
}

static bool uav_supports_float_clear(ID3D11UnorderedAccessView *uav)
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

void ResetPerFrameLimitsCommand::Run(CommandListState *state)
{
    COMMAND_LIST_LOG(state, "%S\n", iniLine.c_str());

    if (shader)
        shader->executionsThisFrame = 0;

    if (resource)
        resource->copiesThisFrame = 0;
}

void PresetCommand::Run(CommandListState *state)
{
    COMMAND_LIST_LOG(state, "%S\n", iniLine.c_str());

    if (exclude)
        preset->Exclude();
    else
        preset->Trigger(this);
}

static UINT get_index_count_from_current_ib(ID3D11DeviceContext *orig_context1)
{
    ID3D11Buffer *ib;
    D3D11_BUFFER_DESC desc;
    DXGI_FORMAT format;
    UINT offset;

    orig_context1->IAGetIndexBuffer(&ib, &format, &offset);
    if (!ib)
        return 0;

    ib->GetDesc(&desc);
    ib->Release();

    switch(format) {
        case DXGI_FORMAT_R16_UINT:
            return (desc.ByteWidth - offset) / 2;
        case DXGI_FORMAT_R32_UINT:
            return (desc.ByteWidth - offset) / 4;
    }

    return 0;
}

void DrawCommand::DoIndirectDrawCall(CommandListState *state, char *name,
        void (__stdcall ID3D11DeviceContext::*IndirectDrawCall)(THIS_
        ID3D11Buffer *pBufferForArgs,
        UINT aligned_byte_offset_for_args))
{
    ID3D11Resource *resource = NULL;
    ID3D11View *view = NULL;
    UINT stride = 0;
    UINT offset = 0;
    UINT buf_size = 0;
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    UINT arg = (UINT)args[0].Evaluate(state);

    resource = indirectBuffer.GetResource(state, &view, &stride, &offset, &format, NULL);
    if (view)
        view->Release();

    if (!resource) {
        COMMAND_LIST_LOG(state, "[%S] %s(%p, %u) -> INDIRECT BUFFER IS NULL\n",
                iniSection.c_str(), name, resource, arg);
        return;
    }

    COMMAND_LIST_LOG(state, "[%S] %s(%p, %u)\n", iniSection.c_str(), name, resource, arg);

    (state->origContext1->*IndirectDrawCall)((ID3D11Buffer*)resource, arg);

    resource->Release();
}

void DrawCommand::EvalArgs(int nargs, INT result[5], CommandListState *state)
{
    for (int i = 0; i < nargs; i++)
        result[i] = (INT)args[i].Evaluate(state);
}

void DrawCommand::Run(CommandListState *state)
{
    HackerContext *hacker_context = state->hackerContext;
    ID3D11DeviceContext *orig_context1 = state->origContext1;
    DrawCallInfo *info = state->callInfo;
    UINT auto_count = 0;
    INT eargs[5];

    // If this command list was triggered from something currently skipped
    // due to hunting, we also skip any custom draw calls, so that if we
    // are replacing the original draw call we will still be able to see
    // the object being hunted.
    if (info && info->hunting_skip) {
        COMMAND_LIST_LOG(state, "[%S] Draw -> SKIPPED DUE TO HUNTING\n", iniSection.c_str());
        return;
    }

    // Ensure IniParams are visible:
    command_list_flush_state(state);

    Profiling::injected_draw_calls++;

    switch (type) {
        case DrawCommandType::DRAW:
            EvalArgs(2, eargs, state);
            COMMAND_LIST_LOG(state, "[%S] Draw(%u, %u)\n", iniSection.c_str(), eargs[0], eargs[1]);
            orig_context1->Draw(eargs[0], eargs[1]);
            break;
        case DrawCommandType::DRAW_AUTO:
            COMMAND_LIST_LOG(state, "[%S] DrawAuto()\n", iniSection.c_str());
            orig_context1->DrawAuto();
            break;
        case DrawCommandType::DRAW_INDEXED:
            EvalArgs(3, eargs, state);
            COMMAND_LIST_LOG(state, "[%S] DrawIndexed(%u, %u, %i)\n", iniSection.c_str(), eargs[0], eargs[1], (INT)eargs[2]);
            orig_context1->DrawIndexed(eargs[0], eargs[1], (INT)eargs[2]);
            break;
        case DrawCommandType::DRAW_INDEXED_INSTANCED:
            EvalArgs(5, eargs, state);
            COMMAND_LIST_LOG(state, "[%S] DrawIndexedInstanced(%u, %u, %u, %i, %u)\n", iniSection.c_str(), eargs[0], eargs[1], eargs[2], (INT)eargs[3], eargs[4]);
            orig_context1->DrawIndexedInstanced(eargs[0], eargs[1], eargs[2], (INT)eargs[3], eargs[4]);
            break;
        case DrawCommandType::DRAW_INSTANCED:
            EvalArgs(4, eargs, state);
            COMMAND_LIST_LOG(state, "[%S] DrawInstanced(%u, %u, %u, %u)\n", iniSection.c_str(), eargs[0], eargs[1], eargs[2], eargs[3]);
            orig_context1->DrawInstanced(eargs[0], eargs[1], eargs[2], eargs[3]);
            break;
        case DrawCommandType::DISPATCH:
            EvalArgs(3, eargs, state);
            COMMAND_LIST_LOG(state, "[%S] Dispatch(%u, %u, %u)\n", iniSection.c_str(), eargs[0], eargs[1], eargs[2]);
            orig_context1->Dispatch(eargs[0], eargs[1], eargs[2]);
            break;
        case DrawCommandType::DRAW_INDEXED_INSTANCED_INDIRECT:
            DoIndirectDrawCall(state, "DrawIndexedInstancedIndirect", &ID3D11DeviceContext::DrawIndexedInstancedIndirect);
            break;
        case DrawCommandType::DRAW_INSTANCED_INDIRECT:
            DoIndirectDrawCall(state, "DrawInstancedIndirect", &ID3D11DeviceContext::DrawInstancedIndirect);
            break;
        case DrawCommandType::DISPATCH_INDIRECT:
            DoIndirectDrawCall(state, "DispatchIndirect", &ID3D11DeviceContext::DispatchIndirect);
            break;
        case DrawCommandType::FROM_CALLER:
            if (!info) {
                COMMAND_LIST_LOG(state, "[%S] Draw = from_caller -> NO ACTIVE DRAW CALL\n", iniSection.c_str());
                break;
            }
            switch (info->type) {
                case DrawCall::DrawIndexedInstanced:
                    COMMAND_LIST_LOG(state, "[%S] Draw = from_caller -> DrawIndexedInstanced(%u, %u, %u, %i, %u)\n", iniSection.c_str(), info->IndexCount, info->InstanceCount, info->FirstIndex, info->FirstVertex, info->FirstInstance);
                    orig_context1->DrawIndexedInstanced(info->IndexCount, info->InstanceCount, info->FirstIndex, info->FirstVertex, info->FirstInstance);
                    break;
                case DrawCall::DrawInstanced:
                    COMMAND_LIST_LOG(state, "[%S] Draw = from_caller -> DrawInstanced(%u, %u, %u, %u)\n", iniSection.c_str(), info->VertexCount, info->InstanceCount, info->FirstVertex, info->FirstInstance);
                    orig_context1->DrawInstanced(info->VertexCount, info->InstanceCount, info->FirstVertex, info->FirstInstance);
                    break;
                case DrawCall::DrawIndexed:
                    COMMAND_LIST_LOG(state, "[%S] Draw = from_caller -> DrawIndexed(%u, %u, %i)\n", iniSection.c_str(), info->IndexCount, info->FirstIndex, info->FirstVertex);
                    orig_context1->DrawIndexed(info->IndexCount, info->FirstIndex, info->FirstVertex);
                    break;
                case DrawCall::Draw:
                    COMMAND_LIST_LOG(state, "[%S] Draw = from_caller -> Draw(%u, %u)\n", iniSection.c_str(), info->VertexCount, info->FirstVertex);
                    orig_context1->Draw(info->VertexCount, info->FirstVertex);
                    break;
                case DrawCall::DrawInstancedIndirect:
                    if (!info->indirect_buffer) {
                        LogOverlay(LOG_DIRE, "BUG: draw = from_caller -> DrawInstancedIndirect missing args\n");
                        break;
                    }
                    COMMAND_LIST_LOG(state, "[%S] Draw = from_caller -> DrawInstancedIndirect(0x%p, %u)\n", iniSection.c_str(), *info->indirect_buffer, info->args_offset);
                    orig_context1->DrawInstancedIndirect(*info->indirect_buffer, info->args_offset);
                    break;
                case DrawCall::DrawIndexedInstancedIndirect:
                    if (!info->indirect_buffer) {
                        LogOverlay(LOG_DIRE, "BUG: draw = from_caller -> DrawIndexedInstancedIndirect missing args\n");
                        break;
                    }
                    COMMAND_LIST_LOG(state, "[%S] Draw = from_caller -> DrawIndexedInstancedIndirect(0x%p, %u)\n", iniSection.c_str(), *info->indirect_buffer, info->args_offset);
                    orig_context1->DrawIndexedInstancedIndirect(*info->indirect_buffer, info->args_offset);
                    break;
                case DrawCall::DispatchIndirect:
                    if (!info->indirect_buffer) {
                        LogOverlay(LOG_DIRE, "BUG: draw = from_caller -> DispatchIndirect missing args\n");
                        break;
                    }
                    COMMAND_LIST_LOG(state, "[%S] Draw = from_caller -> DispatchIndirect(0x%p, %u)\n", iniSection.c_str(), *info->indirect_buffer, info->args_offset);
                    orig_context1->DispatchIndirect(*info->indirect_buffer, info->args_offset);
                    break;
                case DrawCall::Dispatch:
                    COMMAND_LIST_LOG(state, "[%S] Draw = from_caller -> Dispatch(%u, %u, %u)\n", iniSection.c_str(), info->ThreadGroupCountX, info->ThreadGroupCountY, info->ThreadGroupCountZ);
                    orig_context1->Dispatch(info->ThreadGroupCountX, info->ThreadGroupCountY, info->ThreadGroupCountZ);
                    break;
                case DrawCall::DrawAuto:
                    COMMAND_LIST_LOG(state, "[%S] Draw = from_caller -> DrawAuto()\n", iniSection.c_str());
                    orig_context1->DrawAuto();
                    break;
                default:
                    LogOverlay(LOG_DIRE, "BUG: draw = from_caller -> unknown draw call type\n");
                    break;
            }
            break;
        case DrawCommandType::AUTO_INDEX_COUNT:
            auto_count = get_index_count_from_current_ib(orig_context1);
            COMMAND_LIST_LOG(state, "[%S] drawindexed = auto -> DrawIndexed(%u, 0, 0)\n", iniSection.c_str(), auto_count);
            if (auto_count)
                orig_context1->DrawIndexed(auto_count, 0, 0);
            else
                COMMAND_LIST_LOG(state, "  Unable to determine index count\n");
            break;
    }
}

void SkipCommand::Run(CommandListState *state)
{
    COMMAND_LIST_LOG(state, "[%S] handling = skip\n", iniSection.c_str());

    if (state->callInfo)
        state->callInfo->skip = true;
    else
        COMMAND_LIST_LOG(state, "  No active draw call to skip\n");
}

void AbortCommand::Run(CommandListState *state)
{
    COMMAND_LIST_LOG(state, "[%S] handling = abort\n", iniSection.c_str());

    state->aborted = true;
}

PerDrawStereoOverrideCommand::PerDrawStereoOverrideCommand(bool restore_on_post) :
        stagingType(false),
        val(FLT_MAX),
        saved(FLT_MAX),
        restoreOnPost(restore_on_post),
        didSetValueOnPre(false)
{}

bool PerDrawStereoOverrideCommand::UpdateVal(CommandListState *state)
{
    D3D11_MAPPED_SUBRESOURCE mapping;
    HRESULT hr;
    float tmp;
    bool ret = false;

    if (!stagingType)
        return true;

    if (stagingOp.staging) {
        hr = stagingOp.Map(state, &mapping);
        if (FAILED(hr)) {
            if (hr == DXGI_ERROR_WAS_STILL_DRAWING)
                COMMAND_LIST_LOG(state, "  Transfer in progress...\n");
            else
                COMMAND_LIST_LOG(state, "  Unknown error: 0x%x\n", hr);
            return false;
        }

        // FIXME: Check if resource is at least 4 bytes (maybe we can
        // use RowPitch, but MSDN contradicts itself so I'm not sure.
        // Otherwise we can refer to the resource description)
        tmp = ((float*)mapping.pData)[0];
        stagingOp.Unmap(state);

        if (isnan(tmp)) {
            COMMAND_LIST_LOG(state, "  Disregarding NAN\n");
        } else {
            val = tmp;
            ret = true;
        }

        // To make auto-convergence as responsive as possible, we start
        // the next transfer as soon as we have retrieved the value
        // from the previous transfer. This should minimise the number
        // of frames displayed with wrong convergence on scene changes.
    }

    stagingOp.staging = true;
    stagingOp.Run(state);
    return ret;
}

void PerDrawStereoOverrideCommand::Run(CommandListState *state)
{
    COMMAND_LIST_LOG(state, "%S\n", iniLine.c_str());

    if (!state->hackerDevice->stereoHandle) {
        COMMAND_LIST_LOG(state, "  No Stereo Handle\n");
        return;
    }

    if (restoreOnPost) {
        if (state->post) {
            if (!didSetValueOnPre)
                return;
            didSetValueOnPre = false;

            COMMAND_LIST_LOG(state, "  Restoring %s = %f\n", StereoParamName(), saved);
            SetStereoValue(state, saved);
        } else {
            if (stagingType) {
                if (!(didSetValueOnPre = UpdateVal(state)))
                    return;
            } else {
                val = expression.Evaluate(state);
                didSetValueOnPre = true;
            }

            saved = GetStereoValue(state);

            COMMAND_LIST_LOG(state, "  Setting per-draw call %s = %f * %f = %f\n",
                    StereoParamName(), val, saved, val * saved);

            // The original ShaderOverride code multiplied the new
            // separation and convergence by the old ones, so I'm
            // doing that as well, but while that makes sense for
            // separation, I'm not really convinced it makes sense
            // for convergence. Still, the convergence override is
            // generally only useful to use convergence=0 to move
            // something to infinity, and in that case it won't
            // matter.
            SetStereoValue(state, val * saved);
        }
    } else {
        if (stagingType) {
            if (!UpdateVal(state))
                return;
        } else
            val = expression.Evaluate(state);

        COMMAND_LIST_LOG(state, "  Setting %s = %f\n", StereoParamName(), val);
        SetStereoValue(state, val);
    }
}

bool PerDrawStereoOverrideCommand::Optimise(HackerDevice *device)
{
    if (stagingType)
        return false;
    return expression.Optimise(device);
}

bool PerDrawStereoOverrideCommand::Noop(bool post, bool ignore_cto_pre, bool ignore_cto_post)
{
    NvU8 enabled = false;

    nvapi_override();
    Profiling::NvAPI_Stereo_IsEnabled(&enabled);
    return !enabled;
}

void DirectModeSetActiveEyeCommand::Run(CommandListState *state)
{
    COMMAND_LIST_LOG(state, "%S\n", iniLine.c_str());

    if (NVAPI_OK != Profiling::NvAPI_Stereo_SetActiveEye(state->hackerDevice->stereoHandle, eye))
        COMMAND_LIST_LOG(state, "  Stereo_SetActiveEye failed\n");
}

bool DirectModeSetActiveEyeCommand::Noop(bool post, bool ignore_cto_pre, bool ignore_cto_post)
{
    NvU8 enabled = false;

    nvapi_override();
    Profiling::NvAPI_Stereo_IsEnabled(&enabled);
    return !enabled;

    // FIXME: Should also return false if direct mode is disabled...
    // if only nvapi provided a GetDriverMode() API to determine that
}

float PerDrawSeparationOverrideCommand::GetStereoValue(CommandListState *state)
{
    float ret = 0.0f;

    if (NVAPI_OK != Profiling::NvAPI_Stereo_GetSeparation(state->hackerDevice->stereoHandle, &ret))
        COMMAND_LIST_LOG(state, "  Stereo_GetSeparation failed\n");

    return ret;
}

void PerDrawSeparationOverrideCommand::SetStereoValue(CommandListState *state, float val)
{
    nvapi_override();
    if (NVAPI_OK != Profiling::NvAPI_Stereo_SetSeparation(state->hackerDevice->stereoHandle, val))
        COMMAND_LIST_LOG(state, "  Stereo_SetSeparation failed\n");
}

float PerDrawConvergenceOverrideCommand::GetStereoValue(CommandListState *state)
{
    float ret = 0.0f;

    if (NVAPI_OK != Profiling::NvAPI_Stereo_GetConvergence(state->hackerDevice->stereoHandle, &ret))
        COMMAND_LIST_LOG(state, "  Stereo_GetConvergence failed\n");

    return ret;
}

void PerDrawConvergenceOverrideCommand::SetStereoValue(CommandListState *state, float val)
{
    nvapi_override();
    if (NVAPI_OK != Profiling::NvAPI_Stereo_SetConvergence(state->hackerDevice->stereoHandle, val))
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

    analyseOptions = parse_enum_option_string<wchar_t *, FrameAnalysisOptions>
        (FrameAnalysisOptionNames, buf, NULL);

    delete [] buf;
}

void FrameAnalysisChangeOptionsCommand::Run(CommandListState *state)
{
    COMMAND_LIST_LOG(state, "%S\n", iniLine.c_str());

    state->hackerContext->FrameAnalysisTrigger(analyseOptions);
}

bool FrameAnalysisChangeOptionsCommand::Noop(bool post, bool ignore_cto_pre, bool ignore_cto_post)
{
    return (G->hunting == HUNTING_MODE_DISABLED || G->frame_analysis_registered == false);
}

static void fill_in_missing_info(ResourceCopyTargetType type, ID3D11Resource *resource, ID3D11View *view,
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

void FrameAnalysisDumpCommand::Run(CommandListState *state)
{
    ID3D11Resource *resource = NULL;
    ID3D11View *view = NULL;
    UINT stride = 0;
    UINT offset = 0;
    UINT buf_size = 0;
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;

    // Fast exit if frame analysis is currently inactive:
    if (!G->analyse_frame)
        return;

    COMMAND_LIST_LOG(state, "%S\n", iniLine.c_str());

    resource = target.GetResource(state, &view, &stride, &offset, &format, NULL);
    if (!resource) {
        COMMAND_LIST_LOG(state, "  No resource to dump\n");
        return;
    }

    // Fill in any missing info before handing it to frame analysis. The
    // format is particularly important to try to avoid saving TYPELESS
    // resources:
    fill_in_missing_info(target.type, resource, view, &stride, &offset, &buf_size, &format);

    state->hackerContext->FrameAnalysisDump(resource, analyseOptions, targetName.c_str(), format, stride, offset);

    if (resource)
        resource->Release();
    if (view)
        view->Release();
}

bool FrameAnalysisDumpCommand::Noop(bool post, bool ignore_cto_pre, bool ignore_cto_post)
{
    return (G->hunting == HUNTING_MODE_DISABLED || G->frame_analysis_registered == false);
}

UpscalingFlipBBCommand::UpscalingFlipBBCommand(wstring section) :
    iniSection(section)
{
    G->upscaling_command_list_using_explicit_bb_flip = true;
}

UpscalingFlipBBCommand::~UpscalingFlipBBCommand()
{
    G->upscaling_command_list_using_explicit_bb_flip = false;
}

void UpscalingFlipBBCommand::Run(CommandListState *state)
{
    COMMAND_LIST_LOG(state, "[%S] special = upscaling_switch_bb\n", iniSection.c_str());

    G->bb_is_upscaling_bb = false;
}

void Draw3DMigotoOverlayCommand::Run(CommandListState *state)
{
    COMMAND_LIST_LOG(state, "[%S] special = draw_3dmigoto_overlay\n", iniSection.c_str());

    HackerSwapChain *hacker_swap_chain = state->hackerDevice->GetHackerSwapChain();
    if (hacker_swap_chain->overlay) {
        hacker_swap_chain->overlay->DrawOverlay();
        G->suppress_overlay = true;
    }
}

CustomShader::CustomShader() :
    vsOverride(false), hsOverride(false), dsOverride(false),
    gsOverride(false), psOverride(false), csOverride(false),
    vs(NULL), hs(NULL), ds(NULL), gs(NULL), ps(NULL), cs(NULL),
    vsBytecode(NULL), hsBytecode(NULL), dsBytecode(NULL),
    gsBytecode(NULL), psBytecode(NULL), csBytecode(NULL),
    blendOverride(0), blendState(NULL),
    blendSampleMask(0xffffffff), blendSampleMaskMergeMask(0xffffffff),
    depthStencilOverride(0), depthStencilState(NULL),
    stencilRef(0), stencilRefMask(~0),
    rsOverride(0), rsState(NULL),
    topology(D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED),
    substantiated(false),
    maxExecutionsPerFrame(0),
    frameNo(0),
    executionsThisFrame(0),
    samplerOverride(0),
    samplerState(nullptr),
    compileFlags(D3DCompileFlags::OPTIMIZATION_LEVEL3)
{
    int i;

    for (i = 0; i < 4; i++) {
        blendFactor[i] = 1.0f;
        blendFactorMergeMask[i] = ~0;
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

    if (blendState)
        blendState->Release();
    if (depthStencilState)
        depthStencilState->Release();
    if (rsState)
        rsState->Release();

    if (vsBytecode)
        vsBytecode->Release();
    if (hsBytecode)
        hsBytecode->Release();
    if (dsBytecode)
        dsBytecode->Release();
    if (gsBytecode)
        gsBytecode->Release();
    if (psBytecode)
        psBytecode->Release();
    if (csBytecode)
        csBytecode->Release();
    if (samplerState)
        samplerState->Release();
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
        LOG_INFO("    Discarding stale cached shader: %S\n", cache_path);
        goto err_close;
    }

    filesize = GetFileSize(f_cache, 0);
    if (FAILED(D3DCreateBlob(filesize, ppBytecode))) {
        LOG_INFO("    D3DCreateBlob failed\n");
        goto err_close;
    }

    if (!ReadFile(f_cache, (*ppBytecode)->GetBufferPointer(), (DWORD)(*ppBytecode)->GetBufferSize(), &readsize, 0)
            || readsize != filesize) {
        LOG_INFO("    Error reading cached shader\n");
        goto err_free;
    }

    LOG_INFO("    Loaded cached shader: %S\n", cache_path);
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
static const D3D_SHADER_MACRO hs_macros[] = { "HULL_SHADER", "", NULL, NULL };
static const D3D_SHADER_MACRO ds_macros[] = { "DOMAIN_SHADER", "", NULL, NULL };
static const D3D_SHADER_MACRO gs_macros[] = { "GEOMETRY_SHADER", "", NULL, NULL };
static const D3D_SHADER_MACRO ps_macros[] = { "PIXEL_SHADER", "", NULL, NULL };
static const D3D_SHADER_MACRO cs_macros[] = { "COMPUTE_SHADER", "", NULL, NULL };

// This is similar to the other compile routines, but still distinct enough to
// get it's own function for now - TODO: Refactor out the common code
bool CustomShader::Compile(char type, wchar_t *filename, const wstring *wname, const wstring *namespace_path)
{
    wchar_t wpath[MAX_PATH], cache_path[MAX_PATH];
    char apath[MAX_PATH];
    HANDLE f;
    DWORD src_data_size, read_size;
    vector<char> src_data;
    HRESULT hr;
    char shader_model[7];
    ID3DBlob **ppBytecode = NULL;
    ID3DBlob *pErrorMsgs = NULL;
    const D3D_SHADER_MACRO *macros = NULL;
    bool found = false;
    FILETIME timestamp;

    LOG_INFO("  %cs=%S\n", type, filename);

    switch(type) {
        case 'v':
            ppBytecode = &vsBytecode;
            macros = vs_macros;
            vsOverride = true;
            break;
        case 'h':
            ppBytecode = &hsBytecode;
            macros = hs_macros;
            hsOverride = true;
            break;
        case 'd':
            ppBytecode = &dsBytecode;
            macros = ds_macros;
            dsOverride = true;
            break;
        case 'g':
            ppBytecode = &gsBytecode;
            macros = gs_macros;
            gsOverride = true;
            break;
        case 'p':
            ppBytecode = &psBytecode;
            macros = ps_macros;
            psOverride = true;
            break;
        case 'c':
            ppBytecode = &csBytecode;
            macros = cs_macros;
            csOverride = true;
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
        GetModuleFileName(migoto_handle, wpath, MAX_PATH);
        wcsrchr(wpath, L'\\')[1] = 0;
        wcscat(wpath, namespace_path->c_str());
        wcscat(wpath, filename);
        if (GetFileAttributes(wpath) != INVALID_FILE_ATTRIBUTES)
            found = true;
    }
    if (!found) {
        if (!GetModuleFileName(migoto_handle, wpath, MAX_PATH)) {
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
    _snprintf_s(shader_model, 7, 7, "%cs_5_0", type);

    // XXX: If we allow the compilation to be customised further (e.g. with
    // addition preprocessor defines), make the cache filename unique for
    // each possible combination
    wchar_t *ext = wcsrchr(wpath, L'.');
    if (ext > wcsrchr(wpath, L'\\'))
        swprintf_s(cache_path, MAX_PATH, L"%.*s.%S.%x.bin", (int)(ext - wpath), wpath, shader_model, (UINT)compileFlags);
    else
        swprintf_s(cache_path, MAX_PATH, L"%s.%S.%x.bin", wpath, shader_model, (UINT)compileFlags);

    GetFileTime(f, NULL, NULL, &timestamp);
    if (load_cached_shader(timestamp, cache_path, ppBytecode)) {
        CloseHandle(f);
        return false;
    }

    src_data_size = GetFileSize(f, 0);
    src_data.resize(src_data_size);

    if (!ReadFile(f, src_data.data(), src_data_size, &read_size, 0)
            || src_data_size != read_size) {
        LOG_INFO("    Error reading HLSL file\n");
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
    {
        MigotoIncludeHandler include_handler(apath);
        hr = D3DCompile(src_data.data(), src_data_size, apath, macros,
            G->recursive_include == -1 ? D3D_COMPILE_STANDARD_FILE_INCLUDE : &include_handler,
            "main", shader_model, (UINT)compileFlags, 0, ppBytecode, &pErrorMsgs);
    }

    if (pErrorMsgs) {
        LPVOID err_msg = pErrorMsgs->GetBufferPointer();
        SIZE_T err_size = pErrorMsgs->GetBufferSize();
        LOG_INFO("--------------------------------------------- BEGIN ---------------------------------------------\n");
        LogOverlay(LOG_NOTICE, "%*s\n", err_size, err_msg);
        LOG_INFO("---------------------------------------------- END ----------------------------------------------\n");
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
            LOG_INFO("    Storing compiled shader to %S\n", cache_path);
            fwrite((*ppBytecode)->GetBufferPointer(), 1, (*ppBytecode)->GetBufferSize(), fw);
            fclose(fw);

            set_file_last_write_time(cache_path, &timestamp);
        } else
            LOG_INFO("    Error writing compiled shader to %S\n", cache_path);
    }

    return false;
err_close:
    CloseHandle(f);
err:
    return true;
}

void CustomShader::Substantiate(ID3D11Device *orig_device)
{
    if (substantiated)
        return;
    substantiated = true;

    if (vsBytecode) {
        orig_device->CreateVertexShader(vsBytecode->GetBufferPointer(), vsBytecode->GetBufferSize(), NULL, &vs);
        cleanup_shader_maps(vs);
        vsBytecode->Release();
        vsBytecode = NULL;
    }
    if (hsBytecode) {
        orig_device->CreateHullShader(hsBytecode->GetBufferPointer(), hsBytecode->GetBufferSize(), NULL, &hs);
        cleanup_shader_maps(hs);
        hsBytecode->Release();
        hsBytecode = NULL;
    }
    if (dsBytecode) {
        orig_device->CreateDomainShader(dsBytecode->GetBufferPointer(), dsBytecode->GetBufferSize(), NULL, &ds);
        cleanup_shader_maps(ds);
        dsBytecode->Release();
        dsBytecode = NULL;
    }
    if (gsBytecode) {
        orig_device->CreateGeometryShader(gsBytecode->GetBufferPointer(), gsBytecode->GetBufferSize(), NULL, &gs);
        cleanup_shader_maps(gs);
        gsBytecode->Release();
        gsBytecode = NULL;
    }
    if (psBytecode) {
        orig_device->CreatePixelShader(psBytecode->GetBufferPointer(), psBytecode->GetBufferSize(), NULL, &ps);
        cleanup_shader_maps(ps);
        psBytecode->Release();
        psBytecode = NULL;
    }
    if (csBytecode) {
        orig_device->CreateComputeShader(csBytecode->GetBufferPointer(), csBytecode->GetBufferSize(), NULL, &cs);
        cleanup_shader_maps(cs);
        csBytecode->Release();
        csBytecode = NULL;
    }

    if (blendOverride == 1) // 2 will merge the blend state at draw time
        orig_device->CreateBlendState(&blendDesc, &blendState);

    if (depthStencilOverride == 1) // 2 will merge depth/stencil state at draw time
        orig_device->CreateDepthStencilState(&depthStencilDesc, &depthStencilState);

    if (rsOverride == 1) // 2 will merge rasterizer state at draw time
        orig_device->CreateRasterizerState(&rsDesc, &rsState);

    if (samplerOverride == 1)
        orig_device->CreateSamplerState(&samplerDesc, &samplerState);
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

void CustomShader::MergeBlendStates(ID3D11BlendState *src_state, FLOAT src_blend_factor[4], UINT src_sample_mask, ID3D11Device *orig_device)
{
    D3D11_BLEND_DESC src_desc;
    int i;

    if (blendOverride != 2)
        return;

    if (blendState)
        blendState->Release();
    blendState = NULL;

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

    memcpy_masked_merge(&blendDesc, &src_desc, &blendMask, sizeof(D3D11_BLEND_DESC));

    for (i = 0; i < 4; i++) {
        if (blendFactorMergeMask[i])
            blendFactor[i] = src_blend_factor[i];
    }
    blendSampleMask = blendSampleMask & ~blendSampleMaskMergeMask | src_sample_mask & blendSampleMaskMergeMask;

    orig_device->CreateBlendState(&blendDesc, &blendState);
}

void CustomShader::MergeDepthStencilStates(ID3D11DepthStencilState *src_state, UINT src_stencil_ref, ID3D11Device *orig_device)
{
    D3D11_DEPTH_STENCIL_DESC src_desc;

    if (depthStencilOverride != 2)
        return;

    if (depthStencilState)
        depthStencilState->Release();
    depthStencilState = NULL;

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

    memcpy_masked_merge(&depthStencilDesc, &src_desc, &depthStencilMask, sizeof(D3D11_DEPTH_STENCIL_DESC));
    stencilRef = stencilRef & ~stencilRefMask | src_stencil_ref & stencilRefMask;

    orig_device->CreateDepthStencilState(&depthStencilDesc, &depthStencilState);
}

void CustomShader::MergeRasterizerStates(ID3D11RasterizerState *src_state, ID3D11Device *orig_device)
{
    D3D11_RASTERIZER_DESC src_desc;

    if (rsOverride != 2)
        return;

    if (rsState)
        rsState->Release();
    rsState = NULL;

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

    memcpy_masked_merge(&rsDesc, &src_desc, &rsMask, sizeof(D3D11_RASTERIZER_DESC));

    orig_device->CreateRasterizerState(&rsDesc, &rsState);
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

void RunCustomShaderCommand::Run(CommandListState *state)
{
    ID3D11Device *orig_device1 = state->origDevice1;
    ID3D11DeviceContext *orig_context1 = state->origContext1;
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
    struct OMState om_state;
    UINT i;
    D3D11_PRIMITIVE_TOPOLOGY saved_topology;
    UINT num_sampler = D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT;
    ID3D11SamplerState* saved_sampler_states[D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT];

    for (UINT i = 0; i < D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT; ++i)
    {
        saved_sampler_states[i] = nullptr;
    }

    COMMAND_LIST_LOG(state, "%S\n", iniLine.c_str());

    if (customShader->maxExecutionsPerFrame) {
        if (customShader->frameNo != G->frame_no) {
            customShader->frameNo = G->frame_no;
            customShader->executionsThisFrame = 1;
        } else if (customShader->executionsThisFrame++ >= customShader->maxExecutionsPerFrame) {
            COMMAND_LIST_LOG(state, "  max_executions_per_frame exceeded\n");
            Profiling::max_executions_per_frame_exceeded++;
            return;
        }
    }

    customShader->Substantiate(orig_device1);

    saved_shader_inst vs_inst, hs_inst, ds_inst, gs_inst, ps_inst, cs_inst;

    // Assign custom shaders first before running the command lists, and
    // restore them last. This is so that if someone was injecting a
    // sequence of pixel shaders that all shared a common vertex shader
    // we can avoid having to repeatedly save & restore the vertex shader
    // by calling the next shader in sequence from the command list after
    // the draw call.

    if (customShader->vsOverride) {
        orig_context1->VSGetShader(&saved_vs, vs_inst.instances, &vs_inst.num_instances);
        orig_context1->VSSetShader(customShader->vs, NULL, 0);
    }
    if (customShader->hsOverride) {
        orig_context1->HSGetShader(&saved_hs, hs_inst.instances, &hs_inst.num_instances);
        orig_context1->HSSetShader(customShader->hs, NULL, 0);
    }
    if (customShader->dsOverride) {
        orig_context1->DSGetShader(&saved_ds, ds_inst.instances, &ds_inst.num_instances);
        orig_context1->DSSetShader(customShader->ds, NULL, 0);
    }
    if (customShader->gsOverride) {
        orig_context1->GSGetShader(&saved_gs, gs_inst.instances, &gs_inst.num_instances);
        orig_context1->GSSetShader(customShader->gs, NULL, 0);
    }
    if (customShader->psOverride) {
        orig_context1->PSGetShader(&saved_ps, ps_inst.instances, &ps_inst.num_instances);
        orig_context1->PSSetShader(customShader->ps, NULL, 0);
    }
    if (customShader->csOverride) {
        orig_context1->CSGetShader(&saved_cs, cs_inst.instances, &cs_inst.num_instances);
        orig_context1->CSSetShader(customShader->cs, NULL, 0);
    }
    if (customShader->blendOverride) {
        orig_context1->OMGetBlendState(&saved_blend, saved_blend_factor, &saved_sample_mask);
        customShader->MergeBlendStates(saved_blend, saved_blend_factor, saved_sample_mask, orig_device1);
        orig_context1->OMSetBlendState(customShader->blendState, customShader->blendFactor, customShader->blendSampleMask);
    }
    if (customShader->depthStencilOverride) {
        orig_context1->OMGetDepthStencilState(&saved_depth_stencil, &saved_stencil_ref);
        customShader->MergeDepthStencilStates(saved_depth_stencil, saved_stencil_ref, orig_device1);
        orig_context1->OMSetDepthStencilState(customShader->depthStencilState, customShader->stencilRef);
    }
    if (customShader->rsOverride) {
        orig_context1->RSGetState(&saved_rs);
        customShader->MergeRasterizerStates(saved_rs, orig_device1);
        orig_context1->RSSetState(customShader->rsState);
    }
    if (customShader->topology != D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED) {
        orig_context1->IAGetPrimitiveTopology(&saved_topology);
        orig_context1->IASetPrimitiveTopology(customShader->topology);
    }
    if (customShader->samplerOverride) {
        orig_context1->PSGetSamplers(0, num_sampler, saved_sampler_states);
        orig_context1->PSSetSamplers(0, 1, &customShader->samplerState); //just one slot for the moment TODO: allow more via *.ini file
    }
    // We save off the viewports unconditionally for now. We could
    // potentially skip this by flagging if a command list may alter them,
    // but that probably wouldn't buy us anything:
    orig_context1->RSGetViewports(&num_viewports, saved_viewports);
    // Likewise, save off all RTVs, UAVs and DSVs unconditionally:
    save_om_state(state->origContext1, &om_state);

    // Run the command lists. This should generally include a draw or
    // dispatch call, or call out to another command list which does.
    // The reason for having a post command list is so that people can
    // write 'ps-t100 = ResourceFoo; post ps-t100 = null' and have it work.
    saved_post = state->post;
    state->post = false;
    _run_command_list(&customShader->commandList, state);
    state->post = true;
    _run_command_list(&customShader->postCommandList, state);
    state->post = saved_post;

    // Finally restore the original shaders
    if (customShader->vsOverride)
        orig_context1->VSSetShader(saved_vs, vs_inst.instances, vs_inst.num_instances);
    if (customShader->hsOverride)
        orig_context1->HSSetShader(saved_hs, hs_inst.instances, hs_inst.num_instances);
    if (customShader->dsOverride)
        orig_context1->DSSetShader(saved_ds, ds_inst.instances, ds_inst.num_instances);
    if (customShader->gsOverride)
        orig_context1->GSSetShader(saved_gs, gs_inst.instances, gs_inst.num_instances);
    if (customShader->psOverride)
        orig_context1->PSSetShader(saved_ps, ps_inst.instances, ps_inst.num_instances);
    if (customShader->csOverride)
        orig_context1->CSSetShader(saved_cs, cs_inst.instances, cs_inst.num_instances);
    if (customShader->blendOverride)
        orig_context1->OMSetBlendState(saved_blend, saved_blend_factor, saved_sample_mask);
    if (customShader->depthStencilOverride)
        orig_context1->OMSetDepthStencilState(saved_depth_stencil, saved_stencil_ref);
    if (customShader->rsOverride)
        orig_context1->RSSetState(saved_rs);
    if (customShader->topology != D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED)
        orig_context1->IASetPrimitiveTopology(saved_topology);
    if (customShader->samplerOverride)
        orig_context1->PSSetSamplers(0, num_sampler, saved_sampler_states);

    orig_context1->RSSetViewports(num_viewports, saved_viewports);
    restore_om_state(orig_context1, &om_state);

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
}

bool RunCustomShaderCommand::Noop(bool post, bool ignore_cto_pre, bool ignore_cto_post)
{
    return (customShader->commandList.commands.empty() && customShader->postCommandList.commands.empty());
}

void RunExplicitCommandList::Run(CommandListState *state)
{
    bool saved_post;

    COMMAND_LIST_LOG(state, "%S\n", iniLine.c_str());

    if (runPreAndPostTogether) {
        saved_post = state->post;
        state->post = false;
        _run_command_list(&commandListSection->commandList, state);
        state->post = true;
        _run_command_list(&commandListSection->postCommandList, state);
        state->post = saved_post;
    } else if (state->post)
        _run_command_list(&commandListSection->postCommandList, state);
    else
        _run_command_list(&commandListSection->commandList, state);
}

bool RunExplicitCommandList::Noop(bool post, bool ignore_cto_pre, bool ignore_cto_post)
{
    if (runPreAndPostTogether)
        return (commandListSection->commandList.commands.empty() && commandListSection->postCommandList.commands.empty());

    if (post)
        return commandListSection->postCommandList.commands.empty();
    return commandListSection->commandList.commands.empty();
}

std::shared_ptr<RunLinkedCommandList> link_command_lists(CommandList *dst, CommandList *link, const wstring *ini_line)
{
    RunLinkedCommandList *operation = new RunLinkedCommandList(link);
    operation->iniLine = *ini_line;
    std::shared_ptr<RunLinkedCommandList> p(operation);
    dst->commands.push_back(p);
    return p;
}

void RunLinkedCommandList::Run(CommandListState *state)
{
    _run_command_list(link, state, false);
}

bool RunLinkedCommandList::Noop(bool post, bool ignore_cto_pre, bool ignore_cto_post)
{
    return link->commands.empty();
}

static void process_param_rt_size(CommandListState *state)
{
    D3D11_RENDER_TARGET_VIEW_DESC view_desc;
    D3D11_TEXTURE2D_DESC res_desc;
    ID3D11RenderTargetView *view = NULL;
    ID3D11Resource *res = NULL;
    ID3D11Texture2D *tex = NULL;

    if (state->rtWidth != -1)
        return;

    state->origContext1->OMGetRenderTargets(1, &view, NULL);
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

    state->rtWidth = (float)res_desc.Width;
    state->rtHeight = (float)res_desc.Height;

    tex->Release();
out_release_view:
    view->Release();
}

static void update_scissor_info(CommandListState *state)
{
    UINT num = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;

    if (state->scissorValid)
        return;

    state->origContext1->RSGetScissorRects(&num, state->scissorRects);

    state->scissorValid = true;
}

float CommandListOperand::ProcessTextureFilter(CommandListState *state)
{
    TextureOverrideMatches matches;
    TextureOverrideMatches::reverse_iterator rit;
    bool resource_found;

    textureFilterTarget.FindTextureOverrides(state, &resource_found, &matches);

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

float CommandListOperand::ProcessShaderFilter(CommandListState *state)
{
    HackerContext *hacker_context = state->hackerContext;
    ID3D11DeviceChild *shader = NULL;

    switch (shaderFilterTarget) {
        case L'v':
            shader = hacker_context->currentVertexShaderHandle;
            break;
        case L'h':
            shader = hacker_context->currentHullShaderHandle;
            break;
        case L'd':
            shader = hacker_context->currentDomainShaderHandle;
            break;
        case L'g':
            shader = hacker_context->currentGeometryShaderHandle;
            break;
        case L'p':
            shader = hacker_context->currentPixelShaderHandle;
            break;
        case L'c':
            shader = hacker_context->currentComputeShaderHandle;
            break;
        default:
            LogOverlay(LOG_DIRE, "BUG: Unknown shader filter type: \"%C\"\n", shaderFilterTarget);
            break;
    }

    // Negative zero means no shader bound:
    if (!shader)
        return -0.0;

    ShaderMap::iterator shader_it = lookup_shader_hash(shader);

    if (shader_it == G->mShaders.end())
        return 0.0;

    // Positive zero means shader bound with no ShaderOverride
    ShaderOverrideMap::iterator override = lookup_shaderoverride(shader_it->second);
    if (override == G->mShaderOverrideMap.end())
        return 0.0;

    if (override->second.filter_index != FLT_MAX)
        return override->second.filter_index;

    // Matched ShaderOverride / ShaderRegex, but no filter_index:
    return 1.0;
}

void CommandList::Clear()
{
    commands.clear();
    staticVars.clear();
}

CommandListState::CommandListState() :
    hackerDevice(NULL),
    hackerContext(NULL),
    origDevice1(NULL),
    origContext1(NULL),
    rtWidth(-1),
    rtHeight(-1),
    callInfo(NULL),
    thisTarget(NULL),
    resource(NULL),
    view(NULL),
    post(false),
    updateParams(false),
    cursorMaskTex(NULL),
    cursorMaskView(NULL),
    cursorColorTex(NULL),
    cursorColorView(NULL),
    recursion(0),
    extraIndent(0),
    aborted(false),
    scissorValid(false)
{
    memset(&cursorInfo, 0, sizeof(CURSORINFO));
    memset(&cursorInfoEx, 0, sizeof(ICONINFO));
    memset(&windowRect, 0, sizeof(RECT));
}

CommandListState::~CommandListState()
{
    if (cursorInfoEx.hbmMask)
        DeleteObject(cursorInfoEx.hbmMask);
    if (cursorInfoEx.hbmColor)
        DeleteObject(cursorInfoEx.hbmColor);
    if (cursorMaskView)
        cursorMaskView->Release();
    if (cursorMaskTex)
        cursorMaskTex->Release();
    if (cursorColorView)
        cursorColorView->Release();
    if (cursorColorTex)
        cursorColorTex->Release();
}

static void update_window_info(CommandListState *state)
{
    if (state->windowRect.right)
        return;

    if (G->hWnd)
        CursorUpscalingBypass_GetClientRect(G->hWnd, &state->windowRect);
    else
        LOG_DEBUG("UpdateWindowInfo: No hWnd\n");
}

static void update_cursor_info(CommandListState *state)
{
    if (state->cursorInfo.cbSize)
        return;

    state->cursorInfo.cbSize = sizeof(CURSORINFO);
    CursorUpscalingBypass_GetCursorInfo(&state->cursorInfo);
    memcpy(&state->cursorWindowCoords, &state->cursorInfo.ptScreenPos, sizeof(POINT));

    if (G->hWnd)
        CursorUpscalingBypass_ScreenToClient(G->hWnd, &state->cursorWindowCoords);
    else
        LOG_DEBUG("UpdateCursorInfo: No hWnd\n");
}

static void update_cursor_info_ex(CommandListState *state)
{
    if (state->cursorInfoEx.hbmMask)
        return;

    update_cursor_info(state);

    GetIconInfo(state->cursorInfo.hCursor, &state->cursorInfoEx);
}

// Uses an undocumented Windows API to get info about animated cursors and
// calculate the current frame based on the global tick count
// https://stackoverflow.com/questions/6969801/how-do-i-determine-if-the-current-mouse-cursor-is-animated
static unsigned get_cursor_frame(HCURSOR cursor)
{
    typedef HCURSOR(WINAPI* GET_CURSOR_FRAME_INFO)(HCURSOR, LPCWSTR, DWORD, DWORD*, DWORD*);
    static GET_CURSOR_FRAME_INFO fn_get_cursor_frame_info = NULL;
    HMODULE lib_user32 = NULL;
    DWORD period = 6, frames = 1;

    if (!fn_get_cursor_frame_info) {
        lib_user32 = LoadLibraryA("user32.dll");
        if (!lib_user32)
            return 0;

        fn_get_cursor_frame_info = (GET_CURSOR_FRAME_INFO)GetProcAddress(lib_user32, "GetCursorFrameInfo");
        if (!fn_get_cursor_frame_info)
            return 0;
    }

    fn_get_cursor_frame_info(cursor, L"", 0, &period, &frames);

    // Avoid divide by zero if not an animated cursor:
    if (!period || !frames)
        return 0;

    // period is a multiple of 1/60 seconds. We should really use the ms
    // since this cursor was most recently displayed, but the global tick
    // count works well enough and means we have less state to track:
    return (GetTickCount() * 6) / (period * 100) % frames;
}

static void _create_texture_from_bitmap(HDC dc, BITMAP *bitmap_obj,
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
        LOG_INFO("Software Mouse: GetDIBits() failed\n");
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

    LOCK_RESOURCE_CREATION_MODE();
    hr = state->origDevice1->CreateTexture2D(&desc, &data, tex);
    UNLOCK_RESOURCE_CREATION_MODE();
    if (FAILED(hr)) {
        LOG_INFO("Software Mouse: CreateTexture2D Failed: 0x%x\n", hr);
        goto err_free;
    }

    rv_desc.Format = desc.Format;
    rv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    rv_desc.Texture2D.MostDetailedMip = 0;
    rv_desc.Texture2D.MipLevels = 1;

    hr = state->origDevice1->CreateShaderResourceView(*tex, &rv_desc, view);
    if (FAILED(hr)) {
        LOG_INFO("Software Mouse: CreateShaderResourceView Failed: 0x%x\n", hr);
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

static void create_texture_from_bitmap(HDC dc, HBITMAP hbitmap, CommandListState *state,
        ID3D11Texture2D **tex, ID3D11ShaderResourceView **view)
{
    BITMAP bitmap_obj;

    if (!GetObject(hbitmap, sizeof(BITMAP), &bitmap_obj)) {
        LOG_INFO("Software Mouse: GetObject() failed\n");
        return;
    }

    _create_texture_from_bitmap(dc, &bitmap_obj, hbitmap, state, tex, view);
}

static void create_texture_from_animated_cursor(
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
        LOG_INFO("Software Mouse: GetObject() failed\n");
        return;
    }

    dc_mem = CreateCompatibleDC(dc);
    if (!dc_mem) {
        LOG_INFO("Software Mouse: CreateCompatibleDC() failed\n");
        return;
    }

    ani_bitmap = CreateCompatibleBitmap(dc, bitmap_obj.bmWidth, bitmap_obj.bmHeight);
    if (!ani_bitmap) {
        LOG_INFO("Software Mouse: CreateCompatibleBitmap() failed\n");
        goto out_delete_mem_dc;
    }

    frame = get_cursor_frame(cursor);

    // To get a frame from an animated cursor we have to use DrawIconEx to
    // draw it to another bitmap, then we can create a texture from that
    // bitmap:
    SelectObject(dc_mem, ani_bitmap);
    if (!DrawIconEx(dc_mem, 0, 0, cursor, bitmap_obj.bmWidth, bitmap_obj.bmHeight, frame, NULL, flags)) {
        LOG_INFO("Software Mouse: DrawIconEx failed\n");
        // Fall back to getting the first frame from the static_bitmap we already have:
        _create_texture_from_bitmap(dc, &bitmap_obj, static_bitmap, state, tex, view);
        goto out_delete_ani_bitmap;
    }

    _create_texture_from_bitmap(dc, &bitmap_obj, ani_bitmap, state, tex, view);

out_delete_ani_bitmap:
    DeleteObject(ani_bitmap);
out_delete_mem_dc:
    DeleteDC(dc_mem);
}

static void update_cursor_resources(CommandListState *state)
{
    HDC dc;
    Profiling::State profiling_state;

    if (state->cursorMaskTex || state->cursorColorTex)
        return;

    if (Profiling::mode == Profiling::Mode::SUMMARY)
        Profiling::start(&profiling_state);

    update_cursor_info_ex(state);

    // XXX: Should maybe be the device context for the window?
    dc = GetDC(NULL);
    if (!dc) {
        LOG_INFO("Software Mouse: GetDC() failed\n");
        return;
    }

    if (state->cursorInfoEx.hbmColor) {
        // Colour cursor, which may or may not be animated, but the
        // animated routine will work either way:
        create_texture_from_animated_cursor(
                dc,
                state->cursorInfo.hCursor,
                DI_IMAGE,
                state->cursorInfoEx.hbmColor,
                state,
                &state->cursorColorTex,
                &state->cursorColorView);

        if (state->cursorInfoEx.hbmMask) {
            // Since it's a colour cursor the mask bitmap will be
            // the regular height, which will work with the
            // animated routine:
            create_texture_from_animated_cursor(
                    dc,
                    state->cursorInfo.hCursor,
                    DI_MASK,
                    state->cursorInfoEx.hbmMask,
                    state,
                    &state->cursorMaskTex,
                    &state->cursorMaskView);
        }
    } else if (state->cursorInfoEx.hbmMask) {
        // Black and white cursor, which means the hbmMask bitmap is
        // double height and won't work with the animated cursor
        // routines, so just turn the bitmap into a texture directly:
        create_texture_from_bitmap(
                dc,
                state->cursorInfoEx.hbmMask,
                state,
                &state->cursorMaskTex,
                &state->cursorMaskView);
    }

    ReleaseDC(NULL, dc);

    if (Profiling::mode == Profiling::Mode::SUMMARY)
        Profiling::end(&profiling_state, &Profiling::cursor_overhead);
}

static bool sli_enabled(HackerDevice *device)
{
    NV_GET_CURRENT_SLI_STATE sli_state;
    sli_state.version = NV_GET_CURRENT_SLI_STATE_VER;
    NvAPI_Status status;

    status = Profiling::NvAPI_D3D_GetCurrentSLIState(device->GetPossiblyHookedOrigDevice1(), &sli_state);
    if (status != NVAPI_OK) {
        LOG_INFO("Unable to retrieve SLI state from nvapi\n");
        return false;
    }

    return sli_state.maxNumAFRGroups > 1;
}

float CommandListOperand::Evaluate(CommandListState *state, HackerDevice *device)
{
    NvU8 stereo = false;
    float fret;

    if (state)
        device = state->hackerDevice;
    else if (!device) {
        LogOverlay(LOG_DIRE, "BUG: CommandListOperand::evaluate called with neither state nor device\n");
        return 0;
    }

    // XXX: If updating this list, be sure to also update
    // XXX: operand_allowed_in_context()
    switch (type) {
        case ParamOverrideType::VALUE:
            return val;
        case ParamOverrideType::INI_PARAM:
            return G->iniParams[paramIdx].*paramComponent;
        case ParamOverrideType::VARIABLE:
            return *varFtarget;
        case ParamOverrideType::RES_WIDTH:
            return (float)G->mResolutionInfo.width;
        case ParamOverrideType::RES_HEIGHT:
            return (float)G->mResolutionInfo.height;
        case ParamOverrideType::TIME:
            return (float)(GetTickCount() - G->ticks_at_launch) / 1000.0f;
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
            Profiling::NvAPI_Stereo_GetSeparation(device->stereoHandle, &fret);
            return fret;
        case ParamOverrideType::CONVERGENCE:
            Profiling::NvAPI_Stereo_GetConvergence(device->stereoHandle, &fret);
            return fret;
        case ParamOverrideType::EYE_SEPARATION:
            Profiling::NvAPI_Stereo_GetEyeSeparation(device->stereoHandle, &fret);
            return fret;
        case ParamOverrideType::STEREO_ACTIVE:
            Profiling::NvAPI_Stereo_IsActivated(device->stereoHandle, &stereo);
            return !!stereo;
        case ParamOverrideType::STEREO_AVAILABLE:
            Profiling::NvAPI_Stereo_IsEnabled(&stereo);
            return !!stereo;
        case ParamOverrideType::SLI:
            return sli_enabled(device);
        case ParamOverrideType::HUNTING:
            return (float)G->hunting;
        case ParamOverrideType::FRAME_ANALYSIS:
            return G->analyse_frame;
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
            process_param_rt_size(state);
            return state->rtWidth;
        case ParamOverrideType::RT_HEIGHT:
            process_param_rt_size(state);
            return state->rtHeight;
        case ParamOverrideType::WINDOW_WIDTH:
            update_window_info(state);
            return (float)state->windowRect.right;
        case ParamOverrideType::WINDOW_HEIGHT:
            update_window_info(state);
            return (float)state->windowRect.bottom;
        case ParamOverrideType::TEXTURE:
            return ProcessTextureFilter(state);
        case ParamOverrideType::SHADER:
            return ProcessShaderFilter(state);
        case ParamOverrideType::VERTEX_COUNT:
            if (state->callInfo)
                return (float)state->callInfo->VertexCount;
            return 0;
        case ParamOverrideType::INDEX_COUNT:
            if (state->callInfo)
                return (float)state->callInfo->IndexCount;
            return 0;
        case ParamOverrideType::INSTANCE_COUNT:
            if (state->callInfo)
                return (float)state->callInfo->InstanceCount;
            return 0;
        case ParamOverrideType::FIRST_VERTEX:
            if (state->callInfo)
                return (float)state->callInfo->FirstVertex;
            return 0;
        case ParamOverrideType::FIRST_INDEX:
            if (state->callInfo)
                return (float)state->callInfo->FirstIndex;
            return 0;
        case ParamOverrideType::FIRST_INSTANCE:
            if (state->callInfo)
                return (float)state->callInfo->FirstInstance;
            return 0;
        case ParamOverrideType::THREAD_GROUP_COUNT_X:
            if (state->callInfo)
                return (float)state->callInfo->ThreadGroupCountX;
            return 0;
        case ParamOverrideType::THREAD_GROUP_COUNT_Y:
            if (state->callInfo)
                return (float)state->callInfo->ThreadGroupCountY;
            return 0;
        case ParamOverrideType::THREAD_GROUP_COUNT_Z:
            if (state->callInfo)
                return (float)state->callInfo->ThreadGroupCountZ;
            return 0;
        case ParamOverrideType::INDIRECT_OFFSET:
            if (state->callInfo)
                return (float)state->callInfo->args_offset;
            return 0;
        case ParamOverrideType::DRAW_TYPE:
            if (state->callInfo)
                return (float)state->callInfo->type;
            return 0;
        case ParamOverrideType::CURSOR_VISIBLE:
            update_cursor_info(state);
            return !!(state->cursorInfo.flags & CURSOR_SHOWING);
        case ParamOverrideType::CURSOR_SCREEN_X:
            update_cursor_info(state);
            return (float)state->cursorInfo.ptScreenPos.x;
        case ParamOverrideType::CURSOR_SCREEN_Y:
            update_cursor_info(state);
            return (float)state->cursorInfo.ptScreenPos.y;
        case ParamOverrideType::CURSOR_WINDOW_X:
            update_cursor_info(state);
            return (float)state->cursorWindowCoords.x;
        case ParamOverrideType::CURSOR_WINDOW_Y:
            update_cursor_info(state);
            return (float)state->cursorWindowCoords.y;
        case ParamOverrideType::CURSOR_X:
            update_cursor_info(state);
            update_window_info(state);
            return (float)state->cursorWindowCoords.x / (float)state->windowRect.right;
        case ParamOverrideType::CURSOR_Y:
            update_cursor_info(state);
            update_window_info(state);
            return (float)state->cursorWindowCoords.y / (float)state->windowRect.bottom;
        case ParamOverrideType::CURSOR_HOTSPOT_X:
            update_cursor_info_ex(state);
            return (float)state->cursorInfoEx.xHotspot;
        case ParamOverrideType::CURSOR_HOTSPOT_Y:
            update_cursor_info_ex(state);
            return (float)state->cursorInfoEx.yHotspot;
        case ParamOverrideType::SCISSOR_LEFT:
            update_scissor_info(state);
            return (float)state->scissorRects[scissor].left;
        case ParamOverrideType::SCISSOR_TOP:
            update_scissor_info(state);
            return (float)state->scissorRects[scissor].top;
        case ParamOverrideType::SCISSOR_RIGHT:
            update_scissor_info(state);
            return (float)state->scissorRects[scissor].right;
        case ParamOverrideType::SCISSOR_BOTTOM:
            update_scissor_info(state);
            return (float)state->scissorRects[scissor].bottom;
    }

    LogOverlay(LOG_DIRE, "BUG: Unhandled operand type %i\n", type);
    return 0;
}

bool CommandListOperand::StaticEvaluate(float *ret, HackerDevice *device)
{
    NvU8 stereo = false;

    switch (type) {
        case ParamOverrideType::VALUE:
            *ret = val;
            return true;
        case ParamOverrideType::RAW_SEPARATION:
        case ParamOverrideType::CONVERGENCE:
        case ParamOverrideType::EYE_SEPARATION:
        case ParamOverrideType::STEREO_ACTIVE:
            nvapi_override();
            Profiling::NvAPI_Stereo_IsEnabled(&stereo);
            if (!stereo) {
                *ret = 0.0;
                return true;
            }
            break;
        case ParamOverrideType::STEREO_AVAILABLE:
            Profiling::NvAPI_Stereo_IsEnabled(&stereo);
            *ret = stereo;
            return true;
        case ParamOverrideType::SLI:
            if (device) {
                *ret = sli_enabled(device);
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

bool CommandListOperand::Optimise(HackerDevice *device, std::shared_ptr<CommandListEvaluatable> *replacement)
{
    if (type == ParamOverrideType::VALUE)
        return false;

    if (!StaticEvaluate(&val, device))
        return false;

    LOG_INFO("Statically evaluated %S as %f\n",
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

class CommandListSyntaxError: public exception
{
public:
    wstring msg;
    size_t pos;

    CommandListSyntaxError(wstring msg, size_t pos) :
        msg(msg), pos(pos)
    {}
};

static void tokenise(const wstring *expression, CommandListSyntaxTree *tree, const wstring *ini_namespace, CommandListScope *scope)
{
    wstring remain = *expression;
    ResourceCopyTarget texture_filter_target;
    shared_ptr<CommandListOperand> operand;
    wstring token;
    size_t pos = 0;
    int ipos = 0;
    size_t friendly_pos = 0;
    float fval;
    int ret;
    int i;
    bool last_was_operand = false;

    LOG_DEBUG("    Tokenising \"%S\"\n", expression->c_str());

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
                LOG_DEBUG("      Operator: \"%S\"\n", tree->tokens.back()->token.c_str());
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
                operand = make_shared<CommandListOperand>(friendly_pos, token);
                if (operand->Parse(&token, ini_namespace, scope)) {
                    tree->tokens.emplace_back(std::move(operand));
                    LOG_DEBUG("      Resource Slot: \"%S\"\n", tree->tokens.back()->token.c_str());
                    if (last_was_operand)
                        throw CommandListSyntaxError(L"Unexpected identifier", friendly_pos);
                    last_was_operand = true;
                    continue;
                } else {
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
            pos = remain.find_first_not_of(L"abcdefghijklmnopqrstuvwxyz_0123456789$\\.");
            if (pos) {
                token = remain.substr(0, pos);
                operand = make_shared<CommandListOperand>(friendly_pos, token);
                if (operand->Parse(&token, ini_namespace, scope)) {
                    tree->tokens.emplace_back(std::move(operand));
                    LOG_DEBUG("      Identifier: \"%S\"\n", tree->tokens.back()->token.c_str());
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
            operand = make_shared<CommandListOperand>(friendly_pos, token);
            if (operand->Parse(&token, ini_namespace, scope)) {
                tree->tokens.emplace_back(std::move(operand));
                LOG_DEBUG("      Float: \"%S\"\n", tree->tokens.back()->token.c_str());
                if (last_was_operand)
                    throw CommandListSyntaxError(L"Unexpected identifier", friendly_pos);
                last_was_operand = true;
                continue;
            } else {
                LogOverlay(LOG_DIRE, "BUG: Token parsed as float, but not as operand: \"%S\"\n", token.c_str());
                throw CommandListSyntaxError(L"BUG", friendly_pos);
            }
        }

        throw CommandListSyntaxError(L"Parse error", friendly_pos);
    }
}

static void group_parenthesis(CommandListSyntaxTree *tree)
{
    CommandListSyntaxTree::Tokens::iterator i;
    CommandListSyntaxTree::Tokens::reverse_iterator rit;
    CommandListOperatorToken *rbracket, *lbracket;
    std::shared_ptr<CommandListSyntaxTree> inner;

    for (i = tree->tokens.begin(); i != tree->tokens.end(); i++) {
        rbracket = dynamic_cast<CommandListOperatorToken*>(i->get());
        if (rbracket && !rbracket->token.compare(L")")) {
            for (rit = std::reverse_iterator<CommandListSyntaxTree::Tokens::iterator>(i); rit != tree->tokens.rend(); rit++) {
                lbracket = dynamic_cast<CommandListOperatorToken*>(rit->get());
                if (lbracket && !lbracket->token.compare(L"(")) {
                    inner = std::make_shared<CommandListSyntaxTree>(lbracket->tokenPos);
                    // XXX: Double check bounds are right:
                    inner->tokens.assign(rit.base(), i);
                    i = tree->tokens.erase(rit.base() - 1, i + 1);
                    i = tree->tokens.insert(i, std::move(inner));
                    goto continue_rbracket_search; // continue would continue wrong loop
                }
            }
            throw CommandListSyntaxError(L"Unmatched )", rbracket->tokenPos);
        }
    continue_rbracket_search: false;
    }

    for (i = tree->tokens.begin(); i != tree->tokens.end(); i++) {
        lbracket = dynamic_cast<CommandListOperatorToken*>(i->get());
        if (lbracket && !lbracket->token.compare(L"("))
            throw CommandListSyntaxError(L"Unmatched (", lbracket->tokenPos);
    }
}

// Expression operator definitions:
#define DEFINE_OPERATOR(name, operator_pattern, fn) \
class name##T : public CommandListOperator { \
public: \
    name##T( \
            std::shared_ptr<CommandListToken> lhs, \
            CommandListOperatorToken &t, \
            std::shared_ptr<CommandListToken> rhs \
        ) : CommandListOperator(lhs, t, rhs) \
    {} \
    static const wchar_t* pattern() { return L##operator_pattern; } \
    float Evaluate(float lhs, float rhs) override { return (fn); } \
}; \
static CommandListOperatorFactory<name##T> name;

// Highest level of precedence, allows for negative numbers
DEFINE_OPERATOR(unary_not_operator,     "!",  (!rhs));
DEFINE_OPERATOR(unary_plus_operator,    "+",  (+rhs));
DEFINE_OPERATOR(unary_negate_operator,  "-",  (-rhs));

// High level of precedence, right-associative. Lower than unary operators, so
// that 4**-2 works for square root
DEFINE_OPERATOR(exponent_operator,      "**", (pow(lhs, rhs)));

DEFINE_OPERATOR(multiplication_operator,"*",  (lhs * rhs));
DEFINE_OPERATOR(division_operator,      "/",  (lhs / rhs));
DEFINE_OPERATOR(floor_division_operator,"//", (floor(lhs / rhs)));
DEFINE_OPERATOR(modulus_operator,       "%",  (fmod(lhs, rhs)));

DEFINE_OPERATOR(addition_operator,      "+",  (lhs + rhs));
DEFINE_OPERATOR(subtraction_operator,   "-",  (lhs - rhs));

DEFINE_OPERATOR(less_operator,          "<",  (lhs < rhs));
DEFINE_OPERATOR(less_equal_operator,    "<=", (lhs <= rhs));
DEFINE_OPERATOR(greater_operator,       ">",  (lhs > rhs));
DEFINE_OPERATOR(greater_equal_operator, ">=", (lhs >= rhs));

// The triple equals operator tests for binary equivalence - in particular,
// this allows us to test for negative zero, used in texture filtering to
// signify that nothing is bound to a given slot. Negative zero cannot be
// tested for using the regular equals operator, since -0.0 == +0.0. This
// operator could also test for specific cases of NAN (though, without the
// vs2015 toolchain "nan" won't parse as such).
DEFINE_OPERATOR(equality_operator,      "==", (lhs == rhs));
DEFINE_OPERATOR(inequality_operator,    "!=", (lhs != rhs));
DEFINE_OPERATOR(identical_operator,     "===",(*(uint32_t*)&lhs == *(uint32_t*)&rhs));
DEFINE_OPERATOR(not_identical_operator, "!==",(*(uint32_t*)&lhs != *(uint32_t*)&rhs));

DEFINE_OPERATOR(and_operator,           "&&", (lhs && rhs));

DEFINE_OPERATOR(or_operator,            "||", (lhs || rhs));

// TODO: Ternary if operator

static CommandListOperatorFactoryBase *unary_operators[] = {
    &unary_not_operator,
    &unary_negate_operator,
    &unary_plus_operator,
};
static CommandListOperatorFactoryBase *exponent_operators[] = {
    &exponent_operator,
};
static CommandListOperatorFactoryBase *multi_division_operators[] = {
    &multiplication_operator,
    &division_operator,
    &floor_division_operator,
    &modulus_operator,
};
static CommandListOperatorFactoryBase *add_subtract_operators[] = {
    &addition_operator,
    &subtraction_operator,
};
static CommandListOperatorFactoryBase *relational_operators[] = {
    &less_operator,
    &less_equal_operator,
    &greater_operator,
    &greater_equal_operator,
};
static CommandListOperatorFactoryBase *equality_operators[] = {
    &equality_operator,
    &inequality_operator,
    &identical_operator,
    &not_identical_operator,
};
static CommandListOperatorFactoryBase *and_operators[] = {
    &and_operator,
};
static CommandListOperatorFactoryBase *or_operators[] = {
    &or_operator,
};

static CommandListSyntaxTree::Tokens::iterator transform_operators_token(
        CommandListSyntaxTree *tree,
        CommandListSyntaxTree::Tokens::iterator i,
        CommandListOperatorFactoryBase *factories[], int num_factories,
        bool unary)
{
    std::shared_ptr<CommandListOperatorToken> token;
    std::shared_ptr<CommandListOperator> op;
    std::shared_ptr<CommandListOperandBase> lhs;
    std::shared_ptr<CommandListOperandBase> rhs;
    int f;

    token = dynamic_pointer_cast<CommandListOperatorToken>(*i);
    if (!token)
        return i;

    for (f = 0; f < num_factories; f++) {
        if (token->token.compare(factories[f]->Pattern()))
            continue;

        lhs = nullptr;
        rhs = nullptr;
        if (i > tree->tokens.begin())
            lhs = dynamic_pointer_cast<CommandListOperandBase>(*(i-1));
        if (i < tree->tokens.end() - 1)
            rhs = dynamic_pointer_cast<CommandListOperandBase>(*(i+1));

        if (unary) {
            // It is particularly important that we check that the
            // LHS is *not* an operand so the unary +/- operators
            // don't trump the binary addition/subtraction operators:
            if (rhs && !lhs) {
                op = factories[f]->Create(nullptr, *token, *(i+1));
                i = tree->tokens.erase(i, i+2);
                i = tree->tokens.insert(i, std::move(op));
                break;
            }
        } else {
            if (lhs && rhs) {
                op = factories[f]->Create(*(i-1), *token, *(i+1));
                i = tree->tokens.erase(i-1, i+2);
                i = tree->tokens.insert(i, std::move(op));
                break;
            }
        }
    }

    return i;
}

// Transforms operator tokens in the syntax tree into actual operators
static void transform_operators_visit(CommandListSyntaxTree *tree,
        CommandListOperatorFactoryBase *factories[], int num_factories,
        bool right_associative, bool unary)
{
    CommandListSyntaxTree::Tokens::iterator i;
    CommandListSyntaxTree::Tokens::reverse_iterator rit;

    if (!tree)
        return;

    if (right_associative) {
        if (unary) {
            // Start at the second from the right
            for (rit = tree->tokens.rbegin() + 1; rit != tree->tokens.rend(); rit++) {
                // C++ gotcha: reverse_iterator::base() points to the *next* element
                i = transform_operators_token(tree, rit.base() - 1, factories, num_factories, unary);
                rit = std::reverse_iterator<CommandListSyntaxTree::Tokens::iterator>(i + 1);
            }
        } else {
            for (rit = tree->tokens.rbegin() + 1; rit < tree->tokens.rend() - 1; rit++) {
                // C++ gotcha: reverse_iterator::base() points to the *next* element
                i = transform_operators_token(tree, rit.base() - 1, factories, num_factories, unary);
                rit = std::reverse_iterator<CommandListSyntaxTree::Tokens::iterator>(i + 1);
            }
        }
    } else {
        if (unary) {
            throw CommandListSyntaxError(L"FIXME: Implement left-associative unary operators", 0);
        } else {
            // Since this is binary operators, skip the first and last
            // nodes as they must be operands, and this way I don't have to
            // worry about bounds checks.
            for (i = tree->tokens.begin() + 1; i < tree->tokens.end() - 1; i++)
                i = transform_operators_token(tree, i, factories, num_factories, unary);
        }
    }
}

static void transform_operators_recursive(CommandListWalkable *tree,
        CommandListOperatorFactoryBase *factories[], int num_factories,
        bool right_associative, bool unary)
{
    // Depth first to ensure that we have visited all sub-trees before
    // transforming operators in this level, since that may add new
    // sub-trees
    for (auto &inner: tree->Walk()) {
        transform_operators_recursive(dynamic_cast<CommandListWalkable*>(inner.get()),
                factories, num_factories, right_associative, unary);
    }

    transform_operators_visit(dynamic_cast<CommandListSyntaxTree*>(tree),
            factories, num_factories, right_associative, unary);
}

// Using raw pointers here so that ::optimise() can call it with "this"
static void _log_syntax_tree(CommandListSyntaxTree *tree);
static void _log_token(CommandListToken *token)
{
    CommandListSyntaxTree *inner;
    CommandListOperator *op;
    CommandListOperatorToken *op_tok;
    CommandListOperand *operand;

    if (!token)
        return;

    // Can't use CommandListWalkable here, because it only walks over inner
    // syntax trees and this debug dumper needs to walk over everything

    inner = dynamic_cast<CommandListSyntaxTree*>(token);
    op = dynamic_cast<CommandListOperator*>(token);
    op_tok = dynamic_cast<CommandListOperatorToken*>(token);
    operand = dynamic_cast<CommandListOperand*>(token);
    if (inner) {
        _log_syntax_tree(inner);
    } else if (op) {
        LOG_INFO_NO_NL("Operator \"%S\"[ ", token->token.c_str());
        if (op->lhsTree)
            _log_token(op->lhsTree.get());
        else if (op->lhs)
            _log_token(dynamic_cast<CommandListToken*>(op->lhs.get()));
        if ((op->lhsTree || op->lhs) && (op->rhsTree || op->rhs))
            LOG_INFO_NO_NL(", ");
        if (op->rhsTree)
            _log_token(op->rhsTree.get());
        else if (op->rhs)
            _log_token(dynamic_cast<CommandListToken*>(op->rhs.get()));
        LOG_INFO_NO_NL(" ]");
    } else if (op_tok) {
        LOG_INFO_NO_NL("OperatorToken \"%S\"", token->token.c_str());
    } else if (operand) {
        LOG_INFO_NO_NL("Operand \"%S\"", token->token.c_str());
    } else {
        LOG_INFO_NO_NL("Token \"%S\"", token->token.c_str());
    }
}
static void _log_syntax_tree(CommandListSyntaxTree *tree)
{
    CommandListSyntaxTree::Tokens::iterator i;

    LOG_INFO_NO_NL("SyntaxTree[ ");
    for (i = tree->tokens.begin(); i != tree->tokens.end(); i++) {
        _log_token((*i).get());
        if (i != tree->tokens.end()-1)
            LOG_INFO_NO_NL(", ");
    }
    LOG_INFO_NO_NL(" ]");
}

static void log_syntax_tree(CommandListSyntaxTree *tree, const char *msg)
{
    if (!gLogDebug)
        return;

    LOG_INFO(msg);
    _log_syntax_tree(tree);
    LOG_INFO("\n");
}

template<class T>
static void log_syntax_tree(T token, const char *msg)
{
    if (!gLogDebug)
        return;

    LOG_INFO(msg);
    _log_token(dynamic_cast<CommandListToken*>(token.get()));
    LOG_INFO("\n");
}

bool CommandListExpression::Parse(const wstring *expression, const wstring *ini_namespace, CommandListScope *scope)
{
    CommandListSyntaxTree tree(0);

    try {
        tokenise(expression, &tree, ini_namespace, scope);

        group_parenthesis(&tree);

        transform_operators_recursive(&tree, unary_operators, ARRAYSIZE(unary_operators), true, true);
        transform_operators_recursive(&tree, exponent_operators, ARRAYSIZE(exponent_operators), true, false);
        transform_operators_recursive(&tree, multi_division_operators, ARRAYSIZE(multi_division_operators), false, false);
        transform_operators_recursive(&tree, add_subtract_operators, ARRAYSIZE(add_subtract_operators), false, false);
        transform_operators_recursive(&tree, relational_operators, ARRAYSIZE(relational_operators), false, false);
        transform_operators_recursive(&tree, equality_operators, ARRAYSIZE(equality_operators), false, false);
        transform_operators_recursive(&tree, and_operators, ARRAYSIZE(and_operators), false, false);
        transform_operators_recursive(&tree, or_operators, ARRAYSIZE(or_operators), false, false);

        evaluatable = tree.Finalise();
        log_syntax_tree(evaluatable, "Final syntax tree:\n");
        return true;
    } catch (const CommandListSyntaxError &e) {
        LogOverlay(LOG_WARNING_MONOSPACE,
                "Syntax Error: %S\n"
                "              %*s: %S\n",
                expression->c_str(), (int)e.pos+1, "^", e.msg.c_str());
        return false;
    }
}

float CommandListExpression::Evaluate(CommandListState *state, HackerDevice *device)
{
    return evaluatable->Evaluate(state, device);
}

bool CommandListExpression::StaticEvaluate(float *ret, HackerDevice *device)
{
    return evaluatable->StaticEvaluate(ret, device);
}

bool CommandListExpression::Optimise(HackerDevice *device)
{
    std::shared_ptr<CommandListEvaluatable> replacement;
    bool ret;

    if (!evaluatable) {
        LogOverlay(LOG_DIRE, "BUG: Non-evaluatable expression, please report this and provide your d3dx.ini\n");
        evaluatable = std::make_shared<CommandListOperand>(0, L"<BUG>");
        return false;
    }

    ret = evaluatable->Optimise(device, &replacement);

    if (replacement)
        evaluatable = replacement;

    return ret;
}

// Finalises the syntax trees in the operator into evaluatable operands,
// thereby making this operator also evaluatable.
std::shared_ptr<CommandListEvaluatable> CommandListOperator::Finalise()
{
    auto lhs_finalisable = dynamic_pointer_cast<CommandListFinalisable>(lhsTree);
    auto rhs_finalisable = dynamic_pointer_cast<CommandListFinalisable>(rhsTree);
    auto lhs_evaluatable = dynamic_pointer_cast<CommandListEvaluatable>(lhsTree);
    auto rhs_evaluatable = dynamic_pointer_cast<CommandListEvaluatable>(rhsTree);

    if (lhs || rhs) {
        LOG_INFO("BUG: Attempted to finalise already final operator\n");
        throw CommandListSyntaxError(L"BUG", tokenPos);
    }

    if (lhsTree) { // Binary operators only
        if (!lhs && lhs_finalisable)
            lhs = lhs_finalisable->Finalise();
        if (!lhs && lhs_evaluatable)
            lhs = lhs_evaluatable;
        if (!lhs)
            throw CommandListSyntaxError(L"BUG: LHS operand invalid", tokenPos);
        lhsTree = nullptr;
    }

    if (!rhs && rhs_finalisable)
        rhs = rhs_finalisable->Finalise();
    if (!rhs && rhs_evaluatable)
        rhs = rhs_evaluatable;
    if (!rhs)
        throw CommandListSyntaxError(L"BUG: RHS operand invalid", tokenPos);
    rhsTree = nullptr;

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
std::shared_ptr<CommandListEvaluatable> CommandListSyntaxTree::Finalise()
{
    std::shared_ptr<CommandListFinalisable> finalisable;
    std::shared_ptr<CommandListEvaluatable> evaluatable;
    std::shared_ptr<CommandListToken> token;
    Tokens::iterator i;

    for (i = tokens.begin(); i != tokens.end(); i++) {
        finalisable = dynamic_pointer_cast<CommandListFinalisable>(*i);
        if (finalisable) {
            evaluatable = finalisable->Finalise();
            if (evaluatable) {
                // A recursive syntax tree has been finalised
                // and we replace it with its sole evaluatable
                // contents:
                token = dynamic_pointer_cast<CommandListToken>(evaluatable);
                if (!token) {
                    LOG_INFO("BUG: finalised token did not cast back\n");
                    throw CommandListSyntaxError(L"BUG", tokenPos);
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
        throw CommandListSyntaxError(L"Unexpected", tokens[1]->tokenPos);

    evaluatable = dynamic_pointer_cast<CommandListEvaluatable>(tokens[0]);
    if (!evaluatable)
        throw CommandListSyntaxError(L"Non-evaluatable", tokens[0]->tokenPos);

    return evaluatable;
}

CommandListSyntaxTree::Walks CommandListSyntaxTree::Walk()
{
    Walks ret;
    std::shared_ptr<CommandListWalkable> inner;
    Tokens::iterator i;

    for (i = tokens.begin(); i != tokens.end(); i++) {
        inner = dynamic_pointer_cast<CommandListWalkable>(*i);
        if (inner)
            ret.push_back(std::move(inner));
    }

    return ret;
}

float CommandListOperator::Evaluate(CommandListState *state, HackerDevice *device)
{
    if (lhs) // Binary operator
        return Evaluate(lhs->Evaluate(state, device), rhs->Evaluate(state, device));
    return Evaluate(std::numeric_limits<float>::quiet_NaN(), rhs->Evaluate(state, device));
}

bool CommandListOperator::StaticEvaluate(float *ret, HackerDevice *device)
{
    float lhs_static = std::numeric_limits<float>::quiet_NaN(), rhs_static;
    bool is_static;

    is_static = rhs->StaticEvaluate(&rhs_static, device);
    if (lhs) // Binary operator
        is_static = lhs->StaticEvaluate(&lhs_static, device) && is_static;

    if (is_static) {
        if (ret)
            *ret = Evaluate(lhs_static, rhs_static);
        return true;
    }

    return false;
}

bool CommandListOperator::Optimise(HackerDevice *device, std::shared_ptr<CommandListEvaluatable> *replacement)
{
    std::shared_ptr<CommandListEvaluatable> lhs_replacement;
    std::shared_ptr<CommandListEvaluatable> rhs_replacement;
    shared_ptr<CommandListOperand> operand;
    bool making_progress = false;
    float static_val;
    wstring static_val_str;

    if (lhs)
        making_progress = lhs->Optimise(device, &lhs_replacement) || making_progress;
    if (rhs)
        making_progress = rhs->Optimise(device, &rhs_replacement) || making_progress;

    if (lhs_replacement)
        lhs = lhs_replacement;
    if (rhs_replacement)
        rhs = rhs_replacement;

    if (!StaticEvaluate(&static_val, device))
        return making_progress;

    // FIXME: Pretty print rather than dumping syntax tree
    LOG_INFO_NO_NL("Statically evaluated \"");
    _log_token(dynamic_cast<CommandListToken*>(this));
    LOG_INFO("\" as %f\n", static_val);
    static_val_str = std::to_wstring(static_val);

    operand = make_shared<CommandListOperand>(tokenPos, static_val_str.c_str());
    operand->type = ParamOverrideType::VALUE;
    operand->val = static_val;
    *replacement = dynamic_pointer_cast<CommandListEvaluatable>(operand);
    return true;
}

CommandListSyntaxTree::Walks CommandListOperator::Walk()
{
    Walks ret;
    std::shared_ptr<CommandListWalkable> lhs;
    std::shared_ptr<CommandListWalkable> rhs;

    lhs = dynamic_pointer_cast<CommandListWalkable>(lhsTree);
    rhs = dynamic_pointer_cast<CommandListWalkable>(rhsTree);

    if (lhs)
        ret.push_back(std::move(lhs));
    if (rhs)
        ret.push_back(std::move(rhs));

    return ret;
}

void ParamOverride::Run(CommandListState *state)
{
    float *dest = &(G->iniParams[paramIdx].*paramComponent);
    float orig = *dest;

    COMMAND_LIST_LOG(state, "%S\n", iniLine.c_str());

    *dest = expression.Evaluate(state);

    COMMAND_LIST_LOG(state, "  ini param override = %f\n", *dest);

    state->updateParams |= (*dest != orig);
}

void VariableAssignment::Run(CommandListState *state)
{
    float orig = var->fval;

    COMMAND_LIST_LOG(state, "%S\n", iniLine.c_str());

    var->fval = expression.Evaluate(state);

    COMMAND_LIST_LOG(state, "  = %f\n", var->fval);

    if (var->flags & VariableFlags::PERSIST)
        G->user_config_dirty |= (var->fval != orig);
}

bool AssignmentCommand::Optimise(HackerDevice *device)
{
    return expression.Optimise(device);
}

static bool operand_allowed_in_context(ParamOverrideType type, CommandListScope *scope)
{
    if (scope)
        return true;

    // List of operand types allowed outside of a command list, e.g. in a
    // [Key] / [Preset] section
    switch (type) {
        case ParamOverrideType::VALUE:
        case ParamOverrideType::INI_PARAM:
        case ParamOverrideType::VARIABLE:
        case ParamOverrideType::RES_WIDTH:
        case ParamOverrideType::RES_HEIGHT:
        case ParamOverrideType::TIME:
        case ParamOverrideType::RAW_SEPARATION:
        case ParamOverrideType::CONVERGENCE:
        case ParamOverrideType::EYE_SEPARATION:
        case ParamOverrideType::STEREO_ACTIVE:
        case ParamOverrideType::STEREO_AVAILABLE:
        case ParamOverrideType::SLI:
        case ParamOverrideType::HUNTING:
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

bool parse_command_list_var_name(const wstring &name, const wstring *ini_namespace, CommandListVariable **target)
{
    CommandListVariables::iterator var = command_list_globals.end();

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

int find_local_variable(const wstring &name, CommandListScope *scope, CommandListVariable **var)
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

bool declare_local_variable(const wchar_t *section, wstring &name,
        CommandList *pre_command_list, const wstring *ini_namespace)
{
    CommandListVariable *var = NULL;

    if (!valid_variable_name(name)) {
        LogOverlay(LOG_WARNING, "WARNING: Illegal local variable name: [%S] \"%S\"\n", section, name.c_str());
        return false;
    }

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

    pre_command_list->staticVars.emplace_front(name, 0.0f, VariableFlags::NONE);
    pre_command_list->scope->front()[name] = &pre_command_list->staticVars.front();

    return true;
}

bool CommandListOperand::Parse(const wstring *operand, const wstring *ini_namespace, CommandListScope *scope)
{
    CommandListVariable *var = NULL;
    int ret, len1;

    // Try parsing value as a float
    ret = swscanf_s(operand->c_str(), L"%f%n", &val, &len1);
    if (ret != 0 && ret != EOF && len1 == operand->length()) {
        type = ParamOverrideType::VALUE;
        return operand_allowed_in_context(type, scope);
    }

    // Try parsing operand as an ini param:
    if (parse_ini_param_name(operand->c_str(), &paramIdx, &paramComponent)) {
        type = ParamOverrideType::INI_PARAM;
        // Reserve space in IniParams for this variable:
        G->iniParamsReserved = max(G->iniParamsReserved, paramIdx + 1);
        return operand_allowed_in_context(type, scope);
    }

    // Try parsing operand as a variable:
    if (find_local_variable(*operand, scope, &var) ||
        parse_command_list_var_name(*operand, ini_namespace, &var)) {
        type = ParamOverrideType::VARIABLE;
        varFtarget = &var->fval;
        return operand_allowed_in_context(type, scope);
    }

    // Try parsing value as a resource target for texture filtering
    ret = textureFilterTarget.ParseTarget(operand->c_str(), true, ini_namespace);
    if (ret) {
        type = ParamOverrideType::TEXTURE;
        return operand_allowed_in_context(type, scope);
    }

    // Try parsing value as a shader target for partner filtering
    // WARNING: This test is especially susceptible to an uninitialised
    //          %n fooling it into thinking it has parsed the entire string
    //          if the stack garbage happens to contain operand->length().
    //          This is because the %n does not immediately follow another
    //          conversion specification and does not alter the return
    //          value, so the return value will not distinguish between
    //          early termination and completion, and since %lc will match
    //          any character this can trigger easily. Seems to only occur
    //          on vs2013, though I'm not positive if vs2017 zeroes out
    //          len1 or dumb luck gave different values in the stack.
    len1 = 0;
    ret = swscanf_s(operand->c_str(), L"%lcs%n", &shaderFilterTarget, 1, &len1);
    if (ret == 1 && len1 == operand->length()) {
        switch(shaderFilterTarget) {
        case L'v': case L'h': case L'd': case L'g': case L'p': case L'c':
            type = ParamOverrideType::SHADER;
            return operand_allowed_in_context(type, scope);
        }
    }

    // Try parsing value as a scissor rectangle. scissor_<side> also
    // appears in the keywords list for uses of the default rectangle 0.
    len1 = 0;
    ret = swscanf_s(operand->c_str(), L"scissor%u_%n", &scissor, &len1);
    if (ret == 1 && scissor < D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE) {
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
bool parse_command_list_ini_param_override(const wchar_t *section,
        const wchar_t *key, wstring *val, CommandList *command_list,
        const wstring *ini_namespace)
{
    ParamOverride *param = new ParamOverride();

    if (!parse_ini_param_name(key, &param->paramIdx, &param->paramComponent))
        goto bail;

    if (!param->expression.Parse(val, ini_namespace, command_list->scope))
        goto bail;

    // Reserve space in IniParams for this variable:
    G->iniParamsReserved = max(G->iniParamsReserved, param->paramIdx + 1);

    param->iniLine = L"[" + wstring(section) + L"] " + wstring(key) + L" = " + *val;
    command_list->commands.push_back(std::shared_ptr<CommandListCommand>(param));
    return true;
bail:
    delete param;
    return false;
}

bool parse_command_list_variable_assignment(const wchar_t *section,
        const wchar_t *key, wstring *val, const wstring *raw_line,
        CommandList *command_list, CommandList *pre_command_list, CommandList *post_command_list,
        const wstring *ini_namespace)
{
    VariableAssignment *command = NULL;
    CommandListVariable *var = NULL;
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

    if (!command->expression.Parse(val, ini_namespace, command_list->scope))
        goto bail;

    command->iniLine = L"[" + wstring(section) + L"] " + wstring(key) + L" = " + *val;
    command_list->commands.push_back(std::shared_ptr<CommandListCommand>(command));
    return true;
bail:
    delete command;
    return false;
}

ResourcePool::~ResourcePool()
{
    ResourcePoolCache::iterator i;

    for (i = cache.begin(); i != cache.end(); i++) {
        if (i->second.first)
            i->second.first->Release();
    }
    cache.clear();
}

void ResourcePool::Emplace(uint32_t hash, ID3D11Resource *resource, ID3D11Device *device)
{
    if (resource)
        resource->AddRef();
    cache.emplace(hash, pair<ID3D11Resource*, ID3D11Device*>{resource, device});
}

template <typename ResourceType,
     typename DescType,
    HRESULT (__stdcall ID3D11Device::*CreateResource)(THIS_
          const DescType *pDesc,
          const D3D11_SUBRESOURCE_DATA *pInitialData,
          ResourceType **ppTexture)
    >
static ResourceType* get_resource_from_pool(
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
    ResourcePoolCache::iterator pool_i;
    ID3D11Device *old_device = NULL;

    // We don't want to use the CalTexture2D/3DDescHash functions because
    // the resolution override could produce the same hash for distinct
    // texture descriptions. This hash isn't exposed to the user, so
    // doesn't matter what we use - just has to be fast.
    hash = crc32c_hw(0, desc, sizeof(DescType));

    pool_i = Profiling::lookup_map(resource_pool->cache, hash, &Profiling::resource_pool_lookup_overhead);
    if (pool_i != resource_pool->cache.end()) {
        resource = (ResourceType*)pool_i->second.first;
        old_device = pool_i->second.second;
        if (!resource)
            return NULL;

        if (old_device == state->origDevice1) {
            if (resource == dst_resource)
                return NULL;

            LOG_DEBUG("Switching cached resource %S\n", ini_line->c_str());
            Profiling::resource_pool_swaps++;
            resource->AddRef();
            return resource;
        }

        LOG_INFO("Device mismatch, discarding %S from resource pool\n", ini_line->c_str());
        resource_pool->cache.erase(pool_i);
        resource->Release();
    }

    LOG_INFO("Creating cached resource %S\n", ini_line->c_str());
    Profiling::resources_created++;

    hr = (state->origDevice1->*CreateResource)(desc, NULL, &resource);
    if (FAILED(hr)) {
        LOG_INFO("Resource copy failed %S: 0x%x\n", ini_line->c_str(), hr);
        LogResourceDesc(desc);
        src_resource->GetDesc(&old_desc);
        LOG_INFO("Original resource was:\n");
        LogResourceDesc(&old_desc);

        // Prevent further attempts:
        resource_pool->Emplace(hash, NULL, NULL);

        return NULL;
    }
    resource_pool->Emplace(hash, resource, state->origDevice1);
    size = resource_pool->cache.size();
    if (size > 1)
        LOG_INFO("  NOTICE: cache now contains %Ii resources\n", size);

    LogDebugResourceDesc(desc);
    return resource;
}

CustomResource::CustomResource() :
    resource(NULL),
    device(NULL),
    view(NULL),
    isNull(true),
    substantiated(false),
    bindFlags((D3D11_BIND_FLAG)0),
    miscFlags((D3D11_RESOURCE_MISC_FLAG)0),
    stride(0),
    offset(0),
    bufSize(0),
    format(DXGI_FORMAT_UNKNOWN),
    maxCopiesPerFrame(0),
    frameNo(0),
    copiesThisFrame(0),
    overrideType(CustomResourceType::INVALID),
    overrideMode(CustomResourceMode::DEFAULT),
    overrideBindFlags(CustomResourceBindFlags::INVALID),
    overrideMiscFlags(ResourceMiscFlags::INVALID),
    overrideFormat((DXGI_FORMAT)-1),
    overrideWidth(-1),
    overrideHeight(-1),
    overrideDepth(-1),
    overrideMips(-1),
    overrideArray(-1),
    overrideMsaa(-1),
    overrideMsaaQuality(-1),
    overrideByteWidth(-1),
    overrideStride(-1),
    widthMultiply(1.0f),
    heightMultiply(1.0f),
    initialData(NULL),
    initialDataSize(0)
{}

CustomResource::~CustomResource()
{
    if (resource)
        resource->Release();
    if (view)
        view->Release();
    free(initialData);
}

bool CustomResource::OverrideSurfaceCreationMode(StereoHandle stereo_handle, NVAPI_STEREO_SURFACECREATEMODE *orig_mode)
{

    if (overrideMode == CustomResourceMode::DEFAULT)
        return false;

    Profiling::NvAPI_Stereo_GetSurfaceCreationMode(stereo_handle, orig_mode);

    switch (overrideMode) {
        case CustomResourceMode::STEREO:
            Profiling::NvAPI_Stereo_SetSurfaceCreationMode(stereo_handle,
                    NVAPI_STEREO_SURFACECREATEMODE_FORCESTEREO);
            return true;
        case CustomResourceMode::MONO:
            Profiling::NvAPI_Stereo_SetSurfaceCreationMode(stereo_handle,
                    NVAPI_STEREO_SURFACECREATEMODE_FORCEMONO);
            return true;
        case CustomResourceMode::AUTO:
            Profiling::NvAPI_Stereo_SetSurfaceCreationMode(stereo_handle,
                    NVAPI_STEREO_SURFACECREATEMODE_AUTO);
            return true;
    }

    return false;
}

void CustomResource::Substantiate(ID3D11Device *orig_device, StereoHandle stereo_handle,
        D3D11_BIND_FLAG bind_flags, D3D11_RESOURCE_MISC_FLAG misc_flags)
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

    Profiling::resources_created++;

    // Add any extra bind flags necessary for the current assignment. This
    // covers many (but not all) of the cases 3DMigoto cannot deduce
    // earlier during parsing - it will cover custom resources copied by
    // reference to other custom resources when the "this" resource target
    // is assigned. There are some complicated cases that could still need
    // bind_flags to be manually specified - where multiple bind flags are
    // required and cannot be deduced at parse time.
    this->bindFlags = (D3D11_BIND_FLAG)(this->bindFlags | bind_flags);
    this->miscFlags = (D3D11_RESOURCE_MISC_FLAG)(this->miscFlags | misc_flags);

    // If the resource section has enough information to create a resource
    // we do so the first time it is loaded from. The reason we do it this
    // late is to make sure we know which device is actually being used to
    // render the game - FC4 creates about a dozen devices with different
    // parameters while probing the hardware before it settles on the one
    // it will actually use.

    LOCK_RESOURCE_CREATION_MODE();

    restore_create_mode = OverrideSurfaceCreationMode(stereo_handle, &orig_mode);

    if (!filename.empty()) {
        LoadFromFile(orig_device);
    } else {
        switch (overrideType) {
            case CustomResourceType::BUFFER:
            case CustomResourceType::STRUCTURED_BUFFER:
            case CustomResourceType::RAW_BUFFER:
                SubstantiateBuffer(orig_device, NULL, 0);
                break;
            case CustomResourceType::TEXTURE1D:
                SubstantiateTexture1D(orig_device);
                break;
            case CustomResourceType::TEXTURE2D:
            case CustomResourceType::CUBE:
                SubstantiateTexture2D(orig_device);
                break;
            case CustomResourceType::TEXTURE3D:
                SubstantiateTexture3D(orig_device);
                break;
        }
    }

    if (restore_create_mode)
        Profiling::NvAPI_Stereo_SetSurfaceCreationMode(stereo_handle, orig_mode);

    UNLOCK_RESOURCE_CREATION_MODE();
}

void CustomResource::LoadBufferFromFile(ID3D11Device *orig_device)
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

    SubstantiateBuffer(orig_device, &buf, size);

out_delete:
    free(buf);
out_close:
    CloseHandle(f);
}

void CustomResource::LoadFromFile(ID3D11Device *orig_device)
{
    wstring ext;
    HRESULT hr;

    switch (overrideType) {
        case CustomResourceType::BUFFER:
        case CustomResourceType::STRUCTURED_BUFFER:
        case CustomResourceType::RAW_BUFFER:
            return LoadBufferFromFile(orig_device);
    }

    // This code path doesn't get a chance to override the resource
    // description, since DirectXTK takes care of that, but we can pass in
    // bind flags at least, which is sometimes necessary in complex
    // situations where 3DMigoto cannot automatically determine these or
    // when manipulating driver heuristics:
    if (overrideBindFlags != CustomResourceBindFlags::INVALID)
        bindFlags = (D3D11_BIND_FLAG)overrideBindFlags;
    if (overrideMiscFlags != ResourceMiscFlags::INVALID)
        miscFlags = (D3D11_RESOURCE_MISC_FLAG)overrideMiscFlags;

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
        LOG_INFO_W(L"Loading custom resource %s as DDS, bind_flags=0x%03x\n", filename.c_str(), bindFlags);
        hr = DirectX::CreateDDSTextureFromFileEx(orig_device,
                filename.c_str(), 0,
                D3D11_USAGE_DEFAULT, bindFlags, 0, miscFlags,
                false, &resource, NULL, NULL);
    } else {
        LOG_INFO_W(L"Loading custom resource %s as WIC, bind_flags=0x%03x\n", filename.c_str(), bindFlags);
        hr = DirectX::CreateWICTextureFromFileEx(orig_device,
                filename.c_str(), 0,
                D3D11_USAGE_DEFAULT, bindFlags, 0, miscFlags,
                false, &resource, NULL);
    }
    if (SUCCEEDED(hr)) {
        device = orig_device;
        isNull = false;
        // TODO:
        // format = ...
    } else
        LogOverlay(LOG_WARNING, "Failed to load custom texture resource %S: 0x%x\n", filename.c_str(), hr);
}

void CustomResource::SubstantiateBuffer(ID3D11Device *orig_device, void **buf, DWORD size)
{
    D3D11_SUBRESOURCE_DATA data = {}, *pInitialData = NULL;
    ID3D11Buffer *buffer;
    D3D11_BUFFER_DESC desc;
    HRESULT hr;

    if (!buf) {
        // If no file is passed in, we use the optional initial data to
        // initialise the buffer. We do this even if no initial data
        // has been specified, so that the buffer will be initialised
        // with zeroes for safety.
        buf = &initialData;
        size = (DWORD)initialDataSize;
    }

    memset(&desc, 0, sizeof(desc));
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = bindFlags;
    desc.MiscFlags = miscFlags;

    // Allow the buffer size to be set from the file / initial data size,
    // but it can be overridden if specified explicitly. If it's a
    // structured buffer, we assume just a single structure by default, but
    // again this can be overridden. The reason for doing this here, and
    // not in OverrideBufferDesc, is that this only applies if we are
    // substantiating the resource from scratch, not when copying a resource.
    if (size) {
        desc.ByteWidth = size;
        if (overrideType == CustomResourceType::STRUCTURED_BUFFER)
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
                LOG_INFO("Out of memory enlarging buffer: [%S]\n", name.c_str());
                return;
            }
            memset((char*)new_buf + size, 0, desc.ByteWidth - size);
            *buf = new_buf;
        }

        data.pSysMem = *buf;
        pInitialData = &data;
    }

    hr = orig_device->CreateBuffer(&desc, pInitialData, &buffer);
    if (SUCCEEDED(hr)) {
        LOG_INFO("Substantiated custom %S [%S], bind_flags=0x%03x\n",
                lookup_enum_name(CustomResourceTypeNames, overrideType), name.c_str(), desc.BindFlags);
        LogDebugResourceDesc(&desc);
        resource = (ID3D11Resource*)buffer;
        device = orig_device;
        isNull = false;
        OverrideOutOfBandInfo(&format, &stride);
    } else {
        LogOverlay(LOG_NOTICE, "Failed to substantiate custom %S [%S]: 0x%x\n",
                lookup_enum_name(CustomResourceTypeNames, overrideType), name.c_str(), hr);
        LogResourceDesc(&desc);
    }
}
void CustomResource::SubstantiateTexture1D(ID3D11Device *orig_device)
{
    ID3D11Texture1D *tex1d;
    D3D11_TEXTURE1D_DESC desc;
    HRESULT hr;

    memset(&desc, 0, sizeof(desc));
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = bindFlags;
    desc.MiscFlags = miscFlags;
    OverrideTexDesc(&desc);

    hr = orig_device->CreateTexture1D(&desc, NULL, &tex1d);
    if (SUCCEEDED(hr)) {
        LOG_INFO("Substantiated custom %S [%S], bind_flags=0x%03x\n",
                lookup_enum_name(CustomResourceTypeNames, overrideType), name.c_str(), desc.BindFlags);
        LogDebugResourceDesc(&desc);
        resource = (ID3D11Resource*)tex1d;
        device = orig_device;
        isNull = false;
    } else {
        LogOverlay(LOG_NOTICE, "Failed to substantiate custom %S [%S]: 0x%x\n",
                lookup_enum_name(CustomResourceTypeNames, overrideType), name.c_str(), hr);
        LogResourceDesc(&desc);
    }
}
void CustomResource::SubstantiateTexture2D(ID3D11Device *orig_device)
{
    ID3D11Texture2D *tex2d;
    D3D11_TEXTURE2D_DESC desc;
    HRESULT hr;

    memset(&desc, 0, sizeof(desc));
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = bindFlags;
    desc.MiscFlags = miscFlags;
    OverrideTexDesc(&desc);

    hr = orig_device->CreateTexture2D(&desc, NULL, &tex2d);
    if (SUCCEEDED(hr)) {
        LOG_INFO("Substantiated custom %S [%S], bind_flags=0x%03x\n",
                lookup_enum_name(CustomResourceTypeNames, overrideType), name.c_str(), desc.BindFlags);
        LogDebugResourceDesc(&desc);
        resource = (ID3D11Resource*)tex2d;
        device = orig_device;
        isNull = false;
    } else {
        LogOverlay(LOG_NOTICE, "Failed to substantiate custom %S [%S]: 0x%x\n",
                lookup_enum_name(CustomResourceTypeNames, overrideType), name.c_str(), hr);
        LogResourceDesc(&desc);
    }
}
void CustomResource::SubstantiateTexture3D(ID3D11Device *orig_device)
{
    ID3D11Texture3D *tex3d;
    D3D11_TEXTURE3D_DESC desc;
    HRESULT hr;

    memset(&desc, 0, sizeof(desc));
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = bindFlags;
    desc.MiscFlags = miscFlags;
    OverrideTexDesc(&desc);

    hr = orig_device->CreateTexture3D(&desc, NULL, &tex3d);
    if (SUCCEEDED(hr)) {
        LOG_INFO("Substantiated custom %S [%S], bind_flags=0x%03x\n",
                lookup_enum_name(CustomResourceTypeNames, overrideType), name.c_str(), desc.BindFlags);
        LogDebugResourceDesc(&desc);
        resource = (ID3D11Resource*)tex3d;
        device = orig_device;
        isNull = false;
    } else {
        LogOverlay(LOG_NOTICE, "Failed to substantiate custom %S [%S]: 0x%x\n",
                lookup_enum_name(CustomResourceTypeNames, overrideType), name.c_str(), hr);
        LogResourceDesc(&desc);
    }
}

void CustomResource::OverrideBufferDesc(D3D11_BUFFER_DESC *desc)
{
    switch (overrideType) {
        case CustomResourceType::STRUCTURED_BUFFER:
            desc->MiscFlags |= D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
            break;
        case CustomResourceType::RAW_BUFFER:
            desc->MiscFlags |= D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;
            break;
    }

    if (overrideStride != -1)
        desc->StructureByteStride = overrideStride;
    else if (overrideFormat != (DXGI_FORMAT)-1 && overrideFormat != DXGI_FORMAT_UNKNOWN)
        desc->StructureByteStride = dxgi_format_size(overrideFormat);

    if (overrideByteWidth != -1)
        desc->ByteWidth = overrideByteWidth;
    else if (overrideArray != -1)
        desc->ByteWidth = desc->StructureByteStride * overrideArray;

    if (overrideBindFlags != CustomResourceBindFlags::INVALID)
        desc->BindFlags = (D3D11_BIND_FLAG)overrideBindFlags;
    if (overrideMiscFlags != ResourceMiscFlags::INVALID)
        desc->MiscFlags = (D3D11_RESOURCE_MISC_FLAG)overrideMiscFlags;
}

void CustomResource::OverrideTexDesc(D3D11_TEXTURE1D_DESC *desc)
{
    if (overrideWidth != -1)
        desc->Width = overrideWidth;
    if (overrideMips != -1)
        desc->MipLevels = overrideMips;
    if (overrideArray != -1)
        desc->ArraySize = overrideArray;
    if (overrideFormat != (DXGI_FORMAT)-1)
        desc->Format = overrideFormat;

    desc->Width = (UINT)(desc->Width * widthMultiply);

    if (overrideBindFlags != CustomResourceBindFlags::INVALID)
        desc->BindFlags = (D3D11_BIND_FLAG)overrideBindFlags;
    if (overrideMiscFlags != ResourceMiscFlags::INVALID)
        desc->MiscFlags = (D3D11_RESOURCE_MISC_FLAG)overrideMiscFlags;
}

void CustomResource::OverrideTexDesc(D3D11_TEXTURE2D_DESC *desc)
{
    if (overrideWidth != -1)
        desc->Width = overrideWidth;
    if (overrideHeight != -1)
        desc->Height = overrideHeight;
    if (overrideMips != -1)
        desc->MipLevels = overrideMips;
    if (overrideFormat != (DXGI_FORMAT)-1)
        desc->Format = overrideFormat;
    if (overrideArray != -1)
        desc->ArraySize = overrideArray;
    if (overrideMsaa != -1)
        desc->SampleDesc.Count = overrideMsaa;
    if (overrideMsaaQuality != -1)
        desc->SampleDesc.Quality = overrideMsaaQuality;

    if (overrideType == CustomResourceType::CUBE) {
        desc->MiscFlags |= D3D11_RESOURCE_MISC_TEXTURECUBE;
        if (overrideArray != -1)
            desc->ArraySize = overrideArray * 6;
    }

    desc->Width = (UINT)(desc->Width * widthMultiply);
    desc->Height = (UINT)(desc->Height * heightMultiply);

    if (overrideBindFlags != CustomResourceBindFlags::INVALID)
        desc->BindFlags = (D3D11_BIND_FLAG)overrideBindFlags;
    if (overrideMiscFlags != ResourceMiscFlags::INVALID)
        desc->MiscFlags = (D3D11_RESOURCE_MISC_FLAG)overrideMiscFlags;
}

void CustomResource::OverrideTexDesc(D3D11_TEXTURE3D_DESC *desc)
{
    if (overrideWidth != -1)
        desc->Width = overrideWidth;
    if (overrideHeight != -1)
        desc->Height = overrideHeight;
    if (overrideDepth != -1)
        desc->Height = overrideDepth;
    if (overrideMips != -1)
        desc->MipLevels = overrideMips;
    if (overrideFormat != (DXGI_FORMAT)-1)
        desc->Format = overrideFormat;

    desc->Width = (UINT)(desc->Width * widthMultiply);
    desc->Height = (UINT)(desc->Height * heightMultiply);

    if (overrideBindFlags != CustomResourceBindFlags::INVALID)
        desc->BindFlags = (D3D11_BIND_FLAG)overrideBindFlags;
    if (overrideMiscFlags != ResourceMiscFlags::INVALID)
        desc->MiscFlags = (D3D11_RESOURCE_MISC_FLAG)overrideMiscFlags;
}

void CustomResource::OverrideOutOfBandInfo(DXGI_FORMAT *format, UINT *stride)
{
    if (overrideFormat != (DXGI_FORMAT)-1)
        *format = overrideFormat;
    if (overrideStride != -1)
        *stride = overrideStride;
}

// Returns 1 for definite dsv formats
// Returns 2 for typeless variants of dsv formats (have a stencil buffer)
// Returns -1 for possible typecast depth formats, but with no stencil buffer they may not be
static int is_dsv_format(DXGI_FORMAT fmt)
{
    switch(fmt)
    {
        case DXGI_FORMAT_D32_FLOAT_S8X24_UINT:
        case DXGI_FORMAT_D32_FLOAT:
        case DXGI_FORMAT_D24_UNORM_S8_UINT:
        case DXGI_FORMAT_D16_UNORM:
            return 1;
        case DXGI_FORMAT_R32G8X24_TYPELESS:
        case DXGI_FORMAT_R32_FLOAT_X8X24_TYPELESS:
        case DXGI_FORMAT_X32_TYPELESS_G8X24_UINT:
        case DXGI_FORMAT_R24G8_TYPELESS:
        case DXGI_FORMAT_R24_UNORM_X8_TYPELESS:
        case DXGI_FORMAT_X24_TYPELESS_G8_UINT:
            return 2;
        case DXGI_FORMAT_R32_TYPELESS:
        case DXGI_FORMAT_R32_FLOAT:
        case DXGI_FORMAT_R16_TYPELESS:
        case DXGI_FORMAT_R16_UNORM:
            return -1;
        default:
            return 0;
    }
}


// Transfer a resource from one device to another. First came across this
// possibility in DOAXVV where showing the news creates a new temporary device
// & context, and due to another bug [Constants] would be re-run, which allowed
// certain mods to copy one resource to another crashing the game. For that
// game we want to disallow re-running [Constants], but the possibility of
// running into more of these issues remains, particularly given that many
// games tend to create & destroy devices on startup where we would actually
// prefer the final device to own resources (in contrast to DOAXVV where we
// want the first device to remain the owner). This attempts to transfer
// resources between devices on demand so that whichever device is currently in
// use will own the resources, however this is very slow and if it ever crops
// up in a fast path we'd need to look for ways to avoid this.
//
// This does not handle all edge cases (depth buffers, dynamic destinations and
// MSAA are not supported by UpdateSubresource), and knowing DirectX there will
// probably be more undocumented surprise combinations that don't work either.
//
// TODO: Refactor this - the four cases are very similar with some differences
//
static ID3D11Resource * inter_device_resource_transfer(ID3D11Device *dst_dev, ID3D11DeviceContext *dst_ctx, ID3D11Resource *src_res, wstring *name)
{
    ID3D11Device *src_dev = NULL;
    ID3D11DeviceContext *src_ctx = NULL;
    ID3D11Resource *stg_res = NULL;
    ID3D11Resource *dtg_res = NULL;
    ID3D11Resource *dst_res = NULL;
    D3D11_RESOURCE_DIMENSION dimension;
    D3D11_MAPPED_SUBRESOURCE src_map;
    UINT item, level, index;
    const char *reason = "";

    Profiling::inter_device_copies++;

    // Not really anything sensible we can do with the resource creation
    // mode in the general case here as yet - if we copy via the CPU we
    // will lose the 2nd perspective, but we have no way to even know if
    // one even exists. There are some limited situations we could copy a
    // stereo texture between devices (reverse stereo blit -> stereo
    // header), but let's wait until we have a proven need before we try
    // anything heroic that probably won't work anyway. Tbh we might be
    // better off just trying to avoid getting down this code path - maybe
    // just discarding resources for re-substantiation and re-running
    // Contants will be sufficient in most cases
    LOCK_RESOURCE_CREATION_MODE();

    src_res->GetDevice(&src_dev);
    reason = "Source device unavailable\n";
    if (!src_dev)
        goto err;

    src_dev->GetImmediateContext(&src_ctx);
    reason = "Source context unavailable\n";
    if (!src_ctx)
        goto err;

    src_res->GetType(&dimension);
    switch (dimension) {
        case D3D11_RESOURCE_DIMENSION_BUFFER:
        {
            ID3D11Buffer *buf = (ID3D11Buffer*)src_res;
            D3D11_BUFFER_DESC buf_desc;
            buf->GetDesc(&buf_desc);

            reason = "Dynamic Buffers unsupported\n";
            if (buf_desc.Usage == D3D11_USAGE_DYNAMIC)
                goto err;
            if (buf_desc.Usage == D3D11_USAGE_IMMUTABLE)
                buf_desc.Usage = D3D11_USAGE_DEFAULT;

            dst_dev->CreateBuffer(&buf_desc, NULL, (ID3D11Buffer**)&dst_res);
            if (!dst_res) {
                reason = "Error creating final destination Buffer\n";
                LogResourceDesc(&buf_desc);
                goto err;
            }

            buf_desc.Usage = D3D11_USAGE_STAGING;
            buf_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            buf_desc.BindFlags = 0;
            src_dev->CreateBuffer(&buf_desc, NULL, (ID3D11Buffer**)&stg_res);
            if (!stg_res) {
                reason = "Error creating source staging Buffer\n";
                LogResourceDesc(&buf_desc);
                goto err;
            }
            src_ctx->CopyResource(stg_res, src_res);

            // Buffers only have a single subresource:
            index = 0;
            reason = "Error mapping source staging Buffer\n";
            if (FAILED(src_ctx->Map(stg_res, index, D3D11_MAP_READ, 0, &src_map)))
                goto err;
            dst_ctx->UpdateSubresource(dst_res, index, NULL, src_map.pData, src_map.RowPitch, src_map.DepthPitch);
            src_ctx->Unmap(stg_res, index);
            break;
        }
        case D3D11_RESOURCE_DIMENSION_TEXTURE1D:
        {
            ID3D11Texture1D *tex1d = (ID3D11Texture1D*)src_res;
            D3D11_TEXTURE1D_DESC tex1d_desc;
            tex1d->GetDesc(&tex1d_desc);

            reason = "Dynamic Texture1Ds unsupported\n";
            if (tex1d_desc.Usage == D3D11_USAGE_DYNAMIC)
                goto err;
            if (tex1d_desc.Usage == D3D11_USAGE_IMMUTABLE)
                tex1d_desc.Usage = D3D11_USAGE_DEFAULT;

            // It's not clear to me from the UpdateSubresource documentation if the
            // reason depth/stencil buffers aren't supported is down to the bind flags,
            // or the format... For now, erring on the side of throwing an error either
            // way, but trying to continue if it is just the format
            if ((tex1d_desc.BindFlags & D3D11_BIND_DEPTH_STENCIL)) {
                reason = "depth/stencil buffers unsupported";
                goto err;
            }
            if (is_dsv_format(tex1d_desc.Format) > 0) {
                LogOverlay(LOG_NOTICE, "Inter-device transfer of [%S] with depth/stencil format %s may or may not work. Please report success/failure.\n",
                        name->c_str(), tex_format_str(tex1d_desc.Format));
            }

            dst_dev->CreateTexture1D(&tex1d_desc, NULL, (ID3D11Texture1D**)&dst_res);
            if (!dst_res) {
                reason = "Error creating final destination Texture1D\n";
                LogResourceDesc(&tex1d_desc);
                goto err;
            }

            tex1d_desc.Usage = D3D11_USAGE_STAGING;
            tex1d_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            tex1d_desc.BindFlags = 0;
            tex1d_desc.MiscFlags &= ~D3D11_RESOURCE_MISC_GENERATE_MIPS;
            src_dev->CreateTexture1D(&tex1d_desc, NULL, (ID3D11Texture1D**)&stg_res);
            if (!stg_res) {
                reason = "Error creating staging Texture1D\n";
                LogResourceDesc(&tex1d_desc);
                goto err;
            }
            src_ctx->CopyResource(stg_res, src_res);

            for (item = 0; item < tex1d_desc.ArraySize; item++) {
                for (level = 0; level < tex1d_desc.MipLevels; level++) {
                    index = D3D11CalcSubresource(level, item, max(tex1d_desc.MipLevels, 1));
                    reason = "Error mapping source staging Texture1D\n";
                    if (FAILED(src_ctx->Map(stg_res, index, D3D11_MAP_READ, 0, &src_map)))
                        goto err;
                    dst_ctx->UpdateSubresource(dst_res, index, NULL, src_map.pData, src_map.RowPitch, src_map.DepthPitch);
                    src_ctx->Unmap(stg_res, index);
                }
            }
            break;
        }
        case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
        {
            ID3D11Texture2D *tex2d = (ID3D11Texture2D*)src_res;
            D3D11_TEXTURE2D_DESC tex2d_desc;
            tex2d->GetDesc(&tex2d_desc);

            reason = "Dynamic Texture2Ds unsupported\n";
            if (tex2d_desc.Usage == D3D11_USAGE_DYNAMIC)
                goto err;
            if (tex2d_desc.Usage == D3D11_USAGE_IMMUTABLE)
                tex2d_desc.Usage = D3D11_USAGE_DEFAULT;

            if (tex2d_desc.SampleDesc.Count > 1) {
                // Not sure how to handle this one - I'd be surprised if it works for map/unmap
                // and while we can resolve MSAA (for supported formats) on the source device,
                // what do we do to restore the lost samples on the destination?
                reason = "MSAA resources unsupported";
                goto err;
            }

            // It's not clear to me from the UpdateSubresource documentation if the
            // reason depth/stencil buffers aren't supported is down to the bind flags,
            // or the format... For now, erring on the side of throwing an error either
            // way, but trying to continue if it is just the format
            if ((tex2d_desc.BindFlags & D3D11_BIND_DEPTH_STENCIL)) {
                reason = "depth/stencil buffers unsupported";
                goto err;
            }
            if (is_dsv_format(tex2d_desc.Format) > 0) {
                LogOverlay(LOG_NOTICE, "Inter-device transfer of [%S] with depth/stencil format %s may or may not work. Please report success/failure.\n",
                        name->c_str(), tex_format_str(tex2d_desc.Format));
            }

            dst_dev->CreateTexture2D(&tex2d_desc, NULL, (ID3D11Texture2D**)&dst_res);
            if (!dst_res) {
                reason = "Error creating final destination Texture2D\n";
                LogResourceDesc(&tex2d_desc);
                goto err;
            }

            tex2d_desc.Usage = D3D11_USAGE_STAGING;
            tex2d_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            tex2d_desc.BindFlags = 0;
            tex2d_desc.MiscFlags &= ~D3D11_RESOURCE_MISC_GENERATE_MIPS;
            src_dev->CreateTexture2D(&tex2d_desc, NULL, (ID3D11Texture2D**)&stg_res);
            if (!stg_res) {
                reason = "Error creating staging Texture2D\n";
                LogResourceDesc(&tex2d_desc);
                goto err;
            }
            src_ctx->CopyResource(stg_res, src_res);

            for (item = 0; item < tex2d_desc.ArraySize; item++) {
                for (level = 0; level < tex2d_desc.MipLevels; level++) {
                    index = D3D11CalcSubresource(level, item, max(tex2d_desc.MipLevels, 1));
                    reason = "Error mapping source staging Texture2D\n";
                    if (FAILED(src_ctx->Map(stg_res, index, D3D11_MAP_READ, 0, &src_map)))
                        goto err;
                    dst_ctx->UpdateSubresource(dst_res, index, NULL, src_map.pData, src_map.RowPitch, src_map.DepthPitch);
                    src_ctx->Unmap(stg_res, index);
                }
            }
            break;
        }
        case D3D11_RESOURCE_DIMENSION_TEXTURE3D:
        {
            ID3D11Texture3D *tex3d = (ID3D11Texture3D*)src_res;
            D3D11_TEXTURE3D_DESC tex3d_desc;
            tex3d->GetDesc(&tex3d_desc);

            reason = "Dynamic Texture3Ds unsupported\n";
            if (tex3d_desc.Usage == D3D11_USAGE_DYNAMIC)
                goto err;
            if (tex3d_desc.Usage == D3D11_USAGE_IMMUTABLE)
                tex3d_desc.Usage = D3D11_USAGE_DEFAULT;

            dst_dev->CreateTexture3D(&tex3d_desc, NULL, (ID3D11Texture3D**)&dst_res);
            if (!dst_res) {
                reason = "Error creating final destination Texture3D\n";
                LogResourceDesc(&tex3d_desc);
                goto err;
            }

            tex3d_desc.Usage = D3D11_USAGE_STAGING;
            tex3d_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
            tex3d_desc.BindFlags = 0;
            tex3d_desc.MiscFlags &= ~D3D11_RESOURCE_MISC_GENERATE_MIPS;
            src_dev->CreateTexture3D(&tex3d_desc, NULL, (ID3D11Texture3D**)&stg_res);
            if (!stg_res) {
                reason = "Error creating staging Texture3D\n";
                LogResourceDesc(&tex3d_desc);
                goto err;
            }
            src_ctx->CopyResource(stg_res, src_res);

            for (level = 0; level < tex3d_desc.MipLevels; level++) {
                // 3D Textures cannot be arrays, so only mip-map level counts for subresource index:
                index = level;
                reason = "Error mapping source staging Texture3D\n";
                if (FAILED(src_ctx->Map(stg_res, index, D3D11_MAP_READ, 0, &src_map)))
                    goto err;
                dst_ctx->UpdateSubresource(dst_res, index, NULL, src_map.pData, src_map.RowPitch, src_map.DepthPitch);
                src_ctx->Unmap(stg_res, index);
            }
            break;
        }
        default:
            reason = "Unknown dimension\n";
            goto err;
    }

out:
    if (stg_res)
        stg_res->Release();
    if (src_ctx)
        src_ctx->Release();
    if (src_dev)
        src_dev->Release();
    UNLOCK_RESOURCE_CREATION_MODE();
    return dst_res;

err:
    LogOverlay(LOG_DIRE, "Inter-device transfer of [%S] failed: %s\n", name->c_str(), reason);
    if (dst_res)
        dst_res->Release();
    dst_res = NULL;
    goto out;
}

void CustomResource::Expire(ID3D11Device *orig_device, ID3D11DeviceContext *orig_context)
{
    ID3D11Resource *new_resource = NULL;

    if (!resource || isNull)
        return;

    // Check for device mismatches and handle via expiring cached
    // resources, flagging for re-substantiation and/or performing
    // inter-device transfers if necessary. We cache the device rather than
    // querying it from the resource via GetDevice(), because if ReShade is
    // in use GetDevice() will return the real device, but we will compare
    // it with the ReShade device and think there has been a device swap
    // when there has not been.
    if (device == orig_device)
        return;

    // Attempt to transfer resource to new device by staging to the CPU and
    // back. Rather slow, but ensures the contents are up to date.
    // TODO: Search other custom resources for references to this resource
    //       and update those. Not urgent as yet, but will be important if
    //       we allow the === and !== operators to check if one custom
    //       resource matches another.
    // TODO: Track if resource is dirty (used as render/depth target/UAV,
    //       ever assigned or copied to) and re-substantiate if clean.
    // TODO: Use sharable resources to skip copy back to CPU (limited to 2D
    //       non-mip-mapped resources)
    // TODO: Option to skip inter-device copies instead of transfer
    // TODO: Option to discard individual resources instead of transfer
    // TODO: Option to discard all custom resources and re-run [Constants]
    //       on new device (for convoluted startup sequences)
    LOG_INFO("Device mismatch, transferring [%S] to new device\n", name.c_str());
    new_resource = inter_device_resource_transfer(
            orig_device, orig_context, resource, &name);

    // Expire cache:
    resource->Release();
    if (view)
        view->Release();
    view = NULL;

    if (new_resource) {
        // Inter-device copy succeeded, switch to the new resource:
        resource = new_resource;
        device = orig_device;
    } else {
        // Inter-device copy failed / skipped. Flag resource for
        // re-substantiation (if possible for this resource):
        substantiated = false;
        resource = NULL;
        device = NULL;
        isNull = true;
    }
}

bool ResourceCopyTarget::ParseTarget(const wchar_t *target,
        bool is_source, const wstring *ini_namespace)
{
    int ret, len;
    size_t length = wcslen(target);
    CustomResources::iterator res;

    ret = swscanf_s(target, L"%lcs-cb%u%n", &shaderType, 1, &slot, &len);
    if (ret == 2 && len == length && slot < D3D11_COMMONSHADER_CONSTANT_BUFFER_API_SLOT_COUNT) {
        type = ResourceCopyTargetType::CONSTANT_BUFFER;
        goto check_shader_type;
    }

    ret = swscanf_s(target, L"%lcs-t%u%n", &shaderType, 1, &slot, &len);
    if (ret == 2 && len == length && slot < D3D11_COMMONSHADER_INPUT_RESOURCE_SLOT_COUNT) {
        type = ResourceCopyTargetType::SHADER_RESOURCE;
           goto check_shader_type;
    }

    // TODO: ret = swscanf_s(target, L"%lcs-s%u%n", &shader_type, 1, &slot, &len);
    // TODO: if (ret == 2 && len == length && slot < D3D11_COMMONSHADER_SAMPLER_SLOT_COUNT) {
    // TODO:     type = ResourceCopyTargetType::SAMPLER;
    // TODO:    goto check_shader_type;
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

    ret = swscanf_s(target, L"%lcs-u%u%n", &shaderType, 1, &slot, &len);
    // XXX: On Win8 D3D11_1_UAV_SLOT_COUNT (64) is the limit instead. Use
    // the lower amount for now to enforce compatibility.
    if (ret == 2 && len == length && slot < D3D11_PS_CS_UAV_REGISTER_COUNT) {
        // These views are only valid for pixel and compute shaders:
        if (shaderType == L'p' || shaderType == L'c') {
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
        wstring namespaced_section;

        res = custom_resources.end();
        if (get_namespaced_section_name_lower(&resource_id, ini_namespace, &namespaced_section))
            res = custom_resources.find(namespaced_section);
        if (res == custom_resources.end())
            res = custom_resources.find(resource_id);
        if (res == custom_resources.end())
            return false;

        customResource = &res->second;
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

    if (!wcscmp(target, L"this")) {
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
        forbidViewCache = true;
        return true;
    }

    if (is_source && !wcscmp(target, L"r_bb")) {
        type = ResourceCopyTargetType::REAL_SWAP_CHAIN;
        // Holding a reference on the back buffer will prevent
        // ResizeBuffers() from working, so forbid caching any views of
        // the back buffer. Leaving it bound could also be a problem,
        // but since this is usually only used from custom shader
        // sections they will take care of unbinding it automatically:
        forbidViewCache = true;
        return true;
    }

    if (is_source && !wcscmp(target, L"f_bb")) {
        type = ResourceCopyTargetType::FAKE_SWAP_CHAIN;
        // Holding a reference on the back buffer will prevent
        // ResizeBuffers() from working, so forbid caching any views of
        // the back buffer. Leaving it bound could also be a problem,
        // but since this is usually only used from custom shader
        // sections they will take care of unbinding it automatically:
        forbidViewCache = true;
        return true;
    }

    return false;

check_shader_type:
    switch(shaderType) {
        case L'v': case L'h': case L'd': case L'g': case L'p': case L'c':
            return true;
    }
    return false;
}


bool parse_command_list_resource_copy_directive(const wchar_t *section,
        const wchar_t *key, wstring *val, CommandList *command_list,
        const wstring *ini_namespace)
{
    ResourceCopyOperation *operation = new ResourceCopyOperation();
    wchar_t buf[MAX_PATH];
    wchar_t *src_ptr = NULL;
    D3D11_RESOURCE_MISC_FLAG misc_flags = (D3D11_RESOURCE_MISC_FLAG)0;

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
    // be used in practice, so for now this will do. Update: We now
    // or in the bind flags in use when the custom resource is first
    // Substantiate()ed, which will cover most of these missing cases - the
    // remaining exceptions would be where differing bind flags are used at
    // different times and we still can't deduce it here
    // FIXME: The constant buffer bind flag can't be combined with others
    if (operation->src.type == ResourceCopyTargetType::CUSTOM_RESOURCE &&
            (operation->options & ResourceCopyOptions::REFERENCE)) {
        // Fucking C++ making this line 3x longer than it should be:
        operation->src.customResource->bindFlags = (D3D11_BIND_FLAG)
            (operation->src.customResource->bindFlags | operation->dst.BindFlags(NULL, &misc_flags));
        operation->src.customResource->miscFlags = (D3D11_RESOURCE_MISC_FLAG)
            (operation->src.customResource->miscFlags | misc_flags);
    }

    operation->iniLine = L"[" + wstring(section) + L"] " + wstring(key) + L" = " + *val;
    command_list->commands.push_back(std::shared_ptr<CommandListCommand>(operation));
    return true;
bail:
    delete operation;
    return false;
}

static bool parse_if_command(const wchar_t *section, const wstring *line,
        CommandList *pre_command_list, CommandList *post_command_list,
        const wstring *ini_namespace)
{
    IfCommand *operation = new IfCommand(section);
    wstring expression = line->substr(line->find_first_not_of(L" \t", 3));

    if (!operation->expression.Parse(&expression, ini_namespace, pre_command_list->scope))
        goto bail;

    // New scope level to isolate local variables:
    pre_command_list->scope->emplace_front();

    return add_command_to_list(operation, NULL, NULL, pre_command_list, post_command_list, section, line->c_str(), NULL);
bail:
    delete operation;
    return false;
}

static bool parse_else_if_command(const wchar_t *section, const wstring *line, int prefix,
        CommandList *pre_command_list, CommandList *post_command_list,
        const wstring *ini_namespace)
{
    ElseIfCommand *operation = new ElseIfCommand(section);
    wstring expression = line->substr(line->find_first_not_of(L" \t", prefix));

    if (!operation->expression.Parse(&expression, ini_namespace, pre_command_list->scope))
        goto bail;

    // Clear deepest scope level to isolate local variables:
    pre_command_list->scope->front().clear();

    // "else if" is implemented by nesting another if/endif inside the
    // parent if command's else clause. We add both an ElsePlaceholder and
    // an ElseIfCommand here, and will fix up the "endif" balance later.
    add_command_to_list(new ElsePlaceholder(), NULL, NULL, pre_command_list, post_command_list, section, line->c_str(), NULL);
    return add_command_to_list(operation, NULL, NULL, pre_command_list, post_command_list, section, line->c_str(), NULL);
bail:
    delete operation;
    return false;
}

static bool parse_else_command(const wchar_t *section,
        CommandList *pre_command_list, CommandList *post_command_list)
{
    // Clear deepest scope level to isolate local variables:
    pre_command_list->scope->front().clear();

    return add_command_to_list(new ElsePlaceholder(), NULL, NULL, pre_command_list, post_command_list, section, L"else", NULL);
}

static bool _parse_end_if_command(const wchar_t *section,
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
            if (post && !if_command->postFinalised) {
                // C++ gotcha: reverse_iterator::base() points to the *next* element
                if_command->trueCommandsPost->commands.assign(rit.base(), else_pos);
                if_command->trueCommandsPost->iniSection = if_command->iniLine;
                if (else_pos != command_list->commands.end()) {
                    // Discard the else placeholder command:
                    if_command->falseCommandsPost->commands.assign(else_pos + 1, command_list->commands.end());
                    if_command->falseCommandsPost->iniSection = if_command->iniLine + L" <else>";
                }
                command_list->commands.erase(rit.base(), command_list->commands.end());
                if_command->postFinalised = true;
                if_command->hasNestedElseIf = has_nested_else_if;
                if (else_if_command)
                    return _parse_end_if_command(section, command_list, post, true);
                return true;
            } else if (!post && !if_command->preFinalised) {
                // C++ gotcha: reverse_iterator::base() points to the *next* element
                if_command->trueCommandsPre->commands.assign(rit.base(), else_pos);
                if_command->trueCommandsPre->iniSection = if_command->iniLine;
                if (else_pos != command_list->commands.end()) {
                    // Discard the else placeholder command:
                    if_command->falseCommandsPre->commands.assign(else_pos + 1, command_list->commands.end());
                    if_command->falseCommandsPre->iniSection = if_command->iniLine + L" <else>";
                }
                command_list->commands.erase(rit.base(), command_list->commands.end());
                if_command->preFinalised = true;
                if_command->hasNestedElseIf = has_nested_else_if;
                if (else_if_command)
                    return _parse_end_if_command(section, command_list, post, true);
                return true;
            }
        }
    }

    LogOverlay(LOG_WARNING, "WARNING: [%S] endif missing if\n", section);
    return false;
}

static bool parse_end_if_command(const wchar_t *section,
        CommandList *pre_command_list, CommandList *post_command_list)
{
    bool ret;

    ret = _parse_end_if_command(section, pre_command_list, false);
    if (post_command_list)
        ret = ret && _parse_end_if_command(section, post_command_list, true);

    if (ret)
        pre_command_list->scope->pop_front();

    return ret;
}

bool parse_command_list_flow_control(const wchar_t *section, const wstring *line,
        CommandList *pre_command_list, CommandList *post_command_list,
        const wstring *ini_namespace)
{
    if (!wcsncmp(line->c_str(), L"if ", 3))
        return parse_if_command(section, line, pre_command_list, post_command_list, ini_namespace);
    if (!wcsncmp(line->c_str(), L"elif ", 5))
        return parse_else_if_command(section, line, 5, pre_command_list, post_command_list, ini_namespace);
    if (!wcsncmp(line->c_str(), L"else if ", 8))
        return parse_else_if_command(section, line, 8, pre_command_list, post_command_list, ini_namespace);
    if (!wcscmp(line->c_str(), L"else"))
        return parse_else_command(section, pre_command_list, post_command_list);
    if (!wcscmp(line->c_str(), L"endif"))
        return parse_end_if_command(section, pre_command_list, post_command_list);

    return false;
}

IfCommand::IfCommand(const wchar_t *section) :
    preFinalised(false),
    postFinalised(false),
    hasNestedElseIf(false),
    section(section)
{
    trueCommandsPre = std::make_shared<CommandList>();
    trueCommandsPost = std::make_shared<CommandList>();
    falseCommandsPre = std::make_shared<CommandList>();
    falseCommandsPost = std::make_shared<CommandList>();
    trueCommandsPost->post = true;
    falseCommandsPost->post = true;

    // Placeholder names to be replaced by endif processing - we should
    // never see these, but in case they do show up somewhere these will
    // provide a clue as to what they are:
    trueCommandsPre->iniSection = L"if placeholder";
    trueCommandsPost->iniSection = L"if placeholder";
    falseCommandsPre->iniSection = L"else placeholder";
    falseCommandsPost->iniSection = L"else placeholder";

    // Place the dynamically allocated command lists in this data structure
    // to ensure they stay alive until after the optimisation stage, even
    // if the IfCommand is freed, e.g. by being optimised out:
    dynamically_allocated_command_lists.push_back(trueCommandsPre);
    dynamically_allocated_command_lists.push_back(trueCommandsPost);
    dynamically_allocated_command_lists.push_back(falseCommandsPre);
    dynamically_allocated_command_lists.push_back(falseCommandsPost);

    // And register these command lists for later optimisation:
    registered_command_lists.push_back(trueCommandsPre.get());
    registered_command_lists.push_back(trueCommandsPost.get());
    registered_command_lists.push_back(falseCommandsPre.get());
    registered_command_lists.push_back(falseCommandsPost.get());
}

void IfCommand::Run(CommandListState *state)
{
    if (expression.Evaluate(state)) {
        COMMAND_LIST_LOG(state, "%S: true {\n", iniLine.c_str());
        state->extraIndent++;
        if (state->post)
            _run_command_list(trueCommandsPost.get(), state, false);
        else
            _run_command_list(trueCommandsPre.get(), state, false);
        state->extraIndent--;
        COMMAND_LIST_LOG(state, "} endif\n");
    } else {
        COMMAND_LIST_LOG(state, "%S: false\n", iniLine.c_str());
        if (!hasNestedElseIf) {
            COMMAND_LIST_LOG(state, "[%S] else {\n", section.c_str());
            state->extraIndent++;
        }
        if (state->post)
            _run_command_list(falseCommandsPost.get(), state, false);
        else
            _run_command_list(falseCommandsPre.get(), state, false);
        if (!hasNestedElseIf) {
            state->extraIndent--;
            COMMAND_LIST_LOG(state, "} endif\n");
        }
    }
}

bool IfCommand::Optimise(HackerDevice *device)
{
    return expression.Optimise(device);
}

bool IfCommand::Noop(bool post, bool ignore_cto_pre, bool ignore_cto_post)
{
    float static_val;
    bool is_static;

    if ((post && !postFinalised) || (!post && !preFinalised)) {
        LogOverlay(LOG_WARNING, "WARNING: If missing endif: %S\n", iniLine.c_str());
        return true;
    }

    is_static = expression.StaticEvaluate(&static_val);
    if (is_static) {
        if (static_val) {
            falseCommandsPre->Clear();
            falseCommandsPost->Clear();
        } else {
            trueCommandsPre->Clear();
            trueCommandsPost->Clear();
        }
    }

    if (post)
        return trueCommandsPost->commands.empty() && falseCommandsPost->commands.empty();
    return trueCommandsPre->commands.empty() && falseCommandsPre->commands.empty();
}

void CommandPlaceholder::Run(CommandListState*)
{
    LogOverlay(LOG_DIRE, "BUG: Placeholder command executed: %S\n", iniLine.c_str());
}

bool CommandPlaceholder::Noop(bool post, bool ignore_cto_pre, bool ignore_cto_post)
{
    LogOverlay(LOG_WARNING, "WARNING: Command not terminated: %S\n", iniLine.c_str());
    return true;
}

ID3D11Resource *ResourceCopyTarget::GetResource(
        CommandListState *state,
        ID3D11View **view,   // Used by textures, render targets, depth/stencil buffers & UAVs
        UINT *stride,        // Used by vertex buffers
        UINT *offset,        // Used by vertex & index buffers
        DXGI_FORMAT *format, // Used by index buffers
        UINT *buf_size,      // Used when creating a view of the buffer
        ResourceCopyTarget *dst) // Used to get bind flags when substantiating a custom resource
{
    HackerDevice *hacker_device = state->hackerDevice;
    ID3D11Device *orig_device1 = state->origDevice1;
    ID3D11DeviceContext *orig_context1 = state->origContext1;
    ID3D11Resource *res = NULL;
    ID3D11Buffer *buf = NULL;
    ID3D11Buffer *so_bufs[D3D11_SO_STREAM_COUNT];
    ID3D11ShaderResourceView *resource_view = NULL;
    ID3D11RenderTargetView *render_view[D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT];
    ID3D11DepthStencilView *depth_view = NULL;
    ID3D11UnorderedAccessView *unordered_view = NULL;
    D3D11_BIND_FLAG bind_flags = (D3D11_BIND_FLAG)0;
    D3D11_RESOURCE_MISC_FLAG misc_flags = (D3D11_RESOURCE_MISC_FLAG)0;
    unsigned i;

    switch(type) {
    case ResourceCopyTargetType::CONSTANT_BUFFER:
        // FIXME: On win8 (or with evil update?), we should use
        // Get/SetConstantBuffers1 and copy the offset into the buffer as well
        switch(shaderType) {
        case L'v':
            orig_context1->VSGetConstantBuffers(slot, 1, &buf);
            return buf;
        case L'h':
            orig_context1->HSGetConstantBuffers(slot, 1, &buf);
            return buf;
        case L'd':
            orig_context1->DSGetConstantBuffers(slot, 1, &buf);
            return buf;
        case L'g':
            orig_context1->GSGetConstantBuffers(slot, 1, &buf);
            return buf;
        case L'p':
            orig_context1->PSGetConstantBuffers(slot, 1, &buf);
            return buf;
        case L'c':
            orig_context1->CSGetConstantBuffers(slot, 1, &buf);
            return buf;
        default:
            // Should not happen
            return NULL;
        }
        break;

    case ResourceCopyTargetType::SHADER_RESOURCE:
        switch(shaderType) {
        case L'v':
            orig_context1->VSGetShaderResources(slot, 1, &resource_view);
            break;
        case L'h':
            orig_context1->HSGetShaderResources(slot, 1, &resource_view);
            break;
        case L'd':
            orig_context1->DSGetShaderResources(slot, 1, &resource_view);
            break;
        case L'g':
            orig_context1->GSGetShaderResources(slot, 1, &resource_view);
            break;
        case L'p':
            orig_context1->PSGetShaderResources(slot, 1, &resource_view);
            break;
        case L'c':
            orig_context1->CSGetShaderResources(slot, 1, &resource_view);
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
    // TODO:     break;

    case ResourceCopyTargetType::VERTEX_BUFFER:
        // TODO: If copying this to a constant buffer, provide some
        // means to get the strides + offsets from within the shader.
        // Perhaps as an IniParam, or in another constant buffer?
        orig_context1->IAGetVertexBuffers(slot, 1, &buf, stride, offset);
        return buf;

    case ResourceCopyTargetType::INDEX_BUFFER:
        // TODO: Similar comment as vertex buffers above, provide a
        // means for a shader to get format + offset.
        orig_context1->IAGetIndexBuffer(&buf, format, offset);
        if (stride && format)
            *stride = dxgi_format_size(*format);
        return buf;

    case ResourceCopyTargetType::STREAM_OUTPUT:
        // XXX: Does not give us the offset
        orig_context1->SOGetTargets(slot + 1, so_bufs);

        // Release any buffers we aren't after:
        for (i = 0; i < slot; i++) {
            if (so_bufs[i]) {
                so_bufs[i]->Release();
                so_bufs[i] = NULL;
            }
        }

        return so_bufs[slot];

    case ResourceCopyTargetType::RENDER_TARGET:
        orig_context1->OMGetRenderTargets(slot + 1, render_view, NULL);

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
        orig_context1->OMGetRenderTargets(0, NULL, &depth_view);
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
        switch(shaderType) {
        case L'p':
            // XXX: Not clear if the start slot is ok like this from the docs?
            // Particularly, what happens if we retrieve a subsequent UAV?
            orig_context1->OMGetRenderTargetsAndUnorderedAccessViews(0, NULL, NULL, slot, 1, &unordered_view);
            break;
        case L'c':
            orig_context1->CSGetUnorderedAccessViews(slot, 1, &unordered_view);
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
        customResource->Expire(orig_device1, orig_context1);

        if (dst)
            bind_flags = dst->BindFlags(state, &misc_flags);
        customResource->Substantiate(orig_device1, hacker_device->stereoHandle, bind_flags, misc_flags);

        if (stride)
            *stride = customResource->stride;
        if (offset)
            *offset = customResource->offset;
        if (format)
            *format = customResource->format;
        if (buf_size)
            *buf_size = customResource->bufSize;

        if (customResource->isNull) {
            // Optimisation to allow the resource to be set to null
            // without throwing away the cache so we don't
            // endlessly create & destroy temporary resources.
            *view = NULL;
            return NULL;
        }

        if (customResource->view)
            customResource->view->AddRef();
        *view = customResource->view;
        if (customResource->resource)
            customResource->resource->AddRef();
        return customResource->resource;

    case ResourceCopyTargetType::STEREO_PARAMS:
        if (hacker_device->stereoResourceView)
            hacker_device->stereoResourceView->AddRef();
        *view = hacker_device->stereoResourceView;
        if (hacker_device->stereoTexture)
            hacker_device->stereoTexture->AddRef();
        return hacker_device->stereoTexture;

    case ResourceCopyTargetType::INI_PARAMS:
        if (hacker_device->iniResourceView)
            hacker_device->iniResourceView->AddRef();
        *view = hacker_device->iniResourceView;
        if (hacker_device->iniTexture)
            hacker_device->iniTexture->AddRef();
        return hacker_device->iniTexture;

    case ResourceCopyTargetType::CURSOR_MASK:
        update_cursor_resources(state);
        if (state->cursorMaskView)
            state->cursorMaskView->AddRef();
        *view = state->cursorMaskView;
        if (state->cursorMaskTex)
            state->cursorMaskTex->AddRef();
        return state->cursorMaskTex;

    case ResourceCopyTargetType::CURSOR_COLOR:
        update_cursor_resources(state);
        if (state->cursorColorView)
            state->cursorColorView->AddRef();
        *view = state->cursorColorView;
        if (state->cursorColorTex)
            state->cursorColorTex->AddRef();
        return state->cursorColorTex;

    case ResourceCopyTargetType::THIS_RESOURCE:
        if (state->thisTarget)
            return state->thisTarget->GetResource(state, view, stride, offset, format, buf_size);

        if (state->resource) {
            if (state->view)
                state->view->AddRef();
            *view = state->view;
            if (*state->resource)
                (*state->resource)->AddRef();
            return (*state->resource);
        }

        COMMAND_LIST_LOG(state, "  \"this\"  is not valid in this context\n");
        return NULL;

    case ResourceCopyTargetType::SWAP_CHAIN:
        {
            HackerSwapChain *hacker_swap_chain = hacker_device->GetHackerSwapChain();
            if (hacker_swap_chain) {
                if (G->bb_is_upscaling_bb)
                    hacker_swap_chain->GetBuffer(0, __uuidof(ID3D11Resource), (void**)&res);
                else
                    hacker_swap_chain->GetOrigSwapChain1()->GetBuffer(0, __uuidof(ID3D11Resource), (void**)&res);
            } else
                COMMAND_LIST_LOG(state, "  Unable to get access to swap chain\n");
        }
        return res;

    case ResourceCopyTargetType::REAL_SWAP_CHAIN:
        {
            HackerSwapChain *hacker_swap_chain = hacker_device->GetHackerSwapChain();
            if (hacker_swap_chain)
                hacker_swap_chain->GetOrigSwapChain1()->GetBuffer(0, __uuidof(ID3D11Resource), (void**)&res);
            else
                COMMAND_LIST_LOG(state, "  Unable to get access to real swap chain\n");
        }
        return res;

    case ResourceCopyTargetType::FAKE_SWAP_CHAIN:
        {
            HackerSwapChain *hacker_swap_chain = hacker_device->GetHackerSwapChain();
            if (hacker_swap_chain)
                hacker_swap_chain->GetBuffer(0, __uuidof(ID3D11Resource), (void**)&res);
            else
                COMMAND_LIST_LOG(state, "  Unable to get access to fake swap chain\n");
        }
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
    ID3D11DeviceContext *orig_context1 = state->origContext1;
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
        switch(shaderType) {
        case L'v':
            orig_context1->VSSetConstantBuffers(slot, 1, &buf);
            return;
        case L'h':
            orig_context1->HSSetConstantBuffers(slot, 1, &buf);
            return;
        case L'd':
            orig_context1->DSSetConstantBuffers(slot, 1, &buf);
            return;
        case L'g':
            orig_context1->GSSetConstantBuffers(slot, 1, &buf);
            return;
        case L'p':
            orig_context1->PSSetConstantBuffers(slot, 1, &buf);
            return;
        case L'c':
            orig_context1->CSSetConstantBuffers(slot, 1, &buf);
            return;
        default:
            // Should not happen
            return;
        }
        break;

    case ResourceCopyTargetType::SHADER_RESOURCE:
        resource_view = (ID3D11ShaderResourceView*)view;
        switch(shaderType) {
        case L'v':
            orig_context1->VSSetShaderResources(slot, 1, &resource_view);
            break;
        case L'h':
            orig_context1->HSSetShaderResources(slot, 1, &resource_view);
            break;
        case L'd':
            orig_context1->DSSetShaderResources(slot, 1, &resource_view);
            break;
        case L'g':
            orig_context1->GSSetShaderResources(slot, 1, &resource_view);
            break;
        case L'p':
            orig_context1->PSSetShaderResources(slot, 1, &resource_view);
            break;
        case L'c':
            orig_context1->CSSetShaderResources(slot, 1, &resource_view);
            break;
        default:
            // Should not happen
            return;
        }
        break;

    // TODO: case ResourceCopyTargetType::SAMPLER: // Not an ID3D11Resource, need to think about this one
    // TODO:     break;

    case ResourceCopyTargetType::VERTEX_BUFFER:
        buf = (ID3D11Buffer*)res;
        orig_context1->IASetVertexBuffers(slot, 1, &buf, &stride, &offset);
        return;

    case ResourceCopyTargetType::INDEX_BUFFER:
        buf = (ID3D11Buffer*)res;
        orig_context1->IASetIndexBuffer(buf, format, offset);
        break;

    case ResourceCopyTargetType::STREAM_OUTPUT:
        // XXX: HERE BE UNTESTED CODE PATHS!
        buf = (ID3D11Buffer*)res;
        orig_context1->SOGetTargets(D3D11_SO_STREAM_COUNT, so_bufs);
        if (so_bufs[slot])
            so_bufs[slot]->Release();
        so_bufs[slot] = buf;
        // XXX: We set offsets to NULL here. We should really preserve
        // them, but I'm not sure how to get their original values,
        // so... too bad. Probably will never even use this anyway.
        orig_context1->SOSetTargets(D3D11_SO_STREAM_COUNT, so_bufs, NULL);

        for (i = 0; i < D3D11_SO_STREAM_COUNT; i++) {
            if (so_bufs[i])
                so_bufs[i]->Release();
        }

        break;

    case ResourceCopyTargetType::RENDER_TARGET:
        orig_context1->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, render_view, &depth_view);

        if (render_view[slot])
            render_view[slot]->Release();
        render_view[slot] = (ID3D11RenderTargetView*)view;

        orig_context1->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, render_view, depth_view);

        for (i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++) {
            if (i != slot && render_view[i])
                render_view[i]->Release();
        }
        if (depth_view)
            depth_view->Release();

        break;

    case ResourceCopyTargetType::DEPTH_STENCIL_TARGET:
        orig_context1->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, render_view, &depth_view);

        if (depth_view)
            depth_view->Release();
        depth_view = (ID3D11DepthStencilView*)view;

        orig_context1->OMSetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, render_view, depth_view);

        for (i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++) {
            if (render_view[i])
                render_view[i]->Release();
        }
        break;

    case ResourceCopyTargetType::UNORDERED_ACCESS_VIEW:
        // XXX: HERE BE UNTESTED CODE PATHS!
        unordered_view = (ID3D11UnorderedAccessView*)view;
        switch(shaderType) {
        case L'p':
            // XXX: Not clear if this will unbind other UAVs or not?
            // TODO: Allow pUAVInitialCounts to optionally be set
            orig_context1->OMSetRenderTargetsAndUnorderedAccessViews(D3D11_KEEP_RENDER_TARGETS_AND_DEPTH_STENCIL,
                NULL, NULL, slot, 1, &unordered_view, &uav_counter);
            return;
        case L'c':
            // TODO: Allow pUAVInitialCounts to optionally be set
            orig_context1->CSSetUnorderedAccessViews(slot, 1, &unordered_view, &uav_counter);
            return;
        default:
            // Should not happen
            return;
        }
        break;

    case ResourceCopyTargetType::CUSTOM_RESOURCE:
        customResource->stride = stride;
        customResource->offset = offset;
        customResource->format = format;
        customResource->bufSize = buf_size;


        if (res == NULL && view == NULL) {
            // Optimisation to allow the resource to be set to null
            // without throwing away the cache so we don't
            // endlessly create & destroy temporary resources.
            customResource->isNull = true;
            return;
        }
        customResource->isNull = false;

        // If we are passed our own resource (might happen if the
        // resource is used directly in the run() function, or if
        // someone assigned a resource to itself), don't needlessly
        // AddRef() and Release(), and definitely don't Release()
        // before AddRef()
        if (customResource->view != view) {
            if (customResource->view)
                customResource->view->Release();
            customResource->view = view;
            if (customResource->view)
                customResource->view->AddRef();
        }

        if (customResource->resource != res) {
            if (customResource->resource)
                customResource->resource->Release();
            customResource->resource = res;
            customResource->device = state->origDevice1;
            if (customResource->resource)
                customResource->resource->AddRef();
        }
        break;

    case ResourceCopyTargetType::THIS_RESOURCE:
        if (state->thisTarget)
            return state->thisTarget->SetResource(state, res, view, stride, offset, format, buf_size);

        if (state->resource) {
            if (*state->resource)
                (*state->resource)->Release();
            *state->resource = res;
            break;
        }

        COMMAND_LIST_LOG(state, "  \"this\" target cannot be set outside of a checktextureoverride or indirect draw call context\n");
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

D3D11_BIND_FLAG ResourceCopyTarget::BindFlags(CommandListState *state, D3D11_RESOURCE_MISC_FLAG *misc_flags)
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
            if (misc_flags)
                *misc_flags = customResource->miscFlags;
            return customResource->bindFlags;
        case ResourceCopyTargetType::THIS_RESOURCE:
            if (state) {
                if (state->thisTarget)
                    return state->thisTarget->BindFlags(state);

                if (state->callInfo && state->callInfo->indirect_buffer) {
                    if (misc_flags)
                        *misc_flags = D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS;
                    // No bind flags required, but a common scenario would be copying
                    // from a resource with D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS
                    // that will need to be cleared out if we have neither SRV or UAV
                    // set, which is handled in RecreateCompatibleBuffer()
                    return (D3D11_BIND_FLAG)0;
                }
            }
            // Bind flags are unknown since this cannot be resolved
            // until runtime:
            return (D3D11_BIND_FLAG)0;
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

void ResourceCopyTarget::FindTextureOverrides(CommandListState *state, bool *resource_found, TextureOverrideMatches *matches)
{
    TextureOverrideMap::iterator i;
    ID3D11Resource *resource = NULL;
    ID3D11View *view = NULL;
    uint32_t hash = 0;

    resource = GetResource(state, &view, NULL, NULL, NULL, NULL);

    if (resource_found)
        *resource_found = !!resource;

    if (!resource)
        return;

    find_texture_overrides_for_resource(resource, matches, state->callInfo);

    //COMMAND_LIST_LOG(state, "  found texture hash = %08llx\n", hash);

    resource->Release();
    if (view)
        view->Release();
}

static bool is_coercion_to_structured_buffer_required(ID3D11View *view, UINT stride,
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

static ID3D11Buffer *recreate_compatible_buffer(
        wstring *ini_line,
        ResourceCopyTarget *dst, // May be NULL
        ID3D11Buffer *src_resource,
        ID3D11Buffer *dst_resource,
        ResourcePool *resource_pool,
        ID3D11View *src_view,
        D3D11_BIND_FLAG bind_flags,
        D3D11_RESOURCE_MISC_FLAG misc_flags,
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
    // We reuse the misc flags from the source, which has worked fairly
    // well for the most part with maybe one or two exceptions. We can
    // always clear incompatible flags and allow an override if necessary:
    new_desc.MiscFlags = new_desc.MiscFlags | misc_flags;

    // Raw view misc flag can only be used with SRVs or UAVs
    if (!(new_desc.BindFlags & (D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_UNORDERED_ACCESS)))
        new_desc.MiscFlags &= ~D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS;

    if (dst && dst->type == ResourceCopyTargetType::CPU) {
        new_desc.Usage = D3D11_USAGE_STAGING;
        new_desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    } else {
        new_desc.Usage = D3D11_USAGE_DEFAULT;
        new_desc.CPUAccessFlags = 0;
    }

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
    } else if (is_coercion_to_structured_buffer_required(src_view, stride, offset, format, bind_flags)) {
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
        dst->customResource->OverrideBufferDesc(&new_desc);

    return get_resource_from_pool<ID3D11Buffer, D3D11_BUFFER_DESC, &ID3D11Device::CreateBuffer>
        (ini_line, src_resource, dst_resource, resource_pool, state, &new_desc);
}

static DXGI_FORMAT make_typeless(DXGI_FORMAT fmt)
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

static DXGI_FORMAT make_dsv_format(DXGI_FORMAT fmt)
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
            return ensure_not_typeless(fmt);
    }
}

static DXGI_FORMAT make_non_dsv_format(DXGI_FORMAT fmt)
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
            return ensure_not_typeless(fmt);
    }
}

// MSAA resolving only makes sense for Texture2D types, and the SampleDesc
// entry only exists in those. Use template specialisation so we don't have to
// duplicate the entire RecreateCompatibleTexture() routine for such a small
// difference.
template <typename DescType>
static void texture_2d_desc_resolve_msaa(DescType *desc) {}
template <>
static void texture_2d_desc_resolve_msaa(D3D11_TEXTURE2D_DESC *desc)
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
static ResourceType* recreate_compatible_texture(
        wstring *ini_line,
        ResourceCopyTarget *dst, // May be NULL
        ResourceType *src_resource,
        ResourceType *dst_resource,
        ResourcePool *resource_pool,
        D3D11_BIND_FLAG bind_flags,
        CommandListState *state,
        StereoHandle stereo_handle,
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
    new_desc.Format = make_typeless(new_desc.Format);

    if (options & ResourceCopyOptions::STEREO2MONO)
        new_desc.Width *= 2;

    // TODO: reverse_blit might need to imply resolve_msaa:
    if (options & ResourceCopyOptions::RESOLVE_MSAA)
        texture_2d_desc_resolve_msaa(&new_desc);

    // XXX: Any changes needed in new_desc.MiscFlags?
    //
    // D3D11_RESOURCE_MISC_GENERATE_MIPS requires specific bind flags (both
    // shader resource AND render target must be set) and might prevent us
    // from creating the resource otherwise. Since we don't need to
    // generate mip-maps just clear it out:
    new_desc.MiscFlags &= ~D3D11_RESOURCE_MISC_GENERATE_MIPS;

    if (dst && dst->type == ResourceCopyTargetType::CUSTOM_RESOURCE)
        dst->customResource->OverrideTexDesc(&new_desc);

    return get_resource_from_pool<ResourceType, DescType, CreateTexture>
        (ini_line, src_resource, dst_resource, resource_pool, state, &new_desc);
}

static void recreate_compatible_resource(
        wstring *ini_line,
        ResourceCopyTarget *dst, // May be NULL
        ID3D11Resource *src_resource,
        ID3D11Resource **dst_resource,
        ResourcePool *resource_pool,
        ID3D11View *src_view,
        ID3D11View **dst_view,
        CommandListState *state,
        StereoHandle stereo_handle,
        ResourceCopyOptions options,
        UINT stride,
        UINT offset,
        DXGI_FORMAT format,
        UINT *buf_dst_size)
{
    NVAPI_STEREO_SURFACECREATEMODE orig_mode = NVAPI_STEREO_SURFACECREATEMODE_AUTO;
    D3D11_RESOURCE_DIMENSION src_dimension;
    D3D11_BIND_FLAG bind_flags = (D3D11_BIND_FLAG)0;
    D3D11_RESOURCE_MISC_FLAG misc_flags = (D3D11_RESOURCE_MISC_FLAG)0;
    ID3D11Resource *res = NULL;
    bool restore_create_mode = false;

    if (dst)
        bind_flags = dst->BindFlags(state, &misc_flags);

    LOCK_RESOURCE_CREATION_MODE();

    if (options & ResourceCopyOptions::CREATEMODE_MASK) {
        Profiling::NvAPI_Stereo_GetSurfaceCreationMode(stereo_handle, &orig_mode);
        restore_create_mode = true;

        // STEREO2MONO will force the final destination to mono since
        // it is in the CREATEMODE_MASK, but is not STEREO. It also
        // creates an additional intermediate resource that will be
        // forced to STEREO.

        if (options & ResourceCopyOptions::STEREO) {
            Profiling::NvAPI_Stereo_SetSurfaceCreationMode(stereo_handle,
                    NVAPI_STEREO_SURFACECREATEMODE_FORCESTEREO);
        } else {
            Profiling::NvAPI_Stereo_SetSurfaceCreationMode(stereo_handle,
                    NVAPI_STEREO_SURFACECREATEMODE_FORCEMONO);
        }
    } else if (dst && dst->type == ResourceCopyTargetType::CUSTOM_RESOURCE) {
        restore_create_mode = dst->customResource->OverrideSurfaceCreationMode(stereo_handle, &orig_mode);
    }

    src_resource->GetType(&src_dimension);
    switch (src_dimension) {
        case D3D11_RESOURCE_DIMENSION_BUFFER:
            res = recreate_compatible_buffer(ini_line, dst, (ID3D11Buffer*)src_resource, (ID3D11Buffer*)*dst_resource,
                resource_pool, src_view, bind_flags, misc_flags, state, stride, offset, format, buf_dst_size);
            break;
        case D3D11_RESOURCE_DIMENSION_TEXTURE1D:
            res = recreate_compatible_texture<ID3D11Texture1D, D3D11_TEXTURE1D_DESC, &ID3D11Device::CreateTexture1D>
                (ini_line, dst, (ID3D11Texture1D*)src_resource, (ID3D11Texture1D*)*dst_resource, resource_pool,
                 bind_flags, state, stereo_handle, options);
            break;
        case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
            res = recreate_compatible_texture<ID3D11Texture2D, D3D11_TEXTURE2D_DESC, &ID3D11Device::CreateTexture2D>
                (ini_line, dst, (ID3D11Texture2D*)src_resource, (ID3D11Texture2D*)*dst_resource, resource_pool,
                 bind_flags, state, stereo_handle, options);
            break;
        case D3D11_RESOURCE_DIMENSION_TEXTURE3D:
            res = recreate_compatible_texture<ID3D11Texture3D, D3D11_TEXTURE3D_DESC, &ID3D11Device::CreateTexture3D>
                (ini_line, dst, (ID3D11Texture3D*)src_resource, (ID3D11Texture3D*)*dst_resource, resource_pool,
                 bind_flags, state, stereo_handle, options);
            break;
    }

    if (restore_create_mode)
        Profiling::NvAPI_Stereo_SetSurfaceCreationMode(stereo_handle, orig_mode);

    UNLOCK_RESOURCE_CREATION_MODE();

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
static void fill_out_buffer_desc_common(DescType *desc, UINT stride,
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
    //
    // Possible TODO: Handle vertex/index buffers with "first vertex/index"
    // here? These can now be accessed via command list expression and
    // passed through ini params, so a fix can handle them that way, and
    // any change here would need careful consideration as to backwards
    // compatibility.
    if (stride) {
        desc->FirstElement = offset / stride;
        desc->NumElements = (buf_src_size - offset) / stride;
    } else {
        desc->FirstElement = 0;
        desc->NumElements = 1;
    }
}

static bool requires_raw_view(ID3D11Buffer *buf, DXGI_FORMAT format)
{
    D3D11_BUFFER_DESC buf_desc;

    buf->GetDesc(&buf_desc);
    if (!(buf_desc.MiscFlags & D3D11_RESOURCE_MISC_BUFFER_ALLOW_RAW_VIEWS))
        return false;

    // The buffer allows raw views, but that doesn't forbid other access.
    // However, other non-structured access (structured cannot have the
    // above flag) requires us to know the data format, so if we don't know
    // the data type than raw access is the only remaining option:
    switch (format) {
        case DXGI_FORMAT_UNKNOWN:
        case DXGI_FORMAT_R32_TYPELESS:
            return true;
    }

    return false;
}

static D3D11_SHADER_RESOURCE_VIEW_DESC* fill_out_buffer_desc(ID3D11Buffer *buf,
        D3D11_SHADER_RESOURCE_VIEW_DESC *desc, UINT stride,
        UINT offset, UINT buf_src_size, ResourceCopyOptions options)
{
    if (options & ResourceCopyOptions::RAW_VIEW || requires_raw_view(buf, desc->Format)) {
               desc->ViewDimension = D3D11_SRV_DIMENSION_BUFFEREX;
               desc->BufferEx.Flags = D3D11_BUFFEREX_SRV_FLAG_RAW;
           // Although not specified in MSDN, raw views in SRVs appear to
           // require the R32_TYPELESS format (MSDN does specify this
           // requirement for raw UAVs) and fail if any other format is
           // specified. The only curiosity here is that they are described
           // as being accessible in 1-4 channels, seeming to imply that
           // formats like R32G32B32A32_TYPELESS should also work, but they
           // don't:
           desc->Format = DXGI_FORMAT_R32_TYPELESS;
           stride = 4;
           fill_out_buffer_desc_common<D3D11_BUFFEREX_SRV>(&desc->BufferEx, stride, offset, buf_src_size);
    } else {
        desc->ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
        fill_out_buffer_desc_common<D3D11_BUFFER_SRV>(&desc->Buffer, stride, offset, buf_src_size);
    }
    return desc;
}
static D3D11_RENDER_TARGET_VIEW_DESC* fill_out_buffer_desc(ID3D11Buffer *buf,
        D3D11_RENDER_TARGET_VIEW_DESC *desc, UINT stride,
        UINT offset, UINT buf_src_size, ResourceCopyOptions options)
{
    desc->ViewDimension = D3D11_RTV_DIMENSION_BUFFER;

    fill_out_buffer_desc_common<D3D11_BUFFER_RTV>(&desc->Buffer, stride, offset, buf_src_size);
    return desc;
}
static D3D11_UNORDERED_ACCESS_VIEW_DESC* fill_out_buffer_desc(ID3D11Buffer *buf,
        D3D11_UNORDERED_ACCESS_VIEW_DESC *desc, UINT stride,
        UINT offset, UINT buf_src_size, ResourceCopyOptions options)
{
    desc->ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
    desc->Buffer.Flags = 0;

    if (options & ResourceCopyOptions::RAW_VIEW || requires_raw_view(buf, desc->Format)) {
        desc->Buffer.Flags |= D3D11_BUFFER_UAV_FLAG_RAW;
        desc->Format = DXGI_FORMAT_R32_TYPELESS;
        stride = 4;
    }
    // TODO Support buffer UAV flags for append and counter buffers.

    fill_out_buffer_desc_common<D3D11_BUFFER_UAV>(&desc->Buffer, stride, offset, buf_src_size);
    return desc;
}
static D3D11_DEPTH_STENCIL_VIEW_DESC* fill_out_buffer_desc(ID3D11Buffer *buf,
        D3D11_DEPTH_STENCIL_VIEW_DESC *desc, UINT stride,
        UINT offset, UINT buf_src_size, ResourceCopyOptions options)
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
static D3D11_SHADER_RESOURCE_VIEW_DESC* fill_out_tex_1d_desc(
        D3D11_SHADER_RESOURCE_VIEW_DESC *view_desc,
        D3D11_TEXTURE1D_DESC *resource_desc, DXGI_FORMAT format)
{
    view_desc->Format = make_non_dsv_format(format);

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
static D3D11_RENDER_TARGET_VIEW_DESC* fill_out_tex_1d_desc(
        D3D11_RENDER_TARGET_VIEW_DESC *view_desc,
        D3D11_TEXTURE1D_DESC *resource_desc, DXGI_FORMAT format)
{
    view_desc->Format = make_non_dsv_format(format);

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
static D3D11_DEPTH_STENCIL_VIEW_DESC* fill_out_tex_1d_desc(
        D3D11_DEPTH_STENCIL_VIEW_DESC *view_desc,
        D3D11_TEXTURE1D_DESC *resource_desc, DXGI_FORMAT format)
{
    view_desc->Format = make_dsv_format(format);

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
static D3D11_UNORDERED_ACCESS_VIEW_DESC* fill_out_tex_1d_desc(
        D3D11_UNORDERED_ACCESS_VIEW_DESC *view_desc,
        D3D11_TEXTURE1D_DESC *resource_desc, DXGI_FORMAT format)
{
    view_desc->Format = make_non_dsv_format(format);

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
static D3D11_SHADER_RESOURCE_VIEW_DESC* fill_out_tex_2d_desc(
        D3D11_SHADER_RESOURCE_VIEW_DESC *view_desc,
        D3D11_TEXTURE2D_DESC *resource_desc, DXGI_FORMAT format)
{
    view_desc->Format = make_non_dsv_format(format);

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
static D3D11_RENDER_TARGET_VIEW_DESC* fill_out_tex_2d_desc(
        D3D11_RENDER_TARGET_VIEW_DESC *view_desc,
        D3D11_TEXTURE2D_DESC *resource_desc, DXGI_FORMAT format)
{
    view_desc->Format = make_non_dsv_format(format);

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
static D3D11_DEPTH_STENCIL_VIEW_DESC* fill_out_tex_2d_desc(
        D3D11_DEPTH_STENCIL_VIEW_DESC *view_desc,
        D3D11_TEXTURE2D_DESC *resource_desc, DXGI_FORMAT format)
{
    view_desc->Format = make_dsv_format(format);
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
static D3D11_UNORDERED_ACCESS_VIEW_DESC* fill_out_tex_2d_desc(
        D3D11_UNORDERED_ACCESS_VIEW_DESC *view_desc,
        D3D11_TEXTURE2D_DESC *resource_desc, DXGI_FORMAT format)
{
    view_desc->Format = make_non_dsv_format(format);

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
static D3D11_SHADER_RESOURCE_VIEW_DESC* fill_out_tex_3d_desc(
        D3D11_SHADER_RESOURCE_VIEW_DESC *view_desc,
        DXGI_FORMAT format)
{
    view_desc->Format = make_non_dsv_format(format);

    view_desc->ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
    view_desc->Texture3D.MostDetailedMip = 0;
    view_desc->Texture3D.MipLevels = -1;

    return view_desc;
}
static D3D11_RENDER_TARGET_VIEW_DESC* fill_out_tex_3d_desc(
        D3D11_RENDER_TARGET_VIEW_DESC *view_desc,
        DXGI_FORMAT format)
{
    view_desc->Format = make_non_dsv_format(format);

    view_desc->ViewDimension = D3D11_RTV_DIMENSION_TEXTURE3D;
    view_desc->Texture3D.MipSlice = 0;
    view_desc->Texture3D.FirstWSlice = 0;
    view_desc->Texture3D.WSize = -1;

    return view_desc;
}
static D3D11_DEPTH_STENCIL_VIEW_DESC* fill_out_tex_3d_desc(
        D3D11_DEPTH_STENCIL_VIEW_DESC *view_desc,
        DXGI_FORMAT format)
{
    // DSV cannot be a Texture3D

    return NULL;
}
static D3D11_UNORDERED_ACCESS_VIEW_DESC* fill_out_tex_3d_desc(
        D3D11_UNORDERED_ACCESS_VIEW_DESC *view_desc,
        DXGI_FORMAT format)
{
    view_desc->Format = make_non_dsv_format(format);

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
static ID3D11View* _create_compatible_view(
        ID3D11Resource *resource,
        CommandListState *state,
        UINT stride,
        UINT offset,
        DXGI_FORMAT format,
        UINT buf_src_size,
        ResourceCopyOptions options)
{
    D3D11_RESOURCE_DIMENSION dimension;
    ID3D11Buffer *buf;
    ID3D11Texture1D *tex1d;
    ID3D11Texture2D *tex2d;
    D3D11_TEXTURE1D_DESC tex1d_desc;
    D3D11_TEXTURE2D_DESC tex2d_desc;
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

            buf = (ID3D11Buffer*)resource;
            pDesc = fill_out_buffer_desc(buf, &view_desc, stride, offset, buf_src_size, options);

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
            pDesc = fill_out_tex_1d_desc(&view_desc, &tex1d_desc, format);
            break;
        case D3D11_RESOURCE_DIMENSION_TEXTURE2D:
            tex2d = (ID3D11Texture2D*)resource;
            tex2d->GetDesc(&tex2d_desc);
            pDesc = fill_out_tex_2d_desc(&view_desc, &tex2d_desc, format);
            break;
        case D3D11_RESOURCE_DIMENSION_TEXTURE3D:
            pDesc = fill_out_tex_3d_desc(&view_desc, format);
            break;
    }

    hr = (state->origDevice1->*CreateView)(resource, pDesc, &view);
    if (FAILED(hr)) {
        LOG_INFO("Resource copy CreateCompatibleView failed: %x\n", hr);
        if (pDesc)
            LogViewDesc(pDesc);
        LogResourceDesc(resource);
        return NULL;
    }

    if (pDesc)
        LogDebugViewDesc(pDesc);

    return view;
}

static ID3D11View* create_compatible_view(
        ResourceCopyTarget *dst,
        ID3D11Resource *resource,
        CommandListState *state,
        UINT stride,
        UINT offset,
        DXGI_FORMAT format,
        UINT buf_src_size,
        ResourceCopyOptions options)
{
    switch (dst->type) {
        case ResourceCopyTargetType::SHADER_RESOURCE:
            return _create_compatible_view<ID3D11ShaderResourceView,
                   D3D11_SHADER_RESOURCE_VIEW_DESC,
                   &ID3D11Device::CreateShaderResourceView>
                       (resource, state, stride, offset, format, buf_src_size, options);
        case ResourceCopyTargetType::RENDER_TARGET:
            return _create_compatible_view<ID3D11RenderTargetView,
                   D3D11_RENDER_TARGET_VIEW_DESC,
                   &ID3D11Device::CreateRenderTargetView>
                       (resource, state, stride, offset, format, buf_src_size, options);
        case ResourceCopyTargetType::DEPTH_STENCIL_TARGET:
            return _create_compatible_view<ID3D11DepthStencilView,
                   D3D11_DEPTH_STENCIL_VIEW_DESC,
                   &ID3D11Device::CreateDepthStencilView>
                       (resource, state, stride, offset, format, buf_src_size, options);
        case ResourceCopyTargetType::UNORDERED_ACCESS_VIEW:
            return _create_compatible_view<ID3D11UnorderedAccessView,
                   D3D11_UNORDERED_ACCESS_VIEW_DESC,
                   &ID3D11Device::CreateUnorderedAccessView>
                       (resource, state, stride, offset, format, buf_src_size, options);
        case ResourceCopyTargetType::THIS_RESOURCE:
            if (state->thisTarget)
                return create_compatible_view(state->thisTarget, resource, state, stride, offset, format, buf_src_size, options);
            break;
    }
    return NULL;
}

static void set_viewport_from_resource(CommandListState *state, ID3D11Resource *resource)
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

    state->origContext1->RSSetViewports(1, &viewport);
}

ResourceCopyOperation::ResourceCopyOperation() :
    options(ResourceCopyOptions::INVALID),
    cachedResource(NULL),
    cachedView(NULL),
    stereo2MonoIntermediate(NULL)
{}

ResourceCopyOperation::~ResourceCopyOperation()
{
    if (cachedResource)
        cachedResource->Release();

    if (cachedView)
        cachedView->Release();
}

ResourceStagingOperation::ResourceStagingOperation()
{
    dst.type = ResourceCopyTargetType::CPU;
    options = ResourceCopyOptions::COPY;
    staging = false;
    iniLine = L"  Beginning transfer to CPU...";
}

HRESULT ResourceStagingOperation::Map(CommandListState *state, D3D11_MAPPED_SUBRESOURCE *mapping)
{
    if (!cachedResource)
        return E_FAIL;

    return state->origContext1->Map(cachedResource, 0, D3D11_MAP_READ, D3D11_MAP_FLAG_DO_NOT_WAIT, mapping);
}

void ResourceStagingOperation::Unmap(CommandListState *state)
{
    if (cachedResource)
        state->origContext1->Unmap(cachedResource, 0);
}

static void resolve_msaa(ID3D11Resource *dst_resource, ID3D11Resource *src_resource, CommandListState *state)
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
    fmt = ensure_not_typeless(desc.Format);

    hr = state->origDevice1->CheckFormatSupport( fmt, &support );
    if (FAILED(hr) || !(support & D3D11_FORMAT_SUPPORT_MULTISAMPLE_RESOLVE)) {
        // TODO: Implement a fallback using a SM5 shader to resolve it
        LOG_INFO("Resource copy cannot resolve MSAA format %d\n", fmt);
        return;
    }

    for (item = 0; item < desc.ArraySize; item++) {
        for (level = 0; level < desc.MipLevels; level++) {
            index = D3D11CalcSubresource(level, item, max(desc.MipLevels, 1));
            state->origContext1->ResolveSubresource(dst, index, src, index, fmt);
        }
    }
}

static void reverse_stereo_blit(ID3D11Resource *dst_resource, ID3D11Resource *src_resource, CommandListState *state)
{
    NvAPI_Status nvret;
    D3D11_RESOURCE_DIMENSION src_dimension;
    ID3D11Texture2D *src;
    D3D11_TEXTURE2D_DESC src_desc;
    UINT item, level, index, width, height;
    D3D11_BOX src_box;
    int fallbackside, fallback = 0;

    src_resource->GetType(&src_dimension);
    if (src_dimension != D3D11_RESOURCE_DIMENSION_TEXTURE2D) {
        // TODO: I think it should be possible to do this with all
        // resource types (possibly including buffers from the
        // discovery of the stereo parameters in the cb12 slot), but I
        // need to test it and make sure it works first
        LOG_INFO("Resource copy: Reverse stereo blit not supported on resource type %d\n", src_dimension);
        return;
    }

    src = (ID3D11Texture2D*)src_resource;
    src->GetDesc(&src_desc);

    // TODO: Resolve MSAA
    // TODO: Use intermediate resource if copying from a texture with depth buffer bind flags

    // If stereo is disabled the reverse stereo blit won't work and we
    // would end up with the destination only updated on the left, which
    // may lead to shaders reading stale or 0 data if they read from the
    // right hand side. Use the fallback path to copy the source to both
    // sides of the destination so that the right side will be up to date:
    fallback = state->hackerDevice->paramTextureManager.mActive ? 0 : 1;

    if (!fallback) {
        nvret = Profiling::NvAPI_Stereo_ReverseStereoBlitControl(state->hackerDevice->stereoHandle, true);
        if (nvret != NVAPI_OK) {
            LOG_INFO("Resource copying failed to enable reverse stereo blit\n");
            // Fallback path: Copy 2D resource to both sides of the 2x
            // width destination
            fallback = 1;
        }
    }

    for (fallbackside = 0; fallbackside < 1 + fallback; fallbackside++) {

        // Set the source box as per the nvapi documentation:
        src_box.left = 0;
        src_box.top = 0;
        src_box.front = 0;
        src_box.right = width = src_desc.Width;
        src_box.bottom = height = src_desc.Height;
        src_box.back = 1;

        // Perform the reverse stereo blit on all sub-resources and mip-maps:
        for (item = 0; item < src_desc.ArraySize; item++) {
            for (level = 0; level < src_desc.MipLevels; level++) {
                index = D3D11CalcSubresource(level, item, max(src_desc.MipLevels, 1));
                src_box.right = width >> level;
                src_box.bottom = height >> level;
                state->origContext1->CopySubresourceRegion(dst_resource, index,
                        fallbackside * src_box.right, 0, 0,
                        src, index, &src_box);
            }
        }
    }

    if (!fallback)
        Profiling::NvAPI_Stereo_ReverseStereoBlitControl(state->hackerDevice->stereoHandle, false);
}

static void special_copy_buffer_region(ID3D11Resource *dst_resource,ID3D11Resource *src_resource,
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

    state->origContext1->CopySubresourceRegion(dst_resource, 0, 0, 0, 0, src_resource, 0, &src_box);

    // We have effectively removed the offset during the region copy, so
    // set it to 0 to make sure nothing will try to use it again elsewhere:
    *offset = 0;
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

ID3D11View* ClearViewCommand::CreateBestView(
        ID3D11Resource *resource,
        CommandListState *state,
        UINT stride,
        UINT offset,
        DXGI_FORMAT format,
        UINT buf_src_size)
{
    UINT bind_flags;

    // FIXME: If a buffer can only be accessed via a raw view we may need
    // to handle that case here:
    ResourceCopyOptions options = ResourceCopyOptions::INVALID;

    // We didn't get a view, so we will have to create one, but
    // which type? We will guess based on what the user specified
    // and what bind flags the resource has.

    fill_in_missing_info(target.type, resource, NULL, &stride, &offset,
            &buf_src_size, &format);

    // If the user specified "depth" and/or "stencil" they gave us
    // the answer:
    if (clearDepth || clearStencil) {
        return _create_compatible_view<ID3D11DepthStencilView,
               D3D11_DEPTH_STENCIL_VIEW_DESC,
               &ID3D11Device::CreateDepthStencilView>
                   (resource, state, stride, offset, format, buf_src_size, options);
    }

    // If the user specified "int" or used a hex string then it
    // must be a UAV and we must be doing an int clear on it:
    if (clearUavUint) {
        return _create_compatible_view<ID3D11UnorderedAccessView,
               D3D11_UNORDERED_ACCESS_VIEW_DESC,
               &ID3D11Device::CreateUnorderedAccessView>
                   (resource, state, stride, offset, format, buf_src_size, options);
    }

    // Otherwise just make whatever view is compatible with the bind flags.
    // Since views may have multiple bind flags let's prioritise the more
    // esoteric DSV and UAV before RTV on the theory that if they are
    // available then we are more likely to want to use their clear
    // methods.
    bind_flags = get_resource_bind_flags(resource);
    if (bind_flags & D3D11_BIND_DEPTH_STENCIL) {
        return _create_compatible_view<ID3D11DepthStencilView,
               D3D11_DEPTH_STENCIL_VIEW_DESC,
               &ID3D11Device::CreateDepthStencilView>
                   (resource, state, stride, offset, format, buf_src_size, options);
    }
    if (bind_flags & D3D11_BIND_UNORDERED_ACCESS) {
        return _create_compatible_view<ID3D11UnorderedAccessView,
               D3D11_UNORDERED_ACCESS_VIEW_DESC,
               &ID3D11Device::CreateUnorderedAccessView>
                   (resource, state, stride, offset, format, buf_src_size, options);
    }
    if (bind_flags & D3D11_BIND_RENDER_TARGET) {
        return _create_compatible_view<ID3D11RenderTargetView,
               D3D11_RENDER_TARGET_VIEW_DESC,
               &ID3D11Device::CreateRenderTargetView>
                   (resource, state, stride, offset, format, buf_src_size, options);
    }
    // TODO: In DX 11.1 there is a generic clear routine, so SRVs might work?
    return NULL;
}

void ClearViewCommand::ClearUnknownView(ID3D11View *view, CommandListState *state)
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
        COMMAND_LIST_LOG(state, "  clearing RTV\n");
        Profiling::views_cleared++;
        state->origContext1->ClearRenderTargetView(rtv, fval);
    }
    if (dsv) {
        D3D11_CLEAR_FLAG flags = (D3D11_CLEAR_FLAG)0;
        COMMAND_LIST_LOG(state, "  clearing DSV\n");
        Profiling::views_cleared++;

        if (!clearDepth && !clearStencil)
            flags = (D3D11_CLEAR_FLAG)(D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL);
        else if (clearDepth)
            flags = D3D11_CLEAR_DEPTH;
        else if (clearStencil)
            flags = D3D11_CLEAR_STENCIL;

        state->origContext1->ClearDepthStencilView(dsv, flags, dsvDepth, dsvStencil);
    }
    if (uav) {
        // We can clear UAVs with either floats or uints, but which
        // should we use? The API call doesn't let us know if it
        // failed, and floats will only work with specific view
        // formats, so we try to predict if the float clear will pass
        // unless the user specificially told us to use the int clear.
        if (clearUavUint || !uav_supports_float_clear(uav)) {
            COMMAND_LIST_LOG(state, "  clearing UAV (uint)\n");
            state->origContext1->ClearUnorderedAccessViewUint(uav, uval);
        } else {
            COMMAND_LIST_LOG(state, "  clearing UAV (float)\n");
            state->origContext1->ClearUnorderedAccessViewFloat(uav, fval);
        }
        Profiling::views_cleared++;
    }

    if (rtv)
        rtv->Release();
    if (dsv)
        dsv->Release();
    if (uav)
        uav->Release();
}

void ClearViewCommand::Run(CommandListState *state)
{
    ID3D11Resource *resource = NULL;
    ID3D11View *view = NULL;
    UINT stride = 0;
    UINT offset = 0;
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    UINT buf_src_size = 0;

    COMMAND_LIST_LOG(state, "%S\n", iniLine.c_str());

    resource = target.GetResource(state, &view, &stride, &offset, &format, &buf_src_size);
    if (!resource) {
        COMMAND_LIST_LOG(state, "  No resource to clear\n");
        return;
    }

    if (!view)
        view = CreateBestView(resource, state, stride, offset, format, buf_src_size);

    if (view)
        ClearUnknownView(view, state);
    else
        COMMAND_LIST_LOG(state, "  No view and unable to create view to clear resource\n");

    if (resource)
        resource->Release();
    if (view)
        view->Release();
}


static bool view_matches_resource(ID3D11View *view, ID3D11Resource *resource)
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
static ResourceCopyTargetType equiv_target(ResourceCopyTargetType type)
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

void ResourceCopyOperation::Run(CommandListState *state)
{
    HackerDevice *hacker_device = state->hackerDevice;
    HackerContext *hacker_context = state->hackerContext;
    ID3D11DeviceContext *orig_context1 = state->origContext1;
    ID3D11Resource *src_resource = NULL;
    ID3D11Resource *dst_resource = NULL;
    ID3D11Resource **pp_cached_resource = &cachedResource;
    ID3D11Device **pp_cached_device = NULL;
    ResourcePool *p_resource_pool = &resourcePool;
    ID3D11View *src_view = NULL;
    ID3D11View *dst_view = NULL;
    ID3D11View **pp_cached_view = &cachedView;
    UINT stride = 0;
    UINT offset = 0;
    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
    UINT buf_src_size = 0, buf_dst_size = 0;

    COMMAND_LIST_LOG(state, "%S\n", iniLine.c_str());

    if (src.type == ResourceCopyTargetType::EMPTY) {
        dst.SetResource(state, NULL, NULL, 0, 0, DXGI_FORMAT_UNKNOWN, 0);
        return;
    }

    src_resource = src.GetResource(state, &src_view, &stride, &offset, &format, &buf_src_size,
            ((options & ResourceCopyOptions::REFERENCE) ? &dst : NULL));
    if (!src_resource) {
        COMMAND_LIST_LOG(state, "  Copy source was NULL\n");
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
        pp_cached_resource = &dst.customResource->resource;
        pp_cached_device = &dst.customResource->device;
        p_resource_pool = &dst.customResource->resourcePool;
        pp_cached_view = &dst.customResource->view;

        if (dst.customResource->maxCopiesPerFrame) {
            if (dst.customResource->frameNo != G->frame_no) {
                dst.customResource->frameNo = G->frame_no;
                dst.customResource->copiesThisFrame = 1;
            } else if (dst.customResource->copiesThisFrame++ >= dst.customResource->maxCopiesPerFrame) {
                COMMAND_LIST_LOG(state, "  max_copies_per_frame exceeded\n");
                Profiling::max_copies_per_frame_exceeded++;
                return;
            }
        }

        dst.customResource->OverrideOutOfBandInfo(&format, &stride);
    }

    fill_in_missing_info(src.type, src_resource, src_view, &stride, &offset, &buf_src_size, &format);

    if (options & ResourceCopyOptions::COPY_MASK) {
        recreate_compatible_resource(&iniLine, &dst, src_resource,
            pp_cached_resource, p_resource_pool, src_view, pp_cached_view,
            state, hacker_device->stereoHandle,
            options, stride, offset, format, &buf_dst_size);

        if (!*pp_cached_resource) {
            COMMAND_LIST_LOG(state, "  error creating/updating destination resource\n");
            goto out_release;
        }
        dst_resource = *pp_cached_resource;
        if (pp_cached_device)
            *pp_cached_device = state->origDevice1;
        dst_view = *pp_cached_view;

        if (options & ResourceCopyOptions::COPY_DESC) {
            // RecreateCompatibleResource has already done the work
            COMMAND_LIST_LOG(state, "  copying resource description\n");
        } else if (options & ResourceCopyOptions::STEREO2MONO) {
            COMMAND_LIST_LOG(state, "  performing reverse stereo blit\n");
            Profiling::stereo2mono_copies++;

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

            recreate_compatible_resource(&(iniLine + L" (intermediate)"),
                NULL, src_resource, &stereo2MonoIntermediate,
                p_resource_pool, NULL, NULL,
                state, hacker_device->stereoHandle,
                (ResourceCopyOptions)(options | ResourceCopyOptions::STEREO),
                stride, offset, format, NULL);

            reverse_stereo_blit(stereo2MonoIntermediate, src_resource, state);

            orig_context1->CopyResource(dst_resource, stereo2MonoIntermediate);
        } else if (options & ResourceCopyOptions::RESOLVE_MSAA) {
            COMMAND_LIST_LOG(state, "  resolving MSAA\n");
            Profiling::msaa_resolutions++;
            resolve_msaa(dst_resource, src_resource, state);
        } else if (buf_dst_size) {
            COMMAND_LIST_LOG(state, "  performing region copy\n");
            Profiling::buffer_region_copies++;
            special_copy_buffer_region(dst_resource, src_resource,
                    state, stride, &offset,
                    buf_src_size, buf_dst_size);
        } else {
            COMMAND_LIST_LOG(state, "  performing full copy\n");
            Profiling::resource_full_copies++;
            orig_context1->CopyResource(dst_resource, src_resource);
        }
    } else {
        COMMAND_LIST_LOG(state, "  copying by reference\n");
        Profiling::resource_reference_copies++;
        dst_resource = src_resource;
        if (src_view && (equiv_target(src.type) == equiv_target(dst.type))) {
            dst_view = src_view;
        } else if (*pp_cached_view) {
            if (view_matches_resource(*pp_cached_view, dst_resource)) {
                dst_view = *pp_cached_view;
            } else {
                LOG_DEBUG("Resource copying: Releasing stale view cache\n");
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
        dst_view = create_compatible_view(&dst, dst_resource, state,
                stride, offset, format, buf_src_size, options);
        // Not checking for NULL return as view's are not applicable to
        // all types. Legitimate failures are logged.
        *pp_cached_view = dst_view;
    }

    dst.SetResource(state, dst_resource, dst_view, stride, offset, format, buf_dst_size);

    if (options & ResourceCopyOptions::SET_VIEWPORT)
        set_viewport_from_resource(state, dst_resource);

out_release:

    if ((options & ResourceCopyOptions::NO_VIEW_CACHE || src.forbidViewCache)
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
