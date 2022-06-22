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
#include "main.h"
#include "CPU.h"
#include "debugger.h"
#include "resource.h"

#define IDC_VADDR			0x100
#define IDC_PADDR			0x101
#define IDC_LIST_VIEW		0x102
#define IDC_SCRL_BAR		0x103
#define IDC_REFRESH			0x104

// TODO: Figure out if these can be queried instead of hardcoded
#define PADDING 6
#define CHAR_WIDTH 8

void Setup_Memory_Window (HWND hDlg);
void Start_Auto_Refresh_Thread(void);
void Scroll_Memory_View(int lines);
void Refresh_Memory_With_Diff(BOOL ShowDiff);
void Clear_Selection(void);
int Get_Ascii_Index(POINTS pt);

LRESULT CALLBACK Memory_Window_Proc (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

struct MEMORY_VIEW_ROW {
	unsigned char OldData[16];
	COLORREF TextColors[16];
	HFONT Fonts[16];
	DWORD Location;
	char LocationStr[11];
	char HexStr[16][3];
	char AsciiStr[17];
};

struct SELECTION {
	BOOL enabled;
	BOOL dragging;
	BOOL column_hex;
	DWORD anchor;
	DWORD range[2];
	DWORD range_cmp[2];
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
static HFONT hWatchFont;
static HANDLE hRefreshThread = NULL;
static HANDLE hRefreshMutex = NULL;
static int InMemoryWindow = FALSE;
static int wheel = 0;
static int thumb = -1;
static struct MEMORY_VIEW_ROW MemoryViewRows[16];
static struct SELECTION selection = { 0 };

void __cdecl Create_Memory_Window ( int Child ) {
	DWORD ThreadID;
	if ( Child ) {
		hBkEven = CreateSolidBrush(BG_EVEN);
		hBkOdd = CreateSolidBrush(BG_ODD);

		LOGFONT lf;
		GetObject(GetStockObject(ANSI_FIXED_FONT), sizeof(lf), &lf);
		lf.lfUnderline = TRUE;
		hWatchFont = CreateFontIndirect(&lf);

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

void Update_Data_Column_With_WatchPoint(struct MEMORY_VIEW_ROW* row, DWORD location, MIPS_WORD word, int index, int i, BOOL ShowDiff) {
	sprintf(row->HexStr[index], "%02X", word.UB[3 - i]);

	switch (HasWatchPoint(location + i)) {
	case WP_READ:
		row->Fonts[index] = hWatchFont;
		row->TextColors[index] = TC_READ;
		break;
	case WP_WRITE:
		row->Fonts[index] = hWatchFont;
		row->TextColors[index] = TC_WRITE;
		break;
	case WP_READ_WRITE:
		row->Fonts[index] = hWatchFont;
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

void Insert_MemoryLineDump (unsigned int location, int InsertPos, BOOL ShowDiff) {
	struct MEMORY_VIEW_ROW* row = &MemoryViewRows[InsertPos];
	MIPS_WORD word;

	location <<= 4;

	row->Location = location;
	sprintf(row->LocationStr, "0x%08X", location);

	__try {
		if (SendMessage(hVAddr, BM_GETSTATE, 0, 0) & BST_CHECKED) {
			for (int count = 0; count < 4; count++) {
				if (r4300i_LW_VAddr_NonCPU(location, &word.UW)) {
					for (int i = 0; i < 4; i++) {
						Update_Data_Column_With_WatchPoint(row, location, word, count * 4 + i, i, ShowDiff);
					}
					sprintf(&row->AsciiStr[count * 4], "%c%c%c%c",
						word.UB[3] < ' ' || word.UB[3] > '~' ? '.' : word.UB[3],
						word.UB[2] < ' ' || word.UB[2] > '~' ? '.' : word.UB[2],
						word.UB[1] < ' ' || word.UB[1] > '~' ? '.' : word.UB[1],
						word.UB[0] < ' ' || word.UB[0] > '~' ? '.' : word.UB[0]);
				}
				else {
					for (int i = 0; i < 4; i++) {
						int index = count * 4 + i;

						strcpy(row->HexStr[index], "**");
						row->OldData[index] = 0xff;
						row->Fonts[index] = GetStockObject(ANSI_FIXED_FONT);
						row->TextColors[index] = RGB(0, 0, 0);
					}
					strcpy(&row->AsciiStr[count * 4], "****");
				}
				location += 4;
			}
		} else {
			for (int count = 0; count < 4; count++) {
				if (location < 0x1FFFFFFC) {
					r4300i_LW_PAddr(location, &word.UW);
					for (int i = 0; i < 4; i++) {
						Update_Data_Column(row, word, count * 4 + i, i, ShowDiff);
					}
					sprintf(&row->AsciiStr[count * 4], "%c%c%c%c",
						word.UB[3] < ' ' || word.UB[3] > '~' ? '.' : word.UB[3],
						word.UB[2] < ' ' || word.UB[2] > '~' ? '.' : word.UB[2],
						word.UB[1] < ' ' || word.UB[1] > '~' ? '.' : word.UB[1],
						word.UB[0] < ' ' || word.UB[0] > '~' ? '.' : word.UB[0]);
				}
				else {
					for (int i = 0; i < 4; i++) {
						int index = count * 4 + i;

						strcpy(row->HexStr[index], "**");
						row->OldData[index] = 0xff;
						row->Fonts[index] = GetStockObject(ANSI_FIXED_FONT);
						row->TextColors[index] = RGB(0, 0, 0);
					}
					strcpy(&row->AsciiStr[count * 4], "****");
				}
				location += 4;
			}
		}
	} __except( r4300i_Command_MemoryFilter( GetExceptionCode(), GetExceptionInformation()) ) {
		DisplayError(GS(MSG_UNKNOWN_MEM_ACTION));
		PostQuitMessage(0);
	}
}

LRESULT CALLBACK Memory_Window_Proc (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)  {	
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
		BeginPaint( hDlg, &ps );
		SelectObject(ps.hdc, GetStockObject(ANSI_FIXED_FONT));
		SetBkMode( ps.hdc, TRANSPARENT );
		TextOut(ps.hdc,25,17,"Address:",8);
		rcBox.left   = 5;
		rcBox.top    = 5;
		rcBox.right  = 721;
		rcBox.bottom = 348;
		DrawEdge( ps.hdc, &rcBox, EDGE_RAISED, BF_RECT );
		rcBox.left   = 8;
		rcBox.top    = 8;
		rcBox.right  = 718;
		rcBox.bottom = 345;
		DrawEdge( ps.hdc, &rcBox, EDGE_ETCHED, BF_RECT );
		EndPaint( hDlg, &ps );
		return TRUE;
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDC_ADDR_EDIT:
			if (HIWORD(wParam) == EN_CHANGE ) {
				Refresh_Memory_With_Diff(FALSE);
			}
			break;
		case IDC_VADDR:
		case IDC_PADDR:
			Refresh_Memory_With_Diff(FALSE);
			break;
		case IDC_REFRESH:
			Start_Auto_Refresh_Thread();
			Refresh_Memory_With_Diff(FALSE);
			break;
		case IDCANCEL:
			Clear_Selection();
			break;
		default:
			break;
		}
		return FALSE;
	case WM_SYSCOMMAND:
		if (wParam == SC_CLOSE) {
			EndDialog(hDlg, TRUE);
			return TRUE;
		}
		break;
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
					SetTextColor(lplvcd->nmcd.hdc, GetSysColor(COLOR_WINDOWTEXT));

					DWORD location_start = row->Location;
					DWORD location_end = row->Location + 15;
					if (selection.enabled && location_start <= selection.range[1] && location_end >= selection.range[0]) {
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
						if (location_start >= selection.range[0] && location_end <= selection.range[1]) {
							// Entire line selected
							SetTextColor(lplvcd->nmcd.hdc, GetSysColor(COLOR_HIGHLIGHTTEXT));
							FillRect(lplvcd->nmcd.hdc, &rc, GetSysColorBrush(COLOR_HIGHLIGHT));
							DrawText(lplvcd->nmcd.hdc, row->AsciiStr, strlen(row->AsciiStr), &lplvcd->nmcd.rc, DT_SINGLELINE | DT_CENTER | DT_VCENTER);
						} else if (location_start >= selection.range[0]) {
							// Only the beginning selected
							int unselected_count = location_end - selection.range[1];
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
						} else if (location_end <= selection.range[1]) {
							// Only the end selected
							int selected_count = location_end - selection.range[0] + 1;
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
							int left_count = selection.range[0] - location_start;
							int right_count = location_end - selection.range[1];
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
					DWORD location = row->Location + index;

					SelectObject(lplvcd->nmcd.hdc, row->Fonts[index]);
					if (selection.enabled && location >= selection.range[0] && location <= selection.range[1]) {
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

					if (index % 4 == 3 && index < 15 && (!selection.enabled || location < selection.range[0] || location > selection.range[1])) {
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

		DWORD location = row->Location + index;
		if (wParam & MK_SHIFT) {
			// Shift-Click
			selection.range[0] = min(location, selection.anchor);
			selection.range[1] = max(location, selection.anchor);
			memcpy(selection.range_cmp, selection.range, sizeof(selection.range_cmp));
		}
		else {
			selection.anchor = location;
			selection.range[0] = location;
			selection.range[1] = location;
			selection.range_cmp[0] = location;
			selection.range_cmp[1] = location;
		}
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

			DWORD location = row->Location + index;

			selection.range[0] = min(location, selection.anchor);
			selection.range[1] = max(location, selection.anchor);

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

void Clear_Selection(void) {
	selection.enabled = FALSE;
	selection.dragging = FALSE;
	selection.column_hex = FALSE;
	selection.anchor = 0;
	selection.range[0] = 0;
	selection.range[1] = 0;
	selection.range_cmp[0] = 0;
	selection.range_cmp[1] = 0;
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
	unsigned int location = AsciiToHex(value);

	if (lines > 0) {
		if (UINT_MAX - location >= 256) {
			location += lines * 16;
		} else {
			location = UINT_MAX - 256 + 1;
		}
	} else {
		if (location >= -lines * 16) {
			location += lines * 16;
		} else {
			location = 0;
		}
	}

	sprintf(value, "%08X", location);
	SetWindowText(hAddrEdit, value);

	ReleaseMutex(hRefreshMutex);
}

void __cdecl Refresh_Memory(void) {
	Refresh_Memory_With_Diff(TRUE);
}

void Refresh_Memory_With_Diff(BOOL ShowDiff) {
	DWORD location;
	char Value[20];
	int count;

	if (InMemoryWindow == FALSE) { return; }

	if (hRefreshMutex == NULL) { return; }
	DWORD wait_result = WaitForSingleObject(hRefreshMutex, 0);
	if (wait_result != WAIT_OBJECT_0) {
		return;
	}

	GetWindowText(hAddrEdit, Value, sizeof(Value));
	location = (AsciiToHex(Value) >> 4);
	if (location > 0x0FFFFFF0) { location = 0x0FFFFFF0; }

	for (count = 0; count < 16; count ++) {
		Insert_MemoryLineDump(location + count, count, ShowDiff);
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
#define WindowWidth  742
#define WindowHeight 392
	DWORD X, Y;

	if (hRefreshMutex == NULL) {
		hRefreshMutex = CreateMutex(NULL, FALSE, NULL);
		if (hRefreshMutex == NULL) { return; }
	}

	hVAddr = CreateWindowEx(0,"BUTTON", "Virtual Addressing", WS_CHILD | WS_VISIBLE | 
		BS_AUTORADIOBUTTON, 215,13,150,21,hDlg,(HMENU)IDC_VADDR,hInst,NULL );
	SendMessage(hVAddr,BM_SETCHECK, BST_CHECKED,0);
	
	hPAddr = CreateWindowEx(0,"BUTTON", "Physical Addressing", WS_CHILD | WS_VISIBLE | 
		BS_AUTORADIOBUTTON, 375,13,155,21,hDlg,(HMENU)IDC_PADDR,hInst,NULL );

	hRefresh = CreateWindowEx(0,"BUTTON", "Auto Refresh", WS_CHILD | WS_VISIBLE |
		BS_AUTOCHECKBOX, 595,13,100,21,hDlg,(HMENU)IDC_REFRESH,hInst,NULL );
	SendMessage(hRefresh, BM_SETCHECK, BST_CHECKED, 0);
	Start_Auto_Refresh_Thread();

	hList = CreateWindowEx(WS_EX_CLIENTEDGE, WC_LISTVIEW, "", WS_CHILD | WS_VISIBLE |
		LVS_OWNERDATA | LVS_REPORT | LVS_NOSORTHEADER | LVS_SINGLESEL, 14,39,682,300,hDlg,
		(HMENU)IDC_LIST_VIEW,hInst,NULL );
	if (hList) {
		ListView_SetExtendedListViewStyle(hList, LVS_EX_DOUBLEBUFFER);

		LV_COLUMN  col;
		int count;

		col.mask = LVCF_FMT | LVCF_WIDTH | LVCF_TEXT | LVCF_SUBITEM;
		col.fmt  = LVCFMT_LEFT;

		col.pszText  = "Address";
		col.cx       = 90;
		col.iSubItem = 0;
		ListView_InsertColumn ( hList, 0, &col);

		char ColumnName[3] = { 0 };
		col.pszText  = ColumnName;
		col.cx       = 28;
		for (int i = 0; i < 16; i++) {
			sprintf(ColumnName, " %X", i);
			col.iSubItem = i + 1;
			ListView_InsertColumn(hList, i + 1, &col);
		}

		col.pszText  = "Memory Ascii";
		col.cx       = 140;
		col.iSubItem = 17;
		ListView_InsertColumn ( hList, 17, &col);
		ListView_SetItemCount ( hList, 16);
		SendMessage(hList,WM_SETFONT, (WPARAM)GetStockObject(ANSI_FIXED_FONT),0);
		for (count = 0 ; count < 16;count ++ ){
			Insert_MemoryLineDump(count, count, FALSE);
		}
		SetWindowSubclass(hList, Memory_ListViewScroll_Proc, 0, 0);
		SetWindowSubclass(hList, Memory_ListViewKeys_Proc, 0, 0);
		SetWindowSubclass(hList, Memory_ListViewDrag_Proc, 0, 0);
		Clear_Selection();
	}

	hAddrEdit = GetDlgItem(hDlg, IDC_ADDR_EDIT);

	DWORD wait_result = WaitForSingleObject(hRefreshMutex, 0);
	if (wait_result == WAIT_OBJECT_0) {
		SetWindowText(hAddrEdit, "80000000");
	}
	ReleaseMutex(hRefreshMutex);

	SendMessage(hAddrEdit,EM_SETLIMITTEXT,(WPARAM)8,(LPARAM)0);
	SetWindowPos(hAddrEdit,NULL, 100,13,100,21, SWP_NOZORDER | SWP_SHOWWINDOW);
	SendMessage(hAddrEdit,WM_SETFONT, (WPARAM)GetStockObject(ANSI_FIXED_FONT),0);

	hScrlBar = CreateWindowEx(0, "SCROLLBAR", "", WS_CHILD | WS_VISIBLE |
		WS_TABSTOP | SBS_VERT, 696,39,20,300, hDlg, (HMENU)IDC_SCRL_BAR, hInst, NULL );
	if (hScrlBar) {
		SCROLLINFO si;

		si.cbSize = sizeof(si);
		si.fMask  = SIF_RANGE | SIF_POS | SIF_PAGE;
		si.nMin   = 0;
		si.nMax   = 300;
		si.nPos   = 145;
		si.nPage  = 10;
		SetScrollInfo(hScrlBar,SB_CTL,&si,TRUE);
		SetWindowSubclass(hScrlBar, Memory_ListViewScroll_Proc, 0, 0);
	} 

	if ( !GetStoredWinPos( "Memory", &X, &Y ) ) {
		X = (GetSystemMetrics( SM_CXSCREEN ) - WindowWidth) / 2;
		Y = (GetSystemMetrics( SM_CYSCREEN ) - WindowHeight) / 2;
	}
	
	SetWindowPos(hDlg,NULL,X,Y,WindowWidth,WindowHeight, SWP_NOZORDER | SWP_SHOWWINDOW);
	
}