#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <MiniFB.h>

// const int SCALE = 3;
// const int WIN_WIDTH = 256 * SCALE;
// const int WIN_HEIGHT = 256 * SCALE;
#define SCALE 3
#define WIN_WIDTH 256*SCALE
#define WIN_HEIGHT 256*SCALE

// const int FB_WIDTH = 256;
// const int FB_HEIGHT = 224;
#define FB_WIDTH 256
#define FB_HEIGHT 224

typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int add24;
// typedef unsigned char bool;

void WriteBus(add24 address, uint8 data);
uint8 ReadBus(add24 address);

uint8 SPCRead(uint16 address);
void SPCWrite(uint16 address, uint8 data);

#pragma region System_Vars
// System Variables
bool cpuVBlankIntLatch = false;
bool cpuHBlankIntLatch = false;
bool vBlankEntryMoment = false;
bool hdmaRan = true;
unsigned int vblanktimer = 1;

uint64_t emuStep = 0;

// System Regs
uint8 zeroWire = 0;
uint8 ram[128 * 1024] = {0};
uint8 NMITIMEN; // 0x4200
uint8 HTIMEL;   // 0x4207
uint8 HTIMEH;   // 0x4208
uint8 VTIMEL;   // 0x4209
uint8 VTIMEH;   // 0x420a
uint8 TIMEUP;   // 0x4211
uint8 MEMSEL;   // 0x420D
uint8 RDNMI;
uint8 WMDATA; // 0x2180
uint8 WMADDL; // 0x2181
uint8 WMADDM; // 0x2182
uint8 WMADDH; // 0x2183

// GUI

struct mfb_window *window;
uint32_t window_fb[WIN_WIDTH * WIN_HEIGHT];
#pragma endregion

#pragma region APU_Vars
// APU
uint8 aram[64 * 1024] = {0};
uint8 arom[] = {
    0xCD, 0xEF,
    0xBD,
    0xE8, 0x00,
    0xC6,
    0x1D,
    0xD0, 0xFC,
    0x8F, 0xAA, 0xF4,
    0x8F, 0xBB, 0xF5,
    0x78, 0xCC, 0xF4,
    0xD0, 0xFB,
    0x2F, 0x19,
    0xEB, 0xF4,
    0xD0, 0xFC,
    0x7E, 0xF4,
    0xD0, 0x0B,
    0xE4, 0xF5,
    0xCB, 0xF4,
    0xD7, 0x00,
    0xFC,
    0xD0, 0xF3,
    0xAB, 0x01,
    0x10, 0xEF,
    0x7E, 0xF4,
    0x10, 0xEB,
    0xBA, 0xF6,
    0xDA, 0x00,
    0xBA, 0xF4,
    0xC4, 0XF4,
    0xDD,
    0x5D,
    0xD0, 0xDB,
    0x1F, 0x00, 0x00,
    0xC0, 0xFF};

uint8 APUIO0;
uint8 APUIO1;
uint8 APUIO2;
uint8 APUIO3;

uint8 CPUIO0;
uint8 CPUIO1;
uint8 CPUIO2;
uint8 CPUIO3;

// Timers
bool t0En = 0;
bool t1En = 0;
bool t2En = 0;

uint8 t0Val = 0;
uint8 t1Val = 0;
uint8 t2Val = 0;

uint16 t0div = 1;
uint16 t1div = 1;
uint16 t2div = 1;

bool ramRomSel = 1;

uint32_t stepCnt = 0;
uint16 t0Clk = 0;
uint16 t1Clk = 0;
uint16 t2Clk = 0;
#pragma endregion

#pragma region ROM_Vars
typedef enum
{
    LoROM,
    HiROM,
    ExHiROM
} ROMMapType;

unsigned char *rom;
unsigned char *sram;
bool hasSram = 0;
int romSize;
int sramSize = 0;
ROMMapType romMapType;
char title[21];
bool speed; // 0=Slow , 1=Fast
#pragma endregion

#pragma region CPU_Vars
const uint8_t flagNMask = 0b10000000;
const uint8_t flagVMask = 0b01000000;
const uint8_t flagMMask = 0b00100000;
const uint8_t flagXMask = 0b00010000;
const uint8_t flagDMask = 0b00001000;
const uint8_t flagIMask = 0b00000100;
const uint8_t flagZMask = 0b00000010;
const uint8_t flagCMask = 0b00000001;
typedef struct
{
    bool N;
    bool V;
    bool M;
    bool X;
    bool D;
    bool I;
    bool Z;
    bool C;
} CPUFlagsRegister;

typedef struct
{
    uint16 C;  // accumulator 16 bit
    uint8 DBR; // data bank register 8 bit
    uint16 D;  // Direct 16 bit
    uint8 K;   // Program Bank 8 bit
    uint16 PC; // PC 16 bit
    uint16 S;  // Stack 16 bit
    uint16 X;  // X 16 bit
    uint16 Y;  // Y 16 bit
} CPURegisters;

typedef enum
{
    CPU_AM_Abs = 0,
    CPU_AM_AbsX = 1,
    CPU_AM_AbsY = 2,
    CPU_AM_AbsPtr16 = 3,
    CPU_AM_AbsPtr24 = 4,
    CPU_AM_AbsXPtr16 = 5,
    CPU_AM_Dir = 6,
    CPU_AM_DirX = 7,
    CPU_AM_DirY = 8,
    CPU_AM_DirPtr16 = 9,
    CPU_AM_DirPtr24 = 10,
    CPU_AM_DirXPtr16 = 11,
    CPU_AM_DirPtr16Y = 12,
    CPU_AM_DirPtr24Y = 13,
    CPU_AM_Imm = 14,
    CPU_AM_Long = 15,
    CPU_AM_LongX = 16,
    CPU_AM_Rel8 = 17,
    CPU_AM_Rel16 = 18,
    CPU_AM_SrcDst = 19,
    CPU_AM_STK = 20,
    CPU_AM_STKPtr16Y = 21
} CPUAddressingMode;

CPUFlagsRegister flags; // Status Reg
CPURegisters cregs;
bool flagE;
uint8_t inst;
add24 addL, addH, addXH;
#pragma endregion

#pragma region CTRL_Vars
typedef struct
{
    uint8 LOW;
    uint8 HIGH;
} ControllerRegs;
ControllerRegs ctrl[4];
uint8 LCTRLREG1;
uint8 LCTRLREG2;
uint8 HVBJOY;
uint8 RDIO;
uint8 WRIO;
uint16 player1;
bool stdCtrlEn = 1;

uint8 legCnt1 = 0;
#pragma endregion

#pragma region PPU_Vars



typedef struct
{
    bool objVisible;
    bool size;
    uint8 objY;
    int16_t objX;
    uint8 objChrName;
    bool objChrTable;
    uint8 objClrPal;
    uint8 objPrior;
    bool objHF;
    bool objVF;
} PPUObject;

uint32_t fb[FB_WIDTH * FB_HEIGHT];

// VRam
uint16 vram[32 * 1024] = {0};
uint16 vramAddr = 0;
uint16 vramPtr = 0;

// CGRam
uint16 cgram[256] = {0};
bool cgDataHL = false;
uint8 cgAddr = 0;

// OAM , Object data
uint16 oam[272] = {0};
uint16 oamAddr = 0;
bool oamDataHL = false;
PPUObject objs[128];

// Counters
uint16 hCounter = 0;
uint16 vCounter = 0;
bool hCounterLatchHL = false;
bool vCounterLatchHL = false;
uint16 hCounterLatch = 0;
uint16 vCounterLatch = 0;

// BGs and Mode
uint8 mode;
bool bg3PriorMode;
bool bg1ChrSize;
bool bg2ChrSize;
bool bg3ChrSize;
bool bg4ChrSize;
bool directColor;

uint16 BG1BaseAddr;
uint16 BG2BaseAddr;
uint16 BG3BaseAddr;
uint16 BG4BaseAddr;
uint16 BG1ChrsBaseAddr;
uint16 BG2ChrsBaseAddr;
uint16 BG3ChrsBaseAddr;
uint16 BG4ChrsBaseAddr;

bool BG1onMainScreen = false;
bool BG2onMainScreen = false;
bool BG3onMainScreen = false;
bool BG4onMainScreen = false;
bool OBJonMainScreen = false;
bool BG1onSubScreen = false;
bool BG2onSubScreen = false;
bool BG3onSubScreen = false;
bool BG4onSubScreen = false;
bool OBJonSubScreen = false;

uint8 BG1SCSize = 0;
uint8 BG2SCSize = 0;
uint8 BG3SCSize = 0;
uint8 BG4SCSize = 0;
uint8 objAvailSize = 0;

bool BG1HScrollHL = false;
bool BG1VScrollHL = false;
uint16 BG1HScroll = 0;
uint16 BG1VScroll = 0;

bool BG2HScrollHL = false;
bool BG2VScrollHL = false;
uint16 BG2HScroll = 0;
uint16 BG2VScroll = 0;

bool BG3HScrollHL = false;
bool BG3VScrollHL = false;
uint16 BG3HScroll = 0;
uint16 BG3VScroll = 0;

bool BG4HScrollHL = false;
bool BG4VScrollHL = false;
uint16 BG4HScroll = 0;
uint16 BG4VScroll = 0;

// Mode 7
uint16 mode7A = 0;
uint16 mode7B = 0;
uint16 mode7C = 0;
uint16 mode7D = 0;
uint16 centerX = 0;
uint16 centerY = 0;
uint8 mode7Old = 0;
int32_t mult = 0;
uint8 M7SEL;

bool mode7AHL = false;
bool mode7BHL = false;
bool mode7CHL = false;
bool mode7DHL = false;
bool centerXHL = false;
bool centerYHL = false;

// Frame
bool hBlank = false, vBlank = false;
int frameCount = 0;
bool vBlankInt = false;
bool interlacing = false;
uint8 fadeValue = 0xFF;
bool forceBlank = false;

bool addSub = false;
bool addSubHalf = false;
bool bg1AddSub = false;
bool bg2AddSub = false;
bool bg3AddSub = false;
bool bg4AddSub = false;
bool objAddSub = false;
bool bdpAddSub = false;

uint16 constColor = 0;
bool constColorSel = 0;

// Objs
uint16 OBJChrTable1BaseAddr;
uint16 OBJChrTable2BaseAddr;

// Debug
uint8 bgFilter = 0;
bool pauseEmu = false;

uint8 VMAINC = 0;
uint8 OPHCT = 0;
uint8 OPVCT = 0;
uint8 MOSAIC = 0;

// Window
uint8 TMW;
uint8 TSW;
uint8 W12SEL;
uint8 W34SEL;
uint8 WOBJSEL;
uint8 WH0;
uint8 WH1;
uint8 WH2;
uint8 WH3;
uint8 WBGLOG;
uint8 WOBJLOG;
#pragma endregion

#pragma region SPC700_Vars


const uint8_t spc700_FlagNMask = 0b10000000;
const uint8_t spc700_FlagVMask = 0b01000000;
const uint8_t spc700_FlagPMask = 0b00100000;
const uint8_t spc700_FlagBMask = 0b00010000;
const uint8_t spc700_FlagHMask = 0b00001000;
const uint8_t spc700_FlagIMask = 0b00000100;
const uint8_t spc700_FlagZMask = 0b00000010;
const uint8_t spc700_FlagCMask = 0b00000001;
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

typedef struct
{
    uint8 A;
    uint8 X;
    uint8 Y;
    uint16 PC;
    uint8 SP;
} SPC700Regs;

SPC700Flags SPC_flags;
SPC700Regs SPC_regs;
uint16 SPC_cycles;
uint16 SPC_addL;
uint16 SPC_addH;

#pragma endregion

#pragma region DMA_Vars
uint8 MDMAEN; // What channels to start immediately, each flag is cleared after finishing
uint8 HDMAEN; // What channels to start after HBlank
bool dmaActive = false;

uint16 dmaAAddr[8] = {0};
uint8 dmaAAddrBank[8] = {0};
add24 dmaBAddr[8] = {0};
uint16 dmaDataPtr[8] = {0};
uint8 dmaIndirBank[8] = {0};
uint16 dmaTablePtr[8] = {0};
uint8 dmaUnused[8] = {0};
bool dmaDir[8] = {0};
bool dmaFixedAddr[8] = {0};
bool dmaDecAddr[8] = {0};
uint8 dmaType[8] = {0};
bool dmaIndir[8] = {0};
bool dmaEn[8] = {0};
bool hdmaEn[8] = {0};
bool dmaCont[8] = {0};
uint8 dmaLineCnt[8] = {0};
bool hdmaDoTransfer[8] = {0};

uint8 state = 0;
#pragma endregion

#pragma region MDU_Vars
 uint8 WRMPYA; // 0X4202
    uint8 WRMPYB; // 0X4203
    uint8 WRDIVL; // 0X4204
    uint8 WRDIVH; // 0X4205
    uint8 WRDIVB; // 0X4206

    uint8 RDDIVL; // 0x4214
    uint8 RDDIVH; // 0x4215
    uint8 RDMPYL; // 0x4216
    uint8 RDMPYH; // 0x4217
#pragma endregion

void exitEmu()
{

    mfb_close(window);
    exit(0);
}


#pragma region CPU
add24 PCByteAhead(uint8 n)
{
    return ((add24)cregs.K << 16) | (((add24)cregs.PC + n) & 0x00FFFF);
}


uint16 ReadWord()
{
    uint16 low = ReadBus(addL);
    uint16 high = ReadBus(addH);
    return (high << 8) | low;
}
void WriteWord(uint16 data)
{
    WriteBus(addL, data & 0x00FF);
    WriteBus(addH, data >> 8);
}

uint16 ReadWordAt(add24 add)
{
    uint16 low = ReadBus(add);
    uint16 high = ReadBus(add + 1);
    return (high << 8) | low;
}
void WriteWordAt(add24 add, uint16 data)
{
    WriteBus(add, data & 0x00FF);
    WriteBus(add + 1, data >> 8);
}

