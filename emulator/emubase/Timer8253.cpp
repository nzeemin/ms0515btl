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
// * https://ru.wikipedia.org/wiki/%D0%9A%D0%A0580%D0%92%D0%9853 -- КР580ВИ53
// * https://en.wikipedia.org/wiki/Intel_8253

#include "stdafx.h"
#include "Emubase.h"


void simulate(CTimerChannel* timer);
void decrease_counter_value(CTimerChannel* timer);
uint16_t masked_value(CTimerChannel* timer);
void load_count(CTimerChannel* timer, uint16_t newcount);
void load_counter_value(CTimerChannel* timer);
void set_output(CTimerChannel* timer, int output);

#define CTRL_ACCESS(control)        (((control) >> 4) & 0x03)
#define CTRL_MODE(control)          (((control) >> 1) & (((control) & 0x04) ? 0x03 : 0x07))
#define CTRL_BCD(control)           (((control) >> 0) & 0x01)

//////////////////////////////////////////////////////////////////////

CTimer8253::CTimer8253()
{
    ::memset(m_timers, 0, sizeof(m_timers));
    for (int i = 0; i < 3; i++)
    {
        m_timers[i].control = m_timers[i].status = 0x30;
        m_timers[i].gate = 1;
    }
}

void CTimer8253::SetGate(int channel, bool gate)
{
    if (channel < 0 || channel > 2)
        return;
    CTimerChannel * timer = m_timers + channel;
    timer->gate = gate ? 1 : 0;
}

bool CTimer8253::GetOutput(int channel)
{
    if (channel < 0 || channel > 2)
        return 0;
    CTimerChannel * timer = m_timers + channel;
    return (timer->output != 0);
}

void CTimer8253::Reset()
{
}

void CTimer8253::WriteCommand(uint8_t data)
{
    int channel = (data >> 6) & 3;
    if (channel == 3) channel = 2;  // channel = 0..2
    CTimerChannel * timer = m_timers + channel;

    if (CTRL_ACCESS(data) == 0)
    {
        //
    }
    else
    {
        timer->control = (data & 0x3f);
        timer->null_count = 1;
        timer->wmsb = timer->rmsb = 0;
        // Phase 0 is always the phase after a mode control write
        timer->phase = 0;
        set_output(timer, CTRL_MODE(timer->control) ? 1 : 0);
    }
}

uint8_t CTimer8253::ReadCommand()
{
    return 0;  // Reading mode control register is illegal according to docs
}

uint8_t CTimer8253::Read(int channel)
{
    if (channel < 0 || channel > 2)
        return 0;
    CTimerChannel * timer = m_timers + channel;

    uint8_t data;
    if (timer->latched_count != 0)  // Read back latched count
    {
        data = (timer->latch >> (timer->rmsb ? 8 : 0)) & 0xff;
        timer->rmsb = 1 - timer->rmsb;
        --timer->latched_count;
        return data;
    }
    else
    {
        uint16_t value = masked_value(timer);

        /* Read back current count */
        switch (CTRL_ACCESS(timer->control))
        {
        case 0:
        default:  // This should never happen
            data = 0; /* Appease compiler */
            break;
        case 1:   // read counter bits 0-7 only
            data = (value >> 0) & 0xff;
            break;
        case 2:  // read counter bits 8-15 only
            data = (value >> 8) & 0xff;
            break;
        case 3:  // read bits 0-7 first, then 8-15
            // reading back the current count while in the middle of a
            // 16-bit write returns a xor'ed version of the value written
            // (apricot diagnostic timer test tests this)
            if (timer->wmsb)
                data = ~timer->lowcount;
            else
                data = (uint8_t)(value >> (timer->rmsb ? 8 : 0));
            timer->rmsb = 1 - timer->rmsb;
            break;
        }
    }

    return data;
}

void CTimer8253::Write(int channel, uint8_t data)
{
    if (channel < 0 || channel > 2)
        return;
    CTimerChannel * timer = m_timers + channel;

    switch (CTRL_ACCESS(timer->control))
    {
    case 0:   // This should never happen
        break;
    case 1:   // read/write counter bits 0-7 only
        load_count(timer, data);
        //TODO
        break;
    case 2:  // read/write counter bits 8-15 only
        load_count(timer, data << 8);
        //TODO
        break;
    case 3:  // read/write bits 0-7 first, then 8-15
        if (timer->wmsb)
        {
            load_count(timer, timer->lowcount | (data << 8));
            //TODO
        }
        else
        {
            timer->lowcount = data;
            if (CTRL_MODE(timer->control) == 0)
            {
                timer->phase = 0;
                set_output(timer, 0);
            }
        }
        timer->wmsb = 1 - timer->wmsb;
        break;
    }
}

