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
#include "debugger.h"

#ifdef Log_x86Code

static HANDLE hCPULogFile = NULL;

void CPU_Message (char * Message, ...) {
	DWORD dwWritten;
	char Msg[400];
	
	va_list ap;
	va_start( ap, Message );
	vsprintf( Msg, Message, ap );
	va_end( ap );
	
	strcat(Msg,"\r\n");

	WriteFile( hCPULogFile,Msg,strlen(Msg),&dwWritten,NULL );
}

void Start_x86_Log (void) {
	char path_buffer[_MAX_PATH], drive[_MAX_DRIVE] ,dir[_MAX_DIR];
	char fname[_MAX_FNAME],ext[_MAX_EXT], LogFileName[_MAX_PATH];

	GetModuleFileName(NULL,path_buffer,sizeof(path_buffer));
	_splitpath( path_buffer, drive, dir, fname, ext );
   	_makepath( LogFileName, drive, dir, "CPUoutput", "log" );
		
	if (hCPULogFile) { Stop_x86_Log(); }
	hCPULogFile = CreateFile(LogFileName,GENERIC_WRITE, FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,
		CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	SetFilePointer(hCPULogFile,0,NULL,FILE_BEGIN);
}

void Stop_x86_Log (void) {
	if (hCPULogFile) {
		CloseHandle(hCPULogFile);
		hCPULogFile = NULL;
	}
}
#endif 