void ResolveAddress(CPUAddressingMode mode)
{
    uint8 ll, hh, xh;
    ll = ReadBus(PCByteAhead(1));
    hh = ReadBus(PCByteAhead(2));
    xh = ReadBus(PCByteAhead(3));

    // Phase one
    switch (mode)
    {
    case CPU_AM_Rel8:
    case CPU_AM_Rel16:
    case CPU_AM_Imm:
        addL = PCByteAhead(1);
        addH = PCByteAhead(2);
        break;

    case CPU_AM_AbsPtr16:
    case CPU_AM_AbsPtr24:
        addL = (hh << 8) | ll;
        addH = addL + 1;
        addH &= 0x00FFFF;
        addXH = addL + 2;
        addXH &= 0x00FFFF;
        break;

    case CPU_AM_Abs:
        addL = (cregs.DBR << 16) | (hh << 8) | ll;
        addH = addL + 1;
        addXH = addL + 2;
        break;

    case CPU_AM_AbsXPtr16:
        addL = (hh << 8) | ll;
        addL += cregs.X;

        addH = addL + 1;

        addL &= 0x00FFFF;
        addH &= 0x00FFFF;
        addL |= cregs.K << 16;
        addH |= cregs.K << 16;
        break;

    case CPU_AM_AbsX:
        addL = (cregs.DBR << 16) | (hh << 8) | ll;
        addL += cregs.X;
        addH = addL + 1;
        addXH = addL + 2;
        break;

    case CPU_AM_AbsY:
        addL = (cregs.DBR << 16) | (hh << 8) | ll;
        addL += cregs.Y;
        addH = addL + 1;
        addXH = addL + 2;
        break;

    case CPU_AM_DirPtr16:
    case CPU_AM_DirPtr24:
    case CPU_AM_DirPtr24Y:
    case CPU_AM_DirPtr16Y:
    case CPU_AM_Dir:
        addL = (cregs.D + ll) & 0x00FFFF;
        addH = (cregs.D + ll + 1) & 0x00FFFF;
        addXH = (cregs.D + ll + 2) & 0x00FFFF;
        break;

    case CPU_AM_DirXPtr16:
    case CPU_AM_DirX:
        addL = (cregs.D + ll + cregs.X) & 0x00FFFF;
        addH = (cregs.D + ll + cregs.X + 1) & 0x00FFFF;
        addXH = (cregs.D + ll + cregs.X + 2) & 0x00FFFF;
        break;

    case CPU_AM_DirY:
        addL = (cregs.D + ll + cregs.Y) & 0x00FFFF;
        addH = (cregs.D + ll + cregs.Y + 1) & 0x00FFFF;
        addXH = (cregs.D + ll + cregs.Y + 2) & 0x00FFFF;
        break;

    case CPU_AM_Long:
        addL = (xh << 16) | (hh << 8) | (ll);
        addH = addL + 1;
        break;

    case CPU_AM_LongX:
        addL = (xh << 16) | (hh << 8) | (ll);
        addL += cregs.X;
        addH = addL + 1;
        break;

    case CPU_AM_STKPtr16Y:
    case CPU_AM_STK:
        addL = (ll + cregs.S) & 0x00FFFF;
        addH = (ll + cregs.S + 1) & 0x00FFFF;
        break;
    default:
        break;
    }

    switch (mode)
    {
    case CPU_AM_AbsXPtr16:
    case CPU_AM_AbsPtr16:
        addL = (cregs.K << 16) | (ReadBus(addH) << 8) | ReadBus(addL);
        break;

    case CPU_AM_DirPtr16:
    case CPU_AM_DirXPtr16:
    case CPU_AM_DirPtr16Y:
    case CPU_AM_STKPtr16Y:
        addL = (cregs.DBR << 16) | (ReadBus(addH) << 8) | ReadBus(addL);
        addH = addL + 1;
        break;

    case CPU_AM_AbsPtr24:
    case CPU_AM_DirPtr24:
    case CPU_AM_DirPtr24Y:
        addL = (ReadBus(addXH) << 16) | (ReadBus(addH) << 8) | ReadBus(addL);
        addH = addL + 1;

    default:
        break;
    }

    switch (mode)
    {
    case CPU_AM_DirPtr16Y:
    case CPU_AM_DirPtr24Y:
    case CPU_AM_STKPtr16Y:
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
        WriteBus(addL, d);
    else
        WriteWord(d);
}

void WriteX(uint16 d)
{
    if (flags.X)
        WriteBus(addL, d);
    else
        WriteWord(d);
}

uint16 ReadM()
{
    if (flags.M)
        return ReadBus(addL);
    else
        return ReadWord();
}

uint16 ReadX()
{
    if (flags.X)
        return ReadBus(addL);
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
    WriteBus(cregs.S, d);
    cregs.S -= 1;
}
void PushWord(uint16 d)
{
    cregs.S -= 1;
    WriteBus(cregs.S, d & 0x00ff);
    WriteBus(cregs.S + 1, (d & 0xff00) >> 8);
    cregs.S -= 1;
}
uint8 Pop()
{
    cregs.S += 1;
    return ReadBus(cregs.S);
}
uint16 PopWord()
{
    cregs.S += 1;
    uint16 val = ReadBus(cregs.S + 1) << 8 | ReadBus(cregs.S);
    cregs.S += 1;
    return val;
}

void CPU_Reset()
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
    inst = ReadBus(((add24)cregs.K << 16) + (add24)cregs.PC);
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
    inst = ReadBus(((add24)cregs.K << 16) + (add24)cregs.PC);
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
    inst = ReadBus(((add24)cregs.K << 16) + (add24)cregs.PC);
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
// void printStatus()
// {
//     // nvmxdizc
//
//
//
//
//
//
//
//
//
//
//
//
//
//
//     add24 instAddress = ((add24)cregs.K << 16) + (add24)cregs.PC;

//
//     uint8_t inst = ReadBus(instAddress);
//
//
// }

void CPU_Step()
{
    inst = ReadBus(((add24)cregs.K << 16) + (add24)cregs.PC);

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
        ResolveAddress(CPU_AM_Imm);
        uint8 imm = ReadBus(addL);

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
        ResolveAddress(CPU_AM_Imm);
        uint8 imm = ReadBus(addL);

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
        //
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

        //
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
        //

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

        ResolveAddress(CPU_AM_Imm);
        PushWord(ReadWord());
        cregs.PC += 3;
        break;
    }
    case 0xd4: // PEI
    {
        // TODO : exception in address resolve
        ResolveAddress(CPU_AM_Dir);
        PushWord(ReadWord()); // Or ReadM?
        cregs.PC += 2;
        break;
    }
    case 0x62: // PER
    {
        ResolveAddress(CPU_AM_Imm);
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
        ResolveAddress(CPU_AM_DirXPtr16);
        uint16 d = ReadM();
        doADC(d);
        cregs.PC += 2;
        break;
    }
    case 0x63: // ADC stk,S
    {
        // TODO : BCD for D flag
        ResolveAddress(CPU_AM_STK);
        uint16 d = ReadM();
        doADC(d);
        cregs.PC += 2;
        break;
    }
    case 0x65: // ADC dir
    {
        // TODO : BCD for D flag
        ResolveAddress(CPU_AM_Dir);
        uint16 d = ReadM();
        doADC(d);
        cregs.PC += 2;
        break;
    }
    case 0x67: // ADC [dir]
    {
        // TODO : BCD for D flag
        ResolveAddress(CPU_AM_DirPtr24);
        uint16 d = ReadM();
        doADC(d);
        cregs.PC += 2;
        break;
    }
    case 0x69: // ADC imm
    {
        // TODO : BCD for D flag
        // cin.get();
        ResolveAddress(CPU_AM_Imm);

        uint16 imm = ReadM();
        doADC(imm);

        cregs.PC += 3 - flags.M;

        break;
    }
    case 0x6d: // ADC abs
    {
        // TODO : BCD for D flag
        ResolveAddress(CPU_AM_Abs);
        uint16 d = ReadM();
        doADC(d);
        cregs.PC += 3;
        break;
    }

    case 0x6f: // ADC long
    {
        // TODO : BCD for D flag
        ResolveAddress(CPU_AM_Long);
        uint16 d = ReadM();
        doADC(d);
        cregs.PC += 4;
        break;
    }
    case 0x71: // ADC (dir),y
    {
        // TODO : BCD for D flag
        ResolveAddress(CPU_AM_DirPtr16Y);
        uint16 d = ReadM();
        doADC(d);
        cregs.PC += 2;
        break;
    }

    case 0x72: // ADC (dir)
    {
        // TODO : BCD for D flag
        ResolveAddress(CPU_AM_DirPtr16);
        uint16 d = ReadM();
        doADC(d);
        cregs.PC += 2;
        break;
    }
    case 0x73: // ADC (stk,s),y
    {
        // TODO : BCD for D flag
        ResolveAddress(CPU_AM_STKPtr16Y);
        uint16 d = ReadM();
        doADC(d);
        cregs.PC += 2;
        break;
    }
    case 0x75: // ADC dir,x
    {
        // TODO : BCD for D flag
        ResolveAddress(CPU_AM_DirX);
        uint16 d = ReadM();
        doADC(d);
        cregs.PC += 2;
        break;
    }
    case 0x77: // ADC [dir],y
    {
        // TODO : BCD for D flag
        ResolveAddress(CPU_AM_DirPtr24Y);
        uint16 d = ReadM();
        doADC(d);
        cregs.PC += 2;
        break;
    }

    case 0x79: // ADC abs,y
    {
        // TODO : BCD for D flag
        ResolveAddress(CPU_AM_AbsY);
        uint16 d = ReadM();
        doADC(d);
        cregs.PC += 3;
        break;
    }

    case 0x7d: // ADC abs,x
    {
        // TODO : BCD for D flag
        ResolveAddress(CPU_AM_AbsX);
        uint16 d = ReadM();
        doADC(d);
        cregs.PC += 3;
        break;
    }

    case 0x7f: // ADC long,x
    {
        // TODO : BCD for D flag
        ResolveAddress(CPU_AM_LongX);
        uint16 d = ReadM();
        doADC(d);
        cregs.PC += 4;
        break;
    }

    case 0xe1: // SBC (dir,x)
    {

        ResolveAddress(CPU_AM_DirXPtr16);
        uint16 d = ReadM();
        doSBC(d);
        cregs.PC += 2;
        break;
    }
    case 0xe3: // SBC stk,S
    {

        ResolveAddress(CPU_AM_STK);
        uint16 d = ReadM();
        doSBC(d);
        cregs.PC += 2;
        break;
    }

    case 0xe5: // SBC dir
    {

        ResolveAddress(CPU_AM_Dir);
        uint16 d = ReadM();
        doSBC(d);
        cregs.PC += 2;
        break;
    }

    case 0xe7: // SBC [dir]
    {

        ResolveAddress(CPU_AM_DirPtr24);
        uint16 d = ReadM();
        doSBC(d);
        cregs.PC += 2;
        break;
    }

    case 0xe9: // SBC imm
    {
        ResolveAddress(CPU_AM_Imm);
        uint16 imm = ReadM();
        doSBC(imm);
        cregs.PC += 3 - flags.M;
        break;
    }

    case 0xed: // SBC abs
    {

        ResolveAddress(CPU_AM_Abs);
        uint16 d = ReadM();
        doSBC(d);
        cregs.PC += 3;
        break;
    }
    case 0xef: // SBC long
    {

        ResolveAddress(CPU_AM_Long);
        uint16 d = ReadM();
        doSBC(d);
        cregs.PC += 4;
        break;
    }
    case 0xf1: // SBC (dir),y
    {

        ResolveAddress(CPU_AM_DirPtr16Y);
        uint16 d = ReadM();
        doSBC(d);
        cregs.PC += 2;
        break;
    }
    case 0xf2: // SBC (dir)
    {
        ResolveAddress(CPU_AM_DirPtr16);
        uint16 d = ReadM();
        doSBC(d);
        cregs.PC += 2;
        break;
    }
    case 0xf3: // SBC (stk,S),y
    {

        ResolveAddress(CPU_AM_STKPtr16Y);
        uint16 d = ReadM();
        doSBC(d);
        cregs.PC += 2;
        break;
    }
    case 0xf5: // SBC dir,x
    {

        ResolveAddress(CPU_AM_DirX);
        uint16 d = ReadM();
        doSBC(d);
        cregs.PC += 2;
        break;
    }
    case 0xf7: // SBC [dir],y
    {

        ResolveAddress(CPU_AM_DirPtr24Y);
        uint16 d = ReadM();
        doSBC(d);
        cregs.PC += 2;
        break;
    }
    case 0xf9: // SBC abs,y
    {

        ResolveAddress(CPU_AM_AbsY);
        uint16 d = ReadM();
        doSBC(d);
        cregs.PC += 3;
        break;
    }
    case 0xfd: // SBC abs,x
    {

        ResolveAddress(CPU_AM_AbsX);
        uint16 d = ReadM();
        doSBC(d);
        cregs.PC += 3;
        break;
    }
    case 0xff: // SBC long,x
    {

        ResolveAddress(CPU_AM_LongX);
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
        ResolveAddress(CPU_AM_Dir);
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
        ResolveAddress(CPU_AM_Abs);
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
        ResolveAddress(CPU_AM_DirX);
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
        ResolveAddress(CPU_AM_AbsX);
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
        ResolveAddress(CPU_AM_Dir);
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
        ResolveAddress(CPU_AM_Abs);
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
        ResolveAddress(CPU_AM_DirX);
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
        ResolveAddress(CPU_AM_AbsX);
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
        ResolveAddress(CPU_AM_DirXPtr16);
        uint16 d = ReadM();
        UpdateC(cregs.C & d);
        flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
        flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
        cregs.PC += 2;

        break;
    }
    case 0x23: // AND stk,S
    {
        ResolveAddress(CPU_AM_STK);
        uint16 d = ReadM();
        UpdateC(cregs.C & d);
        flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
        flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
        cregs.PC += 2;
        break;
    }
    case 0x25: // AND dir
    {
        ResolveAddress(CPU_AM_Dir);
        uint16 d = ReadM();
        UpdateC(cregs.C & d);
        flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
        flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
        cregs.PC += 2;
        break;
    }
    case 0x27: // AND [dir]
    {
        ResolveAddress(CPU_AM_DirPtr24);
        uint16 d = ReadM();
        UpdateC(cregs.C & d);
        flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
        flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
        cregs.PC += 2;
        break;
    }
    case 0x29: // AND imm
    {
        ResolveAddress(CPU_AM_Imm);
        uint16 d = ReadM();
        UpdateC(cregs.C & d);
        flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
        flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
        cregs.PC += 3 - flags.M;
        break;
    }
    case 0x2d: // AND abs
    {
        ResolveAddress(CPU_AM_Abs);
        uint16 d = ReadM();
        UpdateC(cregs.C & d);
        flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
        flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
        cregs.PC += 3;
        break;
    }
    case 0x2f: // AND long
    {
        ResolveAddress(CPU_AM_Long);
        uint16 d = ReadM();
        UpdateC(cregs.C & d);
        flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
        flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
        cregs.PC += 4;
        break;
    }

    case 0x31: // AND (dir),y
    {
        ResolveAddress(CPU_AM_DirPtr16Y);
        uint16 d = ReadM();
        UpdateC(cregs.C & d);
        flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
        flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
        cregs.PC += 2;
        break;
    }

    case 0x32: // AND (dir)
    {
        ResolveAddress(CPU_AM_DirPtr16);
        uint16 d = ReadM();
        UpdateC(cregs.C & d);
        flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
        flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
        cregs.PC += 2;
        break;
    }

    case 0x33: // AND (stks,S),y
    {
        ResolveAddress(CPU_AM_STKPtr16Y);
        uint16 d = ReadM();
        UpdateC(cregs.C & d);
        flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
        flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
        cregs.PC += 2;
        break;
    }

    case 0x35: // AND dir,x
    {
        ResolveAddress(CPU_AM_DirX);
        uint16 d = ReadM();
        UpdateC(cregs.C & d);
        flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
        flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
        cregs.PC += 2;
        break;
    }
    case 0x37: // AND [dir],y
    {
        ResolveAddress(CPU_AM_DirPtr24Y);
        uint16 d = ReadM();
        UpdateC(cregs.C & d);
        flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
        flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
        cregs.PC += 2;
        break;
    }

    case 0x39: // AND abs,y
    {
        ResolveAddress(CPU_AM_AbsY);
        uint16 d = ReadM();
        UpdateC(cregs.C & d);
        flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
        flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
        cregs.PC += 3;

        break;
    }
    case 0x3d: // AND abs,x
    {
        ResolveAddress(CPU_AM_AbsX);
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
        ResolveAddress(CPU_AM_LongX);
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
        ResolveAddress(CPU_AM_DirXPtr16);
        uint16 d = ReadM();
        UpdateC(cregs.C ^ d);
        flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
        flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
        cregs.PC += 2;
        break;
    }
    case 0x43: // EOR stk,S
    {
        ResolveAddress(CPU_AM_STK);
        uint16 d = ReadM();
        UpdateC(cregs.C ^ d);
        flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
        flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
        cregs.PC += 2;
        break;
    }
    case 0x45: // EOR dir
    {
        ResolveAddress(CPU_AM_Dir);
        uint16 d = ReadM();
        UpdateC(cregs.C ^ d);
        flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
        flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
        cregs.PC += 2;
        break;
    }
    case 0x47: // EOR [dir]
    {
        ResolveAddress(CPU_AM_DirPtr24);
        uint16 d = ReadM();
        UpdateC(cregs.C ^ d);
        flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
        flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
        cregs.PC += 2;
        break;
    }
    case 0x49: // EOR imm
    {
        ResolveAddress(CPU_AM_Imm);
        uint16 d = ReadM();
        UpdateC(cregs.C ^ d);
        flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
        flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
        cregs.PC += 3 - flags.M;
        break;
    }
    case 0x4d: // EOR abs
    {
        ResolveAddress(CPU_AM_Abs);
        uint16 d = ReadM();
        UpdateC(cregs.C ^ d);
        flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
        flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
        cregs.PC += 3;
        break;
    }
    case 0x4f: // EOR long
    {
        ResolveAddress(CPU_AM_Long);
        uint16 d = ReadM();
        UpdateC(cregs.C ^ d);
        flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
        flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
        cregs.PC += 4;
        break;
    }

    case 0x51: // EOR (dir),y
    {
        ResolveAddress(CPU_AM_DirPtr16Y);
        uint16 d = ReadM();
        UpdateC(cregs.C ^ d);
        flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
        flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
        cregs.PC += 2;
        break;
    }

    case 0x52: // EOR (dir)
    {
        ResolveAddress(CPU_AM_DirPtr16);
        uint16 d = ReadM();
        UpdateC(cregs.C ^ d);
        flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
        flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
        cregs.PC += 2;
        break;
    }

    case 0x53: // EOR (stks,S),y
    {
        ResolveAddress(CPU_AM_STKPtr16Y);
        uint16 d = ReadM();
        UpdateC(cregs.C ^ d);
        flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
        flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
        cregs.PC += 2;
        break;
    }

    case 0x55: // EOR dir,x
    {
        ResolveAddress(CPU_AM_DirX);
        uint16 d = ReadM();
        UpdateC(cregs.C ^ d);
        flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
        flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
        cregs.PC += 2;
        break;
    }
    case 0x57: // EOR [dir],y
    {
        ResolveAddress(CPU_AM_DirPtr24Y);
        uint16 d = ReadM();
        UpdateC(cregs.C ^ d);
        flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
        flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
        cregs.PC += 2;
        break;
    }

    case 0x59: // EOR abs,y
    {
        ResolveAddress(CPU_AM_AbsY);
        uint16 d = ReadM();
        UpdateC(cregs.C ^ d);
        flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
        flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
        cregs.PC += 3;
        break;
    }
    case 0x5d: // EOR abs,x
    {
        ResolveAddress(CPU_AM_AbsX);
        uint16 d = ReadM();
        UpdateC(cregs.C ^ d);
        flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
        flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
        cregs.PC += 3;
        break;
    }

    case 0x5f: // EOR long,x
    {
        ResolveAddress(CPU_AM_LongX);
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
        ResolveAddress(CPU_AM_DirXPtr16);
        uint16 d = ReadM();
        UpdateC(cregs.C | d);
        flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
        flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
        cregs.PC += 2;
        break;
    }
    case 0x03: // ORA stk,S
    {
        ResolveAddress(CPU_AM_STK);
        uint16 d = ReadM();
        UpdateC(cregs.C | d);
        flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
        flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
        cregs.PC += 2;
        break;
    }
    case 0x05: // ORA dir
    {
        ResolveAddress(CPU_AM_Dir);
        uint16 d = ReadM();
        UpdateC(cregs.C | d);
        flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
        flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
        cregs.PC += 2;
        break;
    }
    case 0x07: // ORA [dir]
    {
        ResolveAddress(CPU_AM_DirPtr24);
        uint16 d = ReadM();
        UpdateC(cregs.C | d);
        flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
        flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
        cregs.PC += 2;
        break;
    }
    case 0x09: // ORA imm
    {
        ResolveAddress(CPU_AM_Imm);
        uint16 d = ReadM();
        UpdateC(cregs.C | d);
        flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
        flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
        cregs.PC += 3 - flags.M;
        break;
    }
    case 0x0d: // ORA abs
    {
        ResolveAddress(CPU_AM_Abs);
        uint16 d = ReadM();
        UpdateC(cregs.C | d);
        flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
        flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
        cregs.PC += 3;
        break;
    }
    case 0x0f: // ORA long
    {
        ResolveAddress(CPU_AM_Long);
        uint16 d = ReadM();
        UpdateC(cregs.C | d);
        flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
        flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
        cregs.PC += 4;
        break;
    }

    case 0x11: // ORA (dir),y
    {
        ResolveAddress(CPU_AM_DirPtr16Y);
        uint16 d = ReadM();
        UpdateC(cregs.C | d);
        flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
        flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
        cregs.PC += 2;
        break;
    }

    case 0x12: // ORA (dir)
    {
        ResolveAddress(CPU_AM_DirPtr16);
        uint16 d = ReadM();
        UpdateC(cregs.C | d);
        flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
        flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
        cregs.PC += 2;
        break;
    }

    case 0x13: // ORA (stks,S),y
    {
        ResolveAddress(CPU_AM_STKPtr16Y);
        uint16 d = ReadM();
        UpdateC(cregs.C | d);
        flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
        flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
        cregs.PC += 2;
        break;
    }

    case 0x15: // ORA dir,x
    {
        ResolveAddress(CPU_AM_DirX);
        uint16 d = ReadM();
        UpdateC(cregs.C | d);
        flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
        flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
        cregs.PC += 2;
        break;
    }
    case 0x17: // ORA [dir],y
    {
        ResolveAddress(CPU_AM_DirPtr24Y);
        uint16 d = ReadM();
        UpdateC(cregs.C | d);
        flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
        flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
        cregs.PC += 2;
        break;
    }

    case 0x19: // ORA abs,y
    {
        ResolveAddress(CPU_AM_AbsY);
        uint16 d = ReadM();
        UpdateC(cregs.C | d);
        flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
        flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
        cregs.PC += 3;
        break;
    }
    case 0x1d: // ORA abs,x
    {
        ResolveAddress(CPU_AM_AbsX);
        uint16 d = ReadM();
        UpdateC(cregs.C | d);
        flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
        flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
        cregs.PC += 3;
        break;
    }

    case 0x1f: // ORA long,x
    {
        ResolveAddress(CPU_AM_LongX);
        uint16 d = ReadM();
        UpdateC(cregs.C | d);
        flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
        flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF));
        cregs.PC += 4;
        break;
    }

    case 0x06: // ASL dir
    {
        ResolveAddress(CPU_AM_Dir);
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
        ResolveAddress(CPU_AM_Abs);
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
        ResolveAddress(CPU_AM_DirX);
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
        ResolveAddress(CPU_AM_AbsX);
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
        ResolveAddress(CPU_AM_Dir);
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
        ResolveAddress(CPU_AM_Abs);
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
        ResolveAddress(CPU_AM_DirX);
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
        ResolveAddress(CPU_AM_AbsX);
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

        ResolveAddress(CPU_AM_Dir);
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

        ResolveAddress(CPU_AM_Abs);
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

        ResolveAddress(CPU_AM_DirX);
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

        ResolveAddress(CPU_AM_AbsX);
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

        ResolveAddress(CPU_AM_Dir);
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

        ResolveAddress(CPU_AM_Abs);
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

        ResolveAddress(CPU_AM_DirX);
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

        ResolveAddress(CPU_AM_AbsX);
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
        ResolveAddress(CPU_AM_DirXPtr16);
        UpdateC(ReadM());
        flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
        flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF)); // If M is one, the upper bits of C is already 0, so theres no need for checking JUST the 8 lower bits.
        cregs.PC += 2;
        break;
    }
    case 0xa3: // LDA stk,S
    {
        ResolveAddress(CPU_AM_STK);
        UpdateC(ReadM());
        flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
        flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF)); // If M is one, the upper bits of C is already 0, so theres no need for checking JUST the 8 lower bits.
        cregs.PC += 2;
        break;
    }
    case 0xa5: // LDA dir
    {
        ResolveAddress(CPU_AM_Dir);
        UpdateC(ReadM());
        flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
        flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF)); // If M is one, the upper bits of C is already 0, so theres no need for checking JUST the 8 lower bits.
        cregs.PC += 2;
        break;
    }
    case 0xa7: // LDA  [dir]
    {
        ResolveAddress(CPU_AM_DirPtr24);
        uint16 d = ReadM();
        UpdateC(d);

        flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
        flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF)); // If M is one, the upper bits of C is already 0, so theres no need for checking JUST the 8 lower bits.
        cregs.PC += 2;
        break;
    }
    case 0xa9: // LDA imm
    {
        ResolveAddress(CPU_AM_Imm);
        uint16 imm = ReadM();
        UpdateC(imm);

        flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
        flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF)); // If M is one, the upper bits of C is already 0, so theres no need for checking JUST the 8 lower bits.
        cregs.PC += 3 - flags.M;
        break;
    }
    case 0xad: // LDA abs
    {
        ResolveAddress(CPU_AM_Abs);
        uint16 d = ReadM();
        UpdateC(d);

        flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
        flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF)); // If M is one, the upper bits of C is already 0, so theres no need for checking JUST the 8 lower bits.
        cregs.PC += 3;
        break;
    }
    case 0xaf: // LDA long
    {
        ResolveAddress(CPU_AM_Long);
        UpdateC(ReadM());
        flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
        flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF)); // If M is one, the upper bits of C is already 0, so theres no need for checking JUST the 8 lower bits.
        cregs.PC += 4;
        break;
    }

    case 0xb1: // LDA (dir),y
    {
        ResolveAddress(CPU_AM_DirPtr16Y);
        UpdateC(ReadM());
        flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
        flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF)); // If M is one, the upper bits of C is already 0, so theres no need for checking JUST the 8 lower bits.
        cregs.PC += 2;
        break;
    }

    case 0xb2: // LDA (dir)
    {
        ResolveAddress(CPU_AM_DirPtr16);
        UpdateC(ReadM());
        flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
        flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF)); // If M is one, the upper bits of C is already 0, so theres no need for checking JUST the 8 lower bits.
        cregs.PC += 2;
        break;
    }

    case 0xb3: // LDA (stk,S),y
    {
        ResolveAddress(CPU_AM_STKPtr16Y);
        UpdateC(ReadM());
        flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
        flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF)); // If M is one, the upper bits of C is already 0, so theres no need for checking JUST the 8 lower bits.
        cregs.PC += 2;
        break;
    }
    case 0xb5: // LDA dir,x
    {
        ResolveAddress(CPU_AM_DirX);
        UpdateC(ReadM());
        flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
        flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF)); // If M is one, the upper bits of C is already 0, so theres no need for checking JUST the 8 lower bits.
        cregs.PC += 2;
        break;
    }
    case 0xb7: // LDA [dir],y
    {
        ResolveAddress(CPU_AM_DirPtr24Y);
        UpdateC(ReadM());
        flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
        flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF)); // If M is one, the upper bits of C is already 0, so theres no need for checking JUST the 8 lower bits.
        cregs.PC += 2;
        break;
    }

    case 0xb9: // LDA abs,y
    {
        ResolveAddress(CPU_AM_AbsY);
        uint16 d = ReadM();
        UpdateC(d);

        flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
        flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF)); // If M is one, the upper bits of C is already 0, so theres no need for checking JUST the 8 lower bits.
        cregs.PC += 3;
        break;
    }
    case 0xbd: // LDA abs,x
    {
        ResolveAddress(CPU_AM_AbsX);
        uint16 d = ReadM();
        UpdateC(d);

        flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
        flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF)); // If M is one, the upper bits of C is already 0, so theres no need for checking JUST the 8 lower bits.
        cregs.PC += 3;
        break;
    }
    case 0xbf: // LDA long,x
    {
        ResolveAddress(CPU_AM_LongX);
        UpdateC(ReadM());
        flags.N = cregs.C & (flags.M ? 0x0080 : 0x8000);
        flags.Z = !(cregs.C & (flags.M ? 0x00FF : 0xFFFF)); // If M is one, the upper bits of C is already 0, so theres no need for checking JUST the 8 lower bits.
        cregs.PC += 4;
        break;
    }
    case 0xa2: // LDX imm
    {
        ResolveAddress(CPU_AM_Imm);
        uint16 imm = ReadX();

        UpdateX(imm);

        flags.N = cregs.X & (flags.X ? 0x0080 : 0x8000);
        flags.Z = !(cregs.X & (flags.X ? 0x00FF : 0xFFFF));
        cregs.PC += 3 - flags.X;
        break;
    }
    case 0xa6: // LDX dir
    {
        ResolveAddress(CPU_AM_Dir);
        UpdateX(ReadX());
        flags.N = cregs.X & (flags.X ? 0x0080 : 0x8000);
        flags.Z = !(cregs.X & (flags.X ? 0x00FF : 0xFFFF));
        cregs.PC += 2;
        break;
    }
    case 0xae: // LDX abs
    {
        ResolveAddress(CPU_AM_Abs);
        UpdateX(ReadX());
        flags.N = cregs.X & (flags.X ? 0x0080 : 0x8000);
        flags.Z = !(cregs.X & (flags.X ? 0x00FF : 0xFFFF));
        cregs.PC += 3;
        break;
    }

    case 0xb6: // LDX dir,y
    {
        ResolveAddress(CPU_AM_DirY);
        UpdateX(ReadX());
        flags.N = cregs.X & (flags.X ? 0x0080 : 0x8000);
        flags.Z = !(cregs.X & (flags.X ? 0x00FF : 0xFFFF));
        cregs.PC += 2;
        break;
    }

    case 0xbe: // LDX abs,y
    {
        ResolveAddress(CPU_AM_AbsY);
        UpdateX(ReadX());
        flags.N = cregs.X & (flags.X ? 0x0080 : 0x8000);
        flags.Z = !(cregs.X & (flags.X ? 0x00FF : 0xFFFF));
        cregs.PC += 3;
        break;
    }

    case 0xa0: // LDY imm
    {
        ResolveAddress(CPU_AM_Imm);
        uint16 imm = ReadX();

        UpdateY(imm);

        flags.N = cregs.Y & (flags.X ? 0x0080 : 0x8000);
        flags.Z = !(cregs.Y & (flags.X ? 0x00FF : 0xFFFF));
        cregs.PC += 3 - flags.X;
        break;
    }

    case 0xa4: // LDY dir
    {
        ResolveAddress(CPU_AM_Dir);
        UpdateY(ReadX());
        flags.N = cregs.Y & (flags.X ? 0x0080 : 0x8000);
        flags.Z = !(cregs.Y & (flags.X ? 0x00FF : 0xFFFF));
        cregs.PC += 2;
        break;
    }

    case 0xac: // LDY abs
    {
        ResolveAddress(CPU_AM_Abs);
        UpdateY(ReadX());
        flags.N = cregs.Y & (flags.X ? 0x0080 : 0x8000);
        flags.Z = !(cregs.Y & (flags.X ? 0x00FF : 0xFFFF));
        cregs.PC += 3;
        break;
    }
    case 0xb4: // LDY dir,x
    {
        ResolveAddress(CPU_AM_DirX);
        UpdateY(ReadX());
        flags.N = cregs.Y & (flags.X ? 0x0080 : 0x8000);
        flags.Z = !(cregs.Y & (flags.X ? 0x00FF : 0xFFFF));
        cregs.PC += 2;
        break;
    }
    case 0xbc: // LDY abs,x
    {
        ResolveAddress(CPU_AM_AbsX);
        UpdateY(ReadX());
        flags.N = cregs.Y & (flags.X ? 0x0080 : 0x8000);
        flags.Z = !(cregs.Y & (flags.X ? 0x00FF : 0xFFFF));
        cregs.PC += 3;
        break;
    }

    case 0x81: // STA (dir,x)
    {

        ResolveAddress(CPU_AM_DirXPtr16);
        WriteM(cregs.C);
        // No Flags
        cregs.PC += 2;
        break;
    }

    case 0x83: // STA stk,s
    {

        ResolveAddress(CPU_AM_STK);
        WriteM(cregs.C);
        // No Flags
        cregs.PC += 2;
        break;
    }
    case 0x85: // STA dir
    {

        ResolveAddress(CPU_AM_Dir);
        WriteM(cregs.C);
        // No Flags
        cregs.PC += 2;
        break;
    }
    case 0x87: // STA [dir]
    {

        ResolveAddress(CPU_AM_DirPtr24);
        WriteM(cregs.C);
        // No Flags
        cregs.PC += 2;
        break;
    }
    case 0x8d: // STA abs
    {
        ResolveAddress(CPU_AM_Abs);
        WriteM(cregs.C);
        cregs.PC += 3;
        break;
    }
    case 0x8f: // STA long
    {
        ResolveAddress(CPU_AM_Long);

        WriteM(cregs.C);
        cregs.PC += 4;
        break;
    }
    case 0x91: // STA (dir),y
    {

        ResolveAddress(CPU_AM_DirPtr16Y);
        WriteM(cregs.C);
        // No Flags
        cregs.PC += 2;
        break;
    }
    case 0x92: // STA (dir)
    {
        ResolveAddress(CPU_AM_DirPtr16);

        WriteM(cregs.C);
        // No Flags
        cregs.PC += 2;
        break;
    }
    case 0x93: // STA (stk,s),y
    {

        ResolveAddress(CPU_AM_STKPtr16Y);
        WriteM(cregs.C);
        // No Flags
        cregs.PC += 2;
        break;
    }
    case 0x95: // STA dir,x
    {

        ResolveAddress(CPU_AM_DirX);
        WriteM(cregs.C);
        // No Flags
        cregs.PC += 2;
        break;
    }
    case 0x97: // STA [dir],y
    {

        ResolveAddress(CPU_AM_DirPtr24Y);
        WriteM(cregs.C);
        // No Flags
        cregs.PC += 2;
        break;
    }
    case 0x99: // STA abs,y
    {
        ResolveAddress(CPU_AM_AbsY);
        WriteM(cregs.C);
        cregs.PC += 3;
        break;
    }

    case 0x9d: // STA abs,x
    {
        ResolveAddress(CPU_AM_AbsX);
        WriteM(cregs.C);
        cregs.PC += 3;
        break;
    }

    case 0x9f: // STA long,x
    {
        ResolveAddress(CPU_AM_LongX);
        WriteM(cregs.C);
        cregs.PC += 4;
        break;
    }

    case 0x86: // STX dir
    {

        ResolveAddress(CPU_AM_Dir);
        WriteX(cregs.X);
        // No Flags
        cregs.PC += 2;
        break;
    }
    case 0x8e: // STX abs
    {
        ResolveAddress(CPU_AM_Abs);
        WriteX(cregs.X);
        cregs.PC += 3;
        break;
    }
    case 0x96: // STX dir,y
    {

        ResolveAddress(CPU_AM_DirY);
        WriteX(cregs.X);
        // No Flags
        cregs.PC += 2;
        break;
    }

    case 0x84: // STY dir
    {

        ResolveAddress(CPU_AM_Dir);
        WriteX(cregs.Y);
        // No Flags
        cregs.PC += 2;
        break;
    }

    case 0x8c: // STY abs
    {
        ResolveAddress(CPU_AM_Abs);
        WriteX(cregs.Y);
        cregs.PC += 3;
        break;
    }

    case 0x94: // STY dir,x
    {

        ResolveAddress(CPU_AM_DirX);
        WriteX(cregs.Y);
        // No Flags
        cregs.PC += 2;
        break;
    }

    case 0x64: // STZ dir
    {

        ResolveAddress(CPU_AM_Dir);
        WriteM(0);
        // No Flags
        cregs.PC += 2;
        break;
    }
    case 0x74: // STZ dir,x
    {

        ResolveAddress(CPU_AM_DirX);
        WriteM(0);
        // No Flags
        cregs.PC += 2;
        break;
    }
    case 0x9c: // STZ abs
    {
        ResolveAddress(CPU_AM_Abs);
        WriteM(0);
        cregs.PC += 3;
        break;
    }
    case 0x9e: // STZ abs,x
    {
        ResolveAddress(CPU_AM_AbsX);
        WriteM(0);
        cregs.PC += 3;
        break;
    }
#pragma endregion

#pragma region Compare_and_Test
    case 0xc1: // CMP (dir,x)
    {
        // TODO : Check 8 bit operation
        ResolveAddress(CPU_AM_DirXPtr16);
        uint16 d = ReadM();
        //
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
        ResolveAddress(CPU_AM_STK);
        uint16 d = ReadM();
        //
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
        ResolveAddress(CPU_AM_Dir);
        uint16 d = ReadM();
        //
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
        ResolveAddress(CPU_AM_DirPtr24);
        uint16 d = ReadM();
        //
        flags.C = (cregs.C & (flags.M ? 0x00FF : 0xFFFF)) >= d;
        d = cregs.C - d;
        flags.N = d & (flags.M ? 0x0080 : 0x8000);
        flags.Z = !(d & (flags.M ? 0x00FF : 0xFFFF));
        cregs.PC += 2;
        break;
    }
    case 0xc9: // CMP imm
    {
        ResolveAddress(CPU_AM_Imm);
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
        ResolveAddress(CPU_AM_Abs);
        uint16 d = ReadM();
        //
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
        ResolveAddress(CPU_AM_Long);
        uint16 d = ReadM();
        //
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
        ResolveAddress(CPU_AM_DirPtr16Y);
        uint16 d = ReadM();
        //
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
        ResolveAddress(CPU_AM_DirPtr16);
        uint16 d = ReadM();
        //
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
        ResolveAddress(CPU_AM_STKPtr16Y);
        uint16 d = ReadM();
        //
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
        ResolveAddress(CPU_AM_DirX);
        uint16 d = ReadM();
        //
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
        ResolveAddress(CPU_AM_DirPtr24Y);
        uint16 d = ReadM();
        //
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
        ResolveAddress(CPU_AM_AbsY);
        uint16 d = ReadM();
        //
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
        ResolveAddress(CPU_AM_AbsX);
        uint16 d = ReadM();
        //
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
        ResolveAddress(CPU_AM_LongX);
        uint16 d = ReadM();
        //
        flags.C = (cregs.C & (flags.M ? 0x00FF : 0xFFFF)) >= d;
        d = cregs.C - d;
        flags.N = d & (flags.M ? 0x0080 : 0x8000);
        flags.Z = !(d & (flags.M ? 0x00FF : 0xFFFF));
        cregs.PC += 4;
        break;
    }
    case 0xe0: // CPX imm
    {
        ResolveAddress(CPU_AM_Imm);
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
        ResolveAddress(CPU_AM_Dir);
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
        ResolveAddress(CPU_AM_Abs);
        uint16 d = ReadX();
        //
        flags.C = (cregs.X & (flags.X ? 0x00FF : 0xFFFF)) >= d;
        d = cregs.X - d;

        flags.N = d & (flags.X ? 0x0080 : 0x8000);
        flags.Z = !(d & (flags.X ? 0x00FF : 0xFFFF));

        cregs.PC += 3;
        break;
    }

    case 0xc0: // CPY imm
    {
        ResolveAddress(CPU_AM_Imm);
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
        ResolveAddress(CPU_AM_Dir);
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
        ResolveAddress(CPU_AM_Abs);
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
        ResolveAddress(CPU_AM_Dir);
        uint16 d = ReadM();
        //

        uint16 t = cregs.C & d;
        flags.Z = !(t & (flags.M ? 0x00FF : 0xFFFF));
        flags.N = d & (flags.M ? 0x0080 : 0x8000);
        flags.V = d & (flags.M ? 0x0040 : 0x4000);
        cregs.PC += 2;
        break;
    }
    case 0x2c: // BIT abs
    {
        ResolveAddress(CPU_AM_Abs);
        uint16 d = ReadM();
        //
        uint16 t = cregs.C & d;
        flags.Z = !(t & (flags.M ? 0x00FF : 0xFFFF));
        flags.N = d & (flags.M ? 0x0080 : 0x8000);
        flags.V = d & (flags.M ? 0x0040 : 0x4000);
        cregs.PC += 3;
        break;
    }
    case 0x34: // BIT dir,x
    {
        ResolveAddress(CPU_AM_DirX);
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
        ResolveAddress(CPU_AM_AbsX);
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
        ResolveAddress(CPU_AM_Imm);
        uint16 imm = ReadM();
        uint16 t = cregs.C & imm;
        flags.Z = !(t & (flags.M ? 0x00FF : 0xFFFF));
        cregs.PC += 3 - flags.M;
        break;
    }

    case 0x14: // TRB dir
    {
        ResolveAddress(CPU_AM_Dir);
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
        ResolveAddress(CPU_AM_Abs);
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
        ResolveAddress(CPU_AM_Dir);
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
        ResolveAddress(CPU_AM_Abs);
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
        ResolveAddress(CPU_AM_Rel8);
        signed char ll = ReadBus(addL);
        if (!flags.C)
            cregs.PC += ll;
        cregs.PC += 2;
        break;
    }
    case 0xB0: // BCS
    {
        ResolveAddress(CPU_AM_Rel8);
        signed char ll = ReadBus(addL);
        if (flags.C)
            cregs.PC += ll;
        cregs.PC += 2;
        break;
    }
    case 0xd0: // BNE rel8
    {
        ResolveAddress(CPU_AM_Rel8);
        signed char ll = ReadBus(addL);
        if (!flags.Z)
            cregs.PC += ll;
        cregs.PC += 2;
        break;
    }
    case 0x10: // BPL rel8
    {
        ResolveAddress(CPU_AM_Rel8);
        signed char ll = ReadBus(addL);
        if (!flags.N)
            cregs.PC += ll;
        cregs.PC += 2;
        break;
    }
    case 0x80: // BRA rel8
    {
        ResolveAddress(CPU_AM_Rel8);
        signed char ll = ReadBus(addL);
        cregs.PC += 2 + ll;
        break;
    }
    case 0xf0: // BEQ rel8
    {
        ResolveAddress(CPU_AM_Rel8);
        signed char ll = ReadBus(addL);
        if (flags.Z)
            cregs.PC += ll;
        cregs.PC += 2;

        break;
    }
    case 0x30: // BMI
    {
        ResolveAddress(CPU_AM_Rel8);
        signed char ll = ReadBus(addL);
        if (flags.N)
            cregs.PC += ll;
        cregs.PC += 2;
        break;
    }
    case 0x70: // BVS rel8
    {
        ResolveAddress(CPU_AM_Rel8);
        signed char ll = ReadBus(addL);
        if (flags.V)
            cregs.PC += ll;
        cregs.PC += 2;
        break;
    }
    case 0x50: // BVC rel8
    {
        ResolveAddress(CPU_AM_Rel8);
        signed char ll = ReadBus(addL);
        if (!flags.V)
            cregs.PC += ll;
        cregs.PC += 2;
        break;
    }
    case 0x82: // BRL rel16
    {
        ResolveAddress(CPU_AM_Rel16);
        signed short ll = ReadWord();

        cregs.PC += ll;
        cregs.PC += 3;
        break;
    }

    case 0x4c: // JMP abs
    {
        ResolveAddress(CPU_AM_Abs);
        cregs.PC = addL & 0x00FFFF;
        break;
    }

    case 0x5c: // JMP long
    {
        ResolveAddress(CPU_AM_Long);
        cregs.PC = addL & 0x00FFFF;
        cregs.K = (addL & 0xFF0000) >> 16;
        break;
    }

    case 0x6c: // JMP (abs)
    {
        ResolveAddress(CPU_AM_AbsPtr16);
        cregs.PC = addL & 0x00FFFF;
        break;
    }

    case 0x20: // JSR abs
    {
        ResolveAddress(CPU_AM_Abs);

        // push PC+2 to stack
        PushWord(cregs.PC + 2);
        // No Flags
        cregs.PC = addL;
        break;
    }
    case 0xfc: // JSR (abs,x)
    {
        // This case also needs K in addressing the address
        ResolveAddress(CPU_AM_AbsXPtr16);
        // push PC+2 to stack
        PushWord(cregs.PC + 2);
        //
        // cin.get();
        //  No Flags
        cregs.PC = addL;

        break;
    }
    case 0x22: // JSL long
    {
        ResolveAddress(CPU_AM_Long);
        Push(cregs.K);
        PushWord(cregs.PC + 3);
        cregs.PC = addL;
        cregs.K = addL >> 16;
        break;
    }
    case 0x7c: // JMP (abs,x)
    {
        // This case also needs K in addressing the address
        ResolveAddress(CPU_AM_AbsXPtr16);
        cregs.PC = addL;
        // No Flags
        break;
    }
    case 0xdc: // JMP [abs]
    {
        ResolveAddress(CPU_AM_AbsPtr24);
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
        add24 srcB = ReadBus(PCByteAhead(2)) << 16;

        cregs.DBR = ReadBus(PCByteAhead(1)); // DBR = dest bank
        add24 dstB = cregs.DBR << 16;
        add24 src, dest;

        src = srcB + cregs.X;
        dest = dstB + cregs.Y;
        WriteBus(dest, ReadBus(src));
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
        add24 srcB = ReadBus(PCByteAhead(2)) << 16;
        cregs.DBR = ReadBus(PCByteAhead(1)); // DBR = dest bank
        add24 dstB = cregs.DBR << 16;
        add24 src, dest;

        src = srcB + cregs.X;
        dest = dstB + cregs.Y;
        WriteBus(dest, ReadBus(src));
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
        //
        // cin.get();
    }
    if (flags.X)
    {
        cregs.X &= 0x00FF;
        cregs.Y &= 0x00FF;
    }
}
#pragma endregion


