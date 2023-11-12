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

#include <unordered_map>
#include <windows.h>

extern "C" {
	#include "WatchPoints.h"
	#include "Interpreter CPU.h"
	#include "main.h"
	#include "cpu.h"
}

std::unordered_map<DWORD, int[8]>* WatchPoints = NULL;

void InitWatchPoints(void) {
	// Default size is implementation-defined (may as well be unknown).
	//
	// Since we are using the hash table to quickly avoid false positives with a large
	// keyspace (32-bit virtual addresses) the hash table size should be selected so that
	// more than 99% of address lookups hit an empty bucket.
	//
	// Assuming the average number of watchpoints enabled is 5 and we want a 1% hit-rate
	// (everything else being equal) the optimal size should be 5 / 0.01 = 500.
	const UINT HASH_SIZE = 500;

	WatchPoints = new std::unordered_map<DWORD, int[8]>({}, HASH_SIZE);
}

void AddWatchPoint(DWORD Location, WATCH_TYPE Type) {
	auto search = WatchPoints->find(Location & ~7);
	if (search == WatchPoints->end()) {
		int* wp = (*WatchPoints)[Location & ~7];
		for (int i = 0; i < 8; i++) {
			wp[i] = WP_NONE;
		}
	}

	(*WatchPoints)[Location & ~7][Location & 7] = (int)Type | WP_ENABLED;
}

void RemoveWatchPoint(DWORD Location) {
	auto search = WatchPoints->find(Location & ~7);
	if (search != WatchPoints->end()) {
		int *wp = search->second;
		wp[Location & 7] = WP_NONE;

		int i;
		for (i = 0; i < 8; i++) {
			if (wp[i] != WP_NONE) {
				break;
			}
		}
		if (i == 8) {
			WatchPoints->erase(Location & ~7);
		}
	}
}

void RemoveAllWatchPoints(void) {
	WatchPoints->clear();
}

void ToggleWatchPoint(DWORD Location) {
	WATCH_TYPE Type = HasWatchPoint(Location);
	if (Type != WP_NONE) {
		(*WatchPoints)[Location & ~7][Location & 7] = (int)Type ^ WP_ENABLED;
	}
}

WATCH_TYPE HasWatchPoint(DWORD Location) {
	auto search = WatchPoints->find(Location & ~7);
	if (search == WatchPoints->end()) {
		return WP_NONE;
	}

	return (WATCH_TYPE)search->second[Location & 7];
}

BOOL CheckForWatchPoint(DWORD Location, WATCH_TYPE Type, int Size) {
	// The memory barrier here is a precaution for inlining this function in tight sequences.
	// E.g. DMAs will need to check for watchpoints on read and write accesses
	// between the source and destination with two calls.
	//
	// The barrier ensures that if a watchpoint has trapped the CPU thread and is then released
	// by stopping the CPU, the second funtion call will not attempt to trap the CPU again.
	// Thus avoiding a potential deadlock.
	MemoryBarrier();

	if (!HaveDebugger || CPU_Action.CloseCPU || CPU_Action.Stepping || WatchPoints->empty()) {
		return FALSE;
	}

	auto search = WatchPoints->find(Location & ~7);
	if (search == WatchPoints->end()) {
		return FALSE;
	}

	int *wp = search->second;
	int start = Location & 7;
	for (int i = start; i < start + Size; i++) {
		int value = wp[i];
		if ((value & WP_ENABLED) && (value & (int)Type)) {
			TriggerDebugger();

			// Block the CPU thread until resumed by the debugger
			WaitForSingleObject(CPU_Action.hStepping, INFINITE);

			return TRUE;
		}
	}

	return FALSE;
}

int CountWatchPoints(void) {
	int count = 0;

	for (auto &iter : *WatchPoints) {
		int* wp = iter.second;
		for (int i = 0; i < 8; i++) {
			if (wp[i] != WP_NONE) {
				count++;
			}
		}
	}

	return count;
}

void RefreshWatchPoints(HWND hList) {
	char message[100];

	for (auto &iter : *WatchPoints) {
		int key = iter.first;
		int *wp = iter.second;
		for (int i = 0; i < 8; i++) {
			int value = wp[i];
			if (value == WP_NONE) {
				continue;
			}

			char flags[5] = "----";
			if (value & WP_ENABLED) {
				flags[0] = 'e';
			}
			if (value & WP_READ) {
				flags[2] = 'r';
			}
			if (value & WP_WRITE) {
				flags[3] = 'w';
			}

			int location = key | i;
			sprintf(message, " at 0x%08X (r4300i %s)", location, flags);
			SendMessage(hList, LB_ADDSTRING, 0, (LPARAM)message);
			int index = SendMessage(hList, LB_GETCOUNT, 0, 0) - 1;
			SendMessage(hList, LB_SETITEMDATA, index, (LPARAM)location);
		}
	}
}
