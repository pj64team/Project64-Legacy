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
#include "CPU.h"
#include "debugger.h"
#include "plugin.h"

#define IDC_LIST				100
#define IDC_TAB_CONTROL			101
#define IDC_REMOVE_BUTTON		103
#define IDC_REMOVEALL_BUTTON	104
#define IDC_LOCATION_EDIT		105
#define IDC_FUNCTION_COMBO		106
#define IDC_TYPE_EXEC			107
#define IDC_TYPE_READ			108
#define IDC_TYPE_WRITE			109
#define IDC_TYPE_READ_WRITE		110

void BPoint_AddButtonPressed ( void);
void __cdecl Create_BPoint_Window    ( int );
void DrawBPItem              ( LPARAM );
void HideBPointPanel         ( int Panel);
void Paint_BPoint_Win        ( HWND );
void __cdecl RefreshBreakPoints      (void);
void Setup_BPoint_Win        ( HWND );
void ShowBPointPanel         ( int Panel);
LRESULT CALLBACK BPoint_Proc   ( HWND, UINT, WPARAM, LPARAM );
LRESULT CALLBACK RefreshBPProc ( HWND, UINT, WPARAM, LPARAM );

static HWND BPoint_Win_hDlg, hTab, hList, hStatic, hR4300iLocation, hFunctionlist,
    hAddButton, hRemoveButton, hRemoveAllButton,
	hTypeExec, hTypeRead, hTypeWrite, hTypeReadWrite;
static BOOL InBPWindow = FALSE;
static FARPROC RefProc;
int RSPBP_count;

int Add_R4300iBPoint( MIPS_DWORD Location ) {
	int count;

	if (NoOfBpoints == MaxBPoints) {
		DisplayError("Max amount of Break Points set");
		return FALSE;
	}

	for (count = 0; count < NoOfBpoints; count ++) {
		if (BPoint[count].Location.UDW == Location.UDW) {
			DisplayError("You already have this Break Point");
			return FALSE;
		}
	}

	BPoint[NoOfBpoints].enabled = TRUE;
	BPoint[NoOfBpoints].Location = Location;
	NoOfBpoints += 1;
	RefreshBreakPoints();
	/*if (CPU_Action.Stepping || hMipsCPU == NULL) {
		ClearAllx86Code();
	} else {
		CPU_Action.ResetX86Code = TRUE;
		CPU_Action.do_or_check_something += 1;
	}*/
	return TRUE;
}

void BPoint_AddButtonPressed (void) {
	DWORD Selected;
	MIPS_DWORD Location;
	char Address[18];
	TC_ITEM item;

	item.mask = TCIF_PARAM;
	TabCtrl_GetItem( hTab, TabCtrl_GetCurSel( hTab ), &item );
	switch( item.lParam ) {
	case R4300i_BP:
	{
		int BreakType = 0;
		if (SendMessage(hTypeRead, BM_GETCHECK, 0, 0) == BST_CHECKED) {
			BreakType = 1;
		} else if (SendMessage(hTypeWrite, BM_GETCHECK, 0, 0) == BST_CHECKED) {
			BreakType = 2;
		} else if (SendMessage(hTypeReadWrite, BM_GETCHECK, 0, 0) == BST_CHECKED) {
			BreakType = 3;
		}

		GetWindowText(hR4300iLocation, Address, sizeof(Address));
		if (strlen(Address) <= 8) {
			Location.DW = (int)AsciiToHex(Address);
		} else {
			Location.UDW = AsciiToHex64(Address);
		}
		if (BreakType == 0) {
			if (!Add_R4300iBPoint(Location)) {
				SendMessage(hR4300iLocation, EM_SETSEL, (WPARAM)0, (LPARAM)-1);
				SetFocus(hR4300iLocation);
			}
		} else {
			AddWatchPoint(Location, BreakType);
			RefreshBreakPoints();
		}
		break;
	}
	case R4300i_FUNCTION:
		Selected = SendMessage(hFunctionlist,CB_GETCURSEL,0,0);
		QWORD* FunctionAddress = (QWORD*)SendMessage(hFunctionlist,CB_GETITEMDATA,(WPARAM)Selected,0);
		Location.UDW = *FunctionAddress;
		Add_R4300iBPoint(Location);
		SetFocus(hFunctionlist);
		break;
	case RSP_BP:
		if (RspDebug.UseBPoints) { RspDebug.Add_BPoint(); }
		break;
	}

}

