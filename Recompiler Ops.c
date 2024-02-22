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

DWORD BranchCompare = 0;

void CompileReadTLBMiss (BLOCK_SECTION * Section, int AddressReg, int LookUpReg ) {
	MoveX86regToVariable(AddressReg,&TLBLoadAddress,"TLBLoadAddress");
	TestX86RegToX86Reg(LookUpReg,LookUpReg);
	CompileExit(Section->CompilePC,Section->RegWorking,TLBReadMiss,FALSE,JeLabel32);
}

void CompileWriteTLBMiss(BLOCK_SECTION* Section, int AddressReg, int LookUpReg) {
	MoveX86regToVariable(AddressReg, &TLBLoadAddress, "TLBLoadAddress");
	TestX86RegToX86Reg(LookUpReg, LookUpReg);
	CompileExit(Section->CompilePC, Section->RegWorking, TLBWriteMiss, FALSE, JeLabel32);
}

/************************** Branch functions  ************************/
void Compile_R4300i_Branch (BLOCK_SECTION * Section, void (*CompareFunc)(BLOCK_SECTION * Section), int BranchType, BOOL Link) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s", Section->CompilePC, R4300iOpcodeName(Opcode.Hex, CompilePC));

	static int EffectDelaySlot, DoneJumpDelay, DoneContinueDelay;
	static char ContLabel[100], JumpLabel[100];
	static REG_INFO RegBeforeDelay;
	int count;

	if ( NextInstruction == NORMAL ) {
		CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
		
		if ((Section->CompilePC & 0xFFC) != 0xFFC) {
			MIPS_DWORD CompilePC;
			CompilePC.DW = (int)Section->CompilePC;
			switch (BranchType) {
			case BranchTypeRs: EffectDelaySlot = DelaySlotEffectsCompare(CompilePC,Opcode.BRANCH.rs,0); break;
			case BranchTypeRsRt: EffectDelaySlot = DelaySlotEffectsCompare(CompilePC,Opcode.BRANCH.rs,Opcode.BRANCH.rt); break;
			case BranchTypeCop1: 
				{
					OPCODE Command;
					MIPS_DWORD Address;
					Address.DW = (long)(Section->CompilePC + 4);

					if (!r4300i_LW_VAddr_NonCPU(Address, &Command.Hex)) {
						DisplayError(GS(MSG_FAIL_LOAD_WORD));
						ExitThread(0);
					}
					
					EffectDelaySlot = FALSE;
					if (Command.BRANCH.op == R4300i_CP1) {
						if (Command.FP.fmt == R4300i_COP1_S && (Command.REG.funct & 0x30) == 0x30 ) {
							EffectDelaySlot = TRUE;
						} 
						if (Command.FP.fmt == R4300i_COP1_D && (Command.REG.funct & 0x30) == 0x30 ) {
							EffectDelaySlot = TRUE;
						} 
					}
				}
				break;
			default:
				if (ShowDebugMessages)
					DisplayError("Unknown branch type");
			}
		} else {
			EffectDelaySlot = TRUE;
		}
		if (Section->ContinueSection != NULL) {
			sprintf(ContLabel,"Section_%d",((BLOCK_SECTION *)Section->ContinueSection)->SectionID);
		} else {
			strcpy(ContLabel,"Cont.LinkLocationinue");
		}
		if (Section->JumpSection != NULL) {
			sprintf(JumpLabel,"Section_%d",((BLOCK_SECTION *)Section->JumpSection)->SectionID);
		} else {
			strcpy(JumpLabel,"Jump.LinkLocation");
		}
		Section->Jump.TargetPC        = Section->CompilePC + ((short)Opcode.BRANCH.offset << 2) + 4;
		Section->Jump.BranchLabel     = JumpLabel;
		Section->Jump.LinkLocation    = NULL;
		Section->Jump.LinkLocation2   = NULL;
		Section->Jump.DoneDelaySlot   = FALSE;
		Section->Cont.TargetPC        = Section->CompilePC + 8;
		Section->Cont.BranchLabel     = ContLabel;
		Section->Cont.LinkLocation    = NULL;
		Section->Cont.LinkLocation2   = NULL;
		Section->Cont.DoneDelaySlot   = FALSE;
		if (Section->Jump.TargetPC < Section->Cont.TargetPC) {
			Section->Cont.FallThrough = FALSE;
			Section->Jump.FallThrough = TRUE;
		} else {
			Section->Cont.FallThrough = TRUE;
			Section->Jump.FallThrough = FALSE;
		}
		if (Link) {
			UnMap_GPR(Section, 31, FALSE);
			MipsRegLo(31) = Section->CompilePC + 8;
			MipsRegState(31) = STATE_CONST_32;
		}
		if (EffectDelaySlot) {
			if (Section->ContinueSection != NULL) {
				sprintf(ContLabel,"Continue %d",((BLOCK_SECTION *)Section->ContinueSection)->SectionID);
			} else {
				strcpy(ContLabel,"ExitBlock");
			}
			if (Section->JumpSection != NULL) {
				sprintf(JumpLabel,"Jump %d",((BLOCK_SECTION *)Section->JumpSection)->SectionID);
			} else {
				strcpy(JumpLabel,"ExitBlock");
			}
			CompareFunc(Section); 
			
			if ((Section->CompilePC & 0xFFC) == 0xFFC) {
				GenerateSectionLinkage(Section);
				NextInstruction = END_BLOCK;
				return;
			}
			if (!Section->Jump.FallThrough && !Section->Cont.FallThrough) {
				if (Section->Jump.LinkLocation != NULL) {
					CPU_Message("");
					CPU_Message("      %s:",Section->Jump.BranchLabel);
					SetJump32((DWORD *)Section->Jump.LinkLocation,(DWORD *)RecompPos);
					Section->Jump.LinkLocation = NULL;
					if (Section->Jump.LinkLocation2 != NULL) {
						SetJump32((DWORD *)Section->Jump.LinkLocation2,(DWORD *)RecompPos);
						Section->Jump.LinkLocation2 = NULL;
					}
					Section->Jump.FallThrough = TRUE;
				} else if (Section->Cont.LinkLocation != NULL){
					CPU_Message("");
					CPU_Message("      %s:",Section->Cont.BranchLabel);
					SetJump32((DWORD *)Section->Cont.LinkLocation,(DWORD *)RecompPos);
					Section->Cont.LinkLocation = NULL;
					if (Section->Cont.LinkLocation2 != NULL) {
						SetJump32((DWORD *)Section->Cont.LinkLocation2,(DWORD *)RecompPos);
						Section->Cont.LinkLocation2 = NULL;
					}
					Section->Cont.FallThrough = TRUE;
				}
			}
			for (count = 1; count < 10; count ++) { x86Protected(count) = FALSE; }
			memcpy(&RegBeforeDelay,&Section->RegWorking,sizeof(REG_INFO));
		}
		NextInstruction = DO_DELAY_SLOT;
	} else if (NextInstruction == DELAY_SLOT_DONE ) {		
		if (EffectDelaySlot) { 
			JUMP_INFO * FallInfo = Section->Jump.FallThrough?&Section->Jump:&Section->Cont;
			JUMP_INFO * JumpInfo = Section->Jump.FallThrough?&Section->Cont:&Section->Jump;

			if (FallInfo->FallThrough && !FallInfo->DoneDelaySlot) {
				if (FallInfo == &Section->Jump) {
					if (Section->JumpSection != NULL) {
						sprintf(JumpLabel,"Section_%d",((BLOCK_SECTION *)Section->JumpSection)->SectionID);
					} else {
						strcpy(JumpLabel,"ExitBlock");
					}
				} else {
					if (Section->ContinueSection != NULL) {
						sprintf(ContLabel,"Section_%d",((BLOCK_SECTION *)Section->ContinueSection)->SectionID);
					} else {
						strcpy(ContLabel,"ExitBlock");
					}
				}
				for (count = 1; count < 10; count ++) { x86Protected(count) = FALSE; }
				memcpy(&FallInfo->RegSet,&Section->RegWorking,sizeof(REG_INFO));
				FallInfo->DoneDelaySlot = TRUE;
				if (!JumpInfo->DoneDelaySlot) {
					FallInfo->FallThrough = FALSE;				
					JmpLabel32(FallInfo->BranchLabel,0);
					FallInfo->LinkLocation = RecompPos - 4;
					
					if (JumpInfo->LinkLocation != NULL) {
						CPU_Message("      %s:",JumpInfo->BranchLabel);
						SetJump32((DWORD *)JumpInfo->LinkLocation,(DWORD *)RecompPos);
						JumpInfo->LinkLocation = NULL;
						if (JumpInfo->LinkLocation2 != NULL) {
							SetJump32((DWORD *)JumpInfo->LinkLocation2,(DWORD *)RecompPos);
							JumpInfo->LinkLocation2 = NULL;
						}
						JumpInfo->FallThrough = TRUE;
						NextInstruction = DO_DELAY_SLOT;
						memcpy(&Section->RegWorking,&RegBeforeDelay,sizeof(REG_INFO));
						return; 
					}
				}
			}
		} else {
			int count;

			CompareFunc(Section); 
			for (count = 1; count < 10; count ++) { x86Protected(count) = FALSE; }
			memcpy(&Section->Cont.RegSet,&Section->RegWorking,sizeof(REG_INFO));
			memcpy(&Section->Jump.RegSet,&Section->RegWorking,sizeof(REG_INFO));
		}
		GenerateSectionLinkage(Section);
		NextInstruction = END_BLOCK;
	} else {
		if (ShowDebugMessages)
			DisplayError("WTF\n\nBranch\nNextInstruction = %X", NextInstruction);
	}
}

void Compile_R4300i_BranchLikely (BLOCK_SECTION * Section, void (*CompareFunc)(BLOCK_SECTION * Section), BOOL Link) {
	static char ContLabel[100], JumpLabel[100];
	int count;
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s", Section->CompilePC, R4300iOpcodeName(Opcode.Hex, CompilePC));

	if ( NextInstruction == NORMAL ) {		
		CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
		
		if (Section->ContinueSection != NULL) {
			sprintf(ContLabel,"Section_%d",((BLOCK_SECTION *)Section->ContinueSection)->SectionID);
		} else {
			strcpy(ContLabel,"ExitBlock");
		}
		if (Section->JumpSection != NULL) {
			sprintf(JumpLabel,"Section_%d",((BLOCK_SECTION *)Section->JumpSection)->SectionID);
		} else {
			strcpy(JumpLabel,"ExitBlock");
		}
		Section->Jump.TargetPC      = Section->CompilePC + ((short)Opcode.BRANCH.offset << 2) + 4;
		Section->Jump.BranchLabel   = JumpLabel;
		Section->Jump.FallThrough   = TRUE;
		Section->Jump.LinkLocation  = NULL;
		Section->Jump.LinkLocation2 = NULL;
		Section->Cont.TargetPC      = Section->CompilePC + 8;
		Section->Cont.BranchLabel   = ContLabel;
		Section->Cont.FallThrough   = FALSE;
		Section->Cont.LinkLocation  = NULL;
		Section->Cont.LinkLocation2 = NULL;
		if (Link) {
			UnMap_GPR(Section, 31, FALSE);
			MipsRegLo(31) = Section->CompilePC + 8;
			MipsRegState(31) = STATE_CONST_32;
		}
		CompareFunc(Section); 
		for (count = 1; count < 10; count ++) { x86Protected(count) = FALSE; }
		memcpy(&Section->Cont.RegSet,&Section->RegWorking,sizeof(REG_INFO));
	
		if (Section->Cont.FallThrough)  {
			if (Section->Jump.LinkLocation != NULL) {
				if (ShowDebugMessages)
					DisplayError("WTF .. problem with Compile_R4300i_BranchLikely");
			}
			GenerateSectionLinkage(Section);
			NextInstruction = END_BLOCK;
		} else {
			if ((Section->CompilePC & 0xFFC) == 0xFFC) {
				Section->Jump.FallThrough = FALSE;
				if (Section->Jump.LinkLocation != NULL) {
					SetJump32(Section->Jump.LinkLocation,RecompPos);
					Section->Jump.LinkLocation = NULL;
					if (Section->Jump.LinkLocation2 != NULL) { 
						SetJump32(Section->Jump.LinkLocation2,RecompPos);
						Section->Jump.LinkLocation2 = NULL;
					}
				}
				JmpLabel32("DoDelaySlot",0);
				Section->Jump.LinkLocation = RecompPos - 4;
				CPU_Message("      ");
				CPU_Message("      %s:",Section->Cont.BranchLabel);
				if (Section->Cont.LinkLocation != NULL) {
					SetJump32(Section->Cont.LinkLocation,RecompPos);
					Section->Cont.LinkLocation = NULL;
					if (Section->Cont.LinkLocation2 != NULL) { 
						SetJump32(Section->Cont.LinkLocation2,RecompPos);
						Section->Cont.LinkLocation2 = NULL;
					}
				}
				CompileExit (Section->CompilePC + 8,Section->RegWorking,Normal,TRUE,NULL);
				CPU_Message("      ");
				CPU_Message("      DoDelaySlot");
				GenerateSectionLinkage(Section);
				NextInstruction = END_BLOCK;
			} else {
				NextInstruction = DO_DELAY_SLOT;
			}
		}
	} else if (NextInstruction == DELAY_SLOT_DONE ) {		
		for (count = 1; count < 10; count ++) { x86Protected(count) = FALSE; }
		memcpy(&Section->Jump.RegSet,&Section->RegWorking,sizeof(REG_INFO));
		GenerateSectionLinkage(Section);
		NextInstruction = END_BLOCK;
	} else {
		if (ShowDebugMessages)
			DisplayError("WTF\n\nBranchLikely\nNextInstruction = %X", NextInstruction);
	}
}

void BNE_Compare (BLOCK_SECTION * Section) {
	BYTE *Jump = 0x0;

	if (IsKnown(Opcode.BRANCH.rs) && IsKnown(Opcode.BRANCH.rt)) {
		if (IsConst(Opcode.BRANCH.rs) && IsConst(Opcode.BRANCH.rt)) {
			if (Is64Bit(Opcode.BRANCH.rs) || Is64Bit(Opcode.BRANCH.rt)) {
				Compile_R4300i_UnknownOpcode(Section);
			} else if (MipsRegLo(Opcode.BRANCH.rs) != MipsRegLo(Opcode.BRANCH.rt)) {
				Section->Jump.FallThrough = TRUE;
				Section->Cont.FallThrough = FALSE;
			} else {
				Section->Jump.FallThrough = FALSE;
				Section->Cont.FallThrough = TRUE;
			}
		} else if (IsMapped(Opcode.BRANCH.rs) && IsMapped(Opcode.BRANCH.rt)) {
			if (Is64Bit(Opcode.BRANCH.rs) || Is64Bit(Opcode.BRANCH.rt)) {
				ProtectGPR(Section,Opcode.BRANCH.rs);
				ProtectGPR(Section,Opcode.BRANCH.rt);

				CompX86RegToX86Reg(
					Is32Bit(Opcode.BRANCH.rs)?Map_TempReg(Section,x86_Any,Opcode.BRANCH.rs,TRUE):MipsRegHi(Opcode.BRANCH.rs),
					Is32Bit(Opcode.BRANCH.rt)?Map_TempReg(Section,x86_Any,Opcode.BRANCH.rt,TRUE):MipsRegHi(Opcode.BRANCH.rt)
				);
					
				if (Section->Jump.FallThrough) {
					JneLabel8("continue",0);
					Jump = RecompPos - 1;
				} else {
					JneLabel32(Section->Jump.BranchLabel,0);
					Section->Jump.LinkLocation = RecompPos - 4;
				}
				CompX86RegToX86Reg(MipsRegLo(Opcode.BRANCH.rs),MipsRegLo(Opcode.BRANCH.rt));
				if (Section->Cont.FallThrough) {
					JneLabel32 ( Section->Jump.BranchLabel, 0 );
					Section->Jump.LinkLocation2 = RecompPos - 4;
				} else if (Section->Jump.FallThrough) {
					JeLabel32 ( Section->Cont.BranchLabel, 0 );
					Section->Cont.LinkLocation = RecompPos - 4;
					CPU_Message("      ");
					CPU_Message("      continue:");
					SetJump8(Jump,RecompPos);
				} else {
					JeLabel32 ( Section->Cont.BranchLabel, 0 );
					Section->Cont.LinkLocation = RecompPos - 4;
					JmpLabel32(Section->Jump.BranchLabel,0);
					Section->Jump.LinkLocation2 = RecompPos - 4;
				}
			} else {
				CompX86RegToX86Reg(MipsRegLo(Opcode.BRANCH.rs),MipsRegLo(Opcode.BRANCH.rt));
				if (Section->Cont.FallThrough) {
					JneLabel32 ( Section->Jump.BranchLabel, 0 );
					Section->Jump.LinkLocation = RecompPos - 4;
				} else if (Section->Jump.FallThrough) {
					JeLabel32 ( Section->Cont.BranchLabel, 0 );
					Section->Cont.LinkLocation = RecompPos - 4;
				} else {
					JeLabel32 ( Section->Cont.BranchLabel, 0 );
					Section->Cont.LinkLocation = RecompPos - 4;
					JmpLabel32(Section->Jump.BranchLabel,0);
					Section->Jump.LinkLocation = RecompPos - 4;
				}
			}
		} else {
			DWORD ConstReg = IsConst(Opcode.BRANCH.rt)?Opcode.BRANCH.rt:Opcode.BRANCH.rs;
			DWORD MappedReg = IsConst(Opcode.BRANCH.rt)?Opcode.BRANCH.rs:Opcode.BRANCH.rt;

			if (Is64Bit(ConstReg) || Is64Bit(MappedReg)) {
				if (Is32Bit(ConstReg) || Is32Bit(MappedReg)) {
					ProtectGPR(Section,MappedReg);
					if (Is32Bit(MappedReg)) {
						CompConstToX86reg(Map_TempReg(Section,x86_Any,MappedReg,TRUE),MipsRegHi(ConstReg));
					} else {
						CompConstToX86reg(MipsRegHi(MappedReg),(int)MipsRegLo(ConstReg) >> 31);
					}
				} else {
					CompConstToX86reg(MipsRegHi(MappedReg),MipsRegHi(ConstReg));
				}
				if (Section->Jump.FallThrough) {
					JneLabel8("continue",0);
					Jump = RecompPos - 1;
				} else {
					JneLabel32(Section->Jump.BranchLabel,0);
					Section->Jump.LinkLocation = RecompPos - 4;
				}
				if (MipsRegLo(ConstReg) == 0) {
					OrX86RegToX86Reg(MipsRegLo(MappedReg),MipsRegLo(MappedReg));
				} else {
					CompConstToX86reg(MipsRegLo(MappedReg),MipsRegLo(ConstReg));
				}
				if (Section->Cont.FallThrough) {
					JneLabel32 ( Section->Jump.BranchLabel, 0 );
					Section->Jump.LinkLocation2 = RecompPos - 4;
				} else if (Section->Jump.FallThrough) {
					JeLabel32 ( Section->Cont.BranchLabel, 0 );
					Section->Cont.LinkLocation = RecompPos - 4;
					CPU_Message("      ");
					CPU_Message("      continue:");
					SetJump8(Jump,RecompPos);
				} else {
					JeLabel32 ( Section->Cont.BranchLabel, 0 );
					Section->Cont.LinkLocation = RecompPos - 4;
					JmpLabel32(Section->Jump.BranchLabel,0);
					Section->Jump.LinkLocation2 = RecompPos - 4;
				}
			} else {
				if (MipsRegLo(ConstReg) == 0) {
					OrX86RegToX86Reg(MipsRegLo(MappedReg),MipsRegLo(MappedReg));
				} else {
					CompConstToX86reg(MipsRegLo(MappedReg),MipsRegLo(ConstReg));
				}
				if (Section->Cont.FallThrough) {
					JneLabel32 ( Section->Jump.BranchLabel, 0 );
					Section->Jump.LinkLocation = RecompPos - 4;
				} else if (Section->Jump.FallThrough) {
					JeLabel32 ( Section->Cont.BranchLabel, 0 );
					Section->Cont.LinkLocation = RecompPos - 4;
				} else {
					JeLabel32 ( Section->Cont.BranchLabel, 0 );
					Section->Cont.LinkLocation = RecompPos - 4;
					JmpLabel32(Section->Jump.BranchLabel,0);
					Section->Jump.LinkLocation = RecompPos - 4;
				}
			}
		}
	} else if (IsKnown(Opcode.BRANCH.rs) || IsKnown(Opcode.BRANCH.rt)) {
		DWORD KnownReg = IsKnown(Opcode.BRANCH.rt)?Opcode.BRANCH.rt:Opcode.BRANCH.rs;
		DWORD UnknownReg = IsKnown(Opcode.BRANCH.rt)?Opcode.BRANCH.rs:Opcode.BRANCH.rt;

		if (IsConst(KnownReg)) {
			if (Is64Bit(KnownReg)) {
				CompConstToVariable(MipsRegHi(KnownReg),&GPR[UnknownReg].W[1],GPR_NameHi[UnknownReg]);
			} else if (IsSigned(KnownReg)) {
				CompConstToVariable(((int)MipsRegLo(KnownReg) >> 31),&GPR[UnknownReg].W[1],GPR_NameHi[UnknownReg]);
			} else {
				CompConstToVariable(0,&GPR[UnknownReg].W[1],GPR_NameHi[UnknownReg]);
			}
		} else {
			if (Is64Bit(KnownReg)) {
				CompX86regToVariable(MipsRegHi(KnownReg),&GPR[UnknownReg].W[1],GPR_NameHi[UnknownReg]);
			} else if (IsSigned(KnownReg)) {
				ProtectGPR(Section,KnownReg);
				CompX86regToVariable(Map_TempReg(Section,x86_Any,KnownReg,TRUE),&GPR[UnknownReg].W[1],GPR_NameHi[UnknownReg]);
			} else {
				CompConstToVariable(0,&GPR[UnknownReg].W[1],GPR_NameHi[UnknownReg]);
			}
		}
		if (Section->Jump.FallThrough) {
			JneLabel8("continue",0);
			Jump = RecompPos - 1;
		} else {
			JneLabel32(Section->Jump.BranchLabel,0);
			Section->Jump.LinkLocation = RecompPos - 4;
		}
		if (IsConst(KnownReg)) {
			CompConstToVariable(MipsRegLo(KnownReg),&GPR[UnknownReg].W[0],GPR_NameLo[UnknownReg]);
		} else {
			CompX86regToVariable(MipsRegLo(KnownReg),&GPR[UnknownReg].W[0],GPR_NameLo[UnknownReg]);
		}
		if (Section->Cont.FallThrough) {
			JneLabel32 ( Section->Jump.BranchLabel, 0 );
			Section->Jump.LinkLocation2 = RecompPos - 4;
		} else if (Section->Jump.FallThrough) {
			JeLabel32 ( Section->Cont.BranchLabel, 0 );
			Section->Cont.LinkLocation = RecompPos - 4;
			CPU_Message("      ");
			CPU_Message("      continue:");
			SetJump8(Jump,RecompPos);
		} else {
			JeLabel32 ( Section->Cont.BranchLabel, 0 );
			Section->Cont.LinkLocation = RecompPos - 4;
			JmpLabel32(Section->Jump.BranchLabel,0);
			Section->Jump.LinkLocation2 = RecompPos - 4;
		}
	} else {
		int x86Reg;

		x86Reg = Map_TempReg(Section,x86_Any,Opcode.BRANCH.rt,TRUE);		
		CompX86regToVariable(x86Reg,&GPR[Opcode.BRANCH.rs].W[1],GPR_NameHi[Opcode.BRANCH.rs]);
		if (Section->Jump.FallThrough) {
			JneLabel8("continue",0);
			Jump = RecompPos - 1;
		} else {
			JneLabel32(Section->Jump.BranchLabel,0);
			Section->Jump.LinkLocation = RecompPos - 4;
		}

		x86Reg = Map_TempReg(Section,x86Reg,Opcode.BRANCH.rt,FALSE);
		CompX86regToVariable(x86Reg,&GPR[Opcode.BRANCH.rs].W[0],GPR_NameLo[Opcode.BRANCH.rs]);
		if (Section->Cont.FallThrough) {
			JneLabel32 ( Section->Jump.BranchLabel, 0 );
			Section->Jump.LinkLocation2 = RecompPos - 4;
		} else if (Section->Jump.FallThrough) {
			JeLabel32 ( Section->Cont.BranchLabel, 0 );
			Section->Cont.LinkLocation = RecompPos - 4;
			CPU_Message("      ");
			CPU_Message("      continue:");
			SetJump8(Jump,RecompPos);
		} else {
			JeLabel32 ( Section->Cont.BranchLabel, 0 );
			Section->Cont.LinkLocation = RecompPos - 4;
			JmpLabel32(Section->Jump.BranchLabel,0);
			Section->Jump.LinkLocation2 = RecompPos - 4;
		}
	}
}

void BEQ_Compare (BLOCK_SECTION * Section) {
	BYTE *Jump = 0x0;

	if (IsKnown(Opcode.BRANCH.rs) && IsKnown(Opcode.BRANCH.rt)) {
		if (IsConst(Opcode.BRANCH.rs) && IsConst(Opcode.BRANCH.rt)) {
			if (Is64Bit(Opcode.BRANCH.rs) || Is64Bit(Opcode.BRANCH.rt)) {
				Compile_R4300i_UnknownOpcode(Section);
			} else if (MipsRegLo(Opcode.BRANCH.rs) == MipsRegLo(Opcode.BRANCH.rt)) {
				Section->Jump.FallThrough = TRUE;
				Section->Cont.FallThrough = FALSE;
			} else {
				Section->Jump.FallThrough = FALSE;
				Section->Cont.FallThrough = TRUE;
			}
		} else if (IsMapped(Opcode.BRANCH.rs) && IsMapped(Opcode.BRANCH.rt)) {
			if (Is64Bit(Opcode.BRANCH.rs) || Is64Bit(Opcode.BRANCH.rt)) {
				ProtectGPR(Section,Opcode.BRANCH.rs);
				ProtectGPR(Section,Opcode.BRANCH.rt);

				CompX86RegToX86Reg(
					Is32Bit(Opcode.BRANCH.rs)?Map_TempReg(Section,x86_Any,Opcode.BRANCH.rs,TRUE):MipsRegHi(Opcode.BRANCH.rs),
					Is32Bit(Opcode.BRANCH.rt)?Map_TempReg(Section,x86_Any,Opcode.BRANCH.rt,TRUE):MipsRegHi(Opcode.BRANCH.rt)
				);
				if (Section->Cont.FallThrough) {
					JneLabel8("continue",0);
					Jump = RecompPos - 1;
				} else {
					JneLabel32(Section->Cont.BranchLabel,0);
					Section->Cont.LinkLocation = RecompPos - 4;
				}
				CompX86RegToX86Reg(MipsRegLo(Opcode.BRANCH.rs),MipsRegLo(Opcode.BRANCH.rt));
				if (Section->Cont.FallThrough) {
					JeLabel32 ( Section->Jump.BranchLabel, 0 );
					Section->Jump.LinkLocation = RecompPos - 4;
					CPU_Message("      ");
					CPU_Message("      continue:");
					SetJump8(Jump,RecompPos);
				} else if (Section->Jump.FallThrough) {
					JneLabel32 ( Section->Cont.BranchLabel, 0 );
					Section->Cont.LinkLocation2 = RecompPos - 4;
				} else {
					JneLabel32 ( Section->Cont.BranchLabel, 0 );
					Section->Cont.LinkLocation2 = RecompPos - 4;
					JmpLabel32(Section->Jump.BranchLabel,0);
					Section->Jump.LinkLocation = RecompPos - 4;
				}
			} else {
				CompX86RegToX86Reg(MipsRegLo(Opcode.BRANCH.rs),MipsRegLo(Opcode.BRANCH.rt));
				if (Section->Cont.FallThrough) {
					JeLabel32 ( Section->Jump.BranchLabel, 0 );
					Section->Jump.LinkLocation = RecompPos - 4;
				} else if (Section->Jump.FallThrough) {
					JneLabel32 ( Section->Cont.BranchLabel, 0 );
					Section->Cont.LinkLocation = RecompPos - 4;
				} else {
					JneLabel32 ( Section->Cont.BranchLabel, 0 );
					Section->Cont.LinkLocation = RecompPos - 4;
					JmpLabel32(Section->Jump.BranchLabel,0);
					Section->Jump.LinkLocation = RecompPos - 4;
				}
			}
		} else {
			DWORD ConstReg = IsConst(Opcode.BRANCH.rt)?Opcode.BRANCH.rt:Opcode.BRANCH.rs;
			DWORD MappedReg = IsConst(Opcode.BRANCH.rt)?Opcode.BRANCH.rs:Opcode.BRANCH.rt;

			if (Is64Bit(ConstReg) || Is64Bit(MappedReg)) {
				if (Is32Bit(ConstReg) || Is32Bit(MappedReg)) {
					if (Is32Bit(MappedReg)) {
						ProtectGPR(Section,MappedReg);
						CompConstToX86reg(Map_TempReg(Section,x86_Any,MappedReg,TRUE),MipsRegHi(ConstReg));
					} else {
						CompConstToX86reg(MipsRegHi(MappedReg),(int)MipsRegLo(ConstReg) >> 31);
					}
				} else {
					CompConstToX86reg(MipsRegHi(MappedReg),MipsRegHi(ConstReg));
				}			
				if (Section->Cont.FallThrough) {
					JneLabel8("continue",0);
					Jump = RecompPos - 1;
				} else {
					JneLabel32(Section->Cont.BranchLabel,0);
					Section->Cont.LinkLocation = RecompPos - 4;
				}
				if (MipsRegLo(ConstReg) == 0) {
					OrX86RegToX86Reg(MipsRegLo(MappedReg),MipsRegLo(MappedReg));
				} else {
					CompConstToX86reg(MipsRegLo(MappedReg),MipsRegLo(ConstReg));
				}
				if (Section->Cont.FallThrough) {
					JeLabel32 ( Section->Jump.BranchLabel, 0 );
					Section->Jump.LinkLocation = RecompPos - 4;
					CPU_Message("      ");
					CPU_Message("      continue:");
					SetJump8(Jump,RecompPos);
				} else if (Section->Jump.FallThrough) {
					JneLabel32 ( Section->Cont.BranchLabel, 0 );
					Section->Cont.LinkLocation2 = RecompPos - 4;
				} else {
					JneLabel32 ( Section->Cont.BranchLabel, 0 );
					Section->Cont.LinkLocation2 = RecompPos - 4;
					JmpLabel32(Section->Jump.BranchLabel,0);
					Section->Jump.LinkLocation = RecompPos - 4;
				}
			} else {
				if (MipsRegLo(ConstReg) == 0) {
					OrX86RegToX86Reg(MipsRegLo(MappedReg),MipsRegLo(MappedReg));
				} else {
					CompConstToX86reg(MipsRegLo(MappedReg),MipsRegLo(ConstReg));
				}
				if (Section->Cont.FallThrough) {
					JeLabel32 ( Section->Jump.BranchLabel, 0 );
					Section->Jump.LinkLocation = RecompPos - 4;
				} else if (Section->Jump.FallThrough) {
					JneLabel32 ( Section->Cont.BranchLabel, 0 );
					Section->Cont.LinkLocation = RecompPos - 4;
				} else {
					JneLabel32 ( Section->Cont.BranchLabel, 0 );
					Section->Cont.LinkLocation = RecompPos - 4;
					JmpLabel32(Section->Jump.BranchLabel,0);
					Section->Jump.LinkLocation = RecompPos - 4;
				}
			}
		}
	} else if (IsKnown(Opcode.BRANCH.rs) || IsKnown(Opcode.BRANCH.rt)) {
		DWORD KnownReg = IsKnown(Opcode.BRANCH.rt)?Opcode.BRANCH.rt:Opcode.BRANCH.rs;
		DWORD UnknownReg = IsKnown(Opcode.BRANCH.rt)?Opcode.BRANCH.rs:Opcode.BRANCH.rt;

		if (IsConst(KnownReg)) {
			if (Is64Bit(KnownReg)) {
				CompConstToVariable(MipsRegHi(KnownReg),&GPR[UnknownReg].W[1],GPR_NameHi[UnknownReg]);
			} else if (IsSigned(KnownReg)) {
				CompConstToVariable((int)MipsRegLo(KnownReg) >> 31,&GPR[UnknownReg].W[1],GPR_NameHi[UnknownReg]);
			} else {
				CompConstToVariable(0,&GPR[UnknownReg].W[1],GPR_NameHi[UnknownReg]);
			}
		} else {
			ProtectGPR(Section,KnownReg);
			if (Is64Bit(KnownReg)) {
				CompX86regToVariable(MipsRegHi(KnownReg),&GPR[UnknownReg].W[1],GPR_NameHi[UnknownReg]);
			} else if (IsSigned(KnownReg)) {
				CompX86regToVariable(Map_TempReg(Section,x86_Any,KnownReg,TRUE),&GPR[UnknownReg].W[1],GPR_NameHi[UnknownReg]);
			} else {
				CompConstToVariable(0,&GPR[UnknownReg].W[1],GPR_NameHi[UnknownReg]);
			}
		}
		if (Section->Cont.FallThrough) {
			JneLabel8("continue",0);
			Jump = RecompPos - 1;
		} else {
			JneLabel32(Section->Cont.BranchLabel,0);
			Section->Cont.LinkLocation = RecompPos - 4;
		}
		if (IsConst(KnownReg)) {
			CompConstToVariable(MipsRegLo(KnownReg),&GPR[UnknownReg].W[0],GPR_NameLo[UnknownReg]);
		} else {
			CompX86regToVariable(MipsRegLo(KnownReg),&GPR[UnknownReg].W[0],GPR_NameLo[UnknownReg]);
		}
		if (Section->Cont.FallThrough) {
			JeLabel32 ( Section->Jump.BranchLabel, 0 );
			Section->Jump.LinkLocation = RecompPos - 4;
			CPU_Message("      ");
			CPU_Message("      continue:");
			SetJump8(Jump,RecompPos);
		} else if (Section->Jump.FallThrough) {
			JneLabel32 ( Section->Cont.BranchLabel, 0 );
			Section->Cont.LinkLocation2 = RecompPos - 4;
		} else {
			JneLabel32 ( Section->Cont.BranchLabel, 0 );
			Section->Cont.LinkLocation2 = RecompPos - 4;
			JmpLabel32(Section->Jump.BranchLabel,0);
			Section->Jump.LinkLocation = RecompPos - 4;
		}
	} else {
		int x86Reg = Map_TempReg(Section,x86_Any,Opcode.BRANCH.rs,TRUE);
		CompX86regToVariable(x86Reg,&GPR[Opcode.BRANCH.rt].W[1],GPR_NameHi[Opcode.BRANCH.rt]);
		if (Section->Cont.FallThrough) {
			JneLabel8("continue",0);
			Jump = RecompPos - 1;
		} else {
			JneLabel32(Section->Cont.BranchLabel,0);
			Section->Cont.LinkLocation = RecompPos - 4;
		}
		CompX86regToVariable(Map_TempReg(Section,x86Reg,Opcode.BRANCH.rs,FALSE),&GPR[Opcode.BRANCH.rt].W[0],GPR_NameLo[Opcode.BRANCH.rt]);
		if (Section->Cont.FallThrough) {
			JeLabel32 ( Section->Jump.BranchLabel, 0 );
			Section->Jump.LinkLocation = RecompPos - 4;
			CPU_Message("      ");
			CPU_Message("      continue:");
			SetJump8(Jump,RecompPos);
		} else if (Section->Jump.FallThrough) {
			JneLabel32 ( Section->Cont.BranchLabel, 0 );
			Section->Cont.LinkLocation2 = RecompPos - 4;
		} else {
			JneLabel32 ( Section->Cont.BranchLabel, 0 );
			Section->Cont.LinkLocation2 = RecompPos - 4;
			JmpLabel32(Section->Jump.BranchLabel,0);
			Section->Jump.LinkLocation = RecompPos - 4;
		}
	}
}

