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

// Required for Windows XP build profile
#ifndef _WIN32_WINNT_WIN8
#define _WIN32_WINNT_WIN8 0x0602
#endif

#include <windows.h>
#include <windowsx.h>

// VersionHelpers only exists in SDK 8.0 and newer!
#if (_WIN32_WINNT >= _WIN32_WINNT_WIN8)
#include <versionhelpers.h>
#endif

#include <commctrl.h>
#include <stdio.h>
#include "main.h"
#include "CPU.h"
#include "debugger.h"
#include "resource.h"
#include "rom.h"
#include "SessionMemBookmarks.h"

#define IDC_VADDR			0x100
#define IDC_PADDR			0x101
#define IDC_LIST_VIEW		0x102
#define IDC_SCRL_BAR		0x103
#define IDC_REFRESH			0x104
#define IDC_BOOKMARKS		0x105
#define IDC_BOOKMARK_ADD	0x106
#define IDC_BOOKMARK_UPDATE	0x107
#define IDC_BOOKMARK_REMOVE	0x108

// TODO: Figure out if these can be queried instead of hardcoded
#define PADDING 6
#define CHAR_WIDTH 8

void Setup_Memory_Window (HWND hDlg);
void Start_Auto_Refresh_Thread(void);
void Scroll_Memory_View(int lines);
void Refresh_Memory_With_Diff(BOOL ShowDiff);
void Clear_Selection(void);
void Copy_Selection(void);
int Get_Ascii_Index(POINTS pt);
LRESULT Change_Bookmark_Selection(int next);
unsigned int Get_Bookmark_Name_Width(char *name);
void Update_Bookmark_Width(void);
void Add_Bookmark(void);
void Edit_Bookmark(void);
void Update_Bookmark(unsigned int item);
void Remove_Bookmark(unsigned int item);
void Load_Bookmark(unsigned int item);

LRESULT CALLBACK Memory_Window_Proc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

struct MEMORY_VIEW_ROW {
	unsigned char OldData[16];
	COLORREF TextColors[16];
	HFONT Fonts[16];
	MIPS_DWORD Location;
	char LocationStr[19];
	char HexStr[16][3];
	char AsciiStr[17];
};

struct SELECTION {
	BOOL enabled;
	BOOL dragging;
	BOOL column_hex;
	MIPS_DWORD anchor;
	MIPS_DWORD range[2];
	MIPS_DWORD range_cmp[2];
};

const COLORREF TC_INC			= RGB(0, 180, 0); // Value increased
const COLORREF TC_DEC			= RGB(180, 0, 0); // Value decreased
const COLORREF TC_READ			= RGB(0, 0, 255); // Address has read watchpoint
const COLORREF TC_WRITE			= RGB(255, 0, 255); // Address has write watchpoint
const COLORREF TC_READ_WRITE	= RGB(128, 0, 255); // Address has read/write watchpoint

const COLORREF BG_EVEN			= RGB(240, 240, 240);
const COLORREF BG_ODD			= RGB(230, 230, 230);

static HWND Memory_Win_hDlg, hAddrEdit, hVAddr, hPAddr, hRefresh, hList, hScrlBar;
static HBRUSH hBkEven, hBkOdd;
static HFONT hWatchFont, hDefaultFont;
static HANDLE hRefreshThread = NULL;
static HANDLE hRefreshMutex = NULL;
static int InMemoryWindow = FALSE;
static int wheel = 0;
static int thumb = -1;
static struct MEMORY_VIEW_ROW MemoryViewRows[16];
static struct SELECTION selection = { 0 };
static struct MEM_BOOKMARK bookmarks[MAX_MEM_BOOKMARKS] = { 0 };
static unsigned int num_bookmarks = 0;
static char bookmarks_cbor[MAX_PATH + 1] = { 0 };

void __cdecl Create_Memory_Window ( int Child ) {
	DWORD ThreadID;
	if ( Child ) {
		hBkEven = CreateSolidBrush(BG_EVEN);
		hBkOdd = CreateSolidBrush(BG_ODD);

		// Create a font for the watchpoints within the hex editor
		LOGFONT lf;
		GetObject(GetStockObject(ANSI_FIXED_FONT), sizeof(lf), &lf);
		lf.lfUnderline = TRUE;
		hWatchFont = CreateFontIndirect(&lf);

		// Create a default font that matches the system theme
		NONCLIENTMETRICS metrics = { 0 };

#if (_WIN32_WINNT >= _WIN32_WINNT_WIN8)
		metrics.cbSize = sizeof(NONCLIENTMETRICS);
		if (!IsWindowsVistaOrGreater()) {
			// NOTE: This is for compatibility with Windows XP.
			metrics.cbSize -= sizeof(metrics.iPaddedBorderWidth);
		}
#else
		// NOTE: This is for compatibility with Windows XP.
		metrics.cbSize -= sizeof(metrics.iPaddedBorderWidth);
#endif

		SystemParametersInfo(SPI_GETNONCLIENTMETRICS, sizeof(NONCLIENTMETRICS), &metrics, 0);
		hDefaultFont = CreateFontIndirect(&metrics.lfMessageFont);

		// Load session bookmarks
		char save_directory[MAX_PATH + 1] = { 0 };
		Settings_GetDirectory(AutoSaveDir, save_directory, sizeof(save_directory));
		if (strlen(save_directory) + strlen(RomFullName) + 27 <= sizeof(bookmarks_cbor)) {
			sprintf(bookmarks_cbor, "%s%s.session.membookmarks.cbor", save_directory, RomFullName);
			if (!Session_Load_MemBookmarks(&num_bookmarks, bookmarks, MAX_MEM_BOOKMARKS, bookmarks_cbor)) {
				num_bookmarks = 0;
			}
		} else {
			num_bookmarks = 0;
			bookmarks_cbor[0] = 0;
		}

		InMemoryWindow = TRUE;
		DialogBox( hInst, "MEMORY", NULL,(DLGPROC) Memory_Window_Proc );
		InMemoryWindow = FALSE;

		DeleteObject(hWatchFont);
		DeleteObject(hBkOdd);
		DeleteObject(hBkEven);
	} else {
		if (!InMemoryWindow) {
			CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)Create_Memory_Window,
				(LPVOID)TRUE,0, &ThreadID);	
		} else {
			SetForegroundWindow(Memory_Win_hDlg);
		}
	}
}

void __cdecl Enter_Memory_Window ( void ) {
	if (!HaveDebugger) { return; }
    Create_Memory_Window ( FALSE );
}

void Update_Data_Column(struct MEMORY_VIEW_ROW* row, MIPS_WORD word, int index, int i, BOOL ShowDiff) {
	sprintf(row->HexStr[index], "%02X", word.UB[3 - i]);
	row->Fonts[index] = GetStockObject(ANSI_FIXED_FONT);

	if (ShowDiff && word.UB[3 - i] > row->OldData[index]) {
		row->TextColors[index] = TC_INC;
	} else if (ShowDiff && word.UB[3 - i] < row->OldData[index]) {
		row->TextColors[index] = TC_DEC;
	} else {
		row->TextColors[index] = GetSysColor(COLOR_WINDOWTEXT);
	}

	row->OldData[index] = word.UB[3 - i];
}

