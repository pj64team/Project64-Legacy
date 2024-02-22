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
#include "plugin.h"
#include "Debugger.h"
#include "n64_cic_nus_6105.h"
#include "RomTools_Common.h"
#include "Real-Time Clock.h"

void ProcessControllerCommand(int Control, BYTE* Command);
void ReadControllerCommand(int Control, BYTE* Command);

BYTE PifRom[0x7C0], * PIF_Ram;

enum CIC_CHIP GetCicChipID(BYTE* RomData) {
	_int64 CRC = 0;
	int count;

	for (count = 0x40; count < 0x1000; count += 4) {
		CRC += *(DWORD*)(RomData + count);
	}

	switch (CRC) {
		case 0x000000D0027FDF31:
		case 0x000000CFFB631223:
		case 0x000000C34B2826B8:	// iQue 
		case 0x0000002F35CF0DE9:	// iQue (Paper Mario)
		case 0x000000C92ADFE50A:	// iQue (Sin and Punishment)
			return CIC_NUS_6101;

		case 0x000000D057C85244:
			return CIC_NUS_6102;

		case 0x000000D6497E414B:
			return CIC_NUS_6103;

		case 0x0000011A49F60E96:
			return CIC_NUS_6105;

		case 0x000000D6D5BE5580:
			return CIC_NUS_6106;

		case 0x000001053BC19870:
			return CIC_NUS_5167;

		case 0x000000D2E53EF008:	// Added to support N64DD IPLROM (J)
		case 0x000000D2E53EF39F:
			return CIC_NUS_8303;

		case 0x000000D2E53E5DDA:	// Includes the 64DD hacked roms
			return CIC_NUS_DDUS;
		case 0x000000ec932a00ed:
			return CIC_NUS_XENO;
	}
	
	// Aleck64, they all seem to start with A7 or A8 past 32 bits
	if (CRC >> 32 == 0xA7 || CRC >> 32 == 0xA8)
		return CIC_NUS_8401;
	
	return CIC_UNKNOWN;
}

void LogControllerPakData(char* Description) {
	if (HaveDebugger) {
		int count, count2;
		char HexData[100], AsciiData[100], Addon[20];

		LogMessage("\t%s:", Description);
		LogMessage("\t------------------------------");

		for (count = 0; count < 16; count++) {
			if ((count % 4) == 0) {
				sprintf(HexData, "\0");
				sprintf(AsciiData, "\0");
			}

			sprintf(Addon, "%02X %02X %02X %02X",
				PIF_Ram[(count << 2) + 0], PIF_Ram[(count << 2) + 1],
				PIF_Ram[(count << 2) + 2], PIF_Ram[(count << 2) + 3]);

			strcat(HexData, Addon);

			if (((count + 1) % 4) != 0) {
				sprintf(Addon, "-");
				strcat(HexData, Addon);
			}

			Addon[0] = 0;
			for (count2 = 0; count2 < 4; count2++) {
				if (PIF_Ram[(count << 2) + count2] < 30) {
					strcat(Addon, ".");
				}
				else {
					sprintf(Addon, "%s%c", Addon, PIF_Ram[(count << 2) + count2]);
				}
			}

			strcat(AsciiData, Addon);

			if (((count + 1) % 4) == 0) {
				LogMessage("\t%s %s", HexData, AsciiData);
			}
		}

		LogMessage("");
	}
}

void PifRamRead(void) {

	// CIC NUS 6105 challenge/response system
	if (PIF_Ram[0x3F] == 0x2) {
		char hold[32], resp[32] = { 0 };
		int i, j;

		for (i = 0, j = 48; i < 32; i += 2, j++)
		{
			hold[i + 1] = PIF_Ram[j] % 16;
			hold[i] = PIF_Ram[j] / 16;
		}

		n64_cic_nus_6105(hold, resp, 32 - 2);

		for (i = 48, j = 0; i <= 63; i++, j += 2)
			PIF_Ram[i] = resp[j] * 16 + resp[j + 1];

		return;
	}

	for (size_t CurPos = 0, Channel = 0; CurPos < 0x40; ++CurPos) {

		switch (PIF_Ram[CurPos]) {
			case 0x00:
				// Advance to the next Channel, signal stop if past Channel 6
				Channel += 1;
				if (Channel > 6)
					CurPos = 0x40;
				break;

			case 0xFD:
			case 0xFE:
				// End of request
				CurPos = 0x40;
				break;

			case 0xFF:
				break;

			case 0xB4:
			case 0x56:
			case 0xB8:
				// What is this for?
				break;

			default:
				if ((PIF_Ram[CurPos] & 0xC0) == 0) {
					if (Channel < 4) {

						if (Controllers[Channel].Present && Controllers[Channel].RawData) {
							if (ReadController) {
								ReadController(Channel, &PIF_Ram[CurPos]);
							}
						}
						else {
							ReadControllerCommand(Channel, &PIF_Ram[CurPos]);
						}
					}
					CurPos += PIF_Ram[CurPos] + (PIF_Ram[CurPos + 1] & 0x3F) + 1;
					Channel += 1;
				}
				else {
					if (ShowPifRamErrors) {
						DisplayError("Unknown Command in PifRamRead(%X)", PIF_Ram[CurPos]);
					}
					CurPos = 0x40;
				}

				break;
		}
	}

	if (ReadController) {
		ReadController(-1, NULL);
	}
}

