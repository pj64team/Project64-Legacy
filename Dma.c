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
#include "debugger.h"
#include "CPU.h"

int DMAUsed;

void FirstDMA (void) {
	switch (GetCicChipID(ROM)) {
	case CIC_NUS_6101: *(DWORD *)&N64MEM[0x318] = RdramSize; break;
	case CIC_NUS_6102: *(DWORD *)&N64MEM[0x318] = RdramSize; break;
	case CIC_NUS_6103: *(DWORD *)&N64MEM[0x318] = RdramSize; break;
	case CIC_NUS_6105: *(DWORD *)&N64MEM[0x3F0] = RdramSize; break;
	case CIC_NUS_6106: *(DWORD *)&N64MEM[0x318] = RdramSize; break;
	case CIC_NUS_5167: *(DWORD *)&N64MEM[0x318] = RdramSize; break;
	case CIC_NUS_8303: *(DWORD *)&N64MEM[0x318] = RdramSize; break;
	case CIC_NUS_8401: *(DWORD *)&N64MEM[0x318] = RdramSize; break;
	case CIC_NUS_DDUS: *(DWORD *)&N64MEM[0x318] = RdramSize; break;
	case CIC_NUS_XENO: *(DWORD *)&N64MEM[0x318] = RdramSize; break;
	default: 
		*(DWORD *)&N64MEM[0x318] = RdramSize;
		if (ShowDebugMessages)
			DisplayError("Unhandled CicChip(%d) in first DMA",GetCicChipID(ROM));
	}
}

void PI_DMA_READ (void) {
//	PI_STATUS_REG |= PI_STATUS_DMA_BUSY;
	PI_DRAM_ADDR_REG &= 0x1FFFFFFF;

	PI_RD_LEN_REG = (PI_RD_LEN_REG & 1) ? PI_RD_LEN_REG : PI_RD_LEN_REG + 1;	// Fix for Ai Shogi 3

	PI_CART_ADDR_REG &= ~1;	// Taz Express fix
	PI_DRAM_ADDR_REG &= ~7;	// Tax Express fix

	if ( PI_DRAM_ADDR_REG + PI_RD_LEN_REG + 1 > RdramSize) {
		if (ShowDebugMessages)
			DisplayError("PI_DMA_READ not in Memory");
		PI_STATUS_REG &= ~PI_STATUS_DMA_BUSY;
		MI_INTR_REG |= MI_INTR_PI;
		CheckInterrupts();
		return;
	}

	// Trying to fix saves for Dezaemon 3D (J)
	// There is likely a better way to do this
	if ((PI_CART_ADDR_REG >= 0x08000000 && PI_CART_ADDR_REG <= 0x08010000) ||
		(PI_CART_ADDR_REG >= 0x08040000 && PI_CART_ADDR_REG <= 0x08050000) ||
		(PI_CART_ADDR_REG >= 0x08080000 && PI_CART_ADDR_REG <= 0x08090000))
	{
		switch (SaveUsing) {
		case Auto:
			SaveUsing = Sram;
		case Sram:
			DmaToSram(
				N64MEM+PI_DRAM_ADDR_REG,
				PI_CART_ADDR_REG - 0x08000000,
				PI_RD_LEN_REG + 1
				);
			PI_STATUS_REG &= ~PI_STATUS_DMA_BUSY;
			MI_INTR_REG |= MI_INTR_PI;
			CheckInterrupts();
			break;
		case FlashRam:
			DmaToFlashram(
				N64MEM+PI_DRAM_ADDR_REG,
				PI_CART_ADDR_REG - 0x08000000,
				PI_WR_LEN_REG + 1
				);
			PI_STATUS_REG &= ~PI_STATUS_DMA_BUSY;
			MI_INTR_REG |= MI_INTR_PI;
			CheckInterrupts();
			break;
		}
		return;
	}

	if (SaveUsing == FlashRam) {
		if (ShowDebugMessages)
			DisplayError("**** FLashRam DMA Read address %X *****",PI_CART_ADDR_REG);
		PI_STATUS_REG &= ~PI_STATUS_DMA_BUSY;
		MI_INTR_REG |= MI_INTR_PI;
		CheckInterrupts();
		return;
	}
	if (ShowDebugMessages)
		DisplayError("PI_DMA_READ where are you dmaing to ?");
	PI_STATUS_REG &= ~PI_STATUS_DMA_BUSY;
	MI_INTR_REG |= MI_INTR_PI;
	CheckInterrupts();
	return;
}