#pragma region SPC700


void SPC700_ResolveAddress(SPC700AddressingMode mode)
{
    uint8 ll, hh;
    ll = SPCRead(SPC_regs.PC + 1);
    hh = SPCRead(SPC_regs.PC + 2);

    switch (mode)
    {
    case SPC700_AM_Imm:
        SPC_addL = SPC_regs.PC + 1;
        break;

    case SPC700_AM_XPtr:
        SPC_addL = SPC_regs.X | (SPC_flags.P ? 0x0100 : 0x0000);
        break;

    case SPC700_AM_YPtr:
        SPC_addL = SPC_regs.Y | (SPC_flags.P ? 0x0100 : 0x0000);
        break;

    case SPC700_AM_DP:
        SPC_addL = ll | (SPC_flags.P ? 0x0100 : 0x0000);
        SPC_addH = ((ll + 1) & 0x00FF) | (SPC_flags.P ? 0x0100 : 0x0000);
        break;

    case SPC700_AM_DPX:
        SPC_addL = ((uint8)(ll + SPC_regs.X)) | (SPC_flags.P ? 0x0100 : 0x0000);
        SPC_addH = hh;
        break;
    case SPC700_AM_DPY:
        SPC_addL = ((uint8)(ll + SPC_regs.Y)) | (SPC_flags.P ? 0x0100 : 0x0000);
        break;

    case SPC700_AM_Abs:
        SPC_addL = (hh << 8) | ll;
        break;

    case SPC700_AM_AbsX:
        SPC_addL = ((hh << 8) | ll) + SPC_regs.X;
        SPC_addH = ((hh << 8) | ll) + SPC_regs.X + 1;
        break;

    case SPC700_AM_AbsY:
        SPC_addL = ((hh << 8) | ll) + SPC_regs.Y;
        break;

    case SPC700_AM_DPXPtr:
        SPC_addL = ((uint8)(ll + SPC_regs.X)) | (SPC_flags.P ? 0x0100 : 0x0000);
        SPC_addH = ((uint8)(ll + SPC_regs.X + 1)) | (SPC_flags.P ? 0x0100 : 0x0000);
        SPC_addL = (SPCRead(SPC_addH) << 8) | SPCRead(SPC_addL);
        break;

    case SPC700_AM_DPPtrY:
        SPC_addL = ((uint8)(ll)) | (SPC_flags.P ? 0x0100 : 0x0000);
        SPC_addH = ((uint8)(ll + 1)) | (SPC_flags.P ? 0x0100 : 0x0000);
        SPC_addL = (SPCRead(SPC_addH) << 8) | SPCRead(SPC_addL);
        SPC_addL += SPC_regs.Y;
        break;

    case SPC700_AM_SrcDst:
        SPC_addL = ll | (SPC_flags.P ? 0x0100 : 0x0000);
        SPC_addH = hh | (SPC_flags.P ? 0x0100 : 0x0000);
        break;

    case SPC700_AM_MemBit:
        SPC_addL = (ll) | ((hh & 0x1F) << 8);
        SPC_addH = (hh & 0xE0) >> 5;
        break;

    default:
        break;
    }
}

uint8 SPC700_GetFlagsAsByte()
{
    return (SPC_flags.N ? spc700_FlagNMask : 0) | (SPC_flags.V ? spc700_FlagVMask : 0) | (SPC_flags.P ? spc700_FlagPMask : 0) | (SPC_flags.B ? spc700_FlagBMask : 0) | (SPC_flags.H ? spc700_FlagHMask : 0) | (SPC_flags.I ? spc700_FlagIMask : 0) | (SPC_flags.Z ? spc700_FlagZMask : 0) | (SPC_flags.C ? spc700_FlagCMask : 0);
}
void SPC700_SetFlagsAsByte(uint8 bits)
{
    SPC_flags.N = bits & spc700_FlagNMask;
    SPC_flags.V = bits & spc700_FlagVMask;
    SPC_flags.P = bits & spc700_FlagPMask;
    SPC_flags.B = bits & spc700_FlagBMask;
    SPC_flags.H = bits & spc700_FlagHMask;
    SPC_flags.I = bits & spc700_FlagIMask;
    SPC_flags.Z = bits & spc700_FlagZMask;
    SPC_flags.C = bits & spc700_FlagCMask;
}
uint8 SPC700_DoADC(uint8 A, uint8 B)
{
    unsigned int t;

    int op1 = A & 0xFF;
    int op2 = B & 0xFF;
    t = op1 + op2 + SPC_flags.C;
    SPC_flags.V = (((op1 ^ t) & (op2 ^ t) & 0x80) != 0);
    SPC_flags.H = ((A & 0x0F) + (B & 0x0F) + SPC_flags.C) > 0x0F;
    SPC_flags.C = (t > 0xFF);
    SPC_flags.N = t & 0x0080;
    SPC_flags.Z = !(t & 0x00FF);

    return t;
}

uint8 SPC700_DoSBC(uint8 A, uint8 B)
{
    unsigned int t;
    int op1 = A;
    int op2 = B;
    int binDiff = op1 - op2 - (SPC_flags.C ? 0 : 1);

    t = binDiff & 0xFF;
    SPC_flags.H = ((int)(op1 & 0x0F) - (int)(op2 & 0x0F) - (int)(SPC_flags.C ? 0 : 1)) >= 0;
    SPC_flags.V = (((op1 ^ op2) & (op1 ^ t) & 0x80) != 0);
    SPC_flags.C = (binDiff >= 0);

    SPC_flags.N = (t & 0x0080) != 0;
    SPC_flags.Z = (t & 0x00FF) == 0;

    return t;
}

uint8 SPC700_DoCMP(uint8 A, uint8 B)
{
    unsigned int t;
    int op1 = A;
    int op2 = B;
    int binDiff = op1 - op2;

    t = binDiff & 0xFF;
    SPC_flags.C = (binDiff >= 0);
    SPC_flags.N = (t & 0x0080) != 0;
    SPC_flags.Z = (t & 0x00FF) == 0;

    return t;
}

void SPC700_Push(uint8 data)
{
    SPCWrite(0x0100 | SPC_regs.SP, data);
    SPC_regs.SP--;
}

uint8 SPC700_Pop()
{
    SPC_regs.SP++;
    return SPCRead(0x0100 | SPC_regs.SP);
}

// void PrintState()
// {
//     // nvmxdizc
//     cout << "--------------------------NEW INSTRUCTON---------------------------------------" << endl;
//     cout << "-----------Flags----------" << endl;
//     cout << "N V P B H I Z C" << endl;
//     cout << SPC_flags.N << " " << SPC_flags.V << " " << SPC_flags.P << " " << SPC_flags.B << " " << SPC_flags.H << " " << SPC_flags.I << " " << SPC_flags.Z << " " << SPC_flags.C << endl;
//     cout << "-----------Regs----------" << endl;
//     // cout << "Stack top : " << hex << (unsigned int)SPCRead(cregs.S + 1) << " " << (unsigned int)SPCRead(cregs.S + 2) << endl;
//     cout << "SPC_regs.A : " << std::hex << (unsigned int)SPC_regs.A << endl;
//     cout << "SPC_regs.SP : " << std::hex << (unsigned int)SPC_regs.SP << endl;
//     cout << "SPC_regs.X : " << std::hex << (unsigned int)SPC_regs.X << endl;
//     cout << "SPC_regs.Y : " << std::hex << (unsigned int)SPC_regs.Y << endl;
//     cout << "SPC_regs.PC : " << std::hex << (unsigned int)SPC_regs.PC << endl;
//     uint8_t inst = SPCRead(SPC_regs.PC);
//     cout << "OpCode : " << std::hex << (int)inst << endl;
//     cout << "3 Bytes Ahead : " << std::hex << (int)SPCRead(SPC_regs.PC + 1) << " " << std::hex << (int)SPCRead(SPC_regs.PC + 2) << " " << std::hex << (int)SPCRead(SPC_regs.PC + 3) << " " << endl;
// }

void SPC700_Reset()
{
    SPC_cycles = 0;
    SPC_regs.A = SPC_regs.X = SPC_regs.Y = 0;
    SPC_flags.N = SPC_flags.V = SPC_flags.P = SPC_flags.B = SPC_flags.H = SPC_flags.I = SPC_flags.Z = SPC_flags.C = 0;
    SPC_regs.PC = 0xFFC0;
    SPC_addL = 0;
}

