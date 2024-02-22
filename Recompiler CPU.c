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
#include <commctrl.h>
#include <stdio.h>
#include "main.h"
#include "cpu.h"
#include "x86.h"
#include "debugger.h"
#include "plugin.h"

void _fastcall CreateSectionLinkage (BLOCK_SECTION * Section);
void _fastcall DetermineLoop(BLOCK_SECTION * Section, DWORD Test, DWORD Test2, DWORD TestID);
BOOL DisplaySectionInformation (BLOCK_SECTION * Section, DWORD ID, DWORD Test);
BLOCK_SECTION * ExistingSection(BLOCK_SECTION * StartSection, DWORD Addr, DWORD Test);
void _fastcall FillSectionInfo(BLOCK_SECTION * Section);
void _fastcall FixConstants ( BLOCK_SECTION * Section, DWORD Test,int * Changed );
BOOL GenerateX86Code (BLOCK_SECTION * Section, DWORD Test );
DWORD GetNewTestValue( void );
void _fastcall InheritConstants(BLOCK_SECTION * Section);
BOOL InheritParentInfo (BLOCK_SECTION * Section);
void _fastcall InitilzeSection(BLOCK_SECTION * Section, BLOCK_SECTION * Parent, DWORD StartAddr, DWORD ID);
void InitilizeRegSet(REG_INFO * RegSet);
BOOL IsAllParentLoops(BLOCK_SECTION * Section, BLOCK_SECTION * Parent, BOOL IgnoreIfCompiled, DWORD Test);
void MarkCodeBlock (DWORD PAddr);
void SyncRegState (BLOCK_SECTION * Section, REG_INFO * SyncTo);

DWORD TLBLoadAddress, TargetIndex;
TARGET_INFO * TargetInfo = NULL;
BLOCK_INFO BlockInfo;
ORIGINAL_MEMMARKER * OrigMem = NULL;

void InitilizeInitialCompilerVariable ( void)
{
	memset(&BlockInfo,0,sizeof(BlockInfo));
}

void _fastcall AddParent(BLOCK_SECTION * Section, BLOCK_SECTION * Parent){
	int NoOfParents, count;


	if (Section == NULL) { return; }
	if (Parent == NULL) {
		InitilizeRegSet(&Section->RegStart);
		memcpy(&Section->RegWorking,&Section->RegStart,sizeof(REG_INFO));
		return;
	}
	
	if (Section->ParentSection != NULL) {	
		for (NoOfParents = 0;Section->ParentSection[NoOfParents] != NULL;NoOfParents++) {
			if (Section->ParentSection[NoOfParents] == Parent) {
				return;
			}
		}
		for (NoOfParents = 0;Section->ParentSection[NoOfParents] != NULL;NoOfParents++);
		NoOfParents += 1;
	} else {
		NoOfParents = 1;
	}

	if (NoOfParents == 1) {
		Section->ParentSection = malloc((NoOfParents + 1)*sizeof(void *));
	} else {
		Section->ParentSection = realloc(Section->ParentSection,(NoOfParents + 1)*sizeof(void *));
	}
	Section->ParentSection[NoOfParents - 1] = Parent;
	Section->ParentSection[NoOfParents] = NULL;

	if (NoOfParents == 1) {
		if (Parent->ContinueSection == Section) {
			memcpy(&Section->RegStart,&Parent->Cont.RegSet,sizeof(REG_INFO));
		} else if (Parent->JumpSection == Section) {
			memcpy(&Section->RegStart,&Parent->Jump.RegSet,sizeof(REG_INFO));
		} else {
			if (ShowDebugMessages)
				DisplayError("How are these sections joined?????");
		}
		memcpy(&Section->RegWorking,&Section->RegStart,sizeof(REG_INFO));
	} else {
		if (Parent->ContinueSection == Section) {
			for (count = 0; count < 32; count++) {
				if (Section->RegStart.MIPS_RegState[count] != Parent->Cont.RegSet.MIPS_RegState[count]) {
					Section->RegStart.MIPS_RegState[count] = STATE_UNKNOWN;
				}
			}
		}
		if (Parent->JumpSection == Section) {
			for (count = 0; count < 32; count++) {
				if (Section->RegStart.MIPS_RegState[count] != Parent->Jump.RegSet.MIPS_RegState[count]) {
					Section->RegStart.MIPS_RegState[count] = STATE_UNKNOWN;
				}
			}
		}
		memcpy(&Section->RegWorking,&Section->RegStart,sizeof(REG_INFO));
	}
}

void AnalyseBlock (void) {
	int Changed;

	BLOCK_SECTION * Section = &BlockInfo.BlockInfo;

	BlockInfo.NoOfSections = 1;
	InitilzeSection (Section, NULL, BlockInfo.StartVAddr, BlockInfo.NoOfSections);
	if (UseLinking) { 
		CreateSectionLinkage (Section);
		DetermineLoop(Section,GetNewTestValue(),GetNewTestValue(), Section->SectionID);
		do {
			Changed = FALSE;
			FixConstants(Section,GetNewTestValue(),&Changed);
		} while (Changed == TRUE);
	}
}

int ConstantsType (__int64 Value) {
	if (((Value >> 32) == -1) && ((Value & 0x80000000) != 0)) { return STATE_CONST_32; } 
	if (((Value >> 32) == 0) && ((Value & 0x80000000) == 0)) { return STATE_CONST_32; } 
	return STATE_CONST_64;
}

BYTE * Compiler4300iBlock(void) {
	DWORD StartAddress;
	int count;

	//reset BlockInfo	
	if (BlockInfo.ExitInfo)
	{
		for (count = 0; count < BlockInfo.ExitCount; count ++) {
			free(BlockInfo.ExitInfo[count]);
		}
		if (BlockInfo.ExitInfo) { free(BlockInfo.ExitInfo); }
		BlockInfo.ExitInfo = NULL;
	}

	memset(&BlockInfo,0,sizeof(BlockInfo));
	BlockInfo.CompiledLocation = RecompPos;
	BlockInfo.StartVAddr = PROGRAM_COUNTER.UW[0];

	AnalyseBlock();
	
	StartAddress = BlockInfo.StartVAddr;
	TranslateVaddr(&StartAddress);

	MarkCodeBlock(StartAddress);
	if (StartAddress < RdramSize) {
		CPU_Message("====== RDRAM: block (%X:%d) ======", StartAddress>>12,N64_Blocks.NoOfRDRamBlocks[StartAddress>>12]);
	} else if (StartAddress >= 0x04000000 && StartAddress <= 0x04000FFC) {
		CPU_Message("====== DMEM: block (%d) ======", N64_Blocks.NoOfDMEMBlocks);
	} else if (StartAddress >= 0x04001000 && StartAddress <= 0x04001FFC) {
		CPU_Message("====== IMEM: block (%d) ======", N64_Blocks.NoOfIMEMBlocks);
	} else if (StartAddress >= 0x1FC00000 && StartAddress <= 0x1FC00800) {
		CPU_Message("====== PIF ROM: block ======");
	} else {
		if (ShowDebugMessages)
			DisplayError("Ummm... Where does this block go");
		ExitThread(0);			
	}
	CPU_Message("x86 code at: %X",BlockInfo.CompiledLocation);
	CPU_Message("Start of Block: %X",BlockInfo.StartVAddr );
	CPU_Message("No of Sections: %d",BlockInfo.NoOfSections );
	CPU_Message("====== recompiled code ======");
	if (UseLinking) {
		/*for (count = 0; count < BlockInfo.NoOfSections; count ++) {
			DisplaySectionInformation(&BlockInfo.BlockInfo,count + 1,BlockInfo.BlockInfo.Test + 1);
		}*/
	}
	if (CPU_Type == CPU_SyncCores) {
		//if ((DWORD)BlockInfo.CompiledLocation == 0x60A7B73B) { BreakPoint(); }
		MoveConstToVariable((DWORD)BlockInfo.CompiledLocation,&CurrentBlock,"CurrentBlock");
	}
	
	if (UseLinking) {
		while (GenerateX86Code(&BlockInfo.BlockInfo,GetNewTestValue()));
	} else {
		GenerateX86Code(&BlockInfo.BlockInfo,GetNewTestValue());
	}
	for (count = 0; count < BlockInfo.ExitCount; count ++) {
		CPU_Message("");
		CPU_Message("      $Exit_%d",count);
		SetJump32(BlockInfo.ExitInfo[count]->JumpLoc,RecompPos);	
		NextInstruction = BlockInfo.ExitInfo[count]->NextInstruction;
		CompileExit(BlockInfo.ExitInfo[count]->TargetPC,BlockInfo.ExitInfo[count]->ExitRegSet,
			BlockInfo.ExitInfo[count]->reason,TRUE,NULL);
	}	
	CPU_Message("====== End of recompiled code ======");
	FreeSection (BlockInfo.BlockInfo.ContinueSection,&BlockInfo.BlockInfo);
	FreeSection (BlockInfo.BlockInfo.JumpSection,&BlockInfo.BlockInfo);
	for (count = 0; count < BlockInfo.ExitCount; count ++) {
		free(BlockInfo.ExitInfo[count]);
	}
	if (BlockInfo.ExitInfo) { free(BlockInfo.ExitInfo); }
	BlockInfo.ExitInfo = NULL;
	BlockInfo.ExitCount = 0;

	if (ShowCompMem) {
		char StatusString[256];		
		DWORD Size, MB, KB;
		
		Size = RecompPos - RecompCode;
		MB = Size / 0x100000;
		Size -= MB * 0x100000;
		KB = Size / 1024;
		Size -= KB  * 1024;
		sprintf(StatusString,"Memory used: %d mb %d kb %d bytes ",MB,KB,Size);
		SendMessage( hStatusWnd, SB_SETTEXT, 0, (LPARAM)StatusString );
	}

	return BlockInfo.CompiledLocation;
}

BYTE * CompileDelaySlot(void) {
	DWORD StartAddress = PROGRAM_COUNTER.UW[0];
	BLOCK_SECTION *Section, DelaySection;
	BYTE * Block = RecompPos;
	int count, x86Reg;

	Section = &DelaySection;

	if ((StartAddress & 0xFFC) != 0) {
		if (ShowDebugMessages)
			DisplayError("Why are you compiling the Delay Slot at %X",StartAddress);
		ExitThread(0);
	}
	MIPS_DWORD Address;
	Address.DW = (long)StartAddress;
	if (!r4300i_LW_VAddr_NonCPU(Address, &Opcode.Hex)) {
		if (ShowDebugMessages)
			DisplayError("TLB Miss in delay slot\nEmulation will now stop");
		ExitThread(0);
	} 

	TranslateVaddr(&StartAddress);
	if (StartAddress < RdramSize) {
		CPU_Message("====== RDRAM: Delay Slot ======", 1);
	} else if (StartAddress >= 0x04000000 && StartAddress <= 0x04000FFC) {
		CPU_Message("====== DMEM: Delay Slot ======");
	} else if (StartAddress >= 0x04001000 && StartAddress <= 0x04001FFC) {
		CPU_Message("====== IMEM: Delay Slot ======");
	} else if (StartAddress >= 0x1FC00000 && StartAddress <= 0x1FC00800) {
		CPU_Message("====== PIF ROM: Delay Slot ======");
	} else {
		if (ShowDebugMessages)
			DisplayError("Ummm... Where does this block go");
		ExitThread(0);
	}
	MarkCodeBlock(StartAddress);
	CPU_Message("x86 code at: %X",Block);
	CPU_Message("Delay Slot location: %X",PROGRAM_COUNTER.UW[0] );
	CPU_Message("====== recompiled code ======");

	InitilzeSection (Section, NULL, PROGRAM_COUNTER.UW[0], 0);
	InitilizeRegSet(&Section->RegStart);
	memcpy(&Section->RegWorking,&Section->RegStart,sizeof(REG_INFO));		

	if (CPU_Type == CPU_SyncCores) {
		MoveConstToVariable((DWORD)Block,&CurrentBlock,"CurrentBlock");
	}

	BlockCycleCount += CountPerOp;

	//CPU_Message("BlockCycleCount = %d",BlockCycleCount);
	BlockRandomModifier += 1;
	//CPU_Message("BlockRandomModifier = %d",BlockRandomModifier);

	switch (Opcode.BRANCH.op) {
	case R4300i_SPECIAL:
		switch (Opcode.REG.funct) {
		case R4300i_SPECIAL_SYNC: break;
		case R4300i_SPECIAL_SLL: Compile_R4300i_SPECIAL_SLL(Section); break;
		case R4300i_SPECIAL_SRL: Compile_R4300i_SPECIAL_SRL(Section); break;
		case R4300i_SPECIAL_SRA: Compile_R4300i_SPECIAL_SRA(Section); break;
		case R4300i_SPECIAL_SLLV: Compile_R4300i_SPECIAL_SLLV(Section); break;
		case R4300i_SPECIAL_SRLV: Compile_R4300i_SPECIAL_SRLV(Section); break;
		case R4300i_SPECIAL_SRAV: Compile_R4300i_SPECIAL_SRAV(Section); break;
		case R4300i_SPECIAL_MFLO: Compile_R4300i_SPECIAL_MFLO(Section); break;
		case R4300i_SPECIAL_MTLO: Compile_R4300i_SPECIAL_MTLO(Section); break;
		case R4300i_SPECIAL_MFHI: Compile_R4300i_SPECIAL_MFHI(Section); break;
		case R4300i_SPECIAL_MTHI: Compile_R4300i_SPECIAL_MTHI(Section); break;
		case R4300i_SPECIAL_DSLLV: Compile_R4300i_SPECIAL_DSLLV(Section); break;
		case R4300i_SPECIAL_DSRLV: Compile_R4300i_SPECIAL_DSRLV(Section); break;
		case R4300i_SPECIAL_DSRAV: Compile_R4300i_SPECIAL_DSRAV(Section); break;
		case R4300i_SPECIAL_MULT: Compile_R4300i_SPECIAL_MULT(Section); break;
		case R4300i_SPECIAL_DIV: Compile_R4300i_SPECIAL_DIV(Section); break;
		case R4300i_SPECIAL_DIVU: Compile_R4300i_SPECIAL_DIVU(Section); break;
		case R4300i_SPECIAL_MULTU: Compile_R4300i_SPECIAL_MULTU(Section); break;
		case R4300i_SPECIAL_DMULT: Compile_R4300i_SPECIAL_DMULT(Section); break;
		case R4300i_SPECIAL_DMULTU: Compile_R4300i_SPECIAL_DMULTU(Section); break;
		case R4300i_SPECIAL_DDIV: Compile_R4300i_SPECIAL_DDIV(Section); break;
		case R4300i_SPECIAL_DDIVU: Compile_R4300i_SPECIAL_DDIVU(Section); break;
		case R4300i_SPECIAL_ADD: Compile_R4300i_SPECIAL_ADD(Section); break;
		case R4300i_SPECIAL_ADDU: Compile_R4300i_SPECIAL_ADDU(Section); break;
		case R4300i_SPECIAL_SUB: Compile_R4300i_SPECIAL_SUB(Section); break;
		case R4300i_SPECIAL_SUBU: Compile_R4300i_SPECIAL_SUBU(Section); break;
		case R4300i_SPECIAL_AND: Compile_R4300i_SPECIAL_AND(Section); break;
		case R4300i_SPECIAL_OR: Compile_R4300i_SPECIAL_OR(Section); break;
		case R4300i_SPECIAL_XOR: Compile_R4300i_SPECIAL_XOR(Section); break;
		case R4300i_SPECIAL_NOR: Compile_R4300i_SPECIAL_NOR(Section); break;
		case R4300i_SPECIAL_SLT: Compile_R4300i_SPECIAL_SLT(Section); break;
		case R4300i_SPECIAL_SLTU: Compile_R4300i_SPECIAL_SLTU(Section); break;
		case R4300i_SPECIAL_DADD: Compile_R4300i_SPECIAL_DADD(Section); break;
		case R4300i_SPECIAL_DADDU: Compile_R4300i_SPECIAL_DADDU(Section); break;
		case R4300i_SPECIAL_DSUB: Compile_R4300i_SPECIAL_DSUB(Section); break;
		case R4300i_SPECIAL_DSUBU: Compile_R4300i_SPECIAL_DSUBU(Section); break;
		case R4300i_SPECIAL_DSLL: Compile_R4300i_SPECIAL_DSLL(Section); break;
		case R4300i_SPECIAL_DSRL: Compile_R4300i_SPECIAL_DSRL(Section); break;
		case R4300i_SPECIAL_DSRA: Compile_R4300i_SPECIAL_DSRA(Section); break;
		case R4300i_SPECIAL_DSLL32: Compile_R4300i_SPECIAL_DSLL32(Section); break;
		case R4300i_SPECIAL_DSRL32: Compile_R4300i_SPECIAL_DSRL32(Section); break;
		case R4300i_SPECIAL_DSRA32: Compile_R4300i_SPECIAL_DSRA32(Section); break;
		default:
			Compile_R4300i_UnknownOpcode(Section); break;
		}
		break;
	case R4300i_ADDI: Compile_R4300i_ADDI(Section); break;
	case R4300i_ADDIU: Compile_R4300i_ADDIU(Section); break;
	case R4300i_SLTI: Compile_R4300i_SLTI(Section); break;
	case R4300i_SLTIU: Compile_R4300i_SLTIU(Section); break;
	case R4300i_ANDI: Compile_R4300i_ANDI(Section); break;
	case R4300i_ORI: Compile_R4300i_ORI(Section); break;
	case R4300i_XORI: Compile_R4300i_XORI(Section); break;
	case R4300i_LUI: Compile_R4300i_LUI(Section); break;
	case R4300i_CP0:
		switch (Opcode.BRANCH.rs) {
		case R4300i_COP0_MF: Compile_R4300i_COP0_MF(Section); break;
		case R4300i_COP0_DMF: Compile_R4300i_COP0_DMF(Section); break;
		case R4300i_COP0_MT: Compile_R4300i_COP0_MT(Section); break;
		case R4300i_COP0_DMT: Compile_R4300i_COP0_DMT(Section); break;
		default:
			if ((Opcode.BRANCH.rs & 0x10) != 0) {
				switch (Opcode.REG.funct) {
				case R4300i_COP0_CO_TLBR: Compile_R4300i_COP0_CO_TLBR(Section); break;
				case R4300i_COP0_CO_TLBWI: Compile_R4300i_COP0_CO_TLBWI(Section); break;
				case R4300i_COP0_CO_TLBWR: Compile_R4300i_COP0_CO_TLBWR(Section); break;
				case R4300i_COP0_CO_TLBP: Compile_R4300i_COP0_CO_TLBP(Section); break;
				case R4300i_COP0_CO_ERET: Compile_R4300i_COP0_CO_ERET(Section); break;
				default: Compile_R4300i_UnknownOpcode(Section); break;
				}
			} else {
				Compile_R4300i_UnknownOpcode(Section);
			}
		}
		break;
	case R4300i_CP1:
		switch (Opcode.BRANCH.rs) {
		case R4300i_COP1_MF: Compile_R4300i_COP1_MF(Section); break;
		case R4300i_COP1_DMF: Compile_R4300i_COP1_DMF(Section); break;
		case R4300i_COP1_CF: Compile_R4300i_COP1_CF(Section); break;
		case R4300i_COP1_MT: Compile_R4300i_COP1_MT(Section); break;
		case R4300i_COP1_DMT: Compile_R4300i_COP1_DMT(Section); break;
		case R4300i_COP1_CT: Compile_R4300i_COP1_CT(Section); break;
		case R4300i_COP1_S: 
			switch (Opcode.REG.funct) {
			case R4300i_COP1_FUNCT_ADD: Compile_R4300i_COP1_S_ADD(Section); break;
			case R4300i_COP1_FUNCT_SUB: Compile_R4300i_COP1_S_SUB(Section); break;
			case R4300i_COP1_FUNCT_MUL: Compile_R4300i_COP1_S_MUL(Section); break;
			case R4300i_COP1_FUNCT_DIV: Compile_R4300i_COP1_S_DIV(Section); break;
			case R4300i_COP1_FUNCT_ABS: Compile_R4300i_COP1_S_ABS(Section); break;
			case R4300i_COP1_FUNCT_NEG: Compile_R4300i_COP1_S_NEG(Section); break;
			case R4300i_COP1_FUNCT_SQRT: Compile_R4300i_COP1_S_SQRT(Section); break;
			case R4300i_COP1_FUNCT_MOV: Compile_R4300i_COP1_S_MOV(Section); break;
			case R4300i_COP1_FUNCT_TRUNC_L: Compile_R4300i_COP1_S_TRUNC_L(Section); break;
			case R4300i_COP1_FUNCT_CEIL_L: Compile_R4300i_COP1_S_CEIL_L(Section); break;
			case R4300i_COP1_FUNCT_FLOOR_L: Compile_R4300i_COP1_S_FLOOR_L(Section); break;
			case R4300i_COP1_FUNCT_ROUND_W: Compile_R4300i_COP1_S_ROUND_W(Section); break;
			case R4300i_COP1_FUNCT_TRUNC_W: Compile_R4300i_COP1_S_TRUNC_W(Section); break;
			case R4300i_COP1_FUNCT_CEIL_W: Compile_R4300i_COP1_S_CEIL_W(Section); break;
			case R4300i_COP1_FUNCT_FLOOR_W: Compile_R4300i_COP1_S_FLOOR_W(Section); break;
			case R4300i_COP1_FUNCT_CVT_D: Compile_R4300i_COP1_S_CVT_D(Section); break;
			case R4300i_COP1_FUNCT_CVT_W: Compile_R4300i_COP1_S_CVT_W(Section); break;
			case R4300i_COP1_FUNCT_CVT_L: Compile_R4300i_COP1_S_CVT_L(Section); break;
			case R4300i_COP1_FUNCT_C_F:   case R4300i_COP1_FUNCT_C_UN:
			case R4300i_COP1_FUNCT_C_EQ:  case R4300i_COP1_FUNCT_C_UEQ:
			case R4300i_COP1_FUNCT_C_OLT: case R4300i_COP1_FUNCT_C_ULT:
			case R4300i_COP1_FUNCT_C_OLE: case R4300i_COP1_FUNCT_C_ULE:
			case R4300i_COP1_FUNCT_C_SF:  case R4300i_COP1_FUNCT_C_NGLE:
			case R4300i_COP1_FUNCT_C_SEQ: case R4300i_COP1_FUNCT_C_NGL:
			case R4300i_COP1_FUNCT_C_LT:  case R4300i_COP1_FUNCT_C_NGE:
			case R4300i_COP1_FUNCT_C_LE:  case R4300i_COP1_FUNCT_C_NGT:
				Compile_R4300i_COP1_S_CMP(Section); break;
			default:
				Compile_R4300i_UnknownOpcode(Section); break;
			}
			break;
		case R4300i_COP1_D: 
			switch (Opcode.REG.funct) {
			case R4300i_COP1_FUNCT_ADD: Compile_R4300i_COP1_D_ADD(Section); break;
			case R4300i_COP1_FUNCT_SUB: Compile_R4300i_COP1_D_SUB(Section); break;
			case R4300i_COP1_FUNCT_MUL: Compile_R4300i_COP1_D_MUL(Section); break;
			case R4300i_COP1_FUNCT_DIV: Compile_R4300i_COP1_D_DIV(Section); break;
			case R4300i_COP1_FUNCT_ABS: Compile_R4300i_COP1_D_ABS(Section); break;
			case R4300i_COP1_FUNCT_NEG: Compile_R4300i_COP1_D_NEG(Section); break;
			case R4300i_COP1_FUNCT_SQRT: Compile_R4300i_COP1_D_SQRT(Section); break;
			case R4300i_COP1_FUNCT_MOV: Compile_R4300i_COP1_D_MOV(Section); break;
			case R4300i_COP1_FUNCT_TRUNC_L: Compile_R4300i_COP1_D_TRUNC_L(Section); break;
			case R4300i_COP1_FUNCT_CEIL_L: Compile_R4300i_COP1_D_CEIL_L(Section); break;
			case R4300i_COP1_FUNCT_FLOOR_L: Compile_R4300i_COP1_D_FLOOR_L(Section); break;
			case R4300i_COP1_FUNCT_ROUND_W: Compile_R4300i_COP1_D_ROUND_W(Section); break;
			case R4300i_COP1_FUNCT_TRUNC_W: Compile_R4300i_COP1_D_TRUNC_W(Section); break;
			case R4300i_COP1_FUNCT_CEIL_W: Compile_R4300i_COP1_D_CEIL_W(Section); break;
			case R4300i_COP1_FUNCT_FLOOR_W: Compile_R4300i_COP1_D_FLOOR_W(Section); break;
			case R4300i_COP1_FUNCT_CVT_S: Compile_R4300i_COP1_D_CVT_S(Section); break;
			case R4300i_COP1_FUNCT_CVT_W: Compile_R4300i_COP1_D_CVT_W(Section); break;
			case R4300i_COP1_FUNCT_CVT_L: Compile_R4300i_COP1_D_CVT_L(Section); break;
			case R4300i_COP1_FUNCT_C_F:   case R4300i_COP1_FUNCT_C_UN:
			case R4300i_COP1_FUNCT_C_EQ:  case R4300i_COP1_FUNCT_C_UEQ:
			case R4300i_COP1_FUNCT_C_OLT: case R4300i_COP1_FUNCT_C_ULT:
			case R4300i_COP1_FUNCT_C_OLE: case R4300i_COP1_FUNCT_C_ULE:
			case R4300i_COP1_FUNCT_C_SF:  case R4300i_COP1_FUNCT_C_NGLE:
			case R4300i_COP1_FUNCT_C_SEQ: case R4300i_COP1_FUNCT_C_NGL:
			case R4300i_COP1_FUNCT_C_LT:  case R4300i_COP1_FUNCT_C_NGE:
			case R4300i_COP1_FUNCT_C_LE:  case R4300i_COP1_FUNCT_C_NGT:
				Compile_R4300i_COP1_D_CMP(Section); break;
			default:
				Compile_R4300i_UnknownOpcode(Section); break;
			}
			break;
		case R4300i_COP1_W: 
			switch (Opcode.REG.funct) {
			case R4300i_COP1_FUNCT_CVT_S: Compile_R4300i_COP1_W_CVT_S(Section); break;
			case R4300i_COP1_FUNCT_CVT_D: Compile_R4300i_COP1_W_CVT_D(Section); break;
			default:
				Compile_R4300i_UnknownOpcode(Section); break;
			}
			break;
		case R4300i_COP1_L:
			switch (Opcode.REG.funct) {
			case R4300i_COP1_FUNCT_CVT_S: Compile_R4300i_COP1_L_CVT_S(Section); break;
			case R4300i_COP1_FUNCT_CVT_D: Compile_R4300i_COP1_L_CVT_D(Section); break;
			default:
				Compile_R4300i_UnknownOpcode(Section); break;
			}
			break;
		default:
			Compile_R4300i_UnknownOpcode(Section); break;
		}
		break;
	case R4300i_DADDI: Compile_R4300i_DADDI(Section); break;
	case R4300i_DADDIU: Compile_R4300i_DADDIU(Section); break;
	case R4300i_LDL: Compile_R4300i_LDL(Section); break;
	case R4300i_LDR: Compile_R4300i_LDR(Section); break;
	case R4300i_LB: Compile_R4300i_LB(Section); break;
	case R4300i_LH: Compile_R4300i_LH(Section); break;
	case R4300i_LWL: Compile_R4300i_LWL(Section); break;
	case R4300i_LW: Compile_R4300i_LW(Section); break;
	case R4300i_LBU: Compile_R4300i_LBU(Section); break;
	case R4300i_LHU: Compile_R4300i_LHU(Section); break;
	case R4300i_LWR: Compile_R4300i_LWR(Section); break;
	case R4300i_LWU: Compile_R4300i_LWU(Section); break;
	case R4300i_SB: Compile_R4300i_SB(Section); break;
	case R4300i_SH: Compile_R4300i_SH(Section); break;
	case R4300i_SWL: Compile_R4300i_SWL(Section); break;
	case R4300i_SW: Compile_R4300i_SW(Section); break;
	case R4300i_SWR: Compile_R4300i_SWR(Section); break;
	case R4300i_SDL: Compile_R4300i_SDL(Section); break;
	case R4300i_SDR: Compile_R4300i_SDR(Section); break;
	case R4300i_CACHE: Compile_R4300i_CACHE(Section); break;
	case R4300i_LL: Compile_R4300i_LL(Section); break;
	case R4300i_LWC1: Compile_R4300i_LWC1(Section); break;
	case R4300i_LDC1: Compile_R4300i_LDC1(Section); break;
	case R4300i_SC: Compile_R4300i_SC(Section); break;
	case R4300i_LD: Compile_R4300i_LD(Section); break;
	case R4300i_SWC1: Compile_R4300i_SWC1(Section); break;
	case R4300i_SDC1: Compile_R4300i_SDC1(Section); break;
	case R4300i_SD: Compile_R4300i_SD(Section); break;
	default:
		Compile_R4300i_UnknownOpcode(Section); break;
	}

	for (count = 1; count < 10; count ++) { x86Protected(count) = FALSE; }
	
	WriteBackRegisters(Section);
	if (BlockCycleCount != 0) { 
		AddConstToVariable(BlockCycleCount,&CP0[9],Cop0_Name[9]); 
		SubConstFromVariable(BlockCycleCount,&Timers.Timer,"Timer");
	}
	if (BlockRandomModifier != 0) { SubConstFromVariable(BlockRandomModifier,&CP0[1],Cop0_Name[1]); }
	x86Reg = Map_TempReg(Section,x86_Any,-1,FALSE);
	MoveVariableToX86reg(&JumpToLocation,"JumpToLocation",x86Reg);
	MoveX86regToVariable(x86Reg,&PROGRAM_COUNTER,"PROGRAM_COUNTER");
	MoveConstToVariable(NORMAL,&NextInstruction,"NextInstruction");
	if (CPU_Type == CPU_SyncCores) { Call_Direct(SyncToPC, "SyncToPC"); }
	Ret();
	CPU_Message("====== End of recompiled code ======");
	return Block;
}