void PI_DMA_WRITE (void) {
	DWORD i;	
	PI_DRAM_ADDR_REG &= 0x1FFFFFFF;

	if (PI_WR_LEN_REG > 0x300)
		PI_WR_LEN_REG = ((PI_WR_LEN_REG & 1)) ? PI_WR_LEN_REG : PI_WR_LEN_REG + 1;	// Fix for Ai Shogi 3
	
	int wr_len = PI_WR_LEN_REG;
	int wr_len_cart = PI_WR_LEN_REG;

	if (!wr_len) wr_len = 7;
	if (!wr_len_cart) wr_len_cart = 1;

	PI_WR_LEN_REG -= PI_DRAM_ADDR_REG & 6;

	PI_CART_ADDR_REG &= ~1;	// Taz Express fix
	PI_DRAM_ADDR_REG &= ~1;	// Taz Express fix

	PI_STATUS_REG |= PI_STATUS_DMA_BUSY;
	if ( PI_DRAM_ADDR_REG + PI_WR_LEN_REG + 1 > RdramSize) {
		if (ShowDebugMessages)
			DisplayError("PI_DMA_WRITE not in Memory");
		PI_STATUS_REG &= ~PI_STATUS_DMA_BUSY;
		MI_INTR_REG |= MI_INTR_PI;
		CheckInterrupts();
		return;
	}

	// Trying to fix saves for Dezaemon 3D (J)
	// There is likely a better way to do this
	if ((PI_CART_ADDR_REG >= 0x08000000 && PI_CART_ADDR_REG <= 0x08010000) ||
		(PI_CART_ADDR_REG >= 0x08040000 && PI_CART_ADDR_REG <= 0x08050000) ||
		(PI_CART_ADDR_REG >= 0x08080000 && PI_CART_ADDR_REG <= 0x08090000))
	{
		switch (SaveUsing) {
		case Auto:
			SaveUsing = Sram;
		case Sram:
			DmaFromSram(
				N64MEM+PI_DRAM_ADDR_REG,
				PI_CART_ADDR_REG - 0x08000000,
				PI_WR_LEN_REG + 1
				);
			PI_STATUS_REG &= ~PI_STATUS_DMA_BUSY;
			MI_INTR_REG |= MI_INTR_PI;
			CheckInterrupts();
			break;
		case FlashRam:
			DmaFromFlashram(
				N64MEM+PI_DRAM_ADDR_REG,
				PI_CART_ADDR_REG - 0x08000000,
				PI_WR_LEN_REG + 1
				);
			PI_STATUS_REG &= ~PI_STATUS_DMA_BUSY;
			MI_INTR_REG |= MI_INTR_PI;
			CheckInterrupts();
			break;
		}
		return;
	}
	
	if ( PI_CART_ADDR_REG >= 0x06000000 && PI_CART_ADDR_REG < 0x08000000) {
		PI_CART_ADDR_REG -= 0x06000000;
		if (PI_CART_ADDR_REG + PI_WR_LEN_REG + 1 < RomFileSize) {
			for (i = 0; i < PI_WR_LEN_REG + 1; i ++) {
				*(N64MEM+((PI_DRAM_ADDR_REG + i) ^ 3)) =  *(ROM+((PI_CART_ADDR_REG + i) ^ 3));
			}
		} else if (RomFileSize > PI_CART_ADDR_REG) {
			DWORD Len;
			Len = RomFileSize - PI_CART_ADDR_REG;
			for (i = 0; i < Len; i ++) {
				*(N64MEM+((PI_DRAM_ADDR_REG + i) ^ 3)) =  *(ROM+((PI_CART_ADDR_REG + i) ^ 3));
			}
			for (i = Len; i < PI_WR_LEN_REG + 1 - Len; i ++) {
				*(N64MEM+((PI_DRAM_ADDR_REG + i) ^ 3)) =  0;
			}
		}
		PI_CART_ADDR_REG += 0x06000000;

		PI_DRAM_ADDR_REG += wr_len + 1;
		PI_CART_ADDR_REG += wr_len_cart + 1;
		PI_WR_LEN_REG = 0x7f;
		PI_RD_LEN_REG = 0x7f;

		if (!DMAUsed) { 
			DMAUsed = TRUE;
			FirstDMA(); 
		}
		PI_STATUS_REG &= ~PI_STATUS_DMA_BUSY;
		MI_INTR_REG |= MI_INTR_PI;
		CheckInterrupts();
		//ChangeTimer(PiTimer,(int)(PI_WR_LEN_REG * 8.9) + 50);
		//ChangeTimer(PiTimer,(int)(PI_WR_LEN_REG * 8.9));
		return;
	}
	
	if ( PI_CART_ADDR_REG >= 0x10000000 && PI_CART_ADDR_REG <= 0x1FBFFFFF) {
		PI_CART_ADDR_REG -= 0x10000000;
		if (PI_CART_ADDR_REG + PI_WR_LEN_REG + 1 < RomFileSize) {
			for (i = 0; i < PI_WR_LEN_REG + 1; i ++) {
				*(N64MEM+((PI_DRAM_ADDR_REG + i) ^ 3)) =  *(ROM+((PI_CART_ADDR_REG + i) ^ 3));
			}
		} else if (RomFileSize > PI_CART_ADDR_REG) {
			DWORD Len;
			Len = RomFileSize - PI_CART_ADDR_REG;
			for (i = 0; i < Len; i ++) {
				*(N64MEM+((PI_DRAM_ADDR_REG + i) ^ 3)) =  *(ROM+((PI_CART_ADDR_REG + i) ^ 3));
			}
			for (i = Len; i < PI_WR_LEN_REG + 1 - Len; i ++) {
				*(N64MEM+((PI_DRAM_ADDR_REG + i) ^ 3)) =  0;
			}
		}
		PI_CART_ADDR_REG += 0x10000000;

		PI_DRAM_ADDR_REG += wr_len + 1;
		PI_CART_ADDR_REG += wr_len_cart + 1;
		PI_DRAM_ADDR_REG = (PI_DRAM_ADDR_REG) & 0x7FFFFF;
		PI_WR_LEN_REG = 0x7f;
		PI_RD_LEN_REG = 0x7f;

		if (!DMAUsed) { 
			DMAUsed = TRUE;
			FirstDMA(); 
		}
		PI_STATUS_REG &= ~PI_STATUS_DMA_BUSY;
		MI_INTR_REG |= MI_INTR_PI;
		CheckInterrupts();
		//ChangeTimer(PiTimer,(int)(PI_WR_LEN_REG * 8.9) + 50);
		//ChangeTimer(PiTimer,(int)(PI_WR_LEN_REG * 8.9));
		return;
	}
	
	if (HaveDebugger && ShowUnhandledMemory) { DisplayError("PI_DMA_WRITE not in ROM"); }
	PI_STATUS_REG &= ~PI_STATUS_DMA_BUSY;
	MI_INTR_REG |= MI_INTR_PI;
	CheckInterrupts();

}

