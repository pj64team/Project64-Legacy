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

//#define SHOW_SCAN_TIME

#include <Windows.h>
#include <commctrl.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <stdio.h>
#include <time.h>
#include <tchar.h>
#include "main.h"
#include "cpu.h"
#include "plugin.h"
#include "resource.h"
#include "RomTools_Common.h"
#include <time.h>
#include "pif.h"

#include <vector>
#include <string>
#include <algorithm>

#define NoOfSortKeys		3
#define RB_FileName			0
#define RB_InternalName		1
#define RB_GoodName			2
#define RB_Status			3
#define RB_RomSize			4
#define RB_CoreNotes		5
#define RB_PluginNotes		6
#define RB_UserNotes		7
#define RB_CartridgeID		8
#define RB_ReleaseVer		9
#define RB_SdkVer			10
#define RB_Manufacturer		11
#define RB_Country			12
#define RB_Developer		13
#define RB_Crc1				14
#define RB_Crc2				15
#define RB_CICChip			16
#define RB_ReleaseDate		17
#define RB_Genre			18
#define RB_Players			19
#define RB_ForceFeedback	20
#define COLOR_TEXT			0
#define COLOR_SELECTED_TEXT	1
#define COLOR_HIGHLIGHTED   2

typedef struct {
	char szFullFileName[MAX_PATH + 1];
	char Status[60];
	char FileName[200];
	char InternalName[22];
	char GoodName[200];
	char CartID[3];
	char PluginNotes[250];
	char CoreNotes[250];
	char UserNotes[250];
	char Developer[30];
	char ReleaseDate[30];
	char Genre[15];
	int	Players;
	int  RomSize;
	int ReleaseVersion;
	BYTE SdkVersion[2];
	BYTE Manufacturer;
	BYTE Country;
	DWORD CRC1;
	DWORD CRC2;
	enum CIC_CHIP CicChip;
	char ForceFeedback[15];
} ROM_INFO;

typedef struct {
	int    Key[NoOfSortKeys];
	BOOL   KeyAscend[NoOfSortKeys];
} SORT_FIELDS;

typedef struct {
	char* status_name;
	COLORREF HighLight;
	COLORREF Text;
	COLORREF SelectedText;
} COLOR_ENTRY;


// Function Prototypes
void GetSortField(int Index, char* ret, int max);
void LoadRomList();
void RomList_SortList();
void RomList_SelectFind(char* match);
void FillRomExtensionInfo(ROM_INFO* pRomInfo);
BOOL FillRomInfo(ROM_INFO* pRomInfo);
void SetSortAscending(BOOL Ascending, int Index);
void SetSortField(char* FieldName, int Index);
void SaveRomList();
COLORREF GetColor(char* status, int selection);
int ColorIndex(char* status);
void SetColors(char* status);
void FillRomList(char* Directory);
DWORD WINAPI RefreshRomBrowserMT(LPVOID lpArgs);
DWORD WINAPI UpdateBrowser(LPVOID lpArgs);

// Global accessed by C code, do not change
char CurrentRBFileName[MAX_PATH + 1] = { "" };

// Global accessed by C code, do not change
ROMBROWSER_FIELDS RomBrowserFields[] =
{
	"File Name",              -1, RB_FileName,      218, RB_FILENAME,
	"Internal Name",          -1, RB_InternalName,  200, RB_INTERNALNAME,
	"Good Name",               0, RB_GoodName,      218, RB_GOODNAME,
	"Status",                  1, RB_Status,         92, RB_STATUS,
	"Rom Size",               -1, RB_RomSize,       100, RB_ROMSIZE,
	"Notes (Core)",            2, RB_CoreNotes,     120, RB_NOTES_CORE,
	"Notes (default plugins)", 3, RB_PluginNotes,   188, RB_NOTES_PLUGIN,
	"Notes (User)",           -1, RB_UserNotes,     100, RB_NOTES_USER,
	"Cartridge ID",           -1, RB_CartridgeID,   100, RB_CART_ID,
	"Release Version",        -1, RB_ReleaseVer,    100, RB_RELEASE_VER,
	"SDK Version",            -1, RB_SdkVer,        100, RB_SDK_VER,
	"Manufacturer",           -1, RB_Manufacturer,  100, RB_MANUFACTUER,
	"Country",                -1, RB_Country,       100, RB_COUNTRY,
	"Developer",              -1, RB_Developer,     100, RB_DEVELOPER,
	"CRC1",                   -1, RB_Crc1,          100, RB_CRC1,
	"CRC2",                   -1, RB_Crc2,          100, RB_CRC2,
	"CIC Chip",               -1, RB_CICChip,       100, RB_CICCHIP,
	"Release Date",           -1, RB_ReleaseDate,   100, RB_RELEASE_DATE,
	"Genre",                  -1, RB_Genre,         100, RB_GENRE,
	"Players",                -1, RB_Players,       100, RB_PLAYERS,
	"Force Feedback",          4, RB_ForceFeedback, 100, RB_FORCE_FEEDBACK,
};

// Global accessed by C code, do not change
int NoOfFields = sizeof(RomBrowserFields) / sizeof(RomBrowserFields[0]);


HWND hRomList = NULL;
int FieldType[(sizeof(RomBrowserFields) / sizeof(RomBrowserFields[0])) + 1];

// Threading related globals
BOOL scanning = FALSE, cancelled = TRUE, pendingUpdate = FALSE;
HANDLE gRBMutex = NULL, romThread = NULL;

// Most data relevant to the rom browser is stored here, the rom list, the color list, and related sorting fields
std::vector<ROM_INFO> RomList;
std::vector<COLOR_ENTRY> ColorList;
SORT_FIELDS gSortFields = { 0 };

// Threaded test
DWORD WINAPI GetRomInfo(LPVOID lpArgs) {
	ROM_INFO* transfer = (ROM_INFO*)lpArgs;

	// Failure to fill rom info, use szFullFileName to denote an error by clearing it
	if (!FillRomInfo(transfer))
		strcpy(transfer->szFullFileName, "");

	return 0;
}

void AddRomToList(char* RomLocation) {
	DWORD wait_result;
	ROM_INFO temp = { 0 };

	strncpy(temp.szFullFileName, RomLocation, MAX_PATH);
	if (!FillRomInfo(&temp))
		return;

	wait_result = WaitForSingleObject(gRBMutex, INFINITE);

	// Mutex ownership acquired, okay to continue
	if (wait_result == WAIT_OBJECT_0) {
		RomList.push_back(temp);
	}

	// No longer require mutual exclusion
	if (!ReleaseMutex(gRBMutex))
		MessageBox(NULL, "Failed to release a mutex???", "Error", MB_OK);

	// Corruption? Mutex was released by operating system, just bail and reuse memory allocation error
	if (wait_result == WAIT_ABANDONED) {
		DisplayError(GS(MSG_MEM_ALLOC_ERROR));
		ExitThread(0);
	}
}

