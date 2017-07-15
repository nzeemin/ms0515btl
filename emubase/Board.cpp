/*  This file is part of MS0515BTL.
    MS0515BTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    MS0515BTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
MS0515BTL. If not, see <http://www.gnu.org/licenses/>. */

// Board.cpp
//

#include "stdafx.h"
#include "Emubase.h"

void TraceInstruction(CProcessor* pProc, CMotherboard* pBoard, uint16_t address, DWORD dwTrace);


//////////////////////////////////////////////////////////////////////

CMotherboard::CMotherboard ()
{
    // Create devices
    m_pCPU = new CProcessor(this);
    m_pFloppyCtl = NULL;
    m_pKeyboard = new CKeyboard();

    m_dwTrace = TRACE_NONE;
    m_SoundGenCallback = NULL;
    m_SerialInCallback = NULL;
    m_SerialOutCallback = NULL;
    m_ParallelOutCallback = NULL;
    m_okTimer50OnOff = false;
    m_okSoundOnOff = false;
    m_Timer1 = m_Timer1div = m_Timer2 = 0;

    // Allocate memory for RAM and ROM
    m_pRAM = (uint8_t*) ::malloc(128 * 1024);  ::memset(m_pRAM, 0, 128 * 1024);
    m_pROM = (uint8_t*) ::malloc(16 * 1024);  ::memset(m_pROM, 0, 16 * 1024);

    SetConfiguration(0);  // Default configuration

    Reset();
}

CMotherboard::~CMotherboard ()
{
    // Delete devices
    delete m_pCPU;
    if (m_pFloppyCtl != NULL)
        delete m_pFloppyCtl;
    delete m_pKeyboard;

    // Free memory
    ::free(m_pRAM);
    ::free(m_pROM);
}

void CMotherboard::SetConfiguration(uint16_t conf)
{
    m_Configuration = conf;

    // Clean RAM/ROM
    ::memset(m_pRAM, 0, 128 * 1024);
    ::memset(m_pROM, 0, 16 * 1024);

    //// Pre-fill RAM with "uninitialized" values
    //uint16_t * pMemory = (uint16_t *) m_pRAM;
    //uint16_t val = 0;
    //uint8_t flag = 0;
    //for (uint32_t i = 0; i < 128 * 1024; i += 2, flag--)
    //{
    //    *pMemory = val;  pMemory++;
    //    if (flag == 192)
    //        flag = 0;
    //    else
    //        val = ~val;
    //}

    if (m_pFloppyCtl == NULL /*&& (conf & BK_COPT_FDD) != 0*/)
    {
        m_pFloppyCtl = new CFloppyController();
        m_pFloppyCtl->SetTrace(m_dwTrace & TRACE_FLOPPY);
    }
    //if (m_pFloppyCtl != NULL /*&& (conf & BK_COPT_FDD) == 0*/)
    //{
    //    delete m_pFloppyCtl;  m_pFloppyCtl = NULL;
    //}
}

void CMotherboard::SetTrace(uint32_t dwTrace)
{
    m_dwTrace = dwTrace;
    if (m_pFloppyCtl != NULL)
        m_pFloppyCtl->SetTrace(dwTrace & TRACE_FLOPPY);
}

void CMotherboard::Reset ()
{
    m_pCPU->Stop();

    // Reset ports
    m_okSoundOnOff = false;
    m_Port177400 = 0; //TODO
    m_Port177442r = 0201;
    m_Port177604 = 0; //TODO
    //TODO

    ResetDevices();

    m_pCPU->Start();
}

// Load 16 KB ROM image from the buffer
void CMotherboard::LoadROM(const uint8_t* pBuffer)
{
    ::memcpy(m_pROM, pBuffer, 16384);
}

void CMotherboard::LoadRAM(int startbank, const uint8_t* pBuffer, int length)
{
    ASSERT(pBuffer != NULL);
    ASSERT(startbank >= 0 && startbank < 15);
    int address = 8192 * startbank;
    ASSERT(address + length <= 128 * 1024);
    ::memcpy(m_pRAM + address, pBuffer, length);
}


// Floppy ////////////////////////////////////////////////////////////

bool CMotherboard::IsFloppyImageAttached(int slot) const
{
    ASSERT(slot >= 0 && slot < 4);
    if (m_pFloppyCtl == NULL)
        return false;
    return m_pFloppyCtl->IsAttached(slot);
}

