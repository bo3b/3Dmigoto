#include "profiling.h"
#include "globals.h"

#include <algorithm>
#include <DirectXMath.h>

void Profiling::Overhead::clear()
{
	cpu.QuadPart = 0;
	count = 0;
	hits = 0;
}

namespace Profiling {
	Mode mode;
	Overhead present_overhead;
	Overhead overlay_overhead;
	Overhead draw_overhead;
	Overhead map_overhead;
	Overhead hash_tracking_overhead;
	Overhead stat_overhead;
	Overhead shaderregex_overhead;
	Overhead cursor_overhead;
	Overhead nvapi_overhead;
	wstring text;
	wstring cto_warning;
	INT64 interval;
	bool freeze;

	Overhead shader_hash_lookup_overhead;
	Overhead shader_reload_lookup_overhead;
	Overhead shader_original_lookup_overhead;
	Overhead shaderoverride_lookup_overhead;
	Overhead texture_handle_info_lookup_overhead;
	Overhead textureoverride_lookup_overhead;
	Overhead resource_pool_lookup_overhead;

	unsigned resource_full_copies;
	unsigned resource_reference_copies;
	unsigned stereo2mono_copies;
	unsigned msaa_resolutions;
	unsigned buffer_region_copies;
	unsigned surfaces_cleared;
	unsigned resources_created;
	unsigned resource_pool_swaps;
	unsigned max_copies_per_frame_exceeded;
	unsigned injected_draw_calls;
	unsigned skipped_draw_calls;
	unsigned max_executions_per_frame_exceeded;
	unsigned iniparams_updates;
}

static LARGE_INTEGER profiling_start_time;
static unsigned start_frame_no;

//static const struct D3D11_QUERY_DESC query_timestamp = {
//	D3D11_QUERY_TIMESTAMP,
//	0,
//};
typedef struct D3D9_QUERY_DESC
{
	::D3DQUERYTYPE Query;
	UINT MiscFlags;
} 	D3D9_QUERY_DESC;
static const struct D3D9_QUERY_DESC query_timestamp = {
	::D3DQUERYTYPE::D3DQUERYTYPE_TIMESTAMP,
	0,
};
static std::unordered_set<CommandList*> warned_cto_command_lists;

static void cto_warn_post_commands(CommandList *command_list)
{
	IfCommand *if_command;
	RunExplicitCommandList *run_command;

	// Only visit each command list at most once both to keep the list
	// shorter and make sure we can't enter an infinite recursion if a list
	// calls itself:
	if (warned_cto_command_lists.count(command_list))
		return;
	warned_cto_command_lists.insert(command_list);

	for (auto &command : command_list->commands) {
		if_command = dynamic_cast<IfCommand*>(command.get());
		if (if_command) {
			// If commands aren't the culprits - it's whatever
			// stopped their post phases from being optimised out
			// we should warn about:
			cto_warn_post_commands(if_command->true_commands_post.get());
			cto_warn_post_commands(if_command->false_commands_post.get());
		} else
			Profiling::cto_warning += command->ini_line + L"\n";

		run_command = dynamic_cast<RunExplicitCommandList*>(command.get());
		if (run_command) {
			// Run commands can be the culprits themselves, or
			// what they contain, so list the run command and
			// recurse into the command lists it calls:
			if (run_command->run_pre_and_post_together)
				cto_warn_post_commands(&run_command->command_list_section->command_list);
			cto_warn_post_commands(&run_command->command_list_section->post_command_list);
		}
	}
}

void Profiling::update_cto_warning(bool warn)
{
	Profiling::cto_warning.clear();

	if (!warn)
		return;

	Profiling::cto_warning = L"\nThe following commands prevented optimising out all implicit post checktextureoverrides:\n";
	warned_cto_command_lists.clear();

	for (auto &tolkv : G->mTextureOverrideMap) {
		for (TextureOverride &to : tolkv.second)
			cto_warn_post_commands(&to.post_command_list);
	}
	for (auto &tof : G->mFuzzyTextureOverrides)
		cto_warn_post_commands(&tof->texture_override->post_command_list);

	LogInfoNoNL("%S", Profiling::cto_warning.c_str());
}

