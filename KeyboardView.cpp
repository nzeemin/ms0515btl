/*  This file is part of UKNCBTL.
    UKNCBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    UKNCBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
UKNCBTL. If not, see <http://www.gnu.org/licenses/>. */

// KeyboardView.cpp

#include "stdafx.h"
#include "Main.h"
#include "Views.h"
#include "Emulator.h"


//////////////////////////////////////////////////////////////////////

#define COLOR_KEYBOARD_BACKGROUND   RGB(220,220,220)
#define COLOR_KEYBOARD_LITE         RGB(240,240,240)
#define COLOR_KEYBOARD_GRAY         RGB(210,210,210)
#define COLOR_KEYBOARD_DARK         RGB(100,100,100)
#define COLOR_KEYBOARD_RED          RGB(200,80,80)

#define KEYCLASSLITE 0
#define KEYCLASSGRAY 1
#define KEYCLASSDARK 2

#define KEYSCAN_NONE 255

HWND g_hwndKeyboard = (HWND) INVALID_HANDLE_VALUE;  // Keyboard View window handle

int m_nKeyboardBitmapLeft = 0;
int m_nKeyboardBitmapTop = 0;
BYTE m_nKeyboardKeyPressed = KEYSCAN_NONE;  // Scan-code for the key pressed, or KEYSCAN_NONE

void KeyboardView_OnDraw(HDC hdc);
int KeyboardView_GetKeyByPoint(int x, int y);
void Keyboard_DrawKey(HDC hdc, BYTE keyscan);


//////////////////////////////////////////////////////////////////////

#define KEYEXTRA_CIF    0201
#define KEYEXTRA_DOP    0202
#define KEYEXTRA_SHIFT  0203
#define KEYEXTRA_RUS    0204
#define KEYEXTRA_LAT    0205
#define KEYEXTRA_RUSLAT 0206
#define KEYEXTRA_UPR    0207
#define KEYEXTRA_FPB    0210

#define KEYEXTRA_START  0220
#define KEYEXTRA_STOP   0221
#define KEYEXTRA_TIMER  0222

