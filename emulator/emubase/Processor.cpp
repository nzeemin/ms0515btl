/*  This file is part of MS0515BTL.
    MS0515BTL is free software: you can redistribute it and/or modify it under the terms
of the GNU Lesser General Public License as published by the Free Software Foundation,
either version 3 of the License, or (at your option) any later version.
    MS0515BTL is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
See the GNU Lesser General Public License for more details.
    You should have received a copy of the GNU Lesser General Public License along with
MS0515BTL. If not, see <http://www.gnu.org/licenses/>. */

// Processor.cpp
//

#include "stdafx.h"
#include "Processor.h"


// Timings ///////////////////////////////////////////////////////////
// See T11_UsersMan.pdf page A-14 "DCTll-AA Instruction Execution Times at Maximum Operating Frequency"

// See Table A-17 XOR and Single-Operand Instructions
const int TIMING_ONE[8] = { 12, 21, 21, 27, 24, 31, 31, 37 };  // CLR(B), COM(B) etc.
const int TIMING_TST[8] = { 12, 18, 18, 24, 21, 28, 28, 34 };
const int TIMING_MTPS[8] = { 24, 30, 30, 36, 33, 40, 40, 46 };
// See Table A-18 Double-Operand Instructions
const int TIMING_MOV_SRC_DST04[8] = {  9, 15, 15, 21, 18, 25, 25, 31 };  // MOV, CMP, ADD, SUB etc. by src mode, for dst mode 0..4
const int TIMING_MOV_SRC_DST57[8] = { 10, 16, 16, 22, 19, 25, 25, 31 };  // MOV, CMP, ADD, SUB etc. by src mode, for dst mode 5..7
const int TIMING_MOV_DST[8]       = {  3, 12, 12, 18, 15, 21, 21, 27 };  // MOV, ADD, SUB, BIC, BIS by dst mode
const int TIMING_CMP_DST[8]       = {  3,  9,  9, 15, 12, 18, 18, 24 };  // CMP, BIT by dst mode
// See Table A-19 Jump and Subroutine Instructions
const int TIMING_JMP[8] = { 0, 15, 18, 18, 18, 22, 22, 20 };
const int TIMING_JSR[8] = { 0, 27, 30, 30, 30, 34, 34, 40 };
const int TIMING_RTS    =   21;  // 2.80 us
const int TIMING_SOB    =   18;  // 2.40 us
// See Table A-20 Branch, Trap, and Interrupt Instructions
const int TIMING_BRANCH =   12;  // 1.60 us - BR, BEQ etc. - See Table A-20 Branch, Trap, and Interrupt Instructions
const int TIMING_EMT    =   49;  // 6.53 us - IOT, BPT, EMT, TRAP
const int TIMING_RTI    =   24;  // 3.20 us
const int TIMING_RTT    =   33;  // 4.40 us
// See Table A-21 Miscellaneous and Condition Code Instructions
const int TIMING_HALT   =   43;  // 5.73 us
const int TIMING_WAIT   =   12;  // 1.60 us
const int TIMING_RESET  =  110;  // 14.60 us
const int TIMING_NOP    =   18;  // 2.40 us - NOP, CLC, ..., CCC, SEC, ..., SCC
const int TIMING_MFPT   =   15;  // 2.00 us

#define TIMING_MOV(m_methsrc, m_methdest) \
    (((m_methdest) < 5 ? TIMING_MOV_SRC_DST04[m_methsrc] : TIMING_MOV_SRC_DST57[m_methsrc]) + TIMING_MOV_DST[m_methdest])
#define TIMING_CMP(m_methsrc, m_methdest) \
    (((m_methdest) < 5 ? TIMING_MOV_SRC_DST04[m_methsrc] : TIMING_MOV_SRC_DST57[m_methsrc]) + TIMING_CMP_DST[m_methdest])

const int TIMING_ILLEGAL = 110; //TODO


//////////////////////////////////////////////////////////////////////


CProcessor::ExecuteMethodRef* CProcessor::m_pExecuteMethodMap = nullptr;

