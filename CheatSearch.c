/* Witten 20110303 ****************************************/
/**********************************************************/

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <stdio.h>
#include <winuser.h>
#include "main.h"
#include "CPU.h"
#include "debugger.h"
#include "resource_cheat.h"
#include "rom.h"
#include "cheats.h"
#include "cheatsearch.h"
#include "Memory.h"
#include "MemDump.h"
#include "Cheats.h"
#include "CheatSearch_Input.h"
#include "RomTools_Common.h"
#include "CheatSearch_Search.h"
#include "CheatSearch_Dev.h"

const char *N64GSCODETYPES8BIT[] = { "8-bit Constant Write (80)", "80",
	"8-bit Uncached Write (A0)", "A0",
	"8-bit GS Button (88)", "88",
	"8-Bit Equal To Activator (D0)", "D0",
	"8-Bit Different To Activator (D2)", "D2" };

const char *N64GSCODETYPES16BIT[] = { "16-bit Constant Write (81)", "81",
	"16-bit Uncached Write (A1)", "A1",
	"16-bit GS Button (89)", "89",
	"16-Bit Equal To Activator (D1)", "D1",
	"16-Bit Different To Activator (D3)", "D3" };

#define LISTVIEW_MAXSHOWN 4096


/////////////////////////////////////
// Start of Function Prototypes
/////////////////////////////////////
void Search (HWND hDlg);
void InitSearchFormatList(HWND hDlg);
void InitSearchNumBitsList(HWND hDlg);
void InitSearchResultList(HWND hDlg);
void PopulateSearchResultList(HWND hDlg);
void InitCheatCreateList(HWND hDlg);
void UpdateCheatCreateList(HWND hDlg);
void SetResultsGroupBoxTitle(HWND hDlg, char *title);
void UpdateCheatCodePreview(HWND hDlg);
void Defaults_CheatSearchDlg();
void CE_UpdateControls(HWND hDlg, int item);
void CE_SaveCheat(int item);
void CS_UpdateSearchProc(HWND hDlg);
DWORD WINAPI LiveUpdate(LPVOID arg);

CHTDEVENTRY *ReadProject64ChtDev();
void WriteProject64ChtDev(HWND hDlg);
char *ResultsToString(void);

BOOL CALLBACK CheatSearchDlgProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam);
/////////////////////////////////////
// End of Function Prototypes
/////////////////////////////////////


/////////////////////////////////////
// Global Variables 
/////////////////////////////////////
CS_SEARCH search;
CS_RESULTS results;
CS_DEV cheat_dev;

BOOL secondSearch = FALSE;
BOOL searched = FALSE;

HANDLE chtLVMutex = NULL;
BOOL doingLiveUpdate = FALSE;
/////////////////////////////////////
// End of Global Variables 
/////////////////////////////////////


/////////////////////////////////////
// Start of function implementation 
/////////////////////////////////////

void Show_CheatSearchDlg (HWND hParent) {
	if(hCheatSearchDlg != NULL) {	// Stop multiple instances
		SetForegroundWindow(hCheatSearchDlg);
		return;
	}
	hCheatSearchDlg = CreateDialog(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_Cheats_Search), hParent, CheatSearchDlgProc);
	if(hCheatSearchDlg != NULL)
	{
		Setup_CheatSearch_Window (hCheatSearchDlg);
		ShowWindow(hCheatSearchDlg, SW_SHOW);
	}
}

void Close_CheatSearchDlg () {
	CS_ClearResults(&results);
	CS_ClearDev(&cheat_dev);

	if (hCheatSearchDlg != NULL) {
		DestroyWindow(hCheatSearchDlg);
		hCheatSearchDlg = NULL;
	}
}

void Defaults_CheatSearchDlg() { 
	searched = FALSE;
	secondSearch = FALSE;

	Button_Enable(GetDlgItem(hCheatSearchDlg, IDB_CS_CHEAT_EDIT), FALSE);
	Button_Enable(GetDlgItem(hCheatSearchDlg, IDB_CS_CHEAT_REMOVE), FALSE);
	Button_Enable(GetDlgItem(hCheatSearchDlg, IDB_CS_CHEAT_MOVEUP), FALSE);
	Button_Enable(GetDlgItem(hCheatSearchDlg, IDB_CS_CHEAT_MOVEDOWN), FALSE);
	Button_Enable(GetDlgItem(hCheatSearchDlg, IDB_CS_CHEAT_ENABLE), FALSE);
	Button_Enable(GetDlgItem(hCheatSearchDlg, IDB_CS_CHEAT_SAVE), FALSE);
}

char *trimwhitespace(char *str) {
	char *end;

	// Trim leading space
	while(isspace(*str)) str++;
	if(*str == 0)  // All spaces? 
		return str;

	// Trim trailing space
	end = str + strlen(str) - 1;
	while(end > str && isspace(*end)) end--;

	// Write new null terminator 
	*(end+1) = 0; 

	return str;
}