void CompileExit (DWORD TargetPC, REG_INFO ExitRegSet, int reason, int CompileNow, void (*x86Jmp)(char * Label, DWORD Value)) {
	BLOCK_SECTION Section;
		
	if (!CompileNow) {
		char String[100];
		if (BlockInfo.ExitCount == 0) {
			BlockInfo.ExitInfo = malloc(sizeof(void *));
		} else {
			BlockInfo.ExitInfo = realloc(BlockInfo.ExitInfo,(BlockInfo.ExitCount + 1) * sizeof(void *));
		}
		sprintf(String,"Exit_%d",BlockInfo.ExitCount);
		if (x86Jmp == NULL) { 
			if (ShowDebugMessages)
				DisplayError("CompileExit error");
			ExitThread(0);
		}
		x86Jmp(String,0);
		BlockInfo.ExitInfo[BlockInfo.ExitCount] = malloc(sizeof(EXIT_INFO));
		BlockInfo.ExitInfo[BlockInfo.ExitCount]->TargetPC = TargetPC;
		BlockInfo.ExitInfo[BlockInfo.ExitCount]->ExitRegSet = ExitRegSet;
		BlockInfo.ExitInfo[BlockInfo.ExitCount]->reason = reason;
		BlockInfo.ExitInfo[BlockInfo.ExitCount]->NextInstruction = NextInstruction;
		BlockInfo.ExitInfo[BlockInfo.ExitCount]->JumpLoc = RecompPos - 4;
		BlockInfo.ExitCount += 1;
		return;
	}
	
	//CPU_Message("CompileExit: %d",reason);
	InitilzeSection (&Section, NULL, (DWORD)-1, 0);
	memcpy(&Section.RegWorking, &ExitRegSet, sizeof(REG_INFO));

	if (TargetPC != (DWORD)-1) { MoveConstToVariable(TargetPC,&PROGRAM_COUNTER,"PROGRAM_COUNTER"); }
	if (ExitRegSet.CycleCount != 0) { 
		AddConstToVariable(ExitRegSet.CycleCount,&CP0[9],Cop0_Name[9]); 
		SubConstFromVariable(ExitRegSet.CycleCount,&Timers.Timer,"Timer");
	}
	if (ExitRegSet.RandomModifier != 0) { SubConstFromVariable(ExitRegSet.RandomModifier,&CP0[1],Cop0_Name[1]); }
	WriteBackRegisters(&Section);

	switch (reason) {
	case Normal: case Normal_NoSysCheck:
		if (CPU_Type == CPU_SyncCores) { Call_Direct(SyncToPC, "SyncToPC"); }
		Section.RegWorking.RandomModifier = 0;
		Section.RegWorking.CycleCount = 0;
		if (reason == Normal) { CompileSystemCheck(0,(DWORD)-1,Section.RegWorking);	}
#ifdef LinkBlocks
		if (SelfModCheck == ModCode_ChangeMemory) {
			BYTE * Jump, * Jump2;
			if (TargetPC >= 0x80000000 && TargetPC < 0xC0000000) {
				DWORD pAddr = TargetPC & 0x1FFFFFFF;
	
				MoveVariableToX86reg((BYTE *)N64MEM + pAddr,"N64MEM + pAddr",x86_EAX);
				Jump2 = NULL;
			} else {				
				MoveConstToX86reg((TargetPC >> 12),x86_ECX);
				MoveConstToX86reg(TargetPC,x86_EBX);
				MoveVariableDispToX86Reg(TLB_ReadMap,"TLB_ReadMap",x86_ECX,x86_ECX,4);
				TestX86RegToX86Reg(x86_ECX,x86_ECX);
				JeLabel8("NoTlbEntry",0);
				Jump2 = RecompPos - 1;
				MoveX86regPointerToX86reg(x86_ECX, x86_EBX,x86_EAX);
			}
			MoveX86RegToX86Reg(x86_EAX,x86_ECX);
			AndConstToX86Reg(x86_ECX,0xFFFF0000);
			CompConstToX86reg(x86_ECX,0x7C7C0000);
			JneLabel8("NoCode",0);
			Jump = RecompPos - 1;
			AndConstToX86Reg(x86_EAX,0xFFFF);
			ShiftLeftSignImmed(x86_EAX,4);
			AddConstToX86Reg(x86_EAX,0xC);
			MoveVariableDispToX86Reg(OrigMem,"OrigMem",x86_ECX,x86_EAX,1);
			JmpDirectReg(x86_ECX);
			CPU_Message("      NoCode:");
			*((BYTE *)(Jump))=(BYTE)(RecompPos - Jump - 1);
			if (Jump2 != NULL) {
				CPU_Message("      NoTlbEntry:");
				*((BYTE *)(Jump2))=(BYTE)(RecompPos - Jump2 - 1);
			}
		} else if (SelfModCheck == ModCode_CheckMemoryCache) {
		} else if (SelfModCheck == ModCode_CheckMemory2) { // *** Add in Build 53
		} else {
			BYTE * Jump, * Jump2;
			if (TargetPC >= 0x80000000 && TargetPC < 0x90000000) {
				DWORD pAddr = TargetPC & 0x1FFFFFFF;
	
				MoveVariableToX86reg((BYTE *)JumpTable + pAddr,"JumpTable + pAddr",x86_ECX);
				Jump2 = NULL;
			} else if (TargetPC >= 0x90000000 && TargetPC < 0xC0000000) {
			} else {				
				MoveConstToX86reg((TargetPC >> 12),x86_ECX);
				MoveConstToX86reg(TargetPC,x86_EBX);
				MoveVariableDispToX86Reg(TLB_ReadMap,"TLB_ReadMap",x86_ECX,x86_ECX,4);
				TestX86RegToX86Reg(x86_ECX,x86_ECX);
				JeLabel8("NoTlbEntry",0);
				Jump2 = RecompPos - 1;
				AddConstToX86Reg(x86_ECX,(DWORD)JumpTable - (DWORD)N64MEM);
				MoveX86regPointerToX86reg(x86_ECX, x86_EBX,x86_ECX);
			}
			if (TargetPC < 0x90000000 || TargetPC >= 0xC0000000)
			{
				JecxzLabel8("NullPointer",0);
				Jump = RecompPos - 1;
				JmpDirectReg(x86_ECX);
				CPU_Message("      NullPointer:");
				*((BYTE *)(Jump))=(BYTE)(RecompPos - Jump - 1);
				if (Jump2 != NULL) {
					CPU_Message("      NoTlbEntry:");
					*((BYTE *)(Jump2))=(BYTE)(RecompPos - Jump2 - 1);
				}
			}
		}
		Ret();
#else
		Ret();
#endif
		break;
	case DoCPU_Action:
		if (CPU_Type == CPU_SyncCores) { Call_Direct(SyncToPC, "SyncToPC"); }
		Call_Direct(DoSomething,"DoSomething");
		if (CPU_Type == CPU_SyncCores) { Call_Direct(SyncToPC, "SyncToPC"); }
		Ret();
		break;
	case DoSysCall:
		MoveConstToX86reg(NextInstruction == JUMP || NextInstruction == DELAY_SLOT, x86_ECX);
		Call_Direct(DoSysCallException, "DoSysCallException");
		if (CPU_Type == CPU_SyncCores) { Call_Direct(SyncToPC, "SyncToPC"); }
		Ret();
		break;
	case DoIlleaglOp:
		MoveConstToX86reg(NextInstruction == JUMP || NextInstruction == DELAY_SLOT, x86_ECX);
		Call_Direct(DoIllegalInstructionException, "DoIllegalInstructionException");
		if (CPU_Type == CPU_SyncCores) { Call_Direct(SyncToPC, "SyncToPC"); }
		Ret();
		break;
	case DoBreak:
		MoveConstToX86reg(NextInstruction == JUMP || NextInstruction == DELAY_SLOT, x86_ECX);
		Call_Direct(DoBreakException, "DoBreakException");
		if (CPU_Type == CPU_SyncCores) { Call_Direct(SyncToPC, "SyncToPC"); }
		Ret();
		break;
	case DoTrap:
		MoveConstToX86reg(NextInstruction == JUMP || NextInstruction == DELAY_SLOT, x86_ECX);
		Call_Direct(DoTrapException, "DoTrapException");
		if (CPU_Type == CPU_SyncCores) { Call_Direct(SyncToPC, "SyncToPC"); }
		Ret();
		break;
	case COP1_Unuseable:
		MoveConstToX86reg(NextInstruction == JUMP || NextInstruction == DELAY_SLOT,x86_ECX);		
		MoveConstToX86reg(1,x86_EDX);
		Call_Direct(DoCopUnusableException,"DoCopUnusableException");
		if (CPU_Type == CPU_SyncCores) { Call_Direct(SyncToPC, "SyncToPC"); }
		Ret();
		break;
	case ExitResetRecompCode:
		if (NextInstruction == JUMP || NextInstruction == DELAY_SLOT) {
			BreakPoint();
		}
		if (CPU_Type == CPU_SyncCores) { Call_Direct(SyncToPC, "SyncToPC"); }
		Call_Direct(ResetRecompCode, "ResetRecompCode");
		Ret();
		break;
	case TLBReadMiss:
		MoveVariableToX86reg(&TLBLoadAddress, "TLBLoadAddress", x86_EDX);
		MoveX86RegToX86Reg(x86_EDX, x86_ECX);
		ShiftRightSignImmed(x86_ECX, 31);
		Push(x86_ECX);
		Push(x86_EDX);
		MoveConstToX86reg(NextInstruction == JUMP || NextInstruction == DELAY_SLOT, x86_ECX);
		MoveConstToX86reg(1, x86_EDX);
		Call_Direct(DoTLBMiss, "DoTLBMiss");
		if (CPU_Type == CPU_SyncCores) { Call_Direct(SyncToPC, "SyncToPC"); }
		Ret();
		break;
	case TLBWriteMiss:
		MoveVariableToX86reg(&TLBLoadAddress, "TLBLoadAddress", x86_EDX);
		MoveX86RegToX86Reg(x86_EDX, x86_ECX);
		ShiftRightSignImmed(x86_ECX, 31);
		Push(x86_ECX);
		Push(x86_EDX);
		MoveConstToX86reg(NextInstruction == JUMP || NextInstruction == DELAY_SLOT, x86_ECX);
		MoveConstToX86reg(0, x86_EDX);
		Call_Direct(DoTLBMiss, "DoTLBMiss");
		if (CPU_Type == CPU_SyncCores) { Call_Direct(SyncToPC, "SyncToPC"); }
		Ret();
		break;
	default:
		if (ShowDebugMessages)
			DisplayError("how did you want to exit on reason (%d) ???",reason);
	}
}

void CompileSystemCheck (DWORD TimerModifier, DWORD TargetPC, REG_INFO RegSet) {
	BLOCK_SECTION Section;
	BYTE *Jump, *Jump2;

	// Timer
	if (TimerModifier != 0) {
		SubConstFromVariable(TimerModifier,&Timers.Timer,"Timer");
	} else {
		CompConstToVariable(0,&Timers.Timer,"Timer");
	}
	JnsLabel32("Continue_From_Timer_Test",0);
	Jump = RecompPos - 4;
	Pushad();
	if (TargetPC != (DWORD)-1) { MoveConstToVariable(TargetPC,&PROGRAM_COUNTER,"PROGRAM_COUNTER"); }
	InitilzeSection (&Section, NULL, (DWORD)-1, 0);
	memcpy(&Section.RegWorking, &RegSet, sizeof(REG_INFO));
	WriteBackRegisters(&Section);
	if (CPU_Type == CPU_SyncCores) { Call_Direct(SyncToPC, "SyncToPC"); }
	Call_Direct(TimerDone,"TimerDone");
	if (CPU_Type == CPU_SyncCores) { Call_Direct(SyncToPC, "SyncToPC"); }
	Popad();

	//Interrupt
	CompConstToVariable(0,&CPU_Action.DoSomething,"CPU_Action.DoSomething");
	JeLabel32("Continue_From_Interrupt_Test",0);
	Jump2 = RecompPos - 4;
	CompileExit(-1,Section.RegWorking,DoCPU_Action,TRUE,NULL);

	CPU_Message("");
	CPU_Message("      $Continue_From_Interrupt_Test:");
	SetJump32(Jump2,RecompPos);	
	Ret();
	
	CPU_Message("");
	CPU_Message("      $Continue_From_Timer_Test:");
	SetJump32(Jump,RecompPos);

	//Interrupt 2
	CompConstToVariable(0,&CPU_Action.DoSomething,"CPU_Action.DoSomething");
	JeLabel32("Continue_From_Interrupt_Test",0);
	Jump = RecompPos - 4;
	if (TargetPC != (DWORD)-1) { MoveConstToVariable(TargetPC,&PROGRAM_COUNTER,"PROGRAM_COUNTER"); }
	InitilzeSection (&Section, NULL, (DWORD)-1, 0);
	memcpy(&Section.RegWorking, &RegSet, sizeof(REG_INFO));
	WriteBackRegisters(&Section);		
	CompileExit(-1,Section.RegWorking,DoCPU_Action,TRUE,NULL);
	CPU_Message("");
	CPU_Message("      $Continue_From_Interrupt_Test:");
	SetJump32(Jump,RecompPos);	
	return;
}

void _fastcall CreateSectionLinkage (BLOCK_SECTION * Section) {
	BLOCK_SECTION ** TargetSection[2];
	DWORD * TargetPC[2], count;

	InheritConstants(Section);
	__try {
		FillSectionInfo(Section);
	} __except( r4300i_CPU_MemoryFilter( GetExceptionCode(), GetExceptionInformation()) ) {
		DisplayError(GS(MSG_UNKNOWN_MEM_ACTION));
		ExitThread(0);
	}
	
	if (Section->Jump.TargetPC < Section->Cont.TargetPC) {
		TargetSection[0] = (BLOCK_SECTION **)&Section->JumpSection;
		TargetSection[1] = (BLOCK_SECTION **)&Section->ContinueSection;
		TargetPC[0] = &Section->Jump.TargetPC;
		TargetPC[1] = &Section->Cont.TargetPC;	
	} else {
		TargetSection[0] = (BLOCK_SECTION **)&Section->ContinueSection;
		TargetSection[1] = (BLOCK_SECTION **)&Section->JumpSection;
		TargetPC[0] = &Section->Cont.TargetPC;	
		TargetPC[1] = &Section->Jump.TargetPC;
	}

	for (count = 0; count < 2; count ++) {
		if (*TargetPC[count] != (DWORD)-1 && *TargetSection[count] == NULL) {
			*TargetSection[count] = ExistingSection(BlockInfo.BlockInfo.ContinueSection,*TargetPC[count],GetNewTestValue());
			if (*TargetSection[count] == NULL) {
				*TargetSection[count] = ExistingSection(BlockInfo.BlockInfo.JumpSection,*TargetPC[count],GetNewTestValue());
			}
			if (*TargetSection[count] == NULL) {
				BlockInfo.NoOfSections += 1;
				*TargetSection[count] = malloc(sizeof(BLOCK_SECTION));
				InitilzeSection (*TargetSection[count], Section, *TargetPC[count], BlockInfo.NoOfSections);
				CreateSectionLinkage(*TargetSection[count]);
			} else {
				AddParent(*TargetSection[count],Section);
			}
		}
	}
}

void _fastcall DetermineLoop(BLOCK_SECTION * Section, DWORD Test, DWORD Test2, DWORD TestID) {
	if (Section == NULL) { return; }
	if (Section->SectionID != TestID) {
		if (Section->Test2 == Test2) {
			return; 
		}
		Section->Test2 = Test2;
		DetermineLoop(Section->ContinueSection,Test,Test2,TestID);
		DetermineLoop(Section->JumpSection,Test,Test2,TestID);
		return;
	}
	if (Section->Test2 == Test2) { 
		Section->InLoop = TRUE;
		return; 
	}
	Section->Test2 = Test2;
	DetermineLoop(Section->ContinueSection,Test,Test2,TestID);
	DetermineLoop(Section->JumpSection,Test,Test2,TestID);
	if (Section->Test == Test) { return; }
	Section->Test = Test;
	if (Section->ContinueSection != NULL) {
		DetermineLoop(Section->ContinueSection,Test,GetNewTestValue(),((BLOCK_SECTION *)Section->ContinueSection)->SectionID);
	}
	if (Section->JumpSection != NULL) {
		DetermineLoop(Section->JumpSection,Test,GetNewTestValue(),((BLOCK_SECTION *)Section->JumpSection)->SectionID);
	}
}