void CProcessor::Init()
{
    ASSERT(m_pExecuteMethodMap == nullptr);
    m_pExecuteMethodMap = static_cast<CProcessor::ExecuteMethodRef*>(::calloc(65536, sizeof(CProcessor::ExecuteMethodRef)));

    // Сначала заполняем таблицу ссылками на метод ExecuteUNKNOWN
    RegisterMethodRef( 0000000, 0177777, &CProcessor::ExecuteUNKNOWN );

    RegisterMethodRef( 0000000, 0000000, &CProcessor::ExecuteHALT );
    RegisterMethodRef( 0000001, 0000001, &CProcessor::ExecuteWAIT );
    RegisterMethodRef( 0000002, 0000002, &CProcessor::ExecuteRTI );
    RegisterMethodRef( 0000003, 0000003, &CProcessor::ExecuteBPT );
    RegisterMethodRef( 0000004, 0000004, &CProcessor::ExecuteIOT );
    RegisterMethodRef( 0000005, 0000005, &CProcessor::ExecuteRESET );
    RegisterMethodRef( 0000006, 0000006, &CProcessor::ExecuteRTT );
    RegisterMethodRef( 0000007, 0000007, &CProcessor::ExecuteMFPT );
    // RESERVED:       0000010, 0000077
    RegisterMethodRef( 0000100, 0000177, &CProcessor::ExecuteJMP );
    RegisterMethodRef( 0000200, 0000207, &CProcessor::ExecuteRTS );  // RTS / RETURN
    // RESERVED:       0000210, 0000227
    // SPL             0000230, 0000237
    RegisterMethodRef( 0000240, 0000257, &CProcessor::ExecuteCCC );
    RegisterMethodRef( 0000260, 0000277, &CProcessor::ExecuteSCC );
    RegisterMethodRef( 0000300, 0000377, &CProcessor::ExecuteSWAB );
    RegisterMethodRef( 0000400, 0000777, &CProcessor::ExecuteBR );
    RegisterMethodRef( 0001000, 0001377, &CProcessor::ExecuteBNE );
    RegisterMethodRef( 0001400, 0001777, &CProcessor::ExecuteBEQ );
    RegisterMethodRef( 0002000, 0002377, &CProcessor::ExecuteBGE );
    RegisterMethodRef( 0002400, 0002777, &CProcessor::ExecuteBLT );
    RegisterMethodRef( 0003000, 0003377, &CProcessor::ExecuteBGT );
    RegisterMethodRef( 0003400, 0003777, &CProcessor::ExecuteBLE );
    RegisterMethodRef( 0004000, 0004777, &CProcessor::ExecuteJSR );  // JSR / CALL
    RegisterMethodRef( 0005000, 0005077, &CProcessor::ExecuteCLR );
    RegisterMethodRef( 0005100, 0005177, &CProcessor::ExecuteCOM );
    RegisterMethodRef( 0005200, 0005277, &CProcessor::ExecuteINC );
    RegisterMethodRef( 0005300, 0005377, &CProcessor::ExecuteDEC );
    RegisterMethodRef( 0005400, 0005477, &CProcessor::ExecuteNEG );
    RegisterMethodRef( 0005500, 0005577, &CProcessor::ExecuteADC );
    RegisterMethodRef( 0005600, 0005677, &CProcessor::ExecuteSBC );
    RegisterMethodRef( 0005700, 0005777, &CProcessor::ExecuteTST );
    RegisterMethodRef( 0006000, 0006077, &CProcessor::ExecuteROR );
    RegisterMethodRef( 0006100, 0006177, &CProcessor::ExecuteROL );
    RegisterMethodRef( 0006200, 0006277, &CProcessor::ExecuteASR );
    RegisterMethodRef( 0006300, 0006377, &CProcessor::ExecuteASL );
    // MARK            0006400, 0006477
    // MFPI            0006500, 0006577
    // MTPI            0006600, 0006677
    RegisterMethodRef( 0006700, 0006777, &CProcessor::ExecuteSXT );
    // RESERVED:       0007000, 0007777
    RegisterMethodRef( 0010000, 0017777, &CProcessor::ExecuteMOV );
    RegisterMethodRef( 0020000, 0027777, &CProcessor::ExecuteCMP );
    RegisterMethodRef( 0030000, 0037777, &CProcessor::ExecuteBIT );
    RegisterMethodRef( 0040000, 0047777, &CProcessor::ExecuteBIC );
    RegisterMethodRef( 0050000, 0057777, &CProcessor::ExecuteBIS );
    RegisterMethodRef( 0060000, 0067777, &CProcessor::ExecuteADD );
    // MUL             0070000, 0070777
    // DIV             0071000, 0071777
    // ASH             0072000, 0072777
    // ASHC            0073000, 0073777
    RegisterMethodRef( 0074000, 0074777, &CProcessor::ExecuteXOR );
    // FADD etc.       0075000, 0075777
    // RESERVED:       0076000, 0076777
    RegisterMethodRef( 0077000, 0077777, &CProcessor::ExecuteSOB );
    RegisterMethodRef( 0100000, 0100377, &CProcessor::ExecuteBPL );
    RegisterMethodRef( 0100400, 0100777, &CProcessor::ExecuteBMI );
    RegisterMethodRef( 0101000, 0101377, &CProcessor::ExecuteBHI );
    RegisterMethodRef( 0101400, 0101777, &CProcessor::ExecuteBLOS );
    RegisterMethodRef( 0102000, 0102377, &CProcessor::ExecuteBVC );
    RegisterMethodRef( 0102400, 0102777, &CProcessor::ExecuteBVS );
    RegisterMethodRef( 0103000, 0103377, &CProcessor::ExecuteBHIS );  // BCC
    RegisterMethodRef( 0103400, 0103777, &CProcessor::ExecuteBLO );   // BCS
    RegisterMethodRef( 0104000, 0104377, &CProcessor::ExecuteEMT );
    RegisterMethodRef( 0104400, 0104777, &CProcessor::ExecuteTRAP );
    RegisterMethodRef( 0105000, 0105077, &CProcessor::ExecuteCLR );  // CLRB
    RegisterMethodRef( 0105100, 0105177, &CProcessor::ExecuteCOM );  // COMB
    RegisterMethodRef( 0105200, 0105277, &CProcessor::ExecuteINC );  // INCB
    RegisterMethodRef( 0105300, 0105377, &CProcessor::ExecuteDEC );  // DECB
    RegisterMethodRef( 0105400, 0105477, &CProcessor::ExecuteNEG );  // NEGB
    RegisterMethodRef( 0105500, 0105577, &CProcessor::ExecuteADC );  // ADCB
    RegisterMethodRef( 0105600, 0105677, &CProcessor::ExecuteSBC );  // SBCB
    RegisterMethodRef( 0105700, 0105777, &CProcessor::ExecuteTST );  // TSTB
    RegisterMethodRef( 0106000, 0106077, &CProcessor::ExecuteROR );  // RORB
    RegisterMethodRef( 0106100, 0106177, &CProcessor::ExecuteROL );  // ROLB
    RegisterMethodRef( 0106200, 0106277, &CProcessor::ExecuteASR );  // ASRB
    RegisterMethodRef( 0106300, 0106377, &CProcessor::ExecuteASL );  // ASLB
    RegisterMethodRef( 0106400, 0106477, &CProcessor::ExecuteMTPS );
    // MFPD            0106500, 0106577
    // MTPD            0106600, 0106677
    RegisterMethodRef( 0106700, 0106777, &CProcessor::ExecuteMFPS );
    RegisterMethodRef( 0110000, 0117777, &CProcessor::ExecuteMOV );  // MOVB
    RegisterMethodRef( 0120000, 0127777, &CProcessor::ExecuteCMP );  // CMPB
    RegisterMethodRef( 0130000, 0137777, &CProcessor::ExecuteBIT );  // BITB
    RegisterMethodRef( 0140000, 0147777, &CProcessor::ExecuteBIC );  // BICB
    RegisterMethodRef( 0150000, 0157777, &CProcessor::ExecuteBIS );  // BISB
    RegisterMethodRef( 0160000, 0167777, &CProcessor::ExecuteSUB );
    // FPP             0170000, 0177777
}

void CProcessor::Done()
{
    ::free(m_pExecuteMethodMap);  m_pExecuteMethodMap = nullptr;
}

void CProcessor::RegisterMethodRef(uint16_t start, uint16_t end, CProcessor::ExecuteMethodRef methodref)
{
    for (size_t opcode = start; opcode <= end; opcode++ )
        m_pExecuteMethodMap[opcode] = methodref;
}

//////////////////////////////////////////////////////////////////////


CProcessor::CProcessor(CMotherboard* pBoard)
{
    ASSERT(pBoard != nullptr);
    m_pBoard = pBoard;
    ::memset(m_R, 0, sizeof(m_R));
    m_psw = 0340;
    m_okStopped = true;
    m_internalTick = 0;
    m_waitmode = false;
    m_stepmode = false;
    m_RPLYrq = m_RSVDrq = m_RSVD4rq = m_TBITrq = m_HALTrq = m_RPL2rq = m_IRQ5rq = m_IRQ2rq = m_IRQ11rq = false;
    m_BPT_rq = m_IOT_rq = m_EMT_rq = m_TRAPrq = false;
    m_virqrq = 0;

    m_instruction = m_instructionpc = 0;
    m_regsrc = m_methsrc = 0;
    m_regdest = m_methdest = 0;
    m_addrsrc = m_addrdest = 0;
    memset(m_virq, 0, sizeof(m_virq));
}

void CProcessor::Start()
{
    m_okStopped = false;
    m_stepmode = false;
    m_waitmode = false;
    m_RPLYrq = m_RSVDrq = m_RSVD4rq = m_TBITrq = m_HALTrq = m_RPL2rq = m_IRQ5rq = m_IRQ2rq = m_IRQ11rq = false;
    m_BPT_rq = m_IOT_rq = m_EMT_rq = m_TRAPrq = false;
    m_virqrq = 0;  memset(m_virq, 0, sizeof(m_virq));

    // "Turn On" interrupt processing
    uint16_t pc = 0172000;
    SetPC(pc);
    SetPSW(0340);
    SetSP(376);
    m_internalTick = 1000000;  // Количество тактов на включение процессора (значение с потолка)
}
void CProcessor::Stop()
{
    m_okStopped = true;

    m_stepmode = false;
    m_waitmode = false;
    m_psw = 0340;
    m_internalTick = 0;
    m_RPLYrq = m_RSVDrq = m_RSVD4rq = m_TBITrq = m_HALTrq = m_RPL2rq = m_IRQ5rq = m_IRQ2rq = m_IRQ11rq = false;
    m_BPT_rq = m_IOT_rq = m_EMT_rq = m_TRAPrq = false;
    m_virqrq = 0;  memset(m_virq, 0, sizeof(m_virq));
}

