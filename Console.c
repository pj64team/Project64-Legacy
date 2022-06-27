/*
 * Project 64 - A Nintendo 64 emulator.
 *
 * (c) Copyright 2022 parasyte (jay@kodewerx.org)
 *
 * pj64 homepage: www.pj64.net
 *
 * Permission to use, copy, modify and distribute Project64 in both binary and
 * source form, for non-commercial purposes, is hereby granted without fee,
 * providing that this license information and copyright notice appear with
 * all copies and any derived work.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event shall the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Project64 is freeware for PERSONAL USE only. Commercial users should
 * seek permission of the copyright holders first. Commercial use includes
 * charging money for Project64 or software derived from Project64.
 *
 * The copyright holders request that bug fixes and improvements to the code
 * should be forwarded to them so if they want them.
 *
 */

#include <stdio.h>
#include <windows.h>

#include "main.h"
#include "Console.h"
#include "Settings Api.h"

// See: https://docs.microsoft.com/en-us/windows/win32/intl/code-page-identifiers
#define CODEPAGE_EUC_JP 20932

static HANDLE STDOUT = NULL;
static HANDLE hOutputMutex = NULL;
static char OUTPUT_BUFFER[4096];
static wchar_t WIDE_BUFFER[4096];

void CreateConsole(void);
void ConfigureConsole(void);

void InitConsole(void) {
	hOutputMutex = CreateMutex(NULL, FALSE, NULL);

	if (Settings_ReadBool(APPS_NAME, STR_SETTINGS, STR_SHOWCONSOLEWINDOW, Default_ShowConsoleWindow)) {
		CreateConsole();
	}
}

void CreateConsole(void) {
	AllocConsole();

	STDOUT = GetStdHandle(STD_OUTPUT_HANDLE);
	ConfigureConsole();

	// Update settings to remember the console window state
	Settings_Write(APPS_NAME, STR_SETTINGS, STR_SHOWCONSOLEWINDOW, STR_TRUE);
}

void CloseConsole(void) {
	FreeConsole();
	STDOUT = NULL;

	// Update settings to remember the console window state
	Settings_Write(APPS_NAME, STR_SETTINGS, STR_SHOWCONSOLEWINDOW, STR_FALSE);
}

BOOL ToggleConsole(void) {
	if (STDOUT == NULL) {
		CreateConsole();
		return TRUE;
	} else {
		CloseConsole();
		return FALSE;
	}
}

void ConfigureConsole(void) {
	// Enable ANSI color codes
	char *mode_error = "";
	if (SetConsoleMode(STDOUT, ENABLE_PROCESSED_OUTPUT | ENABLE_WRAP_AT_EOL_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING) == 0) {
		SetConsoleMode(STDOUT, ENABLE_PROCESSED_OUTPUT | ENABLE_WRAP_AT_EOL_OUTPUT);
		mode_error = "ERROR: Your console does not support VT100 output. Colors will not work and text may appear garbled.\n";
	}

	// Check for EUC-JP support. Required for most commercial games written in Japan.
	if (IsValidCodePage(CODEPAGE_EUC_JP) == TRUE) {
		// Set the font to one which can render Japanese
		CONSOLE_FONT_INFOEX font = { 0 };
		font.cbSize = sizeof(CONSOLE_FONT_INFOEX);
		font.dwFontSize.X = 7;
		font.dwFontSize.Y = 14;
		font.FontFamily = FF_DONTCARE | TMPF_VECTOR | TMPF_TRUETYPE;
		font.FontWeight = 400;
		wcscpy_s(font.FaceName, LF_FACESIZE, L"MS Gothic");
		SetCurrentConsoleFontEx(STDOUT, FALSE, &font);

		ConsolePrintf("%s\n%s\n", AppName, mode_error);
	} else {
		ConsolePrintf("%s\n%s\x1b[31mERROR:\x1b[0m EUC-JP CodePage is not avaialble!\n\n", AppName, mode_error);
	}
}

void ConsolePrintf(char *fmt, ...) {
	if (STDOUT == NULL) {
		return;
	}

	DWORD wait_result = WaitForSingleObject(hOutputMutex, INFINITE);
	if (wait_result == WAIT_OBJECT_0) {
		va_list args;
		va_start(args, fmt);
		int length = vsnprintf(OUTPUT_BUFFER, sizeof(OUTPUT_BUFFER), fmt, args);
		va_end(args);

		if (length > 0 && length < sizeof(OUTPUT_BUFFER)) {
			int chars = MultiByteToWideChar(CODEPAGE_EUC_JP, MB_PRECOMPOSED, OUTPUT_BUFFER, length, WIDE_BUFFER, 4096);
			if (chars > 0) {
				WriteConsoleW(STDOUT, WIDE_BUFFER, chars, NULL, NULL);
			} else {
				char buffer[100];
				length = sprintf(buffer, "\x1b[31mERROR:\x1b[0m Unable to decode text. Error code %d\n", GetLastError());
				WriteConsole(STDOUT, buffer, length, NULL, NULL);
			}
		}
	}
	ReleaseMutex(hOutputMutex);
}