void SPC700_Step()
{
    uint8 inst = SPCRead(SPC_regs.PC);

    switch (inst)
    {
#pragma region DataTransfer_MemToReg
    case 0xE8: // MOV SPC_regs.A,Imm
    {
        SPC700_ResolveAddress(SPC700_AM_Imm);
        uint8 imm = SPCRead(SPC_addL);
        SPC_regs.A = imm;
        SPC_flags.N = SPC_regs.A & 0x80;
        SPC_flags.Z = !(SPC_regs.A);
        SPC_regs.PC += 2;
        SPC_cycles += 2;
        break;
    }

    case 0xE6: // MOV SPC_regs.A,(SPC_regs.X)
    {
        SPC700_ResolveAddress(SPC700_AM_XPtr);
        uint8 d = SPCRead(SPC_addL);
        SPC_regs.A = d;
        SPC_flags.N = SPC_regs.A & 0x80;
        SPC_flags.Z = !(SPC_regs.A);
        SPC_regs.PC += 1;
        SPC_cycles += 3;
        break;
    }

    case 0xBF: // MOV SPC_regs.A,(SPC_regs.X)+
    {
        SPC700_ResolveAddress(SPC700_AM_XPtr);
        uint8 d = SPCRead(SPC_addL);
        SPC_regs.A = d;
        SPC_flags.N = SPC_regs.A & 0x80;
        SPC_flags.Z = !(SPC_regs.A);
        SPC_regs.X += 1;
        SPC_regs.PC += 1;
        SPC_cycles += 4;
        break;
    }

    case 0xE4: // MOV SPC_regs.A,(dp)
    {
        SPC700_ResolveAddress(SPC700_AM_DP);
        uint8 d = SPCRead(SPC_addL);
        SPC_regs.A = d;
        SPC_flags.N = SPC_regs.A & 0x80;
        SPC_flags.Z = !(SPC_regs.A);
        SPC_regs.PC += 2;
        SPC_cycles += 3;
        break;
    }

    case 0xF4: // MOV SPC_regs.A,(dp+SPC_regs.X)
    {
        SPC700_ResolveAddress(SPC700_AM_DPX);
        uint8 d = SPCRead(SPC_addL);
        SPC_regs.A = d;
        SPC_flags.N = SPC_regs.A & 0x80;
        SPC_flags.Z = !(SPC_regs.A);
        SPC_regs.PC += 2;
        SPC_cycles += 4;
        break;
    }

    case 0xE5: // MOV SPC_regs.A,(abs)
    {
        SPC700_ResolveAddress(SPC700_AM_Abs);
        uint8 d = SPCRead(SPC_addL);
        SPC_regs.A = d;
        SPC_flags.N = SPC_regs.A & 0x80;
        SPC_flags.Z = !(SPC_regs.A);
        SPC_regs.PC += 3;
        SPC_cycles += 4;
        break;
    }

    case 0xF5: // MOV SPC_regs.A,(abs+SPC_regs.X)
    {
        SPC700_ResolveAddress(SPC700_AM_AbsX);
        uint8 d = SPCRead(SPC_addL);
        SPC_regs.A = d;
        SPC_flags.N = SPC_regs.A & 0x80;
        SPC_flags.Z = !(SPC_regs.A);
        SPC_regs.PC += 3;
        SPC_cycles += 5;
        break;
    }

    case 0xF6: // MOV SPC_regs.A,(abs+SPC_regs.X)
    {
        SPC700_ResolveAddress(SPC700_AM_AbsY);
        uint8 d = SPCRead(SPC_addL);
        SPC_regs.A = d;
        SPC_flags.N = SPC_regs.A & 0x80;
        SPC_flags.Z = !(SPC_regs.A);
        SPC_regs.PC += 3;
        SPC_cycles += 5;
        break;
    }

    case 0xE7: // MOV SPC_regs.A,((dp+SPC_regs.X))
    {
        SPC700_ResolveAddress(SPC700_AM_DPXPtr);
        uint8 d = SPCRead(SPC_addL);
        SPC_regs.A = d;
        SPC_flags.N = SPC_regs.A & 0x80;
        SPC_flags.Z = !(SPC_regs.A);
        SPC_regs.PC += 2;
        SPC_cycles += 6;
        break;
    }

    case 0xF7: // MOV SPC_regs.A,((dp),y)
    {
        SPC700_ResolveAddress(SPC700_AM_DPPtrY);
        uint8 d = SPCRead(SPC_addL);
        SPC_regs.A = d;
        SPC_flags.N = SPC_regs.A & 0x80;
        SPC_flags.Z = !(SPC_regs.A);
        SPC_regs.PC += 2;
        SPC_cycles += 6;
        break;
    }

    case 0xCD: // MOV SPC_regs.X,Imm
    {
        SPC700_ResolveAddress(SPC700_AM_Imm);
        uint8 d = SPCRead(SPC_addL);
        SPC_regs.X = d;
        SPC_flags.N = SPC_regs.X & 0x80;
        SPC_flags.Z = !(SPC_regs.X);
        SPC_regs.PC += 2;
        SPC_cycles += 2;
        break;
    }

    case 0xF8: // MOV SPC_regs.X,(dp)
    {
        SPC700_ResolveAddress(SPC700_AM_DP);
        uint8 d = SPCRead(SPC_addL);
        SPC_regs.X = d;
        SPC_flags.N = SPC_regs.X & 0x80;
        SPC_flags.Z = !(SPC_regs.X);
        SPC_regs.PC += 2;
        SPC_cycles += 3;
        break;
    }

    case 0xF9: // MOV SPC_regs.X,(dp+SPC_regs.Y)
    {
        SPC700_ResolveAddress(SPC700_AM_DPY);
        uint8 d = SPCRead(SPC_addL);
        SPC_regs.X = d;
        SPC_flags.N = SPC_regs.X & 0x80;
        SPC_flags.Z = !(SPC_regs.X);
        SPC_regs.PC += 2;
        SPC_cycles += 4;
        break;
    }

    case 0xE9: // MOV SPC_regs.X,(abs)
    {
        SPC700_ResolveAddress(SPC700_AM_Abs);
        uint8 d = SPCRead(SPC_addL);
        SPC_regs.X = d;
        SPC_flags.N = SPC_regs.X & 0x80;
        SPC_flags.Z = !(SPC_regs.X);
        SPC_regs.PC += 3;
        SPC_cycles += 4;
        break;
    }

    case 0x8D: // MOV SPC_regs.Y,Imm
    {
        SPC700_ResolveAddress(SPC700_AM_Imm);
        uint8 d = SPCRead(SPC_addL);
        SPC_regs.Y = d;
        SPC_flags.N = SPC_regs.Y & 0x80;
        SPC_flags.Z = !(SPC_regs.Y);
        SPC_regs.PC += 2;
        SPC_cycles += 2;
        break;
    }

    case 0xEB: // MOV SPC_regs.Y,(dp)
    {
        SPC700_ResolveAddress(SPC700_AM_DP);
        uint8 d = SPCRead(SPC_addL);
        SPC_regs.Y = d;
        SPC_flags.N = SPC_regs.Y & 0x80;
        SPC_flags.Z = !(SPC_regs.Y);
        SPC_regs.PC += 2;
        SPC_cycles += 3;
        break;
    }

    case 0xFB: // MOV SPC_regs.Y,(dp+SPC_regs.X)
    {
        SPC700_ResolveAddress(SPC700_AM_DPX);
        uint8 d = SPCRead(SPC_addL);
        SPC_regs.Y = d;
        SPC_flags.N = SPC_regs.Y & 0x80;
        SPC_flags.Z = !(SPC_regs.Y);
        SPC_regs.PC += 2;
        SPC_cycles += 4;
        break;
    }

    case 0xEC: // MOV SPC_regs.Y,(abs)
    {
        SPC700_ResolveAddress(SPC700_AM_Abs);
        uint8 d = SPCRead(SPC_addL);
        SPC_regs.Y = d;
        SPC_flags.N = SPC_regs.Y & 0x80;
        SPC_flags.Z = !(SPC_regs.Y);
        SPC_regs.PC += 3;
        SPC_cycles += 4;
        break;
    }

#pragma endregion
#pragma region DataTransfer_RegToMem

    case 0xC6: // MOV (SPC_regs.X),SPC_regs.A
    {
        SPC700_ResolveAddress(SPC700_AM_XPtr);
        SPCWrite(SPC_addL, SPC_regs.A);
        SPC_regs.PC += 1;
        SPC_cycles += 4;
        break;
    }

    case 0xAF: // MOV (SPC_regs.X)+,SPC_regs.A
    {
        SPC700_ResolveAddress(SPC700_AM_XPtr);
        SPCWrite(SPC_addL, SPC_regs.A);
        SPC_regs.X += 1;
        SPC_regs.PC += 1;
        SPC_cycles += 4;
        break;
    }

    case 0xC4: // MOV (dp),SPC_regs.A
    {
        SPC700_ResolveAddress(SPC700_AM_DP);
        SPCWrite(SPC_addL, SPC_regs.A);
        SPC_regs.PC += 2;
        SPC_cycles += 4;
        break;
    }

    case 0xD4: // MOV (dp+SPC_regs.X),SPC_regs.A
    {
        SPC700_ResolveAddress(SPC700_AM_DPX);
        SPCWrite(SPC_addL, SPC_regs.A);
        SPC_regs.PC += 2;
        SPC_cycles += 5;
        break;
    }

    case 0xC5: // MOV (abs),SPC_regs.A
    {
        SPC700_ResolveAddress(SPC700_AM_Abs);
        SPCWrite(SPC_addL, SPC_regs.A);
        SPC_regs.PC += 3;
        SPC_cycles += 5;
        break;
    }

    case 0xD5: // MOV (abs+SPC_regs.X),SPC_regs.A
    {
        SPC700_ResolveAddress(SPC700_AM_AbsX);
        SPCWrite(SPC_addL, SPC_regs.A);
        SPC_regs.PC += 3;
        SPC_cycles += 6;
        break;
    }

    case 0xD6: // MOV (abs+SPC_regs.X),SPC_regs.A
    {
        SPC700_ResolveAddress(SPC700_AM_AbsY);
        SPCWrite(SPC_addL, SPC_regs.A);
        SPC_regs.PC += 3;
        SPC_cycles += 6;
        break;
    }

    case 0xC7: // MOV ((dp+SPC_regs.X)),SPC_regs.A
    {
        SPC700_ResolveAddress(SPC700_AM_DPXPtr);
        SPCWrite(SPC_addL, SPC_regs.A);
        SPC_regs.PC += 2;
        SPC_cycles += 7;
        break;
    }

    case 0xD7: // MOV ((dp),y),SPC_regs.A
    {
        SPC700_ResolveAddress(SPC700_AM_DPPtrY);
        SPCWrite(SPC_addL, SPC_regs.A);
        SPC_regs.PC += 2;
        SPC_cycles += 7;
        break;
    }

    case 0xD8: // MOV (dp),SPC_regs.X
    {
        SPC700_ResolveAddress(SPC700_AM_DP);
        SPCWrite(SPC_addL, SPC_regs.X);
        SPC_regs.PC += 2;
        SPC_cycles += 4;
        break;
    }

    case 0xD9: // MOV (dp+SPC_regs.Y),SPC_regs.X
    {
        SPC700_ResolveAddress(SPC700_AM_DPY);
        SPCWrite(SPC_addL, SPC_regs.X);
        SPC_regs.PC += 2;
        SPC_cycles += 5;
        break;
    }

    case 0xC9: // MOV (abs),SPC_regs.X
    {
        SPC700_ResolveAddress(SPC700_AM_Abs);
        SPCWrite(SPC_addL, SPC_regs.X);
        SPC_regs.PC += 3;
        SPC_cycles += 5;
        break;
    }

    case 0xCB: // MOV (dp),SPC_regs.Y
    {
        SPC700_ResolveAddress(SPC700_AM_DP);
        SPCWrite(SPC_addL, SPC_regs.Y);
        SPC_regs.PC += 2;
        SPC_cycles += 5;
        break;
    }

    case 0xDB: // MOV (dp+SPC_regs.X),SPC_regs.Y
    {
        SPC700_ResolveAddress(SPC700_AM_DPX);
        SPCWrite(SPC_addL, SPC_regs.Y);
        SPC_regs.PC += 2;
        SPC_cycles += 5;
        break;
    }

    case 0xCC: // MOV SPC_regs.Y,(abs)
    {
        SPC700_ResolveAddress(SPC700_AM_Abs);
        SPCWrite(SPC_addL, SPC_regs.Y);
        SPC_regs.PC += 3;
        SPC_cycles += 5;
        break;
    }
#pragma endregion
#pragma region DataTransfer_RegToReg
    case 0x7D: // MOV SPC_regs.A,SPC_regs.X
    {
        SPC_regs.A = SPC_regs.X;
        SPC_flags.N = SPC_regs.A & 0x80;
        SPC_flags.Z = !(SPC_regs.A);
        SPC_regs.PC += 1;
        SPC_cycles += 2;
        break;
    }
    case 0xDD: // MOV SPC_regs.A,SPC_regs.Y
    {
        SPC_regs.A = SPC_regs.Y;
        SPC_flags.N = SPC_regs.A & 0x80;
        SPC_flags.Z = !(SPC_regs.A);
        SPC_regs.PC += 1;
        SPC_cycles += 2;
        break;
    }
    case 0x5D: // MOV SPC_regs.X,SPC_regs.A
    {
        SPC_regs.X = SPC_regs.A;
        SPC_flags.N = SPC_regs.X & 0x80;
        SPC_flags.Z = !(SPC_regs.X);
        SPC_regs.PC += 1;
        SPC_cycles += 2;
        break;
    }
    case 0xFD: // MOV SPC_regs.Y,SPC_regs.A
    {
        SPC_regs.Y = SPC_regs.A;
        SPC_flags.N = SPC_regs.Y & 0x80;
        SPC_flags.Z = !(SPC_regs.Y);
        SPC_regs.PC += 1;
        SPC_cycles += 2;
        break;
    }
    case 0x9D: // MOV SPC_regs.X,SPC_regs.SP
    {
        SPC_regs.X = SPC_regs.SP;
        SPC_flags.N = SPC_regs.X & 0x80;
        SPC_flags.Z = !(SPC_regs.X);
        SPC_regs.PC += 1;
        SPC_cycles += 2;
        break;
    }
    case 0xBD: // MOV SPC_regs.SP,SPC_regs.X
    {
        SPC_regs.SP = SPC_regs.X;
        SPC_regs.PC += 1;
        SPC_cycles += 2;
        break;
    }
#pragma endregion
#pragma region DataTransfer_MemToMem
    case 0xFA: // MOV dest,src
    {
        SPC700_ResolveAddress(SPC700_AM_SrcDst);
        SPCWrite(SPC_addH, SPCRead(SPC_addL));
        SPC_regs.PC += 3;
        SPC_cycles += 5;
        break;
    }
    case 0x8F: // MOV dp,imm
    {
        SPC700_ResolveAddress(SPC700_AM_SrcDst);
        SPCWrite(SPC_addH, SPC_addL & 0x00FF);
        SPC_regs.PC += 3;
        SPC_cycles += 5;
        break;
    }
#pragma endregion
#pragma region Arithmatics
    case 0x88: // ADC SPC_regs.A,imm
    {
        SPC700_ResolveAddress(SPC700_AM_Imm);
        uint8 d = SPCRead(SPC_addL);
        SPC_regs.A = SPC700_DoADC(SPC_regs.A, d);
        SPC_regs.PC += 2;
        SPC_cycles += 2;
        break;
    }
    case 0x86: // ADC SPC_regs.A,(SPC_regs.X)
    {
        SPC700_ResolveAddress(SPC700_AM_XPtr);
        uint8 d = SPCRead(SPC_addL);
        SPC_regs.A = SPC700_DoADC(SPC_regs.A, d);
        SPC_regs.PC += 1;
        SPC_cycles += 3;
        break;
    }
    case 0x84: // ADC SPC_regs.A,(dp)
    {
        SPC700_ResolveAddress(SPC700_AM_DP);
        uint8 d = SPCRead(SPC_addL);
        SPC_regs.A = SPC700_DoADC(SPC_regs.A, d);
        SPC_regs.PC += 2;
        SPC_cycles += 3;
        break;
    }
    case 0x94: // ADC SPC_regs.A,(dp+SPC_regs.X)
    {
        SPC700_ResolveAddress(SPC700_AM_DPX);
        uint8 d = SPCRead(SPC_addL);
        SPC_regs.A = SPC700_DoADC(SPC_regs.A, d);
        SPC_regs.PC += 2;
        SPC_cycles += 4;
        break;
    }
    case 0x85: // ADC SPC_regs.A,(abs)
    {
        SPC700_ResolveAddress(SPC700_AM_Abs);
        uint8 d = SPCRead(SPC_addL);
        SPC_regs.A = SPC700_DoADC(SPC_regs.A, d);
        SPC_regs.PC += 3;
        SPC_cycles += 4;
        break;
    }
    case 0x95: // ADC SPC_regs.A,(abs+SPC_regs.X)
    {
        SPC700_ResolveAddress(SPC700_AM_AbsX);
        uint8 d = SPCRead(SPC_addL);
        SPC_regs.A = SPC700_DoADC(SPC_regs.A, d);
        SPC_regs.PC += 3;
        SPC_cycles += 5;
        break;
    }
    case 0x96: // ADC SPC_regs.A,(abs+SPC_regs.Y)
    {
        SPC700_ResolveAddress(SPC700_AM_AbsY);
        uint8 d = SPCRead(SPC_addL);
        SPC_regs.A = SPC700_DoADC(SPC_regs.A, d);
        SPC_regs.PC += 3;
        SPC_cycles += 5;
        break;
    }

    case 0x87: // ADC SPC_regs.A,((dp+x))
    {
        SPC700_ResolveAddress(SPC700_AM_DPXPtr);
        uint8 d = SPCRead(SPC_addL);
        SPC_regs.A = SPC700_DoADC(SPC_regs.A, d);
        SPC_regs.PC += 2;
        SPC_cycles += 6;
        break;
    }
    case 0x97: // ADC SPC_regs.A,((dp)+y)
    {
        SPC700_ResolveAddress(SPC700_AM_DPPtrY);
        uint8 d = SPCRead(SPC_addL);
        SPC_regs.A = SPC700_DoADC(SPC_regs.A, d);
        SPC_regs.PC += 2;
        SPC_cycles += 6;
        break;
    }

    case 0x99: // ADC (x),(y)
    {

        SPC700_ResolveAddress(SPC700_AM_YPtr);
        uint8 y = SPCRead(SPC_addL);
        SPC700_ResolveAddress(SPC700_AM_XPtr);
        uint8 x = SPCRead(SPC_addL);
        SPCWrite(SPC_addL, SPC700_DoADC(x, y));
        SPC_regs.PC += 1;
        SPC_cycles += 5;
        break;
    }
    case 0x89: // ADC dp,dp
    {

        SPC700_ResolveAddress(SPC700_AM_SrcDst);
        uint8 x = SPCRead(SPC_addL);
        uint8 y = SPCRead(SPC_addH);
        SPCWrite(SPC_addH, SPC700_DoADC(x, y));
        SPC_regs.PC += 3;
        SPC_cycles += 6;
        break;
    }
    case 0x98: // ADC dp,imm
    {
        SPC700_ResolveAddress(SPC700_AM_SrcDst);
        uint8 x = SPC_addL;
        uint8 y = SPCRead(SPC_addH);
        SPCWrite(SPC_addH, SPC700_DoADC(x, y));
        SPC_regs.PC += 3;
        SPC_cycles += 5;
        break;
    }

    case 0xA8: // SBC SPC_regs.A,imm
    {
        SPC700_ResolveAddress(SPC700_AM_Imm);
        uint8 d = SPCRead(SPC_addL);
        SPC_regs.A = SPC700_DoSBC(SPC_regs.A, d);
        SPC_regs.PC += 2;
        SPC_cycles += 2;
        break;
    }
    case 0xA6: // SBC SPC_regs.A,(SPC_regs.X)
    {
        SPC700_ResolveAddress(SPC700_AM_XPtr);
        uint8 d = SPCRead(SPC_addL);
        SPC_regs.A = SPC700_DoSBC(SPC_regs.A, d);
        SPC_regs.PC += 1;
        SPC_cycles += 3;
        break;
    }
    case 0xA4: // SBC SPC_regs.A,(dp)
    {
        SPC700_ResolveAddress(SPC700_AM_DP);
        uint8 d = SPCRead(SPC_addL);
        SPC_regs.A = SPC700_DoSBC(SPC_regs.A, d);
        SPC_regs.PC += 2;
        SPC_cycles += 3;
        break;
    }
    case 0xB4: // SBC SPC_regs.A,(dp+SPC_regs.X)
    {
        SPC700_ResolveAddress(SPC700_AM_DPX);
        uint8 d = SPCRead(SPC_addL);
        SPC_regs.A = SPC700_DoSBC(SPC_regs.A, d);
        SPC_regs.PC += 2;
        SPC_cycles += 4;
        break;
    }
    case 0xA5: // SBC SPC_regs.A,(abs)
    {
        SPC700_ResolveAddress(SPC700_AM_Abs);
        uint8 d = SPCRead(SPC_addL);
        SPC_regs.A = SPC700_DoSBC(SPC_regs.A, d);
        SPC_regs.PC += 3;
        SPC_cycles += 4;
        break;
    }
    case 0xB5: // SBC SPC_regs.A,(abs+SPC_regs.X)
    {
        SPC700_ResolveAddress(SPC700_AM_AbsX);
        uint8 d = SPCRead(SPC_addL);
        SPC_regs.A = SPC700_DoSBC(SPC_regs.A, d);
        SPC_regs.PC += 3;
        SPC_cycles += 5;
        break;
    }
    case 0xB6: // SBC SPC_regs.A,(abs+SPC_regs.Y)
    {
        SPC700_ResolveAddress(SPC700_AM_AbsY);
        uint8 d = SPCRead(SPC_addL);
        SPC_regs.A = SPC700_DoSBC(SPC_regs.A, d);
        SPC_regs.PC += 3;
        SPC_cycles += 5;
        break;
    }

    case 0xA7: // SBC SPC_regs.A,((dp+x))
    {
        SPC700_ResolveAddress(SPC700_AM_DPXPtr);
        uint8 d = SPCRead(SPC_addL);
        SPC_regs.A = SPC700_DoSBC(SPC_regs.A, d);
        SPC_regs.PC += 2;
        SPC_cycles += 6;
        break;
    }
    case 0xB7: // SBC SPC_regs.A,((dp)+y)
    {
        SPC700_ResolveAddress(SPC700_AM_DPPtrY);
        uint8 d = SPCRead(SPC_addL);
        SPC_regs.A = SPC700_DoSBC(SPC_regs.A, d);
        SPC_regs.PC += 2;
        SPC_cycles += 6;
        break;
    }

    case 0xB9: // SBC (x),(y)
    {

        SPC700_ResolveAddress(SPC700_AM_YPtr);
        uint8 y = SPCRead(SPC_addL);
        SPC700_ResolveAddress(SPC700_AM_XPtr);
        uint8 x = SPCRead(SPC_addL);
        SPCWrite(SPC_addL, SPC700_DoSBC(x, y));
        SPC_regs.PC += 1;
        SPC_cycles += 5;
        break;
    }
    case 0xA9: // SBC dp,dp
    {

        SPC700_ResolveAddress(SPC700_AM_SrcDst);
        uint8 x = SPCRead(SPC_addH);
        uint8 y = SPCRead(SPC_addL);
        SPCWrite(SPC_addH, SPC700_DoSBC(x, y));
        SPC_regs.PC += 3;
        SPC_cycles += 6;
        break;
    }
    case 0xB8: // SBC dp,imm
    {
        SPC700_ResolveAddress(SPC700_AM_SrcDst);
        uint8 x = SPCRead(SPC_addH);
        uint8 y = SPC_addL;
        SPCWrite(SPC_addH, SPC700_DoSBC(x, y));
        SPC_regs.PC += 3;
        SPC_cycles += 5;
        break;
    }

    case 0x68: // CMP SPC_regs.A,imm
    {
        SPC700_ResolveAddress(SPC700_AM_Imm);
        uint8 d = SPCRead(SPC_addL);
        SPC700_DoCMP(SPC_regs.A, d);
        SPC_regs.PC += 2;
        SPC_cycles += 2;
        break;
    }
    case 0x66: // CMP SPC_regs.A,(SPC_regs.X)
    {
        SPC700_ResolveAddress(SPC700_AM_XPtr);
        uint8 d = SPCRead(SPC_addL);
        SPC700_DoCMP(SPC_regs.A, d);
        SPC_regs.PC += 1;
        SPC_cycles += 3;
        break;
    }
    case 0x64: // CMP SPC_regs.A,(dp)
    {
        SPC700_ResolveAddress(SPC700_AM_DP);
        uint8 d = SPCRead(SPC_addL);
        SPC700_DoCMP(SPC_regs.A, d);
        SPC_regs.PC += 2;
        SPC_cycles += 3;
        break;
    }
    case 0x74: // CMP SPC_regs.A,(dp+SPC_regs.X)
    {
        SPC700_ResolveAddress(SPC700_AM_DPX);
        uint8 d = SPCRead(SPC_addL);
        SPC700_DoCMP(SPC_regs.A, d);
        SPC_regs.PC += 2;
        SPC_cycles += 4;
        break;
    }
    case 0x65: // CMP SPC_regs.A,(abs)
    {
        SPC700_ResolveAddress(SPC700_AM_Abs);
        uint8 d = SPCRead(SPC_addL);
        SPC700_DoCMP(SPC_regs.A, d);
        SPC_regs.PC += 3;
        SPC_cycles += 4;
        break;
    }
    case 0x75: // CMP SPC_regs.A,(abs+SPC_regs.X)
    {
        SPC700_ResolveAddress(SPC700_AM_AbsX);
        uint8 d = SPCRead(SPC_addL);
        SPC700_DoCMP(SPC_regs.A, d);
        SPC_regs.PC += 3;
        SPC_cycles += 5;
        break;
    }
    case 0x76: // CMP SPC_regs.A,(abs+SPC_regs.Y)
    {
        SPC700_ResolveAddress(SPC700_AM_AbsY);
        uint8 d = SPCRead(SPC_addL);
        SPC700_DoCMP(SPC_regs.A, d);
        SPC_regs.PC += 3;
        SPC_cycles += 5;
        break;
    }

    case 0x67: // CMP SPC_regs.A,((dp+x))
    {
        SPC700_ResolveAddress(SPC700_AM_DPXPtr);
        uint8 d = SPCRead(SPC_addL);
        SPC700_DoCMP(SPC_regs.A, d);
        SPC_regs.PC += 2;
        SPC_cycles += 6;
        break;
    }
    case 0x77: // CMP SPC_regs.A,((dp)+y)
    {
        SPC700_ResolveAddress(SPC700_AM_DPPtrY);
        uint8 d = SPCRead(SPC_addL);
        SPC700_DoCMP(SPC_regs.A, d);
        SPC_regs.PC += 2;
        SPC_cycles += 6;
        break;
    }

    case 0x79: // CMP (x),(y)
    {

        SPC700_ResolveAddress(SPC700_AM_YPtr);
        uint8 y = SPCRead(SPC_addL);
        SPC700_ResolveAddress(SPC700_AM_XPtr);
        uint8 x = SPCRead(SPC_addL);
        SPC700_DoCMP(x, y);
        SPC_regs.PC += 1;
        SPC_cycles += 5;
        break;
    }
    case 0x69: // CMP dp,dp
    {

        SPC700_ResolveAddress(SPC700_AM_SrcDst);
        uint8 x = SPCRead(SPC_addH);
        uint8 y = SPCRead(SPC_addL);
        SPC700_DoCMP(x, y);
        SPC_regs.PC += 3;
        SPC_cycles += 6;
        break;
    }
    case 0x78: // CMP dp,imm
    {
        SPC700_ResolveAddress(SPC700_AM_SrcDst);
        uint8 x = SPCRead(SPC_addH);
        uint8 y = SPC_addL;
        SPC700_DoCMP(x, y);
        SPC_regs.PC += 3;
        SPC_cycles += 5;
        break;
    }

    case 0xC8: // CMP SPC_regs.X,imm
    {
        SPC700_ResolveAddress(SPC700_AM_Imm);
        uint8 d = SPCRead(SPC_addL);
        SPC700_DoCMP(SPC_regs.X, d);
        SPC_regs.PC += 2;
        SPC_cycles += 2;
        break;
    }
    case 0x3E: // CMP SPC_regs.X,(dp)
    {
        SPC700_ResolveAddress(SPC700_AM_DP);
        uint8 d = SPCRead(SPC_addL);
        SPC700_DoCMP(SPC_regs.X, d);
        SPC_regs.PC += 2;
        SPC_cycles += 3;
        break;
    }
    case 0x1E: // CMP SPC_regs.X,(abs)
    {
        SPC700_ResolveAddress(SPC700_AM_Abs);
        uint8 d = SPCRead(SPC_addL);
        SPC700_DoCMP(SPC_regs.X, d);
        SPC_regs.PC += 3;
        SPC_cycles += 4;
        break;
    }

    case 0xAD: // CMP SPC_regs.Y,imm
    {
        SPC700_ResolveAddress(SPC700_AM_Imm);
        uint8 d = SPCRead(SPC_addL);
        SPC700_DoCMP(SPC_regs.Y, d);
        SPC_regs.PC += 2;
        SPC_cycles += 2;
        break;
    }
    case 0x7E: // CMP SPC_regs.Y,(dp)
    {
        SPC700_ResolveAddress(SPC700_AM_DP);
        uint8 d = SPCRead(SPC_addL);
        SPC700_DoCMP(SPC_regs.Y, d);
        SPC_regs.PC += 2;
        SPC_cycles += 3;
        break;
    }
    case 0x5E: // CMP SPC_regs.Y,(abs)
    {
        SPC700_ResolveAddress(SPC700_AM_Abs);
        uint8 d = SPCRead(SPC_addL);
        SPC700_DoCMP(SPC_regs.Y, d);
        SPC_regs.PC += 3;
        SPC_cycles += 4;
        break;
    }
#pragma endregion
#pragma region LogicOperations
    case 0x28: // AND SPC_regs.A,imm
    {
        SPC700_ResolveAddress(SPC700_AM_Imm);
        uint8 d = SPCRead(SPC_addL);
        SPC_regs.A = SPC_regs.A & d;
        SPC_flags.Z = !(SPC_regs.A);
        SPC_flags.N = SPC_regs.A & 0x80;
        SPC_regs.PC += 2;
        SPC_cycles += 2;
        break;
    }
    case 0x26: // AND SPC_regs.A,(SPC_regs.X)
    {
        SPC700_ResolveAddress(SPC700_AM_XPtr);
        uint8 d = SPCRead(SPC_addL);
        SPC_regs.A = SPC_regs.A & d;
        SPC_flags.Z = !(SPC_regs.A);
        SPC_flags.N = SPC_regs.A & 0x80;
        SPC_regs.PC += 1;
        SPC_cycles += 3;
        break;
    }
    case 0x24: // AND SPC_regs.A,(dp)
    {
        SPC700_ResolveAddress(SPC700_AM_DP);
        uint8 d = SPCRead(SPC_addL);
        SPC_regs.A = SPC_regs.A & d;
        SPC_flags.Z = !(SPC_regs.A);
        SPC_flags.N = SPC_regs.A & 0x80;
        SPC_regs.PC += 2;
        SPC_cycles += 3;
        break;
    }
    case 0x34: // AND SPC_regs.A,(dp+SPC_regs.X)
    {
        SPC700_ResolveAddress(SPC700_AM_DPX);
        uint8 d = SPCRead(SPC_addL);
        SPC_regs.A = SPC_regs.A & d;
        SPC_flags.Z = !(SPC_regs.A);
        SPC_flags.N = SPC_regs.A & 0x80;
        SPC_regs.PC += 2;
        SPC_cycles += 4;
        break;
    }
    case 0x25: // AND SPC_regs.A,(abs)
    {
        SPC700_ResolveAddress(SPC700_AM_Abs);
        uint8 d = SPCRead(SPC_addL);
        SPC_regs.A = SPC_regs.A & d;
        SPC_flags.Z = !(SPC_regs.A);
        SPC_flags.N = SPC_regs.A & 0x80;
        SPC_regs.PC += 3;
        SPC_cycles += 4;
        break;
    }
    case 0x35: // AND SPC_regs.A,(abs+SPC_regs.X)
    {
        SPC700_ResolveAddress(SPC700_AM_AbsX);
        uint8 d = SPCRead(SPC_addL);
        SPC_regs.A = SPC_regs.A & d;
        SPC_flags.Z = !(SPC_regs.A);
        SPC_flags.N = SPC_regs.A & 0x80;
        SPC_regs.PC += 3;
        SPC_cycles += 5;
        break;
    }
    case 0x36: // AND SPC_regs.A,(abs+SPC_regs.Y)
    {
        SPC700_ResolveAddress(SPC700_AM_AbsY);
        uint8 d = SPCRead(SPC_addL);
        SPC_regs.A = SPC_regs.A & d;
        SPC_flags.Z = !(SPC_regs.A);
        SPC_flags.N = SPC_regs.A & 0x80;
        SPC_regs.PC += 3;
        SPC_cycles += 5;
        break;
    }

    case 0x27: // AND SPC_regs.A,((dp+x))
    {
        SPC700_ResolveAddress(SPC700_AM_DPXPtr);
        uint8 d = SPCRead(SPC_addL);
        SPC_regs.A = SPC_regs.A & d;
        SPC_flags.Z = !(SPC_regs.A);
        SPC_flags.N = SPC_regs.A & 0x80;
        SPC_regs.PC += 2;
        SPC_cycles += 6;
        break;
    }
    case 0x37: // AND SPC_regs.A,((dp)+y)
    {
        SPC700_ResolveAddress(SPC700_AM_DPPtrY);
        uint8 d = SPCRead(SPC_addL);
        SPC_regs.A = SPC_regs.A & d;
        SPC_flags.Z = !(SPC_regs.A);
        SPC_flags.N = SPC_regs.A & 0x80;
        SPC_regs.PC += 2;
        SPC_cycles += 6;
        break;
    }

    case 0x39: // AND (x),(y)
    {

        SPC700_ResolveAddress(SPC700_AM_YPtr);
        uint8 y = SPCRead(SPC_addL);
        SPC700_ResolveAddress(SPC700_AM_XPtr);
        uint8 x = SPCRead(SPC_addL);
        uint8 t = x & y;
        SPC_flags.Z = !(t);
        SPC_flags.N = t & 0x80;
        SPCWrite(SPC_addL, t);
        SPC_regs.PC += 1;
        SPC_cycles += 5;
        break;
    }
    case 0x29: // AND dp,dp
    {

        SPC700_ResolveAddress(SPC700_AM_SrcDst);
        uint8 x = SPCRead(SPC_addL);
        uint8 y = SPCRead(SPC_addH);
        uint8 t = x & y;
        SPC_flags.Z = !(t);
        SPC_flags.N = t & 0x80;
        SPCWrite(SPC_addH, t);
        SPC_regs.PC += 3;
        SPC_cycles += 6;
        break;
    }
    case 0x38: // AND dp,imm
    {
        SPC700_ResolveAddress(SPC700_AM_SrcDst);
        uint8 x = SPC_addL;
        uint8 y = SPCRead(SPC_addH);
        uint8 t = x & y;
        SPC_flags.Z = !(t);
        SPC_flags.N = t & 0x80;
        SPCWrite(SPC_addH, t);
        SPC_regs.PC += 3;
        SPC_cycles += 5;
        break;
    }

    case 0x08: // OR SPC_regs.A,imm
    {
        SPC700_ResolveAddress(SPC700_AM_Imm);
        uint8 d = SPCRead(SPC_addL);
        SPC_regs.A = SPC_regs.A | d;
        SPC_flags.Z = !(SPC_regs.A);
        SPC_flags.N = SPC_regs.A & 0x80;
        SPC_regs.PC += 2;
        SPC_cycles += 2;
        break;
    }
    case 0x06: // OR SPC_regs.A,(SPC_regs.X)
    {
        SPC700_ResolveAddress(SPC700_AM_XPtr);
        uint8 d = SPCRead(SPC_addL);
        SPC_regs.A = SPC_regs.A | d;
        SPC_flags.Z = !(SPC_regs.A);
        SPC_flags.N = SPC_regs.A & 0x80;
        SPC_regs.PC += 1;
        SPC_cycles += 3;
        break;
    }
    case 0x04: // OR SPC_regs.A,(dp)
    {
        SPC700_ResolveAddress(SPC700_AM_DP);
        uint8 d = SPCRead(SPC_addL);
        SPC_regs.A = SPC_regs.A | d;
        SPC_flags.Z = !(SPC_regs.A);
        SPC_flags.N = SPC_regs.A & 0x80;
        SPC_regs.PC += 2;
        SPC_cycles += 3;
        break;
    }
    case 0x14: // OR SPC_regs.A,(dp+SPC_regs.X)
    {
        SPC700_ResolveAddress(SPC700_AM_DPX);
        uint8 d = SPCRead(SPC_addL);
        SPC_regs.A = SPC_regs.A | d;
        SPC_flags.Z = !(SPC_regs.A);
        SPC_flags.N = SPC_regs.A & 0x80;
        SPC_regs.PC += 2;
        SPC_cycles += 4;
        break;
    }
    case 0x05: // OR SPC_regs.A,(abs)
    {
        SPC700_ResolveAddress(SPC700_AM_Abs);
        uint8 d = SPCRead(SPC_addL);
        SPC_regs.A = SPC_regs.A | d;
        SPC_flags.Z = !(SPC_regs.A);
        SPC_flags.N = SPC_regs.A & 0x80;
        SPC_regs.PC += 3;
        SPC_cycles += 4;
        break;
    }
    case 0x15: // OR SPC_regs.A,(abs+SPC_regs.X)
    {
        SPC700_ResolveAddress(SPC700_AM_AbsX);
        uint8 d = SPCRead(SPC_addL);
        SPC_regs.A = SPC_regs.A | d;
        SPC_flags.Z = !(SPC_regs.A);
        SPC_flags.N = SPC_regs.A & 0x80;
        SPC_regs.PC += 3;
        SPC_cycles += 5;
        break;
    }
    case 0x16: // OR SPC_regs.A,(abs+SPC_regs.Y)
    {
        SPC700_ResolveAddress(SPC700_AM_AbsY);
        uint8 d = SPCRead(SPC_addL);
        SPC_regs.A = SPC_regs.A | d;
        SPC_flags.Z = !(SPC_regs.A);
        SPC_flags.N = SPC_regs.A & 0x80;
        SPC_regs.PC += 3;
        SPC_cycles += 5;
        break;
    }

    case 0x07: // OR SPC_regs.A,((dp+x))
    {
        SPC700_ResolveAddress(SPC700_AM_DPXPtr);
        uint8 d = SPCRead(SPC_addL);
        SPC_regs.A = SPC_regs.A | d;
        SPC_flags.Z = !(SPC_regs.A);
        SPC_flags.N = SPC_regs.A & 0x80;
        SPC_regs.PC += 2;
        SPC_cycles += 6;
        break;
    }
    case 0x17: // OR SPC_regs.A,((dp)+y)
    {
        SPC700_ResolveAddress(SPC700_AM_DPPtrY);
        uint8 d = SPCRead(SPC_addL);
        SPC_regs.A = SPC_regs.A | d;
        SPC_flags.Z = !(SPC_regs.A);
        SPC_flags.N = SPC_regs.A & 0x80;
        SPC_regs.PC += 2;
        SPC_cycles += 6;
        break;
    }

    case 0x19: // OR (x),(y)
    {

        SPC700_ResolveAddress(SPC700_AM_YPtr);
        uint8 y = SPCRead(SPC_addL);
        SPC700_ResolveAddress(SPC700_AM_XPtr);
        uint8 x = SPCRead(SPC_addL);
        uint8 t = x | y;
        SPC_flags.Z = !(t);
        SPC_flags.N = t & 0x80;
        SPCWrite(SPC_addL, t);
        SPC_regs.PC += 1;
        SPC_cycles += 5;
        break;
    }
    case 0x09: // OR dp,dp
    {

        SPC700_ResolveAddress(SPC700_AM_SrcDst);
        uint8 x = SPCRead(SPC_addL);
        uint8 y = SPCRead(SPC_addH);
        uint8 t = x | y;
        SPC_flags.Z = !(t);
        SPC_flags.N = t & 0x80;
        SPCWrite(SPC_addH, t);
        SPC_regs.PC += 3;
        SPC_cycles += 6;
        break;
    }
    case 0x18: // OR dp,imm
    {
        SPC700_ResolveAddress(SPC700_AM_SrcDst);
        uint8 x = SPC_addL;
        uint8 y = SPCRead(SPC_addH);
        uint8 t = x | y;
        SPC_flags.Z = !(t);
        SPC_flags.N = t & 0x80;
        SPCWrite(SPC_addH, t);
        SPC_regs.PC += 3;
        SPC_cycles += 5;
        break;
    }

    case 0x48: // EOR SPC_regs.A,imm
    {
        SPC700_ResolveAddress(SPC700_AM_Imm);
        uint8 d = SPCRead(SPC_addL);
        SPC_regs.A = SPC_regs.A ^ d;
        SPC_flags.Z = !(SPC_regs.A);
        SPC_flags.N = SPC_regs.A & 0x80;
        SPC_regs.PC += 2;
        SPC_cycles += 2;
        break;
    }
    case 0x46: // EOR SPC_regs.A,(SPC_regs.X)
    {
        SPC700_ResolveAddress(SPC700_AM_XPtr);
        uint8 d = SPCRead(SPC_addL);
        SPC_regs.A = SPC_regs.A ^ d;
        SPC_flags.Z = !(SPC_regs.A);
        SPC_flags.N = SPC_regs.A & 0x80;
        SPC_regs.PC += 1;
        SPC_cycles += 3;
        break;
    }
    case 0x44: // EOR SPC_regs.A,(dp)
    {
        SPC700_ResolveAddress(SPC700_AM_DP);
        uint8 d = SPCRead(SPC_addL);
        SPC_regs.A = SPC_regs.A ^ d;
        SPC_flags.Z = !(SPC_regs.A);
        SPC_flags.N = SPC_regs.A & 0x80;
        SPC_regs.PC += 2;
        SPC_cycles += 3;
        break;
    }
    case 0x54: // EOR SPC_regs.A,(dp+SPC_regs.X)
    {
        SPC700_ResolveAddress(SPC700_AM_DPX);
        uint8 d = SPCRead(SPC_addL);
        SPC_regs.A = SPC_regs.A ^ d;
        SPC_flags.Z = !(SPC_regs.A);
        SPC_flags.N = SPC_regs.A & 0x80;
        SPC_regs.PC += 2;
        SPC_cycles += 4;
        break;
    }
    case 0x45: // EOR SPC_regs.A,(abs)
    {
        SPC700_ResolveAddress(SPC700_AM_Abs);
        uint8 d = SPCRead(SPC_addL);
        SPC_regs.A = SPC_regs.A ^ d;
        SPC_flags.Z = !(SPC_regs.A);
        SPC_flags.N = SPC_regs.A & 0x80;
        SPC_regs.PC += 3;
        SPC_cycles += 4;
        break;
    }
    case 0x55: // EOR SPC_regs.A,(abs+SPC_regs.X)
    {
        SPC700_ResolveAddress(SPC700_AM_AbsX);
        uint8 d = SPCRead(SPC_addL);
        SPC_regs.A = SPC_regs.A ^ d;
        SPC_flags.Z = !(SPC_regs.A);
        SPC_flags.N = SPC_regs.A & 0x80;
        SPC_regs.PC += 3;
        SPC_cycles += 5;
        break;
    }
    case 0x56: // EOR SPC_regs.A,(abs+SPC_regs.Y)
    {
        SPC700_ResolveAddress(SPC700_AM_AbsY);
        uint8 d = SPCRead(SPC_addL);
        SPC_regs.A = SPC_regs.A ^ d;
        SPC_flags.Z = !(SPC_regs.A);
        SPC_flags.N = SPC_regs.A & 0x80;
        SPC_regs.PC += 3;
        SPC_cycles += 5;
        break;
    }

    case 0x47: // EOR SPC_regs.A,((dp+x))
    {
        SPC700_ResolveAddress(SPC700_AM_DPXPtr);
        uint8 d = SPCRead(SPC_addL);
        SPC_regs.A = SPC_regs.A ^ d;
        SPC_flags.Z = !(SPC_regs.A);
        SPC_flags.N = SPC_regs.A & 0x80;
        SPC_regs.PC += 2;
        SPC_cycles += 6;
        break;
    }
    case 0x57: // EOR SPC_regs.A,((dp)+y)
    {
        SPC700_ResolveAddress(SPC700_AM_DPPtrY);
        uint8 d = SPCRead(SPC_addL);
        SPC_regs.A = SPC_regs.A ^ d;
        SPC_flags.Z = !(SPC_regs.A);
        SPC_flags.N = SPC_regs.A & 0x80;
        SPC_regs.PC += 2;
        SPC_cycles += 6;
        break;
    }

    case 0x59: // EOR (x),(y)
    {

        SPC700_ResolveAddress(SPC700_AM_YPtr);
        uint8 y = SPCRead(SPC_addL);
        SPC700_ResolveAddress(SPC700_AM_XPtr);
        uint8 x = SPCRead(SPC_addL);
        uint8 t = x ^ y;
        SPC_flags.Z = !(t);
        SPC_flags.N = t & 0x80;
        SPCWrite(SPC_addL, t);
        SPC_regs.PC += 1;
        SPC_cycles += 5;
        break;
    }
    case 0x49: // EOR dp,dp
    {

        SPC700_ResolveAddress(SPC700_AM_SrcDst);
        uint8 x = SPCRead(SPC_addL);
        uint8 y = SPCRead(SPC_addH);
        uint8 t = x ^ y;
        SPC_flags.Z = !(t);
        SPC_flags.N = t & 0x80;
        SPCWrite(SPC_addH, t);
        SPC_regs.PC += 3;
        SPC_cycles += 6;
        break;
    }
    case 0x58: // EOR dp,imm
    {
        SPC700_ResolveAddress(SPC700_AM_SrcDst);
        uint8 x = SPC_addL;
        uint8 y = SPCRead(SPC_addH);
        uint8 t = x ^ y;
        SPC_flags.Z = !(t);
        SPC_flags.N = t & 0x80;
        SPCWrite(SPC_addH, t);
        SPC_regs.PC += 3;
        SPC_cycles += 5;
        break;
    }
#pragma endregion
#pragma region IncDec
    case 0xBC: // INC SPC_regs.A
    {
        SPC_regs.A++;
        SPC_flags.Z = !(SPC_regs.A);
        SPC_flags.N = SPC_regs.A & 0x80;
        SPC_regs.PC += 1;
        SPC_cycles += 2;
        break;
    }

    case 0xAB: // INC (dp)
    {
        SPC700_ResolveAddress(SPC700_AM_DP);
        uint8 t = SPCRead(SPC_addL);
        t++;
        SPC_flags.Z = !(t);
        SPC_flags.N = t & 0x80;
        SPCWrite(SPC_addL, t);
        SPC_regs.PC += 2;
        SPC_cycles += 4;
        break;
    }

    case 0xBB: // INC (dp+x)
    {
        SPC700_ResolveAddress(SPC700_AM_DPX);
        uint8 t = SPCRead(SPC_addL);
        t++;
        SPC_flags.Z = !(t);
        SPC_flags.N = t & 0x80;
        SPCWrite(SPC_addL, t);
        SPC_regs.PC += 2;
        SPC_cycles += 5;
        break;
    }

    case 0xAC: // INC (abs)
    {
        SPC700_ResolveAddress(SPC700_AM_Abs);
        uint8 t = SPCRead(SPC_addL);
        t++;
        SPC_flags.Z = !(t);
        SPC_flags.N = t & 0x80;
        SPCWrite(SPC_addL, t);
        SPC_regs.PC += 3;
        SPC_cycles += 5;
        break;
    }
    case 0x3D: // INC SPC_regs.X
    {
        SPC_regs.X++;
        SPC_flags.Z = !(SPC_regs.X);
        SPC_flags.N = SPC_regs.X & 0x80;
        SPC_regs.PC += 1;
        SPC_cycles += 2;
        break;
    }
    case 0xFC: // INC SPC_regs.Y
    {
        SPC_regs.Y++;
        SPC_flags.Z = !(SPC_regs.Y);
        SPC_flags.N = SPC_regs.Y & 0x80;
        SPC_regs.PC += 1;
        SPC_cycles += 2;
        break;
    }

    case 0x9C: // DEC SPC_regs.A
    {
        SPC_regs.A--;
        SPC_flags.Z = !(SPC_regs.A);
        SPC_flags.N = SPC_regs.A & 0x80;
        SPC_regs.PC += 1;
        SPC_cycles += 2;
        break;
    }

    case 0x8B: // DEC (dp)
    {
        SPC700_ResolveAddress(SPC700_AM_DP);
        uint8 t = SPCRead(SPC_addL);
        t--;
        SPC_flags.Z = !(t);
        SPC_flags.N = t & 0x80;
        SPCWrite(SPC_addL, t);
        SPC_regs.PC += 2;
        SPC_cycles += 4;
        break;
    }

    case 0x9B: // DEC (dp+x)
    {
        SPC700_ResolveAddress(SPC700_AM_DPX);
        uint8 t = SPCRead(SPC_addL);
        t--;
        SPC_flags.Z = !(t);
        SPC_flags.N = t & 0x80;
        SPCWrite(SPC_addL, t);
        SPC_regs.PC += 2;
        SPC_cycles += 5;
        break;
    }

    case 0x8C: // DEC (abs)
    {
        SPC700_ResolveAddress(SPC700_AM_Abs);
        uint8 t = SPCRead(SPC_addL);
        t--;
        SPC_flags.Z = !(t);
        SPC_flags.N = t & 0x80;
        SPCWrite(SPC_addL, t);
        SPC_regs.PC += 3;
        SPC_cycles += 5;
        break;
    }
    case 0x1D: // DEC SPC_regs.X
    {
        SPC_regs.X--;
        SPC_flags.Z = !(SPC_regs.X);
        SPC_flags.N = SPC_regs.X & 0x80;
        SPC_regs.PC += 1;
        SPC_cycles += 2;
        break;
    }
    case 0xDC: // DEC SPC_regs.Y
    {
        SPC_regs.Y--;
        SPC_flags.Z = !(SPC_regs.Y);
        SPC_flags.N = SPC_regs.Y & 0x80;
        SPC_regs.PC += 1;
        SPC_cycles += 2;
        break;
    }
#pragma endregion

#pragma region ShiftRotate
    case 0x1C: // ASL SPC_regs.A
    {
        SPC_flags.C = SPC_regs.A & 0x80;
        SPC_regs.A = SPC_regs.A << 1;
        SPC_flags.Z = !(SPC_regs.A);
        SPC_flags.N = SPC_regs.A & 0x80;
        SPC_regs.PC += 1;
        SPC_cycles += 2;
        break;
    }
    case 0x0B: // ASL (dp)
    {
        SPC700_ResolveAddress(SPC700_AM_DP);
        uint8 t = SPCRead(SPC_addL);
        SPC_flags.C = t & 0x80;
        t = t << 1;
        SPC_flags.Z = !(t);
        SPC_flags.N = t & 0x80;
        SPCWrite(SPC_addL, t);
        SPC_regs.PC += 2;
        SPC_cycles += 4;
        break;
    }
    case 0x1B: // ASL (dp+x)
    {
        SPC700_ResolveAddress(SPC700_AM_DPX);
        uint8 t = SPCRead(SPC_addL);
        SPC_flags.C = t & 0x80;
        t = t << 1;
        SPC_flags.Z = !(t);
        SPC_flags.N = t & 0x80;
        SPCWrite(SPC_addL, t);
        SPC_regs.PC += 2;
        SPC_cycles += 5;
        break;
    }

    case 0x0C: // ASL (abs)
    {
        SPC700_ResolveAddress(SPC700_AM_Abs);
        uint8 t = SPCRead(SPC_addL);
        SPC_flags.C = t & 0x80;
        t = t << 1;
        SPC_flags.Z = !(t);
        SPC_flags.N = t & 0x80;
        SPCWrite(SPC_addL, t);
        SPC_regs.PC += 3;
        SPC_cycles += 5;
        break;
    }

    case 0x5C: // LSR SPC_regs.A
    {
        SPC_flags.C = SPC_regs.A & 0x01;
        SPC_regs.A = SPC_regs.A >> 1;

        SPC_flags.Z = !(SPC_regs.A);
        SPC_flags.N = SPC_regs.A & 0x80;
        SPC_regs.PC += 1;
        SPC_cycles += 2;
        break;
    }
    case 0x4B: // LSR (dp)
    {
        SPC700_ResolveAddress(SPC700_AM_DP);
        uint8 t = SPCRead(SPC_addL);
        SPC_flags.C = t & 0x01;
        t = t >> 1;

        SPC_flags.Z = !(t);
        SPC_flags.N = t & 0x80;
        SPCWrite(SPC_addL, t);
        SPC_regs.PC += 2;
        SPC_cycles += 4;
        break;
    }
    case 0x5B: // LSR (dp+x)
    {
        SPC700_ResolveAddress(SPC700_AM_DPX);
        uint8 t = SPCRead(SPC_addL);
        SPC_flags.C = t & 0x01;
        t = t >> 1;

        SPC_flags.Z = !(t);
        SPC_flags.N = t & 0x80;
        SPCWrite(SPC_addL, t);
        SPC_regs.PC += 2;
        SPC_cycles += 5;
        break;
    }

    case 0x4C: // LSR (abs)
    {
        SPC700_ResolveAddress(SPC700_AM_Abs);
        uint8 t = SPCRead(SPC_addL);
        SPC_flags.C = t & 0x01;
        t = t >> 1;

        SPC_flags.Z = !(t);
        SPC_flags.N = t & 0x80;
        SPCWrite(SPC_addL, t);
        SPC_regs.PC += 3;
        SPC_cycles += 5;
        break;
    }

    case 0x3C: // ROL SPC_regs.A
    {
        bool c = SPC_flags.C;
        SPC_flags.C = SPC_regs.A & 0x80;
        SPC_regs.A = SPC_regs.A << 1;
        SPC_regs.A |= c ? 0x01 : 0x00;
        SPC_flags.Z = !(SPC_regs.A);
        SPC_flags.N = SPC_regs.A & 0x80;
        SPC_regs.PC += 1;
        SPC_cycles += 2;
        break;
    }
    case 0x2B: // ROL (dp)
    {
        SPC700_ResolveAddress(SPC700_AM_DP);
        uint8 t = SPCRead(SPC_addL);
        bool c = SPC_flags.C;
        SPC_flags.C = t & 0x80;
        t = t << 1;
        t |= c ? 0x01 : 0x00;
        SPC_flags.Z = !(t);
        SPC_flags.N = t & 0x80;
        SPCWrite(SPC_addL, t);
        SPC_regs.PC += 2;
        SPC_cycles += 4;
        break;
    }
    case 0x3B: // ROL (dp+x)
    {
        SPC700_ResolveAddress(SPC700_AM_DPX);
        uint8 t = SPCRead(SPC_addL);
        bool c = SPC_flags.C;
        SPC_flags.C = t & 0x80;
        t = t << 1;
        t |= c ? 0x01 : 0x00;
        SPC_flags.Z = !(t);
        SPC_flags.N = t & 0x80;
        SPCWrite(SPC_addL, t);
        SPC_regs.PC += 2;
        SPC_cycles += 5;
        break;
    }

    case 0x2C: // ROL (abs)
    {
        SPC700_ResolveAddress(SPC700_AM_Abs);
        uint8 t = SPCRead(SPC_addL);
        bool c = SPC_flags.C;
        SPC_flags.C = t & 0x80;
        t = t << 1;
        t |= c ? 0x01 : 0x00;
        SPC_flags.Z = !(t);
        SPC_flags.N = t & 0x80;
        SPCWrite(SPC_addL, t);
        SPC_regs.PC += 3;
        SPC_cycles += 5;
        break;
    }

    case 0x7C: // ROR SPC_regs.A
    {
        bool c = SPC_flags.C;
        SPC_flags.C = SPC_regs.A & 0x01;
        SPC_regs.A = SPC_regs.A >> 1;
        SPC_regs.A |= c ? 0x80 : 0x00;
        SPC_flags.Z = !(SPC_regs.A);
        SPC_flags.N = SPC_regs.A & 0x80;
        SPC_regs.PC += 1;
        SPC_cycles += 2;
        break;
    }
    case 0x6B: // ROR (dp)
    {
        SPC700_ResolveAddress(SPC700_AM_DP);
        uint8 t = SPCRead(SPC_addL);
        bool c = SPC_flags.C;
        SPC_flags.C = t & 0x01;
        t = t >> 1;
        t |= c ? 0x80 : 0x00;
        SPC_flags.Z = !(t);
        SPC_flags.N = t & 0x80;
        SPCWrite(SPC_addL, t);
        SPC_regs.PC += 2;
        SPC_cycles += 4;
        break;
    }
    case 0x7B: // ROR (dp+x)
    {
        SPC700_ResolveAddress(SPC700_AM_DPX);
        uint8 t = SPCRead(SPC_addL);
        bool c = SPC_flags.C;
        SPC_flags.C = t & 0x01;
        t = t >> 1;
        t |= c ? 0x80 : 0x00;
        SPC_flags.Z = !(t);
        SPC_flags.N = t & 0x80;
        SPCWrite(SPC_addL, t);
        SPC_regs.PC += 2;
        SPC_cycles += 5;
        break;
    }

    case 0x6C: // ROR (abs)
    {
        SPC700_ResolveAddress(SPC700_AM_Abs);
        uint8 t = SPCRead(SPC_addL);
        bool c = SPC_flags.C;
        SPC_flags.C = t & 0x01;
        t = t >> 1;
        t |= c ? 0x80 : 0x00;
        SPC_flags.Z = !(t);
        SPC_flags.N = t & 0x80;
        SPCWrite(SPC_addL, t);
        SPC_regs.PC += 3;
        SPC_cycles += 5;
        break;
    }

    case 0x9F: // XCN SPC_regs.A
    {
        SPC_regs.A = (SPC_regs.A >> 4) | (SPC_regs.A << 4);
        SPC_flags.Z = !(SPC_regs.A);
        SPC_flags.N = SPC_regs.A & 0x80;
        SPC_regs.PC += 1;
        SPC_cycles += 5;
        break;
    }
#pragma endregion

#pragma region BitOps16
    case 0xBA: // MOVW YA,(dp)
    {
        SPC700_ResolveAddress(SPC700_AM_DP);
        SPC_regs.A = SPCRead(SPC_addL);
        SPC_regs.Y = SPCRead(SPC_addH);
        SPC_flags.Z = !(SPC_regs.A | SPC_regs.Y);
        SPC_flags.N = SPC_regs.Y & 0x80;
        SPC_regs.PC += 2;
        SPC_cycles += 5;
        break;
    }
    case 0xDA: // MOVW (dp),YA
    {
        SPC700_ResolveAddress(SPC700_AM_DP);
        SPCWrite(SPC_addL, SPC_regs.A);
        SPCWrite(SPC_addH, SPC_regs.Y);

        SPC_regs.PC += 2;
        SPC_cycles += 4;
        break;
    }
    case 0x3A: // INCW (dp)
    {
        SPC700_ResolveAddress(SPC700_AM_DP);
        uint16 t = SPCRead(SPC_addL) | (SPCRead(SPC_addH) << 8);
        t++;
        SPCWrite(SPC_addL, t & 0x00FF);
        SPCWrite(SPC_addH, t >> 8);
        SPC_flags.Z = !t;
        SPC_flags.N = t & 0x8000;
        SPC_regs.PC += 2;
        SPC_cycles += 6;
        break;
    }
    case 0x1A: // DECW (dp)
    {
        SPC700_ResolveAddress(SPC700_AM_DP);
        uint16 t = SPCRead(SPC_addL) | (SPCRead(SPC_addH) << 8);
        t--;
        SPCWrite(SPC_addL, t & 0x00FF);
        SPCWrite(SPC_addH, t >> 8);
        SPC_flags.Z = !t;
        SPC_flags.N = t & 0x8000;
        SPC_regs.PC += 2;
        SPC_cycles += 6;
        break;
    }
    case 0x7A: // ADDW YA,(dp)
    {
        SPC700_ResolveAddress(SPC700_AM_DP);
        uint16 op1 = SPCRead(SPC_addL) | (SPCRead(SPC_addH) << 8);
        uint16 op2 = ((SPC_regs.Y << 8) | SPC_regs.A);
        uint32_t add = op1 + op2;
        SPC_flags.Z = !(add & 0x00FFFF);
        SPC_flags.N = add & 0x8000;

        SPC_flags.V = (((op1 ^ add) & (op2 ^ add) & 0x8000) != 0);
        SPC_flags.H = ((op1 & 0x0FFF) + (op2 & 0x0FFF)) > 0x0FFF;
        SPC_flags.C = (add > 0xFFFF);

        SPC_regs.A = add & 0x00FF;
        SPC_regs.Y = (add & 0xFF00) >> 8;

        SPC_regs.PC += 2;
        SPC_cycles += 5;
        break;
    }

    case 0x9A: // SUBW YA,(dp)
    {
        SPC700_ResolveAddress(SPC700_AM_DP);
        int op2 = SPCRead(SPC_addL) | (SPCRead(SPC_addH) << 8);
        int op1 = ((SPC_regs.Y << 8) | SPC_regs.A);
        int sub = op1 - op2;
        SPC_flags.Z = !(sub & 0x00FFFF);
        SPC_flags.N = sub & 0x8000;

        SPC_flags.V = (((op1 ^ op2) & (op1 ^ sub) & 0x8000) != 0);
        SPC_flags.H = ((op1 & 0x0FFF) - (op2 & 0x0FFF)) >= 0;
        SPC_flags.C = (sub >= 0);

        SPC_regs.A = sub & 0x00FF;
        SPC_regs.Y = (sub & 0xFF00) >> 8;

        SPC_regs.PC += 2;
        SPC_cycles += 5;
        break;
    }

    case 0x5A: // CMPW YA,(dp)
    {
        SPC700_ResolveAddress(SPC700_AM_DP);
        int op2 = SPCRead(SPC_addL) | (SPCRead(SPC_addH) << 8);
        int op1 = ((SPC_regs.Y << 8) | SPC_regs.A);
        int sub = op1 - op2;
        SPC_flags.Z = !(sub & 0x00FFFF);
        SPC_flags.N = sub & 0x8000;

        SPC_flags.C = (sub >= 0);

        SPC_regs.PC += 2;
        SPC_cycles += 4;
        break;
    }
#pragma endregion

#pragma region MulDiv
    case 0xCF: // MUL YA
    {
        uint16 mul = SPC_regs.A * SPC_regs.Y;
        SPC_regs.A = mul & 0x00FF;
        SPC_regs.Y = mul >> 8;
        // Yes, only SPC_regs.Y is checked for Z and N
        SPC_flags.Z = !(SPC_regs.Y);
        SPC_flags.N = SPC_regs.Y & 0x80;
        SPC_regs.PC += 1;
        SPC_cycles += 9;
        break;
    }
    case 0x9E: // DIV YA,SPC_regs.X
    {
        SPC_flags.H = (SPC_regs.X & 0x0F) <= (SPC_regs.Y & 0x0F);
        uint32_t D = (SPC_regs.A | (SPC_regs.Y << 8));
        for (int i = 1; i < 10; i++)
        {
            D = D << 1;
            if (D & 0x20000)
                D ^= 0x20001;
            if (D >= (SPC_regs.X * 0x200))
                D ^= 0x1;
            if (D & 0x1)
                D = (D - (SPC_regs.X * 0x200)) & 0x1FFFF;
        }

        SPC_regs.A = D & 0xFF;
        SPC_regs.Y = D / 0x200;

        SPC_flags.Z = !(SPC_regs.A);
        SPC_flags.N = SPC_regs.A & 0x80;

        SPC_flags.V = D & 0x100;

        SPC_regs.PC += 1;
        SPC_cycles += 12;
        break;
    }
#pragma endregion
#pragma region Misc
    case 0xDF: // DAA SPC_regs.A
    {

        uint16_t t = SPC_regs.A;
        if ((t & 0x0F) > 0x09 || SPC_flags.H)
        {
            t += 0x06;
        }

        if (t > 0x9F || SPC_flags.C)
        {
            t += 0x60;
            SPC_flags.C = 1;
        }

        SPC_regs.A = t & 0xFF;

        SPC_flags.Z = !(SPC_regs.A);
        SPC_flags.N = SPC_regs.A & 0x80;
        SPC_regs.PC += 1;
        SPC_cycles += 3;
        break;
    }
    case 0xBE: // DAS SPC_regs.A
    {
        bool sub = (SPC_regs.A > 0x99) || !SPC_flags.C;
        uint16_t t = SPC_regs.A;

        if ((t & 0x0F) > 0x09 || !SPC_flags.H)
        {
            t -= 0x06;
        }

        if (sub)
        {
            t -= 0x60;
            SPC_flags.C = 0;
        }

        SPC_regs.A = t & 0xFF;
        SPC_flags.Z = !(SPC_regs.A);
        SPC_flags.N = SPC_regs.A & 0x80;
        SPC_regs.PC += 1;
        SPC_cycles += 3;
        break;
    }
    case 0x00: // NOP
    {
        SPC_regs.PC += 1;
        SPC_cycles += 2;
        break;
    }
    case 0xef: // SLEEP
    {
        SPC_regs.PC += 1;
        SPC_cycles += 3;
        break;
    }
    case 0xff: // STOP
    {
        SPC_regs.PC += 1;
        SPC_cycles += 3;
        break;
    }

#pragma endregion
#pragma region Branch
    case 0x2F: // BRA rel
    {
        SPC700_ResolveAddress(SPC700_AM_Imm);
        int8_t t = SPCRead(SPC_addL);
        SPC_regs.PC = (SPC_regs.PC + 2) + t;
        SPC_cycles += 4;
        break;
    }

    case 0xF0: // BEQ rel
    {
        SPC700_ResolveAddress(SPC700_AM_Imm);
        int8_t t = SPCRead(SPC_addL);
        if (SPC_flags.Z)
        {
            SPC_regs.PC += t;
            SPC_cycles += 2;
        }
        SPC_regs.PC += 2;
        SPC_cycles += 2;
        break;
    }

    case 0xD0: // BNE rel
    {
        SPC700_ResolveAddress(SPC700_AM_Imm);
        int8_t t = SPCRead(SPC_addL);
        if (!SPC_flags.Z)
        {
            SPC_regs.PC += t;
            SPC_cycles += 2;
        }
        SPC_regs.PC += 2;
        SPC_cycles += 2;
        break;
    }

    case 0xB0: // BCS rel
    {
        SPC700_ResolveAddress(SPC700_AM_Imm);
        int8_t t = SPCRead(SPC_addL);
        if (SPC_flags.C)
        {
            SPC_regs.PC += t;
            SPC_cycles += 2;
        }
        SPC_regs.PC += 2;
        SPC_cycles += 2;
        break;
    }
    case 0x90: // BCC rel
    {
        SPC700_ResolveAddress(SPC700_AM_Imm);
        int8_t t = SPCRead(SPC_addL);
        if (!SPC_flags.C)
        {
            SPC_regs.PC += t;
            SPC_cycles += 2;
        }
        SPC_regs.PC += 2;
        SPC_cycles += 2;
        break;
    }
    case 0x70: // BVS rel
    {
        SPC700_ResolveAddress(SPC700_AM_Imm);
        int8_t t = SPCRead(SPC_addL);
        if (SPC_flags.V)
        {
            SPC_regs.PC += t;
            SPC_cycles += 2;
        }
        SPC_regs.PC += 2;
        SPC_cycles += 2;
        break;
    }
    case 0x50: // BVC rel
    {
        SPC700_ResolveAddress(SPC700_AM_Imm);
        int8_t t = SPCRead(SPC_addL);
        if (!SPC_flags.V)
        {
            SPC_regs.PC += t;
            SPC_cycles += 2;
        }
        SPC_regs.PC += 2;
        SPC_cycles += 2;
        break;
    }
    case 0x30: // BMI rel
    {
        SPC700_ResolveAddress(SPC700_AM_Imm);
        int8_t t = SPCRead(SPC_addL);
        if (SPC_flags.N)
        {
            SPC_regs.PC += t;
            SPC_cycles += 2;
        }
        SPC_regs.PC += 2;
        SPC_cycles += 2;
        break;
    }
    case 0x10: // BPL rel
    {
        SPC700_ResolveAddress(SPC700_AM_Imm);
        int8_t t = SPCRead(SPC_addL);
        if (!SPC_flags.N)
        {
            SPC_regs.PC += t;
            SPC_cycles += 2;
        }
        SPC_regs.PC += 2;
        SPC_cycles += 2;
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
        uint8 d = SPCRead(SPC_addL);
        int8_t t = SPC_addH;
        if (d & (1 << ((inst & 0b11100000) >> 5)))
        {
            SPC_regs.PC += t;
            SPC_cycles += 2;
        }
        SPC_regs.PC += 3;
        SPC_cycles += 5;
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
        uint8 d = SPCRead(SPC_addL);
        int8_t t = SPC_addH;
        if (!(d & (1 << ((inst & 0b11100000) >> 5))))
        {
            SPC_regs.PC += t;
            SPC_cycles += 2;
        }
        SPC_regs.PC += 3;
        SPC_cycles += 5;
        break;
    }

    case 0x2E: // CBNE dp,rel
    {
        SPC700_ResolveAddress(SPC700_AM_SrcDst);
        uint8 d = SPCRead(SPC_addL);
        int8_t t = SPC_addH;
        if (d != SPC_regs.A)
        {
            SPC_regs.PC += t;
            SPC_cycles += 2;
        }
        SPC_regs.PC += 3;
        SPC_cycles += 5;
        break;
    }
    case 0xDE: // CBNE (dp+x),rel
    {
        SPC700_ResolveAddress(SPC700_AM_DPX);
        uint8 d = SPCRead(SPC_addL);
        int8_t t = SPC_addH;
        if (d != SPC_regs.A)
        {
            SPC_regs.PC += t;
            SPC_cycles += 2;
        }
        SPC_regs.PC += 3;
        SPC_cycles += 6;
        break;
    }
    case 0x6E: // DBNZ dp,rel
    {
        SPC700_ResolveAddress(SPC700_AM_SrcDst);
        uint8 d = SPCRead(SPC_addL);
        int8_t t = SPC_addH;
        d--;
        SPCWrite(SPC_addL, d);
        if (d)
        {
            SPC_regs.PC += t;
            SPC_cycles += 2;
        }
        SPC_regs.PC += 3;
        SPC_cycles += 5;
        break;
    }
    case 0xFE: // DBNZ SPC_regs.Y,rel
    {
        SPC700_ResolveAddress(SPC700_AM_Imm);
        int8_t t = SPCRead(SPC_addL);
        SPC_regs.Y--;
        if (SPC_regs.Y)
        {
            SPC_regs.PC += t;
            SPC_cycles += 2;
        }
        SPC_regs.PC += 2;
        SPC_cycles += 4;
        break;
    }

#pragma endregion
#pragma region CallsAndJumps
    case 0x5F: // JMP abs
    {
        SPC700_ResolveAddress(SPC700_AM_Abs);

        SPC_regs.PC = SPC_addL;
        SPC_cycles += 3;
        break;
    }
    case 0x1F: // JMP (abs+x)
    {
        SPC700_ResolveAddress(SPC700_AM_AbsX);
        uint16 t = SPCRead(SPC_addL) | (SPCRead(SPC_addH) << 8);
        SPC_regs.PC = t;
        SPC_cycles += 6;
        break;
    }

    case 0x3F: // CALL abs
    {
        SPC700_ResolveAddress(SPC700_AM_Abs);
        SPC_regs.PC += 3;
        SPC700_Push(SPC_regs.PC >> 8);
        SPC700_Push(SPC_regs.PC & 0x00FF);
        SPC_regs.PC = SPC_addL;
        SPC_cycles += 8;
        break;
    }

    case 0x4F: // PCALL upage
    {
        SPC700_ResolveAddress(SPC700_AM_Imm);
        SPC_regs.PC += 2;
        SPC700_Push(SPC_regs.PC >> 8);
        SPC700_Push(SPC_regs.PC & 0x00FF);
        SPC_regs.PC = 0XFF00 | SPCRead(SPC_addL);
        SPC_cycles += 6;
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
        SPC_regs.PC += 1;
        SPC700_Push(SPC_regs.PC >> 8);
        SPC700_Push(SPC_regs.PC & 0x00FF);
        SPC_regs.PC = SPCRead(ta) | (SPCRead(ta + 1) << 8);
        SPC_cycles += 8;
        break;
    }

    case 0x0F: // BRK
    {

        SPC_regs.PC += 1;
        SPC700_Push(SPC_regs.PC >> 8);
        SPC700_Push(SPC_regs.PC & 0x00FF);
        SPC700_Push(SPC700_GetFlagsAsByte());
        SPC_flags.B = 1;
        SPC_flags.I = 0;
        SPC_regs.PC = SPCRead(0xFFDE) | (SPCRead(0xFFDF) << 8);
        SPC_cycles += 8;
        break;
    }

    case 0x6F: // RET
    {
        SPC_regs.PC = SPC700_Pop();
        SPC_regs.PC |= SPC700_Pop() << 8;
        SPC_cycles += 5;
        break;
    }
    case 0x7F: // RETI
    {
        SPC700_SetFlagsAsByte(SPC700_Pop());
        SPC_regs.PC = SPC700_Pop();
        SPC_regs.PC |= SPC700_Pop() << 8;
        SPC_cycles += 6;
        break;
    }
#pragma endregion

#pragma region StackOps
    case 0x2D: // PUSH SPC_regs.A
    {
        SPC700_Push(SPC_regs.A);
        SPC_regs.PC += 1;
        SPC_cycles += 4;
        break;
    }
    case 0x4D: // PUSH SPC_regs.X
    {
        SPC700_Push(SPC_regs.X);
        SPC_regs.PC += 1;
        SPC_cycles += 4;
        break;
    }
    case 0x6D: // PUSH SPC_regs.Y
    {
        SPC700_Push(SPC_regs.Y);
        SPC_regs.PC += 1;
        SPC_cycles += 4;
        break;
    }
    case 0x0D: // PUSH SPC_flags
    {
        SPC700_Push(SPC700_GetFlagsAsByte());
        SPC_regs.PC += 1;
        SPC_cycles += 4;
        break;
    }

    case 0xAE: // POP SPC_regs.A
    {
        SPC_regs.A = SPC700_Pop();
        SPC_regs.PC += 1;
        SPC_cycles += 4;
        break;
    }
    case 0xCE: // POP SPC_regs.X
    {
        SPC_regs.X = SPC700_Pop();
        SPC_regs.PC += 1;
        SPC_cycles += 4;
        break;
    }
    case 0xEE: // POP SPC_regs.Y
    {
        SPC_regs.Y = SPC700_Pop();
        SPC_regs.PC += 1;
        SPC_cycles += 4;
        break;
    }
    case 0x8E: // POP SPC_flags
    {
        SPC700_SetFlagsAsByte(SPC700_Pop());
        SPC_regs.PC += 1;
        SPC_cycles += 4;
        break;
    }
#pragma endregion

#pragma region FlagOps
    case 0x60: // CLRC
    {
        SPC_flags.C = 0;
        SPC_regs.PC += 1;
        SPC_cycles += 2;
        break;
    }
    case 0x80: // SETC
    {
        SPC_flags.C = 1;
        SPC_regs.PC += 1;
        SPC_cycles += 2;
        break;
    }

    case 0xED: // NOTC
    {
        SPC_flags.C = !SPC_flags.C;
        SPC_regs.PC += 1;
        SPC_cycles += 3;
        break;
    }

    case 0xE0: // CLRC
    {
        SPC_flags.V = 0;
        SPC_flags.H = 0;
        SPC_regs.PC += 1;
        SPC_cycles += 2;
        break;
    }

    case 0x20: // CLRP
    {
        SPC_flags.P = 0;
        SPC_regs.PC += 1;
        SPC_cycles += 2;
        break;
    }

    case 0x40: // SETP
    {
        SPC_flags.P = 1;
        SPC_regs.PC += 1;
        SPC_cycles += 2;
        break;
    }

    case 0xA0: // EI
    {
        SPC_flags.I = 1;
        SPC_regs.PC += 1;
        SPC_cycles += 3;
        break;
    }

    case 0xC0: // DI
    {
        SPC_flags.I = 0;
        SPC_regs.PC += 1;
        SPC_cycles += 3;
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
        uint8 d = SPCRead(SPC_addL);
        d |= 1 << n;
        SPCWrite(SPC_addL, d);
        SPC_regs.PC += 2;
        SPC_cycles += 4;
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
        uint8 d = SPCRead(SPC_addL);
        d &= ~(1 << n);
        SPCWrite(SPC_addL, d);
        SPC_regs.PC += 2;
        SPC_cycles += 4;
        break;
    }

    case 0x0e: // TSET abs
    {
        SPC700_ResolveAddress(SPC700_AM_Abs);
        uint8 d = SPCRead(SPC_addL);
        uint8 t = SPC_regs.A - d;
        d |= SPC_regs.A;
        SPCWrite(SPC_addL, d);
        SPC_flags.Z = !(t);
        SPC_flags.N = t & 0x80;
        SPC_regs.PC += 3;
        SPC_cycles += 6;
        break;
    }

    case 0x4e: // TCLR abs
    {
        SPC700_ResolveAddress(SPC700_AM_Abs);
        uint8 d = SPCRead(SPC_addL);
        uint8 t = SPC_regs.A - d;
        d &= ~SPC_regs.A;
        SPCWrite(SPC_addL, d);
        SPC_flags.Z = !(t);
        SPC_flags.N = t & 0x80;
        SPC_regs.PC += 3;
        SPC_cycles += 6;
        break;
    }

    case 0x4a: // AND C,mem.bit
    {
        SPC700_ResolveAddress(SPC700_AM_MemBit);
        bool d = SPCRead(SPC_addL) & (1 << SPC_addH);
        SPC_flags.C = SPC_flags.C && d;
        SPC_regs.PC += 3;
        SPC_cycles += 4;
        break;
    }

    case 0x6a: // AND C,/mem.bit
    {
        SPC700_ResolveAddress(SPC700_AM_MemBit);
        bool d = ~SPCRead(SPC_addL) & (1 << SPC_addH);
        SPC_flags.C = SPC_flags.C && d;
        SPC_regs.PC += 3;
        SPC_cycles += 4;
        break;
    }

    case 0x0a: // OR C,mem.bit
    {
        SPC700_ResolveAddress(SPC700_AM_MemBit);
        bool d = SPCRead(SPC_addL) & (1 << SPC_addH);
        SPC_flags.C = SPC_flags.C || d;
        SPC_regs.PC += 3;
        SPC_cycles += 5;
        break;
    }

    case 0x2a: // OR C,/mem.bit
    {
        SPC700_ResolveAddress(SPC700_AM_MemBit);
        bool d = ~SPCRead(SPC_addL) & (1 << SPC_addH);
        SPC_flags.C = SPC_flags.C || d;
        SPC_regs.PC += 3;
        SPC_cycles += 5;
        break;
    }

    case 0x8a: // EOR C,mem.bit
    {
        SPC700_ResolveAddress(SPC700_AM_MemBit);
        bool d = SPCRead(SPC_addL) & (1 << SPC_addH);
        SPC_flags.C = SPC_flags.C ^ d;
        SPC_regs.PC += 3;
        SPC_cycles += 5;
        break;
    }

    case 0xea: // NOT mem.bit
    {
        SPC700_ResolveAddress(SPC700_AM_MemBit);
        uint8 d = SPCRead(SPC_addL) ^ (1 << SPC_addH);
        SPCWrite(SPC_addL, d);
        SPC_regs.PC += 3;
        SPC_cycles += 5;
        break;
    }

    case 0xaa: // MOV C,mem.bit
    {
        SPC700_ResolveAddress(SPC700_AM_MemBit);
        SPC_flags.C = SPCRead(SPC_addL) & (1 << SPC_addH);
        SPC_regs.PC += 3;
        SPC_cycles += 4;
        break;
    }

    case 0xca: // MOV mem.bit,C
    {
        SPC700_ResolveAddress(SPC700_AM_MemBit);
        uint8 d = SPCRead(SPC_addL) & (~(1 << SPC_addH));
        d = d | ((SPC_flags.C << SPC_addH));
        SPCWrite(SPC_addL, d);
        SPC_regs.PC += 3;
        SPC_cycles += 6;
        break;
    }

#pragma endregion

    default:
        break;
    }
}

