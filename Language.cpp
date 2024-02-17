#include <windows.h>
#include <stdio.h>

#include "Main.h" // for hinst
#include "Language.h"
#include "resource.h"

/*******************************************************************************
* Definitions                                                                  *
*******************************************************************************/

#define MAX_LANGUAGES	45
#define MAX_LANNAME_LEN	100
#define MAX_STRINGS		300
#define MAX_STRING_LEN	400

typedef struct {
	int    ID;
	char   Str[MAX_STRING_LEN];
} LANG_STR;

LANG_STR DefaultString[] = {
	{ INI_CURRENT_LANG,    "Current Language"        },
	{ INI_AUTHOR,          "Author"                  },
	{ INI_VERSION,         "Version"                 },
	{ INI_DATE,            "Date"                    },
	{ INI_HOMEPAGE,        "Visit Home Page"         },
	{ INI_CURRENT_RDB,     "ROM Database (.RDS)"     },
	{ INI_CURRENT_CHT,     "Cheat Code file (.CDB)"  },
	{ INI_CURRENT_RDX,     "Extended Rom Info (.RDI)"},
	{ INI_TITLE,           "About INI Files"         },

/*********************************************************************************
* Numbers                                                                        *
*********************************************************************************/
	{ NUMBER_0,             "0"                      },
	{ NUMBER_1,             "1"                      },
	{ NUMBER_2,             "2"                      },
	{ NUMBER_3,             "3"                      },
	{ NUMBER_4,             "4"                      },
	{ NUMBER_5,             "5"                      },
	{ NUMBER_6,             "6"                      },
	{ NUMBER_7,             "7"                      },
	{ NUMBER_8,             "8"                      },
	{ NUMBER_9,             "9"                      },

	{ MENU_FILE,     "&File"     },
		{ MENU_OPEN,       "&Open Rom"               },
		{ MENU_ROM_INFO,   "Rom &Info...."           },
		{ MENU_GAME_INFO,	"&Game Information"},
		{ MENU_START,      "Start Emulation"         },
		{ MENU_END,        "&End Emulation"          },
		{ MENU_CHOOSE_ROM, "Choose Rom Directory..." },
		{ MENU_REFRESH,    "Refresh Rom List"        },
		{ MENU_RECENT_ROM, "Recent Rom"              },
		{ MENU_RECENT_DIR, "Recent Rom Directories"  },
		{ MENU_EXIT,       "E&xit"                   },
	{ MENU_SYSTEM,   "&System"   },
		{ MENU_RESET,       "&Reset"                 },
		{ MENU_PAUSE,       "&Pause"                 },
		{ MENU_RESUME,      "R&esume"                },
		{ MENU_BITMAP,      "Screenshot Capture"        },
		{ MENU_LIMIT_FPS,   "Limit FPS"              },
		{ MENU_SAVE,        "&Save"                  },
		{ MENU_SAVE_AS,     "Save As..."             },
		{ MENU_RESTORE,     "&Restore"               },
		{ MENU_LOAD,        "Restore From"                },
		{ MENU_CURRENT_SAVE,"Current Save S&tate"    },
			{ MENU_SLOT_DEFAULT,"Default"            },
			{ MENU_SLOT_1,      "Slot 1"             },
			{ MENU_SLOT_2,      "Slot 2"             },
			{ MENU_SLOT_3,      "Slot 3"             },
			{ MENU_SLOT_4,      "Slot 4"             },
			{ MENU_SLOT_5,      "Slot 5"             },
			{ MENU_SLOT_6,      "Slot 6"             },
			{ MENU_SLOT_7,      "Slot 7"             },
			{ MENU_SLOT_8,      "Slot 8"             },
			{ MENU_SLOT_9,      "Slot 9"             },
			{ MENU_SLOT_10,     "Slot 10"            },
		{ MENU_CHEAT,       "Cheats..."              },
		{ MENU_GS_BUTTON,   "GS Button"              },
	{ MENU_OPTIONS,  "&Options"  },
		{ MENU_FULL_SCREEN, "&Full Screen"                   },
		{ MENU_ON_TOP,      "&Always On &Top"                },
		{ MENU_CONFG_GFX,   "Configure &Graphics Plugin"   },
		{ MENU_CONFG_AUDIO, "Configure A&udio Plugin"      },
		{ MENU_CONFG_CTRL,  "Configure &Controller Plugin" },
		{ MENU_CONFG_RSP,   "Configure &RSP Plugin"        },
		{ MENU_SHOW_CPU,    "Show CPU usage %"               },
		{ MENU_SETTINGS,    "&Settings..."                   },
	{ MENU_DEBUGGER, "&Debugger" },
	{ MENU_LANGUAGE, "&Language" },
	{ MENU_HELP,     "&Help"     },
		{ MENU_USER_MAN,    "&User Manual..."    },
		{ MENU_GAME_FAQ,    "&Game FAQ..."       },
		{ MENU_ABOUT_INI,   "About &INI Files"   },
		{ MENU_ABOUT_PJ64,  "&About Project 64"  },
		{ MENU_GITHUB,      "GitHub"  },
		{ MENU_HOMEPAGE,    "&Homepage"  },
		{ MENU_DISCORD,     "&Discord"   },

//Pop up Menu
	{ POPUP_PLAY,      "Play Game"  },
	{ POPUP_INFO,      "Rom Information"  },
	{ POPUP_SETTINGS,  "Edit Game Settings"  },
	{ POPUP_CHEATS,    "Edit Cheats"  },
	{ POPUP_GAMEINFO,  "Game Information"},

/*********************************************************************************
* Rom Browser                                                                    *
*********************************************************************************/
//Rom Browser Fields
	{ RB_FILENAME,     "File Name" },
	{ RB_INTERNALNAME, "Internal Name" },
	{ RB_GOODNAME,     "Good Name" },
	{ RB_STATUS,       "Status" },
	{ RB_ROMSIZE,      "Rom Size" },
	{ RB_NOTES_CORE,   "Notes (Core)" },
	{ RB_NOTES_PLUGIN, "Notes (default plugins)" },
	{ RB_NOTES_USER,   "Notes (User)" },
	{ RB_CART_ID,      "Cartridge ID" },
	{ RB_RELEASE_VER,  "Release Version" },
	{ RB_SDK_VER,      "SDK Version" },
	{ RB_MANUFACTUER,  "Manufacturer" },
	{ RB_COUNTRY,      "Country" },
	{ RB_DEVELOPER,    "Developer" },
	{ RB_CRC1,         "CRC1" },
	{ RB_CRC2,         "CRC2" },
	{ RB_CICCHIP,      "CIC Chip" },
	{ RB_RELEASE_DATE, "Release Date" },
	{ RB_GENRE,        "Genre" },
	{ RB_PLAYERS,      "Players" },
	{ RB_FORCE_FEEDBACK,"Force Feedback" },

//Select Rom
	{ SELECT_ROM_DIR,  "Select current Rom Directory" },

//Messages, Changed from Bad ROM? Use GoodN64 & check for updated INI as most games are Prototypes or Hacks these days.
	{ RB_NOT_GOOD_FILE,"Unknown Rom? Check for good rom or add new entry" },

/*********************************************************************************
* Options                                                                        *
*********************************************************************************/
//Options Title
	{ OPTIONS_TITLE,STR_SETTINGS},

//Tabs
	{ TAB_PLUGIN,"Plugins"},
	{ TAB_DIRECTORY,"Directories"},
	{ TAB_OPTIONS,"Options"},
	{ TAB_ROMSELECTION,"Rom Selection"},
	{ TAB_ADVANCED,"Advanced"},
	{ TAB_ROMSETTINGS,"Rom Settings"},
	{ TAB_ROMNOTES,"Rom Notes"},
	{ TAB_SHELLINTERGATION,"Shell Integration"},

//Plugin Dialog
	{ PLUG_ABOUT, "About"},
	{ PLUG_RSP,   " Reality Signal Processor plugin: "},
	{ PLUG_GFX,   " Video (graphics) plugin: "},
	{ PLUG_AUDIO, " Audio (sound) plugin: "},
	{ PLUG_CTRL,  " Input (controller) plugin: "},

//Directory Dialog
	{ DIR_PLUGIN,        " Plugin Directoy: "},
	{ DIR_ROM,           " Rom Directory: "},
	{ DIR_AUTO_SAVE,     " N64 Auto saves: "},
	{ DIR_INSTANT_SAVE,  " Instant saves: "},
	{ DIR_SCREEN_SHOT,   " Screenshots: "},
	{ DIR_ROM_DEFAULT,   "Last folder that a rom was open from."},
	{ DIR_SELECT_PLUGIN, "Select Plugin Directory"},
	{ DIR_SELECT_ROM,    "Select Rom Directory"},
	{ DIR_SELECT_AUTO,   "Select Automatic save Directory"},
	{ DIR_SELECT_INSTANT,"Select Instant save Directory"},
	{ DIR_SELECT_SCREEN, "Select Screenshot Directory"},

//Options (general) Tab
	{ OPTION_AUTO_SLEEP,      "Pause emulation when window is not active?"},
	{ OPTION_AUTO_FULLSCREEN, "On loading a ROM go to full screen"},
	{ OPTION_BASIC_MODE,      "Hide Advanced Settings"},
	{ OPTION_REMEMBER_CHEAT,  "Remember selected cheats"},

//Rom Browser Tab
	{ RB_MAX_ROMS,         "Max # of Roms Remembered (Max 10):"},
	{ RB_ROMS,             "roms"},
	{ RB_MAX_DIRS,         "Max # of Rom Dirs Remembered (Max 10):"},
	{ RB_DIRS,             "dirs"},
	{ RB_USE,              "Use Rom Browser"},
	{ RB_DIR_RECURSION,    "Use Directory recursion"},
	{ RB_AVALIABLE_FIELDS, "Available fields:"},
	{ RB_SHOW_FIELDS,      "Show fields in this order:"},
	{ RB_ADD,              "Add ->"},
	{ RB_REMOVE,           "<- Remove"},
	{ RB_UP,               "Up"},
	{ RB_DOWN,             "Down"},

//Advanced Options
	{ ADVANCE_INFO,        "Most of these changes will not take effect till a new rom is opened or current rom is reset."},
	{ ADVANCE_DEFAULTS,    "Core Defaults"},
	{ ADVANCE_CPU_STYLE,   "CPU core style:"},
	{ ADVANCE_SMCM,        "Self-mod code method:"},
	{ ADVANCE_MEM_SIZE,    "Default Memory Size:"},
	{ ADVANCE_ABL,         "Advanced Block Linking:"},
	{ ADVANCE_AUTO_START,  "Start Emulation when rom is opened?"},
	{ ADVANCE_OVERWRITE,   "Always overwrite default settings with ones from ini?"},
	{ ADVANCE_COMPRESS,    "Automatically compress instant saves"},
	{ ADVANCE_CLEAR_MEMORY, "Clear Memory at Start of Emulation"},
	{ ADVANCE_USEDEBUGGER, "Enable Debugger (forces CPU Interpreter)" },
	{ ADVANCE_SHOREMORERRORS, "Show More Error Messages" },

//Rom Options
	{ ROM_CPU_STYLE,       "CPU core style:"},
	{ ROM_SMCM,            "Self-modifying code Method:"},
	{ ROM_MEM_SIZE,        "Memory Size:"},
	{ ROM_ABL,             "Advanced Block Linking:"},
	{ ROM_SAVE_TYPE,       "Default Save type:"},
	{ ROM_COUNTER_FACTOR,  "Counter Factor:"},
	{ ROM_LARGE_BUFFER,    "Larger Compile Buffer"},
	{ ROM_USE_TLB,         "Use TLB"},
	{ ROM_REG_CACHE,       "Register caching"},
	{ ROM_DELAY_SI,        "Delay SI Interrupt"},
	{ ROM_AUDIO_SIGNAL,    "RSP Audio Signal"},
	{ ROM_SP_HACK,         "SP Hack"},
	{ ROM_DEFAULT,         "Default"},
	{ ROM_DELAY_RDP,       "Delay RDP Interrupt"},
	{ ROM_DELAY_RSP,       "Delay RSP Interrupt"},
	{ ROM_EMULATE_AI,      "Emulate AI"},

//Core Styles
	{ CORE_INTERPTER,      "Interpreter"},
	{ CORE_RECOMPILER,     "Recompiler"},
	{ CORE_SYNC,           "Synchronise Cores"},

//Self Mod Methods
	{ SMCM_NONE,           "None"},
	{ SMCM_CACHE,          "Cache"},
	{ SMCM_PROECTED,       "Protect Memory"},
	{ SMCM_CHECK_MEM,      "Check Memory & Cache"},
	{ SMCM_CHANGE_MEM,     "Change Memory & Cache"},
	{ SMCM_CHECK_ADV,      "Check Memory Advance"},

//RDRAM Size
	{ RDRAM_4MB,           "4 MB"},
	{ RDRAM_8MB,           "8 MB"},

//Advanced Block Linking
	{ ABL_ON,              "On"},
	{ ABL_OFF,             "Off"},

//Save Type
	{ SAVE_FIRST_USED,     "Use First Used Save Type"},
	{ SAVE_4K_EEPROM,      "4kbit Eeprom"},
	{ SAVE_16K_EEPROM,     "16kbit Eeprom"},
	{ SAVE_SRAM,           "32kbytes SRAM"},
	{ SAVE_FLASHRAM,       "Flashram"},

//Shell Intergration Tab
	{ SHELL_TEXT,          "File extension association:"},

//Rom Notes
	{ NOTE_STATUS,         "Rom Status:"},
	{ NOTE_CORE,           "Core Note:"},
	{ NOTE_PLUGIN,         "Plugin Note:"},

/*********************************************************************************
* ROM Information                                                                *
*********************************************************************************/
//Rom Info Title Title
	{ INFO_TITLE,             "Rom Information"},

//Rom Info Text
	{ INFO_ROM_NAME_TEXT,     "ROM Name:"},
	{ INFO_FILE_NAME_TEXT,    "File Name:"},
	{ INFO_LOCATION_TEXT,     "Location:"},
	{ INFO_SIZE_TEXT,         "Rom Size:"},
	{ INFO_CART_ID_TEXT,      "Cartridge ID:"},
	{ INFO_RELEASE_VERSION,   "Release Version:"},
	{ INFO_SDK_VERSION,       "SDK Version:"},
	{ INFO_MANUFACTURER_TEXT, "Manufacturer:" },
	{ INFO_COUNTRY_TEXT,      "Country:"},
	{ INFO_CRC1_TEXT,         "CRC1:"},
	{ INFO_CRC2_TEXT,         "CRC2:"},
	{ INFO_CIC_CHIP_TEXT,     "CIC Chip:"},

/*********************************************************************************
* Cheats                                                                         *
*********************************************************************************/
//Cheat List
	{ CHEAT_TITLE,           "Cheats"},
	{ CHEAT_LIST_FRAME,      "Cheats:"},
	{ CHEAT_NOTES_FRAME,     " Notes: "},
	{ CHEAT_MARK_ALL,        "Mark All"},
	{ CHEAT_MARK_NONE,       "Unmark All"},

//Add Cheat
	{ CHEAT_ADDCHEAT_FRAME,  "Add Cheat"},
	{ CHEAT_ADDCHEAT_NAME,   "Name:"},
	{ CHEAT_ADDCHEAT_CODE,   "Code:"},
	{ CHEAT_ADDCHEAT_INSERT, "Insert"},
	{ CHEAT_ADDCHEAT_CLEAR,  "Clear"},
	{ CHEAT_ADDCHEAT_NOTES,  " Cheat Notes: "},
	{ CHEAT_ADD_TO_DB,       "Add to DB"},
	{ CHEAT_ADDCHEAT_ADD,    "Add Cheat"},
	{ CHEAT_ADDCHEAT_NEW,    "New Cheat"},
	{ CHEAT_ADDCHEAT_CODEDES,"<address> <value>"},
	{ CHEAT_ADDCHEAT_OPT,    "Options:"},
	{ CHEAT_ADDCHEAT_OPTDES, "<value> <label>"},

//Code extension
	{ CHEAT_CODE_EXT_TITLE,  "Code Extensions"},
	{ CHEAT_CODE_EXT_TXT,    "Please choose a value to be used for:"},
	{ CHEAT_OK,              "OK"},
	{ CHEAT_CANCEL,          "Cancel"},

//Digital Value
	{ CHEAT_QUANTITY_TITLE,  "Quantity Digit"},
	{ CHEAT_CHOOSE_VALUE,    "Please choose a value for:"},
	{ CHEAT_VALUE,           "&Value"},
	{ CHEAT_FROM,            "from"},
	{ CHEAT_TO,              "to"},
	{ CHEAT_NOTES,           "&Notes:"},

//Edit Cheat
	{ CHEAT_EDITCHEAT_WINDOW,"Edit Cheat"},
	{ CHEAT_EDITCHEAT_UPDATE,"Update Cheat"},

//Cheat Popup Menu
	{ CHEAT_ADDNEW,          "Add New Cheat..."},
	{ CHEAT_EDIT,            "Edit"},
	{ CHEAT_DELETE,          "Delete"},

/*********************************************************************************
* Messages                                                                       *
*********************************************************************************/
	{ MSG_CPU_PAUSED,         "*** CPU PAUSED ***"},
	{ MSG_CPU_RESUMED,        "CPU Resumed"},
	{ MSG_PERM_LOOP,          "In a permanent loop that cannot be exited. \nEmulation will now stop. \n\nVerify ROM and ROM Settings."},
	{ MSG_MEM_ALLOC_ERROR,    "Failed to allocate Memory"},
	{ MSG_FAIL_INIT_GFX,      "The default or selected video plugin is missing or invalid.\n\nYou need to go into Settings and select a video (graphics) plugin.\nCheck that you have at least one compatible plugin file in your plugin folder."},
	{ MSG_FAIL_INIT_AUDIO,    "The default or selected audio plugin is missing or invalid.\n\nYou need to go into Settings and select a audio (sound) plugin.\nCheck that you have at least one compatible plugin file in your plugin folder."},
	{ MSG_FAIL_INIT_RSP,      "The default or selected RSP plugin is missing or invalid. \n\nYou need to go into Settings and select an RSP plugin.\nCheck that you have at least one compatible plugin file in your plugin folder."},
	{ MSG_FAIL_INIT_CONTROL,  "The default or selected input plugin is missing or invalid. \n\nYou need to go into Settings and select a video (graphics) plugin.\nCheck that you have at least one compatible plugin file in your plugin folder."},
	{ MSG_FAIL_LOAD_PLUGIN,   "Failed to load plugin:"},
	{ MSG_FAIL_LOAD_WORD,     "Failed to load word\n\nVerify ROM and ROM Settings."},
	{ MSG_FAIL_OPEN_SAVE,     "Failed to open Save File"},
	{ MSG_FAIL_OPEN_EEPROM,   "Failed to open Eeprom"},
	{ MSG_FAIL_OPEN_FLASH,    "Failed to open Flashram"},
	{ MSG_FAIL_OPEN_MEMPAK,   "Failed to open mempak"},
	{ MSG_FAIL_OPEN_ZIP,      "Attempt to open zip file failed. \n\nProbably a corrupt zip file - try unzipping ROM manually."},
	{ MSG_FAIL_OPEN_IMAGE,    "Attempt to open file failed."},
	{ MSG_FAIL_ZIP,           "Error occured when trying to open zip file."},
	{ MSG_FAIL_IMAGE,         "File loaded does not appear to be a valid Nintendo64 ROM. \n\nVerify your ROMs with GoodN64."},
	{ MSG_UNKNOWN_COUNTRY,    "Unknown country"},
	{ MSG_UNKNOWN_CIC_CHIP,   "Unknown Cic Chip"},
	{ MSG_UNKNOWN_FILE_FORMAT,"Unknown file format"},
	{ MSG_UNKNOWN_MEM_ACTION, "Unknown memory action\n\nEmulation stop"},
	{ MSG_UNHANDLED_OP,       "Unhandled R4300i OpCode at"},
	{ MSG_NONMAPPED_SPACE,    "Executing from non-mapped space.\n\nVerify ROM and ROM Settings."},
	{ MSG_SAVE_STATE_HEADER,  "State save does not appear to match the running ROM. \n\nState saves must be saved & loaded between 100% identical ROMs, \nin particular the REGION and VERSION need to be the same. \nLoading this state is likely to cause the game and/or emulator to crash. \n\nAre you sure you want to continue loading?"},
	{ MSG_MSGBOX_TITLE,       "Error"},
	{ MSG_PIF2_ERROR,         "Copyright sequence not found in LUT.  Game will no longer function."},
	{ MSG_PIF2_TITLE,         "Copy Protection Failure"},
	{ MSG_PLUGIN_CHANGE,      "Changing a plugin requires MiB64 to reset a running ROM. \nIf you don't want to lose your place, answer No and make a state save first. \n\nChange plugins and restart game now?"},
	{ MSG_PLUGIN_CHANGE_TITLE,"Change Plugins"},
	{ MSG_EMULATION_ENDED,    "Emulation ended"},
	{ MSG_EMULATION_STARTED,  "Emulation started"},
	{ MSG_UNABLED_LOAD_STATE, "Unable to load save state"},
	{ MSG_LOADED_STATE,       "Loaded save state"},
	{ MSG_SAVED_STATE,        "Saved current state to"},
	{ MSG_SAVE_SLOT,          "Save state slot"},
	{ MSG_BYTESWAP,           "Byte swapping image"},
	{ MSG_CHOOSE_IMAGE,       "Choosing N64 image"},
	{ MSG_LOADED,             "Loaded"},
	{ MSG_LOADING,            "Loading image"},
	{ MSG_PLUGIN_NOT_INIT,    "Cannot open a rom because plugins have not successfully initialised"},
	{ MSG_DEL_SURE,           "Are you sure you really want to delete this?"},
	{ MSG_DEL_TITLE,          "Delete Cheat"},
	{ MSG_CHEAT_NAME_IN_USE,  "Cheat Name is already in use"},
	{ MSG_MAX_CHEATS,         "You Have reached the Maxiumn amount of cheats for this rom"},
	{ MSG_NO_GAME_INFORMATION,"No game information available"},
	{ MSG_FAIL_CREATE_TEMP,	  "Unable to create temporary ROM file"},
	{ MSG_SAVESTATE_OLDFORMAT,"State isn't compatible with current version of MiB64. \n\nLoading this state may cause the game and/or emulator to crash. \n\nAre you sure you want to continue loading?"},
};

