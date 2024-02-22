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
#include "R4300i Commands.h"
#include "BreakPoints.h"
#include "mapping.h"
#include "Compression/unzip.h"

MAP_ENTRY * MapTable = NULL;
DWORD NoOfMapEntries = 0;
char strLabelName[100];

void GetMapDirectory ( char * Directory );
int  ProcessMapFile ( BYTE * File, DWORD FileLen );
void SetMapDirectory ( char * Directory );

void AddMapEntry ( MIPS_DWORD Address, char * Label) {
	if (Label == NULL) { return; }
	
	if (NoOfMapEntries == 0) {
		NoOfMapEntries = 1;
		MapTable = malloc(sizeof(MAP_ENTRY));
	} else {
		NoOfMapEntries += 1;
		MapTable = realloc(MapTable, sizeof(MAP_ENTRY) * NoOfMapEntries);
	}
	MapTable[NoOfMapEntries - 1].VAddr = Address;
	strcpy(MapTable[NoOfMapEntries - 1].Label, Label);
}

void ChooseMapFile ( HWND hWnd ) {
	OPENFILENAME OpenFileName;
	char Directory[MAX_PATH];
	char MapFileName[MAX_PATH];

	memset(MapFileName, 0, MAX_PATH);
	memset(&OpenFileName, 0, sizeof(OpenFileName));

	GetCurrentDirectory( 255, Directory );
	GetMapDirectory( Directory );

	OpenFileName.lStructSize  = sizeof( OpenFileName );
	OpenFileName.hwndOwner    = hWnd;
	OpenFileName.lpstrFilter  = "Map file(*.map;*.cod)\0*.map;*.cod\0All files (*.*)\0*.*\0\0";
	OpenFileName.lpstrFile    = MapFileName;
	OpenFileName.lpstrInitialDir = Directory;
	OpenFileName.nMaxFile     = MAX_PATH;
	OpenFileName.Flags        = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

	if (GetOpenFileName (&OpenFileName)) {							
		GetCurrentDirectory( 255, Directory );
		SetMapDirectory( Directory );
		ResetMappings ();
		if (!OpenMapFile(MapFileName)) {
			DisplayError("Failed to process %s",MapFileName);
			ResetMappings ();
		}
	}
}

void GetMapDirectory ( char * Directory ) {
	char Dir[255], Group[200];
	long lResult;
	HKEY hKeyResults = 0;

	sprintf(Group,"Software\\N64 Emulation\\%s\\Main",AppName);
	lResult = RegOpenKeyEx( HKEY_CURRENT_USER,Group,0,KEY_ALL_ACCESS,
		&hKeyResults);
	
	if (lResult == ERROR_SUCCESS) {
		DWORD Type, Bytes = sizeof(Dir);
		lResult = RegQueryValueEx(hKeyResults,"Map Directory",0,&Type,(LPBYTE)Dir,&Bytes);
		if (lResult == ERROR_SUCCESS) { strcpy(Directory,Dir); }
	}
	RegCloseKey(hKeyResults);	
}

char * LabelName (MIPS_DWORD Address) {
	DWORD count;

	if (!HaveDebugger) {
		sprintf(strLabelName, "0x%016llX", Address.UDW);
		return strLabelName;
	}
	else {
		for (count = 0; count < NoOfMapEntries; count++) {
			if (MapTable[count].VAddr.UDW == Address.UDW) {
				sprintf(strLabelName, "%s", MapTable[count].Label);
				return strLabelName;
			}
		}
		sprintf(strLabelName, "0x%016llX", Address.UDW);
		return strLabelName;
	}
}

void OpenZipMapFile(char * FileName) {
	int port = 0, FoundMap;
    unz_file_info info;
	char zname[_MAX_PATH];
	unzFile file;

	file = unzOpen(FileName);
	if (file == NULL) {
		return;
	}

	port = unzGoToFirstFile(file);
	FoundMap = FALSE; 
	while(port == UNZ_OK && FoundMap == FALSE) {
		unzGetCurrentFileInfo(file, &info, zname, 128, NULL,0, NULL,0);
	    if (unzLocateFile(file, zname, 1) != UNZ_OK ) {
			unzClose(file);
			return;
		}
		if( unzOpenCurrentFile(file) != UNZ_OK ) {
			unzClose(file);
			return;
		}
		if (strrchr(zname,'.') != NULL) {
			char *p = strrchr(zname,'.');
			if (strcmp(p,".cod") == 0 || strcmp(p,".map") == 0) {
				BYTE * MapFileContents;
				DWORD dwRead;
				HGLOBAL hMem;

				MapFileContents = GlobalAlloc(GPTR,info.uncompressed_size + 1);	

			    if (MapFileContents == NULL) {
					unzCloseCurrentFile(file);
				    unzClose(file);
					DisplayError("Not enough memory for Map File Contents");
					return;
				}

				dwRead = unzReadCurrentFile(file,MapFileContents,info.uncompressed_size);

				unzCloseCurrentFile(file);
			    unzClose(file);
				if (info.uncompressed_size != dwRead) {
					hMem = GlobalHandle (MapFileContents);
					GlobalFree(hMem);
					DisplayError("Failed to copy Map File to memory");
					return;
				}
				
				ProcessMapFile(MapFileContents, info.uncompressed_size);

				hMem = GlobalHandle (MapFileContents);
				GlobalFree(hMem);
				UpdateBP_FunctionList();
				Update_r4300iCommandList();
				return;
			}
		}
		if (FoundMap == FALSE) {
			unzCloseCurrentFile(file);
			port = unzGoToNextFile(file);
		}
	}
    unzClose(file);
}