static void update_txt_cto_warning()
{
	Profiling::text += L" (post [TextureOverride] commands):\n" + Profiling::cto_warning;
}

static void update_txt_summary(LARGE_INTEGER collection_duration, LARGE_INTEGER freq, unsigned frames)
{
	LARGE_INTEGER present_overhead = {0};
	LARGE_INTEGER command_list_overhead = {0};
	LARGE_INTEGER overlay_overhead;
	LARGE_INTEGER draw_overhead;
	LARGE_INTEGER map_overhead;
	LARGE_INTEGER hash_tracking_overhead;
	LARGE_INTEGER stat_overhead;
	LARGE_INTEGER shaderregex_overhead;
	LARGE_INTEGER cursor_overhead;
	LARGE_INTEGER nvapi_overhead;
	LARGE_INTEGER shader_hash_lookup_overhead;
	LARGE_INTEGER shader_reload_lookup_overhead;
	LARGE_INTEGER shader_original_lookup_overhead;
	LARGE_INTEGER shaderoverride_lookup_overhead;
	LARGE_INTEGER texture_handle_info_lookup_overhead;
	LARGE_INTEGER textureoverride_lookup_overhead;
	LARGE_INTEGER resource_pool_lookup_overhead;
	wchar_t buf[1024];

	// The overlay overhead should be a subset of the present overhead, but
	// given that it includes the overhead of drawing the profiling HUD we
	// want it counted separately. The > check is to stop the case where
	// the profiling overlay was only just turned on and the first frame
	// won't have counted any present overhead yet, but will have counted
	// overlay overhead:
	if (Profiling::present_overhead.cpu.QuadPart > Profiling::overlay_overhead.cpu.QuadPart)
		present_overhead.QuadPart = Profiling::present_overhead.cpu.QuadPart - Profiling::overlay_overhead.cpu.QuadPart;

	for (CommandList *command_list : command_lists_profiling)
		command_list_overhead.QuadPart += command_list->time_spent_exclusive.QuadPart;

	present_overhead.QuadPart = present_overhead.QuadPart * 1000000 / freq.QuadPart;
	command_list_overhead.QuadPart = command_list_overhead.QuadPart * 1000000 / freq.QuadPart;
	overlay_overhead.QuadPart = Profiling::overlay_overhead.cpu.QuadPart * 1000000 / freq.QuadPart;
	draw_overhead.QuadPart = Profiling::draw_overhead.cpu.QuadPart * 1000000 / freq.QuadPart;
	map_overhead.QuadPart = Profiling::map_overhead.cpu.QuadPart * 1000000 / freq.QuadPart;
	hash_tracking_overhead.QuadPart = Profiling::hash_tracking_overhead.cpu.QuadPart * 1000000 / freq.QuadPart;
	stat_overhead.QuadPart = Profiling::stat_overhead.cpu.QuadPart * 1000000 / freq.QuadPart;
	shaderregex_overhead.QuadPart = Profiling::shaderregex_overhead.cpu.QuadPart * 1000000 / freq.QuadPart;
	cursor_overhead.QuadPart = Profiling::cursor_overhead.cpu.QuadPart * 1000000 / freq.QuadPart;
	nvapi_overhead.QuadPart = Profiling::nvapi_overhead.cpu.QuadPart * 1000000 / freq.QuadPart;

	shader_hash_lookup_overhead.QuadPart = Profiling::shader_hash_lookup_overhead.cpu.QuadPart * 1000000 / freq.QuadPart;
	shader_reload_lookup_overhead.QuadPart = Profiling::shader_reload_lookup_overhead.cpu.QuadPart * 1000000 / freq.QuadPart;
	shader_original_lookup_overhead.QuadPart = Profiling::shader_original_lookup_overhead.cpu.QuadPart * 1000000 / freq.QuadPart;
	shaderoverride_lookup_overhead.QuadPart = Profiling::shaderoverride_lookup_overhead.cpu.QuadPart * 1000000 / freq.QuadPart;
	texture_handle_info_lookup_overhead.QuadPart = Profiling::texture_handle_info_lookup_overhead.cpu.QuadPart * 1000000 / freq.QuadPart;
	textureoverride_lookup_overhead.QuadPart = Profiling::textureoverride_lookup_overhead.cpu.QuadPart * 1000000 / freq.QuadPart;
	resource_pool_lookup_overhead.QuadPart = Profiling::resource_pool_lookup_overhead.cpu.QuadPart * 1000000 / freq.QuadPart;

	Profiling::text += L" (CPU Performance Summary):\n";
	_snwprintf_s(buf, ARRAYSIZE(buf), _TRUNCATE,
			    L"     Present overhead: %7.2fus/frame ~%ffps\n"
			    L"     Overlay overhead: %7.2fus/frame ~%ffps\n"
			    L"   Draw call overhead: %7.2fus/frame ~%ffps\n"
			    L"  Command lists total: %7.2fus/frame ~%ffps\n"
			    L"   Map/Unmap overhead: %7.2fus/frame ~%ffps\n"
			    L"track_texture_updates: %7.2fus/frame ~%ffps\n"
			    L"  dump_usage overhead: %7.2fus/frame ~%ffps\n"
			    L" ShaderRegex overhead: %7.2fus/frame ~%ffps\n"
			    L"Mouse cursor overhead: %7.2fus/frame ~%ffps\n"
			    L"       NvAPI overhead: %7.2fus/frame ~%ffps\n"
			    ,
			    (float)present_overhead.QuadPart / frames,
			    60.0 * present_overhead.QuadPart / collection_duration.QuadPart,

			    (float)overlay_overhead.QuadPart / frames,
			    60.0 * overlay_overhead.QuadPart / collection_duration.QuadPart,

			    (float)draw_overhead.QuadPart / frames,
			    60.0 * draw_overhead.QuadPart / collection_duration.QuadPart,

			    (float)command_list_overhead.QuadPart / frames,
			    60.0 * command_list_overhead.QuadPart / collection_duration.QuadPart,

			    (float)map_overhead.QuadPart / frames,
			    60.0 * map_overhead.QuadPart / collection_duration.QuadPart,

			    (float)hash_tracking_overhead.QuadPart / frames,
			    60.0 * hash_tracking_overhead.QuadPart / collection_duration.QuadPart,

			    (float)stat_overhead.QuadPart / frames,
			    60.0 * stat_overhead.QuadPart / collection_duration.QuadPart,

			    (float)shaderregex_overhead.QuadPart / frames,
			    60.0 * shaderregex_overhead.QuadPart / collection_duration.QuadPart,

			    (float)cursor_overhead.QuadPart / frames,
			    60.0 * cursor_overhead.QuadPart / collection_duration.QuadPart,

			    (float)nvapi_overhead.QuadPart / frames,
			    60.0 * nvapi_overhead.QuadPart / collection_duration.QuadPart
	);
	Profiling::text += buf;

	_snwprintf_s(buf, ARRAYSIZE(buf), _TRUNCATE,
			    L"\n"
			    L"Map Lookups (CPU):\n"
			    L"          Shader hash: %7.2fus/frame ~%ffps (%u/%u hits/frame)\n"
			    L"Live reloaded shaders: %7.2fus/frame ~%ffps (%u/%u hits/frame)\n"
			    L"     Original shaders: %7.2fus/frame ~%ffps (%u/%u hits/frame)\n"
			    L"       ShaderOverride: %7.2fus/frame ~%ffps (%u/%u hits/frame)\n"
			    L"  Texture hash / info: %7.2fus/frame ~%ffps (%u/%u hits/frame)\n"
			    L"      TextureOverride: %7.2fus/frame ~%ffps (%u/%u hits/frame)\n"
			    L"       Resource pools: %7.2fus/frame ~%ffps (%u/%u hits/frame)\n"
			    ,
			    (float)shader_hash_lookup_overhead.QuadPart / frames,
			    60.0 * shader_hash_lookup_overhead.QuadPart / collection_duration.QuadPart,
			    Profiling::shader_hash_lookup_overhead.hits / frames,
			    Profiling::shader_hash_lookup_overhead.count / frames,

			    (float)shader_reload_lookup_overhead.QuadPart / frames,
			    60.0 * shader_reload_lookup_overhead.QuadPart / collection_duration.QuadPart,
			    Profiling::shader_reload_lookup_overhead.hits / frames,
			    Profiling::shader_reload_lookup_overhead.count / frames,

			    (float)shader_original_lookup_overhead.QuadPart / frames,
			    60.0 * shader_original_lookup_overhead.QuadPart / collection_duration.QuadPart,
			    Profiling::shader_original_lookup_overhead.hits / frames,
			    Profiling::shader_original_lookup_overhead.count / frames,

			    (float)shaderoverride_lookup_overhead.QuadPart / frames,
			    60.0 * shaderoverride_lookup_overhead.QuadPart / collection_duration.QuadPart,
			    Profiling::shaderoverride_lookup_overhead.hits / frames,
			    Profiling::shaderoverride_lookup_overhead.count / frames,

			    (float)texture_handle_info_lookup_overhead.QuadPart / frames,
			    60.0 * texture_handle_info_lookup_overhead.QuadPart / collection_duration.QuadPart,
			    Profiling::texture_handle_info_lookup_overhead.hits / frames,
			    Profiling::texture_handle_info_lookup_overhead.count / frames,

			    (float)textureoverride_lookup_overhead.QuadPart / frames,
			    60.0 * textureoverride_lookup_overhead.QuadPart / collection_duration.QuadPart,
			    Profiling::textureoverride_lookup_overhead.hits / frames,
			    Profiling::textureoverride_lookup_overhead.count / frames,

			    (float)resource_pool_lookup_overhead.QuadPart / frames,
			    60.0 * resource_pool_lookup_overhead.QuadPart / collection_duration.QuadPart,
			    Profiling::resource_pool_lookup_overhead.hits / frames,
			    Profiling::resource_pool_lookup_overhead.count / frames
	);
	Profiling::text += buf;

	_snwprintf_s(buf, ARRAYSIZE(buf), _TRUNCATE,
			    L"\n"
			    L"GPU Performance Impacting Stats (costs are guidelines only):\n"
			    L"   IniParams GPU resource updates: %4u/frame (%Iu bytes)\n"
			    L"             Full resource copies: %4u/frame (High cost)\n"
			    L"     By-Reference resource copies: %4u/frame (Low cost)\n"
			    L"      stereo2mono resource copies: %4u/frame (Extremely high cost on SLI)\n"
			    L"          MSAA resources resolved: %4u/frame (High cost)\n"
			    L"             Region buffer copies: %4u/frame\n"
			    L"                Resources cleared: %4u/frame (Cost saving in some circumstances, e.g. SLI)\n"
			    L"            Resources [re]created: %4u       (High cost)\n"
			    L"              Resource pool swaps: %4u/frame (Low cost)\n"
			    L"    max_copies_per_frame exceeded: %4u/frame (Cost saving)\n"
			    L"     Injected draw/dispatch calls: %4u/frame\n"
			    L"               Skipped draw calls: %4u/frame (Cost saving)\n"
			    L"max_executions_per_frame exceeded: %4u/frame (Cost saving)\n"
			    ,
			    Profiling::iniparams_updates / frames, G->IniConstants.size() * sizeof(DirectX::XMFLOAT4),
			    Profiling::resource_full_copies / frames,
			    Profiling::resource_reference_copies / frames,
			    Profiling::stereo2mono_copies / frames,
			    Profiling::msaa_resolutions / frames,
			    Profiling::buffer_region_copies / frames,
			    Profiling::surfaces_cleared / frames,
			    Profiling::resources_created,
			    Profiling::resource_pool_swaps / frames,
			    Profiling::max_copies_per_frame_exceeded / frames,
			    Profiling::injected_draw_calls / frames,
			    Profiling::skipped_draw_calls / frames,
			    Profiling::max_executions_per_frame_exceeded / frames
	);
	Profiling::text += buf;

	if (G->implicit_post_checktextureoverride_used && !Profiling::cto_warning.empty())
		Profiling::text += L"\nImplicit post checktextureoverrides were not optimised out\n";
}