void Update_Data_Column_With_WatchPoint(struct MEMORY_VIEW_ROW* row, MIPS_DWORD location, MIPS_WORD word, int index, int i, BOOL ShowDiff) {
	sprintf(row->HexStr[index], "%02X", word.UB[3 - i]);

	int has_watch = (int)HasWatchPoint(location);
	if (has_watch & WP_ENABLED) {
		row->Fonts[index] = hWatchFont;
	} else {
		row->Fonts[index] = GetStockObject(ANSI_FIXED_FONT);
	}

	switch (has_watch & ~WP_ENABLED) {
	case WP_READ:
		row->TextColors[index] = TC_READ;
		break;
	case WP_WRITE:
		row->TextColors[index] = TC_WRITE;
		break;
	case WP_READ_WRITE:
		row->TextColors[index] = TC_READ_WRITE;
		break;
	default:
		row->Fonts[index] = GetStockObject(ANSI_FIXED_FONT);

		if (ShowDiff && word.UB[3 - i] > row->OldData[index]) {
			row->TextColors[index] = TC_INC;
		} else if (ShowDiff && word.UB[3 - i] < row->OldData[index]) {
			row->TextColors[index] = TC_DEC;
		} else {
			row->TextColors[index] = GetSysColor(COLOR_WINDOWTEXT);
		}
		break;
	}

	row->OldData[index] = word.UB[3 - i];
}

void Insert_MemoryLineDump (MIPS_DWORD location, int InsertPos, BOOL ShowDiff) {
	struct MEMORY_VIEW_ROW* row = &MemoryViewRows[InsertPos];
	MIPS_WORD word;

	location.UDW <<= 4;

	row->Location = location;

	if (SendMessage(hVAddr, BM_GETSTATE, 0, 0) & BST_CHECKED) {
		sprintf(row->LocationStr, "0x%016llX", location.UDW);
		for (int count = 0; count < 4; count++) {
			if (IsValidAddress(location) && r4300i_LW_VAddr_NonCPU(location, &word.UW)) {
				for (int i = 0; i < 4; i++) {
					Update_Data_Column_With_WatchPoint(row, location, word, count * 4 + i, i, ShowDiff);
				}
				sprintf(&row->AsciiStr[count * 4], "%c%c%c%c",
					word.UB[3] < ' ' || word.UB[3] > '~' ? '.' : word.UB[3],
					word.UB[2] < ' ' || word.UB[2] > '~' ? '.' : word.UB[2],
					word.UB[1] < ' ' || word.UB[1] > '~' ? '.' : word.UB[1],
					word.UB[0] < ' ' || word.UB[0] > '~' ? '.' : word.UB[0]);
			} else {
				for (int i = 0; i < 4; i++) {
					int index = count * 4 + i;

					strcpy(row->HexStr[index], "**");
					row->OldData[index] = 0xff;
					row->Fonts[index] = GetStockObject(ANSI_FIXED_FONT);
					row->TextColors[index] = RGB(0, 0, 0);
				}
				strcpy(&row->AsciiStr[count * 4], "****");
			}
			location.UDW += 4;
		}
	} else {
		sprintf(row->LocationStr, "0x%08X", location.UW[0]);
		for (int count = 0; count < 4; count++) {
			if (location.UW[0] <= 0x1FFFFFFC) {
				r4300i_LW_NonMemory(location.UW[0], &word.UW);
				for (int i = 0; i < 4; i++) {
					Update_Data_Column(row, word, count * 4 + i, i, ShowDiff);
				}
				sprintf(&row->AsciiStr[count * 4], "%c%c%c%c",
					word.UB[3] < ' ' || word.UB[3] > '~' ? '.' : word.UB[3],
					word.UB[2] < ' ' || word.UB[2] > '~' ? '.' : word.UB[2],
					word.UB[1] < ' ' || word.UB[1] > '~' ? '.' : word.UB[1],
					word.UB[0] < ' ' || word.UB[0] > '~' ? '.' : word.UB[0]);
			} else {
				for (int i = 0; i < 4; i++) {
					int index = count * 4 + i;

					strcpy(row->HexStr[index], "**");
					row->OldData[index] = 0xff;
					row->Fonts[index] = GetStockObject(ANSI_FIXED_FONT);
					row->TextColors[index] = RGB(0, 0, 0);
				}
				strcpy(&row->AsciiStr[count * 4], "****");
			}
			location.UDW += 4;
		}
	}
}

void Write_MemoryLineDump(char *output, MIPS_DWORD location) {
	MIPS_WORD word;

	char bytes[49] = { 0 };
	char ascii[17] = { 0 };
	char *b = bytes;
	char *a = ascii;
	if (SendMessage(hVAddr, BM_GETSTATE, 0, 0) & BST_CHECKED) {
		sprintf(output, "0x%016llX  ", location.UDW);
		output += 12;
		for (int count = 0; count < 4; count++) {
			if (IsValidAddress(location) && r4300i_LW_VAddr_NonCPU(location, &word.UW)) {
				for (int i = 0; i < 4; i++) {
					if (selection.enabled && (selection.range[0].UDW > location.UDW + i || selection.range[1].UDW < location.UDW + i)) {
						strcpy(b, "   ");
						strcpy(a, " ");
					} else {
						sprintf(b, "%02X ", word.UB[3 - i]);
						sprintf(a, "%c", (word.UB[3 - i] < ' ' || word.UB[3 - i] > '~') ? '.' : word.UB[3 - i]);
					}
					b += 3;
					a++;
				}
			} else {
				for (int i = 0; i < 4; i++) {
					if (selection.enabled && (selection.range[0].UDW > location.UDW + i || selection.range[1].UDW < location.UDW + i)) {
						strcpy(b, "   ");
						strcpy(a, " ");
					} else {
						strcpy(b, "** ");
						strcpy(a, "*");
					}
					b += 3;
					a++;
				}
			}
			location.UDW += 4;
		}
	} else {
		sprintf(output, "0x%08X  ", location.UW[0]);
		output += 12;
		for (int count = 0; count < 4; count++) {
			if (location.UDW <= 0x1FFFFFFC) {
				r4300i_LW_NonMemory(location.UW[0], &word.UW);
				for (int i = 0; i < 4; i++) {
					if (selection.enabled && (selection.range[0].UDW > location.UDW + i || selection.range[1].UDW < location.UDW + i)) {
						strcpy(b, "   ");
						strcpy(a, " ");
					} else {
						sprintf(b, "%02X ", word.UB[3 - i]);
						sprintf(a, "%c", (word.UB[3 - i] < ' ' || word.UB[3 - i] > '~') ? '.' : word.UB[3 - i]);
					}
					b += 3;
					a++;
				}
			} else {
				for (int i = 0; i < 4; i++) {
					if (selection.enabled && (selection.range[0].UDW > location.UDW + i || selection.range[1].UDW < location.UDW + i)) {
						strcpy(b, "   ");
						strcpy(a, " ");
					} else {
						strcpy(b, "** ");
						strcpy(a, "*");
					}
					b += 3;
					a++;
				}
			}
			location.UDW += 4;
		}
	}

	sprintf(output, "%s %s\r\n", bytes, ascii);
}

