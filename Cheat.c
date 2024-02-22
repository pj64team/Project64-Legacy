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
#include <windowsx.h>
#include <commctrl.h>
#include <stdio.h>
#include <winuser.h>
#include <string.h>
#include "main.h"
#include "cheats.h"
#include "cpu.h"
#include "resource.h"
#include "Cheats_Preprocessor.h"
#include "RomTools_Common.h"

#define UM_CLOSE_CHEATS 		(WM_USER + 132)
#define UM_CHANGEEXTENSION		(WM_USER + 101)
#define IDC_MYTREE				0x500

#define SelectCheat				1
#define EditCheat				2
#define NewCheat 				3 

HWND hManageWindow = NULL;
HWND hSelectCheat, hAddCheat, hCheatTree;
CHEAT_CODES Codes[MaxCheats];
int NoOfCodes;


// *******************************************************************************************
// Functions for Add Cheat and Edit Cheat
// *******************************************************************************************
void ReadDialogInput(CHEAT* cheat, HWND hDlg);

void NormalizeInput(HWND hDlg, DWORD hHandle);
BOOL NeededCRLFChange(char** str);
// *******************************************************************************************


void RefreshCheatManager(void);
void SaveCheat(char* CheatName, BOOL Active);
int  _TreeView_GetCheckState(HWND hwndTreeView, HTREEITEM hItem);
BOOL _TreeView_SetCheckState(HWND hwndTreeView, HTREEITEM hItem, int State);
DWORD ConvertXP64Address(DWORD Address); //Witten
WORD ConvertXP64Value(WORD Value); //Witten

LRESULT CALLBACK ManageCheatsProc(HWND, UINT, WPARAM, LPARAM);

enum Dialog_State {
	CONTRACTED,
	EXPANDED
} DialogState;

enum Cheat_Type {
	SIMPLE,
	OPTIONS,
	RANGE
} CheatType;

enum TV_CHECK_STATE {
	TV_STATE_CLEAR,
	TV_STATE_CHECKED,
	TV_STATE_INDETERMINATE,
} DialogState;

int MinSizeDlg;
int MaxSizeDlg;

/********************************************************************************************
  ConvertXP64Address

  Purpose: Decode encoded XP64 address to physical address
  Parameters:
  Returns:
  Author: Witten

********************************************************************************************/
DWORD ConvertXP64Address(DWORD Address) {
	DWORD tmpAddress;

	tmpAddress = (Address ^ 0x68000000) & 0xFF000000;
	tmpAddress += ((Address + 0x002B0000) ^ 0x00810000) & 0x00FF0000;
	tmpAddress += ((Address + 0x00002B00) ^ 0x00008200) & 0x0000FF00;
	tmpAddress += ((Address + 0x0000002B) ^ 0x00000083) & 0x000000FF;
	return tmpAddress;
}

/********************************************************************************************
  ConvertXP64Value

  Purpose: Decode encoded XP64 value
  Parameters:
  Returns:
  Author: Witten

********************************************************************************************/
WORD ConvertXP64Value(WORD Value) {
	WORD  tmpValue;

	tmpValue = ((Value + 0x2B00) ^ 0x8400) & 0xFF00;
	tmpValue += ((Value + 0x002B) ^ 0x0085) & 0x00FF;
	return tmpValue;
}

void ApplyGSButton(void) {
	int count, count2, count3;
	MIPS_DWORD Address;
	WORD  Memory;
	GAMESHARK_CODE Code;

	for (count = 0; count < NoOfCodes; count++) {
		for (count2 = 0; count2 < MaxGSEntries; count2++) {
			Code.Command = Codes[count].Code[count2].Command;
			Code.Value = Codes[count].Code[count2].Value;

			switch (Code.Command & 0xFF000000) {
			case 0x00000000:
				count2 = MaxGSEntries;
				break;
			case 0x40000000: {
				int next = count2 + 1;
				if (next < MaxGSEntries) {
					int byte_count = Codes[count].Code[next].Value + 1;
					count2 += (byte_count / 6) + (byte_count % 6 ? 1 : 0) + 1;
				}
				break;
			}
			case 0x50000000: {
				int numrepeats = (Code.Command & 0x0000FF00) >> 8;
				int offset = Code.Command & 0x000000FF;
				WORD incr = Code.Value;

				if (++count2 >= MaxGSEntries) {
					break;
				}
				Code.Command = Codes[count].Code[count2].Command;
				Code.Value = Codes[count].Code[count2].Value;

				switch (Code.Command & 0xFF000000) {
					// Gameshark / AR
				case 0x88000000:
					Address.DW = (int)(0x80000000 | (Code.Command & 0xFFFFFF));
					Memory = Code.Value;
					for (count3 = 0; count3 < numrepeats; count3++) {
						r4300i_SB_VAddr_NonCPU(Address, (BYTE)Memory);
						Address.UDW += offset;
						Memory += incr;
					}
					break;
				case 0x89000000:
					Address.DW = (int)(0x80000000 | (Code.Command & 0xFFFFFF));
					Memory = Code.Value;
					for (count3 = 0; count3 < numrepeats; count3++) {
						r4300i_SH_VAddr_NonCPU(Address, (WORD)Memory);
						Address.UDW += offset;
						Memory += incr;
					}
					break;

					// Xplorer64
				case 0xA8000000:
					Address.DW = (int)(0x80000000 | (ConvertXP64Address(Code.Command) & 0xFFFFFF));
					Memory = ConvertXP64Value(Code.Value);
					for (count3 = 0; count3 < numrepeats; count3++) {
						r4300i_SB_VAddr_NonCPU(Address, (BYTE)Memory);
						Address.UDW += offset;
						Memory += incr;
					}
					break;
				case 0xA9000000:
					Address.DW = (int)(0x80000000 | (ConvertXP64Address(Code.Command) & 0xFFFFFF));
					Memory = ConvertXP64Value(Code.Value);
					for (count3 = 0; count3 < numrepeats; count3++) {
						r4300i_SH_VAddr_NonCPU(Address, (WORD)Memory);
						Address.UDW += offset;
						Memory += incr;
					}
					break;
				}
				break;
			}
			default:
				switch (Code.Command & 0xFF000000) {
					// Gameshark / AR
				case 0x88000000:
					Address.DW = (int)(0x80000000 | (Code.Command & 0xFFFFFF));
					r4300i_SB_VAddr_NonCPU(Address, (BYTE)Code.Value);
					break;
				case 0x89000000:
					Address.DW = (int)(0x80000000 | (Code.Command & 0xFFFFFF));
					r4300i_SH_VAddr_NonCPU(Address, Code.Value);
					break;
					// Xplorer64
				case 0xA8000000:
					Address.DW = (int)(0x80000000 | (ConvertXP64Address(Code.Command) & 0xFFFFFF));
					r4300i_SB_VAddr_NonCPU(Address, (BYTE)ConvertXP64Value(Code.Value));
					break;
				case 0xA9000000:
					Address.DW = (int)(0x80000000 | (ConvertXP64Address(Code.Command) & 0xFFFFFF));
					r4300i_SH_VAddr_NonCPU(Address, ConvertXP64Value(Code.Value));
					break;
				default:
					break;
				}
			}
		}
	}
}

