#if defined(__cplusplus)
extern "C" {
#endif

void LoadLanguage       ();
void CreateLangList     ( HMENU hMenu, int uPosition, int MenuID );
void SelectLangMenuItem ( HMENU hMenu, int LangMenuID );
char * GS               ( int StringID );

#if defined(__cplusplus)
}
#endif

/*********************************************************************************
* Meta Information                                                               *
*********************************************************************************/
#define LANGUAGE_NAME	        1
#define LANGUAGE_AUTHOR	        2
#define LANGUAGE_VERSION        3
#define LANGUAGE_DATE	        4
#define INI_CURRENT_LANG        5
#define INI_AUTHOR		        6
#define INI_VERSION		        7
#define INI_DATE		        8
#define INI_HOMEPAGE	        9
#define INI_CURRENT_RDB         10
#define INI_CURRENT_CHT         11
#define INI_CURRENT_RDX         12

//About INI title
#define INI_TITLE               20

/*********************************************************************************
* Numbers                                                                        *
*********************************************************************************/
#define NUMBER_0		        50
#define NUMBER_1		        51
#define NUMBER_2		        52
#define NUMBER_3		        53
#define NUMBER_4		        54
#define NUMBER_5		        55
#define NUMBER_6		        56
#define NUMBER_7		        57
#define NUMBER_8		        58
#define NUMBER_9		        59

// Menu
#define MENU_FILE				100
	#define MENU_OPEN			101
	#define MENU_ROM_INFO		102
	#define MENU_START			103
	#define MENU_END			104
	#define MENU_CHOOSE_ROM		105
	#define MENU_REFRESH		106
	#define MENU_RECENT_ROM		107
	#define MENU_RECENT_DIR		108
	#define MENU_EXIT			109
    #define MENU_GAME_INFO		110

#define MENU_SYSTEM				120
	#define MENU_RESET			121
	#define MENU_PAUSE			122
	#define MENU_BITMAP			123
	#define MENU_LIMIT_FPS		124
	#define MENU_SAVE			125
	#define MENU_SAVE_AS		126
	#define MENU_RESTORE		127
	#define MENU_LOAD			128
	#define MENU_CURRENT_SAVE	129
	#define MENU_CHEAT			130
	#define MENU_GS_BUTTON		131
	#define MENU_RESUME			132
	#define MENU_CHEATSEARCH	133

#define MENU_OPTIONS			140
	#define MENU_FULL_SCREEN	141
	#define MENU_ON_TOP			142
	#define MENU_CONFG_GFX		143
	#define MENU_CONFG_AUDIO	144
	#define MENU_CONFG_CTRL		145
	#define MENU_CONFG_RSP		146
	#define MENU_SHOW_CPU		147
	#define MENU_SETTINGS		148

#define MENU_DEBUGGER			160

#define MENU_LANGUAGE			175

#define MENU_HELP				180
	#define MENU_USER_MAN		181
	#define MENU_GAME_FAQ		182
	#define MENU_ABOUT_INI		183
	#define MENU_ABOUT_PJ64		184
	#define MENU_GITHUB			185
	#define MENU_HOMEPAGE		186
	#define MENU_DISCORD		187

//Current Save Slot menu
#define MENU_SLOT_DEFAULT		190
#define MENU_SLOT_1				191
#define MENU_SLOT_2				192
#define MENU_SLOT_3				193
#define MENU_SLOT_4				194
#define MENU_SLOT_5				195
#define MENU_SLOT_6				196
#define MENU_SLOT_7				197
#define MENU_SLOT_8				198
#define MENU_SLOT_9				199

// Added an extra 10 save state slots with 10-19 on Shift+0-9 (Gent)

#define MENU_SLOT_10			200
#define MENU_SLOT_11			2048
#define MENU_SLOT_12			2049
#define MENU_SLOT_13			2050
#define MENU_SLOT_14			2051
#define MENU_SLOT_15			2052
#define MENU_SLOT_16			2053
#define MENU_SLOT_17			2054
#define MENU_SLOT_18			2055
#define MENU_SLOT_19			2056

