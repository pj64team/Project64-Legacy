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
#include <commctrl.h>
#include <stdio.h>
#include "main.h"
#include "cpu.h"
#include "memory.h"
#include "debugger.h"
#include "plugin.h"
#include "cheats.h"
#include "Compression/unzip.h"
#include "EmulateAI.h"
#include "resource.h"
#include "RomTools_Common.h"
#include "Win32Timer.h"

#define MenuLocOfUsedFiles	12
#define MenuLocOfUsedDirs	(MenuLocOfUsedFiles + 1)

DWORD RomFileSize = 0;
int RomRamSize, RomSaveUsing, RomCPUType, RomSelfMod,
RomUseTlb, RomUseLinking, RomCF, RomUseLargeBuffer, RomUseCache,
RomDelaySI, RomSPHack, RomAudioSignal, RomEmulateAI, RomModVIS;
char CurrentFileName[MAX_PATH + 1] = { "" }, RomName[MAX_PATH + 1] = { "" }, RomFullName[MAX_PATH + 1] = { "" };
char LastRoms[10][MAX_PATH + 1], LastDirs[10][MAX_PATH + 1];
BYTE RomHeader[0x1000];
const DWORD CRC_SRC_SIZE = 0x00101000;
const BYTE ZEROS[4096] = { 0 };
DWORD PrevCRC1, PrevCRC2;
HANDLE hRomFile = NULL;
HANDLE hRomMapping = NULL;

BOOL IsValidRomImage(BYTE Test[4]);

void AddRecentDir(HWND hWnd, char* addition) {
	int count;

	if (addition != NULL && RomDirsToRemember > 0) {
		char Dir[MAX_PATH + 1];
		BOOL bFound = FALSE;

		strcpy(Dir, addition);
		for (count = 0; count < RomDirsToRemember && !bFound; count++) {
			if (strcmp(addition, LastDirs[count]) == 0) {
				if (count != 0) {
					memmove(&LastDirs[1], &LastDirs[0], sizeof(LastDirs[0]) * count);
				}
				bFound = TRUE;
			}
		}

		if (bFound == FALSE) { memmove(&LastDirs[1], &LastDirs[0], sizeof(LastDirs[0]) * (RomDirsToRemember - 1)); }
		strcpy(LastDirs[0], Dir);
		SaveRecentDirs();
	}
	SetupMenu(hMainWindow);
}

void AddRecentFile(HWND hWnd, char* addition) {
	int count;

	if (addition != NULL && RomsToRemember > 0) {
		char Rom[MAX_PATH + 1];
		BOOL bFound = FALSE;

		strcpy(Rom, addition);
		for (count = 0; count < RomsToRemember && !bFound; count++) {
			if (strcmp(addition, LastRoms[count]) == 0) {
				if (count != 0) {
					memmove(&LastRoms[1], &LastRoms[0], sizeof(LastRoms[0]) * count);
				}
				bFound = TRUE;
			}
		}

		if (bFound == FALSE) { memmove(&LastRoms[1], &LastRoms[0], sizeof(LastRoms[0]) * (RomsToRemember - 1)); }
		strcpy(LastRoms[0], Rom);
		SaveRecentFiles();
	}
	SetupMenu(hMainWindow);
}

// Create a temporary file for the unzipped/byte-swapped/patched ROM.
// Expects the `RomFileSize` global to have been initialized correctly.
// Initializes the `ROM` global as a memory-mapped file.
// The temporary file can be closed and deleted with `CloseMappedRomFile()`
BOOL CreateMappedRomFile(void) {
	if (RomFileSize == 0) {
		return FALSE;
	}

	char temp_path[_MAX_PATH + 1] = { 0 };
	char temp_file[_MAX_PATH + 1] = { 0 };

	if (GetTempPath(_MAX_PATH, temp_path) == 0) {
		return FALSE;
	}

	if (GetTempFileName(temp_path, "PJ64", 0, temp_file) == 0) {
		return FALSE;
	}

	// Create the file
	hRomFile = CreateFile(temp_file, GENERIC_READ | GENERIC_WRITE, 0, NULL, CREATE_ALWAYS,
		FILE_ATTRIBUTE_NORMAL | FILE_ATTRIBUTE_TEMPORARY | FILE_FLAG_DELETE_ON_CLOSE | FILE_FLAG_RANDOM_ACCESS, NULL);
	if (hRomFile == INVALID_HANDLE_VALUE) {
		DeleteFile(temp_file);
		return FALSE;
	}

	// Zero out the file
	for (DWORD i = 0; i < RomFileSize; i += sizeof(ZEROS)) {
		DWORD written = 0;
		if (WriteFile(hRomFile, ZEROS, sizeof(ZEROS), &written, NULL) == FALSE || written != sizeof(ZEROS)) {
			CloseHandle(hRomFile);
			return FALSE;
		}
	}

	// Map the file into memory
	hRomMapping = CreateFileMapping(hRomFile, NULL, PAGE_READWRITE, 0, RomFileSize, NULL);
	if (hRomMapping == NULL) {
		CloseHandle(hRomFile);
		return FALSE;
	}

	ROM = MapViewOfFile(hRomMapping, FILE_MAP_WRITE, 0, 0, 0);
	if (ROM == NULL) {
		CloseHandle(hRomMapping);
		CloseHandle(hRomFile);
		return FALSE;
	}

	return TRUE;
}

// Close the temporary ROM file.
// The file will be deleted automatically by the system when closed.
void CloseMappedRomFile(void) {
	if (ROM != NULL && hRomFile != NULL && hRomMapping != NULL) {
		UnmapViewOfFile(ROM);
		CloseHandle(hRomMapping);
		CloseHandle(hRomFile);

		ROM = NULL;
		hRomMapping = NULL;
		hRomFile = NULL;
		RomFileSize = 0;
	}
}

void ByteSwapRom(BYTE* Rom, DWORD RomLen) {
	DWORD count;

	// Comment this out for now, otherwise it causes a hang with the new LoadRomRecalcCRCs
	//SendMessage( hStatusWnd, SB_SETTEXT, 0, (LPARAM)GS(MSG_BYTESWAP) );
	switch (*((DWORD*)&Rom[0])) {
	case 0x12408037:
		for (count = 0; count < RomLen; count += 4) {
			Rom[count] ^= Rom[count + 2];
			Rom[count + 2] ^= Rom[count];
			Rom[count] ^= Rom[count + 2];
			Rom[count + 1] ^= Rom[count + 3];
			Rom[count + 3] ^= Rom[count + 1];
			Rom[count + 1] ^= Rom[count + 3];
		}
		break;
	case 0x40072780:
	case 0x40123780:
		for (count = 0; count < RomLen; count += 4) {
			Rom[count] ^= Rom[count + 3];
			Rom[count + 3] ^= Rom[count];
			Rom[count] ^= Rom[count + 3];
			Rom[count + 1] ^= Rom[count + 2];
			Rom[count + 2] ^= Rom[count + 1];
			Rom[count + 1] ^= Rom[count + 2];
		}
		break;
	case 0x80371240: break;
	//default:
		//DisplayError(GS(MSG_UNKNOWN_FILE_FORMAT));
	}
}

int ChooseN64RomToOpen(void) {
	OPENFILENAME openfilename;
	char FileName[256], Directory[255];

	memset(&FileName, 0, sizeof(FileName));
	memset(&openfilename, 0, sizeof(openfilename));

	Settings_GetDirectory(RomDir, Directory, sizeof(Directory));

	openfilename.lStructSize = sizeof(openfilename);
	openfilename.hwndOwner = hMainWindow;
	openfilename.lpstrFilter = "N64 ROMs (*.zip, *.?64, *.rom, *.usa, *.jap, *.pal, *.bin)\0*.?64;*.zip;*.bin;*.rom;*.usa;*.jap;*.pal\0All files (*.*)\0*.*\0";
	openfilename.lpstrFile = FileName;
	openfilename.lpstrInitialDir = Directory;
	openfilename.nMaxFile = MAX_PATH;
	openfilename.Flags = OFN_FILEMUSTEXIST | OFN_HIDEREADONLY;

	if (GetOpenFileName(&openfilename)) {
		strcpy(CurrentFileName, FileName);
		return TRUE;
	}

	return FALSE;
}

