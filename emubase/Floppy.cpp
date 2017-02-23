/*  This file is part of BKBTL.
    BKBTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    BKBTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
BKBTL. If not, see <http://www.gnu.org/licenses/>. */

// Floppy.cpp
// Floppy controller and drives emulation
// See defines in header file Emubase.h

#include "stdafx.h"
#include <sys/types.h>
#include <sys/stat.h>
#include "Emubase.h"


//////////////////////////////////////////////////////////////////////

#define CB_SEEK_RATE             0x03
#define CB_SEEK_VERIFY           0x04
#define CB_SEEK_HEADLOAD         0x08
#define CB_SEEK_TRKUPD           0x10
#define CB_SEEK_DIR              0x20
#define CB_WRITE_DEL             0x01
#define CB_SIDE_CMP              0x02
#define CB_DELAY                 0x04
#define CB_SIDE                  0x08
#define CB_SIDE_SHIFT            0x03
#define CB_MULTIPLE              0x10

#define ST_BUSY                  0x01
#define ST_INDEX                 0x02
#define ST_DRQ                   0x02
#define ST_TRK00                 0x04
#define ST_LOST                  0x04
#define ST_CRCERR                0x08
#define ST_NOTFOUND              0x10
#define ST_SEEKERR               0x10
#define ST_RECORDT               0x20
#define ST_HEADL                 0x20
#define ST_WRFAULT               0x20
#define ST_WRITEP                0x40
#define ST_NOTRDY                0x80

enum eState
{
    S_IDLE, S_WAIT, S_DELAY_BEFORE_CMD, S_CMD_RW, S_FOUND_NEXT_ID,
    S_READ, S_WRSEC, S_WRITE, S_WRTRACK, S_WR_TRACK_DATA, S_TYPE1_CMD,
    S_STEP, S_SEEKSTART, S_SEEK, S_VERIFY, S_RESET
};
enum eRequest { R_NONE, R_DRQ = 0x40, R_INTRQ = 0x80 };

const int MAX_PHYS_CYL = 86;	// don't seek over it

static void EncodeTrackData(const uint8_t* pSrc, uint8_t* data, uint8_t* marker, uint16_t track, uint16_t side);
static bool DecodeTrackData(const uint8_t* pRaw, uint8_t* pDest, uint16_t track);

//////////////////////////////////////////////////////////////////////


CFloppyDrive::CFloppyDrive()
{
    fpFile = NULL;
    okReadOnly = false;
    datatrack = 0;
    dataptr = 0;
    memset(data, 0, sizeof(data));
    memset(marker, 0, sizeof(marker));
}

void CFloppyDrive::Reset()
{
    //datatrack = 0;
    dataptr = 0;
}


//////////////////////////////////////////////////////////////////////


CFloppyController::CFloppyController()
{
    m_drive = -1;  m_pDrive = NULL;
    m_track = 0;
    m_motoron = false;
    m_opercount = 0;
    m_trackchanged = false;
    m_status = m_tshift = 0;
    m_data = m_cmd = 0;
    m_system = 0;
    m_state = S_IDLE;  m_statenext = S_IDLE;
    m_track = m_side = m_sector = m_direction = 0;
    m_rqs = R_NONE;  m_endwaitingam = 0;
    m_rwptr = m_rwlen = 0;
    m_crc = 0;  m_startcrc = -1;
}

CFloppyController::~CFloppyController()
{
    for (int drive = 0; drive < 8; drive++)
        DetachImage(drive);
}

void CFloppyController::Reset()
{
#if !defined(PRODUCT)
    if (m_okTrace) DebugLog(_T("Floppy RESET\r\n"));
#endif

    FlushChanges();

    m_drive = -1;  m_pDrive = NULL;
    m_track = 0;
    m_opercount = 0;
    m_data = 0;
    m_trackchanged = false;
    m_status = 0;
}

