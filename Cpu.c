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
#include <commctrl.h>
#include <stdio.h>
#include "main.h"
#include "Compression/unzip.h"
#include "Compression/zip.h"
#include "cpu.h"
#include "cheats.h"
#include "debugger.h"
#include "plugin.h"
#include "EmulateAI.h"
#include "resource.h" 
#include "CheatSearch.h"
#include "RomTools_Common.h"

int CPOAdjust = 0;
int NextInstruction, ManualPaused, CPU_Paused, CountPerOp;
MIPS_DWORD JumpToLocation;
char SaveAsFileName[MAX_PATH], LoadFileName[MAX_PATH];
int DlistCount, AlistCount, CurrentSaveSlot;
enum SaveType SaveUsing;
CPU_ACTION CPU_Action;
SYSTEM_TIMERS Timers;
HANDLE hPauseMutex;
OPCODE Opcode;
HANDLE hCPU;
BOOL inFullScreen, CPURunning, SPHack;
DWORD MemoryStack;
static unsigned int firstFrameWithInterruptsDisabled = 0;

#ifdef CFB_READ
DWORD CFBStart = 0, CFBEnd = 0;
#endif

#ifdef Interpreter_StackTest
DWORD StackValue;
#endif

#ifdef CFB_READ
void __cdecl SetFrameBuffer (DWORD Address, DWORD Length) {
	DWORD NewStart, NewLength, OldProtect;

	NewStart = Address;
	NewLength = Length;
	
	if (CFBStart != 0) {
		VirtualProtect(N64MEM + CFBStart,CFBEnd - CFBStart,PAGE_READWRITE,&OldProtect);
	}
	if (Length == 0) {
		CFBStart = 0; 
		CFBEnd   = 0;
		return;
	}
	CFBStart = Address & ~0xFFF;
	CFBEnd = ((CFBStart + Length + 0xFFC) & ~0xFFF) - 1;
	VirtualProtect(N64MEM + CFBStart,CFBEnd - CFBStart,PAGE_READONLY,&OldProtect);
}
#endif

char *TimeName[MaxTimers] = { "CompareTimer","SiTimer","PiTimer","ViTimer" };

void InitiliazeCPUFlags (void) {
	inFullScreen = FALSE;
	CPURunning   = FALSE;
	CurrentSaveSlot = ID_CURRENTSAVE_DEFAULT;
	SPHack       = FALSE;
}

void ChangeCompareTimer(void) {
	DWORD NextCompare = COMPARE_REGISTER - COUNT_REGISTER;
	if ((NextCompare & 0x80000000) != 0) {  NextCompare = 0x7FFFFFFF; }
	if (NextCompare == 0) { NextCompare = 0x1; }	
	ChangeTimer(CompareTimer,NextCompare);
}

void ChangeTimer(int Type, int Value) {
	int TimeSinceCurrentEventInserted = Timers.NextTimer[Timers.CurrentTimerType] - Timers.Timer;

	for (int count = 0; count < MaxTimers; count++) {
		if (count == Timers.CurrentTimerType || count == Type) continue;
		if (!Timers.Active[count]) { continue; }
		if (!(count == CompareTimer && Timers.NextTimer[count] == 0x7FFFFFFF)) {
			Timers.NextTimer[count] -= TimeSinceCurrentEventInserted;
		}
	}

	if (Value == 0) { 
		Timers.NextTimer[Type] = 0;
		Timers.Active[Type] = FALSE;
	}
	else {
		Timers.NextTimer[Type] = Value;
		Timers.Active[Type] = TRUE;
	}

	if (Timers.CurrentTimerType != Type) {
		if (!(Timers.CurrentTimerType == CompareTimer && Timers.NextTimer[Timers.CurrentTimerType] == 0x7FFFFFFF)) {
			if (Timers.Active[Timers.CurrentTimerType]) {
				Timers.NextTimer[Timers.CurrentTimerType] = Timers.Timer;
			}
		}
	}
	CheckTimer();
}

void CheckTimer (void) {
	int count;

	if (Timers.NextTimer[CompareTimer] == 0x7FFFFFFF) {
		DWORD NextCompare = COMPARE_REGISTER - COUNT_REGISTER;
		if ((NextCompare & 0x80000000) == 0 && NextCompare != 0x7FFFFFFF) {
			Timers.NextTimer[CompareTimer] = NextCompare;
		}
	}

	Timers.CurrentTimerType = -1;
	Timers.Timer = 0x7FFFFFFF;
	for (count = 0; count < MaxTimers; count++) {
		if (!Timers.Active[count]) { continue; }
		if (Timers.NextTimer[count] >= Timers.Timer) { continue; }
		Timers.Timer = Timers.NextTimer[count];
		Timers.CurrentTimerType = count;
	}

	if (Timers.CurrentTimerType == -1) {
		DisplayError("No active timers ???\nEmulation Stoped");
		ExitThread(0);
	}
}

void CloseCpu (void) {
	DWORD ExitCode, OldProtect;
	
	if (CPU_Action.CloseCPU || !CPURunning || hCPU == NULL) { return; }

	// FIXME: Don't race global state like `CPU_Action` and `CPURunning` across multiple threads!!!
	CPU_Action.CloseCPU = TRUE;
	CPU_Action.Stepping = FALSE;
	CPU_Action.DoInterrupt = FALSE;
	CPU_Action.CheckInterrupts = FALSE;
	CPU_Action.DoSomething = TRUE;

	// The memory barrier will ensure that the writes above occur before the CPU is released from the stepping mode.
	MemoryBarrier();
	PulseEvent(CPU_Action.hStepping);

	ManualPaused = FALSE;
	if (CPU_Paused) { PauseCpu(); }

	{
		BOOL Temp = AlwaysOnTop;
		AlwaysOnTop = FALSE;
		AlwaysOnTopWindow(hMainWindow);
		AlwaysOnTop = Temp;
	}

	LARGE_INTEGER deadline = { 0 };
	QueryPerformanceCounter(&deadline);
	deadline.QuadPart += Frequency.QuadPart * 2; // Deadline is 2 seconds from now
	do {
		ExitCode = WaitForSingleObject(hCPU, 16);
		if (ExitCode == WAIT_OBJECT_0) {
			hCPU = NULL;
		} else if (ExitCode == WAIT_TIMEOUT) {
			// Process queued messages while waiting for the CPU thread to stop. This is necessary
			// because the CPU thread may be waiting for the GUI thread to do something.
			//
			// One example is the TimerDone => RefreshScreen => DisplayFPS path, which is called by
			// the recompiler and intepreter.
			//
			// This fixes the deadlock that was blamed on plugins. Hooray!
			MSG msg;
			while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) != 0) {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}

		LARGE_INTEGER now = { 0 };
		QueryPerformanceCounter(&now);
		if (now.QuadPart >= deadline.QuadPart && hCPU != NULL) {
			// We're out of time! The CPU thread cannot make any further progress
			// since the mutex has been abandoned or the wait failed for another reason.
			// There is nothing else we can do about this.
			DisplayError("Emulation thread failed to stop\nReport this if you can reproduce reliably");

			// This is a last resort when the CPU thread refuses to gracefully exit.
			// Calling this function WILL cause problems!
			// SEE: https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-terminatethread#remarks
			TerminateThread(hCPU, 0);

			hCPU = NULL;
		}
	} while (hCPU != NULL);

#ifdef Log_x86Code
	Stop_x86_Log();
#endif

	CPURunning = FALSE;
	VirtualProtect(N64MEM,RdramSize,PAGE_READWRITE,&OldProtect);
	VirtualProtect(N64MEM + 0x04000000,0x2000,PAGE_READWRITE,&OldProtect);
	Timer_Stop();
	SetCurrentSaveState(hMainWindow,ID_CURRENTSAVE_DEFAULT);
	CloseEeprom();
	CloseMempak();
	CloseSram();
	FreeSyncMemory();
	if (GfxRomClosed != NULL) { GfxRomClosed(); }
	if (AiRomClosed != NULL) { AiRomClosed(); }
	if (RSPRomClosed != NULL) { RSPRomClosed(); }
	if (ContRomClosed != NULL) { ContRomClosed(); }
	if (Profiling) { GenerateTimerResults(); }
	CloseHandle(CPU_Action.hStepping);
	SendMessage( hStatusWnd, SB_SETTEXT, 0, (LPARAM)GS(MSG_EMULATION_ENDED) );
}

