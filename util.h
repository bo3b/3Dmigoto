#pragma once

#include <ctype.h>
#include <wchar.h>
#include <string.h>

// Strip spaces from the right of a string.
// Returns a pointer to the last non-NULL character of the truncated string.
static char *arstrip(char *buf)
{
	char *end = buf + strlen(buf) - 1;
	while (end > buf && isspace(*end))
		end--;
	*(end + 1) = 0;
	return end;
}
static wchar_t *wrstrip(wchar_t *buf)
{
	wchar_t *end = buf + wcslen(buf) - 1;
	while (end > buf && iswspace(*end))
		end--;
	*(end + 1) = 0;
	return end;
}