bool CFloppyController::AttachImage(int drive, LPCTSTR sFileName)
{
    ASSERT(sFileName != NULL);

    // If image attached - detach one first
    if (m_drivedata[drive].fpFile != NULL)
        DetachImage(drive);

    // Open file
    m_drivedata[drive].okReadOnly = false;
    m_drivedata[drive].fpFile = ::_tfopen(sFileName, _T("r+b"));
    if (m_drivedata[drive].fpFile == NULL)
    {
        m_drivedata[drive].okReadOnly = true;
        m_drivedata[drive].fpFile = ::_tfopen(sFileName, _T("rb"));
    }
    if (m_drivedata[drive].fpFile == NULL)
        return false;

    m_track = m_drivedata[drive].datatrack = 0;
    m_drivedata[drive].dataptr = 0;
    m_data = 0;
    m_trackchanged = false;
    m_status = 0;
    m_opercount = 0;

    PrepareTrack();

    return true;
}

void CFloppyController::DetachImage(int drive)
{
    if (m_drivedata[drive].fpFile == NULL) return;

    FlushChanges();

    ::fclose(m_drivedata[drive].fpFile);
    m_drivedata[drive].fpFile = NULL;
    m_drivedata[drive].okReadOnly = false;
    m_drivedata[drive].Reset();
}

//////////////////////////////////////////////////////////////////////

static uint16_t Floppy_LastStatus = 0177777;  //DEBUG
uint16_t CFloppyController::GetStatus(void)
{
    m_rqs &= ~R_INTRQ;
    uint16_t res = m_status;

//#if !defined(PRODUCT)
//    if (m_okTrace && Floppy_LastStatus != m_status)
//    {
//        DebugLogFormat(_T("Floppy GET STATUS %02X\r\n"), res);
//        Flopy_LastStatus = m_status;
//    }
//#endif

    return res;
}

static uint8_t Floppy_LastControl = 0x0f;  //DEBUG
void CFloppyController::SetControl(uint8_t data)
{
#if !defined(PRODUCT)
    if (m_okTrace && data != Floppy_LastControl)
    {
        DebugLogFormat(_T("Floppy%d CONTROL %02X\r\n"), m_drive, data);
        Floppy_LastControl = data;
    }
#endif

    bool okPrepareTrack = false;  // ����� �� ��������� ������� � �����

    int newdrive = (data & 3); //((data & 2) << 2) | (((data & 8) >> 2) ^ 2) | (data & 1);
    if (data == 0x0f || data == 0) newdrive = -1;

    if (m_drive != newdrive)
    {
        FlushChanges();
        m_drive = newdrive;
        m_pDrive = (newdrive < 0) ? NULL : m_drivedata + m_drive;
        okPrepareTrack = true;
#if !defined(PRODUCT)
        if (m_okTrace) DebugLogFormat(_T("Floppy CURRENT DRIVE %d\r\n"), newdrive);
#endif
    }

    m_motoron = ((data & 4) == 0);

    if (okPrepareTrack)
        PrepareTrack();
}

void CFloppyController::SetCommand(uint8_t cmd)
{
#if !defined(PRODUCT)
    if (m_okTrace) DebugLogFormat(_T("Floppy%d SET COMMAND %02X =====\r\n"), m_drive, cmd);
#endif

    m_opercount = -1;

    if ((cmd & 0xF0) == 0xD0)  // D0..DF -- Force Interrupt
    {
        m_state = S_IDLE;
        m_rqs = R_INTRQ;
        m_status &= ~ST_BUSY;
        return;
    }

    if (m_status & ST_BUSY)
        return;

    m_cmd = cmd;

    m_status |= ST_BUSY;
    m_rqs = R_NONE;
    if (m_cmd & 0x80)  // read/write command
    {
        if (m_status & ST_NOTRDY)  // abort if no disk
        {
            m_state = S_IDLE;
            m_rqs = R_INTRQ;
            return;
        }
        m_state = S_DELAY_BEFORE_CMD;
        m_opercount = 235;  // 15ms delay before a read/write operation; 15 ms = 234.375 periodic calls
        m_statenext = S_CMD_RW;
        m_startcrc = -1;
        return;
    }

    m_state = S_TYPE1_CMD;  // else seek/step command
}

