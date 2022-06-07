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
#include "Cheats_Preprocessor.h"
#include "CheatSearch_Input.h"
#include "RomTools_Common.h"
#include "CheatSearch_Search.h"
#include "CheatSearch_Dev.h"

const char* N64GSCODETYPES8BIT[] = { "8-bit Constant Write (80)", "80",
	"8-bit Uncached Write (A0)", "A0",
	"8-bit GS Button (88)", "88",
	"8-Bit Equal To Activator (D0)", "D0",
	"8-Bit Different To Activator (D2)", "D2" };

const char* N64GSCODETYPES16BIT[] = { "16-bit Constant Write (81)", "81",
	"16-bit Uncached Write (A1)", "A1",
	"16-bit GS Button (89)", "89",
	"16-Bit Equal To Activator (D1)", "D1",
	"16-Bit Different To Activator (D3)", "D3" };

#define LISTVIEW_MAXSHOWN 4096
#define WM_LV_UPDATE (WM_APP + 1)


/////////////////////////////////////
// Start of Function Prototypes
/////////////////////////////////////
void Search(HWND hDlg);
void InitSearchFormatList(HWND hDlg);
void InitSearchNumBitsList(HWND hDlg);
void InitSearchResultList(HWND hDlg);
void PopulateSearchResultList(HWND hDlg);
void InitCheatCreateList(HWND hDlg);
void UpdateCheatCreateList(HWND hDlg);
void UpdateCheatCodePreview(HWND hDlg);
void Defaults_CheatSearchDlg();
void CE_UpdateControls(HWND hDlg, int item);
void CE_SaveCheat(int item);
void CS_UpdateSearchProc(HWND hDlg);
DWORD WINAPI LiveUpdate(LPVOID arg);
void StopLiveUpdate();

// The following were done to clean up the message map a little
LRESULT CheatSearchResults_FindItem(NMHDR* lParam);
LRESULT CheatSearchResults_DrawItem(HANDLE hDlg, NMHDR* lParam);
LRESULT CheatSearchResults_ReadItem(NMHDR* lParam);

CHTDEVENTRY* ReadProject64ChtDev();
void WriteProject64ChtDev(HWND hDlg);
char* ResultsToString(void);

BOOL CALLBACK CheatSearchDlgProc(HWND hwnd, UINT Message, WPARAM wParam, LPARAM lParam);

BYTE Value_ReadByte(DWORD addr);
WORD Value_ReadWord(DWORD addr);
BYTE Text_ReadByte(DWORD addr);

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

HANDLE hThread = NULL;
BOOL doingLiveUpdate = FALSE;
HANDLE hLUMutex;
/////////////////////////////////////
// End of Global Variables 
/////////////////////////////////////


/////////////////////////////////////
// Start of function implementation 
/////////////////////////////////////

void Show_CheatSearchDlg(HWND hParent) {
	if (hCheatSearchDlg != NULL) {	// Stop multiple instances
		SetForegroundWindow(hCheatSearchDlg);
		return;
	}
	// The third argument used to be hParent, this made the Dialog Window being created act as always on top of the parent window
	// Setting this to NULL makes it behave as a "normal" window that can be behind the main application instead of always on top of
	hCheatSearchDlg = CreateDialog(GetModuleHandle(NULL), MAKEINTRESOURCE(IDD_Cheats_Search), NULL, CheatSearchDlgProc);
	if (hCheatSearchDlg != NULL)
	{
		Setup_CheatSearch_Window(hCheatSearchDlg);
		ShowWindow(hCheatSearchDlg, SW_SHOW);
	}
}

void Close_CheatSearchDlg() {
	CS_ClearResults(&results);
	CS_ClearDev(&cheat_dev);

	// Stop the live update
	StopLiveUpdate();

	if (hCheatSearchDlg != NULL) {
		DestroyWindow(hCheatSearchDlg);
		hCheatSearchDlg = NULL;
	}
}

void Defaults_CheatSearchDlg() {
	searched = FALSE;
	secondSearch = FALSE;
	StopLiveUpdate();

	Button_Enable(GetDlgItem(hCheatSearchDlg, IDB_CS_CHEAT_EDIT), FALSE);
	Button_Enable(GetDlgItem(hCheatSearchDlg, IDB_CS_CHEAT_REMOVE), FALSE);
	Button_Enable(GetDlgItem(hCheatSearchDlg, IDB_CS_CHEAT_MOVEUP), FALSE);
	Button_Enable(GetDlgItem(hCheatSearchDlg, IDB_CS_CHEAT_MOVEDOWN), FALSE);
	Button_Enable(GetDlgItem(hCheatSearchDlg, IDB_CS_CHEAT_ENABLE), FALSE);
	Button_Enable(GetDlgItem(hCheatSearchDlg, IDB_CS_CHEAT_SAVE), FALSE);
}

char* trimwhitespace(char* str) {
	char* end;

	// Trim leading space
	while (isspace(*str)) str++;
	if (*str == 0)  // All spaces? 
		return str;

	// Trim trailing space
	end = str + strlen(str) - 1;
	while (end > str && isspace(*end)) end--;

	// Write new null terminator 
	*(end + 1) = 0;

	return str;
}

