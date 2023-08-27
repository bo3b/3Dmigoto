#include <util.h>

#include <sddl.h>
#include <io.h>
#include <fcntl.h>
#include <Dbghelp.h>
#include <shellscalingapi.h>

// FIXME: Move any dependencies from these headers into common:
#if MIGOTO_DX == 9
#include "DirectX9\Overlay.h"
#elif MIGOTO_DX == 11
#include "DirectX11\HackerDevice.h"
#include "DirectX11\HackerContext.h"
#endif // MIGOTO_DX

// Sometimes game directories get funny permissions that cause us problems. I
// have no clue how or why this happens, and the usual way to deal with it is
// to recursively reset the permissions and ownership on the game directory
// with something like:
//
//     takeown /F <path> /R
//     icacls <path> /T /Q /C /RESET
//
// But, I'd like to see if we can do better and handle this from 3DMigoto to
// ensure that we always have access to the files and directories we create if
// at all possible. I don't fully understand windows filesystem permissions,
// but then I doubt many people really and truly do - the ACL complexity is
// where this problem stems from after all (I would say give me UNIX
// permissions over this any day, but then some masochist went and created
// SELinux so now we have a similar headache over there who's only saving grace
// is that we can turn it off), so this is partially (and possibly naively)
// based on this MSDN article:
//
//   https://msdn.microsoft.com/en-us/library/windows/desktop/ms717798(v=vs.85).aspx
//

static SECURITY_ATTRIBUTES* init_security_attributes(SECURITY_ATTRIBUTES *sa)
{
	sa->nLength = sizeof(SECURITY_ATTRIBUTES);
	sa->bInheritHandle = FALSE;
	sa->lpSecurityDescriptor = NULL;

	if (ConvertStringSecurityDescriptorToSecurityDescriptor(
			L"D:" // Discretionary ACL
			// Removed string from MSDN that denies guests/anonymous users
			L"(A;OICI;GRGX;;;WD)" // Give everyone read/execute access
			L"(A;OICI;GA;;;AU)" // Allow full control to authenticated users (GRGWGX is not enough to delete contents?)
			// Using "CO" for Creator/Owner instead of "AU" seems ineffective
			L"(A;OICI;GA;;;BA)" // Allow full control to administrators
			, SDDL_REVISION_1, &sa->lpSecurityDescriptor, NULL)) {
		return sa;
	}

	LogInfo("ConvertStringSecurityDescriptorToSecurityDescriptor failed\n");
	return NULL;
}

BOOL CreateDirectoryEnsuringAccess(LPCWSTR path)
{
	SECURITY_ATTRIBUTES sa, *psa = NULL;
	BOOL ret = false;

	psa = init_security_attributes(&sa);

	ret = CreateDirectory(path, psa);

	LocalFree(sa.lpSecurityDescriptor);

	return ret;
}