void CreateRomListControl(HWND hParent) {
	DWORD dwStyle;
	hRomList = CreateWindow(WC_LISTVIEW, NULL,
		WS_TABSTOP | WS_VISIBLE | WS_CHILD |
		LVS_OWNERDATA | LVS_OWNERDRAWFIXED | LVS_SINGLESEL | LVS_REPORT,
		0, 0, 0, 0, hParent, (HMENU)IDC_ROMLIST, hInst, NULL);

	// Double buffering! Useful to keep the flicker down
	dwStyle = ListView_GetExtendedListViewStyle(hRomList) | LVS_EX_DOUBLEBUFFER;
	ListView_SetExtendedListViewStyle(hRomList, dwStyle);

	// Will be used to control when the listview can be updated
	gRBMutex = CreateMutex(NULL, FALSE, NULL);

	ResetRomBrowserColomuns();
	LoadRomList();
}

void FixRomBrowserColumnLang(void) {
	ResetRomBrowserColomuns();
}

void HideRomBrowser(void) {
	DWORD X, Y;
	long Style;

	if (CPURunning) { return; }
	if (hRomList == NULL) { return; }

	IgnoreMove = TRUE;
	if (IsRomBrowserMaximized()) 
		ShowWindow(hMainWindow, SW_RESTORE);
	ShowWindow(hMainWindow, SW_HIDE);

	Style = GetWindowLong(hMainWindow, GWL_STYLE) & ~(WS_SIZEBOX | WS_MAXIMIZEBOX);
	SetWindowLong(hMainWindow, GWL_STYLE, Style);

	if (GetStoredWinPos("Main", &X, &Y))
		SetWindowPos(hMainWindow, NULL, X, Y, 0, 0, SWP_NOZORDER | SWP_NOSIZE);

	EnableWindow(hRomList, FALSE);
	ShowWindow(hRomList, SW_HIDE);
	SetupPlugins(hMainWindow);

	SendMessage(hMainWindow, WM_USER + 17, 0, 0);
	ShowWindow(hMainWindow, SW_SHOW);
	IgnoreMove = FALSE;
}

BOOL IsRomBrowserMaximized(void) {
	return Settings_ReadBool(APPS_NAME, "Rom Browser Page", "Maximized", FALSE);
}

BOOL IsSortAscending(int Index) {
	char Search[200];
	sprintf(Search, "Sort Ascending %d", Index);
	return Settings_ReadBool(APPS_NAME, "Rom Browser Page", Search, FALSE);
}

void LoadRomList(void) {
	char path_buffer[_MAX_PATH], drive[_MAX_DRIVE], dir[_MAX_DIR];
	char fname[_MAX_FNAME], ext[_MAX_EXT];
	char FileName[_MAX_PATH];
	int Size, count;
	DWORD dwRead;
	HANDLE hFile;
	ROM_INFO temp;

	// Clear the rom browser
	FreeRomBrowser();
	ListView_SetItemCount(hRomList, 0);

	GetModuleFileName(NULL, path_buffer, sizeof(path_buffer));
	_splitpath(path_buffer, drive, dir, fname, ext);
	sprintf(FileName, "%s%s%s", drive, dir, ROC_NAME);
	
	hFile = CreateFile(FileName, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		RefreshRomBrowser();
		return;
	}

	// The first 4 bytes contains the size of ROM_INFO
	Size = 0;
	ReadFile(hFile, &Size, sizeof(Size), &dwRead, NULL);
	if (Size != sizeof(ROM_INFO) || dwRead != sizeof(Size)) {
		CloseHandle(hFile);
		RefreshRomBrowser();
		return;
	}	

	// The next 4 bytes contains the amount of entries
	ReadFile(hFile, &count, sizeof(count), &dwRead, NULL);
	if (count == 0) {
		CloseHandle(hFile);
		RefreshRomBrowser();
		return;
	}
	RomList.reserve(count);

	// Read each entry and add it to the vector
	while (count != 0) {
		ReadFile(hFile, &temp, sizeof(temp), &dwRead, NULL);
		RomList.push_back(temp);

		// Make sure the color cache for the status is set
		SetColors(RomList.back().Status);

		// Read extra information from the files
		FillRomExtensionInfo(&RomList.back());

		--count;
	}

	CloseHandle(hFile);

	RomList_SortList();
	ListView_SetItemCount(hRomList, RomList.size());
	RomList_SelectFind(LastRoms[0]);
}

void LoadRomBrowserColumnInfo(void) {
	char String[200];
	int count;

	for (count = 0; count < NoOfFields; count++) {
		// Column Position
		RomBrowserFields[count].Pos = Settings_ReadInt(APPS_NAME, "Rom Browser", RomBrowserFields[count].Name, RomBrowserFields[count].Pos);

		// Column Width
		sprintf(String, "%s.Width", RomBrowserFields[count].Name);
		RomBrowserFields[count].ColWidth = Settings_ReadInt(APPS_NAME, "Rom Browser", String, RomBrowserFields[count].ColWidth);
	}
	FixRomBrowserColumnLang();
}

void FillRomExtensionInfo(ROM_INFO* pRomInfo) {
	char Identifier[100], * read;

	RomIDPreScanned(Identifier, &pRomInfo->CRC1, &pRomInfo->CRC2, &pRomInfo->Country);

	//Rom Notes
	if (RomBrowserFields[RB_UserNotes].Pos >= 0) {
		Settings_Read(RDN_NAME, Identifier, "Note", "", &read);
		strncpy(pRomInfo->UserNotes, read, sizeof(pRomInfo->UserNotes));
		if (read) free(read);
	}

	//Rom Extension info
	if (RomBrowserFields[RB_Developer].Pos >= 0) {
		Settings_Read(RDI_NAME, Identifier, "Developer", "", &read);
		strncpy(pRomInfo->Developer, read, sizeof(pRomInfo->Developer));
		if (read) free(read);
	}

	if (RomBrowserFields[RB_ReleaseDate].Pos >= 0) {
		Settings_Read(RDI_NAME, Identifier, "ReleaseDate", "", &read);
		strncpy(pRomInfo->ReleaseDate, read, sizeof(pRomInfo->ReleaseDate));
		if (read) free(read);
	}

	if (RomBrowserFields[RB_Genre].Pos >= 0) {
		Settings_Read(RDI_NAME, Identifier, "Genre", "", &read);
		strncpy(pRomInfo->Genre, read, sizeof(pRomInfo->Genre));
		if (read) free(read);
	}

	if (RomBrowserFields[RB_Players].Pos >= 0)
		pRomInfo->Players = Settings_ReadInt(RDI_NAME, Identifier, "Players", 1);

	if (RomBrowserFields[RB_ForceFeedback].Pos >= 0) {
		Settings_Read(RDI_NAME, Identifier, "ForceFeedback", "", &read);
		strncpy(pRomInfo->ForceFeedback, read, sizeof(pRomInfo->ForceFeedback));
		if (read) free(read);
	}

	//Rom Settings
	if (RomBrowserFields[RB_GoodName].Pos >= 0) {
		Settings_Read(RDS_NAME, Identifier, "Name", GS(RB_NOT_GOOD_FILE), &read);
		strncpy(pRomInfo->GoodName, read, sizeof(pRomInfo->GoodName));
		if (read) free(read);
	}

	if (RomBrowserFields[RB_Status].Pos >= 0) {
		Settings_Read(RDS_NAME, Identifier, "Status", Default_RomStatus, &read);
		strncpy(pRomInfo->Status, read, sizeof(pRomInfo->Status));
		if (read) free(read);
	}

	if (RomBrowserFields[RB_CoreNotes].Pos >= 0) {
		Settings_Read(RDS_NAME, Identifier, "Core Note", "", &read);
		strncpy(pRomInfo->CoreNotes, read, sizeof(pRomInfo->CoreNotes));
		if (read) free(read);
	}

	if (RomBrowserFields[RB_PluginNotes].Pos >= 0) {
		Settings_Read(RDS_NAME, Identifier, "Plugin Note", "", &read);
		strncpy(pRomInfo->PluginNotes, read, sizeof(pRomInfo->PluginNotes));
		if (read) free(read);
	}

	SetColors(pRomInfo->Status);
}

