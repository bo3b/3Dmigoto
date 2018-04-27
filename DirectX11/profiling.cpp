#include "profiling.h"
#include "globals.h"

#include "HackerDevice.h"
#include "HackerContext.h"

#include <algorithm>
#include <deque>

static const struct D3D11_QUERY_DESC query_disjoint = {
	D3D11_QUERY_TIMESTAMP_DISJOINT,
	0,
};
static const struct D3D11_QUERY_DESC query_timestamp = {
	D3D11_QUERY_TIMESTAMP,
	0,
};

static Microsoft::WRL::ComPtr<ID3D11Query> get_query(HackerDevice *device)
{
	Microsoft::WRL::ComPtr<ID3D11Query> ret;

	EnterCriticalSection(&G->mCriticalSection);

	if (device->profiling_pool.empty()) {
		device->GetPassThroughOrigDevice1()->CreateQuery(&query_timestamp, &ret);
	} else {
		ret = device->profiling_pool.back();
		device->profiling_pool.pop_back();
	}

	LeaveCriticalSection(&G->mCriticalSection);

	return ret;
}

static void put_query(HackerDevice *device, Microsoft::WRL::ComPtr<ID3D11Query> query)
{
	EnterCriticalSection(&G->mCriticalSection);
	device->profiling_pool.push_back(query);
	LeaveCriticalSection(&G->mCriticalSection);
}

typedef deque<pair<Microsoft::WRL::ComPtr<ID3D11Query>, Microsoft::WRL::ComPtr<ID3D11Query>>> GPUTimestampQueries;
class Profiling::Overhead {
public:
	LARGE_INTEGER cpu;
	GPUTimestampQueries queries;
	UINT64 gpu;

	void clear();
	bool tally(HackerDevice *device, HackerContext *context);
};

void Profiling::Overhead::clear()
{
	cpu.QuadPart = 0;
	gpu = 0;
	EnterCriticalSection(&G->mCriticalSection);
	queries.clear();
	LeaveCriticalSection(&G->mCriticalSection);
}

bool Profiling::Overhead::tally(HackerDevice *device, HackerContext *context)
{
	UINT64 start, end;

	EnterCriticalSection(&G->mCriticalSection);
	while (!queries.empty()) {
		if ((context->GetPassThroughOrigContext1()->GetData(queries.front().first.Get(), &start, sizeof(start), D3D11_ASYNC_GETDATA_DONOTFLUSH) == S_FALSE)
		 || (context->GetPassThroughOrigContext1()->GetData(queries.front().second.Get(), &end, sizeof(end), D3D11_ASYNC_GETDATA_DONOTFLUSH) == S_FALSE)) {
			LeaveCriticalSection(&G->mCriticalSection);
			return true;
		}
		gpu += end - start;

		put_query(device, queries.front().first);
		put_query(device, queries.front().second);
		queries.pop_front();
	}
	LeaveCriticalSection(&G->mCriticalSection);

	return false;
}

static bool tally_gpu_overhead(HackerDevice *device, HackerContext *context)
{
	bool tallying = false;

	tallying = Profiling::present_overhead.tally(device, context) || tallying;
	tallying = Profiling::overlay_overhead.tally(device, context) || tallying;
	tallying = Profiling::draw_overhead.tally(device, context) || tallying;
	tallying = Profiling::map_overhead.tally(device, context) || tallying;
	tallying = Profiling::hash_tracking_overhead.tally(device, context) || tallying;

	return tallying;
}

namespace Profiling {
	Mode mode;
	Overhead present_overhead;
	Overhead overlay_overhead;
	Overhead draw_overhead;
	Overhead map_overhead;
	Overhead hash_tracking_overhead;
	wstring text;
	INT64 interval;
	bool freeze;
}

static LARGE_INTEGER profiling_start_time;
static unsigned start_frame_no;
static int pending_final_queries;

void Profiling::start(State *state, HackerDevice *device, HackerContext *context)
{
	state->start_time_query.Reset();

	if (device && context) {
		state->start_time_query = get_query(device);
		if (state->start_time_query)
			context->GetPassThroughOrigContext1()->End(state->start_time_query.Get());
	}

	QueryPerformanceCounter(&state->start_time);
}

void Profiling::end(State *state, HackerDevice *device, HackerContext *context, Profiling::Overhead *overhead)
{
	LARGE_INTEGER end_time;
	Microsoft::WRL::ComPtr<ID3D11Query> end_time_query;

	// If we are currently waiting on final queries from the GPU to update
	// the display, don't add any new ones, and don't even tally CPU
	// overhead during this time. Also don't add any new queries if the
	// profiling display has been frozen by the user - otherwise we would
	// keep allocating new queries killing performance.
	if (pending_final_queries)
		return;

	QueryPerformanceCounter(&end_time);
	overhead->cpu.QuadPart += end_time.QuadPart - state->start_time.QuadPart;

	if (state->start_time_query) {
		end_time_query = get_query(device);
		if (end_time_query) {
			context->GetPassThroughOrigContext1()->End(end_time_query.Get());
			EnterCriticalSection(&G->mCriticalSection);
			overhead->queries.emplace_back(state->start_time_query, end_time_query);
			LeaveCriticalSection(&G->mCriticalSection);
		}
	}
}

