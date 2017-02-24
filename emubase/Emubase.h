/*  This file is part of MS0515BTL.
    MS0515BTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    MS0515BTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
MS0515BTL. If not, see <http://www.gnu.org/licenses/>. */

// Emubase.h  Header for use of all emubase classes
//

#pragma once

#include "Board.h"
#include "Processor.h"


//////////////////////////////////////////////////////////////////////


#define SOUNDSAMPLERATE  22050


//////////////////////////////////////////////////////////////////////
// Disassembler

// Disassemble one instruction of KM1801VM2 processor
//   pMemory - memory image (we read only words of the instruction)
//   sInstr  - instruction mnemonics buffer - at least 8 characters
//   sArg    - instruction arguments buffer - at least 32 characters
//   Return value: number of words in the instruction
int DisassembleInstruction(uint16_t* pMemory, uint16_t addr, TCHAR* sInstr, TCHAR* sArg);


//////////////////////////////////////////////////////////////////////
// CFloppy

#define FLOPPY_RAWTRACKSIZE             6250    // Track length, bytes
#define FLOPPY_RAWMARKERSIZE            FLOPPY_RAWTRACKSIZE
#define FLOPPY_INDEXLENGTH              150     // Index mark length
#define FLOPPY_TRACKSIZE                5120    // Logical track size, 10 sectors, 512 bytes each sector

struct CFloppyDrive
{
    FILE* fpFile;
    bool okReadOnly;        // Write protection flag
    uint16_t dataptr;       // Data offset within m_data - "head" position
    uint16_t datatrack;     // Track number of data in m_data array
    uint8_t data[FLOPPY_RAWTRACKSIZE];  // Raw track image for the current track
    uint8_t marker[FLOPPY_RAWMARKERSIZE];  // Marker positions

public:
    CFloppyDrive();
    void Reset();

public:
    uint8_t  GetCurrentByte() { return data[dataptr]; }
    void     SetCurrentByte(uint8_t b) { data[dataptr] = b; }

};

class CFloppyController
{
protected:
    CFloppyDrive m_drivedata[8];  // Четыре привода по две стороны
    int m_drive;            // Drive number: from 0 to 7; -1 if not selected
    CFloppyDrive* m_pDrive; // Current drive; NULL if not selected
    bool m_trackchanged;    // TRUE = data was changed - need to save it into the file
    bool m_motoron;         // Motor ON flag
    bool m_okTrace;         // Trace mode on/off

    int  m_opercount;       // Operation counter - countdown of current operation stage
    int  m_tshift;
    int  m_state;
    int  m_statenext;
    uint8_t m_cmd;
    uint8_t m_data;
    int m_track;            // Track number: from 0 to ??
    int m_side;
    int m_sector;
    int m_direction;
    uint8_t m_rqs;
    uint16_t m_status;      // See FLOPPY_ST_XXX defines
    uint16_t m_system;
    long m_endwaitingam;
    int  m_rwptr;//TODO: Remove
    int  m_rwlen;
    uint16_t m_crc;
    int  m_startcrc;

public:
    CFloppyController();
    ~CFloppyController();
    void Reset();

public:
    bool AttachImage(int drive, LPCTSTR sFileName);
    void DetachImage(int drive);
    bool IsAttached(int drive) { return (m_drivedata[drive].fpFile != NULL); }
    bool IsReadOnly(int drive) { return m_drivedata[drive].okReadOnly; } // return (m_status & FLOPPY_STATUS_WRITEPROTECT) != 0; }
    bool IsEngineOn() const { return m_motoron; }
    uint16_t GetStatus();           // Reading status
    uint16_t GetData();             // Reading data
    uint8_t  GetTrack() const { return m_track; }
    uint8_t  GetSector() const { return m_sector; }
    uint16_t GetStateView() const { return m_status; }  // Get state value for debugger
    uint16_t GetDataView() const { return m_data; }  // Get data value for debugger
    void SetControl(uint8_t data);
    void SetCommand(uint8_t cmd);
    void SetTrack(uint8_t track) { m_track = track; }
    void SetSector(uint8_t sector) { m_sector = sector; }
    void WriteData(uint16_t data);
    void Periodic();                // Rotate disk; call it each 64 us - 15625 times per second
    void SetTrace(bool okTrace) { m_okTrace = okTrace; }  // Set trace mode on/off

private:
    void ReadFirstByte();
    void PrepareTrack();
    void FlushChanges();  // If current track was changed - save it

};

//////////////////////////////////////////////////////////////////////
// CKeyboard

#define KEYBOARD_QUEUELENGTH    8

class CKeyboard  // MS-7004 keyboard simulator
{
private:
    int m_nQueueLength;
    uint8_t m_Queue[KEYBOARD_QUEUELENGTH];
    int m_nTxCounter;

public:
    CKeyboard();
    ~CKeyboard();
    void Reset();               // Reset the device
    void SendByte(uint8_t);     // Send byte to the keyboard
    bool HasByteReady() const;  // Do we have a byte to receive from the keyboard
    uint8_t ReceiveByte();      // Receive byte from the keyboard
    void Periodic();            // Time tick; call it around 4900 times per second
    void KeyPressed(uint8_t scan);  // Key press event

private:
    void PutByteToQueue(uint8_t);

};


//////////////////////////////////////////////////////////////////////