void EnableOpenMenuItems(void) {
	SetupMenu(hMainWindow);
}

BOOL IsValidRomImage(BYTE Test[4]) {
	if (*((DWORD*)&Test[0]) == 0x40123780) { return TRUE; }
	if (*((DWORD*)&Test[0]) == 0x12408037) { return TRUE; }
	if (*((DWORD*)&Test[0]) == 0x80371240) { return TRUE; }
	if (*((DWORD*)&Test[0]) == 0x40072780) { return TRUE; }
	return FALSE;
}

BOOL LoadDataFromRomFile(char* FileName, BYTE* Data, int DataLen, int* RomSize) {
	BYTE Test[4];
	int count;

	if (_strnicmp(&FileName[strlen(FileName) - 4], ".ZIP", 4) == 0) {
		int len, port = 0, FoundRom;
		unz_file_info info;
		char zname[132];
		unzFile file;
		file = unzOpen(FileName);
		if (file == NULL) { return FALSE; }

		port = unzGoToFirstFile(file);
		FoundRom = FALSE;
		while (port == UNZ_OK && FoundRom == FALSE) {
			unzGetCurrentFileInfo(file, &info, zname, 128, NULL, 0, NULL, 0);
			if (unzLocateFile(file, zname, 1) != UNZ_OK) {
				unzClose(file);
				return FALSE;
			}
			if (unzOpenCurrentFile(file) != UNZ_OK) {
				unzClose(file);
				return FALSE;
			}
			unzReadCurrentFile(file, Test, 4);
			if (IsValidRomImage(Test)) {
				FoundRom = TRUE;
				RomFileSize = info.uncompressed_size;
				memcpy(Data, Test, 4);
				len = unzReadCurrentFile(file, &Data[4], DataLen - 4) + 4;

				if ((int)DataLen != len) {
					unzCloseCurrentFile(file);
					unzClose(file);
					return FALSE;
				}
				*RomSize = info.uncompressed_size;
				if (unzCloseCurrentFile(file) == UNZ_CRCERROR) {
					unzClose(file);
					return FALSE;
				}
				unzClose(file);
			}
			if (FoundRom == FALSE) {
				unzCloseCurrentFile(file);
				port = unzGoToNextFile(file);
			}

		}
		if (FoundRom == FALSE) {
			unzClose(file);
			return FALSE;
		}
	}
	else {
		DWORD dwRead;
		HANDLE hFile;

		hFile = CreateFile(FileName, GENERIC_READ, FILE_SHARE_READ, NULL,
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS,
			NULL);

		if (hFile == INVALID_HANDLE_VALUE) { return FALSE; }

		SetFilePointer(hFile, 0, 0, FILE_BEGIN);
		ReadFile(hFile, Test, 4, &dwRead, NULL);
		if (!IsValidRomImage(Test)) { CloseHandle(hFile); return FALSE; }
		RomFileSize = GetFileSize(hFile, NULL);
		SetFilePointer(hFile, 0, 0, FILE_BEGIN);
		if (!ReadFile(hFile, Data, DataLen, &dwRead, NULL)) { CloseHandle(hFile); return FALSE; }
		*RomSize = GetFileSize(hFile, NULL);
		CloseHandle(hFile);
	}

	switch (*((DWORD*)&Data[0])) {
	case 0x12408037:
		for (count = 0; count < DataLen; count += 4) {
			Data[count] ^= Data[count + 2];
			Data[count + 2] ^= Data[count];
			Data[count] ^= Data[count + 2];
			Data[count + 1] ^= Data[count + 3];
			Data[count + 3] ^= Data[count + 1];
			Data[count + 1] ^= Data[count + 3];
		}
		break;
	case 0x40072780:
	case 0x40123780:
		for (count = 0; count < DataLen; count += 4) {
			Data[count] ^= Data[count + 3];
			Data[count + 3] ^= Data[count];
			Data[count] ^= Data[count + 3];
			Data[count + 1] ^= Data[count + 2];
			Data[count + 2] ^= Data[count + 1];
			Data[count + 1] ^= Data[count + 2];
		}
		break;
	case 0x80371240: break;
	}

	// Made a bad assumption here, this function is also called by the rom browser on scan so doing a full check of the crc causes a massive slowdown
	// Only do a check when the CRCs both read 0
	PrevCRC1 = *(DWORD*)&Data[0x10];
	PrevCRC2 = *(DWORD*)&Data[0x14];
	if (*(DWORD*)&Data[0x10] == 0 && *(DWORD*)&Data[0x14] == 0) {
		LoadRomRecalcCRCs(FileName, &Data[0x10], &Data[0x14]);
	}

	return TRUE;
}

void CreateRecentDirList(HMENU hMenu) {
	char String[256], * read;
	int count;
	HMENU hSubMenu;
	MENUITEMINFO menuinfo;

	for (count = 0; count < RomDirsToRemember; count++) {
		sprintf(String, "RecentDir%d", count + 1);
		Settings_Read(APPS_NAME, "Directories", String, "", &read);
		strncpy(LastDirs[count], read, sizeof(LastDirs[count]));
		if (read) free(read);
	}

	hSubMenu = GetSubMenu(hMenu, 0);
	DeleteMenu(hSubMenu, MenuLocOfUsedDirs, MF_BYPOSITION);
	if (BasicMode || !RomListVisible()) { return; }
	memset(&menuinfo, 0, sizeof(MENUITEMINFO));
	menuinfo.cbSize = sizeof(MENUITEMINFO);
	menuinfo.fMask = MIIM_TYPE | MIIM_ID;
	menuinfo.fType = MFT_STRING;
	menuinfo.fState = MFS_ENABLED;
	menuinfo.dwTypeData = String;
	menuinfo.cch = 256;

	sprintf(String, "None");
	InsertMenuItem(hSubMenu, MenuLocOfUsedDirs, TRUE, &menuinfo);
	hSubMenu = CreateMenu();

	if (strlen(LastDirs[0]) == 0) {
		menuinfo.wID = ID_FILE_RECENT_DIR;
		sprintf(String, "None");
		InsertMenuItem(hSubMenu, 0, TRUE, &menuinfo);
	}
	menuinfo.fMask = MIIM_TYPE | MIIM_ID;
	for (count = 0; count < RomDirsToRemember; count++) {
		if (strlen(LastDirs[count]) == 0) { break; }
		menuinfo.wID = ID_FILE_RECENT_DIR + count;

		// Changed  (count + 1) % 10, to  (count + 1), So it no longer forces Rom 10 to Rom 0 position in Recent List, because that makes more sense right? - Icepir8
		
		sprintf(String, "&%d %s", (count + 1), LastDirs[count]);
		InsertMenuItem(hSubMenu, count, TRUE, &menuinfo);
	}
	ModifyMenu(GetSubMenu(hMenu, 0), MenuLocOfUsedDirs, MF_POPUP | MF_BYPOSITION, (DWORD)hSubMenu, GS(MENU_RECENT_DIR));
	if (strlen(LastDirs[0]) == 0 || CPURunning || RomDirsToRemember == 0) {
		EnableMenuItem(GetSubMenu(hMenu, 0), MenuLocOfUsedDirs, MF_BYPOSITION | MFS_DISABLED);
	}
	else {
		EnableMenuItem(GetSubMenu(hMenu, 0), MenuLocOfUsedDirs, MF_BYPOSITION | MFS_ENABLED);
	}
}

