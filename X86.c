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
#include "main.h"
#include "cpu.h"
#include "x86.h"
#include "debugger.h"

#define PUTDST8(dest,value)  (*((BYTE *)(dest))=(BYTE)(value)); dest += 1;
#define PUTDST16(dest,value) (*((WORD *)(dest))=(WORD)(value)); dest += 2;
#define PUTDST32(dest,value) (*((DWORD *)(dest))=(DWORD)(value)); dest += 4;

void AdcX86regToVariable(int x86reg, void * Variable, char * VariableName) {
	CPU_Message("      adc dword ptr [%s], %s",VariableName, x86_Name(x86reg));
	switch (x86reg) {
	case x86_EAX: PUTDST16(RecompPos,0x0511); break;
	case x86_EBX: PUTDST16(RecompPos,0x1D11); break;
	case x86_ECX: PUTDST16(RecompPos,0x0D11); break;
	case x86_EDX: PUTDST16(RecompPos,0x1511); break;
	case x86_ESI: PUTDST16(RecompPos,0x3511); break;
	case x86_EDI: PUTDST16(RecompPos,0x3D11); break;
	case x86_ESP: PUTDST16(RecompPos,0x2511); break;
	case x86_EBP: PUTDST16(RecompPos,0x2D11); break;	
	default:
		DisplayError("AddVariableToX86reg\nUnknown x86 Register");
	}
    PUTDST32(RecompPos,Variable); 
}

void AdcConstToVariable(void *Variable, char *VariableName, BYTE Constant) {
	CPU_Message("      adc dword ptr [%s], %Xh", VariableName, Constant);
	PUTDST16(RecompPos,0x1583);
    PUTDST32(RecompPos,Variable);
	PUTDST8(RecompPos,Constant);
}

void AdcConstToX86Reg (int x86Reg, DWORD Const) {
	CPU_Message("      adc %s, %Xh",x86_Name(x86Reg),Const);
	if ((Const & 0xFFFFFF80) != 0 && (Const & 0xFFFFFF80) != 0xFFFFFF80) {
		switch (x86Reg) {
		case x86_EAX: PUTDST16(RecompPos,0xD081); break;
		case x86_EBX: PUTDST16(RecompPos,0xD381); break;
		case x86_ECX: PUTDST16(RecompPos,0xD181); break;
		case x86_EDX: PUTDST16(RecompPos,0xD281); break;
		case x86_ESI: PUTDST16(RecompPos,0xD681); break;
		case x86_EDI: PUTDST16(RecompPos,0xD781); break;
		case x86_ESP: PUTDST16(RecompPos,0xD481); break;
		case x86_EBP: PUTDST16(RecompPos,0xD581); break;
		}
		PUTDST32(RecompPos, Const);
	} else {
		switch (x86Reg) {
		case x86_EAX: PUTDST16(RecompPos,0xD083); break;
		case x86_EBX: PUTDST16(RecompPos,0xD383); break;
		case x86_ECX: PUTDST16(RecompPos,0xD183); break;
		case x86_EDX: PUTDST16(RecompPos,0xD283); break;
		case x86_ESI: PUTDST16(RecompPos,0xD683); break;
		case x86_EDI: PUTDST16(RecompPos,0xD783); break;
		case x86_ESP: PUTDST16(RecompPos,0xD483); break;
		case x86_EBP: PUTDST16(RecompPos,0xD583); break;
		}
		PUTDST8(RecompPos, Const);
	}
}

void AdcVariableToX86reg(int x86reg, void * Variable, char * VariableName) {
	CPU_Message("      adc %s, dword ptr [%s]",x86_Name(x86reg),VariableName);
	switch (x86reg) {
	case x86_EAX: PUTDST16(RecompPos,0x0513); break;
	case x86_EBX: PUTDST16(RecompPos,0x1D13); break;
	case x86_ECX: PUTDST16(RecompPos,0x0D13); break;
	case x86_EDX: PUTDST16(RecompPos,0x1513); break;
	case x86_ESI: PUTDST16(RecompPos,0x3513); break;
	case x86_EDI: PUTDST16(RecompPos,0x3D13); break;
	case x86_ESP: PUTDST16(RecompPos,0x2513); break;
	case x86_EBP: PUTDST16(RecompPos,0x2D13); break;
	default:
		DisplayError("AdcVariableToX86reg\nUnknown x86 Register");
	}
    PUTDST32(RecompPos,Variable); 
}

void AdcX86RegToX86Reg(int Destination, int Source) {
	WORD x86Command = 0x0;

	CPU_Message("      adc %s, %s",x86_Name(Destination),x86_Name(Source));
	switch (Source) {
	case x86_EAX: x86Command = 0x0013; break;
	case x86_EBX: x86Command = 0x0313; break;
	case x86_ECX: x86Command = 0x0113; break;
	case x86_EDX: x86Command = 0x0213; break;
	case x86_ESI: x86Command = 0x0613; break;
	case x86_EDI: x86Command = 0x0713; break;
	case x86_ESP: x86Command = 0x0413; break;
	case x86_EBP: x86Command = 0x0513; break;
	}
	switch (Destination) {
	case x86_EAX: x86Command += 0xC000; break;
	case x86_EBX: x86Command += 0xD800; break;
	case x86_ECX: x86Command += 0xC800; break;
	case x86_EDX: x86Command += 0xD000; break;
	case x86_ESI: x86Command += 0xF000; break;
	case x86_EDI: x86Command += 0xF800; break;
	case x86_ESP: x86Command += 0xE000; break;
	case x86_EBP: x86Command += 0xE800; break;
	}
	PUTDST16(RecompPos,x86Command);
}

void AddConstToVariable (DWORD Const, void *Variable, char *VariableName) {
	CPU_Message("      add dword ptr [%s], 0x%X",VariableName, Const);\
	PUTDST16(RecompPos,0x0581);
	PUTDST32(RecompPos,Variable);
	PUTDST32(RecompPos,Const);
}

void AddConstToX86Reg (int x86Reg, DWORD Const) {
	CPU_Message("      add %s, %Xh",x86_Name(x86Reg),Const);
	if ((Const & 0xFFFFFF80) != 0 && (Const & 0xFFFFFF80) != 0xFFFFFF80) {
		switch (x86Reg) {
		case x86_EAX: PUTDST16(RecompPos,0xC081); break;
		case x86_EBX: PUTDST16(RecompPos,0xC381); break;
		case x86_ECX: PUTDST16(RecompPos,0xC181); break;
		case x86_EDX: PUTDST16(RecompPos,0xC281); break;
		case x86_ESI: PUTDST16(RecompPos,0xC681); break;
		case x86_EDI: PUTDST16(RecompPos,0xC781); break;
		case x86_ESP: PUTDST16(RecompPos,0xC481); break;
		case x86_EBP: PUTDST16(RecompPos,0xC581); break;
		}
		PUTDST32(RecompPos, Const);
	} else {
		switch (x86Reg) {
		case x86_EAX: PUTDST16(RecompPos,0xC083); break;
		case x86_EBX: PUTDST16(RecompPos,0xC383); break;
		case x86_ECX: PUTDST16(RecompPos,0xC183); break;
		case x86_EDX: PUTDST16(RecompPos,0xC283); break;
		case x86_ESI: PUTDST16(RecompPos,0xC683); break;
		case x86_EDI: PUTDST16(RecompPos,0xC783); break;
		case x86_ESP: PUTDST16(RecompPos,0xC483); break;
		case x86_EBP: PUTDST16(RecompPos,0xC583); break;
		}
		PUTDST8(RecompPos, Const);
	}
}

void AddVariableToX86reg(int x86reg, void * Variable, char * VariableName) {
	CPU_Message("      add %s, dword ptr [%s]",x86_Name(x86reg),VariableName);
	switch (x86reg) {
	case x86_EAX: PUTDST16(RecompPos,0x0503); break;
	case x86_EBX: PUTDST16(RecompPos,0x1D03); break;
	case x86_ECX: PUTDST16(RecompPos,0x0D03); break;
	case x86_EDX: PUTDST16(RecompPos,0x1503); break;
	case x86_ESI: PUTDST16(RecompPos,0x3503); break;
	case x86_EDI: PUTDST16(RecompPos,0x3D03); break;
	case x86_ESP: PUTDST16(RecompPos,0x2503); break;
	case x86_EBP: PUTDST16(RecompPos,0x2D03); break;
	default:
		DisplayError("AddVariableToX86reg\nUnknown x86 Register");
	}
    PUTDST32(RecompPos,Variable); 
}

void AddX86regToVariable(int x86reg, void * Variable, char * VariableName) {
	CPU_Message("      add dword ptr [%s], %s",VariableName, x86_Name(x86reg));
	switch (x86reg) {
	case x86_EAX: PUTDST16(RecompPos,0x0501); break;
	case x86_EBX: PUTDST16(RecompPos,0x1D01); break;
	case x86_ECX: PUTDST16(RecompPos,0x0D01); break;
	case x86_EDX: PUTDST16(RecompPos,0x1501); break;
	case x86_ESI: PUTDST16(RecompPos,0x3501); break;
	case x86_EDI: PUTDST16(RecompPos,0x3D01); break;
	case x86_ESP: PUTDST16(RecompPos,0x2501); break;
	case x86_EBP: PUTDST16(RecompPos,0x2D01); break;
	default:
		DisplayError("AddVariableToX86reg\nUnknown x86 Register");
	}
    PUTDST32(RecompPos,Variable); 
}

void AddX86RegToX86Reg(int Destination, int Source) {
	WORD x86Command = 0x0;

	CPU_Message("      add %s, %s",x86_Name(Destination),x86_Name(Source));
	switch (Source) {
	case x86_EAX: x86Command = 0x0003; break;
	case x86_EBX: x86Command = 0x0303; break;
	case x86_ECX: x86Command = 0x0103; break;
	case x86_EDX: x86Command = 0x0203; break;
	case x86_ESI: x86Command = 0x0603; break;
	case x86_EDI: x86Command = 0x0703; break;
	case x86_ESP: x86Command = 0x0403; break;
	case x86_EBP: x86Command = 0x0503; break;
	}
	switch (Destination) {
	case x86_EAX: x86Command += 0xC000; break;
	case x86_EBX: x86Command += 0xD800; break;
	case x86_ECX: x86Command += 0xC800; break;
	case x86_EDX: x86Command += 0xD000; break;
	case x86_ESI: x86Command += 0xF000; break;
	case x86_EDI: x86Command += 0xF800; break;
	case x86_ESP: x86Command += 0xE000; break;
	case x86_EBP: x86Command += 0xE800; break;
	}
	PUTDST16(RecompPos,x86Command);
}

void AndConstToVariable (DWORD Const, void *Variable, char *VariableName) {
	CPU_Message("      and dword ptr [%s], 0x%X",VariableName, Const);\
	PUTDST16(RecompPos,0x2581);
	PUTDST32(RecompPos,Variable);
	PUTDST32(RecompPos,Const);
}

void AndConstToX86Reg(int x86Reg, DWORD Const) {
	CPU_Message("      and %s, %Xh",x86_Name(x86Reg),Const);
	if ((Const & 0xFFFFFF80) != 0 && (Const & 0xFFFFFF80) != 0xFFFFFF80) {
		switch (x86Reg) {
		case x86_EAX: PUTDST16(RecompPos,0xE081); break;
		case x86_EBX: PUTDST16(RecompPos,0xE381); break;
		case x86_ECX: PUTDST16(RecompPos,0xE181); break;
		case x86_EDX: PUTDST16(RecompPos,0xE281); break;
		case x86_ESI: PUTDST16(RecompPos,0xE681); break;
		case x86_EDI: PUTDST16(RecompPos,0xE781); break;
		case x86_ESP: PUTDST16(RecompPos,0xE481); break;
		case x86_EBP: PUTDST16(RecompPos,0xE581); break;
		}
		PUTDST32(RecompPos, Const);
	} else {
		switch (x86Reg) {
		case x86_EAX: PUTDST16(RecompPos,0xE083); break;
		case x86_EBX: PUTDST16(RecompPos,0xE383); break;
		case x86_ECX: PUTDST16(RecompPos,0xE183); break;
		case x86_EDX: PUTDST16(RecompPos,0xE283); break;
		case x86_ESI: PUTDST16(RecompPos,0xE683); break;
		case x86_EDI: PUTDST16(RecompPos,0xE783); break;
		case x86_ESP: PUTDST16(RecompPos,0xE483); break;
		case x86_EBP: PUTDST16(RecompPos,0xE583); break;
		}
		PUTDST8(RecompPos, Const);
	}
}

void AndVariableDispToX86Reg(void *Variable, char *VariableName, int x86Reg, int AddrReg, int Multiplier) {
	int x;
	CPU_Message("      and %s, dword ptr [%s+%s*%i]",x86_Name(x86Reg),VariableName, x86_Name(AddrReg), Multiplier);
	
	PUTDST8(RecompPos,0x23);

	switch (x86Reg) {
	case x86_EAX: PUTDST8(RecompPos,0x04); break;
	case x86_EBX: PUTDST8(RecompPos,0x1C); break;
	case x86_ECX: PUTDST8(RecompPos,0x0C); break;
	case x86_EDX: PUTDST8(RecompPos,0x14); break;
	case x86_ESI: PUTDST8(RecompPos,0x34); break;
	case x86_EDI: PUTDST8(RecompPos,0x3C); break;
	case x86_ESP: PUTDST8(RecompPos,0x24); break;
	case x86_EBP: PUTDST8(RecompPos,0x2C); break;	
	}

	/* put in shifter 2(01), 4(10), 8(11) */
	switch (Multiplier) {
	case 2: x = 0x40; break;
	case 4: x = 0x80; break;
	case 8: x = 0xC0; break;
	default: 
		x = 0x40;
		DisplayError("And\nInvalid x86 multiplier");
	}

	/* format xx|000000 */
	switch (AddrReg) {
	case x86_EAX: PUTDST8(RecompPos,0x05|x); break;
	case x86_EBX: PUTDST8(RecompPos,0x1D|x); break;
	case x86_ECX: PUTDST8(RecompPos,0x0D|x); break;
	case x86_EDX: PUTDST8(RecompPos,0x15|x); break;
	case x86_ESI: PUTDST8(RecompPos,0x35|x); break;
	case x86_EDI: PUTDST8(RecompPos,0x3D|x); break;
	case x86_ESP: PUTDST8(RecompPos,0x25|x); break;
	case x86_EBP: PUTDST8(RecompPos,0x2D|x); break;	
	}

	PUTDST32(RecompPos,Variable);
}

void AndVariableToX86Reg(void * Variable, char * VariableName, int x86Reg) {
	CPU_Message("      and %s, dword ptr [%s]",x86_Name(x86Reg),VariableName);
	switch (x86Reg) {
	case x86_EAX: PUTDST16(RecompPos,0x0523); break;
	case x86_EBX: PUTDST16(RecompPos,0x1D23); break;
	case x86_ECX: PUTDST16(RecompPos,0x0D23); break;
	case x86_EDX: PUTDST16(RecompPos,0x1523); break;
	case x86_ESI: PUTDST16(RecompPos,0x3523); break;
	case x86_EDI: PUTDST16(RecompPos,0x3D23); break;
	case x86_ESP: PUTDST16(RecompPos,0x2523); break;
	case x86_EBP: PUTDST16(RecompPos,0x2D23); break;
	}
	PUTDST32(RecompPos,Variable);
}

void AndX86RegToX86Reg(int Destination, int Source) {
	WORD x86Command = 0x0;

	CPU_Message("      and %s, %s",x86_Name(Destination),x86_Name(Source));
	switch (Destination) {
	case x86_EAX: x86Command = 0x0021; break;
	case x86_EBX: x86Command = 0x0321; break;
	case x86_ECX: x86Command = 0x0121; break;
	case x86_EDX: x86Command = 0x0221; break;
	case x86_ESI: x86Command = 0x0621; break;
	case x86_EDI: x86Command = 0x0721; break;
	case x86_ESP: x86Command = 0x0421; break;
	case x86_EBP: x86Command = 0x0521; break;
	}
	switch (Source) {
	case x86_EAX: x86Command += 0xC000; break;
	case x86_EBX: x86Command += 0xD800; break;
	case x86_ECX: x86Command += 0xC800; break;
	case x86_EDX: x86Command += 0xD000; break;
	case x86_ESI: x86Command += 0xF000; break;
	case x86_EDI: x86Command += 0xF800; break;
	case x86_ESP: x86Command += 0xE000; break;
	case x86_EBP: x86Command += 0xE800; break;
	}
	PUTDST16(RecompPos,x86Command);
}

void BreakPoint (void) {
	CPU_Message("      int 3");
	PUTDST8(RecompPos,0xCC);
}

void Call_Direct(void * FunctAddress, char * FunctName) {
	CPU_Message("      call offset %s",FunctName);
	PUTDST8(RecompPos,0xE8);
	PUTDST32(RecompPos,(DWORD)FunctAddress-(DWORD)RecompPos - 4);
}

void Call_Indirect(void * FunctAddress, char * FunctName) {
	CPU_Message("      call [%s]",FunctName);
	PUTDST16(RecompPos,0x15FF);
	PUTDST32(RecompPos,FunctAddress);
}

