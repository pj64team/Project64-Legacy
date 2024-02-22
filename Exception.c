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
#include "main.h"
#include "cpu.h"
#include "plugin.h"
#include "debugger.h"

void __cdecl AiCheckInterrupts ( void ) {	
	CPU_Action.CheckInterrupts = TRUE;
	CPU_Action.DoSomething = TRUE;
}

void __cdecl CheckInterrupts ( void ) {	

	MI_INTR_REG &= ~MI_INTR_AI;
	if (CPU_Type != CPU_SyncCores) {
		MI_INTR_REG |= (AudioIntrReg & MI_INTR_AI);
	}
	if ((MI_INTR_MASK_REG & MI_INTR_REG) != 0) {
		CAUSE_REGISTER |= CAUSE_IP2;
	} else  {
		CAUSE_REGISTER &= ~CAUSE_IP2;
	}

	if (( STATUS_REGISTER & STATUS_IE   ) == 0 ) { return; }
	if (( STATUS_REGISTER & STATUS_EXL  ) != 0 ) { return; }
	if (( STATUS_REGISTER & STATUS_ERL  ) != 0 ) { return; }

	if (( STATUS_REGISTER & CAUSE_REGISTER & 0xFF00) != 0) {
		if (!CPU_Action.DoInterrupt) {
			CPU_Action.DoSomething = TRUE;
			CPU_Action.DoInterrupt = TRUE;
		}
	}
}

void DoIntegerOverflow(BOOL DelaySlot) {
	if (ShowDebugMessages) {
		if ((STATUS_REGISTER & STATUS_EXL) != 0) {
			DisplayError("EXL set in AddressError Exception");
		}
		if ((STATUS_REGISTER & STATUS_ERL) != 0) {
			DisplayError("ERL set in AddressError Exception");
		}
	}
	CAUSE_REGISTER &= 0xFF00;
	CAUSE_REGISTER |= EXC_OV;
	if (DelaySlot) {
		CAUSE_REGISTER |= CAUSE_BD;
		EPC_REGISTER = PROGRAM_COUNTER.UDW - 4;
	} else {
		EPC_REGISTER = PROGRAM_COUNTER.UDW;
	}
	STATUS_REGISTER |= STATUS_EXL;
	UpdateCPUMode();
	PROGRAM_COUNTER.UDW = 0xFFFFFFFF80000180LL;
}

void DoAddressError ( BOOL DelaySlot, QWORD BadVaddr, BOOL FromRead) {
	if (ShowDebugMessages) {
		DisplayError("AddressError while accessing %016llX (%s). PC: %016llX",
			BadVaddr,
			FromRead ? "read" : "write",
			DelaySlot ? PROGRAM_COUNTER.UDW - 4 : PROGRAM_COUNTER.UDW);
		if ((STATUS_REGISTER & STATUS_EXL) != 0) {
			DisplayError("EXL set in AddressError Exception");
		}
		if ((STATUS_REGISTER & STATUS_ERL) != 0) {
			DisplayError("ERL set in AddressError Exception");
		}
	}
	CAUSE_REGISTER &= 0xFF00;
	if (FromRead) {
		CAUSE_REGISTER |= EXC_RADE;
	} else {
		CAUSE_REGISTER |= EXC_WADE;
	}
	BAD_VADDR_REGISTER = BadVaddr;
	CONTEXT_REGISTER &= 0xFFFFFFFFFF80000FLL;
	CONTEXT_REGISTER |= (BadVaddr >> 9) & 0x007FFFF0LL;
	XCONTEXT_REGISTER &= 0xFFFFFFFE00000000LL;
	XCONTEXT_REGISTER |= ((QWORD)BadVaddr >> 9) & 0x7FFFFFF0LL;
	XCONTEXT_REGISTER |= ((QWORD)BadVaddr >> 31) & 0x180000000LL;
	ENTRYHI_REGISTER = BadVaddr & 0xC00000FFFFFFE000LL;
	if (DelaySlot) {
		CAUSE_REGISTER |= CAUSE_BD;
		EPC_REGISTER = PROGRAM_COUNTER.UDW - 4;
	} else {
		EPC_REGISTER = PROGRAM_COUNTER.UDW;
	}
	STATUS_REGISTER |= STATUS_EXL;
	UpdateCPUMode();
	PROGRAM_COUNTER.UDW = 0xFFFFFFFF80000180LL;
}