void BGTZ_Compare (BLOCK_SECTION * Section) {
	if (IsConst(Opcode.BRANCH.rs)) {
		if (Is64Bit(Opcode.BRANCH.rs)) {
			if (MipsReg_S(Opcode.BRANCH.rs) > 0) {
				Section->Jump.FallThrough = TRUE;
				Section->Cont.FallThrough = FALSE;
			} else {
				Section->Jump.FallThrough = FALSE;
				Section->Cont.FallThrough = TRUE;
			}
		} else {
			if (MipsRegLo_S(Opcode.BRANCH.rs) > 0) {
				Section->Jump.FallThrough = TRUE;
				Section->Cont.FallThrough = FALSE;
			} else {
				Section->Jump.FallThrough = FALSE;
				Section->Cont.FallThrough = TRUE;
			}
		}
	} else if (IsMapped(Opcode.BRANCH.rs) && Is32Bit(Opcode.BRANCH.rs)) {
		CompConstToX86reg(MipsRegLo(Opcode.BRANCH.rs),0);
		if (Section->Jump.FallThrough) {
			JleLabel32 (Section->Cont.BranchLabel, 0 );
			Section->Cont.LinkLocation = RecompPos - 4;
		} else if (Section->Cont.FallThrough) {
			JgLabel32(Section->Jump.BranchLabel,0);
			Section->Jump.LinkLocation = RecompPos - 4;
		} else {
			JleLabel32 (Section->Cont.BranchLabel, 0 );
			Section->Cont.LinkLocation = RecompPos - 4;
			JmpLabel32(Section->Jump.BranchLabel,0);
			Section->Jump.LinkLocation = RecompPos - 4;
		}
	} else {
		BYTE *Jump = 0x0;

		if (IsMapped(Opcode.BRANCH.rs)) {
			CompConstToX86reg(MipsRegHi(Opcode.BRANCH.rs),0);
		} else {
			CompConstToVariable(0,&GPR[Opcode.BRANCH.rs].W[1],GPR_NameHi[Opcode.BRANCH.rs]);
		}
		if (Section->Jump.FallThrough) {
			JlLabel32 (Section->Cont.BranchLabel, 0 );
			Section->Cont.LinkLocation = RecompPos - 4;
			JgLabel8("continue",0);
			Jump = RecompPos - 1;
		} else if (Section->Cont.FallThrough) {
			JlLabel8("continue",0);
			Jump = RecompPos - 1;
			JgLabel32(Section->Jump.BranchLabel,0);
			Section->Jump.LinkLocation = RecompPos - 4;
		} else {
			JlLabel32 (Section->Cont.BranchLabel, 0 );
			Section->Cont.LinkLocation = RecompPos - 4;
			JgLabel32(Section->Jump.BranchLabel,0);
			Section->Jump.LinkLocation = RecompPos - 4;
		}

		if (IsMapped(Opcode.BRANCH.rs)) {
			CompConstToX86reg(MipsRegLo(Opcode.BRANCH.rs),0);
		} else {
			CompConstToVariable(0,&GPR[Opcode.BRANCH.rs].W[0],GPR_NameLo[Opcode.BRANCH.rs]);
		}
		if (Section->Jump.FallThrough) {
			JeLabel32 (Section->Cont.BranchLabel, 0 );
			Section->Cont.LinkLocation2 = RecompPos - 4;
			CPU_Message("      continue:");
			*((BYTE *)(Jump))=(BYTE)(RecompPos - Jump - 1);
		} else if (Section->Cont.FallThrough) {
			JneLabel32(Section->Jump.BranchLabel,0);
			Section->Jump.LinkLocation = RecompPos - 4;
			CPU_Message("      continue:");
			*((BYTE *)(Jump))=(BYTE)(RecompPos - Jump - 1);
		} else {
			JneLabel32(Section->Jump.BranchLabel,0);
			Section->Jump.LinkLocation = RecompPos - 4;
			JmpLabel32 (Section->Cont.BranchLabel, 0 );
			Section->Cont.LinkLocation2 = RecompPos - 4;
		}
	}
}

void BLEZ_Compare (BLOCK_SECTION * Section) {
	if (IsConst(Opcode.BRANCH.rs)) {
		if (Is64Bit(Opcode.BRANCH.rs)) {
			if (MipsReg_S(Opcode.BRANCH.rs) <= 0) {
				Section->Jump.FallThrough = TRUE;
				Section->Cont.FallThrough = FALSE;
			} else {
				Section->Jump.FallThrough = FALSE;
				Section->Cont.FallThrough = TRUE;
			}
		} else if (IsSigned(Opcode.BRANCH.rs)) {
			if (MipsRegLo_S(Opcode.BRANCH.rs) <= 0) {
				Section->Jump.FallThrough = TRUE;
				Section->Cont.FallThrough = FALSE;
			} else {
				Section->Jump.FallThrough = FALSE;
				Section->Cont.FallThrough = TRUE;
			}
		} else {
			if (MipsRegLo(Opcode.BRANCH.rs) == 0) {
				Section->Jump.FallThrough = TRUE;
				Section->Cont.FallThrough = FALSE;
			} else {
				Section->Jump.FallThrough = FALSE;
				Section->Cont.FallThrough = TRUE;
			}
		}
	} else {
		if (IsMapped(Opcode.BRANCH.rs) && Is32Bit(Opcode.BRANCH.rs)) {
			CompConstToX86reg(MipsRegLo(Opcode.BRANCH.rs),0);
			if (Section->Jump.FallThrough) {
				JgLabel32 (Section->Cont.BranchLabel, 0 );
				Section->Cont.LinkLocation = RecompPos - 4;
			} else if (Section->Cont.FallThrough) {
				JleLabel32(Section->Jump.BranchLabel,0);
				Section->Jump.LinkLocation = RecompPos - 4;
			} else {
				JgLabel32 (Section->Cont.BranchLabel, 0 );
				Section->Cont.LinkLocation = RecompPos - 4;
				JmpLabel32(Section->Jump.BranchLabel,0);
				Section->Jump.LinkLocation = RecompPos - 4;
			}
		} else {
			BYTE *Jump = 0x0;

			if (IsMapped(Opcode.BRANCH.rs)) {
				CompConstToX86reg(MipsRegHi(Opcode.BRANCH.rs),0);
			} else {
				CompConstToVariable(0,&GPR[Opcode.BRANCH.rs].W[1],GPR_NameHi[Opcode.BRANCH.rs]);
			}
			if (Section->Jump.FallThrough) {
				JgLabel32 (Section->Cont.BranchLabel, 0 );
				Section->Cont.LinkLocation = RecompPos - 4;
				JlLabel8("Continue",0);
				Jump = RecompPos - 1;
			} else if (Section->Cont.FallThrough) {
				JgLabel8("Continue",0);
				Jump = RecompPos - 1;
				JlLabel32(Section->Jump.BranchLabel,0);
				Section->Jump.LinkLocation = RecompPos - 4;
			} else {
				JgLabel32 (Section->Cont.BranchLabel, 0 );
				Section->Cont.LinkLocation = RecompPos - 4;
				JlLabel32(Section->Jump.BranchLabel,0);
				Section->Jump.LinkLocation = RecompPos - 4;
			}

			if (IsMapped(Opcode.BRANCH.rs)) {
				CompConstToX86reg(MipsRegLo(Opcode.BRANCH.rs),0);
			} else {
				CompConstToVariable(0,&GPR[Opcode.BRANCH.rs].W[0],GPR_NameLo[Opcode.BRANCH.rs]);
			}
			if (Section->Jump.FallThrough) {
				JneLabel32 (Section->Cont.BranchLabel, 0 );
				Section->Cont.LinkLocation2 = RecompPos - 4;
				CPU_Message("      continue:");
				*((BYTE *)(Jump))=(BYTE)(RecompPos - Jump - 1);
			} else if (Section->Cont.FallThrough) {
				JeLabel32 (Section->Jump.BranchLabel, 0 );
				Section->Jump.LinkLocation2 = RecompPos - 4;
				CPU_Message("      continue:");
				*((BYTE *)(Jump))=(BYTE)(RecompPos - Jump - 1);
			} else {
				JneLabel32 (Section->Cont.BranchLabel, 0 );
				Section->Cont.LinkLocation2 = RecompPos - 4;
				JmpLabel32("BranchToJump",0);
				Section->Jump.LinkLocation2 = RecompPos - 4;
			}
		}
	}
}

void BLTZ_Compare (BLOCK_SECTION * Section) {
	if (IsConst(Opcode.BRANCH.rs)) {
		if (Is64Bit(Opcode.BRANCH.rs)) {
			if (MipsReg_S(Opcode.BRANCH.rs) < 0) {
				Section->Jump.FallThrough = TRUE;
				Section->Cont.FallThrough = FALSE;
			} else {
				Section->Jump.FallThrough = FALSE;
				Section->Cont.FallThrough = TRUE;
			}
		} else if (IsSigned(Opcode.BRANCH.rs)) {
			if (MipsRegLo_S(Opcode.BRANCH.rs) < 0) {
				Section->Jump.FallThrough = TRUE;
				Section->Cont.FallThrough = FALSE;
			} else {
				Section->Jump.FallThrough = FALSE;
				Section->Cont.FallThrough = TRUE;
			}
		} else {
			Section->Jump.FallThrough = FALSE;
			Section->Cont.FallThrough = TRUE;
		}
	} else if (IsMapped(Opcode.BRANCH.rs)) {
		if (Is64Bit(Opcode.BRANCH.rs)) {
			CompConstToX86reg(MipsRegHi(Opcode.BRANCH.rs),0);
			if (Section->Jump.FallThrough) {
				JgeLabel32 (Section->Cont.BranchLabel, 0 );
				Section->Cont.LinkLocation = RecompPos - 4;
			} else if (Section->Cont.FallThrough) {
				JlLabel32(Section->Jump.BranchLabel,0);
				Section->Jump.LinkLocation = RecompPos - 4;
			} else {
				JgeLabel32 (Section->Cont.BranchLabel, 0 );
				Section->Cont.LinkLocation = RecompPos - 4;
				JmpLabel32(Section->Jump.BranchLabel,0);
				Section->Jump.LinkLocation = RecompPos - 4;
			}
		} else if (IsSigned(Opcode.BRANCH.rs)) {
			CompConstToX86reg(MipsRegLo(Opcode.BRANCH.rs),0);
			if (Section->Jump.FallThrough) {
				JgeLabel32 (Section->Cont.BranchLabel, 0 );
				Section->Cont.LinkLocation = RecompPos - 4;
			} else if (Section->Cont.FallThrough) {
				JlLabel32(Section->Jump.BranchLabel,0);
				Section->Jump.LinkLocation = RecompPos - 4;
			} else {
				JgeLabel32 (Section->Cont.BranchLabel, 0 );
				Section->Cont.LinkLocation = RecompPos - 4;
				JmpLabel32(Section->Jump.BranchLabel,0);
				Section->Jump.LinkLocation = RecompPos - 4;
			}
		} else {
			Section->Jump.FallThrough = FALSE;
			Section->Cont.FallThrough = TRUE;
		}
	} else if (IsUnknown(Opcode.BRANCH.rs)) {
		CompConstToVariable(0,&GPR[Opcode.BRANCH.rs].W[1],GPR_NameHi[Opcode.BRANCH.rs]);
		if (Section->Jump.FallThrough) {
			JgeLabel32 (Section->Cont.BranchLabel, 0 );
			Section->Cont.LinkLocation = RecompPos - 4;
		} else if (Section->Cont.FallThrough) {
			JlLabel32(Section->Jump.BranchLabel,0);
			Section->Jump.LinkLocation = RecompPos - 4;
		} else {
			JlLabel32(Section->Jump.BranchLabel,0);
			Section->Jump.LinkLocation = RecompPos - 4;
			JmpLabel32 (Section->Cont.BranchLabel, 0 );
			Section->Cont.LinkLocation = RecompPos - 4;
		}
	}
}

void BGEZ_Compare (BLOCK_SECTION * Section) {
	if (IsConst(Opcode.BRANCH.rs)) {
		if (Is64Bit(Opcode.BRANCH.rs)) {
			if (ShowDebugMessages)
				DisplayError("BGEZ 1");

			Compile_R4300i_UnknownOpcode(Section);
		} else if IsSigned(Opcode.BRANCH.rs) {
			if (MipsRegLo_S(Opcode.BRANCH.rs) >= 0) {
				Section->Jump.FallThrough = TRUE;
				Section->Cont.FallThrough = FALSE;
			} else {
				Section->Jump.FallThrough = FALSE;
				Section->Cont.FallThrough = TRUE;
			}
		} else {
			Section->Jump.FallThrough = TRUE;
			Section->Cont.FallThrough = FALSE;
		}
	} else if (IsMapped(Opcode.BRANCH.rs)) {
		if (Is64Bit(Opcode.BRANCH.rs)) { 
			CompConstToX86reg(MipsRegHi(Opcode.BRANCH.rs),0);
			if (Section->Cont.FallThrough) {
				JgeLabel32 ( Section->Jump.BranchLabel, 0 );
				Section->Jump.LinkLocation = RecompPos - 4;
			} else if (Section->Jump.FallThrough) {
				JlLabel32 ( Section->Cont.BranchLabel, 0 );
				Section->Cont.LinkLocation = RecompPos - 4;
			} else {
				JlLabel32 ( Section->Cont.BranchLabel, 0 );
				Section->Cont.LinkLocation = RecompPos - 4;
				JmpLabel32(Section->Jump.BranchLabel,0);
				Section->Jump.LinkLocation = RecompPos - 4;
			}
		} else if (IsSigned(Opcode.BRANCH.rs)) {
			CompConstToX86reg(MipsRegLo(Opcode.BRANCH.rs),0);
			if (Section->Cont.FallThrough) {
				JgeLabel32 ( Section->Jump.BranchLabel, 0 );
				Section->Jump.LinkLocation = RecompPos - 4;
			} else if (Section->Jump.FallThrough) {
				JlLabel32 ( Section->Cont.BranchLabel, 0 );
				Section->Cont.LinkLocation = RecompPos - 4;
			} else {
				JlLabel32 ( Section->Cont.BranchLabel, 0 );
				Section->Cont.LinkLocation = RecompPos - 4;
				JmpLabel32(Section->Jump.BranchLabel,0);
				Section->Jump.LinkLocation = RecompPos - 4;
			}
		} else { 
			Section->Jump.FallThrough = TRUE;
			Section->Cont.FallThrough = FALSE;
		}
	} else {
		CompConstToVariable(0,&GPR[Opcode.BRANCH.rs].W[1],GPR_NameHi[Opcode.BRANCH.rs]);		
		if (Section->Cont.FallThrough) {
			JgeLabel32 ( Section->Jump.BranchLabel, 0 );
			Section->Jump.LinkLocation = RecompPos - 4;
		} else if (Section->Jump.FallThrough) {
			JlLabel32 ( Section->Cont.BranchLabel, 0 );
			Section->Cont.LinkLocation = RecompPos - 4;
		} else {
			JlLabel32 ( Section->Cont.BranchLabel, 0 );
			Section->Cont.LinkLocation = RecompPos - 4;
			JmpLabel32(Section->Jump.BranchLabel,0);
			Section->Jump.LinkLocation = RecompPos - 4;
		}
	}
}

void COP1_BCF_Compare (BLOCK_SECTION * Section) {
	TestVariable(FPCSR_C,&FPCR[31],"FPCR[31]");
	if (Section->Cont.FallThrough) {
		JeLabel32 ( Section->Jump.BranchLabel, 0 );
		Section->Jump.LinkLocation = RecompPos - 4;
	} else if (Section->Jump.FallThrough) {
		JneLabel32 ( Section->Cont.BranchLabel, 0 );
		Section->Cont.LinkLocation = RecompPos - 4;
	} else {
		JneLabel32 ( Section->Cont.BranchLabel, 0 );
		Section->Cont.LinkLocation = RecompPos - 4;
		JmpLabel32(Section->Jump.BranchLabel,0);
		Section->Jump.LinkLocation = RecompPos - 4;
	}
}

void COP1_BCT_Compare (BLOCK_SECTION * Section) {
	TestVariable(FPCSR_C,&FPCR[31],"FPCR[31]");
	if (Section->Cont.FallThrough) {
		JneLabel32 ( Section->Jump.BranchLabel, 0 );
		Section->Jump.LinkLocation = RecompPos - 4;
	} else if (Section->Jump.FallThrough) {
		JeLabel32 ( Section->Cont.BranchLabel, 0 );
		Section->Cont.LinkLocation = RecompPos - 4;
	} else {
		JeLabel32 ( Section->Cont.BranchLabel, 0 );
		Section->Cont.LinkLocation = RecompPos - 4;
		JmpLabel32(Section->Jump.BranchLabel,0);
		Section->Jump.LinkLocation = RecompPos - 4;
	}
}
/*************************  OpCode functions *************************/
void Compile_R4300i_J (BLOCK_SECTION * Section) {
	static char JumpLabel[100];
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s", Section->CompilePC, R4300iOpcodeName(Opcode.Hex, CompilePC));

	if ( NextInstruction == NORMAL ) {
		CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));

		if (Section->JumpSection != NULL) {
			sprintf(JumpLabel,"Section_%d",((BLOCK_SECTION *)Section->JumpSection)->SectionID);
		} else {
			strcpy(JumpLabel,"ExitBlock");
		}
		Section->Jump.TargetPC      = (Section->CompilePC & 0xF0000000) + (Opcode.JMP.target << 2);;
		Section->Jump.BranchLabel   = JumpLabel;
		Section->Jump.FallThrough   = TRUE;
		Section->Jump.LinkLocation  = NULL;
		Section->Jump.LinkLocation2 = NULL;
		NextInstruction = DO_DELAY_SLOT;
		if ((Section->CompilePC & 0xFFC) == 0xFFC) {
			memcpy(&Section->Jump.RegSet,&Section->RegWorking,sizeof(REG_INFO));
			GenerateSectionLinkage(Section);
			NextInstruction = END_BLOCK;
		}
	} else if (NextInstruction == DELAY_SLOT_DONE ) {		
		memcpy(&Section->Jump.RegSet,&Section->RegWorking,sizeof(REG_INFO));
		GenerateSectionLinkage(Section);
		NextInstruction = END_BLOCK;
	} else {
		if (ShowDebugMessages)
			DisplayError("WTF\n\nJal\nNextInstruction = %X", NextInstruction);
	}
}

void Compile_R4300i_JAL (BLOCK_SECTION * Section) {
	static char JumpLabel[100];
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s", Section->CompilePC, R4300iOpcodeName(Opcode.Hex, CompilePC));

	if ( NextInstruction == NORMAL ) {
		CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
		UnMap_GPR(Section, 31, FALSE);
		MipsRegLo(31) = Section->CompilePC + 8;
		MipsRegState(31) = STATE_CONST_32;
		if ((Section->CompilePC & 0xFFC) == 0xFFC) {
			MoveConstToVariable((Section->CompilePC & 0xF0000000) + (Opcode.JMP.target << 2),&JumpToLocation,"JumpToLocation");
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
			NextInstruction = END_BLOCK;
			return;
		}
		NextInstruction = DO_DELAY_SLOT;
	} else if (NextInstruction == DELAY_SLOT_DONE ) {		
		MoveConstToVariable((Section->CompilePC & 0xF0000000) + (Opcode.JMP.target << 2),&PROGRAM_COUNTER,"PROGRAM_COUNTER");
		CompileExit((DWORD)-1,Section->RegWorking,Normal,TRUE,NULL);
		NextInstruction = END_BLOCK;
	} else {
		if (ShowDebugMessages)
			DisplayError("WTF\n\nBranch\nNextInstruction = %X", NextInstruction);
	}
	return;
/*
	if ( NextInstruction == NORMAL ) {
		CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,Section->CompilePC));

		UnMap_GPR(Section, 31, FALSE);
		MipsRegLo(31) = Section->CompilePC + 8;
		MipsRegState(31) = STATE_CONST_32;
		NextInstruction = DO_DELAY_SLOT;
		if (Section->JumpSection != NULL) {
			sprintf(JumpLabel,"Section_%d",((BLOCK_SECTION *)Section->JumpSection)->SectionID);
		} else {
			strcpy(JumpLabel,"ExitBlock");
		}
		Section->Jump.TargetPC      = (Section->CompilePC & 0xF0000000) + (Opcode.JMP.target << 2);
		Section->Jump.BranchLabel   = JumpLabel;
		Section->Jump.FallThrough   = TRUE;
		Section->Jump.LinkLocation  = NULL;
		Section->Jump.LinkLocation2 = NULL;
		if ((Section->CompilePC & 0xFFC) == 0xFFC) {
			memcpy(&Section->Jump.RegSet,&Section->RegWorking,sizeof(REG_INFO));
			GenerateSectionLinkage(Section);
			NextInstruction = END_BLOCK;
		}
	} else if (NextInstruction == DELAY_SLOT_DONE ) {		
		memcpy(&Section->Jump.RegSet,&Section->RegWorking,sizeof(REG_INFO));
		GenerateSectionLinkage(Section);
		NextInstruction = END_BLOCK;
	} else {
	if (ShowDebugMessages)
		DisplayError("WTF\n\nJal\nNextInstruction = %X", NextInstruction);
	}*/
}

void Compile_R4300i_ADDI (BLOCK_SECTION * Section) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));

	if (Opcode.BRANCH.rt == 0) { return; }

	if (SPHack && Opcode.BRANCH.rs == 29 && Opcode.BRANCH.rt == 29) {
		AddConstToX86Reg(Map_MemoryStack(Section, TRUE),(short)Opcode.IMM.immediate);
	}

	if (IsConst(Opcode.BRANCH.rs)) { 
		if (IsMapped(Opcode.BRANCH.rt)) { UnMap_GPR(Section,Opcode.BRANCH.rt, FALSE); }
		MipsRegLo(Opcode.BRANCH.rt) = MipsRegLo(Opcode.BRANCH.rs) + (short)Opcode.IMM.immediate;
		MipsRegState(Opcode.BRANCH.rt) = STATE_CONST_32;
		return;
	}
	Map_GPR_32bit(Section,Opcode.BRANCH.rt,TRUE,Opcode.BRANCH.rs);
	if (Opcode.IMM.immediate == 0) { 
	} else if (Opcode.IMM.immediate == 1) {
		IncX86reg(MipsRegLo(Opcode.BRANCH.rt));
	} else if (Opcode.IMM.immediate == 0xFFFF) {			
		DecX86reg(MipsRegLo(Opcode.BRANCH.rt));
	} else {
		AddConstToX86Reg(MipsRegLo(Opcode.BRANCH.rt),(short)Opcode.IMM.immediate);
	}
	if (SPHack && Opcode.BRANCH.rt == 29 && Opcode.BRANCH.rs != 29) { 
		int count;

		for (count = 0; count < 10; count ++) { x86Protected(count) = FALSE; }
		ResetMemoryStack(Section); 
	}

}

void Compile_R4300i_ADDIU (BLOCK_SECTION * Section) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));

	if (Opcode.BRANCH.rt == 0) { return; }

	if (SPHack && Opcode.BRANCH.rs == 29 && Opcode.BRANCH.rt == 29) {
		AddConstToX86Reg(Map_MemoryStack(Section, TRUE),(short)Opcode.IMM.immediate);
	}

	if (IsConst(Opcode.BRANCH.rs)) { 
		if (IsMapped(Opcode.BRANCH.rt)) { UnMap_GPR(Section,Opcode.BRANCH.rt, FALSE); }
		MipsRegLo(Opcode.BRANCH.rt) = MipsRegLo(Opcode.BRANCH.rs) + (short)Opcode.IMM.immediate;
		MipsRegState(Opcode.BRANCH.rt) = STATE_CONST_32;
		return;
	}
	Map_GPR_32bit(Section,Opcode.BRANCH.rt,TRUE,Opcode.BRANCH.rs);
	if (Opcode.IMM.immediate == 0) { 
	} else if (Opcode.IMM.immediate == 1) {
		IncX86reg(MipsRegLo(Opcode.BRANCH.rt));
	} else if (Opcode.IMM.immediate == 0xFFFF) {			
		DecX86reg(MipsRegLo(Opcode.BRANCH.rt));
	} else {
		AddConstToX86Reg(MipsRegLo(Opcode.BRANCH.rt),(short)Opcode.IMM.immediate);
	}
	if (SPHack && Opcode.BRANCH.rt == 29 && Opcode.BRANCH.rs != 29) { 
		int count;

		for (count = 0; count < 10; count ++) { x86Protected(count) = FALSE; }
		ResetMemoryStack(Section); 
	}
}

void Compile_R4300i_SLTIU (BLOCK_SECTION * Section) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
	if (Opcode.BRANCH.rt == 0) { return; }

	if (IsConst(Opcode.BRANCH.rs)) { 
		DWORD Result = 0x0;

		if (Is64Bit(Opcode.BRANCH.rs)) {
			_int64 Immediate = (_int64)((short)Opcode.IMM.immediate);
			Result = MipsReg(Opcode.BRANCH.rs) < ((unsigned)(Immediate))?1:0;
		} else if (Is32Bit(Opcode.BRANCH.rs)) {
			Result = MipsRegLo(Opcode.BRANCH.rs) < ((unsigned)((short)Opcode.IMM.immediate))?1:0;
		}
		UnMap_GPR(Section,Opcode.BRANCH.rt, FALSE);
		MipsRegState(Opcode.BRANCH.rt) = STATE_CONST_32;
		MipsRegLo(Opcode.BRANCH.rt) = Result;
	} else if (IsMapped(Opcode.BRANCH.rs)) { 
		if (Is64Bit(Opcode.BRANCH.rs)) {
			BYTE * Jump[2];

			CompConstToX86reg(MipsRegHi(Opcode.BRANCH.rs),((short)Opcode.IMM.immediate >> 31));
			JeLabel8("Low Compare",0);
			Jump[0] = RecompPos - 1;
			SetbVariable(&BranchCompare,"BranchCompare");
			JmpLabel8("Continue",0);
			Jump[1] = RecompPos - 1;
			CPU_Message("");
			CPU_Message("      Low Compare:");
			*((BYTE *)(Jump[0]))=(BYTE)(RecompPos - Jump[0] - 1);
			CompConstToX86reg(MipsRegLo(Opcode.BRANCH.rs),(short)Opcode.IMM.immediate);
			SetbVariable(&BranchCompare,"BranchCompare");
			CPU_Message("");
			CPU_Message("      Continue:");
			*((BYTE *)(Jump[1]))=(BYTE)(RecompPos - Jump[1] - 1);
			Map_GPR_32bit(Section,Opcode.BRANCH.rt,FALSE, -1);
			MoveVariableToX86reg(&BranchCompare,"BranchCompare",MipsRegLo(Opcode.BRANCH.rt));
		} else {
			CompConstToX86reg(MipsRegLo(Opcode.BRANCH.rs),(short)Opcode.IMM.immediate);
			SetbVariable(&BranchCompare,"BranchCompare");
			Map_GPR_32bit(Section,Opcode.BRANCH.rt,FALSE, -1);
			MoveVariableToX86reg(&BranchCompare,"BranchCompare",MipsRegLo(Opcode.BRANCH.rt));
		}
	} else {
		BYTE * Jump;

		CompConstToVariable(((short)Opcode.IMM.immediate >> 31),&GPR[Opcode.BRANCH.rs].W[1],GPR_NameHi[Opcode.BRANCH.rs]);
		JneLabel8("CompareSet",0);
		Jump = RecompPos - 1;
		CompConstToVariable((short)Opcode.IMM.immediate,&GPR[Opcode.BRANCH.rs].W[0],GPR_NameLo[Opcode.BRANCH.rs]);
		CPU_Message("");
		CPU_Message("      CompareSet:");
		*((BYTE *)(Jump))=(BYTE)(RecompPos - Jump - 1);
		SetbVariable(&BranchCompare,"BranchCompare");
		Map_GPR_32bit(Section,Opcode.BRANCH.rt,FALSE, -1);
		MoveVariableToX86reg(&BranchCompare,"BranchCompare",MipsRegLo(Opcode.BRANCH.rt));
		
		
		/*SetbVariable(&BranchCompare,"BranchCompare");
		JmpLabel8("Continue",0);
		Jump[1] = RecompPos - 1;
		CPU_Message("");
		CPU_Message("      Low Compare:");
		*((BYTE *)(Jump[0]))=(BYTE)(RecompPos - Jump[0] - 1);
		CompConstToVariable((short)Opcode.IMM.immediate,&GPR[Opcode.BRANCH.rs].W[0],GPR_NameLo[Opcode.BRANCH.rs]);
		SetbVariable(&BranchCompare,"BranchCompare");
		CPU_Message("");
		CPU_Message("      Continue:");
		*((BYTE *)(Jump[1]))=(BYTE)(RecompPos - Jump[1] - 1);
		Map_GPR_32bit(Section,Opcode.BRANCH.rt,FALSE, -1);
		MoveVariableToX86reg(&BranchCompare,"BranchCompare",MipsRegLo(Opcode.BRANCH.rt));*/
	}
}

void Compile_R4300i_SLTI (BLOCK_SECTION * Section) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
	if (Opcode.BRANCH.rt == 0) { return; }

	if (IsConst(Opcode.BRANCH.rs)) { 
		DWORD Result = 0x0;

		if (Is64Bit(Opcode.BRANCH.rs)) {
			_int64 Immediate = (_int64)((short)Opcode.IMM.immediate);
			Result = (_int64)MipsReg(Opcode.BRANCH.rs) < Immediate?1:0;
		} else if (Is32Bit(Opcode.BRANCH.rs)) {
			Result = MipsRegLo_S(Opcode.BRANCH.rs) < (short)Opcode.IMM.immediate?1:0;
		}
		UnMap_GPR(Section,Opcode.BRANCH.rt, FALSE);
		MipsRegState(Opcode.BRANCH.rt) = STATE_CONST_32;
		MipsRegLo(Opcode.BRANCH.rt) = Result;
	} else if (IsMapped(Opcode.BRANCH.rs)) { 
		if (Is64Bit(Opcode.BRANCH.rs)) {
			BYTE * Jump[2];

			CompConstToX86reg(MipsRegHi(Opcode.BRANCH.rs),((short)Opcode.IMM.immediate >> 31));
			JeLabel8("Low Compare",0);
			Jump[0] = RecompPos - 1;
			SetlVariable(&BranchCompare,"BranchCompare");
			JmpLabel8("Continue",0);
			Jump[1] = RecompPos - 1;
			CPU_Message("");
			CPU_Message("      Low Compare:");
			*((BYTE *)(Jump[0]))=(BYTE)(RecompPos - Jump[0] - 1);
			CompConstToX86reg(MipsRegLo(Opcode.BRANCH.rs),(short)Opcode.IMM.immediate);
			SetbVariable(&BranchCompare,"BranchCompare");
			CPU_Message("");
			CPU_Message("      Continue:");
			*((BYTE *)(Jump[1]))=(BYTE)(RecompPos - Jump[1] - 1);
			Map_GPR_32bit(Section,Opcode.BRANCH.rt,FALSE, -1);
			MoveVariableToX86reg(&BranchCompare,"BranchCompare",MipsRegLo(Opcode.BRANCH.rt));
		} else {
		/*	CompConstToX86reg(MipsRegLo(Opcode.BRANCH.rs),(short)Opcode.IMM.immediate);
			SetlVariable(&BranchCompare,"BranchCompare");
			Map_GPR_32bit(Section,Opcode.BRANCH.rt,FALSE, -1);
			MoveVariableToX86reg(&BranchCompare,"BranchCompare",MipsRegLo(Opcode.BRANCH.rt));
			*/
			ProtectGPR(Section, Opcode.BRANCH.rs);
			Map_GPR_32bit(Section,Opcode.BRANCH.rt,FALSE, -1);
			CompConstToX86reg(MipsRegLo(Opcode.BRANCH.rs),(short)Opcode.IMM.immediate);
			
			if (MipsRegLo(Opcode.BRANCH.rt) > x86_EDX) {
				SetlVariable(&BranchCompare,"BranchCompare");
				MoveVariableToX86reg(&BranchCompare,"BranchCompare",MipsRegLo(Opcode.BRANCH.rt));
			} else {
				Setl(MipsRegLo(Opcode.BRANCH.rt));
				AndConstToX86Reg(MipsRegLo(Opcode.BRANCH.rt), 1);
			}
		}
	} else {
		BYTE * Jump[2];

		CompConstToVariable(((short)Opcode.IMM.immediate >> 31),&GPR[Opcode.BRANCH.rs].W[1],GPR_NameHi[Opcode.BRANCH.rs]);
		JeLabel8("Low Compare",0);
		Jump[0] = RecompPos - 1;
		SetlVariable(&BranchCompare,"BranchCompare");
		JmpLabel8("Continue",0);
		Jump[1] = RecompPos - 1;
		CPU_Message("");
		CPU_Message("      Low Compare:");
		*((BYTE *)(Jump[0]))=(BYTE)(RecompPos - Jump[0] - 1);
		CompConstToVariable((short)Opcode.IMM.immediate,&GPR[Opcode.BRANCH.rs].W[0],GPR_NameLo[Opcode.BRANCH.rs]);
		SetbVariable(&BranchCompare,"BranchCompare");
		CPU_Message("");
		CPU_Message("      Continue:");
		*((BYTE *)(Jump[1]))=(BYTE)(RecompPos - Jump[1] - 1);
		Map_GPR_32bit(Section,Opcode.BRANCH.rt,FALSE, -1);
		MoveVariableToX86reg(&BranchCompare,"BranchCompare",MipsRegLo(Opcode.BRANCH.rt));
	}
}

void Compile_R4300i_ANDI (BLOCK_SECTION * Section) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));

	if (Opcode.BRANCH.rt == 0) { return;}

	if (IsConst(Opcode.BRANCH.rs)) {
		if (IsMapped(Opcode.BRANCH.rt)) { UnMap_GPR(Section,Opcode.BRANCH.rt, FALSE); }
		MipsRegState(Opcode.BRANCH.rt) = STATE_CONST_32;
		MipsRegLo(Opcode.BRANCH.rt) = MipsRegLo(Opcode.BRANCH.rs) & Opcode.IMM.immediate;
	} else if (Opcode.IMM.immediate != 0) { 
		Map_GPR_32bit(Section,Opcode.BRANCH.rt,FALSE,Opcode.BRANCH.rs);
		AndConstToX86Reg(MipsRegLo(Opcode.BRANCH.rt),Opcode.IMM.immediate);
	} else {
		Map_GPR_32bit(Section,Opcode.BRANCH.rt,FALSE,0);
	}
}

void Compile_R4300i_ORI (BLOCK_SECTION * Section) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
	if (Opcode.BRANCH.rt == 0) { return;}

	if (IsConst(Opcode.BRANCH.rs)) {
		if (IsMapped(Opcode.BRANCH.rt)) { UnMap_GPR(Section,Opcode.BRANCH.rt, FALSE); }
		MipsRegState(Opcode.BRANCH.rt) = MipsRegState(Opcode.BRANCH.rs);
		MipsRegHi(Opcode.BRANCH.rt) = MipsRegHi(Opcode.BRANCH.rs);
		MipsRegLo(Opcode.BRANCH.rt) = MipsRegLo(Opcode.BRANCH.rs) | Opcode.IMM.immediate;
	} else if (IsMapped(Opcode.BRANCH.rs)) {
		if (Is64Bit(Opcode.BRANCH.rs)) {
			Map_GPR_64bit(Section,Opcode.BRANCH.rt,Opcode.BRANCH.rs);
		} else {
			Map_GPR_32bit(Section,Opcode.BRANCH.rt,IsSigned(Opcode.BRANCH.rs),Opcode.BRANCH.rs);
		}
		OrConstToX86Reg(Opcode.IMM.immediate,MipsRegLo(Opcode.BRANCH.rt));
	} else {
		Map_GPR_64bit(Section,Opcode.BRANCH.rt,Opcode.BRANCH.rs);
		OrConstToX86Reg(Opcode.IMM.immediate,MipsRegLo(Opcode.BRANCH.rt));
	}
}

void Compile_R4300i_XORI (BLOCK_SECTION * Section) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
	if (Opcode.BRANCH.rt == 0) { return;}

	if (IsConst(Opcode.BRANCH.rs)) {
		if (Opcode.BRANCH.rs != Opcode.BRANCH.rt) { UnMap_GPR(Section,Opcode.BRANCH.rt, FALSE); }
		MipsRegState(Opcode.BRANCH.rt) = MipsRegState(Opcode.BRANCH.rs);
		MipsRegHi(Opcode.BRANCH.rt) = MipsRegHi(Opcode.BRANCH.rs);
		MipsRegLo(Opcode.BRANCH.rt) = MipsRegLo(Opcode.BRANCH.rs) ^ Opcode.IMM.immediate;
	} else {
		if (IsMapped(Opcode.BRANCH.rs) && Is32Bit(Opcode.BRANCH.rs)) {
			Map_GPR_32bit(Section,Opcode.BRANCH.rt,IsSigned(Opcode.BRANCH.rs),Opcode.BRANCH.rs);
		} else {
			Map_GPR_64bit(Section,Opcode.BRANCH.rt,Opcode.BRANCH.rs);
		}
		if (Opcode.IMM.immediate != 0) { XorConstToX86Reg(MipsRegLo(Opcode.BRANCH.rt),Opcode.IMM.immediate); }
	}
}