bool CMotherboard::IsFloppyReadOnly(int slot) const
{
    ASSERT(slot >= 0 && slot < 4);
    if (m_pFloppyCtl == NULL)
        return false;
    return m_pFloppyCtl->IsReadOnly(slot);
}

bool CMotherboard::AttachFloppyImage(int slot, LPCTSTR sFileName)
{
    ASSERT(slot >= 0 && slot < 4);
    if (m_pFloppyCtl == NULL)
        return false;
    return m_pFloppyCtl->AttachImage(slot, sFileName);
}

void CMotherboard::DetachFloppyImage(int slot)
{
    ASSERT(slot >= 0 && slot < 4);
    if (m_pFloppyCtl == NULL)
        return;
    m_pFloppyCtl->DetachImage(slot);
}

bool CMotherboard::IsFloppyEngineOn() const
{
    return m_pFloppyCtl->IsEngineOn();
}


// Работа с памятью //////////////////////////////////////////////////

uint16_t CMotherboard::GetLORAMWord(uint16_t offset)
{
    return *((uint16_t*)(m_pRAM + offset));  // Lower 56 KB
}
uint16_t CMotherboard::GetHIRAMWord(uint16_t offset)
{
    return *((uint16_t*)(m_pRAM + (uint32_t)0160000 + (uint32_t)offset));  // Higher 56 KB
}
uint16_t CMotherboard::GetVRAMWord(uint16_t offset)
{
    return *((uint16_t*)(m_pRAM + (uint32_t)0340000 + (uint32_t)offset));  // Top 16 KB of 128 KB RAM
}

uint8_t CMotherboard::GetLORAMByte(uint16_t offset)
{
    return m_pRAM[offset];
}
uint8_t CMotherboard::GetHIRAMByte(uint16_t offset)
{
    uint32_t dwOffset = (uint32_t)0160000 + (uint32_t)offset;
    return m_pRAM[dwOffset];
}
uint8_t CMotherboard::GetVRAMByte(uint16_t offset)
{
    uint32_t dwOffset = (uint32_t)0340000 + (uint32_t)offset;
    return m_pRAM[dwOffset];
}

void CMotherboard::SetLORAMWord(uint16_t offset, uint16_t word)
{
    *((uint16_t*)(m_pRAM + offset)) = word;
}
void CMotherboard::SetHIRAMWord(uint16_t offset, uint16_t word)
{
    uint32_t dwOffset = (uint32_t)0160000 + (uint32_t)offset;
    *((uint16_t*)(m_pRAM + dwOffset)) = word;
}
void CMotherboard::SetVRAMWord(uint16_t offset, uint16_t word)
{
    uint32_t dwOffset = (uint32_t)0340000 + (uint32_t)offset;
    *((uint16_t*)(m_pRAM + dwOffset)) = word;
}

void CMotherboard::SetLORAMByte(uint16_t offset, uint8_t byte)
{
    m_pRAM[offset] = byte;
}
void CMotherboard::SetHIRAMByte(uint16_t offset, uint8_t byte)
{
    uint32_t dwOffset = (uint32_t)0160000 + (uint32_t)offset;
    m_pRAM[dwOffset] = byte;
}
void CMotherboard::SetVRAMByte(uint16_t offset, uint8_t byte)
{
    uint32_t dwOffset = (uint32_t)0340000 + (uint32_t)offset;
    m_pRAM[dwOffset] = byte;
}

uint16_t CMotherboard::GetROMWord(uint16_t offset)
{
    ASSERT(offset < 1024 * 16);
    return *((uint16_t*)(m_pROM + offset));
}
uint8_t CMotherboard::GetROMByte(uint16_t offset)
{
    ASSERT(offset < 1024 * 16);
    return m_pROM[offset];
}

// Calculates video buffer start address, for screen drawing procedure
const uint8_t* CMotherboard::GetVideoBuffer()
{
    return (m_pRAM + (uint32_t)0340000);
}


//////////////////////////////////////////////////////////////////////


void CMotherboard::ResetDevices()
{
    if (m_pFloppyCtl != NULL)
        m_pFloppyCtl->Reset();

    m_pKeyboard->Reset();

    //m_pCPU->DeassertHALT();//DEBUG
    //m_pCPU->FireHALT();

    // Reset ports
    //TODO
}

void CMotherboard::ResetHALT()
{
    //TODO
}

void CMotherboard::Tick50()  // Vblank
{
    if (m_Port177400 & 0400)
        m_pCPU->TickIRQ2();
}

void CMotherboard::TimerTick() // Timer Tick, 8 MHz
{
}

