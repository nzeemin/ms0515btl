﻿/*  This file is part of MS0515BTL.
    MS0515BTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    MS0515BTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
MS0515BTL. If not, see <http://www.gnu.org/licenses/>. */

// ScreenView.cpp

#include "stdafx.h"
#include <windowsx.h>
#include <mmintrin.h>
#include <vfw.h>
#include "Main.h"
#include "Views.h"
#include "Emulator.h"
#include "util/BitmapFile.h"

//////////////////////////////////////////////////////////////////////


#define COLOR_BK_BACKGROUND RGB(115,115,115)


HWND g_hwndScreen = NULL;  // Screen View window handle

HDRAWDIB m_hdd = NULL;
BITMAPINFO m_bmpinfo;
HBITMAP m_hbmp = NULL;
DWORD * m_bits = NULL;
int m_cxScreenWidth = MS0515_SCREEN_WIDTH;
int m_cyScreenHeight = MS0515_SCREEN_HEIGHT;
BYTE m_ScreenKeyState[256];
int m_ScreenMode = 0;

void ScreenView_CreateDisplay();
void ScreenView_OnDraw(HDC hdc);
//BOOL ScreenView_OnKeyEvent(WPARAM vkey, BOOL okExtKey, BOOL okPressed);
void ScreenView_OnRButtonDown(int mousex, int mousey);

const int KEYEVENT_QUEUE_SIZE = 32;
WORD m_ScreenKeyQueue[KEYEVENT_QUEUE_SIZE];
int m_ScreenKeyQueueTop = 0;
int m_ScreenKeyQueueBottom = 0;
int m_ScreenKeyQueueCount = 0;
void ScreenView_PutKeyEventToQueue(WORD keyevent);
WORD ScreenView_GetKeyEventFromQueue();


//////////////////////////////////////////////////////////////////////

void ScreenView_RegisterClass()
{
    WNDCLASSEX wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style          = CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc    = ScreenViewWndProc;
    wcex.cbClsExtra     = 0;
    wcex.cbWndExtra     = 0;
    wcex.hInstance      = g_hInst;
    wcex.hIcon          = NULL;
    wcex.hCursor        = LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground  = NULL; //(HBRUSH)(COLOR_WINDOW+1);
    wcex.lpszMenuName   = NULL;
    wcex.lpszClassName  = CLASSNAME_SCREENVIEW;
    wcex.hIconSm        = NULL;

    RegisterClassEx(&wcex);
}

void ScreenView_Init()
{
    m_hdd = DrawDibOpen();
    ScreenView_CreateDisplay();
}

void ScreenView_Done()
{
    if (m_hbmp != NULL)
    {
        DeleteObject(m_hbmp);
        m_hbmp = NULL;
    }

    DrawDibClose( m_hdd );
}

void ScreenView_CreateDisplay()
{
    ASSERT(g_hwnd != NULL);

    if (m_hbmp != NULL)
    {
        DeleteObject(m_hbmp);
        m_hbmp = NULL;
    }

    HDC hdc = GetDC( g_hwnd );

    m_bmpinfo.bmiHeader.biSize = sizeof( BITMAPINFOHEADER );
    m_bmpinfo.bmiHeader.biWidth = m_cxScreenWidth;
    m_bmpinfo.bmiHeader.biHeight = m_cyScreenHeight;
    m_bmpinfo.bmiHeader.biPlanes = 1;
    m_bmpinfo.bmiHeader.biBitCount = 32;
    m_bmpinfo.bmiHeader.biCompression = BI_RGB;
    m_bmpinfo.bmiHeader.biSizeImage = 0;
    m_bmpinfo.bmiHeader.biXPelsPerMeter = 0;
    m_bmpinfo.bmiHeader.biYPelsPerMeter = 0;
    m_bmpinfo.bmiHeader.biClrUsed = 0;
    m_bmpinfo.bmiHeader.biClrImportant = 0;

    m_hbmp = CreateDIBSection( hdc, &m_bmpinfo, DIB_RGB_COLORS, (void **) &m_bits, NULL, 0 );

    ReleaseDC( g_hwnd, hdc );
}

