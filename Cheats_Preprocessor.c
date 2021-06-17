#include "Cheats_Preprocessor.h"

void FetchRedirection(char** string);
BOOL Cheats_Store(char** string, char* source, unsigned int length);
void Cheats_ClearCheat(CHEAT* cheat);

// Verify the cheat string and return the type (See the defines above, NO_REPLACE, O_REPLACE, SO_REPLACE, MO_REPLACE, BAD_REPLACE)
BOOL Cheats_Verify(CHEAT* cheat);

// Replace any ? in a one to one relationship based on a selected option
// Returns FALSE on failure, TRUE otherwise
BOOL Cheats_LoadOption(CHEAT* cheat);


BOOL Cheats_Read(CHEAT* cheat, unsigned int cheat_num) {
	char Identifier[100], CheatFormat[100];
	char* tmp, *find1, *find2;

	RomID(Identifier, RomHeader);

	// Read the main cheat entry that contains the name and the code string
	sprintf(CheatFormat, CHT_ENT, cheat_num);
	Settings_Read(CDB_NAME, Identifier, CheatFormat, STR_EMPTY, &tmp);

	// The cheat name is encapsulated by " "
	find1 = strchr(tmp, '"');
	find2 = strrchr(tmp, '"');

	// The cheat does not contain a name
	if (find1 == NULL || find2 == NULL || find1 == find2) {
		if (tmp)
			free(tmp);
		return FALSE;
	}

	// Populate the name
	// This only fails if memory could not be allocated
	if (!Cheats_Store(&(cheat->name), find1 + 1, find2 - find1 - 1)) {
		if (tmp)
			free(tmp);
		return FALSE;
	}

	// Populate the code string
	// This only fails if memory could not be allocated
	if (!Cheats_Store(&(cheat->codestring), find2 + 1, strlen((find2 + 1)))) {
		free(cheat->name);
		cheat->name = NULL;

		if (tmp)
			free(tmp);
		return FALSE;
	}

	// Populate the code string that will be used for the loading of options
	// This only fails if memory could not be allocated
	if (!Cheats_Store(&(cheat->replacedstring), find2 + 1, strlen((find2 + 1)))) {
		free(cheat->name);
		cheat->name = NULL;

		free(cheat->codestring);
		cheat->codestring = NULL;

		if (tmp)
			free(tmp);
		return FALSE;
	}

	if (tmp)
		free(tmp);

	// Make sure the code string that is associated with the name is valid
	Cheats_Verify(cheat);
	
	// Load any options that may exist (If none this will be an empty string)
	sprintf(CheatFormat, CHT_ENT_O, cheat_num);
	Settings_Read(CDB_NAME, Identifier, CheatFormat, STR_EMPTY, &cheat->options);
	FetchRedirection(&cheat->options);
	
	// Load any option that may be selected (If none this will be an empty string)
	sprintf(CheatFormat, CHT_EXT_O, cheat->name);
	Settings_Read(APPS_NAME, Identifier, CheatFormat, STR_EMPTY, &cheat->selected);
		
	// See if the selected option can be loaded, if it can't replacedstring will be shrunk
	if (!Cheats_LoadOption(cheat)) {
		// Could not load the option
		// Copy STR_EMPTY and reallocate space to only the minimum
		char* copy;
		strcpy(cheat->replacedstring, STR_EMPTY);
		copy = realloc(cheat->replacedstring, strlen(cheat->replacedstring) + 1);
		if (!copy)
			cheat->replacedstring = copy;
	}

	// Read the note
	sprintf(CheatFormat, CHT_ENT_N, cheat_num);
	Settings_Read(CDB_NAME, Identifier, CheatFormat, STR_EMPTY, &cheat->note);
	FetchRedirection(&cheat->note);

	return TRUE;
}


