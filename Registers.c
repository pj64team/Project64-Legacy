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
#include "debugger.h"
#include "RomTools_Common.h"

char *GPR_Name[32] = {"r0","at","v0","v1","a0","a1","a2","a3",
                     "t0","t1","t2","t3","t4","t5","t6","t7",
                     "s0","s1","s2","s3","s4","s5","s6","s7",
                     "t8","t9","k0","k1","gp","sp","s8","ra"};

char *GPR_NameHi[32] = {"r0.HI","at.HI","v0.HI","v1.HI","a0.HI","a1.HI",
						"a2.HI","a3.HI","t0.HI","t1.HI","t2.HI","t3.HI",
						"t4.HI","t5.HI","t6.HI","t7.HI","s0.HI","s1.HI",
						"s2.HI","s3.HI","s4.HI","s5.HI","s6.HI","s7.HI",
						"t8.HI","t9.HI","k0.HI","k1.HI","gp.HI","sp.HI",
						"s8.HI","ra.HI"};

char *GPR_NameLo[32] = {"r0.LO","at.LO","v0.LO","v1.LO","a0.LO","a1.LO",
						"a2.LO","a3.LO","t0.LO","t1.LO","t2.LO","t3.LO",
						"t4.LO","t5.LO","t6.LO","t7.LO","s0.LO","s1.LO",
						"s2.LO","s3.LO","s4.LO","s5.LO","s6.LO","s7.LO",
						"t8.LO","t9.LO","k0.LO","k1.LO","gp.LO","sp.LO",
						"s8.LO","ra.LO"};

char *FPR_Name[32] = {"f0","f1","f2","f3","f4","f5","f6","f7",
                     "f8","f9","f10","f11","f12","f13","f14","f15",
                     "f16","f17","f18","f19","f20","f21","f22","f23",
                     "f24","f25","f26","f27","f28","f29","f30","f31"};

char *FPR_NameHi[32] = {"f0.hi","f1.hi","f2.hi","f3.hi","f4.hi","f5.hi","f6.hi","f7.hi",
                     "f8.hi","f9.hi","f10.hi","f11.hi","f12.hi","f13.hi","f14.hi","f15.hi",
                     "f16.hi","f17.hi","f18.hi","f19.hi","f20.hi","f21.hi","f22.hi","f23.hi",
                     "f24.hi","f25.hi","f26.hi","f27.hi","f28.hi","f29.hi","f30.hi","f31.hi"};

char *FPR_NameLo[32] = {"f0.lo","f1.lo","f2.lo","f3.lo","f4.lo","f5.lo","f6.lo","f7.lo",
                     "f8.lo","f9.lo","f10.lo","f11.lo","f12.lo","f13.lo","f14.lo","f15.lo",
                     "f16.lo","f17.lo","f18.lo","f19.lo","f20.lo","f21.lo","f22.lo","f23.lo",
                     "f24.lo","f25.lo","f26.lo","f27.lo","f28.lo","f29.lo","f30.lo","f31.lo"};

char *FPR_Ctrl_Name[32] = {"Revision","Unknown","Unknown","Unknown","Unknown",
					"Unknown","Unknown","Unknown","Unknown","Unknown","Unknown",
					"Unknown","Unknown","Unknown","Unknown","Unknown","Unknown",
					"Unknown","Unknown","Unknown","Unknown","Unknown","Unknown",
					"Unknown","Unknown","Unknown","Unknown","Unknown","Unknown",
					"Unknown","Unknown","FCSR"};

char *Cop0_Name[32] = {"Index","Random","EntryLo0","EntryLo1","Context","PageMask","Wired","7",
                    "BadVAddr","Count","EntryHi","Compare","Status","Cause","EPC","PRId",
                    "Config","LLAddr","WatchLo","WatchHi","XContext","21","22","23",
                    "24","25","ECC","CacheErr","TagLo","TagHi","ErrEPC","31"};

DWORD *FPCR,*RegSP,*RegDPC,*RegMI,*RegVI,*RegAI,*RegPI,
	*RegRI,*RegSI, HalfLine, RegModValue, ViFieldSerration, LLBit;
DWORD (*RegRDRAM)[4][10];
void * FPRDoubleLocation[32], * FPRFloatLoadStoreLocation[32], *FPRFloatUpperHalfLocation[32], *FPRFloatFSLocation[32];
void* FPRFloatOtherLocation[32], *FPRDoubleFTFDLocation[32];
MIPS_DWORD PROGRAM_COUNTER, *GPR, *FPR, HI, LO, *CP0;
N64_REGISTERS Registers;
int fpuControl;
int lastUnusedCOP0Register;
MIPS_DWORD cop2LatchedValue;
DWORD RegSPW[2];


int  UnMap_8BitTempReg (BLOCK_SECTION * Section);
int  UnMap_TempReg     (BLOCK_SECTION * Section);
BOOL UnMap_X86reg      (BLOCK_SECTION * Section, DWORD x86Reg);

char *Format_Name[] = {"Unkown","dword","qword","float","double"};

void ChangeFPURegFormat (BLOCK_SECTION * Section, int Reg, int OldFormat, int NewFormat, int RoundingModel) {
	DWORD i;

	for (i = 0; i < 8; i++) {
		if (FpuMappedTo(i) == (DWORD)Reg) {
			if (FpuState(i) != (DWORD)OldFormat) {		
				UnMap_FPR(Section,Reg,TRUE);
				Load_FPR_ToTop(Section,Reg,Reg,OldFormat);
				ChangeFPURegFormat(Section,Reg,OldFormat,NewFormat,RoundingModel);
				return;
			}
			CPU_Message("    regcache: Changed format of ST(%d) from %s to %s", 
				(i - StackTopPos + 8) & 7,Format_Name[OldFormat],Format_Name[NewFormat]);			
			FpuRoundingModel(i) = RoundingModel;
			FpuState(i)         = NewFormat;
			return;
		}
	}

	if (ShowDebugMessages)
		DisplayError("ChangeFormat: Register not on stack!!");
}

void ChangeMiIntrMask (void) {
	if ( ( RegModValue & MI_INTR_MASK_CLR_SP ) != 0 ) { MI_INTR_MASK_REG &= ~MI_INTR_MASK_SP; }
	if ( ( RegModValue & MI_INTR_MASK_SET_SP ) != 0 ) { MI_INTR_MASK_REG |= MI_INTR_MASK_SP; }
	if ( ( RegModValue & MI_INTR_MASK_CLR_SI ) != 0 ) { MI_INTR_MASK_REG &= ~MI_INTR_MASK_SI; }
	if ( ( RegModValue & MI_INTR_MASK_SET_SI ) != 0 ) { MI_INTR_MASK_REG |= MI_INTR_MASK_SI; }
	if ( ( RegModValue & MI_INTR_MASK_CLR_AI ) != 0 ) { MI_INTR_MASK_REG &= ~MI_INTR_MASK_AI; }
	if ( ( RegModValue & MI_INTR_MASK_SET_AI ) != 0 ) { MI_INTR_MASK_REG |= MI_INTR_MASK_AI; }
	if ( ( RegModValue & MI_INTR_MASK_CLR_VI ) != 0 ) { MI_INTR_MASK_REG &= ~MI_INTR_MASK_VI; }
	if ( ( RegModValue & MI_INTR_MASK_SET_VI ) != 0 ) { MI_INTR_MASK_REG |= MI_INTR_MASK_VI; }
	if ( ( RegModValue & MI_INTR_MASK_CLR_PI ) != 0 ) { MI_INTR_MASK_REG &= ~MI_INTR_MASK_PI; }
	if ( ( RegModValue & MI_INTR_MASK_SET_PI ) != 0 ) { MI_INTR_MASK_REG |= MI_INTR_MASK_PI; }
	if ( ( RegModValue & MI_INTR_MASK_CLR_DP ) != 0 ) { MI_INTR_MASK_REG &= ~MI_INTR_MASK_DP; }
	if ( ( RegModValue & MI_INTR_MASK_SET_DP ) != 0 ) { MI_INTR_MASK_REG |= MI_INTR_MASK_DP; }
}

void ChangeMiModeReg (void) {
	MI_MODE_REG &= ~0x7F;
	MI_MODE_REG |= (RegModValue & 0x7F);
	if ( ( RegModValue & MI_CLR_INIT ) != 0 ) { MI_MODE_REG &= ~MI_MODE_INIT; }
	if ( ( RegModValue & MI_SET_INIT ) != 0 ) { MI_MODE_REG |= MI_MODE_INIT; }
	if ( ( RegModValue & MI_CLR_EBUS ) != 0 ) { MI_MODE_REG &= ~MI_MODE_EBUS; }
	if ( ( RegModValue & MI_SET_EBUS ) != 0 ) { MI_MODE_REG |= MI_MODE_EBUS; }
	if ( ( RegModValue & MI_CLR_DP_INTR ) != 0 ) { MI_INTR_REG &= ~MI_INTR_DP; }
	if ( ( RegModValue & MI_CLR_RDRAM ) != 0 ) { MI_MODE_REG &= ~MI_MODE_RDRAM; }
	if ( ( RegModValue & MI_SET_RDRAM ) != 0 ) { MI_MODE_REG |= MI_MODE_RDRAM; }
}