struct KeyboardKeys
{
    int x, y, w, h;
    int keyclass;
    LPCTSTR text;
    BYTE scan;
}
m_arrKeyboardKeys[] =
{
    {   2,   2, 25, 25, KEYCLASSGRAY, _T(" 1"),     0126 }, // ÑÒÎÏ ÊÀÄÐ
    {  27,   2, 25, 25, KEYCLASSGRAY, _T(" 2"),     0127 }, // ÏÅ×ÀÒÜ ÊÀÄÐÀ
    {  52,   2, 25, 25, KEYCLASSGRAY, _T(" 3"),     0130 }, // ÏÀÓÇÀ
    {  77,   2, 25, 25, KEYCLASSGRAY, _T(" 4"),     0131 }, // ÓÑÒ ÐÅÆÈÌÀ
    { 102,   2, 25, 25, KEYCLASSGRAY, _T(" 5"),     0132 }, // Ô5
    { 152,   2, 25, 25, KEYCLASSGRAY, _T(" 6"),     0144 }, // ÏÐÅÐÛÂ
    { 177,   2, 25, 25, KEYCLASSGRAY, _T(" 7"),     0145 }, // ÏÐÎÄÎËÆ
    { 202,   2, 25, 25, KEYCLASSGRAY, _T(" 8"),     0146 }, // ÎÒÌÅÍ
    { 227,   2, 25, 25, KEYCLASSGRAY, _T(" 9"),     0147 }, // ÎÑÍÎÂÍ ÊÀÄÐ
    { 252,   2, 25, 25, KEYCLASSGRAY, _T("10"),     0150 }, // ÂÛÕÎÄ
    { 302,   2, 25, 25, KEYCLASSGRAY, _T("11"),     0161 },
    { 327,   2, 25, 25, KEYCLASSGRAY, _T("12"),     0162 },
    { 352,   2, 25, 25, KEYCLASSGRAY, _T("13"),     0163 },
    { 377,   2, 25, 25, KEYCLASSGRAY, _T("14"),     0164 },
    { 427,   2, 25, 25, KEYCLASSGRAY, _T("ïì"),     0174 }, // ÏÌ
    { 477,   2, 25, 25, KEYCLASSGRAY, _T("èñï"),    0175 }, // ÈÑÏ
    { 527,   2, 25, 25, KEYCLASSGRAY, _T("17"),     0200 },
    { 552,   2, 25, 25, KEYCLASSGRAY, _T("18"),     0201 },
    { 577,   2, 25, 25, KEYCLASSGRAY, _T("19"),     0202 },
    { 602,   2, 25, 25, KEYCLASSGRAY, _T("20"),     0203 },

    {   2,  42, 25, 25, KEYCLASSLITE, _T("{ |"),    0374 }, // { |
    {  27,  42, 25, 25, KEYCLASSLITE, _T("; +"),    0277 }, // ; +
    {  52,  42, 25, 25, KEYCLASSLITE, _T("1 !"),    0300 }, // 1 !
    {  77,  42, 25, 25, KEYCLASSLITE, _T("2 \""),   0305 }, // 2 "
    { 102,  42, 25, 25, KEYCLASSLITE, _T("3 #"),    0313 }, // 3 #
    { 127,  42, 25, 25, KEYCLASSLITE, _T("4 \u00a4"), 0320 }, // 4
    { 152,  42, 25, 25, KEYCLASSLITE, _T("5 %"),    0326 }, // 5 %
    { 177,  42, 25, 25, KEYCLASSLITE, _T("6 &"),    0333 }, // 6 &
    { 202,  42, 25, 25, KEYCLASSLITE, _T("7 \'"),   0340 }, // 7 '
    { 227,  42, 25, 25, KEYCLASSLITE, _T("8 ("),    0345 }, // 8 (
    { 252,  42, 25, 25, KEYCLASSLITE, _T("9 )"),    0352 }, // 9 )
    { 277,  42, 25, 25, KEYCLASSLITE, _T("0"),      0357 }, // 0
    { 302,  42, 25, 25, KEYCLASSLITE, _T("- ="),    0371 }, // - =
    { 327,  42, 25, 25, KEYCLASSLITE, _T("} \u2196"), 0365 }, // } NW-arrow
    { 352,  42, 25, 25, KEYCLASSLITE, _T("  "),     0000 },
    { 377,  42, 25, 25, KEYCLASSLITE, _T("ÇÁ"),     0274 }, // ÇÁ

    {  15,  67, 25, 25, KEYCLASSLITE, _T("ÒÀÁ"),    0276 }, // TAB
    {  40,  67, 25, 25, KEYCLASSLITE, _T("ÉJ"),     0301 }, // É J
    {  65,  67, 25, 25, KEYCLASSLITE, _T("ÖC"),     0306 }, // Ö C
    {  90,  67, 25, 25, KEYCLASSLITE, _T("ÓU"),     0314 }, // Ó U
    { 115,  67, 25, 25, KEYCLASSLITE, _T("ÊK"),     0321 }, // Ê K
    { 140,  67, 25, 25, KEYCLASSLITE, _T("ÅE"),     0327 }, // Å E
    { 165,  67, 25, 25, KEYCLASSLITE, _T("ÍN"),     0334 }, // Í N
    { 190,  67, 25, 25, KEYCLASSLITE, _T("ÃG"),     0341 }, // Ã G
    { 215,  67, 25, 25, KEYCLASSLITE, _T("Ø ["),    0346 }, // Ø [
    { 240,  67, 25, 25, KEYCLASSLITE, _T("Ù ]"),    0353 }, // Ù ]
    { 265,  67, 25, 25, KEYCLASSLITE, _T("ÇZ"),     0360 }, // Ç Z
    { 290,  67, 25, 25, KEYCLASSLITE, _T("ÕH"),     0366 }, // Õ H
    { 315,  67, 25, 25, KEYCLASSLITE, _T(": *"),    0372 }, // : *
    { 340,  67, 25, 25, KEYCLASSLITE, _T("~"),      0000 }, // ~
    { 365,  67, 25, 25, KEYCLASSLITE, _T("ÂÊ"),     0275 }, // ÂÊ

    {   2,  92, 25, 25, KEYCLASSLITE, _T("ÑÓ"),     0000 }, // ÑÓ
    {  27,  92, 25, 25, KEYCLASSLITE, _T("ôêñ"),    0260 }, // ÔÊÑ
    {  52,  92, 25, 25, KEYCLASSLITE, _T("ÔF"),     0302 }, // Ô F
    {  77,  92, 25, 25, KEYCLASSLITE, _T("ÛY"),     0307 }, // Û Y
    { 102,  92, 25, 25, KEYCLASSLITE, _T("ÂW"),     0315 }, // Â W
    { 127,  92, 25, 25, KEYCLASSLITE, _T("ÀA"),     0322 }, // À A
    { 152,  92, 25, 25, KEYCLASSLITE, _T("ÏP"),     0330 }, // Ï P
    { 177,  92, 25, 25, KEYCLASSLITE, _T("ÐR"),     0335 }, // Ð R
    { 202,  92, 25, 25, KEYCLASSLITE, _T("ÎO"),     0342 }, // Î O
    { 227,  92, 25, 25, KEYCLASSLITE, _T("ËL"),     0347 }, // Ë L
    { 252,  92, 25, 25, KEYCLASSLITE, _T("ÄD"),     0354 }, // Ä D
    { 277,  92, 25, 25, KEYCLASSLITE, _T("ÆV"),     0362 }, // Æ V
    { 302,  92, 25, 25, KEYCLASSLITE, _T("Ý \\"),   0373 }, // Ý backslash
    { 327,  92, 25, 25, KEYCLASSLITE, _T(". >"),    0367 }, // . >
    { 352,  92, 25, 25, KEYCLASSLITE, _T("Ú"),      0304 }, // Ú

    {  15, 117, 25, 25, KEYCLASSLITE, _T("ÂÐ"),     0000 }, // Shift
    {  40, 117, 25, 25, KEYCLASSLITE, _T("Ð/Ë"),    0262 }, // ÐÓÑ / ËÀÒ
    {  65, 117, 25, 25, KEYCLASSLITE, _T("ßQ"),     0303 }, // ß Q
    {  90, 117, 25, 25, KEYCLASSLITE, _T("×\u00ac"), 0310 }, // × ^
    { 115, 117, 25, 25, KEYCLASSLITE, _T("ÑS"),     0316 }, // Ñ S
    { 140, 117, 25, 25, KEYCLASSLITE, _T("ÌM"),     0323 }, // Ì M
    { 165, 117, 25, 25, KEYCLASSLITE, _T("ÈI"),     0331 }, // È I
    { 190, 117, 25, 25, KEYCLASSLITE, _T("ÒT"),     0336 }, // Ò T
    { 215, 117, 25, 25, KEYCLASSLITE, _T("ÜX"),     0343 }, // Ü X
    { 240, 117, 25, 25, KEYCLASSLITE, _T("ÁB"),     0350 }, // Á B
    { 265, 117, 25, 25, KEYCLASSLITE, _T("Þ@"),     0355 }, // Þ @
    { 290, 117, 25, 25, KEYCLASSLITE, _T(", <"),    0363 }, // ,
    { 315, 117, 25, 25, KEYCLASSLITE, _T("/ ?"),    0312 }, // /
    { 340, 117, 25, 25, KEYCLASSLITE, _T("_"),      0361 }, // _
    { 365, 117, 25, 25, KEYCLASSLITE, _T("ÂÐ"),     0000 }, // Shift

    {  65, 142, 25, 25, KEYCLASSLITE, _T("êìï"),    0261 }, // ÊÌÏ
    {  90, 142, 225, 25, KEYCLASSLITE, NULL,        0324 }, // Space

    { 427,  42, 25, 25, KEYCLASSGRAY, _T("íò"),     0212 }, // ÍÒ
    { 452,  42, 25, 25, KEYCLASSGRAY, _T("âñò"),    0213 }, // ÂÑÒ
    { 477,  42, 25, 25, KEYCLASSGRAY, _T("óä"),     0214 }, // ÓÄÀË
    { 427,  67, 25, 25, KEYCLASSGRAY, _T("âáð"),    0215 }, // ÂÛÁÐ
    { 452,  67, 25, 25, KEYCLASSGRAY, _T("ÏðÊ"),    0216 }, // ÏÐÅÄ ÊÀÄÐ
    { 477,  67, 25, 25, KEYCLASSGRAY, _T("ÑëÊ"),    0217 }, // ÑÄÅÆ ÊÀÄÐ
    { 452,  92, 25, 25, KEYCLASSGRAY, _T("\u2191"), 0252 }, // Up
    { 427, 117, 25, 25, KEYCLASSGRAY, _T("\u2190"), 0247 }, // Left
    { 452, 117, 25, 25, KEYCLASSGRAY, _T("\u2193"), 0251 }, // Down
    { 477, 117, 25, 25, KEYCLASSGRAY, _T("\u2192"), 0250 }, // Right

    { 527,  42, 25, 25, KEYCLASSLITE, _T("ïô1"),    0241 }, // NumPad ÏÔ1
    { 552,  42, 25, 25, KEYCLASSLITE, _T("ïô2"),    0242 }, // NumPad ÏÔ2
    { 577,  42, 25, 25, KEYCLASSLITE, _T("ïô3"),    0243 }, // NumPad ÏÔ3
    { 602,  42, 25, 25, KEYCLASSLITE, _T("ïô4"),    0244 }, // NumPad ÏÔ4
    { 527,  67, 25, 25, KEYCLASSLITE, _T("7"),      0235 }, // NumPad 7
    { 552,  67, 25, 25, KEYCLASSLITE, _T("8"),      0236 }, // NumPad 8
    { 577,  67, 25, 25, KEYCLASSLITE, _T("9"),      0237 }, // NumPad 9
    { 602,  67, 25, 25, KEYCLASSLITE, _T(","),      0234 }, // NumPad
    { 527,  92, 25, 25, KEYCLASSLITE, _T("4"),      0231 }, // NumPad 4
    { 552,  92, 25, 25, KEYCLASSLITE, _T("5"),      0232 }, // NumPad 5
    { 577,  92, 25, 25, KEYCLASSLITE, _T("6"),      0233 }, // NumPad 6
    { 602,  92, 25, 25, KEYCLASSLITE, _T("-"),      0240 }, // NumPad
    { 527, 117, 25, 25, KEYCLASSLITE, _T("1"),      0226 }, // NumPad 1
    { 552, 117, 25, 25, KEYCLASSLITE, _T("2"),      0227 }, // NumPad 2
    { 577, 117, 25, 25, KEYCLASSLITE, _T("3"),      0230 }, // NumPad 3
    { 602, 117, 25, 25, KEYCLASSLITE, _T("."),      0224 }, // NumPad
    { 527, 142, 50, 25, KEYCLASSLITE, _T("0"),      0105 }, // NumPad 0 ÂÑÒ
    { 577, 142, 50, 25, KEYCLASSLITE, _T("ââîä"),   0225 }, // NumPad ÂÂÎÄ
};