void CMotherboard::DebugTicks()
{
    m_pCPU->SetInternalTick(0);
    m_pCPU->Execute();
    if (m_pFloppyCtl != NULL)
        m_pFloppyCtl->Periodic();
}

void CMotherboard::ExecuteCPU()
{
    m_pCPU->Execute();
}

/*
Каждый фрейм равен 1/25 секунды = 40 мс
Фрейм делим на 20000 тиков, 1 тик = 2 мкс
В каждый фрейм происходит:
* 300000 тиков таймер 1 -- 15 раз за тик -- 7.5 МГц
* 300000 тиков ЦП       -- 15 раз за тик
*      2 тика vblank    -- 50 Гц, в 0-й и 10000-й тик фрейма
*    625 тиков FDD -- каждый 32-й тик (300 RPM = 5 оборотов в секунду)
*    196 обращений к клавиатуре (4950 бод) -- каждый 101-й тик
*/
bool CMotherboard::SystemFrame()
{
    const int frameProcTicks = 15;
    const int audioticks = 20286 / (SOUNDSAMPLERATE / 25);
    const int floppyTicks = 32;
    const int serialOutTicks = 20000 / (9600 / 25);
    int serialTxCount = 0;
    const int keyboardTicks = 20000 / (4950 / 25);
    int keyboardTxCount = 0;

    for (int frameticks = 0; frameticks < 20000; frameticks++)
    {
        for (int procticks = 0; procticks < frameProcTicks; procticks++)  // CPU ticks
        {
#if !defined(PRODUCT)
            if ((m_dwTrace & TRACE_CPU) && m_pCPU->GetInternalTick() == 0)
                TraceInstruction(m_pCPU, this, m_pCPU->GetPC(), m_dwTrace);
#endif
            m_pCPU->Execute();
            if (m_pCPU->GetPC() == m_CPUbp)
                return false;  // Breakpoint

            // Timer 1 ticks
            //TimerTick();
        }

        if (frameticks == 0 || frameticks == 10000)
        {
            Tick50();  // Vblank tick
        }

        if (frameticks % floppyTicks == 0)  // FDD tick, every 64 uS
        {
            if (m_pFloppyCtl != NULL)
                m_pFloppyCtl->Periodic();
        }

        if (frameticks % audioticks == 0)  // AUDIO tick
            DoSound();

        if (frameticks % keyboardTicks)
        {
            m_pKeyboard->Periodic();
            if ((m_Port177442r & 2) == 0 && m_pKeyboard->HasByteReady())
            {
                m_Port177440 = m_pKeyboard->ReceiveByte();
#if !defined(PRODUCT)
                if (m_dwTrace & TRACE_KEYBOARD) DebugLogFormat(_T("Keyboard received %03o\r\n"), m_Port177440);
#endif
                m_Port177442r |= 2;  // Установка флага "готовность приёмника"
                m_pCPU->FireIRQ5();
            }
            if (keyboardTxCount > 0)
            {
                keyboardTxCount--;
                if (keyboardTxCount == 0)
                {
                    m_pKeyboard->SendByte(m_Port177460 & 0xff);
                    m_Port177442r |= 1;  // Установка флага "готовность передатчика"
                }
            }
            else if ((m_Port177442r & 1) == 0)  // Ready is 0?
            {
                keyboardTxCount = 8;  // Start translation countdown
            }
        }

        //if (m_SerialInCallback != NULL && frameticks % 52 == 0)
        //{
        //    uint8_t b;
        //    if (m_SerialInCallback(&b))
        //    {
        //        if (m_Port176500 & 0200)  // Ready?
        //            m_Port176500 |= 010000;  // Set Overflow flag
        //        else
        //        {
        //            m_Port176502 = (uint16_t)b;
        //            m_Port176500 |= 0200;  // Set Ready flag
        //            if (m_Port176500 & 0100)  // Interrupt?
        //                m_pCPU->InterruptVIRQ(7, 0300);
        //        }
        //    }
        //}
        //if (m_SerialOutCallback != NULL && frameticks % 52 == 0)
        //{
        //    if (serialTxCount > 0)
        //    {
        //        serialTxCount--;
        //        if (serialTxCount == 0)  // Translation countdown finished - the byte translated
        //        {
        //            (*m_SerialOutCallback)((uint8_t)(m_Port176506 & 0xff));
        //            m_Port176504 |= 0200;  // Set Ready flag
        //            if (m_Port176504 & 0100)  // Interrupt?
        //                m_pCPU->InterruptVIRQ(8, 0304);
        //        }
        //    }
        //    else if ((m_Port176504 & 0200) == 0)  // Ready is 0?
        //    {
        //        serialTxCount = 8;  // Start translation countdown
        //    }
        //}

        if (m_ParallelOutCallback != NULL)
        {
            //if ((m_Port177514 & 0240) == 040)
            //{
            //    m_Port177514 |= 0200;  // Set TR flag
            //    // Now printer waits for a next byte
            //    if (m_Port177514 & 0100)
            //        m_pCPU->InterruptVIRQ(5, 0200);
            //}
            //else if ((m_Port177514 & 0240) == 0)
            //{
            //    // Byte is ready, print it
            //    (*m_ParallelOutCallback)((uint8_t)(m_Port177516 & 0xff));
            //    m_Port177514 |= 040;  // Set Printer Acknowledge
            //}
        }
    }

    return true;
}

