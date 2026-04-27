#include "SPC700.h"

SPC700Flags flags;
SPC700Regs regs;
uint16 cycles;
uint16 addL;
uint16 addH;

void SPC700_ResolveAddress(SPC700AddressingMode mode)
{
    uint8 ll, hh;
    ll = SPCRead(regs.PC + 1);
    hh = SPCRead(regs.PC + 2);

    switch (mode)
    {
    case SPC700_AM_Imm:
        addL = regs.PC + 1;
        break;

    case SPC700_AM_XPtr:
        addL = regs.X | (flags.P ? 0x0100 : 0x0000);
        break;

    case SPC700_AM_YPtr:
        addL = regs.Y | (flags.P ? 0x0100 : 0x0000);
        break;

    case SPC700_AM_DP:
        addL = ll | (flags.P ? 0x0100 : 0x0000);
        addH = ((ll + 1) & 0x00FF) | (flags.P ? 0x0100 : 0x0000);
        break;

    case SPC700_AM_DPX:
        addL = ((uint8)(ll + regs.X)) | (flags.P ? 0x0100 : 0x0000);
        addH = hh;
        break;
    case SPC700_AM_DPY:
        addL = ((uint8)(ll + regs.Y)) | (flags.P ? 0x0100 : 0x0000);
        break;

    case SPC700_AM_Abs:
        addL = (hh << 8) | ll;
        break;

    case SPC700_AM_AbsX:
        addL = ((hh << 8) | ll) + regs.X;
        addH = ((hh << 8) | ll) + regs.X + 1;
        break;

    case SPC700_AM_AbsY:
        addL = ((hh << 8) | ll) + regs.Y;
        break;

    case SPC700_AM_DPXPtr:
        addL = ((uint8)(ll + regs.X)) | (flags.P ? 0x0100 : 0x0000);
        addH = ((uint8)(ll + regs.X + 1)) | (flags.P ? 0x0100 : 0x0000);
        addL = (SPCRead(addH) << 8) | SPCRead(addL);
        break;

    case SPC700_AM_DPPtrY:
        addL = ((uint8)(ll)) | (flags.P ? 0x0100 : 0x0000);
        addH = ((uint8)(ll + 1)) | (flags.P ? 0x0100 : 0x0000);
        addL = (SPCRead(addH) << 8) | SPCRead(addL);
        addL += regs.Y;
        break;

    case SPC700_AM_SrcDst:
        addL = ll | (flags.P ? 0x0100 : 0x0000);
        addH = hh | (flags.P ? 0x0100 : 0x0000);
        break;

    case SPC700_AM_MemBit:
        addL = (ll) | ((hh & 0x1F) << 8);
        addH = (hh & 0xE0) >> 5;
        break;

    default:
        break;
    }
}

uint8 SPC700_GetFlagsAsByte()
{
    return (flags.N ? spc700_FlagNMask : 0) | (flags.V ? spc700_FlagVMask : 0) | (flags.P ? spc700_FlagPMask : 0) | (flags.B ? spc700_FlagBMask : 0) | (flags.H ? spc700_FlagHMask : 0) | (flags.I ? spc700_FlagIMask : 0) | (flags.Z ? spc700_FlagZMask : 0) | (flags.C ? spc700_FlagCMask : 0);
}
void SPC700_SetFlagsAsByte(uint8 bits)
{
    flags.N = bits & spc700_FlagNMask;
    flags.V = bits & spc700_FlagVMask;
    flags.P = bits & spc700_FlagPMask;
    flags.B = bits & spc700_FlagBMask;
    flags.H = bits & spc700_FlagHMask;
    flags.I = bits & spc700_FlagIMask;
    flags.Z = bits & spc700_FlagZMask;
    flags.C = bits & spc700_FlagCMask;
}
uint8 SPC700_DoADC(uint8 A, uint8 B)
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

uint8 SPC700_DoSBC(uint8 A, uint8 B)
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

uint8 SPC700_DoCMP(uint8 A, uint8 B)
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

void SPC700_Push(uint8 data)
{
    SPCWrite(0x0100 | regs.SP, data);
    regs.SP--;
}

uint8 SPC700_Pop()
{
    regs.SP++;
    return SPCRead(0x0100 | regs.SP);
}

// void PrintState()
// {
//     // nvmxdizc
//     cout << "--------------------------NEW INSTRUCTON---------------------------------------" << endl;
//     cout << "-----------Flags----------" << endl;
//     cout << "N V P B H I Z C" << endl;
//     cout << flags.N << " " << flags.V << " " << flags.P << " " << flags.B << " " << flags.H << " " << flags.I << " " << flags.Z << " " << flags.C << endl;
//     cout << "-----------Regs----------" << endl;
//     // cout << "Stack top : " << hex << (unsigned int)SPCRead(cregs.S + 1) << " " << (unsigned int)SPCRead(cregs.S + 2) << endl;
//     cout << "regs.A : " << std::hex << (unsigned int)regs.A << endl;
//     cout << "regs.SP : " << std::hex << (unsigned int)regs.SP << endl;
//     cout << "regs.X : " << std::hex << (unsigned int)regs.X << endl;
//     cout << "regs.Y : " << std::hex << (unsigned int)regs.Y << endl;
//     cout << "regs.PC : " << std::hex << (unsigned int)regs.PC << endl;
//     uint8_t inst = SPCRead(regs.PC);
//     cout << "OpCode : " << std::hex << (int)inst << endl;
//     cout << "3 Bytes Ahead : " << std::hex << (int)SPCRead(regs.PC + 1) << " " << std::hex << (int)SPCRead(regs.PC + 2) << " " << std::hex << (int)SPCRead(regs.PC + 3) << " " << endl;
// }

void SPC700_Reset()
{
    cycles = 0;
    regs.A = regs.X = regs.Y = 0;
    flags.N = flags.V = flags.P = flags.B = flags.H = flags.I = flags.Z = flags.C = 0;
    regs.PC = 0xFFC0;
    addL = 0;
}