/********************************************************************************************
  ApplyCheats

  Purpose: Patch codes into memory
  Parameters: None
  Returns: None

********************************************************************************************/
int ApplyCheatEntry(GAMESHARK_CODE* Code, BOOL Execute) {
	MIPS_DWORD Address;
	WORD  Memory;

	switch (Code->Command & 0xFF000000) {
		// Gameshark / AR
	case 0x40000000:
	{
		// Write bytes with virtual addresses (supports TLB mapping)
		Address.DW = (int)Code[1].Command;
		int count = Code[1].Value + 1;
		int i;

		if (Execute) {
			for (i = 0; i < count; i += 2) {
				switch (i % 6) {
				case 0:
					Memory = Code[(i / 6) + 2].Command >> 16;
					break;
				case 2:
					Memory = Code[(i / 6) + 2].Command;
					break;
				default:
					Memory = Code[(i / 6) + 2].Value;
					break;
				}
				r4300i_SH_VAddr_NonCPU(Address, Memory);
				Address.UDW += 2;
			}
			if (count % 2) {
				switch (i % 6) {
				case 0:
					Memory = Code[(i / 6) + 2].Command >> 24;
					break;
				case 2:
					Memory = Code[(i / 6) + 2].Command >> 8;
					break;
				default:
					Memory = Code[(i / 6) + 2].Value >> 8;
					break;
				}
				r4300i_SB_VAddr_NonCPU(Address, (BYTE)Memory);
			}
		}

		return (count / 6) + (count % 6 ? 1 : 0) + 2;
	}
	case 0x50000000:													// Added by Witten (witten@pj64cheats.net)
	{
		int numrepeats = (Code->Command & 0x0000FF00) >> 8;
		int offset = Code->Command & 0x000000FF;
		WORD incr = Code->Value;
		int count;

		switch (Code[1].Command & 0xFF000000) {
		case 0x80000000:
			if (Execute) {
				Address.DW = (int)(0x80000000 | (Code[1].Command & 0xFFFFFF));
				Memory = Code[1].Value;
				for (count = 0; count < numrepeats; count++) {
					r4300i_SB_VAddr_NonCPU(Address, (BYTE)Memory);
					Address.UDW += offset;
					Memory += incr;
				}
			}
			return 2;
		case 0x81000000:
			if (Execute) {
				Address.DW = (int)(0x80000000 | (Code[1].Command & 0xFFFFFF));
				Memory = Code[1].Value;
				for (count = 0; count < numrepeats; count++) {
					r4300i_SH_VAddr_NonCPU(Address, (WORD)Memory);
					Address.UDW += offset;
					Memory += incr;
				}
			}
			return 2;
		default: return 1;
		}
	}
	break;
	case 0x80000000:
		Address.DW = (int)(0x80000000 | (Code->Command & 0xFFFFFF));
		if (Execute) { r4300i_SB_VAddr_NonCPU(Address, (BYTE)Code->Value); }
		break;
	case 0x81000000:
		Address.DW = (int)(0x80000000 | (Code->Command & 0xFFFFFF));
		if (Execute) { r4300i_SH_VAddr_NonCPU(Address, Code->Value); }
		break;
	case 0xA0000000:
		Address.DW = (int)(0xA0000000 | (Code->Command & 0xFFFFFF));
		if (Execute) { r4300i_SB_VAddr_NonCPU(Address, (BYTE)Code->Value); }
		break;
	case 0xA1000000:
		Address.DW = (int)(0xA0000000 | (Code->Command & 0xFFFFFF));
		if (Execute) { r4300i_SH_VAddr_NonCPU(Address, Code->Value); }
		break;
	case 0xD0000000:													// Added by Witten (witten@pj64cheats.net)
		Address.DW = (int)(0x80000000 | (Code->Command & 0xFFFFFF));
		r4300i_LB_VAddr_NonCPU(Address, (BYTE*)&Memory);
		Memory &= 0x00FF;
		if (Memory != Code->Value) { Execute = FALSE; }
		return ApplyCheatEntry(&Code[1], Execute) + 1;
	case 0xD1000000:													// Added by Witten (witten@pj64cheats.net)
		Address.DW = (int)(0x80000000 | (Code->Command & 0xFFFFFF));
		r4300i_LH_VAddr_NonCPU(Address, (WORD*)&Memory);
		if (Memory != Code->Value) { Execute = FALSE; }
		return ApplyCheatEntry(&Code[1], Execute) + 1;
	case 0xD2000000:													// Added by Witten (witten@pj64cheats.net)
		Address.DW = (int)(0x80000000 | (Code->Command & 0xFFFFFF));
		r4300i_LB_VAddr_NonCPU(Address, (BYTE*)&Memory);
		Memory &= 0x00FF;
		if (Memory == Code->Value) { Execute = FALSE; }
		return ApplyCheatEntry(&Code[1], Execute) + 1;
	case 0xD3000000:													// Added by Witten (witten@pj64cheats.net)
		Address.DW = (int)(0x80000000 | (Code->Command & 0xFFFFFF));
		r4300i_LH_VAddr_NonCPU(Address, (WORD*)&Memory);
		if (Memory == Code->Value) { Execute = FALSE; }
		return ApplyCheatEntry(&Code[1], Execute) + 1;

		// Xplorer64 (Author: Witten)
	case 0x30000000:
	case 0x82000000:
	case 0x84000000:
		Address.DW = (int)(0x80000000 | (Code->Command & 0xFFFFFF));
		if (Execute) { r4300i_SB_VAddr_NonCPU(Address, (BYTE)Code->Value); }
		break;
	case 0x31000000:
	case 0x83000000:
	case 0x85000000:
		Address.DW = (int)(0x80000000 | (Code->Command & 0xFFFFFF));
		if (Execute) { r4300i_SH_VAddr_NonCPU(Address, Code->Value); }
		break;
	case 0xE8000000:
		Address.DW = (int)(0x80000000 | (ConvertXP64Address(Code->Command) & 0xFFFFFF));
		if (Execute) { r4300i_SB_VAddr_NonCPU(Address, (BYTE)ConvertXP64Value(Code->Value)); }
		break;
	case 0xE9000000:
		Address.DW = (int)(0x80000000 | (ConvertXP64Address(Code->Command) & 0xFFFFFF));
		if (Execute) { r4300i_SH_VAddr_NonCPU(Address, ConvertXP64Value(Code->Value)); }
		break;
	case 0xC8000000:
		Address.DW = (int)(0xA0000000 | (ConvertXP64Address(Code->Command) & 0xFFFFFF));
		if (Execute) { r4300i_SB_VAddr_NonCPU(Address, (BYTE)Code->Value); }
		break;
	case 0xC9000000:
		Address.DW = (int)(0xA0000000 | (ConvertXP64Address(Code->Command) & 0xFFFFFF));
		if (Execute) { r4300i_SH_VAddr_NonCPU(Address, ConvertXP64Value(Code->Value)); }
		break;
	case 0xB8000000:
		Address.DW = (int)(0x80000000 | (ConvertXP64Address(Code->Command) & 0xFFFFFF));
		r4300i_LB_VAddr_NonCPU(Address, (BYTE*)&Memory);
		Memory &= 0x00FF;
		if (Memory != ConvertXP64Value(Code->Value)) { Execute = FALSE; }
		return ApplyCheatEntry(&Code[1], Execute) + 1;
	case 0xB9000000:
		Address.DW = (int)(0x80000000 | (ConvertXP64Address(Code->Command) & 0xFFFFFF));
		r4300i_LH_VAddr_NonCPU(Address, (WORD*)&Memory);
		if (Memory != ConvertXP64Value(Code->Value)) { Execute = FALSE; }
		return ApplyCheatEntry(&Code[1], Execute) + 1;
	case 0xBA000000:
		Address.DW = (int)(0x80000000 | (ConvertXP64Address(Code->Command) & 0xFFFFFF));
		r4300i_LB_VAddr_NonCPU(Address, (BYTE*)&Memory);
		Memory &= 0x00FF;
		if (Memory == ConvertXP64Value(Code->Value)) { Execute = FALSE; }
		return ApplyCheatEntry(&Code[1], Execute) + 1;
	case 0xBB000000:
		Address.DW = (int)(0x80000000 | (ConvertXP64Address(Code->Command) & 0xFFFFFF));
		r4300i_LH_VAddr_NonCPU(Address, (WORD*)&Memory);
		if (Memory == ConvertXP64Value(Code->Value)) { Execute = FALSE; }
		return ApplyCheatEntry(&Code[1], Execute) + 1;
	case 0: 
		return MaxGSEntries; 
	}
	return 1;
}

void ApplyCheats(void) {
	int CurrentCheat, CurrentEntry;

	for (CurrentCheat = 0; CurrentCheat < NoOfCodes; CurrentCheat++) {
		for (CurrentEntry = 0; CurrentEntry < MaxGSEntries;) {
			CurrentEntry += ApplyCheatEntry(&Codes[CurrentCheat].Code[CurrentEntry], TRUE);
		}
	}
}

void ChangeRomCheats(HWND hwndOwner) {
	char OrigRomName[sizeof(RomName)], OrigFileName[sizeof(CurrentFileName)], OrigFullName[sizeof(RomFullName)];
	BYTE OrigByteHeader[sizeof(RomHeader)];
	DWORD OrigFileSize;

	// Always remember cheats if the emulator is running
	if (CPURunning) {
		ManageCheats(hwndOwner);
		return;
	}

	//Load information about target rom and back up current information
	memcpy(OrigRomName, RomName, sizeof(OrigRomName));
	memcpy(OrigFileName, CurrentFileName, sizeof(OrigFileName));
	memcpy(OrigFullName, RomFullName, sizeof(OrigFullName));
	memcpy(CurrentFileName, CurrentRBFileName, sizeof(CurrentFileName));
	memcpy(OrigByteHeader, RomHeader, sizeof(RomHeader));
	OrigFileSize = RomFileSize;

	LoadRomHeader();
	if (!RememberCheats)
		DisableAllCheats();

	ManageCheats(hwndOwner);

	//Restore details
	memcpy(RomName, OrigRomName, sizeof(RomName));
	memcpy(CurrentFileName, OrigFileName, sizeof(CurrentFileName));
	memcpy(RomFullName, OrigFullName, sizeof(RomFullName));
	memcpy(CurrentRBFileName, CurrentFileName, sizeof(CurrentRBFileName));
	memcpy(RomHeader, OrigByteHeader, sizeof(RomHeader));
	RomFileSize = OrigFileSize;
}

/********************************************************************************************
  CheatActive

  Purpose: Checks in Application Settings file if cheat is active
  Parameters: char*
	Name: name of cheat
  Returns: Boolean
	True: cheat is active
	False: cheat isn't active or cheat isn't found in the file

********************************************************************************************/
BOOL CheatActive(char* Name) {
	char Identifier[100];

	RomID(Identifier, RomHeader);
	return Settings_ReadBool(APPS_NAME, Identifier, Name, FALSE);
}

