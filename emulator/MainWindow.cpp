/*  This file is part of MS0515BTL.
    MS0515BTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    MS0515BTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
MS0515BTL. If not, see <http://www.gnu.org/licenses/>. */

// MainWindow.cpp
//

#include "stdafx.h"
#include <commdlg.h>
#include <crtdbg.h>
#include <mmintrin.h>
#include <vfw.h>
#include <commctrl.h>

#include "Main.h"
#include "Emulator.h"
#include "Dialogs.h"
#include "Views.h"
#include "ToolWindow.h"


//////////////////////////////////////////////////////////////////////


TCHAR g_szTitle[MAX_LOADSTRING];            // The title bar text
TCHAR g_szWindowClass[MAX_LOADSTRING];      // Main window class name

HWND m_hwndToolbar = NULL;
HWND m_hwndStatusbar = NULL;
HWND m_hwndSplitter = (HWND)INVALID_HANDLE_VALUE;

int m_MainWindowMinCx = MS0515_SCREEN_WIDTH + 16;
int m_MainWindowMinCy = MS0515_SCREEN_HEIGHT + 40;

BOOL m_MainWindow_Fullscreen = FALSE;
LONG m_MainWindow_FullscreenOldStyle = 0;
BOOL m_MainWindow_FullscreenOldMaximized = FALSE;
RECT m_MainWindow_FullscreenOldRect;


//////////////////////////////////////////////////////////////////////
// Forward declarations

BOOL MainWindow_InitToolbar();
BOOL MainWindow_InitStatusbar();
void MainWindow_RestorePositionAndShow();
LRESULT CALLBACK MainWindow_WndProc(HWND, UINT, WPARAM, LPARAM);
void MainWindow_AdjustWindowLayout();
bool MainWindow_DoCommand(int commandId);
void MainWindow_DoViewDebug();
void MainWindow_DoViewToolbar();
void MainWindow_DoViewKeyboard();
void MainWindow_DoViewFullscreen();
void MainWindow_DoViewScreenMode(int newMode);
void MainWindow_DoEmulatorRun();
void MainWindow_DoEmulatorAutostart();
void MainWindow_DoEmulatorReset();
void MainWindow_DoEmulatorSpeed(WORD speed);
void MainWindow_DoEmulatorSound();
void MainWindow_DoEmulatorSerial();
void MainWindow_DoEmulatorParallel();
void MainWindow_DoFileSaveState();
void MainWindow_DoFileLoadState();
void MainWindow_DoEmulatorFloppy(int slot);
void MainWindow_DoEmulatorConf(int configuration);
void MainWindow_DoFileScreenshot();
void MainWindow_DoFileScreenshotSaveAs();
void MainWindow_DoFileCreateDisk();
void MainWindow_DoFileSettings();
void MainWindow_OnToolbarGetInfoTip(LPNMTBGETINFOTIP);


//////////////////////////////////////////////////////////////////////


void MainWindow_RegisterClass()
{
    WNDCLASSEX wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = MainWindow_WndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = g_hInst;
    wcex.hIcon          = LoadIcon(g_hInst, MAKEINTRESOURCE(IDI_MS0515BTL));
    wcex.hCursor        = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground  = (HBRUSH)(COLOR_BTNFACE + 1);
    wcex.lpszMenuName   = MAKEINTRESOURCE(IDC_MS0515BTL);
    wcex.lpszClassName  = g_szWindowClass;
    wcex.hIconSm        = LoadIcon(wcex.hInstance, MAKEINTRESOURCE(IDI_SMALL));

    RegisterClassEx(&wcex);

    ToolWindow_RegisterClass();
    OverlappedWindow_RegisterClass();
    SplitterWindow_RegisterClass();

    // Register view classes
    ScreenView_RegisterClass();
    KeyboardView_RegisterClass();
    MemoryView_RegisterClass();
    DebugView_RegisterClass();
    //MemoryMapView_RegisterClass();
    DisasmView_RegisterClass();
    ConsoleView_RegisterClass();
}

BOOL CreateMainWindow()
{
    // Create the window
    g_hwnd = CreateWindow(
            g_szWindowClass, g_szTitle,
            WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
            0, 0, 0, 0,
            NULL, NULL, g_hInst, NULL);
    if (!g_hwnd)
        return FALSE;

    // Create and set up the toolbar and the statusbar
    if (!MainWindow_InitToolbar())
        return FALSE;
    if (!MainWindow_InitStatusbar())
        return FALSE;

    DebugView_Init();
    ScreenView_Init();
    KeyboardView_Init();

    // Create screen window as a child of the main window
    ScreenView_Create(g_hwnd, 0, 0);

    MainWindow_RestoreSettings();

    MainWindow_ShowHideToolbar();
    MainWindow_ShowHideKeyboard();
    MainWindow_ShowHideDebug();

    MainWindow_RestorePositionAndShow();

    UpdateWindow(g_hwnd);
    MainWindow_UpdateAllViews();
    MainWindow_UpdateMenu();

    // Autostart
    if (Settings_GetAutostart())
        ::PostMessage(g_hwnd, WM_COMMAND, ID_EMULATOR_RUN, 0);

    return TRUE;
}