void SPC700_Step()
{
    uint8 inst = SPCRead(regs.PC);

    switch (inst)
    {
#pragma region DataTransfer_MemToReg
    case 0xE8: // MOV regs.A,Imm
    {
        SPC700_ResolveAddress(SPC700_AM_Imm);
        uint8 imm = SPCRead(addL);
        regs.A = imm;
        flags.N = regs.A & 0x80;
        flags.Z = !(regs.A);
        regs.PC += 2;
        cycles += 2;
        break;
    }

    case 0xE6: // MOV regs.A,(regs.X)
    {
        SPC700_ResolveAddress(SPC700_AM_XPtr);
        uint8 d = SPCRead(addL);
        regs.A = d;
        flags.N = regs.A & 0x80;
        flags.Z = !(regs.A);
        regs.PC += 1;
        cycles += 3;
        break;
    }

    case 0xBF: // MOV regs.A,(regs.X)+
    {
        SPC700_ResolveAddress(SPC700_AM_XPtr);
        uint8 d = SPCRead(addL);
        regs.A = d;
        flags.N = regs.A & 0x80;
        flags.Z = !(regs.A);
        regs.X += 1;
        regs.PC += 1;
        cycles += 4;
        break;
    }

    case 0xE4: // MOV regs.A,(dp)
    {
        SPC700_ResolveAddress(SPC700_AM_DP);
        uint8 d = SPCRead(addL);
        regs.A = d;
        flags.N = regs.A & 0x80;
        flags.Z = !(regs.A);
        regs.PC += 2;
        cycles += 3;
        break;
    }

    case 0xF4: // MOV regs.A,(dp+regs.X)
    {
        SPC700_ResolveAddress(SPC700_AM_DPX);
        uint8 d = SPCRead(addL);
        regs.A = d;
        flags.N = regs.A & 0x80;
        flags.Z = !(regs.A);
        regs.PC += 2;
        cycles += 4;
        break;
    }

    case 0xE5: // MOV regs.A,(abs)
    {
        SPC700_ResolveAddress(SPC700_AM_Abs);
        uint8 d = SPCRead(addL);
        regs.A = d;
        flags.N = regs.A & 0x80;
        flags.Z = !(regs.A);
        regs.PC += 3;
        cycles += 4;
        break;
    }

    case 0xF5: // MOV regs.A,(abs+regs.X)
    {
        SPC700_ResolveAddress(SPC700_AM_AbsX);
        uint8 d = SPCRead(addL);
        regs.A = d;
        flags.N = regs.A & 0x80;
        flags.Z = !(regs.A);
        regs.PC += 3;
        cycles += 5;
        break;
    }

    case 0xF6: // MOV regs.A,(abs+regs.X)
    {
        SPC700_ResolveAddress(SPC700_AM_AbsY);
        uint8 d = SPCRead(addL);
        regs.A = d;
        flags.N = regs.A & 0x80;
        flags.Z = !(regs.A);
        regs.PC += 3;
        cycles += 5;
        break;
    }

    case 0xE7: // MOV regs.A,((dp+regs.X))
    {
        SPC700_ResolveAddress(SPC700_AM_DPXPtr);
        uint8 d = SPCRead(addL);
        regs.A = d;
        flags.N = regs.A & 0x80;
        flags.Z = !(regs.A);
        regs.PC += 2;
        cycles += 6;
        break;
    }

    case 0xF7: // MOV regs.A,((dp),y)
    {
        SPC700_ResolveAddress(SPC700_AM_DPPtrY);
        uint8 d = SPCRead(addL);
        regs.A = d;
        flags.N = regs.A & 0x80;
        flags.Z = !(regs.A);
        regs.PC += 2;
        cycles += 6;
        break;
    }

    case 0xCD: // MOV regs.X,Imm
    {
        SPC700_ResolveAddress(SPC700_AM_Imm);
        uint8 d = SPCRead(addL);
        regs.X = d;
        flags.N = regs.X & 0x80;
        flags.Z = !(regs.X);
        regs.PC += 2;
        cycles += 2;
        break;
    }

    case 0xF8: // MOV regs.X,(dp)
    {
        SPC700_ResolveAddress(SPC700_AM_DP);
        uint8 d = SPCRead(addL);
        regs.X = d;
        flags.N = regs.X & 0x80;
        flags.Z = !(regs.X);
        regs.PC += 2;
        cycles += 3;
        break;
    }

    case 0xF9: // MOV regs.X,(dp+regs.Y)
    {
        SPC700_ResolveAddress(SPC700_AM_DPY);
        uint8 d = SPCRead(addL);
        regs.X = d;
        flags.N = regs.X & 0x80;
        flags.Z = !(regs.X);
        regs.PC += 2;
        cycles += 4;
        break;
    }

    case 0xE9: // MOV regs.X,(abs)
    {
        SPC700_ResolveAddress(SPC700_AM_Abs);
        uint8 d = SPCRead(addL);
        regs.X = d;
        flags.N = regs.X & 0x80;
        flags.Z = !(regs.X);
        regs.PC += 3;
        cycles += 4;
        break;
    }

    case 0x8D: // MOV regs.Y,Imm
    {
        SPC700_ResolveAddress(SPC700_AM_Imm);
        uint8 d = SPCRead(addL);
        regs.Y = d;
        flags.N = regs.Y & 0x80;
        flags.Z = !(regs.Y);
        regs.PC += 2;
        cycles += 2;
        break;
    }

    case 0xEB: // MOV regs.Y,(dp)
    {
        SPC700_ResolveAddress(SPC700_AM_DP);
        uint8 d = SPCRead(addL);
        regs.Y = d;
        flags.N = regs.Y & 0x80;
        flags.Z = !(regs.Y);
        regs.PC += 2;
        cycles += 3;
        break;
    }

    case 0xFB: // MOV regs.Y,(dp+regs.X)
    {
        SPC700_ResolveAddress(SPC700_AM_DPX);
        uint8 d = SPCRead(addL);
        regs.Y = d;
        flags.N = regs.Y & 0x80;
        flags.Z = !(regs.Y);
        regs.PC += 2;
        cycles += 4;
        break;
    }

    case 0xEC: // MOV regs.Y,(abs)
    {
        SPC700_ResolveAddress(SPC700_AM_Abs);
        uint8 d = SPCRead(addL);
        regs.Y = d;
        flags.N = regs.Y & 0x80;
        flags.Z = !(regs.Y);
        regs.PC += 3;
        cycles += 4;
        break;
    }

#pragma endregion
#pragma region DataTransfer_RegToMem

    case 0xC6: // MOV (regs.X),regs.A
    {
        SPC700_ResolveAddress(SPC700_AM_XPtr);
        SPCWrite(addL, regs.A);
        regs.PC += 1;
        cycles += 4;
        break;
    }

    case 0xAF: // MOV (regs.X)+,regs.A
    {
        SPC700_ResolveAddress(SPC700_AM_XPtr);
        SPCWrite(addL, regs.A);
        regs.X += 1;
        regs.PC += 1;
        cycles += 4;
        break;
    }

    case 0xC4: // MOV (dp),regs.A
    {
        SPC700_ResolveAddress(SPC700_AM_DP);
        SPCWrite(addL, regs.A);
        regs.PC += 2;
        cycles += 4;
        break;
    }

    case 0xD4: // MOV (dp+regs.X),regs.A
    {
        SPC700_ResolveAddress(SPC700_AM_DPX);
        SPCWrite(addL, regs.A);
        regs.PC += 2;
        cycles += 5;
        break;
    }

    case 0xC5: // MOV (abs),regs.A
    {
        SPC700_ResolveAddress(SPC700_AM_Abs);
        SPCWrite(addL, regs.A);
        regs.PC += 3;
        cycles += 5;
        break;
    }

    case 0xD5: // MOV (abs+regs.X),regs.A
    {
        SPC700_ResolveAddress(SPC700_AM_AbsX);
        SPCWrite(addL, regs.A);
        regs.PC += 3;
        cycles += 6;
        break;
    }

    case 0xD6: // MOV (abs+regs.X),regs.A
    {
        SPC700_ResolveAddress(SPC700_AM_AbsY);
        SPCWrite(addL, regs.A);
        regs.PC += 3;
        cycles += 6;
        break;
    }

    case 0xC7: // MOV ((dp+regs.X)),regs.A
    {
        SPC700_ResolveAddress(SPC700_AM_DPXPtr);
        SPCWrite(addL, regs.A);
        regs.PC += 2;
        cycles += 7;
        break;
    }

    case 0xD7: // MOV ((dp),y),regs.A
    {
        SPC700_ResolveAddress(SPC700_AM_DPPtrY);
        SPCWrite(addL, regs.A);
        regs.PC += 2;
        cycles += 7;
        break;
    }

    case 0xD8: // MOV (dp),regs.X
    {
        SPC700_ResolveAddress(SPC700_AM_DP);
        SPCWrite(addL, regs.X);
        regs.PC += 2;
        cycles += 4;
        break;
    }

    case 0xD9: // MOV (dp+regs.Y),regs.X
    {
        SPC700_ResolveAddress(SPC700_AM_DPY);
        SPCWrite(addL, regs.X);
        regs.PC += 2;
        cycles += 5;
        break;
    }

    case 0xC9: // MOV (abs),regs.X
    {
        SPC700_ResolveAddress(SPC700_AM_Abs);
        SPCWrite(addL, regs.X);
        regs.PC += 3;
        cycles += 5;
        break;
    }

    case 0xCB: // MOV (dp),regs.Y
    {
        SPC700_ResolveAddress(SPC700_AM_DP);
        SPCWrite(addL, regs.Y);
        regs.PC += 2;
        cycles += 5;
        break;
    }

    case 0xDB: // MOV (dp+regs.X),regs.Y
    {
        SPC700_ResolveAddress(SPC700_AM_DPX);
        SPCWrite(addL, regs.Y);
        regs.PC += 2;
        cycles += 5;
        break;
    }

    case 0xCC: // MOV regs.Y,(abs)
    {
        SPC700_ResolveAddress(SPC700_AM_Abs);
        SPCWrite(addL, regs.Y);
        regs.PC += 3;
        cycles += 5;
        break;
    }
#pragma endregion
#pragma region DataTransfer_RegToReg
    case 0x7D: // MOV regs.A,regs.X
    {
        regs.A = regs.X;
        flags.N = regs.A & 0x80;
        flags.Z = !(regs.A);
        regs.PC += 1;
        cycles += 2;
        break;
    }
    case 0xDD: // MOV regs.A,regs.Y
    {
        regs.A = regs.Y;
        flags.N = regs.A & 0x80;
        flags.Z = !(regs.A);
        regs.PC += 1;
        cycles += 2;
        break;
    }
    case 0x5D: // MOV regs.X,regs.A
    {
        regs.X = regs.A;
        flags.N = regs.X & 0x80;
        flags.Z = !(regs.X);
        regs.PC += 1;
        cycles += 2;
        break;
    }
    case 0xFD: // MOV regs.Y,regs.A
    {
        regs.Y = regs.A;
        flags.N = regs.Y & 0x80;
        flags.Z = !(regs.Y);
        regs.PC += 1;
        cycles += 2;
        break;
    }
    case 0x9D: // MOV regs.X,regs.SP
    {
        regs.X = regs.SP;
        flags.N = regs.X & 0x80;
        flags.Z = !(regs.X);
        regs.PC += 1;
        cycles += 2;
        break;
    }
    case 0xBD: // MOV regs.SP,regs.X
    {
        regs.SP = regs.X;
        regs.PC += 1;
        cycles += 2;
        break;
    }
#pragma endregion
#pragma region DataTransfer_MemToMem
    case 0xFA: // MOV dest,src
    {
        SPC700_ResolveAddress(SPC700_AM_SrcDst);
        SPCWrite(addH, SPCRead(addL));
        regs.PC += 3;
        cycles += 5;
        break;
    }
    case 0x8F: // MOV dp,imm
    {
        SPC700_ResolveAddress(SPC700_AM_SrcDst);
        SPCWrite(addH, addL & 0x00FF);
        regs.PC += 3;
        cycles += 5;
        break;
    }
#pragma endregion
#pragma region Arithmatics
    case 0x88: // ADC regs.A,imm
    {
        SPC700_ResolveAddress(SPC700_AM_Imm);
        uint8 d = SPCRead(addL);
        regs.A = SPC700_DoADC(regs.A, d);
        regs.PC += 2;
        cycles += 2;
        break;
    }
    case 0x86: // ADC regs.A,(regs.X)
    {
        SPC700_ResolveAddress(SPC700_AM_XPtr);
        uint8 d = SPCRead(addL);
        regs.A = SPC700_DoADC(regs.A, d);
        regs.PC += 1;
        cycles += 3;
        break;
    }
    case 0x84: // ADC regs.A,(dp)
    {
        SPC700_ResolveAddress(SPC700_AM_DP);
        uint8 d = SPCRead(addL);
        regs.A = SPC700_DoADC(regs.A, d);
        regs.PC += 2;
        cycles += 3;
        break;
    }
    case 0x94: // ADC regs.A,(dp+regs.X)
    {
        SPC700_ResolveAddress(SPC700_AM_DPX);
        uint8 d = SPCRead(addL);
        regs.A = SPC700_DoADC(regs.A, d);
        regs.PC += 2;
        cycles += 4;
        break;
    }
    case 0x85: // ADC regs.A,(abs)
    {
        SPC700_ResolveAddress(SPC700_AM_Abs);
        uint8 d = SPCRead(addL);
        regs.A = SPC700_DoADC(regs.A, d);
        regs.PC += 3;
        cycles += 4;
        break;
    }
    case 0x95: // ADC regs.A,(abs+regs.X)
    {
        SPC700_ResolveAddress(SPC700_AM_AbsX);
        uint8 d = SPCRead(addL);
        regs.A = SPC700_DoADC(regs.A, d);
        regs.PC += 3;
        cycles += 5;
        break;
    }
    case 0x96: // ADC regs.A,(abs+regs.Y)
    {
        SPC700_ResolveAddress(SPC700_AM_AbsY);
        uint8 d = SPCRead(addL);
        regs.A = SPC700_DoADC(regs.A, d);
        regs.PC += 3;
        cycles += 5;
        break;
    }

    case 0x87: // ADC regs.A,((dp+x))
    {
        SPC700_ResolveAddress(SPC700_AM_DPXPtr);
        uint8 d = SPCRead(addL);
        regs.A = SPC700_DoADC(regs.A, d);
        regs.PC += 2;
        cycles += 6;
        break;
    }
    case 0x97: // ADC regs.A,((dp)+y)
    {
        SPC700_ResolveAddress(SPC700_AM_DPPtrY);
        uint8 d = SPCRead(addL);
        regs.A = SPC700_DoADC(regs.A, d);
        regs.PC += 2;
        cycles += 6;
        break;
    }

    case 0x99: // ADC (x),(y)
    {

        SPC700_ResolveAddress(SPC700_AM_YPtr);
        uint8 y = SPCRead(addL);
        SPC700_ResolveAddress(SPC700_AM_XPtr);
        uint8 x = SPCRead(addL);
        SPCWrite(addL, SPC700_DoADC(x, y));
        regs.PC += 1;
        cycles += 5;
        break;
    }
    case 0x89: // ADC dp,dp
    {

        SPC700_ResolveAddress(SPC700_AM_SrcDst);
        uint8 x = SPCRead(addL);
        uint8 y = SPCRead(addH);
        SPCWrite(addH, SPC700_DoADC(x, y));
        regs.PC += 3;
        cycles += 6;
        break;
    }
    case 0x98: // ADC dp,imm
    {
        SPC700_ResolveAddress(SPC700_AM_SrcDst);
        uint8 x = addL;
        uint8 y = SPCRead(addH);
        SPCWrite(addH, SPC700_DoADC(x, y));
        regs.PC += 3;
        cycles += 5;
        break;
    }

    case 0xA8: // SBC regs.A,imm
    {
        SPC700_ResolveAddress(SPC700_AM_Imm);
        uint8 d = SPCRead(addL);
        regs.A = SPC700_DoSBC(regs.A, d);
        regs.PC += 2;
        cycles += 2;
        break;
    }
    case 0xA6: // SBC regs.A,(regs.X)
    {
        SPC700_ResolveAddress(SPC700_AM_XPtr);
        uint8 d = SPCRead(addL);
        regs.A = SPC700_DoSBC(regs.A, d);
        regs.PC += 1;
        cycles += 3;
        break;
    }
    case 0xA4: // SBC regs.A,(dp)
    {
        SPC700_ResolveAddress(SPC700_AM_DP);
        uint8 d = SPCRead(addL);
        regs.A = SPC700_DoSBC(regs.A, d);
        regs.PC += 2;
        cycles += 3;
        break;
    }
    case 0xB4: // SBC regs.A,(dp+regs.X)
    {
        SPC700_ResolveAddress(SPC700_AM_DPX);
        uint8 d = SPCRead(addL);
        regs.A = SPC700_DoSBC(regs.A, d);
        regs.PC += 2;
        cycles += 4;
        break;
    }
    case 0xA5: // SBC regs.A,(abs)
    {
        SPC700_ResolveAddress(SPC700_AM_Abs);
        uint8 d = SPCRead(addL);
        regs.A = SPC700_DoSBC(regs.A, d);
        regs.PC += 3;
        cycles += 4;
        break;
    }
    case 0xB5: // SBC regs.A,(abs+regs.X)
    {
        SPC700_ResolveAddress(SPC700_AM_AbsX);
        uint8 d = SPCRead(addL);
        regs.A = SPC700_DoSBC(regs.A, d);
        regs.PC += 3;
        cycles += 5;
        break;
    }
    case 0xB6: // SBC regs.A,(abs+regs.Y)
    {
        SPC700_ResolveAddress(SPC700_AM_AbsY);
        uint8 d = SPCRead(addL);
        regs.A = SPC700_DoSBC(regs.A, d);
        regs.PC += 3;
        cycles += 5;
        break;
    }

    case 0xA7: // SBC regs.A,((dp+x))
    {
        SPC700_ResolveAddress(SPC700_AM_DPXPtr);
        uint8 d = SPCRead(addL);
        regs.A = SPC700_DoSBC(regs.A, d);
        regs.PC += 2;
        cycles += 6;
        break;
    }
    case 0xB7: // SBC regs.A,((dp)+y)
    {
        SPC700_ResolveAddress(SPC700_AM_DPPtrY);
        uint8 d = SPCRead(addL);
        regs.A = SPC700_DoSBC(regs.A, d);
        regs.PC += 2;
        cycles += 6;
        break;
    }

    case 0xB9: // SBC (x),(y)
    {

        SPC700_ResolveAddress(SPC700_AM_YPtr);
        uint8 y = SPCRead(addL);
        SPC700_ResolveAddress(SPC700_AM_XPtr);
        uint8 x = SPCRead(addL);
        SPCWrite(addL, SPC700_DoSBC(x, y));
        regs.PC += 1;
        cycles += 5;
        break;
    }
    case 0xA9: // SBC dp,dp
    {

        SPC700_ResolveAddress(SPC700_AM_SrcDst);
        uint8 x = SPCRead(addH);
        uint8 y = SPCRead(addL);
        SPCWrite(addH, SPC700_DoSBC(x, y));
        regs.PC += 3;
        cycles += 6;
        break;
    }
    case 0xB8: // SBC dp,imm
    {
        SPC700_ResolveAddress(SPC700_AM_SrcDst);
        uint8 x = SPCRead(addH);
        uint8 y = addL;
        SPCWrite(addH, SPC700_DoSBC(x, y));
        regs.PC += 3;
        cycles += 5;
        break;
    }

    case 0x68: // CMP regs.A,imm
    {
        SPC700_ResolveAddress(SPC700_AM_Imm);
        uint8 d = SPCRead(addL);
        SPC700_DoCMP(regs.A, d);
        regs.PC += 2;
        cycles += 2;
        break;
    }
    case 0x66: // CMP regs.A,(regs.X)
    {
        SPC700_ResolveAddress(SPC700_AM_XPtr);
        uint8 d = SPCRead(addL);
        SPC700_DoCMP(regs.A, d);
        regs.PC += 1;
        cycles += 3;
        break;
    }
    case 0x64: // CMP regs.A,(dp)
    {
        SPC700_ResolveAddress(SPC700_AM_DP);
        uint8 d = SPCRead(addL);
        SPC700_DoCMP(regs.A, d);
        regs.PC += 2;
        cycles += 3;
        break;
    }
    case 0x74: // CMP regs.A,(dp+regs.X)
    {
        SPC700_ResolveAddress(SPC700_AM_DPX);
        uint8 d = SPCRead(addL);
        SPC700_DoCMP(regs.A, d);
        regs.PC += 2;
        cycles += 4;
        break;
    }
    case 0x65: // CMP regs.A,(abs)
    {
        SPC700_ResolveAddress(SPC700_AM_Abs);
        uint8 d = SPCRead(addL);
        SPC700_DoCMP(regs.A, d);
        regs.PC += 3;
        cycles += 4;
        break;
    }
    case 0x75: // CMP regs.A,(abs+regs.X)
    {
        SPC700_ResolveAddress(SPC700_AM_AbsX);
        uint8 d = SPCRead(addL);
        SPC700_DoCMP(regs.A, d);
        regs.PC += 3;
        cycles += 5;
        break;
    }
    case 0x76: // CMP regs.A,(abs+regs.Y)
    {
        SPC700_ResolveAddress(SPC700_AM_AbsY);
        uint8 d = SPCRead(addL);
        SPC700_DoCMP(regs.A, d);
        regs.PC += 3;
        cycles += 5;
        break;
    }

    case 0x67: // CMP regs.A,((dp+x))
    {
        SPC700_ResolveAddress(SPC700_AM_DPXPtr);
        uint8 d = SPCRead(addL);
        SPC700_DoCMP(regs.A, d);
        regs.PC += 2;
        cycles += 6;
        break;
    }
    case 0x77: // CMP regs.A,((dp)+y)
    {
        SPC700_ResolveAddress(SPC700_AM_DPPtrY);
        uint8 d = SPCRead(addL);
        SPC700_DoCMP(regs.A, d);
        regs.PC += 2;
        cycles += 6;
        break;
    }

    case 0x79: // CMP (x),(y)
    {

        SPC700_ResolveAddress(SPC700_AM_YPtr);
        uint8 y = SPCRead(addL);
        SPC700_ResolveAddress(SPC700_AM_XPtr);
        uint8 x = SPCRead(addL);
        SPC700_DoCMP(x, y);
        regs.PC += 1;
        cycles += 5;
        break;
    }
    case 0x69: // CMP dp,dp
    {

        SPC700_ResolveAddress(SPC700_AM_SrcDst);
        uint8 x = SPCRead(addH);
        uint8 y = SPCRead(addL);
        SPC700_DoCMP(x, y);
        regs.PC += 3;
        cycles += 6;
        break;
    }
    case 0x78: // CMP dp,imm
    {
        SPC700_ResolveAddress(SPC700_AM_SrcDst);
        uint8 x = SPCRead(addH);
        uint8 y = addL;
        SPC700_DoCMP(x, y);
        regs.PC += 3;
        cycles += 5;
        break;
    }

    case 0xC8: // CMP regs.X,imm
    {
        SPC700_ResolveAddress(SPC700_AM_Imm);
        uint8 d = SPCRead(addL);
        SPC700_DoCMP(regs.X, d);
        regs.PC += 2;
        cycles += 2;
        break;
    }
    case 0x3E: // CMP regs.X,(dp)
    {
        SPC700_ResolveAddress(SPC700_AM_DP);
        uint8 d = SPCRead(addL);
        SPC700_DoCMP(regs.X, d);
        regs.PC += 2;
        cycles += 3;
        break;
    }
    case 0x1E: // CMP regs.X,(abs)
    {
        SPC700_ResolveAddress(SPC700_AM_Abs);
        uint8 d = SPCRead(addL);
        SPC700_DoCMP(regs.X, d);
        regs.PC += 3;
        cycles += 4;
        break;
    }

    case 0xAD: // CMP regs.Y,imm
    {
        SPC700_ResolveAddress(SPC700_AM_Imm);
        uint8 d = SPCRead(addL);
        SPC700_DoCMP(regs.Y, d);
        regs.PC += 2;
        cycles += 2;
        break;
    }
    case 0x7E: // CMP regs.Y,(dp)
    {
        SPC700_ResolveAddress(SPC700_AM_DP);
        uint8 d = SPCRead(addL);
        SPC700_DoCMP(regs.Y, d);
        regs.PC += 2;
        cycles += 3;
        break;
    }
    case 0x5E: // CMP regs.Y,(abs)
    {
        SPC700_ResolveAddress(SPC700_AM_Abs);
        uint8 d = SPCRead(addL);
        SPC700_DoCMP(regs.Y, d);
        regs.PC += 3;
        cycles += 4;
        break;
    }
#pragma endregion
#pragma region LogicOperations
    case 0x28: // AND regs.A,imm
    {
        SPC700_ResolveAddress(SPC700_AM_Imm);
        uint8 d = SPCRead(addL);
        regs.A = regs.A & d;
        flags.Z = !(regs.A);
        flags.N = regs.A & 0x80;
        regs.PC += 2;
        cycles += 2;
        break;
    }
    case 0x26: // AND regs.A,(regs.X)
    {
        SPC700_ResolveAddress(SPC700_AM_XPtr);
        uint8 d = SPCRead(addL);
        regs.A = regs.A & d;
        flags.Z = !(regs.A);
        flags.N = regs.A & 0x80;
        regs.PC += 1;
        cycles += 3;
        break;
    }
    case 0x24: // AND regs.A,(dp)
    {
        SPC700_ResolveAddress(SPC700_AM_DP);
        uint8 d = SPCRead(addL);
        regs.A = regs.A & d;
        flags.Z = !(regs.A);
        flags.N = regs.A & 0x80;
        regs.PC += 2;
        cycles += 3;
        break;
    }
    case 0x34: // AND regs.A,(dp+regs.X)
    {
        SPC700_ResolveAddress(SPC700_AM_DPX);
        uint8 d = SPCRead(addL);
        regs.A = regs.A & d;
        flags.Z = !(regs.A);
        flags.N = regs.A & 0x80;
        regs.PC += 2;
        cycles += 4;
        break;
    }
    case 0x25: // AND regs.A,(abs)
    {
        SPC700_ResolveAddress(SPC700_AM_Abs);
        uint8 d = SPCRead(addL);
        regs.A = regs.A & d;
        flags.Z = !(regs.A);
        flags.N = regs.A & 0x80;
        regs.PC += 3;
        cycles += 4;
        break;
    }
    case 0x35: // AND regs.A,(abs+regs.X)
    {
        SPC700_ResolveAddress(SPC700_AM_AbsX);
        uint8 d = SPCRead(addL);
        regs.A = regs.A & d;
        flags.Z = !(regs.A);
        flags.N = regs.A & 0x80;
        regs.PC += 3;
        cycles += 5;
        break;
    }
    case 0x36: // AND regs.A,(abs+regs.Y)
    {
        SPC700_ResolveAddress(SPC700_AM_AbsY);
        uint8 d = SPCRead(addL);
        regs.A = regs.A & d;
        flags.Z = !(regs.A);
        flags.N = regs.A & 0x80;
        regs.PC += 3;
        cycles += 5;
        break;
    }

    case 0x27: // AND regs.A,((dp+x))
    {
        SPC700_ResolveAddress(SPC700_AM_DPXPtr);
        uint8 d = SPCRead(addL);
        regs.A = regs.A & d;
        flags.Z = !(regs.A);
        flags.N = regs.A & 0x80;
        regs.PC += 2;
        cycles += 6;
        break;
    }
    case 0x37: // AND regs.A,((dp)+y)
    {
        SPC700_ResolveAddress(SPC700_AM_DPPtrY);
        uint8 d = SPCRead(addL);
        regs.A = regs.A & d;
        flags.Z = !(regs.A);
        flags.N = regs.A & 0x80;
        regs.PC += 2;
        cycles += 6;
        break;
    }

    case 0x39: // AND (x),(y)
    {

        SPC700_ResolveAddress(SPC700_AM_YPtr);
        uint8 y = SPCRead(addL);
        SPC700_ResolveAddress(SPC700_AM_XPtr);
        uint8 x = SPCRead(addL);
        uint8 t = x & y;
        flags.Z = !(t);
        flags.N = t & 0x80;
        SPCWrite(addL, t);
        regs.PC += 1;
        cycles += 5;
        break;
    }
    case 0x29: // AND dp,dp
    {

        SPC700_ResolveAddress(SPC700_AM_SrcDst);
        uint8 x = SPCRead(addL);
        uint8 y = SPCRead(addH);
        uint8 t = x & y;
        flags.Z = !(t);
        flags.N = t & 0x80;
        SPCWrite(addH, t);
        regs.PC += 3;
        cycles += 6;
        break;
    }
    case 0x38: // AND dp,imm
    {
        SPC700_ResolveAddress(SPC700_AM_SrcDst);
        uint8 x = addL;
        uint8 y = SPCRead(addH);
        uint8 t = x & y;
        flags.Z = !(t);
        flags.N = t & 0x80;
        SPCWrite(addH, t);
        regs.PC += 3;
        cycles += 5;
        break;
    }

    case 0x08: // OR regs.A,imm
    {
        SPC700_ResolveAddress(SPC700_AM_Imm);
        uint8 d = SPCRead(addL);
        regs.A = regs.A | d;
        flags.Z = !(regs.A);
        flags.N = regs.A & 0x80;
        regs.PC += 2;
        cycles += 2;
        break;
    }
    case 0x06: // OR regs.A,(regs.X)
    {
        SPC700_ResolveAddress(SPC700_AM_XPtr);
        uint8 d = SPCRead(addL);
        regs.A = regs.A | d;
        flags.Z = !(regs.A);
        flags.N = regs.A & 0x80;
        regs.PC += 1;
        cycles += 3;
        break;
    }
    case 0x04: // OR regs.A,(dp)
    {
        SPC700_ResolveAddress(SPC700_AM_DP);
        uint8 d = SPCRead(addL);
        regs.A = regs.A | d;
        flags.Z = !(regs.A);
        flags.N = regs.A & 0x80;
        regs.PC += 2;
        cycles += 3;
        break;
    }
    case 0x14: // OR regs.A,(dp+regs.X)
    {
        SPC700_ResolveAddress(SPC700_AM_DPX);
        uint8 d = SPCRead(addL);
        regs.A = regs.A | d;
        flags.Z = !(regs.A);
        flags.N = regs.A & 0x80;
        regs.PC += 2;
        cycles += 4;
        break;
    }
    case 0x05: // OR regs.A,(abs)
    {
        SPC700_ResolveAddress(SPC700_AM_Abs);
        uint8 d = SPCRead(addL);
        regs.A = regs.A | d;
        flags.Z = !(regs.A);
        flags.N = regs.A & 0x80;
        regs.PC += 3;
        cycles += 4;
        break;
    }
    case 0x15: // OR regs.A,(abs+regs.X)
    {
        SPC700_ResolveAddress(SPC700_AM_AbsX);
        uint8 d = SPCRead(addL);
        regs.A = regs.A | d;
        flags.Z = !(regs.A);
        flags.N = regs.A & 0x80;
        regs.PC += 3;
        cycles += 5;
        break;
    }
    case 0x16: // OR regs.A,(abs+regs.Y)
    {
        SPC700_ResolveAddress(SPC700_AM_AbsY);
        uint8 d = SPCRead(addL);
        regs.A = regs.A | d;
        flags.Z = !(regs.A);
        flags.N = regs.A & 0x80;
        regs.PC += 3;
        cycles += 5;
        break;
    }

    case 0x07: // OR regs.A,((dp+x))
    {
        SPC700_ResolveAddress(SPC700_AM_DPXPtr);
        uint8 d = SPCRead(addL);
        regs.A = regs.A | d;
        flags.Z = !(regs.A);
        flags.N = regs.A & 0x80;
        regs.PC += 2;
        cycles += 6;
        break;
    }
    case 0x17: // OR regs.A,((dp)+y)
    {
        SPC700_ResolveAddress(SPC700_AM_DPPtrY);
        uint8 d = SPCRead(addL);
        regs.A = regs.A | d;
        flags.Z = !(regs.A);
        flags.N = regs.A & 0x80;
        regs.PC += 2;
        cycles += 6;
        break;
    }

    case 0x19: // OR (x),(y)
    {

        SPC700_ResolveAddress(SPC700_AM_YPtr);
        uint8 y = SPCRead(addL);
        SPC700_ResolveAddress(SPC700_AM_XPtr);
        uint8 x = SPCRead(addL);
        uint8 t = x | y;
        flags.Z = !(t);
        flags.N = t & 0x80;
        SPCWrite(addL, t);
        regs.PC += 1;
        cycles += 5;
        break;
    }
    case 0x09: // OR dp,dp
    {

        SPC700_ResolveAddress(SPC700_AM_SrcDst);
        uint8 x = SPCRead(addL);
        uint8 y = SPCRead(addH);
        uint8 t = x | y;
        flags.Z = !(t);
        flags.N = t & 0x80;
        SPCWrite(addH, t);
        regs.PC += 3;
        cycles += 6;
        break;
    }
    case 0x18: // OR dp,imm
    {
        SPC700_ResolveAddress(SPC700_AM_SrcDst);
        uint8 x = addL;
        uint8 y = SPCRead(addH);
        uint8 t = x | y;
        flags.Z = !(t);
        flags.N = t & 0x80;
        SPCWrite(addH, t);
        regs.PC += 3;
        cycles += 5;
        break;
    }

    case 0x48: // EOR regs.A,imm
    {
        SPC700_ResolveAddress(SPC700_AM_Imm);
        uint8 d = SPCRead(addL);
        regs.A = regs.A ^ d;
        flags.Z = !(regs.A);
        flags.N = regs.A & 0x80;
        regs.PC += 2;
        cycles += 2;
        break;
    }
    case 0x46: // EOR regs.A,(regs.X)
    {
        SPC700_ResolveAddress(SPC700_AM_XPtr);
        uint8 d = SPCRead(addL);
        regs.A = regs.A ^ d;
        flags.Z = !(regs.A);
        flags.N = regs.A & 0x80;
        regs.PC += 1;
        cycles += 3;
        break;
    }
    case 0x44: // EOR regs.A,(dp)
    {
        SPC700_ResolveAddress(SPC700_AM_DP);
        uint8 d = SPCRead(addL);
        regs.A = regs.A ^ d;
        flags.Z = !(regs.A);
        flags.N = regs.A & 0x80;
        regs.PC += 2;
        cycles += 3;
        break;
    }
    case 0x54: // EOR regs.A,(dp+regs.X)
    {
        SPC700_ResolveAddress(SPC700_AM_DPX);
        uint8 d = SPCRead(addL);
        regs.A = regs.A ^ d;
        flags.Z = !(regs.A);
        flags.N = regs.A & 0x80;
        regs.PC += 2;
        cycles += 4;
        break;
    }
    case 0x45: // EOR regs.A,(abs)
    {
        SPC700_ResolveAddress(SPC700_AM_Abs);
        uint8 d = SPCRead(addL);
        regs.A = regs.A ^ d;
        flags.Z = !(regs.A);
        flags.N = regs.A & 0x80;
        regs.PC += 3;
        cycles += 4;
        break;
    }
    case 0x55: // EOR regs.A,(abs+regs.X)
    {
        SPC700_ResolveAddress(SPC700_AM_AbsX);
        uint8 d = SPCRead(addL);
        regs.A = regs.A ^ d;
        flags.Z = !(regs.A);
        flags.N = regs.A & 0x80;
        regs.PC += 3;
        cycles += 5;
        break;
    }
    case 0x56: // EOR regs.A,(abs+regs.Y)
    {
        SPC700_ResolveAddress(SPC700_AM_AbsY);
        uint8 d = SPCRead(addL);
        regs.A = regs.A ^ d;
        flags.Z = !(regs.A);
        flags.N = regs.A & 0x80;
        regs.PC += 3;
        cycles += 5;
        break;
    }

    case 0x47: // EOR regs.A,((dp+x))
    {
        SPC700_ResolveAddress(SPC700_AM_DPXPtr);
        uint8 d = SPCRead(addL);
        regs.A = regs.A ^ d;
        flags.Z = !(regs.A);
        flags.N = regs.A & 0x80;
        regs.PC += 2;
        cycles += 6;
        break;
    }
    case 0x57: // EOR regs.A,((dp)+y)
    {
        SPC700_ResolveAddress(SPC700_AM_DPPtrY);
        uint8 d = SPCRead(addL);
        regs.A = regs.A ^ d;
        flags.Z = !(regs.A);
        flags.N = regs.A & 0x80;
        regs.PC += 2;
        cycles += 6;
        break;
    }

    case 0x59: // EOR (x),(y)
    {

        SPC700_ResolveAddress(SPC700_AM_YPtr);
        uint8 y = SPCRead(addL);
        SPC700_ResolveAddress(SPC700_AM_XPtr);
        uint8 x = SPCRead(addL);
        uint8 t = x ^ y;
        flags.Z = !(t);
        flags.N = t & 0x80;
        SPCWrite(addL, t);
        regs.PC += 1;
        cycles += 5;
        break;
    }
    case 0x49: // EOR dp,dp
    {

        SPC700_ResolveAddress(SPC700_AM_SrcDst);
        uint8 x = SPCRead(addL);
        uint8 y = SPCRead(addH);
        uint8 t = x ^ y;
        flags.Z = !(t);
        flags.N = t & 0x80;
        SPCWrite(addH, t);
        regs.PC += 3;
        cycles += 6;
        break;
    }
    case 0x58: // EOR dp,imm
    {
        SPC700_ResolveAddress(SPC700_AM_SrcDst);
        uint8 x = addL;
        uint8 y = SPCRead(addH);
        uint8 t = x ^ y;
        flags.Z = !(t);
        flags.N = t & 0x80;
        SPCWrite(addH, t);
        regs.PC += 3;
        cycles += 5;
        break;
    }
#pragma endregion
#pragma region IncDec
    case 0xBC: // INC regs.A
    {
        regs.A++;
        flags.Z = !(regs.A);
        flags.N = regs.A & 0x80;
        regs.PC += 1;
        cycles += 2;
        break;
    }

    case 0xAB: // INC (dp)
    {
        SPC700_ResolveAddress(SPC700_AM_DP);
        uint8 t = SPCRead(addL);
        t++;
        flags.Z = !(t);
        flags.N = t & 0x80;
        SPCWrite(addL, t);
        regs.PC += 2;
        cycles += 4;
        break;
    }

    case 0xBB: // INC (dp+x)
    {
        SPC700_ResolveAddress(SPC700_AM_DPX);
        uint8 t = SPCRead(addL);
        t++;
        flags.Z = !(t);
        flags.N = t & 0x80;
        SPCWrite(addL, t);
        regs.PC += 2;
        cycles += 5;
        break;
    }

    case 0xAC: // INC (abs)
    {
        SPC700_ResolveAddress(SPC700_AM_Abs);
        uint8 t = SPCRead(addL);
        t++;
        flags.Z = !(t);
        flags.N = t & 0x80;
        SPCWrite(addL, t);
        regs.PC += 3;
        cycles += 5;
        break;
    }
    case 0x3D: // INC regs.X
    {
        regs.X++;
        flags.Z = !(regs.X);
        flags.N = regs.X & 0x80;
        regs.PC += 1;
        cycles += 2;
        break;
    }
    case 0xFC: // INC regs.Y
    {
        regs.Y++;
        flags.Z = !(regs.Y);
        flags.N = regs.Y & 0x80;
        regs.PC += 1;
        cycles += 2;
        break;
    }

    case 0x9C: // DEC regs.A
    {
        regs.A--;
        flags.Z = !(regs.A);
        flags.N = regs.A & 0x80;
        regs.PC += 1;
        cycles += 2;
        break;
    }

    case 0x8B: // DEC (dp)
    {
        SPC700_ResolveAddress(SPC700_AM_DP);
        uint8 t = SPCRead(addL);
        t--;
        flags.Z = !(t);
        flags.N = t & 0x80;
        SPCWrite(addL, t);
        regs.PC += 2;
        cycles += 4;
        break;
    }

    case 0x9B: // DEC (dp+x)
    {
        SPC700_ResolveAddress(SPC700_AM_DPX);
        uint8 t = SPCRead(addL);
        t--;
        flags.Z = !(t);
        flags.N = t & 0x80;
        SPCWrite(addL, t);
        regs.PC += 2;
        cycles += 5;
        break;
    }

    case 0x8C: // DEC (abs)
    {
        SPC700_ResolveAddress(SPC700_AM_Abs);
        uint8 t = SPCRead(addL);
        t--;
        flags.Z = !(t);
        flags.N = t & 0x80;
        SPCWrite(addL, t);
        regs.PC += 3;
        cycles += 5;
        break;
    }
    case 0x1D: // DEC regs.X
    {
        regs.X--;
        flags.Z = !(regs.X);
        flags.N = regs.X & 0x80;
        regs.PC += 1;
        cycles += 2;
        break;
    }
    case 0xDC: // DEC regs.Y
    {
        regs.Y--;
        flags.Z = !(regs.Y);
        flags.N = regs.Y & 0x80;
        regs.PC += 1;
        cycles += 2;
        break;
    }
#pragma endregion

#pragma region ShiftRotate
    case 0x1C: // ASL regs.A
    {
        flags.C = regs.A & 0x80;
        regs.A = regs.A << 1;
        flags.Z = !(regs.A);
        flags.N = regs.A & 0x80;
        regs.PC += 1;
        cycles += 2;
        break;
    }
    case 0x0B: // ASL (dp)
    {
        SPC700_ResolveAddress(SPC700_AM_DP);
        uint8 t = SPCRead(addL);
        flags.C = t & 0x80;
        t = t << 1;
        flags.Z = !(t);
        flags.N = t & 0x80;
        SPCWrite(addL, t);
        regs.PC += 2;
        cycles += 4;
        break;
    }
    case 0x1B: // ASL (dp+x)
    {
        SPC700_ResolveAddress(SPC700_AM_DPX);
        uint8 t = SPCRead(addL);
        flags.C = t & 0x80;
        t = t << 1;
        flags.Z = !(t);
        flags.N = t & 0x80;
        SPCWrite(addL, t);
        regs.PC += 2;
        cycles += 5;
        break;
    }

    case 0x0C: // ASL (abs)
    {
        SPC700_ResolveAddress(SPC700_AM_Abs);
        uint8 t = SPCRead(addL);
        flags.C = t & 0x80;
        t = t << 1;
        flags.Z = !(t);
        flags.N = t & 0x80;
        SPCWrite(addL, t);
        regs.PC += 3;
        cycles += 5;
        break;
    }

    case 0x5C: // LSR regs.A
    {
        flags.C = regs.A & 0x01;
        regs.A = regs.A >> 1;

        flags.Z = !(regs.A);
        flags.N = regs.A & 0x80;
        regs.PC += 1;
        cycles += 2;
        break;
    }
    case 0x4B: // LSR (dp)
    {
        SPC700_ResolveAddress(SPC700_AM_DP);
        uint8 t = SPCRead(addL);
        flags.C = t & 0x01;
        t = t >> 1;

        flags.Z = !(t);
        flags.N = t & 0x80;
        SPCWrite(addL, t);
        regs.PC += 2;
        cycles += 4;
        break;
    }
    case 0x5B: // LSR (dp+x)
    {
        SPC700_ResolveAddress(SPC700_AM_DPX);
        uint8 t = SPCRead(addL);
        flags.C = t & 0x01;
        t = t >> 1;

        flags.Z = !(t);
        flags.N = t & 0x80;
        SPCWrite(addL, t);
        regs.PC += 2;
        cycles += 5;
        break;
    }

    case 0x4C: // LSR (abs)
    {
        SPC700_ResolveAddress(SPC700_AM_Abs);
        uint8 t = SPCRead(addL);
        flags.C = t & 0x01;
        t = t >> 1;

        flags.Z = !(t);
        flags.N = t & 0x80;
        SPCWrite(addL, t);
        regs.PC += 3;
        cycles += 5;
        break;
    }

    case 0x3C: // ROL regs.A
    {
        bool c = flags.C;
        flags.C = regs.A & 0x80;
        regs.A = regs.A << 1;
        regs.A |= c ? 0x01 : 0x00;
        flags.Z = !(regs.A);
        flags.N = regs.A & 0x80;
        regs.PC += 1;
        cycles += 2;
        break;
    }
    case 0x2B: // ROL (dp)
    {
        SPC700_ResolveAddress(SPC700_AM_DP);
        uint8 t = SPCRead(addL);
        bool c = flags.C;
        flags.C = t & 0x80;
        t = t << 1;
        t |= c ? 0x01 : 0x00;
        flags.Z = !(t);
        flags.N = t & 0x80;
        SPCWrite(addL, t);
        regs.PC += 2;
        cycles += 4;
        break;
    }
    case 0x3B: // ROL (dp+x)
    {
        SPC700_ResolveAddress(SPC700_AM_DPX);
        uint8 t = SPCRead(addL);
        bool c = flags.C;
        flags.C = t & 0x80;
        t = t << 1;
        t |= c ? 0x01 : 0x00;
        flags.Z = !(t);
        flags.N = t & 0x80;
        SPCWrite(addL, t);
        regs.PC += 2;
        cycles += 5;
        break;
    }

    case 0x2C: // ROL (abs)
    {
        SPC700_ResolveAddress(SPC700_AM_Abs);
        uint8 t = SPCRead(addL);
        bool c = flags.C;
        flags.C = t & 0x80;
        t = t << 1;
        t |= c ? 0x01 : 0x00;
        flags.Z = !(t);
        flags.N = t & 0x80;
        SPCWrite(addL, t);
        regs.PC += 3;
        cycles += 5;
        break;
    }

    case 0x7C: // ROR regs.A
    {
        bool c = flags.C;
        flags.C = regs.A & 0x01;
        regs.A = regs.A >> 1;
        regs.A |= c ? 0x80 : 0x00;
        flags.Z = !(regs.A);
        flags.N = regs.A & 0x80;
        regs.PC += 1;
        cycles += 2;
        break;
    }
    case 0x6B: // ROR (dp)
    {
        SPC700_ResolveAddress(SPC700_AM_DP);
        uint8 t = SPCRead(addL);
        bool c = flags.C;
        flags.C = t & 0x01;
        t = t >> 1;
        t |= c ? 0x80 : 0x00;
        flags.Z = !(t);
        flags.N = t & 0x80;
        SPCWrite(addL, t);
        regs.PC += 2;
        cycles += 4;
        break;
    }
    case 0x7B: // ROR (dp+x)
    {
        SPC700_ResolveAddress(SPC700_AM_DPX);
        uint8 t = SPCRead(addL);
        bool c = flags.C;
        flags.C = t & 0x01;
        t = t >> 1;
        t |= c ? 0x80 : 0x00;
        flags.Z = !(t);
        flags.N = t & 0x80;
        SPCWrite(addL, t);
        regs.PC += 2;
        cycles += 5;
        break;
    }

    case 0x6C: // ROR (abs)
    {
        SPC700_ResolveAddress(SPC700_AM_Abs);
        uint8 t = SPCRead(addL);
        bool c = flags.C;
        flags.C = t & 0x01;
        t = t >> 1;
        t |= c ? 0x80 : 0x00;
        flags.Z = !(t);
        flags.N = t & 0x80;
        SPCWrite(addL, t);
        regs.PC += 3;
        cycles += 5;
        break;
    }

    case 0x9F: // XCN regs.A
    {
        regs.A = (regs.A >> 4) | (regs.A << 4);
        flags.Z = !(regs.A);
        flags.N = regs.A & 0x80;
        regs.PC += 1;
        cycles += 5;
        break;
    }
#pragma endregion

#pragma region BitOps16
    case 0xBA: // MOVW YA,(dp)
    {
        SPC700_ResolveAddress(SPC700_AM_DP);
        regs.A = SPCRead(addL);
        regs.Y = SPCRead(addH);
        flags.Z = !(regs.A | regs.Y);
        flags.N = regs.Y & 0x80;
        regs.PC += 2;
        cycles += 5;
        break;
    }
    case 0xDA: // MOVW (dp),YA
    {
        SPC700_ResolveAddress(SPC700_AM_DP);
        SPCWrite(addL, regs.A);
        SPCWrite(addH, regs.Y);

        regs.PC += 2;
        cycles += 4;
        break;
    }
    case 0x3A: // INCW (dp)
    {
        SPC700_ResolveAddress(SPC700_AM_DP);
        uint16 t = SPCRead(addL) | (SPCRead(addH) << 8);
        t++;
        SPCWrite(addL, t & 0x00FF);
        SPCWrite(addH, t >> 8);
        flags.Z = !t;
        flags.N = t & 0x8000;
        regs.PC += 2;
        cycles += 6;
        break;
    }
    case 0x1A: // DECW (dp)
    {
        SPC700_ResolveAddress(SPC700_AM_DP);
        uint16 t = SPCRead(addL) | (SPCRead(addH) << 8);
        t--;
        SPCWrite(addL, t & 0x00FF);
        SPCWrite(addH, t >> 8);
        flags.Z = !t;
        flags.N = t & 0x8000;
        regs.PC += 2;
        cycles += 6;
        break;
    }
    case 0x7A: // ADDW YA,(dp)
    {
        SPC700_ResolveAddress(SPC700_AM_DP);
        uint16 op1 = SPCRead(addL) | (SPCRead(addH) << 8);
        uint16 op2 = ((regs.Y << 8) | regs.A);
        uint32_t add = op1 + op2;
        flags.Z = !(add & 0x00FFFF);
        flags.N = add & 0x8000;

        flags.V = (((op1 ^ add) & (op2 ^ add) & 0x8000) != 0);
        flags.H = ((op1 & 0x0FFF) + (op2 & 0x0FFF)) > 0x0FFF;
        flags.C = (add > 0xFFFF);

        regs.A = add & 0x00FF;
        regs.Y = (add & 0xFF00) >> 8;

        regs.PC += 2;
        cycles += 5;
        break;
    }

    case 0x9A: // SUBW YA,(dp)
    {
        SPC700_ResolveAddress(SPC700_AM_DP);
        int op2 = SPCRead(addL) | (SPCRead(addH) << 8);
        int op1 = ((regs.Y << 8) | regs.A);
        int sub = op1 - op2;
        flags.Z = !(sub & 0x00FFFF);
        flags.N = sub & 0x8000;

        flags.V = (((op1 ^ op2) & (op1 ^ sub) & 0x8000) != 0);
        flags.H = ((op1 & 0x0FFF) - (op2 & 0x0FFF)) >= 0;
        flags.C = (sub >= 0);

        regs.A = sub & 0x00FF;
        regs.Y = (sub & 0xFF00) >> 8;

        regs.PC += 2;
        cycles += 5;
        break;
    }

    case 0x5A: // CMPW YA,(dp)
    {
        SPC700_ResolveAddress(SPC700_AM_DP);
        int op2 = SPCRead(addL) | (SPCRead(addH) << 8);
        int op1 = ((regs.Y << 8) | regs.A);
        int sub = op1 - op2;
        flags.Z = !(sub & 0x00FFFF);
        flags.N = sub & 0x8000;

        flags.C = (sub >= 0);

        regs.PC += 2;
        cycles += 4;
        break;
    }
#pragma endregion

#pragma region MulDiv
    case 0xCF: // MUL YA
    {
        uint16 mul = regs.A * regs.Y;
        regs.A = mul & 0x00FF;
        regs.Y = mul >> 8;
        // Yes, only regs.Y is checked for Z and N
        flags.Z = !(regs.Y);
        flags.N = regs.Y & 0x80;
        regs.PC += 1;
        cycles += 9;
        break;
    }
    case 0x9E: // DIV YA,regs.X
    {
        flags.H = (regs.X & 0x0F) <= (regs.Y & 0x0F);
        uint32_t D = (regs.A | (regs.Y << 8));
        for (int i = 1; i < 10; i++)
        {
            D = D << 1;
            if (D & 0x20000)
                D ^= 0x20001;
            if (D >= (regs.X * 0x200))
                D ^= 0x1;
            if (D & 0x1)
                D = (D - (regs.X * 0x200)) & 0x1FFFF;
        }

        regs.A = D & 0xFF;
        regs.Y = D / 0x200;

        flags.Z = !(regs.A);
        flags.N = regs.A & 0x80;

        flags.V = D & 0x100;

        regs.PC += 1;
        cycles += 12;
        break;
    }
#pragma endregion
#pragma region Misc
    case 0xDF: // DAA regs.A
    {

        uint16_t t = regs.A;
        if ((t & 0x0F) > 0x09 || flags.H)
        {
            t += 0x06;
        }

        if (t > 0x9F || flags.C)
        {
            t += 0x60;
            flags.C = 1;
        }

        regs.A = t & 0xFF;

        flags.Z = !(regs.A);
        flags.N = regs.A & 0x80;
        regs.PC += 1;
        cycles += 3;
        break;
    }
    case 0xBE: // DAS regs.A
    {
        bool sub = (regs.A > 0x99) || !flags.C;
        uint16_t t = regs.A;

        if ((t & 0x0F) > 0x09 || !flags.H)
        {
            t -= 0x06;
        }

        if (sub)
        {
            t -= 0x60;
            flags.C = 0;
        }

        regs.A = t & 0xFF;
        flags.Z = !(regs.A);
        flags.N = regs.A & 0x80;
        regs.PC += 1;
        cycles += 3;
        break;
    }
    case 0x00: // NOP
    {
        regs.PC += 1;
        cycles += 2;
        break;
    }
    case 0xef: // SLEEP
    {
        regs.PC += 1;
        cycles += 3;
        break;
    }
    case 0xff: // STOP
    {
        regs.PC += 1;
        cycles += 3;
        break;
    }

#pragma endregion
#pragma region Branch
    case 0x2F: // BRA rel
    {
        SPC700_ResolveAddress(SPC700_AM_Imm);
        int8_t t = SPCRead(addL);
        regs.PC = (regs.PC + 2) + t;
        cycles += 4;
        break;
    }

    case 0xF0: // BEQ rel
    {
        SPC700_ResolveAddress(SPC700_AM_Imm);
        int8_t t = SPCRead(addL);
        if (flags.Z)
        {
            regs.PC += t;
            cycles += 2;
        }
        regs.PC += 2;
        cycles += 2;
        break;
    }

    case 0xD0: // BNE rel
    {
        SPC700_ResolveAddress(SPC700_AM_Imm);
        int8_t t = SPCRead(addL);
        if (!flags.Z)
        {
            regs.PC += t;
            cycles += 2;
        }
        regs.PC += 2;
        cycles += 2;
        break;
    }

    case 0xB0: // BCS rel
    {
        SPC700_ResolveAddress(SPC700_AM_Imm);
        int8_t t = SPCRead(addL);
        if (flags.C)
        {
            regs.PC += t;
            cycles += 2;
        }
        regs.PC += 2;
        cycles += 2;
        break;
    }
    case 0x90: // BCC rel
    {
        SPC700_ResolveAddress(SPC700_AM_Imm);
        int8_t t = SPCRead(addL);
        if (!flags.C)
        {
            regs.PC += t;
            cycles += 2;
        }
        regs.PC += 2;
        cycles += 2;
        break;
    }
    case 0x70: // BVS rel
    {
        SPC700_ResolveAddress(SPC700_AM_Imm);
        int8_t t = SPCRead(addL);
        if (flags.V)
        {
            regs.PC += t;
            cycles += 2;
        }
        regs.PC += 2;
        cycles += 2;
        break;
    }
    case 0x50: // BVC rel
    {
        SPC700_ResolveAddress(SPC700_AM_Imm);
        int8_t t = SPCRead(addL);
        if (!flags.V)
        {
            regs.PC += t;
            cycles += 2;
        }
        regs.PC += 2;
        cycles += 2;
        break;
    }
    case 0x30: // BMI rel
    {
        SPC700_ResolveAddress(SPC700_AM_Imm);
        int8_t t = SPCRead(addL);
        if (flags.N)
        {
            regs.PC += t;
            cycles += 2;
        }
        regs.PC += 2;
        cycles += 2;
        break;
    }
    case 0x10: // BPL rel
    {
        SPC700_ResolveAddress(SPC700_AM_Imm);
        int8_t t = SPCRead(addL);
        if (!flags.N)
        {
            regs.PC += t;
            cycles += 2;
        }
        regs.PC += 2;
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
        SPC700_ResolveAddress(SPC700_AM_SrcDst);
        uint8 d = SPCRead(addL);
        int8_t t = addH;
        if (d & (1 << ((inst & 0b11100000) >> 5)))
        {
            regs.PC += t;
            cycles += 2;
        }
        regs.PC += 3;
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
        SPC700_ResolveAddress(SPC700_AM_SrcDst);
        uint8 d = SPCRead(addL);
        int8_t t = addH;
        if (!(d & (1 << ((inst & 0b11100000) >> 5))))
        {
            regs.PC += t;
            cycles += 2;
        }
        regs.PC += 3;
        cycles += 5;
        break;
    }

    case 0x2E: // CBNE dp,rel
    {
        SPC700_ResolveAddress(SPC700_AM_SrcDst);
        uint8 d = SPCRead(addL);
        int8_t t = addH;
        if (d != regs.A)
        {
            regs.PC += t;
            cycles += 2;
        }
        regs.PC += 3;
        cycles += 5;
        break;
    }
    case 0xDE: // CBNE (dp+x),rel
    {
        SPC700_ResolveAddress(SPC700_AM_DPX);
        uint8 d = SPCRead(addL);
        int8_t t = addH;
        if (d != regs.A)
        {
            regs.PC += t;
            cycles += 2;
        }
        regs.PC += 3;
        cycles += 6;
        break;
    }
    case 0x6E: // DBNZ dp,rel
    {
        SPC700_ResolveAddress(SPC700_AM_SrcDst);
        uint8 d = SPCRead(addL);
        int8_t t = addH;
        d--;
        SPCWrite(addL, d);
        if (d)
        {
            regs.PC += t;
            cycles += 2;
        }
        regs.PC += 3;
        cycles += 5;
        break;
    }
    case 0xFE: // DBNZ regs.Y,rel
    {
        SPC700_ResolveAddress(SPC700_AM_Imm);
        int8_t t = SPCRead(addL);
        regs.Y--;
        if (regs.Y)
        {
            regs.PC += t;
            cycles += 2;
        }
        regs.PC += 2;
        cycles += 4;
        break;
    }

#pragma endregion
#pragma region CallsAndJumps
    case 0x5F: // JMP abs
    {
        SPC700_ResolveAddress(SPC700_AM_Abs);

        regs.PC = addL;
        cycles += 3;
        break;
    }
    case 0x1F: // JMP (abs+x)
    {
        SPC700_ResolveAddress(SPC700_AM_AbsX);
        uint16 t = SPCRead(addL) | (SPCRead(addH) << 8);
        regs.PC = t;
        cycles += 6;
        break;
    }

    case 0x3F: // CALL abs
    {
        SPC700_ResolveAddress(SPC700_AM_Abs);
        regs.PC += 3;
        SPC700_Push(regs.PC >> 8);
        SPC700_Push(regs.PC & 0x00FF);
        regs.PC = addL;
        cycles += 8;
        break;
    }

    case 0x4F: // PCALL upage
    {
        SPC700_ResolveAddress(SPC700_AM_Imm);
        regs.PC += 2;
        SPC700_Push(regs.PC >> 8);
        SPC700_Push(regs.PC & 0x00FF);
        regs.PC = 0XFF00 | SPCRead(addL);
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
        regs.PC += 1;
        SPC700_Push(regs.PC >> 8);
        SPC700_Push(regs.PC & 0x00FF);
        regs.PC = SPCRead(ta) | (SPCRead(ta + 1) << 8);
        cycles += 8;
        break;
    }

    case 0x0F: // BRK
    {

        regs.PC += 1;
        SPC700_Push(regs.PC >> 8);
        SPC700_Push(regs.PC & 0x00FF);
        SPC700_Push(SPC700_GetFlagsAsByte());
        flags.B = 1;
        flags.I = 0;
        regs.PC = SPCRead(0xFFDE) | (SPCRead(0xFFDF) << 8);
        cycles += 8;
        break;
    }

    case 0x6F: // RET
    {
        regs.PC = SPC700_Pop();
        regs.PC |= SPC700_Pop() << 8;
        cycles += 5;
        break;
    }
    case 0x7F: // RETI
    {
        SPC700_SetFlagsAsByte(SPC700_Pop());
        regs.PC = SPC700_Pop();
        regs.PC |= SPC700_Pop() << 8;
        cycles += 6;
        break;
    }
#pragma endregion

#pragma region StackOps
    case 0x2D: // PUSH regs.A
    {
        SPC700_Push(regs.A);
        regs.PC += 1;
        cycles += 4;
        break;
    }
    case 0x4D: // PUSH regs.X
    {
        SPC700_Push(regs.X);
        regs.PC += 1;
        cycles += 4;
        break;
    }
    case 0x6D: // PUSH regs.Y
    {
        SPC700_Push(regs.Y);
        regs.PC += 1;
        cycles += 4;
        break;
    }
    case 0x0D: // PUSH flags
    {
        SPC700_Push(SPC700_GetFlagsAsByte());
        regs.PC += 1;
        cycles += 4;
        break;
    }

    case 0xAE: // POP regs.A
    {
        regs.A = SPC700_Pop();
        regs.PC += 1;
        cycles += 4;
        break;
    }
    case 0xCE: // POP regs.X
    {
        regs.X = SPC700_Pop();
        regs.PC += 1;
        cycles += 4;
        break;
    }
    case 0xEE: // POP regs.Y
    {
        regs.Y = SPC700_Pop();
        regs.PC += 1;
        cycles += 4;
        break;
    }
    case 0x8E: // POP flags
    {
        SPC700_SetFlagsAsByte(SPC700_Pop());
        regs.PC += 1;
        cycles += 4;
        break;
    }
#pragma endregion

#pragma region FlagOps
    case 0x60: // CLRC
    {
        flags.C = 0;
        regs.PC += 1;
        cycles += 2;
        break;
    }
    case 0x80: // SETC
    {
        flags.C = 1;
        regs.PC += 1;
        cycles += 2;
        break;
    }

    case 0xED: // NOTC
    {
        flags.C = !flags.C;
        regs.PC += 1;
        cycles += 3;
        break;
    }

    case 0xE0: // CLRC
    {
        flags.V = 0;
        flags.H = 0;
        regs.PC += 1;
        cycles += 2;
        break;
    }

    case 0x20: // CLRP
    {
        flags.P = 0;
        regs.PC += 1;
        cycles += 2;
        break;
    }

    case 0x40: // SETP
    {
        flags.P = 1;
        regs.PC += 1;
        cycles += 2;
        break;
    }

    case 0xA0: // EI
    {
        flags.I = 1;
        regs.PC += 1;
        cycles += 3;
        break;
    }

    case 0xC0: // DI
    {
        flags.I = 0;
        regs.PC += 1;
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
        SPC700_ResolveAddress(SPC700_AM_DP);
        uint8 d = SPCRead(addL);
        d |= 1 << n;
        SPCWrite(addL, d);
        regs.PC += 2;
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
        SPC700_ResolveAddress(SPC700_AM_DP);
        uint8 d = SPCRead(addL);
        d &= ~(1 << n);
        SPCWrite(addL, d);
        regs.PC += 2;
        cycles += 4;
        break;
    }

    case 0x0e: // TSET abs
    {
        SPC700_ResolveAddress(SPC700_AM_Abs);
        uint8 d = SPCRead(addL);
        uint8 t = regs.A - d;
        d |= regs.A;
        SPCWrite(addL, d);
        flags.Z = !(t);
        flags.N = t & 0x80;
        regs.PC += 3;
        cycles += 6;
        break;
    }

    case 0x4e: // TCLR abs
    {
        SPC700_ResolveAddress(SPC700_AM_Abs);
        uint8 d = SPCRead(addL);
        uint8 t = regs.A - d;
        d &= ~regs.A;
        SPCWrite(addL, d);
        flags.Z = !(t);
        flags.N = t & 0x80;
        regs.PC += 3;
        cycles += 6;
        break;
    }

    case 0x4a: // AND C,mem.bit
    {
        SPC700_ResolveAddress(SPC700_AM_MemBit);
        bool d = SPCRead(addL) & (1 << addH);
        flags.C = flags.C && d;
        regs.PC += 3;
        cycles += 4;
        break;
    }

    case 0x6a: // AND C,/mem.bit
    {
        SPC700_ResolveAddress(SPC700_AM_MemBit);
        bool d = ~SPCRead(addL) & (1 << addH);
        flags.C = flags.C && d;
        regs.PC += 3;
        cycles += 4;
        break;
    }

    case 0x0a: // OR C,mem.bit
    {
        SPC700_ResolveAddress(SPC700_AM_MemBit);
        bool d = SPCRead(addL) & (1 << addH);
        flags.C = flags.C || d;
        regs.PC += 3;
        cycles += 5;
        break;
    }

    case 0x2a: // OR C,/mem.bit
    {
        SPC700_ResolveAddress(SPC700_AM_MemBit);
        bool d = ~SPCRead(addL) & (1 << addH);
        flags.C = flags.C || d;
        regs.PC += 3;
        cycles += 5;
        break;
    }

    case 0x8a: // EOR C,mem.bit
    {
        SPC700_ResolveAddress(SPC700_AM_MemBit);
        bool d = SPCRead(addL) & (1 << addH);
        flags.C = flags.C ^ d;
        regs.PC += 3;
        cycles += 5;
        break;
    }

    case 0xea: // NOT mem.bit
    {
        SPC700_ResolveAddress(SPC700_AM_MemBit);
        uint8 d = SPCRead(addL) ^ (1 << addH);
        SPCWrite(addL, d);
        regs.PC += 3;
        cycles += 5;
        break;
    }

    case 0xaa: // MOV C,mem.bit
    {
        SPC700_ResolveAddress(SPC700_AM_MemBit);
        flags.C = SPCRead(addL) & (1 << addH);
        regs.PC += 3;
        cycles += 4;
        break;
    }

    case 0xca: // MOV mem.bit,C
    {
        SPC700_ResolveAddress(SPC700_AM_MemBit);
        uint8 d = SPCRead(addL) & (~(1 << addH));
        d = d | ((flags.C << addH));
        SPCWrite(addL, d);
        regs.PC += 3;
        cycles += 6;
        break;
    }

#pragma endregion

    default:
        break;
    }
}