uint16_t CFloppyController::GetData(void)
{
    m_status &= ~ST_DRQ;
    m_rqs &= ~R_DRQ;

#if !defined(PRODUCT)
    uint16_t offset = m_pDrive->dataptr;
    if (m_okTrace && offset >= 10 && (offset - 96) % 614 == 0)
        DebugLogFormat(_T("Floppy%d READ %02X POS%04d SC%02d TR%02d\r\n"), m_drive, m_data, offset, (offset - 96) / 614 + 1, m_track);
#endif

    return m_data;
}

void CFloppyController::WriteData(uint16_t data)
{
//#if !defined(PRODUCT)
//    uint16_t offset = m_pDrive->dataptr;
//    if (m_okTrace && offset >= 10 && (offset - 10) % 130 == 0)
//        DebugLogFormat(_T("Floppy%d WRITE %02x POS%04d SC%02d TR%02d\r\n"), m_drive, data, m_pDrive->dataptr, (offset - 10) / 130 + 1, m_track);
//#endif

    m_data = data & 0xff;
}

static uint8_t FloppyLastState = 0;//DEBUG
void CFloppyController::Periodic()
{
    if (IsEngineOn())  // ������� ������� ������ ���� ������� �����
    {
        // ������� ������� �� ���� ������� �����
        for (int drive = 0; drive < 8; drive++)
        {
            m_drivedata[drive].dataptr++;
            if (m_drivedata[drive].dataptr >= FLOPPY_RAWTRACKSIZE)
                m_drivedata[drive].dataptr = 0;
        }
    }

    if (m_opercount > 0)  // ��������� ������� ������� ��������
    {
        m_opercount--;
//#if !defined(PRODUCT)
//        if (m_okTrace && m_opercount == 0)
//            DebugLogFormat(_T("Floppy state %d end, next %d\r\n"), m_state, m_statenext);
//#endif
    }

    bool diskpresent = (m_pDrive != NULL) && (IsAttached(m_drive));
    if (diskpresent)
        m_status &= ~ST_NOTRDY;
    else
        m_status |= ST_NOTRDY;

    if (!(m_cmd & 0x80))  // seek/step commands
    {
        m_status &= ~(ST_TRK00 | ST_INDEX);

        if (m_pDrive != NULL && m_pDrive->datatrack == 0)
            m_status |= ST_TRK00;
        if (m_pDrive != NULL && m_pDrive->dataptr < FLOPPY_INDEXLENGTH)
            m_status |= ST_INDEX;
    }

#if !defined(PRODUCT)
    if (m_okTrace && m_state != FloppyLastState)
    {
        DebugLogFormat(_T("Floppy state changed %d -> %d\r\n"), (int)FloppyLastState, (int)m_state);
        FloppyLastState = m_state;
    }
#endif
    switch (m_state)
    {
    case S_IDLE:
        m_status &= ~ST_BUSY;
        m_rqs = R_INTRQ;
        break;
    case S_WAIT:
        if (m_opercount > 0)
            break;
        m_state = m_statenext;
        m_statenext = 0;
        break;
    case S_DELAY_BEFORE_CMD:
        if (m_opercount > 0)
            break;
        m_status = (m_status | ST_BUSY) & ~(ST_DRQ | ST_LOST | ST_NOTFOUND | ST_RECORDT | ST_WRITEP);
        m_state = S_WAIT;
        m_statenext = S_CMD_RW;
        break;
    case S_CMD_RW:
        if (((m_cmd & 0xe0) == 0xa0 || (m_cmd & 0xf0) == 0xf0) &&
            m_pDrive != NULL && m_pDrive->okReadOnly)
        {
            m_status |= ST_WRITEP;
            m_state = S_IDLE;
            break;
        }
        if ((m_cmd & 0xc0) == 0x80 || (m_cmd & 0xf8) == 0xc0)  // read/write sectors or read AM -- find next AM
        {
            if (m_pDrive != NULL)
            {
                if (m_startcrc < 0)
                {
                    if (!m_pDrive->marker[m_pDrive->dataptr] || m_pDrive->data[m_pDrive->dataptr] != 0xfe)
                        break;  // Wait for ID marker
                    // Marker found
                    m_startcrc = m_pDrive->dataptr;  // Start reading 6-byte sector header
                }
                if (m_startcrc >= 0)
                {
                    //m_data = m_pDrive->data[m_pDrive->dataptr];  // Read next byte of the current sector header
                    //m_rqs = R_DRQ;
                    //m_status |= ST_DRQ;
                    int readlen = m_pDrive->dataptr - m_startcrc;  if (readlen < 0) readlen += FLOPPY_RAWTRACKSIZE;
                    if (readlen == 3 && m_pDrive->data[m_pDrive->dataptr] != m_sector && (m_cmd & 0xf8) != 0xc0)  // Wrong sector, wait for next one
                    {
                        m_startcrc = -1;
                        break;
                    }
                    if (readlen >= 6 && (m_cmd & 0xf8) == 0xc0)
                    {
                        // ��� ���������� ������� "������ ������"
                        m_sector = m_track;      // ����������� �������� ������� ������������ � ������� �������
                        m_state = S_IDLE;
                        break;
                    }
                    if (!m_pDrive->marker[m_pDrive->dataptr] || m_pDrive->data[m_pDrive->dataptr] != 0xfb)  // Data marker
                        break;
                    // Finished reading the current sector header
                    m_startcrc = -1;
                    m_status &= ~ST_CRCERR;
                    if ((m_cmd & 0xc0) == 0x80)
                    {
                        m_rwlen = 512;
                        m_state = S_READ;
                        break;
                    }
                }
            }
        }
        //TODO
        m_state = S_IDLE;
        break;
    case S_FOUND_NEXT_ID:
        if (m_pDrive == NULL) //TODO: Not accurate implementation for now
        {
            m_status |= ST_NOTFOUND;
            m_state = S_IDLE;
        }
        m_status &= ~ST_CRCERR;
        //TODO: verify after seek
        {
            if (m_startcrc < 0) m_startcrc = m_pDrive->dataptr;
            m_data = m_pDrive->data[m_pDrive->dataptr];  // Read next byte of the current sector header
            m_rqs = R_DRQ;
            m_status |= ST_DRQ;
            int readlen = m_pDrive->dataptr - m_startcrc;  if (readlen < 0) readlen += FLOPPY_RAWTRACKSIZE;
            if (readlen == 3 && m_data != m_sector)
            {
                m_state = S_VERIFY;  // Find next marker
                break;
            }
            if (readlen < 6)
                break;
            // Now we finished to read the header
            m_startcrc = -1;
        }
        //TODO: if (m_cmd & 0x20)  // write sector(s)
        //TODO
        m_rwlen = 512;
        ReadFirstByte();
        break;
    case S_READ:
        if (m_rwlen > 0)
        {
            if (m_rqs & R_DRQ)
                m_status |= ST_LOST;
            m_data = m_pDrive->data[m_pDrive->dataptr];  // Read next byte
            m_rwlen--;
            //TODO: Update m_crc
            m_rqs = R_DRQ;
            m_status |= ST_DRQ;
        }
        else
        {
            //TODO
            m_state = S_IDLE;
        }
        break;
    case S_WRSEC:
        //TODO
        break;
    case S_WRITE:
        //TODO
        break;
    case S_WRTRACK:
        //TODO
        break;
    case S_WR_TRACK_DATA:
        //TODO
        break;
    case S_TYPE1_CMD:
        m_status = (m_status | ST_BUSY) & ~(ST_DRQ | ST_CRCERR | ST_SEEKERR | ST_WRITEP);
        m_rqs = R_NONE;
        if (m_pDrive != NULL && m_pDrive->okReadOnly)
            m_status |= ST_WRITEP;
        //TODO: Motor
        m_statenext = S_SEEKSTART;
        if (m_cmd & 0xE0) //single step
        {
            if (m_cmd & 0x40) m_direction = (m_cmd & CB_SEEK_DIR) ? -1 : 1;
            m_statenext = S_STEP;
        }
        m_opercount = 15625;  // 1000 ms
        m_state = S_WAIT;
        break;
    case S_STEP:
        {
            /*if (m_pDrive != NULL && m_pDrive->datatrack == 0 && (m_cmd & 0xf0) == 0)
            {
                m_track = 0;
                m_state = S_VERIFY;
                break;
            }*/
            //if ((m_cmd & 0xe0) == 0 || (m_cmd & CB_SEEK_TRKUPD))
            {
                m_track += m_direction;
            }
            int cyl = (m_pDrive != NULL) ? m_pDrive->datatrack : 0;
            if (cyl < 0) cyl = 0;
            if (cyl > MAX_PHYS_CYL) cyl = MAX_PHYS_CYL;

            PrepareTrack();

            static const int steps[] = { 6, 12, 20, 30 };
            m_opercount = steps[m_cmd & CB_SEEK_RATE] * 16;
            m_state = S_WAIT;
            m_statenext = (m_cmd & 0xe0) ? S_VERIFY : S_SEEK;
            break;
        }
    case S_SEEKSTART:
        if ((m_cmd & 0x10) == 0)
        {
            m_track = 0xff;
            m_data = 0;
        }
    case S_SEEK:
        if (m_data == m_track)
        {
            m_state = S_VERIFY;
            break;
        }
        m_direction = (m_data < m_track) ? -1 : 1;
        m_state = S_STEP;
        break;
    case S_VERIFY:
        if ((m_cmd & CB_SEEK_VERIFY) == 0 || m_pDrive == NULL)
        {
            m_state = S_IDLE;
            break;
        }
        // Search for marker
        if (!m_pDrive->marker[m_pDrive->dataptr] /*&& m_startcrc < 0*/)
            break;  // Wait for a marker
        if (m_pDrive->marker[m_pDrive->dataptr])  // Marker found
        {
            m_state = S_FOUND_NEXT_ID;
            break;
        }
        break;
    case S_RESET: //seek to trk0, but don't be busy
        if (m_pDrive != NULL && m_pDrive->datatrack == 0)
        {
            m_state = S_IDLE;
        }
        else
        {
            //TODO: Switch to lower track
        }
        m_opercount = 94;  // 6ms = 93.75 periodic calls
        break;
    }
}