BOOL FillRomInfo(ROM_INFO* pRomInfo) {
	BYTE RomData[0x1000];
	
	// This is the experimental work for using xxhash, comment it out for now
	//if (!LoadDataForRomBrowser(pRomInfo->szFullFileName, RomData, sizeof(RomData), &pRomInfo->RomSize))
	//	return FALSE;
	
	if (!LoadDataFromRomFile(pRomInfo->szFullFileName, RomData, sizeof(RomData), &pRomInfo->RomSize))
		return FALSE;

	_splitpath(pRomInfo->szFullFileName, NULL, NULL, pRomInfo->FileName, NULL);

	GetRomName(pRomInfo->InternalName, RomData);
	GetRomCartID(pRomInfo->CartID, RomData);
	GetRomReleaseVersion(&pRomInfo->ReleaseVersion, RomData);
	GetRomSdkVersion(pRomInfo->SdkVersion, RomData);
	GetRomManufacturer(&pRomInfo->Manufacturer, RomData);
	GetRomCountry(&pRomInfo->Country, RomData);
	GetRomCRC1(&pRomInfo->CRC1, RomData);
	GetRomCRC2(&pRomInfo->CRC2, RomData);
	pRomInfo->CicChip = GetRomCicChipID(RomData);

	FillRomExtensionInfo(pRomInfo);
	return TRUE;
}

int GetRomBrowserSize(DWORD* nWidth, DWORD* nHeight) {
	*nWidth = Settings_ReadInt(APPS_NAME, "Rom Browser Page", "Width", -1);
	*nHeight = Settings_ReadInt(APPS_NAME, "Rom Browser Page", "Height", -1);

	if (*nWidth == -1 || *nHeight == -1)
		return FALSE;

	return TRUE;
}

void GetSortField(int Index, char* ret, int max) {
	char String[200], * read;

	sprintf(String, "Sort Field %d", Index);
	Settings_Read(APPS_NAME, "Rom Browser Page", String, "", &read);
	strncpy(ret, read, max);
	if (read) free(read);
}

void RefreshRomBrowser(void) {
	// Update to not scan if the rom list is hidden
	if (hRomList && !IsWindowEnabled(hRomList)) {
		FreeRomBrowser();
		SaveRomList();
		pendingUpdate = TRUE;
		return;
	}

	if (!hRomList)
		return;

	if (scanning) {
		MessageBox(NULL, "Still Scanning!", "Wait up!", MB_OK);
		return;
	}

	ListView_SetItemCount(hRomList, 0);
	FreeRomBrowser();

	romThread = CreateThread(NULL, 0, RefreshRomBrowserMT, NULL, 0, NULL);
}

void ResetRomBrowserColomuns(void) {
	int Column, index;
	LV_COLUMN lvColumn;
	char szString[300];

	//SaveRomBrowserColumnInfo();
	memset(&lvColumn, 0, sizeof(lvColumn));
	lvColumn.mask = LVCF_FMT;
	while (ListView_GetColumn(hRomList, 0, &lvColumn)) {
		ListView_DeleteColumn(hRomList, 0);
	}

	//Add Colomuns
	lvColumn.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
	lvColumn.fmt = LVCFMT_LEFT;
	lvColumn.pszText = szString;

	for (Column = 0; Column < NoOfFields; Column++) {
		for (index = 0; index < NoOfFields; index++) {
			if (RomBrowserFields[index].Pos == Column) { break; }
		}
		if (index == NoOfFields || RomBrowserFields[index].Pos != Column) {
			FieldType[Column] = -1;
			break;
		}
		FieldType[Column] = RomBrowserFields[index].ID;
		lvColumn.cx = RomBrowserFields[index].ColWidth;
		strncpy(szString, GS(RomBrowserFields[index].LangID), sizeof(szString));
		ListView_InsertColumn(hRomList, Column, &lvColumn);
	}
}

void ResizeRomListControl(WORD nWidth, WORD nHeight) {
	if (IsWindow(hRomList)) {
		if (IsWindow(hStatusWnd)) {
			RECT rc;

			GetWindowRect(hStatusWnd, &rc);
			nHeight -= (WORD)(rc.bottom - rc.top);
		}
		MoveWindow(hRomList, 0, 0, nWidth, nHeight, TRUE);
	}
}

void RomList_ColumnSortList(LPNMLISTVIEW pnmv) {
	int index, iItem;
	char selected_filename[261], String[200];

	// Save the previously selected item 
	iItem = ListView_GetNextItem(hRomList, -1, LVNI_SELECTED);
	if (iItem != -1)
		strcpy(selected_filename, RomList.at(iItem).szFullFileName);

	for (index = 0; index < NoOfFields; index++) {
		if (RomBrowserFields[index].Pos == pnmv->iSubItem) { break; }
	}
	if (NoOfFields == index) { return; }
	GetSortField(0, String, sizeof(String));
	if (_stricmp(String, RomBrowserFields[index].Name) == 0) {
		SetSortAscending(!IsSortAscending(0), 0);
	}
	else {
		int count;

		for (count = NoOfSortKeys - 1; count > 0; count--) {
			GetSortField(count - 1, String, sizeof(String));
			if (strlen(String) > 0) {
				SetSortField(String, count);
				SetSortAscending(IsSortAscending(count - 1), count);
			}
		}
		SetSortField(RomBrowserFields[index].Name, 0);
		SetSortAscending(TRUE, 0);
	}
	RomList_SortList();
	ListView_RedrawItems(hRomList, 0, RomList.size());

	// Make sure the last item selected is still selected
	if (iItem != -1) {
		RomList_SelectFind(selected_filename);
	}
}