void CreateRecentFileList(HMENU hMenu) {
	char String[256], * read;
	int count;

	for (count = 0; count < RomsToRemember; count++) {
		sprintf(String, "RecentFile%d", count + 1);
		Settings_Read(APPS_NAME, "Directories", String, "", &read);
		strncpy(LastRoms[count], read, sizeof(LastRoms[count]));
		if (read) free(read);
	}

	if (BasicMode || !RomListVisible()) {
		HMENU hSubMenu;
		MENUITEMINFO menuinfo;

		hSubMenu = GetSubMenu(hMenu, 0);
		DeleteMenu(hSubMenu, MenuLocOfUsedFiles, MF_BYPOSITION);
		if (strlen(LastRoms[0]) == 0) {
			DeleteMenu(hSubMenu, MenuLocOfUsedFiles, MF_BYPOSITION);
		}

		memset(&menuinfo, 0, sizeof(MENUITEMINFO));
		menuinfo.cbSize = sizeof(MENUITEMINFO);
		menuinfo.fMask = MIIM_TYPE | MIIM_ID;
		menuinfo.fType = MFT_STRING;
		menuinfo.fState = MFS_ENABLED;
		menuinfo.dwTypeData = String;
		menuinfo.cch = 256;
		for (count = 0; count < RomsToRemember; count++) {
			if (strlen(LastRoms[count]) == 0) { break; }
			menuinfo.wID = ID_FILE_RECENT_FILE + count;

			// Changed  (count + 1) % 10, to  (count + 1), So it no longer forces Rom 10 to Rom 0 position in Recent List, because that makes more sense right? - Icepir8

			sprintf(String, "&%d %s", (count + 1), LastRoms[count]);
			InsertMenuItem(hSubMenu, MenuLocOfUsedFiles + count, TRUE, &menuinfo);
		}
	}
	else {
		HMENU hSubMenu;
		MENUITEMINFO menuinfo;

		memset(&menuinfo, 0, sizeof(MENUITEMINFO));
		menuinfo.cbSize = sizeof(MENUITEMINFO);
		menuinfo.fMask = MIIM_TYPE | MIIM_ID;
		menuinfo.fType = MFT_STRING;
		menuinfo.fState = MFS_ENABLED;
		menuinfo.dwTypeData = String;
		menuinfo.cch = 256;

		hSubMenu = GetSubMenu(hMenu, 0);
		DeleteMenu(hSubMenu, MenuLocOfUsedFiles, MF_BYPOSITION);
		sprintf(String, "None");
		InsertMenuItem(hSubMenu, MenuLocOfUsedFiles, TRUE, &menuinfo);
		hSubMenu = CreateMenu();

		if (strlen(LastRoms[0]) == 0) {
			menuinfo.wID = ID_FILE_RECENT_DIR;
			sprintf(String, "None");
			InsertMenuItem(hSubMenu, 0, TRUE, &menuinfo);
		}
		menuinfo.fMask = MIIM_TYPE | MIIM_ID;
		for (count = 0; count < RomsToRemember; count++) {
			if (strlen(LastRoms[count]) == 0) { break; }
			menuinfo.wID = ID_FILE_RECENT_FILE + count;

			// Changed  (count + 1) % 10, to  (count + 1), So it no longer forces Rom 10 to Rom 0 position in Recent List, because that makes more sense right? - Icepir8

			sprintf(String, "&%d %s", (count + 1), LastRoms[count]);
			InsertMenuItem(hSubMenu, count, TRUE, &menuinfo);
		}
		ModifyMenu(GetSubMenu(hMenu, 0), MenuLocOfUsedFiles, MF_POPUP | MF_BYPOSITION, (DWORD)hSubMenu, GS(MENU_RECENT_ROM));
		if (strlen(LastRoms[0]) == 0) {
			EnableMenuItem(GetSubMenu(hMenu, 0), MenuLocOfUsedFiles, MF_BYPOSITION | MFS_DISABLED);
		}
		else {
			EnableMenuItem(GetSubMenu(hMenu, 0), MenuLocOfUsedFiles, MF_BYPOSITION | MFS_ENABLED);
		}
	}
}

void LoadRecentRom(DWORD Index) {
	DWORD ThreadID;

	Index -= ID_FILE_RECENT_FILE;
	if (Index < 0 || Index >(DWORD)RomsToRemember) { return; }
	strcpy(CurrentFileName, LastRoms[Index]);
	CreateThread(NULL, 0, OpenChosenFile, NULL, 0, &ThreadID);
}


BOOL LoadRomHeader(void) {
	char drive[_MAX_DRIVE], FileName[_MAX_DIR], dir[_MAX_DIR], ext[_MAX_EXT];
	BYTE Test[4];

	if (_strnicmp(&CurrentFileName[strlen(CurrentFileName) - 4], ".ZIP", 4) == 0) {
		int port = 0, FoundRom;
		unz_file_info info;
		char zname[132];
		unzFile file;
		file = unzOpen(CurrentFileName);
		if (file == NULL) {
			DisplayError(GS(MSG_FAIL_OPEN_ZIP));
			return FALSE;
		}

		port = unzGoToFirstFile(file);
		FoundRom = FALSE;
		while (port == UNZ_OK && FoundRom == FALSE) {
			unzGetCurrentFileInfo(file, &info, zname, 128, NULL, 0, NULL, 0);
			if (unzLocateFile(file, zname, 1) != UNZ_OK) {
				unzClose(file);
				DisplayError(GS(MSG_FAIL_ZIP));
				return FALSE;
			}
			if (unzOpenCurrentFile(file) != UNZ_OK) {
				unzClose(file);
				DisplayError(GS(MSG_FAIL_OPEN_ZIP));
				return FALSE;
			}
			unzReadCurrentFile(file, Test, 4);
			if (IsValidRomImage(Test)) {
				RomFileSize = info.uncompressed_size;
				FoundRom = TRUE;
				memcpy(RomHeader, Test, 4);
				unzReadCurrentFile(file, &RomHeader[4], sizeof(RomHeader) - 4);
				if (unzCloseCurrentFile(file) == UNZ_CRCERROR) {
					unzClose(file);
					DisplayError(GS(MSG_FAIL_OPEN_ZIP));
					return FALSE;
				}
				_splitpath(CurrentFileName, drive, dir, FileName, ext);
				unzClose(file);
			}
			if (FoundRom == FALSE) {
				unzCloseCurrentFile(file);
				port = unzGoToNextFile(file);
			}
		}
		if (FoundRom == FALSE) {
			DisplayError(GS(MSG_FAIL_OPEN_ZIP));
			unzClose(file);
			return FALSE;
		}
	}
	else {
		DWORD dwRead;
		HANDLE hFile;

		hFile = CreateFile(CurrentFileName, GENERIC_READ, FILE_SHARE_READ, NULL,
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS,
			NULL);

		if (hFile == INVALID_HANDLE_VALUE) {
			SendMessage(hStatusWnd, SB_SETTEXT, 0, (LPARAM)"");
			DisplayError(GS(MSG_FAIL_OPEN_IMAGE));
			return FALSE;
		}

		SetFilePointer(hFile, 0, 0, FILE_BEGIN);
		ReadFile(hFile, Test, 4, &dwRead, NULL);
		if (!IsValidRomImage(Test)) {
			CloseHandle(hFile);
			SendMessage(hStatusWnd, SB_SETTEXT, 0, (LPARAM)"");
			DisplayError(GS(MSG_FAIL_IMAGE));
			return FALSE;
		}
		SetFilePointer(hFile, 0, 0, FILE_BEGIN);
		ReadFile(hFile, RomHeader, sizeof(RomHeader), &dwRead, NULL);
		RomFileSize = GetFileSize(hFile, NULL);	// Read rom size for uncompressed roms
		CloseHandle(hFile);
	}
	ByteSwapRom(RomHeader, sizeof(RomHeader));
	GetRomName(RomName, RomHeader);
	if (strlen(RomName) == 0)
		_splitpath(CurrentFileName, NULL, NULL, RomName, NULL);
	GetRomFullName(RomFullName, RomHeader, CurrentFileName);

	// Bad CRCs, we are forced to recalculate (This means loading the entire rom into memory)
	PrevCRC1 = *(DWORD*)&RomHeader[0x10];
	PrevCRC2 = *(DWORD*)&RomHeader[0x14];
	if (*(DWORD*)&RomHeader[0x10] == 0 || *(DWORD*)&RomHeader[0x14] == 0)
		LoadRomRecalcCRCs(CurrentFileName, &RomHeader[0x10], &RomHeader[0x14]);

	return TRUE;
}