const int m_nKeyboardKeysCount = sizeof(m_arrKeyboardKeys) / sizeof(KeyboardKeys);


//////////////////////////////////////////////////////////////////////


void KeyboardView_RegisterClass()
{
    WNDCLASSEX wcex;
    wcex.cbSize = sizeof(WNDCLASSEX);

    wcex.style			= CS_HREDRAW | CS_VREDRAW;
    wcex.lpfnWndProc	= KeyboardViewWndProc;
    wcex.cbClsExtra		= 0;
    wcex.cbWndExtra		= 0;
    wcex.hInstance		= g_hInst;
    wcex.hIcon			= NULL;
    wcex.hCursor		= LoadCursor(NULL, IDC_ARROW);
    wcex.hbrBackground	= (HBRUSH)(COLOR_BTNFACE + 1);
    wcex.lpszMenuName	= NULL;
    wcex.lpszClassName	= CLASSNAME_KEYBOARDVIEW;
    wcex.hIconSm		= NULL;

    RegisterClassEx(&wcex);
}

void KeyboardView_Init()
{
}

void KeyboardView_Done()
{
}

void KeyboardView_Create(HWND hwndParent, int x, int y, int width, int height)
{
    ASSERT(hwndParent != NULL);

    g_hwndKeyboard = CreateWindow(
            CLASSNAME_KEYBOARDVIEW, NULL,
            WS_CHILD | WS_VISIBLE,
            x, y, width, height,
            hwndParent, NULL, g_hInst, NULL);
}