#pragma endregion


#pragma region APU
uint8 SPCRead(uint16 address)
{
    if ((address & 0xFFF0) == 0x00F0) // IO 00F0 to 00FF
    {
        switch (address & 0x000F)
        {
        case 0x0: // Test
            break;
        case 0x1: // CONTROL
            break;
        case 0x2: // DSPADDR
            break;
        case 0x3: // DSPDATA
            break;

        case 0x4: // CPUIO0
            return APUIO0;
            break;
        case 0x5: // CPUIO1
            return APUIO1;
            break;
        case 0x6: // CPUIO2
            return APUIO2;
            break;
        case 0x7: // CPUIO3
            return APUIO3;
            break;

        // The following are unsued but they hold their value, so they can be used as storage.
        // And since SPC writes also are passed through the ARAM at that location, we can just return the ram.
        case 0x8: // AUXIO4 (unused)
            return aram[address];
            break;
        case 0x9: // AUXIO5 (unused)
            return aram[address];
            break;

        // These 3 are write only
        case 0xa: // T0DIV
            return t0div;
            break;
        case 0xb: // T1DIV
            return t1div;
            break;
        case 0xc: // T2DIV
            return t2div;
            break;

        // These 3 are read only
        case 0xd: // T0OUT
        {
            uint8 t = t0Val & 0x0F;
            t0Val = 0;
            return t;
            break;
        }
        case 0xe: // T1OUT
        {
            uint8 t = t1Val & 0x0F;
            t1Val = 0;
            return t;
            break;
        }
        case 0xf: // T2OUT
        {
            uint8 t = t2Val & 0x0F;
            t2Val = 0;
            return t;
            break;
        }
        default:
            break;
        }

        return 0;
    }

    if (ramRomSel && (address & 0xFFC0) == 0xFFC0)
        return arom[(address & (~0xFFC0))];
    else
        return aram[address];
    return 0;
}
void SPCWrite(uint16 address, uint8 data)
{
    aram[address] = data;
    if ((address & 0xFFF0) == 0x00F0) // IO 00F0 to 00FF
    {
        switch (address & 0x000F)
        {

        case 0x0: // Test

            break;
        case 0x1: // CONTROL
        {
            if (data & 0b00000001)
                t0En = true;
            else
            {
                t0En = false;
                t0Val = 0;
                t0div = 1;
            }

            if (data & 0b00000010)
                t1En = true;
            else
            {
                t1En = false;
                t1Val = 0;
                t1div = 1;
            }

            if (data & 0b00000100)
                t2En = true;
            else
            {
                t2En = false;
                t2Val = 0;
                t2div = 1;
            }

            ramRomSel = data & 0b10000000;
            break;
        }

        case 0x2: // DSPADDR
            break;
        case 0x3: // DSPDATA
            break;

        case 0x4: // CPUIO0
            CPUIO0 = data;
            return;
            break;
        case 0x5: // CPUIO1
            CPUIO1 = data;
            return;
            break;
        case 0x6: // CPUIO2
            CPUIO2 = data;
            return;
            break;
        case 0x7: // CPUIO3
            CPUIO3 = data;
            return;
            break;

        case 0x8: // AUXIO4 (unused)
            break;
        case 0x9: // AUXIO5 (unused)
            break;

        // Write only
        case 0xa: // T0DIV
            t0div = data ? data : 256;
            break;
        case 0xb: // T1DIV
            t1div = data ? data : 256;
            break;
        case 0xc: // T2DIV
            t2div = data ? data : 256;
            break;

        default:
            break;
        }
    }
}