/********************************************************************************************
  CheatCodeExProc

  Purpose: Message handler for ... who knows! Git blame me, I dare you! ROFL
  Parameters:
  Returns:

********************************************************************************************/
LRESULT CALLBACK CheatsCodeExProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch (uMsg) {
	case WM_INITDIALOG:
	{
		char option_name[300], * ReadPos;
		DWORD len;
		BOOL matched;
		CHEAT cheat = { 0 };

		cheat = *(CHEAT*)lParam;

		SetWindowText(hDlg, GS(CHEAT_CODE_EXT_TITLE));
		SetDlgItemText(hDlg, IDC_NOTE, GS(CHEAT_CODE_EXT_TXT));
		SetDlgItemText(hDlg, IDOK, GS(CHEAT_OK));
		SetDlgItemText(hDlg, IDCANCEL, GS(CHEAT_CANCEL));

		if (cheat.name != NULL)
			SetDlgItemText(hDlg, IDC_CHEAT_NAME, cheat.name);
		else
			SetDlgItemText(hDlg, IDC_CHEAT_NAME, STR_EMPTY);

		// No options
		if (cheat.type == NO_REPLACE || cheat.type == BAD_CODE || strcmp(cheat.options, STR_EMPTY) == 0) {
			EndDialog(hDlg, 0);
			return TRUE;
		}

		// Parse String for options to display, these will be displayed line by line
		ReadPos = cheat.options;
		matched = FALSE;
		while (ReadPos != NULL) {
			int index;
			char* name_only;

			// Nothing left to parse
			if (strlen(ReadPos) == 0)
				break;
			
			// The amount to copy, either the entire string (No , detected) or up to the next ,
			if (strchr(ReadPos, ',') == NULL)
				len = strlen(ReadPos);
			else
				len = strchr(ReadPos, ',') - ReadPos;

			// Set a cap of CheatName's length (remove a 1 for the null character)
			// This may be changed later
			if (len >= sizeof(option_name))
				len = sizeof(option_name) - 1;

			// Copy the option name into CheatName
			strncpy(option_name, ReadPos, len);
			option_name[len] = 0;

			name_only = strchr(option_name, ' ');
			index = SendMessage(GetDlgItem(hDlg, IDC_CHEAT_LIST), LB_ADDSTRING, 0, (LPARAM)(name_only + 1));
			SendMessage(GetDlgItem(hDlg, IDC_CHEAT_FULL), LB_ADDSTRING, 0, (LPARAM)option_name);
			if (strcmp(cheat.selected, option_name) == 0) {
				matched = TRUE;
				SendMessage(GetDlgItem(hDlg, IDC_CHEAT_LIST), LB_SETCURSEL, index, 0);
			}

			// Find the start of the next code
			if (strchr(ReadPos, ',') == NULL)
				ReadPos += strlen(ReadPos);
			else
				ReadPos = strchr(ReadPos, ',') + 1;
		}

		// There is a selected option that is not in the list, remove it
		if (matched == FALSE && strcmp(cheat.selected, STR_EMPTY) != 0) {
			char Identifier[100], * tmp;
			unsigned int length;
			
			length = strlen(cheat.selected) + strlen(CHT_EXT_O);
			tmp = malloc(sizeof(*tmp) * length);
			if (tmp) {
				RomID(Identifier, RomHeader);
				snprintf(tmp, length, CHT_EXT_O, cheat.selected);
				Settings_Delete(APPS_NAME, Identifier, tmp);
				free(tmp);
			}
		}
	}
	break;
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_CHEAT_LIST:
			if (HIWORD(wParam) == LBN_DBLCLK) { 
				PostMessage(hDlg, WM_COMMAND, IDOK, 0);
				break; 
			}
			break;
		case IDOK:
		{
			char * name, * ext;
			unsigned int index, name_length, ext_length;

			index = SendMessage(GetDlgItem(hDlg, IDC_CHEAT_LIST), LB_GETCURSEL, 0, 0);

			if (index < 0)
				index = 0;

			name_length = GetWindowTextLength(GetDlgItem(hDlg, IDC_CHEAT_NAME));
			ext_length = SendMessage(GetDlgItem(hDlg, IDC_CHEAT_FULL), LB_GETTEXTLEN, index, 0);

			if (name_length == 0 || ext_length == 0) {
				EndDialog(hDlg, 0);
				return TRUE;
			}

			// Make these arrays large enough to hold the name and selected option
			name_length++;
			ext_length++;
			name = malloc(sizeof(*name) * name_length);
			ext = malloc(sizeof(*ext) * ext_length);

			// Could not allocate memory
			if (!name || !ext) {
				if (name)
					free(name);
				if (ext)
					free(ext);
				EndDialog(hDlg, 0);
				return TRUE;
			}

			GetDlgItemText(hDlg, IDC_CHEAT_NAME, name, name_length);
			index = SendMessage(GetDlgItem(hDlg, IDC_CHEAT_FULL), LB_GETTEXT, index, (LPARAM)ext);

			if (index != LB_ERR) {
				char* tmp = malloc(name_length + strlen(CHT_EXT_O));

				if (tmp != NULL) {
					char Identifier[100];

					RomID(Identifier, RomHeader);

					snprintf(tmp, name_length + strlen(CHT_EXT_O), CHT_EXT_O, name);
					Settings_Write(APPS_NAME, Identifier, tmp, ext);
					free(tmp);
				}
			}

			if (name)
				free(name);
			if (ext)
				free(ext);

			LoadCheats();
		}
		EndDialog(hDlg, 0);
		break;
		case IDCANCEL:
			EndDialog(hDlg, 0);
			break;
		}
	default:
		return FALSE;
	}
	return TRUE;
}

// Ensure the input into IDC_CHEAT_CODES and IDC_CHEAT_OPTIONS contains Windows style line endings (Carriage Return Line Feed)
// Depending on the hDlg passed this will either use the Edit Cheat or Add Cheat dialogs
void NormalizeInput(HWND hDlg, DWORD hHandle) {
	unsigned int len;
	char* str;

	len = Edit_GetTextLength(GetDlgItem(hDlg, hHandle));
	if (len > 0) {
		str = malloc(sizeof(*str) * (len + 1));	// Include NULL character
		if (str == NULL)
			return;
		Edit_GetText(GetDlgItem(hDlg, hHandle), str, len + 1);

		// To prevent the caret from being moved unnecessarily (while editing for example) only update if the text needed carriage returns
		if (NeededCRLFChange(&str))
			Edit_SetText(GetDlgItem(hDlg, hHandle), str);

		// Cleanup of allocated memory
		free(str);
	}
}

BOOL NeededCRLFChange(char** str) {
	char* grow, * scan;
	unsigned int length, count, replaces;

	length = strlen(*str);

	// Nothing to change
	if (*str == NULL || length <= 0)
		return FALSE;

	// Prep work for scanning of the amount of carriage returns that must be inserted
	replaces = 0;
	scan = strchr(*str, '\n');

	// Exceptional case, the line starts with a new line character and we cannot check the previous character for a carriage return
	if (scan == *str) {
		replaces++;
		scan = strchr(scan + 1, '\n');
	}

	while (scan != NULL) {
		if ((scan - 1)[0] != '\r')
			replaces++;
		scan = strchr(scan + 1, '\n');
	}

	if (replaces == 0)
		return FALSE;

	grow = malloc(sizeof(*grow) * (length + replaces + 1));
	if (!grow)
		return FALSE;

	strcpy(grow, *str);
	free(*str);
	*str = grow;

	// Scan for and add missing carriage returns before newlines
	for (count = length + replaces - 1; replaces != 0 && count > 0; count--) {
		(*str)[count] = (*str)[count];
		
		if ((*str)[count] == '\n' && (*str)[count - 1] != '\r') {
			replaces--;
			(*str)[count] = '\r';
		}
	}

	return TRUE;
}

void ReadDialogInput(CHEAT* cheat, HWND hDlg) {
	unsigned int length, count;

	// Copy the cheat name
	cheat->name = NULL;
	length = SendDlgItemMessage(hDlg, IDC_CODE_NAME, WM_GETTEXTLENGTH, 0, 0) + 1;	// + 1 to include null pointer
	cheat->name = malloc(sizeof(*cheat->name) * length);
	if (cheat->name)
		GetDlgItemText(hDlg, IDC_CODE_NAME, cheat->name, length);

	// Copy the code string and normalize the input (These are in lines but we need them to be separated with commas)
	cheat->codestring = NULL;
	length = SendDlgItemMessage(hDlg, IDC_CHEAT_CODES, WM_GETTEXTLENGTH, 0, 0);
	if (length > 0) {
		char* buffer;
		unsigned int count2;

		length += 2;
		buffer = malloc(sizeof(*cheat->codestring) * length);
		cheat->codestring = malloc(sizeof(*cheat->codestring) * length);

		if (cheat->codestring && buffer) {
			buffer[0] = ',';
			GetDlgItemText(hDlg, IDC_CHEAT_CODES, (buffer + 1), length);

			// Replace all carriage returns with commas and new lines with spaces
			for (count = 0, count2 = 0; count < length; count++) {

				// Skip carriage returns
				if (buffer[count] == '\r')
					continue;

				// New lines are special, they are replaced with commas unless the previous replacement was a comma
				else if (buffer[count] == '\n') {
					if (count2 > 0 && cheat->codestring[count2 - 1] == ',')
						continue;
					else
						cheat->codestring[count2] = ',';
				}
				else
					cheat->codestring[count2] = buffer[count];
				
				count2++;
			}
			
			// Should not end in a comma
			if (cheat->codestring[count2 - 2] == ',')
				cheat->codestring[count2 - 2] = '\0';

			free(buffer);
		}
	}

	// Copy the options string and normalize the input (These are in lines but we need them to be separated with commas)
	cheat->options = NULL;
	length = SendDlgItemMessage(hDlg, IDC_CHEAT_OPTIONS, WM_GETTEXTLENGTH, 0, 0);
	if (length > 0) {
		length += 2;
		cheat->options = malloc(sizeof(*cheat->options) * length);
		if (cheat->options) {
			GetDlgItemText(hDlg, IDC_CHEAT_OPTIONS, (cheat->options + 1), length);
			cheat->options[0] = '$';

			// Replace all carriage returns with commas and new lines with spaces
			for (count = 1; count < length; count++) {
				if (cheat->options[count] == '\r')
					cheat->options[count] = ',';

				if (cheat->options[count] == '\n')
					cheat->options[count] = '$';
			}
		}
	}

	// Copy the note
	cheat->note = NULL;
	length = SendDlgItemMessage(hDlg, IDC_NOTES, WM_GETTEXTLENGTH, 0, 0);
	if (length > 0) {
		length++;
		cheat->note = malloc(sizeof(*cheat->note) * length);
		if (cheat->note)
			GetDlgItemText(hDlg, IDC_NOTES, cheat->note, length);
	}

	cheat->selected = NULL;
	cheat->replacedstring = NULL;
}