int DelaySlotEffectsCompare (MIPS_DWORD PC, DWORD Reg1, DWORD Reg2) {
	OPCODE Command;
	MIPS_DWORD DelaySlotAddress;
	DelaySlotAddress.UDW = PC.UDW + 4;

	if (!r4300i_LW_VAddr_NonCPU(DelaySlotAddress, &Command.Hex)) {
		if (ShowDebugMessages) {
			char msg[100];
			sprintf(msg, "%s\r\nPC: %016llX", "Failed to load word 2" , PC.UDW + 4);
			DisplayError(msg);
		}
		Command.Hex = 0x0;
//		ExitThread(0);
//		return TRUE;
	}

	if (SelfModCheck == ModCode_ChangeMemory) {
		if ( (Command.Hex >> 16) == 0x7C7C) {
			Command.Hex = OrigMem[(Command.Hex & 0xFFFF)].OriginalValue;
		}
	}

	switch (Command.BRANCH.op) {
	case R4300i_SPECIAL:
		switch (Command.REG.funct) {
		case R4300i_SPECIAL_SLL:
		case R4300i_SPECIAL_SRL:
		case R4300i_SPECIAL_SRA:
		case R4300i_SPECIAL_SLLV:
		case R4300i_SPECIAL_SRLV:
		case R4300i_SPECIAL_SRAV:
		case R4300i_SPECIAL_MFHI:
		case R4300i_SPECIAL_MTHI:
		case R4300i_SPECIAL_MFLO:
		case R4300i_SPECIAL_MTLO:
		case R4300i_SPECIAL_DSLLV:
		case R4300i_SPECIAL_DSRLV:
		case R4300i_SPECIAL_DSRAV:
		case R4300i_SPECIAL_ADD:
		case R4300i_SPECIAL_ADDU:
		case R4300i_SPECIAL_SUB:
		case R4300i_SPECIAL_SUBU:
		case R4300i_SPECIAL_AND:
		case R4300i_SPECIAL_OR:
		case R4300i_SPECIAL_XOR:
		case R4300i_SPECIAL_NOR:
		case R4300i_SPECIAL_SLT:
		case R4300i_SPECIAL_SLTU:
		case R4300i_SPECIAL_DADD:
		case R4300i_SPECIAL_DADDU:
		case R4300i_SPECIAL_DSUB:
		case R4300i_SPECIAL_DSUBU:
		case R4300i_SPECIAL_DSLL:
		case R4300i_SPECIAL_DSRL:
		case R4300i_SPECIAL_DSRA:
		case R4300i_SPECIAL_DSLL32:
		case R4300i_SPECIAL_DSRL32:
		case R4300i_SPECIAL_DSRA32:
			if (Command.REG.rd == 0) { return FALSE; }
			if (Command.REG.rd == Reg1) { return TRUE; }
			if (Command.REG.rd == Reg2) { return TRUE; }
			break;
		case R4300i_SPECIAL_MULT:
		case R4300i_SPECIAL_MULTU:
		case R4300i_SPECIAL_DIV:
		case R4300i_SPECIAL_DIVU:
		case R4300i_SPECIAL_DMULT:
		case R4300i_SPECIAL_DMULTU:
		case R4300i_SPECIAL_DDIV:
		case R4300i_SPECIAL_DDIVU:
			break;
		default:
			if (ShowDebugMessages)
				DisplayError("Does %s effect Delay slot at %llX?",R4300iOpcodeName(Command.Hex,DelaySlotAddress), PC);
			return TRUE;
		}
		break;
	case R4300i_CP0:
		switch (Command.BRANCH.rs) {
		case R4300i_COP0_MT: break;
		case R4300i_COP0_MF:
			if (Command.BRANCH.rt == 0) { return FALSE; }
			if (Command.BRANCH.rt == Reg1) { return TRUE; }
			if (Command.BRANCH.rt == Reg2) { return TRUE; }
			break;
		default:
			if ( (Command.BRANCH.rs & 0x10 ) != 0 ) {
				switch( Opcode.REG.funct ) {
				case R4300i_COP0_CO_TLBR: break;
				case R4300i_COP0_CO_TLBWI: break;
				case R4300i_COP0_CO_TLBWR: break;
				case R4300i_COP0_CO_TLBP: break;
				default: 
					if (ShowDebugMessages)
						DisplayError("Does %s effect Delay slot at %llX?\n6",R4300iOpcodeName(Command.Hex, DelaySlotAddress), PC);
					return TRUE;
				}
			} else {
				if (ShowDebugMessages)
					DisplayError("Does %s effect Delay slot at %X?\n7",R4300iOpcodeName(Command.Hex, DelaySlotAddress), PC);
				return TRUE;
			}
		}
		break;
	case R4300i_CP1:
		switch (Command.FP.fmt) {
		case R4300i_COP1_MF:
			if (Command.BRANCH.rt == 0) { return FALSE; }
			if (Command.BRANCH.rt == Reg1) { return TRUE; }
			if (Command.BRANCH.rt == Reg2) { return TRUE; }
			break;
		case R4300i_COP1_CF: break;
		case R4300i_COP1_MT: break;
		case R4300i_COP1_CT: break;
		case R4300i_COP1_S: break;
		case R4300i_COP1_D: break;
		case R4300i_COP1_W: break;
		case R4300i_COP1_L: break;
		default:
			if (ShowDebugMessages)
				DisplayError("Does %s effect Delay slot at %llX?",R4300iOpcodeName(Command.Hex, DelaySlotAddress), PC);
			return TRUE;
		}
		break;
	case R4300i_ANDI:
	case R4300i_ORI:
	case R4300i_XORI:
	case R4300i_LUI:
	case R4300i_ADDI:
	case R4300i_ADDIU:
	case R4300i_SLTI:
	case R4300i_SLTIU:
	case R4300i_DADDI:
	case R4300i_DADDIU:
	case R4300i_LB:
	case R4300i_LH:
	case R4300i_LW:
	case R4300i_LWL:
	case R4300i_LWR:
	case R4300i_LDL:
	case R4300i_LDR:
	case R4300i_LBU:
	case R4300i_LHU:
	case R4300i_LD:
	case R4300i_LWC1:
	case R4300i_LDC1:
		if (Command.BRANCH.rt == 0) { return FALSE; }
		if (Command.BRANCH.rt == Reg1) { return TRUE; }
		if (Command.BRANCH.rt == Reg2) { return TRUE; }
		break;
	case R4300i_CACHE: break;
	case R4300i_SB: break;
	case R4300i_SH: break;
	case R4300i_SW: break;
	case R4300i_SWR: break;
	case R4300i_SWL: break;
	case R4300i_SWC1: break;
	case R4300i_SDC1: break;
	case R4300i_SD: break;
	default:
		if (ShowDebugMessages)
			DisplayError("Does %s effect Delay slot at %llX?",R4300iOpcodeName(Command.Hex, DelaySlotAddress), PC);
		return TRUE;
	}
	return FALSE;
}