LRESULT CALLBACK KeyboardViewWndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (message)
    {
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(hWnd, &ps);

            KeyboardView_OnDraw(hdc);

            EndPaint(hWnd, &ps);
        }
        break;
    case WM_SETCURSOR:
        {
            POINT ptCursor;  ::GetCursorPos(&ptCursor);
            ::ScreenToClient(g_hwndKeyboard, &ptCursor);
            int keyindex = KeyboardView_GetKeyByPoint(ptCursor.x, ptCursor.y);
            LPCTSTR cursor = (keyindex == -1) ? IDC_ARROW : IDC_HAND;
            ::SetCursor(::LoadCursor(NULL, cursor));
        }
        return (LRESULT)TRUE;
    case WM_LBUTTONDOWN:
        {
            int x = LOWORD(lParam);
            int y = HIWORD(lParam);
            WORD fwkeys = wParam;

            int keyindex = KeyboardView_GetKeyByPoint(x, y);
            if (keyindex == -1) break;
            BYTE keyscan = (BYTE) m_arrKeyboardKeys[keyindex].scan;
            if (keyscan == KEYSCAN_NONE) break;

            // Fire keydown event
            ScreenView_KeyEvent(keyscan, TRUE);

            ::SetCapture(g_hwndKeyboard);

            // Draw focus frame for the key pressed
            HDC hdc = ::GetDC(g_hwndKeyboard);
            Keyboard_DrawKey(hdc, keyscan);
            ::ReleaseDC(g_hwndKeyboard, hdc);

            // Remember key pressed
            m_nKeyboardKeyPressed = keyscan;
            break;
        }
    case WM_LBUTTONUP:
        if (m_nKeyboardKeyPressed != KEYSCAN_NONE)
        {
            // Fire keyup event and release mouse
            ScreenView_KeyEvent(m_nKeyboardKeyPressed, FALSE);
            ::ReleaseCapture();

            // Draw focus frame for the released key
            HDC hdc = ::GetDC(g_hwndKeyboard);
            Keyboard_DrawKey(hdc, m_nKeyboardKeyPressed);
            ::ReleaseDC(g_hwndKeyboard, hdc);

            m_nKeyboardKeyPressed = KEYSCAN_NONE;
        }
        break;
    default:
        return DefWindowProc(hWnd, message, wParam, lParam);
    }
    return (LRESULT)FALSE;
}

