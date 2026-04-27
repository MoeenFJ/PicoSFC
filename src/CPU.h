#include <bits/stdc++.h>
using namespace std;

typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int add24;

namespace CPU
{

    const uint8_t flagNMask = 0b10000000;
    const uint8_t flagVMask = 0b01000000;
    const uint8_t flagMMask = 0b00100000;
    const uint8_t flagXMask = 0b00010000;
    const uint8_t flagDMask = 0b00001000;
    const uint8_t flagIMask = 0b00000100;
    const uint8_t flagZMask = 0b00000010;
    const uint8_t flagCMask = 0b00000001;
    struct FlagsRegister
    {
        bool N;
        bool V;
        bool M;
        bool X;
        bool D;
        bool I;
        bool Z;
        bool C;
    };

    struct CPURegisters
    {
        uint16 C;  // accumulator 16 bit
        uint8 DBR; // data bank register 8 bit
        uint16 D;  // Direct 16 bit
        uint8 K;   // Program Bank 8 bit
        uint16 PC; // PC 16 bit
        uint16 S;  // Stack 16 bit
        uint16 X;  // X 16 bit
        uint16 Y;  // Y 16 bit
    };

    enum CPUAddressingMode
    {
        Abs = 0,
        AbsX = 1,
        AbsY = 2,
        AbsPtr16 = 3,
        AbsPtr24 = 4,
        AbsXPtr16 = 5,

        Dir = 6,
        DirX = 7,
        DirY = 8,
        DirPtr16 = 9,
        DirPtr24 = 10,
        DirXPtr16 = 11,
        DirPtr16Y = 12,
        DirPtr24Y = 13,

        Imm = 14,
        Long = 15,
        LongX = 16,
        Rel8 = 17,
        Rel16 = 18,
        SrcDst = 19,
        STK = 20,
        STKPtr16Y = 21
    };

    FlagsRegister flags; // Status Reg
    CPURegisters cregs;
    bool flagE;
    uint8_t inst;
    add24 addL, addH, addXH;

    add24 PCByteAhead(uint8 n = 1)
    {
        return ((add24)cregs.K << 16) | (((add24)cregs.PC + n) & 0x00FFFF);
    }

    uint16 ReadWord()
    {
        uint16 low = C65Read(addL);
        uint16 high = C65Read(addH);
        return (high << 8) | low;
    }
    void WriteWord(uint16 data)
    {
        C65Write(addL, data & 0x00FF);
        C65Write(addH, data >> 8);
    }

    uint16 ReadWordAt(add24 add)
    {
        uint16 low = C65Read(add);
        uint16 high = C65Read(add + 1);
        return (high << 8) | low;
    }
    void WriteWordAt(add24 add, uint16 data)
    {
        C65Write(add, data & 0x00FF);
        C65Write(add + 1, data >> 8);
    }

    void ResolveAddress(CPUAddressingMode mode)
    {
        uint8 ll, hh, xh;
        ll = C65Read(PCByteAhead(1));
        hh = C65Read(PCByteAhead(2));
        xh = C65Read(PCByteAhead(3));

        // Phase one
        switch (mode)
        {
        case CPUAddressingMode::Rel8:
        case CPUAddressingMode::Rel16:
        case CPUAddressingMode::Imm:
            addL = PCByteAhead(1);
            addH = PCByteAhead(2);
            break;

        case CPUAddressingMode::AbsPtr16:
        case CPUAddressingMode::AbsPtr24:
            addL = (hh << 8) | ll;
            addH = addL + 1;
            addH &= 0x00FFFF;
            addXH = addL + 2;
            addXH &= 0x00FFFF;
            break;

        case CPUAddressingMode::Abs:
            addL = (cregs.DBR << 16) | (hh << 8) | ll;
            addH = addL + 1;
            addXH = addL + 2;
            break;

        case CPUAddressingMode::AbsXPtr16:
            addL = (hh << 8) | ll;
            addL += cregs.X;

            addH = addL + 1;

            addL &= 0x00FFFF;
            addH &= 0x00FFFF;
            addL |= cregs.K << 16;
            addH |= cregs.K << 16;
            break;

        case CPUAddressingMode::AbsX:
            addL = (cregs.DBR << 16) | (hh << 8) | ll;
            addL += cregs.X;
            addH = addL + 1;
            addXH = addL + 2;
            break;

        case CPUAddressingMode::AbsY:
            addL = (cregs.DBR << 16) | (hh << 8) | ll;
            addL += cregs.Y;
            addH = addL + 1;
            addXH = addL + 2;
            break;

        case CPUAddressingMode::DirPtr16:
        case CPUAddressingMode::DirPtr24:
        case CPUAddressingMode::DirPtr24Y:
        case CPUAddressingMode::DirPtr16Y:
        case CPUAddressingMode::Dir:
            addL = (cregs.D + ll) & 0x00FFFF;
            addH = (cregs.D + ll + 1) & 0x00FFFF;
            addXH = (cregs.D + ll + 2) & 0x00FFFF;
            break;

        case CPUAddressingMode::DirXPtr16:
        case CPUAddressingMode::DirX:
            addL = (cregs.D + ll + cregs.X) & 0x00FFFF;
            addH = (cregs.D + ll + cregs.X + 1) & 0x00FFFF;
            addXH = (cregs.D + ll + cregs.X + 2) & 0x00FFFF;
            break;

        case CPUAddressingMode::DirY:
            addL = (cregs.D + ll + cregs.Y) & 0x00FFFF;
            addH = (cregs.D + ll + cregs.Y + 1) & 0x00FFFF;
            addXH = (cregs.D + ll + cregs.Y + 2) & 0x00FFFF;
            break;

        case CPUAddressingMode::Long:
            addL = (xh << 16) | (hh << 8) | (ll);
            addH = addL + 1;
            break;

        case CPUAddressingMode::LongX:
            addL = (xh << 16) | (hh << 8) | (ll);
            addL += cregs.X;
            addH = addL + 1;
            break;

        case CPUAddressingMode::STKPtr16Y:
        case CPUAddressingMode::STK:
            addL = (ll + cregs.S) & 0x00FFFF;
            addH = (ll + cregs.S + 1) & 0x00FFFF;
            break;
        default:
            break;
        }

        switch (mode)
        {
        case CPUAddressingMode::AbsXPtr16:
        case CPUAddressingMode::AbsPtr16:
            addL = (cregs.K << 16) | (C65Read(addH) << 8) | C65Read(addL);
            break;

        case CPUAddressingMode::DirPtr16:
        case CPUAddressingMode::DirXPtr16:
        case CPUAddressingMode::DirPtr16Y:
        case CPUAddressingMode::STKPtr16Y:
            addL = (cregs.DBR << 16) | (C65Read(addH) << 8) | C65Read(addL);
            addH = addL + 1;
            break;

        case CPUAddressingMode::AbsPtr24:
        case CPUAddressingMode::DirPtr24:
        case CPUAddressingMode::DirPtr24Y:
            addL = (C65Read(addXH) << 16) | (C65Read(addH) << 8) | C65Read(addL);
            addH = addL + 1;

        default:
            break;
        }

        switch (mode)
        {
        case CPUAddressingMode::DirPtr16Y:
        case CPUAddressingMode::DirPtr24Y:
        case CPUAddressingMode::STKPtr16Y:
            addL += cregs.Y;
            addH += cregs.Y;
            break;

        default:
            break;
        }

        addL &= 0x00FFFFFF;
        addH &= 0x00FFFFFF;
    }

    void WriteM(uint16 d)
    {
        if (flags.M)
            C65Write(addL, d);
        else
            WriteWord(d);
    }

    void WriteX(uint16 d)
    {
        if (flags.X)
            C65Write(addL, d);
        else
            WriteWord(d);
    }

    uint16 ReadM()
    {
        if (flags.M)
            return C65Read(addL);
        else
            return ReadWord();
    }

    uint16 ReadX()
    {
        if (flags.X)
            return C65Read(addL);
        else
            return ReadWord();
    }

    void UpdateC(uint16 val)
    {
        if (flags.M)
        {
            cregs.C = (cregs.C & 0xFF00) | (val & 0x00FF);
        }
        else
        {
            cregs.C = val;
        }
    }

    void UpdateX(uint16 val)
    {
        if (flags.X)
        {
            cregs.X = (cregs.X & 0xFF00) | (val & 0x00FF);
        }
        else
        {
            cregs.X = val;
        }
    }

    void UpdateY(uint16 val)
    {
        if (flags.X)
        {
            cregs.Y = (cregs.Y & 0xFF00) | (val & 0x00FF);
        }
        else
        {
            cregs.Y = val;
        }
    }

    void Push(uint8 d)
    {
        C65Write(cregs.S, d);
        cregs.S -= 1;
    }
    void PushWord(uint16 d)
    {
        cregs.S -= 1;
        C65Write(cregs.S, d & 0x00ff);
        C65Write(cregs.S + 1, (d & 0xff00) >> 8);
        cregs.S -= 1;
    }
    uint8 Pop()
    {
        cregs.S += 1;
        return C65Read(cregs.S);
    }
    uint16 PopWord()
    {
        cregs.S += 1;
        uint16 val = C65Read(cregs.S + 1) << 8 | C65Read(cregs.S);
        cregs.S += 1;
        return val;
    }

    // Interrupts
    void reset()
    {
        uint16 rstVector = ReadWordAt(0x00FFFC);
        cregs.C = 0;
        cregs.DBR = 0;
        cregs.D = 0;
        cregs.K = 0;
        cregs.PC = 0;
        cregs.S = 0;
        cregs.X = 0;
        cregs.Y = 0;
        flagE = 1;
        flags.M = 1;
        flags.X = 1;
        flags.I = 1;
        flags.D = 0;
        cregs.S = 0x0100;
        cregs.K = 0;
        cregs.D = 0;
        cregs.DBR = 0;
        cregs.PC = rstVector;
        inst = C65Read(((add24)cregs.K << 16) + (add24)cregs.PC);
    }

    void invokeNMI()
    {

        if (!flagE)
            Push(cregs.K);
        PushWord(cregs.PC);
        uint8 t = (flags.N ? flagNMask : 0) | (flags.V ? flagVMask : 0) | (flags.M ? flagMMask : 0) | (flags.X ? flagXMask : 0) | (flags.D ? flagDMask : 0) | (flags.I ? flagIMask : 0) | (flags.Z ? flagZMask : 0) | (flags.C ? flagCMask : 0);

        Push(t);

        uint16 nmi;
        cregs.K = 0;
        if (flagE)
            nmi = ReadWordAt(0x00FFFA);
        else
            nmi = ReadWordAt(0x00FFEA);
        cregs.PC = nmi;
        inst = C65Read(((add24)cregs.K << 16) + (add24)cregs.PC);
    }

    void setOverflow()
    {
        flags.V = 1;
    }

    void invokeIRQ()
    {

        if (flags.I) // I == 1 => disable intrrupts.
            return;

        if (!flagE)
            Push(cregs.K);  // 8 Bit
        PushWord(cregs.PC); // 16 Bit
        uint8 t = (flags.N ? flagNMask : 0) | (flags.V ? flagVMask : 0) | (flags.M ? flagMMask : 0) | (flags.X ? flagXMask : 0) | (flags.D ? flagDMask : 0) | (flags.I ? flagIMask : 0) | (flags.Z ? flagZMask : 0) | (flags.C ? flagCMask : 0);

        Push(t); // 8 Bit

        uint16 irq;
        cregs.K = 0;
        if (flagE)
            irq = ReadWordAt(0x00FFFE);
        else
            irq = ReadWordAt(0x00FFEE);
        cregs.PC = irq;
        inst = C65Read(((add24)cregs.K << 16) + (add24)cregs.PC);
    }