//Pop up Menu
#define POPUP_PLAY				210
#define POPUP_INFO				211
#define POPUP_SETTINGS			212
#define POPUP_CHEATS			213
#define POPUP_GAMEINFO			214

// Menu Descriptions
#define MENUDES_OPEN			250
#define MENUDES_ROM_INFO		251
#define MENUDES_START			252
#define MENUDES_END				253
#define MENUDES_CHOOSE_ROM		254
#define MENUDES_REFRESH			255
#define MENUDES_EXIT			256
#define MENUDES_RESET			257
#define MENUDES_PAUSE			258
#define MENUDES_BITMAP			259
#define MENUDES_LIMIT_FPS		260
#define MENUDES_SAVE			261
#define MENUDES_SAVE_AS			262
#define MENUDES_RESTORE			263
#define MENUDES_LOAD			264
#define MENUDES_CHEAT			265
#define MENUDES_GS_BUTTON		266
#define MENUDES_FULL_SCREEN		267
#define MENUDES_ON_TOP			268
#define MENUDES_CONFG_GFX		269
#define MENUDES_CONFG_AUDIO		270
#define MENUDES_CONFG_CTRL		271
#define MENUDES_CONFG_RSP		272
#define MENUDES_SHOW_CPU		273
#define MENUDES_SETTINGS		274
#define MENUDES_USER_MAN		275
#define MENUDES_GAME_FAQ		276
#define MENUDES_ABOUT_INI		277
#define MENUDES_ABOUT_PJ64		278
#define MENUDES_RECENT_ROM		279
#define MENUDES_RECENT_DIR		280
#define MENUDES_LANGUAGES		281
#define MENUDES_GAME_SLOT		282
#define MENUDES_PLAY_GAME		283
#define MENUDES_GAME_INFO		284
#define MENUDES_GAME_SETTINGS	285
#define MENUDES_GAME_CHEATS		286
#define MENUDES_GAMEINFORMATION 287

/*********************************************************************************
* Rom Browser                                                                    *
*********************************************************************************/
//Rom Browser Fields
#define RB_FILENAME				300
#define RB_INTERNALNAME			301
#define RB_GOODNAME				302
#define RB_STATUS				303
#define RB_ROMSIZE				304
#define RB_NOTES_CORE			305
#define RB_NOTES_PLUGIN			306
#define RB_NOTES_USER			307
#define RB_CART_ID				308
#define RB_RELEASE_VER			309
#define RB_SDK_VER				310
#define RB_MANUFACTUER			311
#define RB_COUNTRY				312
#define RB_DEVELOPER			313
#define RB_CRC1					314
#define RB_CRC2					315
#define RB_CICCHIP				316
#define RB_RELEASE_DATE			317
#define RB_GENRE				318
#define RB_PLAYERS				319
#define RB_FORCE_FEEDBACK		320

//Select Rom
#define SELECT_ROM_DIR			321

//Messages
#define RB_NOT_GOOD_FILE		340

/*********************************************************************************
* Options                                                                        *
*********************************************************************************/
//Options Title
#define OPTIONS_TITLE			400

//Tabs
#define TAB_PLUGIN				401
#define TAB_DIRECTORY			402
#define TAB_OPTIONS				403
#define TAB_ROMSELECTION		404
#define TAB_ADVANCED			405
#define TAB_ROMSETTINGS			406
#define TAB_SHELLINTERGATION	407
#define TAB_ROMNOTES			408

//Plugin Dialog
#define PLUG_ABOUT				420
#define PLUG_RSP				421
#define PLUG_GFX				422
#define PLUG_AUDIO				423
#define PLUG_CTRL				424

//Directory Dialog
#define DIR_PLUGIN				440
#define DIR_ROM					441
#define DIR_AUTO_SAVE			442
#define DIR_INSTANT_SAVE		443
#define DIR_SCREEN_SHOT			444
#define DIR_ROM_DEFAULT			445
#define DIR_SELECT_PLUGIN		446
#define DIR_SELECT_ROM			447
#define DIR_SELECT_AUTO			448
#define DIR_SELECT_INSTANT		449
#define DIR_SELECT_SCREEN		450

