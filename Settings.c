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
#include <windowsx.h>
#include <commctrl.h>
#include <shlwapi.h>
#include <shlobj.h>
#include <stdio.h>
#include "main.h"
#include "cpu.h"
#include "plugin.h"
#include "debugger.h"
#include "resource.h"
#include "RomTools_Common.h"

BOOL CALLBACK DefaultOptionsProc(HWND, UINT, WPARAM, LPARAM);
BOOL CALLBACK GeneralOptionsProc(HWND, UINT, WPARAM, LPARAM);
BOOL CALLBACK DirSelectProc(HWND, UINT, WPARAM, LPARAM);
BOOL CALLBACK PluginSelectProc(HWND, UINT, WPARAM, LPARAM);
BOOL CALLBACK RomBrowserProc(HWND, UINT, WPARAM, LPARAM);
BOOL CALLBACK RomSettingsProc(HWND, UINT, WPARAM, LPARAM);
BOOL CALLBACK RomNotesProc(HWND, UINT, WPARAM, LPARAM);
BOOL CALLBACK ShellIntegrationProc(HWND, UINT, WPARAM, LPARAM);

typedef struct {
	int     LanguageID;
	WORD    TemplateID;
	DLGPROC pfnDlgProc;
} SETTINGS_TAB;

SETTINGS_TAB SettingsTabs[] = {
	{ TAB_PLUGIN,          IDD_Settings_PlugSel,   PluginSelectProc     },
	{ TAB_DIRECTORY,       IDD_Settings_Directory, DirSelectProc        },
	{ TAB_OPTIONS,         IDD_Settings_General,   GeneralOptionsProc   },
	{ TAB_ROMSELECTION,    IDD_Settings_RomBrowser,RomBrowserProc       },
	{ TAB_ADVANCED,        IDD_Settings_Options,   DefaultOptionsProc   },
	{ TAB_ROMSETTINGS,     IDD_Settings_Rom,       RomSettingsProc      },
	{ TAB_ROMNOTES,        IDD_Settings_RomNotes,  RomNotesProc         },
	{ TAB_SHELLINTERGATION,IDD_Settings_ShellInt,  ShellIntegrationProc }
};

SETTINGS_TAB SettingsTabsBasic[] = {
	{ TAB_PLUGIN, IDD_Settings_PlugSel,   PluginSelectProc     },
	{ TAB_OPTIONS,IDD_Settings_General,   GeneralOptionsProc   },
};

SETTINGS_TAB SettingsTabsRom[] = {
	{ TAB_ROMSETTINGS, IDD_Settings_Rom,      RomSettingsProc  },
	{ TAB_ROMNOTES,    IDD_Settings_RomNotes, RomNotesProc     },
};

void ChangeRomSettings(HWND hwndOwner) {
	char OrigRomName[sizeof(RomName)], OrigFileName[sizeof(CurrentFileName)], OrigFullName[sizeof(RomFullName)];
	PROPSHEETPAGE psp[sizeof(SettingsTabsRom) / sizeof(SETTINGS_TAB)];
	BYTE OrigByteHeader[sizeof(RomHeader)];
	PROPSHEETHEADER psh;
	DWORD OrigFileSize, count;

	strncpy(OrigRomName, RomName, sizeof(OrigRomName));
	strncpy(OrigFileName, CurrentFileName, sizeof(OrigFileName));
	strncpy(OrigFullName, RomFullName, sizeof(RomFullName));
	memcpy(OrigByteHeader, RomHeader, sizeof(RomHeader));
	strncpy(CurrentFileName, CurrentRBFileName, sizeof(CurrentFileName));
	OrigFileSize = RomFileSize;
	LoadRomHeader();

	for (count = 0; count < (sizeof(SettingsTabsRom) / sizeof(SETTINGS_TAB)); count++) {
		psp[count].dwSize = sizeof(PROPSHEETPAGE);
		psp[count].dwFlags = PSP_USETITLE;
		psp[count].hInstance = hInst;
		psp[count].pszTemplate = MAKEINTRESOURCE(SettingsTabsRom[count].TemplateID);
		psp[count].pfnDlgProc = SettingsTabsRom[count].pfnDlgProc;
		psp[count].pszTitle = GS(SettingsTabsRom[count].LanguageID);
		psp[count].lParam = 0;
		psp[count].pfnCallback = NULL;
	}

	psh.dwSize = sizeof(PROPSHEETHEADER);
	psh.dwFlags = PSH_PROPSHEETPAGE | PSH_NOAPPLYNOW;
	psh.hwndParent = hwndOwner;
	psh.hInstance = hInst;
	psh.pszCaption = (LPSTR)GS(OPTIONS_TITLE);
	psh.nPages = sizeof(psp) / sizeof(PROPSHEETPAGE);
	psh.nStartPage = 0;
	psh.ppsp = (LPCPROPSHEETPAGE)&psp;
	psh.pfnCallback = NULL;

	PropertySheet(&psh);

	strncpy(RomName, OrigRomName, sizeof(RomName));
	strncpy(CurrentFileName, OrigFileName, sizeof(CurrentFileName));
	strncpy(RomFullName, OrigFullName, sizeof(RomFullName));
	memcpy(RomHeader, OrigByteHeader, sizeof(RomHeader));
	RomFileSize = OrigFileSize;
}

void ChangeSettings(HWND hwndOwner) {
	PROPSHEETPAGE psp[sizeof(SettingsTabs) / sizeof(SETTINGS_TAB)];
	PROPSHEETPAGE BasicPsp[sizeof(SettingsTabsBasic) / sizeof(SETTINGS_TAB)];
	PROPSHEETHEADER psh;
	psh.dwFlags |= MB_SYSTEMMODAL | MB_TOPMOST;
	int count;

	for (count = 0; count < (sizeof(SettingsTabs) / sizeof(SETTINGS_TAB)); count++) {
		psp[count].dwSize = sizeof(PROPSHEETPAGE);
		psp[count].dwFlags = PSP_USETITLE;
		psp[count].hInstance = hInst;
		psp[count].pszTemplate = MAKEINTRESOURCE(SettingsTabs[count].TemplateID);
		psp[count].pfnDlgProc = SettingsTabs[count].pfnDlgProc;
		psp[count].pszTitle = GS(SettingsTabs[count].LanguageID);
		psp[count].lParam = 0;
		psp[count].pfnCallback = NULL;
	}

	for (count = 0; count < (sizeof(SettingsTabsBasic) / sizeof(SETTINGS_TAB)); count++) {
		BasicPsp[count].dwSize = sizeof(PROPSHEETPAGE);
		BasicPsp[count].dwFlags = PSP_USETITLE;
		BasicPsp[count].hInstance = hInst;
		BasicPsp[count].pszTemplate = MAKEINTRESOURCE(SettingsTabsBasic[count].TemplateID);
		BasicPsp[count].pfnDlgProc = SettingsTabsBasic[count].pfnDlgProc;
		BasicPsp[count].pszTitle = GS(SettingsTabsBasic[count].LanguageID);
		BasicPsp[count].lParam = 0;
		BasicPsp[count].pfnCallback = NULL;
	}

	psh.dwSize = sizeof(PROPSHEETHEADER);
	psh.dwFlags = PSH_PROPSHEETPAGE | PSH_NOAPPLYNOW;
	psh.hwndParent = hwndOwner;
	psh.hInstance = hInst;
	psh.pszCaption = (LPSTR)GS(OPTIONS_TITLE);
	psh.nPages = (BasicMode ? sizeof(BasicPsp) : sizeof(psp)) / sizeof(PROPSHEETPAGE);
	psh.nStartPage = 0;
	psh.ppsp = BasicMode ? (LPCPROPSHEETPAGE)&BasicPsp : (LPCPROPSHEETPAGE)&psp;
	psh.pfnCallback = NULL;

	ShowCursor(TRUE);

	PropertySheet(&psh);
	LoadSettings();
	if (!inFullScreen)
		SetupMenu(hMainWindow);
	else
	{
		HMENU hMenu = GetMenu(hMainWindow), hSubMenu;
		ShowCursor(FALSE);
		DestroyMenu(hMenu);
	}

	if (!AutoSleep && !ManualPaused && (CPU_Paused || CPU_Action.Pause)) { PauseCpu(); }
	return;
}

void SetFlagControl(HWND hDlg, BOOL* Flag, WORD CtrlID, int StringID) {
	SetDlgItemText(hDlg, CtrlID, GS(StringID));
	if (*Flag) { SendMessage(GetDlgItem(hDlg, CtrlID), BM_SETCHECK, BST_CHECKED, 0); }
}