    void doADC(uint16 d)
    {
        unsigned int t;
        if (flags.D)
        {
            // --- BCD (Decimal) Addition ---
            if (flags.M)
            {
                // 8-bit BCD
                int op1 = cregs.C & 0xFF;
                int op2 = d & 0xFF;

                // 1. Calculate pure binary sum just for the V flag
                int binSum = op1 + op2 + flags.C;
                // flags.V = (((op1 ^ binSum) & (op2 ^ binSum) & 0x80) != 0);

                // 2. Perform BCD Adjustments
                int al = (op1 & 0x0F) + (op2 & 0x0F) + flags.C;
                if (al > 0x09)
                    al += 0x06;

                int ah = (op1 & 0xF0) + (op2 & 0xF0) + (al > 0x0F ? 0x10 : 0);
                if (ah > 0x90)
                    ah += 0x60;

                t = (ah & 0xF0) | (al & 0x0F);
                flags.C = (ah > 0xFF);
                flags.V = (((op1 ^ t) & (op2 ^ t) & 0x80) != 0);
            }
            else
            {
                // 16-bit BCD
                int op1 = cregs.C & 0xFFFF;
                int op2 = d & 0xFFFF;

                int binSum = op1 + op2 + flags.C;
                // flags.V = (((op1 ^ binSum) & (op2 ^ binSum) & 0x8000) != 0);

                int al = (op1 & 0x000F) + (op2 & 0x000F) + flags.C;
                if (al > 0x0009)
                    al += 0x0006;

                int ah = (op1 & 0x00F0) + (op2 & 0x00F0) + (al > 0x000F ? 0x0010 : 0);
                if (ah > 0x0090)
                    ah += 0x0060;

                int bl = (op1 & 0x0F00) + (op2 & 0x0F00) + (ah > 0x00FF ? 0x0100 : 0);
                if (bl > 0x0900)
                    bl += 0x0600;

                int bh = (op1 & 0xF000) + (op2 & 0xF000) + (bl > 0x0FFF ? 0x1000 : 0);
                if (bh > 0x9000)
                    bh += 0x6000;

                t = (bh & 0xF000) | (bl & 0x0F00) | (ah & 0x00F0) | (al & 0x000F);
                flags.C = (bh > 0xFFFF);
                flags.V = (((op1 ^ t) & (op2 ^ t) & 0x80) != 0);
            }
        }
        else
        {
            // --- Standard Binary Addition ---
            if (flags.M)
            {
                int op1 = cregs.C & 0xFF;
                int op2 = d & 0xFF;
                t = op1 + op2 + flags.C;
                flags.V = (((op1 ^ t) & (op2 ^ t) & 0x80) != 0);
                flags.C = (t > 0xFF);
            }
            else
            {
                int op1 = cregs.C & 0xFFFF;
                int op2 = d & 0xFFFF;
                t = op1 + op2 + flags.C;
                flags.V = (((op1 ^ t) & (op2 ^ t) & 0x8000) != 0);
                flags.C = (t > 0xFFFF);
            }
        }

        UpdateC(t);

        flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
        flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
    }

    void doSBC(uint16 d)
    {
        unsigned int t;

        if (flags.D)
        {
            // --- BCD (Decimal) Subtraction ---
            if (flags.M)
            {
                // 8-bit BCD
                int op1 = cregs.C & 0xFF;
                int op2 = d & 0xFF;
                int carryIn = flags.C ? 0 : 1; // 65816 Carry is inverted borrow

                // 1. Pure binary subtraction for V and C flags
                int binDiff = op1 - op2 - carryIn;
                // V flag: Checks if signs of operands differed, and if result sign differs from accumulator
                flags.V = (((op1 ^ op2) & (op1 ^ binDiff) & 0x80) != 0);
                flags.C = (binDiff >= 0); // Carry is 1 if no borrow occurred

                // 2. Perform BCD Adjustments (nibble by nibble)
                int al = (op1 & 0x0F) - (op2 & 0x0F) - carryIn;
                int ah = ((op1 >> 4) & 0x0F) - ((op2 >> 4) & 0x0F) - (al < 0 ? 1 : 0);

                // If a nibble underflows (borrows), we apply the BCD correction (-6)
                if (al < 0)
                    al -= 6;
                if (ah < 0)
                    ah -= 6;

                t = ((ah << 4) | (al & 0x0F)) & 0xFF;
            }
            else
            {
                // 16-bit BCD
                int op1 = cregs.C & 0xFFFF;
                int op2 = d & 0xFFFF;
                int carryIn = flags.C ? 0 : 1;

                int binDiff = op1 - op2 - carryIn;
                flags.V = (((op1 ^ op2) & (op1 ^ binDiff) & 0x8000) != 0);
                flags.C = (binDiff >= 0);

                // Cascaded nibble subtraction
                int al = (op1 & 0x000F) - (op2 & 0x000F) - carryIn;
                int ah = ((op1 >> 4) & 0x000F) - ((op2 >> 4) & 0x000F) - (al < 0 ? 1 : 0);
                int bl = ((op1 >> 8) & 0x000F) - ((op2 >> 8) & 0x000F) - (ah < 0 ? 1 : 0);
                int bh = ((op1 >> 12) & 0x000F) - ((op2 >> 12) & 0x000F) - (bl < 0 ? 1 : 0);

                // Apply corrections where borrows occurred
                if (al < 0)
                    al -= 6;
                if (ah < 0)
                    ah -= 6;
                if (bl < 0)
                    bl -= 6;
                if (bh < 0)
                    bh -= 6;

                t = ((bh << 12) | ((bl & 0x0F) << 8) | ((ah & 0x0F) << 4) | (al & 0x0F)) & 0xFFFF;
            }
        }
        else
        {
            // --- Standard Binary Subtraction ---
            if (flags.M)
            {
                int op1 = cregs.C & 0xFF;
                int op2 = d & 0xFF;
                int binDiff = op1 - op2 - (flags.C ? 0 : 1);

                t = binDiff & 0xFF;
                flags.V = (((op1 ^ op2) & (op1 ^ t) & 0x80) != 0);
                flags.C = (binDiff >= 0);
            }
            else
            {
                int op1 = cregs.C & 0xFFFF;
                int op2 = d & 0xFFFF;
                int binDiff = op1 - op2 - (flags.C ? 0 : 1);

                t = binDiff & 0xFFFF;
                flags.V = (((op1 ^ op2) & (op1 ^ t) & 0x8000) != 0);
                flags.C = (binDiff >= 0);
            }
        }

        UpdateC(t); // Updates the Accumulator (handles 8-bit B register preservation)

        // The N and Z flags evaluate the resulting accumulator value (Valid for BCD!)
        flags.N = (t & (flags.M ? 0x0080 : 0x8000)) != 0;
        flags.Z = (t & (flags.M ? 0x00FF : 0xFFFF)) == 0;
    }

    // Rename to printState
    void printStatus()
    {
        // nvmxdizc
        cout << "--------------------------NEW INSTRUCTON---------------------------------------" << endl;
        cout << "-----------Flags----------" << endl;
        cout << "N V M X D I Z C    E" << endl;
        cout << flags.N << " " << flags.V << " " << flags.M << " " << flags.X << " " << flags.D << " " << flags.I << " " << flags.Z << " " << flags.C << "    " << flagE << endl;
        cout << "-----------Regs----------" << endl;
        cout << "Stack top : " << hex << (unsigned int)C65Read(cregs.S + 1) << " " << (unsigned int)C65Read(cregs.S + 2) << endl;
        cout << "C : " << std::hex << (unsigned int)cregs.C << endl;
        cout << "DBR : " << std::hex << (unsigned int)cregs.DBR << endl;
        cout << "D : " << std::hex << (unsigned int)cregs.D << endl;
        cout << "K : " << std::hex << (unsigned int)cregs.K << endl;
        cout << "S : " << std::hex << (unsigned int)cregs.S << endl;
        cout << "X : " << std::hex << (unsigned int)cregs.X << endl;
        cout << "Y : " << std::hex << (unsigned int)cregs.Y << endl;
        cout << "PC : " << std::hex << (unsigned int)cregs.PC << endl;
        add24 instAddress = ((add24)cregs.K << 16) + (add24)cregs.PC;

        cout << "InstAddress : " << std::hex << instAddress << endl;
        uint8_t inst = C65Read(instAddress);
        cout << "OpCode : " << std::hex << (int)inst << endl;
        cout << "3 Bytes Ahead : " << std::hex << (int)C65Read(instAddress + 1) << " " << std::hex << (int)C65Read(instAddress + 2) << " " << std::hex << (int)C65Read(instAddress + 3) << " " << endl;
    }

    string stringStatus()
    {
        stringstream ss;
        // nvmxdizc
        ss << "--------------------------NEW INSTRUCTON---------------------------------------" << endl;
        ss << "-----------Flags----------" << endl;
        ss << "N V M X D I Z C    E" << endl;
        ss << flags.N << " " << flags.V << " " << flags.M << " " << flags.X << " " << flags.D << " " << flags.I << " " << flags.Z << " " << flags.C << "    " << flagE << endl;
        ss << "-----------Regs----------" << endl;
        ss << "Stack top : " << hex << (unsigned int)C65Read(cregs.S + 1) << " " << (unsigned int)C65Read(cregs.S + 2) << endl;

        ss << "C : " << std::hex << (unsigned int)cregs.C << endl;
        ss << "DBR : " << std::hex << (unsigned int)cregs.DBR << endl;
        ss << "D : " << std::hex << (unsigned int)cregs.D << endl;
        ss << "K : " << std::hex << (unsigned int)cregs.K << endl;
        ss << "S : " << std::hex << (unsigned int)cregs.S << endl;
        ss << "X : " << std::hex << (unsigned int)cregs.X << endl;
        ss << "Y : " << std::hex << (unsigned int)cregs.Y << endl;
        ss << "PC : " << std::hex << (unsigned int)cregs.PC << endl;
        add24 instAddress = ((add24)cregs.K << 16) + (add24)cregs.PC;

        ss << "InstAddress : " << std::hex << instAddress << endl;
        uint8_t inst = C65Read(instAddress);
        ss << "OpCode : " << std::hex << (int)inst << endl;
        ss << "3 Bytes Ahead : " << std::hex << (int)C65Read(instAddress + 1) << " " << std::hex << (int)C65Read(instAddress + 2) << " " << std::hex << (int)C65Read(instAddress + 3) << " " << endl;

        return ss.str();
    }

