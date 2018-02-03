#include <util.h>

#include <sddl.h>

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
BOOL CreateDirectoryEnsuringAccess(LPCWSTR path)
{
	SECURITY_ATTRIBUTES sa, *psa = NULL;
	BOOL ret = false;

	sa.nLength = sizeof(SECURITY_ATTRIBUTES);
	sa.bInheritHandle = FALSE;
	sa.lpSecurityDescriptor = NULL;

	if (ConvertStringSecurityDescriptorToSecurityDescriptor(
			L"D:" // Discretionary ACL
			// Removed string from MSDN that denies guests/anonymous users
			L"(A;OICI;GRGX;;;WD)" // Give everyone read/execute access
			L"(A;OICI;GRGWGX;;;AU)" // Allow read/write/execute to authenticated users
			L"(A;OICI;GA;;;BA)" // Allow full control to administrators
			, SDDL_REVISION_1, &sa.lpSecurityDescriptor, NULL)) {
		psa = &sa;
	} else {
		LogInfo("ConvertStringSecurityDescriptorToSecurityDescriptor failed\n");
	}

	ret = CreateDirectory(path, psa);

	if (psa)
		LocalFree(sa.lpSecurityDescriptor);

	return ret;
}
