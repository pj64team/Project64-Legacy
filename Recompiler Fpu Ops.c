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
#include <Windows.h>
#include <stdio.h>
#include "main.h"
#include "cpu.h"
#include "x86.h"
#include "debugger.h"

WORD FPU_RoundingMode = 0x0000;//_RC_NEAR
char Name[50];

void ChangeDefaultRoundingModel (void) {
	switch((FPCR[31] & 3)) {
	case 0: FPU_RoundingMode = 0x0000; break; //_RC_NEAR
	case 1: FPU_RoundingMode = 0x0C00; break; //_RC_CHOP
	case 2: FPU_RoundingMode = 0x0800; break; //_RC_UP
	case 3: FPU_RoundingMode = 0x0400; break; //_RC_UP
	}
}

void CompileCop1Test (BLOCK_SECTION * Section) {
	if (FpuBeenUsed) { return; }
	TestVariable(STATUS_CU1,&STATUS_REGISTER,"STATUS_REGISTER");
	CompileExit(Section->CompilePC,Section->RegWorking,COP1_Unuseable,FALSE,JeLabel32);
	FpuBeenUsed = TRUE;
}

/********************** Load/store functions ************************/
void Compile_R4300i_LWC1 (BLOCK_SECTION * Section) {
	DWORD TempReg1, TempReg2, TempReg3;
	char Name[50];
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));

	CompileCop1Test(Section);
	if ((Opcode.FP.ft & 1) != 0) {
		if (RegInStack(Section,Opcode.FP.ft-1,FPU_Double) || RegInStack(Section,Opcode.FP.ft-1,FPU_Qword)) {
			UnMap_FPR(Section,Opcode.FP.ft-1,TRUE);
		}
	}
	if (RegInStack(Section,Opcode.FP.ft,FPU_Double) || RegInStack(Section,Opcode.FP.ft,FPU_Qword)) {
		UnMap_FPR(Section,Opcode.FP.ft,TRUE);
	} else {
		UnMap_FPR(Section,Opcode.FP.ft,FALSE);
	}
	if (IsConst(Opcode.IMM.base)) { 
		DWORD Address = MipsRegLo(Opcode.IMM.base) + (short)Opcode.IMM.immediate;

		TempReg1 = Map_TempReg(Section,x86_Any,-1,FALSE);
		if (Compile_LW(TempReg1, Address)) {
			TempReg2 = Map_TempReg(Section, x86_Any, -1, FALSE);
			sprintf(Name, "FPRFloatLocation[%d]", Opcode.FP.ft);
			MoveVariableToX86reg(&FPRFloatLoadStoreLocation[Opcode.FP.ft], Name, TempReg2);
			MoveX86regToX86Pointer(TempReg1, TempReg2);
			return;
		}

		// Deoptimization: Address translation for a constant virtual address failed.
		// Unmap the base register to force it to be loaded from memory by the following codegen.
		UnMap_GPR(Section, Opcode.IMM.base, TRUE);
	}
	if (IsMapped(Opcode.IMM.base) && Opcode.IMM.immediate == 0) { 
		if (UseTlb) {
			ProtectGPR(Section,Opcode.IMM.base);
			TempReg1 = MipsRegLo(Opcode.IMM.base);
		} else {
			TempReg1 = Map_TempReg(Section,x86_Any,Opcode.IMM.base,FALSE);
		}
	} else {
		if (IsMapped(Opcode.IMM.base)) { 
			ProtectGPR(Section,Opcode.IMM.base);
			if (Opcode.IMM.immediate != 0) {
				TempReg1 = Map_TempReg(Section,x86_Any,-1,FALSE);
				LeaSourceAndOffset(TempReg1,MipsRegLo(Opcode.IMM.base),(short)Opcode.IMM.immediate);
			} else {
				TempReg1 = Map_TempReg(Section,x86_Any,Opcode.IMM.base,FALSE);
			}
			UnProtectGPR(Section,Opcode.IMM.base);
		} else {
			TempReg1 = Map_TempReg(Section,x86_Any,Opcode.IMM.base,FALSE);
			if (Opcode.IMM.immediate == 0) { 
			} else if (Opcode.IMM.immediate == 1) {
				IncX86reg(TempReg1);
			} else if (Opcode.IMM.immediate == 0xFFFF) {			
				DecX86reg(TempReg1);
			} else {
				AddConstToX86Reg(TempReg1,(short)Opcode.IMM.immediate);
			}
		}
	}
	TempReg2 = Map_TempReg(Section,x86_Any,-1,FALSE);
	if (UseTlb) {
		MoveX86RegToX86Reg(TempReg1, TempReg2);
		ShiftRightUnsignImmed(TempReg2,12);
		MoveVariableDispToX86Reg(TLB_ReadMap,"TLB_ReadMap",TempReg2,TempReg2,4);
		CompileReadTLBMiss(Section,TempReg1,TempReg2);
		
		TempReg3 = Map_TempReg(Section,x86_Any,-1,FALSE);
		MoveX86regPointerToX86reg(TempReg1, TempReg2,TempReg3);
	} else {
		AndConstToX86Reg(TempReg1,0x1FFFFFFF);
		TempReg3 = Map_TempReg(Section,x86_Any,-1,FALSE);
		MoveN64MemToX86reg(TempReg3,TempReg1);
	}
	sprintf(Name,"FPRFloatLocation[%d]",Opcode.FP.ft);
	MoveVariableToX86reg(&FPRFloatLoadStoreLocation[Opcode.FP.ft],Name,TempReg2);
	MoveX86regToX86Pointer(TempReg3,TempReg2);
}

