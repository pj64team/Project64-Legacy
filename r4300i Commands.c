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

#include <Windows.h>
#include <commctrl.h>
#include <stdio.h>
#include "main.h"
#include "CPU.h"
#include "debugger.h"

char CommandName[110];

#define R4300i_MaxCommandLines		37

typedef struct {
	MIPS_DWORD Location;
	char  String[150];
    DWORD status;
} R4300ICOMMANDLINE;

#define R4300i_Status_PC            1
#define R4300i_Status_BP            2
#define R4300i_Status_Selected      4

#define IDC_LIST					1000
#define IDC_ADDRESS					1001
#define IDCfunctION_COMBO			1002
#define IDC_GO_BUTTON				1003
#define IDC_BREAK_BUTTON			1004
#define IDC_STEP_BUTTON				1005
#define IDC_SKIP_BUTTON				1006
#define IDC_BP_BUTTON				1007
#define IDC_R4300I_REGISTERS_BUTTON	1008
#define IDCrsP_DEBUGGER_BUTTON		1009
#define IDCrsP_REGISTERS_BUTTON	1010
#define IDC_MEMORY_BUTTON			1011
#define IDC_SCRL_BAR				1012

void Paint_R4300i_Commands ( HWND hDlg );
void R4300i_Commands_Setup ( HWND hDlg );
void RefreshR4300iCommands ( void );
void Scroll_R4300i_Commands(int lines);

LRESULT CALLBACK R4300i_Commands_Proc ( HWND, UINT, WPARAM, LPARAM );

const COLORREF TEXT_COLOR_PC = RGB(0, 255, 0); // Currently executing instruction
const COLORREF TEXT_COLOR_BP = RGB(255, 0, 0); // Breakpoint enabled
const COLORREF TEXT_COLOR_PC_BP = RGB(255, 128, 0); // Currently executing instruction + breakpoint enabled

static USHORT bCheckerBits[8] = { 0x33, 0x33, 0xcc, 0xcc, 0x33, 0x33, 0xcc, 0xcc }; // 2x2 checkerboard
static HBITMAP hBmChecker;
static HBRUSH hBrushChecker;
static HWND R4300i_Commands_hDlg, hList, hAddress, hFunctionlist, hGoButton, hBreakButton,
	hStepButton, hSkipButton, hBPButton, hR4300iRegisters, hRSPDebugger, hRSPRegisters,
	hMemory, hScrlBar;
static R4300ICOMMANDLINE r4300iCommandLine[R4300i_MaxCommandLines];
static int wheel = 0;
static int thumb = -1;
static UINT DragListMsg;
static BOOL has_selection = FALSE;
static MIPS_DWORD selection_anchor = { 0 };
static MIPS_DWORD selection_range[2] = { 0, 0 };

BOOL InR4300iCommandsWindow = FALSE;

void __cdecl Create_R4300i_Commands_Window ( int Child ) {
	DWORD ThreadID;
	if ( Child ) {
		hBmChecker = CreateBitmap(8, 8, 1, 1, bCheckerBits);
		hBrushChecker = CreatePatternBrush(hBmChecker);
		InR4300iCommandsWindow = TRUE;
		DialogBox( hInst, "BLANK", NULL,(DLGPROC) R4300i_Commands_Proc );
		InR4300iCommandsWindow = FALSE;
		DeleteObject(hBrushChecker);
		DeleteObject(hBmChecker);
		memset(r4300iCommandLine,0,sizeof(r4300iCommandLine));
		SetR4300iCommandToRunning();
	} else {
		if (!InR4300iCommandsWindow) {
			SetCoreToStepping();
			CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)Create_R4300i_Commands_Window,
				(LPVOID)TRUE,0, &ThreadID);	
		} else {
			SetForegroundWindow(R4300i_Commands_hDlg);
		}	
	}
}

void Disable_R4300i_Commands_Window ( void ) {
	SCROLLINFO si;

	EnableWindow(hList,            FALSE);
	EnableWindow(hAddress,         FALSE);
	EnableWindow(hScrlBar,         FALSE);
	EnableWindow(hGoButton,        FALSE);
	EnableWindow(hStepButton,      FALSE);
	EnableWindow(hSkipButton,      FALSE);
	EnableWindow(hR4300iRegisters, FALSE);
	EnableWindow(hRSPRegisters,    FALSE);
	EnableWindow(hRSPDebugger,     FALSE);
	EnableWindow(hMemory,          FALSE);
	
	si.cbSize = sizeof(si);
	si.fMask  = SIF_RANGE | SIF_POS | SIF_PAGE;
	si.nMin   = 0;
	si.nMax   = 0;
	si.nPos   = 1;
	si.nPage  = 1;
	SetScrollInfo(hScrlBar,SB_CTL,&si,TRUE);
}

int DisplayR4300iCommand (MIPS_DWORD location, int InsertPos) {
	DWORD OpCode, count, LinesUsed = 1, status;
	BOOL Redraw = FALSE;

	for (count = 0; count < NoOfMapEntries; count ++ ) {
		if (MapTable[count].VAddr.UDW == location.UDW) {

			if (strcmp(r4300iCommandLine[InsertPos].String, MapTable[count].Label) != 0) {
				Redraw = TRUE;
			}

			if (Redraw) {
				r4300iCommandLine[InsertPos].Location.DW = -1;
				r4300iCommandLine[InsertPos].status = 0;
				sprintf(r4300iCommandLine[InsertPos].String, " %s:", MapTable[count].Label);
				if (SendMessage(hList, LB_GETCOUNT, 0, 0) <= InsertPos) {
					SendMessage(hList, LB_INSERTSTRING, (WPARAM)InsertPos, (LPARAM)r4300iCommandLine[InsertPos].String);
				} else {
					RECT ItemRC;
					SendMessage(hList, LB_GETITEMRECT, (WPARAM)InsertPos, (LPARAM)&ItemRC);
					RedrawWindow(hList, &ItemRC, NULL, RDW_INVALIDATE);
				}
			}
			InsertPos += 1;
			if (InsertPos >= R4300i_MaxCommandLines) {
				return LinesUsed;
			}
			LinesUsed = 2;
			count = NoOfMapEntries;
		}
	}

	Redraw = FALSE;
	if (!r4300i_LW_VAddr_NonCPU(location, &OpCode)) {
		r4300iCommandLine[InsertPos].Location = location;
		r4300iCommandLine[InsertPos].status = 0;
		sprintf(r4300iCommandLine[InsertPos].String," 0x%016llX\tCould not resolve address",location.UDW);
		if ( SendMessage(hList,LB_GETCOUNT,0,0) <= InsertPos) {
			SendMessage(hList,LB_INSERTSTRING,(WPARAM)InsertPos, (LPARAM)r4300iCommandLine[InsertPos].String);
		} else {
			RECT ItemRC;
			SendMessage(hList, LB_GETITEMRECT, (WPARAM)InsertPos, (LPARAM)&ItemRC);
			RedrawWindow(hList, &ItemRC, NULL, RDW_INVALIDATE);
		}
		return LinesUsed;
	}
	if (SelfModCheck == ModCode_ChangeMemory) {
		if ( (OpCode >> 16) == 0x7C7C) {
			OpCode = OrigMem[(OpCode & 0xFFFF)].OriginalValue;
		}
	}
	
	status = 0;
	if (location.UDW == PROGRAM_COUNTER.UDW) { status = R4300i_Status_PC; }
	if (HasR4300iBPoint(location)) { status |= R4300i_Status_BP; }
	if (has_selection && location.UDW >= selection_range[0].UDW && location.UDW <= selection_range[1].UDW) { status |= R4300i_Status_Selected; }

	if (r4300iCommandLine[InsertPos].Location.UDW != location.UDW) { Redraw = TRUE; }
	if (r4300iCommandLine[InsertPos].status != status) { Redraw = TRUE; }

	if (Redraw) {
		r4300iCommandLine[InsertPos].Location = location;
		r4300iCommandLine[InsertPos].status = status;
		sprintf(r4300iCommandLine[InsertPos].String, " 0x%016llX\t%08X\t%s", location.UDW, OpCode, R4300iOpcodeName(OpCode, location));
		if (SendMessage(hList, LB_GETCOUNT, 0, 0) <= InsertPos) {
			SendMessage(hList, LB_INSERTSTRING, (WPARAM)InsertPos, (LPARAM)r4300iCommandLine[InsertPos].String);
		} else {
			RECT ItemRC;
			SendMessage(hList, LB_GETITEMRECT, (WPARAM)InsertPos, (LPARAM)&ItemRC);
			RedrawWindow(hList, &ItemRC, NULL, RDW_INVALIDATE);
		}
	}
	return LinesUsed;
}

int WriteR4300iCommand(char *commands, MIPS_DWORD location) {
	DWORD OpCode;
	int res = 0;

	for (int count = 0; count < NoOfMapEntries; count++) {
		if (MapTable[count].VAddr.UDW == location.UDW) {
			res = sprintf(commands, "%s:\r\n", MapTable[count].Label);
			commands += res;

			count = NoOfMapEntries;
		}
	}

	if (!r4300i_LW_VAddr_NonCPU(location, &OpCode)) {
		res += sprintf(commands, "0x%016llX  Could not resolve address\r\n", location.UDW);
		return res;
	}
	if (SelfModCheck == ModCode_ChangeMemory) {
		if ((OpCode >> 16) == 0x7C7C) {
			OpCode = OrigMem[(OpCode & 0xFFFF)].OriginalValue;
		}
	}

	res += sprintf(commands, "0x%016llX  %08X  ", location.UDW, OpCode);

	char inst[150];
	strcpy(inst, R4300iOpcodeName(OpCode, location));

	// Replace tabs with spaces
	char* p = strtok(inst, "\t");
	int tok_len = 0;
	while (p) {
		if (tok_len) {
			int len = strlen(commands);
			int indent = 8 - (tok_len % 8);
			memset(commands + len, ' ', indent);
			commands[len + indent] = '\0';
			res += indent;
		}
		tok_len = strlen(p);

		strcat(commands, p);
		res += tok_len;

		p = strtok(NULL, "\t");
	}

	strcat(commands, "\r\n");
	res += 2;

	return res;
}

void R4300i_Copy_Commands(void) {
	if (!has_selection || !OpenClipboard(NULL)) {
		return;
	}
	EmptyClipboard();

	int lines = (selection_range[1].UDW - selection_range[0].UDW) / 4 + 1;

	// Assume each line is up to 150 bytes.
	HGLOBAL hCommands = GlobalAlloc(GMEM_MOVEABLE, lines * 150);
	if (!hCommands) {
		CloseClipboard();
		return;
	}

	char* commands = GlobalLock(hCommands);

	char* output = commands;
	MIPS_DWORD location = selection_range[0];
	for (int i = 0; i < lines; i++) {
		output += WriteR4300iCommand(output, location);
		location.UDW += 4;
	}

	GlobalUnlock(hCommands);
	SetClipboardData(CF_TEXT, commands);
	CloseClipboard();
}