void APU_Init()
{
    ramRomSel = 1;

    SPC700_Reset();
}

void APU_IOWrite(uint16 port, uint8 data)
{
    switch (port)
    {
    case 0x2140:
        APUIO0 = data;
        break;

    case 0x2141:
        APUIO1 = data;
        break;

    case 0x2142:
        APUIO2 = data;
        break;

    case 0x2143:
        APUIO3 = data;
        break;

    default:
        break;
    }
}

uint8 APU_IORead(uint16 port)
{
    switch (port)
    {
    case 0x2140:
        return CPUIO0;
        break;

    case 0x2141:
        return CPUIO1;
        break;

    case 0x2142:
        return CPUIO2;
        break;

    case 0x2143:
        return CPUIO3;
        break;

    default:
        break;
    }
}

void APU_Step()
{

    if (SPC_cycles < stepCnt)
    {
        SPC700_Step();
        // SPC700::PrintState();
    }

    // if (SPC700::cycles > 60000 && stepCnt > 60000)
    //{
    //     stepCnt -= 60000;
    //     SPC700::cycles -= 60000;
    // }

    // The 3 counters

    if (t0En)
    {
        if ((stepCnt & 127) == 0)
        {
            t0Clk++;

            if (t0div == t0Clk)
            {
                t0Val++;
                t0Clk = 0;
            }
        }
    }

    if (t1En)
    {
        if ((stepCnt & 127) == 0)
        {
            t1Clk++;

            if (t1div == t1Clk)
            {
                t1Val++;
                t1Clk = 0;
            }
        }
    }

    if (t2En)
    {
        if ((stepCnt & 15) == 0)
        {
            t2Clk++;

            if (t2div == t2Clk)
            {
                t2Val++;
                t2Clk = 0;
            }
        }
    }

    // Update step counter
    stepCnt++;
}

#pragma endregion

#pragma region PPU

uint16 TranslateVRAMAddress(uint16 addr)
{
    uint8 translation = (VMAINC >> 2) & 0b00000011;
    uint16 mappedAddr = addr;

    switch (translation)
    {

    case 1:
        // 01: 8-bit rotation (for 32 times)
        // Keep upper 8 bits, shift lower 5 bits left by 3, wrap top 3 bits to bottom
        mappedAddr = (addr & 0xFF00) | ((addr & 0x001F) << 3) | ((addr >> 5) & 0x0007);
        break;

    case 2:
        // 10: 9-bit rotation (for 64 times)
        // Keep upper 7 bits, shift lower 6 bits left by 3, wrap top 3 bits to bottom
        mappedAddr = (addr & 0xFE00) | ((addr & 0x003F) << 3) | ((addr >> 6) & 0x0007);
        break;

    case 3:
        // 11: 10-bit rotation (for 128 times)
        // Keep upper 6 bits, shift lower 7 bits left by 3, wrap top 3 bits to bottom
        mappedAddr = (addr & 0xFC00) | ((addr & 0x007F) << 3) | ((addr >> 7) & 0x0007);
        break;
    }

    return mappedAddr;
}

uint16 VRAMRead(uint16 add)
{
    return vram[TranslateVRAMAddress(add & 0x7FFF)];
}

void doMult()
{
    mult = (int16_t)mode7A * (int8_t)((mode7B >> 8));
}

void IncVMADD()
{
    uint8 inc = VMAINC & 0b00000011;
    switch (inc)
    {
    case 0:
        vramPtr += 1;
        break;
    case 1:
        vramPtr += 32;
        break;
    case 2:
        vramPtr += 128;
        break;
    case 3:
        vramPtr += 128;
        break;
    default:

        break;
    }
    vramPtr &= 0x7FFF; // Bit 15 of vram address is ignored.
}

// TOOD : later all these registers can be replaced by just their funtionality
uint8 PPU_IORead(add24 address)
{
    switch (address)
    {

    case 0x2134: // MPYL
        return (mult & 0x000000FF);
        break;
    case 0x2135: // MPYM
        return ((mult & 0x0000FF00) >> 8);
        break;
    case 0x2136: // MPYH
        return (mult & 0x00FF0000) >> 16;
        break;

    case 0x2137: // SLHV

        hCounterLatch = hCounter;
        vCounterLatch = vCounter;
        return 0;
        break;
    case 0x2138: // OAMDATA_READ
        return 0;
        break;

    case 0x2118: // for some reason zelda uses this
    case 0x2139: // VMDATAL_READ
    {
        uint16 add = (vramAddr + TranslateVRAMAddress(vramPtr)) & 0x7FFF;
        uint8 t = vram[add] & 0x00ff;
        if (!(VMAINC & 0b10000000)) // Inc on low
            IncVMADD();
        return t;
        break;
    }

    case 0x2119: // for some reason zelda uses this
    case 0x213A: // VMDATAH_READ
    {
        uint16 add = (vramAddr + TranslateVRAMAddress(vramPtr)) & 0x7FFF;
        uint8 t = (vram[add] & 0xff00) >> 8;
        if (VMAINC & 0b10000000) // Inc on high
            IncVMADD();
        return t;
        break;
    }

    case 0x213B: // CGDATA_READ
    {
        uint8 t;
        if (!cgDataHL) // Low
        {
            cgDataHL = true;
            t = cgram[cgAddr] & 0x00ff;
        }
        else // High
        {
            t = (cgram[cgAddr] & 0xff00) >> 8;
            cgAddr++;
            cgDataHL = false;
        }
        return t;
        break;
    }

    case 0x213C: // OPHCT
    {
        if (!hCounterLatchHL) // Low
        {
            hCounterLatchHL = true;
            OPHCT = hCounterLatch & 0x00FF;
        }
        else
        {
            hCounterLatchHL = false;
            OPHCT = (hCounterLatch & 0xFF00) >> 8;
        }
        return OPHCT;
        break;
    }
    case 0x213D: // OPVCT
    {
        if (!vCounterLatchHL) // Low
        {
            vCounterLatchHL = true;
            OPVCT = vCounterLatch & 0x00FF;
        }
        else
        {
            vCounterLatchHL = false;
            OPVCT = (vCounterLatch & 0xFF00) >> 8;
        }
        return OPVCT;
        break;
    }
    case 0x213E: // STAT77
        return 0;
        break;
    case 0x213F:                 // STAT78
        hCounterLatchHL = false; // Yes, here, based on manual page 2-27-23
        vCounterLatchHL = false;
        return 0;
        break;
    default:
        return 0;
        break;
    }
}
void PPU_IOWrite(add24 address, uint8 data)
{
    switch (address)
    {
    case 0x2100: // INIDISP
        forceBlank = data >> 4;
        fadeValue = data & 0x0F;
        // cout << "dt : "<< hex << (uint16)  data << endl;
        break;

    case 0x2101: // OBJSEL
        objAvailSize = (data & 0b11100000) >> 5;
        OBJChrTable1BaseAddr = (data & 0b00000111) * 0x2000;
        OBJChrTable2BaseAddr = OBJChrTable1BaseAddr + (((data >> 3) & 0x03) + 1) * 0x1000;

        break;

    case 0x2102: // OAMADDL
        oamDataHL = false;
        oamAddr = (oamAddr & 0xff00) | data;
        break;
    case 0x2103: // OAMADDH
        oamDataHL = false;
        oamAddr = (oamAddr & 0x00ff) | ((data & 0b00000001) << 8);
        break;

    case 0x2104: // OAMDATA
    {
        uint8 idx = oamAddr >> 1;
        uint8 wSel = oamAddr & 0x0001;
        uint8 secIdx = (oamAddr - 0x100) << 3;
        if (!oamDataHL) // Low
        {

            oamDataHL = true;

            if (oamAddr <= 255)
            {
                if (wSel == 0)
                {
                    objs[idx].objX = (objs[idx].objX & 0xFF00) | data;
                    objs[idx].objVisible = objs[idx].objX > -8;
                }
                else
                    objs[idx].objChrName = data;
            }
            else
            {
                objs[secIdx].objX = (objs[secIdx].objX & 0x00FF) | ((data & 0b00000001) ? 0xFF00 : 0x0000);
                objs[secIdx].size = data & 0b00000010;
                objs[secIdx].objVisible = objs[secIdx].objX > -8;

                objs[secIdx + 1].objX = (objs[secIdx + 1].objX & 0x00FF) | ((data & 0b00000100) ? 0xFF00 : 0x0000);
                objs[secIdx + 1].size = data & 0b00001000;
                objs[secIdx + 1].objVisible = objs[secIdx + 1].objX > -8;

                objs[secIdx + 2].objX = (objs[secIdx + 2].objX & 0x00FF) | ((data & 0b00010000) ? 0xFF00 : 0x0000);
                objs[secIdx + 2].size = data & 0b00100000;
                objs[secIdx + 2].objVisible = objs[secIdx + 2].objX > -8;

                objs[secIdx + 3].objX = (objs[secIdx + 3].objX & 0x00FF) | ((data & 0b01000000) ? 0xFF00 : 0x0000);
                objs[secIdx + 3].size = data & 0b10000000;
                objs[secIdx + 3].objVisible = objs[secIdx + 3].objX > -8;
            }
        }
        else // High
        {

            if (oamAddr <= 255)
            {
                if (wSel == 0)
                {
                    objs[idx].objY = data;
                }
                else
                {
                    objs[idx].objChrTable = data & 0b00000001;
                    objs[idx].objClrPal = (data & 0b00001110) >> 1;
                    objs[idx].objPrior = (data & 0b00110000) >> 4;
                    objs[idx].objHF = data & 0b01000000;
                    objs[idx].objVF = data & 0b10000000;
                }
            }
            else
            {
                objs[secIdx + 4].objX = (objs[secIdx + 4].objX & 0x00FF) | ((data & 0b00000001) ? 0xFF00 : 0x000);
                objs[secIdx + 4].size = data & 0b00000010;
                objs[secIdx + 4].objVisible = objs[secIdx + 4].objX > -8;

                objs[secIdx + 5].objX = (objs[secIdx + 5].objX & 0x00FF) | ((data & 0b00000100) ? 0xFF00 : 0x0000);
                objs[secIdx + 5].size = data & 0b00001000;
                objs[secIdx + 5].objVisible = objs[secIdx + 5].objX > -8;

                objs[secIdx + 6].objX = (objs[secIdx + 6].objX & 0x00FF) | ((data & 0b00010000) ? 0xFF00 : 0x0000);
                objs[secIdx + 6].size = data & 0b00100000;
                objs[secIdx + 6].objVisible = objs[secIdx + 6].objX > -8;

                objs[secIdx + 7].objX = (objs[secIdx + 7].objX & 0x00FF) | ((data & 0b01000000) ? 0xFF00 : 0x0000);
                objs[secIdx + 7].size = data & 0b10000000;
                objs[secIdx + 7].objVisible = objs[secIdx + 7].objX > -8;
            }
            oamAddr++;
            if (oamAddr > 271)
                oamAddr = 0;
            oamDataHL = false;
        }
        break;
    }

    case 0x2105: // BGMODE
        mode = 0b00000111 & data;
        bg3PriorMode = 0b00001000 & data;
        bg1ChrSize = 0b00010000 & data;
        bg2ChrSize = 0b00100000 & data;
        bg3ChrSize = 0b01000000 & data;
        bg4ChrSize = 0b10000000 & data;
        break;

    case 0x2106: // MOSAIC
        MOSAIC = data;
        break;

    // 2 lower bits indicate the size which if is bigger than 32x32 has to be used with scrolling to see which tile we are working with.
    case 0x2107: // BG1SC
        BG1BaseAddr = ((data & 0b11111100) >> 2) * 0x0400;
        BG1BaseAddr &= 0x7fff;
        BG1SCSize = data & 0b00000011;
        break;
    case 0x2108: // BG2SC
        BG2BaseAddr = ((data & 0b11111100) >> 2) * 0x0400;
        BG2BaseAddr &= 0x7fff;
        BG2SCSize = data & 0b00000011;
        break;
    case 0x2109: // BG3SC
        BG3BaseAddr = ((data & 0b11111100) >> 2) * 0x0400;
        BG3BaseAddr &= 0x7fff;
        BG3SCSize = data & 0b00000011;
        break;
    case 0x210A: // BG4SC
        BG4BaseAddr = ((data & 0b11111100) >> 2) * 0x0400;
        BG4BaseAddr &= 0x7fff;
        BG4SCSize = data & 0b00000011;
        break;

    case 0x210B: // BG12NBA
        BG1ChrsBaseAddr = (data & 0x0F) * 0x1000;
        BG1ChrsBaseAddr &= 0x7fff;
        BG2ChrsBaseAddr = ((data & 0xF0) >> 4) * 0x1000;

        BG2ChrsBaseAddr &= 0x7fff;
        break;
    case 0x210C: // BG34NBA
        BG3ChrsBaseAddr = (data & 0x0F) * 0x1000;
        BG3ChrsBaseAddr &= 0x7fff;
        BG4ChrsBaseAddr = ((data & 0xF0) >> 4) * 0x1000;
        BG4ChrsBaseAddr &= 0x7fff;
        break;

    case 0x210D: // BG1H0FS

        BG1HScroll = (data << 8) | mode7Old;
        mode7Old = data;
        break;
    case 0x210E: // BG1V0FS

        BG1VScroll = (data << 8) | mode7Old;
        mode7Old = data;
        break;

    case 0X210F:           // BG2H0FS
        if (!BG2HScrollHL) // Low
        {
            BG2HScroll = (BG2HScroll & 0xFF00) | data;
            BG2HScrollHL = true;
        }
        else // High
        {
            BG2HScroll = (BG2HScroll & 0x00FF) | (data << 8);
            BG2HScrollHL = false;
        }
        break;
    case 0X2110:           // BG2V0FS
        if (!BG2VScrollHL) // Low
        {
            BG2VScroll = (BG2VScroll & 0xFF00) | data;
            BG2VScrollHL = true;
        }
        else // High
        {
            BG2VScroll = (BG2VScroll & 0x00FF) | (data << 8);
            BG2VScrollHL = false;
        }
        break;

    case 0X2111:           // BG3H0FS
        if (!BG3HScrollHL) // Low
        {
            BG3HScroll = (BG3HScroll & 0xFF00) | data;
            BG3HScrollHL = true;
        }
        else // High
        {
            BG3HScroll = (BG3HScroll & 0x00FF) | (data << 8);
            BG3HScrollHL = false;
        }
        break;
    case 0X2112:           // BG3V0FS
        if (!BG3VScrollHL) // Low
        {
            BG3VScroll = (BG3VScroll & 0xFF00) | data;
            BG3VScrollHL = true;
        }
        else // High
        {
            BG3VScroll = (BG3VScroll & 0x00FF) | (data << 8);
            BG3VScrollHL = false;
        }
        break;

    case 0X2113:           // BG4H0FS
        if (!BG4HScrollHL) // Low
        {
            BG4HScroll = (BG4HScroll & 0xFF00) | data;
            BG4HScrollHL = true;
        }
        else // High
        {
            BG4HScroll = (BG4HScroll & 0x00FF) | (data << 8);
            BG4HScrollHL = false;
        }
        break;
    case 0X2114:           // BG4V0FS
        if (!BG4VScrollHL) // Low
        {
            BG4VScroll = (BG4VScroll & 0xFF00) | data;
            BG4VScrollHL = true;
        }
        else // High
        {
            BG4VScroll = (BG4VScroll & 0x00FF) | (data << 8);
            BG4VScrollHL = false;
        }
        break;

    case 0x2115: // VMAINC
        VMAINC = data;
        break;

    case 0x2116: // VMADDL
        vramAddr = (vramAddr & 0xff00) | data;
        vramPtr = 0;
        break;
    case 0x2117: // VMADDH
        vramAddr = (vramAddr & 0x00ff) | (data << 8);
        vramPtr = 0;
        break;

    case 0x2118: // VMDATAL
    {
        uint16 add = (vramAddr + TranslateVRAMAddress(vramPtr)) & 0x7FFF;
        vram[add] = (vram[add] & 0xff00) | data;
        if (!(VMAINC & 0b10000000)) // Inc on low
            IncVMADD();

        break;
    }
    case 0x2119: // VMDATAH
    {
        uint16 add = (vramAddr + TranslateVRAMAddress(vramPtr)) & 0x7FFF;
        vram[add] = (vram[add] & 0x00ff) | (data << 8);
        if (VMAINC & 0b10000000) // Inc on high
            IncVMADD();

        break;
    }

    case 0x211A: // M7SEL
        M7SEL = data;
        break;

    case 0x211B: // M7A

        mode7A = (data << 8) | mode7Old;
        mode7Old = data;
        break;

    case 0x211C: // M7B
        mode7B = (data << 8) | mode7Old;
        mode7Old = data;
        doMult();

        break;

    case 0x211D: // M7C
        mode7C = (data << 8) | mode7Old;
        mode7Old = data;
        break;

    case 0x211E: // M7D
        mode7D = (data << 8) | mode7Old;
        mode7Old = data;
        break;

    case 0x211F: // M7X
        centerX = (data << 8) | mode7Old;
        mode7Old = data;
        break;

    case 0x2120: // M7Y
        centerY = (data << 8) | mode7Old;
        mode7Old = data;
        break;

    case 0x2121: // CGADD
    {
        cgDataHL = false;
        cgAddr = data;
        break;
    }
    case 0x2122: // CGDATA
    {
        if (!cgDataHL) // Low
        {
            cgDataHL = true;
            cgram[cgAddr] = (cgram[cgAddr] & 0xff00) | data;
            // cout << "CGDATA(Low) : " << hex << (unsigned int)data << endl;
        }
        else // High
        {
            cgram[cgAddr] = (cgram[cgAddr] & 0x00ff) | (data << 8);
            // cout << "CGDATA(High) : " << hex << (unsigned int)data << endl;
            cgAddr++;
            cgDataHL = false;
        }
        break;
    }

    case 0x2123: // W12SEL
        W12SEL = data;
        break;
    case 0x2124: // W34SEL
        W34SEL = data;
        break;
    case 0x2125: // WOBJSEL
        WOBJSEL = data;
        break;
    case 0x2126: // WH0
        WH0 = data;
        break;
    case 0x2127: // WH1
        WH1 = data;
        break;
    case 0x2128: // WH2
        WH2 = data;
        break;
    case 0x2129: // WH3
        WH3 = data;
        break;
    case 0x212A: // WBGLOG
        WBGLOG = data;
        break;
    case 0x212B: // WOBJLOG
        WOBJLOG = data;
        break;

    case 0x212C: // TM
        BG1onMainScreen = data & 0b00000001;
        BG2onMainScreen = data & 0b00000010;
        BG3onMainScreen = data & 0b00000100;
        BG4onMainScreen = data & 0b00001000;
        OBJonMainScreen = data & 0b00010000;
        break;

    case 0x212D: // TS
        BG1onSubScreen = data & 0b00000001;
        BG2onSubScreen = data & 0b00000010;
        BG3onSubScreen = data & 0b00000100;
        BG4onSubScreen = data & 0b00001000;
        OBJonSubScreen = data & 0b00010000;
        break;
    case 0x212e: // TMW
        TMW = data;
        break;
    case 0x212f: // TSW
        TSW = data;
        break;
    case 0x2130: // CGSWSEL
        constColorSel = !(data & 0x02);
        directColor = data & 0x01;
        break;
    case 0x2131: // CGADSUB
        bg1AddSub = data & 0b00000001;
        bg2AddSub = data & 0b00000010;
        bg3AddSub = data & 0b00000100;
        bg4AddSub = data & 0b00001000;
        objAddSub = data & 0b00010000;
        bdpAddSub = data & 0b00100000;
        addSubHalf = data & 0b01000000;
        addSub = data & 0b10000000;

        break;
    case 0x2132: // COLDATA
    {
        if (data & 0x20)
            constColor = (constColor & 0x7C00) | (data & 0x1F); // Red
        if (data & 0x40)
            constColor = (constColor & 0x03FF) | ((data & 0x1F) << 5); // Green
        if (data & 0x80)
            constColor = (constColor & 0x00FF) | ((data & 0x1F) << 10); // Blue

        break;
    }
    case 0x2133: // SETINI
        interlacing = data & 0x01;
        break;
    }
}

void PPU_Reset()
{

    VMAINC = 0x80;
    // CGSWSEL = 0x30;
    constColor = 0xE0;
    hCounter = 0;
    vCounter = 0;
    hBlank = false;
    vBlank = false;
    frameCount = 0;
}

