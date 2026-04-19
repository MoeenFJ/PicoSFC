#include <bits/stdc++.h>
using namespace std;

typedef unsigned char uint8;
typedef unsigned short uint16;

using SPC700ReadByteFunc_t = uint8 (*)(uint16);
using SPC700WriteByteFunc_t = void (*)(uint16, uint8);

class SPCFlagsRegister
{
    const uint8_t flagNMask = 0b10000000;
    const uint8_t flagVMask = 0b01000000;
    const uint8_t flagPMask = 0b00100000;
    const uint8_t flagBMask = 0b00010000;
    const uint8_t flagHMask = 0b00001000;
    const uint8_t flagIMask = 0b00000100;
    const uint8_t flagZMask = 0b00000010;
    const uint8_t flagCMask = 0b00000001;

public:
    bool N;
    bool V;
    bool P;
    bool B;
    bool H;
    bool I;
    bool Z;
    bool C;
    SPCFlagsRegister()
    {
        N = V = P = B = H = I = Z = C = 0;
    }

    void SetMask(uint8_t mask)
    {
        N = (mask & flagNMask) ? 1 : N;
        V = (mask & flagVMask) ? 1 : V;
        P = (mask & flagPMask) ? 1 : P;
        B = (mask & flagBMask) ? 1 : B;
        H = (mask & flagHMask) ? 1 : H;
        I = (mask & flagIMask) ? 1 : I;
        Z = (mask & flagZMask) ? 1 : Z;
        C = (mask & flagCMask) ? 1 : C;
    }
    void ResetMask(uint8_t mask)
    {
        // cout << "Reset Mask : " << std::bitset<8>(mask) << endl;
        N = (mask & flagNMask) ? 0 : N;
        V = (mask & flagVMask) ? 0 : V;
        P = (mask & flagPMask) ? 0 : P;
        B = (mask & flagBMask) ? 0 : B;
        H = (mask & flagHMask) ? 0 : H;
        I = (mask & flagIMask) ? 0 : I;
        Z = (mask & flagZMask) ? 0 : Z;
        C = (mask & flagCMask) ? 0 : C;
    }

    operator uint8_t() const
    {
        return (N ? flagNMask : 0) | (V ? flagVMask : 0) | (P ? flagPMask : 0) | (B ? flagBMask : 0) | (H ? flagHMask : 0) | (I ? flagIMask : 0) | (Z ? flagZMask : 0) | (C ? flagCMask : 0);
    }

    SPCFlagsRegister &operator=(const uint8_t &bits)
    {
        N = bits & flagNMask;
        V = bits & flagVMask;
        P = bits & flagPMask;
        B = bits & flagBMask;
        H = bits & flagHMask;
        I = bits & flagIMask;
        Z = bits & flagZMask;
        C = bits & flagCMask;
        return *this;
    }
};

// I really didn't want to go through audio, but every game needs it, so here we go.

enum class SPC700AddressingMode
{
    Imm = 0,

    Abs = 1,
    AbsX = 2,
    AbsY = 3,

    XPtr = 11,
    YPtr = 12,

    DP = 4,
    DPX = 5,
    DPY = 6,

    DPPtrY = 7,
    DPXPtr = 8,

    STK = 9,

    SrcDst = 10,

    MemBit = 13,
};
class SPC700
{

    SPC700ReadByteFunc_t ReadByte;
    SPC700WriteByteFunc_t WriteByte;

    uint16 addL;
    uint16 addH;
    void ResolveAddress(SPC700AddressingMode mode)
    {
        uint8 ll, hh;
        ll = ReadByte(PC + 1);
        hh = ReadByte(PC + 2);

        switch (mode)
        {
        case SPC700AddressingMode::Imm:
            addL = PC + 1;
            break;

        case SPC700AddressingMode::XPtr:
            addL = X | (flags.P ? 0x0100 : 0x0000);
            break;

        case SPC700AddressingMode::YPtr:
            addL = Y | (flags.P ? 0x0100 : 0x0000);
            break;

        case SPC700AddressingMode::DP:
            addL = ll | (flags.P ? 0x0100 : 0x0000);
            addH = ((ll + 1) & 0x00FF) | (flags.P ? 0x0100 : 0x0000);
            break;

        case SPC700AddressingMode::DPX:
            addL = ((uint8)(ll + X)) | (flags.P ? 0x0100 : 0x0000);
            addH = hh;
            break;
        case SPC700AddressingMode::DPY:
            addL = ((uint8)(ll + Y)) | (flags.P ? 0x0100 : 0x0000);
            break;

        case SPC700AddressingMode::Abs:
            addL = (hh << 8) | ll;
            break;

        case SPC700AddressingMode::AbsX:
            addL = ((hh << 8) | ll) + X;
            addH = ((hh << 8) | ll) + X + 1;
            break;

        case SPC700AddressingMode::AbsY:
            addL = ((hh << 8) | ll) + Y;
            break;

        case SPC700AddressingMode::DPXPtr:
            addL = ((uint8)(ll + X)) | (flags.P ? 0x0100 : 0x0000);
            addH = ((uint8)(ll + X + 1)) | (flags.P ? 0x0100 : 0x0000);
            addL = (ReadByte(addH) << 8) | ReadByte(addL);
            break;

        case SPC700AddressingMode::DPPtrY:
            addL = ((uint8)(ll)) | (flags.P ? 0x0100 : 0x0000);
            addH = ((uint8)(ll + 1)) | (flags.P ? 0x0100 : 0x0000);
            addL = (ReadByte(addH) << 8) | ReadByte(addL);
            addL += Y;
            break;

        case SPC700AddressingMode::SrcDst:
            addL = ll | (flags.P ? 0x0100 : 0x0000);
            addH = hh | (flags.P ? 0x0100 : 0x0000);
            break;

        case SPC700AddressingMode::MemBit:
            addL = (ll) | ((hh & 0x1F) << 8);
            addH = (hh & 0xE0) >> 5;
            break;

        default:
            break;
        }
    }

    uint8 doADC(uint8 A, uint8 B)
    {
        unsigned int t;

        int op1 = A & 0xFF;
        int op2 = B & 0xFF;
        t = op1 + op2 + flags.C;
        flags.V = (((op1 ^ t) & (op2 ^ t) & 0x80) != 0);
        flags.H = ((A & 0x0F) + (B & 0x0F) + flags.C) > 0x0F;
        flags.C = (t > 0xFF);
        flags.N = t & 0x0080;
        flags.Z = !(t & 0x00FF);

        return t;
    }

    uint8 doSBC(uint8 A, uint8 B)
    {
        unsigned int t;
        int op1 = A;
        int op2 = B;
        int binDiff = op1 - op2 - (flags.C ? 0 : 1);

        t = binDiff & 0xFF;
        flags.H = ((int)(op1 & 0x0F) - (int)(op2 & 0x0F) - (int)(flags.C ? 0 : 1)) >= 0;
        flags.V = (((op1 ^ op2) & (op1 ^ t) & 0x80) != 0);
        flags.C = (binDiff >= 0);

        flags.N = (t & 0x0080) != 0;
        flags.Z = (t & 0x00FF) == 0;

        return t;
    }

    uint8 doCMP(uint8 A, uint8 B)
    {
        unsigned int t;
        int op1 = A;
        int op2 = B;
        int binDiff = op1 - op2;

        t = binDiff & 0xFF;
        flags.C = (binDiff >= 0);
        flags.N = (t & 0x0080) != 0;
        flags.Z = (t & 0x00FF) == 0;

        return t;
    }

    void Push(uint8 data)
    {
        WriteByte(0x0100 | SP, data);
        SP--;
    }

    uint8 Pop()
    {
        SP++;
        return ReadByte(0x0100 | SP);
    }

public:
    uint8 A;
    uint8 X;
    uint8 Y;
    uint16 PC;
    uint8 SP;
    SPCFlagsRegister flags;

    uint16 cycles;

    SPC700(SPC700ReadByteFunc_t rf, SPC700WriteByteFunc_t wf)
    {
        this->ReadByte = rf;
        this->WriteByte = wf;

        Reset();
    }

    void PrintState()
    {
        // nvmxdizc
        cout << "--------------------------NEW INSTRUCTON---------------------------------------" << endl;
        cout << "-----------Flags----------" << endl;
        cout << "N V P B H I Z C" << endl;
        cout << flags.N << " " << flags.V << " " << flags.P << " " << flags.B << " " << flags.H << " " << flags.I << " " << flags.Z << " " << flags.C << endl;
        cout << "-----------Regs----------" << endl;
        // cout << "Stack top : " << hex << (unsigned int)ReadByte(cregs.S + 1) << " " << (unsigned int)ReadByte(cregs.S + 2) << endl;
        cout << "A : " << std::hex << (unsigned int)A << endl;
        cout << "SP : " << std::hex << (unsigned int)SP << endl;
        cout << "X : " << std::hex << (unsigned int)X << endl;
        cout << "Y : " << std::hex << (unsigned int)Y << endl;
        cout << "PC : " << std::hex << (unsigned int)PC << endl;
        uint8_t inst = ReadByte(PC);
        cout << "OpCode : " << std::hex << (int)inst << endl;
        cout << "3 Bytes Ahead : " << std::hex << (int)ReadByte(PC + 1) << " " << std::hex << (int)ReadByte(PC + 2) << " " << std::hex << (int)ReadByte(PC + 3) << " " << endl;
    }