void Compile_R4300i_LDC1 (BLOCK_SECTION * Section) {
	DWORD TempReg1, TempReg2, TempReg3;
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));

	CompileCop1Test(Section);

	UnMap_FPR(Section,Opcode.FP.ft,FALSE);
	if (IsConst(Opcode.IMM.base)) { 
		DWORD Address = MipsRegLo(Opcode.IMM.base) + (short)Opcode.IMM.immediate;
		TempReg1 = Map_TempReg(Section,x86_Any,-1,FALSE);
		if (Compile_LW(TempReg1, Address)) {
			TempReg2 = Map_TempReg(Section, x86_Any, -1, FALSE);
			sprintf(Name, "FPRDoubleLocation[%d]", Opcode.FP.ft);
			MoveVariableToX86reg(&FPRDoubleLocation[Opcode.FP.ft], Name, TempReg2);
			AddConstToX86Reg(TempReg2, 4);
			MoveX86regToX86Pointer(TempReg1, TempReg2);

			Compile_LW(TempReg1, Address + 4);
			sprintf(Name, "FPRFloatLocation[%d]", Opcode.FP.ft);
			MoveVariableToX86reg(&FPRDoubleLocation[Opcode.FP.ft], Name, TempReg2);
			MoveX86regToX86Pointer(TempReg1, TempReg2);
			return;
		}

		// Deoptimization: Address translation for a constant virtual address failed.
		// Unmap the base register to force it to be loaded from memory by the following codegen.
		UnMap_GPR(Section, Opcode.IMM.base, TRUE);
	}
	if (IsMapped(Opcode.IMM.base) && Opcode.IMM.immediate == 0) { 
		if (UseTlb) {
			ProtectGPR(Section,Opcode.IMM.base);
			TempReg1 = MipsRegLo(Opcode.IMM.base);
		} else {
			TempReg1 = Map_TempReg(Section,x86_Any,Opcode.IMM.base,FALSE);
		}
	} else {
		if (IsMapped(Opcode.IMM.base)) { 
			ProtectGPR(Section,Opcode.IMM.base);
			if (Opcode.IMM.immediate != 0) {
				TempReg1 = Map_TempReg(Section,x86_Any,-1,FALSE);
				LeaSourceAndOffset(TempReg1,MipsRegLo(Opcode.IMM.base),(short)Opcode.IMM.immediate);
			} else {
				TempReg1 = Map_TempReg(Section,x86_Any,Opcode.IMM.base,FALSE);
			}
		} else {
			TempReg1 = Map_TempReg(Section,x86_Any,Opcode.IMM.base,FALSE);
			if (Opcode.IMM.immediate == 0) { 
			} else if (Opcode.IMM.immediate == 1) {
				IncX86reg(TempReg1);
			} else if (Opcode.IMM.immediate == 0xFFFF) {			
				DecX86reg(TempReg1);
			} else {
				AddConstToX86Reg(TempReg1,(short)Opcode.IMM.immediate);
			}
		}
	}

	TempReg2 = Map_TempReg(Section,x86_Any,-1,FALSE);
	if (UseTlb) {
		MoveX86RegToX86Reg(TempReg1, TempReg2);
		ShiftRightUnsignImmed(TempReg2,12);
		MoveVariableDispToX86Reg(TLB_ReadMap,"TLB_ReadMap",TempReg2,TempReg2,4);
		CompileReadTLBMiss(Section,TempReg1,TempReg2);
		TempReg3 = Map_TempReg(Section,x86_Any,-1,FALSE);
		MoveX86regPointerToX86reg(TempReg1, TempReg2,TempReg3);
		Push(TempReg2);
		sprintf(Name,"FPRFloatLocation[%d]",Opcode.FP.ft);
		MoveVariableToX86reg(&FPRDoubleLocation[Opcode.FP.ft],Name,TempReg2);
		AddConstToX86Reg(TempReg2,4);
		MoveX86regToX86Pointer(TempReg3,TempReg2);
		Pop(TempReg2);
		MoveX86regPointerToX86regDisp8(TempReg1, TempReg2,TempReg3,4);
		sprintf(Name,"FPRFloatLocation[%d]",Opcode.FP.ft);
		MoveVariableToX86reg(&FPRDoubleLocation[Opcode.FP.ft],Name,TempReg2);
		MoveX86regToX86Pointer(TempReg3,TempReg2);
	} else {
		AndConstToX86Reg(TempReg1,0x1FFFFFFF);
		TempReg3 = Map_TempReg(Section,x86_Any,-1,FALSE);
		MoveN64MemToX86reg(TempReg3,TempReg1);

		sprintf(Name,"FPRFloatLocation[%d]",Opcode.FP.ft);
		MoveVariableToX86reg(&FPRDoubleLocation[Opcode.FP.ft],Name,TempReg2);
		AddConstToX86Reg(TempReg2,4);
		MoveX86regToX86Pointer(TempReg3,TempReg2);

		MoveN64MemDispToX86reg(TempReg3,TempReg1,4);
		sprintf(Name,"FPRFloatLocation[%d]",Opcode.FP.ft);
		MoveVariableToX86reg(&FPRDoubleLocation[Opcode.FP.ft],Name,TempReg2);
		MoveX86regToX86Pointer(TempReg3,TempReg2);
	}
}

void Compile_R4300i_SWC1 (BLOCK_SECTION * Section){
	DWORD TempReg1, TempReg2, TempReg3;
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));

	CompileCop1Test(Section);
	
	if (IsConst(Opcode.IMM.base)) { 
		DWORD Address = MipsRegLo(Opcode.IMM.base) + (short)Opcode.IMM.immediate;
		
		UnMap_FPR(Section,Opcode.FP.ft,TRUE);
		TempReg1 = Map_TempReg(Section,x86_Any,-1,FALSE);

		sprintf(Name,"FPRFloatLocation[%d]",Opcode.FP.ft);
		MoveVariableToX86reg(&FPRFloatLoadStoreLocation[Opcode.FP.ft],Name,TempReg1);
		MoveX86PointerToX86reg(TempReg1,TempReg1);
		Compile_SW_Register(TempReg1, Address);
		return;
	}
	if (IsMapped(Opcode.IMM.base)) { 
		ProtectGPR(Section,Opcode.IMM.base);
		if (Opcode.IMM.immediate != 0) {
			TempReg1 = Map_TempReg(Section,x86_Any,-1,FALSE);
			LeaSourceAndOffset(TempReg1,MipsRegLo(Opcode.IMM.base),(short)Opcode.IMM.immediate);
		} else {
			TempReg1 = Map_TempReg(Section,x86_Any,Opcode.IMM.base,FALSE);
		}
	} else {
		TempReg1 = Map_TempReg(Section,x86_Any,Opcode.IMM.base,FALSE);
		if (Opcode.IMM.immediate == 0) { 
		} else if (Opcode.IMM.immediate == 1) {
			IncX86reg(TempReg1);
		} else if (Opcode.IMM.immediate == 0xFFFF) {			
			DecX86reg(TempReg1);
		} else {
			AddConstToX86Reg(TempReg1,(short)Opcode.IMM.immediate);
		}
	}
	if (UseTlb) {
		TempReg2 = Map_TempReg(Section,x86_Any,-1,FALSE);
		MoveX86RegToX86Reg(TempReg1, TempReg2);
		ShiftRightUnsignImmed(TempReg2,12);
		MoveVariableDispToX86Reg(TLB_WriteMap,"TLB_WriteMap",TempReg2,TempReg2,4);
		//For tlb miss
		//0041C522 85 C0                test        eax,eax
		//0041C524 75 01                jne         0041C527

		UnMap_FPR(Section,Opcode.FP.ft,TRUE);
		TempReg3 = Map_TempReg(Section,x86_Any,-1,FALSE);
		sprintf(Name,"FPRFloatLocation[%d]",Opcode.FP.ft);
		MoveVariableToX86reg(&FPRFloatLoadStoreLocation[Opcode.FP.ft],Name,TempReg3);
		MoveX86PointerToX86reg(TempReg3,TempReg3);
		MoveX86regToX86regPointer(TempReg3,TempReg1, TempReg2);
	} else {
		TempReg2 = Map_TempReg(Section,x86_Any,-1,FALSE);
		UnMap_FPR(Section,Opcode.FP.ft,TRUE);
		sprintf(Name,"FPRFloatLocation[%d]",Opcode.FP.ft);
		MoveVariableToX86reg(&FPRFloatLoadStoreLocation[Opcode.FP.ft],Name,TempReg2);
		MoveX86PointerToX86reg(TempReg2,TempReg2);
		AndConstToX86Reg(TempReg1,0x1FFFFFFF);
		MoveX86regToN64Mem(TempReg2, TempReg1);
	}
}

