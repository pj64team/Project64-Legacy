/*
 * Project 64 Legacy - A Nintendo 64 emulator.
 *
 * (c) Copyright 2001 Zilmar, Jabo, Smiff, Gent, Witten 
 * (c) Copyright 2010 PJ64LegacyTeam
 *
 * Project64 Legacy Homepage: www.project64-legacy.com
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
#include <stdio.h>
#include "main.h"
#include "CPU.h"
#include "Debugger.h"
#include "plugin.h"
#include "settings.h"
#include "EmulateAI.h"
#include "resource.h"

char RspDLL[100], GfxDLL[100], AudioDLL[100],ControllerDLL[100], * PluginNames[MaxDlls];
HINSTANCE hAudioDll, hControllerDll, hGfxDll, hRspDll;
DWORD PluginCount, RspTaskValue, AudioIntrReg;
WORD RSPVersion,ContVersion;
HANDLE hAudioThread = NULL;
GFXDEBUG_INFO GFXDebug;
RSPDEBUG_INFO RspDebug;
CONTROL Controllers[4];
BOOL PluginsInitilized = FALSE;

BOOL PluginsChanged ( HWND hDlg );
BOOL ValidPluginVersion ( PLUGIN_INFO * PluginInfo );
volatile BOOL bTerminateAudioThread = FALSE;
volatile BOOL bAudioThreadExiting = FALSE;

void __cdecl AudioThread (void) {
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL );
	while (bTerminateAudioThread == 0) {
		AiUpdate(TRUE);
	}
	bAudioThreadExiting = TRUE;
}

void GetCurrentDlls (void) {
	char *read;

	Settings_Read(APPS_NAME, "Plugins", "RSP", DefaultRSPDll, &read);
	strncpy(RspDLL, read, sizeof(RspDLL));
	if (read) free(read);

	Settings_Read(APPS_NAME, "Plugins", "Graphics", DefaultGFXDll, &read);
	strncpy(GfxDLL, read, sizeof(GfxDLL));
	if (read) free(read);

	Settings_Read(APPS_NAME, "Plugins", "Audio", DefaultAudioDll, &read);
	strncpy(AudioDLL, read, sizeof(AudioDLL));
	if (read) free(read);

	Settings_Read(APPS_NAME, "Plugins", "Controller", DefaultControllerDll, &read);
	strncpy(ControllerDLL, read, sizeof(ControllerDLL));
	if (read) free(read);
}

BOOL LoadAudioDll(char * AudioDll) {
	PLUGIN_INFO PluginInfo;
	char DllName[300];

	Settings_GetDirectory(PluginDir, DllName, sizeof(DllName));
	strcat(DllName,AudioDll);

	hAudioDll = LoadLibrary(DllName);
	if (hAudioDll == NULL) {  return FALSE; }


	GetDllInfo = (void (__cdecl *)(PLUGIN_INFO *))GetProcAddress( hAudioDll, "GetDllInfo" );
	if (GetDllInfo == NULL) { return FALSE; }

	GetDllInfo(&PluginInfo);
	if (!ValidPluginVersion(&PluginInfo) || PluginInfo.MemoryBswaped == FALSE) { return FALSE; }

	AiCloseDLL = (void (__cdecl *)(void))GetProcAddress( hAudioDll, "CloseDLL" );
	if (AiCloseDLL == NULL) { return FALSE; }
	AiDacrateChanged = (void (__cdecl *)(int))GetProcAddress( hAudioDll, "AiDacrateChanged" );
	if (AiDacrateChanged == NULL) { return FALSE; }
	AiLenChanged = (void (__cdecl *)(void))GetProcAddress( hAudioDll, "AiLenChanged" );
	if (AiLenChanged == NULL) { return FALSE; }
	AiReadLength = (DWORD (__cdecl *)(void))GetProcAddress( hAudioDll, "AiReadLength" );
	if (AiReadLength == NULL) { return FALSE; }
	InitiateAudio = (BOOL (__cdecl *)(AUDIO_INFO))GetProcAddress( hAudioDll, "InitiateAudio" );
	if (InitiateAudio == NULL) { return FALSE; }
	AiRomOpen = (void(__cdecl*)(void))GetProcAddress(hAudioDll, "RomOpen");
	//if (AiRomOpen == NULL) { return FALSE; }
	AiRomClosed = (void (__cdecl *)(void))GetProcAddress( hAudioDll, "RomClosed" );
	if (AiRomClosed == NULL) { return FALSE; }
	ProcessAList = (void (__cdecl *)(void))GetProcAddress( hAudioDll, "ProcessAList" );	
	if (ProcessAList == NULL) { return FALSE; }

	AiDllConfig = (void (__cdecl *)(HWND))GetProcAddress( hAudioDll, "DllConfig" );
	AiUpdate = (void (__cdecl *)(BOOL))GetProcAddress( hAudioDll, "AiUpdate" );
	return TRUE;
}

BOOL LoadControllerDll(char * ControllerDll) {
	PLUGIN_INFO PluginInfo;
	char DllName[300];

	Settings_GetDirectory(PluginDir, DllName, sizeof(DllName));
	strcat(DllName,ControllerDll);

	hControllerDll = LoadLibrary(DllName);
	if (hControllerDll == NULL) {  return FALSE; }

	GetDllInfo = (void (__cdecl *)(PLUGIN_INFO *))GetProcAddress( hControllerDll, "GetDllInfo" );
	if (GetDllInfo == NULL) { return FALSE; }

	PluginInfo.MemoryBswaped = TRUE;
	PluginInfo.NormalMemory = TRUE;
	GetDllInfo(&PluginInfo);
	if (!ValidPluginVersion(&PluginInfo)|| PluginInfo.MemoryBswaped == FALSE) { return FALSE; }
	ContVersion = PluginInfo.Version;

	ContCloseDLL = (void (__cdecl *)(void))GetProcAddress( hControllerDll, "CloseDLL" );
	if (ContCloseDLL == NULL) { return FALSE; }

	if (ContVersion == 0x0100) {
		InitiateControllers_1_0 = (void (__cdecl *)(HWND, CONTROL *))GetProcAddress( hControllerDll, "InitiateControllers" );
		if (InitiateControllers_1_0 == NULL) { return FALSE; }
	}
	if (ContVersion == 0x0101) {
		InitiateControllers_1_1 = (void (__cdecl *)(CONTROL_INFO))GetProcAddress( hControllerDll, "InitiateControllers" );
		if (InitiateControllers_1_1 == NULL) { return FALSE; }
	}

	ControllerCommand = (void (__cdecl *)(int, BYTE *))GetProcAddress( hControllerDll, "ControllerCommand" );
	ReadController = (void (__cdecl *)(int, BYTE *))GetProcAddress( hControllerDll, "ReadController" );
	ContConfig = (void (__cdecl *)(HWND))GetProcAddress( hControllerDll, "DllConfig" );

	GetKeys = (void (__cdecl *)(int, BUTTONS *))GetProcAddress( hControllerDll, "GetKeys" );
	WM_KeyDown = (void (__cdecl *)(WPARAM,LPARAM))GetProcAddress( hControllerDll, "WM_KeyDown" );
	WM_KeyUp = (void (__cdecl *)(WPARAM,LPARAM))GetProcAddress( hControllerDll, "WM_KeyUp" );

	ContRomOpen = (void (__cdecl *)(void))GetProcAddress( hControllerDll, "RomOpen" );
	ContRomClosed = (void (__cdecl *)(void))GetProcAddress( hControllerDll, "RomClosed" );

	RumbleCommand = (void (__cdecl *)(int, BOOL))GetProcAddress( hControllerDll, "RumbleCommand" );
	return TRUE;
}

BOOL LoadGFXDll(char * GfxDll) {
	PLUGIN_INFO PluginInfo;
	char DllName[300];

	Settings_GetDirectory(PluginDir, DllName, sizeof(DllName));
	strcat(DllName,GfxDll);

	hGfxDll = LoadLibrary(DllName);
	if (hGfxDll == NULL) {return FALSE;}

	GetDllInfo = (void (__cdecl *)(PLUGIN_INFO *))GetProcAddress( hGfxDll, "GetDllInfo" );
	if (GetDllInfo == NULL) { return FALSE; }

	GetDllInfo(&PluginInfo);
	if (!ValidPluginVersion(&PluginInfo) || PluginInfo.MemoryBswaped == FALSE) { return FALSE; }

	GFXCloseDLL = (void (__cdecl *)(void))GetProcAddress( hGfxDll, "CloseDLL" );
	if (GFXCloseDLL == NULL) { return FALSE; }
	ChangeWindow = (void (__cdecl *)(void))GetProcAddress( hGfxDll, "ChangeWindow" );
	if (ChangeWindow == NULL) { return FALSE; }
	GFXDllConfig = (void (__cdecl *)(HWND))GetProcAddress( hGfxDll, "DllConfig" );
	DrawScreen = (void (__cdecl *)(void))GetProcAddress( hGfxDll, "DrawScreen" );
	if (DrawScreen == NULL) { return FALSE; }
	InitiateGFX = (BOOL (__cdecl *)(GFX_INFO))GetProcAddress( hGfxDll, "InitiateGFX" );
	if (InitiateGFX == NULL) { return FALSE; }
	MoveScreen = (void (__cdecl *)(int, int))GetProcAddress( hGfxDll, "MoveScreen" );
	if (MoveScreen == NULL) { return FALSE; }
	ProcessDList = (void (__cdecl *)(void))GetProcAddress( hGfxDll, "ProcessDList" );
	if (ProcessDList == NULL) { return FALSE; }
	GfxRomClosed = (void(__cdecl*)(void))GetProcAddress(hGfxDll, "RomClosed");
	if (GfxRomClosed == NULL) { return FALSE; }
	GfxRomOpen = (void (__cdecl *)(void))GetProcAddress( hGfxDll, "RomOpen" );
	if (GfxRomOpen == NULL) { return FALSE; }
	UpdateScreen = (void (__cdecl *)(void))GetProcAddress( hGfxDll, "UpdateScreen" );
	if (UpdateScreen == NULL) { return FALSE; }
	ViStatusChanged = (void (__cdecl *)(void))GetProcAddress( hGfxDll, "ViStatusChanged" );
	if (ViStatusChanged == NULL) { return FALSE; }
	ViWidthChanged = (void (__cdecl *)(void))GetProcAddress( hGfxDll, "ViWidthChanged" );
	if (ViWidthChanged == NULL) { return FALSE; }
	
	if (PluginInfo.Version >= 0x0103 ){
		ProcessRDPList = (void (__cdecl *)(void))GetProcAddress( hGfxDll, "ProcessRDPList" );
		if (ProcessRDPList == NULL) { return FALSE; }
		CaptureScreen = (void (__cdecl *)(char *))GetProcAddress( hGfxDll, "CaptureScreen" );
		if (CaptureScreen == NULL) { return FALSE; }
		ShowCFB = (void (__cdecl *)(void))GetProcAddress( hGfxDll, "ShowCFB" );
		if (ShowCFB == NULL) { return FALSE; }
		GetGfxDebugInfo = (void (__cdecl *)(GFXDEBUG_INFO *))GetProcAddress( hGfxDll, "GetGfxDebugInfo" );
		InitiateGFXDebugger = (void (__cdecl *)(DEBUG_INFO))GetProcAddress( hGfxDll, "InitiateGFXDebugger" );
	} else {
		ProcessRDPList = NULL;
		CaptureScreen = NULL;
		ShowCFB = NULL;
		GetGfxDebugInfo = NULL;
		InitiateGFXDebugger = NULL;
	}
#ifdef CFB_READ
	FrameBufferRead = (void (__cdecl *)(DWORD))GetProcAddress( hGfxDll, "FBRead" );
	FrameBufferWrite = (void (__cdecl *)(DWORD, DWORD))GetProcAddress( hGfxDll, "FBWrite" );
#endif
	return TRUE;
}

BOOL LoadRSPDll(char * RspDll) {
	PLUGIN_INFO PluginInfo;
	char DllName[300];

	Settings_GetDirectory(PluginDir, DllName, sizeof(DllName));
	strcat(DllName,RspDll);

	RspDebug.UseBPoints = FALSE;

	hRspDll = LoadLibrary(DllName);
	if (hRspDll == NULL) {  return FALSE; }

	GetDllInfo = (void (__cdecl *)(PLUGIN_INFO *))GetProcAddress( hRspDll, "GetDllInfo" );
	if (GetDllInfo == NULL) { return FALSE; }

	GetDllInfo(&PluginInfo);
	if (!ValidPluginVersion(&PluginInfo) || PluginInfo.MemoryBswaped == FALSE) { return FALSE; }
	RSPVersion = PluginInfo.Version;
	if (RSPVersion == 1) { RSPVersion = 0x0100; }

	DoRspCycles = (DWORD (__cdecl *)(DWORD))GetProcAddress( hRspDll, "DoRspCycles" );
	if (DoRspCycles == NULL) { return FALSE; }
	InitiateRSP_1_0 = NULL;
	InitiateRSP_1_1 = NULL;
	if (RSPVersion == 0x100) {
		InitiateRSP_1_0 = (void (__cdecl *)(RSP_INFO_1_0,DWORD *))GetProcAddress( hRspDll, "InitiateRSP" );
		if (InitiateRSP_1_0 == NULL) { return FALSE; }
	}
	if (RSPVersion >= 0x101) {
		InitiateRSP_1_1 = (void(__cdecl*)(RSP_INFO_1_1, DWORD*))GetProcAddress(hRspDll, "InitiateRSP");
		if (InitiateRSP_1_1 == NULL) { return FALSE; }
	}
	RSPRomOpen = (void(__cdecl*)(void))GetProcAddress(hRspDll, "RomOpen");
	//if (RSPRomOpen == NULL) { return FALSE; }
	RSPRomClosed = (void (__cdecl *)(void))GetProcAddress( hRspDll, "RomClosed" );
	if (RSPRomClosed == NULL) { return FALSE; }
	RSPCloseDLL = (void (__cdecl *)(void))GetProcAddress( hRspDll, "CloseDLL" );
	if (RSPCloseDLL == NULL) { return FALSE; }
	GetRspDebugInfo = (void (__cdecl *)(RSPDEBUG_INFO *))GetProcAddress( hRspDll, "GetRspDebugInfo" );
	InitiateRSPDebugger = (void (__cdecl *)(DEBUG_INFO))GetProcAddress( hRspDll, "InitiateRSPDebugger" );
	RSPDllConfig = (void (__cdecl *)(HWND))GetProcAddress( hRspDll, "DllConfig" );
	SetRomHeader = (DWORD(__cdecl*)(BYTE*))GetProcAddress(hRspDll, "SetRomHeader");

	return TRUE;
}

void TerminateAudioThread()
{
	int sleepCnt = 0;
	if (AiCloseDLL != NULL) {
		AiCloseDLL();
	}

	while (AiUpdate != NULL && !bAudioThreadExiting) {
		bTerminateAudioThread = TRUE;
		Sleep(10);
		sleepCnt++;

		if (!bAudioThreadExiting && sleepCnt >= 50) {
			//DisplayError("Audio thread failed to stop gracefully");

			// This is a last resort when the audio thread refuses to gracefully exit.
			// Calling this function WILL cause problems!
			// SEE: https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-terminatethread#remarks
			TerminateThread(hAudioThread, 0);
			break;
		}
	}
	FreeLibrary(hAudioDll);

	AiCloseDLL = NULL;
	AiDacrateChanged = NULL;
	AiLenChanged = NULL;
	AiReadLength = NULL;
	AiUpdate = NULL;
	InitiateAudio = NULL;
	ProcessAList = NULL;
	AiRomClosed = NULL;
	PluginsInitilized = FALSE;
}

void ResetAudio(HWND hWnd) {
	static DWORD AI_DUMMY = 0;

	TerminateAudioThread();
	
	if (!LoadAudioDll(AudioDLL) ) {
		AiCloseDLL       = NULL;
		AiDacrateChanged = NULL;
		AiLenChanged     = NULL;
		AiReadLength     = NULL;
		AiUpdate         = NULL;
		InitiateAudio    = NULL;
		ProcessAList     = NULL;
		AiRomClosed      = NULL;
		DisplayError(GS(MSG_FAIL_INIT_AUDIO));
		PluginsInitilized = FALSE;
	} else {
		AUDIO_INFO AudioInfo;
		
		AudioInfo.hwnd = hWnd;
		AudioInfo.hinst = hInst;
		AudioInfo.MemoryBswaped = TRUE;
		AudioInfo.HEADER = (BYTE *)RomHeader;
		AudioInfo.RDRAM = N64MEM;
		AudioInfo.DMEM = DMEM;
		AudioInfo.IMEM = IMEM;
		if (EmulateAI == TRUE) AudioInfo.MI__INTR_REG = &AI_DUMMY; 
		else AudioInfo.MI__INTR_REG = &AudioIntrReg;
		AudioInfo.AI__DRAM_ADDR_REG = &AI_DRAM_ADDR_REG;	
		AudioInfo.AI__LEN_REG = &AI_LEN_REG;	
		AudioInfo.AI__CONTROL_REG = &AI_CONTROL_REG;	
		if (EmulateAI == TRUE) AudioInfo.AI__STATUS_REG =  &AI_DUMMY; 
		else AudioInfo.AI__STATUS_REG =  &AI_STATUS_REG;	
		AudioInfo.AI__DACRATE_REG = &AI_DACRATE_REG;	
		AudioInfo.AI__BITRATE_REG = &AI_BITRATE_REG;	
		AudioInfo.CheckInterrupts = AiCheckInterrupts;
		if (EmulateAI == TRUE) EmuAI_InitializePluginHook();
		if (!InitiateAudio(AudioInfo)) {
			AiCloseDLL       = NULL;
			AiDacrateChanged = NULL;
			AiLenChanged     = NULL;
			AiReadLength     = NULL;
			AiUpdate         = NULL;
			InitiateAudio    = NULL;
			ProcessAList     = NULL;
			AiRomClosed      = NULL;
			DisplayError(GS(MSG_FAIL_INIT_AUDIO));
			PluginsInitilized = FALSE;
		}
		if (AiUpdate) { 
			DWORD ThreadID;
			bTerminateAudioThread = FALSE;
			bAudioThreadExiting = FALSE;
			hAudioThread = CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)AudioThread, (LPVOID)NULL,0, &ThreadID);
		}
	}
}

void SetupPlugins (HWND hWnd) {
	static DWORD AI_DUMMY = 0;
	if (PluginsInitilized)
		return;
	ShutdownPlugins();
	GetCurrentDlls();

	PluginsInitilized = TRUE;

	if (!LoadGFXDll(GfxDLL)) { 
		DisplayError(GS(MSG_FAIL_INIT_GFX));
		PluginsInitilized = FALSE;
	} else { 
		GFX_INFO GfxInfo;

		GfxInfo.MemoryBswaped = TRUE;
		GfxInfo.CheckInterrupts = CheckInterrupts;
		GfxInfo.hStatusBar = hStatusWnd;
		GfxInfo.hWnd = hWnd;
		GfxInfo.HEADER = (BYTE *)RomHeader;
		GfxInfo.RDRAM = N64MEM;
		GfxInfo.DMEM = DMEM;
		GfxInfo.IMEM = IMEM;
		GfxInfo.MI__INTR_REG = &MI_INTR_REG;	
		GfxInfo.DPC__START_REG = &DPC_START_REG;
		GfxInfo.DPC__END_REG = &DPC_END_REG;
		GfxInfo.DPC__CURRENT_REG = &DPC_CURRENT_REG;
		GfxInfo.DPC__STATUS_REG = &DPC_STATUS_REG;
		GfxInfo.DPC__CLOCK_REG = &DPC_CLOCK_REG;
		GfxInfo.DPC__BUFBUSY_REG = &DPC_BUFBUSY_REG;
		GfxInfo.DPC__PIPEBUSY_REG = &DPC_PIPEBUSY_REG;
		GfxInfo.DPC__TMEM_REG = &DPC_TMEM_REG;
		GfxInfo.VI__STATUS_REG = &VI_STATUS_REG;
		GfxInfo.VI__ORIGIN_REG = &VI_ORIGIN_REG;
		GfxInfo.VI__WIDTH_REG = &VI_WIDTH_REG;
		GfxInfo.VI__INTR_REG = &VI_INTR_REG;
		GfxInfo.VI__V_CURRENT_LINE_REG = &VI_CURRENT_REG;
		GfxInfo.VI__TIMING_REG = &VI_TIMING_REG;
		GfxInfo.VI__V_SYNC_REG = &VI_V_SYNC_REG;
		GfxInfo.VI__H_SYNC_REG = &VI_H_SYNC_REG;
		GfxInfo.VI__LEAP_REG = &VI_LEAP_REG;
		GfxInfo.VI__H_START_REG = &VI_H_START_REG;
		GfxInfo.VI__V_START_REG = &VI_V_START_REG;
		GfxInfo.VI__V_BURST_REG = &VI_V_BURST_REG;
		GfxInfo.VI__X_SCALE_REG = &VI_X_SCALE_REG;
		GfxInfo.VI__Y_SCALE_REG = &VI_Y_SCALE_REG;
		
		if (!InitiateGFX(GfxInfo)) {
			DisplayError(GS(MSG_FAIL_INIT_GFX));
			PluginsInitilized = FALSE;
		}
	}

	if (!LoadAudioDll(AudioDLL) ) {
		AiCloseDLL       = NULL;
		AiDacrateChanged = NULL;
		AiLenChanged     = NULL;
		AiReadLength     = NULL;
		AiUpdate         = NULL;
		InitiateAudio    = NULL;
		ProcessAList     = NULL;
		AiRomClosed      = NULL;
		DisplayError(GS(MSG_FAIL_INIT_AUDIO));
		PluginsInitilized = FALSE;
	} else {
		AUDIO_INFO AudioInfo;
		
		AudioInfo.hwnd = hWnd;
		AudioInfo.hinst = hInst;
		AudioInfo.MemoryBswaped = TRUE;
		AudioInfo.HEADER = (BYTE *)RomHeader;
		AudioInfo.RDRAM = N64MEM;
		AudioInfo.DMEM = DMEM;
		AudioInfo.IMEM = IMEM;
		if (EmulateAI == TRUE) AudioInfo.MI__INTR_REG = &AI_DUMMY; 
		else AudioInfo.MI__INTR_REG = &AudioIntrReg;
		AudioInfo.AI__DRAM_ADDR_REG = &AI_DRAM_ADDR_REG;	
		AudioInfo.AI__LEN_REG = &AI_LEN_REG;	
		AudioInfo.AI__CONTROL_REG = &AI_CONTROL_REG;	
		if (EmulateAI == TRUE) AudioInfo.AI__STATUS_REG =  &AI_DUMMY; 
		else AudioInfo.AI__STATUS_REG =  &AI_STATUS_REG;	
		AudioInfo.AI__DACRATE_REG = &AI_DACRATE_REG;	
		AudioInfo.AI__BITRATE_REG = &AI_BITRATE_REG;	
		AudioInfo.CheckInterrupts = AiCheckInterrupts;
		if (EmulateAI == TRUE) EmuAI_InitializePluginHook();
		if (!InitiateAudio(AudioInfo)) {
			AiCloseDLL = NULL;
			AiDacrateChanged = NULL;
			AiLenChanged = NULL;
			AiReadLength = NULL;
			AiUpdate = NULL;
			InitiateAudio = NULL;
			ProcessAList = NULL;
			AiRomClosed = NULL;
			DisplayError(GS(MSG_FAIL_INIT_AUDIO));
			PluginsInitilized = FALSE;
		}
		if (AiUpdate) {
			DWORD ThreadID;
			bTerminateAudioThread = FALSE;
			bAudioThreadExiting = FALSE;
			hAudioThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)AudioThread, (LPVOID)NULL, 0, &ThreadID);
		}
	}

	if (!LoadRSPDll(RspDLL)) { 
		DisplayError(GS(MSG_FAIL_INIT_RSP));
		PluginsInitilized = FALSE;
	}
	else {
		RSP_INFO_1_0 RspInfo10;
		RSP_INFO_1_1 RspInfo11;

		RspInfo10.CheckInterrupts = CheckInterrupts;
		RspInfo11.CheckInterrupts = CheckInterrupts;

		RspInfo10.ProcessDlist = ProcessDList;
		RspInfo11.ProcessDlist = ProcessDList;

		RspInfo10.ProcessAlist = ProcessAList;
		RspInfo11.ProcessAlist = ProcessAList;

		RspInfo10.ProcessRdpList = ProcessRDPList;
		RspInfo11.ProcessRdpList = ProcessRDPList;

		RspInfo11.ShowCFB = ShowCFB;

		//RspInfo12.HEADER = (BYTE*)RomHeader;
		//RspInfo12.Version = 0x102;

		RspInfo10.hInst = hInst;
		RspInfo11.hInst = hInst;

		RspInfo10.RDRAM = N64MEM;
		RspInfo11.RDRAM = N64MEM;

		RspInfo10.DMEM = DMEM;
		RspInfo11.DMEM = DMEM;

		RspInfo10.IMEM = IMEM;
		RspInfo11.IMEM = IMEM;

		RspInfo10.MemoryBswaped = FALSE;
		RspInfo11.MemoryBswaped = FALSE;

		RspInfo10.MI__INTR_REG = &MI_INTR_REG;
		RspInfo11.MI__INTR_REG = &MI_INTR_REG;

		RspInfo10.SP__MEM_ADDR_REG = &SP_MEM_ADDR_REG;
		RspInfo11.SP__MEM_ADDR_REG = &SP_MEM_ADDR_REG;

		RspInfo10.SP__DRAM_ADDR_REG = &SP_DRAM_ADDR_REG;
		RspInfo11.SP__DRAM_ADDR_REG = &SP_DRAM_ADDR_REG;

		RspInfo10.SP__RD_LEN_REG = &SP_RD_LEN_REG;
		RspInfo11.SP__RD_LEN_REG = &SP_RD_LEN_REG;

		RspInfo10.SP__WR_LEN_REG = &SP_WR_LEN_REG;
		RspInfo11.SP__WR_LEN_REG = &SP_WR_LEN_REG;

		RspInfo10.SP__STATUS_REG = &SP_STATUS_REG;
		RspInfo11.SP__STATUS_REG = &SP_STATUS_REG;

		RspInfo10.SP__DMA_FULL_REG = &SP_DMA_FULL_REG;
		RspInfo11.SP__DMA_FULL_REG = &SP_DMA_FULL_REG;

		RspInfo10.SP__DMA_BUSY_REG = &SP_DMA_BUSY_REG;
		RspInfo11.SP__DMA_BUSY_REG = &SP_DMA_BUSY_REG;

		RspInfo10.SP__PC_REG = &SP_PC_REG;
		RspInfo11.SP__PC_REG = &SP_PC_REG;

		RspInfo10.SP__SEMAPHORE_REG = &SP_SEMAPHORE_REG;
		RspInfo11.SP__SEMAPHORE_REG = &SP_SEMAPHORE_REG;

		RspInfo10.DPC__START_REG = &DPC_START_REG;
		RspInfo11.DPC__START_REG = &DPC_START_REG;

		RspInfo10.DPC__END_REG = &DPC_END_REG;
		RspInfo11.DPC__END_REG = &DPC_END_REG;

		RspInfo10.DPC__CURRENT_REG = &DPC_CURRENT_REG;
		RspInfo11.DPC__CURRENT_REG = &DPC_CURRENT_REG;

		RspInfo10.DPC__STATUS_REG = &DPC_STATUS_REG;
		RspInfo11.DPC__STATUS_REG = &DPC_STATUS_REG;

		RspInfo10.DPC__CLOCK_REG = &DPC_CLOCK_REG;
		RspInfo11.DPC__CLOCK_REG = &DPC_CLOCK_REG;

		RspInfo10.DPC__BUFBUSY_REG = &DPC_BUFBUSY_REG;
		RspInfo11.DPC__BUFBUSY_REG = &DPC_BUFBUSY_REG;

		RspInfo10.DPC__PIPEBUSY_REG = &DPC_PIPEBUSY_REG;
		RspInfo11.DPC__PIPEBUSY_REG = &DPC_PIPEBUSY_REG;

		RspInfo10.DPC__TMEM_REG = &DPC_TMEM_REG;
		RspInfo11.DPC__TMEM_REG = &DPC_TMEM_REG;

		switch (RSPVersion)
		{
		case 0x0100:
			InitiateRSP_1_0(RspInfo10, &RspTaskValue);
			break;
		case 0x0101:
		case 0x0102:
			InitiateRSP_1_1(RspInfo11, &RspTaskValue);
			break;
		default:
			InitiateRSP_1_0(RspInfo10, &RspTaskValue);
			break;
		}

		if (((DWORD)SetRomHeader) != NULL)
		{
			SetRomHeader((BYTE*)RomHeader);
		}
	}
	
	if (HaveDebugger) {
		DEBUG_INFO DebugInfo;

		if (GetRspDebugInfo != NULL) { GetRspDebugInfo(&RspDebug); }				
		if (GetGfxDebugInfo != NULL) { GetGfxDebugInfo(&GFXDebug); }
		
		DebugInfo.UpdateBreakPoints = RefreshBreakPoints;
		DebugInfo.UpdateMemory = Refresh_Memory;
		DebugInfo.UpdateR4300iRegisters = UpdateCurrentR4300iRegisterPanel;
		DebugInfo.Enter_BPoint_Window = Enter_BPoint_Window;
		DebugInfo.Enter_Memory_Window = Enter_Memory_Window;
		DebugInfo.Enter_R4300i_Commands_Window = Enter_R4300i_Commands_Window;
		DebugInfo.Enter_R4300i_Register_Window = Enter_R4300i_Register_Window;
		DebugInfo.Enter_RSP_Commands_Window = RspDebug.Enter_RSP_Commands_Window;
		if (InitiateRSPDebugger != NULL) { InitiateRSPDebugger(DebugInfo); }
		if (InitiateGFXDebugger != NULL) { InitiateGFXDebugger(DebugInfo); }
	}

	if (!LoadControllerDll(ControllerDLL)) { 
		DisplayError(GS(MSG_FAIL_INIT_CONTROL));
		PluginsInitilized = FALSE;
	} else {
		Controllers[0].Present = FALSE;
		Controllers[0].RawData = FALSE;
		Controllers[0].Plugin  = PLUGIN_NONE;
		
		Controllers[1].Present = FALSE;
		Controllers[1].RawData = FALSE;
		Controllers[1].Plugin  = PLUGIN_NONE;
		
		Controllers[2].Present = FALSE;
		Controllers[2].RawData = FALSE;
		Controllers[2].Plugin  = PLUGIN_NONE;
		
		Controllers[3].Present = FALSE;
		Controllers[3].RawData = FALSE;
		Controllers[3].Plugin  = PLUGIN_NONE;
	
		if (ContVersion == 0x0100) {
			InitiateControllers_1_0(hWnd, Controllers);
		}
		if (ContVersion == 0x0101) {
			CONTROL_INFO ControlInfo;
			ControlInfo.Controls = Controllers;
			ControlInfo.HEADER = (BYTE*)RomHeader;
			ControlInfo.hinst = hInst;
			ControlInfo.hMainWindow = hWnd;
			ControlInfo.MemoryBswaped = TRUE;
			InitiateControllers_1_1(ControlInfo);
		}
#ifndef EXTERNAL_RELEASE
//		Controllers[0].Plugin  = PLUGIN_RUMBLE_PAK;
#endif
	}
	if (!PluginsInitilized) { ChangeSettings(hMainWindow); }
}

void SetupPluginScreen (HWND hDlg) {
	WIN32_FIND_DATA FindData;
	PLUGIN_INFO PluginInfo;
	char SearchsStr[300], Plugin[300];
	HANDLE hFind;
	HMODULE hLib;
	int index;

	SetDlgItemText(hDlg,RSP_ABOUT,GS(PLUG_ABOUT));
	SetDlgItemText(hDlg,GFX_ABOUT,GS(PLUG_ABOUT));
	SetDlgItemText(hDlg,AUDIO_ABOUT,GS(PLUG_ABOUT));
	SetDlgItemText(hDlg,CONT_ABOUT,GS(PLUG_ABOUT));

	SetDlgItemText(hDlg,IDC_RSP_NAME,GS(PLUG_RSP));
	SetDlgItemText(hDlg,IDC_GFX_NAME,GS(PLUG_GFX));
	SetDlgItemText(hDlg,IDC_AUDIO_NAME,GS(PLUG_AUDIO));
	SetDlgItemText(hDlg,IDC_CONT_NAME,GS(PLUG_CTRL));
	
	Settings_GetDirectory(PluginDir, SearchsStr, sizeof(SearchsStr));
	strcat(SearchsStr,"*.dll");
	hFind = FindFirstFile(SearchsStr, &FindData);
	if (hFind == INVALID_HANDLE_VALUE) { return; }
	PluginCount = 0;
	for (;;) {
		PluginNames[PluginCount] = (char *) malloc(strlen(FindData.cFileName) + 1);
		strcpy(PluginNames[PluginCount],FindData.cFileName);
		Settings_GetDirectory(PluginDir, Plugin, sizeof(Plugin));
		strcat(Plugin,PluginNames[PluginCount]);
		hLib = LoadLibrary(Plugin);
		if (hLib == NULL) {
			FreeLibrary(hLib);
			free(PluginNames[PluginCount]);
			PluginNames[PluginCount] = 0;
			DisplayError("%s\n %s",GS(MSG_FAIL_LOAD_PLUGIN),Plugin); 
			if (FindNextFile(hFind,&FindData) == 0) { return; }
			continue;
		}
		GetDllInfo = (void (__cdecl *)(PLUGIN_INFO *))GetProcAddress( hLib, "GetDllInfo" );
		if (GetDllInfo == NULL) {
			FreeLibrary(hLib);
			free(PluginNames[PluginCount]);
			PluginNames[PluginCount] = 0;
			if (FindNextFile(hFind,&FindData) == 0) { return; }
			continue; 
		}
		GetDllInfo(&PluginInfo);
		if (!ValidPluginVersion(&PluginInfo) || 
			(PluginInfo.Type != PLUGIN_TYPE_CONTROLLER && PluginInfo.MemoryBswaped == FALSE))
		{
			FreeLibrary(hLib);
			free(PluginNames[PluginCount]);
			PluginNames[PluginCount] = 0;
			if (FindNextFile(hFind,&FindData) == 0) { return; }
			continue;
		}


		switch(PluginInfo.Type) {
		case PLUGIN_TYPE_RSP:
			index = SendMessage(GetDlgItem(hDlg,RSP_LIST),CB_ADDSTRING,(WPARAM)0, (LPARAM)&PluginInfo.Name);		
			SendMessage(GetDlgItem(hDlg,RSP_LIST),CB_SETITEMDATA ,(WPARAM)index, (LPARAM)PluginCount);		
			if(_stricmp(RspDLL,PluginNames[PluginCount]) == 0) {
				SendMessage(GetDlgItem(hDlg,RSP_LIST),CB_SETCURSEL,(WPARAM)index,(LPARAM)0);
				RSPDllAbout = (void (__cdecl *)(HWND))GetProcAddress( hLib, "DllAbout" );
				EnableWindow(GetDlgItem(hDlg,RSP_ABOUT),RSPDllAbout != NULL ? TRUE:FALSE);
			} else {
				FreeLibrary(hLib);
			}
			break;
		case PLUGIN_TYPE_GFX:
			index = SendMessage(GetDlgItem(hDlg,GFX_LIST),CB_ADDSTRING,(WPARAM)0, (LPARAM)&PluginInfo.Name);		
			SendMessage(GetDlgItem(hDlg,GFX_LIST),CB_SETITEMDATA ,(WPARAM)index, (LPARAM)PluginCount);		
			if(_stricmp(GfxDLL,PluginNames[PluginCount]) == 0) {
				SendMessage(GetDlgItem(hDlg,GFX_LIST),CB_SETCURSEL,(WPARAM)index,(LPARAM)0);
				GFXDllAbout = (void (__cdecl *)(HWND))GetProcAddress( hLib, "DllAbout" );
				EnableWindow(GetDlgItem(hDlg,GFX_ABOUT),GFXDllAbout != NULL ? TRUE:FALSE);
			} else {
				FreeLibrary(hLib);
			}
			break;
		case PLUGIN_TYPE_AUDIO:
			index = SendMessage(GetDlgItem(hDlg,AUDIO_LIST),CB_ADDSTRING,(WPARAM)0, (LPARAM)&PluginInfo.Name);
			SendMessage(GetDlgItem(hDlg,AUDIO_LIST),CB_SETITEMDATA ,(WPARAM)index, (LPARAM)PluginCount);		
			if(_stricmp(AudioDLL,PluginNames[PluginCount]) == 0) {
				SendMessage(GetDlgItem(hDlg,AUDIO_LIST),CB_SETCURSEL,(WPARAM)index,(LPARAM)0);
				AiDllAbout = (void (__cdecl *)(HWND))GetProcAddress( hLib, "DllAbout" );
				EnableWindow(GetDlgItem(hDlg,AUDIO_ABOUT),AiDllAbout != NULL ? TRUE:FALSE);
			} else {
				FreeLibrary(hLib);
			}
			break;
		case PLUGIN_TYPE_CONTROLLER:
			index = SendMessage(GetDlgItem(hDlg,CONT_LIST),CB_ADDSTRING,(WPARAM)0, (LPARAM)&PluginInfo.Name);		
			SendMessage(GetDlgItem(hDlg,CONT_LIST),CB_SETITEMDATA ,(WPARAM)index, (LPARAM)PluginCount);		
			if(_stricmp(ControllerDLL,PluginNames[PluginCount]) == 0) {
				SendMessage(GetDlgItem(hDlg,CONT_LIST),CB_SETCURSEL,(WPARAM)index,(LPARAM)0);
				ContDllAbout = (void (__cdecl *)(HWND))GetProcAddress( hLib, "DllAbout" );
				EnableWindow(GetDlgItem(hDlg,CONT_ABOUT),ContDllAbout != NULL ? TRUE:FALSE);
			} else {
				FreeLibrary(hLib);
			}
			break;
		}
		PluginCount += 1;
		if (FindNextFile(hFind,&FindData) == 0) { return; }
	}
}

void ShutdownPlugins(void) {
	unsigned int names;

	if (!PluginsInitilized)
		return;

	for (names = 0; names < PluginCount; names++) {
		free(PluginNames[names]);
		PluginNames[names] = 0;
	}

	TerminateAudioThread();
	if (GFXCloseDLL != NULL) { GFXCloseDLL(); }
	if (RSPCloseDLL != NULL) { RSPCloseDLL(); }
	if (ContCloseDLL != NULL) { ContCloseDLL(); }
	FreeLibrary(hControllerDll);
	FreeLibrary(hGfxDll);
	FreeLibrary(hRspDll);
	PluginsInitilized = FALSE;
}

BOOL ValidPluginVersion ( PLUGIN_INFO * PluginInfo ) {
	switch (PluginInfo->Type) {
	case PLUGIN_TYPE_RSP: 
		if (PluginInfo->Version == 0x0001) { return TRUE; }
		if (PluginInfo->Version == 0x0100) { return TRUE; }
		if (PluginInfo->Version == 0x0101) { return TRUE; }
		//if (PluginInfo->Version == 0x0102) { return TRUE; }
		break;
	case PLUGIN_TYPE_GFX:
		if (PluginInfo->Version == 0x0102) { return TRUE; }
		if (PluginInfo->Version == 0x0103) { return TRUE; }
		break;
	case PLUGIN_TYPE_AUDIO:
		if (PluginInfo->Version == 0x0101) { return TRUE; }
		break;
	case PLUGIN_TYPE_CONTROLLER:
		if (PluginInfo->Version == 0x0100) { return TRUE; }
		if (PluginInfo->Version == 0x0101) { return TRUE; }		// This was missing for some reason but we do have initialization code
		break;
	}
	return FALSE;
}