void CFloppyController::ReadFirstByte()
{
    m_crc = 0;//TODO: Init m_crc
    m_data = m_pDrive->data[m_pDrive->dataptr];  // Read next byte
    m_rwlen--;
    m_rqs = R_DRQ;
    m_status |= ST_DRQ;
    m_state = S_READ;
}

// Read track data from file and fill m_data
void CFloppyController::PrepareTrack()
{
    FlushChanges();

    if (m_pDrive == NULL) return;

#if !defined(PRODUCT)
    if (m_okTrace) DebugLogFormat(_T("Floppy%d PREPARE TRACK %d\r\n"), m_drive, m_track);
#endif

    uint32_t count;

    m_trackchanged = false;
    m_pDrive->dataptr = 0;
    m_pDrive->datatrack = m_track;

    long foffset = m_track * 5120;  // Track has 10 sectors, 512 bytes each
    uint8_t data[5120];
    memset(data, 0, 5120);

    if (m_pDrive->fpFile != NULL)
    {
        ::fseek(m_pDrive->fpFile, foffset, SEEK_SET);
        count = ::fread(&data, 1, 5120, m_pDrive->fpFile);
        //TODO: �������� ������ ������
    }

    // Fill m_data array with data
    EncodeTrackData(data, m_pDrive->data, m_pDrive->marker, m_track, m_side);

    //FILE* fpTrack = ::_tfopen(_T("RawTrack.bin"), _T("w+b"));
    //::fwrite(m_pDrive->data, 1, FLOPPY_RAWTRACKSIZE, fpTrack);
    //::fclose(fpTrack);

//    //DEBUG: Test DecodeTrackData()
//    uint8_t data2[5120];
//    bool parsed = DecodeTrackData(m_pDrive->data, data2, m_track);
//    ASSERT(parsed);
//    bool tested = true;
//    for (int i = 0; i < 5120; i++)
//        if (data[i] != data2[i])
//        {
//            tested = false;
//            break;
//        }
//    ASSERT(tested);
}