void Compile_R4300i_SDC1 (BLOCK_SECTION * Section){
	DWORD TempReg1, TempReg2, TempReg3;
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));

	CompileCop1Test(Section);
	
	if (IsConst(Opcode.IMM.base)) { 
		DWORD Address = MipsRegLo(Opcode.IMM.base) + (short)Opcode.IMM.immediate;
		
		TempReg1 = Map_TempReg(Section,x86_Any,-1,FALSE);
		sprintf(Name,"FPRDoubleLocation[%d]",Opcode.FP.ft);
		MoveVariableToX86reg((BYTE *)&FPRDoubleLocation[Opcode.FP.ft],Name,TempReg1);
		AddConstToX86Reg(TempReg1,4);
		MoveX86PointerToX86reg(TempReg1,TempReg1);
		Compile_SW_Register(TempReg1, Address);

		sprintf(Name,"FPRDoubleLocation[%d]",Opcode.FP.ft);
		MoveVariableToX86reg(&FPRDoubleLocation[Opcode.FP.ft],Name,TempReg1);
		MoveX86PointerToX86reg(TempReg1,TempReg1);
		Compile_SW_Register(TempReg1, Address + 4);		
		return;
	}
	if (IsMapped(Opcode.IMM.base)) { 
		ProtectGPR(Section,Opcode.IMM.base);
		if (Opcode.IMM.immediate != 0) {
			TempReg1 = Map_TempReg(Section,x86_Any,-1,FALSE);
			LeaSourceAndOffset(TempReg1,MipsRegLo(Opcode.IMM.base),(short)Opcode.IMM.immediate);
		} else {
			TempReg1 = Map_TempReg(Section,x86_Any,Opcode.IMM.base,FALSE);
		}
	} else {
		TempReg1 = Map_TempReg(Section,x86_Any,Opcode.IMM.base,FALSE);
		if (Opcode.IMM.immediate == 0) { 
		} else if (Opcode.IMM.immediate == 1) {
			IncX86reg(TempReg1);
		} else if (Opcode.IMM.immediate == 0xFFFF) {			
			DecX86reg(TempReg1);
		} else {
			AddConstToX86Reg(TempReg1,(short)Opcode.IMM.immediate);
		}
	}
	if (UseTlb) {
		TempReg2 = Map_TempReg(Section,x86_Any,-1,FALSE);
		MoveX86RegToX86Reg(TempReg1, TempReg2);
		ShiftRightUnsignImmed(TempReg2,12);
		MoveVariableDispToX86Reg(TLB_WriteMap,"TLB_WriteMap",TempReg2,TempReg2,4);
		//For tlb miss
		//0041C522 85 C0                test        eax,eax
		//0041C524 75 01                jne         0041C527

		TempReg3 = Map_TempReg(Section,x86_Any,-1,FALSE);
		sprintf(Name,"FPRDoubleLocation[%d]",Opcode.FP.ft);
		MoveVariableToX86reg((BYTE *)&FPRDoubleLocation[Opcode.FP.ft],Name,TempReg3);
		AddConstToX86Reg(TempReg3,4);
		MoveX86PointerToX86reg(TempReg3,TempReg3);		
		MoveX86regToX86regPointer(TempReg3,TempReg1, TempReg2);
		AddConstToX86Reg(TempReg1,4);

		sprintf(Name,"FPRDoubleLocation[%d]",Opcode.FP.ft);
		MoveVariableToX86reg((BYTE *)&FPRDoubleLocation[Opcode.FP.ft],Name,TempReg3);
		MoveX86PointerToX86reg(TempReg3,TempReg3);		
		MoveX86regToX86regPointer(TempReg3,TempReg1, TempReg2);
	} else {
		AndConstToX86Reg(TempReg1,0x1FFFFFFF);		
		TempReg3 = Map_TempReg(Section,x86_Any,-1,FALSE);
		sprintf(Name,"FPRDoubleLocation[%d]",Opcode.FP.ft);
		MoveVariableToX86reg((BYTE *)&FPRDoubleLocation[Opcode.FP.ft],Name,TempReg3);
		AddConstToX86Reg(TempReg3,4);
		MoveX86PointerToX86reg(TempReg3,TempReg3);		
		MoveX86regToN64Mem(TempReg3, TempReg1);
		sprintf(Name,"FPRDoubleLocation[%d]",Opcode.FP.ft);
		MoveVariableToX86reg((BYTE *)&FPRDoubleLocation[Opcode.FP.ft],Name,TempReg3);
		MoveX86PointerToX86reg(TempReg3,TempReg3);		
		MoveX86regToN64MemDisp(TempReg3, TempReg1,4);
	}

}

/************************** COP1 functions **************************/
void Compile_R4300i_COP1_MF (BLOCK_SECTION * Section) {
	DWORD TempReg;
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
	if (Opcode.BRANCH.rt == 0) { return; }
	CompileCop1Test(Section);

	UnMap_FPR(Section,Opcode.FP.fs,TRUE);
	Map_GPR_32bit(Section,Opcode.BRANCH.rt, TRUE, -1);
	TempReg = Map_TempReg(Section,x86_Any,-1,FALSE);
	sprintf(Name,"FPRFloatLocation[%d]",Opcode.FP.fs);
	MoveVariableToX86reg((BYTE *)&FPRFloatLoadStoreLocation[Opcode.FP.fs],Name,TempReg);
	MoveX86PointerToX86reg(MipsRegLo(Opcode.BRANCH.rt),TempReg);		
}

void Compile_R4300i_COP1_DMF (BLOCK_SECTION * Section) {
	DWORD TempReg;
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
	if (Opcode.BRANCH.rt == 0) { return; }
	CompileCop1Test(Section);

	UnMap_FPR(Section,Opcode.FP.fs,TRUE);
	Map_GPR_64bit(Section,Opcode.BRANCH.rt, -1);
	TempReg = Map_TempReg(Section,x86_Any,-1,FALSE);
	sprintf(Name,"FPRDoubleLocation[%d]",Opcode.FP.fs);
	MoveVariableToX86reg((BYTE *)&FPRDoubleLocation[Opcode.FP.fs],Name,TempReg);
	AddConstToX86Reg(TempReg,4);
	MoveX86PointerToX86reg(MipsRegHi(Opcode.BRANCH.rt),TempReg);		
	sprintf(Name,"FPRDoubleLocation[%d]",Opcode.FP.fs);
	MoveVariableToX86reg((BYTE *)&FPRDoubleLocation[Opcode.FP.fs],Name,TempReg);
	MoveX86PointerToX86reg(MipsRegLo(Opcode.BRANCH.rt),TempReg);		
}

void Compile_R4300i_COP1_CF(BLOCK_SECTION * Section) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));

	CompileCop1Test(Section);
	
	if (Opcode.FP.fs != 31 && Opcode.FP.fs != 0) { Compile_R4300i_UnknownOpcode (Section); return; }
	Map_GPR_32bit(Section,Opcode.BRANCH.rt,TRUE,-1);
	MoveVariableToX86reg(&FPCR[Opcode.FP.fs],FPR_Ctrl_Name[Opcode.FP.fs],MipsRegLo(Opcode.BRANCH.rt));
}

void Compile_R4300i_COP1_MT( BLOCK_SECTION * Section) {	
	DWORD TempReg;
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
	CompileCop1Test(Section);
	
	if ((Opcode.FP.fs & 1) != 0) {
		if (RegInStack(Section,Opcode.FP.fs-1,FPU_Double) || RegInStack(Section,Opcode.FP.fs-1,FPU_Qword)) {
			UnMap_FPR(Section,Opcode.FP.fs-1,TRUE);
		}
	}
	UnMap_FPR(Section,Opcode.FP.fs,TRUE);
	TempReg = Map_TempReg(Section,x86_Any,-1,FALSE);
	sprintf(Name,"FPRFloatLocation[%d]",Opcode.FP.fs);
	MoveVariableToX86reg((BYTE *)&FPRFloatLoadStoreLocation[Opcode.FP.fs],Name,TempReg);

	if (IsConst(Opcode.BRANCH.rt)) {
		MoveConstToX86Pointer(MipsRegLo(Opcode.BRANCH.rt),TempReg);
	} else if (IsMapped(Opcode.BRANCH.rt)) {
		MoveX86regToX86Pointer(MipsRegLo(Opcode.BRANCH.rt),TempReg);
	} else {
		MoveX86regToX86Pointer(Map_TempReg(Section,x86_Any, Opcode.BRANCH.rt, FALSE),TempReg);
	}
}

