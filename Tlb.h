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
typedef struct {
	BOOL EntryDefined;
	union {
		unsigned long Value;
		unsigned char A[4];
		
		struct BreakDownPageMask{
			unsigned zero : 13;
			unsigned Mask : 12;
			unsigned zero2 : 7;
		} BreakDownPageMask;
		
	} PageMask;
	
	union {
		unsigned long long Value;
		unsigned char A[4];
		
		struct BreakDownEntryHi {
			unsigned ASID : 8;
			unsigned Zero : 4;
			unsigned G : 1;
			unsigned VPN2 : 19;
		} BreakDownEntryHi;
		
	} EntryHi;

	union {
		unsigned long Value;
		unsigned char A[4];
		
		struct BreakDownEntryLo0{
			unsigned GLOBAL: 1;
			unsigned V : 1;
			unsigned D : 1;
			unsigned C : 3;
			unsigned PFN : 20;
			unsigned ZERO: 6;
		} BreakDownEntryLo0;
		
	} EntryLo0;
	
	union {
		unsigned long Value;
		unsigned char A[4];
		
		struct BreakDownEntryLo1{
			unsigned GLOBAL: 1;
			unsigned V : 1;
			unsigned D : 1;
			unsigned C : 3;
			unsigned PFN : 20;
			unsigned ZERO: 6;
		} BreakDownEntryLo1;
		
	} EntryLo1;
} TLB;

typedef struct {
   DWORD VSTART;
   DWORD VEND;
   DWORD PHYSSTART;
   BOOL VALID;
   BOOL DIRTY;
   BOOL GLOBAL;
   BOOL ValidEntry;
} FASTTLB; 

extern FASTTLB FastTlb[64];
extern TLB tlb[32];

BOOL AddressDefined ( DWORD VAddr);
void InitilizeTLB   ( void );
void SetupTLB       ( void );
void TLB_Probe      ( void );
void TLB_Read       ( void );
BOOL TranslateVaddr ( DWORD * Addr);
void _fastcall WriteTLBEntry  ( int index );
BOOL Translate64BitsVAddrToPAddr(MIPS_DWORD VAddr, DWORD* PAddr, BOOL ReadOnly);
BOOL IsLastFailWriteProtectedPage();
BOOL IsLastFailInvalidPage();
