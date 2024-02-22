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
#include "r4300i Registers.h"

#define GeneralPurpose			1
#define ControlProcessor0		2
#define FloatingRegisters 		3 
#define SpecialRegister	 		4 
#define RDRAMRegisters	 		5
#define RDRAMRegisters2         6
#define ExpRDRAMRegisters       7
#define ExpRDRAMRegisters2      8
#define SPRegisters				9
#define MIPSInterface	 		10
#define VideoInterface 		  	11
#define AudioInterface		  	12
#define PeripheralInterface 	13
#define RDRAMInterface	 		14
#define SerialInterface			15
#define COP1SRegisters			16
#define COP1DRegisters			17
#define COP1WRegisters			18
#define COP1LRegisters			19

#define IDC_TAB_CONTROL			1000

void __cdecl Create_R4300i_Register_Window     ( int );
void PaintR4300iAIPanel                ( HWND );
void PaintR4300iCP0Panel               ( HWND );
void PaintR4300iFPRPanel               ( HWND );
void PaintR4300iCOP1SPanel             ( HWND );
void PaintR4300iCOP1DPanel             ( HWND );
void PaintR4300iCOP1WPanel             ( HWND );
void PaintR4300iCOP1LPanel             ( HWND );
void PaintR4300iGPRPanel               ( HWND );
void PaintR4300iMIPanel                ( HWND );
void PaintR4300iRDRamPanel             ( HWND );
void PaintR4300iPIPanel                ( HWND );
void PaintR4300iRIPanel                ( HWND );
void PaintR4300iSIPanel                ( HWND );
void PaintR4300iSPPanel                ( HWND );
void PaintR4300iSpecialPanel           ( HWND );
void PaintR4300iMIPanel                ( HWND );
void PaintR4300iVIPanel                ( HWND );
void PaintR4300iRIPanel                ( HWND );

void SetupR4300iAIPanel                ( HWND );
void SetupR4300iCP0Panel               ( HWND );
void SetupR4300iFPRPanel               ( HWND );
void SetupR4300iCOP1SPanel             ( HWND );
void SetupR4300iCOP1DPanel             ( HWND );
void SetupR4300iCOP1WPanel             ( HWND );
void SetupR4300iCOP1LPanel             ( HWND );
void SetupR4300iGPRPanel               ( HWND );
void SetupR4300iMIPanel                ( HWND );
void SetupR4300iRDRamPanel             ( HWND );
void SetupR4300iRDRam2Panel            ( HWND );
void SetupR4300iExpRDRamPanel          ( HWND );
void SetupR4300iExpRDRam2Panel         ( HWND );
void SetupR4300iPIPanel                ( HWND );
void SetupR4300iRIPanel                ( HWND );
void SetupR4300iSIPanel                ( HWND );
void SetupR4300iSPPanel                ( HWND );
void SetupR4300iSpecialPanel           ( HWND );
void SetupR4300iVIPanel                ( HWND );
void SetupR4300iRegistersMain          ( HWND );
void ShowR4300iRegisterPanel           ( int );

LRESULT CALLBACK RefreshR4300iRegProc  ( HWND, UINT, WPARAM, LPARAM );
LRESULT CALLBACK R4300i_Registers_Proc ( HWND, UINT, WPARAM, LPARAM );

HWND R4300i_Registers_hDlg, hTab, hStatic, hGPR[32], hCP0[32], hFPR[32], hCOP1S[32], hCOP1D[32], hCOP1W[32], hCOP1L[32],
	hSpecial[6], hRDRam[10], hRDRam2[10], hExpRDRam[10], hExpRDRam2[10], hSP[10], hMI[4], hVI[14], hAI[6], hPI[13], hRI[8], hSI[4];
int InR4300iRegisterWindow = FALSE;
FARPROC r4300iRegRefreshProc;

void __cdecl Create_R4300i_Register_Window ( int Child ) {
	DWORD ThreadID;
	
	if ( Child ) {
		InR4300iRegisterWindow = TRUE;
		DialogBox( hInst, "BLANK", NULL,(DLGPROC) R4300i_Registers_Proc );
		InR4300iRegisterWindow = FALSE;
	} else {
		if (!InR4300iRegisterWindow) {
			CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)Create_R4300i_Register_Window,
				(LPVOID)TRUE,0, &ThreadID);	
		} else {
			SetForegroundWindow(R4300i_Registers_hDlg);
		}	
	}
}

void __cdecl Enter_R4300i_Register_Window ( void ) {
	if (!HaveDebugger) { return; }
    Create_R4300i_Register_Window ( FALSE );
}

void HideR4300iRegisterPanel ( int Panel) {
	int count;

	switch( Panel ) {
	case GeneralPurpose:
		for (count = 0; count < 32;count ++) { ShowWindow(hGPR[count],FALSE); }
		break;
	case ControlProcessor0:
		for (count = 0; count < 32;count ++) { ShowWindow(hCP0[count],FALSE); }
		break;
	case FloatingRegisters:
		for (count = 0; count < 32;count ++) { ShowWindow(hFPR[count],FALSE); }
		break;
	case COP1SRegisters:
		for (count = 0; count < 32; count++) { ShowWindow(hCOP1S[count], FALSE); }
		break;
	case COP1DRegisters:
		for (count = 0; count < 32; count++) { ShowWindow(hCOP1D[count], FALSE); }
		break;
	case COP1WRegisters:
		for (count = 0; count < 32; count++) { ShowWindow(hCOP1W[count], FALSE); }
		break;
	case COP1LRegisters:
		for (count = 0; count < 32; count++) { ShowWindow(hCOP1L[count], FALSE); }
		break;
	case SpecialRegister:
		for (count = 0; count < 6;count ++) { ShowWindow(hSpecial[count],FALSE); }
		break;
	case RDRAMRegisters:
		for (count = 0; count < 10;count ++) { ShowWindow(hRDRam[count],FALSE); }
		break;
	case RDRAMRegisters2:
		for (count = 0; count < 10; count++) { ShowWindow(hRDRam2[count], FALSE); }
		break;
	case ExpRDRAMRegisters:
		for (count = 0; count < 10; count++) { ShowWindow(hExpRDRam[count], FALSE); }
		break;
	case ExpRDRAMRegisters2:
		for (count = 0; count < 10; count++) { ShowWindow(hExpRDRam2[count], FALSE); }
		break;
	case SPRegisters:
		for (count = 0; count < 10;count ++) { ShowWindow(hSP[count],FALSE); }
		break;
	case MIPSInterface:
		for (count = 0; count < 4;count ++) { ShowWindow(hMI[count],FALSE); }
		break;
	case VideoInterface:
		for (count = 0; count < 14;count ++) { ShowWindow(hVI[count],FALSE); }
		break;
	case AudioInterface:
		for (count = 0; count < 6;count ++) { ShowWindow(hAI[count],FALSE); }
		break;
	case PeripheralInterface:
		for (count = 0; count < 13;count ++) { ShowWindow(hPI[count],FALSE); }
		break;
	case RDRAMInterface:
		for (count = 0; count < 13;count ++) { ShowWindow(hRI[count],FALSE); }
		break;
	case SerialInterface:
		for (count = 0; count < 4;count ++) { ShowWindow(hSI[count],FALSE); }
		break;
	}
}

void PaintR4300iAIPanel (HWND hWnd) {	
	PAINTSTRUCT ps;
	RECT rcBox;
	HFONT hOldFont;
	int OldBkMode;
	BeginPaint( hWnd, &ps );
	
	rcBox.left   = 184;
	rcBox.top    = 94;
	rcBox.right  = 450;
	rcBox.bottom = 300;
	DrawEdge( ps.hdc, &rcBox, EDGE_ETCHED, BF_RECT );

	hOldFont = SelectObject( ps.hdc,
	GetStockObject(DEFAULT_GUI_FONT) );
	OldBkMode = SetBkMode( ps.hdc, TRANSPARENT );

	TextOut( ps.hdc, 207,116,"AI_DRAM_ADDR_REG:",17);
	TextOut( ps.hdc, 207,146,"AI_LEN_REG:",11);
	TextOut( ps.hdc, 207,176,"AI_CONTROL_REG:",15);
	TextOut( ps.hdc, 207,206,"AI_STATUS_REG:",14);
	TextOut( ps.hdc, 207,236,"AI_DACRATE_REG:",15);
	TextOut( ps.hdc, 207,266,"AI_BITRATE_REG:",15);
		
	SelectObject( ps.hdc,hOldFont );
	SetBkMode( ps.hdc, OldBkMode );
	EndPaint( hWnd, &ps );
}

void PaintR4300iCP0Panel (HWND hWnd) {	
	PAINTSTRUCT ps;
	RECT rcBox;
	HFONT hOldFont;
	int OldBkMode;
	BeginPaint( hWnd, &ps );
	
	rcBox.left   = 10;
	rcBox.top    = 84;
	rcBox.right  = 650;
	rcBox.bottom = 320;
	DrawEdge( ps.hdc, &rcBox, EDGE_ETCHED, BF_RECT );

	hOldFont = SelectObject( ps.hdc,
		GetStockObject(DEFAULT_GUI_FONT) );
	OldBkMode = SetBkMode( ps.hdc, TRANSPARENT );

	TextOut( ps.hdc, 20,99,"Index:",6);
	TextOut( ps.hdc, 20,123,"Random:",7);
	TextOut( ps.hdc, 20,147,"EntryLo0:",9);
	TextOut( ps.hdc, 20,171,"EntryLo1:",9);
	TextOut( ps.hdc, 20,195,"Context:",8);
	TextOut( ps.hdc, 20,219,"PageMask:",9);
	TextOut( ps.hdc, 20,243,"Wired:",6);
	TextOut( ps.hdc, 20,267,"BadVaddr:",9);
	TextOut( ps.hdc, 20,291,"Count:",6);
	TextOut( ps.hdc, 225,99,"EntryHi:",8);
	TextOut( ps.hdc, 225,123,"Compare:",8);
	TextOut( ps.hdc, 225,147,"Status:",7);
	TextOut( ps.hdc, 225,171,"Cause:",6);
	TextOut( ps.hdc, 225,195,"EPC:",4);
	TextOut( ps.hdc, 225,219,"PRId:",5);
	TextOut( ps.hdc, 225,243,"Config:",7);
	TextOut( ps.hdc, 225,267,"LLAddr:",7);
	TextOut( ps.hdc, 435,99,"WatchLo:",8);
	TextOut( ps.hdc, 435,123,"WatchHi:",8);
	TextOut( ps.hdc, 435,147,"XContext:",9);
	TextOut( ps.hdc, 435,171,"Parity Error:",13);
	TextOut( ps.hdc, 435,195,"Cache Error:",12);
	TextOut( ps.hdc, 435,219,"TagLo:",6);
	TextOut( ps.hdc, 435,243,"TagHi:",6);
	TextOut( ps.hdc, 435,267,"ErrorEPC:",9);
		
	SelectObject( ps.hdc,hOldFont );
	SetBkMode( ps.hdc, OldBkMode );

	EndPaint( hWnd, &ps );
}