void LoadRomOptions(void) {
	DWORD NewRamSize;
	byte Country[2] = { 0,0 };

	ReadRomOptions();

	NewRamSize = RomRamSize;
	if ((int)RomRamSize < 0) { NewRamSize = SystemRdramSize; }

	if (RomUseLargeBuffer) {
		if (VirtualAlloc(RecompCode, LargeCompileBufferSize, MEM_COMMIT, PAGE_EXECUTE_READWRITE) == NULL) {
			DisplayError(GS(MSG_MEM_ALLOC_ERROR));
			ExitThread(0);
		}
	}
	else {
		VirtualFree(RecompCode, LargeCompileBufferSize, MEM_DECOMMIT);
		if (VirtualAlloc(RecompCode, NormalCompileBufferSize, MEM_COMMIT, PAGE_EXECUTE_READWRITE) == NULL) {
			DisplayError(GS(MSG_MEM_ALLOC_ERROR));
			ExitThread(0);
		}
	}
	if (NewRamSize != RdramSize) {
		if (RdramSize == 0x400000) {
			if (VirtualAlloc(N64MEM + 0x400000, 0x400000, MEM_COMMIT, PAGE_READWRITE) == NULL) {
				DisplayError(GS(MSG_MEM_ALLOC_ERROR));
				ExitThread(0);
			}
			if (VirtualAlloc((BYTE*)JumpTable + 0x400000, 0x400000, MEM_COMMIT, PAGE_READWRITE) == NULL) {
				DisplayError(GS(MSG_MEM_ALLOC_ERROR));
				ExitThread(0);
			}
			if (VirtualAlloc((BYTE*)DelaySlotTable + (0x400000 >> 0xA), (0x400000 >> 0xA), MEM_COMMIT, PAGE_READWRITE) == NULL) {
				DisplayError(GS(MSG_MEM_ALLOC_ERROR));
				ExitThread(0);
			}
		}
		else {
			VirtualFree(N64MEM + 0x400000, 0x400000, MEM_DECOMMIT);
			VirtualFree((BYTE*)JumpTable + 0x400000, 0x400000, MEM_DECOMMIT);
			VirtualFree((BYTE*)DelaySlotTable + (0x400000 >> 0xA), (0x400000 >> 0xA), MEM_DECOMMIT);
		}
	}
	DWORD CRC1 = *(DWORD*)&ROM[0x10];
	DWORD CRC2 = *(DWORD*)&ROM[0x14];
	RdramSize = NewRamSize;
	CPU_Type = SystemCPU_Type;
	if (HaveDebugger) { CPU_Type = CPU_Interpreter; }
	else if (CPU_Type != CPU_SyncCores && RomCPUType != CPU_Default && !(CRC1 == 0 && CRC2 == 0)) { CPU_Type = RomCPUType; }
	CountPerOp = RomCF;
	if (CountPerOp < 1 || CountPerOp > 6)
		CountPerOp = Default_CountPerOp;

	SaveUsing = RomSaveUsing;
	SelfModCheck = SystemSelfModCheck;
	if (RomSelfMod != ModCode_Default) { SelfModCheck = RomSelfMod; }
	UseTlb = RomUseTlb;
	DelaySI = RomDelaySI;
	EmulateAI = RomEmulateAI;
	AudioSignal = RomAudioSignal;
	SPHack = RomSPHack;
	UseLinking = SystemABL;
	DisableRegCaching = !RomUseCache;
	if (RomUseLinking == 0) { UseLinking = TRUE; }
	if (RomUseLinking == 1) { UseLinking = FALSE; }
	ModVI = RomModVIS;

	switch (GetRomRegion(ROM)) {
	case PAL_Region:
		CPOAdjust = 0;
		EmuAI_SetFrameRate(49);
		Timer_Initialize((double)50); break;
	case NTSC_Region:
	default:
		GetRomCountry(&Country, ROM);
		if (Country != "J")
		{
			CPOAdjust = -1;
		}
		else
		{
			CPOAdjust = -2;
		}

		EmuAI_SetFrameRate(60);
		Timer_Initialize((double)60); break;
	}
}

void RemoveRecentDirList(HWND hWnd) {
	HMENU hMenu;
	int count;

	hMenu = GetMenu(hWnd);
	for (count = 0; count < RomDirsToRemember; count++) {
		DeleteMenu(hMenu, ID_FILE_RECENT_DIR + count, MF_BYCOMMAND);
	}
	memset(LastRoms[0], 0, sizeof(LastRoms[0]));
}

void RemoveRecentList(HWND hWnd) {
	HMENU hMenu;
	int count;

	hMenu = GetMenu(hWnd);
	for (count = 0; count < RomsToRemember; count++) {
		DeleteMenu(hMenu, ID_FILE_RECENT_FILE + count, MF_BYCOMMAND);
	}
	memset(LastRoms[0], 0, sizeof(LastRoms[0]));
}

void ReadRomOptions(void) {
	RomRamSize = 0x800000;
	RomSaveUsing = Auto;
	RomCF = -1;
	RomCPUType = CPU_Interpreter; //Changed CPU_Default to CPU_Interpreter so Unknown/Prototypes or Rom Hacks not in RDS can boot for further settings, where Default did nothing. (Gent)
	RomSelfMod = ModCode_Default;
	RomUseTlb = TRUE;
	RomDelaySI = FALSE;
	RomAudioSignal = FALSE;
	RomSPHack = FALSE;
	RomUseCache = TRUE;
	RomUseLargeBuffer = FALSE;
	RomUseLinking = -1;
	RomEmulateAI = FALSE;
	RomModVIS = 1500;

	if (strlen(RomFullName) != 0) {
		char Identifier[100], * String = NULL;

		LoadRomRecalcCRCs(CurrentFileName, (DWORD*)&RomHeader[0x10], (DWORD*)&RomHeader[0x14]);

		RomID(Identifier, RomHeader);

		if (!Settings_EntryExists(RDS_NAME, Identifier))
			return;

		RomCF = Settings_ReadInt(RDS_NAME, Identifier, "Counter Factor", -1);
		if (RomCF > 6 || RomCF < 1)
			RomCF = -1;

		Settings_Read(RDS_NAME, Identifier, "Save Type", "", &String);
		if (strcmp(String, "4kbit Eeprom") == 0) { RomSaveUsing = Eeprom_4K; }
		else if (strcmp(String, "16kbit Eeprom") == 0) { RomSaveUsing = Eeprom_16K; }
		else if (strcmp(String, "Sram") == 0) { RomSaveUsing = Sram; }
		else if (strcmp(String, "FlashRam") == 0) { RomSaveUsing = FlashRam; }
		else { RomSaveUsing = Auto; }
		if (String) free(String);
		
		Settings_Read(RDS_NAME, Identifier, STR_CPUTYPE, "", &String);
		if (strcmp(String, "Interpreter") == 0) { RomCPUType = CPU_Interpreter; }
		else if (strcmp(String, "Recompiler") == 0) { RomCPUType = CPU_Recompiler; }
		else if (strcmp(String, "SyncCores") == 0) { RomCPUType = CPU_SyncCores; }
		else { RomCPUType = CPU_Default; }
		if (String) free(String);
		
		Settings_Read(RDS_NAME, Identifier, "Self-modifying code Method", "", &String);
		if (strcmp(String, "None") == 0) { RomSelfMod = ModCode_None; }
		else if (strcmp(String, "Cache") == 0) { RomSelfMod = ModCode_Cache; }
		else if (strcmp(String, "Protected Memory") == 0) { RomSelfMod = ModCode_ProtectedMemory; }
		else if (strcmp(String, "Check Memory") == 0) { RomSelfMod = ModCode_CheckMemoryCache; }
		else if (strcmp(String, "Check Memory & cache") == 0) { RomSelfMod = ModCode_CheckMemoryCache; }
		else if (strcmp(String, "Check Memory Advance") == 0) { RomSelfMod = ModCode_CheckMemory2; }
		else if (strcmp(String, "Change Memory") == 0) { RomSelfMod = ModCode_ChangeMemory; }
		else { RomSelfMod = ModCode_Default; }
		if (String) free(String);

		Settings_Read(RDS_NAME, Identifier, "Linking", "", &String);
		if (strcmp(String, "On") == 0) { RomUseLinking = 0; }
		if (strcmp(String, "Off") == 0) { RomUseLinking = 1; }
		if (String) free(String);

		if (Settings_HasSetting(RDS_NAME, Identifier, "ExpansionPak"))
			RomRamSize = 0x800000;
		else
			RomRamSize = 0x400000;

		RomUseTlb = !Settings_HasSetting(RDS_NAME, Identifier, "Disable TLB");
		RomDelaySI = Settings_HasSetting(RDS_NAME, Identifier, "Delay SI");
		RomAudioSignal = Settings_HasSetting(RDS_NAME, Identifier, "Audio Signal");
		RomSPHack = Settings_HasSetting(RDS_NAME, Identifier, "SP Hack");
		RomUseCache = !Settings_HasSetting(RDS_NAME, Identifier, "Disable Reg Cache");
		RomUseLargeBuffer = Settings_HasSetting(RDS_NAME, Identifier, "Use Large Buffer");
		RomEmulateAI = Settings_HasSetting(RDS_NAME, Identifier, "Emulate AI");
		RomModVIS = Settings_ReadInt(RDS_NAME, Identifier, "VI", RomModVIS);
		// To do!
		// Provide a defined way of setting a minimum and maximum value for VI, using 500 and 4500 for now (3x lower and 3x higher)
		if (RomModVIS > 4500 || RomModVIS < 500)
			RomModVIS = 1500;
	}
}