static void update_txt_command_lists(LARGE_INTEGER collection_duration, LARGE_INTEGER freq, unsigned frames)
{
	LARGE_INTEGER inclusive, exclusive;
	double inclusive_fps, exclusive_fps;
	wchar_t buf[256];

	vector<CommandList*> sorted(command_lists_profiling.begin(), command_lists_profiling.end());
	std::sort(sorted.begin(), sorted.end(), [](const CommandList *lhs, const CommandList *rhs) {
		return lhs->time_spent_inclusive.QuadPart > rhs->time_spent_inclusive.QuadPart;
	});

	Profiling::text += L" (Top Command Lists):\n"
			    L"      | Including sub-lists | Excluding sub-lists |\n"
			    L"count | CPU/frame ~fps cost | CPU/frame ~fps cost |\n"
			    L"----- | --------- --------- | --------- --------- |\n";
	for (CommandList *command_list : sorted) {
		inclusive.QuadPart = command_list->time_spent_inclusive.QuadPart * 1000000 / freq.QuadPart;
		exclusive.QuadPart = command_list->time_spent_exclusive.QuadPart * 1000000 / freq.QuadPart;

		// fps estimate based on the assumption that if we took 100%
		// CPU time it would cost all 60fps:
		inclusive_fps = 60.0 * inclusive.QuadPart / collection_duration.QuadPart;
		exclusive_fps = 60.0 * exclusive.QuadPart / collection_duration.QuadPart;

		_snwprintf_s(buf, ARRAYSIZE(buf), _TRUNCATE,
				L"%5.0f | %7.2fus %9f | %7.2fus %9f | %4s [%s]\n",
				ceil((float)command_list->executions / frames),
				(float)inclusive.QuadPart / frames,
				inclusive_fps,
				(float)exclusive.QuadPart / frames,
				exclusive_fps,
				command_list->post ? L"post" : L"pre",
				command_list->ini_section.c_str()
		);
		Profiling::text += buf;
		// TODO: GPU time spent
	}
}