void SI_DMA_READ (void) {
	BYTE * PifRamPos = &PIF_Ram[0];

	SI_DRAM_ADDR_REG &= 0x7FFFFFFF;

	if ((int)SI_DRAM_ADDR_REG > (int)RdramSize) {
		if (ShowDebugMessages)
			DisplayError("SI DMA\nSI_DRAM_ADDR_REG not in RDRam space");
		return;
	}
	
	PifRamRead();
	SI_DRAM_ADDR_REG &= 0xFFFFFFF8;
	if ((int)SI_DRAM_ADDR_REG < 0) {
		int count, RdramPos;

		RdramPos = (int)SI_DRAM_ADDR_REG;
		for (count = 0; count < 0x40; count++, RdramPos++) {
			if (RdramPos < 0) { continue; }
			N64MEM[RdramPos ^3] = PIF_Ram[count];
		}
	} else {
		_asm {
			mov edi, dword ptr [RegSI]
			mov edi, dword ptr [edi]
			add edi, N64MEM
			mov ecx, PifRamPos
			mov edx, 0		
	memcpyloop:
			mov eax, dword ptr [ecx + edx]
			bswap eax
			mov  dword ptr [edi + edx],eax
			mov eax, dword ptr [ecx + edx + 4]
			bswap eax
			mov  dword ptr [edi + edx + 4],eax
			mov eax, dword ptr [ecx + edx + 8]
			bswap eax
			mov  dword ptr [edi + edx + 8],eax
			mov eax, dword ptr [ecx + edx + 12]
			bswap eax
			mov  dword ptr [edi + edx + 12],eax
			add edx, 16
			cmp edx, 64
			jb memcpyloop
		}
	}
	
	if (HaveDebugger && LogOptions.LogPRDMAMemStores) {
			int count;
			char HexData[100], AsciiData[100], Addon[20];
			LogMessage("\tData DMAed to RDRAM:");			
			LogMessage("\t--------------------");
			for (count = 0; count < 16; count ++ ) {
				if ((count % 4) == 0) { 
					sprintf(HexData,"\0"); 
					sprintf(AsciiData,"\0"); 
				}
 				sprintf(Addon,"%02X %02X %02X %02X", 
					PIF_Ram[(count << 2) + 0], PIF_Ram[(count << 2) + 1], 
					PIF_Ram[(count << 2) + 2], PIF_Ram[(count << 2) + 3] );
				strcat(HexData,Addon);
				if (((count + 1) % 4) != 0) {
					sprintf(Addon,"-");
					strcat(HexData,Addon);
				} 
			
				sprintf(Addon,"%c%c%c%c", 
					PIF_Ram[(count << 2) + 0], PIF_Ram[(count << 2) + 1], 
					PIF_Ram[(count << 2) + 2], PIF_Ram[(count << 2) + 3] );
				strcat(AsciiData,Addon);
			
				if (((count + 1) % 4) == 0) {
					LogMessage("\t%s %s",HexData, AsciiData);
				} 
			}
			LogMessage("");
	}

	if (DelaySI) {
		ChangeTimer(SiTimer,0x900);
	} else {
		MI_INTR_REG |= MI_INTR_SI;
		SI_STATUS_REG |= SI_STATUS_INTERRUPT;
		CheckInterrupts();
	}
}