    void cpuStep()
    {
        inst = C65Read(((add24)cregs.K << 16) + (add24)cregs.PC);

        switch (inst)
        {
#pragma region IgnoredInstructions
        case 0xea: // NOP
        {
            cregs.PC += 1;
            break;
        }
        case 0x42: // WDM
        {
            cregs.PC += 2;
            break;
        }

        case 0x00: // BRK
        {
            cregs.PC += 1;
            break;
        }

        case 0x02: // COP
        {
            cregs.PC += 2;
            break;
        }

        case 0xdb: // STP
        {
            cregs.PC += 1;
            break;
        }
        case 0xcb: // WAI
        {
            cregs.PC += 1;
            break;
        }
#pragma endregion

#pragma region Flag_Manipulation
        case 0xfb: // XCE
        {
            bool e = flagE;
            flagE = flags.C;
            flags.C = e;

            cregs.PC += 1;
            break;
        }
        case 0x78: // SEI
        {
            flags.I = 1;
            cregs.PC += 1;
            break;
        }
        case 0x38: // SEC
        {
            flags.C = 1;
            cregs.PC += 1;
            break;
        }
        case 0xF8: // SED
        {
            flags.D = 1;
            cregs.PC += 1;
            break;
        }
        case 0xB8: // CLV
        {
            flags.V = 0;
            cregs.PC += 1;
            break;
        }
        case 0x18: // CLC
        {
            flags.C = 0;
            cregs.PC += 1;
            break;
        }
        case 0xD8: // CLD
        {
            flags.D = 0;
            cregs.PC += 1;
            break;
        }
        case 0x58: // CLI
        {
            flags.I = 0;
            cregs.PC += 1;
            break;
        }

        case 0xc2: // REP imm - Reset selected Flags
        {
            ResolveAddress(CPUAddressingMode::Imm);
            uint8 imm = C65Read(addL);

            flags.N = (imm & flagNMask) ? 0 : flags.N;
            flags.V = (imm & flagVMask) ? 0 : flags.V;
            flags.M = (imm & flagMMask) ? 0 : flags.M;
            flags.X = (imm & flagXMask) ? 0 : flags.X;
            flags.D = (imm & flagDMask) ? 0 : flags.D;
            flags.I = (imm & flagIMask) ? 0 : flags.I;
            flags.Z = (imm & flagZMask) ? 0 : flags.Z;
            flags.C = (imm & flagCMask) ? 0 : flags.C;

            cregs.PC += 2;
            break;
        }
        case 0xe2: // SEP imm - Set selected Flags
        {
            ResolveAddress(CPUAddressingMode::Imm);
            uint8 imm = C65Read(addL);

            flags.N = (imm & flagNMask) ? 1 : flags.N;
            flags.V = (imm & flagVMask) ? 1 : flags.V;
            flags.M = (imm & flagMMask) ? 1 : flags.M;
            flags.X = (imm & flagXMask) ? 1 : flags.X;
            flags.D = (imm & flagDMask) ? 1 : flags.D;
            flags.I = (imm & flagIMask) ? 1 : flags.I;
            flags.Z = (imm & flagZMask) ? 1 : flags.Z;
            flags.C = (imm & flagCMask) ? 1 : flags.C;

            cregs.PC += 2;
            break;
        }
#pragma endregion

#pragma region Stack_Operations

        case 0x08: // PHP
        {
            uint8 t = (flags.N ? flagNMask : 0) | (flags.V ? flagVMask : 0) | (flags.M ? flagMMask : 0) | (flags.X ? flagXMask : 0) | (flags.D ? flagDMask : 0) | (flags.I ? flagIMask : 0) | (flags.Z ? flagZMask : 0) | (flags.C ? flagCMask : 0);
            Push(t);
            // No Flags
            // cout << "PHP : " << hex << (uint16)flags << endl;
            // cin.get();
            cregs.PC += 1;

            break;
        }
        case 0x48: // PHA
        {
            if (flags.M)
                Push((uint8)cregs.C);
            else
                PushWord((uint16)cregs.C);
            cregs.PC += 1;
            break;
        }
        case 0xda: // PHX
        {
            if (flags.X)
                Push((uint8)cregs.X);
            else
                PushWord((uint16)cregs.X);
            cregs.PC += 1;
            break;
        }
        case 0x5a: // PHY
        {
            if (flags.X)
                Push((uint8)cregs.Y);
            else
                PushWord((uint16)cregs.Y);
            cregs.PC += 1;
            break;
        }
        case 0x0b: // PHD
        {

            PushWord((uint16)cregs.D);
            cregs.PC += 1;
            break;
        }
        case 0x8b: // PHB
        {
            // TODO : Make sure about 1 byte
            Push((uint16)cregs.DBR);
            cregs.PC += 1;
            break;
        }
        case 0x4b: // PHK
        {
            Push((uint16)cregs.K);
            cregs.PC += 1;
            break;
        }

        case 0x68: // PLA
        {
            uint16 d;
            if (flags.M)
                d = Pop();
            else
                d = PopWord();

            UpdateC(d);

            // cout << "PLA : " << hex << (uint16)cregs.C << endl;
            //  cin.get();
            flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
            cregs.PC += 1;
            break;
        }
        case 0x7a: // PLY
        {
            uint16 d;
            if (flags.X)
                d = Pop();
            else
                d = PopWord();

            UpdateY(d);

            flags.N = cregs.Y & (flags.X ? 0x0080 : 0x8000);
            flags.Z = !(cregs.Y & (flags.X ? 0x00FF : 0xFFFF));
            cregs.PC += 1;
            break;
        }
        case 0xfa: // PLX
        {
            uint16 d;
            if (flags.X)
                d = Pop();
            else
                d = PopWord();

            UpdateX(d);

            flags.N = cregs.X & (flags.X ? 0x0080 : 0x8000);
            flags.Z = !(cregs.X & (flags.X ? 0x00FF : 0xFFFF));
            cregs.PC += 1;
            break;
        }
        case 0xab: // PLB
        {
            // DBR is always 8 bit
            cregs.DBR = Pop();
            flags.N = cregs.DBR & 0x0080;
            flags.Z = !(cregs.DBR & 0x00FF);
            cregs.PC += 1;
            break;
        }
        case 0x28: // PLP
        {
            uint8 d = Pop();
            flags.N = d & flagNMask;
            flags.V = d & flagVMask;
            flags.M = d & flagMMask;
            flags.X = d & flagXMask;
            flags.D = d & flagDMask;
            flags.I = d & flagIMask;
            flags.Z = d & flagZMask;
            flags.C = d & flagCMask;

            cregs.PC += 1;
            // cout << "new flags " << (uint16)flags << endl;

            break;
        }
        case 0x2B: // PLD
        {
            uint16 d = PopWord();
            cregs.D = d;
            flags.N = cregs.D & 0x8000;
            flags.Z = !cregs.D; // Since operation is 16 bit
            cregs.PC += 1;
            break;
        }

        case 0xf4: // PEA
        {

            ResolveAddress(CPUAddressingMode::Imm);
            PushWord(ReadWord());
            cregs.PC += 3;
            break;
        }
        case 0xd4: // PEI
        {
            // TODO : exception in address resolve
            ResolveAddress(CPUAddressingMode::Dir);
            PushWord(ReadWord()); // Or ReadM?
            cregs.PC += 2;
            break;
        }
        case 0x62: // PER
        {
            ResolveAddress(CPUAddressingMode::Imm);
            signed short imm = ReadWord();
            PushWord(PCByteAhead(0) + 3 + imm);
            cregs.PC += 3;
            break;
        }
#pragma endregion

#pragma region Arithmatics

        case 0x61: // ADC (dir,x)
        {
            // TODO : BCD for D flag
            ResolveAddress(CPUAddressingMode::DirXPtr16);
            uint16 d = ReadM();
            doADC(d);
            cregs.PC += 2;
            break;
        }
        case 0x63: // ADC stk,S
        {
            // TODO : BCD for D flag
            ResolveAddress(CPUAddressingMode::STK);
            uint16 d = ReadM();
            doADC(d);
            cregs.PC += 2;
            break;
        }
        case 0x65: // ADC dir
        {
            // TODO : BCD for D flag
            ResolveAddress(CPUAddressingMode::Dir);
            uint16 d = ReadM();
            doADC(d);
            cregs.PC += 2;
            break;
        }
        case 0x67: // ADC [dir]
        {
            // TODO : BCD for D flag
            ResolveAddress(CPUAddressingMode::DirPtr24);
            uint16 d = ReadM();
            doADC(d);
            cregs.PC += 2;
            break;
        }
        case 0x69: // ADC imm
        {
            // TODO : BCD for D flag
            // cin.get();
            ResolveAddress(CPUAddressingMode::Imm);

            uint16 imm = ReadM();
            doADC(imm);

            cregs.PC += 3 - flags.M;

            break;
        }
        case 0x6d: // ADC abs
        {
            // TODO : BCD for D flag
            ResolveAddress(CPUAddressingMode::Abs);
            uint16 d = ReadM();
            doADC(d);
            cregs.PC += 3;
            break;
        }

        case 0x6f: // ADC long
        {
            // TODO : BCD for D flag
            ResolveAddress(CPUAddressingMode::Long);
            uint16 d = ReadM();
            doADC(d);
            cregs.PC += 4;
            break;
        }
        case 0x71: // ADC (dir),y
        {
            // TODO : BCD for D flag
            ResolveAddress(CPUAddressingMode::DirPtr16Y);
            uint16 d = ReadM();
            doADC(d);
            cregs.PC += 2;
            break;
        }

        case 0x72: // ADC (dir)
        {
            // TODO : BCD for D flag
            ResolveAddress(CPUAddressingMode::DirPtr16);
            uint16 d = ReadM();
            doADC(d);
            cregs.PC += 2;
            break;
        }
        case 0x73: // ADC (stk,s),y
        {
            // TODO : BCD for D flag
            ResolveAddress(CPUAddressingMode::STKPtr16Y);
            uint16 d = ReadM();
            doADC(d);
            cregs.PC += 2;
            break;
        }
        case 0x75: // ADC dir,x
        {
            // TODO : BCD for D flag
            ResolveAddress(CPUAddressingMode::DirX);
            uint16 d = ReadM();
            doADC(d);
            cregs.PC += 2;
            break;
        }
        case 0x77: // ADC [dir],y
        {
            // TODO : BCD for D flag
            ResolveAddress(CPUAddressingMode::DirPtr24Y);
            uint16 d = ReadM();
            doADC(d);
            cregs.PC += 2;
            break;
        }

        case 0x79: // ADC abs,y
        {
            // TODO : BCD for D flag
            ResolveAddress(CPUAddressingMode::AbsY);
            uint16 d = ReadM();
            doADC(d);
            cregs.PC += 3;
            break;
        }

        case 0x7d: // ADC abs,x
        {
            // TODO : BCD for D flag
            ResolveAddress(CPUAddressingMode::AbsX);
            uint16 d = ReadM();
            doADC(d);
            cregs.PC += 3;
            break;
        }

        case 0x7f: // ADC long,x
        {
            // TODO : BCD for D flag
            ResolveAddress(CPUAddressingMode::LongX);
            uint16 d = ReadM();
            doADC(d);
            cregs.PC += 4;
            break;
        }

        case 0xe1: // SBC (dir,x)
        {

            ResolveAddress(CPUAddressingMode::DirXPtr16);
            uint16 d = ReadM();
            doSBC(d);
            cregs.PC += 2;
            break;
        }
        case 0xe3: // SBC stk,S
        {

            ResolveAddress(CPUAddressingMode::STK);
            uint16 d = ReadM();
            doSBC(d);
            cregs.PC += 2;
            break;
        }

        case 0xe5: // SBC dir
        {

            ResolveAddress(CPUAddressingMode::Dir);
            uint16 d = ReadM();
            doSBC(d);
            cregs.PC += 2;
            break;
        }

        case 0xe7: // SBC [dir]
        {

            ResolveAddress(CPUAddressingMode::DirPtr24);
            uint16 d = ReadM();
            doSBC(d);
            cregs.PC += 2;
            break;
        }

        case 0xe9: // SBC imm
        {
            ResolveAddress(CPUAddressingMode::Imm);
            uint16 imm = ReadM();
            doSBC(imm);
            cregs.PC += 3 - flags.M;
            break;
        }

        case 0xed: // SBC abs
        {

            ResolveAddress(CPUAddressingMode::Abs);
            uint16 d = ReadM();
            doSBC(d);
            cregs.PC += 3;
            break;
        }
        case 0xef: // SBC long
        {

            ResolveAddress(CPUAddressingMode::Long);
            uint16 d = ReadM();
            doSBC(d);
            cregs.PC += 4;
            break;
        }
        case 0xf1: // SBC (dir),y
        {

            ResolveAddress(CPUAddressingMode::DirPtr16Y);
            uint16 d = ReadM();
            doSBC(d);
            cregs.PC += 2;
            break;
        }
        case 0xf2: // SBC (dir)
        {
            ResolveAddress(CPUAddressingMode::DirPtr16);
            uint16 d = ReadM();
            doSBC(d);
            cregs.PC += 2;
            break;
        }
        case 0xf3: // SBC (stk,S),y
        {

            ResolveAddress(CPUAddressingMode::STKPtr16Y);
            uint16 d = ReadM();
            doSBC(d);
            cregs.PC += 2;
            break;
        }
        case 0xf5: // SBC dir,x
        {

            ResolveAddress(CPUAddressingMode::DirX);
            uint16 d = ReadM();
            doSBC(d);
            cregs.PC += 2;
            break;
        }
        case 0xf7: // SBC [dir],y
        {

            ResolveAddress(CPUAddressingMode::DirPtr24Y);
            uint16 d = ReadM();
            doSBC(d);
            cregs.PC += 2;
            break;
        }
        case 0xf9: // SBC abs,y
        {

            ResolveAddress(CPUAddressingMode::AbsY);
            uint16 d = ReadM();
            doSBC(d);
            cregs.PC += 3;
            break;
        }
        case 0xfd: // SBC abs,x
        {

            ResolveAddress(CPUAddressingMode::AbsX);
            uint16 d = ReadM();
            doSBC(d);
            cregs.PC += 3;
            break;
        }
        case 0xff: // SBC long,x
        {

            ResolveAddress(CPUAddressingMode::LongX);
            uint16 d = ReadM();
            doSBC(d);
            cregs.PC += 4;
            break;
        }

        case 0x3a: // DEC acc
        {

            UpdateC(cregs.C - 1);
            flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
            cregs.PC += 1;
            break;
        }
        case 0xc6: // DEC dir
        {
            ResolveAddress(CPUAddressingMode::Dir);
            uint16 d = ReadM();
            d -= 1;
            WriteM(d);
            flags.N = d & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(d & (flags.M ? 0x00FF : 0xFFFF));

            cregs.PC += 2;
            break;
        }
        case 0xce: // DEC abs
        {
            ResolveAddress(CPUAddressingMode::Abs);
            uint16 d = ReadM();
            d -= 1;
            WriteM(d);
            flags.N = d & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(d & (flags.M ? 0x00FF : 0xFFFF));

            cregs.PC += 3;
            break;
        }
        case 0xd6: // DEC dir,x
        {
            ResolveAddress(CPUAddressingMode::DirX);
            uint16 d = ReadM();
            d -= 1;
            WriteM(d);
            flags.N = d & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(d & (flags.M ? 0x00FF : 0xFFFF));

            cregs.PC += 2;
            break;
        }
        case 0xde: // DEC abs,x
        {
            ResolveAddress(CPUAddressingMode::AbsX);
            uint16 d = ReadM();
            d -= 1;
            WriteM(d);
            flags.N = d & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(d & (flags.M ? 0x00FF : 0xFFFF));

            cregs.PC += 3;
            break;
        }

        case 0xca: // DEX
        {
            cregs.X -= 1;
            flags.N = cregs.X & (flags.X ? 0x0080 : 0x8000);
            flags.Z = !(cregs.X & (flags.X ? 0x00FF : 0xFFFF));
            cregs.PC += 1;
            break;
        }

        case 0x88: // DEY
        {
            cregs.Y -= 1;
            flags.N = cregs.Y & (flags.X ? 0x0080 : 0x8000);
            flags.Z = !(cregs.Y & (flags.X ? 0x00FF : 0xFFFF));
            cregs.PC += 1;
            break;
        }

        case 0x1a: // INC acc
        {

            UpdateC(cregs.C + 1);
            flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
            cregs.PC += 1;
            break;
        }

        case 0xe6: // INC dir
        {
            ResolveAddress(CPUAddressingMode::Dir);
            uint16 d = ReadM();
            d += 1;
            WriteM(d);
            flags.N = d & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(d & (flags.M ? 0x00FF : 0xFFFF));

            cregs.PC += 2;
            break;
        }
        case 0xee: // INC abs
        {
            ResolveAddress(CPUAddressingMode::Abs);
            uint16 d = ReadM();
            d += 1;
            WriteM(d);
            flags.N = d & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(d & (flags.M ? 0x00FF : 0xFFFF));

            cregs.PC += 3;
            break;
        }
        case 0xf6: // INC dir,x
        {
            ResolveAddress(CPUAddressingMode::DirX);
            uint16 d = ReadM();
            d += 1;
            WriteM(d);
            flags.N = d & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(d & (flags.M ? 0x00FF : 0xFFFF));

            cregs.PC += 2;
            break;
        }
        case 0xfe: // INC abs,x
        {
            ResolveAddress(CPUAddressingMode::AbsX);
            uint16 d = ReadM();
            d += 1;
            WriteM(d);
            flags.N = d & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(d & (flags.M ? 0x00FF : 0xFFFF));

            cregs.PC += 3;
            break;
        }

        case 0xe8: // INX
        {
            cregs.X += 1;
            flags.N = cregs.X & (flags.X ? 0x0080 : 0x8000);
            flags.Z = !(cregs.X & (flags.X ? 0x00FF : 0xFFFF));
            cregs.PC += 1;
            break;
        }
        case 0xc8: // INY
        {
            cregs.Y += 1;

            flags.N = cregs.Y & (flags.X ? 0x0080 : 0x8000);
            flags.Z = !(cregs.Y & (flags.X ? 0x00FF : 0xFFFF));
            cregs.PC += 1;
            break;
        }

#pragma endregion

#pragma region Bitwise_Arithmatics
        case 0x21: // AND (dir,x)
        {
            ResolveAddress(CPUAddressingMode::DirXPtr16);
            uint16 d = ReadM();
            UpdateC(cregs.C & d);
            flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
            cregs.PC += 2;

            break;
        }
        case 0x23: // AND stk,S
        {
            ResolveAddress(CPUAddressingMode::STK);
            uint16 d = ReadM();
            UpdateC(cregs.C & d);
            flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
            cregs.PC += 2;
            break;
        }
        case 0x25: // AND dir
        {
            ResolveAddress(CPUAddressingMode::Dir);
            uint16 d = ReadM();
            UpdateC(cregs.C & d);
            flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
            cregs.PC += 2;
            break;
        }
        case 0x27: // AND [dir]
        {
            ResolveAddress(CPUAddressingMode::DirPtr24);
            uint16 d = ReadM();
            UpdateC(cregs.C & d);
            flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
            cregs.PC += 2;
            break;
        }
        case 0x29: // AND imm
        {
            ResolveAddress(CPUAddressingMode::Imm);
            uint16 d = ReadM();
            UpdateC(cregs.C & d);
            flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
            cregs.PC += 3 - flags.M;
            break;
        }
        case 0x2d: // AND abs
        {
            ResolveAddress(CPUAddressingMode::Abs);
            uint16 d = ReadM();
            UpdateC(cregs.C & d);
            flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
            cregs.PC += 3;
            break;
        }
        case 0x2f: // AND long
        {
            ResolveAddress(CPUAddressingMode::Long);
            uint16 d = ReadM();
            UpdateC(cregs.C & d);
            flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
            cregs.PC += 4;
            break;
        }

        case 0x31: // AND (dir),y
        {
            ResolveAddress(CPUAddressingMode::DirPtr16Y);
            uint16 d = ReadM();
            UpdateC(cregs.C & d);
            flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
            cregs.PC += 2;
            break;
        }

        case 0x32: // AND (dir)
        {
            ResolveAddress(CPUAddressingMode::DirPtr16);
            uint16 d = ReadM();
            UpdateC(cregs.C & d);
            flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
            cregs.PC += 2;
            break;
        }

        case 0x33: // AND (stks,S),y
        {
            ResolveAddress(CPUAddressingMode::STKPtr16Y);
            uint16 d = ReadM();
            UpdateC(cregs.C & d);
            flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
            cregs.PC += 2;
            break;
        }

        case 0x35: // AND dir,x
        {
            ResolveAddress(CPUAddressingMode::DirX);
            uint16 d = ReadM();
            UpdateC(cregs.C & d);
            flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
            cregs.PC += 2;
            break;
        }
        case 0x37: // AND [dir],y
        {
            ResolveAddress(CPUAddressingMode::DirPtr24Y);
            uint16 d = ReadM();
            UpdateC(cregs.C & d);
            flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
            cregs.PC += 2;
            break;
        }

        case 0x39: // AND abs,y
        {
            ResolveAddress(CPUAddressingMode::AbsY);
            uint16 d = ReadM();
            UpdateC(cregs.C & d);
            flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
            cregs.PC += 3;

            break;
        }
        case 0x3d: // AND abs,x
        {
            ResolveAddress(CPUAddressingMode::AbsX);
            uint16 d = ReadM();
            UpdateC(cregs.C & d);
            flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
            cregs.PC += 3;
            // cin.get();
            break;
        }

        case 0x3f: // AND long,x
        {
            ResolveAddress(CPUAddressingMode::LongX);
            uint16 d = ReadM();
            UpdateC(cregs.C & d);
            flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
            cregs.PC += 4;
            break;
        }

        // EORS
        case 0x41: // EOR (dir,x)
        {
            ResolveAddress(CPUAddressingMode::DirXPtr16);
            uint16 d = ReadM();
            UpdateC(cregs.C ^ d);
            flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
            cregs.PC += 2;
            break;
        }
        case 0x43: // EOR stk,S
        {
            ResolveAddress(CPUAddressingMode::STK);
            uint16 d = ReadM();
            UpdateC(cregs.C ^ d);
            flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
            cregs.PC += 2;
            break;
        }
        case 0x45: // EOR dir
        {
            ResolveAddress(CPUAddressingMode::Dir);
            uint16 d = ReadM();
            UpdateC(cregs.C ^ d);
            flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
            cregs.PC += 2;
            break;
        }
        case 0x47: // EOR [dir]
        {
            ResolveAddress(CPUAddressingMode::DirPtr24);
            uint16 d = ReadM();
            UpdateC(cregs.C ^ d);
            flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
            cregs.PC += 2;
            break;
        }
        case 0x49: // EOR imm
        {
            ResolveAddress(CPUAddressingMode::Imm);
            uint16 d = ReadM();
            UpdateC(cregs.C ^ d);
            flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
            cregs.PC += 3 - flags.M;
            break;
        }
        case 0x4d: // EOR abs
        {
            ResolveAddress(CPUAddressingMode::Abs);
            uint16 d = ReadM();
            UpdateC(cregs.C ^ d);
            flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
            cregs.PC += 3;
            break;
        }
        case 0x4f: // EOR long
        {
            ResolveAddress(CPUAddressingMode::Long);
            uint16 d = ReadM();
            UpdateC(cregs.C ^ d);
            flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
            cregs.PC += 4;
            break;
        }

        case 0x51: // EOR (dir),y
        {
            ResolveAddress(CPUAddressingMode::DirPtr16Y);
            uint16 d = ReadM();
            UpdateC(cregs.C ^ d);
            flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
            cregs.PC += 2;
            break;
        }

        case 0x52: // EOR (dir)
        {
            ResolveAddress(CPUAddressingMode::DirPtr16);
            uint16 d = ReadM();
            UpdateC(cregs.C ^ d);
            flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
            cregs.PC += 2;
            break;
        }

        case 0x53: // EOR (stks,S),y
        {
            ResolveAddress(CPUAddressingMode::STKPtr16Y);
            uint16 d = ReadM();
            UpdateC(cregs.C ^ d);
            flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
            cregs.PC += 2;
            break;
        }

        case 0x55: // EOR dir,x
        {
            ResolveAddress(CPUAddressingMode::DirX);
            uint16 d = ReadM();
            UpdateC(cregs.C ^ d);
            flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
            cregs.PC += 2;
            break;
        }
        case 0x57: // EOR [dir],y
        {
            ResolveAddress(CPUAddressingMode::DirPtr24Y);
            uint16 d = ReadM();
            UpdateC(cregs.C ^ d);
            flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
            cregs.PC += 2;
            break;
        }

        case 0x59: // EOR abs,y
        {
            ResolveAddress(CPUAddressingMode::AbsY);
            uint16 d = ReadM();
            UpdateC(cregs.C ^ d);
            flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
            cregs.PC += 3;
            break;
        }
        case 0x5d: // EOR abs,x
        {
            ResolveAddress(CPUAddressingMode::AbsX);
            uint16 d = ReadM();
            UpdateC(cregs.C ^ d);
            flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
            cregs.PC += 3;
            break;
        }

        case 0x5f: // EOR long,x
        {
            ResolveAddress(CPUAddressingMode::LongX);
            uint16 d = ReadM();
            UpdateC(cregs.C ^ d);
            flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
            cregs.PC += 4;
            break;
        }

        // ORAs
        case 0x01: // ORA (dir,x)
        {
            ResolveAddress(CPUAddressingMode::DirXPtr16);
            uint16 d = ReadM();
            UpdateC(cregs.C | d);
            flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
            cregs.PC += 2;
            break;
        }
        case 0x03: // ORA stk,S
        {
            ResolveAddress(CPUAddressingMode::STK);
            uint16 d = ReadM();
            UpdateC(cregs.C | d);
            flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
            cregs.PC += 2;
            break;
        }
        case 0x05: // ORA dir
        {
            ResolveAddress(CPUAddressingMode::Dir);
            uint16 d = ReadM();
            UpdateC(cregs.C | d);
            flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
            cregs.PC += 2;
            break;
        }
        case 0x07: // ORA [dir]
        {
            ResolveAddress(CPUAddressingMode::DirPtr24);
            uint16 d = ReadM();
            UpdateC(cregs.C | d);
            flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
            cregs.PC += 2;
            break;
        }
        case 0x09: // ORA imm
        {
            ResolveAddress(CPUAddressingMode::Imm);
            uint16 d = ReadM();
            UpdateC(cregs.C | d);
            flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
            cregs.PC += 3 - flags.M;
            break;
        }
        case 0x0d: // ORA abs
        {
            ResolveAddress(CPUAddressingMode::Abs);
            uint16 d = ReadM();
            UpdateC(cregs.C | d);
            flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
            cregs.PC += 3;
            break;
        }
        case 0x0f: // ORA long
        {
            ResolveAddress(CPUAddressingMode::Long);
            uint16 d = ReadM();
            UpdateC(cregs.C | d);
            flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
            cregs.PC += 4;
            break;
        }

        case 0x11: // ORA (dir),y
        {
            ResolveAddress(CPUAddressingMode::DirPtr16Y);
            uint16 d = ReadM();
            UpdateC(cregs.C | d);
            flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
            cregs.PC += 2;
            break;
        }

        case 0x12: // ORA (dir)
        {
            ResolveAddress(CPUAddressingMode::DirPtr16);
            uint16 d = ReadM();
            UpdateC(cregs.C | d);
            flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
            cregs.PC += 2;
            break;
        }

        case 0x13: // ORA (stks,S),y
        {
            ResolveAddress(CPUAddressingMode::STKPtr16Y);
            uint16 d = ReadM();
            UpdateC(cregs.C | d);
            flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
            cregs.PC += 2;
            break;
        }

        case 0x15: // ORA dir,x
        {
            ResolveAddress(CPUAddressingMode::DirX);
            uint16 d = ReadM();
            UpdateC(cregs.C | d);
            flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
            cregs.PC += 2;
            break;
        }
        case 0x17: // ORA [dir],y
        {
            ResolveAddress(CPUAddressingMode::DirPtr24Y);
            uint16 d = ReadM();
            UpdateC(cregs.C | d);
            flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
            cregs.PC += 2;
            break;
        }

        case 0x19: // ORA abs,y
        {
            ResolveAddress(CPUAddressingMode::AbsY);
            uint16 d = ReadM();
            UpdateC(cregs.C | d);
            flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
            cregs.PC += 3;
            break;
        }
        case 0x1d: // ORA abs,x
        {
            ResolveAddress(CPUAddressingMode::AbsX);
            uint16 d = ReadM();
            UpdateC(cregs.C | d);
            flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
            cregs.PC += 3;
            break;
        }

        case 0x1f: // ORA long,x
        {
            ResolveAddress(CPUAddressingMode::LongX);
            uint16 d = ReadM();
            UpdateC(cregs.C | d);
            flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
            cregs.PC += 4;
            break;
        }

        case 0x06: // ASL dir
        {
            ResolveAddress(CPUAddressingMode::Dir);
            uint16 t = ReadM();
            if (flags.M)
            {

                flags.C = t & 0x0080;
            }
            else
            {
                flags.C = t & 0x8000;
            }
            t = t << 1;
            WriteM(t);
            flags.N = t & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(t & (flags.M ? 0x00FF : 0xFFFF));

            cregs.PC += 2;
            break;
        }
        case 0x0a: // ASL acc
        {
            if (flags.M)
            {

                flags.C = cregs.C & 0x0080;
            }
            else
            {
                flags.C = cregs.C & 0x8000;
            }
            UpdateC(cregs.C << 1);
            flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));

            cregs.PC += 1;
            break;
        }
        case 0x0e: // ASL abs
        {
            ResolveAddress(CPUAddressingMode::Abs);
            uint16 t = ReadM();
            if (flags.M)
            {

                flags.C = t & 0x0080;
            }
            else
            {
                flags.C = t & 0x8000;
            }
            t = t << 1;
            WriteM(t);
            flags.N = t & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(t & (flags.M ? 0x00FF : 0xFFFF));

            cregs.PC += 3;
            break;
        }