BOOL DisplaySectionInformation (BLOCK_SECTION * Section, DWORD ID, DWORD Test) {
	int NoOfParents;
			
	if (Section == NULL) { return FALSE; }
	if (Section->Test == Test) { return FALSE; }
	Section->Test = Test;
	if (Section->SectionID != ID) {
		if (DisplaySectionInformation(Section->ContinueSection,ID,Test)) { return TRUE; }
		if (DisplaySectionInformation(Section->JumpSection,ID,Test)) { return TRUE; }
		return FALSE;
	}
	CPU_Message("====== Section %d ======",Section->SectionID);
	CPU_Message("Start PC: %X",Section->StartPC);
	if (Section->ParentSection != NULL) {
		for (NoOfParents = 0;Section->ParentSection[NoOfParents] != NULL;NoOfParents++);
		CPU_Message("Number of parents: %d",NoOfParents);
	}

	if (Section->JumpSection != NULL) {
		CPU_Message("Jump Section: %d",((BLOCK_SECTION *)Section->JumpSection)->SectionID);
	} else {
		CPU_Message("Jump Section: None");
	}
	if (Section->ContinueSection != NULL) {
		CPU_Message("Continue Section: %d",((BLOCK_SECTION *)Section->ContinueSection)->SectionID);
	} else {
		CPU_Message("Continue Section: None");
	}
	CPU_Message("=======================",Section->SectionID);
	return TRUE;
}

BLOCK_SECTION * ExistingSection(BLOCK_SECTION * StartSection, DWORD Addr, DWORD Test) {
	BLOCK_SECTION * Section;

	if (StartSection == NULL) { return NULL; }
	if (StartSection->StartPC == Addr) { return StartSection; }
	if (StartSection->Test == Test) { return NULL; }
	StartSection->Test = Test;
	Section = ExistingSection(StartSection->JumpSection,Addr,Test);
	if (Section != NULL) { return Section; }
	Section = ExistingSection(StartSection->ContinueSection,Addr,Test);
	if (Section != NULL) { return Section; }
	return NULL;
}