void CompConstToVariable(DWORD Const, void * Variable, char * VariableName) {
	CPU_Message("      cmp dword ptr [%s], 0x%X",VariableName, Const);
	PUTDST16(RecompPos,0x3D81);
	PUTDST32(RecompPos,Variable);
	PUTDST32(RecompPos,Const);
}

void CompConstToX86reg(int x86Reg, DWORD Const) {
	CPU_Message("      cmp %s, %Xh",x86_Name(x86Reg),Const);
	if ((Const & 0xFFFFFF80) != 0 && (Const & 0xFFFFFF80) != 0xFFFFFF80) {
		switch (x86Reg) {
		case x86_EAX: PUTDST16(RecompPos,0xF881); break;
		case x86_EBX: PUTDST16(RecompPos,0xFB81); break;
		case x86_ECX: PUTDST16(RecompPos,0xF981); break;
		case x86_EDX: PUTDST16(RecompPos,0xFA81); break;
		case x86_ESI: PUTDST16(RecompPos,0xFE81); break;
		case x86_EDI: PUTDST16(RecompPos,0xFF81); break;
		case x86_ESP: PUTDST16(RecompPos,0xFC81); break;
		case x86_EBP: PUTDST16(RecompPos,0xFD81); break;
		default:
			DisplayError("CompConstToX86reg\nUnknown x86 Register");
		}
		PUTDST32(RecompPos,Const);
	} else {
	switch (x86Reg) {
		case x86_EAX: PUTDST16(RecompPos,0xF883); break;
		case x86_EBX: PUTDST16(RecompPos,0xFB83); break;
		case x86_ECX: PUTDST16(RecompPos,0xF983); break;
		case x86_EDX: PUTDST16(RecompPos,0xFA83); break;
		case x86_ESI: PUTDST16(RecompPos,0xFE83); break;
		case x86_EDI: PUTDST16(RecompPos,0xFF83); break;
		case x86_ESP: PUTDST16(RecompPos,0xFC83); break;
		case x86_EBP: PUTDST16(RecompPos,0xFD83); break;
		}
		PUTDST8(RecompPos, Const);
	}
}

void CompX86regToVariable(int x86Reg, void * Variable, char * VariableName) {
	CPU_Message("      cmp %s, dword ptr [%s]",x86_Name(x86Reg),VariableName);
	switch (x86Reg) {
	case x86_EAX: PUTDST16(RecompPos,0x053B); break;
	case x86_EBX: PUTDST16(RecompPos,0x1D3B); break;
	case x86_ECX: PUTDST16(RecompPos,0x0D3B); break;
	case x86_EDX: PUTDST16(RecompPos,0x153B); break;
	case x86_ESI: PUTDST16(RecompPos,0x353B); break;
	case x86_EDI: PUTDST16(RecompPos,0x3D3B); break;
	case x86_ESP: PUTDST16(RecompPos,0x253B); break;
	case x86_EBP: PUTDST16(RecompPos,0x2D3B); break;
	default:
		DisplayError("Unknown x86 Register");
	}
	PUTDST32(RecompPos,Variable);
}

void CompVariableToX86reg(int x86Reg, void * Variable, char * VariableName) {
	CPU_Message("      cmp dword ptr [%s], %s",VariableName, x86_Name(x86Reg));
	switch (x86Reg) {
	case x86_EAX: PUTDST16(RecompPos,0x0539); break;
	case x86_EBX: PUTDST16(RecompPos,0x1D39); break;
	case x86_ECX: PUTDST16(RecompPos,0x0D39); break;
	case x86_EDX: PUTDST16(RecompPos,0x1539); break;
	case x86_ESI: PUTDST16(RecompPos,0x3539); break;
	case x86_EDI: PUTDST16(RecompPos,0x3D39); break;
	case x86_ESP: PUTDST16(RecompPos,0x2539); break;
	case x86_EBP: PUTDST16(RecompPos,0x2D39); break;
	default:
		DisplayError("Unknown x86 Register");
	}
	PUTDST32(RecompPos,Variable);
}

void CompX86RegToX86Reg(int Destination, int Source) {
	WORD x86Command = 0x0;

	CPU_Message("      cmp %s, %s",x86_Name(Destination),x86_Name(Source));
	
	switch (Source) {
	case x86_EAX: x86Command = 0x003B; break;
	case x86_EBX: x86Command = 0x033B; break;
	case x86_ECX: x86Command = 0x013B; break;
	case x86_EDX: x86Command = 0x023B; break;
	case x86_ESI: x86Command = 0x063B; break;
	case x86_EDI: x86Command = 0x073B; break;
	case x86_ESP: x86Command = 0x043B; break;
	case x86_EBP: x86Command = 0x053B; break;
	}
	switch (Destination) {
	case x86_EAX: x86Command += 0xC000; break;
	case x86_EBX: x86Command += 0xD800; break;
	case x86_ECX: x86Command += 0xC800; break;
	case x86_EDX: x86Command += 0xD000; break;
	case x86_ESI: x86Command += 0xF000; break;
	case x86_EDI: x86Command += 0xF800; break;
	case x86_ESP: x86Command += 0xE000; break;
	case x86_EBP: x86Command += 0xE800; break;
	}
	PUTDST16(RecompPos,x86Command);
}

void DecX86reg(int x86Reg) {
	CPU_Message("      dec %s",x86_Name(x86Reg));
	switch (x86Reg) {
	case x86_EAX: PUTDST16(RecompPos,0xC8FF); break;
	case x86_EBX: PUTDST16(RecompPos,0xCBFF); break;
	case x86_ECX: PUTDST16(RecompPos,0xC9FF); break;
	case x86_EDX: PUTDST16(RecompPos,0xCAFF); break;
	case x86_ESI: PUTDST16(RecompPos,0xCEFF); break;
	case x86_EDI: PUTDST16(RecompPos,0xCFFF); break;
	case x86_ESP: PUTDST8 (RecompPos,0x4C);   break;
	case x86_EBP: PUTDST8 (RecompPos,0x4D);   break;
	default:
		DisplayError("DecX86reg\nUnknown x86 Register");
	}
}

void DivX86reg(int x86reg) {
	CPU_Message("      div %s",x86_Name(x86reg));
	switch (x86reg) {
	case x86_EBX: PUTDST16(RecompPos,0xf3F7); break;
	case x86_ECX: PUTDST16(RecompPos,0xf1F7); break;
	case x86_EDX: PUTDST16(RecompPos,0xf2F7); break;
	case x86_ESI: PUTDST16(RecompPos,0xf6F7); break;
	case x86_EDI: PUTDST16(RecompPos,0xf7F7); break;
	case x86_ESP: PUTDST16(RecompPos,0xf4F7); break;
	case x86_EBP: PUTDST16(RecompPos,0xf5F7); break;
	default:
		DisplayError("divX86reg\nUnknown x86 Register");
	}
}

void idivX86reg(int x86reg) {
	CPU_Message("      idiv %s",x86_Name(x86reg));\
	switch (x86reg) {
	case x86_EBX: PUTDST16(RecompPos,0xfbF7); break;
	case x86_ECX: PUTDST16(RecompPos,0xf9F7); break;
	case x86_EDX: PUTDST16(RecompPos,0xfaF7); break;
	case x86_ESI: PUTDST16(RecompPos,0xfeF7); break;
	case x86_EDI: PUTDST16(RecompPos,0xffF7); break;
	case x86_ESP: PUTDST16(RecompPos,0xfcF7); break;
	case x86_EBP: PUTDST16(RecompPos,0xfdF7); break;
	default:
		DisplayError("idivX86reg\nUnknown x86 Register");
	}
}

void imulX86reg(int x86reg) {
	CPU_Message("      imul %s",x86_Name(x86reg));
	switch (x86reg) {
	case x86_EAX: PUTDST16(RecompPos,0xE8F7); break;
	case x86_EBX: PUTDST16(RecompPos,0xEBF7); break;
	case x86_ECX: PUTDST16(RecompPos,0xE9F7); break;
	case x86_EDX: PUTDST16(RecompPos,0xEAF7); break;
	case x86_ESI: PUTDST16(RecompPos,0xEEF7); break;
	case x86_EDI: PUTDST16(RecompPos,0xEFF7); break;
	case x86_ESP: PUTDST16(RecompPos,0xECF7); break;
	case x86_EBP: PUTDST16(RecompPos,0xEDF7); break;
	default:
		DisplayError("imulX86reg\nUnknown x86 Register");
	}
}

void IncX86reg(int x86Reg) {
	CPU_Message("      inc %s",x86_Name(x86Reg));
	switch (x86Reg) {
	case x86_EAX: PUTDST16(RecompPos,0xC0FF); break;
	case x86_EBX: PUTDST16(RecompPos,0xC3FF); break;
	case x86_ECX: PUTDST16(RecompPos,0xC1FF); break;
	case x86_EDX: PUTDST16(RecompPos,0xC2FF); break;
	case x86_ESI: PUTDST16(RecompPos,0xC6FF); break;
	case x86_EDI: PUTDST16(RecompPos,0xC7FF); break;
	case x86_ESP: PUTDST8 (RecompPos,0x44);   break;
	case x86_EBP: PUTDST8 (RecompPos,0x45);   break;
	default:
		DisplayError("IncX86reg\nUnknown x86 Register");
	}
}

void JaeLabel8(char * Label, BYTE Value) {
	CPU_Message("      jae $%s",Label);
	PUTDST8(RecompPos,0x73);
	PUTDST8(RecompPos,Value);
}

void JaeLabel32(char * Label,DWORD Value) {
	CPU_Message("      jae $%s",Label);
	PUTDST16(RecompPos,0x830F);
	PUTDST32(RecompPos,Value);
}

void JaLabel8(char * Label, BYTE Value) {
	CPU_Message("      ja $%s",Label);
	PUTDST8(RecompPos,0x77);
	PUTDST8(RecompPos,Value);
}

void JaLabel32(char * Label,DWORD Value) {
	CPU_Message("      ja $%s",Label);
	PUTDST16(RecompPos,0x870F);
	PUTDST32(RecompPos,Value);
}

void JbLabel8(char * Label, BYTE Value) {
	CPU_Message("      jb $%s",Label);
	PUTDST8(RecompPos,0x72);
	PUTDST8(RecompPos,Value);
}

void JbLabel32(char * Label,DWORD Value) {
	CPU_Message("      jb $%s",Label);
	PUTDST16(RecompPos,0x820F);
	PUTDST32(RecompPos,Value);
}

void JecxzLabel8(char * Label, BYTE Value) {
	CPU_Message("      jecxz $%s",Label);
	PUTDST8(RecompPos,0xE3);
	PUTDST8(RecompPos,Value);
}

void JeLabel8(char * Label, BYTE Value) {
	CPU_Message("      je $%s",Label);
	PUTDST8(RecompPos,0x74);
	PUTDST8(RecompPos,Value);
}

void JeLabel32(char * Label,DWORD Value) {
	CPU_Message("      je $%s",Label);
	PUTDST16(RecompPos,0x840F);
	PUTDST32(RecompPos,Value);
}

void JgeLabel32(char * Label,DWORD Value) {
	CPU_Message("      jge $%s",Label);
	PUTDST16(RecompPos,0x8D0F);
	PUTDST32(RecompPos,Value);
}

void JgLabel8(char * Label, BYTE Value) {
	CPU_Message("      jg $%s",Label);
	PUTDST8(RecompPos,0x7F);
	PUTDST8(RecompPos,Value);
}

void JgLabel32(char * Label,DWORD Value) {
	CPU_Message("      jg $%s",Label);
	PUTDST16(RecompPos,0x8F0F);
	PUTDST32(RecompPos,Value);
}

void JleLabel8(char * Label, BYTE Value) {
	CPU_Message("      jle $%s",Label);
	PUTDST8(RecompPos,0x7E);
	PUTDST8(RecompPos,Value);
}

void JleLabel32(char * Label,DWORD Value) {
	CPU_Message("      jle $%s",Label);
	PUTDST16(RecompPos,0x8E0F);
	PUTDST32(RecompPos,Value);
}

void JlLabel8(char * Label, BYTE Value) {
	CPU_Message("      jl $%s",Label);
	PUTDST8(RecompPos,0x7C);
	PUTDST8(RecompPos,Value);
}

void JlLabel32(char * Label,DWORD Value) {
	CPU_Message("      jl $%s",Label);
	PUTDST16(RecompPos,0x8C0F);
	PUTDST32(RecompPos,Value);
}

void JmpDirectReg( int x86reg ) {
	CPU_Message("      jmp %s",x86_Name(x86reg));

	switch (x86reg) {
	case x86_EAX: PUTDST16(RecompPos,0xE0ff); break;
	case x86_EBX: PUTDST16(RecompPos,0xE3ff); break;
	case x86_ECX: PUTDST16(RecompPos,0xE1ff); break;
	case x86_EDX: PUTDST16(RecompPos,0xE2ff); break;
	case x86_ESI: PUTDST16(RecompPos,0xE6ff); break;
	case x86_EDI: PUTDST16(RecompPos,0xE7ff); break;
	default:
		DisplayError("JmpDirectReg\nUnknown x86 Register");		
		break;
	}
}

void JmpIndirectLabel32(char * Label,DWORD location) {
	CPU_Message("      jmp dword ptr [%s]", Label);
	PUTDST16(RecompPos, 0x25ff);
	PUTDST32(RecompPos, location);
}

void JmpIndirectReg( int x86reg ) {
	CPU_Message("      jmp dword ptr [%s]",x86_Name(x86reg));

	switch (x86reg) {
	case x86_EAX: PUTDST16(RecompPos,0x20ff); break;
	case x86_EBX: PUTDST16(RecompPos,0x23ff); break;
	case x86_ECX: PUTDST16(RecompPos,0x21ff); break;
	case x86_EDX: PUTDST16(RecompPos,0x22ff); break;
	case x86_ESI: PUTDST16(RecompPos,0x26ff); break;
	case x86_EDI: PUTDST16(RecompPos,0x27ff); break;
	case x86_ESP: 
		PUTDST8(RecompPos,0xff);
		PUTDST16(RecompPos,0x2434); 
	/*	_asm int 3 */
		break;		
	case x86_EBP: 
		PUTDST8(RecompPos,0xff);
		PUTDST16(RecompPos,0x0065); 
	/*	_asm int 3 */
		break;
	}
}

void JmpLabel8(char * Label, BYTE Value) {
	CPU_Message("      jmp $%s",Label);
	PUTDST8(RecompPos,0xEB);
	PUTDST8(RecompPos,Value);
}

void JmpLabel32(char * Label, DWORD Value) {
	CPU_Message("      jmp $%s",Label);
	PUTDST8(RecompPos,0xE9);
	PUTDST32(RecompPos,Value);
}

void JneLabel8(char * Label, BYTE Value) {
	CPU_Message("      jne $%s",Label);
	PUTDST8(RecompPos,0x75);
	PUTDST8(RecompPos,Value);
}

void JneLabel32(char *Label, DWORD Value) {
	CPU_Message("      jne $%s",Label);
	PUTDST16(RecompPos,0x850F);
	PUTDST32(RecompPos,Value);
}

void JnsLabel8(char * Label, BYTE Value) {
	CPU_Message("      jns $%s",Label);
	PUTDST8(RecompPos,0x79);
	PUTDST8(RecompPos,Value);
}

void JnsLabel32(char *Label, DWORD Value) {
	CPU_Message("      jns $%s",Label);
	PUTDST16(RecompPos,0x890F);
	PUTDST32(RecompPos,Value);
}

void JsLabel32(char *Label, DWORD Value) {
	CPU_Message("      js $%s",Label);
	PUTDST16(RecompPos,0x880F);
	PUTDST32(RecompPos,Value);
}

void LeaRegReg(int x86RegDest, int x86RegSrc, int multiplier) {
	int s = 0x0;
	CPU_Message("      lea %s, [%s*%i]", x86_Name(x86RegDest), x86_Name(x86RegSrc), multiplier);

	PUTDST8(RecompPos,0x8D);
	switch (x86RegDest) {
	case x86_EAX: PUTDST8(RecompPos,0x04); break;
	case x86_EBX: PUTDST8(RecompPos,0x1C); break;
	case x86_ECX: PUTDST8(RecompPos,0x0C); break;
	case x86_EDX: PUTDST8(RecompPos,0x14); break;
	case x86_ESI: PUTDST8(RecompPos,0x34); break;
	case x86_EDI: PUTDST8(RecompPos,0x3C); break;
	case x86_ESP: PUTDST8(RecompPos,0x24); break;
	case x86_EBP: PUTDST8(RecompPos,0x2C); break;	
	default:
		DisplayError("LEA\nUnknown x86 Register");
	}

	/* put in shifter 2(01), 4(10), 8(11) */
	switch(multiplier) {
	case 2: s = 0x40; break;
	case 4: s = 0x80; break;
	case 8: s = 0xC0; break;
	default:
		DisplayError("LEA\nInvalid x86 multiplier");
	}

	/* format ss|000000 */
	switch (x86RegSrc) {
	case x86_EAX: PUTDST8(RecompPos,0x05|s); break;
	case x86_EBX: PUTDST8(RecompPos,0x1D|s); break;
	case x86_ECX: PUTDST8(RecompPos,0x0D|s); break;
	case x86_EDX: PUTDST8(RecompPos,0x15|s); break;
	case x86_ESI: PUTDST8(RecompPos,0x35|s); break;
	case x86_EDI: PUTDST8(RecompPos,0x3D|s); break;
	case x86_EBP: PUTDST8(RecompPos,0x2D|s); break;
	case x86_ESP: 
		DisplayError("LEA\nESP is invalid"); break;
	default:
		DisplayError("LEA\nUnknown x86 Register");
	}

	PUTDST32(RecompPos,0x00000000);
}