void PaintR4300iFPRPanel (HWND hWnd) {
	PAINTSTRUCT ps;
	RECT rcBox;
	HFONT hOldFont;
	int OldBkMode;
	BeginPaint( hWnd, &ps );
	
	rcBox.left   = 10;
	rcBox.top    = 79;
	rcBox.right  = 650;
	rcBox.bottom = 325;
	DrawEdge( ps.hdc, &rcBox, EDGE_ETCHED, BF_RECT );

	hOldFont = SelectObject( ps.hdc,
	GetStockObject(DEFAULT_GUI_FONT) );
	OldBkMode = SetBkMode( ps.hdc, TRANSPARENT );	

	TextOut( ps.hdc, 25,96," Reg 0:",7);
	TextOut( ps.hdc, 25,116," Reg 1:",7);
	TextOut( ps.hdc, 25,136," Reg 2:",7);
	TextOut( ps.hdc, 25,156," Reg 3:",7);
	TextOut( ps.hdc, 25,176," Reg 4:",7);
	TextOut( ps.hdc, 25,196," Reg 5:",7);
	TextOut( ps.hdc, 25,216," Reg 6:",7);
	TextOut( ps.hdc, 25,236," Reg 7:",7);
	TextOut( ps.hdc, 25,256," Reg 8:",7);
	TextOut( ps.hdc, 25,276," Reg 9:",7);
	TextOut( ps.hdc, 25,296,"Reg 10:",7);
	TextOut( ps.hdc, 240,96,"Reg 11:",7);
	TextOut( ps.hdc, 240,116,"Reg 12:",7);
	TextOut( ps.hdc, 240,136,"Reg 13:",7);
	TextOut( ps.hdc, 240,156,"Reg 14:",7);
	TextOut( ps.hdc, 240,176,"Reg 15:",7);
	TextOut( ps.hdc, 240,196,"Reg 16:",7);
	TextOut( ps.hdc, 240,216,"Reg 17:",7);
	TextOut( ps.hdc, 240,236,"Reg 18:",7);
	TextOut( ps.hdc, 240,256,"Reg 19:",7);
	TextOut( ps.hdc, 240,276,"Reg 20:",7);
	TextOut( ps.hdc, 240,296,"Reg 21:",7);
	TextOut( ps.hdc, 450,96,"Reg 22:",7);
	TextOut( ps.hdc, 450,116,"Reg 23:",7);
	TextOut( ps.hdc, 450,136,"Reg 24:",7);
	TextOut( ps.hdc, 450,156,"Reg 25:",7);
	TextOut( ps.hdc, 450,176,"Reg 26:",7);
	TextOut( ps.hdc, 450,196,"Reg 27:",7);
	TextOut( ps.hdc, 450,216,"Reg 28:",7);
	TextOut( ps.hdc, 450,236,"Reg 29:",7);
	TextOut( ps.hdc, 450,256,"Reg 30:",7);
	TextOut( ps.hdc, 450,276,"Reg 31:",7);
		
	SelectObject( ps.hdc,hOldFont );
	SetBkMode( ps.hdc, OldBkMode );
	EndPaint( hWnd, &ps );
}

void PaintR4300iCOP1SPanel(HWND hWnd) {
	PAINTSTRUCT ps;
	RECT rcBox;
	HFONT hOldFont;
	int OldBkMode;
	BeginPaint(hWnd, &ps);

	rcBox.left = 10;
	rcBox.top = 79;
	rcBox.right = 650;
	rcBox.bottom = 325;
	DrawEdge(ps.hdc, &rcBox, EDGE_ETCHED, BF_RECT);

	hOldFont = SelectObject(ps.hdc,
		GetStockObject(DEFAULT_GUI_FONT));
	OldBkMode = SetBkMode(ps.hdc, TRANSPARENT);

	TextOut(ps.hdc, 25, 96, " Reg 0:", 7);
	TextOut(ps.hdc, 25, 116, " Reg 1:", 7);
	TextOut(ps.hdc, 25, 136, " Reg 2:", 7);
	TextOut(ps.hdc, 25, 156, " Reg 3:", 7);
	TextOut(ps.hdc, 25, 176, " Reg 4:", 7);
	TextOut(ps.hdc, 25, 196, " Reg 5:", 7);
	TextOut(ps.hdc, 25, 216, " Reg 6:", 7);
	TextOut(ps.hdc, 25, 236, " Reg 7:", 7);
	TextOut(ps.hdc, 25, 256, " Reg 8:", 7);
	TextOut(ps.hdc, 25, 276, " Reg 9:", 7);
	TextOut(ps.hdc, 25, 296, "Reg 10:", 7);
	TextOut(ps.hdc, 240, 96, "Reg 11:", 7);
	TextOut(ps.hdc, 240, 116, "Reg 12:", 7);
	TextOut(ps.hdc, 240, 136, "Reg 13:", 7);
	TextOut(ps.hdc, 240, 156, "Reg 14:", 7);
	TextOut(ps.hdc, 240, 176, "Reg 15:", 7);
	TextOut(ps.hdc, 240, 196, "Reg 16:", 7);
	TextOut(ps.hdc, 240, 216, "Reg 17:", 7);
	TextOut(ps.hdc, 240, 236, "Reg 18:", 7);
	TextOut(ps.hdc, 240, 256, "Reg 19:", 7);
	TextOut(ps.hdc, 240, 276, "Reg 20:", 7);
	TextOut(ps.hdc, 240, 296, "Reg 21:", 7);
	TextOut(ps.hdc, 450, 96, "Reg 22:", 7);
	TextOut(ps.hdc, 450, 116, "Reg 23:", 7);
	TextOut(ps.hdc, 450, 136, "Reg 24:", 7);
	TextOut(ps.hdc, 450, 156, "Reg 25:", 7);
	TextOut(ps.hdc, 450, 176, "Reg 26:", 7);
	TextOut(ps.hdc, 450, 196, "Reg 27:", 7);
	TextOut(ps.hdc, 450, 216, "Reg 28:", 7);
	TextOut(ps.hdc, 450, 236, "Reg 29:", 7);
	TextOut(ps.hdc, 450, 256, "Reg 30:", 7);
	TextOut(ps.hdc, 450, 276, "Reg 31:", 7);

	SelectObject(ps.hdc, hOldFont);
	SetBkMode(ps.hdc, OldBkMode);
	EndPaint(hWnd, &ps);
}

void PaintR4300iCOP1DPanel(HWND hWnd) {
	PAINTSTRUCT ps;
	RECT rcBox;
	HFONT hOldFont;
	int OldBkMode;
	BeginPaint(hWnd, &ps);

	rcBox.left = 10;
	rcBox.top = 79;
	rcBox.right = 650;
	rcBox.bottom = 325;
	DrawEdge(ps.hdc, &rcBox, EDGE_ETCHED, BF_RECT);

	hOldFont = SelectObject(ps.hdc,
		GetStockObject(DEFAULT_GUI_FONT));
	OldBkMode = SetBkMode(ps.hdc, TRANSPARENT);

	TextOut(ps.hdc, 25, 96, " Reg 0:", 7);
	TextOut(ps.hdc, 25, 116, " Reg 1:", 7);
	TextOut(ps.hdc, 25, 136, " Reg 2:", 7);
	TextOut(ps.hdc, 25, 156, " Reg 3:", 7);
	TextOut(ps.hdc, 25, 176, " Reg 4:", 7);
	TextOut(ps.hdc, 25, 196, " Reg 5:", 7);
	TextOut(ps.hdc, 25, 216, " Reg 6:", 7);
	TextOut(ps.hdc, 25, 236, " Reg 7:", 7);
	TextOut(ps.hdc, 25, 256, " Reg 8:", 7);
	TextOut(ps.hdc, 25, 276, " Reg 9:", 7);
	TextOut(ps.hdc, 25, 296, "Reg 10:", 7);
	TextOut(ps.hdc, 240, 96, "Reg 11:", 7);
	TextOut(ps.hdc, 240, 116, "Reg 12:", 7);
	TextOut(ps.hdc, 240, 136, "Reg 13:", 7);
	TextOut(ps.hdc, 240, 156, "Reg 14:", 7);
	TextOut(ps.hdc, 240, 176, "Reg 15:", 7);
	TextOut(ps.hdc, 240, 196, "Reg 16:", 7);
	TextOut(ps.hdc, 240, 216, "Reg 17:", 7);
	TextOut(ps.hdc, 240, 236, "Reg 18:", 7);
	TextOut(ps.hdc, 240, 256, "Reg 19:", 7);
	TextOut(ps.hdc, 240, 276, "Reg 20:", 7);
	TextOut(ps.hdc, 240, 296, "Reg 21:", 7);
	TextOut(ps.hdc, 450, 96, "Reg 22:", 7);
	TextOut(ps.hdc, 450, 116, "Reg 23:", 7);
	TextOut(ps.hdc, 450, 136, "Reg 24:", 7);
	TextOut(ps.hdc, 450, 156, "Reg 25:", 7);
	TextOut(ps.hdc, 450, 176, "Reg 26:", 7);
	TextOut(ps.hdc, 450, 196, "Reg 27:", 7);
	TextOut(ps.hdc, 450, 216, "Reg 28:", 7);
	TextOut(ps.hdc, 450, 236, "Reg 29:", 7);
	TextOut(ps.hdc, 450, 256, "Reg 30:", 7);
	TextOut(ps.hdc, 450, 276, "Reg 31:", 7);

	SelectObject(ps.hdc, hOldFont);
	SetBkMode(ps.hdc, OldBkMode);
	EndPaint(hWnd, &ps);
}