// Key pressed or released
void CMotherboard::KeyboardEvent(uint8_t scancode, bool okPressed)
{
    if (okPressed)  // Key released
    {
        m_pKeyboard->KeyPressed(scancode);

#if !defined(PRODUCT)
        if (m_dwTrace & TRACE_KEYBOARD)
        {
            if (scancode >= ' ' && scancode <= 127)
                DebugLogFormat(_T("Keyboard '%c'\r\n"), scancode);
            else
                DebugLogFormat(_T("Keyboard 0x%02x\r\n"), scancode);
        }
#endif
        return;
    }
}


//////////////////////////////////////////////////////////////////////
// Motherboard: memory management

// Read word from memory for debugger
uint16_t CMotherboard::GetWordView(uint16_t address, bool okHaltMode, bool okExec, int* pAddrType)
{
    uint16_t offset;
    int addrtype = TranslateAddress(address, okHaltMode, okExec, &offset);

    *pAddrType = addrtype;

    switch (addrtype & ADDRTYPE_MASK)
    {
    case ADDRTYPE_RAM:
        return GetLORAMWord(offset & 0177776);
    case ADDRTYPE_HIRAM:
        return GetHIRAMWord(offset & 0177776);
    case ADDRTYPE_VRAM:
        return GetVRAMWord(offset & 0177776);
    case ADDRTYPE_ROM:
        return GetROMWord(offset & 0177776);
    case ADDRTYPE_IO:
        return 0;  // I/O port, not memory
    case ADDRTYPE_DENY:
        return 0;  // This memory is inaccessible for reading
    }

    ASSERT(false);  // If we are here - then addrtype has invalid value
    return 0;
}

uint16_t CMotherboard::GetWord(uint16_t address, bool okHaltMode, bool okExec)
{
    uint16_t offset;
    int addrtype = TranslateAddress(address, okHaltMode, okExec, &offset);

    switch (addrtype & ADDRTYPE_MASK)
    {
    case ADDRTYPE_RAM:
        return GetLORAMWord(offset & 0177776);
    case ADDRTYPE_HIRAM:
        return GetHIRAMWord(offset & 0177776);
    case ADDRTYPE_VRAM:
        return GetVRAMWord(offset & 0177776);
    case ADDRTYPE_ROM:
        return GetROMWord(offset & 0177776);
    case ADDRTYPE_IO:
        //TODO: What to do if okExec == true ?
        return GetPortWord(address);
    case ADDRTYPE_DENY:
        m_pCPU->MemoryError();
        return 0;
    }

    ASSERT(false);  // If we are here - then addrtype has invalid value
    return 0;
}

uint8_t CMotherboard::GetByte(uint16_t address, bool okHaltMode)
{
    uint16_t offset;
    int addrtype = TranslateAddress(address, okHaltMode, false, &offset);

    switch (addrtype & ADDRTYPE_MASK)
    {
    case ADDRTYPE_RAM:
        return GetLORAMByte(offset);
    case ADDRTYPE_HIRAM:
        return GetHIRAMByte(offset);
    case ADDRTYPE_VRAM:
        return GetVRAMByte(offset);
    case ADDRTYPE_ROM:
        return GetROMByte(offset);
    case ADDRTYPE_IO:
        //TODO: What to do if okExec == true ?
        return GetPortByte(address);
    case ADDRTYPE_DENY:
        m_pCPU->MemoryError();
        return 0;
    }

    ASSERT(false);  // If we are here - then addrtype has invalid value
    return 0;
}