// Create Screen View as child of Main Window
void ScreenView_Create(HWND hwndParent, int x, int y)
{
    ASSERT(hwndParent != NULL);

    int xLeft = x;
    int yTop = y;
    int cyScreenHeight = m_cyScreenHeight + 8;
    int cyHeight = 4 + cyScreenHeight + 4;
    int cxWidth = 4 + m_cxScreenWidth + 4;

    g_hwndScreen = CreateWindow(
            CLASSNAME_SCREENVIEW, NULL,
            WS_CHILD | WS_VISIBLE,
            xLeft, yTop, cxWidth, cyHeight,
            hwndParent, NULL, g_hInst, NULL);

    // Initialize m_ScreenKeyState
    VERIFY(::GetKeyboardState(m_ScreenKeyState));
}

LRESULT CALLBACK ScreenViewWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_COMMAND:
        ::PostMessage(g_hwnd, WM_COMMAND, wParam, lParam);
        break;
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);

            ScreenView_PrepareScreen();  //DEBUG
            ScreenView_OnDraw(hdc);

            EndPaint(hWnd, &ps);
        }
        break;
    case WM_LBUTTONDOWN:
        SetFocus(hWnd);
        break;
    case WM_RBUTTONDOWN:
        ScreenView_OnRButtonDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        break;
        //case WM_KEYDOWN:
        //    //if ((lParam & (1 << 30)) != 0)  // Auto-repeats should be ignored
        //    //    return (LRESULT) TRUE;
        //    //return (LRESULT) ScreenView_OnKeyEvent(wParam, (lParam & (1 << 24)) != 0, TRUE);
        //    return (LRESULT) TRUE;
        //case WM_KEYUP:
        //    //return (LRESULT) ScreenView_OnKeyEvent(wParam, (lParam & (1 << 24)) != 0, FALSE);
        //    return (LRESULT) TRUE;
    case WM_SETCURSOR:
        if (::GetFocus() == g_hwndScreen)
        {
            SetCursor(::LoadCursor(NULL, MAKEINTRESOURCE(IDC_IBEAM)));
            return (LRESULT) TRUE;
        }
        else
            return DefWindowProc(hWnd, message, wParam, lParam);
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return (LRESULT)FALSE;
}

void ScreenView_OnRButtonDown(int mousex, int mousey)
{
    ::SetFocus(g_hwndScreen);

    HMENU hMenu = ::CreatePopupMenu();
    ::AppendMenu(hMenu, 0, ID_FILE_SCREENSHOT, _T("Screenshot"));
    ::AppendMenu(hMenu, 0, ID_FILE_SCREENSHOTTOCLIPBOARD, _T("Screenshot to Clipboard"));

    POINT pt = { mousex, mousey };
    ::ClientToScreen(g_hwndScreen, &pt);
    ::TrackPopupMenu(hMenu, 0, pt.x, pt.y, 0, g_hwndScreen, NULL);

    VERIFY(::DestroyMenu(hMenu));
}

int ScreenView_GetScreenMode()
{
    return m_ScreenMode;
}
void ScreenView_SetScreenMode(int newMode)
{
    if (m_ScreenMode == newMode) return;

    m_ScreenMode = newMode;

    // Ask Emulator module for screen width and height
    int cxWidth, cyHeight;
    Emulator_GetScreenSize(newMode, &cxWidth, &cyHeight);
    m_cxScreenWidth = cxWidth;
    m_cyScreenHeight = cyHeight;
    ScreenView_CreateDisplay();

    RECT rc;  ::GetWindowRect(g_hwndScreen, &rc);
    ::SetWindowPos(g_hwndScreen, NULL, 0, 0, 4 + cxWidth + 4, 4 + cyHeight + 4, SWP_NOZORDER | SWP_NOMOVE);

    ScreenView_RedrawScreen();
}

void ScreenView_OnDraw(HDC hdc)
{
    if (m_bits == NULL) return;

    RECT rc;  ::GetClientRect(g_hwndScreen, &rc);
    int x = (rc.right - m_cxScreenWidth) / 2;

    DrawDibDraw(m_hdd, hdc,
            x, 4, -1, -1,
            &m_bmpinfo.bmiHeader, m_bits, 0, 0,
            m_cxScreenWidth, m_cyScreenHeight,
            0);

    // Empty border
    HBRUSH hBrush = ::CreateSolidBrush(COLOR_BK_BACKGROUND);
    HGDIOBJ hOldBrush = ::SelectObject(hdc, hBrush);
    PatBlt(hdc, 0, 0, x, rc.bottom, PATCOPY);
    PatBlt(hdc, x + m_cxScreenWidth, 0, rc.right, rc.bottom, PATCOPY);
    PatBlt(hdc, x, 0, m_cxScreenWidth, 4, PATCOPY);
    PatBlt(hdc, x, rc.bottom - 4, m_cxScreenWidth, 4, PATCOPY);
    ::SelectObject(hdc, hOldBrush);
    ::DeleteObject(hBrush);
}