void PaintR4300iCOP1WPanel(HWND hWnd) {
	PAINTSTRUCT ps;
	RECT rcBox;
	HFONT hOldFont;
	int OldBkMode;
	BeginPaint(hWnd, &ps);

	rcBox.left = 10;
	rcBox.top = 79;
	rcBox.right = 650;
	rcBox.bottom = 325;
	DrawEdge(ps.hdc, &rcBox, EDGE_ETCHED, BF_RECT);

	hOldFont = SelectObject(ps.hdc,
		GetStockObject(DEFAULT_GUI_FONT));
	OldBkMode = SetBkMode(ps.hdc, TRANSPARENT);

	TextOut(ps.hdc, 25, 96, " Reg 0:", 7);
	TextOut(ps.hdc, 25, 116, " Reg 1:", 7);
	TextOut(ps.hdc, 25, 136, " Reg 2:", 7);
	TextOut(ps.hdc, 25, 156, " Reg 3:", 7);
	TextOut(ps.hdc, 25, 176, " Reg 4:", 7);
	TextOut(ps.hdc, 25, 196, " Reg 5:", 7);
	TextOut(ps.hdc, 25, 216, " Reg 6:", 7);
	TextOut(ps.hdc, 25, 236, " Reg 7:", 7);
	TextOut(ps.hdc, 25, 256, " Reg 8:", 7);
	TextOut(ps.hdc, 25, 276, " Reg 9:", 7);
	TextOut(ps.hdc, 25, 296, "Reg 10:", 7);
	TextOut(ps.hdc, 240, 96, "Reg 11:", 7);
	TextOut(ps.hdc, 240, 116, "Reg 12:", 7);
	TextOut(ps.hdc, 240, 136, "Reg 13:", 7);
	TextOut(ps.hdc, 240, 156, "Reg 14:", 7);
	TextOut(ps.hdc, 240, 176, "Reg 15:", 7);
	TextOut(ps.hdc, 240, 196, "Reg 16:", 7);
	TextOut(ps.hdc, 240, 216, "Reg 17:", 7);
	TextOut(ps.hdc, 240, 236, "Reg 18:", 7);
	TextOut(ps.hdc, 240, 256, "Reg 19:", 7);
	TextOut(ps.hdc, 240, 276, "Reg 20:", 7);
	TextOut(ps.hdc, 240, 296, "Reg 21:", 7);
	TextOut(ps.hdc, 450, 96, "Reg 22:", 7);
	TextOut(ps.hdc, 450, 116, "Reg 23:", 7);
	TextOut(ps.hdc, 450, 136, "Reg 24:", 7);
	TextOut(ps.hdc, 450, 156, "Reg 25:", 7);
	TextOut(ps.hdc, 450, 176, "Reg 26:", 7);
	TextOut(ps.hdc, 450, 196, "Reg 27:", 7);
	TextOut(ps.hdc, 450, 216, "Reg 28:", 7);
	TextOut(ps.hdc, 450, 236, "Reg 29:", 7);
	TextOut(ps.hdc, 450, 256, "Reg 30:", 7);
	TextOut(ps.hdc, 450, 276, "Reg 31:", 7);

	SelectObject(ps.hdc, hOldFont);
	SetBkMode(ps.hdc, OldBkMode);
	EndPaint(hWnd, &ps);
}

void PaintR4300iCOP1LPanel(HWND hWnd) {
	PAINTSTRUCT ps;
	RECT rcBox;
	HFONT hOldFont;
	int OldBkMode;
	BeginPaint(hWnd, &ps);

	rcBox.left = 10;
	rcBox.top = 79;
	rcBox.right = 650;
	rcBox.bottom = 325;
	DrawEdge(ps.hdc, &rcBox, EDGE_ETCHED, BF_RECT);

	hOldFont = SelectObject(ps.hdc,
		GetStockObject(DEFAULT_GUI_FONT));
	OldBkMode = SetBkMode(ps.hdc, TRANSPARENT);

	TextOut(ps.hdc, 25, 96, " Reg 0:", 7);
	TextOut(ps.hdc, 25, 116, " Reg 1:", 7);
	TextOut(ps.hdc, 25, 136, " Reg 2:", 7);
	TextOut(ps.hdc, 25, 156, " Reg 3:", 7);
	TextOut(ps.hdc, 25, 176, " Reg 4:", 7);
	TextOut(ps.hdc, 25, 196, " Reg 5:", 7);
	TextOut(ps.hdc, 25, 21, " Reg 6:", 7);
	TextOut(ps.hdc, 25, 236, " Reg 7:", 7);
	TextOut(ps.hdc, 25, 256, " Reg 8:", 7);
	TextOut(ps.hdc, 25, 276, " Reg 9:", 7);
	TextOut(ps.hdc, 25, 296, "Reg 10:", 7);
	TextOut(ps.hdc, 240, 96, "Reg 11:", 7);
	TextOut(ps.hdc, 240, 116, "Reg 12:", 7);
	TextOut(ps.hdc, 240, 136, "Reg 13:", 7);
	TextOut(ps.hdc, 240, 156, "Reg 14:", 7);
	TextOut(ps.hdc, 240, 176, "Reg 15:", 7);
	TextOut(ps.hdc, 240, 196, "Reg 16:", 7);
	TextOut(ps.hdc, 240, 216, "Reg 17:", 7);
	TextOut(ps.hdc, 240, 236, "Reg 18:", 7);
	TextOut(ps.hdc, 240, 256, "Reg 19:", 7);
	TextOut(ps.hdc, 240, 276, "Reg 20:", 7);
	TextOut(ps.hdc, 240, 296, "Reg 21:", 7);
	TextOut(ps.hdc, 450, 96, "Reg 22:", 7);
	TextOut(ps.hdc, 450, 116, "Reg 23:", 7);
	TextOut(ps.hdc, 450, 136, "Reg 24:", 7);
	TextOut(ps.hdc, 450, 156, "Reg 25:", 7);
	TextOut(ps.hdc, 450, 176, "Reg 26:", 7);
	TextOut(ps.hdc, 450, 196, "Reg 27:", 7);
	TextOut(ps.hdc, 450, 216, "Reg 28:", 7);
	TextOut(ps.hdc, 450, 236, "Reg 29:", 7);
	TextOut(ps.hdc, 450, 256, "Reg 30:", 7);
	TextOut(ps.hdc, 450, 276, "Reg 31:", 7);

	SelectObject(ps.hdc, hOldFont);
	SetBkMode(ps.hdc, OldBkMode);
	EndPaint(hWnd, &ps);
}

void PaintR4300iGPRPanel (HWND hWnd) {	
	PAINTSTRUCT ps;
	RECT rcBox;
	HFONT hOldFont;
	int OldBkMode;
	BeginPaint( hWnd, &ps );
	
	rcBox.left   = 10;
	rcBox.top    = 79;
	rcBox.right  = 650;
	rcBox.bottom = 325;
	DrawEdge( ps.hdc, &rcBox, EDGE_ETCHED, BF_RECT );

	hOldFont = SelectObject( ps.hdc,
	GetStockObject(DEFAULT_GUI_FONT) );
	OldBkMode = SetBkMode( ps.hdc, TRANSPARENT );

	TextOut( ps.hdc, 20,96,"R0 - Reg 0:",11);
	TextOut( ps.hdc, 20,116,"AT - Reg 1:",11);
	TextOut( ps.hdc, 20,136,"V0 - Reg 2:",11);
	TextOut( ps.hdc, 20,156,"V1 - Reg 3:",11);
	TextOut( ps.hdc, 20,176,"A0 - Reg 4:",11);
	TextOut( ps.hdc, 20,196,"A1 - Reg 5:",11);
	TextOut( ps.hdc, 20,216,"A2 - Reg 6:",11);
	TextOut( ps.hdc, 20,236,"A3 - Reg 7:",11);
	TextOut( ps.hdc, 20,256,"T0 - Reg 8:",11);
	TextOut( ps.hdc, 20,276,"T1 - Reg 9:",11);
	TextOut( ps.hdc, 20,296,"T2 - Reg 10:",12);
	TextOut( ps.hdc, 225,96,"T3 - Reg 11:",12);
	TextOut( ps.hdc, 225,116,"T4 - Reg 12:",12);
	TextOut( ps.hdc, 225,136,"T5 - Reg 13:",12);
	TextOut( ps.hdc, 225,156,"T6 - Reg 14:",12);
	TextOut( ps.hdc, 225,176,"T7 - Reg 15:",12);
	TextOut( ps.hdc, 225,196,"S0 - Reg 16:",12);
	TextOut( ps.hdc, 225,216,"S1 - Reg 17:",12);
	TextOut( ps.hdc, 225,236,"S2 - Reg 18:",12);
	TextOut( ps.hdc, 225,256,"S3 - Reg 19:",12);
	TextOut( ps.hdc, 225,276,"S4 - Reg 20:",12);
	TextOut( ps.hdc, 225,296,"S5 - Reg 21:",12);
	TextOut( ps.hdc, 435,96,"S6 - Reg 22:",12);
	TextOut( ps.hdc, 435,116,"S7 - Reg 23:",12);
	TextOut( ps.hdc, 435,136,"T8 - Reg 24:",12);
	TextOut( ps.hdc, 435,156,"T9 - Reg 25:",12);
	TextOut( ps.hdc, 435,176,"K0 - Reg 26:",12);
	TextOut( ps.hdc, 435,196,"K1 - Reg 27:",12);
	TextOut( ps.hdc, 435,216,"GP - Reg 28:",12);
	TextOut( ps.hdc, 435,236,"SP - Reg 29:",12);
	TextOut( ps.hdc, 435,256,"S8 - Reg 30:",12);
	TextOut( ps.hdc, 435,276,"RA - Reg 31:",12);
		
	SelectObject( ps.hdc,hOldFont );
	SetBkMode( ps.hdc, OldBkMode );
	EndPaint( hWnd, &ps );
}