BOOL CALLBACK CheatSearchDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch (uMsg) {

		case WM_CLOSE:
			Close_CheatSearchDlg();
			break;

		case WM_NOTIFY: {
			switch (((LPNMHDR)lParam)->code) {

				case NM_DBLCLK: {
					switch (((LPNMHDR)lParam)->idFrom) {
						case IDL_SEARCH_RESULT_LIST: {
							int item = ((LPNMITEMACTIVATE)lParam)->iItem;
							CODEENTRY tmp;
							CS_HITS hit;

							if (item == -1)
								break;

							hit = *CS_GetHit(&results, item);
							tmp.Address = hit.address;
							tmp.Value = hit.value;
							tmp.numBits = search.searchNumBits;
							tmp.Enabled = FALSE;
							tmp.searchBy = search.searchBy;
							strcpy(tmp.Name, "");
							strcpy(tmp.Note, "");
							strcpy(tmp.Text, "");

							cheat_dev.modify = &tmp;

							// Only save the code if okay was pressed
							if (DialogBox(hInst, MAKEINTRESOURCE(IDD_Cheats_Search_Edit), hDlg, (DLGPROC)CheatSearch_Add_Proc) == IDOK) {
								CS_AddCode(&cheat_dev, tmp);
								UpdateCheatCreateList(hDlg);
							}
							break;
						}

						case IDL_CS_CHEAT_CREATE: {	// Double click to allow Edit cheat shortcut
							int item = ((LPNMITEMACTIVATE)lParam)->iItem;
							CODEENTRY tmp;

							tmp.Address = 0;
							tmp.Value = 0;
							tmp.numBits = search.searchNumBits;
							tmp.Enabled = FALSE;
							tmp.searchBy = search.searchBy;
							strcpy(tmp.Name, "");
							strcpy(tmp.Note, "");
							strcpy(tmp.Text, "");

							cheat_dev.modify = &tmp;

							// Double clicking on the empty space in the list for the cheat dev area will add a new entry
							if (item == -1) {
								if (DialogBox(hInst, MAKEINTRESOURCE(IDD_Cheats_Search_Edit), hDlg, (DLGPROC)CheatSearch_Add_Proc) == IDOK) {
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
				}	// End of NM_DBLCLK

				case NM_CLICK: {
					switch (((LPNMHDR)lParam)->idFrom) {
						case IDL_CS_CHEAT_CREATE:	// To toggle the buttons as the item is clicked in the cheat creation section
							CE_UpdateControls(hDlg, ((LPNMITEMACTIVATE)lParam)->iItem);
							break;
					}
					break;
				}

				case NM_CUSTOMDRAW: {
					if (wParam == IDL_SEARCH_RESULT_LIST)
						return CheatSearchResults_DrawItem(hDlg, (LPNMHDR)lParam);
					break;
				}

				case LVN_GETDISPINFO: {
					if (wParam == IDL_SEARCH_RESULT_LIST)
						return CheatSearchResults_ReadItem((LPNMHDR)lParam);
					break;
				}

				case LVN_ODFINDITEM: {
					if (wParam == IDL_SEARCH_RESULT_LIST) {
						SetWindowLong(hDlg, DWL_MSGRESULT, CheatSearchResults_FindItem((LPNMHDR)lParam));
						return TRUE;
					}
					break;
				}
			}
			break;
		}	// End of WM_NOTIFY

		case WM_COMMAND: {
			switch (LOWORD(wParam)) {

				case IDR_SEARCH_VALUE: {
					Edit_Enable(GetDlgItem(hDlg, IDT_SEARCH_TEXT), FALSE);
					Edit_Enable(GetDlgItem(hDlg, IDT_SEARCH_VALUE), TRUE);
					break;
				}

				case IDR_SEARCH_TEXT: {
					Edit_Enable(GetDlgItem(hDlg, IDT_SEARCH_TEXT), TRUE);
					Edit_Enable(GetDlgItem(hDlg, IDT_SEARCH_VALUE), FALSE);
					break;
				}

				case IDB_CS_SEARCH: {
					Search(hDlg);
					if (search.searchBy == searchbyvalue)
						WriteProject64ChtDev(hDlg);
					break;
				}

				case IDB_CS_RESET: {
					ListView_SetItemCount(GetDlgItem(hDlg, IDL_SEARCH_RESULT_LIST), 0);
					CS_ClearResults(&results);
					Defaults_CheatSearchDlg();
					InitSearchFormatList(hDlg);
					ComboBox_Enable(GetDlgItem(hDlg, IDC_SEARCH_NUMBITS), TRUE);
					ComboBox_Enable(GetDlgItem(hDlg, IDC_ADDRESS_RANGE), TRUE);
					Edit_Enable(GetDlgItem(hDlg, IDT_SEARCH_VALUE), TRUE);
					Edit_SetText(GetDlgItem(hDlg, IDT_SEARCH_VALUE), "");
					Edit_Enable(GetDlgItem(hDlg, IDT_SEARCH_TEXT), FALSE);
					Edit_SetText(GetDlgItem(hDlg, IDT_SEARCH_TEXT), "");
					Button_SetCheck(GetDlgItem(hDlg, IDR_SEARCH_VALUE), BST_CHECKED);
					Button_SetCheck(GetDlgItem(hDlg, IDR_SEARCH_TEXT), BST_UNCHECKED);
					SetWindowText(GetDlgItem(hDlg, IDGB_CS_RESULTS), "Results");
					break;
				}

				case IDB_CS_DUMPMEM: {
					Show_MemDumpDlg(hDlg);
					//SaveMemDump(hDlg);
					//DumpPCAndDisassembled(hDlg, 0x80000400, 0x80001000);
					break;
				}

				case IDC_SEARCH_HEXDEC: {
					if (HIWORD(wParam) == CBN_SELCHANGE) {
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
					}
					break;
				}

				case IDC_SEARCH_NUMBITS: {
					if (HIWORD(wParam) == CBN_SELCHANGE) {
						search.searchNumBits = (NUMBITS)ComboBox_GetCurSel((HWND)lParam);
						CS_UpdateSearchProc(hDlg);
						Edit_SetText(GetDlgItem(hDlg, IDT_SEARCH_VALUE), "");
					}
					break;
				}

				case IDCANCEL: {
					EndDialog(hDlg, IDCANCEL);
					break;
				}

				case IDB_CS_CHEAT_EDIT: {
					int sItem = ListView_GetNextItem(GetDlgItem(hDlg, IDL_CS_CHEAT_CREATE), -1, LVNI_FOCUSED);
					CODEENTRY* cItem = CS_GetCodeAt(&cheat_dev, sItem);

					// Should not happen (Out of bounds)
					if (sItem == -1 || cItem == NULL) break;

					cheat_dev.modify = cItem;	// Address of item to be modified

					if (DialogBox(hInst, MAKEINTRESOURCE(IDD_Cheats_Search_Edit), hDlg, (DLGPROC)CheatSearch_Add_Proc) == IDOK) {
						UpdateCheatCreateList(hDlg);
						CE_UpdateControls(hDlg, sItem);
					}
					break;
				}

				case IDB_CS_CHEAT_REMOVE: {
					CS_RemoveCodeAt(&cheat_dev, ListView_GetNextItem(GetDlgItem(hDlg, IDL_CS_CHEAT_CREATE), -1, LVNI_SELECTED));
					UpdateCheatCreateList(hDlg);
					CE_UpdateControls(hDlg, -1);
					break;
				}

				case IDB_CS_CHEAT_MOVEUP: {
					int item = ListView_GetNextItem(GetDlgItem(hDlg, IDL_CS_CHEAT_CREATE), -1, LVNI_SELECTED);

					// Nothing selected or already top-most item
					if (item == -1 || item == 0)
						break;

					CS_SwapDev(&cheat_dev, item, item - 1);

					// Repopulate the list and update controls
					UpdateCheatCreateList(hDlg);
					CE_UpdateControls(hDlg, item - 1);
					break;
				}

				case IDB_CS_CHEAT_MOVEDOWN: {
					int item = ListView_GetNextItem(GetDlgItem(hDlg, IDL_CS_CHEAT_CREATE), -1, LVNI_SELECTED);

					// Nothing selected
					if (item == -1)
						break;

					CS_SwapDev(&cheat_dev, item, item + 1);

					// Repopulate the list and update controls
					UpdateCheatCreateList(hDlg);
					CE_UpdateControls(hDlg, item + 1);
					break;
				}

				case IDB_CS_CHEAT_ENABLE: {
					int item = ListView_GetNextItem(GetDlgItem(hDlg, IDL_CS_CHEAT_CREATE), -1, LVNI_SELECTED);
					CODEENTRY* cheat;

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

				case IDB_CS_CHEAT_SAVE: {
					int item = ListView_GetNextItem(GetDlgItem(hDlg, IDL_CS_CHEAT_CREATE), -1, LVNI_SELECTED);

					if (item == -1)
						break;

					CE_SaveCheat(item);
					MessageBox(hDlg, "Cheat has been added to the database.\nAccess through normal cheat window.", "Cheat Saved", MB_OK);
					UpdateCheatCreateList(hDlg);
					CE_UpdateControls(hDlg, -1);	// Save eats the item so default to nothing
					break;
				}

				case IDT_SEARCH_VALUE: {
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
			}
			break;
		}	// End of WM_COMMAND

		case WM_LV_UPDATE: {
			InvalidateRect(GetDlgItem(hDlg, IDL_SEARCH_RESULT_LIST), NULL, TRUE);
			break;
		}

		default:
			return FALSE;
	}
	return TRUE;
}

void Setup_CheatSearch_Window(HWND hParent) {
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

	X = 0;
	Y = 0;

	if (!GetStoredWinPos("CheatSearch", &X, &Y)) {
		X = (GetSystemMetrics(SM_CXSCREEN) - WindowWidth) / 2;
		Y = (GetSystemMetrics(SM_CYSCREEN) - WindowHeight) / 2;
	}
	SetWindowPos(hParent, NULL, X, Y, WindowWidth, WindowHeight, SWP_NOZORDER | SWP_SHOWWINDOW | SWP_NOSIZE);
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
	ListView_SetItemCount(hListView, 0);

	col.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
	col.fmt = LVCFMT_LEFT;

	// Clear and remove any previous incarnations of the ListView control
	while (ListView_DeleteColumn(hListView, 0));

	state = Button_GetCheck(GetDlgItem(hDlg, IDR_SEARCH_TEXT));
	if (state == BST_CHECKED) {
		// Build the results list for Text Search
		col.pszText = "Address";
		col.cx = 110;
		col.iSubItem = 0;
		ListView_InsertColumn(hListView, 0, &col);

		col.pszText = "Text";
		col.cx = 150;
		col.iSubItem = 1;
		ListView_InsertColumn(hListView, 1, &col);

		col.pszText = "Old Text";
		col.cx = 150;
		col.iSubItem = 2;
		ListView_InsertColumn(hListView, 2, &col);
	}
	else {
		// Build the results list for Value
		col.pszText = "Address";
		col.cx = 110;
		col.iSubItem = 0;
		ListView_InsertColumn(hListView, 0, &col);

		col.pszText = "Value (Hex)";
		col.cx = 70;
		col.iSubItem = 1;
		ListView_InsertColumn(hListView, 1, &col);

		col.pszText = "Value (Dec)";
		col.cx = 70;
		col.iSubItem = 2;
		ListView_InsertColumn(hListView, 2, &col);

		col.pszText = "Old (Hex)";
		col.cx = 70;
		col.iSubItem = 3;
		ListView_InsertColumn(hListView, 3, &col);
	}
}

void PopulateSearchResultList(HWND hDlg) {
	char title[STRING_MAX];

	ListView_SetItemCount(GetDlgItem(hDlg, IDL_SEARCH_RESULT_LIST), results.num_stored);

	sprintf(title, "Results (found %u)", results.num_stored);
	SetWindowText(GetDlgItem(hDlg, IDGB_CS_RESULTS), title);
}

void InitCheatCreateList(HWND hDlg) {
	HWND hListView;
	LVCOLUMN col;

	hListView = GetDlgItem(hDlg, IDL_CS_CHEAT_CREATE);

	col.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH | LVCF_ORDER;
	col.iOrder = 0;
	col.pszText = "Act.";
	col.fmt = LVCFMT_IMAGE;
	col.cx = 40;
	col.iSubItem = 0;
	ListView_InsertColumn(hListView, 0, &col);

	col.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH | LVCF_ORDER;
	col.iOrder = 1;
	col.pszText = "Code";
	col.fmt = LVCFMT_LEFT;
	col.cx = 100;
	col.iSubItem = 1;
	ListView_InsertColumn(hListView, 1, &col);

	col.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH | LVCF_ORDER;
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
	CODEENTRY* curr;

	// Add to the list view
	hListView = GetDlgItem(hDlg, IDL_CS_CHEAT_CREATE);
	ListView_DeleteAllItems(hListView);

	ctr = 0;
	curr = CS_GetCodeAt(&cheat_dev, 0);

	while (curr != NULL) {
		item.mask = LVIF_TEXT;
		if (curr->Enabled)
			item.pszText = "On";
		else
			item.pszText = "Off";
		item.iItem = ctr;
		item.iSubItem = 0;
		ListView_InsertItem(hListView, &item);

		if (curr->numBits == bits8)
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
}

void Search(HWND hDlg) {
	char startAddress[11], endAddress[11];
	DWORD dwstartAddress, dwendAddress, count;

	StopLiveUpdate();

	// Reset the results window, this depends on what was ticked.
	InitSearchResultList(hDlg);

	// Address search range
	Edit_GetText(GetDlgItem(hDlg, IDT_ADDRESS_START), startAddress, 11);
	Edit_GetText(GetDlgItem(hDlg, IDT_ADDRESS_END), endAddress, 11);

	dwstartAddress = strtoul(startAddress, NULL, 16);
	dwendAddress = strtoul(endAddress, NULL, 16);

	// Value search
	if (Button_GetCheck(GetDlgItem(hDlg, IDR_SEARCH_VALUE)) == BST_CHECKED) {
		WORD searchValue, memValue;

		search.searchBy = searchbyvalue;

		// Value to search by, it's either in Hex or Dec
		Edit_GetText(GetDlgItem(hDlg, IDT_SEARCH_VALUE), search.search_string, 9);
		if (search.searchType == hex)
			searchValue = (WORD)strtoul(search.search_string, NULL, 16);
		else
			searchValue = (WORD)strtoul(search.search_string, NULL, 10);

		// The initial search will either add all addresses (unknown search)
		//	or addresses that match the search value (8bit or 16bit)
		if (!searched) {

			// Reserve space for the initial search, absolute worst case scenario is for unknown search
			if (search.searchType == unknown)
				CS_ReserveSpace(&results, dwendAddress - dwstartAddress);

			if (search.searchNumBits == bits8) {
				for (count = dwstartAddress; count < dwendAddress; count++) {
					memValue = Value_ReadByte(count);
					if (search.searchType == unknown || ((BYTE)searchValue == memValue))
						CS_AddResult(&results, count, memValue);
				}
			}
			else {
				for (count = dwstartAddress; count < dwendAddress; count += 2) {
					memValue = Value_ReadWord(count);
					if (search.searchType == unknown || (searchValue == memValue))
						CS_AddResult(&results, count, memValue);
				}
			}

			if (search.searchType != unknown)
				searched = TRUE;
			ComboBox_Enable(GetDlgItem(hDlg, IDC_SEARCH_NUMBITS), FALSE);
			ComboBox_Enable(GetDlgItem(hDlg, IDC_ADDRESS_RANGE), FALSE);
		}

		// Search performed using previous (or the initial) search result list
		else {
			struct CS_RESULTS tmp = { 0 };
			WORD mem_value;
			CS_HITS* hit;
			BOOL matched;

			CS_ReserveSpace(&tmp, dwendAddress - dwstartAddress);

			// Subsequent searches of Hex/Dec values
			for (count = 0; count < results.num_stored; count++) {

				hit = CS_GetHit(&results, count);

				if (search.searchNumBits == bits8)
					mem_value = Value_ReadByte(hit->address);
				else
					mem_value = Value_ReadWord(hit->address);

				switch (search.searchType) {
					case dec:
					case hex:
						matched = (searchValue == mem_value);
						break;
					case unknown:
						matched = TRUE;
						break;
					case changed:
						matched = (hit->value != mem_value);
						break;
					case unchanged:
						matched = (hit->value == mem_value);
						break;
					case lower:
						matched = (hit->value > mem_value);
						break;
					case higher:
						matched = (hit->value < mem_value);
						break;
					default:
						matched = FALSE;
						break;
				}

				if (!matched)
					continue;

				hit->prev_value = hit->value;
				hit->value = mem_value;
				CS_AddHit(&tmp, hit);
			}

			secondSearch = TRUE;
			CS_ClearResults(&results);
			results = tmp;
		}
		searched = TRUE;
	}

	// Text search
	else if (Button_GetCheck(GetDlgItem(hDlg, IDR_SEARCH_TEXT)) == BST_CHECKED) {
		DWORD possibleAddress, count2, textsearch_len;

		search.searchBy = searchbytext;

		CS_ClearResults(&results);

		possibleAddress = 0;

		// Fetch the string being searched for
		Edit_GetText(GetDlgItem(hDlg, IDT_SEARCH_TEXT), search.search_string, STRING_MAX - 1);
		textsearch_len = strlen(search.search_string);

		for (count = dwstartAddress, count2 = 0; count < dwendAddress; count++) {

			// Pattern match
			if ((BYTE)search.search_string[count2] == Text_ReadByte(count + 1)) {
				if (count2 == 0) {
					possibleAddress = count + 1;
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
	}

	else
		return;

	PopulateSearchResultList(hDlg);

	// Update to include changed, unchanged, higher, lower, etc...
	InitSearchFormatList(hDlg);

	// The Live Update thread (hThread should always be NULL since Live Update is stopped before searching)
	if (hThread == NULL)
		hThread = CreateThread(NULL, 0, LiveUpdate, hDlg, 0, NULL);
}

LRESULT CALLBACK CheatSearch_Add_Proc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	HWND hComboBox;
	int Count;
	char strAddress[9], strValue[7];

	switch (uMsg) {
		case WM_INITDIALOG: {
			// Populate ComboBox with GS Cheat Types
			hComboBox = GetDlgItem(hDlg, IDC_CSE_GSCODETYPE);
			ComboBox_ResetContent(hComboBox);

			// Subclass HexEditControls for Address
			ADDR_HEX.hWnd = GetDlgItem(hDlg, IDT_CSE_ADDRESS);
			ADDR_HEX.callBack = (WNDPROC)SetWindowLongPtr(ADDR_HEX.hWnd, GWLP_WNDPROC, (LONG_PTR)HexEditControlProc);
			ADDR_HEX.value = cheat_dev.modify->Address;
			ADDR_HEX.max_value = RdramSize - 1;

			if (cheat_dev.modify->searchBy == searchbyvalue) {
				Edit_Enable(GetDlgItem(hDlg, IDT_CSE_VALUE_HEX), TRUE);
				Edit_Enable(GetDlgItem(hDlg, IDT_CSE_VALUE_DEC), TRUE);
				Edit_Enable(GetDlgItem(hDlg, IDT_CSE_TEXT), FALSE);
				Edit_Enable(GetDlgItem(hDlg, IDT_CSE_PREVIEW), TRUE);

				switch (cheat_dev.modify->numBits) {
					int loc;
					char check[3];

					case bits8:
						for (Count = 0, loc = 0; Count < 5; Count++) {
							ComboBox_AddString(hComboBox, N64GSCODETYPES8BIT[Count * 2]);
							sprintf(check, "%2X", cheat_dev.modify->Activator);
							if (strcmp(check, N64GSCODETYPES8BIT[Count * 2 + 1]) == 0)
								loc = Count;
						}
						ComboBox_SetCurSel(hComboBox, loc);
						break;

					case bits16:
						for (Count = 0, loc = 0; Count < 5; Count++) {
							ComboBox_AddString(hComboBox, N64GSCODETYPES16BIT[Count * 2]);
							sprintf(check, "%2X", cheat_dev.modify->Activator);
							if (strcmp(check, N64GSCODETYPES16BIT[Count * 2 + 1]) == 0)
								loc = Count;
						}
						ComboBox_SetCurSel(hComboBox, loc);
						break;
				}

				// Subclass HexEditControls for Hex Value (Linked with VALUE_DEC later on)
				VALUE_HEX.hWnd = GetDlgItem(hDlg, IDT_CSE_VALUE_HEX);
				VALUE_HEX.callBack = (WNDPROC)SetWindowLongPtr(VALUE_HEX.hWnd, GWLP_WNDPROC, (LONG_PTR)HexEditControlProc);
				VALUE_HEX.value = cheat_dev.modify->Value;
				if (cheat_dev.modify->numBits == bits8)
					VALUE_HEX.max_value = 0xFF;
				else
					VALUE_HEX.max_value = 0xFFFF;

				// Subclass DecEditControls for Dec Value (Linked with VALUE_HEX later on)
				VALUE_DEC.hWnd = GetDlgItem(hDlg, IDT_CSE_VALUE_DEC);
				VALUE_DEC.callBack = (WNDPROC)SetWindowLongPtr(VALUE_DEC.hWnd, GWLP_WNDPROC, (LONG_PTR)DecEditControlProc);
				VALUE_DEC.value = cheat_dev.modify->Value;
				if (cheat_dev.modify->numBits == bits8)
					VALUE_DEC.max_value = 0xFF;
				else
					VALUE_DEC.max_value = 0xFFFF;

				sprintf((char*)&strValue, "%X", cheat_dev.modify->Value);
				Edit_SetText(GetDlgItem(hDlg, IDT_CSE_VALUE_HEX), strValue);
				sprintf((char*)&strValue, "%u", cheat_dev.modify->Value);
				Edit_SetText(GetDlgItem(hDlg, IDT_CSE_VALUE_DEC), strValue);

				UpdateCheatCodePreview(hDlg);
			}

			// Searching by text
			else {
				Edit_Enable(GetDlgItem(hDlg, IDT_CSE_VALUE_HEX), FALSE);
				Edit_Enable(GetDlgItem(hDlg, IDT_CSE_VALUE_DEC), FALSE);
				Edit_Enable(GetDlgItem(hDlg, IDT_CSE_TEXT), TRUE);
				Edit_Enable(GetDlgItem(hDlg, IDT_CSE_PREVIEW), FALSE);

				Edit_SetText(GetDlgItem(hDlg, IDT_CSE_TEXT), cheat_dev.modify->Text);
			}

			// Had to move these after subclassing the controls to avoid a race condition between setting and checking the values
			sprintf((char*)&strAddress, "%06X", cheat_dev.modify->Address);
			Edit_SetText(GetDlgItem(hDlg, IDT_CSE_ADDRESS), strAddress);
			Edit_SetText(GetDlgItem(hDlg, IDT_CSE_Name), cheat_dev.modify->Name);
			Edit_SetText(GetDlgItem(hDlg, IDT_CSE_NOTE), cheat_dev.modify->Note);

			break;
		}

		case WM_COMMAND: {
			switch (LOWORD(wParam)) {
				case IDOK:
				{
					char s[STRING_MAX], * trimmed;
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
					if (cheat_dev.modify->searchBy == searchbyvalue) {
						// Value
						Edit_GetText(GetDlgItem(hDlg, IDT_CSE_VALUE_DEC), s, STRING_MAX);
						cheat_dev.modify->Value = (WORD)strtoul(s, NULL, 10);

						// Activator
						selActivator = ComboBox_GetCurSel(GetDlgItem(hDlg, IDC_CSE_GSCODETYPE));
						if (cheat_dev.modify->numBits == bits8)
							cheat_dev.modify->Activator = (BYTE)strtoul(N64GSCODETYPES8BIT[selActivator * 2 + 1], NULL, 16);
						else
							cheat_dev.modify->Activator = (BYTE)strtoul(N64GSCODETYPES16BIT[selActivator * 2 + 1], NULL, 16);
					}

					// Search by TEXT
					else if (cheat_dev.modify->searchBy == searchbytext) {
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

							if (cheat_dev.modify->numBits == bits8)
								VALUE_HEX.max_value = 0xFF;
							else
								VALUE_HEX.max_value = 0xFFFF;

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

							if (cheat_dev.modify->numBits == bits8) {
								VALUE_DEC.max_value = 0xFF;
							}
							else
								VALUE_DEC.max_value = 0xFFFF;

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
		}

		default:
			return FALSE;
	}
	return TRUE;
}

void UpdateCheatCodePreview(HWND hDlg) {
	int ndxCodeType;
	char address[7];
	char value[5];
	char s[14];	// Maximum of 8 (activator and address) + 1 (space) + 4 (16bit value) + 1 (null character)

	// Nothing to update if not searching by value
	if (search.searchBy != searchbyvalue)
		return;

	ndxCodeType = ComboBox_GetCurSel(GetDlgItem(hDlg, IDC_CSE_GSCODETYPE));
	Edit_GetText(GetDlgItem(hDlg, IDT_CSE_ADDRESS), address, 7);
	Edit_GetText(GetDlgItem(hDlg, IDT_CSE_VALUE_HEX), value, 5);

	if (search.searchNumBits == bits8)
		sprintf(s, "%02s%06s %02s", N64GSCODETYPES8BIT[ndxCodeType * 2 + 1], address, value);
	else
		sprintf(s, "%02s%06s %04s", N64GSCODETYPES16BIT[ndxCodeType * 2 + 1], address, value);

	Edit_SetText(GetDlgItem(hDlg, IDT_CSE_PREVIEW), s);
}

/********************************************************************************************
GetCheatDevIniFileName

Purpose:
Parameters:
Returns:

********************************************************************************************/
char* GetCheatDevIniFileName(void) {
	char path_buffer[_MAX_PATH], drive[_MAX_DRIVE], dir[_MAX_DIR];
	char fname[_MAX_FNAME], ext[_MAX_EXT];
	static char IniFileName[_MAX_PATH];

	GetModuleFileName(NULL, path_buffer, sizeof(path_buffer));
	_splitpath(path_buffer, drive, dir, fname, ext);
	sprintf(IniFileName, "%s%s%s", drive, dir, CHTDEV_NAME);
	return IniFileName;
}

CHTDEVENTRY* ReadProject64ChtDev() {
	FILE* pFile;
	size_t lSize, result;
	char* buffer;
	//CHTDEVENTRY * ChtDev;

	pFile = fopen(GetCheatDevIniFileName(), "rb");
	if (pFile == NULL) {
		// file error
		return NULL;
	}

	// obtain file size:
	fseek(pFile, 0, SEEK_END);
	lSize = ftell(pFile);
	rewind(pFile);

	// allocate memory to contain the whole file:
	buffer = (char*)malloc(sizeof(char) * lSize);
	if (buffer == NULL) {
		// Memory error
		return NULL;
	}

	// copy the file into the buffer:
	result = fread(buffer, 1, lSize, pFile);
	if (result != lSize) {
		// Reading error
		return NULL;
	}

	/* the whole file is now loaded in the memory buffer. */

	// terminate
	fclose(pFile);
	free(buffer);

	return NULL; //ChtDev;
}

void WriteProject64ChtDev(HWND hDlg) {
	FILE* pFile;

	char Identifier[100];
	char s[16];
	CHTDEVENTRY* ChtDev;

	//ChtDev = ReadProject64ChtDev(GetCheatDevIniFileName());
	ChtDev = (CHTDEVENTRY*)malloc(sizeof(CHTDEVENTRY));
	if (ChtDev == NULL)
		return;

	RomID(Identifier, RomHeader);

	ChtDev->Identifier = Identifier;
	ChtDev->Name = RomFullName;
	ChtDev->LastSearch = (LASTSEARCH*)malloc(sizeof(LASTSEARCH));
	if (ChtDev->LastSearch == NULL)
		return;
	ChtDev->LastSearch->SearchType = "Value";

	Edit_GetText(GetDlgItem(hDlg, IDT_SEARCH_VALUE), s, 9);
	ChtDev->LastSearch->SearchValue = s;

	ChtDev->LastSearch->ValueSearchType = search.searchType;
	ChtDev->LastSearch->NumBits = search.searchNumBits;
	ChtDev->LastSearch->Results = ResultsToString();

	pFile = fopen(GetCheatDevIniFileName(), "wb");
	if (pFile == NULL)
		return;	// File Error

	fprintf(pFile, "<?xml version=""1.0"" encoding=""ISO-8859-1""?>\r\n");
	fprintf(pFile, "<CheatDev>\r\n");
	fprintf(pFile, "\t<Game>\r\n");
	fprintf(pFile, "\t\t<ID>%s</ID>\r\n", ChtDev->Identifier);
	fprintf(pFile, "\t\t<Name>%s</Name>\r\n", ChtDev->Name);
	fprintf(pFile, "\t\t<LastSearch>\r\n");
	fprintf(pFile, "\t\t\t<SearchType>%s</SearchType>\r\n", ChtDev->LastSearch->SearchType);
	fprintf(pFile, "\t\t\t<Value>%s</Value>\r\n", ChtDev->LastSearch->SearchValue);

	switch (ChtDev->LastSearch->ValueSearchType) {
		case unknown:
			fprintf(pFile, "\t\t\t<ValueSearchType>%s</ValueSearchType>\r\n", "Unknown");
			break;
		case dec:
			fprintf(pFile, "\t\t\t<ValueSearchType>%s</ValueSearchType>\r\n", "Decimal");
			break;
		case hex:
			fprintf(pFile, "\t\t\t<ValueSearchType>%s</ValueSearchType>\r\n", "Hexadecimal");
			break;
		case changed:
			fprintf(pFile, "\t\t\t<ValueSearchType>%s</ValueSearchType>\r\n", "Changed");
			break;
		case unchanged:
			fprintf(pFile, "\t\t\t<ValueSearchType>%s</ValueSearchType>\r\n", "Unchanged");
			break;
		case higher:
			fprintf(pFile, "\t\t\t<ValueSearchType>%s</ValueSearchType>\r\n", "Higher");
			break;
		case lower:
			fprintf(pFile, "\t\t\t<ValueSearchType>%s</ValueSearchType>\r\n", "Lower");
			break;
		default:
			fprintf(pFile, "\t\t\t<ValueSearchType>%s</ValueSearchType>\r\n", "Unsupported");
			break;
	}

	if (ChtDev->LastSearch->NumBits == bits8)
		fprintf(pFile, "\t\t\t<NumBits>%s</NumBits>\r\n", "8bit");
	else
		fprintf(pFile, "\t\t\t<NumBits>%s</NumBits>\r\n", "16bit");

	fprintf(pFile, "\t\t\t<Results>\r\n%s\r\n\t\t\t</Results>\r\n", ChtDev->LastSearch->Results);
	fprintf(pFile, "\t\t</LastSearch>\r\n");
	fprintf(pFile, "\t</Game>\r\n");

	fprintf(pFile, "</CheatDev>\r\n");

	// terminate
	fclose(pFile);

	free(ChtDev->LastSearch);
	free(ChtDev);
}

// TO DO!
// Rewrite this and make it crash recovery compatible
// Currently it only backs up the listview shown, it would break future searches if loaded
// -- Assuming there were over 4096 hits (Or whatever LISTVIEW_MAXSHOW is)
char* ResultsToString(void) {
	DWORD count, maxlength = 0;
	char s[20], * result = NULL;
	CS_HITS* hit;

	if (results.num_stored == 0)
		return result;

	maxlength = (sizeof(s) * min(results.num_stored, LISTVIEW_MAXSHOWN) + 1);
	maxlength += (6 * (maxlength / 8));	// Extra space to store \r\n\t\t\t\t

	result = calloc(maxlength, sizeof(*result));

	if (result == NULL)
		return result;

	strcat_s(result, maxlength, "\t\t\t\t");

	for (count = 0; count < min(results.num_stored, LISTVIEW_MAXSHOWN); count++) {
		hit = CS_GetHit(&results, count);

		if (count != 0 && count % 8 != 0)
			strcat_s(result, maxlength, ",   ");

		// 8 hex, a comma, up to 4 hex, a comma, 4 hex, a comma, null character
		// So 20 minimum to do a copy
		if (search.searchNumBits == bits8) {
			if (secondSearch)
				sprintf_s(s, sizeof(s), "%06X %02X %02X", hit->address, hit->value, hit->prev_value);
			else
				sprintf_s(s, sizeof(s), "%06X %02X", hit->address, hit->value);
		}
		else {
			if (secondSearch)
				sprintf_s(s, sizeof(s), "%06X %04X %04X", hit->address, hit->value, hit->prev_value);
			else
				sprintf_s(s, sizeof(s), "%06X %04X", hit->address, hit->value);
		}

		strcat_s(result, maxlength, s);

		if (count < min(results.num_stored, LISTVIEW_MAXSHOWN) - 1) {
			if ((count + 1) % 8 == 0)
				strcat_s(result, maxlength, "\r\n\t\t\t\t");
		}
	}

	return result;
}

// Called on CPU.c's RefreshScreen () on each VI
void Apply_CheatSearchDev() {
	DWORD counter;
	GAMESHARK_CODE code;
	CODEENTRY* curr;

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
				DWORD count;

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
	CODEENTRY* curr, * next;

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

// Save the cheat to file
// All entries with the same name as item's shall be saved and removed
void CE_SaveCheat(int item) {
	int CheatLen, count;
	CODEENTRY* code;
	CHEAT make_cheat = { 0 };

	code = CS_GetCodeAt(&cheat_dev, item);

	if (code == NULL)
		return;

	// Direct copy of these, no processing needed
	Cheats_Store(&make_cheat.name, code->Name, sizeof(code->Name));
	Cheats_Store(&make_cheat.note, code->Note, sizeof(code->Note));

	// For storing the null character start at 1
	CheatLen = 1;

	// Build the code string (It is currently stored in an array
	// Each code is 14 characters long ,XXXXXXXX XXXX
	for (count = 0; ; count++) {

		code = CS_GetCodeAt(&cheat_dev, count);
		if (code == NULL)
			break;

		if (strcmp(make_cheat.name, code->Name) == 0) {
			char junk[50], * tmp = NULL;

			sprintf(junk, ",%02X%06X %04X", code->Activator, code->Address, code->Value);
			CheatLen += strlen(junk);
			tmp = realloc(make_cheat.codestring, sizeof(*make_cheat.codestring) * CheatLen);
			if (!tmp)
				return;	// Failed to allocate memory

			// First entry
			if (make_cheat.codestring == NULL) {
				make_cheat.codestring = tmp;
				strcpy(make_cheat.codestring, junk);
			}
			// Additional entries, append them
			else {
				make_cheat.codestring = tmp;
				strcat(make_cheat.codestring, junk);
			}
		}
	}

	// Write the cheat out
	if (Cheats_Write(&make_cheat)) {
		// Remove all entries with the same name
		for (count = 0; ; count++) {
			code = CS_GetCodeAt(&cheat_dev, count);
			if (code == NULL)
				break;

			if (strcmp(make_cheat.name, code->Name) == 0) {
				CS_RemoveCodeAt(&cheat_dev, count);

				// Revalidate the iterator
				count--;
			}
		}
	}

	// Clean-up
	Cheats_ClearCheat(&make_cheat);
}

void CS_UpdateSearchProc(HWND hDlg) {
	// Always reset to the original proc, in case there was a change to one of the other search types
	if (VALUE_SEARCH.callBack != NULL)
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

// This thread is created to update the list view in real time without affecting performance of the game or interface
DWORD WINAPI LiveUpdate(LPVOID arg) {
	DWORD wait_result;

	if (hLUMutex == NULL)
		hLUMutex = CreateMutex(NULL, FALSE, NULL);

	// Failed to create the mutex
	if (hLUMutex == NULL)
		return 0;

	wait_result = WaitForSingleObject(hLUMutex, INFINITE);

	if (wait_result == WAIT_OBJECT_0)
		doingLiveUpdate = TRUE;
	ReleaseMutex(hLUMutex);

	while (TRUE) {
		Sleep(150);

		wait_result = WaitForSingleObject(hLUMutex, INFINITE);

		if (wait_result == WAIT_OBJECT_0 && doingLiveUpdate == TRUE) {
			SendMessage(arg, WM_LV_UPDATE, 0, 0);
			ReleaseMutex(hLUMutex);
		}
		else {
			ReleaseMutex(hLUMutex);
			return 0;
		}
	}
}

void StopLiveUpdate() {
	DWORD wait_result;

	// Nothing to stop
	if (hThread == NULL || hLUMutex == NULL)
		return;

	// Atomic access, update doingLiveUpdate to stop the thread
	wait_result = WaitForSingleObject(hLUMutex, INFINITE);
	if (wait_result == WAIT_OBJECT_0)
		doingLiveUpdate = FALSE;
	ReleaseMutex(hLUMutex);

	if (hThread == NULL)
		return;

	WaitForSingleObject(hThread, INFINITE);
	hThread = NULL;
}

// Value search
BYTE Value_ReadByte(DWORD addr) {
	switch (addr & 0x3) {
		case 3:
		case 2:
			// Covers 2 3 6 7 A B E F
			return *(BYTE*)(N64MEM + addr - 2);
		default:
			// Covers 0 1 4 5 8 9 C D
			return *(BYTE*)(N64MEM + addr + 2);
	}
}

// If memory contains DWORD W X Y Z we want to read WORD Z Y first and WORD X W second
WORD Value_ReadWord(DWORD addr) {
	// Covers 2 3 6 7 A B E F
	if (addr & 0x3 || addr & 0x2) {
		return ((WORD)(*(BYTE*)(N64MEM + addr - 1)) << 8) + *(BYTE*)(N64MEM + addr - 2);
	}
	// Covers 0 1 4 5 8 9 C D
	else {
		return ((WORD)(*(BYTE*)(N64MEM + addr + 3)) << 8) + *(BYTE*)(N64MEM + addr + 2);
	}
}

// Text search
BYTE Text_ReadByte(DWORD addr) {
	switch (addr & 0x3) {
		case 3:
			// Covers 3 6 B F
			return *(BYTE*)(N64MEM + addr - 3);
		case 2:
			// Covers 2 5 A E
			return *(BYTE*)(N64MEM + addr - 1);
		case 1:
			// Covers 1 5 9 D
			return *(BYTE*)(N64MEM + addr + 1);
		default:
			// Covers 0 4 8 C
			return *(BYTE*)(N64MEM + addr + 3);
	}
}

LRESULT CheatSearchResults_FindItem(NMHDR* lParam) {
	NMLVFINDITEM* findInfo;
	DWORD currentPos, startPos, searchstr;

	findInfo = (NMLVFINDITEM*)lParam;

	// searchstr criteria is not supported, only works with strings for now
	if (((findInfo->lvfi.flags) & LVFI_STRING) == 0)
		return -1;

	startPos = findInfo->iStart;

	// Either the last item was selected or nothing was, start at the top
	if (startPos >= results.num_stored || startPos < 0)
		startPos = 0;

	currentPos = startPos;
	searchstr = strtoul(findInfo->lvfi.psz, NULL, 16);

	// The array that we search against contains DWORDs so this will make it so we can search partials
	// Shift the number enough time to the right and this works
	switch (strlen(findInfo->lvfi.psz)) {
		case 1:
			searchstr = searchstr << 20;
			break;
		case 2:
			searchstr = searchstr << 16;
			break;
		case 3:
			searchstr = searchstr << 12;
			break;
		case 4:
			searchstr = searchstr << 8;
			break;
		case 5:
			searchstr = searchstr << 4;
			break;
			/*case 6:
				searchstr = searchstr << 8;
				break;
			case 7:
				searchstr = searchstr << 4;
				break;*/
		default:
			break;
	}

	do {
		if (results.hits[currentPos].address == searchstr)
			return currentPos;

		// Start at the top if we've reached the bottom of the list
		if (currentPos == results.num_stored - 1)
			currentPos = 0;
		else
			currentPos++;
	} while (currentPos != startPos);

	// Not found
	return -1;
}

LRESULT CheatSearchResults_DrawItem(HANDLE hDlg, NMHDR* lParam) {
	LPNMLVCUSTOMDRAW lplvcd = (LPNMLVCUSTOMDRAW)lParam;

	switch (lplvcd->nmcd.dwDrawStage) {
		case CDDS_PREPAINT: {
			// Before the paint cycle begins request notifications for individual listview items
			SetWindowLong(hDlg, DWL_MSGRESULT, CDRF_NOTIFYITEMDRAW);
			return TRUE;
		}

		case CDDS_ITEMPREPAINT: {
			// Before the paint cycle begins request notifications for individual listview subitems
			SetWindowLong(hDlg, DWL_MSGRESULT, CDRF_NOTIFYSUBITEMDRAW);
			return TRUE;
		}

		case CDDS_ITEMPREPAINT | CDDS_SUBITEM: {
			// Before a subitem is drawn
			if (search.searchBy == searchbyvalue) {
				switch (lplvcd->iSubItem) {
					case 1:
					case 2: {
						char val[10];
						DWORD dwVal, loc;

						loc = (DWORD)lplvcd->nmcd.dwItemSpec;

						// White background
						lplvcd->clrTextBk = RGB(255, 255, 255);

						// Not found (This should not happen)
						// Black text
						if (loc < 0 || loc > results.num_stored)
							lplvcd->clrText = RGB(0, 0, 0);

						else {
							// Get the first value (hex)
							ListView_GetItemText(GetDlgItem(hDlg, IDL_SEARCH_RESULT_LIST), loc, 1, val, sizeof(val));

							val[sizeof(val) - 1] = 0;	// To get rid of the "Might not be null terminated" warning
							dwVal = strtoul(val, NULL, 16);

							// Black text (Same value)
							if (results.hits[loc].value == dwVal)
								lplvcd->clrText = RGB(0, 0, 0);

							// Red text (Lower value)
							else if (results.hits[loc].value > dwVal)
								lplvcd->clrText = RGB(180, 0, 0);

							// Green text (Higher value)
							else
								lplvcd->clrText = RGB(0, 180, 0);
						}
						break;
					}
					default:
						// Black text against a white background
						lplvcd->clrText = RGB(0, 0, 0);
						lplvcd->clrTextBk = RGB(255, 255, 255);
						break;
				}

				SetWindowLong(hDlg, DWL_MSGRESULT, CDRF_NEWFONT);
				return TRUE;
			}
		}

		default: {
			SetWindowLong(hDlg, DWL_MSGRESULT, CDRF_DODEFAULT);
			return TRUE;
		}
	}
}

LRESULT CheatSearchResults_ReadItem(NMHDR* lParam) {
	LV_DISPINFO* lpdi = (LV_DISPINFO*)lParam;
	char cpy[20];
	CS_HITS* curr;

	// Only setting text
	if (!(lpdi->item.mask & LVIF_TEXT))
		return TRUE;

	// To do, check to see if that second check is actually necessary
	if (results.num_stored == 0 || (DWORD)lpdi->item.iItem > results.num_stored)
		return TRUE;

	curr = &results.hits[lpdi->item.iItem];

	// Searching by Value
	if (search.searchBy == searchbyvalue) {
		switch (lpdi->item.iSubItem) {
			case 0:
				// Address
				sprintf(cpy, "%06X", curr->address);
				lstrcpy(lpdi->item.pszText, cpy);
				break;
			case 1:
				// Current Value (Hex)
				if (!doingLiveUpdate) {
					if (search.searchNumBits == bits16)
						sprintf(cpy, "%04X", curr->value);
					else
						sprintf(cpy, "%02X", curr->value);
				}
				else {
					if (search.searchNumBits == bits16)
						sprintf(cpy, "%04X", Value_ReadWord(curr->address));
					else
						sprintf(cpy, "%02X", Value_ReadByte(curr->address));
				}
				lstrcpy(lpdi->item.pszText, cpy);
				break;
			case 2:
				// Current Value (Dec)
				if (!doingLiveUpdate) {
					sprintf(cpy, "%u", curr->value);
				}
				else {
					if (search.searchNumBits == bits16)
						sprintf(cpy, "%u", Value_ReadWord(curr->address));
					else
						sprintf(cpy, "%u", Value_ReadByte(curr->address));
				}
				lstrcpy(lpdi->item.pszText, cpy);
				break;
			case 3:
				// Old Value (Hex)
				if (secondSearch) {
					if (search.searchNumBits == bits16)
						sprintf(cpy, "%04X", curr->prev_value);
					else
						sprintf(cpy, "%02X", curr->prev_value);
					lstrcpy(lpdi->item.pszText, cpy);
				}
				break;
		}
	}

	// Searching by Text
	else {
		switch (lpdi->item.iSubItem) {
			case 0:
				// Address
				sprintf(cpy, "%06X", curr->address);
				lstrcpy(lpdi->item.pszText, cpy);
				break;
			case 1:
				// Text
				lstrcpy(lpdi->item.pszText, search.search_string);
				break;
			case 2:
				// Old Text
				lstrcpy(lpdi->item.pszText, "");
				break;
		}
	}
	return TRUE;
}