// Write out the cheat with any appropriate options
// This will delete anything with the name that is currently in use
// Will return false on failure to allocate memory or write to file
BOOL Cheats_Write(CHEAT* cheat) {
	char* tmp;
	unsigned int length, count;
	char format[50], Identifier[100];

	// Find the next available location (This is up to MaxCheats, a define inside of Cheats.h)
	for (count = 0; count < MaxCheats; count++) {
		CHEAT read = { 0 };

		if (!Cheats_Read(&read, count))
			break;

		if ((read.name != NULL) && strcmp(cheat->name, read.name) == 0) {
			DisplayError(GS(MSG_CHEAT_NAME_IN_USE));
			Cheats_ClearCheat(&read);
			return FALSE;
		}
	}

	// Should never reach MaxCheats
	if (count == MaxCheats) {
		DisplayError(GS(MSG_MAX_CHEATS));
		return FALSE;
	}

	RomID(Identifier, RomHeader);

	Settings_Write(CDB_NAME, Identifier, "Name", RomFullName);

	// To store the name and code string together
	// Need to add two " and one , and add space for the null byte at the end
	length = strlen(cheat->name) + strlen(cheat->codestring) + 4;
	tmp = malloc(sizeof(*tmp) * length);
	if (!tmp)
		return FALSE;

	// Write out the main entry (This is always written, the format is Cheat0="Name Here",80000000 0000
	// The code string should already have been verified so it's fine to simply write it out
	sprintf(format, CHT_ENT, count);
	snprintf(tmp, length, "\"%s\"%s", cheat->name, cheat->codestring);
	Settings_Write(CDB_NAME, Identifier, format, tmp);
	free(tmp);

	// Write the note out if the cheat has one
	if (cheat->note != NULL && strcmp(cheat->note, STR_EMPTY) != 0) {
		sprintf(format, CHT_ENT_N, count);
		Settings_Write(CDB_NAME, Identifier, format, cheat->note);
	}
	
	// Write the options out if the cheat has any
	if (cheat->options != NULL && strcmp(cheat->options, STR_EMPTY) != 0) {
		sprintf(format, CHT_ENT_O, count);
		Settings_Write(CDB_NAME, Identifier, format, cheat->options);
	}
	return TRUE;
}


void Cheats_Delete(CHEAT* cheat) {
	char* tmp;
	unsigned int length, count, i;
	char format[50], previous[50], Identifier[100];
	char *names[] = { CHT_ENT, CHT_ENT_N, CHT_ENT_O };

	// Delete the first cheat that matches the current name
	for (count = 0; count < MaxCheats; count++) {
		CHEAT read = { 0 };

		if (!Cheats_Read(&read, count))
			continue;

		if ((read.name != NULL) && strcmp(cheat->name, read.name) == 0) {
			Cheats_ClearCheat(&read);
			break;
		}
	}

	// Nothing to delete
	if (count == MaxCheats)
		return;

	RomID(Identifier, RomHeader);

	// Delete the three entries: the main one, the note, and the option
	for (i = 0; i < sizeof(names) / sizeof(*names); i++) {
		sprintf(format, names[i], count);
		strcat(format, "=");
		Settings_Delete(CDB_NAME, Identifier, format);
	}

	// Now change the keys on the remaining cheats to reflect the newly emptied count
	count++;
	while (count != MaxCheats) {
		// Change the keys for the three entries: the main one, the note, and the option
		for (i = 0; i < sizeof(names) / sizeof(*names); i++) {
			sprintf(format, names[i], count);
			strcat(format, "=");

			sprintf(previous, names[i], count - 1);
			strcat(previous, "=");

			Settings_ChangeKey(CDB_NAME, Identifier, format, previous);
		}
		count++;
	}

	// To do! Remove this hack once the file handler has been updated to do delayed/timed writes
	Settings_Delete(CDB_NAME, Identifier, "Cheat9000");

	// Remove the cheat from the APPS file
	Settings_Delete(APPS_NAME, Identifier, cheat->name);

	// Remove any selected option from the APPS file
	length = strlen(cheat->name) + strlen(CHT_EXT_O);
	tmp = malloc(sizeof(*tmp) * length);
	if (!tmp)
		return;
	snprintf(tmp, length, CHT_EXT_O, cheat->name);
	Settings_Delete(APPS_NAME, Identifier, cheat->name);
}


BOOL Cheats_Store(char** string, char* source, unsigned int length) {
	*string = malloc(sizeof(**string) * (length + 1));

	if (!*string)
		return FALSE;

	strncpy(*string, source, length);
	(*string)[length] = '\0';
	return TRUE;
}