void PaintR4300iRDRamPanel(HWND hWnd) {
	PAINTSTRUCT ps;
	RECT rcBox;
	HFONT hOldFont;
	int OldBkMode;
	BeginPaint(hWnd, &ps);

	rcBox.left = 155;
	rcBox.top = 84;
	rcBox.right = 495;
	rcBox.bottom = 320;
	DrawEdge(ps.hdc, &rcBox, EDGE_ETCHED, BF_RECT);

	hOldFont = SelectObject(ps.hdc,
		GetStockObject(DEFAULT_GUI_FONT));
	OldBkMode = SetBkMode(ps.hdc, TRANSPARENT);

	TextOut(ps.hdc, 190, 106, "RDRAM_CONFIG_REG:", 17);
	TextOut(ps.hdc, 190, 126, "RDRAM_DEVICE_ID_REG:", 20);
	TextOut(ps.hdc, 190, 146, "RDRAM_DELAY_REG:", 16);
	TextOut(ps.hdc, 190, 166, "RDRAM_MODE_REG:", 15);
	TextOut(ps.hdc, 190, 186, "RDRAM_REF_INTERVAL_REG:", 23);
	TextOut(ps.hdc, 190, 206, "RDRAM_REF_ROW_REG:", 18);
	TextOut(ps.hdc, 190, 226, "RDRAM_RAS_INTERVAL_REG:", 23);
	TextOut(ps.hdc, 190, 246, "RDRAM_MIN_INTERVAL_REG:", 23);
	TextOut(ps.hdc, 190, 266, "RDRAM_ADDR_SELECT_REG:", 22);
	TextOut(ps.hdc, 190, 286, "RDRAM_DEVICE_MANUF_REG:", 23);

	SelectObject(ps.hdc, hOldFont);
	SetBkMode(ps.hdc, OldBkMode);
	EndPaint(hWnd, &ps);
}

void PaintR4300iRIPanel (HWND hWnd) { 
	PAINTSTRUCT ps;
	RECT rcBox;
	HFONT hOldFont;	
	int OldBkMode;
	BeginPaint( hWnd, &ps );
	
	rcBox.left   = 95;
	rcBox.top    = 114;
	rcBox.right  = 565;
	rcBox.bottom = 265;
	DrawEdge( ps.hdc, &rcBox, EDGE_ETCHED, BF_RECT );

	hOldFont = SelectObject( ps.hdc,
	GetStockObject(DEFAULT_GUI_FONT) );
	OldBkMode = SetBkMode( ps.hdc, TRANSPARENT );

	TextOut( ps.hdc, 110,136,"RI_MODE_REG:",12);
	TextOut( ps.hdc, 110,166,"RI_CONFIG_REG:",14);
	TextOut( ps.hdc, 110,196,"RI_CURRENT_LOAD_REG:",20);
	TextOut( ps.hdc, 110,226,"RI_SELECT_REG:",14);
	TextOut( ps.hdc, 360,136,"RI_REFRESH_REG:",15);
	TextOut( ps.hdc, 360,166,"RI_LATENCY_REG:",15);
	TextOut( ps.hdc, 360,196,"RI_RERROR_REG:",14);
	TextOut( ps.hdc, 360,226,"RI_WERROR_REG:",14);
		
	SelectObject( ps.hdc,hOldFont );
	SetBkMode( ps.hdc, OldBkMode );
	EndPaint( hWnd, &ps );
}

void PaintR4300iSIPanel (HWND hWnd) { 
	PAINTSTRUCT ps;
	RECT rcBox;
	HFONT hOldFont;
	int OldBkMode;
	BeginPaint( hWnd, &ps );
	
	rcBox.left   = 175;
	rcBox.top    = 114;
	rcBox.right  = 475;
	rcBox.bottom = 260;
	DrawEdge( ps.hdc, &rcBox, EDGE_ETCHED, BF_RECT );

	hOldFont = SelectObject( ps.hdc,
	GetStockObject(DEFAULT_GUI_FONT) );
	OldBkMode = SetBkMode( ps.hdc, TRANSPARENT );

	TextOut( ps.hdc, 200,136,"SI_DRAM_ADDR_REG:",17);
	TextOut( ps.hdc, 200,166,"SI_PIF_ADDR_RD64B_REG:",22);
	TextOut( ps.hdc, 200,196,"SI_PIF_ADDR_WR64B_REG:",22);
	TextOut( ps.hdc, 200,226,"SI_STATUS_REG:",14);
		
	SelectObject( ps.hdc,hOldFont );
	SetBkMode( ps.hdc, OldBkMode );
	EndPaint( hWnd, &ps );
}

void PaintR4300iSPPanel (HWND hWnd) { 
	PAINTSTRUCT ps;
	RECT rcBox;
	HFONT hOldFont;	
	int OldBkMode;
	BeginPaint( hWnd, &ps );
	
	rcBox.left   = 95;
	rcBox.top    = 114;
	rcBox.right  = 565;
	rcBox.bottom = 282;
	DrawEdge( ps.hdc, &rcBox, EDGE_ETCHED, BF_RECT );

	hOldFont = SelectObject( ps.hdc,
	GetStockObject(DEFAULT_GUI_FONT) );
	OldBkMode = SetBkMode( ps.hdc, TRANSPARENT );

	TextOut( ps.hdc, 110,131,"SP_MEM_ADDR_REG:",16);
	TextOut( ps.hdc, 110,161,"SP_DRAM_ADDR_REG:",17);
	TextOut( ps.hdc, 110,191,"SP_RD_LEN_REG:",14);
	TextOut( ps.hdc, 110,221,"SP_WR_LEN_REG:",14);
	TextOut( ps.hdc, 110,251,"SP_STATUS_REG:",14);
	TextOut( ps.hdc, 340,131,"SP_DMA_FULL_REG:",16);
	TextOut( ps.hdc, 340,161,"SP_DMA_BUSY_REG:",16);
	TextOut( ps.hdc, 340,191,"SP_SEMAPHORE_REG:",17);
	TextOut( ps.hdc, 340,221,"SP_PC_REG:",10);
	TextOut( ps.hdc, 340,251,"SP_IBIST_REG:",13);
		
	SelectObject( ps.hdc,hOldFont );
	SetBkMode( ps.hdc, OldBkMode );
	EndPaint( hWnd, &ps );
}

void PaintR4300iSpecialPanel (HWND hWnd) { 
	PAINTSTRUCT ps;
	RECT rcBox;
	HFONT hOldFont;
	int OldBkMode;
	BeginPaint( hWnd, &ps );
	
	rcBox.left   = 155;
	rcBox.top    = 84;
	rcBox.right  = 495;
	rcBox.bottom = 320;
	DrawEdge( ps.hdc, &rcBox, EDGE_ETCHED, BF_RECT );

	hOldFont = SelectObject( ps.hdc,
	GetStockObject(DEFAULT_GUI_FONT) );
	OldBkMode = SetBkMode( ps.hdc, TRANSPARENT );

	TextOut( ps.hdc, 210,116,"Program Counter:",16);
	TextOut( ps.hdc, 210,146,"Multi/Divide HI:",16);
	TextOut( ps.hdc, 210,176,"Multi/Divide LO:",16);
	TextOut( ps.hdc, 210,206,"Load/Link Bit:",14);
	TextOut( ps.hdc, 210,236,"Implementation/Revision:",24);
	TextOut( ps.hdc, 210,266,"Control/Status:",15);
		
	SelectObject( ps.hdc,hOldFont );
	SetBkMode( ps.hdc, OldBkMode );
	EndPaint( hWnd, &ps );
}

void PaintR4300iMIPanel (HWND hWnd) { 
	PAINTSTRUCT ps;
	RECT rcBox;
	HFONT hOldFont;	
	int OldBkMode;
	BeginPaint( hWnd, &ps );
	
	rcBox.left   = 175;
	rcBox.top    = 114;
	rcBox.right  = 475;
	rcBox.bottom = 260;
	DrawEdge( ps.hdc, &rcBox, EDGE_ETCHED, BF_RECT );

	hOldFont = SelectObject( ps.hdc,
	GetStockObject(DEFAULT_GUI_FONT) );
	OldBkMode = SetBkMode( ps.hdc, TRANSPARENT );

	TextOut( ps.hdc, 210,136,"MI_MODE_REG:",12);
	TextOut( ps.hdc, 210,166,"MI_VERSION_REG:",15);
	TextOut( ps.hdc, 210,196,"MI_INTR_REG:",12);
	TextOut( ps.hdc, 210,226,"MI_INTR_MASK_REG:",17);
		
	SelectObject( ps.hdc,hOldFont );
	SetBkMode( ps.hdc, OldBkMode );
	EndPaint( hWnd, &ps );
}

void PaintR4300iPIPanel (HWND hWnd) { 
	PAINTSTRUCT ps;
	RECT rcBox;
	HFONT hOldFont;	
	int OldBkMode;
	BeginPaint( hWnd, &ps );
	
	rcBox.left   = 85;
	rcBox.top    = 84;
	rcBox.right  = 575;
	rcBox.bottom = 320;
	DrawEdge( ps.hdc, &rcBox, EDGE_ETCHED, BF_RECT );

	hOldFont = SelectObject( ps.hdc, GetStockObject(DEFAULT_GUI_FONT) );
	OldBkMode = SetBkMode( ps.hdc, TRANSPARENT );

	TextOut( ps.hdc, 95,105,"PI_DRAM_ADDR_REG:",17);
	TextOut( ps.hdc, 95,135,"PI_CART_ADDR_REG:",17);
	TextOut( ps.hdc, 95,165,"PI_RD_LEN_REG:",14);
	TextOut( ps.hdc, 95,195,"PI_WR_LEN_REG:",14);
	TextOut( ps.hdc, 95,225,"PI_STATUS_REG:",14);
	TextOut( ps.hdc, 95,255,"PI_DOMAIN1_REG:",15);
	TextOut( ps.hdc, 95,285,"PI_BSD_DOM1_PWD_REG:",20);
	TextOut( ps.hdc, 330,105,"PI_BSD_DOM1_PGS_REG:",20);
	TextOut( ps.hdc, 330,135,"PI_BSD_DOM1_RLS_REG:",20);
	TextOut( ps.hdc, 330,165,"PI_DOMAIN2_REG:",15);
	TextOut( ps.hdc, 330,195,"PI_BSD_DOM2_PWD_REG:",20);
	TextOut( ps.hdc, 330,225,"PI_BSD_DOM2_PGS_REG:",20);
	TextOut( ps.hdc, 330,255,"PI_BSD_DOM2_RLS_REG:",20);
		
	SelectObject( ps.hdc,hOldFont );
	SetBkMode( ps.hdc, OldBkMode );
	EndPaint( hWnd, &ps );
}

