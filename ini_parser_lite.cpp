#include "util_min.h"

#include <ctype.h>
#include <string.h>
#include <stdlib.h>

// Minimalistic ini file parsing routines that are intended to be safe to use
// from DllMain. Should be fairly fast since they don't update our usual data
// structures and don't do any unecessary backtracking while scanning for the
// queried setting, which will help minimise the time we stall newly spawned
// processes we may have been injected into that are not our intended target.

static const char* skip_space(const char *buf)
{
	for (; *buf == ' ' || *buf == '\t'; buf++) {}
	return buf;
}

// Returns a pointer to the next non-whitespace character on a following line
static const char* next_line(const char *buf)
{
	for (; *buf != '\0' && *buf != '\n' && *buf != '\r'; buf++) {}
	for (; *buf == '\n' || *buf == '\r' || *buf == ' ' || *buf == '\t'; buf++) {}
	return buf;
}

// Returns a pointer to the first non-whitespace character on the line
// following [section_name] (which may be a pointer to the zero terminator if
// EOF is encountered), or NULL if the section is not found. section_name must
// be lower case.
const char* find_ini_section_lite(const char *buf, const char *section_name)
{
	const char *p;

	for (buf = skip_space(buf); *buf; buf = next_line(buf)) {
		if (*buf == '[') {
			for (buf++, p = section_name; *p && (tolower((unsigned char)*buf) == *p); buf++, p++) {}
			if (*buf == ']' && *p == '\0')
				return next_line(buf);
		}
	}

	return 0;
}

// Searches for the setting. If found in the current section, copies the value
// to ret stripping any whitespace from the end of line and returns true. If
// not found or the buffer size is insufficient, returns false.
bool find_ini_setting_lite(const char *buf, const char *setting, char *ret, size_t n)
{
	const char *p;
	char *r;
	size_t i;

	for (buf = skip_space(buf); *buf; buf = next_line(buf)) {
		// Check for end of section
		if (*buf == '[')
			return false;

		// Check if line matches setting
		for (p = setting; *p && tolower((unsigned char)*buf) == *p; buf++, p++) {}
		buf = skip_space(buf);
		if (*buf != '=' || *p != '\0')
			continue;

		// Copy setting until EOL/EOF to ret buffer
		buf = skip_space(buf + 1);
		for (i = 0, r = ret; i < n; i++, buf++, r++) {
			*r = *buf;
			if (*buf == '\n' || *buf == '\r' || *buf == '\0') {
				// Null terminate return buffer and strip any whitespace from EOL:
				for (; r >= ret && (*r == '\0' || *r == '\n' || *r == '\r' || *r == ' ' || *r == '\t'); r--)
					*r = '\0';
				return true;
			}
		}
		// Insufficient room in buffer
		return false;
	}
	return false;
}

bool find_ini_bool_lite(const char *buf, const char *setting, bool def)
{
	char val[8];

	if (!find_ini_setting_lite(buf, setting, val, 8))
		return def;

	if (!_stricmp(val, "1") || !_stricmp(val, "true") || !_stricmp(val, "yes") || !_stricmp(val, "on"))
		return true;

	if (!_stricmp(val, "0") || !_stricmp(val, "false") || !_stricmp(val, "no") || !_stricmp(val, "off"))
		return false;

	return def;
}

int find_ini_int_lite(const char *buf, const char *setting, int def)
{
	char val[16];

	if (!find_ini_setting_lite(buf, setting, val, 16))
		return def;

	return atoi(val);
}