void OpenN64Image(void) {
	DWORD ThreadID;

	SendMessage(hStatusWnd, SB_SETTEXT, 0, (LPARAM)GS(MSG_CHOOSE_IMAGE));
	if (ChooseN64RomToOpen()) {
		CreateThread(NULL, 0, OpenChosenFile, NULL, 0, &ThreadID);
	}
	else {
		EnableOpenMenuItems();
	}
}

void SetNewFileDirectory(void) {
	char Directory[255], CurrentDir[255], drive[_MAX_DRIVE], dir[_MAX_DIR], * String = NULL;

	Settings_Read(APPS_NAME, "Directories", "Use Default Rom", STR_TRUE, &String);

	if (strcmp(String, STR_FALSE) == 0) {
		free(String);
		return;
	}
	if (String) free(String);

	_splitpath(CurrentFileName, drive, dir, NULL, NULL);
	sprintf(Directory, "%s%s", drive, dir);

	Settings_GetDirectory(RomDir, CurrentDir, sizeof(CurrentDir));
	if (strcmp(CurrentDir, Directory) == 0) { return; }
	SetRomDirectory(Directory);
	RefreshRomBrowser();
}

DWORD WINAPI OpenChosenFile(LPVOID lpArgs) {
	char drive[_MAX_DRIVE], FileName[_MAX_DIR], dir[_MAX_DIR], ext[_MAX_EXT];
	char WinTitle[300], MapFile[_MAX_PATH];
	char Message[100];
	BYTE Test[4];
	int count;

	if (!PluginsInitilized) {
		DisplayError(GS(MSG_PLUGIN_NOT_INIT));
		return 0;
	}
	EnableMenuItem(hMainMenu, ID_FILE_OPEN_ROM, MFS_DISABLED | MF_BYCOMMAND);
	for (count = 0; count < (int)RomsToRemember; count++) {
		if (strlen(LastRoms[count]) == 0) { break; }
		EnableMenuItem(hMainMenu, ID_FILE_RECENT_FILE + count, MFS_DISABLED | MF_BYCOMMAND);
	}
	HideRomBrowser();
	CloseCheatWindow();

	{
		HMENU hMenu = GetMenu(hMainWindow);
		for (count = 0; count < 10; count++) {
			EnableMenuItem(hMenu, count, MFS_DISABLED | MF_BYPOSITION);
		}
		DrawMenuBar(hMainWindow);
	}
	Sleep(100); // Adjusted down to 100ms from 1000ms
	CloseCpu();
	if (HaveDebugger)
		ResetMappings();
	SetNewFileDirectory();
	strcpy(MapFile, CurrentFileName);

	SendMessage(hStatusWnd, SB_SETTEXT, 0, (LPARAM)GS(MSG_LOADING));

	if (_strnicmp(&CurrentFileName[strlen(CurrentFileName) - 4], ".ZIP", 4) == 0) {
		int len, port = 0, FoundRom;
		unz_file_info info;
		char zname[132];
		unzFile file;
		file = unzOpen(CurrentFileName);
		if (file == NULL) {
			SendMessage(hStatusWnd, SB_SETTEXT, 0, (LPARAM)"");
			DisplayError(GS(MSG_FAIL_OPEN_ZIP));
			EnableOpenMenuItems();
			ShowRomList(hMainWindow);
			return 0;
		}

		port = unzGoToFirstFile(file);
		FoundRom = FALSE;
		while (port == UNZ_OK && FoundRom == FALSE) {
			unzGetCurrentFileInfo(file, &info, zname, 128, NULL, 0, NULL, 0);
			if (unzLocateFile(file, zname, 1) != UNZ_OK) {
				SendMessage(hStatusWnd, SB_SETTEXT, 0, (LPARAM)"");
				unzClose(file);
				DisplayError(GS(MSG_FAIL_ZIP));
				EnableOpenMenuItems();
				ShowRomList(hMainWindow);
				return 0;
			}
			if (unzOpenCurrentFile(file) != UNZ_OK) {
				SendMessage(hStatusWnd, SB_SETTEXT, 0, (LPARAM)"");
				unzClose(file);
				DisplayError(GS(MSG_FAIL_OPEN_ZIP));
				EnableOpenMenuItems();
				ShowRomList(hMainWindow);
				return 0;
			}
			unzReadCurrentFile(file, Test, 4);
			if (IsValidRomImage(Test)) {
				FoundRom = TRUE;

				// Create a temporary file for memory-mapping the ROM
				RomFileSize = info.uncompressed_size;
				if (CreateMappedRomFile() == FALSE) {
					SendMessage(hStatusWnd, SB_SETTEXT, 0, (LPARAM)"");
					unzCloseCurrentFile(file);
					unzClose(file);
					DisplayError(GS(MSG_FAIL_CREATE_TEMP));
					EnableOpenMenuItems();
					ShowRomList(hMainWindow);
					return 0;
				}

				memcpy(ROM, Test, 4);
				len = unzReadCurrentFile(file, &ROM[4], RomFileSize - 4) + 4;
				if ((int)RomFileSize != len) {
					SendMessage(hStatusWnd, SB_SETTEXT, 0, (LPARAM)"");
					CloseMappedRomFile();
					unzCloseCurrentFile(file);
					unzClose(file);
					switch (len) {
					case UNZ_ERRNO:
					case UNZ_EOF:
					case UNZ_PARAMERROR:
					case UNZ_BADZIPFILE:
					case UNZ_INTERNALERROR:
					case UNZ_CRCERROR:
						DisplayError(GS(MSG_FAIL_OPEN_ZIP));
						break;
					}
					EnableOpenMenuItems();
					ShowRomList(hMainWindow);
					return 0;
				}
				if (unzCloseCurrentFile(file) == UNZ_CRCERROR) {
					SendMessage(hStatusWnd, SB_SETTEXT, 0, (LPARAM)"");
					CloseMappedRomFile();
					unzClose(file);
					DisplayError(GS(MSG_FAIL_OPEN_ZIP));
					EnableOpenMenuItems();
					ShowRomList(hMainWindow);
					return 0;
				}
				AddRecentFile(hMainWindow, CurrentFileName);
				_splitpath(CurrentFileName, drive, dir, FileName, ext);
				unzClose(file);
			}
			if (FoundRom == FALSE) {
				unzCloseCurrentFile(file);
				port = unzGoToNextFile(file);
			}
		}
		if (FoundRom == FALSE) {
			SendMessage(hStatusWnd, SB_SETTEXT, 0, (LPARAM)"");
			DisplayError(GS(MSG_FAIL_OPEN_ZIP));
			unzClose(file);
			EnableOpenMenuItems();
			ShowRomList(hMainWindow);
			return 0;
		}
		if (HaveDebugger && AutoLoadMapFile) {
			OpenZipMapFile(MapFile);
		}
	}
	else {
		DWORD dwRead;
		HANDLE hFile;

		hFile = CreateFile(CurrentFileName, GENERIC_READ, FILE_SHARE_READ, NULL,
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
			NULL);

		if (hFile == INVALID_HANDLE_VALUE) {
			SendMessage(hStatusWnd, SB_SETTEXT, 0, (LPARAM)"");
			DisplayError(GS(MSG_FAIL_OPEN_IMAGE));
			EnableOpenMenuItems();
			ShowRomList(hMainWindow);
			return 0;
		}

		SetFilePointer(hFile, 0, 0, FILE_BEGIN);
		if (!ReadFile(hFile, Test, 4, &dwRead, NULL) || !IsValidRomImage(Test)) {
			CloseHandle(hFile);
			SendMessage(hStatusWnd, SB_SETTEXT, 0, (LPARAM)"");
			DisplayError(GS(MSG_FAIL_IMAGE));
			EnableOpenMenuItems();
			ShowRomList(hMainWindow);
			return 0;
		}

		// Create a temporary file for memory-mapping the ROM
		RomFileSize = GetFileSize(hFile, NULL);
		if (CreateMappedRomFile() == FALSE) {
			SendMessage(hStatusWnd, SB_SETTEXT, 0, (LPARAM)"");
			DisplayError(GS(MSG_FAIL_CREATE_TEMP));
			EnableOpenMenuItems();
			ShowRomList(hMainWindow);
			return 0;
		}

		SetFilePointer(hFile, 0, 0, FILE_BEGIN);

		if (!ReadFile(hFile, ROM, RomFileSize, &dwRead, NULL) || RomFileSize != dwRead) {
			CloseMappedRomFile();
			CloseHandle(hFile);
			SendMessage(hStatusWnd, SB_SETTEXT, 0, (LPARAM)"");
			DisplayError(GS(MSG_FAIL_OPEN_IMAGE));
			EnableOpenMenuItems();
			ShowRomList(hMainWindow);
			return 0;
		}
		CloseHandle(hFile);
		AddRecentFile(hMainWindow, CurrentFileName);
		_splitpath(CurrentFileName, drive, dir, FileName, ext);
	}
	ByteSwapRom(ROM, RomFileSize);
	memcpy(RomHeader, ROM, sizeof(RomHeader));
	RecalculateCRCs(ROM, RomFileSize);

	// Blind copy of the recalculated CRC1 and CRC2, no checks needed really
	*(DWORD*)&RomHeader[0x10] = *(DWORD*)&ROM[0x10];
	*(DWORD*)&RomHeader[0x14] = *(DWORD*)&ROM[0x14];

	// Bypass hardware checks on ALECK64
	if (GetCicChipID(ROM) == CIC_NUS_8401) {
		ROM[0x67c] = 0;
		ROM[0x67d] = 0;
		ROM[0x67e] = 0;
		ROM[0x67f] = 0;
	}

	SendMessage(hStatusWnd, SB_SETTEXT, 0, (LPARAM)"");
	if (HaveDebugger && AutoLoadMapFile) {
		char* p;

		p = strrchr(MapFile, '.');
		if (p != NULL) {
			*p = '\0';
		}
		strcat(MapFile, ".cod");
		if (OpenMapFile(MapFile)) {
			p = strrchr(MapFile, '.');
			if (p != NULL) {
				*p = '\0';
			}
			strcat(MapFile, ".map");
			OpenMapFile(MapFile);
		}
	}

	GetRomName(RomName, ROM);
	if (strlen(RomName) == 0)
		strcpy(RomName, FileName);

	GetRomFullName(RomFullName, ROM, FileName);
	sprintf(WinTitle, "%s - %s", RomFullName, AppName);

	for (count = 0; count < (int)strlen(RomName); count++) {
		switch (RomName[count]) {
		case '/':
		case '\\':
			RomName[count] = '-';
			break;
		case ':':
			RomName[count] = ';';
			break;
		}
	}
	SetWindowText(hMainWindow, WinTitle);

	if (!RememberCheats) { DisableAllCheats(); }
	EnableOpenMenuItems();
	SetCurrentSaveState(hMainWindow, ID_CURRENTSAVE_DEFAULT);
	sprintf(WinTitle, "%s - [ %s ]", GS(MSG_LOADED), FileName);
	SendMessage(hStatusWnd, SB_SETTEXT, 0, (LPARAM)WinTitle);
	if (AutoStart) {
		StartEmulation();
		Sleep(100);
		if (AutoFullScreen) {
			char Status[100], Identifier[100], result[100], * read;

			RomID(Identifier, RomHeader);

			Settings_Read(RDS_NAME, Identifier, "Status", Default_RomStatus, &read);
			strncpy(Status, read, sizeof(Status));
			if (read) free(read);

			strcat(Status, ".AutoFullScreen");
			Settings_Read(RDS_NAME, Identifier, Status, STR_TRUE, &read);
			strncpy(result, read, sizeof(result));
			if (read) free(read);
			if (strcmp(result, STR_TRUE) == 0) {
				SendMessage(hMainWindow, WM_COMMAND, ID_OPTIONS_FULLSCREEN, 0);
			}
		}
	}
	return 0;
}

