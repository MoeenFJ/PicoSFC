#include "stdint.h"


typedef unsigned char uint8;
typedef unsigned short uint16;

const uint8_t spc700_FlagNMask = 0b10000000;
const uint8_t spc700_FlagVMask = 0b01000000;
const uint8_t spc700_FlagPMask = 0b00100000;
const uint8_t spc700_FlagBMask = 0b00010000;
const uint8_t spc700_FlagHMask = 0b00001000;
const uint8_t spc700_FlagIMask = 0b00000100;
const uint8_t spc700_FlagZMask = 0b00000010;
const uint8_t spc700_FlagCMask = 0b00000001;

extern void SPCWrite(uint16 address, uint8 data);
extern uint8 SPCRead(uint16 address);

typedef enum
{
    SPC700_AM_Imm = 0,
    SPC700_AM_Abs = 1,
    SPC700_AM_AbsX = 2,
    SPC700_AM_AbsY = 3,
    SPC700_AM_XPtr = 11,
    SPC700_AM_YPtr = 12,
    SPC700_AM_DP = 4,
    SPC700_AM_DPX = 5,
    SPC700_AM_DPY = 6,
    SPC700_AM_DPPtrY = 7,
    SPC700_AM_DPXPtr = 8,
    SPC700_AM_STK = 9,
    SPC700_AM_SrcDst = 10,
    SPC700_AM_MemBit = 13,
} SPC700AddressingMode;

typedef struct 
{
    bool N;
    bool V;
    bool P;
    bool B;
    bool H;
    bool I;
    bool Z;
    bool C;
} SPC700Flags;
void SPC700_SetFlagsAsByte(uint8 bits);
uint8 SPC700_GetFlagsAsByte();

typedef struct 
{
    uint8 A;
    uint8 X;
    uint8 Y;
    uint16 PC;
    uint8 SP;
}SPC700Regs ;

extern SPC700Flags flags;
extern SPC700Regs regs;
extern uint16 cycles;

void SPC700_Reset();
void SPC700_Step();


