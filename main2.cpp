#include <windows.h>
#include <ATLBASE.H>
#include <EXDISP.h>
#include <CommCtrl.h>
#include "main.h"
#include "resource.h"

LRESULT CALLBACK AboutBoxProc (HWND, UINT, WPARAM, LPARAM);
void AboutCenterImage (HWND hDlg);

int ComputeHeight(HFONT font, RECT rect, char* message);
void DrawBoxText(HDC dc, RECT rect, HFONT font, int location);
void FillHelper();
void FreeHelper();
void ComputeDimensions(HWND hDlg);
void SetScrollBarSize(HWND hDlg);
void AdjustRectangle(RECT *rect);
void DrawBox(HDC dc, RECT rect);
void DrawTitle(HDC dc, RECT rect);

typedef struct {
	int allocated;
	int count;
	char **strings;

	int *height;
	int *string_height;
	int name_width;
	int desc_width;

	HFONT title_font;
	HFONT title_font_i;
	HFONT name_font;
	HFONT desc_font;
} AboutHelper;

AboutHelper helper;
#define left_margin 10
#define title_padding 10
#define text_padding 20
HWND hScroll;

void AboutBox (void) {
	DialogBox(hInst, MAKEINTRESOURCE(IDD_About), hMainWindow, (DLGPROC)AboutBoxProc);
}

LRESULT CALLBACK AboutBoxProc (HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch (uMsg) {
	case WM_INITDIALOG:
		{
			HDC hdc = GetDC(NULL);
			int point1 = -MulDiv(12, GetDeviceCaps(hdc, LOGPIXELSY), 72);
			int point2 = -MulDiv(14, GetDeviceCaps(hdc, LOGPIXELSY), 72);
			ReleaseDC(NULL, hdc);

			SetWindowText(hDlg, "About MiB64 - " __DATE__ " " __TIME__);

			SendMessage(hDlg, WM_SETICON, ICON_SMALL, (LPARAM)LoadIcon(hInst, "ICON"));

			CoInitialize(NULL);
			AboutCenterImage(hDlg);
			hScroll = GetDlgItem(hDlg, IDC_ABOUT_SCROLL);

			if (helper.allocated != 0) { FreeHelper(); }

			helper.title_font = CreateFont(point2,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_OUTLINE_PRECIS,
				CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY, VARIABLE_PITCH,TEXT("Times"));

			helper.title_font_i = CreateFont(point2,0,0,0,FW_BOLD,TRUE,FALSE,FALSE,DEFAULT_CHARSET,OUT_OUTLINE_PRECIS,
				CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY, VARIABLE_PITCH,TEXT("Times"));
			
			helper.name_font = CreateFont(point1,0,0,0,FW_BOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_OUTLINE_PRECIS,
				CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY, VARIABLE_PITCH,TEXT("Times"));
			
			helper.desc_font = CreateFont(point1,0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_OUTLINE_PRECIS,
				CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY, VARIABLE_PITCH,TEXT("Times"));

			FillHelper();
			ComputeDimensions(hDlg);
			SetScrollBarSize(hDlg);

			break;
		}
	case WM_VSCROLL:
		{
			int CurPos, NewPos, min, max;
			
			GetScrollRange(hScroll, SB_CTL, &min, &max);
			NewPos = CurPos = GetScrollPos(hScroll, SB_CTL);

			switch (LOWORD(wParam)) {
			case SB_TOP:
				NewPos = 0;
				break;
			
			case SB_LINEUP:
				if (CurPos > 0)
					NewPos -= 10;
				break;

			case SB_THUMBPOSITION:
				NewPos = HIWORD(wParam);
				break;
			
			case SB_THUMBTRACK:
				NewPos = HIWORD(wParam);
				break;
			
			case SB_LINEDOWN:
				if (CurPos < max)
					NewPos += 10;
				break;
			
			case SB_BOTTOM:
				NewPos = max;
				break;
			
			case SB_ENDSCROLL:
				break;
			}
			
			SetScrollPos(hScroll, SB_CTL, NewPos, TRUE);
			if (NewPos != CurPos)
				InvalidateRect(hDlg, NULL, FALSE);
			break;
		}
	case WM_PAINT:
		{
			PAINTSTRUCT paint;
			HWND hWnd;
			HDC dc, compatdc;
			HBITMAP compatbm;
			HBRUSH hBrush;
			int i, width;
			RECT rect;
			HFONT font;

			hWnd = GetDlgItem(hDlg, IDC_ABOUT_VER);
			dc = BeginPaint(hWnd, &paint);

			GetClientRect(hWnd, &rect);
			compatdc = CreateCompatibleDC(dc);
			compatbm = CreateCompatibleBitmap(dc, rect.right, rect.bottom);
			SelectObject(compatdc, compatbm);

			// Fill the background with a grayish color
			hBrush = CreateSolidBrush(GetSysColor(COLOR_BTNFACE));
			FillRect(compatdc, &rect, hBrush);
			SetBkMode(compatdc, TRANSPARENT);

			// Compute the starting point
			rect.top = title_padding - GetScrollPos(hScroll, SB_CTL);
			rect.left = left_margin;

			// Draw the title
			DrawTitle(compatdc, rect);

			// Iterate through each string, start at 1 since the title has been handled.
			for (i = 1; i < helper.count; i++) {
				switch (i % 4) {
				case 1:
					rect.top = (i == 1) ? (rect.top + helper.height[0]) : rect.bottom;
					rect.bottom = rect.top + helper.height[(i / 4) + 1] + text_padding;
					rect.right = left_margin;
				case 3:
					width = helper.name_width;
					font = helper.name_font;
					break;
				case 0:
				case 2:
					width = helper.desc_width;
					font = helper.desc_font;
					break;
				}

				rect.left = rect.right;
				rect.right += width;
				DrawBoxText(compatdc, rect, font, i);
			}

			GetClientRect(hWnd, &rect);
			BitBlt(dc, 0, 0, rect.right, rect.bottom, compatdc, 0, 0, SRCCOPY);

			DeleteObject(hBrush);
			DeleteObject(compatbm);
			DeleteDC(compatdc);
			EndPaint(hWnd, &paint);
			return FALSE;
		}
	case WM_LBUTTONDOWN:
		{
			POINT mouse_click = {LOWORD(lParam), HIWORD(lParam)};
			RECT dialog, image;

			GetClientRect(hDlg, &dialog);				// The about window's size
			ClientToScreen(hDlg, (LPPOINT)&dialog);		// Offset the top and left to be relative to the screen
			mouse_click.x += dialog.left;
			mouse_click.y += dialog.top;

			// Disabled URL on Image Click - Gent (13-09.2021)
			
			// GetWindowRect(GetDlgItem(hDlg, IDB_PJ64LOGO), &image);

			// if (PtInRect(&image, mouse_click))
			// ShellExecute(NULL, "open", "http://www.project64-legacy.com", NULL, NULL, SW_SHOWMAXIMIZED);

			break;
		}
	case WM_MOUSEWHEEL:
		{
			if (GET_WHEEL_DELTA_WPARAM(wParam) > 0)
				SendMessage(hDlg, WM_VSCROLL, MAKELONG(SB_LINEUP, 0), 0L);
			else
				SendMessage(hDlg, WM_VSCROLL, MAKELONG(SB_LINEDOWN, 0), 0L);
		}
	case WM_COMMAND:
		switch (LOWORD(wParam)) {
		case IDOK:
		case IDCANCEL:
			CoUninitialize();
			EndDialog(hDlg, 0);
			FreeHelper();
			break;
		}
	default:
		return FALSE;
	}
	return TRUE;
}

