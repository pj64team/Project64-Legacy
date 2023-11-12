/*
 * Project 64 Legacy - A Nintendo 64 emulator.
 *
 * (c) Copyright 2023 parasyte (jay@kodewerx.org)
 *
 * Project64 Legacy Homepage: www.project64-legacy.com
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

#pragma once

#define MAX_MEM_BOOKMARKS 1000

struct MEM_BOOKMARK {
	char name[100];
	unsigned int width_required;
	DWORD selection_range[2];
	BOOL is_virtual;
};

BOOL Session_Load_MemBookmarks(unsigned int *num_bookmarks, struct MEM_BOOKMARK *bookmarks, unsigned int max_bookmarks, char *filename);
BOOL Session_Save_MemBookmarks(char *filename, struct MEM_BOOKMARK *bookmarks, unsigned int num_bookmarks);