void CMotherboard::SetWord(uint16_t address, bool okHaltMode, uint16_t word)
{
    uint16_t offset;

    int addrtype = TranslateAddress(address, okHaltMode, false, &offset);

    switch (addrtype & ADDRTYPE_MASK)
    {
    case ADDRTYPE_RAM:
        SetLORAMWord(offset & 0177776, word);
        return;
    case ADDRTYPE_HIRAM:
        SetHIRAMWord(offset & 0177776, word);
        return;
    case ADDRTYPE_VRAM:
        SetVRAMWord(offset & 0177776, word);
        return;
    case ADDRTYPE_ROM:  // Writing to ROM: exception
        //m_pCPU->MemoryError();
#if !defined(PRODUCT)
        DebugLogFormat(_T("SetWord WRITE TO ROM addr=%06o val=%06o at PC=%06o\r\n"), address, word, m_pCPU->GetInstructionPC());
#endif
        return;
    case ADDRTYPE_IO:
        SetPortWord(address, word);
        return;
    case ADDRTYPE_DENY:
        m_pCPU->MemoryError();
        return;
    }

    ASSERT(false);  // If we are here - then addrtype has invalid value
}

void CMotherboard::SetByte(uint16_t address, bool okHaltMode, uint8_t byte)
{
    uint16_t offset;
    int addrtype = TranslateAddress(address, okHaltMode, false, &offset);

    switch (addrtype & ADDRTYPE_MASK)
    {
    case ADDRTYPE_RAM:
        SetLORAMByte(offset, byte);
        return;
    case ADDRTYPE_HIRAM:
        SetHIRAMByte(offset, byte);
        return;
    case ADDRTYPE_VRAM:
        SetVRAMByte(offset, byte);
        return;
    case ADDRTYPE_ROM:  // Writing to ROM: exception
        m_pCPU->MemoryError();
        return;
    case ADDRTYPE_IO:
        SetPortByte(address, byte);
        return;
    case ADDRTYPE_DENY:
        m_pCPU->MemoryError();
        return;
    }

    ASSERT(false);  // If we are here - then addrtype has invalid value
}

int CMotherboard::TranslateAddress(uint16_t address, bool okHaltMode, bool okExec, uint16_t* pOffset)
{
    int window = (address >> 13) & 7;  // Номер запрашиваемого окна в основной памяти 0..7

    // Сначала проверяем, не маппится ли на это окно видеопамять
    bool vramturedon = (m_Port177400 & 0200) != 0;  // Включено ли VRAM в основную память
    if (vramturedon)
    {
        int vramwindow0, vramwindow1;
        int vramflags = (m_Port177400 >> 10) & 3;  // Биты 10 и 11 указывают один из трёх блоков куда маппится VRAM
        switch (vramflags)
        {
        case 0:
            vramwindow0 = 0;  vramwindow1 = 1;  break;
        case 1:
            vramwindow0 = 2;  vramwindow1 = 3;  break;
        default:
            vramwindow0 = 4;  vramwindow1 = 5;  break;
        }
        if (window == vramwindow0 || window == vramwindow1)
        {
            *pOffset = address & 037777;  // 0..16383
            return ADDRTYPE_VRAM;
        }
    }

    // Если это окно 7, то это ПЗУ и порты
    if (window == 7)
    {
        if (address < 0177400)
        {
            *pOffset = (address - 0140000) & 037777;  // 8192..16383
            return ADDRTYPE_ROM;
        }

        *pOffset = address;
        return ADDRTYPE_IO;
    }
    // Если это окно 6, то возможно это тоже ПЗУ
    //TODO if (window == 6 && )

    bool isprimary = ((m_Port177400 >> window) & 1) != 0;  // Признак того что выбрано основное ОЗУ
    *pOffset = address;
    return isprimary ? ADDRTYPE_RAM : ADDRTYPE_HIRAM;
}

uint8_t CMotherboard::GetPortByte(uint16_t address)
{
    if (address & 1)
        return GetPortWord(address & 0xfffe) >> 8;

    return (uint8_t) GetPortWord(address);
}