void SI_DMA_WRITE (void) {
	BYTE * PifRamPos = &PIF_Ram[0];

	SI_DRAM_ADDR_REG &= 0x7FFFFFFF;

	if ((int)SI_DRAM_ADDR_REG > (int)RdramSize) {
		if (ShowDebugMessages)
			DisplayError("SI DMA\nSI_DRAM_ADDR_REG not in RDRam space");
		return;
	}
	
	SI_DRAM_ADDR_REG &= 0xFFFFFFF8;
	if ((int)SI_DRAM_ADDR_REG < 0) {
		int count, RdramPos;

		RdramPos = (int)SI_DRAM_ADDR_REG;
		for (count = 0; count < 0x40; count++, RdramPos++) {
			if (RdramPos < 0) { PIF_Ram[count] = 0; continue; }
			PIF_Ram[count] = N64MEM[RdramPos ^3];
		}
	} else {
		_asm {
			mov ecx, dword ptr [RegSI]
			mov ecx, dword ptr [ecx]
			add ecx, N64MEM
			mov edi, PifRamPos
			mov edx, 0		
	memcpyloop:
			mov eax, dword ptr [ecx + edx]
			bswap eax
			mov  dword ptr [edi + edx],eax
			mov eax, dword ptr [ecx + edx + 4]
			bswap eax
			mov  dword ptr [edi + edx + 4],eax
			mov eax, dword ptr [ecx + edx + 8]
			bswap eax
			mov  dword ptr [edi + edx + 8],eax
			mov eax, dword ptr [ecx + edx + 12]
			bswap eax
			mov  dword ptr [edi + edx + 12],eax
			add edx, 16
			cmp edx, 64
			jb memcpyloop
		}
	}
	
	if (HaveDebugger && LogOptions.LogPRDMAMemLoads) {
		int count;
		char HexData[100], AsciiData[100], Addon[20];
		LogMessage("");
		LogMessage("\tData DMAed to the Pif Ram:");			
		LogMessage("\t--------------------------");
		for (count = 0; count < 16; count ++ ) {
			if ((count % 4) == 0) { 
				sprintf(HexData,"\0"); 
				sprintf(AsciiData,"\0"); 
			}
			sprintf(Addon,"%02X %02X %02X %02X", 
				PIF_Ram[(count << 2) + 0], PIF_Ram[(count << 2) + 1], 
				PIF_Ram[(count << 2) + 2], PIF_Ram[(count << 2) + 3] );
			strcat(HexData,Addon);
			if (((count + 1) % 4) != 0) {
				sprintf(Addon,"-");
				strcat(HexData,Addon);
			} 
			
			sprintf(Addon,"%c%c%c%c", 
				PIF_Ram[(count << 2) + 0], PIF_Ram[(count << 2) + 1], 
				PIF_Ram[(count << 2) + 2], PIF_Ram[(count << 2) + 3] );
			strcat(AsciiData,Addon);
			
			if (((count + 1) % 4) == 0) {
				LogMessage("\t%s %s",HexData, AsciiData);
			} 
		}
		LogMessage("");
	}

	PifRamWrite();
	
	if (DelaySI) {
		ChangeTimer(SiTimer,0x900);
	} else {
		MI_INTR_REG |= MI_INTR_SI;
		SI_STATUS_REG |= SI_STATUS_INTERRUPT;
		CheckInterrupts();
	}
}

