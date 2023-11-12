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
#include <windows.h>
#include <stdio.h>
#include <math.h>
#include <float.h>
#include "main.h"
#include "cpu.h"
#include "debugger.h"

int RoundingModel = _RC_NEAR;

#define ADDRESS_ERROR_EXCEPTION(Address,FromRead) \
	DoAddressError(NextInstruction == JUMP,Address,FromRead);\
	NextInstruction = JUMP;\
	JumpToLocation = PROGRAM_COUNTER;\
	return;

//#define TEST_COP1_USABLE_EXCEPTION
#define TEST_COP1_USABLE_EXCEPTION \
	if ((STATUS_REGISTER & STATUS_CU1) == 0) {\
		DoCopUnusableException(NextInstruction == JUMP,1);\
		NextInstruction = JUMP;\
		JumpToLocation = PROGRAM_COUNTER;\
		return;\
	}

#define TLB_READ_EXCEPTION(Address) \
	DoTLBMiss(NextInstruction == JUMP,Address);\
	NextInstruction = JUMP;\
	JumpToLocation = PROGRAM_COUNTER;\
	return;

/************************* OpCode functions *************************/
void _fastcall r4300i_J (void) {
	NextInstruction = DELAY_SLOT;
	JumpToLocation = (PROGRAM_COUNTER & 0xF0000000) + (Opcode.JMP.target << 2);
	TestInterpreterJump(PROGRAM_COUNTER,JumpToLocation,0,0);
}

void _fastcall r4300i_JAL (void) {
	NextInstruction = DELAY_SLOT;
	JumpToLocation = (PROGRAM_COUNTER & 0xF0000000) + (Opcode.JMP.target << 2);
	TestInterpreterJump(PROGRAM_COUNTER,JumpToLocation,0,0);
	GPR[31].DW= (long)(PROGRAM_COUNTER + 8);
}

void _fastcall r4300i_BEQ (void) {
	NextInstruction = DELAY_SLOT;
	if (GPR[Opcode.BRANCH.rs].DW == GPR[Opcode.BRANCH.rt].DW) {
		JumpToLocation = PROGRAM_COUNTER + ((short)Opcode.BRANCH.offset << 2) + 4;
		TestInterpreterJump(PROGRAM_COUNTER,JumpToLocation,Opcode.BRANCH.rs,Opcode.BRANCH.rt);
	} else {
		JumpToLocation = PROGRAM_COUNTER + 8;
	}
}

void _fastcall r4300i_BNE (void) {
	NextInstruction = DELAY_SLOT;
	if (GPR[Opcode.BRANCH.rs].DW != GPR[Opcode.BRANCH.rt].DW) {
		JumpToLocation = PROGRAM_COUNTER + ((short)Opcode.BRANCH.offset << 2) + 4;
		TestInterpreterJump(PROGRAM_COUNTER,JumpToLocation,Opcode.BRANCH.rs,Opcode.BRANCH.rt);
	} else {
		JumpToLocation = PROGRAM_COUNTER + 8;
	}
}

void _fastcall r4300i_BLEZ (void) {
	NextInstruction = DELAY_SLOT;
	if (GPR[Opcode.BRANCH.rs].DW <= 0) {
		JumpToLocation = PROGRAM_COUNTER + ((short)Opcode.BRANCH.offset << 2) + 4;
		TestInterpreterJump(PROGRAM_COUNTER,JumpToLocation,Opcode.BRANCH.rs,0);
	} else {
		JumpToLocation = PROGRAM_COUNTER + 8;
	}
}

void _fastcall r4300i_BGTZ (void) {
	NextInstruction = DELAY_SLOT;
	if (GPR[Opcode.BRANCH.rs].DW > 0) {
		JumpToLocation = PROGRAM_COUNTER + ((short)Opcode.BRANCH.offset << 2) + 4;
		TestInterpreterJump(PROGRAM_COUNTER,JumpToLocation,Opcode.BRANCH.rs,0);
	} else {
		JumpToLocation = PROGRAM_COUNTER + 8;
	}
}

void _fastcall r4300i_ADDI (void) {
#ifdef Interpreter_StackTest
	if (Opcode.BRANCH.rs == 29 && Opcode.BRANCH.rt == 29) {
		StackValue += (short)Opcode.BRANCH.offset;
	}
#endif
	if (Opcode.BRANCH.rt == 0) { return; }
	GPR[Opcode.BRANCH.rt].DW = (GPR[Opcode.BRANCH.rs].W[0] + ((short)Opcode.BRANCH.offset));
#ifdef Interpreter_StackTest
	if (Opcode.BRANCH.rt == 29 && Opcode.BRANCH.rs != 29) {
		StackValue = GPR[Opcode.BRANCH.rt].W[0];		
	}
#endif
}

void _fastcall r4300i_ADDIU (void) {
#ifdef Interpreter_StackTest
	if (Opcode.BRANCH.rs == 29 && Opcode.BRANCH.rt == 29) {
		StackValue += (short)Opcode.BRANCH.offset;
	}
#endif
	GPR[Opcode.BRANCH.rt].DW = (GPR[Opcode.BRANCH.rs].W[0] + ((short)Opcode.BRANCH.offset));
#ifdef Interpreter_StackTest
	if (Opcode.BRANCH.rt == 29 && Opcode.BRANCH.rs != 29) {
		StackValue = GPR[Opcode.BRANCH.rt].W[0];		
	}
#endif
}

void _fastcall r4300i_SLTI (void) {
	GPR[Opcode.BRANCH.rt].DW = GPR[Opcode.BRANCH.rs].DW < (__int64)((short)Opcode.BRANCH.offset)?1:0;
}

void _fastcall r4300i_SLTIU (void) {
	GPR[Opcode.BRANCH.rt].DW = GPR[Opcode.BRANCH.rs].UDW < (unsigned __int64)((short)Opcode.BRANCH.offset)?1:0;
}

void _fastcall r4300i_ANDI (void) {
	GPR[Opcode.BRANCH.rt].DW = GPR[Opcode.BRANCH.rs].DW & Opcode.BRANCH.offset;
}

void _fastcall r4300i_ORI (void) {
	GPR[Opcode.BRANCH.rt].DW = GPR[Opcode.BRANCH.rs].DW | Opcode.BRANCH.offset;
}

void _fastcall r4300i_XORI (void) {
	GPR[Opcode.BRANCH.rt].DW = GPR[Opcode.BRANCH.rs].DW ^ Opcode.BRANCH.offset;
}

void _fastcall r4300i_LUI (void) {
	if (Opcode.BRANCH.rt == 0) { return; }
	GPR[Opcode.BRANCH.rt].DW = (long)((short)Opcode.BRANCH.offset << 16);
#ifdef Interpreter_StackTest
	if (Opcode.BRANCH.rt == 29) {
		StackValue = GPR[Opcode.BRANCH.rt].W[0];
	}
#endif
}

void _fastcall r4300i_BEQL (void) {
	if (GPR[Opcode.BRANCH.rs].DW == GPR[Opcode.BRANCH.rt].DW) {
		NextInstruction = DELAY_SLOT;
		JumpToLocation = PROGRAM_COUNTER + ((short)Opcode.BRANCH.offset << 2) + 4;
		TestInterpreterJump(PROGRAM_COUNTER,JumpToLocation,Opcode.BRANCH.rs,Opcode.BRANCH.rt);
	} else {
		NextInstruction = JUMP;
		JumpToLocation = PROGRAM_COUNTER + 8;
	}
}

void _fastcall r4300i_BNEL (void) {
	if (GPR[Opcode.BRANCH.rs].DW != GPR[Opcode.BRANCH.rt].DW) {
		NextInstruction = DELAY_SLOT;
		JumpToLocation = PROGRAM_COUNTER + ((short)Opcode.BRANCH.offset << 2) + 4;
		TestInterpreterJump(PROGRAM_COUNTER,JumpToLocation,Opcode.BRANCH.rs,Opcode.BRANCH.rt);
	} else {
		NextInstruction = JUMP;
		JumpToLocation = PROGRAM_COUNTER + 8;
	}
}

void _fastcall r4300i_BLEZL (void) {
	if (GPR[Opcode.BRANCH.rs].DW <= 0) {
		NextInstruction = DELAY_SLOT;
		JumpToLocation = PROGRAM_COUNTER + ((short)Opcode.BRANCH.offset << 2) + 4;
		TestInterpreterJump(PROGRAM_COUNTER,JumpToLocation,Opcode.BRANCH.rs,0);
	} else {
		NextInstruction = JUMP;
		JumpToLocation = PROGRAM_COUNTER + 8;
	}
}

void _fastcall r4300i_BGTZL (void) {
	if (GPR[Opcode.BRANCH.rs].DW > 0) {
		NextInstruction = DELAY_SLOT;
		JumpToLocation = PROGRAM_COUNTER + ((short)Opcode.BRANCH.offset << 2) + 4;
		TestInterpreterJump(PROGRAM_COUNTER,JumpToLocation,Opcode.BRANCH.rs,0);
	} else {
		NextInstruction = JUMP;
		JumpToLocation = PROGRAM_COUNTER + 8;
	}
}

void _fastcall r4300i_DADDIU (void) {
	GPR[Opcode.BRANCH.rt].DW = GPR[Opcode.BRANCH.rs].DW + (_int64)((short)Opcode.BRANCH.offset);
}

