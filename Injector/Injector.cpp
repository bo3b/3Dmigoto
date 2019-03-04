// Injector.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "Injector.h"
#include "util_min.h"

#include <windows.h>
#include <stdio.h>

static void wait_keypress(const char *msg)
{
	puts(msg);
	getchar();
}

static void wait_exit(int code=0, char *msg="\nPress enter to close...\n")
{
	wait_keypress(msg);
	exit(code);
}

static void exit_usage(const char *msg)
{
	//                                                          80 column limit --------> \n
	printf("The injector is not configured correctly. Please copy the 3DMigoto d3d11.dll\n"
	       "and d3dx.ini into this directory, then edit the d3dx.ini's [Injector] section\n"
	       "to set the target executable and 3DMigoto module name.\n"
	       "\n"
	       "%s", msg);

	wait_exit(EXIT_FAILURE);
}

int main()
{
	char *buf, setting[MAX_PATH], module_path[MAX_PATH];
	DWORD filesize, readsize;
	const char *ini_section;
	wchar_t path[MAX_PATH];
	int rc = EXIT_FAILURE;
	HANDLE ini_file;
	HMODULE module;
	FARPROC fn;
	HHOOK hook;

	printf("\n------------------------------ 3DMigoto Injector -----------------------------\n\n");

	ini_file = CreateFile(L"d3dx.ini", GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (ini_file == INVALID_HANDLE_VALUE)
		exit_usage("Unable to open d3dx.ini\n");

	filesize = GetFileSize(ini_file, NULL);
	buf = new char[filesize + 1];
	if (!buf)
		wait_exit(EXIT_FAILURE, "Unable to allocate d3dx.ini buffer\n");

	if (!ReadFile(ini_file, buf, filesize, &readsize, 0) || filesize != readsize)
		wait_exit(EXIT_FAILURE, "Error reading d3dx.ini\n");

	CloseHandle(ini_file);

	ini_section = find_ini_section_lite(buf, "injector");
	if (!ini_section)
		exit_usage("d3dx.ini missing required [Injector] section\n");

	// Check that the target is configured. We don't do anything with this
	// setting from here other than to make sure it is set, because the
	// injection method we are using cannot single out a specific process.
	// Once 3DMigoto has been injected it into a process it will check this
	// value and bail if it is in the wrong one.
	if (!find_ini_setting_lite(ini_section, "target", setting, MAX_PATH))
		exit_usage("d3dx.ini [Injector] section missing required \"target\" setting\n");

	if (!find_ini_setting_lite(ini_section, "module", module_path, MAX_PATH))
		exit_usage("d3dx.ini [Injector] section missing required \"module\" setting\n");

	module = LoadLibraryA(module_path);
	if (!module) {
		printf("Unable to load 3DMigoto \"%s\"\n", module_path);
		wait_exit(EXIT_FAILURE);
	}

	GetModuleFileName(module, path, MAX_PATH);
	printf("Loaded %S\n\n", path);

	fn = GetProcAddress(module, "CBTProc");
	if (!fn) {
		wait_exit(EXIT_FAILURE, "Module does not support injection method\n"
			"Make sure this is a recent 3DMigoto d3d11.dll\n");
	}

	hook = SetWindowsHookEx(WH_CBT, (HOOKPROC)fn, module, 0);
	if (!hook)
		wait_exit(EXIT_FAILURE, "Error installing hook\n");

	wait_keypress("3DMigoto ready - Now run the game.\n"
	              "\n"
		      "Press enter when finished...\n");
	rc = EXIT_SUCCESS;

	UnhookWindowsHookEx(hook);
	delete [] buf;

	return rc;
}