        case 0x16: // ASL dir,x
        {
            ResolveAddress(CPUAddressingMode::DirX);
            uint16 t = ReadM();
            if (flags.M)
            {

                flags.C = t & 0x0080;
            }
            else
            {
                flags.C = t & 0x8000;
            }
            t = t << 1;
            WriteM(t);
            flags.N = t & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(t & (flags.M ? 0x00FF : 0xFFFF));

            cregs.PC += 2;
            break;
        }

        case 0x1e: // ASL abs,x
        {
            ResolveAddress(CPUAddressingMode::AbsX);
            uint16 t = ReadM();
            if (flags.M)
            {

                flags.C = t & 0x0080;
            }
            else
            {
                flags.C = t & 0x8000;
            }
            t = t << 1;
            WriteM(t);
            flags.N = t & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(t & (flags.M ? 0x00FF : 0xFFFF));

            cregs.PC += 3;
            break;
        }

        case 0x46: // LSR dir
        {
            ResolveAddress(CPUAddressingMode::Dir);
            uint16 t = ReadM();
            flags.C = t & 0x0001;
            t = t >> 1;
            WriteM(t);
            flags.N = 0;
            flags.Z = !t;
            cregs.PC += 2;
            break;
        }

        case 0x4a: // LSR acc
        {
            flags.C = cregs.C & 0x0001;
            uint16 t;
            if (flags.M)
                t = (cregs.C & 0x00FF) >> 1;
            else
                t = cregs.C >> 1;
            UpdateC(t);
            flags.N = 0;
            flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
            cregs.PC += 1;
            break;
        }

