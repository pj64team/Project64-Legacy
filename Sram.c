/*
 * MiB64 - A Nintendo 64 emulator.
 *
 * Project64 (c) Copyright 2001 Zilmar, Jabo, Smiff, Gent, Witten
 * Projectg64 Legacy (c) Copyright 2010 PJ64LegacyTeam
 * MiB64 (c) Copyright 2024 MiB64Team
 *
 * MiB64 Homepage: www.mib64.net
 *
 * Permission to use, copy, modify and distribute MiB64 in both binary and
 * source form, for non-commercial purposes, is hereby granted without fee,
 * providing that this license information and copyright notice appear with
 * all copies and any derived work.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event shall the authors be held liable for any damages
 * arising from the use of this software.
 *
 * MiB64 is freeware for PERSONAL USE only. Commercial users should
 * seek permission of the copyright holders first. Commercial use includes
 * charging money for MiB64 or software derived from MiB64.
 *
 * The copyright holders request that bug fixes and improvements to the code
 * should be forwarded to them so if they want them.
 *
 */
#include <windows.h>
#include <stdio.h>
#include "main.h"
#include "CPU.h"

static HANDLE hSramFile = NULL;

void CloseSram(void) {
	if (hSramFile) {
		CloseHandle(hSramFile);
		hSramFile = NULL;
	}
}

BOOL LoadSram(void) {
	char File[255], Directory[255];
	LPVOID lpMsgBuf;

	Settings_GetDirectory(AutoSaveDir, Directory, sizeof(Directory));
	sprintf(File, "%s%s.sra", Directory, RomFullName);

	hSramFile = CreateFile(File, GENERIC_WRITE | GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_ALWAYS,
		FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS, NULL);
	if (hSramFile == INVALID_HANDLE_VALUE) {
		switch (GetLastError()) {
		case ERROR_PATH_NOT_FOUND:
			CreateDirectory(Directory, NULL);
			hSramFile = CreateFile(File, GENERIC_WRITE | GENERIC_READ, FILE_SHARE_READ,
				NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS, NULL);
			if (hSramFile == INVALID_HANDLE_VALUE) {
				FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
					FORMAT_MESSAGE_IGNORE_INSERTS, NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
					(LPTSTR)&lpMsgBuf, 0, NULL
				);
				DisplayError(lpMsgBuf);
				LocalFree(lpMsgBuf);
				return FALSE;
			}
			break;
		default:
			FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM |
				FORMAT_MESSAGE_IGNORE_INSERTS, NULL, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
				(LPTSTR)&lpMsgBuf, 0, NULL
			);
			DisplayError(lpMsgBuf);
			LocalFree(lpMsgBuf);
			return FALSE;
		}
	}
	return TRUE;
}

void DmaFromSram(BYTE* dest, int StartOffset, int len) {
	DWORD dwRead;

	if (hSramFile == NULL && !LoadSram())
		return;
	SetFilePointer(hSramFile, StartOffset, NULL, FILE_BEGIN);
	ReadFile(hSramFile, dest, len, &dwRead, NULL);
}

void DmaToSram(BYTE* Source, int StartOffset, int len) {
	DWORD dwWritten;

	if (hSramFile == NULL && !LoadSram())
		return;
	SetFilePointer(hSramFile, StartOffset, NULL, FILE_BEGIN);
	WriteFile(hSramFile, Source, len, &dwWritten, NULL);
}