// Replacement for _wfopen_s that ensures the permissions will be set so we can
// read it back later.
errno_t wfopen_ensuring_access(FILE** pFile, const wchar_t *filename, const wchar_t *mode)
{
	SECURITY_ATTRIBUTES sa, *psa = NULL;
	HANDLE fh = NULL;
	int fd = -1;
	FILE *fp = NULL;
	int osf_flags = 0;

	*pFile = NULL;

	if (wcsstr(mode, L"w") == NULL) {
		// This function is for creating new files for now. We could
		// make it do some heroics on read/append as well, but I don't
		// want to push this further than we need to.
		LogInfo("FIXME: wfopen_ensuring_access only supports opening for write\n");
		DoubleBeepExit();
	}

	if (wcsstr(mode, L"b") == NULL)
		osf_flags |= _O_TEXT;

	// We use _wfopen_s so that we can use formatted print routines, but to
	// set security attributes at creation time to make sure the
	// permissions give us read access we need to use CreateFile, and
	// convert the resulting handle into a C file descriptor, then a FILE*
	// that can be used as per usual.
	psa = init_security_attributes(&sa);
	fh = CreateFile(filename, GENERIC_WRITE, 0, psa, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	LocalFree(sa.lpSecurityDescriptor);
	if (fh == INVALID_HANDLE_VALUE) {
		// FIXME: Map GetLastError() to appropriate errno
		return EIO;
	}

	// Convert the HANDLE into a file descriptor.
	fd = _open_osfhandle((intptr_t)fh, osf_flags);
	if (fd == -1) {
		CloseHandle(fh);
		return EIO;
	}

	// From this point on, we do not use CloseHandle(fh), as it will be
	// implicitly closed with close(fd)

	// Convert the file descriptor into a file pointer.
	fp = _wfdopen(fd, mode);
	if (!fp) {
		_close(fd);
		return EIO;
	}

	// From this point on, we do not use CloseHandle(fh) or close(fd) as it
	// will be implicitly closed with fclose(fp). Convenient for us,
	// because it means the caller doesn't have to care about the fh or fd.

	*pFile = fp;
	return 0;
}

void set_file_last_write_time(wchar_t *path, FILETIME *ftWrite, DWORD flags)
{
	HANDLE f;

	f = CreateFile(path, GENERIC_WRITE, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | flags, NULL);
	if (f == INVALID_HANDLE_VALUE)
		return;

	SetFileTime(f, NULL, NULL, ftWrite);
	CloseHandle(f);
}

void touch_file(wchar_t *path, DWORD flags)
{
	FILETIME ft;
	SYSTEMTIME st;

	GetSystemTime(&st);
	SystemTimeToFileTime(&st, &ft);
	set_file_last_write_time(path, &ft, flags);
}


// -----------------------------------------------------------------------------------------------
// When logging, it's not very helpful to have long sequences of hex instead of
// the actual names of the objects in question.
// e.g.
// DEFINE_GUID(IID_IDXGIFactory,0x7b7166ec,0x21c7,0x44ae,0xb2,0x1a,0xc9,0xae,0x32,0x1a,0xe3,0x69);
//

std::string NameFromIID(IID id)
{
	// Adding every MIDL_INTERFACE from d3d11_1.h to make this reporting complete.
	// Doesn't seem useful to do every object from d3d11.h itself.

	if (__uuidof(IUnknown) == id)
		return "IUnknown";

	// FIXME: We should probably have these IIDs defined regardless of target
	// to catch potential cases where multiple versions of 3DMigoto are
	// coexisting and the devices get mixed up
#if MIGOTO_DX == 11
	if (IID_HackerDevice == id)
		return "HackerDevice";
	if (IID_HackerContext == id)
		return "HackerContext";
#elif MIGOTO_DX == 9
	// FIXME: DX9 GUIDs are not using the correct macros, and need verification
	// that they haven't been copy + pasted
	//if (IID_D3D9Wrapper_IDirect3DDevice9 == id)
	//	return "3DMigotoDevice9";
#endif

#ifdef _D3D9_H_
	if (__uuidof(IDirect3DDevice9) == id)
		return "IDirect3DDevice9";
#endif // _D3D9_H_

#ifdef __d3d10_h__
	if (__uuidof(ID3D10Multithread) == id)
		return "ID3D10Multithread";
	if (__uuidof(ID3D10Device) == id)
		return "ID3D10Device";
#endif // __d3d10_h__

#ifdef __d3d11_h__
	if (__uuidof(ID3D11Device) == id)
		return "ID3D11Device";
	if (__uuidof(ID3D11DeviceContext) == id)
		return "ID3D11DeviceContext";
	if (__uuidof(ID3D11DeviceChild) == id)
		return "ID3D11DeviceChild";
	if (__uuidof(ID3D11BlendState) == id)
		return "ID3D11BlendState";
	if (__uuidof(ID3D11RasterizerState) == id)
		return "ID3D11RasterizerState";
	if (__uuidof(ID3D11Texture2D) == id)	// Used to fetch backbuffer
		return "ID3D11Texture2D";
#endif // __d3d11_h__

#ifdef __d3d11_1_h__
	if (__uuidof(ID3D11BlendState1) == id)
		return "ID3D11BlendState1";
	if (__uuidof(ID3D11Device1) == id)
		return "ID3D11Device1";
	if (__uuidof(ID3D11DeviceContext1) == id)
		return "ID3D11DeviceContext1";
	if (__uuidof(ID3D11RasterizerState1) == id)
		return "ID3D11RasterizerState1";
	if (__uuidof(ID3DDeviceContextState) == id)
		return "ID3DDeviceContextState";
	if (__uuidof(ID3DUserDefinedAnnotation) == id)
		return "ID3DUserDefinedAnnotation";
#endif // __d3d11_1_h__

	// XXX: From newer Windows SDK than we are using. Defined in util.h for now
	if (__uuidof(ID3D11Device2) == id)  // d3d11_2.h when the time comes
		return "ID3D11Device2";
	if (__uuidof(ID3D11DeviceContext2) == id) // d3d11_2.h when the time comes
		return "ID3D11DeviceContext2";

#ifdef __d3d11sdklayers_h__
	if (__uuidof(ID3D11InfoQueue) == id)
		return "ID3D11InfoQueue";
#endif

	// All the DXGI interfaces from dxgi.h, and dxgi1_2.h
#ifdef __dxgi_h__
	if (__uuidof(IDXGIAdapter) == id)
		return "IDXGIAdapter";
	if (__uuidof(IDXGIAdapter1) == id)
		return "IDXGIAdapter1";
	if (__uuidof(IDXGIDevice) == id)
		return "IDXGIDevice";
	if (__uuidof(IDXGIDevice1) == id)
		return "IDXGIDevice1";
	if (__uuidof(IDXGIDeviceSubObject) == id)
		return "IDXGIDeviceSubObject";
	if (__uuidof(IDXGIFactory) == id)
		return "IDXGIFactory";
	if (__uuidof(IDXGIFactory1) == id)
		return "IDXGIFactory1";
	if (__uuidof(IDXGIKeyedMutex) == id)
		return "IDXGIKeyedMutex";
	if (__uuidof(IDXGIObject) == id)
		return "IDXGIObject";
	if (__uuidof(IDXGIOutput) == id)
		return "IDXGIOutput";
	if (__uuidof(IDXGIResource) == id)
		return "IDXGIResource";
	if (__uuidof(IDXGISurface) == id)
		return "IDXGISurface";
	if (__uuidof(IDXGISurface1) == id)
		return "IDXGISurface1";
	if (__uuidof(IDXGISwapChain) == id)
		return "IDXGISwapChain";
#endif // __dxgi_h__

#ifdef __dxgi1_2_h__
	if (__uuidof(IDXGIAdapter2) == id)
		return "IDXGIAdapter2";
	if (__uuidof(IDXGIDevice2) == id)
		return "IDXGIDevice2";
	if (__uuidof(IDXGIDisplayControl) == id)
		return "IDXGIDisplayControl";
	if (__uuidof(IDXGIFactory2) == id)
		return "IDXGIFactory2";
	if (__uuidof(IDXGIOutput1) == id)
		return "IDXGIOutput1";
	if (__uuidof(IDXGIOutputDuplication) == id)
		return "IDXGIOutputDuplication";
	if (__uuidof(IDXGIResource1) == id)
		return "IDXGIResource1";
	if (__uuidof(IDXGISurface2) == id)
		return "IDXGISurface2";
	if (__uuidof(IDXGISwapChain1) == id)
		return "IDXGISwapChain1";
#endif // __dxgi1_2_h__

	// XXX: From newer Windows SDK than we are using. Defined in util.h for now
	if (__uuidof(IDXGISwapChain2) == id)		// dxgi1_3 A8BE2AC4-199F-4946-B331-79599FB98DE7
		return "IDXGISwapChain2";
	if (__uuidof(IDXGISwapChain3) == id)		// dxgi1_4 94D99BDB-F1F8-4AB0-B236-7DA0170EDAB1
		return "IDXGISwapChain3";
	if (__uuidof(IDXGISwapChain4) == id)		// dxgi1_5 3D585D5A-BD4A-489E-B1F4-3DBCB6452FFB
		return "IDXGISwapChain4";

	// For unknown IIDs lets return the hex string.
	// Converting from wchar_t to string using stackoverflow suggestion.

	std::string iidString;
	wchar_t wiid[128];
	if (SUCCEEDED(StringFromGUID2(id, wiid, 128)))
	{
		std::wstring convert = std::wstring(wiid);
		iidString = std::string(convert.begin(), convert.end());
	}
	else
	{
		iidString = "unknown";
	}

	return iidString;
}

static void WarnIfConflictingFileExists(wchar_t *path, wchar_t *conflicting_path, const char *message)
{
	DWORD attrib = GetFileAttributes(conflicting_path);

	if (attrib == INVALID_FILE_ATTRIBUTES)
		return;

	LogOverlay(LOG_DIRE, "WARNING: %s\"%S\" conflicts with \"%S\"\n", message, conflicting_path, path);
}

void WarnIfConflictingShaderExists(wchar_t *orig_path, const char *message)
{
	wchar_t conflicting_path[MAX_PATH], *postfix;

	wcscpy_s(conflicting_path, MAX_PATH, orig_path);

	// If we're using a HLSL shader, make sure there are no conflicting
	// assembly shaders, either text or binary:
	postfix = wcsstr(conflicting_path, L"_replace");
	if (postfix) {
		wcscpy_s(postfix, conflicting_path + MAX_PATH - postfix, L".txt");
		WarnIfConflictingFileExists(orig_path, conflicting_path, message);
		wcscpy_s(postfix, conflicting_path + MAX_PATH - postfix, L".bin");
		WarnIfConflictingFileExists(orig_path, conflicting_path, message);
		return;
	}

	// If we're using an assembly shader, make sure there are no
	// conflicting HLSL shaders, either text or binary:
	postfix = wcsstr(conflicting_path, L".");
	if (postfix) {
		wcscpy_s(postfix, conflicting_path + MAX_PATH - postfix, L"_replace.txt");
		WarnIfConflictingFileExists(orig_path, conflicting_path, message);
		wcscpy_s(postfix, conflicting_path + MAX_PATH - postfix, L"_replace.bin");
		WarnIfConflictingFileExists(orig_path, conflicting_path, message);
		return;
	}
}

extern "C" typedef HRESULT(__stdcall* tGetDpiForMonitor)(HMONITOR, MONITOR_DPI_TYPE, UINT*, UINT*);
extern "C" typedef HRESULT(__stdcall* tGetProcessDpiAwareness)(HANDLE, PROCESS_DPI_AWARENESS*);
typedef DPI_AWARENESS_CONTEXT(WINAPI* tSetThreadDpiAwarenessContext)(DPI_AWARENESS_CONTEXT);
float get_effective_dpi()
{
	static tGetDpiForMonitor fnGetDpiForMonitor = nullptr;
	static tGetProcessDpiAwareness fnGetProcessDpiAwareness = nullptr;
	static tSetThreadDpiAwarenessContext fnSetThreadDpiAwarenessContext = nullptr;
	static bool init_done = false;
	if (!init_done) {
		// GetDpiForMonitor & GetProcessDpiAwareness were introduced in Windows
		// 8.1 and SetThreadDpiAwarenessContext was added in Win 10 1607, so
		// calling them directly would add a link time dependency breaking
		// backwards compatibility with Windows 7 (in a very bad way since the
		// game will silently fail to launch, and even if the user does run it
		// from the command line the error message doesn't really tell them
		// what's actually wrong). Try to load it dynamically, and if it isn't
		// available fall back to other methods to get the DPI.
		HMODULE libShcore = LoadLibraryA("Shcore.dll");
		if (libShcore) {
			fnGetDpiForMonitor = (tGetDpiForMonitor)GetProcAddress(libShcore, "GetDpiForMonitor");
			fnGetProcessDpiAwareness = (tGetProcessDpiAwareness)GetProcAddress(libShcore, "GetProcessDpiAwareness");
		}
		HMODULE libUser32 = LoadLibraryA("user32.dll");
		if (libUser32)
			fnSetThreadDpiAwarenessContext = (tSetThreadDpiAwarenessContext)GetProcAddress(libUser32, "SetThreadDpiAwarenessContext");
		if (!fnGetDpiForMonitor)
			LogInfo("Obsolete Windows version, GetDpiForMonitor() unavailable, falling back to reporting effective_dpi as 96\n");
		else if (!fnSetThreadDpiAwarenessContext)
			LogInfo("Obsolete Windows version, SetThreadDpiAwarenessContext() unavailable, effective_dpi will be at the mercy of the process DPI awareness\n");
		if (fnGetProcessDpiAwareness) {
			PROCESS_DPI_AWARENESS awareness;
			fnGetProcessDpiAwareness(NULL, &awareness);
			LogInfo("Process DPI Awareness: %u\n", awareness);
		}
		init_done = true;
	}

	if (fnGetDpiForMonitor) {
		HMONITOR mon;
		if (G->hWnd)
			mon = MonitorFromWindow(G->hWnd, MONITOR_DEFAULTTONEAREST);
		else
			mon = MonitorFromPoint(POINT{ 0,0 }, MONITOR_DEFAULTTOPRIMARY);
		UINT x = 0, y = 0;
		// XXX: NOTE GetDpiForMonitor() may return 96 if the process is
		// PROCESS_DPI_UNAWARE, and PROCESS_SYSTEM_DPI_AWARE is also
		// sub-optimal. On Windows 10 1607+ we can temporarily switch the
		// thread to multi-monitor DPI aware that should* then return the
		// correct effective DPI for the monitor that (most of) the window is
		// on. Prior to that version we are at the mercy of the process DPI
		// awareness, and while there is an API that can set it
		// programmatically since 8.1, it can only be called once in the
		// lifetime of the process and it might be a bad idea to perturb that.
		//
		// * though the doco on this is all over the place and I'm really
		// unsure if this will work in all cases. e.g. it suggests that it
		// must be set prior to window creation as it gets baked into the
		// thread at that time... but that seems false, at least for
		// GetDpiForMonitor... Also it says GetDpiForMonitor (Win8.1) is DPI
		// unaware and we should use GetDpiForWindow (Win10) before detailing
		// what it returns for different DPI awareness settings, seemingly
		// contradicting itself... I do get the impression that the behaviour
		// may have changed several times throughout history, so maybe this is
		// true in older versions of Windows?
		//
		// Point is, this could do with wider testing in games with different
		// DPI awareness at various resolutions and on several versions of
		// Windows to see how it behaves in practice and if there are any
		// particularly bad cases.
		DPI_AWARENESS_CONTEXT old = DPI_AWARENESS_CONTEXT_UNAWARE;
		if (fnSetThreadDpiAwarenessContext)
			old = fnSetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE);
		fnGetDpiForMonitor(mon, MDT_EFFECTIVE_DPI, &x, &y);
		if (fnSetThreadDpiAwarenessContext)
			fnSetThreadDpiAwarenessContext(old);
		return (float)x;
	}

	// Fallback for Win 7: Just return 96, which is the effective DPI Windows
	// reports at 100% scaling. We could potentially try to return the
	// recommended scaling factors for different resolutions (e.g. 120 for
	// 1080p at 125% recommended scaling, 240 for 4k at 250% recommended
	// scaling), but it's probably not worth going to such heroics on an OS
	// that has outdated notions of DPI scaling where no one should really be
	// using a 4K display anyway. We definitely should not be naive and return
	// the real / physical / raw DPI here, as that is not the same as effective
	// DPI and generally unsuitable for UI scaling.
	return 96.0f;
}