/********************************************************************************************
  CheatAddProc

  Purpose:
  Parameters:
  Returns:

********************************************************************************************/
LRESULT CALLBACK CheatAddProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch (uMsg) {
	case WM_INITDIALOG:
		SetWindowText(hDlg, GS(CHEAT_ADDCHEAT_FRAME));
		SetWindowText(GetDlgItem(hDlg, IDC_NAME), GS(CHEAT_ADDCHEAT_NAME));
		SetWindowText(GetDlgItem(hDlg, IDC_CODE), GS(CHEAT_ADDCHEAT_CODE));
		SetWindowText(GetDlgItem(hDlg, IDC_LABEL_OPTIONS), GS(CHEAT_ADDCHEAT_OPT));
		SetWindowText(GetDlgItem(hDlg, IDC_CODE_DES), GS(CHEAT_ADDCHEAT_CODEDES));
		SetWindowText(GetDlgItem(hDlg, IDC_LABEL_OPTIONS_FORMAT), GS(CHEAT_ADDCHEAT_OPTDES));
		SetWindowText(GetDlgItem(hDlg, IDC_CHEATNOTES), GS(CHEAT_ADDCHEAT_NOTES));
		SetWindowText(GetDlgItem(hDlg, IDC_NEWCHEAT), GS(CHEAT_ADDCHEAT_NEW));
		SetWindowText(GetDlgItem(hDlg, IDC_ADD), GS(CHEAT_ADDCHEAT_ADD));
		break;
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDCANCEL:
			EndDialog(hDlg, 0);
			break;
		case IDC_CODE_NAME:
			if (HIWORD(wParam) == EN_CHANGE) {
				CHEAT cheat;

				ReadDialogInput(&cheat, hDlg);

				if (Cheats_VerifyInput(&cheat))
					EnableWindow(GetDlgItem(hDlg, IDC_ADD), TRUE);
				else
					EnableWindow(GetDlgItem(hDlg, IDC_ADD), FALSE);

				// This was added because if you write out the code string first and then the name the side screen for options is disabled
				// until you go back and adjust one of the codes
				switch (cheat.type) {
				case O_REPLACE:
				case MO_REPLACE:
				case SO_REPLACE:
				case BAD_REPLACE:
					if (!IsWindowEnabled(GetDlgItem(hDlg, IDC_LABEL_OPTIONS))) {
						EnableWindow(GetDlgItem(hDlg, IDC_LABEL_OPTIONS), TRUE);
						EnableWindow(GetDlgItem(hDlg, IDC_LABEL_OPTIONS_FORMAT), TRUE);
						EnableWindow(GetDlgItem(hDlg, IDC_CHEAT_OPTIONS), TRUE);
					}
					break;
				default:
					if (IsWindowEnabled(GetDlgItem(hDlg, IDC_LABEL_OPTIONS))) {
						EnableWindow(GetDlgItem(hDlg, IDC_LABEL_OPTIONS), FALSE);
						EnableWindow(GetDlgItem(hDlg, IDC_LABEL_OPTIONS_FORMAT), FALSE);
						EnableWindow(GetDlgItem(hDlg, IDC_CHEAT_OPTIONS), FALSE);
					}
					break;
				}

				Cheats_ClearCheat(&cheat);
			}
			break;
		case IDC_CHEAT_CODES:
			if (HIWORD(wParam) == EN_CHANGE) {
				CHEAT cheat;

				NormalizeInput(hDlg, IDC_CHEAT_CODES);
				ReadDialogInput(&cheat, hDlg);

				if (Cheats_VerifyInput(&cheat))
					EnableWindow(GetDlgItem(hDlg, IDC_ADD), TRUE);
				else
					EnableWindow(GetDlgItem(hDlg, IDC_ADD), FALSE);

				switch (cheat.type) {
				case O_REPLACE:
				case MO_REPLACE:
				case SO_REPLACE:
				case BAD_REPLACE:
					if (!IsWindowEnabled(GetDlgItem(hDlg, IDC_LABEL_OPTIONS))) {
						EnableWindow(GetDlgItem(hDlg, IDC_LABEL_OPTIONS), TRUE);
						EnableWindow(GetDlgItem(hDlg, IDC_LABEL_OPTIONS_FORMAT), TRUE);
						EnableWindow(GetDlgItem(hDlg, IDC_CHEAT_OPTIONS), TRUE);
					}
					break;
				default:
					if (IsWindowEnabled(GetDlgItem(hDlg, IDC_LABEL_OPTIONS))) {
						EnableWindow(GetDlgItem(hDlg, IDC_LABEL_OPTIONS), FALSE);
						EnableWindow(GetDlgItem(hDlg, IDC_LABEL_OPTIONS_FORMAT), FALSE);
						EnableWindow(GetDlgItem(hDlg, IDC_CHEAT_OPTIONS), FALSE);
					}
					break;
				}

				Cheats_ClearCheat(&cheat);
			}
		case IDC_CHEAT_OPTIONS:
			if (HIWORD(wParam) == EN_CHANGE) {
				CHEAT cheat;

				NormalizeInput(hDlg, IDC_CHEAT_OPTIONS);
				ReadDialogInput(&cheat, hDlg);

				if (Cheats_VerifyInput(&cheat))
					EnableWindow(GetDlgItem(hDlg, IDC_ADD), TRUE);
				else
					EnableWindow(GetDlgItem(hDlg, IDC_ADD), FALSE);

				Cheats_ClearCheat(&cheat);
			}
			break;
		case IDC_ADD:
		{
			CHEAT cheat;

			ReadDialogInput(&cheat, hDlg);
			
			// This should always be true if the add cheat button is enabled (IDC_ADD case cannot occur if the button is disabled)
			// However it doesn't cost much to check again so might as make sure it's good
			if (Cheats_VerifyInput(&cheat)) {
				// Write the new cheat
				Cheats_Write(&cheat);
				Cheats_ClearCheat(&cheat);

				// Update the cheat list and close the edit dialog box
				RefreshCheatManager();
				SetDlgItemText(hDlg, IDC_CODE_NAME, STR_EMPTY);
				SetDlgItemText(hDlg, IDC_CHEAT_CODES, STR_EMPTY);
				SetDlgItemText(hDlg, IDC_CHEAT_OPTIONS, STR_EMPTY);
				SetDlgItemText(hDlg, IDC_NOTES, STR_EMPTY);

				EnableWindow(GetDlgItem(hDlg, IDC_ADD), FALSE);
				EnableWindow(hDlg, FALSE);
				EndDialog(hDlg, 0);
			}
			else
				Cheats_ClearCheat(&cheat);
		}
		break;
		case IDC_NEWCHEAT:
		{
			SetDlgItemText(hDlg, IDC_CODE_NAME, STR_EMPTY);
			SetDlgItemText(hDlg, IDC_CHEAT_CODES, STR_EMPTY);
			SetDlgItemText(hDlg, IDC_CHEAT_OPTIONS, STR_EMPTY);
			SetDlgItemText(hDlg, IDC_NOTES, STR_EMPTY);
			EnableWindow(GetDlgItem(hDlg, IDC_ADD), FALSE);
		}
		break;
		}
		break;
	default:
		return FALSE;
	}
	return TRUE;
}