void Cheats_ClearCheat(CHEAT* cheat) {
	if (cheat->name) {
		free(cheat->name);
		cheat->name = NULL;
	}

	if (cheat->codestring) {
		free(cheat->codestring);
		cheat->codestring = NULL;
	}

	if (cheat->options) {
		free(cheat->options);
		cheat->options = NULL;
	}

	if (cheat->selected) {
		free(cheat->selected);
		cheat->selected = NULL;
	}

	if (cheat->note) {
		free(cheat->note);
		cheat->note = NULL;
	}

	cheat->type = BAD_CODE;
}


// There are multiple formats available to the cheat string but ultimately it is as follows, Cheat0="Name Here" 80?????? ????
//  With a comma following additional cheats to be activated, example 80?????? ????,80?????? ????
// The 80 is an activator and has limits -- This will be checked for at a later time for now this code is written without verification of activator
BOOL Cheats_Verify(CHEAT *cheat) {
	int address_replace = -1, addr_tmp, value_replace = -1, val_tmp;
	char* find;

	if ((cheat->name != NULL && strlen(cheat->name) == 0) || (cheat->codestring != NULL && strlen(cheat->codestring) == 0)) {
		cheat->type = BAD_CODE;
		return FALSE;
	}

	find = cheat->codestring;
	while (find != NULL) {
		addr_tmp = 0;
		val_tmp = 0;

		// The next check will be ,XXXXXXXX XXXX so at least 14 characters long
		if (strlen(find) < 14) {
			cheat->type = BAD_CODE;
			return FALSE;
		}

		// Skip past the ,
		find++;

		// A space may be the next character, simply ignore it
		// This means we now support ", 80000000 0000, 80000010 0000" (Note the space after the comma)
		if (find[0] == ' ')
			find++;

		// This is where the activator is checked and where more code may be added in the future (The list of activators is in Cheat.c)
		// For now this must be a hexadecimal digit and we are not allowing ? in the activator
		if (!isxdigit(find[0]) || !isxdigit(find[1])) {
			cheat->type = BAD_CODE;
			return FALSE;
		}

		for (int i = 2; i < 8; i++) {
			if (!isxdigit(find[i])) {
				if (find[i] == '?')
					addr_tmp++;
				else {
					cheat->type = BAD_CODE;
					return FALSE;
				}
			}
		}

		// Set the number of replaces for address if not set
		if (address_replace == -1 && addr_tmp != 0)
			address_replace = addr_tmp;

		// Fow now this supports uniform replacement, 0 to 6 replacement
		// So, a replacement of 4 for the address will only accept 0 or 4 replacements
		// If something else is read, such as a 3 then the code will be rejected
		// In the future this will be changed to check against the appropriate option (Be it address, value, or both)
		if (addr_tmp != 0 && address_replace != addr_tmp) {
			cheat->type = BAD_CODE;
			return FALSE;
		}

		// A space separates the address and value
		if (find[8] != ' ') {
			cheat->type = BAD_CODE;
			return FALSE;
		}

		// The first two digits in the value may be replaced
		if (!isxdigit(find[9]) || !isxdigit(find[10])) {
			if (find[9] == '?' && find[10] == '?')
				val_tmp++;
			else {
				cheat->type = BAD_CODE;
				return FALSE;
			}
		}

		// The second two digits in the value may be replaced
		if (!isxdigit(find[11]) || !isxdigit(find[12])) {
			if (find[11] == '?' && find[12] == '?')
				val_tmp++;
			else {
				cheat->type = BAD_CODE;
				return FALSE;
			}
		}

		// Set the number of replaces for value if not set
		if (value_replace == -1 && val_tmp != 0)
			value_replace = val_tmp;

		// For now this is supporting uniform replacement, 8bit or 16bit replacement and not a mixture
		// In the future this will be changed to check against the appropriate option (Be it address, value, or both)
		if (val_tmp != 0 && value_replace != val_tmp) {
			cheat->type = BAD_CODE;
			return FALSE;
		}

		// Check the next code (Doing + 13 just to skip past characters that have already been verified)
		find = strchr(find + 13, ',');
	}

	if (address_replace == -1)
		address_replace = 0;

	if (value_replace == -1)
		value_replace = 0;

	if (address_replace == 0 && value_replace == 0) {
		cheat->type = NO_REPLACE;
		return TRUE;
	}

	if (address_replace == 0 && value_replace != 0) {
		cheat->type = O_REPLACE;
		return TRUE;
	}

	if (address_replace != 0 && value_replace == 0) {
		cheat->type = SO_REPLACE;
		return TRUE;
	}

	if (address_replace != 0 && value_replace != 0) {
		cheat->type = MO_REPLACE;
		return TRUE;
	}

	cheat->type = BAD_CODE;
	return FALSE;
}