uint16_t CMotherboard::GetPortWord(uint16_t address)
{
    switch (address)
    {
    default:
        if ((address & 0177440) == 0177400)  // 177400-177437
            return m_Port177400;  // Регистр диспетчера памяти

        m_pCPU->MemoryError();
#if !defined(PRODUCT)
        DebugLogFormat(_T("GetPort UNKNOWN port %06o at %06o\r\n"), address, m_pCPU->GetInstructionPC());
#endif
        return 0;

    case 0177440:  // Клавиатура: буфер данных приёмника, READ ONLY
        m_Port177442r &= ~2;  // Reset Ready flag
//#if !defined(PRODUCT)
//        if (m_dwTrace & TRACE_KEYBOARD) DebugLogFormat(_T("Keyboard 177440 read %06o\r\n"), m_Port177442r);
//#endif
        return m_Port177440;
    case 0177442: // Клавиатура: регистр состояния порта, READ
//#if !defined(PRODUCT)
//        if (m_dwTrace & TRACE_KEYBOARD) DebugLogFormat(_T("Keyboard 177440 read %06o\r\n"), m_Port177442r);
//#endif
        return m_Port177442r;
    case 0177460:  // Клавиатура: буфер данных передатчика
        return m_Port177460;
    case 0177462:  // Клавиатура: регистр уплавления, READ ONLY
        return 0;

    case 0177500:  // Таймер
        return 0; //STUB
    case 0177502:  // Таймер
        return 0; //STUB
    case 0177504:  // Таймер
        return 0; //STUB
    case 0177506:  // Таймер: управляющее слово
        return 0; //STUB

    case 0177524:
        return 0; //STUB
    case 0177526:
        return 0; //STUB

    case 0177540:  // Регистр порта A
        return 0; //STUB
    case 0177542:  // Регистр порта B
        return 0177777; //STUB
        //return 0;//DEBUG
    case 0177544:  // Регистр порта C
        return 0177777; //STUB
        //return 0;//DEBUG
    case 0177546:  // ИРПР: регистр управления
        return 0; //STUB
    case 0177600:  // Системный регистр A
        return 0; //STUB
    case 0177602:  // Системный регистр B
        return 0; //STUB
    case 0177604:  // Системный регистр C
        return m_Port177604;
    case 0177606:  // Управляющее слово программируемого интерфейса
        return 0; //STUB

    case 0177640:  // НГМД: регистр состояния
        if (m_pFloppyCtl == NULL) return 0;
        return m_pFloppyCtl->GetStatus();
    case 0177642:  // НГМД: регистр дорожки
        if (m_pFloppyCtl == NULL) return 0;
        return m_pFloppyCtl->GetTrack();
    case 0177644:  // НГМД: регистр сектора
        if (m_pFloppyCtl == NULL) return 0;
        return m_pFloppyCtl->GetSector();
    case 0177646:  // НГМД: регистр данных
        if (m_pFloppyCtl == NULL) return 0;
        return m_pFloppyCtl->GetData();

    case 0177700:  // Стык С2
        return 0; //STUB
    case 0177702:  // Стык С2
        return 0; //STUB
    case 0177720:
        return 0; //STUB
    case 0177722:
        return 0; //STUB

    case 0177770:
        return 0; //STUB
    }
}

// Read word from port for debugger
uint16_t CMotherboard::GetPortView(uint16_t address)
{
    switch (address)
    {
    case 0177600:  // Системный регистр A
        return 0; //STUB
    case 0177602:  // Системный регистр B
        return 0; //STUB
    case 0177604:  // Системный регистр C
        return m_Port177604;

    default:
        return 0;
    }
}

void CMotherboard::SetPortByte(uint16_t address, uint8_t byte)
{
    uint16_t word;
    if (address & 1)
    {
        word = GetPortWord(address & 0xfffe);
        word &= 0xff;
        word |= byte << 8;
        SetPortWord(address & 0xfffe, word);
    }
    else
    {
        word = GetPortWord(address);
        word &= 0xff00;
        SetPortWord(address, word | byte);
    }
}

