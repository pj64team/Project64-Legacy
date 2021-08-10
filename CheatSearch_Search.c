#include <Windows.h>
#include "CheatSearch_Search.h"

#define arr_growth 65536

void CS_InitSearch(CS_SEARCH* search) {
	search->searchBy = searchbyvalue;
	search->searchNumBits = bits8;
	search->searchType = dec;
	strcpy(search->search_string, "");
}

// This does not clear allocated memory, be sure to call CS_ClearResults instead
void CS_InitResults(CS_RESULTS* res) {
	res->allocated = 0;
	res->num_stored = 0;
	res->hits = NULL;
}

// This is helpful for the initial search, it can preallocate enough memory to hold all unknown search results (4 or 8 million depending on RDRAM setting)
void CS_ReserveSpace(CS_RESULTS* res, DWORD amount) {
	CS_HITS* tmp;

	CS_ClearResults(res);

	tmp = malloc(sizeof(*res->hits) * amount);
	if (tmp == NULL)
		return;
	res->hits = tmp;

	res->allocated = amount;
}

void CS_AddResult(CS_RESULTS* res, DWORD address, WORD value) {
	CS_HITS* tmp;

	// Reallocate memory if needed
	if (res->allocated == res->num_stored) {
		tmp = (CS_HITS*)realloc(res->hits, sizeof(*res->hits) * (res->allocated + arr_growth));

		// Failure to allocate more memory
		// Consider throwing an error here of some kind
		if (tmp == NULL) {
			CS_ClearResults(res);
			return;
		}

		res->hits = tmp;
		res->allocated += arr_growth;
	}

	// Store the hit in the array
	res->hits[res->num_stored].address = address;
	res->hits[res->num_stored].value = value;
	res->num_stored++;
}

// TO DO!!
// Come up with a better way to handle this
void CS_AddTextResult(CS_RESULTS* res, DWORD address, char* value) {
	CS_AddResult(res, address, 0);
}

void CS_ClearResults(CS_RESULTS* res) {
	if (res->hits != NULL)
		free(res->hits);

	CS_InitResults(res);
}

void CS_AddHit(CS_RESULTS* res, CS_HITS* hit) {
	CS_AddResult(res, hit->address, hit->value);
	res->hits[res->num_stored - 1].prev_value = hit->prev_value;
}

CS_HITS* CS_GetHit(CS_RESULTS* res, DWORD loc) {
	if (loc < res->num_stored)
		return &res->hits[loc];
	else
		return NULL;
}