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
    {   2,   2, 25, 25, KEYCLASSGRAY, _T(" 1"),     0126 }, // ���� ����
    {  27,   2, 25, 25, KEYCLASSGRAY, _T(" 2"),     0127 }, // ������ �����
    {  52,   2, 25, 25, KEYCLASSGRAY, _T(" 3"),     0130 }, // �����
    {  77,   2, 25, 25, KEYCLASSGRAY, _T(" 4"),     0131 }, // ��� ������
    { 102,   2, 25, 25, KEYCLASSGRAY, _T(" 5"),     0132 }, // �5
    { 152,   2, 25, 25, KEYCLASSGRAY, _T(" 6"),     0144 }, // ������
    { 177,   2, 25, 25, KEYCLASSGRAY, _T(" 7"),     0145 }, // �������
    { 202,   2, 25, 25, KEYCLASSGRAY, _T(" 8"),     0146 }, // �����
    { 227,   2, 25, 25, KEYCLASSGRAY, _T(" 9"),     0147 }, // ������ ����
    { 252,   2, 25, 25, KEYCLASSGRAY, _T("10"),     0150 }, // �����
    { 302,   2, 25, 25, KEYCLASSGRAY, _T("11"),     0161 },
    { 327,   2, 25, 25, KEYCLASSGRAY, _T("12"),     0162 },
    { 352,   2, 25, 25, KEYCLASSGRAY, _T("13"),     0163 },
    { 377,   2, 25, 25, KEYCLASSGRAY, _T("14"),     0164 },
    { 427,   2, 25, 25, KEYCLASSGRAY, _T("��"),     0174 }, // ��
    { 477,   2, 25, 25, KEYCLASSGRAY, _T("���"),    0175 }, // ���
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
    { 377,  42, 25, 25, KEYCLASSLITE, _T("��"),     0274 }, // ��

    {  15,  67, 25, 25, KEYCLASSLITE, _T("���"),    0276 }, // TAB
    {  40,  67, 25, 25, KEYCLASSLITE, _T("�J"),     0301 }, // � J
    {  65,  67, 25, 25, KEYCLASSLITE, _T("�C"),     0306 }, // � C
    {  90,  67, 25, 25, KEYCLASSLITE, _T("�U"),     0314 }, // � U
    { 115,  67, 25, 25, KEYCLASSLITE, _T("�K"),     0321 }, // � K
    { 140,  67, 25, 25, KEYCLASSLITE, _T("�E"),     0327 }, // � E
    { 165,  67, 25, 25, KEYCLASSLITE, _T("�N"),     0334 }, // � N
    { 190,  67, 25, 25, KEYCLASSLITE, _T("�G"),     0341 }, // � G
    { 215,  67, 25, 25, KEYCLASSLITE, _T("� ["),    0346 }, // � [
    { 240,  67, 25, 25, KEYCLASSLITE, _T("� ]"),    0353 }, // � ]
    { 265,  67, 25, 25, KEYCLASSLITE, _T("�Z"),     0360 }, // � Z
    { 290,  67, 25, 25, KEYCLASSLITE, _T("�H"),     0366 }, // � H
    { 315,  67, 25, 25, KEYCLASSLITE, _T(": *"),    0372 }, // : *
    { 340,  67, 25, 25, KEYCLASSLITE, _T("~"),      0000 }, // ~
    { 365,  67, 25, 25, KEYCLASSLITE, _T("��"),     0275 }, // ��

    {   2,  92, 25, 25, KEYCLASSLITE, _T("��"),     0000 }, // ��
    {  27,  92, 25, 25, KEYCLASSLITE, _T("���"),    0260 }, // ���
    {  52,  92, 25, 25, KEYCLASSLITE, _T("�F"),     0302 }, // � F
    {  77,  92, 25, 25, KEYCLASSLITE, _T("�Y"),     0307 }, // � Y
    { 102,  92, 25, 25, KEYCLASSLITE, _T("�W"),     0315 }, // � W
    { 127,  92, 25, 25, KEYCLASSLITE, _T("�A"),     0322 }, // � A
    { 152,  92, 25, 25, KEYCLASSLITE, _T("�P"),     0330 }, // � P
    { 177,  92, 25, 25, KEYCLASSLITE, _T("�R"),     0335 }, // � R
    { 202,  92, 25, 25, KEYCLASSLITE, _T("�O"),     0342 }, // � O
    { 227,  92, 25, 25, KEYCLASSLITE, _T("�L"),     0347 }, // � L
    { 252,  92, 25, 25, KEYCLASSLITE, _T("�D"),     0354 }, // � D
    { 277,  92, 25, 25, KEYCLASSLITE, _T("�V"),     0362 }, // � V
    { 302,  92, 25, 25, KEYCLASSLITE, _T("� \\"),   0373 }, // � backslash
    { 327,  92, 25, 25, KEYCLASSLITE, _T(". >"),    0367 }, // . >
    { 352,  92, 25, 25, KEYCLASSLITE, _T("�"),      0304 }, // �

    {  15, 117, 25, 25, KEYCLASSLITE, _T("��"),     0000 }, // Shift
    {  40, 117, 25, 25, KEYCLASSLITE, _T("�/�"),    0262 }, // ��� / ���
    {  65, 117, 25, 25, KEYCLASSLITE, _T("�Q"),     0303 }, // � Q
    {  90, 117, 25, 25, KEYCLASSLITE, _T("�\u00ac"), 0310 }, // � ^
    { 115, 117, 25, 25, KEYCLASSLITE, _T("�S"),     0316 }, // � S
    { 140, 117, 25, 25, KEYCLASSLITE, _T("�M"),     0323 }, // � M
    { 165, 117, 25, 25, KEYCLASSLITE, _T("�I"),     0331 }, // � I
    { 190, 117, 25, 25, KEYCLASSLITE, _T("�T"),     0336 }, // � T
    { 215, 117, 25, 25, KEYCLASSLITE, _T("�X"),     0343 }, // � X
    { 240, 117, 25, 25, KEYCLASSLITE, _T("�B"),     0350 }, // � B
    { 265, 117, 25, 25, KEYCLASSLITE, _T("�@"),     0355 }, // � @
    { 290, 117, 25, 25, KEYCLASSLITE, _T(", <"),    0363 }, // ,
    { 315, 117, 25, 25, KEYCLASSLITE, _T("/ ?"),    0312 }, // /
    { 340, 117, 25, 25, KEYCLASSLITE, _T("_"),      0361 }, // _
    { 365, 117, 25, 25, KEYCLASSLITE, _T("��"),     0000 }, // Shift

    {  65, 142, 25, 25, KEYCLASSLITE, _T("���"),    0261 }, // ���
    {  90, 142, 225, 25, KEYCLASSLITE, NULL,        0324 }, // Space

    { 427,  42, 25, 25, KEYCLASSGRAY, _T("��"),     0212 }, // ��
    { 452,  42, 25, 25, KEYCLASSGRAY, _T("���"),    0213 }, // ���
    { 477,  42, 25, 25, KEYCLASSGRAY, _T("��"),     0214 }, // ����
    { 427,  67, 25, 25, KEYCLASSGRAY, _T("���"),    0215 }, // ����
    { 452,  67, 25, 25, KEYCLASSGRAY, _T("���"),    0216 }, // ���� ����
    { 477,  67, 25, 25, KEYCLASSGRAY, _T("���"),    0217 }, // ���� ����
    { 452,  92, 25, 25, KEYCLASSGRAY, _T("\u2191"), 0252 }, // Up
    { 427, 117, 25, 25, KEYCLASSGRAY, _T("\u2190"), 0247 }, // Left
    { 452, 117, 25, 25, KEYCLASSGRAY, _T("\u2193"), 0251 }, // Down
    { 477, 117, 25, 25, KEYCLASSGRAY, _T("\u2192"), 0250 }, // Right

    { 527,  42, 25, 25, KEYCLASSLITE, _T("��1"),    0241 }, // NumPad ��1
    { 552,  42, 25, 25, KEYCLASSLITE, _T("��2"),    0242 }, // NumPad ��2
    { 577,  42, 25, 25, KEYCLASSLITE, _T("��3"),    0243 }, // NumPad ��3
    { 602,  42, 25, 25, KEYCLASSLITE, _T("��4"),    0244 }, // NumPad ��4
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
    { 527, 142, 50, 25, KEYCLASSLITE, _T("0"),      0105 }, // NumPad 0 ���
    { 577, 142, 50, 25, KEYCLASSLITE, _T("����"),   0225 }, // NumPad ����
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