void LeaSourceAndOffset(int x86DestReg, int x86SourceReg, int offset) {
	WORD x86Command = 0x0;

	CPU_Message("      lea %s, [%s + %0Xh]",x86_Name(x86DestReg),x86_Name(x86SourceReg),offset);

//	if ((offset & 0xFFFFFF80) != 0 && (offset & 0xFFFFFF80) != 0xFFFFFF80) {
/*	if (1) {*/
	switch (x86DestReg) {
	case x86_EAX: x86Command = 0x808D; break;
	case x86_EBX: x86Command = 0x988D; break;
	case x86_ECX: x86Command = 0x888D; break;
	case x86_EDX: x86Command = 0x908D; break;
	case x86_ESI: x86Command = 0xB08D; break;
	case x86_EDI: x86Command = 0xB88D; break;
	case x86_ESP: x86Command = 0xA08D; break;
	case x86_EBP: x86Command = 0xA88D; break;
	default:
		DisplayError("LeaSourceAndOffset\nUnknown x86 Register");
	}
	switch (x86SourceReg) {
	case x86_EAX: x86Command += 0x0000; break;
	case x86_EBX: x86Command += 0x0300; break;
	case x86_ECX: x86Command += 0x0100; break;
	case x86_EDX: x86Command += 0x0200; break;
	case x86_ESI: x86Command += 0x0600; break;
	case x86_EDI: x86Command += 0x0700; break;
	case x86_ESP: x86Command += 0x0400; break;
	case x86_EBP: x86Command += 0x0500; break;
	default:
		DisplayError("LeaSourceAndOffset\nUnknown x86 Register");
	}
	PUTDST16(RecompPos,x86Command);
	PUTDST32(RecompPos,offset);
	/*
	} else {
		switch (x86DestReg) {
		case x86_EAX: x86Command = 0x408D; break;
		case x86_EBX: x86Command = 0x588D; break;
		case x86_ECX: x86Command = 0x488D; break;
		case x86_EDX: x86Command = 0x508D; break;
		case x86_ESI: x86Command = 0x708D; break;
		case x86_EDI: x86Command = 0x788D; break;
		case x86_ESP: x86Command = 0x608D; break;
		case x86_EBP: x86Command = 0x688D; break;
		default:
			DisplayError("LeaSourceAndOffset\nUnknown x86 Register");
		}
		switch (x86SourceReg) {
		case x86_EAX: x86Command += 0x0000; break;
		case x86_EBX: x86Command += 0x0300; break;
		case x86_ECX: x86Command += 0x0100; break;
		case x86_EDX: x86Command += 0x0200; break;
		case x86_ESI: x86Command += 0x0600; break;
		case x86_EDI: x86Command += 0x0700; break;
		case x86_ESP: x86Command += 0x0400; break;
		case x86_EBP: x86Command += 0x0500; break;
		default:
			DisplayError("LeaSourceAndOffset\nUnknown x86 Register");
		}
		PUTDST16(RecompPos,x86Command);
		PUTDST8(RecompPos,offset);
	}*/
}

void MoveConstByteToN64Mem(BYTE Const, int AddrReg) {
	CPU_Message("      mov byte ptr [%s+N64mem], %Xh",x86_Name(AddrReg),Const);
	switch (AddrReg) {
	case x86_EAX: PUTDST16(RecompPos,0x80C6); break;
	case x86_EBX: PUTDST16(RecompPos,0x83C6); break;
	case x86_ECX: PUTDST16(RecompPos,0x81C6); break;
	case x86_EDX: PUTDST16(RecompPos,0x82C6); break;
	case x86_ESI: PUTDST16(RecompPos,0x86C6); break;
	case x86_EDI: PUTDST16(RecompPos,0x87C6); break;
	case x86_ESP: PUTDST16(RecompPos,0x84C6); break;
	case x86_EBP: PUTDST16(RecompPos,0x85C6); break;
	default:
		DisplayError("MoveConstByteToN64Mem\nUnknown x86 Register");
	}
	PUTDST32(RecompPos,N64MEM);
	PUTDST8(RecompPos,Const);
}

void MoveConstByteToVariable (BYTE Const,void *Variable, char *VariableName) {
	CPU_Message("      mov byte ptr [%s], %Xh",VariableName,Const);
	PUTDST16(RecompPos,0x05C6);
    PUTDST32(RecompPos,Variable);
    PUTDST8(RecompPos,Const);
}

void MoveConstHalfToN64Mem(WORD Const, int AddrReg) {
	CPU_Message("      mov word ptr [%s+N64mem], %Xh",x86_Name(AddrReg),Const);
	PUTDST8(RecompPos,0x66);
	switch (AddrReg) {
	case x86_EAX: PUTDST16(RecompPos,0x80C7); break;
	case x86_EBX: PUTDST16(RecompPos,0x83C7); break;
	case x86_ECX: PUTDST16(RecompPos,0x81C7); break;
	case x86_EDX: PUTDST16(RecompPos,0x82C7); break;
	case x86_ESI: PUTDST16(RecompPos,0x86C7); break;
	case x86_EDI: PUTDST16(RecompPos,0x87C7); break;
	case x86_ESP: PUTDST16(RecompPos,0x84C7); break;
	case x86_EBP: PUTDST16(RecompPos,0x85C7); break;
	default:
		DisplayError("MoveConstToN64Mem\nUnknown x86 Register");
	}
	PUTDST32(RecompPos,N64MEM);
	PUTDST16(RecompPos,Const);
}

void MoveConstHalfToVariable (WORD Const,void *Variable, char *VariableName) {
	CPU_Message("      mov word ptr [%s], %Xh",VariableName,Const);
	PUTDST8(RecompPos,0x66);
	PUTDST16(RecompPos,0x05C7);
    PUTDST32(RecompPos,Variable);
    PUTDST16(RecompPos,Const);
}

void MoveConstHalfToX86regPointer(WORD Const, int AddrReg1, int AddrReg2) {
	BYTE Param = 0x0;

	CPU_Message("      mov word ptr [%s+%s],%Xh",x86_Name(AddrReg1), x86_Name(AddrReg2), Const);

	PUTDST8(RecompPos,0x66);
	PUTDST16(RecompPos,0x04C7);

	switch (AddrReg1) {
	case x86_EAX: Param = 0x00; break;
	case x86_EBX: Param = 0x03; break;
	case x86_ECX: Param = 0x01; break;
	case x86_EDX: Param = 0x02; break;
	case x86_ESI: Param = 0x06; break;
	case x86_EDI: Param = 0x07; break;
	default:
		DisplayError("MoveConstToX86regPointer\nUnhandled x86 Register");
	}

	switch (AddrReg2) {
	case x86_EAX: Param += 0x00; break;
	case x86_EBX: Param += 0x18; break;
	case x86_ECX: Param += 0x08; break;
	case x86_EDX: Param += 0x10; break;
	case x86_ESI: Param += 0x30; break;
	case x86_EDI: Param += 0x38; break;
	case x86_ESP: Param += 0x20; break;
	case x86_EBP: Param += 0x28; break;
	default:
		DisplayError("MoveConstToX86regPointer\nUnhandled x86 Register");
	}
	PUTDST8(RecompPos,Param);
    PUTDST16(RecompPos,Const); 
}

void MoveConstToMemoryDisp (DWORD Const, int AddrReg, DWORD Disp) {
	CPU_Message("      mov dword ptr [%s+%Xh], %Xh",x86_Name(AddrReg),Disp,Const);
	switch (AddrReg) {
	case x86_EAX: PUTDST16(RecompPos,0x80C7); break;
	case x86_EBX: PUTDST16(RecompPos,0x83C7); break;
	case x86_ECX: PUTDST16(RecompPos,0x81C7); break;
	case x86_EDX: PUTDST16(RecompPos,0x82C7); break;
	case x86_ESI: PUTDST16(RecompPos,0x86C7); break;
	case x86_EDI: PUTDST16(RecompPos,0x87C7); break;
	case x86_ESP: PUTDST16(RecompPos,0x84C7); break;
	case x86_EBP: PUTDST16(RecompPos,0x85C7); break;
	default:
		DisplayError("MoveConstToN64Mem\nUnknown x86 Register");
	}
	PUTDST32(RecompPos,Disp);
	PUTDST32(RecompPos,Const);
}

void MoveConstToN64Mem(DWORD Const, int AddrReg) {
	CPU_Message("      mov dword ptr [%s+N64mem], %Xh",x86_Name(AddrReg),Const);
	switch (AddrReg) {
	case x86_EAX: PUTDST16(RecompPos,0x80C7); break;
	case x86_EBX: PUTDST16(RecompPos,0x83C7); break;
	case x86_ECX: PUTDST16(RecompPos,0x81C7); break;
	case x86_EDX: PUTDST16(RecompPos,0x82C7); break;
	case x86_ESI: PUTDST16(RecompPos,0x86C7); break;
	case x86_EDI: PUTDST16(RecompPos,0x87C7); break;
	case x86_ESP: PUTDST16(RecompPos,0x84C7); break;
	case x86_EBP: PUTDST16(RecompPos,0x85C7); break;
	default:
		DisplayError("MoveConstToN64Mem\nUnknown x86 Register");
	}
	PUTDST32(RecompPos,N64MEM);
	PUTDST32(RecompPos,Const);
}

void MoveConstToN64MemDisp (DWORD Const, int AddrReg, BYTE Disp) {
	CPU_Message("      mov dword ptr [%s+N64mem+%Xh], %Xh",x86_Name(AddrReg),Const,Disp);
	switch (AddrReg) {
	case x86_EAX: PUTDST16(RecompPos,0x80C7); break;
	case x86_EBX: PUTDST16(RecompPos,0x83C7); break;
	case x86_ECX: PUTDST16(RecompPos,0x81C7); break;
	case x86_EDX: PUTDST16(RecompPos,0x82C7); break;
	case x86_ESI: PUTDST16(RecompPos,0x86C7); break;
	case x86_EDI: PUTDST16(RecompPos,0x87C7); break;
	case x86_ESP: PUTDST16(RecompPos,0x84C7); break;
	case x86_EBP: PUTDST16(RecompPos,0x85C7); break;
	default:
		DisplayError("MoveConstToN64Mem\nUnknown x86 Register");
	}
	PUTDST32(RecompPos,N64MEM + Disp);
	PUTDST32(RecompPos,Const);
}

void MoveConstToVariable (DWORD Const,void *Variable, char *VariableName) {
	CPU_Message("      mov dword ptr [%s], %Xh",VariableName,Const);
	PUTDST16(RecompPos,0x05C7);
    PUTDST32(RecompPos,Variable);
    PUTDST32(RecompPos,Const);
}

void MoveConstToX86Pointer(DWORD Const, int X86Pointer) {
	WORD x86Command = 0x0;

	CPU_Message("      mov dword ptr [%s], %Xh",x86_Name(X86Pointer),Const);

	switch (X86Pointer) {
	case x86_EAX: x86Command = 0x00C7; break;
	case x86_EBX: x86Command = 0x03C7; break;
	case x86_ECX: x86Command = 0x01C7; break;
	case x86_EDX: x86Command = 0x02C7; break;
	case x86_ESI: x86Command = 0x06C7; break;
	case x86_EDI: x86Command = 0x07C7; break;
	}
	PUTDST16(RecompPos,x86Command);
    PUTDST32(RecompPos,Const);
}


void MoveConstToX86reg(DWORD Const, int x86reg) {
	CPU_Message("      mov %s, %Xh",x86_Name(x86reg),Const);
	switch (x86reg) {
	case x86_EAX: PUTDST16(RecompPos,0xC0C7); break;
	case x86_EBX: PUTDST16(RecompPos,0xC3C7); break;
	case x86_ECX: PUTDST16(RecompPos,0xC1C7); break;
	case x86_EDX: PUTDST16(RecompPos,0xC2C7); break;
	case x86_ESI: PUTDST16(RecompPos,0xC6C7); break;
	case x86_EDI: PUTDST16(RecompPos,0xC7C7); break;
	case x86_ESP: PUTDST16(RecompPos,0xC4C7); break;
	case x86_EBP: PUTDST16(RecompPos,0xC5C7); break;
	default:
		DisplayError("MoveConstToX86reg\nUnknown x86 Register");
		_asm int 3
	}
    PUTDST32(RecompPos,Const); 
}

void MoveConstByteToX86regPointer(BYTE Const, int AddrReg1, int AddrReg2) {
	BYTE Param = 0x0;

	CPU_Message("      mov byte ptr [%s+%s],%Xh",x86_Name(AddrReg1), x86_Name(AddrReg2), Const);

	PUTDST16(RecompPos,0x04C6);

	switch (AddrReg1) {
	case x86_EAX: Param = 0x00; break;
	case x86_EBX: Param = 0x03; break;
	case x86_ECX: Param = 0x01; break;
	case x86_EDX: Param = 0x02; break;
	case x86_ESI: Param = 0x06; break;
	case x86_EDI: Param = 0x07; break;
	default:
		DisplayError("MoveConstByteToX86regPointer\nUnhandled x86 Register");
	}

	switch (AddrReg2) {
	case x86_EAX: Param += 0x00; break;
	case x86_EBX: Param += 0x18; break;
	case x86_ECX: Param += 0x08; break;
	case x86_EDX: Param += 0x10; break;
	case x86_ESI: Param += 0x30; break;
	case x86_EDI: Param += 0x38; break;
	case x86_ESP: Param += 0x20; break;
	case x86_EBP: Param += 0x28; break;
	default:
		DisplayError("MoveConstByteToX86regPointer\nUnhandled x86 Register");
	}
	PUTDST8(RecompPos,Param);
    PUTDST8(RecompPos,Const); 
}

void MoveConstToX86regPointer(DWORD Const, int AddrReg1, int AddrReg2) {
	BYTE Param = 0x0;

	CPU_Message("      mov dword ptr [%s+%s],%Xh",x86_Name(AddrReg1), x86_Name(AddrReg2), Const);

	PUTDST16(RecompPos,0x04C7);

	switch (AddrReg1) {
	case x86_EAX: Param = 0x00; break;
	case x86_EBX: Param = 0x03; break;
	case x86_ECX: Param = 0x01; break;
	case x86_EDX: Param = 0x02; break;
	case x86_ESI: Param = 0x06; break;
	case x86_EDI: Param = 0x07; break;
	default:
		DisplayError("MoveConstToX86regPointer\nUnhandled x86 Register");
	}

	switch (AddrReg2) {
	case x86_EAX: Param += 0x00; break;
	case x86_EBX: Param += 0x18; break;
	case x86_ECX: Param += 0x08; break;
	case x86_EDX: Param += 0x10; break;
	case x86_ESI: Param += 0x30; break;
	case x86_EDI: Param += 0x38; break;
	case x86_ESP: Param += 0x20; break;
	case x86_EBP: Param += 0x28; break;
	default:
		DisplayError("MoveConstToX86regPointer\nUnhandled x86 Register");
	}
	PUTDST8(RecompPos,Param);
    PUTDST32(RecompPos,Const); 
}

void MoveN64MemDispToX86reg(int x86reg, int AddrReg, BYTE Disp) {
	WORD x86Command = 0x0;

	CPU_Message("      mov %s, dword ptr [%s+N64mem+%Xh]",x86_Name(x86reg),x86_Name(AddrReg),Disp);
	switch (AddrReg) {
	case x86_EAX: x86Command = 0x008B; break;
	case x86_EBX: x86Command = 0x038B; break;
	case x86_ECX: x86Command = 0x018B; break;
	case x86_EDX: x86Command = 0x028B; break;
	case x86_ESI: x86Command = 0x068B; break;
	case x86_EDI: x86Command = 0x078B; break;
	case x86_ESP: x86Command = 0x048B; break;
	case x86_EBP: x86Command = 0x058B; break;
	}
	switch (x86reg) {
	case x86_EAX: x86Command += 0x8000; break;
	case x86_EBX: x86Command += 0x9800; break;
	case x86_ECX: x86Command += 0x8800; break;
	case x86_EDX: x86Command += 0x9000; break;
	case x86_ESI: x86Command += 0xB000; break;
	case x86_EDI: x86Command += 0xB800; break;
	case x86_ESP: x86Command += 0xA000; break;
	case x86_EBP: x86Command += 0xA800; break;
	}
	PUTDST16(RecompPos,x86Command);
	PUTDST32(RecompPos,N64MEM + Disp);
}

void MoveN64MemToX86reg(int x86reg, int AddrReg) {
	WORD x86Command = 0x0;

	CPU_Message("      mov %s, dword ptr [%s+N64mem]",x86_Name(x86reg),x86_Name(AddrReg));
	
	switch (AddrReg) {
	case x86_EAX: x86Command = 0x008B; break;
	case x86_EBX: x86Command = 0x038B; break;
	case x86_ECX: x86Command = 0x018B; break;
	case x86_EDX: x86Command = 0x028B; break;
	case x86_ESI: x86Command = 0x068B; break;
	case x86_EDI: x86Command = 0x078B; break;
	case x86_ESP: x86Command = 0x048B; break;
	case x86_EBP: x86Command = 0x058B; break;
	}
	switch (x86reg) {
	case x86_EAX: x86Command += 0x8000; break;
	case x86_EBX: x86Command += 0x9800; break;
	case x86_ECX: x86Command += 0x8800; break;
	case x86_EDX: x86Command += 0x9000; break;
	case x86_ESI: x86Command += 0xB000; break;
	case x86_EDI: x86Command += 0xB800; break;
	case x86_ESP: x86Command += 0xA000; break;
	case x86_EBP: x86Command += 0xA800; break;
	}
	PUTDST16(RecompPos,x86Command);
	PUTDST32(RecompPos,N64MEM);
}
	