LRESULT CALLBACK BPoint_Proc (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	static RECT rcDisp;
	static int CurrentPanel = R4300i_BP;
	int selected;
	TC_ITEM item;

	switch (uMsg) {
	case WM_INITDIALOG:
		BPoint_Win_hDlg = hDlg;
		Setup_BPoint_Win( hDlg );
		RefreshBreakPoints();
		break;
	case WM_MOVE:
		StoreCurrentWinPos("Break Point",hDlg);
		break;
	case WM_SIZE:
		GetClientRect( hDlg, &rcDisp);
		TabCtrl_AdjustRect( hTab, FALSE, &rcDisp );
		break;
	case WM_PAINT:
		Paint_BPoint_Win( hDlg );
		return TRUE;
	case WM_NOTIFY:
		switch (((NMHDR *)lParam)->code) {
		case TCN_SELCHANGE:
			InvalidateRect( hTab, &rcDisp, TRUE );
			HideBPointPanel (CurrentPanel);
			item.mask = TCIF_PARAM;
			TabCtrl_GetItem( hTab, TabCtrl_GetCurSel( hTab ), &item );
			CurrentPanel = item.lParam;
			InvalidateRect( hStatic, NULL, FALSE );
			ShowBPointPanel ( CurrentPanel );
			break;
		}
		break;
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDOK:
			BPoint_AddButtonPressed();
			break;
		case IDC_REMOVE_BUTTON:
			selected = SendMessage(hList,LB_GETCURSEL,0,0);
			if (selected < NoOfBpoints) {
				QWORD* location = (QWORD*)SendMessage(hList,LB_GETITEMDATA,selected,0);
				MIPS_DWORD BPointAddress;
				BPointAddress.UDW = *location;
				RemoveR4300iBreakPoint(BPointAddress);
				break;
			}
			if (selected - NoOfBpoints < CountWatchPoints()) {
				QWORD* location = (QWORD*)SendMessage(hList, LB_GETITEMDATA, selected, 0);
				MIPS_DWORD BPointAddress;
				BPointAddress.UDW = *location;
				RemoveWatchPoint(BPointAddress);
				RefreshBreakPoints();
				break;
			}
			if (selected - NoOfBpoints - CountWatchPoints() < RSPBP_count) {
				RspDebug.RemoveBpoint(hList,SendMessage(hList,LB_GETCURSEL,0,0));
				break;
			}
			DisplayError("what is this BP");
			break;
		case IDC_REMOVEALL_BUTTON:
			NoOfBpoints = 0;
			RemoveAllWatchPoints();
			if (RspDebug.UseBPoints) { RspDebug.RemoveAllBpoint(); }
			RefreshBreakPoints();
			RefreshR4300iCommands();
			break;
		case IDCANCEL:
			CurrentPanel = R4300i_BP;
			EndDialog( hDlg, IDCANCEL );
			break;
		case IDC_LIST:
			if (HIWORD(wParam) == LBN_DBLCLK) {
				selected = SendMessage(hList, LB_GETCURSEL, 0, 0);
				QWORD* location = (QWORD*)SendMessage(hList, LB_GETITEMDATA, selected, 0);
				MIPS_DWORD BPointAddress;
				BPointAddress.UDW = *location;
				if (selected < NoOfBpoints) {
					ToggleR4300iBPoint(BPointAddress);
				} else if (selected - NoOfBpoints < CountWatchPoints()) {
					ToggleWatchPoint(BPointAddress);
				} else {
					// TODO: Support toggling RSP breakpoints
					DisplayError("what is this BP");
				}
				RefreshBreakPoints();
				SendMessage(hList, LB_SETCURSEL, selected, 0);
			}
			break;
		}
		break;
	default:
		return FALSE;
	}
	return TRUE;
}

int CheckForR4300iBPoint ( MIPS_DWORD Location ) {
	int count;

	for (count = 0; count < NoOfBpoints; count ++){
		if (BPoint[count].enabled && BPoint[count].Location.UDW == Location.UDW) {
			return TRUE;
		}
	}
	return FALSE;
}