void PaintR4300iVIPanel (HWND hWnd) { 
	PAINTSTRUCT ps;
	RECT rcBox;
	HFONT hOldFont;	
	int OldBkMode;
	BeginPaint( hWnd, &ps );
	
	rcBox.left   = 85;
	rcBox.top    = 84;
	rcBox.right  = 575;
	rcBox.bottom = 320;
	DrawEdge( ps.hdc, &rcBox, EDGE_ETCHED, BF_RECT );

	hOldFont = SelectObject( ps.hdc,GetStockObject(DEFAULT_GUI_FONT) );
	OldBkMode = SetBkMode( ps.hdc, TRANSPARENT );

	TextOut( ps.hdc, 95,105,"VI_STATUS_REG:",14);
	TextOut( ps.hdc, 95,135,"VI_DRAM_ADDR_REG:",17);
	TextOut( ps.hdc, 95,165,"VI_WIDTH_REG:",13);
	TextOut( ps.hdc, 95,195,"VI_INTR_REG:",12);
	TextOut( ps.hdc, 95,225,"VI_V_CURRENT_LINE_REG:",22);
	TextOut( ps.hdc, 95,255,"VI_TIMING_REG:",14);
	TextOut( ps.hdc, 95,285,"VI_V_SYNC_REG:",14);
	TextOut( ps.hdc, 345,105,"VI_H_SYNC_REG:",14);
	TextOut( ps.hdc, 345,135,"VI_H_SYNC_LEAP_REG:",19);
	TextOut( ps.hdc, 345,165,"VI_H_START_REG:",15);
	TextOut( ps.hdc, 345,195,"VI_V_START_REG:",15);
	TextOut( ps.hdc, 345,225,"VI_V_BURST_REG:",15);
	TextOut( ps.hdc, 345,255,"VI_X_SCALE_REG:",15);
	TextOut( ps.hdc, 345,285,"VI_Y_SCALE_REG:",15);
		
	SelectObject( ps.hdc,hOldFont );
	SetBkMode( ps.hdc, OldBkMode );
	EndPaint( hWnd, &ps );
}

LRESULT CALLBACK R4300i_Registers_Proc (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {	
	static RECT rcDisp;
	static int CurrentPanel = GeneralPurpose;
	TC_ITEM item;

	switch (uMsg) {
	case WM_INITDIALOG:
		R4300i_Registers_hDlg = hDlg;
		SetupR4300iRegistersMain( hDlg );
		break;
	case WM_MOVE:
		StoreCurrentWinPos("R4300i Registers",hDlg);
		break;
	case WM_SIZE:
		GetClientRect( hDlg, &rcDisp);
		TabCtrl_AdjustRect( hTab, FALSE, &rcDisp );
		break;
	case WM_NOTIFY:
		switch (((NMHDR *)lParam)->code) {
		case TCN_SELCHANGE:
			InvalidateRect( hTab, &rcDisp, TRUE );
			HideR4300iRegisterPanel (CurrentPanel);			
			item.mask = TCIF_PARAM;
			TabCtrl_GetItem( hTab, TabCtrl_GetCurSel( hTab ), &item );
			CurrentPanel = item.lParam;
			InvalidateRect( hStatic, NULL, FALSE );
			UpdateCurrentR4300iRegisterPanel();
			ShowR4300iRegisterPanel ( CurrentPanel );			
			break;
		}
		break;
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDCANCEL:
			CurrentPanel = GeneralPurpose;
			EndDialog( hDlg, IDCANCEL );
			break;
		}
		break;
	default:
		return FALSE;
	}
	return TRUE;
}

LRESULT CALLBACK RefreshR4300iRegProc ( HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam ) {
	int nSel;
	TC_ITEM item;

	switch( uMsg ) {
	case WM_PAINT:
		nSel = TabCtrl_GetCurSel( hTab );
		if ( nSel > -1 ) {
			item.mask = TCIF_PARAM;
			TabCtrl_GetItem( hTab, nSel, &item );
			switch( item.lParam ) {
			case GeneralPurpose:
				PaintR4300iGPRPanel (hWnd);
				break;
			case ControlProcessor0:
				PaintR4300iCP0Panel (hWnd);
				break;
			case FloatingRegisters:
				PaintR4300iFPRPanel (hWnd);
				break;
			case COP1SRegisters:
				PaintR4300iCOP1SPanel(hWnd);
				break;
			case COP1DRegisters:
				PaintR4300iCOP1DPanel(hWnd);
				break;
			case COP1WRegisters:
				PaintR4300iCOP1WPanel(hWnd);
				break;
			case COP1LRegisters:
				PaintR4300iCOP1LPanel(hWnd);
				break;
			case SpecialRegister:
				PaintR4300iSpecialPanel (hWnd);
				break;
			case RDRAMRegisters:
				PaintR4300iRDRamPanel (hWnd);
				break;
			case RDRAMRegisters2:
				PaintR4300iRDRamPanel(hWnd);
				break;
			case ExpRDRAMRegisters:
				if (NUMBER_OF_RDRAM_MODULES > 2) {
					PaintR4300iRDRamPanel(hWnd);
				}
				break;
			case ExpRDRAMRegisters2:
				if (NUMBER_OF_RDRAM_MODULES > 2) {
					PaintR4300iRDRamPanel(hWnd);
				}
				break;
			case SPRegisters:
				PaintR4300iSPPanel (hWnd);
				break;
			case MIPSInterface:
				PaintR4300iMIPanel (hWnd);
				break;
			case VideoInterface:
				PaintR4300iVIPanel (hWnd);
				break;
			case AudioInterface:
				PaintR4300iAIPanel (hWnd);
				break;
			case PeripheralInterface:
				PaintR4300iPIPanel (hWnd);
				break;
			case RDRAMInterface:
				PaintR4300iRIPanel (hWnd);
				break;
			case SerialInterface:
				PaintR4300iSIPanel (hWnd);
				break;
			}

		}
		break;
	default:
		return TRUE;	// Fixes Win10 Crashing, though Win7 seems unaffected?
		//return( (*r4300iRegRefreshProc)(hWnd, uMsg, wParam, lParam) );
	}
	return( FALSE );
}

void SetupR4300iAIPanel (HWND hDlg) {
	int count;

	for (count = 0; count < 6;count ++) {
		hAI[count] = CreateWindowEx(WS_EX_CLIENTEDGE,"EDIT","", WS_CHILD | 
			ES_READONLY | WS_BORDER | WS_TABSTOP,342,(count*30) + 119,80,19, 
			hDlg,0,hInst, NULL );
		SendMessage(hAI[count],WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT),0);
	}
}

void SetupR4300iCP0Panel (HWND hDlg) {
	int count, top;
	for (count = 0;count < 32;count ++) { hCP0[count] = NULL; }
	top = 103;
	for (count = 0; count < 10;count ++) {
		if (count == 7) { continue; }
		hCP0[count] = CreateWindowEx(WS_EX_CLIENTEDGE,"EDIT","", WS_CHILD | 
			ES_READONLY | WS_BORDER | WS_TABSTOP,90,top,135,19, 
			hDlg,0,hInst, NULL );
		SendMessage(hCP0[count],WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT),0);
		top += 24;
	}

	top = 103;
	for (count = 0; count < 8;count ++) {
		hCP0[count + 10] = CreateWindowEx(WS_EX_CLIENTEDGE,"EDIT","", WS_CHILD | 
			ES_READONLY | WS_BORDER | WS_TABSTOP,300,top,135,19, 
			hDlg,0,hInst, NULL );
		SendMessage(hCP0[count + 10],WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT),0);
		top += 24;
	}

	top = 103;
	for (count = 0; count < 13;count ++) {
		if (count >= 3 && count <= 7 ) { continue; }
		hCP0[count + 18] = CreateWindowEx(WS_EX_CLIENTEDGE,"EDIT","", WS_CHILD | 
			ES_READONLY | WS_BORDER | WS_TABSTOP,510,top,135,19, 
			hDlg,0,hInst, NULL );
		SendMessage(hCP0[count + 18],WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT),0);
		top += 24;
	}

}

void SetupR4300iFPRPanel (HWND hDlg) {
	int count;

	for (count = 0; count < 11;count ++) {
		hFPR[count] = CreateWindowEx(WS_EX_CLIENTEDGE,"EDIT","", WS_CHILD | 
			ES_READONLY | WS_BORDER | WS_TABSTOP,90,(count*20) + 100,135,19, 
			hDlg,0,hInst, NULL );
		SendMessage(hFPR[count],WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT),0);

	}
	for (count = 0; count < 11;count ++) {
		hFPR[count + 11] = CreateWindowEx(WS_EX_CLIENTEDGE,"EDIT","", WS_CHILD | 
			ES_READONLY | WS_BORDER | WS_TABSTOP,300,(count*20) + 100,135,19, 
			hDlg,0,hInst, NULL );
		SendMessage(hFPR[ count + 11 ],WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT),0);
	}
	for (count = 0; count < 10;count ++) {
		hFPR[count + 22] = CreateWindowEx(WS_EX_CLIENTEDGE,"EDIT","", WS_CHILD |  
			ES_READONLY | WS_BORDER | WS_TABSTOP,510,(count*20) + 100,135,19, 
			hDlg,0,hInst, NULL );
		SendMessage(hFPR[ count + 22 ],WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT),0);
	}
}

void SetupR4300iCOP1SPanel(HWND hDlg) {
	int count;

	for (count = 0; count < 11; count++) {
		hCOP1S[count] = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD |
			ES_READONLY | WS_BORDER | WS_TABSTOP, 90, (count * 20) + 100, 135, 19,
			hDlg, 0, hInst, NULL);
		SendMessage(hCOP1S[count], WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), 0);

	}
	for (count = 0; count < 11; count++) {
		hCOP1S[count + 11] = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD |
			ES_READONLY | WS_BORDER | WS_TABSTOP, 300, (count * 20) + 100, 135, 19,
			hDlg, 0, hInst, NULL);
		SendMessage(hCOP1S[count + 11], WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), 0);
	}
	for (count = 0; count < 10; count++) {
		hCOP1S[count + 22] = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD |
			ES_READONLY | WS_BORDER | WS_TABSTOP, 510, (count * 20) + 100, 135, 19,
			hDlg, 0, hInst, NULL);
		SendMessage(hCOP1S[count + 22], WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), 0);
	}
}

