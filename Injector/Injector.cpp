// Injector.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include "Injector.h"
#include "util_min.h"

#include <windows.h>
#include <stdio.h>
#include <tlhelp32.h>

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

static bool check_file_description(const char *buf, const char *module_path)
{
	// https://docs.microsoft.com/en-gb/windows/desktop/api/winver/nf-winver-verqueryvaluea
	struct LANGANDCODEPAGE {
		WORD wLanguage;
		WORD wCodePage;
	} *translate_query;
	char id[50];
	char *file_description = "";
	unsigned int query_size, file_desc_size;
	HRESULT hr;
	int i;

	if (!VerQueryValueA(buf, "\\VarFileInfo\\Translation", (void**)&translate_query, &query_size))
		wait_exit(EXIT_FAILURE, "3DMigoto file information query failed\n");

	// Look for the 3DMigoto file description in all language blocks... We
	// could likely skip the loop since we know which language it should be
	// in, but for some reason we included it in the German section which
	// we might want to change, so this way we won't need to adjust this
	// code if we do:
	for (i = 0; i < (query_size / sizeof(struct LANGANDCODEPAGE)); i++) {
		hr = _snprintf_s(id, 50, 50, "\\StringFileInfo\\%04x%04x\\FileDescription",
				translate_query[i].wLanguage,
				translate_query[i].wCodePage);
		if (FAILED(hr))
			wait_exit(EXIT_FAILURE, "3DMigoto file description query bugged\n");

		if (!VerQueryValueA(buf, id, (void**)&file_description, &file_desc_size))
			wait_exit(EXIT_FAILURE, "3DMigoto file description query failed\n");

		// Only look for the 3Dmigoto prefix. We've had a whitespace
		// error in the description for all this time that we want to
		// ignore, and we later might want to add other 3DMigoto DLLs
		// like d3d9 and d3d12 with injection support
		printf("%s description: \"%s\"\n", module_path, file_description);
		if (!strncmp(file_description, "3Dmigoto", 8))
			return true;
	}

	return false;
}

static void check_3dmigoto_version(const char *module_path, const char *ini_section)
{
	VS_FIXEDFILEINFO *query = NULL;
	DWORD pointless_handle = 0;
	unsigned int size;
	char *buf;

	size = GetFileVersionInfoSizeA(module_path, &pointless_handle);
	if (!size)
		wait_exit(EXIT_FAILURE, "3DMigoto version size check failed\n");

	buf = new char[size];

	if (!GetFileVersionInfoA(module_path, pointless_handle, size, buf))
		wait_exit(EXIT_FAILURE, "3DMigoto version info check failed\n");

	if (!check_file_description(buf, module_path)) {
		printf("ERROR: The requested module \"%s\" is not 3DMigoto\n"
		       "Please ensure that [Injector] \"module\" is set correctly and the DLL is in place.", module_path);
		wait_exit(EXIT_FAILURE);
	}

	if (!VerQueryValueA(buf, "\\", (void**)&query, &size))
		wait_exit(EXIT_FAILURE, "3DMigoto version query check failed\n");

	printf("3DMigoto Version %d.%d.%d\n",
			query->dwProductVersionMS >> 16,
			query->dwProductVersionMS & 0xffff,
			query->dwProductVersionLS >> 16);

	if (query->dwProductVersionMS <  0x00010003 ||
	    query->dwProductVersionMS == 0x00010003 && query->dwProductVersionLS < 0x000f0000) {
		wait_exit(EXIT_FAILURE, "This version of 3DMigoto is too old to be safely injected - please use 1.3.15 or later\n");
	}

	delete [] buf;
}

static bool check_for_running_target(wchar_t *target)
{
	// https://docs.microsoft.com/en-us/windows/desktop/ToolHelp/taking-a-snapshot-and-viewing-processes
	HANDLE snapshot;
	PROCESSENTRY32 pe;
	bool rc = false;
	wchar_t *basename = wcsrchr(target, '\\');

	if (basename)
		basename++;
	else
		basename = target;

	snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
	if (snapshot == INVALID_HANDLE_VALUE)
		return false;

	pe.dwSize = sizeof(PROCESSENTRY32);
	if (!Process32First(snapshot, &pe))
		goto out_close;

	do {
		// TODO: Also check directory path if that has been specified
		if (_wcsicmp(pe.szExeFile, basename))
			continue;

		// TODO: Check the process modules (if we have permission) to
		// see if 3DMigoto has been injected

		rc = true;

		printf("Target process found (%i): %S\n", pe.th32ProcessID, pe.szExeFile);

		break;
	} while (Process32Next(snapshot, &pe));

out_close:
	CloseHandle(snapshot);
	return rc;
}

