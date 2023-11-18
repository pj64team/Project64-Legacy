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

#ifndef __watchpoints_h
#define __watchpoints_h

#include "Types.h"

typedef enum {
	WP_NONE,
	WP_READ,
	WP_WRITE,
	WP_READ_WRITE,
	WP_ENABLED,
} WATCH_TYPE;

void InitWatchPoints(void);
void AddWatchPoint(MIPS_DWORD Location, WATCH_TYPE Type);
void RemoveWatchPoint(MIPS_DWORD Location);
void ToggleWatchPoint(MIPS_DWORD Location);
void RemoveAllWatchPoints(void);
WATCH_TYPE HasWatchPoint(MIPS_DWORD Location);
BOOL CheckForWatchPoint(MIPS_DWORD Location, WATCH_TYPE Type, int Size);
int CountWatchPoints(void);
void RefreshWatchPoints(HWND hList);

#endif