int DelaySlotEffectsJump (MIPS_DWORD JumpPC) {
	OPCODE Command;
	MIPS_DWORD Address;
	Address = JumpPC;

	if (!r4300i_LW_VAddr_NonCPU(Address, &Command.Hex)) { return TRUE; }
	if (SelfModCheck == ModCode_ChangeMemory) {
		if ( (Command.Hex >> 16) == 0x7C7C) {
			Command.Hex = OrigMem[(Command.Hex & 0xFFFF)].OriginalValue;
		}
	}

	switch (Command.BRANCH.op) {
	case R4300i_SPECIAL:
		switch (Command.REG.funct) {
		case R4300i_SPECIAL_JR:	return DelaySlotEffectsCompare(JumpPC,Command.BRANCH.rs,0);
		case R4300i_SPECIAL_JALR: return DelaySlotEffectsCompare(JumpPC,Command.BRANCH.rs,31);
		}
		break;
	case R4300i_REGIMM:
		switch (Command.BRANCH.rt) {
		case R4300i_REGIMM_BLTZ:
		case R4300i_REGIMM_BGEZ:
		case R4300i_REGIMM_BLTZL:
		case R4300i_REGIMM_BGEZL:
		case R4300i_REGIMM_BLTZAL:
		case R4300i_REGIMM_BGEZAL:
			return DelaySlotEffectsCompare(JumpPC,Command.BRANCH.rs,0);
		}
		break;
	case R4300i_JAL: 
	case R4300i_SPECIAL_JALR: return DelaySlotEffectsCompare(JumpPC,31,0); break;
	case R4300i_J: return FALSE;
	case R4300i_BEQ: 
	case R4300i_BNE: 
	case R4300i_BLEZ: 
	case R4300i_BGTZ: 
		return DelaySlotEffectsCompare(JumpPC,Command.BRANCH.rs,Command.BRANCH.rt);
	case R4300i_CP1:
		switch (Command.FP.fmt) {
		case R4300i_COP1_BC:
			switch (Command.FP.ft) {
			case R4300i_COP1_BC_BCF:
			case R4300i_COP1_BC_BCT:
			case R4300i_COP1_BC_BCFL:
			case R4300i_COP1_BC_BCTL:
				{
					int EffectDelaySlot;
					OPCODE NewCommand;
					MIPS_DWORD AddressAfterJumpDest;
					AddressAfterJumpDest.UDW = JumpPC.UDW + 4;

					if (!r4300i_LW_VAddr_NonCPU(AddressAfterJumpDest, &NewCommand.Hex)) { return TRUE; }
					
					EffectDelaySlot = FALSE;
					if (NewCommand.BRANCH.op == R4300i_CP1) {
						if (NewCommand.FP.fmt == R4300i_COP1_S && (NewCommand.REG.funct & 0x30) == 0x30 ) {
							EffectDelaySlot = TRUE;
						} 
						if (NewCommand.FP.fmt == R4300i_COP1_D && (NewCommand.REG.funct & 0x30) == 0x30 ) {
							EffectDelaySlot = TRUE;
						} 
					}
					return EffectDelaySlot;
				} 
				break;
			}
			break;
		}
		break;
	case R4300i_BEQL: 
	case R4300i_BNEL: 
	case R4300i_BLEZL: 
	case R4300i_BGTZL: 
		return DelaySlotEffectsCompare(JumpPC,Command.BRANCH.rs,Command.BRANCH.rt);
	}
	return TRUE;
}

void DoSomething ( void ) {
	if (CPU_Action.CloseCPU) { 
		CoUninitialize();
		ExitThread(0); 
	}

	if (CPU_Action.CheckInterrupts) {
		CPU_Action.CheckInterrupts = FALSE;
		CheckInterrupts();
	}

	if (CPU_Action.DoInterrupt) {
		CPU_Action.DoInterrupt = FALSE;
		DoIntrException(FALSE);
		if (CPU_Type == CPU_SyncCores) {
			SyncRegisters.MI[2] = Registers.MI[2];
			SwitchSyncRegisters();
			DoIntrException(FALSE);
			SwitchSyncRegisters();
		}
	}

	if (CPU_Action.ChangeWindow) {
		CPU_Action.ChangeWindow = FALSE;
		CPU_Paused = TRUE;
		SendMessage(hMainWindow,WM_COMMAND,ID_OPTIONS_FULLSCREEN,0);
		CPU_Paused = FALSE;
	}

	if (CPU_Action.Pause) {
		WaitForSingleObject(hPauseMutex, INFINITE);
		if (CPU_Action.Pause) {
			HMENU hMenu = GetMenu(hMainWindow);
			HMENU hSubMenu = GetSubMenu(hMenu,1);
			MenuSetText(hSubMenu, 1, GS(MENU_RESUME),"F2");

			CurrentFrame = 0;
			CurrentPercent = 0;
			CPU_Paused = TRUE;
			CPU_Action.Pause = FALSE;
			ReleaseMutex(hPauseMutex);
			SendMessage( hStatusWnd, SB_SETTEXT, 0, (LPARAM)GS(MSG_CPU_PAUSED));
			DisplayFPS ();
			if (DrawScreen != NULL) { DrawScreen(); }
			WaitForSingleObject(hPauseMutex, INFINITE);
			if (CPU_Paused) { 
				ReleaseMutex(hPauseMutex);
				SuspendThread(hCPU); 
			} else {
				ReleaseMutex(hPauseMutex);
			}
		} else {
			ReleaseMutex(hPauseMutex);
		}
	}
	CPU_Action.DoSomething = FALSE;
	
	if (CPU_Action.SaveState && (SP_STATUS_REG & SP_STATUS_HALT) != 0) {
		//test if allowed
		CPU_Action.SaveState = FALSE;
		if (!Machine_SaveState()) {
			CPU_Action.SaveState = TRUE;
			CPU_Action.DoSomething = TRUE;
		}
	}

	if (CPU_Action.RestoreState) {
		CPU_Action.RestoreState = FALSE;
		Machine_LoadState();
	}

	if (CPU_Action.DoInterrupt == TRUE) {
		CPU_Action.DoSomething = TRUE;
	}
}

void InPermLoop (void) {
	// *** Changed ***/
	if (CPU_Action.DoInterrupt) { return; }

	//Timers.Timer -= 5;
	//COUNT_REGISTER +=5;
	//if (CPU_Type == CPU_SyncCores) { SyncRegisters.CP0[9] +=5; }

	/* Interrupts enabled */
	if (( STATUS_REGISTER & STATUS_IE  ) == 0 ) { goto InterruptsDisabled; }
	if (( STATUS_REGISTER & STATUS_EXL ) != 0 ) { goto InterruptsDisabled; }
	if (( STATUS_REGISTER & STATUS_ERL ) != 0 ) { goto InterruptsDisabled; }
	if (( STATUS_REGISTER & 0xFF00) == 0) { goto InterruptsDisabled; }
	
	/* check sound playing */
	//if (AiReadLength() != 0) { return; }

	/* check RSP running */
	/* check RDP running */
	if (Timers.Timer > 0) {
		COUNT_REGISTER += Timers.Timer + 1;
		if (CPU_Type == CPU_SyncCores) { SyncRegisters.CP0[9].UW[0] += Timers.Timer + 1; }
		Timers.Timer = -1;
	}
	return;

InterruptsDisabled:
	RefreshScreen();
	if (firstFrameWithInterruptsDisabled == 0) {
		firstFrameWithInterruptsDisabled = CurrentFrame;
		return;
	}
	else if ((CurrentFrame - firstFrameWithInterruptsDisabled) < 1) {
		return;
	}
	else {
		firstFrameWithInterruptsDisabled = 0;
	}

	CurrentFrame = 0;
	CurrentPercent = 0;
	DisplayFPS();
	DisplayError(GS(MSG_PERM_LOOP));
	ExitThread(0);
}

