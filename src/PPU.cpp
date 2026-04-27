#include <bits/stdc++.h>
#include <MiniFB.h>
#include <assert.h>
using namespace std;
typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int add24;

struct PPURegs
{
    uint8 INIDISP; // 0x2100 INITIAL SETTINGS FOR SCREEN

    uint8 OBJSEL; // 0x2101 OBJ SIZE AND DATA AREA DESIGNATION

    uint8 OAMADDL; // 0x2102
    uint8 OAMADDH; // 0x2103 OAM ADDRESS
    uint8 OAMDATA; // 0x2104 : Write, 0x2138 : Read

    uint8 BGMODE; // 0x2105

    uint8 MOSAIC; // 0x2106

    uint8 BG1SC; // 0x2107
    uint8 BG2SC; // 0x2108
    uint8 BG3SC; // 0x2109
    uint8 BG4SC; // 0x210A

    uint8 BG12NBA; // 0x210B
    uint8 BG34NBA; // 0x210C

    uint8 BG1H0FS; // 0x210D
    uint8 BG1V0FS; // 0x210E

    uint8 BG2H0FS; // 0X210F
    uint8 BG2V0FS; // 0X2110
    uint8 BG3H0FS; // 0X2111
    uint8 BG3V0FS; // 0X2112
    uint8 BG4H0FS; // 0X2113
    uint8 BG4V0FS; // 0X2114

    uint8 VMAINC; // 0X2115

    uint8 VMADDL; // 0x2116
    uint8 VMADDH; // 0x2117

    uint8 VMDATAL; // 0x2118
    uint8 VMDATAH; // 0x2119

    // Mode 7
    uint8 M7SEL; // 0x211A
    uint8 M7A;   // 0x211B Rotation
    uint8 M7B;   // 0x211C Enlargement
    uint8 M7C;   // 0x211D Reduction, Center, Multiplicand
    uint8 M7D;   // 0x211E Multiplier settings
    uint8 M7X;   // 0x211F Operand
    uint8 M7Y;   // 0x2120 Operand

    uint8 CGADD;  // 0x2121
    uint8 CGDATA; // 0x2122

    uint8 W12SEL;  // 0x2123
    uint8 W34SEL;  // 0x2124
    uint8 WOBJSEL; // 0x2125

    uint8 WH0; // 0x2126
    uint8 WH1; // 0x2127
    uint8 WH2; // 0x2128
    uint8 WH3; // 0x2129

    uint8 WBGLOG;  // 0x212A
    uint8 WOBJLOG; // 0x212B

    uint8 TM; // 0x212C

    uint8 TS; // 0x212D

    uint8 TMW; // 0x212E

    uint8 TSW; // 0x212F

    uint8 CGSWSEL; // 0x2130

    uint8 CGADSUB; // 0x2131

    uint8 COLDATA; // 0x2132

    uint8 SETINI; // 0x2133

    uint8 MPYL; // 0x2134
    uint8 MPYM; // 0x2135
    uint8 MPYH; // 0x2136

    uint8 SLHV; // 0x2137

    // The following might get merged with the write regs
    uint8 OAMDATA_READ; // 0x2138
    uint8 VMDATAL_READ; // 0x2139
    uint8 VMDATAH_READ; // 0x213A
    uint8 CGDATA_READ;  // 0x213B

    uint8 OPHCT; // 0x213C
    uint8 OPVCT; // 0x213D

    uint8 STAT77; // 0x213E
    uint8 STAT78; // 0x213F
};
const int FB_WIDTH = 256;
const int FB_HEIGHT = 224;

struct Object
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
};

class PPU
{

public:
    uint32_t fb[FB_WIDTH * FB_HEIGHT];

    PPURegs regs;

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
    Object objs[128];

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

    uint16 VRAMRead(uint16 add)
    {
        return vram[TranslateVRAMAddress(add & 0x7FFF)];
    }

    void doMult()
    {
        int32_t mult = (int16_t)mode7A * (int8_t)((mode7B >> 8));
        regs.MPYL = mult & 0x000000FF;
        regs.MPYM = (mult & 0x0000FF00) >> 8;
        regs.MPYH = (mult & 0x00FF0000) >> 16;
    }