void SP_DMA_READ(void) {
	SP_DRAM_ADDR_REGW &= 0x1FFFFFF8;
	SP_MEM_ADDR_REGW &= 0x001FFFF8;
	//(RW) : [11:0] length
	//[19:12] count
	//[31:20] skip
	int length = SP_RD_LEN_REG & 0xFFF;
	int count = (SP_RD_LEN_REG >> 12) & 0x0FF;
	int skip = (SP_RD_LEN_REG >> 20) & 0x0FF8;

	int IDMEM_SELECT = SP_MEM_ADDR_REGW & 0x01000;

	if ((length & 0x07) == 0)
		length++;

	if (SP_DRAM_ADDR_REGW > RdramSize) {
		if (ShowDebugMessages)
			DisplayError("SP DMA\nSP_DRAM_ADDR_REG not in RDRam space");
		SP_DMA_BUSY_REG = 0;
		SP_STATUS_REG &= ~SP_STATUS_DMA_BUSY;
		return;
	}

	if (SP_RD_LEN_REG + 1 + (SP_MEM_ADDR_REG & 0xFFF) > 0x1000) {
		//if (ShowDebugMessages)
		//	DisplayError("SP DMA\ncould not fit copy in memory segement");
		//SP_DMA_BUSY_REG = 0;
		//SP_STATUS_REG  &= ~SP_STATUS_DMA_BUSY;
		//return;		
	}

	if ((SP_MEM_ADDR_REG & 3) != 0 || (SP_DRAM_ADDR_REG & 3) != 0 || ((SP_RD_LEN_REG + 1) & 3) != 0) {
		//DisplayErrorFatal("Nonstandard DMA Transfer.\nStopping Emulation.");
		//ExitThread(0);
	}

	SP_MEM_ADDR_REG = SP_MEM_ADDR_REGW;
	SP_DRAM_ADDR_REG = SP_DRAM_ADDR_REGW;

	/*
	if ((SP_MEM_ADDR_REG & 3) != 0) { _asm int 3 }
	if ((SP_DRAM_ADDR_REG & 3) != 0) { _asm int 3 }
	if (((SP_RD_LEN_REG + 1) & 3) != 0) { _asm int 3 }
	*/
	if (length == 0)
		length = 1;
	length = ((length + 7) & 0x01FF8);
	for (int i = 0; i <= count; i++)
	{
		if (IDMEM_SELECT == 0x1000)
		{
			for (int ix = 0; ix < length; ix++)
			{
				IMEM[(SP_MEM_ADDR_REG + ix) & 0xFFF] = N64MEM[(SP_DRAM_ADDR_REG + ix)];
			}
		}
		else
		{
			for (int ix = 0; ix < length; ix++)
			{
				DMEM[(SP_MEM_ADDR_REG + ix) & 0xFFF] = N64MEM[(SP_DRAM_ADDR_REG + ix)];
			}
		}
		SP_MEM_ADDR_REG += length;
		SP_DRAM_ADDR_REG += length + skip;
	}

	SP_DRAM_ADDR_REG -= skip;
	SP_MEM_ADDR_REG &= 0x0fff;
	SP_MEM_ADDR_REG |= IDMEM_SELECT;
	SP_RD_LEN_REG = (SP_RD_LEN_REG & 0xFF000000) | 0xff8;
	SP_WR_LEN_REG = SP_RD_LEN_REG;
	SP_DMA_BUSY_REG = 0;
	SP_STATUS_REG &= ~SP_STATUS_DMA_BUSY;
}