void CProcessor::Execute()
{
    if (m_okStopped) return;  // Processor is stopped - nothing to do

    if (m_internalTick > 0)
    {
        m_internalTick--;
        return;
    }
    m_internalTick = TIMING_ILLEGAL;  // ANYTHING UNKNOWN

    m_RPLYrq = false;

    if (!m_waitmode)
    {
        m_instructionpc = m_R[7];  // Store address of the current instruction
        FetchInstruction();  // Read next instruction from memory
        if (!m_RPLYrq)
        {
            TranslateInstruction();  // Execute next instruction
            if (m_internalTick > 0) m_internalTick--;  // Count current tick too
        }
    }

    if (m_stepmode)
        m_stepmode = false;
    else if (m_instruction == PI_RTT && (GetPSW() & PSW_T))
    {
        // Skip interrupt processing for RTT with T bit set
    }
    else  // Processing interrupts
    {
        for (;;)
        {
            m_TBITrq = (m_psw & 020) != 0;  // T-bit

            // Calculate interrupt vector and mode according to priority
            uint16_t intrVector = 0;
            int priority = (m_psw & 0340) >> 5;  // Priority: 0..7
            if (m_HALTrq)  // HALT command
            {
                //intrVector = 0172004;  intrMode = true;
                m_HALTrq = false;

                DebugLogFormat(_T("HALT interrupt at PC=%06o\r\n"), m_instructionpc);

                // Save PC/PSW to stack
                SetSP(GetSP() - 2);
                SetWord(GetSP(), m_psw);
                SetSP(GetSP() - 2);
                SetWord(GetSP(), GetPC());
                // Restart
                SetPC(0172004);
                SetPSW(0340);
                break;
            }
            else if (m_BPT_rq)  // BPT command
            {
                intrVector = 0000014;  m_BPT_rq = false;
            }
            else if (m_IOT_rq)  // IOT command
            {
                intrVector = 0000020;  m_IOT_rq = false;
            }
            else if (m_EMT_rq)  // EMT command
            {
                intrVector = 0000030;  m_EMT_rq = false;
            }
            else if (m_TRAPrq)  // TRAP command
            {
                intrVector = 0000034;  m_TRAPrq = false;
            }
            else if (m_RPLYrq)  // Зависание
            {
                intrVector = 000000;  m_RPLYrq = false;
            }
            else if (m_RSVD4rq)  // Reserved instruction trap: JMP / JSR wrong mode
            {
                intrVector = 000010;  m_RSVD4rq = false;
            }
            else if (m_RSVDrq)  // Reserved instruction trap: illegal and reserved instruction
            {
                intrVector = 000010;  m_RSVDrq = false;
            }
            else if (m_TBITrq && (!m_waitmode))  // T-bit
            {
                intrVector = 000014;  m_TBITrq = false;
            }
            else if (m_IRQ5rq && priority < 5)  // Keyboard 7004 interrupt, priority 5
            {
                intrVector = 000130;  m_IRQ5rq = false;
            }
            else if (m_IRQ2rq && priority < 4)  // Vblank interrupt, priority 4
            {
                intrVector = 000064;  m_IRQ2rq = false;
            }
            else if (m_IRQ11rq && priority < 6)  // Timer interrupt, priority 6
            {
                intrVector = 000100;  m_IRQ11rq = false;
            }
            else if (m_virqrq > 0 && (m_psw & 0200) != 0200)  // VIRQ
            {
                for (int irq = 0; irq <= 15; irq++)
                {
                    if (m_virq[irq] != 0)
                    {
                        intrVector = m_virq[irq];
                        m_virq[irq] = 0;
                        m_virqrq--;
                        break;
                    }
                }
                if (intrVector == 0) m_virqrq = 0;
            }

            if (intrVector == 0)
                break;  // No more unmasked interrupts

            m_waitmode = false;

            uint16_t oldpsw = m_psw;

            // Save PC/PSW to stack
            SetSP(GetSP() - 2);
            SetWord(GetSP(), oldpsw);
            SetSP(GetSP() - 2);
            SetWord(GetSP(), GetPC());

            SetPC(GetWord(intrVector));
            m_psw = GetWord(intrVector + 2) & 0377;
        }  // end while
    }
}

void CProcessor::InterruptVIRQ(int que, uint16_t interrupt)
{
    if (m_okStopped) return;  // Processor is stopped - nothing to do
    // if (m_virqrq == 1)
    // {
    //  DebugPrintFormat(_T("Lost VIRQ %d %d\r\n"), m_virq, interrupt);
    // }
    m_virqrq += 1;
    m_virq[que] = interrupt;
}

void CProcessor::MemoryError()
{
    m_RPLYrq = true;
}


//////////////////////////////////////////////////////////////////////


uint8_t CProcessor::GetByteSrc()
{
    if (m_methsrc == 0)
        return static_cast<uint8_t>(GetReg(m_regsrc)) & 0377;
    else
        return GetByte( m_addrsrc );
}
uint8_t CProcessor::GetByteDest()
{
    if (m_methdest == 0)
        return static_cast<uint8_t>(GetReg(m_regdest));
    else
        return GetByte( m_addrdest );
}

void CProcessor::SetByteDest(uint8_t byte)
{
    if (m_methdest == 0)
    {
        if (byte & 0200)
            SetReg(m_regdest, 0xff00 | byte);
        else
            SetReg(m_regdest, (GetReg(m_regdest) & 0xff00) | byte);
    }
    else
        SetByte( m_addrdest, byte );
}

uint16_t CProcessor::GetWordSrc ()
{
    if (m_methsrc == 0)
        return GetReg(m_regsrc);
    else
        return GetWord( m_addrsrc );
}
uint16_t CProcessor::GetWordDest ()
{
    if (m_methdest == 0)
        return GetReg(m_regdest);
    else
        return GetWord( m_addrdest );
}

void CProcessor::SetWordDest (uint16_t word)
{
    if (m_methdest == 0)
        SetReg(m_regdest, word);
    else
        SetWord( (m_addrdest), word );
}

uint16_t CProcessor::GetDstWordArgAsBranch ()
{
    int reg = GetDigit(m_instruction, 0);
    int meth = GetDigit(m_instruction, 1);
    uint16_t arg;

    switch (meth)
    {
    case 0:  // R0,     PC
        ASSERT(0);
        return 0;
    case 1:  // (R0),   (PC)
        return GetReg(reg);
    case 2:  // (R0)+,  #012345
        arg = GetReg(reg);
        SetReg(reg, GetReg(reg) + 2);
        return arg;
    case 3:  // @(R0)+, @#012345
        arg = GetWord( GetReg(reg) );
        SetReg(reg, GetReg(reg) + 2);
        return arg;
    case 4:  // -(R0),  -(PC)
        SetReg(reg, GetReg(reg) - 2);
        return GetReg(reg);
    case 5:  // @-(R0), @-(PC)
        SetReg(reg, GetReg(reg) - 2);
        return GetWord( GetReg(reg) );
    case 6:    // 345(R0),  345
        {
            uint16_t pc = GetWordExec( GetPC() );
            SetPC( GetPC() + 2 );
            return static_cast<uint16_t>(pc + GetReg(reg));
        }
    case 7:    // @345(R0),@345
        {
            uint16_t pc = GetWordExec( GetPC() );
            SetPC( GetPC() + 2 );
            return GetWord( static_cast<uint16_t>(pc + GetReg(reg)) );
        }
    }

    return 0;
}


//////////////////////////////////////////////////////////////////////

//static bool TraceStarted = true;//DEBUG
void CProcessor::FetchInstruction()
{
    // Считываем очередную инструкцию
    uint16_t pc = GetPC();
    pc = pc & ~1;

    m_instruction = GetWordExec(pc);
    SetPC(GetPC() + 2);

//#if !defined(PRODUCT)
//    uint16_t address = GetPC() - 2;
//    const uint16_t TraceStartAddress = 0000000;
//    //if (!TraceStarted && address == TraceStartAddress) TraceStarted = true;
//    if (TraceStarted /*&& address < 0160000*/) {
//        uint16_t data[4];
//        for (int i = 0; i < 4; i++)
//            data[i] = GetWord(address + i * 2);
//        TCHAR strInstr[8];
//        TCHAR strArg[32];
//        DisassembleInstruction(data, address, strInstr, strArg);
//        DebugLogFormat(_T("%06o: %s\t%s\n"), address, strInstr, strArg);
//        //DebugLogFormat(_T("%s %06o: %s\t%s\n"), IsHaltMode()?_T("HALT"):_T("USER"), address, strInstr, strArg);
//    }
//#endif
}

