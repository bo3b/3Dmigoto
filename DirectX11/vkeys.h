#pragma once

#include <string>
#include "util.h"

// http://msdn.microsoft.com/en-us/library/windows/desktop/dd375731(v=vs.85).aspx
static EnumName_t<wchar_t *, int> VKMappings[] = {
	{L"LBUTTON", 0x01},
	{L"RBUTTON", 0x02},
	{L"CANCEL", 0x03},
	{L"MBUTTON", 0x04},
	{L"XBUTTON1", 0x05},
	{L"XBUTTON2", 0x06},
	{L"BACK", 0x08}, {L"BACKSPACE", 0x08}, {L"BACK_SPACE", 0x08},
	{L"TAB", 0x09},
	{L"CLEAR", 0x0C},
	{L"RETURN", 0x0D}, {L"ENTER", 0x0D},
	{L"SHIFT", 0x10},
	{L"CONTROL", 0x11}, {L"CTRL", 0x11},
	{L"MENU", 0x12}, {L"ALT", 0x12},
	{L"PAUSE", 0x13},
	{L"CAPITAL", 0x14}, {L"CAPS", 0x14}, {L"CAPSLOCK", 0x14}, {L"CAPS_LOCK", 0x14},
	{L"KANA", 0x15},
	{L"HANGUEL", 0x15},
	{L"HANGUL", 0x15},
	{L"JUNJA", 0x17},
	{L"FINAL", 0x18},
	{L"HANJA", 0x19},
	{L"KANJI", 0x19},
	{L"ESCAPE", 0x1B},
	{L"CONVERT", 0x1C},
	{L"NONCONVERT", 0x1D},
	{L"ACCEPT", 0x1E},
	{L"MODECHANGE", 0x1F},
	{L"SPACE", 0x20},
	{L"PRIOR", 0x21}, {L"PGUP", 0x21}, {L"PAGEUP", 0x21}, {L"PAGE_UP", 0x21},
	{L"NEXT", 0x22}, {L"PGDN", 0x22}, {L"PAGEDOWN", 0x22}, {L"PAGE_DOWN", 0x22},
	{L"END", 0x23},
	{L"HOME", 0x24},
	{L"LEFT", 0x25},
	{L"UP", 0x26},
	{L"RIGHT", 0x27},
	{L"DOWN", 0x28},
	{L"SELECT", 0x29},
	{L"PRINT", 0x2A},
	{L"EXECUTE", 0x2B},
	{L"SNAPSHOT", 0x2C}, {L"PRSCR", 0x2C}, {L"PRINTSCREEN", 0x2C}, {L"PRINT_SCREEN", 0x2C},
	{L"INSERT", 0x2D},
	{L"DELETE", 0x2E},
	{L"HELP", 0x2F},
	/* 0-9 & upper case A-Z match ASCII and are checked programatically */
	{L"LWIN", 0x5B}, {L"LEFT_WIN", 0x5B}, {L"LEFT_WINDOWS", 0x5B},
	{L"RWIN", 0x5C}, {L"RIGHT_WIN", 0x5C},{L"RIGHT_WINDOWS", 0x5C},
	{L"APPS", 0x5D},
	{L"SLEEP", 0x5F},
	{L"NUMPAD0", 0x60},
	{L"NUMPAD1", 0x61},
	{L"NUMPAD2", 0x62},
	{L"NUMPAD3", 0x63},
	{L"NUMPAD4", 0x64},
	{L"NUMPAD5", 0x65},
	{L"NUMPAD6", 0x66},
	{L"NUMPAD7", 0x67},
	{L"NUMPAD8", 0x68},
	{L"NUMPAD9", 0x69},
	{L"MULTIPLY", 0x6A},
	{L"ADD", 0x6B},
	{L"SEPARATOR", 0x6C},
	{L"SUBTRACT", 0x6D},
	{L"DECIMAL", 0x6E},
	{L"DIVIDE", 0x6F},
	{L"F1", 0x70},
	{L"F2", 0x71},
	{L"F3", 0x72},
	{L"F4", 0x73},
	{L"F5", 0x74},
	{L"F6", 0x75},
	{L"F7", 0x76},
	{L"F8", 0x77},
	{L"F9", 0x78},
	{L"F10", 0x79},
	{L"F11", 0x7A},
	{L"F12", 0x7B},
	{L"F13", 0x7C},
	{L"F14", 0x7D},
	{L"F15", 0x7E},
	{L"F16", 0x7F},
	{L"F17", 0x80},
	{L"F18", 0x81},
	{L"F19", 0x82},
	{L"F20", 0x83},
	{L"F21", 0x84},
	{L"F22", 0x85},
	{L"F23", 0x86},
	{L"F24", 0x87},
	{L"NUMLOCK", 0x90},
	{L"SCROLL", 0x91},
	{L"LSHIFT", 0xA0}, {L"LEFT_SHIFT", 0xA0},
	{L"RSHIFT", 0xA1}, {L"RIGHT_SHIFT", 0xA1},
	{L"LCONTROL", 0xA2}, {L"LEFT_CONTROL", 0xA2}, {L"LCTRL", 0xA2}, {L"LEFT_CTRL", 0xA2},
	{L"RCONTROL", 0xA3}, {L"RIGHT_CONTROL", 0xA3}, {L"RCTRL", 0xA3}, {L"RIGHT_CTRL", 0xA3},
	{L"LMENU", 0xA4}, {L"LEFT_MENU", 0xA4}, {L"LALT", 0xA4}, {L"LEFT_ALT", 0xA4},
	{L"RMENU", 0xA5}, {L"RIGHT_MENU", 0xA5}, {L"RALT", 0xA5}, {L"RIGHT_ALT", 0xA5},
	{L"BROWSER_BACK", 0xA6},
	{L"BROWSER_FORWARD", 0xA7},
	{L"BROWSER_REFRESH", 0xA8},
	{L"BROWSER_STOP", 0xA9},
	{L"BROWSER_SEARCH", 0xAA},
	{L"BROWSER_FAVORITES", 0xAB},
	{L"BROWSER_HOME", 0xAC},
	{L"VOLUME_MUTE", 0xAD},
	{L"VOLUME_DOWN", 0xAE},
	{L"VOLUME_UP", 0xAF},
	{L"MEDIA_NEXT_TRACK", 0xB0},
	{L"MEDIA_PREV_TRACK", 0xB1},
	{L"MEDIA_STOP", 0xB2},
	{L"MEDIA_PLAY_PAUSE", 0xB3},
	{L"LAUNCH_MAIL", 0xB4},
	{L"LAUNCH_MEDIA_SELECT", 0xB5},
	{L"LAUNCH_APP1", 0xB6},
	{L"LAUNCH_APP2", 0xB7},
	{L"OEM_1", 0xBA}, {L";", 0xBA}, {L":", 0xBA}, {L"COLON", 0xBA}, {L"SEMICOLON", 0xBA}, {L"SEMI_COLON", 0xBA},
	{L"OEM_PLUS", 0xBB}, {L"=", 0xBB}, {L"PLUS", 0xBB}, {L"EQUALS", 0xBB}, /* "+" alias already used for numpad + */
	{L"OEM_COMMA", 0xBC}, {L",", 0xBC}, {L"<", 0xBC}, {L"COMMA", 0xBC},
	{L"OEM_MINUS", 0xBD}, {L"MINUS", 0xBD}, {L"UNDERSCORE", 0xBD}, {L"_", 0xBD}, /* "-" alias already used for numpad - */
	{L"OEM_PERIOD", 0xBE}, {L".", 0xBE}, {L">", 0xBE}, {L"PERIOD", 0xBE},
	{L"OEM_2", 0xBF}, {L"/", 0xBF}, {L"?", 0xBF}, {L"SLASH", 0xBF}, {L"FORWARD_SLASH", 0xBF}, {L"QUESTION", 0xBF}, {L"QUESTION_MARK", 0xBF},
	{L"OEM_3", 0xC0}, {L"`", 0xC0}, {L"~", 0xC0}, {L"TILDE", 0xC0}, {L"GRAVE", 0xC0},
	{L"OEM_4", 0xDB}, {L"[", 0xDB}, {L"{", 0xDB},
	{L"OEM_5", 0xDC}, {L"\\", 0xDC}, {L"|", 0xDC}, {L"BACKSLASH", 0xDC}, {L"BACK_SLASH", 0xDC}, {L"PIPE", 0xDC}, {L"VERTICAL_BAR", 0xDC},
	{L"OEM_6", 0xDD}, {L"]", 0xDD}, {L"}", 0xDD},
	{L"OEM_7", 0xDE}, {L"'", 0xDE}, {L"\"", 0xDE}, {L"QUOTE", 0xDE}, {L"DOUBLE_QUOTE", 0xDE},
	{L"OEM_8", 0xDF},
	{L"OEM_102", 0xE2}, /* Either the angle bracket key or the backslash key on the RT 102-key keyboard */
	{L"PROCESSKEY", 0xE5},
	/* {L"PACKET", 0xE7}, Would need special handling for unicode characters */
	{L"ATTN", 0xF6},
	{L"CRSEL", 0xF7},
	{L"EXSEL", 0xF8},
	{L"EREOF", 0xF9},
	{L"PLAY", 0xFA},
	{L"ZOOM", 0xFB},
	{L"NONAME", 0xFC},
	{L"PA1", 0xFD},
	{L"OEM_CLEAR", 0xFE},

	/*
	 * The following are to reduce the impact on existing users with an old
	 * d3dx.ini, provided they are using the default english key bindings.
	 *
	 * NOTE: These will not work to specify key combinations (at least not
	 * the ones with spaces) - for that, use the VK naming.
	 */
	{L"Num 1", 0x61},
	{L"Num 2", 0x62},
	{L"Num 3", 0x63},
	{L"Num 4", 0x64},
	{L"Num 5", 0x65},
	{L"Num 6", 0x66},
	{L"Num 7", 0x67},
	{L"Num 8", 0x68},
	{L"Num 9", 0x69},
	{L"*", 0x6A},
	{L"Num /", 0x6F},
	{L"-", 0x6D},
	{L"+", 0x6B},
	{L"Prnt Scrn", 0x2C},
};

static int ParseVKey(const wchar_t *name)
{
	int i;

	if (wcslen(name) == 1) {
		wchar_t c = towupper(name[0]);
		if ((c >= L'0' && c <= L'9') || (c >= L'A' && c <= L'Z'))
			return c;
	}

	if (!wcsncmp(name, L"0x", 2)) {
		unsigned int vkey;
		swscanf_s(name, L"%x", &vkey);
		return vkey;
	}

	if (!_wcsnicmp(name, L"VK_", 3))
		name += 3;

	for (i = 0; i < ARRAYSIZE(VKMappings); i++) {
		if (!_wcsicmp(name, VKMappings[i].name))
			return VKMappings[i].val;
	}

	return -1;
}

// Reverse lookup of key back to string name

static wstring GetKeyName(int key)
{
	for (int i = 0; i < ARRAYSIZE(VKMappings); i++) {
		if (VKMappings[i].val == key) {
			return (VKMappings[i].name);
		}
	}

	return (L"missing");
}
