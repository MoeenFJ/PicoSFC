#include "PPU.h"

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