LRESULT CALLBACK CheatEditProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	static int CheatNo;

	switch (uMsg) {
	case WM_INITDIALOG:
	{
		char* ReadPos, * Buffer;
		TVITEM item;
		CHEAT cheat;

		SetWindowText(hDlg, GS(CHEAT_EDITCHEAT_WINDOW));
		SetWindowText(GetDlgItem(hDlg, IDC_NAME), GS(CHEAT_ADDCHEAT_NAME));
		SetWindowText(GetDlgItem(hDlg, IDC_CODE), GS(CHEAT_ADDCHEAT_CODE));
		SetWindowText(GetDlgItem(hDlg, IDC_LABEL_OPTIONS), GS(CHEAT_ADDCHEAT_OPT));
		SetWindowText(GetDlgItem(hDlg, IDC_CODE_DES), GS(CHEAT_ADDCHEAT_CODEDES));
		SetWindowText(GetDlgItem(hDlg, IDC_LABEL_OPTIONS_FORMAT), GS(CHEAT_ADDCHEAT_OPTDES));
		SetWindowText(GetDlgItem(hDlg, IDC_CHEATNOTES), GS(CHEAT_ADDCHEAT_NOTES));
		SetWindowText(GetDlgItem(hDlg, IDC_ADD), GS(CHEAT_EDITCHEAT_UPDATE));

		item.mask = TVIF_PARAM;
		item.hItem = (HTREEITEM)lParam;
		TreeView_GetItem(hCheatTree, &item);
		CheatNo = item.lParam;

		// Read everything related to the cheat
		// This data will be used to set the fields for Edit Cheat
		Cheats_Read(&cheat, item.lParam);

		// Set Cheat Name
		SetDlgItemText(hDlg, IDC_CODE_NAME, cheat.name);

		// These will be updated with proper values but use these as defaults
		SetDlgItemText(hDlg, IDC_CHEAT_CODES, STR_EMPTY);
		SetDlgItemText(hDlg, IDC_CHEAT_OPTIONS, STR_EMPTY);

		// Add Gameshark codes to screen
		if (strcmp(cheat.codestring, STR_EMPTY) != 0) {
			ReadPos = cheat.codestring + 1;
			Buffer = malloc(sizeof(*Buffer) * (strlen(ReadPos) + MaxGSEntries));
			if (Buffer) {
				memset(Buffer, 0, sizeof(*Buffer) * (strlen(ReadPos) + MaxGSEntries));
				do {
					strncat(Buffer, ReadPos, strchr(ReadPos, ',') - ReadPos);
					ReadPos = strchr(ReadPos, ',');
					if (ReadPos != NULL) {
						strcat(Buffer, "\r\n");
						ReadPos += 1;
					}
				} while (ReadPos);

				SetDlgItemText(hDlg, IDC_CHEAT_CODES, Buffer);

				// Clean up memory
				free(Buffer);
				Buffer = NULL;
			}
		}

		// Add option values to screen
		if (strcmp(cheat.options, STR_EMPTY) != 0) {
			ReadPos = strchr(cheat.options, '$') + 1;
			Buffer = malloc(sizeof(*Buffer) * (strlen(ReadPos) + 2));
			if (Buffer) {
				memset(Buffer, 0, sizeof(*Buffer) * (strlen(ReadPos) + 2));
				do {
					strncat(Buffer, ReadPos, strchr(ReadPos, ',') - ReadPos);
					ReadPos = strchr(ReadPos, '$');
					if (ReadPos != NULL) {
						strcat(Buffer, "\r\n");
						ReadPos += 1;
					}
				} while (ReadPos);

				SetDlgItemText(hDlg, IDC_CHEAT_OPTIONS, Buffer);

				// Clean up memory
				free(Buffer);
				Buffer = NULL;
			}	
		}

		// Add cheat Notes
		SetDlgItemText(hDlg, IDC_NOTES, cheat.note);

		// Clean up the cheat structure
		Cheats_ClearCheat(&cheat);

		// Update Screen
		PostMessage(hDlg, WM_COMMAND, MAKEWPARAM(IDC_CHEAT_CODES, EN_CHANGE), (LPARAM)GetDlgItem(hDlg, IDC_CHEAT_CODES));
	}
	break;
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDCANCEL:
			EndDialog(hDlg, 0);
			break;
		case IDC_CODE_NAME:
			if (HIWORD(wParam) == EN_CHANGE) {
				CHEAT cheat;

				ReadDialogInput(&cheat, hDlg);

				if (Cheats_VerifyInput(&cheat))
					EnableWindow(GetDlgItem(hDlg, IDC_ADD), TRUE);
				else
					EnableWindow(GetDlgItem(hDlg, IDC_ADD), FALSE);

				// This was added because if you write out the code string first and then the name the side screen for options is disabled
				// until you go back and adjust one of the codes
				switch (cheat.type) {
				case O_REPLACE:
				case MO_REPLACE:
				case SO_REPLACE:
				case BAD_REPLACE:
					if (!IsWindowEnabled(GetDlgItem(hDlg, IDC_LABEL_OPTIONS))) {
						EnableWindow(GetDlgItem(hDlg, IDC_LABEL_OPTIONS), TRUE);
						EnableWindow(GetDlgItem(hDlg, IDC_LABEL_OPTIONS_FORMAT), TRUE);
						EnableWindow(GetDlgItem(hDlg, IDC_CHEAT_OPTIONS), TRUE);
					}
					break;
				default:
					if (IsWindowEnabled(GetDlgItem(hDlg, IDC_LABEL_OPTIONS))) {
						EnableWindow(GetDlgItem(hDlg, IDC_LABEL_OPTIONS), FALSE);
						EnableWindow(GetDlgItem(hDlg, IDC_LABEL_OPTIONS_FORMAT), FALSE);
						EnableWindow(GetDlgItem(hDlg, IDC_CHEAT_OPTIONS), FALSE);
					}
					break;
				}

				Cheats_ClearCheat(&cheat);
			}
			break;
		case IDC_CHEAT_CODES:
			if (HIWORD(wParam) == EN_CHANGE) {
				CHEAT cheat;

				NormalizeInput(hDlg, IDC_CHEAT_CODES);
				ReadDialogInput(&cheat, hDlg);

				if (Cheats_VerifyInput(&cheat))
					EnableWindow(GetDlgItem(hDlg, IDC_ADD), TRUE);
				else
					EnableWindow(GetDlgItem(hDlg, IDC_ADD), FALSE);

				switch (cheat.type) {
				case O_REPLACE:
				case MO_REPLACE:
				case SO_REPLACE:
				case BAD_REPLACE:
					if (!IsWindowEnabled(GetDlgItem(hDlg, IDC_LABEL_OPTIONS))) {
						EnableWindow(GetDlgItem(hDlg, IDC_LABEL_OPTIONS), TRUE);
						EnableWindow(GetDlgItem(hDlg, IDC_LABEL_OPTIONS_FORMAT), TRUE);
						EnableWindow(GetDlgItem(hDlg, IDC_CHEAT_OPTIONS), TRUE);
					}
					break;
				default:
					if (IsWindowEnabled(GetDlgItem(hDlg, IDC_LABEL_OPTIONS))) {
						EnableWindow(GetDlgItem(hDlg, IDC_LABEL_OPTIONS), FALSE);
						EnableWindow(GetDlgItem(hDlg, IDC_LABEL_OPTIONS_FORMAT), FALSE);
						EnableWindow(GetDlgItem(hDlg, IDC_CHEAT_OPTIONS), FALSE);
					}
					break;
				}

				Cheats_ClearCheat(&cheat);
			}
			break;
		case IDC_CHEAT_OPTIONS:
			if (HIWORD(wParam) == EN_CHANGE) {
				CHEAT cheat;

				NormalizeInput(hDlg, IDC_CHEAT_OPTIONS);
				ReadDialogInput(&cheat, hDlg);

				if (Cheats_VerifyInput(&cheat))
					EnableWindow(GetDlgItem(hDlg, IDC_ADD), TRUE);
				else
					EnableWindow(GetDlgItem(hDlg, IDC_ADD), FALSE);

				Cheats_ClearCheat(&cheat);
			}
			break;
		case IDC_ADD:
		{
			CHEAT cheat;

			ReadDialogInput(&cheat, hDlg);
			
			// This should always be true if the update cheat button is enabled (IDC_ADD case cannot occur if the button is disabled)
			// However it doesn't cost much to check again so might as make sure it's good
			if (Cheats_VerifyInput(&cheat)) {
				CHEAT del;

				// Delete the current entry being edited and write the new one
				// This is done because the edited cheat may have a new name
				Cheats_Read(&del, CheatNo);				
				Cheats_Delete(&del);
				Cheats_ClearCheat(&del);

				// Write the new cheat
				Cheats_Write(&cheat);
				Cheats_ClearCheat(&cheat);

				// Update the cheat list and close the edit dialog box
				RefreshCheatManager();
				EndDialog(hDlg, 0);
			}
			else
				Cheats_ClearCheat(&cheat);
		}
		break;
		}
		break;
	default:
		return FALSE;
	}
	return TRUE;
}

void ChangeChildrenStatus(HTREEITEM hParent, BOOL Checked) {
	HTREEITEM hItem = TreeView_GetChild(hCheatTree, hParent);
	if (hItem == NULL) {
		//Save Cheat
		CHEAT cheat = { 0 };
		TVITEM item;

		if (hParent == TVI_ROOT)
			return;

		item.mask = TVIF_PARAM;
		item.hItem = hParent;
		TreeView_GetItem(hCheatTree, &item);

		Cheats_Read(&cheat, item.lParam);
		SaveCheat(cheat.name, Checked);
		Cheats_ClearCheat(&cheat);
		return;
	}
	while (hItem != NULL) {
		ChangeChildrenStatus(hItem, Checked);
		_TreeView_SetCheckState(hCheatTree, hItem, Checked ? TV_STATE_CHECKED : TV_STATE_CLEAR);
		hItem = TreeView_GetNextSibling(hCheatTree, hItem);
	}
}

void CheckParentStatus(HTREEITEM hParent) {
	int CurrentState, InitialState;
	HTREEITEM hItem;

	if (!hParent) { return; }
	hItem = TreeView_GetChild(hCheatTree, hParent);
	InitialState = _TreeView_GetCheckState(hCheatTree, hParent);
	CurrentState = _TreeView_GetCheckState(hCheatTree, hItem);

	while (hItem != NULL) {
		if (_TreeView_GetCheckState(hCheatTree, hItem) != CurrentState) {
			CurrentState = TV_STATE_INDETERMINATE;
			break;
		}
		hItem = TreeView_GetNextSibling(hCheatTree, hItem);
	}
	_TreeView_SetCheckState(hCheatTree, hParent, CurrentState);
	if (InitialState != CurrentState) {
		CheckParentStatus(TreeView_GetParent(hCheatTree, hParent));
	}
}