void CProcessor::TranslateInstruction ()
{
    // Prepare values to help decode the command
    m_regdest  = GetDigit(m_instruction, 0);
    m_methdest = GetDigit(m_instruction, 1);
    m_regsrc   = GetDigit(m_instruction, 2);
    m_methsrc  = GetDigit(m_instruction, 3);

    // Find command implementation using the command map
    ExecuteMethodRef methodref = m_pExecuteMethodMap[m_instruction];
    (this->*methodref)();  // Call command implementation method
}

void CProcessor::ExecuteUNKNOWN ()  // Нет такой инструкции - просто вызывается TRAP 10
{
    DebugLogFormat(_T("CPU: Invalid OPCODE %06o at PC=%06o\r\n"), m_instruction, m_instructionpc);

    m_RSVDrq = true;
}


// Instruction execution /////////////////////////////////////////////

void CProcessor::ExecuteWAIT ()  // WAIT - Wait for an interrupt
{
    m_waitmode = true;

    m_internalTick = TIMING_WAIT;
}

void CProcessor::ExecuteHALT ()  // HALT - Останов
{
    m_HALTrq = true;

    m_internalTick = TIMING_HALT;
}

void CProcessor::ExecuteRTI ()  // RTI - Возврат из прерывания
{
    SetReg(7, GetWord( GetSP() ) );  // Pop PC
    SetSP( GetSP() + 2 );

    uint16_t new_psw = GetWord(GetSP());  // Pop PSW --- saving HALT
    SetPSW(new_psw & 0377);

    SetSP( GetSP() + 2 );
    m_internalTick = TIMING_RTI;
}

void CProcessor::ExecuteBPT ()  // BPT - Breakpoint
{
    m_BPT_rq = true;
    m_internalTick = TIMING_EMT;
}

void CProcessor::ExecuteIOT ()  // IOT - I/O trap
{
    m_IOT_rq = true;
    m_internalTick = TIMING_EMT;
}

void CProcessor::ExecuteRESET ()  // Reset input/output devices
{
    m_pBoard->ResetDevices();  // INIT signal

    m_internalTick = TIMING_RESET;
}

void CProcessor::ExecuteMFPT()
{
    SetReg(0, 4);

    m_internalTick = TIMING_MFPT;
}

void CProcessor::ExecuteRTT ()  // RTT - return from trace trap
{
    SetPC( GetWord( GetSP() ) );  // Pop PC
    SetSP( GetSP() + 2 );

    uint16_t new_psw = GetWord(GetSP());  // Pop PSW --- saving HALT
    SetPSW(new_psw & 0377);

    //m_psw |= PSW_T; // set the trap flag ???
    m_internalTick = TIMING_RTT;
}

void CProcessor::ExecuteRTS ()  // RTS - return from subroutine - Возврат из процедуры
{
    SetPC(GetReg(m_regdest));
    SetReg(m_regdest, GetWord(GetSP()));
    SetSP(GetSP() + 2);

    m_internalTick = TIMING_RTS;
}

void CProcessor::ExecuteNOP ()  // NOP - Нет операции
{
    m_internalTick = TIMING_NOP;
}

void CProcessor::ExecuteCCC ()
{
    SetLPSW(GetLPSW() &  ~(static_cast<uint8_t>(m_instruction & 0xff) & 017));
    m_internalTick = TIMING_NOP;
}
void CProcessor::ExecuteSEZ ()
{
    SetZ(true);
    m_internalTick = TIMING_NOP;
}
void CProcessor::ExecuteSCC ()
{
    SetLPSW(GetLPSW() |  (static_cast<uint8_t>(m_instruction & 0xff) & 017));
    m_internalTick = TIMING_NOP;
}

void CProcessor::ExecuteJMP ()  // JMP - jump: PC = &d (a-mode > 0)
{
    if (m_methdest == 0)  // Неправильный метод адресации
    {
        m_RSVD4rq = true;

        m_internalTick = TIMING_EMT;
    }
    else
    {
        SetPC(GetWordAddr(m_methdest, m_regdest));

        m_internalTick = TIMING_JMP[m_methdest];
    }
}

void CProcessor::ExecuteSWAB ()
{
    uint16_t ea = 0;

    uint16_t dst = m_methdest ? GetWord(ea = GetWordAddr(m_methdest, m_regdest)) : GetReg(m_regdest);

    dst = ((dst >> 8) & 0377) | (dst << 8);

    if (m_methdest)
        SetWord(ea, dst);
    else
        SetReg(m_regdest, dst);

    SetN((dst & 0200) != 0);
    SetZ(!(dst & 0377));
    SetV(false);
    SetC(false);

    m_internalTick = TIMING_ONE[m_methdest];
}

void CProcessor::ExecuteCLR ()  // CLR
{
    if (m_instruction & 0100000)
    {
        SetN(false);
        SetZ(true);
        SetV(false);
        SetC(false);

        if (m_methdest)
            SetByte(GetByteAddr(m_methdest, m_regdest), 0);
        else
            SetReg(m_regdest, (GetReg(m_regdest) & 0177400));

        m_internalTick = TIMING_ONE[m_methdest];
    }
    else
    {
        SetN(false);
        SetZ(true);
        SetV(false);
        SetC(false);

        if (m_methdest)
            SetWord(GetWordAddr(m_methdest, m_regdest), 0);
        else
            SetReg(m_regdest, 0);

        m_internalTick = TIMING_ONE[m_methdest];
    }
}

void CProcessor::ExecuteCOM ()  // COM
{
    uint16_t ea = 0;

    if (m_instruction & 0100000)
    {
        uint8_t dst = m_methdest
                ? GetByte(ea = GetByteAddr(m_methdest, m_regdest))
                : static_cast<uint8_t>(GetReg(m_regdest));

        dst = dst ^ 0377;

        SetN((dst >> 7) != 0);
        SetZ(!dst);
        SetV(false);
        SetC(true);

        if (m_methdest)
            SetByte(ea, dst);
        else
            SetReg(m_regdest, (GetReg(m_regdest) & 0177400) | dst);

        m_internalTick = TIMING_ONE[m_methdest];
    }
    else
    {
        uint16_t dst = m_methdest ? GetWord(ea = GetWordAddr(m_methdest, m_regdest)) : GetReg(m_regdest);

        dst = dst ^ 0177777;

        SetN((dst >> 15) != 0);
        SetZ(!dst);
        SetV(false);
        SetC(true);

        if (m_methdest)
            SetWord(ea, dst);
        else
            SetReg(m_regdest, dst);

        m_internalTick = TIMING_ONE[m_methdest];
    }
}

void CProcessor::ExecuteINC ()  // INC - Инкремент
{
    uint16_t ea = 0;

    if (m_instruction & 0100000)
    {
        uint8_t dst = m_methdest
                ? GetByte(ea = GetByteAddr(m_methdest, m_regdest))
                : static_cast<uint8_t>(GetReg(m_regdest));

        dst = dst + 1;

        SetN((dst >> 7) != 0);
        SetZ(!dst);
        SetV(dst == 0200);

        if (m_methdest)
            SetByte(ea, dst);
        else
            SetReg(m_regdest, (GetReg(m_regdest) & 0177400) | dst);

        m_internalTick = TIMING_ONE[m_methdest];
    }
    else
    {
        uint16_t dst = m_methdest ? GetWord(ea = GetWordAddr(m_methdest, m_regdest)) : GetReg(m_regdest);

        dst = dst + 1;

        SetN((dst >> 15) != 0);
        SetZ(!dst);
        SetV(dst == 0100000);

        if (m_methdest)
            SetWord(ea, dst);
        else
            SetReg(m_regdest, dst);

        m_internalTick = TIMING_ONE[m_methdest];
    }
}