QWORD LDL_MASK[8] = { 0,0xFF,0xFFFF,0xFFFFFF,0xFFFFFFFF,0xFFFFFFFFFF,
					 0xFFFFFFFFFFFF, 0xFFFFFFFFFFFFFF };
int LDL_SHIFT[8] = { 0, 8, 16, 24, 32, 40, 48, 56 };

void _fastcall r4300i_LDL (void) {
	DWORD Offset, Address;
	QWORD Value;
	
	Address = GPR[Opcode.IMM.base].UW[0] + (short)Opcode.IMM.immediate;
	Offset  = Address & 7;

	if (!r4300i_LD_VAddr((Address & ~7),&Value)) {
		if (ShowTLBMisses)
			DisplayError("LDL TLB: %X",Address);
		return;
	}
	GPR[Opcode.BRANCH.rt].DW = GPR[Opcode.BRANCH.rt].DW & LDL_MASK[Offset];
	GPR[Opcode.BRANCH.rt].DW += Value << LDL_SHIFT[Offset];
}

QWORD LDR_MASK[8] = { 0xFFFFFFFFFFFFFF00, 0xFFFFFFFFFFFF0000,
                      0xFFFFFFFFFF000000, 0xFFFFFFFF00000000,
                      0xFFFFFF0000000000, 0xFFFF000000000000,
                      0xFF00000000000000, 0 };
int LDR_SHIFT[8] = { 56, 48, 40, 32, 24, 16, 8, 0 };

void _fastcall r4300i_LDR (void) {
	DWORD Offset, Address;
	QWORD Value;
	
	Address = GPR[Opcode.IMM.base].UW[0] + (short)Opcode.IMM.immediate;
	Offset  = Address & 7;

	if (!r4300i_LD_VAddr((Address & ~7),&Value)) {
		if (ShowTLBMisses)
			DisplayError("LDL TLB: %X",Address);
		return;
	}

	GPR[Opcode.BRANCH.rt].DW = GPR[Opcode.BRANCH.rt].DW & LDR_MASK[Offset];
	GPR[Opcode.BRANCH.rt].DW += Value >> LDR_SHIFT[Offset];

}

void _fastcall r4300i_LB (void) {
	DWORD Address =  GPR[Opcode.IMM.base].UW[0] + (short)Opcode.IMM.immediate;	
	if (Opcode.BRANCH.rt == 0) { return; }
	if (!r4300i_LB_VAddr(Address,&GPR[Opcode.BRANCH.rt].UB[0])) {
		if (ShowTLBMisses) {
			if (ShowTLBMisses)
				DisplayError("LB TLB: %X",Address);
		}
		TLB_READ_EXCEPTION(Address);
	} else {
		GPR[Opcode.BRANCH.rt].DW = GPR[Opcode.BRANCH.rt].B[0];
	}
}

void _fastcall r4300i_LH (void) {
	DWORD Address =  GPR[Opcode.IMM.base].UW[0] + (short)Opcode.IMM.immediate;	
	if ((Address & 1) != 0) { ADDRESS_ERROR_EXCEPTION(Address,TRUE); }
	if (!r4300i_LH_VAddr(Address,&GPR[Opcode.BRANCH.rt].UHW[0])) {
		if (ShowTLBMisses) {
			DisplayError("LH TLB: %X",Address);
		}
		TLB_READ_EXCEPTION(Address);
	} else {
		GPR[Opcode.BRANCH.rt].DW = GPR[Opcode.BRANCH.rt].HW[0];
	}
}

DWORD LWL_MASK[4] = { 0,0xFF,0xFFFF,0xFFFFFF };
int LWL_SHIFT[4] = { 0, 8, 16, 24};

void _fastcall r4300i_LWL (void) {
	DWORD Offset, Address, Value;
	
	Address = GPR[Opcode.IMM.base].UW[0] + (short)Opcode.IMM.immediate;
	Offset  = Address & 3;

	if (!r4300i_LW_VAddr((Address & ~3),&Value)) {
		if (ShowTLBMisses)
			DisplayError("LDL TLB: %X",Address);
		return;
	}
	
	GPR[Opcode.BRANCH.rt].DW = (int)(GPR[Opcode.BRANCH.rt].W[0] & LWL_MASK[Offset]);
	GPR[Opcode.BRANCH.rt].DW += (int)(Value << LWL_SHIFT[Offset]);
}

void _fastcall r4300i_LW (void) {
	DWORD Address =  GPR[Opcode.IMM.base].UW[0] + (short)Opcode.IMM.immediate;	
	if ((Address & 3) != 0) { ADDRESS_ERROR_EXCEPTION(Address,TRUE); }

	if (ShowDebugMessages)
		Log_LW(PROGRAM_COUNTER,Address);

	if (Opcode.BRANCH.rt == 0) { return; }

	if (!r4300i_LW_VAddr(Address,&GPR[Opcode.BRANCH.rt].UW[0])) {
		if (ShowTLBMisses) {
			DisplayError("LW TLB: %X",Address);
		}
		TLB_READ_EXCEPTION(Address);
	} else {
		GPR[Opcode.BRANCH.rt].DW = GPR[Opcode.BRANCH.rt].W[0];
		//TranslateVaddr(&Address);
		//if (Address == 0x00090AA0) {
		//	LogMessage("%X: Read %X from %X",PROGRAM_COUNTER,GPR[Opcode.BRANCH.rt].UW[0],GPR[Opcode.IMM.base].UW[0] + (short)Opcode.IMM.immediate);
		//}
	}
}

void _fastcall r4300i_LBU (void) {
	DWORD Address =  GPR[Opcode.IMM.base].UW[0] + (short)Opcode.IMM.immediate;	
	if (!r4300i_LB_VAddr(Address,&GPR[Opcode.BRANCH.rt].UB[0])) {
		if (ShowTLBMisses) {
			DisplayError("LBU TLB: %X",Address);
		}
		TLB_READ_EXCEPTION(Address);
	} else {
		GPR[Opcode.BRANCH.rt].UDW = GPR[Opcode.BRANCH.rt].UB[0];
	}
}

void _fastcall r4300i_LHU (void) {
	DWORD Address =  GPR[Opcode.IMM.base].UW[0] + (short)Opcode.IMM.immediate;	
	if ((Address & 1) != 0) { ADDRESS_ERROR_EXCEPTION(Address,TRUE); }
	if (!r4300i_LH_VAddr(Address,&GPR[Opcode.BRANCH.rt].UHW[0])) {
		if (ShowTLBMisses) {
			DisplayError("LHU TLB: %X",Address);
		}
		TLB_READ_EXCEPTION(Address);
	} else {
		GPR[Opcode.BRANCH.rt].UDW = GPR[Opcode.BRANCH.rt].UHW[0];
	}
}

DWORD LWR_MASK[4] = { 0xFFFFFF00, 0xFFFF0000, 0xFF000000, 0 };
int LWR_SHIFT[4] = { 24, 16 ,8, 0 };

void _fastcall r4300i_LWR (void) {
	DWORD Offset, Address, Value;
	
	Address = GPR[Opcode.IMM.base].UW[0] + (short)Opcode.IMM.immediate;
	Offset  = Address & 3;

	if (!r4300i_LW_VAddr((Address & ~3),&Value)) {
		if (ShowTLBMisses)
			DisplayError("LDL TLB: %X",Address);
		return;
	}
	
	GPR[Opcode.BRANCH.rt].DW = (int)(GPR[Opcode.BRANCH.rt].W[0] & LWR_MASK[Offset]);
	GPR[Opcode.BRANCH.rt].DW += (int)(Value >> LWR_SHIFT[Offset]);
}

void _fastcall r4300i_LWU (void) {
	DWORD Address =  GPR[Opcode.IMM.base].UW[0] + (short)Opcode.IMM.immediate;	
	if ((Address & 3) != 0) { ADDRESS_ERROR_EXCEPTION(Address,TRUE); }
	if (Opcode.BRANCH.rt == 0) { return; }

	if (!r4300i_LW_VAddr(Address,&GPR[Opcode.BRANCH.rt].UW[0])) {
		if (ShowTLBMisses) {
			DisplayError("LWU TLB: %X",Address);
		}
		TLB_READ_EXCEPTION(Address);
	} else {
		GPR[Opcode.BRANCH.rt].UDW = GPR[Opcode.BRANCH.rt].UW[0];
	}
}

void _fastcall r4300i_SB (void) {
	DWORD Address =  GPR[Opcode.IMM.base].UW[0] + (short)Opcode.IMM.immediate;	
	if (!r4300i_SB_VAddr(Address,GPR[Opcode.BRANCH.rt].UB[0])) {
		if (ShowTLBMisses)
			DisplayError("SB TLB: %X",Address);
	}
}

void _fastcall r4300i_SH (void) {
	DWORD Address =  GPR[Opcode.IMM.base].UW[0] + (short)Opcode.IMM.immediate;	
	if ((Address & 1) != 0) { ADDRESS_ERROR_EXCEPTION(Address,FALSE); }
	if (!r4300i_SH_VAddr(Address,GPR[Opcode.BRANCH.rt].UHW[0])) {
		if (ShowTLBMisses)
			DisplayError("SH TLB: %X",Address);
	}
}