BOOL Machine_LoadState(void) {
	char Directory[255], FileName[255], ZipFile[255], LoadHeader[64], String[100];
	char drive[_MAX_DRIVE] ,dir[_MAX_DIR], ext[_MAX_EXT];
	DWORD dwRead, Value, count, SaveRDRAMSize, formatVersion;
	BOOL LoadedZipFile = FALSE;
	HANDLE hSaveFile;
	unzFile file;

	if (strlen(LoadFileName) == 0) {
		Settings_GetDirectory(InstantSaveDir, Directory, sizeof(Directory));
		sprintf(FileName,"%s%s",Directory,CurrentSave);
		sprintf(ZipFile,"%s.zip",FileName);
	} else {
		strcpy(FileName,LoadFileName);
		strcpy(ZipFile,LoadFileName);
	}

	file = unzOpen(ZipFile);
	if (file != NULL) {
	    unz_file_info info;
		char zname[132];
		int port = 0;

		port = unzGoToFirstFile(file);
		while (port == UNZ_OK && LoadedZipFile == FALSE) {
			unzGetCurrentFileInfo(file, &info, zname, 128, NULL,0, NULL,0);
		    if (unzLocateFile(file, zname, 1) != UNZ_OK ) {
				unzClose(file);
				port = -1;
				continue;
			}
			if( unzOpenCurrentFile(file) != UNZ_OK ) {
				unzClose(file);
				port = -1;
				continue;
			}
			unzReadCurrentFile(file,&formatVersion,4);
			if (formatVersion != SaveStateFormat_ORIGINAL && formatVersion != SaveStateFormat_2023_1) {
				unzCloseCurrentFile(file);
				continue; 
			}
			if (formatVersion == SaveStateFormat_ORIGINAL) {
				if (!inFullScreen) {
					int result = MessageBox(hMainWindow, GS(MSG_SAVESTATE_OLDFORMAT), GS(MSG_MSGBOX_TITLE),
						MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2);
					if (result == IDNO) {
						return FALSE;
					}
				}
			}
			unzReadCurrentFile(file,&SaveRDRAMSize,sizeof(SaveRDRAMSize));	
			unzReadCurrentFile(file,LoadHeader,0x40);			
			//Check header
			if (memcmp(LoadHeader,RomHeader,0x40) != 0) {
				int result;

				if (inFullScreen) { return FALSE; }
				result = MessageBox(hMainWindow,GS(MSG_SAVE_STATE_HEADER),GS(MSG_MSGBOX_TITLE),
					MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2);
				if (result == IDNO) { return FALSE; }
			}

			if (CPU_Type == CPU_SyncCores) {
				DWORD OldProtect;

				VirtualProtect(N64MEM,RdramSize,PAGE_READWRITE,&OldProtect);
				VirtualProtect(N64MEM + 0x04000000,0x2000,PAGE_READWRITE,&OldProtect);
				VirtualProtect(SyncMemory,RdramSize,PAGE_READWRITE,&OldProtect);
				VirtualProtect(SyncMemory + 0x04000000,0x2000,PAGE_READWRITE,&OldProtect);	
			}
			if (CPU_Type != CPU_Interpreter) {
				ResetRecompCode(); 
			}

			Timers.CurrentTimerType = -1;
			Timers.Timer = 0;
			for (count = 0; count < MaxTimers; count ++) { Timers.Active[count] = FALSE; }

			//fix rdram size
			if (SaveRDRAMSize != RdramSize) {
				if (RdramSize == 0x400000) { 
					if (VirtualAlloc(N64MEM + 0x400000, 0x400000, MEM_COMMIT, PAGE_READWRITE)==NULL) {
						if (HaveDebugger)
							DisplayError("Failed to Extend memory to 8mb");
						else
							DisplayError(GS(MSG_MEM_ALLOC_ERROR));
						ExitThread(0);
					}
					if (VirtualAlloc((BYTE *)JumpTable + 0x400000, 0x400000, MEM_COMMIT, PAGE_READWRITE)==NULL) {
						if (HaveDebugger)
							DisplayError("Failed to Extend Jump Table to 8mb");
						else
							DisplayError(GS(MSG_MEM_ALLOC_ERROR));
						ExitThread(0);
					}
					if (VirtualAlloc((BYTE *)DelaySlotTable + (0x400000 >> 0xA), (0x400000 >> 0xA), MEM_COMMIT, PAGE_READWRITE)==NULL) {
						if (HaveDebugger)
							DisplayError("Failed to Extend Delay Slot Table to 8mb");
						else
							DisplayError(GS(MSG_MEM_ALLOC_ERROR));
						ExitThread(0);
					}
				} else {
					VirtualFree(N64MEM + 0x400000, 0x400000,MEM_DECOMMIT);
					VirtualFree((BYTE *)JumpTable + 0x400000, 0x400000,MEM_DECOMMIT);
					VirtualFree((BYTE *)DelaySlotTable + (0x400000 >> 0xA), (0x400000 >> 0xA),MEM_DECOMMIT);
				}
			}
			RdramSize = SaveRDRAMSize;
			unzReadCurrentFile(file,&Value,sizeof(Value));
			ChangeTimer(ViTimer,Value);
			if (formatVersion == SaveStateFormat_ORIGINAL) {
				unzReadCurrentFile(file, &PROGRAM_COUNTER.UW[0], sizeof(PROGRAM_COUNTER.UW[0]));
				PROGRAM_COUNTER.DW = PROGRAM_COUNTER.W[0];
			}
			else {
				unzReadCurrentFile(file, &PROGRAM_COUNTER, sizeof(PROGRAM_COUNTER));
			}
			unzReadCurrentFile(file,GPR,sizeof(_int64)*32);
			unzReadCurrentFile(file,FPR,sizeof(_int64)*32);
			if (formatVersion == SaveStateFormat_ORIGINAL) {
				for (int i = 0; i < 32; ++i) {
					unzReadCurrentFile(file, &CP0[i].UW[0], sizeof(DWORD));
					CP0[i].UW[1] = 0;
				}
			}
			else {
				unzReadCurrentFile(file, CP0, sizeof(QWORD) * 32);
			}
			UpdateCPUMode();
			
			unzReadCurrentFile(file,FPCR,sizeof(DWORD)*32);
			unzReadCurrentFile(file,&HI,sizeof(_int64));
			unzReadCurrentFile(file,&LO,sizeof(_int64));
			if (formatVersion == SaveStateFormat_ORIGINAL) {
				unzReadCurrentFile(file, (*RegRDRAM)[0], sizeof(DWORD) * 10);
				for (int i = 1; i < 4; ++i) {
					memset((*RegRDRAM)[i], 0, sizeof(Registers.RDRAM[i]));
				}
			}
			else {
				for (int i = 0; i < 4; ++i) {
					unzReadCurrentFile(file, (*RegRDRAM)[i], sizeof(DWORD) * 10);
				}
			}
			unzReadCurrentFile(file,RegSP,sizeof(DWORD)*10);
			unzReadCurrentFile(file,RegDPC,sizeof(DWORD)*10);
			unzReadCurrentFile(file,RegMI,sizeof(DWORD)*4);
			unzReadCurrentFile(file,RegVI,sizeof(DWORD)*14);
			unzReadCurrentFile(file,RegAI,sizeof(DWORD)*6);
			unzReadCurrentFile(file,RegPI,sizeof(DWORD)*13);
			unzReadCurrentFile(file,RegRI,sizeof(DWORD)*8);
			unzReadCurrentFile(file,RegSI,sizeof(DWORD)*4);
			unzReadCurrentFile(file,tlb,sizeof(TLB)*32);
			unzReadCurrentFile(file,PIF_Ram,0x40);
			unzReadCurrentFile(file,RDRAM,RdramSize);
			unzReadCurrentFile(file,DMEM,0x1000);
			unzReadCurrentFile(file,IMEM,0x1000);

			SetFpuLocations();
			CheckRdramStatus();

			// Specific data introducted in 2023.1
			unzReadCurrentFile(file, &lastUnusedCOP0Register, sizeof(int));
			unzReadCurrentFile(file, &WrittenToRom, sizeof(BOOL));
			unzReadCurrentFile(file, &WrittenToRomCount, sizeof(DWORD));
			unzReadCurrentFile(file, &WroteToRom, sizeof(DWORD));
			unzReadCurrentFile(file, ISViewerBuffer, sizeof(ISViewerBuffer));
			unzReadCurrentFile(file, &cop2LatchedValue.UDW, sizeof(QWORD));

			unzCloseCurrentFile(file);
			unzClose(file);
			LoadedZipFile = TRUE;
			_splitpath( ZipFile, drive, dir, ZipFile, ext );
			sprintf(FileName,"%s%s",ZipFile,ext);
		}
	}
	if (!LoadedZipFile) {
		
		hSaveFile = CreateFile(FileName,GENERIC_WRITE | GENERIC_READ, FILE_SHARE_READ,NULL,OPEN_EXISTING,
			FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS, NULL);
		if (hSaveFile == INVALID_HANDLE_VALUE) {
			_splitpath( FileName, drive, dir, ZipFile, ext );
			sprintf(String,"%s %s%s",GS(MSG_UNABLED_LOAD_STATE),ZipFile,ext);
			SendMessage( hStatusWnd, SB_SETTEXT, 0, (LPARAM)String );
			return FALSE;
		}	
		SetFilePointer(hSaveFile,0,NULL,FILE_BEGIN);	
		ReadFile( hSaveFile,&formatVersion,sizeof(formatVersion),&dwRead,NULL);
		if (formatVersion != SaveStateFormat_ORIGINAL && formatVersion != SaveStateFormat_2023_1) {
			return FALSE;
		}
		if (formatVersion == SaveStateFormat_ORIGINAL) {
			if (!inFullScreen) {
				int result = MessageBox(hMainWindow, GS(MSG_SAVESTATE_OLDFORMAT), GS(MSG_MSGBOX_TITLE),
					MB_YESNO | MB_ICONQUESTION | MB_DEFBUTTON2);
				if (result == IDNO) {
					return FALSE;
				}
			}
		}
		ReadFile( hSaveFile,&SaveRDRAMSize,sizeof(SaveRDRAMSize),&dwRead,NULL);	
		ReadFile( hSaveFile,LoadHeader,0x40,&dwRead,NULL);	

		//Check header
		if (memcmp(LoadHeader,ROM,0x40) != 0) {
			int result;

			if (inFullScreen) { return FALSE; }
			result = MessageBox(hMainWindow,GS(MSG_SAVE_STATE_HEADER),GS(MSG_MSGBOX_TITLE),
				MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2);
			if (result == IDNO) { return FALSE; }
		}

		if (CPU_Type == CPU_SyncCores) {
			DWORD OldProtect;

			VirtualProtect(N64MEM,RdramSize,PAGE_READWRITE,&OldProtect);
			VirtualProtect(N64MEM + 0x04000000,0x2000,PAGE_READWRITE,&OldProtect);
			VirtualProtect(SyncMemory,RdramSize,PAGE_READWRITE,&OldProtect);
			VirtualProtect(SyncMemory + 0x04000000,0x2000,PAGE_READWRITE,&OldProtect);	
		}
		if (CPU_Type != CPU_Interpreter) { 
			ResetRecompCode(); 
		}

		Timers.CurrentTimerType = -1;
		Timers.Timer = 0;
		for (count = 0; count < MaxTimers; count ++) { Timers.Active[count] = FALSE; }

		//fix rdram size
		if (SaveRDRAMSize != RdramSize) {
			if (RdramSize == 0x400000) { 
				if (VirtualAlloc(N64MEM + 0x400000, 0x400000, MEM_COMMIT, PAGE_READWRITE)==NULL) {
					if (HaveDebugger)
						DisplayError("Failed to Extend memory to 8mb");
					else
						DisplayError(GS(MSG_MEM_ALLOC_ERROR));
					ExitThread(0);
				}
				if (VirtualAlloc((BYTE *)JumpTable + 0x400000, 0x400000, MEM_COMMIT, PAGE_READWRITE)==NULL) {
					if (HaveDebugger)
						DisplayError("Failed to Extend Jump Table to 8mb");
					else
						DisplayError(GS(MSG_MEM_ALLOC_ERROR));
					ExitThread(0);
				}
				if (VirtualAlloc((BYTE *)DelaySlotTable + (0x400000 >> 0xA), (0x400000 >> 0xA), MEM_COMMIT, PAGE_READWRITE)==NULL) {
					if (HaveDebugger)
						DisplayError("Failed to Extend Delay Slot Table to 8mb");
					else
						DisplayError(GS(MSG_MEM_ALLOC_ERROR));
					ExitThread(0);
				}
			} else {
				VirtualFree(N64MEM + 0x400000, 0x400000,MEM_DECOMMIT);
				VirtualFree((BYTE *)JumpTable + 0x400000, 0x400000,MEM_DECOMMIT);
				VirtualFree((BYTE *)DelaySlotTable + (0x400000 >> 0xA), (0x400000 >> 0xA),MEM_DECOMMIT);
			}
		}
		RdramSize = SaveRDRAMSize;

		ReadFile( hSaveFile,&Value,sizeof(Value),&dwRead,NULL);
		ChangeTimer(ViTimer,Value);
		if (formatVersion == SaveStateFormat_ORIGINAL) {
			ReadFile(hSaveFile, &PROGRAM_COUNTER.UW[0], sizeof(PROGRAM_COUNTER.UW[0]), &dwRead, NULL);
			PROGRAM_COUNTER.DW = PROGRAM_COUNTER.W[0];
		}
		else {
			ReadFile(hSaveFile, &PROGRAM_COUNTER, sizeof(PROGRAM_COUNTER), &dwRead, NULL);
		}
		ReadFile( hSaveFile,GPR,sizeof(_int64)*32,&dwRead,NULL);
		ReadFile( hSaveFile,FPR,sizeof(_int64)*32,&dwRead,NULL);
		if (formatVersion == SaveStateFormat_ORIGINAL) {
			for (int i = 0; i < 32; ++i) {
				ReadFile(hSaveFile, &CP0[i].UW[0], sizeof(DWORD),&dwRead,NULL);
				CP0[i].UW[1] = 0;
			}
		}
		else {
			ReadFile(hSaveFile, CP0, sizeof(QWORD) * 32,&dwRead,NULL);
		}
		UpdateCPUMode();

		ReadFile( hSaveFile,FPCR,sizeof(DWORD)*32,&dwRead,NULL);
		ReadFile( hSaveFile,&HI,sizeof(_int64),&dwRead,NULL);
		ReadFile( hSaveFile,&LO,sizeof(_int64),&dwRead,NULL);
		if (formatVersion == SaveStateFormat_ORIGINAL) {
			ReadFile(hSaveFile, (*RegRDRAM)[0], sizeof(DWORD) * 10, &dwRead, NULL);
			for (int i = 1; i < 4; ++i) {
				memset((*RegRDRAM)[i], 0, sizeof(Registers.RDRAM[i]));
			}
		}
		else {
			for (int i = 0; i < 4; ++i) {
				ReadFile(hSaveFile, (*RegRDRAM)[i], sizeof(DWORD) * 10, &dwRead, NULL);
			}
		}
		ReadFile( hSaveFile,RegSP,sizeof(DWORD)*10,&dwRead,NULL);
		ReadFile( hSaveFile,RegDPC,sizeof(DWORD)*10,&dwRead,NULL);
		ReadFile( hSaveFile,RegMI,sizeof(DWORD)*4,&dwRead,NULL);
		ReadFile( hSaveFile,RegVI,sizeof(DWORD)*14,&dwRead,NULL);
		ReadFile( hSaveFile,RegAI,sizeof(DWORD)*6,&dwRead,NULL);
		ReadFile( hSaveFile,RegPI,sizeof(DWORD)*13,&dwRead,NULL);
		ReadFile( hSaveFile,RegRI,sizeof(DWORD)*8,&dwRead,NULL);
		ReadFile( hSaveFile,RegSI,sizeof(DWORD)*4,&dwRead,NULL);
		ReadFile( hSaveFile,tlb,sizeof(TLB)*32,&dwRead,NULL);
		ReadFile( hSaveFile,PIF_Ram,0x40,&dwRead,NULL);
		ReadFile( hSaveFile,RDRAM,RdramSize,&dwRead,NULL);
		ReadFile( hSaveFile,DMEM,0x1000,&dwRead,NULL);
		ReadFile( hSaveFile,IMEM,0x1000,&dwRead,NULL);

		SetFpuLocations();
		CheckRdramStatus();

		// Specific data introducted in 2023.1
		ReadFile(hSaveFile, &lastUnusedCOP0Register, sizeof(int), &dwRead, NULL);
		ReadFile(hSaveFile, &WrittenToRom, sizeof(BOOL), &dwRead, NULL);
		ReadFile(hSaveFile, &WrittenToRomCount, sizeof(DWORD), &dwRead, NULL);
		ReadFile(hSaveFile, &WroteToRom, sizeof(DWORD), &dwRead, NULL);
		ReadFile(hSaveFile, ISViewerBuffer, sizeof(ISViewerBuffer), &dwRead, NULL);
		ReadFile(hSaveFile, &cop2LatchedValue.UDW, sizeof(QWORD), &dwRead, NULL);

		CloseHandle(hSaveFile);
		_splitpath( FileName, drive, dir, ZipFile, ext );
		sprintf(FileName,"%s%s",ZipFile,ext);
	}
	//memcpy(RomHeader,ROM,sizeof(RomHeader));
	ChangeCompareTimer();
	if (GfxRomClosed != NULL)  { GfxRomClosed(); }
	if (AiRomClosed != NULL)   { AiRomClosed(); }
	if (ContRomClosed != NULL) { ContRomClosed(); }
	if (RSPRomClosed) { RSPRomClosed(); }
	if (AiRomOpen != NULL) { AiRomOpen(); }
	if (GfxRomOpen != NULL) { GfxRomOpen(); }
	if (ContRomOpen != NULL) { ContRomOpen(); }
	if (RSPRomOpen != NULL) { RSPRomOpen(); }
	DlistCount = 0;
	AlistCount = 0;
	AI_STATUS_REG = 0;
	EmuAI_ClearAudio();
	AiDacrateChanged(SYSTEM_NTSC);
	ViStatusChanged();
	ViWidthChanged();
	SetupTLB();
	
	//Fix up Memory stack location
	MemoryStack = GPR[29].W[0];
	TranslateVaddr(&MemoryStack);
	MemoryStack += (DWORD)N64MEM;

	CheckInterrupts();
	DMAUsed = TRUE;
	strcpy(SaveAsFileName,"");
	strcpy(LoadFileName,"");

	if (CPU_Type == CPU_SyncCores) {		
		Registers.PROGRAM_COUNTER = PROGRAM_COUNTER;
		Registers.HI.DW = HI.DW;
		Registers.LO.DW = LO.DW;
		Registers.DMAUsed = DMAUsed;
		memcpy(&SyncRegisters,&Registers,sizeof(Registers));
		memcpy(SyncFastTlb,FastTlb,sizeof(FastTlb));
		memcpy(SyncTlb,tlb,sizeof(tlb));
		memcpy(SyncMemory,N64MEM,RdramSize);
		memcpy(SyncMemory + 0x04000000,N64MEM + 0x04000000,0x2000);		
		SwitchSyncRegisters();
		SetupTLB();
		SwitchSyncRegisters();		
		SyncNextInstruction = NORMAL;
		SyncJumpToLocation = -1;
		NextInstruction = NORMAL;
		JumpToLocation.DW = -1;
		MemAddrUsedCount[0] = 0;
		MemAddrUsedCount[1] = 0;
		SyncToPC ();
		DisplayError("Loaded");
	}
#ifdef Log_x86Code
	Stop_x86_Log();
	Start_x86_Log();
#endif
	if (HaveDebugger || FORCE_LOGGING) {
		StopLog();
		StartLog();
	}
	sprintf(String,"%s %s",GS(MSG_LOADED_STATE),FileName);
	SendMessage( hStatusWnd, SB_SETTEXT, 0, (LPARAM)String );
	return TRUE;
}

