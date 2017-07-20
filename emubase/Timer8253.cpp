/*  This file is part of MS0515BTL.
MS0515BTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
MS0515BTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
You should have received a copy of the GNU Lesser General Public License along with
MS0515BTL. If not, see <http://www.gnu.org/licenses/>. */

// Timer8253.cpp
//
// * https://ru.wikipedia.org/wiki/%D0%9A%D0%A0580%D0%92%D0%9853 -- ÊÐ580ÂÈ53
// * https://en.wikipedia.org/wiki/Intel_8253

#include "stdafx.h"
#include "Emubase.h"


//////////////////////////////////////////////////////////////////////

CTimer8253::CTimer8253()
{
    ::memset(m_timers, 0, sizeof(m_timers));
}

void CTimer8253::Reset()
{
    ::memset(m_timers, 0, sizeof(m_timers));
}

void CTimer8253::WriteCommand(uint8_t command)
{
    int channel = (command >> 6) & 3;
    if (channel = 3) channel = 2;  // channel = 0..2
    int operation = (command >> 4) & 3;
    int mode = (command >> 1) & 7;

    //TODO
}

uint8_t CTimer8253::ReadCommand()
{
    return 0; //STUB
}

uint8_t CTimer8253::Read(int channel)
{
    if (channel < 0 || channel > 2)
        return 0;

    return 0; //STUB
}

void CTimer8253::Write(int channel, uint8_t value)
{
    if (channel < 0 || channel > 2)
        return;

    //TODO
}

void CTimer8253::ClockTick()
{
    //TODO
}


//////////////////////////////////////////////////////////////////////
