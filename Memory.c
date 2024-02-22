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
#include "cpu.h"
#include "x86.h"
#include "plugin.h"
#include "debugger.h"


DWORD *TLB_ReadMap, *TLB_WriteMap, RdramSize, SystemRdramSize;
BYTE *N64MEM, *RDRAM, *DMEM, *IMEM, *ROM;
void ** JumpTable, ** DelaySlotTable;
BYTE *RecompCode, *RecompPos;
MIPS_DWORD* RegisterCurrentlyWritten;

BOOL WrittenToRom;
DWORD WrittenToRomCount;
DWORD WroteToRom;
DWORD TempValue;

BYTE ISViewerBuffer[0x200];
BYTE ISViewerTempBuffer[0x1000+0x200+1];
DWORD ISViewerTempBufferLength;

int Addressing64Bits;
BOOL KernelMode;
BOOL RdramFullyConfigured;

int Allocate_ROM ( void ) {	
	ROM = (BYTE *)malloc(RomFileSize);
	WrittenToRom = FALSE;
	WrittenToRomCount = 0;
	return ROM == NULL ? FALSE : TRUE;
}

int Allocate_Memory ( void ) {	
	RdramSize = 0x400000;

	WrittenToRom = FALSE;
	WrittenToRomCount = 0;
	
	N64MEM = (unsigned char *) VirtualAlloc( NULL, 0x20000000, MEM_RESERVE | MEM_TOP_DOWN, PAGE_READWRITE );
	if(N64MEM==NULL) {  
		DisplayError(GS(MSG_MEM_ALLOC_ERROR));
		return FALSE;
	}

	(BYTE *)JumpTable = (BYTE *) VirtualAlloc( NULL, 0x10000000, MEM_RESERVE | MEM_TOP_DOWN, PAGE_READWRITE );
	if( JumpTable == NULL ) {  
		DisplayError(GS(MSG_MEM_ALLOC_ERROR));
		return FALSE;
	}

	if (HaveDebugger)
		SyncMemory = (unsigned char *) VirtualAlloc( NULL, 0x20000000, MEM_RESERVE | MEM_TOP_DOWN, PAGE_READWRITE );

	/* Recomp code */
	RecompCode=(BYTE *) VirtualAlloc( NULL, LargeCompileBufferSize + 4, MEM_RESERVE|MEM_TOP_DOWN, PAGE_EXECUTE_READWRITE);
	RecompCode=(BYTE *) VirtualAlloc( RecompCode, NormalCompileBufferSize, MEM_COMMIT, PAGE_EXECUTE_READWRITE);
	if(RecompCode==NULL) {  
		DisplayError(GS(MSG_MEM_ALLOC_ERROR));
		return FALSE;
	}

	/* Memory */
	TLB_ReadMap = (DWORD *)VirtualAlloc(NULL,0xFFFFF * sizeof(DWORD),MEM_RESERVE|MEM_COMMIT| MEM_TOP_DOWN,PAGE_READWRITE);
	if (TLB_ReadMap == NULL) {
		DisplayError(GS(MSG_MEM_ALLOC_ERROR));
		return FALSE;
	}

	TLB_WriteMap = (DWORD *)VirtualAlloc(NULL,0xFFFFF * sizeof(DWORD),MEM_RESERVE|MEM_COMMIT| MEM_TOP_DOWN,PAGE_READWRITE);
	if (TLB_WriteMap == NULL) {
		DisplayError(GS(MSG_MEM_ALLOC_ERROR));
		return FALSE;
	}
	
	/* Delay Slot Table */
	(BYTE *)DelaySlotTable = (BYTE *) VirtualAlloc( NULL, (0x20000000 >> 0xA), MEM_RESERVE | MEM_TOP_DOWN, PAGE_READWRITE );
	if( DelaySlotTable == NULL ) {  
		DisplayError(GS(MSG_MEM_ALLOC_ERROR));
		return FALSE;
	}
	if(VirtualAlloc(DelaySlotTable, (0x400000 >> 0xA), MEM_COMMIT, PAGE_READWRITE)==NULL) {
		DisplayError(GS(MSG_MEM_ALLOC_ERROR));
		return FALSE;
	}

	if(VirtualAlloc((BYTE *)DelaySlotTable + (0x04000000 >> 0xA), (0x2000 >> 0xA), MEM_COMMIT, PAGE_READWRITE)==NULL) {
		DisplayError(GS(MSG_MEM_ALLOC_ERROR));
		return FALSE;
	}

	if(VirtualAlloc(N64MEM, 0x00400000, MEM_COMMIT, PAGE_READWRITE)==NULL) {
		DisplayError(GS(MSG_MEM_ALLOC_ERROR));
		return FALSE;
	}

	if(VirtualAlloc(N64MEM + 0x04000000, 0x2000, MEM_COMMIT, PAGE_READWRITE)==NULL) {
		DisplayError(GS(MSG_MEM_ALLOC_ERROR));
		return FALSE;
	}

	RDRAM = (unsigned char *)(N64MEM);
	DMEM  = (unsigned char *)(N64MEM+0x04000000);
	IMEM  = (unsigned char *)(N64MEM+0x04001000);
	ROM   = NULL;

	/* Jump Table */
	if(VirtualAlloc(JumpTable, 0x00400000, MEM_COMMIT, PAGE_READWRITE)==NULL) {
		DisplayError(GS(MSG_MEM_ALLOC_ERROR));
		return FALSE;
	}

	if(VirtualAlloc((BYTE *)JumpTable + 0x04000000, 0x2000, MEM_COMMIT, PAGE_READWRITE)==NULL) {
		DisplayError(GS(MSG_MEM_ALLOC_ERROR));
		return FALSE;
	}

//	if(VirtualAlloc((BYTE *)JumpTable + 0x1FC00000, 0x7C0, MEM_COMMIT, PAGE_READWRITE)==NULL) {
//		DisplayError(GS(MSG_MEM_ALLOC_ERROR));
//		return FALSE;
//	}

	memset(ISViewerBuffer, 0, sizeof(ISViewerBuffer));
	memset(ISViewerTempBuffer, 0, sizeof(ISViewerTempBuffer));
	ISViewerTempBufferLength = 0;
	RegisterCurrentlyWritten = NULL;

	return TRUE;
}

BOOL Compile_LB ( int Reg, DWORD Addr, BOOL SignExtend) {
	char VarName[100];

	if (!TranslateVaddr(&Addr)) {
		return FALSE;
	}

	switch (Addr & 0xFFF00000) {
	case 0x00000000:
	case 0x00100000: 
	case 0x00200000: 
	case 0x00300000: 
	case 0x00400000: 
	case 0x00500000: 
	case 0x00600000: 
	case 0x00700000: 
	// TODO: This is most certainly not enough! It only covers 1 MB PI ROM!
	// Also, this doesn't appear to emulate byte accesses to PI correctly.
	// See: https://github.com/n64dev/cen64/issues/86#issuecomment-493314371
	case 0x10000000:
		sprintf(VarName,"N64MEM + %X",Addr);
		if (SignExtend) {
			MoveSxVariableToX86regByte(Addr + N64MEM,VarName,Reg); 
		} else {
			MoveZxVariableToX86regByte(Addr + N64MEM,VarName,Reg); 
		}
		break;
	default:
		MoveConstToX86reg(0,Reg);
		CPU_Message("Compile_LB\nFailed to compile address: %X", Addr);
		if (ShowUnhandledMemory) { DisplayError("Compile_LB\nFailed to compile address: %X",Addr); }
	}

	return TRUE;
}

BOOL Compile_LH ( int Reg, DWORD Addr, BOOL SignExtend) {
	char VarName[100];

	if (!TranslateVaddr(&Addr)) {
		return FALSE;
	}

	switch (Addr & 0xFFF00000) {
	case 0x00000000:
	case 0x00100000: 
	case 0x00200000: 
	case 0x00300000: 
	case 0x00400000: 
	case 0x00500000: 
	case 0x00600000: 
	case 0x00700000: 
	// TODO: This is most certainly not enough! It only covers 1 MB PI ROM!
	// Also, this doesn't appear to emulate half-word accesses to PI correctly.
	// See: https://github.com/n64dev/cen64/issues/86#issuecomment-493314371
	case 0x10000000:
		sprintf(VarName,"N64MEM + %X",Addr);
		if (SignExtend) {
			MoveSxVariableToX86regHalf(Addr + N64MEM,VarName,Reg); 
		} else {
			MoveZxVariableToX86regHalf(Addr + N64MEM,VarName,Reg); 
		}
		break;
	default:
		MoveConstToX86reg(0,Reg);
		CPU_Message("Compile_LH\nFailed to compile address: %X", Addr);
		if (ShowUnhandledMemory) { DisplayError("Compile_LH\nFailed to compile address: %X",Addr); }
	}

	return TRUE;
}

DWORD ReadRdramRegisterAddr;
DWORD ReadRdramRegister() {
	int deviceId = (ReadRdramRegisterAddr >> 10) & 0x1FE;
	deviceId = (((deviceId >> 0) & 0x3F) << 26) |
		(((deviceId >> 6) & 1) << 23) |
		(((deviceId >> 7) & 0xFF) << 8) |
		(((deviceId >> 15) & 0x1) << 7);

	int deviceIndex = -1;

	for (int i = 0; i < NUMBER_OF_RDRAM_MODULES; ++i) {
		if (deviceId == (RDRAM_DEVICE_ID_REG(i) & 0xF880FF80)) {
			deviceIndex = i;
			break;
		}
	}

	if (deviceIndex != -1) {
		switch (ReadRdramRegisterAddr & 0x3FF) {
		case 0x000: return RDRAM_DEVICE_TYPE_REG(deviceIndex);
		case 0x004: return RDRAM_DEVICE_ID_REG(deviceIndex);
		case 0x008: return RDRAM_DELAY_REG(deviceIndex);
		case 0x00C: return RDRAM_MODE_REG(deviceIndex);
		case 0x010: return RDRAM_REF_INTERVAL_REG(deviceIndex);
		case 0x014: return RDRAM_REF_ROW_REG(deviceIndex);
		case 0x018: return RDRAM_RAS_INTERVAL_REG(deviceIndex);
		case 0x01C: return RDRAM_MIN_INTERVAL_REG(deviceIndex);
		case 0x020: return RDRAM_ADDR_SELECT_REG(deviceIndex);
		case 0x024: return RDRAM_DEVICE_MANUF_REG(deviceIndex);
		default:
			LogMessage("LW from %x, PC=%llx", ReadRdramRegisterAddr, PROGRAM_COUNTER.UDW);
			return 0;
		}
	}
	else {
		LogMessage("rambus device not found at address %x", ReadRdramRegisterAddr);
	}
	return 0;
}

BOOL Compile_LW(int Reg, DWORD Addr) {
	char VarName[100];

	if (!TranslateVaddr(&Addr)) {
		return FALSE;
	}

	switch (Addr & 0xFFF00000) {
	case 0x00000000: 
	case 0x00100000: 
	case 0x00200000: 
	case 0x00300000: 
	case 0x00400000: 
	case 0x00500000: 
	case 0x00600000: 
	case 0x00700000:
	case 0x06000000:  // Added for 64DD IPL
	// TODO: This is most certainly not enough! It only covers 2 MB PI ROM!
	case 0x10000000:
	case 0x10200000:  // Same as 0x10000000, added for Game Boy 64 by McBain & Snake (POM '98) (PD)
 		sprintf(VarName,"N64MEM + %X",Addr);
		MoveVariableToX86reg(Addr + N64MEM,VarName,Reg); 
		break;
	case 0x03F00000:
		MoveConstToVariable(Addr & 0x1FFFFFFF, &ReadRdramRegisterAddr, "ReadRdramRegisterAddr");
		Pushad();
		Call_Direct(&ReadRdramRegister, "ReadRdramRegister");
		MoveX86regToVariable(x86_EAX, &TempValue, "TempValue");
		Popad();
		MoveVariableToX86reg(&TempValue, "TempValue", Reg);
		break;
	case 0x04000000:
		if (Addr < 0x04002000) { 
			sprintf(VarName,"N64MEM + %X",Addr);
			MoveVariableToX86reg(Addr + N64MEM,VarName,Reg); 
			break; 
		}
		switch (Addr) {
		case 0x04040000: MoveVariableToX86reg(&SP_MEM_ADDR_REG, "SP_MEM_ADDR_REG", Reg); break;
		case 0x04040004: MoveVariableToX86reg(&SP_DRAM_ADDR_REG, "SP_DRAM_ADDR_REG", Reg); break;
		case 0x04040008: MoveVariableToX86reg(&SP_RD_LEN_REG, "SP_RD_LEN_REG", Reg); break;
		case 0x0404000C: MoveVariableToX86reg(&SP_WR_LEN_REG, "SP_WR_LEN_REG", Reg); break;
		case 0x04040010: MoveVariableToX86reg(&SP_STATUS_REG, "SP_STATUS_REG", Reg); break;
		case 0x04040014: MoveVariableToX86reg(&SP_DMA_FULL_REG, "SP_DMA_FULL_REG", Reg); break;
		case 0x04040018: MoveVariableToX86reg(&SP_DMA_BUSY_REG, "SP_DMA_BUSY_REG", Reg); break;
		case 0x04080000: MoveVariableToX86reg(&SP_PC_REG, "SP_PC_REG", Reg); break;
		default:
			MoveConstToX86reg(0, Reg);
			CPU_Message("Compile_LW\nFailed to compile address: %X", Addr);
			if (ShowUnhandledMemory) { DisplayError("Compile_LW\nFailed to compile address: %X", Addr); }
		}
		break;
	case 0x04100000:
		switch (Addr) {
			case 0x0410000C: MoveVariableToX86reg(&DPC_STATUS_REG,"DPC_STATUS_REG",Reg); break;
			case 0x04100010: MoveVariableToX86reg(&DPC_CLOCK_REG,"DPC_CLOCK_REG",Reg); break;
			case 0x04100014: MoveVariableToX86reg(&DPC_BUFBUSY_REG,"DPC_BUFBUSY_REG",Reg); break;
			case 0x04100018: MoveVariableToX86reg(&DPC_PIPEBUSY_REG,"DPC_PIPEBUSY_REG",Reg); break;
			case 0x0410001C: MoveVariableToX86reg(&DPC_TMEM_REG,"DPC_TMEM_REG",Reg); break;
			default:
				CPU_Message("Compile_LW\nFailed to compile address: %X", Addr);
				if (ShowUnhandledMemory) { DisplayError("Compile_LW\nFailed to compile address: %X",Addr); }
				sprintf(VarName,"N64MEM + %X",Addr);
				MoveVariableToX86reg(Addr + N64MEM,VarName,Reg); 
				break;
		}
		break;
	case 0x04300000:
		switch (Addr) {
		case 0x04300000: MoveVariableToX86reg(&MI_MODE_REG,"MI_MODE_REG",Reg); break;
		case 0x04300004: MoveVariableToX86reg(&MI_VERSION_REG,"MI_VERSION_REG",Reg); break;
		case 0x04300008: MoveVariableToX86reg(&MI_INTR_REG,"MI_INTR_REG",Reg); break;
		case 0x0430000C: MoveVariableToX86reg(&MI_INTR_MASK_REG,"MI_INTR_MASK_REG",Reg); break;
		default:
			MoveConstToX86reg(0,Reg);
			CPU_Message("Compile_LW\nFailed to compile address: %X", Addr);
			if (ShowUnhandledMemory) { DisplayError("Compile_LW\nFailed to compile address: %X",Addr); }
		}
		break;
	case 0x04400000: 
		switch (Addr) {
		case 0x04400010:
			Pushad();
			Call_Direct(&UpdateCurrentHalfLine,"UpdateCurrentHalfLine");
			Popad();
			MoveVariableToX86reg(&HalfLine,"HalfLine",Reg);
			break;
		default:
			CPU_Message("Compile_LW\nFailed to compile address: %X", Addr);
			MoveConstToX86reg(0,Reg);
			if (ShowUnhandledMemory) { DisplayError("Compile_LW\nFailed to compile address: %X",Addr); }
		}
		break;
	case 0x04500000: /* AI registers */
		switch (Addr) {
		case 0x04500004: 
			if (AiReadLength != NULL) {
				Pushad();
				Call_Direct(AiReadLength,"AiReadLength");
				MoveX86regToVariable(x86_EAX,&TempValue,"TempValue"); 
				Popad();
				MoveVariableToX86reg(&TempValue,"TempValue",Reg);
			} else {
				MoveConstToX86reg(0,Reg);
			}						
			break;
		case 0x0450000C: MoveVariableToX86reg(&AI_STATUS_REG,"AI_STATUS_REG",Reg); break;
		default:
			MoveConstToX86reg(0,Reg);
			CPU_Message("Compile_LW\nFailed to compile address: %X", Addr);
			if (ShowUnhandledMemory) { DisplayError("Compile_LW\nFailed to compile address: %X",Addr); }
		}
		break;
	case 0x04600000:
		switch (Addr) {
		case 0x04600004: MoveVariableToX86reg(&PI_CART_ADDR_REG,"PI_CART_ADDR_REG", Reg); break;
		case 0x04600010: MoveVariableToX86reg(&PI_STATUS_REG,"PI_STATUS_REG",Reg); break;
		case 0x04600014: MoveVariableToX86reg(&PI_DOMAIN1_REG,"PI_DOMAIN1_REG",Reg); break;
		case 0x04600018: MoveVariableToX86reg(&PI_BSD_DOM1_PWD_REG,"PI_BSD_DOM1_PWD_REG",Reg); break;
		case 0x0460001C: MoveVariableToX86reg(&PI_BSD_DOM1_PGS_REG,"PI_BSD_DOM1_PGS_REG",Reg); break;
		case 0x04600020: MoveVariableToX86reg(&PI_BSD_DOM1_RLS_REG,"PI_BSD_DOM1_RLS_REG",Reg); break;
		case 0x04600024: MoveVariableToX86reg(&PI_DOMAIN2_REG,"PI_DOMAIN2_REG",Reg); break;
		case 0x04600028: MoveVariableToX86reg(&PI_BSD_DOM2_PWD_REG,"PI_BSD_DOM2_PWD_REG",Reg); break;
		case 0x0460002C: MoveVariableToX86reg(&PI_BSD_DOM2_PGS_REG,"PI_BSD_DOM2_PGS_REG",Reg); break;
		case 0x04600030: MoveVariableToX86reg(&PI_BSD_DOM2_RLS_REG,"PI_BSD_DOM2_RLS_REG",Reg); break;
		default:
			MoveConstToX86reg(0,Reg);
			CPU_Message("Compile_LW\nFailed to compile address: %X", Addr);
			if (ShowUnhandledMemory) { DisplayError("Compile_LW\nFailed to compile address: %X",Addr); }
		}
		break;
	case 0x04700000:
		switch (Addr) {
		case 0x0470000C: MoveVariableToX86reg(&RI_SELECT_REG,"RI_SELECT_REG",Reg); break;
		case 0x04700010: MoveVariableToX86reg(&RI_REFRESH_REG,"RI_REFRESH_REG",Reg); break;
		default:
			MoveConstToX86reg(0,Reg);
			CPU_Message("Compile_LW\nFailed to compile address: %X", Addr);
			if (ShowUnhandledMemory) { DisplayError("Compile_LW\nFailed to compile address: %X",Addr); }
		}
		break;
	case 0x04800000:
		switch (Addr) {
		case 0x04800018: MoveVariableToX86reg(&SI_STATUS_REG,"SI_STATUS_REG",Reg); break;
		default:
			MoveConstToX86reg(0,Reg);
			CPU_Message("Compile_LW\nFailed to compile address: %X", Addr);
			if (ShowUnhandledMemory) { DisplayError("Compile_LW\nFailed to compile address: %X",Addr); }
		}
		break;
	case 0x05000000:
		MoveConstToX86reg(0,Reg);
		CPU_Message("Compile_LW\nFailed to compile address: %X", Addr);
		if (ShowUnhandledMemory) { DisplayError("Compile_LW\nFailed to compile address: %X",Addr); }
		break;
	case 0x1FC00000:
		sprintf(VarName,"N64MEM + %X",Addr);
		MoveVariableToX86reg(Addr + N64MEM,VarName,Reg); 
		break;
	default:
		MoveConstToX86reg(((Addr & 0xFFFF) << 16) | (Addr & 0xFFFF),Reg);
		CPU_Message("Compile_LW\nFailed to compile address: %X", Addr);
		if (ShowUnhandledMemory) {
			DisplayError("Compile_LW\nFailed to compile address: %X",Addr);
		}
	}

	return TRUE;
}

