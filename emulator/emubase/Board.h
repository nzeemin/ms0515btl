/*  This file is part of MS0515BTL.
    MS0515BTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    MS0515BTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
MS0515BTL. If not, see <http://www.gnu.org/licenses/>. */

// Board.h
//

#pragma once

#include "Defines.h"


//////////////////////////////////////////////////////////////////////


// TranslateAddress result code
#define ADDRTYPE_RAM     0  // RAM -- lower 56 KB of RAM
#define ADDRTYPE_HIRAM   1  // RAM -- higher 56 KB of RAM
#define ADDRTYPE_VRAM    2  // Video RAM -- top 16 KB of RAM
#define ADDRTYPE_ROM     8  // ROM
#define ADDRTYPE_IO     16  // I/O port
#define ADDRTYPE_DENY  128  // Access denied
#define ADDRTYPE_MASK  255  // RAM type mask

// Trace flags
#define TRACE_NONE         0  // Turn off all tracing
#define TRACE_CPUROM       1  // Trace CPU instructions from ROM
#define TRACE_CPURAM       2  // Trace CPU instructions from RAM
#define TRACE_CPU          3  // Trace CPU instructions (mask)
#define TRACE_TIMER      010  // Trace timer events
#define TRACE_FLOPPY    0100  // Trace floppies
#define TRACE_KEYBOARD 01000  // Trace keyboard events
#define TRACE_ALL    0177777  // Trace all

// Emulator image constants
#define MS0515IMAGE_HEADER_SIZE 32
#define MS0515IMAGE_SIZE 151552
#define MS0515IMAGE_HEADER1 0x3530534D  // "MS05"
#define MS0515IMAGE_HEADER2 0x21213531  // "15!!"
#define MS0515IMAGE_VERSION 0x00010000  // 1.0


//////////////////////////////////////////////////////////////////////

// Sound generator callback function type
typedef void (CALLBACK* SOUNDGENCALLBACK)(uint16_t value);

// Serial port callback for receiving
// Output:
//   pbyte      Byte received
//   result     true means we have a new byte, false means not ready yet
typedef bool (CALLBACK* SERIALINCALLBACK)(uint8_t* pbyte);

// Serial port callback for translating
// Input:
//   byte       A byte to translate
// Output:
//   result     true means we translated the byte successfully, false means we have an error
typedef bool (CALLBACK* SERIALOUTCALLBACK)(uint8_t byte);

// Parallel port output callback
// Input:
//   byte       An output byte
// Output:
//   result     TRUE means OK, FALSE means we have an error
typedef bool (CALLBACK* PARALLELOUTCALLBACK)(uint8_t byte);

class CProcessor;
class CTimer8253;
class CFloppyController;
class CKeyboard;

//////////////////////////////////////////////////////////////////////