void CProcessor::ExecuteDEC ()  // DEC - Декремент
{
    uint16_t ea = 0;

    if (m_instruction & 0100000)
    {
        uint8_t dst = m_methdest
                ? GetByte(ea = GetByteAddr(m_methdest, m_regdest))
                : static_cast<uint8_t>(GetReg(m_regdest));

        dst = dst - 1;

        SetN((dst >> 7) != 0);
        SetZ(!dst);
        SetV(dst == 0177);

        if (m_methdest)
            SetByte(ea, dst);
        else
            SetReg(m_regdest, (GetReg(m_regdest) & 0177400) | dst);

        m_internalTick = TIMING_ONE[m_methdest];
    }
    else
    {
        uint16_t dst = m_methdest ? GetWord(ea = GetWordAddr(m_methdest, m_regdest)) : GetReg(m_regdest);

        dst = dst - 1;

        SetN((dst >> 15) != 0);
        SetZ(!dst);
        SetV(dst == 077777);

        if (m_methdest)
            SetWord(ea, dst);
        else
            SetReg(m_regdest, dst);

        m_internalTick = TIMING_ONE[m_methdest];
    }
}

void CProcessor::ExecuteNEG ()
{
    uint16_t ea = 0;

    if (m_instruction & 0100000)
    {
        uint8_t dst = m_methdest
                ? GetByte(ea = GetByteAddr(m_methdest, m_regdest))
                : static_cast<uint8_t>(GetReg(m_regdest));

        dst = 0 - dst;

        SetN((dst >> 7) != 0);
        SetZ(!dst);
        SetV(dst == 0200);
        SetC(!GetZ());

        if (m_methdest)
            SetByte(ea, dst);
        else
            SetReg(m_regdest, (GetReg(m_regdest) & 0177400) | dst);

        m_internalTick = TIMING_ONE[m_methdest];
    }
    else
    {
        uint16_t dst = m_methdest ? GetWord(ea = GetWordAddr(m_methdest, m_regdest)) : GetReg(m_regdest);

        dst = 0 - dst;

        SetN((dst >> 15) != 0);
        SetZ(!dst);
        SetV(dst == 0100000);
        SetC(!GetZ());

        if (m_methdest)
            SetWord(ea, dst);
        else
            SetReg(m_regdest, dst);

        m_internalTick = TIMING_ONE[m_methdest];
    }
}

void CProcessor::ExecuteADC ()  // ADC{B}
{
    uint16_t ea = 0;

    if (m_instruction & 0100000)
    {
        uint8_t dst = m_methdest
                ? GetByte(ea = GetByteAddr(m_methdest, m_regdest))
                : static_cast<uint8_t>(GetReg(m_regdest));

        dst = dst + (GetC() ? 1 : 0);

        if (m_methdest)
            SetByte(ea, dst);
        else
            SetReg(m_regdest, (GetReg(m_regdest) & 0177400) | dst);

        SetN((dst >> 7) != 0);
        SetZ(!dst);
        SetV(GetC() && (dst == 0200));
        SetC(GetC() && GetZ());

        m_internalTick = TIMING_ONE[m_methdest];
    }
    else
    {
        uint16_t dst = m_methdest ? GetWord(ea = GetWordAddr(m_methdest, m_regdest)) : GetReg(m_regdest);

        dst = dst + (GetC() ? 1 : 0);

        if (m_methdest)
            SetWord(ea, dst);
        else
            SetReg(m_regdest, dst);

        SetN((dst >> 15) != 0);
        SetZ(!dst);
        SetV(GetC() && (dst == 0100000));
        SetC(GetC() && GetZ());

        m_internalTick = TIMING_ONE[m_methdest];
    }
}

void CProcessor::ExecuteSBC ()  // SBC{B}
{
    uint16_t ea = 0;

    if (m_instruction & 0100000)
    {
        uint8_t dst = m_methdest
                ? GetByte(ea = GetByteAddr(m_methdest, m_regdest))
                : static_cast<uint8_t>(GetReg(m_regdest));

        dst = dst - (GetC() ? 1 : 0);

        SetN((dst >> 7) != 0);
        SetZ(!dst);
        SetV(GetC() && (dst == 0177));
        SetC(GetC() && (dst == 0377));

        if (m_methdest)
            SetByte(ea, dst);
        else
            SetReg(m_regdest, (GetReg(m_regdest) & 0177400) | dst);

        m_internalTick = TIMING_ONE[m_methdest];
    }
    else
    {
        uint16_t dst = m_methdest ? GetWord(ea = GetWordAddr(m_methdest, m_regdest)) : GetReg(m_regdest);

        dst = dst - (GetC() ? 1 : 0);

        SetN((dst >> 15) != 0);
        SetZ(!dst);
        SetV(GetC() && (dst == 077777));
        SetC(GetC() && (dst == 0177777));

        if (m_methdest)
            SetWord(ea, dst);
        else
            SetReg(m_regdest, dst);

        m_internalTick = TIMING_ONE[m_methdest];
    }
}

void CProcessor::ExecuteTST ()  // TST{B} - test
{
    if (m_instruction & 0100000)
    {
        uint8_t dst = m_methdest
                ? GetByte(GetByteAddr(m_methdest, m_regdest))
                : static_cast<uint8_t>(GetReg(m_regdest));

        SetN((dst >> 7) != 0);
        SetZ(!dst);
        SetV(false);
        SetC(false);

        m_internalTick = TIMING_TST[m_methdest];
    }
    else
    {
        uint16_t dst = m_methdest ? GetWord(GetWordAddr(m_methdest, m_regdest)) : GetReg(m_regdest);

        SetN((dst >> 15) != 0);
        SetZ(!dst);
        SetV(false);
        SetC(false);

        m_internalTick = TIMING_TST[m_methdest];
    }
}

void CProcessor::ExecuteROR ()  // ROR{B}
{
    uint16_t ea = 0;

    if (m_instruction & 0100000)
    {
        uint8_t src = m_methdest
                ? GetByte(ea = GetByteAddr(m_methdest, m_regdest))
                : static_cast<uint8_t>(GetReg(m_regdest));
        uint8_t dst = (src >> 1) | (GetC() ? 0200 : 0);

        SetN((dst >> 7) != 0);
        SetZ(!dst);
        SetC(src & 1);
        SetV(GetN() != GetC());

        if (m_methdest)
            SetByte(ea, dst);
        else
            SetReg(m_regdest, (GetReg(m_regdest) & 0177400) | dst);

        m_internalTick = TIMING_ONE[m_methdest];
    }
    else
    {
        uint16_t src = m_methdest ? GetWord(ea = GetWordAddr(m_methdest, m_regdest)) : GetReg(m_regdest);
        uint16_t dst = (src >> 1) | (GetC() ? 0100000 : 0);

        SetN((dst >> 15) != 0);
        SetZ(!dst);
        SetC(src & 1);
        SetV(GetN() != GetC());

        if (m_methdest)
            SetWord(ea, dst);
        else
            SetReg(m_regdest, dst);

        m_internalTick = TIMING_ONE[m_methdest];
    }
}