void ChangeSpStatus (void) {
	if ( ( RegModValue & SP_CLR_HALT ) != 0) { SP_STATUS_REG &= ~SP_STATUS_HALT; }
	if ( ( RegModValue & SP_SET_HALT ) != 0) { SP_STATUS_REG |= SP_STATUS_HALT;  }
	if ( ( RegModValue & SP_CLR_BROKE ) != 0) { SP_STATUS_REG &= ~SP_STATUS_BROKE; }
	if ( ( RegModValue & SP_CLR_INTR ) != 0) { 
		MI_INTR_REG &= ~MI_INTR_SP; 
		CheckInterrupts();
	}
	if (ShowDebugMessages)
		if ( ( RegModValue & SP_SET_INTR ) != 0) { DisplayError("SP_SET_INTR"); }

	if ( ( RegModValue & SP_CLR_SSTEP ) != 0) { SP_STATUS_REG &= ~SP_STATUS_SSTEP; }
	if ( ( RegModValue & SP_SET_SSTEP ) != 0) { SP_STATUS_REG |= SP_STATUS_SSTEP;  }
	if ( ( RegModValue & SP_CLR_INTR_BREAK ) != 0) { SP_STATUS_REG &= ~SP_STATUS_INTR_BREAK; }
	if ( ( RegModValue & SP_SET_INTR_BREAK ) != 0) { SP_STATUS_REG |= SP_STATUS_INTR_BREAK;  }
	if ( ( RegModValue & SP_CLR_SIG0 ) != 0) { SP_STATUS_REG &= ~SP_STATUS_SIG0; }
	if ( ( RegModValue & SP_SET_SIG0 ) != 0) { SP_STATUS_REG |= SP_STATUS_SIG0;  }
	if ( ( RegModValue & SP_CLR_SIG1 ) != 0) { SP_STATUS_REG &= ~SP_STATUS_SIG1; }
	if ( ( RegModValue & SP_SET_SIG1 ) != 0) { SP_STATUS_REG |= SP_STATUS_SIG1;  }
	if ( ( RegModValue & SP_CLR_SIG2 ) != 0) { SP_STATUS_REG &= ~SP_STATUS_SIG2; }
	if ( ( RegModValue & SP_SET_SIG2 ) != 0) { SP_STATUS_REG |= SP_STATUS_SIG2;  }
	if ( ( RegModValue & SP_CLR_SIG3 ) != 0) { SP_STATUS_REG &= ~SP_STATUS_SIG3; }
	if ( ( RegModValue & SP_SET_SIG3 ) != 0) { SP_STATUS_REG |= SP_STATUS_SIG3;  }
	if ( ( RegModValue & SP_CLR_SIG4 ) != 0) { SP_STATUS_REG &= ~SP_STATUS_SIG4; }
	if ( ( RegModValue & SP_SET_SIG4 ) != 0) { SP_STATUS_REG |= SP_STATUS_SIG4;  }
	if ( ( RegModValue & SP_CLR_SIG5 ) != 0) { SP_STATUS_REG &= ~SP_STATUS_SIG5; }
	if ( ( RegModValue & SP_SET_SIG5 ) != 0) { SP_STATUS_REG |= SP_STATUS_SIG5;  }
	if ( ( RegModValue & SP_CLR_SIG6 ) != 0) { SP_STATUS_REG &= ~SP_STATUS_SIG6; }
	if ( ( RegModValue & SP_SET_SIG6 ) != 0) { SP_STATUS_REG |= SP_STATUS_SIG6;  }
	if ( ( RegModValue & SP_CLR_SIG7 ) != 0) { SP_STATUS_REG &= ~SP_STATUS_SIG7; }
	if ( ( RegModValue & SP_SET_SIG7 ) != 0) { SP_STATUS_REG |= SP_STATUS_SIG7;  }

	if ( ( RegModValue & SP_SET_SIG0 ) != 0 && AudioSignal)
	{
		MI_INTR_REG |= MI_INTR_SP; 
		CheckInterrupts();				
	}

	// Automated Delay RSP / Delay RDP, based on information from Mupen64 Plus HLE RSP source code
	// If the ucode boot size (DMEM + 0xFED) is not within 0 to 1000 then assume the rsp is being called by the operating system
	// The next bit checks to make sure either the GFX (1) or RSP (2) is being called to do work
	/*if (!(*( DWORD *)(DMEM + 0xFED) >= 0 && *( DWORD *)(DMEM + 0xFED) <= 1000) && 
		(*( DWORD *)(DMEM + 0xFC0) == 1 || *( DWORD *)(DMEM + 0xFC0) == 2)) {
		if ((SP_STATUS_REG & SP_STATUS_HALT) == 0) {
			ChangeTimer(RspTimer, 0x900);
			return;
		}
	}*/
	/*
	if (DelayRDP == TRUE && *( DWORD *)(DMEM + 0xFC0) == 1) {
		int j = 0;
		//ChangeTimer(RspTimer, 0x9000);
		//return;
	}
	/*
	if (/*DelayRSP == TRUE &&*/ /*DlistCount == 0 && *( DWORD *)(DMEM + 0xFC0) == 2) {
		ChangeTimer(RspTimer, 0x900);
		return;
	}*/

	RunRsp();
}

void ChangeDpcStatus (void) {
	if ( ( RegModValue & DPC_CLR_XBUS_DMEM_DMA ) != 0) { DPC_STATUS_REG &= ~DPC_STATUS_XBUS_DMEM_DMA; }
	if ( ( RegModValue & DPC_SET_XBUS_DMEM_DMA ) != 0) { DPC_STATUS_REG |= DPC_STATUS_XBUS_DMEM_DMA;  }
	if ( ( RegModValue & DPC_CLR_FREEZE ) != 0) { DPC_STATUS_REG &= ~DPC_STATUS_FREEZE; }
	if ( ( RegModValue & DPC_SET_FREEZE ) != 0) { DPC_STATUS_REG |= DPC_STATUS_FREEZE;  }
	if ( ( RegModValue & DPC_CLR_FLUSH ) != 0) { DPC_STATUS_REG &= ~DPC_STATUS_FLUSH; }
	if ( ( RegModValue & DPC_SET_FLUSH ) != 0) { DPC_STATUS_REG |= DPC_STATUS_FLUSH;  }
	if ( ( RegModValue & DPC_CLR_FREEZE ) != 0)
	{
		if ( ( SP_STATUS_REG & SP_STATUS_HALT ) == 0) 
		{
			if ( ( SP_STATUS_REG & SP_STATUS_BROKE ) == 0 ) 
			{
				RunRsp();
			}
		}
	}
}

int Free8BitX86Reg (BLOCK_SECTION * Section) {
	int x86Reg, count, MapCount[10], MapReg[10];
	
	if (x86Mapped(x86_EBX) == NotMapped && !x86Protected(x86_EBX)) {return x86_EBX; }
	if (x86Mapped(x86_EAX) == NotMapped && !x86Protected(x86_EAX)) {return x86_EAX; }
	if (x86Mapped(x86_EDX) == NotMapped && !x86Protected(x86_EDX)) {return x86_EDX; }
	if (x86Mapped(x86_ECX) == NotMapped && !x86Protected(x86_ECX)) {return x86_ECX; }

	x86Reg = UnMap_8BitTempReg(Section);
	if (x86Reg > 0) { return x86Reg; }
	
	for (count = 0; count < 10; count ++) {
		MapCount[count] = x86MapOrder(count);
		MapReg[count] = count;
	}
	for (count = 0; count < 10; count ++) {
		int i;
		
		for (i = 0; i < 9; i ++) {
			int temp;

			if (MapCount[i] < MapCount[i+1]) {
				temp = MapCount[i];
				MapCount[i] = MapCount[i+1];
				MapCount[i+1] = temp;
				temp = MapReg[i];
				MapReg[i] = MapReg[i+1];
				MapReg[i+1] = temp;
			}
		}

	}
	for (count = 0; count < 10; count ++) {
		if (MapCount[count] > 0) {			
			if (!Is8BitReg(count)) {  continue; }
			if (UnMap_X86reg(Section,count)) {
				return count;
			}
		}
	}

	return -1;
}