BOOL CALLBACK GeneralOptionsProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch (uMsg) {
	case WM_INITDIALOG:
		SetFlagControl(hDlg, &AutoSleep, IDC_AUTOSLEEP, OPTION_AUTO_SLEEP);
		SetFlagControl(hDlg, &AutoFullScreen, IDC_LOAD_FULLSCREEN, OPTION_AUTO_FULLSCREEN);
		SetFlagControl(hDlg, &BasicMode, IDC_BASIC_MODE, OPTION_BASIC_MODE);
		SetFlagControl(hDlg, &RememberCheats, IDC_REMEMBER_CHEAT, OPTION_REMEMBER_CHEAT);
		break;
	case WM_NOTIFY:
		if (((NMHDR FAR*) lParam)->code == PSN_APPLY) {
			AutoFullScreen = SendMessage(GetDlgItem(hDlg, IDC_LOAD_FULLSCREEN), BM_GETSTATE, 0, 0) == BST_CHECKED ? TRUE : FALSE;
			Settings_Write(APPS_NAME, STR_SETTINGS, STR_AUTOFS, AutoFullScreen ? STR_TRUE : STR_FALSE);

			BasicMode = SendMessage(GetDlgItem(hDlg, IDC_BASIC_MODE), BM_GETSTATE, 0, 0) == BST_CHECKED ? TRUE : FALSE;
			Settings_Write(APPS_NAME, STR_SETTINGS, STR_BASIC, BasicMode ? STR_TRUE : STR_FALSE);

			RememberCheats = SendMessage(GetDlgItem(hDlg, IDC_REMEMBER_CHEAT), BM_GETSTATE, 0, 0) == BST_CHECKED ? TRUE : FALSE;
			Settings_Write(APPS_NAME, STR_SETTINGS, STR_REMEMBER_CHEATS, RememberCheats ? STR_TRUE : STR_FALSE);

			AutoSleep = SendMessage(GetDlgItem(hDlg, IDC_AUTOSLEEP), BM_GETSTATE, 0, 0) == BST_CHECKED ? TRUE : FALSE;
			Settings_Write(APPS_NAME, STR_SETTINGS, STR_AUTOSLEEP, AutoSleep ? STR_TRUE : STR_FALSE);
		}
		break;
	default:
		return FALSE;
	}
	return TRUE;
}

void AddDropDownItem(HWND hDlg, WORD CtrlID, int StringID, int ItemData, int* Variable) {
	HWND hCtrl = GetDlgItem(hDlg, CtrlID);
	int indx;

	indx = SendMessage(hCtrl, CB_ADDSTRING, 0, (LPARAM)GS(StringID));
	SendMessage(hCtrl, CB_SETITEMDATA, indx, ItemData);
	if (*Variable == ItemData) { SendMessage(hCtrl, CB_SETCURSEL, indx, 0); }
	if (SendMessage(hCtrl, CB_GETCOUNT, 0, 0) == 0) { SendMessage(hCtrl, CB_SETCURSEL, 0, 0); }
}

BOOL CALLBACK DefaultOptionsProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	int indx;
	BOOL temp;

	switch (uMsg) {
	case WM_INITDIALOG:
		SetDlgItemText(hDlg, IDC_INFO, GS(ADVANCE_INFO));
		SetDlgItemText(hDlg, IDC_CORE_DEFAULTS, GS(ADVANCE_DEFAULTS));
		SetDlgItemText(hDlg, IDC_TEXT2, GS(ADVANCE_CPU_STYLE));
		SetDlgItemText(hDlg, IDC_TEXT3, GS(ADVANCE_SMCM));
		SetDlgItemText(hDlg, IDC_TEXT4, GS(ADVANCE_MEM_SIZE));
		SetDlgItemText(hDlg, IDC_TEXT5, GS(ADVANCE_ABL));
		SetFlagControl(hDlg, &AutoStart, IDC_START_ON_ROM_OPEN, ADVANCE_AUTO_START);
		SetFlagControl(hDlg, &AutoZip, IDC_ZIP, ADVANCE_COMPRESS);

		temp = Settings_ReadBool(APPS_NAME, STR_SETTINGS, STR_CLEAR_MEMORY, TRUE);
		SetFlagControl(hDlg, &temp, IDC_CLEAR_MEMORY, ADVANCE_CLEAR_MEMORY);

		temp = Settings_ReadBool(APPS_NAME, STR_SETTINGS, STR_HAVEDEBUGGER, FALSE);
		SetFlagControl(hDlg, &temp, IDC_USEDEBUGGER, ADVANCE_USEDEBUGGER);

		temp = Settings_ReadBool(APPS_NAME, STR_SETTINGS, STR_SHOWMOREMESSAGES, FALSE);
		SetFlagControl(hDlg, &temp, IDC_SHOWMOREERRORS, ADVANCE_SHOREMORERRORS);

		AddDropDownItem(hDlg, IDC_CPU_TYPE, CORE_INTERPTER, CPU_Interpreter, &SystemCPU_Type);
		AddDropDownItem(hDlg, IDC_CPU_TYPE, CORE_RECOMPILER, CPU_Recompiler, &SystemCPU_Type);
		AddDropDownItem(hDlg, IDC_CPU_TYPE, CORE_SYNC, CPU_SyncCores, &SystemCPU_Type);

		AddDropDownItem(hDlg, IDC_SELFMOD, SMCM_NONE, ModCode_None, &SystemSelfModCheck);
		AddDropDownItem(hDlg, IDC_SELFMOD, SMCM_CACHE, ModCode_Cache, &SystemSelfModCheck);
		AddDropDownItem(hDlg, IDC_SELFMOD, SMCM_PROECTED, ModCode_ProtectedMemory, &SystemSelfModCheck);
		AddDropDownItem(hDlg, IDC_SELFMOD, SMCM_CHECK_MEM, ModCode_CheckMemoryCache, &SystemSelfModCheck);
		AddDropDownItem(hDlg, IDC_SELFMOD, SMCM_CHANGE_MEM, ModCode_ChangeMemory, &SystemSelfModCheck);
		AddDropDownItem(hDlg, IDC_SELFMOD, SMCM_CHECK_ADV, ModCode_CheckMemory2, &SystemSelfModCheck);

		AddDropDownItem(hDlg, IDC_RDRAM_SIZE, RDRAM_4MB, 0x400000, &SystemRdramSize);
		AddDropDownItem(hDlg, IDC_RDRAM_SIZE, RDRAM_8MB, 0x800000, &SystemRdramSize);

		AddDropDownItem(hDlg, IDC_ABL, ABL_ON, TRUE, &SystemABL);
		AddDropDownItem(hDlg, IDC_ABL, ABL_OFF, FALSE, &SystemABL);
		break;
	case WM_COMMAND:
		if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == IDC_USEDEBUGGER) {
			ShowCursor(TRUE);
		}
		break;
	case WM_NOTIFY:
		if (((NMHDR FAR*) lParam)->code == PSN_APPLY) {
			char String[20];

			// Regular Settings		
			AutoStart = SendMessage(GetDlgItem(hDlg, IDC_START_ON_ROM_OPEN), BM_GETSTATE, 0, 0) == BST_CHECKED ? TRUE : FALSE;
			Settings_Write(APPS_NAME, STR_SETTINGS, STR_AUTOSTART, AutoStart ? STR_TRUE : STR_FALSE);

			AutoZip = SendMessage(GetDlgItem(hDlg, IDC_ZIP), BM_GETSTATE, 0, 0) == BST_CHECKED ? TRUE : FALSE;
			Settings_Write(APPS_NAME, STR_SETTINGS, STR_COMPRESS_STATES, AutoZip ? STR_TRUE : STR_FALSE);

			temp = SendMessage(GetDlgItem(hDlg, IDC_CLEAR_MEMORY), BM_GETSTATE, 0, 0) == BST_CHECKED ? TRUE : FALSE;
			Settings_Write(APPS_NAME, STR_SETTINGS, STR_CLEAR_MEMORY, temp ? STR_TRUE : STR_FALSE);

			temp = SendMessage(GetDlgItem(hDlg, IDC_USEDEBUGGER), BM_GETSTATE, 0, 0) == BST_CHECKED ? TRUE : FALSE;
			Settings_Write(APPS_NAME, STR_SETTINGS, STR_HAVEDEBUGGER, temp ? STR_TRUE : STR_FALSE);

			temp = SendMessage(GetDlgItem(hDlg, IDC_SHOWMOREERRORS), BM_GETSTATE, 0, 0) == BST_CHECKED ? TRUE : FALSE;
			Settings_Write(APPS_NAME, STR_SETTINGS, STR_SHOWMOREMESSAGES, temp ? STR_TRUE : STR_FALSE);

			// Core Defaults
			indx = SendMessage(GetDlgItem(hDlg, IDC_CPU_TYPE), CB_GETCURSEL, 0, 0);
			SystemCPU_Type = SendMessage(GetDlgItem(hDlg, IDC_CPU_TYPE), CB_GETITEMDATA, indx, 0);
			sprintf(String, "%d", SystemCPU_Type);
			Settings_Write(APPS_NAME, STR_COREDEFAULTS, STR_CPUTYPE, String);

			indx = SendMessage(GetDlgItem(hDlg, IDC_SELFMOD), CB_GETCURSEL, 0, 0);
			SystemSelfModCheck = SendMessage(GetDlgItem(hDlg, IDC_SELFMOD), CB_GETITEMDATA, indx, 0);
			sprintf(String, "%d", SystemSelfModCheck);
			Settings_Write(APPS_NAME, STR_COREDEFAULTS, STR_CPUDEF, String);

			indx = SendMessage(GetDlgItem(hDlg, IDC_RDRAM_SIZE), CB_GETCURSEL, 0, 0);
			SystemRdramSize = SendMessage(GetDlgItem(hDlg, IDC_RDRAM_SIZE), CB_GETITEMDATA, indx, 0);
			sprintf(String, "%d", SystemRdramSize);
			Settings_Write(APPS_NAME, STR_COREDEFAULTS, STR_RDRAMDEF, String);

			indx = SendMessage(GetDlgItem(hDlg, IDC_ABL), CB_GETCURSEL, 0, 0);
			SystemABL = SendMessage(GetDlgItem(hDlg, IDC_ABL), CB_GETITEMDATA, indx, 0);
			sprintf(String, "%s", SystemABL ? STR_TRUE : STR_FALSE);
			Settings_Write(APPS_NAME, STR_COREDEFAULTS, STR_ABL, String);
		}
		break;
	default:
		return FALSE;
	}
	return TRUE;
}