BOOL CALLBACK CheatSearchDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg) {
	case WM_CLOSE:
		Close_CheatSearchDlg();
		break;
	case WM_NOTIFY:
		switch (((LPNMHDR)lParam)->code) {
		case NM_DBLCLK:
			switch (((LPNMHDR)lParam)->idFrom) {
			case IDL_SEARCH_RESULT_LIST:
				{
					int item = ((LPNMITEMACTIVATE) lParam)->iItem;
					CODEENTRY tmp;
					CS_HITS hit;

					if (item == -1) break;

					hit = *CS_GetHit(&results, item);
					tmp.Address = hit.address;
					tmp.Value = hit.value;
					tmp.numBits = search.searchNumBits;
					tmp.Enabled = FALSE;
					strcpy(tmp.Name, "");
					strcpy(tmp.Note, "");
					strcpy(tmp.Text, "");

					cheat_dev.modify = &tmp;

					// Only save the code if okay was pressed
					if(DialogBox(hInst, MAKEINTRESOURCE(IDD_Cheats_Search_Edit), hDlg, (DLGPROC)CheatSearch_Add_Proc) == IDOK) {
						CS_AddCode(&cheat_dev, tmp);
						UpdateCheatCreateList(hDlg);
					}
				}
				break;
			case IDL_CS_CHEAT_CREATE:	// Double click to allow Edit cheat shortcut
				{
					int item = ((LPNMITEMACTIVATE) lParam)->iItem;
					CODEENTRY tmp;

					tmp.Address = 0;
					tmp.Value = 0;
					tmp.numBits = search.searchNumBits;
					tmp.Enabled = FALSE;
					strcpy(tmp.Name, "");
					strcpy(tmp.Note, "");
					strcpy(tmp.Text, "");

					cheat_dev.modify = &tmp;

					// Double clicking on the empty space in the list for the cheat dev area will add a new entry
					if (item == -1) {
						if(DialogBox(hInst, MAKEINTRESOURCE(IDD_Cheats_Search_Edit), hDlg, (DLGPROC)CheatSearch_Add_Proc) == IDOK) {
							tmp.numBits = search.searchNumBits;
							CS_AddCode(&cheat_dev, tmp);
							UpdateCheatCreateList(hDlg);
						}
						break;
					}

					// Quick sanity check, the cheat list and the list control should both line up
					// If they do not then something horrible happened, either the list was deallocated or the cheat was removed
					if (CS_GetCodeAt(&cheat_dev, item) == NULL)
						break;

					// Simulate the Edit button being clicked, it already has the appropriate code
					SendMessage(GetDlgItem(hDlg, IDB_CS_CHEAT_EDIT), BM_CLICK, (WPARAM)NULL, (LPARAM)NULL);
					break;
				}
			}
			break;

		case NM_CLICK:
			switch (((LPNMHDR)lParam)->idFrom) {
			case IDL_CS_CHEAT_CREATE:	// To toggle the buttons as the item is clicked in the cheat creation section
				CE_UpdateControls(hDlg, ((LPNMITEMACTIVATE) lParam)->iItem);
				break;
			}
			break;
		}
		break;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDR_SEARCH_VALUE:
			// TODO! Disable text box
			// Enable the 3 value boxes
			Edit_Enable(GetDlgItem(hDlg, IDT_SEARCH_TEXT), FALSE);
			Edit_Enable(GetDlgItem(hDlg, IDT_SEARCH_VALUE), TRUE);
			Edit_Enable(GetDlgItem(hDlg, IDT_SEARCH_JAL), FALSE);
			break;
		case IDR_SEARCH_TEXT:
			// TODO! Disable value box entry (all 3)
			// Enable the text box
			Edit_Enable(GetDlgItem(hDlg, IDT_SEARCH_TEXT), TRUE);
			Edit_Enable(GetDlgItem(hDlg, IDT_SEARCH_VALUE), FALSE);
			Edit_Enable(GetDlgItem(hDlg, IDT_SEARCH_JAL), FALSE);
			break;
		case IDB_CS_SEARCH:
			Search(hDlg);
			/*
			if (search.searchBy == searchingbyvalue)
				WriteProject64ChtDev(hDlg);*/
			break;
		case IDB_CS_RESET:
			CS_ClearResults(&results);
			Defaults_CheatSearchDlg();
			ListView_DeleteAllItems(GetDlgItem(hDlg, IDL_SEARCH_RESULT_LIST));
			InitSearchFormatList(hDlg);
			ComboBox_Enable(GetDlgItem(hDlg, IDC_SEARCH_NUMBITS), TRUE);
			ComboBox_Enable(GetDlgItem(hDlg, IDC_ADDRESS_RANGE), TRUE);
			Edit_Enable(GetDlgItem(hDlg, IDT_SEARCH_VALUE), TRUE);
			Edit_SetText(GetDlgItem(hDlg, IDT_SEARCH_VALUE),"");
			Edit_SetText(GetDlgItem(hDlg, IDT_SEARCH_TEXT),"");
			Button_SetCheck(GetDlgItem(hDlg, IDR_SEARCH_VALUE), BST_CHECKED);
			Button_SetCheck(GetDlgItem(hDlg, IDR_SEARCH_TEXT), BST_UNCHECKED);
			SetResultsGroupBoxTitle(hDlg, "Results");
			break;
		case IDB_CS_DUMPMEM:
			Show_MemDumpDlg (hDlg);
			//SaveMemDump(hDlg);
			//DumpPCAndDisassembled(hDlg, 0x80000400, 0x80001000);
			break;
		case IDC_SEARCH_HEXDEC:
			switch (HIWORD(wParam)) {
				case CBN_SELCHANGE:
					search.searchType = (SEARCHTYPE)ComboBox_GetCurSel((HWND)lParam);
					switch (search.searchType) {
					case dec:
					case hex:
						CS_UpdateSearchProc(hDlg);
						Edit_Enable(GetDlgItem(hDlg, IDT_SEARCH_VALUE), TRUE);
						Edit_SetText(GetDlgItem(hDlg, IDT_SEARCH_VALUE), "");
						break;
					case unknown:
					case changed:
					case unchanged:
					case lower:
					case higher:
						Edit_Enable(GetDlgItem(hDlg, IDT_SEARCH_VALUE), FALSE);
						Edit_SetText(GetDlgItem(hDlg, IDT_SEARCH_VALUE), "");
						break;
					}
					break;
			}
			break;
		case IDC_SEARCH_NUMBITS:
			switch (HIWORD(wParam)) {
			case CBN_SELCHANGE:
				search.searchNumBits = (NUMBITS)ComboBox_GetCurSel((HWND)lParam);
				CS_UpdateSearchProc(hDlg);
				Edit_SetText(GetDlgItem(hDlg, IDT_SEARCH_VALUE), "");
				break;
			}
			break;
		case IDL_SEARCH_RESULT_LIST:
			break;
		case IDCANCEL:
			EndDialog( hDlg, IDCANCEL );
			break;
		case IDB_CS_CHEAT_EDIT:
			{
				int sItem = ListView_GetNextItem(GetDlgItem(hDlg, IDL_CS_CHEAT_CREATE), -1, LVNI_FOCUSED);
				CODEENTRY *cItem = CS_GetCodeAt(&cheat_dev, sItem);

				// Should not happen (Out of bounds)
				if (sItem == -1 || cItem == NULL) break;

				cheat_dev.modify = cItem;	// Address of item to be modified

				if(DialogBox(hInst, MAKEINTRESOURCE(IDD_Cheats_Search_Edit), hDlg, (DLGPROC)CheatSearch_Add_Proc) == IDOK) {
					UpdateCheatCreateList(hDlg);
					CE_UpdateControls(hDlg, sItem);
				}
				break;
			}
		case IDB_CS_CHEAT_REMOVE:
			CS_RemoveCodeAt(&cheat_dev, ListView_GetNextItem(GetDlgItem(hDlg, IDL_CS_CHEAT_CREATE), -1, LVNI_SELECTED));
			UpdateCheatCreateList(hDlg);
			CE_UpdateControls(hDlg, -1);
			break;
		case IDB_CS_CHEAT_MOVEUP:
			{
				int item  = ListView_GetNextItem(GetDlgItem(hDlg, IDL_CS_CHEAT_CREATE), -1, LVNI_SELECTED);

				// Nothing selected or already top-most item
				if (item == -1 || item == 0)
					break;
				
				CS_SwapDev(&cheat_dev, item, item - 1);

				// Repopulate the list and update controls
				UpdateCheatCreateList(hDlg);
				CE_UpdateControls(hDlg, item - 1);
			}
			break;
		case IDB_CS_CHEAT_MOVEDOWN:
			{
				int item = ListView_GetNextItem(GetDlgItem(hDlg, IDL_CS_CHEAT_CREATE), -1, LVNI_SELECTED);

				// Nothing selected
				if (item == -1)
					break;

				CS_SwapDev(&cheat_dev, item, item + 1);

				// Repopulate the list and update controls
				UpdateCheatCreateList(hDlg);
				CE_UpdateControls(hDlg, item + 1);
			}
			break;
		case IDB_CS_CHEAT_ENABLE:
			{
				int item = ListView_GetNextItem(GetDlgItem(hDlg, IDL_CS_CHEAT_CREATE), -1, LVNI_SELECTED);
				CODEENTRY *cheat;

				if (item == -1)
					break;

				cheat = CS_GetCodeAt(&cheat_dev, item);

				if (cheat != NULL)
					cheat->Enabled = !cheat->Enabled;

				// Repopulate the list and update controls
				UpdateCheatCreateList(hDlg);
				CE_UpdateControls(hDlg, item);

				break;
			}
		case IDB_CS_CHEAT_SAVE:
			{
				int item = ListView_GetNextItem(GetDlgItem(hDlg, IDL_CS_CHEAT_CREATE), -1, LVNI_SELECTED);

				if (item == -1)
					break;
				
				CE_SaveCheat(item);
				MessageBox(hDlg, "Cheat has been added to the database.\nAccess through normal cheat window.", "Cheat Saved", MB_OK);
				UpdateCheatCreateList(hDlg);
				CE_UpdateControls(hDlg, -1);	// Save eats the item so default to nothing
			}
			break;
		case IDT_SEARCH_VALUE:
			if (HIWORD(wParam) == EN_CHANGE) { 
				char strVal[8];

				Edit_GetText((HWND)lParam, strVal, 8);

				if (search.searchType == hex) {
					VALUE_SEARCH.value = AsciiToHex(strVal);
					if (VALUE_SEARCH.value > VALUE_SEARCH.max_value)
						VALUE_SEARCH.value = VALUE_SEARCH.max_value;					
					sprintf_s(strVal, 7, "%X", VALUE_SEARCH.value);
				}
				else {
					VALUE_SEARCH.value = atoi(strVal);
					if (VALUE_SEARCH.value > VALUE_SEARCH.max_value)
						VALUE_SEARCH.value = VALUE_SEARCH.max_value;
					sprintf_s(strVal, 7, "%u", VALUE_SEARCH.value);
				}
				Edit_SetText((HWND)lParam, strVal);
				Edit_SetSel((HWND)lParam, strlen(strVal), strlen(strVal));
			}
			break;
		}
	default:
		return FALSE;
	}
	return TRUE;
}