void SetupR4300iCOP1DPanel(HWND hDlg) {
	int count;

	for (count = 0; count < 11; count++) {
		hCOP1D[count] = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD |
			ES_READONLY | WS_BORDER | WS_TABSTOP, 90, (count * 20) + 100, 135, 19,
			hDlg, 0, hInst, NULL);
		SendMessage(hCOP1D[count], WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), 0);

	}
	for (count = 0; count < 11; count++) {
		hCOP1D[count + 11] = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD |
			ES_READONLY | WS_BORDER | WS_TABSTOP, 300, (count * 20) + 100, 135, 19,
			hDlg, 0, hInst, NULL);
		SendMessage(hCOP1D[count + 11], WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), 0);
	}
	for (count = 0; count < 10; count++) {
		hCOP1D[count + 22] = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD |
			ES_READONLY | WS_BORDER | WS_TABSTOP, 510, (count * 20) + 100, 135, 19,
			hDlg, 0, hInst, NULL);
		SendMessage(hCOP1D[count + 22], WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), 0);
	}
}

void SetupR4300iCOP1WPanel(HWND hDlg) {
	int count;

	for (count = 0; count < 11; count++) {
		hCOP1W[count] = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD |
			ES_READONLY | WS_BORDER | WS_TABSTOP, 90, (count * 20) + 100, 135, 19,
			hDlg, 0, hInst, NULL);
		SendMessage(hCOP1W[count], WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), 0);

	}
	for (count = 0; count < 11; count++) {
		hCOP1W[count + 11] = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD |
			ES_READONLY | WS_BORDER | WS_TABSTOP, 300, (count * 20) + 100, 135, 19,
			hDlg, 0, hInst, NULL);
		SendMessage(hCOP1W[count + 11], WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), 0);
	}
	for (count = 0; count < 10; count++) {
		hCOP1W[count + 22] = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD |
			ES_READONLY | WS_BORDER | WS_TABSTOP, 510, (count * 20) + 100, 135, 19,
			hDlg, 0, hInst, NULL);
		SendMessage(hCOP1W[count + 22], WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), 0);
	}
}

void SetupR4300iCOP1LPanel(HWND hDlg) {
	int count;

	for (count = 0; count < 11; count++) {
		hCOP1L[count] = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD |
			ES_READONLY | WS_BORDER | WS_TABSTOP, 90, (count * 20) + 100, 135, 19,
			hDlg, 0, hInst, NULL);
		SendMessage(hCOP1L[count], WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), 0);

	}
	for (count = 0; count < 11; count++) {
		hCOP1L[count + 11] = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD |
			ES_READONLY | WS_BORDER | WS_TABSTOP, 300, (count * 20) + 100, 135, 19,
			hDlg, 0, hInst, NULL);
		SendMessage(hCOP1L[count + 11], WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), 0);
	}
	for (count = 0; count < 10; count++) {
		hCOP1L[count + 22] = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD |
			ES_READONLY | WS_BORDER | WS_TABSTOP, 510, (count * 20) + 100, 135, 19,
			hDlg, 0, hInst, NULL);
		SendMessage(hCOP1L[count + 22], WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), 0);
	}
}

void SetupR4300iGPRPanel (HWND hDlg) {
	int count;

	for (count = 0; count < 11;count ++) {
		hGPR[count] = CreateWindowEx(WS_EX_CLIENTEDGE,"EDIT","", WS_CHILD | 
			ES_READONLY | WS_BORDER | WS_TABSTOP,90,(count*20) + 100,135,19, 
			hDlg,0,hInst, NULL );
		SendMessage(hGPR[count],WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT),0);

	}
	for (count = 0; count < 11;count ++) {
		hGPR[count + 11] = CreateWindowEx(WS_EX_CLIENTEDGE,"EDIT","", WS_CHILD | 
			ES_READONLY | WS_BORDER | WS_TABSTOP,300,(count*20) + 100,135,19, 
			hDlg,0,hInst, NULL );
		SendMessage(hGPR[ count + 11 ],WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT),0);
	}
	for (count = 0; count < 10;count ++) {
		hGPR[count + 22] = CreateWindowEx(WS_EX_CLIENTEDGE,"EDIT","", WS_CHILD |  
			ES_READONLY | WS_BORDER | WS_TABSTOP,510,(count*20) + 100,135,19, 
			hDlg,0,hInst, NULL );
		SendMessage(hGPR[ count + 22 ],WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT),0);
	}
}

void SetupR4300iMIPanel (HWND hDlg) {
	int count;

	for (count = 0; count < 4;count ++) {
		hMI[count] = CreateWindowEx(WS_EX_CLIENTEDGE,"EDIT","", WS_CHILD | 
			ES_READONLY | WS_BORDER | WS_TABSTOP,340,(count*30) + 139,80,19, 
			hDlg,0,hInst, NULL );
		SendMessage(hMI[count],WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT),0);
	}
}

void SetupR4300iPIPanel (HWND hDlg) {
	int count;

	for (count = 0; count < 7;count ++) {
		hPI[count] = CreateWindowEx(WS_EX_CLIENTEDGE,"EDIT","", WS_CHILD | 
			ES_READONLY | WS_BORDER | WS_TABSTOP,245,(count*30) + 108,80,19, 
			hDlg,0,hInst, NULL );
		SendMessage(hPI[count],WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT),0);
	}
	for (count = 0; count < 6;count ++) {
		hPI[count + 7] = CreateWindowEx(WS_EX_CLIENTEDGE,"EDIT","", WS_CHILD | 
			ES_READONLY | WS_BORDER | WS_TABSTOP,480,(count*30) + 108,80,19, 
			hDlg,0,hInst, NULL );
		SendMessage(hPI[count + 7],WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT),0);
	}
}

void SetupR4300iRDRamPanel (HWND hDlg) {
	int count;

	for (count = 0; count < 10;count ++) {
		hRDRam[count] = CreateWindowEx(WS_EX_CLIENTEDGE,"EDIT","", WS_CHILD | 
			ES_READONLY | WS_BORDER | WS_TABSTOP,375,(count*20) + 108,80,19, 
			hDlg,0,hInst, NULL );
		SendMessage(hRDRam[count],WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT),0);
	}
}

void SetupR4300iRDRam2Panel(HWND hDlg) {
	int count;

	for (count = 0; count < 10; count++) {
		hRDRam2[count] = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD |
			ES_READONLY | WS_BORDER | WS_TABSTOP, 375, (count * 20) + 108, 80, 19,
			hDlg, 0, hInst, NULL);
		SendMessage(hRDRam2[count], WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), 0);
	}
}

void SetupR4300iExpRDRamPanel(HWND hDlg) {
	int count;

	for (count = 0; count < 10; count++) {
		hExpRDRam[count] = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD |
			ES_READONLY | WS_BORDER | WS_TABSTOP, 375, (count * 20) + 108, 80, 19,
			hDlg, 0, hInst, NULL);
		SendMessage(hExpRDRam[count], WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), 0);
	}
}

void SetupR4300iExpRDRam2Panel(HWND hDlg) {
	int count;

	for (count = 0; count < 10; count++) {
		hExpRDRam2[count] = CreateWindowEx(WS_EX_CLIENTEDGE, "EDIT", "", WS_CHILD |
			ES_READONLY | WS_BORDER | WS_TABSTOP, 375, (count * 20) + 108, 80, 19,
			hDlg, 0, hInst, NULL);
		SendMessage(hExpRDRam2[count], WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT), 0);
	}
}

void SetupR4300iRIPanel (HWND hDlg) {
	int count;

	for (count = 0; count < 4;count ++) {
		hRI[count] = CreateWindowEx(WS_EX_CLIENTEDGE,"EDIT","", WS_CHILD | 
			ES_READONLY | WS_BORDER | WS_TABSTOP,260,(count*30) + 140,80,19, 
			hDlg,0,hInst, NULL );
		SendMessage(hRI[count],WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT),0);
	}

	for (count = 0; count < 4;count ++) {
		hRI[count + 4] = CreateWindowEx(WS_EX_CLIENTEDGE,"EDIT","", WS_CHILD | 
			ES_READONLY | WS_BORDER | WS_TABSTOP,475,(count*30) + 140,80,19, 
			hDlg,0,hInst, NULL );
		SendMessage(hRI[count + 4],WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT),0);
	}
}

void SetupR4300iSIPanel (HWND hDlg) {
	int count;

	for (count = 0; count < 4;count ++) {
		hSI[count] = CreateWindowEx(WS_EX_CLIENTEDGE,"EDIT","", WS_CHILD | 
			ES_READONLY | WS_BORDER | WS_TABSTOP,360,(count*30) + 139,80,19, 
			hDlg,0,hInst, NULL );
		SendMessage(hSI[count],WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT),0);
	}

}

void SetupR4300iSPPanel (HWND hDlg) {
	int count;

	for (count = 0; count < 5;count ++) {
		hSP[count] = CreateWindowEx(WS_EX_CLIENTEDGE,"EDIT","", WS_CHILD | 
			ES_READONLY | WS_BORDER | WS_TABSTOP,250,(count*30) + 134,80,19, 
			hDlg,0,hInst, NULL );
		SendMessage(hSP[count],WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT),0);
	}
	for (count = 0; count < 5;count ++) {
		hSP[count + 5] = CreateWindowEx(WS_EX_CLIENTEDGE,"EDIT","", WS_CHILD | 
			ES_READONLY | WS_BORDER | WS_TABSTOP,475,(count*30) + 134,80,19, 
			hDlg,0,hInst, NULL );
		SendMessage(hSP[ count + 5 ],WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT),0);
	}
}

void SetupR4300iSpecialPanel (HWND hDlg) {
	int count;
	hSpecial[0] = CreateWindowEx(WS_EX_CLIENTEDGE,"EDIT","", WS_CHILD | 
		ES_READONLY | WS_BORDER | WS_TABSTOP,345,120,135,19, 
		hDlg,0,hInst, NULL );
	hSpecial[1] = CreateWindowEx(WS_EX_CLIENTEDGE,"EDIT","", WS_CHILD | 
		ES_READONLY | WS_BORDER | WS_TABSTOP,345,150,135,19, 
		hDlg,0,hInst, NULL );
	hSpecial[2] = CreateWindowEx(WS_EX_CLIENTEDGE,"EDIT","", WS_CHILD | 
		ES_READONLY | WS_BORDER | WS_TABSTOP,345,180,135,19, 
		hDlg,0,hInst, NULL );
	hSpecial[3] = CreateWindowEx(WS_EX_CLIENTEDGE,"EDIT","", WS_CHILD | 
		ES_READONLY | WS_BORDER | WS_TABSTOP,345,210,34,19, 
		hDlg,0,hInst, NULL );
	hSpecial[4] = CreateWindowEx(WS_EX_CLIENTEDGE,"EDIT","", WS_CHILD | 
		ES_READONLY | WS_BORDER | WS_TABSTOP,345,240,80,19, 
		hDlg,0,hInst, NULL );
	hSpecial[5] = CreateWindowEx(WS_EX_CLIENTEDGE,"EDIT","", WS_CHILD | 
		ES_READONLY | WS_BORDER | WS_TABSTOP,345,270,80,19, 
		hDlg,0,hInst, NULL );
	for (count = 0; count < 6;count ++) {
		SendMessage(hSpecial[count],WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT),0);
	}
}

