#pragma once

#include <wrl.h>
#include <string>
#include <vector>
#include <d3d11_1.h>

class HackerDevice;
class HackerContext;

namespace Profiling {
	typedef std::vector<Microsoft::WRL::ComPtr<ID3D11Query>> Pool;

	enum class Mode {
		NONE = 0,
		SUMMARY,
		TOP_COMMAND_LISTS,
		TOP_COMMANDS,

		INVALID, // Must be last
	};

	class Overhead;

	struct State {
		LARGE_INTEGER start_time;
		Microsoft::WRL::ComPtr<ID3D11Query> start_time_query;
	};

	void start(State *state, HackerDevice *device, HackerContext *context);
	void end(State *state, HackerDevice *device, HackerContext *context, Overhead *overhead);

	void update_txt(HackerDevice *device, HackerContext *context);
	void clear(HackerDevice *device, HackerContext *context);

	void create_disjoint_query(HackerDevice *device);

	extern Mode mode;
	extern Overhead present_overhead;
	extern Overhead overlay_overhead;
	extern Overhead draw_overhead;
	extern Overhead map_overhead;
	extern Overhead hash_tracking_overhead;
	extern std::wstring text;
	extern INT64 interval;
	extern bool freeze;
}