void FillHelper() {	
	HRSRC hRes = FindResource(hInst, MAKEINTRESOURCE(IDR_ABOUT_TXT), "Text");
	LPCSTR string = (LPCSTR)LoadResource(NULL, hRes);
	char *string_copy, *part;
	int length;

	length = SizeofResource(NULL, hRes);

	string_copy = (char *)malloc(length + 1);
	strncpy(string_copy, string, length);
	string_copy[length] = '\0';

	part = strtok(string_copy, "\n\r");

	while (part != NULL) {
		if (helper.allocated == helper.count) {
			char **junk = (char **)realloc(helper.strings, sizeof(char *) * (helper.allocated + 8));

			if (junk == NULL) { break; }
			helper.strings = junk;
			helper.allocated += 8;
		}

		helper.strings[helper.count] = (char *)malloc(strlen(part) + 1);

		if (helper.strings[helper.count] != NULL) {
			strncpy(helper.strings[helper.count], part, strlen(part));
			helper.strings[helper.count][strlen(part)] = '\0';
			helper.count++;
		}

		part = strtok(NULL, "\n\r");
	}

	free(string_copy);
}

void ComputeDimensions(HWND hDlg) { 
	int i, index2;
	RECT rect;
	HFONT font;

	GetClientRect(GetDlgItem(hDlg, IDC_ABOUT_VER), &rect);
	i = (rect.right / 2) - 8;
	helper.name_width = i / 3;
	helper.desc_width = i - helper.name_width;

	helper.height = (int *)malloc(sizeof(int) * ((helper.count / 4) + 2));
	helper.string_height = (int *)malloc(sizeof(int) * helper.allocated);

	if (helper.height == NULL || helper.string_height == NULL) { return; }

	for (i = 0; i <= ((helper.count - 1) / 4) + 1; i++)
		helper.height[i] = 0;

	helper.height[0] = ComputeHeight(helper.title_font, rect, helper.strings[0]) + title_padding;
	helper.string_height[0] = helper.height[0];

	for (i = 1; i < helper.count; i++) {
		if (i % 2 == 0) {
			rect.right = helper.desc_width;
			font = helper.desc_font;
		} else {
			rect.right = helper.name_width;
			font = helper.name_font;
		}
		
		helper.string_height[i] = ComputeHeight(font, rect, helper.strings[i]);
		index2 = ((i - 1) / 4) + 1;

		if (helper.height[index2] < helper.string_height[i])
			helper.height[index2] = helper.string_height[i];
	}
}