static void update_txt_summary(LARGE_INTEGER collection_duration, LARGE_INTEGER freq,
		UINT64 gpu_freq, unsigned frames)
{
	LARGE_INTEGER cpu_present_overhead = {0};
	LARGE_INTEGER cpu_command_list_overhead = {0};
	LARGE_INTEGER cpu_overlay_overhead;
	LARGE_INTEGER cpu_draw_overhead;
	LARGE_INTEGER cpu_map_overhead;
	LARGE_INTEGER cpu_hash_tracking_overhead;
	UINT64 gpu_present_overhead = Profiling::present_overhead.gpu * 1000000 / gpu_freq;
	UINT64 gpu_overlay_overhead = Profiling::overlay_overhead.gpu * 1000000 / gpu_freq;
	UINT64 gpu_draw_overhead = Profiling::draw_overhead.gpu * 1000000 / gpu_freq;
	UINT64 gpu_map_overhead = Profiling::map_overhead.gpu * 1000000 / gpu_freq;
	wchar_t buf[512];

	// The overlay overhead should be a subset of the present overhead, but
	// given that it includes the overhead of drawing the profiling HUD we
	// want it counted separately. The > check is to stop the case where
	// the profiling overlay was only just turned on and the first frame
	// won't have counted any present overhead yet, but will have counted
	// overlay overhead:
	if (Profiling::present_overhead.cpu.QuadPart > Profiling::overlay_overhead.cpu.QuadPart)
		cpu_present_overhead.QuadPart = Profiling::present_overhead.cpu.QuadPart - Profiling::overlay_overhead.cpu.QuadPart;

	for (CommandList *command_list : command_lists_profiling)
		cpu_command_list_overhead.QuadPart += command_list->time_spent_exclusive.QuadPart;

	cpu_present_overhead.QuadPart = cpu_present_overhead.QuadPart * 1000000 / freq.QuadPart;
	cpu_command_list_overhead.QuadPart = cpu_command_list_overhead.QuadPart * 1000000 / freq.QuadPart;
	cpu_overlay_overhead.QuadPart = Profiling::overlay_overhead.cpu.QuadPart * 1000000 / freq.QuadPart;
	cpu_draw_overhead.QuadPart = Profiling::draw_overhead.cpu.QuadPart * 1000000 / freq.QuadPart;
	cpu_map_overhead.QuadPart = Profiling::map_overhead.cpu.QuadPart * 1000000 / freq.QuadPart;
	cpu_hash_tracking_overhead.QuadPart = Profiling::hash_tracking_overhead.cpu.QuadPart * 1000000 / freq.QuadPart;

	Profiling::text += L" (Summary):\n"
	                   L"                     CPU/frame ~fps cost | GPU/frame ~fps cost\n"
	                   L"                     --------- --------- | --------- ---------\n";
	_snwprintf_s(buf, ARRAYSIZE(buf), _TRUNCATE,
	                   L"   Present overhead: %7.2fus %9f | %7.2fus %9f\n"
	                   L"   Overlay overhead: %7.2fus %9f | %7.2fus %9f\n"
	                   L" Draw call overhead: %7.2fus %9f | %7.2fus %9f\n"
	                   L"Command lists total: %7.2fus %9f |\n" // TODO: GPU overhead
	                   L" Map/Unmap overhead: %7.2fus %9f | %7.2fus %9f\n"
	                   L"Hash Track overhead: %7.2fus %9f |\n",
	                   (float)cpu_present_overhead.QuadPart / frames,
	                   60.0 * cpu_present_overhead.QuadPart / collection_duration.QuadPart,
	                   (float)gpu_present_overhead / frames,
	                   60.0 * gpu_present_overhead / collection_duration.QuadPart,

	                   (float)cpu_overlay_overhead.QuadPart / frames,
	                   60.0 * cpu_overlay_overhead.QuadPart / collection_duration.QuadPart,
	                   (float)gpu_overlay_overhead / frames,
	                   60.0 * gpu_overlay_overhead / collection_duration.QuadPart,

	                   (float)cpu_draw_overhead.QuadPart / frames,
	                   60.0 * cpu_draw_overhead.QuadPart / collection_duration.QuadPart,
	                   (float)gpu_draw_overhead / frames,
	                   60.0 * gpu_draw_overhead / collection_duration.QuadPart,

	                   (float)cpu_command_list_overhead.QuadPart / frames,
	                   60.0 * cpu_command_list_overhead.QuadPart / collection_duration.QuadPart,
			   // TODO: GPU overhead

	                   (float)cpu_map_overhead.QuadPart / frames,
	                   60.0 * cpu_map_overhead.QuadPart / collection_duration.QuadPart,
	                   (float)gpu_map_overhead / frames,
	                   60.0 * gpu_map_overhead / collection_duration.QuadPart,

	                   (float)cpu_hash_tracking_overhead.QuadPart / frames,
	                   60.0 * cpu_hash_tracking_overhead.QuadPart / collection_duration.QuadPart
			   // Hash tracking does not issue any GPU commands
	);
	Profiling::text += buf;
}