void Compile_R4300i_LUI (BLOCK_SECTION * Section) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
	if (Opcode.BRANCH.rt == 0) { return;}

	if (SPHack && Opcode.BRANCH.rt == 29) {
		DWORD Address = ((short)Opcode.BRANCH.offset << 16);
		int x86reg = Map_MemoryStack(Section, FALSE);
		TranslateVaddr (&Address);
		if (x86reg < 0) {
			MoveConstToVariable((DWORD)(Address + N64MEM), &MemoryStack, "MemoryStack");
		} else {
			MoveConstToX86reg((DWORD)(Address + N64MEM), x86reg);
		}
	}
	UnMap_GPR(Section,Opcode.BRANCH.rt, FALSE);
	MipsRegLo(Opcode.BRANCH.rt) = ((short)Opcode.BRANCH.offset << 16);
	MipsRegState(Opcode.BRANCH.rt) = STATE_CONST_32;
}

void Compile_R4300i_DADDI (BLOCK_SECTION * Section) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));

	if (Opcode.BRANCH.rt == 0) { return; }
	if (Opcode.BRANCH.rs != 0) { UnMap_GPR(Section,Opcode.BRANCH.rs,TRUE); }
	UnMap_GPR(Section,Opcode.BRANCH.rt,TRUE);
	Pushad();
	MoveConstToVariable(Opcode.Hex, &Opcode.Hex, "Opcode.Hex" );
	Call_Direct(r4300i_DADDI, "r4300i_DADDI");
	Popad();
}

void Compile_R4300i_DADDIU (BLOCK_SECTION * Section) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));

	if (Opcode.BRANCH.rt == 0) { return; }
	if (Opcode.BRANCH.rs != 0) { UnMap_GPR(Section,Opcode.BRANCH.rs,TRUE); }
	UnMap_GPR(Section,Opcode.BRANCH.rt,TRUE);
	Pushad();
	MoveConstToVariable(Opcode.Hex, &Opcode.Hex, "Opcode.Hex" );
	Call_Direct(r4300i_DADDIU, "r4300i_DADDIU");
	Popad();
}

void Compile_R4300i_LDL (BLOCK_SECTION * Section) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));

	if (Opcode.BRANCH.rt == 0) { return; }
	if (Opcode.IMM.base != 0) { UnMap_GPR(Section,Opcode.IMM.base,TRUE); }
	UnMap_GPR(Section,Opcode.BRANCH.rt,TRUE);
	Pushad();
	MoveConstToVariable(Opcode.Hex, &Opcode.Hex, "Opcode.Hex" );
	Call_Direct(r4300i_LDL, "r4300i_LDL");
	Popad();

}

void Compile_R4300i_LDR (BLOCK_SECTION * Section) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));

	if (Opcode.BRANCH.rt == 0) { return; }
	if (Opcode.IMM.base != 0) { UnMap_GPR(Section,Opcode.IMM.base,TRUE); }
	UnMap_GPR(Section,Opcode.BRANCH.rt,TRUE);
	Pushad();
	MoveConstToVariable(Opcode.Hex, &Opcode.Hex, "Opcode.Hex" );
	Call_Direct(r4300i_LDR, "r4300i_LDR");
	Popad();
}


void Compile_R4300i_LB (BLOCK_SECTION * Section) {
	DWORD TempReg1, TempReg2;
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));

	if (Opcode.BRANCH.rt == 0) return;

	if (IsConst(Opcode.IMM.base)) { 
		DWORD Address = (MipsRegLo(Opcode.IMM.base) + (short)Opcode.IMM.immediate) ^ 3;
		Map_GPR_32bit(Section,Opcode.BRANCH.rt,TRUE,0);
		if (Compile_LB(MipsRegLo(Opcode.BRANCH.rt), Address, TRUE)) {
			return;
		}

		// Deoptimization: Address translation for a constant virtual address failed.
		// Unmap the base register to force it to be loaded from memory by the following codegen.
		UnMap_GPR(Section, Opcode.IMM.base, TRUE);
	}
	if (IsMapped(Opcode.BRANCH.rt)) { ProtectGPR(Section,Opcode.BRANCH.rt); }
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
		MoveVariableDispToX86Reg(TLB_ReadMap,"TLB_ReadMap",TempReg2,TempReg2,4);
		CompileReadTLBMiss(Section,TempReg1,TempReg2);
		XorConstToX86Reg(TempReg1,3);	
		Map_GPR_32bit(Section,Opcode.BRANCH.rt,TRUE,-1);
		MoveSxByteX86regPointerToX86reg(TempReg1, TempReg2,MipsRegLo(Opcode.BRANCH.rt));
	} else {
		AndConstToX86Reg(TempReg1,0x1FFFFFFF);
		XorConstToX86Reg(TempReg1,3);
		Map_GPR_32bit(Section,Opcode.BRANCH.rt,TRUE,-1);
		MoveSxN64MemToX86regByte(MipsRegLo(Opcode.BRANCH.rt), TempReg1);
	}
}

void Compile_R4300i_LH (BLOCK_SECTION * Section) {
	DWORD TempReg1, TempReg2;
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));

	if (Opcode.BRANCH.rt == 0) return;

	if (IsConst(Opcode.IMM.base)) { 
		DWORD Address = (MipsRegLo(Opcode.IMM.base) + (short)Opcode.IMM.immediate) ^ 2;
		Map_GPR_32bit(Section,Opcode.BRANCH.rt,TRUE,0);
		Compile_LH(MipsRegLo(Opcode.BRANCH.rt),Address,TRUE);
		return;
	}
	if (IsMapped(Opcode.BRANCH.rt)) { ProtectGPR(Section,Opcode.BRANCH.rt); }
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
		MoveVariableDispToX86Reg(TLB_ReadMap,"TLB_ReadMap",TempReg2,TempReg2,4);
		CompileReadTLBMiss(Section,TempReg1,TempReg2);
		XorConstToX86Reg(TempReg1,2);	
		Map_GPR_32bit(Section,Opcode.BRANCH.rt,TRUE,-1);
		MoveSxHalfX86regPointerToX86reg(TempReg1, TempReg2,MipsRegLo(Opcode.BRANCH.rt));
	} else {
		AndConstToX86Reg(TempReg1,0x1FFFFFFF);
		XorConstToX86Reg(TempReg1,2);
		Map_GPR_32bit(Section,Opcode.BRANCH.rt,TRUE,-1);
		MoveSxN64MemToX86regHalf(MipsRegLo(Opcode.BRANCH.rt), TempReg1);
	}
}

void Compile_R4300i_LWL (BLOCK_SECTION * Section) {
	DWORD TempReg1, TempReg2 = 0x0, Offset, shift;
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));

	if (Opcode.BRANCH.rt == 0) return;

	if (IsConst(Opcode.IMM.base)) {
		DWORD Address, Value;

		Address = MipsRegLo(Opcode.IMM.base) + (short)Opcode.IMM.immediate;
		Offset = Address & 3;

		Map_GPR_32bit(Section, Opcode.BRANCH.rt, TRUE, Opcode.BRANCH.rt);
		Value = Map_TempReg(Section, x86_Any, -1, FALSE);
		if (Compile_LW(Value, (Address & ~3))) {
			AndConstToX86Reg(MipsRegLo(Opcode.BRANCH.rt), LWL_MASK[Offset]);
			ShiftLeftSignImmed(Value, (BYTE)LWL_SHIFT[Offset]);
			AddX86RegToX86Reg(MipsRegLo(Opcode.BRANCH.rt), Value);
			return;
		}

		// Deoptimization: Address translation for a constant virtual address failed.
		// Unmap the base register to force it to be loaded from memory by the following codegen.
		UnMap_GPR(Section, Opcode.IMM.base, TRUE);
	}

	shift = Map_TempReg(Section,x86_ECX,-1,FALSE);
	if (IsMapped(Opcode.BRANCH.rt)) { ProtectGPR(Section,Opcode.BRANCH.rt); }
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
	if (UseTlb) {
		TempReg2 = Map_TempReg(Section,x86_Any,-1,FALSE);
		MoveX86RegToX86Reg(TempReg1, TempReg2);
		ShiftRightUnsignImmed(TempReg2,12);
		MoveVariableDispToX86Reg(TLB_ReadMap,"TLB_ReadMap",TempReg2,TempReg2,4);
		
		CompileReadTLBMiss(Section,TempReg1,TempReg2);
	}
	Offset = Map_TempReg(Section,x86_Any,-1,FALSE);
	MoveX86RegToX86Reg(TempReg1, Offset);
	AndConstToX86Reg(Offset,3);
	AndConstToX86Reg(TempReg1,(DWORD)~3);

	Map_GPR_32bit(Section,Opcode.BRANCH.rt,TRUE,Opcode.BRANCH.rt);
	AndVariableDispToX86Reg(LWL_MASK,"LWL_MASK",MipsRegLo(Opcode.BRANCH.rt),Offset,4);
	MoveVariableDispToX86Reg(LWL_SHIFT,"LWL_SHIFT",shift,Offset,4);
	if (UseTlb) {			
		MoveX86regPointerToX86reg(TempReg1, TempReg2,TempReg1);
	} else {
		AndConstToX86Reg(TempReg1,0x1FFFFFFF);
		MoveN64MemToX86reg(TempReg1,TempReg1);
	}
	ShiftLeftSign(TempReg1);
	AddX86RegToX86Reg(MipsRegLo(Opcode.BRANCH.rt),TempReg1);
}

void Compile_R4300i_LW (BLOCK_SECTION * Section) {
	DWORD TempReg1, TempReg2;
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));

	if (Opcode.BRANCH.rt == 0) return;

	if (Opcode.IMM.base == 29 && SPHack) {
		char String[100];

		Map_GPR_32bit(Section,Opcode.BRANCH.rt,TRUE,-1);
		TempReg1 = Map_MemoryStack(Section,TRUE);
		sprintf(String,"%Xh",(short)Opcode.BRANCH.offset);
		MoveVariableDispToX86Reg((void *)((DWORD)(short)Opcode.BRANCH.offset),String,MipsRegLo(Opcode.BRANCH.rt),TempReg1,1);
	} else {
		if (IsConst(Opcode.IMM.base)) { 
			DWORD Address = MipsRegLo(Opcode.IMM.base) + (short)Opcode.BRANCH.offset;
			Map_GPR_32bit(Section,Opcode.BRANCH.rt,TRUE,-1);
			if (Compile_LW(MipsRegLo(Opcode.BRANCH.rt), Address)) {
				return;
			}

			// Deoptimization: Address translation for a constant virtual address failed.
			// Unmap the base register to force it to be loaded from memory by the following codegen.
			UnMap_GPR(Section, Opcode.IMM.base, TRUE);
		}
		if (UseTlb) {	
			if (IsMapped(Opcode.BRANCH.rt)) { ProtectGPR(Section,Opcode.BRANCH.rt); }
			if (IsMapped(Opcode.IMM.base) && Opcode.BRANCH.offset == 0) { 
				ProtectGPR(Section,Opcode.IMM.base);
				TempReg1 = MipsRegLo(Opcode.IMM.base);
			} else {
				if (IsMapped(Opcode.IMM.base)) { 
					ProtectGPR(Section,Opcode.IMM.base);
					if (Opcode.BRANCH.offset != 0) {
						TempReg1 = Map_TempReg(Section,x86_Any,-1,FALSE);
						LeaSourceAndOffset(TempReg1,MipsRegLo(Opcode.IMM.base),(short)Opcode.BRANCH.offset);
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
			MoveX86RegToX86Reg(TempReg1, TempReg2);
			ShiftRightUnsignImmed(TempReg2,12);
			MoveVariableDispToX86Reg(TLB_ReadMap,"TLB_ReadMap",TempReg2,TempReg2,4);
			CompileReadTLBMiss(Section,TempReg1,TempReg2);
			Map_GPR_32bit(Section,Opcode.BRANCH.rt,TRUE,-1);
			MoveX86regPointerToX86reg(TempReg1, TempReg2,MipsRegLo(Opcode.BRANCH.rt));
		} else {
			if (IsMapped(Opcode.IMM.base)) { 
				ProtectGPR(Section,Opcode.IMM.base);
				if (Opcode.BRANCH.offset != 0) {
					Map_GPR_32bit(Section,Opcode.BRANCH.rt,TRUE,-1);
					LeaSourceAndOffset(MipsRegLo(Opcode.BRANCH.rt),MipsRegLo(Opcode.IMM.base),(short)Opcode.BRANCH.offset);
				} else {
					Map_GPR_32bit(Section,Opcode.BRANCH.rt,TRUE,Opcode.IMM.base);
				}
			} else {
				Map_GPR_32bit(Section,Opcode.BRANCH.rt,TRUE,Opcode.IMM.base);
				if (Opcode.IMM.immediate == 0) { 
				} else if (Opcode.IMM.immediate == 1) {
					IncX86reg(MipsRegLo(Opcode.BRANCH.rt));
				} else if (Opcode.IMM.immediate == 0xFFFF) {			
					DecX86reg(MipsRegLo(Opcode.BRANCH.rt));
				} else {
					AddConstToX86Reg(MipsRegLo(Opcode.BRANCH.rt),(short)Opcode.IMM.immediate);
				}
			}
			AndConstToX86Reg(MipsRegLo(Opcode.BRANCH.rt),0x1FFFFFFF);
			MoveN64MemToX86reg(MipsRegLo(Opcode.BRANCH.rt),MipsRegLo(Opcode.BRANCH.rt));
		}
	}
}

void Compile_R4300i_LBU (BLOCK_SECTION * Section) {
	DWORD TempReg1, TempReg2;
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));

	if (Opcode.BRANCH.rt == 0) return;

	if (IsConst(Opcode.IMM.base)) { 
		DWORD Address = (MipsRegLo(Opcode.IMM.base) + (short)Opcode.BRANCH.offset) ^ 3;
		Map_GPR_32bit(Section,Opcode.BRANCH.rt,FALSE,0);
		if (Compile_LB(MipsRegLo(Opcode.BRANCH.rt), Address, FALSE)) {
			return;
		}

		// Deoptimization: Address translation for a constant virtual address failed.
		// Unmap the base register to force it to be loaded from memory by the following codegen.
		UnMap_GPR(Section, Opcode.IMM.base, TRUE);
	}
	if (IsMapped(Opcode.BRANCH.rt)) { ProtectGPR(Section,Opcode.BRANCH.rt); }
	if (IsMapped(Opcode.IMM.base)) { 
		ProtectGPR(Section,Opcode.IMM.base);
		if (Opcode.BRANCH.offset != 0) {
			TempReg1 = Map_TempReg(Section,x86_Any,-1,FALSE);
			LeaSourceAndOffset(TempReg1,MipsRegLo(Opcode.IMM.base),(short)Opcode.BRANCH.offset);
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
		MoveVariableDispToX86Reg(TLB_ReadMap,"TLB_ReadMap",TempReg2,TempReg2,4);
		CompileReadTLBMiss(Section,TempReg1,TempReg2);
		XorConstToX86Reg(TempReg1,3);	
		Map_GPR_32bit(Section,Opcode.BRANCH.rt,FALSE,-1);
		MoveZxByteX86regPointerToX86reg(TempReg1, TempReg2,MipsRegLo(Opcode.BRANCH.rt));
	} else {
		AndConstToX86Reg(TempReg1,0x1FFFFFFF);
		XorConstToX86Reg(TempReg1,3);
		Map_GPR_32bit(Section,Opcode.BRANCH.rt,FALSE,-1);
		MoveZxN64MemToX86regByte(MipsRegLo(Opcode.BRANCH.rt), TempReg1);
	}
}

void Compile_R4300i_LHU (BLOCK_SECTION * Section) {
	DWORD TempReg1, TempReg2;
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));

	if (Opcode.BRANCH.rt == 0) return;

	if (IsConst(Opcode.IMM.base)) { 
		DWORD Address = (MipsRegLo(Opcode.IMM.base) + (short)Opcode.BRANCH.offset) ^ 2;
		Map_GPR_32bit(Section,Opcode.BRANCH.rt,FALSE,0);
		Compile_LH(MipsRegLo(Opcode.BRANCH.rt),Address,FALSE);
		return;
	}
	if (IsMapped(Opcode.BRANCH.rt)) { ProtectGPR(Section,Opcode.BRANCH.rt); }
	if (IsMapped(Opcode.IMM.base)) { 
		ProtectGPR(Section,Opcode.IMM.base);
		if (Opcode.BRANCH.offset != 0) {
			TempReg1 = Map_TempReg(Section,x86_Any,-1,FALSE);
			LeaSourceAndOffset(TempReg1,MipsRegLo(Opcode.IMM.base),(short)Opcode.BRANCH.offset);
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
		MoveVariableDispToX86Reg(TLB_ReadMap,"TLB_ReadMap",TempReg2,TempReg2,4);
		CompileReadTLBMiss(Section,TempReg1,TempReg2);
		XorConstToX86Reg(TempReg1,2);	
		Map_GPR_32bit(Section,Opcode.BRANCH.rt,FALSE,-1);
		MoveZxHalfX86regPointerToX86reg(TempReg1, TempReg2,MipsRegLo(Opcode.BRANCH.rt));
	} else {
		AndConstToX86Reg(TempReg1,0x1FFFFFFF);
		XorConstToX86Reg(TempReg1,2);
		Map_GPR_32bit(Section,Opcode.BRANCH.rt,TRUE,-1);
		MoveZxN64MemToX86regHalf(MipsRegLo(Opcode.BRANCH.rt), TempReg1);
	}
}

void Compile_R4300i_LWR (BLOCK_SECTION * Section) {
	DWORD TempReg1, TempReg2 = 0x0, Offset, shift;
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));

	if (Opcode.BRANCH.rt == 0) return;

	if (IsConst(Opcode.IMM.base)) { 
		DWORD Address, Value;
		
		Address = MipsRegLo(Opcode.IMM.base) + (short)Opcode.BRANCH.offset;
		Offset  = Address & 3;

		Map_GPR_32bit(Section,Opcode.BRANCH.rt,TRUE,Opcode.BRANCH.rt);
		Value = Map_TempReg(Section,x86_Any,-1,FALSE);
		if (Compile_LW(Value, (Address & ~3))) {
			AndConstToX86Reg(MipsRegLo(Opcode.BRANCH.rt), LWR_MASK[Offset]);
			ShiftRightUnsignImmed(Value, (BYTE)LWR_SHIFT[Offset]);
			AddX86RegToX86Reg(MipsRegLo(Opcode.BRANCH.rt), Value);
			return;
		}

		// Deoptimization: Address translation for a constant virtual address failed.
		// Unmap the base register to force it to be loaded from memory by the following codegen.
		UnMap_GPR(Section, Opcode.IMM.base, TRUE);
	}

	shift = Map_TempReg(Section,x86_ECX,-1,FALSE);
	if (IsMapped(Opcode.BRANCH.rt)) { ProtectGPR(Section,Opcode.BRANCH.rt); }
	if (IsMapped(Opcode.IMM.base)) { 
		ProtectGPR(Section,Opcode.IMM.base);
		if (Opcode.BRANCH.offset != 0) {
			TempReg1 = Map_TempReg(Section,x86_Any,-1,FALSE);
			LeaSourceAndOffset(TempReg1,MipsRegLo(Opcode.IMM.base),(short)Opcode.BRANCH.offset);
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
	
	if (UseTlb) {
		TempReg2 = Map_TempReg(Section,x86_Any,-1,FALSE);
		MoveX86RegToX86Reg(TempReg1, TempReg2);
		ShiftRightUnsignImmed(TempReg2,12);
		MoveVariableDispToX86Reg(TLB_ReadMap,"TLB_ReadMap",TempReg2,TempReg2,4);
		
		CompileReadTLBMiss(Section,TempReg1,TempReg2);
	}
	Offset = Map_TempReg(Section,x86_Any,-1,FALSE);
	MoveX86RegToX86Reg(TempReg1, Offset);
	AndConstToX86Reg(Offset,3);
	AndConstToX86Reg(TempReg1,(DWORD)~3);

	Map_GPR_32bit(Section,Opcode.BRANCH.rt,TRUE,Opcode.BRANCH.rt);
	AndVariableDispToX86Reg(LWR_MASK,"LWR_MASK",MipsRegLo(Opcode.BRANCH.rt),Offset,4);
	MoveVariableDispToX86Reg(LWR_SHIFT,"LWR_SHIFT",shift,Offset,4);
	if (UseTlb) {
		MoveX86regPointerToX86reg(TempReg1, TempReg2,TempReg1);
	} else {
		AndConstToX86Reg(TempReg1,0x1FFFFFFF);
		MoveN64MemToX86reg(TempReg1,TempReg1);
	}
	ShiftRightUnsign(TempReg1);
	AddX86RegToX86Reg(MipsRegLo(Opcode.BRANCH.rt),TempReg1);
}

void Compile_R4300i_LWU (BLOCK_SECTION * Section) {
	DWORD TempReg1, TempReg2;
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));

	if (Opcode.BRANCH.rt == 0) return;

	if (IsConst(Opcode.IMM.base)) { 
		DWORD Address = (MipsRegLo(Opcode.IMM.base) + (short)Opcode.BRANCH.offset);
		Map_GPR_32bit(Section,Opcode.BRANCH.rt,FALSE,-1);
		if (Compile_LW(MipsRegLo(Opcode.BRANCH.rt), Address)) {
			return;
		}

		// Deoptimization: Address translation for a constant virtual address failed.
		// Unmap the base register to force it to be loaded from memory by the following codegen.
		UnMap_GPR(Section, Opcode.IMM.base, TRUE);
	}
	if (UseTlb) {
		if (IsMapped(Opcode.BRANCH.rt)) { ProtectGPR(Section,Opcode.BRANCH.rt); }
		if (IsMapped(Opcode.IMM.base) && Opcode.BRANCH.offset == 0) {
			ProtectGPR(Section, Opcode.IMM.base);
			TempReg1 = MipsRegLo(Opcode.IMM.base);
		} else {
			if (IsMapped(Opcode.IMM.base)) {
				ProtectGPR(Section, Opcode.IMM.base);
				if (Opcode.BRANCH.offset != 0) {
					TempReg1 = Map_TempReg(Section, x86_Any, -1, FALSE);
					LeaSourceAndOffset(TempReg1, MipsRegLo(Opcode.IMM.base), (short)Opcode.BRANCH.offset);
				} else {
					TempReg1 = Map_TempReg(Section, x86_Any, Opcode.IMM.base, FALSE);
				}
			} else {
				TempReg1 = Map_TempReg(Section, x86_Any, Opcode.IMM.base, FALSE);
				if (Opcode.IMM.immediate == 0) {
				} else if (Opcode.IMM.immediate == 1) {
					IncX86reg(TempReg1);
				} else if (Opcode.IMM.immediate == 0xFFFF) {
					DecX86reg(TempReg1);
				} else {
					AddConstToX86Reg(TempReg1, (short)Opcode.IMM.immediate);
				}
			}
		}
		TempReg2 = Map_TempReg(Section, x86_Any, -1, FALSE);
		MoveX86RegToX86Reg(TempReg1, TempReg2);
		ShiftRightUnsignImmed(TempReg2, 12);
		MoveVariableDispToX86Reg(TLB_ReadMap, "TLB_ReadMap", TempReg2, TempReg2, 4);
		CompileReadTLBMiss(Section,TempReg1, TempReg2);
		Map_GPR_32bit(Section,Opcode.BRANCH.rt, FALSE, -1);
		MoveX86regPointerToX86reg(TempReg1, TempReg2, MipsRegLo(Opcode.BRANCH.rt));
	} else {
		if (IsMapped(Opcode.IMM.base)) {
			ProtectGPR(Section, Opcode.IMM.base);
			if (Opcode.BRANCH.offset != 0) {
				Map_GPR_32bit(Section, Opcode.BRANCH.rt, FALSE, -1);
				LeaSourceAndOffset(MipsRegLo(Opcode.BRANCH.rt), MipsRegLo(Opcode.IMM.base), (short)Opcode.BRANCH.offset);
			} else {
				Map_GPR_32bit(Section, Opcode.BRANCH.rt, FALSE, Opcode.IMM.base);
			}
		} else {
			Map_GPR_32bit(Section, Opcode.BRANCH.rt, FALSE, Opcode.IMM.base);
			if (Opcode.IMM.immediate == 0) {
			} else if (Opcode.IMM.immediate == 1) {
				IncX86reg(MipsRegLo(Opcode.BRANCH.rt));
			} else if (Opcode.IMM.immediate == 0xFFFF) {
				DecX86reg(MipsRegLo(Opcode.BRANCH.rt));
			} else {
				AddConstToX86Reg(MipsRegLo(Opcode.BRANCH.rt), (short)Opcode.IMM.immediate);
			}
		}
		AndConstToX86Reg(MipsRegLo(Opcode.BRANCH.rt), 0x1FFFFFFF);
		MoveN64MemToX86reg(MipsRegLo(Opcode.BRANCH.rt), MipsRegLo(Opcode.BRANCH.rt));
	}
}

