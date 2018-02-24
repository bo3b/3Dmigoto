// Injector.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "Injector.h"

#include <windows.h>
#include <stdio.h>


int main()
{
	HMODULE module;
	FARPROC fn;
	HHOOK hook;

	module = LoadLibraryA("d3d11.dll");
	if (!module) {
		printf("Unable to load 3DMigoto d3d11.dll\n");
		getchar();
		return EXIT_FAILURE;
	}

	fn = GetProcAddress(module, "CBTProc");
	if (!fn) {
		printf("d3d11.dll doesn't support hook method - make sure this is a 3DMigoto DLL\n");
		getchar();
		return EXIT_FAILURE;
	}

	hook = SetWindowsHookEx(WH_CBT, (HOOKPROC)fn, module, 0);
	if (!hook) {
		printf("Error installing hook\n");
		getchar();
		return EXIT_FAILURE;
	}

	printf("3DMigoto hook installed, now run the game.\n"
		"\n"
		"Press enter when finished.\n");
	getchar();

	UnhookWindowsHookEx(hook);

    return 0;
}

