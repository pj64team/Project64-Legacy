#include "Settings Common Defines.h"

// Helper function to help with getting these directory names
void Settings_GetDirectory (int select, char *Directory, int len);

////////////////////////////////////
// The following are file reads
////////////////////////////////////

// The reading of one setting, value will contain whatever is after setting=
// In the case of settings without the "=" a "True" will be returned
void Settings_Read(char *filename, char *id, char *setting, char *defaultvalue, char **value);

// The reading of one setting, value will be set according to whether the setting is set to "True" or "False"
// Where there is no setting, defaultvalue will be used
BOOL Settings_ReadBool(char *filename, char *id, char *setting, BOOL defaultvalue);

// The reading of one setting, value will be set according to the number read
// Where there is no setting, defaultvalue will be used
int Settings_ReadInt(char *filename, char *id, char *setting, int defaultvalue);

// To check if a setting exists, no value is read since none exists for this setting
BOOL Settings_HasSetting(char *filename, char *id, char *setting);

// Used to fetch the keys for a given entry
void Settings_FetchKeyNames(char *filename, char *id, char **keys);

////////////////////////////////////
// The following are file writes
////////////////////////////////////
void Settings_Write(char *filename, char *id, char *setting, char *value);
void Settings_Delete(char *filename, char *id, char *setting);
void Settings_DeleteEntry(char *filename, char *id);

////////////////////////////////////////////////////////////////////////////////////////
// Support for renaming a key (Used mainly in Cheats, Cheat1 changed to Cheat0 etc
////////////////////////////////////////////////////////////////////////////////////////
void Settings_ChangeKey(char* filename, char* id, char* oldkey, char* newkey);

///////////////////////////////////////////////////////////
// Added to support PJ64 2.x's new Cheat file system
///////////////////////////////////////////////////////////
void Settings_ReadLine(char* filename, char* id, int line_number, char **line);
void Settings_DeleteLine(char* filename, char* id, char* line);
void Settings_WriteLine(char* filename, char* id, char* line);