// Replace the ? inside the code string
// The option being loaded should be in one of these formats:
// $Value(:Value) Name
// $Value Name 
// $Value:Value(:Value:Value) Name
// Where parenthesis notes potential repeating sections
BOOL Cheats_LoadOption(CHEAT* cheat) {
	char* Ext, * check;
	unsigned int i, rep;
	BOOL serial_replacement;

	// Cannot replace if the code is bad
	if (cheat->type == BAD_CODE)
		return FALSE;

	// Cheat did not have a replacement
	if (cheat->type == NO_REPLACE)
		return TRUE;

	Ext = cheat->selected;

	// No selected option or the option is not in the proper format
	if (strcmp(Ext, STR_EMPTY) == 0 || Ext[0] != '$') {
		cheat->type = BAD_REPLACE;
		return FALSE;
	}

	// Check to see if this selected option has a name
	check = strrchr(Ext, ' ');
	if (!check || strlen(check) == 0) {
		cheat->type = BAD_REPLACE;
		return FALSE;
	}

	// Check to see if the replacement string will be looped back or if the replacement will happen in sequence
	serial_replacement = FALSE;
	check = strchr(Ext, ':');
	if (check != NULL) {
		// Multiple Option replacement uses a :, this is only a serial replacement if there is more than one :
		if (cheat->type == MO_REPLACE) {
			if (strrchr(Ext, ':') != check)
				serial_replacement = TRUE;
		}
		// The other options don't use a : so existence of one indicates a serial replacement
		else
			serial_replacement = TRUE;
	}

	// Scan the string for ? to replace
	check = cheat->replacedstring;
	rep = 1;	// To skip past the $ in Ext
	while (check != NULL) {

		if (check[0] != ',')
			return FALSE;
		else
			check++;

		if (check[0] == ' ')
			check++;

		// Scan the address (First 8 bytes, ignoring the first 2 for the activator) for ? to replace
		for (i = 2; i < 8; i++) {
			if (check[i] == '?') {
				// Regular option does not allow for address replacement
				if (cheat->type == O_REPLACE)
					return FALSE;

				// Replacement requires Ext to contain a valid hexadecimal number
				if (strlen(Ext) >= rep && isxdigit(Ext[rep])) {
					check[i] = Ext[rep];
					rep++;
				}
				else
					return FALSE;
			}
		}

		// Only for Multiple Option, a : must be present
		if (cheat->type == MO_REPLACE) {
			if (Ext[rep] != ':')
				return FALSE;
			else
				rep++;
		}

		// Increment past the ' '
		check += 9;

		for (i = 0; i < 4; i++) {
			if (check[i] == '?') {
				// Special Option does not allow for value replacement
				if (cheat->type == SO_REPLACE)
					return FALSE;

				// Replacement requires Ext to contain a valid hexadecimal number
				if (strlen(Ext) >= rep && isxdigit(Ext[rep])) {
					check[i] = Ext[rep];
					rep++;
				}
				else
					return FALSE;
			}
		}

		// Decide what to do with rep, the 'counter' for the replacement string
		if (serial_replacement) {
			// This must contain a :
			if (Ext[rep] == ':')
				rep++;
			else {
				// Reached the end of the replacement
				if (Ext[rep] != ' ')
					return FALSE;
			}
		}
		else {
			// By this point the next character should be a space, to denote the end of the replacement
			if (Ext[rep] != ' ')
				return FALSE;
			rep = 1;
		}

		// Check the next code
		check = strchr(check, ',');
	}
	return TRUE;
}