void CFloppyController::FlushChanges()
{
    if (m_drive == -1) return;
    if (!IsAttached(m_drive)) return;
    if (!m_trackchanged) return;

#if !defined(PRODUCT)
    if (m_okTrace) DebugLogFormat(_T("Floppy%d FLUSH\r\n"), m_drive);  //DEBUG
#endif

    // Decode track data from m_data
    uint8_t data[128 * 23];  memset(data, 0, 128 * 23);
    bool decoded = DecodeTrackData(m_pDrive->data, data, m_pDrive->datatrack);

    if (decoded)  // Write to the file only if the track was correctly decoded from raw data
    {
        // Track has 23 sectors, 128 bytes each; track 0 has only 22 data sectors
        long foffset = 0;
        int sectors = 22;
        if (m_pDrive->datatrack > 0)
        {
            foffset = (m_pDrive->datatrack * 23 - 1) * 128;
            sectors = 23;
        }

//        // Check file length
//        ::fseek(m_pDrive->fpFile, 0, SEEK_END);
//        uint32_t currentFileSize = ::ftell(m_pDrive->fpFile);
//        while (currentFileSize < (uint32_t)(foffset + 5120))
//        {
//            uint8_t datafill[512];  ::memset(datafill, 0, 512);
//            uint32_t bytesToWrite = ((uint32_t)(foffset + 5120) - currentFileSize) % 512;
//            if (bytesToWrite == 0) bytesToWrite = 512;
//            ::fwrite(datafill, 1, bytesToWrite, m_pDrive->fpFile);
//            //TODO: �������� �� ������ ������
//            currentFileSize += bytesToWrite;
//        }

        // Save data into the file
        ::fseek(m_pDrive->fpFile, foffset, SEEK_SET);
        uint32_t dwBytesWritten = ::fwrite(&data, 1, 128 * sectors, m_pDrive->fpFile);
        //TODO: �������� �� ������ ������
    }
    else
    {
#if !defined(PRODUCT)
        if (m_okTrace) DebugLog(_T("Floppy FLUSH FAILED\r\n"));  //DEBUG
#endif
    }

    m_trackchanged = false;
}