int HasR4300iBPoint(MIPS_DWORD Location) {
	for (int count = 0; count < NoOfBpoints; count++) {
		if (BPoint[count].Location.UDW == Location.UDW) {
			return TRUE;
		}
	}
	return FALSE;
}

void ToggleR4300iBPoint(MIPS_DWORD Location) {
	for (int count = 0; count < NoOfBpoints; count++) {
		if (BPoint[count].Location.UDW == Location.UDW) {
			BPoint[count].enabled = !BPoint[count].enabled;
			return;
		}
	}
}

void __cdecl Create_BPoint_Window (int Child) {
	DWORD ThreadID;

	if (Child) {
		InBPWindow = TRUE;
		DialogBox( hInst, "BLANK", NULL,(DLGPROC) BPoint_Proc );
		InBPWindow = FALSE;
	} else {
		if (!InBPWindow) {
			CloseHandle(CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)Create_BPoint_Window,
				(LPVOID)TRUE,0, &ThreadID));
		} else {
			BringWindowToTop(BPoint_Win_hDlg);
		}
	}
}

void __cdecl Enter_BPoint_Window ( void ) {
    Create_BPoint_Window ( FALSE );
}

void HideBPointPanel ( int Panel) {
	switch( Panel ) {
	case R4300i_BP:
		ShowWindow(hR4300iLocation, FALSE);
		ShowWindow(hTypeExec, FALSE);
		ShowWindow(hTypeRead, FALSE);
		ShowWindow(hTypeWrite, FALSE);
		ShowWindow(hTypeReadWrite, FALSE);
		break;
	case R4300i_FUNCTION:
		ShowWindow(hFunctionlist, FALSE);
		break;
	case RSP_BP:
		if (RspDebug.UseBPoints) { RspDebug.HideBPPanel(); }
		break;
	}
}

void Paint_BPoint_Win (HWND hDlg) {
	RECT rcBox;
	PAINTSTRUCT ps;
	HFONT hOldFont;
	int OldBkMode;

	BeginPaint( hDlg, &ps );
	rcBox.left   = 5;
	rcBox.top    = 175;
	rcBox.right  = 255;
	rcBox.bottom = 290;
	DrawEdge( ps.hdc, &rcBox, EDGE_RAISED, BF_RECT );
	rcBox.left   = 8;
	rcBox.top    = 178;
	rcBox.right  = 252;
	rcBox.bottom = 287;
	DrawEdge( ps.hdc, &rcBox, EDGE_ETCHED, BF_RECT );

	hOldFont = SelectObject(ps.hdc,GetStockObject(DEFAULT_GUI_FONT));
	OldBkMode = SetBkMode( ps.hdc, TRANSPARENT );

	TextOut( ps.hdc, 9,159,"Breakpoints: ",13);

	SelectObject( ps.hdc,hOldFont );
	SetBkMode( ps.hdc, OldBkMode );

	EndPaint( hDlg, &ps );
}

void __cdecl RefreshBreakPoints (void) {
	char Message[100];
	int count;

	if (!InBPWindow) { return; }

	SendMessage(hList,LB_RESETCONTENT,0,0);
	for (count = 0; count < NoOfBpoints; count ++ ) {
		char flags[5] = "-x--";
		if (BPoint[count].enabled) {
			flags[0] = 'e';
		}

		sprintf(Message," at 0x%016llX (r4300i %s)", BPoint[count].Location.UDW, flags);
		SendMessage(hList,LB_ADDSTRING,0,(LPARAM)Message);
		SendMessage(hList,LB_SETITEMDATA,count,(LPARAM)&BPoint[count].Location.UDW);
	}
	RefreshWatchPoints(hList);
	count = SendMessage(hList,LB_GETCOUNT,0,0);
	if (RspDebug.UseBPoints) { RspDebug.RefreshBpoints(hList); }
	RSPBP_count = SendMessage(hList,LB_GETCOUNT,0,0) - count;

	if (SendMessage(hList,LB_GETCOUNT,0,0) > 0) {
		EnableWindow( hRemoveButton, TRUE );
		EnableWindow( hRemoveAllButton, TRUE );
	}
}