LRESULT CALLBACK Memory_Window_Proc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	PAINTSTRUCT ps;
	RECT rcBox;

	switch (uMsg) {
	case WM_INITDIALOG:
		Memory_Win_hDlg = hDlg;
		Setup_Memory_Window ( hDlg );
		break;
	case WM_MOVE:
		if (IsIconic(hDlg)) { break; }
		StoreCurrentWinPos("Memory",hDlg);
		break;
	case WM_PAINT:
		BeginPaint(hDlg, &ps);
		SetBkMode(ps.hdc, TRANSPARENT);

		SelectObject(ps.hdc, hDefaultFont);
		TextOut(ps.hdc, 811, 15, "Bookmarks:", 10);

		SelectObject(ps.hdc, GetStockObject(ANSI_FIXED_FONT));
		TextOut(ps.hdc, 25, 17, "Address:", 8);

		rcBox.left   = 5;
		rcBox.top    = 5;
		rcBox.right  = 799;
		rcBox.bottom = 348;
		DrawEdge(ps.hdc, &rcBox, EDGE_ETCHED, BF_RECT);

		rcBox.left   = 804;
		rcBox.top    = 5;
		rcBox.right  = 971;
		rcBox.bottom = 348;
		DrawEdge(ps.hdc, &rcBox, EDGE_ETCHED, BF_RECT);

		EndPaint(hDlg, &ps);
		return TRUE;
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_ADDR_EDIT:
			if (HIWORD(wParam) == EN_CHANGE ) {
				Refresh_Memory_With_Diff(FALSE);
			}
			break;
		case IDC_VADDR: {
			char Value[20];
			GetWindowText(hAddrEdit, Value, sizeof(Value));
			QWORD address = 0;
			if (strlen(Value) <= 8) {
				address = (int)AsciiToHex(Value);
			}
			else {
				address = AsciiToHex64(Value);
			}

			if (address < 0xFFFFFFFF80000000 || address >= 0xFFFFFFFF80800000) {
				SetWindowText(hAddrEdit, "FFFFFFFF80000000");
			}

			Clear_Selection();
			ListBox_SetCurSel(GetDlgItem(hDlg, IDC_BOOKMARKS), -1);
			Refresh_Memory_With_Diff(FALSE);
			break;
		}

		case IDC_PADDR: {
			char Value[20];
			GetWindowText(hAddrEdit, Value, sizeof(Value));
			DWORD address = AsciiToHex(Value);
			if (address >= 0x20000000) {
				SetWindowText(hAddrEdit, "00000000");
			}

			Clear_Selection();
			ListBox_SetCurSel(GetDlgItem(hDlg, IDC_BOOKMARKS), -1);
			Refresh_Memory_With_Diff(FALSE);
			break;
		}
		case IDC_REFRESH:
			Start_Auto_Refresh_Thread();
			Refresh_Memory_With_Diff(FALSE);
			break;
		case IDCANCEL:
			Clear_Selection();
			break;
		case IDC_BOOKMARKS:
			if (HIWORD(wParam) == LBN_DBLCLK) {
				Edit_Bookmark();
			}
			break;
		case IDC_BOOKMARK_ADD:
			Add_Bookmark();
			break;
		case IDC_BOOKMARK_UPDATE: {
			int item = ListBox_GetCurSel(GetDlgItem(hDlg, IDC_BOOKMARKS));
			if (item != LB_ERR) {
				Update_Bookmark(item);
			}
			break;
		}
		case IDC_BOOKMARK_REMOVE: {
			int item = ListBox_GetCurSel(GetDlgItem(hDlg, IDC_BOOKMARKS));
			if (item != LB_ERR) {
				Remove_Bookmark(item);
			}
			break;
		}
		default:
			break;
		}
		return FALSE;
	case WM_SYSCOMMAND:
		if (wParam == SC_CLOSE) {
			EndDialog(hDlg, TRUE);
			return TRUE;
		}
		return FALSE;
	case WM_NOTIFY:
		switch (((LPNMHDR)lParam)->code) {
		case HDN_BEGINTRACK:
			// Disable column resize
			SetWindowLong(hDlg, DWL_MSGRESULT, TRUE);
			return TRUE;
		case LVN_ITEMCHANGED:
			// Disable selections
			ListView_SetItemState(hList, -1, 0, LVIS_FOCUSED | LVIS_SELECTED);
			break;
		case LVN_GETDISPINFO: {
			LVITEM item = ((NMLVDISPINFO*)lParam)->item;
			struct MEMORY_VIEW_ROW* row = &MemoryViewRows[item.iItem];

			if (item.mask & LVIF_TEXT) {
				switch (item.iSubItem) {
				case 0:
					strcpy_s(item.pszText, sizeof(row->LocationStr), row->LocationStr);
					break;
				case 17:
					strcpy_s(item.pszText, sizeof(row->AsciiStr), row->AsciiStr);
					break;
				default: {
					int index = item.iSubItem - 1;
					strcpy_s(item.pszText, sizeof(row->HexStr[index]), row->HexStr[index]);
					break;
				}
				}
			}

			break;
		}
		case NM_CUSTOMDRAW: {
			LPNMLVCUSTOMDRAW lplvcd = (LPNMLVCUSTOMDRAW)lParam;

			switch (lplvcd->nmcd.dwDrawStage) {
			case CDDS_PREPAINT:
				SetWindowLong(hDlg, DWL_MSGRESULT, CDRF_NOTIFYITEMDRAW);
				return TRUE;
			case CDDS_ITEMPREPAINT:
				SetWindowLong(hDlg, DWL_MSGRESULT, CDRF_NOTIFYSUBITEMDRAW);
				return TRUE;
			case CDDS_ITEMPREPAINT | CDDS_SUBITEM: {
				struct MEMORY_VIEW_ROW* row = &MemoryViewRows[lplvcd->nmcd.dwItemSpec];

				switch (lplvcd->iSubItem) {
				case 0:
					// Address column
					SetWindowLong(hDlg, DWL_MSGRESULT, CDRF_DODEFAULT);
					break;
				case 17: {
					// ASCII column
					SelectObject(lplvcd->nmcd.hdc, GetStockObject(ANSI_FIXED_FONT));
					SetTextColor(lplvcd->nmcd.hdc, GetSysColor(COLOR_WINDOWTEXT));

					MIPS_DWORD location_start = row->Location;
					MIPS_DWORD location_end = row->Location;
					location_end.UDW += 15;
					if (selection.enabled && location_start.UDW <= selection.range[1].UDW && location_end.UDW >= selection.range[0].UDW) {
						RECT rc = lplvcd->nmcd.rc;
						rc.left += PADDING;
						rc.right -= PADDING;

						// Text has selection
						//
						// There are four possible configurations:
						// 1. The entire line is selected (one DrawText call)
						// 2. Only the beginning of the line is selected (two DrawText calls)
						// 3. Only the end of the line is selected (two DrawText calls)
						// 4. Only the middle of the line is selected (three DrawText calls)
						if (location_start.UDW >= selection.range[0].UDW && location_end.UDW <= selection.range[1].UDW) {
							// Entire line selected
							SetTextColor(lplvcd->nmcd.hdc, GetSysColor(COLOR_HIGHLIGHTTEXT));
							FillRect(lplvcd->nmcd.hdc, &rc, GetSysColorBrush(COLOR_HIGHLIGHT));
							DrawText(lplvcd->nmcd.hdc, row->AsciiStr, strlen(row->AsciiStr), &lplvcd->nmcd.rc, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
						} else if (location_start.UDW >= selection.range[0].UDW) {
							// Only the beginning selected
							int unselected_count = (int)(location_end.UDW - selection.range[1].UDW);
							int selected_count = 16 - unselected_count;

							// Draw right side (unselected)
							RECT right_rc = rc;
							right_rc.left += selected_count * CHAR_WIDTH;
							DrawText(lplvcd->nmcd.hdc, &row->AsciiStr[selected_count], unselected_count, &right_rc, DT_SINGLELINE | DT_CENTER | DT_VCENTER);

							// Draw left side (selected)
							RECT left_rc = rc;
							left_rc.right = right_rc.left;
							SetTextColor(lplvcd->nmcd.hdc, GetSysColor(COLOR_HIGHLIGHTTEXT));
							FillRect(lplvcd->nmcd.hdc, &left_rc, GetSysColorBrush(COLOR_HIGHLIGHT));
							DrawText(lplvcd->nmcd.hdc, row->AsciiStr, selected_count, &left_rc, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
						} else if (location_end.UDW <= selection.range[1].UDW) {
							// Only the end selected
							int selected_count = (int)(location_end.UDW - selection.range[0].UDW + 1);
							int unselected_count = 16 - selected_count;

							// Draw left side (unselected)
							RECT left_rc = rc;
							left_rc.right = left_rc.left + unselected_count * CHAR_WIDTH;
							DrawText(lplvcd->nmcd.hdc, row->AsciiStr, unselected_count, &left_rc, DT_SINGLELINE | DT_CENTER | DT_VCENTER);

							// Draw right side (selected)
							RECT right_rc = rc;
							right_rc.left = left_rc.right;
							SetTextColor(lplvcd->nmcd.hdc, GetSysColor(COLOR_HIGHLIGHTTEXT));
							FillRect(lplvcd->nmcd.hdc, &right_rc, GetSysColorBrush(COLOR_HIGHLIGHT));
							DrawText(lplvcd->nmcd.hdc, &row->AsciiStr[unselected_count], selected_count, &right_rc, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
						} else {
							// Only the middle selected
							int left_count = (int)(selection.range[0].UDW - location_start.UDW);
							int right_count = (int)(location_end.UDW - selection.range[1].UDW);
							int selected_count = 16 - (left_count + right_count);

							// Draw left side (unselected)
							RECT left_rc = rc;
							left_rc.right = left_rc.left + left_count * CHAR_WIDTH;
							DrawText(lplvcd->nmcd.hdc, row->AsciiStr, left_count, &left_rc, DT_SINGLELINE | DT_CENTER | DT_VCENTER);

							// Draw right side (unselected)
							RECT right_rc = rc;
							right_rc.left += (left_count + selected_count) * CHAR_WIDTH;
							DrawText(lplvcd->nmcd.hdc, &row->AsciiStr[left_count + selected_count], right_count, &right_rc, DT_SINGLELINE | DT_CENTER | DT_VCENTER);

							// Draw middle (selected)
							RECT mid_rc = rc;
							mid_rc.left = left_rc.right;
							mid_rc.right = right_rc.left;
							SetTextColor(lplvcd->nmcd.hdc, GetSysColor(COLOR_HIGHLIGHTTEXT));
							FillRect(lplvcd->nmcd.hdc, &mid_rc, GetSysColorBrush(COLOR_HIGHLIGHT));
							DrawText(lplvcd->nmcd.hdc, &row->AsciiStr[left_count], selected_count, &mid_rc, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
						}
					} else {
						// Unselected text
						DrawText(lplvcd->nmcd.hdc, row->AsciiStr, strlen(row->AsciiStr), &lplvcd->nmcd.rc, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
					}

					SetWindowLong(hDlg, DWL_MSGRESULT, CDRF_SKIPDEFAULT);
					break;
				}
				default: {
					// Hex columns
					int index = lplvcd->iSubItem - 1;
					MIPS_DWORD location;
					location.UDW = row->Location.UDW + index;

					SelectObject(lplvcd->nmcd.hdc, row->Fonts[index]);
					if (selection.enabled && location.UDW >= selection.range[0].UDW && location.UDW <= selection.range[1].UDW) {
						SetTextColor(lplvcd->nmcd.hdc, GetSysColor(COLOR_HIGHLIGHTTEXT));
						FillRect(lplvcd->nmcd.hdc, &lplvcd->nmcd.rc, GetSysColorBrush(COLOR_HIGHLIGHT));
					} else {
						SetTextColor(lplvcd->nmcd.hdc, row->TextColors[index]);

						if (index & 1) {
							FillRect(lplvcd->nmcd.hdc, &lplvcd->nmcd.rc, hBkOdd);
						} else {
							FillRect(lplvcd->nmcd.hdc, &lplvcd->nmcd.rc, hBkEven);
						}
					}

					char* str = row->HexStr[index];
					DrawText(lplvcd->nmcd.hdc, str, strlen(str), &lplvcd->nmcd.rc, DT_SINGLELINE | DT_CENTER | DT_VCENTER);

					if (index % 4 == 3 && index < 15 && (!selection.enabled || location.UDW < selection.range[0].UDW || location.UDW > selection.range[1].UDW)) {
						// Draw vertical separators on word boundaries only when the byte is not selected
						rcBox.left = lplvcd->nmcd.rc.right - 1;
						rcBox.top = lplvcd->nmcd.rc.top;
						rcBox.right = lplvcd->nmcd.rc.right;
						rcBox.bottom = lplvcd->nmcd.rc.bottom;
						FillRect(lplvcd->nmcd.hdc, &rcBox, (HBRUSH)GetStockObject(GRAY_BRUSH));
					}
					SetWindowLong(hDlg, DWL_MSGRESULT, CDRF_SKIPDEFAULT);
					break;
				}
				}
				return TRUE;
			}
			default:
				SetWindowLong(hDlg, DWL_MSGRESULT, CDRF_DODEFAULT);
				return TRUE;
			}
			break;
		}
		default:
			break;
		}
		return FALSE;
	case WM_VSCROLL:
		if ((HWND)lParam == hScrlBar) {
			switch (LOWORD(wParam)) {
			case SB_LINEUP:
				Scroll_Memory_View(-1);
				break;
			case SB_LINEDOWN:
				Scroll_Memory_View(1);
				break;
			case SB_PAGEUP:
				Scroll_Memory_View(-16);
				break;
			case SB_PAGEDOWN:
				Scroll_Memory_View(16);
				break;
			case SB_THUMBTRACK: {
				int position = HIWORD(wParam);
				if (thumb >= 0) {
					Scroll_Memory_View(position - thumb);
				}
				thumb = position;
				break;
			}
			case SB_ENDSCROLL:
				thumb = -1;
				break;
			}
		}
		break;
	default:
		return FALSE;
	}
	return TRUE;
}

LRESULT CALLBACK Memory_ListViewScroll_Proc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
	switch (uMsg) {
	case WM_MOUSEWHEEL:
		// Accumulate wheel deltas
		wheel -= GET_WHEEL_DELTA_WPARAM(wParam);
		if (abs(wheel) >= WHEEL_DELTA) {
			int lines = wheel / WHEEL_DELTA;
			wheel -= lines * WHEEL_DELTA;

			Scroll_Memory_View(lines);
		}

		return FALSE;
	default:
		return DefSubclassProc(hWnd, uMsg, wParam, lParam);
	}
}

LRESULT CALLBACK Memory_ListViewKeys_Proc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
	switch (uMsg) {
	case WM_KEYDOWN:
		switch (wParam) {
		case VK_UP:
			Scroll_Memory_View(-1);
			return FALSE;
		case VK_DOWN:
			Scroll_Memory_View(1);
			return FALSE;
		case VK_PRIOR:
			Scroll_Memory_View(-16);
			return FALSE;
		case VK_NEXT:
			Scroll_Memory_View(16);
			return FALSE;
		case 'C':
			if (GetKeyState(VK_CONTROL) & 0x8000) {
				Copy_Selection();
			}
			return FALSE;
		default:
			break;
		}
		break;
	default:
		break;
	}

	return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK Memory_ListViewDrag_Proc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
	switch (uMsg) {
	case WM_LBUTTONDOWN: {
		SetFocus(hWnd);

		POINT pt = { LOWORD(lParam), HIWORD(lParam) };
		LVHITTESTINFO hit_test;
		hit_test.pt = pt;
		ListView_SubItemHitTest(hWnd, &hit_test);

		if (hit_test.iSubItem == 0) {
			// Clicking in address column
			break;
		}

		struct MEMORY_VIEW_ROW* row = &MemoryViewRows[hit_test.iItem];

		selection.enabled = TRUE;
		selection.dragging = TRUE;

		int index;
		if (hit_test.iSubItem == 17) {
			// Clicking in ASCII column
			selection.column_hex = FALSE;
			index = Get_Ascii_Index(MAKEPOINTS(lParam));
		} else {
			// Clicking in hex columns
			selection.column_hex = TRUE;
			index = hit_test.iSubItem - 1;
		}

		MIPS_DWORD location;
		location.UDW = row->Location.UDW + index;
		if (wParam & MK_SHIFT) {
			// Shift-Click
			selection.range[0].UDW = min(location.UDW, selection.anchor.UDW);
			selection.range[1].UDW = max(location.UDW, selection.anchor.UDW);
			memcpy(selection.range_cmp, selection.range, sizeof(selection.range_cmp));
		}
		else {
			selection.anchor = location;
			selection.range[0] = location;
			selection.range[1] = location;
			selection.range_cmp[0] = location;
			selection.range_cmp[1] = location;
		}

		EnableWindow(GetDlgItem(Memory_Win_hDlg, IDC_BOOKMARK_ADD), TRUE);

		return FALSE;
	}
	case WM_LBUTTONUP:
		selection.dragging = FALSE;
		break;
	case WM_MOUSEMOVE:
		if (selection.dragging) {
			POINT pt = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
			LVHITTESTINFO hit_test;
			hit_test.pt = pt;
			ListView_SubItemHitTest(hWnd, &hit_test);
			struct MEMORY_VIEW_ROW* row = &MemoryViewRows[hit_test.iItem];

			int index;
			if (hit_test.iSubItem == 17 && selection.column_hex == FALSE) {
				// Dragging in ASCII column
				index = Get_Ascii_Index(MAKEPOINTS(lParam));
			} else if (hit_test.iSubItem > 0 && selection.column_hex == TRUE) {
				// Dragging in hex columns
				index = hit_test.iSubItem - 1;
			} else {
				// Dragging in address column or drag started in a different column
				break;
			}

			MIPS_DWORD location;
			location.UDW= row->Location.UDW + index;

			selection.range[0].UDW = min(location.UDW, selection.anchor.UDW);
			selection.range[1].UDW = max(location.UDW, selection.anchor.UDW);

			if (memcmp(selection.range, selection.range_cmp, sizeof(selection.range)) != 0) {
				// TODO: Invalidate only the changed rect
				InvalidateRect(hWnd, NULL, FALSE);

				memcpy(selection.range_cmp, selection.range, sizeof(selection.range_cmp));
				return FALSE;
			}
		}
		break;
	case WM_ACTIVATE:
		if (selection.dragging && wParam == WA_INACTIVE) {
			Clear_Selection();
			return FALSE;
		}
		break;
	case WM_KILLFOCUS:
	case WM_RBUTTONDOWN:
		if (selection.dragging) {
			Clear_Selection();
			return FALSE;
		}
	default:
		break;
	}

	return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK Bookmarks_ListBox_Proc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
	(void)uIdSubclass;
	(void)dwRefData;

	static BOOL mouse_held = FALSE;

	switch (uMsg) {
	case WM_LBUTTONDOWN: {
		mouse_held = TRUE;

		int height = ListBox_GetItemHeight(hWnd, 0);
		POINT pt = { LOWORD(lParam), HIWORD(lParam) };
		int item = (pt.y / height) + ListBox_GetTopIndex(hWnd);

		if (item >= 0 && (unsigned int)item < num_bookmarks) {
			BOOL is_virtual = (Button_GetCheck(hVAddr) == BST_CHECKED);
			if (bookmarks[item].is_virtual != is_virtual) {
				// Deselect the item if the virtual addressing setting doesn't match
				// TODO: Color these items so they look disabled (needs OWNER DRAW?)
				return FALSE;
			}

			Load_Bookmark(item);
		}

		break;
	}
	case WM_LBUTTONUP:
		mouse_held = FALSE;
		break;
	case WM_MOUSEMOVE:
		if (mouse_held) {
			// Disable dragging
			// TODO: Use drag to reorder bookmarks
			return FALSE;
		}
		break;
	case WM_KEYDOWN:
		switch (wParam) {
		case VK_UP:
			if (Change_Bookmark_Selection(-1)) {
				return TRUE;
			}
			break;
		case VK_DOWN:
			if (Change_Bookmark_Selection(1)) {
				return TRUE;
			}
			break;
		case VK_F2: {
			int item = ListBox_GetCurSel(hWnd);
			if (item != LB_ERR) {
				Edit_Bookmark();
				return TRUE;
			}
			break;
		}
		case VK_DELETE: {
			int item = ListBox_GetCurSel(hWnd);
			if (item != LB_ERR) {
				Remove_Bookmark(item);
				return TRUE;
			}
			break;
		}
		default:
			break;
		}
		break;
	}

	return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

INT_PTR CALLBACK Edit_Bookmark_Proc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	(void)lParam;

	HWND hBookmarks = GetDlgItem(Memory_Win_hDlg, IDC_BOOKMARKS);
	int item = ListBox_GetCurSel(hBookmarks);

	switch (uMsg) {
	case WM_INITDIALOG: {
		char address[17] = { 0 };

		// TODO: Support language translations
		HWND hName = GetDlgItem(hDlg, IDC_NAME);
		Edit_SetText(hName, bookmarks[item].name);
		Edit_LimitText(hName, sizeof(bookmarks->name) - 1);

		HWND hStart = GetDlgItem(hDlg, IDC_START);
		sprintf(address, "%016llX", bookmarks[item].selection_range[0].UDW);
		Edit_SetText(hStart, address);
		Edit_LimitText(hStart, 16);

		HWND hEnd = GetDlgItem(hDlg, IDC_END);
		sprintf(address, "%016llX", bookmarks[item].selection_range[1].UDW);
		Edit_SetText(hEnd, address);
		Edit_LimitText(hEnd, 16);

		CheckRadioButton(hDlg, IDC_BOOKMARK_VADDR, IDC_BOOKMARK_PADDR, bookmarks[item].is_virtual ? IDC_BOOKMARK_VADDR : IDC_BOOKMARK_PADDR);

		return TRUE;
	}
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDOK: {
			char name[sizeof(bookmarks->name)] = { 0 };
			char temp[17] = { 0 };
			char *end = NULL;
			MIPS_DWORD start_address;
			MIPS_DWORD end_address;
			start_address.UDW = end_address.UDW = 0;

			// Validate inputs
			Edit_GetText(GetDlgItem(hDlg, IDC_NAME), name, sizeof(name));
			if (name[0] == 0) {
				DisplayError("Name is required");
				return FALSE;
			}

			Edit_GetText(GetDlgItem(hDlg, IDC_START), temp, sizeof(temp));
			if (strlen(temp) <= 8) {
				start_address.UDW = (int)strtoul(temp, &end, 16);
			} else {
				start_address.UDW = _strtoui64(temp, &end, 16);
			}
			if (end == temp || *end != 0) {
				DisplayError("Start address is invalid");
				return FALSE;
			}

			Edit_GetText(GetDlgItem(hDlg, IDC_END), temp, sizeof(temp));
			if (strlen(temp) <= 8) {
				end_address.UDW = (int)strtoul(temp, &end, 16);
			} else {
				end_address.UDW = _strtoui64(temp, &end, 16);
			}
			if (end == temp || *end != 0) {
				DisplayError("End address is invalid");
				return FALSE;
			}
			if (end_address.UDW < start_address.UDW) {
				DisplayError("Invalid address range: End must be greater or equal to start");
				return FALSE;
			}

			// Update bookmark "atomically"
			strcpy(bookmarks[item].name, name);
			bookmarks[item].width_required = Get_Bookmark_Name_Width(name);
			bookmarks[item].selection_range[0] = start_address;
			bookmarks[item].selection_range[1] = end_address;
			bookmarks[item].is_virtual = (Button_GetCheck(GetDlgItem(hDlg, IDC_BOOKMARK_VADDR)) == BST_CHECKED);

			// Update parent control
			BOOL is_virtual = (Button_GetCheck(hVAddr) == BST_CHECKED);
			if (bookmarks[item].is_virtual == is_virtual) {
				ListBox_DeleteString(hBookmarks, item);
				ListBox_InsertString(hBookmarks, item, name);
				ListBox_SetCurSel(hBookmarks, item);
				Load_Bookmark(item);
			} else {
				ListBox_SetCurSel(hBookmarks, -1);
			}

			EndDialog(hDlg, 1);
			return TRUE;
		}
		case IDCANCEL:
			EndDialog(hDlg, 0);
			return TRUE;
		default:
			return FALSE;
		}
	default:
		return FALSE;
	}
}