void _fastcall FillSectionInfo(BLOCK_SECTION * Section) {
	OPCODE Command;

	if (Section->CompiledLocation != NULL) { return; }
	Section->CompilePC      = Section->StartPC;
	memcpy(&Section->RegWorking,&Section->RegStart,sizeof(REG_INFO));
	NextInstruction = NORMAL;
	do {
		MIPS_DWORD Address;
		Address.DW = (long)Section->CompilePC;
		if (!r4300i_LW_VAddr_NonCPU(Address, &Command.Hex)) {
			DisplayError(GS(MSG_FAIL_LOAD_WORD));
			ExitThread(0);
		} 
		if (SelfModCheck == ModCode_ChangeMemory) {
			if ( (Command.Hex >> 16) == 0x7C7C) {
				Command.Hex = OrigMem[(Command.Hex & 0xFFFF)].OriginalValue;
			}
		}
		
		switch (Command.BRANCH.op) {
		case R4300i_SPECIAL:
			switch (Command.REG.funct) {
			case R4300i_SPECIAL_SYNC:
				break;
			case R4300i_SPECIAL_SLL: 
				if (Command.REG.rd == 0) { break; }
				if (Section->InLoop && Command.BRANCH.rt == Command.REG.rd) {
					MipsRegState(Command.REG.rd) = STATE_UNKNOWN;	
				}
				if (IsConst(Command.BRANCH.rt)) {
					MipsRegState(Command.REG.rd) = STATE_CONST_32;
					MipsRegLo(Command.REG.rd) = MipsRegLo(Command.BRANCH.rt) << Command.REG.sa;
				} else {
					MipsRegState(Command.REG.rd) = STATE_UNKNOWN;
				}
				break;
			case R4300i_SPECIAL_SRL: 
				if (Command.REG.rd == 0) { break; }
				if (Section->InLoop && Command.BRANCH.rt == Command.REG.rd) {
					MipsRegState(Command.REG.rd) = STATE_UNKNOWN;	
				}
				if (IsConst(Command.BRANCH.rt)) {
					MipsRegState(Command.REG.rd) = STATE_CONST_32;
					MipsRegLo(Command.REG.rd) = MipsRegLo(Command.BRANCH.rt) >> Command.REG.sa;
				} else {
					MipsRegState(Command.REG.rd) = STATE_UNKNOWN;
				}
				break;
			case R4300i_SPECIAL_SRA: 
				if (Command.REG.rd == 0) { break; }
				if (Section->InLoop && Command.BRANCH.rt == Command.REG.rd) {
					MipsRegState(Command.REG.rd) = STATE_UNKNOWN;	
				}
				if (IsConst(Command.BRANCH.rt)) {
					MipsRegState(Command.REG.rd) = STATE_CONST_32;
					MipsRegLo(Command.REG.rd) = MipsRegLo_S(Command.BRANCH.rt) >> Command.REG.sa;
				} else {
					MipsRegState(Command.REG.rd) = STATE_UNKNOWN;
				}
				break;
			case R4300i_SPECIAL_SLLV: 
				if (Command.REG.rd == 0) { break; }
				if (Section->InLoop && (Command.BRANCH.rt == Command.REG.rd || Command.BRANCH.rs == Command.REG.rd)) {
					MipsRegState(Command.REG.rd) = STATE_UNKNOWN;	
				}
				if (IsConst(Command.BRANCH.rt) && IsConst(Command.BRANCH.rs)) {
					MipsRegState(Command.REG.rd) = STATE_CONST_32;
					MipsRegLo(Command.REG.rd) = MipsRegLo(Command.BRANCH.rt) << (MipsRegLo(Command.BRANCH.rs) & 0x1F);
				} else {
					MipsRegState(Command.REG.rd) = STATE_UNKNOWN;
				}
				break;
			case R4300i_SPECIAL_SRLV: 
				if (Command.REG.rd == 0) { break; }
				if (Section->InLoop && (Command.BRANCH.rt == Command.REG.rd || Command.BRANCH.rs == Command.REG.rd)) {
					MipsRegState(Command.REG.rd) = STATE_UNKNOWN;	
				}
				if (IsConst(Command.BRANCH.rt) && IsConst(Command.BRANCH.rs)) {
					MipsRegState(Command.REG.rd) = STATE_CONST_32;
					MipsRegLo(Command.REG.rd) = MipsRegLo(Command.BRANCH.rt) >> (MipsRegLo(Command.BRANCH.rs) & 0x1F);
				} else {
					MipsRegState(Command.REG.rd) = STATE_UNKNOWN;
				}
				break;
			case R4300i_SPECIAL_SRAV: 
				if (Command.REG.rd == 0) { break; }
				if (Section->InLoop && (Command.BRANCH.rt == Command.REG.rd || Command.BRANCH.rs == Command.REG.rd)) {
					MipsRegState(Command.REG.rd) = STATE_UNKNOWN;	
				}
				if (IsConst(Command.BRANCH.rt) && IsConst(Command.BRANCH.rs)) {
					MipsRegState(Command.REG.rd) = STATE_CONST_32;
					MipsRegLo(Command.REG.rd) = MipsRegLo_S(Command.BRANCH.rt) >> (MipsRegLo(Command.BRANCH.rs) & 0x1F);
				} else {
					MipsRegState(Command.REG.rd) = STATE_UNKNOWN;
				}
				break;
			case R4300i_SPECIAL_JR:				
				if (IsConst(Command.BRANCH.rs)) {
					Section->Jump.TargetPC = MipsRegLo(Command.BRANCH.rs);
				} else {
					Section->Jump.TargetPC = (DWORD)-1;
				}
				NextInstruction = DELAY_SLOT;
				break;
			case R4300i_SPECIAL_JALR: 
				MipsRegLo(Opcode.REG.rd) = Section->CompilePC + 8;
				MipsRegState(Opcode.REG.rd) = STATE_CONST_32;
				if (IsConst(Command.BRANCH.rs)) {
					Section->Jump.TargetPC = MipsRegLo(Command.BRANCH.rs);
				} else {
					Section->Jump.TargetPC = (DWORD)-1;
				}
				NextInstruction = DELAY_SLOT;
				break;
			case R4300i_SPECIAL_SYSCALL:
			case R4300i_SPECIAL_BREAK:
				NextInstruction = END_BLOCK;
				Section->CompilePC -= 4;
				break;
			case R4300i_SPECIAL_MFHI: MipsRegState(Command.REG.rd) = STATE_UNKNOWN; break;
			case R4300i_SPECIAL_MTHI: break;
			case R4300i_SPECIAL_MFLO: MipsRegState(Command.REG.rd) = STATE_UNKNOWN; break;
			case R4300i_SPECIAL_MTLO: break;
			case R4300i_SPECIAL_DSLLV: 
				if (Command.REG.rd == 0) { break; }
				if (Section->InLoop && (Command.BRANCH.rt == Command.REG.rd || Command.BRANCH.rs == Command.REG.rd)) {
					MipsRegState(Command.REG.rd) = STATE_UNKNOWN;	
				}
				if (IsConst(Command.BRANCH.rt) && IsConst(Command.BRANCH.rs)) {
					MipsRegState(Command.REG.rd) = STATE_CONST_64;
					MipsReg(Command.REG.rd) = Is64Bit(Command.BRANCH.rt)?MipsReg(Command.BRANCH.rt):(QWORD)MipsRegLo_S(Command.BRANCH.rt) << (MipsRegLo(Command.BRANCH.rs) & 0x3F);
				} else {
					MipsRegState(Command.REG.rd) = STATE_UNKNOWN;
				}
				break;
			case R4300i_SPECIAL_DSRLV: 
				if (Command.REG.rd == 0) { break; }
				if (Section->InLoop && (Command.BRANCH.rt == Command.REG.rd || Command.BRANCH.rs == Command.REG.rd)) {
					MipsRegState(Command.REG.rd) = STATE_UNKNOWN;	
				}
				if (IsConst(Command.BRANCH.rt) && IsConst(Command.BRANCH.rs)) {
					MipsRegState(Command.REG.rd) = STATE_CONST_64;
					MipsReg(Command.REG.rd) = Is64Bit(Command.BRANCH.rt)?MipsReg(Command.BRANCH.rt):(QWORD)MipsRegLo_S(Command.BRANCH.rt) >> (MipsRegLo(Command.BRANCH.rs) & 0x3F);
				} else {
					MipsRegState(Command.REG.rd) = STATE_UNKNOWN;
				}
				break;
			case R4300i_SPECIAL_DSRAV: 
				if (Command.REG.rd == 0) { break; }
				if (Section->InLoop && (Command.BRANCH.rt == Command.REG.rd || Command.BRANCH.rs == Command.REG.rd)) {
					MipsRegState(Command.REG.rd) = STATE_UNKNOWN;	
				}
				if (IsConst(Command.BRANCH.rt) && IsConst(Command.BRANCH.rs)) {
					MipsRegState(Command.REG.rd) = STATE_CONST_64;
					MipsReg(Command.REG.rd) = Is64Bit(Command.BRANCH.rt)?MipsReg_S(Command.BRANCH.rt):(_int64)MipsRegLo_S(Command.BRANCH.rt) >> (MipsRegLo(Command.BRANCH.rs) & 0x3F);
				} else {
					MipsRegState(Command.REG.rd) = STATE_UNKNOWN;
				}
				break;
			case R4300i_SPECIAL_MULT: break;
			case R4300i_SPECIAL_MULTU: break;
			case R4300i_SPECIAL_DIV: break;
			case R4300i_SPECIAL_DIVU: break;
			case R4300i_SPECIAL_DMULT: break;
			case R4300i_SPECIAL_DMULTU: break;
			case R4300i_SPECIAL_DDIV: break;
			case R4300i_SPECIAL_DDIVU: break;
			case R4300i_SPECIAL_ADD:
			case R4300i_SPECIAL_ADDU:
				if (Command.REG.rd == 0) { break; }
				if (Section->InLoop && (Command.BRANCH.rt == Command.REG.rd || Command.BRANCH.rs == Command.REG.rd)) {
					MipsRegState(Command.REG.rd) = STATE_UNKNOWN;	
				}
				if (IsConst(Command.BRANCH.rt) && IsConst(Command.BRANCH.rs)) {
					MipsRegLo(Command.REG.rd) = MipsRegLo(Command.BRANCH.rs) + MipsRegLo(Command.BRANCH.rt);
					MipsRegState(Command.REG.rd) = STATE_CONST_32;
				} else {
					MipsRegState(Command.REG.rd) = STATE_UNKNOWN;
				}
				break;
			case R4300i_SPECIAL_SUB: 
			case R4300i_SPECIAL_SUBU: 
				if (Command.REG.rd == 0) { break; }
				if (Section->InLoop && (Command.BRANCH.rt == Command.REG.rd || Command.BRANCH.rs == Command.REG.rd)) {
					MipsRegState(Command.REG.rd) = STATE_UNKNOWN;	
				}
				if (IsConst(Command.BRANCH.rt) && IsConst(Command.BRANCH.rs)) {
					MipsRegLo(Command.REG.rd) = MipsRegLo(Command.BRANCH.rs) - MipsRegLo(Command.BRANCH.rt);
					MipsRegState(Command.REG.rd) = STATE_CONST_32;
				} else {
					MipsRegState(Command.REG.rd) = STATE_UNKNOWN;
				}
				break;
			case R4300i_SPECIAL_AND: 
				if (Command.REG.rd == 0) { break; }
				if (Section->InLoop && (Command.BRANCH.rt == Command.REG.rd || Command.BRANCH.rs == Command.REG.rd)) {
					MipsRegState(Command.REG.rd) = STATE_UNKNOWN;	
				}
				if (IsConst(Command.BRANCH.rt) && IsConst(Command.BRANCH.rs)) {
					if (Is64Bit(Command.BRANCH.rt) && Is64Bit(Command.BRANCH.rs)) {
						MipsReg(Command.REG.rd) = MipsReg(Command.BRANCH.rt) & MipsReg(Command.BRANCH.rs);
						MipsRegState(Command.REG.rd) = STATE_CONST_64;					
					} else if (Is64Bit(Command.BRANCH.rt) || Is64Bit(Command.BRANCH.rs)) {
						if (Is64Bit(Command.BRANCH.rt)) {
							MipsReg(Command.REG.rd) = MipsReg(Command.BRANCH.rt) & MipsRegLo(Command.BRANCH.rs);
						} else {
							MipsReg(Command.REG.rd) = MipsRegLo(Command.BRANCH.rt) & MipsReg(Command.BRANCH.rs);
						}						
						MipsRegState(Command.REG.rd) = ConstantsType(MipsReg(Command.REG.rd));
					} else {
						MipsRegLo(Command.REG.rd) = MipsRegLo(Command.BRANCH.rt) & MipsRegLo(Command.BRANCH.rs);
						MipsRegState(Command.REG.rd) = STATE_CONST_32;
					}
				} else {
					MipsRegState(Command.REG.rd) = STATE_UNKNOWN;
				}
				break;
			case R4300i_SPECIAL_OR: 
				if (Command.REG.rd == 0) { break; }
				if (Section->InLoop && (Command.BRANCH.rt == Command.REG.rd || Command.BRANCH.rs == Command.REG.rd)) {
					MipsRegState(Command.REG.rd) = STATE_UNKNOWN;	
				}
				if (IsConst(Command.BRANCH.rt) && IsConst(Command.BRANCH.rs)) {
					if (Is64Bit(Command.BRANCH.rt) && Is64Bit(Command.BRANCH.rs)) {
						MipsReg(Command.REG.rd) = MipsReg(Command.BRANCH.rt) | MipsReg(Command.BRANCH.rs);
						MipsRegState(Command.REG.rd) = STATE_CONST_64;
					} else if (Is64Bit(Command.BRANCH.rt) || Is64Bit(Command.BRANCH.rs)) {
						if (Is64Bit(Command.BRANCH.rt)) {
							MipsReg(Command.REG.rd) = MipsReg(Command.BRANCH.rt) | MipsRegLo(Command.BRANCH.rs);
						} else {
							MipsReg(Command.REG.rd) = MipsRegLo(Command.BRANCH.rt) | MipsReg(Command.BRANCH.rs);
						}
						MipsRegState(Command.REG.rd) = STATE_CONST_64;
					} else {
						MipsRegLo(Command.REG.rd) = MipsRegLo(Command.BRANCH.rt) | MipsRegLo(Command.BRANCH.rs);
						MipsRegState(Command.REG.rd) = STATE_CONST_32;
					}
				} else {
					MipsRegState(Command.REG.rd) = STATE_UNKNOWN;
				}
				break;
			case R4300i_SPECIAL_XOR: 
				if (Command.REG.rd == 0) { break; }
				if (Section->InLoop && (Command.BRANCH.rt == Command.REG.rd || Command.BRANCH.rs == Command.REG.rd)) {
					MipsRegState(Command.REG.rd) = STATE_UNKNOWN;	
				}
				if (IsConst(Command.BRANCH.rt) && IsConst(Command.BRANCH.rs)) {
					if (Is64Bit(Command.BRANCH.rt) && Is64Bit(Command.BRANCH.rs)) {
						MipsReg(Command.REG.rd) = MipsReg(Command.BRANCH.rt) ^ MipsReg(Command.BRANCH.rs);
						MipsRegState(Command.REG.rd) = STATE_CONST_64;
					} else if (Is64Bit(Command.BRANCH.rt) || Is64Bit(Command.BRANCH.rs)) {
						if (Is64Bit(Command.BRANCH.rt)) {
							MipsReg(Command.REG.rd) = MipsReg(Command.BRANCH.rt) ^ MipsRegLo(Command.BRANCH.rs);
						} else {
							MipsReg(Command.REG.rd) = MipsRegLo(Command.BRANCH.rt) ^ MipsReg(Command.BRANCH.rs);
						}
						MipsRegState(Command.REG.rd) = STATE_CONST_64;
					} else {
						MipsRegLo(Command.REG.rd) = MipsRegLo(Command.BRANCH.rt) ^ MipsRegLo(Command.BRANCH.rs);
						MipsRegState(Command.REG.rd) = STATE_CONST_32;
					}
				} else {
					MipsRegState(Command.REG.rd) = STATE_UNKNOWN;
				}
				break;
			case R4300i_SPECIAL_NOR: 
				if (Command.REG.rd == 0) { break; }
				if (Section->InLoop && (Command.BRANCH.rt == Command.REG.rd || Command.BRANCH.rs == Command.REG.rd)) {
					MipsRegState(Command.REG.rd) = STATE_UNKNOWN;	
				}
				if (IsConst(Command.BRANCH.rt) && IsConst(Command.BRANCH.rs)) {
					if (Is64Bit(Command.BRANCH.rt) && Is64Bit(Command.BRANCH.rs)) {
						MipsReg(Command.REG.rd) = ~(MipsReg(Command.BRANCH.rt) | MipsReg(Command.BRANCH.rs));
						MipsRegState(Command.REG.rd) = STATE_CONST_64;
					} else if (Is64Bit(Command.BRANCH.rt) || Is64Bit(Command.BRANCH.rs)) {
						if (Is64Bit(Command.BRANCH.rt)) {
							MipsReg(Command.REG.rd) = ~(MipsReg(Command.BRANCH.rt) | MipsRegLo(Command.BRANCH.rs));
						} else {
							MipsReg(Command.REG.rd) = ~(MipsRegLo(Command.BRANCH.rt) | MipsReg(Command.BRANCH.rs));
						}
						MipsRegState(Command.REG.rd) = STATE_CONST_64;
					} else {
						MipsRegLo(Command.REG.rd) = ~(MipsRegLo(Command.BRANCH.rt) | MipsRegLo(Command.BRANCH.rs));
						MipsRegState(Command.REG.rd) = STATE_CONST_32;
					}
				} else {
					MipsRegState(Command.REG.rd) = STATE_UNKNOWN;
				}
				break;
			case R4300i_SPECIAL_SLT: 
				if (Command.REG.rd == 0) { break; }
				if (IsConst(Command.BRANCH.rt) && IsConst(Command.BRANCH.rs)) {
					if (Is64Bit(Command.BRANCH.rt) || Is64Bit(Command.BRANCH.rs)) {
						if (Is64Bit(Command.BRANCH.rt)) {
							MipsRegLo(Command.REG.rd) = (MipsRegLo_S(Command.BRANCH.rs) < MipsReg_S(Command.BRANCH.rt))?1:0;
						} else {
							MipsRegLo(Command.REG.rd) = (MipsReg_S(Command.BRANCH.rs) < MipsRegLo_S(Command.BRANCH.rt))?1:0;
						}
					} else {
						MipsRegLo(Command.REG.rd) = (MipsRegLo_S(Command.BRANCH.rs) < MipsRegLo_S(Command.BRANCH.rt))?1:0;
					}
					MipsRegState(Command.REG.rd) = STATE_CONST_32;
				} else {
					MipsRegState(Command.REG.rd) = STATE_UNKNOWN;
				}
				break;
			case R4300i_SPECIAL_SLTU: 
				if (Command.REG.rd == 0) { break; }
				if (IsConst(Command.BRANCH.rt) && IsConst(Command.BRANCH.rs)) {
					if (Is64Bit(Command.BRANCH.rt) || Is64Bit(Command.BRANCH.rs)) {
						if (Is64Bit(Command.BRANCH.rt)) {
							MipsRegLo(Command.REG.rd) = (MipsRegLo(Command.BRANCH.rs) < MipsReg(Command.BRANCH.rt))?1:0;
						} else {
							MipsRegLo(Command.REG.rd) = (MipsReg(Command.BRANCH.rs) < MipsRegLo(Command.BRANCH.rt))?1:0;
						}
					} else {
						MipsRegLo(Command.REG.rd) = (MipsRegLo(Command.BRANCH.rs) < MipsRegLo(Command.BRANCH.rt))?1:0;
					}
					MipsRegState(Command.REG.rd) = STATE_CONST_32;
				} else {
					MipsRegState(Command.REG.rd) = STATE_UNKNOWN;
				}
				break;
			case R4300i_SPECIAL_DADD: 
			case R4300i_SPECIAL_DADDU: 
				if (Command.REG.rd == 0) { break; }
				if (Section->InLoop && (Command.BRANCH.rt == Command.REG.rd || Command.BRANCH.rs == Command.REG.rd)) {
					MipsRegState(Command.REG.rd) = STATE_UNKNOWN;	
				}
				if (IsConst(Command.BRANCH.rt) && IsConst(Command.BRANCH.rs)) {
					MipsReg(Command.REG.rd) = 
						Is64Bit(Command.BRANCH.rs)?MipsReg(Command.BRANCH.rs):(_int64)MipsRegLo_S(Command.BRANCH.rs) +
						Is64Bit(Command.BRANCH.rt)?MipsReg(Command.BRANCH.rt):(_int64)MipsRegLo_S(Command.BRANCH.rt);
					MipsRegState(Command.REG.rd) = STATE_CONST_64;
				} else {
					MipsRegState(Command.REG.rd) = STATE_UNKNOWN;
				}
				break;
			case R4300i_SPECIAL_DSUB: 
			case R4300i_SPECIAL_DSUBU: 
				if (Command.REG.rd == 0) { break; }
				if (Section->InLoop && (Command.BRANCH.rt == Command.REG.rd || Command.BRANCH.rs == Command.REG.rd)) {
					MipsRegState(Command.REG.rd) = STATE_UNKNOWN;	
				}
				if (IsConst(Command.BRANCH.rt) && IsConst(Command.BRANCH.rs)) {
					MipsReg(Command.REG.rd) = 
						Is64Bit(Command.BRANCH.rs)?MipsReg(Command.BRANCH.rs):(_int64)MipsRegLo_S(Command.BRANCH.rs) -
						Is64Bit(Command.BRANCH.rt)?MipsReg(Command.BRANCH.rt):(_int64)MipsRegLo_S(Command.BRANCH.rt);
					MipsRegState(Command.REG.rd) = STATE_CONST_64;
				} else {
					MipsRegState(Command.REG.rd) = STATE_UNKNOWN;
				}
				break;
			case R4300i_SPECIAL_DSLL:
				if (Command.REG.rd == 0) { break; }
				if (Section->InLoop && Command.BRANCH.rt == Command.REG.rd) {
					MipsRegState(Command.REG.rd) = STATE_UNKNOWN;	
				}
				if (IsConst(Command.BRANCH.rt)) {
					MipsRegState(Command.REG.rd) = STATE_CONST_64;
					MipsReg(Command.REG.rd) = Is64Bit(Command.BRANCH.rt)?MipsReg(Command.BRANCH.rt):(_int64)MipsRegLo_S(Command.BRANCH.rt) << Command.REG.sa;
				} else {
					MipsRegState(Command.REG.rd) = STATE_UNKNOWN;
				}
				break;
			case R4300i_SPECIAL_DSRL:
				if (Command.REG.rd == 0) { break; }
				if (Section->InLoop && Command.BRANCH.rt == Command.REG.rd) {
					MipsRegState(Command.REG.rd) = STATE_UNKNOWN;	
				}
				if (IsConst(Command.BRANCH.rt)) {
					MipsRegState(Command.REG.rd) = STATE_CONST_64;
					MipsReg(Command.REG.rd) = Is64Bit(Command.BRANCH.rt)?MipsReg(Command.BRANCH.rt):(QWORD)MipsRegLo_S(Command.BRANCH.rt) >> Command.REG.sa;
				} else {
					MipsRegState(Command.REG.rd) = STATE_UNKNOWN;
				}
				break;
			case R4300i_SPECIAL_DSRA:
				if (Command.REG.rd == 0) { break; }
				if (Section->InLoop && Command.BRANCH.rt == Command.REG.rd) {
					MipsRegState(Command.REG.rd) = STATE_UNKNOWN;	
				}
				if (IsConst(Command.BRANCH.rt)) {
					MipsRegState(Command.REG.rd) = STATE_CONST_64;
					MipsReg_S(Command.REG.rd) = Is64Bit(Command.BRANCH.rt)?MipsReg_S(Command.BRANCH.rt):(_int64)MipsRegLo_S(Command.BRANCH.rt) >> Command.REG.sa;
				} else {
					MipsRegState(Command.REG.rd) = STATE_UNKNOWN;
				}
				break;
			case R4300i_SPECIAL_DSLL32:
				if (Command.REG.rd == 0) { break; }
				if (Section->InLoop && Command.BRANCH.rt == Command.REG.rd) {
					MipsRegState(Command.REG.rd) = STATE_UNKNOWN;	
				}
				if (IsConst(Command.BRANCH.rt)) {
					MipsRegState(Command.REG.rd) = STATE_CONST_64;
					MipsReg(Command.REG.rd) = MipsRegLo(Command.BRANCH.rt) << (Command.REG.sa + 32);
				} else {
					MipsRegState(Command.REG.rd) = STATE_UNKNOWN;
				}
				break;
			case R4300i_SPECIAL_DSRL32:
				if (Command.REG.rd == 0) { break; }
				if (Section->InLoop && Command.BRANCH.rt == Command.REG.rd) {
					MipsRegState(Command.REG.rd) = STATE_UNKNOWN;	
				}
				if (IsConst(Command.BRANCH.rt)) {
					MipsRegState(Command.REG.rd) = STATE_CONST_32;
					MipsRegLo(Command.REG.rd) = (DWORD)(MipsReg(Command.BRANCH.rt) >> (Command.REG.sa + 32));
				} else {
					MipsRegState(Command.REG.rd) = STATE_UNKNOWN;
				}
				break;
			case R4300i_SPECIAL_DSRA32:
				if (Command.REG.rd == 0) { break; }
				if (Section->InLoop && Command.BRANCH.rt == Command.REG.rd) {
					MipsRegState(Command.REG.rd) = STATE_UNKNOWN;	
				}
				if (IsConst(Command.BRANCH.rt)) {
					MipsRegState(Command.REG.rd) = STATE_CONST_32;
					MipsRegLo(Command.REG.rd) = (DWORD)(MipsReg_S(Command.BRANCH.rt) >> (Command.REG.sa + 32));
				} else {
					MipsRegState(Command.REG.rd) = STATE_UNKNOWN;
				}
				break;
			default:
				if (ShowDebugMessages) {
					MIPS_DWORD CompilePC;
					CompilePC.DW = (int)Section->CompilePC;

					if (Command.Hex == 0x00000001) { break; }
					DisplayError("Unhandled R4300i OpCode in FillSectionInfo 5\n%s",
						R4300iOpcodeName(Command.Hex, CompilePC));
				}
				NextInstruction = END_BLOCK;
				Section->CompilePC -= 4;
			}
			break;
		case R4300i_REGIMM:
			switch (Command.BRANCH.rt) {
			case R4300i_REGIMM_BLTZ:
			case R4300i_REGIMM_BGEZ:
				NextInstruction = DELAY_SLOT;
				Section->Cont.TargetPC = Section->CompilePC + 8;
				Section->Jump.TargetPC = Section->CompilePC + ((short)Command.BRANCH.offset << 2) + 4;
				if (Section->CompilePC == Section->Jump.TargetPC) {
					MIPS_DWORD CompilePC;
					CompilePC.DW = (int)Section->CompilePC;
					if (!DelaySlotEffectsCompare(CompilePC,Command.BRANCH.rs,0)) {
						Section->Jump.PermLoop = TRUE;
					}
				} 
				break;
			case R4300i_REGIMM_BLTZL:
			case R4300i_REGIMM_BGEZL:
				NextInstruction = LIKELY_DELAY_SLOT;
				Section->Cont.TargetPC = Section->CompilePC + 8;
				Section->Jump.TargetPC = Section->CompilePC + ((short)Command.BRANCH.offset << 2) + 4;
				if (Section->CompilePC == Section->Jump.TargetPC) {
					MIPS_DWORD CompilePC;
					CompilePC.DW = (int)Section->CompilePC;
					if (!DelaySlotEffectsCompare(CompilePC,Command.BRANCH.rs,0)) {
						Section->Jump.PermLoop = TRUE;
					}
				} 
				break;
			case R4300i_REGIMM_BLTZAL:
			case R4300i_REGIMM_BGEZAL:
				NextInstruction = DELAY_SLOT;
				MipsRegLo(31) = Section->CompilePC + 8;
				MipsRegState(31) = STATE_CONST_32;
				Section->Cont.TargetPC = Section->CompilePC + 8;
				Section->Jump.TargetPC = Section->CompilePC + ((short)Command.BRANCH.offset << 2) + 4;
				if (Section->CompilePC == Section->Jump.TargetPC) {
					MIPS_DWORD CompilePC;
					CompilePC.DW = (int)Section->CompilePC;
					if (!DelaySlotEffectsCompare(CompilePC,Command.BRANCH.rs,0)) {
						Section->Jump.PermLoop = TRUE;
					}
				} 
				break;
			default:
				if (ShowDebugMessages) {
					if (Command.Hex == 0x0407000D) { break; }
					MIPS_DWORD CompilePC;
					CompilePC.DW = (int)Section->CompilePC;

					DisplayError("Unhandled R4300i OpCode in FillSectionInfo 4\n%s",
						R4300iOpcodeName(Command.Hex, CompilePC));
				}
				NextInstruction = END_BLOCK;
				Section->CompilePC -= 4;
			}
			break;
		case R4300i_JAL: 
			NextInstruction = DELAY_SLOT;
			MipsRegLo(31) = Section->CompilePC + 8;
			MipsRegState(31) = STATE_CONST_32;
			Section->Jump.TargetPC = (Section->CompilePC & 0xF0000000) + (Command.JMP.target << 2);
			if (Section->CompilePC == Section->Jump.TargetPC) {
				MIPS_DWORD CompilePC;
				CompilePC.DW = (int)Section->CompilePC;
				if (!DelaySlotEffectsCompare(CompilePC,31,0)) {
					Section->Jump.PermLoop = TRUE;
				}
			} 
			break;
		case R4300i_J: 
			NextInstruction = DELAY_SLOT;
			Section->Jump.TargetPC = (Section->CompilePC & 0xF0000000) + (Command.JMP.target << 2);
			if (Section->CompilePC == Section->Jump.TargetPC) { Section->Jump.PermLoop = TRUE; } 
			break;
		case R4300i_BEQ: 
		case R4300i_BNE: 
		case R4300i_BLEZ: 
		case R4300i_BGTZ: 
			NextInstruction = DELAY_SLOT;
			Section->Cont.TargetPC = Section->CompilePC + 8;
			Section->Jump.TargetPC = Section->CompilePC + ((short)Command.BRANCH.offset << 2) + 4;
			if (Section->CompilePC == Section->Jump.TargetPC) {
				MIPS_DWORD CompilePC;
				CompilePC.DW = (int)Section->CompilePC;
				if (!DelaySlotEffectsCompare(CompilePC,Command.BRANCH.rs,Command.BRANCH.rt)) {
					Section->Jump.PermLoop = TRUE;
				}
			} 
			break;
		case R4300i_ADDI: 
		case R4300i_ADDIU: 
			if (Command.BRANCH.rt == 0) { break; }
			if (Section->InLoop && Command.BRANCH.rs == Command.BRANCH.rt) {
				MipsRegState(Command.BRANCH.rt) = STATE_UNKNOWN;	
			}
			if (IsConst(Command.BRANCH.rs)) { 
				MipsRegLo(Command.BRANCH.rt) = MipsRegLo(Command.BRANCH.rs) + (short)Command.IMM.immediate;
				MipsRegState(Command.BRANCH.rt) = STATE_CONST_32;
			} else {
				MipsRegState(Command.BRANCH.rt) = STATE_UNKNOWN;
			}
			break;
		case R4300i_SLTI: 
			if (Command.BRANCH.rt == 0) { break; }
			if (IsConst(Command.BRANCH.rs)) { 
				if (Is64Bit(Command.BRANCH.rs)) {
					MipsRegLo(Command.BRANCH.rt) = (MipsReg_S(Command.BRANCH.rs) < (_int64)((short)Command.IMM.immediate))?1:0;
				} else {
					MipsRegLo(Command.BRANCH.rt) = (MipsRegLo_S(Command.BRANCH.rs) < (int)((short)Command.IMM.immediate))?1:0;
				}
				MipsRegState(Command.BRANCH.rt) = STATE_CONST_32;
			} else {
				MipsRegState(Command.BRANCH.rt) = STATE_UNKNOWN;
			}
			break;
		case R4300i_SLTIU: 
			if (Command.BRANCH.rt == 0) { break; }
			if (IsConst(Command.BRANCH.rs)) { 
				if (Is64Bit(Command.BRANCH.rs)) {
					MipsRegLo(Command.BRANCH.rt) = (MipsReg(Command.BRANCH.rs) < (unsigned _int64)((short)Command.IMM.immediate))?1:0;
				} else {
					MipsRegLo(Command.BRANCH.rt) = (MipsRegLo(Command.BRANCH.rs) < (DWORD)((short)Command.IMM.immediate))?1:0;
				}
				MipsRegState(Command.BRANCH.rt) = STATE_CONST_32;
			} else {
				MipsRegState(Command.BRANCH.rt) = STATE_UNKNOWN;
			}
			break;
		case R4300i_LUI: 
			if (Command.BRANCH.rt == 0) { break; }
			MipsRegLo(Command.BRANCH.rt) = ((short)Command.BRANCH.offset << 16);
			MipsRegState(Command.BRANCH.rt) = STATE_CONST_32;
			break;
		case R4300i_ANDI: 
			if (Command.BRANCH.rt == 0) { break; }
			if (Section->InLoop && Command.BRANCH.rs == Command.BRANCH.rt) {
				MipsRegState(Command.BRANCH.rt) = STATE_UNKNOWN;	
			}
			if (IsConst(Command.BRANCH.rs)) {
				MipsRegState(Command.BRANCH.rt) = STATE_CONST_32;
				MipsRegLo(Command.BRANCH.rt) = MipsRegLo(Command.BRANCH.rs) & Command.IMM.immediate;
			} else {
				MipsRegState(Command.BRANCH.rt) = STATE_UNKNOWN;
			}
			break;
		case R4300i_ORI: 
			if (Command.BRANCH.rt == 0) { break; }
			if (Section->InLoop && Command.BRANCH.rs == Command.BRANCH.rt) {
				MipsRegState(Command.BRANCH.rt) = STATE_UNKNOWN;	
			}
			if (IsConst(Command.BRANCH.rs)) {
				MipsRegState(Command.BRANCH.rt) = STATE_CONST_32;
				MipsRegLo(Command.BRANCH.rt) = MipsRegLo(Command.BRANCH.rs) | Command.IMM.immediate;
			} else {
				MipsRegState(Command.BRANCH.rt) = STATE_UNKNOWN;
			}
			break;
		case R4300i_XORI: 
			if (Command.BRANCH.rt == 0) { break; }
			if (Section->InLoop && Command.BRANCH.rs == Command.BRANCH.rt) {
				MipsRegState(Command.BRANCH.rt) = STATE_UNKNOWN;	
			}
			if (IsConst(Command.BRANCH.rs)) {
				MipsRegState(Command.BRANCH.rt) = STATE_CONST_32;
				MipsRegLo(Command.BRANCH.rt) = MipsRegLo(Command.BRANCH.rs) ^ Command.IMM.immediate;
			} else {
				MipsRegState(Command.BRANCH.rt) = STATE_UNKNOWN;
			}
			break;
		case R4300i_CP0:
			switch (Command.BRANCH.rs) {
			case R4300i_COP0_MF:
				if (Command.BRANCH.rt == 0) { break; }
				MipsRegState(Command.BRANCH.rt) = STATE_UNKNOWN;
				break;
			case R4300i_COP0_MT: break;
			default:
				if ( (Command.BRANCH.rs & 0x10 ) != 0 ) {
					switch( Command.REG.funct ) {
					case R4300i_COP0_CO_TLBR: break;
					case R4300i_COP0_CO_TLBWI: break;
					case R4300i_COP0_CO_TLBWR: break;
					case R4300i_COP0_CO_TLBP: break;
					case R4300i_COP0_CO_ERET: NextInstruction = END_BLOCK; break;
					default:
						if (ShowDebugMessages) {
							MIPS_DWORD CompilePC;
							CompilePC.DW = (int)Section->CompilePC;

							DisplayError("Unhandled R4300i OpCode in FillSectionInfo\n%s",
								R4300iOpcodeName(Command.Hex, CompilePC));
						}

						NextInstruction = END_BLOCK;
						Section->CompilePC -= 4;
					}
				} else {
					if (ShowDebugMessages) {
						MIPS_DWORD CompilePC;
						CompilePC.DW = (int)Section->CompilePC;

						DisplayError("Unhandled R4300i OpCode in FillSectionInfo 3\n%s",
							R4300iOpcodeName(Command.Hex, CompilePC));
					}

					NextInstruction = END_BLOCK;
					Section->CompilePC -= 4;
				}
			}
			break;
		case R4300i_CP1:
			switch (Command.FP.fmt) {
			case R4300i_COP1_CF:
			case R4300i_COP1_MF:
			case R4300i_COP1_DMF:
				if (Command.BRANCH.rt == 0) { break; }
				MipsRegState(Command.BRANCH.rt) = STATE_UNKNOWN;
				break;
			case R4300i_COP1_BC:
				switch (Command.FP.ft) {
				case R4300i_COP1_BC_BCF:
				case R4300i_COP1_BC_BCT:
				case R4300i_COP1_BC_BCFL:
				case R4300i_COP1_BC_BCTL:
					NextInstruction = DELAY_SLOT;
					Section->Cont.TargetPC = Section->CompilePC + 8;
					Section->Jump.TargetPC = Section->CompilePC + ((short)Command.BRANCH.offset << 2) + 4;
					if (Section->CompilePC == Section->Jump.TargetPC) {
						int EffectDelaySlot;
						OPCODE NewCommand;

						MIPS_DWORD Address;
						Address.DW = (long)(Section->CompilePC + 4);
						if (!r4300i_LW_VAddr_NonCPU(Address, &NewCommand.Hex)) {
							DisplayError(GS(MSG_FAIL_LOAD_WORD));
							ExitThread(0);
						}
						
						EffectDelaySlot = FALSE;
						if (NewCommand.BRANCH.op == R4300i_CP1) {
							if (NewCommand.FP.fmt == R4300i_COP1_S && (NewCommand.REG.funct & 0x30) == 0x30 ) {
								EffectDelaySlot = TRUE;
							} 
							if (NewCommand.FP.fmt == R4300i_COP1_D && (NewCommand.REG.funct & 0x30) == 0x30 ) {
								EffectDelaySlot = TRUE;
							} 
						}						
						if (!EffectDelaySlot) {
							Section->Jump.PermLoop = TRUE;
						}
					} 
					break;
				}
				break;
			case R4300i_COP1_MT: break;
			case R4300i_COP1_DMT: break;
			case R4300i_COP1_CT: break;
			case R4300i_COP1_S: break;
			case R4300i_COP1_D: break;
			case R4300i_COP1_W: break;
			case R4300i_COP1_L: break;
			default:
				if (ShowDebugMessages) {
					MIPS_DWORD CompilePC;
					CompilePC.DW = (int)Section->CompilePC;

					DisplayError("Unhandled R4300i OpCode in FillSectionInfo 2\n%s",
						R4300iOpcodeName(Command.Hex, CompilePC));
				}

				NextInstruction = END_BLOCK;
				Section->CompilePC -= 4;
			}
			break;
		case R4300i_BEQL: 
		case R4300i_BNEL: 
		case R4300i_BLEZL: 
		case R4300i_BGTZL: 
			NextInstruction = LIKELY_DELAY_SLOT;
			Section->Cont.TargetPC = Section->CompilePC + 8;
			Section->Jump.TargetPC = Section->CompilePC + ((short)Command.BRANCH.offset << 2) + 4;
			if (Section->CompilePC == Section->Jump.TargetPC) {
				MIPS_DWORD CompilePC;
				CompilePC.DW = (int)Section->CompilePC;
				if (!DelaySlotEffectsCompare(CompilePC,Command.BRANCH.rs,Command.BRANCH.rt)) {
					Section->Jump.PermLoop = TRUE;
				}
			} 
			break;
		case R4300i_DADDI: 
		case R4300i_DADDIU: 
			if (Command.BRANCH.rt == 0) { break; }
			if (Section->InLoop && Command.BRANCH.rs == Command.BRANCH.rt) {
				MipsRegState(Command.BRANCH.rt) = STATE_UNKNOWN;	
			}
			if (IsConst(Command.BRANCH.rs)) { 
				if (Is64Bit(Command.BRANCH.rs)) { 
					int imm32 = (short)Opcode.IMM.immediate;
					__int64 imm64 = imm32;										
					MipsReg_S(Command.BRANCH.rt) = MipsRegLo_S(Command.BRANCH.rs) + imm64;
				} else {
					MipsReg_S(Command.BRANCH.rt) = MipsRegLo_S(Command.BRANCH.rs) + (short)Command.IMM.immediate;
				}
				MipsRegState(Command.BRANCH.rt) = STATE_CONST_64;
			} else {
				MipsRegState(Command.BRANCH.rt) = STATE_UNKNOWN;
			}
			break;
		case R4300i_LDR:
		case R4300i_LDL:
		case R4300i_LB:
		case R4300i_LH: 
		case R4300i_LWL: 
		case R4300i_LW: 
		case R4300i_LWU: 
		case R4300i_LL: 
		case R4300i_LBU:
		case R4300i_LHU: 
		case R4300i_LWR: 
		case R4300i_SC: 
			if (Command.BRANCH.rt == 0) { break; }
			MipsRegState(Command.BRANCH.rt) = STATE_UNKNOWN;
			break;
		case R4300i_SB: break;
		case R4300i_SH: break;
		case R4300i_SWL: break;
		case R4300i_SW: break;
		case R4300i_SWR: break;
		case R4300i_SDL: break;
		case R4300i_SDR: break;
		case R4300i_CACHE: break;
		case R4300i_LWC1: break;
		case R4300i_SWC1: break;
		case R4300i_LDC1: break;
		case R4300i_LD:
			if (Command.BRANCH.rt == 0) { break; }
			MipsRegState(Command.BRANCH.rt) = STATE_UNKNOWN;
			break;
		case R4300i_SDC1: break;
		case R4300i_SD: break;
		default:
			NextInstruction = END_BLOCK;
			Section->CompilePC -= 4;
			if (Command.Hex == 0x7C1C97C0) { break; }
			if (Command.Hex == 0x7FFFFFFF) { break; }
			if (Command.Hex == 0xF1F3F5F7) { break; }
			if (Command.Hex == 0xC1200000) { break; }
			if (Command.Hex == 0x4C5A5353) { break; }
			if (ShowDebugMessages) {
				MIPS_DWORD CompilePC;
				CompilePC.DW = (int)Section->CompilePC;

				DisplayError("Unhandled R4300i OpCode in FillSectionInfo 1\n%s\n%X",
					R4300iOpcodeName(Command.Hex, CompilePC), Command.Hex);
			}
		}

//		if (Section->CompilePC == 0x8005E4B8) {
//CPU_Message("%X: %s %s = %d",Section->CompilePC,R4300iOpcodeName(Command.Hex,Section->CompilePC),
//			GPR_Name[8],MipsRegState(8));
//_asm int 3
//		}
		switch (NextInstruction) {
		case NORMAL: 
			Section->CompilePC += 4; 
			break;
		case DELAY_SLOT:
			NextInstruction = DELAY_SLOT_DONE;
			Section->CompilePC += 4; 
			break;
		case LIKELY_DELAY_SLOT:
			memcpy(&Section->Cont.RegSet,&Section->RegWorking,sizeof(REG_INFO));
			NextInstruction = LIKELY_DELAY_SLOT_DONE;
			Section->CompilePC += 4; 
			break;
		case DELAY_SLOT_DONE:
			memcpy(&Section->Cont.RegSet,&Section->RegWorking,sizeof(REG_INFO));
			memcpy(&Section->Jump.RegSet,&Section->RegWorking,sizeof(REG_INFO));
			NextInstruction = END_BLOCK;
			break;
		case LIKELY_DELAY_SLOT_DONE:
			memcpy(&Section->Jump.RegSet,&Section->RegWorking,sizeof(REG_INFO));
			NextInstruction = END_BLOCK;
			break;
		}		
		if ((Section->CompilePC & 0xFFFFF000) != (Section->StartPC & 0xFFFFF000)) {
			if (NextInstruction != END_BLOCK && NextInstruction != NORMAL) {
			//	DisplayError("Branch running over delay slot ???\nNextInstruction == %d",NextInstruction);
				Section->Cont.TargetPC = (DWORD)-1;
				Section->Jump.TargetPC = (DWORD)-1;
			} 
			NextInstruction = END_BLOCK;
			Section->CompilePC -= 4;
		}
	} while (NextInstruction != END_BLOCK);

	if (Section->Cont.TargetPC != (DWORD)-1) {
		if ((Section->Cont.TargetPC & 0xFFFFF000) != (Section->StartPC & 0xFFFFF000)) {
			Section->Cont.TargetPC = (DWORD)-1;
		}
	}
	if (Section->Jump.TargetPC != (DWORD)-1) {
		if ((Section->Jump.TargetPC & 0xFFFFF000) != (Section->StartPC & 0xFFFFF000)) {
			Section->Jump.TargetPC = (DWORD)-1;
		}
	}
}