        case 0x4e: // LSR abs
        {
            ResolveAddress(CPUAddressingMode::Abs);
            uint16 t = ReadM();
            flags.C = t & 0x0001;
            t = t >> 1;
            WriteM(t);
            flags.N = 0;
            flags.Z = !t;
            cregs.PC += 3;
            break;
        }

        case 0x56: // LSR dir,x
        {
            ResolveAddress(CPUAddressingMode::DirX);
            uint16 t = ReadM();
            flags.C = t & 0x0001;
            t = t >> 1;
            WriteM(t);
            flags.N = 0;
            flags.Z = !t;
            cregs.PC += 2;
            break;
        }

        case 0x5e: // LSR abs,x
        {
            ResolveAddress(CPUAddressingMode::AbsX);
            uint16 t = ReadM();
            flags.C = t & 0x0001;
            t = t >> 1;
            WriteM(t);
            flags.N = 0;
            flags.Z = !t;
            cregs.PC += 3;
            break;
        }

        case 0x26: // ROL dir
        {

            ResolveAddress(CPUAddressingMode::Dir);
            uint16 t = ReadM();
            bool c = flags.C;
            if (flags.M)
            {
                flags.C = t & 0x0080;
            }
            else
            {
                flags.C = t & 0x8000;
            }
            t = (t << 1) | c;
            WriteM(t);
            flags.N = t & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(t & (flags.M ? 0x00FF : 0xFFFF));

            cregs.PC += 2;
            break;
        }

        case 0x2a: // ROL acc
        {

            if (flags.M)
            {
                bool c = flags.C;
                flags.C = cregs.C & 0x0080;
                UpdateC((cregs.C << 1) + c);
            }
            else
            {
                bool c = flags.C;
                flags.C = cregs.C & 0x8000;

                UpdateC((cregs.C << 1) + c);
            }
            flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));

            cregs.PC += 1;
            break;
        }

        case 0x2e: // ROL abs
        {

            ResolveAddress(CPUAddressingMode::Abs);
            uint16 t = ReadM();
            bool c = flags.C;
            if (flags.M)
            {
                flags.C = t & 0x0080;
            }
            else
            {
                flags.C = t & 0x8000;
            }
            t = (t << 1) | c;
            WriteM(t);
            flags.N = t & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(t & (flags.M ? 0x00FF : 0xFFFF));

            cregs.PC += 3;
            break;
        }

        case 0x36: // ROL dir,x
        {

            ResolveAddress(CPUAddressingMode::DirX);
            uint16 t = ReadM();
            bool c = flags.C;
            if (flags.M)
            {
                flags.C = t & 0x0080;
            }
            else
            {
                flags.C = t & 0x8000;
            }
            t = (t << 1) | c;
            WriteM(t);
            flags.N = t & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(t & (flags.M ? 0x00FF : 0xFFFF));

            cregs.PC += 2;
            break;
        }

        case 0x3e: // ROL abs,x
        {

            ResolveAddress(CPUAddressingMode::AbsX);
            uint16 t = ReadM();
            bool c = flags.C;
            if (flags.M)
            {
                flags.C = t & 0x0080;
            }
            else
            {
                flags.C = t & 0x8000;
            }
            t = (t << 1) | c;
            WriteM(t);
            flags.N = t & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(t & (flags.M ? 0x00FF : 0xFFFF));

            cregs.PC += 3;
            break;
        }

        case 0x66: // ROR dir
        {

            ResolveAddress(CPUAddressingMode::Dir);
            uint16 t = ReadM();
            bool c = flags.C;
            flags.C = t & 0x0001;
            t = (t >> 1) | (flags.M ? c << 7 : c << 15);

            WriteM(t);
            flags.N = t & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(t & (flags.M ? 0x00FF : 0xFFFF));

            cregs.PC += 2;
            break;
        }

        case 0x6a: // ROR acc
        {

            bool c = flags.C;
            flags.C = cregs.C & 0x0001;
            uint16 t;
            if (flags.M)
            {
                t = ((cregs.C & 0x00FF) >> 1) | (c << 7);
            }
            else
            {
                t = (cregs.C >> 1) | (c << 15);
            }
            UpdateC(t);

            flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));

            cregs.PC += 1;
            break;
        }

        case 0x6e: // ROR abs
        {

            ResolveAddress(CPUAddressingMode::Abs);
            uint16 t = ReadM();
            bool c = flags.C;
            flags.C = t & 0x0001;
            t = (t >> 1) | (flags.M ? c << 7 : c << 15);

            WriteM(t);
            flags.N = t & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(t & (flags.M ? 0x00FF : 0xFFFF));

            cregs.PC += 3;
            break;
        }

        case 0x76: // ROR dir,x
        {

            ResolveAddress(CPUAddressingMode::DirX);
            uint16 t = ReadM();
            bool c = flags.C;
            flags.C = t & 0x0001;
            t = (t >> 1) | (flags.M ? c << 7 : c << 15);

            WriteM(t);
            flags.N = t & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(t & (flags.M ? 0x00FF : 0xFFFF));

            cregs.PC += 2;
            break;
        }

        case 0x7e: // ROR abs,x
        {

            ResolveAddress(CPUAddressingMode::AbsX);
            uint16 t = ReadM();
            bool c = flags.C;
            flags.C = t & 0x0001;
            t = (t >> 1) | (flags.M ? c << 7 : c << 15);

            WriteM(t);
            flags.N = t & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(t & (flags.M ? 0x00FF : 0xFFFF));

            cregs.PC += 3;
            break;
        }
#pragma endregion