LRESULT CALLBACK RefreshBPProc( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam ) {
	PAINTSTRUCT ps;
	RECT rcBox;
	HFONT hOldFont;
	int OldBkMode;
	TC_ITEM item;

	switch( uMsg ) {
	case WM_PAINT:
		BeginPaint( hWnd, &ps );
		rcBox.left   = 15;
		rcBox.top    = 40;
		rcBox.right  = 235;
		rcBox.bottom = 125;
		DrawEdge( ps.hdc, &rcBox, EDGE_ETCHED, BF_RECT );

		hOldFont = SelectObject( ps.hdc,
			GetStockObject(DEFAULT_GUI_FONT) );
		OldBkMode = SetBkMode( ps.hdc, TRANSPARENT );

		item.mask = TCIF_PARAM;
		TabCtrl_GetItem( hTab, TabCtrl_GetCurSel( hTab ), &item );
		switch (item.lParam) {
		case R4300i_BP:
			TextOut( ps.hdc, 29,60,"Virtual Address:",16);
			break;
		case R4300i_FUNCTION:
			TextOut( ps.hdc, 75,60,"Break on label:",15);
			break;
		case RSP_BP:
			if (RspDebug.UseBPoints) { RspDebug.PaintBPPanel(ps); }
			break;
		}

		SelectObject( ps.hdc,hOldFont );
		SetBkMode( ps.hdc, OldBkMode );

		EndPaint( hWnd, &ps );
		break;
	//default:
		//return( (*RefProc)(hWnd, uMsg, wParam, lParam) );
	}
	return( FALSE );
}

void RemoveR4300iBreakPoint (MIPS_DWORD Location) {
	int count, location = -1;

	for (count = 0; count < NoOfBpoints; count ++){
		if (BPoint[count].Location.UDW == Location.UDW) {
			location = count;
			count = NoOfBpoints;
		}
	}

	if (location >= 0) {
		for (count = location; count < NoOfBpoints - 1; count ++ ){
			BPoint[count].enabled = BPoint[count + 1].enabled;
			BPoint[count].Location = BPoint[count + 1].Location;
		}
		NoOfBpoints -= 1;
		RefreshBreakPoints ();
		RefreshR4300iCommands();
	}

	/*if (CPU_Action.Stepping || hMipsCPU == NULL) {
		ClearAllx86Code();
	} else {
		CPU_Action.ResetX86Code = TRUE;
		CPU_Action.do_or_check_something += 1;
	}*/
}

