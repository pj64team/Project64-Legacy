#include <Windows.h>
#include "CheatSearch_Dev.h"

#define arr_growth 10

void CS_InitDev(CS_DEV *dev) {
	dev->allocated = 0;
	dev->codes = NULL;
	dev->modify = NULL;
	dev->num_stored = 0;
}

void CS_AddCode(CS_DEV *dev, CODEENTRY code) {
	CODEENTRY *tmp;

	// Reallocate memory if needed
	if (dev->allocated == dev->num_stored) {
		tmp = (CODEENTRY *)realloc(dev->codes, sizeof(*dev->codes) * (dev->allocated + arr_growth));

		// Failure to allocate more memory
		// Consider throwing an error here of some kind
		if (tmp == NULL) {
			CS_ClearDev(dev);
			return;
		}

		dev->codes = tmp;
		dev->allocated += arr_growth;
	}

	memcpy(&dev->codes[dev->num_stored], &code, sizeof(code));
	dev->num_stored++;
}

// This is not a real deletion but simply a swap of pointers
// The item that was removed will be at the end of the list, ready to be replaced/updated
void CS_RemoveCodeAt(CS_DEV *dev, DWORD location) {
	DWORD count;
	
	// Cannot swap if there are less than 2 items
	if (location >= 2) {
		for (count = location; count < dev->num_stored - 2; count++) {
			CS_SwapDev(dev, count, count + 1);
		}
	}

	dev->num_stored--;
}

CODEENTRY* CS_GetCodeAt(CS_DEV *dev, DWORD location) {

	// Check if the location is in bounds
	if (dev->num_stored == 0 || location < 0 || location > dev->num_stored - 1)
		return NULL;

	return &dev->codes[location];
}

void CS_ClearDev(CS_DEV *dev) {
	if (dev->codes != NULL)
		free(dev->codes);

	CS_InitDev(dev);
}

void CS_SwapDev(CS_DEV *dev, DWORD loc1, DWORD loc2) {
	CODEENTRY *one, *two;
	CODEENTRY hold;

	one = CS_GetCodeAt(dev, loc1);
	two = CS_GetCodeAt(dev, loc2);

	if (one != NULL && two != NULL) {
		hold = *one;
		*one = *two;
		*two = hold;
	}
}