static void update_txt_commands(LARGE_INTEGER collection_duration, LARGE_INTEGER freq, unsigned frames)
{
	LARGE_INTEGER pre_time_spent, post_time_spent;
	double pre_fps_cost, post_fps_cost;
	wchar_t buf[256];

	vector<CommandListCommand*> sorted(command_lists_cmd_profiling.begin(), command_lists_cmd_profiling.end());

	std::sort(sorted.begin(), sorted.end(), [](const CommandListCommand *lhs, const CommandListCommand *rhs) {
		return (lhs->pre_time_spent.QuadPart + lhs->post_time_spent.QuadPart) >
		       (rhs->pre_time_spent.QuadPart + rhs->post_time_spent.QuadPart);
	});

	Profiling::text += L" (Top Commands):\n"
			    L"         pre              |         post\n"
			    L"count CPU/frame ~fps cost | count CPU/frame ~fps cost\n"
			    L"----- --------- --------- | ----- --------- ---------\n";
	for (CommandListCommand *cmd : sorted) {
		pre_time_spent.QuadPart = cmd->pre_time_spent.QuadPart * 1000000 / freq.QuadPart;
		post_time_spent.QuadPart = cmd->post_time_spent.QuadPart * 1000000 / freq.QuadPart;

		// fps estimate based on the assumption that if we took 100%
		// CPU time it would cost all 60fps:
		pre_fps_cost = 60.0 * pre_time_spent.QuadPart / collection_duration.QuadPart;
		post_fps_cost = 60.0 * post_time_spent.QuadPart / collection_duration.QuadPart;

		if (cmd->pre_executions) {
			_snwprintf_s(buf, ARRAYSIZE(buf), _TRUNCATE,
					L"%5.0f %7.2fus %9f | ",
					ceil((float)cmd->pre_executions / frames),
					(float)pre_time_spent.QuadPart / frames,
					pre_fps_cost);
			Profiling::text += buf;
		} else
			Profiling::text += L"                          | ";
		if (cmd->post_executions) {
			_snwprintf_s(buf, ARRAYSIZE(buf), _TRUNCATE,
					L"%5.0f %7.2fus %9f ",
					ceil((float)cmd->post_executions / frames),
					(float)post_time_spent.QuadPart / frames,
					post_fps_cost);
			Profiling::text += buf;
		} else
			Profiling::text += L"                          ";
		Profiling::text += cmd->ini_line;
		Profiling::text += L"\n";
		// TODO: GPU time spent
	}
}