void Compile_R4300i_COP1_DMT( BLOCK_SECTION * Section) {
	DWORD TempReg;
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
	CompileCop1Test(Section);
	
	if ((Opcode.FP.fs & 1) == 0) {
		if (RegInStack(Section,Opcode.FP.fs+1,FPU_Float) || RegInStack(Section,Opcode.FP.fs+1,FPU_Dword)) {
			UnMap_FPR(Section,Opcode.FP.fs+1,TRUE);
		}
	}
	UnMap_FPR(Section,Opcode.FP.fs,TRUE);
	TempReg = Map_TempReg(Section,x86_Any,-1,FALSE);
	sprintf(Name,"FPRDoubleLocation[%d]",Opcode.FP.fs);
	MoveVariableToX86reg((BYTE *)&FPRDoubleLocation[Opcode.FP.fs],Name,TempReg);
		
	if (IsConst(Opcode.BRANCH.rt)) {
		MoveConstToX86Pointer(MipsRegLo(Opcode.BRANCH.rt),TempReg);
		AddConstToX86Reg(TempReg,4);
		if Is64Bit(Opcode.BRANCH.rt) {
			MoveConstToX86Pointer(MipsRegHi(Opcode.BRANCH.rt),TempReg);
		} else {
			MoveConstToX86Pointer(MipsRegLo_S(Opcode.BRANCH.rt) >> 31,TempReg);
		}
	} else if (IsMapped(Opcode.BRANCH.rt)) {
		MoveX86regToX86Pointer(MipsRegLo(Opcode.BRANCH.rt),TempReg);
		AddConstToX86Reg(TempReg,4);
		if Is64Bit(Opcode.BRANCH.rt) {
			MoveX86regToX86Pointer(MipsRegHi(Opcode.BRANCH.rt),TempReg);
		} else {
			MoveX86regToX86Pointer(Map_TempReg(Section,x86_Any, Opcode.BRANCH.rt, TRUE),TempReg);
		}
	} else {
		int x86Reg = Map_TempReg(Section,x86_Any, Opcode.BRANCH.rt, FALSE);
		MoveX86regToX86Pointer(x86Reg,TempReg);
		AddConstToX86Reg(TempReg,4);
		MoveX86regToX86Pointer(Map_TempReg(Section,x86Reg, Opcode.BRANCH.rt, TRUE),TempReg);
	}
}


void Compile_R4300i_COP1_CT(BLOCK_SECTION * Section) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
	CompileCop1Test(Section);
	if (Opcode.FP.fs != 31) { Compile_R4300i_UnknownOpcode (Section); return; }

	if (IsConst(Opcode.BRANCH.rt)) {
		MoveConstToVariable(MipsRegLo(Opcode.BRANCH.rt),&FPCR[Opcode.FP.fs],FPR_Ctrl_Name[Opcode.FP.fs]);
	} else if (IsMapped(Opcode.BRANCH.rt)) {
		MoveX86regToVariable(MipsRegLo(Opcode.BRANCH.rt),&FPCR[Opcode.FP.fs],FPR_Ctrl_Name[Opcode.FP.fs]);
	} else {
		MoveX86regToVariable(Map_TempReg(Section,x86_Any,Opcode.BRANCH.rt,FALSE),&FPCR[Opcode.FP.fs],FPR_Ctrl_Name[Opcode.FP.fs]);		
	}
	Pushad();
	Call_Direct(ChangeDefaultRoundingModel, "ChangeDefaultRoundingModel");
	Popad();
	CurrentRoundingModel = RoundUnknown;
}

/************************** COP1: S functions ************************/
void Compile_R4300i_COP1_S_ADD (BLOCK_SECTION * Section) {
	DWORD Reg1 = Opcode.FP.ft == Opcode.FP.fd?Opcode.FP.ft:Opcode.FP.fs;
	DWORD Reg2 = Opcode.FP.ft == Opcode.FP.fd?Opcode.FP.fs:Opcode.FP.ft;
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;
	
	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
	
	CompileCop1Test(Section);	

	Load_FPR_ToTop(Section,Opcode.FP.fd,Reg1, FPU_Float);
	if (RegInStack(Section,Reg2, FPU_Float)) {
		fpuAddReg(StackPosition(Section,Reg2));
	} else {
		DWORD TempReg;

		UnMap_FPR(Section,Reg2,TRUE);
		TempReg = Map_TempReg(Section,x86_Any,-1,FALSE);
		sprintf(Name,"FPRFloatLocation[%d]",Reg2);
		MoveVariableToX86reg((BYTE *)&FPRFloatLoadStoreLocation[Reg2],Name,TempReg);
		Load_FPR_ToTop(Section,Opcode.FP.fd,Opcode.FP.fd, FPU_Float);
		fpuAddDwordRegPointer(TempReg);
	}
	UnMap_FPR(Section,Opcode.FP.fd,TRUE);
}

void Compile_R4300i_COP1_S_SUB (BLOCK_SECTION * Section) {
	DWORD Reg1 = Opcode.FP.ft == Opcode.FP.fd?Opcode.FP.ft:Opcode.FP.fs;
	DWORD Reg2 = Opcode.FP.ft == Opcode.FP.fd?Opcode.FP.fs:Opcode.FP.ft;
	DWORD TempReg;
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;
	
	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
	
	CompileCop1Test(Section);	

	if (Opcode.FP.fd == Opcode.FP.ft) {
		UnMap_FPR(Section,Opcode.FP.fd,TRUE);
		Load_FPR_ToTop(Section,Opcode.FP.fd,Opcode.FP.fs,FPU_Float);

		TempReg = Map_TempReg(Section,x86_Any,-1,FALSE);
		sprintf(Name,"FPRFloatLocation[%d]",Opcode.FP.ft);
		MoveVariableToX86reg((BYTE *)&FPRFloatLoadStoreLocation[Opcode.FP.ft],Name,TempReg);
		fpuSubDwordRegPointer(TempReg);
	} else {
		Load_FPR_ToTop(Section,Opcode.FP.fd,Reg1, FPU_Float);
		if (RegInStack(Section,Reg2, FPU_Float)) {
			fpuSubReg(StackPosition(Section,Reg2));
		} else {
			UnMap_FPR(Section,Reg2,TRUE);
			Load_FPR_ToTop(Section,Opcode.FP.fd,Opcode.FP.fd, FPU_Float);

			TempReg = Map_TempReg(Section,x86_Any,-1,FALSE);
			sprintf(Name,"FPRFloatLocation[%d]",Reg2);
			MoveVariableToX86reg((BYTE *)&FPRFloatLoadStoreLocation[Reg2],Name,TempReg);
			fpuSubDwordRegPointer(TempReg);			
		}
	}
	UnMap_FPR(Section,Opcode.FP.fd,TRUE);
}

void Compile_R4300i_COP1_S_MUL (BLOCK_SECTION * Section) {
	DWORD Reg1 = Opcode.FP.ft == Opcode.FP.fd?Opcode.FP.ft:Opcode.FP.fs;
	DWORD Reg2 = Opcode.FP.ft == Opcode.FP.fd?Opcode.FP.fs:Opcode.FP.ft;
	DWORD TempReg;
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;
	
	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
	
	CompileCop1Test(Section);	

	Load_FPR_ToTop(Section,Opcode.FP.fd,Reg1, FPU_Float);
	if (RegInStack(Section,Reg2, FPU_Float)) {
		fpuMulReg(StackPosition(Section,Reg2));
	} else {
		UnMap_FPR(Section,Reg2,TRUE);
		Load_FPR_ToTop(Section,Opcode.FP.fd,Opcode.FP.fd, FPU_Float);

		TempReg = Map_TempReg(Section,x86_Any,-1,FALSE);
		sprintf(Name,"FPRFloatLocation[%d]",Reg2);
		MoveVariableToX86reg((BYTE *)&FPRFloatLoadStoreLocation[Reg2],Name,TempReg);
		fpuMulDwordRegPointer(TempReg);			
	}
	UnMap_FPR(Section,Opcode.FP.fd,TRUE);
}