#if MIGOTO_DX == 9
void save_om_state(IDirect3DDevice9 *device, struct OMState *state)
{
	DWORD i;

	// OMGetRenderTargetAndUnorderedAccessViews is a poorly designed API as
	// to use it properly to get all RTVs and UAVs we need to pass it some
	// information that we don't know. So, we have to do a few extra steps
	// to find that info.
	D3DCAPS9 caps;
	device->GetDeviceCaps(&caps);
	if (state->rtvs.size() != caps.NumSimultaneousRTs)
		state->rtvs.resize(caps.NumSimultaneousRTs);
	state->NumRTVs = 0;
	for (i = 0; i < caps.NumSimultaneousRTs; i++) {
		IDirect3DSurface9 *rt = NULL;
		device->GetRenderTarget(i, &rt);
		state->rtvs[i] = rt;
		if (rt) {
			state->NumRTVs = i + 1;
		}
	}
	device->GetDepthStencilSurface(&state->dsv);
}

void restore_om_state(IDirect3DDevice9 *device, struct OMState *state)
{
	UINT i;
	for (i = 0; i < state->NumRTVs; i++) {
		device->SetRenderTarget(i, state->rtvs[i]);
		if (state->rtvs[i])
			state->rtvs[i]->Release();
	}
	device->SetDepthStencilSurface(state->dsv);
	if (state->dsv)
		state->dsv->Release();
}
#elif MIGOTO_DX == 11
void save_om_state(ID3D11DeviceContext *context, struct OMState *state)
{
	int i;

	// OMGetRenderTargetAndUnorderedAccessViews is a poorly designed API as
	// to use it properly to get all RTVs and UAVs we need to pass it some
	// information that we don't know. So, we have to do a few extra steps
	// to find that info.

	context->OMGetRenderTargets(D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT, state->rtvs, &state->dsv);

	state->NumRTVs = 0;
	for (i = 0; i < D3D11_SIMULTANEOUS_RENDER_TARGET_COUNT; i++) {
		if (state->rtvs[i])
			state->NumRTVs = i + 1;
	}

	state->UAVStartSlot = state->NumRTVs;
	// Set NumUAVs to the max to retrieve them all now, and so that later
	// when rebinding them we will unbind any others that the command list
	// bound in the meantime
	state->NumUAVs = D3D11_PS_CS_UAV_REGISTER_COUNT - state->UAVStartSlot;

	// Finally get all the UAVs. Since we already retrieved the RTVs and
	// DSV we can skip getting them:
	context->OMGetRenderTargetsAndUnorderedAccessViews(0, NULL, NULL, state->UAVStartSlot, state->NumUAVs, state->uavs);
}