DWORD SWL_MASK[4] = { 0,0xFF000000,0xFFFF0000,0xFFFFFF00 };
int SWL_SHIFT[4] = { 0, 8, 16, 24 };

void _fastcall r4300i_SWL (void) {
	DWORD Offset, Address, Value;
	
	Address = GPR[Opcode.IMM.base].UW[0] + (short)Opcode.IMM.immediate;
	Offset  = Address & 3;

	if (!r4300i_LW_VAddr((Address & ~3),&Value)) {
		if (ShowTLBMisses)
			DisplayError("SWL TLB: %X",Address);
		return;
	}
	
	Value &= SWL_MASK[Offset];
	Value += GPR[Opcode.BRANCH.rt].UW[0] >> SWL_SHIFT[Offset];

	if (!r4300i_SW_VAddr((Address & ~0x03),Value)) {
		if (ShowTLBMisses)
			DisplayError("SWL TLB: %X",Address);
	}
}


void _fastcall r4300i_SW (void) {
	DWORD Address =  GPR[Opcode.IMM.base].UW[0] + (short)Opcode.IMM.immediate;	
	if ((Address & 3) != 0) { ADDRESS_ERROR_EXCEPTION(Address,FALSE); }

	if (ShowDebugMessages)
		Log_SW(PROGRAM_COUNTER,Address,GPR[Opcode.BRANCH.rt].UW[0]);

	if (!r4300i_SW_VAddr(Address,GPR[Opcode.BRANCH.rt].UW[0])) {
		if (ShowTLBMisses)
			DisplayError("SW TLB: %X",Address);
	}
	//TranslateVaddr(&Address);
	//if (Address == 0x00090AA0) {
	//	LogMessage("%X: Write %X to %X",PROGRAM_COUNTER,GPR[Opcode.BRANCH.rt].UW[0],GPR[Opcode.IMM.base].UW[0] + (short)Opcode.IMM.immediate);
	//}
}

QWORD SDL_MASK[8] = { 0,0xFF00000000000000,
						0xFFFF000000000000,
						0xFFFFFF0000000000,
						0xFFFFFFFF00000000,
					    0xFFFFFFFFFF000000,
						0xFFFFFFFFFFFF0000,
						0xFFFFFFFFFFFFFF00 
					};
int SDL_SHIFT[8] = { 0, 8, 16, 24, 32, 40, 48, 56 };

void _fastcall r4300i_SDL (void) {
	DWORD Offset, Address;
	QWORD Value;
	
	Address = GPR[Opcode.IMM.base].UW[0] + (short)Opcode.IMM.immediate;
	Offset  = Address & 7;

	if (!r4300i_LD_VAddr((Address & ~7),&Value)) {
		if (ShowTLBMisses)
			DisplayError("SDL TLB: %X",Address);
		return;
	}
	
	Value &= SDL_MASK[Offset];
	Value += GPR[Opcode.BRANCH.rt].UDW >> SDL_SHIFT[Offset];

	if (!r4300i_SD_VAddr((Address & ~7),Value)) {
		if (ShowTLBMisses)
			DisplayError("SDL TLB: %X",Address);
	}
}

QWORD SDR_MASK[8] = { 0x00FFFFFFFFFFFFFF,
					  0x0000FFFFFFFFFFFF,
					  0x000000FFFFFFFFFF,
					  0x00000000FFFFFFFF,
					  0x0000000000FFFFFF,
					  0x000000000000FFFF,
					  0x00000000000000FF, 
					  0x0000000000000000 
					};
int SDR_SHIFT[8] = { 56,48,40,32,24,16,8,0 };

void _fastcall r4300i_SDR (void) {
	DWORD Offset, Address;
	QWORD Value;
	
	Address = GPR[Opcode.IMM.base].UW[0] + (short)Opcode.IMM.immediate;
	Offset  = Address & 7;

	if (!r4300i_LD_VAddr((Address & ~7),&Value)) {
		if (ShowTLBMisses)
			DisplayError("SDL TLB: %X",Address);
		return;
	}
	
	Value &= SDR_MASK[Offset];
	Value += GPR[Opcode.BRANCH.rt].UDW << SDR_SHIFT[Offset];

	if (!r4300i_SD_VAddr((Address & ~7),Value)) {
		if (ShowTLBMisses)
			DisplayError("SDL TLB: %X",Address);
	}
}

DWORD SWR_MASK[4] = { 0x00FFFFFF,0x0000FFFF,0x000000FF,0x00000000 };
int SWR_SHIFT[4] = { 24, 16 , 8, 0  };

void _fastcall r4300i_SWR (void) {
	DWORD Offset, Address, Value;
	
	Address = GPR[Opcode.IMM.base].UW[0] + (short)Opcode.IMM.immediate;
	Offset  = Address & 3;

	if (!r4300i_LW_VAddr((Address & ~3),&Value)) {
		if (ShowTLBMisses)
			DisplayError("SWL TLB: %X",Address);
		return;
	}
	
	Value &= SWR_MASK[Offset];
	Value += GPR[Opcode.BRANCH.rt].UW[0] << SWR_SHIFT[Offset];

	if (!r4300i_SW_VAddr((Address & ~0x03),Value)) {
		if (ShowTLBMisses)
			DisplayError("SWL TLB: %X",Address);
	}
}

void _fastcall r4300i_CACHE (void) {
	if (HaveDebugger && !LogOptions.LogCache) { return; }
	LogMessage("%08X: Cache operation %d, 0x%08X", PROGRAM_COUNTER, Opcode.BRANCH.rt,
		GPR[Opcode.IMM.base].UW[0] + (short)Opcode.IMM.immediate );
}

void _fastcall r4300i_LL (void) {
	DWORD Address =  GPR[Opcode.IMM.base].UW[0] + (short)Opcode.IMM.immediate;	
	if ((Address & 3) != 0) { ADDRESS_ERROR_EXCEPTION(Address,TRUE); }

	if (Opcode.BRANCH.rt == 0) { return; }

	if (!r4300i_LW_VAddr(Address,&GPR[Opcode.BRANCH.rt].UW[0])) {
		if (ShowTLBMisses) {
			DisplayError("LW TLB: %X",Address);
		}
		TLB_READ_EXCEPTION(Address);
	} else {
		GPR[Opcode.BRANCH.rt].DW = GPR[Opcode.BRANCH.rt].W[0];
	}
	LLBit = 1;
	LLAddr = Address;
	TranslateVaddr(&LLAddr);
}

void _fastcall r4300i_LWC1 (void) {
	DWORD Address =  GPR[Opcode.IMM.base].UW[0] + (DWORD)((short)Opcode.IMM.immediate);
	TEST_COP1_USABLE_EXCEPTION
	if ((Address & 3) != 0) { ADDRESS_ERROR_EXCEPTION(Address,TRUE); }
	if (!r4300i_LW_VAddr(Address,&*(DWORD *)FPRFloatLocation[Opcode.FP.ft])) {
		if (ShowTLBMisses) {
			DisplayError("LWC1 TLB: %X",Address);
		}
		TLB_READ_EXCEPTION(Address);
	}
}

void _fastcall r4300i_SC (void) {
	DWORD Address =  GPR[Opcode.IMM.base].UW[0] + (short)Opcode.IMM.immediate;	
	if ((Address & 3) != 0) { ADDRESS_ERROR_EXCEPTION(Address,FALSE); }

	if (ShowDebugMessages)
		Log_SW(PROGRAM_COUNTER,Address,GPR[Opcode.BRANCH.rt].UW[0]);

	if (LLBit == 1) {
		if (!r4300i_SW_VAddr(Address,GPR[Opcode.BRANCH.rt].UW[0])) {
			DisplayError("SW TLB: %X",Address);
		}
	}
	GPR[Opcode.BRANCH.rt].UW[0] = LLBit;
}

void _fastcall r4300i_LD (void) {
	DWORD Address =  GPR[Opcode.IMM.base].UW[0] + (short)Opcode.IMM.immediate;	
	if ((Address & 7) != 0) { ADDRESS_ERROR_EXCEPTION(Address,TRUE); }
	if (!r4300i_LD_VAddr(Address,&GPR[Opcode.BRANCH.rt].UDW)) {
		if (ShowTLBMisses)
			DisplayError("LD TLB: %X",Address);
	}
#ifdef Interpreter_StackTest
	if (Opcode.BRANCH.rt == 29) {
		StackValue = GPR[Opcode.BRANCH.rt].W[0];
	}
#endif
}


void _fastcall r4300i_LDC1 (void) {
	DWORD Address =  GPR[Opcode.IMM.base].UW[0] + (short)Opcode.IMM.immediate;

	TEST_COP1_USABLE_EXCEPTION
	if ((Address & 7) != 0) { ADDRESS_ERROR_EXCEPTION(Address,TRUE); }
	if (!r4300i_LD_VAddr(Address,&*(unsigned __int64 *)FPRDoubleLocation[Opcode.FP.ft])) {
		if (ShowTLBMisses)
			DisplayError("LD TLB: %X",Address);
	}
}