void Clear_Selection(void) {
	selection.enabled = FALSE;
	selection.dragging = FALSE;
	selection.column_hex = FALSE;
	selection.anchor.UDW = 0;
	selection.range[0].UDW = 0;
	selection.range[1].UDW = 0;
	selection.range_cmp[0].UDW = 0;
	selection.range_cmp[1].UDW = 0;

	EnableWindow(GetDlgItem(Memory_Win_hDlg, IDC_BOOKMARK_ADD), FALSE);
}

void Copy_Selection(void) {
	if (!selection.enabled || !OpenClipboard(NULL)) {
		return;
	}
	EmptyClipboard();

	MIPS_DWORD location;
	location.UDW = selection.range[0].UDW & ~15;
	if ((selection.range[1].UDW - location.UDW) >= INT_MAX) {
		return;
	}
	int lines = (int)(selection.range[1].UDW - location.UDW) / 16 + 1;

	// Each line is exactly 79 bytes, plus the null terminator.
	HGLOBAL hMemory = GlobalAlloc(GMEM_MOVEABLE, lines * 79 + 1);
	if (!hMemory) {
		CloseClipboard();
		return;
	}

	char *memory = GlobalLock(hMemory);

	char *output = memory;
	for (int i = 0; i < lines; i++) {
		Write_MemoryLineDump(output, location);
		output += 79;
		location.UDW += 16;
	}

	GlobalUnlock(hMemory);
	// TODO: Also copy the binary data
	SetClipboardData(CF_TEXT, memory);
	CloseClipboard();
}