void DrawR4300iCommand ( LPARAM lParam ) {	
	char Command[150], *Offset, *OpCode, *Instruction, *Arguments;
	LPDRAWITEMSTRUCT ditem;
	COLORREF oldColor, oldBkColor, textColor;
	int bkColorIndex;
	int oldBkMode;
	RECT TextRect;

	ditem  = (LPDRAWITEMSTRUCT)lParam;
	strcpy(Command, r4300iCommandLine[ditem->itemID].String);
	
	Offset = strtok(Command, "\t");
	OpCode = strtok(NULL, "\t");
	Instruction = strtok(NULL, "\t");
	Arguments = strtok(NULL, "\t");

	MIPS_DWORD Location = r4300iCommandLine[ditem->itemID].Location;
	if (r4300iCommandLine[ditem->itemID].status & R4300i_Status_Selected) {
		textColor = GetSysColor(COLOR_HIGHLIGHTTEXT);
		bkColorIndex = COLOR_HIGHLIGHT;
	} else {
		textColor = GetSysColor(COLOR_WINDOWTEXT);
		bkColorIndex = COLOR_WINDOW;
	}

	int mask = R4300i_Status_PC | R4300i_Status_BP;
	if ((r4300iCommandLine[ditem->itemID].status & mask) == mask) {
		textColor = TEXT_COLOR_PC_BP;
	} else if (r4300iCommandLine[ditem->itemID].status & R4300i_Status_PC) {
		textColor = TEXT_COLOR_PC;
	} else if (r4300iCommandLine[ditem->itemID].status & R4300i_Status_BP) {
		textColor = TEXT_COLOR_BP;
	}

	oldColor = SetTextColor(ditem->hDC, textColor);
	oldBkColor = SetBkColor(ditem->hDC, GetSysColor(bkColorIndex));
	FillRect(ditem->hDC, &ditem->rcItem, GetSysColorBrush(bkColorIndex));

	if (r4300iCommandLine[ditem->itemID].status & R4300i_Status_PC) {
		FrameRect(ditem->hDC, &ditem->rcItem, hBrushChecker);
	}

	oldBkMode = SetBkMode(ditem->hDC, TRANSPARENT);

	if (OpCode == NULL) {
		DrawText(ditem->hDC, Offset, strlen(Offset), &ditem->rcItem, DT_SINGLELINE | DT_VCENTER);
	} else {
		SetRect(&TextRect, ditem->rcItem.left, ditem->rcItem.top, ditem->rcItem.left + 164, ditem->rcItem.bottom);
		DrawText(ditem->hDC, Offset, strlen(Offset), &TextRect, DT_SINGLELINE | DT_VCENTER);

		if (Instruction == NULL) {
			SetRect(&TextRect, ditem->rcItem.left + 164, ditem->rcItem.top, ditem->rcItem.right, ditem->rcItem.bottom);
			DrawText(ditem->hDC, OpCode, strlen(OpCode), &TextRect, DT_SINGLELINE | DT_VCENTER);
		} else {
			SetRect(&TextRect, ditem->rcItem.left + 164, ditem->rcItem.top, ditem->rcItem.left + 252, ditem->rcItem.bottom);
			DrawText(ditem->hDC, OpCode, strlen(OpCode), &TextRect, DT_SINGLELINE | DT_VCENTER);

			if (Arguments == NULL) {
				SetRect(&TextRect, ditem->rcItem.left + 252, ditem->rcItem.top, ditem->rcItem.right, ditem->rcItem.bottom);
				DrawText(ditem->hDC, Instruction, strlen(Instruction), &TextRect, DT_SINGLELINE | DT_VCENTER);
			} else {
				SetRect(&TextRect, ditem->rcItem.left + 252, ditem->rcItem.top, ditem->rcItem.left + 338, ditem->rcItem.bottom);
				DrawText(ditem->hDC, Instruction, strlen(Instruction), &TextRect, DT_SINGLELINE | DT_VCENTER);

				SetRect(&TextRect, ditem->rcItem.left + 338, ditem->rcItem.top, ditem->rcItem.right, ditem->rcItem.bottom);
				DrawText(ditem->hDC, Arguments, strlen(Arguments), &TextRect, DT_SINGLELINE | DT_VCENTER);
			}
		}
	}

	SetBkMode(ditem->hDC, oldBkMode);
	SetBkColor(ditem->hDC, oldBkColor);
	SetTextColor(ditem->hDC, oldColor);
}

void Enable_R4300i_Commands_Window ( void ) {
	SCROLLINFO si;
	char Location[18];

	if (!InR4300iCommandsWindow) { return; }
	EnableWindow(hList,            TRUE);
	EnableWindow(hAddress,         TRUE);
	EnableWindow(hScrlBar,         TRUE);
	EnableWindow(hGoButton,        TRUE);
	EnableWindow(hStepButton,      TRUE);
	EnableWindow(hSkipButton,      TRUE);
	EnableWindow(hR4300iRegisters, TRUE);
	EnableWindow(hRSPRegisters,    FALSE);
	EnableWindow(hRSPDebugger,     FALSE);
	EnableWindow(hMemory,          TRUE);
	Update_r4300iCommandList();
	
	si.cbSize = sizeof(si);
	si.fMask  = SIF_RANGE | SIF_POS | SIF_PAGE;
	si.nMin   = 0;
	si.nMax   = 300;
	si.nPos   = 145;
	si.nPage  = 10;
	SetScrollInfo(hScrlBar,SB_CTL,&si,TRUE);		
	
	sprintf(Location,"%016llX",PROGRAM_COUNTER.UDW);
	SetWindowText(hAddress,Location);

	SetForegroundWindow(R4300i_Commands_hDlg);
}

void __cdecl Enter_R4300i_Commands_Window ( void ) {
	if (!HaveDebugger) { return; }
	Create_R4300i_Commands_Window ( FALSE );
	Update_r4300iCommandList();
}

void Paint_R4300i_Commands (HWND hDlg) {
	PAINTSTRUCT ps;
	RECT rcBox;
	HFONT hOldFont;
	COLORREF OldBkColor;

	BeginPaint( hDlg, &ps );
		
	rcBox.left   = 5;   rcBox.top    = 5;
	rcBox.right  = 621; rcBox.bottom = 563;
	DrawEdge( ps.hdc, &rcBox, EDGE_RAISED, BF_RECT );
		
	rcBox.left   = 8;   rcBox.top    = 8;
	rcBox.right  = 618; rcBox.bottom = 560;
	DrawEdge( ps.hdc, &rcBox, EDGE_ETCHED, BF_RECT );
		
	rcBox.left   = 625; rcBox.top    = 7;
	rcBox.right  = 784; rcBox.bottom = 42;
	DrawEdge( ps.hdc, &rcBox, EDGE_ETCHED, BF_RECT );

	if (NoOfMapEntries) {
		rcBox.left   = 625; rcBox.top    = 49;
		rcBox.right  = 784; rcBox.bottom = 84;
		DrawEdge( ps.hdc, &rcBox, EDGE_ETCHED, BF_RECT );
	}

	rcBox.left   = 14; rcBox.top    = 14;
	rcBox.right  = 172; rcBox.bottom = 32;
	DrawEdge( ps.hdc, &rcBox, EDGE_ETCHED , BF_RECT );

	rcBox.left   = 170; rcBox.top    = 14;
	rcBox.right  = 260; rcBox.bottom = 32;
	DrawEdge( ps.hdc, &rcBox, EDGE_ETCHED , BF_RECT );

	rcBox.left   = 258; rcBox.top    = 14;
	rcBox.right  = 346; rcBox.bottom = 32;
	DrawEdge( ps.hdc, &rcBox, EDGE_ETCHED , BF_RECT );

	rcBox.left   = 344; rcBox.top    = 14;
	rcBox.right  = 598; rcBox.bottom = 32;
	DrawEdge( ps.hdc, &rcBox, EDGE_ETCHED , BF_RECT );

	hOldFont = (HFONT)SelectObject( ps.hdc,GetStockObject(DEFAULT_GUI_FONT ) );
	OldBkColor = SetBkColor(ps.hdc, GetSysColor(COLOR_BTNFACE));
		
	TextOut( ps.hdc, 23,16,"Offset",6);
	TextOut( ps.hdc, 179,16,"Opcode",6);
	TextOut( ps.hdc, 267,16,"Instruction",11);
	TextOut( ps.hdc, 353,16,"Arguments",9);
	TextOut( ps.hdc, 632,2," Address ",9);
	TextOut( ps.hdc, 632,19,"0x",2);
	
	if (NoOfMapEntries) {
		TextOut( ps.hdc, 632,44," goto: ",7);
	}

	SelectObject( ps.hdc,hOldFont );
	SetBkColor( ps.hdc, OldBkColor );
		
	EndPaint( hDlg, &ps );
}