void _fastcall DoFPException(BOOL DelaySlot) {
	if (ShowDebugMessages) {
		if ((STATUS_REGISTER & STATUS_EXL) != 0) {
			DisplayError("EXL set in Break Exception");
		}
		if ((STATUS_REGISTER & STATUS_ERL) != 0) {
			DisplayError("ERL set in Break Exception");
		}
	}
	CAUSE_REGISTER &= 0xFF00;
	CAUSE_REGISTER |= EXC_FPE;
	if (DelaySlot) {
		CAUSE_REGISTER |= CAUSE_BD;
		EPC_REGISTER = PROGRAM_COUNTER.UDW - 4;
	}
	else {
		EPC_REGISTER = PROGRAM_COUNTER.UDW;
	}
	STATUS_REGISTER |= STATUS_EXL;
	UpdateCPUMode();
	PROGRAM_COUNTER.UDW = 0xFFFFFFFF80000180LL;
}

void _fastcall DoBreakException ( BOOL DelaySlot) {
	if (ShowDebugMessages) {
		if ((STATUS_REGISTER & STATUS_EXL) != 0) {
			DisplayError("EXL set in Break Exception");
		}
		if ((STATUS_REGISTER & STATUS_ERL) != 0) {
			DisplayError("ERL set in Break Exception");
		}
	}
	CAUSE_REGISTER &= 0xFF00;
	CAUSE_REGISTER |= EXC_BREAK;
	if (DelaySlot) {
		CAUSE_REGISTER |= CAUSE_BD;
		EPC_REGISTER = PROGRAM_COUNTER.UDW - 4;
	} else {
		EPC_REGISTER = PROGRAM_COUNTER.UDW;
	}
	STATUS_REGISTER |= STATUS_EXL;
	UpdateCPUMode();
	PROGRAM_COUNTER.UDW = 0xFFFFFFFF80000180LL;
}

void _fastcall DoCopUnusableException ( BOOL DelaySlot, int Coprocessor ) {
	if (ShowDebugMessages) {
		if ((STATUS_REGISTER & STATUS_EXL) != 0) {
			DisplayError("EXL set in Break Exception");
		}
		if ((STATUS_REGISTER & STATUS_ERL) != 0) {
			DisplayError("ERL set in Break Exception");
		}
	}
	CAUSE_REGISTER &= 0xFF00;
	CAUSE_REGISTER |= EXC_CPU;
	if (Coprocessor == 1) { CAUSE_REGISTER |= 0x10000000; }
	else if (Coprocessor == 2) { CAUSE_REGISTER |= 0x20000000; }
	if (DelaySlot) {
		CAUSE_REGISTER |= CAUSE_BD;
		EPC_REGISTER = PROGRAM_COUNTER.UDW - 4;
	} else {
		EPC_REGISTER = PROGRAM_COUNTER.UDW;
	}
	STATUS_REGISTER |= STATUS_EXL;
	UpdateCPUMode();
	PROGRAM_COUNTER.UDW = 0xFFFFFFFF80000180LL;
}

void DoIntrException ( BOOL DelaySlot ) {
	if (( STATUS_REGISTER & STATUS_IE   ) == 0 ) { return; }
	if (( STATUS_REGISTER & STATUS_EXL  ) != 0 ) { return; }
	if (( STATUS_REGISTER & STATUS_ERL  ) != 0 ) { return; }
	CAUSE_REGISTER &= 0xFF00;
	CAUSE_REGISTER |= EXC_INT;
	if (DelaySlot) {
		CAUSE_REGISTER |= CAUSE_BD;
		EPC_REGISTER = PROGRAM_COUNTER.UDW - 4;
	} else {
		EPC_REGISTER = PROGRAM_COUNTER.UDW;
	}
	STATUS_REGISTER |= STATUS_EXL;
	UpdateCPUMode();
	PROGRAM_COUNTER.UDW = 0xFFFFFFFF80000180LL;
}