void _fastcall FixConstants (BLOCK_SECTION * Section, DWORD Test, int * Changed) {
	BLOCK_SECTION * Parent;
	int count, NoOfParents;
	REG_INFO Original[2];

	if (Section == NULL) { return; }
	if (Section->Test == Test) { return; }
	Section->Test = Test;

	InheritConstants(Section);
		
	memcpy(&Original[0],&Section->Cont.RegSet,sizeof(REG_INFO));
	memcpy(&Original[1],&Section->Jump.RegSet,sizeof(REG_INFO));

	if (Section->ParentSection) {
		for (NoOfParents = 0;Section->ParentSection[NoOfParents] != NULL;NoOfParents++) {
			Parent = Section->ParentSection[NoOfParents];
			if (Parent->ContinueSection == Section) {
				for (count = 0; count < 32; count++) {
					if (Section->RegStart.MIPS_RegState[count] != Parent->Cont.RegSet.MIPS_RegState[count]) {
						Section->RegStart.MIPS_RegState[count] = STATE_UNKNOWN;							
						//*Changed = TRUE;
					}
					Section->RegStart.MIPS_RegState[count] = STATE_UNKNOWN;							
				}
			}
			if (Parent->JumpSection == Section) {
				for (count = 0; count < 32; count++) {
					if (Section->RegStart.MIPS_RegState[count] != Parent->Jump.RegSet.MIPS_RegState[count]) {
						Section->RegStart.MIPS_RegState[count] = STATE_UNKNOWN;
						//*Changed = TRUE;
					}
				}
			}
			memcpy(&Section->RegWorking,&Section->RegStart,sizeof(REG_INFO));
		}
	}
	FillSectionInfo(Section);
	if (memcmp(&Original[0],&Section->Cont.RegSet,sizeof(REG_INFO)) != 0) { *Changed = TRUE; }
	if (memcmp(&Original[1],&Section->Jump.RegSet,sizeof(REG_INFO)) != 0) { *Changed = TRUE; }

	if (Section->JumpSection) { FixConstants(Section->JumpSection,Test,Changed); }
	if (Section->ContinueSection) { FixConstants(Section->ContinueSection,Test,Changed); }
}

void FixRandomReg (void) {
	while ((int)Registers.CP0[1].W[0] < (int)Registers.CP0[6].W[0]) {
		Registers.CP0[1].UW[0] += 32 - Registers.CP0[6].UW[0];
	}
}

void FreeSection (BLOCK_SECTION * Section, BLOCK_SECTION * Parent) {
	if (Section == NULL) { return; }

	if (Section->ParentSection) {
		int NoOfParents, count;
		
		for (NoOfParents = 0;Section->ParentSection[NoOfParents] != NULL;NoOfParents++);

		for (count = 0; count < NoOfParents; count++) {
			if (Section->ParentSection[count] == Parent) {
				if (NoOfParents == 1) {
					free(Section->ParentSection);
					//CPU_Message("Free Parent Section (Section: %d)",Section->SectionID);
					Section->ParentSection = NULL;
				} else {
					memmove(&Section->ParentSection[count],&Section->ParentSection[count + 1],
						sizeof(void*) * (NoOfParents - count));				
					Section->ParentSection = realloc(Section->ParentSection,NoOfParents*sizeof(void *));
				}
				NoOfParents -= 1;
			}
		}		
		
		if (Parent->JumpSection == Section) { Parent->JumpSection = NULL; }
		if (Parent->ContinueSection == Section) { Parent->ContinueSection = NULL; }
		
		if (Section->ParentSection) {
			for (count = 0; count < NoOfParents; count++) {
				if (!IsAllParentLoops(Section,Section->ParentSection[count],FALSE,GetNewTestValue())) { return; }
			}
			for (count = 0; count < NoOfParents; count++) {
				Parent = Section->ParentSection[count];
				if (Parent->JumpSection == Section) { Parent->JumpSection = NULL; }
				if (Parent->ContinueSection == Section) { Parent->ContinueSection = NULL; }
			}
			free(Section->ParentSection);
			//CPU_Message("Free Parent Section (Section: %d)",Section->SectionID);
			Section->ParentSection = NULL;
		}
	}
	if (Section->ParentSection == NULL) {
		FreeSection(Section->JumpSection,Section);
		FreeSection(Section->ContinueSection,Section);
		//CPU_Message("Free Section (Section: %d)",Section->SectionID);
		free(Section);
	}
}

void GenerateBasicSectionLinkage (BLOCK_SECTION * Section) {
	_asm int 3
}

void GenerateSectionLinkage (BLOCK_SECTION * Section) {
	BLOCK_SECTION * TargetSection[2], *Parent;
	JUMP_INFO * JumpInfo[2];
	BYTE * Jump;
	int count;
	
	TargetSection[0] = Section->ContinueSection;
	TargetSection[1] = Section->JumpSection;	
	JumpInfo[0] = &Section->Cont;
	JumpInfo[1] = &Section->Jump;

	for (count = 0; count < 2; count ++) {
		if (JumpInfo[count]->LinkLocation == NULL && JumpInfo[count]->FallThrough == FALSE) {
			JumpInfo[count]->TargetPC = -1;
		}
	}
	if ((Section->CompilePC & 0xFFC) == 0xFFC) {
		//Handle Fall througth
		Jump = NULL;
		for (count = 0; count < 2; count ++) {
			if (!JumpInfo[count]->FallThrough) { continue; }
			JumpInfo[count]->FallThrough = FALSE;
			if (JumpInfo[count]->LinkLocation != NULL) {
				SetJump32(JumpInfo[count]->LinkLocation,RecompPos);
				JumpInfo[count]->LinkLocation = NULL;
				if (JumpInfo[count]->LinkLocation2 != NULL) { 
					SetJump32(JumpInfo[count]->LinkLocation2,RecompPos);
					JumpInfo[count]->LinkLocation2 = NULL;
				}
			}
			MoveConstToVariable(JumpInfo[count]->TargetPC,&JumpToLocation,"JumpToLocation");
			if (JumpInfo[(count + 1) & 1]->LinkLocation == NULL) { break; }
			JmpLabel8("FinishBlock",0);
			Jump = RecompPos - 1;
		}		
		for (count = 0; count < 2; count ++) {
			if (JumpInfo[count]->LinkLocation == NULL) { continue; }
			JumpInfo[count]->FallThrough = FALSE;
			if (JumpInfo[count]->LinkLocation != NULL) {
				SetJump32(JumpInfo[count]->LinkLocation,RecompPos);
				JumpInfo[count]->LinkLocation = NULL;
				if (JumpInfo[count]->LinkLocation2 != NULL) { 
					SetJump32(JumpInfo[count]->LinkLocation2,RecompPos);
					JumpInfo[count]->LinkLocation2 = NULL;
				}
			}
			MoveConstToVariable(JumpInfo[count]->TargetPC,&JumpToLocation,"JumpToLocation");
			if (JumpInfo[(count + 1) & 1]->LinkLocation == NULL) { break; }
			JmpLabel8("FinishBlock",0);
			Jump = RecompPos - 1;
		}
		if (Jump != NULL) {
			CPU_Message("      $FinishBlock:");
			SetJump8(Jump,RecompPos);
		}
		MoveConstToVariable(Section->CompilePC + 4,&PROGRAM_COUNTER,"PROGRAM_COUNTER");
		if (BlockCycleCount != 0) { 
			AddConstToVariable(BlockCycleCount,&CP0[9],Cop0_Name[9]); 
			SubConstFromVariable(BlockCycleCount,&Timers.Timer,"Timer");
		}
		if (BlockRandomModifier != 0) { SubConstFromVariable(BlockRandomModifier,&CP0[1],Cop0_Name[1]); }
		WriteBackRegisters(Section);
		if (CPU_Type == CPU_SyncCores) { Call_Direct(SyncToPC, "SyncToPC"); }
		MoveConstToVariable(DELAY_SLOT,&NextInstruction,"NextInstruction");
		Ret();
		return;
	}
	if (!UseLinking) {  
		if (Section->CompilePC == Section->Jump.TargetPC && (Section->Cont.TargetPC == -1)) {
			MIPS_DWORD CompilePC;
			CompilePC.DW = (int)Section->CompilePC;
			if (!DelaySlotEffectsJump(CompilePC)) {
				WriteBackRegisters(Section); 
				memcpy(&Section->Jump.RegSet,&Section->RegWorking, sizeof(REG_INFO));
				Call_Direct(InPermLoop,"InPermLoop");
			}
		}
	}
	if (TargetSection[0] != TargetSection[1] || TargetSection[0] == NULL) {
		for (count = 0; count < 2; count ++) {
			if (JumpInfo[count]->LinkLocation == NULL && JumpInfo[count]->FallThrough == FALSE) {
				FreeSection(TargetSection[count],Section);
			} else if (TargetSection[count] == NULL && JumpInfo[count]->FallThrough) {
				if (JumpInfo[count]->LinkLocation != NULL) {
					SetJump32(JumpInfo[count]->LinkLocation,RecompPos);
					JumpInfo[count]->LinkLocation = NULL;
					if (JumpInfo[count]->LinkLocation2 != NULL) { 
						SetJump32(JumpInfo[count]->LinkLocation2,RecompPos);
						JumpInfo[count]->LinkLocation2 = NULL;
					}			
				}
				if (JumpInfo[count]->TargetPC > (Section->CompilePC + 4)) {
					CompileExit (JumpInfo[count]->TargetPC,JumpInfo[count]->RegSet,Normal,TRUE,NULL);
				} else {
					CompileExit (JumpInfo[count]->TargetPC,JumpInfo[count]->RegSet,Normal,TRUE,NULL);
				}
				JumpInfo[count]->FallThrough = FALSE;
			} else if (TargetSection[count] != NULL && JumpInfo[count] != NULL) {
				if (!JumpInfo[count]->FallThrough) { continue; }
				if (JumpInfo[count]->TargetPC == TargetSection[count]->StartPC) { continue; }
				if (JumpInfo[count]->LinkLocation != NULL) {
					SetJump32(JumpInfo[count]->LinkLocation,RecompPos);
					JumpInfo[count]->LinkLocation = NULL;
					if (JumpInfo[count]->LinkLocation2 != NULL) { 
						SetJump32(JumpInfo[count]->LinkLocation2,RecompPos);
						JumpInfo[count]->LinkLocation2 = NULL;
					}			
				}
				CompileExit (JumpInfo[count]->TargetPC,JumpInfo[count]->RegSet,Normal,TRUE,NULL);
				FreeSection(TargetSection[count],Section);
			}
		}
	} else {
		if (Section->Cont.LinkLocation == NULL && Section->Cont.FallThrough == FALSE) { Section->ContinueSection = NULL; }
		if (Section->Jump.LinkLocation == NULL && Section->Jump.FallThrough == FALSE) { Section->JumpSection = NULL; }
		if (Section->JumpSection == NULL &&  Section->ContinueSection == NULL) {
			FreeSection(TargetSection[0],Section);
		}
	}
	
	TargetSection[0] = Section->ContinueSection;
	TargetSection[1] = Section->JumpSection;	

	for (count = 0; count < 2; count ++) {
		if (TargetSection[count] == NULL) { continue; }
		if (!JumpInfo[count]->FallThrough) { continue; }
			
		if (TargetSection[count]->CompiledLocation != NULL) {
			char Label[100];
			sprintf(Label,"Section_%d",TargetSection[count]->SectionID);
			JumpInfo[count]->FallThrough = FALSE;
			if (JumpInfo[count]->LinkLocation != NULL) {
				SetJump32(JumpInfo[count]->LinkLocation,RecompPos);
				JumpInfo[count]->LinkLocation = NULL;
				if (JumpInfo[count]->LinkLocation2 != NULL) { 
					SetJump32(JumpInfo[count]->LinkLocation2,RecompPos);
					JumpInfo[count]->LinkLocation2 = NULL;
				}
			}
			if (JumpInfo[count]->RegSet.RandomModifier != 0) {
				SubConstFromVariable(JumpInfo[count]->RegSet.RandomModifier,&CP0[1],Cop0_Name[1]);
				JumpInfo[count]->RegSet.RandomModifier = 0;
			}
			if (JumpInfo[count]->RegSet.CycleCount != 0) {
				AddConstToVariable(JumpInfo[count]->RegSet.CycleCount,&CP0[9],Cop0_Name[9]);
			}
			if (JumpInfo[count]->TargetPC <= Section->CompilePC) {
				DWORD CycleCount = JumpInfo[count]->RegSet.CycleCount;
				JumpInfo[count]->RegSet.CycleCount = 0;

CPU_Message("PermLoop ***");
				if (JumpInfo[count]->PermLoop) {
					MoveConstToVariable(JumpInfo[count]->TargetPC,&PROGRAM_COUNTER,"PROGRAM_COUNTER");
					Call_Direct(InPermLoop,"InPermLoop");
					CompileSystemCheck(0,-1,JumpInfo[count]->RegSet);
				} else {
					CompileSystemCheck(CycleCount,JumpInfo[count]->TargetPC,JumpInfo[count]->RegSet);
				}
			} else {
				if (JumpInfo[count]->RegSet.CycleCount != 0) {
					SubConstFromVariable(JumpInfo[count]->RegSet.CycleCount,&Timers.Timer,"Timer");
					JumpInfo[count]->RegSet.CycleCount = 0;
				}
			}
			memcpy(&Section->RegWorking, &JumpInfo[count]->RegSet,sizeof(REG_INFO));
			SyncRegState(Section,&TargetSection[count]->RegStart);						
			JmpLabel32(Label,0);
			SetJump32((DWORD *)RecompPos - 1,TargetSection[count]->CompiledLocation);
		}
	}
	//Section->CycleCount = 0;
	//Section->RandomModifier = 0;

	for (count = 0; count < 2; count ++) {
		int count2;

		if (TargetSection[count] == NULL) { continue; }
		if (TargetSection[count]->ParentSection == NULL) { continue; }

		for (count2 = 0;TargetSection[count]->ParentSection[count2] != NULL;count2++) {
			Parent = TargetSection[count]->ParentSection[count2];
			if (Parent->CompiledLocation != NULL) { continue; }
			if (JumpInfo[count]->FallThrough) { 
				JumpInfo[count]->FallThrough = FALSE;
				JmpLabel32(JumpInfo[count]->BranchLabel,0);
				JumpInfo[count]->LinkLocation = RecompPos - 4;
			}
		}
	}

	for (count = 0; count < 2; count ++) {
		if (JumpInfo[count]->FallThrough) { 
			if (JumpInfo[count]->TargetPC < Section->CompilePC) {
				DWORD CycleCount = JumpInfo[count]->RegSet.CycleCount;;

				if (JumpInfo[count]->RegSet.RandomModifier != 0) {
					SubConstFromVariable(JumpInfo[count]->RegSet.RandomModifier,&CP0[1],Cop0_Name[1]);
					JumpInfo[count]->RegSet.RandomModifier = 0;
				}
				if (JumpInfo[count]->RegSet.CycleCount != 0) {
					AddConstToVariable(JumpInfo[count]->RegSet.CycleCount,&CP0[9],Cop0_Name[9]);
				}
				JumpInfo[count]->RegSet.CycleCount = 0;

				CompileSystemCheck(CycleCount,JumpInfo[count]->TargetPC,JumpInfo[count]->RegSet);
			}
		}
	}

	CPU_Message("====== End of Section %d ======",Section->SectionID);

	for (count = 0; count < 2; count ++) {
		if (JumpInfo[count]->FallThrough) { 
			GenerateX86Code(TargetSection[count],GetNewTestValue()); 
		}
	}
	
	//CPU_Message("Section %d",Section->SectionID);
	for (count = 0; count < 2; count ++) {
		if (JumpInfo[count]->LinkLocation == NULL) { continue; }
		if (TargetSection[count] == NULL) {
			CPU_Message("ExitBlock (from %d):",Section->SectionID);
			SetJump32(JumpInfo[count]->LinkLocation,RecompPos);
			JumpInfo[count]->LinkLocation = NULL;
			if (JumpInfo[count]->LinkLocation2 != NULL) { 
				SetJump32(JumpInfo[count]->LinkLocation2,RecompPos);
				JumpInfo[count]->LinkLocation2 = NULL;
			}			
			CompileExit (JumpInfo[count]->TargetPC,JumpInfo[count]->RegSet,Normal,TRUE,NULL);
			continue;
		}
		if (JumpInfo[count]->TargetPC != TargetSection[count]->StartPC) {
			DisplayError("I need to add more code in GenerateSectionLinkage cause this is going to cause an exception");
			_asm int 3
		}
		if (TargetSection[count]->CompiledLocation == NULL) {
			GenerateX86Code(TargetSection[count],GetNewTestValue()); 
		} else {
			char Label[100];

			sprintf(Label, "Section_%d", TargetSection[count]->SectionID);
			CPU_Message("Section_%d (from %d):",TargetSection[count]->SectionID,Section->SectionID);
			SetJump32(JumpInfo[count]->LinkLocation,RecompPos);
			JumpInfo[count]->LinkLocation = NULL;
			if (JumpInfo[count]->LinkLocation2 != NULL) { 
				SetJump32(JumpInfo[count]->LinkLocation2,RecompPos);
				JumpInfo[count]->LinkLocation2 = NULL;
			}			
			memcpy(&Section->RegWorking,&JumpInfo[count]->RegSet,sizeof(REG_INFO));
			if (JumpInfo[count]->RegSet.RandomModifier != 0) {
				SubConstFromVariable(JumpInfo[count]->RegSet.RandomModifier,&CP0[1],Cop0_Name[1]);
				JumpInfo[count]->RegSet.RandomModifier = 0;
			}
			if (JumpInfo[count]->RegSet.CycleCount != 0) {
				AddConstToVariable(JumpInfo[count]->RegSet.CycleCount,&CP0[9],Cop0_Name[9]);
			}
			if (JumpInfo[count]->TargetPC <= Section->CompilePC) {
				DWORD CycleCount = JumpInfo[count]->RegSet.CycleCount;
				JumpInfo[count]->RegSet.CycleCount = 0;

CPU_Message("PermLoop ***");
				if (JumpInfo[count]->PermLoop) {
					MoveConstToVariable(JumpInfo[count]->TargetPC,&PROGRAM_COUNTER,"PROGRAM_COUNTER");
					Call_Direct(InPermLoop,"InPermLoop");
					CompileSystemCheck(0,-1,JumpInfo[count]->RegSet);
				} else {
					CompileSystemCheck(CycleCount,JumpInfo[count]->TargetPC,JumpInfo[count]->RegSet);
				}
			} else {
				if (JumpInfo[count]->RegSet.CycleCount != 0) {
					SubConstFromVariable(JumpInfo[count]->RegSet.CycleCount,&Timers.Timer,"Timer");
					JumpInfo[count]->RegSet.CycleCount = 0;
				}
			}
			memcpy(&Section->RegWorking, &JumpInfo[count]->RegSet,sizeof(REG_INFO));
			SyncRegState(Section,&TargetSection[count]->RegStart);						
			JmpLabel32(Label,0);
			SetJump32((DWORD *)RecompPos - 1,TargetSection[count]->CompiledLocation);
		}
	}
}