static void wait_for_target(const char *target_a, bool wait, int delay)
{
	wchar_t target_w[MAX_PATH];

	if (!MultiByteToWideChar(CP_UTF8, 0, target_a, -1, target_w, MAX_PATH))
		return;

	while (wait) {
		if (check_for_running_target(target_w))
			break;
		Sleep(1000);
	}

	if (delay != -1) {
		for (int i = delay; i > 0; i--) {
			printf("Shutting down loader in %i...\r", i);
			Sleep(1000);
		}
		printf("\n");
	} else {
		wait_keypress("\nPress enter when finished...\n");
	}
}

static void elevate_privileges()
{
	DWORD size = sizeof(TOKEN_ELEVATION);
	TOKEN_ELEVATION Elevation;
	wchar_t path[MAX_PATH];
	HANDLE token = NULL;
	int rc;

	if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
		return;

	if (!GetTokenInformation(token, TokenElevation, &Elevation, sizeof(Elevation), &size)) {
		CloseHandle(token);
		return;
	}

	CloseHandle(token);

	if (Elevation.TokenIsElevated)
		return;

	if (!GetModuleFileName(NULL, path, MAX_PATH))
		return;

	CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	rc = (int)(uintptr_t)ShellExecute(NULL, L"runas", path, NULL, NULL, SW_SHOWNORMAL);
	if (rc > 32) // Success
		exit(0);
	if (rc == SE_ERR_ACCESSDENIED)
		wait_exit(EXIT_FAILURE, "Unable to run as admin: Access Denied\n");
	printf("Unable to run as admin: %d\n", rc);
	wait_exit(EXIT_FAILURE);
}

int main()
{
	char *buf, target[MAX_PATH], setting[MAX_PATH], module_path[MAX_PATH];
	DWORD filesize, readsize;
	const char *ini_section;
	wchar_t path[MAX_PATH];
	int rc = EXIT_FAILURE;
	HANDLE ini_file;
	HMODULE module;
	int hook_proc;
	FARPROC fn;
	HHOOK hook;

	CreateMutexA(0, FALSE, "Local\\3DMigotoInjector");
	if (GetLastError() == ERROR_ALREADY_EXISTS)
		wait_exit(EXIT_FAILURE, "ERROR: Another instance of the 3DMigoto Loader is already running. Please close it and try again\n");

	printf("\n------------------------------- 3DMigoto Loader ------------------------------\n\n");

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
	if (!find_ini_setting_lite(ini_section, "target", target, MAX_PATH))
		exit_usage("d3dx.ini [Injector] section missing required \"target\" setting\n");

	if (!find_ini_setting_lite(ini_section, "module", module_path, MAX_PATH))
		exit_usage("d3dx.ini [Injector] section missing required \"module\" setting\n");

	// We've had support for this injection method in 3DMigoto since 1.3.5,
	// however until 1.3.15 it lacked the check in DllMain to bail out of
	// unwanted processes, so that is the first version we consider safe to
	// use for injection and by default we will not allow older DLLs.
	// Disabling this version check can allow the injector to work with
	// third party DLLs that support the same injection method, such as
	// Helix Mod.
	if (find_ini_bool_lite(ini_section, "check_version", true))
		check_3dmigoto_version(module_path, ini_section);

	if (find_ini_bool_lite(ini_section, "require_admin", false))
		elevate_privileges();

	module = LoadLibraryA(module_path);
	if (!module) {
		printf("Unable to load 3DMigoto \"%s\"\n", module_path);
		wait_exit(EXIT_FAILURE);
	}

	GetModuleFileName(module, path, MAX_PATH);
	printf("Loaded %S\n\n", path);

	if (find_ini_setting_lite(ini_section, "entry_point", setting, MAX_PATH))
		fn = GetProcAddress(module, setting);
	else
		fn = GetProcAddress(module, "CBTProc");
	if (!fn) {
		wait_exit(EXIT_FAILURE, "Module does not support injection method\n"
			"Make sure this is a recent 3DMigoto d3d11.dll\n");
	}

	hook_proc = find_ini_int_lite(ini_section, "hook_proc", WH_CBT);
	hook = SetWindowsHookEx(hook_proc, (HOOKPROC)fn, module, 0);
	if (!hook)
		wait_exit(EXIT_FAILURE, "Error installing hook\n");

	rc = EXIT_SUCCESS;

	if (find_ini_setting_lite(ini_section, "launch", setting, MAX_PATH)) {
		printf("3DMigoto ready, launching \"%s\"...\n", setting);
		CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
		ShellExecuteA(NULL, NULL, setting, NULL, NULL, SW_SHOWNORMAL);
	} else {
		printf("3DMigoto ready - Now run the game.\n");
	}

	wait_for_target(target,
			find_ini_bool_lite(ini_section, "wait_for_target", true),
			find_ini_int_lite(ini_section, "delay", 60));

	UnhookWindowsHookEx(hook);
	delete [] buf;

	return rc;
}