void PPU_Step()
{

    if (hCounter == 255 && vCounter < 224)
    {

        // BG
        // Priority
        // Offset
        // vCounter
        // Size
        // Color mode
        // Mode
        // Direct Color
        // On (Main/Sub) Screen
        // Add Color

        // OBJ
        // Size
        // Offset
        // vCounter
        // Size

        uint16 mainLine[256] = {0};
        uint16 subLine[256] = {0};
        uint8 prior[256] = {0};
        bool addSubable[256] = {0};

        // Apply backdrop by default
        for (int i = 0; i < 256; i++)
        {
            mainLine[i] = cgram[0];
            addSubable[i] = bdpAddSub;
            subLine[i] = constColor;
        }

        // Handle objects

        if (OBJonMainScreen || OBJonSubScreen)
        {
            for (int i = 127; i >= 0; i--)
            {

                bool objSizeFlag = objs[i].size;

                uint8 objSize = 8;
                switch (objAvailSize)
                {
                case 0:
                    objSize = objSizeFlag ? 16 : 8;
                    break;
                case 1:
                    objSize = objSizeFlag ? 32 : 8;
                    break;
                case 2:
                    objSize = objSizeFlag ? 64 : 8;
                    break;
                case 3:
                    objSize = objSizeFlag ? 32 : 16;
                    break;
                case 4:
                    objSize = objSizeFlag ? 64 : 16;
                    break;
                case 5:
                    objSize = objSizeFlag ? 64 : 32;
                    break;

                default:
                    break;
                }

                if (vCounter < objs[i].objY || vCounter >= objs[i].objY + objSize || 255 < objs[i].objX || 0 > objs[i].objX + objSize) // Obj not on line
                {
                    continue;
                }

                uint8 tPrior = 0;
                if (mode == 0 || mode == 1)
                {
                    tPrior = (objs[i].objPrior * 3) + 3;
                }
                else
                {
                    tPrior = (objs[i].objPrior * 2) + 2;
                }
                uint8 tileY = (vCounter - objs[i].objY) >> 3;
                uint8 y = (vCounter - objs[i].objY) & 0x07;

                for (uint8 hOff = 0; hOff < objSize; hOff++)
                {
                    int hCnt = objs[i].objX + hOff;
                    if (hCnt < 0)
                        continue;
                    if (hCnt > 255)
                        break;

                    if (prior[hCnt] > tPrior)
                        continue;

                    uint8 colorIdx = 0;
                    uint8 tileX = hOff >> 3;

                    if (objs[i].objHF)
                        tileX = ((objSize >> 3) - 1) - tileX;
                    if (objs[i].objVF)
                        tileY = ((objSize >> 3) - 1) - tileY;

                    uint8 x = hOff & 0x07;

                    uint16 chr = objs[i].objChrName;
                    chr += (uint16)tileX + ((uint16)tileY << 4);

                    if (objs[i].objHF)
                        x = 7 - x;
                    if (objs[i].objVF)
                        y = 7 - y;

                    uint16 objCharAddr = (objs[i].objChrTable ? OBJChrTable2BaseAddr : OBJChrTable1BaseAddr) + (chr << 4);

                    colorIdx |= ((VRAMRead(objCharAddr + y) >> (7 - x)) & 0x01);

                    colorIdx |= ((VRAMRead(objCharAddr + y) >> (15 - x)) & 0x01) << 1;

                    colorIdx |= ((VRAMRead(objCharAddr + y + 8) >> (7 - x)) & 0x01) << 2;

                    colorIdx |= ((VRAMRead(objCharAddr + y + 8) >> (15 - x)) & 0x01) << 3;

                    if (colorIdx == 0)
                        continue;

                    uint16 col = cgram[0b10000000 | (objs[i].objClrPal << 4) | colorIdx];

                    if (OBJonMainScreen)
                    {
                        mainLine[hCnt] = col;
                        if (objs[i].objClrPal >= 4)
                            addSubable[hCnt] = objAddSub;
                    }
                    if (OBJonSubScreen)
                        subLine[hCnt] = col;
                    prior[hCnt] = tPrior;
                }
            }
        }

        if ((BG1onMainScreen || BG1onSubScreen) && mode <= 6) // BG1 conditions
        {
            uint16 vOffset = ((vCounter << interlacing) + BG1VScroll);

            for (int hCnt = 0; hCnt < 256; hCnt++)
            {
                uint8 tileY = (vOffset >> 3) & ((BG1SCSize & 0x2) ? 63 : 31);

                uint8 y = vOffset & 0x07;
                uint16 hOffset = hCnt + BG1HScroll;

                // To the right of the "&" is the logic for wrapping
                uint8 tileX = (hOffset >> 3) & ((BG1SCSize & 0x1) ? 63 : 31);

                uint8 x = hOffset & 0x07;

                // The tileY shift is done in 2 steps to make sure the first bit is 0
                uint8 tileS = ((tileY >> 4) & 0x02) | (tileX >> 5);

                tileX &= 0x1f;
                tileY &= 0x1f;

                if (BG1SCSize == 2 && tileS == 2)
                {
                    tileS = 1;
                }

                uint16 tileNumber = (tileS << 10) + (tileY << 5) + tileX;

                uint16 tileData = VRAMRead(BG1BaseAddr + tileNumber);

                if (tileData & 0b0100000000000000)
                    x = 7 - x;
                if (tileData & 0b1000000000000000)
                    y = 7 - y;

                uint8 tPrior = 0;
                if (mode == 0 || mode == 1)
                    tPrior = (tileData & 0b0010000000000000) ? 11 : 8;
                else
                    tPrior = (tileData & 0b0010000000000000) ? 7 : 3;
                if (prior[hCnt] > tPrior)
                    continue;

                uint8 pal = (tileData & 0b0001110000000000) >> 10;
                uint16 chr = tileData & 0b0000001111111111;
                // 2bpp 8x8 -> 8 -> 3 shift
                // 4bpp 8x8 -> 16 -> 4 shift
                // 8bpp 8x8 -> 32 -> 5 shift
                uint16 tileCharAddr = BG1ChrsBaseAddr + (chr << (3 + (mode > 0) + (mode == 3 || mode == 4)));

                uint8 colorIdx = 0;

                colorIdx |= ((VRAMRead(tileCharAddr + y) >> (7 - x)) & 0x01);

                colorIdx |= ((VRAMRead(tileCharAddr + y) >> (15 - x)) & 0x01) << 1;

                if (mode > 0)
                {
                    colorIdx |= ((VRAMRead(tileCharAddr + y + 8) >> (7 - x)) & 0x01) << 2;
                    colorIdx |= ((VRAMRead(tileCharAddr + y + 8) >> (15 - x)) & 0x01) << 3;
                }
                if (mode == 3 || mode == 4)
                {
                    colorIdx |= ((VRAMRead(tileCharAddr + y + 16) >> (7 - x)) & 0x01) << 4;
                    colorIdx |= ((VRAMRead(tileCharAddr + y + 16) >> (15 - x)) & 0x01) << 5;
                    colorIdx |= ((VRAMRead(tileCharAddr + y + 24) >> (7 - x)) & 0x01) << 6;
                    colorIdx |= ((VRAMRead(tileCharAddr + y + 24) >> (15 - x)) & 0x01) << 7;
                }

                if (colorIdx == 0)
                    continue;

                uint16 col;
                switch (mode)
                {
                case 0:
                    col = cgram[pal << 2 | colorIdx]; // + offsets for other BGs
                    break;
                case 1:
                    col = cgram[pal << 4 | colorIdx];
                    break;
                case 2:
                    col = cgram[pal << 4 | colorIdx];
                    break;
                case 3:
                    col = directColor ? colorIdx : cgram[colorIdx];
                    break;
                case 4:
                    col = directColor ? colorIdx : cgram[colorIdx];
                    break;
                case 5:
                    col = cgram[pal << 4 | colorIdx];
                    break;
                case 6:
                    col = cgram[pal << 4 | colorIdx];
                    break;

                default:
                    col = 0;
                    break;
                }

                if (BG1onMainScreen)
                {
                    mainLine[hCnt] = col;
                    addSubable[hCnt] = bg1AddSub;
                }
                if (BG1onSubScreen)
                    subLine[hCnt] = col;
                prior[hCnt] = tPrior;
            }
        }

        if ((BG1onMainScreen || BG1onSubScreen) && mode == 7) // BG1 conditions MODE 7!!!
        {

            int16_t cx = (centerX & 0x0FFF) | ((centerX & 0x1000) ? 0xF000 : 0x0000);
            int16_t cy = (centerY & 0x0FFF) | ((centerY & 0x1000) ? 0xF000 : 0x0000);
            int16_t A = mode7A;
            int16_t B = mode7B;
            int16_t C = mode7C;
            int16_t D = mode7D;
            for (int hCnt = 0; hCnt < 256; hCnt++)
            {
                if (prior[hCnt] > 3)
                    continue;
                uint16 vOffset = ((vCounter << interlacing) + BG1VScroll);
                uint16 hOffset = (hCnt + BG1HScroll);

                uint16 transX = (A / 256.f) * ((int16_t)hOffset - cx) + (B / 256.f) * (vOffset - cy) + cx;
                uint16 transY = (C / 256.f) * ((int16_t)hOffset - cx) + (D / 256.f) * (vOffset - cy) + cy;

                hOffset = transX;
                vOffset = transY;
                // Screen is 128x128 tiles of 8x8

                // To the right of the "&" is the logic for wrapping
                uint8 tileX = (hOffset >> 3) & 1023;
                uint8 tileY = (vOffset >> 3) & 1023;
                uint8 x = hOffset & 0x07;
                uint8 y = vOffset & 0x07;

                tileX &= 0x7f;
                tileY &= 0x7f;

                uint16 tileNumber = (tileY << 7) + tileX;

                // Mode7's base is always 0x0000
                uint16 tileData = VRAMRead(tileNumber);

                uint16 chrName = tileData & 0x00FF;

                uint16 tileCharAddr = (chrName << 6);

                uint8 colorIdx = (VRAMRead(tileCharAddr + (y << 3) + x) & 0xFF00) >> 8;

                if (colorIdx == 0)
                    continue;

                if (BG1onMainScreen)
                {
                    mainLine[hCnt] = directColor ? colorIdx : cgram[colorIdx];
                    addSubable[hCnt] = bg1AddSub;
                }
                if (BG1onSubScreen)
                    subLine[hCnt] = directColor ? colorIdx : cgram[colorIdx];
                prior[hCnt] = 3;
            }
        }

        if ((BG2onMainScreen || BG2onSubScreen) && mode < 6) // BG2 conditions
        {
            uint16 vOffset = ((vCounter << interlacing) + BG2VScroll);

            for (int hCnt = 0; hCnt < 256; hCnt++)
            {
                uint8 tileY = (vOffset >> 3) & ((BG2SCSize & 0x2) ? 63 : 31);

                uint8 y = vOffset & 0x07;
                uint16 hOffset = (hCnt + BG2HScroll);

                // To the right of the "&" is the logic for wrapping
                uint8 tileX = (hOffset >> 3) & ((BG2SCSize & 0x1) ? 63 : 31);

                uint8 x = hOffset & 0x07;

                // The tileY shift is done in 2 steps to make sure the first bit is 0
                uint8 tileS = ((tileY >> 5) << 1) | (tileX >> 5);

                tileX &= 0x1f;
                tileY &= 0x1f;

                if (BG2SCSize == 2 && tileS == 2)
                {
                    tileS = 1;
                }

                // (32*32*tileS)
                uint16 tileNumber = (tileS * 32 * 32) + (tileY << 5) + tileX;

                uint16 tileData = VRAMRead(BG2BaseAddr + tileNumber);

                if (tileData & 0b0100000000000000)
                    x = 7 - x;
                if (tileData & 0b1000000000000000)
                    y = 7 - y;

                uint8 tPrior = 0;
                if (mode == 0 || mode == 1)
                    tPrior = (tileData & 0b0010000000000000) ? 10 : 7;
                else
                    tPrior = (tileData & 0b0010000000000000) ? 5 : 1;

                if (prior[hCnt] > tPrior)
                    continue;

                uint8 pal = (tileData & 0b0001110000000000) >> 10;
                uint16 chr = tileData & 0b0000001111111111;

                // 2bpp 8x8 -> 8 -> 3 shift
                // 4bpp 8x8 -> 16 -> 4 shift

                uint16 tileCharAddr = BG2ChrsBaseAddr + (chr << (3 + (mode == 1 || mode == 2 || mode == 3)));

                uint8 colorIdx = 0;

                colorIdx |= ((VRAMRead(tileCharAddr + y) >> (7 - x)) & 0x01);

                colorIdx |= ((VRAMRead(tileCharAddr + y) >> (15 - x)) & 0x01) << 1;

                if (mode == 1 || mode == 2 || mode == 3)
                {
                    colorIdx |= ((VRAMRead(tileCharAddr + y + 8) >> (7 - x)) & 0x01) << 2;
                    colorIdx |= ((VRAMRead(tileCharAddr + y + 8) >> (15 - x)) & 0x01) << 3;
                }

                if (colorIdx == 0)
                    continue;

                uint16 colAdd;
                switch (mode)
                {
                case 0:
                    colAdd = pal << 2 | colorIdx; // + offsets for other BGs
                    break;
                case 1:
                    colAdd = pal << 4 | colorIdx;
                    break;
                case 2:
                    colAdd = pal << 4 | colorIdx;
                    break;
                case 3:
                    colAdd = pal << 4 | colorIdx;
                    break;
                case 4:
                    colAdd = pal << 2 | colorIdx;
                    break;
                case 5:
                    colAdd = pal << 2 | colorIdx;
                    break;
                default:
                    break;
                }

                if (BG2onMainScreen)
                {
                    mainLine[hCnt] = cgram[colAdd];
                    addSubable[hCnt] = bg2AddSub;
                }
                if (BG2onSubScreen)
                    subLine[hCnt] = cgram[colAdd];
                prior[hCnt] = tPrior;
            }
        }

        if ((BG3onMainScreen || BG3onSubScreen) && mode < 2) // BG3 conditions
        {
            uint16 vOffset = ((vCounter << interlacing) + BG3VScroll);

            for (int hCnt = 0; hCnt < 256; hCnt++)
            {
                uint8 tileY = (vOffset >> 3) & ((BG3SCSize & 0x2) ? 63 : 31);
                uint8 y = vOffset & 0x07;
                uint16 hOffset = (hCnt + BG3HScroll);

                // To the right of the "&" is the logic for wrapping
                uint8 tileX = (hOffset >> 3) & ((BG3SCSize & 0x1) ? 63 : 31);
                uint8 x = hOffset & 0x07;

                // The tileY shift is done in 2 steps to make sure the first bit is 0
                uint8 tileS = ((tileY >> 5) << 1) | (tileX >> 5);

                tileX &= 0x1f;
                tileY &= 0x1f;

                if (BG3SCSize == 2 && tileS == 2)
                {
                    tileS = 1;
                }
                // (32*32*tileS)
                uint16 tileNumber = (tileS << 10) + (tileY << 5) + tileX;

                uint16 tileData = VRAMRead(BG3BaseAddr + tileNumber);

                if (tileData & 0b0100000000000000)
                    x = 7 - x;
                if (tileData & 0b1000000000000000)
                    y = 7 - y;

                uint8 tPrior = (tileData & 0b0010000000000000) ? 5 : 2;
                if (bg3PriorMode)
                    tPrior = 13;

                if (prior[hCnt] > tPrior)
                    continue;

                uint8 pal = (tileData & 0b0001110000000000) >> 10;
                uint16 chr = tileData & 0b0000001111111111;

                uint16 tileCharAddr = BG3ChrsBaseAddr + chr * 8;

                uint8 colorIdx = 0;

                colorIdx |= ((VRAMRead(tileCharAddr + y) >> (7 - x)) & 0x01);

                colorIdx |= ((VRAMRead(tileCharAddr + y) >> (15 - x)) & 0x01) << 1;

                if (colorIdx == 0)
                    continue;

                uint16 colAdd;

                colAdd = pal << 2 | colorIdx; // + offsets for other BGs

                if (BG3onMainScreen)
                {
                    mainLine[hCnt] = cgram[colAdd];
                    addSubable[hCnt] = bg3AddSub;
                }
                if (BG3onSubScreen)
                    subLine[hCnt] = cgram[colAdd];
                prior[hCnt] = tPrior;
            }
        }

        if ((BG4onMainScreen || BG4onSubScreen) && mode == 0) // BG4 conditions
        {
            uint16 vOffset = ((vCounter << interlacing) + BG4VScroll);

            for (int hCnt = 0; hCnt < 256; hCnt++)
            {
                uint8 tileY = (vOffset >> 3) & ((BG4SCSize & 0x2) ? 63 : 31);
                uint8 y = vOffset & 0x07;
                uint16 hOffset = (hCnt + BG4HScroll);

                // To the right of the "&" is the logic for wrapping
                uint8 tileX = (hOffset >> 3) & ((BG4SCSize & 0x1) ? 63 : 31);
                uint8 x = hOffset & 0x07;

                // The tileY shift is done in 2 steps to make sure the first bit is 0
                uint8 tileS = ((tileY >> 5) << 1) | (tileX >> 5);

                tileX &= 0x1f;
                tileY &= 0x1f;

                if (BG4SCSize == 2 && tileS == 2)
                {
                    tileS = 1;
                }
                // (32*32*tileS)
                uint16 tileNumber = (tileS << 10) + (tileY << 5) + tileX;

                uint16 tileData = VRAMRead(BG4BaseAddr + tileNumber);

                if (tileData & 0b0100000000000000)
                    x = 7 - x;
                if (tileData & 0b1000000000000000)
                    y = 7 - y;

                uint8 tPrior = (tileData & 0b0010000000000000) ? 4 : 1;

                if (prior[hCnt] > tPrior)
                    continue;

                uint8 pal = (tileData & 0b0001110000000000) >> 10;
                uint16 chr = tileData & 0b0000001111111111;

                uint16 tileCharAddr = BG4ChrsBaseAddr + chr * 8;

                uint8 colorIdx = 0;

                colorIdx |= ((VRAMRead(tileCharAddr + y) >> (7 - x)) & 0x01);

                colorIdx |= ((VRAMRead(tileCharAddr + y) >> (15 - x)) & 0x01) << 1;

                if (colorIdx == 0)
                    continue;

                uint16 colAdd;

                colAdd = pal << 2 | colorIdx; // + offsets for other BGs

                if (BG4onMainScreen)
                {
                    mainLine[hCnt] = cgram[colAdd];
                    addSubable[hCnt] = bg4AddSub;
                }
                if (BG4onSubScreen)
                    subLine[hCnt] = cgram[colAdd];
                prior[hCnt] = tPrior;
            }
        }

        int16_t mainR;
        int16_t mainG;
        int16_t mainB;

        for (int hCnt = 0; hCnt < 256; hCnt++)
        {
            uint16 selectedCol = constColorSel ? constColor : subLine[hCnt];
            int16_t R = (selectedCol & 0b0000000000011111) >> 0;
            int16_t G = (selectedCol & 0b0000001111100000) >> 5;
            int16_t B = (selectedCol & 0b0111110000000000) >> 10;

            mainR = ((mainLine[hCnt] & 0b0000000000011111) >> 0);
            mainG = ((mainLine[hCnt] & 0b0000001111100000) >> 5);
            mainB = ((mainLine[hCnt] & 0b0111110000000000) >> 10);

            if (addSubable[hCnt])
            {

                mainR = (mainR + (addSub ? -R : R)) / (addSubHalf ? 2 : 1);
                mainR = mainR > 31 ? 31 : mainR;
                mainR = mainR < 0 ? 0 : mainR;

                mainG = (mainG + (addSub ? -G : G)) / (addSubHalf ? 2 : 1);
                mainG = mainG > 31 ? 31 : mainG;
                mainG = mainG < 0 ? 0 : mainG;

                mainB = (mainB + (addSub ? -B : B)) / (addSubHalf ? 2 : 1);
                mainB = mainB > 31 ? 31 : mainB;
                mainB = mainB < 0 ? 0 : mainB;
            }
            else
            {
                mainR = (mainR + (addSub ? -R : R));
                mainR = mainR > 31 ? 31 : mainR;
                mainR = mainR < 0 ? 0 : mainR;

                mainG = (mainG + (addSub ? -G : G));
                mainG = mainG > 31 ? 31 : mainG;
                mainG = mainG < 0 ? 0 : mainG;

                mainB = (mainB + (addSub ? -B : B));
                mainB = mainB > 31 ? 31 : mainB;
                mainB = mainB < 0 ? 0 : mainB;
            }

            uint8 fade = ((15 - fadeValue) << 4);
            mainR = (mainR << 3);
            mainR = fade > mainR ? 0 : (mainR - fade);

            mainG = (mainG << 3);
            mainG = fade > mainG ? 0 : (mainG - fade);

            mainB = (mainB << 3);
            mainB = fade > mainB ? 0 : (mainB - fade);

            fb[FB_WIDTH * vCounter + hCnt] = forceBlank ? 0 : MFB_ARGB(255, mainR, mainG, mainB);
        }
    }

    // hCnt from 0 to 339
    // vCnt from 0 to 261
    if (hCounter == 339)
    {
        hCounter = 0;
        if (vCounter == 261)
        {
            vCounter = 0;
            frameCount++;
        }
        else
        {
            vCounter++;
        }
    }
    else
    {
        hCounter++;
    }

    hBlank = vBlank = false;

    if (vCounter <= 224)
    {
        if (hCounter > 255)
        {
            // HBlank
            hBlank = true;
        }
    }
    else
    {

        // VBlank

        vBlank = true;
    }
}

#pragma endregion

#pragma region CTRL

uint8 CTRL_IORead(add24 address)
{
    switch (address)
    {

    case 0x4017: // LCTRLREG2, Manual controller 1
        return LCTRLREG2;
        break;

    case 0x4016: // LCTRLREG1, Manual controller 1
        LCTRLREG1 = player1 >> (15 - legCnt1);
        legCnt1++;
        if (legCnt1 == 16)
            legCnt1 = 0;
        return LCTRLREG1;
        break;

    case 0x4212:
        return HVBJOY;
        break;
    case 0x4213:
        return RDIO;
        break;
    // Standard Controllers
    case 0x4218:
        return ctrl[0].LOW;
        break;
    case 0x4219:
        return ctrl[0].HIGH;
        break;

    case 0x421A:
        return ctrl[1].LOW;
        break;
    case 0x421B:
        return ctrl[1].HIGH;
        break;

    case 0x421C:
        return ctrl[2].LOW;
        break;
    case 0x421D:
        return ctrl[2].HIGH;
        break;

    case 0x421E:
        return ctrl[3].LOW;
        break;
    case 0x421F:
        return ctrl[3].HIGH;
        break;
    }
    return 0;
}
void CTRL_IOWrite(add24 address, uint8 data)
{
    switch (address)
    {

    case 0x4016: // LCTRLREG1, Manual controller 1
        LCTRLREG1 = data;
        break;
    case 0x4201:
        WRIO = data;
        break;
    }
}

void CTRL_Step()
{
    if (true || stdCtrlEn)
    {
        ctrl[0].LOW = player1;
        ctrl[0].HIGH = player1 >> 8;
    }
    else
    {
        ctrl[0].LOW = 0;
        ctrl[0].HIGH = 0;
    }
}

void CTRL_KeyPress(uint8 key)
{
    // 4 - R
    // 5 - L
    // 6 - X
    // 7 - A
    // 8 - Right
    // 9 - Left
    // 10 - Down
    // 11 - UP
    // 12 - Start
    // 13 - Select
    // 14 - Y
    // 15 - B

    player1 |= 1 << key;
}
void CTRL_KeyRelease(uint8 key)
{
    // 4 - R
    // 5 - L
    // 6 - X
    // 7 - A
    // 8 - Right
    // 9 - Left
    // 10 - Down
    // 11 - UP
    // 12 - Start
    // 13 - Select
    // 14 - Y
    // 15 - B
    player1 &= ~(1 << key);
}

#pragma endregion

#pragma region DMA

uint8 DMA_IORead(add24 address)
{
    if ((address & 0xFF00) == 0x4300)
    {
        uint8 chNum = (address & 0x00F0) >> 4;
        uint8 chReg = (address & 0x000F);

        switch (chReg)
        {

        case 0x0: // 0x43_0
        {
            uint8 t = 0;
            t |= dmaType[chNum] & 0b00000111;
            t |= dmaFixedAddr[chNum] << 3;
            t |= dmaDecAddr[chNum] << 4;
            t |= dmaIndir[chNum] << 6;
            t |= dmaDir[chNum] << 7;
            return t;
            break;
        }
        case 0x1: // 0x43_1
            return dmaBAddr[chNum];
            break;

        case 0x2: // 0x43_2 ABus/Table Address Low
            return dmaAAddr[chNum];
            break;
        case 0x3: // 0x43_3 ABus/Table Address High
            return dmaAAddr[chNum] >> 8;
            break;
        case 0x4: // 0x43_4 ABus/Table Address Bank
            return dmaAAddr[chNum] >> 16;
            break;

        case 0x5: // 0x43_5 IndirAddr/Counter Low
            return dmaDataPtr[chNum];
            break;
        case 0x6: // 0x43_6 IndirAddr/Counter High
            return dmaDataPtr[chNum] >> 8;
            break;
        case 0x7: // 0x43_7 IndirAddr Bank
            return dmaIndirBank[chNum];
            break;

        case 0x8: // 0x43_8 TableAddr Low
            return dmaTablePtr[chNum];
            break;
        case 0x9: // 0x43_9 TableAddr High
            return dmaTablePtr[chNum] >> 8;
            break;

        case 0xa: // 0x43_a Table Header (Rep + Line Count)
            return dmaLineCnt[chNum];
            break;

        case 0xf: // 0x43_f Mirror of 0x43_b
        case 0xb: // 0x43_b Unused can be written to or read from
            return dmaUnused[chNum];
            break;

        case 0xc: // 0x43_c OPENBUS
            // WriteBus(0xFFFFFF, data);
            break;
        case 0xd: // 0x43_d OPENBUS
            // WriteBus(0xFFFFFF, data);
            break;
        case 0xe: // 0x43_e OPENBUS
            // WriteBus(0xFFFFFF, data);
            break;

        default: // OPENBUS
            // WriteBus(0xFFFFFF, data);
            break;
        }
    }
    else
    {

        switch (address)
        {

        case 0x420b: // MDMAEN
        {
            uint8 t = 0;
            for (int i = 0; i < 8; i++)
                t |= (dmaEn[i] << i);
            return t;
            break;
        }

        case 0x420c: // HDMAEN
            return HDMAEN;

            break;

        default: // OPENBUS
            // WriteBus(, data);
            break;
        }
    }
    return 0;
}
void DMA_IOWrite(add24 address, uint8 data)
{

    if ((address & 0xFF00) == 0x4300)
    {
        uint8 chNum = (address & 0x00F0) >> 4;
        uint8 chReg = (address & 0x000F);

        switch (chReg)
        {

        case 0x0: // 0x43_0
            dmaType[chNum] = data & 0b00000111;
            dmaFixedAddr[chNum] = data & 0b00001000;
            dmaDecAddr[chNum] = data & 0b00010000;
            dmaIndir[chNum] = data & 0b01000000;
            dmaDir[chNum] = data & 0b10000000;
            break;

        case 0x1: // 0x43_1
            dmaBAddr[chNum] = 0x2100 | data;
            break;

        case 0x2: // 0x43_2 ABus/Table Address Low
            dmaAAddr[chNum] = (dmaAAddr[chNum] & 0xFF00) | data;
            break;
        case 0x3: // 0x43_3 ABus/Table Address High
            dmaAAddr[chNum] = (dmaAAddr[chNum] & 0x00FF) | (data << 8);
            break;
        case 0x4: // 0x43_4 ABus/Table Address Bank
            dmaAAddrBank[chNum] = data;
            break;

        case 0x5: // 0x43_5 IndirAddr/Counter Low
            dmaDataPtr[chNum] = (dmaDataPtr[chNum] & 0xFF00) | data;
            break;
        case 0x6: // 0x43_6 IndirAddr/Counter High
            dmaDataPtr[chNum] = (dmaDataPtr[chNum] & 0x00FF) | (data << 8);
            break;
        case 0x7: // 0x43_7 IndirAddr Bank
            dmaIndirBank[chNum] = data;
            break;

        case 0x8: // 0x43_8 TableAddr Low
            dmaTablePtr[chNum] = (dmaTablePtr[chNum] & 0xFF00) | data;
            break;
        case 0x9: // 0x43_9 TableAddr High
            dmaTablePtr[chNum] = (dmaTablePtr[chNum] & 0x00FF) | (data << 8);
            break;

        // TODO : Make sure the following is correct.
        case 0xa: // 0x43_a Table Header (Rep + Line Count)
            dmaCont[chNum] = data & 0b10000000;
            dmaLineCnt[chNum] = data & 0b11111111;
            break;

        case 0xf: // 0x43_f Mirror of 0x43_b
        case 0xb: // 0x43_b Unused can be written to or read from
            dmaUnused[chNum] = data;
            break;

        case 0xc: // 0x43_c OPENBUS
            // WriteBus(0xFFFFFF, data);
            break;
        case 0xd: // 0x43_d OPENBUS
            // WriteBus(0xFFFFFF, data);
            break;
        case 0xe: // 0x43_e OPENBUS
            // WriteBus(0xFFFFFF, data);
            break;

        default: // OPENBUS
            // WriteBus(0xFFFFFF, data);
            break;
        }
    }
    else
    {

        switch (address)
        {

        case 0x420b: // MDMAEN

            for (int i = 0; i < 8; i++)
                dmaEn[i] = (1 << i) & data;
            break;

        case 0x420c: // HDMAEN
            HDMAEN = data;

            break;

        default: // OPENBUS
            // WriteBus(, data);
            break;
        }
    }
}