BOOL Compile_SB_Const ( BYTE Value, DWORD Addr ) {
	char VarName[100];

	if (!TranslateVaddr(&Addr)) {
		return FALSE;
	}

	switch (Addr & 0xFFF00000) {
	case 0x00000000:
	case 0x00100000: 
	case 0x00200000: 
	case 0x00300000: 
	case 0x00400000: 
	case 0x00500000: 
	case 0x00600000: 
	case 0x00700000: 
		sprintf(VarName,"N64MEM + %X",Addr);
		MoveConstByteToVariable(Value,Addr + N64MEM,VarName); 
		break;
	default:
		CPU_Message("Compile_SB_Const\ntrying to store in %X?", Addr);
		if (ShowUnhandledMemory) { DisplayError("Compile_SB_Const\ntrying to store %X in %X?",Value,Addr); }
	}

	return TRUE;
}

BOOL Compile_SB_Register ( int x86Reg, DWORD Addr ) {
	char VarName[100];

	if (!TranslateVaddr(&Addr)) {
		return FALSE;
	}

	switch (Addr & 0xFFF00000) {
	case 0x00000000:
	case 0x00100000: 
	case 0x00200000: 
	case 0x00300000: 
	case 0x00400000: 
	case 0x00500000: 
	case 0x00600000: 
	case 0x00700000: 
		sprintf(VarName,"N64MEM + %X",Addr);
		MoveX86regByteToVariable(x86Reg,Addr + N64MEM,VarName); 
		break;
	default:
		CPU_Message("Compile_SB_Register\ntrying to store in %X?", Addr);
		if (ShowUnhandledMemory) { DisplayError("Compile_SB_Register\ntrying to store in %X?",Addr); }
	}

	return TRUE;
}

BOOL Compile_SH_Const ( WORD Value, DWORD Addr ) {
	char VarName[100];

	if (!TranslateVaddr(&Addr)) {
		return FALSE;
	}

	switch (Addr & 0xFFF00000) {
	case 0x00000000:
	case 0x00100000: 
	case 0x00200000: 
	case 0x00300000: 
	case 0x00400000: 
	case 0x00500000: 
	case 0x00600000: 
	case 0x00700000: 
		sprintf(VarName,"N64MEM + %X",Addr);
		MoveConstHalfToVariable(Value,Addr + N64MEM,VarName); 
		break;
	default:
		CPU_Message("Compile_SH_Const\ntrying to store in %X?", Addr);
		if (ShowUnhandledMemory) { DisplayError("Compile_SH_Const\ntrying to store %X in %X?",Value,Addr); }
	}

	return TRUE;
}

BOOL Compile_SH_Register ( int x86Reg, DWORD Addr ) {
	char VarName[100];

	if (!TranslateVaddr(&Addr)) {
		return FALSE;
	}

	switch (Addr & 0xFFF00000) {
	case 0x00000000:
	case 0x00100000: 
	case 0x00200000: 
	case 0x00300000: 
	case 0x00400000: 
	case 0x00500000: 
	case 0x00600000: 
	case 0x00700000: 
		sprintf(VarName,"N64MEM + %X",Addr);
		MoveX86regHalfToVariable(x86Reg,Addr + N64MEM,VarName); 
		break;
	default:
		CPU_Message("Compile_SH_Register\ntrying to store in %X?", Addr);
		if (ShowUnhandledMemory) { DisplayError("Compile_SH_Register\ntrying to store in %X?",Addr); }
	}

	return TRUE;
}

static DWORD ValueToWriteToRdramRegister;
static DWORD AddrToWriteToRdramRegister;

static void WriteValueToRdramRegister(void) {
	if (!(AddrToWriteToRdramRegister & 0x80000)) {
		int deviceId = (AddrToWriteToRdramRegister >> 10) & 0x1FE;
		deviceId = (((deviceId >> 0) & 0x3F) << 26) |
			(((deviceId >> 6) & 1) << 23) |
			(((deviceId >> 7) & 0xFF) << 8) |
			(((deviceId >> 15) & 0x1) << 7);

		int deviceIndex = -1;

		for (int i = 0; i < NUMBER_OF_RDRAM_MODULES; ++i) {
			if (deviceId == (RDRAM_DEVICE_ID_REG(i) & 0xF880FF80)) {
				deviceIndex = i;
				break;
			}
		}

		if (deviceIndex != -1) {
			switch (AddrToWriteToRdramRegister & 0x3FF) {
			case 0x000: break; // RDRAM_DEVICE_TYPE_REG
			case 0x004: RDRAM_DEVICE_ID_REG(deviceIndex) = ValueToWriteToRdramRegister; break;
			case 0x008:
				RDRAM_DELAY_REG(deviceIndex) = RDRAM_DELAY_FIXED_VALUE | (ValueToWriteToRdramRegister & ~RDRAM_DELAY_FIXED_VALUE_MASK);
				break;
			case 0x00C:
			{
				DWORD v = ValueToWriteToRdramRegister ^ (RDRAM_MODE_CC | RDRAM_MODE_X2 | RDRAM_MODE_C_MASK);
				RDRAM_MODE_REG(deviceIndex) = v;
			}
			break;
			case 0x010: RDRAM_REF_INTERVAL_REG(deviceIndex) = ValueToWriteToRdramRegister; break;
			case 0x014: RDRAM_REF_ROW_REG(deviceIndex) = ValueToWriteToRdramRegister; break;
			case 0x018: RDRAM_RAS_INTERVAL_REG(deviceIndex) = ValueToWriteToRdramRegister; break;
			case 0x01C: break; // RDRAM_MIN_INTERVAL_REG
			case 0x020: RDRAM_ADDR_SELECT_REG(deviceIndex) = ValueToWriteToRdramRegister; break;
			case 0x024: break; // RDRAM_DEVICE_MANUF_REG is read only

			default:
				LogMessage("WriteValueToRdramRegister to %x", ValueToWriteToRdramRegister);
				return FALSE;
			}
		}
		else {
			LogMessage("rambus device not found at address %x", AddrToWriteToRdramRegister);
		}
	}
	else { // broadcast
		switch (AddrToWriteToRdramRegister & 0x3FF) {
		case 0x004:
			for (int i = 0; i < NUMBER_OF_RDRAM_MODULES; ++i) {
				RDRAM_DEVICE_ID_REG(i) = ValueToWriteToRdramRegister;
			}
			break;
		case 0x008:
			for (int i = 0; i < NUMBER_OF_RDRAM_MODULES; ++i) {
				RDRAM_DELAY_REG(i) = RDRAM_DELAY_FIXED_VALUE | (ValueToWriteToRdramRegister & ~RDRAM_DELAY_FIXED_VALUE_MASK);
			}
			break;
		case 0x00C:
			for (int i = 0; i < NUMBER_OF_RDRAM_MODULES; ++i) {
				RDRAM_MODE_REG(i) = ValueToWriteToRdramRegister ^ RDRAM_MODE_X2;
			}
			break;
		case 0x014:
			for (int i = 0; i < NUMBER_OF_RDRAM_MODULES; ++i) {
				RDRAM_REF_ROW_REG(i) = ValueToWriteToRdramRegister;
			}
			break;

		default:
			LogMessage("WriteValueToRdramRegister to %x", AddrToWriteToRdramRegister);
			return FALSE;
		}
	}
	CheckRdramStatus();
}

BOOL Compile_SW_Const ( DWORD Value, DWORD Addr ) {
	char VarName[100];
	BYTE * Jump;

	if (!TranslateVaddr(&Addr)) {
		return FALSE;
	}

	switch (Addr & 0xFFF00000) {
	case 0x00000000:
	case 0x00100000: 
	case 0x00200000: 
	case 0x00300000: 
	case 0x00400000: 
	case 0x00500000: 
	case 0x00600000: 
	case 0x00700000: 
		sprintf(VarName,"N64MEM + %X",Addr);
		MoveConstToVariable(Value,Addr + N64MEM,VarName); 
		break;
	case 0x03F00000:
		MoveConstToVariable(Value, &ValueToWriteToRdramRegister, "ValueToWriteToRdramRegister");
		MoveConstToVariable(Addr & 0x1FFFFFFF, &AddrToWriteToRdramRegister, "AddrToWriteToRdramRegister");
		Pushad();
		Call_Direct(&WriteValueToRdramRegister, "WriteValueToRdramRegister");
		Popad();
		break;
	case 0x04000000:
		if (Addr < 0x04002000) { 
			sprintf(VarName,"N64MEM + %X",Addr);
			MoveConstToVariable(Value,Addr + N64MEM,VarName); 
			break;
		}
		switch (Addr) {
		case 0x04040000: MoveConstToVariable(Value,&SP_MEM_ADDR_REGW,"SP_MEM_ADDR_REG"); break;
		case 0x04040004: MoveConstToVariable(Value,&SP_DRAM_ADDR_REGW,"SP_DRAM_ADDR_REG"); break;
		case 0x04040008:
			MoveConstToVariable(Value,&SP_RD_LEN_REG,"SP_RD_LEN_REG");
			Pushad();
			Call_Direct(&SP_DMA_READ,"SP_DMA_READ");
			Popad();
			break;
		case 0x04040010: 
			{
				DWORD ModValue;
				ModValue = 0;
				if ( ( Value & SP_CLR_HALT ) != 0 ) { ModValue |= SP_STATUS_HALT; }
				if ( ( Value & SP_CLR_BROKE ) != 0 ) { ModValue |= SP_STATUS_BROKE; }
				if ( ( Value & SP_CLR_SSTEP ) != 0 ) { ModValue |= SP_STATUS_SSTEP; }
				if ( ( Value & SP_CLR_INTR_BREAK ) != 0 ) { ModValue |= SP_STATUS_INTR_BREAK; }
				if ( ( Value & SP_CLR_SIG0 ) != 0 ) { ModValue |= SP_STATUS_SIG0; }
				if ( ( Value & SP_CLR_SIG1 ) != 0 ) { ModValue |= SP_STATUS_SIG1; }
				if ( ( Value & SP_CLR_SIG2 ) != 0 ) { ModValue |= SP_STATUS_SIG2; }
				if ( ( Value & SP_CLR_SIG3 ) != 0 ) { ModValue |= SP_STATUS_SIG3; }
				if ( ( Value & SP_CLR_SIG4 ) != 0 ) { ModValue |= SP_STATUS_SIG4; }
				if ( ( Value & SP_CLR_SIG5 ) != 0 ) { ModValue |= SP_STATUS_SIG5; }
				if ( ( Value & SP_CLR_SIG6 ) != 0 ) { ModValue |= SP_STATUS_SIG6; }
				if ( ( Value & SP_CLR_SIG7 ) != 0 ) { ModValue |= SP_STATUS_SIG7; }
				if (ModValue != 0) {
					AndConstToVariable(~ModValue,&SP_STATUS_REG,"SP_STATUS_REG");
				}

				ModValue = 0;
				if ( ( Value & SP_SET_HALT ) != 0 ) { ModValue |= SP_STATUS_HALT; }
				if ( ( Value & SP_SET_SSTEP ) != 0 ) { ModValue |= SP_STATUS_SSTEP; }
				if ( ( Value & SP_SET_INTR_BREAK ) != 0) { ModValue |= SP_STATUS_INTR_BREAK;  }
				if ( ( Value & SP_SET_SIG0 ) != 0 ) { ModValue |= SP_STATUS_SIG0; }
				if ( ( Value & SP_SET_SIG1 ) != 0 ) { ModValue |= SP_STATUS_SIG1; }
				if ( ( Value & SP_SET_SIG2 ) != 0 ) { ModValue |= SP_STATUS_SIG2; }
				if ( ( Value & SP_SET_SIG3 ) != 0 ) { ModValue |= SP_STATUS_SIG3; }
				if ( ( Value & SP_SET_SIG4 ) != 0 ) { ModValue |= SP_STATUS_SIG4; }
				if ( ( Value & SP_SET_SIG5 ) != 0 ) { ModValue |= SP_STATUS_SIG5; }
				if ( ( Value & SP_SET_SIG6 ) != 0 ) { ModValue |= SP_STATUS_SIG6; }
				if ( ( Value & SP_SET_SIG7 ) != 0 ) { ModValue |= SP_STATUS_SIG7; }
				if (ModValue != 0) {
					OrConstToVariable(ModValue,&SP_STATUS_REG,"SP_STATUS_REG");
				}
				if ( ( Value & SP_SET_SIG0 ) != 0 && AudioSignal ) 
				{ 
					OrConstToVariable(MI_INTR_SP,&MI_INTR_REG,"MI_INTR_REG");
					Pushad();
					Call_Direct(CheckInterrupts,"CheckInterrupts");
					Popad();
				}
				if ( ( Value & SP_CLR_INTR ) != 0) { 
					AndConstToVariable(~MI_INTR_SP,&MI_INTR_REG,"MI_INTR_REG");
					Pushad();
					Call_Direct(RunRsp,"RunRsp");
					Call_Direct(CheckInterrupts,"CheckInterrupts");
					Popad();
				} else {
					Pushad();
					Call_Direct(RunRsp,"RunRsp");
					Popad();
				}
			}
			break;
		case 0x0404001C: MoveConstToVariable(0,&SP_SEMAPHORE_REG,"SP_SEMAPHORE_REG"); break;
		case 0x04080000: MoveConstToVariable(Value & 0xFFC,&SP_PC_REG,"SP_PC_REG"); break;
		default:
			if (ShowUnhandledMemory) { DisplayError("Compile_SW_Const\ntrying to store %X in %X?",Value,Addr); }
		}
		break;
	case 0x04300000: 
		switch (Addr) {
		case 0x04300000: 
			{
				DWORD ModValue;
				ModValue = 0x7F;
				if ( ( Value & MI_CLR_INIT ) != 0 ) { ModValue |= MI_MODE_INIT; }
				if ( ( Value & MI_CLR_EBUS ) != 0 ) { ModValue |= MI_MODE_EBUS; }
				if ( ( Value & MI_CLR_RDRAM ) != 0 ) { ModValue |= MI_MODE_RDRAM; }
				if (ModValue != 0) {
					AndConstToVariable(~ModValue,&MI_MODE_REG,"MI_MODE_REG");
				}

				ModValue = (Value & 0x7F);
				if ( ( Value & MI_SET_INIT ) != 0 ) { ModValue |= MI_MODE_INIT; }
				if ( ( Value & MI_SET_EBUS ) != 0 ) { ModValue |= MI_MODE_EBUS; }
				if ( ( Value & MI_SET_RDRAM ) != 0 ) { ModValue |= MI_MODE_RDRAM; }
				if (ModValue != 0) {
					OrConstToVariable(ModValue,&MI_MODE_REG,"MI_MODE_REG");
				}
				if ( ( Value & MI_CLR_DP_INTR ) != 0 ) { 
					AndConstToVariable(~MI_INTR_DP,&MI_INTR_REG,"MI_INTR_REG");
				}
			}
			break;
		case 0x0430000C: 
			{
				DWORD ModValue;
				ModValue = 0;
				if ( ( Value & MI_INTR_MASK_CLR_SP ) != 0 ) { ModValue |= MI_INTR_MASK_SP; }
				if ( ( Value & MI_INTR_MASK_CLR_SI ) != 0 ) { ModValue |= MI_INTR_MASK_SI; }
				if ( ( Value & MI_INTR_MASK_CLR_AI ) != 0 ) { ModValue |= MI_INTR_MASK_AI; }
				if ( ( Value & MI_INTR_MASK_CLR_VI ) != 0 ) { ModValue |= MI_INTR_MASK_VI; }
				if ( ( Value & MI_INTR_MASK_CLR_PI ) != 0 ) { ModValue |= MI_INTR_MASK_PI; }
				if ( ( Value & MI_INTR_MASK_CLR_DP ) != 0 ) { ModValue |= MI_INTR_MASK_DP; }
				if (ModValue != 0) {
					AndConstToVariable(~ModValue,&MI_INTR_MASK_REG,"MI_INTR_MASK_REG");
				}

				ModValue = 0;
				if ( ( Value & MI_INTR_MASK_SET_SP ) != 0 ) { ModValue |= MI_INTR_MASK_SP; }
				if ( ( Value & MI_INTR_MASK_SET_SI ) != 0 ) { ModValue |= MI_INTR_MASK_SI; }
				if ( ( Value & MI_INTR_MASK_SET_AI ) != 0 ) { ModValue |= MI_INTR_MASK_AI; }
				if ( ( Value & MI_INTR_MASK_SET_VI ) != 0 ) { ModValue |= MI_INTR_MASK_VI; }
				if ( ( Value & MI_INTR_MASK_SET_PI ) != 0 ) { ModValue |= MI_INTR_MASK_PI; }
				if ( ( Value & MI_INTR_MASK_SET_DP ) != 0 ) { ModValue |= MI_INTR_MASK_DP; }
				if (ModValue != 0) {
					OrConstToVariable(ModValue,&MI_INTR_MASK_REG,"MI_INTR_MASK_REG");
				}
			}
			break;
		default:
			if (ShowUnhandledMemory) { DisplayError("Compile_SW_Const\ntrying to store %X in %X?",Value,Addr); }
		}
		break;
	case 0x04400000: 
		switch (Addr) {
		case 0x04400000: 
			if (ViStatusChanged != NULL) {
				CompConstToVariable(Value,&VI_STATUS_REG,"VI_STATUS_REG");
				JeLabel8("Continue",0);
				Jump = RecompPos - 1;
				MoveConstToVariable(Value,&VI_STATUS_REG,"VI_STATUS_REG");
				Pushad();
				Call_Direct(ViStatusChanged,"ViStatusChanged");
				Popad();
				CPU_Message("");
				CPU_Message("      Continue:");
				*((BYTE *)(Jump))=(BYTE)(RecompPos - Jump - 1);
			}
			break;
		case 0x04400004: MoveConstToVariable((Value & 0xFFFFFF),&VI_ORIGIN_REG,"VI_ORIGIN_REG"); break;
		case 0x04400008: 
			if (ViWidthChanged != NULL) {
				CompConstToVariable(Value,&VI_WIDTH_REG,"VI_WIDTH_REG");
				JeLabel8("Continue",0);
				Jump = RecompPos - 1;
				MoveConstToVariable(Value,&VI_WIDTH_REG,"VI_WIDTH_REG");
				Pushad();
				Call_Direct(ViWidthChanged,"ViWidthChanged");
				Popad();
				CPU_Message("");
				CPU_Message("      Continue:");
				*((BYTE *)(Jump))=(BYTE)(RecompPos - Jump - 1);
			}
			break;
		case 0x0440000C: MoveConstToVariable(Value,&VI_INTR_REG,"VI_INTR_REG"); break;
		case 0x04400010: 
			AndConstToVariable(~MI_INTR_VI,&MI_INTR_REG,"MI_INTR_REG");
			Pushad();
			Call_Direct(CheckInterrupts,"CheckInterrupts");
			Popad();
			break;
		case 0x04400014: MoveConstToVariable(Value,&VI_BURST_REG,"VI_BURST_REG"); break;
		case 0x04400018: MoveConstToVariable(Value,&VI_V_SYNC_REG,"VI_V_SYNC_REG"); break;
		case 0x0440001C: MoveConstToVariable(Value,&VI_H_SYNC_REG,"VI_H_SYNC_REG"); break;
		case 0x04400020: MoveConstToVariable(Value,&VI_LEAP_REG,"VI_LEAP_REG"); break;
		case 0x04400024: MoveConstToVariable(Value,&VI_H_START_REG,"VI_H_START_REG"); break;
		case 0x04400028: MoveConstToVariable(Value,&VI_V_START_REG,"VI_V_START_REG"); break;
		case 0x0440002C: MoveConstToVariable(Value,&VI_V_BURST_REG,"VI_V_BURST_REG"); break;
		case 0x04400030: MoveConstToVariable(Value,&VI_X_SCALE_REG,"VI_X_SCALE_REG"); break;
		case 0x04400034: MoveConstToVariable(Value,&VI_Y_SCALE_REG,"VI_Y_SCALE_REG"); break;
		default:
			if (ShowUnhandledMemory) { DisplayError("Compile_SW_Const\ntrying to store %X in %X?",Value,Addr); }
		}
		break;
	case 0x04500000: /* AI registers */
		switch (Addr) {
		case 0x04500000: MoveConstToVariable(Value,&AI_DRAM_ADDR_REG,"AI_DRAM_ADDR_REG"); break;
		case 0x04500004: 
			MoveConstToVariable(Value,&AI_LEN_REG,"AI_LEN_REG");
			Pushad();
			Call_Direct(AiLenChanged,"AiLenChanged");
			Popad();
			break;
		case 0x04500008: MoveConstToVariable((Value & 1),&AI_CONTROL_REG,"AI_CONTROL_REG"); break;
		case 0x0450000C:
			/* Clear Interrupt */; 
			AndConstToVariable(~MI_INTR_AI,&MI_INTR_REG,"MI_INTR_REG");
			AndConstToVariable(~MI_INTR_AI,&AudioIntrReg,"AudioIntrReg");
			Pushad();
			Call_Direct(CheckInterrupts,"CheckInterrupts");
			Popad();
			break;
		case 0x04500010: 
			sprintf(VarName,"N64MEM + %X",Addr);
			MoveConstToVariable(Value,Addr + N64MEM,VarName); 
			break;
		case 0x04500014: MoveConstToVariable(Value,&AI_BITRATE_REG,"AI_BITRATE_REG"); break;
		default:
			sprintf(VarName,"N64MEM + %X",Addr);
			MoveConstToVariable(Value,Addr + N64MEM,VarName); 
			if (ShowUnhandledMemory) { DisplayError("Compile_SW_Const\ntrying to store %X in %X?",Value,Addr); }
		}
		break;
	case 0x04600000:
		switch (Addr) {
		case 0x04600000: MoveConstToVariable(Value,&PI_DRAM_ADDR_REG,"PI_DRAM_ADDR_REG"); break;
		case 0x04600004: MoveConstToVariable(Value,&PI_CART_ADDR_REG,"PI_CART_ADDR_REG"); break;
		case 0x04600008: 
			MoveConstToVariable(Value,&PI_RD_LEN_REG,"PI_RD_LEN_REG");
			Pushad();
			Call_Direct(&PI_DMA_READ,"PI_DMA_READ");
			Popad();
			break;
		case 0x0460000C:
			MoveConstToVariable(Value,&PI_WR_LEN_REG,"PI_WR_LEN_REG");
			Pushad();
			Call_Direct(&PI_DMA_WRITE,"PI_DMA_WRITE");
			Popad();
			break;
		case 0x04600010: 
			if ((Value & PI_CLR_INTR) != 0 ) {
				AndConstToVariable(~MI_INTR_PI,&MI_INTR_REG,"MI_INTR_REG");
				Pushad();
				Call_Direct(CheckInterrupts,"CheckInterrupts");
				Popad();
			}
			break;
		case 0x04600014: MoveConstToVariable((Value & 0xFF),&PI_DOMAIN1_REG,"PI_DOMAIN1_REG"); break;
		case 0x04600018: MoveConstToVariable((Value & 0xFF),&PI_BSD_DOM1_PWD_REG,"PI_BSD_DOM1_PWD_REG"); break;
		case 0x0460001C: MoveConstToVariable((Value & 0xFF),&PI_BSD_DOM1_PGS_REG,"PI_BSD_DOM1_PGS_REG"); break;
		case 0x04600020: MoveConstToVariable((Value & 0xFF),&PI_BSD_DOM1_RLS_REG,"PI_BSD_DOM1_RLS_REG"); break;
		case 0x04600024: MoveConstToVariable((Value & 0xFF),&PI_DOMAIN2_REG,"PI_DOMAIN2_REG"); break;
		default:
			if (ShowUnhandledMemory) { DisplayError("Compile_SW_Const\ntrying to store %X in %X?",Value,Addr); }
		}
		break;
	case 0x04700000:
		switch (Addr) {
		case 0x04700000: MoveConstToVariable(Value,&RI_MODE_REG,"RI_MODE_REG"); break;
		case 0x04700004: MoveConstToVariable(Value,&RI_CONFIG_REG,"RI_CONFIG_REG"); break;
		case 0x04700008: break;
		case 0x0470000C: MoveConstToVariable(Value,&RI_SELECT_REG,"RI_SELECT_REG"); break;
		default:
			if (ShowUnhandledMemory) { DisplayError("Compile_SW_Const\ntrying to store %X in %X?",Value,Addr); }
		}
		break;
	case 0x04800000:
		switch (Addr) {
		case 0x04800000: MoveConstToVariable(Value,&SI_DRAM_ADDR_REG,"SI_DRAM_ADDR_REG"); break;
		case 0x04800004: 			
			MoveConstToVariable(Value,&SI_PIF_ADDR_RD64B_REG,"SI_PIF_ADDR_RD64B_REG");		
			Pushad();
			Call_Direct(&SI_DMA_READ,"SI_DMA_READ");
			Popad();
			break;
		case 0x04800010: 
			MoveConstToVariable(Value,&SI_PIF_ADDR_WR64B_REG,"SI_PIF_ADDR_WR64B_REG");
			Pushad();
			Call_Direct(&SI_DMA_WRITE,"SI_DMA_WRITE");
			Popad();
			break;
		case 0x04800018: 
			AndConstToVariable(~MI_INTR_SI,&MI_INTR_REG,"MI_INTR_REG");
			AndConstToVariable(~SI_STATUS_INTERRUPT,&SI_STATUS_REG,"SI_STATUS_REG");
			Pushad();
			Call_Direct(CheckInterrupts,"CheckInterrupts");
			Popad();
			break;
		default:
			if (ShowUnhandledMemory) { DisplayError("Compile_SW_Const\ntrying to store %X in %X?",Value,Addr); }
		}
		break;
	default:
		if (ShowUnhandledMemory) { DisplayError("Compile_SW_Const\ntrying to store %X in %X?",Value,Addr); }
	}

	return TRUE;
}