// Windows 7 and higher has a bugged SHBrowseForFolder when using BIF_NEWDIALOGSTYLE
// This is a work-around, it enumerates through the child windows until the tree view of the dialog is hit and then
//	ensures the selected element is visible.
// This behavior is not apparent in XP or Vista and it is suggested to use the new FolderBrowserDialog
static BOOL CALLBACK EnumCallback(HWND hWndChild, LPARAM lParam)
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
int CALLBACK SelectDirCallBack(HWND hwnd, DWORD uMsg, DWORD lp, DWORD lpData) {
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

BOOL CALLBACK DirSelectProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch (uMsg) {
	case WM_INITDIALOG:
	{
		char StringVal[200], Path[_MAX_PATH], * read;
		BOOLEAN Value;
		int count;

		typedef struct {
			char* Default, * DirName;
			enum Directories select;
			int BTN_Def, BTN_Other, TXT;
		} FIELDS;

		FIELDS my_fields[5] = {
			{"Use Default Plugin", "Plugin", PluginDir, IDC_PLUGIN_DEFAULT, IDC_PLUGIN_OTHER, IDC_PLUGIN_DIR},
			{"Use Default Rom", "Rom", RomDir, IDC_ROM_DEFAULT, IDC_ROM_OTHER, IDC_ROM_DIR},
			{"Use Default Auto Save", "Auto Save", AutoSaveDir, IDC_AUTO_DEFAULT, IDC_AUTO_OTHER, IDC_AUTO_DIR},
			{"Use Default Instant Save", "Instant Save", InstantSaveDir, IDC_INSTANT_DEFAULT, IDC_INSTANT_OTHER, IDC_INSTANT_DIR},
			{"Use Default Snap Shot", "Snap Shot", SnapShotDir, IDC_SNAP_DEFAULT, IDC_SNAP_OTHER, IDC_SNAP_DIR}
		};

		for (count = 0; count < 5; count++) {
			Settings_Read(APPS_NAME, "Directories", my_fields[count].Default, "", &read);
			strncpy(StringVal, read, sizeof(StringVal));
			if (read) free(read);
			if (StringVal[0] != '\0') {
				Value = (strcmp(StringVal, STR_TRUE) == 0);
				Button_SetCheck(GetDlgItem(hDlg, Value ? my_fields[count].BTN_Def : my_fields[count].BTN_Other), BST_CHECKED);

				Settings_Read(APPS_NAME, "Directories", my_fields[count].DirName, "", &read);
				strncpy(Path, read, sizeof(Path));
				if (read) free(read);
				if (Path[0] == '\0')
					Settings_GetDirectory(my_fields[count].select, Path, sizeof(Path));
			}
			else {
				Button_SetCheck(GetDlgItem(hDlg, my_fields[count].BTN_Def), BST_CHECKED);
				Settings_GetDirectory(my_fields[count].select, Path, sizeof(Path));
			}
			SetDlgItemText(hDlg, my_fields[count].TXT, Path);
		}

		SetDlgItemText(hDlg, IDC_DIR_FRAME1, GS(DIR_PLUGIN));
		SetDlgItemText(hDlg, IDC_DIR_FRAME2, GS(DIR_ROM));
		SetDlgItemText(hDlg, IDC_DIR_FRAME3, GS(DIR_AUTO_SAVE));
		SetDlgItemText(hDlg, IDC_DIR_FRAME4, GS(DIR_INSTANT_SAVE));
		SetDlgItemText(hDlg, IDC_DIR_FRAME5, GS(DIR_SCREEN_SHOT));
		SetDlgItemText(hDlg, IDC_ROM_DEFAULT, GS(DIR_ROM_DEFAULT));
	}
	break;
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_SELECT_PLUGIN_DIR:
		case IDC_SELECT_ROM_DIR:
		case IDC_SELECT_INSTANT_DIR:
		case IDC_SELECT_AUTO_DIR:
		case IDC_SELECT_SNAP_DIR:
		{
			char Buffer[MAX_PATH], Directory[255], Title[255];
			LPITEMIDLIST pidl;
			BROWSEINFO bi;

			switch (LOWORD(wParam)) {
			case IDC_SELECT_PLUGIN_DIR:
				strcpy(Title, GS(DIR_SELECT_PLUGIN));
				Settings_GetDirectory(PluginDir, Directory, sizeof(Directory));
				break;
			case IDC_SELECT_ROM_DIR:
				Settings_GetDirectory(RomDir, Directory, sizeof(Directory));
				strcpy(Title, GS(DIR_SELECT_ROM));
				break;
			case IDC_SELECT_AUTO_DIR:
				Settings_GetDirectory(AutoSaveDir, Directory, sizeof(Directory));
				strcpy(Title, GS(DIR_SELECT_AUTO));
				break;
			case IDC_SELECT_INSTANT_DIR:
				Settings_GetDirectory(InstantSaveDir, Directory, sizeof(Directory));
				strcpy(Title, GS(DIR_SELECT_INSTANT));
				break;
			case IDC_SELECT_SNAP_DIR:
				Settings_GetDirectory(SnapShotDir, Directory, sizeof(Directory));;
				strcpy(Title, GS(DIR_SELECT_SCREEN));
				break;
			}

			bi.hwndOwner = hDlg;
			bi.pidlRoot = NULL;
			bi.pszDisplayName = Buffer;
			bi.lpszTitle = Title;
			bi.ulFlags = BIF_RETURNFSANCESTORS | BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
			bi.lpfn = (BFFCALLBACK)SelectDirCallBack;
			bi.lParam = (DWORD)Directory;
			if ((pidl = SHBrowseForFolder(&bi)) != NULL) {
				if (SHGetPathFromIDList(pidl, Directory)) {
					int len = strlen(Directory);

					if (Directory[len - 1] != '\\') { strcat(Directory, "\\"); }
					switch (LOWORD(wParam)) {
					case IDC_SELECT_PLUGIN_DIR:
						SetDlgItemText(hDlg, IDC_PLUGIN_DIR, Directory);
						SendMessage(GetDlgItem(hDlg, IDC_PLUGIN_DEFAULT), BM_SETCHECK, BST_UNCHECKED, 0);
						SendMessage(GetDlgItem(hDlg, IDC_PLUGIN_OTHER), BM_SETCHECK, BST_CHECKED, 0);
						break;
					case IDC_SELECT_ROM_DIR:
						SetDlgItemText(hDlg, IDC_ROM_DIR, Directory);
						SendMessage(GetDlgItem(hDlg, IDC_ROM_DEFAULT), BM_SETCHECK, BST_UNCHECKED, 0);
						SendMessage(GetDlgItem(hDlg, IDC_ROM_OTHER), BM_SETCHECK, BST_CHECKED, 0);
						break;
					case IDC_SELECT_INSTANT_DIR:
						SetDlgItemText(hDlg, IDC_INSTANT_DIR, Directory);
						SendMessage(GetDlgItem(hDlg, IDC_INSTANT_DEFAULT), BM_SETCHECK, BST_UNCHECKED, 0);
						SendMessage(GetDlgItem(hDlg, IDC_INSTANT_OTHER), BM_SETCHECK, BST_CHECKED, 0);
						break;
					case IDC_SELECT_AUTO_DIR:
						SetDlgItemText(hDlg, IDC_AUTO_DIR, Directory);
						SendMessage(GetDlgItem(hDlg, IDC_AUTO_DEFAULT), BM_SETCHECK, BST_UNCHECKED, 0);
						SendMessage(GetDlgItem(hDlg, IDC_AUTO_OTHER), BM_SETCHECK, BST_CHECKED, 0);
						break;
					case IDC_SELECT_SNAP_DIR:
						SetDlgItemText(hDlg, IDC_SNAP_DIR, Directory);
						SendMessage(GetDlgItem(hDlg, IDC_SNAP_DEFAULT), BM_SETCHECK, BST_UNCHECKED, 0);
						SendMessage(GetDlgItem(hDlg, IDC_SNAP_OTHER), BM_SETCHECK, BST_CHECKED, 0);
						break;
					}
				}
			}
		}
		break;
		}
		break;
	case WM_NOTIFY:
		if (((NMHDR FAR*) lParam)->code == PSN_APPLY) {
			BOOLEAN Value;
			char String[200];
			int count;

			typedef struct {
				char* DefaultDir, * Dir;
				int BTN_Default, TXT;
			} FIELDS;

			FIELDS my_fields[5] = {
				{"Use Default Plugin", "Plugin", IDC_PLUGIN_DEFAULT, IDC_PLUGIN_DIR},
				{"Use Default Rom", "Rom", IDC_ROM_DEFAULT, IDC_ROM_DIR},
				{"Use Default Auto Save", "Auto Save", IDC_AUTO_DEFAULT, IDC_AUTO_DIR},
				{"Use Default Instant Save", "Instant Save", IDC_INSTANT_DEFAULT, IDC_INSTANT_DIR},
				{"Use Default Snap Shot", "Snap Shot", IDC_SNAP_DEFAULT, IDC_SNAP_DIR}
			};

			for (count = 0; count < 5; count++) {
				Value = Button_GetState(GetDlgItem(hDlg, my_fields[count].BTN_Default)) == BST_CHECKED ? TRUE : FALSE;
				Settings_Write(APPS_NAME, "Directories", my_fields[count].DefaultDir, Value ? STR_TRUE : STR_FALSE);
				if (Value == FALSE) {
					GetDlgItemText(hDlg, my_fields[count].TXT, String, sizeof(String));
					Settings_Write(APPS_NAME, "Directories", my_fields[count].Dir, String);
				}
			}
		}
		break;
	default:
		return FALSE;
	}
	return TRUE;
}