LRESULT CALLBACK R4300i_Commands_Proc (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	if (uMsg == DragListMsg) {
		LPDRAGLISTINFO pDragInfo = (LPDRAGLISTINFO)lParam;
		switch (pDragInfo->uNotification) {
		case DL_BEGINDRAG: {
			if ((GetKeyState(VK_SHIFT) & 0x8000) == 0) {
				int Selected = LBItemFromPt(hList, pDragInfo->ptCursor, FALSE);
				if (Selected > -1) {
					MIPS_DWORD Location = r4300iCommandLine[Selected].Location;
					if (Location.UDW != (QWORD)-1) {
						has_selection = TRUE;
						selection_anchor = Location;
						selection_range[0] = Location;
						selection_range[1] = Location;
						RefreshR4300iCommands();
					}
				}
				SetWindowLong(hDlg, DWL_MSGRESULT, TRUE);
			}
			break;
		}
		case DL_DRAGGING:
		case DL_DROPPED: {
			int Selected = LBItemFromPt(hList, pDragInfo->ptCursor, FALSE);
			if (Selected > -1) {
				MIPS_DWORD Location = r4300iCommandLine[Selected].Location;
				if (Location.UDW != (QWORD)-1) {
					selection_range[0].UDW = min(Location.UDW, selection_anchor.UDW);
					selection_range[1].UDW = max(Location.UDW, selection_anchor.UDW);
					RefreshR4300iCommands();
				}
			} else {
				// TODO: Scroll + select
			}
			break;
		}
		case DL_CANCELDRAG:
			has_selection = FALSE;
			selection_anchor.UDW = 0;
			selection_range[0].UDW = 0;
			selection_range[1].UDW = 0;
			RefreshR4300iCommands();
			break;
		default:
			return FALSE;
		}
	} else {
		switch (uMsg) {
		case WM_INITDIALOG:
			R4300i_Commands_hDlg = hDlg;
			R4300i_Commands_Setup(hDlg);
			break;
		case WM_MOVE:
			StoreCurrentWinPos("R4300i Commands", hDlg);
			break;
		case WM_DRAWITEM:
			if (wParam == IDC_LIST) {
				DrawR4300iCommand(lParam);
			}
			break;
		case WM_PAINT:
			Paint_R4300i_Commands(hDlg);
			RedrawWindow(hScrlBar, NULL, NULL, RDW_INVALIDATE | RDW_ERASE);
			return TRUE;
		case WM_COMMAND:
			switch (LOWORD(wParam)) {
			case IDCfunctION_COMBO:
				if (HIWORD(wParam) == CBN_SELENDOK) {
					DWORD Selected;
					MIPS_DWORD Location;
					char Value[20];

					Selected = SendMessage(hFunctionlist, CB_GETCURSEL, 0, 0);
					if ((int)Selected >= 0) {
						QWORD* addr = SendMessage(hFunctionlist, CB_GETITEMDATA, (WPARAM)Selected, 0);
						Location.UDW = *addr;
						sprintf(Value, "%016llX", Location.UDW);
						SetWindowText(hAddress, Value);
					}
				}
				break;
			case IDC_LIST:
				if (HIWORD(wParam) == LBN_DBLCLK) {
					DWORD Selected;
					MIPS_DWORD Location;
					Selected = SendMessage(hList, LB_GETCURSEL, (WPARAM)0, (LPARAM)0);
					Location = r4300iCommandLine[Selected].Location;
					if (Location.UDW != (QWORD)-1) {
						if (HasR4300iBPoint(Location)) {
							RemoveR4300iBreakPoint(Location);
						}
						else {
							Add_R4300iBPoint(Location);
						}
						RefreshR4300iCommands();
					}
				}
				break;
			case IDC_ADDRESS:
				if (HIWORD(wParam) == EN_CHANGE) {
					RefreshR4300iCommands();
				}
				break;
			case IDC_GO_BUTTON:
				SetR4300iCommandToRunning();
				RefreshR4300iCommands();
				break;
			case IDC_BREAK_BUTTON:
				SetR4300iCommandToStepping();
				break;
			case IDC_STEP_BUTTON:
				StepOpcode();
				break;
			case IDC_SKIP_BUTTON:
				SetCoreToSkipping();
				//SkipNextR4300iOpCode = TRUE;
				//WaitingForrsPStep   = FALSE;
				break;
			case IDC_BP_BUTTON:	Enter_BPoint_Window(); break;
			case IDC_R4300I_REGISTERS_BUTTON: Enter_R4300i_Register_Window(); break;
			case IDC_MEMORY_BUTTON: Enter_Memory_Window(); break;
			case IDCANCEL:
				EndDialog(hDlg, IDCANCEL);
				break;
			}
			break;
		case WM_VSCROLL:
			if ((HWND)lParam == hScrlBar) {
				int page_size = R4300i_MaxCommandLines - 1;

				switch (LOWORD(wParam)) {
				case SB_LINEUP:
					Scroll_R4300i_Commands(-1);
					break;
				case SB_LINEDOWN:
					Scroll_R4300i_Commands(1);
					break;
				case SB_PAGEUP:
					Scroll_R4300i_Commands(-page_size);
					break;
				case SB_PAGEDOWN:
					Scroll_R4300i_Commands(page_size);
					break;
				case SB_THUMBTRACK: {
					int position = HIWORD(wParam);
					if (thumb >= 0) {
						Scroll_R4300i_Commands(position - thumb);
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
	}
	return TRUE;
}

LRESULT CALLBACK R4300i_Commands_ListViewScroll_Proc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
	switch (uMsg) {
	case WM_MOUSEWHEEL:
		// Accumulate wheel deltas
		wheel -= GET_WHEEL_DELTA_WPARAM(wParam);
		if (abs(wheel) >= WHEEL_DELTA) {
			int lines = wheel / WHEEL_DELTA;
			wheel -= lines * WHEEL_DELTA;

			Scroll_R4300i_Commands(lines);
		}

		return FALSE;
	default:
		return DefSubclassProc(hDlg, uMsg, wParam, lParam);
	}
}

LRESULT CALLBACK R4300i_Commands_ListViewKeys_Proc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData) {
	switch (uMsg) {
	case WM_KEYDOWN: {
		int page_size = R4300i_MaxCommandLines - 1;

		switch (wParam) {
		case VK_UP:
			Scroll_R4300i_Commands(-1);
			return FALSE;
		case VK_DOWN:
			Scroll_R4300i_Commands(1);
			return FALSE;
		case VK_PRIOR:
			Scroll_R4300i_Commands(-page_size);
			return FALSE;
		case VK_NEXT:
			Scroll_R4300i_Commands(page_size);
			return FALSE;

		case 'C':
			if (GetKeyState(VK_CONTROL) & 0x8000) {
				R4300i_Copy_Commands();
			}
			return FALSE;
		default:
			break;
		}
		break;
	}
	case WM_RBUTTONDOWN:
		R4300i_Copy_Commands();

		has_selection = FALSE;
		selection_anchor.UDW = 0;
		selection_range[0].UDW = 0;
		selection_range[1].UDW = 0;
		RefreshR4300iCommands();
		break;
	default:
		break;
	}

	return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}

void Scroll_R4300i_Commands(int lines) {
	char value[20];
	QWORD location;
	GetWindowText(hAddress, value, sizeof(value));
	if (strlen(value) <= 8) {
		location = (int)AsciiToHex(value);
	} else {
		location = AsciiToHex64(value);
	}

	if (lines > 0) {
		if (UINT_MAX - location >= R4300i_MaxCommandLines * 4) {
			location += (QWORD)(lines * 4);
		} else {
			location = UINT_MAX - R4300i_MaxCommandLines * 4 + 1;
		}
	} else {
		if (location >= (QWORD)(-lines * 4)) {
			location += (QWORD)(lines * 4);
		} else {
			location = 0;
		}
	}

	sprintf(value, "%016llX", location);
	SetWindowText(hAddress, value);
}

void R4300i_Commands_Setup ( HWND hDlg ) {
#define WindowWidth  818
#define WindowHeight 620
	DWORD X, Y;
	
	hList = CreateWindowEx(WS_EX_STATICEDGE, "LISTBOX","", WS_CHILD | WS_VISIBLE | 
		LBS_OWNERDRAWFIXED | LBS_NOTIFY,14,30,581,545, hDlg,
		(HMENU)IDC_LIST, hInst,NULL );
	if ( hList) {
		SendMessage(hList,WM_SETFONT, (WPARAM)GetStockObject(ANSI_FIXED_FONT),0);
		SendMessage(hList,LB_SETITEMHEIGHT, (WPARAM)0,(LPARAM)MAKELPARAM(14, 0));
		MakeDragList(hList);
		DragListMsg = RegisterWindowMessage(DRAGLISTMSGSTRING);
		SetWindowSubclass(hList, R4300i_Commands_ListViewScroll_Proc, 0, 0);
		SetWindowSubclass(hList, R4300i_Commands_ListViewKeys_Proc, 0, 0);
	}

	hAddress = CreateWindowEx(0,"EDIT","", WS_CHILD | ES_UPPERCASE | WS_VISIBLE | 
		WS_BORDER | WS_TABSTOP,650,17,125,18, hDlg,(HMENU)IDC_ADDRESS,hInst, NULL );
	if (hAddress) {
		SendMessage(hAddress,WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT),0);
		SendMessage(hAddress,EM_SETLIMITTEXT, (WPARAM)16,(LPARAM)0);
	} 

	hFunctionlist = CreateWindowEx(0,"COMBOBOX","", WS_CHILD | WS_VSCROLL |
		CBS_DROPDOWNLIST | CBS_SORT | WS_TABSTOP,630,56,149,150,hDlg,
		(HMENU)IDCfunctION_COMBO,hInst,NULL);		
	if (hFunctionlist) {
		SendMessage(hFunctionlist,WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT),0);
	} 

	hGoButton = CreateWindowEx(WS_EX_STATICEDGE, "BUTTON","&Go", WS_CHILD |
		BS_DEFPUSHBUTTON | WS_VISIBLE | WS_TABSTOP, 625,56,160,24, hDlg,(HMENU)IDC_GO_BUTTON,
		hInst,NULL );
	if (hGoButton) {
		SendMessage(hGoButton,WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT),0);
	} 
	
	hBreakButton = CreateWindowEx(WS_EX_STATICEDGE, "BUTTON","&Break", WS_DISABLED |
		WS_CHILD | BS_PUSHBUTTON | WS_VISIBLE | WS_TABSTOP | BS_TEXT, 625,85,160,24,hDlg,
		(HMENU)IDC_BREAK_BUTTON,hInst,NULL );
	if (hBreakButton) {
		SendMessage(hBreakButton,WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT),0);
	}

	hStepButton = CreateWindowEx(WS_EX_STATICEDGE, "BUTTON","&Step", WS_CHILD |
		BS_PUSHBUTTON | WS_VISIBLE | WS_TABSTOP | BS_TEXT, 625,114,160,24,hDlg,
		(HMENU)IDC_STEP_BUTTON,hInst,NULL );
	if (hStepButton) {
		SendMessage(hStepButton,WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT),0);
	}

	hSkipButton = CreateWindowEx(WS_EX_STATICEDGE, "BUTTON","&Skip", WS_CHILD |
		BS_PUSHBUTTON | WS_VISIBLE | WS_TABSTOP | BS_TEXT, 625,143,160,24,hDlg,
		(HMENU)IDC_SKIP_BUTTON,hInst,NULL );
	if (hSkipButton) {
		SendMessage(hSkipButton,WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT),0);
	}

	hBPButton = CreateWindowEx(WS_EX_STATICEDGE, "BUTTON","&Break Points", WS_CHILD |
		BS_PUSHBUTTON | WS_VISIBLE | WS_TABSTOP | BS_TEXT, 625,424,160,24,hDlg,
		(HMENU)IDC_BP_BUTTON,hInst,NULL );
	if (hBPButton) {
		SendMessage(hBPButton,WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT),0);
	}
		
	hR4300iRegisters = CreateWindowEx(WS_EX_STATICEDGE,"BUTTON","R4300i &Registers...",
		WS_CHILD | BS_PUSHBUTTON | WS_VISIBLE | WS_TABSTOP | BS_TEXT, 625,453,160,24,hDlg,
		(HMENU)IDC_R4300I_REGISTERS_BUTTON,hInst,NULL );
	if (hR4300iRegisters) {
		SendMessage(hR4300iRegisters,WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT),0);
	}

	hRSPDebugger = CreateWindowEx(WS_EX_STATICEDGE,"BUTTON", "RSP &Debugger...",
		WS_CHILD | BS_PUSHBUTTON | WS_VISIBLE | WS_TABSTOP | BS_TEXT, 625,482,160,24,hDlg,
		(HMENU)IDCrsP_DEBUGGER_BUTTON,hInst,NULL );
	if (hRSPDebugger) {
		SendMessage(hRSPDebugger,WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT),0);
	}

	hRSPRegisters = CreateWindowEx(WS_EX_STATICEDGE,"BUTTON", "RSP R&egisters...",
		WS_CHILD | BS_PUSHBUTTON | WS_VISIBLE | WS_TABSTOP | BS_TEXT, 625,511,160,24,hDlg,
		(HMENU)IDCrsP_REGISTERS_BUTTON,hInst,NULL );
	if (hRSPRegisters) {
		SendMessage(hRSPRegisters,WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT),0);
	} 

	hMemory = CreateWindowEx(WS_EX_STATICEDGE,"BUTTON", "&Memory...", WS_CHILD |
		BS_PUSHBUTTON | WS_VISIBLE | WS_TABSTOP | BS_TEXT, 625,540,160,24,hDlg,
		(HMENU)IDC_MEMORY_BUTTON,hInst,NULL );
	if (hMemory) {
		SendMessage(hMemory,WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT),0);
	}
	
	hScrlBar = CreateWindowEx(0, "SCROLLBAR","", WS_CHILD | WS_VISIBLE |
		WS_TABSTOP | SBS_VERT, 596,14,18,539, hDlg, (HMENU)IDC_SCRL_BAR, hInst, NULL );
	if (hScrlBar) {
		SetWindowSubclass(hScrlBar, R4300i_Commands_ListViewScroll_Proc, 0, 0);
	}

	if ( RomFileSize != 0 ) {
		Enable_R4300i_Commands_Window();
	} else {
		Disable_R4300i_Commands_Window();
	}
	
	if ( !GetStoredWinPos( "R4300i Commands", &X, &Y ) ) {
		X = (GetSystemMetrics( SM_CXSCREEN ) - WindowWidth) / 2;
		Y = (GetSystemMetrics( SM_CYSCREEN ) - WindowHeight) / 2;
	}
	SetWindowText(hDlg,"R4300i Commands");

	SetWindowPos(hDlg,NULL,X,Y,WindowWidth,WindowHeight, SWP_NOZORDER | 
		SWP_SHOWWINDOW);

}

