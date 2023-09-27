/*
 * Project 64 - A Nintendo 64 emulator.
 *
 * (c) Copyright 2001 zilmar (zilmar@emulation64.com) and 
 * Jabo (jabo@emulation64.com).
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
#include <Windows.h>
#include "main.h"
#include "debugger.h"
#include "cpu.h"

void SetupTLB_Entry (int Entry);

FASTTLB FastTlb[64];
TLB tlb[32];

BOOL AddressDefined ( DWORD VAddr) {
	DWORD i;

	if (VAddr >= 0x80000000 && VAddr <= 0xBFFFFFFF) {
		return TRUE;
	}

	for (i = 0; i < 64; i++) {
		if (FastTlb[i].ValidEntry == FALSE) { continue; }
		if (VAddr >= FastTlb[i].VSTART && VAddr <= FastTlb[i].VEND) {
			return TRUE;
		}
	}	
	return FALSE;	
}

void InitilizeTLB (void) {
	DWORD count;

	for (count = 0; count < 32; count++) { tlb[count].EntryDefined = FALSE; }
	for (count = 0; count < 64; count++) { FastTlb[count].ValidEntry = FALSE; }	
	SetupTLB();
}

void SetupTLB (void) {
	DWORD count;

	memset(TLB_ReadMap,0,(0xFFFFF * sizeof(DWORD)));
	memset(TLB_WriteMap,0,(0xFFFFF * sizeof(DWORD)));
	for (count = 0x80000000; count < 0xC0000000; count += 0x1000) {
		TLB_ReadMap[count >> 12] = ((DWORD)N64MEM + (count & 0x1FFFFFFF)) - count;
		TLB_WriteMap[count >> 12] = ((DWORD)N64MEM + (count & 0x1FFFFFFF)) - count;
	}
	for (count = 0; count < 32; count ++) { SetupTLB_Entry(count); }
	//GE Hack
	//for (count = 0x7F000000; count < 0x80000000; count += 0x1000) {
	//	TLB_ReadMap[count >> 12] = ((DWORD)N64MEM + (count - 0x7F000000 + 0x10034b30)) - count;
	//	TLB_WriteMap[count >> 12] = ((DWORD)N64MEM + (count - 0x7F000000 + 0x10034b30)) - count;
	//}
}

void SetupTLB_Entry (int Entry) {
	int FastIndx;

	if (!tlb[Entry].EntryDefined) { return; }
	FastIndx = Entry << 1;
	FastTlb[FastIndx].VSTART=tlb[Entry].EntryHi.BreakDownEntryHi.VPN2 << 13;
	FastTlb[FastIndx].VEND = FastTlb[FastIndx].VSTART + (tlb[Entry].PageMask.BreakDownPageMask.Mask << 12) + 0xFFF;
	FastTlb[FastIndx].PHYSSTART = tlb[Entry].EntryLo0.BreakDownEntryLo0.PFN << 12;
	FastTlb[FastIndx].VALID = tlb[Entry].EntryLo0.BreakDownEntryLo0.V;
	FastTlb[FastIndx].DIRTY = tlb[Entry].EntryLo0.BreakDownEntryLo0.D; 
	FastTlb[FastIndx].GLOBAL = tlb[Entry].EntryLo0.BreakDownEntryLo0.GLOBAL & tlb[Entry].EntryLo1.BreakDownEntryLo1.GLOBAL;
	FastTlb[FastIndx].ValidEntry = FALSE;

	FastIndx = (Entry << 1) + 1;
	FastTlb[FastIndx].VSTART=(tlb[Entry].EntryHi.BreakDownEntryHi.VPN2 << 13) + ((tlb[Entry].PageMask.BreakDownPageMask.Mask << 12) + 0xFFF + 1);
	FastTlb[FastIndx].VEND = FastTlb[FastIndx].VSTART + (tlb[Entry].PageMask.BreakDownPageMask.Mask << 12) + 0xFFF;
	FastTlb[FastIndx].PHYSSTART = tlb[Entry].EntryLo1.BreakDownEntryLo1.PFN << 12;
	FastTlb[FastIndx].VALID = tlb[Entry].EntryLo1.BreakDownEntryLo1.V;
	FastTlb[FastIndx].DIRTY = tlb[Entry].EntryLo1.BreakDownEntryLo1.D; 
	FastTlb[FastIndx].GLOBAL = tlb[Entry].EntryLo0.BreakDownEntryLo0.GLOBAL & tlb[Entry].EntryLo1.BreakDownEntryLo1.GLOBAL;
	FastTlb[FastIndx].ValidEntry = FALSE;

	for ( FastIndx = Entry << 1; FastIndx <= (Entry << 1) + 1; FastIndx++) {
		DWORD count;

		if (!FastTlb[FastIndx].VALID) { 
			FastTlb[FastIndx].ValidEntry = TRUE;
			continue; 
		}
		if (FastTlb[FastIndx].VEND <= FastTlb[FastIndx].VSTART) {
			if (ShowDebugMessages)
				DisplayError("Vstart = Vend for tlb mapping");
			continue;
		}
		if (FastTlb[FastIndx].VSTART >= 0x80000000 && FastTlb[FastIndx].VEND <= 0xBFFFFFFF) {
			continue;
		}
		if (FastTlb[FastIndx].PHYSSTART > 0x1FFFFFFF) {
			continue;				
		}
	
		//if (FastTlb[FastIndx].GLOBAL == 0) { 
		//	DisplayError("Non Global TLB Entry ???");
		//	continue;
		//}
		
		//test if overlap
		FastTlb[FastIndx].ValidEntry = TRUE;
		for (count = FastTlb[FastIndx].VSTART; count < FastTlb[FastIndx].VEND; count += 0x1000) {
			TLB_ReadMap[count >> 12] = ((DWORD)N64MEM + (count - FastTlb[FastIndx].VSTART + FastTlb[FastIndx].PHYSSTART)) - count;
			if (!FastTlb[FastIndx].DIRTY) { continue; }
			TLB_WriteMap[count >> 12] = ((DWORD)N64MEM + (count - FastTlb[FastIndx].VSTART + FastTlb[FastIndx].PHYSSTART)) - count;
		}
	}
}

void TLB_Probe (void) {
	int Counter;
	
	if (HaveDebugger && LogOptions.GenerateLog) { 
		if (LogOptions.LogTLB) { 
			LogMessage("%08X: TLB Probe:  EntryHI :%X",PROGRAM_COUNTER,ENTRYHI_REGISTER);
		}
	}

	INDEX_REGISTER = 0x80000000;

	for (Counter = 0; Counter < 32; Counter ++) {		
		QWORD TlbValue = tlb[Counter].EntryHi.Value & (~((QWORD)tlb[Counter].PageMask.BreakDownPageMask.Mask) << 13);
		QWORD EntryHi = ENTRYHI_REGISTER & (~((QWORD)tlb[Counter].PageMask.BreakDownPageMask.Mask) << 13);

		if (TlbValue == EntryHi) {
			BOOL Global = tlb[Counter].EntryLo0.BreakDownEntryLo0.GLOBAL && tlb[Counter].EntryLo1.BreakDownEntryLo1.GLOBAL;
			BOOL SameAsid = ((tlb[Counter].EntryHi.Value & 0xFF) == (ENTRYHI_REGISTER & 0xFF));
			
			if (Global || SameAsid) {
				INDEX_REGISTER = Counter;
				return;
			}
		}
	}
}

void TLB_Read (void) {
	DWORD index = INDEX_REGISTER & 0x1F;

	if (HaveDebugger && LogOptions.GenerateLog) { 
		if (LogOptions.LogTLB) { 
			LogMessage("%08X: TLB Read:  index :%X     %X - %X",PROGRAM_COUNTER,INDEX_REGISTER,
				tlb[index].EntryHi.Value,(tlb[index].EntryHi.Value & ~tlb[index].PageMask.Value));
		}
	}

	PAGE_MASK_REGISTER = tlb[index].PageMask.Value ;
	ENTRYHI_REGISTER = (tlb[index].EntryHi.Value & ~((unsigned long long)tlb[index].PageMask.Value));
	ENTRYLO0_REGISTER = tlb[index].EntryLo0.Value;
	ENTRYLO1_REGISTER = tlb[index].EntryLo1.Value;		
}

BOOL TranslateVaddr ( DWORD * Addr) {
	if (TLB_ReadMap[*Addr >> 12] == 0) { return FALSE; }
	*Addr = (DWORD)((BYTE *)(TLB_ReadMap[*Addr >> 12] + *Addr) - N64MEM);
	return TRUE;
}

void _fastcall WriteTLBEntry (int index) {
	int FastIndx;

#ifdef TLB_HACK	
	FastIndx = index << 1;
	if ((PROGRAM_COUNTER >= FastTlb[FastIndx].VSTART && 
		PROGRAM_COUNTER < FastTlb[FastIndx].VEND &&
		FastTlb[FastIndx].ValidEntry && FastTlb[FastIndx].VALID)
		|| 
		(PROGRAM_COUNTER >= FastTlb[FastIndx + 1].VSTART && 
		PROGRAM_COUNTER < FastTlb[FastIndx + 1].VEND &&
		FastTlb[FastIndx + 1].ValidEntry && FastTlb[FastIndx + 1].VALID))
	{
		if (HaveDebugger && LogOptions.GenerateLog && LogOptions.LogTLB) { 
			LogMessage("%08X: TLB write:  Index: %d   PageMask: %X  EntryLo0: %X  EntryLo1: %X  EntryHi: %llX",PROGRAM_COUNTER,
				index,PAGE_MASK_REGISTER,ENTRYLO0_REGISTER,ENTRYLO1_REGISTER,ENTRYHI_REGISTER);
			LogMessage("       Being Ignored");
			LogMessage("");
		}

		return;
	}
#endif

	if (tlb[index].EntryDefined) {
		DWORD count;

		for ( FastIndx = index << 1; FastIndx <= (index << 1) + 1; FastIndx++) {
			if (!FastTlb[FastIndx].ValidEntry) { continue; }
			if (!FastTlb[FastIndx].VALID) { continue; }
			for (count = FastTlb[FastIndx].VSTART; count < FastTlb[FastIndx].VEND; count += 0x1000) {
				TLB_ReadMap[count >> 12] = 0;
				TLB_WriteMap[count >> 12] = 0;
			}
		}
	}

	DWORD pageMask = PAGE_MASK_REGISTER & 0x01554000; // 0000 0001 0101 0101 0100 0000 0000 0000
	pageMask |= pageMask >> 1;

	tlb[index].PageMask.Value = pageMask;
	tlb[index].EntryHi.Value = ENTRYHI_REGISTER;
	tlb[index].EntryLo0.Value = ENTRYLO0_REGISTER & 0x3FFFFFF;
	tlb[index].EntryLo1.Value = ENTRYLO1_REGISTER & 0x3FFFFFF;
	if (!tlb[index].EntryLo0.BreakDownEntryLo0.GLOBAL || !tlb[index].EntryLo1.BreakDownEntryLo1.GLOBAL) {
		tlb[index].EntryLo0.BreakDownEntryLo0.GLOBAL = 0;
		tlb[index].EntryLo1.BreakDownEntryLo1.GLOBAL = 0;
	}
	tlb[index].EntryDefined = TRUE;
	
	if (HaveDebugger && LogOptions.GenerateLog && LogOptions.LogTLB) { 
		LogMessage("%08X: TLB write:  Index: %d   PageMask: %X  EntryLo0: %X  EntryLo1: %X  EntryHi: %llX",PROGRAM_COUNTER,
			index,PAGE_MASK_REGISTER,ENTRYLO0_REGISTER,ENTRYLO1_REGISTER,ENTRYHI_REGISTER);
		LogMessage("      Entry 1:  VStart: %X   VEnd: %X  Physical Start: %X",
			tlb[index].EntryHi.BreakDownEntryHi.VPN2 << 13, //VStart
			(tlb[index].EntryHi.BreakDownEntryHi.VPN2 << 13) + (tlb[index].PageMask.BreakDownPageMask.Mask << 12) + 0xFFF, //Vend
			tlb[index].EntryLo0.BreakDownEntryLo0.PFN << 12);
		LogMessage("      Entry 2:  VStart: %X   VEnd: %X  Physical Start: %X",
			(tlb[index].EntryHi.BreakDownEntryHi.VPN2 << 13) + ((tlb[index].PageMask.BreakDownPageMask.Mask << 12) + 0xFFF + 1), //VStart
			(tlb[index].EntryHi.BreakDownEntryHi.VPN2 << 13) + ((tlb[index].PageMask.BreakDownPageMask.Mask << 12) + 0xFFF + 1) + (tlb[index].PageMask.BreakDownPageMask.Mask << 12) + 0xFFF, //Vend
			tlb[index].EntryLo1.BreakDownEntryLo1.PFN << 12);
		LogMessage("");
	}

	SetupTLB_Entry(index);
	if (HaveDebugger)
		RefreshTLBWindow();
}