void _fastcall DoTLBMiss ( BOOL DelaySlot, QWORD BadVaddr, BOOL FromRead ) {
	CAUSE_REGISTER &= 0xFF00;
	CAUSE_REGISTER |= FromRead ? EXC_RMISS : EXC_WMISS;
	if (CAUSE_REGISTER == EXC_WMISS) {
		if (!Addressing64Bits) {
			if (TLB_ReadMap[BadVaddr >> 12] != 0) {
				CAUSE_REGISTER &= 0xFF00;
				CAUSE_REGISTER |= EXC_MOD;
			}
		}
		else {
			MIPS_DWORD Addr;
			DWORD PAddr;
			Addr.UDW = BadVaddr;
			if(IsLastFailWriteProtectedPage()) {
				CAUSE_REGISTER &= 0xFF00;
				CAUSE_REGISTER |= EXC_MOD;
			}
		}
	}
	BAD_VADDR_REGISTER = BadVaddr;
	CONTEXT_REGISTER &= 0xFFFFFFFFFF80000FLL;
	CONTEXT_REGISTER |= (BadVaddr >> 9) & 0x007FFFF0LL;
	XCONTEXT_REGISTER &= 0xFFFFFFFE00000000LL;
	XCONTEXT_REGISTER |= ((QWORD)BadVaddr >> 9) & 0x7FFFFFF0LL;
	XCONTEXT_REGISTER |= ((QWORD)BadVaddr >> 31) & 0x180000000LL;
	ENTRYHI_REGISTER = (ENTRYHI_REGISTER & 0xFF) | (BadVaddr & 0xC00000FFFFFFE000LL);
	
	if ((STATUS_REGISTER & STATUS_EXL) == 0) {
		if (DelaySlot) {
			CAUSE_REGISTER |= CAUSE_BD;
			EPC_REGISTER = PROGRAM_COUNTER.UDW - 4;
		} else {
			EPC_REGISTER = PROGRAM_COUNTER.UDW;
		}
		if (!Addressing64Bits) {
			if (AddressDefined(BadVaddr)) {
				PROGRAM_COUNTER.UDW = 0xFFFFFFFF80000180LL;
			}
			else {
				PROGRAM_COUNTER.UDW = 0xFFFFFFFF80000000LL;
			}
		}
		else {
			if (!IsLastFailInvalidPage() && !IsLastFailWriteProtectedPage()) {
				PROGRAM_COUNTER.UDW = 0xFFFFFFFF80000080LL;
			}
			else {
				PROGRAM_COUNTER.UDW = 0xFFFFFFFF80000180LL;
			}
		}
		STATUS_REGISTER |= STATUS_EXL;
	} else {
		if (ShowDebugMessages)
			DisplayError("EXL Set\nAddress Defined: %s, %llx",AddressDefined(BadVaddr)?"TRUE":"FALSE", BadVaddr);
		PROGRAM_COUNTER.UDW = 0xFFFFFFFF80000180LL;
	}
	UpdateCPUMode();
}

void _fastcall DoSysCallException(BOOL DelaySlot) {
	if (ShowDebugMessages) {
		if ((STATUS_REGISTER & STATUS_EXL) != 0) {
			DisplayError("EXL set in SysCall Exception");
		}
		if ((STATUS_REGISTER & STATUS_ERL) != 0) {
			DisplayError("ERL set in SysCall Exception");
		}
	}
	CAUSE_REGISTER &= 0xFF00;
	CAUSE_REGISTER |= EXC_SYSCALL;
	if (DelaySlot) {
		CAUSE_REGISTER |= CAUSE_BD;
		EPC_REGISTER = PROGRAM_COUNTER.UDW - 4;
	}
	else {
		EPC_REGISTER = PROGRAM_COUNTER.UDW;
	}
	STATUS_REGISTER |= STATUS_EXL;
	UpdateCPUMode();
	PROGRAM_COUNTER.UDW = 0xFFFFFFFF80000180LL;
}

void _fastcall DoIllegalInstructionException(BOOL DelaySlot) {
	if (ShowDebugMessages) {
		if ((STATUS_REGISTER & STATUS_EXL) != 0) {
			DisplayError("EXL set in SysCall Exception");
		}
		if ((STATUS_REGISTER & STATUS_ERL) != 0) {
			DisplayError("ERL set in SysCall Exception");
		}
	}
	CAUSE_REGISTER &= 0xFF00;
	CAUSE_REGISTER |= EXC_II;
	if (DelaySlot) {
		CAUSE_REGISTER |= CAUSE_BD;
		EPC_REGISTER = PROGRAM_COUNTER.UDW - 4;
	}
	else {
		EPC_REGISTER = PROGRAM_COUNTER.UDW;
	}
	STATUS_REGISTER |= STATUS_EXL;
	UpdateCPUMode();
	PROGRAM_COUNTER.UDW = 0xFFFFFFFF80000180LL;
}

void _fastcall DoTrapException(BOOL DelaySlot) {
	if (ShowDebugMessages) {
		if ((STATUS_REGISTER & STATUS_EXL) != 0) {
			DisplayError("EXL set in Break Exception");
		}
		if ((STATUS_REGISTER & STATUS_ERL) != 0) {
			DisplayError("ERL set in Break Exception");
		}
	}
	CAUSE_REGISTER &= 0xFF00;
	CAUSE_REGISTER |= EXC_TRAP;
	if (DelaySlot) {
		CAUSE_REGISTER |= CAUSE_BD;
		EPC_REGISTER = PROGRAM_COUNTER.UDW - 4;
	}
	else {
		EPC_REGISTER = PROGRAM_COUNTER.UDW;
	}
	STATUS_REGISTER |= STATUS_EXL;
	UpdateCPUMode();
	PROGRAM_COUNTER.UDW = 0xFFFFFFFF80000180LL;
}
