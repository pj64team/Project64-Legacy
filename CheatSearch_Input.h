#ifndef CS_INPUT_H
#define CS_INPUT_H

typedef struct CUSTOM_PROC {
	WNDPROC callBack;
	HWND hWnd;
	int max_value;
	int value;
} CUSTOM_PROC;

struct CUSTOM_PROC VALUE_DEC;
struct CUSTOM_PROC VALUE_HEX;
struct CUSTOM_PROC ADDR_HEX;
struct CUSTOM_PROC VALUE_SEARCH;

LRESULT CALLBACK CheatSearch_Window_Proc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK CheatSearch_Add_Proc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK HexEditControlProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK DecEditControlProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

BOOL isDec(WPARAM wParam);
BOOL isHex(WPARAM wParam);
BOOL isSpecial(WPARAM wParam);

CUSTOM_PROC *getOriginalProc(HWND hWnd);

#endif