void MoveN64MemToX86regByte(int x86reg, int AddrReg) {
	WORD x86Command = 0x0;

	CPU_Message("      mov %s, byte ptr [%s+N64mem]",x86Byte_Name(x86reg),x86_Name(AddrReg));
	switch (AddrReg) {
	case x86_EAX: x86Command = 0x008A; break;
	case x86_EBX: x86Command = 0x038A; break;
	case x86_ECX: x86Command = 0x018A; break;
	case x86_EDX: x86Command = 0x028A; break;
	case x86_ESI: x86Command = 0x068A; break;
	case x86_EDI: x86Command = 0x078A; break;
	case x86_ESP: x86Command = 0x048A; break;
	case x86_EBP: x86Command = 0x058A; break;
	}
	switch (x86reg) {
	case x86_EAX: x86Command += 0x8000; break;
	case x86_EBX: x86Command += 0x9800; break;
	case x86_ECX: x86Command += 0x8800; break;
	case x86_EDX: x86Command += 0x9000; break;
/*	case x86_ESI: x86Command += 0xB000; break; */
/*	case x86_EDI: x86Command += 0xB800; break; */
/*	case x86_ESP: case x86_EBP: */
	default:
		DisplayError("MoveN64MemToX86regByte\nInvalid x86 Register");
		break;
	}
	PUTDST16(RecompPos,x86Command);
	PUTDST32(RecompPos,N64MEM);
}

void MoveN64MemToX86regHalf(int x86reg, int AddrReg) {
	WORD x86Command = 0x0;

	CPU_Message("      mov %s, word ptr [%s+N64mem]",x86Half_Name(x86reg),x86_Name(AddrReg));
	
	PUTDST8(RecompPos,0x66);
	switch (AddrReg) {
	case x86_EAX: x86Command = 0x008B; break;
	case x86_EBX: x86Command = 0x038B; break;
	case x86_ECX: x86Command = 0x018B; break;
	case x86_EDX: x86Command = 0x028B; break;
	case x86_ESI: x86Command = 0x068B; break;
	case x86_EDI: x86Command = 0x078B; break;
	case x86_ESP: x86Command = 0x048B; break;
	case x86_EBP: x86Command = 0x058B; break;
	}
	switch (x86reg) {
	case x86_EAX: x86Command += 0x8000; break;
	case x86_EBX: x86Command += 0x9800; break;
	case x86_ECX: x86Command += 0x8800; break;
	case x86_EDX: x86Command += 0x9000; break;
	case x86_ESI: x86Command += 0xB000; break;
	case x86_EDI: x86Command += 0xB800; break;
	case x86_ESP: x86Command += 0xA000; break;
	case x86_EBP: x86Command += 0xA800; break;
	}
	PUTDST16(RecompPos,x86Command);
	PUTDST32(RecompPos,N64MEM);
}

void MoveSxByteX86regPointerToX86reg(int AddrReg1, int AddrReg2, int x86reg) {
	BYTE Param = 0x0;

	CPU_Message("      movsx %s, byte ptr [%s+%s]",x86_Name(x86reg),x86_Name(AddrReg1), x86_Name(AddrReg2));

	PUTDST16(RecompPos,0xBE0F);
	switch (x86reg) {
	case x86_EAX: PUTDST8(RecompPos,0x04); break;
	case x86_EBX: PUTDST8(RecompPos,0x1C); break;
	case x86_ECX: PUTDST8(RecompPos,0x0C); break;
	case x86_EDX: PUTDST8(RecompPos,0x14); break;
	case x86_ESI: PUTDST8(RecompPos,0x34); break;
	case x86_EDI: PUTDST8(RecompPos,0x3C); break;
	case x86_ESP: PUTDST8(RecompPos,0x24); break;
	case x86_EBP: PUTDST8(RecompPos,0x2C); break;
	default:
		DisplayError("MoveZxByteX86regPointerToX86reg\nUnhandled x86 Register");
	}

	switch (AddrReg1) {
	case x86_EAX: Param = 0x00; break;
	case x86_EBX: Param = 0x03; break;
	case x86_ECX: Param = 0x01; break;
	case x86_EDX: Param = 0x02; break;
	case x86_ESI: Param = 0x06; break;
	case x86_EDI: Param = 0x07; break;
	default:
		DisplayError("MoveZxByteX86regPointerToX86reg\nUnhandled x86 Register");
	}

	switch (AddrReg2) {
	case x86_EAX: Param += 0x00; break;
	case x86_EBX: Param += 0x18; break;
	case x86_ECX: Param += 0x08; break;
	case x86_EDX: Param += 0x10; break;
	case x86_ESI: Param += 0x30; break;
	case x86_EDI: Param += 0x38; break;
	case x86_ESP: Param += 0x20; break;
	case x86_EBP: Param += 0x28; break;
	default:
		DisplayError("MoveZxByteX86regPointerToX86reg\nUnhandled x86 Register");
	}
	PUTDST8(RecompPos,Param);
}

void MoveSxHalfX86regPointerToX86reg(int AddrReg1, int AddrReg2, int x86reg) {
	BYTE Param = 0x0;

	CPU_Message("      movsx %s, word ptr [%s+%s]",x86_Name(x86reg),x86_Name(AddrReg1), x86_Name(AddrReg2));

	PUTDST16(RecompPos,0xBF0F);
	switch (x86reg) {
	case x86_EAX: PUTDST8(RecompPos,0x04); break;
	case x86_EBX: PUTDST8(RecompPos,0x1C); break;
	case x86_ECX: PUTDST8(RecompPos,0x0C); break;
	case x86_EDX: PUTDST8(RecompPos,0x14); break;
	case x86_ESI: PUTDST8(RecompPos,0x34); break;
	case x86_EDI: PUTDST8(RecompPos,0x3C); break;
	case x86_ESP: PUTDST8(RecompPos,0x24); break;
	case x86_EBP: PUTDST8(RecompPos,0x2C); break;
	default:
		DisplayError("MoveZxHalfX86regPointerToX86reg\nUnhandled x86 Register");
	}

	switch (AddrReg1) {
	case x86_EAX: Param = 0x00; break;
	case x86_EBX: Param = 0x03; break;
	case x86_ECX: Param = 0x01; break;
	case x86_EDX: Param = 0x02; break;
	case x86_ESI: Param = 0x06; break;
	case x86_EDI: Param = 0x07; break;
	default:
		DisplayError("MoveZxHalfX86regPointerToX86reg\nUnhandled x86 Register");
	}

	switch (AddrReg2) {
	case x86_EAX: Param += 0x00; break;
	case x86_EBX: Param += 0x18; break;
	case x86_ECX: Param += 0x08; break;
	case x86_EDX: Param += 0x10; break;
	case x86_ESI: Param += 0x30; break;
	case x86_EDI: Param += 0x38; break;
	case x86_ESP: Param += 0x20; break;
	case x86_EBP: Param += 0x28; break;
	default:
		DisplayError("MoveZxHalfX86regPointerToX86reg\nUnhandled x86 Register");
	}
	PUTDST8(RecompPos,Param);
}

void MoveSxN64MemToX86regByte(int x86reg, int AddrReg) {
	WORD x86Command = 0x0;

	CPU_Message("      movsx %s, byte ptr [%s+Dmem]",x86_Name(x86reg),x86_Name(AddrReg));
	switch (AddrReg) {
	case x86_EAX: x86Command = 0x00BE; break;
	case x86_EBX: x86Command = 0x03BE; break;
	case x86_ECX: x86Command = 0x01BE; break;
	case x86_EDX: x86Command = 0x02BE; break;
	case x86_ESI: x86Command = 0x06BE; break;
	case x86_EDI: x86Command = 0x07BE; break;
	case x86_ESP: x86Command = 0x04BE; break;
	case x86_EBP: x86Command = 0x05BE; break;
	}
	switch (x86reg) {
	case x86_EAX: x86Command += 0x8000; break;
	case x86_EBX: x86Command += 0x9800; break;
	case x86_ECX: x86Command += 0x8800; break;
	case x86_EDX: x86Command += 0x9000; break;
	case x86_ESI: x86Command += 0xB000; break;
	case x86_EDI: x86Command += 0xB800; break;
	case x86_ESP: x86Command += 0xA000; break;
	case x86_EBP: x86Command += 0xA800; break;
	default:
		DisplayError("MoveSxN64MemToX86regByte\nInvalid x86 Register");
		break;
	}
	PUTDST8(RecompPos,0x0f);
	PUTDST16(RecompPos,x86Command);
	PUTDST32(RecompPos,N64MEM);
}

void MoveSxN64MemToX86regHalf(int x86reg, int AddrReg) {
	WORD x86Command = 0x0;

	CPU_Message("      movsx %s, word ptr [%s+Dmem]",x86_Name(x86reg),x86_Name(AddrReg));

	switch (AddrReg) {
	case x86_EAX: x86Command = 0x00BF; break;
	case x86_EBX: x86Command = 0x03BF; break;
	case x86_ECX: x86Command = 0x01BF; break;
	case x86_EDX: x86Command = 0x02BF; break;
	case x86_ESI: x86Command = 0x06BF; break;
	case x86_EDI: x86Command = 0x07BF; break;
	case x86_ESP: x86Command = 0x04BF; break;
	case x86_EBP: x86Command = 0x05BF; break;
	}
	switch (x86reg) {
	case x86_EAX: x86Command += 0x8000; break;
	case x86_EBX: x86Command += 0x9800; break;
	case x86_ECX: x86Command += 0x8800; break;
	case x86_EDX: x86Command += 0x9000; break;
	case x86_ESI: x86Command += 0xB000; break;
	case x86_EDI: x86Command += 0xB800; break;
	case x86_ESP: x86Command += 0xA000; break;
	case x86_EBP: x86Command += 0xA800; break;
	}

	PUTDST8(RecompPos, 0x0f);
	PUTDST16(RecompPos,x86Command);
	PUTDST32(RecompPos,N64MEM);
}

void MoveSxVariableToX86regByte(void *Variable, char *VariableName, int x86reg) {
	CPU_Message("      movsx %s, byte ptr [%s]",x86_Name(x86reg),VariableName);

	PUTDST16(RecompPos, 0xbe0f);

	switch (x86reg) {
	case x86_EAX: PUTDST8(RecompPos,0x05); break;
	case x86_EBX: PUTDST8(RecompPos,0x1D); break;
	case x86_ECX: PUTDST8(RecompPos,0x0D); break;
	case x86_EDX: PUTDST8(RecompPos,0x15); break;
	case x86_ESI: PUTDST8(RecompPos,0x35); break;
	case x86_EDI: PUTDST8(RecompPos,0x3D); break;
	case x86_ESP: PUTDST8(RecompPos,0x25); break;
	case x86_EBP: PUTDST8(RecompPos,0x2D); break;
	default: DisplayError("MoveSxVariableToX86regHalf\nUnknown x86 Register");
	}
    PUTDST32(RecompPos,Variable);
}

void MoveSxVariableToX86regHalf(void *Variable, char *VariableName, int x86reg) {
	CPU_Message("      movsx %s, word ptr [%s]",x86_Name(x86reg),VariableName);

	PUTDST16(RecompPos, 0xbf0f);

	switch (x86reg) {
	case x86_EAX: PUTDST8(RecompPos,0x05); break;
	case x86_EBX: PUTDST8(RecompPos,0x1D); break;
	case x86_ECX: PUTDST8(RecompPos,0x0D); break;
	case x86_EDX: PUTDST8(RecompPos,0x15); break;
	case x86_ESI: PUTDST8(RecompPos,0x35); break;
	case x86_EDI: PUTDST8(RecompPos,0x3D); break;
	case x86_ESP: PUTDST8(RecompPos,0x25); break;
	case x86_EBP: PUTDST8(RecompPos,0x2D); break;
	default: DisplayError("MoveSxVariableToX86regHalf\nUnknown x86 Register");
	}
    PUTDST32(RecompPos,Variable);
}

void MoveVariableToX86reg(void *Variable, char *VariableName, int x86reg) {
	CPU_Message("      mov %s, dword ptr [%s]",x86_Name(x86reg),VariableName);
	switch (x86reg) {
	case x86_EAX: PUTDST16(RecompPos,0x058B); break;
	case x86_EBX: PUTDST16(RecompPos,0x1D8B); break;
	case x86_ECX: PUTDST16(RecompPos,0x0D8B); break;
	case x86_EDX: PUTDST16(RecompPos,0x158B); break;
	case x86_ESI: PUTDST16(RecompPos,0x358B); break;
	case x86_EDI: PUTDST16(RecompPos,0x3D8B); break;
	case x86_ESP: PUTDST16(RecompPos,0x258B); break;
	case x86_EBP: PUTDST16(RecompPos,0x2D8B); break;
	default: DisplayError("MoveVariableToX86reg\nUnknown x86 Register");
	}
    PUTDST32(RecompPos,Variable);
}

void MoveVariableDispToX86Reg(void *Variable, char *VariableName, int x86Reg, int AddrReg, int Multiplier) {
	int x = 0x0;
	CPU_Message("      mov %s, dword ptr [%s+%s*%i]",x86_Name(x86Reg),VariableName, x86_Name(AddrReg), Multiplier);
	
	PUTDST8(RecompPos,0x8B);

	switch (x86Reg) {
	case x86_EAX: PUTDST8(RecompPos,0x04); break;
	case x86_EBX: PUTDST8(RecompPos,0x1C); break;
	case x86_ECX: PUTDST8(RecompPos,0x0C); break;
	case x86_EDX: PUTDST8(RecompPos,0x14); break;
	case x86_ESI: PUTDST8(RecompPos,0x34); break;
	case x86_EDI: PUTDST8(RecompPos,0x3C); break;
	case x86_ESP: PUTDST8(RecompPos,0x24); break;
	case x86_EBP: PUTDST8(RecompPos,0x2C); break;	
	}

	/* put in shifter 2(01), 4(10), 8(11) */
	switch (Multiplier) {
	case 1: x = 0; break;
	case 2: x = 0x40; break;
	case 4: x = 0x80; break;
	case 8: x = 0xC0; break;
	default: DisplayError("Move\nInvalid x86 multiplier");
	}

	/* format xx|000000 */
	switch (AddrReg) {
	case x86_EAX: PUTDST8(RecompPos,0x05|x); break;
	case x86_EBX: PUTDST8(RecompPos,0x1D|x); break;
	case x86_ECX: PUTDST8(RecompPos,0x0D|x); break;
	case x86_EDX: PUTDST8(RecompPos,0x15|x); break;
	case x86_ESI: PUTDST8(RecompPos,0x35|x); break;
	case x86_EDI: PUTDST8(RecompPos,0x3D|x); break;
	case x86_ESP: PUTDST8(RecompPos,0x25|x); break;
	case x86_EBP: PUTDST8(RecompPos,0x2D|x); break;	
	}

	PUTDST32(RecompPos,Variable);
}

void MoveVariableToX86regByte(void *Variable, char *VariableName, int x86reg) {
	CPU_Message("      mov %s, byte ptr [%s]",x86Byte_Name(x86reg),VariableName);
	switch (x86reg) {
	case x86_EAX: PUTDST16(RecompPos,0x058A); break;
	case x86_EBX: PUTDST16(RecompPos,0x1D8A); break;
	case x86_ECX: PUTDST16(RecompPos,0x0D8A); break;
	case x86_EDX: PUTDST16(RecompPos,0x158A); break;
	default: DisplayError("MoveVariableToX86regByte\nUnknown x86 Register");
	}
    PUTDST32(RecompPos,Variable);
}

void MoveVariableToX86regHalf(void *Variable, char *VariableName, int x86reg) {
	CPU_Message("      mov %s, word ptr [%s]",x86Half_Name(x86reg),VariableName);
	PUTDST8(RecompPos,0x66);
	switch (x86reg) {
	case x86_EAX: PUTDST16(RecompPos,0x058B); break;
	case x86_EBX: PUTDST16(RecompPos,0x1D8B); break;
	case x86_ECX: PUTDST16(RecompPos,0x0D8B); break;
	case x86_EDX: PUTDST16(RecompPos,0x158B); break;
	case x86_ESI: PUTDST16(RecompPos,0x358B); break;
	case x86_EDI: PUTDST16(RecompPos,0x3D8B); break;
	case x86_ESP: PUTDST16(RecompPos,0x258B); break;
	case x86_EBP: PUTDST16(RecompPos,0x2D8B); break;
	default: DisplayError("MoveVariableToX86reg\nUnknown x86 Register");
	}
    PUTDST32(RecompPos,Variable);
}