void restore_om_state(ID3D11DeviceContext *context, struct OMState *state)
{
	static const UINT uav_counts[D3D11_PS_CS_UAV_REGISTER_COUNT] = {(UINT)-1, (UINT)-1, (UINT)-1, (UINT)-1, (UINT)-1, (UINT)-1, (UINT)-1, (UINT)-1};
	UINT i;

	context->OMSetRenderTargetsAndUnorderedAccessViews(state->NumRTVs, state->rtvs, state->dsv,
			state->UAVStartSlot, state->NumUAVs, state->uavs, uav_counts);

	for (i = 0; i < state->NumRTVs; i++) {
		if (state->rtvs[i])
			state->rtvs[i]->Release();
	}

	if (state->dsv)
		state->dsv->Release();

	for (i = 0; i < state->NumUAVs; i++) {
		if (state->uavs[i])
			state->uavs[i]->Release();
	}
}

IDXGISwapChain *last_fullscreen_swap_chain;
static CRITICAL_SECTION crash_handler_lock;
static int crash_handler_level;

static DWORD WINAPI crash_handler_switch_to_window(_In_ LPVOID lpParameter)
{
	// Debugging is a pain in exclusive full screen, especially without a
	// second monitor attached (and even with one if you don't know about
	// the win+arrow or alt+space shortcuts you may be stuck - alt+tab to
	// the debugger, either win+down or alt+space and choose "Restore",
	// either win+left/right several times to move it to the other monitor
	// or alt+space again and choose "Move", press any arrow key to start
	// moving and *then* you can use the mouse to move the window to the
	// other monitor)... Try to switch to windowed mode to make our lives a
	// lot easier, but depending on the crash this might just hang (DirectX
	// might get stuck waiting on a lock or the window message queue might
	// not be pumping), so we do this in a new thread to allow the main
	// crash handler to continue responding to other keys:
	//
	// TODO: See if we can find a way to make this more reliable
	//
	if (last_fullscreen_swap_chain) {
		LogInfo("Attempting emergency switch to windowed mode on swap chain %p\n",
				last_fullscreen_swap_chain);

		last_fullscreen_swap_chain->SetFullscreenState(FALSE, NULL);
		//last_fullscreen_swap_chain->ResizeBuffers(0, 0, 0, DXGI_FORMAT_UNKNOWN, DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH);
	}

	if (LogFile)
		fflush(LogFile);

	return 0;
}