BOOL PluginsChanged(HWND hDlg) {
	DWORD index;

	index = SendMessage(GetDlgItem(hDlg, RSP_LIST), CB_GETCURSEL, 0, 0);
	index = SendMessage(GetDlgItem(hDlg, RSP_LIST), CB_GETITEMDATA, (WPARAM)index, 0);
	if ((int)index >= 0) {
		if (_stricmp(RspDLL, PluginNames[index]) != 0) { return TRUE; }
	}

	index = SendMessage(GetDlgItem(hDlg, GFX_LIST), CB_GETCURSEL, 0, 0);
	index = SendMessage(GetDlgItem(hDlg, GFX_LIST), CB_GETITEMDATA, (WPARAM)index, 0);
	if ((int)index >= 0) {
		if (_stricmp(GfxDLL, PluginNames[index]) != 0) { return TRUE; }
	}

	index = SendMessage(GetDlgItem(hDlg, AUDIO_LIST), CB_GETCURSEL, 0, 0);
	index = SendMessage(GetDlgItem(hDlg, AUDIO_LIST), CB_GETITEMDATA, (WPARAM)index, 0);
	if ((int)index >= 0) {
		if (_stricmp(AudioDLL, PluginNames[index]) != 0) { return TRUE; }
	}

	index = SendMessage(GetDlgItem(hDlg, CONT_LIST), CB_GETCURSEL, 0, 0);
	index = SendMessage(GetDlgItem(hDlg, CONT_LIST), CB_GETITEMDATA, (WPARAM)index, 0);
	if ((int)index >= 0) {
		if (_stricmp(ControllerDLL, PluginNames[index]) != 0) { return TRUE; }
	}
	return FALSE;
}

void FreePluginList() {
	unsigned int count;
	for (count = 0; count < PluginCount; count++) {
		free(PluginNames[count]);
	}
	PluginCount = 0;
}