BOOL Compile_SW_Register ( int x86Reg, DWORD Addr ) {
	char VarName[100];
	BYTE * Jump;

	if (!TranslateVaddr(&Addr)) {
		return FALSE;
	}

	switch (Addr & 0xFFF00000) {
	case 0x00000000:
	case 0x00100000: 
	case 0x00200000: 
	case 0x00300000: 
	case 0x00400000: 
	case 0x00500000: 
	case 0x00600000: 
	case 0x00700000: 
		sprintf(VarName,"N64MEM + %X",Addr);
		MoveX86regToVariable(x86Reg,Addr + N64MEM,VarName); 
		break;
	case 0x03F00000:
		MoveX86regToVariable(x86Reg, &ValueToWriteToRdramRegister, "ValueToWriteToRdramRegister");
		MoveConstToVariable(Addr & 0x1FFFFFFF, &AddrToWriteToRdramRegister, "AddrToWriteToRdramRegister");
		Pushad();
		Call_Direct(&WriteValueToRdramRegister, "WriteValueToRdramRegister");
		Popad();
		break;
	case 0x04000000: 
		switch (Addr) {
		case 0x04040000: MoveX86regToVariable(x86Reg,&SP_MEM_ADDR_REGW,"SP_MEM_ADDR_REG"); break;
		case 0x04040004: MoveX86regToVariable(x86Reg,&SP_DRAM_ADDR_REGW,"SP_DRAM_ADDR_REG"); break;
		case 0x04040008: 
			MoveX86regToVariable(x86Reg,&SP_RD_LEN_REG,"SP_RD_LEN_REG");
			Pushad();
			Call_Direct(&SP_DMA_READ,"SP_DMA_READ");
			Popad();
			break;
		case 0x0404000C: 
			MoveX86regToVariable(x86Reg,&SP_WR_LEN_REG,"SP_WR_LEN_REG");
			Pushad();
			Call_Direct(&SP_DMA_WRITE,"SP_DMA_WRITE");
			Popad();
			break;
		case 0x04040010: 
			MoveX86regToVariable(x86Reg,&RegModValue,"RegModValue");
			Pushad();
			Call_Direct(ChangeSpStatus,"ChangeSpStatus");
			Popad();
			break;
		case 0x0404001C: MoveConstToVariable(0,&SP_SEMAPHORE_REG,"SP_SEMAPHORE_REG"); break;
		case 0x04080000: 
			MoveX86regToVariable(x86Reg,&SP_PC_REG,"SP_PC_REG");
			AndConstToVariable(0xFFC,&SP_PC_REG,"SP_PC_REG");
			break;
		default:
			if (Addr < 0x04002000) {
				sprintf(VarName,"N64MEM + %X",Addr);
				MoveX86regToVariable(x86Reg,Addr + N64MEM,VarName); 
			} else {
				CPU_Message("    Should be moving %s in to %X ?!?",x86_Name(x86Reg),Addr);
				if (ShowUnhandledMemory) { DisplayError("Compile_SW_Register\ntrying to store at %X?",Addr); }
			}
		}
		break;
	case 0x04100000: 
		switch (Addr) { 
		case 0x0410000C:
			MoveX86regToVariable(x86Reg,&RegModValue,"RegModValue");
			Pushad();
			Call_Direct(ChangeDpcStatus,"ChangeDpcStatus");
			Popad();
			break;
		default:
			CPU_Message("    Should be moving %s in to %X ?!?",x86_Name(x86Reg),Addr);
			sprintf(VarName,"N64MEM + %X",Addr);
			MoveX86regToVariable(x86Reg,Addr + N64MEM,VarName); 
			if (ShowUnhandledMemory) { DisplayError("Compile_SW_Register\ntrying to store at %X?",Addr); }
			break;
		}
		break;
	case 0x04300000: 
		switch (Addr) {
		case 0x04300000: 
			MoveX86regToVariable(x86Reg,&RegModValue,"RegModValue");
			Pushad(); 
			Call_Direct(ChangeMiModeReg,"ChangeMiModeReg");
			Popad();
			break;
		case 0x0430000C: 
			MoveX86regToVariable(x86Reg,&RegModValue,"RegModValue");
			Pushad();
			Call_Direct(ChangeMiIntrMask,"ChangeMiIntrMask");
			Popad();
			break;
		default:
			CPU_Message("    Should be moving %s in to %X ?!?",x86_Name(x86Reg),Addr);
			if (ShowUnhandledMemory) { DisplayError("Compile_SW_Register\ntrying to store at %X?",Addr); }
		}
		break;
	case 0x04400000: 
		switch (Addr) {
		case 0x04400000: 
			if (ViStatusChanged != NULL) {
				CompX86regToVariable(x86Reg,&VI_STATUS_REG,"VI_STATUS_REG");
				JeLabel8("Continue",0);
				Jump = RecompPos - 1;
				MoveX86regToVariable(x86Reg,&VI_STATUS_REG,"VI_STATUS_REG");
				Pushad();
				Call_Direct(ViStatusChanged,"ViStatusChanged");
				Popad();
				CPU_Message("");
				CPU_Message("      Continue:");
				*((BYTE *)(Jump))=(BYTE)(RecompPos - Jump - 1);
			}
			break;
		case 0x04400004: 
			MoveX86regToVariable(x86Reg,&VI_ORIGIN_REG,"VI_ORIGIN_REG"); 
			AndConstToVariable(0xFFFFFF,&VI_ORIGIN_REG,"VI_ORIGIN_REG"); 
			break;
		case 0x04400008: 
			if (ViWidthChanged != NULL) {
				CompX86regToVariable(x86Reg,&VI_WIDTH_REG,"VI_WIDTH_REG");
				JeLabel8("Continue",0);
				Jump = RecompPos - 1;
				MoveX86regToVariable(x86Reg,&VI_WIDTH_REG,"VI_WIDTH_REG");
				Pushad();
				Call_Direct(ViWidthChanged,"ViWidthChanged");
				Popad();
				CPU_Message("");
				CPU_Message("      Continue:");
				*((BYTE *)(Jump))=(BYTE)(RecompPos - Jump - 1);
			}
			break;
		case 0x0440000C: MoveX86regToVariable(x86Reg,&VI_INTR_REG,"VI_INTR_REG"); break;
		case 0x04400010: 
			AndConstToVariable(~MI_INTR_VI,&MI_INTR_REG,"MI_INTR_REG");
			Pushad();
			Call_Direct(CheckInterrupts,"CheckInterrupts");
			Popad();
			break;
		case 0x04400014: MoveX86regToVariable(x86Reg,&VI_BURST_REG,"VI_BURST_REG"); break;
		case 0x04400018: MoveX86regToVariable(x86Reg,&VI_V_SYNC_REG,"VI_V_SYNC_REG"); break;
		case 0x0440001C: MoveX86regToVariable(x86Reg,&VI_H_SYNC_REG,"VI_H_SYNC_REG"); break;
		case 0x04400020: MoveX86regToVariable(x86Reg,&VI_LEAP_REG,"VI_LEAP_REG"); break;
		case 0x04400024: MoveX86regToVariable(x86Reg,&VI_H_START_REG,"VI_H_START_REG"); break;
		case 0x04400028: MoveX86regToVariable(x86Reg,&VI_V_START_REG,"VI_V_START_REG"); break;
		case 0x0440002C: MoveX86regToVariable(x86Reg,&VI_V_BURST_REG,"VI_V_BURST_REG"); break;
		case 0x04400030: MoveX86regToVariable(x86Reg,&VI_X_SCALE_REG,"VI_X_SCALE_REG"); break;
		case 0x04400034: MoveX86regToVariable(x86Reg,&VI_Y_SCALE_REG,"VI_Y_SCALE_REG"); break;
		default:
			CPU_Message("    Should be moving %s in to %X ?!?",x86_Name(x86Reg),Addr);
			if (ShowUnhandledMemory) { DisplayError("Compile_SW_Register\ntrying to store at %X?",Addr); }
		}
		break;
	case 0x04500000: /* AI registers */
		switch (Addr) {
		case 0x04500000: MoveX86regToVariable(x86Reg,&AI_DRAM_ADDR_REG,"AI_DRAM_ADDR_REG"); break;
		case 0x04500004: 
			MoveX86regToVariable(x86Reg,&AI_LEN_REG,"AI_LEN_REG");
			Pushad();
			Call_Direct(AiLenChanged,"AiLenChanged");
			Popad();
			break;
		case 0x04500008: 
			MoveX86regToVariable(x86Reg,&AI_CONTROL_REG,"AI_CONTROL_REG");
			AndConstToVariable(1,&AI_CONTROL_REG,"AI_CONTROL_REG");
		case 0x0450000C:
			/* Clear Interrupt */; 
			AndConstToVariable(~MI_INTR_AI,&MI_INTR_REG,"MI_INTR_REG");
			AndConstToVariable(~MI_INTR_AI,&AudioIntrReg,"AudioIntrReg");
			Pushad();
			Call_Direct(CheckInterrupts,"CheckInterrupts");
			Popad();
			break;
		case 0x04500010: 
			sprintf(VarName,"N64MEM + %X",Addr);
			MoveX86regToVariable(x86Reg,Addr + N64MEM,VarName); 
			break;
		case 0x04500014: MoveX86regToVariable(x86Reg,&AI_BITRATE_REG,"AI_BITRATE_REG"); break;
		default:
			sprintf(VarName,"N64MEM + %X",Addr);
			MoveX86regToVariable(x86Reg,Addr + N64MEM,VarName); 
			if (ShowUnhandledMemory) { DisplayError("Compile_SW_Register\ntrying to store at %X?",Addr); }		}
		break;
	case 0x04600000:
		switch (Addr) {
		case 0x04600000: MoveX86regToVariable(x86Reg,&PI_DRAM_ADDR_REG,"PI_DRAM_ADDR_REG"); break;
		case 0x04600004: MoveX86regToVariable(x86Reg,&PI_CART_ADDR_REG,"PI_CART_ADDR_REG"); break;
		case 0x04600008:
			MoveX86regToVariable(x86Reg,&PI_RD_LEN_REG,"PI_RD_LEN_REG");
			Pushad();
			Call_Direct(&PI_DMA_READ,"PI_DMA_READ");
			Popad();
			break;
		case 0x0460000C:
			MoveX86regToVariable(x86Reg,&PI_WR_LEN_REG,"PI_WR_LEN_REG");
			Pushad();
			Call_Direct(&PI_DMA_WRITE,"PI_DMA_WRITE");
			Popad();
			break;
		case 0x04600010: 
			if (ShowUnhandledMemory) { DisplayError("Compile_SW_Register\ntrying to store at %X?",Addr); }
			AndConstToVariable(~MI_INTR_PI,&MI_INTR_REG,"MI_INTR_REG");
			Pushad();
			Call_Direct(CheckInterrupts,"CheckInterrupts");
			Popad();
			break;
		case 0x04600014: 
			MoveX86regToVariable(x86Reg,&PI_DOMAIN1_REG,"PI_DOMAIN1_REG");
			AndConstToVariable(0xFF,&PI_DOMAIN1_REG,"PI_DOMAIN1_REG"); 
			break;
		case 0x04600018: 
			MoveX86regToVariable(x86Reg,&PI_BSD_DOM1_PWD_REG,"PI_BSD_DOM1_PWD_REG"); 
			AndConstToVariable(0xFF,&PI_BSD_DOM1_PWD_REG,"PI_BSD_DOM1_PWD_REG"); 
			break;
		case 0x0460001C: 
			MoveX86regToVariable(x86Reg,&PI_BSD_DOM1_PGS_REG,"PI_BSD_DOM1_PGS_REG"); 
			AndConstToVariable(0xFF,&PI_BSD_DOM1_PGS_REG,"PI_BSD_DOM1_PGS_REG"); 
			break;
		case 0x04600020: 
			MoveX86regToVariable(x86Reg,&PI_BSD_DOM1_RLS_REG,"PI_BSD_DOM1_RLS_REG"); 
			AndConstToVariable(0xFF,&PI_BSD_DOM1_RLS_REG,"PI_BSD_DOM1_RLS_REG"); 
			break;
		case 0x04600024: 
			MoveX86regToVariable(x86Reg,&PI_DOMAIN2_REG,"PI_DOMAIN2_REG");
			AndConstToVariable(0xFF,&PI_DOMAIN2_REG,"PI_DOMAIN2_REG"); 
			break;
		case 0x04600028:
			MoveX86regToVariable(x86Reg,&PI_BSD_DOM2_PWD_REG,"PI_BSD_DOM2_PWD_REG"); 
			AndConstToVariable(0xFF,&PI_BSD_DOM2_PWD_REG,"PI_BSD_DOM2_PWD_REG"); 
			break;
		case 0x0460002C: 
			MoveX86regToVariable(x86Reg,&PI_BSD_DOM2_PGS_REG,"PI_BSD_DOM2_PGS_REG"); 
			AndConstToVariable(0xFF,&PI_BSD_DOM2_PGS_REG,"PI_BSD_DOM2_PGS_REG"); 
			break;
		case 0x04600030: 
			MoveX86regToVariable(x86Reg,&PI_BSD_DOM2_RLS_REG,"PI_BSD_DOM2_RLS_REG"); 
			AndConstToVariable(0xFF,&PI_BSD_DOM2_RLS_REG,"PI_BSD_DOM2_RLS_REG"); 
			break;
		default:
			CPU_Message("    Should be moving %s in to %X ?!?",x86_Name(x86Reg),Addr);
			if (ShowUnhandledMemory) { DisplayError("Compile_SW_Register\ntrying to store at %X?",Addr); }
		}
		break;
	case 0x04700000:
		switch (Addr) {
		case 0x04700010: MoveX86regToVariable(x86Reg,&RI_REFRESH_REG,"RI_REFRESH_REG"); break;
		default:
			if (ShowUnhandledMemory) { DisplayError("Compile_SW_Register\ntrying to store at %X?",Addr); }
		}
		break;
	case 0x04800000:
		switch (Addr) {
		case 0x04800000: MoveX86regToVariable(x86Reg,&SI_DRAM_ADDR_REG,"SI_DRAM_ADDR_REG"); break;
		case 0x04800004: 
			MoveX86regToVariable(x86Reg,&SI_PIF_ADDR_RD64B_REG,"SI_PIF_ADDR_RD64B_REG"); 
			Pushad();
			Call_Direct(&SI_DMA_READ,"SI_DMA_READ");
			Popad();
			break;
		case 0x04800010: 
			MoveX86regToVariable(x86Reg,&SI_PIF_ADDR_WR64B_REG,"SI_PIF_ADDR_WR64B_REG"); 
			Pushad();
			Call_Direct(&SI_DMA_WRITE,"SI_DMA_WRITE");
			Popad();
			break;
		case 0x04800018: 
			AndConstToVariable(~MI_INTR_SI,&MI_INTR_REG,"MI_INTR_REG");
			AndConstToVariable(~SI_STATUS_INTERRUPT,&SI_STATUS_REG,"SI_STATUS_REG");
			Pushad();
			Call_Direct(CheckInterrupts,"CheckInterrupts");
			Popad();
			break;
		default:
			if (ShowUnhandledMemory) { DisplayError("Compile_SW_Register\ntrying to store at %X?",Addr); }
		}
		break;
	case 0x1FC00000:
		sprintf(VarName,"N64MEM + %X",Addr);
		MoveX86regToVariable(x86Reg,Addr + N64MEM,VarName); 
		break;
	default:
		CPU_Message("    Should be moving %s in to %X ?!?",x86_Name(x86Reg),Addr);
		if (ShowUnhandledMemory) { DisplayError("Compile_SW_Register\ntrying to store in %X?",Addr); }
	}

	return TRUE;
}