int Get_Ascii_Index(POINTS pt) {
	RECT rc;
	// The row doesn't matter because we ignore the Y coordinate
	ListView_GetSubItemRect(hList, 0, 17, LVIR_BOUNDS, &rc);

	// Compute the character index
	int x = pt.x - rc.left;
	int index = (x - PADDING) / CHAR_WIDTH;

	return min(max(index, 0), 15);
}

void Scroll_Memory_View(int lines) {
	char value[20];

	if (hRefreshMutex == NULL) { return; }
	DWORD wait_result = WaitForSingleObject(hRefreshMutex, 0);
	if (wait_result != WAIT_OBJECT_0) {
		return;
	}

	GetWindowText(hAddrEdit, value, sizeof(value));
	MIPS_DWORD location;
	if (strlen(value) <= 8) {
		location.DW = (int)AsciiToHex(value);
	}
	else {
		location.UDW = AsciiToHex64(value);
	}

	if (lines > 0) {
		if (ULLONG_MAX - location.UDW >= 256LL) {
			location.UDW += lines * 16;
		} else {
			location.UDW = ULLONG_MAX - 256 + 1;
		}
	} else {
		if (location.UDW >= (unsigned long long)(-lines * 16)) {
			location.UDW += lines * 16;
		} else {
			location.UDW = 0;
		}
	}

	sprintf(value, "%016llX", location.UDW);
	SetWindowText(hAddrEdit, value);

	ReleaseMutex(hRefreshMutex);
}