void MoveX86regByteToN64Mem(int x86reg, int AddrReg) {
	WORD x86Command = 0x0;

	CPU_Message("      mov byte ptr [%s+N64mem], %s",x86_Name(AddrReg),x86Byte_Name(x86reg));
	
	switch (AddrReg) {
	case x86_EAX: x86Command = 0x0088; break;
	case x86_EBX: x86Command = 0x0388; break;
	case x86_ECX: x86Command = 0x0188; break;
	case x86_EDX: x86Command = 0x0288; break;
	case x86_ESI: x86Command = 0x0688; break;
	case x86_EDI: x86Command = 0x0788; break;
	}
	switch (x86reg) {
	case x86_EAX: x86Command += 0x8000; break;
	case x86_EBX: x86Command += 0x9800; break;
	case x86_ECX: x86Command += 0x8800; break;
	case x86_EDX: x86Command += 0x9000; break;
	}
	PUTDST16(RecompPos,x86Command);
	PUTDST32(RecompPos,N64MEM);
}

void MoveX86regByteToVariable(int x86reg, void * Variable, char * VariableName) {
	CPU_Message("      mov byte ptr [%s], %s",VariableName,x86Byte_Name(x86reg));
	switch (x86reg) {
	case x86_EAX: PUTDST16(RecompPos,0x0588); break;
	case x86_EBX: PUTDST16(RecompPos,0x1D88); break;
	case x86_ECX: PUTDST16(RecompPos,0x0D88); break;
	case x86_EDX: PUTDST16(RecompPos,0x1588); break;
	default:
		DisplayError("MoveX86regByteToVariable\nUnknown x86 Register");
	}
    PUTDST32(RecompPos,Variable);
}

void MoveX86regByteToX86regPointer(int x86reg, int AddrReg1, int AddrReg2) {
	BYTE Param = 0x0;

	CPU_Message("      mov byte ptr [%s+%s],%s",x86_Name(AddrReg1), x86_Name(AddrReg2), x86Byte_Name(x86reg));

	switch (x86reg) {
	case x86_EAX: PUTDST16(RecompPos,0x0488); break;
	case x86_EBX: PUTDST16(RecompPos,0x1C88); break;
	case x86_ECX: PUTDST16(RecompPos,0x0C88); break;
	case x86_EDX: PUTDST16(RecompPos,0x1488); break;
	case x86_ESI: PUTDST16(RecompPos,0x3488); break;
	case x86_EDI: PUTDST16(RecompPos,0x3C88); break;
	case x86_ESP: PUTDST16(RecompPos,0x2488); break;
	case x86_EBP: PUTDST16(RecompPos,0x2C88); break;
	default:
		DisplayError("MoveX86regToX86regPointer\nUnhandled x86 Register");
	}

	switch (AddrReg1) {
	case x86_EAX: Param = 0x00; break;
	case x86_EBX: Param = 0x03; break;
	case x86_ECX: Param = 0x01; break;
	case x86_EDX: Param = 0x02; break;
	case x86_ESI: Param = 0x06; break;
	case x86_EDI: Param = 0x07; break;
	default:
		DisplayError("MoveX86regToX86regPointer\nUnhandled x86 Register");
	}

	switch (AddrReg2) {
	case x86_EAX: Param += 0x00; break;
	case x86_EBX: Param += 0x18; break;
	case x86_ECX: Param += 0x08; break;
	case x86_EDX: Param += 0x10; break;
	case x86_ESI: Param += 0x30; break;
	case x86_EDI: Param += 0x38; break;
	case x86_ESP: Param += 0x20; break;
	case x86_EBP: Param += 0x28; break;
	default:
		DisplayError("MoveX86regByteToX86regPointer\nUnhandled x86 Register");
	}
	PUTDST8(RecompPos,Param);
}

void MoveX86regHalfToN64Mem(int x86reg, int AddrReg) {
	WORD x86Command = 0x0;

	CPU_Message("      mov word ptr [%s+N64mem], %s",x86_Name(AddrReg),x86Half_Name(x86reg));\
	PUTDST8(RecompPos,0x66);
	switch (AddrReg) {
	case x86_EAX: x86Command = 0x0089; break;
	case x86_EBX: x86Command = 0x0389; break;
	case x86_ECX: x86Command = 0x0189; break;
	case x86_EDX: x86Command = 0x0289; break;
	case x86_ESI: x86Command = 0x0689; break;
	case x86_EDI: x86Command = 0x0789; break;
	case x86_ESP: x86Command = 0x0489; break;
	case x86_EBP: x86Command = 0x0589; break;
	}
	switch (x86reg) {
	case x86_EAX: x86Command += 0x8000; break;
	case x86_EBX: x86Command += 0x9800; break;
	case x86_ECX: x86Command += 0x8800; break;
	case x86_EDX: x86Command += 0x9000; break;
	case x86_ESI: x86Command += 0xB000; break;
	case x86_EDI: x86Command += 0xB800; break;
	case x86_ESP: x86Command += 0xA000; break;
	case x86_EBP: x86Command += 0xA800; break;
	}
	PUTDST16(RecompPos,x86Command);
	PUTDST32(RecompPos,N64MEM);
}

void MoveX86regHalfToVariable(int x86reg, void * Variable, char * VariableName) {
	CPU_Message("      mov word ptr [%s], %s",VariableName,x86Half_Name(x86reg));
	PUTDST8(RecompPos,0x66);
	switch (x86reg) {
	case x86_EAX: PUTDST16(RecompPos,0x0589); break;
	case x86_EBX: PUTDST16(RecompPos,0x1D89); break;
	case x86_ECX: PUTDST16(RecompPos,0x0D89); break;
	case x86_EDX: PUTDST16(RecompPos,0x1589); break;
	case x86_ESI: PUTDST16(RecompPos,0x3589); break;
	case x86_EDI: PUTDST16(RecompPos,0x3D89); break;
	case x86_ESP: PUTDST16(RecompPos,0x2589); break;
	case x86_EBP: PUTDST16(RecompPos,0x2D89); break;
	default:
		DisplayError("MoveX86regToVariable\nUnknown x86 Register");
	}
    PUTDST32(RecompPos,Variable);
}

void MoveX86regHalfToX86regPointer(int x86reg, int AddrReg1, int AddrReg2) {
	BYTE Param = 0x0;

	CPU_Message("      mov word ptr [%s+%s],%s",x86_Name(AddrReg1), x86_Name(AddrReg2), x86Half_Name(x86reg));

	PUTDST8(RecompPos,0x66);
	switch (x86reg) {
	case x86_EAX: PUTDST16(RecompPos,0x0489); break;
	case x86_EBX: PUTDST16(RecompPos,0x1C89); break;
	case x86_ECX: PUTDST16(RecompPos,0x0C89); break;
	case x86_EDX: PUTDST16(RecompPos,0x1489); break;
	case x86_ESI: PUTDST16(RecompPos,0x3489); break;
	case x86_EDI: PUTDST16(RecompPos,0x3C89); break;
	case x86_ESP: PUTDST16(RecompPos,0x2489); break;
	case x86_EBP: PUTDST16(RecompPos,0x2C89); break;
	default:
		DisplayError("MoveX86regHalfToX86regPointer\nUnhandled x86 Register");
	}

	switch (AddrReg1) {
	case x86_EAX: Param = 0x00; break;
	case x86_EBX: Param = 0x03; break;
	case x86_ECX: Param = 0x01; break;
	case x86_EDX: Param = 0x02; break;
	case x86_ESI: Param = 0x06; break;
	case x86_EDI: Param = 0x07; break;
	default:
		DisplayError("MoveX86regHalfToX86regPointer\nUnhandled x86 Register");
	}

	switch (AddrReg2) {
	case x86_EAX: Param += 0x00; break;
	case x86_EBX: Param += 0x18; break;
	case x86_ECX: Param += 0x08; break;
	case x86_EDX: Param += 0x10; break;
	case x86_ESI: Param += 0x30; break;
	case x86_EDI: Param += 0x38; break;
	case x86_ESP: Param += 0x20; break;
	case x86_EBP: Param += 0x28; break;
	default:
		DisplayError("MoveX86regHalfToX86regPointer\nUnhandled x86 Register");
	}
	PUTDST8(RecompPos,Param);
}

void MoveX86PointerToX86reg(int x86reg, int X86Pointer) {
	WORD x86Command = 0x0;

	CPU_Message("      mov %s, dword ptr [%s]",x86_Name(x86reg),x86_Name(X86Pointer));

	switch (X86Pointer) {
	case x86_EAX: x86Command = 0x008B; break;
	case x86_EBX: x86Command = 0x038B; break;
	case x86_ECX: x86Command = 0x018B; break;
	case x86_EDX: x86Command = 0x028B; break;
	case x86_ESI: x86Command = 0x068B; break;
	case x86_EDI: x86Command = 0x078B; break;
	}
	
	switch (x86reg) {
	case x86_EAX: x86Command += 0x0000; break;
	case x86_EBX: x86Command += 0x1800; break;
	case x86_ECX: x86Command += 0x0800; break;
	case x86_EDX: x86Command += 0x1000; break;
	case x86_ESI: x86Command += 0x3000; break;
	case x86_EDI: x86Command += 0x3800; break;
	case x86_ESP: x86Command += 0x2000; break;
	case x86_EBP: x86Command += 0x2800; break;
	}
	PUTDST16(RecompPos,x86Command);
}

void MoveX86regPointerToX86reg(int AddrReg1, int AddrReg2, int x86reg) {
	BYTE Param = 0x0;

	CPU_Message("      mov %s, dword ptr [%s+%s]",x86_Name(x86reg),x86_Name(AddrReg1), x86_Name(AddrReg2));

	switch (x86reg) {
	case x86_EAX: PUTDST16(RecompPos,0x048B); break;
	case x86_EBX: PUTDST16(RecompPos,0x1C8B); break;
	case x86_ECX: PUTDST16(RecompPos,0x0C8B); break;
	case x86_EDX: PUTDST16(RecompPos,0x148B); break;
	case x86_ESI: PUTDST16(RecompPos,0x348B); break;
	case x86_EDI: PUTDST16(RecompPos,0x3C8B); break;
	case x86_ESP: PUTDST16(RecompPos,0x248B); break;
	case x86_EBP: PUTDST16(RecompPos,0x2C8B); break;
	default:
		DisplayError("MoveX86regPointerToX86reg\nUnhandled x86 Register");
	}

	switch (AddrReg1) {
	case x86_EAX: Param = 0x00; break;
	case x86_EBX: Param = 0x03; break;
	case x86_ECX: Param = 0x01; break;
	case x86_EDX: Param = 0x02; break;
	case x86_ESI: Param = 0x06; break;
	case x86_EDI: Param = 0x07; break;
	default:
		DisplayError("MoveX86regPointerToX86reg\nUnhandled x86 Register");
	}

	switch (AddrReg2) {
	case x86_EAX: Param += 0x00; break;
	case x86_EBX: Param += 0x18; break;
	case x86_ECX: Param += 0x08; break;
	case x86_EDX: Param += 0x10; break;
	case x86_ESI: Param += 0x30; break;
	case x86_EDI: Param += 0x38; break;
	case x86_ESP: Param += 0x20; break;
	case x86_EBP: Param += 0x28; break;
	default:
		DisplayError("MoveX86regPointerToX86reg\nUnhandled x86 Register");
	}
	PUTDST8(RecompPos,Param);
}

void MoveX86regPointerToX86regDisp8(int AddrReg1, int AddrReg2, int x86reg, BYTE offset) {
	BYTE Param = 0x0;

	CPU_Message("      mov %s, dword ptr [%s+%s]",x86_Name(x86reg),x86_Name(AddrReg1), x86_Name(AddrReg2));

	switch (x86reg) {
	case x86_EAX: PUTDST16(RecompPos,0x448B); break;
	case x86_EBX: PUTDST16(RecompPos,0x5C8B); break;
	case x86_ECX: PUTDST16(RecompPos,0x4C8B); break;
	case x86_EDX: PUTDST16(RecompPos,0x548B); break;
	case x86_ESI: PUTDST16(RecompPos,0x748B); break;
	case x86_EDI: PUTDST16(RecompPos,0x7C8B); break;
	case x86_ESP: PUTDST16(RecompPos,0x648B); break;
	case x86_EBP: PUTDST16(RecompPos,0x6C8B); break;
	default:
		DisplayError("MoveX86regPointerToX86reg\nUnhandled x86 Register");
	}

	switch (AddrReg1) {
	case x86_EAX: Param = 0x00; break;
	case x86_EBX: Param = 0x03; break;
	case x86_ECX: Param = 0x01; break;
	case x86_EDX: Param = 0x02; break;
	case x86_ESI: Param = 0x06; break;
	case x86_EDI: Param = 0x07; break;
	default:
		DisplayError("MoveX86regPointerToX86reg\nUnhandled x86 Register");
	}

	switch (AddrReg2) {
	case x86_EAX: Param += 0x00; break;
	case x86_EBX: Param += 0x18; break;
	case x86_ECX: Param += 0x08; break;
	case x86_EDX: Param += 0x10; break;
	case x86_ESI: Param += 0x30; break;
	case x86_EDI: Param += 0x38; break;
	case x86_ESP: Param += 0x20; break;
	case x86_EBP: Param += 0x28; break;
	default:
		DisplayError("MoveX86regPointerToX86reg\nUnhandled x86 Register");
	}
	PUTDST8(RecompPos,Param);
	PUTDST8(RecompPos,offset);
}

void MoveX86regToMemory(int x86reg, int AddrReg, DWORD Disp) {
	WORD x86Command = 0x0;

	CPU_Message("      mov dword ptr [%s+%X], %s",x86_Name(AddrReg),Disp,x86_Name(x86reg));
	switch (AddrReg) {
	case x86_EAX: x86Command = 0x0089; break;
	case x86_EBX: x86Command = 0x0389; break;
	case x86_ECX: x86Command = 0x0189; break;
	case x86_EDX: x86Command = 0x0289; break;
	case x86_ESI: x86Command = 0x0689; break;
	case x86_EDI: x86Command = 0x0789; break;
	case x86_ESP: x86Command = 0x0489; break;
	case x86_EBP: x86Command = 0x0589; break;
	}
	switch (x86reg) {
	case x86_EAX: x86Command += 0x8000; break;
	case x86_EBX: x86Command += 0x9800; break;
	case x86_ECX: x86Command += 0x8800; break;
	case x86_EDX: x86Command += 0x9000; break;
	case x86_ESI: x86Command += 0xB000; break;
	case x86_EDI: x86Command += 0xB800; break;
	case x86_ESP: x86Command += 0xA000; break;
	case x86_EBP: x86Command += 0xA800; break;
	}
	PUTDST16(RecompPos,x86Command);
	PUTDST32(RecompPos,Disp);
}

void MoveX86regToN64Mem(int x86reg, int AddrReg) {
	WORD x86Command = 0x0;

	CPU_Message("      mov dword ptr [%s+N64mem], %s",x86_Name(AddrReg),x86_Name(x86reg));\
	switch (AddrReg) {
	case x86_EAX: x86Command = 0x0089; break;
	case x86_EBX: x86Command = 0x0389; break;
	case x86_ECX: x86Command = 0x0189; break;
	case x86_EDX: x86Command = 0x0289; break;
	case x86_ESI: x86Command = 0x0689; break;
	case x86_EDI: x86Command = 0x0789; break;
	case x86_ESP: x86Command = 0x0489; break;
	case x86_EBP: x86Command = 0x0589; break;
	}
	switch (x86reg) {
	case x86_EAX: x86Command += 0x8000; break;
	case x86_EBX: x86Command += 0x9800; break;
	case x86_ECX: x86Command += 0x8800; break;
	case x86_EDX: x86Command += 0x9000; break;
	case x86_ESI: x86Command += 0xB000; break;
	case x86_EDI: x86Command += 0xB800; break;
	case x86_ESP: x86Command += 0xA000; break;
	case x86_EBP: x86Command += 0xA800; break;
	}
	PUTDST16(RecompPos,x86Command);
	PUTDST32(RecompPos,N64MEM);
}

void MoveX86regToN64MemDisp(int x86reg, int AddrReg, BYTE Disp) {
	WORD x86Command = 0x0;

	CPU_Message("      mov dword ptr [%s+N64mem+%d], %s",x86_Name(AddrReg),Disp,x86_Name(x86reg));\
	switch (AddrReg) {
	case x86_EAX: x86Command = 0x0089; break;
	case x86_EBX: x86Command = 0x0389; break;
	case x86_ECX: x86Command = 0x0189; break;
	case x86_EDX: x86Command = 0x0289; break;
	case x86_ESI: x86Command = 0x0689; break;
	case x86_EDI: x86Command = 0x0789; break;
	case x86_ESP: x86Command = 0x0489; break;
	case x86_EBP: x86Command = 0x0589; break;
	}
	switch (x86reg) {
	case x86_EAX: x86Command += 0x8000; break;
	case x86_EBX: x86Command += 0x9800; break;
	case x86_ECX: x86Command += 0x8800; break;
	case x86_EDX: x86Command += 0x9000; break;
	case x86_ESI: x86Command += 0xB000; break;
	case x86_EDI: x86Command += 0xB800; break;
	case x86_ESP: x86Command += 0xA000; break;
	case x86_EBP: x86Command += 0xA800; break;
	}
	PUTDST16(RecompPos,x86Command);
	PUTDST32(RecompPos,N64MEM+Disp);
}