int FreeX86Reg (BLOCK_SECTION * Section) {
	int x86Reg, count, MapCount[10], MapReg[10], StackReg;

	if (x86Mapped(x86_EDI) == NotMapped && !x86Protected(x86_EDI)) {return x86_EDI; }
	if (x86Mapped(x86_ESI) == NotMapped && !x86Protected(x86_ESI)) {return x86_ESI; }
	if (x86Mapped(x86_EBX) == NotMapped && !x86Protected(x86_EBX)) {return x86_EBX; }
	if (x86Mapped(x86_EAX) == NotMapped && !x86Protected(x86_EAX)) {return x86_EAX; }
	if (x86Mapped(x86_EDX) == NotMapped && !x86Protected(x86_EDX)) {return x86_EDX; }
	if (x86Mapped(x86_ECX) == NotMapped && !x86Protected(x86_ECX)) {return x86_ECX; }

	x86Reg = UnMap_TempReg(Section);
	if (x86Reg > 0) { return x86Reg; }

	for (count = 0; count < 10; count ++) {
		MapCount[count] = x86MapOrder(count);
		MapReg[count] = count;
	}
	for (count = 0; count < 10; count ++) {
		int i;
		
		for (i = 0; i < 9; i ++) {
			int temp;

			if (MapCount[i] < MapCount[i+1]) {
				temp = MapCount[i];
				MapCount[i] = MapCount[i+1];
				MapCount[i+1] = temp;
				temp = MapReg[i];
				MapReg[i] = MapReg[i+1];
				MapReg[i+1] = temp;
			}
		}

	}
	StackReg = -1;
	for (count = 0; count < 10; count ++) {
		if (MapCount[count] > 0 && x86Mapped(MapReg[count]) != Stack_Mapped) {
			if (UnMap_X86reg(Section,MapReg[count])) {
				return MapReg[count];
			}			
		}
		if (x86Mapped(MapReg[count]) == Stack_Mapped) { StackReg = MapReg[count]; }
	}
	if (StackReg > 0) {
		UnMap_X86reg(Section,StackReg);
		return StackReg;
	}	
	return -1;
}

void InitalizeR4300iRegisters (int UsePif, int Country, enum CIC_CHIP CIC_Chip) {
	memset(CP0,0,sizeof(Registers.CP0));	
	memset(FPCR,0,sizeof(Registers.FPCR));
	for (int i = 0; i < 4; ++i) {
		memset((*RegRDRAM)[i], 0, sizeof(Registers.RDRAM[i]));
		RDRAM_DEVICE_TYPE_REG(i) = (RDRAM_DEVICE_TYPE_COLUMN_BITS << 28) |
			(RDRAM_DEVICE_TYPE_BN << 26) |
			(RDRAM_DEVICE_TYPE_EN << 24) |
			(RDRAM_DEVICE_TYPE_BANK_BITS << 20) |
			(RDRAM_DEVICE_TYPE_ROW_BITS << 16) |
			(RDRAM_DEVICE_TYPE_VERSION << 4) |
			(RDRAM_DEVICE_TYPE_TYPE << 0);
		RDRAM_DELAY_REG(i) = RDRAM_DELAY_FIXED_VALUE;
		RDRAM_DEVICE_MANUF_REG(i) = RDRAM_DEVICE_MANUFACTURER_NEC;
	}
	memset(RegSP,0,sizeof(Registers.SP));	
	memset(RegDPC,0,sizeof(Registers.DPC));	
	memset(RegMI,0,sizeof(Registers.MI));	
	memset(RegVI,0,sizeof(Registers.VI));	
	memset(RegAI,0,sizeof(Registers.AI));	
	memset(RegPI,0,sizeof(Registers.PI));	
	memset(RegRI,0,sizeof(Registers.RI));	
	memset(RegSI,0,sizeof(Registers.SI));	
	memset(GPR,0,sizeof(Registers.GPR));	
	memset(FPR,0,sizeof(Registers.FPR));	
	
	if (CIC_Chip < 0) {
		if (ShowDebugMessages)
			DisplayError(GS(MSG_UNKNOWN_CIC_CHIP));
		CIC_Chip = CIC_NUS_6102;
	}
	LO.DW                 = 0x0;
	HI.DW                 = 0x0;
	RANDOM_REGISTER	  = 0x1F;
	COUNT_REGISTER	  = 0x5000;
	MI_VERSION_REG	  = 0x02020102;
	SP_STATUS_REG      = 0x00000001;
	CAUSE_REGISTER	  = 0xB000005C;
	//ENTRYHI_REGISTER	  = 0xFFFFE0FF;
	CONTEXT_REGISTER   = 0x007FFFF0;
	EPC_REGISTER       = 0xFFFFFFFF;
	BAD_VADDR_REGISTER = 0xFFFFFFFF;
	ERROREPC_REGISTER  = 0xFFFFFFFF;
	CONFIG_REGISTER     = 0x7006E463;
	REVISION_REGISTER   = 0x00000A00;
	STATUS_REGISTER     = 0x34000000;
	Addressing64Bits = 0;
	KernelMode = TRUE;
	PRID_REGISTER       = 0xB22;
	lastUnusedCOP0Register = -1;
	SetFpuLocations();
	CheckRdramStatus();
	cop2LatchedValue.UDW = 0;
	if (UsePif) {
		PROGRAM_COUNTER.DW = (int)0xBFC00000;			
		switch (CIC_Chip) {
		case CIC_NUS_6101:
			PIF_Ram[36] = 0x00;
			PIF_Ram[37] = 0x06;
			PIF_Ram[38] = 0x3F;
			PIF_Ram[39] = 0x3F;
			break;
		case CIC_NUS_6102:
			PIF_Ram[36] = 0x00;
			PIF_Ram[37] = 0x02;
			PIF_Ram[38] = 0x3F;
			PIF_Ram[39] = 0x3F;
			break;
		case CIC_NUS_6103:			
			PIF_Ram[36] = 0x00;
			PIF_Ram[37] = 0x02;
			PIF_Ram[38] = 0x78;
			PIF_Ram[39] = 0x3F;
			break;
		case CIC_NUS_6105:			
			PIF_Ram[36] = 0x00;
			PIF_Ram[37] = 0x02;
			PIF_Ram[38] = 0x91;
			PIF_Ram[39] = 0x3F;
			break;
		case CIC_NUS_6106:			
			PIF_Ram[36] = 0x00;
			PIF_Ram[37] = 0x02;
			PIF_Ram[38] = 0x85;
			PIF_Ram[39] = 0x3F;
			break;
		}
	} else {
		memcpy( (N64MEM+0x4000040), (ROM + 0x040), 0xFBC);
		PROGRAM_COUNTER.DW = (int)0xA4000040;	
		
		GPR[0].DW=0x0000000000000000;
		GPR[6].DW=0xFFFFFFFFA4001F0C;
		GPR[7].DW=0xFFFFFFFFA4001F08;
		GPR[8].DW=0x00000000000000C0;
		GPR[9].DW=0x0000000000000000;
		GPR[10].DW=0x0000000000000040;
		GPR[11].DW=0xFFFFFFFFA4000040;
		GPR[16].DW=0x0000000000000000;
		GPR[17].DW=0x0000000000000000;
		GPR[18].DW=0x0000000000000000;
		GPR[19].DW=0x0000000000000000;
		GPR[21].DW=0x0000000000000000; 
		GPR[26].DW=0x0000000000000000;
		GPR[27].DW=0x0000000000000000;
		GPR[28].DW=0x0000000000000000;
		GPR[29].DW=0xFFFFFFFFA4001FF0;
		GPR[30].DW=0x0000000000000000;

		switch (GetRomRegion(ROM)) {
		case PAL_Region:
			switch (CIC_Chip) {
			case CIC_NUS_6102:
				GPR[5].DW=0xFFFFFFFFC0F1D859;
				GPR[14].DW=0x000000002DE108EA;
				GPR[24].DW=0x0000000000000000;
				break;
			case CIC_NUS_6103:
				GPR[5].DW=0xFFFFFFFFD4646273;
				GPR[14].DW=0x000000001AF99984;
				GPR[24].DW=0x0000000000000000;
				break;
			case CIC_NUS_6105:
				*(DWORD *)&IMEM[0x04] = 0xBDA807FC;
				GPR[5].DW=0xFFFFFFFFDECAAAD1;
				GPR[14].DW=0x000000000CF85C13;
				GPR[24].DW=0x0000000000000002;
				break;
			case CIC_NUS_6106:
				GPR[5].DW=0xFFFFFFFFB04DC903;
				GPR[14].DW=0x000000001AF99984;
				GPR[24].DW=0x0000000000000002;
				break;
			}

			GPR[20].DW=0x0000000000000000;
			GPR[23].DW=0x0000000000000006;
			GPR[31].DW=0xFFFFFFFFA4001554;
			break;
		case NTSC_Region:
		default:
			switch (CIC_Chip) {
			case CIC_NUS_6102:
				GPR[5].DW=0xFFFFFFFFC95973D5;
				GPR[14].DW=0x000000002449A366;
				break;
			case CIC_NUS_6103:
				GPR[5].DW=0xFFFFFFFF95315A28;
				GPR[14].DW=0x000000005BACA1DF;
				break;
			case CIC_NUS_6105:
				*(DWORD *)&IMEM[0x04] = 0x8DA807FC;
				GPR[5].DW=0x000000005493FB9A;
				GPR[14].DW=0xFFFFFFFFC2C20384;
			case CIC_NUS_6106:
				GPR[5].DW=0xFFFFFFFFE067221F;
				GPR[14].DW=0x000000005CD2B70F;
				break;
			}
			GPR[20].DW=0x0000000000000001;
			GPR[23].DW=0x0000000000000000;
			GPR[24].DW=0x0000000000000003;
			GPR[31].DW=0xFFFFFFFFA4001550;
		}

		switch (CIC_Chip) {
		case CIC_NUS_6101:
			GPR[22].DW=0x000000000000003F; 
			break;
		case CIC_NUS_6102:
			GPR[1].DW=0x0000000000000001;
			GPR[2].DW=0x000000000EBDA536;
			GPR[3].DW=0x000000000EBDA536;
			GPR[4].DW=0x000000000000A536;
			GPR[12].DW=0xFFFFFFFFED10D0B3;
			GPR[13].DW=0x000000001402A4CC;
			GPR[15].DW=0x000000003103E121;
			GPR[22].DW=0x000000000000003F; 
			GPR[25].DW=0xFFFFFFFF9DEBB54F;
			break;
		case CIC_NUS_6103:
			GPR[1].DW=0x0000000000000001;
			GPR[2].DW=0x0000000049A5EE96;
			GPR[3].DW=0x0000000049A5EE96;
			GPR[4].DW=0x000000000000EE96;
			GPR[12].DW=0xFFFFFFFFCE9DFBF7;
			GPR[13].DW=0xFFFFFFFFCE9DFBF7;
			GPR[15].DW=0x0000000018B63D28;
			GPR[22].DW=0x0000000000000078; 
			GPR[25].DW=0xFFFFFFFF825B21C9;
			break;
		case CIC_NUS_6105:
			*(DWORD *)&IMEM[0x00] = 0x3C0DBFC0;
			*(DWORD *)&IMEM[0x08] = 0x25AD07C0;
			*(DWORD *)&IMEM[0x0C] = 0x31080080;
			*(DWORD *)&IMEM[0x10] = 0x5500FFFC;
			*(DWORD *)&IMEM[0x14] = 0x3C0DBFC0;
			*(DWORD *)&IMEM[0x18] = 0x8DA80024;
			*(DWORD *)&IMEM[0x1C] = 0x3C0BB000;
			GPR[1].DW=0x0000000000000000;
			GPR[2].DW=0xFFFFFFFFF58B0FBF;
			GPR[3].DW=0xFFFFFFFFF58B0FBF;
			GPR[4].DW=0x0000000000000FBF;
			GPR[12].DW=0xFFFFFFFF9651F81E;
			GPR[13].DW=0x000000002D42AAC5;
			GPR[15].DW=0x0000000056584D60;
			GPR[22].DW=0x0000000000000091; 
			GPR[25].DW=0xFFFFFFFFCDCE565F;
			break;
		case CIC_NUS_6106:
			GPR[1].DW=0x0000000000000000;
			GPR[2].DW=0xFFFFFFFFA95930A4;
			GPR[3].DW=0xFFFFFFFFA95930A4;
			GPR[4].DW=0x00000000000030A4;
			GPR[12].DW=0xFFFFFFFFBCB59510;
			GPR[13].DW=0xFFFFFFFFBCB59510;
			GPR[15].DW=0x000000007A3C07F4;
			GPR[22].DW=0x0000000000000085; 
			GPR[25].DW=0x00000000465E3F72;
			break;
		case CIC_NUS_8303:
		case CIC_NUS_5167:
			GPR[22].DW = 0x00000000000000DD;
			break;
		case CIC_NUS_DDUS:
			GPR[22].DW = 0x00000000000000DE;
			break;
		case CIC_NUS_8401:
			GPR[1].DW=0x0000000000000001;
			GPR[2].DW=0x000000000EBDA536;
			GPR[3].DW=0x000000000EBDA536;
			GPR[4].DW=0x000000000000A536;
			GPR[12].DW=0xFFFFFFFFED10D0B3;
			GPR[13].DW=0x000000001402A4CC;
			GPR[15].DW=0x000000003103E121;
			GPR[22].DW=0x00000000000000DD; 
			GPR[25].DW=0xFFFFFFFF9DEBB54F;
			break;
		}
	}
#ifdef Interpreter_StackTest
	StackValue = GPR[29].W[0];
#endif
	MemoryStack = (DWORD)(N64MEM+(GPR[29].W[0] & 0x1FFFFFFF));
}