/********************************************************************************************
  CheatListProc

  Purpose:
  Parameters:
  Returns:

********************************************************************************************/
LRESULT CALLBACK CheatListProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	static HTREEITEM hSelectedItem;

	switch (uMsg) {
	case WM_INITDIALOG:
	{
		DWORD Style;
		RECT rcList;
		RECT rcButton;

		SetWindowText(GetDlgItem(hDlg, IDC_CHEATSFRAME), GS(CHEAT_LIST_FRAME));
		SetWindowText(GetDlgItem(hDlg, IDC_NOTESFRAME), GS(CHEAT_NOTES_FRAME));
		SetWindowText(GetDlgItem(hDlg, IDC_UNMARK), GS(CHEAT_MARK_NONE));

		GetWindowRect(GetDlgItem(hDlg, IDC_CHEATSFRAME), &rcList);
		GetWindowRect(GetDlgItem(hDlg, IDC_UNMARK), &rcButton);

		hCheatTree = CreateWindowEx(WS_EX_CLIENTEDGE, WC_TREEVIEW, STR_EMPTY,
			WS_CHILD | WS_TABSTOP | WS_VISIBLE, 8, 15, rcList.right - rcList.left - 16, rcButton.top - rcList.top - 22, hDlg, (HMENU)IDC_MYTREE, hInst, NULL);
		Style = GetWindowLong(hCheatTree, GWL_STYLE);
		SetWindowLong(hCheatTree, GWL_STYLE, TVS_HASLINES | TVS_HASBUTTONS | TVS_LINESATROOT | TVS_DISABLEDRAGDROP | TVS_CHECKBOXES | TVS_SHOWSELALWAYS | Style);

		{
			HIMAGELIST hImageList;
			HBITMAP hBmp;

			hImageList = ImageList_Create(16, 16, ILC_COLOR | ILC_MASK, 40, 40);
			hBmp = LoadBitmap(hInst, MAKEINTRESOURCE(IDB_BITMAP1));
			ImageList_AddMasked(hImageList, hBmp, RGB(255, 0, 255));
			DeleteObject(hBmp);

			TreeView_SetImageList(hCheatTree, hImageList, TVSIL_STATE);
		}
		hSelectedItem = NULL;

	}
	break;
	case WM_SIZE:
	{
		int nWidth = LOWORD(lParam);  // width of client area 
		int nHeight = HIWORD(lParam); // height of client area 

		SetWindowPos(hCheatTree, HWND_TOP, 0, 0, nWidth - 13, nHeight - 142, SWP_NOMOVE | SWP_NOOWNERZORDER);
		SetWindowPos(GetDlgItem(hDlg, IDC_CHEATSFRAME), HWND_TOP, 0, 0, nWidth, nHeight - 100, SWP_NOOWNERZORDER);
		SetWindowPos(GetDlgItem(hDlg, IDC_NOTESFRAME), HWND_TOP, 0, nHeight - 96, nWidth, 95, SWP_NOOWNERZORDER);
		SetWindowPos(GetDlgItem(hDlg, IDC_NOTES), HWND_TOP, 6, nHeight - 80, nWidth - 12, 72, SWP_NOOWNERZORDER);
		SetWindowPos(GetDlgItem(hDlg, IDC_UNMARK), HWND_TOP, nWidth - 110, nHeight - 124, 0, 0, SWP_NOSIZE | SWP_NOOWNERZORDER);
		SetWindowPos(GetDlgItem(hDlg, IDC_DELETE), HWND_TOP, nWidth - 165, nHeight - 124, 0, 0, SWP_NOSIZE | SWP_NOOWNERZORDER);
	}
	break;
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case ID_POPUP_ADDNEWCHEAT:
			DialogBox(hInst, MAKEINTRESOURCE(IDD_Cheats_Add), hDlg, (DLGPROC)CheatAddProc);
			break;
		case ID_POPUP_EDIT:
			DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_Cheats_Edit), hDlg, (DLGPROC)CheatEditProc, (LPARAM)hSelectedItem);
			break;
		case ID_POPUP_DELETE:
		{
			TVITEM item;
			CHEAT cheat = { 0 };

			int Response = MessageBox(hDlg, GS(MSG_DEL_SURE), GS(MSG_DEL_TITLE), MB_YESNO | MB_ICONQUESTION);
			if (Response != IDYES) { break; }

			item.hItem = hSelectedItem;
			item.mask = TVIF_PARAM;
			TreeView_GetItem(hCheatTree, &item);

			// Delete the selected cheat and update the Cheat Manager
			if (Cheats_Read(&cheat, item.lParam)) {
				Cheats_Delete(&cheat);
				Cheats_ClearCheat(&cheat);
				RefreshCheatManager();
			}
		}
		break;
		case IDC_UNMARK:
			ChangeChildrenStatus(TVI_ROOT, FALSE);
			LoadCheats();
			break;
		}
		break;
	case WM_NOTIFY:
	{
		LPNMHDR lpnmh = (LPNMHDR)lParam;

		if ((lpnmh->code == NM_RCLICK) && (lpnmh->idFrom == IDC_MYTREE))
		{
			{
				TVHITTESTINFO ht = { 0 };
				DWORD dwpos = GetMessagePos();

				// include <windowsx.h> and <windows.h> header files
				ht.pt.x = GET_X_LPARAM(dwpos);
				ht.pt.y = GET_Y_LPARAM(dwpos);
				MapWindowPoints(HWND_DESKTOP, lpnmh->hwndFrom, &ht.pt, 1);


				/* Removed Add New Cheat, Edit & Delete Cheats from Advanced Mode & made available in Basic Mode. (Gent)
				if (BasicMode) { return TRUE; } */

				TreeView_HitTest(lpnmh->hwndFrom, &ht);
				hSelectedItem = ht.hItem;
			}
			{
				HMENU hMenu = LoadMenu(hInst, MAKEINTRESOURCE(IDR_CHEAT_MENU));
				HMENU hPopupMenu = GetSubMenu(hMenu, 0);
				POINT Mouse;

				GetCursorPos(&Mouse);

				MenuSetText(hPopupMenu, 0, GS(CHEAT_ADDNEW), NULL);
				MenuSetText(hPopupMenu, 1, GS(CHEAT_EDIT), NULL);
				MenuSetText(hPopupMenu, 3, GS(CHEAT_DELETE), NULL);

				if (hSelectedItem == NULL || TreeView_GetChild(hCheatTree, hSelectedItem) != NULL) {
					DeleteMenu(hPopupMenu, 3, MF_BYPOSITION);
					DeleteMenu(hPopupMenu, 2, MF_BYPOSITION);
					DeleteMenu(hPopupMenu, 1, MF_BYPOSITION);
				}
				TrackPopupMenu(hPopupMenu, 0, Mouse.x, Mouse.y, 0, hDlg, NULL);
				DestroyMenu(hMenu);
			}
		}
		if ((lpnmh->code == NM_CLICK) && (lpnmh->idFrom == IDC_MYTREE))
		{
			TVHITTESTINFO ht = { 0 };
			DWORD dwpos = GetMessagePos();

			// include <windowsx.h> and <windows.h> header files
			ht.pt.x = GET_X_LPARAM(dwpos);
			ht.pt.y = GET_Y_LPARAM(dwpos);
			MapWindowPoints(HWND_DESKTOP, lpnmh->hwndFrom, &ht.pt, 1);

			TreeView_HitTest(lpnmh->hwndFrom, &ht);

			if (TVHT_ONITEMSTATEICON & ht.flags)
			{
				switch (_TreeView_GetCheckState(hCheatTree, ht.hItem)) {
				case TV_STATE_CLEAR:
				case TV_STATE_INDETERMINATE:
					_TreeView_SetCheckState(hCheatTree, ht.hItem, TV_STATE_CHECKED);
					ChangeChildrenStatus(ht.hItem, TRUE);
					CheckParentStatus(TreeView_GetParent(hCheatTree, ht.hItem));
					_TreeView_SetCheckState(hCheatTree, ht.hItem, TV_STATE_INDETERMINATE);
					break;
				case TV_STATE_CHECKED:
					_TreeView_SetCheckState(hCheatTree, ht.hItem, TV_STATE_CLEAR);
					ChangeChildrenStatus(ht.hItem, FALSE);
					CheckParentStatus(TreeView_GetParent(hCheatTree, ht.hItem));
					_TreeView_SetCheckState(hCheatTree, ht.hItem, TV_STATE_CHECKED);
					break;
				}
				LoadCheats();
			}
		}
		if ((lpnmh->code == NM_DBLCLK) && (lpnmh->idFrom == IDC_MYTREE))
		{
			TVHITTESTINFO ht = { 0 };
			DWORD dwpos = GetMessagePos();

			// include <windowsx.h> and <windows.h> header files
			ht.pt.x = GET_X_LPARAM(dwpos);
			ht.pt.y = GET_Y_LPARAM(dwpos);
			MapWindowPoints(HWND_DESKTOP, lpnmh->hwndFrom, &ht.pt, 1);

			TreeView_HitTest(lpnmh->hwndFrom, &ht);

			if (TVHT_ONITEMLABEL & ht.flags) {
				PostMessage(hDlg, UM_CHANGEEXTENSION, 0, (LPARAM)ht.hItem);
			}
		}
		if ((lpnmh->code == TVN_SELCHANGED) && (lpnmh->idFrom == IDC_MYTREE)) {
			HTREEITEM hItem;

			hItem = TreeView_GetSelection(hCheatTree);
			if (TreeView_GetChild(hCheatTree, hItem) == NULL) {
				TVITEM item;
				CHEAT cheat = { 0 };

				item.mask = TVIF_PARAM;
				item.hItem = hItem;
				TreeView_GetItem(hCheatTree, &item);

				Cheats_Read(&cheat, item.lParam);
				if (cheat.note != NULL)
					SetDlgItemText(hDlg, IDC_NOTES, cheat.note);
				else
					SetDlgItemText(hDlg, IDC_NOTES, STR_EMPTY);
				Cheats_ClearCheat(&cheat);
			}
			else {
				SetDlgItemText(hDlg, IDC_NOTES, STR_EMPTY);
			}
		}
	}
	break;
	case UM_CHANGEEXTENSION:
	{
		char CheatName[500];
		char Read[500];
		char* check;
		TVITEM item;
		CHEAT cheat = { 0 };

		item.mask = TVIF_TEXT;
		item.hItem = (HTREEITEM)lParam;
		item.cchTextMax = sizeof(Read);
		item.pszText = Read;
		TreeView_GetItem(hCheatTree, &item);

		// On failure go no further, return TRUE because this is still a handled event
		if (!Cheats_Read(&cheat, item.lParam) || cheat.type == NO_REPLACE)
			return TRUE;
		
		// Check to make sure this is the proper child node being double clicked
		check = strrchr(cheat.name, '\\');
		if (check != NULL) {
			if (strncmp((check + 1), Read, strlen(check + 1)) != 0)
				return TRUE;
		}

		DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_Cheats_CodeEx), hDlg, (DLGPROC)CheatsCodeExProc, (LPARAM)&cheat);

		// Re-read the cheat, a new option has been selected
		Cheats_Read(&cheat, item.lParam);

		Cheats_DisplayName(&cheat, CheatName, sizeof(CheatName));
		item.mask = TVIF_PARAM | TVIF_TEXT;
		item.pszText = CheatName;
		item.cchTextMax = sizeof(CheatName);

		// The '\' is used as a special character to separate names into expandable trees so only show the last portion of the name after the '\'
		if (strrchr(CheatName, '\\') != NULL) {
			strcpy(CheatName, strrchr(CheatName, '\\') + 1);
		}
		TreeView_SetItem(hCheatTree, &item);
	}
	break;
	default:
		return FALSE;
	}
	return TRUE;
}

void DisableAllCheats(void) {
	char Identifier[100];

	RomID(Identifier, RomHeader);
	Settings_DeleteEntry(APPS_NAME, Identifier);
}

void CloseCheatWindow(void) {
	if (!hManageWindow) { return; }
	SendMessage(hManageWindow, UM_CLOSE_CHEATS, 0, 0);
}

