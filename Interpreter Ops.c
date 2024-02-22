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
#include <math.h>
#include <float.h>
#include "main.h"
#include "cpu.h"
#include "debugger.h"

#define STOP_ON_UNKNOWN_OPCODE

int RoundingModel = _RC_NEAR;

#define CAUSE_INEXACT       0x01
#define CAUSE_UNDERFLOW     0x02
#define CAUSE_OVERFLOW      0x04
#define CAUSE_DIVBYZERO     0x08
#define CAUSE_INVALID       0x10
#define CAUSE_UNIMPLEMENTED 0x20

#define ADDRESS_ERROR_EXCEPTION(Address,FromRead) \
	DoAddressError(NextInstruction == JUMP,Address,FromRead);\
	NextInstruction = JUMP;\
	JumpToLocation = PROGRAM_COUNTER;\
	return;

#define TEST_COP1_USABLE_EXCEPTION() \
	if ((STATUS_REGISTER & STATUS_CU1) == 0) {\
		DoCopUnusableException(NextInstruction == JUMP,1);\
		NextInstruction = JUMP;\
		JumpToLocation = PROGRAM_COUNTER;\
		return;\
	}

#define TEST_COP1_FP_EXCEPTION() \
	{ \
		const DWORD causeBits = (FPCR[31] >> 12) & 0x3F; \
		const DWORD enableBits = ((FPCR[31] >> 7) & 0x1F) | 0x20; \
		if((causeBits & enableBits) != 0) { \
			DoFPException(NextInstruction == JUMP); \
			NextInstruction = JUMP; \
			JumpToLocation = PROGRAM_COUNTER; \
			return; \
		} \
	}

#define TLB_READ_EXCEPTION(Address) \
	if (UseTlb) { \
		DoTLBMiss(NextInstruction == JUMP, Address, TRUE);\
		NextInstruction = JUMP;\
		JumpToLocation = PROGRAM_COUNTER;\
		return; \
	}

#define TLB_WRITE_EXCEPTION(Address) \
	if (UseTlb) { \
		DoTLBMiss(NextInstruction == JUMP, Address, FALSE);\
		NextInstruction = JUMP;\
		JumpToLocation = PROGRAM_COUNTER;\
		return; \
	}

#define CLEAR_COP1_CAUSE() \
	{ \
		FPCR[31] &= 0xFFFFC0FFF; \
	}

#define SET_COP1_CAUSE(cause) \
	{ \
		if(cause & CAUSE_UNIMPLEMENTED) { \
			FPCR[31] |= (((cause) & CAUSE_UNIMPLEMENTED) << 12); \
		} else { \
			FPCR[31] |= ((cause) << 12); \
		} \
	}

#define SET_COP1_FLAGS(cause) \
	{ \
		FPCR[31] |= ((cause) << 2); \
	}

/************************* OpCode functions *************************/
void _fastcall r4300i_J (void) {
	if(NextInstruction == JUMP) return;
	NextInstruction = DELAY_SLOT;
	JumpToLocation.UDW = (PROGRAM_COUNTER.UDW & 0xFFFFFFFFF0000000LL) + (Opcode.JMP.target << 2);
	TestInterpreterJump(PROGRAM_COUNTER,JumpToLocation,0,0);
}

void _fastcall r4300i_JAL (void) {
	if (NextInstruction == JUMP)
	{
		GPR[31].UDW = JumpToLocation.UDW + 4;
		return;
	}
	NextInstruction = DELAY_SLOT;
	JumpToLocation.UDW = (PROGRAM_COUNTER.UDW & 0xFFFFFFFFF0000000LL) + (Opcode.JMP.target << 2);
	TestInterpreterJump(PROGRAM_COUNTER,JumpToLocation,0,0);
	GPR[31].UDW = PROGRAM_COUNTER.UDW + 8;
}

void _fastcall r4300i_BEQ (void) {
	BOOL inDelay = NextInstruction == JUMP;
	NextInstruction = DELAY_SLOT;
	if (GPR[Opcode.BRANCH.rs].DW == GPR[Opcode.BRANCH.rt].DW) {
		if (inDelay) {
			PROGRAM_COUNTER.UDW = JumpToLocation.UDW - 4; // It will be incremented to JumpToLocation in main loop
			JumpToLocation.UDW = JumpToLocation.UDW + ((short)Opcode.BRANCH.offset << 2);
		}
		else {
			JumpToLocation.UDW = PROGRAM_COUNTER.UDW + ((short)Opcode.BRANCH.offset << 2) + 4;
		}
		TestInterpreterJump(PROGRAM_COUNTER, JumpToLocation, Opcode.BRANCH.rs, Opcode.BRANCH.rt);
	} else {
		if (inDelay) {
			PROGRAM_COUNTER.UDW = JumpToLocation.UDW - 4; // It will be incremented to JumpToLocation in main loop
			JumpToLocation.UDW = JumpToLocation.UDW + 4;
		}
		else {
			JumpToLocation.UDW = PROGRAM_COUNTER.UDW + 8;
		}
	}
}

void _fastcall r4300i_BNE (void) {
	BOOL inDelay = NextInstruction == JUMP;
	NextInstruction = DELAY_SLOT;
	if (GPR[Opcode.BRANCH.rs].DW != GPR[Opcode.BRANCH.rt].DW) {
		if (inDelay) {
			PROGRAM_COUNTER.UDW = JumpToLocation.UDW - 4; // It will be incremented to JumpToLocation in main loop
			JumpToLocation.UDW = JumpToLocation.UDW + ((short)Opcode.BRANCH.offset << 2);
		}
		else {
			JumpToLocation.UDW = PROGRAM_COUNTER.UDW + ((short)Opcode.BRANCH.offset << 2) + 4;
		}
		TestInterpreterJump(PROGRAM_COUNTER,JumpToLocation,Opcode.BRANCH.rs,Opcode.BRANCH.rt);
	} else {
		if (inDelay) {
			PROGRAM_COUNTER.UDW = JumpToLocation.UDW - 4; // It will be incremented to JumpToLocation in main loop
			JumpToLocation.UDW = JumpToLocation.UDW + 4;
		}
		else {
			JumpToLocation.UDW = PROGRAM_COUNTER.UDW + 8;
		}
	}
}

void _fastcall r4300i_BLEZ (void) {
	BOOL inDelay = NextInstruction == JUMP;
	NextInstruction = DELAY_SLOT;
	if (GPR[Opcode.BRANCH.rs].DW <= 0) {
		if (inDelay) {
			PROGRAM_COUNTER.UDW = JumpToLocation.UDW - 4; // It will be incremented to JumpToLocation in main loop
			JumpToLocation.UDW = JumpToLocation.UDW + ((short)Opcode.BRANCH.offset << 2);
		}
		else {
			JumpToLocation.UDW = PROGRAM_COUNTER.UDW + ((short)Opcode.BRANCH.offset << 2) + 4;
		}
		TestInterpreterJump(PROGRAM_COUNTER,JumpToLocation,Opcode.BRANCH.rs,0);
	} else {
		if (inDelay) {
			PROGRAM_COUNTER.UDW = JumpToLocation.UDW - 4; // It will be incremented to JumpToLocation in main loop
			JumpToLocation.UDW = JumpToLocation.UDW + 4;
		}
		else {
			JumpToLocation.UDW = PROGRAM_COUNTER.UDW + 8;
		}
	}
}

void _fastcall r4300i_BGTZ (void) {
	BOOL inDelay = NextInstruction == JUMP;
	NextInstruction = DELAY_SLOT;
	if (GPR[Opcode.BRANCH.rs].DW > 0) {
		if (inDelay) {
			PROGRAM_COUNTER.UDW = JumpToLocation.UDW - 4; // It will be incremented to JumpToLocation in main loop
			JumpToLocation.UDW = JumpToLocation.UDW + ((short)Opcode.BRANCH.offset << 2);
		}
		else {
			JumpToLocation.UDW = PROGRAM_COUNTER.UDW + ((short)Opcode.BRANCH.offset << 2) + 4;
		}
		TestInterpreterJump(PROGRAM_COUNTER,JumpToLocation,Opcode.BRANCH.rs,0);
	} else {
		if (inDelay) {
			PROGRAM_COUNTER.UDW = JumpToLocation.UDW - 4; // It will be incremented to JumpToLocation in main loop
			JumpToLocation.UDW = JumpToLocation.UDW + 4;
		}
		else {
			JumpToLocation.UDW = PROGRAM_COUNTER.UDW + 8;
		}
	}
}