void SaveRecentDirs(void) {
	char String[200];
	int count;

	for (count = 0; count < RomDirsToRemember; count++) {
		if (strlen(LastDirs[count]) == 0)
			break;
		sprintf(String, "RecentDir%d", count + 1);
		Settings_Write(APPS_NAME, "Directories", String, LastDirs[count]);
	}
}

void SaveRecentFiles(void) {
	char String[200];
	int count;

	for (count = 0; count < RomsToRemember; count++) {
		if (strlen(LastRoms[count]) == 0)
			break;
		sprintf(String, "RecentFile%d", count + 1);
		Settings_Write(APPS_NAME, "Directories", String, LastRoms[count]);
	}
}

// Note!!! Due to changes any setting not being explicitly written must be deleted!!!
void SaveRomOptions(void) {
	char Identifier[100], String[100];

	if (strlen(RomName) == 0)
		return;

	RomID(Identifier, RomHeader);

	// Internal Rom Name
	Settings_Write(RDS_NAME, Identifier, "Name", RomFullName);

	// Expansion Pak
	if (RomRamSize == 0x800000)
		Settings_Write(RDS_NAME, Identifier, "ExpansionPak", "");
	else
		Settings_Delete(RDS_NAME, Identifier, "ExpansionPak");

	// Counter Factor
	if (RomCF <= 6 && RomCF >= 1)
		sprintf(String, "%d", RomCF);
	else
		sprintf(String, "Default");
	Settings_Write(RDS_NAME, Identifier, "Counter Factor", String);

	// Save Type
	switch (RomSaveUsing) {
	case Eeprom_4K:
		sprintf(String, "4kbit Eeprom"); break;
	case Eeprom_16K:
		sprintf(String, "16kbit Eeprom"); break;
	case Sram:
		sprintf(String, "Sram"); break;
	case FlashRam:
		sprintf(String, "FlashRam"); break;
	default:
		sprintf(String, "First Save Type"); break;
	}
	Settings_Write(RDS_NAME, Identifier, "Save Type", String);

	// Cpu Type
	switch (RomCPUType) {
	case CPU_Interpreter:
		sprintf(String, "Interpreter"); break;
	case CPU_Recompiler:
		sprintf(String, "Recompiler"); break;
	case CPU_SyncCores:
		sprintf(String, "SyncCores"); break;
	default:
		sprintf(String, "Default"); break;
	}
	Settings_Write(RDS_NAME, Identifier, STR_CPUTYPE, String);

	// Recompiler self code modification
	switch (RomSelfMod) {
	case ModCode_None:
		sprintf(String, "None"); break;
	case ModCode_Cache:
		sprintf(String, "Cache"); break;
	case ModCode_ProtectedMemory:
		sprintf(String, "Protected Memory"); break;
	case ModCode_CheckMemoryCache:
		sprintf(String, "Check Memory & cache"); break;
	case ModCode_CheckMemory2:
		sprintf(String, "Check Memory Advance"); break;
	case ModCode_ChangeMemory:
		sprintf(String, "Change Memory"); break;
	default:
		sprintf(String, "Default"); break;
	}
	Settings_Write(RDS_NAME, Identifier, "Self-modifying code Method", String);

	if (!RomUseCache)	Settings_Write(RDS_NAME, Identifier, "Disable Reg Cache", "");
	else				Settings_Delete(RDS_NAME, Identifier, "Disable Reg Cache");
	if (!RomUseTlb)		Settings_Write(RDS_NAME, Identifier, "Disable TLB", "");
	else				Settings_Delete(RDS_NAME, Identifier, "Disable TLB");
	if (RomDelaySI)		Settings_Write(RDS_NAME, Identifier, "Delay SI", "");
	else				Settings_Delete(RDS_NAME, Identifier, "Delay SI");
	if (RomEmulateAI)	Settings_Write(RDS_NAME, Identifier, "Emulate AI", "");
	else				Settings_Delete(RDS_NAME, Identifier, "Emulate AI");
	if (RomAudioSignal)	Settings_Write(RDS_NAME, Identifier, "Audio Signal", "");
	else				Settings_Delete(RDS_NAME, Identifier, "Audio Signal");
	if (RomSPHack)		Settings_Write(RDS_NAME, Identifier, "SP Hack", "");
	else				Settings_Delete(RDS_NAME, Identifier, "SP Hack");
	if (RomUseLargeBuffer)
		Settings_Write(RDS_NAME, Identifier, "Use Large Buffer", "");
	else				Settings_Delete(RDS_NAME, Identifier, "Use Large Buffer");

	switch (RomUseLinking) {
	case 0:
		Settings_Write(RDS_NAME, Identifier, "Linking", "On"); break;
	case 1:
		Settings_Write(RDS_NAME, Identifier, "Linking", "Off"); break;
	default:
		Settings_Write(RDS_NAME, Identifier, "Linking", "Global"); break;
	}

	if (RomModVIS == 1500)
		Settings_Delete(RDS_NAME, Identifier, "VI");
	else {
		sprintf(String, "%d", RomModVIS);
		Settings_Write(RDS_NAME, Identifier, "VI", String);
	}
}