void ScreenView_RedrawScreen()
{
    ScreenView_PrepareScreen();

    HDC hdc = GetDC(g_hwndScreen);
    ScreenView_OnDraw(hdc);
    ::ReleaseDC(g_hwndScreen, hdc);
}

void ScreenView_PrepareScreen()
{
    if (m_bits == NULL) return;

    Emulator_PrepareScreenRGB32(m_bits, m_ScreenMode);
}

void ScreenView_PutKeyEventToQueue(WORD keyevent)
{
    if (m_ScreenKeyQueueCount == KEYEVENT_QUEUE_SIZE) return;  // Full queue

    m_ScreenKeyQueue[m_ScreenKeyQueueTop] = keyevent;
    m_ScreenKeyQueueTop++;
    if (m_ScreenKeyQueueTop >= KEYEVENT_QUEUE_SIZE)
        m_ScreenKeyQueueTop = 0;
    m_ScreenKeyQueueCount++;
}
WORD ScreenView_GetKeyEventFromQueue()
{
    if (m_ScreenKeyQueueCount == 0) return 0;  // Empty queue

    WORD keyevent = m_ScreenKeyQueue[m_ScreenKeyQueueBottom];
    m_ScreenKeyQueueBottom++;
    if (m_ScreenKeyQueueBottom >= KEYEVENT_QUEUE_SIZE)
        m_ScreenKeyQueueBottom = 0;
    m_ScreenKeyQueueCount--;

    return keyevent;
}

const BYTE arrPcChar2MS7004scan[256] =    // MS7004 scans from PC vkeys
{
    /*       0     1     2     3     4     5     6     7     8     9     a     b     c     d     e     f  */
    /*0*/    0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0274, 0276, 0000, 0000, 0000, 0275, 0000, 0000,
    /*1*/    0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000,
    /*2*/    0324, 0000, 0000, 0000, 0000, 0247, 0252, 0250, 0251, 0000, 0000, 0000, 0000, 0000, 0000, 0000,
    /*3*/    0357, 0300, 0305, 0313, 0320, 0326, 0333, 0340, 0345, 0352, 0000, 0000, 0000, 0000, 0000, 0000,
    /*4*/    0000, 0322, 0350, 0306, 0354, 0327, 0302, 0341, 0366, 0331, 0301, 0321, 0347, 0323, 0334, 0342,
    /*5*/    0330, 0303, 0335, 0316, 0336, 0314, 0362, 0315, 0343, 0307, 0360, 0000, 0000, 0000, 0000, 0000,
    /*6*/    0222, 0226, 0227, 0230, 0231, 0232, 0233, 0235, 0236, 0237, 0000, 0000, 0000, 0000, 0000, 0000,
    /*7*/    0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000,
    /*8*/    0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000,
    /*9*/    0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000,
    /*a*/    0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000,
    /*b*/    0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0372, 0000, 0363, 0371, 0367, 0312,
    /*c*/    0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000,
    /*d*/    0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000,
    /*e*/    0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000,
    /*f*/    0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000,
};

void ScreenView_ScanKeyboard()
{
    if (! g_okEmulatorRunning) return;
    if (::GetFocus() == g_hwndScreen)
    {
        // Read current keyboard state
        BYTE keys[256];
        VERIFY(::GetKeyboardState(keys));

        //BOOL okShift = ((keys[VK_SHIFT] & 128) != 0);
        BOOL okCtrl = ((keys[VK_CONTROL] & 128) != 0);
        //HKL hkl = ::GetKeyboardLayout(0);

        // Check every key for state change
        for (int scan = 0; scan < 256; scan++)
        {
            BYTE newstate = keys[scan];
            BYTE oldstate = m_ScreenKeyState[scan];
            if ((newstate & 128) == (oldstate & 128))
                continue; // Key state not changed

            BYTE vkey = (BYTE) scan;

//            if (newstate & 128) DebugPrintFormat(_T("Key PC: 0x%0x %d %d\r\n"), vkey, okShift, okCtrl);

            BYTE key = arrPcChar2MS7004scan[vkey];
            if (key == 0)
                continue;

            BYTE pressed = (newstate & 128) | (okCtrl ? 64 : 0);
            WORD keyevent = MAKEWORD(key, pressed);
            ScreenView_PutKeyEventToQueue(keyevent);
            KeyboardView_KeyEvent(key, pressed);
        }

        // Save keyboard state
        ::memcpy(m_ScreenKeyState, keys, 256);
    }
}