void _fastcall r4300i_ADDI (void) {
#ifdef Interpreter_StackTest
	if (Opcode.BRANCH.rs == 29 && Opcode.BRANCH.rt == 29) {
		StackValue += (short)Opcode.BRANCH.offset;
	}
#endif
	long result = (GPR[Opcode.BRANCH.rs].W[0] + ((short)Opcode.BRANCH.offset));
	long sign = (long)((short)Opcode.BRANCH.offset) >> 31;
	if ((GPR[Opcode.BRANCH.rs].W[0] >> 31) == sign && (result >> 31) != sign) {
		DoIntegerOverflow(NextInstruction == JUMP);
		NextInstruction = JUMP;
		JumpToLocation = PROGRAM_COUNTER;
	} else {
		GPR[Opcode.BRANCH.rt].DW = result;
	}
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

void _fastcall r4300i_COP3 (void) {
	DoIllegalInstructionException(NextInstruction == JUMP);
	NextInstruction = JUMP;
	JumpToLocation = PROGRAM_COUNTER;
}

void _fastcall r4300i_BEQL (void) {
	BOOL inDelay = NextInstruction == JUMP;
	if (GPR[Opcode.BRANCH.rs].DW == GPR[Opcode.BRANCH.rt].DW) {
		NextInstruction = DELAY_SLOT;
		if (inDelay) {
			PROGRAM_COUNTER.UDW = JumpToLocation.UDW - 4; // It will be incremented to JumpToLocation in main loop
			JumpToLocation.UDW = JumpToLocation.UDW + ((short)Opcode.BRANCH.offset << 2);
		}
		else {
			JumpToLocation.UDW = PROGRAM_COUNTER.UDW + ((short)Opcode.BRANCH.offset << 2) + 4;
		}
		TestInterpreterJump(PROGRAM_COUNTER,JumpToLocation,Opcode.BRANCH.rs,Opcode.BRANCH.rt);
	} else {
		NextInstruction = JUMP;
		if (inDelay) {
			JumpToLocation.UDW = JumpToLocation.UDW + 4;
		}
		else {
			JumpToLocation.UDW = PROGRAM_COUNTER.UDW + 8;
		}
	}
}

void _fastcall r4300i_BNEL (void) {
	BOOL inDelay = NextInstruction == JUMP;
	if (GPR[Opcode.BRANCH.rs].DW != GPR[Opcode.BRANCH.rt].DW) {
		NextInstruction = DELAY_SLOT;
		if (inDelay) {
			PROGRAM_COUNTER.UDW = JumpToLocation.UDW - 4; // It will be incremented to JumpToLocation in main loop
			JumpToLocation.UDW = JumpToLocation.UDW + ((short)Opcode.BRANCH.offset << 2);
		}
		else {
			JumpToLocation.UDW = PROGRAM_COUNTER.UDW + ((short)Opcode.BRANCH.offset << 2) + 4;
		}
		TestInterpreterJump(PROGRAM_COUNTER,JumpToLocation,Opcode.BRANCH.rs,Opcode.BRANCH.rt);
	} else {
		NextInstruction = JUMP;
		if (inDelay) {
			JumpToLocation.UDW = JumpToLocation.UDW + 4;
		}
		else {
			JumpToLocation.UDW = PROGRAM_COUNTER.UDW + 8;
		}
	}
}

void _fastcall r4300i_BLEZL (void) {
	BOOL inDelay = NextInstruction == JUMP;
	if (GPR[Opcode.BRANCH.rs].DW <= 0) {
		NextInstruction = DELAY_SLOT;
		if (inDelay) {
			PROGRAM_COUNTER.UDW = JumpToLocation.UDW - 4; // It will be incremented to JumpToLocation in main loop
			JumpToLocation.UDW = JumpToLocation.UDW + ((short)Opcode.BRANCH.offset << 2);
		}
		else {
			JumpToLocation.UDW = PROGRAM_COUNTER.UDW + ((short)Opcode.BRANCH.offset << 2) + 4;
		}
		TestInterpreterJump(PROGRAM_COUNTER,JumpToLocation,Opcode.BRANCH.rs,0);
	} else {
		NextInstruction = JUMP;
		if (inDelay) {
			JumpToLocation.UDW = JumpToLocation.UDW + 4;
		}
		else {
			JumpToLocation.UDW = PROGRAM_COUNTER.UDW + 8;
		}
	}
}

void _fastcall r4300i_BGTZL (void) {
	BOOL inDelay = NextInstruction == JUMP;
	if (GPR[Opcode.BRANCH.rs].DW > 0) {
		NextInstruction = DELAY_SLOT;
		if (inDelay) {
			PROGRAM_COUNTER.UDW = JumpToLocation.UDW - 4; // It will be incremented to JumpToLocation in main loop
			JumpToLocation.UDW = JumpToLocation.UDW + ((short)Opcode.BRANCH.offset << 2);
		}
		else {
			JumpToLocation.UDW = PROGRAM_COUNTER.UDW + ((short)Opcode.BRANCH.offset << 2) + 4;
		}
		TestInterpreterJump(PROGRAM_COUNTER,JumpToLocation,Opcode.BRANCH.rs,0);
	} else {
		NextInstruction = JUMP;
		if (inDelay) {
			JumpToLocation.UDW = JumpToLocation.UDW + 4;
		}
		else {
			JumpToLocation.UDW = PROGRAM_COUNTER.UDW + 8;
		}
	}
}

void _fastcall r4300i_DADDI (void) {
	_int64 imm = (_int64)((short)Opcode.BRANCH.offset);
	_int64 result = GPR[Opcode.BRANCH.rs].DW + imm;
	_int64 sign = (imm >> 63);
	if ((GPR[Opcode.BRANCH.rs].DW >> 63) == sign && (result >> 63) != sign) {
		DoIntegerOverflow(NextInstruction == JUMP);
		NextInstruction = JUMP;
		JumpToLocation = PROGRAM_COUNTER;
	} else {
		GPR[Opcode.BRANCH.rt].DW = result;
	}
}

void _fastcall r4300i_DADDIU (void) {
	GPR[Opcode.BRANCH.rt].DW = GPR[Opcode.BRANCH.rs].DW + (_int64)((short)Opcode.BRANCH.offset);
}

QWORD LDL_MASK[8] = { 0,0xFF,0xFFFF,0xFFFFFF,0xFFFFFFFF,0xFFFFFFFFFF,
					 0xFFFFFFFFFFFF, 0xFFFFFFFFFFFFFF };
int LDL_SHIFT[8] = { 0, 8, 16, 24, 32, 40, 48, 56 };

void _fastcall r4300i_LDL (void) {
	DWORD Offset;
	QWORD Value;
	MIPS_DWORD Address;
	
	Address.UDW = GPR[Opcode.IMM.base].UDW + (short)Opcode.IMM.immediate;
	if (!IsValidAddress(Address)) {
		ADDRESS_ERROR_EXCEPTION(Address.UDW, TRUE);
		return;
	}
	Offset  = Address.UW[0] & 7;
	MIPS_DWORD AlignedAddress = Address;
	AlignedAddress.UW[0] &= ~7;

	if (!r4300i_LD_VAddr(AlignedAddress,&Value, NULL)) {
		if (ShowTLBMisses) {
			DisplayError("LDL TLB: %llX", Address.UDW);
		}
		TLB_READ_EXCEPTION(Address.UDW);
	} else {
		GPR[Opcode.BRANCH.rt].DW = GPR[Opcode.BRANCH.rt].DW & LDL_MASK[Offset];
		GPR[Opcode.BRANCH.rt].DW += Value << LDL_SHIFT[Offset];
	}
}

QWORD LDR_MASK[8] = { 0xFFFFFFFFFFFFFF00, 0xFFFFFFFFFFFF0000,
                      0xFFFFFFFFFF000000, 0xFFFFFFFF00000000,
                      0xFFFFFF0000000000, 0xFFFF000000000000,
                      0xFF00000000000000, 0 };
int LDR_SHIFT[8] = { 56, 48, 40, 32, 24, 16, 8, 0 };

void _fastcall r4300i_LDR (void) {
	DWORD Offset;
	QWORD Value;
	MIPS_DWORD Address;
	
	Address.UDW = GPR[Opcode.IMM.base].UDW + (short)Opcode.IMM.immediate;
	if (!IsValidAddress(Address)) {
		ADDRESS_ERROR_EXCEPTION(Address.UDW, TRUE);
		return;
	}
	Offset  = Address.UW[0] & 7;
	MIPS_DWORD AlignedAddress = Address;
	AlignedAddress.UW[0] &= ~7;

	if (!r4300i_LD_VAddr(AlignedAddress, &Value, NULL)) {
		if (ShowTLBMisses) {
			DisplayError("LDR TLB: %llX", Address.UDW);
		}
		TLB_READ_EXCEPTION(Address.UDW);
	} else {
		GPR[Opcode.BRANCH.rt].DW = GPR[Opcode.BRANCH.rt].DW & LDR_MASK[Offset];
		GPR[Opcode.BRANCH.rt].DW += Value >> LDR_SHIFT[Offset];
	}
}

void _fastcall r4300i_LB (void) {
	MIPS_DWORD Address;
	Address.UDW = GPR[Opcode.IMM.base].UDW + (short)Opcode.IMM.immediate;
	if (!IsValidAddress(Address)) {
		ADDRESS_ERROR_EXCEPTION(Address.UDW, TRUE);
		return;
	}
	if (!r4300i_LB_VAddr(Address,&GPR[Opcode.BRANCH.rt].UB[0])) {
		if (ShowTLBMisses) {
			DisplayError("LB TLB: %llX",Address.UDW);
		}
		TLB_READ_EXCEPTION(Address.UDW);
	} else {
		GPR[Opcode.BRANCH.rt].DW = GPR[Opcode.BRANCH.rt].B[0];
	}
}

void _fastcall r4300i_LH (void) {
	MIPS_DWORD Address;
	Address.UDW = GPR[Opcode.IMM.base].UDW + (short)Opcode.IMM.immediate;
	if ((Address.UW[0] & 1) != 0 || !IsValidAddress(Address)) {
		ADDRESS_ERROR_EXCEPTION(Address.UDW,TRUE);
		return;
	}
	if (!r4300i_LH_VAddr(Address,&GPR[Opcode.BRANCH.rt].UHW[0])) {
		if (ShowTLBMisses) {
			DisplayError("LH TLB: %llX",Address.UDW);
		}
		TLB_READ_EXCEPTION(Address.UDW);
	} else {
		GPR[Opcode.BRANCH.rt].DW = GPR[Opcode.BRANCH.rt].HW[0];
	}
}

DWORD LWL_MASK[4] = { 0,0xFF,0xFFFF,0xFFFFFF };
int LWL_SHIFT[4] = { 0, 8, 16, 24};

void _fastcall r4300i_LWL (void) {
	DWORD Offset, Value;
	MIPS_DWORD Address;
	
	Address.UDW = GPR[Opcode.IMM.base].UDW + (short)Opcode.IMM.immediate;
	if (!IsValidAddress(Address)) {
		ADDRESS_ERROR_EXCEPTION(Address.UDW, TRUE);
		return;
	}
	Offset  = Address.UW[0] & 3;
	MIPS_DWORD AlignedAddress = Address;
	AlignedAddress.UW[0] &= ~3;

	if (!r4300i_LW_VAddr(AlignedAddress,&Value,NULL)) {
		if (ShowTLBMisses) {
			DisplayError("LWL TLB: %llX", Address.UDW);
		}
		TLB_READ_EXCEPTION(Address.UDW);
	} else {
		GPR[Opcode.BRANCH.rt].DW = (int)(GPR[Opcode.BRANCH.rt].W[0] & LWL_MASK[Offset]);
		GPR[Opcode.BRANCH.rt].DW += (int)(Value << LWL_SHIFT[Offset]);
	}
}

void _fastcall r4300i_LW (void) {
	MIPS_DWORD Address;
	Address.UDW = GPR[Opcode.IMM.base].UDW + (short)Opcode.IMM.immediate;
	if ((Address.UW[0] & 3) != 0 || !IsValidAddress(Address)) {
		ADDRESS_ERROR_EXCEPTION(Address.UDW,TRUE);
	}
	
	if (ShowDebugMessages)
		Log_LW(PROGRAM_COUNTER,Address.UW[0]);

	if (!r4300i_LW_VAddr(Address,&GPR[Opcode.BRANCH.rt].UW[0], NULL)) {
		if (ShowTLBMisses) {
			DisplayError("LW TLB: %llX",Address.UDW);
		}
		TLB_READ_EXCEPTION(Address.UDW);
	} else {
		GPR[Opcode.BRANCH.rt].DW = GPR[Opcode.BRANCH.rt].W[0];
	}
}

void _fastcall r4300i_LBU (void) {
	MIPS_DWORD Address;
	Address.UDW = GPR[Opcode.IMM.base].UDW + (short)Opcode.IMM.immediate;
	if (!IsValidAddress(Address)) {
		ADDRESS_ERROR_EXCEPTION(Address.UDW, TRUE);
		return;
	}
	if (!r4300i_LB_VAddr(Address,&GPR[Opcode.BRANCH.rt].UB[0])) {
		if (ShowTLBMisses) {
			DisplayError("LBU TLB: %llX",Address.UDW);
		}
		TLB_READ_EXCEPTION(Address.UDW);
	} else {
		GPR[Opcode.BRANCH.rt].UDW = GPR[Opcode.BRANCH.rt].UB[0];
	}
}

void _fastcall r4300i_LHU (void) {
	MIPS_DWORD Address;
	Address.UDW = GPR[Opcode.IMM.base].UDW + (short)Opcode.IMM.immediate;
	if ((Address.UW[0] & 1) != 0 || !IsValidAddress(Address)) {
		ADDRESS_ERROR_EXCEPTION(Address.UDW,TRUE);
		return;
	}
	if (!r4300i_LH_VAddr(Address,&GPR[Opcode.BRANCH.rt].UHW[0])) {
		if (ShowTLBMisses) {
			DisplayError("LHU TLB: %llX",Address.UDW);
		}
		TLB_READ_EXCEPTION(Address.UDW);
	} else {
		GPR[Opcode.BRANCH.rt].UDW = GPR[Opcode.BRANCH.rt].UHW[0];
	}
}

DWORD LWR_MASK[4] = { 0xFFFFFF00, 0xFFFF0000, 0xFF000000, 0 };
int LWR_SHIFT[4] = { 24, 16 ,8, 0 };

void _fastcall r4300i_LWR (void) {
	DWORD Offset, Value;
	MIPS_DWORD Address;
	
	Address.UDW = GPR[Opcode.IMM.base].UDW + (short)Opcode.IMM.immediate;
	if (!IsValidAddress(Address)) {
		ADDRESS_ERROR_EXCEPTION(Address.UDW, TRUE);
		return;
	}
	Offset  = Address.UW[0] & 3;
	MIPS_DWORD AlignedAddress = Address;
	AlignedAddress.UW[0] &= ~3;

	if (!r4300i_LW_VAddr(AlignedAddress,&Value, NULL)) {
		if (ShowTLBMisses) {
			DisplayError("LWR TLB: %llX", Address.UDW);
		}
		TLB_READ_EXCEPTION(Address.UDW);
	} else {
		GPR[Opcode.BRANCH.rt].DW = (int)(GPR[Opcode.BRANCH.rt].W[0] & LWR_MASK[Offset]);
		GPR[Opcode.BRANCH.rt].DW += (int)(Value >> LWR_SHIFT[Offset]);
	}
}

void _fastcall r4300i_LWU (void) {
	MIPS_DWORD Address;
	Address.UDW = GPR[Opcode.IMM.base].UDW + (short)Opcode.IMM.immediate;
	if ((Address.UW[0] & 3) != 0 || !IsValidAddress(Address)) {
		ADDRESS_ERROR_EXCEPTION(Address.UDW,TRUE);
		return;
	}

	if (!r4300i_LW_VAddr(Address,&GPR[Opcode.BRANCH.rt].UW[0], NULL)) {
		if (ShowTLBMisses) {
			DisplayError("LWU TLB: %llX",Address.UDW);
		}
		TLB_READ_EXCEPTION(Address.UDW);
	} else {
		GPR[Opcode.BRANCH.rt].UDW = GPR[Opcode.BRANCH.rt].UW[0];
	}
}

void _fastcall r4300i_SB (void) {
	MIPS_DWORD Address;
	Address.UDW = GPR[Opcode.IMM.base].UDW + (short)Opcode.IMM.immediate;
	if (!IsValidAddress(Address)) {
		ADDRESS_ERROR_EXCEPTION(Address.UDW,FALSE);
		return;
	}
	if (!r4300i_SB_VAddr(Address,&GPR[Opcode.BRANCH.rt])) {
		if (ShowTLBMisses) {
			DisplayError("SB TLB: %llX", Address.UDW);
		}
		TLB_WRITE_EXCEPTION(Address.UW[0]);
	}
}

void _fastcall r4300i_SH (void) {
	MIPS_DWORD Address;
	Address.UDW = GPR[Opcode.IMM.base].UDW + (short)Opcode.IMM.immediate;
	if ((Address.UW[0] & 1) != 0 || !IsValidAddress(Address)) {
		ADDRESS_ERROR_EXCEPTION(Address.UDW,FALSE);
		return;
	}
	if (!r4300i_SH_VAddr(Address,&GPR[Opcode.BRANCH.rt])) {
		if (ShowTLBMisses) {
			DisplayError("SH TLB: %llX", Address.UDW);
		}
		TLB_WRITE_EXCEPTION(Address.UW[0]);
	}
}

DWORD SWL_MASK[4] = { 0,0xFF000000,0xFFFF0000,0xFFFFFF00 };
int SWL_SHIFT[4] = { 0, 8, 16, 24 };

void _fastcall r4300i_SWL (void) {
	DWORD Offset, Value;
	MIPS_DWORD Address;

	Address.UDW = GPR[Opcode.IMM.base].UDW + (short)Opcode.IMM.immediate;
	if (!IsValidAddress(Address)) {
		ADDRESS_ERROR_EXCEPTION(Address.UDW, FALSE);
		return;
	}
	Offset  = Address.UW[0] & 3;
	MIPS_DWORD AlignedAddress = Address;
	AlignedAddress.UW[0] &= ~3;

	if (!r4300i_LW_VAddr(AlignedAddress,&Value,NULL)) {
		if (ShowTLBMisses) {
			DisplayError("SWL TLB: %llX", Address.UDW);
		}
		TLB_WRITE_EXCEPTION(Address.UDW);
	} else {
		Value &= SWL_MASK[Offset];
		Value += GPR[Opcode.BRANCH.rt].UW[0] >> SWL_SHIFT[Offset];

		if (!r4300i_SW_VAddr(AlignedAddress, Value)) {
			if (ShowTLBMisses) {
				DisplayError("SWL TLB: %llX", Address.UDW);
			}
			TLB_WRITE_EXCEPTION(Address.UDW);
		}
	}
}


void _fastcall r4300i_SW (void) {
	MIPS_DWORD Address;
	Address.UDW = GPR[Opcode.IMM.base].UDW + (short)Opcode.IMM.immediate;
	if ((Address.UW[0] & 3) != 0 || !IsValidAddress(Address)) {
		ADDRESS_ERROR_EXCEPTION(Address.UDW,FALSE);
		return;
	}

	if (ShowDebugMessages)
		Log_SW(PROGRAM_COUNTER,Address.UW[0],GPR[Opcode.BRANCH.rt].UW[0]);

	if (!r4300i_SW_VAddr(Address,GPR[Opcode.BRANCH.rt].UW[0])) {
		if (ShowTLBMisses) {
			DisplayError("SW TLB: %llX", Address.UDW);
		}
		TLB_WRITE_EXCEPTION(Address.UDW);
	}
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
	DWORD Offset;
	QWORD Value;
	MIPS_DWORD Address;
	
	Address.UDW = GPR[Opcode.IMM.base].UDW + (short)Opcode.IMM.immediate;
	if (!IsValidAddress(Address)) {
		ADDRESS_ERROR_EXCEPTION(Address.UDW, FALSE);
		return;
	}
	Offset  = Address.UW[0] & 7;
	MIPS_DWORD AlignedAddress = Address;
	AlignedAddress.UW[0] &= ~7;

	if (!r4300i_LD_VAddr(AlignedAddress,&Value,NULL)) {
		if (ShowTLBMisses) {
			DisplayError("SDL TLB: %llX", Address.UDW);
		}
		TLB_WRITE_EXCEPTION(Address.UDW);
	} else {
		Value &= SDL_MASK[Offset];
		Value += GPR[Opcode.BRANCH.rt].UDW >> SDL_SHIFT[Offset];

		if (!r4300i_SD_VAddr(AlignedAddress, Value)) {
			if (ShowTLBMisses) {
				DisplayError("SDL TLB: %llX", Address.UDW);
			}
			TLB_WRITE_EXCEPTION(Address.UDW);
		}
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
	DWORD Offset;
	QWORD Value;
	MIPS_DWORD Address;
	
	Address.UDW = GPR[Opcode.IMM.base].UDW + (short)Opcode.IMM.immediate;
	if (!IsValidAddress(Address)) {
		ADDRESS_ERROR_EXCEPTION(Address.UDW, FALSE);
		return;
	}
	Offset  = Address.UW[0] & 7;
	MIPS_DWORD AlignedAddress = Address;
	AlignedAddress.UW[0] &= ~7;

	if (!r4300i_LD_VAddr(AlignedAddress,&Value,NULL)) {
		if (ShowTLBMisses) {
			DisplayError("SDR TLB: %llX", Address.UDW);
		}
		TLB_WRITE_EXCEPTION(Address.UDW);
	} else {
		Value &= SDR_MASK[Offset];
		Value += GPR[Opcode.BRANCH.rt].UDW << SDR_SHIFT[Offset];

		if (!r4300i_SD_VAddr(AlignedAddress, Value)) {
			if (ShowTLBMisses) {
				DisplayError("SDR TLB: %llX", Address.UDW);
			}
			TLB_WRITE_EXCEPTION(Address.UDW);
		}
	}
}

DWORD SWR_MASK[4] = { 0x00FFFFFF,0x0000FFFF,0x000000FF,0x00000000 };
int SWR_SHIFT[4] = { 24, 16 , 8, 0  };

void _fastcall r4300i_SWR (void) {
	DWORD Offset, Value;
	MIPS_DWORD Address;
	
	Address.UDW = GPR[Opcode.IMM.base].UDW + (short)Opcode.IMM.immediate;
	if (!IsValidAddress(Address)) {
		ADDRESS_ERROR_EXCEPTION(Address.UDW, FALSE);
		return;
	}
	Offset  = Address.UW[0] & 3;
	MIPS_DWORD AlignedAddress = Address;
	AlignedAddress.UW[0] &= ~3;

	if (!r4300i_LW_VAddr(AlignedAddress,&Value,NULL)) {
		if (ShowTLBMisses) {
			DisplayError("SWR TLB: %llX", Address.UDW);
		}
		TLB_WRITE_EXCEPTION(Address.UDW);
	} else {
		Value &= SWR_MASK[Offset];
		Value += GPR[Opcode.BRANCH.rt].UW[0] << SWR_SHIFT[Offset];

		if (!r4300i_SW_VAddr(AlignedAddress, Value)) {
			if (ShowTLBMisses) {
				DisplayError("SWR TLB: %llX", Address.UDW);
			}
			TLB_WRITE_EXCEPTION(Address.UDW);
		}
	}
}

void _fastcall r4300i_CACHE (void) {
	if (HaveDebugger && !LogOptions.LogCache) { return; }
	LogMessage("%08X: Cache operation %d, 0x%08X", PROGRAM_COUNTER, Opcode.BRANCH.rt,
		GPR[Opcode.IMM.base].UW[0] + (short)Opcode.IMM.immediate );
}

void _fastcall r4300i_LL (void) {
	MIPS_DWORD Address;
	Address.UDW = GPR[Opcode.IMM.base].UDW + (short)Opcode.IMM.immediate;
	if ((Address.UW[0] & 3) != 0 || !IsValidAddress(Address)) {
		ADDRESS_ERROR_EXCEPTION(Address.UDW,TRUE);
		return;
	}
	DWORD PAddr;
	DWORD tmp;

	if (!r4300i_LW_VAddr(Address,&tmp,&PAddr)) {
		if (ShowTLBMisses) {
			DisplayError("LL TLB: %llX",Address.UDW);
		}
		TLB_READ_EXCEPTION(Address.UDW);
	} else {
		LLBit = 1;
		LLADDR_REGISTER = PAddr >> 4;
		if (Opcode.BRANCH.rt == 0) { return; }
		GPR[Opcode.BRANCH.rt].DW = (long)tmp;
	}
}

void _fastcall r4300i_LLD(void) {
	MIPS_DWORD Address;
	Address.UDW = GPR[Opcode.IMM.base].UDW + (short)Opcode.IMM.immediate;
	if ((Address.UW[0] & 7) != 0 || !IsValidAddress(Address)) {
		ADDRESS_ERROR_EXCEPTION(Address.UDW, TRUE);
		return;
	}
	DWORD PAddr;
	MIPS_DWORD tmp;

	if (!r4300i_LD_VAddr(Address, &tmp.UDW,&PAddr)) {
		if (ShowTLBMisses) {
			DisplayError("LL TLB: %llX", Address.UDW);
		}
		TLB_READ_EXCEPTION(Address.UDW);
	}
	else {
		LLBit = 1;
		LLADDR_REGISTER = PAddr >> 4;
		if (Opcode.BRANCH.rt == 0) { return; }
		GPR[Opcode.BRANCH.rt] = tmp;
	}
}

void _fastcall r4300i_LWC1 (void) {
	MIPS_DWORD Address;
	Address.UDW = GPR[Opcode.IMM.base].UDW + (short)Opcode.IMM.immediate;
	TEST_COP1_USABLE_EXCEPTION();
	if ((Address.UW[0] & 3) != 0 || !IsValidAddress(Address)) {
		ADDRESS_ERROR_EXCEPTION(Address.UDW,TRUE);
		return;
	}
	if (!r4300i_LW_VAddr(Address,&*(DWORD *)FPRFloatLoadStoreLocation[Opcode.FP.ft],NULL)) {
		if (ShowTLBMisses) {
			DisplayError("LWC1 TLB: %llX",Address.UDW);
		}
		TLB_READ_EXCEPTION(Address.UDW);
	}
}

void _fastcall r4300i_SC (void) {
	MIPS_DWORD Address;
	Address.UDW = GPR[Opcode.IMM.base].UDW + (short)Opcode.IMM.immediate;
	if ((Address.UW[0] & 3) != 0 || !IsValidAddress(Address)) {
		ADDRESS_ERROR_EXCEPTION(Address.UDW,FALSE);
		return;
	}

	if (ShowDebugMessages)
		Log_SW(PROGRAM_COUNTER,Address.UW[0],GPR[Opcode.BRANCH.rt].UW[0]);

	if (LLBit == 1) {
		if (!r4300i_SW_VAddr(Address,GPR[Opcode.BRANCH.rt].UW[0])) {
			if (ShowTLBMisses) {
				DisplayError("SW TLB: %llX", Address.UDW);
			}
			TLB_WRITE_EXCEPTION(Address.UDW);
		}
	}
	GPR[Opcode.BRANCH.rt].UDW = LLBit;
}

void _fastcall r4300i_SCD(void) {
	MIPS_DWORD Address;
	Address.UDW = GPR[Opcode.IMM.base].UDW + (short)Opcode.IMM.immediate;
	if ((Address.UW[0] & 7) != 0 || !IsValidAddress(Address)) {
		ADDRESS_ERROR_EXCEPTION(Address.UDW, FALSE);
		return;
	}

	if (LLBit == 1) {
		if (!r4300i_SD_VAddr(Address, GPR[Opcode.BRANCH.rt].UDW)) {
			if (ShowTLBMisses) {
				DisplayError("SW TLB: %llX", Address.UDW);
			}
			TLB_WRITE_EXCEPTION(Address.UDW);
		}
	}
	GPR[Opcode.BRANCH.rt].UDW = LLBit;
}

void _fastcall r4300i_LD (void) {
	MIPS_DWORD Address;
	Address.UDW = GPR[Opcode.IMM.base].UDW + (short)Opcode.IMM.immediate;
	if ((Address.UW[0] & 7) != 0 || !IsValidAddress(Address)) {
		ADDRESS_ERROR_EXCEPTION(Address.UDW, TRUE);
		return;
	}
	if (!r4300i_LD_VAddr(Address,&GPR[Opcode.BRANCH.rt].UDW,NULL)) {
		if (ShowTLBMisses) {
			DisplayError("LD TLB: %llX", Address.UDW);
		}
		TLB_READ_EXCEPTION(Address.UDW);
	}
#ifdef Interpreter_StackTest
	if (Opcode.BRANCH.rt == 29) {
		StackValue = GPR[Opcode.BRANCH.rt].W[0];
	}
#endif
}


void _fastcall r4300i_LDC1 (void) {
	MIPS_DWORD Address;
	Address.UDW = GPR[Opcode.IMM.base].UDW + (short)Opcode.IMM.immediate;

	TEST_COP1_USABLE_EXCEPTION();
	if ((Address.UW[0] & 7) != 0 || !IsValidAddress(Address)) {
		ADDRESS_ERROR_EXCEPTION(Address.UDW,TRUE);
		return;
	}
	if (!r4300i_LD_VAddr(Address,&*(unsigned __int64 *)FPRDoubleLocation[Opcode.FP.ft],NULL)) {
		if (ShowTLBMisses) {
			DisplayError("LDC1 TLB: %llX", Address.UDW);
		}
		TLB_READ_EXCEPTION(Address.UDW);
	}
}

void _fastcall r4300i_SWC1 (void) {
	MIPS_DWORD Address;
	Address.UDW = GPR[Opcode.IMM.base].UDW + (short)Opcode.IMM.immediate;
	TEST_COP1_USABLE_EXCEPTION();
	if ((Address.UW[0] & 3) != 0 || !IsValidAddress(Address)) {
		ADDRESS_ERROR_EXCEPTION(Address.UDW,FALSE);
		return;
	}

	if (!r4300i_SW_VAddr(Address,*(DWORD *)FPRFloatLoadStoreLocation[Opcode.FP.ft])) {
		if (ShowTLBMisses) {
			DisplayError("SWC1 TLB: %llX", Address.UDW);
		}
		TLB_WRITE_EXCEPTION(Address.UDW);
	}
}

void _fastcall r4300i_SDC1 (void) {
	MIPS_DWORD Address;
	Address.UDW = GPR[Opcode.IMM.base].UDW + (short)Opcode.IMM.immediate;

	TEST_COP1_USABLE_EXCEPTION();
	if ((Address.UW[0] & 7) != 0 || !IsValidAddress(Address)) {
		ADDRESS_ERROR_EXCEPTION(Address.UDW,FALSE);
		return;
	}
	if (!r4300i_SD_VAddr(Address,*(__int64 *)FPRDoubleLocation[Opcode.FP.ft])) {
		if (ShowTLBMisses) {
			DisplayError("SDC1 TLB: %llX", Address.UDW);
		}
		TLB_WRITE_EXCEPTION(Address.UDW);
	}
}

void _fastcall r4300i_SD (void) {
	MIPS_DWORD Address;
	Address.UDW = GPR[Opcode.IMM.base].UDW + (short)Opcode.IMM.immediate;
	if ((Address.UW[0] & 7) != 0 || !IsValidAddress(Address)) {
		ADDRESS_ERROR_EXCEPTION(Address.UDW,FALSE);
		return;
	}
	if (!r4300i_SD_VAddr(Address,GPR[Opcode.BRANCH.rt].UDW)) {
		if (ShowTLBMisses) {
			DisplayError("SD TLB: %llX", Address.UDW);
		}
		TLB_WRITE_EXCEPTION(Address.UDW);
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
	GPR[Opcode.REG.rd].DW = (long)(GPR[Opcode.BRANCH.rt].DW >> Opcode.REG.sa);
}

void _fastcall r4300i_SPECIAL_SLLV (void) {
	if (Opcode.REG.rd == 0) { return; }
	GPR[Opcode.REG.rd].DW = (GPR[Opcode.BRANCH.rt].W[0] << (GPR[Opcode.BRANCH.rs].UW[0] & 0x1F));
}

void _fastcall r4300i_SPECIAL_SRLV (void) {
	GPR[Opcode.REG.rd].DW = (int)(GPR[Opcode.BRANCH.rt].UW[0] >> (GPR[Opcode.BRANCH.rs].UW[0] & 0x1F));
}

void _fastcall r4300i_SPECIAL_SRAV (void) {
	GPR[Opcode.REG.rd].DW = (long)(GPR[Opcode.BRANCH.rt].DW >> (GPR[Opcode.BRANCH.rs].UW[0] & 0x1F));
}

void _fastcall r4300i_SPECIAL_JR (void) {
	if (NextInstruction == JUMP) return;
	NextInstruction = DELAY_SLOT;
	JumpToLocation = GPR[Opcode.BRANCH.rs];
	if (JumpToLocation.UW[0] & 3) {
		PROGRAM_COUNTER = JumpToLocation;
		DoAddressError(FALSE, GPR[Opcode.BRANCH.rs].UDW, TRUE);
		NextInstruction = JUMP;
		JumpToLocation = PROGRAM_COUNTER;
	}
}

void _fastcall r4300i_SPECIAL_JALR (void) {
	if (NextInstruction == JUMP) {
		GPR[Opcode.REG.rd].UDW = JumpToLocation.UDW + 4;
		return;
	}
	NextInstruction = DELAY_SLOT;
	JumpToLocation = GPR[Opcode.BRANCH.rs];
	GPR[Opcode.REG.rd].UDW = PROGRAM_COUNTER.UDW + 8;
	if (JumpToLocation.UW[0] & 3) {
		PROGRAM_COUNTER = JumpToLocation;
		DoAddressError(FALSE, GPR[Opcode.BRANCH.rs].UDW, TRUE);
		NextInstruction = JUMP;
		JumpToLocation = PROGRAM_COUNTER;
	}
}

void _fastcall r4300i_SPECIAL_SYSCALL (void) {
	DoSysCallException(NextInstruction == JUMP);
	NextInstruction = JUMP;
	JumpToLocation = PROGRAM_COUNTER;
}

void _fastcall r4300i_SPECIAL_BREAK (void) {
	DoBreakException(NextInstruction == JUMP);
	NextInstruction = JUMP;
	JumpToLocation = PROGRAM_COUNTER;
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
	if (GPR[Opcode.BRANCH.rs].W[0] == INT_MIN && GPR[Opcode.BRANCH.rt].W[0] == -1) {
		// An overflow exception never occurs. This is the only set of inputs that overflows on x86
		LO.DW = INT_MIN;
		HI.DW = 0;
	} else if (GPR[Opcode.BRANCH.rt].UDW != 0) {
		LO.DW = GPR[Opcode.BRANCH.rs].W[0] / GPR[Opcode.BRANCH.rt].W[0];
		HI.DW = GPR[Opcode.BRANCH.rs].W[0] % GPR[Opcode.BRANCH.rt].W[0];
	} else {
		if (GPR[Opcode.BRANCH.rs].W[0] < 0) {
			LO.DW = 1;
		}
		else {
			LO.DW = -1;
		}
		HI.DW = GPR[Opcode.BRANCH.rs].W[0];
		if (ShowDebugMessages)
			DisplayError("DIV by 0 ???");
	}
}

void _fastcall r4300i_SPECIAL_DIVU (void) {
	if ( GPR[Opcode.BRANCH.rt].UDW != 0 ) {
		LO.DW = (long)(GPR[Opcode.BRANCH.rs].UW[0] / GPR[Opcode.BRANCH.rt].UW[0]);
		HI.DW = (long)(GPR[Opcode.BRANCH.rs].UW[0] % GPR[Opcode.BRANCH.rt].UW[0]);
	} else {
		LO.DW = -1;
		HI.DW = GPR[Opcode.BRANCH.rs].W[0];
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
		if (GPR[Opcode.BRANCH.rs].DW < 0) {
			LO.DW = 1;
		}
		else {
			LO.DW = -1;
		}
		HI.DW = GPR[Opcode.BRANCH.rs].DW;
		if (ShowDebugMessages)
			DisplayError("DDIV by 0 ???");
	}
}

void _fastcall r4300i_SPECIAL_DDIVU (void) {
	if ( GPR[Opcode.BRANCH.rt].UDW != 0 ) {
		LO.UDW = GPR[Opcode.BRANCH.rs].UDW / GPR[Opcode.BRANCH.rt].UDW;
		HI.UDW = GPR[Opcode.BRANCH.rs].UDW % GPR[Opcode.BRANCH.rt].UDW;
	} else {
		LO.DW = -1;
		HI.DW = GPR[Opcode.BRANCH.rs].DW;
		if (ShowDebugMessages)
			DisplayError("DDIVU by 0 ???");
	}
}

void _fastcall r4300i_SPECIAL_ADD (void) {
	long result = GPR[Opcode.BRANCH.rs].W[0] + GPR[Opcode.BRANCH.rt].W[0];
	long sign = GPR[Opcode.BRANCH.rt].W[0] >> 31;
	if ((GPR[Opcode.BRANCH.rs].W[0] >> 31) == sign && (result >> 31) != sign) {
		DoIntegerOverflow(NextInstruction == JUMP);
		NextInstruction = JUMP;
		JumpToLocation = PROGRAM_COUNTER;
	} else {
		GPR[Opcode.REG.rd].DW = result;
	}
}

void _fastcall r4300i_SPECIAL_ADDU (void) {
	GPR[Opcode.REG.rd].DW = GPR[Opcode.BRANCH.rs].W[0] + GPR[Opcode.BRANCH.rt].W[0];
}

void _fastcall r4300i_SPECIAL_SUB (void) {
	long result = GPR[Opcode.BRANCH.rs].W[0] - GPR[Opcode.BRANCH.rt].W[0];
	long sign = GPR[Opcode.BRANCH.rt].W[0] >> 31;
	if ((GPR[Opcode.BRANCH.rs].W[0] >> 31) != sign && (result >> 31) == sign) {
		DoIntegerOverflow(NextInstruction == JUMP);
		NextInstruction = JUMP;
		JumpToLocation = PROGRAM_COUNTER;
	} else {
		GPR[Opcode.REG.rd].DW = result;
	}
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
	_int64 result = GPR[Opcode.BRANCH.rs].DW + GPR[Opcode.BRANCH.rt].DW;
	_int64 sign = GPR[Opcode.BRANCH.rt].DW >> 63;
	if ((GPR[Opcode.BRANCH.rs].DW >> 63) == sign && (result >> 63) != sign) {
		DoIntegerOverflow(NextInstruction == JUMP);
		NextInstruction = JUMP;
		JumpToLocation = PROGRAM_COUNTER;
	} else {
		GPR[Opcode.REG.rd].DW = result;
	}
}

void _fastcall r4300i_SPECIAL_DADDU (void) {
	GPR[Opcode.REG.rd].DW = GPR[Opcode.BRANCH.rs].DW + GPR[Opcode.BRANCH.rt].DW;
}

void _fastcall r4300i_SPECIAL_DSUB (void) {
	_int64 result = GPR[Opcode.BRANCH.rs].DW - GPR[Opcode.BRANCH.rt].DW;
	_int64 sign = GPR[Opcode.BRANCH.rt].DW >> 63;
	if ((GPR[Opcode.BRANCH.rs].DW >> 63) != sign && (result >> 63) == sign) {
		DoIntegerOverflow(NextInstruction == JUMP);
		NextInstruction = JUMP;
		JumpToLocation = PROGRAM_COUNTER;
	} else {
		GPR[Opcode.REG.rd].DW = result;
	}
}

void _fastcall r4300i_SPECIAL_DSUBU (void) {
	GPR[Opcode.REG.rd].DW = GPR[Opcode.BRANCH.rs].DW - GPR[Opcode.BRANCH.rt].DW;
}

void _fastcall r4300i_SPECIAL_TGE(void) {
	if (GPR[Opcode.BRANCH.rs].DW >= GPR[Opcode.BRANCH.rt].DW) {
		DoTrapException(NextInstruction == JUMP);
		NextInstruction = JUMP;
		JumpToLocation = PROGRAM_COUNTER;
	}
}

void _fastcall r4300i_SPECIAL_TGEU(void) {
	if (GPR[Opcode.BRANCH.rs].UDW >= GPR[Opcode.BRANCH.rt].UDW) {
		DoTrapException(NextInstruction == JUMP);
		NextInstruction = JUMP;
		JumpToLocation = PROGRAM_COUNTER;
	}
}

void _fastcall r4300i_SPECIAL_TLT(void) {
	if (GPR[Opcode.BRANCH.rs].DW < GPR[Opcode.BRANCH.rt].DW) {
		DoTrapException(NextInstruction == JUMP);
		NextInstruction = JUMP;
		JumpToLocation = PROGRAM_COUNTER;
	}
}

void _fastcall r4300i_SPECIAL_TLTU(void) {
	if (GPR[Opcode.BRANCH.rs].UDW < GPR[Opcode.BRANCH.rt].UDW) {
		DoTrapException(NextInstruction == JUMP);
		NextInstruction = JUMP;
		JumpToLocation = PROGRAM_COUNTER;
	}
}

void _fastcall r4300i_SPECIAL_TEQ (void) {
	if (GPR[Opcode.BRANCH.rs].DW == GPR[Opcode.BRANCH.rt].DW) {
		DoTrapException(NextInstruction == JUMP);
		NextInstruction = JUMP;
		JumpToLocation = PROGRAM_COUNTER;
	}
}

void _fastcall r4300i_SPECIAL_TNE(void) {
	if (GPR[Opcode.BRANCH.rs].DW != GPR[Opcode.BRANCH.rt].DW) {
		DoTrapException(NextInstruction == JUMP);
		NextInstruction = JUMP;
		JumpToLocation = PROGRAM_COUNTER;
	}
}
void _fastcall r4300i_REGIMM_TGEI(void) {
	if (GPR[Opcode.BRANCH.rs].DW >= (__int64)((short)Opcode.BRANCH.offset)) {
		DoTrapException(NextInstruction == JUMP);
		NextInstruction = JUMP;
		JumpToLocation = PROGRAM_COUNTER;
	}
}

void _fastcall  r4300i_REGIMM_TGEIU(void) {
	if (GPR[Opcode.BRANCH.rs].UDW >= (unsigned __int64)((short)Opcode.BRANCH.offset)) {
		DoTrapException(NextInstruction == JUMP);
		NextInstruction = JUMP;
		JumpToLocation = PROGRAM_COUNTER;
	}
}

void _fastcall  r4300i_REGIMM_TLTI(void) {
	if (GPR[Opcode.BRANCH.rs].DW < (__int64)((short)Opcode.BRANCH.offset)) {
		DoTrapException(NextInstruction == JUMP);
		NextInstruction = JUMP;
		JumpToLocation = PROGRAM_COUNTER;
	}
}

void _fastcall  r4300i_REGIMM_TLTIU(void) {
	if (GPR[Opcode.BRANCH.rs].UDW < (unsigned __int64)((short)Opcode.BRANCH.offset)) {
		DoTrapException(NextInstruction == JUMP);
		NextInstruction = JUMP;
		JumpToLocation = PROGRAM_COUNTER;
	}
}

void _fastcall  r4300i_REGIMM_TEQI(void) {
	if (GPR[Opcode.BRANCH.rs].DW == (__int64)((short)Opcode.BRANCH.offset)) {
		DoTrapException(NextInstruction == JUMP);
		NextInstruction = JUMP;
		JumpToLocation = PROGRAM_COUNTER;
	}
}

void _fastcall  r4300i_REGIMM_TNEI(void) {
	if (GPR[Opcode.BRANCH.rs].DW != (__int64)((short)Opcode.BRANCH.offset)) {
		DoTrapException(NextInstruction == JUMP);
		NextInstruction = JUMP;
		JumpToLocation = PROGRAM_COUNTER;
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
	BOOL inDelay = NextInstruction == JUMP;
	NextInstruction = DELAY_SLOT;
	if (GPR[Opcode.BRANCH.rs].DW < 0) {
		if (inDelay) {
			PROGRAM_COUNTER.UDW = JumpToLocation.UDW - 4; // It will be incremented to JumpToLocation in main loop
			JumpToLocation.UDW = JumpToLocation.UDW + ((short)Opcode.BRANCH.offset << 2);
		}
		else {
			JumpToLocation.UDW = PROGRAM_COUNTER.UDW + ((short)Opcode.BRANCH.offset << 2) + 4;
		}
		TestInterpreterJump(PROGRAM_COUNTER,JumpToLocation,Opcode.BRANCH.rs,0);
	} else {
		if (inDelay) {
			PROGRAM_COUNTER.UDW = JumpToLocation.UDW - 4; // It will be incremented to JumpToLocation in main loop
			JumpToLocation.UDW = JumpToLocation.UDW + 4;
		}
		else {
			JumpToLocation.UDW = PROGRAM_COUNTER.UDW + 8;
		}
	}
}

void _fastcall r4300i_REGIMM_BGEZ (void) {
	BOOL inDelay = NextInstruction == JUMP;
	NextInstruction = DELAY_SLOT;
	if (GPR[Opcode.BRANCH.rs].DW >= 0) {
		if (inDelay) {
			PROGRAM_COUNTER.UDW = JumpToLocation.UDW - 4; // It will be incremented to JumpToLocation in main loop
			JumpToLocation.UDW = JumpToLocation.UDW + ((short)Opcode.BRANCH.offset << 2);
		}
		else {
			JumpToLocation.UDW = PROGRAM_COUNTER.UDW + ((short)Opcode.BRANCH.offset << 2) + 4;
		}
		TestInterpreterJump(PROGRAM_COUNTER,JumpToLocation,Opcode.BRANCH.rs,0);
	} else {
		if (inDelay) {
			PROGRAM_COUNTER.UDW = JumpToLocation.UDW - 4; // It will be incremented to JumpToLocation in main loop
			JumpToLocation.UDW = JumpToLocation.UDW + 4;
		}
		else {
			JumpToLocation.UDW = PROGRAM_COUNTER.UDW + 8;
		}
	}
}

void _fastcall r4300i_REGIMM_BLTZL (void) {
	BOOL inDelay = NextInstruction == JUMP;
	if (GPR[Opcode.BRANCH.rs].DW < 0) {
		NextInstruction = DELAY_SLOT;
		if (inDelay) {
			PROGRAM_COUNTER.UDW = JumpToLocation.UDW - 4; // It will be incremented to JumpToLocation in main loop
			JumpToLocation.UDW = JumpToLocation.UDW + ((short)Opcode.BRANCH.offset << 2);
		}
		else {
			JumpToLocation.UDW = PROGRAM_COUNTER.UDW + ((short)Opcode.BRANCH.offset << 2) + 4;
		}
		TestInterpreterJump(PROGRAM_COUNTER,JumpToLocation,Opcode.BRANCH.rs,0);
	} else {
		NextInstruction = JUMP;
		if (inDelay) {
			JumpToLocation.UDW = JumpToLocation.UDW + 4;
		}
		else {
			JumpToLocation.UDW = PROGRAM_COUNTER.UDW + 8;
		}
	}
}

void _fastcall r4300i_REGIMM_BGEZL (void) {
	BOOL inDelay = NextInstruction == JUMP;
	if (GPR[Opcode.BRANCH.rs].DW >= 0) {
		NextInstruction = DELAY_SLOT;
		if (inDelay) {
			PROGRAM_COUNTER.UDW = JumpToLocation.UDW - 4; // It will be incremented to JumpToLocation in main loop
			JumpToLocation.UDW = JumpToLocation.UDW + ((short)Opcode.BRANCH.offset << 2);
		}
		else {
			JumpToLocation.UDW = PROGRAM_COUNTER.UDW + ((short)Opcode.BRANCH.offset << 2) + 4;
		}
		TestInterpreterJump(PROGRAM_COUNTER,JumpToLocation,Opcode.BRANCH.rs,0);
	} else {
		NextInstruction = JUMP;
		if (inDelay) {
			JumpToLocation.UDW = JumpToLocation.UDW + 4;
		}
		else {
			JumpToLocation.UDW = PROGRAM_COUNTER.UDW + 8;
		}
	}
}

void _fastcall r4300i_REGIMM_BLTZAL (void) {
	BOOL inDelay = NextInstruction == JUMP;
	NextInstruction = DELAY_SLOT;
	if (GPR[Opcode.BRANCH.rs].DW < 0) {
		if (inDelay) {
			GPR[31].UDW = JumpToLocation.UDW + 4;
			PROGRAM_COUNTER.UDW = JumpToLocation.UDW - 4; // It will be incremented to JumpToLocation in main loop
			JumpToLocation.UDW = JumpToLocation.UDW + ((short)Opcode.BRANCH.offset << 2);
		}
		else {
			GPR[31].UDW = PROGRAM_COUNTER.UDW + 8;
			JumpToLocation.UDW = PROGRAM_COUNTER.UDW + ((short)Opcode.BRANCH.offset << 2) + 4;
		}
		TestInterpreterJump(PROGRAM_COUNTER,JumpToLocation,Opcode.BRANCH.rs,0);
	} else {
		if (inDelay) {
			GPR[31].UDW = JumpToLocation.UDW + 4;
			PROGRAM_COUNTER.UDW = JumpToLocation.UDW - 4; // It will be incremented to JumpToLocation in main loop
			JumpToLocation.UDW = JumpToLocation.UDW + 4;
		}
		else {
			GPR[31].UDW = PROGRAM_COUNTER.UDW + 8;
			JumpToLocation.UDW = PROGRAM_COUNTER.UDW + 8;
		}
	}
}

void _fastcall r4300i_REGIMM_BGEZAL (void) {
	BOOL inDelay = NextInstruction == JUMP;
	NextInstruction = DELAY_SLOT;
	if (GPR[Opcode.BRANCH.rs].DW >= 0) {
		if (inDelay) {
			GPR[31].UDW = JumpToLocation.UDW + 4;
			PROGRAM_COUNTER.UDW = JumpToLocation.UDW - 4; // It will be incremented to JumpToLocation in main loop
			JumpToLocation.UDW = JumpToLocation.UDW + ((short)Opcode.BRANCH.offset << 2);
		}
		else {
			GPR[31].UDW = PROGRAM_COUNTER.UDW + 8;
			JumpToLocation.UDW = PROGRAM_COUNTER.UDW + ((short)Opcode.BRANCH.offset << 2) + 4;
		}
		TestInterpreterJump(PROGRAM_COUNTER,JumpToLocation,Opcode.BRANCH.rs,0);
	} else {
		if (inDelay) {
			GPR[31].UDW = JumpToLocation.UDW + 4;
			PROGRAM_COUNTER.UDW = JumpToLocation.UDW - 4; // It will be incremented to JumpToLocation in main loop
			JumpToLocation.UDW = JumpToLocation.UDW + 4;
		}
		else {
			GPR[31].UDW = PROGRAM_COUNTER.UDW + 8;
			JumpToLocation.UDW = PROGRAM_COUNTER.UDW + 8;
		}
	}
}

void _fastcall r4300i_REGIMM_BLTZALL(void) {
	BOOL inDelay = NextInstruction == JUMP;
	if (GPR[Opcode.BRANCH.rs].DW >= 0) {
		NextInstruction = DELAY_SLOT;
		if (inDelay) {
			GPR[31].UDW = JumpToLocation.UDW + 4;
			PROGRAM_COUNTER.UDW = JumpToLocation.UDW - 4; // It will be incremented to JumpToLocation in main loop
			JumpToLocation.UDW = JumpToLocation.UDW + ((short)Opcode.BRANCH.offset << 2);
		}
		else {
			GPR[31].UDW = PROGRAM_COUNTER.UDW + 8;
			JumpToLocation.UDW = PROGRAM_COUNTER.UDW + ((short)Opcode.BRANCH.offset << 2) + 4;
		}
		TestInterpreterJump(PROGRAM_COUNTER, JumpToLocation, Opcode.BRANCH.rs, 0);
	}
	else {
		NextInstruction = JUMP;
		if (inDelay) {
			GPR[31].UDW = JumpToLocation.UDW + 4;
			JumpToLocation.UDW = JumpToLocation.UDW + 4;
		}
		else {
			GPR[31].UDW = PROGRAM_COUNTER.UDW + 8;
			JumpToLocation.UDW = PROGRAM_COUNTER.UDW + 8;
		}
	}
}

void _fastcall r4300i_REGIMM_BGEZALL (void) {
	BOOL inDelay = NextInstruction == JUMP;
	if (GPR[Opcode.BRANCH.rs].DW >= 0) {
		NextInstruction = DELAY_SLOT;
		if (inDelay) {
			GPR[31].UDW = JumpToLocation.UDW + 4;
			PROGRAM_COUNTER.UDW = JumpToLocation.UDW - 4; // It will be incremented to JumpToLocation in main loop
			JumpToLocation.UDW = JumpToLocation.UDW + ((short)Opcode.BRANCH.offset << 2);
		}
		else {
			GPR[31].UDW = PROGRAM_COUNTER.UDW + 8;
			JumpToLocation.UDW = PROGRAM_COUNTER.UDW + ((short)Opcode.BRANCH.offset << 2) + 4;
		}
		TestInterpreterJump(PROGRAM_COUNTER, JumpToLocation, Opcode.BRANCH.rs, 0);
	}
	else {
		NextInstruction = JUMP;
		if (inDelay) {
			GPR[31].UDW = JumpToLocation.UDW + 4;
			JumpToLocation.UDW = JumpToLocation.UDW + 4;
		}
		else {
			GPR[31].UDW = PROGRAM_COUNTER.UDW + 8;
			JumpToLocation.UDW = PROGRAM_COUNTER.UDW + 8;
		}
	}
}

/************************** COP0 functions **************************/
void _fastcall r4300i_COP0_MF (void) {
	if (HaveDebugger && LogOptions.LogCP0reads) {
		LogMessage("%016llX: R4300i Read from %s (0x%08X)", PROGRAM_COUNTER.UDW,
			Cop0_Name[Opcode.REG.rd], CP0[Opcode.REG.rd].W[0]);
	}
	GPR[Opcode.BRANCH.rt].DW = (int)CP0[Opcode.REG.rd].W[0];
}

void _fastcall r4300i_COP0_DMF(void) {
	if (HaveDebugger && LogOptions.LogCP0reads) {
		switch (Opcode.REG.rd) {
		case 4: //Context
		case 8: //BadVAddr
		case 10: //EntryHi
		case 14: //EPC
		case 20: //XContext:
		case 30: //ErrEPC
			LogMessage("%016llX: R4300i Read from %s (0x%016llX)", PROGRAM_COUNTER.UDW,
				Cop0_Name[Opcode.REG.rd], CP0[Opcode.REG.rd].UDW);
			break;
		default:
			LogMessage("%016llX: R4300i Read from %s (0x%08X)", PROGRAM_COUNTER.UDW,
				Cop0_Name[Opcode.REG.rd], CP0[Opcode.REG.rd].UW[0]);
		}
	}
	switch (Opcode.REG.rd) {
	case 4: //Context
	case 8: //BadVAddr
	case 10: //EntryHi
	case 14: //EPC
	case 20: //XContext
	case 30: //ErrEPC
		GPR[Opcode.BRANCH.rt].DW = CP0[Opcode.REG.rd].DW;
		break;
	case 17: //LLAddr
		GPR[Opcode.BRANCH.rt].UDW = CP0[Opcode.REG.rd].UW[0];
		break;
	default:
		GPR[Opcode.BRANCH.rt].DW = (int)CP0[Opcode.REG.rd].UW[0];
	}
}

void _fastcall r4300i_COP0_MT (void) {
	if (HaveDebugger && LogOptions.LogCP0changes) {
		LogMessage("%016llX: Writing 0x%X to %s register (Originally: 0x%08X)",PROGRAM_COUNTER.UDW,
			GPR[Opcode.BRANCH.rt].UW[0],Cop0_Name[Opcode.REG.rd], CP0[Opcode.REG.rd].UW[0]);
		if (Opcode.REG.rd == 11) { //Compare
			LogMessage("%016llX: Cause register changed from %08X to %08X",PROGRAM_COUNTER.UDW,
				CAUSE_REGISTER, (CAUSE_REGISTER & ~CAUSE_IP7));
		}
	}
	BOOL unusedRegister = FALSE;

	switch (Opcode.REG.rd) {
	case 17: //LLAddr
	case 18: //WatchLo
	case 19: //WatchHi
	case 28: //Tag lo
	case 29: //Tag Hi
		CP0[Opcode.REG.rd].UW[0] = GPR[Opcode.BRANCH.rt].UW[0];
		break;
	case 0: //Index
		CP0[Opcode.REG.rd].UW[0] = GPR[Opcode.BRANCH.rt].UW[0] & 0x8000003F;
		break;
	case 1: //Random
		break;
	case 2: //EntryLo0
	case 3: //EntryLo1
		CP0[Opcode.REG.rd].UW[0] = GPR[Opcode.BRANCH.rt].UW[0] & 0x3FFFFFFF;
		break;
	case 4: //Context
		CP0[Opcode.REG.rd].DW = (long)((CP0[Opcode.REG.rd].W[0] & 0x7FFFFF) | (GPR[Opcode.BRANCH.rt].W[0] & 0xFF800000));
		break;
	case 5: //PageMask
		CP0[Opcode.REG.rd].UW[0] = GPR[Opcode.BRANCH.rt].UW[0] & 0x1FFE000;
		break;
	case 6: //Wired
		CP0[Opcode.REG.rd].UW[0] = GPR[Opcode.BRANCH.rt].UW[0] & 0x3F;
		RANDOM_REGISTER = 31;
		break;
	case 8: //BadVAddr
		break;
	case 9: //Count
		CP0[Opcode.REG.rd].UW[0] = GPR[Opcode.BRANCH.rt].UW[0];
		ChangeCompareTimer();
		break;
	case 10: //Entry Hi
		CP0[Opcode.REG.rd].UDW = GPR[Opcode.BRANCH.rt].UW[0] & 0x0FFFFE0FF;
		break;
	case 11: //Compare
		CP0[Opcode.REG.rd].UW[0] = GPR[Opcode.BRANCH.rt].UW[0];
		CAUSE_REGISTER &= ~CAUSE_IP7;
		ChangeCompareTimer();
		break;		
	case 12: //Status
		if ((CP0[Opcode.REG.rd].UW[0] ^ GPR[Opcode.BRANCH.rt].UW[0]) != 0) {
			CP0[Opcode.REG.rd].UW[0] = GPR[Opcode.BRANCH.rt].UW[0] & 0XFFF7FFFF;
			SetFpuLocations();
		} else {
			CP0[Opcode.REG.rd].UW[0] = GPR[Opcode.BRANCH.rt].UW[0] & 0XFFF7FFFF;
		}
		if ((CP0[Opcode.REG.rd].UW[0] & STATUS_KSU) != STATUS_KERNEL &&
			(CP0[Opcode.REG.rd].UW[0] & STATUS_KSU) != STATUS_USER) {
			if (ShowDebugMessages)
				DisplayError("Left kernel mode or user mode ??");
		}
		UpdateCPUMode();
		CheckInterrupts();
		break;		
	case 13: //cause
		CP0[Opcode.REG.rd].UW[0] = (CP0[Opcode.REG.rd].UW[0] & 0xFFFFFCFF) | (GPR[Opcode.BRANCH.rt].UW[0] & 0x300);
		if ((GPR[Opcode.BRANCH.rt].UW[0] & 0x300) != 0) {
			if (ShowDebugMessages) { DisplayError("Set IP0 or IP1"); }
			LogMessage("Set IP0 or IP1");
		}
		break;
	case 14: //EPC
		CP0[Opcode.REG.rd].DW = GPR[Opcode.BRANCH.rt].W[0];
		break;
	case 15: //PRId
		break;
	case 16: //Config
		CP0[Opcode.REG.rd].UW[0] = (CP0[Opcode.REG.rd].UW[0] & 0x70066460) | (GPR[Opcode.BRANCH.rt].UW[0] & 0x0F00800F);
		break;
	case 20: //XContext
		CP0[Opcode.REG.rd].UDW = ((CP0[Opcode.REG.rd].UDW & 0x1FFFFFFFFLL) | (((long long)GPR[Opcode.BRANCH.rt].W[0]) & 0xFFFFFFFE00000000LL));
		break;
	case 26: //ECC
		CP0[Opcode.REG.rd].UW[0] = GPR[Opcode.BRANCH.rt].UW[0] & 0xFF;
		break;
	case 27: //CacheErr
		break;
	case 30: //ErrEPC
		CP0[Opcode.REG.rd].DW = GPR[Opcode.BRANCH.rt].W[0];
		break;

	//Unused registers
	case 7:
	case 21:
	case 22:
	case 23:
	case 24:
	case 25:
	case 31:
		CP0[Opcode.REG.rd].UDW = GPR[Opcode.BRANCH.rt].UW[0];
		unusedRegister = TRUE;
		break;

	default:
		R4300i_UnknownOpcode();
	}

	if (lastUnusedCOP0Register >= 0) {
		CP0[lastUnusedCOP0Register].UDW = GPR[Opcode.BRANCH.rt].UW[0];
	}
	if (unusedRegister) {
		lastUnusedCOP0Register = Opcode.REG.rd;
	}
	else {
		lastUnusedCOP0Register = -1;
	}
}

void _fastcall r4300i_COP0_DMT(void) {
	if (HaveDebugger && LogOptions.LogCP0changes) {
		switch (Opcode.REG.rd) {
		case 11: //Compare:
			LogMessage("%016llX: Cause register changed from %08X to %08X", PROGRAM_COUNTER.UDW,
				CAUSE_REGISTER, (CAUSE_REGISTER & ~CAUSE_IP7));
			break;
		case 4: //Context
		case 8: //BadVAddr
		case 10: //Entry Hi
		case 14: //EPC
		case 20: //XContext:
			LogMessage("%016llX: Writing 0x%llX to %s register (Originally: 0x%016llX)", PROGRAM_COUNTER.UDW,
				GPR[Opcode.BRANCH.rt].UDW, Cop0_Name[Opcode.REG.rd], CP0[Opcode.REG.rd].UDW);
			break;
		default:
			LogMessage("%016llX: Writing 0x%llX to %s register (Originally: 0x%08X)", PROGRAM_COUNTER.UDW,
				GPR[Opcode.BRANCH.rt].UDW, Cop0_Name[Opcode.REG.rd], CP0[Opcode.REG.rd].UW[0]);
		}
	}

	BOOL unusedRegister = FALSE;

	switch (Opcode.REG.rd) {
	case 17: //LLAddr
	case 18: //WatchLo
	case 19: //WatchHi
	case 28: //Tag lo
	case 29: //Tag Hi
		CP0[Opcode.REG.rd].UW[0] = GPR[Opcode.BRANCH.rt].UW[0];
		break;
	case 0: //Index
		CP0[Opcode.REG.rd].UW[0] = GPR[Opcode.BRANCH.rt].UW[0] & 0x8000003F;
		break;
	case 2: //EntryLo0
	case 3: //EntryLo1
		CP0[Opcode.REG.rd].UW[0] = GPR[Opcode.BRANCH.rt].UW[0] & 0x3FFFFFFF;
		break;
	case 1: //Random
		break;
	case 4: //Context
		CP0[Opcode.REG.rd].UDW = (CP0[Opcode.REG.rd].UDW & 0x7FFFFFLL) | (GPR[Opcode.BRANCH.rt].UDW & 0xFFFFFFFFFF800000LL);
		break;
	case 5: //PageMask
		CP0[Opcode.REG.rd].UW[0] = GPR[Opcode.BRANCH.rt].UW[0] & 0x1FFE000;
		break;
	case 6: //Wired
		CP0[Opcode.REG.rd].UW[0] = GPR[Opcode.BRANCH.rt].UW[0] & 0x3F;
		RANDOM_REGISTER = 31;
		break;
	case 8: //BadVAddr
		break;
	case 9: //Count
		CP0[Opcode.REG.rd].UW[0] = GPR[Opcode.BRANCH.rt].UW[0];
		ChangeCompareTimer();
		break;
	case 10: //Entry Hi
		CP0[Opcode.REG.rd].UDW = GPR[Opcode.BRANCH.rt].UDW & 0xC00000FFFFFFE0FFLL;
		break;
	case 11: //Compare
		CP0[Opcode.REG.rd].UW[0] = GPR[Opcode.BRANCH.rt].UW[0];
		CAUSE_REGISTER &= ~CAUSE_IP7;
		ChangeCompareTimer();
		break;
	case 12: //Status
		if ((CP0[Opcode.REG.rd].UW[0] ^ GPR[Opcode.BRANCH.rt].UW[0]) != 0) {
			CP0[Opcode.REG.rd].UW[0] = GPR[Opcode.BRANCH.rt].UW[0] & 0XFFF7FFFF;
			SetFpuLocations();
		}
		else {
			CP0[Opcode.REG.rd].UW[0] = GPR[Opcode.BRANCH.rt].UW[0] & 0xFFF7FFFF;
		}
		if ((CP0[Opcode.REG.rd].UW[0] & STATUS_KSU) != STATUS_KERNEL &&
			(CP0[Opcode.REG.rd].UW[0] & STATUS_KSU) != STATUS_USER) {
			if (ShowDebugMessages)
				DisplayError("Left kernel mode or user mode ??");
		}
		UpdateCPUMode();
		CheckInterrupts();
		break;
	case 13: //cause
		CP0[Opcode.REG.rd].UW[0] = (CP0[Opcode.REG.rd].UW[0] & 0xFFFFFCFF) | (GPR[Opcode.BRANCH.rt].UW[0] & 0x300);
		if ((GPR[Opcode.BRANCH.rt].UW[0] & 0x300) != 0) {
			if (ShowDebugMessages) { DisplayError("Set IP0 or IP1"); }
			LogMessage("Set IP0 or IP1");
		}
		break;
	case 14: //EPC
		CP0[Opcode.REG.rd].DW = GPR[Opcode.BRANCH.rt].DW;
		break;
	case 15: //PRId
		break;
	case 16: //Config
		CP0[Opcode.REG.rd].UW[0] = (CP0[Opcode.REG.rd].UW[0] & 0x70066460) | (GPR[Opcode.BRANCH.rt].UW[0] & 0x0F00800F);
		break;
	case 20: //XContext
		CP0[Opcode.REG.rd].UDW = (CP0[Opcode.REG.rd].UDW & 0x1FFFFFFFFLL) | (GPR[Opcode.BRANCH.rt].UDW & 0xFFFFFFFE00000000LL);
		break;
	case 26: //ECC
		CP0[Opcode.REG.rd].UW[0] = GPR[Opcode.BRANCH.rt].UW[0] & 0xFF;
		break;
	case 27: //CacheErr
		break;
	case 30: //ErrEPC
		CP0[Opcode.REG.rd].DW = GPR[Opcode.BRANCH.rt].DW;
		break;

	//Unused registers
	case 7:
	case 21:
	case 22:
	case 23:
	case 24:
	case 25:
	case 31:
		CP0[Opcode.REG.rd].UDW = GPR[Opcode.BRANCH.rt].UDW;
		unusedRegister = TRUE;
		break;

	default:
		R4300i_UnknownOpcode();
	}

	if (lastUnusedCOP0Register >= 0) {
		CP0[lastUnusedCOP0Register].UDW = GPR[Opcode.BRANCH.rt].UW[0];
	}
	if (unusedRegister) {
		lastUnusedCOP0Register = Opcode.REG.rd;
	}
	else {
		lastUnusedCOP0Register = -1;
	}
}

/************************** COP0 CO functions ***********************/
void _fastcall r4300i_COP0_CO_TLBR (void) {
	if (!UseTlb) { return; }
	TLB_Read();
}

void _fastcall r4300i_COP0_CO_TLBWI (void) {
	if (!UseTlb) { return; }
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
		JumpToLocation.UDW = ERROREPC_REGISTER;
		STATUS_REGISTER &= ~STATUS_ERL;
	} else {
		JumpToLocation.UDW = EPC_REGISTER;
		STATUS_REGISTER &= ~STATUS_EXL;
	}
	UpdateCPUMode();
	LLBit = 0;
	CheckInterrupts();
}

/************************** COP1 functions **************************/
void _fastcall r4300i_COP1_MF (void) {
	TEST_COP1_USABLE_EXCEPTION();
	GPR[Opcode.BRANCH.rt].DW = *(int *)FPRFloatLoadStoreLocation[Opcode.FP.fs];
}

void _fastcall r4300i_COP1_DMF (void) {
	TEST_COP1_USABLE_EXCEPTION();
	GPR[Opcode.BRANCH.rt].DW = *(__int64 *)FPRDoubleLocation[Opcode.FP.fs];
}

void _fastcall r4300i_COP1_CF (void) {
	TEST_COP1_USABLE_EXCEPTION();
	if (Opcode.FP.fs != 31 && Opcode.FP.fs != 0) {
		if (ShowDebugMessages)
			DisplayError("CFC1 what register are you writing to ?");
		return;
	}
	GPR[Opcode.BRANCH.rt].DW = (int)FPCR[Opcode.FP.fs];
}

void _fastcall r4300i_COP1_DCF (void) {
	TEST_COP1_USABLE_EXCEPTION();
	CLEAR_COP1_CAUSE();
	SET_COP1_CAUSE(CAUSE_UNIMPLEMENTED);
	TEST_COP1_FP_EXCEPTION();
}

void _fastcall r4300i_COP1_MT (void) {
	TEST_COP1_USABLE_EXCEPTION();
	*(int *)FPRFloatLoadStoreLocation[Opcode.FP.fs] = GPR[Opcode.BRANCH.rt].W[0];
}

void _fastcall r4300i_COP1_DMT (void) {
	TEST_COP1_USABLE_EXCEPTION();
	*(__int64 *)FPRDoubleLocation[Opcode.FP.fs] = GPR[Opcode.BRANCH.rt].DW;
}

void _fastcall r4300i_COP1_CT (void) {
	TEST_COP1_USABLE_EXCEPTION();
	if (Opcode.FP.fs == 31) {
		FPCR[Opcode.FP.fs] = GPR[Opcode.BRANCH.rt].W[0] & 0x183FFFF;
		switch((FPCR[Opcode.FP.fs] & 3)) {
		case 0: RoundingModel = _RC_NEAR; break;
		case 1: RoundingModel = _RC_CHOP; break;
		case 2: RoundingModel = _RC_UP; break;
		case 3: RoundingModel = _RC_DOWN; break;
		}
		TEST_COP1_FP_EXCEPTION();
		return;
	}
	if (ShowDebugMessages)
		DisplayError("CTC1 what register are you writing to ?");
}

void _fastcall r4300i_COP1_DCT(void) {
	TEST_COP1_USABLE_EXCEPTION();
	CLEAR_COP1_CAUSE();
	SET_COP1_CAUSE(CAUSE_UNIMPLEMENTED);
	TEST_COP1_FP_EXCEPTION();
}

/************************* COP1: BC1 functions ***********************/
void _fastcall r4300i_COP1_BCF (void) {
	TEST_COP1_USABLE_EXCEPTION();
	BOOL inDelay = NextInstruction == JUMP;
	NextInstruction = DELAY_SLOT;
	if ((FPCR[31] & FPCSR_C) == 0) {
		if (inDelay) {
			PROGRAM_COUNTER.UDW = JumpToLocation.UDW - 4; // It will be incremented to JumpToLocation in main loop
			JumpToLocation.UDW = JumpToLocation.UDW + ((short)Opcode.BRANCH.offset << 2);
		}
		else {
			JumpToLocation.UDW = PROGRAM_COUNTER.UDW + ((short)Opcode.BRANCH.offset << 2) + 4;
		}
	} else {
		if (inDelay) {
			JumpToLocation.UDW = JumpToLocation.UDW + 4;
		}
		else {
			JumpToLocation.UDW = PROGRAM_COUNTER.UDW + 8;
		}
	}
}

void _fastcall r4300i_COP1_BCT (void) {
	TEST_COP1_USABLE_EXCEPTION();
	BOOL inDelay = NextInstruction == JUMP;
	NextInstruction = DELAY_SLOT;
	if ((FPCR[31] & FPCSR_C) != 0) {
		if (inDelay) {
			PROGRAM_COUNTER.UDW = JumpToLocation.UDW - 4; // It will be incremented to JumpToLocation in main loop
			JumpToLocation.UDW = JumpToLocation.UDW + ((short)Opcode.BRANCH.offset << 2);
		}
		else {
			JumpToLocation.UDW = PROGRAM_COUNTER.UDW + ((short)Opcode.BRANCH.offset << 2) + 4;
		}
	} else {
		if (inDelay) {
			JumpToLocation.UDW = JumpToLocation.UDW + 4;
		}
		else {
			JumpToLocation.UDW = PROGRAM_COUNTER.UDW + 8;
		}
	}
}

void _fastcall r4300i_COP1_BCFL (void) {
	TEST_COP1_USABLE_EXCEPTION();
	BOOL inDelay = NextInstruction == JUMP;
	if ((FPCR[31] & FPCSR_C) == 0) {
		NextInstruction = DELAY_SLOT;
		if(inDelay) {
			PROGRAM_COUNTER.UDW = JumpToLocation.UDW - 4; // It will be incremented to JumpToLocation in main loop
			JumpToLocation.UDW = JumpToLocation.UDW + ((short)Opcode.BRANCH.offset << 2);
		}
		else {
			JumpToLocation.UDW = PROGRAM_COUNTER.UDW + ((short)Opcode.BRANCH.offset << 2) + 4;
		}
	} else {
		NextInstruction = JUMP;
		if (inDelay) {
			JumpToLocation.UDW = JumpToLocation.UDW + 4;
		}
		else {
			JumpToLocation.UDW = PROGRAM_COUNTER.UDW + 8;
		}
	}
}

void _fastcall r4300i_COP1_BCTL (void) {
	TEST_COP1_USABLE_EXCEPTION();
	BOOL inDelay = NextInstruction == JUMP;
	if ((FPCR[31] & FPCSR_C) != 0) {
		NextInstruction = DELAY_SLOT;
		if (inDelay) {
			PROGRAM_COUNTER.UDW = JumpToLocation.UDW - 4; // It will be incremented to JumpToLocation in main loop
			JumpToLocation.UDW = JumpToLocation.UDW + ((short)Opcode.BRANCH.offset << 2);
		}
		else {
			JumpToLocation.UDW = PROGRAM_COUNTER.UDW + ((short)Opcode.BRANCH.offset << 2) + 4;
		}
	} else {
		NextInstruction = JUMP;
		if (inDelay) {
			JumpToLocation.UDW = JumpToLocation.UDW + 4;
		}
		else {
			JumpToLocation.UDW = PROGRAM_COUNTER.UDW + 8;
		}
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

__inline void Float_sqrt_fixed(float* Dest, float* Source) {
	_asm {
		mov esi, [Source]
		mov edi, [Dest]
		fld dword ptr[esi]
		fsqrt
		fstp dword ptr[edi]
	}
}

__inline DWORD getCop1SArgCause(DWORD* v) {
	DWORD cause = 0;
	if (IsSubNormal_S(*v)) {
		cause |= CAUSE_UNIMPLEMENTED;
	}
	else if (IsQNAN_S(*v)) {
		cause |= CAUSE_INVALID;
	}
	else if (IsNAN_S(*v)) {
		cause |= CAUSE_UNIMPLEMENTED;
	}
	return cause;
}

__inline DWORD getCop1SToWArgCause(DWORD* v) {
	if (*(float*)v >= 2147483648.0 ||
		*(float*)v < -2147483648.0) {
		return CAUSE_UNIMPLEMENTED;
	}
	else if (IsQNAN_S(*v) || IsNAN_S(*v) || IsSubNormal_S(*v)) {
		return CAUSE_UNIMPLEMENTED;
	}
	return 0;
}

__inline DWORD getCop1SToLArgCause(DWORD* v) {
	if (*(float*)v >= 9007199254740992.0 || // this 1 << 53 in floating point format
		*(float*)v <=-9007199254740992.0) {
		return CAUSE_UNIMPLEMENTED;
	}
	else if (IsQNAN_S(*v) || IsNAN_S(*v) || IsSubNormal_S(*v)) {
		return CAUSE_UNIMPLEMENTED;
	}
	return 0;
}

__inline DWORD getCop1SCause(float* res) {
	DWORD status = _statusfp();
	DWORD cause = 0;
	int underflow = 0;
	if (status) {
		if (status & _EM_INEXACT) {
			cause |= CAUSE_INEXACT;
		}
		if (status & _EM_OVERFLOW) {
			cause |= CAUSE_OVERFLOW;
		}
		if (status & _EM_UNDERFLOW) {
			underflow = 1;
		}
		if (status & _EM_ZERODIVIDE) {
			cause |= CAUSE_DIVBYZERO;
		}
		if (status & _EM_INVALID) {
			cause |= CAUSE_INVALID;
			*(DWORD*)res = NAN_S;
		}
	}
	if (IsNAN_S(*(DWORD*)res)) {
		*(DWORD*)res = NAN_S;
	}
	
	if (IsSubNormal_S(*(DWORD*)res) || underflow) {
		if ((FPCR[31] & FPCSR_FS) == 0 || (FPCR[31] & FPCSR_EU) || (FPCR[31] & FPCSR_EI)) {
			cause |= CAUSE_UNIMPLEMENTED;
		}
		else {
			cause |= CAUSE_UNDERFLOW | CAUSE_INEXACT;

			switch (FPCR[31] & FPCSR_RM_MASK) {
			case FPCSR_RM_RN:
			case FPCSR_RM_RZ:
				*(DWORD*)res &= 0x80000000; // only keep the sign
				break;

			case FPCSR_RM_RP:
				if ((*(DWORD*)res & 0x80000000) != 0) {
					*(DWORD*)res = 0x80000000; // -0.0
				}
				else {
					*(DWORD*)res = 0x00800000; // min float
				}
				break;

			case FPCSR_RM_RM:
				if ((*(DWORD*)res & 0x80000000) != 0) {
					*(DWORD*)res = 0x80800000; // -min float
				}
				else {
					*(DWORD*)res = 0; // 0.0
				}
				break;
			}
		}
	}
	return cause;
}

__inline DWORD getCop1ConvertCause() {
	DWORD status = _statusfp();
	DWORD cause = 0;
	if (status) {
		if (status & _EM_INEXACT) {
			cause |= CAUSE_INEXACT;
		}
	}
	return cause;
}

void _fastcall r4300i_COP1_S_ADD (void) {
	TEST_COP1_USABLE_EXCEPTION();
	CLEAR_COP1_CAUSE();
	_controlfp(RoundingModel,_MCW_RC);
	_clearfp();
	DWORD cause = getCop1SArgCause((DWORD*)FPRFloatFSLocation[Opcode.FP.fs]);
	cause |= getCop1SArgCause((DWORD*)FPRFloatOtherLocation[Opcode.FP.ft]);
	SET_COP1_CAUSE(cause);
	TEST_COP1_FP_EXCEPTION();
	float result = (*(float *)FPRFloatFSLocation[Opcode.FP.fs] + *(float *)FPRFloatOtherLocation[Opcode.FP.ft]);
	cause |= getCop1SCause(&result);
	SET_COP1_CAUSE(cause);
	TEST_COP1_FP_EXCEPTION();
	SET_COP1_FLAGS(cause);
	*(float*)FPRFloatOtherLocation[Opcode.FP.fd] = result;
	*(long *)FPRFloatUpperHalfLocation[Opcode.FP.fd] = 0;
}

void _fastcall r4300i_COP1_S_SUB (void) {
	TEST_COP1_USABLE_EXCEPTION();
	CLEAR_COP1_CAUSE();
	_controlfp(RoundingModel,_MCW_RC);
	_clearfp();
	DWORD cause = getCop1SArgCause((DWORD*)FPRFloatFSLocation[Opcode.FP.fs]);
	cause |= getCop1SArgCause((DWORD*)FPRFloatOtherLocation[Opcode.FP.ft]);
	SET_COP1_CAUSE(cause);
	TEST_COP1_FP_EXCEPTION();
	float result = (*(float *)FPRFloatFSLocation[Opcode.FP.fs] - *(float *)FPRFloatOtherLocation[Opcode.FP.ft]);
	cause |= getCop1SCause(&result);
	SET_COP1_CAUSE(cause);
	TEST_COP1_FP_EXCEPTION();
	SET_COP1_FLAGS(cause);
	*(float*)FPRFloatOtherLocation[Opcode.FP.fd] = result;
	*(long*)FPRFloatUpperHalfLocation[Opcode.FP.fd] = 0;
}

void _fastcall r4300i_COP1_S_MUL (void) {
	TEST_COP1_USABLE_EXCEPTION();
	CLEAR_COP1_CAUSE();
	_controlfp(RoundingModel,_MCW_RC);
	_clearfp();
	DWORD cause = getCop1SArgCause((DWORD*)FPRFloatFSLocation[Opcode.FP.fs]);
	cause |= getCop1SArgCause((DWORD*)FPRFloatOtherLocation[Opcode.FP.ft]);
	SET_COP1_CAUSE(cause);
	TEST_COP1_FP_EXCEPTION();
	float result = (*(float*)FPRFloatFSLocation[Opcode.FP.fs] * *(float*)FPRFloatOtherLocation[Opcode.FP.ft]);
	cause |= getCop1SCause(&result);
	SET_COP1_CAUSE(cause);
	TEST_COP1_FP_EXCEPTION();
	SET_COP1_FLAGS(cause);
	*(float*)FPRFloatOtherLocation[Opcode.FP.fd] = result;
	*(long*)FPRFloatUpperHalfLocation[Opcode.FP.fd] = 0;
}

void _fastcall r4300i_COP1_S_DIV (void) {
	TEST_COP1_USABLE_EXCEPTION();
	CLEAR_COP1_CAUSE();
	_controlfp(RoundingModel,_MCW_RC);
	_clearfp();
	DWORD cause = getCop1SArgCause((DWORD*)FPRFloatFSLocation[Opcode.FP.fs]);
	cause |= getCop1SArgCause((DWORD*)FPRFloatOtherLocation[Opcode.FP.ft]);
	SET_COP1_CAUSE(cause);
	TEST_COP1_FP_EXCEPTION();
	float result = (*(float *)FPRFloatFSLocation[Opcode.FP.fs] / *(float *)FPRFloatOtherLocation[Opcode.FP.ft]);
	cause |= getCop1SCause(&result);
	SET_COP1_CAUSE(cause);
	TEST_COP1_FP_EXCEPTION();
	SET_COP1_FLAGS(cause);
	*(float*)FPRFloatOtherLocation[Opcode.FP.fd] = result;
	*(long*)FPRFloatUpperHalfLocation[Opcode.FP.fd] = 0;
}

void _fastcall r4300i_COP1_S_SQRT (void) {
	TEST_COP1_USABLE_EXCEPTION();
	CLEAR_COP1_CAUSE();
	_controlfp(RoundingModel,_MCW_RC);
	_clearfp();
	DWORD cause = getCop1SArgCause((DWORD*)FPRFloatFSLocation[Opcode.FP.fs]);
	SET_COP1_CAUSE(cause);
	TEST_COP1_FP_EXCEPTION();
	float result;
	Float_sqrt_fixed(&result, (float*)FPRFloatFSLocation[Opcode.FP.fs]);
	cause |= getCop1SCause(&result);
	SET_COP1_CAUSE(cause);
	TEST_COP1_FP_EXCEPTION();
	SET_COP1_FLAGS(cause);
	*(float*)FPRFloatOtherLocation[Opcode.FP.fd] = result;
	*(long*)FPRFloatUpperHalfLocation[Opcode.FP.fd] = 0;
}

void _fastcall r4300i_COP1_S_ABS (void) {
	TEST_COP1_USABLE_EXCEPTION();
	CLEAR_COP1_CAUSE();
	_controlfp(RoundingModel,_MCW_RC);
	_clearfp();
	DWORD cause = getCop1SArgCause((DWORD*)FPRFloatFSLocation[Opcode.FP.fs]);
	SET_COP1_CAUSE(cause);
	TEST_COP1_FP_EXCEPTION();
	float result = (float)fabs(*(float *)FPRFloatFSLocation[Opcode.FP.fs]);
	getCop1SCause(&result);
	SET_COP1_FLAGS(cause);
	*(float*)FPRFloatOtherLocation[Opcode.FP.fd] = result;
	*(long*)FPRFloatUpperHalfLocation[Opcode.FP.fd] = 0;
}

void _fastcall r4300i_COP1_S_MOV (void) {
	TEST_COP1_USABLE_EXCEPTION();
	_controlfp(RoundingModel,_MCW_RC);
	*(float *)FPRFloatOtherLocation[Opcode.FP.fd] = *(float *)FPRFloatFSLocation[Opcode.FP.fs];
	*(long*)FPRFloatUpperHalfLocation[Opcode.FP.fd] = *((long long*)FPRDoubleLocation[Opcode.FP.fs]) >> 32;
}

void _fastcall r4300i_COP1_S_NEG (void) {
	TEST_COP1_USABLE_EXCEPTION();
	CLEAR_COP1_CAUSE();
	_controlfp(RoundingModel, _MCW_RC);
	_clearfp();
	DWORD cause = getCop1SArgCause((DWORD*)FPRFloatFSLocation[Opcode.FP.fs]);
	SET_COP1_CAUSE(cause);
	TEST_COP1_FP_EXCEPTION();
	float result = (*(float *)FPRFloatFSLocation[Opcode.FP.fs] * -1.0f);
	getCop1SCause(&result);
	SET_COP1_FLAGS(cause);
	*(float*)FPRFloatOtherLocation[Opcode.FP.fd] = result;
	*(long*)FPRFloatUpperHalfLocation[Opcode.FP.fd] = 0;
}

void __fastcall r4300i_COP1_S_ROUND_L(void) {
	TEST_COP1_USABLE_EXCEPTION();
	CLEAR_COP1_CAUSE();
	_controlfp(_RC_NEAR,_MCW_RC);
	DWORD cause = getCop1SToLArgCause((DWORD*)FPRFloatFSLocation[Opcode.FP.fs]);
	SET_COP1_CAUSE(cause);
	TEST_COP1_FP_EXCEPTION();
	_clearfp();
	__int64 result;
	Float_RoundToInteger64(&result, &* (float*)FPRFloatFSLocation[Opcode.FP.fs]);
	cause |= getCop1ConvertCause();
	SET_COP1_CAUSE(cause);
	TEST_COP1_FP_EXCEPTION();
	SET_COP1_FLAGS(cause);
	*(__int64*)FPRDoubleFTFDLocation[Opcode.FP.fd] = result;
}

void _fastcall r4300i_COP1_S_TRUNC_L (void) {
	TEST_COP1_USABLE_EXCEPTION();
	CLEAR_COP1_CAUSE();
	_controlfp(_RC_CHOP,_MCW_RC);
	DWORD cause = getCop1SToLArgCause((DWORD*)FPRFloatFSLocation[Opcode.FP.fs]);
	SET_COP1_CAUSE(cause);
	TEST_COP1_FP_EXCEPTION();
	_clearfp();
	__int64 result;
	Float_RoundToInteger64(&result,&*(float *)FPRFloatFSLocation[Opcode.FP.fs]);
	cause |= getCop1ConvertCause();
	SET_COP1_CAUSE(cause);
	TEST_COP1_FP_EXCEPTION();
	SET_COP1_FLAGS(cause);
	*(__int64*)FPRDoubleFTFDLocation[Opcode.FP.fd] = result;
}

void _fastcall r4300i_COP1_S_CEIL_L (void) {	//added by Witten
	TEST_COP1_USABLE_EXCEPTION();
	CLEAR_COP1_CAUSE();
	_controlfp(_RC_UP,_MCW_RC);
	DWORD cause = getCop1SToLArgCause((DWORD*)FPRFloatFSLocation[Opcode.FP.fs]);
	SET_COP1_CAUSE(cause);
	TEST_COP1_FP_EXCEPTION();
	_clearfp();
	__int64 result;
	Float_RoundToInteger64(&result,&*(float *)FPRFloatFSLocation[Opcode.FP.fs]);
	cause |= getCop1ConvertCause();
	SET_COP1_CAUSE(cause);
	TEST_COP1_FP_EXCEPTION();
	SET_COP1_FLAGS(cause);
	*(__int64*)FPRDoubleFTFDLocation[Opcode.FP.fd] = result;
}

void _fastcall r4300i_COP1_S_FLOOR_L (void) {	//added by Witten
	TEST_COP1_USABLE_EXCEPTION();
	CLEAR_COP1_CAUSE();
	_controlfp(_RC_DOWN,_MCW_RC);
	DWORD cause = getCop1SToLArgCause((DWORD*)FPRFloatFSLocation[Opcode.FP.fs]);
	SET_COP1_CAUSE(cause);
	TEST_COP1_FP_EXCEPTION();
	_clearfp();
	__int64 result;
	Float_RoundToInteger64(&result,&*(float *)FPRFloatFSLocation[Opcode.FP.fs]);
	cause |= getCop1ConvertCause();
	SET_COP1_CAUSE(cause);
	TEST_COP1_FP_EXCEPTION();
	SET_COP1_FLAGS(cause);
	*(__int64*)FPRDoubleFTFDLocation[Opcode.FP.fd] = result;
}

void _fastcall r4300i_COP1_S_ROUND_W (void) {
	TEST_COP1_USABLE_EXCEPTION();
	CLEAR_COP1_CAUSE();
	_controlfp(_RC_NEAR,_MCW_RC);
	DWORD cause = getCop1SToWArgCause((DWORD*)FPRFloatFSLocation[Opcode.FP.fs]);
	SET_COP1_CAUSE(cause);
	TEST_COP1_FP_EXCEPTION();
	_clearfp();
	int result;
	Float_RoundToInteger32(&result,&*(float *)FPRFloatFSLocation[Opcode.FP.fs]);
	cause |= getCop1ConvertCause();
	SET_COP1_CAUSE(cause);
	TEST_COP1_FP_EXCEPTION();
	SET_COP1_FLAGS(cause);
	*(int*)FPRFloatOtherLocation[Opcode.FP.fd] = result;
	*(long*)FPRFloatUpperHalfLocation[Opcode.FP.fd] = 0;
}

void _fastcall r4300i_COP1_S_TRUNC_W (void) {
	TEST_COP1_USABLE_EXCEPTION();
	CLEAR_COP1_CAUSE();
	_controlfp(_RC_CHOP,_MCW_RC);
	DWORD cause = getCop1SToWArgCause((DWORD*)FPRFloatFSLocation[Opcode.FP.fs]);
	SET_COP1_CAUSE(cause);
	TEST_COP1_FP_EXCEPTION();
	_clearfp();
	int result;
	Float_RoundToInteger32(&result,&*(float *)FPRFloatFSLocation[Opcode.FP.fs]);
	cause |= getCop1ConvertCause();
	SET_COP1_CAUSE(cause);
	TEST_COP1_FP_EXCEPTION();
	SET_COP1_FLAGS(cause);
	*(int*)FPRFloatOtherLocation[Opcode.FP.fd] = result;
	*(long*)FPRFloatUpperHalfLocation[Opcode.FP.fd] = 0;
}

void _fastcall r4300i_COP1_S_CEIL_W (void) {	//added by Witten
	TEST_COP1_USABLE_EXCEPTION();
	CLEAR_COP1_CAUSE();
	_controlfp(_RC_UP,_MCW_RC);
	DWORD cause = getCop1SToWArgCause((DWORD*)FPRFloatFSLocation[Opcode.FP.fs]);
	SET_COP1_CAUSE(cause);
	TEST_COP1_FP_EXCEPTION();
	_clearfp();
	int result;
	Float_RoundToInteger32(&result,&*(float *)FPRFloatFSLocation[Opcode.FP.fs]);
	cause |= getCop1ConvertCause();
	SET_COP1_CAUSE(cause);
	TEST_COP1_FP_EXCEPTION();
	SET_COP1_FLAGS(cause);
	*(int*)FPRFloatOtherLocation[Opcode.FP.fd] = result;
	*(long*)FPRFloatUpperHalfLocation[Opcode.FP.fd] = 0;
}

void _fastcall r4300i_COP1_S_FLOOR_W (void) {
	TEST_COP1_USABLE_EXCEPTION();
	CLEAR_COP1_CAUSE();
	_controlfp(_RC_DOWN,_MCW_RC);
	DWORD cause = getCop1SToWArgCause((DWORD*)FPRFloatFSLocation[Opcode.FP.fs]);
	SET_COP1_CAUSE(cause);
	TEST_COP1_FP_EXCEPTION();
	_clearfp();
	int result;
	Float_RoundToInteger32(&result,&*(float *)FPRFloatFSLocation[Opcode.FP.fs]);
	cause |= getCop1ConvertCause();
	SET_COP1_CAUSE(cause);
	TEST_COP1_FP_EXCEPTION();
	SET_COP1_FLAGS(cause);
	*(int*)FPRFloatOtherLocation[Opcode.FP.fd] = result;
	*(long*)FPRFloatUpperHalfLocation[Opcode.FP.fd] = 0;
}

void _fastcall r4300i_COP1_S_CVT_S (void) {
	TEST_COP1_USABLE_EXCEPTION();
	CLEAR_COP1_CAUSE();
	DWORD cause = CAUSE_UNIMPLEMENTED;
	SET_COP1_CAUSE(cause);
	TEST_COP1_FP_EXCEPTION();
}

void _fastcall r4300i_COP1_S_CVT_D (void) {
	TEST_COP1_USABLE_EXCEPTION();
	CLEAR_COP1_CAUSE();
	_controlfp(RoundingModel,_MCW_RC);
	_clearfp();
	DWORD cause = getCop1SArgCause((DWORD*)FPRFloatFSLocation[Opcode.FP.fs]);
	SET_COP1_CAUSE(cause);
	TEST_COP1_FP_EXCEPTION();
	double result = (double)(*(float *)FPRFloatFSLocation[Opcode.FP.fs]);
	cause |= getCop1DCause(&result);
	SET_COP1_CAUSE(cause);
	TEST_COP1_FP_EXCEPTION();
	SET_COP1_FLAGS(cause);
	*(double*)FPRDoubleFTFDLocation[Opcode.FP.fd] = result;
}

void _fastcall r4300i_COP1_S_CVT_W (void) {
	TEST_COP1_USABLE_EXCEPTION();
	CLEAR_COP1_CAUSE();
	_controlfp(RoundingModel,_MCW_RC);
	DWORD cause = getCop1SToWArgCause((DWORD*)FPRFloatFSLocation[Opcode.FP.fs]);
	SET_COP1_CAUSE(cause);
	TEST_COP1_FP_EXCEPTION();
	_clearfp();
	int result;
	Float_RoundToInteger32(&result,&*(float *)FPRFloatFSLocation[Opcode.FP.fs]);
	cause |= getCop1ConvertCause();
	SET_COP1_CAUSE(cause);
	TEST_COP1_FP_EXCEPTION();
	SET_COP1_FLAGS(cause);
	*(int*)FPRFloatOtherLocation[Opcode.FP.fd] = result;
	*(long*)FPRFloatUpperHalfLocation[Opcode.FP.fd] = 0;
}

void _fastcall r4300i_COP1_S_CVT_L (void) {
	TEST_COP1_USABLE_EXCEPTION();
	CLEAR_COP1_CAUSE();
	_controlfp(RoundingModel,_MCW_RC);
	DWORD cause = getCop1SToLArgCause((DWORD*)FPRFloatFSLocation[Opcode.FP.fs]);
	SET_COP1_CAUSE(cause);
	TEST_COP1_FP_EXCEPTION();
	_clearfp();
	__int64 result;
	Float_RoundToInteger64(&result,&*(float *)FPRFloatFSLocation[Opcode.FP.fs]);
	cause |= getCop1ConvertCause();
	SET_COP1_CAUSE(cause);
	TEST_COP1_FP_EXCEPTION();
	SET_COP1_FLAGS(cause);
	*(__int64*)FPRDoubleFTFDLocation[Opcode.FP.fd] = result;
}

void _fastcall r4300i_COP1_S_CMP (void) {
	int less, equal, unorded, condition;
	float Temp0, Temp1;

	TEST_COP1_USABLE_EXCEPTION();
	CLEAR_COP1_CAUSE();

	Temp0 = *(float *)FPRFloatFSLocation[Opcode.FP.fs];
	Temp1 = *(float *)FPRFloatOtherLocation[Opcode.FP.ft];

	if (IsQNAN_S(*(DWORD*)&Temp0) || IsQNAN_S(*(DWORD*)&Temp1)) {
		less = FALSE;
		equal = FALSE;
		unorded = TRUE;
		SET_COP1_CAUSE(CAUSE_INVALID);
		TEST_COP1_FP_EXCEPTION();
		SET_COP1_FLAGS(CAUSE_INVALID);
	} else if (_isnan(Temp0) || _isnan(Temp1)) {
		if (ShowDebugMessages)
			DisplayError("Not a number?");
		less = FALSE;
		equal = FALSE;
		unorded = TRUE;
		if ((Opcode.REG.funct & 8) != 0) {
			if (ShowDebugMessages)
				DisplayError("Signal InvalidOperationException\nin r4300i_COP1_S_CMP\n%X  %ff\n%X  %ff", Temp0, Temp0, Temp1, Temp1);
			SET_COP1_CAUSE(CAUSE_INVALID);
			TEST_COP1_FP_EXCEPTION();
			SET_COP1_FLAGS(CAUSE_INVALID);
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

__inline void Double_sqrt_fixed(double* Dest, double* Source) {
	_asm {
		mov esi, [Source]
		mov edi, [Dest]
		fld qword ptr [esi]
		fsqrt
		fstp qword ptr [edi]
	}
}

__inline DWORD getCop1DArgCause(QWORD* v) {
	DWORD cause = 0;
	if (IsSubNormal_D(*v)) {
		cause |= CAUSE_UNIMPLEMENTED;
	}
	else if (IsQNAN_D(*v)) {
		cause |= CAUSE_INVALID;
	}
	else if (IsNAN_D(*v)) {
		cause |= CAUSE_UNIMPLEMENTED;
	}
	return cause;
}

__inline DWORD getCop1DToWArgCause(QWORD* v) {
	double rv = rint(*(double*)v);
	if (rv >= 2147483648.0 ||
		rv < -2147483648.0) {
		return CAUSE_UNIMPLEMENTED;
	}
	else if (IsQNAN_D(*v) || IsNAN_D(*v) || IsSubNormal_D(*v)) {
		return CAUSE_UNIMPLEMENTED;
	}
	return 0;
}

__inline DWORD getCop1DToLArgCause(QWORD* v) {
	if (*(double*)v >= 9007199254740992.0 || // this 1 << 53 in floating point format
		*(double*)v <= -9007199254740992.0) {
		return CAUSE_UNIMPLEMENTED;
	}
	else if (IsQNAN_D(*v) || IsNAN_D(*v) || IsSubNormal_D(*v)) {
		return CAUSE_UNIMPLEMENTED;
	}
	return 0;
}

__inline DWORD getCop1DCause(double* res) {
	DWORD status = _statusfp();
	DWORD cause = 0;
	int underflow = 0;
	if (status) {
		if (status & _EM_INEXACT) {
			cause |= CAUSE_INEXACT;
		}
		if (status & _EM_OVERFLOW) {
			cause |= CAUSE_OVERFLOW;
		}
		if (status & _EM_UNDERFLOW) {
			underflow = 1;
		}
		if (status & _EM_ZERODIVIDE) {
			cause |= CAUSE_DIVBYZERO;
		}
		if (status & _EM_INVALID) {
			cause |= CAUSE_INVALID;
		}
	}
	if (IsNAN_D(*(QWORD*)res)) {
		*(QWORD*)res = NAN_D;
	}
	if (IsSubNormal_D(*(QWORD*)res) || underflow) {
		if ((FPCR[31] & FPCSR_FS) == 0 || (FPCR[31] & FPCSR_EU) || (FPCR[31] & FPCSR_EI)) {
			cause |= CAUSE_UNIMPLEMENTED;
		}
		else {
			cause |= CAUSE_UNDERFLOW | CAUSE_INEXACT;

			switch (FPCR[31] & FPCSR_RM_MASK) {
			case FPCSR_RM_RN:
			case FPCSR_RM_RZ:
				*(QWORD*)res &= 0x8000000000000000LL; // only keep the sign
				break;

			case FPCSR_RM_RP:
				if ((*(QWORD*)res & 0x8000000000000000LL) != 0LL) {
					*(QWORD*)res = 0x8000000000000000LL; // -0.0
				}
				else {
					*(QWORD*)res = 0x0010000000000000LL; // min double
				}
				break;

			case FPCSR_RM_RM:
				if ((*(QWORD*)res & 0x8000000000000000LL) != 0LL) {
					*(QWORD*)res = 0x8010000000000000LL; // -min double
				}
				else {
					*(QWORD*)res = 0; // 0.0
				}
				break;
			}
		}
	}
	return cause;
}

void _fastcall r4300i_COP1_D_ADD (void) {
	TEST_COP1_USABLE_EXCEPTION();
	CLEAR_COP1_CAUSE();
	_clearfp();
	DWORD cause = getCop1DArgCause((QWORD*)FPRDoubleLocation[Opcode.FP.fs]);
	cause |= getCop1DArgCause((QWORD*)FPRDoubleFTFDLocation[Opcode.FP.ft]);
	SET_COP1_CAUSE(cause);
	TEST_COP1_FP_EXCEPTION();
	double result = *(double *)FPRDoubleLocation[Opcode.FP.fs] + *(double *)FPRDoubleFTFDLocation[Opcode.FP.ft];
	cause |= getCop1DCause(&result);
	SET_COP1_CAUSE(cause);
	TEST_COP1_FP_EXCEPTION();
	SET_COP1_FLAGS(cause);
	*(double*)FPRDoubleFTFDLocation[Opcode.FP.fd] = result;
}

void _fastcall r4300i_COP1_D_SUB (void) {
	TEST_COP1_USABLE_EXCEPTION();
	CLEAR_COP1_CAUSE();
	_clearfp();
	DWORD cause = getCop1DArgCause((QWORD*)FPRDoubleLocation[Opcode.FP.fs]);
	cause |= getCop1DArgCause((QWORD*)FPRDoubleFTFDLocation[Opcode.FP.ft]);
	SET_COP1_CAUSE(cause);
	TEST_COP1_FP_EXCEPTION();
	double result = *(double *)FPRDoubleLocation[Opcode.FP.fs] - *(double *)FPRDoubleFTFDLocation[Opcode.FP.ft];
	cause |= getCop1DCause(&result);
	SET_COP1_CAUSE(cause);
	TEST_COP1_FP_EXCEPTION();
	SET_COP1_FLAGS(cause);
	*(double*)FPRDoubleFTFDLocation[Opcode.FP.fd] = result;
}

void _fastcall r4300i_COP1_D_MUL (void) {
	TEST_COP1_USABLE_EXCEPTION();
	CLEAR_COP1_CAUSE();
	_clearfp();
	DWORD cause = getCop1DArgCause((QWORD*)FPRDoubleLocation[Opcode.FP.fs]);
	cause |= getCop1DArgCause((QWORD*)FPRDoubleFTFDLocation[Opcode.FP.ft]);
	SET_COP1_CAUSE(cause);
	TEST_COP1_FP_EXCEPTION();
	double result = *(double *)FPRDoubleLocation[Opcode.FP.fs] * *(double *)FPRDoubleFTFDLocation[Opcode.FP.ft];
	cause |= getCop1DCause(&result);
	SET_COP1_CAUSE(cause);
	TEST_COP1_FP_EXCEPTION();
	SET_COP1_FLAGS(cause);
	*(double*)FPRDoubleFTFDLocation[Opcode.FP.fd] = result;
}

void _fastcall r4300i_COP1_D_DIV (void) {
	TEST_COP1_USABLE_EXCEPTION();
	CLEAR_COP1_CAUSE();
	_clearfp();
	DWORD cause = getCop1DArgCause((QWORD*)FPRDoubleLocation[Opcode.FP.fs]);
	cause |= getCop1DArgCause((QWORD*)FPRDoubleFTFDLocation[Opcode.FP.ft]);
	SET_COP1_CAUSE(cause);
	TEST_COP1_FP_EXCEPTION();
	double result = *(double *)FPRDoubleLocation[Opcode.FP.fs] / *(double *)FPRDoubleFTFDLocation[Opcode.FP.ft];
	cause |= getCop1DCause(&result);
	SET_COP1_CAUSE(cause);
	TEST_COP1_FP_EXCEPTION();
	SET_COP1_FLAGS(cause);
	*(double*)FPRDoubleFTFDLocation[Opcode.FP.fd] = result;
}

void _fastcall r4300i_COP1_D_SQRT (void) {
	TEST_COP1_USABLE_EXCEPTION();
	CLEAR_COP1_CAUSE();
	_clearfp();
	DWORD cause = getCop1DArgCause((QWORD*)FPRDoubleLocation[Opcode.FP.fs]);
	SET_COP1_CAUSE(cause);
	TEST_COP1_FP_EXCEPTION();
	double result;
	Double_sqrt_fixed(&result, (double*)FPRDoubleLocation[Opcode.FP.fs]);
	cause |= getCop1DCause(&result);
	SET_COP1_CAUSE(cause);
	TEST_COP1_FP_EXCEPTION();
	SET_COP1_FLAGS(cause);
	*(double*)FPRDoubleFTFDLocation[Opcode.FP.fd] = result;
}

void _fastcall r4300i_COP1_D_ABS (void) {
	TEST_COP1_USABLE_EXCEPTION();
	CLEAR_COP1_CAUSE();
	_clearfp();
	DWORD cause = getCop1DArgCause((QWORD*)FPRDoubleLocation[Opcode.FP.fs]);
	SET_COP1_CAUSE(cause);
	TEST_COP1_FP_EXCEPTION();
	double result = fabs(*(double *)FPRDoubleLocation[Opcode.FP.fs]);
	getCop1DCause(&result);
	SET_COP1_FLAGS(cause);
	*(double*)FPRDoubleFTFDLocation[Opcode.FP.fd] = result;
}

void _fastcall r4300i_COP1_D_MOV (void) {
	TEST_COP1_USABLE_EXCEPTION();
	*(__int64 *)FPRDoubleFTFDLocation[Opcode.FP.fd] = *(__int64 *)FPRDoubleLocation[Opcode.FP.fs];
}

void _fastcall r4300i_COP1_D_NEG (void) {
	TEST_COP1_USABLE_EXCEPTION();
	CLEAR_COP1_CAUSE();
	_clearfp();
	DWORD cause = getCop1DArgCause((QWORD*)FPRDoubleLocation[Opcode.FP.fs]);
	SET_COP1_CAUSE(cause);
	TEST_COP1_FP_EXCEPTION();
	double result = (*(double *)FPRDoubleLocation[Opcode.FP.fs] * -1.0);
	getCop1DCause(&result);
	SET_COP1_FLAGS(cause);
	*(double*)FPRDoubleFTFDLocation[Opcode.FP.fd] = result;
}

void _fastcall r4300i_COP1_D_ROUND_L(void) {
	TEST_COP1_USABLE_EXCEPTION();
	CLEAR_COP1_CAUSE();
	_controlfp(RC_NEAR, _MCW_RC);
	DWORD cause = getCop1DToLArgCause((QWORD*)FPRDoubleLocation[Opcode.FP.fs]);
	SET_COP1_CAUSE(cause);
	TEST_COP1_FP_EXCEPTION();
	_clearfp();
	__int64 result;
	Double_RoundToInteger64(&result, &*(double*)FPRDoubleLocation[Opcode.FP.fs]);
	cause |= getCop1ConvertCause();
	SET_COP1_CAUSE(cause);
	TEST_COP1_FP_EXCEPTION();
	SET_COP1_FLAGS(cause);
	*(unsigned __int64*)FPRDoubleFTFDLocation[Opcode.FP.fd] = result;
}

void _fastcall r4300i_COP1_D_TRUNC_L (void) {	//added by Witten
	TEST_COP1_USABLE_EXCEPTION();
	CLEAR_COP1_CAUSE();
	_controlfp(RC_CHOP,_MCW_RC);
	DWORD cause = getCop1DToLArgCause((QWORD*)FPRDoubleLocation[Opcode.FP.fs]);
	SET_COP1_CAUSE(cause);
	TEST_COP1_FP_EXCEPTION();
	_clearfp();
	__int64 result;
	Double_RoundToInteger64(&result,&*(double *)FPRDoubleLocation[Opcode.FP.fs] );
	cause |= getCop1ConvertCause();
	SET_COP1_CAUSE(cause);
	TEST_COP1_FP_EXCEPTION();
	SET_COP1_FLAGS(cause);
	*(unsigned __int64*)FPRDoubleFTFDLocation[Opcode.FP.fd] = result;
}

void _fastcall r4300i_COP1_D_CEIL_L (void) {	//added by Witten
	TEST_COP1_USABLE_EXCEPTION();
	CLEAR_COP1_CAUSE();
	_controlfp(RC_UP,_MCW_RC);
	DWORD cause = getCop1DToLArgCause((QWORD*)FPRDoubleLocation[Opcode.FP.fs]);
	SET_COP1_CAUSE(cause);
	TEST_COP1_FP_EXCEPTION();
	_clearfp();
	__int64 result;
	Double_RoundToInteger64(&result,&*(double *)FPRDoubleLocation[Opcode.FP.fs] );
	cause |= getCop1ConvertCause();
	SET_COP1_CAUSE(cause);
	TEST_COP1_FP_EXCEPTION();
	SET_COP1_FLAGS(cause);
	*(unsigned __int64*)FPRDoubleFTFDLocation[Opcode.FP.fd] = result;
}

void _fastcall r4300i_COP1_D_FLOOR_L (void) {	//added by Witten
	TEST_COP1_USABLE_EXCEPTION();
	CLEAR_COP1_CAUSE();
	_controlfp(_RC_DOWN,_MCW_RC);
	DWORD cause = getCop1DToLArgCause((QWORD*)FPRDoubleLocation[Opcode.FP.fs]);
	SET_COP1_CAUSE(cause);
	TEST_COP1_FP_EXCEPTION();
	_clearfp();
	__int64 result;
	Double_RoundToInteger64(&result,&*(double *)FPRDoubleLocation[Opcode.FP.fs]);
	cause |= getCop1ConvertCause();
	SET_COP1_CAUSE(cause);
	TEST_COP1_FP_EXCEPTION();
	SET_COP1_FLAGS(cause);
	*(unsigned __int64*)FPRDoubleFTFDLocation[Opcode.FP.fd] = result;
}

void _fastcall r4300i_COP1_D_ROUND_W (void) {
	TEST_COP1_USABLE_EXCEPTION();
	CLEAR_COP1_CAUSE();
	_controlfp(_RC_NEAR,_MCW_RC);
	DWORD cause = getCop1DToWArgCause((QWORD*)FPRDoubleLocation[Opcode.FP.fs]);
	SET_COP1_CAUSE(cause);
	TEST_COP1_FP_EXCEPTION();
	_clearfp();
	DWORD result;
	Double_RoundToInteger32(&result,&*(double *)FPRDoubleLocation[Opcode.FP.fs] );
	cause |= getCop1ConvertCause();
	SET_COP1_CAUSE(cause);
	TEST_COP1_FP_EXCEPTION();
	SET_COP1_FLAGS(cause);
	*(DWORD*)FPRFloatOtherLocation[Opcode.FP.fd] = result;
	*(long*)FPRFloatUpperHalfLocation[Opcode.FP.fd] = 0;
}

void _fastcall r4300i_COP1_D_TRUNC_W (void) {
	TEST_COP1_USABLE_EXCEPTION();
	CLEAR_COP1_CAUSE();
	_controlfp(RC_CHOP,_MCW_RC);
	DWORD cause = getCop1DToWArgCause((QWORD*)FPRDoubleLocation[Opcode.FP.fs]);
	SET_COP1_CAUSE(cause);
	TEST_COP1_FP_EXCEPTION();
	_clearfp();
	DWORD result;
	Double_RoundToInteger32(&result,&*(double *)FPRDoubleLocation[Opcode.FP.fs] );
	cause |= getCop1ConvertCause();
	SET_COP1_CAUSE(cause);
	TEST_COP1_FP_EXCEPTION();
	SET_COP1_FLAGS(cause);
	*(DWORD*)FPRFloatOtherLocation[Opcode.FP.fd] = result;
	*(long*)FPRFloatUpperHalfLocation[Opcode.FP.fd] = 0;
}

void _fastcall r4300i_COP1_D_CEIL_W (void) {	//added by Witten
	TEST_COP1_USABLE_EXCEPTION();
	CLEAR_COP1_CAUSE();
	_controlfp(RC_UP,_MCW_RC);
	DWORD cause = getCop1DToWArgCause((QWORD*)FPRDoubleLocation[Opcode.FP.fs]);
	SET_COP1_CAUSE(cause);
	TEST_COP1_FP_EXCEPTION();
	_clearfp();
	DWORD result;
	Double_RoundToInteger32(&result,&*(double *)FPRDoubleLocation[Opcode.FP.fs] );
	cause |= getCop1ConvertCause();
	SET_COP1_CAUSE(cause);
	TEST_COP1_FP_EXCEPTION();
	SET_COP1_FLAGS(cause);
	*(DWORD*)FPRFloatOtherLocation[Opcode.FP.fd] = result;
	*(long*)FPRFloatUpperHalfLocation[Opcode.FP.fd] = 0;
}

void _fastcall r4300i_COP1_D_FLOOR_W (void) {	//added by Witten
	TEST_COP1_USABLE_EXCEPTION();
	CLEAR_COP1_CAUSE();
	_controlfp(_RC_DOWN,_MCW_RC);
	DWORD cause = getCop1DToWArgCause((QWORD*)FPRDoubleLocation[Opcode.FP.fs]);
	SET_COP1_CAUSE(cause);
	TEST_COP1_FP_EXCEPTION();
	_clearfp();
	DWORD result;
	Double_RoundToInteger32(&result,&*(double *)FPRDoubleLocation[Opcode.FP.fs]);
	cause |= getCop1ConvertCause();
	SET_COP1_CAUSE(cause);
	TEST_COP1_FP_EXCEPTION();
	SET_COP1_FLAGS(cause);
	*(DWORD*)FPRFloatOtherLocation[Opcode.FP.fd] = result;
	*(long*)FPRFloatUpperHalfLocation[Opcode.FP.fd] = 0;
}

void _fastcall r4300i_COP1_D_CVT_S (void) {
	TEST_COP1_USABLE_EXCEPTION();
	CLEAR_COP1_CAUSE();
	_controlfp(RoundingModel, _MCW_RC);
	_clearfp();
	DWORD cause = getCop1DArgCause((QWORD*)FPRDoubleLocation[Opcode.FP.fs]);
	SET_COP1_CAUSE(cause);
	TEST_COP1_FP_EXCEPTION();
	float result = (float)*(double *)FPRDoubleLocation[Opcode.FP.fs];
	cause |= getCop1SCause(&result);
	SET_COP1_CAUSE(cause);
	TEST_COP1_FP_EXCEPTION();
	SET_COP1_FLAGS(cause);
	*(float*)FPRFloatOtherLocation[Opcode.FP.fd] = result;
	*(long*)FPRFloatUpperHalfLocation[Opcode.FP.fd] = 0;
}

void _fastcall r4300i_COP1_D_CVT_D (void) {
	TEST_COP1_USABLE_EXCEPTION();
	CLEAR_COP1_CAUSE();
	DWORD cause = CAUSE_UNIMPLEMENTED;
	SET_COP1_CAUSE(cause);
	TEST_COP1_FP_EXCEPTION();
}

void _fastcall r4300i_COP1_D_CVT_W (void) {
	TEST_COP1_USABLE_EXCEPTION();
	CLEAR_COP1_CAUSE();
	_controlfp(RoundingModel,_MCW_RC);
	DWORD cause = getCop1DToWArgCause((QWORD*)FPRDoubleLocation[Opcode.FP.fs]);
	SET_COP1_CAUSE(cause);
	TEST_COP1_FP_EXCEPTION();
	_clearfp();
	DWORD result;
	Double_RoundToInteger32(&result,&*(double *)FPRDoubleLocation[Opcode.FP.fs] );
	cause |= getCop1ConvertCause();
	SET_COP1_CAUSE(cause);
	TEST_COP1_FP_EXCEPTION();
	SET_COP1_FLAGS(cause);
	*(DWORD*)FPRFloatOtherLocation[Opcode.FP.fd] = result;
	*(long*)FPRFloatUpperHalfLocation[Opcode.FP.fd] = 0;
}

void _fastcall r4300i_COP1_D_CVT_L (void) {
	TEST_COP1_USABLE_EXCEPTION();
	CLEAR_COP1_CAUSE();
	_controlfp(RoundingModel,_MCW_RC);
	DWORD cause = getCop1DToLArgCause((QWORD*)FPRDoubleLocation[Opcode.FP.fs]);
	SET_COP1_CAUSE(cause);
	TEST_COP1_FP_EXCEPTION();
	_clearfp();
	__int64 result;
	Double_RoundToInteger64(&result,&*(double *)FPRDoubleLocation[Opcode.FP.fs]);
	cause |= getCop1ConvertCause();
	SET_COP1_CAUSE(cause);
	TEST_COP1_FP_EXCEPTION();
	SET_COP1_FLAGS(cause);
	*(unsigned __int64*)FPRDoubleFTFDLocation[Opcode.FP.fd] = result;
}

void _fastcall r4300i_COP1_D_CMP (void) {
	int less, equal, unorded, condition;
	MIPS_DWORD Temp0, Temp1;

	TEST_COP1_USABLE_EXCEPTION();
	CLEAR_COP1_CAUSE();

	Temp0.DW = *(__int64 *)FPRDoubleLocation[Opcode.FP.fs];
	Temp1.DW = *(__int64 *)FPRDoubleFTFDLocation[Opcode.FP.ft];

	if (IsQNAN_D(Temp0.DW) || IsQNAN_D(Temp1.DW)) {
		less = FALSE;
		equal = FALSE;
		unorded = TRUE;
		SET_COP1_CAUSE(CAUSE_INVALID);
		TEST_COP1_FP_EXCEPTION();
		SET_COP1_FLAGS(CAUSE_INVALID);
	} else if (_isnan(Temp0.D) || _isnan(Temp1.D)) {
		if (ShowDebugMessages)
			DisplayError("Not A Number?");
		less = FALSE;
		equal = FALSE;
		unorded = TRUE;
		if ((Opcode.REG.funct & 8) != 0) {
			if (ShowDebugMessages)
				DisplayError("Signal InvalidOperationException\nin r4300i_COP1_D_CMP");
			SET_COP1_CAUSE(CAUSE_INVALID);
			TEST_COP1_FP_EXCEPTION();
			SET_COP1_FLAGS(CAUSE_INVALID);
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
void _fastcall r4300i_COP1_W_ROUND_L(void) {
	TEST_COP1_USABLE_EXCEPTION();
	CLEAR_COP1_CAUSE();
	SET_COP1_CAUSE(CAUSE_UNIMPLEMENTED);
	TEST_COP1_FP_EXCEPTION();
}

void _fastcall r4300i_COP1_W_TRUNC_L(void) {
	TEST_COP1_USABLE_EXCEPTION();
	CLEAR_COP1_CAUSE();
	SET_COP1_CAUSE(CAUSE_UNIMPLEMENTED);
	TEST_COP1_FP_EXCEPTION();
}

void _fastcall r4300i_COP1_W_CEIL_L(void) {
	TEST_COP1_USABLE_EXCEPTION();
	CLEAR_COP1_CAUSE();
	SET_COP1_CAUSE(CAUSE_UNIMPLEMENTED);
	TEST_COP1_FP_EXCEPTION();
}

void _fastcall r4300i_COP1_W_FLOOR_L(void) {
	TEST_COP1_USABLE_EXCEPTION();
	CLEAR_COP1_CAUSE();
	SET_COP1_CAUSE(CAUSE_UNIMPLEMENTED);
	TEST_COP1_FP_EXCEPTION();
}

void _fastcall r4300i_COP1_W_ROUND_W (void) {
	TEST_COP1_USABLE_EXCEPTION();
	CLEAR_COP1_CAUSE();
	SET_COP1_CAUSE(CAUSE_UNIMPLEMENTED);
	TEST_COP1_FP_EXCEPTION();
}

void _fastcall r4300i_COP1_W_TRUNC_W(void) {
	TEST_COP1_USABLE_EXCEPTION();
	CLEAR_COP1_CAUSE();
	SET_COP1_CAUSE(CAUSE_UNIMPLEMENTED);
	TEST_COP1_FP_EXCEPTION();
}

void _fastcall r4300i_COP1_W_CEIL_W(void) {
	TEST_COP1_USABLE_EXCEPTION();
	CLEAR_COP1_CAUSE();
	SET_COP1_CAUSE(CAUSE_UNIMPLEMENTED);
	TEST_COP1_FP_EXCEPTION();
}

void _fastcall r4300i_COP1_W_FLOOR_W(void) {
	TEST_COP1_USABLE_EXCEPTION();
	CLEAR_COP1_CAUSE();
	SET_COP1_CAUSE(CAUSE_UNIMPLEMENTED);
	TEST_COP1_FP_EXCEPTION();
}

void _fastcall r4300i_COP1_W_CVT_S (void) {
	TEST_COP1_USABLE_EXCEPTION();
	CLEAR_COP1_CAUSE();
	_controlfp(RoundingModel,_MCW_RC);
	_clearfp();
	float result = (float)*(int *)FPRFloatFSLocation[Opcode.FP.fs];
	DWORD cause = getCop1SCause(&result);
	SET_COP1_CAUSE(cause);
	TEST_COP1_FP_EXCEPTION();
	SET_COP1_FLAGS(cause);
	*(float*)FPRFloatOtherLocation[Opcode.FP.fd] = result;
	*(long*)FPRFloatUpperHalfLocation[Opcode.FP.fd] = 0;
}

void _fastcall r4300i_COP1_W_CVT_D (void) {
	TEST_COP1_USABLE_EXCEPTION();
	CLEAR_COP1_CAUSE();
	_controlfp(RoundingModel,_MCW_RC);
	*(double *)FPRDoubleFTFDLocation[Opcode.FP.fd] = (double)*(int *)FPRFloatFSLocation[Opcode.FP.fs];
}

void _fastcall r4300i_COP1_W_CVT_W (void) {
	TEST_COP1_USABLE_EXCEPTION();
	CLEAR_COP1_CAUSE();
	SET_COP1_CAUSE(CAUSE_UNIMPLEMENTED);
	TEST_COP1_FP_EXCEPTION();
}

void _fastcall r4300i_COP1_W_CVT_L (void) {
	TEST_COP1_USABLE_EXCEPTION();
	CLEAR_COP1_CAUSE();
	SET_COP1_CAUSE(CAUSE_UNIMPLEMENTED);
	TEST_COP1_FP_EXCEPTION();
}

/************************** COP1: L functions ************************/
void _fastcall r4300i_COP1_L_ROUND_L(void) {
	TEST_COP1_USABLE_EXCEPTION();
	CLEAR_COP1_CAUSE();
	SET_COP1_CAUSE(CAUSE_UNIMPLEMENTED);
	TEST_COP1_FP_EXCEPTION();
}

void _fastcall r4300i_COP1_L_TRUNC_L(void) {
	TEST_COP1_USABLE_EXCEPTION();
	CLEAR_COP1_CAUSE();
	SET_COP1_CAUSE(CAUSE_UNIMPLEMENTED);
	TEST_COP1_FP_EXCEPTION();
}

void _fastcall r4300i_COP1_L_CEIL_L(void) {
	TEST_COP1_USABLE_EXCEPTION();
	CLEAR_COP1_CAUSE();
	SET_COP1_CAUSE(CAUSE_UNIMPLEMENTED);
	TEST_COP1_FP_EXCEPTION();
}

void _fastcall r4300i_COP1_L_FLOOR_L(void) {
	TEST_COP1_USABLE_EXCEPTION();
	CLEAR_COP1_CAUSE();
	SET_COP1_CAUSE(CAUSE_UNIMPLEMENTED);
	TEST_COP1_FP_EXCEPTION();
}

void _fastcall r4300i_COP1_L_ROUND_W(void) {
	TEST_COP1_USABLE_EXCEPTION();
	CLEAR_COP1_CAUSE();
	SET_COP1_CAUSE(CAUSE_UNIMPLEMENTED);
	TEST_COP1_FP_EXCEPTION();
}

void _fastcall r4300i_COP1_L_TRUNC_W(void) {
	TEST_COP1_USABLE_EXCEPTION();
	CLEAR_COP1_CAUSE();
	SET_COP1_CAUSE(CAUSE_UNIMPLEMENTED);
	TEST_COP1_FP_EXCEPTION();
}

void _fastcall r4300i_COP1_L_CEIL_W(void) {
	TEST_COP1_USABLE_EXCEPTION();
	CLEAR_COP1_CAUSE();
	SET_COP1_CAUSE(CAUSE_UNIMPLEMENTED);
	TEST_COP1_FP_EXCEPTION();
}

void _fastcall r4300i_COP1_L_FLOOR_W(void) {
	TEST_COP1_USABLE_EXCEPTION();
	CLEAR_COP1_CAUSE();
	SET_COP1_CAUSE(CAUSE_UNIMPLEMENTED);
	TEST_COP1_FP_EXCEPTION();
}

void _fastcall r4300i_COP1_L_CVT_S (void) {
	TEST_COP1_USABLE_EXCEPTION();
	CLEAR_COP1_CAUSE();
	_controlfp(RoundingModel,_MCW_RC);
	_clearfp();
	if(*(__int64*)FPRDoubleLocation[Opcode.FP.fs] >= (__int64)0x0080000000000000LL ||
	   *(__int64*)FPRDoubleLocation[Opcode.FP.fs] <  (__int64)0xFF80000000000000LL) {
		SET_COP1_CAUSE(CAUSE_UNIMPLEMENTED);
		TEST_COP1_FP_EXCEPTION();
	}
	float result = (float)*(__int64 *)FPRDoubleLocation[Opcode.FP.fs];
	DWORD cause = getCop1SCause(&result);
	SET_COP1_CAUSE(cause);
	TEST_COP1_FP_EXCEPTION();
	SET_COP1_FLAGS(cause);
	*(float*)FPRFloatOtherLocation[Opcode.FP.fd] = result;
	*(long*)FPRFloatUpperHalfLocation[Opcode.FP.fd] = 0;
}

void _fastcall r4300i_COP1_L_CVT_D (void) {
	TEST_COP1_USABLE_EXCEPTION();
	CLEAR_COP1_CAUSE();
	_controlfp(RoundingModel,_MCW_RC);
	_clearfp();
	if (*(__int64*)FPRDoubleLocation[Opcode.FP.fs] >= (__int64)0x0080000000000000LL ||
		*(__int64*)FPRDoubleLocation[Opcode.FP.fs] < (__int64)0xFF80000000000000LL) {
		SET_COP1_CAUSE(CAUSE_UNIMPLEMENTED);
		TEST_COP1_FP_EXCEPTION();
	}
	double result = (double)*(__int64 *)FPRDoubleLocation[Opcode.FP.fs];
	DWORD cause = getCop1DCause(&result);
	SET_COP1_CAUSE(cause);
	TEST_COP1_FP_EXCEPTION();
	SET_COP1_FLAGS(cause);
	*(double*)FPRDoubleFTFDLocation[Opcode.FP.fd] = result;
}

void _fastcall r4300i_COP1_L_CVT_W (void) {
	TEST_COP1_USABLE_EXCEPTION();
	CLEAR_COP1_CAUSE();
	SET_COP1_CAUSE(CAUSE_UNIMPLEMENTED);
	TEST_COP1_FP_EXCEPTION();
}

void _fastcall r4300i_COP1_L_CVT_L (void) {
	TEST_COP1_USABLE_EXCEPTION();
	CLEAR_COP1_CAUSE();
	SET_COP1_CAUSE(CAUSE_UNIMPLEMENTED);
	TEST_COP1_FP_EXCEPTION();
}

/************************** COP2 functions **************************/
void _fastcall r4300i_COP2_MF(void) {
	if ((STATUS_REGISTER & STATUS_CU2) == 0) {
		DoCopUnusableException(NextInstruction == JUMP, 2);
		NextInstruction = JUMP;
		JumpToLocation = PROGRAM_COUNTER;
	}
	GPR[Opcode.BRANCH.rt].DW = cop2LatchedValue.W[0];
}

void _fastcall r4300i_COP2_DMF(void) {
	if ((STATUS_REGISTER & STATUS_CU2) == 0) {
		DoCopUnusableException(NextInstruction == JUMP, 2);
		NextInstruction = JUMP;
		JumpToLocation = PROGRAM_COUNTER;
	}
	GPR[Opcode.BRANCH.rt].DW = cop2LatchedValue.DW;
}

void _fastcall r4300i_COP2_CF(void) {
	if ((STATUS_REGISTER & STATUS_CU2) == 0) {
		DoCopUnusableException(NextInstruction == JUMP, 2);
		NextInstruction = JUMP;
		JumpToLocation = PROGRAM_COUNTER;
	}
}

void _fastcall r4300i_COP2_DCF(void) {
	if ((STATUS_REGISTER & STATUS_CU2) == 0) {
		DoCopUnusableException(NextInstruction == JUMP, 2);
		NextInstruction = JUMP;
		JumpToLocation = PROGRAM_COUNTER;
		return;
	}
	DoIllegalInstructionException(NextInstruction == JUMP);
	CAUSE_REGISTER |= 0x20000000;
	NextInstruction = JUMP;
	JumpToLocation = PROGRAM_COUNTER;
}

void _fastcall r4300i_COP2_MT(void) {
	if ((STATUS_REGISTER & STATUS_CU2) == 0) {
		DoCopUnusableException(NextInstruction == JUMP, 2);
		NextInstruction = JUMP;
		JumpToLocation = PROGRAM_COUNTER;
	}
	cop2LatchedValue.DW = GPR[Opcode.BRANCH.rt].DW;
}

void _fastcall r4300i_COP2_DMT(void) {
	if ((STATUS_REGISTER & STATUS_CU2) == 0) {
		DoCopUnusableException(NextInstruction == JUMP, 2);
		NextInstruction = JUMP;
		JumpToLocation = PROGRAM_COUNTER;
	}
	cop2LatchedValue.DW = GPR[Opcode.BRANCH.rt].DW;
}

void _fastcall r4300i_COP2_CT(void) {
	if ((STATUS_REGISTER & STATUS_CU2) == 0) {
		DoCopUnusableException(NextInstruction == JUMP, 2);
		NextInstruction = JUMP;
		JumpToLocation = PROGRAM_COUNTER;
	}
}

void _fastcall r4300i_COP2_DCT(void) {
	if ((STATUS_REGISTER & STATUS_CU2) == 0) {
		DoCopUnusableException(NextInstruction == JUMP, 2);
		NextInstruction = JUMP;
		JumpToLocation = PROGRAM_COUNTER;
		return;
	}
	DoIllegalInstructionException(NextInstruction == JUMP);
	CAUSE_REGISTER |= 0x20000000;
	NextInstruction = JUMP;
	JumpToLocation = PROGRAM_COUNTER;
}

/************************** Other functions **************************/
void _fastcall R4300i_UnknownOpcode (void) {
#ifdef STOP_ON_UNKNOWN_OPCODE
	char Message[200];

	sprintf(Message,"%s: %016llX\n%s\n\n", GS(MSG_UNHANDLED_OP), PROGRAM_COUNTER.UDW,
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
#else
	DoIllegalInstructionException(NextInstruction == JUMP);
	NextInstruction = JUMP;
	JumpToLocation = PROGRAM_COUNTER;
#endif
}

void _fastcall r4300i_RESERVED (void) {
	DoIllegalInstructionException(NextInstruction == JUMP);
	NextInstruction = JUMP;
	JumpToLocation = PROGRAM_COUNTER;
}