void Setup_CheatSearch_Window (HWND hParent) {
	DWORD X, Y, dwStyle;
	DWORD WindowWidth = 683;
	DWORD WindowHeight = 341;

	Defaults_CheatSearchDlg();	// Load sane defaults
	CS_InitDev(&cheat_dev);

	CheckRadioButton(hParent, IDR_SEARCH_VALUE, IDR_SEARCH_JAL, IDR_SEARCH_VALUE);

	if (RdramSize == 0x400000) {
		Edit_SetText(GetDlgItem(hParent, IDS_RDRAMSIZE), "RDRam size: 4MB");
		Edit_SetText(GetDlgItem(hParent, IDT_ADDRESS_START), "0x00000000");
		Edit_SetText(GetDlgItem(hParent, IDT_ADDRESS_END), "0x003FFFFF");
	}
	else {
		Edit_SetText(GetDlgItem(hParent, IDS_RDRAMSIZE), "RDRam size: 8MB");
		Edit_SetText(GetDlgItem(hParent, IDT_ADDRESS_START), "0x00000000");
		Edit_SetText(GetDlgItem(hParent, IDT_ADDRESS_END), "0x007FFFFF");
	}

	InitSearchFormatList(hParent);
	InitSearchNumBitsList(hParent);
	CS_UpdateSearchProc(hParent);
	
	dwStyle = ListView_GetExtendedListViewStyle(GetDlgItem(hParent, IDL_SEARCH_RESULT_LIST));
	dwStyle |= LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER;
	ListView_SetExtendedListViewStyle(GetDlgItem(hParent, IDL_SEARCH_RESULT_LIST), dwStyle);
	InitSearchResultList(hParent);

	dwStyle = ListView_GetExtendedListViewStyle(GetDlgItem(hParent, IDL_CS_CHEAT_CREATE));
	dwStyle |= LVS_EX_FULLROWSELECT | LVS_EX_DOUBLEBUFFER;
	ListView_SetExtendedListViewStyle(GetDlgItem(hParent, IDL_CS_CHEAT_CREATE), dwStyle);
	InitCheatCreateList(hParent);

	X=0;
	Y=0;

	if (!GetStoredWinPos("CheatSearch", &X, &Y)) {
		X = (GetSystemMetrics(SM_CXSCREEN) - WindowWidth) / 2;
		Y = (GetSystemMetrics(SM_CYSCREEN) - WindowHeight) / 2;
	}
	SetWindowPos(hParent, NULL, X, Y, WindowWidth, WindowHeight, SWP_NOZORDER | SWP_SHOWWINDOW | SWP_NOSIZE);
}

void SetResultsGroupBoxTitle(HWND hDlg, char *title){
	SetWindowText(GetDlgItem(hDlg, IDGB_CS_RESULTS), title);
}

void InitSearchFormatList(HWND hDlg) {
	HWND hComboBox;
	int prevSelection;
	
	hComboBox = GetDlgItem(hDlg, IDC_SEARCH_HEXDEC);
	prevSelection = ComboBox_GetCurSel(hComboBox);
	
	ComboBox_ResetContent(hComboBox);
	ComboBox_AddString(hComboBox, "Unknown");
	ComboBox_AddString(hComboBox, "Decimal");
	ComboBox_AddString(hComboBox, "Hexadecimal");

	if (!searched) {
		search.searchType = dec;
		Edit_SetText(GetDlgItem(hDlg, IDT_SEARCH_VALUE), "0");
	}
	else {
		ComboBox_AddString(hComboBox, "Changed");
		ComboBox_AddString(hComboBox, "Unchanged");
		ComboBox_AddString(hComboBox, "Higher");
		ComboBox_AddString(hComboBox, "Lower");
		if (prevSelection == CB_ERR)
			search.searchType = unknown;
	}
	
	ComboBox_SetCurSel(hComboBox, search.searchType);
}

void InitSearchNumBitsList(HWND hDlg) {
	HWND hComboBox;

	hComboBox = GetDlgItem(hDlg, IDC_SEARCH_NUMBITS);
	ComboBox_ResetContent(hComboBox);
	ComboBox_AddString(hComboBox, "8bit");
	ComboBox_AddString(hComboBox, "16bit");

	search.searchNumBits = bits8;
	ComboBox_SetCurSel(hComboBox, search.searchNumBits);
}

void InitSearchResultList(HWND hDlg) {
	LV_COLUMN  col;
	HWND hListView;
	long state;

	hListView = GetDlgItem(hDlg, IDL_SEARCH_RESULT_LIST);

	col.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
	col.fmt  = LVCFMT_LEFT;

	// Clear and remove any previous incarnations of the ListView control
	ListView_DeleteAllItems(hListView);
	while (ListView_DeleteColumn(hListView, 0));

	state = Button_GetCheck(GetDlgItem(hDlg, IDR_SEARCH_TEXT));
	if (state == BST_CHECKED) {
		// Build the results list for Text Search
		col.pszText  = "Address";
		col.cx       = 110;
		col.iSubItem = 0;
		ListView_InsertColumn (hListView, 0, &col);

		col.pszText  = "Text";
		col.cx       = 150;
		col.iSubItem = 1;
		ListView_InsertColumn (hListView, 1, &col);

		col.pszText  = "Old Text";
		col.cx       = 150;
		col.iSubItem = 2;
		ListView_InsertColumn (hListView, 2, &col);
	}
	else {
		// Build the results list for Value
		col.pszText  = "Address";
		col.cx       = 110;
		col.iSubItem = 0;
		ListView_InsertColumn (hListView, 0, &col);

		col.pszText  = "Value (Hex)";
		col.cx       = 70;
		col.iSubItem = 1;
		ListView_InsertColumn (hListView, 1, &col);

		col.pszText  = "Value (Dec)";
		col.cx       = 70;
		col.iSubItem = 2;
		ListView_InsertColumn (hListView, 2, &col);

		col.pszText  = "Old (Hex)";
		col.cx       = 70;
		col.iSubItem = 3;
		ListView_InsertColumn (hListView, 3, &col);
	}
}

