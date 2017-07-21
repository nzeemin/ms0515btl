/*  This file is part of MS0515BTL.
    MS0515BTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    MS0515BTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
MS0515BTL. If not, see <http://www.gnu.org/licenses/>. */

// Emulator.cpp

#include "stdafx.h"
#include <stdio.h>
#include <Share.h>
#include "Main.h"
#include "Emulator.h"
#include "Views.h"
#include "emubase\Emubase.h"
#include "SoundGen.h"


//////////////////////////////////////////////////////////////////////


CMotherboard* g_pBoard = NULL;
int g_nEmulatorConfiguration;  // Current configuration
bool g_okEmulatorRunning = false;

uint16_t m_wEmulatorCPUBreakpoint = 0177777;

bool m_okEmulatorSound = false;
bool m_okEmulatorCovox = false;

bool m_okEmulatorParallel = false;
bool m_okEmulatorSerial = false;
HANDLE m_hEmulatorComPort = INVALID_HANDLE_VALUE;

FILE* m_fpEmulatorParallelOut = NULL;

long m_nFrameCount = 0;
uint32_t m_dwTickCount = 0;
long m_nUptimeFrameCount = 0;
uint32_t m_dwTotalFrameCount = 0;

BYTE* g_pEmulatorRam;  // RAM values - for change tracking
BYTE* g_pEmulatorChangedRam;  // RAM change flags
uint16_t g_wEmulatorCpuPC = 0177777;      // Current PC value
uint16_t g_wEmulatorPrevCpuPC = 0177777;  // Previous PC value


void CALLBACK Emulator_SoundGenCallback(unsigned short L, unsigned short R);

//////////////////////////////////////////////////////////////////////
//�������� ������� �������������� ������
// Input:
//   pVideoBuffer   �������� ������, ���� ������ ��
//   pPalette       �������
//   pImageBits     ���������, 32-������ ����, ������ ��� ������ ������� ����
//   hires          ������� ������ �������� ���������� 640x200
//   border         ����� ����� ������� 0..7
//   blink          ���� ��������
typedef void (CALLBACK* PREPARE_SCREEN_CALLBACK)(
    const BYTE* pVideoBuffer, const uint32_t* pPalette, void* pImageBits,
    bool hires, uint8_t border, bool blink);

void CALLBACK Emulator_PrepareScreen640x200(const BYTE*, const uint32_t*, void*, bool, uint8_t, bool);
void CALLBACK Emulator_PrepareScreen360x220(const BYTE*, const uint32_t*, void*, bool, uint8_t, bool);
void CALLBACK Emulator_PrepareScreen720x440(const BYTE*, const uint32_t*, void*, bool, uint8_t, bool);
void CALLBACK Emulator_PrepareScreen880x660(const BYTE*, const uint32_t*, void*, bool, uint8_t, bool);
void CALLBACK Emulator_PrepareScreen1080x660(const BYTE*, const uint32_t*, void*, bool, uint8_t, bool);

struct ScreenModeStruct
{
    int width;
    int height;
    PREPARE_SCREEN_CALLBACK callback;
}
static ScreenModeReference[] =
{
    // wid  hei  callback                               size   scaleX  scaleY  notes
    {  640, 200, Emulator_PrepareScreen640x200  },  //                   1      Debug mode
    {  360, 220, Emulator_PrepareScreen360x220  },  // 320x200   0.5     1
    {  720, 440, Emulator_PrepareScreen720x440  },  // 640x400   1       2
    {  880, 660, Emulator_PrepareScreen880x660  },  // 800x600   1.25    3
    { 1080, 660, Emulator_PrepareScreen1080x660 },  // 960x600   1.5     3      Interlaced
};

const uint32_t Emulator_Palette[24] =
{
    0x000000, 0x0000FF, 0xFF0000, 0xFF00FF, 0x00FF00, 0x00FFFF, 0xFFFF00, 0xFFFFFF,
    0x000000, 0x00007F, 0x7F0000, 0x7F007F, 0x007F00, 0x007F7F, 0x7F7F00, 0x7F7F7F,
    0x101010, 0x0000EF, 0xEF0000, 0xEF00EF, 0x00EF00, 0x00EFEF, 0xEFEF00, 0xEFEFEF,  // Border palette
};


//////////////////////////////////////////////////////////////////////


const LPCTSTR FILENAME_ROM_MS0515 = _T("ms0515.rom");


//////////////////////////////////////////////////////////////////////

