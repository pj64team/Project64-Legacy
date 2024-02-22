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
#include <windows.h>
#include <stdio.h>
#include "main.h"
#include "CPU.h"
#include "Debugger.h"
#include "plugin.h"
#include "EmulateAI.h"
#include "settings.h"
#include "resource.h"

DWORD EmuAI_DUMMY_STATUS;
DWORD EmuAI_Frequency, EmuAI_VICntFrame, EmuAI_BitRate;
DWORD EmuAI_Buffer[2];
int EmuAI_FrameRate;
double CountsPerByte;
DWORD LastVICntFrame;

int Start_COUNT;

// Hooks to the original functions from the Audio Plugin
void (__cdecl *AiDacrateChangedPlugin) ( int SystemType );
void (__cdecl *AiLenChangedPlugin)     ( void );
DWORD (__cdecl *AiReadLengthPlugin)    ( void );

void __cdecl EmuAI_AiLenChanged (void)
{
	EmuAI_BitRate = AI_BITRATE_REG+1;
	if (AI_LEN_REG == 0) return;

	// Determine our COUNT cycles per byte - should only need recalculation if VICntFrame changes.
	// Frequency, BitRate and FrameRate shouldn't change?
	if (LastVICntFrame != EmuAI_VICntFrame && EmuAI_Frequency > 0)
	{
		CountsPerByte = (double)(EmuAI_FrameRate * EmuAI_VICntFrame) / (double)(EmuAI_Frequency * (EmuAI_BitRate/4));
		LastVICntFrame = EmuAI_VICntFrame;
	}

	// Mini-hack used for Twisted Edge -- The plugin spec doesn't capture control register writes to enable DMA
	if (CountsPerByte == 0)	CountsPerByte = (double)(EmuAI_FrameRate * EmuAI_VICntFrame) / (double)(22050 * (16/4));

	if (EmuAI_Buffer[0] == 0) 
	{
		// Set the base
		Start_COUNT = COUNT_REGISTER;
		EmuAI_Buffer[0] = AI_LEN_REG&0x3FFF8;
		// Set our Status Register to DMA BUSY
		AI_STATUS_REG |= 0x40000000;
		// Set our timer
		ChangeTimer(AiTimer, (int)(CountsPerByte * (double)(EmuAI_Buffer[0])));
	} else if (EmuAI_Buffer[1] == 0) {
		EmuAI_Buffer[1] = AI_LEN_REG&0x3FFF8;
		// Set our Status Register to FIFO FULL
		AI_STATUS_REG |= 0x80000000;
	} else {
		AI_STATUS_REG |= 0x80000000;
		DisplayError("AI FIFO FULL But still received an AI");
	}

	// Call our plugin to process the Length register
	if (AiLenChangedPlugin != NULL) AiLenChangedPlugin();
}

DWORD __cdecl EmuAI_AiReadLength (void)
{
	DWORD SetTimer, RemainingCount;
	DWORD retVal = 0;

	SetTimer = (DWORD)(int)(CountsPerByte * (double)(EmuAI_Buffer[0]));
	RemainingCount = SetTimer - (COUNT_REGISTER - Start_COUNT);

	if (EmuAI_Buffer[0] == 0) return 0;

	retVal = (DWORD)((double)RemainingCount / CountsPerByte);
	if (retVal < 8) retVal = 8;
	
	AiReadLengthPlugin();
	return retVal & ~0x7;
}

void __cdecl EmuAI_AiDacrateChanged (int SystemType)
{
	DWORD video_clock;

	switch (SystemType) {
		case SYSTEM_NTSC:  video_clock = 48681812; break;
		case SYSTEM_PAL :  video_clock = 49656530; break;
		case SYSTEM_MPAL:  video_clock = 48628316; break;
		default         :  video_clock = 48681812;
	}
	EmuAI_Frequency = video_clock / (AI_DACRATE_REG + 1);
	if ((EmuAI_Frequency > 7000) && (EmuAI_Frequency < 9000))
		EmuAI_Frequency = 8000;
	else if ((EmuAI_Frequency > 10000) && (EmuAI_Frequency < 12000))
		EmuAI_Frequency = 11025;
	else if ((EmuAI_Frequency > 21000) && (EmuAI_Frequency < 23000))
		EmuAI_Frequency = 22050;
	else if ((EmuAI_Frequency > 31000) && (EmuAI_Frequency < 33000))
		EmuAI_Frequency = 32000;
	else if ((EmuAI_Frequency > 43000) && (EmuAI_Frequency < 45000))
		EmuAI_Frequency = 44100;
	else if ((EmuAI_Frequency > 47000) && (EmuAI_Frequency < 49000))
		EmuAI_Frequency = 48000;

	if (AiDacrateChangedPlugin != NULL) AiDacrateChangedPlugin(SystemType);
}

void EmuAI_ClearAudio()
{
	LastVICntFrame = 0;
	CountsPerByte = 0;
	EmuAI_Buffer[0] = EmuAI_Buffer[1] = 0;
	EmuAI_Frequency = 0;
	AI_STATUS_REG = 0;
	if (AI_LEN_REG > 0)
	{
		Start_COUNT = COUNT_REGISTER;
		AudioIntrReg |= MI_INTR_AI;
		AiCheckInterrupts();
	}
}

void EmuAI_InitializePluginHook()
{
	LastVICntFrame = 0;
	CountsPerByte = 0;
	EmuAI_Buffer[0] = EmuAI_Buffer[1] = 0;
	EmuAI_Frequency = 0;
	if (AiDacrateChanged == EmuAI_AiDacrateChanged) return;
	// Backup Old Values
	AiDacrateChangedPlugin = AiDacrateChanged;
	AiLenChangedPlugin = AiLenChanged;
	AiReadLengthPlugin = AiReadLength;

	// Need to override AiLenChanged, AiReadLength
	AiDacrateChanged = EmuAI_AiDacrateChanged;
	AiLenChanged = EmuAI_AiLenChanged;
	AiReadLength = EmuAI_AiReadLength;
}

void EmuAI_SetFrameRate(int frameRate)
{
	EmuAI_FrameRate = frameRate;
}

void EmuAI_SetVICountPerFrame(DWORD value)
{
	EmuAI_VICntFrame = value;
}

void EmuAI_SetNextTimer ()
{
	// Buffer Switch
	EmuAI_Buffer[0] = EmuAI_Buffer[1];
	EmuAI_Buffer[1] = 0;

	// Remove Status Full Flag
	AI_STATUS_REG &= ~0x80000000;

	if (EmuAI_Buffer[0] > 0)
	{
		AI_STATUS_REG |= 0x40000000;
		// Set our timer
		ChangeTimer(AiTimer, (int)(CountsPerByte * (double)(EmuAI_Buffer[0])));
		// Set the base
		Start_COUNT = COUNT_REGISTER;
	} else {
		// Remove DMA Busy Flag
		AI_STATUS_REG &= ~0x40000000;
		ChangeTimer(AiTimer, 0);
	}
}