//////////////////////////////////////////////////////////////////////


uint16_t CalculateChecksum(const uint8_t* buffer, int length)
{
    ASSERT(buffer != NULL);
    ASSERT(length > 0);
    uint16_t sum = 0;
    while (length > 0)
    {
        uint16_t src = *buffer;
        if (src & 0200) src |= 0177400;  // ���������� ��������� ����
        signed short dst = sum + src;
        sum = dst;

        buffer++;
        length--;
    }
    return sum;
}

// Fill data array and marker array with marked data
// pSrc   array length is 10 * 512
// data   array length is 6250 == FLOPPY_RAWTRACKSIZE
// marker array length is 6250 == FLOPPY_RAWMARKERSIZE
//-------------------------------------------------
//     �  54        4E
//     �  12        00
// 105-+   4        F6 (������������ C2)
//     �  35        4E
//  ---- ������ ������� 1..10 ---------------------
//     �   8        00                            �
//     �   3        F5 (������������ A1)          �
//     �   1        FE � ������ ��������� ������� �
//     �   1        tt � ����� ������� 0..79      �
//     �   1        00 � �������: 0 - ���         �
//     �   1        0s � ����� ������� 1..10      �
//     �   1        02 � 512 ���� �� ������       �
//     �   1(2)     F7 (������������ 2 ����� CRC) �
//     �  22        4E                            �
//     �  12        00                            �
//     �   3        F5 (������������ A1)          �
//     �   1        FB � ������ ������            �
// 612-+ 512        xx � ������ �������           �
//(614)�   1(2)     F7 (������������ 2 ����� CRC) �
//     �  44        4E                            �
//  ---- ����� ������� ----------------------------
//     � 352        4E � �� ����� �������
//-------------------------------------------------
static void EncodeTrackData(const uint8_t* pSrc, uint8_t* data, uint8_t* marker, uint16_t track, uint16_t side)
{
    memset(data, 0, FLOPPY_RAWTRACKSIZE);
    memset(marker, 0, FLOPPY_RAWMARKERSIZE);
    uint32_t count;
    int ptr = 0;
    for (count = 0; count < 54; count++) data[ptr++] = 0x4e;  // Initial gap
    for (count = 0; count < 12; count++) data[ptr++] = 0x00;
    for (count = 0; count < 4; count++) data[ptr++] = 0xC2;
    int gap = 35;
    for (int sect = 0; sect < 10; sect++)
    {
        // sector gap
        for (count = 0; count < (uint32_t)gap; count++) data[ptr++] = 0x4e;
        // sector header
        for (count = 0; count < 8; count++) data[ptr++] = 0x00;
        data[ptr++] = 0xa1;  data[ptr++] = 0xa1;  data[ptr++] = 0xa1;
        marker[ptr] = true;  data[ptr++] = 0xfe;  // ID marker; start CRC calculus
        data[ptr++] = (uint8_t)track;  // Track 0..79
        data[ptr++] = (side != 0);
        data[ptr++] = (uint8_t)sect + 1;  // Sector 1..10
        data[ptr++] = 2; // Assume 512 bytes per sector;
        // crc
        data[ptr++] = 0x12;  data[ptr++] = 0x34;  //TODO: Calculate CRC
        for (count = 0; count < 22; count++) data[ptr++] = 0x4e;
        // data header
        for (count = 0; count < 12; count++) data[ptr++] = 0x00;
        data[ptr++] = 0xa1;  data[ptr++] = 0xa1;  data[ptr++] = 0xa1;
        marker[ptr] = true;  data[ptr++] = 0xfb;  // Data marker; start CRC calculus
        // data
        for (count = 0; count < 512; count++)
            data[ptr++] = pSrc[(sect * 512) + count];
        // crc
        data[ptr++] = 0x43;  data[ptr++] = 0x21;  //TODO: Calculate CRC
        gap = 44;
    }
    // fill to the end of the track
    while (ptr < FLOPPY_RAWTRACKSIZE) data[ptr++] = 0x4e;
}

