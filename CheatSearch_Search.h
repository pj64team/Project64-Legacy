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
	searchbyjal
} SEARCHBY;

// This struct is to be filled and maintained by the GUI
typedef struct CS_SEARCH {
	char search_string[STRING_MAX];
	SEARCHBY searchBy;
	SEARCHTYPE searchType;
	NUMBITS searchNumBits;
} CS_SEARCH;

typedef struct CS_HITS {
	DWORD address;		// The address the value was scanned at
	WORD value;			// The value at the time of scan (These may not be the same, depending on the type of scan)
	WORD prev_value;	// The previous value held at the address (Again, this may not be a static value... greater/less than scans)
} CS_HITS;

// The search results
typedef struct CS_RESULTS {
	CS_HITS *hits;			// The stored results of the scan
	DWORD allocated;		// The amount of memory allocated to results array
	DWORD num_stored;		// The amount of search resutls stored in the results array
} CS_RESULTS;


void CS_InitSearch(CS_SEARCH *search);
void CS_InitResults(CS_RESULTS *res);
void CS_AddResult(CS_RESULTS *res, DWORD address, DWORD value);
void CS_AddTextResult(CS_RESULTS *res, DWORD address, char *value);
void CS_ClearResults(CS_RESULTS *res);
void CS_AddHit(CS_RESULTS *res, CS_HITS *hit);
CS_HITS *CS_GetHit(CS_RESULTS *res, DWORD loc);

#endif