bool Emulator_LoadRomFile(LPCTSTR strFileName, BYTE* buffer, uint32_t fileOffset, uint32_t bytesToRead)
{
    FILE* fpRomFile = ::_tfsopen(strFileName, _T("rb"), _SH_DENYWR);
    if (fpRomFile == NULL)
        return false;

    ::memset(buffer, 0, bytesToRead);

    if (fileOffset > 0)
    {
        ::fseek(fpRomFile, fileOffset, SEEK_SET);
    }

    uint32_t dwBytesRead = ::fread(buffer, 1, bytesToRead, fpRomFile);
    if (dwBytesRead != bytesToRead)
    {
        ::fclose(fpRomFile);
        return false;
    }

    ::fclose(fpRomFile);

    return true;
}

bool Emulator_Init()
{
    ASSERT(g_pBoard == NULL);

    CProcessor::Init();

    g_pBoard = new CMotherboard();

    // Allocate memory for old RAM values
    g_pEmulatorRam = (BYTE*) ::malloc(65536);  ::memset(g_pEmulatorRam, 0, 65536);
    g_pEmulatorChangedRam = (BYTE*) ::malloc(65536);  ::memset(g_pEmulatorChangedRam, 0, 65536);

    g_pBoard->Reset();

    if (m_okEmulatorSound)
    {
        SoundGen_Initialize(Settings_GetSoundVolume());
        g_pBoard->SetSoundGenCallback(Emulator_SoundGenCallback);
    }

    // Load ROM file
    BYTE buffer[16384];
    if (!Emulator_LoadRomFile(FILENAME_ROM_MS0515, buffer, 0, 16384))
    {
        AlertWarning(_T("Failed to load the ROM."));
        return false;
    }
    g_pBoard->LoadROM(buffer);

    return true;
}

void Emulator_Done()
{
    ASSERT(g_pBoard != NULL);

    CProcessor::Done();

    //g_pBoard->SetSoundGenCallback(NULL);
    //SoundGen_Finalize();

    g_pBoard->SetSerialCallbacks(NULL, NULL);
    if (m_hEmulatorComPort != INVALID_HANDLE_VALUE)
    {
        ::CloseHandle(m_hEmulatorComPort);
        m_hEmulatorComPort = INVALID_HANDLE_VALUE;
    }

    delete g_pBoard;
    g_pBoard = NULL;

    // Free memory used for old RAM values
    ::free(g_pEmulatorRam);
    ::free(g_pEmulatorChangedRam);
}

void Emulator_Start()
{
    g_okEmulatorRunning = true;

    // Set title bar text
    SetWindowText(g_hwnd, _T("MS0515 Back to Life [run]"));
    MainWindow_UpdateMenu();

    m_nFrameCount = 0;
    m_dwTickCount = GetTickCount();
}
void Emulator_Stop()
{
    g_okEmulatorRunning = false;
    m_wEmulatorCPUBreakpoint = 0177777;

    if (m_fpEmulatorParallelOut != NULL)
        ::fflush(m_fpEmulatorParallelOut);

    // Reset title bar message
    SetWindowText(g_hwnd, _T("MS0515 Back to Life [stop]"));
    MainWindow_UpdateMenu();
    // Reset FPS indicator
    MainWindow_SetStatusbarText(StatusbarPartFPS, _T(""));

    MainWindow_UpdateAllViews();
}

void Emulator_Reset()
{
    ASSERT(g_pBoard != NULL);

    g_pBoard->Reset();

    m_nUptimeFrameCount = 0;
    m_dwTotalFrameCount = 0;

    MainWindow_UpdateAllViews();
}

void Emulator_SetCPUBreakpoint(uint16_t address)
{
    m_wEmulatorCPUBreakpoint = address;
}

bool Emulator_IsBreakpoint()
{
    uint16_t wCPUAddr = g_pBoard->GetCPU()->GetPC();
    if (wCPUAddr == m_wEmulatorCPUBreakpoint)
        return true;
    return false;
}

void Emulator_SetSound(bool soundOnOff)
{
    if (m_okEmulatorSound != soundOnOff)
    {
        if (soundOnOff)
        {
            SoundGen_Initialize(Settings_GetSoundVolume());
            g_pBoard->SetSoundGenCallback(Emulator_SoundGenCallback);
        }
        else
        {
            g_pBoard->SetSoundGenCallback(NULL);
            SoundGen_Finalize();
        }
    }

    m_okEmulatorSound = soundOnOff;
}

