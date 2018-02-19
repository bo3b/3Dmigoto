#include <util.h>

#include <sddl.h>
#include <io.h>
#include <fcntl.h>

#include "DirectX11\HackerDevice.h"
#include "DirectX11\HackerContext.h"


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

	if (__uuidof(ID3D10Multithread) == id)
		return "ID3D10Multithread";

	if (__uuidof(ID3D11DeviceChild) == id)
		return "ID3D11DeviceChild";
	if (__uuidof(ID3DDeviceContextState) == id)
		return "ID3DDeviceContextState";

	if (__uuidof(IDirect3DDevice9) == id)
		return "IDirect3DDevice9";
	if (__uuidof(ID3D10Device) == id)
		return "ID3D10Device";
	if (__uuidof(ID3D11Device) == id)
		return "ID3D11Device";
	if (__uuidof(ID3D11Device1) == id)
		return "ID3D11Device1";
	if (__uuidof(ID3D11Device2) == id)  // d3d11_2.h when the time comes
		return "ID3D11Device2";
	if (IID_HackerDevice == id)
		return "HackerDevice";

	if (__uuidof(ID3D11DeviceContext) == id)
		return "ID3D11DeviceContext";
	if (__uuidof(ID3D11DeviceContext1) == id)
		return "ID3D11DeviceContext1";
	if (__uuidof(ID3D11DeviceContext2) == id) // d3d11_2.h when the time comes
		return "ID3D11DeviceContext2";
	if (IID_HackerContext == id)
		return "HackerContext";

	if (__uuidof(ID3D11InfoQueue) == id)
		return "ID3D11InfoQueue";
	if (__uuidof(ID3DUserDefinedAnnotation) == id)
		return "ID3DUserDefinedAnnotation";

	if (__uuidof(ID3D11BlendState) == id)
		return "ID3D11BlendState";
	if (__uuidof(ID3D11BlendState1) == id)
		return "ID3D11BlendState1";
	if (__uuidof(ID3D11RasterizerState) == id)
		return "ID3D11RasterizerState";
	if (__uuidof(ID3D11RasterizerState1) == id)
		return "ID3D11RasterizerState1";

	if (__uuidof(ID3D11Texture2D) == id)	// Used to fetch backbuffer
		return "ID3D11Texture2D";

	// All the DXGI interfaces from dxgi.h, and dxgi1_2.h

	if (__uuidof(IDXGIObject) == id)
		return "IDXGIObject";
	if (__uuidof(IDXGIDeviceSubObject) == id)
		return "IDXGIDeviceSubObject";

	if (__uuidof(IDXGIFactory) == id)
		return "IDXGIFactory";
	if (__uuidof(IDXGIFactory1) == id)
		return "IDXGIFactory1";
	if (__uuidof(IDXGIFactory2) == id)
		return "IDXGIFactory2";

	if (__uuidof(IDXGIDevice) == id)
		return "IDXGIDevice";
	if (__uuidof(IDXGIDevice1) == id)
		return "IDXGIDevice1";
	if (__uuidof(IDXGIDevice2) == id)
		return "IDXGIDevice2";

	if (__uuidof(IDXGISwapChain) == id)
		return "IDXGISwapChain";
	if (__uuidof(IDXGISwapChain1) == id)
		return "IDXGISwapChain1";
	if (__uuidof(IDXGISwapChain2) == id)		// dxgi1_3 A8BE2AC4-199F-4946-B331-79599FB98DE7
		return "IDXGISwapChain2";
	if (__uuidof(IDXGISwapChain3) == id)		// dxgi1_4 94D99BDB-F1F8-4AB0-B236-7DA0170EDAB1
		return "IDXGISwapChain3";

	if (__uuidof(IDXGIAdapter) == id)
		return "IDXGIAdapter";
	if (__uuidof(IDXGIAdapter1) == id)
		return "IDXGIAdapter1";
	if (__uuidof(IDXGIAdapter2) == id)
		return "IDXGIAdapter2";

	if (__uuidof(IDXGIOutputDuplication) == id)
		return "IDXGIOutputDuplication";
	if (__uuidof(IDXGIDisplayControl) == id)
		return "IDXGIDisplayControl";

	if (__uuidof(IDXGIOutput) == id)
		return "IDXGIOutput";
	if (__uuidof(IDXGIOutput1) == id)
		return "IDXGIOutput1";
	if (__uuidof(IDXGIResource) == id)
		return "IDXGIResource";
	if (__uuidof(IDXGIResource1) == id)
		return "IDXGIResource1";
	if (__uuidof(IDXGISurface) == id)
		return "IDXGISurface";
	if (__uuidof(IDXGISurface1) == id)
		return "IDXGIResource";
	if (__uuidof(IDXGISurface2) == id)
		return "IDXGISurface2";
	if (__uuidof(IDXGIKeyedMutex) == id)
		return "IDXGIKeyedMutex";

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