void Compile_R4300i_SB (BLOCK_SECTION * Section){
	DWORD TempReg1, TempReg2;
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
	
	if (IsConst(Opcode.IMM.base)) { 
		DWORD Address = (MipsRegLo(Opcode.IMM.base) + (short)Opcode.BRANCH.offset) ^ 3;
		
		if (IsConst(Opcode.BRANCH.rt)) {
			if (Compile_SB_Const((BYTE)MipsRegLo(Opcode.BRANCH.rt), Address)) {
				return;
			}
		} else if (IsMapped(Opcode.BRANCH.rt) && Is8BitReg(MipsRegLo(Opcode.BRANCH.rt))) {
			if (Compile_SB_Register(MipsRegLo(Opcode.BRANCH.rt), Address)) {
				return;
			}
		} else {
			if (Compile_SB_Register(Map_TempReg(Section, x86_Any8Bit, Opcode.BRANCH.rt, FALSE), Address)) {
				return;
			}
		}

		// Deoptimization: Address translation for a constant virtual address failed.
		// Unmap the base register to force it to be loaded from memory by the following codegen.
		UnMap_GPR(Section, Opcode.IMM.base, TRUE);
	}
	if (IsMapped(Opcode.BRANCH.rt)) { ProtectGPR(Section,Opcode.BRANCH.rt); }
	if (IsMapped(Opcode.IMM.base)) { 
		ProtectGPR(Section,Opcode.IMM.base);
		if (Opcode.BRANCH.offset != 0) {
			TempReg1 = Map_TempReg(Section,x86_Any,-1,FALSE);
			LeaSourceAndOffset(TempReg1,MipsRegLo(Opcode.IMM.base),(short)Opcode.BRANCH.offset);
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
	if (UseTlb) {
		TempReg2 = Map_TempReg(Section,x86_Any,-1,FALSE);
		MoveX86RegToX86Reg(TempReg1, TempReg2);
		ShiftRightUnsignImmed(TempReg2,12);
		MoveVariableDispToX86Reg(TLB_WriteMap,"TLB_WriteMap",TempReg2,TempReg2,4);
		CompileWriteTLBMiss(Section, TempReg1, TempReg2);
		//For tlb miss
		//0041C522 85 C0                test        eax,eax
		//0041C524 75 01                jne         0041C527

		XorConstToX86Reg(TempReg1,3);	
		if (IsConst(Opcode.BRANCH.rt)) {
			MoveConstByteToX86regPointer((BYTE)MipsRegLo(Opcode.BRANCH.rt),TempReg1, TempReg2);
		} else if (IsMapped(Opcode.BRANCH.rt) && Is8BitReg(MipsRegLo(Opcode.BRANCH.rt))) {
			MoveX86regByteToX86regPointer(MipsRegLo(Opcode.BRANCH.rt),TempReg1, TempReg2);
		} else {	
			UnProtectGPR(Section,Opcode.BRANCH.rt);
			MoveX86regByteToX86regPointer(Map_TempReg(Section,x86_Any8Bit,Opcode.BRANCH.rt,FALSE),TempReg1, TempReg2);
		}
	} else {
		AndConstToX86Reg(TempReg1,0x1FFFFFFF);
		XorConstToX86Reg(TempReg1,3);
		if (IsConst(Opcode.BRANCH.rt)) {
			MoveConstByteToN64Mem((BYTE)MipsRegLo(Opcode.BRANCH.rt),TempReg1);
		} else if (IsMapped(Opcode.BRANCH.rt) && Is8BitReg(MipsRegLo(Opcode.BRANCH.rt))) {
			MoveX86regByteToN64Mem(MipsRegLo(Opcode.BRANCH.rt),TempReg1);
		} else {	
			UnProtectGPR(Section,Opcode.BRANCH.rt);
			MoveX86regByteToN64Mem(Map_TempReg(Section,x86_Any8Bit,Opcode.BRANCH.rt,FALSE),TempReg1);
		}
	}

}

void Compile_R4300i_SH (BLOCK_SECTION * Section){
	DWORD TempReg1, TempReg2;
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
	
	if (IsConst(Opcode.IMM.base)) { 
		DWORD Address = (MipsRegLo(Opcode.IMM.base) + (short)Opcode.BRANCH.offset) ^ 2;
		
		if (IsConst(Opcode.BRANCH.rt)) {
			if (Compile_SH_Const((WORD)MipsRegLo(Opcode.BRANCH.rt), Address)) {
				return;
			}
		} else if (IsMapped(Opcode.BRANCH.rt)) {
			if (Compile_SH_Register(MipsRegLo(Opcode.BRANCH.rt), Address)) {
				return;
			}
		} else {
			if (Compile_SH_Register(Map_TempReg(Section, x86_Any, Opcode.BRANCH.rt, FALSE), Address)) {
				return;
			}
		}

		// Deoptimization: Address translation for a constant virtual address failed.
		// Unmap the base register to force it to be loaded from memory by the following codegen.
		UnMap_GPR(Section, Opcode.IMM.base, TRUE);
	}
	if (IsMapped(Opcode.BRANCH.rt)) { ProtectGPR(Section,Opcode.BRANCH.rt); }
	if (IsMapped(Opcode.IMM.base)) { 
		ProtectGPR(Section,Opcode.IMM.base);
		if (Opcode.BRANCH.offset != 0) {
			TempReg1 = Map_TempReg(Section,x86_Any,-1,FALSE);
			LeaSourceAndOffset(TempReg1,MipsRegLo(Opcode.IMM.base),(short)Opcode.BRANCH.offset);
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
	if (UseTlb) {
		TempReg2 = Map_TempReg(Section,x86_Any,-1,FALSE);
		MoveX86RegToX86Reg(TempReg1, TempReg2);
		ShiftRightUnsignImmed(TempReg2,12);
		MoveVariableDispToX86Reg(TLB_WriteMap,"TLB_WriteMap",TempReg2,TempReg2,4);
		CompileWriteTLBMiss(Section, TempReg1, TempReg2);
		//For tlb miss
		//0041C522 85 C0                test        eax,eax
		//0041C524 75 01                jne         0041C527

		XorConstToX86Reg(TempReg1,2);	
		if (IsConst(Opcode.BRANCH.rt)) {
			MoveConstHalfToX86regPointer((WORD)MipsRegLo(Opcode.BRANCH.rt),TempReg1, TempReg2);
		} else if (IsMapped(Opcode.BRANCH.rt)) {
			MoveX86regHalfToX86regPointer(MipsRegLo(Opcode.BRANCH.rt),TempReg1, TempReg2);
		} else {	
			MoveX86regHalfToX86regPointer(Map_TempReg(Section,x86_Any,Opcode.BRANCH.rt,FALSE),TempReg1, TempReg2);
		}
	} else {
		AndConstToX86Reg(TempReg1,0x1FFFFFFF);		
		XorConstToX86Reg(TempReg1,2);
		if (IsConst(Opcode.BRANCH.rt)) {
			MoveConstHalfToN64Mem((WORD)MipsRegLo(Opcode.BRANCH.rt),TempReg1);
		} else if (IsMapped(Opcode.BRANCH.rt)) {
			MoveX86regHalfToN64Mem(MipsRegLo(Opcode.BRANCH.rt),TempReg1);		
		} else {	
			MoveX86regHalfToN64Mem(Map_TempReg(Section,x86_Any,Opcode.BRANCH.rt,FALSE),TempReg1);		
		}
	}
}

void Compile_R4300i_SWL (BLOCK_SECTION * Section) {
	DWORD TempReg1, TempReg2 = 0x0, Value, Offset, shift;
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));

	if (IsConst(Opcode.IMM.base)) { 
		DWORD Address;
	
		Address = MipsRegLo(Opcode.IMM.base) + (short)Opcode.BRANCH.offset;
		Offset  = Address & 3;
		
		Value = Map_TempReg(Section,x86_Any,-1,FALSE);
		if (Compile_LW(Value, (Address & ~3))) {
			AndConstToX86Reg(Value, SWL_MASK[Offset]);
			TempReg1 = Map_TempReg(Section, x86_Any, Opcode.BRANCH.rt, FALSE);
			ShiftRightUnsignImmed(TempReg1, (BYTE)SWL_SHIFT[Offset]);
			AddX86RegToX86Reg(Value, TempReg1);
			Compile_SW_Register(Value, (Address & ~3));
			return;
		}

		// Deoptimization: Address translation for a constant virtual address failed.
		// Unmap the base register to force it to be loaded from memory by the following codegen.
		UnMap_GPR(Section, Opcode.IMM.base, TRUE);
	}
	shift = Map_TempReg(Section,x86_ECX,-1,FALSE);
	if (IsMapped(Opcode.IMM.base)) { 
		ProtectGPR(Section,Opcode.IMM.base);
		if (Opcode.BRANCH.offset != 0) {
			TempReg1 = Map_TempReg(Section,x86_Any,-1,FALSE);
			LeaSourceAndOffset(TempReg1,MipsRegLo(Opcode.IMM.base),(short)Opcode.BRANCH.offset);
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
	if (UseTlb) {
		TempReg2 = Map_TempReg(Section,x86_Any,-1,FALSE);
		MoveX86RegToX86Reg(TempReg1, TempReg2);
		ShiftRightUnsignImmed(TempReg2,12);
		MoveVariableDispToX86Reg(TLB_ReadMap,"TLB_ReadMap",TempReg2,TempReg2,4);
		
		//For tlb miss
		//0041C522 85 C0                test        eax,eax
		//0041C524 75 01                jne         0041C527
	}
	
	Offset = Map_TempReg(Section,x86_Any,-1,FALSE);
	MoveX86RegToX86Reg(TempReg1, Offset);
	AndConstToX86Reg(Offset,3);
	AndConstToX86Reg(TempReg1,(DWORD)~3);

	Value = Map_TempReg(Section,x86_Any,-1,FALSE);
	if (UseTlb) {	
		MoveX86regPointerToX86reg(TempReg1, TempReg2,Value);
	} else {
		AndConstToX86Reg(TempReg1,0x1FFFFFFF);
		MoveN64MemToX86reg(Value,TempReg1);
	}

	AndVariableDispToX86Reg(SWL_MASK,"SWL_MASK",Value,Offset,4);
	if (!IsConst(Opcode.BRANCH.rt) || MipsRegLo(Opcode.BRANCH.rt) != 0) {
		MoveVariableDispToX86Reg(SWL_SHIFT,"SWL_SHIFT",shift,Offset,4);
		if (IsConst(Opcode.BRANCH.rt)) {
			MoveConstToX86reg(MipsRegLo(Opcode.BRANCH.rt),Offset);
		} else if (IsMapped(Opcode.BRANCH.rt)) {
			MoveX86RegToX86Reg(MipsRegLo(Opcode.BRANCH.rt),Offset);
		} else {
			MoveVariableToX86reg(&GPR[Opcode.BRANCH.rt].UW[0],GPR_NameLo[Opcode.BRANCH.rt],Offset);
		}
		ShiftRightUnsign(Offset);
		AddX86RegToX86Reg(Value,Offset);
	}

	if (UseTlb) {
		MoveX86RegToX86Reg(TempReg1, TempReg2);
		ShiftRightUnsignImmed(TempReg2,12);
		MoveVariableDispToX86Reg(TLB_WriteMap,"TLB_WriteMap",TempReg2,TempReg2,4);
		CompileWriteTLBMiss(Section, TempReg1, TempReg2);

		MoveX86regToX86regPointer(Value,TempReg1, TempReg2);
	} else {
		MoveX86regToN64Mem(Value,TempReg1);
	}
}

void Compile_R4300i_SW (BLOCK_SECTION * Section){
	DWORD TempReg1, TempReg2;
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
	
	if (Opcode.IMM.base == 29 && SPHack) {
		if (IsMapped(Opcode.BRANCH.rt)) { ProtectGPR(Section,Opcode.BRANCH.rt); }
		TempReg1 = Map_MemoryStack(Section,TRUE);

		if (IsConst(Opcode.BRANCH.rt)) {
			MoveConstToMemoryDisp (MipsRegLo(Opcode.BRANCH.rt),TempReg1, (DWORD)((short)Opcode.BRANCH.offset));
		} else if (IsMapped(Opcode.BRANCH.rt)) {
			MoveX86regToMemory(MipsRegLo(Opcode.BRANCH.rt),TempReg1,(DWORD)((short)Opcode.BRANCH.offset));
		} else {	
			TempReg2 = Map_TempReg(Section,x86_Any,Opcode.BRANCH.rt,FALSE);
			MoveX86regToMemory(TempReg2,TempReg1,(DWORD)((short)Opcode.BRANCH.offset));
		}		
	} else {
		if (IsConst(Opcode.IMM.base)) { 
			DWORD Address = MipsRegLo(Opcode.IMM.base) + (short)Opcode.BRANCH.offset;
			
			if (IsConst(Opcode.BRANCH.rt)) {
				Compile_SW_Const(MipsRegLo(Opcode.BRANCH.rt), Address);
			} else if (IsMapped(Opcode.BRANCH.rt)) {
				Compile_SW_Register(MipsRegLo(Opcode.BRANCH.rt), Address);
			} else {
				Compile_SW_Register(Map_TempReg(Section,x86_Any,Opcode.BRANCH.rt,FALSE), Address);
			}
			return;
		}
		if (IsMapped(Opcode.BRANCH.rt)) { ProtectGPR(Section,Opcode.BRANCH.rt); }
		if (IsMapped(Opcode.IMM.base)) { 
			ProtectGPR(Section,Opcode.IMM.base);
			if (Opcode.BRANCH.offset != 0) {
				TempReg1 = Map_TempReg(Section,x86_Any,-1,FALSE);
				LeaSourceAndOffset(TempReg1,MipsRegLo(Opcode.IMM.base),(short)Opcode.BRANCH.offset);
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
		if (UseTlb) {
			TempReg2 = Map_TempReg(Section,x86_Any,-1,FALSE);
			MoveX86RegToX86Reg(TempReg1, TempReg2);
			ShiftRightUnsignImmed(TempReg2,12);
			MoveVariableDispToX86Reg(TLB_WriteMap,"TLB_WriteMap",TempReg2,TempReg2,4);
			CompileWriteTLBMiss(Section, TempReg1, TempReg2);
			//For tlb miss
			//0041C522 85 C0                test        eax,eax
			//0041C524 75 01                jne         0041C527

			if (IsConst(Opcode.BRANCH.rt)) {
				MoveConstToX86regPointer(MipsRegLo(Opcode.BRANCH.rt),TempReg1, TempReg2);
			} else if (IsMapped(Opcode.BRANCH.rt)) {
				MoveX86regToX86regPointer(MipsRegLo(Opcode.BRANCH.rt),TempReg1, TempReg2);
			} else {	
				MoveX86regToX86regPointer(Map_TempReg(Section,x86_Any,Opcode.BRANCH.rt,FALSE),TempReg1, TempReg2);
			}
		} else {
			AndConstToX86Reg(TempReg1,0x1FFFFFFF);
			if (IsConst(Opcode.BRANCH.rt)) {
				MoveConstToN64Mem(MipsRegLo(Opcode.BRANCH.rt),TempReg1);
			} else if (IsMapped(Opcode.BRANCH.rt)) {
				MoveX86regToN64Mem(MipsRegLo(Opcode.BRANCH.rt),TempReg1);
			} else {	
				MoveX86regToN64Mem(Map_TempReg(Section,x86_Any,Opcode.BRANCH.rt,FALSE),TempReg1);
			}
		}
	}
}

void Compile_R4300i_SWR (BLOCK_SECTION * Section) {
	DWORD TempReg1, TempReg2 = 0x0, Value, Offset, shift;
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));

	if (IsConst(Opcode.IMM.base)) { 
		DWORD Address;
	
		Address = MipsRegLo(Opcode.IMM.base) + (short)Opcode.BRANCH.offset;
		Offset  = Address & 3;
		
		Value = Map_TempReg(Section,x86_Any,-1,FALSE);
		if (Compile_LW(Value, (Address & ~3))) {
			AndConstToX86Reg(Value, SWR_MASK[Offset]);
			TempReg1 = Map_TempReg(Section, x86_Any, Opcode.BRANCH.rt, FALSE);
			ShiftLeftSignImmed(TempReg1, (BYTE)SWR_SHIFT[Offset]);
			AddX86RegToX86Reg(Value, TempReg1);
			Compile_SW_Register(Value, (Address & ~3));
			return;
		}

		// Deoptimization: Address translation for a constant virtual address failed.
		// Unmap the base register to force it to be loaded from memory by the following codegen.
		UnMap_GPR(Section, Opcode.IMM.base, TRUE);
	}
	shift = Map_TempReg(Section,x86_ECX,-1,FALSE);
	if (IsMapped(Opcode.IMM.base)) { 
		ProtectGPR(Section,Opcode.IMM.base);
		if (Opcode.BRANCH.offset != 0) {
			TempReg1 = Map_TempReg(Section,x86_Any,-1,FALSE);
			LeaSourceAndOffset(TempReg1,MipsRegLo(Opcode.IMM.base),(short)Opcode.BRANCH.offset);
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
	if (UseTlb) {
		TempReg2 = Map_TempReg(Section,x86_Any,-1,FALSE);
		MoveX86RegToX86Reg(TempReg1, TempReg2);
		ShiftRightUnsignImmed(TempReg2,12);
		MoveVariableDispToX86Reg(TLB_ReadMap,"TLB_ReadMap",TempReg2,TempReg2,4);
		
		//For tlb miss
		//0041C522 85 C0                test        eax,eax
		//0041C524 75 01                jne         0041C527
	}
	
	Offset = Map_TempReg(Section,x86_Any,-1,FALSE);
	MoveX86RegToX86Reg(TempReg1, Offset);
	AndConstToX86Reg(Offset,3);
	AndConstToX86Reg(TempReg1,(DWORD)~3);

	Value = Map_TempReg(Section,x86_Any,-1,FALSE);
	if (UseTlb) {
		MoveX86regPointerToX86reg(TempReg1, TempReg2,Value);
	} else {
		AndConstToX86Reg(TempReg1,0x1FFFFFFF);
		MoveN64MemToX86reg(Value,TempReg1);
	}

	AndVariableDispToX86Reg(SWR_MASK,"SWR_MASK",Value,Offset,4);
	if (!IsConst(Opcode.BRANCH.rt) || MipsRegLo(Opcode.BRANCH.rt) != 0) {
		MoveVariableDispToX86Reg(SWR_SHIFT,"SWR_SHIFT",shift,Offset,4);
		if (IsConst(Opcode.BRANCH.rt)) {
			MoveConstToX86reg(MipsRegLo(Opcode.BRANCH.rt),Offset);
		} else if (IsMapped(Opcode.BRANCH.rt)) {
			MoveX86RegToX86Reg(MipsRegLo(Opcode.BRANCH.rt),Offset);
		} else {
			MoveVariableToX86reg(&GPR[Opcode.BRANCH.rt].UW[0],GPR_NameLo[Opcode.BRANCH.rt],Offset);
		}
		ShiftLeftSign(Offset);
		AddX86RegToX86Reg(Value,Offset);
	}

	if (UseTlb) {
		MoveX86RegToX86Reg(TempReg1, TempReg2);
		ShiftRightUnsignImmed(TempReg2,12);
		MoveVariableDispToX86Reg(TLB_WriteMap,"TLB_WriteMap",TempReg2,TempReg2,4);
		CompileWriteTLBMiss(Section, TempReg1, TempReg2);

		MoveX86regToX86regPointer(Value,TempReg1, TempReg2);
	} else {
		MoveX86regToN64Mem(Value,TempReg1);
	}
}

void Compile_R4300i_SDL (BLOCK_SECTION * Section) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
	if (Opcode.IMM.base != 0) { UnMap_GPR(Section,Opcode.IMM.base,TRUE); }
	if (Opcode.BRANCH.rt != 0) { UnMap_GPR(Section,Opcode.BRANCH.rt,TRUE); }
	Pushad();
	MoveConstToVariable(Opcode.Hex, &Opcode.Hex, "Opcode.Hex" );
	Call_Direct(r4300i_SDL, "r4300i_SDL");
	Popad();

}

void Compile_R4300i_SDR (BLOCK_SECTION * Section) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
	if (Opcode.IMM.base != 0) { UnMap_GPR(Section,Opcode.IMM.base,TRUE); }
	if (Opcode.BRANCH.rt != 0) { UnMap_GPR(Section,Opcode.BRANCH.rt,TRUE); }
	Pushad();
	MoveConstToVariable(Opcode.Hex, &Opcode.Hex, "Opcode.Hex" );
	Call_Direct(r4300i_SDR, "r4300i_SDR");
	Popad();

}

void _fastcall ClearRecomplierCache (DWORD Address) {
	if (!TranslateVaddr(&Address)) { DisplayError("Cache: Failed to translate: %X",Address); return; }
	if (Address < RdramSize) {
		DWORD Block = Address >> 12;
		if (N64_Blocks.NoOfRDRamBlocks[Block] > 0) {
			N64_Blocks.NoOfRDRamBlocks[Block] = 0;		
			memset(JumpTable + (Block << 10),0,0x1000);
			*(DelaySlotTable + Block) = NULL;
		}		
	} else {
		if (ShowDebugMessages)
			DisplayError("ClearRecomplierCache: %X",Address);
	}
}

void Compile_R4300i_CACHE (BLOCK_SECTION * Section){
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
	if (SelfModCheck != ModCode_Cache && 
		SelfModCheck != ModCode_ChangeMemory && 
		SelfModCheck != ModCode_CheckMemory2 && 
		SelfModCheck != ModCode_CheckMemoryCache) 
	{ 
		return; 
	}

	switch(Opcode.BRANCH.rt) {
	case 0:
	case 16:
		Pushad();
		if (IsConst(Opcode.IMM.base)) { 
			DWORD Address = MipsRegLo(Opcode.IMM.base) + (short)Opcode.BRANCH.offset;
			MoveConstToX86reg(Address,x86_ECX);
		} else if (IsMapped(Opcode.IMM.base)) { 
			if (MipsRegLo(Opcode.IMM.base) == x86_ECX) {
				AddConstToX86Reg(x86_ECX,(short)Opcode.BRANCH.offset);
			} else {
				LeaSourceAndOffset(x86_ECX,MipsRegLo(Opcode.IMM.base),(short)Opcode.BRANCH.offset);
			}
		} else {
			MoveVariableToX86reg(&GPR[Opcode.IMM.base].UW[0],GPR_NameLo[Opcode.IMM.base],x86_ECX);
			AddConstToX86Reg(x86_ECX,(short)Opcode.BRANCH.offset);
		}
		Call_Direct(ClearRecomplierCache, "ClearRecomplierCache");
		Popad();
		break;
	case 1:
	case 3:
	case 13:
	case 5:
	case 8:
	case 9:
	case 17:
	case 21:
	case 25:
		break;
	default:
		if (ShowDebugMessages)
			DisplayError("cache: %d",Opcode.BRANCH.rt);
	}
}

void Compile_R4300i_LL (BLOCK_SECTION * Section) {
	DWORD TempReg1, TempReg2;
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));

	if (Opcode.BRANCH.rt == 0) return;

	if (IsConst(Opcode.IMM.base)) { 
		DWORD Address = MipsRegLo(Opcode.IMM.base) + (short)Opcode.BRANCH.offset;
		Map_GPR_32bit(Section,Opcode.BRANCH.rt,TRUE,-1);
		if (Compile_LW(MipsRegLo(Opcode.BRANCH.rt), Address)) {
			MoveConstToVariable(1, &LLBit, "LLBit");
			TranslateVaddr(&Address);
			MoveConstToVariable(Address, &LLADDR_REGISTER, "LLAddr");
			return;
		}

		// Deoptimization: Address translation for a constant virtual address failed.
		// Unmap the base register to force it to be loaded from memory by the following codegen.
		UnMap_GPR(Section, Opcode.IMM.base, TRUE);
	}
	if (UseTlb) {	
		if (IsMapped(Opcode.BRANCH.rt)) { ProtectGPR(Section,Opcode.BRANCH.rt); }
		if (IsMapped(Opcode.IMM.base) && Opcode.BRANCH.offset == 0) { 
			ProtectGPR(Section,Opcode.IMM.base);
			TempReg1 = MipsRegLo(Opcode.IMM.base);
		} else {
			if (IsMapped(Opcode.IMM.base)) { 
				ProtectGPR(Section,Opcode.IMM.base);
				if (Opcode.BRANCH.offset != 0) {
					TempReg1 = Map_TempReg(Section,x86_Any,-1,FALSE);
					LeaSourceAndOffset(TempReg1,MipsRegLo(Opcode.IMM.base),(short)Opcode.BRANCH.offset);
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
		MoveX86RegToX86Reg(TempReg1, TempReg2);
		ShiftRightUnsignImmed(TempReg2,12);
		MoveVariableDispToX86Reg(TLB_ReadMap,"TLB_ReadMap",TempReg2,TempReg2,4);
		CompileReadTLBMiss(Section,TempReg1,TempReg2);
		Map_GPR_32bit(Section,Opcode.BRANCH.rt,TRUE,-1);
		MoveX86regPointerToX86reg(TempReg1, TempReg2,MipsRegLo(Opcode.BRANCH.rt));
		MoveConstToVariable(1,&LLBit,"LLBit");
		MoveX86regToVariable(TempReg1,&LLADDR_REGISTER,"LLAddr");
		AddX86regToVariable(TempReg2,&LLADDR_REGISTER,"LLAddr");
		SubConstFromVariable((DWORD)N64MEM,&LLADDR_REGISTER,"LLAddr");
	} else {
		if (IsMapped(Opcode.IMM.base)) { 
			ProtectGPR(Section,Opcode.IMM.base);
			if (Opcode.BRANCH.offset != 0) {
				Map_GPR_32bit(Section,Opcode.BRANCH.rt,TRUE,-1);
				LeaSourceAndOffset(MipsRegLo(Opcode.BRANCH.rt),MipsRegLo(Opcode.IMM.base),(short)Opcode.BRANCH.offset);
			} else {
				Map_GPR_32bit(Section,Opcode.BRANCH.rt,TRUE,Opcode.IMM.base);
			}
		} else {
			Map_GPR_32bit(Section,Opcode.BRANCH.rt,TRUE,Opcode.IMM.base);
			if (Opcode.IMM.immediate == 0) { 
			} else if (Opcode.IMM.immediate == 1) {
				IncX86reg(MipsRegLo(Opcode.BRANCH.rt));
			} else if (Opcode.IMM.immediate == 0xFFFF) {			
				DecX86reg(MipsRegLo(Opcode.BRANCH.rt));
			} else {
				AddConstToX86Reg(MipsRegLo(Opcode.BRANCH.rt),(short)Opcode.IMM.immediate);
			}
		}
		AndConstToX86Reg(MipsRegLo(Opcode.BRANCH.rt),0x1FFFFFFF);
		MoveX86regToVariable(MipsRegLo(Opcode.BRANCH.rt),&LLADDR_REGISTER,"LLAddr");
		MoveN64MemToX86reg(MipsRegLo(Opcode.BRANCH.rt),MipsRegLo(Opcode.BRANCH.rt));
		MoveConstToVariable(1,&LLBit,"LLBit");
	}
}

void Compile_R4300i_SC (BLOCK_SECTION * Section){
	DWORD TempReg1, TempReg2;
	BYTE * Jump;
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
	
	CompConstToVariable(1,&LLBit,"LLBit");
	JneLabel32("LLBitNotSet",0);
	Jump = RecompPos - 4;
	if (IsConst(Opcode.IMM.base)) { 
		DWORD Address = MipsRegLo(Opcode.IMM.base) + (short)Opcode.BRANCH.offset;
			
		if (IsConst(Opcode.BRANCH.rt)) {
			Compile_SW_Const(MipsRegLo(Opcode.BRANCH.rt), Address);
		} else if (IsMapped(Opcode.BRANCH.rt)) {
			Compile_SW_Register(MipsRegLo(Opcode.BRANCH.rt), Address);
		} else {
			Compile_SW_Register(Map_TempReg(Section,x86_Any,Opcode.BRANCH.rt,FALSE), Address);
		}
		CPU_Message("      LLBitNotSet:");
		*((DWORD *)(Jump))=(BYTE)(RecompPos - Jump - 4);
		Map_GPR_32bit(Section,Opcode.BRANCH.rt,FALSE,-1);
		MoveVariableToX86reg(&LLBit,"LLBit",MipsRegLo(Opcode.BRANCH.rt));
		return;
	}
	if (IsMapped(Opcode.BRANCH.rt)) { ProtectGPR(Section,Opcode.BRANCH.rt); }
	if (IsMapped(Opcode.IMM.base)) { 
		ProtectGPR(Section,Opcode.IMM.base);
		if (Opcode.BRANCH.offset != 0) {
			TempReg1 = Map_TempReg(Section,x86_Any,-1,FALSE);
			LeaSourceAndOffset(TempReg1,MipsRegLo(Opcode.IMM.base),(short)Opcode.BRANCH.offset);
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
	if (UseTlb) {
		TempReg2 = Map_TempReg(Section,x86_Any,-1,FALSE);
		MoveX86RegToX86Reg(TempReg1, TempReg2);
		ShiftRightUnsignImmed(TempReg2,12);
		MoveVariableDispToX86Reg(TLB_WriteMap,"TLB_WriteMap",TempReg2,TempReg2,4);
		CompileWriteTLBMiss(Section, TempReg1, TempReg2);
		//For tlb miss
		//0041C522 85 C0                test        eax,eax
		//0041C524 75 01                jne         0041C527

		if (IsConst(Opcode.BRANCH.rt)) {
			MoveConstToX86regPointer(MipsRegLo(Opcode.BRANCH.rt),TempReg1, TempReg2);
		} else if (IsMapped(Opcode.BRANCH.rt)) {
			MoveX86regToX86regPointer(MipsRegLo(Opcode.BRANCH.rt),TempReg1, TempReg2);
		} else {	
			MoveX86regToX86regPointer(Map_TempReg(Section,x86_Any,Opcode.BRANCH.rt,FALSE),TempReg1, TempReg2);
		}
	} else {
		AndConstToX86Reg(TempReg1,0x1FFFFFFF);
		if (IsConst(Opcode.BRANCH.rt)) {
			MoveConstToN64Mem(MipsRegLo(Opcode.BRANCH.rt),TempReg1);
		} else if (IsMapped(Opcode.BRANCH.rt)) {
			MoveX86regToN64Mem(MipsRegLo(Opcode.BRANCH.rt),TempReg1);
		} else {	
			MoveX86regToN64Mem(Map_TempReg(Section,x86_Any,Opcode.BRANCH.rt,FALSE),TempReg1);
		}
	}
	CPU_Message("      LLBitNotSet:");
	*((DWORD *)(Jump))=(BYTE)(RecompPos - Jump - 4);
	Map_GPR_32bit(Section,Opcode.BRANCH.rt,FALSE,-1);
	MoveVariableToX86reg(&LLBit,"LLBit",MipsRegLo(Opcode.BRANCH.rt));

}

void Compile_R4300i_LD (BLOCK_SECTION * Section) {
	DWORD TempReg1, TempReg2;
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));

	if (Opcode.BRANCH.rt == 0) return;

	if (IsConst(Opcode.IMM.base)) { 
		DWORD Address = MipsRegLo(Opcode.IMM.base) + (short)Opcode.BRANCH.offset;
		Map_GPR_64bit(Section,Opcode.BRANCH.rt,-1);
		if (Compile_LW(MipsRegHi(Opcode.BRANCH.rt), Address)) {
			Compile_LW(MipsRegLo(Opcode.BRANCH.rt), Address + 4);
			if (SPHack && Opcode.BRANCH.rt == 29) { ResetMemoryStack(Section); }
			return;
		}

		// Deoptimization: Address translation for a constant virtual address failed.
		// Unmap the base register to force it to be loaded from memory by the following codegen.
		UnMap_GPR(Section, Opcode.IMM.base, TRUE);
	}
	if (IsMapped(Opcode.BRANCH.rt)) { ProtectGPR(Section,Opcode.BRANCH.rt); }
	if (IsMapped(Opcode.IMM.base) && Opcode.BRANCH.offset == 0) { 
		if (UseTlb) {
			ProtectGPR(Section,Opcode.IMM.base);
			TempReg1 = MipsRegLo(Opcode.IMM.base);
		} else {
			TempReg1 = Map_TempReg(Section,x86_Any,Opcode.IMM.base,FALSE);
		}
	} else {
		if (IsMapped(Opcode.IMM.base)) { 
			ProtectGPR(Section,Opcode.IMM.base);
			if (Opcode.BRANCH.offset != 0) {
				TempReg1 = Map_TempReg(Section,x86_Any,-1,FALSE);
				LeaSourceAndOffset(TempReg1,MipsRegLo(Opcode.IMM.base),(short)Opcode.BRANCH.offset);
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
	if (UseTlb) {
		TempReg2 = Map_TempReg(Section,x86_Any,-1,FALSE);
		MoveX86RegToX86Reg(TempReg1, TempReg2);
		ShiftRightUnsignImmed(TempReg2,12);
		MoveVariableDispToX86Reg(TLB_ReadMap,"TLB_ReadMap",TempReg2,TempReg2,4);
		//For tlb miss
		//0041C522 85 C0                test        eax,eax
		//0041C524 75 01                jne         0041C527
		Map_GPR_64bit(Section,Opcode.BRANCH.rt,-1);
		MoveX86regPointerToX86reg(TempReg1, TempReg2,MipsRegHi(Opcode.BRANCH.rt));
		MoveX86regPointerToX86regDisp8(TempReg1, TempReg2,MipsRegLo(Opcode.BRANCH.rt),4);
	} else {
		AndConstToX86Reg(TempReg1,0x1FFFFFFF);
		Map_GPR_64bit(Section,Opcode.BRANCH.rt,-1);
		MoveN64MemToX86reg(MipsRegHi(Opcode.BRANCH.rt),TempReg1);
		MoveN64MemDispToX86reg(MipsRegLo(Opcode.BRANCH.rt),TempReg1,4);
	}
	if (SPHack && Opcode.BRANCH.rt == 29) { 
		int count;

		for (count = 0; count < 10; count ++) { x86Protected(count) = FALSE; }
		ResetMemoryStack(Section); 
	}
}

void Compile_R4300i_SD (BLOCK_SECTION * Section){
	DWORD TempReg1, TempReg2;
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
	
	if (IsConst(Opcode.IMM.base)) { 
		DWORD Address = MipsRegLo(Opcode.IMM.base) + (short)Opcode.BRANCH.offset;
		
		if (IsConst(Opcode.BRANCH.rt)) {
			if (Is64Bit(Opcode.BRANCH.rt)) {
				Compile_SW_Const(MipsRegHi(Opcode.BRANCH.rt), Address);
			} else {
				Compile_SW_Const((MipsRegLo_S(Opcode.BRANCH.rt) >> 31), Address);
			}
			Compile_SW_Const(MipsRegLo(Opcode.BRANCH.rt), Address + 4);
		} else if (IsMapped(Opcode.BRANCH.rt)) {
			if (Is64Bit(Opcode.BRANCH.rt)) {
				Compile_SW_Register(MipsRegHi(Opcode.BRANCH.rt), Address);
			} else {
				Compile_SW_Register(Map_TempReg(Section,x86_Any,Opcode.BRANCH.rt,TRUE), Address);
			}
			Compile_SW_Register(MipsRegLo(Opcode.BRANCH.rt), Address + 4);		
		} else {
			Compile_SW_Register(TempReg1 = Map_TempReg(Section,x86_Any,Opcode.BRANCH.rt,TRUE), Address);
			Compile_SW_Register(Map_TempReg(Section,TempReg1,Opcode.BRANCH.rt,FALSE), Address + 4);		
		}
		return;
	}
	if (IsMapped(Opcode.BRANCH.rt)) { ProtectGPR(Section,Opcode.BRANCH.rt); }
	if (IsMapped(Opcode.IMM.base)) { 
		ProtectGPR(Section,Opcode.IMM.base);
		if (Opcode.BRANCH.offset != 0) {
			TempReg1 = Map_TempReg(Section,x86_Any,-1,FALSE);
			LeaSourceAndOffset(TempReg1,MipsRegLo(Opcode.IMM.base),(short)Opcode.BRANCH.offset);
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
		CompileWriteTLBMiss(Section, TempReg1, TempReg2);
		//For tlb miss
		//0041C522 85 C0                test        eax,eax
		//0041C524 75 01                jne         0041C527

		if (IsConst(Opcode.BRANCH.rt)) {
			if (Is64Bit(Opcode.BRANCH.rt)) {
				MoveConstToX86regPointer(MipsRegHi(Opcode.BRANCH.rt),TempReg1, TempReg2);
			} else {
				MoveConstToX86regPointer((MipsRegLo_S(Opcode.BRANCH.rt) >> 31),TempReg1, TempReg2);
			}
			AddConstToX86Reg(TempReg1,4);
			MoveConstToX86regPointer(MipsRegLo(Opcode.BRANCH.rt),TempReg1, TempReg2);
		} else if (IsMapped(Opcode.BRANCH.rt)) {
			if (Is64Bit(Opcode.BRANCH.rt)) {
				MoveX86regToX86regPointer(MipsRegHi(Opcode.BRANCH.rt),TempReg1, TempReg2);
			} else {
				MoveX86regToX86regPointer(Map_TempReg(Section,x86_Any,Opcode.BRANCH.rt,TRUE),TempReg1, TempReg2);
			}
			AddConstToX86Reg(TempReg1,4);
			MoveX86regToX86regPointer(MipsRegLo(Opcode.BRANCH.rt),TempReg1, TempReg2);
		} else {	
			int X86Reg = Map_TempReg(Section,x86_Any,Opcode.BRANCH.rt,TRUE);
			MoveX86regToX86regPointer(X86Reg,TempReg1, TempReg2);
			AddConstToX86Reg(TempReg1,4);
			MoveX86regToX86regPointer(Map_TempReg(Section,X86Reg,Opcode.BRANCH.rt,FALSE),TempReg1, TempReg2);
		}
	} else {
		AndConstToX86Reg(TempReg1,0x1FFFFFFF);		
		if (IsConst(Opcode.BRANCH.rt)) {
			if (Is64Bit(Opcode.BRANCH.rt)) {
				MoveConstToN64Mem(MipsRegHi(Opcode.BRANCH.rt),TempReg1);
			} else if (IsSigned(Opcode.BRANCH.rt)) {
				MoveConstToN64Mem(((int)MipsRegLo(Opcode.BRANCH.rt) >> 31),TempReg1);
			} else {
				MoveConstToN64Mem(0,TempReg1);
			}
			MoveConstToN64MemDisp(MipsRegLo(Opcode.BRANCH.rt),TempReg1,4);
		} else if (IsKnown(Opcode.BRANCH.rt) && IsMapped(Opcode.BRANCH.rt)) {
			if (Is64Bit(Opcode.BRANCH.rt)) {
				MoveX86regToN64Mem(MipsRegHi(Opcode.BRANCH.rt),TempReg1);
			} else if (IsSigned(Opcode.BRANCH.rt)) {
				MoveX86regToN64Mem(Map_TempReg(Section,x86_Any,Opcode.BRANCH.rt,TRUE), TempReg1);
			} else {
				MoveConstToN64Mem(0,TempReg1);
			}
			MoveX86regToN64MemDisp(MipsRegLo(Opcode.BRANCH.rt),TempReg1, 4);		
		} else {	
			int x86reg;
			MoveX86regToN64Mem(x86reg = Map_TempReg(Section,x86_Any,Opcode.BRANCH.rt,TRUE), TempReg1);
			MoveX86regToN64MemDisp(Map_TempReg(Section,x86reg,Opcode.BRANCH.rt,FALSE), TempReg1,4);
		}
	}
}

/********************** R4300i OpCodes: Special **********************/
void Compile_R4300i_SPECIAL_SLL (BLOCK_SECTION * Section) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));

	if (Opcode.REG.rd == 0) { return; }
	if (IsConst(Opcode.BRANCH.rt)) {
		if (IsMapped(Opcode.REG.rd)) { UnMap_GPR(Section,Opcode.REG.rd, FALSE); }
		MipsRegLo(Opcode.REG.rd) = MipsRegLo(Opcode.BRANCH.rt) << Opcode.REG.sa;
		MipsRegState(Opcode.REG.rd) = STATE_CONST_32;
		return;
	}
	if (Opcode.REG.rd != Opcode.BRANCH.rt && IsMapped(Opcode.BRANCH.rt)) {
		switch (Opcode.REG.sa) {
		case 0: 
			Map_GPR_32bit(Section,Opcode.REG.rd,TRUE,Opcode.BRANCH.rt);
			break;
		case 1:
			ProtectGPR(Section,Opcode.BRANCH.rt);
			Map_GPR_32bit(Section,Opcode.REG.rd,TRUE,-1);
			LeaRegReg(MipsRegLo(Opcode.REG.rd),MipsRegLo(Opcode.BRANCH.rt), 2);
			break;			
		case 2:
			ProtectGPR(Section,Opcode.BRANCH.rt);
			Map_GPR_32bit(Section,Opcode.REG.rd,TRUE,-1);
			LeaRegReg(MipsRegLo(Opcode.REG.rd),MipsRegLo(Opcode.BRANCH.rt), 4);
			break;			
		case 3:
			ProtectGPR(Section,Opcode.BRANCH.rt);
			Map_GPR_32bit(Section,Opcode.REG.rd,TRUE,-1);
			LeaRegReg(MipsRegLo(Opcode.REG.rd),MipsRegLo(Opcode.BRANCH.rt), 8);
			break;
		default:
			Map_GPR_32bit(Section,Opcode.REG.rd,TRUE,Opcode.BRANCH.rt);
			ShiftLeftSignImmed(MipsRegLo(Opcode.REG.rd),(BYTE)Opcode.REG.sa);
		}
	} else {
		Map_GPR_32bit(Section,Opcode.REG.rd,TRUE,Opcode.BRANCH.rt);
		ShiftLeftSignImmed(MipsRegLo(Opcode.REG.rd),(BYTE)Opcode.REG.sa);
	}
}

void Compile_R4300i_SPECIAL_SRL (BLOCK_SECTION * Section) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
	if (Opcode.REG.rd == 0) { return; }
	
	if (IsConst(Opcode.BRANCH.rt)) {
		if (IsMapped(Opcode.REG.rd)) { UnMap_GPR(Section,Opcode.REG.rd, FALSE); }
		MipsRegLo(Opcode.REG.rd) = MipsRegLo(Opcode.BRANCH.rt) >> Opcode.REG.sa;
		MipsRegState(Opcode.REG.rd) = STATE_CONST_32;
		return;
	}
	Map_GPR_32bit(Section,Opcode.REG.rd,TRUE,Opcode.BRANCH.rt);
	ShiftRightUnsignImmed(MipsRegLo(Opcode.REG.rd),(BYTE)Opcode.REG.sa);
}

void Compile_R4300i_SPECIAL_SRA (BLOCK_SECTION * Section) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
	if (Opcode.REG.rd == 0) { return; }

	if (IsConst(Opcode.BRANCH.rt)) {
		if (IsMapped(Opcode.REG.rd)) { UnMap_GPR(Section, Opcode.REG.rd, FALSE); }
		MipsRegLo(Opcode.REG.rd) = MipsRegLo_S(Opcode.BRANCH.rt) >> Opcode.REG.sa;
		MipsRegState(Opcode.REG.rd) = STATE_CONST_32;
		return;
	}
	if (Is32Bit(Opcode.BRANCH.rt)) {
		Map_GPR_32bit(Section, Opcode.REG.rd, TRUE, Opcode.BRANCH.rt);
		ShiftRightSignImmed(MipsRegLo(Opcode.REG.rd), (BYTE)Opcode.REG.sa);
	} else {
		Map_GPR_64bit(Section, Opcode.REG.rd, Opcode.BRANCH.rt);
		ShiftRightDoubleImmed(MipsRegLo(Opcode.REG.rd), MipsRegHi(Opcode.REG.rd), (BYTE)Opcode.REG.sa);
		MoveX86RegToX86Reg(MipsRegLo(Opcode.REG.rd), MipsRegHi(Opcode.REG.rd));
		ShiftRightSignImmed(MipsRegHi(Opcode.REG.rd), 31);
	}
}

void Compile_R4300i_SPECIAL_SLLV (BLOCK_SECTION * Section) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
	if (Opcode.REG.rd == 0) { return; }
	
	if (IsConst(Opcode.BRANCH.rs)) {
		DWORD Shift = (MipsRegLo(Opcode.BRANCH.rs) & 0x1F);
		if (IsConst(Opcode.BRANCH.rt)) {
			if (IsMapped(Opcode.REG.rd)) { UnMap_GPR(Section,Opcode.REG.rd, FALSE); }
			MipsRegLo(Opcode.REG.rd) = MipsRegLo(Opcode.BRANCH.rt) << Shift;
			MipsRegState(Opcode.REG.rd) = STATE_CONST_32;
		} else {
			Map_GPR_32bit(Section,Opcode.REG.rd,TRUE,Opcode.BRANCH.rt);
			ShiftLeftSignImmed(MipsRegLo(Opcode.REG.rd),(BYTE)Shift);
		}
		return;
	}
	Map_TempReg(Section,x86_ECX,Opcode.BRANCH.rs,FALSE);
	AndConstToX86Reg(x86_ECX,0x1F);
	Map_GPR_32bit(Section,Opcode.REG.rd,TRUE,Opcode.BRANCH.rt);
	ShiftLeftSign(MipsRegLo(Opcode.REG.rd));
}

void Compile_R4300i_SPECIAL_SRLV (BLOCK_SECTION * Section) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
	if (Opcode.REG.rd == 0) { return; }
	
	if (IsConst(Opcode.BRANCH.rs)) {
		DWORD Shift = (MipsRegLo(Opcode.BRANCH.rs) & 0x1F);
		if (IsConst(Opcode.BRANCH.rt)) {
			if (IsMapped(Opcode.REG.rd)) { UnMap_GPR(Section,Opcode.REG.rd, FALSE); }
			MipsRegLo(Opcode.REG.rd) = MipsRegLo(Opcode.BRANCH.rt) >> Shift;
			MipsRegState(Opcode.REG.rd) = STATE_CONST_32;
			return;
		}
		Map_GPR_32bit(Section,Opcode.REG.rd,TRUE,Opcode.BRANCH.rt);
		ShiftRightUnsignImmed(MipsRegLo(Opcode.REG.rd),(BYTE)Shift);
		return;
	}
	Map_TempReg(Section,x86_ECX,Opcode.BRANCH.rs,FALSE);
	AndConstToX86Reg(x86_ECX,0x1F);
	Map_GPR_32bit(Section,Opcode.REG.rd,TRUE,Opcode.BRANCH.rt);
	ShiftRightUnsign(MipsRegLo(Opcode.REG.rd));
}

void Compile_R4300i_SPECIAL_SRAV (BLOCK_SECTION * Section) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
	if (Opcode.REG.rd == 0) { return; }

	if (IsConst(Opcode.BRANCH.rs)) {
		DWORD Shift = (MipsRegLo(Opcode.BRANCH.rs) & 0x1F);
		if (IsConst(Opcode.BRANCH.rt)) {
			if (IsMapped(Opcode.REG.rd)) { UnMap_GPR(Section, Opcode.REG.rd, FALSE); }
			MipsRegLo(Opcode.REG.rd) = MipsRegLo_S(Opcode.BRANCH.rt) >> Shift;
			MipsRegState(Opcode.REG.rd) = STATE_CONST_32;
			return;
		}
		if (Is32Bit(Opcode.BRANCH.rt)) {
			Map_GPR_32bit(Section, Opcode.REG.rd, TRUE, Opcode.BRANCH.rt);
			ShiftRightSignImmed(MipsRegLo(Opcode.REG.rd), Shift);
		} else {
			Map_GPR_64bit(Section, Opcode.REG.rd, Opcode.BRANCH.rt);
			ShiftRightDoubleImmed(MipsRegLo(Opcode.REG.rd), MipsRegHi(Opcode.REG.rd), Shift);
			MoveX86RegToX86Reg(MipsRegLo(Opcode.REG.rd), MipsRegHi(Opcode.REG.rd));
			ShiftRightSignImmed(MipsRegHi(Opcode.REG.rd), 31);
		}
		return;
	}
	Map_TempReg(Section, x86_ECX, Opcode.BRANCH.rs, FALSE);
	AndConstToX86Reg(x86_ECX, 0x1F);
	if (Is32Bit(Opcode.BRANCH.rt)) {
		Map_GPR_32bit(Section, Opcode.REG.rd, TRUE, Opcode.BRANCH.rt);
		ShiftRightSign(MipsRegLo(Opcode.REG.rd));
	} else {
		Map_GPR_64bit(Section, Opcode.REG.rd, Opcode.BRANCH.rt);
		ShiftRightDouble(MipsRegLo(Opcode.REG.rd), MipsRegHi(Opcode.REG.rd));
		MoveX86RegToX86Reg(MipsRegLo(Opcode.REG.rd), MipsRegHi(Opcode.REG.rd));
		ShiftRightSignImmed(MipsRegHi(Opcode.REG.rd), 31);
	}
}

void Compile_R4300i_SPECIAL_JR (BLOCK_SECTION * Section) {
	static char JumpLabel[100];
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	if ( NextInstruction == NORMAL ) {
		CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
		if (IsConst(Opcode.BRANCH.rs)) { 
			sprintf(JumpLabel,"0x%08X",MipsRegLo(Opcode.BRANCH.rs));
			Section->Jump.BranchLabel   = JumpLabel;
			Section->Jump.TargetPC      = MipsRegLo(Opcode.BRANCH.rs);
			Section->Jump.FallThrough   = TRUE;
			Section->Jump.LinkLocation  = NULL;
			Section->Jump.LinkLocation2 = NULL;
			Section->Cont.FallThrough   = FALSE;
			Section->Cont.LinkLocation  = NULL;
			Section->Cont.LinkLocation2 = NULL;
			if ((Section->CompilePC & 0xFFC) == 0xFFC) {
				GenerateSectionLinkage(Section);
				NextInstruction = END_BLOCK;
				return;
			}
		}
		if ((Section->CompilePC & 0xFFC) == 0xFFC) {
			if (IsMapped(Opcode.BRANCH.rs)) { 
				MoveX86regToVariable(MipsRegLo(Opcode.BRANCH.rs),&JumpToLocation, "JumpToLocation");
			} else {
				MoveX86regToVariable(Map_TempReg(Section,x86_Any,Opcode.BRANCH.rs,FALSE),&JumpToLocation, "JumpToLocation");
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
			NextInstruction = END_BLOCK;
			return;
		}
		MIPS_DWORD CompilePC;
		CompilePC.DW = (int)Section->CompilePC;
		if (DelaySlotEffectsCompare(CompilePC,Opcode.BRANCH.rs,0)) {
			if (IsConst(Opcode.BRANCH.rs)) { 
				MoveConstToVariable(MipsRegLo(Opcode.BRANCH.rs),&PROGRAM_COUNTER, "PROGRAM_COUNTER");
			} else 	if (IsMapped(Opcode.BRANCH.rs)) { 
				MoveX86regToVariable(MipsRegLo(Opcode.BRANCH.rs),&PROGRAM_COUNTER, "PROGRAM_COUNTER");
			} else {
				MoveX86regToVariable(Map_TempReg(Section,x86_Any,Opcode.BRANCH.rs,FALSE),&PROGRAM_COUNTER, "PROGRAM_COUNTER");
			}
		}
		NextInstruction = DO_DELAY_SLOT;
	} else if (NextInstruction == DELAY_SLOT_DONE ) {
		MIPS_DWORD CompilePC;
		CompilePC.DW = (int)Section->CompilePC;
		if (DelaySlotEffectsCompare(CompilePC,Opcode.BRANCH.rs,0)) {
			CompileExit((DWORD)-1,Section->RegWorking,Normal,TRUE,NULL);
		} else {
			if (IsConst(Opcode.BRANCH.rs)) { 
				memcpy(&Section->Jump.RegSet,&Section->RegWorking,sizeof(REG_INFO));
				GenerateSectionLinkage(Section);
			} else {
				if (IsMapped(Opcode.BRANCH.rs)) { 
					MoveX86regToVariable(MipsRegLo(Opcode.BRANCH.rs),&PROGRAM_COUNTER, "PROGRAM_COUNTER");
				} else {
					MoveX86regToVariable(Map_TempReg(Section,x86_Any,Opcode.BRANCH.rs,FALSE),&PROGRAM_COUNTER, "PROGRAM_COUNTER");
				}
				CompileExit((DWORD)-1,Section->RegWorking,Normal,TRUE,NULL);
			}
		}
		NextInstruction = END_BLOCK;
	} else {
		if (ShowDebugMessages)
			DisplayError("WTF\n\nBranch\nNextInstruction = %X", NextInstruction);
	}
}

void Compile_R4300i_SPECIAL_JALR (BLOCK_SECTION * Section) {
	static char JumpLabel[100];
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;
	
	if ( NextInstruction == NORMAL ) {
		CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
		MIPS_DWORD CompilePC;
		CompilePC.DW = (int)Section->CompilePC;
		if (DelaySlotEffectsCompare(CompilePC,Opcode.BRANCH.rs,0)) {
			Compile_R4300i_UnknownOpcode(Section);
		}
		UnMap_GPR(Section, Opcode.REG.rd, FALSE);
		MipsRegLo(Opcode.REG.rd) = Section->CompilePC + 8;
		MipsRegState(Opcode.REG.rd) = STATE_CONST_32;
		if ((Section->CompilePC & 0xFFC) == 0xFFC) {
			if (IsMapped(Opcode.BRANCH.rs)) { 
				MoveX86regToVariable(MipsRegLo(Opcode.BRANCH.rs),&JumpToLocation, "JumpToLocation");
			} else {
				MoveX86regToVariable(Map_TempReg(Section,x86_Any,Opcode.BRANCH.rs,FALSE),&JumpToLocation, "JumpToLocation");
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
			NextInstruction = END_BLOCK;
			return;
		}
		NextInstruction = DO_DELAY_SLOT;
	} else if (NextInstruction == DELAY_SLOT_DONE ) {		
		if (IsConst(Opcode.BRANCH.rs)) { 
			memcpy(&Section->Jump.RegSet,&Section->RegWorking,sizeof(REG_INFO));
			sprintf(JumpLabel,"0x%08X",MipsRegLo(Opcode.BRANCH.rs));
			Section->Jump.BranchLabel   = JumpLabel;
			Section->Jump.TargetPC      = MipsRegLo(Opcode.BRANCH.rs);
			Section->Jump.FallThrough   = TRUE;
			Section->Jump.LinkLocation  = NULL;
			Section->Jump.LinkLocation2 = NULL;
			Section->Cont.FallThrough   = FALSE;
			Section->Cont.LinkLocation  = NULL;
			Section->Cont.LinkLocation2 = NULL;

			GenerateSectionLinkage(Section);
		} else {
			if (IsMapped(Opcode.BRANCH.rs)) { 
				MoveX86regToVariable(MipsRegLo(Opcode.BRANCH.rs),&PROGRAM_COUNTER, "PROGRAM_COUNTER");
			} else {
				MoveX86regToVariable(Map_TempReg(Section,x86_Any,Opcode.BRANCH.rs,FALSE),&PROGRAM_COUNTER, "PROGRAM_COUNTER");
			}
			CompileExit((DWORD)-1,Section->RegWorking,Normal,TRUE,NULL);
		}
		NextInstruction = END_BLOCK;
	} else {
		if (ShowDebugMessages)
			DisplayError("WTF\n\nBranch\nNextInstruction = %X", NextInstruction);
	}
}

void Compile_R4300i_SPECIAL_SYSCALL(BLOCK_SECTION* Section) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s", Section->CompilePC, R4300iOpcodeName(Opcode.Hex, CompilePC));

	CompileExit(Section->CompilePC, Section->RegWorking, DoSysCall, TRUE, NULL);
}

void Compile_R4300i_SPECIAL_BREAK(BLOCK_SECTION* Section) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s", Section->CompilePC, R4300iOpcodeName(Opcode.Hex, CompilePC));

	CompileExit(Section->CompilePC, Section->RegWorking, DoBreak, TRUE, NULL);
}

#define DoTrapLength 0x00000044
#define DoTrapLength32 0x00000046

void Compile_R4300i_SPECIAL_TEQ(BLOCK_SECTION* Section) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s", Section->CompilePC, R4300iOpcodeName(Opcode.Hex, CompilePC));
	int b1 = 0;
	int b2 = 0;
	int b3 = 0;
	//return;

	//BreakPoint();

	for (int i = 0; i < 32; i++)
	{
		if (IsMapped(i)) UnMap_GPR(Section, i, TRUE);
	}

	if (Opcode.BRANCH.rt == Opcode.BRANCH.rs)
	{
		CompileExit(Section->CompilePC, Section->RegWorking, DoTrap, TRUE, NULL);
		return;
	}
	else if (Opcode.BRANCH.rt == 0)
	{
		Map_GPR_32bit(Section, Opcode.BRANCH.rs, FALSE, Opcode.BRANCH.rs);
		CompConstToX86reg(MipsRegLo(Opcode.BRANCH.rs), 0);
	}
	else if (Opcode.BRANCH.rs == 0)
	{
		Map_GPR_32bit(Section, Opcode.BRANCH.rt, FALSE, Opcode.BRANCH.rt);
		CompConstToX86reg(MipsRegLo(Opcode.BRANCH.rs), 0);
	}
	else
	{
		Map_GPR_32bit(Section, Opcode.BRANCH.rt, FALSE, Opcode.BRANCH.rt);
		Map_GPR_32bit(Section, Opcode.BRANCH.rs, FALSE, Opcode.BRANCH.rs);
		CompX86RegToX86Reg(MipsRegLo(Opcode.BRANCH.rs), MipsRegLo(Opcode.BRANCH.rt));
	}

	b1 = (int)RecompPos + 1;
	JneLabel8("bypass TEQ", DoTrapLength);
	b2 = (int)RecompPos;

	//BreakPoint();

	if (IsMapped(Opcode.BRANCH.rt)) UnMap_GPR(Section, Opcode.BRANCH.rt, FALSE);
	if (IsMapped(Opcode.BRANCH.rs)) UnMap_GPR(Section, Opcode.BRANCH.rs, FALSE);

	CompileExit(Section->CompilePC, Section->RegWorking, DoTrap, TRUE, NULL);
	b3 = (int)RecompPos - b2;
	*(byte*)b1 = b3;

	if (IsMapped(Opcode.BRANCH.rt)) UnMap_GPR(Section, Opcode.BRANCH.rt, FALSE);
	if (IsMapped(Opcode.BRANCH.rs)) UnMap_GPR(Section, Opcode.BRANCH.rs, FALSE);
}

