#ifndef CS_SEARCH_H
#define CS_SEARCH_H

#define STRING_MAX 256

// Cheats are either 8bit or 16bit
typedef enum NUMBITS {
	bits8,
	bits16
} NUMBITS;

// The currently supported search types
typedef enum SEARCHTYPE {
	unknown,
	dec,
	hex,
	changed,
	unchanged,
	higher,
	lower
} SEARCHTYPE;

// The type of search that is being performed
typedef enum SEARCHBY {
	searchbyvalue,
	searchbytext,
} SEARCHBY;

// This struct is to be filled and maintained by the GUI
typedef struct CS_SEARCH {
	char search_string[STRING_MAX];
	SEARCHBY searchBy;
	SEARCHTYPE searchType;
	NUMBITS searchNumBits;
} CS_SEARCH;

typedef struct CS_HIT {
	DWORD address;		// The address the value was scanned at
	WORD value;			// The value at the time of scan (These may not be the same, depending on the type of scan)
	WORD prev_value;	// The previous value held at the address (Again, this may not be a static value... greater/less than scans)
} CS_HIT;

typedef struct CS_BITMAP {
	BYTE* bitmap;		// A bitmap of addresses containing results
	BYTE* values;		// An array of values at each address
	BYTE* prev_values;	// An array of previous values at each address
	DWORD reserved;		// Number of addresses that this bitmap can store
} CS_BITMAP;

// The search results
typedef struct CS_RESULTS {
	CS_BITMAP hits;		// The stored results of the scan
	DWORD *addresses;	// THe list of addresses found; needed only for populating the listbox
	DWORD allocated;	// The amount of memory allocated to addresses array
	DWORD num_stored;	// The amount of search results stored in the addresses array
} CS_RESULTS;

void CS_InitSearch(CS_SEARCH* search);

BOOL CS_ReserveSpace(CS_RESULTS* res, DWORD amount);
void CS_ClearResults(CS_RESULTS* res);

void CS_AddTextResult(CS_RESULTS* res, DWORD address, char *value);

BOOL CS_AddResultByte(CS_RESULTS* res, DWORD address, BYTE value);
BOOL CS_AddHitByte(CS_RESULTS* res, CS_HIT* hit);
BOOL CS_GetHitByte(CS_HIT* hit, CS_RESULTS* res, DWORD address);

BOOL CS_AddResultWord(CS_RESULTS* res, DWORD address, WORD value);
BOOL CS_AddHitWord(CS_RESULTS* res, CS_HIT* hit);
BOOL CS_GetHitWord(CS_HIT* hit, CS_RESULTS* res, DWORD address);

#endif