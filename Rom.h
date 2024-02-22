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
#ifdef __cplusplus
extern "C" {
#endif
	extern DWORD RomFileSize;
	extern int RomRamSize, RomSaveUsing, RomCPUType, RomSelfMod,
		RomUseTlb, RomUseLinking, RomCF, RomUseLargeBuffer, RomUseCache,
		RomDelaySI, RomSPHack, RomAudioSignal, RomEmulateAI, RomModVIS;
	extern char CurrentFileName[MAX_PATH + 1], RomName[MAX_PATH + 1], RomFullName[MAX_PATH + 1];
	extern char LastRoms[10][MAX_PATH + 1], LastDirs[10][MAX_PATH + 1];
	extern BYTE RomHeader[0x1000];
	extern DWORD PrevCRC1, PrevCRC2;

	void CloseMappedRomFile(void);
	void AddRecentFile(HWND hWnd, char* addition);
	BOOL LoadDataFromRomFile(char* FileName, BYTE* Data, int DataLen, int* RomSize);
	BOOL LoadRomHeader(void);
	void CreateRecentFileList(HMENU hMenu);
	void CreateRecentDirList(HMENU hMenu);
	void LoadRecentRom(DWORD Index);
	void LoadRomOptions(void);
	DWORD WINAPI OpenChosenFile(LPVOID lpArgs);
	void ReadRomOptions(void);
	void RemoveRecentList(HWND hWnd);
	void OpenN64Image(void);
	void SaveRecentDirs(void);
	void SaveRecentFiles(void);
	void SaveRomOptions(void);
	void SetRecentRomDir(DWORD Index);
	void SetRomDirectory(char* Directory);
	void RecalculateCRCs(BYTE* data, DWORD data_size);
	void LoadRomRecalcCRCs(char* FileName, BYTE* CRC1, BYTE* CRC2);

	BOOL LoadDataForRomBrowser(char* FileName, BYTE* Data, int DataLen, int* RomSize);

#ifdef __cplusplus
}
#endif