void SP_DMA_WRITE(void) {
	SP_DRAM_ADDR_REGW &= 0x1FFFFFF8;
	SP_MEM_ADDR_REGW &= 0x001FFFF8;
	//(RW) : [11:0] length
	//[19:12] count
	//[31:20] skip
	int length = SP_WR_LEN_REG & 0xFFF;
	int count = (SP_WR_LEN_REG >> 12) & 0x0FF;
	int skip = (SP_WR_LEN_REG >> 20) & 0x0FF8;

	int IDMEM_SELECT = SP_MEM_ADDR_REGW & 0x01000;

	if (SP_DRAM_ADDR_REGW > RdramSize) {
		if (ShowDebugMessages)
			DisplayError("SP DMA WRITE\nSP_DRAM_ADDR_REG not in RDRam space");
		return;
	}

	if (SP_WR_LEN_REG + 1 + (SP_MEM_ADDR_REG & 0xFFF) > 0x1000) {
		//if (ShowDebugMessages)
		//	DisplayError("SP DMA WRITE\ncould not fit copy in memory segement");
		//return;		
	}

	SP_MEM_ADDR_REG = SP_MEM_ADDR_REGW;
	SP_DRAM_ADDR_REG = SP_DRAM_ADDR_REGW;

	//if ((SP_MEM_ADDR_REG & 3) != 0) { _asm int 3 }
	//if ((SP_DRAM_ADDR_REG & 3) != 0) { _asm int 3 }
	//if (((SP_WR_LEN_REG + 1) & 3) != 0) { _asm int 3 }

	if (length == 0)
		length = 1;
	length = ((length + 7) & 0x01FF8);
	for (int i = 0; i <= count; i++)
	{
		int remainingLength = length;
		while (remainingLength > 0) {
			unsigned int blockLength = remainingLength;
			if ((0x1000 - (SP_MEM_ADDR_REG & 0xFFF)) < blockLength) {
				blockLength = 0x1000 - (SP_MEM_ADDR_REG & 0xFFF);
			}
			memcpy(N64MEM + SP_DRAM_ADDR_REG, DMEM + (SP_MEM_ADDR_REG & 0x1FFF), blockLength);
			if (((SP_MEM_ADDR_REG + blockLength) & 0xFFF) != 0) {
				SP_MEM_ADDR_REG += blockLength;
			}
			else { // loop
				SP_MEM_ADDR_REG &= 0x1000;
			}
			SP_DRAM_ADDR_REG += blockLength;
			remainingLength -= blockLength;
		}
		SP_DRAM_ADDR_REG += skip;
	}

	//SP_DRAM_ADDR_REG += 8;
	SP_DRAM_ADDR_REG -= skip;
	SP_MEM_ADDR_REG &= 0x0fff;
	SP_MEM_ADDR_REG |= IDMEM_SELECT;
	SP_WR_LEN_REG = (SP_WR_LEN_REG & 0xFF000000) | 0xff8;
	SP_RD_LEN_REG = SP_WR_LEN_REG;
	SP_DMA_BUSY_REG = 0;
	SP_STATUS_REG &= ~SP_STATUS_DMA_BUSY;
}
