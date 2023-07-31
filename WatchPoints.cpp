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

// Each watchpoint covers 8 bytes of memory, aligned to 64-bit boundaries.
// The 8 bytes stored in the map are bitflags with the following layout:
//
// ╭─────┬──────┬──────┬───┬───┬───╮
// │ Bit │ 7..5 │ 4..3 │ 2 │ 1 │ 0 │
// ╰─────┴───┬──┴───┬──┴─┬─┴─┬─┴─┬─╯
//           ┆      ┆    ┆   ┆   ╰┄┄┄┄ Read access
//           ┆      ┆    ┆   ╰┄┄┄┄┄┄┄┄ Write access
//           ┆      ┆    ╰┄┄┄┄┄┄┄┄┄┄┄┄ Watchpoint enabled
//           ┆      ╰┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄ RESERVED
//           ╰┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄┄ Upper bits of original address
//
std::unordered_map<QWORD, uint8_t[8]>* WatchPoints = NULL;

std::unordered_map<QWORD, QWORD> WatchPointsAddresses; // Used to manage the watch points in GUI

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

	WatchPoints = new std::unordered_map<QWORD, uint8_t[8]>({}, HASH_SIZE);
}

BOOL AddWatchPoint(MIPS_DWORD Location, WATCH_TYPE Type) {
	if ((Location.UW[0] >= 0x20000000 && Location.UW[0] < 0x80000000) || (Location.UW[0] >= 0xC0000000)) {
		// TODO: Currently only supports physical addresses below 0x20000000, KSEG0, and KSEG1.
		// This is a horrible hack, but we assume that any address below 0x20000000 is a physical address.
		return FALSE;
	}

	MIPS_DWORD requested_location = Location;
	uint8_t upper_bits = (Location.UDW >> 24) & 0xE0;
	Location.UDW &= 0x1FFFFFFF;
	auto search = WatchPoints->find(Location.UDW & ~7);
	if (search == WatchPoints->end()) {
		uint8_t *wp = (*WatchPoints)[Location.UDW & ~7];
		for (int i = 0; i < 8; i++) {
			wp[i] = WP_NONE;
		}
	}

	(*WatchPoints)[Location.UDW & ~7][Location.UB[0] & 7] = (uint8_t)Type | WP_ENABLED | upper_bits;
	WatchPointsAddresses[Location.UDW] = requested_location.UDW;

	return TRUE;
}

void RemoveWatchPoint(MIPS_DWORD Location) {
	Location.UDW &= 0x1FFFFFFF;
	auto search = WatchPoints->find(Location.UDW & ~7);
	if (search != WatchPoints->end()) {
		uint8_t *wp = search->second;
		wp[Location.UB[0] & 7] = WP_NONE;
		WatchPointsAddresses.erase(Location.UDW);

		int i;
		for (i = 0; i < 8; i++) {
			if (wp[i] != WP_NONE) {
				break;
			}
		}
		if (i == 8) {
			WatchPoints->erase(Location.UDW & ~7);
		}
	}
}

void RemoveAllWatchPoints(void) {
	WatchPoints->clear();
	WatchPointsAddresses.clear();
}

void ToggleWatchPoint(MIPS_DWORD Location) {
	Location.UDW &= 0x1FFFFFFF;
	WATCH_TYPE Type = HasWatchPoint(Location);
	if (Type != WP_NONE) {
		(*WatchPoints)[Location.UDW & ~7][Location.UB[0] & 7] = (int)Type ^ WP_ENABLED;
	}
}

WATCH_TYPE HasWatchPoint(MIPS_DWORD Location) {
	Location.UDW &= 0x1FFFFFFF;
	auto search = WatchPoints->find(Location.UDW & ~7);
	if (search == WatchPoints->end()) {
		return WP_NONE;
	}

	return (WATCH_TYPE)search->second[Location.UB[0] & 7];
}

BOOL CheckForWatchPoint(MIPS_DWORD Location, WATCH_TYPE Type, int Size) {
	// The memory barrier here is a precaution for inlining this function in tight sequences.
	// E.g. DMAs will need to check for watchpoints on read and write accesses
	// between the source and destination with two calls.
	//
	// The barrier ensures that if a watchpoint has trapped the CPU thread and is then released
	// by stopping the CPU, the second funtion call will not attempt to trap the CPU again.
	// Thus avoiding a potential deadlock.
	MemoryBarrier();

	Location.UDW &= 0x1FFFFFFF;

	if (!HaveDebugger || CPU_Action.CloseCPU || CPU_Action.Stepping || WatchPoints->empty()) {
		return FALSE;
	}

	while (Size > 0) {
		int start = Location.UB[0] & 7;
		int end = min(start + Size, 8);

		Location.UDW &= ~7;

		auto search = WatchPoints->find(Location.UDW);
		if (search != WatchPoints->end()) {
			uint8_t *wp = search->second;
			for (int i = start; i < end; i++) {
				int value = wp[i];
				if ((value & WP_ENABLED) && (value & (int)Type)) {
					TriggerDebugger();

					// Block the CPU thread until resumed by the debugger
					WaitForSingleObject(CPU_Action.hStepping, INFINITE);

					return TRUE;
				}
			}
		}

		Location.UDW += 8;
		Size -= (8 - start);
	}

	return FALSE;
}

int CountWatchPoints(void) {
	int count = 0;

	for (auto &iter : *WatchPoints) {
		uint8_t *wp = iter.second;
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
		QWORD key = iter.first;
		uint8_t *wp = iter.second;
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

			QWORD location = key | i;
			auto requested_location = WatchPointsAddresses.find(location);
			QWORD *address = &(*requested_location).second;
			sprintf(message, " at 0x%016llX (r4300i %s)", *address, flags);
			SendMessage(hList, LB_ADDSTRING, 0, (LPARAM)message);
			int index = SendMessage(hList, LB_GETCOUNT, 0, 0) - 1;
			SendMessage(hList, LB_SETITEMDATA, index, (LPARAM)address);
		}
	}
}
