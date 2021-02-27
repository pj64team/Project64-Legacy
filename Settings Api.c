#include <stdio.h>
#include <sys/locking.h>
#include <time.h>
#include <Windows.h>
#include "Settings Api.h"
#include "FileHandler.h"

void Settings_GetDirectory (int select, char *Directory, int len) {
	char path_buffer[_MAX_PATH], drive[_MAX_DRIVE], dir[_MAX_DIR];
	char *default_path, *use_default, *search, *ret;

	switch (select) {
	case RomDir:
		default_path = DEF_ROM_PATH;
		use_default = USE_DEF_ROM_PATH;
		search = DEF_ROM_NAME;
		break;
	case AutoSaveDir:
		default_path = DEF_AUTOSAVE_PATH;
		use_default = USE_DEF_AUTOSAVE_PATH;
		search = DEF_AUTOSAVE_NAME;
		break;
	case InstantSaveDir:
		default_path = DEF_SAVE_PATH;
		use_default = USE_DEF_SAVE_PATH;
		search = DEF_SAVE_NAME;
		break;
	case PluginDir:
		default_path = DEF_PLUGIN_PATH;
		use_default = USE_DEF_PLUGIN_PATH;
		search = DEF_PLUGIN_NAME;
		break;
	case SnapShotDir:
		default_path = DEF_SNAPSHOT_PATH;
		use_default = USE_DEF_SNAPSHOT_PATH;
		search = DEF_SNAPSHOT_NAME;
		break;
	default:
		return;
	}

	GetModuleFileName(NULL, path_buffer, sizeof(path_buffer));
	_splitpath(path_buffer, drive, dir, NULL, NULL);
	sprintf(Directory, default_path, drive, dir);

	Settings_Read(APPS_NAME, DIRECTORIES, use_default, STR_FALSE, &ret);
	if (strcmp(ret, STR_TRUE) != 0 || select == RomDir) {	// Rom Directory is a special case, it is always read.
		free (ret);
		Settings_Read(APPS_NAME, DIRECTORIES, search, Directory, &ret);
		if (ret) {
			strncpy(Directory, ret, len);
			free(ret);
		}
	}
}

void Settings_Read(char *filename, char *id, char *setting, char *defaultvalue, char **value) {
	*value = ReadStr(filename, id, setting, defaultvalue);
}

BOOL Settings_ReadBool(char *filename, char *id, char *setting, BOOL defaultvalue) {
	char *value;
	BOOL ret;

	value = ReadStr(filename, id, setting, defaultvalue ? STR_TRUE : STR_FALSE);
	ret = strcmp(value, STR_TRUE) == 0 ? TRUE : FALSE;
	if (value) free(value);

	return ret;
}

int Settings_ReadInt(char *filename, char *id, char *setting, int defaultvalue) {
	return FetchIntValue(filename, id, setting, defaultvalue);
}

BOOL Settings_HasSetting(char *filename, char *id, char *setting) { 
	return IsSet(filename, id, setting);
}

void Settings_Write(char *filename, char *id, char *setting, char *value) {
	Write(filename, id, setting, value);
}

void Settings_Delete(char *filename, char *id, char *setting) {
	Delete(filename, id, setting);
}

void Settings_DeleteEntry(char *filename, char *id) {
	DeleteAll(filename, id);
}

void Settings_FetchKeyNames(char *filename, char *id, char **keys) {
	*keys = ReadStr(filename, id, NULL, NULL);
}