#include "api.h"

bool extension_dll_needs_cb(UINT size)
{
	return (size == 4096);
}