    void IncVMADD()
    {
        uint8 inc = regs.VMAINC & 0b00000011;
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
    uint16 TranslateVRAMAddress(uint16 addr)
    {
        uint8 translation = (regs.VMAINC >> 2) & 0b00000011;
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

    // TOOD : later all these registers can be replaced by just their funtionality
    uint8 IORead(add24 address)
    {
        switch (address)
        {

        case 0x2134: // MPYL
            return this->regs.MPYL;
            break;
        case 0x2135: // MPYM
            return this->regs.MPYM;
            break;
        case 0x2136: // MPYH
            return this->regs.MPYH;
            break;

        case 0x2137: // SLHV

            hCounterLatch = hCounter;
            vCounterLatch = vCounter;
            return 0;
            break;
        case 0x2138: // OAMDATA_READ
            return this->regs.OAMDATA_READ;
            break;

        case 0x2118: // for some reason zelda uses this
        case 0x2139: // VMDATAL_READ
        {
            uint16 add = (vramAddr + TranslateVRAMAddress(vramPtr)) & 0x7FFF;
            uint8 t = vram[add] & 0x00ff;
            if (!(regs.VMAINC & 0b10000000)) // Inc on low
                IncVMADD();
            return t;
            break;
        }

        case 0x2119: // for some reason zelda uses this
        case 0x213A: // VMDATAH_READ
        {
            uint16 add = (vramAddr + TranslateVRAMAddress(vramPtr)) & 0x7FFF;
            uint8 t = (vram[add] & 0xff00) >> 8;
            if (regs.VMAINC & 0b10000000) // Inc on high
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
                regs.OPHCT = hCounterLatch & 0x00FF;
            }
            else
            {
                hCounterLatchHL = false;
                regs.OPHCT = (hCounterLatch & 0xFF00) >> 8;
            }
            return this->regs.OPHCT;
            break;
        }
        case 0x213D: // OPVCT
        {
            if (!vCounterLatchHL) // Low
            {
                vCounterLatchHL = true;
                regs.OPVCT = vCounterLatch & 0x00FF;
            }
            else
            {
                vCounterLatchHL = false;
                regs.OPVCT = (vCounterLatch & 0xFF00) >> 8;
            }
            return this->regs.OPVCT;
            break;
        }
        case 0x213E: // STAT77
            return this->regs.STAT77;
            break;
        case 0x213F:                 // STAT78
            hCounterLatchHL = false; // Yes, here, based on manual page 2-27-23
            vCounterLatchHL = false;
            return this->regs.STAT78;
            break;
        default:
            return 0;
            break;
        }
    }
    void IOWrite(add24 address, uint8 data)
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
            this->regs.MOSAIC = data;
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
            this->regs.VMAINC = data;
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
            if (!(regs.VMAINC & 0b10000000)) // Inc on low
                IncVMADD();

            break;
        }
        case 0x2119: // VMDATAH
        {
            uint16 add = (vramAddr + TranslateVRAMAddress(vramPtr)) & 0x7FFF;
            vram[add] = (vram[add] & 0x00ff) | (data << 8);
            if (regs.VMAINC & 0b10000000) // Inc on high
                IncVMADD();

            break;
        }

        case 0x211A: // M7SEL
            this->regs.M7SEL = data;
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
            this->regs.W12SEL = data;
            break;
        case 0x2124: // W34SEL
            this->regs.W34SEL = data;
            break;
        case 0x2125: // WOBJSEL
            this->regs.WOBJSEL = data;
            break;
        case 0x2126: // WH0
            this->regs.WH0 = data;
            break;
        case 0x2127: // WH1
            this->regs.WH1 = data;
            break;
        case 0x2128: // WH2
            this->regs.WH2 = data;
            break;
        case 0x2129: // WH3
            this->regs.WH3 = data;
            break;
        case 0x212A: // WBGLOG
            this->regs.WBGLOG = data;
            break;
        case 0x212B: // WOBJLOG
            this->regs.WOBJLOG = data;
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
            this->regs.TMW = data;
            break;
        case 0x212f: // TSW
            this->regs.TSW = data;
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

    PPU()
    {

        memset(fb, 0, FB_HEIGHT * FB_HEIGHT * sizeof(uint32_t));
    }

    void reset()
    {
        regs.INIDISP = 0x8F; // BLANK
        regs.VMAINC = 0x80;
        regs.CGSWSEL = 0x30;
        regs.COLDATA = 0xE0;
        hCounter = 0;
        vCounter = 0;
        hBlank = false;
        vBlank = false;
        frameCount = 0;
    }

    void step()
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

            // Apply backdrop by default
            for (int i = 0; i < 256; i++)
                mainLine[i] = subLine[i] = cgram[0];

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
                            mainLine[hCnt] = col;
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
                        mainLine[hCnt] = col;
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
                        mainLine[hCnt] = directColor ? colorIdx : cgram[colorIdx];
                    if (BG1onSubScreen)
                        subLine[hCnt] = directColor ? colorIdx : cgram[colorIdx];
                    // prior[hCnt] = bg1prior;
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
                        mainLine[hCnt] = cgram[colAdd];
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
                        mainLine[hCnt] = cgram[colAdd];
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
                        mainLine[hCnt] = cgram[colAdd];
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
                uint8 R = (selectedCol & 0b0000000000011111) >> 0;
                uint8 G = (selectedCol & 0b0000001111100000) >> 5;
                uint8 B = (selectedCol & 0b0111110000000000) >> 10;

                mainR = ((mainLine[hCnt] & 0b0000000000011111) >> 0);
                mainG = ((mainLine[hCnt] & 0b0000001111100000) >> 5);
                mainB = ((mainLine[hCnt] & 0b0111110000000000) >> 10);

                // mainR = mainR + (addSub ? -R : R);
                // mainR = mainR > 31 ? 31 : mainR;
                // mainR = mainR < 0 ? 0 : mainR;
                // mainR = mainR >> (addSubHalf ? 1 : 0);

                // mainG = mainG + (addSub ? -G : G);
                // mainG = mainG > 31 ? 31 : mainG;
                // mainG = mainG < 0 ? 0 : mainG;
                // mainG = mainG >> (addSubHalf ? 1 : 0);

                // mainB = mainB + (addSub ? -B : B);
                // mainB = mainB > 31 ? 31 : mainB;
                // mainB = mainB < 0 ? 0 : mainB;
                // mainB = mainB >> (addSubHalf ? 1 : 0);

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
};