BOOL MainWindow_InitToolbar()
{
    m_hwndToolbar = CreateWindowEx(0, TOOLBARCLASSNAME, NULL,
            WS_CHILD | TBSTYLE_FLAT | TBSTYLE_TRANSPARENT | TBSTYLE_LIST | TBSTYLE_TOOLTIPS | CCS_NOPARENTALIGN | CCS_NODIVIDER,
            4, 4, 0, 0, g_hwnd,
            (HMENU) 102,
            g_hInst, NULL);
    if (! m_hwndToolbar)
        return FALSE;

    SendMessage(m_hwndToolbar, TB_SETEXTENDEDSTYLE, 0, (LPARAM) (DWORD) TBSTYLE_EX_MIXEDBUTTONS);
    SendMessage(m_hwndToolbar, TB_BUTTONSTRUCTSIZE, (WPARAM) sizeof(TBBUTTON), 0);
    SendMessage(m_hwndToolbar, TB_SETBUTTONSIZE, 0, (LPARAM) MAKELONG (26, 26));

    TBADDBITMAP addbitmap;
    addbitmap.hInst = g_hInst;
    addbitmap.nID = IDB_TOOLBAR;
    SendMessage(m_hwndToolbar, TB_ADDBITMAP, 2, (LPARAM) &addbitmap);

    TBBUTTON buttons[10];
    ZeroMemory(buttons, sizeof(buttons));
    for (int i = 0; i < sizeof(buttons) / sizeof(TBBUTTON); i++)
    {
        buttons[i].fsState = TBSTATE_ENABLED;
        buttons[i].iString = -1;
    }
    buttons[0].idCommand = ID_EMULATOR_RUN;
    buttons[0].iBitmap = ToolbarImageRun;
    buttons[0].fsStyle = BTNS_BUTTON | BTNS_SHOWTEXT;
    buttons[0].iString = (int)SendMessage(m_hwndToolbar, TB_ADDSTRING, (WPARAM)0, (LPARAM)_T("Run"));
    buttons[1].idCommand = ID_EMULATOR_RESET;
    buttons[1].iBitmap = ToolbarImageReset;
    buttons[1].fsStyle = BTNS_BUTTON | BTNS_SHOWTEXT;
    buttons[1].iString = (int)SendMessage(m_hwndToolbar, TB_ADDSTRING, (WPARAM)0, (LPARAM)_T("Reset"));
    buttons[2].fsStyle = BTNS_SEP;
    buttons[3].idCommand = ID_EMULATOR_FLOPPY0;
    buttons[3].iBitmap = ToolbarImageFloppySlot;
    buttons[3].fsStyle = BTNS_BUTTON | BTNS_SHOWTEXT;
    buttons[3].iString = (int)SendMessage(m_hwndToolbar, TB_ADDSTRING, (WPARAM)0, (LPARAM)_T("0"));
    buttons[4].idCommand = ID_EMULATOR_FLOPPY1;
    buttons[4].iBitmap = ToolbarImageFloppySlot;
    buttons[4].fsStyle = BTNS_BUTTON | BTNS_SHOWTEXT;
    buttons[4].iString = (int)SendMessage(m_hwndToolbar, TB_ADDSTRING, (WPARAM)0, (LPARAM)_T("1"));
    buttons[5].idCommand = ID_EMULATOR_FLOPPY2;
    buttons[5].iBitmap = ToolbarImageFloppySlot;
    buttons[5].fsStyle = BTNS_BUTTON | BTNS_SHOWTEXT;
    buttons[5].iString = (int)SendMessage(m_hwndToolbar, TB_ADDSTRING, (WPARAM)0, (LPARAM)_T("2"));
    buttons[6].idCommand = ID_EMULATOR_FLOPPY3;
    buttons[6].iBitmap = ToolbarImageFloppySlot;
    buttons[6].fsStyle = BTNS_BUTTON | BTNS_SHOWTEXT;
    buttons[6].iString = (int)SendMessage(m_hwndToolbar, TB_ADDSTRING, (WPARAM)0, (LPARAM)_T("3"));
    buttons[7].fsStyle = BTNS_SEP;
    buttons[8].idCommand = ID_EMULATOR_SOUND;
    buttons[8].iBitmap = 8;
    buttons[8].fsStyle = BTNS_BUTTON | BTNS_SHOWTEXT;
    buttons[8].iString = (int)SendMessage(m_hwndToolbar, TB_ADDSTRING, (WPARAM)0, (LPARAM)_T("Sound"));
    buttons[9].idCommand = ID_FILE_SCREENSHOT;
    buttons[9].iBitmap = ToolbarImageScreenshot;
    buttons[9].fsStyle = BTNS_BUTTON;
    SendMessage(m_hwndToolbar, TB_ADDBUTTONS, (WPARAM) sizeof(buttons) / sizeof(TBBUTTON), (LPARAM) &buttons);

    if (Settings_GetToolbar())
        ShowWindow(m_hwndToolbar, SW_SHOW);

    return TRUE;
}

BOOL MainWindow_InitStatusbar()
{
    TCHAR buffer[100];
    wsprintf(buffer, _T("MS0515BTL - version %s"), _T(MS0515BTL_VERSION_STRING));
    m_hwndStatusbar = CreateStatusWindow(
            WS_CHILD | WS_VISIBLE | SBT_TOOLTIPS | CCS_NOPARENTALIGN | CCS_NODIVIDER,
            buffer,
            g_hwnd, 101);
    if (! m_hwndStatusbar)
        return FALSE;

    int statusbarParts[6];
    statusbarParts[0] = 300;
    statusbarParts[1] = statusbarParts[0] + 50;  // Sound
    statusbarParts[2] = statusbarParts[1] + 50;  // Motor
    statusbarParts[3] = statusbarParts[2] + 50;  // FPS
    statusbarParts[4] = statusbarParts[3] + 105; // Uptime
    statusbarParts[5] = -1;
    SendMessage(m_hwndStatusbar, SB_SETPARTS, sizeof(statusbarParts) / sizeof(int), (LPARAM) statusbarParts);

    return TRUE;
}

void MainWindow_RestoreSettings()
{
    TCHAR buf[MAX_PATH];

    // Reattach floppy images
    for (int slot = 0; slot < 4; slot++)
    {
        buf[0] = _T('\0');
        Settings_GetFloppyFilePath(slot, buf);
        if (buf[0] != _T('\0'))
        {
            if (! g_pBoard->AttachFloppyImage(slot, buf))
                Settings_SetFloppyFilePath(slot, NULL);
        }
    }

    // Restore ScreenViewMode
    int scrmode = Settings_GetScreenViewMode();
    ScreenView_SetScreenMode(scrmode);

    //// Restore Serial flag
    //if (Settings_GetSerial())
    //{
    //    TCHAR portname[10];
    //    Settings_GetSerialPort(portname);
    //    if (!Emulator_SetSerial(TRUE, portname))
    //        Settings_SetSerial(FALSE);
    //}

    //// Restore Parallel
    //if (Settings_GetParallel())
    //{
    //    Emulator_SetParallel(TRUE);
    //}
}

