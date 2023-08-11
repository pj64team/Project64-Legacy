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
#define LargeCompileBufferSize	0x03200000
#define NormalCompileBufferSize	0x01400000

extern DWORD *TLB_ReadMap, *TLB_WriteMap, RdramSize, SystemRdramSize;
extern BYTE *N64MEM, *RDRAM, *DMEM, *IMEM, *ROM;
extern void ** JumpTable, ** DelaySlotTable;
extern BYTE *RecompCode, *RecompPos;
extern BOOL WrittenToRom;

/* Memory Control */
int  Allocate_ROM                ( void );
int  Allocate_Memory             ( void );	
void Release_Memory              ( void );

/* CPU memory functions */
int  r4300i_CPU_MemoryFilter     ( DWORD dwExptCode, LPEXCEPTION_POINTERS lpEP );
int  r4300i_LB_NonMemory         ( DWORD PAddr, DWORD * Value, BOOL SignExtend );
BOOL r4300i_LB_VAddr             ( DWORD VAddr, BYTE * Value );
BOOL r4300i_LB_VAddr_NonCPU      ( DWORD VAddr, BYTE * Value );
BOOL r4300i_LD_VAddr             ( DWORD VAddr, unsigned _int64 * Value );
int  r4300i_LH_NonMemory         ( DWORD PAddr, DWORD * Value, int SignExtend );
BOOL r4300i_LH_VAddr             ( DWORD VAddr, WORD * Value );
BOOL r4300i_LH_VAddr_NonCPU      ( DWORD VAddr, WORD * Value );
int  r4300i_LW_NonMemory         ( DWORD PAddr, DWORD * Value );
BOOL r4300i_LW_VAddr             ( DWORD VAddr, DWORD * Value );
BOOL r4300i_LW_VAddr_NonCPU      ( DWORD VAddr, DWORD * Value );
int  r4300i_SB_NonMemory         ( DWORD PAddr, BYTE Value );
BOOL r4300i_SB_VAddr             ( DWORD VAddr, MIPS_DWORD* Value);
BOOL r4300i_SB_VAddr_NonCPU      ( DWORD VAddr, BYTE Value );
BOOL r4300i_SD_VAddr             ( DWORD VAddr, unsigned _int64 Value );
int  r4300i_SH_NonMemory         ( DWORD PAddr, WORD Value );
BOOL r4300i_SH_VAddr             ( DWORD VAddr, WORD Value );
BOOL r4300i_SH_VAddr_NonCPU      ( DWORD VAddr, WORD Value );
int  r4300i_SW_NonMemory         ( DWORD PAddr, DWORD Value );
BOOL r4300i_SW_VAddr             ( DWORD VAddr, DWORD Value );
BOOL r4300i_SW_VAddr_NonCPU      ( DWORD VAddr, DWORD Value );

/* Recompiler Memory Functions */
BOOL Compile_LB                  ( int Reg, DWORD Addr, BOOL SignExtend );
BOOL Compile_LH                  ( int Reg, DWORD Addr, BOOL SignExtend );
BOOL Compile_LW                  ( int Reg, DWORD Addr );
BOOL Compile_SB_Const            ( BYTE Value, DWORD Addr );
BOOL Compile_SB_Register         ( int x86Reg, DWORD Addr );
BOOL Compile_SH_Const            ( WORD Value, DWORD Addr );
BOOL Compile_SH_Register         ( int x86Reg, DWORD Addr );
BOOL Compile_SW_Const            ( DWORD Value, DWORD Addr );
BOOL Compile_SW_Register         ( int x86Reg, DWORD Addr );
void ResetMemoryStack            ( BLOCK_SECTION * Section );
void ResetRecompCode             ( void );