char * R4300iRegImmName ( DWORD OpCode, MIPS_DWORD PC ) {
	OPCODE command;
	command.Hex = OpCode;
	MIPS_DWORD RelativeTarget;
	RelativeTarget.UDW = PC.UDW + ((short)command.BRANCH.offset << 2) + 4;

	switch (command.BRANCH.rt) {
	case R4300i_REGIMM_BLTZ:
		sprintf(CommandName,"bltz\t%s, %s",GPR_Name[command.BRANCH.rs], LabelName(RelativeTarget));
		break;
	case R4300i_REGIMM_BGEZ:
		if (command.BRANCH.rs == 0) {
			sprintf(CommandName,"b\t%s", LabelName(RelativeTarget));
		} else {
			sprintf(CommandName,"bgez\t%s, %s",GPR_Name[command.BRANCH.rs], LabelName(RelativeTarget));
		}
		break;
	case R4300i_REGIMM_BLTZL:
		sprintf(CommandName,"bltzl\t%s, %s",GPR_Name[command.BRANCH.rs], LabelName(RelativeTarget));
		break;
	case R4300i_REGIMM_BGEZL:
		sprintf(CommandName,"bgezl\t%s, %s",GPR_Name[command.BRANCH.rs], LabelName(RelativeTarget));
		break;
	case R4300i_REGIMM_TGEI:
		sprintf(CommandName,"tgei\t%s, 0x%X",GPR_Name[command.BRANCH.rs],command.IMM.immediate);
		break;
	case R4300i_REGIMM_TGEIU:
		sprintf(CommandName,"tgeiu\t%s, 0x%X",GPR_Name[command.BRANCH.rs],command.IMM.immediate);
		break;
	case R4300i_REGIMM_TLTI:
		sprintf(CommandName,"tlti\t%s, 0x%X",GPR_Name[command.BRANCH.rs],command.IMM.immediate);
		break;
	case R4300i_REGIMM_TLTIU:
		sprintf(CommandName,"tltiu\t%s, 0x%X",GPR_Name[command.BRANCH.rs],command.IMM.immediate);
		break;
	case R4300i_REGIMM_TEQI:
		sprintf(CommandName,"teqi\t%s, 0x%X",GPR_Name[command.BRANCH.rs],command.IMM.immediate);
		break;
	case R4300i_REGIMM_TNEI:
		sprintf(CommandName,"tnei\t%s, 0x%X",GPR_Name[command.BRANCH.rs],command.IMM.immediate);
		break;
	case R4300i_REGIMM_BLTZAL:
		sprintf(CommandName,"bltzal\t%s, %s",GPR_Name[command.BRANCH.rs], LabelName(RelativeTarget));
		break;
	case R4300i_REGIMM_BGEZAL:
		if (command.BRANCH.rs == 0) {
			sprintf(CommandName,"bal\t%s",LabelName(RelativeTarget));
		} else {
			sprintf(CommandName,"bgezal\t%s, %s",GPR_Name[command.BRANCH.rs], LabelName(RelativeTarget));
		}
		break;
	case R4300i_REGIMM_BLTZALL:
		sprintf(CommandName,"bltzall\t%s, %s",GPR_Name[command.BRANCH.rs], LabelName(RelativeTarget));
		break;
	case R4300i_REGIMM_BGEZALL:
		sprintf(CommandName,"bgezall\t%s, %s",GPR_Name[command.BRANCH.rs], LabelName(RelativeTarget));
		break;
	default:	
		sprintf(CommandName,"Unknown\t%02X %02X %02X %02X",
			command.Ascii[3],command.Ascii[2],command.Ascii[1],command.Ascii[0]);
	}
	return CommandName;
}