void Compile_R4300i_SPECIAL_TGE(BLOCK_SECTION* Section) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s", Section->CompilePC, R4300iOpcodeName(Opcode.Hex, CompilePC));

	for (int i = 0; i < 32; i++)
	{
		if (IsMapped(i)) UnMap_GPR(Section, i, TRUE);
	}

	if (Opcode.BRANCH.rt == Opcode.BRANCH.rs)
	{
		//CompileExit(Section->CompilePC, Section->RegWorking, DoTrap, TRUE, NULL);
	}
	else if (Opcode.BRANCH.rt == 0)
	{
		Map_GPR_32bit(Section, Opcode.BRANCH.rs, TRUE, Opcode.BRANCH.rs);
		CompConstToX86reg(MipsRegLo(Opcode.BRANCH.rs), 0);
		JlLabel8("bypass TGE", DoTrapLength);
		CompileExit(Section->CompilePC, Section->RegWorking, DoTrap, TRUE, NULL);
	}
	else if (Opcode.BRANCH.rs == 0)
	{
		Map_GPR_32bit(Section, Opcode.BRANCH.rt, TRUE, Opcode.BRANCH.rt);
		CompConstToX86reg(MipsRegLo(Opcode.BRANCH.rs), 0);
		JlLabel8("bypass TGE", DoTrapLength);
		CompileExit(Section->CompilePC, Section->RegWorking, DoTrap, TRUE, NULL);
	}
	else
	{
		Map_GPR_32bit(Section, Opcode.BRANCH.rt, TRUE, Opcode.BRANCH.rt);
		Map_GPR_32bit(Section, Opcode.BRANCH.rs, TRUE, Opcode.BRANCH.rs);
		CompX86RegToX86Reg(MipsRegLo(Opcode.BRANCH.rs), MipsRegLo(Opcode.BRANCH.rt));
		JlLabel8("bypass TGE", DoTrapLength);
		CompileExit(Section->CompilePC, Section->RegWorking, DoTrap, TRUE, NULL);
	}
}

void Compile_R4300i_SPECIAL_TGEU(BLOCK_SECTION* Section) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s", Section->CompilePC, R4300iOpcodeName(Opcode.Hex, CompilePC));

	for (int i = 0; i < 32; i++)
	{
		if (IsMapped(i)) UnMap_GPR(Section, i, TRUE);
	}

	if (Opcode.BRANCH.rt == Opcode.BRANCH.rs)
	{
		//CompileExit(Section->CompilePC, Section->RegWorking, DoTrap, TRUE, NULL);
	}
	else if (Opcode.BRANCH.rt == 0)
	{
		Map_GPR_32bit(Section, Opcode.BRANCH.rs, FALSE, Opcode.BRANCH.rs);
		CompConstToX86reg(MipsRegLo(Opcode.BRANCH.rs), 0);
		JneLabel8("bypass TGEU", DoTrapLength);
		CompileExit(Section->CompilePC, Section->RegWorking, DoTrap, TRUE, NULL);
	}
	else if (Opcode.BRANCH.rs == 0)
	{
		Map_GPR_32bit(Section, Opcode.BRANCH.rt, FALSE, Opcode.BRANCH.rt);
		CompConstToX86reg(MipsRegLo(Opcode.BRANCH.rs), 0);
		JneLabel8("bypass TGEU", DoTrapLength);
		CompileExit(Section->CompilePC, Section->RegWorking, DoTrap, TRUE, NULL);
	}
	else
	{
		Map_GPR_32bit(Section, Opcode.BRANCH.rt, FALSE, Opcode.BRANCH.rt);
		Map_GPR_32bit(Section, Opcode.BRANCH.rs, FALSE, Opcode.BRANCH.rs);
		CompX86RegToX86Reg(MipsRegLo(Opcode.BRANCH.rs), MipsRegLo(Opcode.BRANCH.rt));
		JneLabel8("bypass TGEU", DoTrapLength);
		CompileExit(Section->CompilePC, Section->RegWorking, DoTrap, TRUE, NULL);
	}
}

void Compile_R4300i_SPECIAL_TLT(BLOCK_SECTION* Section) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s", Section->CompilePC, R4300iOpcodeName(Opcode.Hex, CompilePC));

	for (int i = 0; i < 32; i++)
	{
		if (IsMapped(i)) UnMap_GPR(Section, i, TRUE);
	}

	if (Opcode.BRANCH.rt == Opcode.BRANCH.rs)
	{
		//CompileExit(Section->CompilePC, Section->RegWorking, DoTrap, TRUE, NULL);
	}
	else if (Opcode.BRANCH.rt == 0)
	{
		Map_GPR_32bit(Section, Opcode.BRANCH.rs, TRUE, Opcode.BRANCH.rs);
		CompConstToX86reg(MipsRegLo(Opcode.BRANCH.rs), 0);
		JgeLabel32("bypass TLT", DoTrapLength32);
		CompileExit(Section->CompilePC, Section->RegWorking, DoTrap, TRUE, NULL);
	}
	else if (Opcode.BRANCH.rs == 0)
	{
		Map_GPR_32bit(Section, Opcode.BRANCH.rt, TRUE, Opcode.BRANCH.rt);
		CompConstToX86reg(MipsRegLo(Opcode.BRANCH.rs), 0);
		JgeLabel32("bypass TLT", DoTrapLength32);
		CompileExit(Section->CompilePC, Section->RegWorking, DoTrap, TRUE, NULL);
	}
	else
	{
		Map_GPR_32bit(Section, Opcode.BRANCH.rt, TRUE, Opcode.BRANCH.rt);
		Map_GPR_32bit(Section, Opcode.BRANCH.rs, TRUE, Opcode.BRANCH.rs);
		CompX86RegToX86Reg(MipsRegLo(Opcode.BRANCH.rs), MipsRegLo(Opcode.BRANCH.rt));
		JgeLabel32("bypass TLT", DoTrapLength32);
		CompileExit(Section->CompilePC, Section->RegWorking, DoTrap, TRUE, NULL);
	}
}

void Compile_R4300i_SPECIAL_TLTU(BLOCK_SECTION* Section) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s", Section->CompilePC, R4300iOpcodeName(Opcode.Hex, CompilePC));

	for (int i = 0; i < 32; i++)
	{
		if (IsMapped(i)) UnMap_GPR(Section, i, TRUE);
	}

	if (Opcode.BRANCH.rt == Opcode.BRANCH.rs)
	{
		//CompileExit(Section->CompilePC, Section->RegWorking, DoTrap, TRUE, NULL);
	}
	else if (Opcode.BRANCH.rt == 0)
	{
		Map_GPR_32bit(Section, Opcode.BRANCH.rs, FALSE, Opcode.BRANCH.rs);
		CompConstToX86reg(MipsRegLo(Opcode.BRANCH.rs), 0);
		JgeLabel32("bypass TLTU", DoTrapLength32);
		CompileExit(Section->CompilePC, Section->RegWorking, DoTrap, TRUE, NULL);
	}
	else if (Opcode.BRANCH.rs == 0)
	{
		Map_GPR_32bit(Section, Opcode.BRANCH.rt, FALSE, Opcode.BRANCH.rt);
		CompConstToX86reg(MipsRegLo(Opcode.BRANCH.rs), 0);
		JgeLabel32("bypass TLTU", DoTrapLength32);
		CompileExit(Section->CompilePC, Section->RegWorking, DoTrap, TRUE, NULL);
	}
	else
	{
		Map_GPR_32bit(Section, Opcode.BRANCH.rt, FALSE, Opcode.BRANCH.rt);
		Map_GPR_32bit(Section, Opcode.BRANCH.rs, FALSE, Opcode.BRANCH.rs);
		CompX86RegToX86Reg(MipsRegLo(Opcode.BRANCH.rs), MipsRegLo(Opcode.BRANCH.rt));
		JgeLabel32("bypass TLT", DoTrapLength32);
		CompileExit(Section->CompilePC, Section->RegWorking, DoTrap, TRUE, NULL);
	}
}

void Compile_R4300i_SPECIAL_TNE(BLOCK_SECTION* Section) {
	//CompileExit(Section->CompilePC, Section->RegWorking, DoTrap, TRUE, NULL);
}


void Compile_R4300i_SPECIAL_MFLO (BLOCK_SECTION * Section) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
	if (Opcode.REG.rd == 0) { return; }

	Map_GPR_64bit(Section,Opcode.REG.rd,-1);
	MoveVariableToX86reg(&LO.UW[0],"LO.UW[0]",MipsRegLo(Opcode.REG.rd));
	MoveVariableToX86reg(&LO.UW[1],"LO.UW[1]",MipsRegHi(Opcode.REG.rd));
}

void Compile_R4300i_SPECIAL_MTLO (BLOCK_SECTION * Section) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));

	if (IsKnown(Opcode.BRANCH.rs) && IsConst(Opcode.BRANCH.rs)) {
		if (Is64Bit(Opcode.BRANCH.rs)) {
			MoveConstToVariable(MipsRegHi(Opcode.BRANCH.rs),&LO.UW[1],"LO.UW[1]");
		} else if (IsSigned(Opcode.BRANCH.rs) && ((MipsRegLo(Opcode.BRANCH.rs) & 0x80000000) != 0)) {
			MoveConstToVariable(0xFFFFFFFF,&LO.UW[1],"LO.UW[1]");
		} else {
			MoveConstToVariable(0,&LO.UW[1],"LO.UW[1]");
		}
		MoveConstToVariable(MipsRegLo(Opcode.BRANCH.rs), &LO.UW[0],"LO.UW[0]");
	} else if (IsKnown(Opcode.BRANCH.rs) && IsMapped(Opcode.BRANCH.rs)) {
		if (Is64Bit(Opcode.BRANCH.rs)) {
			MoveX86regToVariable(MipsRegHi(Opcode.BRANCH.rs),&LO.UW[1],"LO.UW[1]");
		} else if (IsSigned(Opcode.BRANCH.rs)) {
			MoveX86regToVariable(Map_TempReg(Section,x86_Any,Opcode.BRANCH.rs,TRUE),&LO.UW[1],"LO.UW[1]");
		} else {
			MoveConstToVariable(0,&LO.UW[1],"LO.UW[1]");
		}
		MoveX86regToVariable(MipsRegLo(Opcode.BRANCH.rs), &LO.UW[0],"LO.UW[0]");
	} else {
		int x86reg = Map_TempReg(Section,x86_Any,Opcode.BRANCH.rs,TRUE);
		MoveX86regToVariable(x86reg,&LO.UW[1],"LO.UW[1]");
		MoveX86regToVariable(Map_TempReg(Section,x86reg,Opcode.BRANCH.rs,FALSE), &LO.UW[0],"LO.UW[0]");
	}
}

void Compile_R4300i_SPECIAL_MFHI (BLOCK_SECTION * Section) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
	if (Opcode.REG.rd == 0) { return; }

	Map_GPR_64bit(Section,Opcode.REG.rd,-1);
	MoveVariableToX86reg(&HI.UW[0],"HI.UW[0]",MipsRegLo(Opcode.REG.rd));
	MoveVariableToX86reg(&HI.UW[1],"HI.UW[1]",MipsRegHi(Opcode.REG.rd));
}

void Compile_R4300i_SPECIAL_MTHI (BLOCK_SECTION * Section) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
	if (IsKnown(Opcode.BRANCH.rs) && IsConst(Opcode.BRANCH.rs)) {
		if (Is64Bit(Opcode.BRANCH.rs)) {
			MoveConstToVariable(MipsRegHi(Opcode.BRANCH.rs),&HI.UW[1],"HI.UW[1]");
		} else if (IsSigned(Opcode.BRANCH.rs) && ((MipsRegLo(Opcode.BRANCH.rs) & 0x80000000) != 0)) {
			MoveConstToVariable(0xFFFFFFFF,&HI.UW[1],"HI.UW[1]");
		} else {
			MoveConstToVariable(0,&HI.UW[1],"HI.UW[1]");
		}
		MoveConstToVariable(MipsRegLo(Opcode.BRANCH.rs), &HI.UW[0],"HI.UW[0]");
	} else if (IsKnown(Opcode.BRANCH.rs) && IsMapped(Opcode.BRANCH.rs)) {
		if (Is64Bit(Opcode.BRANCH.rs)) {
			MoveX86regToVariable(MipsRegHi(Opcode.BRANCH.rs),&HI.UW[1],"HI.UW[1]");
		} else if (IsSigned(Opcode.BRANCH.rs)) {
			MoveX86regToVariable(Map_TempReg(Section,x86_Any,Opcode.BRANCH.rs,TRUE),&HI.UW[1],"HI.UW[1]");
		} else {
			MoveConstToVariable(0,&HI.UW[1],"HI.UW[1]");
		}
		MoveX86regToVariable(MipsRegLo(Opcode.BRANCH.rs), &HI.UW[0],"HI.UW[0]");
	} else {
		int x86reg = Map_TempReg(Section,x86_Any,Opcode.BRANCH.rs,TRUE);
		MoveX86regToVariable(x86reg,&HI.UW[1],"HI.UW[1]");
		MoveX86regToVariable(Map_TempReg(Section,x86reg,Opcode.BRANCH.rs,FALSE), &HI.UW[0],"HI.UW[0]");
	}
}

void Compile_R4300i_SPECIAL_DSLLV (BLOCK_SECTION * Section) {
	BYTE * Jump[2];
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
	if (Opcode.REG.rd == 0) { return; }
	
	if (IsConst(Opcode.BRANCH.rs)) {
//		DWORD Shift = (MipsRegLo(Opcode.BRANCH.rs) & 0x3F);
		Compile_R4300i_UnknownOpcode(Section);
		return;
	}
	Map_TempReg(Section,x86_ECX,Opcode.BRANCH.rs,FALSE);
	AndConstToX86Reg(x86_ECX,0x3F);
	Map_GPR_64bit(Section,Opcode.REG.rd,Opcode.BRANCH.rt);
	CompConstToX86reg(x86_ECX,0x20);
	JaeLabel8("MORE32", 0);
	Jump[0] = RecompPos - 1;
	ShiftLeftDouble(MipsRegHi(Opcode.REG.rd),MipsRegLo(Opcode.REG.rd));
	ShiftLeftSign(MipsRegLo(Opcode.REG.rd));
	JmpLabel8("continue", 0);
	Jump[1] = RecompPos - 1;
	
	//MORE32:
	CPU_Message("");
	CPU_Message("      MORE32:");
	*((BYTE *)(Jump[0]))=(BYTE)(RecompPos - Jump[0] - 1);
	MoveX86RegToX86Reg(MipsRegLo(Opcode.REG.rd),MipsRegHi(Opcode.REG.rd));
	XorX86RegToX86Reg(MipsRegLo(Opcode.REG.rd),MipsRegLo(Opcode.REG.rd));
	AndConstToX86Reg(x86_ECX,0x1F);
	ShiftLeftSign(MipsRegHi(Opcode.REG.rd));

	//continue:
	CPU_Message("");
	CPU_Message("      continue:");
	*((BYTE *)(Jump[1]))=(BYTE)(RecompPos - Jump[1] - 1);
}

void Compile_R4300i_SPECIAL_DSRLV (BLOCK_SECTION * Section) {
	BYTE * Jump[2];
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
	if (Opcode.REG.rd == 0) { return; }
	
	if (IsConst(Opcode.BRANCH.rs)) {
		DWORD Shift = (MipsRegLo(Opcode.BRANCH.rs) & 0x3F);
		if (IsConst(Opcode.BRANCH.rt)) {
			if (IsMapped(Opcode.REG.rd)) { UnMap_GPR(Section,Opcode.REG.rd, FALSE); }
			MipsReg(Opcode.REG.rd) = Is64Bit(Opcode.BRANCH.rt)?MipsReg(Opcode.BRANCH.rt):(_int64)MipsRegLo_S(Opcode.BRANCH.rt);
			MipsReg(Opcode.REG.rd) = MipsReg(Opcode.REG.rd) >> Shift;
			if ((MipsRegHi(Opcode.REG.rd) == 0) && (MipsRegLo(Opcode.REG.rd) & 0x80000000) == 0) {
				MipsRegState(Opcode.REG.rd) = STATE_CONST_32;
			} else if ((MipsRegHi(Opcode.REG.rd) == 0xFFFFFFFF) && (MipsRegLo(Opcode.REG.rd) & 0x80000000) != 0) {
				MipsRegState(Opcode.REG.rd) = STATE_CONST_32;
			} else {
				MipsRegState(Opcode.REG.rd) = STATE_CONST_64;
			}
			return;
		}
		//if (Shift < 0x20) {
		//} else {
		//}
		//Compile_R4300i_UnknownOpcode(Section);
		//return;
	}
	Map_TempReg(Section,x86_ECX,Opcode.BRANCH.rs,FALSE);
	AndConstToX86Reg(x86_ECX,0x3F);
	Map_GPR_64bit(Section,Opcode.REG.rd,Opcode.BRANCH.rt);
	CompConstToX86reg(x86_ECX,0x20);
	JaeLabel8("MORE32", 0);
	Jump[0] = RecompPos - 1;
	ShiftRightDouble(MipsRegLo(Opcode.REG.rd),MipsRegHi(Opcode.REG.rd));
	ShiftRightUnsign(MipsRegHi(Opcode.REG.rd));
	JmpLabel8("continue", 0);
	Jump[1] = RecompPos - 1;
	
	//MORE32:
	CPU_Message("");
	CPU_Message("      MORE32:");
	*((BYTE *)(Jump[0]))=(BYTE)(RecompPos - Jump[0] - 1);
	MoveX86RegToX86Reg(MipsRegHi(Opcode.REG.rd),MipsRegLo(Opcode.REG.rd));
	XorX86RegToX86Reg(MipsRegHi(Opcode.REG.rd),MipsRegHi(Opcode.REG.rd));
	AndConstToX86Reg(x86_ECX,0x1F);
	ShiftRightUnsign(MipsRegLo(Opcode.REG.rd));

	//continue:
	CPU_Message("");
	CPU_Message("      continue:");
	*((BYTE *)(Jump[1]))=(BYTE)(RecompPos - Jump[1] - 1);
}

void Compile_R4300i_SPECIAL_DSRAV (BLOCK_SECTION * Section) {
	BYTE * Jump[2];
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
	if (Opcode.REG.rd == 0) { return; }
	
	if (IsConst(Opcode.BRANCH.rs)) {
//		DWORD Shift = (MipsRegLo(Opcode.BRANCH.rs) & 0x3F);
		Compile_R4300i_UnknownOpcode(Section);
		return;
	}
	Map_TempReg(Section,x86_ECX,Opcode.BRANCH.rs,FALSE);
	AndConstToX86Reg(x86_ECX,0x3F);
	Map_GPR_64bit(Section,Opcode.REG.rd,Opcode.BRANCH.rt);
	CompConstToX86reg(x86_ECX,0x20);
	JaeLabel8("MORE32", 0);
	Jump[0] = RecompPos - 1;
	ShiftRightDouble(MipsRegLo(Opcode.REG.rd),MipsRegHi(Opcode.REG.rd));
	ShiftRightSign(MipsRegHi(Opcode.REG.rd));
	JmpLabel8("continue", 0);
	Jump[1] = RecompPos - 1;
	
	//MORE32:
	CPU_Message("");
	CPU_Message("      MORE32:");
	*((BYTE *)(Jump[0]))=(BYTE)(RecompPos - Jump[0] - 1);
	MoveX86RegToX86Reg(MipsRegHi(Opcode.REG.rd),MipsRegLo(Opcode.REG.rd));
	ShiftRightSignImmed(MipsRegHi(Opcode.REG.rd),0x1F);
	AndConstToX86Reg(x86_ECX,0x1F);
	ShiftRightSign(MipsRegLo(Opcode.REG.rd));

	//continue:
	CPU_Message("");
	CPU_Message("      continue:");
	*((BYTE *)(Jump[1]))=(BYTE)(RecompPos - Jump[1] - 1);
}

void Compile_R4300i_SPECIAL_MULT ( BLOCK_SECTION * Section) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));

	x86Protected(x86_EDX) = TRUE;
	Map_TempReg(Section,x86_EAX,Opcode.BRANCH.rs,FALSE);
	x86Protected(x86_EDX) = FALSE;
	Map_TempReg(Section,x86_EDX,Opcode.BRANCH.rt,FALSE);

	imulX86reg(x86_EDX);

	MoveX86regToVariable(x86_EAX,&LO.UW[0],"LO.UW[0]");
	MoveX86regToVariable(x86_EDX,&HI.UW[0],"HI.UW[0]");
	ShiftRightSignImmed(x86_EAX,31);	/* paired */
	ShiftRightSignImmed(x86_EDX,31);
	MoveX86regToVariable(x86_EAX,&LO.UW[1],"LO.UW[1]");
	MoveX86regToVariable(x86_EDX,&HI.UW[1],"HI.UW[1]");
}

void Compile_R4300i_SPECIAL_MULTU (BLOCK_SECTION * Section) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));

	x86Protected(x86_EDX) = TRUE;
	Map_TempReg(Section,x86_EAX,Opcode.BRANCH.rs,FALSE);
	x86Protected(x86_EDX) = FALSE;
	Map_TempReg(Section,x86_EDX,Opcode.BRANCH.rt,FALSE);

	MulX86reg(x86_EDX);

	MoveX86regToVariable(x86_EAX,&LO.UW[0],"LO.UW[0]");
	MoveX86regToVariable(x86_EDX,&HI.UW[0],"HI.UW[0]");
	ShiftRightSignImmed(x86_EAX,31);	/* paired */
	ShiftRightSignImmed(x86_EDX,31);
	MoveX86regToVariable(x86_EAX,&LO.UW[1],"LO.UW[1]");
	MoveX86regToVariable(x86_EDX,&HI.UW[1],"HI.UW[1]");
}

void Compile_R4300i_SPECIAL_DIV (BLOCK_SECTION * Section) {
	BYTE *Jump[4] = { 0 };
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
	
	if (IsConst(Opcode.BRANCH.rt)) {
		if (MipsRegLo(Opcode.BRANCH.rs) == INT_MIN && MipsRegLo(Opcode.BRANCH.rt) == -1) {
			// An overflow exception never occurs. This is the only set of inputs that overflows on x86
			MoveConstToVariable(UINT_MAX / 2 + 1, &LO.UW[0], "LO.UW[0]");
			MoveConstToVariable(0, &LO.UW[1], "LO.UW[1]");
			MoveConstToVariable(0, &HI.UW[0], "HI.UW[0]");
			MoveConstToVariable(0, &HI.UW[1], "HI.UW[1]");
			return;
		}
		if (MipsRegLo(Opcode.BRANCH.rt) == 0) {
			MoveConstToVariable(0, &LO.UW[0], "LO.UW[0]");
			MoveConstToVariable(0, &LO.UW[1], "LO.UW[1]");
			MoveConstToVariable(0, &HI.UW[0], "HI.UW[0]");
			MoveConstToVariable(0, &HI.UW[1], "HI.UW[1]");
			return;
		}
	} else {
		// Check for overflow
		if (IsMapped(Opcode.BRANCH.rs)) {
			CompConstToX86reg(MipsRegLo(Opcode.BRANCH.rs), INT_MIN);
		} else {
			CompConstToVariable(INT_MIN, &GPR[Opcode.BRANCH.rs].W[0], GPR_NameLo[Opcode.BRANCH.rs]);
		}
		JneLabel8("NoOverflow", 0);
		Jump[0] = RecompPos - 1;

		if (IsMapped(Opcode.BRANCH.rt)) {
			CompConstToX86reg(MipsRegLo(Opcode.BRANCH.rt), -1);
		} else {
			CompConstToVariable(-1, &GPR[Opcode.BRANCH.rt].W[0], GPR_NameLo[Opcode.BRANCH.rt]);
		}
		JneLabel8("NoOverflow", 0);
		Jump[1] = RecompPos - 1;

		// Special case handling for overflowing division
		MoveConstToVariable(UINT_MAX / 2 + 1, &LO.UW[0], "LO.UW[0]");
		MoveConstToVariable(0, &LO.UW[1], "LO.UW[1]");
		MoveConstToVariable(0, &HI.UW[0], "HI.UW[0]");
		MoveConstToVariable(0, &HI.UW[1], "HI.UW[1]");

		JmpLabel8("EndDiv", 0);
		Jump[2] = RecompPos - 1;

		CPU_Message("");
		CPU_Message("      NoOverflow:");
		*((BYTE *)(Jump[0])) = (BYTE)(RecompPos - Jump[0] - 1);
		*((BYTE *)(Jump[1])) = (BYTE)(RecompPos - Jump[1] - 1);

		// Check for divide-by-zero
		if (IsMapped(Opcode.BRANCH.rt)) {
			CompConstToX86reg(MipsRegLo(Opcode.BRANCH.rt), 0);
		} else {
			CompConstToVariable(0, &GPR[Opcode.BRANCH.rt].W[0], GPR_NameLo[Opcode.BRANCH.rt]);
		}
		// The result for divide-by-zero is undefined, so the LO and HI registers do not need to be updated at all.
		JeLabel8("EndDiv", 0);
		Jump[3] = RecompPos - 1;
	}
	/*	lo = (SD)rs / (SD)rt;
		hi = (SD)rs % (SD)rt; */

	x86Protected(x86_EDX) = TRUE;
	Map_TempReg(Section,x86_EAX,Opcode.BRANCH.rs,FALSE);

	/* edx is the signed portion to eax */
	x86Protected(x86_EDX) = FALSE;
	Map_TempReg(Section,x86_EDX, -1, FALSE);

	MoveX86RegToX86Reg(x86_EAX, x86_EDX);
	ShiftRightSignImmed(x86_EDX,31);

	if (IsMapped(Opcode.BRANCH.rt)) {
		idivX86reg(MipsRegLo(Opcode.BRANCH.rt));
	} else {
		idivX86reg(Map_TempReg(Section,x86_Any,Opcode.BRANCH.rt,FALSE));
	}

	MoveX86regToVariable(x86_EAX,&LO.UW[0],"LO.UW[0]");
	MoveX86regToVariable(x86_EDX,&HI.UW[0],"HI.UW[0]");
	ShiftRightSignImmed(x86_EAX,31);	/* paired */
	ShiftRightSignImmed(x86_EDX,31);
	MoveX86regToVariable(x86_EAX,&LO.UW[1],"LO.UW[1]");
	MoveX86regToVariable(x86_EDX,&HI.UW[1],"HI.UW[1]");

	if (Jump[2] != NULL && Jump[3] != NULL) {
		CPU_Message("");
		CPU_Message("      EndDiv:");
		*((BYTE *)(Jump[2])) = (BYTE)(RecompPos - Jump[2] - 1);
		*((BYTE *)(Jump[3])) = (BYTE)(RecompPos - Jump[3] - 1);
	}
}

