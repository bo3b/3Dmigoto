#include "profiling.h"
#include "globals.h"

#include <algorithm>

class Profiling::Overhead {
public:
	LARGE_INTEGER cpu;

	void clear();
};

void Profiling::Overhead::clear()
{
	cpu.QuadPart = 0;
}

namespace Profiling {
	Mode mode;
	Overhead present_overhead;
	Overhead overlay_overhead;
	Overhead draw_overhead;
	Overhead map_overhead;
	Overhead hash_tracking_overhead;
	Overhead stat_overhead;
	wstring text;
	INT64 interval;
	bool freeze;
}

static LARGE_INTEGER profiling_start_time;
static unsigned start_frame_no;

static const struct D3D11_QUERY_DESC query_timestamp = {
	D3D11_QUERY_TIMESTAMP,
	0,
};

void Profiling::start(State *state)
{
	QueryPerformanceCounter(&state->start_time);
}

void Profiling::end(State *state, Profiling::Overhead *overhead)
{
	LARGE_INTEGER end_time;

	QueryPerformanceCounter(&end_time);
	overhead->cpu.QuadPart += end_time.QuadPart - state->start_time.QuadPart;
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
	wchar_t buf[512];

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

	Profiling::text += L" (Summary):\n";
	_snwprintf_s(buf, ARRAYSIZE(buf), _TRUNCATE,
			    L"     Present overhead: %7.2fus/frame ~%ffps\n"
			    L"     Overlay overhead: %7.2fus/frame ~%ffps\n"
			    L"   Draw call overhead: %7.2fus/frame ~%ffps\n"
			    L"  Command lists total: %7.2fus/frame ~%ffps\n"
			    L"   Map/Unmap overhead: %7.2fus/frame ~%ffps\n"
			    L"track_texture_updates: %7.2fus/frame ~%ffps\n"
			    L"  dump_usage overhead: %7.2fus/frame ~%ffps\n",
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
			    60.0 * stat_overhead.QuadPart / collection_duration.QuadPart
	);
	Profiling::text += buf;
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
	freeze = false;

	start_frame_no = G->frame_no;
	QueryPerformanceCounter(&profiling_start_time);
}