void CTimer8253::ClockTick()
{
    for (int i = 0; i < 3; i++)
    {
        CTimerChannel* timer = m_timers + i;
        simulate(timer);
    }
}

void simulate(CTimerChannel* timer)
{
    int mode = CTRL_MODE(timer->control);
    switch (mode)
    {
    case 0:  // Mode 0: (Interrupt on Terminal Count)
        if (timer->phase == 0)
            break;
        if (timer->phase == 1)
        {
            timer->phase = 2;
            load_counter_value(timer);
        }
        //TODO: if (timer->gate == 0)
        if (timer->phase == 2)
        {
            timer->phase = 3;
            timer->value = 0;
            set_output(timer, 1);
        }
        decrease_counter_value(timer);
        break;
    case 1:  // Mode 1: (Hardware Retriggerable One-Shot a.k.a. Programmable One-Shot)
        //TODO
        break;
    case 2:  // Mode 2: (Rate Generator)
        //TODO
        break;
    case 3:  // Mode 3: (Square Wave Generator)
        if (timer->gate == 0 || timer->phase == 0)
        {
            set_output(timer, 1);
        }
        else
        {
            if (timer->phase == 1 && timer->value == 0)
            {
                timer->phase = 2;
                load_counter_value(timer);
            }
            if (timer->phase == 2 && timer->value == 0)
            {
                timer->phase = 3;
                load_counter_value(timer);
                set_output(timer, 0);
            }
            if (timer->phase == 3 && timer->value == 0)
            {
                timer->phase = 2;
                load_counter_value(timer);
                set_output(timer, 1);
            }

            decrease_counter_value(timer);
            if (timer->value > 0) decrease_counter_value(timer);
        }
        break;
    case 4:  // Mode 4: (Software Trigger Strobe)
    case 5:  // Mode 5: (Hardware Trigger Strobe)
        //TODO
        break;
    }
}

void decrease_counter_value(CTimerChannel* timer)
{
    if (CTRL_BCD(timer->control) == 0)
    {
        timer->value--;
        return;
    }

    // BCD decrement
    uint16_t value = timer->value;
    uint8_t units = value & 0xf;
    uint8_t tens = (value >> 4) & 0xf;
    uint8_t hundreds = (value >> 8) & 0xf;
    uint8_t thousands = (value >> 12) & 0xf;
    units--;
    if (units == 255)
    {
        units = 9;  tens--;
        if (tens == 255)
        {
            tens = 9;  hundreds--;
            if (hundreds == 255)
            {
                hundreds = 9;  thousands--;
                if (thousands == 255)
                    thousands = 9;
            }
        }
    }
    timer->value = (thousands << 12) | (hundreds << 8) | (tens << 4) | units;
}

uint16_t masked_value(CTimerChannel* timer)
{
    if ((CTRL_MODE(timer->control) == 3) /*&& (m_type != FE2010)*/)
        return timer->value & 0xfffe;
    return timer->value;
}

void load_count(CTimerChannel* timer, uint16_t newcount)
{
    int mode = CTRL_MODE(timer->control);

    if (newcount == 1)
    {
        // Count of 1 is illegal in modes 2 and 3. What happens here was determined experimentally.
        if (mode == 2)
            newcount = 2;
        if (mode == 3)
            newcount = 0;
    }

    timer->count = newcount;

    if (mode == 2 || mode == 3)
    {
        if (timer->phase == 0)
            timer->phase = 1;
    }
    else
    {
        if (mode == 0 || mode == 4)
            timer->phase = 1;
    }
}

void load_counter_value(CTimerChannel* timer)
{
    timer->value = timer->count;
    timer->null_count = 0;

    if (CTRL_MODE(timer->control) == 3 && timer->output == 0)
        timer->value &= 0xfffe;
}

void set_output(CTimerChannel* timer, int output)
{
    if (output == timer->output)
        return;

    timer->output = output;

    //TODO: fire events
}


//////////////////////////////////////////////////////////////////////