BOOL Machine_SaveState(void) {
	char Directory[255], FileName[255], ZipFile[255], String[100];
	char drive[_MAX_DRIVE] ,dir[_MAX_DIR], ext[_MAX_EXT];
	DWORD dwWritten, Value;
	HANDLE hSaveFile;

	//LogMessage("SaveState");
	if (Timers.CurrentTimerType != CompareTimer &&  Timers.CurrentTimerType != ViTimer) {
		return FALSE;
	}
	if (strlen(SaveAsFileName) == 0) {
		Settings_GetDirectory(InstantSaveDir, Directory, sizeof(Directory));
		sprintf(FileName,"%s%s",Directory,CurrentSave);
		sprintf(ZipFile,"%s.zip",FileName);
	} else {
		sprintf(FileName,"%s.pj",SaveAsFileName);
		sprintf(ZipFile,"%s.zip",SaveAsFileName);
	}

	if (SelfModCheck == ModCode_ChangeMemory) { ResetRecompCode(); }
	if (AutoZip) {
		zip_fileinfo	ZipInfo;
		zipFile			file;

		CreateDirectory(Directory,NULL);
		file = zipOpen(ZipFile,FALSE);
		zipOpenNewFileInZip(file,CurrentSave,&ZipInfo,NULL,0,NULL,0,NULL,Z_DEFLATED,Z_DEFAULT_COMPRESSION);
		Value = SaveStateFormat_2023_1;
		zipWriteInFileInZip( file,&Value,sizeof(Value));
		zipWriteInFileInZip( file,&RdramSize,sizeof(RdramSize));
		zipWriteInFileInZip( file,RomHeader,0x40);	
		Value = Timers.NextTimer[ViTimer] + Timers.Timer;
		zipWriteInFileInZip( file,&Value,sizeof(Value));
		zipWriteInFileInZip( file,&PROGRAM_COUNTER,sizeof(PROGRAM_COUNTER));
		zipWriteInFileInZip( file,GPR,sizeof(_int64)*32);
		zipWriteInFileInZip( file,FPR,sizeof(_int64)*32);
		zipWriteInFileInZip( file,CP0,sizeof(QWORD)*32);
		zipWriteInFileInZip( file,FPCR,sizeof(DWORD)*32);
		zipWriteInFileInZip( file,&HI,sizeof(_int64));
		zipWriteInFileInZip( file,&LO,sizeof(_int64));
		for (int i = 0; i < 4; ++i) {
			zipWriteInFileInZip(file, (*RegRDRAM)[i], sizeof(DWORD) * 10);
		}
		zipWriteInFileInZip( file,RegSP,sizeof(DWORD)*10);
		zipWriteInFileInZip( file,RegDPC,sizeof(DWORD)*10);

		Value = MI_INTR_REG;
		if (AiReadLength() != 0) { MI_INTR_REG |= MI_INTR_AI; }
		zipWriteInFileInZip( file,RegMI,sizeof(DWORD)*4);
		MI_INTR_REG = Value;
		zipWriteInFileInZip( file,RegVI,sizeof(DWORD)*14);
		zipWriteInFileInZip( file,RegAI,sizeof(DWORD)*6);
		zipWriteInFileInZip( file,RegPI,sizeof(DWORD)*13);
		zipWriteInFileInZip( file,RegRI,sizeof(DWORD)*8);
		zipWriteInFileInZip( file,RegSI,sizeof(DWORD)*4);
		zipWriteInFileInZip( file,tlb,sizeof(TLB)*32);
		zipWriteInFileInZip( file,PIF_Ram,0x40);
		zipWriteInFileInZip( file,RDRAM,RdramSize);
		zipWriteInFileInZip( file,DMEM,0x1000);
		zipWriteInFileInZip( file,IMEM,0x1000);

		// Specific data introducted in 2023.1
		zipWriteInFileInZip(file, &lastUnusedCOP0Register, sizeof(int));
		zipWriteInFileInZip(file, &WrittenToRom, sizeof(BOOL));
		zipWriteInFileInZip(file, &WrittenToRomCount, sizeof(DWORD));
		zipWriteInFileInZip(file, &WroteToRom, sizeof(DWORD));
		zipWriteInFileInZip(file, ISViewerBuffer, sizeof(ISViewerBuffer));
		zipWriteInFileInZip(file, &cop2LatchedValue.UDW, sizeof(QWORD));

		zipCloseFileInZip(file);
		zipClose(file,"");
		DeleteFile(FileName);
		_splitpath( ZipFile, drive, dir, FileName, ext );
		sprintf(FileName,"%s%s",FileName,ext);
	} else {
		hSaveFile = CreateFile(FileName,GENERIC_WRITE | GENERIC_READ, FILE_SHARE_READ,NULL,OPEN_ALWAYS,
			FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS, NULL);
		if (hSaveFile == INVALID_HANDLE_VALUE) {
			switch (GetLastError()) {
			case ERROR_PATH_NOT_FOUND:
				CreateDirectory(Directory,NULL);
				hSaveFile = CreateFile(FileName,GENERIC_WRITE | GENERIC_READ, FILE_SHARE_READ,
					NULL,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS, NULL);
				if (hSaveFile == INVALID_HANDLE_VALUE) {
					DisplayError(GS(MSG_FAIL_OPEN_SAVE));
					return TRUE;
				}
				break;
			default:
				DisplayError(GS(MSG_FAIL_OPEN_SAVE));
				return TRUE;
			}
		}

		while ((int)Registers.CP0[1].W[0] < (int)Registers.CP0[6].W[0]) {
			Registers.CP0[1].W[0] += 32 - Registers.CP0[6].W[0];
		}	
		//if fake cause set then do not save ????


		SetFilePointer(hSaveFile,0,NULL,FILE_BEGIN);	
		Value = SaveStateFormat_2023_1;
		WriteFile( hSaveFile,&Value,sizeof(Value),&dwWritten,NULL);
		WriteFile( hSaveFile,&RdramSize,sizeof(RdramSize),&dwWritten,NULL);
		WriteFile( hSaveFile,RomHeader,0x40,&dwWritten,NULL);	
		Value = Timers.NextTimer[ViTimer] + Timers.Timer;
		WriteFile( hSaveFile,&Value,sizeof(Value),&dwWritten,NULL);
		WriteFile( hSaveFile,&PROGRAM_COUNTER,sizeof(PROGRAM_COUNTER),&dwWritten,NULL);
		WriteFile( hSaveFile,GPR,sizeof(_int64)*32,&dwWritten,NULL);
		WriteFile( hSaveFile,FPR,sizeof(_int64)*32,&dwWritten,NULL);
		WriteFile( hSaveFile,CP0,sizeof(QWORD)*32,&dwWritten,NULL);
		WriteFile( hSaveFile,FPCR,sizeof(DWORD)*32,&dwWritten,NULL);
		WriteFile( hSaveFile,&HI,sizeof(_int64),&dwWritten,NULL);
		WriteFile( hSaveFile,&LO,sizeof(_int64),&dwWritten,NULL);
		for (int i = 0; i < 4; ++i) {
			WriteFile(hSaveFile, (*RegRDRAM)[i], sizeof(DWORD) * 10, &dwWritten, NULL);
		}
		WriteFile( hSaveFile,RegSP,sizeof(DWORD)*10,&dwWritten,NULL);
		WriteFile( hSaveFile,RegDPC,sizeof(DWORD)*10,&dwWritten,NULL);

		Value = MI_INTR_REG;
		if (AiReadLength() != 0) { MI_INTR_REG |= MI_INTR_AI; }
		WriteFile( hSaveFile,RegMI,sizeof(DWORD)*4,&dwWritten,NULL);
		MI_INTR_REG = Value;
		WriteFile( hSaveFile,RegVI,sizeof(DWORD)*14,&dwWritten,NULL);
		WriteFile( hSaveFile,RegAI,sizeof(DWORD)*6,&dwWritten,NULL);
		WriteFile( hSaveFile,RegPI,sizeof(DWORD)*13,&dwWritten,NULL);
		WriteFile( hSaveFile,RegRI,sizeof(DWORD)*8,&dwWritten,NULL);
		WriteFile( hSaveFile,RegSI,sizeof(DWORD)*4,&dwWritten,NULL);
		WriteFile( hSaveFile,tlb,sizeof(TLB)*32,&dwWritten,NULL);
		WriteFile( hSaveFile,PIF_Ram,0x40,&dwWritten,NULL);
		WriteFile( hSaveFile,RDRAM,RdramSize,&dwWritten,NULL);
		WriteFile( hSaveFile,DMEM,0x1000,&dwWritten,NULL);
		WriteFile( hSaveFile,IMEM,0x1000,&dwWritten,NULL);

		// Specific data introducted in 2023.1
		WriteFile(hSaveFile, &lastUnusedCOP0Register, sizeof(int), &dwWritten, NULL);
		WriteFile(hSaveFile, &WrittenToRom, sizeof(BOOL), &dwWritten, NULL);
		WriteFile(hSaveFile, &WrittenToRomCount, sizeof(DWORD), &dwWritten, NULL);
		WriteFile(hSaveFile, &WroteToRom, sizeof(DWORD), &dwWritten, NULL);
		WriteFile(hSaveFile, ISViewerBuffer, sizeof(ISViewerBuffer), &dwWritten, NULL);
		WriteFile(hSaveFile, &cop2LatchedValue.UDW, sizeof(QWORD), &dwWritten, NULL);

		CloseHandle(hSaveFile);
		DeleteFile(ZipFile);
		_splitpath( FileName, drive, dir, ZipFile, ext );
		sprintf(FileName,"%s%s",ZipFile,ext);
	}
	strcpy(SaveAsFileName,"");
	strcpy(LoadFileName,"");
	sprintf(String,"%s %s",GS(MSG_SAVED_STATE),FileName);
	SendMessage( hStatusWnd, SB_SETTEXT, 0, (LPARAM)String );
	return TRUE;
}