void KeyboardView_OnDraw(HDC hdc)
{
    RECT rc;  ::GetClientRect(g_hwndKeyboard, &rc);

    // Keyboard background
    HBRUSH hBkBrush = ::CreateSolidBrush(COLOR_KEYBOARD_BACKGROUND);
    HGDIOBJ hOldBrush = ::SelectObject(hdc, hBkBrush);
    ::PatBlt(hdc, 0, 0, rc.right, rc.bottom, PATCOPY);
    ::SelectObject(hdc, hOldBrush);

    if (m_nKeyboardKeyPressed != KEYSCAN_NONE)
        Keyboard_DrawKey(hdc, m_nKeyboardKeyPressed);

    HBRUSH hbrLite = ::CreateSolidBrush(COLOR_KEYBOARD_LITE);
    HBRUSH hbrGray = ::CreateSolidBrush(COLOR_KEYBOARD_GRAY);
    HBRUSH hbrDark = ::CreateSolidBrush(COLOR_KEYBOARD_DARK);
    HBRUSH hbrRed = ::CreateSolidBrush(COLOR_KEYBOARD_RED);
    m_nKeyboardBitmapLeft = (rc.right - 630) / 2;
    m_nKeyboardBitmapTop = (rc.bottom - 170) / 2;
    if (m_nKeyboardBitmapTop < 0) m_nKeyboardBitmapTop = 0;
    if (m_nKeyboardBitmapTop > 16) m_nKeyboardBitmapTop = 16;

    HFONT hfont = CreateDialogFont();
    HGDIOBJ hOldFont = ::SelectObject(hdc, hfont);
    ::SetBkMode(hdc, TRANSPARENT);

    // Draw keys
    for (int i = 0; i < m_nKeyboardKeysCount; i++)
    {
        RECT rcKey;
        rcKey.left = m_nKeyboardBitmapLeft + m_arrKeyboardKeys[i].x;
        rcKey.top = m_nKeyboardBitmapTop + m_arrKeyboardKeys[i].y;
        rcKey.right = rcKey.left + m_arrKeyboardKeys[i].w;
        rcKey.bottom = rcKey.top + m_arrKeyboardKeys[i].h;

        HBRUSH hbr = hBkBrush;
        COLORREF textcolor = COLOR_KEYBOARD_DARK;
        switch (m_arrKeyboardKeys[i].keyclass)
        {
        case KEYCLASSLITE: hbr = hbrLite; break;
        case KEYCLASSGRAY: hbr = hbrGray; break;
        case KEYCLASSDARK: hbr = hbrDark;  textcolor = COLOR_KEYBOARD_LITE; break;
        }
        HGDIOBJ hOldBrush = ::SelectObject(hdc, hbr);
        //rcKey.left++; rcKey.top++; rcKey.right--; rc.bottom--;
        ::PatBlt(hdc, rcKey.left, rcKey.top, rcKey.right - rcKey.left, rcKey.bottom - rcKey.top, PATCOPY);
        ::SelectObject(hdc, hOldBrush);

        //TCHAR text[10];
        //wsprintf(text, _T("%02x"), (int)m_arrKeyboardKeys[i].scan);
        LPCTSTR text = m_arrKeyboardKeys[i].text;
        if (text != NULL)
        {
            ::SetTextColor(hdc, textcolor);
            ::DrawText(hdc, text, wcslen(text), &rcKey, DT_NOPREFIX | DT_SINGLELINE | DT_CENTER | DT_VCENTER);
        }

        ::DrawEdge(hdc, &rcKey, BDR_RAISEDOUTER, BF_RECT);
    }

    ::SelectObject(hdc, hOldFont);
    ::DeleteObject(hfont);

    ::DeleteObject(hbrLite);
    ::DeleteObject(hbrGray);
    ::DeleteObject(hbrDark);
    ::DeleteObject(hbrRed);
    ::DeleteObject(hBkBrush);
}

