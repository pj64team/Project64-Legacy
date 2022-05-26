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

#include <unordered_map>
#include <windows.h>

extern "C" {
	#include "WatchPoints.h"
	#include "Interpreter CPU.h"
	#include "main.h"
	#include "cpu.h"
}

std::unordered_map<DWORD, int> *WatchPoints = NULL;

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

	WatchPoints = new std::unordered_map<DWORD, int>({}, HASH_SIZE);
}

void AddWatchPoint(DWORD Location, WATCH_TYPE Type) {
	(*WatchPoints)[Location] = (int)Type;
}

void RemoveWatchPoint(DWORD Location) {
	WatchPoints->erase(Location);
}

void RemoveAllWatchPoints(void) {
	WatchPoints->clear();
}

BOOL CheckForWatchPoint(DWORD Location, WATCH_TYPE Type) {
	if (!HaveDebugger || CPU_Action.Stepping || WatchPoints->empty()) {
		return FALSE;
	}

	auto search = WatchPoints->find(Location);
	if (search == WatchPoints->end()) {
		return FALSE;
	}

	int value = search->second;
	if (value & (int)Type) {
		TriggerDebugger(TRUE);
		return TRUE;
	}

	return FALSE;
}

int CountWatchPoints(void) {
	return WatchPoints->size();
}

void RefreshWatchPoints(HWND hList) {
	char message[100];

	for (auto iter : *WatchPoints) {
		DWORD key = iter.first;
		int value = iter.second;

		sprintf(message, " at 0x%08X (r4300i %s)", key, (value == READ ? "-r-" : (value == WRITE ? "--w" : "-rw")));
		SendMessage(hList, LB_ADDSTRING, 0, (LPARAM)message);
		int index = SendMessage(hList, LB_GETCOUNT, 0, 0) - 1;
		SendMessage(hList, LB_SETITEMDATA, index, (LPARAM)key);
	}
}