char * R4300iSpecialName ( DWORD OpCode, MIPS_DWORD PC ) {
	OPCODE command;
	command.Hex = OpCode;

	switch (command.REG.funct) {
	case R4300i_SPECIAL_SLL:
		if (command.Hex != 0) {
			sprintf(CommandName,"sll\t%s, %s, 0x%X",GPR_Name[command.REG.rd],
			GPR_Name[command.BRANCH.rt], command.REG.sa);
		} else {
			sprintf(CommandName,"nop");
		}
		break;
	case R4300i_SPECIAL_SRL:
		sprintf(CommandName,"srl\t%s, %s, 0x%X",GPR_Name[command.REG.rd], GPR_Name[command.BRANCH.rt],
			command.REG.sa);
		break;
	case R4300i_SPECIAL_SRA:
		sprintf(CommandName,"sra\t%s, %s, 0x%X",GPR_Name[command.REG.rd], GPR_Name[command.BRANCH.rt],
				command.REG.sa);
		break;
	case R4300i_SPECIAL_SLLV:
		sprintf(CommandName,"sllv\t%s, %s, %s",GPR_Name[command.REG.rd], GPR_Name[command.BRANCH.rt],
			GPR_Name[command.BRANCH.rs]);
		break;
	case R4300i_SPECIAL_SRLV:
		sprintf(CommandName,"srlv\t%s, %s, %s",GPR_Name[command.REG.rd], GPR_Name[command.BRANCH.rt],
			GPR_Name[command.BRANCH.rs]);
		break;
	case R4300i_SPECIAL_SRAV:
		sprintf(CommandName,"srav\t%s, %s, %s",GPR_Name[command.REG.rd], GPR_Name[command.BRANCH.rt],
			GPR_Name[command.BRANCH.rs]);
		break;
	case R4300i_SPECIAL_JR:
		sprintf(CommandName,"jr\t%s",GPR_Name[command.BRANCH.rs]);
		break;
	case R4300i_SPECIAL_JALR:
		sprintf(CommandName,"jalr\t%s, %s",GPR_Name[command.REG.rd],GPR_Name[command.BRANCH.rs]);
		break;
	case R4300i_SPECIAL_SYSCALL:
		sprintf(CommandName,"system call");
		break;
	case R4300i_SPECIAL_BREAK:
		sprintf(CommandName,"break");
		break;
	case R4300i_SPECIAL_SYNC:
		sprintf(CommandName,"sync");
		break;
	case R4300i_SPECIAL_MFHI:
		sprintf(CommandName,"mfhi\t%s",GPR_Name[command.REG.rd]);
		break;
	case R4300i_SPECIAL_MTHI:
		sprintf(CommandName,"mthi\t%s",GPR_Name[command.BRANCH.rs]);
		break;
	case R4300i_SPECIAL_MFLO:
		sprintf(CommandName,"mflo\t%s",GPR_Name[command.REG.rd]);
		break;
	case R4300i_SPECIAL_MTLO:
		sprintf(CommandName,"mtlo\t%s",GPR_Name[command.BRANCH.rs]);
		break;
	case R4300i_SPECIAL_DSLLV:
		sprintf(CommandName,"dsllv\t%s, %s, %s",GPR_Name[command.REG.rd], GPR_Name[command.BRANCH.rt],
			GPR_Name[command.BRANCH.rs]);
		break;
	case R4300i_SPECIAL_DSRLV:
		sprintf(CommandName,"dsrlv\t%s, %s, %s",GPR_Name[command.REG.rd], GPR_Name[command.BRANCH.rt],
			GPR_Name[command.BRANCH.rs]);
		break;
	case R4300i_SPECIAL_DSRAV:
		sprintf(CommandName,"dsrav\t%s, %s, %s",GPR_Name[command.REG.rd], GPR_Name[command.BRANCH.rt],
			GPR_Name[command.BRANCH.rs]);
		break;
	case R4300i_SPECIAL_MULT:
		sprintf(CommandName,"mult\t%s, %s",GPR_Name[command.BRANCH.rs], GPR_Name[command.BRANCH.rt]);
		break;
	case R4300i_SPECIAL_MULTU:
		sprintf(CommandName,"multu\t%s, %s",GPR_Name[command.BRANCH.rs], GPR_Name[command.BRANCH.rt]);
		break;
	case R4300i_SPECIAL_DIV:
		sprintf(CommandName,"div\t%s, %s",GPR_Name[command.BRANCH.rs], GPR_Name[command.BRANCH.rt]);
		break;
	case R4300i_SPECIAL_DIVU:
		sprintf(CommandName,"divu\t%s, %s",GPR_Name[command.BRANCH.rs], GPR_Name[command.BRANCH.rt]);
		break;
	case R4300i_SPECIAL_DMULT:
		sprintf(CommandName,"dmult\t%s, %s",GPR_Name[command.BRANCH.rs], GPR_Name[command.BRANCH.rt]);
		break;
	case R4300i_SPECIAL_DMULTU:
		sprintf(CommandName,"dmultu\t%s, %s",GPR_Name[command.BRANCH.rs], GPR_Name[command.BRANCH.rt]);
		break;
	case R4300i_SPECIAL_DDIV:
		sprintf(CommandName,"ddiv\t%s, %s",GPR_Name[command.BRANCH.rs], GPR_Name[command.BRANCH.rt]);
		break;
	case R4300i_SPECIAL_DDIVU:
		sprintf(CommandName,"ddivu\t%s, %s",GPR_Name[command.BRANCH.rs], GPR_Name[command.BRANCH.rt]);
		break;
	case R4300i_SPECIAL_ADD:
		sprintf(CommandName,"add\t%s, %s, %s",GPR_Name[command.REG.rd], GPR_Name[command.BRANCH.rs],
			GPR_Name[command.BRANCH.rt]);
		break;
	case R4300i_SPECIAL_ADDU:
		sprintf(CommandName,"addu\t%s, %s, %s",GPR_Name[command.REG.rd], GPR_Name[command.BRANCH.rs],
			GPR_Name[command.BRANCH.rt]);
		break;
	case R4300i_SPECIAL_SUB:
		sprintf(CommandName,"sub\t%s, %s, %s",GPR_Name[command.REG.rd], GPR_Name[command.BRANCH.rs],
			GPR_Name[command.BRANCH.rt]);
		break;
	case R4300i_SPECIAL_SUBU:
		sprintf(CommandName,"subu\t%s, %s, %s",GPR_Name[command.REG.rd], GPR_Name[command.BRANCH.rs],
			GPR_Name[command.BRANCH.rt]);
		break;
	case R4300i_SPECIAL_AND:
		sprintf(CommandName,"and\t%s, %s, %s",GPR_Name[command.REG.rd], GPR_Name[command.BRANCH.rs],
			GPR_Name[command.BRANCH.rt]);
		break;
	case R4300i_SPECIAL_OR:
		sprintf(CommandName,"or\t%s, %s, %s",GPR_Name[command.REG.rd], GPR_Name[command.BRANCH.rs],
			GPR_Name[command.BRANCH.rt]);
		break;
	case R4300i_SPECIAL_XOR:
		sprintf(CommandName,"xor\t%s, %s, %s",GPR_Name[command.REG.rd], GPR_Name[command.BRANCH.rs],
			GPR_Name[command.BRANCH.rt]);
		break;
	case R4300i_SPECIAL_NOR:
		sprintf(CommandName,"nor\t%s, %s, %s",GPR_Name[command.REG.rd], GPR_Name[command.BRANCH.rs],
			GPR_Name[command.BRANCH.rt]);
		break;
	case R4300i_SPECIAL_SLT:
		sprintf(CommandName,"slt\t%s, %s, %s",GPR_Name[command.REG.rd], GPR_Name[command.BRANCH.rs],
			GPR_Name[command.BRANCH.rt]);
		break;
	case R4300i_SPECIAL_SLTU:
		sprintf(CommandName,"sltu\t%s, %s, %s",GPR_Name[command.REG.rd], GPR_Name[command.BRANCH.rs],
			GPR_Name[command.BRANCH.rt]);
		break;
	case R4300i_SPECIAL_DADD:
		sprintf(CommandName,"dadd\t%s, %s, %s",GPR_Name[command.REG.rd], GPR_Name[command.BRANCH.rs],
			GPR_Name[command.BRANCH.rt]);
		break;
	case R4300i_SPECIAL_DADDU:
		sprintf(CommandName,"daddu\t%s, %s, %s",GPR_Name[command.REG.rd], GPR_Name[command.BRANCH.rs],
			GPR_Name[command.BRANCH.rt]);
		break;
	case R4300i_SPECIAL_DSUB:
		sprintf(CommandName,"dsub\t%s, %s, %s",GPR_Name[command.REG.rd], GPR_Name[command.BRANCH.rs],
			GPR_Name[command.BRANCH.rt]);
		break;
	case R4300i_SPECIAL_DSUBU:
		sprintf(CommandName,"dsubu\t%s, %s, %s",GPR_Name[command.REG.rd], GPR_Name[command.BRANCH.rs],
			GPR_Name[command.BRANCH.rt]);
		break;
	case R4300i_SPECIAL_TGE:
		sprintf(CommandName,"tge\t%s, %s",GPR_Name[command.BRANCH.rs],GPR_Name[command.BRANCH.rt]);
		break;
	case R4300i_SPECIAL_TGEU:
		sprintf(CommandName,"tgeu\t%s, %s",GPR_Name[command.BRANCH.rs],GPR_Name[command.BRANCH.rt]);
		break;
	case R4300i_SPECIAL_TLT:
		sprintf(CommandName,"tlt\t%s, %s",GPR_Name[command.BRANCH.rs],GPR_Name[command.BRANCH.rt]);
		break;
	case R4300i_SPECIAL_TLTU:
		sprintf(CommandName,"tltu\t%s, %s",GPR_Name[command.BRANCH.rs],GPR_Name[command.BRANCH.rt]);
		break;
	case R4300i_SPECIAL_TEQ:
		sprintf(CommandName,"teq\t%s, %s",GPR_Name[command.BRANCH.rs],GPR_Name[command.BRANCH.rt]);
		break;
	case R4300i_SPECIAL_TNE:
		sprintf(CommandName,"tne\t%s, %s",GPR_Name[command.BRANCH.rs],GPR_Name[command.BRANCH.rt]);
		break;
	case R4300i_SPECIAL_DSLL:
		sprintf(CommandName,"dsll\t%s, %s, 0x%X",GPR_Name[command.REG.rd],
			GPR_Name[command.BRANCH.rt], command.REG.sa);
		break;
	case R4300i_SPECIAL_DSRL:
		sprintf(CommandName,"dsrl\t%s, %s, 0x%X",GPR_Name[command.REG.rd], GPR_Name[command.BRANCH.rt],
			command.REG.sa);
		break;
	case R4300i_SPECIAL_DSRA:
		sprintf(CommandName,"dsra\t%s, %s, 0x%X",GPR_Name[command.REG.rd], GPR_Name[command.BRANCH.rt],
			command.REG.sa);
		break;
	case R4300i_SPECIAL_DSLL32:
		sprintf(CommandName,"dsll32\t%s, %s, 0x%X",GPR_Name[command.REG.rd],GPR_Name[command.BRANCH.rt], command.REG.sa);
		break;
	case R4300i_SPECIAL_DSRL32:
		sprintf(CommandName,"dsrl32\t%s, %s, 0x%X",GPR_Name[command.REG.rd], GPR_Name[command.BRANCH.rt], command.REG.sa);
		break;
	case R4300i_SPECIAL_DSRA32:
		sprintf(CommandName,"dsra32\t%s, %s, 0x%X",GPR_Name[command.REG.rd], GPR_Name[command.BRANCH.rt], command.REG.sa);
		break;
	default:	
		sprintf(CommandName,"Unknown\t%02X %02X %02X %02X",
			command.Ascii[3],command.Ascii[2],command.Ascii[1],command.Ascii[0]);
	}
	return CommandName;
}