void _fastcall r4300i_SWC1 (void) {
	DWORD Address =  GPR[Opcode.IMM.base].UW[0] + (short)Opcode.IMM.immediate;
	TEST_COP1_USABLE_EXCEPTION
	if ((Address & 3) != 0) { ADDRESS_ERROR_EXCEPTION(Address,FALSE); }

	if (!r4300i_SW_VAddr(Address,*(DWORD *)FPRFloatLocation[Opcode.FP.ft])) {
		if (ShowTLBMisses)
			DisplayError("SWC1 TLB: %X",Address);
	}
}

void _fastcall r4300i_SDC1 (void) {
	DWORD Address =  GPR[Opcode.IMM.base].UW[0] + (short)Opcode.IMM.immediate;

	TEST_COP1_USABLE_EXCEPTION
	if ((Address & 7) != 0) { ADDRESS_ERROR_EXCEPTION(Address,FALSE); }
	if (!r4300i_SD_VAddr(Address,*(__int64 *)FPRDoubleLocation[Opcode.FP.ft])) {
		if (ShowTLBMisses)
			DisplayError("SDC1 TLB: %X",Address);
	}
}

void _fastcall r4300i_SD (void) {
	DWORD Address =  GPR[Opcode.IMM.base].UW[0] + (short)Opcode.IMM.immediate;	
	if ((Address & 7) != 0) { ADDRESS_ERROR_EXCEPTION(Address,FALSE); }
	if (!r4300i_SD_VAddr(Address,GPR[Opcode.BRANCH.rt].UDW)) {
		if (ShowTLBMisses)
			DisplayError("SD TLB: %X",Address);
	}
}
/********************** R4300i OpCodes: Special **********************/
void _fastcall r4300i_SPECIAL_SLL (void) {
	GPR[Opcode.REG.rd].DW = (GPR[Opcode.BRANCH.rt].W[0] << Opcode.REG.sa);
}

void _fastcall r4300i_SPECIAL_SRL (void) {
	GPR[Opcode.REG.rd].DW = (int)(GPR[Opcode.BRANCH.rt].UW[0] >> Opcode.REG.sa);
}

void _fastcall r4300i_SPECIAL_SRA (void) {
	GPR[Opcode.REG.rd].DW = (GPR[Opcode.BRANCH.rt].W[0] >> Opcode.REG.sa);
}

void _fastcall r4300i_SPECIAL_SLLV (void) {
	if (Opcode.REG.rd == 0) { return; }
	GPR[Opcode.REG.rd].DW = (GPR[Opcode.BRANCH.rt].W[0] << (GPR[Opcode.BRANCH.rs].UW[0] & 0x1F));
}

void _fastcall r4300i_SPECIAL_SRLV (void) {
	GPR[Opcode.REG.rd].DW = (int)(GPR[Opcode.BRANCH.rt].UW[0] >> (GPR[Opcode.BRANCH.rs].UW[0] & 0x1F));
}

void _fastcall r4300i_SPECIAL_SRAV (void) {
	GPR[Opcode.REG.rd].DW = (GPR[Opcode.BRANCH.rt].W[0] >> (GPR[Opcode.BRANCH.rs].UW[0] & 0x1F));
}

void _fastcall r4300i_SPECIAL_JR (void) {
	NextInstruction = DELAY_SLOT;
	JumpToLocation = GPR[Opcode.BRANCH.rs].UW[0];
}

void _fastcall r4300i_SPECIAL_JALR (void) {
	NextInstruction = DELAY_SLOT;
	JumpToLocation = GPR[Opcode.BRANCH.rs].UW[0];
	GPR[Opcode.REG.rd].DW = (long)(PROGRAM_COUNTER + 8);
}

void _fastcall r4300i_SPECIAL_SYSCALL (void) {
	DoSysCallException(NextInstruction == JUMP);
	NextInstruction = JUMP;
	JumpToLocation = PROGRAM_COUNTER;
}

void _fastcall r4300i_SPECIAL_BREAK (void) {
	/*DoBreakException(NextInstruction == JUMP);
	NextInstruction = JUMP;
	JumpToLocation = PROGRAM_COUNTER;*/
}

void _fastcall r4300i_SPECIAL_SYNC (void) {
}

void _fastcall r4300i_SPECIAL_MFHI (void) {
	GPR[Opcode.REG.rd].DW = HI.DW;
}

void _fastcall r4300i_SPECIAL_MTHI (void) {
	HI.DW = GPR[Opcode.BRANCH.rs].DW;
}

void _fastcall r4300i_SPECIAL_MFLO (void) {
	GPR[Opcode.REG.rd].DW = LO.DW;
}

void _fastcall r4300i_SPECIAL_MTLO (void) {
	LO.DW = GPR[Opcode.BRANCH.rs].DW;
}

void _fastcall r4300i_SPECIAL_DSLLV (void) {
	GPR[Opcode.REG.rd].DW = GPR[Opcode.BRANCH.rt].DW << (GPR[Opcode.BRANCH.rs].UW[0] & 0x3F);
}

void _fastcall r4300i_SPECIAL_DSRLV (void) {
	GPR[Opcode.REG.rd].UDW = GPR[Opcode.BRANCH.rt].UDW >> (GPR[Opcode.BRANCH.rs].UW[0] & 0x3F);
}

void _fastcall r4300i_SPECIAL_DSRAV (void) {
	GPR[Opcode.REG.rd].DW = GPR[Opcode.BRANCH.rt].DW >> (GPR[Opcode.BRANCH.rs].UW[0] & 0x3F);
}

void _fastcall r4300i_SPECIAL_MULT (void) {
	HI.DW = (_int64)(GPR[Opcode.BRANCH.rs].W[0]) * (_int64)(GPR[Opcode.BRANCH.rt].W[0]);
	LO.DW = HI.W[0];
	HI.DW = HI.W[1];
}

void _fastcall r4300i_SPECIAL_MULTU (void) {
	HI.DW = (unsigned _int64)(GPR[Opcode.BRANCH.rs].UW[0]) * (unsigned _int64)(GPR[Opcode.BRANCH.rt].UW[0]);
	LO.DW = HI.W[0];
	HI.DW = HI.W[1];
}

void _fastcall r4300i_SPECIAL_DIV (void) {
	if ( GPR[Opcode.BRANCH.rt].UDW != 0 ) {
		LO.DW = GPR[Opcode.BRANCH.rs].W[0] / GPR[Opcode.BRANCH.rt].W[0];
		HI.DW = GPR[Opcode.BRANCH.rs].W[0] % GPR[Opcode.BRANCH.rt].W[0];
	} else {
		if (ShowDebugMessages)
			DisplayError("DIV by 0 ???");
	}
}

void _fastcall r4300i_SPECIAL_DIVU (void) {
	if ( GPR[Opcode.BRANCH.rt].UDW != 0 ) {
		LO.DW = (int)(GPR[Opcode.BRANCH.rs].UW[0] / GPR[Opcode.BRANCH.rt].UW[0]);
		HI.DW = (int)(GPR[Opcode.BRANCH.rs].UW[0] % GPR[Opcode.BRANCH.rt].UW[0]);
	} else {
		if (ShowDebugMessages)
			DisplayError("DIVU by 0 ???");
	}
}

void _fastcall r4300i_SPECIAL_DMULT (void) {
	MIPS_DWORD Tmp[3];
	
	LO.UDW = (QWORD)GPR[Opcode.BRANCH.rs].UW[0] * (QWORD)GPR[Opcode.BRANCH.rt].UW[0];
	Tmp[0].UDW = (_int64)GPR[Opcode.BRANCH.rs].W[1] * (_int64)(QWORD)GPR[Opcode.BRANCH.rt].UW[0];
	Tmp[1].UDW = (_int64)(QWORD)GPR[Opcode.BRANCH.rs].UW[0] * (_int64)GPR[Opcode.BRANCH.rt].W[1];
	HI.UDW = (_int64)GPR[Opcode.BRANCH.rs].W[1] * (_int64)GPR[Opcode.BRANCH.rt].W[1];
	
	Tmp[2].UDW = (QWORD)LO.UW[1] + (QWORD)Tmp[0].UW[0] + (QWORD)Tmp[1].UW[0];
	LO.UDW += ((QWORD)Tmp[0].UW[0] + (QWORD)Tmp[1].UW[0]) << 32;
	HI.UDW += (QWORD)Tmp[0].W[1] + (QWORD)Tmp[1].W[1] + Tmp[2].UW[1];
}

void _fastcall r4300i_SPECIAL_DMULTU (void) {
	MIPS_DWORD Tmp[3];
	
	LO.UDW = (QWORD)GPR[Opcode.BRANCH.rs].UW[0] * (QWORD)GPR[Opcode.BRANCH.rt].UW[0];
	Tmp[0].UDW = (QWORD)GPR[Opcode.BRANCH.rs].UW[1] * (QWORD)GPR[Opcode.BRANCH.rt].UW[0];
	Tmp[1].UDW = (QWORD)GPR[Opcode.BRANCH.rs].UW[0] * (QWORD)GPR[Opcode.BRANCH.rt].UW[1];
	HI.UDW = (QWORD)GPR[Opcode.BRANCH.rs].UW[1] * (QWORD)GPR[Opcode.BRANCH.rt].UW[1];
	
	Tmp[2].UDW = (QWORD)LO.UW[1] + (QWORD)Tmp[0].UW[0] + (QWORD)Tmp[1].UW[0];
	LO.UDW += ((QWORD)Tmp[0].UW[0] + (QWORD)Tmp[1].UW[0]) << 32;
	HI.UDW += (QWORD)Tmp[0].UW[1] + (QWORD)Tmp[1].UW[1] + Tmp[2].UW[1];
}