void Compile_R4300i_SPECIAL_DIVU ( BLOCK_SECTION * Section) {
	BYTE *Jump[2];
	int x86reg;
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));

	if (IsConst(Opcode.BRANCH.rt)) {
		if (MipsRegLo(Opcode.BRANCH.rt) == 0) {
			MoveConstToVariable(0, &LO.UW[0], "LO.UW[0]");
			MoveConstToVariable(0, &LO.UW[1], "LO.UW[1]");
			MoveConstToVariable(0, &HI.UW[0], "HI.UW[0]");
			MoveConstToVariable(0, &HI.UW[1], "HI.UW[1]");
			return;
		}
		Jump[1] = NULL;
	} else {
		if (IsMapped(Opcode.BRANCH.rt)) {
			CompConstToX86reg(MipsRegLo(Opcode.BRANCH.rt),0);
		} else {
			CompConstToVariable(0, &GPR[Opcode.BRANCH.rt].W[0], GPR_NameLo[Opcode.BRANCH.rt]);
		}
		JneLabel8("NoExcept", 0);
		Jump[0] = RecompPos - 1;

		MoveConstToVariable(0, &LO.UW[0], "LO.UW[0]");
		MoveConstToVariable(0, &LO.UW[1], "LO.UW[1]");
		MoveConstToVariable(0, &HI.UW[0], "HI.UW[0]");
		MoveConstToVariable(0, &HI.UW[1], "HI.UW[1]");

		JmpLabel8("EndDivu", 0);
		Jump[1] = RecompPos - 1;
		
		CPU_Message("");
		CPU_Message("      NoExcept:");
		*((BYTE *)(Jump[0]))=(BYTE)(RecompPos - Jump[0] - 1);
	}


	/*	lo = (UD)rs / (UD)rt;
		hi = (UD)rs % (UD)rt; */

	x86Protected(x86_EAX) = TRUE;
	Map_TempReg(Section,x86_EDX, 0, FALSE);
	x86Protected(x86_EAX) = FALSE;

	Map_TempReg(Section,x86_EAX,Opcode.BRANCH.rs,FALSE);
	x86reg = Map_TempReg(Section,x86_Any,Opcode.BRANCH.rt,FALSE);

	DivX86reg(x86reg);

	MoveX86regToVariable(x86_EAX,&LO.UW[0],"LO.UW[0]");
	MoveX86regToVariable(x86_EDX,&HI.UW[0],"HI.UW[0]");

	/* wouldnt these be zero (???) */

	ShiftRightSignImmed(x86_EAX,31);	/* paired */
	ShiftRightSignImmed(x86_EDX,31);
	MoveX86regToVariable(x86_EAX,&LO.UW[1],"LO.UW[1]");
	MoveX86regToVariable(x86_EDX,&HI.UW[1],"HI.UW[1]");

	if( Jump[1] != NULL ) {
		CPU_Message("");
		CPU_Message("      EndDivu:");
		*((BYTE *)(Jump[1]))=(BYTE)(RecompPos - Jump[1] - 1);
	}
}

void Compile_R4300i_SPECIAL_DMULT (BLOCK_SECTION * Section) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));

	if (Opcode.BRANCH.rs != 0) { UnMap_GPR(Section,Opcode.BRANCH.rs,TRUE); }
	if (Opcode.BRANCH.rt != 0) { UnMap_GPR(Section,Opcode.BRANCH.rt,TRUE); }
	Pushad();
	MoveConstToVariable(Opcode.Hex, &Opcode.Hex, "Opcode.Hex" );
	Call_Direct(r4300i_SPECIAL_DMULT, "r4300i_SPECIAL_DMULT");
	Popad();
}

void Compile_R4300i_SPECIAL_DMULTU (BLOCK_SECTION * Section) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));

	/* LO.UDW = (uint64)GPR[Opcode.BRANCH.rs].UW[0] * (uint64)GPR[Opcode.BRANCH.rt].UW[0]; */
	x86Protected(x86_EDX) = TRUE;
	Map_TempReg(Section,x86_EAX,Opcode.BRANCH.rs,FALSE);
	x86Protected(x86_EDX) = FALSE;
	Map_TempReg(Section,x86_EDX,Opcode.BRANCH.rt,FALSE);

	MulX86reg(x86_EDX);
	MoveX86regToVariable(x86_EAX, &LO.UW[0], "LO.UW[0]");
	MoveX86regToVariable(x86_EDX, &LO.UW[1], "LO.UW[1]");

	/* HI.UDW = (uint64)GPR[Opcode.BRANCH.rs].UW[1] * (uint64)GPR[Opcode.BRANCH.rt].UW[1]; */
	Map_TempReg(Section,x86_EAX,Opcode.BRANCH.rs,TRUE);
	Map_TempReg(Section,x86_EDX,Opcode.BRANCH.rt,TRUE);

	MulX86reg(x86_EDX);
	MoveX86regToVariable(x86_EAX, &HI.UW[0], "HI.UW[0]");
	MoveX86regToVariable(x86_EDX, &HI.UW[1], "HI.UW[1]");

	/* Tmp[0].UDW = (uint64)GPR[Opcode.BRANCH.rs].UW[1] * (uint64)GPR[Opcode.BRANCH.rt].UW[0]; */
	Map_TempReg(Section,x86_EAX,Opcode.BRANCH.rs,TRUE);
	Map_TempReg(Section,x86_EDX,Opcode.BRANCH.rt,FALSE);

	Map_TempReg(Section,x86_EBX,-1,FALSE);
	Map_TempReg(Section,x86_ECX,-1,FALSE);

	MulX86reg(x86_EDX);
	MoveX86RegToX86Reg(x86_EAX, x86_EBX); /* EDX:EAX -> ECX:EBX */
	MoveX86RegToX86Reg(x86_EDX, x86_ECX);

	/* Tmp[1].UDW = (uint64)GPR[Opcode.BRANCH.rs].UW[0] * (uint64)GPR[Opcode.BRANCH.rt].UW[1]; */
	Map_TempReg(Section,x86_EAX,Opcode.BRANCH.rs,FALSE);
	Map_TempReg(Section,x86_EDX,Opcode.BRANCH.rt,TRUE);

	MulX86reg(x86_EDX);
	Map_TempReg(Section,x86_ESI,-1,FALSE);
	Map_TempReg(Section,x86_EDI,-1,FALSE);
	MoveX86RegToX86Reg(x86_EAX, x86_ESI); /* EDX:EAX -> EDI:ESI */
	MoveX86RegToX86Reg(x86_EDX, x86_EDI);

	/* Tmp[2].UDW = (uint64)LO.UW[1] + (uint64)Tmp[0].UW[0] + (uint64)Tmp[1].UW[0]; */
	XorX86RegToX86Reg(x86_EDX, x86_EDX);
	MoveVariableToX86reg(&LO.UW[1], "LO.UW[1]", x86_EAX);
	AddX86RegToX86Reg(x86_EAX, x86_EBX);
	AddConstToX86Reg(x86_EDX, 0);
	AddX86RegToX86Reg(x86_EAX, x86_ESI);
	AddConstToX86Reg(x86_EDX, 0);			/* EDX:EAX */

	/* LO.UDW += ((uint64)Tmp[0].UW[0] + (uint64)Tmp[1].UW[0]) << 32; */
	/* [low+4] += ebx + esi */

	AddX86regToVariable(x86_EBX, &LO.UW[1], "LO.UW[1]");
	AddX86regToVariable(x86_ESI, &LO.UW[1], "LO.UW[1]");

	/* HI.UDW += (uint64)Tmp[0].UW[1] + (uint64)Tmp[1].UW[1] + Tmp[2].UW[1]; */
	/* [hi] += ecx + edi + edx */
	
	AddX86regToVariable(x86_ECX, &HI.UW[0], "HI.UW[0]");
	AdcConstToVariable(&HI.UW[1], "HI.UW[1]", 0);

	AddX86regToVariable(x86_EDI, &HI.UW[0], "HI.UW[0]");
	AdcConstToVariable(&HI.UW[1], "HI.UW[1]", 0);

	AddX86regToVariable(x86_EDX, &HI.UW[0], "HI.UW[0]");
	AdcConstToVariable(&HI.UW[1], "HI.UW[1]", 0);
}

void Compile_R4300i_SPECIAL_DDIV (BLOCK_SECTION * Section) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));

	if (Opcode.BRANCH.rt == 0) { return; }
	if (Opcode.BRANCH.rs != 0) { UnMap_GPR(Section, Opcode.BRANCH.rs, TRUE); }
	UnMap_GPR(Section, Opcode.BRANCH.rt, TRUE);
	Pushad();
	MoveConstToVariable(Opcode.Hex, &Opcode.Hex, "Opcode.Hex" );
	Call_Direct(r4300i_SPECIAL_DDIV, "r4300i_SPECIAL_DDIV");
	Popad();
}

void Compile_R4300i_SPECIAL_DDIVU (BLOCK_SECTION * Section) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));

	if (Opcode.BRANCH.rt == 0) { return; }
	if (Opcode.BRANCH.rs != 0) { UnMap_GPR(Section, Opcode.BRANCH.rs, TRUE); }
	UnMap_GPR(Section, Opcode.BRANCH.rt, TRUE);
	Pushad();
	MoveConstToVariable(Opcode.Hex, &Opcode.Hex, "Opcode.Hex" );
	Call_Direct(r4300i_SPECIAL_DDIVU, "r4300i_SPECIAL_DDIVU");
	Popad();
}

void Compile_R4300i_SPECIAL_ADD (BLOCK_SECTION * Section) {
	int source1 = Opcode.REG.rd == Opcode.BRANCH.rt?Opcode.BRANCH.rt:Opcode.BRANCH.rs;
	int source2 = Opcode.REG.rd == Opcode.BRANCH.rt?Opcode.BRANCH.rs:Opcode.BRANCH.rt;
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
	if (Opcode.REG.rd == 0) { return; }

	if (IsConst(source1) && IsConst(source2)) {
		DWORD temp = MipsRegLo(source1) + MipsRegLo(source2);
		if (IsMapped(Opcode.REG.rd)) { UnMap_GPR(Section,Opcode.REG.rd, FALSE); }
		MipsRegLo(Opcode.REG.rd) = temp;
		MipsRegState(Opcode.REG.rd) = STATE_CONST_32;
		return;
	}

	Map_GPR_32bit(Section,Opcode.REG.rd,TRUE, source1);
	if (IsConst(source2)) {
		if (MipsRegLo(source2) != 0) {
			AddConstToX86Reg(MipsRegLo(Opcode.REG.rd),MipsRegLo(source2));
		}
	} else if (IsKnown(source2) && IsMapped(source2)) {
		AddX86RegToX86Reg(MipsRegLo(Opcode.REG.rd),MipsRegLo(source2));
	} else {
		AddVariableToX86reg(MipsRegLo(Opcode.REG.rd),&GPR[source2].W[0],GPR_NameLo[source2]);
	}
}

void Compile_R4300i_SPECIAL_ADDU (BLOCK_SECTION * Section) {
	int source1 = Opcode.REG.rd == Opcode.BRANCH.rt?Opcode.BRANCH.rt:Opcode.BRANCH.rs;
	int source2 = Opcode.REG.rd == Opcode.BRANCH.rt?Opcode.BRANCH.rs:Opcode.BRANCH.rt;
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
	if (Opcode.REG.rd == 0) { return; }

	if (IsConst(source1) && IsConst(source2)) {
		DWORD temp = MipsRegLo(source1) + MipsRegLo(source2);
		if (IsMapped(Opcode.REG.rd)) { UnMap_GPR(Section,Opcode.REG.rd, FALSE); }
		MipsRegLo(Opcode.REG.rd) = temp;
		MipsRegState(Opcode.REG.rd) = STATE_CONST_32;
		return;
	}

	Map_GPR_32bit(Section,Opcode.REG.rd,TRUE, source1);
	if (IsConst(source2)) {
		if (MipsRegLo(source2) != 0) {
			AddConstToX86Reg(MipsRegLo(Opcode.REG.rd),MipsRegLo(source2));
		}
	} else if (IsKnown(source2) && IsMapped(source2)) {
		AddX86RegToX86Reg(MipsRegLo(Opcode.REG.rd),MipsRegLo(source2));
	} else {
		AddVariableToX86reg(MipsRegLo(Opcode.REG.rd),&GPR[source2].W[0],GPR_NameLo[source2]);
	}
}

void Compile_R4300i_SPECIAL_SUB (BLOCK_SECTION * Section) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
	if (Opcode.REG.rd == 0) { return; }

	if (IsConst(Opcode.BRANCH.rt)  && IsConst(Opcode.BRANCH.rs)) {
		DWORD temp = MipsRegLo(Opcode.BRANCH.rs) - MipsRegLo(Opcode.BRANCH.rt);
		if (IsMapped(Opcode.REG.rd)) { UnMap_GPR(Section,Opcode.REG.rd, FALSE); }
		MipsRegLo(Opcode.REG.rd) = temp;
		MipsRegState(Opcode.REG.rd) = STATE_CONST_32;
	} else {
		if (Opcode.REG.rd == Opcode.BRANCH.rt) {
			int x86Reg = Map_TempReg(Section,x86_Any,Opcode.BRANCH.rt,FALSE);
			Map_GPR_32bit(Section,Opcode.REG.rd,TRUE, Opcode.BRANCH.rs);
			SubX86RegToX86Reg(MipsRegLo(Opcode.REG.rd),x86Reg);
			return;
		}
		Map_GPR_32bit(Section,Opcode.REG.rd,TRUE, Opcode.BRANCH.rs);
		if (IsConst(Opcode.BRANCH.rt)) {
			SubConstFromX86Reg(MipsRegLo(Opcode.REG.rd),MipsRegLo(Opcode.BRANCH.rt));
		} else if (IsMapped(Opcode.BRANCH.rt)) {
			SubX86RegToX86Reg(MipsRegLo(Opcode.REG.rd),MipsRegLo(Opcode.BRANCH.rt));
		} else {
			SubVariableFromX86reg(MipsRegLo(Opcode.REG.rd),&GPR[Opcode.BRANCH.rt].W[0],GPR_NameLo[Opcode.BRANCH.rt]);
		}
	}
}

void Compile_R4300i_SPECIAL_SUBU (BLOCK_SECTION * Section) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
	if (Opcode.REG.rd == 0) { return; }

	if (IsConst(Opcode.BRANCH.rt)  && IsConst(Opcode.BRANCH.rs)) {
		DWORD temp = MipsRegLo(Opcode.BRANCH.rs) - MipsRegLo(Opcode.BRANCH.rt);
		if (IsMapped(Opcode.REG.rd)) { UnMap_GPR(Section,Opcode.REG.rd, FALSE); }
		MipsRegLo(Opcode.REG.rd) = temp;
		MipsRegState(Opcode.REG.rd) = STATE_CONST_32;
	} else {
		if (Opcode.REG.rd == Opcode.BRANCH.rt) {
			int x86Reg = Map_TempReg(Section,x86_Any,Opcode.BRANCH.rt,FALSE);
			Map_GPR_32bit(Section,Opcode.REG.rd,TRUE, Opcode.BRANCH.rs);
			SubX86RegToX86Reg(MipsRegLo(Opcode.REG.rd),x86Reg);
			return;
		}
		Map_GPR_32bit(Section,Opcode.REG.rd,TRUE, Opcode.BRANCH.rs);
		if (IsConst(Opcode.BRANCH.rt)) {
			SubConstFromX86Reg(MipsRegLo(Opcode.REG.rd),MipsRegLo(Opcode.BRANCH.rt));
		} else if (IsMapped(Opcode.BRANCH.rt)) {
			SubX86RegToX86Reg(MipsRegLo(Opcode.REG.rd),MipsRegLo(Opcode.BRANCH.rt));
		} else {
			SubVariableFromX86reg(MipsRegLo(Opcode.REG.rd),&GPR[Opcode.BRANCH.rt].W[0],GPR_NameLo[Opcode.BRANCH.rt]);
		}
	}
}

void Compile_R4300i_SPECIAL_AND (BLOCK_SECTION * Section) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
	if (Opcode.REG.rd == 0) { return; }
	if (IsKnown(Opcode.BRANCH.rt) && IsKnown(Opcode.BRANCH.rs)) {
		if (IsConst(Opcode.BRANCH.rt) && IsConst(Opcode.BRANCH.rs)) {
			if (Is64Bit(Opcode.BRANCH.rt) || Is64Bit(Opcode.BRANCH.rs)) {
				MipsReg(Opcode.REG.rd) = 
					(Is64Bit(Opcode.BRANCH.rt)?MipsReg(Opcode.BRANCH.rt):(_int64)MipsRegLo_S(Opcode.BRANCH.rt)) &
					(Is64Bit(Opcode.BRANCH.rs)?MipsReg(Opcode.BRANCH.rs):(_int64)MipsRegLo_S(Opcode.BRANCH.rs));
				
				if (MipsRegLo_S(Opcode.REG.rd) < 0 && MipsRegHi_S(Opcode.REG.rd) == -1){ 
					MipsRegState(Opcode.REG.rd) = STATE_CONST_32;
				} else if (MipsRegLo_S(Opcode.REG.rd) >= 0 && MipsRegHi_S(Opcode.REG.rd) == 0){ 
					MipsRegState(Opcode.REG.rd) = STATE_CONST_32;
				} else {
					MipsRegState(Opcode.REG.rd) = STATE_CONST_64;
				}
			} else {
				MipsReg(Opcode.REG.rd) = MipsRegLo(Opcode.BRANCH.rt) & MipsReg(Opcode.BRANCH.rs);
				MipsRegState(Opcode.REG.rd) = STATE_CONST_32;
			}			
		} else if (IsMapped(Opcode.BRANCH.rt) && IsMapped(Opcode.BRANCH.rs)) {
			int source1 = Opcode.REG.rd == Opcode.BRANCH.rt?Opcode.BRANCH.rt:Opcode.BRANCH.rs;
			int source2 = Opcode.REG.rd == Opcode.BRANCH.rt?Opcode.BRANCH.rs:Opcode.BRANCH.rt;
		
			ProtectGPR(Section,source1);
			ProtectGPR(Section,source2);
			if (Is32Bit(source1) && Is32Bit(source2)) {
				int Sign = (IsSigned(Opcode.BRANCH.rt) && IsSigned(Opcode.BRANCH.rs))?TRUE:FALSE;
				Map_GPR_32bit(Section,Opcode.REG.rd,Sign,source1);				
				AndX86RegToX86Reg(MipsRegLo(Opcode.REG.rd),MipsRegLo(source2));
			} else if (Is32Bit(source1) || Is32Bit(source2)) {
				if (IsUnsigned(Is32Bit(source1)?source1:source2)) {
					Map_GPR_32bit(Section,Opcode.REG.rd,FALSE,source1);
					AndX86RegToX86Reg(MipsRegLo(Opcode.REG.rd),MipsRegLo(source2));
				} else {
					Map_GPR_64bit(Section,Opcode.REG.rd,source1);
					if (Is32Bit(source2)) {
						AndX86RegToX86Reg(MipsRegHi(Opcode.REG.rd),Map_TempReg(Section,x86_Any,source2,TRUE));
					} else {
						AndX86RegToX86Reg(MipsRegHi(Opcode.REG.rd),MipsRegHi(source2));
					}
					AndX86RegToX86Reg(MipsRegLo(Opcode.REG.rd),MipsRegLo(source2));
				}
			} else {
				Map_GPR_64bit(Section,Opcode.REG.rd,source1);
				AndX86RegToX86Reg(MipsRegHi(Opcode.REG.rd),MipsRegHi(source2));
				AndX86RegToX86Reg(MipsRegLo(Opcode.REG.rd),MipsRegLo(source2));
			}
		} else {
			int ConstReg = IsConst(Opcode.BRANCH.rt)?Opcode.BRANCH.rt:Opcode.BRANCH.rs;
			int MappedReg = IsConst(Opcode.BRANCH.rt)?Opcode.BRANCH.rs:Opcode.BRANCH.rt;

			if (Is64Bit(ConstReg)) {
				if (Is32Bit(MappedReg) && IsUnsigned(MappedReg)) {
					if (MipsRegLo(ConstReg) == 0) {
						Map_GPR_32bit(Section,Opcode.REG.rd,FALSE, 0);
					} else {
						DWORD Value = MipsRegLo(ConstReg);
						Map_GPR_32bit(Section,Opcode.REG.rd,FALSE, MappedReg);
						AndConstToX86Reg(MipsRegLo(Opcode.REG.rd),Value);
					}
				} else {
					_int64 Value = MipsReg(ConstReg);
					Map_GPR_64bit(Section,Opcode.REG.rd,MappedReg);
					AndConstToX86Reg(MipsRegHi(Opcode.REG.rd),(DWORD)(Value >> 32));
					AndConstToX86Reg(MipsRegLo(Opcode.REG.rd),(DWORD)Value);
				}
			} else if (Is64Bit(MappedReg)) {
				DWORD Value = MipsRegLo(ConstReg); 
				if (Value != 0) {
					Map_GPR_32bit(Section,Opcode.REG.rd,IsSigned(ConstReg)?TRUE:FALSE,MappedReg);					
					AndConstToX86Reg(MipsRegLo(Opcode.REG.rd),(DWORD)Value);
				} else {
					Map_GPR_32bit(Section,Opcode.REG.rd,IsSigned(ConstReg)?TRUE:FALSE, 0);
				}
			} else {
				DWORD Value = MipsRegLo(ConstReg); 
				int Sign = FALSE;
				if (IsSigned(ConstReg) && IsSigned(MappedReg)) { Sign = TRUE; }				
				if (Value != 0) {
					Map_GPR_32bit(Section,Opcode.REG.rd,Sign,MappedReg);
					AndConstToX86Reg(MipsRegLo(Opcode.REG.rd),Value);
				} else {
					Map_GPR_32bit(Section,Opcode.REG.rd,FALSE, 0);
				}
			}
		}
	} else if (IsKnown(Opcode.BRANCH.rt) || IsKnown(Opcode.BRANCH.rs)) {
		DWORD KnownReg = IsKnown(Opcode.BRANCH.rt)?Opcode.BRANCH.rt:Opcode.BRANCH.rs;
		DWORD UnknownReg = IsKnown(Opcode.BRANCH.rt)?Opcode.BRANCH.rs:Opcode.BRANCH.rt;

		if (IsConst(KnownReg)) {
			if (Is64Bit(KnownReg)) {
				unsigned __int64 Value = MipsReg(KnownReg);
				Map_GPR_64bit(Section,Opcode.REG.rd,UnknownReg);
				AndConstToX86Reg(MipsRegHi(Opcode.REG.rd),(DWORD)(Value >> 32));
				AndConstToX86Reg(MipsRegLo(Opcode.REG.rd),(DWORD)Value);
			} else {
				DWORD Value = MipsRegLo(KnownReg);
				Map_GPR_32bit(Section,Opcode.REG.rd,IsSigned(KnownReg),UnknownReg);
				AndConstToX86Reg(MipsRegLo(Opcode.REG.rd),(DWORD)Value);
			}
		} else {
			ProtectGPR(Section,KnownReg);
			if (KnownReg == Opcode.REG.rd) {
				if (Is64Bit(KnownReg)) {
					Map_GPR_64bit(Section,Opcode.REG.rd,KnownReg);
					AndVariableToX86Reg(&GPR[UnknownReg].W[1],GPR_NameHi[UnknownReg],MipsRegHi(Opcode.REG.rd));
					AndVariableToX86Reg(&GPR[UnknownReg].W[0],GPR_NameLo[UnknownReg],MipsRegLo(Opcode.REG.rd));
				} else {
					Map_GPR_32bit(Section,Opcode.REG.rd,IsSigned(KnownReg),KnownReg);
					AndVariableToX86Reg(&GPR[UnknownReg].W[0],GPR_NameLo[UnknownReg],MipsRegLo(Opcode.REG.rd));
				}
			} else {
				if (Is64Bit(KnownReg)) {
					Map_GPR_64bit(Section,Opcode.REG.rd,UnknownReg);
					AndX86RegToX86Reg(MipsRegHi(Opcode.REG.rd),MipsRegHi(KnownReg));
					AndX86RegToX86Reg(MipsRegLo(Opcode.REG.rd),MipsRegLo(KnownReg));
				} else {
					Map_GPR_32bit(Section,Opcode.REG.rd,IsSigned(KnownReg),UnknownReg);
					AndX86RegToX86Reg(MipsRegLo(Opcode.REG.rd),MipsRegLo(KnownReg));
				}
			}
		}
	} else {
		Map_GPR_64bit(Section,Opcode.REG.rd,Opcode.BRANCH.rt);
		AndVariableToX86Reg(&GPR[Opcode.BRANCH.rs].W[1],GPR_NameHi[Opcode.BRANCH.rs],MipsRegHi(Opcode.REG.rd));
		AndVariableToX86Reg(&GPR[Opcode.BRANCH.rs].W[0],GPR_NameLo[Opcode.BRANCH.rs],MipsRegLo(Opcode.REG.rd));
	}
}

void Compile_R4300i_SPECIAL_OR (BLOCK_SECTION * Section) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));

	if (Opcode.REG.rd == 0) { return; }
	if (IsKnown(Opcode.BRANCH.rt) && IsKnown(Opcode.BRANCH.rs)) {
		if (IsConst(Opcode.BRANCH.rt) && IsConst(Opcode.BRANCH.rs)) {
			if (IsMapped(Opcode.REG.rd)) { UnMap_GPR(Section,Opcode.REG.rd, FALSE); }
			if (Is64Bit(Opcode.BRANCH.rt) || Is64Bit(Opcode.BRANCH.rs)) {				
				MipsReg(Opcode.REG.rd) = 
					(Is64Bit(Opcode.BRANCH.rt)?MipsReg(Opcode.BRANCH.rt):(_int64)MipsRegLo_S(Opcode.BRANCH.rt)) |
					(Is64Bit(Opcode.BRANCH.rs)?MipsReg(Opcode.BRANCH.rs):(_int64)MipsRegLo_S(Opcode.BRANCH.rs));
				if (MipsRegLo_S(Opcode.REG.rd) < 0 && MipsRegHi_S(Opcode.REG.rd) == -1){ 
					MipsRegState(Opcode.REG.rd) = STATE_CONST_32;
				} else if (MipsRegLo_S(Opcode.REG.rd) >= 0 && MipsRegHi_S(Opcode.REG.rd) == 0){ 
					MipsRegState(Opcode.REG.rd) = STATE_CONST_32;
				} else {
					MipsRegState(Opcode.REG.rd) = STATE_CONST_64;
				}
			} else {
				MipsRegLo(Opcode.REG.rd) = MipsRegLo(Opcode.BRANCH.rt) | MipsRegLo(Opcode.BRANCH.rs);
				MipsRegState(Opcode.REG.rd) = STATE_CONST_32;
			}
		} else if (IsMapped(Opcode.BRANCH.rt) && IsMapped(Opcode.BRANCH.rs)) {
			int source1 = Opcode.REG.rd == Opcode.BRANCH.rt?Opcode.BRANCH.rt:Opcode.BRANCH.rs;
			int source2 = Opcode.REG.rd == Opcode.BRANCH.rt?Opcode.BRANCH.rs:Opcode.BRANCH.rt;
			
			ProtectGPR(Section,Opcode.BRANCH.rt);
			ProtectGPR(Section,Opcode.BRANCH.rs);
			if (Is64Bit(Opcode.BRANCH.rt) || Is64Bit(Opcode.BRANCH.rs)) {
				Map_GPR_64bit(Section,Opcode.REG.rd,source1);
				if (Is64Bit(source2)) {
					OrX86RegToX86Reg(MipsRegHi(Opcode.REG.rd),MipsRegHi(source2));
				} else {
					OrX86RegToX86Reg(MipsRegHi(Opcode.REG.rd),Map_TempReg(Section,x86_Any,source2,TRUE));
				}
			} else {
				ProtectGPR(Section,source2);
				Map_GPR_32bit(Section,Opcode.REG.rd,TRUE,source1);
			}
			OrX86RegToX86Reg(MipsRegLo(Opcode.REG.rd),MipsRegLo(source2));
		} else {
			DWORD ConstReg = IsConst(Opcode.BRANCH.rt)?Opcode.BRANCH.rt:Opcode.BRANCH.rs;
			DWORD MappedReg = IsConst(Opcode.BRANCH.rt)?Opcode.BRANCH.rs:Opcode.BRANCH.rt;

			if (Is64Bit(Opcode.BRANCH.rt) || Is64Bit(Opcode.BRANCH.rs)) {
				unsigned _int64 Value;

				if (Is64Bit(ConstReg)) {
					Value = MipsReg(ConstReg);
				} else {
					Value = IsSigned(ConstReg)?MipsRegLo_S(ConstReg):MipsRegLo(ConstReg);
				}
				Map_GPR_64bit(Section,Opcode.REG.rd,MappedReg);
				if ((Value >> 32) != 0) {
					OrConstToX86Reg((DWORD)(Value >> 32),MipsRegHi(Opcode.REG.rd));
				}
				if ((DWORD)Value != 0) {
					OrConstToX86Reg((DWORD)Value,MipsRegLo(Opcode.REG.rd));
				}
			} else {
				int Value = MipsRegLo(ConstReg);
				Map_GPR_32bit(Section,Opcode.REG.rd,TRUE, MappedReg);
				if (Value != 0) { OrConstToX86Reg(Value,MipsRegLo(Opcode.REG.rd)); }
			}
		}
	} else if (IsKnown(Opcode.BRANCH.rt) || IsKnown(Opcode.BRANCH.rs)) {
		int KnownReg = IsKnown(Opcode.BRANCH.rt)?Opcode.BRANCH.rt:Opcode.BRANCH.rs;
		int UnknownReg = IsKnown(Opcode.BRANCH.rt)?Opcode.BRANCH.rs:Opcode.BRANCH.rt;
		
		if (IsConst(KnownReg)) {
			unsigned _int64 Value;

			Value = Is64Bit(KnownReg)?MipsReg(KnownReg):MipsRegLo_S(KnownReg);
			Map_GPR_64bit(Section,Opcode.REG.rd,UnknownReg);
			if ((Value >> 32) != 0) {
				OrConstToX86Reg((DWORD)(Value >> 32),MipsRegHi(Opcode.REG.rd));
			}
			if ((DWORD)Value != 0) {
				OrConstToX86Reg((DWORD)Value,MipsRegLo(Opcode.REG.rd));
			}
		} else {
			Map_GPR_64bit(Section,Opcode.REG.rd,KnownReg);
			OrVariableToX86Reg(&GPR[UnknownReg].W[1],GPR_NameHi[UnknownReg],MipsRegHi(Opcode.REG.rd));
			OrVariableToX86Reg(&GPR[UnknownReg].W[0],GPR_NameLo[UnknownReg],MipsRegLo(Opcode.REG.rd));
		}
	} else {
		Map_GPR_64bit(Section,Opcode.REG.rd,Opcode.BRANCH.rt);
		OrVariableToX86Reg(&GPR[Opcode.BRANCH.rs].W[1],GPR_NameHi[Opcode.BRANCH.rs],MipsRegHi(Opcode.REG.rd));
		OrVariableToX86Reg(&GPR[Opcode.BRANCH.rs].W[0],GPR_NameLo[Opcode.BRANCH.rs],MipsRegLo(Opcode.REG.rd));
	}
	if (SPHack && Opcode.REG.rd == 29) { 
		int count;

		for (count = 0; count < 10; count ++) { x86Protected(count) = FALSE; }
		ResetMemoryStack(Section); 
	}

}

void Compile_R4300i_SPECIAL_XOR (BLOCK_SECTION * Section) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
	if (Opcode.REG.rd == 0) { return; }

	if (Opcode.BRANCH.rt == Opcode.BRANCH.rs) {
		UnMap_GPR(Section, Opcode.REG.rd, FALSE);
		MipsRegState(Opcode.REG.rd) = STATE_CONST_32;
		MipsRegLo(Opcode.REG.rd) = 0;
		return;
	}
	if (IsKnown(Opcode.BRANCH.rt) && IsKnown(Opcode.BRANCH.rs)) {
		if (IsConst(Opcode.BRANCH.rt) && IsConst(Opcode.BRANCH.rs)) {
			if (IsMapped(Opcode.REG.rd)) { UnMap_GPR(Section,Opcode.REG.rd, FALSE); }
			if (Is64Bit(Opcode.BRANCH.rt) || Is64Bit(Opcode.BRANCH.rs)) {
				if (ShowDebugMessages)
					DisplayError("XOR 1");

				Compile_R4300i_UnknownOpcode(Section);
			} else {
				MipsRegState(Opcode.REG.rd) = STATE_CONST_32;
				MipsRegLo(Opcode.REG.rd) = MipsRegLo(Opcode.BRANCH.rt) ^ MipsRegLo(Opcode.BRANCH.rs);
			}
		} else if (IsMapped(Opcode.BRANCH.rt) && IsMapped(Opcode.BRANCH.rs)) {
			int source1 = Opcode.REG.rd == Opcode.BRANCH.rt?Opcode.BRANCH.rt:Opcode.BRANCH.rs;
			int source2 = Opcode.REG.rd == Opcode.BRANCH.rt?Opcode.BRANCH.rs:Opcode.BRANCH.rt;
			
			ProtectGPR(Section,source1);
			ProtectGPR(Section,source2);
			if (Is64Bit(Opcode.BRANCH.rt) || Is64Bit(Opcode.BRANCH.rs)) {
				Map_GPR_64bit(Section,Opcode.REG.rd,source1);
				if (Is64Bit(source2)) {
					XorX86RegToX86Reg(MipsRegHi(Opcode.REG.rd),MipsRegHi(source2));
				} else if (IsSigned(source2)) {
					XorX86RegToX86Reg(MipsRegHi(Opcode.REG.rd),Map_TempReg(Section,x86_Any,source2,TRUE));
				}
				XorX86RegToX86Reg(MipsRegLo(Opcode.REG.rd),MipsRegLo(source2));
			} else {
				if (IsSigned(Opcode.BRANCH.rt) != IsSigned(Opcode.BRANCH.rs)) {
					Map_GPR_32bit(Section,Opcode.REG.rd,TRUE,source1);
				} else {
					Map_GPR_32bit(Section,Opcode.REG.rd,IsSigned(Opcode.BRANCH.rt),source1);
				}
				XorX86RegToX86Reg(MipsRegLo(Opcode.REG.rd),MipsRegLo(source2));
			}
		} else {
			DWORD ConstReg = IsConst(Opcode.BRANCH.rt)?Opcode.BRANCH.rt:Opcode.BRANCH.rs;
			DWORD MappedReg = IsConst(Opcode.BRANCH.rt)?Opcode.BRANCH.rs:Opcode.BRANCH.rt;

			if (Is64Bit(Opcode.BRANCH.rt) || Is64Bit(Opcode.BRANCH.rs)) {
				DWORD ConstHi, ConstLo;

				ConstHi = Is32Bit(ConstReg)?(DWORD)(MipsRegLo_S(ConstReg) >> 31):MipsRegHi(ConstReg);
				ConstLo = MipsRegLo(ConstReg);
				Map_GPR_64bit(Section,Opcode.REG.rd,MappedReg);
				if (ConstHi != 0) { XorConstToX86Reg(MipsRegHi(Opcode.REG.rd),ConstHi); }
				if (ConstLo != 0) { XorConstToX86Reg(MipsRegLo(Opcode.REG.rd),ConstLo); }
			} else {
				int Value = MipsRegLo(ConstReg);
				if (IsSigned(Opcode.BRANCH.rt) != IsSigned(Opcode.BRANCH.rs)) {
					Map_GPR_32bit(Section,Opcode.REG.rd,TRUE, MappedReg);
				} else {
					Map_GPR_32bit(Section,Opcode.REG.rd,IsSigned(MappedReg)?TRUE:FALSE, MappedReg);
				}
				if (Value != 0) { XorConstToX86Reg(MipsRegLo(Opcode.REG.rd),Value); }
			}
		}
	} else if (IsKnown(Opcode.BRANCH.rt) || IsKnown(Opcode.BRANCH.rs)) {
		int KnownReg = IsKnown(Opcode.BRANCH.rt)?Opcode.BRANCH.rt:Opcode.BRANCH.rs;
		int UnknownReg = IsKnown(Opcode.BRANCH.rt)?Opcode.BRANCH.rs:Opcode.BRANCH.rt;
		
		if (IsConst(KnownReg)) {
			unsigned _int64 Value;

			if (Is64Bit(KnownReg)) {
				Value = MipsReg(KnownReg);
			} else {
				if (IsSigned(KnownReg)) {
					Value = (int)MipsRegLo(KnownReg);
				} else {
					Value = MipsRegLo(KnownReg);
				}
			}
			Map_GPR_64bit(Section,Opcode.REG.rd,UnknownReg);
			if ((Value >> 32) != 0) {
				XorConstToX86Reg(MipsRegHi(Opcode.REG.rd),(DWORD)(Value >> 32));
			}
			if ((DWORD)Value != 0) {
				XorConstToX86Reg(MipsRegLo(Opcode.REG.rd),(DWORD)Value);
			}
		} else {
			Map_GPR_64bit(Section,Opcode.REG.rd,KnownReg);
			XorVariableToX86reg(&GPR[UnknownReg].W[1],GPR_NameHi[UnknownReg],MipsRegHi(Opcode.REG.rd));
			XorVariableToX86reg(&GPR[UnknownReg].W[0],GPR_NameLo[UnknownReg],MipsRegLo(Opcode.REG.rd));
		}
	} else {
		Map_GPR_64bit(Section,Opcode.REG.rd,Opcode.BRANCH.rt);
		XorVariableToX86reg(&GPR[Opcode.BRANCH.rs].W[1],GPR_NameHi[Opcode.BRANCH.rs],MipsRegHi(Opcode.REG.rd));
		XorVariableToX86reg(&GPR[Opcode.BRANCH.rs].W[0],GPR_NameLo[Opcode.BRANCH.rs],MipsRegLo(Opcode.REG.rd));
	}
}