void DMA_InitializeHDMA()
{
    for (int i = 0; i < 8; i++)
        hdmaEn[i] = (1 << i) & HDMAEN;
    for (int i = 0; i < 8; i++)
    {
        if (!hdmaEn[i])
            continue;

        dmaTablePtr[i] = dmaAAddr[i]; // Current table addres = Start table address
        uint8 line = ReadBus((dmaAAddrBank[i] << 16) | dmaTablePtr[i]);
        if (line == 0x00)
        {
            hdmaEn[i] = false;
        }
        else
        {

            if (line == 0x80)
            {
                dmaLineCnt[i] = 128;
                dmaCont[i] = false;
            }
            else
            {
                dmaLineCnt[i] = line & 0b11111111;
                dmaCont[i] = line & 0b10000000;
            }

            // dmaLineCnt[i] = line;

            hdmaDoTransfer[i] = true;
        }

        if (dmaIndir[i])
        {
            dmaTablePtr[i]++;
            add24 addL = (dmaTablePtr[i]) & 0xFFFF;
            addL |= (dmaAAddrBank[i] << 16);
            dmaTablePtr[i]++;
            add24 addH = (dmaTablePtr[i]) & 0xFFFF;
            addH |= (dmaAAddrBank[i] << 16);
            dmaDataPtr[i] = ReadBus(addL) | (ReadBus(addH) << 8);
            dmaTablePtr[i]++;
        }
        else
        {
            dmaTablePtr[i]++;
        }
        // cout << "========== HDMA[" << i << "] Init ==========" << endl;
        // cout << "table header : " << hex << (uint16)line << endl;
        // cout << "table addr : " << hex << ((dmaAAddrBank[i] << 16) | dmaAAddr[i]) << endl;
        // cout << "table ptr : " << hex << (dmaTablePtr[i]) << endl;
        // cout << "type : " << dec << (uint16)dmaType[i] << endl;
        // cout << "abs/indir : " << (dmaIndir[i] ? "indir" : "abs") << endl;
        // cout << "cont : " << (dmaCont[i] ? "y" : "n") << endl;
        // cout << "count : " << hex << (uint16)dmaLineCnt[i] << endl;
        // cout << "b addr : " << hex << dmaBAddr[i] << endl;
    }
}
void DMA_Step(bool hBlank)
{
    // Check for active HDMA transactions
    // Do the active HDMA transactions
    // Check for active Normal DMA transactions
    // Do the active Normal DMA transactions

    // Regs:
    //  Param :
    //       0,1,2 :
    //       3 : 1 -> Fixed address, 0 -> Automatically change address
    //       4 : 1 -> dec , 0 -> inc
    //       5 : unused
    //       6 : (HDMA ONLY) 1 -> INDIRECT , 0 -> ABSOLUTE
    //       7 : Dir, 1 -> B to A , 0 -> A to B
    //

    // BBADD : B-bus address, 0021XX will become the B-bus address

    // ABADD : A-bus address

    // DMABYTESL DMABYTESH and HDMAADDB
    // General DMA : holds the number of bytes to transfer
    // HDMA : if using indirect mode, the inderct address will be placed here

    // DMAA2ADDL: (not sure)
    // General DMA : Not used.
    // HDMA : IDK really

    // HDMALINES : number of lines for HDMA

    // For now, we preform 1 transfer/cycle

    dmaActive = false;
    for (int i = 0; i < 8; i++)
    {
        dmaActive = dmaActive || (hBlank && hdmaEn[i]) || dmaEn[i];
    }

    if (hBlank)
    {

        for (int i = 0; i < 8; i++)
        {

            if (!hdmaEn[i])
                continue;

            if (hdmaDoTransfer[i])
            {

                add24 dataAddr;
                uint8 bytesTrf = 0;
                if (dmaIndir[i])
                {
                    dataAddr = dmaDataPtr[i];
                    dataAddr |= (dmaIndirBank[i] << 16);
                }
                else
                {
                    dataAddr = dmaTablePtr[i];
                    dataAddr |= (dmaAAddrBank[i] << 16);
                }

                uint8 data;
                switch (dmaType[i])
                {
                case 0:
                {
                    data = ReadBus(dataAddr);
                    WriteBus(dmaBAddr[i], data);
                    bytesTrf = 1;
                    break;
                }
                case 1:
                {

                    data = ReadBus(dataAddr);
                    WriteBus(dmaBAddr[i], data);
                    data = ReadBus(dataAddr + 1);
                    WriteBus(dmaBAddr[i] + 1, data);
                    bytesTrf = 2;
                    break;
                }
                case 6:
                case 2:
                {

                    data = ReadBus(dataAddr);
                    WriteBus(dmaBAddr[i], data);
                    data = ReadBus(dataAddr + 1);
                    WriteBus(dmaBAddr[i], data);
                    bytesTrf = 2;
                    break;
                }
                case 7:
                case 3:
                {

                    data = ReadBus(dataAddr);

                    WriteBus(dmaBAddr[i], data);
                    data = ReadBus(dataAddr + 1);
                    WriteBus(dmaBAddr[i], data);
                    data = ReadBus(dataAddr + 2);
                    WriteBus(dmaBAddr[i] + 1, data);
                    data = ReadBus(dataAddr + 3);
                    WriteBus(dmaBAddr[i] + 1, data);
                    bytesTrf = 4;
                    break;
                }
                case 4:
                {

                    data = ReadBus(dataAddr);
                    WriteBus(dmaBAddr[i], data);
                    data = ReadBus(dataAddr + 1);
                    WriteBus(dmaBAddr[i] + 1, data);
                    data = ReadBus(dataAddr + 2);
                    WriteBus(dmaBAddr[i] + 2, data);
                    data = ReadBus(dataAddr + 3);
                    WriteBus(dmaBAddr[i] + 3, data);
                    bytesTrf = 4;
                    break;
                }
                case 5:
                {
                    data = ReadBus(dataAddr);

                    WriteBus(dmaBAddr[i], data);
                    data = ReadBus(dataAddr + 1);
                    WriteBus(dmaBAddr[i] + 1, data);
                    data = ReadBus(dataAddr + 2);
                    WriteBus(dmaBAddr[i], data);
                    data = ReadBus(dataAddr + 3);
                    WriteBus(dmaBAddr[i] + 1, data);
                    bytesTrf = 4;
                    break;
                }

                default:
                    break;
                }

                if (dmaIndir[i])
                {
                    dmaDataPtr[i] += bytesTrf;
                }
                else
                {
                    dmaTablePtr[i] += bytesTrf;
                }
            }

            dmaLineCnt[i] -= 1;
            hdmaDoTransfer[i] = dmaCont[i]; // dmaLineCnt[i] & 0b10000000;

            if (dmaLineCnt[i] == 0) // Re-initialize for the next table
            {
                uint8 line;

                line = ReadBus((dmaAAddrBank[i] << 16) | dmaTablePtr[i]);
                if (line == 0x00)
                {
                    hdmaEn[i] = false;
                }
                else
                {

                    if (line == 0x80)
                    {
                        dmaLineCnt[i] = 128;
                        dmaCont[i] = false;
                    }
                    else
                    {
                        dmaLineCnt[i] = line & 0b11111111;
                        dmaCont[i] = line & 0b10000000;
                    }

                    // dmaLineCnt[i] = line;

                    hdmaDoTransfer[i] = true;
                }

                if (dmaIndir[i])
                {
                    dmaTablePtr[i]++;
                    add24 addL = (dmaTablePtr[i]) & 0xFFFF;
                    addL |= (dmaAAddrBank[i] << 16);

                    dmaTablePtr[i]++;
                    add24 addH = (dmaTablePtr[i]) & 0xFFFF;
                    addH |= (dmaAAddrBank[i] << 16);

                    dmaDataPtr[i] = ReadBus(addL) | (ReadBus(addH) << 8);
                    dmaTablePtr[i]++;
                }
                else
                {
                    dmaTablePtr[i]++;
                }
            }

            // cout << "-=-=-=-=-=-=-=-=-=HDMA[" << i << "]=-=-=-=-=-=-=-=-=-=" << endl;
            // cout << "table addr : " << hex << ((dmaAAddrBank[i] << 16) | dmaAAddr[i]) << endl;
            // cout << "table ptr : " << hex << (dmaTablePtr[i]) << endl;
            // cout << "type : " << dec << (uint16)dmaType[i] << endl;
            // cout << "abs/indir : " << (dmaIndir[i] ? "indir" : "abs") << endl;
            // cout << "cont : " << (dmaCont[i] ? "y" : "n") << endl;
            // cout << "count : " << hex << (uint16)dmaLineCnt[i] << endl;
            // cout << "b addr : " << hex << dmaBAddr[i] << endl;
        }
    }
    else
    {

        int i = 0;
        for (; i < 8; i++)
            if (dmaEn[i])
                break;
        if (i == 8)
            return;

        add24 aAddr = dmaAAddrBank[i] << 16 | dmaAAddr[i];

        // cout << "-=-=-=-=-=-=-=-=-=DMA[" << i << "]=-=-=-=-=-=-=-=-=-=" << endl;
        // cout << "type : " << dec << (uint16)dmaType[i] << endl;
        // cout << "fixed : " << (dmaFixedAddr[i] ? "fixed" : "change") << endl;
        // cout << "inc : " << (dmaDecAddr[i] ? "-" : "+") << endl;
        // cout << "dir : " << (dmaDir[i] ? "B->A" : "A->B") << endl;
        // cout << "b addr : " << hex << dmaBAddr[i] << endl;
        // cout << "a addr : " << hex << aAddr << endl;
        // cout << "bytes : " << hex << dmaDataPtr[i] << endl;
        // cout << "state : " << dec << state << endl;

        add24 offset = dmaType[i] == 0 ? 0 : dmaType[i] == 1 ? state % 2
                                         : dmaType[i] == 2   ? 0
                                         : dmaType[i] == 3   ? (state >> 1) % 2
                                         : dmaType[i] == 4   ? state % 4
                                         : dmaType[i] == 5   ? state % 2
                                         : dmaType[i] == 6   ? 0
                                         : dmaType[i] == 7   ? state >> 1 % 2
                                                             : 0;

        add24 bEffAdd = (dmaBAddr[i] + offset);
        if (dmaDir[i]) // B to A
        {
            uint8 t = ReadBus(bEffAdd);
            // cout << "effective Addr : " << hex << aAddr << endl;
            // cout << "data : " << hex << (uint16)t << endl;
            WriteBus(aAddr, t);
        }
        else // A to B
        {
            uint8 t = ReadBus(aAddr);
            // cout << "effective B Addr : " << hex << bAddr + offset << endl;
            // cout << "effective A Addr : " << hex << aEffAdd << endl;
            // cout << "data : " << hex << (uint16)t << endl;
            WriteBus(bEffAdd, t);
        }

        if (!dmaFixedAddr[i])
        {
            if (dmaDecAddr[i])
            {
                dmaAAddr[i]--;
            }
            else
            {
                dmaAAddr[i]++;
            }
        }

        dmaDataPtr[i]--;
        state++;
        if (dmaDataPtr[i] == 0)
        {
            state = 0;
            dmaEn[i] = 0;
        }
    }
}

#pragma endregion

#pragma region Cartridge

void LoadRom(const char *address)
{
    FILE *romFile;
    romFile = fopen(address, "rb");

    fseek(romFile, 0, SEEK_END);
    romSize = ftell(romFile);
    rewind(romFile);

    printf("Size : %d\n", romSize);

    rom = (uint8_t *)malloc(romSize);

    size_t bytes_read = fread(rom, 1, romSize, romFile);

    fclose(romFile);

    uint8 readByte;

    int headerLoc;
    //                    LoROM        LoROM      HiROM          HiROM     ExHiROM      ExHiROM
    int headerLocs[] = {0x007fc0, 0x007fc0 + 512, 0x00ffc0, 0x00ffc0 + 512, 0x40ffc0, 0x40ffc0 + 512};
    uint8 romTypeNum[] = {0, 0, 1, 1, 5, 5};

    int i = 0;
    do
    {
        headerLoc = headerLocs[i];

        memcpy(&readByte, rom + headerLoc + 21, 1);

        if ((readByte & 0x0F) == romTypeNum[i])
        {
            memcpy(&readByte, rom + headerLoc + 23, 1);

            if ((((1 << (readByte + 10)) + (i % 2 ? 512 : 0)) >= (romSize) && ((1 << (readByte + 9)) + (i % 2 ? 512 : 0)) < (romSize)) ||
                (((1 << (readByte + 11)) + (i % 2 ? 512 : 0)) >= (romSize) && ((1 << (readByte + 10)) + (i % 2 ? 512 : 0)) < (romSize)))
                break;
        }

        i++;

        if (i == 6)
        {
            printf("Couldn't identify the ROM.\n");
            exit(0);
        }
    } while (1);

    // At this point we can do a checksum to make sure of rom map type and header location
    headerLoc = headerLocs[i];

    memcpy(title, rom + headerLoc + 0, 21); // Title

    switch (romTypeNum[i])
    {
    case 0:
        romMapType = LoROM;
        break;
    case 1:
        romMapType = HiROM;
        break;
    case 5:
        romMapType = ExHiROM;
        break;
    }

    memcpy(&readByte, rom + headerLoc + 21, 1); // Speed And Type - 0xFFD5
    speed = readByte & 0b00010000;

    memcpy(&readByte, rom + headerLoc + 22, 1); // 0xFFD6

    printf("Chipset settings : %x \n", (uint16)readByte);
    if (readByte == 0x02)
    {
        hasSram = 1;
        memcpy(&readByte, rom + headerLoc + 24, 1); // SramSize - 0xFFD8

        sramSize = 1 << (readByte + 10);
        printf("Sram size : %d\n", sramSize);
        sram = (uint8_t *)malloc(sramSize);
    }

    if (i % 2)
    {
        rom += 512;
    }

    printf("Title : %s\n", title);
    printf("Speed : %s\n", (speed ? "Fast" : "Slow"));
    printf("Map Type : %s\n", (romMapType == LoROM ? "LoROM" : romMapType == HiROM ? "HiROM"
                                                           : romMapType == ExHiROM ? "ExHiROM"
                                                                                   : "Invalid"));
}

#pragma endregion

#pragma region MDU
void MDU_IOWrite(add24 address, uint8 data)
{
    switch (address)
    {
    case 0X4202:
        WRMPYA = data;
        break;
    case 0X4203:
    {
        WRMPYB = data;
        uint16 c = (uint16)WRMPYA * (uint16)WRMPYB;
        RDMPYL = c;
        RDMPYH = c >> 8;
        break;
    }

    case 0X4204:
        WRDIVL = data;
        break;
    case 0X4205:
        WRDIVH = data;
        break;
    case 0X4206:
    {
        WRDIVB = data;
        uint16 c = WRDIVB != 0 ? ((uint16)WRDIVL | (((uint16)WRDIVH) << 8)) / (uint16)WRDIVB : 0;
        RDDIVL = c;
        RDDIVH = c >> 8;
        uint8 rem = WRDIVB != 0 ? ((uint16)WRDIVL | (((uint16)WRDIVH) << 8)) % (uint16)WRDIVB : 0;
        RDMPYL = rem;
        RDMPYH = rem >> 8;
        break;
    }

    default:
        break;
    }
}

uint8 MDU_IORead(add24 address)
{
    switch (address)
    {
    case 0x4214: // RDDIVL
        return RDDIVL;
        break;
    case 0x4215: // RDDIVH
        return RDDIVH;
        break;
    case 0x4216: // RDMPYL
        return RDMPYL;
        break;
    case 0x4217: // RDMPYH
        return RDMPYH;
        break;

    default:
        return 0;
        break;
    }
    return 0;
}

#pragma endregion

void emu()
{
    CPU_Reset();
    PPU_Reset();
    APU_Init();
    VTIMEL = 0xff;
    VTIMEH = 0x01;
    HTIMEL = 0xff;
    HTIMEH = 0x01;
    RDNMI = RDNMI & 0x7F;

    while (true) // Emulation Loop
    {

        if (!dmaActive)
        {

            if (emuStep % 8 == 0)
            {
                CPU_Step();
            }
        }

        if (emuStep % 2 == 0)
        {

            DMA_Step(!hdmaRan);
            if (!hdmaRan)
            {
                hdmaRan = true;
            }
        }

        if (emuStep % 6 == 0)
        {
            APU_Step();
        }

        // ctrl is read
        if (emuStep % 2 == 0)
        {
            CTRL_Step();
        }

        vBlankEntryMoment = false;

        PPU_Step();

        if (hCounter == 0 && vCounter == 225) // Start of VBlank
        {

            vBlankEntryMoment = true;
            RDNMI = 0b10000000;
            if (NMITIMEN & 0b10000000) // NMI is enabled
                invokeNMI(); 

            // for (int y = 0; y < FB_HEIGHT * (WIN_WIDTH / FB_WIDTH); y++)
            // {
            //     for (int x = 0; x < WIN_WIDTH; x++)
            //     {
            //         window_fb[y * WIN_WIDTH + x] = PPU::fb[(y / (WIN_WIDTH / FB_WIDTH)) * FB_WIDTH + (x / (WIN_WIDTH / FB_WIDTH))];
            //     }
            // }

            for (int sy = 0; sy < FB_HEIGHT; sy++)
            {
                int src_row_idx = sy * FB_WIDTH;
                int dest_row_idx = sy * SCALE * WIN_WIDTH;

                // 1. Scale a single row horizontally
                for (int sx = 0; sx < FB_WIDTH; sx++)
                {
                    auto pixel = fb[src_row_idx + sx];
                    int dx = sx * SCALE;

                    // Because SCALE is a compile-time constant, the compiler will
                    // unroll this loop automatically (e.g., if SCALE is 3, it writes 3 times directly)
                    for (int i = 0; i < SCALE; i++)
                    {
                        window_fb[dest_row_idx + dx + i] = pixel;
                    }
                }

                // 2. Duplicate that scaled row vertically
                for (int dy = 1; dy < SCALE; dy++)
                {
                    memcpy(&window_fb[dest_row_idx + (dy * WIN_WIDTH)],
                           &window_fb[dest_row_idx],
                           WIN_WIDTH * sizeof(window_fb[0]));
                }
            }

            for (int y = FB_HEIGHT * (WIN_WIDTH / FB_WIDTH); y < (FB_HEIGHT + 1) * (WIN_WIDTH / FB_WIDTH); y++)
            {
                for (int x = 0; x < WIN_WIDTH; x++)
                {

                    uint16 col = cgram[x / (WIN_WIDTH / FB_WIDTH)];

                    uint8 R = (col & 0b0000000000011111) << 3;
                    uint8 G = (col & 0b0000001111100000) >> 2;
                    uint8 B = (col & 0b0111110000000000) >> 7;

                    window_fb[y * WIN_WIDTH + x] = MFB_ARGB(255, R, G, B);
                }
            }

            mfb_update_ex(window, window_fb, WIN_WIDTH, WIN_HEIGHT);
            mfb_set_title(window, "wee");
            mfb_wait_sync(window);
        }

        if (vCounter == 261 && hCounter == 340) // End of VBlank
        {
            RDNMI = 0b00000000;
        }
        if (vCounter < 225 && hCounter == 256) // Start of HBlank
        {
            hdmaRan = false;
        }
        if (hCounter == 0 && vCounter == 0) // Start of Frame
        {
            DMA_InitializeHDMA();
        }

        // H-V Timer IRQ
        uint8 type = (NMITIMEN & 0b00110000) >> 4;

        switch (type)
        {
        case 0:
            break;
        case 1:
            TIMEUP = 0b10000000;
            if ((HTIMEH << 8 | HTIMEL) == hCounter)
                invokeIRQ();
            break;
        case 2:
            TIMEUP = 0b10000000;
            if ((VTIMEH << 8 | VTIMEL) == vCounter && hCounter == 0)
                invokeIRQ();
            break;
        case 3:
            TIMEUP = 0b10000000;
            if ((VTIMEH << 8 | VTIMEL) == vCounter && hCounter == (HTIMEH << 8 | HTIMEL))
                invokeIRQ();
            break;

        default:
            break;
        }

        emuStep++;
    }
}


void WriteIO(add24 address, uint8 data)
{
    uint16 port = address & 0x0000FFFF;
    //
    //  cin.get();
    if (port >= 0x2100 && port <= 0x213F) // PPU
    {
        PPU_IOWrite(port, data);
        return;
    }
    else if (port >= 0x2140 && port <= 0x2143) // APU
    {
        APU_IOWrite(port, data);
        return;
    }
    else if (port >= 0x2144 && port <= 0x217F) // APU Mirrored
    {
        APU_IOWrite(0x2140 | (port & 0x03), data);
        return;
    }
    else if (port >= 0x4300 && port <= 0x437a) // DMA
    {
        DMA_IOWrite(port, data);
        return;
    }
    else if (port >= 0x4218 && port <= 0x421F) // Controller
    {
        CTRL_IOWrite(port, data);
        return;
    }
    else if (port >= 0x4202 && port <= 0x4206)
    {
        MDU_IOWrite(port, data);
        return;
    }
    else
    {
        switch (port)
        {

        case 0x2180: // WMDATA
        {
            // TODO : check this later
            add24 memAdd = (WMADDH << 16 | WMADDM << 8 | WMADDL);
            ram[memAdd] = data;
            memAdd++;
            WMADDL = memAdd & 0x0000FF;
            WMADDM = (memAdd & 0x00FF00) >> 8;
            WMADDH = (memAdd & 0xFF0000) >> 16;
            break;
        }

        case 0x2181: // WMADDL
            WMADDL = data;
            break;
        case 0x2182: // WMADDM
            WMADDM = data;
            break;
        case 0x2183: // WMADDH
            WMADDH = data;
            WMADDH &= 0x01; // Make sure upper bits are 0 for later
            break;

        case 0x4016: // LCTRLREG1, Manual controller 1
            CTRL_IOWrite(port, data);
            break;

        case 0x4200: // NMITIMEN
            NMITIMEN = data;
            stdCtrlEn = data & 0x01;

            break;
        case 0x4201: // WRIO
            CTRL_IOWrite(port, data);
            break;

        case 0x4207:
            HTIMEL = data;
            break;
        case 0x4208:
            HTIMEH = data;
            break;
        case 0x4209:
            VTIMEL = data;
            break;
        case 0x420a:
            VTIMEH = data;
            break;

        case 0x420b: // MDMAEN

            DMA_IOWrite(port, data);
            break;

        case 0x420c: // HDMAEN

            DMA_IOWrite(port, data);
            break;

        // TODO : implement actual memory speed here
        case 0x420d: // MEMSEL
            MEMSEL = data;
            break;

        // Some games really like to write into read-only regs. so here they are
        default:

            zeroWire = data;
            // pauseEmu = true;
            break;
        }
    }
}

uint8 ReadIO(add24 address)
{

    uint16 port = address & 0x0000FFFF;
    //

    if (port >= 0x2100 && port <= 0x213F) // PPU
    {
        if (port == 0x2137)
            setOverflow();
        return PPU_IORead(port);
    }
    else if (port >= 0x2140 && port <= 0x2143) // APU
    {
        return APU_IORead(port);
    }
    else if (port >= 0x2144 && port <= 0x217F) // APU Mirrored
    {
        return APU_IORead(0x2140 | (port & 0x03));
    }
    else if (port >= 0x4300 && port <= 0x437a) // DMA
    {
        return DMA_IORead(port);
    }
    else if (port >= 0x4218 && port <= 0x421F) // Controller
    {
        return CTRL_IORead(port);
    }
    else if (port >= 0x4214 && port <= 0x4217)
    {
        return MDU_IORead(port);
    }
    else
    {
        switch (port)
        {
        case 0x2180: // WMDATA
        {
            add24 memAdd = (WMADDH << 16 | WMADDM << 8 | WMADDL);
            uint8 d = ram[memAdd];
            memAdd++;
            WMADDL = memAdd & 0x0000FF;
            WMADDM = (memAdd & 0x00FF00) >> 8;
            WMADDH = (memAdd & 0xFF0000) >> 16;
            return d;
            break;
        }

        case 0x4016: // LCTRLREG1, Manual controller 1
            return CTRL_IORead(port);
            break;
        case 0x4017: // LCTRLREG2, Manual controller 1
            return CTRL_IORead(port);
            break;

        case 0x4212: // HVBJOY
            return ((vBlank) << 7) | ((hBlank) << 6) | (0);
            break;

        case 0x4213: // RDIO
            return CTRL_IORead(port);
            break;

        case 0x4210: // RDNMI
        {
            uint8 t = RDNMI;
            RDNMI = RDNMI & 0x7F;
            return t | 0b01110010; // IDK why but look like every other emulator sets the 3 XXX bits in this reg to 1
            break;
        }

        case 0x4211:
        {
            uint8 t = TIMEUP;
            TIMEUP = TIMEUP & 0x7F;
            return t;
            break;
        }

        default:

            // pauseEmu = true;
            return zeroWire;
            break;
        }
    }
    return zeroWire;
}



void WriteBus(add24 address, uint8 data)
{

    //

    if (romMapType == LoROM)
    {
        // Extract bank and offset for easier math
        uint8_t bank = (address >> 16) & 0xFF;
        uint16_t offset = address & 0xFFFF;

        // 1. WRAM (Banks $7E and $7F)
        // This must be checked BEFORE we mirror the upper banks down.
        if (bank == 0x7E || bank == 0x7F)
        {

      
                *(ram + (address & 0x01FFFF)) = data;
            
        }

        // Mirror FastROM banks ($80-$FF) down to SlowROM banks ($00-$7F)
        bank = bank & 0x7F;

 

        // 3. Low RAM and I/O (Banks $00-$3F)
        if (bank <= 0x3F)
        {
            if (offset < 0x2000) // First 8KB mirrors WRAM
            {

           
                    *(ram + offset) = data;
            }
            if (offset >= 0x2100 && offset < 0x6000) // Hardware I/O
            {
              
                    WriteIO(address, data);
            }
        }

        // 4. SRAM (Typically Banks $70-$7D, Offsets $0000-$7FFF)
        if (bank >= 0x70 && bank <= 0x7D && offset < 0x8000)
        {
            if (!hasSram)
            {
                zeroWire = data;
                return;
            }
            // Calculate contiguous SRAM offset
            uint32_t sram_offset = ((bank - 0x70) * 0x8000) + offset;
            sram_offset &= (sramSize - 1);

  
                *(sram + sram_offset) = data;
        }
        return ;
    }
    else if (romMapType == HiROM || romMapType == ExHiROM)
    {
        uint8_t bank = (address >> 16) & 0xFF;
        uint16_t offset = address & 0xFFFF;

        // 1. WRAM (Banks $7E and $7F)
        // Same as LoROM, must be checked first
        if (bank == 0x7E || bank == 0x7F)
        {
          
                *(ram + (address & 0x01FFFF)) = data;
                return; 
            
        }

        // 2. Low RAM, I/O, and SRAM (Banks $00-$3F and $80-$BF)
        // These banks share the same layout in the bottom 32KB
        if ((bank >= 0x00 && bank <= 0x3F) || (bank >= 0x80 && bank <= 0xBF))
        {
            if (offset < 0x2000) // First 8KB mirrors WRAM
            {
             
                    *(ram + offset) = data;
            }
            else if (offset >= 0x2100 && offset < 0x6000) // Hardware I/O
            {
               
                    WriteIO(address, data);
            }
            else if (offset >= 0x6000 && offset < 0x8000) // SRAM
            {
                if (!hasSram)
                {
                    zeroWire = data;
                    return;
                }
                // SRAM maps to 8KB chunks. Strip to 5 bits for continuous SRAM mapping.
                uint32_t sram_offset = ((bank & 0x1F) * 0x2000) + (offset - 0x6000);
                sram_offset &= (sramSize - 1);

                
                    *(sram + sram_offset) = data;
            }
        }


        return ;
    }

    return ;
}

uint8 ReadBus(add24 address)
{

    //

    if (romMapType == LoROM)
    {
        // Extract bank and offset for easier math
        uint8_t bank = (address >> 16) & 0xFF;
        uint16_t offset = address & 0xFFFF;

        // 1. WRAM (Banks $7E and $7F)
        // This must be checked BEFORE we mirror the upper banks down.
        if (bank == 0x7E || bank == 0x7F)
        {

            // Mask to 17 bits (128KB of WRAM)

            return *(ram + (address & 0x01FFFF));
        }

        // Mirror FastROM banks ($80-$FF) down to SlowROM banks ($00-$7F)
        bank = bank & 0x7F;

        // 2. ROM (Offsets $8000-$FFFF in banks $00-$7D)
        if (offset >= 0x8000)
        {
            // Calculate physical contiguous ROM offset without holes
            uint32_t rom_offset = (bank * 0x8000) + (offset - 0x8000);

            // Good practice: ensure we don't read out of bounds
            if (rom_offset < romSize)
            {

                return *(rom + rom_offset);
            }
        }

        // 3. Low RAM and I/O (Banks $00-$3F)
        if (bank <= 0x3F)
        {
            if (offset < 0x2000) // First 8KB mirrors WRAM
            {

                return *(ram + offset);
            }
            if (offset >= 0x2100 && offset < 0x6000) // Hardware I/O
            {

                return ReadIO(address);
            }
        }

        // 4. SRAM (Typically Banks $70-$7D, Offsets $0000-$7FFF)
        if (bank >= 0x70 && bank <= 0x7D && offset < 0x8000)
        {
            if (!hasSram)
            {
                return zeroWire;
            }
            // Calculate contiguous SRAM offset
            uint32_t sram_offset = ((bank - 0x70) * 0x8000) + offset;
            sram_offset &= (sramSize - 1);

            return *(sram + sram_offset);
        }
        return zeroWire;
    }
    else if (romMapType == HiROM || romMapType == ExHiROM)
    {
        uint8_t bank = (address >> 16) & 0xFF;
        uint16_t offset = address & 0xFFFF;

        // 1. WRAM (Banks $7E and $7F)
        // Same as LoROM, must be checked first
        if (bank == 0x7E || bank == 0x7F)
        {

            return *(ram + (address & 0x01FFFF));
        }

        // 2. Low RAM, I/O, and SRAM (Banks $00-$3F and $80-$BF)
        // These banks share the same layout in the bottom 32KB
        if ((bank >= 0x00 && bank <= 0x3F) || (bank >= 0x80 && bank <= 0xBF))
        {
            if (offset < 0x2000) // First 8KB mirrors WRAM
            {
                return *(ram + offset);
            }
            else if (offset >= 0x2100 && offset < 0x6000) // Hardware I/O
            {

                return ReadIO(address);
            }
            else if (offset >= 0x6000 && offset < 0x8000) // SRAM
            {
                if (!hasSram)
                {
                    return zeroWire;
                }
                // SRAM maps to 8KB chunks. Strip to 5 bits for continuous SRAM mapping.
                uint32_t sram_offset = ((bank & 0x1F) * 0x2000) + (offset - 0x6000);
                sram_offset &= (sramSize - 1);

                return *(sram + sram_offset);
            }
        }

        // 3. ROM
        // Remove the FastROM bit (e.g., $80-$FF becomes $00-$7F) to simplify math
        uint8_t rom_bank = bank & 0x7F;

        if (rom_bank >= 0x40 && rom_bank <= 0x7F)
        {
            // Full 64KB banks mapped here
            uint32_t rom_offset = ((rom_bank - 0x40) * 0x10000) + offset;

            // ExHiROM moves the upper 4MB to banks $40-$7D
            if (romMapType == ExHiROM)
            {
                rom_offset += 0x400000; // Offset by 4MB
            }

            if (rom_offset < romSize)
            {

                return *(rom + rom_offset);
            }
        }
        else if (offset >= 0x8000)
        {
            // For banks $00-$3F (and their FastROM mirrors $80-$BF),
            // offsets $8000-$FFFF mirror the ROM data from banks $C0-$FF.
            uint8_t mirror_bank = bank & 0x3F;
            uint32_t rom_offset = (mirror_bank * 0x10000) + offset;

            if (rom_offset < romSize)
            {

                return *(rom + rom_offset);
            }
        }

        return zeroWire;
    }

    return 0;
}


void keyboard_callback(struct mfb_window *window, mfb_key key, mfb_key_mod mod, bool is_pressed)
{
    if (is_pressed)
    {
        if (key == KB_KEY_ESCAPE)
        {
            exitEmu();
        }
    }
    // Controller
    if (key == KB_KEY_Z)
    {
        if (is_pressed)
            CTRL_KeyPress(15); // B
        else
            CTRL_KeyRelease(15);
    }
    if (key == KB_KEY_X)
    {
        if (is_pressed)
            CTRL_KeyPress(7); // A
        else
            CTRL_KeyRelease(7);
    }
    if (key == KB_KEY_A)
    {
        if (is_pressed)
            CTRL_KeyPress(14); // Y
        else
            CTRL_KeyRelease(14);
    }
    if (key == KB_KEY_S)
    {
        if (is_pressed)
            CTRL_KeyPress(6); // X
        else
            CTRL_KeyRelease(6);
    }
    if (key == KB_KEY_SPACE)
    {
        if (is_pressed)
            CTRL_KeyPress(12); // Start
        else
            CTRL_KeyRelease(12);
    }
    if (key == KB_KEY_ENTER)
    {
        if (is_pressed)
            CTRL_KeyPress(13); // Start
        else
            CTRL_KeyRelease(13);
    }
    if (key == KB_KEY_LEFT)
    {
        if (is_pressed)
            CTRL_KeyPress(9); // Left
        else
            CTRL_KeyRelease(9);
    }
    if (key == KB_KEY_RIGHT)
    {
        if (is_pressed)
            CTRL_KeyPress(8); // Right
        else
            CTRL_KeyRelease(8);
    }
    if (key == KB_KEY_UP)
    {
        if (is_pressed)
            CTRL_KeyPress(11); // UP
        else
            CTRL_KeyRelease(11);
    }
    if (key == KB_KEY_DOWN)
    {
        if (is_pressed)
            CTRL_KeyPress(10); // Down
        else
            CTRL_KeyRelease(10);
    }
}


int main(int argc, char *argv[])
{

    if (argc < 2)
    {
        printf("Error: No file path provided.\n");
        printf("Usage: %s <path_to_sfc_file>\n", argv[0]);
        return 1;
    }

    LoadRom(argv[1]);

    // if (Cartridge::hasSram)
    // {
    //     bool sramExists = true;
    //     if (!sramFile.is_open())
    //     {
    //         sramExists = false;
    //     }
    //     if (sramExists)
    //     {
    //         sramFile.read(reinterpret_cast<char *>(Cartridge::sram), Cartridge::sramSize);
    //     }
    //     sramFile.close();
    // }

    window = mfb_open_ex("MTOSFC", WIN_WIDTH, WIN_HEIGHT, 0);
    mfb_set_target_fps(240);
    mfb_set_keyboard_callback(window, keyboard_callback);

    zeroWire = 0;

    emu();

    return 0;
}