static void update_txt_command_lists(LARGE_INTEGER collection_duration, LARGE_INTEGER freq,
		UINT64 gpu_freq, unsigned frames)
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

static void update_txt_commands(LARGE_INTEGER collection_duration, LARGE_INTEGER freq,
		UINT64 gpu_freq, unsigned frames)
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

		_snwprintf_s(buf, ARRAYSIZE(buf), _TRUNCATE,
				L"%5.0f %7.2fus %9f | %5.0f %7.2fus %9f %s\n",
				ceil((float)cmd->pre_executions / frames),
				(float)pre_time_spent.QuadPart / frames,
				pre_fps_cost,
				ceil((float)cmd->post_executions / frames),
				(float)post_time_spent.QuadPart / frames,
				post_fps_cost,
				cmd->ini_line.c_str()
		);
		Profiling::text += buf;
		// TODO: GPU time spent
	}
}

void Profiling::update_txt(HackerDevice *device, HackerContext *context)
{
	static LARGE_INTEGER freq = {0};
	static UINT64 gpu_freq = 0;
	D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjoint;
	static LARGE_INTEGER end_time, collection_duration;
	static unsigned frames;
	wchar_t buf[256];
	bool tallying;

	// Do this every frame to drain the queries as they become ready,
	// otherwise we will stall when we try to drain them all at once to
	// update the display
	tallying = tally_gpu_overhead(device, context);

	if (freeze)
		return;

	if (!pending_final_queries) {
		QueryPerformanceCounter(&end_time);

		if (!freq.QuadPart)
			QueryPerformanceFrequency(&freq);
		// Safety - in case of zero frequency avoid divide by zero:
		if (!freq.QuadPart)
			return;

		collection_duration.QuadPart = (end_time.QuadPart - profiling_start_time.QuadPart) * 1000000 / freq.QuadPart;
		if (collection_duration.QuadPart < interval && !Profiling::text.empty())
			return;

		frames = G->frame_no - start_frame_no;
	}

	if (device && context && device->disjoint_query) {
		if (!pending_final_queries) {
			context->GetPassThroughOrigContext1()->End(device->disjoint_query);
			pending_final_queries = 1;
		}

		if (pending_final_queries == 1 && context->GetPassThroughOrigContext1()->GetData(device->disjoint_query, &disjoint, sizeof(disjoint), D3D11_ASYNC_GETDATA_DONOTFLUSH) == S_FALSE) {
			return;
		}

		pending_final_queries = 2;

		// FIXME: Disjoint query is only sometimes returning the
		// frequency. Not positive why - maybe the game is also running
		// a disjoint query that is interfering with ours? For now use
		// the last valid frequency it reported:
		LogDebug("Disjoint: %i Frequency: %llu\n", disjoint.Disjoint, disjoint.Frequency);
		if (disjoint.Frequency)
			gpu_freq = disjoint.Frequency;
	}

	if (tallying)
		return;
	pending_final_queries = 0;

	if (frames && collection_duration.QuadPart) {
		_snwprintf_s(buf, ARRAYSIZE(buf), _TRUNCATE,
				    L"Performance Monitor %.1ffps", frames * 1000000.0 / collection_duration.QuadPart);
		Profiling::text = buf;

		switch (Profiling::mode) {
			case Profiling::Mode::SUMMARY:
				update_txt_summary(collection_duration, freq, gpu_freq, frames);
				break;
			case Profiling::Mode::TOP_COMMAND_LISTS:
				update_txt_command_lists(collection_duration, freq, gpu_freq, frames);
				break;
			case Profiling::Mode::TOP_COMMANDS:
				update_txt_commands(collection_duration, freq, gpu_freq, frames);
				break;
		}
	}

	// Restart profiling for the next time interval:
	clear(device, context);
}

void Profiling::clear(HackerDevice *device, HackerContext *context)
{
	command_lists_profiling.clear();
	command_lists_cmd_profiling.clear();
	present_overhead.clear();
	overlay_overhead.clear();
	draw_overhead.clear();
	map_overhead.clear();
	hash_tracking_overhead.clear();
	freeze = false;

	if (mode != Mode::NONE) {
		start_frame_no = G->frame_no;
		QueryPerformanceCounter(&profiling_start_time);
		if (device && context && device->disjoint_query)
			context->GetPassThroughOrigContext1()->Begin(device->disjoint_query);
	} else if (device && context && device->disjoint_query) {
		context->GetPassThroughOrigContext1()->End(device->disjoint_query);
	}
}

void Profiling::create_disjoint_query(HackerDevice *device)
{
	device->GetPassThroughOrigDevice1()->CreateQuery(&query_disjoint, &device->disjoint_query);
}