void CProcessor::ExecuteROL ()  // ROL{B}
{
    uint16_t ea = 0;

    if (m_instruction & 0100000)
    {
        uint8_t src = m_methdest
                ? GetByte(ea = GetByteAddr(m_methdest, m_regdest))
                : static_cast<uint8_t>(GetReg(m_regdest));
        uint8_t dst = (src << 1) | (GetC() ? 1 : 0);

        SetN((dst >> 7) != 0);
        SetZ(!dst);
        SetC((src >> 7) != 0);
        SetV(GetN() != GetC());

        if (m_methdest)
            SetByte(ea, dst);
        else
            SetReg(m_regdest, (GetReg(m_regdest) & 0177400) | dst);

        m_internalTick = TIMING_ONE[m_methdest];
    }
    else
    {
        uint16_t src = m_methdest ? GetWord(ea = GetWordAddr(m_methdest, m_regdest)) : GetReg(m_regdest);
        uint16_t dst = (src << 1) | (GetC() ? 1 : 0);

        SetN((dst >> 15) != 0);
        SetZ(!dst);
        SetC((src >> 15) != 0);
        SetV(GetN() != GetC());

        if (m_methdest)
            SetWord(ea, dst);
        else
            SetReg(m_regdest, dst);

        m_internalTick = TIMING_ONE[m_methdest];
    }
}

void CProcessor::ExecuteASR ()  // ASR{B}
{
    uint16_t ea = 0;

    if (m_instruction & 0100000)
    {
        uint8_t src = m_methdest
                ? GetByte(ea = GetByteAddr(m_methdest, m_regdest))
                : static_cast<uint8_t>(GetReg(m_regdest));
        uint8_t dst = (src >> 1) | (src & 0200);

        SetN((dst >> 7) != 0);
        SetZ(!dst);
        SetC(src & 1);
        SetV(GetN() != GetC());

        if (m_methdest)
            SetByte(ea, dst);
        else
            SetReg(m_regdest, (GetReg(m_regdest) & 0177400) | dst);

        m_internalTick = TIMING_ONE[m_methdest];
    }
    else
    {
        uint16_t src = m_methdest ? GetWord(ea = GetWordAddr(m_methdest, m_regdest)) : GetReg(m_regdest);
        uint16_t dst = (src >> 1) | (src & 0100000);

        SetN((dst >> 15) != 0);
        SetZ(!dst);
        SetC(src & 1);
        SetV(GetN() != GetC());

        if (m_methdest)
            SetWord(ea, dst);
        else
            SetReg(m_regdest, dst);

        m_internalTick = TIMING_ONE[m_methdest];
    }
}

void CProcessor::ExecuteASL ()  // ASL{B}
{
    uint16_t ea = 0;

    if (m_instruction & 0100000)
    {
        uint8_t src = m_methdest
                ? GetByte(ea = GetByteAddr(m_methdest, m_regdest))
                : static_cast<uint8_t>(GetReg(m_regdest));
        uint8_t dst = (src << 1) & 0377;

        SetN((dst >> 7) != 0);
        SetZ(!dst);
        SetC((src >> 7) != 0);
        SetV(GetN() != GetC());

        if (m_methdest)
            SetByte(ea, dst);
        else
            SetReg(m_regdest, (GetReg(m_regdest) & 0177400) | dst);

        m_internalTick = TIMING_ONE[m_methdest];
    }
    else
    {
        uint16_t src = m_methdest ? GetWord(ea = GetWordAddr(m_methdest, m_regdest)) : GetReg(m_regdest);
        uint16_t dst = src << 1;

        SetN((dst >> 15) != 0);
        SetZ(!dst);
        SetC((src >> 15) != 0);
        SetV(GetN() != GetC());

        if (m_methdest)
            SetWord(ea, dst);
        else
            SetReg(m_regdest, dst);

        m_internalTick = TIMING_ONE[m_methdest];
    }
}

void CProcessor::ExecuteSXT ()  // SXT - sign-extend
{
    if (m_methdest)
        SetWord(GetWordAddr(m_methdest, m_regdest), GetN() ? 0177777 : 0);
    else
        SetReg(m_regdest, GetN() ? 0177777 : 0); //sign extend

    SetZ(!GetN());
    SetV(false);

    m_internalTick = TIMING_ONE[m_methdest];
}

void CProcessor::ExecuteMTPS ()  // MTPS - move to PS
{
    uint8_t dst = m_methdest
            ? GetByte(GetByteAddr(m_methdest, m_regdest))
            : static_cast<uint8_t>(GetReg(m_regdest));

    if (GetPSW() & 0400) //in halt?
    {
        //allow everything
        SetPSW((GetPSW() & 0400) | dst);
    }
    else
    {
        SetPSW((GetPSW() & 0420) | (dst & 0357));  // preserve T
    }

    m_internalTick = TIMING_MTPS[m_methdest];
}

void CProcessor::ExecuteMFPS ()  // MFPS - move from PS
{
    uint8_t psw = GetPSW() & 0377;

    if (m_methdest)
        SetByte(GetByteAddr(m_methdest, m_regdest), psw);
    else
        SetReg(m_regdest, (char)psw); //sign extend

    SetN((psw & 0200) != 0);
    SetZ(psw == 0);
    SetV(false);

    m_internalTick = TIMING_ONE[m_methdest];
}

void CProcessor::ExecuteBR ()
{
    SetReg(7, GetPC() + ((short)(char)LOBYTE (m_instruction)) * 2 );

    m_internalTick = TIMING_BRANCH;
}

void CProcessor::ExecuteBNE ()
{
    if (! GetZ())
    {
        SetReg(7, GetPC() + ((short)(char)LOBYTE (m_instruction)) * 2 );
    }

    m_internalTick = TIMING_BRANCH;
}

void CProcessor::ExecuteBEQ ()
{
    if (GetZ())
    {
        SetReg(7, GetPC() + ((short)(char)LOBYTE (m_instruction)) * 2 );
    }

    m_internalTick = TIMING_BRANCH;
}

void CProcessor::ExecuteBGE ()
{
    if (GetN() == GetV())
    {
        SetReg(7, GetPC() + ((short)(char)LOBYTE (m_instruction)) * 2 );
    }

    m_internalTick = TIMING_BRANCH;
}

void CProcessor::ExecuteBLT ()
{
    if (GetN() != GetV())
    {
        SetReg(7, GetPC() + ((short)(char)LOBYTE (m_instruction)) * 2 );
    }

    m_internalTick = TIMING_BRANCH;
}

void CProcessor::ExecuteBGT ()
{
    if (! ((GetN() != GetV()) || GetZ()))
    {
        SetReg(7, GetPC() + ((short)(char)LOBYTE (m_instruction)) * 2 );
    }

    m_internalTick = TIMING_BRANCH;
}

void CProcessor::ExecuteBLE ()
{
    if ((GetN() != GetV()) || GetZ())
    {
        SetReg(7, GetPC() + ((short)(char)LOBYTE (m_instruction)) * 2 );
    }

    m_internalTick = TIMING_BRANCH;
}

void CProcessor::ExecuteBPL ()
{
    if (! GetN())
    {
        SetReg(7, GetPC() + ((short)(char)LOBYTE (m_instruction)) * 2 );
    }

    m_internalTick = TIMING_BRANCH;
}

void CProcessor::ExecuteBMI ()
{
    if (GetN())
    {
        SetReg(7, GetPC() + ((short)(char) LOBYTE(m_instruction)) * 2 );
    }

    m_internalTick = TIMING_BRANCH;
}

void CProcessor::ExecuteBHI ()
{
    if (! (GetZ() || GetC()))
    {
        SetReg(7, GetPC() + ((short)(char) LOBYTE(m_instruction)) * 2 );
    }

    m_internalTick = TIMING_BRANCH;
}

void CProcessor::ExecuteBLOS ()
{
    if (GetZ() || GetC())
    {
        SetReg(7, GetPC() + ((short)(char) LOBYTE(m_instruction)) * 2 );
    }

    m_internalTick = TIMING_BRANCH;
}