void SetScrollBarSize(HWND hDlg) {
	int total_height, i;
	SCROLLINFO si;
	RECT rect;

	if (hScroll == NULL)
		hScroll = GetDlgItem(hDlg, IDC_ABOUT_SCROLL);

	GetClientRect(GetDlgItem(hDlg, IDC_ABOUT_VER), &rect);

	total_height = 0;
	for (i = 0; i <= (helper.count / 4) + 1; i++)
		total_height += helper.height[i] + text_padding;

	ZeroMemory(&si, sizeof(si));
	si.cbSize = sizeof(si);
	si.fMask  = SIF_RANGE | SIF_PAGE | SIF_POS;
	si.nMin   = 0;
	si.nMax   = total_height;
	si.nPage  = total_height / 2;
	si.nPos   = 0;

	SetScrollInfo(hScroll, SB_CTL, &si, TRUE);
}

void FreeHelper() {
	int i;

	for (i = 0; i < helper.count; i++)
		free(helper.strings[i]);

	free(helper.strings);
	free(helper.height);
	free(helper.string_height);
	helper.strings = NULL;	
	helper.height = NULL;	
	helper.string_height = NULL;
	DeleteObject(helper.desc_font);
	DeleteObject(helper.name_font);
	DeleteObject(helper.title_font);
	DeleteObject(helper.title_font_i);
	helper.allocated = 0;
	helper.count = 0;
}

void AboutCenterImage(HWND hDlg) {
	RECT rect, rect2;
	int x, y;

	// rect will hold the size of the entire dialog
	// rect2 holds the size of the pj64 logo
	GetClientRect(hDlg, &rect);
	GetWindowRect(GetDlgItem(hDlg, IDB_PJ64LOGO), &rect2);

	// Center the image horizontally
	x = (rect.right - rect2.right + rect2.left) / 2;

	// Keep the old vertical position, no reason to change it	
	ScreenToClient(hDlg, (LPPOINT)&rect2);
	y = rect2.top;

	// Center the image and it's container horizontally, keeping it's vertical position
	SetWindowPos(GetDlgItem(hDlg, IDB_PJ64LOGO), HWND_TOP, x, y, 0, 0, SWP_NOSIZE | SWP_NOREPOSITION);
}

int ComputeHeight(HFONT font, RECT rect, char* message) {
	HDC dc = GetDC(NULL);
	int height = 0;

	SelectObject(dc, font);
	AdjustRectangle(&rect);
	height = DrawText(dc, message, strlen(message), &rect, DT_WORDBREAK | DT_CALCRECT);
	ReleaseDC(NULL, dc);
	return height;
}

void DrawTitle(HDC dc, RECT rect) {
	int mid_point, full_length;
	char *find;
	
	if (dc == NULL) { return; }

	find = strchr(helper.strings[0], ':');
	full_length = strlen(helper.strings[0]);

	if (find == NULL) {
		mid_point = full_length;
		SelectObject(dc, helper.title_font);
	} else {
		SelectObject(dc, helper.title_font_i);
		mid_point = (int)(find - helper.strings[0]) + 1;
	}

	DrawText(dc, helper.strings[0], mid_point, &rect, DT_NOCLIP);

	if (mid_point < full_length) {
		DrawText(dc, helper.strings[0], mid_point, &rect, DT_NOCLIP | DT_CALCRECT);
		rect.left = rect.right;
		SelectObject(dc, helper.title_font);
		DrawText(dc, helper.strings[0] + mid_point, -1, &rect, DT_NOCLIP);
	}
}

void DrawBoxText(HDC dc, RECT rect, HFONT font, int location) {
	DrawBox(dc, rect);

	AdjustRectangle(&rect);
	rect.top += (rect.bottom - rect.top - helper.string_height[location]) / 2;
	SelectObject(dc, font);
	DrawText(dc, helper.strings[location], -1, &rect, DT_WORDBREAK);
}

void DrawBox(HDC dc, RECT rect) {
	HBRUSH brush;

	if (dc == NULL) { return; }

	brush = CreateSolidBrush(RGB(0,0,0));
	rect.bottom -= 1;
	rect.right -= 1;

	FrameRect(dc, &rect, brush);
	DeleteObject(brush);
}

void AdjustRectangle(RECT *rect) {
	rect->left += 5;
	rect->right -= 5;
}
