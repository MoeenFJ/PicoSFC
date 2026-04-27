
#include "SPC700.h"

typedef unsigned char uint8;
typedef unsigned char bool;
typedef unsigned short uint16;
typedef unsigned int add24;

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

uint8 SPCRead(uint16 address);
void SPCWrite(uint16 address, uint8 data);

void APU_Init();
void APU_IOWrite(uint16 port, uint8 data);
uint8 APU_IORead(uint16 port);
void APU_Step();