int r4300i_CPU_MemoryFilter( DWORD dwExptCode, LPEXCEPTION_POINTERS lpEP) {
	DWORD MemAddress = (char *)lpEP->ExceptionRecord->ExceptionInformation[1] - (char *)N64MEM;
    EXCEPTION_RECORD exRec;
	BYTE * ReadPos, *TypePos;
	void * Reg;
	
	if (dwExptCode != EXCEPTION_ACCESS_VIOLATION) {
		return EXCEPTION_CONTINUE_SEARCH;
	}

	TypePos = (unsigned char *)lpEP->ContextRecord->Eip;
	exRec = *lpEP->ExceptionRecord;

    if ((int)(MemAddress) < 0 || MemAddress > 0x1FFFFFFF) { return EXCEPTION_CONTINUE_SEARCH; }
	
	if (*TypePos == 0xF3 && *(TypePos + 1) == 0xA5) {
		DWORD Start, End, count, OldProtect;
		Start = (lpEP->ContextRecord->Edi - (DWORD)N64MEM);
		End = (Start + (lpEP->ContextRecord->Ecx << 2) - 1);
		if ((int)Start < 0) { 
			if (ShowDebugMessages)
				DisplayError("hmmm.... where does this dma start ?");
			return EXCEPTION_CONTINUE_SEARCH;
		}
#ifdef CFB_READ
		if (Start >= CFBStart && End < CFBEnd) {
			for ( count = Start; count < End; count += 0x1000 ) {
				VirtualProtect(N64MEM+count,4,PAGE_READONLY, &OldProtect);
				if (FrameBufferRead) { FrameBufferRead(count & ~0xFFF); }
			}
			return EXCEPTION_CONTINUE_EXECUTION;
		}	
#endif
		if ((int)End < RdramSize) {
			for ( count = Start; count < End; count += 0x1000 ) {
				if (N64_Blocks.NoOfRDRamBlocks[(count >> 12)] > 0) {
					N64_Blocks.NoOfRDRamBlocks[(count >> 12)] = 0;		
					memset(JumpTable + ((count & 0x00FFFFF0) >> 2),0,0x1000);
					*(DelaySlotTable + count) = NULL;
					if (VirtualProtect(N64MEM + count, 4, PAGE_READWRITE, &OldProtect) == 0) {
						if (ShowDebugMessages)
							DisplayError("Failed to unprotect %X\n1", count);
					}
				}
			}			
			return EXCEPTION_CONTINUE_EXECUTION;
		}
		if (Start >= 0x04000000 && End < 0x04001000) {
			N64_Blocks.NoOfDMEMBlocks = 0;
			memset(JumpTable + (0x04000000 >> 2),0,0x1000);
			*(DelaySlotTable + (0x04000000 >> 12)) = NULL;
			if (VirtualProtect(N64MEM + 0x04000000, 4, PAGE_READWRITE, &OldProtect) == 0) {
				if (ShowDebugMessages)
					DisplayError("Failed to unprotect %X\n7", 0x04000000);
			}
			return EXCEPTION_CONTINUE_EXECUTION;
		}
		if (Start >= 0x04001000 && End < 0x04002000) {
			N64_Blocks.NoOfIMEMBlocks = 0;
			memset(JumpTable + (0x04001000 >> 2),0,0x1000);
			*(DelaySlotTable + (0x04001000 >> 12)) = NULL;
			if (VirtualProtect(N64MEM + 0x04001000, 4, PAGE_READWRITE, &OldProtect) == 0) {
				if (ShowDebugMessages)
					DisplayError("Failed to unprotect %X\n6", 0x04001000);
			}
			return EXCEPTION_CONTINUE_EXECUTION;
		}
		if (ShowDebugMessages)
			DisplayError("hmmm.... where does this dma End ?\nstart: %X\nend:%X\nlocation %X", Start,End,lpEP->ContextRecord->Eip);
		return EXCEPTION_CONTINUE_SEARCH;
	}

	if (*TypePos == 0x0F && *(TypePos + 1) == 0xB6) {
		ReadPos = TypePos + 2;
	} else if (*TypePos == 0x0F && *(TypePos + 1) == 0xB7) {
		ReadPos = TypePos + 2;
	} else if (*TypePos == 0x0F && *(TypePos + 1) == 0xBE) {
		ReadPos = TypePos + 2;
	} else if (*TypePos == 0x0F && *(TypePos + 1) == 0xBF) {
		ReadPos = TypePos + 2;
	} else if (*TypePos == 0x66) {
		ReadPos = TypePos + 2;
	} else {
		ReadPos = TypePos + 1;
	}

	switch ((*ReadPos & 0x38)) {
	case 0x00: Reg = &lpEP->ContextRecord->Eax; break;
	case 0x08: Reg = &lpEP->ContextRecord->Ecx; break; 
	case 0x10: Reg = &lpEP->ContextRecord->Edx; break; 
	case 0x18: Reg = &lpEP->ContextRecord->Ebx; break; 
	case 0x20: Reg = &lpEP->ContextRecord->Esp; break;
	case 0x28: Reg = &lpEP->ContextRecord->Ebp; break;
	case 0x30: Reg = &lpEP->ContextRecord->Esi; break;
	case 0x38: Reg = &lpEP->ContextRecord->Edi; break;
	}

	switch ((*ReadPos & 0xC7)) {
	case 0: ReadPos += 1; break;
	case 1: ReadPos += 1; break;
	case 2: ReadPos += 1; break;
	case 3: ReadPos += 1; break;
	case 4: 
		ReadPos += 1; 
		switch ((*ReadPos & 0xC7)) {
		case 0: ReadPos += 1; break;
		case 1: ReadPos += 1; break;
		case 2: ReadPos += 1; break;
		case 3: ReadPos += 1; break;
		case 6: ReadPos += 1; break;
		case 7: ReadPos += 1; break;
		default:
			//_asm int 3
			break;
		}
		break;
	case 5: ReadPos += 5; break;
	case 6: ReadPos += 1; break;
	case 7: ReadPos += 1; break;
	case 0x40: ReadPos += 2; break;
	case 0x41: ReadPos += 2; break;
	case 0x42: ReadPos += 2; break;
	case 0x43: ReadPos += 2; break;
	case 0x44: ReadPos += 3; break;
	case 0x46: ReadPos += 2; break;
	case 0x47: ReadPos += 2; break;
	case 0x80: ReadPos += 5; break;
	case 0x81: ReadPos += 5; break;
	case 0x82: ReadPos += 5; break;
	case 0x83: ReadPos += 5; break;
	case 0x86: ReadPos += 5; break;
	case 0x87: ReadPos += 5; break;
	default:
		if (ShowDebugMessages) {
			DisplayError("Unknown x86 opcode %X\n\nEip: %X\nMIPS Address: %X", TypePos, lpEP->ContextRecord->Eip, MemAddress);
		}
		return EXCEPTION_CONTINUE_SEARCH;
	}

	switch(*TypePos) {
	case 0x0F:
		switch(*(TypePos + 1)) {
		case 0xB6:
			if (!r4300i_LB_NonMemory(MemAddress,Reg,FALSE)) {
				if (ShowUnhandledMemory) {
					DisplayError("Failed to load byte\n\nEip: %X\nMIPS Address: %X", lpEP->ContextRecord->Eip, MemAddress);
				}
			}
			lpEP->ContextRecord->Eip = (DWORD)ReadPos;
			return EXCEPTION_CONTINUE_EXECUTION;
		case 0xB7:
			if (!r4300i_LH_NonMemory(MemAddress, Reg, FALSE)) {
				if (ShowUnhandledMemory) {
					DisplayError("Failed to load half word\n\nEip: %X\nMIPS Address: %X", lpEP->ContextRecord->Eip, MemAddress);
				}
			}
			lpEP->ContextRecord->Eip = (DWORD)ReadPos;
			return EXCEPTION_CONTINUE_EXECUTION;
		case 0xBE:
			if (!r4300i_LB_NonMemory(MemAddress, Reg, TRUE)) {
				if (ShowUnhandledMemory) {
					DisplayError("Failed to load byte\n\nEip: %X\nMIPS Address: %X", lpEP->ContextRecord->Eip, MemAddress);
				}
			}
			lpEP->ContextRecord->Eip = (DWORD)ReadPos;
			return EXCEPTION_CONTINUE_EXECUTION;
		case 0xBF:
			if (!r4300i_LH_NonMemory(MemAddress, Reg, TRUE)) {
				if (ShowUnhandledMemory) {
					DisplayError("Failed to load half word\n\nEip: %X\nMIPS Address: %X", lpEP->ContextRecord->Eip, MemAddress);
				}
			}
			lpEP->ContextRecord->Eip = (DWORD)ReadPos;
			return EXCEPTION_CONTINUE_EXECUTION;		
		default:
			DisplayError("Unkown x86 opcode %X\n\nEip: %X\nMIPS Address: %X", TypePos, lpEP->ContextRecord->Eip, MemAddress);
			return EXCEPTION_CONTINUE_SEARCH;
		}
		break;
	case 0x66:
		switch(*(TypePos + 1)) {
		case 0x8B:
			if (!r4300i_LH_NonMemory(MemAddress,Reg,FALSE)) {
				if (ShowUnhandledMemory) {
					DisplayError("Failed to half word\n\nEip: %X\nMIPS Address: %X", lpEP->ContextRecord->Eip, MemAddress);
				}
			}
			lpEP->ContextRecord->Eip = (DWORD)ReadPos;
			return EXCEPTION_CONTINUE_EXECUTION;		
		case 0x89:
			if (!r4300i_SH_NonMemory(MemAddress,*(WORD *)Reg)) {
				if (ShowUnhandledMemory) {
					DisplayError("Failed to store half word\n\nEip: %X\nMIPS Address: %X", lpEP->ContextRecord->Eip, MemAddress);
				}
			}
			lpEP->ContextRecord->Eip = (DWORD)ReadPos;
			return EXCEPTION_CONTINUE_EXECUTION;		
		case 0xC7:
			if (Reg != &lpEP->ContextRecord->Eax) { return EXCEPTION_CONTINUE_SEARCH; }
			if (!r4300i_SH_NonMemory(MemAddress,*(WORD *)ReadPos)) {
				if (ShowUnhandledMemory) {
					DisplayError("Failed to store half word\n\nEip: %X\nMIPS Address: %X", lpEP->ContextRecord->Eip, MemAddress);
				}
			}
			lpEP->ContextRecord->Eip = (DWORD)(ReadPos + 2);
			return EXCEPTION_CONTINUE_EXECUTION;		
		default:
			DisplayError("Unkown x86 opcode %X\n\nEip: %X\nMIPS Address: %X", TypePos, lpEP->ContextRecord->Eip, MemAddress);
			return EXCEPTION_CONTINUE_SEARCH;
		}
		break;
	case 0x88: 
		if (!r4300i_SB_NonMemory(MemAddress,*(BYTE *)Reg)) {
			if (ShowUnhandledMemory) {
				DisplayError("Failed to store byte\n\nEip: %X\nMIPS Address: %X", lpEP->ContextRecord->Eip, MemAddress);
			}
		}
		lpEP->ContextRecord->Eip = (DWORD)ReadPos;
		return EXCEPTION_CONTINUE_EXECUTION;
	case 0x8A:
		if (!r4300i_LB_NonMemory(MemAddress, Reg, FALSE)) {
			if (ShowUnhandledMemory) {
				DisplayError("Failed to load byte\n\nEip: %X\nMIPS Address: %X", lpEP->ContextRecord->Eip, MemAddress);
			}
		}
		lpEP->ContextRecord->Eip = (DWORD)ReadPos;
		return EXCEPTION_CONTINUE_EXECUTION;
	case 0x8B:
		if (!r4300i_LW_NonMemory(MemAddress, Reg)) {
			if (ShowUnhandledMemory) {
				DisplayError("Failed to load word\n\nEip: %X\nMIPS Address: %X", lpEP->ContextRecord->Eip, MemAddress);
			}
		}
		lpEP->ContextRecord->Eip = (DWORD)ReadPos;
		return EXCEPTION_CONTINUE_EXECUTION;
	case 0x89:
		if (!r4300i_SW_NonMemory(MemAddress, *(DWORD *)Reg)) {
			if (ShowUnhandledMemory) {
				DisplayError("Failed to store word\n\nEip: %X\nMIPS Address: %X", lpEP->ContextRecord->Eip, MemAddress);
			}
		}
		lpEP->ContextRecord->Eip = (DWORD)ReadPos;
		return EXCEPTION_CONTINUE_EXECUTION;		
	case 0xC6:
		if (Reg != &lpEP->ContextRecord->Eax) { return EXCEPTION_CONTINUE_SEARCH; }
		if (!r4300i_SB_NonMemory(MemAddress,*(BYTE *)ReadPos)) {
			if (ShowUnhandledMemory) {
				DisplayError("Failed to store byte\n\nEip: %X\nMIPS Address: %X", lpEP->ContextRecord->Eip, MemAddress);
			}
		}
		lpEP->ContextRecord->Eip = (DWORD)(ReadPos + 1);
		return EXCEPTION_CONTINUE_EXECUTION;		
	case 0xC7:
		if (Reg != &lpEP->ContextRecord->Eax) { return EXCEPTION_CONTINUE_SEARCH; }
		if (!r4300i_SW_NonMemory(MemAddress,*(DWORD *)ReadPos)) {
			if (ShowUnhandledMemory) {
				DisplayError("Failed to store word\n\nEip: %X\nMIPS Address: %X", lpEP->ContextRecord->Eip, MemAddress);
			}
		}
		lpEP->ContextRecord->Eip = (DWORD)(ReadPos + 4);
		return EXCEPTION_CONTINUE_EXECUTION;		
	default:
		DisplayError("Unkown x86 opcode %X\n\nEip: %X\nMIPS Address: %X", TypePos, lpEP->ContextRecord->Eip, MemAddress);
		return EXCEPTION_CONTINUE_SEARCH;
	}
}

static void checkValueWrittenToRomDecay() {
	if (WrittenToRom) {
		DWORD diff = COUNT_REGISTER - WrittenToRomCount;
		if (COUNT_REGISTER < WrittenToRomCount) {
			diff = COUNT_REGISTER + (0XFFFFFFFF - WrittenToRomCount) + 1;
		}
		if (diff > 3*70*CountPerOp) {
			WrittenToRom = FALSE;
			PI_STATUS_REG &= ~PI_STATUS_IO_BUSY;
		}
	}
}

int r4300i_LB_NonMemory ( DWORD PAddr, DWORD * Value, BOOL SignExtend ) {
	checkValueWrittenToRomDecay();
	if (PAddr >= 0x10000000 && PAddr < 0x13FF0000 ||
		PAddr >= 0x14000000 && PAddr < 0X1FC00000) {
		if (WrittenToRom) {
			*Value = WroteToRom >> 24;
			WrittenToRom = FALSE;
			PI_STATUS_REG &= ~PI_STATUS_IO_BUSY;
			return TRUE;
		}
		if ((PAddr & 2) == 0) { PAddr = (PAddr + 4) ^ 2; }
		if ((PAddr - 0x10000000) < RomFileSize) {
			if (SignExtend) {
				(int)*Value = (char)ROM[PAddr - 0x10000000];
			} else {
				*Value = ROM[PAddr - 0x10000000];
			}
			return TRUE;
		} else {
			*Value = 0;
			return FALSE;
		}
	}

	switch (PAddr & 0xFFF00000) {
	case 0x1FC00000:
		if (PAddr < 0x1FC007C0) {
			*Value = 0;
			return TRUE;
		}
		else if (PAddr < 0x1FC00800) {
			*Value = PIF_Ram[(PAddr - 0x1FC007C0) ^ 3];
			return TRUE;
		}
		else {
			*Value = 0;
			return FALSE;
		}
		break;
	default:
		* Value = 0;
		return FALSE;
		break;
	}
}