void Setup_BPoint_Win (HWND hDlg) {
#define WindowWidth  380
#define WindowHeight 350
	DWORD X, Y;

	hAddButton = CreateWindowEx(0,"BUTTON","&Add",
		BS_PUSHBUTTON | WS_CHILD | WS_VISIBLE | WS_TABSTOP,
		262,26,75,22,hDlg,(HMENU)IDOK,
		hInst,NULL );
	if ( hAddButton ) {
		SendMessage(hAddButton,WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT),0);
	}

	hRemoveButton = CreateWindowEx(0,"BUTTON","&Remove",
		WS_CHILD | BS_PUSHBUTTON | WS_VISIBLE | WS_TABSTOP |
		WS_DISABLED,262,177,75,22,hDlg,(HMENU)IDC_REMOVE_BUTTON,
		hInst,NULL );
	if ( hRemoveButton ) {
		SendMessage(hRemoveButton,WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT),0);
	}

	hRemoveAllButton = CreateWindowEx(0,"BUTTON","Remove A&ll", WS_CHILD |
		BS_PUSHBUTTON | WS_VISIBLE | WS_TABSTOP | WS_DISABLED,262,202,75,22,hDlg,
		(HMENU)IDC_REMOVEALL_BUTTON,hInst,NULL );
	if ( hRemoveAllButton ) {
		SendMessage(hRemoveAllButton,WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT),0);
	}

	hTab = CreateWindowEx(0,WC_TABCONTROL,"", WS_CHILD | WS_TABSTOP | WS_VISIBLE,
		5,6,250,150,hDlg,(HMENU)IDC_TAB_CONTROL,hInst,NULL );
	if (hTab) {
		TC_ITEM item;

		SendMessage(hTab, WM_SETFONT, (WPARAM)GetStockObject( DEFAULT_GUI_FONT ), 0 );
		item.mask    = TCIF_TEXT | TCIF_PARAM;
		item.pszText = " R4300i ";
		item.lParam  = R4300i_BP;
		TabCtrl_InsertItem( hTab,0, &item);
		if (NoOfMapEntries != 0) {
			item.mask    = TCIF_TEXT | TCIF_PARAM;
			item.pszText = " Function ";
			item.lParam  = R4300i_FUNCTION;
			TabCtrl_InsertItem( hTab,1, &item);
		}
		if (RspDebug.UseBPoints) {
			RECT rcBox;
			rcBox.left   = 15;  rcBox.top    = 40;
			rcBox.right  = 235; rcBox.bottom = 125;
			item.mask    = TCIF_TEXT | TCIF_PARAM;
			item.pszText = RspDebug.BPPanelName;
			item.lParam  = RSP_BP;
			TabCtrl_InsertItem( hTab,2, &item);
			RspDebug.CreateBPPanel(hDlg,rcBox);
		}
	}

	hR4300iLocation = CreateWindowEx(0,"EDIT","", WS_CHILD | WS_VISIBLE | WS_BORDER |
		ES_UPPERCASE | WS_TABSTOP,115,65,120,17,hDlg,(HMENU)IDC_LOCATION_EDIT,hInst,NULL);
	if (hR4300iLocation) {
		char Address[20];
		SendMessage(hR4300iLocation,WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT),0);
		SendMessage(hR4300iLocation,EM_SETLIMITTEXT,(WPARAM)16,(LPARAM)0);
		sprintf(Address,"%016llX",PROGRAM_COUNTER.UDW);
		SetWindowText(hR4300iLocation, Address);
	}

	hTypeExec = CreateWindowEx(0, "BUTTON", "Exec", WS_CHILD | WS_VISIBLE | WS_TABSTOP |
		BS_AUTORADIOBUTTON, 40, 85, 45, 21, hDlg, (HMENU)IDC_TYPE_EXEC, hInst, NULL);
	SendMessage(hTypeExec, BM_SETCHECK, BST_CHECKED, 0);
	SendMessage(hTypeExec, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), 0);

	hTypeRead = CreateWindowEx(0, "BUTTON", "Read", WS_CHILD | WS_VISIBLE |
		BS_AUTORADIOBUTTON, 40, 106, 50, 21, hDlg, (HMENU)IDC_TYPE_READ, hInst, NULL);
	SendMessage(hTypeRead, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), 0);

	hTypeWrite = CreateWindowEx(0, "BUTTON", "Write", WS_CHILD | WS_VISIBLE |
		BS_AUTORADIOBUTTON, 90, 106, 50, 21, hDlg, (HMENU)IDC_TYPE_WRITE, hInst, NULL);
	SendMessage(hTypeWrite, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), 0);

	hTypeReadWrite = CreateWindowEx(0, "BUTTON", "Read/Write", WS_CHILD | WS_VISIBLE |
		BS_AUTORADIOBUTTON, 140, 106, 80, 21, hDlg, (HMENU)IDC_TYPE_READ_WRITE, hInst, NULL);
	SendMessage(hTypeReadWrite, WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), 0);

	hFunctionlist = CreateWindowEx(0,"COMBOBOX","", WS_CHILD | WS_VSCROLL |
		CBS_DROPDOWNLIST | CBS_SORT | WS_TABSTOP,55,90,150,150,hDlg,
		(HMENU)IDC_FUNCTION_COMBO,hInst,NULL);
	if (hFunctionlist) {
		DWORD count, pos;

		SendMessage(hFunctionlist,WM_SETFONT,(WPARAM)GetStockObject(DEFAULT_GUI_FONT),0);
		for (count = 0; count < NoOfMapEntries; count ++ ) {
			pos = SendMessage(hFunctionlist,CB_ADDSTRING,(WPARAM)0,(LPARAM)MapTable[count].Label);
			SendMessage(hFunctionlist,CB_SETITEMDATA,(WPARAM)pos,(LPARAM)&MapTable[count].VAddr.UDW);
		}
		SendMessage(hFunctionlist,CB_SETCURSEL,(WPARAM)1,(LPARAM)0);
	}

	hList = CreateWindowEx(WS_EX_STATICEDGE, "LISTBOX","", WS_CHILD | WS_VISIBLE | LBS_DISABLENOSCROLL | LBS_NOTIFY | WS_VSCROLL,
		16,187,228,112, hDlg, (HMENU)IDC_LIST, hInst,NULL );
	if ( hList) {
		SendMessage(hList,WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT),0);
	}

	hStatic = CreateWindowEx(0, "STATIC", "", WS_CHILD | WS_VISIBLE, 5, 6, 250, 150, hDlg, 0, hInst, NULL);
	RefProc = (FARPROC)SetWindowLong(hStatic, GWL_WNDPROC, (long)RefreshBPProc);

	SetWindowText(hDlg," Breakpoints");

	if ( !GetStoredWinPos( "Break Point", &X, &Y ) ) {
		X = (GetSystemMetrics( SM_CXSCREEN ) - WindowWidth) / 2;
		Y = (GetSystemMetrics( SM_CYSCREEN ) - WindowHeight) / 2;
	}
	SetWindowPos(hDlg,NULL,X,Y,WindowWidth,WindowHeight, SWP_NOZORDER | SWP_SHOWWINDOW);
}