BOOL Is8BitReg (int x86Reg) {
	if (x86Reg == x86_EAX) { return TRUE; }
	if (x86Reg == x86_EBX) { return TRUE; }
	if (x86Reg == x86_ECX) { return TRUE; }
	if (x86Reg == x86_EDX) { return TRUE; }
	return FALSE;
}

void Load_FPR_ToTop (BLOCK_SECTION * Section, int Reg, int RegToLoad, int Format) {
	int i;

	if (RegToLoad < 0) { DisplayError("Load_FPR_ToTop\nRegToLoad < 0 ???"); return; }
	if (Reg < 0) { DisplayError("Load_FPR_ToTop\nReg < 0 ???"); return; }

	if (Format == FPU_Double || Format == FPU_Qword) {
		UnMap_FPR(Section,Reg + 1,TRUE);
		UnMap_FPR(Section,RegToLoad + 1,TRUE);
	} else {
		if ((Reg & 1) != 0) {
			for (i = 0; i < 8; i++) {
				if (FpuMappedTo(i) == (DWORD)(Reg - 1)) {
					if (FpuState(i) == FPU_Double || FpuState(i) == FPU_Qword) {
						UnMap_FPR(Section,Reg,TRUE);
					}
					i = 8;
				}
			}		
		}
		if ((RegToLoad & 1) != 0) {
			for (i = 0; i < 8; i++) {
				if (FpuMappedTo(i) == (DWORD)(RegToLoad - 1)) {
					if (FpuState(i) == FPU_Double || FpuState(i) == FPU_Qword) {
						UnMap_FPR(Section,RegToLoad,TRUE);
					}
					i = 8;
				}
			}		
		}
	}

	if (Reg == RegToLoad) {
		//if different format then unmap original reg from stack
		for (i = 0; i < 8; i++) {
			if (FpuMappedTo(i) == (DWORD)Reg) {
				if (FpuState(i) != (DWORD)Format) {
					UnMap_FPR(Section,Reg,TRUE);
				}
				i = 8;
			}
		}
	} else {
		UnMap_FPR(Section,Reg,FALSE);
	}

	if (RegInStack(Section,RegToLoad,Format)) {
		if (Reg != RegToLoad) {
			if (FpuMappedTo((StackTopPos - 1) & 7) != (DWORD)RegToLoad) {
				UnMap_FPR(Section,FpuMappedTo((StackTopPos - 1) & 7),TRUE);
				CPU_Message("    regcache: allocate ST(0) to %s", FPR_Name[Reg]);
				fpuLoadReg(&StackTopPos,StackPosition(Section,RegToLoad));		
				FpuRoundingModel(StackTopPos) = RoundDefault;
				FpuMappedTo(StackTopPos)      = Reg;
				FpuState(StackTopPos)         = Format;
			} else {
				UnMap_FPR(Section,FpuMappedTo((StackTopPos - 1) & 7),TRUE);
				Load_FPR_ToTop (Section,Reg, RegToLoad, Format);
			}
		} else {
			DWORD RegPos, StackPos, i;

			for (i = 0; i < 8; i++) {
				if (FpuMappedTo(i) == (DWORD)Reg) {
					RegPos = i;
					i = 8;
				}
			}

			if (RegPos == StackTopPos) {
				return;
			}
			StackPos = StackPosition(Section,Reg);

			FpuRoundingModel(RegPos) = FpuRoundingModel(StackTopPos);
			FpuMappedTo(RegPos)      = FpuMappedTo(StackTopPos);
			FpuState(RegPos)         = FpuState(StackTopPos);
			CPU_Message("    regcache: allocate ST(%d) to %s", StackPos,FPR_Name[FpuMappedTo(RegPos)]);
			CPU_Message("    regcache: allocate ST(0) to %s", FPR_Name[Reg]);

			fpuExchange(StackPos);

			FpuRoundingModel(StackTopPos) = RoundDefault;
			FpuMappedTo(StackTopPos)      = Reg;
			FpuState(StackTopPos)         = Format;
		}
	} else {
		char Name[50];
		int TempReg;

		UnMap_FPR(Section,FpuMappedTo((StackTopPos - 1) & 7),TRUE);
		for (i = 0; i < 8; i++) {
			if (FpuMappedTo(i) == (DWORD)RegToLoad) {
				UnMap_FPR(Section,RegToLoad,TRUE);
				i = 8;
			}
		}
		CPU_Message("    regcache: allocate ST(0) to %s", FPR_Name[Reg]);
		TempReg = Map_TempReg(Section,x86_Any,-1,FALSE);
		switch (Format) {
		case FPU_Dword:
			sprintf(Name,"FPRFloatLocation[%d]",RegToLoad);
			MoveVariableToX86reg(&FPRFloatLoadStoreLocation[RegToLoad],Name,TempReg);
			fpuLoadIntegerDwordFromX86Reg(&StackTopPos,TempReg);
			break;
		case FPU_Qword:
			sprintf(Name,"FPRDoubleLocation[%d]",RegToLoad);
			MoveVariableToX86reg(&FPRDoubleLocation[RegToLoad],Name,TempReg);
			fpuLoadIntegerQwordFromX86Reg(&StackTopPos,TempReg);
			break;
		case FPU_Float:
			sprintf(Name,"FPRFloatLocation[%d]",RegToLoad);
			MoveVariableToX86reg(&FPRFloatLoadStoreLocation[RegToLoad],Name,TempReg);
			fpuLoadDwordFromX86Reg(&StackTopPos,TempReg);
			break;
		case FPU_Double:
			sprintf(Name,"FPRDoubleLocation[%d]",RegToLoad);
			MoveVariableToX86reg(&FPRDoubleLocation[RegToLoad],Name,TempReg);
			fpuLoadQwordFromX86Reg(&StackTopPos,TempReg);
			break;
		default:
			if (ShowDebugMessages)
				DisplayError("Load_FPR_ToTop\nUnkown format to load %d",Format);
		}
		x86Protected(TempReg) = FALSE;
		FpuRoundingModel(StackTopPos) = RoundDefault;
		FpuMappedTo(StackTopPos)      = Reg;
		FpuState(StackTopPos)         = Format;
	}
	CPU_Message("CurrentRoundingModel: %d  FpuRoundingModel(StackTopPos): %d",
		CurrentRoundingModel,FpuRoundingModel(StackTopPos));
}

