#include <util.h>

#include <sddl.h>
#include <io.h>
#include <fcntl.h>

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