LRESULT Change_Bookmark_Selection(int next) {
	BOOL result = FALSE;
	BOOL is_virtual = (Button_GetCheck(hVAddr) == BST_CHECKED);
	HWND hBookmarks = GetDlgItem(Memory_Win_hDlg, IDC_BOOKMARKS);
	int item = ListBox_GetCurSel(hBookmarks) + next;
	while (item >= 0 && (unsigned int)item < num_bookmarks) {
		result = TRUE;
		if (bookmarks[item].is_virtual == is_virtual) {
			ListBox_SetCurSel(hBookmarks, item);
			Load_Bookmark(item);
			return TRUE;
		}

		item += next;
	}
	return result;
}

void __cdecl Refresh_Memory(void) {
	Refresh_Memory_With_Diff(TRUE);
}

void Refresh_Memory_With_Diff(BOOL ShowDiff) {
	MIPS_DWORD location;
	char Value[20];
	int count;

	if (InMemoryWindow == FALSE) { return; }

	if (hRefreshMutex == NULL) { return; }
	DWORD wait_result = WaitForSingleObject(hRefreshMutex, 0);
	if (wait_result != WAIT_OBJECT_0) {
		return;
	}

	GetWindowText(hAddrEdit, Value, sizeof(Value));
	if (strlen(Value) <= 8) {
		location.DW = (int)AsciiToHex(Value);
		location.UDW >>= 4;
	}
	else {
		location.UDW = AsciiToHex64(Value) >> 4;
	}
	if (location.UDW > 0x0FFFFFFFFFFFFFF0LL) { location.UDW = 0x0FFFFFFFFFFFFFF0LL; }

	for (count = 0; count < 16; count ++) {
		Insert_MemoryLineDump(location, count, ShowDiff);
		location.UDW++;
	}
	InvalidateRect(hList, NULL, FALSE);

	ReleaseMutex(hRefreshMutex);
}

