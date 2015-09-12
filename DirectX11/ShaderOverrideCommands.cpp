#include "ShaderOverrideCommands.h"

void RunShaderOverrideCommandList(HackerDevice *mHackerDevice,
		HackerContext *mHackerContext,
		ShaderOverrideCommandList *command_list)
{
	ShaderOverrideCommandList::iterator i;
	ShaderOverrideState state;
	ID3D11DeviceContext *mOrigContext = mHackerContext->GetOrigContext();

	for (i = command_list->begin(); i < command_list->end(); i++) {
		(*i)->run(mHackerContext, mOrigContext, &state);
	}
}