void MoveX86regToVariable(int x86reg, void * Variable, char * VariableName) {
	CPU_Message("      mov dword ptr [%s], %s",VariableName,x86_Name(x86reg));
	switch (x86reg) {
	case x86_EAX: PUTDST16(RecompPos,0x0589); break;
	case x86_EBX: PUTDST16(RecompPos,0x1D89); break;
	case x86_ECX: PUTDST16(RecompPos,0x0D89); break;
	case x86_EDX: PUTDST16(RecompPos,0x1589); break;
	case x86_ESI: PUTDST16(RecompPos,0x3589); break;
	case x86_EDI: PUTDST16(RecompPos,0x3D89); break;
	case x86_ESP: PUTDST16(RecompPos,0x2589); break;
	case x86_EBP: PUTDST16(RecompPos,0x2D89); break;
	default:
		DisplayError("MoveX86regToVariable\nUnknown x86 Register");
	}
    PUTDST32(RecompPos,Variable);
}

void MoveX86RegToX86Reg(int Source, int Destination) {
	WORD x86Command = 0x0;
	
	CPU_Message("      mov %s, %s",x86_Name(Destination),x86_Name(Source));

	switch (Destination) {\
	case x86_EAX: x86Command = 0x0089; break;
	case x86_EBX: x86Command = 0x0389; break;
	case x86_ECX: x86Command = 0x0189; break;
	case x86_EDX: x86Command = 0x0289; break;
	case x86_ESI: x86Command = 0x0689; break;
	case x86_EDI: x86Command = 0x0789; break;
	case x86_ESP: x86Command = 0x0489; break;
	case x86_EBP: x86Command = 0x0589; break;
	}
	
	switch (Source) {
	case x86_EAX: x86Command += 0xC000; break;
	case x86_EBX: x86Command += 0xD800; break;
	case x86_ECX: x86Command += 0xC800; break;
	case x86_EDX: x86Command += 0xD000; break;
	case x86_ESI: x86Command += 0xF000; break;
	case x86_EDI: x86Command += 0xF800; break;
	case x86_ESP: x86Command += 0xE000; break;
	case x86_EBP: x86Command += 0xE800; break;
	}
	PUTDST16(RecompPos,x86Command);
}

void MoveX86regToX86Pointer(int x86reg, int X86Pointer) {
	WORD x86Command = 0x0;

	CPU_Message("      mov dword ptr [%s], %s",x86_Name(X86Pointer),x86_Name(x86reg));

	switch (X86Pointer) {
	case x86_EAX: x86Command = 0x0089; break;
	case x86_EBX: x86Command = 0x0389; break;
	case x86_ECX: x86Command = 0x0189; break;
	case x86_EDX: x86Command = 0x0289; break;
	case x86_ESI: x86Command = 0x0689; break;
	case x86_EDI: x86Command = 0x0789; break;
	}
	
	switch (x86reg) {
	case x86_EAX: x86Command += 0x0000; break;
	case x86_EBX: x86Command += 0x1800; break;
	case x86_ECX: x86Command += 0x0800; break;
	case x86_EDX: x86Command += 0x1000; break;
	case x86_ESI: x86Command += 0x3000; break;
	case x86_EDI: x86Command += 0x3800; break;
	case x86_ESP: x86Command += 0x2000; break;
	case x86_EBP: x86Command += 0x2800; break;
	}
	PUTDST16(RecompPos,x86Command);
}

void MoveX86regToX86regPointer(int x86reg, int AddrReg1, int AddrReg2) {
	BYTE Param = 0x0;

	CPU_Message("      mov dword ptr [%s+%s],%s",x86_Name(AddrReg1), x86_Name(AddrReg2), x86_Name(x86reg));

	switch (x86reg) {
	case x86_EAX: PUTDST16(RecompPos,0x0489); break;
	case x86_EBX: PUTDST16(RecompPos,0x1C89); break;
	case x86_ECX: PUTDST16(RecompPos,0x0C89); break;
	case x86_EDX: PUTDST16(RecompPos,0x1489); break;
	case x86_ESI: PUTDST16(RecompPos,0x3489); break;
	case x86_EDI: PUTDST16(RecompPos,0x3C89); break;
	case x86_ESP: PUTDST16(RecompPos,0x2489); break;
	case x86_EBP: PUTDST16(RecompPos,0x2C89); break;
	default:
		DisplayError("MoveX86regToX86regPointer\nUnhandled x86 Register");
	}

	switch (AddrReg1) {
	case x86_EAX: Param = 0x00; break;
	case x86_EBX: Param = 0x03; break;
	case x86_ECX: Param = 0x01; break;
	case x86_EDX: Param = 0x02; break;
	case x86_ESI: Param = 0x06; break;
	case x86_EDI: Param = 0x07; break;
	default:
		DisplayError("MoveX86regToX86regPointer\nUnhandled x86 Register");
	}

	switch (AddrReg2) {
	case x86_EAX: Param += 0x00; break;
	case x86_EBX: Param += 0x18; break;
	case x86_ECX: Param += 0x08; break;
	case x86_EDX: Param += 0x10; break;
	case x86_ESI: Param += 0x30; break;
	case x86_EDI: Param += 0x38; break;
	case x86_ESP: Param += 0x20; break;
	case x86_EBP: Param += 0x28; break;
	default:
		DisplayError("MoveX86regToX86regPointer\nUnhandled x86 Register");
	}
	PUTDST8(RecompPos,Param);
}

void MoveZxByteX86regPointerToX86reg(int AddrReg1, int AddrReg2, int x86reg) {
	BYTE Param = 0x0;

	CPU_Message("      movzx %s, byte ptr [%s+%s]",x86_Name(x86reg),x86_Name(AddrReg1), x86_Name(AddrReg2));

	PUTDST16(RecompPos,0xB60F);
	switch (x86reg) {
	case x86_EAX: PUTDST8(RecompPos,0x04); break;
	case x86_EBX: PUTDST8(RecompPos,0x1C); break;
	case x86_ECX: PUTDST8(RecompPos,0x0C); break;
	case x86_EDX: PUTDST8(RecompPos,0x14); break;
	case x86_ESI: PUTDST8(RecompPos,0x34); break;
	case x86_EDI: PUTDST8(RecompPos,0x3C); break;
	case x86_ESP: PUTDST8(RecompPos,0x24); break;
	case x86_EBP: PUTDST8(RecompPos,0x2C); break;
	default:
		DisplayError("MoveZxByteX86regPointerToX86reg\nUnhandled x86 Register");
	}

	switch (AddrReg1) {
	case x86_EAX: Param = 0x00; break;
	case x86_EBX: Param = 0x03; break;
	case x86_ECX: Param = 0x01; break;
	case x86_EDX: Param = 0x02; break;
	case x86_ESI: Param = 0x06; break;
	case x86_EDI: Param = 0x07; break;
	default:
		DisplayError("MoveZxByteX86regPointerToX86reg\nUnhandled x86 Register");
	}

	switch (AddrReg2) {
	case x86_EAX: Param += 0x00; break;
	case x86_EBX: Param += 0x18; break;
	case x86_ECX: Param += 0x08; break;
	case x86_EDX: Param += 0x10; break;
	case x86_ESI: Param += 0x30; break;
	case x86_EDI: Param += 0x38; break;
	case x86_ESP: Param += 0x20; break;
	case x86_EBP: Param += 0x28; break;
	default:
		DisplayError("MoveZxByteX86regPointerToX86reg\nUnhandled x86 Register");
	}
	PUTDST8(RecompPos,Param);
}

void MoveZxHalfX86regPointerToX86reg(int AddrReg1, int AddrReg2, int x86reg) {
	BYTE Param = 0x0;

	CPU_Message("      movzx %s, word ptr [%s+%s]",x86_Name(x86reg),x86_Name(AddrReg1), x86_Name(AddrReg2));

	PUTDST16(RecompPos,0xB70F);
	switch (x86reg) {
	case x86_EAX: PUTDST8(RecompPos,0x04); break;
	case x86_EBX: PUTDST8(RecompPos,0x1C); break;
	case x86_ECX: PUTDST8(RecompPos,0x0C); break;
	case x86_EDX: PUTDST8(RecompPos,0x14); break;
	case x86_ESI: PUTDST8(RecompPos,0x34); break;
	case x86_EDI: PUTDST8(RecompPos,0x3C); break;
	case x86_ESP: PUTDST8(RecompPos,0x24); break;
	case x86_EBP: PUTDST8(RecompPos,0x2C); break;
	default:
		DisplayError("MoveZxHalfX86regPointerToX86reg\nUnhandled x86 Register");
	}

	switch (AddrReg1) {
	case x86_EAX: Param = 0x00; break;
	case x86_EBX: Param = 0x03; break;
	case x86_ECX: Param = 0x01; break;
	case x86_EDX: Param = 0x02; break;
	case x86_ESI: Param = 0x06; break;
	case x86_EDI: Param = 0x07; break;
	default:
		DisplayError("MoveZxHalfX86regPointerToX86reg\nUnhandled x86 Register");
	}

	switch (AddrReg2) {
	case x86_EAX: Param += 0x00; break;
	case x86_EBX: Param += 0x18; break;
	case x86_ECX: Param += 0x08; break;
	case x86_EDX: Param += 0x10; break;
	case x86_ESI: Param += 0x30; break;
	case x86_EDI: Param += 0x38; break;
	case x86_ESP: Param += 0x20; break;
	case x86_EBP: Param += 0x28; break;
	default:
		DisplayError("MoveZxHalfX86regPointerToX86reg\nUnhandled x86 Register");
	}
	PUTDST8(RecompPos,Param);
}

void MoveZxN64MemToX86regByte(int x86reg, int AddrReg) {
	WORD x86Command = 0x0;

	CPU_Message("      movzx %s, byte ptr [%s+N64MEM]",x86_Name(x86reg),x86_Name(AddrReg));
	switch (AddrReg) {
	case x86_EAX: x86Command = 0x00B6; break;
	case x86_EBX: x86Command = 0x03B6; break;
	case x86_ECX: x86Command = 0x01B6; break;
	case x86_EDX: x86Command = 0x02B6; break;
	case x86_ESI: x86Command = 0x06B6; break;
	case x86_EDI: x86Command = 0x07B6; break;
	case x86_ESP: x86Command = 0x04B6; break;
	case x86_EBP: x86Command = 0x05B6; break;
	}
	switch (x86reg) {
	case x86_EAX: x86Command += 0x8000; break;
	case x86_EBX: x86Command += 0x9800; break;
	case x86_ECX: x86Command += 0x8800; break;
	case x86_EDX: x86Command += 0x9000; break;
	case x86_ESI: x86Command += 0xB000; break;
	case x86_EDI: x86Command += 0xB800; break;
	case x86_ESP: x86Command += 0xA000; break;
	case x86_EBP: x86Command += 0xA800; break;
	default:
		DisplayError("MoveZxN64MemToX86regByte\nInvalid x86 Register");
		break;
	}
	PUTDST8(RecompPos,0x0f);
	PUTDST16(RecompPos,x86Command);
	PUTDST32(RecompPos,N64MEM);
}

void MoveZxN64MemToX86regHalf(int x86reg, int AddrReg) {
	WORD x86Command = 0x0;

	CPU_Message("      movzx %s, word ptr [%s+N64MEM]",x86_Name(x86reg),x86_Name(AddrReg));

	switch (AddrReg) {
	case x86_EAX: x86Command = 0x00B7; break;
	case x86_EBX: x86Command = 0x03B7; break;
	case x86_ECX: x86Command = 0x01B7; break;
	case x86_EDX: x86Command = 0x02B7; break;
	case x86_ESI: x86Command = 0x06B7; break;
	case x86_EDI: x86Command = 0x07B7; break;
	case x86_ESP: x86Command = 0x04B7; break;
	case x86_EBP: x86Command = 0x05B7; break;
	}
	switch (x86reg) {
	case x86_EAX: x86Command += 0x8000; break;
	case x86_EBX: x86Command += 0x9800; break;
	case x86_ECX: x86Command += 0x8800; break;
	case x86_EDX: x86Command += 0x9000; break;
	case x86_ESI: x86Command += 0xB000; break;
	case x86_EDI: x86Command += 0xB800; break;
	case x86_ESP: x86Command += 0xA000; break;
	case x86_EBP: x86Command += 0xA800; break;
	}

	PUTDST8(RecompPos, 0x0f);
	PUTDST16(RecompPos,x86Command);
	PUTDST32(RecompPos,N64MEM);
}

void MoveZxVariableToX86regByte(void *Variable, char *VariableName, int x86reg) {
	CPU_Message("      movzx %s, byte ptr [%s]",x86_Name(x86reg),VariableName);

	PUTDST16(RecompPos, 0xb60f);

	switch (x86reg) {
	case x86_EAX: PUTDST8(RecompPos,0x05); break;
	case x86_EBX: PUTDST8(RecompPos,0x1D); break;
	case x86_ECX: PUTDST8(RecompPos,0x0D); break;
	case x86_EDX: PUTDST8(RecompPos,0x15); break;
	case x86_ESI: PUTDST8(RecompPos,0x35); break;
	case x86_EDI: PUTDST8(RecompPos,0x3D); break;
	case x86_ESP: PUTDST8(RecompPos,0x25); break;
	case x86_EBP: PUTDST8(RecompPos,0x2D); break;
	default: DisplayError("MoveZxVariableToX86regHalf\nUnknown x86 Register");
	}
    PUTDST32(RecompPos,Variable);
}

void MoveZxVariableToX86regHalf(void *Variable, char *VariableName, int x86reg) {
	CPU_Message("      movzx %s, word ptr [%s]",x86_Name(x86reg),VariableName);

	PUTDST16(RecompPos, 0xb70f);

	switch (x86reg) {
	case x86_EAX: PUTDST8(RecompPos,0x05); break;
	case x86_EBX: PUTDST8(RecompPos,0x1D); break;
	case x86_ECX: PUTDST8(RecompPos,0x0D); break;
	case x86_EDX: PUTDST8(RecompPos,0x15); break;
	case x86_ESI: PUTDST8(RecompPos,0x35); break;
	case x86_EDI: PUTDST8(RecompPos,0x3D); break;
	case x86_ESP: PUTDST8(RecompPos,0x25); break;
	case x86_EBP: PUTDST8(RecompPos,0x2D); break;
	default: DisplayError("MoveZxVariableToX86regHalf\nUnknown x86 Register");
	}
    PUTDST32(RecompPos,Variable);
}

void MulX86reg(int x86reg) {
	CPU_Message("      mul %s",x86_Name(x86reg));\
	switch (x86reg) {
	case x86_EAX: PUTDST16(RecompPos,0xE0F7); break;
	case x86_EBX: PUTDST16(RecompPos,0xE3F7); break;
	case x86_ECX: PUTDST16(RecompPos,0xE1F7); break;
	case x86_EDX: PUTDST16(RecompPos,0xE2F7); break;
	case x86_ESI: PUTDST16(RecompPos,0xE6F7); break;
	case x86_EDI: PUTDST16(RecompPos,0xE7F7); break;
	case x86_ESP: PUTDST16(RecompPos,0xE4F7); break;
	case x86_EBP: PUTDST16(RecompPos,0xE5F7); break;
	default:
		DisplayError("MulX86reg\nUnknown x86 Register");
	}
}

void NotX86Reg(int  x86Reg) {
	CPU_Message("      not %s",x86_Name(x86Reg));
	switch (x86Reg) {
	case x86_EAX: PUTDST16(RecompPos,0xD0F7); break;
	case x86_EBX: PUTDST16(RecompPos,0xD3F7); break;
	case x86_ECX: PUTDST16(RecompPos,0xD1F7); break;
	case x86_EDX: PUTDST16(RecompPos,0xD2F7); break;
	case x86_ESI: PUTDST16(RecompPos,0xD6F7); break;
	case x86_EDI: PUTDST16(RecompPos,0xD7F7); break;
	case x86_ESP: PUTDST16(RecompPos,0xD4F7); break;
	case x86_EBP: PUTDST16(RecompPos,0xD5F7); break;
	}
}

void OrConstToVariable(DWORD Const, void * Variable, char * VariableName) {
	CPU_Message("      or dword ptr [%s], 0x%X",VariableName, Const);
	PUTDST16(RecompPos,0x0D81);
	PUTDST32(RecompPos,Variable);
	PUTDST32(RecompPos,Const);
}

void OrConstToX86Reg(DWORD Const, int  x86Reg) {
	CPU_Message("      or %s, %Xh",x86_Name(x86Reg),Const);
	if ((Const & 0xFFFFFF80) != 0 && (Const & 0xFFFFFF80) != 0xFFFFFF80) {
		switch (x86Reg) {
		case x86_EAX: PUTDST16(RecompPos,0xC881); break;
		case x86_EBX: PUTDST16(RecompPos,0xCB81); break;
		case x86_ECX: PUTDST16(RecompPos,0xC981); break;
		case x86_EDX: PUTDST16(RecompPos,0xCA81); break;
		case x86_ESI: PUTDST16(RecompPos,0xCE81); break;
		case x86_EDI: PUTDST16(RecompPos,0xCF81); break;
		case x86_ESP: PUTDST16(RecompPos,0xCC81); break;
		case x86_EBP: PUTDST16(RecompPos,0xCD81); break;
		}
		PUTDST32(RecompPos, Const);
	} else {
		switch (x86Reg) {
		case x86_EAX: PUTDST16(RecompPos,0xC883); break;
		case x86_EBX: PUTDST16(RecompPos,0xCB83); break;
		case x86_ECX: PUTDST16(RecompPos,0xC983); break;
		case x86_EDX: PUTDST16(RecompPos,0xCA83); break;
		case x86_ESI: PUTDST16(RecompPos,0xCE83); break;
		case x86_EDI: PUTDST16(RecompPos,0xCF83); break;
		case x86_ESP: PUTDST16(RecompPos,0xCC83); break;
		case x86_EBP: PUTDST16(RecompPos,0xCD83); break;
		}
		PUTDST8(RecompPos, Const);
	}
}