void Compile_R4300i_COP1_S_DIV (BLOCK_SECTION * Section) {
	DWORD Reg1 = Opcode.FP.ft == Opcode.FP.fd?Opcode.FP.ft:Opcode.FP.fs;
	DWORD Reg2 = Opcode.FP.ft == Opcode.FP.fd?Opcode.FP.fs:Opcode.FP.ft;
	DWORD TempReg;
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;
	
	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
	
	CompileCop1Test(Section);	

	if (Opcode.FP.fd == Opcode.FP.ft) {
		UnMap_FPR(Section,Opcode.FP.fd,TRUE);
		Load_FPR_ToTop(Section,Opcode.FP.fd,Opcode.FP.fs,FPU_Float);

		TempReg = Map_TempReg(Section,x86_Any,-1,FALSE);
		sprintf(Name,"FPRFloatLocation[%d]",Opcode.FP.ft);
		MoveVariableToX86reg((BYTE *)&FPRFloatLoadStoreLocation[Opcode.FP.ft],Name,TempReg);
		fpuDivDwordRegPointer(TempReg);
	} else {
		Load_FPR_ToTop(Section,Opcode.FP.fd,Reg1, FPU_Float);
		if (RegInStack(Section,Reg2, FPU_Float)) {
			fpuDivReg(StackPosition(Section,Reg2));
		} else {
			UnMap_FPR(Section,Reg2,TRUE);
			Load_FPR_ToTop(Section,Opcode.FP.fd,Opcode.FP.fd, FPU_Float);

			TempReg = Map_TempReg(Section,x86_Any,-1,FALSE);
			sprintf(Name,"FPRFloatLocation[%d]",Reg2);
			MoveVariableToX86reg((BYTE *)&FPRFloatLoadStoreLocation[Reg2],Name,TempReg);
			fpuDivDwordRegPointer(TempReg);			
		}
	}

	UnMap_FPR(Section,Opcode.FP.fd,TRUE);
}

void Compile_R4300i_COP1_S_ABS (BLOCK_SECTION * Section) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
	Load_FPR_ToTop(Section,Opcode.FP.fd,Opcode.FP.fs,FPU_Float);
	fpuAbs();
	UnMap_FPR(Section,Opcode.FP.fd,TRUE);
}

void Compile_R4300i_COP1_S_NEG (BLOCK_SECTION * Section) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
	Load_FPR_ToTop(Section,Opcode.FP.fd,Opcode.FP.fs,FPU_Float);
	fpuNeg();
	UnMap_FPR(Section,Opcode.FP.fd,TRUE);
}

void Compile_R4300i_COP1_S_SQRT (BLOCK_SECTION * Section) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
	Load_FPR_ToTop(Section,Opcode.FP.fd,Opcode.FP.fs,FPU_Float);
	fpuSqrt();
	UnMap_FPR(Section,Opcode.FP.fd,TRUE);
}

void Compile_R4300i_COP1_S_MOV (BLOCK_SECTION * Section) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
	Load_FPR_ToTop(Section,Opcode.FP.fd,Opcode.FP.fs,FPU_Float);
}

void Compile_R4300i_COP1_S_TRUNC_L (BLOCK_SECTION * Section) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
	
	CompileCop1Test(Section);	
	if (Opcode.FP.fd != Opcode.FP.fs || !RegInStack(Section,Opcode.FP.fd,FPU_Float)) { 
		Load_FPR_ToTop(Section,Opcode.FP.fd,Opcode.FP.fs,FPU_Float); 
	}
	ChangeFPURegFormat(Section,Opcode.FP.fd,FPU_Float,FPU_Qword,RoundTruncate);
}

void Compile_R4300i_COP1_S_CEIL_L (BLOCK_SECTION * Section) {			//added by Witten
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
	
	CompileCop1Test(Section);	
	if (Opcode.FP.fd != Opcode.FP.fs || !RegInStack(Section,Opcode.FP.fd,FPU_Float)) { 
		Load_FPR_ToTop(Section,Opcode.FP.fd,Opcode.FP.fs,FPU_Float); 
	}
	ChangeFPURegFormat(Section,Opcode.FP.fd,FPU_Float,FPU_Qword,RoundUp);
}

void Compile_R4300i_COP1_S_FLOOR_L (BLOCK_SECTION * Section) {			//added by Witten
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
	
	CompileCop1Test(Section);	
	if (Opcode.FP.fd != Opcode.FP.fs || !RegInStack(Section,Opcode.FP.fd,FPU_Float)) { 
		Load_FPR_ToTop(Section,Opcode.FP.fd,Opcode.FP.fs,FPU_Float); 
	}
	ChangeFPURegFormat(Section,Opcode.FP.fd,FPU_Float,FPU_Qword,RoundDown);
}

void Compile_R4300i_COP1_S_ROUND_W (BLOCK_SECTION * Section) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
	
	CompileCop1Test(Section);	
	if (Opcode.FP.fd != Opcode.FP.fs || !RegInStack(Section,Opcode.FP.fd,FPU_Float)) { 
		Load_FPR_ToTop(Section,Opcode.FP.fd,Opcode.FP.fs,FPU_Float); 
	}
	ChangeFPURegFormat(Section,Opcode.FP.fd,FPU_Float,FPU_Dword,RoundNearest);
}

void Compile_R4300i_COP1_S_TRUNC_W (BLOCK_SECTION * Section) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
	
	CompileCop1Test(Section);	
	if (Opcode.FP.fd != Opcode.FP.fs || !RegInStack(Section,Opcode.FP.fd,FPU_Float)) { 
		Load_FPR_ToTop(Section,Opcode.FP.fd,Opcode.FP.fs,FPU_Float); 
	}
	ChangeFPURegFormat(Section,Opcode.FP.fd,FPU_Float,FPU_Dword,RoundTruncate);
}

void Compile_R4300i_COP1_S_CEIL_W (BLOCK_SECTION * Section) {			// added by Witten
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
	
	CompileCop1Test(Section);	
	if (Opcode.FP.fd != Opcode.FP.fs || !RegInStack(Section,Opcode.FP.fd,FPU_Float)) { 
		Load_FPR_ToTop(Section,Opcode.FP.fd,Opcode.FP.fs,FPU_Float); 
	}
	ChangeFPURegFormat(Section,Opcode.FP.fd,FPU_Float,FPU_Dword,RoundUp);
}

void Compile_R4300i_COP1_S_FLOOR_W (BLOCK_SECTION * Section) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
	
	CompileCop1Test(Section);	
	if (Opcode.FP.fd != Opcode.FP.fs || !RegInStack(Section,Opcode.FP.fd,FPU_Float)) { 
		Load_FPR_ToTop(Section,Opcode.FP.fd,Opcode.FP.fs,FPU_Float); 
	}
	ChangeFPURegFormat(Section,Opcode.FP.fd,FPU_Float,FPU_Dword,RoundDown);
}

void Compile_R4300i_COP1_S_CVT_D (BLOCK_SECTION * Section) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
	
	CompileCop1Test(Section);	
	if (Opcode.FP.fd != Opcode.FP.fs || !RegInStack(Section,Opcode.FP.fd,FPU_Float)) {
		Load_FPR_ToTop(Section,Opcode.FP.fd,Opcode.FP.fs,FPU_Float);
	}
	ChangeFPURegFormat(Section,Opcode.FP.fd,FPU_Float,FPU_Double,RoundDefault);
}

void Compile_R4300i_COP1_S_CVT_W (BLOCK_SECTION * Section) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
	
	CompileCop1Test(Section);	
	if (Opcode.FP.fd != Opcode.FP.fs || !RegInStack(Section,Opcode.FP.fd,FPU_Float)) {
		Load_FPR_ToTop(Section,Opcode.FP.fd,Opcode.FP.fs,FPU_Float);
	}
	ChangeFPURegFormat(Section,Opcode.FP.fd,FPU_Float,FPU_Dword,RoundDefault);
}