BOOL GenerateX86Code (BLOCK_SECTION * Section, DWORD Test) {
	int count;

	if (Section == NULL) { return FALSE; }
	//CPU_Message("Section: %d",Section->SectionID);
	if (Section->CompiledLocation != NULL) { 		
		if (Section->Test == Test) { return FALSE; }
		Section->Test = Test;
		if (GenerateX86Code(Section->ContinueSection,Test)) { return TRUE; }
		if (GenerateX86Code(Section->JumpSection,Test)) { return TRUE; }
		return FALSE; 
	}
	if (Section->ParentSection) {
		for (count = 0;Section->ParentSection[count] != NULL;count++) {
			BLOCK_SECTION * Parent;
			
			Parent = Section->ParentSection[count];
			if (Parent->CompiledLocation != NULL) { continue; }
			if (IsAllParentLoops(Section,Parent,TRUE,GetNewTestValue())) { continue; }
			return FALSE;
		}
	}
	if (!InheritParentInfo(Section)) { return FALSE; }
	Section->CompiledLocation = RecompPos;
	Section->CompilePC = Section->StartPC;
	NextInstruction = NORMAL;	
	/*if (CPU_Type == CPU_SyncCores) { 
	//if (CPU_Type == CPU_SyncCores && (DWORD)RecompPos > 0x6094C283) { 
		MoveConstToVariable(Section->StartPC,&PROGRAM_COUNTER,"PROGRAM_COUNTER");	
		if (BlockCycleCount != 0) { 
			AddConstToVariable(BlockCycleCount,&CP0[9],Cop0_Name[9]); 
			SubConstFromVariable(BlockCycleCount,&Timers.Timer,"Timer");
		}
		if (BlockRandomModifier != 0) { SubConstFromVariable(BlockRandomModifier,&CP0[1],Cop0_Name[1]); }
		BlockCycleCount = 0;
		BlockRandomModifier = 0;
		Call_Direct(SyncToPC, "SyncToPC"); 
		MoveConstToVariable((DWORD)RecompPos,&CurrentBlock,"CurrentBlock");
	}*/
	do {
		__try {
			MIPS_DWORD Address;
			Address.DW = (long)Section->CompilePC;
			if (!r4300i_LW_VAddr_NonCPU(Address, &Opcode.Hex)) {
				DisplayError(GS(MSG_FAIL_LOAD_WORD));
				ExitThread(0);
			} 
		} __except( r4300i_CPU_MemoryFilter( GetExceptionCode(), GetExceptionInformation()) ) {
			DisplayError(GS(MSG_UNKNOWN_MEM_ACTION));
			ExitThread(0);
		}
		if (SelfModCheck == ModCode_ChangeMemory) {
			if ( (Opcode.Hex >> 16) == 0x7C7C) {
				Opcode.Hex = OrigMem[(Opcode.Hex & 0xFFFF)].OriginalValue;
			}
		}

		//if (Section->CompilePC == 0x800AA51C && NextInstruction == NORMAL) { _asm int 3 }
		//if (Section->CompilePC == 0x80000050 && NextInstruction == NORMAL) { BreakPoint(); }
		/*if (Section->CompilePC >= 0x800C4024 && Section->CompilePC < 0x800C4030) {
			CurrentRoundingModel = RoundUnknown;
		}*/

		/*if (Section->CompilePC >= 0x8006F970 && Section->CompilePC < 0x8006F9A0 && NextInstruction == NORMAL) {
			WriteBackRegisters(Section); 
			if (BlockCycleCount != 0) { 
				AddConstToVariable(BlockCycleCount,&CP0[9],Cop0_Name[9]); 
				SubConstFromVariable(BlockCycleCount,&Timers.Timer,"Timer");
			}
			if (BlockRandomModifier != 0) { SubConstFromVariable(BlockRandomModifier,&CP0[1],Cop0_Name[1]); }
			BlockCycleCount = 0;
			BlockRandomModifier = 0;
			MoveConstToVariable(Section->CompilePC,&PROGRAM_COUNTER,"PROGRAM_COUNTER");
			MoveConstToVariable((DWORD)RecompPos,&CurrentBlock,"CurrentBlock");
			if (CPU_Type == CPU_SyncCores) { Call_Direct(SyncToPC, "SyncToPC"); }
		}*/


		/*if (Section->CompilePC == 0x80000054 && NextInstruction == NORMAL) { 
			WriteBackRegisters(Section); 
			if (BlockCycleCount != 0) { 
				AddConstToVariable(BlockCycleCount,&CP0[9],Cop0_Name[9]); 
				SubConstFromVariable(BlockCycleCount,&Timers.Timer,"Timer");
			}
			if (BlockRandomModifier != 0) { SubConstFromVariable(BlockRandomModifier,&CP0[1],Cop0_Name[1]); }
			BlockCycleCount = 0;
			BlockRandomModifier = 0;
			MoveConstToVariable(Section->CompilePC,&PROGRAM_COUNTER,"PROGRAM_COUNTER");
			MoveConstToVariable((DWORD)RecompPos,&CurrentBlock,"CurrentBlock");
			if (CPU_Type == CPU_SyncCores) { Call_Direct(SyncToPC, "SyncToPC"); }
		}*/
		/*if (Section->CompilePC == 0x8005E460 && NextInstruction == NORMAL) { 
			WriteBackRegisters(Section); 
			if (BlockCycleCount != 0) { 
				AddConstToVariable(BlockCycleCount,&CP0[9],Cop0_Name[9]); 
				SubConstFromVariable(BlockCycleCount,&Timers.Timer,"Timer");
			}
			if (BlockRandomModifier != 0) { SubConstFromVariable(BlockRandomModifier,&CP0[1],Cop0_Name[1]); }
			BlockCycleCount = 0;
			BlockRandomModifier = 0;
			MoveConstToVariable(Section->CompilePC,&PROGRAM_COUNTER,"PROGRAM_COUNTER");
			MoveConstToVariable((DWORD)RecompPos,&CurrentBlock,"CurrentBlock");
			if (CPU_Type == CPU_SyncCores) { Call_Direct(SyncToPC, "SyncToPC"); }
		}*/
		
		/*if (Section->CompilePC == 0x150A1570 && NextInstruction == NORMAL) { 
			CPU_Message("%s = %d",GPR_Name[14],MipsRegState(14));
		}
		if (Section->CompilePC == 0x150A1514 && NextInstruction == NORMAL) { 
			CPU_Message("%s = %d",GPR_Name[14],MipsRegState(14));
		}
		if (Section->CompilePC == 0x150A1454 && NextInstruction == NORMAL) { 
			CPU_Message("%s = %d",GPR_Name[14],MipsRegState(14));
		}*/
		/*if (Section->CompilePC == 0x150A1570 && NextInstruction == NORMAL) { 
			WriteBackRegisters(Section); 
			if (BlockCycleCount != 0) { 
				AddConstToVariable(BlockCycleCount,&CP0[9],Cop0_Name[9]); 
				SubConstFromVariable(BlockCycleCount,&Timers.Timer,"Timer");
			}
			if (BlockRandomModifier != 0) { SubConstFromVariable(BlockRandomModifier,&CP0[1],Cop0_Name[1]); }
			BlockCycleCount = 0;
			BlockRandomModifier = 0;
			MoveConstToVariable(Section->CompilePC,&PROGRAM_COUNTER,"PROGRAM_COUNTER");
			MoveConstToVariable((DWORD)RecompPos,&CurrentBlock,"CurrentBlock");
			if (CPU_Type == CPU_SyncCores) { Call_Direct(SyncToPC, "SyncToPC"); }
		}
		if (Section->CompilePC == 0x150A1514 && NextInstruction == NORMAL) { 
			WriteBackRegisters(Section); 
			if (BlockCycleCount != 0) { 
				AddConstToVariable(BlockCycleCount,&CP0[9],Cop0_Name[9]); 
				SubConstFromVariable(BlockCycleCount,&Timers.Timer,"Timer");
			}
			if (BlockRandomModifier != 0) { SubConstFromVariable(BlockRandomModifier,&CP0[1],Cop0_Name[1]); }
			BlockCycleCount = 0;
			BlockRandomModifier = 0;
			MoveConstToVariable(Section->CompilePC,&PROGRAM_COUNTER,"PROGRAM_COUNTER");
			MoveConstToVariable((DWORD)RecompPos,&CurrentBlock,"CurrentBlock");
			if (CPU_Type == CPU_SyncCores) { Call_Direct(SyncToPC, "SyncToPC"); }
		}

		if (Section->CompilePC == 0x150A1454 && NextInstruction == NORMAL) { 
			WriteBackRegisters(Section); 
			if (BlockCycleCount != 0) { 
				AddConstToVariable(BlockCycleCount,&CP0[9],Cop0_Name[9]); 
				SubConstFromVariable(BlockCycleCount,&Timers.Timer,"Timer");
			}
			if (BlockRandomModifier != 0) { SubConstFromVariable(BlockRandomModifier,&CP0[1],Cop0_Name[1]); }
			BlockCycleCount = 0;
			BlockRandomModifier = 0;
			MoveConstToVariable(Section->CompilePC,&PROGRAM_COUNTER,"PROGRAM_COUNTER");
			MoveConstToVariable((DWORD)RecompPos,&CurrentBlock,"CurrentBlock");
			if (CPU_Type == CPU_SyncCores) { Call_Direct(SyncToPC, "SyncToPC"); }
		}*/

		BlockCycleCount += CountPerOp;
		BlockRandomModifier += 1;
				
		for (count = 1; count < 10; count ++) { x86Protected(count) = FALSE; }

		switch (Opcode.BRANCH.op) {
		case R4300i_SPECIAL:
			switch (Opcode.REG.funct) {
			case R4300i_SPECIAL_SYNC: break;
			case R4300i_SPECIAL_SLL: Compile_R4300i_SPECIAL_SLL(Section); break;
			case R4300i_SPECIAL_SRL: Compile_R4300i_SPECIAL_SRL(Section); break;
			case R4300i_SPECIAL_SRA: Compile_R4300i_SPECIAL_SRA(Section); break;
			case R4300i_SPECIAL_SLLV: Compile_R4300i_SPECIAL_SLLV(Section); break;
			case R4300i_SPECIAL_SRLV: Compile_R4300i_SPECIAL_SRLV(Section); break;
			case R4300i_SPECIAL_SRAV: Compile_R4300i_SPECIAL_SRAV(Section); break;
			case R4300i_SPECIAL_JR: Compile_R4300i_SPECIAL_JR(Section); break;
			case R4300i_SPECIAL_JALR: Compile_R4300i_SPECIAL_JALR(Section); break;
			case R4300i_SPECIAL_MFLO: Compile_R4300i_SPECIAL_MFLO(Section); break;
			case R4300i_SPECIAL_SYSCALL: Compile_R4300i_SPECIAL_SYSCALL(Section); break;
			case R4300i_SPECIAL_MTLO: Compile_R4300i_SPECIAL_MTLO(Section); break;
			case R4300i_SPECIAL_MFHI: Compile_R4300i_SPECIAL_MFHI(Section); break;
			case R4300i_SPECIAL_MTHI: Compile_R4300i_SPECIAL_MTHI(Section); break;
			case R4300i_SPECIAL_DSLLV: Compile_R4300i_SPECIAL_DSLLV(Section); break;
			case R4300i_SPECIAL_DSRLV: Compile_R4300i_SPECIAL_DSRLV(Section); break;
			case R4300i_SPECIAL_DSRAV: Compile_R4300i_SPECIAL_DSRAV(Section); break;
			case R4300i_SPECIAL_MULT: Compile_R4300i_SPECIAL_MULT(Section); break;
			case R4300i_SPECIAL_DIV: Compile_R4300i_SPECIAL_DIV(Section); break;
			case R4300i_SPECIAL_DIVU: Compile_R4300i_SPECIAL_DIVU(Section); break;
			case R4300i_SPECIAL_MULTU: Compile_R4300i_SPECIAL_MULTU(Section); break;
			case R4300i_SPECIAL_DMULT: Compile_R4300i_SPECIAL_DMULT(Section); break;
			case R4300i_SPECIAL_DMULTU: Compile_R4300i_SPECIAL_DMULTU(Section); break;
			case R4300i_SPECIAL_DDIV: Compile_R4300i_SPECIAL_DDIV(Section); break;
			case R4300i_SPECIAL_DDIVU: Compile_R4300i_SPECIAL_DDIVU(Section); break;
			case R4300i_SPECIAL_ADD: Compile_R4300i_SPECIAL_ADD(Section); break;
			case R4300i_SPECIAL_ADDU: Compile_R4300i_SPECIAL_ADDU(Section); break;
			case R4300i_SPECIAL_SUB: Compile_R4300i_SPECIAL_SUB(Section); break;
			case R4300i_SPECIAL_SUBU: Compile_R4300i_SPECIAL_SUBU(Section); break;
			case R4300i_SPECIAL_AND: Compile_R4300i_SPECIAL_AND(Section); break;
			case R4300i_SPECIAL_OR: Compile_R4300i_SPECIAL_OR(Section); break;
			case R4300i_SPECIAL_XOR: Compile_R4300i_SPECIAL_XOR(Section); break;
			case R4300i_SPECIAL_NOR: Compile_R4300i_SPECIAL_NOR(Section); break;
			case R4300i_SPECIAL_SLT: Compile_R4300i_SPECIAL_SLT(Section); break;
			case R4300i_SPECIAL_SLTU: Compile_R4300i_SPECIAL_SLTU(Section); break;
			case R4300i_SPECIAL_DADD: Compile_R4300i_SPECIAL_DADD(Section); break;
			case R4300i_SPECIAL_DADDU: Compile_R4300i_SPECIAL_DADDU(Section); break;
			case R4300i_SPECIAL_DSUB: Compile_R4300i_SPECIAL_DSUB(Section); break;
			case R4300i_SPECIAL_DSUBU: Compile_R4300i_SPECIAL_DSUBU(Section); break;
			case R4300i_SPECIAL_DSLL: Compile_R4300i_SPECIAL_DSLL(Section); break;
			case R4300i_SPECIAL_DSRL: Compile_R4300i_SPECIAL_DSRL(Section); break;
			case R4300i_SPECIAL_DSRA: Compile_R4300i_SPECIAL_DSRA(Section); break;
			case R4300i_SPECIAL_DSLL32: Compile_R4300i_SPECIAL_DSLL32(Section); break;
			case R4300i_SPECIAL_DSRL32: Compile_R4300i_SPECIAL_DSRL32(Section); break;
			case R4300i_SPECIAL_DSRA32: Compile_R4300i_SPECIAL_DSRA32(Section); break;
			case R4300i_SPECIAL_BREAK: Compile_R4300i_SPECIAL_BREAK(Section); break;
			case R4300i_SPECIAL_TEQ: Compile_R4300i_SPECIAL_TEQ(Section); break;
			case R4300i_SPECIAL_TGE: Compile_R4300i_SPECIAL_TGE(Section); break;
			case R4300i_SPECIAL_TGEU: Compile_R4300i_SPECIAL_TGEU(Section); break;
			case R4300i_SPECIAL_TLT: Compile_R4300i_SPECIAL_TLT(Section); break;
			case R4300i_SPECIAL_TLTU: Compile_R4300i_SPECIAL_TLTU(Section); break;
			case R4300i_SPECIAL_TNE: Compile_R4300i_SPECIAL_TNE(Section); break;
			default:
				Compile_R4300i_UnknownOpcode(Section); break;
			}
			break;
		case R4300i_REGIMM: 
			switch (Opcode.BRANCH.rt) {
			case R4300i_REGIMM_BLTZ:Compile_R4300i_Branch(Section,BLTZ_Compare,BranchTypeRs, FALSE); break;
			case R4300i_REGIMM_BGEZ:Compile_R4300i_Branch(Section,BGEZ_Compare,BranchTypeRs, FALSE); break;
			case R4300i_REGIMM_BLTZL:Compile_R4300i_BranchLikely(Section,BLTZ_Compare, FALSE); break;
			case R4300i_REGIMM_BGEZL:Compile_R4300i_BranchLikely(Section,BGEZ_Compare, FALSE); break;
			case R4300i_REGIMM_BLTZAL:Compile_R4300i_Branch(Section,BLTZ_Compare,BranchTypeRs, TRUE); break;
			case R4300i_REGIMM_BGEZAL:Compile_R4300i_Branch(Section,BGEZ_Compare,BranchTypeRs, TRUE); break;
			default:
				Compile_R4300i_UnknownOpcode(Section); break;
			}
			break;
		case R4300i_BEQ: Compile_R4300i_Branch(Section,BEQ_Compare,BranchTypeRsRt,FALSE); break;
		case R4300i_BNE: Compile_R4300i_Branch(Section,BNE_Compare,BranchTypeRsRt,FALSE); break;
		case R4300i_BGTZ:Compile_R4300i_Branch(Section,BGTZ_Compare,BranchTypeRs,FALSE); break;
		case R4300i_BLEZ:Compile_R4300i_Branch(Section,BLEZ_Compare,BranchTypeRs,FALSE); break;
		case R4300i_J: Compile_R4300i_J(Section); break;
		case R4300i_JAL: Compile_R4300i_JAL(Section); break;
		case R4300i_ADDI: Compile_R4300i_ADDI(Section); break;
		case R4300i_ADDIU: Compile_R4300i_ADDIU(Section); break;
		case R4300i_SLTI: Compile_R4300i_SLTI(Section); break;
		case R4300i_SLTIU: Compile_R4300i_SLTIU(Section); break;
		case R4300i_ANDI: Compile_R4300i_ANDI(Section); break;
		case R4300i_ORI: Compile_R4300i_ORI(Section); break;
		case R4300i_XORI: Compile_R4300i_XORI(Section); break;
		case R4300i_LUI: Compile_R4300i_LUI(Section); break;
		case R4300i_CP0:
			switch (Opcode.BRANCH.rs) {
			case R4300i_COP0_MF: Compile_R4300i_COP0_MF(Section); break;
			case R4300i_COP0_DMF: Compile_R4300i_COP0_DMF(Section); break;
			case R4300i_COP0_MT: Compile_R4300i_COP0_MT(Section); break;
			case R4300i_COP0_DMT: Compile_R4300i_COP0_DMT(Section); break;
			default:
				if ( (Opcode.BRANCH.rs & 0x10 ) != 0 ) {
					switch( Opcode.REG.funct ) {
					case R4300i_COP0_CO_TLBR: Compile_R4300i_COP0_CO_TLBR(Section); break;
					case R4300i_COP0_CO_TLBWI: Compile_R4300i_COP0_CO_TLBWI(Section); break;
					case R4300i_COP0_CO_TLBWR: Compile_R4300i_COP0_CO_TLBWR(Section); break;
					case R4300i_COP0_CO_TLBP: Compile_R4300i_COP0_CO_TLBP(Section); break;
					case R4300i_COP0_CO_ERET: Compile_R4300i_COP0_CO_ERET(Section); break;
					default: Compile_R4300i_UnknownOpcode(Section); break;
					}
				} else {
					Compile_R4300i_UnknownOpcode(Section);
				}
			}
			break;
		case R4300i_CP1:
			switch (Opcode.BRANCH.rs) {
			case R4300i_COP1_MF: Compile_R4300i_COP1_MF(Section); break;
			case R4300i_COP1_DMF: Compile_R4300i_COP1_DMF(Section); break;
			case R4300i_COP1_CF: Compile_R4300i_COP1_CF(Section); break;
			case R4300i_COP1_MT: Compile_R4300i_COP1_MT(Section); break;
			case R4300i_COP1_DMT: Compile_R4300i_COP1_DMT(Section); break;
			case R4300i_COP1_CT: Compile_R4300i_COP1_CT(Section); break;
			case R4300i_COP1_BC:
				switch (Opcode.FP.ft) {
				case R4300i_COP1_BC_BCF: Compile_R4300i_Branch(Section,COP1_BCF_Compare,BranchTypeCop1,FALSE); break;
				case R4300i_COP1_BC_BCT: Compile_R4300i_Branch(Section,COP1_BCT_Compare,BranchTypeCop1,FALSE); break;
				case R4300i_COP1_BC_BCFL: Compile_R4300i_BranchLikely(Section,COP1_BCF_Compare,FALSE); break;
				case R4300i_COP1_BC_BCTL: Compile_R4300i_BranchLikely(Section,COP1_BCT_Compare,FALSE); break;
				default:
					Compile_R4300i_UnknownOpcode(Section); break;
				}
				break;
			case R4300i_COP1_S: 
				switch (Opcode.REG.funct) {
				case R4300i_COP1_FUNCT_ADD: Compile_R4300i_COP1_S_ADD(Section); break;
				case R4300i_COP1_FUNCT_SUB: Compile_R4300i_COP1_S_SUB(Section); break;
				case R4300i_COP1_FUNCT_MUL: Compile_R4300i_COP1_S_MUL(Section); break;
				case R4300i_COP1_FUNCT_DIV: Compile_R4300i_COP1_S_DIV(Section); break;
				case R4300i_COP1_FUNCT_ABS: Compile_R4300i_COP1_S_ABS(Section); break;
				case R4300i_COP1_FUNCT_NEG: Compile_R4300i_COP1_S_NEG(Section); break;
				case R4300i_COP1_FUNCT_SQRT: Compile_R4300i_COP1_S_SQRT(Section); break;
				case R4300i_COP1_FUNCT_MOV: Compile_R4300i_COP1_S_MOV(Section); break;
				case R4300i_COP1_FUNCT_TRUNC_L: Compile_R4300i_COP1_S_TRUNC_L(Section); break;
				case R4300i_COP1_FUNCT_CEIL_L: Compile_R4300i_COP1_S_CEIL_L(Section); break;	//added by Witten
				case R4300i_COP1_FUNCT_FLOOR_L: Compile_R4300i_COP1_S_FLOOR_L(Section); break;	//added by Witten
				case R4300i_COP1_FUNCT_ROUND_W: Compile_R4300i_COP1_S_ROUND_W(Section); break;
				case R4300i_COP1_FUNCT_TRUNC_W: Compile_R4300i_COP1_S_TRUNC_W(Section); break;
				case R4300i_COP1_FUNCT_CEIL_W: Compile_R4300i_COP1_S_CEIL_W(Section); break;	//added by Witten
				case R4300i_COP1_FUNCT_FLOOR_W: Compile_R4300i_COP1_S_FLOOR_W(Section); break;
				case R4300i_COP1_FUNCT_CVT_D: Compile_R4300i_COP1_S_CVT_D(Section); break;
				case R4300i_COP1_FUNCT_CVT_W: Compile_R4300i_COP1_S_CVT_W(Section); break;
				case R4300i_COP1_FUNCT_CVT_L: Compile_R4300i_COP1_S_CVT_L(Section); break;
				case R4300i_COP1_FUNCT_C_F:   case R4300i_COP1_FUNCT_C_UN:
				case R4300i_COP1_FUNCT_C_EQ:  case R4300i_COP1_FUNCT_C_UEQ:
				case R4300i_COP1_FUNCT_C_OLT: case R4300i_COP1_FUNCT_C_ULT:
				case R4300i_COP1_FUNCT_C_OLE: case R4300i_COP1_FUNCT_C_ULE:
				case R4300i_COP1_FUNCT_C_SF:  case R4300i_COP1_FUNCT_C_NGLE:
				case R4300i_COP1_FUNCT_C_SEQ: case R4300i_COP1_FUNCT_C_NGL:
				case R4300i_COP1_FUNCT_C_LT:  case R4300i_COP1_FUNCT_C_NGE:
				case R4300i_COP1_FUNCT_C_LE:  case R4300i_COP1_FUNCT_C_NGT:
					Compile_R4300i_COP1_S_CMP(Section); break;
				default:
					Compile_R4300i_UnknownOpcode(Section); break;
				}
				break;
			case R4300i_COP1_D: 
				switch (Opcode.REG.funct) {
				case R4300i_COP1_FUNCT_ADD: Compile_R4300i_COP1_D_ADD(Section); break;
				case R4300i_COP1_FUNCT_SUB: Compile_R4300i_COP1_D_SUB(Section); break;
				case R4300i_COP1_FUNCT_MUL: Compile_R4300i_COP1_D_MUL(Section); break;
				case R4300i_COP1_FUNCT_DIV: Compile_R4300i_COP1_D_DIV(Section); break;
				case R4300i_COP1_FUNCT_ABS: Compile_R4300i_COP1_D_ABS(Section); break;
				case R4300i_COP1_FUNCT_NEG: Compile_R4300i_COP1_D_NEG(Section); break;
				case R4300i_COP1_FUNCT_SQRT: Compile_R4300i_COP1_D_SQRT(Section); break;
				case R4300i_COP1_FUNCT_MOV: Compile_R4300i_COP1_D_MOV(Section); break;
				case R4300i_COP1_FUNCT_TRUNC_L: Compile_R4300i_COP1_D_TRUNC_L(Section); break;	//added by Witten
				case R4300i_COP1_FUNCT_CEIL_L: Compile_R4300i_COP1_D_CEIL_L(Section); break;	//added by Witten
				case R4300i_COP1_FUNCT_FLOOR_L: Compile_R4300i_COP1_D_FLOOR_L(Section); break;	//added by Witten
				case R4300i_COP1_FUNCT_ROUND_W: Compile_R4300i_COP1_D_ROUND_W(Section); break;
				case R4300i_COP1_FUNCT_TRUNC_W: Compile_R4300i_COP1_D_TRUNC_W(Section); break;
				case R4300i_COP1_FUNCT_CEIL_W: Compile_R4300i_COP1_D_CEIL_W(Section); break;	//added by Witten
				case R4300i_COP1_FUNCT_FLOOR_W: Compile_R4300i_COP1_D_FLOOR_W(Section); break;	//added by Witten
				case R4300i_COP1_FUNCT_CVT_S: Compile_R4300i_COP1_D_CVT_S(Section); break;
				case R4300i_COP1_FUNCT_CVT_W: Compile_R4300i_COP1_D_CVT_W(Section); break;
				case R4300i_COP1_FUNCT_CVT_L: Compile_R4300i_COP1_D_CVT_L(Section); break;
				case R4300i_COP1_FUNCT_C_F:   case R4300i_COP1_FUNCT_C_UN:
				case R4300i_COP1_FUNCT_C_EQ:  case R4300i_COP1_FUNCT_C_UEQ:
				case R4300i_COP1_FUNCT_C_OLT: case R4300i_COP1_FUNCT_C_ULT:
				case R4300i_COP1_FUNCT_C_OLE: case R4300i_COP1_FUNCT_C_ULE:
				case R4300i_COP1_FUNCT_C_SF:  case R4300i_COP1_FUNCT_C_NGLE:
				case R4300i_COP1_FUNCT_C_SEQ: case R4300i_COP1_FUNCT_C_NGL:
				case R4300i_COP1_FUNCT_C_LT:  case R4300i_COP1_FUNCT_C_NGE:
				case R4300i_COP1_FUNCT_C_LE:  case R4300i_COP1_FUNCT_C_NGT:
					Compile_R4300i_COP1_D_CMP(Section); break;
				default:
					Compile_R4300i_UnknownOpcode(Section); break;
				}
				break;
			case R4300i_COP1_W: 
				switch (Opcode.REG.funct) {
				case R4300i_COP1_FUNCT_CVT_S: Compile_R4300i_COP1_W_CVT_S(Section); break;
				case R4300i_COP1_FUNCT_CVT_D: Compile_R4300i_COP1_W_CVT_D(Section); break;
				default:
					Compile_R4300i_UnknownOpcode(Section); break;
				}
				break;
			case R4300i_COP1_L: 
				switch (Opcode.REG.funct) {
				case R4300i_COP1_FUNCT_CVT_S: Compile_R4300i_COP1_L_CVT_S(Section); break;
				case R4300i_COP1_FUNCT_CVT_D: Compile_R4300i_COP1_L_CVT_D(Section); break;
				default:
					Compile_R4300i_UnknownOpcode(Section); break;
				}
				break;
			default:
				Compile_R4300i_UnknownOpcode(Section); break;
			}
			break;
		case R4300i_BEQL: Compile_R4300i_BranchLikely(Section,BEQ_Compare,FALSE); break;
		case R4300i_BNEL: Compile_R4300i_BranchLikely(Section,BNE_Compare,FALSE); break;
		case R4300i_BGTZL:Compile_R4300i_BranchLikely(Section,BGTZ_Compare,FALSE); break;
		case R4300i_BLEZL:Compile_R4300i_BranchLikely(Section,BLEZ_Compare,FALSE); break;
		case R4300i_DADDI: Compile_R4300i_DADDI(Section); break;
		case R4300i_DADDIU: Compile_R4300i_DADDIU(Section); break;
		case R4300i_LDL: Compile_R4300i_LDL(Section); break;
		case R4300i_LDR: Compile_R4300i_LDR(Section); break;
		case R4300i_LB: Compile_R4300i_LB(Section); break;
		case R4300i_LH: Compile_R4300i_LH(Section); break;
		case R4300i_LWL: Compile_R4300i_LWL(Section); break;
		case R4300i_LW: Compile_R4300i_LW(Section); break;
		case R4300i_LBU: Compile_R4300i_LBU(Section); break;
		case R4300i_LHU: Compile_R4300i_LHU(Section); break;
		case R4300i_LWR: Compile_R4300i_LWR(Section); break;
		case R4300i_LWU: Compile_R4300i_LWU(Section); break;	//added by Witten
		case R4300i_SB: Compile_R4300i_SB(Section); break;
		case R4300i_SH: Compile_R4300i_SH(Section); break;
		case R4300i_SWL: Compile_R4300i_SWL(Section); break;
		case R4300i_SW: Compile_R4300i_SW(Section); break;
		case R4300i_SWR: Compile_R4300i_SWR(Section); break;
		case R4300i_SDL: Compile_R4300i_SDL(Section); break;
		case R4300i_SDR: Compile_R4300i_SDR(Section); break;
		case R4300i_CACHE: Compile_R4300i_CACHE(Section); break;
		case R4300i_LL: Compile_R4300i_LL(Section); break;
		case R4300i_LWC1: Compile_R4300i_LWC1(Section); break;
		case R4300i_LDC1: Compile_R4300i_LDC1(Section); break;
		case R4300i_SC: Compile_R4300i_SC(Section); break;
		case R4300i_LD: Compile_R4300i_LD(Section); break;
		case R4300i_SWC1: Compile_R4300i_SWC1(Section); break;
		case R4300i_SDC1: Compile_R4300i_SDC1(Section); break;
		case R4300i_SD: Compile_R4300i_SD(Section); break;
		default:
			Compile_R4300i_UnknownOpcode(Section); break;
		}

		if (DisableRegCaching) { WriteBackRegisters(Section); }
		for (count = 1; count < 10; count ++) { x86Protected(count) = FALSE; }
		
		/*if ((DWORD)RecompPos > 0x60B452E6) {
			if (Section->CompilePC == 0x8002D9B8 && Section->CompilePC < 0x8002DA20) {
				CurrentRoundingModel = RoundUnknown;
			}
		}*/
		UnMap_AllFPRs(Section);
		
		/*if ((DWORD)RecompPos > 0x60AD0BD3) {
			if (Section->CompilePC >= 0x8008B804 && Section->CompilePC < 0x800496D8) {
				CPU_Message("Blah *");
				WriteBackRegisters(Section);
			}
			/*if (Section->CompilePC >= 0x80000180 && Section->CompilePC < 0x80000190) {
				CPU_Message("Blah *");
				//WriteBackRegisters(Section);
			}*/
		//}

		/*for (count = 1; count < 10; count ++) { 
			if (x86Mapped(count) == Stack_Mapped) { 
				UnMap_X86reg (Section, count); 
			}
		}*/
		//CPU_Message("MemoryStack = %s",Map_MemoryStack(Section, FALSE) > 0?x86_Name(Map_MemoryStack(Section, FALSE)):"Not Mapped");

		if ((Section->CompilePC &0xFFC) == 0xFFC) {
			if (NextInstruction == DO_DELAY_SLOT) {
				if (ShowDebugMessages)
					DisplayError("Wanting to do delay slot over end of block");
			}
			if (NextInstruction == NORMAL) {
				CompileExit (Section->CompilePC + 4,Section->RegWorking,Normal,TRUE,NULL);
				NextInstruction = END_BLOCK;
			}
		}

		switch (NextInstruction) {
		case NORMAL: 
			Section->CompilePC += 4; 
			break;
		case DO_DELAY_SLOT:
			NextInstruction = DELAY_SLOT;
			Section->CompilePC += 4; 
			break;
		case DELAY_SLOT:
			NextInstruction = DELAY_SLOT_DONE;
			BlockCycleCount += CPOAdjust;
			BlockRandomModifier -= 1;
			Section->CompilePC -= 4; 
			break;
		}
	} while (NextInstruction != END_BLOCK);
	
	return TRUE;
}

