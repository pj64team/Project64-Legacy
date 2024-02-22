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
#ifndef __main_h 
#define __main_h 

#include "MemTest.h"

#ifdef __cplusplus
extern "C" {
#endif
#include <stdio.h>
#include <objbase.h>
#include "Types.h"
#include "win32Timer.h"
#include "Profiling.h"
#include "Settings Api.h"
#include "rombrowser.h"
#include "Language.h"

/********* General Defaults **********/
#if (!defined(EXTERNAL_RELEASE))
#define AppVer   "MiB64 - Build 01"
#define AppName  "MiB64 (Build 01)"
#else
#define BETA_VERSION
#define AppVer   "MiB64 - 1.0"
#ifdef BETA_VERSION
#define AppName  "MiB64 - 1.0 Beta"
#else
#define AppName  "MiB64 - 1.0 Alpha"
#endif
#endif

#define Default_AdvancedBlockLink	TRUE
#define Default_AutoStart			TRUE
#define Default_AutoSleep			TRUE
#define Default_AutoFullScreen		FALSE
#define Default_DisableRegCaching	FALSE
#define Default_RdramSize			0x800000
#define Default_UseIni				TRUE
#define Default_AutoZip				TRUE
#define Default_LimitFPS			TRUE
#define Default_AlwaysOnTop			FALSE
#define Default_BasicMode			TRUE
#define Default_RememberCheats		TRUE
#define Default_RomsToRemember		4
#define Default_RomsDirsToRemember	4
#define LinkBlocks
#define TLB_HACK

/*********** Menu Stuff **************/
#define ID_FILE_RECENT_FILE		1000
#define ID_FILE_RECENT_DIR		1100
#define ID_LANG_SELECT			2000

/************** Core *****************/
#define CPU_Default					-1
#define CPU_Interpreter				0
#define CPU_Recompiler				1
#define CPU_SyncCores				2
#define Default_CPU					CPU_Recompiler

/*********** GFX Defaults ************/
#define NoOfFrames	7
//#define CFB_READ

/******* Self modifying code *********/
#define ModCode_Default				-1
#define ModCode_None				0
#define ModCode_Cache				1
#define ModCode_ProtectedMemory		2
#define ModCode_ChangeMemory		4
#define ModCode_CheckMemoryCache	6
#define ModCode_CheckMemory2		7 // *** Add in Build 53

/********** Counter Factor ***********/
#define Default_CountPerOp			2
#define ADJUSTMENT					(0x10000000 * 0x10)

/************ Debugging **************/
#define Default_HaveDebugger		FALSE
#define Default_AutoMap				FALSE
#define Default_ShowUnhandledMemory	FALSE
#define Default_ShowTLBMisses		FALSE
#define Default_ShowDlistCount		FALSE
#define Default_ShowCompileMemory	FALSE
#define Default_ShowPifRamErrors	FALSE
#define Default_SelfModCheck		ModCode_CheckMemory2
//#define Interpreter_StackTest		

/************ Profiling **************/
#define Default_ShowCPUPer			FALSE
#define Default_ProfilingOn			FALSE
#define Default_IndividualBlock		FALSE

/********** Rom Browser **************/
#define Default_UseRB				TRUE
#define Default_Recursion			TRUE
#define Default_RomStatus			"Unknown"

/************* Logging ***************/
//#define Log_x86Code

/********* Global Variables **********/
extern LARGE_INTEGER Frequency, Frames[NoOfFrames], LastFrame;
extern BOOL HaveDebugger, ShowDebugMessages, AutoLoadMapFile, ShowUnhandledMemory, ShowTLBMisses, 
	ShowDListAListCount, ShowCompMem, Profiling, IndividualBlock, AutoStart, 
	AutoSleep, DisableRegCaching, UseTlb, UseLinking, RomBrowser,
	IgnoreMove, Recursion, ShowPifRamErrors, LimitFPS, ShowCPUPer, AutoZip, 
	AutoFullScreen, SystemABL, AlwaysOnTop, BasicMode, DelaySI, RememberCheats,AudioSignal, EmulateAI;
extern unsigned int CurrentFrame;
extern int CPU_Type, SystemCPU_Type, SelfModCheck, SystemSelfModCheck, RomsToRemember, RomDirsToRemember;
extern HWND hMainWindow, hStatusWnd;
extern char CurrentSave[256];
extern HMENU hMainMenu;
extern HINSTANCE hInst;
extern DWORD ModVI;

/******** Function Prototype *********/
DWORD AsciiToHex          ( char * HexValue );
QWORD AsciiToHex64        ( char* HexValue );
void AlwaysOnTopWindow    ( HWND hWnd );
void  __cdecl DisplayError       ( char * Message, ... );
void  __cdecl DisplayErrorFatal  ( char * Message, ... );
void ChangeWinSize        ( HWND hWnd, long width, long height, HWND hStatusBar );
void  DisplayFPS          ( void );
int   GetStoredWinPos     ( char * WinName, DWORD * X, DWORD * Y );
int   GetStoredWinSize    ( char * WinName, DWORD * Width, DWORD * Height );
void  LoadSettings        ( void );
void  MenuSetText         ( HMENU hMenu, int MenuPos, char * Title, char * ShotCut );
void  RegisterExtension   ( char * Extension, BOOL RegisterWithPj64 );
void  SetCurrentSaveState ( HWND hWnd, int State);
void  SetupMenu           ( HWND hWnd );
//void  SetupMenuTitle      ( HMENU hMenu, int MenuPos, char * ShotCut, char * Title, char * Language, char *  LangFile );
void  StoreCurrentWinPos  ( char * WinName, HWND hWnd );
void  StoreCurrentWinSize ( char * WinName, HWND hWnd );
BOOL  TestExtensionRegistered ( char * Extension );

void AboutBox (void);

#ifdef __cplusplus
}
#endif
#endif

// Building this in Visual Studio 2005 and higher allows us to dump the external manifest file.
// This line below allows the use of "XP Style" buttons and other stuff on the gui.
//#ifdef EXTERNAL_RELEASE
#pragma comment(linker,"\"/manifestdependency:type='win32' \
name='Microsoft.Windows.Common-Controls' version='6.0.0.0' \
processorArchitecture='*' publicKeyToken='6595b64144ccf1df' language='*'\"")
//#endif