void _fastcall r4300i_SPECIAL_DDIV (void) {
	if ( GPR[Opcode.BRANCH.rt].UDW != 0 ) {
		LO.DW = GPR[Opcode.BRANCH.rs].DW / GPR[Opcode.BRANCH.rt].DW;
		HI.DW = GPR[Opcode.BRANCH.rs].DW % GPR[Opcode.BRANCH.rt].DW;
	} else {
		if (ShowDebugMessages)
			DisplayError("DDIV by 0 ???");
	}
}

void _fastcall r4300i_SPECIAL_DDIVU (void) {
	if ( GPR[Opcode.BRANCH.rt].UDW != 0 ) {
		LO.UDW = GPR[Opcode.BRANCH.rs].UDW / GPR[Opcode.BRANCH.rt].UDW;
		HI.UDW = GPR[Opcode.BRANCH.rs].UDW % GPR[Opcode.BRANCH.rt].UDW;
	} else {
		if (ShowDebugMessages)
			DisplayError("DDIVU by 0 ???");
	}
}

void _fastcall r4300i_SPECIAL_ADD (void) {
	GPR[Opcode.REG.rd].DW = GPR[Opcode.BRANCH.rs].W[0] + GPR[Opcode.BRANCH.rt].W[0];
}

void _fastcall r4300i_SPECIAL_ADDU (void) {
	GPR[Opcode.REG.rd].DW = GPR[Opcode.BRANCH.rs].W[0] + GPR[Opcode.BRANCH.rt].W[0];
}

void _fastcall r4300i_SPECIAL_SUB (void) {
	GPR[Opcode.REG.rd].DW = GPR[Opcode.BRANCH.rs].W[0] - GPR[Opcode.BRANCH.rt].W[0];
}

void _fastcall r4300i_SPECIAL_SUBU (void) {
	GPR[Opcode.REG.rd].DW = GPR[Opcode.BRANCH.rs].W[0] - GPR[Opcode.BRANCH.rt].W[0];
}

void _fastcall r4300i_SPECIAL_AND (void) {
	GPR[Opcode.REG.rd].DW = GPR[Opcode.BRANCH.rs].DW & GPR[Opcode.BRANCH.rt].DW;
}

void _fastcall r4300i_SPECIAL_OR (void) {
	GPR[Opcode.REG.rd].DW = GPR[Opcode.BRANCH.rs].DW | GPR[Opcode.BRANCH.rt].DW;
#ifdef Interpreter_StackTest
	if (Opcode.REG.rd == 29) {
		StackValue = GPR[Opcode.REG.rd].W[0];
	}
#endif
}

void _fastcall r4300i_SPECIAL_XOR (void) {
	GPR[Opcode.REG.rd].DW = GPR[Opcode.BRANCH.rs].DW ^ GPR[Opcode.BRANCH.rt].DW;
}

void _fastcall r4300i_SPECIAL_NOR (void) {
	GPR[Opcode.REG.rd].DW = ~(GPR[Opcode.BRANCH.rs].DW | GPR[Opcode.BRANCH.rt].DW);
}

void _fastcall r4300i_SPECIAL_SLT (void) {
	if (GPR[Opcode.BRANCH.rs].DW < GPR[Opcode.BRANCH.rt].DW) {
		GPR[Opcode.REG.rd].DW = 1;
	} else {
		GPR[Opcode.REG.rd].DW = 0;
	}
}

void _fastcall r4300i_SPECIAL_SLTU (void) {
	if (GPR[Opcode.BRANCH.rs].UDW < GPR[Opcode.BRANCH.rt].UDW) {
		GPR[Opcode.REG.rd].DW = 1;
	} else {
		GPR[Opcode.REG.rd].DW = 0;
	}
}

void _fastcall r4300i_SPECIAL_DADD (void) {
	GPR[Opcode.REG.rd].DW = GPR[Opcode.BRANCH.rs].DW + GPR[Opcode.BRANCH.rt].DW;
}

void _fastcall r4300i_SPECIAL_DADDU (void) {
	GPR[Opcode.REG.rd].DW = GPR[Opcode.BRANCH.rs].DW + GPR[Opcode.BRANCH.rt].DW;
}

void _fastcall r4300i_SPECIAL_DSUB (void) {
	GPR[Opcode.REG.rd].DW = GPR[Opcode.BRANCH.rs].DW - GPR[Opcode.BRANCH.rt].DW;
}

void _fastcall r4300i_SPECIAL_DSUBU (void) {
	GPR[Opcode.REG.rd].DW = GPR[Opcode.BRANCH.rs].DW - GPR[Opcode.BRANCH.rt].DW;
}

void _fastcall r4300i_SPECIAL_TEQ (void) {
	if (GPR[Opcode.BRANCH.rs].DW == GPR[Opcode.BRANCH.rt].DW) {
		if (ShowDebugMessages)
			DisplayError("Should trap this ???");
	}
}

void _fastcall r4300i_SPECIAL_DSLL (void) {
	GPR[Opcode.REG.rd].DW = (GPR[Opcode.BRANCH.rt].DW << Opcode.REG.sa);
}

void _fastcall r4300i_SPECIAL_DSRL (void) {
	GPR[Opcode.REG.rd].UDW = (GPR[Opcode.BRANCH.rt].UDW >> Opcode.REG.sa);
}

void _fastcall r4300i_SPECIAL_DSRA (void) {
	GPR[Opcode.REG.rd].DW = (GPR[Opcode.BRANCH.rt].DW >> Opcode.REG.sa);
}

void _fastcall r4300i_SPECIAL_DSLL32 (void) {
	GPR[Opcode.REG.rd].DW = (GPR[Opcode.BRANCH.rt].DW << (Opcode.REG.sa + 32));
}

void _fastcall r4300i_SPECIAL_DSRL32 (void) {
   GPR[Opcode.REG.rd].UDW = (GPR[Opcode.BRANCH.rt].UDW >> (Opcode.REG.sa + 32));
}

void _fastcall r4300i_SPECIAL_DSRA32 (void) {
	GPR[Opcode.REG.rd].DW = (GPR[Opcode.BRANCH.rt].DW >> (Opcode.REG.sa + 32));
}

/********************** R4300i OpCodes: RegImm **********************/
void _fastcall r4300i_REGIMM_BLTZ (void) {
	NextInstruction = DELAY_SLOT;
	if (GPR[Opcode.BRANCH.rs].DW < 0) {
		JumpToLocation = PROGRAM_COUNTER + ((short)Opcode.BRANCH.offset << 2) + 4;
		TestInterpreterJump(PROGRAM_COUNTER,JumpToLocation,Opcode.BRANCH.rs,0);
	} else {
		JumpToLocation = PROGRAM_COUNTER + 8;
	}
}

void _fastcall r4300i_REGIMM_BGEZ (void) {
	NextInstruction = DELAY_SLOT;
	if (GPR[Opcode.BRANCH.rs].DW >= 0) {
		JumpToLocation = PROGRAM_COUNTER + ((short)Opcode.BRANCH.offset << 2) + 4;
		TestInterpreterJump(PROGRAM_COUNTER,JumpToLocation,Opcode.BRANCH.rs,0);
	} else {
		JumpToLocation = PROGRAM_COUNTER + 8;
	}
}

void _fastcall r4300i_REGIMM_BLTZL (void) {
	if (GPR[Opcode.BRANCH.rs].DW < 0) {
		NextInstruction = DELAY_SLOT;
		JumpToLocation = PROGRAM_COUNTER + ((short)Opcode.BRANCH.offset << 2) + 4;
		TestInterpreterJump(PROGRAM_COUNTER,JumpToLocation,Opcode.BRANCH.rs,0);
	} else {
		NextInstruction = JUMP;
		JumpToLocation = PROGRAM_COUNTER + 8;
	}
}

void _fastcall r4300i_REGIMM_BGEZL (void) {
	if (GPR[Opcode.BRANCH.rs].DW >= 0) {
		NextInstruction = DELAY_SLOT;
		JumpToLocation = PROGRAM_COUNTER + ((short)Opcode.BRANCH.offset << 2) + 4;
		TestInterpreterJump(PROGRAM_COUNTER,JumpToLocation,Opcode.BRANCH.rs,0);
	} else {
		NextInstruction = JUMP;
		JumpToLocation = PROGRAM_COUNTER + 8;
	}
}

void _fastcall r4300i_REGIMM_BLTZAL (void) {
	NextInstruction = DELAY_SLOT;
	if (GPR[Opcode.BRANCH.rs].DW < 0) {
		JumpToLocation = PROGRAM_COUNTER + ((short)Opcode.BRANCH.offset << 2) + 4;
		TestInterpreterJump(PROGRAM_COUNTER,JumpToLocation,Opcode.BRANCH.rs,0);
	} else {
		JumpToLocation = PROGRAM_COUNTER + 8;
	}
	GPR[31].DW= (long)(PROGRAM_COUNTER + 8);
}