void PauseCpu (void) {
	DWORD Result;
	if (!CPURunning) { return; }
		
	do {
		Result = MsgWaitForMultipleObjects(1,&hPauseMutex,FALSE,INFINITE,QS_ALLINPUT);
		if (Result != WAIT_OBJECT_0) {
			MSG msg;

			while (PeekMessage(&msg,NULL,0,0,PM_REMOVE) != 0) {
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
	} while (Result != WAIT_OBJECT_0);

	if (CPU_Paused || CPU_Action.Pause) {
		HMENU hMenu = GetMenu(hMainWindow);
		HMENU hSubMenu = GetSubMenu(hMenu,1);

		if (CPU_Action.Pause) {
			CPU_Action.Pause = FALSE;
			CPU_Paused = FALSE;
			ManualPaused = FALSE;
			SendMessage( hStatusWnd, SB_SETTEXT, 0, (LPARAM)GS(MSG_CPU_RESUMED) );
			ReleaseMutex(hPauseMutex);
			return;
		}
		ResumeThread(hCPU);
		SendMessage( hStatusWnd, SB_SETTEXT, 0, (LPARAM)GS(MSG_CPU_RESUMED));	
		MenuSetText(hSubMenu, 1, GS(MENU_PAUSE),"F2");
		ManualPaused = FALSE;
		CPU_Paused = FALSE;
	} else {
		CPU_Action.Pause = TRUE;
		CPU_Action.DoSomething = TRUE;
	}
	ReleaseMutex(hPauseMutex);
}

#define INIT_VI_INTR_TIME 500000

void RefreshScreen (void ){ 
	static DWORD VI_INTR_TIME = INIT_VI_INTR_TIME;
	static int DlistWaitFor = -2, VIWaitMult = 0;
	LARGE_INTEGER Time;
	char Label[100];
	
	// A hack to allow the iQue games to boot faster, seems the VI call may be incomplete?
	// This allows them to wait for longer and for now this will use the DlistCount
	if (DlistWaitFor == -2) {
		char Identifier[100];
		RomID(Identifier, RomHeader);
		DlistWaitFor = Settings_ReadInt(RDS_NAME, Identifier, "DlistWait", -1);
		VIWaitMult = Settings_ReadInt(RDS_NAME, Identifier, "WaitMult", -1);
		if (DlistWaitFor <= 0)
			DlistWaitFor = -1;
	}

	if (Profiling || ShowCPUPer) { memcpy(Label,ProfilingLabel,sizeof(Label)); }
	if (Profiling) { StartTimer("RefreshScreen"); }
	
	if (VI_V_SYNC_REG == 0) {
		VI_INTR_TIME = INIT_VI_INTR_TIME;
	} 
	else {
		VI_INTR_TIME = (VI_V_SYNC_REG + 1) * ModVI;
		if (DlistWaitFor != -1 && DlistCount <= DlistWaitFor)	// The hack that allows a much longer wait time than normal
			VI_INTR_TIME *= VIWaitMult;
		if ((VI_V_SYNC_REG % 1) != 0) {
			VI_INTR_TIME -= 38;
		}
	}

	ChangeTimer(ViTimer,Timers.Timer + /*Timers.NextTimer[ViTimer] +*/ VI_INTR_TIME);
	EmuAI_SetVICountPerFrame(VI_INTR_TIME);
	

	UpdateFieldSerration((VI_STATUS_REG & 0x40) != 0);

	if (ShowCPUPer || Profiling) { StartTimer("CPU Idle"); }
	if (LimitFPS) {	Timer_Process(NULL); }
	if (ShowCPUPer || Profiling) { StopTimer(); }
	if (Profiling) { StartTimer("RefreshScreen: Update FPS"); }
	if ((CurrentFrame & 7) == 0) {
		//Disables Screen saver
		//mouse_event(MOUSEEVENTF_MOVE,1,1,0,GetMessageExtraInfo());
		//mouse_event(MOUSEEVENTF_MOVE,-1,-1,0,GetMessageExtraInfo());

		QueryPerformanceCounter(&Time);
		Frames[(CurrentFrame >> 3) % NoOfFrames].QuadPart = Time.QuadPart - LastFrame.QuadPart;
		LastFrame.QuadPart = Time.QuadPart;	
		DisplayFPS();
	}
	if (Profiling) { StopTimer(); }
	if (ShowCPUPer) { DisplayCPUPer(); }
	CurrentFrame += 1;

	if (Profiling) { StartTimer("RefreshScreen: Update Screen"); }
	__try {
		if (UpdateScreen != NULL) { UpdateScreen(); }
	} __except( r4300i_CPU_MemoryFilter( GetExceptionCode(), GetExceptionInformation()) ) {
		DisplayError("Unknown memory action in trying to update the screen\n\nEmulation stop");
		ExitThread(0);
	}
	if (Profiling) { StartTimer("RefreshScreen: Cheats"); }
	if ((STATUS_REGISTER & STATUS_IE) != 0 ) { ApplyCheats(); Apply_CheatSearchDev(); }
	if (Profiling || ShowCPUPer) { StartTimer(Label); }
}

#define NUMCYCLES 200
#define RSP_TIMER_INC 300
#define RSP_TIMER_INC 100
int RSPisRunning = 0;
int CheckRSPInterrupt = 0;

void RunRsp (void) {
	if (Timers.Active[RspTimer]) return;

	if ( ( SP_STATUS_REG & SP_STATUS_HALT ) == 0) {
		DWORD Task = *( DWORD *)(DMEM + 0xFC0);

		if (RSPisRunning) {
			int temp = DoRspCycles(NUMCYCLES);
			if ((SP_STATUS_REG & SP_STATUS_HALT) != 0)
			{
				RSPisRunning = 0;
				if (CheckRSPInterrupt)
					CheckInterrupts();
				CheckRSPInterrupt = 0;
			}
			else
			{
				ChangeTimer(RspTimer, RSP_TIMER_INC);
			}
			return;
		}

		if ((SP_STATUS_REG & SP_STATUS_BROKE) == 0) {
			if (Task == 1 && (DPC_STATUS_REG & DPC_STATUS_FREEZE) != 0) {
return;
			}

			switch (Task) {
			case 0: {
				// TWINTRIS Support, the RSP Task is 0 yet it is meant to be a 1 (Graphics call)
				unsigned int ucode = *(DWORD*)(DMEM + 0xFFC);
				unsigned int size = *(DWORD*)(DMEM + 0x100B) & 0xF80;

				unsigned int i;
				unsigned int sum = 0;
				if (ucode < 0x800000)
					for (i = 0, sum = 0; i < (size / 2); i++)
						sum += *(BYTE*)(RDRAM + ucode + i);

				if (sum == 0x7efd)
					*(DWORD*)(DMEM + 0xFC0) = 1;
			}
			case 1:
				DlistCount += 1;
				/*if ((DlistCount % 2) == 0) {
					SP_STATUS_REG |= (0x0203 );
					MI_INTR_REG |= MI_INTR_SP | MI_INTR_DP;
					CheckInterrupts();
					return;
				}*/
				break;
			case 2:
				AlistCount += 1;
				break;
			}

			if (ShowDListAListCount) {
				char StatusString[256];

				sprintf(StatusString, "Dlist: %d   Alist: %d", DlistCount, AlistCount);
				SendMessage(hStatusWnd, SB_SETTEXT, 0, (LPARAM)StatusString);
			}
		}
		if (Profiling || ShowCPUPer) {
			char Label[100];

			strncpy(Label,ProfilingLabel,sizeof(Label));

			if (IndividualBlock && !ShowCPUPer) {
				StartTimer("RSP");
			}
			else {
				switch (*(DWORD*)(DMEM + 0xFC0)) {
				case 1:  StartTimer("RSP: Dlist"); break;
				case 2:  StartTimer("RSP: Alist"); break;
				default: StartTimer("RSP: Unknown"); break;
				}
			}

			RSPisRunning = 1;
			DoRspCycles(NUMCYCLES);
			if ((SP_STATUS_REG & SP_STATUS_HALT) != 0) {
				RSPisRunning = 0;
			}
			else
			{
				ChangeTimer(RspTimer, RSP_TIMER_INC);
			}
			StartTimer(Label);
		} else {
			RSPisRunning = 1;
			DoRspCycles(NUMCYCLES);

			if ((SP_STATUS_REG & SP_STATUS_HALT) != 0) {
				RSPisRunning = 0;
			}
			else
			{
				ChangeTimer(RspTimer, RSP_TIMER_INC);
			}
		}
#ifdef CFB_READ
			if (VI_ORIGIN_REG > 0x280) {
				SetFrameBuffer(VI_ORIGIN_REG, (DWORD)(VI_WIDTH_REG * (VI_WIDTH_REG *.75)));
			}
#endif
		/*if ((SP_STATUS_REG & SP_STATUS_HALT) == 0) {
			ChangeTimer(RspTimer, RSP_TIMER_INC);
		}*/
	} 
}

void SetCoreToRunning  ( void ) {
	CPU_Action.Stepping = FALSE;
	PulseEvent( CPU_Action.hStepping );
}

void SetCoreToStepping ( void ) {
	CPU_Action.Stepping = TRUE;
}

void SetCoreToSkipping(void) {
	CPU_Action.Stepping = TRUE;
	CPU_Action.Skipping = TRUE;
}

void StartEmulation ( void ) {
	DWORD ThreadID, count;

	memset(&CPU_Action,0,sizeof(CPU_Action));
	//memcpy(RomHeader,ROM,sizeof(RomHeader));
	CPU_Action.hStepping = CreateEvent( NULL, FALSE, FALSE, NULL);
	WrittenToRom = FALSE;
	
	// Previous versions of PJ64 did not clear memory upon start of emulation
	// This resulted in junk being left in memory and potentially affecting future games being loaded or checked
	if (Settings_ReadBool(APPS_NAME, STR_SETTINGS, STR_CLEAR_MEMORY, TRUE))
		memset(N64MEM, 0, RdramSize);

	InitilizeTLB();
	InitalizeR4300iRegisters(LoadPifRom(*(ROM + 0x3D)),*(ROM + 0x3D),GetCicChipID(ROM));

	BuildInterpreter();
	
	RecompPos = RecompCode;

	if (HaveDebugger) {
		Enable_R4300i_Commands_Window();
	}
	if (InR4300iCommandsWindow) {
		SetCoreToStepping();
	}

    DlistCount = 0;
	AlistCount = 0;

	Timers.CurrentTimerType = -1;
	Timers.Timer = 0;
	CurrentFrame = 0;
	firstFrameWithInterruptsDisabled = 0;
	CurrentPercent = 0;
	for (count = 0; count < MaxTimers; count ++) { Timers.Active[count] = FALSE; }
	ChangeTimer(ViTimer,5000); 
	ChangeCompareTimer();
	ViFieldSerration = 0;
	DMAUsed = FALSE;
	CPU_Paused = FALSE;
	ManualPaused = FALSE;
	Timer_Start();
	LoadRomOptions();
	LoadCheats();
	if (Profiling) { ResetTimerList(); }
	strcpy(ProfilingLabel,"");
	strcpy(LoadFileName,"");
	strcpy(SaveAsFileName,"");
	CPURunning = TRUE;
	if (!inFullScreen)
		SetupMenu(hMainWindow);

	if (inFullScreen) {
		ResetAudio(hMainWindow);
		SetupPlugins(hMainWindow);
	}

	switch (CPU_Type) {
	case CPU_Interpreter: hCPU = CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)StartInterpreterCPU,NULL,0, &ThreadID); break;
	case CPU_Recompiler: hCPU = CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)StartRecompilerCPU,NULL,0, &ThreadID); break;
	case CPU_SyncCores: hCPU = CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)StartSyncCPU,NULL,0, &ThreadID); break;
	default:
		DisplayError("Unhandled CPU %d",CPU_Type);
	}
	SendMessage( hStatusWnd, SB_SETTEXT, 0, (LPARAM)GS(MSG_EMULATION_STARTED) );
	AlwaysOnTopWindow(hMainWindow);
}