void LoadCode(CHEAT *cheat)
{
	char* ReadPos;
	int count2;

	if (cheat->replacedstring != NULL && strcmp(cheat->replacedstring, STR_EMPTY) != 0)
		ReadPos = cheat->replacedstring;
	else
		ReadPos = cheat->codestring;

	// Nothing to load
	if (ReadPos == NULL || strlen(ReadPos) == 0)
		return;

	if (ReadPos[0] == ',')
		ReadPos++;

	// Just in case a space was passed -- We are now supporting spaces after the commas
	// Because it makes sense to me -- RadeonUser
	if (ReadPos[0] == ' ')
		ReadPos++;

	for (count2 = 0; count2 < MaxGSEntries; count2++) {

		// This will always be in "Address Value" format so reject if there is no space
		if (strchr(ReadPos, ' ') == NULL)
			return;

		Codes[NoOfCodes].Code[count2].Command = AsciiToHex(ReadPos);

		// There is always a space following the address to separate the value
		ReadPos = strchr(ReadPos, ' ') + 1;
		
		Codes[NoOfCodes].Code[count2].Value = (WORD)AsciiToHex(ReadPos);

		ReadPos = strchr(ReadPos, ',');

		// Finished parsing
		if (ReadPos == NULL) {
			count2++;
			break;
		}

		// Move on to the next entry, keep in mind there may be an optional space
		if (ReadPos[1] == ' ')
			ReadPos += 2;
		else
			ReadPos++;
	}

	if (count2 == 0)
		return;

	if (count2 < MaxGSEntries) {
		Codes[NoOfCodes].Code[count2].Command = 0;
		Codes[NoOfCodes].Code[count2].Value = 0;
	}
	NoOfCodes += 1;
}

void LoadPermCheats() {
	char Identifier[100], CheatName[300];
	int count;
	CHEAT cheat = { 0 };

	RomID(Identifier, RomHeader);

	for (count = 0; count < MaxCheats; count++) {
		sprintf(CheatName, CHT_ENT, count);
		Settings_Read(RDS_NAME, Identifier, CheatName, STR_EMPTY, &cheat.codestring);

		if (strcmp(cheat.codestring, STR_EMPTY) != 0)
			LoadCode(&cheat);
		else
			count = MaxCheats;

		Cheats_ClearCheat(&cheat);
	}
}

void LoadJaboCheats() {   // Jabo Video PermCheat Specific (Gent)
	char Identifier[100], CheatName[300];
	int count;
	CHEAT cheat = { 0 };

	RomID(Identifier, RomHeader);

	for (count = 0; count < MaxCheats; count++) {
		sprintf(CheatName, CHT_ENT, count);
		Settings_Read(INI_NAME, Identifier, CheatName, STR_EMPTY, &cheat.codestring);

		if (strcmp(cheat.codestring, STR_EMPTY) != 0)
			LoadCode(&cheat);
		else
			count = MaxCheats;

		Cheats_ClearCheat(&cheat);
	}
}

/********************************************************************************************
  LoadCheats

  Purpose:
  Parameters:
  Returns:

********************************************************************************************/
void LoadCheats(void) {
	DWORD count;
	CHEAT cheat = { 0 };

	NoOfCodes = 0;

	LoadPermCheats();

	for (count = 0; count < MaxCheats; count++) {

		// A return of FALSE means the cheat entry at count does not exist, keep scanning
		if (!Cheats_Read(&cheat, count))
			continue;

		// If the cheat is not active keep scanning
		// Also clean up memory usage
		if (!CheatActive(cheat.name)) {
			Cheats_ClearCheat(&cheat);
			continue;
		}

		LoadCode(&cheat);
		Cheats_ClearCheat(&cheat);
	}

	LoadJaboCheats();  // Jabo Video PermCheat Specific (Gent)

	for (count = 0; count < MaxCheats; count++) {

		// A return of FALSE means the cheat entry at count does not exist, keep scanning
		if (!Cheats_Read(&cheat, count))
			continue;

		// If the cheat is not active keep scanning
		// Also clean up memory usage
		if (!CheatActive(cheat.name)) {
			Cheats_ClearCheat(&cheat);
			continue;
		}

		LoadCode(&cheat);
		Cheats_ClearCheat(&cheat);
	}
}

void ManageCheats(HWND hParent) {
#define DefaultWindowWidth  315
#define DefaultWindowHeight 415
	DWORD X, Y, WindowWidth, WindowHeight, Style = 0;

	if (hManageWindow != NULL) {
		SetForegroundWindow(hManageWindow);
		RefreshCheatManager();
		ShowWindow(hManageWindow, SW_RESTORE);
		return;
	}
	/*if (hParent) {
		DialogBox(hInst, MAKEINTRESOURCE(IDD_MANAGECHEATS), hParent, (DLGPROC)ManageCheatsProc);
	}else {
		CreateDialog(hInst, MAKEINTRESOURCE(IDD_MANAGECHEATS), hParent, (DLGPROC)ManageCheatsProc);
	}*/

	if (!GetStoredWinSize("Cheat", &WindowWidth, &WindowHeight)) {
		WindowWidth = DefaultWindowWidth;
		WindowHeight = DefaultWindowHeight;
	}
	if (!GetStoredWinPos("Cheat", &X, &Y)) {
		X = (GetSystemMetrics(SM_CXSCREEN) - WindowWidth) / 2;
		Y = (GetSystemMetrics(SM_CYSCREEN) - WindowHeight) / 2;
	}
	Style = hParent ? WS_SIZEBOX | WS_SYSMENU | WS_MINIMIZEBOX : WS_SIZEBOX | WS_SYSMENU | WS_MINIMIZEBOX;
	hManageWindow = CreateWindowEx(NULL, "PJ64.Cheats", GS(CHEAT_TITLE), Style, X, Y, WindowWidth, WindowHeight, hParent, NULL, hInst, NULL);
	RefreshCheatManager();
	ShowWindow(hManageWindow, SW_SHOW);
	SetWindowPos(hManageWindow,       // handle to window
		HWND_TOPMOST,  // placement-order handle
		X,     // horizontal position
		Y,      // vertical position
		WindowWidth,  // width
		WindowHeight, // height
		SWP_SHOWWINDOW // window-positioning options
	);

	while (ShowCursor(TRUE) < 0);
	//if (hParent)
	{
		MSG msg;
		if (hParent)
			EnableWindow(hParent, FALSE);
		while (hManageWindow) {
			if (!GetMessage(&msg, NULL, 0, 0)) {
				DestroyWindow(hManageWindow);
				hManageWindow = NULL;
				PostMessage(msg.hwnd, msg.message, msg.wParam, msg.lParam);
				continue;
			}
			if (IsDialogMessage(hManageWindow, &msg)) { continue; }
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		EnableWindow(hParent, TRUE);
		if (hParent)
			ShowCursor(TRUE);
		SetFocus(hParent);
	}
}

LRESULT CALLBACK Cheat_Proc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
#define MinHeight 260
#define MinWidth  190
	switch (uMsg) {
	case WM_CREATE:
		hSelectCheat = CreateDialog(hInst, MAKEINTRESOURCE(IDD_Cheats_List), hWnd, (DLGPROC)CheatListProc);
		SetWindowPos(hSelectCheat, HWND_TOP, 5, 8, 0, 0, SWP_NOSIZE);
		ShowWindow(hSelectCheat, SW_SHOW);
		break;
		/* Move this to WM_DESTROY, this saves the position EVERY time the window is touched, looked at, moved, etc...
	case WM_MOVE:
		if (IsIconic(hWnd)) { break; }
		StoreCurrentWinPos("Cheat", hWnd );
		break;*/
	case UM_CLOSE_CHEATS:
		DestroyWindow(hWnd);
		break;
	case WM_SIZING:
	{
		LPRECT lprc = (LPRECT)lParam;
		int fwSide = wParam;

		if ((lprc->bottom - lprc->top) <= MinHeight) {
			switch (fwSide) {
			case WMSZ_TOPLEFT:
			case WMSZ_TOP:
			case WMSZ_TOPRIGHT:
				lprc->top = lprc->bottom - MinHeight;
				break;
			case WMSZ_BOTTOMLEFT:
			case WMSZ_BOTTOM:
			case WMSZ_BOTTOMRIGHT:
				lprc->bottom = lprc->top + MinHeight;
				break;
			}
		}
		if ((lprc->right - lprc->left) <= MinWidth) {
			switch (fwSide) {
			case WMSZ_TOPLEFT:
			case WMSZ_LEFT:
			case WMSZ_BOTTOMLEFT:
				lprc->left = lprc->right - MinWidth;
				break;
			case WMSZ_TOPRIGHT:
			case WMSZ_RIGHT:
			case WMSZ_BOTTOMRIGHT:
				lprc->right = lprc->left + MinWidth;
				break;
			}
		}
	}
	break;
	case WM_SIZE:
	{
		int nWidth = LOWORD(lParam);  // width of client area 
		int nHeight = HIWORD(lParam); // height of client area 
		SetWindowPos(hSelectCheat, HWND_TOP, 5, 5, nWidth - 8, nHeight - 10, SWP_NOOWNERZORDER);
		//StoreCurrentWinSize("Cheat",hWnd);
	}
	break;
	case WM_DESTROY:
		if (IsIconic(hWnd)) { break; }
		StoreCurrentWinPos("Cheat", hWnd);
		StoreCurrentWinSize("Cheat", hWnd);
		hManageWindow = NULL;
		break;
	default:
		return DefWindowProc(hWnd, uMsg, wParam, lParam);
	}
	return TRUE;
}