BOOL CALLBACK PluginSelectProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	char Plugin[300];
	HANDLE hLib;
	DWORD index;

	switch (uMsg) {
	case WM_INITDIALOG:
		SetupPluginScreen(hDlg);
		break;
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case RSP_LIST:
			if (HIWORD(wParam) == CBN_SELCHANGE) {
				index = SendMessage(GetDlgItem(hDlg, RSP_LIST), CB_GETCURSEL, 0, 0);
				if (index == CB_ERR) { break; } // *** Add in Build 53
				index = SendMessage(GetDlgItem(hDlg, RSP_LIST), CB_GETITEMDATA, (WPARAM)index, 0);

				Settings_GetDirectory(PluginDir, Plugin, sizeof(Plugin));
				strcat(Plugin, PluginNames[index]);
				hLib = LoadLibrary(Plugin);
				if (hLib == NULL) { DisplayError("%s %s", GS(MSG_FAIL_LOAD_PLUGIN), Plugin); }
				RSPDllAbout = (void(__cdecl*)(HWND))GetProcAddress(hLib, "DllAbout");
				EnableWindow(GetDlgItem(hDlg, RSP_ABOUT), RSPDllAbout != NULL ? TRUE : FALSE);
			}
			break;

		case GFX_LIST:
			if (HIWORD(wParam) == CBN_SELCHANGE) {
				index = SendMessage(GetDlgItem(hDlg, GFX_LIST), CB_GETCURSEL, 0, 0);
				if (index == CB_ERR) { break; } // *** Add in Build 53
				index = SendMessage(GetDlgItem(hDlg, GFX_LIST), CB_GETITEMDATA, (WPARAM)index, 0);

				Settings_GetDirectory(PluginDir, Plugin, sizeof(Plugin));
				strcat(Plugin, PluginNames[index]);
				hLib = LoadLibrary(Plugin);
				if (hLib == NULL) { DisplayError("%s %s", GS(MSG_FAIL_LOAD_PLUGIN), Plugin); }
				GFXDllAbout = (void(__cdecl*)(HWND))GetProcAddress(hLib, "DllAbout");
				EnableWindow(GetDlgItem(hDlg, GFX_ABOUT), GFXDllAbout != NULL ? TRUE : FALSE);
			}
			break;
		case AUDIO_LIST:
			if (HIWORD(wParam) == CBN_SELCHANGE) {
				index = SendMessage(GetDlgItem(hDlg, AUDIO_LIST), CB_GETCURSEL, 0, 0);
				if (index == CB_ERR) { break; } // *** Add in Build 53
				index = SendMessage(GetDlgItem(hDlg, AUDIO_LIST), CB_GETITEMDATA, (WPARAM)index, 0);

				Settings_GetDirectory(PluginDir, Plugin, sizeof(Plugin));
				strcat(Plugin, PluginNames[index]);
				hLib = LoadLibrary(Plugin);
				if (hLib == NULL) { DisplayError("%s %s", GS(MSG_FAIL_LOAD_PLUGIN), Plugin); }
				AiDllAbout = (void(__cdecl*)(HWND))GetProcAddress(hLib, "DllAbout");
				EnableWindow(GetDlgItem(hDlg, GFX_ABOUT), GFXDllAbout != NULL ? TRUE : FALSE);
			}
			break;
		case CONT_LIST:
			if (HIWORD(wParam) == CBN_SELCHANGE) {
				index = SendMessage(GetDlgItem(hDlg, CONT_LIST), CB_GETCURSEL, 0, 0);
				if (index == CB_ERR) { break; } // *** Add in Build 53
				index = SendMessage(GetDlgItem(hDlg, CONT_LIST), CB_GETITEMDATA, (WPARAM)index, 0);

				Settings_GetDirectory(PluginDir, Plugin, sizeof(Plugin));
				strcat(Plugin, PluginNames[index]);
				hLib = LoadLibrary(Plugin);
				if (hLib == NULL) { DisplayError("%s %s", GS(MSG_FAIL_LOAD_PLUGIN), Plugin); }
				ContDllAbout = (void(__cdecl*)(HWND))GetProcAddress(hLib, "DllAbout");
				EnableWindow(GetDlgItem(hDlg, CONT_ABOUT), ContDllAbout != NULL ? TRUE : FALSE);
			}
			break;
		case RSP_ABOUT: RSPDllAbout(hDlg); break;
		case GFX_ABOUT: GFXDllAbout(hDlg); break;
		case CONT_ABOUT: ContDllAbout(hDlg); break;
		case AUDIO_ABOUT: AiDllAbout(hDlg); break;
		}
		break;
	case WM_NOTIFY:
		if (((NMHDR FAR*) lParam)->code == PSN_APPLY) {
			int index;
			char String[200];

			if (PluginsChanged(hDlg) == FALSE) { FreePluginList(); break; }

			if (CPURunning) {
				int Response;

				ShowWindow(hDlg, SW_HIDE);
				Response = MessageBox(hMainWindow, GS(MSG_PLUGIN_CHANGE), GS(MSG_PLUGIN_CHANGE_TITLE), MB_YESNO | MB_ICONQUESTION);
				if (Response != IDYES) { FreePluginList(); break; }
			}

			index = SendMessage(GetDlgItem(hDlg, RSP_LIST), CB_GETCURSEL, 0, 0);
			index = SendMessage(GetDlgItem(hDlg, RSP_LIST), CB_GETITEMDATA, (WPARAM)index, 0);
			sprintf(String, "%s", PluginNames[index]);
			Settings_Write(APPS_NAME, "Plugins", "RSP", String);

			index = SendMessage(GetDlgItem(hDlg, GFX_LIST), CB_GETCURSEL, 0, 0);
			index = SendMessage(GetDlgItem(hDlg, GFX_LIST), CB_GETITEMDATA, (WPARAM)index, 0);
			sprintf(String, "%s", PluginNames[index]);
			Settings_Write(APPS_NAME, "Plugins", "Graphics", String);

			index = SendMessage(GetDlgItem(hDlg, AUDIO_LIST), CB_GETCURSEL, 0, 0);
			index = SendMessage(GetDlgItem(hDlg, AUDIO_LIST), CB_GETITEMDATA, (WPARAM)index, 0);
			sprintf(String, "%s", PluginNames[index]);
			Settings_Write(APPS_NAME, "Plugins", "Audio", String);

			index = SendMessage(GetDlgItem(hDlg, CONT_LIST), CB_GETCURSEL, 0, 0);
			index = SendMessage(GetDlgItem(hDlg, CONT_LIST), CB_GETITEMDATA, (WPARAM)index, 0);
			sprintf(String, "%s", PluginNames[index]);
			Settings_Write(APPS_NAME, "Plugins", "Controller", String);

			if (CPURunning) {
				CloseCpu();
				ShutdownPlugins();
				SetupPlugins(hMainWindow);
				StartEmulation();
			}
			else {
				ShutdownPlugins();
				SetupPlugins(hMainWindow);
			}
			FreePluginList();
		}
		if (((NMHDR FAR*) lParam)->code == PSN_RESET) {
			FreePluginList();
		}
		break;
	default:
		return FALSE;
	}
	return TRUE;
}

void RomAddFieldToList(HWND hDlg, char* Name, int Pos, int ID) {
	int listCount, index;

	if (Pos < 0) {
		index = SendDlgItemMessage(hDlg, IDC_AVALIABLE, LB_ADDSTRING, 0, (LPARAM)Name);
		SendDlgItemMessage(hDlg, IDC_AVALIABLE, LB_SETITEMDATA, index, ID);
		return;
	}
	listCount = SendDlgItemMessage(hDlg, IDC_USING, LB_GETCOUNT, 0, 0);
	if (Pos > listCount) { Pos = listCount; }
	index = SendDlgItemMessage(hDlg, IDC_USING, LB_INSERTSTRING, Pos, (LPARAM)Name);
	SendDlgItemMessage(hDlg, IDC_USING, LB_SETITEMDATA, index, ID);
}