void MainWindow_SavePosition()
{
    if (m_MainWindow_Fullscreen)
    {
        Settings_SetWindowRect(&m_MainWindow_FullscreenOldRect);
        Settings_SetWindowMaximized(m_MainWindow_FullscreenOldMaximized);
    }
    else
    {
        WINDOWPLACEMENT placement;
        placement.length = sizeof(WINDOWPLACEMENT);
        ::GetWindowPlacement(g_hwnd, &placement);

        Settings_SetWindowRect(&(placement.rcNormalPosition));
        Settings_SetWindowMaximized(placement.showCmd == SW_SHOWMAXIMIZED);
    }
    Settings_SetWindowFullscreen(m_MainWindow_Fullscreen);
}
void MainWindow_RestorePositionAndShow()
{
    RECT rc;
    if (Settings_GetWindowRect(&rc))
    {
        HMONITOR hmonitor = MonitorFromRect(&rc, MONITOR_DEFAULTTONULL);
        if (hmonitor != NULL)
        {
            ::SetWindowPos(g_hwnd, NULL, rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top,
                    SWP_NOACTIVATE | SWP_NOZORDER);
        }
    }

    ShowWindow(g_hwnd, Settings_GetWindowMaximized() ? SW_SHOWMAXIMIZED : SW_SHOW);

    //if (Settings_GetWindowFullscreen())
    //    MainWindow_DoViewFullscreen();
}

void MainWindow_UpdateWindowTitle(LPCTSTR emustate)
{
    TCHAR buffer[100];
    wsprintf(buffer, _T("%s [%s]"), g_szTitle, emustate);
    SetWindowText(g_hwnd, buffer);
}

// Processes messages for the main window
LRESULT CALLBACK MainWindow_WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_ACTIVATE:
        SetFocus(g_hwndScreen);
        break;
    case WM_COMMAND:
        {
            int wmId    = LOWORD(wParam);
            //int wmEvent = HIWORD(wParam);
            bool okProcessed = MainWindow_DoCommand(wmId);
            if (!okProcessed)
                return DefWindowProc(hWnd, message, wParam, lParam);
        }
        break;
    case WM_DESTROY:
        MainWindow_SavePosition();
        PostQuitMessage(0);
        break;
    case WM_SIZE:
        MainWindow_AdjustWindowLayout();
        break;
    case WM_GETMINMAXINFO:
        {
            DefWindowProc(hWnd, message, wParam, lParam);
            MINMAXINFO* mminfo = (MINMAXINFO*)lParam;
            if (!m_MainWindow_Fullscreen)
            {
                mminfo->ptMinTrackSize.x = m_MainWindowMinCx;
                mminfo->ptMinTrackSize.y = m_MainWindowMinCy;
            }
        }
        break;
    case WM_NOTIFY:
        {
            //int idCtrl = (int) wParam;
            HWND hwndFrom = ((LPNMHDR) lParam)->hwndFrom;
            UINT code = ((LPNMHDR) lParam)->code;
            if (code == TTN_SHOW)
            {
                return 0;
            }
            else if (hwndFrom == m_hwndToolbar && code == TBN_GETINFOTIP)
            {
                MainWindow_OnToolbarGetInfoTip((LPNMTBGETINFOTIP) lParam);
            }
            else
                return DefWindowProc(hWnd, message, wParam, lParam);
        }
        break;
    case WM_DRAWITEM:
        {
            //int idCtrl = (int) wParam;
            HWND hwndItem = ((LPDRAWITEMSTRUCT) lParam)->hwndItem;
            if (hwndItem == m_hwndStatusbar)
                ; //MainWindow_OnStatusbarDrawItem((LPDRAWITEMSTRUCT) lParam);
            else
                return DefWindowProc(hWnd, message, wParam, lParam);
        }
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return 0;
}

void MainWindow_AdjustWindowSize()
{
    const int MAX_DEBUG_WIDTH = 1450;
    const int MAX_DEBUG_HEIGHT = 1400;
    const int MIN_NODEBUG_WIDTH = 660;

    // If Fullscreen or Maximized then do nothing
    //if (m_MainWindow_Fullscreen)
    //    return;
    WINDOWPLACEMENT placement;
    placement.length = sizeof(WINDOWPLACEMENT);
    ::GetWindowPlacement(g_hwnd, &placement);
    if (placement.showCmd == SW_MAXIMIZE)
        return;

    // Get metrics
    RECT rcWorkArea;  SystemParametersInfo(SPI_GETWORKAREA, 0, &rcWorkArea, 0);
    int cxFrame   = ::GetSystemMetrics(SM_CXDLGFRAME);
    int cyFrame   = ::GetSystemMetrics(SM_CYDLGFRAME);
    int cyCaption = ::GetSystemMetrics(SM_CYCAPTION);
    int cyMenu    = ::GetSystemMetrics(SM_CYMENU);

    RECT rcToolbar;  GetWindowRect(m_hwndToolbar, &rcToolbar);
    int cyToolbar = rcToolbar.bottom - rcToolbar.top;
    RECT rcScreen;  GetWindowRect(g_hwndScreen, &rcScreen);
    int cxScreen = rcScreen.right - rcScreen.left;
    int cyScreen = rcScreen.bottom - rcScreen.top;
    RECT rcStatus;  GetWindowRect(m_hwndStatusbar, &rcStatus);
    int cyStatus = rcStatus.bottom - rcStatus.top;
    int cyKeyboard = 0;

    if (Settings_GetKeyboard())
    {
        RECT rcKeyboard;  GetWindowRect(g_hwndKeyboard, &rcKeyboard);
        cyKeyboard = rcKeyboard.bottom - rcKeyboard.top;
    }

    // Adjust main window size
    int xLeft = rcWorkArea.left;
    int yTop = rcWorkArea.top;
    int cxWidth, cyHeight;
    if (Settings_GetDebug())
    {
        cxWidth = rcWorkArea.right - rcWorkArea.left;
        if (cxWidth > MAX_DEBUG_WIDTH) cxWidth = MAX_DEBUG_WIDTH;
        cyHeight = rcWorkArea.bottom - rcWorkArea.top;
        if (cyHeight > MAX_DEBUG_HEIGHT) cyHeight = MAX_DEBUG_HEIGHT;
    }
    else
    {
        cxWidth = cxScreen + cxFrame * 2 + 8;
        if (cxWidth < MIN_NODEBUG_WIDTH) cxWidth = MIN_NODEBUG_WIDTH;
        cyHeight = cyCaption + cyMenu + cyScreen + 4 + cyStatus + cyFrame * 2;
        if (Settings_GetToolbar())
            cyHeight += cyToolbar;
        if (Settings_GetKeyboard())
            cyHeight += cyKeyboard;
    }

    SetWindowPos(g_hwnd, NULL, xLeft, yTop, cxWidth, cyHeight, SWP_NOZORDER | SWP_NOMOVE);
}