int OpenMapFile(char * FileName) {
	DWORD RomFileSize, dwRead;
	BYTE * MapFileContents;
	HGLOBAL hMem;
	HANDLE hFile;

	hFile = CreateFile(FileName,GENERIC_READ,FILE_SHARE_READ,NULL,
	OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS,
	NULL);

	if (hFile == INVALID_HANDLE_VALUE) { return FALSE; }
	
	RomFileSize = GetFileSize(hFile,NULL);
	MapFileContents = GlobalAlloc(GPTR,RomFileSize + 1);	

    if (MapFileContents == NULL) {
		DisplayError("Not enough memory for Map File Contents");
		CloseHandle( hFile ); 
		return FALSE;
	}

	SetFilePointer(hFile,0,0,FILE_BEGIN);
	if (!ReadFile(hFile,MapFileContents,RomFileSize,&dwRead,NULL)) {
		DisplayError("Failed to copy Map File to memory");
		CloseHandle( hFile ); 
		return FALSE;
	}

	CloseHandle( hFile ); 

	if (RomFileSize != dwRead) {
		DisplayError("Failed to copy Map File to memory");
		return FALSE;
	}
	
	if (!ProcessMapFile(MapFileContents, RomFileSize)) {
		hMem = GlobalHandle (MapFileContents);
		GlobalFree(hMem);
		return FALSE;
	}

	hMem = GlobalHandle (MapFileContents);
	GlobalFree(hMem);
	UpdateBP_FunctionList();
	Update_r4300iCommandList();
	return TRUE;
}

int ProcessCODFile(BYTE * File, DWORD FileLen) {
	BYTE * CurrentPos = File;
	char Label[40];
	MIPS_DWORD Address;
	int Length;

	while ( CurrentPos < File + FileLen ) {
		int FirstFieldLength = strchr(CurrentPos, ',') - CurrentPos;

		if (FirstFieldLength == 10 || FirstFieldLength == 18) {
			if (*CurrentPos != '0') { return FALSE; }
			CurrentPos += 1;
			if (*CurrentPos != 'x') { return FALSE; }
			CurrentPos += 1;
		}
		
		if (FirstFieldLength == 8 || FirstFieldLength == 10) {
			Address.DW = (int)AsciiToHex(CurrentPos);
			CurrentPos += 9;
		}
		else if (FirstFieldLength == 16 || FirstFieldLength == 18) {
			Address.UDW = AsciiToHex64(CurrentPos);
			CurrentPos += 17;
		}
		else {
			return FALSE;
		}

		if (strchr(CurrentPos,'\r') == NULL) {
			Length = strchr(CurrentPos,'\n') - CurrentPos;
		} else {
			Length = strchr(CurrentPos,'\r') - CurrentPos;
			if (Length > (strchr(CurrentPos,'\n') - CurrentPos)) {
				Length = strchr(CurrentPos,'\n') - CurrentPos;
			}
		}

		if (Length > 40) { Length = 40; }
		memcpy(Label,CurrentPos,Length);
		Label[Length] = '\0';

		AddMapEntry (Address, Label);
		CurrentPos = strchr(CurrentPos,'\n') + 1;
	}
	return TRUE;
}

int ProcessMapFile(BYTE * File, DWORD FileLen) {
	if (ProcessCODFile(File,FileLen)) { return TRUE; }
	return FALSE;
}


void ResetMappings (void) {
	NoOfMapEntries = 0;
	if (MapTable != NULL) { free(MapTable); }
	MapTable = NULL;
	UpdateBP_FunctionList ();
	Update_r4300iCommandList();
}

void SetMapDirectory ( char * Directory ) {
	long lResult;
	HKEY hKeyResults = 0;
	DWORD Disposition = 0;
	char Group[200];

	sprintf(Group,"Software\\N64 Emulation\\%s\\Main",AppName);
	lResult = RegCreateKeyEx( HKEY_CURRENT_USER, Group,0,"",REG_OPTION_NON_VOLATILE, 
		KEY_ALL_ACCESS,NULL,&hKeyResults,&Disposition);
	if (lResult == ERROR_SUCCESS) {
		RegSetValueEx(hKeyResults,"Map Directory",0,REG_SZ,(LPBYTE)Directory,strlen(Directory));
	}
}