void PifRamWrite(void) {

	if (PIF_Ram[0x3F] > 0x1) {

		switch (PIF_Ram[0x3F]) {
			case 0x08:
				PIF_Ram[0x3F] = 0;
				MI_INTR_REG |= MI_INTR_SI;
				SI_STATUS_REG |= SI_STATUS_INTERRUPT;
				CheckInterrupts();
				break;

			case 0x10:
				memset(PifRom, 0, 0x7C0);
				break;

			case 0x30:
				PIF_Ram[0x3F] = 0x80;
				break;

			case 0xC0:
				memset(PIF_Ram, 0, 0x40);
				break;

			case 0x02:
				// CIC NUS 6105 Encryption related, already handled in PifRamRead
				break;	

			default:
				if (ShowPifRamErrors) {
					DisplayError("Unknown PifRam control: %d", PIF_Ram[0x3F]);
				}
		}
		return;
	}

	for (size_t CurPos = 0, Channel = 0; CurPos < 0x40; CurPos++) {

		switch (PIF_Ram[CurPos]) {
			case 0x00:
				Channel += 1;
				if (Channel > 6) {
					CurPos = 0x40;
				}
				break;

			case 0xFD:
			case 0xFE:
				CurPos = 0x40;
				break;

			case 0xFF:
				break;
				
			case 0xB4:
			case 0x56:
			case 0xB8:
				// What is this for?
				break;

			default:
				if ((PIF_Ram[CurPos] & 0xC0) == 0) {
					if (Channel < 4) {
						if (Controllers[Channel].Present && Controllers[Channel].RawData) {
							if (ControllerCommand) {
								ControllerCommand(Channel, &PIF_Ram[CurPos]);
							}
						}
						else {
							ProcessControllerCommand(Channel, &PIF_Ram[CurPos]);
						}
					}
					else if (Channel == 4) {
						if (RTC_Command(&PIF_Ram[CurPos]) == FALSE)
							EepromCommand(&PIF_Ram[CurPos]);
					}
					else {
						if (ShowPifRamErrors)
							DisplayError("Command on channel 5?");
					}

					CurPos += PIF_Ram[CurPos] + (PIF_Ram[CurPos + 1] & 0x3F) + 1;
					Channel += 1;
				}
				else {
					if (ShowPifRamErrors) {
						DisplayError("Unknown Command in PifRamWrite(%X)", PIF_Ram[CurPos]);
					}
					CurPos = 0x40;
				}
				break;
		}
	}

	PIF_Ram[0x3F] = 0;
	if (ControllerCommand) {
		ControllerCommand(-1, NULL);
	}
}