bool RomList_Compare(const ROM_INFO& a, const ROM_INFO& b) {
	ROM_INFO pRomInfo1, pRomInfo2;
	int count, compare;

	for (count = 0; count < NoOfSortKeys; count++) {
		pRomInfo1 = gSortFields.KeyAscend[count] ? a : b;
		pRomInfo2 = gSortFields.KeyAscend[count] ? b : a;

		switch (gSortFields.Key[count]) {
			case RB_FileName:
				compare = lstrcmpi(pRomInfo1.FileName, pRomInfo2.FileName);
				break;
			case RB_InternalName:
				compare = lstrcmpi(pRomInfo1.InternalName, pRomInfo2.InternalName);
				break;
			case RB_GoodName:
				compare = lstrcmpi(pRomInfo1.GoodName, pRomInfo2.GoodName);
				break;
			case RB_Status:
				compare = lstrcmpi(pRomInfo1.Status, pRomInfo2.Status);
				break;
			case RB_RomSize:
				compare = pRomInfo1.RomSize - pRomInfo2.RomSize;
				break;
			case RB_CoreNotes:
				compare = lstrcmpi(pRomInfo1.CoreNotes, pRomInfo2.CoreNotes);
				break;
			case RB_PluginNotes:
				compare = lstrcmpi(pRomInfo1.PluginNotes, pRomInfo2.PluginNotes);
				break;
			case RB_UserNotes:
				compare = lstrcmpi(pRomInfo1.UserNotes, pRomInfo2.UserNotes);
				break;
			case RB_CartridgeID:
				compare = lstrcmpi(pRomInfo1.CartID, pRomInfo2.CartID);
				break;
			case RB_ReleaseVer:
				compare = (int)pRomInfo1.ReleaseVersion - (int)pRomInfo2.ReleaseVersion;
				break;
			case RB_SdkVer:
				compare = (int)(*(SHORT*)pRomInfo1.SdkVersion) - (int)(*(SHORT*)pRomInfo2.SdkVersion);
				break;
			case RB_Manufacturer:
				compare = (int)pRomInfo1.Manufacturer - (int)pRomInfo2.Manufacturer;
				break;
			case RB_Country:
			{
				char junk1[50], junk2[50];
				CountryCodeToString(junk1, pRomInfo1.Country, sizeof(junk1));
				CountryCodeToString(junk2, pRomInfo2.Country, sizeof(junk2));
				compare = lstrcmpi(junk1, junk2);
				break;
			}
			case RB_Developer:
				compare = lstrcmpi(pRomInfo1.Developer, pRomInfo2.Developer);
				break;
			case RB_Crc1:
			{
				char crc_str1[9], crc_str2[9];
				sprintf(crc_str1, "%08x", pRomInfo1.CRC1);
				sprintf(crc_str2, "%08x", pRomInfo2.CRC1);
				compare = lstrcmpi(crc_str1, crc_str2);
				break;
			}
			case RB_Crc2:
			{
				char crc_str1[9], crc_str2[9];
				sprintf(crc_str1, "%08x", pRomInfo1.CRC2);
				sprintf(crc_str2, "%08x", pRomInfo2.CRC2);
				compare = lstrcmpi(crc_str1, crc_str2);
				break;
			}
			case RB_CICChip:
			{
				char junk1[50], junk2[50];
				BuildRomCicChipString(pRomInfo1.CicChip, junk1, sizeof(junk1), GetRomRegionByCode(pRomInfo1.Country));
				BuildRomCicChipString(pRomInfo2.CicChip, junk2, sizeof(junk2), GetRomRegionByCode(pRomInfo2.Country));
				compare = lstrcmpi(junk1, junk2);
				break;
			}
			case RB_ReleaseDate:
				compare = lstrcmpi(pRomInfo1.ReleaseDate, pRomInfo2.ReleaseDate);
				break;
			case RB_Players:
				compare = pRomInfo1.Players - pRomInfo2.Players;
				break;
			case RB_ForceFeedback:
				compare = lstrcmpi(pRomInfo1.ForceFeedback, pRomInfo2.ForceFeedback);
				break;
			case RB_Genre:
				compare = lstrcmpi(pRomInfo1.Genre, pRomInfo2.Genre);
				break;
			default:
				compare = 0;
				break;
		}

		if (compare > 0) {
			// a > b (compare is returning 1, so a is greater than b)
			return false;
		} else if (compare == 0) {
			// Same, continue the compare by using the other sort keys
			break;
		} else {
			// a < b (compare is returning -1, so a is less than b)
			return true;
		}
	}
	return false;
}

void RomList_GetDispInfo(LPNMHDR pnmh) {
	LV_DISPINFO* lpdi = (LV_DISPINFO*)pnmh;
	ROM_INFO* pRomInfo = &RomList.at(lpdi->item.iItem);

	// Do not continue if the request does not contain a valid text field (Windows XP was having issues here)
	if (!(lpdi->item.mask & LVIF_TEXT))
		return;

	switch (FieldType[lpdi->item.iSubItem]) {
		case RB_FileName:
			strncpy(lpdi->item.pszText, pRomInfo->FileName, lpdi->item.cchTextMax);
			break;
		case RB_InternalName:
			strncpy(lpdi->item.pszText, pRomInfo->InternalName, lpdi->item.cchTextMax);
			break;
		case RB_GoodName:
			strncpy(lpdi->item.pszText, pRomInfo->GoodName, lpdi->item.cchTextMax);
			break;
		case RB_CoreNotes:
			strncpy(lpdi->item.pszText, pRomInfo->CoreNotes, lpdi->item.cchTextMax);
			break;
		case RB_PluginNotes:
			strncpy(lpdi->item.pszText, pRomInfo->PluginNotes, lpdi->item.cchTextMax);
			break;
		case RB_Status:
			strncpy(lpdi->item.pszText, pRomInfo->Status, lpdi->item.cchTextMax);
			break;
		case RB_RomSize:
			sprintf(lpdi->item.pszText, "%.1f MBit", (float)pRomInfo->RomSize / 0x20000);
			break;
		case RB_CartridgeID:
			strncpy(lpdi->item.pszText, pRomInfo->CartID, lpdi->item.cchTextMax);
			break;
		case RB_ReleaseVer:
			sprintf(lpdi->item.pszText, "v1.%d", pRomInfo->ReleaseVersion);
			break;
		case RB_SdkVer:
			sprintf(lpdi->item.pszText, "v%d.%d%c", pRomInfo->SdkVersion[1] / 10, pRomInfo->SdkVersion[1] % 10, pRomInfo->SdkVersion[0]);
			break;
		case RB_Manufacturer:
			switch (pRomInfo->Manufacturer) {
				case 'C':
				case 'N':
					strncpy(lpdi->item.pszText, "Nintendo", lpdi->item.cchTextMax);
					break;

				case 0:
					strncpy(lpdi->item.pszText, "None", lpdi->item.cchTextMax);
					break;

				default:
					sprintf(lpdi->item.pszText, "(Unknown %c (%X))", pRomInfo->Manufacturer, pRomInfo->Manufacturer);
					break;
			}
			break;
		case RB_Country: {
			char junk[50];
			CountryCodeToString(junk, pRomInfo->Country, 50);
			strncpy(lpdi->item.pszText, junk, lpdi->item.cchTextMax);
			break;
		}
		case RB_Crc1:
			sprintf(lpdi->item.pszText, "0x%08X", pRomInfo->CRC1);
			break;
		case RB_Crc2:
			sprintf(lpdi->item.pszText, "0x%08X", pRomInfo->CRC2);
			break;
		case RB_CICChip:
			BuildRomCicChipString(pRomInfo->CicChip, lpdi->item.pszText, lpdi->item.cchTextMax, GetRomRegionByCode(pRomInfo->Country));
			break;
		case RB_UserNotes:
			strncpy(lpdi->item.pszText, pRomInfo->UserNotes, lpdi->item.cchTextMax);
			break;
		case RB_Developer:
			strncpy(lpdi->item.pszText, pRomInfo->Developer, lpdi->item.cchTextMax);
			break;
		case RB_ReleaseDate:
			strncpy(lpdi->item.pszText, pRomInfo->ReleaseDate, lpdi->item.cchTextMax);
			break;
		case RB_Genre:
			strncpy(lpdi->item.pszText, pRomInfo->Genre, lpdi->item.cchTextMax);
			break;
		case RB_Players:
			sprintf(lpdi->item.pszText, "%d", pRomInfo->Players);
			break;
		case RB_ForceFeedback:
			strncpy(lpdi->item.pszText, pRomInfo->ForceFeedback, lpdi->item.cchTextMax);
			break;
		default:
			strncpy(lpdi->item.pszText, " ", lpdi->item.cchTextMax);
	}

	if (lpdi->item.pszText == NULL)
		return;

	if (strlen(lpdi->item.pszText) == 0)
		strcpy(lpdi->item.pszText, " ");
}