void CProcessor::ExecuteBVC ()
{
    if (! GetV())
    {
        SetReg(7, GetPC() + ((short)(char) LOBYTE(m_instruction)) * 2 );
    }

    m_internalTick = TIMING_BRANCH;
}

void CProcessor::ExecuteBVS ()
{
    if (GetV())
    {
        SetReg(7, GetPC() + ((short)(char) LOBYTE(m_instruction)) * 2 );
    }

    m_internalTick = TIMING_BRANCH;
}

void CProcessor::ExecuteBHIS ()
{
    if (! GetC())
    {
        SetReg(7, GetPC() + ((short)(char) LOBYTE(m_instruction)) * 2 );
    }

    m_internalTick = TIMING_BRANCH;
}

void CProcessor::ExecuteBLO ()
{
    if (GetC())
    {
        SetReg(7, GetPC() + ((short)(char) LOBYTE(m_instruction)) * 2 );
    }

    m_internalTick = TIMING_BRANCH;
}

void CProcessor::ExecuteXOR ()  // XOR
{
    uint16_t ea = 0;

    uint16_t dst = m_methdest ? GetWord(ea = GetWordAddr(m_methdest, m_regdest)) : GetReg(m_regdest);

    dst = dst ^ GetReg(m_regsrc);

    SetN((dst >> 15) != 0);
    SetZ(!dst);
    SetV(false);

    if (m_methdest)
        SetWord(ea, dst);
    else
        SetReg(m_regdest, dst);

    m_internalTick = TIMING_ONE[m_methdest];
}

void CProcessor::ExecuteSOB ()  // SOB - subtract one: R = R - 1 ; if R != 0 : PC = PC - 2*nn
{
    uint16_t dst = GetReg(m_regsrc);

    --dst;
    SetReg(m_regsrc, dst);

    if (dst)
    {
        SetPC(GetPC() - (m_instruction & (uint16_t)077) * 2 );
    }

    m_internalTick = TIMING_SOB;
}

void CProcessor::ExecuteMOV ()
{
    if (m_instruction & 0100000) // MOVB
    {
        uint8_t dst = m_methsrc
                ? GetByte(GetByteAddr(m_methsrc, m_regsrc))
                : static_cast<uint8_t>(GetReg(m_regsrc));

        SetN((dst >> 7) != 0);
        SetZ(!dst);
        SetV(false);

        if (m_methdest)
            SetByte(GetByteAddr(m_methdest, m_regdest), dst);
        else
            SetReg(m_regdest, (dst & 0200) ? (0177400 | dst) : dst);

        m_internalTick = TIMING_MOV(m_methsrc, m_methdest);
    }
    else  // MOV
    {
        uint16_t dst = m_methsrc ? GetWord(GetWordAddr(m_methsrc, m_regsrc)) : GetReg(m_regsrc);

        SetN((dst >> 15) != 0);
        SetZ(!dst);
        SetV(false);

        if (m_methdest)
            SetWord(GetWordAddr(m_methdest, m_regdest), dst);
        else
            SetReg(m_regdest, dst);

        m_internalTick = TIMING_MOV(m_methsrc, m_methdest);
    }
}

void CProcessor::ExecuteCMP ()
{
    if (m_instruction & 0100000)
    {
        uint8_t src = m_methsrc ? GetByte(GetByteAddr(m_methsrc, m_regsrc)) : static_cast<uint8_t>(GetReg(m_regsrc));
        uint8_t src2 = m_methdest ? GetByte(GetByteAddr(m_methdest, m_regdest)) : static_cast<uint8_t>(GetReg(m_regdest));

        SetN( CheckForNegative(static_cast<uint8_t>(src - src2)) );
        SetZ( CheckForZero(static_cast<uint8_t>(src - src2)) );
        SetV( CheckSubForOverflow (src, src2) );
        SetC( CheckSubForCarry (src, src2) );

        m_internalTick = TIMING_CMP(m_methsrc, m_methdest);
    }
    else
    {
        uint16_t src = m_methsrc ? GetWord(GetWordAddr(m_methsrc, m_regsrc)) : GetReg(m_regsrc);
        uint16_t src2 = m_methdest ? GetWord(GetWordAddr(m_methdest, m_regdest)) : GetReg(m_regdest);

        SetN( CheckForNegative (static_cast<uint16_t>(src - src2)) );
        SetZ( CheckForZero (static_cast<uint16_t>(src - src2)) );
        SetV( CheckSubForOverflow (src, src2) );
        SetC( CheckSubForCarry (src, src2) );

        m_internalTick = TIMING_CMP(m_methsrc, m_methdest);
    }
}

void CProcessor::ExecuteBIT ()  // BIT{B} - bit test
{
    uint16_t ea;
    if (m_instruction & 0100000)
    {
        uint8_t src = m_methsrc ? GetByte(GetByteAddr(m_methsrc, m_regsrc)) : static_cast<uint8_t>(GetReg(m_regsrc));
        uint8_t src2 = m_methdest ? GetByte(ea = GetByteAddr(m_methdest, m_regdest)) : static_cast<uint8_t>(GetReg(m_regdest));

        uint8_t dst = src2 & src;

        SetN((dst >> 7) != 0);
        SetZ(!dst);
        SetV(false);

        m_internalTick = TIMING_CMP(m_methsrc, m_methdest);
    }
    else
    {
        uint16_t src  = m_methsrc  ? GetWord(GetWordAddr(m_methsrc, m_regsrc)) : GetReg(m_regsrc);
        uint16_t src2 = m_methdest ? GetWord(ea = GetWordAddr(m_methdest, m_regdest)) : GetReg(m_regdest);

        uint16_t dst = src2 & src;

        SetN((dst >> 15) != 0);
        SetZ(!dst);
        SetV(false);

        m_internalTick = TIMING_CMP(m_methsrc, m_methdest);
    }
}

void CProcessor::ExecuteBIC ()  // BIC{B} - bit clear
{
    uint16_t ea = 0;

    if (m_instruction & 0100000)
    {
        uint8_t src = m_methsrc ? GetByte(GetByteAddr(m_methsrc, m_regsrc)) : static_cast<uint8_t>(GetReg(m_regsrc));
        uint8_t src2 = m_methdest ? GetByte(ea = GetByteAddr(m_methdest, m_regdest)) : static_cast<uint8_t>(GetReg(m_regdest));

        uint8_t dst = src2 & (~src);

        SetN((dst >> 7) != 0);
        SetZ(!dst);
        SetV(false);

        if (m_methdest)
            SetByte(ea, dst);
        else
            SetReg(m_regdest, (GetReg(m_regdest) & 0177400) | dst);

        m_internalTick = TIMING_MOV(m_methsrc, m_methdest);
    }
    else
    {
        uint16_t src = m_methsrc ? GetWord(GetWordAddr(m_methsrc, m_regsrc)) : GetReg(m_regsrc);
        uint16_t src2 = m_methdest ? GetWord(ea = GetWordAddr(m_methdest, m_regdest)) : GetReg(m_regdest);

        uint16_t dst = src2 & (~src);

        SetN((dst >> 15) != 0);
        SetZ(!dst);
        SetV(false);

        if (m_methdest)
            SetWord(ea, dst);
        else
            SetReg(m_regdest, dst);

        m_internalTick = TIMING_MOV(m_methsrc, m_methdest);
    }
}