void __cdecl Auto_Refresh(void) {
	for (;;) {
		Sleep(150);

		if (InMemoryWindow == FALSE || SendMessage(hRefresh, BM_GETCHECK, 0, 0) != BST_CHECKED) {
			hRefreshThread = NULL;
			return;
		}

		Refresh_Memory();
	}
}

void Start_Auto_Refresh_Thread(void) {
	if (hRefreshThread == NULL && SendMessage(hRefresh, BM_GETCHECK, 0, 0) == BST_CHECKED) {
		hRefreshThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)Auto_Refresh, NULL, 0, NULL);
	}
}

void Setup_Memory_Window (HWND hDlg) {
#define WindowWidth  992
#define WindowHeight 392
	HWND hBookmarks, hBookmarkAdd, hBookmarkEdit, hBookmarkRemove;
	DWORD X, Y;

	if (hRefreshMutex == NULL) {
		hRefreshMutex = CreateMutex(NULL, FALSE, NULL);
		if (hRefreshMutex == NULL) { return; }
	}

	hVAddr = CreateWindowEx(0,"BUTTON", "Virtual Addressing", WS_CHILD | WS_VISIBLE | 
		BS_AUTORADIOBUTTON, 255,13,150,21,hDlg,(HMENU)IDC_VADDR,hInst,NULL );
	SendMessage(hVAddr,BM_SETCHECK, BST_CHECKED,0);
	SendMessage(hVAddr, WM_SETFONT, (WPARAM)hDefaultFont, 0);

	hPAddr = CreateWindowEx(0, "BUTTON", "Physical Addressing", WS_CHILD | WS_VISIBLE |
		BS_AUTORADIOBUTTON, 455, 13, 155, 21, hDlg, (HMENU)IDC_PADDR, hInst, NULL);
	SendMessage(hPAddr, WM_SETFONT, (WPARAM)hDefaultFont, 0);

	hRefresh = CreateWindowEx(0, "BUTTON", "Auto Refresh", WS_CHILD | WS_VISIBLE |
		BS_AUTOCHECKBOX, 675, 13, 100, 21, hDlg, (HMENU)IDC_REFRESH, hInst, NULL);
	SendMessage(hRefresh, BM_SETCHECK, BST_CHECKED, 0);
	Start_Auto_Refresh_Thread();
	SendMessage(hRefresh, WM_SETFONT, (WPARAM)hDefaultFont, 0);

	hList = CreateWindowEx(WS_EX_CLIENTEDGE, WC_LISTVIEW, "", WS_CHILD | WS_VISIBLE |
		LVS_OWNERDATA | LVS_REPORT | LVS_NOSORTHEADER | LVS_SINGLESEL, 12, 39, 762, 300, hDlg,
		(HMENU)IDC_LIST_VIEW, hInst, NULL);
	if (hList) {
		SendMessage(hList, WM_SETFONT, (WPARAM)hDefaultFont, 0);
		ListView_SetExtendedListViewStyle(hList, LVS_EX_DOUBLEBUFFER);

		LV_COLUMN  col;
		int count;

		col.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
		col.fmt = LVCFMT_LEFT;

		col.pszText = "Address";
		col.cx = 160;
		col.iSubItem = 0;
		ListView_InsertColumn(hList, 0, &col);

		char ColumnName[3] = { 0 };
		col.pszText = ColumnName;
		col.cx = 28;
		for (int i = 0; i < 16; i++) {
			sprintf(ColumnName, " %X", i);
			col.iSubItem = i + 1;
			ListView_InsertColumn(hList, i + 1, &col);
		}

		col.pszText = "Memory Ascii";
		col.cx = 140;
		col.iSubItem = 17;
		ListView_InsertColumn(hList, 17, &col);
		ListView_SetItemCount(hList, 16);
		SendMessage(hList, WM_SETFONT, (WPARAM)GetStockObject(ANSI_FIXED_FONT), 0);
		for (count = 0; count < 16; count++) {
			MIPS_DWORD location;
			location.UDW = count;
			Insert_MemoryLineDump(location, count, FALSE);
		}
		SetWindowSubclass(hList, Memory_ListViewScroll_Proc, 0, 0);
		SetWindowSubclass(hList, Memory_ListViewKeys_Proc, 0, 0);
		SetWindowSubclass(hList, Memory_ListViewDrag_Proc, 0, 0);
		Clear_Selection();
	}

	hAddrEdit = GetDlgItem(hDlg, IDC_ADDR_EDIT);

	DWORD wait_result = WaitForSingleObject(hRefreshMutex, 0);
	if (wait_result == WAIT_OBJECT_0) {
		SetWindowText(hAddrEdit, "FFFFFFFF80000000");
	}
	ReleaseMutex(hRefreshMutex);

	SendMessage(hAddrEdit, EM_SETLIMITTEXT, (WPARAM)16, (LPARAM)0);
	SetWindowPos(hAddrEdit, NULL, 100, 13, 140, 21, SWP_NOZORDER | SWP_SHOWWINDOW);
	SendMessage(hAddrEdit, WM_SETFONT, (WPARAM)GetStockObject(ANSI_FIXED_FONT), 0);

	hScrlBar = CreateWindowEx(0, "SCROLLBAR", "", WS_CHILD | WS_VISIBLE |
		WS_TABSTOP | SBS_VERT, 774, 39, 20, 300, hDlg, (HMENU)IDC_SCRL_BAR, hInst, NULL);
	if (hScrlBar) {
		SCROLLINFO si;

		si.cbSize = sizeof(si);
		si.fMask = SIF_RANGE | SIF_POS | SIF_PAGE;
		si.nMin = 0;
		si.nMax = 300;
		si.nPos = 145;
		si.nPage = 10;
		SetScrollInfo(hScrlBar, SB_CTL, &si, TRUE);
		SetWindowSubclass(hScrlBar, Memory_ListViewScroll_Proc, 0, 0);
	}

	hBookmarks = CreateWindowEx(WS_EX_CLIENTEDGE, WC_LISTBOX, "", WS_CHILD | WS_VISIBLE | WS_VSCROLL | WS_HSCROLL | LBS_NOTIFY,
		811, 39, 152, 280, hDlg, (HMENU)IDC_BOOKMARKS, hInst, NULL);
	SendMessage(hBookmarks, WM_SETFONT, (WPARAM)hDefaultFont, 0);
	SetWindowSubclass(hBookmarks, Bookmarks_ListBox_Proc, 0, 0);

	for (unsigned int i = 0; i < num_bookmarks; i++) {
		ListBox_AddString(hBookmarks, bookmarks[i].name);
	}
	Update_Bookmark_Width();

	hBookmarkAdd = CreateWindowEx(WS_EX_WINDOWEDGE, WC_BUTTON, "Add", WS_CHILD | WS_VISIBLE,
		811, 318, 36, 22, hDlg, (HMENU)IDC_BOOKMARK_ADD, hInst, NULL);
	SendMessage(hBookmarkAdd, WM_SETFONT, (WPARAM)hDefaultFont, 0);
	EnableWindow(hBookmarkAdd, FALSE);

	hBookmarkEdit = CreateWindowEx(WS_EX_WINDOWEDGE, WC_BUTTON, "Update", WS_CHILD | WS_VISIBLE,
		849, 318, 56, 22, hDlg, (HMENU)IDC_BOOKMARK_UPDATE, hInst, NULL);
	SendMessage(hBookmarkEdit, WM_SETFONT, (WPARAM)hDefaultFont, 0);
	EnableWindow(hBookmarkEdit, FALSE);

	hBookmarkRemove = CreateWindowEx(WS_EX_WINDOWEDGE, WC_BUTTON, "Remove", WS_CHILD | WS_VISIBLE,
		907, 318, 56, 22, hDlg, (HMENU)IDC_BOOKMARK_REMOVE, hInst, NULL);
	SendMessage(hBookmarkRemove, WM_SETFONT, (WPARAM)hDefaultFont, 0);
	EnableWindow(hBookmarkRemove, FALSE);

	if ( !GetStoredWinPos( "Memory", &X, &Y ) ) {
		X = (GetSystemMetrics( SM_CXSCREEN ) - WindowWidth) / 2;
		Y = (GetSystemMetrics( SM_CYSCREEN ) - WindowHeight) / 2;
	}
	
	SetWindowPos(hDlg,NULL,X,Y,WindowWidth,WindowHeight, SWP_NOZORDER | SWP_SHOWWINDOW);
}