void RomList_PopupMenu(LPNMHDR) {
	HMENU hMenu = LoadMenu(hInst, MAKEINTRESOURCE(IDR_POPUP));
	HMENU hPopupMenu = GetSubMenu(hMenu, 0);
	POINT Mouse;
	LONG iItem;

	GetCursorPos(&Mouse);

	iItem = ListView_GetNextItem(hRomList, -1, LVNI_SELECTED);
	if (iItem != -1)
		strcpy(CurrentRBFileName, RomList.at(iItem).szFullFileName);
	else
		strcpy(CurrentRBFileName, "");

	//Fix up menu
	MenuSetText(hPopupMenu, 0, GS(POPUP_PLAY), NULL);
	MenuSetText(hPopupMenu, 2, GS(MENU_REFRESH), NULL);
	MenuSetText(hPopupMenu, 3, GS(MENU_CHOOSE_ROM), NULL);
	MenuSetText(hPopupMenu, 5, GS(POPUP_INFO), NULL);
	MenuSetText(hPopupMenu, 6, GS(POPUP_GAMEINFO), NULL);
	MenuSetText(hPopupMenu, 8, GS(POPUP_SETTINGS), NULL);
	MenuSetText(hPopupMenu, 9, GS(POPUP_CHEATS), NULL);

	if (strlen(CurrentRBFileName) == 0) {
		DeleteMenu(hPopupMenu, 9, MF_BYPOSITION);
		DeleteMenu(hPopupMenu, 8, MF_BYPOSITION);
		DeleteMenu(hPopupMenu, 7, MF_BYPOSITION);
		DeleteMenu(hPopupMenu, 6, MF_BYPOSITION);
		DeleteMenu(hPopupMenu, 5, MF_BYPOSITION);
		DeleteMenu(hPopupMenu, 4, MF_BYPOSITION);
		DeleteMenu(hPopupMenu, 1, MF_BYPOSITION);
		DeleteMenu(hPopupMenu, 0, MF_BYPOSITION);
	}
	else {
		if (BasicMode && !RememberCheats)
			DeleteMenu(hPopupMenu, 9, MF_BYPOSITION);

		if (BasicMode)
			DeleteMenu(hPopupMenu, 8, MF_BYPOSITION);

		if (BasicMode && !RememberCheats)
			DeleteMenu(hPopupMenu, 7, MF_BYPOSITION);
	}

	TrackPopupMenu(hPopupMenu, 0, Mouse.x, Mouse.y, 0, hMainWindow, NULL);
	DestroyMenu(hMenu);
}

void RomList_SetFocus(void) {
	if (!RomListVisible())
		return;
	SetFocus(hRomList);
}

void RomList_OpenRom(LPNMHDR) {
	DWORD ThreadID;
	LONG iItem;

	iItem = ListView_GetNextItem(hRomList, -1, LVNI_SELECTED);
	if (iItem == -1)
		return;

	strcpy(CurrentFileName, RomList.at(iItem).szFullFileName);
	CreateThread(NULL, 0, OpenChosenFile, NULL, 0, &ThreadID);
}

void RomList_SortList(void) {
	char SortField[200];
	int count, index;

	for (count = 0; count < NoOfSortKeys; count++) {
		GetSortField(count, SortField, sizeof(SortField));

		for (index = 0; index < NoOfFields; index++) {
			if (_stricmp(RomBrowserFields[index].Name, SortField) == 0)
				break;
		}

		// Global variables used, sort does not allow passing of variables
		gSortFields.Key[count] = index;
		gSortFields.KeyAscend[count] = IsSortAscending(count);
	}

	std::sort(RomList.begin(), RomList.end(), RomList_Compare);	
}

void RomListDrawItem(LPDRAWITEMSTRUCT ditem) {
	RECT rcItem, rcDraw;
	ROM_INFO* pRomInfo;
	char String[300];
	BOOL bSelected;
	HBRUSH hBrush;
	LV_COLUMN lvc;
	int nColumn;

	bSelected = (ListView_GetItemState(hRomList, ditem->itemID, -1) & LVIS_SELECTED);
	pRomInfo = &RomList.at(ditem->itemID);
	if (bSelected) {
		hBrush = CreateSolidBrush(GetColor(pRomInfo->Status, COLOR_HIGHLIGHTED));
		SetTextColor(ditem->hDC, GetColor(pRomInfo->Status, COLOR_SELECTED_TEXT));
	}
	else {
		hBrush = GetSysColorBrush(COLOR_WINDOW);
		SetTextColor(ditem->hDC, GetColor(pRomInfo->Status, COLOR_TEXT));
	}
	FillRect(ditem->hDC, &ditem->rcItem, hBrush);
	SetBkMode(ditem->hDC, TRANSPARENT);

	//Draw
	ListView_GetItemRect(hRomList, ditem->itemID, &rcItem, LVIR_LABEL);
	ListView_GetItemText(hRomList, ditem->itemID, 0, String, sizeof(String));
	memcpy(&rcDraw, &rcItem, sizeof(RECT));
	rcDraw.right -= 3;
	DrawText(ditem->hDC, String, strlen(String), &rcDraw, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX | DT_VCENTER);

	memset(&lvc, 0, sizeof(lvc));
	lvc.mask = LVCF_FMT | LVCF_WIDTH;
	for (nColumn = 1; ListView_GetColumn(hRomList, nColumn, &lvc); nColumn += 1) {
		rcItem.left = rcItem.right;
		rcItem.right += lvc.cx;

		ListView_GetItemText(hRomList, ditem->itemID, nColumn, String, sizeof(String));
		memcpy(&rcDraw, &rcItem, sizeof(RECT));
		rcDraw.right -= 3;
		DrawText(ditem->hDC, String, strlen(String), &rcDraw, DT_LEFT | DT_SINGLELINE | DT_NOPREFIX | DT_VCENTER);
	}

	DeleteObject(hBrush);
}