static LONG WINAPI migoto_exception_filter(_In_ struct _EXCEPTION_POINTERS *ExceptionInfo)
{
	wchar_t path[MAX_PATH];
	tm timestruct;
	time_t ltime;
	LONG ret = EXCEPTION_CONTINUE_EXECUTION;

	// SOS
	Beep(250, 100); Beep(250, 100); Beep(250, 100);
	Beep(200, 300); Beep(200, 200); Beep(200, 200);
	Beep(250, 100); Beep(250, 100); Beep(250, 100);

	// Before anything else, flush the log file and log exception info

	if (LogFile) {
		fflush(LogFile);

		LogInfo("\n\n ######################################\n"
		            " ### 3DMigoto Crash Handler Invoked ###\n");

		int i = 0;
		for (auto record = ExceptionInfo->ExceptionRecord; record; record = record->ExceptionRecord, i++) {
			LogInfo(" ######################################\n"
			        " ### Exception Record %i\n"
				" ###    ExceptionCode: 0x%08x\n"
				" ###   ExceptionFlags: 0x%08x\n"
				" ### ExceptionAddress: 0x%p\n"
				" ### NumberParameters: 0x%u\n"
				" ###",
				i,
				record->ExceptionCode,
				record->ExceptionFlags,
				record->ExceptionAddress,
				record->NumberParameters);
			for (unsigned j = 0; j < record->NumberParameters; j++)
				LogInfo(" %08Ix", record->ExceptionInformation[j]);
			LogInfo("\n");
		}

		fflush(LogFile);
	}

	// Next, write a minidump file so we can examine this in a debugger
	// later. Note that if the stack is corrupt there is some possibility
	// this could fail - if we really want a robust crash handler we could
	// bring in something like breakpad

	ltime = time(NULL);
	localtime_s(&timestruct, &ltime);
	wcsftime(path, MAX_PATH, L"3DM-%Y%m%d%H%M%S.dmp", &timestruct);

	// If multiple threads crash only allow one to write the crash dump and
	// the rest stop here:
	EnterCriticalSectionPretty(&crash_handler_lock);

	auto fp = CreateFile(path, GENERIC_WRITE, FILE_SHARE_READ,
			0, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, NULL);
	if (fp != INVALID_HANDLE_VALUE) {
		LogInfo("Writing minidump to %S...\n", path);

		MINIDUMP_EXCEPTION_INFORMATION dump_info =
			{ GetCurrentThreadId(), ExceptionInfo, FALSE };

		if (MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(),
				fp, MiniDumpWithHandleData, &dump_info, NULL, NULL))
			LogInfo("Succeeded\n");
		else
			LogInfo("Failed :(\n");

		CloseHandle(fp);
	} else
		LogInfo("Error creating minidump file \"%S\": %d\n", path, GetLastError());

	if (LogFile)
		fflush(LogFile);

	// If crash is set to 2 instead of continuing we will stop and start
	// responding to various key bindings, sounding a reminder tone every
	// 5 seconds. All key bindings in this mode are prefixed with Ctrl+Alt
	// to prevent them being accidentally triggered.
	if (crash_handler_level == 2) {
		if (LogFile) {
			LogInfo("3DMigoto interactive crash handler invoked:\n");
			LogInfo(" Ctrl+Alt+Q: Quit (execute exception handler)\n");
			LogInfo(" Ctrl+Alt+K: Kill process\n");
			LogInfo(" Ctrl+Alt+C: Continue execution\n");
			LogInfo(" Ctrl+Alt+B: Break into the debugger (make sure one is attached)\n");
			LogInfo(" Ctrl+Alt+W: Attempt to switch to Windowed mode\n");
			LogInfo("\n");
			fflush(LogFile);
		}
		while (1) {
			Beep(500, 100);
			for (int i = 0; i < 50; i++) {
				Sleep(100);
				if (GetAsyncKeyState(VK_CONTROL) < 0 &&
				    GetAsyncKeyState(VK_MENU) < 0) {
					if (GetAsyncKeyState('C') < 0) {
						LogInfo("Attempting to continue...\n"); fflush(LogFile); Beep(1000, 100);
						ret = EXCEPTION_CONTINUE_EXECUTION;
						goto unlock;
					}

					if (GetAsyncKeyState('Q') < 0) {
						LogInfo("Executing exception handler...\n"); fflush(LogFile); Beep(1000, 100);
						ret = EXCEPTION_EXECUTE_HANDLER;
						goto unlock;
					}

					if (GetAsyncKeyState('K') < 0) {
						LogInfo("Killing process...\n"); fflush(LogFile); Beep(1000, 100);
						ExitProcess(0x3D819070);
					}

					// TODO:
					// S = Suspend all other threads
					// R = Resume all other threads

					if (GetAsyncKeyState('B') < 0) {
						LogInfo("Dropping to debugger...\n"); fflush(LogFile); Beep(1000, 100);
						__debugbreak();
						goto unlock;
					}

					if (GetAsyncKeyState('W') < 0) {
						LogInfo("Attempting to switch to windowed mode...\n"); fflush(LogFile); Beep(1000, 100);
						CreateThread(NULL, 0, crash_handler_switch_to_window, NULL, 0, NULL);
						Sleep(1000);
					}
				}
			}
		}
	}

