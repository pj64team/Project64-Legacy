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
#ifndef PIF_H
#define PIF_H

extern BYTE PifRom[0x7C0], *PIF_Ram;
typedef enum CIC_CHIP
{
	CIC_UNKNOWN,	// Default
	CIC_NUS_6101,	// Also 7102
	CIC_NUS_6102,	// Also 7101
	CIC_NUS_6103,	// Also 7103
	CIC_NUS_6104,	// Unused
	CIC_NUS_6105,	// Also 7105
	CIC_NUS_6106,	// Also 7106
	CIC_NUS_5167,	// N64DD IPL
	CIC_NUS_8303,	// N64DD IPL TOOL
	CIC_NUS_DDUS,	// N64DD IPL US (alternative)
	CIC_NUS_8401,	// Aleck64
	CIC_NUS_XENO	// Xeno Crisis
} CIC_CHIP;

#ifdef __cplusplus
extern "C" {
#endif
	enum CIC_CHIP GetCicChipID(BYTE* RomData);
#ifdef __cplusplus
};
#endif

int  LoadPifRom   ( BYTE country );
void PifRamWrite  ( void );
void PifRamRead   ( void );

#endif
