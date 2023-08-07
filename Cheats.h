/*
 * Project 64 Legacy - A Nintendo 64 emulator.
 *
 * (c) Copyright 2001 Zilmar, Jabo, Smiff, Gent, Witten 
 * (c) Copyright 2010 PJ64LegacyTeam 
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
#ifndef CHEATS
#define CHEATS

#define MaxGSEntries	100
#define MaxCheats		500

typedef struct {
	DWORD Command;
	WORD  Value;
} GAMESHARK_CODE;

typedef struct {
	GAMESHARK_CODE Code[MaxGSEntries];
} CHEAT_CODES;

void ChangeRomCheats	( HWND hParent );
void CloseCheatWindow	( void );
void DisableAllCheats   ( void );
void ApplyCheats		( void );
void LoadCheats			( void );
void ManageCheats		( HWND hParent );
void ApplyGSButton		(void);
int ApplyCheatEntry		(GAMESHARK_CODE * Code, BOOL Execute );

LRESULT CALLBACK Cheat_Proc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

// *** Add in Build 53
extern HWND hManageWindow;

#endif