DWORD GetNewTestValue(void) {
	static DWORD LastTest = 0;
	if (LastTest == 0xFFFFFFFF) { LastTest = 0; }
	LastTest += 1;
	return LastTest;
}

void InitilizeRegSet(REG_INFO * RegSet) {
	int count;
	
	RegSet->MIPS_RegState[0]  = STATE_CONST_32;
	RegSet->MIPS_RegVal[0].DW = 0;
	for (count = 1; count < 32; count ++ ) {
		RegSet->MIPS_RegState[count]   = STATE_UNKNOWN;
		RegSet->MIPS_RegVal[count].DW = 0;

	}
	for (count = 0; count < 10; count ++ ) {
		RegSet->x86reg_MappedTo[count]  = NotMapped;
		RegSet->x86reg_Protected[count] = FALSE;
		RegSet->x86reg_MapOrder[count]  = 0;
	}
	RegSet->CycleCount = 0;
	RegSet->RandomModifier = 0;

	RegSet->Stack_TopPos = 0;
	for (count = 0; count < 8; count ++ ) {
		RegSet->x86fpu_MappedTo[count] = -1;
		RegSet->x86fpu_State[count] = FPU_Unkown;
		RegSet->x86fpu_RoundingModel[count] = RoundDefault;
	}
	RegSet->Fpu_Used = FALSE;
	RegSet->RoundingModel = RoundUnknown;
}

void _fastcall InheritConstants(BLOCK_SECTION * Section) {
	int NoOfParents, count;
	BLOCK_SECTION * Parent;
	REG_INFO * RegSet;


	if (Section->ParentSection == NULL) {
		InitilizeRegSet(&Section->RegStart);
		memcpy(&Section->RegWorking,&Section->RegStart,sizeof(REG_INFO));		
		return;
	} 

	Parent = Section->ParentSection[0];
	RegSet = Section == Parent->ContinueSection?&Parent->Cont.RegSet:&Parent->Jump.RegSet;
	memcpy(&Section->RegStart,RegSet,sizeof(REG_INFO));		
	memcpy(&Section->RegWorking,&Section->RegStart,sizeof(REG_INFO));		

	for (NoOfParents = 1;Section->ParentSection[NoOfParents] != NULL;NoOfParents++) {
		Parent = Section->ParentSection[NoOfParents];
		RegSet = Section == Parent->ContinueSection?&Parent->Cont.RegSet:&Parent->Jump.RegSet;
			
		for (count = 0; count < 32; count++) {
			if (IsConst(count)) {
				if (MipsRegState(count) != RegSet->MIPS_RegState[count]) {
					MipsRegState(count) = STATE_UNKNOWN;
				} else if (Is32Bit(count) && MipsRegLo(count) != RegSet->MIPS_RegVal[count].UW[0]) {
					MipsRegState(count) = STATE_UNKNOWN;
				} else if (Is64Bit(count) && MipsReg(count) != RegSet->MIPS_RegVal[count].UDW) {
					MipsRegState(count) = STATE_UNKNOWN;
				}
			}
		}
	}
	memcpy(&Section->RegStart,&Section->RegWorking,sizeof(REG_INFO));		
}

BOOL InheritParentInfo (BLOCK_SECTION * Section) {
	int count, start, NoOfParents, NoOfCompiledParents, FirstParent,CurrentParent;
	BLOCK_PARENT * SectionParents;
	BLOCK_SECTION * Parent;
	JUMP_INFO * JumpInfo;
	char Label[100];
	BOOL NeedSync;

	DisplaySectionInformation(Section,Section->SectionID,GetNewTestValue());

	if (Section->ParentSection == NULL) {
		InitilizeRegSet(&Section->RegStart);
		memcpy(&Section->RegWorking,&Section->RegStart,sizeof(REG_INFO));		
		return TRUE;
	} 

	NoOfParents = 0;
	for (count = 0;Section->ParentSection[count] != NULL;count++) {
		Parent = Section->ParentSection[count];
		NoOfParents += Parent->JumpSection != Parent->ContinueSection?1:2;
	}
	
	if (NoOfParents == 0) { 
		if (ShowDebugMessages)
			DisplayError("No Parents ???"); 

		return FALSE;
	} else if (NoOfParents == 1) { 
		Parent = Section->ParentSection[0];
		if (Section == Parent->ContinueSection) { JumpInfo = &Parent->Cont; }
		else if (Section == Parent->JumpSection) { JumpInfo = &Parent->Jump; }
		else { 
			if (ShowDebugMessages)
				DisplayError("How are these sections joined?????"); return FALSE; 
		}

		memcpy(&Section->RegStart,&JumpInfo->RegSet,sizeof(REG_INFO));
		if (JumpInfo->LinkLocation != NULL) {
			CPU_Message("   Section_%d:",Section->SectionID);
			SetJump32(JumpInfo->LinkLocation,RecompPos);
			if (JumpInfo->LinkLocation2 != NULL) { 
				SetJump32(JumpInfo->LinkLocation2,RecompPos);
			}
		}
		memcpy(&Section->RegWorking,&Section->RegStart,sizeof(REG_INFO));
		return TRUE;
	}

	//Multiple Parents
	for (count = 0, NoOfCompiledParents = 0;Section->ParentSection[count] != NULL;count++) {
		Parent = Section->ParentSection[count];
		if (Parent->CompiledLocation != NULL) {
			NoOfCompiledParents += Parent->JumpSection != Parent->ContinueSection?1:2;
		}
	}
	if (NoOfCompiledParents == 0){ DisplayError("No Parent has been compiled ????"); return FALSE; }	
	SectionParents = (BLOCK_PARENT *)malloc(NoOfParents * sizeof(BLOCK_PARENT));
	
	for (count = 0, NoOfCompiledParents = 0;Section->ParentSection[count] != NULL;count++) {
		Parent = Section->ParentSection[count];
		if (Parent->CompiledLocation == NULL) { continue; }
		if (Parent->JumpSection != Parent->ContinueSection) {
			SectionParents[NoOfCompiledParents].Parent = Parent;
			SectionParents[NoOfCompiledParents].JumpInfo = 
				Section == Parent->ContinueSection?&Parent->Cont:&Parent->Jump;
			NoOfCompiledParents += 1;
		} else {
			SectionParents[NoOfCompiledParents].Parent = Parent;
			SectionParents[NoOfCompiledParents].JumpInfo = &Parent->Cont;
			NoOfCompiledParents += 1;
			SectionParents[NoOfCompiledParents].Parent = Parent;
			SectionParents[NoOfCompiledParents].JumpInfo = &Parent->Jump;
			NoOfCompiledParents += 1;
		}
	}

	start = NoOfCompiledParents;
	for (count = 0;Section->ParentSection[count] != NULL;count++) {
		Parent = Section->ParentSection[count];
		if (Parent->CompiledLocation != NULL) { continue; }
		if (Parent->JumpSection != Parent->ContinueSection) {
			SectionParents[start].Parent = Parent;
			SectionParents[start].JumpInfo = 
				Section == Parent->ContinueSection?&Parent->Cont:&Parent->Jump;
			start += 1;
		} else {
			SectionParents[start].Parent = Parent;
			SectionParents[start].JumpInfo = &Parent->Cont;
			start += 1;
			SectionParents[start].Parent = Parent;
			SectionParents[start].JumpInfo = &Parent->Jump;
			start += 1;
		}
	}
	FirstParent = 0;
	for (count = 1;count < NoOfCompiledParents;count++) {
		if (SectionParents[count].JumpInfo->FallThrough) {
			FirstParent = count; break;
		}
	}

	//Link First Parent to start
	Parent = SectionParents[FirstParent].Parent;
	JumpInfo = SectionParents[FirstParent].JumpInfo;

	memcpy(&Section->RegWorking,&JumpInfo->RegSet,sizeof(REG_INFO));
	if (JumpInfo->LinkLocation != NULL) {
		CPU_Message("   Section_%d (from %d):",Section->SectionID,Parent->SectionID);
		SetJump32(JumpInfo->LinkLocation,RecompPos);
		JumpInfo->LinkLocation  = NULL;
		if (JumpInfo->LinkLocation2 != NULL) { 
			SetJump32(JumpInfo->LinkLocation2,RecompPos);
			JumpInfo->LinkLocation2  = NULL;
		}
	}
	if (BlockRandomModifier != 0) { SubConstFromVariable(BlockRandomModifier,&CP0[1],Cop0_Name[1]); }
	if (BlockCycleCount != 0) { 
		AddConstToVariable(BlockCycleCount,&CP0[9],Cop0_Name[9]); 
		SubConstFromVariable(BlockCycleCount,&Timers.Timer,"Timer");
	}
	JumpInfo->FallThrough   = FALSE;

	//Fix up initial state
	WriteBackRegisters(Section);
	UnMap_AllFPRs(Section);
	for (count = 0;count < NoOfParents;count++) {
		int count2, count3, MemoryStackPos;
		REG_INFO * RegSet;

		if (count == FirstParent) { continue; }		
		Parent = SectionParents[count].Parent;
		RegSet = &SectionParents[count].JumpInfo->RegSet;
			
		if (CurrentRoundingModel != RegSet->RoundingModel) { CurrentRoundingModel = RoundUnknown; }
		if (NoOfParents != NoOfCompiledParents) { CurrentRoundingModel = RoundUnknown; }

		//Find Parent MapRegState
		MemoryStackPos = -1;
		for (count2 = 1; count2 < 10; count2++) {
			if (RegSet->x86reg_MappedTo[count2] == Stack_Mapped) {
				MemoryStackPos = count2;
				break;
			}
		}
		if (MemoryStackPos < 0) {
			if (Map_MemoryStack(Section,FALSE) > 0) {
				UnMap_X86reg(Section,Map_MemoryStack(Section,FALSE));
			}
		}

		for (count2 = 1; count2 < 32; count2++) {
			if (Is32BitMapped(count2)) {
				switch (RegSet->MIPS_RegState[count2]) {
				case STATE_MAPPED_64: Map_GPR_64bit(Section,count2,count2); break;
				case STATE_MAPPED_32_ZERO: break;
				case STATE_MAPPED_32_SIGN:
					if (IsUnsigned(count2)) {
						MipsRegState(count2) = STATE_MAPPED_32_SIGN;
					}
					break;
				case STATE_CONST_64: Map_GPR_64bit(Section,count2,count2); break;
				case STATE_CONST_32: 
					if ((RegSet->MIPS_RegVal[count2].W[0] < 0) && IsUnsigned(count2)) {
						MipsRegState(count2) = STATE_MAPPED_32_SIGN;
					}
					break;
				case STATE_UNKNOWN:
					//Map_GPR_32bit(Section,count2,TRUE,count2);
					Map_GPR_64bit(Section,count2,count2); //??
					//UnMap_GPR(Section,count2,TRUE); ??
					break;
				default:
					if (ShowDebugMessages)
						DisplayError("Unknown CPU State(%d) in InheritParentInfo",RegSet->MIPS_RegState[count2]);
				}
			}
			if (IsConst(count2)) {
				if (MipsRegState(count2) != RegSet->MIPS_RegState[count2]) {
					if (Is32Bit(count2)) {
						Map_GPR_32bit(Section,count2,TRUE,count2);
					} else {
						Map_GPR_32bit(Section,count2,TRUE,count2);
					}
				} else if (Is32Bit(count2) && MipsRegLo(count2) != RegSet->MIPS_RegVal[count2].UW[0]) {
					Map_GPR_32bit(Section,count2,TRUE,count2);				
				} else if (Is64Bit(count2) && MipsReg(count2) != RegSet->MIPS_RegVal[count2].UDW) {
					Map_GPR_32bit(Section,count2,TRUE,count2);
				}
			}
			for (count3 = 1; count3 < 10; count3 ++) { x86Protected(count3) = FALSE; }
		}
	}
	memcpy(&Section->RegStart,&Section->RegWorking,sizeof(REG_INFO));

	//Sync registers for different blocks
	sprintf(Label,"Section_%d",Section->SectionID);
	CurrentParent = FirstParent;
	NeedSync = FALSE;
	for (count = 0;count < NoOfCompiledParents;count++) {
		REG_INFO * RegSet;
		int count2;

		if (count == FirstParent) { continue; }		
		Parent    = SectionParents[count].Parent;
		JumpInfo = SectionParents[count].JumpInfo; 
		RegSet   = &SectionParents[count].JumpInfo->RegSet;
	
		if (JumpInfo->RegSet.CycleCount != 0) { NeedSync = TRUE; }
		if (JumpInfo->RegSet.RandomModifier  != 0) { NeedSync = TRUE; }
		
		for (count2 = 0; count2 < 8; count2++) {
			if (FpuMappedTo(count2) == (DWORD)-1) {
				NeedSync = TRUE;
			}
		}

		for (count2 = 1; count2 < 10; count2++) {
			if (x86Mapped(count2) == Stack_Mapped) {
				if (x86Mapped(count2) != RegSet->x86reg_MappedTo[count2]) {
					NeedSync = TRUE;
				}
				break;
			}
		}
		for (count2 = 0; count2 < 32; count2++) {
			if (NeedSync == TRUE)  { break; }
			if (MipsRegState(count2) != RegSet->MIPS_RegState[count2]) {
				NeedSync = TRUE;
				continue;
			}
			switch (MipsRegState(count2)) {
			case STATE_UNKNOWN: break;
			case STATE_MAPPED_64:
				if (MipsReg(count2) != RegSet->MIPS_RegVal[count2].UDW) {
					NeedSync = TRUE;
				}
				break;
			case STATE_MAPPED_32_ZERO:
			case STATE_MAPPED_32_SIGN:
				if (MipsRegLo(count2) != RegSet->MIPS_RegVal[count2].UW[0]) {
					//DisplayError("Parent: %d",Parent->SectionID);
					NeedSync = TRUE;
				}
				break;
			case STATE_CONST_32:
				if (MipsRegLo(count2) != RegSet->MIPS_RegVal[count2].UW[0]) {
					if (ShowDebugMessages)
						DisplayError("Umm.. how ???");

					NeedSync = TRUE;
				}
				break;
			default:
				if (ShowDebugMessages)
					DisplayError("Unhandled Reg state %d\nin InheritParentInfo",MipsRegState(count2));
			}
		}
		if (NeedSync == FALSE) { continue; }
		Parent   = SectionParents[CurrentParent].Parent;
		JumpInfo = SectionParents[CurrentParent].JumpInfo; 
		JmpLabel32(Label,0);		
		JumpInfo->LinkLocation  = RecompPos - 4;
		JumpInfo->LinkLocation2 = NULL;

		CurrentParent = count;		
		Parent   = SectionParents[CurrentParent].Parent;
		JumpInfo = SectionParents[CurrentParent].JumpInfo; 
		CPU_Message("   Section_%d (from %d):",Section->SectionID,Parent->SectionID);
		if (JumpInfo->LinkLocation != NULL) {
			SetJump32(JumpInfo->LinkLocation,RecompPos);
			JumpInfo->LinkLocation = NULL;
			if (JumpInfo->LinkLocation2 != NULL) { 
				SetJump32(JumpInfo->LinkLocation2,RecompPos);
				JumpInfo->LinkLocation2 = NULL;
			}
		}
		memcpy(&Section->RegWorking,&JumpInfo->RegSet,sizeof(REG_INFO));
		if (BlockRandomModifier != 0) { SubConstFromVariable(BlockRandomModifier,&CP0[1],Cop0_Name[1]); }
		if (BlockCycleCount != 0) { 
			AddConstToVariable(BlockCycleCount,&CP0[9],Cop0_Name[9]); 
			SubConstFromVariable(BlockCycleCount,&Timers.Timer,"Timer");
		}
		SyncRegState(Section,&Section->RegStart); 		//Sync				
		memcpy(&Section->RegStart,&Section->RegWorking,sizeof(REG_INFO));

	}

	for (count = 0;count < NoOfCompiledParents;count++) {
		Parent   = SectionParents[count].Parent;
		JumpInfo = SectionParents[count].JumpInfo; 

		if (JumpInfo->LinkLocation != NULL) {
			SetJump32(JumpInfo->LinkLocation,RecompPos);
			JumpInfo->LinkLocation = NULL;
			if (JumpInfo->LinkLocation2 != NULL) { 
				SetJump32(JumpInfo->LinkLocation2,RecompPos);
				JumpInfo->LinkLocation2 = NULL;
			}
		}
	}

	CPU_Message("   Section_%d:",Section->SectionID);
	BlockCycleCount = 0;
	BlockRandomModifier = 0;
	free(SectionParents);
	return TRUE;
}

BOOL IsAllParentLoops(BLOCK_SECTION * Section, BLOCK_SECTION * Parent, BOOL IgnoreIfCompiled, DWORD Test) { 
	int count;

	if (IgnoreIfCompiled && Parent->CompiledLocation != NULL) { return TRUE; }
	if (!Section->InLoop) { return FALSE; }
	if (!Parent->InLoop) { return FALSE; }
	if (Parent->ParentSection == NULL) { return FALSE; }
	if (Section == Parent) { return TRUE; }	
	if (Parent->Test == Test) { return TRUE; }
	Parent->Test = Test;
		
	for (count = 0;Parent->ParentSection[count] != NULL;count++) {
		if (!IsAllParentLoops(Section,Parent->ParentSection[count],IgnoreIfCompiled,Test)) { return FALSE; }
	}
	return TRUE;
}

void _fastcall InitilzeSection (BLOCK_SECTION * Section, BLOCK_SECTION * Parent, DWORD StartAddr, DWORD ID) {
	Section->ParentSection      = NULL;
	Section->JumpSection        = NULL;
	Section->ContinueSection    = NULL;
	Section->CompiledLocation   = NULL;

	Section->SectionID          = ID;
	Section->Test               = 0;
	Section->Test2              = 0;
	Section->InLoop             = FALSE;

	Section->StartPC            = StartAddr;
	Section->CompilePC          = Section->StartPC;

	Section->Jump.BranchLabel   = NULL;
	Section->Jump.LinkLocation  = NULL;
	Section->Jump.LinkLocation2 = NULL;
	Section->Jump.FallThrough   = FALSE;
	Section->Jump.PermLoop      = FALSE;
	Section->Jump.TargetPC      = (DWORD)-1;
	Section->Cont.BranchLabel   = NULL;
	Section->Cont.LinkLocation  = NULL;
	Section->Cont.LinkLocation2 = NULL;
	Section->Cont.FallThrough   = FALSE;
	Section->Cont.PermLoop      = FALSE;
	Section->Cont.TargetPC      = (DWORD)-1;

	AddParent(Section,Parent);
}

void MarkCodeBlock (DWORD PAddr) {
	if (PAddr < RdramSize) {
		N64_Blocks.NoOfRDRamBlocks[PAddr >> 12] += 1;
	} else if (PAddr >= 0x04000000 && PAddr <= 0x04000FFC) {
		N64_Blocks.NoOfDMEMBlocks += 1;
	} else if (PAddr >= 0x04001000 && PAddr <= 0x04001FFC) {
		N64_Blocks.NoOfIMEMBlocks += 1;
	} else if (PAddr >= 0x1FC00000 && PAddr <= 0x1FC00800) {
		N64_Blocks.NoOfPifRomBlocks += 1;
	} else {
		if (ShowDebugMessages)
			DisplayError("Ummm... Which code block should be marked on\nPC = 0x%08X\nRdramSize: %X",PAddr,RdramSize);

		ExitThread(0);
	}
}