void Map_GPR_32bit (BLOCK_SECTION * Section, int Reg, BOOL SignValue, int MipsRegToLoad) {
	int x86Reg,count;

	if (Reg == 0) {
		if (ShowDebugMessages)
			DisplayError("Map_GPR_32bit\n\nWhy are you trying to map reg 0");
		return;
	}

	if (IsUnknown(Reg) || IsConst(Reg)) {		
		x86Reg = FreeX86Reg(Section);		
		if (x86Reg < 0) { 
			if (ShowDebugMessages) {
				DisplayError("Map_GPR_32bit\n\nOut of registers");
				_asm int 3
			}
			return; 
		}		
		CPU_Message("    regcache: allocate %s to %s",x86_Name(x86Reg),GPR_Name[Reg]);
	} else {
		if (Is64Bit(Reg)) { 
			CPU_Message("    regcache: unallocate %s from high 32bit of %s",x86_Name(MipsRegHi(Reg)),GPR_NameHi[Reg]);
			x86MapOrder(MipsRegHi(Reg)) = 0;
			x86Mapped(MipsRegHi(Reg)) = NotMapped;
			x86Protected(MipsRegHi(Reg)) = FALSE;
			MipsRegHi(Reg) = 0;
		}
		x86Reg = MipsRegLo(Reg);
	}
	for (count = 0; count < 10; count ++) {
		if (x86MapOrder(count) > 0) { 
			x86MapOrder(count) += 1;
		}
	}
	x86MapOrder(x86Reg) = 1;
	
	if (MipsRegToLoad > 0) {
		if (IsUnknown(MipsRegToLoad)) {
			MoveVariableToX86reg(&GPR[MipsRegToLoad].UW[0],GPR_NameLo[MipsRegToLoad],x86Reg);
		} else if (IsMapped(MipsRegToLoad)) {
			if (Reg != MipsRegToLoad) {
				MoveX86RegToX86Reg(MipsRegLo(MipsRegToLoad),x86Reg);
			}
		} else {
			if (MipsRegLo(MipsRegToLoad) != 0) {
				MoveConstToX86reg(MipsRegLo(MipsRegToLoad),x86Reg);
			} else {
				XorX86RegToX86Reg(x86Reg,x86Reg);
			}
		}
	} else if (MipsRegToLoad == 0) {
		XorX86RegToX86Reg(x86Reg,x86Reg);
	}
	x86Mapped(x86Reg) = GPR_Mapped;
	x86Protected(x86Reg) = TRUE;
	MipsRegLo(Reg) = x86Reg;
	MipsRegState(Reg) = SignValue ? STATE_MAPPED_32_SIGN : STATE_MAPPED_32_ZERO;
}

void Map_GPR_64bit (BLOCK_SECTION * Section, int Reg, int MipsRegToLoad) {
	int x86Hi, x86lo, count;

	if (Reg == 0) {
		if (ShowDebugMessages)
			DisplayError("Map_GPR_32bit\n\nWhy are you trying to map reg 0");
		return;
	}

	ProtectGPR(Section,Reg);
	if (IsUnknown(Reg) || IsConst(Reg)) {
		x86Hi = FreeX86Reg(Section);
		if (x86Hi < 0) {  DisplayError("Map_GPR_64bit\n\nOut of registers"); return; }
		x86Protected(x86Hi) = TRUE;

		x86lo = FreeX86Reg(Section);
		if (x86lo < 0) {  DisplayError("Map_GPR_64bit\n\nOut of registers"); return; }
		x86Protected(x86lo) = TRUE;
		
		CPU_Message("    regcache: allocate %s to hi word of %s",x86_Name(x86Hi),GPR_Name[Reg]);
		CPU_Message("    regcache: allocate %s to low word of %s",x86_Name(x86lo),GPR_Name[Reg]);
	} else {
		x86lo = MipsRegLo(Reg);
		if (Is32Bit(Reg)) {
			x86Protected(x86lo) = TRUE;
			x86Hi = FreeX86Reg(Section);
			if (x86Hi < 0) {  DisplayError("Map_GPR_64bit\n\nOut of registers"); return; }
			x86Protected(x86Hi) = TRUE;
		} else {
			x86Hi = MipsRegHi(Reg);
		}
	}
	
	for (count = 0; count < 10; count ++) {
		if (x86MapOrder(count) > 0) { x86MapOrder(count) += 1; }
	}
	
	x86MapOrder(x86Hi) = 1;
	x86MapOrder(x86lo) = 1;
	if (MipsRegToLoad > 0) {
		if (IsUnknown(MipsRegToLoad)) {
			MoveVariableToX86reg(&GPR[MipsRegToLoad].UW[1],GPR_NameHi[MipsRegToLoad],x86Hi);
			MoveVariableToX86reg(&GPR[MipsRegToLoad].UW[0],GPR_NameLo[MipsRegToLoad],x86lo);
		} else if (IsMapped(MipsRegToLoad)) {
			if (Is32Bit(MipsRegToLoad)) {
				if (IsSigned(MipsRegToLoad)) {
					MoveX86RegToX86Reg(MipsRegLo(MipsRegToLoad),x86Hi);
					ShiftRightSignImmed(x86Hi,31);
				} else {
					XorX86RegToX86Reg(x86Hi,x86Hi);
				}
				if (Reg != MipsRegToLoad) {
					MoveX86RegToX86Reg(MipsRegLo(MipsRegToLoad),x86lo);
				}
			} else {
				if (Reg != MipsRegToLoad) {
					MoveX86RegToX86Reg(MipsRegHi(MipsRegToLoad),x86Hi);
					MoveX86RegToX86Reg(MipsRegLo(MipsRegToLoad),x86lo);
				}
			}
		} else {
CPU_Message("Map_GPR_64bit 11");
			if (Is32Bit(MipsRegToLoad)) {
				if (IsSigned(MipsRegToLoad)) {
					if (MipsRegLo((int)MipsRegLo(MipsRegToLoad) >> 31) != 0) {
						MoveConstToX86reg((int)MipsRegLo(MipsRegToLoad) >> 31,x86Hi);
					} else {
						XorX86RegToX86Reg(x86Hi,x86Hi);
					}
				} else {
					XorX86RegToX86Reg(x86Hi,x86Hi);
				}
			} else {
				if (MipsRegHi(MipsRegToLoad) != 0) {
					MoveConstToX86reg(MipsRegHi(MipsRegToLoad),x86Hi);
				} else {
					XorX86RegToX86Reg(x86Hi,x86Hi);
				}
			}
			if (MipsRegLo(MipsRegToLoad) != 0) {
				MoveConstToX86reg(MipsRegLo(MipsRegToLoad),x86lo);
			} else {
				XorX86RegToX86Reg(x86lo,x86lo);
			}
		}
	} else if (MipsRegToLoad == 0) {
		XorX86RegToX86Reg(x86Hi,x86Hi);
		XorX86RegToX86Reg(x86lo,x86lo);
	}
	x86Mapped(x86Hi) = GPR_Mapped;
	x86Mapped(x86lo) = GPR_Mapped;
	MipsRegHi(Reg) = x86Hi;
	MipsRegLo(Reg) = x86lo;
	MipsRegState(Reg) = STATE_MAPPED_64;
}