void Profiling::update_txt()
{
	static LARGE_INTEGER freq = {0};
	LARGE_INTEGER end_time, collection_duration;
	unsigned frames = G->frame_no - start_frame_no;
	wchar_t buf[256];

	if (freeze)
		return;

	QueryPerformanceCounter(&end_time);
	if (!freq.QuadPart)
		QueryPerformanceFrequency(&freq);

	// Safety - in case of zero frequency avoid divide by zero:
	if (!freq.QuadPart)
		return;

	collection_duration.QuadPart = (end_time.QuadPart - profiling_start_time.QuadPart) * 1000000 / freq.QuadPart;
	if (collection_duration.QuadPart < interval && !Profiling::text.empty())
		return;

	if (frames && collection_duration.QuadPart) {
		_snwprintf_s(buf, ARRAYSIZE(buf), _TRUNCATE,
				    L"Performance Monitor %.1ffps", frames * 1000000.0 / collection_duration.QuadPart);
		Profiling::text = buf;

		switch (Profiling::mode) {
			case Profiling::Mode::SUMMARY:
				update_txt_summary(collection_duration, freq, frames);
				break;
			case Profiling::Mode::TOP_COMMAND_LISTS:
				update_txt_command_lists(collection_duration, freq, frames);
				break;
			case Profiling::Mode::TOP_COMMANDS:
				update_txt_commands(collection_duration, freq, frames);
				break;
			case Profiling::Mode::CTO_WARNING:
				update_txt_cto_warning();
				break;
		}
	}

	// Restart profiling for the next time interval:
	clear();
}

