/*
 * Project 64 - A Nintendo 64 emulator.
 *
 * (c) Copyright 2001 zilmar (zilmar@emulation64.com) and 
 * Jabo (jabo@emulation64.com).
 *
 * pj64 homepage: www.pj64.net
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
#include <windowsx.h>
#include <commctrl.h>
#include <stdio.h>
#include <winuser.h>
#include "main.h"
#include "CPU.h"
#include "debugger.h"
#include "resource.h"

HWND hMemDumpDlg;

void Show_MemDumpDlg (HWND hParent);
BOOL CALLBACK MemDumpDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

FILE *OpenFileForSaving(HWND hParent);
void DumpHexData (HWND hDlg, DWORD startAddress, DWORD endAddress);
void DumpPCAndDisassembled (HWND hDlg, DWORD startAddress, DWORD endAddress);

const char *ADDRESSRANGE[] = {"RDRAM",
							  "Custom"};
const char *FORMAT[] = {"Hex data",
	                    "PC + disassembled"};

void Show_MemDumpDlg (HWND hParent) {
    if (DialogBox(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_Debugger_MemDump), hParent, MemDumpDlgProc)==IDOK) 
    {
        // Complete the command; szItemName contains the 
        // name of the item to delete. 
    }

    else 
    {
        // Cancel the command. 
    } 
}

BOOL CALLBACK MemDumpDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	int Count;
	char tmp[11];
	DWORD startAddress, endAddress;

	switch (uMsg) 
    {
        case WM_INITDIALOG: 
			for (Count = 0; Count < 2; Count++) {
				SendDlgItemMessage(hDlg, IDC_MD_RANGE, CB_ADDSTRING, 0, (LPARAM)(LPCTSTR) ADDRESSRANGE[Count]);
			}
			SendDlgItemMessage(hDlg, IDC_MD_RANGE, CB_SETCURSEL, (WPARAM) 0, 0); //default selection is RDRAM
			SendDlgItemMessage(hDlg, IDC_MD_RANGESTART, WM_SETTEXT, 0, (LPARAM)(LPCTSTR)"0xA0000000");
			if (RdramSize == 0x00400000) {
				SendDlgItemMessage(hDlg, IDC_MD_RANGEEND, WM_SETTEXT, 0, (LPARAM)(LPCTSTR)"0xA03FFFFF");
			} else {
				SendDlgItemMessage(hDlg, IDC_MD_RANGEEND, WM_SETTEXT, 0, (LPARAM)(LPCTSTR)"0xA07FFFFF");
			}

			for (Count = 0; Count < 2; Count++) {
				SendDlgItemMessage(hDlg, IDC_MD_FORMAT, CB_ADDSTRING, 0, (LPARAM)(LPCTSTR) FORMAT[Count]);
			}
			SendDlgItemMessage(hDlg, IDC_MD_FORMAT, CB_SETCURSEL, (WPARAM) 0, 0); //default selection is HEX DATA
			return TRUE; 

        case WM_COMMAND: 
            switch (LOWORD(wParam)) 
            {
				case IDC_MD_RANGE:
					switch (HIWORD(wParam)) {
						case CBN_SELCHANGE:
							if (SendDlgItemMessage(hDlg, IDC_MD_RANGE, CB_GETCURSEL, (WPARAM) 0, 0) == 0) {
								SendDlgItemMessage(hDlg, IDC_MD_RANGESTART, WM_SETTEXT, 0, (LPARAM)(LPCTSTR)"0xA0000000");
								if (RdramSize == 0x00400000) {
									SendDlgItemMessage(hDlg, IDC_MD_RANGEEND, WM_SETTEXT, 0, (LPARAM)(LPCTSTR)"0xA03FFFFF");
								} else {
									SendDlgItemMessage(hDlg, IDC_MD_RANGEEND, WM_SETTEXT, 0, (LPARAM)(LPCTSTR)"0xA07FFFFF");
								}
							}
							break;
					}
					break;
				
				case IDC_MD_FORMAT:
					switch (HIWORD(wParam)) {
						case CBN_SELCHANGE:
							break;
					}
					break;

				case IDC_MD_RANGESTART:
				case IDC_MD_RANGEEND:
					switch (HIWORD(wParam)) {
						case EN_CHANGE:
							SendDlgItemMessage(hDlg, IDC_MD_RANGESTART, WM_GETTEXT, 11, (LPARAM)tmp);
							startAddress = strtoul(tmp, NULL, 16);
							SendDlgItemMessage(hDlg, IDC_MD_RANGEEND, WM_GETTEXT, 11, (LPARAM)tmp);
							endAddress = strtoul(tmp, NULL, 16);
							if ((startAddress == 0xA0000000) && (endAddress == 0xA0000000+RdramSize-1)) {
								SendDlgItemMessage(hDlg, IDC_MD_RANGE, CB_SETCURSEL, (WPARAM) 0, 0);
							} else {
								SendDlgItemMessage(hDlg, IDC_MD_RANGE, CB_SETCURSEL, (WPARAM) 1, 0);
							}
							break;
					}
					break;
             
				case IDOK: 
					SendDlgItemMessage(hDlg, IDC_MD_RANGESTART, WM_GETTEXT, 11, (LPARAM)tmp);
					startAddress = strtoul(tmp, NULL, 16);
					SendDlgItemMessage(hDlg, IDC_MD_RANGEEND, WM_GETTEXT, 11, (LPARAM)tmp);
					endAddress = strtoul(tmp, NULL, 16);
					if (endAddress <= startAddress) {
						MessageBox(hDlg, "End address need to be larger then start address!", "Error", MB_OK);
						break;
					}
					if ((startAddress > 0xFFFFFFFF) || (endAddress > 0xFFFFFFFF)) {
						MessageBox(hDlg, "Address needs to be between 0x00000000 and 0xFFFFFFFF!", "Error", MB_OK);
						break;
					}
					switch (SendDlgItemMessage(hDlg, IDC_MD_FORMAT, CB_GETCURSEL, (WPARAM) 0, 0)) {
						case 0: //Hex data
							DumpHexData (hDlg, startAddress, endAddress);
							break;

						case 1: // pc + disassembled
							DumpPCAndDisassembled (hDlg, startAddress, endAddress);
							break;
					}
                    // Fall through. 
 
                case IDCANCEL: 
                    EndDialog(hDlg, wParam); 
                    return TRUE; 
            } 
    } 
    return FALSE; 
}

FILE *OpenFileForSaving(HWND hParent) {
	char path_buffer[_MAX_PATH], drive[_MAX_DRIVE], dir[_MAX_DIR], fname[_MAX_FNAME], ext[_MAX_EXT];
	char Directory[255], SaveFile[255];
	OPENFILENAME openfilename;
	FILE * pFile = NULL;

	memset(&SaveFile, 0, sizeof(SaveFile));
	memset(&openfilename, 0, sizeof(openfilename));

	GetModuleFileName(NULL,path_buffer,sizeof(path_buffer));
	_splitpath( path_buffer, drive, dir, fname, ext );
	sprintf(Directory,"%s%s",drive,dir);

	openfilename.lStructSize  = sizeof(openfilename);
	openfilename.hwndOwner    = hParent;
	openfilename.lpstrFilter  = "Text files (*.txt)\0*.txt\0";
	openfilename.lpstrFile    = SaveFile;
	openfilename.lpstrInitialDir    = Directory;
	openfilename.nMaxFile     = MAX_PATH;
	openfilename.Flags        = OFN_HIDEREADONLY;

	if (GetSaveFileName (&openfilename)) {
		_splitpath( SaveFile, drive, dir, fname, ext );
		if (strcmp(ext, ".txt") == -1) {
			_makepath( SaveFile, drive, dir, fname, "txt" );
		}

		pFile = fopen (SaveFile, "w");
	}

	return pFile;
}

void DumpHexData (HWND hDlg, DWORD startAddress, DWORD endAddress) {
	BYTE *buffer;
	long bufferSize, count;
	MIPS_WORD word;
	int i;
	char substring[17];

	FILE * pFile;
	pFile = OpenFileForSaving(hDlg);

	if (pFile != NULL) {
		bufferSize = endAddress - startAddress;

		buffer = (BYTE *)malloc(bufferSize);
		for (count=0; count<bufferSize-4; count+=4) {
			r4300i_LW_VAddr(startAddress+count, (DWORD *)&word);
			buffer[count]=word.UB[3];
			buffer[count+1]=word.UB[2];
			buffer[count+2]=word.UB[1];
			buffer[count+3]=word.UB[0];
		}

		count = 0;
		while (count <= bufferSize) {
			for (i=0; i<16; i++) {
				if ((buffer[count+i] >= 0x20) && (buffer[count+i] <= 0x7F)) { //Printable characters
					substring[i] = buffer[count+i];
				} else {
					substring[i] = '.';
				}
			}
			substring[16] = '\0';

			fprintf(pFile,
				"%08X-%08X   %02X %02X %02X %02X %02X %02X %02X %02X - %02X %02X %02X %02X %02X %02X %02X %02X   %s\n",
				startAddress+count,
				startAddress+count+8,
				buffer[count],
				buffer[count+1],
				buffer[count+2],
				buffer[count+3],
				buffer[count+4],
				buffer[count+5],
				buffer[count+6],
				buffer[count+7],
				buffer[count+8],
				buffer[count+9],
				buffer[count+10],
				buffer[count+11],
				buffer[count+12],
				buffer[count+13],
				buffer[count+14],
				buffer[count+15],
				substring);
			count += 16;
		}

	// terminate
	fclose (pFile);
	free(buffer);
	}
}

void DumpPCAndDisassembled (HWND hDlg, DWORD startAddress, DWORD endAddress) {
	DWORD OpCode, location;
	BOOL validOpCode;

	FILE * pFile;
	pFile = OpenFileForSaving(hDlg);

	if (pFile != NULL) {
		location = startAddress - startAddress % 4;
		while (location < endAddress) {
			validOpCode = TRUE;
			__try {
				if (!r4300i_LW_VAddr(location, &OpCode)) {
					fprintf(pFile, " 0x%08X\tCould not resolve address\n", location);
					validOpCode = FALSE;
				}
			} __except( r4300i_Command_MemoryFilter( GetExceptionCode(), GetExceptionInformation()) ) {
				DisplayError(GS(MSG_UNKNOWN_MEM_ACTION));
				ExitThread(0);
			}	

			if (validOpCode) {
				if (SelfModCheck == ModCode_ChangeMemory) {
					if ( (OpCode >> 16) == 0x7C7C) {
						OpCode = OrigMem[(OpCode & 0xFFFF)].OriginalValue;
					}
				}

				fprintf(pFile, " 0x%08X\t%s\n", location, R4300iOpcodeName(OpCode, location));
			}

			location +=4;
		}

		// terminate
		fclose (pFile);
	}
}