void CProcessor::ExecuteBIS ()  // BIS{B} - bit set
{
    uint16_t ea = 0;

    if (m_instruction & 0100000)
    {
        uint8_t src = m_methsrc ? GetByte(GetByteAddr(m_methsrc, m_regsrc)) : static_cast<uint8_t>(GetReg(m_regsrc));
        uint8_t src2 = m_methdest ? GetByte(ea = GetByteAddr(m_methdest, m_regdest)) : static_cast<uint8_t>(GetReg(m_regdest));

        uint8_t dst = src2 | src;

        SetN((dst >> 7) != 0);
        SetZ(!dst);
        SetV(false);

        if (m_methdest)
            SetByte(ea, dst);
        else
            SetReg(m_regdest, (GetReg(m_regdest) & 0177400) | dst);

        m_internalTick = TIMING_MOV(m_methsrc, m_methdest);
    }
    else
    {
        uint16_t src = m_methsrc ? GetWord(GetWordAddr(m_methsrc, m_regsrc)) : GetReg(m_regsrc);
        uint16_t src2 = m_methdest ? GetWord(ea = GetWordAddr(m_methdest, m_regdest)) : GetReg(m_regdest);

        uint16_t dst = src2 | src;

        SetN((dst >> 15) != 0);
        SetZ(!dst);
        SetV(false);

        if (m_methdest)
            SetWord(ea, dst);
        else
            SetReg(m_regdest, dst);

        m_internalTick = TIMING_MOV(m_methsrc, m_methdest);
    }
}

void CProcessor::ExecuteADD ()  // ADD
{
    uint16_t ea = 0;

    uint16_t src = m_methsrc ? GetWord(GetWordAddr(m_methsrc, m_regsrc)) : GetReg(m_regsrc);
    uint16_t src2 = m_methdest ? GetWord(ea = GetWordAddr(m_methdest, m_regdest)) : GetReg(m_regdest);

    SetN(CheckForNegative (static_cast<uint16_t>(src2 + src)));
    SetZ(CheckForZero (static_cast<uint16_t>(src2 + src)));
    SetV(CheckAddForOverflow (src2, src));
    SetC(CheckAddForCarry (src2, src));

    signed short dst = src2 + src;

    if (m_methdest)
        SetWord(ea, dst);
    else
        SetReg(m_regdest, dst);

    m_internalTick = TIMING_MOV(m_methsrc, m_methdest);
}

void CProcessor::ExecuteSUB ()
{
    uint16_t ea = 0;

    uint16_t src = m_methsrc ? GetWord(GetWordAddr(m_methsrc, m_regsrc)) : GetReg(m_regsrc);
    uint16_t src2 = m_methdest ? GetWord(ea = GetWordAddr(m_methdest, m_regdest)) : GetReg(m_regdest);

    SetN(CheckForNegative (static_cast<uint16_t>(src2 - src)));
    SetZ(CheckForZero (static_cast<uint16_t>(src2 - src)));
    SetV(CheckSubForOverflow (src2, src));
    SetC(CheckSubForCarry (src2, src));

    uint16_t dst = src2 - src;

    if (m_methdest)
        SetWord(ea, dst);
    else
        SetReg(m_regdest, dst);

    m_internalTick = TIMING_MOV(m_methsrc, m_methdest);
}

void CProcessor::ExecuteEMT ()  // EMT - emulator trap
{
    m_EMT_rq = true;
    m_internalTick = TIMING_EMT;
}

void CProcessor::ExecuteTRAP ()
{
    m_TRAPrq = true;
    m_internalTick = TIMING_EMT;
}

void CProcessor::ExecuteJSR ()  // JSR - Jump subroutine: *--SP = R; R = PC; PC = &d (a-mode > 0)
{
    if (m_methdest == 0)
    {
        // Неправильный метод адресации
        m_RSVD4rq = true;
        m_internalTick = TIMING_EMT;
    }
    else
    {
        uint16_t dst = GetWordAddr(m_methdest, m_regdest);

        SetSP( GetSP() - 2 );

        SetWord( GetSP(), GetReg(m_regsrc) );

        SetReg(m_regsrc, GetPC());

        SetPC(dst);

        m_internalTick = TIMING_JSR[m_methdest];
    }
}

//////////////////////////////////////////////////////////////////////
//
// CPU image format (32 bytes):
//   2 bytes        PSW
//   2*8 bytes      Registers R0..R7
//   2*2 bytes      Saved PC and PSW
//   2 bytes        Stopped flag: !0 - stopped, 0 - not stopped

void CProcessor::SaveToImage(uint8_t* pImage)
{
    uint16_t* pwImage = reinterpret_cast<uint16_t*>(pImage);
    // PSW
    *pwImage++ = m_psw;
    // Registers R0..R7
    ::memcpy(pwImage, m_R, 2 * 8);
    pwImage += 2 * 8;
    // Saved PC and PSW
    *pwImage++ = 0;  //m_savepc;
    *pwImage++ = 0;  //m_savepsw;
    // Modes
    *pwImage++ = (m_okStopped ? 1 : 0);
    *pwImage++ = (m_waitmode ? 1 : 0);
    *pwImage++ = (m_stepmode ? 1 : 0);
    *pwImage++ = 0;
}

void CProcessor::LoadFromImage(const uint8_t* pImage)
{
    Stop();  // Reset internal state

    const uint16_t* pwImage = reinterpret_cast<const uint16_t*>(pImage);
    // PSW
    m_psw = *pwImage++;
    // Registers R0..R7
    ::memcpy(m_R, pwImage, 2 * 8);
    pwImage += 2 * 8;
    // Saved PC and PSW - skip
    pwImage++;
    pwImage++;
    // Modes
    m_okStopped = (*pwImage++ != 0);
    m_waitmode = (*pwImage++ != 0);
    m_stepmode = (*pwImage++ != 0);
    //pwImage++;
}

uint16_t CProcessor::GetWordAddr (uint8_t meth, uint8_t reg)
{
    uint16_t addr = 0;

    switch (meth)
    {
    case 1:   //(R)
        addr = GetReg(reg);
        break;
    case 2:   //(R)+
        addr = GetReg(reg);
        SetReg(reg, addr + 2);
        break;
    case 3:  //@(R)+
        addr = GetReg(reg);
        SetReg(reg, addr + 2);
        addr = GetWord(addr);
        break;
    case 4: //-(R)
        SetReg(reg, GetReg(reg) - 2);
        addr = GetReg(reg);
        break;
    case 5: //@-(R)
        SetReg(reg, GetReg(reg) - 2);
        addr = GetReg(reg);
        addr = GetWord(addr);
        break;
    case 6: //d(R)
        addr = GetWord(GetPC());
        SetPC(GetPC() + 2);
        addr = GetReg(reg) + addr;
        break;
    case 7: //@d(r)
        addr = GetWord(GetPC());
        SetPC(GetPC() + 2);
        addr = GetReg(reg) + addr;
        addr = GetWord(addr);
        break;
    }
    return addr;
}

uint16_t CProcessor::GetByteAddr (uint8_t meth, uint8_t reg)
{
    uint16_t addr = 0, delta;

    switch (meth)
    {
    case 1:
        addr = GetReg(reg);
        break;
    case 2:
        delta = 1 + (reg >= 6);
        addr = GetReg(reg);
        SetReg(reg, addr + delta);
        break;
    case 3:
        addr = GetReg(reg);
        SetReg(reg, addr + 2);
        addr = GetWord(addr);
        break;
    case 4:
        delta = 1 + (reg >= 6);
        SetReg(reg, GetReg(reg) - delta);
        addr = GetReg(reg);
        break;
    case 5:
        SetReg(reg, GetReg(reg) - 2);
        addr = GetReg(reg);
        addr = GetWord(addr);
        break;
    case 6: //d(R)
        addr = GetWord(GetPC());
        SetPC(GetPC() + 2);
        addr = GetReg(reg) + addr;
        break;
    case 7: //@d(r)
        addr = GetWord(GetPC());
        SetPC(GetPC() + 2);
        addr = GetReg(reg) + addr;
        addr = GetWord(addr);
        break;
    }

    return addr;
}


//////////////////////////////////////////////////////////////////////