void SetupR4300iVIPanel (HWND hDlg) {
	int count;

	for (count = 0; count < 7;count ++) {
		hVI[count] = CreateWindowEx(WS_EX_CLIENTEDGE,"EDIT","", WS_CHILD | 
			ES_READONLY | WS_BORDER | WS_TABSTOP,250,(count*30) + 108,80,19, 
			hDlg,0,hInst, NULL );
		SendMessage(hVI[count],WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT),0);
	}
	for (count = 0; count < 7;count ++) {
		hVI[count + 7] = CreateWindowEx(WS_EX_CLIENTEDGE,"EDIT","", WS_CHILD | 
			ES_READONLY | WS_BORDER | WS_TABSTOP,480,(count*30) + 108,80,19, 
			hDlg,0,hInst, NULL );
		SendMessage(hVI[count + 7],WM_SETFONT, (WPARAM)GetStockObject(DEFAULT_GUI_FONT),0);
	}
}

void SetupR4300iRegistersMain (HWND hDlg) {
#define WindowWidth  685
#define WindowHeight 390
	DWORD X, Y;

	hTab = CreateWindowEx(0,WC_TABCONTROL,"", WS_TABSTOP | WS_CHILD | WS_VISIBLE | TCS_MULTILINE,5,6,660,340,
		hDlg,(HMENU)IDC_TAB_CONTROL,hInst,NULL );
	if ( hTab ) {
		TC_ITEM item;
		SendMessage(hTab, WM_SETFONT, (WPARAM)GetStockObject( DEFAULT_GUI_FONT ), 0);
		item.mask    = TCIF_TEXT | TCIF_PARAM;
		item.pszText = " General Purpose ";
		item.lParam  = GeneralPurpose;
		TabCtrl_InsertItem( hTab,0, &item);		
		item.lParam  = ControlProcessor0;
		item.pszText = " Control Processor 0 ";
		TabCtrl_InsertItem( hTab,1, &item);	
		item.lParam  = FloatingRegisters;
		item.pszText = " floating-point Registers ";
		TabCtrl_InsertItem( hTab,2, &item);
		item.lParam = COP1SRegisters;
		item.pszText = " COP1.S ";
		TabCtrl_InsertItem(hTab, 3, &item);
		item.lParam = COP1DRegisters;
		item.pszText = " COP1.D ";
		TabCtrl_InsertItem(hTab, 4, &item);
		item.lParam = COP1WRegisters;
		item.pszText = " COP1.W ";
		TabCtrl_InsertItem(hTab, 5, &item);
		item.lParam = COP1LRegisters;
		item.pszText = " COP1.L ";
		TabCtrl_InsertItem(hTab, 6, &item);
		item.lParam  = SpecialRegister;
		item.pszText = " Special Registers ";
		TabCtrl_InsertItem( hTab,7, &item);	
		item.lParam  = RDRAMRegisters;
		item.pszText = " RDRAM Registers Bank 1 ";
		TabCtrl_InsertItem( hTab,8, &item);
		item.lParam  = RDRAMRegisters2;
		item.pszText = " RDRAM Registers Bank 2 ";
		TabCtrl_InsertItem( hTab, 9, &item);
		item.lParam  = ExpRDRAMRegisters;
		item.pszText = " Expansion Pak Bank 1 ";
		TabCtrl_InsertItem( hTab, 10, &item);
		item.lParam  = ExpRDRAMRegisters2;
		item.pszText = " Expansion Pak Bank 2 ";
		TabCtrl_InsertItem( hTab, 11, &item);
		item.lParam  = SPRegisters;
		item.pszText = " SP Registers ";
		TabCtrl_InsertItem( hTab, 12, &item);	
		item.lParam  = MIPSInterface;
		item.pszText = " MIPS Interface ";
		TabCtrl_InsertItem( hTab,13, &item);
		item.lParam  = VideoInterface  ;
		item.pszText = " Video Interface   ";
		TabCtrl_InsertItem( hTab,14, &item);	
		item.lParam  = AudioInterface ;
		item.pszText = " Audio Interface  ";
		TabCtrl_InsertItem( hTab,15, &item);	
		item.lParam  = PeripheralInterface;
		item.pszText = " Peripheral Interface ";
		TabCtrl_InsertItem( hTab,16, &item);	
		item.lParam  = RDRAMInterface;
		item.pszText = " RDRAM Interface ";
		TabCtrl_InsertItem( hTab,17, &item);	
		item.lParam  = SerialInterface;
		item.pszText = " Serial Interface ";
		TabCtrl_InsertItem( hTab,18, &item);	
	}
	
	SetupR4300iAIPanel ( hDlg );
	SetupR4300iCP0Panel ( hDlg );
	SetupR4300iFPRPanel ( hDlg );
	SetupR4300iCOP1SPanel( hDlg );
	SetupR4300iCOP1DPanel( hDlg );
	SetupR4300iCOP1WPanel( hDlg );
	SetupR4300iCOP1LPanel( hDlg );
	SetupR4300iGPRPanel ( hDlg );
	SetupR4300iMIPanel ( hDlg );
	SetupR4300iRDRamPanel ( hDlg );
	SetupR4300iRDRam2Panel(hDlg);
	SetupR4300iExpRDRamPanel(hDlg);
	SetupR4300iExpRDRam2Panel(hDlg);
	SetupR4300iPIPanel ( hDlg );
	SetupR4300iRIPanel ( hDlg );
	SetupR4300iSIPanel ( hDlg );
	SetupR4300iSPPanel ( hDlg );
	SetupR4300iSpecialPanel ( hDlg );
	SetupR4300iVIPanel ( hDlg);

	hStatic = CreateWindowEx(0,"STATIC","", WS_CHILD|WS_VISIBLE, 5,6,660,340,hDlg,0,hInst,NULL );
	r4300iRegRefreshProc = (FARPROC)SetWindowLong( hStatic,GWL_WNDPROC,(long)RefreshR4300iRegProc);

	ShowR4300iRegisterPanel ( GeneralPurpose );
	UpdateCurrentR4300iRegisterPanel ();
	SetWindowText(hDlg," R4300i Registers");
	
	if ( !GetStoredWinPos( "R4300i Registers", &X, &Y ) ) {
		X = (GetSystemMetrics( SM_CXSCREEN ) - WindowWidth) / 2;
		Y = (GetSystemMetrics( SM_CYSCREEN ) - WindowHeight) / 2;
	}
	SetWindowPos(hDlg,NULL,X,Y,WindowWidth,WindowHeight, SWP_NOZORDER | SWP_SHOWWINDOW);
}

void ShowR4300iRegisterPanel ( int Panel) {
	int count;

	switch( Panel ) {
	case GeneralPurpose:
		for (count = 0; count < 32;count ++) { ShowWindow(hGPR[count],TRUE); }
		break;
	case ControlProcessor0:
		for (count = 0; count < 32;count ++) { ShowWindow(hCP0[count],TRUE); }
		break;
	case FloatingRegisters:
		for (count = 0; count < 32;count ++) { ShowWindow(hFPR[count],TRUE); }
		break;
	case COP1SRegisters:
		for (count = 0; count < 32; count++) { ShowWindow(hCOP1S[count], TRUE); }
		break;
	case COP1DRegisters:
		for (count = 0; count < 32; count++) { ShowWindow(hCOP1D[count], TRUE); }
		break;
	case COP1WRegisters:
		for (count = 0; count < 32; count++) { ShowWindow(hCOP1W[count], TRUE); }
		break;
	case COP1LRegisters:
		for (count = 0; count < 32; count++) { ShowWindow(hCOP1L[count], TRUE); }
		break;
	case SpecialRegister:
		for (count = 0; count < 6;count ++) { ShowWindow(hSpecial[count],TRUE); }
		break;
	case RDRAMRegisters:
		for (count = 0; count < 10;count ++) { ShowWindow(hRDRam[count],TRUE); }
		break;
	case RDRAMRegisters2:
		for (count = 0; count < 10; count++) { ShowWindow(hRDRam2[count], TRUE); }
		break;
	case ExpRDRAMRegisters:
		if (NUMBER_OF_RDRAM_MODULES > 2) {
			for (count = 0; count < 10; count++) { ShowWindow(hExpRDRam[count], TRUE); }
		}
		break;
	case ExpRDRAMRegisters2:
		if (NUMBER_OF_RDRAM_MODULES > 2) {
			for (count = 0; count < 10; count++) { ShowWindow(hExpRDRam2[count], TRUE); }
		}
		break;
	case SPRegisters:
		for (count = 0; count < 10;count ++) { ShowWindow(hSP[count],TRUE); }
		break;
	case MIPSInterface:
		for (count = 0; count < 4;count ++) { ShowWindow(hMI[count],TRUE); }
		break;
	case VideoInterface:
		for (count = 0; count < 14;count ++) { ShowWindow(hVI[count],TRUE); }
		break;
	case AudioInterface:
		for (count = 0; count < 6;count ++) { ShowWindow(hAI[count],TRUE); }
		break;
	case PeripheralInterface:
		for (count = 0; count < 13;count ++) { ShowWindow(hPI[count],TRUE); }
		break;
	case RDRAMInterface:
		for (count = 0; count < 8;count ++) { ShowWindow(hRI[count],TRUE); }
		break;
	case SerialInterface:
		for (count = 0; count < 4;count ++) { ShowWindow(hSI[count],TRUE); }
		break;
	}
}