BOOL r4300i_LB_VAddr ( MIPS_DWORD VAddr, BYTE * Value ) {
	CheckForWatchPoint(VAddr, WP_READ, sizeof(BYTE));

	DWORD PAddr;
	if (!Addressing64Bits) {
		if (TLB_ReadMap[VAddr.UW[0] >> 12] == 0) { return FALSE; }
		PAddr = (DWORD)TLB_ReadMap[VAddr.UW[0] >> 12] + VAddr.UW[0] - (DWORD)N64MEM;
	}
	else {
		if (!Translate64BitsVAddrToPAddr(VAddr, &PAddr, TRUE)) {
			return FALSE;
		}
	}

	// DRAM, DMEM, and IMEM can all be accessed directly through the host's virtual memory.
	if (PAddr < RdramSize) {
		if (RdramFullyConfigured) {
			*Value = *(BYTE*)(N64MEM + (PAddr ^ 3));
		}
		else {
			BYTE* base = GetBaseRdramAddress(PAddr);
			if (base) {
				*Value = *(BYTE*)(base + (PAddr ^ 3));
			}
			else {
				*Value = 0;
			}
		}
	}
	else if (((PAddr & ~0x03E000) >= 0x04000000 && (PAddr & ~0x03E000) < 0x04004000)) {
		*Value = *(BYTE*)(N64MEM + (((PAddr & ~0x3E000) & ~0x2000) ^ 3));
	}
	else {
		DWORD DValue = 0;
		r4300i_LB_NonMemory(PAddr ^ 3, &DValue, FALSE);
		*Value = (BYTE)DValue;
	}
	return TRUE;
}

BOOL r4300i_LB_VAddr_NonCPU(MIPS_DWORD VAddr, BYTE *Value) {
	DWORD PAddr;
	if (!Addressing64Bits) {
		if (TLB_ReadMap[VAddr.UW[0] >> 12] == 0) { return FALSE; }
		PAddr = (DWORD)TLB_ReadMap[VAddr.UW[0] >> 12] + VAddr.UW[0] - (DWORD)N64MEM;
	}
	else {
		if (!Translate64BitsVAddrToPAddr(VAddr, &PAddr, TRUE)) {
			return FALSE;
		}
	}

	// DRAM, DMEM, and IMEM can all be accessed directly through the host's virtual memory.
	if (PAddr < RdramSize) {
		if (RdramFullyConfigured) {
			*Value = *(BYTE*)(N64MEM + (PAddr ^ 3));
		}
		else {
			BYTE* base = GetBaseRdramAddress(PAddr);
			if (base) {
				*Value = *(BYTE*)(base + (PAddr ^ 3));
			}
			else {
				*Value = 0;
			}
		}
	} else if (((PAddr & ~0x03E000) >= 0x04000000 && (PAddr & ~0x03E000) < 0x04004000)) {
		*Value = *(BYTE*)(N64MEM + (((PAddr & ~0x3E000) & ~0x2000) ^ 3));
	}
	else {
		DWORD DValue = 0;
		if (!r4300i_LB_NonMemory(PAddr^3, &DValue, FALSE)) {
			return FALSE;
		}
		*Value = (BYTE)DValue;
	}
	return TRUE;
}

BOOL r4300i_LD_VAddr ( MIPS_DWORD VAddr, unsigned _int64 * Value, DWORD* outPAddr ) {
	CheckForWatchPoint(VAddr, WP_READ, sizeof(unsigned _int64));

	DWORD PAddr;
	if (!Addressing64Bits) {
		if (TLB_ReadMap[VAddr.UW[0] >> 12] == 0) { return FALSE; }
		PAddr = (DWORD)TLB_ReadMap[VAddr.UW[0] >> 12] + VAddr.UW[0] - (DWORD)N64MEM;
	}
	else {
		if (!Translate64BitsVAddrToPAddr(VAddr, &PAddr, TRUE)) {
			return FALSE;
		}
	}
	if (outPAddr) {
		*outPAddr = PAddr;
	}

	// DRAM, DMEM, and IMEM can all be accessed directly through the host's virtual memory.
	if (PAddr < RdramSize) {
		if (RdramFullyConfigured) {
			*((DWORD*)(Value)+1) = *(DWORD*)(N64MEM + PAddr);
			*((DWORD*)(Value)) = *(DWORD*)(N64MEM + PAddr + 4);
		}
		else {
			BYTE* base = GetBaseRdramAddress(PAddr);
			if (base) {
				*((DWORD*)(Value)+1) = *(DWORD*)(base + PAddr);
				*((DWORD*)(Value)) = *(DWORD*)(base + PAddr + 4);
			}
			else {
				*Value = 0;
			}
		}
	}
	else if (((PAddr & ~0x03E000) >= 0x04000000 && (PAddr & ~0x03E000) < 0x04004000)) {
		*((DWORD*)(Value)+1) = *(DWORD*)(N64MEM + ((PAddr & ~0x3E000) & ~0x2000));
		*((DWORD*)(Value)) = *(DWORD*)(N64MEM + (((PAddr + 4) & ~0x3E000) & ~0x2000));
	}
	else {
		DWORD DValue = 0;
		r4300i_LW_NonMemory(PAddr, &DValue);
		*((DWORD*)(Value)+1) = DValue;
		r4300i_LW_NonMemory(PAddr + 4, &DValue);
		*((DWORD*)(Value)) = DValue;
	}
	return TRUE;
}

int r4300i_LH_NonMemory ( DWORD PAddr, DWORD * Value, int SignExtend ) {
	checkValueWrittenToRomDecay();

	if (PAddr < 0x03F00000) {
		if (PAddr < RdramSize)
			*Value = *(WORD *)(RDRAM + PAddr);
		else Value = 0x0;
		return TRUE;
	}

	if (PAddr >= 0x10000000 && PAddr < 0x13FF0000 ||
		PAddr >= 0x14000000 && PAddr < 0X1FC00000) {
		if (WrittenToRom) {
			*Value = WroteToRom >> 16;
			WrittenToRom = FALSE;
			PI_STATUS_REG &= ~PI_STATUS_IO_BUSY;
			return TRUE;
		}
		if ((PAddr - 0x10000000) < RomFileSize) {
			if ((PAddr & 3) == 0) {
				*Value = *(WORD*)&ROM[(PAddr - 0x10000000 + 6)]; // this is a bug happening on real hardware: read next aligned word
			}
			else {
				*Value = *(WORD*)&ROM[PAddr - 0x10000000];
			}
			return TRUE;
		}
		else {
			*Value = PAddr & 0xFFFF;
			*Value = (*Value << 16) | *Value;
			return FALSE;
		}
	}

	switch (PAddr & 0xFFF00000) {
	case 0x1FC00000:
		if (PAddr < 0x1FC007C0) {
			*Value = 0;
			return TRUE;
		}
		else if (PAddr < 0x1FC00800) {
			DWORD ToSwap = *(DWORD*)(&PIF_Ram[(PAddr & ~3) - 0x1FC007C0]);
			_asm {
				mov eax, ToSwap
				bswap eax
				mov ToSwap, eax
			}
			if ((PAddr&3) == 0) *Value = ToSwap & 0xFFFF;
			else *Value = (ToSwap >> 16) & 0xFFFF;
			return TRUE;
		}
		else {
			*Value = 0;
			return FALSE;
		}
		break;
	default:
		* Value = 0;
		return FALSE;
		break;
	}
}

BOOL r4300i_LH_VAddr ( MIPS_DWORD VAddr, WORD * Value ) {
	CheckForWatchPoint(VAddr, WP_READ, sizeof(WORD));

	DWORD PAddr;
	if (!Addressing64Bits) {
		if (TLB_ReadMap[VAddr.UW[0] >> 12] == 0) { return FALSE; }
		PAddr = (DWORD)TLB_ReadMap[VAddr.UW[0] >> 12] + VAddr.UW[0] - (DWORD)N64MEM;
	}
	else {
		if (!Translate64BitsVAddrToPAddr(VAddr, &PAddr, TRUE)) {
			return FALSE;
		}
	}

	// DRAM, DMEM, and IMEM can all be accessed directly through the host's virtual memory.
	if (PAddr < RdramSize) {
		if (RdramFullyConfigured) {
			*Value = *(WORD*)(N64MEM + (PAddr ^ 2));
		}
		else {
			BYTE* base = GetBaseRdramAddress(PAddr);
			if (base) {
				*Value = *(WORD*)(base + (PAddr ^ 2));
			}
			else {
				*Value = 0;
			}
		}
	}
	else if (((PAddr & ~0x03E000) >= 0x04000000 && (PAddr & ~0x03E000) < 0x04004000)) {
    *Value = *(WORD*)(N64MEM + (((PAddr & ~0x3E000) & ~0x2000) ^ 2));
	}
	else {
		DWORD DValue = 0;
		r4300i_LH_NonMemory(PAddr ^ 2, &DValue, FALSE);
		*Value = (WORD)DValue;
	}
	return TRUE;
}

BOOL r4300i_LH_VAddr_NonCPU ( MIPS_DWORD VAddr, WORD * Value ) {
	DWORD PAddr;
	if (!Addressing64Bits) {
		if (TLB_ReadMap[VAddr.UW[0] >> 12] == 0) { return FALSE; }
		PAddr = (DWORD)TLB_ReadMap[VAddr.UW[0] >> 12] + VAddr.UW[0] - (DWORD)N64MEM;
	}
	else {
		if (!Translate64BitsVAddrToPAddr(VAddr, &PAddr, TRUE)) {
			return FALSE;
		}
	}

	// DRAM, DMEM, and IMEM can all be accessed directly through the host's virtual memory.
	if (PAddr < RdramSize) {
		if (RdramFullyConfigured) {
			*Value = *(WORD*)(N64MEM + (PAddr ^ 2));
		}
		else {
			BYTE* base = GetBaseRdramAddress(PAddr);
			if (base) {
				*Value = *(WORD*)(base + (PAddr ^ 2));
			}
			else {
				*Value = 0;
			}
		}
	}
	else if (((PAddr & ~0x03E000) >= 0x04000000 && (PAddr & ~0x03E000) < 0x04004000)) {
    *Value = *(WORD*)(N64MEM + (((PAddr & ~0x3E000) & ~0x2000) ^ 2));
	}
	else {
		DWORD DValue = 0;
		if (!r4300i_LH_NonMemory(PAddr^2, &DValue, FALSE)) {
			return FALSE;
		}
		*Value = (WORD)DValue;
	}
	return TRUE;
}

int r4300i_LW_NonMemory ( DWORD PAddr, DWORD * Value ) {
	checkValueWrittenToRomDecay();

#ifdef CFB_READ
	if (PAddr >= CFBStart && PAddr < CFBEnd) {
		DWORD OldProtect;
		VirtualProtect(N64MEM+(PAddr & ~0xFFF),0xFFC,PAGE_READONLY, &OldProtect);
		if (FrameBufferRead) { FrameBufferRead(PAddr & ~0xFFF); }
		*Value = *(DWORD *)(N64MEM+PAddr);
		return TRUE;
	}	
#endif

	// N64DD hacked cartridges read from here and take a long time to boot if this isn't handled
	if (PAddr == 0x18000200) {
		*Value = 0x0;
		return TRUE;
	}
	
	if (PAddr < 0x03F00000) {
		if (PAddr < RdramSize) {
			*Value = (DWORD*)(RDRAM + PAddr);
		}
		else *Value = 0x0;
		return TRUE;
	}

	if (PAddr >= 0x06000000 && PAddr < 0x08000000) { 
		if (WrittenToRom) { 
			*Value = WroteToRom;
			WrittenToRom = FALSE;
			return TRUE;
		}
		if ((PAddr - 0x06000000) < RomFileSize) {
			*Value = *(DWORD *)&ROM[PAddr - 0x06000000];
			return TRUE;
		} else {
			*Value = PAddr & 0xFFFF;
			*Value = (*Value << 16) | *Value;
			return FALSE;
		}
	}

	if (PAddr >= 0x10000000 && PAddr < 0x13FF0000 ||
		PAddr >= 0x14000000 && PAddr < 0X1FC00000) {
		if (WrittenToRom) { 
			*Value = WroteToRom;
			//LogMessage("%X: Read crap from Rom %X from %X",PROGRAM_COUNTER,*Value,PAddr);
			WrittenToRom = FALSE;
			PI_STATUS_REG &= ~PI_STATUS_IO_BUSY;
			return TRUE;
		}
		if ((PAddr - 0x10000000) < RomFileSize) {
			*Value = *(DWORD *)&ROM[PAddr - 0x10000000];
			return TRUE;
		} else {
			*Value = PAddr & 0xFFFF;
			*Value = (*Value << 16) | *Value;
			return FALSE;
		}
	}

	switch (PAddr & 0xFFF00000) {
	case 0x03F00000:
		{
			int deviceId = (PAddr >> 10) & 0x1FE;
			deviceId = (((deviceId >> 0) & 0x3F) << 26) |
				(((deviceId >> 6) & 1) << 23) |
				(((deviceId >> 7) & 0xFF) << 8) |
				(((deviceId >> 15) & 0x1) << 7);
				
			int deviceIndex = -1;

			for (int i = 0; i < NUMBER_OF_RDRAM_MODULES; ++i) {
				if (deviceId == (RDRAM_DEVICE_ID_REG(i) & 0xF880FF80)) {
					deviceIndex = i;
					break;
				}
			}
			
			if (deviceIndex != -1) {
				switch (PAddr & 0x3FF) {
				case 0x000: *Value = RDRAM_DEVICE_TYPE_REG(deviceIndex); break;
				case 0x004: *Value = RDRAM_DEVICE_ID_REG(deviceIndex); break;
				case 0x008: *Value = RDRAM_DELAY_REG(deviceIndex); break;
				case 0x00C: *Value = RDRAM_MODE_REG(deviceIndex); break;
				case 0x010: *Value = RDRAM_REF_INTERVAL_REG(deviceIndex); break;
				case 0x014: *Value = RDRAM_REF_ROW_REG(deviceIndex); break;
				case 0x018: *Value = RDRAM_RAS_INTERVAL_REG(deviceIndex); break;
				case 0x01C: *Value = RDRAM_MIN_INTERVAL_REG(deviceIndex); break;
				case 0x020: *Value = RDRAM_ADDR_SELECT_REG(deviceIndex); break;
				case 0x024: *Value = RDRAM_DEVICE_MANUF_REG(deviceIndex); break;
				default:
					LogMessage("LW from %x, PC=%llx", PAddr, PROGRAM_COUNTER.UDW);
					*Value = 0;
					return FALSE;
				}
			}
			else {
				LogMessage("rambus device not found at address %x", PAddr);
			}
		}
		break;
	case 0x04000000:
		switch (PAddr) {
		case 0x04040000: *Value = SP_MEM_ADDR_REG; break;
		case 0x04040004: *Value = SP_DRAM_ADDR_REG; break;
		case 0x04040008: *Value = SP_RD_LEN_REG; break;
		case 0x0404000C: *Value = SP_WR_LEN_REG; break;
		case 0x04040010: *Value = SP_STATUS_REG; break;
		case 0x04040014: *Value = SP_DMA_FULL_REG; break;
		case 0x04040018: *Value = SP_DMA_BUSY_REG; break;
		case 0x0404001C:
			*Value = SP_SEMAPHORE_REG;
			SP_SEMAPHORE_REG = 1; break;
		case 0x04080000: *Value = SP_PC_REG; break;
		default:
			* Value = 0;
			return FALSE;
		}
		break;
	case 0x04100000:
		switch (PAddr) {
		case 0x04100000: *Value = DPC_START_REG; break;
		case 0x04100004: *Value = DPC_END_REG; break;
		case 0x04100008: *Value = DPC_CURRENT_REG; break;
		case 0x0410000C: *Value = DPC_STATUS_REG; break;
		case 0x04100010: *Value = DPC_CLOCK_REG; break;
		case 0x04100014: *Value = DPC_BUFBUSY_REG; break;
		case 0x04100018: *Value = DPC_PIPEBUSY_REG; break;
		case 0x0410001C: *Value = DPC_TMEM_REG; break;
		default:
			* Value = 0;
			return FALSE;
		}
		break;
	case 0x04300000:
		switch (PAddr) {
		case 0x04300000: * Value = MI_MODE_REG; break;
		case 0x04300004: * Value = MI_VERSION_REG; break;
		case 0x04300008: * Value = MI_INTR_REG; break;
		case 0x0430000C: * Value = MI_INTR_MASK_REG; break;
		default:
			* Value = 0;
			return FALSE;
		}
		break;
	case 0x04400000:
		switch (PAddr) {
		case 0x04400000: *Value = VI_STATUS_REG; break;
		case 0x04400004: *Value = VI_ORIGIN_REG; break;
		case 0x04400008: *Value = VI_WIDTH_REG; break;
		case 0x0440000C: *Value = VI_INTR_REG; break;
		case 0x04400010: 
			UpdateCurrentHalfLine();
			*Value = HalfLine;
			break;
		case 0x04400014: *Value = VI_BURST_REG; break;
		case 0x04400018: *Value = VI_V_SYNC_REG; break;
		case 0x0440001C: *Value = VI_H_SYNC_REG; break;
		case 0x04400020: *Value = VI_LEAP_REG; break;
		case 0x04400024: *Value = VI_H_START_REG; break;
		case 0x04400028: *Value = VI_V_START_REG ; break;
		case 0x0440002C: *Value = VI_V_BURST_REG; break;
		case 0x04400030: *Value = VI_X_SCALE_REG; break;
		case 0x04400034: *Value = VI_Y_SCALE_REG; break;
		default:
			* Value = 0;
			return FALSE;
		}
		break;
	case 0x04500000:
		switch (PAddr) {
		case 0x04500004: 
			if (AiReadLength != NULL) {
				*Value = AiReadLength(); 
			} else {
				*Value = 0;
			}
			break;
		case 0x0450000C: 
			*Value = AI_STATUS_REG;
			break;
		default:
			* Value = 0;
			return FALSE;
		}
		break;
	case 0x04600000:
		switch (PAddr) {
		case 0x04600000: *Value = PI_DRAM_ADDR_REG & 0xFFFFFE; break;
		case 0x04600004: *Value = PI_CART_ADDR_REG & 0xFFFFFFFE; break;
		case 0x04600008: *Value = PI_RD_LEN_REG; break;
		case 0x0460000C: *Value = PI_WR_LEN_REG; break;
		case 0x04600010: *Value = PI_STATUS_REG; break;
		case 0x04600014: *Value = PI_DOMAIN1_REG; break;
		case 0x04600018: *Value = PI_BSD_DOM1_PWD_REG; break;
		case 0x0460001C: *Value = PI_BSD_DOM1_PGS_REG; break;
		case 0x04600020: *Value = PI_BSD_DOM1_RLS_REG; break;
		case 0x04600024: *Value = PI_DOMAIN2_REG; break;
		case 0x04600028: *Value = PI_BSD_DOM2_PWD_REG; break;
		case 0x0460002C: *Value = PI_BSD_DOM2_PGS_REG; break;
		case 0x04600030: *Value = PI_BSD_DOM2_RLS_REG; break;
		default:
			* Value = 0;
			return FALSE;
		}
		break;
	case 0x04700000:
		switch (PAddr) {
		case 0x04700000: * Value = RI_MODE_REG; break;
		case 0x04700004: * Value = RI_CONFIG_REG; break;
		case 0x04700008:
			*Value = 0x6 |
				(RI_RERROR_REG & 0x01) | // Ack
				(RI_MODE_REG   & 0x08) | // STOP_R
				(RI_SELECT_REG & 0x10);  // TSEL[0]
			break;
		case 0x0470000C: * Value = RI_SELECT_REG; break;
		case 0x04700010: * Value = RI_REFRESH_REG; break;
		case 0x04700014: * Value = RI_LATENCY_REG; break;
		case 0x04700018: * Value = RI_RERROR_REG; break;
		case 0x0470001C: * Value = RI_BANK_STATUS_REG; break;
		default:
			* Value = 0;
			return FALSE;
		}
		break;
	case 0x04800000:
		switch (PAddr) {
		case 0x04800018: *Value = SI_STATUS_REG; break;
		default:
			*Value = 0;
			return FALSE;
		}
		break;
	case 0x05000000:
		switch (PAddr) {
		//case 0x05000508: *Value = 0x0000000000000000; break;	// EEPROM access? Setting to 0 makes N64DD IPL Rom not display Error 41 (Real-Time Clock Related)
		default:
			*Value = PAddr & 0xFFFF;
			*Value = (*Value << 16) | *Value;
			return FALSE;
		}
		break;
	case 0x08000000:
		if (SaveUsing == Auto) { SaveUsing = FlashRam; }
		if (SaveUsing == Sram) {
			*Value = *(DWORD *)N64MEM+PAddr;
			break;
		}
		if (SaveUsing != FlashRam) { 
			*Value = PAddr & 0xFFFF;
			*Value = (*Value << 16) | *Value;
			return FALSE;
		}
		*Value = ReadFromFlashStatus(PAddr);
		break;
	case 0x13F00000:
		if (PAddr >= 0x13FF0020 && PAddr < 0x13FF0220) {
			*Value =
				(ISViewerBuffer[PAddr - 0x13FF0020 + 0] << 24) |
				(ISViewerBuffer[PAddr - 0x13FF0020 + 1] << 16) |
				(ISViewerBuffer[PAddr - 0x13FF0020 + 2] << 8) |
				(ISViewerBuffer[PAddr - 0x13FF0020 + 3] << 0);
		}
		break;
	case 0x1FC00000:
		if (PAddr < 0x1FC007C0) {
			DWORD ToSwap = *(DWORD *)(&PifRom[PAddr - 0x1FC00000]);
			_asm {
				mov eax,ToSwap
				bswap eax
				mov ToSwap,eax
			}
			* Value = ToSwap;
			return TRUE;
		} else if (PAddr < 0x1FC00800) {
			DWORD ToSwap = *(DWORD *)(&PIF_Ram[PAddr - 0x1FC007C0]);
			_asm {
				mov eax,ToSwap
				bswap eax
				mov ToSwap,eax
			}
			* Value = ToSwap;
			return TRUE;
		} else {
			* Value = 0;
			return FALSE;
		}
		break;
	default:
		*Value = PAddr & 0xFFFF;
		*Value = (*Value << 16) | *Value;
		return FALSE;
		break;
	}
	return TRUE;
}