void SetRecentRomDir(DWORD Index) {
	Index -= ID_FILE_RECENT_DIR;
	if (Index < 0 || Index >(DWORD)RomDirsToRemember) { return; }
	SetRomDirectory(LastDirs[Index]);
	RefreshRomBrowser();
}

void SetRomDirectory(char* Directory) {
	Settings_Write(APPS_NAME, "Directories", "Rom", Directory);
	AddRecentDir(hMainWindow, Directory);
}

void RecalculateCRCs(BYTE* data, DWORD data_size) {
	int i;
	unsigned int seed, crc[2];
	unsigned int t1, t2, t3, t4, t5, t6, r, d, j;
	enum CIC_CHIP chip;

	chip = GetCicChipID(data);

	switch (chip) {
	case CIC_NUS_6101:
	case CIC_NUS_6102:
		seed = 0xF8CA4DDC; break;
	case CIC_NUS_6103:
		seed = 0xA3886759; break;
	case CIC_NUS_6105:
		seed = 0xDF26F436; break;
	case CIC_NUS_6106:
		seed = 0x1FEA617A; break;
	default:
		return;
	}

	t1 = t2 = t3 = t4 = t5 = t6 = seed;

	for (i = 0x00001000; i < CRC_SRC_SIZE; i += 4) {
		if ((unsigned int)(i + 3) > data_size)
			d = 0;
		else
			d = data[i + 3] << 24 | data[i + 2] << 16 | data[i + 1] << 8 | data[i];
		if ((t6 + d) < t6)
			t4++;
		t6 += d;
		t3 ^= d;
		r = (d << (d & 0x1F)) | (d >> (32 - (d & 0x1F)));
		t5 += r;
		if (t2 > d)
			t2 ^= r;
		else
			t2 ^= t6 ^ d;

		if (chip == CIC_NUS_6105) {
			j = 0x40 + 0x0710 + (i & 0xFF);
			if ((unsigned int)(j + 3) <= data_size) {
				t1 += (data[j + 3] << 24 | data[j + 2] << 16 | data[j + 1] << 8 | data[j]) ^ d;
			}
		}
		else
			t1 += t5 ^ d;
	}

	if (chip == CIC_NUS_6103) {
		crc[0] = (t6 ^ t4) + t3;
		crc[1] = (t5 ^ t2) + t1;
	}
	else if (chip == CIC_NUS_6106) {
		crc[0] = (t6 * t4) + t3;
		crc[1] = (t5 * t2) + t1;
	}
	else {
		crc[0] = t6 ^ t4 ^ t3;
		crc[1] = t5 ^ t2 ^ t1;
	}

	if (*(DWORD*)&data[0x10] != crc[0] || *(DWORD*)&data[0x14] != crc[1]) {
		/*if (ShowDebugMessages)
			DisplayError("Calculated CRC does not match CRC1 and CRC2.");
		*/
		data[0x13] = (crc[0] & 0xFF000000) >> 24;
		data[0x12] = (BYTE)((crc[0] & 0x00FF0000) >> 16);
		data[0x11] = (crc[0] & 0x0000FF00) >> 8;
		data[0x10] = (crc[0] & 0x000000FF);

		data[0x17] = (crc[1] & 0xFF000000) >> 24;
		data[0x16] = (BYTE)((crc[1] & 0x00FF0000) >> 16);
		data[0x15] = (crc[1] & 0x0000FF00) >> 8;
		data[0x14] = (crc[1] & 0x000000FF);
	}
}

