#ifndef CS_DEV_H
#define CS_DEV_H

#include "CheatSearch_Search.h"

// The single code entry currently being tested/made
typedef struct CODEENTRY {
	DWORD Address;
	WORD Value;
	char Note[STRING_MAX];
	char Name[STRING_MAX];
	char Text[STRING_MAX];
	BYTE Enabled;
	BYTE Activator;
	NUMBITS numBits;
	SEARCHBY searchBy;
} CODEENTRY;

// Used to write the results to file
// Later will be used to read back to memory for crash recovery and accidental closure
// May be dumped at a later date
typedef struct LASTSEARCH {
	char *SearchType;
	char *SearchValue;
	NUMBITS NumBits;
	SEARCHTYPE ValueSearchType;
	char *Results;
} LASTSEARCH;

// Grouping of all dev entry variables
// Must be deallocated by calling CS_ClearDev
typedef struct CS_DEV {
	CODEENTRY *codes;	// Hold all the entries being modified
	CODEENTRY *modify;	// Pointer to an entry in codes array (The entry is currently being modified)
	long allocated;		// The amount of allocated memory to codes
	long num_stored;	// The number of entries in codes
} CS_DEV;

void CS_InitDev(CS_DEV *dev);
void CS_AddCode(CS_DEV *dev, CODEENTRY code);
void CS_RemoveCodeAt(CS_DEV *dev, int location);
CODEENTRY* CS_GetCodeAt(CS_DEV *dev, int location);
void CS_ClearDev(CS_DEV *dev);
void CS_SwapDev(CS_DEV *dev, int loc1, int loc2);

#endif