class CLanguage  {
	void FindLangName  ( int Index );
	void LoadStrings   ( char * FileName );
	void SaveCurrentLang ( char * String );
    
	friend LRESULT CALLBACK LangSelectProc (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

public:
	CLanguage();

	void CreateLangList ( HMENU hMenu, int uPosition, int MenuID );
	char * GetString    ( int StringID );
	void LoadLangList   ( void );
	void LoadLanguage  ();
	const char * LangName  ( int index );
	int  GetNumberLang  ( void );
	void SetCurrentLang ( HMENU hMenu, int MenuIndx );
	int  SetMenuBase    ( int MenuBase );

private:
	LANG_STR m_Strings[MAX_STRINGS];
	char m_filenames[MAX_LANGUAGES][_MAX_PATH];
	char m_LangName[MAX_LANGUAGES][MAX_LANNAME_LEN];
	char m_CurrentLangName[MAX_LANNAME_LEN];
	int m_NoOflangs;
	int m_NoOfStrings;
	int m_BaseMenuID;
};

LRESULT CALLBACK LangSelectProc (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);

/*******************************************************************************
* Variables                                                                    *
*******************************************************************************/
CLanguage lng;


/*******************************************************************************
* Code                                                                         *
*******************************************************************************/
CLanguage::CLanguage() {
	m_NoOflangs = 0;
	m_NoOfStrings = 0;
	strcpy(m_CurrentLangName,STR_EMPTY);
}

int CLanguage::GetNumberLang(void) {
	return m_NoOflangs;
}

const char * CLanguage::LangName  ( int index ) {
	if (index >= MAX_LANGUAGES) { return NULL; }
	return m_LangName[index];
}

void CLanguage::LoadLanguage  () {
	char *String;
	LoadLangList();

	Settings_Read(APPS_NAME, STR_SETTINGS, STR_LANGUAGE, STR_EMPTY, &String);
	strncpy(m_CurrentLangName, String, sizeof(m_CurrentLangName));
	if (String) free(String);

	bool Found = false;
	for (int count = 0; count < m_NoOflangs; count++) {
		if (strcmp(m_LangName[count], m_CurrentLangName) == 0) {
			LoadStrings(m_filenames[count]);
			Found = true;
			break;
		}
	}
	
	if (!Found) {
		strcpy(m_CurrentLangName,STR_EMPTY);
		//Do Dialog box to choose language
		DialogBoxParam(hInst,MAKEINTRESOURCE(IDD_LangSelect),NULL,(DLGPROC)LangSelectProc, (LPARAM)this);
	}
}

void CLanguage::CreateLangList ( HMENU hMenu, int uPosition, int MenuID ) {
	LoadLangList();

	HMENU hSubMenu = CreateMenu();
	MENUITEMINFO menuinfo;
	char String[100];

	menuinfo.cbSize = sizeof(MENUITEMINFO);
	menuinfo.fMask = MIIM_TYPE|MIIM_ID;
	menuinfo.fType = MFT_STRING;
	menuinfo.dwTypeData = String;
	menuinfo.cch = sizeof(String);

	if (GetNumberLang() == 0) {
		menuinfo.wID = MenuID;
		strcpy(String,"English");
		InsertMenuItem(hSubMenu, 0, TRUE, &menuinfo);
		CheckMenuItem(hSubMenu, MenuID, MF_BYCOMMAND | MFS_CHECKED );
		EnableMenuItem(hSubMenu,MenuID, MFS_DISABLED|MF_BYCOMMAND);		
	}
	for (int count = 0; count < GetNumberLang(); count ++) {
		menuinfo.wID = MenuID + count;
		strcpy(String,lng.LangName(count));
		InsertMenuItem(hSubMenu, 0, TRUE, &menuinfo);
		if (strcmp(m_CurrentLangName, String) == 0) {
			CheckMenuItem(hSubMenu, menuinfo.wID, MF_BYCOMMAND | MFS_CHECKED );
		}
	}
	ModifyMenu(hMenu,uPosition,MF_STRING|MF_POPUP|MF_BYPOSITION,(DWORD)hSubMenu,GS(MENU_LANGUAGE));
}

char * CLanguage::GetString (int StringID) {
	for (int count = 0; count < m_NoOfStrings; count ++) {
		if (m_Strings[count].ID == StringID) { return m_Strings[count].Str; }
	}
	return NULL;
}

void CLanguage::LoadLangList (void) {
	char Directory[_MAX_PATH], SearchString[_MAX_PATH];
	{
		char path_buffer[_MAX_PATH], drive[_MAX_DRIVE] ,dir[_MAX_DIR];
		char fname[_MAX_FNAME],ext[_MAX_EXT];

		GetModuleFileName(NULL,path_buffer,sizeof(path_buffer));
		_splitpath( path_buffer, drive, dir, fname, ext );
		sprintf(Directory,"%s%sLang\\",drive,dir);
		sprintf(SearchString,"%s*.pj.Lang",Directory);
	}

	WIN32_FIND_DATA find_data;
	HANDLE   search_handle;
	search_handle = FindFirstFile(SearchString, &find_data);
	m_NoOflangs=0;
	if(search_handle !=INVALID_HANDLE_VALUE){
		do {
			strcpy( m_filenames[m_NoOflangs], Directory );
			strcat( m_filenames[m_NoOflangs], find_data.cFileName );
			m_NoOflangs += 1;
		} while (FindNextFile(search_handle, &find_data ) && search_handle != INVALID_HANDLE_VALUE);
		FindClose(search_handle);
	}

	for (int count = 0; count < m_NoOflangs; count ++) { FindLangName(count); }
}

void CLanguage::LoadStrings  ( char * FileName ) {
	m_NoOfStrings = 0;

	FILE *file = fopen(FileName, "rb");
	if (file == NULL) { return; }

	char  token=0;
	while(!feof(file)){
		token = 0;

		//Search for token #
		while(token!='#' && !feof(file)) { fread(&token, 1, 1, file); }
		if(feof(file)){ continue; } 
		
		//get StringID after token
		fscanf(file, "%d", &m_Strings[m_NoOfStrings].ID);
	
		//Search for token #
		while(token!='#' && !feof(file)) { fread(&token, 1, 1, file); }
		if(feof(file)){ continue; } 

		//Search for start of string '"'
		while(token!='"' && !feof(file)) { fread(&token, 1, 1, file); }
		if(feof(file)){ continue; } 		

		int pos = 0;
		fread(&token, 1, 1, file); 
		while(token!='"' && !feof(file)){ 
			m_Strings[m_NoOfStrings].Str[pos++] = token;
			fread(&token, 1, 1, file); 
			if (pos == MAX_STRING_LEN - 2) { token = '"'; }
		}
		m_Strings[m_NoOfStrings].Str[pos++] = 0;
		m_NoOfStrings += 1;
		if (m_NoOfStrings == MAX_STRINGS) { break; }
	}
	fclose(file);
}


void CLanguage::FindLangName  ( int Index ) {
	strcpy(m_LangName[Index],"Unknown");

	FILE *file = fopen(m_filenames[Index], "rb");
	if (file == NULL) { return; }

	char  token=0;
	int   StringID;

	while(!feof(file)){
		token = 0;

		//Search for token #
		while(token!='#' && !feof(file)) { fread(&token, 1, 1, file); }
		if(feof(file)){ continue; } 
		
		//get StringID after token
		fscanf(file, "%d", &StringID);
	
		//Search for token #
		while(token!='#' && !feof(file)) { fread(&token, 1, 1, file); }
		if(feof(file)){ continue; } 
		if (StringID != LANGUAGE_NAME) { continue; }

		//Search for start of string '"'
		while(token!='"' && !feof(file)) { fread(&token, 1, 1, file); }
		if(feof(file)){ continue; } 		
		
		int pos = 0;
		fread(&token, 1, 1, file); 
		while(token!='"' && !feof(file)){ 
			m_LangName[Index][pos++] = token;
			fread(&token, 1, 1, file); 
			if (pos == sizeof(m_LangName[Index]) - 2) { token = '"'; }
		}
		m_LangName[Index][pos++] = 0;		
	}
	fclose(file);

}

void CLanguage::SaveCurrentLang ( char * String ) {
	Settings_Write(APPS_NAME, STR_SETTINGS, STR_LANGUAGE, String);

	strcpy(m_CurrentLangName, String);

	for (int count = 0; count < m_NoOflangs; count++) {
		if (strcmp(m_LangName[count], m_CurrentLangName) == 0) {
			LoadStrings(m_filenames[count]);
			break;
		}
	}
}

void CLanguage::SetCurrentLang ( HMENU hMenu, int MenuIndx ) {
	MENUITEMINFO menuinfo;
	char String[MAX_LANNAME_LEN];

	menuinfo.cbSize = sizeof(MENUITEMINFO);
	menuinfo.fMask = MIIM_TYPE;
	menuinfo.fType = MFT_STRING;
	menuinfo.dwTypeData = String;
	menuinfo.cch = sizeof(String);
	GetMenuItemInfo(hMenu,MenuIndx,FALSE,&menuinfo);
	SaveCurrentLang(String);
}

LRESULT CALLBACK LangSelectProc (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	static CLanguage * lngClass;

	switch (uMsg) {
	case WM_INITDIALOG:
		SetWindowPos(hDlg,HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOREPOSITION|SWP_NOSIZE);
		{
			lngClass = (CLanguage *)lParam;
			
			if (lngClass->m_NoOflangs == 0) { EndDialog(hDlg,0); }
			for (int count = 0; count < lngClass->m_NoOflangs; count ++) {
				int index = SendMessage(GetDlgItem(hDlg,IDC_LANG_SEL),CB_ADDSTRING,0,(WPARAM)&lngClass->m_LangName[count][0]);
				if (strcmp(&lngClass->m_LangName[count][0],"English") == 0) {
					SendMessage(GetDlgItem(hDlg,IDC_LANG_SEL),CB_SETCURSEL,index,0);
				}
			}
			int Index = SendMessage(GetDlgItem(hDlg,IDC_LANG_SEL),CB_GETCURSEL,0,0);
			if (Index < 0) { SendMessage(GetDlgItem(hDlg,IDC_LANG_SEL),CB_SETCURSEL,0,0); }
		}
		break;
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDOK:
			{
				int Index = SendMessage(GetDlgItem(hDlg,IDC_LANG_SEL),CB_GETCURSEL,0,0);
				
				if (Index >= 0) { 
					char String[255];
					SendMessage(GetDlgItem(hDlg,IDC_LANG_SEL),CB_GETLBTEXT,Index,(LPARAM)String);
					lngClass->SaveCurrentLang(String);
				}
			}

			EndDialog(hDlg,0);
			break;
		}
	default:
		return FALSE;
	}
	return TRUE;
}

char * GS (int StringID) {
	int count;

	char * Ret = lng.GetString(StringID);
	if (Ret != NULL) { return Ret; }

	for (count = 0; count < (sizeof(DefaultString) / sizeof(LANG_STR)); count ++) {
		if (DefaultString[count].ID == StringID) { return DefaultString[count].Str; }
	}
	return STR_EMPTY;
}

void CreateLangList (HMENU hMenu, int uPosition, int MenuID) {
	lng.CreateLangList(hMenu,uPosition,MenuID);
}

void LoadLanguage () {
	lng.LoadLanguage();
}

void SelectLangMenuItem ( HMENU hMenu, int LangMenuID) {
	lng.SetCurrentLang(hMenu,LangMenuID);
}