BOOL r4300i_LW_VAddr ( MIPS_DWORD VAddr, DWORD * Value, DWORD* outPAddr ) {
	CheckForWatchPoint(VAddr, WP_READ, sizeof(DWORD));

	DWORD PAddr;
	if (!Addressing64Bits) {
		if (TLB_ReadMap[VAddr.UW[0] >> 12] == 0) { return FALSE; }
		PAddr = (DWORD)TLB_ReadMap[VAddr.UW[0] >> 12] + VAddr.UW[0] - (DWORD)N64MEM;
	}
	else {
		if (!Translate64BitsVAddrToPAddr(VAddr, &PAddr, TRUE)) {
			return FALSE;
		}
	}
	if (outPAddr) {
		*outPAddr = PAddr;
	}
	
	// DRAM, DMEM, and IMEM can all be accessed directly through the host's virtual memory.
	if (PAddr < RdramSize) {
		if (RdramFullyConfigured) {
			*Value = *(DWORD*)(N64MEM + PAddr);
		}
		else {
			BYTE* base = GetBaseRdramAddress(PAddr);
			if (base) {
				*Value = *(DWORD*)(base + PAddr);
			}
			else {
				*Value = 0;
			}
		}
	}
	else if (((PAddr & ~0x03E000) >= 0x04000000 && (PAddr & ~0x03E000) < 0x04004000)) {
    *Value = *(DWORD*)(N64MEM + ((PAddr& ~0x3E000) & ~0x2000));
	}
	else {
		r4300i_LW_NonMemory(PAddr, Value);
	}
	return TRUE;
}

BOOL r4300i_LW_VAddr_NonCPU ( MIPS_DWORD VAddr, DWORD * Value ) {
	DWORD PAddr;
	if (!Addressing64Bits) {
		if (TLB_ReadMap[VAddr.UW[0] >> 12] == 0) { return FALSE; }
		PAddr = (DWORD)TLB_ReadMap[VAddr.UW[0] >> 12] + VAddr.UW[0] - (DWORD)N64MEM;
	}
	else {
		if (!Translate64BitsVAddrToPAddr(VAddr, &PAddr, TRUE)) {
			return FALSE;
		}
	}

	// DRAM, DMEM, and IMEM can all be accessed directly through the host's virtual memory.
	if (PAddr < RdramSize) {
		if (RdramFullyConfigured) {
			*Value = *(DWORD*)(N64MEM + PAddr);
		}
		else {
			BYTE* base = GetBaseRdramAddress(PAddr);
			if (base) {
				*Value = *(DWORD*)(base + PAddr);
			}
			else {
				*Value = 0;
			}
		}
	} 
	else if (((PAddr & ~0x03E000) >= 0x04000000 && (PAddr & ~0x03E000) < 0x04004000)) {
		*Value = *(DWORD*)(N64MEM + ((PAddr& ~0x3E000) & ~0x2000));
	}
	else if (!r4300i_LW_NonMemory(PAddr, Value)) {
		// TODO: Returning false here is the right thing to do.
		// But it changes the behavior of the memory editor when viewing MMIO registers.
		//return FALSE;
	}
	return TRUE;
}

int r4300i_SB_NonMemory ( DWORD PAddr, BYTE Value ) {
	if (PAddr >= 0x10000000 && PAddr < 0x13FF0000 ||
		PAddr >= 0x14000000 && PAddr < 0X1FC00000) {
		if (!WrittenToRom) {
			WrittenToRom = TRUE;
			WrittenToRomCount = COUNT_REGISTER;
			WroteToRom = RegisterCurrentlyWritten->UW[0] << (8 * (PAddr & 3));
			PI_STATUS_REG |= PI_STATUS_IO_BUSY;
			//LogMessage("%X: Wrote To Rom %X from %X",PROGRAM_COUNTER,Value,PAddr);
		}
		return TRUE;
	}
	switch (PAddr & 0xFFF00000) {
	case 0x00000000:
	case 0x00100000:
	case 0x00200000:
	case 0x00300000:
	case 0x00400000:
	case 0x00500000:
	case 0x00600000:
	case 0x00700000:
#ifdef CFB_READ
		if (PAddr >= CFBStart && PAddr < CFBEnd) {
			DWORD OldProtect;
			VirtualProtect(N64MEM+(PAddr & ~0xFFF),0xFFC,PAGE_READWRITE, &OldProtect);
			*(BYTE *)(N64MEM+PAddr) = Value;
			VirtualProtect(N64MEM+(PAddr & ~0xFFF),0xFFC,OldProtect, &OldProtect);
			DisplayError("FrameBufferWrite");
			if (FrameBufferWrite) { FrameBufferWrite(PAddr,1); }
			break;
		}	
#endif
		if (PAddr < RdramSize) {
			DWORD OldProtect;
			
			if (VirtualProtect((N64MEM + PAddr), 1, PAGE_READWRITE, &OldProtect) == 0) {
				DisplayError("Failed to unprotect %X\n5", PAddr);
			}
			*(BYTE *)(N64MEM+PAddr) = Value;
			if (N64_Blocks.NoOfRDRamBlocks[(PAddr & 0x00FFFFF0) >> 12] == 0) { break; } 
			N64_Blocks.NoOfRDRamBlocks[(PAddr & 0x00FFFFF0) >> 12] = 0;
			memset(JumpTable+((PAddr & 0xFFFFF000) >> 2),0,0x1000);
			*(DelaySlotTable + ((PAddr & 0xFFFFF000) >> 12)) = NULL;
		}
		break;
	case 0x13F00000:
		if (PAddr >= 0x13FF0020 && PAddr < 0x13FF0220) {
			ISViewerBuffer[PAddr - 0x13FF0020] = Value;
		}
		break;
	case 0x1FC00000:
		if (PAddr < 0x1FC007C0) {
			return FALSE;
		}
		else if (PAddr < 0x1FC00800) {
			DWORD v = RegisterCurrentlyWritten->UW[0];
			v <<= 8 * (PAddr & 3);
			_asm {
				mov eax, v
				bswap eax
				mov v, eax
			}
			DWORD alignedAddr = PAddr & ~3;
			*(DWORD*)(&PIF_Ram[alignedAddr - 0x1FC007C0]) = v;
			if (alignedAddr == 0x1FC007FC) {
				PifRamWrite();
			}
			return TRUE;
		}
		return FALSE;
		break;
	default:
		return FALSE;
		break;
	}
	return TRUE;
}

BOOL r4300i_SB_VAddr ( MIPS_DWORD VAddr, MIPS_DWORD* Value ) {
	CheckForWatchPoint(VAddr, WP_WRITE, sizeof(BYTE));

	DWORD PAddr;
	if (!Addressing64Bits) {
		if (TLB_WriteMap[VAddr.UW[0] >> 12] == 0) { return FALSE; }
		PAddr = (DWORD)TLB_WriteMap[VAddr.UW[0] >> 12] + VAddr.UW[0] - (DWORD)N64MEM;
	}
	else {
		if (!Translate64BitsVAddrToPAddr(VAddr, &PAddr, FALSE)) {
			return FALSE;
		}
	}

	// DRAM, DMEM, and IMEM can all be accessed directly through the host's virtual memory.
	if (PAddr < RdramSize) {
		if (RdramFullyConfigured) {
			*(BYTE*)(N64MEM + (PAddr ^ 3)) = Value->UB[0];
		}
		else {
			BYTE* base = GetBaseRdramAddress(PAddr);
			if (base) {
				*(BYTE*)(base + (PAddr ^ 3)) = Value->UB[0];
			}
		}
	}
	else if (((PAddr & ~0x03E000) >= 0x04000000 && (PAddr & ~0x03E000) < 0x04004000)) {
		int tmp = PAddr & 3;
		*(DWORD*)(N64MEM + (((PAddr & ~3) & ~0x3E000) & ~0x2000)) = Value->W[0] << ((3 - tmp) * 8);
	}
	else {
		RegisterCurrentlyWritten = Value;
		r4300i_SB_NonMemory(PAddr ^ 3, Value->UB[0]);
	}
	
	return TRUE;
}

BOOL r4300i_SB_VAddr_NonCPU ( MIPS_DWORD VAddr, BYTE Value ) {
	DWORD PAddr;
	if (!Addressing64Bits) {
		if (TLB_WriteMap[VAddr.UW[0] >> 12] == 0) { return FALSE; }
		PAddr = (DWORD)TLB_WriteMap[VAddr.UW[0] >> 12] + VAddr.UW[0] - (DWORD)N64MEM;
	}
	else {
		if (!Translate64BitsVAddrToPAddr(VAddr, &PAddr, FALSE)) {
			return FALSE;
		}
	}

	// DRAM, DMEM, and IMEM can all be accessed directly through the host's virtual memory.
	if (PAddr < RdramSize) {
		if (RdramFullyConfigured) {
			*(BYTE*)(N64MEM + (PAddr ^ 3)) = Value;
		}
		else {
			BYTE* base = GetBaseRdramAddress(PAddr);
			if (base) {
				*(BYTE*)(base + (PAddr ^ 3)) = Value;
			}
		}
	}
	else if (((PAddr & ~0x03E000) >= 0x04000000 && (PAddr & ~0x03E000) < 0x04004000)) {
		int tmp = PAddr & 3;
		*(DWORD*)(N64MEM + (((PAddr & ~3) & ~0x3E000) & ~0x2000)) = Value << ((3 - tmp) * 8);
	}
	else {
		r4300i_SB_NonMemory(PAddr ^ 3, Value);
	}
	
	return TRUE;
}

int r4300i_SH_NonMemory ( DWORD PAddr, WORD Value ) {
	if (PAddr >= 0x10000000 && PAddr < 0x13FF0000 ||
		PAddr >= 0x14000000 && PAddr < 0X1FC00000) {
		if (!WrittenToRom) {
			WrittenToRom = TRUE;
			WrittenToRomCount = COUNT_REGISTER;
			WroteToRom = Value << 16;
			PI_STATUS_REG |= PI_STATUS_IO_BUSY;
		}
		return TRUE;
	}
	switch (PAddr & 0xFFF00000) {
	case 0x00000000:
	case 0x00100000:
	case 0x00200000:
	case 0x00300000:
	case 0x00400000:
	case 0x00500000:
	case 0x00600000:
	case 0x00700000:
#ifdef CFB_READ
		if (PAddr >= CFBStart && PAddr < CFBEnd) {
			DWORD OldProtect;
			VirtualProtect(N64MEM+(PAddr & ~0xFFF),0xFFC,PAGE_READWRITE, &OldProtect);
			*(WORD *)(N64MEM+PAddr) = Value;
			if (FrameBufferWrite) { FrameBufferWrite(PAddr & ~0xFFF,2); }
			//*(WORD *)(N64MEM+PAddr) = 0xFFFF;
			//VirtualProtect(N64MEM+(PAddr & ~0xFFF),0xFFC,PAGE_NOACCESS, &OldProtect);
			DisplayError("PAddr = %x",PAddr);
			break;
		}	
#endif
		if (PAddr < RdramSize) {
			DWORD OldProtect;
			
			if (VirtualProtect((N64MEM + PAddr), 2, PAGE_READWRITE, &OldProtect) == 0) {
				DisplayError("Failed to unprotect %X\n4", PAddr);
			}
			*(WORD *)(N64MEM+PAddr) = Value;
			if (N64_Blocks.NoOfRDRamBlocks[(PAddr & 0x00FFFFF0) >> 12] == 0) { break; } 
			N64_Blocks.NoOfRDRamBlocks[(PAddr & 0x00FFFFF0) >> 12] = 0;
			memset(JumpTable+((PAddr & 0xFFFFF000) >> 2),0,0x1000);
			*(DelaySlotTable + ((PAddr & 0xFFFFF000) >> 12)) = NULL;
		}
		break;
	case 0x1FC00000:
		if (PAddr < 0x1FC007C0) {
			return FALSE;
		}
		else if (PAddr < 0x1FC00800) {
			DWORD v = RegisterCurrentlyWritten->UW[0];
			if ((PAddr & 3) != 0) v <<= 16;
			_asm {
				mov eax, v
				bswap eax
				mov v, eax
			}
			DWORD alignedAddr = PAddr & ~3;
			*(DWORD*)(&PIF_Ram[alignedAddr - 0x1FC007C0]) = v;
			if (alignedAddr == 0x1FC007FC) {
				PifRamWrite();
			}
			return TRUE;
		}
		return FALSE;
		break;
	default:
		return FALSE;
		break;
	}
	return TRUE;
}

BOOL r4300i_SD_VAddr ( MIPS_DWORD VAddr, unsigned _int64 Value ) {
	CheckForWatchPoint(VAddr, WP_WRITE, sizeof(unsigned _int64));

	DWORD PAddr;
	if (!Addressing64Bits) {
		if (TLB_WriteMap[VAddr.UW[0] >> 12] == 0) { return FALSE; }
		PAddr = (DWORD)TLB_WriteMap[VAddr.UW[0] >> 12] + VAddr.UW[0] - (DWORD)N64MEM;
	}
	else {
		if (!Translate64BitsVAddrToPAddr(VAddr, &PAddr, FALSE)) {
			return FALSE;
		}
	}

	// DRAM, DMEM, and IMEM can all be accessed directly through the host's virtual memory.
	if (PAddr < RdramSize) {
		if (RdramFullyConfigured) {
			*(DWORD*)(N64MEM + PAddr) = *((DWORD*)(&Value) + 1);
			*(DWORD*)(N64MEM + PAddr + 4) = *((DWORD*)(&Value));
		}
		else {
			BYTE* base = GetBaseRdramAddress(PAddr);
			if (base) {
				*(DWORD*)(base + PAddr) = *((DWORD*)(&Value) + 1);
				*(DWORD*)(base + PAddr + 4) = *((DWORD*)(&Value));
			}
		}
	}
	else if (((PAddr & ~0x03E000) >= 0x04000000 && (PAddr & ~0x03E000) < 0x04004000)) {
    *(DWORD*)(N64MEM + ((PAddr & ~0x3E000 & ~0x2000))) = *((DWORD*)(&Value)+1);
	}
	else {
		r4300i_SW_NonMemory(PAddr, *((DWORD*)(&Value) + 1));
		r4300i_SW_NonMemory(PAddr + 4, *((DWORD*)(&Value)));
	}
	return TRUE;
}

BOOL r4300i_SH_VAddr ( MIPS_DWORD VAddr, MIPS_DWORD* Value) {
	CheckForWatchPoint(VAddr, WP_WRITE, sizeof(WORD));

	DWORD PAddr;
	if (!Addressing64Bits) {
		if (TLB_WriteMap[VAddr.UW[0] >> 12] == 0) { return FALSE; }
		PAddr = (DWORD)TLB_WriteMap[VAddr.UW[0] >> 12] + VAddr.UW[0] - (DWORD)N64MEM;
	}
	else {
		if (!Translate64BitsVAddrToPAddr(VAddr, &PAddr, FALSE)) {
			return FALSE;
		}
	}

	// DRAM, DMEM, and IMEM can all be accessed directly through the host's virtual memory.
	if (PAddr < RdramSize) {
		if (RdramFullyConfigured) {
			*(WORD*)(N64MEM + (PAddr ^ 2)) = Value->UHW[0];
		}
		else {
			BYTE* base = GetBaseRdramAddress(PAddr);
			if (base) {
				*(WORD*)(base + (PAddr ^ 2)) = Value->UHW[0];
			}
		}
	}
	else if (((PAddr & ~0x03E000) >= 0x04000000 && (PAddr & ~0x03E000) < 0x04004000)) {
		if ((PAddr & 2) == 0)
			*(WORD*)(N64MEM + ((PAddr & ~0x3E000) & ~0x2000)) = 0;
		else
			*(WORD*)(N64MEM + ((PAddr & ~0x3E000) & ~0x2000)) = Value->UHW[1];
		*(WORD*)(N64MEM + (((PAddr & ~0x3E000) & ~0x2000) ^ 2)) = Value->UHW[0];
	}
	else {
		RegisterCurrentlyWritten = Value;
		r4300i_SH_NonMemory(PAddr ^ 2, Value->UHW[0]);
	}

	return TRUE;
}