char * R4300iCop1Name ( DWORD OpCode, MIPS_DWORD PC ) {
	OPCODE command;
	command.Hex = OpCode;
	MIPS_DWORD RelativeTarget;
	RelativeTarget.UDW = PC.UDW + ((short)command.BRANCH.offset << 2) + 4;

	switch (command.FP.fmt) {
	case R4300i_COP1_MF:
		sprintf(CommandName,"mfc1\t%s, %s",GPR_Name[command.BRANCH.rt], FPR_Name[command.FP.fs]);
		break;
	case R4300i_COP1_DMF:
		sprintf(CommandName,"dmfc1\t%s, %s",GPR_Name[command.BRANCH.rt], FPR_Name[command.FP.fs]);
		break;
	case R4300i_COP1_CF:
		sprintf(CommandName,"cfc1\t%s, %s",GPR_Name[command.BRANCH.rt], FPR_Ctrl_Name[command.FP.fs]);
		break;
	case R4300i_COP1_DCF:
		sprintf(CommandName, "dcfc1\t%s, %s", GPR_Name[command.BRANCH.rt], FPR_Ctrl_Name[command.FP.fs]);
		break;
	case R4300i_COP1_MT:
		sprintf(CommandName,"mtc1\t%s, %s",GPR_Name[command.BRANCH.rt], FPR_Name[command.FP.fs]);
		break;
	case R4300i_COP1_DMT:
		sprintf(CommandName,"dmtc1\t%s, %s",GPR_Name[command.BRANCH.rt], FPR_Name[command.FP.fs]);
		break;
	case R4300i_COP1_CT:
		sprintf(CommandName,"ctc1\t%s, %s",GPR_Name[command.BRANCH.rt], FPR_Ctrl_Name[command.FP.fs]);
		break;
	case R4300i_COP1_DCT:
		sprintf(CommandName, "dctc1\t%s, %s", GPR_Name[command.BRANCH.rt], FPR_Ctrl_Name[command.FP.fs]);
		break;
	case R4300i_COP1_BC:
		switch (command.FP.ft) {
		case R4300i_COP1_BC_BCF:
			sprintf(CommandName,"bc1f\t%s", LabelName(RelativeTarget));
			break;
		case R4300i_COP1_BC_BCT:
			sprintf(CommandName,"bc1t\t%s", LabelName(RelativeTarget));
			break;
		case R4300i_COP1_BC_BCFL:
			sprintf(CommandName,"bc1fl\t%s", LabelName(RelativeTarget));
			break;
		case R4300i_COP1_BC_BCTL:
			sprintf(CommandName,"bc1tl\t%s", LabelName(RelativeTarget));
			break;
		default:
			sprintf(CommandName,"Unknown Cop1\t%02X %02X %02X %02X",
				command.Ascii[3],command.Ascii[2],command.Ascii[1],command.Ascii[0]);
		}
		break;
	case R4300i_COP1_S:
	case R4300i_COP1_D:
	case R4300i_COP1_W:
	case R4300i_COP1_L:
		switch (command.REG.funct) {			
		case R4300i_COP1_FUNCT_ADD:
			sprintf(CommandName,"add.%s\t%s, %s, %s",FPR_Type(command.FP.fmt),  
				FPR_Name[command.FP.fd], FPR_Name[command.FP.fs], 
				FPR_Name[command.FP.ft]);
			break;
		case R4300i_COP1_FUNCT_SUB:
			sprintf(CommandName,"sub.%s\t%s, %s, %s",FPR_Type(command.FP.fmt),  
				FPR_Name[command.FP.fd], FPR_Name[command.FP.fs], 
				FPR_Name[command.FP.ft]);
			break;
		case R4300i_COP1_FUNCT_MUL:
			sprintf(CommandName,"mul.%s\t%s, %s, %s",FPR_Type(command.FP.fmt),  
				FPR_Name[command.FP.fd], FPR_Name[command.FP.fs], 
				FPR_Name[command.FP.ft]);
			break;
		case R4300i_COP1_FUNCT_DIV:
			sprintf(CommandName,"div.%s\t%s, %s, %s",FPR_Type(command.FP.fmt),  
				FPR_Name[command.FP.fd], FPR_Name[command.FP.fs], 
				FPR_Name[command.FP.ft]);
			break;
		case R4300i_COP1_FUNCT_SQRT:
			sprintf(CommandName,"sqrt.%s\t%s, %s",FPR_Type(command.FP.fmt),  
				FPR_Name[command.FP.fd], FPR_Name[command.FP.fs]);
			break;
		case R4300i_COP1_FUNCT_ABS:
			sprintf(CommandName,"abs.%s\t%s, %s",FPR_Type(command.FP.fmt),  
				FPR_Name[command.FP.fd], FPR_Name[command.FP.fs]);
			break;
		case R4300i_COP1_FUNCT_MOV:
			sprintf(CommandName,"mov.%s\t%s, %s",FPR_Type(command.FP.fmt),  
				FPR_Name[command.FP.fd], FPR_Name[command.FP.fs]);
			break;
		case R4300i_COP1_FUNCT_NEG:
			sprintf(CommandName,"neg.%s\t%s, %s",FPR_Type(command.FP.fmt),  
				FPR_Name[command.FP.fd], FPR_Name[command.FP.fs]);
			break;
		case R4300i_COP1_FUNCT_ROUND_L:
			sprintf(CommandName,"round.l.%s\t%s, %s",FPR_Type(command.FP.fmt),  
				FPR_Name[command.FP.fd], FPR_Name[command.FP.fs]);
			break;
		case R4300i_COP1_FUNCT_TRUNC_L:
			sprintf(CommandName,"trunc.l.%s\t%s, %s",FPR_Type(command.FP.fmt),  
				FPR_Name[command.FP.fd], FPR_Name[command.FP.fs]);
			break;
		case R4300i_COP1_FUNCT_CEIL_L:
			sprintf(CommandName,"ceil.l.%s\t%s, %s",FPR_Type(command.FP.fmt),  
				FPR_Name[command.FP.fd], FPR_Name[command.FP.fs]);
			break;
		case R4300i_COP1_FUNCT_FLOOR_L:
			sprintf(CommandName,"floor.l.%s\t%s, %s",FPR_Type(command.FP.fmt),  
				FPR_Name[command.FP.fd], FPR_Name[command.FP.fs]);
			break;
		case R4300i_COP1_FUNCT_ROUND_W:
			sprintf(CommandName,"round.w.%s\t%s, %s",FPR_Type(command.FP.fmt),  
				FPR_Name[command.FP.fd], FPR_Name[command.FP.fs]);
			break;
		case R4300i_COP1_FUNCT_TRUNC_W:
			sprintf(CommandName,"trunc.w.%s\t%s, %s",FPR_Type(command.FP.fmt),  
				FPR_Name[command.FP.fd], FPR_Name[command.FP.fs]);
			break;
		case R4300i_COP1_FUNCT_CEIL_W:
			sprintf(CommandName,"ceil.w.%s\t%s, %s",FPR_Type(command.FP.fmt),  
				FPR_Name[command.FP.fd], FPR_Name[command.FP.fs]);
			break;
		case R4300i_COP1_FUNCT_FLOOR_W:
			sprintf(CommandName,"floor.w.%s\t%s, %s",FPR_Type(command.FP.fmt),  
				FPR_Name[command.FP.fd], FPR_Name[command.FP.fs]);
			break;
		case R4300i_COP1_FUNCT_CVT_S:
			sprintf(CommandName,"cvt.s.%s\t%s, %s",FPR_Type(command.FP.fmt),  
				FPR_Name[command.FP.fd], FPR_Name[command.FP.fs]);
			break;
		case R4300i_COP1_FUNCT_CVT_D:
			sprintf(CommandName,"cvt.d.%s\t%s, %s",FPR_Type(command.FP.fmt),  
				FPR_Name[command.FP.fd], FPR_Name[command.FP.fs]);
			break;
		case R4300i_COP1_FUNCT_CVT_W:
			sprintf(CommandName,"cvt.w.%s\t%s, %s",FPR_Type(command.FP.fmt),  
				FPR_Name[command.FP.fd], FPR_Name[command.FP.fs]);
			break;
		case R4300i_COP1_FUNCT_CVT_L:
			sprintf(CommandName,"cvt.l.%s\t%s, %s",FPR_Type(command.FP.fmt),  
				FPR_Name[command.FP.fd], FPR_Name[command.FP.fs]);
			break;
		case R4300i_COP1_FUNCT_C_F:
			sprintf(CommandName,"c.f.%s\t%s, %s",FPR_Type(command.FP.fmt),  
				FPR_Name[command.FP.fs], FPR_Name[command.FP.ft]);
			break;
		case R4300i_COP1_FUNCT_C_UN:
			sprintf(CommandName,"c.un.%s\t%s, %s",FPR_Type(command.FP.fmt),  
				FPR_Name[command.FP.fs], FPR_Name[command.FP.ft]);
			break;
		case R4300i_COP1_FUNCT_C_EQ:
			sprintf(CommandName,"c.eq.%s\t%s, %s",FPR_Type(command.FP.fmt),  
				FPR_Name[command.FP.fs], FPR_Name[command.FP.ft]);
			break;
		case R4300i_COP1_FUNCT_C_UEQ:
			sprintf(CommandName,"c.ueq.%s\t%s, %s",FPR_Type(command.FP.fmt),  
				FPR_Name[command.FP.fs], FPR_Name[command.FP.ft]);
			break;
		case R4300i_COP1_FUNCT_C_OLT:
			sprintf(CommandName,"c.olt.%s\t%s, %s",FPR_Type(command.FP.fmt),  
				FPR_Name[command.FP.fs], FPR_Name[command.FP.ft]);
			break;
		case R4300i_COP1_FUNCT_C_ULT:
			sprintf(CommandName,"c.ult.%s\t%s, %s",FPR_Type(command.FP.fmt),  
				FPR_Name[command.FP.fs], FPR_Name[command.FP.ft]);
			break;
		case R4300i_COP1_FUNCT_C_OLE:
			sprintf(CommandName,"c.ole.%s\t%s, %s",FPR_Type(command.FP.fmt),  
				FPR_Name[command.FP.fs], FPR_Name[command.FP.ft]);
			break;
		case R4300i_COP1_FUNCT_C_ULE:
			sprintf(CommandName,"c.ule.%s\t%s, %s",FPR_Type(command.FP.fmt),  
				FPR_Name[command.FP.fs], FPR_Name[command.FP.ft]);
			break;
		case R4300i_COP1_FUNCT_C_SF:
			sprintf(CommandName,"c.sf.%s\t%s, %s",FPR_Type(command.FP.fmt),  
				FPR_Name[command.FP.fs], FPR_Name[command.FP.ft]);
			break;
		case R4300i_COP1_FUNCT_C_NGLE:
			sprintf(CommandName,"c.ngle.%s\t%s, %s",FPR_Type(command.FP.fmt),  
				FPR_Name[command.FP.fs], FPR_Name[command.FP.ft]);
			break;
		case R4300i_COP1_FUNCT_C_SEQ:
			sprintf(CommandName,"c.seq.%s\t%s, %s",FPR_Type(command.FP.fmt),  
				FPR_Name[command.FP.fs], FPR_Name[command.FP.ft]);
			break;
		case R4300i_COP1_FUNCT_C_NGL:
			sprintf(CommandName,"c.ngl.%s\t%s, %s",FPR_Type(command.FP.fmt),  
				FPR_Name[command.FP.fs], FPR_Name[command.FP.ft]);
			break;
		case R4300i_COP1_FUNCT_C_LT:
			sprintf(CommandName,"c.lt.%s\t%s, %s",FPR_Type(command.FP.fmt),  
				FPR_Name[command.FP.fs], FPR_Name[command.FP.ft]);
			break;
		case R4300i_COP1_FUNCT_C_NGE:
			sprintf(CommandName,"c.nge.%s\t%s, %s",FPR_Type(command.FP.fmt),  
				FPR_Name[command.FP.fs], FPR_Name[command.FP.ft]);
			break;
		case R4300i_COP1_FUNCT_C_LE:
			sprintf(CommandName,"c.le.%s\t%s, %s",FPR_Type(command.FP.fmt),  
				FPR_Name[command.FP.fs], FPR_Name[command.FP.ft]);
			break;
		case R4300i_COP1_FUNCT_C_NGT:
			sprintf(CommandName,"c.ngt.%s\t%s, %s",FPR_Type(command.FP.fmt),  
				FPR_Name[command.FP.fs], FPR_Name[command.FP.ft]);
			break;
		default:
			sprintf(CommandName,"Unknown Cop1\t%02X %02X %02X %02X",
				command.Ascii[3],command.Ascii[2],command.Ascii[1],command.Ascii[0]);
		}
		break;
	default:
		sprintf(CommandName,"Unknown Cop1\t%02X %02X %02X %02X",
			command.Ascii[3],command.Ascii[2],command.Ascii[1],command.Ascii[0]);
	}
	return CommandName;
}