unsigned int Get_Bookmark_Name_Width(char *name) {
	HWND hBookmark = GetDlgItem(Memory_Win_hDlg, IDC_BOOKMARKS);
	HDC hDcBookmarks = GetDC(hBookmark);
	HDC hdc = CreateCompatibleDC(hDcBookmarks);
	HGDIOBJ hOldFont = SelectObject(hdc, hDefaultFont);

	SIZE size = { 0 };
	GetTextExtentPoint32(hdc, name, strlen(name), &size);

	SelectObject(hdc, hOldFont);
	DeleteDC(hdc);
	ReleaseDC(hBookmark, hDcBookmarks);

	return size.cx + GetSystemMetrics(SM_CXEDGE) * 2;
}

void Update_Bookmark_Width(void) {
	unsigned int width_required = 0;
	for (unsigned int i = 0; i < num_bookmarks; i++) {
		width_required = max(width_required, bookmarks[i].width_required);
	}

	HWND hBookmarks = GetDlgItem(Memory_Win_hDlg, IDC_BOOKMARKS);
	if (width_required != ListBox_GetHorizontalExtent(hBookmarks)) {
		ListBox_SetHorizontalExtent(hBookmarks, width_required);
	}
}

void Create_Bookmark_Name(char *name, BOOL is_virtual) {
	MIPS_DWORD address = selection.range[0];
	QWORD len = min(selection.range[1].UDW - address.UDW + 1, sizeof(bookmarks->name) - (8 + 6));
	BYTE value = 0;

	if (is_virtual) {
		unsigned int i = 0;
		while (i < len) {
			MIPS_DWORD add;
			add.UDW = address.UDW + i;
			if (IsValidAddress(add) && r4300i_LB_VAddr_NonCPU(add, &value) && isprint(value)) {
				name[i] = value;
			} else {
				sprintf(name, "[0x%016llX]", address.UDW);
				return;
			}
			i++;
		}
		sprintf(&name[i], " [0x%016llX]", address.UDW);
	} else {
		sprintf(name, "PHYS [0x%08X]", address.UW[0]);
	}
}

void Add_Bookmark(void) {
	if (num_bookmarks < MAX_MEM_BOOKMARKS && selection.enabled) {
		bookmarks[num_bookmarks].selection_range[0] = selection.range[0];
		bookmarks[num_bookmarks].selection_range[1] = selection.range[1];
		bookmarks[num_bookmarks].is_virtual = (Button_GetCheck(hVAddr) == BST_CHECKED);

		Create_Bookmark_Name(bookmarks[num_bookmarks].name, bookmarks[num_bookmarks].is_virtual);
		bookmarks[num_bookmarks].width_required = Get_Bookmark_Name_Width(bookmarks[num_bookmarks].name);

		ListBox_AddString(GetDlgItem(Memory_Win_hDlg, IDC_BOOKMARKS), bookmarks[num_bookmarks].name);

		num_bookmarks += 1;

		Update_Bookmark_Width();

		Session_Save_MemBookmarks(bookmarks_cbor, bookmarks, num_bookmarks);
	}
}

void Edit_Bookmark(void) {
	if (DialogBox(NULL, MAKEINTRESOURCE(IDD_MEM_BOOKMARK_EDIT), Memory_Win_hDlg, Edit_Bookmark_Proc)) {
		Update_Bookmark_Width();

		Session_Save_MemBookmarks(bookmarks_cbor, bookmarks, num_bookmarks);
	}
}

void Update_Bookmark(unsigned int item) {
	bookmarks[item].selection_range[0] = selection.range[0];
	bookmarks[item].selection_range[1] = selection.range[1];

	Session_Save_MemBookmarks(bookmarks_cbor, bookmarks, num_bookmarks);
}

void Remove_Bookmark(unsigned int item) {
	if (item < num_bookmarks - 1) {
		memmove(&bookmarks[item], &bookmarks[item + 1], sizeof(struct MEM_BOOKMARK) * (num_bookmarks - item - 1));
	}

	HWND hBookmarks = GetDlgItem(Memory_Win_hDlg, IDC_BOOKMARKS);
	ListBox_DeleteString(hBookmarks, item);

	num_bookmarks -= 1;

	BOOL is_virtual = (Button_GetCheck(hVAddr) == BST_CHECKED);
	if (item < num_bookmarks && bookmarks[item].is_virtual == is_virtual) {
		ListBox_SetCurSel(hBookmarks, item);
	} else {
		EnableWindow(GetDlgItem(Memory_Win_hDlg, IDC_BOOKMARK_UPDATE), FALSE);
		EnableWindow(GetDlgItem(Memory_Win_hDlg, IDC_BOOKMARK_REMOVE), FALSE);
	}

	Update_Bookmark_Width();

	Session_Save_MemBookmarks(bookmarks_cbor, bookmarks, num_bookmarks);
}

void Load_Bookmark(unsigned int item) {
	selection.enabled = TRUE;
	selection.dragging = FALSE;
	selection.column_hex = TRUE;
	selection.anchor = bookmarks[item].selection_range[0];
	selection.range[0] = bookmarks[item].selection_range[0];
	selection.range[1] = bookmarks[item].selection_range[1];
	selection.range_cmp[0] = bookmarks[item].selection_range[0];
	selection.range_cmp[1] = bookmarks[item].selection_range[1];

	char address[18] = { 0 };
	sprintf(address, "%016llX", bookmarks[item].selection_range[0].UDW);
	SetWindowText(hAddrEdit, address);

	EnableWindow(GetDlgItem(Memory_Win_hDlg, IDC_BOOKMARK_ADD), TRUE);
	EnableWindow(GetDlgItem(Memory_Win_hDlg, IDC_BOOKMARK_UPDATE), TRUE);
	EnableWindow(GetDlgItem(Memory_Win_hDlg, IDC_BOOKMARK_REMOVE), TRUE);
}
