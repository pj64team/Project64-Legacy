// RadeonUser -- All input verification will be moved to over here
// The code was bloating up a bit too much and I was having difficulty navigating

#include <Windows.h>
#include "CheatSearch_Input.h"

LRESULT CALLBACK HexEditControlProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	struct CUSTOM_PROC *examine;

	// The original unsubclassed control
	examine = getOriginalProc(hWnd);

	// Failed to ID, so don't do anything just hope the default proc handles things
	if (examine == NULL)
		return DefWindowProc(hWnd, uMsg, wParam, lParam);

	switch (uMsg)
	{
	case WM_CHAR:
		if (isHex(wParam) || isSpecial(wParam))
			return CallWindowProc(examine->callBack, hWnd, uMsg, wParam, lParam);

		return 0;
	case WM_DESTROY:
		SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR)examine->callBack);
		return DefWindowProc(hWnd, uMsg, wParam, lParam);
	}
	return CallWindowProc(examine->callBack, hWnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK DecEditControlProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	struct CUSTOM_PROC *examine;

	// The original unsubclassed control
	examine = getOriginalProc(hWnd);

	// Failed to ID, so don't do anything just hope the default proc handles things
	if (examine == NULL)
		return DefWindowProc(hWnd, uMsg, wParam, lParam);

	switch (uMsg)
	{
	case WM_CHAR:
		if (isDec(wParam) || isSpecial(wParam))
			return CallWindowProc(examine->callBack, hWnd, uMsg, wParam, lParam);

		return 0;
	case WM_DESTROY:
		SetWindowLongPtr(hWnd, GWLP_WNDPROC, (LONG_PTR)examine->callBack);
		return DefWindowProc(hWnd, uMsg, wParam, lParam);
	}
	return CallWindowProc(examine->callBack, hWnd, uMsg, wParam, lParam);
}

BOOL isDec(WPARAM wParam)
{
	switch(wParam) {
	case 0x30:	//0
	case 0x31:	//1
	case 0x32:	//2
	case 0x33:	//3
	case 0x34:	//4
	case 0x35:	//5
	case 0x36:	//6
	case 0x37:	//7
	case 0x38:	//8
	case 0x39:	//9
		return TRUE;
	}
	return FALSE;
}

BOOL isHex(WPARAM wParam)
{ 
	// These share the same values for 0 through 9
	if (isDec(wParam)) return TRUE;

	switch(wParam) {
	case 0x41:	//A
	case 0x42:	//B
	case 0x43:	//C
	case 0x44:	//D
	case 0x45:	//E
	case 0x46:	//F
	case 0x61:	//a
	case 0x62:	//b
	case 0x63:	//c
	case 0x64:	//d
	case 0x65:	//e
	case 0x66:	//f
		return TRUE;
	}

	return FALSE;
}

BOOL isSpecial(WPARAM wParam)
{ 
	switch(wParam) {
	case 0x08:	// VK_BACKSPACE
	case 0x09:	// VK_TAB
		return TRUE;
	}
	return FALSE;
}

CUSTOM_PROC *getOriginalProc(HWND hWnd)
{
	if (hWnd == VALUE_HEX.hWnd)
		return &VALUE_HEX;

	else if (hWnd == VALUE_SEARCH.hWnd)
		return &VALUE_SEARCH;

	else if (hWnd == VALUE_DEC.hWnd)
		return &VALUE_DEC;

	else if (hWnd == ADDR_HEX.hWnd)
		return &ADDR_HEX;

	else
		return NULL;
}