char * R4300iOpcodeName ( DWORD OpCode, MIPS_DWORD PC ) {
	OPCODE command;
	command.Hex = OpCode;
	MIPS_DWORD RelativeTarget;
	RelativeTarget.UDW = PC.UDW + ((short)command.BRANCH.offset << 2) + 4;
	MIPS_DWORD AbsoluteTarget;
	AbsoluteTarget.UDW = (PC.UDW & 0xFFFFFFFFF0000000LL) + (command.JMP.target << 2);
		
	switch (command.BRANCH.op) {
	case R4300i_SPECIAL:
		return R4300iSpecialName ( OpCode, PC );
		break;
	case R4300i_REGIMM:
		return R4300iRegImmName ( OpCode, PC );
		break;
	case R4300i_J:
		sprintf(CommandName,"j\t%s",LabelName(AbsoluteTarget));
		break;
	case R4300i_JAL:
		sprintf(CommandName,"jal\t%s",LabelName(AbsoluteTarget));
		break;
	case R4300i_BEQ:
		if (command.BRANCH.rs == 0 && command.BRANCH.rt == 0) {
			sprintf(CommandName,"b\t%s", LabelName(RelativeTarget));
		} else if (command.BRANCH.rs == 0 || command.BRANCH.rt == 0) {
			sprintf(CommandName,"beqz\t%s, %s", GPR_Name[command.BRANCH.rs == 0 ? command.BRANCH.rt : command.BRANCH.rs ],
				LabelName(RelativeTarget));
		} else {
			sprintf(CommandName,"beq\t%s, %s, %s", GPR_Name[command.BRANCH.rs], GPR_Name[command.BRANCH.rt],
				LabelName(RelativeTarget));
		}
		break;
	case R4300i_BNE:
		if ((command.BRANCH.rs == 0) ^ (command.BRANCH.rt == 0)){
			sprintf(CommandName,"bnez\t%s, %s", GPR_Name[command.BRANCH.rs == 0 ? command.BRANCH.rt : command.BRANCH.rs ],
				LabelName(RelativeTarget));
		} else {
			sprintf(CommandName,"bne\t%s, %s, %s", GPR_Name[command.BRANCH.rs], GPR_Name[command.BRANCH.rt],
				LabelName(RelativeTarget));
		}
		break;
	case R4300i_BLEZ:
		sprintf(CommandName,"blez\t%s, %s",GPR_Name[command.BRANCH.rs], LabelName(RelativeTarget));
		break;
	case R4300i_BGTZ:
		sprintf(CommandName,"bgtz\t%s, %s",GPR_Name[command.BRANCH.rs], LabelName(RelativeTarget));
		break;
	case R4300i_ADDI:
		sprintf(CommandName,"addi\t%s, %s, 0x%X",GPR_Name[command.BRANCH.rt], GPR_Name[command.BRANCH.rs],command.IMM.immediate);
		break;
	case R4300i_ADDIU:
		sprintf(CommandName,"addiu\t%s, %s, 0x%X",GPR_Name[command.BRANCH.rt], GPR_Name[command.BRANCH.rs],command.IMM.immediate);
		break;
	case R4300i_SLTI:
		sprintf(CommandName,"slti\t%s, %s, 0x%X",GPR_Name[command.BRANCH.rt], GPR_Name[command.BRANCH.rs],command.IMM.immediate);
		break;
	case R4300i_SLTIU:
		sprintf(CommandName,"sltiu\t%s, %s, 0x%X",GPR_Name[command.BRANCH.rt], GPR_Name[command.BRANCH.rs],command.IMM.immediate);
		break;
	case R4300i_ANDI:
		sprintf(CommandName,"andi\t%s, %s, 0x%X",GPR_Name[command.BRANCH.rt], GPR_Name[command.BRANCH.rs],command.IMM.immediate);
		break;
	case R4300i_ORI:
		sprintf(CommandName,"ori\t%s, %s, 0x%X",GPR_Name[command.BRANCH.rt], GPR_Name[command.BRANCH.rs],command.IMM.immediate);
		break;
	case R4300i_XORI:
		sprintf(CommandName,"xori\t%s, %s, 0x%X",GPR_Name[command.BRANCH.rt], GPR_Name[command.BRANCH.rs],command.IMM.immediate);
		break;
	case R4300i_LUI:
		sprintf(CommandName,"lui\t%s, 0x%X",GPR_Name[command.BRANCH.rt], command.IMM.immediate);
		break;
	case R4300i_CP0:
		switch (command.BRANCH.rs) {
		case R4300i_COP0_MF:
			sprintf(CommandName,"mfc0\t%s, %s",GPR_Name[command.BRANCH.rt], Cop0_Name[command.REG.rd]);
			break;
		case R4300i_COP0_DMF:
			sprintf(CommandName, "dmfc0\t%s, %s", GPR_Name[command.BRANCH.rt], Cop0_Name[command.REG.rd]);
			break;
		case R4300i_COP0_MT:
			sprintf(CommandName,"mtc0\t%s, %s",GPR_Name[command.BRANCH.rt], Cop0_Name[command.REG.rd]);
			break;
		case R4300i_COP0_DMT:
			sprintf(CommandName, "dmtc0\t%s, %s", GPR_Name[command.BRANCH.rt], Cop0_Name[command.REG.rd]);
			break;
		default:
			if ( (command.BRANCH.rs & 0x10 ) != 0 ) {
				switch( command.REG.funct ) {
				case R4300i_COP0_CO_TLBR:  sprintf(CommandName,"tlbr"); break;
				case R4300i_COP0_CO_TLBWI: sprintf(CommandName,"tlbwi"); break;
				case R4300i_COP0_CO_TLBWR: sprintf(CommandName,"tlbwr"); break;
				case R4300i_COP0_CO_TLBP:  sprintf(CommandName,"tlbp"); break;
				case R4300i_COP0_CO_ERET:  sprintf(CommandName,"eret"); break;
				default:	
					sprintf(CommandName,"Unknown\t%02X %02X %02X %02X",
						command.Ascii[3],command.Ascii[2],command.Ascii[1],command.Ascii[0]);
				}
			} else {
				sprintf(CommandName,"Unknown\t%02X %02X %02X %02X",
				command.Ascii[3],command.Ascii[2],command.Ascii[1],command.Ascii[0]);
			}
			break;
		}
		break;
	case R4300i_CP1:
		return R4300iCop1Name ( OpCode, PC );
	case R4300i_CP2:
		switch (command.BRANCH.rs) {
		case R4300i_COP2_MF:
			sprintf(CommandName, "mfc2\t%s, %d", GPR_Name[command.BRANCH.rt], command.REG.rd);
			break;
		case R4300i_COP2_DMF:
			sprintf(CommandName, "dmfc2\t%s, %d", GPR_Name[command.BRANCH.rt], command.REG.rd);
			break;
		case R4300i_COP2_CF:
			sprintf(CommandName, "cfc2\t%s, %d", GPR_Name[command.BRANCH.rt], command.REG.rd);
			break;
		case R4300i_COP2_DCF:
			sprintf(CommandName, "dcfc2\t%s, %d", GPR_Name[command.BRANCH.rt], command.REG.rd);
			break;
		case R4300i_COP2_MT:
			sprintf(CommandName, "mtc2\t%s, %d", GPR_Name[command.BRANCH.rt], command.REG.rd);
			break;
		case R4300i_COP2_DMT:
			sprintf(CommandName, "dmtc2\t%s, %d", GPR_Name[command.BRANCH.rt], command.REG.rd);
			break;
		case R4300i_COP2_CT:
			sprintf(CommandName, "ctc2\t%s, %d", GPR_Name[command.BRANCH.rt], command.REG.rd);
			break;
		case R4300i_COP2_DCT:
			sprintf(CommandName, "dctc2\t%s, %d", GPR_Name[command.BRANCH.rt], command.REG.rd);
			break;
		default:
			sprintf(CommandName, "Unknown\t%02X %02X %02X %02X",
				command.Ascii[3], command.Ascii[2], command.Ascii[1], command.Ascii[0]);
		}
		break;
	case R4300i_CP3:
		sprintf(CommandName, "cop3");
		break;
	case R4300i_BEQL:
		if (command.BRANCH.rs == command.BRANCH.rt) {
			sprintf(CommandName,"b\t%s", LabelName(RelativeTarget));
		} else if ((command.BRANCH.rs == 0) ^ (command.BRANCH.rt == 0)){
			sprintf(CommandName,"beqzl\t%s, %s", GPR_Name[command.BRANCH.rs == 0 ? command.BRANCH.rt : command.BRANCH.rs ],
				LabelName(RelativeTarget));
		} else {
			sprintf(CommandName,"beql\t%s, %s, %s", GPR_Name[command.BRANCH.rs], GPR_Name[command.BRANCH.rt],
				LabelName(RelativeTarget));
		}
		break;
	case R4300i_BNEL:
		if ((command.BRANCH.rs == 0) ^ (command.BRANCH.rt == 0)){
			sprintf(CommandName,"bnezl\t%s, %s", GPR_Name[command.BRANCH.rs == 0 ? command.BRANCH.rt : command.BRANCH.rs ],
				LabelName(RelativeTarget));
		} else {
			sprintf(CommandName,"bnel\t%s, %s, %s", GPR_Name[command.BRANCH.rs], GPR_Name[command.BRANCH.rt],
				LabelName(RelativeTarget));
		}
		break;
	case R4300i_BLEZL:
		sprintf(CommandName,"blezl\t%s, %s",GPR_Name[command.BRANCH.rs], LabelName(RelativeTarget));
		break;
	case R4300i_BGTZL:
		sprintf(CommandName,"bgtzl\t%s, %s",GPR_Name[command.BRANCH.rs], LabelName(RelativeTarget));
		break;
	case R4300i_DADDI:
		sprintf(CommandName,"daddi\t%s, %s, 0x%X",GPR_Name[command.BRANCH.rt], GPR_Name[command.BRANCH.rs],command.IMM.immediate);
		break;
	case R4300i_DADDIU:
		sprintf(CommandName,"daddiu\t%s, %s, 0x%X",GPR_Name[command.BRANCH.rt], GPR_Name[command.BRANCH.rs],command.IMM.immediate);
		break;
	case R4300i_LDL:
		sprintf(CommandName,"ldl\t%s, 0x%X (%s)",GPR_Name[command.BRANCH.rt], command.BRANCH.offset, GPR_Name[command.IMM.base]);
		break;
	case R4300i_LDR:
		sprintf(CommandName,"ldr\t%s, 0x%X (%s)",GPR_Name[command.BRANCH.rt], command.BRANCH.offset, GPR_Name[command.IMM.base]);
		break;
	case R4300i_LB:
		sprintf(CommandName,"lb\t%s, 0x%X (%s)",GPR_Name[command.BRANCH.rt], command.BRANCH.offset, GPR_Name[command.IMM.base]);
		break;
	case R4300i_LH:
		sprintf(CommandName,"lh\t%s, 0x%X (%s)",GPR_Name[command.BRANCH.rt], command.BRANCH.offset, GPR_Name[command.IMM.base]);
		break;
	case R4300i_LWL:
		sprintf(CommandName,"lwl\t%s, 0x%X (%s)",GPR_Name[command.BRANCH.rt], command.BRANCH.offset, GPR_Name[command.IMM.base]);
		break;
	case R4300i_LW:
		sprintf(CommandName,"lw\t%s, 0x%X (%s)",GPR_Name[command.BRANCH.rt], command.BRANCH.offset, GPR_Name[command.IMM.base]);
		break;
	case R4300i_LBU:
		sprintf(CommandName,"lbu\t%s, 0x%X (%s)",GPR_Name[command.BRANCH.rt], command.BRANCH.offset, GPR_Name[command.IMM.base]);
		break;
	case R4300i_LHU:
		sprintf(CommandName,"lhu\t%s, 0x%X (%s)",GPR_Name[command.BRANCH.rt], command.BRANCH.offset, GPR_Name[command.IMM.base]);
		break;
	case R4300i_LWR:
		sprintf(CommandName,"lwr\t%s, 0x%X (%s)",GPR_Name[command.BRANCH.rt], command.BRANCH.offset, GPR_Name[command.IMM.base]);
		break;
	case R4300i_LWU:
		sprintf(CommandName,"lwu\t%s, 0x%X (%s)",GPR_Name[command.BRANCH.rt], command.BRANCH.offset, GPR_Name[command.IMM.base]);
		break;
	case R4300i_SB:
		sprintf(CommandName,"sb\t%s, 0x%X (%s)",GPR_Name[command.BRANCH.rt], command.BRANCH.offset, GPR_Name[command.IMM.base]);
		break;
	case R4300i_SH:
		sprintf(CommandName,"sh\t%s, 0x%X (%s)",GPR_Name[command.BRANCH.rt], command.BRANCH.offset, GPR_Name[command.IMM.base]);
		break;
	case R4300i_SWL:
		sprintf(CommandName,"swl\t%s, 0x%X (%s)",GPR_Name[command.BRANCH.rt], command.BRANCH.offset, GPR_Name[command.IMM.base]);
		break;
	case R4300i_SW:
		sprintf(CommandName,"sw\t%s, 0x%X (%s)",GPR_Name[command.BRANCH.rt], command.BRANCH.offset, GPR_Name[command.IMM.base]);
		break;
	case R4300i_SDL:
		sprintf(CommandName,"sdl\t%s, 0x%X (%s)",GPR_Name[command.BRANCH.rt], command.BRANCH.offset, GPR_Name[command.IMM.base]);
		break;
	case R4300i_SDR:
		sprintf(CommandName,"sdr\t%s, 0x%X (%s)",GPR_Name[command.BRANCH.rt], command.BRANCH.offset, GPR_Name[command.IMM.base]);
		break;
	case R4300i_SWR:
		sprintf(CommandName,"swr\t%s, 0x%X (%s)",GPR_Name[command.BRANCH.rt], command.BRANCH.offset, GPR_Name[command.IMM.base]);
		break;
	case R4300i_CACHE:
		sprintf(CommandName,"cache\t%d, 0x%X (%s)",command.BRANCH.rt, command.BRANCH.offset, GPR_Name[command.IMM.base]);
		break;
	case R4300i_LL:
		sprintf(CommandName,"ll\t%s, 0x%X (%s)",GPR_Name[command.BRANCH.rt], command.BRANCH.offset, GPR_Name[command.IMM.base]);
		break;
	case R4300i_LWC1:
		sprintf(CommandName,"lwc1\t%s, 0x%X (%s)",FPR_Name[command.BRANCH.rt], command.BRANCH.offset, GPR_Name[command.IMM.base]);
		break;
	case R4300i_LLD:
		sprintf(CommandName, "lld\t%s, 0x%X (%s)", GPR_Name[command.BRANCH.rt], command.BRANCH.offset, GPR_Name[command.IMM.base]);
		break;
	case R4300i_LDC1:
		sprintf(CommandName,"ldc1\t%s, 0x%X (%s)",FPR_Name[command.BRANCH.rt], command.BRANCH.offset, GPR_Name[command.IMM.base]);
		break;
	case R4300i_LD:
		sprintf(CommandName,"ld\t%s, 0x%X (%s)",GPR_Name[command.BRANCH.rt], command.BRANCH.offset, GPR_Name[command.IMM.base]);
		break;
	case R4300i_SC:
		sprintf(CommandName,"sc\t%s, 0x%X (%s)",GPR_Name[command.BRANCH.rt], command.BRANCH.offset, GPR_Name[command.IMM.base]);
		break;
	case R4300i_SWC1:
		sprintf(CommandName,"swc1\t%s, 0x%X (%s)",FPR_Name[command.BRANCH.rt], command.BRANCH.offset, GPR_Name[command.IMM.base]);
		break;
	case R4300i_SCD:
		sprintf(CommandName, "scd\t%s, 0x%X (%s)", GPR_Name[command.BRANCH.rt], command.BRANCH.offset, GPR_Name[command.IMM.base]);
		break;
	case R4300i_SDC1:
		sprintf(CommandName,"sdc1\t%s, 0x%X (%s)",FPR_Name[command.BRANCH.rt], command.BRANCH.offset, GPR_Name[command.IMM.base]);
		break;
	case R4300i_SD:
		sprintf(CommandName,"sd\t%s, 0x%X (%s)",GPR_Name[command.BRANCH.rt], command.BRANCH.offset, GPR_Name[command.IMM.base]);
		break;
	default:	
		sprintf(CommandName,"Unknown\t%02X %02X %02X %02X",
			command.Ascii[3],command.Ascii[2],command.Ascii[1],command.Ascii[0]);
	}

	return CommandName;
}