void _fastcall r4300i_REGIMM_BGEZAL (void) {
	NextInstruction = DELAY_SLOT;
	if (GPR[Opcode.BRANCH.rs].DW >= 0) {
		JumpToLocation = PROGRAM_COUNTER + ((short)Opcode.BRANCH.offset << 2) + 4;
		TestInterpreterJump(PROGRAM_COUNTER,JumpToLocation,Opcode.BRANCH.rs,0);
	} else {
		JumpToLocation = PROGRAM_COUNTER + 8;
	}
	GPR[31].DW = (long)(PROGRAM_COUNTER + 8);
}
/************************** COP0 functions **************************/
void _fastcall r4300i_COP0_MF (void) {
	if (HaveDebugger && LogOptions.LogCP0reads) {
		LogMessage("%08X: R4300i Read from %s (0x%08X)", PROGRAM_COUNTER,
			Cop0_Name[Opcode.REG.rd], CP0[Opcode.REG.rd]);
	}
	GPR[Opcode.BRANCH.rt].DW = (int)CP0[Opcode.REG.rd];
}

void _fastcall r4300i_COP0_MT (void) {
	if (HaveDebugger && LogOptions.LogCP0changes) {
		LogMessage("%08X: Writing 0x%X to %s register (Originally: 0x%08X)",PROGRAM_COUNTER,
			GPR[Opcode.BRANCH.rt].UW[0],Cop0_Name[Opcode.REG.rd], CP0[Opcode.REG.rd]);
		if (Opcode.REG.rd == 11) { //Compare
			LogMessage("%08X: Cause register changed from %08X to %08X",PROGRAM_COUNTER,
				CAUSE_REGISTER, (CAUSE_REGISTER & ~CAUSE_IP7));
		}
	}
	switch (Opcode.REG.rd) {	
	case 0: //Index
	case 2: //EntryLo0
	case 3: //EntryLo1
	case 5: //PageMask
	case 6: //Wired
	case 10: //Entry Hi
	case 14: //EPC
	case 16: //Config
	case 18: //WatchLo
	case 19: //WatchHi
	case 28: //Tag lo
	case 29: //Tag Hi
	case 30: //ErrEPC
		CP0[Opcode.REG.rd] = GPR[Opcode.BRANCH.rt].UW[0];
		break;
	case 4: //Context
		CP0[Opcode.REG.rd] = GPR[Opcode.BRANCH.rt].UW[0] & 0xFF800000;
		break;
	case 9: //Count
		CP0[Opcode.REG.rd]= GPR[Opcode.BRANCH.rt].UW[0];
		ChangeCompareTimer();
		break;		
	case 11: //Compare
		CP0[Opcode.REG.rd] = GPR[Opcode.BRANCH.rt].UW[0];
		FAKE_CAUSE_REGISTER &= ~CAUSE_IP7;
		ChangeCompareTimer();
		break;		
	case 12: //Status
		if ((CP0[Opcode.REG.rd] ^ GPR[Opcode.BRANCH.rt].UW[0]) != 0) {
			CP0[Opcode.REG.rd] = GPR[Opcode.BRANCH.rt].UW[0];
			SetFpuLocations();
		} else {
			CP0[Opcode.REG.rd] = GPR[Opcode.BRANCH.rt].UW[0];
		}
		if ((CP0[Opcode.REG.rd] & 0x18) != 0) { 
			if (ShowDebugMessages)
				DisplayError("Left kernel mode ??");
		}
		CheckInterrupts();
		break;		
	case 13: //cause
		CP0[Opcode.REG.rd] &= 0xFFFFCFF;
		if (ShowDebugMessages)
			if ((GPR[Opcode.BRANCH.rt].UW[0] & 0x300) != 0 ){ DisplayError("Set IP0 or IP1"); }
		break;
	default:
		R4300i_UnknownOpcode();
	}

}

/************************** COP0 CO functions ***********************/
void _fastcall r4300i_COP0_CO_TLBR (void) {
	if (!UseTlb) { return; }
	TLB_Read();
}

void _fastcall r4300i_COP0_CO_TLBWI (void) {
	if (!UseTlb) { return; }
/*	if (PROGRAM_COUNTER == 0x00136260 && INDEX_REGISTER == 0x1F) {
		DisplayError("TLBWI");
	} else {
		WriteTLBEntry(INDEX_REGISTER & 0x1F);
	}*/
	WriteTLBEntry(INDEX_REGISTER & 0x1F);
}

void _fastcall r4300i_COP0_CO_TLBWR (void) {
	if (!UseTlb) { return; }
	WriteTLBEntry(RANDOM_REGISTER & 0x1F);
}

void _fastcall r4300i_COP0_CO_TLBP (void) {
	if (!UseTlb) { return; }
	TLB_Probe();
}

void _fastcall r4300i_COP0_CO_ERET (void) {
	NextInstruction = JUMP;
	if ((STATUS_REGISTER & STATUS_ERL) != 0) {
		JumpToLocation = ERROREPC_REGISTER;
		STATUS_REGISTER &= ~STATUS_ERL;
	} else {
		JumpToLocation = EPC_REGISTER;
		STATUS_REGISTER &= ~STATUS_EXL;
	}
	LLBit = 0;
	CheckInterrupts();
}

/************************** COP1 functions **************************/
void _fastcall r4300i_COP1_MF (void) {
	TEST_COP1_USABLE_EXCEPTION	
	GPR[Opcode.BRANCH.rt].DW = *(int *)FPRFloatLocation[Opcode.FP.fs];
}

void _fastcall r4300i_COP1_DMF (void) {
	TEST_COP1_USABLE_EXCEPTION
	GPR[Opcode.BRANCH.rt].DW = *(__int64 *)FPRDoubleLocation[Opcode.FP.fs];
}

void _fastcall r4300i_COP1_CF (void) {
	TEST_COP1_USABLE_EXCEPTION
	if (Opcode.FP.fs != 31 && Opcode.FP.fs != 0) {
		if (ShowDebugMessages)
			DisplayError("CFC1 what register are you writing to ?");
		return;
	}
	GPR[Opcode.BRANCH.rt].DW = (int)FPCR[Opcode.FP.fs];
}

void _fastcall r4300i_COP1_MT (void) {
	TEST_COP1_USABLE_EXCEPTION
	*(int *)FPRFloatLocation[Opcode.FP.fs] = GPR[Opcode.BRANCH.rt].W[0];
}

void _fastcall r4300i_COP1_DMT (void) {
	TEST_COP1_USABLE_EXCEPTION
	*(__int64 *)FPRDoubleLocation[Opcode.FP.fs] = GPR[Opcode.BRANCH.rt].DW;
}

void _fastcall r4300i_COP1_CT (void) {
	TEST_COP1_USABLE_EXCEPTION
	if (Opcode.FP.fs == 31) {
		FPCR[Opcode.FP.fs] = GPR[Opcode.BRANCH.rt].W[0];
		switch((FPCR[Opcode.FP.fs] & 3)) {
		case 0: RoundingModel = _RC_NEAR; break;
		case 1: RoundingModel = _RC_CHOP; break;
		case 2: RoundingModel = _RC_UP; break;
		case 3: RoundingModel = _RC_DOWN; break;
		}
		return;
	}
	if (ShowDebugMessages)
		DisplayError("CTC1 what register are you writing to ?");
}

/************************* COP1: BC1 functions ***********************/
void _fastcall r4300i_COP1_BCF (void) {
	TEST_COP1_USABLE_EXCEPTION
	NextInstruction = DELAY_SLOT;
	if ((FPCR[31] & FPCSR_C) == 0) {
		JumpToLocation = PROGRAM_COUNTER + ((short)Opcode.BRANCH.offset << 2) + 4;
	} else {
		JumpToLocation = PROGRAM_COUNTER + 8;
	}
}

void _fastcall r4300i_COP1_BCT (void) {
	TEST_COP1_USABLE_EXCEPTION
	NextInstruction = DELAY_SLOT;
	if ((FPCR[31] & FPCSR_C) != 0) {
		JumpToLocation = PROGRAM_COUNTER + ((short)Opcode.BRANCH.offset << 2) + 4;
	} else {
		JumpToLocation = PROGRAM_COUNTER + 8;
	}
}

void _fastcall r4300i_COP1_BCFL (void) {
	TEST_COP1_USABLE_EXCEPTION
	if ((FPCR[31] & FPCSR_C) == 0) {
		NextInstruction = DELAY_SLOT;
		JumpToLocation = PROGRAM_COUNTER + ((short)Opcode.BRANCH.offset << 2) + 4;
	} else {
		NextInstruction = JUMP;
		JumpToLocation = PROGRAM_COUNTER + 8;
	}
}

void _fastcall r4300i_COP1_BCTL (void) {
	TEST_COP1_USABLE_EXCEPTION
	if ((FPCR[31] & FPCSR_C) != 0) {
		NextInstruction = DELAY_SLOT;
		JumpToLocation = PROGRAM_COUNTER + ((short)Opcode.BRANCH.offset << 2) + 4;
	} else {
		NextInstruction = JUMP;
		JumpToLocation = PROGRAM_COUNTER + 8;
	}
}
/************************** COP1: S functions ************************/
__inline void Float_RoundToInteger32( int * Dest, float * Source ) {
	_asm {
		mov esi, [Source]
		mov edi, [Dest]
		fld dword ptr [esi]
		fistp dword ptr [edi]
	}
}