#pragma region Reg_Transfer

        case 0xeb: // XBA
        {
            cregs.C = ((cregs.C & 0xFF00) >> 8) | ((cregs.C & 0x00FF) << 8);
            // Flags are always set based on the lower 8 bits (in this case)
            flags.N = cregs.C & 0x0080;
            flags.Z = !(cregs.C & 0x00FF);
            cregs.PC += 1;
            break;
        }

        case 0x5b: // TCD - D <- C
        {
            cregs.D = cregs.C;

            flags.N = cregs.C & 0x8000;
            flags.Z = !(cregs.C);

            cregs.PC += 1;
            break;
        }
        case 0x1b: // TCS - S <- C
        {
            cregs.S = cregs.C;
            // Yes, no flags here
            cregs.PC += 1;
            break;
        }

        case 0x7b: // TDC - C <- D
        {
            cregs.C = cregs.D; // 16 bit transfer always

            flags.N = cregs.C & 0x8000;
            flags.Z = !(cregs.C);

            cregs.PC += 1;
            break;
        }

        case 0x3b: // TSC - C <- S
        {
            cregs.C = cregs.S; // 16 bit always

            flags.N = cregs.C & 0x8000;
            flags.Z = !(cregs.C);

            cregs.PC += 1;
            break;
        }

        case 0xaa: // TAX - X <- C
        {
            // TODO : in 8 bit mode, how should X.H be affected?
            // Replaced? keep what ever there was?
            // Should we instead write : cregs.X.L = cregs.C.L?
            UpdateX(cregs.C);
            flags.N = cregs.X & (flags.X ? 0x0080 : 0x8000);
            flags.Z = !(cregs.X & (flags.X ? 0x00FF : 0xFFFF));
            cregs.PC += 1;
            break;
        }
        case 0xa8: // TAY - Y <- C
        {
            UpdateY(cregs.C);
            flags.N = cregs.Y & (flags.X ? 0x0080 : 0x8000);
            flags.Z = !(cregs.Y & (flags.X ? 0x00FF : 0xFFFF));
            cregs.PC += 1;
            break;
        }

        case 0xba: // TSX - X <- S
        {

            UpdateX(cregs.S);
            flags.N = cregs.X & (flags.X ? 0x0080 : 0x8000);
            flags.Z = !(cregs.X & (flags.X ? 0x00FF : 0xFFFF));
            cregs.PC += 1;
            break;
        }
        case 0x8a: // TXA - C <- X
        {

            UpdateC(cregs.X);
            flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
            cregs.PC += 1;
            break;
        }
        case 0x9a: // TXS - S <- X
        {

            cregs.S = cregs.X;

            cregs.PC += 1;
            break;
        }
        case 0x9b: // TXY - Y <- X
        {

            UpdateY(cregs.X);
            flags.N = cregs.Y & (flags.X ? 0x0080 : 0x8000);
            flags.Z = !(cregs.Y & (flags.X ? 0x00FF : 0xFFFF));
            cregs.PC += 1;
            break;
        }

        case 0x98: // TYA - C <- Y
        {

            UpdateC(cregs.Y);
            flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
            cregs.PC += 1;
            break;
        }
        case 0xbb: // TYX - X <- Y
        {

            UpdateX(cregs.Y);
            flags.N = cregs.X & (flags.X ? 0x0080 : 0x8000);
            flags.Z = !(cregs.X & (flags.X ? 0x00FF : 0xFFFF));
            cregs.PC += 1;
            break;
        }
#pragma endregion

#pragma region Load_and_Store

        case 0xa1: // LDA (dir,x)
        {
            ResolveAddress(CPUAddressingMode::DirXPtr16);
            UpdateC(ReadM());
            flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF)); // If M is one, the upper bits of C is already 0, so theres no need for checking JUST the 8 lower bits.
            cregs.PC += 2;
            break;
        }
        case 0xa3: // LDA stk,S
        {
            ResolveAddress(CPUAddressingMode::STK);
            UpdateC(ReadM());
            flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF)); // If M is one, the upper bits of C is already 0, so theres no need for checking JUST the 8 lower bits.
            cregs.PC += 2;
            break;
        }
        case 0xa5: // LDA dir
        {
            ResolveAddress(CPUAddressingMode::Dir);
            UpdateC(ReadM());
            flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF)); // If M is one, the upper bits of C is already 0, so theres no need for checking JUST the 8 lower bits.
            cregs.PC += 2;
            break;
        }
        case 0xa7: // LDA  [dir]
        {
            ResolveAddress(CPUAddressingMode::DirPtr24);
            uint16 d = ReadM();
            UpdateC(d);

            flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF)); // If M is one, the upper bits of C is already 0, so theres no need for checking JUST the 8 lower bits.
            cregs.PC += 2;
            break;
        }
        case 0xa9: // LDA imm
        {
            ResolveAddress(CPUAddressingMode::Imm);
            uint16 imm = ReadM();
            UpdateC(imm);

            flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF)); // If M is one, the upper bits of C is already 0, so theres no need for checking JUST the 8 lower bits.
            cregs.PC += 3 - flags.M;
            break;
        }
        case 0xad: // LDA abs
        {
            ResolveAddress(CPUAddressingMode::Abs);
            uint16 d = ReadM();
            UpdateC(d);

            flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF)); // If M is one, the upper bits of C is already 0, so theres no need for checking JUST the 8 lower bits.
            cregs.PC += 3;
            break;
        }
        case 0xaf: // LDA long
        {
            ResolveAddress(CPUAddressingMode::Long);
            UpdateC(ReadM());
            flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF)); // If M is one, the upper bits of C is already 0, so theres no need for checking JUST the 8 lower bits.
            cregs.PC += 4;
            break;
        }

        case 0xb1: // LDA (dir),y
        {
            ResolveAddress(CPUAddressingMode::DirPtr16Y);
            UpdateC(ReadM());
            flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF)); // If M is one, the upper bits of C is already 0, so theres no need for checking JUST the 8 lower bits.
            cregs.PC += 2;
            break;
        }

        case 0xb2: // LDA (dir)
        {
            ResolveAddress(CPUAddressingMode::DirPtr16);
            UpdateC(ReadM());
            flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF)); // If M is one, the upper bits of C is already 0, so theres no need for checking JUST the 8 lower bits.
            cregs.PC += 2;
            break;
        }

        case 0xb3: // LDA (stk,S),y
        {
            ResolveAddress(CPUAddressingMode::STKPtr16Y);
            UpdateC(ReadM());
            flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF)); // If M is one, the upper bits of C is already 0, so theres no need for checking JUST the 8 lower bits.
            cregs.PC += 2;
            break;
        }
        case 0xb5: // LDA dir,x
        {
            ResolveAddress(CPUAddressingMode::DirX);
            UpdateC(ReadM());
            flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF)); // If M is one, the upper bits of C is already 0, so theres no need for checking JUST the 8 lower bits.
            cregs.PC += 2;
            break;
        }
        case 0xb7: // LDA [dir],y
        {
            ResolveAddress(CPUAddressingMode::DirPtr24Y);
            UpdateC(ReadM());
            flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF)); // If M is one, the upper bits of C is already 0, so theres no need for checking JUST the 8 lower bits.
            cregs.PC += 2;
            break;
        }

        case 0xb9: // LDA abs,y
        {
            ResolveAddress(CPUAddressingMode::AbsY);
            uint16 d = ReadM();
            UpdateC(d);

            flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF)); // If M is one, the upper bits of C is already 0, so theres no need for checking JUST the 8 lower bits.
            cregs.PC += 3;
            break;
        }
        case 0xbd: // LDA abs,x
        {
            ResolveAddress(CPUAddressingMode::AbsX);
            uint16 d = ReadM();
            UpdateC(d);

            flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF)); // If M is one, the upper bits of C is already 0, so theres no need for checking JUST the 8 lower bits.
            cregs.PC += 3;
            break;
        }
        case 0xbf: // LDA long,x
        {
            ResolveAddress(CPUAddressingMode::LongX);
            UpdateC(ReadM());
            flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF)); // If M is one, the upper bits of C is already 0, so theres no need for checking JUST the 8 lower bits.
            cregs.PC += 4;
            break;
        }
        case 0xa2: // LDX imm
        {
            ResolveAddress(CPUAddressingMode::Imm);
            uint16 imm = ReadX();

            UpdateX(imm);

            flags.N = cregs.X & (flags.X ? 0x0080 : 0x8000);
            flags.Z = !(cregs.X & (flags.X ? 0x00FF : 0xFFFF));
            cregs.PC += 3 - flags.X;
            break;
        }
        case 0xa6: // LDX dir
        {
            ResolveAddress(CPUAddressingMode::Dir);
            UpdateX(ReadX());
            flags.N = cregs.X & (flags.X ? 0x0080 : 0x8000);
            flags.Z = !(cregs.X & (flags.X ? 0x00FF : 0xFFFF));
            cregs.PC += 2;
            break;
        }
        case 0xae: // LDX abs
        {
            ResolveAddress(CPUAddressingMode::Abs);
            UpdateX(ReadX());
            flags.N = cregs.X & (flags.X ? 0x0080 : 0x8000);
            flags.Z = !(cregs.X & (flags.X ? 0x00FF : 0xFFFF));
            cregs.PC += 3;
            break;
        }

        case 0xb6: // LDX dir,y
        {
            ResolveAddress(CPUAddressingMode::DirY);
            UpdateX(ReadX());
            flags.N = cregs.X & (flags.X ? 0x0080 : 0x8000);
            flags.Z = !(cregs.X & (flags.X ? 0x00FF : 0xFFFF));
            cregs.PC += 2;
            break;
        }

        case 0xbe: // LDX abs,y
        {
            ResolveAddress(CPUAddressingMode::AbsY);
            UpdateX(ReadX());
            flags.N = cregs.X & (flags.X ? 0x0080 : 0x8000);
            flags.Z = !(cregs.X & (flags.X ? 0x00FF : 0xFFFF));
            cregs.PC += 3;
            break;
        }

        case 0xa0: // LDY imm
        {
            ResolveAddress(CPUAddressingMode::Imm);
            uint16 imm = ReadX();

            UpdateY(imm);

            flags.N = cregs.Y & (flags.X ? 0x0080 : 0x8000);
            flags.Z = !(cregs.Y & (flags.X ? 0x00FF : 0xFFFF));
            cregs.PC += 3 - flags.X;
            break;
        }

        case 0xa4: // LDY dir
        {
            ResolveAddress(CPUAddressingMode::Dir);
            UpdateY(ReadX());
            flags.N = cregs.Y & (flags.X ? 0x0080 : 0x8000);
            flags.Z = !(cregs.Y & (flags.X ? 0x00FF : 0xFFFF));
            cregs.PC += 2;
            break;
        }

        case 0xac: // LDY abs
        {
            ResolveAddress(CPUAddressingMode::Abs);
            UpdateY(ReadX());
            flags.N = cregs.Y & (flags.X ? 0x0080 : 0x8000);
            flags.Z = !(cregs.Y & (flags.X ? 0x00FF : 0xFFFF));
            cregs.PC += 3;
            break;
        }
        case 0xb4: // LDY dir,x
        {
            ResolveAddress(CPUAddressingMode::DirX);
            UpdateY(ReadX());
            flags.N = cregs.Y & (flags.X ? 0x0080 : 0x8000);
            flags.Z = !(cregs.Y & (flags.X ? 0x00FF : 0xFFFF));
            cregs.PC += 2;
            break;
        }
        case 0xbc: // LDY abs,x
        {
            ResolveAddress(CPUAddressingMode::AbsX);
            UpdateY(ReadX());
            flags.N = cregs.Y & (flags.X ? 0x0080 : 0x8000);
            flags.Z = !(cregs.Y & (flags.X ? 0x00FF : 0xFFFF));
            cregs.PC += 3;
            break;
        }

        case 0x81: // STA (dir,x)
        {

            ResolveAddress(CPUAddressingMode::DirXPtr16);
            WriteM(cregs.C);
            // No Flags
            cregs.PC += 2;
            break;
        }

        case 0x83: // STA stk,s
        {

            ResolveAddress(CPUAddressingMode::STK);
            WriteM(cregs.C);
            // No Flags
            cregs.PC += 2;
            break;
        }
        case 0x85: // STA dir
        {

            ResolveAddress(CPUAddressingMode::Dir);
            WriteM(cregs.C);
            // No Flags
            cregs.PC += 2;
            break;
        }
        case 0x87: // STA [dir]
        {

            ResolveAddress(CPUAddressingMode::DirPtr24);
            WriteM(cregs.C);
            // No Flags
            cregs.PC += 2;
            break;
        }
        case 0x8d: // STA abs
        {
            ResolveAddress(CPUAddressingMode::Abs);
            WriteM(cregs.C);
            cregs.PC += 3;
            break;
        }
        case 0x8f: // STA long
        {
            ResolveAddress(CPUAddressingMode::Long);

            WriteM(cregs.C);
            cregs.PC += 4;
            break;
        }
        case 0x91: // STA (dir),y
        {

            ResolveAddress(CPUAddressingMode::DirPtr16Y);
            WriteM(cregs.C);
            // No Flags
            cregs.PC += 2;
            break;
        }
        case 0x92: // STA (dir)
        {
            ResolveAddress(CPUAddressingMode::DirPtr16);

            WriteM(cregs.C);
            // No Flags
            cregs.PC += 2;
            break;
        }
        case 0x93: // STA (stk,s),y
        {

            ResolveAddress(CPUAddressingMode::STKPtr16Y);
            WriteM(cregs.C);
            // No Flags
            cregs.PC += 2;
            break;
        }
        case 0x95: // STA dir,x
        {

            ResolveAddress(CPUAddressingMode::DirX);
            WriteM(cregs.C);
            // No Flags
            cregs.PC += 2;
            break;
        }
        case 0x97: // STA [dir],y
        {

            ResolveAddress(CPUAddressingMode::DirPtr24Y);
            WriteM(cregs.C);
            // No Flags
            cregs.PC += 2;
            break;
        }
        case 0x99: // STA abs,y
        {
            ResolveAddress(CPUAddressingMode::AbsY);
            WriteM(cregs.C);
            cregs.PC += 3;
            break;
        }

        case 0x9d: // STA abs,x
        {
            ResolveAddress(CPUAddressingMode::AbsX);
            WriteM(cregs.C);
            cregs.PC += 3;
            break;
        }

        case 0x9f: // STA long,x
        {
            ResolveAddress(CPUAddressingMode::LongX);
            WriteM(cregs.C);
            cregs.PC += 4;
            break;
        }

        case 0x86: // STX dir
        {

            ResolveAddress(CPUAddressingMode::Dir);
            WriteX(cregs.X);
            // No Flags
            cregs.PC += 2;
            break;
        }
        case 0x8e: // STX abs
        {
            ResolveAddress(CPUAddressingMode::Abs);
            WriteX(cregs.X);
            cregs.PC += 3;
            break;
        }
        case 0x96: // STX dir,y
        {

            ResolveAddress(CPUAddressingMode::DirY);
            WriteX(cregs.X);
            // No Flags
            cregs.PC += 2;
            break;
        }

        case 0x84: // STY dir
        {

            ResolveAddress(CPUAddressingMode::Dir);
            WriteX(cregs.Y);
            // No Flags
            cregs.PC += 2;
            break;
        }

        case 0x8c: // STY abs
        {
            ResolveAddress(CPUAddressingMode::Abs);
            WriteX(cregs.Y);
            cregs.PC += 3;
            break;
        }

        case 0x94: // STY dir,x
        {

            ResolveAddress(CPUAddressingMode::DirX);
            WriteX(cregs.Y);
            // No Flags
            cregs.PC += 2;
            break;
        }

        case 0x64: // STZ dir
        {

            ResolveAddress(CPUAddressingMode::Dir);
            WriteM(0);
            // No Flags
            cregs.PC += 2;
            break;
        }
        case 0x74: // STZ dir,x
        {

            ResolveAddress(CPUAddressingMode::DirX);
            WriteM(0);
            // No Flags
            cregs.PC += 2;
            break;
        }
        case 0x9c: // STZ abs
        {
            ResolveAddress(CPUAddressingMode::Abs);
            WriteM(0);
            cregs.PC += 3;
            break;
        }
        case 0x9e: // STZ abs,x
        {
            ResolveAddress(CPUAddressingMode::AbsX);
            WriteM(0);
            cregs.PC += 3;
            break;
        }