void CMotherboard::SetPortWord(uint16_t address, uint16_t word)
{
    switch (address)
    {
    default:
        if ((address & 0177440) == 0177400)  // 177400-177437
        {
            m_Port177400 = word;
            return;
        }

        m_pCPU->MemoryError();
#if !defined(PRODUCT)
        DebugLogFormat(_T("SetPort UNKNOWN port %06o\r\n"), address);
#endif
        break;

    case 0177440:
        return; //STUB
    case 0177442: // Клавиатура: регистр инструкций
#if !defined(PRODUCT)
        if (m_dwTrace & TRACE_KEYBOARD) DebugLogFormat(_T("Keyboard %06o -> 177442\r\n"), word);
#endif
        return; //STUB
    case 0177460:
        m_Port177460 = word;
        m_Port177442r &= ~1;  // Reset Ready flag
#if !defined(PRODUCT)
        if (m_dwTrace & TRACE_KEYBOARD) DebugLogFormat(_T("Keyboard %06o -> 177460\r\n"), word);
#endif
        return; //STUB

    case 0177462:
        return; //STUB
    case 0177500:
        return; //STUB
    case 0177502:
        return; //STUB
    case 0177506:
        return; //STUB
    case 0177524:
        return; //STUB
    case 0177526:
        return; //STUB
    case 0177540:
        return; //STUB
    case 0177546:
        return; //STUB
    case 0177600:  // Системный регистр A
//#if !defined(PRODUCT)
//        DebugLogFormat(_T("SysRegA %06o -> 177600\r\n"), word);
//#endif
        if (m_pFloppyCtl != NULL)
            m_pFloppyCtl->SetControl(word & 017);
        return; //STUB
    case 0177602:  // Системный регистр B
        return; //STUB
    case 0177604:  // Системный регистр C
        m_Port177604 = word;
        return;
    case 0177606:
        return; //STUB

    case 0177640:  // НГМД: регистр команд
//#if !defined(PRODUCT)
//        if (m_dwTrace & TRACE_FLOPPY) DebugLogFormat(_T("Floppy %06o -> 177640\r\n"), word);
//#endif
        if (m_pFloppyCtl != NULL)
            m_pFloppyCtl->SetCommand(word & 0xff);
        return;
    case 0177642:  // НГМД: регистр дорожки
#if !defined(PRODUCT)
        if (m_dwTrace & TRACE_FLOPPY) DebugLogFormat(_T("Floppy SET TRACK %d\r\n"), (int)word);
#endif
        if (m_pFloppyCtl != NULL)
            m_pFloppyCtl->SetTrack(word & 0xff);
        return;
    case 0177644:  // НГМД: регистр сектора
#if !defined(PRODUCT)
        if (m_dwTrace & TRACE_FLOPPY) DebugLogFormat(_T("Floppy SET SECTOR %d\r\n"), (int)word);
#endif
        if (m_pFloppyCtl != NULL)
            m_pFloppyCtl->SetSector(word & 0xff);
        return;
    case 0177646:  // НГМД: регистр данных
#if !defined(PRODUCT)
        if (m_dwTrace & TRACE_FLOPPY) DebugLogFormat(_T("Floppy SET DATA %02X\r\n"), word);
#endif
        if (m_pFloppyCtl != NULL)
            m_pFloppyCtl->WriteData(word);
        return; //STUB

    case 0177700:
        return; //STUB
    case 0177702:
        return; //STUB
    case 0177720:
        return; //STUB
    case 0177722:
        return; //STUB

    case 0177770:
        return; //STUB

    }
}


//////////////////////////////////////////////////////////////////////
// Emulator image
//  Offset Length
//       0     32 bytes  - Header
//      32    128 bytes  - Board status
//     160     32 bytes  - CPU status
//     192   3904 bytes  - RESERVED
//    4096  16384 bytes  - ROM image 16K
//   20480 131072 bytes  - RAM image 128K
//  151552     --        - END

void CMotherboard::SaveToImage(uint8_t* pImage)
{
    // Board data
    uint16_t* pwImage = (uint16_t*) (pImage + 32);
    *pwImage++ = m_Configuration;
    pwImage += 6;  // RESERVED
    *pwImage++ = m_Port177400;
    *pwImage++ = m_Port177440;
    *pwImage++ = m_Port177442r;
    *pwImage++ = m_Port177460;
    *pwImage++ = 0;  // Reserved for port 177600
    *pwImage++ = 0;  // Reserved for port 177602
    *pwImage++ = m_Port177604;
    pwImage += 8;  // RESERVED
    *pwImage++ = m_Timer1;
    *pwImage++ = m_Timer1div;
    *pwImage++ = m_Timer2;
    *pwImage++ = (uint16_t)m_okSoundOnOff;

    // CPU status
    uint8_t* pImageCPU = pImage + 160;
    m_pCPU->SaveToImage(pImageCPU);
    // ROM
    uint8_t* pImageRom = pImage + 4096;
    memcpy(pImageRom, m_pROM, 16384);
    // RAM
    uint8_t* pImageRam = pImage + 20480;
    memcpy(pImageRam, m_pRAM, 128 * 1024);
}
void CMotherboard::LoadFromImage(const uint8_t* pImage)
{
    // Board data
    uint16_t* pwImage = (uint16_t*)(pImage + 32);
    m_Configuration = *pwImage++;
    pwImage += 6;  // RESERVED
    m_Port177400 = *pwImage++;
    m_Port177440 = *pwImage++;
    m_Port177442r = *pwImage++;
    m_Port177460 = *pwImage++;
    *pwImage++;  // Reserved for port 177600
    *pwImage++;  // Reserved for port 177602
    m_Port177604 = *pwImage++;
    pwImage += 8;  // RESERVED
    m_Timer1 = *pwImage++;
    m_Timer1div = *pwImage++;
    m_Timer2 = *pwImage++;
    m_okSoundOnOff = ((*pwImage++) != 0);

    // CPU status
    const uint8_t* pImageCPU = pImage + 160;
    m_pCPU->LoadFromImage(pImageCPU);

    // ROM
    const uint8_t* pImageRom = pImage + 4096;
    memcpy(m_pROM, pImageRom, 16384);
    // RAM
    const uint8_t* pImageRam = pImage + 20480;
    memcpy(m_pRAM, pImageRam, 128 * 1024);
}