BOOL CALLBACK RomBrowserProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch (uMsg) {
	case WM_INITDIALOG:
		if (RomBrowser) { SendMessage(GetDlgItem(hDlg, IDC_USE_ROMBROWSER), BM_SETCHECK, BST_CHECKED, 0); }
		if (Recursion) { SendMessage(GetDlgItem(hDlg, IDC_RECURSION), BM_SETCHECK, BST_CHECKED, 0); }
		{
			int count;

			for (count = 0; count < NoOfFields; count++) {
				RomAddFieldToList(hDlg, GS(RomBrowserFields[count].LangID), RomBrowserFields[count].Pos, count);
			}
		}
		{
			char String[256];
			sprintf(String, "%d", RomsToRemember);
			SetDlgItemText(hDlg, IDC_REMEMBER, String);
			sprintf(String, "%d", RomDirsToRemember);
			SetDlgItemText(hDlg, IDC_REMEMBERDIR, String);

			SetDlgItemText(hDlg, IDC_ROMSEL_TEXT1, GS(RB_MAX_ROMS));
			SetDlgItemText(hDlg, IDC_ROMSEL_TEXT2, GS(RB_ROMS));
			SetDlgItemText(hDlg, IDC_ROMSEL_TEXT3, GS(RB_MAX_DIRS));
			SetDlgItemText(hDlg, IDC_ROMSEL_TEXT4, GS(RB_DIRS));
			SetDlgItemText(hDlg, IDC_USE_ROMBROWSER, GS(RB_USE));
			SetDlgItemText(hDlg, IDC_RECURSION, GS(RB_DIR_RECURSION));
			SetDlgItemText(hDlg, IDC_ROMSEL_TEXT5, GS(RB_AVALIABLE_FIELDS));
			SetDlgItemText(hDlg, IDC_ROMSEL_TEXT6, GS(RB_SHOW_FIELDS));
			SetDlgItemText(hDlg, IDC_ADD, GS(RB_ADD));
			SetDlgItemText(hDlg, IDC_REMOVE, GS(RB_REMOVE));
			SetDlgItemText(hDlg, IDC_UP, GS(RB_UP));
			SetDlgItemText(hDlg, IDC_DOWN, GS(RB_DOWN));
		}
		break;
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_ADD:
		{
			char String[100];
			int index, listCount, Data;

			index = SendMessage(GetDlgItem(hDlg, IDC_AVALIABLE), LB_GETCURSEL, 0, 0);
			if (index < 0) { break; }
			SendMessage(GetDlgItem(hDlg, IDC_AVALIABLE), LB_GETTEXT, index, (LPARAM)String);
			Data = SendMessage(GetDlgItem(hDlg, IDC_AVALIABLE), LB_GETITEMDATA, index, 0);
			SendDlgItemMessage(hDlg, IDC_AVALIABLE, LB_DELETESTRING, index, 0);
			listCount = SendDlgItemMessage(hDlg, IDC_AVALIABLE, LB_GETCOUNT, 0, 0);
			if (index >= listCount) { index -= 1; }
			SendDlgItemMessage(hDlg, IDC_AVALIABLE, LB_SETCURSEL, index, 0);
			index = SendDlgItemMessage(hDlg, IDC_USING, LB_ADDSTRING, 0, (LPARAM)String);
			SendDlgItemMessage(hDlg, IDC_USING, LB_SETITEMDATA, index, Data);
		}
		break;
		case IDC_REMOVE:
		{
			char String[100];
			int index, listCount, Data;

			index = SendMessage(GetDlgItem(hDlg, IDC_USING), LB_GETCURSEL, 0, 0);
			if (index < 0) { break; }
			SendMessage(GetDlgItem(hDlg, IDC_USING), LB_GETTEXT, index, (LPARAM)String);
			Data = SendMessage(GetDlgItem(hDlg, IDC_USING), LB_GETITEMDATA, index, 0);
			SendDlgItemMessage(hDlg, IDC_USING, LB_DELETESTRING, index, 0);
			listCount = SendDlgItemMessage(hDlg, IDC_USING, LB_GETCOUNT, 0, 0);
			if (index >= listCount) { index -= 1; }
			SendDlgItemMessage(hDlg, IDC_USING, LB_SETCURSEL, index, 0);
			index = SendDlgItemMessage(hDlg, IDC_AVALIABLE, LB_ADDSTRING, 0, (LPARAM)String);
			SendDlgItemMessage(hDlg, IDC_AVALIABLE, LB_SETITEMDATA, index, Data);
		}
		break;
		case IDC_UP:
		{
			char String[100];
			int index, Data;

			index = SendMessage(GetDlgItem(hDlg, IDC_USING), LB_GETCURSEL, 0, 0);
			if (index <= 0) { break; }
			SendMessage(GetDlgItem(hDlg, IDC_USING), LB_GETTEXT, index, (LPARAM)String);
			Data = SendMessage(GetDlgItem(hDlg, IDC_USING), LB_GETITEMDATA, index, 0);
			SendDlgItemMessage(hDlg, IDC_USING, LB_DELETESTRING, index, 0);
			index = SendDlgItemMessage(hDlg, IDC_USING, LB_INSERTSTRING, index - 1, (LPARAM)String);
			SendDlgItemMessage(hDlg, IDC_USING, LB_SETCURSEL, index, 0);
			SendDlgItemMessage(hDlg, IDC_USING, LB_SETITEMDATA, index, Data);
		}
		break;
		case IDC_DOWN:
		{
			char String[100];
			int index, listCount, Data;

			index = SendMessage(GetDlgItem(hDlg, IDC_USING), LB_GETCURSEL, 0, 0);
			listCount = SendDlgItemMessage(hDlg, IDC_USING, LB_GETCOUNT, 0, 0);
			if ((index + 1) == listCount) { break; }
			SendMessage(GetDlgItem(hDlg, IDC_USING), LB_GETTEXT, index, (LPARAM)String);
			Data = SendMessage(GetDlgItem(hDlg, IDC_USING), LB_GETITEMDATA, index, 0);
			SendDlgItemMessage(hDlg, IDC_USING, LB_DELETESTRING, index, 0);
			index = SendDlgItemMessage(hDlg, IDC_USING, LB_INSERTSTRING, index + 1, (LPARAM)String);
			SendDlgItemMessage(hDlg, IDC_USING, LB_SETCURSEL, index, 0);
			SendDlgItemMessage(hDlg, IDC_USING, LB_SETITEMDATA, index, Data);
		}
		break;
		}
		break;

	case WM_NOTIFY:
		if (((NMHDR FAR*) lParam)->code == PSN_APPLY) {
			char String[10];
			int index, listCount, Pos, recursionChanged = Recursion;

			RomBrowser = SendMessage(GetDlgItem(hDlg, IDC_USE_ROMBROWSER), BM_GETSTATE, 0, 0) == BST_CHECKED ? TRUE : FALSE;
			Recursion = SendMessage(GetDlgItem(hDlg, IDC_RECURSION), BM_GETSTATE, 0, 0) == BST_CHECKED ? TRUE : FALSE;

			// Recursion is now being used, make note of it
			if (recursionChanged != Recursion) recursionChanged = 1;

			Settings_Write(APPS_NAME, STR_SETTINGS, STR_USERB, RomBrowser ? STR_TRUE : STR_FALSE);
			Settings_Write(APPS_NAME, STR_SETTINGS, STR_USERECUR, Recursion ? STR_TRUE : STR_FALSE);

			RomsToRemember = GetDlgItemInt(hDlg, IDC_REMEMBER, NULL, FALSE);
			if (RomsToRemember < 0) { RomsToRemember = 0; }
			if (RomsToRemember > 10) { RomsToRemember = 10; }
			sprintf(String, "%d", RomsToRemember);
			Settings_Write(APPS_NAME, STR_SETTINGS, STR_ROMSREMEMBER, String);

			RomDirsToRemember = GetDlgItemInt(hDlg, IDC_REMEMBERDIR, NULL, FALSE);
			if (RomDirsToRemember < 0) { RomDirsToRemember = 0; }
			if (RomDirsToRemember > 10) { RomDirsToRemember = 10; }
			sprintf(String, "%d", RomDirsToRemember);
			Settings_Write(APPS_NAME, STR_SETTINGS, STR_ROMDIRSREMEMBER, String);

			SaveRomBrowserColumnInfo(); // Any column width changes get saved
			listCount = SendDlgItemMessage(hDlg, IDC_USING, LB_GETCOUNT, 0, 0);
			for (Pos = 0; Pos < listCount; Pos++) {
				index = SendMessage(GetDlgItem(hDlg, IDC_USING), LB_GETITEMDATA, Pos, 0);
				SaveRomBrowserColumnPosition(index, Pos);
			}
			listCount = SendDlgItemMessage(hDlg, IDC_AVALIABLE, LB_GETCOUNT, 0, 0);
			strcpy(String, "-1");
			for (Pos = 0; Pos < listCount; Pos++) {
				index = SendMessage(GetDlgItem(hDlg, IDC_AVALIABLE), LB_GETITEMDATA, Pos, 0);
				SaveRomBrowserColumnPosition(index, -1);
			}
			LoadRomBrowserColumnInfo();
			ResetRomBrowserColomuns();
			if (RomBrowser) { if (recursionChanged == 1) RefreshRomBrowser(); ShowRomList(hMainWindow); }
			if (!RomBrowser) { HideRomBrowser(); }

			RemoveRecentList(hMainWindow);
		}
		break;
	default:
		return FALSE;
	}
	return TRUE;
}

BOOL CALLBACK RomNotesProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch (uMsg) {
	case WM_INITDIALOG:
	{
		char Identifier[100], RomStatus[100], Notes[800], * read, * token;
		int i, index;
		BOOLEAN cb_selected = FALSE;

		SetDlgItemText(hDlg, IDC_STATUS_TEXT, GS(NOTE_STATUS));
		SetDlgItemText(hDlg, IDC_CORE, GS(NOTE_CORE));
		SetDlgItemText(hDlg, IDC_PLUGIN, GS(NOTE_PLUGIN));

		RomID(Identifier, RomHeader);

		// Fetch the status of the Rom (Compatible, Bad Rom, etc...)
		Settings_Read(RDS_NAME, Identifier, "Status", Default_RomStatus, &read);
		strncpy(RomStatus, read, sizeof(RomStatus));
		if (read) free(read);

		Settings_Read(RDS_NAME, Identifier, "Core Note", "", &read);
		strncpy(Notes, read, sizeof(Notes));
		if (read) free(read);
		SetDlgItemText(hDlg, IDC_CORE_NOTES, Notes);

		Settings_Read(RDS_NAME, Identifier, "Plugin Note", "", &read);
		strncpy(Notes, read, sizeof(Notes));
		if (read) free(read);
		SetDlgItemText(hDlg, IDC_PLUGIN_NOTE, Notes);

		Settings_FetchKeyNames(RDS_NAME, "Rom Status", &read);
		for (token = strtok(read, ","); token != NULL; token = strtok(NULL, ",")) {
			// Ignore these as they are special colors based on context
			if (strstr(token, ".Sel") == NULL && strstr(token, ".SelText") == NULL && strstr(token, ".AutoFullScreen") == NULL) {
				index = ComboBox_AddString(GetDlgItem(hDlg, IDC_STATUS), token);

				// Set the selected item to be what is in RomStatus
				if (!cb_selected && strcmp(token, RomStatus) == 0) {
					ComboBox_SetCurSel(GetDlgItem(hDlg, IDC_STATUS), index);
					cb_selected = TRUE;
				}
			}
		}
		if (read) free(read);

		if (strlen(RomFullName) == 0) {
			EnableWindow(GetDlgItem(hDlg, IDC_STATUS_TEXT), FALSE);
			EnableWindow(GetDlgItem(hDlg, IDC_STATUS), FALSE);
			EnableWindow(GetDlgItem(hDlg, IDC_CORE), FALSE);
			EnableWindow(GetDlgItem(hDlg, IDC_CORE_NOTES), FALSE);
			EnableWindow(GetDlgItem(hDlg, IDC_PLUGIN), FALSE);
			EnableWindow(GetDlgItem(hDlg, IDC_PLUGIN_NOTE), FALSE);
		}
	}
	break;
	case WM_NOTIFY:
		if (((NMHDR FAR*) lParam)->code == PSN_APPLY) {
			char Identifier[100], string[200];

			RomID(Identifier, RomHeader);

			GetWindowText(GetDlgItem(hDlg, IDC_STATUS), string, sizeof(string));
			if (strlen(string) == 0)
				strcpy(string, Default_RomStatus);
			Settings_Write(RDS_NAME, Identifier, "Status", string);

			Settings_Delete(RDS_NAME, Identifier, "Core Note");
			Settings_Delete(RDS_NAME, Identifier, "Plugin Note");

			GetWindowText(GetDlgItem(hDlg, IDC_CORE_NOTES), string, sizeof(string));
			if (strlen(string) != 0)
				Settings_Write(RDS_NAME, Identifier, "Core Note", string);

			GetWindowText(GetDlgItem(hDlg, IDC_PLUGIN_NOTE), string, sizeof(string));
			if (strlen(string) != 0)
				Settings_Write(RDS_NAME, Identifier, "Plugin Note", string);
		}
		break;
	default:
		return FALSE;
	}
	return TRUE;
}