void Compile_R4300i_COP1_S_CVT_L (BLOCK_SECTION * Section) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
	
	CompileCop1Test(Section);	
	if (Opcode.FP.fd != Opcode.FP.fs || !RegInStack(Section,Opcode.FP.fd,FPU_Float)) {
		Load_FPR_ToTop(Section,Opcode.FP.fd,Opcode.FP.fs,FPU_Float);
	}
	ChangeFPURegFormat(Section,Opcode.FP.fd,FPU_Float,FPU_Qword,RoundDefault);
}

void Compile_R4300i_COP1_S_CMP (BLOCK_SECTION * Section) {
	DWORD Reg1 = RegInStack(Section,Opcode.FP.ft, FPU_Float)?Opcode.FP.ft:Opcode.FP.fs;
	DWORD Reg2 = RegInStack(Section,Opcode.FP.ft, FPU_Float)?Opcode.FP.fs:Opcode.FP.ft;
	int x86reg, cmp = 0;
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;
	
	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
	
	CompileCop1Test(Section);	
	//if ((Opcode.REG.funct & 1) != 0) { Compile_R4300i_UnknownOpcode(Section); }
	if ((Opcode.REG.funct & 2) != 0) { cmp |= 0x4000; }
	if ((Opcode.REG.funct & 4) != 0) { cmp |= 0x0100; }
	
	Load_FPR_ToTop(Section,Reg1,Reg1, FPU_Float);
	Map_TempReg(Section,x86_EAX, 0, FALSE);
	if (RegInStack(Section,Reg2, FPU_Float)) {
		fpuComReg(StackPosition(Section,Reg2),FALSE);
	} else {
		DWORD TempReg;

		UnMap_FPR(Section,Reg2,TRUE);
		Load_FPR_ToTop(Section,Reg1,Reg1, FPU_Float);

		TempReg = Map_TempReg(Section,x86_Any,-1,FALSE);
		sprintf(Name,"FPRFloatLocation[%d]",Reg2);
		MoveVariableToX86reg((BYTE *)&FPRFloatLoadStoreLocation[Reg2],Name,TempReg);
		fpuComDwordRegPointer(TempReg,FALSE);
	}
	AndConstToVariable((DWORD)~FPCSR_C, &FSTATUS_REGISTER, "FSTATUS_REGISTER");
	fpuStoreStatus();
	x86reg = Map_TempReg(Section,x86_Any8Bit, 0, FALSE);
	
	if (cmp != 0) {
		TestConstToX86Reg(cmp,x86_EAX);	
		Setnz(x86reg);
		
		if ((Opcode.REG.funct & 1) != 0) {
			int x86reg2 = Map_TempReg(Section,x86_Any8Bit, 0, FALSE);
			AndConstToX86Reg(x86_EAX, 0x4300);
			CompConstToX86reg(x86_EAX, 0x4300);
			Setz(x86reg2);
	
			OrX86RegToX86Reg(x86reg, x86reg2);
		}
	} else if ((Opcode.REG.funct & 1) != 0) {
		AndConstToX86Reg(x86_EAX, 0x4300);
		CompConstToX86reg(x86_EAX, 0x4300);
		Setz(x86reg);
	}
	ShiftLeftSignImmed(x86reg, 23);
	OrX86RegToVariable(&FPCR[31], "FPCR[31]", x86reg);
}

/************************** COP1: D functions ************************/
void Compile_R4300i_COP1_D_ADD (BLOCK_SECTION * Section) {
	DWORD Reg1 = Opcode.FP.ft == Opcode.FP.fd?Opcode.FP.ft:Opcode.FP.fs;
	DWORD Reg2 = Opcode.FP.ft == Opcode.FP.fd?Opcode.FP.fs:Opcode.FP.ft;
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;
	
	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
	
	CompileCop1Test(Section);	

	Load_FPR_ToTop(Section,Opcode.FP.fd,Reg1, FPU_Double);
	if (RegInStack(Section,Reg2, FPU_Double)) {
		fpuAddReg(StackPosition(Section,Reg2));
	} else {
		DWORD TempReg;

		UnMap_FPR(Section,Reg2,TRUE);
		TempReg = Map_TempReg(Section,x86_Any,-1,FALSE);
		sprintf(Name,"FPRDoubleLocation[%d]",Reg2);
		MoveVariableToX86reg((BYTE *)&FPRDoubleLocation[Reg2],Name,TempReg);
		Load_FPR_ToTop(Section,Opcode.FP.fd,Opcode.FP.fd, FPU_Double);
		fpuAddQwordRegPointer(TempReg);	
	}
}

void Compile_R4300i_COP1_D_SUB (BLOCK_SECTION * Section) {
	DWORD Reg1 = Opcode.FP.ft == Opcode.FP.fd?Opcode.FP.ft:Opcode.FP.fs;
	DWORD Reg2 = Opcode.FP.ft == Opcode.FP.fd?Opcode.FP.fs:Opcode.FP.ft;
	DWORD TempReg;
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;
	
	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
	
	CompileCop1Test(Section);	

	if (Opcode.FP.fd == Opcode.FP.ft) {
		UnMap_FPR(Section,Opcode.FP.fd,TRUE);
		TempReg = Map_TempReg(Section,x86_Any,-1,FALSE);
		sprintf(Name,"FPRDoubleLocation[%d]",Opcode.FP.ft);
		MoveVariableToX86reg((BYTE *)&FPRDoubleLocation[Opcode.FP.ft],Name,TempReg);
		Load_FPR_ToTop(Section,Opcode.FP.fd,Opcode.FP.fs,FPU_Double);
		fpuSubQwordRegPointer(TempReg);
	} else {
		Load_FPR_ToTop(Section,Opcode.FP.fd,Reg1, FPU_Double);
		if (RegInStack(Section,Reg2, FPU_Double)) {
			fpuSubReg(StackPosition(Section,Reg2));
		} else {
			UnMap_FPR(Section,Reg2,TRUE);

			TempReg = Map_TempReg(Section,x86_Any,-1,FALSE);
			sprintf(Name,"FPRDoubleLocation[%d]",Reg2);
			MoveVariableToX86reg((BYTE *)&FPRDoubleLocation[Reg2],Name,TempReg);
			Load_FPR_ToTop(Section,Opcode.FP.fd,Opcode.FP.fd, FPU_Double);
			fpuSubQwordRegPointer(TempReg);
		}
	}
}

void Compile_R4300i_COP1_D_MUL (BLOCK_SECTION * Section) {
	DWORD Reg1 = Opcode.FP.ft == Opcode.FP.fd?Opcode.FP.ft:Opcode.FP.fs;
	DWORD Reg2 = Opcode.FP.ft == Opcode.FP.fd?Opcode.FP.fs:Opcode.FP.ft;
	DWORD TempReg;
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;
	
	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
	
	CompileCop1Test(Section);	

	Load_FPR_ToTop(Section,Opcode.FP.fd,Reg1, FPU_Double);
	if (RegInStack(Section,Reg2, FPU_Double)) {
		fpuMulReg(StackPosition(Section,Reg2));
	} else {
		UnMap_FPR(Section,Reg2,TRUE);
		Load_FPR_ToTop(Section,Opcode.FP.fd,Opcode.FP.fd, FPU_Double);
		TempReg = Map_TempReg(Section,x86_Any,-1,FALSE);
		sprintf(Name,"FPRDoubleLocation[%d]",Reg2);
		MoveVariableToX86reg((BYTE *)&FPRDoubleLocation[Reg2],Name,TempReg);
		fpuMulQwordRegPointer(TempReg);
	}
}