#pragma endregion

#pragma region Compare_and_Test
        case 0xc1: // CMP (dir,x)
        {
            // TODO : Check 8 bit operation
            ResolveAddress(CPUAddressingMode::DirXPtr16);
            uint16 d = ReadM();
            // cout << "CMP : " << std::hex << d << " " << cregs.C << endl;
            flags.C = (cregs.C & (flags.M ? 0x00FF : 0xFFFF)) >= d;
            d = cregs.C - d;
            flags.N = d & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(d & (flags.M ? 0x00FF : 0xFFFF));
            cregs.PC += 2;
            break;
        }
        case 0xc3: // CMP stk,s
        {
            // TODO : Check 8 bit operation
            ResolveAddress(CPUAddressingMode::STK);
            uint16 d = ReadM();
            // cout << "CMP : " << std::hex << d << " " << cregs.C << endl;
            flags.C = (cregs.C & (flags.M ? 0x00FF : 0xFFFF)) >= d;
            d = cregs.C - d;
            flags.N = d & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(d & (flags.M ? 0x00FF : 0xFFFF));
            cregs.PC += 2;
            break;
        }
        case 0xc5: // CMP dir
        {
            // TODO : Check 8 bit operation
            ResolveAddress(CPUAddressingMode::Dir);
            uint16 d = ReadM();
            // cout << "CMP : " << std::hex << d << " " << cregs.C << endl;
            flags.C = (cregs.C & (flags.M ? 0x00FF : 0xFFFF)) >= d;
            d = cregs.C - d;
            flags.N = d & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(d & (flags.M ? 0x00FF : 0xFFFF));
            cregs.PC += 2;
            break;
        }
        case 0xc7: // CMP [dir]
        {
            // TODO : Check 8 bit operation
            ResolveAddress(CPUAddressingMode::DirPtr24);
            uint16 d = ReadM();
            // cout << "CMP : " << std::hex << d << " " << cregs.C << endl;
            flags.C = (cregs.C & (flags.M ? 0x00FF : 0xFFFF)) >= d;
            d = cregs.C - d;
            flags.N = d & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(d & (flags.M ? 0x00FF : 0xFFFF));
            cregs.PC += 2;
            break;
        }
        case 0xc9: // CMP imm
        {
            ResolveAddress(CPUAddressingMode::Imm);
            uint16 imm = ReadM();

            flags.C = (cregs.C & (flags.M ? 0x00FF : 0xFFFF)) >= imm;
            imm = cregs.C - imm;

            flags.N = imm & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(imm & (flags.M ? 0x00FF : 0xFFFF));
            cregs.PC += 3 - flags.M;

            break;
        }
        case 0xcd: // CMP abs
        {
            // TODO : Check 8 bit operation
            ResolveAddress(CPUAddressingMode::Abs);
            uint16 d = ReadM();
            // cout << "CMP : " << std::hex << d << " " << cregs.C << endl;
            flags.C = (cregs.C & (flags.M ? 0x00FF : 0xFFFF)) >= d;
            d = cregs.C - d;
            flags.N = d & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(d & (flags.M ? 0x00FF : 0xFFFF));
            cregs.PC += 3;
            break;
        }
        case 0xcf: // CMP long
        {
            // TODO : Check 8 bit operation
            ResolveAddress(CPUAddressingMode::Long);
            uint16 d = ReadM();
            // cout << "CMP : " << std::hex << d << " " << cregs.C << endl;
            flags.C = (cregs.C & (flags.M ? 0x00FF : 0xFFFF)) >= d;
            d = cregs.C - d;
            flags.N = d & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(d & (flags.M ? 0x00FF : 0xFFFF));
            cregs.PC += 4;
            break;
        }
        case 0xd1: // CMP (dir),y
        {
            // TODO : Check 8 bit operation
            ResolveAddress(CPUAddressingMode::DirPtr16Y);
            uint16 d = ReadM();
            // cout << "CMP : " << std::hex << d << " " << cregs.C << endl;
            flags.C = (cregs.C & (flags.M ? 0x00FF : 0xFFFF)) >= d;
            d = cregs.C - d;
            flags.N = d & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(d & (flags.M ? 0x00FF : 0xFFFF));
            cregs.PC += 2;
            break;
        }
        case 0xd2: // CMP (dir)
        {
            // TODO : Check 8 bit operation
            ResolveAddress(CPUAddressingMode::DirPtr16);
            uint16 d = ReadM();
            // cout << "CMP : " << std::hex << d << " " << cregs.C << endl;
            flags.C = (cregs.C & (flags.M ? 0x00FF : 0xFFFF)) >= d;
            d = cregs.C - d;
            flags.N = d & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(d & (flags.M ? 0x00FF : 0xFFFF));
            cregs.PC += 2;
            break;
        }
        case 0xd3: // CMP (stk,s),y
        {
            // TODO : Check 8 bit operation
            ResolveAddress(CPUAddressingMode::STKPtr16Y);
            uint16 d = ReadM();
            // cout << "CMP : " << std::hex << d << " " << cregs.C << endl;
            flags.C = (cregs.C & (flags.M ? 0x00FF : 0xFFFF)) >= d;
            d = cregs.C - d;
            flags.N = d & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(d & (flags.M ? 0x00FF : 0xFFFF));
            cregs.PC += 2;
            break;
        }
        case 0xd5: // CMP dir,x
        {
            // TODO : Check 8 bit operation
            ResolveAddress(CPUAddressingMode::DirX);
            uint16 d = ReadM();
            // cout << "CMP : " << std::hex << d << " " << cregs.C << endl;
            flags.C = (cregs.C & (flags.M ? 0x00FF : 0xFFFF)) >= d;
            d = cregs.C - d;
            flags.N = d & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(d & (flags.M ? 0x00FF : 0xFFFF));
            cregs.PC += 2;
            break;
        }
        case 0xd7: // CMP [dir],y
        {
            // TODO : Check 8 bit operation
            ResolveAddress(CPUAddressingMode::DirPtr24Y);
            uint16 d = ReadM();
            // cout << "CMP : " << std::hex << d << " " << cregs.C << endl;
            flags.C = (cregs.C & (flags.M ? 0x00FF : 0xFFFF)) >= d;
            d = cregs.C - d;
            flags.N = d & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(d & (flags.M ? 0x00FF : 0xFFFF));
            cregs.PC += 2;
            break;
        }
        case 0xd9: // CMP abs,y
        {
            // TODO : Check 8 bit operation
            ResolveAddress(CPUAddressingMode::AbsY);
            uint16 d = ReadM();
            // cout << "CMP : " << std::hex << d << " " << cregs.C << endl;
            flags.C = (cregs.C & (flags.M ? 0x00FF : 0xFFFF)) >= d;
            d = cregs.C - d;
            flags.N = d & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(d & (flags.M ? 0x00FF : 0xFFFF));
            cregs.PC += 3;
            break;
        }
        case 0xdd: // CMP abs,x
        {
            // TODO : Check 8 bit operation
            ResolveAddress(CPUAddressingMode::AbsX);
            uint16 d = ReadM();
            // cout << "CMP : " << std::hex << d << " " << cregs.C << endl;
            flags.C = (cregs.C & (flags.M ? 0x00FF : 0xFFFF)) >= d;
            d = cregs.C - d;
            flags.N = d & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(d & (flags.M ? 0x00FF : 0xFFFF));
            cregs.PC += 3;
            break;
        }
        case 0xdf: // CMP long,x
        {
            // TODO : Check 8 bit operation
            ResolveAddress(CPUAddressingMode::LongX);
            uint16 d = ReadM();
            // cout << "CMP : " << std::hex << d << " " << cregs.C << endl;
            flags.C = (cregs.C & (flags.M ? 0x00FF : 0xFFFF)) >= d;
            d = cregs.C - d;
            flags.N = d & (flags.M ? 0x0080 : 0x8000);
            flags.Z = !(d & (flags.M ? 0x00FF : 0xFFFF));
            cregs.PC += 4;
            break;
        }
        case 0xe0: // CPX imm
        {
            ResolveAddress(CPUAddressingMode::Imm);
            uint16 imm = ReadX();
            flags.C = (cregs.X & (flags.X ? 0x00FF : 0xFFFF)) >= imm;

            imm = cregs.X - imm;
            flags.N = imm & (flags.X ? 0x0080 : 0x8000);
            flags.Z = !(imm & (flags.X ? 0x00FF : 0xFFFF));

            cregs.PC += 3 - flags.X;
            break;
        }
        case 0xe4: // CPX dir
        {
            ResolveAddress(CPUAddressingMode::Dir);
            uint16 d = ReadX();
            flags.C = (cregs.X & (flags.X ? 0x00FF : 0xFFFF)) >= d;
            d = cregs.X - d;
            flags.N = d & (flags.X ? 0x0080 : 0x8000);
            flags.Z = !(d & (flags.X ? 0x00FF : 0xFFFF));

            cregs.PC += 2;
            break;
        }
        case 0xec: // CPX abs
        {
            ResolveAddress(CPUAddressingMode::Abs);
            uint16 d = ReadX();
            // cout << "CPX " << hex << cregs.X << " , " << d << endl;
            flags.C = (cregs.X & (flags.X ? 0x00FF : 0xFFFF)) >= d;
            d = cregs.X - d;

            flags.N = d & (flags.X ? 0x0080 : 0x8000);
            flags.Z = !(d & (flags.X ? 0x00FF : 0xFFFF));

            cregs.PC += 3;
            break;
        }

        case 0xc0: // CPY imm
        {
            ResolveAddress(CPUAddressingMode::Imm);
            uint16 imm = ReadX();
            flags.C = (cregs.Y & (flags.X ? 0x00FF : 0xFFFF)) >= imm;
            imm = cregs.Y - imm;
            flags.N = imm & (flags.X ? 0x0080 : 0x8000);
            flags.Z = !(imm & (flags.X ? 0x00FF : 0xFFFF));

            cregs.PC += 3 - flags.X;
            break;
        }
        case 0xc4: // CPY dir
        {
            ResolveAddress(CPUAddressingMode::Dir);
            uint16 d = ReadX();
            flags.C = (cregs.Y & (flags.X ? 0x00FF : 0xFFFF)) >= d;
            d = cregs.Y - d;
            flags.N = d & (flags.X ? 0x0080 : 0x8000);
            flags.Z = !(d & (flags.X ? 0x00FF : 0xFFFF));

            cregs.PC += 2;
            break;
        }
        case 0xcc: // CPY abs
        {
            ResolveAddress(CPUAddressingMode::Abs);
            uint16 d = ReadX();
            flags.C = (cregs.Y & (flags.X ? 0x00FF : 0xFFFF)) >= d;
            d = cregs.Y - d;
            flags.N = d & (flags.X ? 0x0080 : 0x8000);
            flags.Z = !(d & (flags.X ? 0x00FF : 0xFFFF));

            cregs.PC += 3;
            break;
        }
        case 0x24: // BIT dir
        {
            ResolveAddress(CPUAddressingMode::Dir);
            uint16 d = ReadM();
            // cout << "BIT : " << hex << cregs.C << " , " << d << endl;

            uint16 t = cregs.C & d;
            flags.Z = !(t & (flags.M ? 0x00FF : 0xFFFF));
            flags.N = d & (flags.M ? 0x0080 : 0x8000);
            flags.V = d & (flags.M ? 0x0040 : 0x4000);
            cregs.PC += 2;
            break;
        }
        case 0x2c: // BIT abs
        {
            ResolveAddress(CPUAddressingMode::Abs);
            uint16 d = ReadM();
            // cout << "BIT : " << hex << cregs.C << " , " << d << endl;
            uint16 t = cregs.C & d;
            flags.Z = !(t & (flags.M ? 0x00FF : 0xFFFF));
            flags.N = d & (flags.M ? 0x0080 : 0x8000);
            flags.V = d & (flags.M ? 0x0040 : 0x4000);
            cregs.PC += 3;
            break;
        }
        case 0x34: // BIT dir,x
        {
            ResolveAddress(CPUAddressingMode::DirX);
            uint16 d = ReadM();
            uint16 t = cregs.C & d;
            flags.Z = !(t & (flags.M ? 0x00FF : 0xFFFF));
            flags.N = d & (flags.M ? 0x0080 : 0x8000);
            flags.V = d & (flags.M ? 0x0040 : 0x4000);
            cregs.PC += 2;
            break;
        }
        case 0x3c: // BIT abs,x
        {
            ResolveAddress(CPUAddressingMode::AbsX);
            uint16 d = ReadM();
            uint16 t = cregs.C & d;
            flags.Z = !(t & (flags.M ? 0x00FF : 0xFFFF)); // THE AND NOT THE ARGUMENT!!
            flags.N = d & (flags.M ? 0x0080 : 0x8000);
            flags.V = d & (flags.M ? 0x0040 : 0x4000);
            cregs.PC += 3;
            break;
        }
        case 0x89: // BIT imm
        {
            ResolveAddress(CPUAddressingMode::Imm);
            uint16 imm = ReadM();
            uint16 t = cregs.C & imm;
            flags.Z = !(t & (flags.M ? 0x00FF : 0xFFFF));
            cregs.PC += 3 - flags.M;
            break;
        }

        case 0x14: // TRB dir
        {
            ResolveAddress(CPUAddressingMode::Dir);
            uint16 d = ReadM();
            uint16 t = cregs.C & d;
            flags.Z = !(t & (flags.M ? 0x00FF : 0xFFFF));
            d = d & (~cregs.C);
            WriteM(d);

            cregs.PC += 2;
            break;
        }
        case 0x1c: // TRB abs
        {
            ResolveAddress(CPUAddressingMode::Abs);
            uint16 d = ReadM();
            uint16 t = cregs.C & d;
            flags.Z = !(t & (flags.M ? 0x00FF : 0xFFFF));
            d = d & (~cregs.C);
            WriteM(d);

            cregs.PC += 3;
            break;
        }
        case 0x04: // TSB dir
        {
            ResolveAddress(CPUAddressingMode::Dir);
            uint16 d = ReadM();
            uint16 t = cregs.C & d;
            flags.Z = !(t & (flags.M ? 0x00FF : 0xFFFF));
            d = d | cregs.C;
            WriteM(d);

            cregs.PC += 2;
            break;
        }
        case 0x0c: // TSB abs
        {
            ResolveAddress(CPUAddressingMode::Abs);
            uint16 d = ReadM();
            uint16 t = cregs.C & d;
            flags.Z = !(t & (flags.M ? 0x00FF : 0xFFFF));
            d = d | cregs.C;
            WriteM(d);

            cregs.PC += 3;
            break;
        }