void RomListNotify(LPNMHDR pnmh) {
	switch (pnmh->code) {
		case LVN_GETDISPINFO: {
			DWORD wait_result = WaitForSingleObject(gRBMutex, INFINITE);

			// Got control of the mutex, that means nothing else is using the listview control
			if (wait_result == WAIT_OBJECT_0) {
				RomList_GetDispInfo(pnmh);

				if (!ReleaseMutex(gRBMutex))
					MessageBox(NULL, "Failed to release a mutex???", "Error!", MB_OK);
			}

			// Abandoned mutex, why???
			if (wait_result == WAIT_ABANDONED) {
				DisplayError(GS(MSG_MEM_ALLOC_ERROR));
				ExitThread(0);
			}
		}
		break;
		case LVN_COLUMNCLICK:
			RomList_ColumnSortList((LPNMLISTVIEW)pnmh);
			break;
		case NM_RETURN:
			RomList_OpenRom(pnmh);
			break;
		case NM_DBLCLK:
			RomList_OpenRom(pnmh);
			break;
		case NM_RCLICK:
			RomList_PopupMenu(pnmh);
			break;
	}
}

BOOL RomListVisible(void) {
	if (hRomList == NULL)
		return FALSE;
	return (IsWindowVisible(hRomList));
}

void SaveRomBrowserColumnInfo(void) {
	int Column, index;
	LV_COLUMN lvColumn;
	char String[200], String2[200];

	memset(&lvColumn, 0, sizeof(lvColumn));
	lvColumn.mask = LVCF_WIDTH;

	for (Column = 0; ListView_GetColumn(hRomList, Column, &lvColumn); Column++) {
		for (index = 0; index < NoOfFields; index++) {
			if (RomBrowserFields[index].Pos == Column)
				break;
		}

		RomBrowserFields[index].ColWidth = lvColumn.cx;
		sprintf(String, "%s.Width", RomBrowserFields[index].Name);
		sprintf(String2, "%d", lvColumn.cx);
		Settings_Write(APPS_NAME, "Rom Browser", String, String2);
	}
}

void SaveRomBrowserColumnPosition(int index, int Position) {
	char szPos[10];

	sprintf(szPos, "%d", Position);
	Settings_Write(APPS_NAME, "Rom Browser", RomBrowserFields[index].Name, szPos);
}

void SaveRomList(void) {
	char path_buffer[_MAX_PATH], drive[_MAX_DRIVE], dir[_MAX_DIR];
	char fname[_MAX_FNAME], ext[_MAX_EXT];
	char FileName[_MAX_PATH];
	DWORD dwWritten;
	HANDLE hFile;
	int Size, count;

	GetModuleFileName(NULL, path_buffer, sizeof(path_buffer));
	_splitpath(path_buffer, drive, dir, fname, ext);
	sprintf(FileName, "%s%s%s", drive, dir, ROC_NAME);

	hFile = CreateFile(FileName, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_RANDOM_ACCESS, NULL);
	Size = sizeof(ROM_INFO);

	// Write the size of ROM_INFO
	WriteFile(hFile, &Size, sizeof(Size), &dwWritten, NULL);

	// Write the count out
	count = (int)RomList.size();
	WriteFile(hFile, &count, sizeof(count), &dwWritten, NULL);

	// Write out each entry
	for (const ROM_INFO &info : RomList) {
		WriteFile(hFile, &info, sizeof(info), &dwWritten, NULL);
	}

	CloseHandle(hFile);
}

// Windows 7 and higher has a bugged SHBrowseForFolder when using BIF_NEWDIALOGSTYLE
// This is a work-around, it enumerates through the child windows until the tree view of the dialog is hit and then
//	ensures the selected element is visible.
// This behavior is not apparent in XP or Vista and it is suggested to use the new FolderBrowserDialog
static BOOL CALLBACK EnumCallback(HWND hWndChild, LPARAM)
{
	char szClass[MAX_PATH];
	HTREEITEM hNode;
	if (GetClassName(hWndChild, szClass, sizeof(szClass)) && strcmp(szClass, "SysTreeView32") == 0) {
		hNode = TreeView_GetSelection(hWndChild);    // found the tree view window
		TreeView_EnsureVisible(hWndChild, hNode);   // ensure its selection is visible
		return(FALSE);   // done; stop enumerating
	}
	return(TRUE);       // continue enumerating
}

// This is sort of a hack, it's designed to force SHBrowseForFolder to scroll down to the selected folder
// It seems to not have worked until BFFM_SELCHANGED was included
int CALLBACK SelectRomDirCallBack(HWND hwnd, DWORD uMsg, DWORD, DWORD lpData) {
	switch (uMsg)
	{
		case BFFM_INITIALIZED:
			// lpData is TRUE since you are passing a path.
			// It would be FALSE if you were passing a pidl.
			if (lpData)
				SendMessage((HWND)hwnd, BFFM_SETSELECTION, TRUE, lpData);
			break;

		case BFFM_SELCHANGED:
			EnumChildWindows(hwnd, EnumCallback, NULL);
			break;
	}
	return 0;
}

void SelectRomDir(void) {
	char Buffer[MAX_PATH], Directory[MAX_PATH], RomDirectory[MAX_PATH + 1];
	LPITEMIDLIST pidl;
	BROWSEINFO bi = { 0 };	// Initialization to 0 prevents XP crash

	Settings_GetDirectory(RomDir, RomDirectory, sizeof(RomDirectory));

	bi.hwndOwner = hMainWindow;
	bi.pidlRoot = NULL;
	bi.pszDisplayName = Buffer;
	bi.lpszTitle = GS(SELECT_ROM_DIR);
	bi.ulFlags = BIF_RETURNFSANCESTORS | BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
	bi.lpfn = (BFFCALLBACK)SelectRomDirCallBack;
	bi.lParam = (DWORD)RomDirectory;

	CoInitialize(NULL);

	if ((pidl = SHBrowseForFolder(&bi)) != NULL) {
		if (SHGetPathFromIDList(pidl, Directory)) {
			int len = strlen(Directory);

			if (Directory[len - 1] != '\\') {
				strcat(Directory, "\\");
			}
			SetRomDirectory(Directory);
			Settings_Write(APPS_NAME, "Directories", "Use Default Rom", "False");
			RefreshRomBrowser();
		}
	}

	CoUninitialize();
}