void Compile_R4300i_COP1_D_DIV (BLOCK_SECTION * Section) {
	DWORD Reg1 = Opcode.FP.ft == Opcode.FP.fd?Opcode.FP.ft:Opcode.FP.fs;
	DWORD Reg2 = Opcode.FP.ft == Opcode.FP.fd?Opcode.FP.fs:Opcode.FP.ft;
	DWORD TempReg;
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;
	
	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
	
	CompileCop1Test(Section);	

	if (Opcode.FP.fd == Opcode.FP.ft) {
		UnMap_FPR(Section,Opcode.FP.fd,TRUE);
		TempReg = Map_TempReg(Section,x86_Any,-1,FALSE);
		sprintf(Name,"FPRDoubleLocation[%d]",Opcode.FP.ft);
		MoveVariableToX86reg((BYTE *)&FPRDoubleLocation[Opcode.FP.ft],Name,TempReg);
		Load_FPR_ToTop(Section,Opcode.FP.fd,Opcode.FP.fs,FPU_Double);
		fpuDivQwordRegPointer(TempReg);
	} else {
		Load_FPR_ToTop(Section,Opcode.FP.fd,Reg1, FPU_Double);
		if (RegInStack(Section,Reg2, FPU_Double)) {
			fpuDivReg(StackPosition(Section,Reg2));
		} else {
			UnMap_FPR(Section,Reg2,TRUE);
			TempReg = Map_TempReg(Section,x86_Any,-1,FALSE);
			sprintf(Name,"FPRDoubleLocation[%d]",Reg2);
			MoveVariableToX86reg((BYTE *)&FPRDoubleLocation[Reg2],Name,TempReg);
			Load_FPR_ToTop(Section,Opcode.FP.fd,Opcode.FP.fd, FPU_Double);
			fpuDivQwordRegPointer(TempReg);
		}
	}
}

void Compile_R4300i_COP1_D_ABS (BLOCK_SECTION * Section) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
	Load_FPR_ToTop(Section,Opcode.FP.fd,Opcode.FP.fs,FPU_Double);
	fpuAbs();
}

void Compile_R4300i_COP1_D_NEG (BLOCK_SECTION * Section) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
	Load_FPR_ToTop(Section,Opcode.FP.fd,Opcode.FP.fs,FPU_Double);
	fpuNeg();
}

void Compile_R4300i_COP1_D_SQRT (BLOCK_SECTION * Section) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
	Load_FPR_ToTop(Section,Opcode.FP.fd,Opcode.FP.fs,FPU_Double);
	fpuSqrt();
}

void Compile_R4300i_COP1_D_MOV (BLOCK_SECTION * Section) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
	Load_FPR_ToTop(Section,Opcode.FP.fd,Opcode.FP.fs,FPU_Double);
}

void Compile_R4300i_COP1_D_TRUNC_L (BLOCK_SECTION * Section) {			//added by Witten
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
	
	CompileCop1Test(Section);	
	if (RegInStack(Section,Opcode.FP.fs,FPU_Double) || RegInStack(Section,Opcode.FP.fs,FPU_Qword)) {
		UnMap_FPR(Section,Opcode.FP.fs,TRUE);
	}
	if (Opcode.FP.fd != Opcode.FP.fs || !RegInStack(Section,Opcode.FP.fd,FPU_Double)) { 
		Load_FPR_ToTop(Section,Opcode.FP.fd,Opcode.FP.fs,FPU_Double); 
	}
	ChangeFPURegFormat(Section,Opcode.FP.fd,FPU_Double,FPU_Qword,RoundTruncate);
}

void Compile_R4300i_COP1_D_CEIL_L (BLOCK_SECTION * Section) {			//added by Witten
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
	
	CompileCop1Test(Section);	
	if (RegInStack(Section,Opcode.FP.fs,FPU_Double) || RegInStack(Section,Opcode.FP.fs,FPU_Qword)) {
		UnMap_FPR(Section,Opcode.FP.fs,TRUE);
	}
	if (Opcode.FP.fd != Opcode.FP.fs || !RegInStack(Section,Opcode.FP.fd,FPU_Double)) { 
		Load_FPR_ToTop(Section,Opcode.FP.fd,Opcode.FP.fs,FPU_Double); 
	}
	ChangeFPURegFormat(Section,Opcode.FP.fd,FPU_Double,FPU_Qword,RoundUp);
}

void Compile_R4300i_COP1_D_FLOOR_L (BLOCK_SECTION * Section) {			//added by Witten
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
	
	CompileCop1Test(Section);	
	if (RegInStack(Section,Opcode.FP.fs,FPU_Double) || RegInStack(Section,Opcode.FP.fs,FPU_Qword)) {
		UnMap_FPR(Section,Opcode.FP.fs,TRUE);
	}
	if (Opcode.FP.fd != Opcode.FP.fs || !RegInStack(Section,Opcode.FP.fd,FPU_Double)) { 
		Load_FPR_ToTop(Section,Opcode.FP.fd,Opcode.FP.fs,FPU_Double); 
	}
	ChangeFPURegFormat(Section,Opcode.FP.fd,FPU_Double,FPU_Qword,RoundDown);
}

void Compile_R4300i_COP1_D_ROUND_W (BLOCK_SECTION * Section) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
	
	CompileCop1Test(Section);	
	if (RegInStack(Section,Opcode.FP.fs,FPU_Double) || RegInStack(Section,Opcode.FP.fs,FPU_Qword)) {
		UnMap_FPR(Section,Opcode.FP.fs,TRUE);
	}
	if (Opcode.FP.fd != Opcode.FP.fs || !RegInStack(Section,Opcode.FP.fd,FPU_Double)) { 
		Load_FPR_ToTop(Section,Opcode.FP.fd,Opcode.FP.fs,FPU_Double); 
	}
	ChangeFPURegFormat(Section,Opcode.FP.fd,FPU_Double,FPU_Dword,RoundNearest);
}

void Compile_R4300i_COP1_D_TRUNC_W (BLOCK_SECTION * Section) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
	
	CompileCop1Test(Section);	
	if (RegInStack(Section,Opcode.FP.fs,FPU_Double) || RegInStack(Section,Opcode.FP.fs,FPU_Qword)) {
		UnMap_FPR(Section,Opcode.FP.fs,TRUE);
	}
	if (Opcode.FP.fd != Opcode.FP.fs || !RegInStack(Section,Opcode.FP.fd,FPU_Double)) { 
		Load_FPR_ToTop(Section,Opcode.FP.fd,Opcode.FP.fs,FPU_Double); 
	}
	ChangeFPURegFormat(Section,Opcode.FP.fd,FPU_Double,FPU_Dword,RoundTruncate);
}

void Compile_R4300i_COP1_D_CEIL_W (BLOCK_SECTION * Section) {				// added by Witten
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
	
	CompileCop1Test(Section);	
	if (RegInStack(Section,Opcode.FP.fs,FPU_Double) || RegInStack(Section,Opcode.FP.fs,FPU_Qword)) {
		UnMap_FPR(Section,Opcode.FP.fs,TRUE);
	}
	if (Opcode.FP.fd != Opcode.FP.fs || !RegInStack(Section,Opcode.FP.fd,FPU_Double)) { 
		Load_FPR_ToTop(Section,Opcode.FP.fd,Opcode.FP.fs,FPU_Double); 
	}
	ChangeFPURegFormat(Section,Opcode.FP.fd,FPU_Double,FPU_Dword,RoundUp);
}

void Compile_R4300i_COP1_D_FLOOR_W (BLOCK_SECTION * Section) {			//added by Witten
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
	
	CompileCop1Test(Section);	
	if (RegInStack(Section,Opcode.FP.fs,FPU_Double) || RegInStack(Section,Opcode.FP.fs,FPU_Qword)) {
		UnMap_FPR(Section,Opcode.FP.fs,TRUE);
	}
	if (Opcode.FP.fd != Opcode.FP.fs || !RegInStack(Section,Opcode.FP.fd,FPU_Double)) { 
		Load_FPR_ToTop(Section,Opcode.FP.fd,Opcode.FP.fs,FPU_Double); 
	}
	ChangeFPURegFormat(Section,Opcode.FP.fd,FPU_Double,FPU_Dword,RoundDown);
}