unlock:
	LeaveCriticalSection(&crash_handler_lock);

	return ret;
}

static DWORD WINAPI exception_keyboard_monitor(_In_ LPVOID lpParameter)
{
	while (1) {
		Sleep(1000);
		if (GetAsyncKeyState(VK_CONTROL) < 0 &&
		    GetAsyncKeyState(VK_MENU) < 0 &&
		    GetAsyncKeyState(VK_F11) < 0) {
			// User must be really committed to this to invoke the
			// crash handler, and this is a simple measure against
			// accidentally invoking it multiple times in a row:
			Sleep(3000);
			if (GetAsyncKeyState(VK_CONTROL) < 0 &&
			    GetAsyncKeyState(VK_MENU) < 0 &&
			    GetAsyncKeyState(VK_F11) < 0) {
				// Make sure 3DMigoto's exception handler is
				// still installed and trigger it:
				SetUnhandledExceptionFilter(migoto_exception_filter);
				RaiseException(0x3D819070, 0, 0, NULL);
			}
		}
	}

}

void install_crash_handler(int level)
{
	LPTOP_LEVEL_EXCEPTION_FILTER old_handler;
	UINT old_mode;

	crash_handler_level = level;

	old_handler = SetUnhandledExceptionFilter(migoto_exception_filter);
	// TODO: Call set_terminate() on every thread to catch unhandled C++
	// exceptions as well

	if (old_handler == migoto_exception_filter) {
		LogInfo("  > 3DMigoto crash handler already installed\n");
		return;
	}

	InitializeCriticalSectionPretty(&crash_handler_lock);

	old_mode = SetErrorMode(SEM_FAILCRITICALERRORS);

	LogInfo("  > Installed 3DMigoto crash handler, previous exception filter: %p, previous error mode: %x\n",
			old_handler, old_mode);

	// Spawn a thread to monitor for a keyboard salute to trigger the
	// exception handler in the event of a hang/deadlock:
	CreateThread(NULL, 0, exception_keyboard_monitor, NULL, 0, NULL);
}
#endif
