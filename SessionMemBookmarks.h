/*
 * MiB64 - A Nintendo 64 emulator.
 *
 * (c) Copyright 2023 parasyte (jay@kodewerx.org)
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

#include "Types.h"

#pragma once

#define MAX_MEM_BOOKMARKS 1000

struct MEM_BOOKMARK {
	char name[100];
	unsigned int width_required;
	MIPS_DWORD selection_range[2];
	BOOL is_virtual;
};

BOOL Session_Load_MemBookmarks(unsigned int *num_bookmarks, struct MEM_BOOKMARK *bookmarks, unsigned int max_bookmarks, char *filename);
BOOL Session_Save_MemBookmarks(char *filename, struct MEM_BOOKMARK *bookmarks, unsigned int num_bookmarks);