//////////////////////////////////////////////////////////////////////

void CMotherboard::DoSound(void)
{
    if (m_SoundGenCallback == NULL)
        return;

    //uint16_t volume = (m_Port170030 >> 3) & 3;  // Громкость 0..3
    //uint16_t octave = m_Port170030 & 7;  // Октава 1..7
    //if (!m_okSoundOnOff || volume == 0 || octave == 0)
    //{
    //    (*m_SoundGenCallback)(0, 0);
    //    return;
    //}
    //if (m_Timer1 > m_Port170022 / 2)
    //{
    //    (*m_SoundGenCallback)(0, 0);
    //    return;
    //}

    //uint16_t sound = 0x1fff >> (3 - volume);
    //(*m_SoundGenCallback)(sound, sound);
}

void CMotherboard::SetSoundGenCallback(SOUNDGENCALLBACK callback)
{
    if (callback == NULL)  // Reset callback
    {
        m_SoundGenCallback = NULL;
    }
    else
    {
        m_SoundGenCallback = callback;
    }
}

void CMotherboard::SetSerialCallbacks(SERIALINCALLBACK incallback, SERIALOUTCALLBACK outcallback)
{
    if (incallback == NULL || outcallback == NULL)  // Reset callbacks
    {
        m_SerialInCallback = NULL;
        m_SerialOutCallback = NULL;
        //TODO: Set port value to indicate we are not ready to translate
    }
    else
    {
        m_SerialInCallback = incallback;
        m_SerialOutCallback = outcallback;
        //TODO: Set port value to indicate we are ready to translate
    }
}

void CMotherboard::SetParallelOutCallback(PARALLELOUTCALLBACK outcallback)
{
    if (outcallback == NULL)  // Reset callback
    {
        //m_Port177514 |= 0100000;  // Set Error flag
        m_ParallelOutCallback = NULL;
    }
    else
    {
        //m_Port177514 &= ~0100000;  // Reset Error flag
        m_ParallelOutCallback = outcallback;
    }
}


//////////////////////////////////////////////////////////////////////

#if !defined(PRODUCT)

void TraceInstruction(CProcessor* pProc, CMotherboard* pBoard, uint16_t address, DWORD dwTrace)
{
    bool okHaltMode = pProc->IsHaltMode();

    uint16_t memory[4];
    int addrtype = ADDRTYPE_RAM;
    memory[0] = pBoard->GetWordView(address + 0 * 2, okHaltMode, true, &addrtype);
    if (!((addrtype == ADDRTYPE_RAM || addrtype == ADDRTYPE_HIRAM) && (dwTrace & TRACE_CPURAM)) &&
        !(addrtype == ADDRTYPE_ROM && (dwTrace & TRACE_CPUROM)))
        return;
    memory[1] = pBoard->GetWordView(address + 1 * 2, okHaltMode, true, &addrtype);
    memory[2] = pBoard->GetWordView(address + 2 * 2, okHaltMode, true, &addrtype);
    memory[3] = pBoard->GetWordView(address + 3 * 2, okHaltMode, true, &addrtype);

    TCHAR bufaddr[7];
    PrintOctalValue(bufaddr, address);

    TCHAR instr[8];
    TCHAR args[32];
    DisassembleInstruction(memory, address, instr, args);
    TCHAR buffer[64];
    wsprintf(buffer, _T("%s: %s\t%s\r\n"), bufaddr, instr, args);
    //wsprintf(buffer, _T("%s %s: %s\t%s\r\n"), pProc->IsHaltMode() ? _T("HALT") : _T("USER"), bufaddr, instr, args);

    DebugLog(buffer);
}

#endif

//////////////////////////////////////////////////////////////////////
