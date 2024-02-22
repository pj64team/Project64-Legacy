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
typedef struct {
	BOOL		GenerateLog;

	/* Registers Log */
	BOOL	LogRDRamRegisters;
	BOOL	LogSPRegisters;
	BOOL	LogDPCRegisters;
	BOOL	LogDPSRegisters;
	BOOL	LogMIPSInterface;
	BOOL	LogVideoInterface;
	BOOL	LogAudioInterface;
	BOOL	LogPerInterface;
	BOOL	LogRDRAMInterface;
	BOOL	LogSerialInterface;

	/* Pif Ram Log */
  	BOOL	LogPRDMAOperations;
	BOOL	LogPRDirectMemLoads;  	
	BOOL	LogPRDMAMemLoads;  	
	BOOL	LogPRDirectMemStores;
	BOOL	LogPRDMAMemStores;
	BOOL	LogControllerPak;

	/* Special Log */
	BOOL	LogCP0changes;
	BOOL	LogCP0reads;
	BOOL	LogTLB;
	BOOL	LogExceptions;
	BOOL	NoInterrupts;
	BOOL	LogCache;
	BOOL	LogRomHeader;
	BOOL	LogUnknown;
	BOOL    LogISViewer;
} LOG_OPTIONS;

extern LOG_OPTIONS LogOptions;
extern const int FORCE_LOGGING;

void EnterLogOptions ( HWND hwndOwner );
void LoadLogOptions  ( LOG_OPTIONS * LogOptions, BOOL AlwaysFill );
void Log_LW          ( MIPS_DWORD PC, DWORD VAddr );
void __cdecl LogMessage      ( char * Message, ... );
void Log_SW          ( MIPS_DWORD PC, DWORD VAddr, DWORD Value );
void StartLog        ( void );
void StopLog         ( void );
