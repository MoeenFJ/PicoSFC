#include <bits/stdc++.h>
#include "SPC700.cpp"
using namespace std;

typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int add24;

namespace APU
{
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

    SPC700 *spc;

    uint8 APUIO0;
    uint8 APUIO1;
    uint8 APUIO2;
    uint8 APUIO3;

    uint8 CPUIO0;
    uint8 CPUIO1;
    uint8 CPUIO2;
    uint8 CPUIO3;

    // Timers
    bool t0En = false;
    bool t1En = false;
    bool t2En = false;

    uint8 t0Val = 0;
    uint8 t1Val = 0;
    uint8 t2Val = 0;

    uint16 t0div = 1;
    uint16 t1div = 1;
    uint16 t2div = 1;

    bool ramRomSel = 1;

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
                uint t = t0Val & 0x0F;
                t0Val = 0;
                return t;
                break;
            }
            case 0xe: // T1OUT
            {
                uint t = t1Val & 0x0F;
                t1Val = 0;
                return t;
                break;
            }
            case 0xf: // T2OUT
            {
                uint t = t2Val & 0x0F;
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

    void Init()
    {
        ramRomSel = 1;
        spc = new SPC700(SPCRead, SPCWrite);
        spc->Reset();
    }

    void IOWrite(uint16 port, uint8 data)
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

    uint8 IORead(uint16 port)
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

    // Timing:
    // APU clock LCM = 24*128 = 3072
    // So the APU step counter must reset at a multiple of 3072
    // Biggest Multiple of 3072 that fits in 32 bits, is 1398101 * 3072
    uint32_t stepCnt = 0;
    uint16 t0Clk = 0;
    uint16 t1Clk = 0;
    uint16 t2Clk = 0;
    void Step()
    {

        if (spc->cycles < stepCnt)
        {
            spc->Step();
            // spc->PrintState();
        }

        // if (spc->cycles > 60000 && stepCnt > 60000)
        //{
        //     stepCnt -= 60000;
        //     spc->cycles -= 60000;
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
};
