#pragma once

#include <wrl.h>
#include <string>

namespace Profiling {
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
	};

	void start(State *state);
	void end(State *state, Overhead *overhead);

	void update_txt();
	void clear();

	extern Mode mode;
	extern Overhead present_overhead;
	extern Overhead overlay_overhead;
	extern Overhead draw_overhead;
	extern Overhead map_overhead;
	extern Overhead hash_tracking_overhead;
	extern Overhead stat_overhead;
	extern std::wstring text;
	extern INT64 interval;
	extern bool freeze;
}