class CMotherboard  // MS0515 computer
{
private:  // Devices
    CProcessor* m_pCPU;  // CPU device
    CTimer8253* m_pTimer;
    CFloppyController*  m_pFloppyCtl;  // FDD control
    CKeyboard*  m_pKeyboard;
    bool        m_okTimer50OnOff;
private:  // Memory
    uint16_t    m_Configuration;  // See BK_COPT_Xxx flag constants
    uint8_t*    m_pRAM;  // RAM, 8 * 16 = 128 KB; top 16 KB is VideoRAM
    uint8_t*    m_pROM;  // ROM, 16 KB
public:  // Construct / destruct
    CMotherboard();
    ~CMotherboard();
public:  // Getting devices
    CProcessor* GetCPU() { return m_pCPU; }
public:  // Memory access  //TODO: Make it private
    uint16_t    GetLORAMWord(uint16_t offset);
    uint16_t    GetHIRAMWord(uint16_t offset);
    uint16_t    GetVRAMWord(uint16_t offset);
    uint8_t     GetLORAMByte(uint16_t offset);
    uint8_t     GetHIRAMByte(uint16_t offset);
    uint8_t     GetVRAMByte(uint16_t offset);
    void        SetLORAMWord(uint16_t offset, uint16_t word);
    void        SetHIRAMWord(uint16_t offset, uint16_t word);
    void        SetVRAMWord(uint16_t offset, uint16_t word);
    void        SetLORAMByte(uint16_t offset, uint8_t byte);
    void        SetHIRAMByte(uint16_t offset, uint8_t byte);
    void        SetVRAMByte(uint16_t offset, uint8_t byte);
    uint16_t    GetROMWord(uint16_t offset);
    uint8_t     GetROMByte(uint16_t offset);
public:  // Debug
    void        DebugTicks();  // One Debug CPU tick -- use for debug step or debug breakpoint
    void        SetCPUBreakpoints(const uint16_t* bps) { m_CPUbps = bps; } // Set CPU breakpoint list
    uint32_t    GetTrace() const { return m_dwTrace; }
    void        SetTrace(uint32_t dwTrace);
public:  // System control
    void        SetConfiguration(uint16_t conf);
    void        Reset();  // Reset computer
    void        LoadROM(const uint8_t* pBuffer);  // Load 8 KB ROM image from the biffer
    void        LoadRAM(int startbank, const uint8_t* pBuffer, int length);  // Load data into the RAM
    void        SetTimer50OnOff(bool okOnOff) { m_okTimer50OnOff = okOnOff; }
    bool        IsTimer50OnOff() const { return m_okTimer50OnOff; }
    void        Tick50();           // Tick 50 Hz - goes to CPU EVNT line
    void        ResetDevices();     // INIT signal
    void        ResetHALT();//DEBUG
public:
    void        ExecuteCPU();  // Execute one CPU instruction
    bool        SystemFrame();  // Do one frame -- use for normal run
    void        KeyboardEvent(uint8_t scancode, bool okPressed);  // Key pressed or released
    int         GetSoundChanges() const { return m_SoundChanges; }  ///< Sound signal 0 to 1 changes since the beginning of the frame
public:  // Floppy
    bool        AttachFloppyImage(int slot, LPCTSTR sFileName);
    void        DetachFloppyImage(int slot);
    bool        IsFloppyImageAttached(int slot) const;
    bool        IsFloppyReadOnly(int slot) const;
    bool        IsFloppyEngineOn() const;
public:  // Callbacks
    void        SetSoundGenCallback(SOUNDGENCALLBACK callback);
    void        SetSerialCallbacks(SERIALINCALLBACK incallback, SERIALOUTCALLBACK outcallback);
    void        SetParallelOutCallback(PARALLELOUTCALLBACK outcallback);
public:  // Memory
    // Read command for execution
    uint16_t GetWordExec(uint16_t address) { return GetWord(address, TRUE); }
    // Read word from memory
    uint16_t GetWord(uint16_t address) { return GetWord(address, FALSE); }
    // Read word
    uint16_t GetWord(uint16_t address, bool okExec);
    // Write word
    void SetWord(uint16_t address, uint16_t word);
    // Read byte
    uint8_t GetByte(uint16_t address);
    // Write byte
    void SetByte(uint16_t address, uint8_t byte);
    // Read word from memory for debugger
    uint16_t GetWordView(uint16_t address, bool okExec, int* pValid);
    // Read word from port for debugger
    uint16_t GetPortView(uint16_t address);
    // Get video buffer address
    const uint8_t* GetVideoBuffer();
private:
    // Determite memory type for given address - see ADDRTYPE_Xxx constants
    //   okExec - TRUE: read instruction for execution; FALSE: read memory
    //   pOffset - result - offset in memory plane
    int TranslateAddress(uint16_t address, bool okExec, uint16_t* pOffset);
private:  // Access to I/O ports
    uint16_t    GetPortWord(uint16_t address);
    void        SetPortWord(uint16_t address, uint16_t word);
    uint8_t     GetPortByte(uint16_t address);
    void        SetPortByte(uint16_t address, uint8_t byte);
public:  // Saving/loading emulator status
    void        SaveToImage(uint8_t* pImage);
    void        LoadFromImage(const uint8_t* pImage);
private:  // Ports: implementation
    uint16_t    m_Port177400;       // Регистр диспетчера памяти
    uint16_t    m_Port177440;       // Клавиатура: буфер данных приёмника
    uint16_t    m_Port177442r;      // Клавиатура: регистр состояния порта
    uint16_t    m_Port177460;       // Клавиатура: буфер данных передатчика
    uint16_t    m_Port177600;       // Системный регистр A
    uint16_t    m_Port177604;       // Системный регистр C
private:
    const uint16_t* m_CPUbps;  // CPU breakpoint list, ends with 177777 value
    uint32_t    m_dwTrace;  // Trace flags
    bool        m_okSoundOnOff;
    int         m_SoundPrevValue;  ///< Previous value of the sound signal
    int         m_SoundChanges;  ///< Sound signal 0 to 1 changes since the beginning of the frame
private:
    SOUNDGENCALLBACK m_SoundGenCallback;
    SERIALINCALLBACK    m_SerialInCallback;
    SERIALOUTCALLBACK   m_SerialOutCallback;
    PARALLELOUTCALLBACK m_ParallelOutCallback;
private:
    void        DoSound();
};


//////////////////////////////////////////////////////////////////////
