/*
 * MasterMind - Can you master your mind?
 * (c) Tormod Fjeldskår
 */

#include <windows.h>
#include <stdlib.h> // rand(), srand()
#include <time.h>   // time()
#include "resource.h"

#define BTN_WIDTH       20
#define BTN_HEIGHT      20
#define ID_STATIC_TIMER 201
#define ID_TIMER        202
#define ID_ARROW        203

HINSTANCE hInst;    // global copy of hInstance
TCHAR     szAppName[] = "mastermind";

LRESULT CALLBACK WndProc     (HWND, UINT, WPARAM, LPARAM); // Window procedure for main window
LRESULT CALLBACK ArrowProc   (HWND, UINT, WPARAM, LPARAM); // Window procedure for arrow child window
BOOL    CALLBACK AboutDlgProc(HWND, UINT, WPARAM, LPARAM); // Window procedure for About dialog
BOOL    CALLBACK HelpDlgProc (HWND, UINT, WPARAM, LPARAM); // Window procedure for Help dialog

WNDPROC OldArrow; // Copy of original window procedure for arrow child window to allow subclassing

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR szCmdLine, int nCmdShow)
{
	WNDCLASS wc;
	HWND     hwnd;
	MSG      msg;
	HACCEL   hAccel;

	hInst = hInstance; // making global copy of hInstance

	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hIcon = LoadIcon(hInstance, szAppName);
	wc.hInstance = hInstance;
	wc.lpfnWndProc = WndProc;
	wc.lpszClassName = szAppName;
	wc.lpszMenuName = szAppName;
	wc.style = CS_HREDRAW | CS_VREDRAW;

	if(!RegisterClass(&wc))
	{
		MessageBox(NULL, TEXT("This program requires Windows NT!"), TEXT("Mastermind"), MB_ICONERROR);
		return 0;
	}

	hwnd = CreateWindow(szAppName, TEXT("Mastermind"),
						WS_POPUPWINDOW | WS_CAPTION,
						(GetSystemMetrics(SM_CXSCREEN) - 250) / 2, 
						(GetSystemMetrics(SM_CYSCREEN) - 400) / 2,
						250, 400,
						NULL, NULL, hInstance, NULL);

	ShowWindow(hwnd, nCmdShow);
	UpdateWindow(hwnd);

	hAccel = LoadAccelerators(hInstance, szAppName); // load accelerator table

	while(GetMessage(&msg, NULL, 0, 0))
	{
		if(!TranslateAccelerator(hwnd, hAccel, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
	return (int)msg.wParam;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	// 6 ownerdraw buttons
	static HWND      hwndColor[6];
	// static child for timer
	static HWND      hwndTimer;
	// static child for pointing arrow
	static HWND      hwndArrow;
	// colors for ownerdraw buttons
	static COLORREF  crColors[6] = {RGB(255,0  ,0),RGB(0,255,  0),RGB(  0,0,255),
								    RGB(255,255,0),RGB(0,255,255),RGB(255,0,255)};
	// colors for the 4 colors to guess
	static COLORREF  crAnswer[4] = {RGB(0,0,0),RGB(0,0,0),
								    RGB(0,0,0),RGB(0,0,0)};
	// 2-dimensional array for the colors guessed by the user
	static COLORREF  crUser[10][4] = {{0, 0, 0, 0},
									  {0, 0, 0, 0},
									  {0, 0, 0, 0},
									  {0, 0, 0, 0},
									  {0, 0, 0, 0},
									  {0, 0, 0, 0},
									  {0, 0, 0, 0},
									  {0, 0, 0, 0},
									  {0, 0, 0, 0},
									  {0, 0, 0, 0}};
	// currently elapsed time
	static TCHAR     szStatic[] = "00:00";
	// current position
	static int       x = 0, y = 0;
	// time of game start
	static int       nTime = (int)time(0);
	// amount of white and black pins on the board
	static int       nBlackPins[10] = {0,0,0,0,0,0,0,0,0,0}, 
					 nWhitePins[10] = {0,0,0,0,0,0,0,0,0,0};
	// flag to determine whether to ask user for confirmation when exiting og restarting
	static BOOL      fAsk = FALSE;
	// flag to determine whether the clock should be ticking or not
	static BOOL      fHalt = FALSE;
	// flag to determine whether the player has finished his game
	static BOOL      fClear = FALSE;
	
	LPDRAWITEMSTRUCT pdis;
	HBRUSH           hBrush;
	PAINTSTRUCT      ps;
	HDC              hdc;
	int              nMin, nSec;
	TCHAR            szBuffer[50];
	BOOL             fUser[4]   = {FALSE, FALSE, FALSE, FALSE}; // flag to determine which user-colors are previously checked
	BOOL             fAnswer[4] = {FALSE, FALSE, FALSE, FALSE}; // flag to determine which answer-colors are previously checked
	int              nTemp;

	switch(message)
	{
	case WM_CREATE:
		// create the 6 ownerdraw buttons
		for(int i = 0; i < 6; i++)
			hwndColor[i] = CreateWindow(TEXT("button"), TEXT(""),
						   WS_CHILD | WS_VISIBLE | BS_OWNERDRAW,
						   0, 0, BTN_WIDTH, BTN_HEIGHT,
						   hwnd, (HMENU)i, hInst, NULL);
		// create the timer static
		hwndTimer = CreateWindow(TEXT("static"), TEXT("00:00"),
								 WS_CHILD | WS_VISIBLE | SS_RIGHT,
								 175, 15, 60, 20,
								 hwnd, (HMENU)ID_STATIC_TIMER, hInst, NULL);
		// create the arrow static
		hwndArrow = CreateWindow(TEXT("static"), NULL,
								 WS_CHILD | WS_VISIBLE,
								 0, 0, 15, 15,
								 hwnd, (HMENU)ID_ARROW, hInst, NULL);
		// let's get ready to subclass
		OldArrow = (WNDPROC) SetWindowLong(hwndArrow, GWL_WNDPROC, (LPARAM) ArrowProc);
		// initialize randomizer
		srand((int)time(0));
		// select 4 random colors to guess
		for(int i = 0; i < 4; i++)
			crAnswer[i] = crColors[rand()%6];
		// start timer
		SetTimer(hwnd, ID_TIMER, 1000, NULL);
		return 0;
	case WM_TIMER:
		// update timer-static
		if(!fHalt)
		{
			nSec = (int)time(0) - nTime;
			nMin = nSec / 60;
			nSec = nSec % 60;
			wsprintf(szStatic, TEXT("%i%i:%i%i"), nMin / 10, nMin % 10, nSec / 10, nSec % 10);
			SetWindowText(hwndTimer, szStatic);
		}
		return 0;
	case WM_SIZE:
		// position the 6 ownerdraw buttons
		for(int i = 0; i < 6; i++)
			MoveWindow(hwndColor[i], 10, 10 + i * (BTN_HEIGHT + 10), BTN_WIDTH, BTN_HEIGHT, TRUE);
		return 0;
	case WM_COMMAND:
		switch(LOWORD(wParam))
		{
		case 0: // red     button
		case 1: // green   button
		case 2: // blue    button
		case 3: // yellow  button
		case 4: // cyan    button
		case 5: // magenta button
			if(y < 10)
			{
				crUser[y][x] = crColors[LOWORD(wParam)];
				x++;
				if(x > 3)
				{
					// let's check if the player guessed correctly
					for(int i = 0; i < 4; i++)
					{
						if(crAnswer[i] == crUser[y][i]) // if black pin
						{
							nBlackPins[y]++;
							fUser[i] = fAnswer[i] = TRUE; // checked
						}
					}
					for(int i = 0; i < 4; i++)
					{
						for(int j = 0; j < 4; j++)
						{
							if(!fAnswer[i] && !fUser[j]) // if not checked
							{
								if(crAnswer[i] == crUser[y][j]) // if white pin
								{
									nWhitePins[y]++;
									fAnswer[i] = fUser[j] = TRUE; // checked
								}
							}
						}
					}
					InvalidateRect(hwnd, NULL, TRUE); // repaint window
					if(nBlackPins[y] == 4) // victory!!
					{
						fAsk = FALSE;
						fHalt = TRUE;
						fClear = TRUE;
						wsprintf(szBuffer, "Elapsed time: %s\nClick OK to restart", szStatic);
						MessageBox(hwnd, szBuffer, TEXT("Congratulations!"), MB_OK | MB_ICONINFORMATION);
						SendMessage(hwnd, WM_COMMAND, IDM_FILE_NEW, 0);
					}
					else
					{
						y++; // next row
					}
				}
				x = x % 4;
				InvalidateRect(hwnd, NULL, TRUE); // repaint window
				fAsk = TRUE;
				if(y == 10 && !fClear)
				{
					fAsk = FALSE;
					fHalt = TRUE;
					fClear = TRUE;
					InvalidateRect(hwnd, NULL, TRUE);
					MessageBox(hwnd, TEXT("Bad luck! Try again!"), TEXT("Mastermind"), MB_OK | MB_ICONINFORMATION);
				}
			}
			break;
		case IDM_FILE_NEW:
			// start new game if user approves
			if(fAsk) nTemp = MessageBox(hwnd, TEXT("Abort current game?"), TEXT("Mastermind"), MB_YESNO | MB_ICONQUESTION);
			if(!fAsk || nTemp == IDYES)
			{
				for(int i = 0; i < 4; i++)
					crAnswer[i] = crColors[rand()%6];
				nTime = (int)time(0);              // reset  timer
				SendMessage(hwnd, WM_TIMER, 0, 0); // redraw timer
				for(int i = 0; i < 10; i++)
				{
					nBlackPins[i] = nWhitePins[i] = 0;
					for(int j = 0; j < 4; j++)
						crUser[i][j] = 0;          // reset user input
				}
				x = 0; y = 0;                      // reset coordinates
				InvalidateRect(hwnd, NULL, TRUE);
				fAsk   = FALSE;
				fHalt  = FALSE;
				fClear = FALSE;
			}
			break;
		case IDM_APP_EXIT:
			// close game if user approves
			SendMessage(hwnd, WM_CLOSE, 0, 0);
			break;
		case IDM_APP_ABOUTMASTERMIND:
			// show about dialog
			DialogBox(hInst, TEXT("about"), hwnd, AboutDlgProc);
			break;
		case IDM_HELP_INDEX:
			// show help dialog
			DialogBox(hInst, TEXT("help"), hwnd, HelpDlgProc);
			break;
		}
		return 0;
	case WM_DRAWITEM:
		// draw the face of the 6 ownerdraw buttons
		pdis = (LPDRAWITEMSTRUCT) lParam;
		FillRect(pdis->hDC, &pdis->rcItem, (HBRUSH)(COLOR_BTNFACE + 1));

		hBrush = CreateSolidBrush(crColors[pdis->CtlID]);
		hBrush = (HBRUSH)SelectObject(pdis->hDC, hBrush);

		Ellipse(pdis->hDC, pdis->rcItem.left, pdis->rcItem.top, pdis->rcItem.right, pdis->rcItem.bottom);
		
		DeleteObject(SelectObject(pdis->hDC, hBrush));

		return 0;
	case WM_PAINT:
		// redraw the client area
		hdc = BeginPaint(hwnd, &ps);

		// draw the four answer buttons
		for(int i = 0; i < 4; i++)
		{
			if(fClear)
				hBrush = CreateSolidBrush(crAnswer[i]);
			else
				hBrush = (HBRUSH)(COLOR_GRAYTEXT + 1);
			hBrush = (HBRUSH)SelectObject(hdc, hBrush);
			Ellipse(hdc, 60 + i * (BTN_WIDTH + 10), 10, 60 + i * (BTN_WIDTH + 10) + BTN_WIDTH, 10 + BTN_HEIGHT);
			DeleteObject(SelectObject(hdc, hBrush));
		}
		// draw the game board
		for(int i = 0, c = 0; i < 10; i++)
		{
			// draw the user's choices
			for(int j = 0; j < 4; j++, c = 0)
			{
				if(crUser[i][j])
				{
					hBrush = CreateSolidBrush(crUser[i][j]);
					hBrush = (HBRUSH)SelectObject(hdc, hBrush);
					Ellipse(hdc,
						60 + j * (BTN_WIDTH + 10), 
						40 + i * (BTN_HEIGHT + 10),
						60 + j * (BTN_WIDTH + 10) + BTN_WIDTH,
						40 + i * (BTN_HEIGHT + 10) + BTN_HEIGHT);
					DeleteObject(SelectObject(hdc, hBrush));
				}
			}
			if(nBlackPins[i]) // draw any black pins
			{
				for(int j = 0; j < nBlackPins[i]; j++)
				{
					hBrush = (HBRUSH)GetStockObject(BLACK_BRUSH);
					hBrush = (HBRUSH)SelectObject(hdc, hBrush);
					Rectangle(hdc, 
						200 + c * 10,
						40  + i * (BTN_HEIGHT + 10),
						200 + c * 10 + 5,
						40  + i * (BTN_HEIGHT + 10) + BTN_HEIGHT);
					DeleteObject(SelectObject(hdc, hBrush));
					c++;
				}
			}
			if(nWhitePins[i]) // draw any white pins
			{
				for(int j = 0; j < nWhitePins[i]; j++)
				{
					hBrush = (HBRUSH)GetStockObject(WHITE_BRUSH);
					hBrush = (HBRUSH)SelectObject(hdc, hBrush);
					Rectangle(hdc, 
						200 + c * 10,
						40  + i * (BTN_HEIGHT + 10),
						200 + c * 10 + 5,
						40  + i * (BTN_HEIGHT + 10) + BTN_HEIGHT);
					DeleteObject(SelectObject(hdc, hBrush));
					c++;
				}
			}
		}
		// draw two lines to mark game board areas
		MoveToEx(hdc, 40, 10, NULL);
		LineTo(hdc, 40, 350);
		MoveToEx(hdc, 40, 35, NULL);
		LineTo(hdc, 250, 35);

		if(y < 10) // move the arrow if not full board
            MoveWindow(hwndArrow, 42, 42 + y * (BTN_HEIGHT + 10), 15, 15, FALSE);

		EndPaint(hwnd, &ps);
		return 0;
	case WM_CLOSE:
		if(IDYES == MessageBox(hwnd, TEXT("Really quit?"), TEXT("Mastermind"), MB_YESNO | MB_ICONQUESTION))
		{
			DestroyWindow(hwnd);
		}
		return 0;
	case WM_QUERYENDSESSION:
		if(IDYES == MessageBox(hwnd, TEXT("Really quit?"), TEXT("Mastermind"), MB_YESNO | MB_ICONQUESTION))
			return 1;
		else
			return 0;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hwnd, message, wParam, lParam);
}

LRESULT CALLBACK ArrowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	// arrow coordinates
	static POINT        ptArrow[7] = {0,4, 7,4, 7,0, 14,7, 
								   7,14, 7,10, 0,10};
	       HDC          hdc;
	       PAINTSTRUCT  ps;
	
	// paints arrow in static child window
	if(message == WM_PAINT)
	{
		hdc = BeginPaint(hwnd, &ps);
		Polygon(hdc, ptArrow, 7);
		EndPaint(hwnd, &ps);
	}
	return CallWindowProc(OldArrow, hwnd, message, wParam, lParam); // return to old wndproc
}
BOOL CALLBACK AboutDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch(message)
	{
	case WM_INITDIALOG:
		return TRUE;
	case WM_COMMAND:
		switch(LOWORD(wParam))
		{
		case IDOK:
		case IDCANCEL:
			EndDialog(hDlg, FALSE);
			return TRUE;
		}
		break;
	}
	return FALSE;
}
BOOL CALLBACK HelpDlgProc(HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch(message)
	{
	case WM_INITDIALOG:
		return TRUE;
	case WM_COMMAND:
		switch(LOWORD(wParam))
		{
		case IDOK:
		case IDCANCEL:
			EndDialog(hDlg, FALSE);
			return TRUE;
		}
		break;
	}
	return FALSE;
}