int Map_MemoryStack (BLOCK_SECTION * Section, BOOL AutoMap) {
	int x86Reg;

	for (x86Reg = 0; x86Reg < 10; x86Reg ++ ) {
		if (x86Mapped(x86Reg) == Stack_Mapped) {
			return x86Reg;
		}
	}
	if (!AutoMap) { return -1; }
	x86Reg = FreeX86Reg(Section);	
	if (x86Reg < 0) {
		if (ShowDebugMessages) {
			DisplayError("Map_MemoryStack\n\nOut of registers");
			_asm int 3
		}
	}
	x86Mapped(x86Reg) = Stack_Mapped;
	CPU_Message("    regcache: allocate %s as Memory Stack",x86_Name(x86Reg));		
	MoveVariableToX86reg(&MemoryStack,"MemoryStack",x86Reg);
	return x86Reg;
}

int Map_TempReg (BLOCK_SECTION * Section, int x86Reg, int MipsReg, BOOL LoadHiWord) {
	int count;

	if (x86Reg == x86_Any) {		
		for (count = 0; count < 10; count ++ ) {
			if (x86Mapped(count) == Temp_Mapped) {
				if (x86Protected(count) == FALSE) { x86Reg = count; }
			}
		}

		if (x86Reg == x86_Any) {
			x86Reg = FreeX86Reg(Section);
			if (x86Reg < 0) {
				if (ShowDebugMessages) {
					DisplayError("Map_TempReg\n\nOut of registers");
					_asm int 3
				}

				x86Reg = FreeX86Reg(Section);
				return -1;
			}
			CPU_Message("    regcache: allocate %s as temp storage",x86_Name(x86Reg));		
		}
	} else if (x86Reg == x86_Any8Bit) {
		if (x86Mapped(x86_EBX) == Temp_Mapped && !x86Protected(x86_EBX)) { x86Reg = x86_EBX; }
		if (x86Mapped(x86_EAX) == Temp_Mapped && !x86Protected(x86_EAX)) { x86Reg = x86_EAX; }
		if (x86Mapped(x86_EDX) == Temp_Mapped && !x86Protected(x86_EDX)) { x86Reg = x86_EDX; }
		if (x86Mapped(x86_ECX) == Temp_Mapped && !x86Protected(x86_ECX)) { x86Reg = x86_ECX; }
		
		if (x86Reg == x86_Any8Bit) {	
			x86Reg = Free8BitX86Reg(Section);
			if (x86Reg < 0) {
				if (ShowDebugMessages) {
					DisplayError("Map_GPR_8bit\n\nOut of registers");
					_asm int 3
				}
				return -1;
			}
		}
	} else {
		int NewReg;

		if (x86Mapped(x86Reg) == GPR_Mapped) {
			if (x86Protected(x86Reg) == TRUE) {
				if (ShowDebugMessages)
					DisplayError("Map_TempReg\nRegister is protected !!!");
				return -1;
			}
			x86Protected(x86Reg) = TRUE;
			NewReg = FreeX86Reg(Section);
			for (count = 1; count < 32; count ++) {
				if (IsMapped(count)) {
					if (MipsRegLo(count) == (DWORD)x86Reg) {
						if (NewReg < 0) {
							UnMap_GPR(Section,count,TRUE);
							count = 32;
							continue;
						}
						CPU_Message("    regcache: change allocation of %s from %s to %s",
							GPR_Name[count],x86_Name(x86Reg),x86_Name(NewReg));
						x86Mapped(NewReg) = GPR_Mapped;
						x86MapOrder(NewReg) = x86MapOrder(x86Reg);
						MipsRegLo(count) = NewReg;
						MoveX86RegToX86Reg(x86Reg,NewReg);
						if (MipsReg == count && LoadHiWord == FALSE) { MipsReg = -1; }
						count = 32;
					}
					if (Is64Bit(count) && MipsRegHi(count) == (DWORD)x86Reg) {
						if (NewReg < 0) {
							UnMap_GPR(Section,count,TRUE);
							count = 32;
							continue;
						}
						CPU_Message("    regcache: change allocation of %s from %s to %s",
							GPR_NameHi[count],x86_Name(x86Reg),x86_Name(NewReg));
						x86Mapped(NewReg) = GPR_Mapped;
						x86MapOrder(NewReg) = x86MapOrder(x86Reg);
						MipsRegHi(count) = NewReg;
						MoveX86RegToX86Reg(x86Reg,NewReg);
						if (MipsReg == count && LoadHiWord == TRUE) { MipsReg = -1; }
						count = 32;
					}
				}
			}
		}
		if (x86Mapped(x86Reg) == Stack_Mapped) {
			UnMap_X86reg(Section,x86Reg);
		}
		CPU_Message("    regcache: allocate %s as temp storage",x86_Name(x86Reg));		
	}
	if (MipsReg >= 0) {
		if (LoadHiWord) {
			if (IsUnknown(MipsReg)) {
				MoveVariableToX86reg(&GPR[MipsReg].UW[1],GPR_NameHi[MipsReg],x86Reg);
			} else if (IsMapped(MipsReg)) {
				if (Is64Bit(MipsReg)) {
					MoveX86RegToX86Reg(MipsRegHi(MipsReg),x86Reg);
				} else if (IsSigned(MipsReg)){
					MoveX86RegToX86Reg(MipsRegLo(MipsReg),x86Reg);
					ShiftRightSignImmed(x86Reg,31);
				} else {
					MoveConstToX86reg(0,x86Reg);
				}
			} else {
				if (Is64Bit(MipsReg)) {
					if (MipsRegHi(MipsReg) != 0) {
						MoveConstToX86reg(MipsRegHi(MipsReg),x86Reg);
					} else {
						XorX86RegToX86Reg(x86Reg,x86Reg);
					}
				} else {
					if ((int)MipsRegLo(MipsReg) >> 31 != 0) {
						MoveConstToX86reg((int)MipsRegLo(MipsReg) >> 31,x86Reg);
					} else {
						XorX86RegToX86Reg(x86Reg,x86Reg);
					}
				}
			}
		} else {
			if (IsUnknown(MipsReg)) {
				MoveVariableToX86reg(&GPR[MipsReg].UW[0],GPR_NameLo[MipsReg],x86Reg);
			} else if (IsMapped(MipsReg)) {
				MoveX86RegToX86Reg(MipsRegLo(MipsReg),x86Reg);
			} else {
				if (MipsRegLo(MipsReg) != 0) {
					MoveConstToX86reg(MipsRegLo(MipsReg),x86Reg);
				} else {
					XorX86RegToX86Reg(x86Reg,x86Reg);
				}
			}
		}
	}
	x86Mapped(x86Reg) = Temp_Mapped;
	x86Protected(x86Reg) = TRUE;
	for (count = 0; count < 10; count ++) {
		if (x86MapOrder(count) > 0) { 
			x86MapOrder(count) += 1;
		}
	}
	x86MapOrder(x86Reg) = 1;
	return x86Reg;
}

void ProtectGPR(BLOCK_SECTION * Section, DWORD Reg) {
	if (IsUnknown(Reg)) { return; }
	if (IsConst(Reg)) { return; }
	if (Is64Bit(Reg)) {
		x86Protected(MipsRegHi(Reg)) = TRUE;
	}
	x86Protected(MipsRegLo(Reg)) = TRUE;
}

BOOL RegInStack(BLOCK_SECTION * Section,int Reg, int Format) {
	int i;

	for (i = 0; i < 8; i++) {
		if (FpuMappedTo(i) == (DWORD)Reg) {
			if (FpuState(i) == (DWORD)Format) { return TRUE; }
			else if (Format == -1) { return TRUE; }
			return FALSE;
		}
	}
	return FALSE;
}

void SetFpuLocations (void) {
	int count;

	if ((STATUS_REGISTER & STATUS_FR) == 0) {
		for (count = 0; count < 32; count++) {
			FPRFloatLoadStoreLocation[count] = (void*)(&FPR[count & ~1].W[count & 1]);
			FPRFloatFSLocation[count] = (void*)(&FPR[count & ~1].W[0]);
			FPRFloatOtherLocation[count] = (void*)(&FPR[count].W[0]);
			FPRFloatUpperHalfLocation[count] = (void*)(&FPR[count].W[1]);
			FPRDoubleLocation[count] = (void*)(&FPR[count & ~1].DW);
			FPRDoubleFTFDLocation[count] = (void*)(&FPR[count].DW);
		}
	} else {
		for (count = 0; count < 32; count++) {
			FPRFloatLoadStoreLocation[count] = (void*)(&FPR[count].W[0]);
			FPRFloatFSLocation[count] = (void*)(&FPR[count].W[0]);
			FPRFloatOtherLocation[count] = (void*)(&FPR[count].W[0]);
			FPRFloatUpperHalfLocation[count] = (void*)(&FPR[count].W[1]);
			FPRDoubleLocation[count] = (void*)(&FPR[count].DW);
			FPRDoubleFTFDLocation[count] = (void*)(&FPR[count].DW);
		}
	}
}