void SetRomBrowserMaximized(BOOL Maximized) {
	Settings_Write(APPS_NAME, "Page Setup", "Rom Browser Maximized", Maximized ? STR_TRUE : STR_FALSE);
}

void SetRomBrowserSize(int nWidth, int nHeight) {
	char String[100];

	sprintf(String, "%d", nWidth);
	Settings_Write(APPS_NAME, "Rom Browser Page", "Width", String);
	sprintf(String, "%d", nHeight);
	Settings_Write(APPS_NAME, "Rom Browser Page", "Height", String);
}

void SetSortAscending(BOOL Ascending, int Index) {
	char String[200];

	sprintf(String, "Sort Ascending %d", Index);
	Settings_Write(APPS_NAME, "Rom Browser Page", String, Ascending ? STR_TRUE : STR_FALSE);
}

void SetSortField(char* FieldName, int Index) {
	char String[200];

	sprintf(String, "Sort Field %d", Index);
	Settings_Write(APPS_NAME, "Rom Browser Page", String, FieldName);
}

void FillRomList(char* Directory) {
	char FullPath[MAX_PATH + 1], FileName[MAX_PATH + 1], SearchSpec[MAX_PATH + 1];
	char drive[_MAX_DRIVE], dir[_MAX_DIR], ext[_MAX_EXT];
	WIN32_FIND_DATA fd;
	HANDLE hFind;

	strcpy(SearchSpec, Directory);
	if (SearchSpec[strlen(Directory) - 1] != '\\')
		strcat(SearchSpec, "\\");
	strcat(SearchSpec, "*.*");

	hFind = FindFirstFile(SearchSpec, &fd);
	if (hFind == INVALID_HANDLE_VALUE)
		return;

	do {
		// Force a stop of the scanning
		if (!scanning)
			break;

		if (strcmp(fd.cFileName, ".") == 0 || strcmp(fd.cFileName, "..") == 0)
			continue;

		strcpy(FullPath, Directory);
		if (FullPath[strlen(Directory) - 1] != '\\')
			strcat(FullPath, "\\");

		strcat(FullPath, fd.cFileName);
		if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
			if (Recursion)
				FillRomList(FullPath);
			continue;
		}

		_splitpath(FullPath, drive, dir, FileName, ext);
		if (_stricmp(ext, ".zip") == 0 || _stricmp(ext, ".v64") == 0 || _stricmp(ext, ".z64") == 0 || _stricmp(ext, ".n64") == 0 ||
			_stricmp(ext, ".rom") == 0 || _stricmp(ext, ".jap") == 0 || _stricmp(ext, ".pal") == 0 || _stricmp(ext, ".usa") == 0 ||
			_stricmp(ext, ".eur") == 0 || _stricmp(ext, ".bin") == 0) {
			AddRomToList(FullPath);
		}
	} while (FindNextFile(hFind, &fd));

	FindClose(hFind);
}

void ShowRomList(HWND hParent) {
	DWORD X, Y, Width, Height;
	long Style;
	int iItem;

	if (CPURunning)
		return;

	if (hRomList != NULL && IsWindowVisible(hRomList))
		return;
	
	SetupPlugins(hHiddenWin);
	SetupMenu(hMainWindow);
	IgnoreMove = TRUE;	
	ShowWindow(hMainWindow, SW_HIDE);

	if (hRomList == NULL)
		CreateRomListControl(hParent);
	else
		EnableWindow(hRomList, TRUE);

	if (!GetRomBrowserSize(&Width, &Height)) {
		Width = 640;
		Height = 480;
	}
	ChangeWinSize(hMainWindow, Width, Height, NULL);
	iItem = ListView_GetNextItem(hRomList, -1, LVNI_SELECTED);
	ListView_EnsureVisible(hRomList, iItem, FALSE);

	ShowWindow(hRomList, SW_SHOW);
	InvalidateRect(hParent, NULL, TRUE);
	Style = GetWindowLong(hMainWindow, GWL_STYLE) | WS_SIZEBOX | WS_MAXIMIZEBOX;
	SetWindowLong(hMainWindow, GWL_STYLE, Style);
	if (!GetStoredWinPos("Main", &X, &Y)) {
		X = (GetSystemMetrics(SM_CXSCREEN) - Width) / 2;
		Y = (GetSystemMetrics(SM_CYSCREEN) - Height) / 2;
	}
	SetWindowPos(hMainWindow, HWND_NOTOPMOST, X, Y, 0, 0, SWP_NOSIZE);
	if (IsRomBrowserMaximized()) {
		ShowWindow(hMainWindow, SW_MAXIMIZE);
	}
	else {
		ShowWindow(hMainWindow, SW_SHOW);
		DrawMenuBar(hMainWindow);
		ChangeWinSize(hMainWindow, Width, Height, NULL);
	}
	IgnoreMove = FALSE;
	SetupMenu(hMainWindow);

	SetFocus(hRomList);

	if (pendingUpdate) {
		RefreshRomBrowser();
		pendingUpdate = FALSE;
	}
}

void FreeRomBrowser(void) {
	RomList.clear();
	ColorList.clear();
}

COLORREF GetColor(char* status, int selection) {
	int i = ColorIndex(status);

	switch (selection) {
		case COLOR_SELECTED_TEXT:
			if (i == -1)
				return RGB(0xFF, 0xFF, 0xFF);
			return ColorList.at(i).SelectedText;

		case COLOR_HIGHLIGHTED:
			if (i == -1)
				return RGB(0, 0, 0);
			return ColorList.at(i).HighLight;

		default:
			if (i == -1)
				return RGB(0, 0, 0);
			return ColorList.at(i).Text;
	}
}

int ColorIndex(char* status) {
	for (size_t i = 0; i < ColorList.size(); i++) {
		if (strcmp(ColorList.at(i).status_name, status) == 0)
			return i;
	}
	return -1;
}

void SetColors(char* status) {
	int count;
	COLOR_ENTRY colors;
	char String[100], * read;

	if (ColorIndex(status) == -1) {
		sprintf(String, "%s", status);
		Settings_Read(RDS_NAME, "Rom Status", String, "000000", &read);
		count = (AsciiToHex(read) & 0xFFFFFF);
		if (read)
			free(read);
		colors.Text = (count & 0x00FF00) | ((count >> 0x10) & 0xFF) | ((count & 0xFF) << 0x10);

		sprintf(String, "%s.Sel", status);
		Settings_Read(RDS_NAME, "Rom Status", String, "FFFFFF", &read);
		count = (AsciiToHex(read) & 0xFFFFFF);
		if (read)
			free(read);
		if (count < 0) {
			colors.HighLight = COLOR_HIGHLIGHT + 1;
		}
		else {
			count = (count & 0x00FF00) | ((count >> 0x10) & 0xFF) | ((count & 0xFF) << 0x10);
			colors.HighLight = count;
		}

		sprintf(String, "%s.Seltext", status);
		Settings_Read(RDS_NAME, "Rom Status", String, "FFFFFF", &read);
		count = (AsciiToHex(read) & 0xFFFFFF);
		if (read)
			free(read);
		colors.SelectedText = (count & 0x00FF00) | ((count >> 0x10) & 0xFF) | ((count & 0xFF) << 0x10);

		colors.status_name = (char*)malloc(strlen(status) + 1);
		strcpy(colors.status_name, status);

		// Save the colors for status
		ColorList.push_back(colors);
	}
}

