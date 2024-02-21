/*
 * Project 64 Legacy - A Nintendo 64 emulator.
 *
 * (c) Copyright 2022 parasyte (jay@kodewerx.org)
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

#ifndef __watchpoints_h
#define __watchpoints_h

#include "Types.h"

typedef enum {
	WP_NONE,		// 0b0000
	WP_READ,		// 0b0001
	WP_WRITE,		// 0b0010
	WP_READ_WRITE,	// 0b0011
	WP_ENABLED,		// 0b0100
} WATCH_TYPE;

// The mask that defines "all bits enabled" in a WATCH_TYPE value.
#define WATCH_TYPE_MASK 7

void InitWatchPoints(void);
BOOL AddWatchPoint(MIPS_DWORD Location, WATCH_TYPE Type);
void RemoveWatchPoint(MIPS_DWORD Location);
void ToggleWatchPoint(MIPS_DWORD Location);
void RemoveAllWatchPoints(void);
WATCH_TYPE HasWatchPoint(MIPS_DWORD Location);
BOOL CheckForWatchPoint(MIPS_DWORD Location, WATCH_TYPE Type, int Size);
int CountWatchPoints(void);
void RefreshWatchPoints(HWND hList);

#endif