bool CALLBACK Emulator_SerialIn_Callback(BYTE* pByte)
{
    DWORD dwBytesRead;
    BOOL result = ::ReadFile(m_hEmulatorComPort, pByte, 1, &dwBytesRead, NULL);

    return result && (dwBytesRead == 1);
}

bool CALLBACK Emulator_SerialOut_Callback(BYTE byte)
{
    DWORD dwBytesWritten;
    ::WriteFile(m_hEmulatorComPort, &byte, 1, &dwBytesWritten, NULL);

    return (dwBytesWritten == 1);
}

bool Emulator_SetSerial(bool serialOnOff, LPCTSTR serialPort)
{
    if (m_okEmulatorSerial != serialOnOff)
    {
        if (serialOnOff)
        {
            // Prepare port name
            TCHAR port[15];
            wsprintf(port, _T("\\\\.\\%s"), serialPort);

            // Open port
            m_hEmulatorComPort = ::CreateFile(port, GENERIC_READ | GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
            if (m_hEmulatorComPort == INVALID_HANDLE_VALUE)
            {
                uint32_t dwError = ::GetLastError();
                AlertWarningFormat(_T("Failed to open COM port (0x%08lx)."), dwError);
                return false;
            }

            // Set port settings
            DCB dcb;
            Settings_GetSerialConfig(&dcb);
            if (!::SetCommState(m_hEmulatorComPort, &dcb))
            {
                uint32_t dwError = ::GetLastError();
                ::CloseHandle(m_hEmulatorComPort);
                m_hEmulatorComPort = INVALID_HANDLE_VALUE;
                AlertWarningFormat(_T("Failed to configure the COM port (0x%08lx)."), dwError);
                return false;
            }

            // Set timeouts: ReadIntervalTimeout value of MAXDWORD, combined with zero values for both the ReadTotalTimeoutConstant
            // and ReadTotalTimeoutMultiplier members, specifies that the read operation is to return immediately with the bytes
            // that have already been received, even if no bytes have been received.
            COMMTIMEOUTS timeouts;
            ::memset(&timeouts, 0, sizeof(timeouts));
            timeouts.ReadIntervalTimeout = MAXDWORD;
            timeouts.WriteTotalTimeoutConstant = 100;
            if (!::SetCommTimeouts(m_hEmulatorComPort, &timeouts))
            {
                uint32_t dwError = ::GetLastError();
                ::CloseHandle(m_hEmulatorComPort);
                m_hEmulatorComPort = INVALID_HANDLE_VALUE;
                AlertWarningFormat(_T("Failed to set the COM port timeouts (0x%08lx)."), dwError);
                return false;
            }

            // Clear port input buffer
            ::PurgeComm(m_hEmulatorComPort, PURGE_RXABORT | PURGE_RXCLEAR);

            // Set callbacks
            g_pBoard->SetSerialCallbacks(Emulator_SerialIn_Callback, Emulator_SerialOut_Callback);
        }
        else
        {
            g_pBoard->SetSerialCallbacks(NULL, NULL);  // Reset callbacks

            // Close port
            if (m_hEmulatorComPort != INVALID_HANDLE_VALUE)
            {
                ::CloseHandle(m_hEmulatorComPort);
                m_hEmulatorComPort = INVALID_HANDLE_VALUE;
            }
        }
    }

    m_okEmulatorSerial = serialOnOff;

    return true;
}

bool CALLBACK Emulator_ParallelOut_Callback(BYTE byte)
{
    if (m_fpEmulatorParallelOut != NULL)
    {
        ::fwrite(&byte, 1, 1, m_fpEmulatorParallelOut);
    }

    ////DEBUG
    //TCHAR buffer[32];
    //_snwprintf_s(buffer, 32, _T("Printer: <%02x>\r\n"), byte);
    //ConsoleView_Print(buffer);

    return true;
}

void Emulator_SetParallel(bool parallelOnOff)
{
    if (m_okEmulatorParallel == parallelOnOff)
        return;

    if (!parallelOnOff)
    {
        g_pBoard->SetParallelOutCallback(NULL);
        if (m_fpEmulatorParallelOut != NULL)
            ::fclose(m_fpEmulatorParallelOut);
    }
    else
    {
        g_pBoard->SetParallelOutCallback(Emulator_ParallelOut_Callback);
        m_fpEmulatorParallelOut = ::_tfopen(_T("printer.log"), _T("wb"));
    }

    m_okEmulatorParallel = parallelOnOff;
}

int Emulator_SystemFrame()
{
    g_pBoard->SetCPUBreakpoint(m_wEmulatorCPUBreakpoint);

    ScreenView_ScanKeyboard();
    ScreenView_ProcessKeyboard();

    if (!g_pBoard->SystemFrame())
        return 0;

    // Calculate frames per second
    m_nFrameCount++;
    uint32_t dwCurrentTicks = GetTickCount();
    long nTicksElapsed = dwCurrentTicks - m_dwTickCount;
    if (nTicksElapsed >= 1200)
    {
        double dFramesPerSecond = m_nFrameCount * 1000.0 / nTicksElapsed;
        double dSpeed = dFramesPerSecond / 25.0 * 100;
        TCHAR buffer[16];
        swprintf_s(buffer, 16, _T("%03.f%%"), dSpeed);
        MainWindow_SetStatusbarText(StatusbarPartFPS, buffer);

        bool floppyEngine = g_pBoard->IsFloppyEngineOn();
        MainWindow_SetStatusbarText(StatusbarPartFloppyEngine, floppyEngine ? _T("Motor") : NULL);

        m_nFrameCount = 0;
        m_dwTickCount = dwCurrentTicks;
    }

    // Calculate emulator uptime (25 frames per second)
    m_dwTotalFrameCount++;
    m_nUptimeFrameCount++;
    if (m_nUptimeFrameCount >= 25)
    {
        m_nUptimeFrameCount = 0;

        uint32_t dwEmulatorUptime = m_dwTotalFrameCount / 25;
        int seconds = (int) (dwEmulatorUptime % 60);
        int minutes = (int) (dwEmulatorUptime / 60 % 60);
        int hours   = (int) (dwEmulatorUptime / 3600 % 60);

        TCHAR buffer[20];
        swprintf_s(buffer, 20, _T("Uptime: %02d:%02d:%02d"), hours, minutes, seconds);
        MainWindow_SetStatusbarText(StatusbarPartUptime, buffer);
    }

    return 1;
}

void CALLBACK Emulator_SoundGenCallback(unsigned short L, unsigned short R)
{
    SoundGen_FeedDAC(L, R);
}

// Update cached values after Run or Step
void Emulator_OnUpdate()
{
    // Update stored PC value
    g_wEmulatorPrevCpuPC = g_wEmulatorCpuPC;
    g_wEmulatorCpuPC = g_pBoard->GetCPU()->GetPC();

    // Update memory change flags
    {
        BYTE* pOld = g_pEmulatorRam;
        BYTE* pChanged = g_pEmulatorChangedRam;
        uint16_t addr = 0;
        do
        {
            BYTE newvalue = g_pBoard->GetLORAMByte(addr);  //TODO
            BYTE oldvalue = *pOld;
            *pChanged = (newvalue != oldvalue) ? 255 : 0;
            *pOld = newvalue;
            addr++;
            pOld++;  pChanged++;
        }
        while (addr < 65535);
    }
}

// Get RAM change flag
//   addrtype - address mode - see ADDRTYPE_XXX constants
uint16_t Emulator_GetChangeRamStatus(uint16_t address)
{
    return *((uint16_t*)(g_pEmulatorChangedRam + address));
}

void Emulator_GetScreenSize(int scrmode, int* pwid, int* phei)
{
    if (scrmode < 0 || scrmode >= sizeof(ScreenModeReference) / sizeof(ScreenModeStruct))
        return;
    ScreenModeStruct* pinfo = ScreenModeReference + scrmode;
    *pwid = pinfo->width;
    *phei = pinfo->height;
}

void Emulator_PrepareScreenRGB32(void* pImageBits, int screenMode)
{
    if (pImageBits == NULL) return;

    const BYTE* pVideoBuffer = g_pBoard->GetVideoBuffer();
    ASSERT(pVideoBuffer != NULL);

    // Render to bitmap
    bool hires = g_pBoard->GetPortView(0177604) & 010;
    uint8_t border = g_pBoard->GetPortView(0177604) & 7;
    bool blink = (m_dwTotalFrameCount % 75) > 37;
    PREPARE_SCREEN_CALLBACK callback = ScreenModeReference[screenMode].callback;
    callback(pVideoBuffer, Emulator_Palette, pImageBits, hires, border, blink);
}

const uint32_t * Emulator_GetPalette()
{
    return Emulator_Palette;
}

// 1/2 part of "a" plus 1/2 part of "b"
#define AVERAGERGB(a, b)  ( (((a) & 0xfefefeffUL) + ((b) & 0xfefefeffUL)) >> 1 )

// 1/4 part of "a" plus 3/4 parts of "b"
#define AVERAGERGB13(a, b)  ( ((a) == (b)) ? a : (((a) & 0xfcfcfcffUL) >> 2) + ((b) - (((b) & 0xfcfcfcffUL) >> 2)) )

void CALLBACK Emulator_PrepareScreen640x200(
    const BYTE* pVideoBuffer, const uint32_t* palette, void* pImageBits, bool hires, uint8_t border, bool blink)
{
    if (!hires)
    {
        uint32_t colorborder = palette[(border & 7) + 16];
        for (int y = 0; y < 200; y++)
        {
            const uint16_t* pVideo = (uint16_t*)(pVideoBuffer + y * 320 / 4);
            uint32_t* pBits = (uint32_t*)pImageBits + (200 - 1 - y) * 640;
            for (int i = 0; i < 160; i++)  // Left part of line
                *pBits++ = colorborder;
            for (int x = 0; x < 320 / 8; x++)
            {
                uint16_t value = *pVideo++;
                uint32_t colorpaper = palette[(value >> 11) & 7];
                uint32_t colorink = palette[(value >> 8) & 7];
                if ((value & 0x8000) && blink)
                {
                    uint32_t temp = colorink;  colorink = colorpaper;  colorpaper = temp;
                }
                uint16_t mask = 0x80;
                for (int f = 0; f < 8; f++)
                {
                    *pBits++ = (value & mask) ? colorink : colorpaper;
                    mask = mask >> 1;
                }
            }
            for (int i = 0; i < 160; i++)  // Right part of line
                *pBits++ = colorborder;
        }
    }
    else  // hires
    {
        uint32_t colorpaper = palette[border & 7];
        uint32_t colorink = palette[(border & 7) ^ 7];
        for (int y = 0; y < 200; y++)
        {
            const uint8_t* pVideo = (uint8_t*)(pVideoBuffer + y * 640 / 8);
            uint32_t* pBits = (uint32_t*)pImageBits + (200 - 1 - y) * 640;
            for (int x = 0; x < 640; x += 8)
            {
                uint8_t value = *pVideo++;
                uint8_t mask = 0x80;
                for (int f = 0; f < 8; f++)
                {
                    *pBits++ = (value & mask) ? colorink : colorpaper;
                    mask = mask >> 1;
                }
            }
        }
    }
}

// 320x200 plus 15 pix border at left/right, plus 10 pix border at top/bottom
void CALLBACK Emulator_PrepareScreen360x220(
    const BYTE* pVideoBuffer, const uint32_t* palette, void* pImageBits, bool hires, uint8_t border, bool blink)
{
    uint32_t colorborder = palette[(border & 7) + 16];
    for (int y = 0; y < 220; y++)
    {
        uint32_t* pBits = (uint32_t*)pImageBits + (220 - 1 - y) * 360;
        if (y < 10 || y >= 210)  // Border at the top/bottom
        {
            for (int i = 0; i < 360; i++)
                *pBits++ = colorborder;
            continue;
        }
        for (int i = 0; i < 20; i++)  // Left part of line
            *pBits++ = colorborder;
        if (!hires)
        {
            const uint16_t* pVideo = (uint16_t*)(pVideoBuffer + (y - 10) * 320 / 4);
            for (int x = 0; x < 320 / 8; x++)
            {
                uint16_t value = *pVideo++;
                uint32_t colorpaper = palette[(value >> 11) & 7];
                uint32_t colorink = palette[(value >> 8) & 7];
                if ((value & 0x8000) && blink)
                {
                    uint32_t temp = colorink;  colorink = colorpaper;  colorpaper = temp;
                }
                uint16_t mask = 0x80;
                for (int f = 0; f < 8; f++)
                {
                    *pBits++ = (value & mask) ? colorink : colorpaper;
                    mask = mask >> 1;
                }
            }
        }
        else  // hires
        {
            uint32_t colorborder = palette[(border & 7) + 16];
            uint32_t colorpaper = palette[border & 7];
            uint32_t colorink = palette[(border & 7) ^ 7];
            const uint8_t* pVideo = (uint8_t*)(pVideoBuffer + (y - 10) * 640 / 8);
            for (int x = 0; x < 320; x += 4)
            {
                uint8_t value = *pVideo++;
                uint8_t mask = 0x80;
                for (int f = 0; f < 8; f += 2)
                {
                    uint32_t color1 = (value & mask) ? colorink : colorpaper;
                    mask = mask >> 1;
                    uint32_t color2 = (value & mask) ? colorink : colorpaper;
                    mask = mask >> 1;
                    *pBits++ = AVERAGERGB(color1, color2);
                }
            }
        }
        for (int i = 0; i < 20; i++)  // Right part of line
            *pBits++ = colorborder;
    }
}

// 640x400, plus 40 pix border at left/right, plus 20 pix border at top/bottom
void CALLBACK Emulator_PrepareScreen720x440(
    const BYTE* pVideoBuffer, const uint32_t* palette, void* pImageBits, bool hires, uint8_t border, bool blink)
{
    uint32_t colorborder = palette[(border & 7) + 16];
    for (int y = 0; y < 220; y++)
    {
        uint32_t* pBits1 = (uint32_t*)pImageBits + (440 - 1 - y * 2) * 720;
        uint32_t* pBits2 = (uint32_t*)pImageBits + (440 - 2 - y * 2) * 720;
        if (y < 10 || y >= 210)  // Border at the top/bottom
        {
            for (int i = 0; i < 720; i++)
                *pBits1++ = *pBits2++ = colorborder;
            continue;
        }
        for (int i = 0; i < 40; i++)  // Border at the left
            *pBits1++ = *pBits2++ = colorborder;
        if (!hires)
        {
            const uint16_t* pVideo = (uint16_t*)(pVideoBuffer + (y - 10) * 320 / 4);
            for (int x = 0; x < 320 / 8; x++)
            {
                uint16_t value = *pVideo++;
                uint32_t colorpaper = palette[(value >> 11) & 7];
                uint32_t colorink = palette[(value >> 8) & 7];
                if ((value & 0x8000) && blink)
                {
                    uint32_t temp = colorink;  colorink = colorpaper;  colorpaper = temp;
                }
                uint16_t mask = 0x80;
                for (int f = 0; f < 8; f++)
                {
                    *pBits1++ = *pBits2++ = (value & mask) ? colorink : colorpaper;
                    *pBits1++ = *pBits2++ = (value & mask) ? colorink : colorpaper;
                    mask = mask >> 1;
                }
            }
        }
        else  // hires
        {
            uint32_t colorpaper = palette[border & 7];
            uint32_t colorink = palette[(border & 7) ^ 7];
            const uint8_t* pVideo = (uint8_t*)(pVideoBuffer + (y - 10) * 640 / 8);
            for (int x = 0; x < 640; x += 8)
            {
                uint8_t value = *pVideo++;
                uint8_t mask = 0x80;
                for (int f = 0; f < 8; f++)
                {
                    *pBits1++ = *pBits2++ = (value & mask) ? colorink : colorpaper;
                    mask = mask >> 1;
                }
            }
        }
        for (int i = 0; i < 40; i++)  // Border at the right
            *pBits1++ = *pBits2++ = colorborder;
    }
}

// 800x600 plus 40 pix border for left/right sides, 30 pix border for top/bottom
void CALLBACK Emulator_PrepareScreen880x660(
    const BYTE* pVideoBuffer, const uint32_t* palette, void* pImageBits, bool hires, uint8_t border, bool blink)
{
    uint32_t colorborder = palette[(border & 7) + 16];
    for (int y = 0; y < 220; y++)
    {
        uint32_t* pBits1 = (uint32_t*)pImageBits + (660 - 1 - y * 3) * 880;
        uint32_t* pBits2 = (uint32_t*)pImageBits + (660 - 2 - y * 3) * 880;
        uint32_t* pBits3 = (uint32_t*)pImageBits + (660 - 3 - y * 3) * 880;
        if (y < 10 || y >= 210)  // Border at the top/bottom
        {
            for (int i = 0; i < 880; i++)
                *pBits1++ = *pBits2++ = *pBits3++ = colorborder;
            continue;
        }

        for (int i = 0; i < 40; i++)  // Border at the left
            *pBits1++ = *pBits2++ = *pBits3++ = colorborder;

        if (!hires)
        {
            const uint16_t* pVideo = (uint16_t*)(pVideoBuffer + (y - 10) * 320 / 4);
            for (int x = 0; x < 320 / 8; x++)
            {
                uint16_t value = *pVideo++;
                uint32_t colorpaper = palette[(value >> 11) & 7];
                uint32_t colorink = palette[(value >> 8) & 7];
                if ((value & 0x8000) && blink)
                {
                    uint32_t temp = colorink;  colorink = colorpaper;  colorpaper = temp;
                }
                uint16_t mask = 0x80;
                for (int f = 0; f < 4; f++)
                {
                    uint32_t color1 = (value & mask) ? colorink : colorpaper;
                    mask = mask >> 1;
                    uint32_t color2 = (value & mask) ? colorink : colorpaper;
                    mask = mask >> 1;
                    *pBits1++ = *pBits2++ = *pBits3++ = color1;
                    *pBits1++ = *pBits2++ = *pBits3++ = color1;
                    *pBits1++ = *pBits2++ = *pBits3++ = AVERAGERGB(color1, color2);
                    *pBits1++ = *pBits2++ = *pBits3++ = color2;
                    *pBits1++ = *pBits2++ = *pBits3++ = color2;
                }
            }
        }
        else  // hires
        {
            const uint8_t* pVideo = (uint8_t*)(pVideoBuffer + (y - 10) * 640 / 8);
            uint32_t colorpaper = palette[border & 7];
            uint32_t colorink = palette[(border & 7) ^ 7];
            for (int x = 0; x < 640; x += 8)
            {
                uint8_t value = *pVideo++;
                uint16_t mask = 0x80;
                for (int f = 0; f < 2; f++)
                {
                    uint32_t color1 = (value & mask) ? colorink : colorpaper;
                    mask = mask >> 1;
                    uint32_t color2 = (value & mask) ? colorink : colorpaper;
                    mask = mask >> 1;
                    uint32_t color3 = (value & mask) ? colorink : colorpaper;
                    mask = mask >> 1;
                    uint32_t color4 = (value & mask) ? colorink : colorpaper;
                    mask = mask >> 1;

                    *pBits1++ = *pBits2++ = *pBits3++ = color1;
                    *pBits1++ = *pBits2++ = *pBits3++ = AVERAGERGB13(color1, color2);
                    *pBits1++ = *pBits2++ = *pBits3++ = AVERAGERGB(color2, color3);
                    *pBits1++ = *pBits2++ = *pBits3++ = AVERAGERGB13(color4, color3);
                    *pBits1++ = *pBits2++ = *pBits3++ = color4;
                }
            }
        }

        for (int i = 0; i < 40; i++)  // Border at the right
            *pBits1++ = *pBits2++ = *pBits3++ = colorborder;
    }
}

// 960x600 plus 60 pix border for left/right sides, 30 pix border for top/bottom
void CALLBACK Emulator_PrepareScreen1080x660(
    const BYTE* pVideoBuffer, const uint32_t* palette, void* pImageBits, bool hires, uint8_t border, bool blink)
{
    uint32_t colorborder = palette[(border & 7) + 16];
    for (int y = 0; y < 220; y++)
    {
        uint32_t* pBits1 = (uint32_t*)pImageBits + (660 - 1 - y * 3) * 1080;
        uint32_t* pBits2 = (uint32_t*)pImageBits + (660 - 2 - y * 3) * 1080;
        //uint32_t* pBits3 = (uint32_t*)pImageBits + (660 - 3 - y * 3) * 1080;
        //for (int i = 0; i < 1080; i++)
        //    *pBits3++ = 0;
        if (y < 10 || y >= 210)  // Border at the top/bottom
        {
            for (int i = 0; i < 1080; i++)
                *pBits1++ = *pBits2++ = colorborder;
            continue;
        }
        for (int i = 0; i < 60; i++)  // Border at the left
            *pBits1++ = *pBits2++ = colorborder;
        if (!hires)
        {
            const uint16_t* pVideo = (uint16_t*)(pVideoBuffer + (y - 10) * 320 / 4);
            for (int x = 0; x < 320 / 8; x++)
            {
                uint16_t value = *pVideo++;
                uint32_t colorpaper = palette[(value >> 11) & 7];
                uint32_t colorink = palette[(value >> 8) & 7];
                if ((value & 0x8000) && blink)
                {
                    uint32_t temp = colorink;  colorink = colorpaper;  colorpaper = temp;
                }
                uint16_t mask = 0x80;
                for (int f = 0; f < 8; f++)
                {
                    uint32_t color = (value & mask) ? colorink : colorpaper;
                    *pBits1++ = *pBits2++ = color;
                    *pBits1++ = *pBits2++ = color;
                    *pBits1++ = *pBits2++ = color;
                    mask = mask >> 1;
                }
            }
        }
        else  // hires
        {
            const uint8_t* pVideo = (uint8_t*)(pVideoBuffer + (y - 10) * 640 / 8);
            uint32_t colorpaper = palette[border & 7];
            uint32_t colorink = palette[(border & 7) ^ 7];
            for (int x = 0; x < 640; x += 8)
            {
                uint8_t value = *pVideo++;
                uint8_t mask1 = 0x80;
                uint8_t mask2 = 0x40;
                for (int f = 0; f < 4; f++)
                {
                    uint32_t color1 = (value & mask1) ? colorink : colorpaper;
                    uint32_t color2 = (value & mask2) ? colorink : colorpaper;
                    *pBits1++ = *pBits2++ = color1;
                    *pBits1++ = *pBits2++ = AVERAGERGB(color1, color2);
                    *pBits1++ = *pBits2++ = color2;
                    mask1 = mask1 >> 2;
                    mask2 = mask2 >> 2;
                }
            }
        }
        for (int i = 0; i < 60; i++)  // Border at the right
            *pBits1++ = *pBits2++ = colorborder;
    }
}

//////////////////////////////////////////////////////////////////////
//
// Emulator image format - see CMotherboard::SaveToImage()
// Image header format (32 bytes):
//   4 bytes        MS0515IMAGE_HEADER1
//   4 bytes        MS0515IMAGE_HEADER2
//   4 bytes        MS0515IMAGE_VERSION
//   4 bytes        MS0515IMAGE_SIZE
//   4 bytes        MS0515 uptime
//   12 bytes       Not used

bool Emulator_SaveImage(LPCTSTR sFilePath)
{
    // Create file
    FILE* fpFile = ::_tfsopen(sFilePath, _T("w+b"), _SH_DENYWR);
    if (fpFile == NULL)
        return false;

    // Allocate memory
    BYTE* pImage = (BYTE*) ::malloc(MS0515IMAGE_SIZE);
    if (pImage == NULL)
    {
        ::fclose(fpFile);
        return false;
    }
    memset(pImage, 0, MS0515IMAGE_SIZE);
    // Prepare header
    uint32_t* pHeader = (uint32_t*) pImage;
    *pHeader++ = MS0515IMAGE_HEADER1;
    *pHeader++ = MS0515IMAGE_HEADER2;
    *pHeader++ = MS0515IMAGE_VERSION;
    *pHeader++ = MS0515IMAGE_SIZE;
    // Store emulator state to the image
    g_pBoard->SaveToImage(pImage);
    *(uint32_t*)(pImage + 16) = m_dwTotalFrameCount;

    // Save image to the file
    size_t dwBytesWritten = ::fwrite(pImage, 1, MS0515IMAGE_SIZE, fpFile);
    ::free(pImage);
    ::fclose(fpFile);
    if (dwBytesWritten != MS0515IMAGE_SIZE)
        return false;

    return true;
}

bool Emulator_LoadImage(LPCTSTR sFilePath)
{
    Emulator_Stop();
    Emulator_Reset();

    // Open file
    FILE* fpFile = ::_tfsopen(sFilePath, _T("rb"), _SH_DENYWR);
    if (fpFile == NULL)
        return false;

    // Read header
    uint32_t bufHeader[MS0515IMAGE_HEADER_SIZE / sizeof(uint32_t)];
    uint32_t dwBytesRead = ::fread(bufHeader, 1, MS0515IMAGE_HEADER_SIZE, fpFile);
    if (dwBytesRead != MS0515IMAGE_HEADER_SIZE)
    {
        ::fclose(fpFile);
        return false;
    }

    //TODO: Check version and size

    // Allocate memory
    BYTE* pImage = (BYTE*) ::malloc(MS0515IMAGE_SIZE);
    if (pImage == NULL)
    {
        ::fclose(fpFile);
        return false;
    }

    // Read image
    ::fseek(fpFile, 0, SEEK_SET);
    dwBytesRead = ::fread(pImage, 1, MS0515IMAGE_SIZE, fpFile);
    if (dwBytesRead != MS0515IMAGE_SIZE)
    {
        ::free(pImage);
        ::fclose(fpFile);
        return false;
    }

    // Restore emulator state from the image
    g_pBoard->LoadFromImage(pImage);

    m_dwTotalFrameCount = (*(uint32_t*)(pImage + 16));
    g_wEmulatorCpuPC = g_pBoard->GetCPU()->GetPC();

    // Free memory, close file
    ::free(pImage);
    ::fclose(fpFile);

    return true;
}


//////////////////////////////////////////////////////////////////////