void Compile_R4300i_SPECIAL_NOR (BLOCK_SECTION * Section) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));

	if (Opcode.REG.rd == 0) { return; }
	if (IsKnown(Opcode.BRANCH.rt) && IsKnown(Opcode.BRANCH.rs)) {
		if (IsConst(Opcode.BRANCH.rt) && IsConst(Opcode.BRANCH.rs)) {
			if (IsMapped(Opcode.REG.rd)) { UnMap_GPR(Section,Opcode.REG.rd, FALSE); }
			if (Is64Bit(Opcode.BRANCH.rt) || Is64Bit(Opcode.BRANCH.rs)) {				
				Compile_R4300i_UnknownOpcode(Section);
			} else {
				MipsRegLo(Opcode.REG.rd) = ~(MipsRegLo(Opcode.BRANCH.rt) | MipsRegLo(Opcode.BRANCH.rs));
				MipsRegState(Opcode.REG.rd) = STATE_CONST_32;
			}
		} else if (IsMapped(Opcode.BRANCH.rt) && IsMapped(Opcode.BRANCH.rs)) {
			int source1 = Opcode.REG.rd == Opcode.BRANCH.rt?Opcode.BRANCH.rt:Opcode.BRANCH.rs;
			int source2 = Opcode.REG.rd == Opcode.BRANCH.rt?Opcode.BRANCH.rs:Opcode.BRANCH.rt;
			
			ProtectGPR(Section,source1);
			ProtectGPR(Section,source2);
			if (Is64Bit(Opcode.BRANCH.rt) || Is64Bit(Opcode.BRANCH.rs)) {
				Map_GPR_64bit(Section,Opcode.REG.rd,source1);
				if (Is64Bit(source2)) {
					OrX86RegToX86Reg(MipsRegHi(Opcode.REG.rd),MipsRegHi(source2));
				} else {
					OrX86RegToX86Reg(MipsRegHi(Opcode.REG.rd),Map_TempReg(Section,x86_Any,source2,TRUE));
				}
				OrX86RegToX86Reg(MipsRegLo(Opcode.REG.rd),MipsRegLo(source2));
				NotX86Reg(MipsRegHi(Opcode.REG.rd));
				NotX86Reg(MipsRegLo(Opcode.REG.rd));
			} else {
				ProtectGPR(Section,source2);
				if (IsSigned(Opcode.BRANCH.rt) != IsSigned(Opcode.BRANCH.rs)) {
					Map_GPR_32bit(Section,Opcode.REG.rd,TRUE,source1);
				} else {
					Map_GPR_32bit(Section,Opcode.REG.rd,IsSigned(Opcode.BRANCH.rt),source1);
				}
				OrX86RegToX86Reg(MipsRegLo(Opcode.REG.rd),MipsRegLo(source2));
				NotX86Reg(MipsRegLo(Opcode.REG.rd));
			}
		} else {
			DWORD ConstReg = IsConst(Opcode.BRANCH.rt)?Opcode.BRANCH.rt:Opcode.BRANCH.rs;
			DWORD MappedReg = IsConst(Opcode.BRANCH.rt)?Opcode.BRANCH.rs:Opcode.BRANCH.rt;

			if (Is64Bit(Opcode.BRANCH.rt) || Is64Bit(Opcode.BRANCH.rs)) {
				unsigned _int64 Value;

				if (Is64Bit(ConstReg)) {
					Value = MipsReg(ConstReg);
				} else {
					Value = IsSigned(ConstReg)?MipsRegLo_S(ConstReg):MipsRegLo(ConstReg);
				}
				Map_GPR_64bit(Section,Opcode.REG.rd,MappedReg);
				if ((Value >> 32) != 0) {
					OrConstToX86Reg((DWORD)(Value >> 32),MipsRegHi(Opcode.REG.rd));
				}
				if ((DWORD)Value != 0) {
					OrConstToX86Reg((DWORD)Value,MipsRegLo(Opcode.REG.rd));
				}
				NotX86Reg(MipsRegHi(Opcode.REG.rd));
				NotX86Reg(MipsRegLo(Opcode.REG.rd));
			} else {
				int Value = MipsRegLo(ConstReg);
				if (IsSigned(Opcode.BRANCH.rt) != IsSigned(Opcode.BRANCH.rs)) {
					Map_GPR_32bit(Section,Opcode.REG.rd,TRUE, MappedReg);
				} else {
					Map_GPR_32bit(Section,Opcode.REG.rd,IsSigned(MappedReg)?TRUE:FALSE, MappedReg);
				}
				if (Value != 0) { OrConstToX86Reg(Value,MipsRegLo(Opcode.REG.rd)); }
				NotX86Reg(MipsRegLo(Opcode.REG.rd));
			}
		}
	} else if (IsKnown(Opcode.BRANCH.rt) || IsKnown(Opcode.BRANCH.rs)) {
		int KnownReg = IsKnown(Opcode.BRANCH.rt)?Opcode.BRANCH.rt:Opcode.BRANCH.rs;
		int UnknownReg = IsKnown(Opcode.BRANCH.rt)?Opcode.BRANCH.rs:Opcode.BRANCH.rt;
		
		if (IsConst(KnownReg)) {
			unsigned _int64 Value;

			Value = Is64Bit(KnownReg)?MipsReg(KnownReg):MipsRegLo_S(KnownReg);
			Map_GPR_64bit(Section,Opcode.REG.rd,UnknownReg);
			if ((Value >> 32) != 0) {
				OrConstToX86Reg((DWORD)(Value >> 32),MipsRegHi(Opcode.REG.rd));
			}
			if ((DWORD)Value != 0) {
				OrConstToX86Reg((DWORD)Value,MipsRegLo(Opcode.REG.rd));
			}
		} else {
			Map_GPR_64bit(Section,Opcode.REG.rd,KnownReg);
			OrVariableToX86Reg(&GPR[UnknownReg].W[1],GPR_NameHi[UnknownReg],MipsRegHi(Opcode.REG.rd));
			OrVariableToX86Reg(&GPR[UnknownReg].W[0],GPR_NameLo[UnknownReg],MipsRegLo(Opcode.REG.rd));
		}
		NotX86Reg(MipsRegHi(Opcode.REG.rd));
		NotX86Reg(MipsRegLo(Opcode.REG.rd));
	} else {
		Map_GPR_64bit(Section,Opcode.REG.rd,Opcode.BRANCH.rt);
		OrVariableToX86Reg(&GPR[Opcode.BRANCH.rs].W[1],GPR_NameHi[Opcode.BRANCH.rs],MipsRegHi(Opcode.REG.rd));
		OrVariableToX86Reg(&GPR[Opcode.BRANCH.rs].W[0],GPR_NameLo[Opcode.BRANCH.rs],MipsRegLo(Opcode.REG.rd));
		NotX86Reg(MipsRegHi(Opcode.REG.rd));
		NotX86Reg(MipsRegLo(Opcode.REG.rd));
	}
}

void Compile_R4300i_SPECIAL_SLT (BLOCK_SECTION * Section) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));

	if (Opcode.REG.rd == 0) { return; }
	if (IsKnown(Opcode.BRANCH.rt) && IsKnown(Opcode.BRANCH.rs)) {
		if (IsConst(Opcode.BRANCH.rt) && IsConst(Opcode.BRANCH.rs)) {
			if (Is64Bit(Opcode.BRANCH.rt) || Is64Bit(Opcode.BRANCH.rs)) {
				DisplayError("1");
				Compile_R4300i_UnknownOpcode(Section);
			} else {
				if (IsMapped(Opcode.REG.rd)) { UnMap_GPR(Section,Opcode.REG.rd, FALSE); }
				MipsRegState(Opcode.REG.rd) = STATE_CONST_32;	
				if (MipsRegLo_S(Opcode.BRANCH.rs) < MipsRegLo_S(Opcode.BRANCH.rt)) {
					MipsRegLo(Opcode.REG.rd) = 1;
				} else {
					MipsRegLo(Opcode.REG.rd) = 0;
				}
			}
		} else if (IsMapped(Opcode.BRANCH.rt) && IsMapped(Opcode.BRANCH.rs)) {
			ProtectGPR(Section,Opcode.BRANCH.rt);
			ProtectGPR(Section,Opcode.BRANCH.rs);
			if (Is64Bit(Opcode.BRANCH.rt) || Is64Bit(Opcode.BRANCH.rs)) {
				BYTE *Jump[2];

				CompX86RegToX86Reg(
					Is64Bit(Opcode.BRANCH.rs)?MipsRegHi(Opcode.BRANCH.rs):Map_TempReg(Section,x86_Any,Opcode.BRANCH.rs,TRUE), 
					Is64Bit(Opcode.BRANCH.rt)?MipsRegHi(Opcode.BRANCH.rt):Map_TempReg(Section,x86_Any,Opcode.BRANCH.rt,TRUE)
				);
				JeLabel8("Low Compare",0);
				Jump[0] = RecompPos - 1;
				SetlVariable(&BranchCompare,"BranchCompare");
				JmpLabel8("Continue",0);
				Jump[1] = RecompPos - 1;
				
				CPU_Message("");
				CPU_Message("      Low Compare:");
				*((BYTE *)(Jump[0]))=(BYTE)(RecompPos - Jump[0] - 1);
				CompX86RegToX86Reg(MipsRegLo(Opcode.BRANCH.rs), MipsRegLo(Opcode.BRANCH.rt));
				SetbVariable(&BranchCompare,"BranchCompare");
				CPU_Message("");
				CPU_Message("      Continue:");
				*((BYTE *)(Jump[1]))=(BYTE)(RecompPos - Jump[1] - 1);
				Map_GPR_32bit(Section,Opcode.REG.rd,TRUE, -1);
				MoveVariableToX86reg(&BranchCompare,"BranchCompare",MipsRegLo(Opcode.REG.rd));
			} else {
				Map_GPR_32bit(Section,Opcode.REG.rd,TRUE, -1);
				CompX86RegToX86Reg(MipsRegLo(Opcode.BRANCH.rs), MipsRegLo(Opcode.BRANCH.rt));

				if (MipsRegLo(Opcode.REG.rd) > x86_EDX) {
					SetlVariable(&BranchCompare,"BranchCompare");
					MoveVariableToX86reg(&BranchCompare,"BranchCompare",MipsRegLo(Opcode.REG.rd));
				} else {					
					Setl(MipsRegLo(Opcode.REG.rd));
					AndConstToX86Reg(MipsRegLo(Opcode.REG.rd), 1);
				}
			}
		} else {
			DWORD ConstReg  = IsConst(Opcode.BRANCH.rs)?Opcode.BRANCH.rs:Opcode.BRANCH.rt;
			DWORD MappedReg = IsConst(Opcode.BRANCH.rs)?Opcode.BRANCH.rt:Opcode.BRANCH.rs;

			ProtectGPR(Section,MappedReg);
			if (Is64Bit(Opcode.BRANCH.rt) || Is64Bit(Opcode.BRANCH.rs)) {
				BYTE *Jump[2];

				CompConstToX86reg(
					Is64Bit(MappedReg)?MipsRegHi(MappedReg):Map_TempReg(Section,x86_Any,MappedReg,TRUE), 
					Is64Bit(ConstReg)?MipsRegHi(ConstReg):(MipsRegLo_S(ConstReg) >> 31)
				);
				JeLabel8("Low Compare",0);
				Jump[0] = RecompPos - 1;
				if (MappedReg == Opcode.BRANCH.rs) {
					SetlVariable(&BranchCompare,"BranchCompare");
				} else {
					SetgVariable(&BranchCompare,"BranchCompare");
				}
				JmpLabel8("Continue",0);
				Jump[1] = RecompPos - 1;
				
				CPU_Message("");
				CPU_Message("      Low Compare:");
				*((BYTE *)(Jump[0]))=(BYTE)(RecompPos - Jump[0] - 1);
				CompConstToX86reg(MipsRegLo(MappedReg), MipsRegLo(ConstReg));
				if (MappedReg == Opcode.BRANCH.rs) {
					SetbVariable(&BranchCompare,"BranchCompare");
				} else {
					SetaVariable(&BranchCompare,"BranchCompare");
				}
				CPU_Message("");
				CPU_Message("      Continue:");
				*((BYTE *)(Jump[1]))=(BYTE)(RecompPos - Jump[1] - 1);
				Map_GPR_32bit(Section,Opcode.REG.rd,TRUE, -1);
				MoveVariableToX86reg(&BranchCompare,"BranchCompare",MipsRegLo(Opcode.REG.rd));
			} else {
				DWORD Constant = MipsRegLo(ConstReg);
				Map_GPR_32bit(Section,Opcode.REG.rd,TRUE, -1);
				CompConstToX86reg(MipsRegLo(MappedReg), Constant);
			
				if (MipsRegLo(Opcode.REG.rd) > x86_EDX) {
					if (MappedReg == Opcode.BRANCH.rs) {
						SetlVariable(&BranchCompare,"BranchCompare");
					} else {
						SetgVariable(&BranchCompare,"BranchCompare");
					}
					MoveVariableToX86reg(&BranchCompare,"BranchCompare",MipsRegLo(Opcode.REG.rd));
				} else {					
					if (MappedReg == Opcode.BRANCH.rs) {
						Setl(MipsRegLo(Opcode.REG.rd));
					} else {
						Setg(MipsRegLo(Opcode.REG.rd));
					}
					AndConstToX86Reg(MipsRegLo(Opcode.REG.rd), 1);
				}
			}
		}		
	} else if (IsKnown(Opcode.BRANCH.rt) || IsKnown(Opcode.BRANCH.rs)) {
		DWORD KnownReg = IsKnown(Opcode.BRANCH.rt)?Opcode.BRANCH.rt:Opcode.BRANCH.rs;
		DWORD UnknownReg = IsKnown(Opcode.BRANCH.rt)?Opcode.BRANCH.rs:Opcode.BRANCH.rt;
		BYTE *Jump[2];
			
		if (IsConst(KnownReg)) {
			if (Is64Bit(KnownReg)) {
				CompConstToVariable(MipsRegHi(KnownReg),&GPR[UnknownReg].W[1],GPR_NameHi[UnknownReg]);
			} else {
				CompConstToVariable(((int)MipsRegLo(KnownReg) >> 31),&GPR[UnknownReg].W[1],GPR_NameHi[UnknownReg]);
			}
		} else {
			if (Is64Bit(KnownReg)) {
				CompX86regToVariable(MipsRegHi(KnownReg),&GPR[UnknownReg].W[1],GPR_NameHi[UnknownReg]);
			} else {
				ProtectGPR(Section,KnownReg);
				CompX86regToVariable(Map_TempReg(Section,x86_Any,KnownReg,TRUE),&GPR[UnknownReg].W[1],GPR_NameHi[UnknownReg]);
			}			
		}
		JeLabel8("Low Compare",0);
		Jump[0] = RecompPos - 1;
		if (KnownReg == (IsConst(KnownReg)?Opcode.BRANCH.rs:Opcode.BRANCH.rt)) {
			SetgVariable(&BranchCompare,"BranchCompare");
		} else {
			SetlVariable(&BranchCompare,"BranchCompare");
		}
		JmpLabel8("Continue",0);
		Jump[1] = RecompPos - 1;
	
		CPU_Message("");
		CPU_Message("      Low Compare:");
		*((BYTE *)(Jump[0]))=(BYTE)(RecompPos - Jump[0] - 1);
		if (IsConst(KnownReg)) {
			CompConstToVariable(MipsRegLo(KnownReg),&GPR[UnknownReg].W[0],GPR_NameLo[UnknownReg]);
		} else {
			CompX86regToVariable(MipsRegLo(KnownReg),&GPR[UnknownReg].W[0],GPR_NameLo[UnknownReg]);
		}
		if (KnownReg == (IsConst(KnownReg)?Opcode.BRANCH.rs:Opcode.BRANCH.rt)) {
			SetaVariable(&BranchCompare,"BranchCompare");
		} else {
			SetbVariable(&BranchCompare,"BranchCompare");
		}
		CPU_Message("");
		CPU_Message("      Continue:");
		*((BYTE *)(Jump[1]))=(BYTE)(RecompPos - Jump[1] - 1);
		Map_GPR_32bit(Section,Opcode.REG.rd,TRUE, -1);
		MoveVariableToX86reg(&BranchCompare,"BranchCompare",MipsRegLo(Opcode.REG.rd));
	} else {
		BYTE *Jump[2];
		int x86Reg;			

		x86Reg = Map_TempReg(Section,x86_Any,Opcode.BRANCH.rs,TRUE);
		CompX86regToVariable(x86Reg,&GPR[Opcode.BRANCH.rt].W[1],GPR_NameHi[Opcode.BRANCH.rt]);
		JeLabel8("Low Compare",0);
		Jump[0] = RecompPos - 1;
		SetlVariable(&BranchCompare,"BranchCompare");
		JmpLabel8("Continue",0);
		Jump[1] = RecompPos - 1;
		
		CPU_Message("");
		CPU_Message("      Low Compare:");
		*((BYTE *)(Jump[0]))=(BYTE)(RecompPos - Jump[0] - 1);
		CompX86regToVariable(Map_TempReg(Section,x86Reg,Opcode.BRANCH.rs,FALSE),&GPR[Opcode.BRANCH.rt].W[0],GPR_NameLo[Opcode.BRANCH.rt]);
		SetbVariable(&BranchCompare,"BranchCompare");
		CPU_Message("");
		CPU_Message("      Continue:");
		*((BYTE *)(Jump[1]))=(BYTE)(RecompPos - Jump[1] - 1);
		Map_GPR_32bit(Section,Opcode.REG.rd,TRUE, -1);
		MoveVariableToX86reg(&BranchCompare,"BranchCompare",MipsRegLo(Opcode.REG.rd));
	}
}

void Compile_R4300i_SPECIAL_SLTU (BLOCK_SECTION * Section) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
	if (Opcode.REG.rd == 0) { return; }

	if (IsKnown(Opcode.BRANCH.rt) && IsKnown(Opcode.BRANCH.rs)) {
		if (IsConst(Opcode.BRANCH.rt) && IsConst(Opcode.BRANCH.rs)) {
			if (Is64Bit(Opcode.BRANCH.rt) || Is64Bit(Opcode.BRANCH.rs)) {
				if (ShowDebugMessages)
					DisplayError("1");

				Compile_R4300i_UnknownOpcode(Section);
			} else {
				if (IsMapped(Opcode.REG.rd)) { UnMap_GPR(Section,Opcode.REG.rd, FALSE); }
				MipsRegState(Opcode.REG.rd) = STATE_CONST_32;	
				if (MipsRegLo(Opcode.BRANCH.rs) < MipsRegLo(Opcode.BRANCH.rt)) {
					MipsRegLo(Opcode.REG.rd) = 1;
				} else {
					MipsRegLo(Opcode.REG.rd) = 0;
				}
			}
		} else if (IsMapped(Opcode.BRANCH.rt) && IsMapped(Opcode.BRANCH.rs)) {
			ProtectGPR(Section,Opcode.BRANCH.rt);
			ProtectGPR(Section,Opcode.BRANCH.rs);
			if (Is64Bit(Opcode.BRANCH.rt) || Is64Bit(Opcode.BRANCH.rs)) {
				BYTE *Jump[2];

				CompX86RegToX86Reg(
					Is64Bit(Opcode.BRANCH.rs)?MipsRegHi(Opcode.BRANCH.rs):Map_TempReg(Section,x86_Any,Opcode.BRANCH.rs,TRUE), 
					Is64Bit(Opcode.BRANCH.rt)?MipsRegHi(Opcode.BRANCH.rt):Map_TempReg(Section,x86_Any,Opcode.BRANCH.rt,TRUE)
				);
				JeLabel8("Low Compare",0);
				Jump[0] = RecompPos - 1;
				SetbVariable(&BranchCompare,"BranchCompare");
				JmpLabel8("Continue",0);
				Jump[1] = RecompPos - 1;
				
				CPU_Message("");
				CPU_Message("      Low Compare:");
				*((BYTE *)(Jump[0]))=(BYTE)(RecompPos - Jump[0] - 1);
				CompX86RegToX86Reg(MipsRegLo(Opcode.BRANCH.rs), MipsRegLo(Opcode.BRANCH.rt));
				SetbVariable(&BranchCompare,"BranchCompare");
				CPU_Message("");
				CPU_Message("      Continue:");
				*((BYTE *)(Jump[1]))=(BYTE)(RecompPos - Jump[1] - 1);
				Map_GPR_32bit(Section,Opcode.REG.rd,TRUE, -1);
				MoveVariableToX86reg(&BranchCompare,"BranchCompare",MipsRegLo(Opcode.REG.rd));
			} else {
				CompX86RegToX86Reg(MipsRegLo(Opcode.BRANCH.rs), MipsRegLo(Opcode.BRANCH.rt));
				SetbVariable(&BranchCompare,"BranchCompare");
				Map_GPR_32bit(Section,Opcode.REG.rd,TRUE, -1);
				MoveVariableToX86reg(&BranchCompare,"BranchCompare",MipsRegLo(Opcode.REG.rd));
			}
		} else {
			if (Is64Bit(Opcode.BRANCH.rt) || Is64Bit(Opcode.BRANCH.rs)) {
				DWORD MappedRegHi, MappedRegLo, ConstHi, ConstLo, MappedReg, ConstReg;
				BYTE *Jump[2];

				ConstReg  = IsConst(Opcode.BRANCH.rt)?Opcode.BRANCH.rt:Opcode.BRANCH.rs;
				MappedReg = IsConst(Opcode.BRANCH.rt)?Opcode.BRANCH.rs:Opcode.BRANCH.rt;
				
				ConstLo = MipsRegLo(ConstReg);
				ConstHi = (int)ConstLo >> 31;
				if (Is64Bit(ConstReg)) { ConstHi = MipsRegHi(ConstReg); }

				ProtectGPR(Section,MappedReg);
				MappedRegLo = MipsRegLo(MappedReg);
				MappedRegHi = MipsRegHi(MappedReg);
				if (Is32Bit(MappedReg)) {
					MappedRegHi = Map_TempReg(Section,x86_Any,MappedReg,TRUE);
				}

		
				Map_GPR_32bit(Section,Opcode.REG.rd,TRUE, -1);
				CompConstToX86reg(MappedRegHi, ConstHi);
				JeLabel8("Low Compare",0);
				Jump[0] = RecompPos - 1;
				if (MappedReg == Opcode.BRANCH.rs) {
					SetbVariable(&BranchCompare,"BranchCompare");
				} else {
					SetaVariable(&BranchCompare,"BranchCompare");
				}
				JmpLabel8("Continue",0);
				Jump[1] = RecompPos - 1;
	
				CPU_Message("");
				CPU_Message("      Low Compare:");
				*((BYTE *)(Jump[0]))=(BYTE)(RecompPos - Jump[0] - 1);
				CompConstToX86reg(MappedRegLo, ConstLo);
				if (MappedReg == Opcode.BRANCH.rs) {
					SetbVariable(&BranchCompare,"BranchCompare");
				} else {
					SetaVariable(&BranchCompare,"BranchCompare");
				}
				CPU_Message("");
				CPU_Message("      Continue:");
				*((BYTE *)(Jump[1]))=(BYTE)(RecompPos - Jump[1] - 1);
				Map_GPR_32bit(Section,Opcode.REG.rd,TRUE, -1);
				MoveVariableToX86reg(&BranchCompare,"BranchCompare",MipsRegLo(Opcode.REG.rd));
			} else {
				DWORD Const = IsConst(Opcode.BRANCH.rs)?MipsRegLo(Opcode.BRANCH.rs):MipsRegLo(Opcode.BRANCH.rt);
				DWORD MappedReg = IsConst(Opcode.BRANCH.rt)?Opcode.BRANCH.rs:Opcode.BRANCH.rt;

				CompConstToX86reg(MipsRegLo(MappedReg), Const);
				if (MappedReg == Opcode.BRANCH.rs) {
					SetbVariable(&BranchCompare,"BranchCompare");
				} else {
					SetaVariable(&BranchCompare,"BranchCompare");
				}
				Map_GPR_32bit(Section,Opcode.REG.rd,TRUE, -1);
				MoveVariableToX86reg(&BranchCompare,"BranchCompare",MipsRegLo(Opcode.REG.rd));
			}
		}		
	} else if (IsKnown(Opcode.BRANCH.rt) || IsKnown(Opcode.BRANCH.rs)) {
		DWORD KnownReg = IsKnown(Opcode.BRANCH.rt)?Opcode.BRANCH.rt:Opcode.BRANCH.rs;
		DWORD UnknownReg = IsKnown(Opcode.BRANCH.rt)?Opcode.BRANCH.rs:Opcode.BRANCH.rt;
		BYTE *Jump[2];
			
		if (IsConst(KnownReg)) {
			if (Is64Bit(KnownReg)) {
				CompConstToVariable(MipsRegHi(KnownReg),&GPR[UnknownReg].W[1],GPR_NameHi[UnknownReg]);
			} else {
				CompConstToVariable(((int)MipsRegLo(KnownReg) >> 31),&GPR[UnknownReg].W[1],GPR_NameHi[UnknownReg]);
			}
		} else {
			if (Is64Bit(KnownReg)) {
				CompX86regToVariable(MipsRegHi(KnownReg),&GPR[UnknownReg].W[1],GPR_NameHi[UnknownReg]);
			} else {
				ProtectGPR(Section,KnownReg);
				CompX86regToVariable(Map_TempReg(Section,x86_Any,KnownReg,TRUE),&GPR[UnknownReg].W[1],GPR_NameHi[UnknownReg]);
			}			
		}
		JeLabel8("Low Compare",0);
		Jump[0] = RecompPos - 1;
		if (KnownReg == (IsConst(KnownReg)?Opcode.BRANCH.rs:Opcode.BRANCH.rt)) {
			SetaVariable(&BranchCompare,"BranchCompare");
		} else {
			SetbVariable(&BranchCompare,"BranchCompare");
		}
		JmpLabel8("Continue",0);
		Jump[1] = RecompPos - 1;
	
		CPU_Message("");
		CPU_Message("      Low Compare:");
		*((BYTE *)(Jump[0]))=(BYTE)(RecompPos - Jump[0] - 1);
		if (IsConst(KnownReg)) {
			CompConstToVariable(MipsRegLo(KnownReg),&GPR[UnknownReg].W[0],GPR_NameLo[UnknownReg]);
		} else {
			CompX86regToVariable(MipsRegLo(KnownReg),&GPR[UnknownReg].W[0],GPR_NameLo[UnknownReg]);
		}
		if (KnownReg == (IsConst(KnownReg)?Opcode.BRANCH.rs:Opcode.BRANCH.rt)) {
			SetaVariable(&BranchCompare,"BranchCompare");
		} else {
			SetbVariable(&BranchCompare,"BranchCompare");
		}
		CPU_Message("");
		CPU_Message("      Continue:");
		*((BYTE *)(Jump[1]))=(BYTE)(RecompPos - Jump[1] - 1);
		Map_GPR_32bit(Section,Opcode.REG.rd,TRUE, -1);
		MoveVariableToX86reg(&BranchCompare,"BranchCompare",MipsRegLo(Opcode.REG.rd));
	} else {
		BYTE *Jump[2];
		int x86Reg;			

		x86Reg = Map_TempReg(Section,x86_Any,Opcode.BRANCH.rs,TRUE);
		CompX86regToVariable(x86Reg,&GPR[Opcode.BRANCH.rt].W[1],GPR_NameHi[Opcode.BRANCH.rt]);
		JeLabel8("Low Compare",0);
		Jump[0] = RecompPos - 1;
		SetbVariable(&BranchCompare,"BranchCompare");
		JmpLabel8("Continue",0);
		Jump[1] = RecompPos - 1;
		
		CPU_Message("");
		CPU_Message("      Low Compare:");
		*((BYTE *)(Jump[0]))=(BYTE)(RecompPos - Jump[0] - 1);
		CompX86regToVariable(Map_TempReg(Section,x86Reg,Opcode.BRANCH.rs,FALSE),&GPR[Opcode.BRANCH.rt].W[0],GPR_NameLo[Opcode.BRANCH.rt]);
		SetbVariable(&BranchCompare,"BranchCompare");
		CPU_Message("");
		CPU_Message("      Continue:");
		*((BYTE *)(Jump[1]))=(BYTE)(RecompPos - Jump[1] - 1);
		Map_GPR_32bit(Section,Opcode.REG.rd,TRUE, -1);
		MoveVariableToX86reg(&BranchCompare,"BranchCompare",MipsRegLo(Opcode.REG.rd));
	}
}

void Compile_R4300i_SPECIAL_DADD (BLOCK_SECTION * Section) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
	if (Opcode.REG.rd == 0) { return; }

	if (IsConst(Opcode.BRANCH.rt)  && IsConst(Opcode.BRANCH.rs)) {
		if (IsMapped(Opcode.REG.rd)) { UnMap_GPR(Section,Opcode.REG.rd, FALSE); }
		MipsReg(Opcode.REG.rd) = 
			(Is64Bit(Opcode.BRANCH.rs) ? MipsReg(Opcode.BRANCH.rs) : (_int64)MipsRegLo_S(Opcode.BRANCH.rs)) +
			(Is64Bit(Opcode.BRANCH.rt) ? MipsReg(Opcode.BRANCH.rt) : (_int64)MipsRegLo_S(Opcode.BRANCH.rt));
		if (MipsRegLo_S(Opcode.REG.rd) < 0 && MipsRegHi_S(Opcode.REG.rd) == -1){ 
			MipsRegState(Opcode.REG.rd) = STATE_CONST_32;
		} else if (MipsRegLo_S(Opcode.REG.rd) >= 0 && MipsRegHi_S(Opcode.REG.rd) == 0){ 
			MipsRegState(Opcode.REG.rd) = STATE_CONST_32;
		} else {
			MipsRegState(Opcode.REG.rd) = STATE_CONST_64;
		}
	} else {
		ProtectGPR(Section, Opcode.BRANCH.rt);
		Map_GPR_64bit(Section,Opcode.REG.rd,Opcode.BRANCH.rs);
		if (IsConst(Opcode.BRANCH.rt)) {
			AddConstToX86Reg(MipsRegLo(Opcode.REG.rd),MipsRegLo(Opcode.BRANCH.rt));
			AddConstToX86Reg(MipsRegHi(Opcode.REG.rd),MipsRegHi(Opcode.BRANCH.rt));
		} else if (IsMapped(Opcode.BRANCH.rt)) {
			ProtectGPR(Section, Opcode.REG.rd);
			int HiReg = Is64Bit(Opcode.BRANCH.rt)?MipsRegHi(Opcode.BRANCH.rt):Map_TempReg(Section,x86_Any,Opcode.BRANCH.rt,TRUE);
			AddX86RegToX86Reg(MipsRegLo(Opcode.REG.rd),MipsRegLo(Opcode.BRANCH.rt));
			AdcX86RegToX86Reg(MipsRegHi(Opcode.REG.rd),HiReg);
		} else {
			AddVariableToX86reg(MipsRegLo(Opcode.REG.rd),&GPR[Opcode.BRANCH.rt].W[0],GPR_NameLo[Opcode.BRANCH.rt]);
			AdcVariableToX86reg(MipsRegHi(Opcode.REG.rd),&GPR[Opcode.BRANCH.rt].W[1],GPR_NameHi[Opcode.BRANCH.rt]);
		}
	}
}

void Compile_R4300i_SPECIAL_DADDU (BLOCK_SECTION * Section) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
	if (Opcode.REG.rd == 0) { return; }

	if (IsConst(Opcode.BRANCH.rt)  && IsConst(Opcode.BRANCH.rs)) {
		__int64 ValRs = Is64Bit(Opcode.BRANCH.rs)?MipsReg(Opcode.BRANCH.rs):(_int64)MipsRegLo_S(Opcode.BRANCH.rs);
		__int64 ValRt = Is64Bit(Opcode.BRANCH.rt)?MipsReg(Opcode.BRANCH.rt):(_int64)MipsRegLo_S(Opcode.BRANCH.rt);
		if (IsMapped(Opcode.REG.rd)) { UnMap_GPR(Section,Opcode.REG.rd, FALSE); }
		MipsReg(Opcode.REG.rd) = ValRs + ValRt;
		if ((MipsRegHi(Opcode.REG.rd) == 0) && (MipsRegLo(Opcode.REG.rd) & 0x80000000) == 0) {
			MipsRegState(Opcode.REG.rd) = STATE_CONST_32;
		} else if ((MipsRegHi(Opcode.REG.rd) == 0xFFFFFFFF) && (MipsRegLo(Opcode.REG.rd) & 0x80000000) != 0) {
			MipsRegState(Opcode.REG.rd) = STATE_CONST_32;
		} else {
			MipsRegState(Opcode.REG.rd) = STATE_CONST_64;
		}
	} else {
		ProtectGPR(Section, Opcode.BRANCH.rt);
		Map_GPR_64bit(Section,Opcode.REG.rd,Opcode.BRANCH.rs);
		if (IsConst(Opcode.BRANCH.rt)) {
			AddConstToX86Reg(MipsRegLo(Opcode.REG.rd),MipsRegLo(Opcode.BRANCH.rt));
			AddConstToX86Reg(MipsRegHi(Opcode.REG.rd),MipsRegHi(Opcode.BRANCH.rt));
		} else if (IsMapped(Opcode.BRANCH.rt)) {
			ProtectGPR(Section, Opcode.REG.rd);
			int HiReg = Is64Bit(Opcode.BRANCH.rt)?MipsRegHi(Opcode.BRANCH.rt):Map_TempReg(Section,x86_Any,Opcode.BRANCH.rt,TRUE);
			AddX86RegToX86Reg(MipsRegLo(Opcode.REG.rd),MipsRegLo(Opcode.BRANCH.rt));
			AdcX86RegToX86Reg(MipsRegHi(Opcode.REG.rd),HiReg);
		} else {
			AddVariableToX86reg(MipsRegLo(Opcode.REG.rd),&GPR[Opcode.BRANCH.rt].W[0],GPR_NameLo[Opcode.BRANCH.rt]);
			AdcVariableToX86reg(MipsRegHi(Opcode.REG.rd),&GPR[Opcode.BRANCH.rt].W[1],GPR_NameHi[Opcode.BRANCH.rt]);
		}
	}
}

