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

#include <windows.h>

#include "Console.h"
#include "IsViewer64.h"

#define ISVIEWER64_READ_HEAD	0x04
#define ISVIEWER64_WRITE_HEAD	0x14
#define ISVIEWER64_BUFFER		0x20
#define ISVIEWER64_BUFFER_SIZE	ISVIEWER64_SIZE - ISVIEWER64_BUFFER

static BYTE buffer[ISVIEWER64_SIZE] = {0};

inline DWORD byte_swap(DWORD value) {
	_asm {
		mov eax, value
		bswap eax
		mov value, eax
	}
	return value;
}

DWORD IsViewer64_Read(DWORD location) {
	int index = location - ISVIEWER64_ADDR;

	return byte_swap(*(DWORD*)(buffer + index));
}

void IsViewer64_Write(DWORD location, DWORD value) {
	int index = location - ISVIEWER64_ADDR;

	if (index == ISVIEWER64_WRITE_HEAD && value < ISVIEWER64_BUFFER_SIZE) {
		BOOL consumed = FALSE;
		DWORD read_head = byte_swap(*(DWORD *)(buffer + ISVIEWER64_READ_HEAD));

		if (read_head > value) {
			// Ring buffer has wrapped
			DWORD length = ISVIEWER64_BUFFER_SIZE - read_head;
			if (memchr(&buffer[ISVIEWER64_BUFFER + read_head], '\n', length) != NULL ||
				memchr(&buffer[ISVIEWER64_BUFFER], '\n', value) != NULL) {
				ConsolePrintf("%.*s", length, &buffer[ISVIEWER64_BUFFER + read_head]);
				ConsolePrintf("%.*s", value, &buffer[ISVIEWER64_BUFFER]);
				consumed = TRUE;
			}
		} else {
			DWORD length = value - read_head;
			if (memchr(&buffer[ISVIEWER64_BUFFER + read_head], '\n', length) != NULL) {
				ConsolePrintf("%.*s", length, &buffer[ISVIEWER64_BUFFER + read_head]);
				consumed = TRUE;
			}
		}

		// Update read head
		if (consumed) {
			*(DWORD*)(buffer + ISVIEWER64_READ_HEAD) = byte_swap(value);
		}
	}

	*(DWORD*)(buffer + index) = byte_swap(value);
}