// Decode track data from raw data
// pRaw is array of FLOPPY_RAWTRACKSIZE bytes
// pDest is array of 5120 bytes
// Returns: true - decoded, false - parse error
static bool DecodeTrackData(const uint8_t* pRaw, uint8_t* pDest, uint16_t track)
{
    uint16_t dataptr = 0;  // Offset in raw track array
    uint16_t destptr = 0;  // Offset in data array
    // Skip gap and sync at the track start
    while (dataptr < FLOPPY_RAWTRACKSIZE && pRaw[dataptr] == 0x4e) dataptr++;
    while (dataptr < FLOPPY_RAWTRACKSIZE && (pRaw[dataptr] == 0x00 || pRaw[dataptr] == 0xC2)) dataptr++;
    for (;;)
    {
        if (dataptr >= FLOPPY_RAWTRACKSIZE) break;  // End of track or error
        while (dataptr < FLOPPY_RAWTRACKSIZE && pRaw[dataptr] == 0x4e) dataptr++;  // Skip sector gap
        if (dataptr >= FLOPPY_RAWTRACKSIZE) break;  // End of track or error
        while (dataptr < FLOPPY_RAWTRACKSIZE && pRaw[dataptr] == 0x00) dataptr++;  // Skip sync
        if (dataptr >= FLOPPY_RAWTRACKSIZE) return false;  // Something wrong

        if (dataptr < FLOPPY_RAWTRACKSIZE && pRaw[dataptr] == 0xa1) dataptr++;
        if (dataptr < FLOPPY_RAWTRACKSIZE && pRaw[dataptr] == 0xa1) dataptr++;
        if (dataptr < FLOPPY_RAWTRACKSIZE && pRaw[dataptr] == 0xa1) dataptr++;
        if (dataptr >= FLOPPY_RAWTRACKSIZE) return false;  // Something wrong
        if (pRaw[dataptr++] != 0xfe)
            return false;  // Marker not found

        uint8_t sectcyl, secthd, sectsec, sectno = 0;
        if (dataptr < FLOPPY_RAWTRACKSIZE) sectcyl = pRaw[dataptr++];
        if (dataptr < FLOPPY_RAWTRACKSIZE) secthd = pRaw[dataptr++];
        if (dataptr < FLOPPY_RAWTRACKSIZE) sectsec = pRaw[dataptr++];
        if (dataptr < FLOPPY_RAWTRACKSIZE) sectno = pRaw[dataptr++];
        if (dataptr >= FLOPPY_RAWTRACKSIZE) return false;  // Something wrong

        int sectorsize;
        if (sectno == 1) sectorsize = 256;
        else if (sectno == 2) sectorsize = 512;
        else if (sectno == 3) sectorsize = 1024;
        else return false;  // Something wrong: unknown sector size
        // crc
        if (dataptr < FLOPPY_RAWTRACKSIZE) dataptr++;
        if (dataptr < FLOPPY_RAWTRACKSIZE) dataptr++;

        while (dataptr < FLOPPY_RAWTRACKSIZE && pRaw[dataptr] == 0x4e) dataptr++;  // Skip GAP2
        if (dataptr >= FLOPPY_RAWTRACKSIZE) return false;  // Something wrong
        while (dataptr < FLOPPY_RAWTRACKSIZE && pRaw[dataptr] == 0x00) dataptr++;  // Skip sync
        if (dataptr >= FLOPPY_RAWTRACKSIZE) return false;  // Something wrong

        if (dataptr < FLOPPY_RAWTRACKSIZE && pRaw[dataptr] == 0xa1) dataptr++;
        if (dataptr < FLOPPY_RAWTRACKSIZE && pRaw[dataptr] == 0xa1) dataptr++;
        if (dataptr < FLOPPY_RAWTRACKSIZE && pRaw[dataptr] == 0xa1) dataptr++;
        if (dataptr >= FLOPPY_RAWTRACKSIZE) return false;  // Something wrong
        if (pRaw[dataptr++] != 0xfb)
            return false;  // Marker not found

        for (int count = 0; count < sectorsize; count++)  // Copy sector data
        {
            if (destptr >= 5120) break;  // End of track or error
            pDest[destptr++] = pRaw[dataptr++];
            if (dataptr >= FLOPPY_RAWTRACKSIZE)
                return false;  // Something wrong
        }
        if (dataptr >= FLOPPY_RAWTRACKSIZE)
            return false;  // Something wrong
        // crc
        if (dataptr < FLOPPY_RAWTRACKSIZE) dataptr++;
        if (dataptr < FLOPPY_RAWTRACKSIZE) dataptr++;
    }

    return true;
}


//////////////////////////////////////////////////////////////////////