// Returns: index of key under the cursor position, or -1 if not found
int KeyboardView_GetKeyByPoint(int x, int y)
{
    for (int i = 0; i < m_nKeyboardKeysCount; i++)
    {
        RECT rcKey;
        rcKey.left = m_nKeyboardBitmapLeft + m_arrKeyboardKeys[i].x;
        rcKey.top = m_nKeyboardBitmapTop + m_arrKeyboardKeys[i].y;
        rcKey.right = rcKey.left + m_arrKeyboardKeys[i].w;
        rcKey.bottom = rcKey.top + m_arrKeyboardKeys[i].h;

        if (x >= rcKey.left && x < rcKey.right && y >= rcKey.top && y < rcKey.bottom)
        {
            return i;
        }
    }
    return -1;
}

void Keyboard_DrawKey(HDC hdc, BYTE keyscan)
{
    for (int i = 0; i < m_nKeyboardKeysCount; i++)
        if (keyscan == m_arrKeyboardKeys[i].scan)
        {
            RECT rcKey;
            rcKey.left = m_nKeyboardBitmapLeft + m_arrKeyboardKeys[i].x;
            rcKey.top = m_nKeyboardBitmapTop + m_arrKeyboardKeys[i].y;
            rcKey.right = rcKey.left + m_arrKeyboardKeys[i].w;
            rcKey.bottom = rcKey.top + m_arrKeyboardKeys[i].h;
            ::DrawFocusRect(hdc, &rcKey);
        }
}


//////////////////////////////////////////////////////////////////////