void __cdecl UpdateCurrentR4300iRegisterPanel ( void ) {
	char RegisterValue[60], OldWinText[60];
	int count, nSel;
	TC_ITEM item;

	if (!InR4300iRegisterWindow) { return; }
	nSel = TabCtrl_GetCurSel( hTab );
	if ( nSel > -1 ) {
		item.mask = TCIF_PARAM;
		TabCtrl_GetItem( hTab, nSel, &item );
		switch( item.lParam ) {
		case GeneralPurpose:
			for (count = 0; count < 32;count ++) {
				GetWindowText(hGPR[count],OldWinText,60);
				sprintf(RegisterValue," 0x%08X - %08X",GPR[count].W[1],GPR[count].W[0]);
				if ( strcmp( RegisterValue, OldWinText) != 0 ) {
					SetWindowText(hGPR[count],RegisterValue);
				}
			}
			break;
		case ControlProcessor0:
			for (count = 0; count < 32;count ++) {
				GetWindowText(hCP0[count],OldWinText,60);
				switch (count) {
				case 4: //Context
				case 8: //BadVAddr
				case 10: //EntryHi
				case 14: //EPC
				case 20: //XContext
				case 30: //ErrEPC
					sprintf(RegisterValue, " 0x%08X - %08X", CP0[count].W[1], CP0[count].W[0]);
					break;
				default:
					sprintf(RegisterValue, " 0x%08X", CP0[count].W[0]);
				}
				if ( strcmp( RegisterValue, OldWinText) != 0 ) {
					SetWindowText(hCP0[count],RegisterValue);
				}
			}
			break;
		case FloatingRegisters:
			for (count = 0; count < 32;count ++) {
				GetWindowText(hFPR[count],OldWinText,60);
				sprintf(RegisterValue," 0x%08X - %08X",FPR[count].W[1],FPR[count].W[0]);
				if ( strcmp( RegisterValue, OldWinText) != 0 ) {
					SetWindowText(hFPR[count],RegisterValue);
				}
			}
			break;
		case COP1SRegisters:
			for (count = 0; count < 32; count++) {
				GetWindowText(hCOP1S[count], OldWinText, 60);
				if ((STATUS_REGISTER & STATUS_FR) == 0 && (count & 1) == 1) { // odd not alowed in half mode
					sprintf(RegisterValue, " -");
				}
				else if (IsSubNormal_S(*(DWORD*)FPRFloatLoadStoreLocation[count])) {
					sprintf(RegisterValue, " subnormal");
				}
				else if (IsNAN_S(*(DWORD*)FPRFloatLoadStoreLocation[count])) {
					sprintf(RegisterValue, " NAN");
				}
				else if (IsQNAN_S(*(DWORD*)FPRFloatLoadStoreLocation[count])) {
					sprintf(RegisterValue, " quiet NAN");
				}
				else {
					snprintf(RegisterValue, 50, " %f", *(float*)FPRFloatLoadStoreLocation[count]);
				}
				if (strcmp(RegisterValue, OldWinText) != 0) {
					SetWindowText(hCOP1S[count], RegisterValue);
				}
			}
			break;
		case COP1DRegisters:
			for (count = 0; count < 32; count++) {
				GetWindowText(hCOP1D[count], OldWinText, 60);
				if ((STATUS_REGISTER & STATUS_FR) == 0 && (count & 1) == 1) { // odd not alowed in half mode
					sprintf(RegisterValue, " -");
				}
				else if (IsSubNormal_D(*(QWORD*)FPRDoubleLocation[count])) {
					sprintf(RegisterValue, " subnormal");
				}
				else if (IsNAN_D(*(QWORD*)FPRDoubleLocation[count])) {
					sprintf(RegisterValue, " NAN");
				}
				else if (IsQNAN_D(*(QWORD*)FPRDoubleLocation[count])) {
					sprintf(RegisterValue, " quiet NAN");
				}
				else {
					snprintf(RegisterValue, 50, " %lf", *(double*)FPRDoubleLocation[count]);
				}
				if (strcmp(RegisterValue, OldWinText) != 0) {
					SetWindowText(hCOP1D[count], RegisterValue);
				}
			}
			break;
		case COP1WRegisters:
			for (count = 0; count < 32; count++) {
				GetWindowText(hCOP1W[count], OldWinText, 60);
				if ((STATUS_REGISTER & STATUS_FR) == 0 && (count & 1) == 1) { // odd not alowed in half mode
					sprintf(RegisterValue, " -");
				}
				else {
					sprintf(RegisterValue, " %d", *(DWORD*)FPRFloatLoadStoreLocation[count]);
				}
				if (strcmp(RegisterValue, OldWinText) != 0) {
					SetWindowText(hCOP1W[count], RegisterValue);
				}
			}
			break;
		case COP1LRegisters:
			for (count = 0; count < 32; count++) {
				GetWindowText(hCOP1L[count], OldWinText, 60);
				if ((STATUS_REGISTER & STATUS_FR) == 0 && (count & 1) == 1) { // odd not alowed in half mode
					sprintf(RegisterValue, " -");
				}
				else {
					sprintf(RegisterValue, " %lld", *(QWORD*)FPRDoubleLocation[count]);
				}
				if (strcmp(RegisterValue, OldWinText) != 0) {
					SetWindowText(hCOP1L[count], RegisterValue);
				}
			}
			break;
		case SpecialRegister:
			GetWindowText(hSpecial[0],OldWinText,60);
			sprintf( RegisterValue," 0x%08X - %08X",PROGRAM_COUNTER.W[1], PROGRAM_COUNTER.W[0]);
			if ( strcmp( RegisterValue, OldWinText) != 0 ) {
				SetWindowText(hSpecial[0],RegisterValue);
			}			
			GetWindowText(hSpecial[0],OldWinText,60);
			sprintf(RegisterValue," 0x%08X - %08X",HI.W[1],HI.W[0]);
			if ( strcmp( RegisterValue, OldWinText) != 0 ) {
				SetWindowText(hSpecial[1],RegisterValue);
			}			
			GetWindowText(hSpecial[0],OldWinText,60);
			sprintf(RegisterValue," 0x%08X - %08X",LO.W[1],LO.W[0]);
			if ( strcmp( RegisterValue, OldWinText) != 0 ) {
				SetWindowText(hSpecial[2],RegisterValue);
			}			
			GetWindowText(hSpecial[0],OldWinText,60);
			sprintf(RegisterValue," 0x%08X",REVISION_REGISTER);
			if ( strcmp( RegisterValue, OldWinText) != 0 ) {
				SetWindowText(hSpecial[4],RegisterValue);
			}			
			GetWindowText(hSpecial[0],OldWinText,60);
			sprintf(RegisterValue," 0x%08X",FSTATUS_REGISTER);
			if ( strcmp( RegisterValue, OldWinText) != 0 ) {
				SetWindowText(hSpecial[5],RegisterValue);
			}			
			break;
		case RDRAMRegisters:
			for (count = 0; count < 10;count ++) {
				GetWindowText(hRDRam[count],OldWinText,60);
				sprintf(RegisterValue,"  0x%08X",(*RegRDRAM)[0][count]);
				if ( strcmp( RegisterValue, OldWinText) != 0 ) {
					SetWindowText(hRDRam[count],RegisterValue);
				}
			}
			break;
		case RDRAMRegisters2:
			for (count = 0; count < 10; count++) {
				GetWindowText(hRDRam2[count], OldWinText, 60);
				sprintf(RegisterValue, "  0x%08X", (*RegRDRAM)[1][count]);
				if (strcmp(RegisterValue, OldWinText) != 0) {
					SetWindowText(hRDRam2[count], RegisterValue);
				}
			}
			break;
		case ExpRDRAMRegisters:
			for (count = 0; count < 10; count++) {
				GetWindowText(hExpRDRam[count], OldWinText, 60);
				sprintf(RegisterValue, "  0x%08X", (*RegRDRAM)[2][count]);
				if (strcmp(RegisterValue, OldWinText) != 0) {
					SetWindowText(hExpRDRam[count], RegisterValue);
				}
			}
			break;
		case ExpRDRAMRegisters2:
			for (count = 0; count < 10; count++) {
				GetWindowText(hExpRDRam2[count], OldWinText, 60);
				sprintf(RegisterValue, "  0x%08X", (*RegRDRAM)[3][count]);
				if (strcmp(RegisterValue, OldWinText) != 0) {
					SetWindowText(hExpRDRam2[count], RegisterValue);
				}
			}
			break;
		case SPRegisters:
			for (count = 0; count < 10;count ++) {
				GetWindowText(hSP[count],OldWinText,60);
				sprintf(RegisterValue," 0x%08X",RegSP[count]);
				if ( strcmp( RegisterValue, OldWinText) != 0 ) {
					SetWindowText(hSP[count],RegisterValue);
				}
			}
			break;
		case MIPSInterface:
			for (count = 0; count < 4;count ++) {
				GetWindowText(hMI[count],OldWinText,60);
				sprintf(RegisterValue," 0x%08X",RegMI[count]);
				if ( strcmp( RegisterValue, OldWinText) != 0 ) {
					SetWindowText(hMI[count],RegisterValue);
				}
			}
			break;
		case VideoInterface:
			for (count = 0; count < 14;count ++) {
				GetWindowText(hVI[count],OldWinText,60);
				sprintf(RegisterValue," 0x%08X",RegVI[count]);
				if ( strcmp( RegisterValue, OldWinText) != 0 ) {
					SetWindowText(hVI[count],RegisterValue);
				}
			}
			break;
		case AudioInterface:
			for (count = 0; count < 6;count ++) {
				GetWindowText(hAI[count],OldWinText,60);
				sprintf(RegisterValue," 0x%08X",RegAI[count]);
				if ( strcmp( RegisterValue, OldWinText) != 0 ) {
					SetWindowText(hAI[count],RegisterValue);
				}
			}
			break;
		case PeripheralInterface:
			for (count = 0; count < 13;count ++) {
				GetWindowText(hPI[count],OldWinText,60);
				sprintf(RegisterValue," 0x%08X",RegPI[count]);
				if ( strcmp( RegisterValue, OldWinText) != 0 ) {
					SetWindowText(hPI[count],RegisterValue);
				}
			}
			break;
		case RDRAMInterface:
			for (count = 0; count < 8;count ++) {
				GetWindowText(hRI[count],OldWinText,60);
				sprintf(RegisterValue," 0x%08X",RegRI[count]);
				if ( strcmp( RegisterValue, OldWinText) != 0 ) {
					SetWindowText(hRI[count],RegisterValue);
				}
			}
			break;
		case SerialInterface:
			for (count = 0; count < 4;count ++) {
				GetWindowText(hSI[count],OldWinText,60);
				sprintf(RegisterValue," 0x%08X",RegSI[count]);
				if ( strcmp( RegisterValue, OldWinText) != 0 ) {
					SetWindowText(hSI[count],RegisterValue);
				}
			}
			break;
		}
	}
}