void RefreshR4300iCommands ( void ) {
	DWORD LinesUsed = 1;
	MIPS_DWORD location, max_location;
	char AsciiAddress[20];
	int count;

	if (InR4300iCommandsWindow == FALSE) { return; }

	GetWindowText(hAddress,AsciiAddress,sizeof(AsciiAddress));
	if (strlen(AsciiAddress) <= 8) {
		location.DW = (int)AsciiToHex(AsciiAddress) & ~3;
	} else {
		location.UDW = AsciiToHex64(AsciiAddress) & ~3;
	}

	max_location.UDW = ULLONG_MAX - R4300i_MaxCommandLines * 4 + 1;
	if (location.UDW > max_location.UDW) { location = max_location; }
	for (count = 0 ; count < R4300i_MaxCommandLines; count += LinesUsed ){
		LinesUsed = DisplayR4300iCommand(location, count);
		location.UDW += 4;
	}
}

void SetR4300iCommandToRunning ( void ) { 
	//if (HasR4300iBPoint(PROGRAM_COUNTER)) {
		StepOpcode();
	//}
	SetCoreToRunning();
	if (InR4300iCommandsWindow == FALSE) { return; }
	EnableWindow(hGoButton,    FALSE);
	EnableWindow(hBreakButton, TRUE);
	EnableWindow(hStepButton,  FALSE);
	EnableWindow(hSkipButton,  FALSE);
	SendMessage(hGoButton, BM_SETSTYLE,BS_PUSHBUTTON,TRUE);
	SendMessage(hBreakButton, BM_SETSTYLE,BS_DEFPUSHBUTTON,TRUE);
	SetFocus(hBreakButton);
}

void SetR4300iCommandToStepping ( void ) { 
	EnableWindow(hGoButton,    TRUE);
	EnableWindow(hBreakButton, FALSE);
	EnableWindow(hStepButton,  TRUE);
	EnableWindow(hSkipButton,  TRUE);
	SendMessage(hBreakButton, BM_SETSTYLE, BS_PUSHBUTTON,TRUE);
	SendMessage(hStepButton, BM_SETSTYLE, BS_DEFPUSHBUTTON,TRUE);
	SetFocus(hStepButton);
	SetCoreToStepping();
}

void SetR4300iCommandViewto ( MIPS_DWORD NewLocation ) {
	QWORD location;
	char Value[20];

	if (InR4300iCommandsWindow == FALSE) { return; }

	GetWindowText(hAddress,Value,sizeof(Value));
	if (strlen(Value) <= 8) {
		location = (int)AsciiToHex(Value) & ~3;
	} else {
		location = AsciiToHex64(Value) & ~3;
	}

	if ( NewLocation.UDW < location || NewLocation.UDW >= location + R4300i_MaxCommandLines * 4 ) {
		sprintf(Value,"%016llX",NewLocation.UDW);
		SetWindowText(hAddress,Value);
	} else {
		RefreshR4300iCommands();
	}
}

void Update_r4300iCommandList (void) {
	if (!InR4300iCommandsWindow) { return; }
	
	if (NoOfMapEntries == 0) {
		ShowWindow(hFunctionlist, FALSE);
		SetWindowPos(hGoButton,0,625,56,0,0, SWP_NOZORDER | SWP_NOSIZE| SWP_SHOWWINDOW);
		SetWindowPos(hBreakButton,0,625,85,0,0, SWP_NOZORDER | SWP_NOSIZE| SWP_SHOWWINDOW);
		SetWindowPos(hStepButton,0,625,114,0,0, SWP_NOZORDER | SWP_NOSIZE| SWP_SHOWWINDOW);
		SetWindowPos(hSkipButton,0,625,143,0,0, SWP_NOZORDER | SWP_NOSIZE| SWP_SHOWWINDOW);
	} else {	
		DWORD count, pos;

		ShowWindow(hFunctionlist, TRUE);
		SetWindowPos(hGoButton,0,625,86,0,0, SWP_NOZORDER | SWP_NOSIZE| SWP_SHOWWINDOW);
		SetWindowPos(hBreakButton,0,625,115,0,0, SWP_NOZORDER | SWP_NOSIZE| SWP_SHOWWINDOW);
		SetWindowPos(hStepButton,0,625,144,0,0, SWP_NOZORDER | SWP_NOSIZE| SWP_SHOWWINDOW);
		SetWindowPos(hSkipButton,0,625,173,0,0, SWP_NOZORDER | SWP_NOSIZE| SWP_SHOWWINDOW);
		
		SendMessage(hFunctionlist,CB_RESETCONTENT,(WPARAM)0,(LPARAM)0);		
		for (count = 0; count < NoOfMapEntries; count ++ ) {
			pos = SendMessage(hFunctionlist,CB_ADDSTRING,(WPARAM)0,(LPARAM)MapTable[count].Label);
			SendMessage(hFunctionlist,CB_SETITEMDATA,(WPARAM)pos,(LPARAM)&MapTable[count].VAddr.UDW);
		}
		SendMessage(hFunctionlist,CB_SETCURSEL,(WPARAM)-1,(LPARAM)0);
		
		InvalidateRect( R4300i_Commands_hDlg, NULL, TRUE );
	}
}