void Compile_R4300i_COP1_D_CVT_S (BLOCK_SECTION * Section) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
	
	CompileCop1Test(Section);	
	if (RegInStack(Section,Opcode.FP.fs,FPU_Double) || RegInStack(Section,Opcode.FP.fs,FPU_Qword)) {
		UnMap_FPR(Section,Opcode.FP.fs,TRUE);
	}
	if (Opcode.FP.fd != Opcode.FP.fs || !RegInStack(Section,Opcode.FP.fd,FPU_Double)) { 
		Load_FPR_ToTop(Section,Opcode.FP.fd,Opcode.FP.fs,FPU_Double); 
	}
	ChangeFPURegFormat(Section,Opcode.FP.fd,FPU_Double,FPU_Float,RoundDefault);
}

void Compile_R4300i_COP1_D_CVT_W (BLOCK_SECTION * Section) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
	
	CompileCop1Test(Section);	
	if (RegInStack(Section,Opcode.FP.fs,FPU_Double) || RegInStack(Section,Opcode.FP.fs,FPU_Qword)) {
		UnMap_FPR(Section,Opcode.FP.fs,TRUE);
	}
	if (Opcode.FP.fd != Opcode.FP.fs || !RegInStack(Section,Opcode.FP.fd,FPU_Double)) { 
		Load_FPR_ToTop(Section,Opcode.FP.fd,Opcode.FP.fs,FPU_Double); 
	}
	ChangeFPURegFormat(Section,Opcode.FP.fd,FPU_Double,FPU_Dword,RoundDefault);
}

void Compile_R4300i_COP1_D_CVT_L (BLOCK_SECTION * Section) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
	
	CompileCop1Test(Section);	
	if (RegInStack(Section,Opcode.FP.fs,FPU_Double) || RegInStack(Section,Opcode.FP.fs,FPU_Qword)) {
		UnMap_FPR(Section,Opcode.FP.fs,TRUE);
	}
	if (Opcode.FP.fd != Opcode.FP.fs || !RegInStack(Section,Opcode.FP.fd,FPU_Double)) { 
		Load_FPR_ToTop(Section,Opcode.FP.fd,Opcode.FP.fs,FPU_Double); 
	}
	ChangeFPURegFormat(Section,Opcode.FP.fd,FPU_Double,FPU_Qword,RoundDefault);
}

void Compile_R4300i_COP1_D_CMP (BLOCK_SECTION * Section) {
	DWORD Reg1 = RegInStack(Section,Opcode.FP.ft, FPU_Float)?Opcode.FP.ft:Opcode.FP.fs;
	DWORD Reg2 = RegInStack(Section,Opcode.FP.ft, FPU_Float)?Opcode.FP.fs:Opcode.FP.ft;
	int x86reg, cmp = 0;
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;
	
	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
	
	CompileCop1Test(Section);	
	//if ((Opcode.REG.funct & 1) != 0) { Compile_R4300i_UnknownOpcode(Section); }
	if ((Opcode.REG.funct & 2) != 0) { cmp |= 0x4000; }
	if ((Opcode.REG.funct & 4) != 0) { cmp |= 0x0100; }
	
	Load_FPR_ToTop(Section,Reg1,Reg1, FPU_Double);
	Map_TempReg(Section,x86_EAX, 0, FALSE);
	if (RegInStack(Section,Reg2, FPU_Double)) {
		fpuComReg(StackPosition(Section,Reg2),FALSE);
	} else {
		DWORD TempReg;

		UnMap_FPR(Section,Reg2,TRUE);
		TempReg = Map_TempReg(Section,x86_Any,-1,FALSE);
		sprintf(Name,"FPRDoubleLocation[%d]",Reg2);
		MoveVariableToX86reg((BYTE *)&FPRDoubleLocation[Reg2],Name,TempReg);
		Load_FPR_ToTop(Section,Reg1,Reg1, FPU_Double);
		fpuComQwordRegPointer(TempReg,FALSE);
	}
	AndConstToVariable((DWORD)~FPCSR_C, &FSTATUS_REGISTER, "FSTATUS_REGISTER");
	fpuStoreStatus();
	x86reg = Map_TempReg(Section,x86_Any8Bit, 0, FALSE);
	TestConstToX86Reg(cmp,x86_EAX);	
	Setnz(x86reg);
	if (cmp != 0) {
		TestConstToX86Reg(cmp,x86_EAX);	
		Setnz(x86reg);
		
		if ((Opcode.REG.funct & 1) != 0) {
			int x86reg2 = Map_TempReg(Section,x86_Any8Bit, 0, FALSE);
			AndConstToX86Reg(x86_EAX, 0x4300);
			CompConstToX86reg(x86_EAX, 0x4300);
			Setz(x86reg2);
	
			OrX86RegToX86Reg(x86reg, x86reg2);
		}
	} else if ((Opcode.REG.funct & 1) != 0) {
		AndConstToX86Reg(x86_EAX, 0x4300);
		CompConstToX86reg(x86_EAX, 0x4300);
		Setz(x86reg);
	}
	ShiftLeftSignImmed(x86reg, 23);
	OrX86RegToVariable(&FPCR[31], "FPCR[31]", x86reg);
}

/************************** COP1: W functions ************************/
void Compile_R4300i_COP1_W_CVT_S (BLOCK_SECTION * Section) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
	
	CompileCop1Test(Section);	
	if (Opcode.FP.fd != Opcode.FP.fs || !RegInStack(Section,Opcode.FP.fd,FPU_Dword)) { 
		Load_FPR_ToTop(Section,Opcode.FP.fd,Opcode.FP.fs,FPU_Dword); 
	}
	ChangeFPURegFormat(Section,Opcode.FP.fd,FPU_Dword,FPU_Float,RoundDefault);
}

void Compile_R4300i_COP1_W_CVT_D (BLOCK_SECTION * Section) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
	
	CompileCop1Test(Section);	
	if (Opcode.FP.fd != Opcode.FP.fs || !RegInStack(Section,Opcode.FP.fd,FPU_Dword)) { 
		Load_FPR_ToTop(Section,Opcode.FP.fd,Opcode.FP.fs,FPU_Dword); 
	}
	ChangeFPURegFormat(Section,Opcode.FP.fd,FPU_Dword,FPU_Double,RoundDefault);
}

/************************** COP1: L functions ************************/
void Compile_R4300i_COP1_L_CVT_S (BLOCK_SECTION * Section) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
	
	CompileCop1Test(Section);	
	if (Opcode.FP.fd != Opcode.FP.fs || !RegInStack(Section,Opcode.FP.fd,FPU_Qword)) { 
		Load_FPR_ToTop(Section,Opcode.FP.fd,Opcode.FP.fs,FPU_Qword); 
	}
	ChangeFPURegFormat(Section,Opcode.FP.fd,FPU_Qword,FPU_Float,RoundDefault);
}

void Compile_R4300i_COP1_L_CVT_D (BLOCK_SECTION * Section) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
	
	CompileCop1Test(Section);	
	if (Opcode.FP.fd != Opcode.FP.fs || !RegInStack(Section,Opcode.FP.fd,FPU_Qword)) { 
		Load_FPR_ToTop(Section,Opcode.FP.fd,Opcode.FP.fs,FPU_Qword); 
	}
	ChangeFPURegFormat(Section,Opcode.FP.fd,FPU_Qword,FPU_Double,RoundDefault);
}