void ShowBPointPanel ( int Panel) {
	switch( Panel ) {
	case R4300i_BP:
		ShowWindow(hR4300iLocation, TRUE);
		ShowWindow(hTypeExec, TRUE);
		ShowWindow(hTypeRead, TRUE);
		ShowWindow(hTypeWrite, TRUE);
		ShowWindow(hTypeReadWrite, TRUE);
		break;
	case R4300i_FUNCTION:
		ShowWindow(hFunctionlist, TRUE);
		break;
	case RSP_BP:
		if (RspDebug.UseBPoints) { RspDebug.ShowBPPanel(); }
		break;
	}
}

void UpdateBPointGUI (void) {
	TC_ITEM item;
	DWORD count;

	if (!InBPWindow) { return; }

	if (TabCtrl_GetCurSel(hTab) != 0) {
		InvalidateRect( hTab, NULL, TRUE );
		item.mask = TCIF_PARAM;
		TabCtrl_GetItem( hTab, TabCtrl_GetCurSel( hTab ), &item );
		HideBPointPanel (item.lParam);
		TabCtrl_SetCurSel(hTab, 0);
		TabCtrl_GetItem( hTab, TabCtrl_GetCurSel( hTab ), &item );
		InvalidateRect( hStatic, NULL, FALSE );
		ShowBPointPanel ( item.lParam );
	}

	for (count = TabCtrl_GetItemCount(hTab); count > 1; count--) {
		TabCtrl_DeleteItem(hTab, count - 1);
	}

	if (NoOfMapEntries > 0) {
		item.mask    = TCIF_TEXT | TCIF_PARAM;
		item.pszText = " Function ";
		item.lParam  = R4300i_FUNCTION;
		TabCtrl_InsertItem( hTab,1, &item);
	}
	if (RspDebug.UseBPoints) {
		RECT rcBox;
		rcBox.left   = 15;  rcBox.top    = 40;
		rcBox.right  = 235; rcBox.bottom = 125;
		item.mask    = TCIF_TEXT | TCIF_PARAM;
		item.pszText = RspDebug.BPPanelName;
		item.lParam  = RSP_BP;
		TabCtrl_InsertItem( hTab,2, &item);
		RspDebug.CreateBPPanel(BPoint_Win_hDlg, rcBox);
	}
	InvalidateRect( BPoint_Win_hDlg, NULL, TRUE );
	RefreshBreakPoints ();
}

void UpdateBP_FunctionList (void) {
	DWORD pos, count;

	if (!InBPWindow) { return; }

	SendMessage(hFunctionlist,CB_RESETCONTENT,(WPARAM)0,(LPARAM)0);
	for (count = 0; count < NoOfMapEntries; count ++ ) {
		pos = SendMessage(hFunctionlist,CB_ADDSTRING,(WPARAM)0,(LPARAM)MapTable[count].Label);
		SendMessage(hFunctionlist,CB_SETITEMDATA,(WPARAM)pos,(LPARAM)&MapTable[count].VAddr.UDW);
	}
	SendMessage(hFunctionlist,CB_SETCURSEL,(WPARAM)1,(LPARAM)0);
	UpdateBPointGUI();
}