void PopulateSearchResultList(HWND hDlg) {
	HWND hListView;
	LV_ITEM item;
	int count;
	char s[9], title[STRING_MAX];
	long state;
	CS_HITS *hit;

	hListView = GetDlgItem(hDlg, IDL_SEARCH_RESULT_LIST);
	ListView_DeleteAllItems(hListView);

	// There is a different format for Text and Value searches
	state = Button_GetCheck(GetDlgItem(hDlg, IDR_SEARCH_TEXT));

	SetWindowRedraw(hListView, FALSE);
	for (count=0; count < LISTVIEW_MAXSHOWN; count++) {

		// Stop condition, assuming count has not reached LISTVIEW_MAXSHOWN
		hit = CS_GetHit(&results, count);
		if (hit == NULL)
			break;

		item.mask      = LVIF_TEXT;
		item.iItem     = count;
		item.iSubItem  = 0;

		// First entry is for Address
		sprintf_s(s, 9, "%08X", hit->address);
		item.pszText = (LPSTR) &s;
		ListView_InsertItem(hListView,&item);

		// Text Search
		if (state == BST_CHECKED) {
			item.pszText = (LPSTR) search.search_string;
			item.iSubItem = 1;
			ListView_SetItem(hListView,&item);

			if (secondSearch) {
				item.pszText = "Is this needed?";
				item.iSubItem = 2;
				ListView_SetItem(hListView,&item);
			}
		}
		// Dec/Hex or Value Search
		else {
			// Second entry is for Value (Hex)
			if (search.searchNumBits == bits8)
				sprintf_s(s, 9, "%02X", hit->value);
			else 
				sprintf_s(s, 9, "%04X", hit->value);

			item.pszText = (LPSTR) &s;
			item.iSubItem  = 1;
			ListView_SetItem(hListView,&item);

			// Third entry is for Value (Dec)
			sprintf_s(s, 9,"%u", hit->value);	
			item.pszText = (LPSTR) &s;
			item.iSubItem  = 2;
			ListView_SetItem(hListView,&item);

			// Fourth Entry is for Old Value (Hex)
			if (secondSearch) {
				if (search.searchNumBits == bits8)
				sprintf_s(s, 9, "%02X", hit->prev_value);
			else 
				sprintf_s(s, 9, "%04X", hit->prev_value);
				item.pszText = (LPSTR) &s;
				item.iSubItem  = 3;
				ListView_SetItem(hListView,&item);
			}
		}
	}

	// The listview only displays up to 4096 entries but it can store more
	count = results.num_stored;

	if (count > LISTVIEW_MAXSHOWN)
		sprintf(title, "Results (found %u / displaying first 4096)", count);
	else
		sprintf(title, "Results (found %u)", count);
	SetResultsGroupBoxTitle(hDlg, title);

	// Force a redraw of the listview control
	SetWindowRedraw(hListView, TRUE);
	InvalidateRect(hListView, NULL, TRUE);
}

void InitCheatCreateList(HWND hDlg) {
	HWND hListView;
	LVCOLUMN col;

	hListView = GetDlgItem(hDlg, IDL_CS_CHEAT_CREATE);

	col.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH  | LVCF_ORDER;
	col.iOrder = 0;
	col.pszText = "Act.";
	col.fmt = LVCFMT_IMAGE;
	col.cx = 40;
	col.iSubItem = 0;
	ListView_InsertColumn(hListView, 0, &col);

	col.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH  | LVCF_ORDER;
	col.iOrder = 1;
	col.pszText = "Code";
	col.fmt = LVCFMT_LEFT;
	col.cx = 100;
	col.iSubItem = 1;
	ListView_InsertColumn(hListView, 1, &col);

	col.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH  | LVCF_ORDER;
	col.iOrder = 2;
	col.pszText = ROM_NAME;
	col.fmt = LVCFMT_LEFT;
	col.cx = 220;
	col.iSubItem = 2;
	ListView_InsertColumn(hListView, 2, &col);
}

void UpdateCheatCreateList(HWND hDlg) {
	HWND hListView;
	LV_ITEM item;
	int ctr;
	char s[14];
	CODEENTRY *curr;

	// Add to the list view
	hListView = GetDlgItem(hDlg, IDL_CS_CHEAT_CREATE);
	ListView_DeleteAllItems(hListView);

	ctr = 0;
	curr = CS_GetCodeAt(&cheat_dev, 0);

	while (curr != NULL) {
		item.mask = LVIF_TEXT;
		if(curr->Enabled)
			item.pszText = "On";
		else
			item.pszText = "Off";
		item.iItem = ctr;
		item.iSubItem = 0;
		ListView_InsertItem(hListView, &item);

		if(curr->numBits == bits8)
			sprintf(s, "%02X%06X %02X", curr->Activator, curr->Address, curr->Value);
		else
			sprintf(s, "%02X%06X %04X", curr->Activator, curr->Address, curr->Value);
	
		item.pszText = s;
		item.iSubItem = 1;
		ListView_SetItem(hListView, &item);

		item.pszText = curr->Name;
		item.iSubItem = 2;
		ListView_SetItem(hListView, &item);

		// Increment to the next item
		ctr++;
		curr = CS_GetCodeAt(&cheat_dev, ctr);
	}

	// TO DO!
	// Wrong place to live update the values....
	//CreateThread(NULL, 0, LiveUpdate, &hDlg, 0, NULL);
}

void Search(HWND hDlg) {
	BYTE *buffer;
	long bufferSize, count;
	char startAddress[11], endAddress[11];
	DWORD dwstartAddress;

	// Reset the results window, this depends on what was ticked.
	InitSearchResultList(hDlg);

	// Address search range
	Edit_GetText(GetDlgItem(hDlg, IDT_ADDRESS_START), startAddress, 11);
	Edit_GetText(GetDlgItem(hDlg, IDT_ADDRESS_END), endAddress, 11);

	dwstartAddress = strtoul(startAddress, NULL, 16);
	bufferSize = strtoul(endAddress, NULL, 16) - strtoul(startAddress, NULL, 16);

	// Search Type (Must be set because it will be used later on for live updates)
	if (Button_GetCheck(GetDlgItem(hDlg, IDR_SEARCH_VALUE)) == BST_CHECKED)
		search.searchBy = searchbyvalue;
	else if (Button_GetCheck(GetDlgItem(hDlg, IDR_SEARCH_TEXT)) == BST_CHECKED)
		search.searchBy = searchbytext;
	else
		search.searchBy = searchbyjal;

	// Value search
	if (search.searchBy == searchbyvalue) {
		WORD searchValue;
		MIPS_WORD word;

		// Copy RDRAM into a buffer and byteswap, note this is still in Big Endian
		buffer = (BYTE *)malloc(bufferSize);
		for (count=0; count<bufferSize-4; count+=4) {
			r4300i_LW_PAddr(dwstartAddress + count, (DWORD *)&word);
			buffer[count]=word.UB[2];
			buffer[count+1]=word.UB[3];
			buffer[count+2]=word.UB[0];
			buffer[count+3]=word.UB[1];
		}
		//***********************************************

		// Value to search by, it's either in Hex or Dec
		Edit_GetText(GetDlgItem(hDlg, IDT_SEARCH_VALUE), search.search_string, 9);
		if (search.searchType == hex)
			searchValue = (WORD)strtoul(search.search_string, NULL, 16);
		else
			searchValue = (WORD)strtoul(search.search_string, NULL, 10);

		// The initial search will either add all addresses (unknown search)
		//	or addresses that match the search value (8bit or 16bit)
		if (!searched) {			
			switch (search.searchNumBits) {
			case bits8:
				for (count = 0; count < bufferSize - 1; count++) {
					if (search.searchType == unknown || 
						((BYTE)searchValue == (BYTE)buffer[count])) {
							CS_AddResult(&results, (DWORD)count + dwstartAddress + 1, (BYTE)buffer[count]);
					}
				}
				break;
			case bits16:
				for (count = 0; count < bufferSize - 2; count += 2) {
					if (search.searchType == unknown || 
						(searchValue == (WORD)(buffer[count+1] << 8) + buffer[count])) {
							CS_AddResult(&results, (DWORD)count + dwstartAddress, (WORD)(buffer[count + 1] << 8) + buffer[count]);
					}
				}
				break;
			}

			searched = TRUE;
			ComboBox_Enable(GetDlgItem(hDlg, IDC_SEARCH_NUMBITS), FALSE);
			ComboBox_Enable(GetDlgItem(hDlg, IDC_ADDRESS_RANGE), FALSE);
		}

		// Search performed using previous (or the initial) search result list
		else {
			struct CS_RESULTS tmp;
			WORD buffer_value;
			DWORD bufferAddress, count;
			CS_HITS *hit;
			BOOL matched;

			CS_InitResults(&tmp);

			// Subsequent searches of Hex/Dec values
			for (count = 0; count < results.num_stored; count++) {

				hit = CS_GetHit(&results, count);

				if (search.searchNumBits == bits8) {
					bufferAddress = hit->address + dwstartAddress - 1;
					buffer_value = (BYTE)buffer[bufferAddress];
				}
				else {
					bufferAddress = hit->address + dwstartAddress;
					buffer_value = (WORD)(buffer[bufferAddress + 1] << 8) + buffer[bufferAddress];
				}

				switch (search.searchType) {
				case dec:
				case hex:
					matched = (searchValue == buffer_value);
					break;
				case unknown:
					matched = TRUE;
					break;
				case changed:
					matched = (hit->value != buffer_value);
					break;
				case unchanged:
					matched = (hit->value == buffer_value);
					break;
				case lower:
					matched = (hit->value > buffer_value);
					break;
				case higher:
					matched = (hit->value < buffer_value);
					break;
				default:
					matched = FALSE;
					break;
				}
				
				if (!matched)
					continue;

				hit->prev_value = hit->value;
				hit->value = buffer_value;
				CS_AddHit(&tmp, hit);
			}

			secondSearch = TRUE;
			CS_ClearResults(&results);
			results = tmp;
		}
		searched = TRUE;
		free(buffer);
	}

	// Text search
	else if (search.searchBy == searchbytext) {
		MIPS_WORD word;
		long count2, textsearch_len;
		DWORD currentAddress, possibleAddress;

		CS_ClearResults(&results);

		// Copy RDRAM and convert to Little Endian format
		buffer = (BYTE *)malloc(bufferSize);
		for (count=0; count<bufferSize-4; count+=4) {
			r4300i_LW_PAddr(strtoul(startAddress, NULL, 16)+count, (DWORD *)&word);
			buffer[count]=word.UB[3];
			buffer[count+1]=word.UB[2];
			buffer[count+2]=word.UB[1];
			buffer[count+3]=word.UB[0];
		}
		//***********************************************

		possibleAddress = 0;

		// Fetch the string being searched for
		Edit_GetText(GetDlgItem(hDlg, IDT_SEARCH_TEXT), search.search_string, STRING_MAX - 1);
		textsearch_len = strlen(search.search_string);
		
		for (count = 0, count2 = 0; count < bufferSize-1; count++) {
			currentAddress = (DWORD)count + dwstartAddress + 1;
			
			// Pattern match
			if ((BYTE)search.search_string[count2] == (BYTE)buffer[currentAddress]) {
				if (count2 == 0) {
					possibleAddress = currentAddress;
				}
				count2++;
				
				if (count2 == textsearch_len) {
					CS_AddTextResult(&results, possibleAddress, search.search_string);
					count2 = 0;
				}
			}
			else
					count2 = 0;
		}

		free(buffer);
	}
	
	// Unused -- Implementation seeems unnecessary but will check with Gent
	else if (search.searchBy == searchbyjal) {
		return;
	}

	else
		return;

	PopulateSearchResultList(hDlg);

	// Update to include changed, unchanged, higher, lower, etc...
	InitSearchFormatList(hDlg);
}

