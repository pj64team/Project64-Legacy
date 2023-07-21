#include <Windows.h>
#include "CheatSearch_Search.h"

#define arr_growth 65536

void CS_InitSearch(CS_SEARCH* search) {
	search->searchBy = searchbyvalue;
	search->searchNumBits = bits8;
	search->searchType = dec;
	strcpy(search->search_string, "");
}

// This is helpful for the initial search, it can preallocate enough memory to hold all unknown search results (4 or 8 million depending on RDRAM setting)
BOOL CS_ReserveSpace(CS_RESULTS* res, DWORD amount) {
	BYTE* tmp;

	CS_ClearResults(res);

	tmp = calloc(amount / 8, 1);
	if (tmp == NULL) {
		return FALSE;
	}
	res->hits.bitmap = tmp;

	tmp = malloc(amount);
	if (tmp == NULL) {
		free(res->hits.bitmap);
		return FALSE;
	}
	res->hits.values = tmp;

	tmp = malloc(amount);
	if (tmp == NULL) {
		free(res->hits.bitmap);
		free(res->hits.values);
		return FALSE;
	}
	res->hits.prev_values = tmp;

	res->hits.reserved = amount;

	return TRUE;
}

void CS_ClearResults(CS_RESULTS* res) {
	if (res->hits.bitmap != NULL) {
		free(res->hits.bitmap);
	}
	if (res->hits.values != NULL) {
		free(res->hits.values);
	}
	if (res->hits.prev_values != NULL) {
		free(res->hits.prev_values);
	}
	if (res->addresses != NULL) {
		free(res->addresses);
	}

	res->hits.bitmap = NULL;
	res->hits.values = NULL;
	res->hits.prev_values = NULL;
	res->addresses = NULL;
	res->hits.reserved = 0;
	res->allocated = 0;
	res->num_stored = 0;
}

BOOL CS_ReallocateAddresses(CS_RESULTS *res) {
	DWORD* tmp = (DWORD*)realloc(res->addresses, sizeof(DWORD) * (res->allocated + arr_growth));

	// Failure to allocate more memory
	if (tmp == NULL) {
		CS_ClearResults(res);
		return FALSE;
	}

	res->addresses = tmp;
	res->allocated += arr_growth;

	return TRUE;
}

BOOL CS_AddResultByte(CS_RESULTS* res, DWORD address, BYTE value) {
	// Reallocate memory if needed
	if (res->allocated == res->num_stored && !CS_ReallocateAddresses(res)) {
		return FALSE;
	}

	// Store the hit in the bitmap
	int index = address / 8;
	int bit = address % 8;
	res->hits.bitmap[index] |= (BYTE)(1 << bit);
	res->hits.values[address] = value;
	res->addresses[res->num_stored] = address;
	res->num_stored += 1;

	return TRUE;
}

BOOL CS_AddResultWord(CS_RESULTS* res, DWORD address, WORD value) {
	// Reallocate memory if needed
	if (res->allocated == res->num_stored && !CS_ReallocateAddresses(res)) {
		return FALSE;
	}

	// Store the hit in the bitmap
	int index = address / 8;
	int bit = address % 8;
	res->hits.bitmap[index] |= (BYTE)(3 << bit);
	res->hits.values[address] = (BYTE)(value >> 8);
	res->hits.values[address + 1] = (BYTE)(value);
	res->addresses[res->num_stored] = address;
	res->num_stored += 1;

	return TRUE;
}

// TO DO!!
// Come up with a better way to handle this
void CS_AddTextResult(CS_RESULTS* res, DWORD address, char* value) {
	(void)value;

	CS_AddResultByte(res, address, 0);
}

BOOL CS_AddHitByte(CS_RESULTS* res, CS_HIT* hit) {
	if (CS_AddResultByte(res, hit->address, (BYTE)hit->value)) {
		res->hits.prev_values[hit->address] = (BYTE)hit->prev_value;
		return TRUE;
	}

	return FALSE;
}

BOOL CS_AddHitWord(CS_RESULTS* res, CS_HIT* hit) {
	if (!(hit->address & 1) && CS_AddResultWord(res, hit->address, hit->value)) {
		res->hits.prev_values[hit->address] = (BYTE)(hit->prev_value >> 8);
		res->hits.prev_values[hit->address + 1] = (BYTE)(hit->prev_value);
		return TRUE;
	}

	return FALSE;
}

BOOL CS_HasHit(CS_RESULTS* res, DWORD address) {
	if (address >= res->hits.reserved) {
		return FALSE;
	}

	int index = address / 8;
	int bit = address % 8;

	return (res->hits.bitmap[index] & (BYTE)(1 << bit)) != 0;
}

BOOL CS_GetHitByte(CS_HIT* hit, CS_RESULTS* res, DWORD address) {
	if (CS_HasHit(res, address)) {
		hit->address = address;
		hit->value = res->hits.values[address];
		hit->prev_value = res->hits.prev_values[address];

		return TRUE;
	}

	return FALSE;
}

BOOL CS_GetHitWord(CS_HIT* hit, CS_RESULTS* res, DWORD address) {
	if (!(address & 1) && CS_HasHit(res, address)) {
		hit->address = address;
		hit->value = (res->hits.values[address] << 8) | res->hits.values[address + 1];
		hit->prev_value = (res->hits.prev_values[address] << 8) | res->hits.prev_values[address + 1];

		return TRUE;
	}

	return FALSE;
}