LRESULT RomList_FindItem(NMHDR* lParam) {
	NMLVFINDITEM* findInfo;
	size_t currentPos, startPos;
	int col;

	findInfo = (NMLVFINDITEM*)lParam;

	// Search criteria is not supported, only works with strings for now
	if (((findInfo->lvfi.flags) & LVFI_STRING) == 0)
		return -1;

	// Fetch first column, thats what we are searching by
	for (col = 0; col < NoOfFields; col++) {
		if (RomBrowserFields[col].Pos == 0)
			break;
	}

	startPos = findInfo->iStart;

	// Either the last item was selected or nothing was, start at the top
	if (startPos >= RomList.size() - 1 || startPos < 0)
		startPos = 0;

	currentPos = startPos;

	do {
		// String insensitive compare, limited to the size of our search criteria in findInfo->lvfi.psz
		// So this is a "String starts with, ignore case" match
		switch (col) {
			case RB_FileName:
				if (_tcsnicmp(RomList.at(currentPos).FileName, findInfo->lvfi.psz, strlen(findInfo->lvfi.psz)) == 0)
					return currentPos;
				break;
			case RB_InternalName:
				if (_tcsnicmp(RomList.at(currentPos).InternalName, findInfo->lvfi.psz, strlen(findInfo->lvfi.psz)) == 0)
					return currentPos;
				break;
			case RB_GoodName:
				if (_tcsnicmp(RomList.at(currentPos).GoodName, findInfo->lvfi.psz, strlen(findInfo->lvfi.psz)) == 0)
					return currentPos;
				break;
			case RB_Status:
				if (_tcsnicmp(RomList.at(currentPos).Status, findInfo->lvfi.psz, strlen(findInfo->lvfi.psz)) == 0)
					return currentPos;
				break;
			case RB_CoreNotes:
				if (_tcsnicmp(RomList.at(currentPos).CoreNotes, findInfo->lvfi.psz, strlen(findInfo->lvfi.psz)) == 0)
					return currentPos;
				break;
			case RB_PluginNotes:
				if (_tcsnicmp(RomList.at(currentPos).PluginNotes, findInfo->lvfi.psz, strlen(findInfo->lvfi.psz)) == 0)
					return currentPos;
				break;
			case RB_UserNotes:
				if (_tcsnicmp(RomList.at(currentPos).UserNotes, findInfo->lvfi.psz, strlen(findInfo->lvfi.psz)) == 0)
					return currentPos;
				break;
			case RB_Developer:
				if (_tcsnicmp(RomList.at(currentPos).Developer, findInfo->lvfi.psz, strlen(findInfo->lvfi.psz)) == 0)
					return currentPos;
				break;
			case RB_ReleaseDate:
				if (_tcsnicmp(RomList.at(currentPos).ReleaseDate, findInfo->lvfi.psz, strlen(findInfo->lvfi.psz)) == 0)
					return currentPos;
				break;
			case RB_Genre:
				if (_tcsnicmp(RomList.at(currentPos).Genre, findInfo->lvfi.psz, strlen(findInfo->lvfi.psz)) == 0)
					return currentPos;
				break;
			default:
				return -1;	// Do not support, this was done because there is no uniqueness on these search results
		}

		// Start at the top if we've reached the bottom of the list
		if (currentPos == RomList.size() - 1)
			currentPos = 0;
		else
			currentPos++;
	} while (currentPos != startPos);

	// Nothing found
	return -1;
}

DWORD WINAPI RefreshRomBrowserMT(LPVOID) {
	char RomDirectory[MAX_PATH + 1];

	if (scanning)
		return 0;

	scanning = TRUE;
	cancelled = FALSE;
	CreateThread(NULL, 0, UpdateBrowser, NULL, 0, NULL);

	Settings_GetDirectory(RomDir, RomDirectory, sizeof(RomDirectory));
	FillRomList(RomDirectory);
	scanning = FALSE;

	if (!cancelled) {
		RomList_SortList();
		ListView_SetItemCount(hRomList, RomList.size());
		RomList_SelectFind(LastRoms[0]);
		SaveRomList();
	}

	return 0;
}

// Thread to update the Rom list as it is being loaded
DWORD WINAPI UpdateBrowser(LPVOID) {
	DWORD wait_result;

#ifdef SHOW_SCAN_TIME
	time_t start = time(NULL), diff;
	char msg[100];
#endif

	while (scanning) {
		wait_result = WaitForSingleObject(gRBMutex, INFINITE);

		// Got control of the mutex, that means nothing else is using the listview control
		if (wait_result == WAIT_OBJECT_0)
			RomList_SortList();

		// Abandoned mutex, why???
		if (wait_result == WAIT_ABANDONED) {
			DisplayError(GS(MSG_MEM_ALLOC_ERROR));
			ExitThread(0);
		}

		// Be sure the mutex is released, as the displaying of items is also going to use the mutex
		if (!ReleaseMutex(gRBMutex))
			MessageBox(NULL, "Failed to release a mutex???", "Error!", MB_OK);

		ListView_SetItemCount(hRomList, RomList.size());
		Sleep(250);
	}

#ifdef SHOW_SCAN_TIME
	diff = time(NULL) - start;
	sprintf(msg, "%llu seconds for %u roms", diff, ListView_GetItemCount(hRomList));
	MessageBox(NULL, msg, "Time to scan", MB_OK);
#endif

	return 0;
}

// Not worrying about size of arrays because both szFullFileName and LastRoms are 261 characters long
// Otherwise I would use a safer compare
void RomList_SelectFind(char* match) {

	if (scanning)
		return;

	for (size_t count = 0; count < RomList.size(); count++) {
		if (_stricmp(RomList.at(count).szFullFileName, match) == 0) {
			ListView_SetItemState(hRomList, count, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
			ListView_EnsureVisible(hRomList, count, FALSE);
			break;
		}
	}
}

void RomList_UpdateSelectedInfo(void) {
	int selected;

	if (scanning)
		return;

	selected = ListView_GetNextItem(hRomList, -1, LVNI_SELECTED);

	if (selected != -1) {
		FillRomExtensionInfo(&RomList.at(selected));
		ListView_RedrawItems(hRomList, selected, selected);
	}
}

void RomList_StopScanning() {
	scanning = FALSE;
	cancelled = TRUE;
	WaitForSingleObject(romThread, INFINITE);
}