BOOL r4300i_SH_VAddr_NonCPU ( MIPS_DWORD VAddr, WORD Value ) {
	DWORD PAddr;
	if (!Addressing64Bits) {
		if (TLB_WriteMap[VAddr.UW[0] >> 12] == 0) { return FALSE; }
		PAddr = (DWORD)TLB_WriteMap[VAddr.UW[0] >> 12] + VAddr.UW[0] - (DWORD)N64MEM;
	}
	else {
		if (!Translate64BitsVAddrToPAddr(VAddr, &PAddr, FALSE)) {
			return FALSE;
		}
	}

	// DRAM, DMEM, and IMEM can all be accessed directly through the host's virtual memory.
	if (PAddr < RdramSize) {
		if (RdramFullyConfigured) {
			*(WORD*)(N64MEM + (PAddr ^ 2)) = Value;
		}
		else {
			BYTE* base = GetBaseRdramAddress(PAddr);
			if (base) {
				*(WORD*)(base + (PAddr ^ 2)) = Value;
			}
		}
	}
  else if (((PAddr & ~0x03E000) >= 0x04000000 && (PAddr & ~0x03E000) < 0x04004000)) {
		if ((PAddr & 2) == 0)
			*(WORD*)(N64MEM + ((PAddr & ~0x3E000) & ~0x2000)) = 0;
		else
			*(WORD*)(N64MEM + ((PAddr & ~0x3E000) & ~0x2000)) = (Value >> 16) & 0xFFFF;
		*(WORD*)(N64MEM + (((PAddr & ~0x3E000) & ~0x2000) ^ 2)) = Value & 0xFFFF;
	}
	else {
		r4300i_SH_NonMemory(PAddr ^ 2, Value);
	}
	
	return TRUE;
}

int r4300i_SW_NonMemory ( DWORD PAddr, DWORD Value ) {
	if (PAddr >= 0x10000000 && PAddr < 0x13FF0000 ||
		PAddr >= 0x14000000 && PAddr < 0X1FC00000) {
		if (!WrittenToRom) {
			WrittenToRom = TRUE;
			WrittenToRomCount = COUNT_REGISTER;
			WroteToRom = Value;
			PI_STATUS_REG |= PI_STATUS_IO_BUSY;
			//LogMessage("%X: Wrote To Rom %X from %X",PROGRAM_COUNTER,Value,PAddr);
		}
		return TRUE;
	}

	switch (PAddr & 0xFFF00000) {
	case 0x00000000:
	case 0x00100000:
	case 0x00200000:
	case 0x00300000:
	case 0x00400000:
	case 0x00500000:
	case 0x00600000:
	case 0x00700000:
#ifdef CFB_READ
		if (PAddr >= CFBStart && PAddr < CFBEnd) {
			DWORD OldProtect;
			VirtualProtect(N64MEM+(PAddr & ~0xFFF),0xFFC,PAGE_READWRITE, &OldProtect);
			*(DWORD *)(N64MEM+PAddr) = Value;
			VirtualProtect(N64MEM+(PAddr & ~0xFFF),0xFFC,OldProtect, &OldProtect);
			DisplayError("FrameBufferWrite %X",PAddr);
			if (FrameBufferWrite) { FrameBufferWrite(PAddr,4); }
			break;
		}	
#endif
		if (PAddr < RdramSize) {
			DWORD OldProtect;
			
			if (VirtualProtect((N64MEM + PAddr), 4, PAGE_READWRITE, &OldProtect) == 0) {
				DisplayError("Failed to unprotect %X\n3", PAddr);
			}
			*(DWORD *)(N64MEM+PAddr) = Value;
			if (N64_Blocks.NoOfRDRamBlocks[(PAddr & 0x00FFFFF0) >> 12] == 0) { break; } 
			N64_Blocks.NoOfRDRamBlocks[(PAddr & 0x00FFFFF0) >> 12] = 0;
			memset(JumpTable+((PAddr & 0xFFFFF000) >> 2),0,0x1000);
			*(DelaySlotTable + ((PAddr & 0xFFFFF000) >> 12)) = NULL;
		}
		break;
	case 0x03F00000:
		if (!(PAddr & 0x80000)) {
			int deviceId = (PAddr >> 10) & 0x1FE;
			deviceId = (((deviceId >> 0) & 0x3F) << 26) |
				(((deviceId >> 6) & 1) << 23) |
				(((deviceId >> 7) & 0xFF) << 8) |
				(((deviceId >> 15) & 0x1) << 7);

			int deviceIndex = -1;

			for (int i = 0; i < NUMBER_OF_RDRAM_MODULES; ++i) {
				if (deviceId == (RDRAM_DEVICE_ID_REG(i) & 0xF880FF80)) {
					deviceIndex = i;
					break;
				}
			}

			if (deviceIndex != -1) {
				switch (PAddr & 0x3FF) {
				case 0x000: break; // RDRAM_DEVICE_TYPE_REG
				case 0x004: RDRAM_DEVICE_ID_REG(deviceIndex) = Value; break;
				case 0x008:
					RDRAM_DELAY_REG(deviceIndex) = RDRAM_DELAY_FIXED_VALUE | (Value & ~RDRAM_DELAY_FIXED_VALUE_MASK);
					break;
				case 0x00C:
					{
						DWORD v = Value ^ (RDRAM_MODE_CC | RDRAM_MODE_X2 | RDRAM_MODE_C_MASK);
						RDRAM_MODE_REG(deviceIndex) = v;
					}
					break;
				case 0x010: RDRAM_REF_INTERVAL_REG(deviceIndex) = Value; break;
				case 0x014: RDRAM_REF_ROW_REG(deviceIndex) = Value; break;
				case 0x018: RDRAM_RAS_INTERVAL_REG(deviceIndex) = Value; break;
				case 0x01C: break; // RDRAM_MIN_INTERVAL_REG
				case 0x020: RDRAM_ADDR_SELECT_REG(deviceIndex) = Value; break;
				case 0x024: break; // RDRAM_DEVICE_MANUF_REG is read only

				default:
					LogMessage("SW to %x", PAddr);
					return FALSE;
				}
			}
			else {
				LogMessage("rambus device not found at address %x", PAddr);
			}
		}
		else { // broadcast
			switch (PAddr & 0x3FF) {
			case 0x004:
				for (int i = 0; i < NUMBER_OF_RDRAM_MODULES; ++i) {
					RDRAM_DEVICE_ID_REG(i) = Value;
				}
				break;
			case 0x008:
				for (int i = 0; i < NUMBER_OF_RDRAM_MODULES; ++i) {
					RDRAM_DELAY_REG(i) = RDRAM_DELAY_FIXED_VALUE | (Value & ~RDRAM_DELAY_FIXED_VALUE_MASK);
				}
				break;
			case 0x00C:
				for (int i = 0; i < NUMBER_OF_RDRAM_MODULES; ++i) {
					RDRAM_MODE_REG(i) = Value ^ RDRAM_MODE_X2;
				}
				break;
			case 0x014:
				for (int i = 0; i < NUMBER_OF_RDRAM_MODULES; ++i) {
					RDRAM_REF_ROW_REG(i) = Value;
				}
				break;

			default:
				LogMessage("SW to %x", PAddr);
				return FALSE;
			}
		}
		CheckRdramStatus();
		break;
	case 0x04000000: 
		if (PAddr < 0x04004000) {
			DWORD OldProtect;
			PAddr &= ~0x2000;
			if (VirtualProtect((N64MEM + PAddr), 4, PAGE_READWRITE, &OldProtect) == 0) {
				DisplayError("Failed to unprotect %X\n2", PAddr);
			}
			*(DWORD *)(N64MEM+PAddr) = Value;
			if (PAddr < 0x04001000) {
				if (N64_Blocks.NoOfDMEMBlocks == 0) { break; } 
				N64_Blocks.NoOfDMEMBlocks = 0;
			} else {
				if (N64_Blocks.NoOfIMEMBlocks == 0) { break; } 
				N64_Blocks.NoOfIMEMBlocks = 0;
			}
			memset(JumpTable+((PAddr & 0xFFFFF000) >> 2),0,0x1000);
			*(DelaySlotTable + ((PAddr & 0xFFFFF000) >> 12)) = NULL;
			return TRUE;
		}
		switch (PAddr) {
		case 0x04040000: SP_MEM_ADDR_REGW = (Value & 0x1FF8); break;
		case 0x04040004: SP_DRAM_ADDR_REGW = (Value & 0xFFFFF8); break;
		case 0x04040008: 
			SP_RD_LEN_REG = (Value & 0xFF8FFFF8); 
			SP_DMA_READ();
			break;
		case 0x0404000C: 
			SP_WR_LEN_REG = (Value & 0xFF8FFFF8); 
			SP_DMA_WRITE();
			break;
		case 0x04040010: 

			switch (Value & (SP_CLR_HALT | SP_SET_HALT))
			{
			case SP_CLR_HALT: SP_STATUS_REG &= ~SP_STATUS_HALT;
				break;
			case SP_SET_HALT: SP_STATUS_REG |= SP_STATUS_HALT;
				break;
			}

			if ( ( Value & SP_CLR_BROKE ) != 0) { SP_STATUS_REG &= ~SP_STATUS_BROKE; }

			//if (ShowDebugMessages)
			//	if ( ( Value & SP_SET_INTR ) != 0) { DisplayError("SP_SET_INTR"); }

			switch (Value & (SP_CLR_INTR | SP_SET_INTR))
			{
			case SP_CLR_INTR: MI_INTR_REG &= ~MI_INTR_SP;
				CheckInterrupts();
				break;
			case SP_SET_INTR: MI_INTR_REG |= MI_INTR_SP;
				CheckInterrupts();
				break;
			}
			//if (ShowDebugMessages)
			//	if ((Value & SP_SET_INTR) != 0) { DisplayError("SP_SET_INTR"); }

			switch (Value & (SP_CLR_SSTEP | SP_SET_SSTEP))
			{
			case SP_CLR_SSTEP: SP_STATUS_REG &= ~SP_STATUS_SSTEP;
				break;
			case SP_SET_SSTEP: SP_STATUS_REG |= SP_STATUS_SSTEP;
				break;
			}

			switch (Value & (SP_CLR_INTR_BREAK | SP_SET_INTR_BREAK))
			{
			case SP_CLR_INTR_BREAK: SP_STATUS_REG &= ~SP_STATUS_INTR_BREAK;
				break;
			case SP_SET_INTR_BREAK: SP_STATUS_REG |= SP_STATUS_INTR_BREAK;
				break;
			}

			switch (Value & (SP_CLR_SIG0 | SP_SET_SIG0))
			{
			case  SP_CLR_SIG0: SP_STATUS_REG &= ~SP_STATUS_SIG0;
				break;
			case  SP_SET_SIG0: SP_STATUS_REG |= SP_STATUS_SIG0;
				if (AudioSignal)
				{
					MI_INTR_REG |= MI_INTR_SP;
					CheckInterrupts();
				}
				break;
			}

			switch (Value & (SP_CLR_SIG1 | SP_SET_SIG1))
			{
			case  SP_CLR_SIG1: SP_STATUS_REG &= ~SP_STATUS_SIG1;
				break;
			case  SP_SET_SIG1: SP_STATUS_REG |= SP_STATUS_SIG1;
				break;
			}

			switch (Value & (SP_CLR_SIG2 | SP_SET_SIG2))
			{
			case  SP_CLR_SIG2: SP_STATUS_REG &= ~SP_STATUS_SIG2;
				break;
			case  SP_SET_SIG2: SP_STATUS_REG |= SP_STATUS_SIG2;
				break;
			}

			switch (Value & (SP_CLR_SIG3 | SP_SET_SIG3))
			{
			case  SP_CLR_SIG3: SP_STATUS_REG &= ~SP_STATUS_SIG3;
				break;
			case  SP_SET_SIG3: SP_STATUS_REG |= SP_STATUS_SIG3;
				break;
			}

			switch (Value & (SP_CLR_SIG4 | SP_SET_SIG4))
			{
			case  SP_CLR_SIG4: SP_STATUS_REG &= ~SP_STATUS_SIG4;
				break;
			case  SP_SET_SIG4: SP_STATUS_REG |= SP_STATUS_SIG4;
				break;
			}

			switch (Value & (SP_CLR_SIG5 | SP_SET_SIG5))
			{
			case  SP_CLR_SIG5: SP_STATUS_REG &= ~SP_STATUS_SIG5;
				break;
			case  SP_SET_SIG5: SP_STATUS_REG |= SP_STATUS_SIG5;
				break;
			}

			switch (Value & (SP_CLR_SIG6 | SP_SET_SIG6))
			{
			case  SP_CLR_SIG6: SP_STATUS_REG &= ~SP_STATUS_SIG6;
				break;
			case  SP_SET_SIG6: SP_STATUS_REG |= SP_STATUS_SIG6;
				break;
			}

			switch (Value & (SP_CLR_SIG7 | SP_SET_SIG7))
			{
			case  SP_CLR_SIG7: SP_STATUS_REG &= ~SP_STATUS_SIG7;
				break;
			case  SP_SET_SIG7: SP_STATUS_REG |= SP_STATUS_SIG7;
				break;
			}


			//if ( ( Value & SP_SET_SIG0 ) != 0 && AudioSignal) 
			//{ 
			//	MI_INTR_REG |= MI_INTR_SP; 
			//	CheckInterrupts();				
			//}

			// Automated Delay RSP / Delay RDP, based on information from Mupen64 Plus HLE RSP source code
			// // If the ucode boot size (DMEM + 0xFED) is not within 0 to 1000 then assume the rsp is being called by the operating system
			// The next bit checks to make sure either the GFX (1) or RSP (2) is being called to do work
			/*if (!(*(DWORD*)(DMEM + 0xFED) >= 0 && *(DWORD*)(DMEM + 0xFED) <= 1000) &&
				(*(DWORD*)(DMEM + 0xFC0) == 1 || *(DWORD*)(DMEM + 0xFC0) == 2)) {
				ChangeTimer(RspTimer, 0x900);
				break;
			}*/

			/*
			if (DelayRDP == TRUE && *( DWORD *)(DMEM + 0xFC0) == 1) {
				ChangeTimer(RspTimer, 0x900);
				break;
			}
			
			if (DelayRSP == TRUE && *( DWORD *)(DMEM + 0xFC0) == 2) {
				ChangeTimer(RspTimer, 0x900);
				break;
			}*/

			RunRsp();
			break;
		case 0x0404001C:
			SP_SEMAPHORE_REG = 0; break;
		case 0x04080000:
			SP_PC_REG = Value & 0xFFC; break;
		default:
			return FALSE;
		}
		break;
	case 0x04100000:
		switch (PAddr) {
		case 0x04100000:
			if ((DPC_STATUS_REG & DPC_STATUS_START_VALID) == 0) {
				DPC_START_REG = Value & 0xFFFFF8;
				DPC_STATUS_REG |= DPC_STATUS_START_VALID;
			}
			break;
		case 0x04100004: 
			DPC_END_REG = Value & 0xFFFFF8;
			if (DPC_STATUS_REG & DPC_STATUS_START_VALID) {
				DPC_CURRENT_REG = DPC_START_REG;
				DPC_STATUS_REG &= ~DPC_STATUS_START_VALID;
			}
			if (ProcessRDPList) { ProcessRDPList(); }
			break;
		//case 0x04100008: DPC_CURRENT_REG = Value; break;
		case 0x0410000C:
			if ( ( Value & DPC_CLR_XBUS_DMEM_DMA ) != 0) { DPC_STATUS_REG &= ~DPC_STATUS_XBUS_DMEM_DMA; }
			if ( ( Value & DPC_SET_XBUS_DMEM_DMA ) != 0) { DPC_STATUS_REG |= DPC_STATUS_XBUS_DMEM_DMA;  }
			if ( ( Value & DPC_CLR_FREEZE ) != 0) { DPC_STATUS_REG &= ~DPC_STATUS_FREEZE; }
			if ( ( Value & DPC_SET_FREEZE ) != 0) { DPC_STATUS_REG |= DPC_STATUS_FREEZE;  }		
			if ( ( Value & DPC_CLR_FLUSH ) != 0) { DPC_STATUS_REG &= ~DPC_STATUS_FLUSH; }
			if ( ( Value & DPC_SET_FLUSH ) != 0) { DPC_STATUS_REG |= DPC_STATUS_FLUSH;  }
			if ( ( Value & DPC_CLR_FREEZE ) != 0) 
			{
				/*if ( ( SP_STATUS_REG & SP_STATUS_HALT ) == 0) 
				{
					if ( ( SP_STATUS_REG & SP_STATUS_BROKE ) == 0 ) 
					{
						RunRsp();
					}
				}*/
				if (ProcessRDPList) { ProcessRDPList(); }
			}
			if (ShowUnhandledMemory) {
				//if ( ( Value & DPC_CLR_TMEM_CTR ) != 0) { DisplayError("RSP: DPC_STATUS_REG: DPC_CLR_TMEM_CTR"); }
				//if ( ( Value & DPC_CLR_PIPE_CTR ) != 0) { DisplayError("RSP: DPC_STATUS_REG: DPC_CLR_PIPE_CTR"); }
				//if ( ( Value & DPC_CLR_CMD_CTR ) != 0) { DisplayError("RSP: DPC_STATUS_REG: DPC_CLR_CMD_CTR"); }
				//if ( ( Value & DPC_CLR_CLOCK_CTR ) != 0) { DisplayError("RSP: DPC_STATUS_REG: DPC_CLR_CLOCK_CTR"); }
			}
			break;
		default:
			return FALSE;
		}
		break;
	case 0x04300000: 
		switch (PAddr) {
		case 0x04300000: 
			MI_MODE_REG &= ~0x7F;
			MI_MODE_REG |= (Value & 0x7F);
			if ( ( Value & MI_CLR_INIT ) != 0 ) { MI_MODE_REG &= ~MI_MODE_INIT; }
			if ( ( Value & MI_SET_INIT ) != 0 ) { MI_MODE_REG |= MI_MODE_INIT; }
			if ( ( Value & MI_CLR_EBUS ) != 0 ) { MI_MODE_REG &= ~MI_MODE_EBUS; }
			if ( ( Value & MI_SET_EBUS ) != 0 ) { MI_MODE_REG |= MI_MODE_EBUS; }
			if ( ( Value & MI_CLR_DP_INTR ) != 0 ) { 
				MI_INTR_REG &= ~MI_INTR_DP; 
				CheckInterrupts();
			}
			if ( ( Value & MI_CLR_RDRAM ) != 0 ) { MI_MODE_REG &= ~MI_MODE_RDRAM; }
			if ( ( Value & MI_SET_RDRAM ) != 0 ) { MI_MODE_REG |= MI_MODE_RDRAM; }
			break;
		case 0x0430000C: 
			if ( ( Value & MI_INTR_MASK_CLR_SP ) != 0 ) { MI_INTR_MASK_REG &= ~MI_INTR_MASK_SP; }
			if ( ( Value & MI_INTR_MASK_SET_SP ) != 0 ) { MI_INTR_MASK_REG |= MI_INTR_MASK_SP; }
			if ( ( Value & MI_INTR_MASK_CLR_SI ) != 0 ) { MI_INTR_MASK_REG &= ~MI_INTR_MASK_SI; }
			if ( ( Value & MI_INTR_MASK_SET_SI ) != 0 ) { MI_INTR_MASK_REG |= MI_INTR_MASK_SI; }
			if ( ( Value & MI_INTR_MASK_CLR_AI ) != 0 ) { MI_INTR_MASK_REG &= ~MI_INTR_MASK_AI; }
			if ( ( Value & MI_INTR_MASK_SET_AI ) != 0 ) { MI_INTR_MASK_REG |= MI_INTR_MASK_AI; }
			if ( ( Value & MI_INTR_MASK_CLR_VI ) != 0 ) { MI_INTR_MASK_REG &= ~MI_INTR_MASK_VI; }
			if ( ( Value & MI_INTR_MASK_SET_VI ) != 0 ) { MI_INTR_MASK_REG |= MI_INTR_MASK_VI; }
			if ( ( Value & MI_INTR_MASK_CLR_PI ) != 0 ) { MI_INTR_MASK_REG &= ~MI_INTR_MASK_PI; }
			if ( ( Value & MI_INTR_MASK_SET_PI ) != 0 ) { MI_INTR_MASK_REG |= MI_INTR_MASK_PI; }
			if ( ( Value & MI_INTR_MASK_CLR_DP ) != 0 ) { MI_INTR_MASK_REG &= ~MI_INTR_MASK_DP; }
			if ( ( Value & MI_INTR_MASK_SET_DP ) != 0 ) { MI_INTR_MASK_REG |= MI_INTR_MASK_DP; }
			CheckInterrupts();
			break;
		default:
			return FALSE;
		}
		break;
	case 0x04400000: 
		switch (PAddr) {
		case 0x04400000: 
			if (VI_STATUS_REG != Value) { 
				VI_STATUS_REG = Value; 
				if (ViStatusChanged != NULL ) { ViStatusChanged(); }
			}
			break;
		case 0x04400004: 
#ifdef CFB_READ
			if (VI_ORIGIN_REG > 0x280) {
				SetFrameBuffer(VI_ORIGIN_REG, (DWORD)(VI_WIDTH_REG * (VI_WIDTH_REG *.75)));
			}
#endif
			VI_ORIGIN_REG = (Value & 0xFFFFFF); 
			//if (UpdateScreen != NULL ) { UpdateScreen(); }
			break;
		case 0x04400008: 
			if (VI_WIDTH_REG != Value) {
				VI_WIDTH_REG = Value; 
				if (ViWidthChanged != NULL ) { ViWidthChanged(); }
			}
			break;
		case 0x0440000C: VI_INTR_REG = Value; break;
		case 0x04400010: 
			MI_INTR_REG &= ~MI_INTR_VI;
			CheckInterrupts();
			break;
		case 0x04400014: VI_BURST_REG = Value; break;
		case 0x04400018: VI_V_SYNC_REG = Value; break;
		case 0x0440001C: VI_H_SYNC_REG = Value; break;
		case 0x04400020: VI_LEAP_REG = Value; break;
		case 0x04400024: VI_H_START_REG = Value; break;
		case 0x04400028: VI_V_START_REG = Value; break;
		case 0x0440002C: VI_V_BURST_REG = Value; break;
		case 0x04400030: VI_X_SCALE_REG = Value; break;
		case 0x04400034: VI_Y_SCALE_REG = Value; break;
		default:
			return FALSE;
		}
		break;
	case 0x04500000: 
		switch (PAddr) {
		case 0x04500000: AI_DRAM_ADDR_REG = (Value & 0xFFFFF8); break;
		case 0x04500004: 
			AI_LEN_REG = Value;  
			if (AiLenChanged != NULL) { AiLenChanged(); }
			break;
		case 0x04500008: AI_CONTROL_REG = (Value & 1); break;
		case 0x0450000C:
			/* Clear Interrupt */; 
			MI_INTR_REG &= ~MI_INTR_AI;
			AudioIntrReg &= ~MI_INTR_AI;
			CheckInterrupts();
			break;
		case 0x04500010: 
			AI_DACRATE_REG = Value;  
			if (AiDacrateChanged != NULL) { AiDacrateChanged(SYSTEM_NTSC); }
			break;
		case 0x04500014:  AI_BITRATE_REG = Value; break;
		default:
			return FALSE;
		}
		break;
	case 0x04600000: 
		switch (PAddr) {
		case 0x04600000: PI_DRAM_ADDR_REG = (Value & 0xFFFFFE); break;
		case 0x04600004: PI_CART_ADDR_REG = Value; break;
		case 0x04600008: 
			PI_RD_LEN_REG = Value;
			PI_DMA_READ();
			break;
		case 0x0460000C: 
			PI_WR_LEN_REG = Value;
			PI_DMA_WRITE();
			break;
		case 0x04600010:
			//if ((Value & PI_SET_RESET) != 0 ) { DisplayError("reset Controller"); }
			if ((Value & PI_CLR_INTR) != 0 ) {
				MI_INTR_REG &= ~MI_INTR_PI;
				CheckInterrupts();
			}
			break;
		case 0x04600014: PI_DOMAIN1_REG = (Value & 0xFF); break; 
		case 0x04600018: PI_BSD_DOM1_PWD_REG = (Value & 0xFF); break; 
		case 0x0460001C: PI_BSD_DOM1_PGS_REG = (Value & 0xFF); break; 
		case 0x04600020: PI_BSD_DOM1_RLS_REG = (Value & 0xFF); break; 
		case 0x04600024: PI_DOMAIN2_REG = (Value & 0xFF); break;
		case 0x04600028: PI_BSD_DOM2_PWD_REG = (Value & 0xFF); break;
		case 0x0460002C: PI_BSD_DOM2_PGS_REG = (Value & 0xFF); break;
		case 0x04600030: PI_BSD_DOM2_RLS_REG = (Value & 0xFF); break;
		default:
			return FALSE;
		}
		break;
	case 0x04700000:
		switch (PAddr) {
		case 0x04700000:
			RI_MODE_REG = Value;
			if ((RI_MODE_REG & 3) == 0) { // reset mode
				for (int i = 0; i < NUMBER_OF_RDRAM_MODULES; ++i) {
					RDRAM_MODE_REG(i) &= ~RDRAM_MODE_DE;
				}
			}
			break;
		case 0x04700004: RI_CONFIG_REG = Value; break;
		case 0x04700008: break; // RI_CURRENT_LOAD_REG
		case 0x0470000C: RI_SELECT_REG = Value; break;
		case 0x04700010: RI_REFRESH_REG = Value; break;
		case 0x04700014: RI_LATENCY_REG = Value; break;
		case 0x04700018: RI_RERROR_REG = Value; break;
		case 0x0470001C: RI_BANK_STATUS_REG = Value; break;
		default:
			return FALSE;
		}
		CheckRdramStatus();
		break;
	case 0x04800000:
		switch (PAddr) {
		case 0x04800000: SI_DRAM_ADDR_REG = Value; break;
		case 0x04800004: 
			SI_PIF_ADDR_RD64B_REG = Value; 
			SI_DMA_READ ();
			break;
		case 0x04800010: 
			SI_PIF_ADDR_WR64B_REG = Value; 
			SI_DMA_WRITE();
			break;
		case 0x04800018: 
			MI_INTR_REG &= ~MI_INTR_SI; 
			SI_STATUS_REG &= ~SI_STATUS_INTERRUPT;
			CheckInterrupts();
			break;
		default:
			return FALSE;
		}
		break;
	case 0x08000000:
		if (SaveUsing == Sram) {
			*(N64MEM + PAddr) = Value;
			break;
		}
		if (PAddr != 0x08010000) { return FALSE; }
		if (SaveUsing == Auto) { SaveUsing = FlashRam; }
		if (SaveUsing != FlashRam) { return TRUE; }
		WriteToFlashCommand(Value);
		break;
	case 0x13F00000:
		if (PAddr == 0x13FF0014) {
			if (Value < 0x221 && HaveDebugger && LogOptions.LogISViewer) {
				for (DWORD i = 0; i < Value; ++i) {
					ISViewerTempBuffer[ISViewerTempBufferLength++] = ISViewerBuffer[i];
				}
				if (ISViewerTempBuffer[ISViewerTempBufferLength - 1] == '\n' || ISViewerTempBufferLength > 0x1000) {
					LogMessage("ISViewer:%s", ISViewerTempBuffer);
					memset(ISViewerTempBuffer, 0, sizeof(ISViewerTempBuffer));
					ISViewerTempBufferLength = 0;
				}
			}
		} else if (PAddr >= 0x13FF0020 && PAddr < 0x13FF0220) {
			ISViewerBuffer[PAddr - 0x13FF0020 + 0] = (Value >> 24) & 0XFF;
			ISViewerBuffer[PAddr - 0x13FF0020 + 1] = (Value >> 16) & 0XFF;
			ISViewerBuffer[PAddr - 0x13FF0020 + 2] = (Value >>  8) & 0XFF;
			ISViewerBuffer[PAddr - 0x13FF0020 + 3] = (Value >>  0) & 0XFF;
		}
		break;
	case 0x1FC00000:
		if (PAddr < 0x1FC007C0) {
			return FALSE;
		} else if (PAddr < 0x1FC00800) {
			_asm {
				mov eax,Value
				bswap eax
				mov Value,eax
			}
			*(DWORD *)(&PIF_Ram[PAddr - 0x1FC007C0]) = Value;
			if (PAddr == 0x1FC007FC) {
				PifRamWrite();
			}
			return TRUE;
		}
		return FALSE;
		break;
	default:
		return FALSE;
		break;
	}
	return TRUE;
}