void ProcessControllerCommand(int Control, BYTE* Command) {
	switch (Command[2]) {
		case 0x00: // check
		case 0xFF: // reset & check ?
			if ((Command[1] & 0x80) != 0) {
				break;
			}

			if (ShowPifRamErrors && (Command[0] != 1 || Command[1] != 3)) {
				DisplayError("What am I meant to do with this Controller Command");
			}

			if (Controllers[Control].Present == TRUE) {
				// This has been changed but keeps backwards compatibility
				// 0x1, or TRUE is N64 Controller
				Command[3] = 0x05;
				Command[4] = 0x00;

				switch (Controllers[Control].Plugin) {
					case PLUGIN_RUMBLE_PAK:
					case PLUGIN_MEMPAK:
					case PLUGIN_RAW:
						Command[5] = 1;
						break;

					default:
						Command[5] = 0;
						break;
				}
			}			
			else if (Controllers[Control].Present == 0x2) {
				// 0x2 is N64 Mouse
				Command[3] = 0x02;
				Command[4] = 0x00;
				Command[5] = 0x00;
			}
			else {
				// 0x1, FALSE, or any other value
				Command[1] |= 0x80;
			}
			break;

		case 0x01: // read controller
			if (ShowPifRamErrors && (Command[0] != 1 || Command[1] != 4)) {
				DisplayError("What am I meant to do with this Controller Command");
			}

			if (Controllers[Control].Present == FALSE) {
				Command[1] |= 0x80;
			}
			break;

		case 0x02: //read from controller pack
			if (HaveDebugger && LogOptions.LogControllerPak) {
				LogControllerPakData("Read: Before Getting Results");
			}

			if (ShowPifRamErrors && (Command[0] != 3 || Command[1] != 33)) {
				DisplayError("What am I meant to do with this Controller Command");
			}

			if (Controllers[Control].Present == TRUE) {
				DWORD address = (Command[3] << 8) | (Command[4] & 0xE0);

				switch (Controllers[Control].Plugin) {
					case PLUGIN_RUMBLE_PAK:
						memset(&Command[5], (address >= 0x8000 && address < 0x9000) ? 0x80 : 0x00, 0x20);
						Command[0x25] = Mempacks_CalulateCrc(&Command[5]);
						break;

					case PLUGIN_MEMPAK:
						ReadFromMempak(Control, address, &Command[5]);
						break;

					case PLUGIN_RAW:
						if (ControllerCommand) {
							ControllerCommand(Control, Command);
						}
						break;

					default:
						memset(&Command[5], 0, 0x20);
						Command[0x25] = 0;
				}
			}
			else {
				Command[1] |= 0x80;
			}

			if (HaveDebugger && LogOptions.LogControllerPak) {
				LogControllerPakData("Read: After Getting Results");
			}
			break;

		case 0x03: //write controller pak
			if (HaveDebugger && LogOptions.LogControllerPak) {
				LogControllerPakData("Write: Before Processing");
			}

			if (ShowPifRamErrors && (Command[0] != 35 || Command[1] != 1)) {
				DisplayError("What am I meant to do with this Controller Command");
			}

			if (Controllers[Control].Present == TRUE) {
				DWORD address = (Command[3] << 8) | (Command[4] & 0xE0);

				switch (Controllers[Control].Plugin) {
					case PLUGIN_MEMPAK:
						WriteToMempak(Control, address, &Command[5]);
						break;

					case PLUGIN_RAW:
						if (ControllerCommand) {
							ControllerCommand(Control, Command);
						}
						break;

					case PLUGIN_RUMBLE_PAK:
						if (RumbleCommand != NULL) {
							if ((address & 0xFFE0) == 0xC000)
								RumbleCommand(Control, *(BOOL*)(&Command[5]));
							break;
						}						
				}

				if (Controllers[Control].Plugin != PLUGIN_RAW) 
					Command[0x25] = Mempacks_CalulateCrc(&Command[5]);
			}
			else {
				Command[1] |= 0x80;
			}

			if (HaveDebugger && LogOptions.LogControllerPak) {
				LogControllerPakData("Write: After Processing");
			}
			break;
		default:
			if (ShowPifRamErrors) {
				DisplayError("Unknown ControllerCommand %d", Command[2]);
			}
	}
}

void ReadControllerCommand(int Control, BYTE* Command) {
	switch (Command[2]) {
		case 0x01: // read controller
			if (Controllers[Control].Present == TRUE) {
				if (ShowPifRamErrors && (Command[0] != 1 || Command[1] != 4)) {
					DisplayError("What am I meant to do with this Controller Command");
				}

				if (GetKeys) {
					BUTTONS Keys;

					GetKeys(Control, &Keys);
					*(DWORD*)&Command[3] = Keys.Value;
				}
				else {
					*(DWORD*)&Command[3] = 0;
				}
			}
			break;

		case 0x02: //read from controller pack
			if (Controllers[Control].Present == TRUE) {
				switch (Controllers[Control].Plugin) {
					case PLUGIN_RAW:
						if (ControllerCommand) {
							ReadController(Control, Command);
						}
						break;
				}
			}
			break;

		case 0x03: //write controller pak
			if (Controllers[Control].Present == TRUE) {
				switch (Controllers[Control].Plugin) {
					case PLUGIN_RAW:
						if (ControllerCommand) {
							ReadController(Control, Command);
						}
						break;
				}
			}
			break;
	}
}

int LoadPifRom(BYTE country) {
	char path_buffer[_MAX_PATH], drive[_MAX_DRIVE], dir[_MAX_DIR], PifRomName[255];
	HANDLE hPifFile;
	DWORD dwRead;
	BOOL read_status;

	GetModuleFileName(NULL, path_buffer, sizeof(path_buffer));
	_splitpath(path_buffer, drive, dir, NULL, NULL);

	switch (GetRomRegionByCode(country)) {
		case PAL_Region:
			sprintf(PifRomName, "%s%sPifrom\\pifpal.raw", drive, dir);
			break;
		case NTSC_Region:
			sprintf(PifRomName, "%s%sPifrom\\pifntsc.raw", drive, dir);
			break;
		default:
			// Failed to detect the rom as PAL or NTSC
			if (ShowPifRamErrors)
				DisplayError(GS(MSG_UNKNOWN_COUNTRY));
			memset(PifRom, 0, 0x7C0);
			return FALSE;
	}

	hPifFile = CreateFile(PifRomName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS, NULL);

	// Failed to open the file
	if (hPifFile == INVALID_HANDLE_VALUE) {
		memset(PifRom, 0, 0x7C0);
		return FALSE;
	}

	SetFilePointer(hPifFile, 0, NULL, FILE_BEGIN);
	read_status = ReadFile(hPifFile, PifRom, 0x7C0, &dwRead, NULL);
	CloseHandle(hPifFile);

	// Failed to read the file or did not read enough bytes
	if (read_status == FALSE || dwRead != 0x7C0) {
		memset(PifRom, 0, 0x7C0);
		return FALSE;
	}

	return TRUE;
}
