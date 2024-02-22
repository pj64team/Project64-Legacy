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
	typedef struct {
		char Name[50];
		int  Pos;
		int  ID;
		int  ColWidth;
		int  LangID;
	} ROMBROWSER_FIELDS;

	void FixRomBrowserColumnLang(void);
	void HideRomBrowser(void);
	BOOL IsRomBrowserMaximized(void);
	void LoadRomBrowserColumnInfo(void);
	void RefreshRomBrowser(void);
	void ResetRomBrowserColomuns(void);
	void ResizeRomListControl(WORD nWidth, WORD nHeight);
	void RomListDrawItem(LPDRAWITEMSTRUCT ditem);
	void RomList_SetFocus(void);
	void RomListNotify(LPNMHDR pnmh);
	BOOL RomListVisible(void);
	void SaveRomBrowserColumnPosition(int index, int Position);
	void SaveRomBrowserColumnInfo(void);
	void SetRomBrowserMaximized(BOOL Maximized);
	void SetRomBrowserSize(int nWidth, int nHeight);
	void SelectRomDir(void);
	void ShowRomList(HWND hParent);
	void FreeRomBrowser(void);
	LRESULT RomList_FindItem(NMHDR* lParam);
	void RomList_UpdateSelectedInfo(void);
	void RomList_StopScanning(void);

#define IDC_ROMLIST		                 223

	extern char CurrentRBFileName[MAX_PATH + 1];
	extern ROMBROWSER_FIELDS RomBrowserFields[];
	extern int NoOfFields;
	extern HWND hRomList;
#ifdef __cplusplus
}
#endif
