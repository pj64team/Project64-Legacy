#include <ctype.h>
#include <stdio.h>
#include <windows.h>
#include "Cheats.h"
#include "Main.h"
#include "Rom.h"
#include "RomTools_Common.h"
#include "Settings Api.h"
#include "Settings Common Defines.h"

// The supported types of replacement in the cheat
#define NO_REPLACE 0	// Code string is good and does not do replacement
#define O_REPLACE 1		// Code string is good and does value replacement
#define SO_REPLACE 2	// Code string is good and does address replacement
#define MO_REPLACE 3	// Code string is good and does both value and address replacement
#define BAD_REPLACE 4	// Code string is good but the options are bad
#define BAD_CODE 5		// Code string is not good

typedef struct {
	char* name;				// The cheat name
	char* codestring;		// The code string
	char* replacedstring;	// The replaced code string
	char* options;			// The options string
	char* selected;			// The selected option
	char* note;				// The related note for the cheat

	unsigned int type;	// The type (See the defines above from 0 to 4, NO_REPLACE etc...)
} CHEAT;


// Special Cheats reading function
// This will preprocess the cheat input before it is used inside Cheats
BOOL Cheats_Read(CHEAT* cheat, unsigned int cheat_num);

// Special Cheats write function
// This will preprocess the cheats before being output to file
BOOL Cheats_Write(CHEAT *cheat);

void Cheats_Delete(CHEAT* cheat);

BOOL Cheats_Store(char** string, char* source, unsigned int length);

void Cheats_ClearCheat(CHEAT* cheat);

// The Cheat Name that will be displayed on the Cheat Manager List
void Cheats_DisplayName(CHEAT *cheat, char* string, unsigned int string_length);

// Verify that all options can be loaded into the code string
// This will return FALSE on any failure
// This is designed for use by the dialog boxes / graphical interface and does not interfere with normal code loading
BOOL Cheats_VerifyInput(CHEAT* cheat);