void OrVariableToX86Reg(void * Variable, char * VariableName, int x86Reg) {
	CPU_Message("      or %s, dword ptr [%s]",x86_Name(x86Reg),VariableName);
	switch (x86Reg) {
	case x86_EAX: PUTDST16(RecompPos,0x050B); break;
	case x86_EBX: PUTDST16(RecompPos,0x1D0B); break;
	case x86_ECX: PUTDST16(RecompPos,0x0D0B); break;
	case x86_EDX: PUTDST16(RecompPos,0x150B); break;
	case x86_ESI: PUTDST16(RecompPos,0x350B); break;
	case x86_EDI: PUTDST16(RecompPos,0x3D0B); break;
	case x86_ESP: PUTDST16(RecompPos,0x250B); break;
	case x86_EBP: PUTDST16(RecompPos,0x2D0B); break;
	}
	PUTDST32(RecompPos,Variable);
}

void OrX86RegToVariable(void * Variable, char * VariableName, int x86Reg) {
	CPU_Message("      or dword ptr [%s], %s",VariableName, x86_Name(x86Reg));
	switch (x86Reg) {
	case x86_EAX: PUTDST16(RecompPos,0x0509); break;
	case x86_EBX: PUTDST16(RecompPos,0x1D09); break;
	case x86_ECX: PUTDST16(RecompPos,0x0D09); break;
	case x86_EDX: PUTDST16(RecompPos,0x1509); break;
	case x86_ESI: PUTDST16(RecompPos,0x3509); break;
	case x86_EDI: PUTDST16(RecompPos,0x3D09); break;
	case x86_ESP: PUTDST16(RecompPos,0x2509); break;
	case x86_EBP: PUTDST16(RecompPos,0x2D09); break;
	}
	PUTDST32(RecompPos,Variable);
}

void OrX86RegToX86Reg(int Destination, int Source) {
	WORD x86Command = 0x0;

	CPU_Message("      or %s, %s",x86_Name(Destination),x86_Name(Source));
	switch (Source) {
	case x86_EAX: x86Command = 0x000B; break;
	case x86_EBX: x86Command = 0x030B; break;
	case x86_ECX: x86Command = 0x010B; break;
	case x86_EDX: x86Command = 0x020B; break;
	case x86_ESI: x86Command = 0x060B; break;
	case x86_EDI: x86Command = 0x070B; break;
	case x86_ESP: x86Command = 0x040B; break;
	case x86_EBP: x86Command = 0x050B; break;
	}
	switch (Destination) {
	case x86_EAX: x86Command += 0xC000; break;
	case x86_EBX: x86Command += 0xD800; break;
	case x86_ECX: x86Command += 0xC800; break;
	case x86_EDX: x86Command += 0xD000; break;
	case x86_ESI: x86Command += 0xF000; break;
	case x86_EDI: x86Command += 0xF800; break;
	case x86_ESP: x86Command += 0xE000; break;
	case x86_EBP: x86Command += 0xE800; break;
	}
	PUTDST16(RecompPos,x86Command);
}

void Popad(void) {
	CPU_Message("      popad");
	PUTDST8(RecompPos,0x61);
}

void Pushad(void) {
	CPU_Message("      pushad");
	PUTDST8(RecompPos,0x60);
}

void Push(int x86reg) {
	CPU_Message("      push %s", x86_Name(x86reg));

	switch(x86reg) {
	case x86_EAX: PUTDST8(RecompPos, 0x50); break;
	case x86_EBX: PUTDST8(RecompPos, 0x53); break;
	case x86_ECX: PUTDST8(RecompPos, 0x51); break;
	case x86_EDX: PUTDST8(RecompPos, 0x52); break;
	case x86_ESI: PUTDST8(RecompPos, 0x56); break;
	case x86_EDI: PUTDST8(RecompPos, 0x57); break;
	case x86_ESP: PUTDST8(RecompPos, 0x54); break;
	case x86_EBP: PUTDST8(RecompPos, 0x55); break;
	}
}

void Pop(int x86reg) {
	CPU_Message("      pop %s", x86_Name(x86reg));

	switch(x86reg) {
	case x86_EAX: PUTDST8(RecompPos, 0x58); break;
	case x86_EBX: PUTDST8(RecompPos, 0x5B); break;
	case x86_ECX: PUTDST8(RecompPos, 0x59); break;
	case x86_EDX: PUTDST8(RecompPos, 0x5A); break;
	case x86_ESI: PUTDST8(RecompPos, 0x5E); break;
	case x86_EDI: PUTDST8(RecompPos, 0x5F); break;
	case x86_ESP: PUTDST8(RecompPos, 0x5C); break;
	case x86_EBP: PUTDST8(RecompPos, 0x5D); break;
	}
}

void PushImm32(char * String, DWORD Value) {
	CPU_Message("      push %s",String);
	PUTDST8(RecompPos,0x68);
	PUTDST32(RecompPos,Value);
}

void Ret(void) {
	CPU_Message("      ret");
	PUTDST8(RecompPos,0xC3);
}

void Seta(int x86reg) {
	CPU_Message("      seta %s",x86Byte_Name(x86reg));
	PUTDST16(RecompPos,0x970F);
	switch (x86reg) {
	case x86_EAX: PUTDST8(RecompPos,0xC0); break;
	case x86_EBX: PUTDST8(RecompPos,0xC3); break;
	case x86_ECX: PUTDST8(RecompPos,0xC1); break;
	case x86_EDX: PUTDST8(RecompPos,0xC2); break;
	default:
		DisplayError("Seta\nUnknown x86 Register");
	}
}

void SetaVariable(void * Variable, char * VariableName) {
	CPU_Message("      seta byte ptr [%s]",VariableName);
	PUTDST16(RecompPos,0x970F);
	PUTDST8(RecompPos,0x05);
	PUTDST32(RecompPos,Variable);
}

void Setae(int x86reg) {
	CPU_Message("      setae %s",x86Byte_Name(x86reg));
	PUTDST16(RecompPos,0x930F);
	switch (x86reg) {
	case x86_EAX: PUTDST8(RecompPos,0xC0); break;
	case x86_EBX: PUTDST8(RecompPos,0xC3); break;
	case x86_ECX: PUTDST8(RecompPos,0xC1); break;
	case x86_EDX: PUTDST8(RecompPos,0xC2); break;
	default:
		DisplayError("Seta\nUnknown x86 Register");
	}
}

void Setb(int x86reg) {
	CPU_Message("      setb %s",x86Byte_Name(x86reg));
	PUTDST16(RecompPos,0x920F);
	switch (x86reg) {
	case x86_EAX: PUTDST8(RecompPos,0xC0); break;
	case x86_EBX: PUTDST8(RecompPos,0xC3); break;
	case x86_ECX: PUTDST8(RecompPos,0xC1); break;
	case x86_EDX: PUTDST8(RecompPos,0xC2); break;
	default:
		DisplayError("Setb\nUnknown x86 Register");
	}
}

void SetbVariable(void * Variable, char * VariableName) {
	CPU_Message("      setb byte ptr [%s]",VariableName);
	PUTDST16(RecompPos,0x920F);
	PUTDST8(RecompPos,0x05);
	PUTDST32(RecompPos,Variable);
}

void Setg(int x86reg) {
	CPU_Message("      setg %s",x86Byte_Name(x86reg));
	PUTDST16(RecompPos,0x9F0F);
	switch (x86reg) {
	case x86_EAX: PUTDST8(RecompPos,0xC0); break;
	case x86_EBX: PUTDST8(RecompPos,0xC3); break;
	case x86_ECX: PUTDST8(RecompPos,0xC1); break;
	case x86_EDX: PUTDST8(RecompPos,0xC2); break;
	default:
		DisplayError("Setg\nUnknown x86 Register");
	}
}

void SetgVariable(void * Variable, char * VariableName) {
	CPU_Message("      setg byte ptr [%s]",VariableName);
	PUTDST16(RecompPos,0x9F0F);
	PUTDST8(RecompPos,0x05);
	PUTDST32(RecompPos,Variable);
}

void Setl(int x86reg) {
	CPU_Message("      setl %s",x86Byte_Name(x86reg));
	PUTDST16(RecompPos,0x9C0F);
	switch (x86reg) {
	case x86_EAX: PUTDST8(RecompPos,0xC0); break;
	case x86_EBX: PUTDST8(RecompPos,0xC3); break;
	case x86_ECX: PUTDST8(RecompPos,0xC1); break;
	case x86_EDX: PUTDST8(RecompPos,0xC2); break;
	default:
		DisplayError("Setl\nUnknown x86 Register");
	}
}

void SetlVariable(void * Variable, char * VariableName) {
	CPU_Message("      setl byte ptr [%s]",VariableName);
	PUTDST16(RecompPos,0x9C0F);
	PUTDST8(RecompPos,0x05);
	PUTDST32(RecompPos,Variable);
}


void Setz(int x86reg) {
	CPU_Message("      setz %s",x86Byte_Name(x86reg));
	PUTDST16(RecompPos,0x940F);
	switch (x86reg) {
	case x86_EAX: PUTDST8(RecompPos,0xC0); break;
	case x86_EBX: PUTDST8(RecompPos,0xC3); break;
	case x86_ECX: PUTDST8(RecompPos,0xC1); break;
	case x86_EDX: PUTDST8(RecompPos,0xC2); break;
	default:
		DisplayError("Setz\nUnknown x86 Register");
	}
}

void Setnz(int x86reg) {
	CPU_Message("      setnz %s",x86Byte_Name(x86reg));
	PUTDST16(RecompPos,0x950F);
	switch (x86reg) {
	case x86_EAX: PUTDST8(RecompPos,0xC0); break;
	case x86_EBX: PUTDST8(RecompPos,0xC3); break;
	case x86_ECX: PUTDST8(RecompPos,0xC1); break;
	case x86_EDX: PUTDST8(RecompPos,0xC2); break;
	default:
		DisplayError("Setnz\nUnknown x86 Register");
	}
}

void ShiftLeftDouble(int Destination, int Source) {
	unsigned char s = 0xC0;

	CPU_Message("      shld %s, %s, cl", x86_Name(Destination),x86_Name(Source));
	PUTDST16(RecompPos,0xA50F);

	switch (Destination) {
	case x86_EAX: s |= 0x00; break;
	case x86_EBX: s |= 0x03; break;
	case x86_ECX: s |= 0x01; break;
	case x86_EDX: s |= 0x02; break;
	case x86_ESI: s |= 0x06; break;
	case x86_EDI: s |= 0x07; break;
	case x86_ESP: s |= 0x04; break;
	case x86_EBP: s |= 0x05; break;
	}

	switch (Source) {
	case x86_EAX: s |= 0x00 << 3; break;
	case x86_EBX: s |= 0x03 << 3; break;
	case x86_ECX: s |= 0x01 << 3; break;
	case x86_EDX: s |= 0x02 << 3; break;
	case x86_ESI: s |= 0x06 << 3; break;
	case x86_EDI: s |= 0x07 << 3; break;
	case x86_ESP: s |= 0x04 << 3; break;
	case x86_EBP: s |= 0x05 << 3; break;
	}

	PUTDST8(RecompPos,s);
}

void ShiftLeftDoubleImmed(int Destination, int Source, BYTE Immediate) {
	unsigned char s = 0xC0;

	CPU_Message("      shld %s, %s, %Xh", x86_Name(Destination),x86_Name(Source), Immediate);
	PUTDST16(RecompPos,0xA40F);

	switch (Destination) {
	case x86_EAX: s |= 0x00; break;
	case x86_EBX: s |= 0x03; break;
	case x86_ECX: s |= 0x01; break;
	case x86_EDX: s |= 0x02; break;
	case x86_ESI: s |= 0x06; break;
	case x86_EDI: s |= 0x07; break;
	case x86_ESP: s |= 0x04; break;
	case x86_EBP: s |= 0x05; break;
	}

	switch (Source) {
	case x86_EAX: s |= 0x00 << 3; break;
	case x86_EBX: s |= 0x03 << 3; break;
	case x86_ECX: s |= 0x01 << 3; break;
	case x86_EDX: s |= 0x02 << 3; break;
	case x86_ESI: s |= 0x06 << 3; break;
	case x86_EDI: s |= 0x07 << 3; break;
	case x86_ESP: s |= 0x04 << 3; break;
	case x86_EBP: s |= 0x05 << 3; break;
	}

	PUTDST8(RecompPos,s);
	PUTDST8(RecompPos,Immediate);
}

void ShiftLeftSign(int x86reg) {
	CPU_Message("      shl %s, cl",x86_Name(x86reg));
	switch (x86reg) {
	case x86_EAX: PUTDST16(RecompPos,0xE0D3); break;
	case x86_EBX: PUTDST16(RecompPos,0xE3D3); break;
	case x86_ECX: PUTDST16(RecompPos,0xE1D3); break;
	case x86_EDX: PUTDST16(RecompPos,0xE2D3); break;
	case x86_ESI: PUTDST16(RecompPos,0xE6D3); break;
	case x86_EDI: PUTDST16(RecompPos,0xE7D3); break;
	case x86_ESP: PUTDST16(RecompPos,0xE4D3); break;
	case x86_EBP: PUTDST16(RecompPos,0xE5D3); break;
	}
}

void ShiftLeftSignImmed(int x86reg, BYTE Immediate) {
	CPU_Message("      shl %s, %Xh",x86_Name(x86reg),Immediate);
	switch (x86reg) {
	case x86_EAX: PUTDST16(RecompPos,0xE0C1); break;
	case x86_EBX: PUTDST16(RecompPos,0xE3C1); break;
	case x86_ECX: PUTDST16(RecompPos,0xE1C1); break;
	case x86_EDX: PUTDST16(RecompPos,0xE2C1); break;
	case x86_ESI: PUTDST16(RecompPos,0xE6C1); break;
	case x86_EDI: PUTDST16(RecompPos,0xE7C1); break;
	case x86_ESP: PUTDST16(RecompPos,0xE4C1); break;
	case x86_EBP: PUTDST16(RecompPos,0xE5C1); break;
	}
	PUTDST8(RecompPos,Immediate);
}

void ShiftRightSign(int x86reg) {
	CPU_Message("      sar %s, cl",x86_Name(x86reg));
	switch (x86reg) {
	case x86_EAX: PUTDST16(RecompPos,0xF8D3); break;
	case x86_EBX: PUTDST16(RecompPos,0xFBD3); break;
	case x86_ECX: PUTDST16(RecompPos,0xF9D3); break;
	case x86_EDX: PUTDST16(RecompPos,0xFAD3); break;
	case x86_ESI: PUTDST16(RecompPos,0xFED3); break;
	case x86_EDI: PUTDST16(RecompPos,0xFFD3); break;
	case x86_ESP: PUTDST16(RecompPos,0xFCD3); break;
	case x86_EBP: PUTDST16(RecompPos,0xFDD3); break;
	}
}

void ShiftRightSignImmed(int x86reg, BYTE Immediate) {
	CPU_Message("      sar %s, %Xh",x86_Name(x86reg),Immediate);
	switch (x86reg) {
	case x86_EAX: PUTDST16(RecompPos,0xF8C1); break;
	case x86_EBX: PUTDST16(RecompPos,0xFBC1); break;
	case x86_ECX: PUTDST16(RecompPos,0xF9C1); break;
	case x86_EDX: PUTDST16(RecompPos,0xFAC1); break;
	case x86_ESI: PUTDST16(RecompPos,0xFEC1); break;
	case x86_EDI: PUTDST16(RecompPos,0xFFC1); break;
	case x86_ESP: PUTDST16(RecompPos,0xFCC1); break;
	case x86_EBP: PUTDST16(RecompPos,0xFDC1); break;
	default:
		DisplayError("ShiftRightSignImmed\nUnknown x86 Register");
	}
	PUTDST8(RecompPos,Immediate);
}

void ShiftRightUnsign(int x86reg) {
	CPU_Message("      shr %s, cl",x86_Name(x86reg));
	switch (x86reg) {
	case x86_EAX: PUTDST16(RecompPos,0xE8D3); break;
	case x86_EBX: PUTDST16(RecompPos,0xEBD3); break;
	case x86_ECX: PUTDST16(RecompPos,0xE9D3); break;
	case x86_EDX: PUTDST16(RecompPos,0xEAD3); break;
	case x86_ESI: PUTDST16(RecompPos,0xEED3); break;
	case x86_EDI: PUTDST16(RecompPos,0xEFD3); break;
	case x86_ESP: PUTDST16(RecompPos,0xECD3); break;
	case x86_EBP: PUTDST16(RecompPos,0xEDD3); break;
	}
}