void Profiling::clear()
{
	command_lists_profiling.clear();
	command_lists_cmd_profiling.clear();
	present_overhead.clear();
	overlay_overhead.clear();
	draw_overhead.clear();
	map_overhead.clear();
	hash_tracking_overhead.clear();
	stat_overhead.clear();
	shaderregex_overhead.clear();
	cursor_overhead.clear();
	nvapi_overhead.clear();
	freeze = false;

	shader_hash_lookup_overhead.clear();
	shader_reload_lookup_overhead.clear();
	shader_original_lookup_overhead.clear();
	shaderoverride_lookup_overhead.clear();
	texture_handle_info_lookup_overhead.clear();
	textureoverride_lookup_overhead.clear();
	resource_pool_lookup_overhead.clear();

	resource_full_copies = 0;
	resource_reference_copies = 0;
	stereo2mono_copies = 0;
	msaa_resolutions = 0;
	buffer_region_copies = 0;
	surfaces_cleared = 0;
	resources_created = 0;
	resource_pool_swaps = 0;
	max_copies_per_frame_exceeded = 0;
	injected_draw_calls = 0;
	skipped_draw_calls = 0;
	max_executions_per_frame_exceeded = 0;
	iniparams_updates = 0;

	start_frame_no = G->frame_no;
	QueryPerformanceCounter(&profiling_start_time);
}