BOOL CALLBACK RomSettingsProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	char String[256];
	int indx;

	switch (uMsg) {
	case WM_INITDIALOG:
		ReadRomOptions();
		char junk[20];

		SetDlgItemText(hDlg, IDC_CPU_TYPE_TEXT, GS(ROM_CPU_STYLE));
		SetDlgItemText(hDlg, IDC_SELFMOD_TEXT, GS(ROM_SMCM));
		SetDlgItemText(hDlg, IDC_MEMORY_SIZE_TEXT, GS(ROM_MEM_SIZE));
		SetDlgItemText(hDlg, IDC_BLOCK_LINKING_TEXT, GS(ROM_ABL));
		SetDlgItemText(hDlg, IDC_SAVE_TYPE_TEXT, GS(ROM_SAVE_TYPE));
		SetDlgItemText(hDlg, IDC_COUNTFACT_TEXT, GS(ROM_COUNTER_FACTOR));

		sprintf(junk, "%d", RomModVIS);
		SetDlgItemText(hDlg, IDC_VIS, junk);

		AddDropDownItem(hDlg, IDC_CPU_TYPE, ROM_DEFAULT, CPU_Default, &RomCPUType);
		AddDropDownItem(hDlg, IDC_CPU_TYPE, CORE_INTERPTER, CPU_Interpreter, &RomCPUType);
		AddDropDownItem(hDlg, IDC_CPU_TYPE, CORE_RECOMPILER, CPU_Recompiler, &RomCPUType);
		AddDropDownItem(hDlg, IDC_CPU_TYPE, CORE_SYNC, CPU_SyncCores, &RomCPUType);

		AddDropDownItem(hDlg, IDC_SELFMOD, ROM_DEFAULT, ModCode_Default, &RomSelfMod);
		AddDropDownItem(hDlg, IDC_SELFMOD, SMCM_NONE, ModCode_None, &RomSelfMod);
		AddDropDownItem(hDlg, IDC_SELFMOD, SMCM_CACHE, ModCode_Cache, &RomSelfMod);
		AddDropDownItem(hDlg, IDC_SELFMOD, SMCM_PROECTED, ModCode_ProtectedMemory, &RomSelfMod);
		AddDropDownItem(hDlg, IDC_SELFMOD, SMCM_CHECK_MEM, ModCode_CheckMemoryCache, &RomSelfMod);
		AddDropDownItem(hDlg, IDC_SELFMOD, SMCM_CHANGE_MEM, ModCode_ChangeMemory, &RomSelfMod);
		AddDropDownItem(hDlg, IDC_SELFMOD, SMCM_CHECK_ADV, ModCode_CheckMemory2, &RomSelfMod);

		AddDropDownItem(hDlg, IDC_RDRAM_SIZE, RDRAM_4MB, 0x400000, &RomRamSize);
		AddDropDownItem(hDlg, IDC_RDRAM_SIZE, RDRAM_8MB, 0x800000, &RomRamSize);

		AddDropDownItem(hDlg, IDC_BLOCK_LINKING, ROM_DEFAULT, -1, &RomUseLinking);
		AddDropDownItem(hDlg, IDC_BLOCK_LINKING, ABL_ON, 0, &RomUseLinking);
		AddDropDownItem(hDlg, IDC_BLOCK_LINKING, ABL_OFF, 1, &RomUseLinking);

		AddDropDownItem(hDlg, IDC_SAVE_TYPE, SAVE_FIRST_USED, Auto, &RomSaveUsing);
		AddDropDownItem(hDlg, IDC_SAVE_TYPE, SAVE_4K_EEPROM, Eeprom_4K, &RomSaveUsing);
		AddDropDownItem(hDlg, IDC_SAVE_TYPE, SAVE_16K_EEPROM, Eeprom_16K, &RomSaveUsing);
		AddDropDownItem(hDlg, IDC_SAVE_TYPE, SAVE_SRAM, Sram, &RomSaveUsing);
		AddDropDownItem(hDlg, IDC_SAVE_TYPE, SAVE_FLASHRAM, FlashRam, &RomSaveUsing);

		AddDropDownItem(hDlg, IDC_COUNTFACT, ROM_DEFAULT, -1, &RomCF);
		AddDropDownItem(hDlg, IDC_COUNTFACT, NUMBER_1, 1, &RomCF);
		AddDropDownItem(hDlg, IDC_COUNTFACT, NUMBER_2, 2, &RomCF);
		AddDropDownItem(hDlg, IDC_COUNTFACT, NUMBER_3, 3, &RomCF);
		AddDropDownItem(hDlg, IDC_COUNTFACT, NUMBER_4, 4, &RomCF);
		AddDropDownItem(hDlg, IDC_COUNTFACT, NUMBER_5, 5, &RomCF);
		AddDropDownItem(hDlg, IDC_COUNTFACT, NUMBER_6, 6, &RomCF);

		SetFlagControl(hDlg, &RomUseLargeBuffer, IDC_LARGE_COMPILE_BUFFER, ROM_LARGE_BUFFER);
		SetFlagControl(hDlg, &RomUseTlb, IDC_USE_TLB, ROM_USE_TLB);
		SetFlagControl(hDlg, &RomUseCache, IDC_ROM_REGCACHE, ROM_REG_CACHE);
		SetFlagControl(hDlg, &RomDelaySI, IDC_DELAY_SI, ROM_DELAY_SI);
		SetFlagControl(hDlg, &RomEmulateAI, IDC_EMULATE_AI, ROM_EMULATE_AI);
		SetFlagControl(hDlg, &RomAudioSignal, IDC_AUDIO_SIGNAL, ROM_AUDIO_SIGNAL);
		SetFlagControl(hDlg, &RomSPHack, IDC_ROM_SPHACK, ROM_SP_HACK);

		if (strlen(RomFullName) == 0) {
			EnableWindow(GetDlgItem(hDlg, IDC_MEMORY_SIZE_TEXT), FALSE);
			EnableWindow(GetDlgItem(hDlg, IDC_RDRAM_SIZE), FALSE);
			EnableWindow(GetDlgItem(hDlg, IDC_SAVE_TYPE_TEXT), FALSE);
			EnableWindow(GetDlgItem(hDlg, IDC_SAVE_TYPE), FALSE);
			EnableWindow(GetDlgItem(hDlg, IDC_COUNTFACT_TEXT), FALSE);
			EnableWindow(GetDlgItem(hDlg, IDC_COUNTFACT), FALSE);
			EnableWindow(GetDlgItem(hDlg, IDC_CPU_TYPE_TEXT), FALSE);
			EnableWindow(GetDlgItem(hDlg, IDC_CPU_TYPE), FALSE);
			EnableWindow(GetDlgItem(hDlg, IDC_SELFMOD_TEXT), FALSE);
			EnableWindow(GetDlgItem(hDlg, IDC_SELFMOD), FALSE);
			EnableWindow(GetDlgItem(hDlg, IDC_USE_TLB), FALSE);
			EnableWindow(GetDlgItem(hDlg, IDC_DELAY_SI), FALSE);
			EnableWindow(GetDlgItem(hDlg, IDC_EMULATE_AI), FALSE);
			EnableWindow(GetDlgItem(hDlg, IDC_ROM_SPHACK), FALSE);
			EnableWindow(GetDlgItem(hDlg, IDC_ROM_REGCACHE), FALSE);
			EnableWindow(GetDlgItem(hDlg, IDC_BLOCK_LINKING_TEXT), FALSE);
			EnableWindow(GetDlgItem(hDlg, IDC_BLOCK_LINKING), FALSE);
			EnableWindow(GetDlgItem(hDlg, IDC_AUDIO_SIGNAL), FALSE);
			EnableWindow(GetDlgItem(hDlg, IDC_LARGE_COMPILE_BUFFER), FALSE);
			EnableWindow(GetDlgItem(hDlg, IDC_NOTES), FALSE);
			EnableWindow(GetDlgItem(hDlg, IDC_VIS), FALSE);
			EnableWindow(GetDlgItem(hDlg, IDC_VIS_TEXT), FALSE);
			EnableWindow(GetDlgItem(hDlg, IDC_VIS_RESET), FALSE);
		}
		break;
	case WM_NOTIFY:
		if (((NMHDR FAR*) lParam)->code == PSN_APPLY) {
			char Identifier[100];
			char* junk;
			int junklen;

			if (strlen(RomFullName) == 0) { break; }

			LoadRomRecalcCRCs(CurrentFileName, (DWORD*)&RomHeader[0x10], (DWORD*)&RomHeader[0x14]);

			RomID(Identifier, RomHeader);

			// Do not write to file if there is no note
			GetDlgItemText(hDlg, IDC_NOTES, String, sizeof(String));
			if (strlen(String) != 0) {
				Settings_Write(RDN_NAME, Identifier, "Name", RomFullName);
				Settings_Write(RDN_NAME, Identifier, "Note", String);
			}

			indx = SendMessage(GetDlgItem(hDlg, IDC_RDRAM_SIZE), CB_GETCURSEL, 0, 0);
			RomRamSize = SendMessage(GetDlgItem(hDlg, IDC_RDRAM_SIZE), CB_GETITEMDATA, indx, 0);
			indx = SendMessage(GetDlgItem(hDlg, IDC_SAVE_TYPE), CB_GETCURSEL, 0, 0);
			RomSaveUsing = SendMessage(GetDlgItem(hDlg, IDC_SAVE_TYPE), CB_GETITEMDATA, indx, 0);
			indx = SendMessage(GetDlgItem(hDlg, IDC_COUNTFACT), CB_GETCURSEL, 0, 0);
			RomCF = SendMessage(GetDlgItem(hDlg, IDC_COUNTFACT), CB_GETITEMDATA, indx, 0);
			indx = SendMessage(GetDlgItem(hDlg, IDC_CPU_TYPE), CB_GETCURSEL, 0, 0);
			RomCPUType = SendMessage(GetDlgItem(hDlg, IDC_CPU_TYPE), CB_GETITEMDATA, indx, 0);
			indx = SendMessage(GetDlgItem(hDlg, IDC_SELFMOD), CB_GETCURSEL, 0, 0);
			RomSelfMod = SendMessage(GetDlgItem(hDlg, IDC_SELFMOD), CB_GETITEMDATA, indx, 0);
			indx = SendMessage(GetDlgItem(hDlg, IDC_BLOCK_LINKING), CB_GETCURSEL, 0, 0);
			RomUseLinking = SendMessage(GetDlgItem(hDlg, IDC_BLOCK_LINKING), CB_GETITEMDATA, indx, 0);
			RomDelaySI = SendMessage(GetDlgItem(hDlg, IDC_DELAY_SI), BM_GETSTATE, 0, 0) == BST_CHECKED ? TRUE : FALSE;
			RomEmulateAI = SendMessage(GetDlgItem(hDlg, IDC_EMULATE_AI), BM_GETSTATE, 0, 0) == BST_CHECKED ? TRUE : FALSE;
			RomAudioSignal = SendMessage(GetDlgItem(hDlg, IDC_AUDIO_SIGNAL), BM_GETSTATE, 0, 0) == BST_CHECKED ? TRUE : FALSE;
			RomSPHack = SendMessage(GetDlgItem(hDlg, IDC_ROM_SPHACK), BM_GETSTATE, 0, 0) == BST_CHECKED ? TRUE : FALSE;
			RomUseTlb = SendMessage(GetDlgItem(hDlg, IDC_USE_TLB), BM_GETSTATE, 0, 0) == BST_CHECKED ? TRUE : FALSE;
			RomUseCache = SendMessage(GetDlgItem(hDlg, IDC_ROM_REGCACHE), BM_GETSTATE, 0, 0) == BST_CHECKED ? TRUE : FALSE;
			RomUseLargeBuffer = SendMessage(GetDlgItem(hDlg, IDC_LARGE_COMPILE_BUFFER), BM_GETSTATE, 0, 0) == BST_CHECKED ? TRUE : FALSE;

			// Sanity check for VI/s setting, not less than 500 and not more than 4500
			RomModVIS = 1500;
			junklen = GetWindowTextLength(GetDlgItem(hDlg, IDC_VIS)) + 1;
			junk = (char*)malloc(sizeof(char) * junklen);

			if (junk != NULL) {
				GetWindowText(GetDlgItem(hDlg, IDC_VIS), junk, junklen);
				RomModVIS = atoi(junk);
				free(junk);
			}
			if (RomModVIS < 500 || RomModVIS > 4500)
				RomModVIS = 1500;

			SaveRomOptions();
		}
		break;
	case WM_COMMAND:
		if (LOWORD(wParam) == IDC_VIS_RESET) {
			SetWindowText(GetDlgItem(hDlg, IDC_VIS), "1500");
		}
		break;
	default:
		return FALSE;
	}
	return TRUE;
}