LRESULT CALLBACK CheatSearch_Add_Proc (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	HWND hComboBox;
	int Count;
	char strAddress[9], strValue[7];

	switch (uMsg) {
	case WM_INITDIALOG:
		// Populate ComboBox with GS Cheat Types
		hComboBox = GetDlgItem(hDlg, IDC_CSE_GSCODETYPE);
		ComboBox_ResetContent(hComboBox);
		
		// Subclass HexEditControls for Address
		ADDR_HEX.hWnd = GetDlgItem(hDlg, IDT_CSE_ADDRESS);
		ADDR_HEX.callBack = (WNDPROC)SetWindowLongPtr(ADDR_HEX.hWnd, GWLP_WNDPROC, (LONG_PTR)HexEditControlProc);
		ADDR_HEX.value = cheat_dev.modify->Address;
		ADDR_HEX.max_value = RdramSize - 1;

		if (search.searchBy == searchbyvalue) {
			Edit_Enable(GetDlgItem(hDlg, IDT_CSE_VALUE_HEX), TRUE);
			Edit_Enable(GetDlgItem(hDlg, IDT_CSE_VALUE_DEC), TRUE);
			Edit_Enable(GetDlgItem(hDlg, IDT_CSE_TEXT), FALSE);
			Edit_Enable(GetDlgItem(hDlg, IDT_CSE_PREVIEW), TRUE);

			switch (cheat_dev.modify->numBits) {
				int loc;
				char check[3];

			case bits8:
				for (Count = 0, loc = 0; Count < 5; Count++) {
					ComboBox_AddString(hComboBox, N64GSCODETYPES8BIT[Count*2]);
					sprintf(check, "%2X", cheat_dev.modify->Activator);
					if (strcmp(check, N64GSCODETYPES8BIT[Count*2+1]) == 0)
						loc = Count;
				}
				ComboBox_SetCurSel(hComboBox, loc);
				break;

			case bits16:
				for (Count = 0, loc = 0; Count < 5; Count++) {
					ComboBox_AddString(hComboBox, N64GSCODETYPES16BIT[Count*2]);
					sprintf(check, "%2X", cheat_dev.modify->Activator);
					if (strcmp(check, N64GSCODETYPES16BIT[Count*2+1]) == 0)
						loc = Count;
				}
				ComboBox_SetCurSel(hComboBox, loc);
				break;
			}

			// Subclass HexEditControls for Hex Value (Linked with VALUE_DEC later on)
			VALUE_HEX.hWnd = GetDlgItem(hDlg, IDT_CSE_VALUE_HEX);
			VALUE_HEX.callBack = (WNDPROC)SetWindowLongPtr(VALUE_HEX.hWnd, GWLP_WNDPROC, (LONG_PTR)HexEditControlProc);
			VALUE_HEX.value = cheat_dev.modify->Value;
			if(cheat_dev.modify->numBits == bits8)
				VALUE_HEX.max_value = 0xFF;
			else
				VALUE_HEX.max_value = 0xFFFF;

			// Subclass DecEditControls for Dec Value (Linked with VALUE_HEX later on)
			VALUE_DEC.hWnd = GetDlgItem(hDlg, IDT_CSE_VALUE_DEC);
			VALUE_DEC.callBack = (WNDPROC)SetWindowLongPtr(VALUE_DEC.hWnd, GWLP_WNDPROC, (LONG_PTR)DecEditControlProc);
			VALUE_DEC.value = cheat_dev.modify->Value;
			if(cheat_dev.modify->numBits == bits8)
				VALUE_DEC.max_value = 0xFF;
			else
				VALUE_DEC.max_value = 0xFFFF;
			
			sprintf((char *)&strValue, "%X", cheat_dev.modify->Value);
			Edit_SetText(GetDlgItem(hDlg, IDT_CSE_VALUE_HEX), strValue);
			sprintf((char *)&strValue, "%u", cheat_dev.modify->Value);
			Edit_SetText(GetDlgItem(hDlg, IDT_CSE_VALUE_DEC), strValue);

			UpdateCheatCodePreview(hDlg);
		}

		// Searching by text
		else if (search.searchBy == searchbytext) {
			Edit_Enable(GetDlgItem(hDlg, IDT_CSE_VALUE_HEX), FALSE);
			Edit_Enable(GetDlgItem(hDlg, IDT_CSE_VALUE_DEC), FALSE);
			Edit_Enable(GetDlgItem(hDlg, IDT_CSE_TEXT), TRUE);
			Edit_Enable(GetDlgItem(hDlg, IDT_CSE_PREVIEW), FALSE);

			Edit_SetText(GetDlgItem(hDlg, IDT_CSE_TEXT), cheat_dev.modify->Text);
		}

		// Search by jal, unused
		else {
			Edit_Enable(GetDlgItem(hDlg, IDT_CSE_VALUE_HEX), FALSE);
			Edit_Enable(GetDlgItem(hDlg, IDT_CSE_VALUE_DEC), FALSE);
			Edit_Enable(GetDlgItem(hDlg, IDT_CSE_TEXT), FALSE);
			Edit_Enable(GetDlgItem(hDlg, IDT_CSE_PREVIEW), FALSE);
		}

		// Had to move these after subclassing the controls to avoid a race condition between setting and checking the values
		sprintf((char *)&strAddress, "%06X", cheat_dev.modify->Address);
		Edit_SetText(GetDlgItem(hDlg, IDT_CSE_ADDRESS), strAddress);
		Edit_SetText(GetDlgItem(hDlg, IDT_CSE_Name), cheat_dev.modify->Name);
		Edit_SetText(GetDlgItem(hDlg, IDT_CSE_NOTE), cheat_dev.modify->Note);

		break;

	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDOK:
			{
				char s[STRING_MAX], *trimmed;
				int selActivator;

				// Name
				Edit_GetText(GetDlgItem(hDlg, IDT_CSE_Name), s, STRING_MAX);
				trimmed = trimwhitespace(s);

				// Require a name before anything is saved
				if (strlen(trimmed) == 0) {
					MessageBox(hDlg, "Please enter a name for the cheat first!", "Error", MB_OK);
					return TRUE;
				}
				strncpy(cheat_dev.modify->Name, trimmed, STRING_MAX);

				// Note
				Edit_GetText(GetDlgItem(hDlg, IDT_CSE_NOTE), s, STRING_MAX);
				trimmed = trimwhitespace(s);
				strncpy(cheat_dev.modify->Note, trimmed, STRING_MAX);

				// Address
				Edit_GetText(GetDlgItem(hDlg, IDT_CSE_ADDRESS), s, 7);
				cheat_dev.modify->Address = (DWORD)strtoul(s, NULL, 16);

				// Search by VALUE
				if (search.searchBy == searchbyvalue) {
					// Value
					Edit_GetText(GetDlgItem(hDlg, IDT_CSE_VALUE_DEC), s, STRING_MAX);
					cheat_dev.modify->Value = (WORD)strtoul(s, NULL, 10);
					
					// Activator
					selActivator = ComboBox_GetCurSel(GetDlgItem(hDlg, IDC_CSE_GSCODETYPE));
					if (cheat_dev.modify->numBits == bits8)
						cheat_dev.modify->Activator = (BYTE)strtoul(N64GSCODETYPES8BIT[selActivator*2+1], NULL, 16);
					else
						cheat_dev.modify->Activator = (BYTE)strtoul(N64GSCODETYPES16BIT[selActivator*2+1], NULL, 16);
				}

				// Search by TEXT
				else if (search.searchBy == searchbytext) {
					Edit_GetText(GetDlgItem(hDlg, IDT_CSE_TEXT), cheat_dev.modify->Text, STRING_MAX);
				}

				// Search by JAL (Do nothing for now)
				else {
				}

				cheat_dev.modify = NULL;
			}

			// Pass through
		case IDCANCEL: 
			EndDialog(hDlg, wParam); 
			return TRUE;

		case IDC_CSE_GSCODETYPE:
			if (HIWORD(wParam) == CBN_SELCHANGE) {
				UpdateCheatCodePreview(hDlg);
			}
			break;

		case IDT_CSE_VALUE_HEX:
			if (HIWORD(wParam) == EN_CHANGE) {
				Edit_GetText((HWND)lParam, strValue, 6);	// Maximum of FFFF plus 2 characters, 1 overflow and 1 newline
				VALUE_HEX.value = AsciiToHex(strValue);
				if (VALUE_HEX.value != VALUE_DEC.value) {
					if (search.searchNumBits == bits8) {VALUE_HEX.max_value = 0xFF;} else {VALUE_HEX.max_value = 0xFFFF;}
					if (VALUE_HEX.value > VALUE_HEX.max_value) {
						VALUE_HEX.value = VALUE_HEX.max_value;
						sprintf_s(strValue, 5, "%X", VALUE_HEX.max_value);
						Edit_SetText((HWND)lParam, strValue);
						Edit_SetSel((HWND)lParam, strlen(strValue), strlen(strValue));
					}
					else {
						VALUE_DEC.value = VALUE_HEX.value;
						sprintf_s(strValue, 6, "%u", VALUE_DEC.value);
						Edit_SetText(VALUE_DEC.hWnd, strValue);
						UpdateCheatCodePreview(hDlg);
					}
				}
			}
			break;

		case IDT_CSE_VALUE_DEC:
			if (HIWORD(wParam) == EN_CHANGE) {
				Edit_GetText((HWND)lParam, strValue, 7);	// Maximum of 65535 plus 2 characters, 1 overflow and 1 newline
				VALUE_DEC.value = atoi(strValue);

				if (VALUE_DEC.value != VALUE_HEX.value) {
					if (search.searchNumBits == bits8) {VALUE_DEC.max_value = 0xFF;} else {VALUE_DEC.max_value = 0xFFFF;}
					if (VALUE_DEC.value > VALUE_DEC.max_value) {
						sprintf_s(strValue, 6, "%u", VALUE_DEC.max_value);
						Edit_SetText((HWND)lParam, strValue);
						Edit_SetSel((HWND)lParam, strlen(strValue), strlen(strValue));
					}
					else {
						VALUE_HEX.value = VALUE_DEC.value;
						sprintf_s(strValue, 5, "%X", VALUE_HEX.value);
						Edit_SetText(VALUE_HEX.hWnd, strValue);
						UpdateCheatCodePreview(hDlg);
					}
				}
			}
			break;

		case IDT_CSE_ADDRESS:
			if (HIWORD(wParam) == EN_CHANGE) { 
				Edit_GetText((HWND)lParam, strAddress, 8);
				ADDR_HEX.value = AsciiToHex(strAddress);
				if (ADDR_HEX.value > ADDR_HEX.max_value) {
					sprintf_s(strAddress, 7, "%X", ADDR_HEX.max_value);
					Edit_SetText((HWND)lParam, strAddress);
				}
				Edit_SetSel((HWND)lParam, strlen(strAddress), strlen(strAddress));
				UpdateCheatCodePreview(hDlg);
			}
			break;
		}
		break;
	default:
		return FALSE;
	}
	return TRUE;
}