void Compile_R4300i_SPECIAL_DSUB (BLOCK_SECTION * Section) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
	if (Opcode.REG.rd == 0) { return; }

	if (IsConst(Opcode.BRANCH.rt)  && IsConst(Opcode.BRANCH.rs)) {
		if (IsMapped(Opcode.REG.rd)) { UnMap_GPR(Section,Opcode.REG.rd, FALSE); }
		MipsReg(Opcode.REG.rd) = 
			(Is64Bit(Opcode.BRANCH.rs) ? MipsReg(Opcode.BRANCH.rs) : (_int64)MipsRegLo_S(Opcode.BRANCH.rs)) -
			(Is64Bit(Opcode.BRANCH.rt) ? MipsReg(Opcode.BRANCH.rt) : (_int64)MipsRegLo_S(Opcode.BRANCH.rt));
		if (MipsRegLo_S(Opcode.REG.rd) < 0 && MipsRegHi_S(Opcode.REG.rd) == -1){ 
			MipsRegState(Opcode.REG.rd) = STATE_CONST_32;
		} else if (MipsRegLo_S(Opcode.REG.rd) >= 0 && MipsRegHi_S(Opcode.REG.rd) == 0){ 
			MipsRegState(Opcode.REG.rd) = STATE_CONST_32;
		} else {
			MipsRegState(Opcode.REG.rd) = STATE_CONST_64;
		}
	} else {
		ProtectGPR(Section, Opcode.BRANCH.rt);
		if (Opcode.REG.rd == Opcode.BRANCH.rt) {
			int HiReg = Map_TempReg(Section,x86_Any,Opcode.BRANCH.rt,TRUE);
			int LoReg = Map_TempReg(Section,x86_Any,Opcode.BRANCH.rt,FALSE);
			Map_GPR_64bit(Section,Opcode.REG.rd,Opcode.BRANCH.rs);
			SubX86RegToX86Reg(MipsRegLo(Opcode.REG.rd),LoReg);
			SbbX86RegToX86Reg(MipsRegHi(Opcode.REG.rd),HiReg);
			return;
		}
		Map_GPR_64bit(Section,Opcode.REG.rd,Opcode.BRANCH.rs);
		if (IsConst(Opcode.BRANCH.rt)) {
			SubConstFromX86Reg(MipsRegLo(Opcode.REG.rd),MipsRegLo(Opcode.BRANCH.rt));
			SbbConstFromX86Reg(MipsRegHi(Opcode.REG.rd),MipsRegHi(Opcode.BRANCH.rt));
		} else if (IsMapped(Opcode.BRANCH.rt)) {
			ProtectGPR(Section, Opcode.REG.rd);
			int HiReg = Is64Bit(Opcode.BRANCH.rt)?MipsRegHi(Opcode.BRANCH.rt):Map_TempReg(Section,x86_Any,Opcode.BRANCH.rt,TRUE);
			SubX86RegToX86Reg(MipsRegLo(Opcode.REG.rd),MipsRegLo(Opcode.BRANCH.rt));
			SbbX86RegToX86Reg(MipsRegHi(Opcode.REG.rd),HiReg);
		} else {
			SubVariableFromX86reg(MipsRegLo(Opcode.REG.rd),&GPR[Opcode.BRANCH.rt].W[0],GPR_NameLo[Opcode.BRANCH.rt]);
			SbbVariableFromX86reg(MipsRegHi(Opcode.REG.rd),&GPR[Opcode.BRANCH.rt].W[1],GPR_NameHi[Opcode.BRANCH.rt]);
		}
	}
}

void Compile_R4300i_SPECIAL_DSUBU (BLOCK_SECTION * Section) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
	if (Opcode.REG.rd == 0) { return; }

	if (IsConst(Opcode.BRANCH.rt)  && IsConst(Opcode.BRANCH.rs)) {
		if (IsMapped(Opcode.REG.rd)) { UnMap_GPR(Section,Opcode.REG.rd, FALSE); }
		MipsReg(Opcode.REG.rd) = 
			(Is64Bit(Opcode.BRANCH.rs) ? MipsReg(Opcode.BRANCH.rs) : (_int64)MipsRegLo_S(Opcode.BRANCH.rs)) -
			(Is64Bit(Opcode.BRANCH.rt) ? MipsReg(Opcode.BRANCH.rt) : (_int64)MipsRegLo_S(Opcode.BRANCH.rt));
		if (MipsRegLo_S(Opcode.REG.rd) < 0 && MipsRegHi_S(Opcode.REG.rd) == -1){ 
			MipsRegState(Opcode.REG.rd) = STATE_CONST_32;
		} else if (MipsRegLo_S(Opcode.REG.rd) >= 0 && MipsRegHi_S(Opcode.REG.rd) == 0){ 
			MipsRegState(Opcode.REG.rd) = STATE_CONST_32;
		} else {
			MipsRegState(Opcode.REG.rd) = STATE_CONST_64;
		}
	} else {
		ProtectGPR(Section, Opcode.BRANCH.rt);
		if (Opcode.REG.rd == Opcode.BRANCH.rt) {
			int HiReg = Map_TempReg(Section,x86_Any,Opcode.BRANCH.rt,TRUE);
			int LoReg = Map_TempReg(Section,x86_Any,Opcode.BRANCH.rt,FALSE);
			Map_GPR_64bit(Section,Opcode.REG.rd,Opcode.BRANCH.rs);
			SubX86RegToX86Reg(MipsRegLo(Opcode.REG.rd),LoReg);
			SbbX86RegToX86Reg(MipsRegHi(Opcode.REG.rd),HiReg);
			return;
		}
		Map_GPR_64bit(Section,Opcode.REG.rd,Opcode.BRANCH.rs);
		if (IsConst(Opcode.BRANCH.rt)) {
			SubConstFromX86Reg(MipsRegLo(Opcode.REG.rd),MipsRegLo(Opcode.BRANCH.rt));
			SbbConstFromX86Reg(MipsRegHi(Opcode.REG.rd),MipsRegHi(Opcode.BRANCH.rt));
		} else if (IsMapped(Opcode.BRANCH.rt)) {
			ProtectGPR(Section, Opcode.REG.rd);
			int HiReg = Is64Bit(Opcode.BRANCH.rt)?MipsRegHi(Opcode.BRANCH.rt):Map_TempReg(Section,x86_Any,Opcode.BRANCH.rt,TRUE);
			SubX86RegToX86Reg(MipsRegLo(Opcode.REG.rd),MipsRegLo(Opcode.BRANCH.rt));
			SbbX86RegToX86Reg(MipsRegHi(Opcode.REG.rd),HiReg);
		} else {
			SubVariableFromX86reg(MipsRegLo(Opcode.REG.rd),&GPR[Opcode.BRANCH.rt].W[0],GPR_NameLo[Opcode.BRANCH.rt]);
			SbbVariableFromX86reg(MipsRegHi(Opcode.REG.rd),&GPR[Opcode.BRANCH.rt].W[1],GPR_NameHi[Opcode.BRANCH.rt]);
		}
	}
}

void Compile_R4300i_SPECIAL_DSLL (BLOCK_SECTION * Section) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));

	if (Opcode.REG.rd == 0) { return; }
	if (IsConst(Opcode.BRANCH.rt)) {
		if (IsMapped(Opcode.REG.rd)) { UnMap_GPR(Section,Opcode.REG.rd, FALSE); }

		MipsReg(Opcode.REG.rd) = (Is64Bit(Opcode.BRANCH.rt) ? MipsReg(Opcode.BRANCH.rt) : (_int64)MipsRegLo_S(Opcode.BRANCH.rt)) << Opcode.REG.sa;
		if (MipsRegLo_S(Opcode.REG.rd) < 0 && MipsRegHi_S(Opcode.REG.rd) == -1){
			MipsRegState(Opcode.REG.rd) = STATE_CONST_32;
		} else if (MipsRegLo_S(Opcode.REG.rd) >= 0 && MipsRegHi_S(Opcode.REG.rd) == 0){ 
			MipsRegState(Opcode.REG.rd) = STATE_CONST_32;
		} else {
			MipsRegState(Opcode.REG.rd) = STATE_CONST_64;
		}
		return;
	}
	
	Map_GPR_64bit(Section,Opcode.REG.rd,Opcode.BRANCH.rt);
	ShiftLeftDoubleImmed(MipsRegHi(Opcode.REG.rd),MipsRegLo(Opcode.REG.rd),(BYTE)Opcode.REG.sa);
	ShiftLeftSignImmed(	MipsRegLo(Opcode.REG.rd),(BYTE)Opcode.REG.sa);
}

void Compile_R4300i_SPECIAL_DSRL (BLOCK_SECTION * Section) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));

	if (Opcode.REG.rd == 0) { return; }
	if (IsConst(Opcode.BRANCH.rt)) {
		if (IsMapped(Opcode.REG.rd)) { UnMap_GPR(Section,Opcode.REG.rd, FALSE); }

		MipsReg(Opcode.REG.rd) = (Is64Bit(Opcode.BRANCH.rt) ? MipsReg(Opcode.BRANCH.rt) : (QWORD)MipsRegLo_S(Opcode.BRANCH.rt)) >> Opcode.REG.sa;
		if (MipsRegLo_S(Opcode.REG.rd) < 0 && MipsRegHi_S(Opcode.REG.rd) == -1){ 
			MipsRegState(Opcode.REG.rd) = STATE_CONST_32;
		} else if (MipsRegLo_S(Opcode.REG.rd) >= 0 && MipsRegHi_S(Opcode.REG.rd) == 0){ 
			MipsRegState(Opcode.REG.rd) = STATE_CONST_32;
		} else {
			MipsRegState(Opcode.REG.rd) = STATE_CONST_64;
		}
		return;
	}	
	Map_GPR_64bit(Section,Opcode.REG.rd,Opcode.BRANCH.rt);
	ShiftRightDoubleImmed(MipsRegLo(Opcode.REG.rd),MipsRegHi(Opcode.REG.rd),(BYTE)Opcode.REG.sa);
	ShiftRightUnsignImmed(MipsRegHi(Opcode.REG.rd),(BYTE)Opcode.REG.sa);
}

void Compile_R4300i_SPECIAL_DSRA (BLOCK_SECTION * Section) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));

	if (Opcode.REG.rd == 0) { return; }
	if (IsConst(Opcode.BRANCH.rt)) {
		if (IsMapped(Opcode.REG.rd)) { UnMap_GPR(Section,Opcode.REG.rd, FALSE); }

		MipsReg_S(Opcode.REG.rd) = (Is64Bit(Opcode.BRANCH.rt) ? MipsReg_S(Opcode.BRANCH.rt) : (_int64)MipsRegLo_S(Opcode.BRANCH.rt)) >> Opcode.REG.sa;
		if (MipsRegLo_S(Opcode.REG.rd) < 0 && MipsRegHi_S(Opcode.REG.rd) == -1){ 
			MipsRegState(Opcode.REG.rd) = STATE_CONST_32;
		} else if (MipsRegLo_S(Opcode.REG.rd) >= 0 && MipsRegHi_S(Opcode.REG.rd) == 0){ 
			MipsRegState(Opcode.REG.rd) = STATE_CONST_32;
		} else {
			MipsRegState(Opcode.REG.rd) = STATE_CONST_64;
		}
		return;
	}	
	Map_GPR_64bit(Section,Opcode.REG.rd,Opcode.BRANCH.rt);
	ShiftRightDoubleImmed(MipsRegLo(Opcode.REG.rd),MipsRegHi(Opcode.REG.rd),(BYTE)Opcode.REG.sa);
	ShiftRightSignImmed(MipsRegHi(Opcode.REG.rd),(BYTE)Opcode.REG.sa);
}

void Compile_R4300i_SPECIAL_DSLL32 (BLOCK_SECTION * Section) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));

	if (Opcode.REG.rd == 0) { return; }
	if (IsConst(Opcode.BRANCH.rt)) {
		if (Opcode.BRANCH.rt != Opcode.REG.rd) { UnMap_GPR(Section,Opcode.REG.rd, FALSE); }
		MipsRegHi(Opcode.REG.rd) = MipsRegLo(Opcode.BRANCH.rt) << Opcode.REG.sa;
		MipsRegLo(Opcode.REG.rd) = 0;
		if (MipsRegLo_S(Opcode.REG.rd) < 0 && MipsRegHi_S(Opcode.REG.rd) == -1){ 
			MipsRegState(Opcode.REG.rd) = STATE_CONST_32;
		} else if (MipsRegLo_S(Opcode.REG.rd) >= 0 && MipsRegHi_S(Opcode.REG.rd) == 0){ 
			MipsRegState(Opcode.REG.rd) = STATE_CONST_32;
		} else {
			MipsRegState(Opcode.REG.rd) = STATE_CONST_64;
		}

	} else if (IsMapped(Opcode.BRANCH.rt)) {
		ProtectGPR(Section,Opcode.BRANCH.rt);
		Map_GPR_64bit(Section,Opcode.REG.rd,-1);		
		if (Opcode.BRANCH.rt != Opcode.REG.rd) {
			MoveX86RegToX86Reg(MipsRegLo(Opcode.BRANCH.rt),MipsRegHi(Opcode.REG.rd));
		} else {
			int HiReg = MipsRegHi(Opcode.BRANCH.rt);
			MipsRegHi(Opcode.BRANCH.rt) = MipsRegLo(Opcode.BRANCH.rt);
			MipsRegLo(Opcode.BRANCH.rt) = HiReg;
		}
		if ((BYTE)Opcode.REG.sa != 0) {
			ShiftLeftSignImmed(MipsRegHi(Opcode.REG.rd),(BYTE)Opcode.REG.sa);
		}
		XorX86RegToX86Reg(MipsRegLo(Opcode.REG.rd),MipsRegLo(Opcode.REG.rd));
	} else {
		ProtectGPR(Section, Opcode.BRANCH.rt);
		Map_GPR_64bit(Section,Opcode.REG.rd,-1);
		MoveVariableToX86reg(&GPR[Opcode.BRANCH.rt],GPR_NameHi[Opcode.BRANCH.rt],MipsRegHi(Opcode.REG.rd));
		if ((BYTE)Opcode.REG.sa != 0) {
			ShiftLeftSignImmed(MipsRegHi(Opcode.REG.rd),(BYTE)Opcode.REG.sa);
		}
		XorX86RegToX86Reg(MipsRegLo(Opcode.REG.rd),MipsRegLo(Opcode.REG.rd));
	}
}

void Compile_R4300i_SPECIAL_DSRL32 (BLOCK_SECTION * Section) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));

	if (Opcode.REG.rd == 0) { return; }
	if (IsConst(Opcode.BRANCH.rt)) {
		if (Opcode.BRANCH.rt != Opcode.REG.rd) { UnMap_GPR(Section,Opcode.REG.rd, FALSE); }
		MipsRegState(Opcode.REG.rd) = STATE_CONST_32;
		MipsRegLo(Opcode.REG.rd) = (DWORD)(MipsReg(Opcode.BRANCH.rt) >> (Opcode.REG.sa + 32));
	} else if (IsMapped(Opcode.BRANCH.rt)) {
		ProtectGPR(Section,Opcode.BRANCH.rt);
		if (Is64Bit(Opcode.BRANCH.rt)) {
			if (Opcode.BRANCH.rt == Opcode.REG.rd) {
				int HiReg = MipsRegHi(Opcode.BRANCH.rt);
				MipsRegHi(Opcode.BRANCH.rt) = MipsRegLo(Opcode.BRANCH.rt);
				MipsRegLo(Opcode.BRANCH.rt) = HiReg;
				Map_GPR_32bit(Section,Opcode.REG.rd,FALSE,-1);
			} else {
				Map_GPR_32bit(Section,Opcode.REG.rd,FALSE,-1);
				MoveX86RegToX86Reg(MipsRegHi(Opcode.BRANCH.rt),MipsRegLo(Opcode.REG.rd));
			}
			if ((BYTE)Opcode.REG.sa != 0) {
				ShiftRightUnsignImmed(MipsRegLo(Opcode.REG.rd),(BYTE)Opcode.REG.sa);
			}
		} else {
			Compile_R4300i_UnknownOpcode(Section);
		}
	} else {
		ProtectGPR(Section, Opcode.BRANCH.rt);
		Map_GPR_32bit(Section,Opcode.REG.rd,FALSE,-1);
		MoveVariableToX86reg(&GPR[Opcode.BRANCH.rt].UW[1],GPR_NameLo[Opcode.BRANCH.rt],MipsRegLo(Opcode.REG.rd));
		if ((BYTE)Opcode.REG.sa != 0) {
			ShiftRightUnsignImmed(MipsRegLo(Opcode.REG.rd),(BYTE)Opcode.REG.sa);
		}
	}
}

void Compile_R4300i_SPECIAL_DSRA32 (BLOCK_SECTION * Section) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));

	if (Opcode.REG.rd == 0) { return; }
	if (IsConst(Opcode.BRANCH.rt)) {
		if (Opcode.BRANCH.rt != Opcode.REG.rd) { UnMap_GPR(Section,Opcode.REG.rd, FALSE); }
		MipsRegState(Opcode.REG.rd) = STATE_CONST_32;
		MipsRegLo(Opcode.REG.rd) = (DWORD)(MipsReg_S(Opcode.BRANCH.rt) >> (Opcode.REG.sa + 32));
	} else if (IsMapped(Opcode.BRANCH.rt)) {
		ProtectGPR(Section,Opcode.BRANCH.rt);
		if (Is64Bit(Opcode.BRANCH.rt)) {
			if (Opcode.BRANCH.rt == Opcode.REG.rd) {
				int HiReg = MipsRegHi(Opcode.BRANCH.rt);
				MipsRegHi(Opcode.BRANCH.rt) = MipsRegLo(Opcode.BRANCH.rt);
				MipsRegLo(Opcode.BRANCH.rt) = HiReg;
				Map_GPR_32bit(Section,Opcode.REG.rd,TRUE,-1);
			} else {
				Map_GPR_32bit(Section,Opcode.REG.rd,TRUE,-1);
				MoveX86RegToX86Reg(MipsRegHi(Opcode.BRANCH.rt),MipsRegLo(Opcode.REG.rd));
			}
			if ((BYTE)Opcode.REG.sa != 0) {
				ShiftRightSignImmed(MipsRegLo(Opcode.REG.rd),(BYTE)Opcode.REG.sa);
			}
		} else {
			Compile_R4300i_UnknownOpcode(Section);
		}
	} else {
		ProtectGPR(Section, Opcode.BRANCH.rt);
		Map_GPR_32bit(Section,Opcode.REG.rd,TRUE,-1);
		MoveVariableToX86reg(&GPR[Opcode.BRANCH.rt].UW[1],GPR_NameLo[Opcode.BRANCH.rt],MipsRegLo(Opcode.REG.rd));
		if ((BYTE)Opcode.REG.sa != 0) {
			ShiftRightSignImmed(MipsRegLo(Opcode.REG.rd),(BYTE)Opcode.REG.sa);
		}
	}
}

/************************** COP0 functions **************************/
void Compile_R4300i_COP0_MF(BLOCK_SECTION * Section) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));

	if (Opcode.BRANCH.rt == 0) { return; }

	switch (Opcode.REG.rd) {
	case 9: //Count
		AddConstToVariable(BlockCycleCount,&CP0[9],Cop0_Name[9]);
		SubConstFromVariable(BlockCycleCount,&Timers.Timer,"Timer");
		BlockCycleCount = 0;
	}
	ProtectGPR(Section, Opcode.REG.rd);
	Map_GPR_32bit(Section,Opcode.BRANCH.rt,TRUE,-1);
	MoveVariableToX86reg(&CP0[Opcode.REG.rd],Cop0_Name[Opcode.REG.rd],MipsRegLo(Opcode.BRANCH.rt));
}

void Compile_R4300i_COP0_DMF(BLOCK_SECTION * Section) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s", Section->CompilePC, R4300iOpcodeName(Opcode.Hex, CompilePC));

	if (Opcode.BRANCH.rt == 0) { return; }

	switch (Opcode.REG.rd) {
	case 9: //Count
		AddConstToVariable(BlockCycleCount, &CP0[9], Cop0_Name[9]);
		SubConstFromVariable(BlockCycleCount, &Timers.Timer, "Timer");
		BlockCycleCount = 0;
	}
	ProtectGPR(Section, Opcode.REG.rd);
	Map_GPR_64bit(Section, Opcode.BRANCH.rt, -1);
	MoveVariableToX86reg(&CP0[Opcode.REG.rd], Cop0_Name[Opcode.REG.rd], MipsRegLo(Opcode.BRANCH.rt));
	MoveX86RegToX86Reg(MipsRegLo(Opcode.BRANCH.rt), MipsRegHi(Opcode.BRANCH.rt));
	ShiftRightSignImmed(MipsRegHi(Opcode.BRANCH.rt), 31);
}

void Compile_R4300i_COP0_MT (BLOCK_SECTION * Section) {
	int OldStatusReg;
	BYTE *Jump;
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));

	switch (Opcode.REG.rd) {
	case 0: //Index
	case 2: //EntryLo0
	case 3: //EntryLo1
	case 4: //Context
	case 5: //PageMask
	case 9: //Count
	case 10: //Entry Hi
	case 11: //Compare
	case 14: //EPC
	case 16: //Config
	case 18: //WatchLo 
	case 19: //WatchHi
	case 28: //Tag lo
	case 29: //Tag Hi
	case 30: //ErrEPC
		if (IsConst(Opcode.BRANCH.rt)) {
			MoveConstToVariable(MipsRegLo(Opcode.BRANCH.rt), &CP0[Opcode.REG.rd], Cop0_Name[Opcode.REG.rd]);
		} else if (IsMapped(Opcode.BRANCH.rt)) {
			MoveX86regToVariable(MipsRegLo(Opcode.BRANCH.rt), &CP0[Opcode.REG.rd], Cop0_Name[Opcode.REG.rd]);
		} else {
			MoveX86regToVariable(Map_TempReg(Section,x86_Any,Opcode.BRANCH.rt,FALSE), &CP0[Opcode.REG.rd], Cop0_Name[Opcode.REG.rd]);
		}
		switch (Opcode.REG.rd) {
		case 4: //Context
			AndConstToVariable(0xFF800000,&CP0[Opcode.REG.rd], Cop0_Name[Opcode.REG.rd]);
			break;			
		case 9: //Count
			BlockCycleCount = 0;
			Pushad();
			Call_Direct(ChangeCompareTimer,"ChangeCompareTimer");
			Popad();
			break;
		case 11: //Compare
			AddConstToVariable(BlockCycleCount,&CP0[9],Cop0_Name[9]);
			SubConstFromVariable(BlockCycleCount,&Timers.Timer,"Timer");
			BlockCycleCount = 0;
			AndConstToVariable((DWORD)~CAUSE_IP7,&CAUSE_REGISTER,"CAUSE_REGISTER");
			Pushad();
			Call_Direct(ChangeCompareTimer,"ChangeCompareTimer");
			Popad();
		}
		break;
	case 12: //Status
		ProtectGPR(Section, Opcode.BRANCH.rt);
		OldStatusReg = Map_TempReg(Section,x86_Any,-1,FALSE);
		MoveVariableToX86reg(&CP0[Opcode.REG.rd],Cop0_Name[Opcode.REG.rd],OldStatusReg);
		if (IsConst(Opcode.BRANCH.rt)) {
			MoveConstToVariable(MipsRegLo(Opcode.BRANCH.rt), &CP0[Opcode.REG.rd], Cop0_Name[Opcode.REG.rd]);
		} else if (IsMapped(Opcode.BRANCH.rt)) {
			MoveX86regToVariable(MipsRegLo(Opcode.BRANCH.rt), &CP0[Opcode.REG.rd], Cop0_Name[Opcode.REG.rd]);
		} else {
			MoveX86regToVariable(Map_TempReg(Section,x86_Any,Opcode.BRANCH.rt,FALSE), &CP0[Opcode.REG.rd], Cop0_Name[Opcode.REG.rd]);
		}
		XorVariableToX86reg(&CP0[Opcode.REG.rd], Cop0_Name[Opcode.REG.rd],OldStatusReg);
		TestConstToX86Reg(STATUS_FR,OldStatusReg);
		JeLabel8("FpuFlagFine",0);
		Jump = RecompPos - 1;
		Pushad();
		Call_Direct(SetFpuLocations,"SetFpuLocations");
		Popad();
		*(BYTE *)(Jump)= (BYTE )(((BYTE )(RecompPos)) - (((BYTE )(Jump)) + 1));
				
		//TestConstToX86Reg(STATUS_FR,OldStatusReg);
		//CompileExit(Section->CompilePC+4,Section->RegWorking,ExitResetRecompCode,FALSE,JneLabel32);
		Pushad();
		Call_Direct(CheckInterrupts,"CheckInterrupts");
		Popad();
		break;
	case 6: //Wired
		Pushad();
		if (BlockRandomModifier != 0) { SubConstFromVariable(BlockRandomModifier,&CP0[1],Cop0_Name[1]); }
		BlockRandomModifier = 0;
		Call_Direct(FixRandomReg,"FixRandomReg");
		Popad();
		if (IsConst(Opcode.BRANCH.rt)) {
			MoveConstToVariable(MipsRegLo(Opcode.BRANCH.rt), &CP0[Opcode.REG.rd], Cop0_Name[Opcode.REG.rd]);
		} else if (IsMapped(Opcode.BRANCH.rt)) {
			MoveX86regToVariable(MipsRegLo(Opcode.BRANCH.rt), &CP0[Opcode.REG.rd], Cop0_Name[Opcode.REG.rd]);
		} else {
			MoveX86regToVariable(Map_TempReg(Section,x86_Any,Opcode.BRANCH.rt,FALSE), &CP0[Opcode.REG.rd], Cop0_Name[Opcode.REG.rd]);
		}
		break;
	case 13: //cause
		AndConstToVariable(0xFFFFCFF, &CP0[Opcode.REG.rd], Cop0_Name[Opcode.REG.rd]);

		if (IsConst(Opcode.BRANCH.rt)) {			
			if (ShowDebugMessages && (MipsRegLo(Opcode.BRANCH.rt) & 0x300) != 0 ) {
				DisplayError("Set IP0 or IP1"); 
			}
		} /*else {
			Compile_R4300i_UnknownOpcode(Section);
		}*/
		Pushad();
		Call_Direct(CheckInterrupts,"CheckInterrupts");
		Popad();
		break;
	default:
		Compile_R4300i_UnknownOpcode(Section);
	}
}

void Compile_R4300i_COP0_DMT(BLOCK_SECTION * Section) {
	int OldStatusReg;
	BYTE *Jump;
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s", Section->CompilePC, R4300iOpcodeName(Opcode.Hex, CompilePC));

	switch (Opcode.REG.rd) {
	case 0: //Index
	case 2: //EntryLo0
	case 3: //EntryLo1
	case 4: //Context
	case 5: //PageMask
	case 9: //Count
	case 10: //Entry Hi
	case 11: //Compare
	case 14: //EPC
	case 16: //Config
	case 18: //WatchLo
	case 19: //WatchHi
	case 28: //Tag lo
	case 29: //Tag Hi
	case 30: //ErrEPC
		if (IsConst(Opcode.BRANCH.rt)) {
			MoveConstToVariable(MipsRegLo(Opcode.BRANCH.rt), &CP0[Opcode.REG.rd], Cop0_Name[Opcode.REG.rd]);
		} else if (IsMapped(Opcode.BRANCH.rt)) {
			MoveX86regToVariable(MipsRegLo(Opcode.BRANCH.rt), &CP0[Opcode.REG.rd], Cop0_Name[Opcode.REG.rd]);
		} else {
			MoveX86regToVariable(Map_TempReg(Section,x86_Any,Opcode.BRANCH.rt,FALSE), &CP0[Opcode.REG.rd], Cop0_Name[Opcode.REG.rd]);
		}
		switch (Opcode.REG.rd) {
		case 4: //Context
			AndConstToVariable(0xFF800000,&CP0[Opcode.REG.rd], Cop0_Name[Opcode.REG.rd]);
			break;
		case 9: //Count
			BlockCycleCount = 0;
			Pushad();
			Call_Direct(ChangeCompareTimer,"ChangeCompareTimer");
			Popad();
			break;
		case 11: //Compare
			AddConstToVariable(BlockCycleCount,&CP0[9],Cop0_Name[9]);
			SubConstFromVariable(BlockCycleCount,&Timers.Timer,"Timer");
			BlockCycleCount = 0;
			AndConstToVariable((DWORD)~CAUSE_IP7,&CAUSE_REGISTER,"CAUSE_REGISTER");
			Pushad();
			Call_Direct(ChangeCompareTimer,"ChangeCompareTimer");
			Popad();
		}
		break;
	case 12: //Status
		ProtectGPR(Section, Opcode.BRANCH.rt);
		OldStatusReg = Map_TempReg(Section,x86_Any,-1,FALSE);
		MoveVariableToX86reg(&CP0[Opcode.REG.rd],Cop0_Name[Opcode.REG.rd],OldStatusReg);
		if (IsConst(Opcode.BRANCH.rt)) {
			MoveConstToVariable(MipsRegLo(Opcode.BRANCH.rt), &CP0[Opcode.REG.rd], Cop0_Name[Opcode.REG.rd]);
		} else if (IsMapped(Opcode.BRANCH.rt)) {
			MoveX86regToVariable(MipsRegLo(Opcode.BRANCH.rt), &CP0[Opcode.REG.rd], Cop0_Name[Opcode.REG.rd]);
		} else {
			MoveX86regToVariable(Map_TempReg(Section,x86_Any,Opcode.BRANCH.rt,FALSE), &CP0[Opcode.REG.rd], Cop0_Name[Opcode.REG.rd]);
		}
		XorVariableToX86reg(&CP0[Opcode.REG.rd], Cop0_Name[Opcode.REG.rd],OldStatusReg);
		TestConstToX86Reg(STATUS_FR,OldStatusReg);
		JeLabel8("FpuFlagFine",0);
		Jump = RecompPos - 1;
		Pushad();
		Call_Direct(SetFpuLocations,"SetFpuLocations");
		Popad();
		*(BYTE *)(Jump)= (BYTE )(((BYTE )(RecompPos)) - (((BYTE )(Jump)) + 1));

		//TestConstToX86Reg(STATUS_FR,OldStatusReg);
		//CompileExit(Section->CompilePC+4,Section->RegWorking,ExitResetRecompCode,FALSE,JneLabel32);
		Pushad();
		Call_Direct(CheckInterrupts,"CheckInterrupts");
		Popad();
		break;
	case 6: //Wired
		Pushad();
		if (BlockRandomModifier != 0) { SubConstFromVariable(BlockRandomModifier,&CP0[1],Cop0_Name[1]); }
		BlockRandomModifier = 0;
		Call_Direct(FixRandomReg,"FixRandomReg");
		Popad();
		if (IsConst(Opcode.BRANCH.rt)) {
			MoveConstToVariable(MipsRegLo(Opcode.BRANCH.rt), &CP0[Opcode.REG.rd], Cop0_Name[Opcode.REG.rd]);
		} else if (IsMapped(Opcode.BRANCH.rt)) {
			MoveX86regToVariable(MipsRegLo(Opcode.BRANCH.rt), &CP0[Opcode.REG.rd], Cop0_Name[Opcode.REG.rd]);
		} else {
			MoveX86regToVariable(Map_TempReg(Section,x86_Any,Opcode.BRANCH.rt,FALSE), &CP0[Opcode.REG.rd], Cop0_Name[Opcode.REG.rd]);
		}
		break;
	case 13: //cause
		AndConstToVariable(0xFFFFCFF, &CP0[Opcode.REG.rd], Cop0_Name[Opcode.REG.rd]);

		if (IsConst(Opcode.BRANCH.rt)) {
			if (ShowDebugMessages && (MipsRegLo(Opcode.BRANCH.rt) & 0x300) != 0 ) {
				DisplayError("Set IP0 or IP1");
			}
		} /*else {
			Compile_R4300i_UnknownOpcode(Section);
		}*/
		Pushad();
		Call_Direct(CheckInterrupts,"CheckInterrupts");
		Popad();
		break;
	default:
		Compile_R4300i_UnknownOpcode(Section);
	}
}

/************************** COP0 CO functions ***********************/
void Compile_R4300i_COP0_CO_TLBR( BLOCK_SECTION * Section) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
	if (!UseTlb) {	return; }
	Pushad();
	Call_Direct(TLB_Read,"TLB_Read");
	Popad();
}

void Compile_R4300i_COP0_CO_TLBWI( BLOCK_SECTION * Section) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
	if (!UseTlb) {	return; }
	Pushad();
	MoveVariableToX86reg(&INDEX_REGISTER,"INDEX_REGISTER",x86_ECX);
	AndConstToX86Reg(x86_ECX,0x1F);
	Call_Direct(WriteTLBEntry,"WriteTLBEntry");
	Popad();
}

void Compile_R4300i_COP0_CO_TLBWR( BLOCK_SECTION * Section) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
	if (!UseTlb) {	return; }
	if (BlockRandomModifier != 0) { SubConstFromVariable(BlockRandomModifier,&CP0[1],Cop0_Name[1]); }
	BlockRandomModifier = 0;
	Pushad();
	Call_Direct(FixRandomReg,"FixRandomReg");
	MoveVariableToX86reg(&RANDOM_REGISTER,"RANDOM_REGISTER",x86_ECX);
	AndConstToX86Reg(x86_ECX,0x1F);
	Call_Direct(WriteTLBEntry,"WriteTLBEntry");
	Popad();
}

void Compile_R4300i_COP0_CO_TLBP( BLOCK_SECTION * Section) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));
	
	if (!UseTlb) {	return; }
	Pushad();
	Call_Direct(TLB_Probe,"TLB_Probe");
	Popad();
}

void compiler_COP0_CO_ERET (void) {
	if ((STATUS_REGISTER & STATUS_ERL) != 0) {
		PROGRAM_COUNTER.DW = ERROREPC_REGISTER;
		STATUS_REGISTER &= ~STATUS_ERL;
	} else {
		PROGRAM_COUNTER.DW = EPC_REGISTER;
		STATUS_REGISTER &= ~STATUS_EXL;
	}
	LLBit = 0;
	CheckInterrupts();
}

void Compile_R4300i_COP0_CO_ERET( BLOCK_SECTION * Section) {
	MIPS_DWORD CompilePC;
	CompilePC.DW = (int)Section->CompilePC;

	CPU_Message("  %X %s",Section->CompilePC,R4300iOpcodeName(Opcode.Hex,CompilePC));

	WriteBackRegisters(Section);
	Call_Direct(compiler_COP0_CO_ERET,"compiler_COP0_CO_ERET");
	CompileExit((DWORD)-1,Section->RegWorking,Normal,TRUE,NULL);
	NextInstruction = END_BLOCK;
}

/************************** Other functions **************************/
void Compile_R4300i_UnknownOpcode (BLOCK_SECTION * Section) {
//	CPU_Message("  %X Unhandled Opcode: %s",Section->CompilePC, R4300iOpcodeName(Opcode.Hex,Section->CompilePC));
	CompileExit(Section->CompilePC, Section->RegWorking, DoIlleaglOp, TRUE, NULL);

	//FreeSection(Section->ContinueSection,Section);
	//FreeSection(Section->JumpSection,Section);
	//BlockCycleCount -= CountPerOp;
	//BlockRandomModifier -= 1;
	//MoveConstToVariable(Section->CompilePC,&PROGRAM_COUNTER,"PROGRAM_COUNTER");
	//WriteBackRegisters(Section);
	//AddConstToVariable(BlockCycleCount,&CP0[9],Cop0_Name[9]);
	//SubConstFromVariable(BlockRandomModifier,&CP0[1],Cop0_Name[1]);
	//if (CPU_Type == CPU_SyncCores) { Call_Direct(SyncToPC, "SyncToPC"); }
	//MoveConstToVariable(Opcode.Hex,&Opcode.Hex,"Opcode.Hex");
	//Call_Direct(R4300i_UnknownOpcode, "R4300i_UnknownOpcode");
	//Ret();
	//if (NextInstruction == NORMAL) { NextInstruction = END_BLOCK; }
}