void StepOpcode        ( void ) {
	PulseEvent( CPU_Action.hStepping );
}

void TimerDone (void) {
	char Label[100];
	if (Profiling) { 
		strncpy(Label, ProfilingLabel, sizeof(Label));
		StartTimer("TimerDone"); 
	}

	switch (Timers.CurrentTimerType) {
	case CompareTimer:
		CAUSE_REGISTER |= CAUSE_IP7;
		CheckInterrupts();
		ChangeCompareTimer();
		break;
	case SiTimer:
		ChangeTimer(SiTimer,0);
		MI_INTR_REG |= MI_INTR_SI;
		SI_STATUS_REG |= SI_STATUS_INTERRUPT;
		CheckInterrupts();
		break;
	case PiTimer:
		ChangeTimer(PiTimer,0);
		PI_STATUS_REG &= ~PI_STATUS_DMA_BUSY;
		MI_INTR_REG |= MI_INTR_PI;
		CheckInterrupts();
		break;
	case ViTimer:
		RefreshScreen();
		MI_INTR_REG |= MI_INTR_VI;
		CheckInterrupts();
		break;
	case RspTimer:
		ChangeTimer(RspTimer,0);
		RunRsp();
		if (!RSPisRunning)
			// Test to see if this helps netplay out any.
			// Jabo believes if it does help netplay then there could be a possible issue inside the rsp.
			CheckInterrupts();
		else
			CheckRSPInterrupt = 1;
		break;
	case AiTimer:
		EmuAI_SetNextTimer();
		AudioIntrReg |= MI_INTR_AI;
		AiCheckInterrupts();
		break;
	}
	if (Profiling) { 
		StartTimer(Label); 
	}
}
