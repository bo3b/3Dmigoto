#pragma once

#include <memory>

#include "HackerDevice.h"
#include "HackerContext.h"

// Forward declarations to resolve circular includes (we include Hacker*.h,
// which includes Globals.h, which includes us):
class HackerDevice;
class HackerContext;

struct ShaderOverrideState {
	// Used to avoid querying the render target dimensions twice in the
	// common case we are going to store both width & height in separate
	// ini params:
	float rt_width, rt_height;

	ShaderOverrideState() :
		rt_width(-1),
		rt_height(-1)
	{}
};

class ShaderOverrideCommand {
public:
	virtual ~ShaderOverrideCommand() {};

	virtual void run(HackerContext*, ID3D11DeviceContext*, ShaderOverrideState*) = 0;
};

// Using vector of pointers to allow mixed types, and unique_ptr to handle
// destruction of each object:
typedef std::vector<std::unique_ptr<ShaderOverrideCommand>> ShaderOverrideCommandList;

void RunShaderOverrideCommandList(HackerDevice *mHackerDevice,
		HackerContext *mHackerContext,
		ShaderOverrideCommandList *command_list);