BOOL CALLBACK ShellIntegrationProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch (uMsg) {
	case WM_INITDIALOG:
		SetDlgItemText(hDlg, IDC_SHELL_INT_TEXT, GS(SHELL_TEXT));
		if (TestExtensionRegistered(".v64")) { SendMessage(GetDlgItem(hDlg, IDC_V64), BM_SETCHECK, BST_CHECKED, 0); }
		if (TestExtensionRegistered(".z64")) { SendMessage(GetDlgItem(hDlg, IDC_Z64), BM_SETCHECK, BST_CHECKED, 0); }
		if (TestExtensionRegistered(".n64")) { SendMessage(GetDlgItem(hDlg, IDC_N64), BM_SETCHECK, BST_CHECKED, 0); }
		if (TestExtensionRegistered(".rom")) { SendMessage(GetDlgItem(hDlg, IDC_ROM), BM_SETCHECK, BST_CHECKED, 0); }
		if (TestExtensionRegistered(".jap")) { SendMessage(GetDlgItem(hDlg, IDC_JAP), BM_SETCHECK, BST_CHECKED, 0); }
		if (TestExtensionRegistered(".pal")) { SendMessage(GetDlgItem(hDlg, IDC_PAL), BM_SETCHECK, BST_CHECKED, 0); }
		if (TestExtensionRegistered(".usa")) { SendMessage(GetDlgItem(hDlg, IDC_USA), BM_SETCHECK, BST_CHECKED, 0); }
		break;
	case WM_NOTIFY:
		if (((NMHDR FAR*) lParam)->code == PSN_APPLY) {
			RegisterExtension(".v64", SendMessage(GetDlgItem(hDlg, IDC_V64), BM_GETSTATE, 0, 0) == BST_CHECKED ? TRUE : FALSE);
			RegisterExtension(".z64", SendMessage(GetDlgItem(hDlg, IDC_Z64), BM_GETSTATE, 0, 0) == BST_CHECKED ? TRUE : FALSE);
			RegisterExtension(".n64", SendMessage(GetDlgItem(hDlg, IDC_N64), BM_GETSTATE, 0, 0) == BST_CHECKED ? TRUE : FALSE);
			RegisterExtension(".rom", SendMessage(GetDlgItem(hDlg, IDC_ROM), BM_GETSTATE, 0, 0) == BST_CHECKED ? TRUE : FALSE);
			RegisterExtension(".jap", SendMessage(GetDlgItem(hDlg, IDC_JAP), BM_GETSTATE, 0, 0) == BST_CHECKED ? TRUE : FALSE);
			RegisterExtension(".pal", SendMessage(GetDlgItem(hDlg, IDC_PAL), BM_GETSTATE, 0, 0) == BST_CHECKED ? TRUE : FALSE);
			RegisterExtension(".usa", SendMessage(GetDlgItem(hDlg, IDC_USA), BM_GETSTATE, 0, 0) == BST_CHECKED ? TRUE : FALSE);
		}
		break;
	default:
		return FALSE;
	}
	return TRUE;
}