void UpdateCheatCodePreview(HWND hDlg) {
	int ndxCodeType;
	char address[7];
	char value[5];
	char s[STRING_MAX];

	// Nothing to update if not searching by value
	if (search.searchBy != searchbyvalue)
		return;
	
	ndxCodeType = ComboBox_GetCurSel(GetDlgItem(hDlg, IDC_CSE_GSCODETYPE));
	Edit_GetText(GetDlgItem(hDlg, IDT_CSE_ADDRESS), address, 7);
	Edit_GetText(GetDlgItem(hDlg, IDT_CSE_VALUE_HEX), value, 5);

	if (search.searchNumBits == bits8)
		sprintf((char*)&s,
			"%X %02X",
			(DWORD)strtoul(N64GSCODETYPES8BIT[ndxCodeType * 2 + 1], NULL, 16) << 24 ^ ((DWORD)strtoul(address, NULL, 16) & 0x00FFFFFF),
			(DWORD)strtoul(value, NULL, 16));
	else
		sprintf((char*)&s,
			"%X %04X",
			(DWORD)strtoul(N64GSCODETYPES16BIT[ndxCodeType * 2 + 1], NULL, 16) << 24 ^ ((DWORD)strtoul(address, NULL, 16) & 0x00FFFFFF),
			(DWORD)strtoul(value, NULL, 16));

	Edit_SetText(GetDlgItem(hDlg, IDT_CSE_PREVIEW), s);
}