void MainWindow_AdjustWindowLayout()
{
    RECT rcStatus;  GetWindowRect(m_hwndStatusbar, &rcStatus);
    int cyStatus = rcStatus.bottom - rcStatus.top;
    if (m_MainWindow_Fullscreen)
        cyStatus = 0;

    int yScreen = 0;
    int cxScreen = 0, cyScreen = 0;

    int cyToolbar = 0;
    if (Settings_GetToolbar())
    {
        RECT rcToolbar;  GetWindowRect(m_hwndToolbar, &rcToolbar);
        cyToolbar = rcToolbar.bottom - rcToolbar.top;
        yScreen += cyToolbar + 4;
    }

    RECT rc;  GetClientRect(g_hwnd, &rc);

    if (!Settings_GetDebug())  // No debug views
    {
        cxScreen = rc.right;

        int yTape = rc.bottom - cyStatus + 4;
        RECT rcScreen;  GetWindowRect(g_hwndScreen, &rcScreen);
        cyScreen = rcScreen.bottom - rcScreen.top;

        int yKeyboard = yTape;
        int cxKeyboard = 0, cyKeyboard = 0;
        if (Settings_GetKeyboard())  // Fills space between the screen and tape
        {
            cxKeyboard = cxScreen;
            yKeyboard = yScreen + cyScreen;
            cyKeyboard = yTape - yKeyboard - 4;
        }

        if (Settings_GetKeyboard())
        {
            int xKeyboard = (cxScreen - cxKeyboard) / 2;
            SetWindowPos(g_hwndKeyboard, NULL, xKeyboard, yKeyboard, cxKeyboard, cyKeyboard, SWP_NOZORDER | SWP_NOCOPYBITS);
        }
    }
    if (Settings_GetDebug())  // Debug views shown -- keyboard/tape snapped to top
    {
        cxScreen = 640;
        cyScreen = MS0515_SCREEN_HEIGHT + 8;

        int yKeyboard = yScreen + cyScreen;
        int yConsole = yKeyboard;

        if (Settings_GetKeyboard())
        {
            int cxKeyboard = cxScreen;
            int cyKeyboard = 192;
            int xKeyboard = (cxScreen - cxKeyboard) / 2;
            SetWindowPos(g_hwndKeyboard, NULL, xKeyboard, yKeyboard, cxKeyboard, cyKeyboard, SWP_NOZORDER | SWP_NOCOPYBITS);
            yConsole += cyKeyboard + 4;
        }

        int cyConsole = rc.bottom - cyStatus - yConsole - 4;
        SetWindowPos(g_hwndConsole, NULL, 0, yConsole, cxScreen, cyConsole, SWP_NOZORDER);

        RECT rcDebug;  GetWindowRect(g_hwndDebug, &rcDebug);
        int cxDebug = rc.right - cxScreen - 4;
        int cyDebug = rcDebug.bottom - rcDebug.top;
        SetWindowPos(g_hwndDebug, NULL, cxScreen + 4, 0, cxDebug, cyDebug, SWP_NOZORDER);

        RECT rcDisasm;  GetWindowRect(g_hwndDisasm, &rcDisasm);
        int yDebug = cyDebug + 4;
        int cyDisasm = rcDisasm.bottom - rcDisasm.top;
        SetWindowPos(g_hwndDisasm, NULL, cxScreen + 4, yDebug, cxDebug, cyDisasm, SWP_NOZORDER);

        int yMemory = cyDebug + 4 + cyDisasm + 4;
        int cyMemory = rc.bottom - yMemory;
        SetWindowPos(g_hwndMemory, NULL, cxScreen + 4, yMemory, cxDebug, cyMemory, SWP_NOZORDER);

        SetWindowPos(m_hwndSplitter, NULL, cxScreen + 4, yMemory - 4, cxDebug, 4, SWP_NOZORDER);
    }

    SetWindowPos(m_hwndToolbar, NULL, 4, 4, cxScreen, cyToolbar, SWP_NOZORDER);

    SetWindowPos(g_hwndScreen, NULL, 0, yScreen, cxScreen, cyScreen, SWP_NOZORDER /*| SWP_NOCOPYBITS*/);

    int cyStatusReal = rcStatus.bottom - rcStatus.top;
    SetWindowPos(m_hwndStatusbar, NULL, 0, rc.bottom - cyStatusReal, cxScreen, cyStatusReal,
            SWP_NOZORDER | (m_MainWindow_Fullscreen ? SWP_HIDEWINDOW : SWP_SHOWWINDOW));
}

void MainWindow_ShowHideDebug()
{
    if (!Settings_GetDebug())
    {
        // Delete debug views
        if (m_hwndSplitter != INVALID_HANDLE_VALUE)
            DestroyWindow(m_hwndSplitter);
        if (g_hwndConsole != INVALID_HANDLE_VALUE)
            DestroyWindow(g_hwndConsole);
        if (g_hwndDebug != INVALID_HANDLE_VALUE)
            DestroyWindow(g_hwndDebug);
        if (g_hwndDisasm != INVALID_HANDLE_VALUE)
            DestroyWindow(g_hwndDisasm);
        if (g_hwndMemory != INVALID_HANDLE_VALUE)
            DestroyWindow(g_hwndMemory);

        MainWindow_AdjustWindowSize();
    }
    else  // Debug Views ON
    {
        MainWindow_AdjustWindowSize();

        // Calculate children positions
        RECT rc;  GetClientRect(g_hwnd, &rc);
        RECT rcScreen;  GetWindowRect(g_hwndScreen, &rcScreen);
        RECT rcStatus;  GetWindowRect(m_hwndStatusbar, &rcStatus);
        int cyStatus = rcStatus.bottom - rcStatus.top;
        int yConsoleTop = rcScreen.bottom - rcScreen.top + 8;
        int cxConsoleWidth = rcScreen.right - rcScreen.left;
        int cyConsoleHeight = rc.bottom - cyStatus - yConsoleTop - 4;
        int xDebugLeft = (rcScreen.right - rcScreen.left) + 8;
        int cxDebugWidth = rc.right - xDebugLeft - 4;
        int cyDebugHeight = 188;
        int yDisasmTop = 4 + cyDebugHeight + 4;
        int cyDisasmHeight = 328;
        int yMemoryTop = cyDebugHeight + 4 + cyDisasmHeight + 8;
        int cyMemoryHeight = rc.bottom - cyStatus - yMemoryTop - 4;

        // Create debug views
        if (g_hwndConsole == INVALID_HANDLE_VALUE)
            ConsoleView_Create(g_hwnd, 4, yConsoleTop, cxConsoleWidth, cyConsoleHeight);
        if (g_hwndDebug == INVALID_HANDLE_VALUE)
            DebugView_Create(g_hwnd, xDebugLeft, 4, cxDebugWidth, cyDebugHeight);
        if (g_hwndDisasm == INVALID_HANDLE_VALUE)
            DisasmView_Create(g_hwnd, xDebugLeft, yDisasmTop, cxDebugWidth, cyDisasmHeight);
        if (g_hwndMemory == INVALID_HANDLE_VALUE)
            MemoryView_Create(g_hwnd, xDebugLeft, yMemoryTop, cxDebugWidth, cyMemoryHeight);
        m_hwndSplitter = SplitterWindow_Create(g_hwnd, g_hwndDisasm, g_hwndMemory);
    }

    MainWindow_AdjustWindowLayout();

    MainWindow_UpdateMenu();

    SetFocus(g_hwndScreen);
}