__inline void Float_RoundToInteger64( __int64 * Dest, float * Source ) {
	_asm {
		mov esi, [Source]
		mov edi, [Dest]
		fld dword ptr [esi]
		fistp qword ptr [edi]
	}
}

void _fastcall r4300i_COP1_S_ADD (void) {
	TEST_COP1_USABLE_EXCEPTION
	_controlfp(RoundingModel,_MCW_RC);
	*(float *)FPRFloatLocation[Opcode.FP.fd] = (*(float *)FPRFloatLocation[Opcode.FP.fs] + *(float *)FPRFloatLocation[Opcode.FP.ft]); 
}

void _fastcall r4300i_COP1_S_SUB (void) {
	TEST_COP1_USABLE_EXCEPTION
	_controlfp(RoundingModel,_MCW_RC);
	*(float *)FPRFloatLocation[Opcode.FP.fd] = (*(float *)FPRFloatLocation[Opcode.FP.fs] - *(float *)FPRFloatLocation[Opcode.FP.ft]); 
}

void _fastcall r4300i_COP1_S_MUL (void) {
	TEST_COP1_USABLE_EXCEPTION
	_controlfp(RoundingModel,_MCW_RC);
	*(float *)FPRFloatLocation[Opcode.FP.fd] = (*(float *)FPRFloatLocation[Opcode.FP.fs] * *(float *)FPRFloatLocation[Opcode.FP.ft]); 
}

void _fastcall r4300i_COP1_S_DIV (void) {
	TEST_COP1_USABLE_EXCEPTION
	_controlfp(RoundingModel,_MCW_RC);
	*(float *)FPRFloatLocation[Opcode.FP.fd] = (*(float *)FPRFloatLocation[Opcode.FP.fs] / *(float *)FPRFloatLocation[Opcode.FP.ft]); 
}

void _fastcall r4300i_COP1_S_SQRT (void) {
	TEST_COP1_USABLE_EXCEPTION
	_controlfp(RoundingModel,_MCW_RC);
	*(float *)FPRFloatLocation[Opcode.FP.fd] = (float)sqrt(*(float *)FPRFloatLocation[Opcode.FP.fs]);
}

void _fastcall r4300i_COP1_S_ABS (void) {
	TEST_COP1_USABLE_EXCEPTION
	_controlfp(RoundingModel,_MCW_RC);
	*(float *)FPRFloatLocation[Opcode.FP.fd] = (float)fabs(*(float *)FPRFloatLocation[Opcode.FP.fs]);
}

void _fastcall r4300i_COP1_S_MOV (void) {
	TEST_COP1_USABLE_EXCEPTION
	_controlfp(RoundingModel,_MCW_RC);
	*(float *)FPRFloatLocation[Opcode.FP.fd] = *(float *)FPRFloatLocation[Opcode.FP.fs];
}

void _fastcall r4300i_COP1_S_NEG (void) {
	TEST_COP1_USABLE_EXCEPTION
	_controlfp(RoundingModel,_MCW_RC);
	*(float *)FPRFloatLocation[Opcode.FP.fd] = (*(float *)FPRFloatLocation[Opcode.FP.fs] * -1.0f);
}

void _fastcall r4300i_COP1_S_TRUNC_L (void) {
	TEST_COP1_USABLE_EXCEPTION
	_controlfp(_RC_CHOP,_MCW_RC);
	Float_RoundToInteger64(&*(__int64 *)FPRDoubleLocation[Opcode.FP.fd],&*(float *)FPRFloatLocation[Opcode.FP.fs]);
}

void _fastcall r4300i_COP1_S_CEIL_L (void) {	//added by Witten
	TEST_COP1_USABLE_EXCEPTION
	_controlfp(_RC_UP,_MCW_RC);
	Float_RoundToInteger64(&*(__int64 *)FPRDoubleLocation[Opcode.FP.fd],&*(float *)FPRFloatLocation[Opcode.FP.fs]);
}

void _fastcall r4300i_COP1_S_FLOOR_L (void) {	//added by Witten
	TEST_COP1_USABLE_EXCEPTION
	_controlfp(_RC_DOWN,_MCW_RC);
	Float_RoundToInteger64(&*(__int64 *)FPRDoubleLocation[Opcode.FP.fd],&*(float *)FPRFloatLocation[Opcode.FP.fs]);
}

void _fastcall r4300i_COP1_S_ROUND_W (void) {
	TEST_COP1_USABLE_EXCEPTION
	_controlfp(_RC_NEAR,_MCW_RC);
	Float_RoundToInteger32(&*(int *)FPRFloatLocation[Opcode.FP.fd],&*(float *)FPRFloatLocation[Opcode.FP.fs]);
}

void _fastcall r4300i_COP1_S_TRUNC_W (void) {
	TEST_COP1_USABLE_EXCEPTION
	_controlfp(_RC_CHOP,_MCW_RC);
	Float_RoundToInteger32(&*(int *)FPRFloatLocation[Opcode.FP.fd],&*(float *)FPRFloatLocation[Opcode.FP.fs]);
}

void _fastcall r4300i_COP1_S_CEIL_W (void) {	//added by Witten
	TEST_COP1_USABLE_EXCEPTION
	_controlfp(_RC_UP,_MCW_RC);
	Float_RoundToInteger32(&*(int *)FPRFloatLocation[Opcode.FP.fd],&*(float *)FPRFloatLocation[Opcode.FP.fs]);
}

void _fastcall r4300i_COP1_S_FLOOR_W (void) {
	TEST_COP1_USABLE_EXCEPTION
	_controlfp(_RC_DOWN,_MCW_RC);
	Float_RoundToInteger32(&*(int *)FPRFloatLocation[Opcode.FP.fd],&*(float *)FPRFloatLocation[Opcode.FP.fs]);
}

void _fastcall r4300i_COP1_S_CVT_D (void) {
	TEST_COP1_USABLE_EXCEPTION
	_controlfp(RoundingModel,_MCW_RC);
	*(double *)FPRDoubleLocation[Opcode.FP.fd] = (double)(*(float *)FPRFloatLocation[Opcode.FP.fs]);
}

void _fastcall r4300i_COP1_S_CVT_W (void) {
	TEST_COP1_USABLE_EXCEPTION
	_controlfp(RoundingModel,_MCW_RC);
	Float_RoundToInteger32(&*(int *)FPRFloatLocation[Opcode.FP.fd],&*(float *)FPRFloatLocation[Opcode.FP.fs]);
}

void _fastcall r4300i_COP1_S_CVT_L (void) {
	TEST_COP1_USABLE_EXCEPTION
	_controlfp(RoundingModel,_MCW_RC);
	Float_RoundToInteger64(&*(__int64 *)FPRDoubleLocation[Opcode.FP.fd],&*(float *)FPRFloatLocation[Opcode.FP.fs]);
}

void _fastcall r4300i_COP1_S_CMP (void) {
	int less, equal, unorded, condition;
	float Temp0, Temp1;

	TEST_COP1_USABLE_EXCEPTION

	Temp0 = *(float *)FPRFloatLocation[Opcode.FP.fs];
	Temp1 = *(float *)FPRFloatLocation[Opcode.FP.ft];

	if (_isnan(Temp0) || _isnan(Temp1)) {
		if (ShowDebugMessages)
			DisplayError("Not a number?");
		less = FALSE;
		equal = FALSE;
		unorded = TRUE;
		if ((Opcode.REG.funct & 8) != 0) {
			if (ShowDebugMessages)
				DisplayError("Signal InvalidOperationException\nin r4300i_COP1_S_CMP\n%X  %ff\n%X  %ff", Temp0, Temp0, Temp1, Temp1);
		}
	} else {
		less = Temp0 < Temp1;
		equal = Temp0 == Temp1;
		unorded = FALSE;
	}
	
	condition = ((Opcode.REG.funct & 4) && less) | ((Opcode.REG.funct & 2) && equal) | ((Opcode.REG.funct & 1) && unorded);

	if (condition) {
		FPCR[31] |= FPCSR_C;
	} else {
		FPCR[31] &= ~FPCSR_C;
	}
	
}

/************************** COP1: D functions ************************/
__inline void Double_RoundToInteger32( int * Dest, double * Source ) {
	_asm {
		mov esi, [Source]
		mov edi, [Dest]
		fld qword ptr [esi]
		fistp dword ptr [edi]
	}
}

__inline void Double_RoundToInteger64( __int64 * Dest, double * Source ) {
	_asm {
		mov esi, [Source]
		mov edi, [Dest]
		fld qword ptr [esi]
		fistp qword ptr [edi]
	}
}

void _fastcall r4300i_COP1_D_ADD (void) {
	TEST_COP1_USABLE_EXCEPTION
	*(double *)FPRDoubleLocation[Opcode.FP.fd] = *(double *)FPRDoubleLocation[Opcode.FP.fs] + *(double *)FPRDoubleLocation[Opcode.FP.ft]; 
}

void _fastcall r4300i_COP1_D_SUB (void) {
	TEST_COP1_USABLE_EXCEPTION
	*(double *)FPRDoubleLocation[Opcode.FP.fd] = *(double *)FPRDoubleLocation[Opcode.FP.fs] - *(double *)FPRDoubleLocation[Opcode.FP.ft]; 
}

