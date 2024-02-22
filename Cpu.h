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
#include "Interpreter CPU.h"
#include "Interpreter Ops.h"
#include "Recompiler CPU.h"
#include "registers.h"
#include "Recompiler Ops.h"
#include "tlb.h"
#include "Sync CPU.h"
#include "memory.h"
#include "DMA.h"
#include "eeprom.h"
#include "sram.h"
#include "flashram.h"
#include "mempak.h"
#include "Exception.h"
#include "pif.h"
#include "opcode.h"
#include "rom.h"

typedef struct {
	HANDLE hStepping;

	BOOL DoSomething;
	BOOL CloseCPU;
	BOOL ChangeWindow;
	BOOL CheckInterrupts;
	BOOL Pause;
	BOOL SaveState;
	BOOL RestoreState;
	BOOL DoInterrupt;
	BOOL Stepping;
	BOOL Skipping;
} CPU_ACTION;


#define MaxTimers				6
#define CompareTimer			0
#define SiTimer					1
#define PiTimer					2
#define ViTimer					3
#define RspTimer				4
#define AiTimer					5


typedef struct {
	int  NextTimer[MaxTimers];
	BOOL Active[MaxTimers];
	int  CurrentTimerType;
	int  Timer;
} SYSTEM_TIMERS;

void ChangeCompareTimer(void);
void ChangeTimer(int Type, int Value);
void CheckTimer(void);
void CloseCpu(void);
int  DelaySlotEffectsCompare(MIPS_DWORD PC, DWORD Reg1, DWORD Reg2);
int  DelaySlotEffectsJump(MIPS_DWORD JumpPC);
void DoSomething(void);
void InPermLoop(void);
void InitiliazeCPUFlags(void);
BOOL Machine_LoadState(void);
BOOL Machine_SaveState(void);
void PauseCpu(void);
void RefreshScreen(void);
void RunRsp(void);
void SetCoreToRunning(void);
void SetCoreToStepping(void);
void SetCoreToSkipping(void);
void StartEmulation(void);
void StepOpcode(void);
void TimerDone(void);

#define NORMAL					0
#define DO_DELAY_SLOT			1
#define DO_END_DELAY_SLOT		2
#define DELAY_SLOT				3
#define END_DELAY_SLOT			4
#define LIKELY_DELAY_SLOT		5
#define JUMP	 				6
#define DELAY_SLOT_DONE			7
#define LIKELY_DELAY_SLOT_DONE	8
#define END_BLOCK 				9

enum SaveType {
	Auto,
	Eeprom_4K,
	Eeprom_16K,
	Sram,
	FlashRam
};

enum  SaveStateFormat {
	SaveStateFormat_ORIGINAL = 0x23D8A6C8,
	SaveStateFormat_2023_1   = 0x23D8A6C9
};

#ifdef CFB_READ
extern DWORD CFBStart, CFBEnd;

void __cdecl SetFrameBuffer(DWORD Address, DWORD Length);
#endif

extern int CPOAdjust;
extern int NextInstruction, ManualPaused, CPU_Paused, CountPerOp;
extern MIPS_DWORD JumpToLocation;
extern char SaveAsFileName[MAX_PATH], LoadFileName[MAX_PATH];
extern int DlistCount, AlistCount, CurrentSaveSlot;
extern enum SaveType SaveUsing;
extern CPU_ACTION CPU_Action;
extern SYSTEM_TIMERS Timers;
extern HANDLE hPauseMutex;
extern OPCODE Opcode;
extern HANDLE hCPU;
#ifdef __cplusplus
extern "C" {
#endif
	extern BOOL inFullScreen, CPURunning, SPHack;
#ifdef __cplusplus
}
#endif

#ifdef Interpreter_StackTest
extern DWORD StackValue;
#endif
extern DWORD MemoryStack;