#pragma endregion

#pragma region Flow_Control
        case 0x90: // BCC
        {
            ResolveAddress(CPUAddressingMode::Rel8);
            signed char ll = C65Read(addL);
            if (!flags.C)
                cregs.PC += ll;
            cregs.PC += 2;
            break;
        }
        case 0xB0: // BCS
        {
            ResolveAddress(CPUAddressingMode::Rel8);
            signed char ll = C65Read(addL);
            if (flags.C)
                cregs.PC += ll;
            cregs.PC += 2;
            break;
        }
        case 0xd0: // BNE rel8
        {
            ResolveAddress(CPUAddressingMode::Rel8);
            signed char ll = C65Read(addL);
            if (!flags.Z)
                cregs.PC += ll;
            cregs.PC += 2;
            break;
        }
        case 0x10: // BPL rel8
        {
            ResolveAddress(CPUAddressingMode::Rel8);
            signed char ll = C65Read(addL);
            if (!flags.N)
                cregs.PC += ll;
            cregs.PC += 2;
            break;
        }
        case 0x80: // BRA rel8
        {
            ResolveAddress(CPUAddressingMode::Rel8);
            signed char ll = C65Read(addL);
            cregs.PC += 2 + ll;
            break;
        }
        case 0xf0: // BEQ rel8
        {
            ResolveAddress(CPUAddressingMode::Rel8);
            signed char ll = C65Read(addL);
            if (flags.Z)
                cregs.PC += ll;
            cregs.PC += 2;

            break;
        }
        case 0x30: // BMI
        {
            ResolveAddress(CPUAddressingMode::Rel8);
            signed char ll = C65Read(addL);
            if (flags.N)
                cregs.PC += ll;
            cregs.PC += 2;
            break;
        }
        case 0x70: // BVS rel8
        {
            ResolveAddress(CPUAddressingMode::Rel8);
            signed char ll = C65Read(addL);
            if (flags.V)
                cregs.PC += ll;
            cregs.PC += 2;
            break;
        }
        case 0x50: // BVC rel8
        {
            ResolveAddress(CPUAddressingMode::Rel8);
            signed char ll = C65Read(addL);
            if (!flags.V)
                cregs.PC += ll;
            cregs.PC += 2;
            break;
        }
        case 0x82: // BRL rel16
        {
            ResolveAddress(CPUAddressingMode::Rel16);
            signed short ll = ReadWord();

            cregs.PC += ll;
            cregs.PC += 3;
            break;
        }

        case 0x4c: // JMP abs
        {
            ResolveAddress(CPUAddressingMode::Abs);
            cregs.PC = addL & 0x00FFFF;
            break;
        }

        case 0x5c: // JMP long
        {
            ResolveAddress(CPUAddressingMode::Long);
            cregs.PC = addL & 0x00FFFF;
            cregs.K = (addL & 0xFF0000) >> 16;
            break;
        }

        case 0x6c: // JMP (abs)
        {
            ResolveAddress(CPUAddressingMode::AbsPtr16);
            cregs.PC = addL & 0x00FFFF;
            break;
        }

        case 0x20: // JSR abs
        {
            ResolveAddress(CPUAddressingMode::Abs);

            // push PC+2 to stack
            PushWord(cregs.PC + 2);
            // No Flags
            cregs.PC = addL;
            break;
        }
        case 0xfc: // JSR (abs,x)
        {
            // This case also needs K in addressing the address
            ResolveAddress(CPUAddressingMode::AbsXPtr16);
            // push PC+2 to stack
            PushWord(cregs.PC + 2);
            // cout << "wow" << endl;
            // cin.get();
            //  No Flags
            cregs.PC = addL;

            break;
        }
        case 0x22: // JSL long
        {
            ResolveAddress(CPUAddressingMode::Long);
            Push(cregs.K);
            PushWord(cregs.PC + 3);
            cregs.PC = addL;
            cregs.K = addL >> 16;
            break;
        }
        case 0x7c: // JMP (abs,x)
        {
            // This case also needs K in addressing the address
            ResolveAddress(CPUAddressingMode::AbsXPtr16);
            cregs.PC = addL;
            // No Flags
            break;
        }
        case 0xdc: // JMP [abs]
        {
            ResolveAddress(CPUAddressingMode::AbsPtr24);
            cregs.PC = addL & 0x00FFFF;
            cregs.K = (addL & 0xFF0000) >> 16;
            break;
        }
        case 0x60: // RTS
        {
            cregs.PC = PopWord();
            cregs.PC += 1; // Intentional
            break;
        }
        case 0x6B: // RTL
        {
            cregs.PC = PopWord();
            cregs.PC += 1; // Intentional
            cregs.K = Pop();
            break;
        }
        case 0x40: // RTI
        {
            uint8 t = Pop();
            flags.N = t & flagNMask;
            flags.V = t & flagVMask;
            flags.M = t & flagMMask;
            flags.X = t & flagXMask;
            flags.D = t & flagDMask;
            flags.I = t & flagIMask;
            flags.Z = t & flagZMask;
            flags.C = t & flagCMask;
            cregs.PC = PopWord();
            if (!flagE)
                cregs.K = Pop();
            break;
        }

        case 0x44: // MVP
        {
            add24 srcB = C65Read(PCByteAhead(2)) << 16;

            cregs.DBR = C65Read(PCByteAhead(1)); // DBR = dest bank
            add24 dstB = cregs.DBR << 16;
            add24 src, dest;

            src = srcB + cregs.X;
            dest = dstB + cregs.Y;
            C65Write(dest, C65Read(src));
            UpdateX(cregs.X - 1);
            UpdateY(cregs.Y - 1);
            cregs.C -= 1;

            if (cregs.C == 0xFFFF)
            {

                cregs.PC += 3;
            }
            break;
        }
        case 0x54: // MVN
        {
            add24 srcB = C65Read(PCByteAhead(2)) << 16;
            cregs.DBR = C65Read(PCByteAhead(1)); // DBR = dest bank
            add24 dstB = cregs.DBR << 16;
            add24 src, dest;

            src = srcB + cregs.X;
            dest = dstB + cregs.Y;
            C65Write(dest, C65Read(src));
            UpdateX(cregs.X + 1);
            UpdateY(cregs.Y + 1);

            cregs.C -= 1;

            if (cregs.C == 0xFFFF)
            {
                cregs.PC += 3;
            }
            break;
        }
#pragma endregion
        }

        if (flagE)
        {
            cregs.S = (cregs.S & 0x00FF) | 0x0100;
            flags.X = 1;
            flags.M = 1;
            // cout << "emu is on for some reasons?!" << endl;
            // cin.get();
        }
        if (flags.X)
        {
            cregs.X &= 0x00FF;
            cregs.Y &= 0x00FF;
        }
    }
};