/********************************************************************************************
GetCheatDevIniFileName

Purpose: 
Parameters:
Returns:

********************************************************************************************/
char * GetCheatDevIniFileName(void) {
	char path_buffer[_MAX_PATH], drive[_MAX_DRIVE] ,dir[_MAX_DIR];
	char fname[_MAX_FNAME],ext[_MAX_EXT];
	static char IniFileName[_MAX_PATH];

	GetModuleFileName(NULL,path_buffer,sizeof(path_buffer));
	_splitpath( path_buffer, drive, dir, fname, ext );
	sprintf(IniFileName,"%s%s%s",drive,dir,CHTDEV_NAME);
	return IniFileName;
}

CHTDEVENTRY *ReadProject64ChtDev() {
	FILE * pFile;
	size_t lSize, result;
	char * buffer;
	//CHTDEVENTRY * ChtDev;

	pFile = fopen (GetCheatDevIniFileName(), "rb");
	if (pFile == NULL) {
		// file error
		return NULL;
	}

	// obtain file size:
	fseek (pFile, 0, SEEK_END);
	lSize = ftell (pFile);
	rewind (pFile);

	// allocate memory to contain the whole file:
	buffer = (char*) malloc (sizeof(char)*lSize);
	if (buffer == NULL) {
		// Memory error
		return NULL;
	}

	// copy the file into the buffer:
	result = fread (buffer,1,lSize,pFile);
	if (result != lSize) {
		// Reading error
		return NULL;
	}

	/* the whole file is now loaded in the memory buffer. */

	// terminate
	fclose (pFile);
	free (buffer);

	return NULL; //ChtDev;
}

void WriteProject64ChtDev(HWND hDlg) {/*
	FILE * pFile;

	char Identifier[100];
	char s[16];
	CHTDEVENTRY *ChtDev;

	//ChtDev = ReadProject64ChtDev(GetCheatDevIniFileName());
	ChtDev = (CHTDEVENTRYA *)malloc(sizeof(CHTDEVENTRYA));

	RomID(Identifier, RomHeader);
	ChtDev->Identifier = Identifier;

	ChtDev->Name = RomName;

	ChtDev->LastSearch = (LASTSEARCHA *)malloc(sizeof(LASTSEARCHA));

	ChtDev->LastSearch->SearchType = "Value";

	Edit_GetText(GetDlgItem(hDlg, IDT_SEARCH_VALUE), s, 9);
	ChtDev->LastSearch->SearchValue = s;

	ChtDev->LastSearch->ValueSearchType = search.searchType;

	ChtDev->LastSearch->NumBits = search.searchNumBits;

	ChtDev->LastSearch->Results = ResultsToString();

	pFile = fopen (GetCheatDevIniFileName(), "w");
	if (pFile == NULL) {
		// file error
		return;
	}

	fprintf(pFile, "<?xml version=""1.0"" encoding=""ISO-8859-1""?>\n");
	fprintf(pFile, "<CheatDev>\n");

	fprintf(pFile, "   <Game>\n");
	fprintf(pFile, "      <ID>%s</ID>\n", ChtDev->Identifier);
	fprintf(pFile, "      <Name>%s</Name>\n", ChtDev->Name);
	fprintf(pFile, "      <LastSearch>\n");
	fprintf(pFile, "         <SearchType>%s</SearchType>\n", ChtDev->LastSearch->SearchType);
	fprintf(pFile, "         <Value>%s</Value>\n", ChtDev->LastSearch->SearchValue);
	fprintf(pFile, "         <ValueSearchType>%d</ValueSearchType>\n", ChtDev->LastSearch->ValueSearchType);
	fprintf(pFile, "         <NumBits>%d</NumBits>\n", ChtDev->LastSearch->NumBits);
	fprintf(pFile, "         <Results>%s</Results>\n", ChtDev->LastSearch->Results);
	fprintf(pFile, "      </LastSearch>\n");
	fprintf(pFile, "   </Game>\n");

	fprintf(pFile, "</CheatDev>\n");

	// terminate
	fclose (pFile);

	free(ChtDev->LastSearch);
	free(ChtDev);*/
}

// TO DO!
// Rewrite this and make it crash recovery compatible
// Currently it only backs up the listview shown, it would break future searches if loaded
// -- Assuming there were over 4096 hits (Or whatever LISTVIEW_MAXSHOW is)
char *ResultsToString(void) {
	DWORD count;
	char s[16], *result;
	long maxlength = 0;
	CS_HITS *hit;

	if (search.searchNumBits == bits8) {
		maxlength= (9+3+3)*min(results.num_stored, LISTVIEW_MAXSHOWN)+1;
	} else {
		maxlength= (9+5+5)*min(results.num_stored, LISTVIEW_MAXSHOWN)+1;
	}
	result = (char *)calloc(maxlength, sizeof(char));

	for (count=0; count<min(results.num_stored, LISTVIEW_MAXSHOWN); count++) {
		hit = CS_GetHit(&results, count);

		sprintf_s(s, 16, "%08X,", hit->address);
		strcat_s(result, maxlength, s);

		if (search.searchNumBits == bits8)
			sprintf_s(s, 16, "%02X,", hit->value);
		else
			sprintf_s(s, 16, "%04X,", hit->value);

		strcat_s(result, maxlength, s);

		if (secondSearch) {
			if (search.searchNumBits == bits8)
				sprintf_s(s, 16, "%02X,", hit->prev_value);
			else
				sprintf_s(s, 16, "%04X,", hit->prev_value);

			strcat_s(result, maxlength, s);
		} else {
			if (count<min(results.num_stored, LISTVIEW_MAXSHOWN)-1) {
				sprintf_s(s, 16, "  ,");
				strcat_s(result, maxlength, s);
			}
		}
	}

	return result;
}

// Called on CPU.c's RefreshScreen () on each VI
void Apply_CheatSearchDev () {
	int counter;
	GAMESHARK_CODE code;
	CODEENTRY *curr;

	counter = 0;
	curr = CS_GetCodeAt(&cheat_dev, 0);

	while (curr != NULL) {
		if (curr->Enabled) {
			if (strlen(curr->Text) == 0) {
				code.Command = curr->Activator << 24 ^ curr->Address;
				code.Value = (WORD)curr->Value;
				ApplyCheatEntry(&code, TRUE);
			}
			else {
				int count;

				for (count = 0; count < strlen(curr->Text); count++) {
					code.Command = 0x80 << 24 ^ curr->Address + count;
					code.Value = (WORD)curr->Text[count];
					ApplyCheatEntry(&code, TRUE);
				}
			}
		}

		// Fetch next item on the list
		counter++;
		curr = CS_GetCodeAt(&cheat_dev, counter);
	}
}