//Options (general) Tab
#define OPTION_AUTO_SLEEP		460
#define OPTION_AUTO_FULLSCREEN	461
#define OPTION_BASIC_MODE		462
#define OPTION_REMEMBER_CHEAT	463

//Rom Browser Tab
#define RB_MAX_ROMS				480
#define RB_ROMS					481
#define RB_MAX_DIRS				482
#define RB_DIRS					483
#define RB_USE					484
#define RB_DIR_RECURSION		485
#define RB_AVALIABLE_FIELDS		486
#define RB_SHOW_FIELDS			487
#define RB_ADD					488
#define RB_REMOVE				489
#define RB_UP					490
#define RB_DOWN					491

//Advanced Options
#define ADVANCE_INFO			500
#define ADVANCE_DEFAULTS		501
#define ADVANCE_CPU_STYLE		502
#define ADVANCE_SMCM			503
#define ADVANCE_MEM_SIZE		504
#define ADVANCE_ABL				505
#define ADVANCE_AUTO_START		506
#define ADVANCE_OVERWRITE		507
#define ADVANCE_COMPRESS		508
#define ADVANCE_CLEAR_MEMORY    509
#define ADVANCE_USEDEBUGGER     510
#define ADVANCE_SHOREMORERRORS  511

//Rom Options
#define ROM_CPU_STYLE			520
#define ROM_SMCM				521
#define ROM_MEM_SIZE			522
#define ROM_ABL					523
#define ROM_SAVE_TYPE			524
#define ROM_COUNTER_FACTOR		525
#define ROM_LARGE_BUFFER		526
#define ROM_USE_TLB				527
#define ROM_REG_CACHE			528
#define ROM_DELAY_SI			529
#define ROM_SP_HACK				530
#define ROM_DEFAULT				531
#define ROM_AUDIO_SIGNAL		532
#define ROM_DELAY_RDP			533
#define ROM_DELAY_RSP			534
#define ROM_EMULATE_AI          535

//Core Styles
#define CORE_INTERPTER			540
#define CORE_RECOMPILER			541
#define CORE_SYNC				542

//Core Styles
#define SMCM_NONE				560
#define SMCM_CACHE				561
#define SMCM_PROECTED			562
#define SMCM_CHECK_MEM			563
#define SMCM_CHANGE_MEM			564
#define SMCM_CHECK_ADV			565

//RDRAM Size
#define RDRAM_4MB				580
#define RDRAM_8MB				581

//Advanced Block Linking
#define ABL_ON					600
#define ABL_OFF					601

//Save Type
#define SAVE_FIRST_USED			620
#define SAVE_4K_EEPROM			621
#define SAVE_16K_EEPROM			622
#define SAVE_SRAM				623
#define SAVE_FLASHRAM			624

//Shell Intergration Tab
#define SHELL_TEXT				640

//Rom Notes
#define NOTE_STATUS				660
#define NOTE_CORE				661
#define NOTE_PLUGIN				662

/*********************************************************************************
* ROM Information                                                                *
*********************************************************************************/
//Rom Info Title Title
#define INFO_TITLE				800

//Rom Info Text
#define INFO_ROM_NAME_TEXT		801
#define INFO_FILE_NAME_TEXT		802
#define INFO_LOCATION_TEXT		803
#define INFO_SIZE_TEXT			804
#define INFO_CART_ID_TEXT		805
#define INFO_MANUFACTURER_TEXT	806
#define INFO_COUNTRY_TEXT		807
#define INFO_CRC1_TEXT			808
#define INFO_CRC2_TEXT			809
#define INFO_CIC_CHIP_TEXT		810
#define INFO_RELEASE_VERSION	811
#define INFO_SDK_VERSION		812

/*********************************************************************************
* Cheats                                                                         *
*********************************************************************************/
//Cheat List
#define CHEAT_TITLE				1000
#define CHEAT_LIST_FRAME		1001
#define CHEAT_NOTES_FRAME		1002
#define CHEAT_MARK_ALL			1003
#define CHEAT_MARK_NONE			1004