void MainWindow_ShowHideToolbar()
{
    ShowWindow(m_hwndToolbar, Settings_GetToolbar() ? SW_SHOW : SW_HIDE);

    MainWindow_AdjustWindowSize();
    MainWindow_AdjustWindowLayout();
    MainWindow_UpdateMenu();
}

void MainWindow_ShowHideKeyboard()
{
    if (!Settings_GetKeyboard())
    {
        if (g_hwndKeyboard != INVALID_HANDLE_VALUE)
        {
            ::DestroyWindow(g_hwndKeyboard);
            g_hwndKeyboard = (HWND) INVALID_HANDLE_VALUE;
        }
    }
    else
    {
        // Calculate children positions
        RECT rc;  GetClientRect(g_hwnd, &rc);
        RECT rcScreen;  GetWindowRect(g_hwndScreen, &rcScreen);
        int yKeyboardTop = rcScreen.bottom - rcScreen.top + 8;
        int cxKeyboardWidth = rcScreen.right - rcScreen.left;
        const int cyKeyboardHeight = 200;

        if (g_hwndKeyboard == INVALID_HANDLE_VALUE)
            KeyboardView_Create(g_hwnd, 4, yKeyboardTop, cxKeyboardWidth, cyKeyboardHeight);
    }

    MainWindow_AdjustWindowSize();
    MainWindow_AdjustWindowLayout();
    MainWindow_UpdateMenu();
}

void MainWindow_ShowHideMemoryMap()
{
    //if (g_hwndMemoryMap == INVALID_HANDLE_VALUE)
    //{
    //    RECT rcScreen;  ::GetWindowRect(g_hwndScreen, &rcScreen);
    //    CreateMemoryMapView(rcScreen.right, rcScreen.top);
    //}
    //else
    //{
    //    ::SetFocus(g_hwndMemoryMap);
    //}
}