void SetupRegisters(N64_REGISTERS * n64_Registers) {
	PROGRAM_COUNTER = n64_Registers->PROGRAM_COUNTER;
	HI.DW    = n64_Registers->HI.DW;
	LO.DW    = n64_Registers->LO.DW;
	CP0      = n64_Registers->CP0;
	GPR      = n64_Registers->GPR;
	FPR      = n64_Registers->FPR;
	FPCR     = n64_Registers->FPCR;
	RegRDRAM = &n64_Registers->RDRAM;
	RegSP    = n64_Registers->SP;
	RegDPC   = n64_Registers->DPC;
	RegMI    = n64_Registers->MI;
	RegVI    = n64_Registers->VI;
	RegAI    = n64_Registers->AI;
	RegPI    = n64_Registers->PI;
	RegRI    = n64_Registers->RI;
	RegSI    = n64_Registers->SI;
	PIF_Ram  = n64_Registers->PIF_Ram;
	DMAUsed  = n64_Registers->DMAUsed;
}

int StackPosition (BLOCK_SECTION * Section,int Reg) {
	int i;

	for (i = 0; i < 8; i++) {
		if (FpuMappedTo(i) == (DWORD)Reg) {
			return ((i - StackTopPos) & 7);
		}
	}
	return -1;
}

int UnMap_8BitTempReg (BLOCK_SECTION * Section) {
	int count;

	for (count = 0; count < 10; count ++) {
		if (!Is8BitReg(count)) { continue; }
		if (MipsRegState(count) == Temp_Mapped) {
			if (x86Protected(count) == FALSE) {
				CPU_Message("    regcache: unallocate %s from temp storage",x86_Name(count));
				x86Mapped(count) = NotMapped;
				return count;
			}		
		}
	}
	return -1;
}

void UnMap_AllFPRs ( BLOCK_SECTION * Section ) {
	DWORD StackPos;

	for (;;) {
		int i, StartPos;
		StackPos = StackTopPos;
		if (FpuMappedTo(StackTopPos) != -1 ) {
			UnMap_FPR(Section,FpuMappedTo(StackTopPos),TRUE);
			continue;
		}
		//see if any more registers mapped
		StartPos = StackTopPos;
		for (i = 0; i < 8; i++) {
			if (FpuMappedTo((StartPos + i) & 7) != -1 ) { fpuIncStack(&StackTopPos); }
		}
		if (StackPos != StackTopPos) { continue; }
		return;
	}
}

void UnMap_FPR (BLOCK_SECTION * Section, int Reg, int WriteBackValue ) {
	char Name[50];
	int TempReg;
	int i;

	if (Reg < 0) { return; }
	for (i = 0; i < 8; i++) {
		if (FpuMappedTo(i) != (DWORD)Reg) { continue; }
		CPU_Message("    regcache: unallocate %s from ST(%d)",FPR_Name[Reg],(i - StackTopPos + 8) & 7);
		if (WriteBackValue) {
			int RegPos;
			
			if (((i - StackTopPos + 8) & 7) != 0) {
				DWORD RoundingModel, MappedTo, RegState;
				
				RoundingModel = FpuRoundingModel(StackTopPos);
				MappedTo      = FpuMappedTo(StackTopPos);
				RegState      = FpuState(StackTopPos);
				FpuRoundingModel(StackTopPos) = FpuRoundingModel(i);
				FpuMappedTo(StackTopPos)      = FpuMappedTo(i);
				FpuState(StackTopPos)         = FpuState(i);
				FpuRoundingModel(i) = RoundingModel; 
				FpuMappedTo(i)      = MappedTo;
				FpuState(i)         = RegState;
				fpuExchange((i - StackTopPos) & 7);
			}
			
			CPU_Message("CurrentRoundingModel: %d  FpuRoundingModel(i): %d",
				CurrentRoundingModel,FpuRoundingModel(i));

			if (CurrentRoundingModel != FpuRoundingModel(i)) {
				int x86reg;

				fpuControl = 0;			
				fpuStoreControl(&fpuControl, "fpuControl");
				x86reg = Map_TempReg(Section,x86_Any,-1,FALSE);
				MoveVariableToX86reg(&fpuControl, "fpuControl", x86reg);
				AndConstToX86Reg(x86reg, 0xF3FF);
				
				switch (FpuRoundingModel(i)) {
				case RoundDefault: OrVariableToX86Reg(&FPU_RoundingMode,"FPU_RoundingMode", x86reg); break;
				case RoundTruncate: OrConstToX86Reg(0x0C00, x86reg); break;
				case RoundNearest: /*OrConstToX86Reg(0x0000, x86reg);*/ break;
				case RoundDown: OrConstToX86Reg(0x0400, x86reg); break;
				case RoundUp: OrConstToX86Reg(0x0800, x86reg); break;
				default:
					if (ShowDebugMessages)
						DisplayError("Unknown Rounding model");
				}
				MoveX86regToVariable(x86reg, &fpuControl, "fpuControl");
				fpuLoadControl(&fpuControl, "fpuControl");
				CurrentRoundingModel = FpuRoundingModel(i);
			}

			RegPos = StackTopPos;
			TempReg = Map_TempReg(Section,x86_Any,-1,FALSE);
			switch (FpuState(StackTopPos)) {
			case FPU_Dword: 
				sprintf(Name,"FPRFloatLocation[%d]",FpuMappedTo(StackTopPos));
				MoveVariableToX86reg(&FPRFloatLoadStoreLocation[FpuMappedTo(StackTopPos)],Name,TempReg);
				fpuStoreIntegerDwordFromX86Reg(&StackTopPos,TempReg, TRUE); 
				break;
			case FPU_Qword: 
				sprintf(Name,"FPRDoubleLocation[%d]",FpuMappedTo(StackTopPos));
				MoveVariableToX86reg(&FPRDoubleLocation[FpuMappedTo(StackTopPos)],Name,TempReg);
				fpuStoreIntegerQwordFromX86Reg(&StackTopPos,TempReg, TRUE); 
				break;
			case FPU_Float: 
				sprintf(Name,"FPRFloatLocation[%d]",FpuMappedTo(StackTopPos));
				MoveVariableToX86reg(&FPRFloatLoadStoreLocation[FpuMappedTo(StackTopPos)],Name,TempReg);
				fpuStoreDwordFromX86Reg(&StackTopPos,TempReg, TRUE); 
				break;
			case FPU_Double: 
				sprintf(Name,"FPRDoubleLocation[%d]",FpuMappedTo(StackTopPos));
				MoveVariableToX86reg(&FPRDoubleLocation[FpuMappedTo(StackTopPos)],Name,TempReg);
				fpuStoreQwordFromX86Reg(&StackTopPos,TempReg, TRUE); 
				break;
			default:
				if (ShowDebugMessages)
					DisplayError("UnMap_FPR\nUnknown format to load %d",FpuState(StackTopPos));
			}
			x86Protected(TempReg) = FALSE;
			FpuRoundingModel(RegPos) = RoundDefault;
			FpuMappedTo(RegPos)      = -1;
			FpuState(RegPos)         = FPU_Unkown;
		} else {				
			fpuFree((i - StackTopPos) & 7);
			FpuRoundingModel(i) = RoundDefault;
			FpuMappedTo(i)      = -1;
			FpuState(i)         = FPU_Unkown;
		}
		return;
	}
}

void UnMap_GPR (BLOCK_SECTION * Section, DWORD Reg, int WriteBackValue) {
	if (Reg == 0) {
		if (ShowDebugMessages)
			DisplayError("UnMap_GPR\n\nWhy are you trying to unmap reg 0");
		return;
	}

	if (IsUnknown(Reg)) { return; }
	//CPU_Message("UnMap_GPR: State: %X\tReg: %s\tWriteBack: %s",State,GPR_Name[Reg],WriteBackValue?"TRUE":"FALSE");
	if (IsConst(Reg)) { 
		if (!WriteBackValue) { 
			MipsRegState(Reg) = STATE_UNKNOWN;
			return; 
		}
		if (Is64Bit(Reg)) {
			MoveConstToVariable(MipsRegHi(Reg),&GPR[Reg].UW[1],GPR_NameHi[Reg]);
			MoveConstToVariable(MipsRegLo(Reg),&GPR[Reg].UW[0],GPR_NameLo[Reg]);
			MipsRegState(Reg) = STATE_UNKNOWN;
			return;
		}
		if ((MipsRegLo(Reg) & 0x80000000) != 0) {
			MoveConstToVariable(0xFFFFFFFF,&GPR[Reg].UW[1],GPR_NameHi[Reg]);
		} else {
			MoveConstToVariable(0,&GPR[Reg].UW[1],GPR_NameHi[Reg]);
		}
		MoveConstToVariable(MipsRegLo(Reg),&GPR[Reg].UW[0],GPR_NameLo[Reg]);
		MipsRegState(Reg) = STATE_UNKNOWN;
		return;
	}
	if (Is64Bit(Reg)) {
		CPU_Message("    regcache: unallocate %s from %s",x86_Name(MipsRegHi(Reg)),GPR_NameHi[Reg]);
		x86Mapped(MipsRegHi(Reg)) = NotMapped;
		x86Protected(MipsRegHi(Reg)) = FALSE;
	}
	CPU_Message("    regcache: unallocate %s from %s",x86_Name(MipsRegLo(Reg)),GPR_NameLo[Reg]);
	x86Mapped(MipsRegLo(Reg)) = NotMapped;
	x86Protected(MipsRegLo(Reg)) = FALSE;
	if (!WriteBackValue) { 
		MipsRegState(Reg) = STATE_UNKNOWN;
		return; 
	}
	MoveX86regToVariable(MipsRegLo(Reg),&GPR[Reg].UW[0],GPR_NameLo[Reg]);
	if (Is64Bit(Reg)) {
		MoveX86regToVariable(MipsRegHi(Reg),&GPR[Reg].UW[1],GPR_NameHi[Reg]);
	} else {
		if (IsSigned(Reg)) {
			ShiftRightSignImmed(MipsRegLo(Reg),31);
			MoveX86regToVariable(MipsRegLo(Reg),&GPR[Reg].UW[1],GPR_NameHi[Reg]);
		} else {
			MoveConstToVariable(0,&GPR[Reg].UW[1],GPR_NameHi[Reg]);
		}
	}
	MipsRegState(Reg) = STATE_UNKNOWN;
}