void _fastcall r4300i_COP1_D_MUL (void) {
	TEST_COP1_USABLE_EXCEPTION
	*(double *)FPRDoubleLocation[Opcode.FP.fd] = *(double *)FPRDoubleLocation[Opcode.FP.fs] * *(double *)FPRDoubleLocation[Opcode.FP.ft]; 
}

void _fastcall r4300i_COP1_D_DIV (void) {
	TEST_COP1_USABLE_EXCEPTION
	*(double *)FPRDoubleLocation[Opcode.FP.fd] = *(double *)FPRDoubleLocation[Opcode.FP.fs] / *(double *)FPRDoubleLocation[Opcode.FP.ft]; 
}

void _fastcall r4300i_COP1_D_SQRT (void) {
	TEST_COP1_USABLE_EXCEPTION
	*(double *)FPRDoubleLocation[Opcode.FP.fd] = (double)sqrt(*(double *)FPRDoubleLocation[Opcode.FP.fs]); 
}

void _fastcall r4300i_COP1_D_ABS (void) {
	TEST_COP1_USABLE_EXCEPTION
	*(double *)FPRDoubleLocation[Opcode.FP.fd] = fabs(*(double *)FPRDoubleLocation[Opcode.FP.fs]);
}

void _fastcall r4300i_COP1_D_MOV (void) {
	TEST_COP1_USABLE_EXCEPTION
	*(__int64 *)FPRDoubleLocation[Opcode.FP.fd] = *(__int64 *)FPRDoubleLocation[Opcode.FP.fs];
}

void _fastcall r4300i_COP1_D_NEG (void) {
	TEST_COP1_USABLE_EXCEPTION
	*(double *)FPRDoubleLocation[Opcode.FP.fd] = (*(double *)FPRDoubleLocation[Opcode.FP.fs] * -1.0);
}

void _fastcall r4300i_COP1_D_TRUNC_L (void) {	//added by Witten
	TEST_COP1_USABLE_EXCEPTION
	_controlfp(RC_CHOP,_MCW_RC);
	Double_RoundToInteger64(&*(__int64 *)FPRFloatLocation[Opcode.FP.fd],&*(double *)FPRDoubleLocation[Opcode.FP.fs] );
}

void _fastcall r4300i_COP1_D_CEIL_L (void) {	//added by Witten
	TEST_COP1_USABLE_EXCEPTION
	_controlfp(RC_UP,_MCW_RC);
	Double_RoundToInteger64(&*(__int64 *)FPRFloatLocation[Opcode.FP.fd],&*(double *)FPRDoubleLocation[Opcode.FP.fs] );
}

void _fastcall r4300i_COP1_D_FLOOR_L (void) {	//added by Witten
	TEST_COP1_USABLE_EXCEPTION
	_controlfp(_RC_DOWN,_MCW_RC);
	Double_RoundToInteger64(&*(__int64 *)FPRDoubleLocation[Opcode.FP.fd],&*(double *)FPRFloatLocation[Opcode.FP.fs]);
}

void _fastcall r4300i_COP1_D_ROUND_W (void) {
	TEST_COP1_USABLE_EXCEPTION
	_controlfp(_RC_NEAR,_MCW_RC);
	Double_RoundToInteger32(&*(DWORD *)FPRFloatLocation[Opcode.FP.fd],&*(double *)FPRDoubleLocation[Opcode.FP.fs] );
}

void _fastcall r4300i_COP1_D_TRUNC_W (void) {
	TEST_COP1_USABLE_EXCEPTION
	_controlfp(RC_CHOP,_MCW_RC);
	Double_RoundToInteger32(&*(DWORD *)FPRFloatLocation[Opcode.FP.fd],&*(double *)FPRDoubleLocation[Opcode.FP.fs] );
}

void _fastcall r4300i_COP1_D_CEIL_W (void) {	//added by Witten
	TEST_COP1_USABLE_EXCEPTION
	_controlfp(RC_UP,_MCW_RC);
	Double_RoundToInteger32(&*(DWORD *)FPRFloatLocation[Opcode.FP.fd],&*(double *)FPRDoubleLocation[Opcode.FP.fs] );
}

void _fastcall r4300i_COP1_D_FLOOR_W (void) {	//added by Witten
	TEST_COP1_USABLE_EXCEPTION
	_controlfp(_RC_DOWN,_MCW_RC);
	Double_RoundToInteger32(&*(DWORD *)FPRDoubleLocation[Opcode.FP.fd],&*(double *)FPRFloatLocation[Opcode.FP.fs]);
}

void _fastcall r4300i_COP1_D_CVT_S (void) {
	TEST_COP1_USABLE_EXCEPTION
	_controlfp(RoundingModel,_MCW_RC);
	*(float *)FPRFloatLocation[Opcode.FP.fd] = (float)*(double *)FPRDoubleLocation[Opcode.FP.fs];
}

void _fastcall r4300i_COP1_D_CVT_W (void) {
	TEST_COP1_USABLE_EXCEPTION
	_controlfp(RoundingModel,_MCW_RC);
	Double_RoundToInteger32(&*(DWORD *)FPRFloatLocation[Opcode.FP.fd],&*(double *)FPRDoubleLocation[Opcode.FP.fs] );
}

void _fastcall r4300i_COP1_D_CVT_L (void) {
	TEST_COP1_USABLE_EXCEPTION
	_controlfp(RoundingModel,_MCW_RC);
	Double_RoundToInteger64(&*(unsigned __int64 *)FPRDoubleLocation[Opcode.FP.fd],&*(double *)FPRDoubleLocation[Opcode.FP.fs]);
}

void _fastcall r4300i_COP1_D_CMP (void) {
	int less, equal, unorded, condition;
	MIPS_DWORD Temp0, Temp1;

	TEST_COP1_USABLE_EXCEPTION

	Temp0.DW = *(__int64 *)FPRDoubleLocation[Opcode.FP.fs];
	Temp1.DW = *(__int64 *)FPRDoubleLocation[Opcode.FP.ft];

	if (_isnan(Temp0.D) || _isnan(Temp1.D)) {
		if (ShowDebugMessages)
			DisplayError("Not A Number?");
		less = FALSE;
		equal = FALSE;
		unorded = TRUE;
		if ((Opcode.REG.funct & 8) != 0) {
			if (ShowDebugMessages)
				DisplayError("Signal InvalidOperationException\nin r4300i_COP1_D_CMP");
		}
	} else {
		less = Temp0.D < Temp1.D;
		equal = Temp0.D == Temp1.D;
		unorded = FALSE;
	}
	
	condition = ((Opcode.REG.funct & 4) && less) | ((Opcode.REG.funct & 2) && equal) | ((Opcode.REG.funct & 1) && unorded);

	if (condition) {
		FPCR[31] |= FPCSR_C;
	} else {
		FPCR[31] &= ~FPCSR_C;
	}	
}

/************************** COP1: W functions ************************/
void _fastcall r4300i_COP1_W_CVT_S (void) {
	TEST_COP1_USABLE_EXCEPTION
	_controlfp(RoundingModel,_MCW_RC);
	*(float *)FPRFloatLocation[Opcode.FP.fd] = (float)*(int *)FPRFloatLocation[Opcode.FP.fs];
}

void _fastcall r4300i_COP1_W_CVT_D (void) {
	TEST_COP1_USABLE_EXCEPTION
	_controlfp(RoundingModel,_MCW_RC);
	*(double *)FPRDoubleLocation[Opcode.FP.fd] = (double)*(int *)FPRFloatLocation[Opcode.FP.fs];
}

/************************** COP1: L functions ************************/
void _fastcall r4300i_COP1_L_CVT_S (void) {
	TEST_COP1_USABLE_EXCEPTION
	_controlfp(RoundingModel,_MCW_RC);
	*(float *)FPRFloatLocation[Opcode.FP.fd] = (float)*(__int64 *)FPRDoubleLocation[Opcode.FP.fs];
}

void _fastcall r4300i_COP1_L_CVT_D (void) {
	TEST_COP1_USABLE_EXCEPTION
	_controlfp(RoundingModel,_MCW_RC);
	*(double *)FPRDoubleLocation[Opcode.FP.fd] = (double)*(__int64 *)FPRDoubleLocation[Opcode.FP.fs];
}

/************************** Other functions **************************/
void _fastcall R4300i_UnknownOpcode (void) {
	char Message[200];

	sprintf(Message,"%s: %08X\n%s\n\n", GS(MSG_UNHANDLED_OP), PROGRAM_COUNTER,
		R4300iOpcodeName(Opcode.Hex,PROGRAM_COUNTER));
	strcat(Message,"Stoping Emulation !");
	
	if (HaveDebugger && !inFullScreen) {
		int response;

		strcat(Message,"\n\nDo you wish to enter the debugger?");
	
		response = MessageBox(NULL,Message,GS(MSG_MSGBOX_TITLE), MB_YESNO | MB_ICONERROR );
		if (response == IDYES) {
			Enter_R4300i_Commands_Window ();
		}
		ExitThread(0);
	} 
	else {
		DisplayError(Message);
		ExitThread(0);
	}
}