void LoadRomRecalcCRCs(char* FileName, BYTE* CRC1, BYTE* CRC2) {
	BYTE* data = NULL;
	DWORD data_size = 0;

	// As much as this pains me, this is mostly a copy of code that already exists as a copy and paste nightmare
	// Some of the code has been reduced because the checks have already been performed
	// This code will, currently, only be called when the CRC1 and CRC2 of a loaded Rom Header are read as 0's
	if (_strnicmp(&FileName[strlen(FileName) - 4], ".ZIP", 4) == 0) {
		int len, port = 0, FoundRom;
		unz_file_info info;
		char zname[132];
		unzFile file;
		BYTE Test[4];

		// Should not happen at this point, the file should have been opened once already
		file = unzOpen(FileName);
		if (file == NULL) {
			DisplayError(GS(MSG_FAIL_OPEN_ZIP));
			return;
		}

		port = unzGoToFirstFile(file);
		FoundRom = FALSE;
		while (port == UNZ_OK && FoundRom == FALSE) {
			unzGetCurrentFileInfo(file, &info, zname, 128, NULL, 0, NULL, 0);

			if (unzLocateFile(file, zname, 1) != UNZ_OK) {
				unzClose(file);
				DisplayError(GS(MSG_FAIL_ZIP));
				return;
			}

			if (unzOpenCurrentFile(file) != UNZ_OK) {
				unzClose(file);
				DisplayError(GS(MSG_FAIL_OPEN_ZIP));
				return;
			}

			unzReadCurrentFile(file, Test, 4);
			if (IsValidRomImage(Test)) {
				FoundRom = TRUE;
				data_size = min(CRC_SRC_SIZE, info.uncompressed_size);

				data = (BYTE*)malloc(sizeof(BYTE) * data_size);
				if (!data) {
					unzCloseCurrentFile(file);
					unzClose(file);
					DisplayError(GS(MSG_MEM_ALLOC_ERROR));
					return;
				}
				memcpy(data, Test, 4);
				len = unzReadCurrentFile(file, &data[4], data_size - 4) + 4;
				if ((int)data_size != len) {
					free(data);
					unzCloseCurrentFile(file);
					unzClose(file);
					switch (len) {
					case UNZ_ERRNO:
					case UNZ_EOF:
					case UNZ_PARAMERROR:
					case UNZ_BADZIPFILE:
					case UNZ_INTERNALERROR:
					case UNZ_CRCERROR:
						DisplayError(GS(MSG_FAIL_OPEN_ZIP));
						break;
					}
					return;
				}
				if (unzCloseCurrentFile(file) == UNZ_CRCERROR) {
					free(data);
					unzClose(file);
					DisplayError(GS(MSG_FAIL_OPEN_ZIP));
					return;
				}
				unzClose(file);
			}
			if (FoundRom == FALSE) {
				unzCloseCurrentFile(file);
				port = unzGoToNextFile(file);
			}
		}
		if (FoundRom == FALSE) {
			DisplayError(GS(MSG_FAIL_OPEN_ZIP));
			unzClose(file);
			return;
		}
	}
	else {
		DWORD dwRead;
		HANDLE hFile;
		BYTE Test[4];

		hFile = CreateFile(FileName, GENERIC_READ, FILE_SHARE_READ, NULL,
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS,
			NULL);

		if (hFile == INVALID_HANDLE_VALUE) {
			DisplayError(GS(MSG_FAIL_OPEN_IMAGE));
			return;
		}

		SetFilePointer(hFile, 0, 0, FILE_BEGIN);
		ReadFile(hFile, Test, 4, &dwRead, NULL);
		if (!IsValidRomImage(Test)) {
			CloseHandle(hFile);
			DisplayError(GS(MSG_FAIL_IMAGE));
			return;
		}
		data_size = min(CRC_SRC_SIZE, GetFileSize(hFile, NULL));

		data = (BYTE*)malloc(sizeof(BYTE) * data_size);
		if (!data) {
			CloseHandle(hFile);
			DisplayError(GS(MSG_MEM_ALLOC_ERROR));
			return;
		}

		SetFilePointer(hFile, 0, 0, FILE_BEGIN);

		if (!ReadFile(hFile, data, data_size, &dwRead, NULL) || data_size != dwRead) {
			free(data);
			CloseHandle(hFile);
			DisplayError(GS(MSG_FAIL_OPEN_IMAGE));
			return;
		}
		CloseHandle(hFile);
	}
	ByteSwapRom(data, data_size);

	// Finished loading the rom into our temporary byte array, can recalculate now
	RecalculateCRCs(data, data_size);

	// No checks needed, this function only gets called on an as needed basis
	*(DWORD*)CRC1 = *(DWORD*)&data[0x10];
	*(DWORD*)CRC2 = *(DWORD*)&data[0x14];

	// Make sure to free allocated space
	free(data);
}

// This was a test, unzipping is a limiting factor when calculating the xxhash
// Leaving the code as this will likely be used again in the future
BOOL LoadDataForRomBrowser(char* FileName, BYTE* Data, int DataLen, int* RomSize) {
	BYTE* data = NULL;
	DWORD data_size = 0;
	char hash[100];

	if (_strnicmp(&FileName[strlen(FileName) - 4], ".ZIP", 4) == 0) {
		int len, port = 0, FoundRom;
		unz_file_info info;
		char zname[132];
		unzFile file;
		BYTE Test[4];

		// Should not happen at this point, the file should have been opened once already
		file = unzOpen(FileName);
		if (file == NULL) {
			return FALSE;
		}

		port = unzGoToFirstFile(file);
		FoundRom = FALSE;
		while (port == UNZ_OK && FoundRom == FALSE) {
			unzGetCurrentFileInfo(file, &info, zname, 128, NULL, 0, NULL, 0);

			if (unzLocateFile(file, zname, 1) != UNZ_OK) {
				unzClose(file);
				return FALSE;
			}

			if (unzOpenCurrentFile(file) != UNZ_OK) {
				unzClose(file);
				return FALSE;
			}

			unzReadCurrentFile(file, Test, 4);
			if (IsValidRomImage(Test)) {
				FoundRom = TRUE;
				data_size = info.uncompressed_size;

				data = (BYTE*)malloc(sizeof(BYTE) * data_size);
				if (!data) {
					unzCloseCurrentFile(file);
					unzClose(file);
					return FALSE;
				}

				memcpy(data, Test,  4);
				unzReadCurrentFile(file, data + 4, data_size - 4);
				*RomSize = data_size;

				if (unzCloseCurrentFile(file) == UNZ_CRCERROR) {
					unzClose(file);
					return FALSE;
				}
				unzClose(file);
			}
			if (FoundRom == FALSE) {
				unzCloseCurrentFile(file);
				port = unzGoToNextFile(file);
			}
		}
		if (FoundRom == FALSE) {
			unzClose(file);
			return FALSE;
		}
	}
	else {
		DWORD dwRead, dwToRead, TotalRead;
		HANDLE hFile;
		BYTE Test[4];

		hFile = CreateFile(FileName, GENERIC_READ, FILE_SHARE_READ, NULL,
			OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS,
			NULL);

		if (hFile == INVALID_HANDLE_VALUE)
			return FALSE;

		// Read the first 4 bytes (This will let us know if the file is a valid rom)
		SetFilePointer(hFile, 0, 0, FILE_BEGIN);
		if (!ReadFile(hFile, Test, 4, &dwRead, NULL)) {
			CloseHandle(hFile);
			return FALSE;
		}

		// Test the first 4 bytes and only continue if it is a valid rom
		if (!IsValidRomImage(Test)) {
			CloseHandle(hFile);
			return FALSE;
		}

		// Allocate memory for the buffer
		data_size = GetFileSize(hFile, NULL);
		data = (BYTE*)malloc(sizeof(BYTE) * data_size);
		if (!data) {
			CloseHandle(hFile);
			return FALSE;
		}

		// Read the entire file into memory
		SetFilePointer(hFile, 0, 0, FILE_BEGIN);
		if (!ReadFile(hFile, data, data_size, &dwRead, NULL)) {
			CloseHandle(hFile);
			return FALSE;
		}
		*RomSize = data_size;

		// Failed to read the entire file, abort
		if (data_size != dwRead) {
			CloseHandle(hFile);
			return FALSE;
		}
		CloseHandle(hFile);
	}

	ByteSwapRom(data, data_size);

	// This is the new Identifier for the ROM, xxhash
	RomHASH(hash, data, data_size);	

	// Since the rom is fully loaded into memory do the CRC recalculation
	RecalculateCRCs(data, data_size);

	// The rom browser only needs the first 0x1000 bytes
	memcpy(Data, data, DataLen);

	free(data);

	return TRUE;
}
