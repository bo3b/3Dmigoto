// Hunting
//  This code is to implement the Hunting mechanism as a separate compilation from the main wrapper code.
//  It implements all the shader management based on user input via key presses from Input.

#include "HackerDevice.h"

// Custom #include handler used to track which shaders need to be reloaded after an included file is modified
class MigotoIncludeHandler : public ID3DInclude
{
	std::vector<std::string> dir_stack;

	void push_dir(const char *path);
public:
	MigotoIncludeHandler(const char *path);

	STDMETHOD(Open)(D3D_INCLUDE_TYPE IncludeType, LPCSTR pFileName, LPCVOID pParentData, LPCVOID *ppData, UINT *pBytes);
	STDMETHOD(Close)(LPCVOID pData);
};

void TimeoutHuntingBuffers();
void ParseHuntingSection();
void DumpUsage(wchar_t *dir);