void ScreenView_ProcessKeyboard()
{
    // Process next event in the keyboard queue
    WORD keyevent = ScreenView_GetKeyEventFromQueue();
    if (keyevent != 0)
    {
        bool pressed = ((keyevent & 0x8000) != 0);
        //bool ctrl = ((keyevent & 0x4000) != 0);
        BYTE scan = LOBYTE(keyevent);

//        DebugPrintFormat(_T("KeyEvent: 0x%0x %d %d\r\n"), scan, pressed, ctrl);

        g_pBoard->KeyboardEvent(scan, pressed);
    }
}

// External key event - e.g. from KeyboardView
void ScreenView_KeyEvent(BYTE keyscan, BOOL pressed)
{
    ScreenView_PutKeyEventToQueue(MAKEWORD(keyscan, pressed ? 128 : 0));
}

BOOL ScreenView_SaveScreenshot(LPCTSTR sFileName)
{
    ASSERT(sFileName != NULL);
    ASSERT(m_bits != NULL);

    uint32_t* pBits = (uint32_t*) ::calloc(m_cxScreenWidth * m_cyScreenHeight, sizeof(uint32_t));
    Emulator_PrepareScreenRGB32(pBits, m_ScreenMode);

    LPCTSTR sFileNameExt = _tcsrchr(sFileName, _T('.'));
    BitmapFileFormat format = BitmapFileFormatPng;
    if (sFileNameExt != NULL && _tcsicmp(sFileNameExt, _T(".bmp")) == 0)
        format = BitmapFileFormatBmp;
    else if (sFileNameExt != NULL && (_tcsicmp(sFileNameExt, _T(".tif")) == 0 || _tcsicmp(sFileNameExt, _T(".tiff")) == 0))
        format = BitmapFileFormatTiff;
    bool result = BitmapFile_SaveImageFile(
            (const uint32_t *)pBits, sFileName, format, m_cxScreenWidth, m_cyScreenHeight);

    ::free(pBits);

    return result;
}

HGLOBAL ScreenView_GetScreenshotAsDIB()
{
    void* pBits = ::calloc(m_cxScreenWidth * m_cyScreenHeight, 4);
    Emulator_PrepareScreenRGB32(pBits, m_ScreenMode);

    BITMAPINFOHEADER bi;
    ::ZeroMemory(&bi, sizeof(BITMAPINFOHEADER));
    bi.biSize = sizeof(BITMAPINFOHEADER);
    bi.biWidth = m_cxScreenWidth;
    bi.biHeight = m_cyScreenHeight;
    bi.biPlanes = 1;
    bi.biBitCount = 32;
    bi.biCompression = BI_RGB;
    bi.biSizeImage = bi.biWidth * bi.biHeight * 4;

    HGLOBAL hDIB = ::GlobalAlloc(GMEM_MOVEABLE, sizeof(BITMAPINFOHEADER) + bi.biSizeImage);
    if (hDIB == NULL)
    {
        ::free(pBits);
        return NULL;
    }

    LPBYTE p = (LPBYTE)::GlobalLock(hDIB);
    ::CopyMemory(p, &bi, sizeof(BITMAPINFOHEADER));
    p += sizeof(BITMAPINFOHEADER);
    for (int line = 0; line < m_cyScreenHeight; line++)
    {
        LPBYTE psrc = (LPBYTE)pBits + line * m_cxScreenWidth * 4;
        ::CopyMemory(p, psrc, m_cxScreenWidth * 4);
        p += m_cxScreenWidth * 4;
    }
    ::GlobalUnlock(hDIB);

    ::free(pBits);

    return hDIB;
}


//////////////////////////////////////////////////////////////////////