//Add Cheat
#define CHEAT_ADDCHEAT_FRAME	1005
#define CHEAT_ADDCHEAT_NAME		1006
#define CHEAT_ADDCHEAT_CODE		1007
#define CHEAT_ADDCHEAT_INSERT	1008
#define CHEAT_ADDCHEAT_CLEAR	1009
#define CHEAT_ADDCHEAT_NOTES	1010
#define CHEAT_ADD_TO_DB 		1011
#define CHEAT_ADDCHEAT_ADD 		1022
#define CHEAT_ADDCHEAT_NEW 		1023
#define CHEAT_ADDCHEAT_CODEDES 	1024
#define CHEAT_ADDCHEAT_OPT 		1025
#define CHEAT_ADDCHEAT_OPTDES 	1026

//Code extension
#define CHEAT_CODE_EXT_TITLE	1012
#define CHEAT_CODE_EXT_TXT		1013
#define CHEAT_OK				1014
#define CHEAT_CANCEL			1015

//Digital Value
#define CHEAT_QUANTITY_TITLE	1016
#define CHEAT_CHOOSE_VALUE		1017
#define CHEAT_VALUE				1018
#define CHEAT_FROM				1019
#define CHEAT_TO				1020
#define CHEAT_NOTES				1021

//Edit Cheat
#define CHEAT_EDITCHEAT_WINDOW	1027
#define CHEAT_EDITCHEAT_UPDATE	1028

//Cheat Popup Menu
#define CHEAT_ADDNEW			1040
#define CHEAT_EDIT				1041
#define CHEAT_DELETE			1042

/*********************************************************************************
* Messages                                                                       *
*********************************************************************************/
#define MSG_CPU_PAUSED			2000
#define MSG_CPU_RESUMED			2001
#define MSG_PERM_LOOP           2002
#define MSG_MEM_ALLOC_ERROR     2003
#define MSG_FAIL_INIT_GFX       2004
#define MSG_FAIL_INIT_AUDIO     2005
#define MSG_FAIL_INIT_RSP       2006
#define MSG_FAIL_INIT_CONTROL   2007
#define MSG_FAIL_LOAD_PLUGIN    2008
#define MSG_FAIL_LOAD_WORD      2009
#define MSG_FAIL_OPEN_SAVE      2010
#define MSG_FAIL_OPEN_EEPROM    2011
#define MSG_FAIL_OPEN_FLASH     2012
#define MSG_FAIL_OPEN_MEMPAK    2013
#define MSG_FAIL_OPEN_ZIP       2014
#define MSG_FAIL_OPEN_IMAGE     2015
#define MSG_FAIL_ZIP            2016
#define MSG_FAIL_IMAGE          2017
#define MSG_UNKNOWN_COUNTRY     2018
#define MSG_UNKNOWN_CIC_CHIP    2019
#define MSG_UNKNOWN_FILE_FORMAT 2020
#define MSG_UNKNOWN_MEM_ACTION  2021
#define MSG_UNHANDLED_OP        2022
#define MSG_NONMAPPED_SPACE     2023
#define MSG_SAVE_STATE_HEADER   2024
#define MSG_MSGBOX_TITLE        2025
#define MSG_PIF2_ERROR          2026
#define MSG_PIF2_TITLE          2027
#define MSG_PLUGIN_CHANGE       2028
#define MSG_PLUGIN_CHANGE_TITLE 2029
#define MSG_EMULATION_ENDED     2030
#define MSG_EMULATION_STARTED   2031
#define MSG_UNABLED_LOAD_STATE  2032
#define MSG_LOADED_STATE        2033
#define MSG_SAVED_STATE         2034
#define MSG_SAVE_SLOT           2035
#define MSG_BYTESWAP            2036
#define MSG_CHOOSE_IMAGE        2037
#define MSG_LOADED              2038
#define MSG_LOADING             2039
#define MSG_PLUGIN_NOT_INIT     2040
#define MSG_DEL_SURE            2041
#define MSG_DEL_TITLE           2042
#define MSG_CHEAT_NAME_IN_USE   2043
#define MSG_MAX_CHEATS          2044
#define MSG_NO_GAME_INFORMATION	2045
#define MSG_FAIL_CREATE_TEMP	2046
#define MSG_SAVESTATE_OLDFORMAT 2047