BOOL r4300i_SW_VAddr ( MIPS_DWORD VAddr, DWORD Value ) {
	CheckForWatchPoint(VAddr, WP_WRITE, sizeof(DWORD));

	DWORD PAddr;
	if (!Addressing64Bits) {
		if (TLB_WriteMap[VAddr.UW[0] >> 12] == 0) { return FALSE; }
		PAddr = (DWORD)TLB_WriteMap[VAddr.UW[0] >> 12] + VAddr.UW[0] - (DWORD)N64MEM;
	}
	else {
		if (!Translate64BitsVAddrToPAddr(VAddr, &PAddr, FALSE)) {
			return FALSE;
		}
	}

	// DRAM, DMEM, and IMEM can all be accessed directly through the host's virtual memory.
	if (PAddr < RdramSize) {
		if (RdramFullyConfigured) {
			*(DWORD*)(N64MEM + PAddr) = Value;
		}
		else {
			BYTE* base = GetBaseRdramAddress(PAddr);
			if (base) {
				*(DWORD*)(base + PAddr) = Value;
			}
		}
	}
	else if (((PAddr & ~0x03E000) >= 0x04000000 && (PAddr & ~0x03E000) < 0x04004000)) {
    *(DWORD*)(N64MEM + ((PAddr & ~0x3E000) & ~0x2000)) = Value;
	}
	else
	{
		r4300i_SW_NonMemory(PAddr, Value);
	}

	return TRUE;
}

BOOL r4300i_SW_VAddr_NonCPU ( MIPS_DWORD VAddr, DWORD Value ) {
	DWORD PAddr;
	if (!Addressing64Bits) {
		if (TLB_WriteMap[VAddr.UW[0] >> 12] == 0) { return FALSE; }
		PAddr = (DWORD)TLB_WriteMap[VAddr.UW[0] >> 12] + VAddr.UW[0] - (DWORD)N64MEM;
	}
	else {
		if (!Translate64BitsVAddrToPAddr(VAddr, &PAddr, FALSE)) {
			return FALSE;
		}
	}

	// DRAM, DMEM, and IMEM can all be accessed directly through the host's virtual memory.
	if (PAddr < RdramSize) {
		if (RdramFullyConfigured) {
			*(DWORD*)(N64MEM + PAddr) = Value;
		}
		else {
			BYTE* base = GetBaseRdramAddress(PAddr);
			if (base) {
				*(DWORD*)(base + PAddr) = Value;
			}
		}
	}
  else if (((PAddr & ~0x03E000) >= 0x04000000 && (PAddr & ~0x03E000) < 0x04004000)) {
    *(DWORD*)(N64MEM + ((PAddr & ~0x3E000) & ~0x2000)) = Value;
	}
	else {
		r4300i_SW_NonMemory(PAddr, Value);
	}
	
	return TRUE;
}

void Release_Memory ( void ) {
	FreeSyncMemory();
	if (OrigMem != NULL) { VirtualFree(OrigMem,0,MEM_RELEASE); }
	CloseMappedRomFile();
	VirtualFree( TLB_ReadMap, 0 , MEM_RELEASE);
	VirtualFree( TLB_WriteMap, 0 , MEM_RELEASE);
	VirtualFree( N64MEM, 0 , MEM_RELEASE);
	VirtualFree( DelaySlotTable, 0 , MEM_RELEASE);
	VirtualFree( SyncMemory, 0 , MEM_RELEASE);
	VirtualFree( JumpTable, 0 , MEM_RELEASE);
	VirtualFree( RecompCode, 0 , MEM_RELEASE);
}

void ResetMemoryStack (BLOCK_SECTION * Section) {
	int x86reg, TempReg;

	CPU_Message("    ResetMemoryStack");
	x86reg = Map_MemoryStack(Section, FALSE);
	if (x86reg >= 0) { UnMap_X86reg(Section,x86reg); }

	x86reg = Map_TempReg(Section,x86_Any, 29, FALSE);
	if (UseTlb) {	
	    TempReg = Map_TempReg(Section,x86_Any,-1,FALSE);
		MoveX86RegToX86Reg(x86reg,TempReg);
		ShiftRightUnsignImmed(TempReg,12);
		MoveVariableDispToX86Reg(TLB_ReadMap,"TLB_ReadMap",TempReg,TempReg,4);
		AddX86RegToX86Reg(x86reg,TempReg);
	} else {
		AndConstToX86Reg(x86reg,0x1FFFFFFF);
		AddConstToX86Reg(x86reg,(DWORD)N64MEM);
	}
	MoveX86regToVariable(x86reg, &MemoryStack, "MemoryStack");
}

void ResetRecompCode (void) {
	DWORD count, OldProtect;
	RecompPos = RecompCode;
	if (SelfModCheck == ModCode_ChangeMemory) {
		DWORD count, PAddr, Value;

		for (count = 0; count < TargetIndex; count++) {
			PAddr = OrigMem[(WORD)(count)].PAddr;
			Value = *(DWORD *)(N64MEM + PAddr);
			if ( ((Value >> 16) == 0x7C7C) && ((Value & 0xFFFF) == count)) {
				*(DWORD *)(N64MEM + PAddr) = OrigMem[(WORD)(count)].OriginalValue;
			} 			
		}
	}
	TargetIndex = 0;
	
	//Jump Table
	for (count = 0; count < (RdramSize >> 12); count ++ ) {
		if (N64_Blocks.NoOfRDRamBlocks[count] > 0) {
			N64_Blocks.NoOfRDRamBlocks[count] = 0;		
			memset(JumpTable + (count << 10),0,0x1000);
			*(DelaySlotTable + count) = NULL;

			if (VirtualProtect((N64MEM + (count << 12)), 4, PAGE_READWRITE, &OldProtect) == 0) {
				DisplayError("Failed to unprotect %X\n1", (count << 12));
			}
		}			
	}
	
	if (N64_Blocks.NoOfDMEMBlocks > 0) {
		N64_Blocks.NoOfDMEMBlocks = 0;
		memset(JumpTable + (0x04000000 >> 2),0,0x1000);
		*(DelaySlotTable + (0x04000000 >> 12)) = NULL;
		if (VirtualProtect((N64MEM + 0x04000000), 4, PAGE_READWRITE, &OldProtect) == 0) {
			DisplayError("Failed to unprotect %X\n0", 0x04000000);
		}
	}
	if (N64_Blocks.NoOfIMEMBlocks > 0) {
		N64_Blocks.NoOfIMEMBlocks = 0;
		memset(JumpTable + (0x04001000 >> 2),0,0x1000);
		*(DelaySlotTable + (0x04001000 >> 12)) = NULL;
		if (VirtualProtect((N64MEM + 0x04001000), 4, PAGE_READWRITE, &OldProtect) == 0) {
			DisplayError("Failed to unprotect %X\n4", 0x04001000);
		}
	}	
//	if (N64_Blocks.NoOfPifRomBlocks > 0) {
//		N64_Blocks.NoOfPifRomBlocks = 0;
//		memset(JumpTable + (0x1FC00000 >> 2),0,0x1000);
//	}
}

void UpdateCPUMode() {
	if (((STATUS_REGISTER & STATUS_KX) != 0 && (STATUS_REGISTER & STATUS_KSU) == STATUS_KERNEL) ||
		((STATUS_REGISTER & STATUS_UX) != 0 && (STATUS_REGISTER & STATUS_KSU) == STATUS_USER)) {
		Addressing64Bits = 1;
	}
	else {
		Addressing64Bits = 0;
	}
	KernelMode = (STATUS_REGISTER & STATUS_KSU) == STATUS_KERNEL || (STATUS_REGISTER & STATUS_EXL) || (STATUS_REGISTER & STATUS_ERL);
}

BOOL IsValidAddress(MIPS_DWORD address) {
	if (Addressing64Bits) {
		switch ((address.UDW >> 60) & 0xF) {
		case 0x0:
			if (address.UW[1] < 0x100) {
				return TRUE;
			}
			return FALSE;
		case 0x4:
			if (KernelMode && address.UW[1] < 0x40000100) {
				return TRUE;
			}
			return FALSE;
		case 0x9:
			if (KernelMode) {
				if (address.UW[1] == 0x90000000 ||
					address.UW[1] == 0x98000000) {
					return TRUE;
				}
			}
			return FALSE;
		case 0xC:
			if (KernelMode && address.UDW < 0xC00000FF80000000LL) {
				return TRUE;
			}
			return FALSE;
		case 0xF:
			if (!KernelMode) {
				return FALSE;
			}
			if (address.UW[1] != 0xFFFFFFFF) {
				return FALSE;
			}
			if (address.UW[1] < 0x80000000) {
				return FALSE;
			}
			return TRUE;
		default:
			LogMessage("Is it a valid address ? %llx", address.UDW);
			return FALSE;
		}
	}
	else {
		if (!KernelMode && (address.UW[0] & 0x80000000) != 0) {
			return FALSE;
		}
		return IsSignExtended(address);
	}
}

#define CURRENT_THRESHOLD 0x00808000 // below this value, the rdram chip doesn't enough current

void CheckRdramStatus() {
	RdramFullyConfigured = TRUE;
	if (CPU_Type != CPU_Interpreter) {
		return;
	}
	for (int i = 0; i < NUMBER_OF_RDRAM_MODULES; ++i) {
		if ((RDRAM_MODE_REG(i) & RDRAM_MODE_DE) == 0) {
			RdramFullyConfigured = FALSE;
			return;
		}
		if ((RDRAM_MODE_REG(i) & CURRENT_THRESHOLD) == 0) {
			RdramFullyConfigured = FALSE;
			return;
		}
		if (((RDRAM_DEVICE_ID_REG(i) >> 26) & 7) != (i * 2)) {
			RdramFullyConfigured = FALSE;
			return;
		}
	}
}

BYTE* GetBaseRdramAddress(DWORD PAddr) {
	int deviceId = (PAddr >> 21) & 3;
	if ((RDRAM_MODE_REG(deviceId) & RDRAM_MODE_DE) == 0) {
		return NULL;
	}
	if ((RDRAM_MODE_REG(deviceId) & CURRENT_THRESHOLD) == 0) {
		return NULL;
	}
	if (((RDRAM_DEVICE_ID_REG(deviceId) >> 26) & 7) != (deviceId * 2)) {
		return NULL;
	}
	return N64MEM;
}