int UnMap_TempReg (BLOCK_SECTION * Section) {
	int count;

	for (count = 0; count < 10; count ++) {
		if (x86Mapped(count) == Temp_Mapped) {
			if (x86Protected(count) == FALSE) {
				CPU_Message("    regcache: unallocate %s from temp storage",x86_Name(count));
				x86Mapped(count) = NotMapped;
				return count;
			}		
		}
	}
	return -1;
}

BOOL UnMap_X86reg (BLOCK_SECTION * Section, DWORD x86Reg) {
	int count;

	if (x86Mapped(x86Reg) == NotMapped && x86Protected(x86Reg) == FALSE) { return TRUE; }
	if (x86Mapped(x86Reg) == Temp_Mapped) { 
		if (x86Protected(x86Reg) == FALSE) {
			CPU_Message("    regcache: unallocate %s from temp storage",x86_Name(x86Reg));
			x86Mapped(x86Reg) = NotMapped;
			return TRUE;
		}
		return FALSE;
	}
	for (count = 1; count < 32; count ++) {
		if (IsMapped(count)) {
			if (Is64Bit(count)) {
				if (MipsRegHi(count) == x86Reg) {
					if (x86Protected(x86Reg) == FALSE) {
						UnMap_GPR(Section,count,TRUE);
						return TRUE;
					}
					break;
				}
			} 
			if (MipsRegLo(count) == x86Reg) {
				if (x86Protected(x86Reg) == FALSE) {
					UnMap_GPR(Section,count,TRUE);
					return TRUE;
				}
				break;
			}
		}
	}
	if (x86Mapped(x86Reg) == Stack_Mapped) { 
		CPU_Message("    regcache: unallocate %s from Memory Stack",x86_Name(x86Reg));
		MoveX86regToVariable(x86Reg,&MemoryStack,"MemoryStack");
		x86Mapped(x86Reg) = NotMapped;
		return TRUE;
	}
	return FALSE;
}

void UnProtectGPR(BLOCK_SECTION * Section, DWORD Reg) {
	if (IsUnknown(Reg)) { return; }
	if (IsConst(Reg)) { return; }
	if (Is64Bit(Reg)) {
		x86Protected(MipsRegHi(Reg)) = FALSE;
	}
	x86Protected(MipsRegLo(Reg)) = FALSE;
}

void UpdateCurrentHalfLine (void) {
	if (CPU_Type == CPU_SyncCores) {
		HalfLine = 0;
		return;
	}
    if (Timers.Timer < 0) {
		//LogMessage("negative timer");
		HalfLine = 0;
		return;
	}
	//DisplayError("Timer: %X",Timers.Timer);
	//HalfLine = (Timer / 1500) + VI_INTR_REG;
	HalfLine = (Timers.Timer / RomModVIS);
	HalfLine &= ~1;
	HalfLine |= ViFieldSerration;
	VI_V_CURRENT_LINE_REG = HalfLine;
	//Timers.Timer -= 1500;
}

void UpdateFieldSerration(int interlaced)
{
	ViFieldSerration ^= 1;
	ViFieldSerration &= interlaced;
}

/*void WriteBackRegisters (BLOCK_SECTION * Section) {
	int count;

	for (count = 1; count < 10; count ++) { x86Protected(count) = FALSE; }
	for (count = 1; count < 10; count ++) { UnMap_X86reg (Section, count); }
	for (count = 1; count < 32; count ++) {
		switch (MipsRegState(count)) {
		case STATE_UNKNOWN: break;
		case STATE_CONST_32:
			if ((MipsRegLo(count) & 0x80000000) != 0) {
				MoveConstToVariable(0xFFFFFFFF,&GPR[count].UW[1],GPR_NameHi[count]);
			} else {
				MoveConstToVariable(0,&GPR[count].UW[1],GPR_NameHi[count]);
			}
			MoveConstToVariable(MipsRegLo(count),&GPR[count].UW[0],GPR_NameLo[count]);
			MipsRegState(count) = STATE_UNKNOWN;
			break;
		default:
			DisplayError("Unknown State: %d\nin WriteBackRegisters",MipsRegState(count));
		}
	}
	UnMap_AllFPRs(Section);
}*/
void WriteBackRegisters (BLOCK_SECTION * Section) {
	int count;
	BOOL bEdiZero = FALSE;
	BOOL bEsiSign = FALSE;
	/*** coming soon ***/
	BOOL bEaxGprLo = FALSE;
	BOOL bEbxGprHi = FALSE;

	for (count = 1; count < 10; count ++) { x86Protected(count) = FALSE; }
	for (count = 1; count < 10; count ++) { UnMap_X86reg (Section, count); }

	/*************************************/
	
	for (count = 1; count < 32; count ++) {
		switch (MipsRegState(count)) {
		case STATE_UNKNOWN: break;
		case STATE_CONST_32:
			if (!bEdiZero && (!MipsRegLo(count) || !(MipsRegLo(count) & 0x80000000))) {
				XorX86RegToX86Reg(x86_EDI, x86_EDI);
				bEdiZero = TRUE;
			}
			if (!bEsiSign && (MipsRegLo(count) & 0x80000000)) {
				MoveConstToX86reg(0xFFFFFFFF, x86_ESI);
				bEsiSign = TRUE;
			}

			if ((MipsRegLo(count) & 0x80000000) != 0) {
				MoveX86regToVariable(x86_ESI,&GPR[count].UW[1],GPR_NameHi[count]);
			} else {
				MoveX86regToVariable(x86_EDI,&GPR[count].UW[1],GPR_NameHi[count]);
			}

			if (MipsRegLo(count) == 0) {
				MoveX86regToVariable(x86_EDI,&GPR[count].UW[0],GPR_NameLo[count]);
			} else if (MipsRegLo(count) == 0xFFFFFFFF) {
				MoveX86regToVariable(x86_ESI,&GPR[count].UW[0],GPR_NameLo[count]);
			} else
				MoveConstToVariable(MipsRegLo(count),&GPR[count].UW[0],GPR_NameLo[count]);

			MipsRegState(count) = STATE_UNKNOWN;
			break;
		case STATE_CONST_64:
			if (MipsRegLo(count) == 0 || MipsRegHi(count) == 0) {
				XorX86RegToX86Reg(x86_EDI, x86_EDI);
				bEdiZero = TRUE;
			}
			if (MipsRegLo(count) == 0xFFFFFFFF || MipsRegHi(count) == 0xFFFFFFFF) {
				MoveConstToX86reg(0xFFFFFFFF, x86_ESI);
				bEsiSign = TRUE;
			}

			if (MipsRegHi(count) == 0) {
				MoveX86regToVariable(x86_EDI,&GPR[count].UW[1],GPR_NameHi[count]);
			} else if (MipsRegLo(count) == 0xFFFFFFFF) {
				MoveX86regToVariable(x86_ESI,&GPR[count].UW[1],GPR_NameHi[count]);
			} else {
				MoveConstToVariable(MipsRegHi(count),&GPR[count].UW[1],GPR_NameHi[count]);
			} 

			if (MipsRegLo(count) == 0) {
				MoveX86regToVariable(x86_EDI,&GPR[count].UW[0],GPR_NameLo[count]);
			} else if (MipsRegLo(count) == 0xFFFFFFFF) {
				MoveX86regToVariable(x86_ESI,&GPR[count].UW[0],GPR_NameLo[count]);
			} else {
				MoveConstToVariable(MipsRegLo(count),&GPR[count].UW[0],GPR_NameLo[count]);
			}
			MipsRegState(count) = STATE_UNKNOWN;
			break;
		default:
			if (ShowDebugMessages)
				DisplayError("Unknown State: %d\nin WriteBackRegisters",MipsRegState(count));
		}
	}
	UnMap_AllFPRs(Section);
}

BOOL IsSignExtended(MIPS_DWORD v) {
	return (v.W[0] >> 31) == v.W[1];
}