void __cdecl StartRecompilerCPU (void ) { 
	DWORD Addr;
	BYTE * Block;

	if (CoInitialize(NULL) != S_OK) {
		return;
	}
#ifdef Log_x86Code
	Start_x86_Log();
#endif

	if (SelfModCheck == ModCode_CheckMemoryCache || SelfModCheck == ModCode_CheckMemory2) {// *** Add in Build 53
		if (TargetInfo == NULL) {
			TargetInfo = VirtualAlloc(NULL,MaxCodeBlocks * sizeof(TARGET_INFO),MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
			if (TargetInfo == NULL) {
				DisplayError(GS(MSG_MEM_ALLOC_ERROR));
				ExitThread(0);
			}
		}
		TargetIndex = 0;
	}
	if (SelfModCheck == ModCode_ChangeMemory) {
		if (OrigMem == NULL) { 
			OrigMem = VirtualAlloc(NULL,MaxOrigMem * sizeof(ORIGINAL_MEMMARKER),MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);
			if (OrigMem == NULL) {
				DisplayError(GS(MSG_MEM_ALLOC_ERROR));
				ExitThread(0);
			}
		}
		TargetIndex = 0;
	} else {
		if (OrigMem != NULL) { 
			VirtualFree(OrigMem,0,MEM_RELEASE); 
			OrigMem = NULL;
		}
	}

	if (AiRomOpen != NULL) { AiRomOpen(); }
	if (GfxRomOpen != NULL) { GfxRomOpen(); }
	if (ContRomOpen != NULL) { ContRomOpen(); }
	if (RSPRomOpen != NULL) { RSPRomOpen(); }
	ResetRecompCode();
	memset(&N64_Blocks,0,sizeof(N64_Blocks));
	NextInstruction = NORMAL;	
	__try {
		if (SelfModCheck == ModCode_ChangeMemory) {
			DWORD Value;

			for (;;) {
				Addr = PROGRAM_COUNTER.UW[0];
				if (UseTlb) {
					if (!TranslateVaddr(&Addr)) {
						DoTLBMiss(NextInstruction == DELAY_SLOT,PROGRAM_COUNTER.W[0], TRUE);
						NextInstruction = NORMAL;
						Addr = PROGRAM_COUNTER.UW[0];
						if (!TranslateVaddr(&Addr)) {
							DisplayError("Failed to tranlate PC to a PAddr: %llX\n\nEmulation stopped",PROGRAM_COUNTER.UDW);
							ExitThread(0);
						}
					}
				} else {
					Addr &= 0x1FFFFFFF;
				}

				if (NextInstruction == DELAY_SLOT) {
					__try {
						Value = (DWORD)(*(DelaySlotTable + (Addr >> 12)));
					} __except(EXCEPTION_EXECUTE_HANDLER) {
						DisplayError("Executing Delay Slot from non maped space\nPROGRAM_COUNTER = 0x%llX\nEmulation stopped",PROGRAM_COUNTER.UDW);
						ExitThread(0);
					}
					if ( (Value >> 16) == 0x7C7C) {
						DWORD Index = (Value & 0xFFFF);
						Block = OrigMem[Index].CompiledLocation;
						if (OrigMem[Index].PAddr != Addr) { Block = NULL; }
						if (OrigMem[Index].VAddr != PROGRAM_COUNTER.UW[0]) { Block = NULL; }
						if (Index >= TargetIndex) { Block = NULL; }
					} else {
						Block = NULL;
					}						
					if (Block == NULL) {
						DWORD MemValue;

						Block = CompileDelaySlot();
						Value = 0x7C7C0000;
						Value += (WORD)(TargetIndex);
						MemValue = *(DWORD *)(N64MEM + Addr);
						if ( (MemValue >> 16) == 0x7C7C) {
							MemValue = OrigMem[(MemValue & 0xFFFF)].OriginalValue;
						}
						OrigMem[(WORD)(TargetIndex)].OriginalValue = MemValue;
						OrigMem[(WORD)(TargetIndex)].CompiledLocation = Block;
						OrigMem[(WORD)(TargetIndex)].PAddr = Addr;
						OrigMem[(WORD)(TargetIndex)].VAddr = PROGRAM_COUNTER.UW[0];
						TargetIndex += 1;
						*(DelaySlotTable + (Addr >> 12)) = (void *)Value;
						NextInstruction = NORMAL;
					}
					_asm {
						pushad
						call Block
						popad
					}
					continue;
				}

				__try {
					Value = *(DWORD *)(N64MEM + Addr);
					if ( (Value >> 16) == 0x7C7C) {
						DWORD Index = (Value & 0xFFFF);
						Block = OrigMem[Index].CompiledLocation;						
						if (OrigMem[Index].PAddr != Addr) { Block = NULL; }
						if (OrigMem[Index].VAddr != PROGRAM_COUNTER.UW[0]) { Block = NULL; }
						if (Index >= TargetIndex) { Block = NULL; }
					} else {
						Block = NULL;
					}
				} __except(EXCEPTION_EXECUTE_HANDLER) {
					DisplayError(GS(MSG_NONMAPPED_SPACE));
					ExitThread(0);
				}
								
				if (Block == NULL) {
					DWORD MemValue;

					__try {
						Block = Compiler4300iBlock();
					} __except(EXCEPTION_EXECUTE_HANDLER) {
						ResetRecompCode();
						Block = Compiler4300iBlock();
					}
					if (TargetIndex == MaxOrigMem) {
						ResetRecompCode();
						continue;
					}
					Value = 0x7C7C0000;
					Value += (WORD)(TargetIndex);
					MemValue = *(DWORD *)(N64MEM + Addr);
					if ( (MemValue >> 16) == 0x7C7C) {
						MemValue = OrigMem[(MemValue & 0xFFFF)].OriginalValue;
					}
					OrigMem[(WORD)(TargetIndex)].OriginalValue = MemValue;
					OrigMem[(WORD)(TargetIndex)].CompiledLocation = Block;
					OrigMem[(WORD)(TargetIndex)].PAddr = Addr;					
					OrigMem[(WORD)(TargetIndex)].VAddr = PROGRAM_COUNTER.UW[0];
					TargetIndex += 1;
					*(DWORD *)(N64MEM + Addr) = Value;					
					NextInstruction = NORMAL;
				}
				if (Profiling && IndividualBlock) {
					static DWORD ProfAddress = 0;

					if ((PROGRAM_COUNTER.UW[0] & ~0xFFF) != ProfAddress) {
						char Label[100];
		
						ProfAddress = PROGRAM_COUNTER.UW[0] & ~0xFFF;
						sprintf(Label,"PC: %X to %X",ProfAddress,ProfAddress+ 0xFFC);
						StartTimer(Label);				
					}
					/*if (PROGRAM_COUNTER >= 0x800DD000 && PROGRAM_COUNTER <= 0x800DDFFC) {
						char Label[100];
						sprintf(Label,"PC: %X   Block: %X",PROGRAM_COUNTER,Block);
						StartTimer(Label);				
					}*/
				} else 	if ((Profiling || ShowCPUPer) && ProfilingLabel[0] == 0) { 
					StartTimer("r4300i Running"); 
				}
				_asm {
					pushad
					call Block
					popad
				}
			} // end for(;;)
		} // end if (SelfModCheck == ModCode_ChangeMemory) {
		for (;;) {
			Addr = PROGRAM_COUNTER.UW[0];
			if (UseTlb) {
				if (!TranslateVaddr(&Addr)) {
					DoTLBMiss(NextInstruction == DELAY_SLOT,PROGRAM_COUNTER.W[0], TRUE);
					NextInstruction = NORMAL;
					Addr = PROGRAM_COUNTER.UW[0];
					if (!TranslateVaddr(&Addr)) {
						DisplayError("Failed to tranlate PC to a PAddr: %llX\n\nEmulation stopped",PROGRAM_COUNTER.UDW);
						ExitThread(0);
					}
				}
			} else {
				Addr &= 0x1FFFFFFF;
			}

			if (NextInstruction == DELAY_SLOT) {
				__try {
					Block = *(DelaySlotTable + (Addr >> 12));
				} __except(EXCEPTION_EXECUTE_HANDLER) {
					DisplayError(GS(MSG_NONMAPPED_SPACE));
					ExitThread(0);
				}
				if (Block == NULL) {
					DWORD OldProtect;
					
					Block = CompileDelaySlot();
					*(DelaySlotTable + (Addr >> 12)) = Block;
					if (SelfModCheck == ModCode_ProtectedMemory) {
						VirtualProtect(N64MEM + Addr, 4, PAGE_READONLY, &OldProtect);
					}
					NextInstruction = NORMAL;
				}
				_asm {
					pushad
					call Block
					popad
				}
				continue;
			}

			__try {
				if (Addr > 0x10000000)
				{
					if (PROGRAM_COUNTER.UW[0] >= 0xB0000000 && PROGRAM_COUNTER.UW[0] < (RomFileSize | 0xB0000000)) {
						while (PROGRAM_COUNTER.UW[0] >= 0xB0000000 && PROGRAM_COUNTER.UW[0] < (RomFileSize | 0xB0000000)) {
							ExecuteInterpreterOpCode();
						}
						continue;
					} else {
						DisplayError(GS(MSG_NONMAPPED_SPACE));
						ExitThread(0);
					}
				}
				Block = *(JumpTable + (Addr >> 2));
			} __except(EXCEPTION_EXECUTE_HANDLER) {
				if (PROGRAM_COUNTER.UW[0] >= 0xB0000000 && PROGRAM_COUNTER.UW[0] < (RomFileSize | 0xB0000000)) {
					while (PROGRAM_COUNTER.UW[0] >= 0xB0000000 && PROGRAM_COUNTER.UW[0] < (RomFileSize | 0xB0000000)) {
						ExecuteInterpreterOpCode();
					}
					continue;
				} else {
					DisplayError(GS(MSG_NONMAPPED_SPACE));
					ExitThread(0);
				}
			}
			
			if (SelfModCheck == ModCode_CheckMemoryCache && Block != NULL) {
				TARGET_INFO * Target = (TARGET_INFO *)Block;
				if (*(QWORD *)(N64MEM+Addr) != Target->OriginalMemory) {
					Block = NULL;
				} else {
					Block = Target->CodeBlock;
				}
			}
			
			if (SelfModCheck == ModCode_CheckMemory2 && Block != NULL) {
				TARGET_INFO * Target = (TARGET_INFO *)Block;
				if (*(QWORD *)(N64MEM+Addr) != Target->OriginalMemory) {
					DWORD Start = (Addr & ~0xFFF) - 0x10000;
					DWORD End   = Start + 0x20000;
					DWORD count;

					if (End < RdramSize) { End = RdramSize; }
					for (count = (Start >> 12); count < (End >> 12); count ++ ) {
						if (N64_Blocks.NoOfRDRamBlocks[count] > 0) {
							N64_Blocks.NoOfRDRamBlocks[count] = 0;		
							memset(JumpTable + (count << 10),0,0x1000);
							*(DelaySlotTable + count) = NULL;
						}			
					}
					//ResetRecompCode();
					Block = NULL;
				} else {
					Block = Target->CodeBlock;
				}
			}

			if (Block == NULL) {
				DWORD OldProtect;
				char Label[100];
				
				if (Profiling) { strncpy(Label, ProfilingLabel, sizeof(Label)); }
				if (Profiling) { StartTimer("Compiling Block"); }	
				__try {
					Block = Compiler4300iBlock();
				} __except(EXCEPTION_EXECUTE_HANDLER) {
					//DisplayError("Reset Recompiler Code %X",RecompPos - RecompCode);
					ResetRecompCode();
					Block = Compiler4300iBlock();
				}
				if (Profiling) { StartTimer(Label); }	
				if (SelfModCheck == ModCode_CheckMemoryCache || SelfModCheck == ModCode_CheckMemory2) {
					TargetInfo[TargetIndex].CodeBlock  = Block;
					TargetInfo[TargetIndex].OriginalMemory = *(QWORD *)(N64MEM+Addr);
					*(JumpTable + (Addr >> 2)) = &TargetInfo[TargetIndex];
					TargetIndex += 1;
					if (TargetIndex == MaxCodeBlocks) {
						ResetRecompCode();
						continue;
					}
				} else {
					*(JumpTable + (Addr >> 2)) = Block;
				}
				if (SelfModCheck == ModCode_ProtectedMemory) {
					VirtualProtect(N64MEM + Addr, 4, PAGE_READONLY, &OldProtect);
				}
				NextInstruction = NORMAL;
			}
			if (Profiling && IndividualBlock) {
				static DWORD ProfAddress = 0;

				if ((PROGRAM_COUNTER.UW[0] & ~0xFFF) != ProfAddress) {
					char Label[100];
	
					ProfAddress = PROGRAM_COUNTER.UW[0] & ~0xFFF;
					sprintf(Label,"PC: %X to %X",ProfAddress,ProfAddress+ 0xFFC);
					StartTimer(Label);				
				}
				/*if (PROGRAM_COUNTER >= 0x800DD000 && PROGRAM_COUNTER <= 0x800DDFFC) {
					char Label[100];
					sprintf(Label,"PC: %X   Block: %X",PROGRAM_COUNTER,Block);
					StartTimer(Label);				
				}*/
			} else 	if ((Profiling || ShowCPUPer) && ProfilingLabel[0] == 0) { 
				StartTimer("r4300i Running"); 
			}

			_asm {
				pushad
				call Block
				popad
			}
		}
	} __except( r4300i_CPU_MemoryFilter( GetExceptionCode(), GetExceptionInformation()) ) {
		DisplayError(GS(MSG_UNKNOWN_MEM_ACTION));
		ExitThread(0);
	}
}

void SyncRegState (BLOCK_SECTION * Section, REG_INFO * SyncTo) {
	int count, x86Reg,x86RegHi, changed;
	
	changed = FALSE;
	UnMap_AllFPRs(Section);
	if (CurrentRoundingModel != SyncTo->RoundingModel) { CurrentRoundingModel = RoundUnknown; }
	x86Reg = Map_MemoryStack(Section, FALSE);
	//CPU_Message("MemoryStack for Original State = %s",x86Reg > 0?x86_Name(x86Reg):"Not Mapped");

	for (x86Reg = 1; x86Reg < 10; x86Reg ++) {
		if (x86Mapped(x86Reg) != Stack_Mapped) { continue; }
		if (SyncTo->x86reg_MappedTo[x86Reg] != Stack_Mapped) {
			UnMap_X86reg(Section,x86Reg);
			for (count = 1; count < 10; count ++) {
				if (SyncTo->x86reg_MappedTo[count] == Stack_Mapped) {
					MoveX86RegToX86Reg(count,x86Reg); 
					changed = TRUE;
				}
			}
			if (!changed) {
				MoveVariableToX86reg(&MemoryStack,"MemoryStack",x86Reg);
			}
			changed = TRUE;
		}
	}
	for (x86Reg = 1; x86Reg < 10; x86Reg ++) {
		if (SyncTo->x86reg_MappedTo[x86Reg] != Stack_Mapped) { continue; }
		//CPU_Message("MemoryStack for Sync State = %s",x86Reg > 0?x86_Name(x86Reg):"Not Mapped");
		if (x86Mapped(x86Reg) == Stack_Mapped) { break; }
		UnMap_X86reg(Section,x86Reg);		
	}
	
	for (count = 1; count < 32; count ++) {
		if (MipsRegState(count) == SyncTo->MIPS_RegState[count]) {
			switch (MipsRegState(count)) {
			case STATE_UNKNOWN: continue;
			case STATE_MAPPED_64:
				if (MipsReg(count) == SyncTo->MIPS_RegVal[count].UDW) {
					continue;
				}
				break;
			case STATE_MAPPED_32_ZERO:
			case STATE_MAPPED_32_SIGN:
				if (MipsRegLo(count) == SyncTo->MIPS_RegVal[count].UW[0]) {
					continue;
				}
				break;
			case STATE_CONST_64:
				if (MipsReg(count) != SyncTo->MIPS_RegVal[count].UDW) {
					if (ShowDebugMessages)
						DisplayError("Umm.. how ???");
				}
				continue;
			case STATE_CONST_32:
				if (MipsRegLo(count) != SyncTo->MIPS_RegVal[count].UW[0]) {
					if (ShowDebugMessages)
						DisplayError("Umm.. how ???");
				}
				continue;
			default:
				if (ShowDebugMessages)
					DisplayError("Unhandled Reg state %d\nin SyncRegState",MipsRegState(count));
			}			
		}
		changed = TRUE;

		switch (SyncTo->MIPS_RegState[count]) {
		case STATE_UNKNOWN: UnMap_GPR(Section, count, TRUE);  break;
		case STATE_MAPPED_64:
			x86Reg = SyncTo->MIPS_RegVal[count].UW[0];
			x86RegHi = SyncTo->MIPS_RegVal[count].UW[1];
			UnMap_X86reg(Section, x86Reg);
			UnMap_X86reg(Section, x86RegHi);
			switch (MipsRegState(count)) {
			case STATE_UNKNOWN:
				MoveVariableToX86reg(&GPR[count].UW[0], GPR_NameLo[count], x86Reg);
				MoveVariableToX86reg(&GPR[count].UW[1], GPR_NameHi[count], x86RegHi);
				break;
			case STATE_MAPPED_64:
				MoveX86RegToX86Reg(MipsRegLo(count), x86Reg);
				x86Mapped(MipsRegLo(count)) = NotMapped;
				MoveX86RegToX86Reg(MipsRegHi(count), x86RegHi);
				x86Mapped(MipsRegHi(count)) = NotMapped;
				break;
			case STATE_MAPPED_32_SIGN:
				MoveX86RegToX86Reg(MipsRegLo(count), x86RegHi);
				ShiftRightSignImmed(x86RegHi, 31);
				MoveX86RegToX86Reg(MipsRegLo(count), x86Reg);
				x86Mapped(MipsRegLo(count)) = NotMapped;
				break;
			case STATE_MAPPED_32_ZERO:
				XorX86RegToX86Reg(x86RegHi, x86RegHi);
				MoveX86RegToX86Reg(MipsRegLo(count), x86Reg);
				x86Mapped(MipsRegLo(count)) = NotMapped;
				break;
			case STATE_CONST_64:
				MoveConstToX86reg(MipsRegHi(count), x86RegHi);
				MoveConstToX86reg(MipsRegLo(count), x86Reg);
				break;
			case STATE_CONST_32:
				MoveConstToX86reg(MipsRegLo_S(count) >> 31, x86RegHi);
				MoveConstToX86reg(MipsRegLo(count), x86Reg);
				break;
			default:
				if (ShowDebugMessages) {
					CPU_Message("Do something with states in SyncRegState\nSTATE_MAPPED_64\n%d", MipsRegState(count));
					DisplayError("Do something with states in SyncRegState\nSTATE_MAPPED_64\n%d", MipsRegState(count));
				}
				continue;
			}
			MipsRegLo(count) = x86Reg;
			MipsRegHi(count) = x86RegHi;
			MipsRegState(count) = STATE_MAPPED_64;
			x86Mapped(x86Reg) = GPR_Mapped;
			x86Mapped(x86RegHi) = GPR_Mapped;
			x86MapOrder(x86Reg) = 1;
			x86MapOrder(x86RegHi) = 1;
			break;
		case STATE_MAPPED_32_SIGN:
			x86Reg = SyncTo->MIPS_RegVal[count].UW[0];
			UnMap_X86reg(Section, x86Reg);
			switch (MipsRegState(count)) {
			case STATE_UNKNOWN: MoveVariableToX86reg(&GPR[count].UW[0], GPR_NameLo[count], x86Reg); break;
			case STATE_CONST_32: MoveConstToX86reg(MipsRegLo(count), x86Reg); break;
			case STATE_MAPPED_32_SIGN:
				MoveX86RegToX86Reg(MipsRegLo(count), x86Reg);
				x86Mapped(MipsRegLo(count)) = NotMapped;
				break;
			case STATE_MAPPED_32_ZERO:
				if (MipsRegLo(count) != (DWORD)x86Reg) {
					MoveX86RegToX86Reg(MipsRegLo(count), x86Reg);
					x86Mapped(MipsRegLo(count)) = NotMapped;
				}
				break;
			case STATE_MAPPED_64:
				MoveX86RegToX86Reg(MipsRegLo(count), x86Reg);
				x86Mapped(MipsRegLo(count)) = NotMapped;
				x86Mapped(MipsRegHi(count)) = NotMapped;
				break;
			case STATE_CONST_64:
				if (ShowDebugMessages)
					DisplayError("hi %X\nLo %X", MipsRegHi(count), MipsRegLo(count));
			default:
				if (ShowDebugMessages) {
					CPU_Message("Do something with states in SyncRegState\nSTATE_MAPPED_32_SIGN\n%d", MipsRegState(count));
					DisplayError("Do something with states in SyncRegState\nSTATE_MAPPED_32_SIGN\n%d", MipsRegState(count));
				}
			}
			MipsRegLo(count) = x86Reg;
			MipsRegState(count) = STATE_MAPPED_32_SIGN;
			x86Mapped(x86Reg) = GPR_Mapped;
			x86MapOrder(x86Reg) = 1;
			break;
		case STATE_MAPPED_32_ZERO:
			x86Reg = SyncTo->MIPS_RegVal[count].UW[0];
			UnMap_X86reg(Section, x86Reg);
			switch (MipsRegState(count)) {
			case STATE_MAPPED_64:
			case STATE_UNKNOWN:
				MoveVariableToX86reg(&GPR[count].UW[0], GPR_NameLo[count], x86Reg);
				break;
			case STATE_MAPPED_32_ZERO:
				MoveX86RegToX86Reg(MipsRegLo(count), x86Reg);
				x86Mapped(MipsRegLo(count)) = NotMapped;
				break;
			case STATE_CONST_32:
				if (MipsRegLo_S(count) < 0) {
					CPU_Message("Sign Problems in SyncRegState\nSTATE_MAPPED_32_ZERO");
					CPU_Message("%s: %X", GPR_Name[count], MipsRegLo_S(count));
					if (ShowDebugMessages)
						DisplayError("Sign Problems in SyncRegState\nSTATE_MAPPED_32_ZERO");
				}
				MoveConstToX86reg(MipsRegLo(count), x86Reg);
				break;
			default:
				if (ShowDebugMessages) {
					CPU_Message("Do something with states in SyncRegState\nSTATE_MAPPED_32_ZERO\n%d", MipsRegState(count));
					DisplayError("Do something with states in SyncRegState\nSTATE_MAPPED_32_ZERO\n%d", MipsRegState(count));
				}
			}
			MipsRegLo(count) = x86Reg;
			MipsRegState(count) = SyncTo->MIPS_RegState[count];
			x86Mapped(x86Reg) = GPR_Mapped;
			x86MapOrder(x86Reg) = 1;
			break;
		default:
			if (ShowDebugMessages) {
				CPU_Message("%d\n%d\nreg: %s (%d)", SyncTo->MIPS_RegState[count], MipsRegState(count), GPR_Name[count], count);
				DisplayError("%d\n%d\nreg: %s (%d)", SyncTo->MIPS_RegState[count], MipsRegState(count), GPR_Name[count], count);
				DisplayError("Do something with states in SyncRegState");
		}
			changed = FALSE;
		}
	}
}