// Used only when reading _O (Option) or _N (Note)
void FetchRedirection(char** string) {

	// Empty string passed
	if (*string == NULL || strcmp(*string, STR_EMPTY) == 0)
		return;

	// This redirection goes to [Global] by default, used to go outside of the current cheat entry
	if (*string[0] == '@') {
		Settings_Read(CDB_NAME, "Global", *string + 1, STR_EMPTY, string);
		return;
	}

	// This redirection goes to [Name], used to go outside of the current cheat entry
	if (*string[0] == '[') {
		char* find, Id2[100];

		find = strchr(*string, ']');

		// This is not a proper redirection
		if (find == NULL || strlen(find) < 2)
			return;

		memset(Id2, '\0', sizeof(Id2));
		strncpy(Id2, *string + 1, find - *string - 1);
		Settings_Read(CDB_NAME, Id2, find + 1, STR_EMPTY, string);

		return;
	}

	// If this is not an option (denoted by a $ at the start) then try to fetch a redirection
	if (*string[0] != '$') {
		char* tmp, Identifier[100];
		RomID(Identifier, RomHeader);

		Settings_Read(CDB_NAME, Identifier, *string, *string, &tmp);
		free(*string);
		*string = tmp;
	}
}


// The Cheat List will display the cheat by name
// This includes any added options or a notice that options are available (via a ?)
void Cheats_DisplayName(CHEAT* cheat, char* string, unsigned int string_length) {

	// The cheat supports replacement of values (Either the address, value, or a combination of both)
	// So append the name of the option if one is selected or append a ? if none is selected
	if (cheat->type != NO_REPLACE) {

		// This is a bad code, show (Bad Code) after the cheat name
		if (cheat->type == BAD_CODE) {
			snprintf(string, string_length, "%s (Bad Code)", cheat->name);
			return;
		}

		// No option selected, show (=> ?)
		if (cheat->selected == NULL || strcmp(cheat->selected, STR_EMPTY) == 0) {
			snprintf(string, string_length, CHT_EXT, cheat->name, "?");
			return;
		}

		// An option is selected, show only the name of the option
		else {
			char* find = strchr(cheat->selected, ' ');

			// What's stored in the file is not in the appropriate format so show (=> Bad Option?)
			if (find == NULL || cheat->selected[0] != '$')
				snprintf(string, string_length, CHT_EXT, cheat->name, "Bad Option?");

			// Skip past the value and just display the name of the option so for example (=> Invincible)
			else
				snprintf(string, string_length, CHT_EXT, cheat->name, find + 1);
		}
	}
	// No option to show so simply output the name
	else
		snprintf(string, string_length, "%s", cheat->name);
}

BOOL Cheats_VerifyInput(CHEAT* cheat) {
	unsigned int length;
	char* navigate, * end;

	// First verify the code string
	if (!Cheats_Verify(cheat))
		return FALSE;

	// A code that does not need replacements is good
	if (cheat->type == NO_REPLACE)
		return TRUE;

	// No options to verify
	if (cheat->options == NULL || strcmp(cheat->options, STR_EMPTY) == 0)
		return FALSE;

	// Allocate enough space for any option
	// This will use a lot more memory than is needed but it shouldn't be an issue
	if (cheat->selected)
		free(cheat->selected);
	length = strlen(cheat->options);
	if (length > 0) {
		cheat->selected = malloc(sizeof(*cheat->selected) * (length + 1));
		if (!cheat->selected)
			return FALSE;
	}
	else
		return FALSE;

	// This will be a copy of cheat->codestring
	if (!cheat->replacedstring) {
		length = strlen(cheat->codestring);
		cheat->replacedstring = malloc(sizeof(*cheat->replacedstring) * (length + 1));
		if (!cheat->replacedstring)
			return FALSE;
	}

	navigate = cheat->options;

	while (navigate != NULL) {
		// Always reset this, it must contain ? to be replaced
		strcpy(cheat->replacedstring, cheat->codestring);

		// Calculate how much to copy for the next option (Up to a , or the remainder of the options string)
		end = strchr(navigate, ',');
		if (!end)
			length = strlen(navigate);
		else
			length = end - navigate - 1;

		if (length > 0) {
			// This strncpy should be fine, it will copy at most the entire length of the options string
			// That's the amount of memory that was allocated for this
			strncpy(cheat->selected, navigate, length);
			cheat->selected[length - 1] = '\0';

			if (strlen(cheat->selected) > 0) {
				// Note: Not deallocating cheat->selected because it should be deallocated by Cheats_ClearCheat()
				if (!Cheats_LoadOption(cheat))
					return FALSE;
			}
		}

		// Go to the next option to try
		navigate = strchr(navigate + 1, '$');
	}

	return TRUE;
}