void CE_UpdateControls(HWND hDlg, int item) {
	CODEENTRY *curr, *next;

	if (item == -1) {	// Nothing selected, this is the default
		Button_Enable(GetDlgItem(hDlg, IDB_CS_CHEAT_EDIT), FALSE);
		Button_Enable(GetDlgItem(hDlg, IDB_CS_CHEAT_REMOVE), FALSE);
		Button_Enable(GetDlgItem(hDlg, IDB_CS_CHEAT_MOVEUP), FALSE);
		Button_Enable(GetDlgItem(hDlg, IDB_CS_CHEAT_MOVEDOWN), FALSE);
		Button_Enable(GetDlgItem(hDlg, IDB_CS_CHEAT_ENABLE), FALSE);
		Button_Enable(GetDlgItem(hDlg, IDB_CS_CHEAT_SAVE), FALSE);
		Edit_SetText(GetDlgItem(hDlg, IDT_CS_CHEAT_DESCRIPTION), '\0');
		Edit_SetText(GetDlgItem(hDlg, IDT_CS_CHEAT_NOTE), '\0');
		Edit_SetText(GetDlgItem(hDlg, IDB_CS_CHEAT_ENABLE), "Enable");
		return;
	}

	curr = CS_GetCodeAt(&cheat_dev, item);
	next = CS_GetCodeAt(&cheat_dev, item + 1);
					
	// No additional check needed for these
	Button_Enable(GetDlgItem(hDlg, IDB_CS_CHEAT_EDIT), TRUE);
	Button_Enable(GetDlgItem(hDlg, IDB_CS_CHEAT_REMOVE), TRUE);
	Button_Enable(GetDlgItem(hDlg, IDB_CS_CHEAT_ENABLE), TRUE);
	Button_Enable(GetDlgItem(hDlg, IDB_CS_CHEAT_SAVE), TRUE);
					
	Button_Enable(GetDlgItem(hDlg, IDB_CS_CHEAT_MOVEUP), item != 0);
	Button_Enable(GetDlgItem(hDlg, IDB_CS_CHEAT_MOVEDOWN), next != NULL);

	Edit_SetText(GetDlgItem(hDlg, IDT_CS_CHEAT_DESCRIPTION), curr->Name);
	Edit_SetText(GetDlgItem(hDlg, IDT_CS_CHEAT_NOTE), curr->Note);

	// Be sure the Enable button is showing the right text
	if (curr->Enabled)
		Edit_SetText(GetDlgItem(hDlg, IDB_CS_CHEAT_ENABLE), "Disable");
	else
		Edit_SetText(GetDlgItem(hDlg, IDB_CS_CHEAT_ENABLE), "Enable");

	// Ensure this item is visible
	ListView_SetItemState(GetDlgItem(hDlg, IDL_CS_CHEAT_CREATE), item, LVIS_SELECTED | LVIS_FOCUSED, LVIS_SELECTED | LVIS_FOCUSED);
	ListView_EnsureVisible(GetDlgItem(hDlg, IDL_CS_CHEAT_CREATE), item, FALSE);
}

// Cheat file uses this format
//	CheatX=ROM_NAME,ActivatorAddress Value
// If multiple cheats have the same name then they will be appended in one entry as follows
//	CheatX=ROM_NAME,ActivatorAddress Value, ActivatorAddress Value
//	and so forth, the comma being the separator between codes
// NOTE: This will remove all entries with the same name as item's
void CE_SaveCheat(int item) {
	char Identifier[100], CheatName[STRING_MAX], NewCheatName[STRING_MAX], *cheat, *tmp;
	int CheatLen, count, CheatNo, MaxCheats;
	CODEENTRY *code;

	// From a #define in Cheat.c
	// Update at a later date to have a universally allowed amount
	MaxCheats = 500;
	CheatNo = 0;

	code = CS_GetCodeAt(&cheat_dev, item);

	if (code == NULL)
		return;

	strncpy(NewCheatName, code->Name, STRING_MAX);

	for (count = 0; count < MaxCheats; count ++) {
		GetCheatName(count,CheatName,sizeof(CheatName));
		if (strlen(CheatName) == 0) {
			CheatNo = count;
			break;
		}
		if (strcmp(CheatName,NewCheatName) == 0) {
			DisplayError(GS(MSG_CHEAT_NAME_IN_USE));
			return;
		}
	}
	if (count == MaxCheats) {
		DisplayError(GS(MSG_MAX_CHEATS));
		return;
	}

	// Cheat name is in the format ROM_NAME hence +2 for the quotation marks and +1 for null terminator
	CheatLen = strlen(NewCheatName) + 3;

	cheat = NULL;
	tmp = (char*)realloc(cheat, sizeof(*cheat) * CheatLen);
	if (!tmp) return;	// Failed to allocate memory
	cheat = tmp;

	sprintf(cheat, "\"%s\"", NewCheatName);

	// Scan for additional entries and append to the current string being built
	// Each code is 14 characters long ,XXXXXXXX XXXX
	for (count = 0; ; count++) {

		code = CS_GetCodeAt(&cheat_dev, count);
		if (code == NULL)
			break;

		if (strcmp(NewCheatName, code->Name) == 0) {
			sprintf(CheatName, ",%02X%06X %04X", code->Activator, code->Address, code->Value);
			CheatLen += strlen(CheatName);
			tmp = (char*)realloc(cheat, sizeof(*cheat) * CheatLen);
			if (!tmp) return;	// Failed to allocate memory
			cheat = tmp;
			strcat(cheat, CheatName);
		}
	}

	//Add to ini
	RomID(Identifier, RomHeader);

	Settings_Write(CDB_NAME, Identifier, ROM_NAME, RomName);
	sprintf(NewCheatName, CHT_ENT, CheatNo);
	Settings_Write(CDB_NAME, Identifier, NewCheatName, cheat);
	if (cheat) { free(cheat); cheat = NULL; }

	code = CS_GetCodeAt(&cheat_dev, item);
	CheatLen = strlen(code->Note) + 5;
	if (CheatLen > 5) {
		cheat = (char*)malloc(CheatLen);
		strncpy(cheat, code->Note, CheatLen);
		sprintf(NewCheatName, CHT_ENT_N, CheatNo);
		Settings_Write(CDB_NAME, Identifier, NewCheatName, cheat);
		if (cheat) { free(cheat); cheat = NULL; }
	}

	// Remove all entries that were saved
	strncpy(NewCheatName, code->Name, STRING_MAX);

	count = 0;
	code = CS_GetCodeAt(&cheat_dev, 0);
	while (code != NULL) { 
		if (strcmp(NewCheatName, code->Name) == 0)
			CS_RemoveCodeAt(&cheat_dev, count);
		else
			count++;
		code = CS_GetCodeAt(&cheat_dev, count);
	}
}

void CS_UpdateSearchProc(HWND hDlg) {
	// Always reset to the original proc, in case there was a change to one of the other search types
	if(VALUE_SEARCH.callBack != NULL)
		SetWindowLongPtr(GetDlgItem(hDlg, IDT_SEARCH_VALUE), GWLP_WNDPROC, (LONG_PTR)VALUE_SEARCH.callBack);

	VALUE_SEARCH.hWnd = GetDlgItem(hDlg, IDT_SEARCH_VALUE);
	VALUE_SEARCH.value = 0;

	if (search.searchType == hex)
		VALUE_SEARCH.callBack = (WNDPROC)SetWindowLongPtr(VALUE_SEARCH.hWnd, GWLP_WNDPROC, (LONG_PTR)HexEditControlProc);
	else	// Default to Decimal
		VALUE_SEARCH.callBack = (WNDPROC)SetWindowLongPtr(VALUE_SEARCH.hWnd, GWLP_WNDPROC, (LONG_PTR)DecEditControlProc);

	if (search.searchNumBits == bits8)
		VALUE_SEARCH.max_value = 0xFF;
	else
		VALUE_SEARCH.max_value = 0xFFFF;
}


// This thread is created to scan through the address hits and continually update the value or text ListView control
// The updated values/text will be marked in red text
DWORD WINAPI LiveUpdate(LPVOID arg) {
	DWORD wait_result;
	HANDLE hList;

	hList = (HANDLE)arg;

	/*
	int count = 0;
	
	while (doingLiveUpdate) {
		wait_result = WaitForSingleObject(gLVMutex, INFINITE);
		
		// Got control of the mutex, that means nothing else is using the listview control
		if (wait_result == WAIT_OBJECT_0) {
			RomList_SortList();
			count = ItemList.ListCount;
		}
		
		// Abandoned mutex, why???
		if (wait_result == WAIT_ABANDONED) {
			DisplayError(GS(MSG_MEM_ALLOC_ERROR));
			ExitThread(0);
		}

		if (!ReleaseMutex(gLVMutex))
			MessageBox(NULL, "Failed to release a mutex???", "Error!", MB_OK);

		// Be sure the mutex is released, as the displaying of items is also going to use the mutex
		ListView_SetItemCount(hRomList, count);
		Sleep(50);
	}*/
	return 0;
}