    void Reset()
    {
        cycles = 0;
        A = X = Y = 0;
        flags = 0;
        PC = 0xFFC0;
        addL = 0;
    }

    void Step()
    {
        uint8 inst = ReadByte(PC);

        switch (inst)
        {
#pragma region DataTransfer_MemToReg
        case 0xE8: // MOV A,Imm
        {
            ResolveAddress(SPC700AddressingMode::Imm);
            uint8 imm = ReadByte(addL);
            A = imm;
            flags.N = A & 0x80;
            flags.Z = !(A);
            PC += 2;
            cycles += 2;
            break;
        }

        case 0xE6: // MOV A,(X)
        {
            ResolveAddress(SPC700AddressingMode::XPtr);
            uint8 d = ReadByte(addL);
            A = d;
            flags.N = A & 0x80;
            flags.Z = !(A);
            PC += 1;
            cycles += 3;
            break;
        }

        case 0xBF: // MOV A,(X)+
        {
            ResolveAddress(SPC700AddressingMode::XPtr);
            uint8 d = ReadByte(addL);
            A = d;
            flags.N = A & 0x80;
            flags.Z = !(A);
            X += 1;
            PC += 1;
            cycles += 4;
            break;
        }

        case 0xE4: // MOV A,(dp)
        {
            ResolveAddress(SPC700AddressingMode::DP);
            uint8 d = ReadByte(addL);
            A = d;
            flags.N = A & 0x80;
            flags.Z = !(A);
            PC += 2;
            cycles += 3;
            break;
        }

        case 0xF4: // MOV A,(dp+X)
        {
            ResolveAddress(SPC700AddressingMode::DPX);
            uint8 d = ReadByte(addL);
            A = d;
            flags.N = A & 0x80;
            flags.Z = !(A);
            PC += 2;
            cycles += 4;
            break;
        }

        case 0xE5: // MOV A,(abs)
        {
            ResolveAddress(SPC700AddressingMode::Abs);
            uint8 d = ReadByte(addL);
            A = d;
            flags.N = A & 0x80;
            flags.Z = !(A);
            PC += 3;
            cycles += 4;
            break;
        }

        case 0xF5: // MOV A,(abs+X)
        {
            ResolveAddress(SPC700AddressingMode::AbsX);
            uint8 d = ReadByte(addL);
            A = d;
            flags.N = A & 0x80;
            flags.Z = !(A);
            PC += 3;
            cycles += 5;
            break;
        }

        case 0xF6: // MOV A,(abs+X)
        {
            ResolveAddress(SPC700AddressingMode::AbsY);
            uint8 d = ReadByte(addL);
            A = d;
            flags.N = A & 0x80;
            flags.Z = !(A);
            PC += 3;
            cycles += 5;
            break;
        }

        case 0xE7: // MOV A,((dp+X))
        {
            ResolveAddress(SPC700AddressingMode::DPXPtr);
            uint8 d = ReadByte(addL);
            A = d;
            flags.N = A & 0x80;
            flags.Z = !(A);
            PC += 2;
            cycles += 6;
            break;
        }

        case 0xF7: // MOV A,((dp),y)
        {
            ResolveAddress(SPC700AddressingMode::DPPtrY);
            uint8 d = ReadByte(addL);
            A = d;
            flags.N = A & 0x80;
            flags.Z = !(A);
            PC += 2;
            cycles += 6;
            break;
        }

        case 0xCD: // MOV X,Imm
        {
            ResolveAddress(SPC700AddressingMode::Imm);
            uint8 d = ReadByte(addL);
            X = d;
            flags.N = X & 0x80;
            flags.Z = !(X);
            PC += 2;
            cycles += 2;
            break;
        }

        case 0xF8: // MOV X,(dp)
        {
            ResolveAddress(SPC700AddressingMode::DP);
            uint8 d = ReadByte(addL);
            X = d;
            flags.N = X & 0x80;
            flags.Z = !(X);
            PC += 2;
            cycles += 3;
            break;
        }

        case 0xF9: // MOV X,(dp+Y)
        {
            ResolveAddress(SPC700AddressingMode::DPY);
            uint8 d = ReadByte(addL);
            X = d;
            flags.N = X & 0x80;
            flags.Z = !(X);
            PC += 2;
            cycles += 4;
            break;
        }

        case 0xE9: // MOV X,(abs)
        {
            ResolveAddress(SPC700AddressingMode::Abs);
            uint8 d = ReadByte(addL);
            X = d;
            flags.N = X & 0x80;
            flags.Z = !(X);
            PC += 3;
            cycles += 4;
            break;
        }

        case 0x8D: // MOV Y,Imm
        {
            ResolveAddress(SPC700AddressingMode::Imm);
            uint8 d = ReadByte(addL);
            Y = d;
            flags.N = Y & 0x80;
            flags.Z = !(Y);
            PC += 2;
            cycles += 2;
            break;
        }

        case 0xEB: // MOV Y,(dp)
        {
            ResolveAddress(SPC700AddressingMode::DP);
            uint8 d = ReadByte(addL);
            Y = d;
            flags.N = Y & 0x80;
            flags.Z = !(Y);
            PC += 2;
            cycles += 3;
            break;
        }

        case 0xFB: // MOV Y,(dp+X)
        {
            ResolveAddress(SPC700AddressingMode::DPX);
            uint8 d = ReadByte(addL);
            Y = d;
            flags.N = Y & 0x80;
            flags.Z = !(Y);
            PC += 2;
            cycles += 4;
            break;
        }

        case 0xEC: // MOV Y,(abs)
        {
            ResolveAddress(SPC700AddressingMode::Abs);
            uint8 d = ReadByte(addL);
            Y = d;
            flags.N = Y & 0x80;
            flags.Z = !(Y);
            PC += 3;
            cycles += 4;
            break;
        }

#pragma endregion
#pragma region DataTransfer_RegToMem

        case 0xC6: // MOV (X),A
        {
            ResolveAddress(SPC700AddressingMode::XPtr);
            WriteByte(addL, A);
            PC += 1;
            cycles += 4;
            break;
        }

        case 0xAF: // MOV (X)+,A
        {
            ResolveAddress(SPC700AddressingMode::XPtr);
            WriteByte(addL, A);
            X += 1;
            PC += 1;
            cycles += 4;
            break;
        }

        case 0xC4: // MOV (dp),A
        {
            ResolveAddress(SPC700AddressingMode::DP);
            WriteByte(addL, A);
            PC += 2;
            cycles += 4;
            break;
        }

        case 0xD4: // MOV (dp+X),A
        {
            ResolveAddress(SPC700AddressingMode::DPX);
            WriteByte(addL, A);
            PC += 2;
            cycles += 5;
            break;
        }

        case 0xC5: // MOV (abs),A
        {
            ResolveAddress(SPC700AddressingMode::Abs);
            WriteByte(addL, A);
            PC += 3;
            cycles += 5;
            break;
        }

        case 0xD5: // MOV (abs+X),A
        {
            ResolveAddress(SPC700AddressingMode::AbsX);
            WriteByte(addL, A);
            PC += 3;
            cycles += 6;
            break;
        }

        case 0xD6: // MOV (abs+X),A
        {
            ResolveAddress(SPC700AddressingMode::AbsY);
            WriteByte(addL, A);
            PC += 3;
            cycles += 6;
            break;
        }

        case 0xC7: // MOV ((dp+X)),A
        {
            ResolveAddress(SPC700AddressingMode::DPXPtr);
            WriteByte(addL, A);
            PC += 2;
            cycles += 7;
            break;
        }

        case 0xD7: // MOV ((dp),y),A
        {
            ResolveAddress(SPC700AddressingMode::DPPtrY);
            WriteByte(addL, A);
            PC += 2;
            cycles += 7;
            break;
        }

        case 0xD8: // MOV (dp),X
        {
            ResolveAddress(SPC700AddressingMode::DP);
            WriteByte(addL, X);
            PC += 2;
            cycles += 4;
            break;
        }

        case 0xD9: // MOV (dp+Y),X
        {
            ResolveAddress(SPC700AddressingMode::DPY);
            WriteByte(addL, X);
            PC += 2;
            cycles += 5;
            break;
        }

        case 0xC9: // MOV (abs),X
        {
            ResolveAddress(SPC700AddressingMode::Abs);
            WriteByte(addL, X);
            PC += 3;
            cycles += 5;
            break;
        }

        case 0xCB: // MOV (dp),Y
        {
            ResolveAddress(SPC700AddressingMode::DP);
            WriteByte(addL, Y);
            PC += 2;
            cycles += 5;
            break;
        }

        case 0xDB: // MOV (dp+X),Y
        {
            ResolveAddress(SPC700AddressingMode::DPX);
            WriteByte(addL, Y);
            PC += 2;
            cycles += 5;
            break;
        }

        case 0xCC: // MOV Y,(abs)
        {
            ResolveAddress(SPC700AddressingMode::Abs);
            WriteByte(addL, Y);
            PC += 3;
            cycles += 5;
            break;
        }
#pragma endregion
#pragma region DataTransfer_RegToReg
        case 0x7D: // MOV A,X
        {
            A = X;
            flags.N = A & 0x80;
            flags.Z = !(A);
            PC += 1;
            cycles += 2;
            break;
        }
        case 0xDD: // MOV A,Y
        {
            A = Y;
            flags.N = A & 0x80;
            flags.Z = !(A);
            PC += 1;
            cycles += 2;
            break;
        }
        case 0x5D: // MOV X,A
        {
            X = A;
            flags.N = X & 0x80;
            flags.Z = !(X);
            PC += 1;
            cycles += 2;
            break;
        }
        case 0xFD: // MOV Y,A
        {
            Y = A;
            flags.N = Y & 0x80;
            flags.Z = !(Y);
            PC += 1;
            cycles += 2;
            break;
        }
        case 0x9D: // MOV X,SP
        {
            X = SP;
            flags.N = X & 0x80;
            flags.Z = !(X);
            PC += 1;
            cycles += 2;
            break;
        }
        case 0xBD: // MOV SP,X
        {
            SP = X;
            PC += 1;
            cycles += 2;
            break;
        }
#pragma endregion
#pragma region DataTransfer_MemToMem
        case 0xFA: // MOV dest,src
        {
            ResolveAddress(SPC700AddressingMode::SrcDst);
            WriteByte(addH, ReadByte(addL));
            PC += 3;
            cycles += 5;
            break;
        }
        case 0x8F: // MOV dp,imm
        {
            ResolveAddress(SPC700AddressingMode::SrcDst);
            WriteByte(addH, addL & 0x00FF);
            PC += 3;
            cycles += 5;
            break;
        }
#pragma endregion
#pragma region Arithmatics
        case 0x88: // ADC A,imm
        {
            ResolveAddress(SPC700AddressingMode::Imm);
            uint8 d = ReadByte(addL);
            A = doADC(A, d);
            PC += 2;
            cycles += 2;
            break;
        }
        case 0x86: // ADC A,(X)
        {
            ResolveAddress(SPC700AddressingMode::XPtr);
            uint8 d = ReadByte(addL);
            A = doADC(A, d);
            PC += 1;
            cycles += 3;
            break;
        }
        case 0x84: // ADC A,(dp)
        {
            ResolveAddress(SPC700AddressingMode::DP);
            uint8 d = ReadByte(addL);
            A = doADC(A, d);
            PC += 2;
            cycles += 3;
            break;
        }
        case 0x94: // ADC A,(dp+X)
        {
            ResolveAddress(SPC700AddressingMode::DPX);
            uint8 d = ReadByte(addL);
            A = doADC(A, d);
            PC += 2;
            cycles += 4;
            break;
        }
        case 0x85: // ADC A,(abs)
        {
            ResolveAddress(SPC700AddressingMode::Abs);
            uint8 d = ReadByte(addL);
            A = doADC(A, d);
            PC += 3;
            cycles += 4;
            break;
        }
        case 0x95: // ADC A,(abs+X)
        {
            ResolveAddress(SPC700AddressingMode::AbsX);
            uint8 d = ReadByte(addL);
            A = doADC(A, d);
            PC += 3;
            cycles += 5;
            break;
        }
        case 0x96: // ADC A,(abs+Y)
        {
            ResolveAddress(SPC700AddressingMode::AbsY);
            uint8 d = ReadByte(addL);
            A = doADC(A, d);
            PC += 3;
            cycles += 5;
            break;
        }

        case 0x87: // ADC A,((dp+x))
        {
            ResolveAddress(SPC700AddressingMode::DPXPtr);
            uint8 d = ReadByte(addL);
            A = doADC(A, d);
            PC += 2;
            cycles += 6;
            break;
        }
        case 0x97: // ADC A,((dp)+y)
        {
            ResolveAddress(SPC700AddressingMode::DPPtrY);
            uint8 d = ReadByte(addL);
            A = doADC(A, d);
            PC += 2;
            cycles += 6;
            break;
        }

        case 0x99: // ADC (x),(y)
        {

            ResolveAddress(SPC700AddressingMode::YPtr);
            uint8 y = ReadByte(addL);
            ResolveAddress(SPC700AddressingMode::XPtr);
            uint8 x = ReadByte(addL);
            WriteByte(addL, doADC(x, y));
            PC += 1;
            cycles += 5;
            break;
        }
        case 0x89: // ADC dp,dp
        {

            ResolveAddress(SPC700AddressingMode::SrcDst);
            uint8 x = ReadByte(addL);
            uint8 y = ReadByte(addH);
            WriteByte(addH, doADC(x, y));
            PC += 3;
            cycles += 6;
            break;
        }
        case 0x98: // ADC dp,imm
        {
            ResolveAddress(SPC700AddressingMode::SrcDst);
            uint8 x = addL;
            uint8 y = ReadByte(addH);
            WriteByte(addH, doADC(x, y));
            PC += 3;
            cycles += 5;
            break;
        }

        case 0xA8: // SBC A,imm
        {
            ResolveAddress(SPC700AddressingMode::Imm);
            uint8 d = ReadByte(addL);
            A = doSBC(A, d);
            PC += 2;
            cycles += 2;
            break;
        }
        case 0xA6: // SBC A,(X)
        {
            ResolveAddress(SPC700AddressingMode::XPtr);
            uint8 d = ReadByte(addL);
            A = doSBC(A, d);
            PC += 1;
            cycles += 3;
            break;
        }
        case 0xA4: // SBC A,(dp)
        {
            ResolveAddress(SPC700AddressingMode::DP);
            uint8 d = ReadByte(addL);
            A = doSBC(A, d);
            PC += 2;
            cycles += 3;
            break;
        }
        case 0xB4: // SBC A,(dp+X)
        {
            ResolveAddress(SPC700AddressingMode::DPX);
            uint8 d = ReadByte(addL);
            A = doSBC(A, d);
            PC += 2;
            cycles += 4;
            break;
        }
        case 0xA5: // SBC A,(abs)
        {
            ResolveAddress(SPC700AddressingMode::Abs);
            uint8 d = ReadByte(addL);
            A = doSBC(A, d);
            PC += 3;
            cycles += 4;
            break;
        }
        case 0xB5: // SBC A,(abs+X)
        {
            ResolveAddress(SPC700AddressingMode::AbsX);
            uint8 d = ReadByte(addL);
            A = doSBC(A, d);
            PC += 3;
            cycles += 5;
            break;
        }
        case 0xB6: // SBC A,(abs+Y)
        {
            ResolveAddress(SPC700AddressingMode::AbsY);
            uint8 d = ReadByte(addL);
            A = doSBC(A, d);
            PC += 3;
            cycles += 5;
            break;
        }

        case 0xA7: // SBC A,((dp+x))
        {
            ResolveAddress(SPC700AddressingMode::DPXPtr);
            uint8 d = ReadByte(addL);
            A = doSBC(A, d);
            PC += 2;
            cycles += 6;
            break;
        }
        case 0xB7: // SBC A,((dp)+y)
        {
            ResolveAddress(SPC700AddressingMode::DPPtrY);
            uint8 d = ReadByte(addL);
            A = doSBC(A, d);
            PC += 2;
            cycles += 6;
            break;
        }

        case 0xB9: // SBC (x),(y)
        {

            ResolveAddress(SPC700AddressingMode::YPtr);
            uint8 y = ReadByte(addL);
            ResolveAddress(SPC700AddressingMode::XPtr);
            uint8 x = ReadByte(addL);
            WriteByte(addL, doSBC(x, y));
            PC += 1;
            cycles += 5;
            break;
        }
        case 0xA9: // SBC dp,dp
        {

            ResolveAddress(SPC700AddressingMode::SrcDst);
            uint8 x = ReadByte(addH);
            uint8 y = ReadByte(addL);
            WriteByte(addH, doSBC(x, y));
            PC += 3;
            cycles += 6;
            break;
        }
        case 0xB8: // SBC dp,imm
        {
            ResolveAddress(SPC700AddressingMode::SrcDst);
            uint8 x = ReadByte(addH);
            uint8 y = addL;
            WriteByte(addH, doSBC(x, y));
            PC += 3;
            cycles += 5;
            break;
        }

        case 0x68: // CMP A,imm
        {
            ResolveAddress(SPC700AddressingMode::Imm);
            uint8 d = ReadByte(addL);
            doCMP(A, d);
            PC += 2;
            cycles += 2;
            break;
        }
        case 0x66: // CMP A,(X)
        {
            ResolveAddress(SPC700AddressingMode::XPtr);
            uint8 d = ReadByte(addL);
            doCMP(A, d);
            PC += 1;
            cycles += 3;
            break;
        }
        case 0x64: // CMP A,(dp)
        {
            ResolveAddress(SPC700AddressingMode::DP);
            uint8 d = ReadByte(addL);
            doCMP(A, d);
            PC += 2;
            cycles += 3;
            break;
        }
        case 0x74: // CMP A,(dp+X)
        {
            ResolveAddress(SPC700AddressingMode::DPX);
            uint8 d = ReadByte(addL);
            doCMP(A, d);
            PC += 2;
            cycles += 4;
            break;
        }
        case 0x65: // CMP A,(abs)
        {
            ResolveAddress(SPC700AddressingMode::Abs);
            uint8 d = ReadByte(addL);
            doCMP(A, d);
            PC += 3;
            cycles += 4;
            break;
        }
        case 0x75: // CMP A,(abs+X)
        {
            ResolveAddress(SPC700AddressingMode::AbsX);
            uint8 d = ReadByte(addL);
            doCMP(A, d);
            PC += 3;
            cycles += 5;
            break;
        }
        case 0x76: // CMP A,(abs+Y)
        {
            ResolveAddress(SPC700AddressingMode::AbsY);
            uint8 d = ReadByte(addL);
            doCMP(A, d);
            PC += 3;
            cycles += 5;
            break;
        }

        case 0x67: // CMP A,((dp+x))
        {
            ResolveAddress(SPC700AddressingMode::DPXPtr);
            uint8 d = ReadByte(addL);
            doCMP(A, d);
            PC += 2;
            cycles += 6;
            break;
        }
        case 0x77: // CMP A,((dp)+y)
        {
            ResolveAddress(SPC700AddressingMode::DPPtrY);
            uint8 d = ReadByte(addL);
            doCMP(A, d);
            PC += 2;
            cycles += 6;
            break;
        }

        case 0x79: // CMP (x),(y)
        {

            ResolveAddress(SPC700AddressingMode::YPtr);
            uint8 y = ReadByte(addL);
            ResolveAddress(SPC700AddressingMode::XPtr);
            uint8 x = ReadByte(addL);
            doCMP(x, y);
            PC += 1;
            cycles += 5;
            break;
        }
        case 0x69: // CMP dp,dp
        {

            ResolveAddress(SPC700AddressingMode::SrcDst);
            uint8 x = ReadByte(addH);
            uint8 y = ReadByte(addL);
            doCMP(x, y);
            PC += 3;
            cycles += 6;
            break;
        }
        case 0x78: // CMP dp,imm
        {
            ResolveAddress(SPC700AddressingMode::SrcDst);
            uint8 x = ReadByte(addH);
            uint8 y = addL;
            doCMP(x, y);
            PC += 3;
            cycles += 5;
            break;
        }

        case 0xC8: // CMP X,imm
        {
            ResolveAddress(SPC700AddressingMode::Imm);
            uint8 d = ReadByte(addL);
            doCMP(X, d);
            PC += 2;
            cycles += 2;
            break;
        }
        case 0x3E: // CMP X,(dp)
        {
            ResolveAddress(SPC700AddressingMode::DP);
            uint8 d = ReadByte(addL);
            doCMP(X, d);
            PC += 2;
            cycles += 3;
            break;
        }
        case 0x1E: // CMP X,(abs)
        {
            ResolveAddress(SPC700AddressingMode::Abs);
            uint8 d = ReadByte(addL);
            doCMP(X, d);
            PC += 3;
            cycles += 4;
            break;
        }

        case 0xAD: // CMP Y,imm
        {
            ResolveAddress(SPC700AddressingMode::Imm);
            uint8 d = ReadByte(addL);
            doCMP(Y, d);
            PC += 2;
            cycles += 2;
            break;
        }
        case 0x7E: // CMP Y,(dp)
        {
            ResolveAddress(SPC700AddressingMode::DP);
            uint8 d = ReadByte(addL);
            doCMP(Y, d);
            PC += 2;
            cycles += 3;
            break;
        }
        case 0x5E: // CMP Y,(abs)
        {
            ResolveAddress(SPC700AddressingMode::Abs);
            uint8 d = ReadByte(addL);
            doCMP(Y, d);
            PC += 3;
            cycles += 4;
            break;
        }
#pragma endregion
#pragma region LogicOperations
        case 0x28: // AND A,imm
        {
            ResolveAddress(SPC700AddressingMode::Imm);
            uint8 d = ReadByte(addL);
            A = A & d;
            flags.Z = !(A);
            flags.N = A & 0x80;
            PC += 2;
            cycles += 2;
            break;
        }
        case 0x26: // AND A,(X)
        {
            ResolveAddress(SPC700AddressingMode::XPtr);
            uint8 d = ReadByte(addL);
            A = A & d;
            flags.Z = !(A);
            flags.N = A & 0x80;
            PC += 1;
            cycles += 3;
            break;
        }
        case 0x24: // AND A,(dp)
        {
            ResolveAddress(SPC700AddressingMode::DP);
            uint8 d = ReadByte(addL);
            A = A & d;
            flags.Z = !(A);
            flags.N = A & 0x80;
            PC += 2;
            cycles += 3;
            break;
        }
        case 0x34: // AND A,(dp+X)
        {
            ResolveAddress(SPC700AddressingMode::DPX);
            uint8 d = ReadByte(addL);
            A = A & d;
            flags.Z = !(A);
            flags.N = A & 0x80;
            PC += 2;
            cycles += 4;
            break;
        }
        case 0x25: // AND A,(abs)
        {
            ResolveAddress(SPC700AddressingMode::Abs);
            uint8 d = ReadByte(addL);
            A = A & d;
            flags.Z = !(A);
            flags.N = A & 0x80;
            PC += 3;
            cycles += 4;
            break;
        }
        case 0x35: // AND A,(abs+X)
        {
            ResolveAddress(SPC700AddressingMode::AbsX);
            uint8 d = ReadByte(addL);
            A = A & d;
            flags.Z = !(A);
            flags.N = A & 0x80;
            PC += 3;
            cycles += 5;
            break;
        }
        case 0x36: // AND A,(abs+Y)
        {
            ResolveAddress(SPC700AddressingMode::AbsY);
            uint8 d = ReadByte(addL);
            A = A & d;
            flags.Z = !(A);
            flags.N = A & 0x80;
            PC += 3;
            cycles += 5;
            break;
        }

        case 0x27: // AND A,((dp+x))
        {
            ResolveAddress(SPC700AddressingMode::DPXPtr);
            uint8 d = ReadByte(addL);
            A = A & d;
            flags.Z = !(A);
            flags.N = A & 0x80;
            PC += 2;
            cycles += 6;
            break;
        }
        case 0x37: // AND A,((dp)+y)
        {
            ResolveAddress(SPC700AddressingMode::DPPtrY);
            uint8 d = ReadByte(addL);
            A = A & d;
            flags.Z = !(A);
            flags.N = A & 0x80;
            PC += 2;
            cycles += 6;
            break;
        }

        case 0x39: // AND (x),(y)
        {

            ResolveAddress(SPC700AddressingMode::YPtr);
            uint8 y = ReadByte(addL);
            ResolveAddress(SPC700AddressingMode::XPtr);
            uint8 x = ReadByte(addL);
            uint8 t = x & y;
            flags.Z = !(t);
            flags.N = t & 0x80;
            WriteByte(addL, t);
            PC += 1;
            cycles += 5;
            break;
        }
        case 0x29: // AND dp,dp
        {

            ResolveAddress(SPC700AddressingMode::SrcDst);
            uint8 x = ReadByte(addL);
            uint8 y = ReadByte(addH);
            uint8 t = x & y;
            flags.Z = !(t);
            flags.N = t & 0x80;
            WriteByte(addH, t);
            PC += 3;
            cycles += 6;
            break;
        }
        case 0x38: // AND dp,imm
        {
            ResolveAddress(SPC700AddressingMode::SrcDst);
            uint8 x = addL;
            uint8 y = ReadByte(addH);
            uint8 t = x & y;
            flags.Z = !(t);
            flags.N = t & 0x80;
            WriteByte(addH, t);
            PC += 3;
            cycles += 5;
            break;
        }

        case 0x08: // OR A,imm
        {
            ResolveAddress(SPC700AddressingMode::Imm);
            uint8 d = ReadByte(addL);
            A = A | d;
            flags.Z = !(A);
            flags.N = A & 0x80;
            PC += 2;
            cycles += 2;
            break;
        }
        case 0x06: // OR A,(X)
        {
            ResolveAddress(SPC700AddressingMode::XPtr);
            uint8 d = ReadByte(addL);
            A = A | d;
            flags.Z = !(A);
            flags.N = A & 0x80;
            PC += 1;
            cycles += 3;
            break;
        }
        case 0x04: // OR A,(dp)
        {
            ResolveAddress(SPC700AddressingMode::DP);
            uint8 d = ReadByte(addL);
            A = A | d;
            flags.Z = !(A);
            flags.N = A & 0x80;
            PC += 2;
            cycles += 3;
            break;
        }
        case 0x14: // OR A,(dp+X)
        {
            ResolveAddress(SPC700AddressingMode::DPX);
            uint8 d = ReadByte(addL);
            A = A | d;
            flags.Z = !(A);
            flags.N = A & 0x80;
            PC += 2;
            cycles += 4;
            break;
        }
        case 0x05: // OR A,(abs)
        {
            ResolveAddress(SPC700AddressingMode::Abs);
            uint8 d = ReadByte(addL);
            A = A | d;
            flags.Z = !(A);
            flags.N = A & 0x80;
            PC += 3;
            cycles += 4;
            break;
        }
        case 0x15: // OR A,(abs+X)
        {
            ResolveAddress(SPC700AddressingMode::AbsX);
            uint8 d = ReadByte(addL);
            A = A | d;
            flags.Z = !(A);
            flags.N = A & 0x80;
            PC += 3;
            cycles += 5;
            break;
        }
        case 0x16: // OR A,(abs+Y)
        {
            ResolveAddress(SPC700AddressingMode::AbsY);
            uint8 d = ReadByte(addL);
            A = A | d;
            flags.Z = !(A);
            flags.N = A & 0x80;
            PC += 3;
            cycles += 5;
            break;
        }

        case 0x07: // OR A,((dp+x))
        {
            ResolveAddress(SPC700AddressingMode::DPXPtr);
            uint8 d = ReadByte(addL);
            A = A | d;
            flags.Z = !(A);
            flags.N = A & 0x80;
            PC += 2;
            cycles += 6;
            break;
        }
        case 0x17: // OR A,((dp)+y)
        {
            ResolveAddress(SPC700AddressingMode::DPPtrY);
            uint8 d = ReadByte(addL);
            A = A | d;
            flags.Z = !(A);
            flags.N = A & 0x80;
            PC += 2;
            cycles += 6;
            break;
        }

        case 0x19: // OR (x),(y)
        {

            ResolveAddress(SPC700AddressingMode::YPtr);
            uint8 y = ReadByte(addL);
            ResolveAddress(SPC700AddressingMode::XPtr);
            uint8 x = ReadByte(addL);
            uint8 t = x | y;
            flags.Z = !(t);
            flags.N = t & 0x80;
            WriteByte(addL, t);
            PC += 1;
            cycles += 5;
            break;
        }
        case 0x09: // OR dp,dp
        {

            ResolveAddress(SPC700AddressingMode::SrcDst);
            uint8 x = ReadByte(addL);
            uint8 y = ReadByte(addH);
            uint8 t = x | y;
            flags.Z = !(t);
            flags.N = t & 0x80;
            WriteByte(addH, t);
            PC += 3;
            cycles += 6;
            break;
        }
        case 0x18: // OR dp,imm
        {
            ResolveAddress(SPC700AddressingMode::SrcDst);
            uint8 x = addL;
            uint8 y = ReadByte(addH);
            uint8 t = x | y;
            flags.Z = !(t);
            flags.N = t & 0x80;
            WriteByte(addH, t);
            PC += 3;
            cycles += 5;
            break;
        }

        case 0x48: // EOR A,imm
        {
            ResolveAddress(SPC700AddressingMode::Imm);
            uint8 d = ReadByte(addL);
            A = A ^ d;
            flags.Z = !(A);
            flags.N = A & 0x80;
            PC += 2;
            cycles += 2;
            break;
        }
        case 0x46: // EOR A,(X)
        {
            ResolveAddress(SPC700AddressingMode::XPtr);
            uint8 d = ReadByte(addL);
            A = A ^ d;
            flags.Z = !(A);
            flags.N = A & 0x80;
            PC += 1;
            cycles += 3;
            break;
        }
        case 0x44: // EOR A,(dp)
        {
            ResolveAddress(SPC700AddressingMode::DP);
            uint8 d = ReadByte(addL);
            A = A ^ d;
            flags.Z = !(A);
            flags.N = A & 0x80;
            PC += 2;
            cycles += 3;
            break;
        }
        case 0x54: // EOR A,(dp+X)
        {
            ResolveAddress(SPC700AddressingMode::DPX);
            uint8 d = ReadByte(addL);
            A = A ^ d;
            flags.Z = !(A);
            flags.N = A & 0x80;
            PC += 2;
            cycles += 4;
            break;
        }
        case 0x45: // EOR A,(abs)
        {
            ResolveAddress(SPC700AddressingMode::Abs);
            uint8 d = ReadByte(addL);
            A = A ^ d;
            flags.Z = !(A);
            flags.N = A & 0x80;
            PC += 3;
            cycles += 4;
            break;
        }
        case 0x55: // EOR A,(abs+X)
        {
            ResolveAddress(SPC700AddressingMode::AbsX);
            uint8 d = ReadByte(addL);
            A = A ^ d;
            flags.Z = !(A);
            flags.N = A & 0x80;
            PC += 3;
            cycles += 5;
            break;
        }
        case 0x56: // EOR A,(abs+Y)
        {
            ResolveAddress(SPC700AddressingMode::AbsY);
            uint8 d = ReadByte(addL);
            A = A ^ d;
            flags.Z = !(A);
            flags.N = A & 0x80;
            PC += 3;
            cycles += 5;
            break;
        }

        case 0x47: // EOR A,((dp+x))
        {
            ResolveAddress(SPC700AddressingMode::DPXPtr);
            uint8 d = ReadByte(addL);
            A = A ^ d;
            flags.Z = !(A);
            flags.N = A & 0x80;
            PC += 2;
            cycles += 6;
            break;
        }
        case 0x57: // EOR A,((dp)+y)
        {
            ResolveAddress(SPC700AddressingMode::DPPtrY);
            uint8 d = ReadByte(addL);
            A = A ^ d;
            flags.Z = !(A);
            flags.N = A & 0x80;
            PC += 2;
            cycles += 6;
            break;
        }

        case 0x59: // EOR (x),(y)
        {

            ResolveAddress(SPC700AddressingMode::YPtr);
            uint8 y = ReadByte(addL);
            ResolveAddress(SPC700AddressingMode::XPtr);
            uint8 x = ReadByte(addL);
            uint8 t = x ^ y;
            flags.Z = !(t);
            flags.N = t & 0x80;
            WriteByte(addL, t);
            PC += 1;
            cycles += 5;
            break;
        }
        case 0x49: // EOR dp,dp
        {

            ResolveAddress(SPC700AddressingMode::SrcDst);
            uint8 x = ReadByte(addL);
            uint8 y = ReadByte(addH);
            uint8 t = x ^ y;
            flags.Z = !(t);
            flags.N = t & 0x80;
            WriteByte(addH, t);
            PC += 3;
            cycles += 6;
            break;
        }
        case 0x58: // EOR dp,imm
        {
            ResolveAddress(SPC700AddressingMode::SrcDst);
            uint8 x = addL;
            uint8 y = ReadByte(addH);
            uint8 t = x ^ y;
            flags.Z = !(t);
            flags.N = t & 0x80;
            WriteByte(addH, t);
            PC += 3;
            cycles += 5;
            break;
        }
#pragma endregion
#pragma region IncDec
        case 0xBC: // INC A
        {
            A++;
            flags.Z = !(A);
            flags.N = A & 0x80;
            PC += 1;
            cycles += 2;
            break;
        }

        case 0xAB: // INC (dp)
        {
            ResolveAddress(SPC700AddressingMode::DP);
            uint8 t = ReadByte(addL);
            t++;
            flags.Z = !(t);
            flags.N = t & 0x80;
            WriteByte(addL, t);
            PC += 2;
            cycles += 4;
            break;
        }

        case 0xBB: // INC (dp+x)
        {
            ResolveAddress(SPC700AddressingMode::DPX);
            uint8 t = ReadByte(addL);
            t++;
            flags.Z = !(t);
            flags.N = t & 0x80;
            WriteByte(addL, t);
            PC += 2;
            cycles += 5;
            break;
        }

        case 0xAC: // INC (abs)
        {
            ResolveAddress(SPC700AddressingMode::Abs);
            uint8 t = ReadByte(addL);
            t++;
            flags.Z = !(t);
            flags.N = t & 0x80;
            WriteByte(addL, t);
            PC += 3;
            cycles += 5;
            break;
        }
        case 0x3D: // INC X
        {
            X++;
            flags.Z = !(X);
            flags.N = X & 0x80;
            PC += 1;
            cycles += 2;
            break;
        }
        case 0xFC: // INC Y
        {
            Y++;
            flags.Z = !(Y);
            flags.N = Y & 0x80;
            PC += 1;
            cycles += 2;
            break;
        }

        case 0x9C: // DEC A
        {
            A--;
            flags.Z = !(A);
            flags.N = A & 0x80;
            PC += 1;
            cycles += 2;
            break;
        }

        case 0x8B: // DEC (dp)
        {
            ResolveAddress(SPC700AddressingMode::DP);
            uint8 t = ReadByte(addL);
            t--;
            flags.Z = !(t);
            flags.N = t & 0x80;
            WriteByte(addL, t);
            PC += 2;
            cycles += 4;
            break;
        }

        case 0x9B: // DEC (dp+x)
        {
            ResolveAddress(SPC700AddressingMode::DPX);
            uint8 t = ReadByte(addL);
            t--;
            flags.Z = !(t);
            flags.N = t & 0x80;
            WriteByte(addL, t);
            PC += 2;
            cycles += 5;
            break;
        }

        case 0x8C: // DEC (abs)
        {
            ResolveAddress(SPC700AddressingMode::Abs);
            uint8 t = ReadByte(addL);
            t--;
            flags.Z = !(t);
            flags.N = t & 0x80;
            WriteByte(addL, t);
            PC += 3;
            cycles += 5;
            break;
        }
        case 0x1D: // DEC X
        {
            X--;
            flags.Z = !(X);
            flags.N = X & 0x80;
            PC += 1;
            cycles += 2;
            break;
        }
        case 0xDC: // DEC Y
        {
            Y--;
            flags.Z = !(Y);
            flags.N = Y & 0x80;
            PC += 1;
            cycles += 2;
            break;
        }
#pragma endregion

#pragma region ShiftRotate
        case 0x1C: // ASL A
        {
            flags.C = A & 0x80;
            A = A << 1;
            flags.Z = !(A);
            flags.N = A & 0x80;
            PC += 1;
            cycles += 2;
            break;
        }
        case 0x0B: // ASL (dp)
        {
            ResolveAddress(SPC700AddressingMode::DP);
            uint8 t = ReadByte(addL);
            flags.C = t & 0x80;
            t = t << 1;
            flags.Z = !(t);
            flags.N = t & 0x80;
            WriteByte(addL, t);
            PC += 2;
            cycles += 4;
            break;
        }
        case 0x1B: // ASL (dp+x)
        {
            ResolveAddress(SPC700AddressingMode::DPX);
            uint8 t = ReadByte(addL);
            flags.C = t & 0x80;
            t = t << 1;
            flags.Z = !(t);
            flags.N = t & 0x80;
            WriteByte(addL, t);
            PC += 2;
            cycles += 5;
            break;
        }

        case 0x0C: // ASL (abs)
        {
            ResolveAddress(SPC700AddressingMode::Abs);
            uint8 t = ReadByte(addL);
            flags.C = t & 0x80;
            t = t << 1;
            flags.Z = !(t);
            flags.N = t & 0x80;
            WriteByte(addL, t);
            PC += 3;
            cycles += 5;
            break;
        }

        case 0x5C: // LSR A
        {
            flags.C = A & 0x01;
            A = A >> 1;

            flags.Z = !(A);
            flags.N = A & 0x80;
            PC += 1;
            cycles += 2;
            break;
        }
        case 0x4B: // LSR (dp)
        {
            ResolveAddress(SPC700AddressingMode::DP);
            uint8 t = ReadByte(addL);
            flags.C = t & 0x01;
            t = t >> 1;

            flags.Z = !(t);
            flags.N = t & 0x80;
            WriteByte(addL, t);
            PC += 2;
            cycles += 4;
            break;
        }
        case 0x5B: // LSR (dp+x)
        {
            ResolveAddress(SPC700AddressingMode::DPX);
            uint8 t = ReadByte(addL);
            flags.C = t & 0x01;
            t = t >> 1;

            flags.Z = !(t);
            flags.N = t & 0x80;
            WriteByte(addL, t);
            PC += 2;
            cycles += 5;
            break;
        }

        case 0x4C: // LSR (abs)
        {
            ResolveAddress(SPC700AddressingMode::Abs);
            uint8 t = ReadByte(addL);
            flags.C = t & 0x01;
            t = t >> 1;

            flags.Z = !(t);
            flags.N = t & 0x80;
            WriteByte(addL, t);
            PC += 3;
            cycles += 5;
            break;
        }

        case 0x3C: // ROL A
        {
            bool c = flags.C;
            flags.C = A & 0x80;
            A = A << 1;
            A |= c ? 0x01 : 0x00;
            flags.Z = !(A);
            flags.N = A & 0x80;
            PC += 1;
            cycles += 2;
            break;
        }
        case 0x2B: // ROL (dp)
        {
            ResolveAddress(SPC700AddressingMode::DP);
            uint8 t = ReadByte(addL);
            bool c = flags.C;
            flags.C = t & 0x80;
            t = t << 1;
            t |= c ? 0x01 : 0x00;
            flags.Z = !(t);
            flags.N = t & 0x80;
            WriteByte(addL, t);
            PC += 2;
            cycles += 4;
            break;
        }
        case 0x3B: // ROL (dp+x)
        {
            ResolveAddress(SPC700AddressingMode::DPX);
            uint8 t = ReadByte(addL);
            bool c = flags.C;
            flags.C = t & 0x80;
            t = t << 1;
            t |= c ? 0x01 : 0x00;
            flags.Z = !(t);
            flags.N = t & 0x80;
            WriteByte(addL, t);
            PC += 2;
            cycles += 5;
            break;
        }

        case 0x2C: // ROL (abs)
        {
            ResolveAddress(SPC700AddressingMode::Abs);
            uint8 t = ReadByte(addL);
            bool c = flags.C;
            flags.C = t & 0x80;
            t = t << 1;
            t |= c ? 0x01 : 0x00;
            flags.Z = !(t);
            flags.N = t & 0x80;
            WriteByte(addL, t);
            PC += 3;
            cycles += 5;
            break;
        }

        case 0x7C: // ROR A
        {
            bool c = flags.C;
            flags.C = A & 0x01;
            A = A >> 1;
            A |= c ? 0x80 : 0x00;
            flags.Z = !(A);
            flags.N = A & 0x80;
            PC += 1;
            cycles += 2;
            break;
        }
        case 0x6B: // ROR (dp)
        {
            ResolveAddress(SPC700AddressingMode::DP);
            uint8 t = ReadByte(addL);
            bool c = flags.C;
            flags.C = t & 0x01;
            t = t >> 1;
            t |= c ? 0x80 : 0x00;
            flags.Z = !(t);
            flags.N = t & 0x80;
            WriteByte(addL, t);
            PC += 2;
            cycles += 4;
            break;
        }
        case 0x7B: // ROR (dp+x)
        {
            ResolveAddress(SPC700AddressingMode::DPX);
            uint8 t = ReadByte(addL);
            bool c = flags.C;
            flags.C = t & 0x01;
            t = t >> 1;
            t |= c ? 0x80 : 0x00;
            flags.Z = !(t);
            flags.N = t & 0x80;
            WriteByte(addL, t);
            PC += 2;
            cycles += 5;
            break;
        }

        case 0x6C: // ROR (abs)
        {
            ResolveAddress(SPC700AddressingMode::Abs);
            uint8 t = ReadByte(addL);
            bool c = flags.C;
            flags.C = t & 0x01;
            t = t >> 1;
            t |= c ? 0x80 : 0x00;
            flags.Z = !(t);
            flags.N = t & 0x80;
            WriteByte(addL, t);
            PC += 3;
            cycles += 5;
            break;
        }

        case 0x9F: // XCN A
        {
            A = (A >> 4) | (A << 4);
            flags.Z = !(A);
            flags.N = A & 0x80;
            PC += 1;
            cycles += 5;
            break;
        }
#pragma endregion

#pragma region 16BitOps
        case 0xBA: // MOVW YA,(dp)
        {
            ResolveAddress(SPC700AddressingMode::DP);
            A = ReadByte(addL);
            Y = ReadByte(addH);
            flags.Z = !(A | Y);
            flags.N = Y & 0x80;
            PC += 2;
            cycles += 5;
            break;
        }
        case 0xDA: // MOVW (dp),YA
        {
            ResolveAddress(SPC700AddressingMode::DP);
            WriteByte(addL, A);
            WriteByte(addH, Y);

            PC += 2;
            cycles += 4;
            break;
        }
        case 0x3A: // INCW (dp)
        {
            ResolveAddress(SPC700AddressingMode::DP);
            uint16 t = ReadByte(addL) | (ReadByte(addH) << 8);
            t++;
            WriteByte(addL, t & 0x00FF);
            WriteByte(addH, t >> 8);
            flags.Z = !t;
            flags.N = t & 0x8000;
            PC += 2;
            cycles += 6;
            break;
        }
        case 0x1A: // DECW (dp)
        {
            ResolveAddress(SPC700AddressingMode::DP);
            uint16 t = ReadByte(addL) | (ReadByte(addH) << 8);
            t--;
            WriteByte(addL, t & 0x00FF);
            WriteByte(addH, t >> 8);
            flags.Z = !t;
            flags.N = t & 0x8000;
            PC += 2;
            cycles += 6;
            break;
        }
        case 0x7A: // ADDW YA,(dp)
        {
            ResolveAddress(SPC700AddressingMode::DP);
            uint16 op1 = ReadByte(addL) | (ReadByte(addH) << 8);
            uint16 op2 = ((Y << 8) | A);
            uint32_t add = op1 + op2;
            flags.Z = !(add & 0x00FFFF);
            flags.N = add & 0x8000;

            flags.V = (((op1 ^ add) & (op2 ^ add) & 0x8000) != 0);
            flags.H = ((op1 & 0x0FFF) + (op2 & 0x0FFF)) > 0x0FFF;
            flags.C = (add > 0xFFFF);

            A = add & 0x00FF;
            Y = (add & 0xFF00) >> 8;

            PC += 2;
            cycles += 5;
            break;
        }

        case 0x9A: // SUBW YA,(dp)
        {
            ResolveAddress(SPC700AddressingMode::DP);
            int op2 = ReadByte(addL) | (ReadByte(addH) << 8);
            int op1 = ((Y << 8) | A);
            int sub = op1 - op2;
            flags.Z = !(sub & 0x00FFFF);
            flags.N = sub & 0x8000;

            flags.V = (((op1 ^ op2) & (op1 ^ sub) & 0x8000) != 0);
            flags.H = ((op1 & 0x0FFF) - (op2 & 0x0FFF)) >= 0;
            flags.C = (sub >= 0);

            A = sub & 0x00FF;
            Y = (sub & 0xFF00) >> 8;

            PC += 2;
            cycles += 5;
            break;
        }

        case 0x5A: // CMPW YA,(dp)
        {
            ResolveAddress(SPC700AddressingMode::DP);
            int op2 = ReadByte(addL) | (ReadByte(addH) << 8);
            int op1 = ((Y << 8) | A);
            int sub = op1 - op2;
            flags.Z = !(sub & 0x00FFFF);
            flags.N = sub & 0x8000;

            flags.C = (sub >= 0);

            PC += 2;
            cycles += 4;
            break;
        }
#pragma endregion

#pragma region MulDiv
        case 0xCF: // MUL YA
        {
            uint16 mul = A * Y;
            A = mul & 0x00FF;
            Y = mul >> 8;
            // Yes, only Y is checked for Z and N
            flags.Z = !(Y);
            flags.N = Y & 0x80;
            PC += 1;
            cycles += 9;
            break;
        }
        case 0x9E: // DIV YA,X
        {
            flags.H = (X & 0x0F) <= (Y & 0x0F);
            uint32_t D = (A | (Y << 8));
            for (int i = 1; i < 10; i++)
            {
                D = D << 1;
                if (D & 0x20000)
                    D ^= 0x20001;
                if (D >= (X * 0x200))
                    D ^= 0x1;
                if (D & 0x1)
                    D = (D - (X * 0x200)) & 0x1FFFF;
            }

            A = D & 0xFF;
            Y = D / 0x200;

            flags.Z = !(A);
            flags.N = A & 0x80;

            flags.V = D & 0x100;

            PC += 1;
            cycles += 12;
            break;
        }
#pragma endregion
#pragma region Misc
        case 0xDF: // DAA A
        {

            uint16_t t = A;
            if ((t & 0x0F) > 0x09 || flags.H)
            {
                t += 0x06;
            }

            if (t > 0x9F || flags.C)
            {
                t += 0x60;
                flags.C = 1;
            }

            A = t & 0xFF;

            flags.Z = !(A);
            flags.N = A & 0x80;
            PC += 1;
            cycles += 3;
            break;
        }
        case 0xBE: // DAS A
        {
            bool sub = (A > 0x99) || !flags.C;
            uint16_t t = A;

            if ((t & 0x0F) > 0x09 || !flags.H)
            {
                t -= 0x06;
            }

            if (sub)
            {
                t -= 0x60;
                flags.C = 0;
            }

            A = t & 0xFF;
            flags.Z = !(A);
            flags.N = A & 0x80;
            PC += 1;
            cycles += 3;
            break;
        }
        case 0x00: // NOP
        {
            PC += 1;
            cycles += 2;
            break;
        }
        case 0xef: // SLEEP
        {
            PC += 1;
            cycles += 3;
            break;
        }
        case 0xff: // STOP
        {
            PC += 1;
            cycles += 3;
            break;
        }

#pragma endregion
#pragma region Branch
        case 0x2F: // BRA rel
        {
            ResolveAddress(SPC700AddressingMode::Imm);
            int8_t t = ReadByte(addL);
            PC = (PC + 2) + t;
            cycles += 4;
            break;
        }

        case 0xF0: // BEQ rel
        {
            ResolveAddress(SPC700AddressingMode::Imm);
            int8_t t = ReadByte(addL);
            if (flags.Z)
            {
                PC += t;
                cycles += 2;
            }
            PC += 2;
            cycles += 2;
            break;
        }

        case 0xD0: // BNE rel
        {
            ResolveAddress(SPC700AddressingMode::Imm);
            int8_t t = ReadByte(addL);
            if (!flags.Z)
            {
                PC += t;
                cycles += 2;
            }
            PC += 2;
            cycles += 2;
            break;
        }

        case 0xB0: // BCS rel
        {
            ResolveAddress(SPC700AddressingMode::Imm);
            int8_t t = ReadByte(addL);
            if (flags.C)
            {
                PC += t;
                cycles += 2;
            }
            PC += 2;
            cycles += 2;
            break;
        }
        case 0x90: // BCC rel
        {
            ResolveAddress(SPC700AddressingMode::Imm);
            int8_t t = ReadByte(addL);
            if (!flags.C)
            {
                PC += t;
                cycles += 2;
            }
            PC += 2;
            cycles += 2;
            break;
        }
        case 0x70: // BVS rel
        {
            ResolveAddress(SPC700AddressingMode::Imm);
            int8_t t = ReadByte(addL);
            if (flags.V)
            {
                PC += t;
                cycles += 2;
            }
            PC += 2;
            cycles += 2;
            break;
        }
        case 0x50: // BVC rel
        {
            ResolveAddress(SPC700AddressingMode::Imm);
            int8_t t = ReadByte(addL);
            if (!flags.V)
            {
                PC += t;
                cycles += 2;
            }
            PC += 2;
            cycles += 2;
            break;
        }
        case 0x30: // BMI rel
        {
            ResolveAddress(SPC700AddressingMode::Imm);
            int8_t t = ReadByte(addL);
            if (flags.N)
            {
                PC += t;
                cycles += 2;
            }
            PC += 2;
            cycles += 2;
            break;
        }
        case 0x10: // BPL rel
        {
            ResolveAddress(SPC700AddressingMode::Imm);
            int8_t t = ReadByte(addL);
            if (!flags.N)
            {
                PC += t;
                cycles += 2;
            }
            PC += 2;
            cycles += 2;
            break;
        }

        case 0x03:
        case 0x23:
        case 0x43:
        case 0x63:
        case 0x83:
        case 0xa3:
        case 0xc3:
        case 0xe3: // BBS dp,bit,rel
        {
            ResolveAddress(SPC700AddressingMode::SrcDst);
            uint8 d = ReadByte(addL);
            int8_t t = addH;
            if (d & (1 << ((inst & 0b11100000) >> 5)))
            {
                PC += t;
                cycles += 2;
            }
            PC += 3;
            cycles += 5;
            break;
        }

        case 0x13:
        case 0x33:
        case 0x53:
        case 0x73:
        case 0x93:
        case 0xb3:
        case 0xd3:
        case 0xf3: // BBC dp,bit,rel
        {
            ResolveAddress(SPC700AddressingMode::SrcDst);
            uint8 d = ReadByte(addL);
            int8_t t = addH;
            if (!(d & (1 << ((inst & 0b11100000) >> 5))))
            {
                PC += t;
                cycles += 2;
            }
            PC += 3;
            cycles += 5;
            break;
        }

        case 0x2E: // CBNE dp,rel
        {
            ResolveAddress(SPC700AddressingMode::SrcDst);
            uint8 d = ReadByte(addL);
            int8_t t = addH;
            if (d != A)
            {
                PC += t;
                cycles += 2;
            }
            PC += 3;
            cycles += 5;
            break;
        }
        case 0xDE: // CBNE (dp+x),rel
        {
            ResolveAddress(SPC700AddressingMode::DPX);
            uint8 d = ReadByte(addL);
            int8_t t = addH;
            if (d != A)
            {
                PC += t;
                cycles += 2;
            }
            PC += 3;
            cycles += 6;
            break;
        }
        case 0x6E: // DBNZ dp,rel
        {
            ResolveAddress(SPC700AddressingMode::SrcDst);
            uint8 d = ReadByte(addL);
            int8_t t = addH;
            d--;
            WriteByte(addL, d);
            if (d)
            {
                PC += t;
                cycles += 2;
            }
            PC += 3;
            cycles += 5;
            break;
        }
        case 0xFE: // DBNZ Y,rel
        {
            ResolveAddress(SPC700AddressingMode::Imm);
            int8_t t = ReadByte(addL);
            Y--;
            if (Y)
            {
                PC += t;
                cycles += 2;
            }
            PC += 2;
            cycles += 4;
            break;
        }

#pragma endregion
#pragma region CallsAndJumps
        case 0x5F: // JMP abs
        {
            ResolveAddress(SPC700AddressingMode::Abs);

            PC = addL;
            cycles += 3;
            break;
        }
        case 0x1F: // JMP (abs+x)
        {
            ResolveAddress(SPC700AddressingMode::AbsX);
            uint16 t = ReadByte(addL) | (ReadByte(addH) << 8);
            PC = t;
            cycles += 6;
            break;
        }

        case 0x3F: // CALL abs
        {
            ResolveAddress(SPC700AddressingMode::Abs);
            PC += 3;
            Push(PC >> 8);
            Push(PC & 0x00FF);
            PC = addL;
            cycles += 8;
            break;
        }

        case 0x4F: // PCALL upage
        {
            ResolveAddress(SPC700AddressingMode::Imm);
            PC += 2;
            Push(PC >> 8);
            Push(PC & 0x00FF);
            PC = 0XFF00 | ReadByte(addL);
            cycles += 6;
            break;
        }

        case 0x01:
        case 0x11:
        case 0x21:
        case 0x31:
        case 0x41:
        case 0x51:
        case 0x61:
        case 0x71:
        case 0x81:
        case 0x91:
        case 0xa1:
        case 0xb1:
        case 0xc1:
        case 0xd1:
        case 0xe1:
        case 0xf1: // TCALL n
        {
            uint16 n = inst >> 4;
            uint16 ta = 0xFFDE - (n << 1);
            PC += 1;
            Push(PC >> 8);
            Push(PC & 0x00FF);
            PC = ReadByte(ta) | (ReadByte(ta + 1) << 8);
            cycles += 8;
            break;
        }

        case 0x0F: // BRK
        {

            PC += 1;
            Push(PC >> 8);
            Push(PC & 0x00FF);
            Push(flags);
            flags.B = 1;
            flags.I = 0;
            PC = ReadByte(0xFFDE) | (ReadByte(0xFFDF) << 8);
            cycles += 8;
            break;
        }

        case 0x6F: // RET
        {
            PC = Pop();
            PC |= Pop() << 8;
            cycles += 5;
            break;
        }
        case 0x7F: // RETI
        {
            flags = Pop();
            PC = Pop();
            PC |= Pop() << 8;
            cycles += 6;
            break;
        }
#pragma endregion

#pragma region StackOps
        case 0x2D: // PUSH A
        {
            Push(A);
            PC += 1;
            cycles += 4;
            break;
        }
        case 0x4D: // PUSH X
        {
            Push(X);
            PC += 1;
            cycles += 4;
            break;
        }
        case 0x6D: // PUSH Y
        {
            Push(Y);
            PC += 1;
            cycles += 4;
            break;
        }
        case 0x0D: // PUSH flags
        {
            Push(flags);
            PC += 1;
            cycles += 4;
            break;
        }

        case 0xAE: // POP A
        {
            A = Pop();
            PC += 1;
            cycles += 4;
            break;
        }
        case 0xCE: // POP X
        {
            X = Pop();
            PC += 1;
            cycles += 4;
            break;
        }
        case 0xEE: // POP Y
        {
            Y = Pop();
            PC += 1;
            cycles += 4;
            break;
        }
        case 0x8E: // POP flags
        {
            flags = Pop();
            PC += 1;
            cycles += 4;
            break;
        }
#pragma endregion

#pragma region FlagOps
        case 0x60: // CLRC
        {
            flags.C = 0;
            PC += 1;
            cycles += 2;
            break;
        }
        case 0x80: // SETC
        {
            flags.C = 1;
            PC += 1;
            cycles += 2;
            break;
        }

        case 0xED: // NOTC
        {
            flags.C = !flags.C;
            PC += 1;
            cycles += 3;
            break;
        }

        case 0xE0: // CLRC
        {
            flags.V = 0;
            flags.H = 0;
            PC += 1;
            cycles += 2;
            break;
        }

        case 0x20: // CLRP
        {
            flags.P = 0;
            PC += 1;
            cycles += 2;
            break;
        }

        case 0x40: // SETP
        {
            flags.P = 1;
            PC += 1;
            cycles += 2;
            break;
        }

        case 0xA0: // EI
        {
            flags.I = 1;
            PC += 1;
            cycles += 3;
            break;
        }

        case 0xC0: // DI
        {
            flags.I = 0;
            PC += 1;
            cycles += 3;
            break;
        }
#pragma endregion

#pragma region BitOps
        case 0x02:
        case 0x22:
        case 0x42:
        case 0x62:
        case 0x82:
        case 0xa2:
        case 0xc2:
        case 0xe2: // SET Bit
        {
            uint8 n = inst >> 5;
            ResolveAddress(SPC700AddressingMode::DP);
            uint8 d = ReadByte(addL);
            d |= 1 << n;
            WriteByte(addL, d);
            PC += 2;
            cycles += 4;
            break;
        }

        case 0x12:
        case 0x32:
        case 0x52:
        case 0x72:
        case 0x92:
        case 0xb2:
        case 0xd2:
        case 0xf2: // CLR Bit
        {
            uint8 n = inst >> 5;
            ResolveAddress(SPC700AddressingMode::DP);
            uint8 d = ReadByte(addL);
            d &= ~(1 << n);
            WriteByte(addL, d);
            PC += 2;
            cycles += 4;
            break;
        }

        case 0x0e: // TSET abs
        {
            ResolveAddress(SPC700AddressingMode::Abs);
            uint8 d = ReadByte(addL);
            uint8 t = A - d;
            d |= A;
            WriteByte(addL, d);
            flags.Z = !(t);
            flags.N = t & 0x80;
            PC += 3;
            cycles += 6;
            break;
        }

        case 0x4e: // TCLR abs
        {
            ResolveAddress(SPC700AddressingMode::Abs);
            uint8 d = ReadByte(addL);
            uint8 t = A - d;
            d &= ~A;
            WriteByte(addL, d);
            flags.Z = !(t);
            flags.N = t & 0x80;
            PC += 3;
            cycles += 6;
            break;
        }

        case 0x4a: // AND C,mem.bit
        {
            ResolveAddress(SPC700AddressingMode::MemBit);
            bool d = ReadByte(addL) & (1 << addH);
            flags.C = flags.C && d;
            PC += 3;
            cycles += 4;
            break;
        }

        case 0x6a: // AND C,/mem.bit
        {
            ResolveAddress(SPC700AddressingMode::MemBit);
            bool d = ~ReadByte(addL) & (1 << addH);
            flags.C = flags.C && d;
            PC += 3;
            cycles += 4;
            break;
        }

        case 0x0a: // OR C,mem.bit
        {
            ResolveAddress(SPC700AddressingMode::MemBit);
            bool d = ReadByte(addL) & (1 << addH);
            flags.C = flags.C || d;
            PC += 3;
            cycles += 5;
            break;
        }

        case 0x2a: // OR C,/mem.bit
        {
            ResolveAddress(SPC700AddressingMode::MemBit);
            bool d = ~ReadByte(addL) & (1 << addH);
            flags.C = flags.C || d;
            PC += 3;
            cycles += 5;
            break;
        }

        case 0x8a: // EOR C,mem.bit
        {
            ResolveAddress(SPC700AddressingMode::MemBit);
            bool d = ReadByte(addL) & (1 << addH);
            flags.C = flags.C ^ d;
            PC += 3;
            cycles += 5;
            break;
        }

        case 0xea: // NOT mem.bit
        {
            ResolveAddress(SPC700AddressingMode::MemBit);
            uint8 d = ReadByte(addL) ^ (1 << addH);
            WriteByte(addL, d);
            PC += 3;
            cycles += 5;
            break;
        }

        case 0xaa: // MOV C,mem.bit
        {
            ResolveAddress(SPC700AddressingMode::MemBit);
            flags.C = ReadByte(addL) & (1 << addH);
            PC += 3;
            cycles += 4;
            break;
        }

        case 0xca: // MOV mem.bit,C
        {
            ResolveAddress(SPC700AddressingMode::MemBit);
            uint8 d = ReadByte(addL) & (~(1 << addH));
            d = d | ((flags.C << addH));
            WriteByte(addL, d);
            PC += 3;
            cycles += 6;
            break;
        }

#pragma endregion

        default:
            break;
        }
    }
};