void ShiftRightDouble(int Destination, int Source) {
	unsigned char s = 0xC0;

	CPU_Message("      shrd %s, %s, cl", x86_Name(Destination),x86_Name(Source));
	PUTDST16(RecompPos,0xAD0F);

	switch (Destination) {
	case x86_EAX: s |= 0x00; break;
	case x86_EBX: s |= 0x03; break;
	case x86_ECX: s |= 0x01; break;
	case x86_EDX: s |= 0x02; break;
	case x86_ESI: s |= 0x06; break;
	case x86_EDI: s |= 0x07; break;
	case x86_ESP: s |= 0x04; break;
	case x86_EBP: s |= 0x05; break;
	}

	switch (Source) {
	case x86_EAX: s |= 0x00 << 3; break;
	case x86_EBX: s |= 0x03 << 3; break;
	case x86_ECX: s |= 0x01 << 3; break;
	case x86_EDX: s |= 0x02 << 3; break;
	case x86_ESI: s |= 0x06 << 3; break;
	case x86_EDI: s |= 0x07 << 3; break;
	case x86_ESP: s |= 0x04 << 3; break;
	case x86_EBP: s |= 0x05 << 3; break;
	}

	PUTDST8(RecompPos,s);
}

void ShiftRightDoubleImmed(int Destination, int Source, BYTE Immediate) {
	unsigned char s = 0xC0;

	CPU_Message("      shrd %s, %s, %Xh", x86_Name(Destination),x86_Name(Source), Immediate);
	PUTDST16(RecompPos,0xAC0F);

	switch (Destination) {
	case x86_EAX: s |= 0x00; break;
	case x86_EBX: s |= 0x03; break;
	case x86_ECX: s |= 0x01; break;
	case x86_EDX: s |= 0x02; break;
	case x86_ESI: s |= 0x06; break;
	case x86_EDI: s |= 0x07; break;
	case x86_ESP: s |= 0x04; break;
	case x86_EBP: s |= 0x05; break;
	}

	switch (Source) {
	case x86_EAX: s |= 0x00 << 3; break;
	case x86_EBX: s |= 0x03 << 3; break;
	case x86_ECX: s |= 0x01 << 3; break;
	case x86_EDX: s |= 0x02 << 3; break;
	case x86_ESI: s |= 0x06 << 3; break;
	case x86_EDI: s |= 0x07 << 3; break;
	case x86_ESP: s |= 0x04 << 3; break;
	case x86_EBP: s |= 0x05 << 3; break;
	}

	PUTDST8(RecompPos,s);
	PUTDST8(RecompPos,Immediate);
}

void ShiftRightUnsignImmed(int x86reg, BYTE Immediate) {
	CPU_Message("      shr %s, %Xh",x86_Name(x86reg),Immediate);
	switch (x86reg) {
	case x86_EAX: PUTDST16(RecompPos,0xE8C1); break;
	case x86_EBX: PUTDST16(RecompPos,0xEBC1); break;
	case x86_ECX: PUTDST16(RecompPos,0xE9C1); break;
	case x86_EDX: PUTDST16(RecompPos,0xEAC1); break;
	case x86_ESI: PUTDST16(RecompPos,0xEEC1); break;
	case x86_EDI: PUTDST16(RecompPos,0xEFC1); break;
	case x86_ESP: PUTDST16(RecompPos,0xECC1); break;
	case x86_EBP: PUTDST16(RecompPos,0xEDC1); break;
	}
	PUTDST8(RecompPos,Immediate);
}

void SbbConstFromX86Reg (int x86Reg, DWORD Const) {
	CPU_Message("      sbb %s, %Xh",x86_Name(x86Reg),Const);
	if ((Const & 0xFFFFFF80) != 0 && (Const & 0xFFFFFF80) != 0xFFFFFF80) {
		switch (x86Reg) {
		case x86_EAX: PUTDST16(RecompPos,0xD881); break;
		case x86_EBX: PUTDST16(RecompPos,0xDB81); break;
		case x86_ECX: PUTDST16(RecompPos,0xD981); break;
		case x86_EDX: PUTDST16(RecompPos,0xDA81); break;
		case x86_ESI: PUTDST16(RecompPos,0xDE81); break;
		case x86_EDI: PUTDST16(RecompPos,0xDF81); break;
		case x86_ESP: PUTDST16(RecompPos,0xDC81); break;
		case x86_EBP: PUTDST16(RecompPos,0xDD81); break;
		}
		PUTDST32(RecompPos, Const);
	} else {
		switch (x86Reg) {
		case x86_EAX: PUTDST16(RecompPos,0xD883); break;
		case x86_EBX: PUTDST16(RecompPos,0xDB83); break;
		case x86_ECX: PUTDST16(RecompPos,0xD983); break;
		case x86_EDX: PUTDST16(RecompPos,0xDA83); break;
		case x86_ESI: PUTDST16(RecompPos,0xDE83); break;
		case x86_EDI: PUTDST16(RecompPos,0xDF83); break;
		case x86_ESP: PUTDST16(RecompPos,0xDC83); break;
		case x86_EBP: PUTDST16(RecompPos,0xDD83); break;
		}
		PUTDST8(RecompPos, Const);
	}
}

void SbbVariableFromX86reg(int x86reg, void * Variable, char * VariableName) {
	CPU_Message("      sbb %s, dword ptr [%s]",x86_Name(x86reg),VariableName);
	switch (x86reg) {
	case x86_EAX: PUTDST16(RecompPos,0x051B); break;
	case x86_EBX: PUTDST16(RecompPos,0x1D1B); break;
	case x86_ECX: PUTDST16(RecompPos,0x0D1B); break;
	case x86_EDX: PUTDST16(RecompPos,0x151B); break;
	case x86_ESI: PUTDST16(RecompPos,0x351B); break;
	case x86_EDI: PUTDST16(RecompPos,0x3D1B); break;
	case x86_ESP: PUTDST16(RecompPos,0x251B); break;
	case x86_EBP: PUTDST16(RecompPos,0x2D1B); break;
	default:
		DisplayError("SbbVariableFromX86reg\nUnknown x86 Register");
	}
    PUTDST32(RecompPos,Variable); 
}

void SbbX86RegToX86Reg(int Destination, int Source) {
	WORD x86Command = 0x0;
	CPU_Message("      sbb %s, %s",x86_Name(Destination),x86_Name(Source));
	switch (Source) {
	case x86_EAX: x86Command = 0x001B; break;
	case x86_EBX: x86Command = 0x031B; break;
	case x86_ECX: x86Command = 0x011B; break;
	case x86_EDX: x86Command = 0x021B; break;
	case x86_ESI: x86Command = 0x061B; break;
	case x86_EDI: x86Command = 0x071B; break;
	case x86_ESP: x86Command = 0x041B; break;
	case x86_EBP: x86Command = 0x051B; break;
	}
	switch (Destination) {
	case x86_EAX: x86Command += 0xC000; break;
	case x86_EBX: x86Command += 0xD800; break;
	case x86_ECX: x86Command += 0xC800; break;
	case x86_EDX: x86Command += 0xD000; break;
	case x86_ESI: x86Command += 0xF000; break;
	case x86_EDI: x86Command += 0xF800; break;
	case x86_ESP: x86Command += 0xE000; break;
	case x86_EBP: x86Command += 0xE800; break;
	}
	PUTDST16(RecompPos,x86Command);
}

void SubConstFromVariable (DWORD Const, void *Variable, char *VariableName) {
	CPU_Message("      sub dword ptr [%s], 0x%X",VariableName, Const);\
	PUTDST16(RecompPos,0x2D81);
	PUTDST32(RecompPos,Variable);
	PUTDST32(RecompPos,Const);
}

void SubConstFromX86Reg (int x86Reg, DWORD Const) {
	CPU_Message("      sub %s, %Xh",x86_Name(x86Reg),Const);
	if ((Const & 0xFFFFFF80) != 0 && (Const & 0xFFFFFF80) != 0xFFFFFF80) {
		switch (x86Reg) {
		case x86_EAX: PUTDST16(RecompPos,0xE881); break;
		case x86_EBX: PUTDST16(RecompPos,0xEB81); break;
		case x86_ECX: PUTDST16(RecompPos,0xE981); break;
		case x86_EDX: PUTDST16(RecompPos,0xEA81); break;
		case x86_ESI: PUTDST16(RecompPos,0xEE81); break;
		case x86_EDI: PUTDST16(RecompPos,0xEF81); break;
		case x86_ESP: PUTDST16(RecompPos,0xEC81); break;
		case x86_EBP: PUTDST16(RecompPos,0xED81); break;
		}
		PUTDST32(RecompPos, Const);
	} else {
		switch (x86Reg) {
		case x86_EAX: PUTDST16(RecompPos,0xE883); break;
		case x86_EBX: PUTDST16(RecompPos,0xEB83); break;
		case x86_ECX: PUTDST16(RecompPos,0xE983); break;
		case x86_EDX: PUTDST16(RecompPos,0xEA83); break;
		case x86_ESI: PUTDST16(RecompPos,0xEE83); break;
		case x86_EDI: PUTDST16(RecompPos,0xEF83); break;
		case x86_ESP: PUTDST16(RecompPos,0xEC83); break;
		case x86_EBP: PUTDST16(RecompPos,0xED83); break;
		}
		PUTDST8(RecompPos, Const);
	}
}

void SubVariableFromX86reg(int x86reg, void * Variable, char * VariableName) {
	CPU_Message("      sub %s, dword ptr [%s]",x86_Name(x86reg),VariableName);
	switch (x86reg) {
	case x86_EAX: PUTDST16(RecompPos,0x052B); break;
	case x86_EBX: PUTDST16(RecompPos,0x1D2B); break;
	case x86_ECX: PUTDST16(RecompPos,0x0D2B); break;
	case x86_EDX: PUTDST16(RecompPos,0x152B); break;
	case x86_ESI: PUTDST16(RecompPos,0x352B); break;
	case x86_EDI: PUTDST16(RecompPos,0x3D2B); break;
	case x86_ESP: PUTDST16(RecompPos,0x252B); break;
	case x86_EBP: PUTDST16(RecompPos,0x2D2B); break;
	default:
		DisplayError("SubVariableFromX86reg\nUnknown x86 Register");
	}
    PUTDST32(RecompPos,Variable); 
}

void SubX86RegToX86Reg(int Destination, int Source) {
	WORD x86Command = 0x0;
	CPU_Message("      sub %s, %s",x86_Name(Destination),x86_Name(Source));
	switch (Source) {
	case x86_EAX: x86Command = 0x002B; break;
	case x86_EBX: x86Command = 0x032B; break;
	case x86_ECX: x86Command = 0x012B; break;
	case x86_EDX: x86Command = 0x022B; break;
	case x86_ESI: x86Command = 0x062B; break;
	case x86_EDI: x86Command = 0x072B; break;
	case x86_ESP: x86Command = 0x042B; break;
	case x86_EBP: x86Command = 0x052B; break;
	}
	switch (Destination) {
	case x86_EAX: x86Command += 0xC000; break;
	case x86_EBX: x86Command += 0xD800; break;
	case x86_ECX: x86Command += 0xC800; break;
	case x86_EDX: x86Command += 0xD000; break;
	case x86_ESI: x86Command += 0xF000; break;
	case x86_EDI: x86Command += 0xF800; break;
	case x86_ESP: x86Command += 0xE000; break;
	case x86_EBP: x86Command += 0xE800; break;
	}
	PUTDST16(RecompPos,x86Command);
}

void TestConstToX86Reg(DWORD Const, int x86reg) {
	CPU_Message("      test %s, 0x%X",x86_Name(x86reg), Const);
	
	switch (x86reg) {
	case x86_EAX: PUTDST8 (RecompPos,0xA9); break;
	case x86_EBX: PUTDST16(RecompPos,0xC3F7); break;
	case x86_ECX: PUTDST16(RecompPos,0xC1F7); break;
	case x86_EDX: PUTDST16(RecompPos,0xC2F7); break;
	case x86_ESI: PUTDST16(RecompPos,0xC6F7); break;
	case x86_EDI: PUTDST16(RecompPos,0xC7F7); break;
	case x86_ESP: PUTDST16(RecompPos,0xC4F7); break;
	case x86_EBP: PUTDST16(RecompPos,0xC5F7); break;
	}
	PUTDST32(RecompPos,Const);
}

void TestVariable(DWORD Const, void * Variable, char * VariableName) {
	CPU_Message("      test dword ptr ds:[%s], 0x%X",VariableName, Const);
	PUTDST16(RecompPos,0x05F7);
	PUTDST32(RecompPos,Variable);
	PUTDST32(RecompPos,Const);
}

void TestX86RegToX86Reg(int Destination, int Source) {
	WORD x86Command = 0x0;
	CPU_Message("      test %s, %s",x86_Name(Destination),x86_Name(Source));
	switch (Source) {
	case x86_EAX: x86Command = 0x0085; break;
	case x86_EBX: x86Command = 0x0385; break;
	case x86_ECX: x86Command = 0x0185; break;
	case x86_EDX: x86Command = 0x0285; break;
	case x86_ESI: x86Command = 0x0685; break;
	case x86_EDI: x86Command = 0x0785; break;
	case x86_ESP: x86Command = 0x0485; break;
	case x86_EBP: x86Command = 0x0585; break;
	}
	switch (Destination) {
	case x86_EAX: x86Command += 0xC000; break;
	case x86_EBX: x86Command += 0xD800; break;
	case x86_ECX: x86Command += 0xC800; break;
	case x86_EDX: x86Command += 0xD000; break;
	case x86_ESI: x86Command += 0xF000; break;
	case x86_EDI: x86Command += 0xF800; break;
	case x86_ESP: x86Command += 0xE000; break;
	case x86_EBP: x86Command += 0xE800; break;
	}
	PUTDST16(RecompPos,x86Command);
}

void XorConstToX86Reg(int x86Reg, DWORD Const) {
	CPU_Message("      xor %s, %Xh",x86_Name(x86Reg),Const);
	if ((Const & 0xFFFFFF80) != 0 && (Const & 0xFFFFFF80) != 0xFFFFFF80) {
		switch (x86Reg) {
		case x86_EAX: PUTDST16(RecompPos,0xF081); break;
		case x86_EBX: PUTDST16(RecompPos,0xF381); break;
		case x86_ECX: PUTDST16(RecompPos,0xF181); break;
		case x86_EDX: PUTDST16(RecompPos,0xF281); break;
		case x86_ESI: PUTDST16(RecompPos,0xF681); break;
		case x86_EDI: PUTDST16(RecompPos,0xF781); break;
		case x86_ESP: PUTDST16(RecompPos,0xF481); break;
		case x86_EBP: PUTDST16(RecompPos,0xF581); break;
		}
		PUTDST32(RecompPos, Const);
	} else {
		switch (x86Reg) {
		case x86_EAX: PUTDST16(RecompPos,0xF083); break;
		case x86_EBX: PUTDST16(RecompPos,0xF383); break;
		case x86_ECX: PUTDST16(RecompPos,0xF183); break;
		case x86_EDX: PUTDST16(RecompPos,0xF283); break;
		case x86_ESI: PUTDST16(RecompPos,0xF683); break;
		case x86_EDI: PUTDST16(RecompPos,0xF783); break;
		case x86_ESP: PUTDST16(RecompPos,0xF483); break;
		case x86_EBP: PUTDST16(RecompPos,0xF583); break;
		}
		PUTDST8(RecompPos, Const);
	}
}

void XorX86RegToX86Reg(int Source, int Destination) {
	WORD x86Command = 0x0;

	CPU_Message("      xor %s, %s",x86_Name(Source),x86_Name(Destination));
		
	switch (Source) {
	case x86_EAX: x86Command = 0x0031; break;
	case x86_EBX: x86Command = 0x0331; break;
	case x86_ECX: x86Command = 0x0131; break;
	case x86_EDX: x86Command = 0x0231; break;
	case x86_ESI: x86Command = 0x0631; break;
	case x86_EDI: x86Command = 0x0731; break;
	case x86_ESP: x86Command = 0x0431; break;
	case x86_EBP: x86Command = 0x0531; break;
	}
	switch (Destination) {
	case x86_EAX: x86Command += 0xC000; break;
	case x86_EBX: x86Command += 0xD800; break;
	case x86_ECX: x86Command += 0xC800; break;
	case x86_EDX: x86Command += 0xD000; break;
	case x86_ESI: x86Command += 0xF000; break;
	case x86_EDI: x86Command += 0xF800; break;
	case x86_ESP: x86Command += 0xE000; break;
	case x86_EBP: x86Command += 0xE800; break;
	}
	PUTDST16(RecompPos,x86Command);
}

void XorVariableToX86reg(void *Variable, char *VariableName, int x86reg) {
	CPU_Message("      Xor %s, dword ptr [%s]",x86_Name(x86reg),VariableName);
	switch (x86reg) {
	case x86_EAX: PUTDST16(RecompPos,0x0533); break;
	case x86_EBX: PUTDST16(RecompPos,0x1D33); break;
	case x86_ECX: PUTDST16(RecompPos,0x0D33); break;
	case x86_EDX: PUTDST16(RecompPos,0x1533); break;
	case x86_ESI: PUTDST16(RecompPos,0x3533); break;
	case x86_EDI: PUTDST16(RecompPos,0x3D33); break;
	case x86_ESP: PUTDST16(RecompPos,0x2533); break;
	case x86_EBP: PUTDST16(RecompPos,0x2D33); break;
	default: DisplayError("XorVariableToX86reg\nUnknown x86 Register");
	}
    PUTDST32(RecompPos,Variable);
}