/********************************************************************************************
  ManageCheatsProc

  Purpose:
  Parameters:
  Returns:

********************************************************************************************/
LRESULT CALLBACK ManageCheatsProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	static int CurrentPanel = SelectCheat;
	static RECT rcDisp;
	RECT clientrect;
	static RECT rcList;
	static RECT rcAdd;
	HANDLE hIcon;
	HWND hStateButton;

	switch (uMsg) {
	case WM_INITDIALOG:
		hManageWindow = hDlg;
		{
			WINDOWPLACEMENT WndPlac;
			RECT* rc;

			WndPlac.length = sizeof(WndPlac);
			GetWindowPlacement(hDlg, &WndPlac);
			rc = &WndPlac.rcNormalPosition;

			SetWindowText(hDlg, GS(CHEAT_TITLE));
			hSelectCheat = CreateDialog(hInst, MAKEINTRESOURCE(IDD_Cheats_List), hDlg, (DLGPROC)CheatListProc);
			SetWindowPos(hSelectCheat, HWND_TOP, 5, 8, 0, 0, SWP_NOSIZE);
			ShowWindow(hSelectCheat, SW_SHOW);

			hAddCheat = CreateDialog(hInst, MAKEINTRESOURCE(IDD_Cheats_Add), hDlg, (DLGPROC)CheatAddProc);
			SetWindowPos(hAddCheat, HWND_TOP, (rc->right - rc->left) / 2, 8, 0, 0, SWP_NOSIZE);
			ShowWindow(hAddCheat, SW_HIDE);

			GetWindowRect(GetDlgItem(hSelectCheat, IDC_CHEATSFRAME), &rcList);
			GetWindowRect(GetDlgItem(hAddCheat, IDC_ADDCHEATSFRAME), &rcAdd);
			MinSizeDlg = rcList.right - rcList.left + 32;
			MaxSizeDlg = rcAdd.right - rcList.left + 32;

			DialogState = CONTRACTED;
			WndPlac.rcNormalPosition.right = WndPlac.rcNormalPosition.left + MinSizeDlg;
			SetWindowPlacement(hDlg, &WndPlac);

			GetClientRect(hDlg, rc);
			hStateButton = GetDlgItem(hDlg, IDC_STATE);
			SetWindowPos(hStateButton, HWND_TOP, (rc->right - rc->left) - 16, 0, 16, rc->bottom - rc->top, 0);
			hIcon = LoadImage(hInst, MAKEINTRESOURCE(IDI_RIGHT), IMAGE_ICON, 0, 0, LR_DEFAULTCOLOR);
			SendDlgItemMessage(hDlg, IDC_STATE, BM_SETIMAGE, (WPARAM)IMAGE_ICON, (LPARAM)(HANDLE)hIcon);
		}
		hManageWindow = hDlg;
		RefreshCheatManager();
		break;
		//case WM_SIZE:
		//	GetClientRect( hDlg, &rcDisp);
		//	TabCtrl_AdjustRect( GetDlgItem(hDlg,IDC_TAB), FALSE, &rcDisp );
		//	break;
		/*case WM_NOTIFY:
			switch (((NMHDR *)lParam)->code) {
			case TCN_SELCHANGE:
				{
					TC_ITEM item;
					HWND hTab;

					hTab = GetDlgItem(hDlg,IDC_TAB);
					InvalidateRect( hTab, &rcDisp, TRUE );
					switch (CurrentPanel) {
					case SelectCheat: ShowWindow(hSelectCheat,SW_HIDE); break;
					case NewCheat: ShowWindow(hAddCheat,SW_HIDE); break;
					}
					item.mask = TCIF_PARAM;
					TabCtrl_GetItem( hTab, TabCtrl_GetCurSel( hTab ), &item );
					CurrentPanel = item.lParam;
					switch (CurrentPanel) {
					case SelectCheat: ShowWindow(hSelectCheat,SW_SHOW); break;
					case NewCheat: ShowWindow(hAddCheat,SW_SHOW); break;
					}
					break;
				}
			}
			break;*/
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDCANCEL:
			EndDialog(hDlg, 0);
			DestroyWindow(hDlg);
			break;
		case IDC_STATE:
		{
			WINDOWPLACEMENT WndPlac;
			WndPlac.length = sizeof(WndPlac);
			GetWindowPlacement(hDlg, &WndPlac);

			if (DialogState == CONTRACTED)
			{
				DialogState = EXPANDED;
				WndPlac.rcNormalPosition.right = WndPlac.rcNormalPosition.left + MaxSizeDlg;
				SetWindowPlacement(hDlg, &WndPlac);

				GetClientRect(hDlg, &clientrect);
				hStateButton = GetDlgItem(hDlg, IDC_STATE);
				SetWindowPos(hStateButton, HWND_TOP, (clientrect.right - clientrect.left) - 16, 0, 16, clientrect.bottom - clientrect.top, 0);

				hIcon = LoadImage(hInst, MAKEINTRESOURCE(IDI_LEFT), IMAGE_ICON, 0, 0, LR_DEFAULTCOLOR);
				SendDlgItemMessage(hDlg, IDC_STATE, BM_SETIMAGE, (WPARAM)IMAGE_ICON, (LPARAM)(HANDLE)hIcon);

				ShowWindow(hAddCheat, SW_SHOW);
			}
			else
			{
				DialogState = CONTRACTED;
				WndPlac.rcNormalPosition.right = WndPlac.rcNormalPosition.left + MinSizeDlg;
				SetWindowPlacement(hDlg, &WndPlac);

				hIcon = LoadImage(hInst, MAKEINTRESOURCE(IDI_RIGHT), IMAGE_ICON, 0, 0, LR_DEFAULTCOLOR);
				SendDlgItemMessage(hDlg, IDC_STATE, BM_SETIMAGE, (WPARAM)IMAGE_ICON, (LPARAM)(HANDLE)hIcon);

				GetClientRect(hDlg, &clientrect);
				hStateButton = GetDlgItem(hDlg, IDC_STATE);
				SetWindowPos(hStateButton, HWND_TOP, (clientrect.right - clientrect.left) - 16, 0, 16, clientrect.bottom - clientrect.top, 0);

				ShowWindow(hAddCheat, SW_HIDE);
			}

		}
		break;
		}
		break;
		// *** Add in Build 53
	case WM_DESTROY:
		hManageWindow = NULL;
		break;
	default:
		return FALSE;
	}
	return TRUE;
}

void AddCodeLayers(int CheatNumber, char* CheatName, HTREEITEM hParent, BOOL CheatActive) {
	char Text[500], Item[500];
	TV_INSERTSTRUCT tv;

	// Work out text to add
	strcpy(Text, CheatName);
	if (strchr(Text, '\\') > 0)
		*strchr(Text, '\\') = 0; 

	// See if text is already added
	tv.item.mask = TVIF_TEXT;
	tv.item.pszText = Item;
	tv.item.cchTextMax = sizeof(Item);
	tv.item.hItem = TreeView_GetChild(hCheatTree, hParent);
	while (tv.item.hItem) {
		TreeView_GetItem(hCheatTree, &tv.item);
		Item[499] = '\0';	// This is already null terminated according to MSDN documentation but Intellisense is complaining about the strcmp below
		if (strcmp(Text, Item) == 0) {
			// If already exists then just use existing one
			int State = _TreeView_GetCheckState(hCheatTree, tv.item.hItem);
			if ((CheatActive && State == TV_STATE_CLEAR) || (!CheatActive && State == TV_STATE_CHECKED)) {
				_TreeView_SetCheckState(hCheatTree, tv.item.hItem, TV_STATE_INDETERMINATE);
			}
			AddCodeLayers(CheatNumber, CheatName + strlen(Text) + 1, tv.item.hItem, CheatActive);
			return;
		}
		tv.item.hItem = TreeView_GetNextSibling(hCheatTree, tv.item.hItem);
	}

	// Add to dialog
	tv.hInsertAfter = TVI_SORT;
	tv.item.mask = TVIF_TEXT | TVIF_PARAM;
	tv.item.pszText = Text;
	tv.item.lParam = CheatNumber;
	tv.hParent = hParent;
	hParent = TreeView_InsertItem(hCheatTree, &tv);
	_TreeView_SetCheckState(hCheatTree, hParent, CheatActive ? TV_STATE_CHECKED : TV_STATE_CLEAR);

	if (strcmp(Text, CheatName) == 0)
		return;
	AddCodeLayers(CheatNumber, CheatName + strlen(Text) + 1, hParent, CheatActive);
}

/********************************************************************************************
  RefreshCheatManager

  Purpose:
  Parameters:
  Returns:

********************************************************************************************/
void RefreshCheatManager(void) {
	char CheatName[500];
	BOOL IsCheatActive;
	DWORD count;
	CHEAT cheat = { 0 };

	if (hManageWindow == NULL)
		return;

	TreeView_DeleteAllItems(hCheatTree);
	for (count = 0; count < MaxCheats; count++) {
		if (!Cheats_Read(&cheat, count)) {
			Cheats_ClearCheat(&cheat);
			continue;
		}

		IsCheatActive = CheatActive(cheat.name);
		Cheats_DisplayName(&cheat, CheatName, sizeof(CheatName));
		AddCodeLayers(count, CheatName, TVI_ROOT, IsCheatActive);
	}
}

void SaveCheat(char* CheatName, BOOL Active) {
	char Identifier[100];

	RomID(Identifier, RomHeader);

	Settings_Write(APPS_NAME, Identifier, CheatName, Active ? STR_TRUE : STR_FALSE);
}

int _TreeView_GetCheckState(HWND hwndTreeView, HTREEITEM hItem)
{
	TVITEM tvItem;

	// Prepare to receive the desired information.
	tvItem.mask = TVIF_HANDLE | TVIF_STATE;
	tvItem.hItem = hItem;
	tvItem.stateMask = TVIS_STATEIMAGEMASK;

	// Request the information.
	TreeView_GetItem(hwndTreeView, &tvItem);

	// Return zero if it's not checked, or nonzero otherwise.
	switch (tvItem.state >> 12) {
	case 1: return TV_STATE_CHECKED;
	case 2: return TV_STATE_CLEAR;
	case 3: return TV_STATE_INDETERMINATE;
	}
	return ((int)(tvItem.state >> 12) - 1);
}

BOOL _TreeView_SetCheckState(HWND hwndTreeView, HTREEITEM hItem, int state)
{
	TVITEM tvItem;

	tvItem.mask = TVIF_HANDLE | TVIF_STATE;
	tvItem.hItem = hItem;
	tvItem.stateMask = TVIS_STATEIMAGEMASK;

	/*Image 1 in the tree-view check box image list is the
	unchecked box. Image 2 is the checked box.*/

	switch (state) {
	case TV_STATE_CHECKED: tvItem.state = INDEXTOSTATEIMAGEMASK(1); break;
	case TV_STATE_CLEAR: tvItem.state = INDEXTOSTATEIMAGEMASK(2); break;
	case TV_STATE_INDETERMINATE: tvItem.state = INDEXTOSTATEIMAGEMASK(3); break;
	default: tvItem.state = INDEXTOSTATEIMAGEMASK(0); break;
	}
	return TreeView_SetItem(hwndTreeView, &tvItem);
}
