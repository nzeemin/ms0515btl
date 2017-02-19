/*  This file is part of NEMIGABTL.
NEMIGABTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
NEMIGABTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
You should have received a copy of the GNU Lesser General Public License along with
NEMIGABTL. If not, see <http://www.gnu.org/licenses/>. */

// Keyboard.cpp
//

#include "stdafx.h"
#include "Emubase.h"

//////////////////////////////////////////////////////////////////////

CKeyboard::CKeyboard()
{
    m_nQueueLength = 0;
    m_nTxCounter = 0;
}
CKeyboard::~CKeyboard()
{
}

void CKeyboard::Reset()
{
    m_nQueueLength = 0;
    m_nTxCounter = 0;

    PutByteToQueue(1);
    PutByteToQueue(0);
    PutByteToQueue(0);
    PutByteToQueue(0);
}

bool CKeyboard::HasByteReady() const
{
    return (m_nTxCounter == 0) && (m_nQueueLength > 0);
}

void CKeyboard::Periodic()
{
    if (m_nTxCounter > 0)
        m_nTxCounter--;
}

void CKeyboard::KeyPressed(uint8_t scan)
{
    PutByteToQueue(scan);
}

uint8_t CKeyboard::ReceiveByte()
{
    if (m_nQueueLength == 0)
        return 0;
    uint8_t byte = m_Queue[0];
    m_nQueueLength--;
    if (m_nQueueLength > 0)
        memmove(m_Queue, m_Queue + 1, m_nQueueLength);
    m_nTxCounter = 8;
    return byte;
}

void CKeyboard::PutByteToQueue(uint8_t byte)
{
    if (m_nQueueLength == KEYBOARD_QUEUELENGTH)
        return;  // Output buffer overflow

    m_Queue[m_nQueueLength++] = byte;
}

void CKeyboard::SendByte(uint8_t byte)
{
    switch (byte)
    {
    case 0357:
        Reset();
        break;
    case 0253:
        PutByteToQueue(1);
        PutByteToQueue(0);
        break;
    }
}


//////////////////////////////////////////////////////////////////////