void MainWindow_UpdateMenu()
{
    // Get main menu
    HMENU hMenu = GetMenu(g_hwnd);

    // Emulator|Run check
    CheckMenuItem(hMenu, ID_EMULATOR_RUN, (g_okEmulatorRunning ? MF_CHECKED : MF_UNCHECKED));
    SendMessage(m_hwndToolbar, TB_CHECKBUTTON, ID_EMULATOR_RUN, (g_okEmulatorRunning ? 1 : 0));
    //MainWindow_SetToolbarImage(ID_EMULATOR_RUN, g_okEmulatorRunning ? ToolbarImageRun : ToolbarImagePause);
    // View|Debug check
    CheckMenuItem(hMenu, ID_VIEW_TOOLBAR, (Settings_GetToolbar() ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(hMenu, ID_VIEW_DEBUG, (Settings_GetDebug() ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(hMenu, ID_VIEW_KEYBOARD, (Settings_GetKeyboard() ? MF_CHECKED : MF_UNCHECKED));
    //// View|Color Screen
    //MainWindow_SetToolbarImage(ID_VIEW_RGBSCREEN,
    //    (ScreenView_GetScreenMode() & 1) ? ToolbarImageColorScreen : ToolbarImageBWScreen);
    // View|Screen Mode
    UINT scrmodecmd = 0;
    switch (ScreenView_GetScreenMode())
    {
    case 0: scrmodecmd = ID_VIEW_SCREENMODE0; break;
    case 1: scrmodecmd = ID_VIEW_SCREENMODE1; break;
    case 2: scrmodecmd = ID_VIEW_SCREENMODE2; break;
    case 3: scrmodecmd = ID_VIEW_SCREENMODE3; break;
    case 4: scrmodecmd = ID_VIEW_SCREENMODE4; break;
    case 5: scrmodecmd = ID_VIEW_SCREENMODE5; break;
    }
    CheckMenuRadioItem(hMenu, ID_VIEW_SCREENMODE0, ID_VIEW_SCREENMODE5, scrmodecmd, MF_BYCOMMAND);

    // Emulator menu options
    CheckMenuItem(hMenu, ID_EMULATOR_AUTOSTART, (Settings_GetAutostart() ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(hMenu, ID_EMULATOR_SOUND, (Settings_GetSound() ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(hMenu, ID_EMULATOR_SERIAL, (Settings_GetSerial() ? MF_CHECKED : MF_UNCHECKED));
    SendMessage(m_hwndToolbar, TB_CHECKBUTTON, ID_EMULATOR_SERIAL, (Settings_GetSerial() ? 1 : 0));
    CheckMenuItem(hMenu, ID_EMULATOR_PARALLEL, (Settings_GetParallel() ? MF_CHECKED : MF_UNCHECKED));

    UINT speedcmd = 0;
    switch (Settings_GetRealSpeed())
    {
    case 0x7ffe: speedcmd = ID_EMULATOR_SPEED25;   break;
    case 0x7fff: speedcmd = ID_EMULATOR_SPEED50;   break;
    case 0:      speedcmd = ID_EMULATOR_SPEEDMAX;  break;
    case 1:      speedcmd = ID_EMULATOR_REALSPEED; break;
    case 2:      speedcmd = ID_EMULATOR_SPEED200;  break;
    }
    CheckMenuRadioItem(hMenu, ID_EMULATOR_SPEED25, ID_EMULATOR_SPEED200, speedcmd, MF_BYCOMMAND);

    MainWindow_SetToolbarImage(ID_EMULATOR_SOUND, (Settings_GetSound() ? ToolbarImageSoundOn : ToolbarImageSoundOff));
    EnableMenuItem(hMenu, ID_DEBUG_STEPINTO, (g_okEmulatorRunning ? MF_DISABLED : MF_ENABLED));

    //UINT configcmd = 0;
    //switch (g_nEmulatorConfiguration)
    //{
    //case EMU_CONF_NEMIGA303: configcmd = ID_CONF_NEMIGA303; break;
    //case EMU_CONF_NEMIGA405: configcmd = ID_CONF_NEMIGA405; break;
    //case EMU_CONF_NEMIGA406: configcmd = ID_CONF_NEMIGA406; break;
    //}
    //CheckMenuRadioItem(hMenu, ID_CONF_NEMIGA303, ID_CONF_NEMIGA406, configcmd, MF_BYCOMMAND);

    // Emulator|FloppyX
    CheckMenuItem(hMenu, ID_EMULATOR_FLOPPY0, (g_pBoard->IsFloppyImageAttached(0) ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(hMenu, ID_EMULATOR_FLOPPY1, (g_pBoard->IsFloppyImageAttached(1) ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(hMenu, ID_EMULATOR_FLOPPY2, (g_pBoard->IsFloppyImageAttached(2) ? MF_CHECKED : MF_UNCHECKED));
    CheckMenuItem(hMenu, ID_EMULATOR_FLOPPY3, (g_pBoard->IsFloppyImageAttached(3) ? MF_CHECKED : MF_UNCHECKED));
    MainWindow_SetToolbarImage(ID_EMULATOR_FLOPPY0,
            g_pBoard->IsFloppyImageAttached(0) ? (g_pBoard->IsFloppyReadOnly(0) ? ToolbarImageFloppyDiskWP : ToolbarImageFloppyDisk) : ToolbarImageFloppySlot);
    MainWindow_SetToolbarImage(ID_EMULATOR_FLOPPY1,
            g_pBoard->IsFloppyImageAttached(1) ? (g_pBoard->IsFloppyReadOnly(1) ? ToolbarImageFloppyDiskWP : ToolbarImageFloppyDisk) : ToolbarImageFloppySlot);
    MainWindow_SetToolbarImage(ID_EMULATOR_FLOPPY2,
            g_pBoard->IsFloppyImageAttached(2) ? (g_pBoard->IsFloppyReadOnly(2) ? ToolbarImageFloppyDiskWP : ToolbarImageFloppyDisk) : ToolbarImageFloppySlot);
    MainWindow_SetToolbarImage(ID_EMULATOR_FLOPPY3,
            g_pBoard->IsFloppyImageAttached(3) ? (g_pBoard->IsFloppyReadOnly(3) ? ToolbarImageFloppyDiskWP : ToolbarImageFloppyDisk) : ToolbarImageFloppySlot);
}

// Process menu command
// Returns: true - command was processed, false - command not found
bool MainWindow_DoCommand(int commandId)
{
    switch (commandId)
    {
    case IDM_ABOUT:
        ShowAboutBox();
        break;
    case IDM_EXIT:
        DestroyWindow(g_hwnd);
        break;
    case ID_VIEW_DEBUG:
        MainWindow_DoViewDebug();
        break;
    case ID_VIEW_TOOLBAR:
        MainWindow_DoViewToolbar();
        break;
    case ID_VIEW_KEYBOARD:
        MainWindow_DoViewKeyboard();
        break;
        //case ID_VIEW_FULLSCREEN:
        //    MainWindow_DoViewFullscreen();
        //    break;
    case ID_VIEW_SCREENMODE0:
        MainWindow_DoViewScreenMode(0);
        break;
    case ID_VIEW_SCREENMODE1:
        MainWindow_DoViewScreenMode(1);
        break;
    case ID_VIEW_SCREENMODE2:
        MainWindow_DoViewScreenMode(2);
        break;
    case ID_VIEW_SCREENMODE3:
        MainWindow_DoViewScreenMode(3);
        break;
    case ID_VIEW_SCREENMODE4:
        MainWindow_DoViewScreenMode(4);
        break;
    case ID_VIEW_SCREENMODE5:
        MainWindow_DoViewScreenMode(5);
        break;
    case ID_EMULATOR_RUN:
        MainWindow_DoEmulatorRun();
        break;
    case ID_EMULATOR_AUTOSTART:
        MainWindow_DoEmulatorAutostart();
        break;
    case ID_DEBUG_STEPINTO:
        if (!g_okEmulatorRunning && Settings_GetDebug())
            ConsoleView_StepInto();
        break;
    case ID_DEBUG_STEPOVER:
        if (!g_okEmulatorRunning && Settings_GetDebug())
            ConsoleView_StepOver();
        break;
    case ID_DEBUG_MEMORY_WORDBYTE:
        MemoryView_SwitchWordByte();
        break;
    case ID_DEBUG_MEMORY_GOTO:
        MemoryView_SelectAddress();
        break;
    case ID_EMULATOR_RESET:
        MainWindow_DoEmulatorReset();
        break;
    case ID_EMULATOR_SPEED25:
        MainWindow_DoEmulatorSpeed(0x7ffe);
        break;
    case ID_EMULATOR_SPEED50:
        MainWindow_DoEmulatorSpeed(0x7fff);
        break;
    case ID_EMULATOR_SPEEDMAX:
        MainWindow_DoEmulatorSpeed(0);
        break;
    case ID_EMULATOR_REALSPEED:
        MainWindow_DoEmulatorSpeed(1);
        break;
    case ID_EMULATOR_SPEED200:
        MainWindow_DoEmulatorSpeed(2);
        break;
    case ID_EMULATOR_SOUND:
        MainWindow_DoEmulatorSound();
        break;
    case ID_EMULATOR_SERIAL:
        MainWindow_DoEmulatorSerial();
        break;
    case ID_EMULATOR_PARALLEL:
        MainWindow_DoEmulatorParallel();
        break;
    case ID_EMULATOR_FLOPPY0:
        MainWindow_DoEmulatorFloppy(0);
        break;
    case ID_EMULATOR_FLOPPY1:
        MainWindow_DoEmulatorFloppy(1);
        break;
    case ID_EMULATOR_FLOPPY2:
        MainWindow_DoEmulatorFloppy(2);
        break;
    case ID_EMULATOR_FLOPPY3:
        MainWindow_DoEmulatorFloppy(3);
        break;
    case ID_FILE_LOADSTATE:
        MainWindow_DoFileLoadState();
        break;
    case ID_FILE_SAVESTATE:
        MainWindow_DoFileSaveState();
        break;
    case ID_FILE_SCREENSHOT:
        MainWindow_DoFileScreenshot();
        break;
    case ID_FILE_SAVESCREENSHOTAS:
        MainWindow_DoFileScreenshotSaveAs();
        break;
        //case ID_FILE_CREATEDISK:
        //    MainWindow_DoFileCreateDisk();
        //    break;
    case ID_FILE_SETTINGS:
        MainWindow_DoFileSettings();
        break;
    default:
        return false;
    }
    return true;
}

void MainWindow_DoViewDebug()
{
    MainWindow_DoViewScreenMode(0);  // Switch to 512x256 Short mode

    Settings_SetDebug(!Settings_GetDebug());
    MainWindow_ShowHideDebug();
}
void MainWindow_DoDebugMemoryMap()
{
    MainWindow_ShowHideMemoryMap();
}
void MainWindow_DoViewToolbar()
{
    Settings_SetToolbar(!Settings_GetToolbar());
    MainWindow_ShowHideToolbar();
}
void MainWindow_DoViewKeyboard()
{
    Settings_SetKeyboard(!Settings_GetKeyboard());
    MainWindow_ShowHideKeyboard();
}

void MainWindow_DoViewScreenMode(int newMode)
{
    if (Settings_GetDebug() && newMode != 0) return;  // Deny switching to other mode in Debug mode

    ScreenView_SetScreenMode(newMode);

    MainWindow_AdjustWindowSize();
    MainWindow_AdjustWindowLayout();
    MainWindow_UpdateMenu();

    Settings_SetScreenViewMode(newMode);
}


void MainWindow_DoViewFullscreen()
{
    if (Settings_GetDebug())
        MainWindow_DoViewDebug();  // Leave Debug mode

    if (!m_MainWindow_Fullscreen)
    {
        // Store current window state and position
        m_MainWindow_FullscreenOldMaximized = ::IsZoomed(g_hwnd);
        if (m_MainWindow_FullscreenOldMaximized)
            ::SendMessage(g_hwnd, WM_SYSCOMMAND, SC_RESTORE, 0);
        ::GetWindowRect(g_hwnd, &m_MainWindow_FullscreenOldRect);
    }

    m_MainWindow_Fullscreen = !m_MainWindow_Fullscreen;

    if (m_MainWindow_Fullscreen)
    {
        MONITORINFO monitorinfo;
        monitorinfo.cbSize = sizeof(monitorinfo);
        ::GetMonitorInfo(::MonitorFromWindow(g_hwnd, MONITOR_DEFAULTTONEAREST), &monitorinfo);
        RECT rcnew = monitorinfo.rcMonitor;

        m_MainWindow_FullscreenOldStyle = ::GetWindowLong(g_hwnd, GWL_STYLE);
        ::SetWindowLong(g_hwnd, GWL_STYLE, m_MainWindow_FullscreenOldStyle & ~(WS_CAPTION | WS_THICKFRAME));
        ::SetWindowPos(g_hwnd, NULL, rcnew.left, rcnew.top, rcnew.right - rcnew.left, rcnew.bottom - rcnew.top,
                SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
    }
    else
    {
        // Restore saved window position
        RECT rcnew = m_MainWindow_FullscreenOldRect;
        ::SetWindowLong(g_hwnd, GWL_STYLE, m_MainWindow_FullscreenOldStyle);
        ::SetWindowPos(g_hwnd, NULL, rcnew.left, rcnew.top, rcnew.right - rcnew.left, rcnew.bottom - rcnew.top,
                SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
        if (m_MainWindow_FullscreenOldMaximized)
            ::SendMessage(g_hwnd, WM_SYSCOMMAND, SC_MAXIMIZE, 0);
    }

    MainWindow_UpdateMenu();
}

void MainWindow_DoEmulatorRun()
{
    if (g_okEmulatorRunning)
    {
        Emulator_Stop();
    }
    else
    {
        Emulator_Start();
    }
}
void MainWindow_DoEmulatorAutostart()
{
    Settings_SetAutostart(!Settings_GetAutostart());

    MainWindow_UpdateMenu();
}
void MainWindow_DoEmulatorReset()
{
    Emulator_Reset();
}
void MainWindow_DoEmulatorSpeed(WORD speed)
{
    Settings_SetRealSpeed(speed);
    Emulator_SetSpeed(speed);

    MainWindow_UpdateMenu();
}
void MainWindow_DoEmulatorSound()
{
    Settings_SetSound(!Settings_GetSound());

    Emulator_SetSound(Settings_GetSound());

    MainWindow_UpdateMenu();
}

void MainWindow_DoEmulatorSerial()
{
    //BOOL okSerial = Settings_GetSerial();
    //if (!okSerial)
    //{
    //    TCHAR portname[10];
    //    Settings_GetSerialPort(portname);
    //    if (Emulator_SetSerial(TRUE, portname))
    //        Settings_SetSerial(TRUE);
    //}
    //else
    //{
    //    Emulator_SetSerial(FALSE, NULL);
    //    Settings_SetSerial(FALSE);
    //}

    //MainWindow_UpdateMenu();
}

void MainWindow_DoEmulatorParallel()
{
    //BOOL okParallel = Settings_GetParallel();
    //if (!okParallel)
    //{
    //    Emulator_SetParallel(TRUE);
    //    Settings_SetParallel(TRUE);
    //}
    //else
    //{
    //    Emulator_SetParallel(FALSE);
    //    Settings_SetParallel(FALSE);
    //}

    //MainWindow_UpdateMenu();
}

void MainWindow_DoFileLoadState()
{
    TCHAR bufFileName[MAX_PATH];
    BOOL okResult = ShowOpenDialog(g_hwnd,
            _T("Open state image to load"),
            _T("MS0515 state images (*.msst)\0*.msst\0All Files (*.*)\0*.*\0\0"),
            bufFileName);
    if (!okResult) return;

    if (!Emulator_LoadImage(bufFileName))
    {
        AlertWarning(_T("Failed to load image file."));
    }

    MainWindow_UpdateAllViews();
}

void MainWindow_DoFileSaveState()
{
    TCHAR bufFileName[MAX_PATH];
    BOOL okResult = ShowSaveDialog(g_hwnd,
            _T("Save state image as"),
            _T("MS0515 state images (*.msst)\0*.msst\0All Files (*.*)\0*.*\0\0"),
            _T("msst"),
            bufFileName);
    if (! okResult) return;

    if (!Emulator_SaveImage(bufFileName))
    {
        AlertWarning(_T("Failed to save image file."));
    }
}

void MainWindow_DoFileScreenshot()
{
    TCHAR bufFileName[MAX_PATH];
    SYSTEMTIME st;
    ::GetSystemTime(&st);
    wsprintf(bufFileName, _T("%04d%02d%02d%02d%02d%02d%03d.png"),
            st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

    if (!ScreenView_SaveScreenshot(bufFileName))
    {
        AlertWarning(_T("Failed to save screenshot bitmap."));
    }
}

void MainWindow_DoFileScreenshotSaveAs()
{
    TCHAR bufFileName[MAX_PATH];
    BOOL okResult = ShowSaveDialog(g_hwnd,
            _T("Save screenshot as"),
            _T("PNG bitmaps (*.png)\0*.png\0BMP bitmaps (*.bmp)\0*.bmp\0All Files (*.*)\0*.*\0\0"),
            _T("png"),
            bufFileName);
    if (! okResult) return;

    if (!ScreenView_SaveScreenshot(bufFileName))
    {
        AlertWarning(_T("Failed to save screenshot bitmap."));
    }
}

//void MainWindow_DoFileCreateDisk()
//{
//    Dialogs_DoCreateDisk();
//}

void MainWindow_DoFileSettings()
{
    ShowSettingsDialog();
}

void MainWindow_DoEmulatorFloppy(int slot)
{
    BOOL okImageAttached = g_pBoard->IsFloppyImageAttached(slot);
    if (okImageAttached)
    {
        g_pBoard->DetachFloppyImage(slot);
        Settings_SetFloppyFilePath(slot, NULL);
    }
    else
    {
        //if ((g_nEmulatorConfiguration & BK_COPT_FDD) == 0)
        //{
        //    AlertWarning(_T("Current configuration has no floppy controller."));
        //    return;
        //}

        // File Open dialog
        TCHAR bufFileName[MAX_PATH];
        BOOL okResult = ShowOpenDialog(g_hwnd,
                _T("Open floppy image to attach"),
                _T("MS0515 floppy images (*.dsk)\0*.dsk\0All Files (*.*)\0*.*\0\0"),
                bufFileName);
        if (! okResult) return;

        if (! g_pBoard->AttachFloppyImage(slot, bufFileName))
        {
            AlertWarning(_T("Failed to attach floppy image."));
            return;
        }

        Settings_SetFloppyFilePath(slot, bufFileName);
    }
    MainWindow_UpdateMenu();
}

void MainWindow_OnToolbarGetInfoTip(LPNMTBGETINFOTIP lpnm)
{
    int commandId = lpnm->iItem;

    if (commandId == ID_EMULATOR_FLOPPY0 || commandId == ID_EMULATOR_FLOPPY1 ||
        commandId == ID_EMULATOR_FLOPPY2 || commandId == ID_EMULATOR_FLOPPY3)
    {
        int floppyslot = 0;
        switch (commandId)
        {
        case ID_EMULATOR_FLOPPY0: floppyslot = 0; break;
        case ID_EMULATOR_FLOPPY1: floppyslot = 1; break;
        case ID_EMULATOR_FLOPPY2: floppyslot = 2; break;
        case ID_EMULATOR_FLOPPY3: floppyslot = 3; break;
        }

        if (g_pBoard->IsFloppyImageAttached(floppyslot))
        {
            TCHAR buffilepath[MAX_PATH];
            Settings_GetFloppyFilePath(floppyslot, buffilepath);

            LPCTSTR lpFileName = GetFileNameFromFilePath(buffilepath);
            _tcsncpy_s(lpnm->pszText, 80, lpFileName, _TRUNCATE);
        }
    }
}

void MainWindow_UpdateAllViews()
{
    // Update cached values in views
    Emulator_OnUpdate();
    DebugView_OnUpdate();
    DisasmView_OnUpdate();

    // Update screen
    InvalidateRect(g_hwndScreen, NULL, TRUE);

    // Update debug windows
    if (g_hwndDebug != NULL)
        InvalidateRect(g_hwndDebug, NULL, TRUE);
    if (g_hwndDisasm != NULL)
        InvalidateRect(g_hwndDisasm, NULL, TRUE);
    if (g_hwndMemory != NULL)
        InvalidateRect(g_hwndMemory, NULL, TRUE);
    //if (g_hwndMemoryMap != NULL)
    //    InvalidateRect(g_hwndMemoryMap, NULL, TRUE);
}

void MainWindow_SetToolbarImage(int commandId, int imageIndex)
{
    TBBUTTONINFO info;
    info.cbSize = sizeof(info);
    info.iImage = imageIndex;
    info.dwMask = TBIF_IMAGE;
    SendMessage(m_hwndToolbar, TB_SETBUTTONINFO, commandId, (LPARAM) &info);
}

void MainWindow_SetStatusbarText(int part, LPCTSTR message)
{
    SendMessage(m_hwndStatusbar, SB_SETTEXT, part, (LPARAM) message);
}
void MainWindow_SetStatusbarBitmap(int part, UINT resourceId)
{
    SendMessage(m_hwndStatusbar, SB_SETTEXT, part | SBT_OWNERDRAW, (LPARAM) resourceId);
}
void MainWindow_SetStatusbarIcon(int part, HICON hIcon)
{
    SendMessage(m_hwndStatusbar, SB_SETICON, part, (LPARAM) hIcon);
}


//////////////////////////////////////////////////////////////////////
