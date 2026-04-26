#include <bits/stdc++.h>
#include <MiniFB.h>
#include <chrono>

using namespace std;

typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int add24;

#include "Cartridge.cpp"
#include "PPU.cpp"
#include "APU.cpp"
#include "CPU.cpp"
#include "DMA.cpp"
#include "CTRL.cpp"
#include "MDUnit.cpp"

Cartridge *rom;
CPU *cpu;
PPU *ppu;
DMA *dma;
MDUnit *mdu;
ControllerSystem *ctrlsys;

uint8 zeroWire = 0;
uint8 ram[128 * 1024] = {0};

uint8 NMITIMEN; // 0x4200
uint8 HTIMEL;   // 0x4207
uint8 HTIMEH;   // 0x4208
uint8 VTIMEL;   // 0x4209
uint8 VTIMEH;   // 0x420a
uint8 TIMEUP;   // 0x4211
uint8 MEMSEL;   // 0x420D

uint8 WMDATA; // 0x2180

uint8 WMADDL; // 0x2181
uint8 WMADDM; // 0x2182
uint8 WMADDH; // 0x2183

uint8 RDNMI;

uint8 C65Read(add24 address);
void C65Write(add24 address, uint8 data);

uint8 DMARead(add24 address);
void DMAWrite(add24 address, uint8 data);

uint8 BusAccess(add24 address, uint8 data, bool rd);

unsigned int vblanktimer = 1;

bool cpuVBlankIntLatch = false;
bool cpuHBlankIntLatch = false;
bool vBlankEntryMoment = false;
bool hdmaRan = true;

// GUI
const int SCALE = 3;
const int WIN_WIDTH = 256 * SCALE;
const int WIN_HEIGHT = 256 * SCALE;
struct mfb_window *window;
uint32_t window_fb[WIN_WIDTH * WIN_HEIGHT];

// Debugging tools
bool debug = false;
bool pauseEmu = true;
bool runStep = false;
bool runVBlank = false;
bool runInst = false;
bool runHBlank = false;
bool cpuTrace = false;
uint16 mouseX, mouseY;
ofstream cpuTraceFile;

void DumpVRam()
{
    ofstream outFile("VRAMDump.bin", std::ios::binary);

    outFile.write(reinterpret_cast<const char *>(ppu->vram), (size_t)32 * 1024 * sizeof(uint16));

    outFile.close();
}
void DumpOARam()
{
    ofstream outFile("OAMDump.bin", std::ios::binary);

    outFile.write(reinterpret_cast<const char *>(ppu->oam), (size_t)272 * sizeof(uint16));

    outFile.close();
}
void DumpRam()
{
    ofstream outFile("RAMDump.bin", std::ios::binary);

    outFile.write(reinterpret_cast<const char *>(ram), (size_t)128 * 1024 * sizeof(uint8));

    outFile.close();
}

uint64_t cpuTime = 0;
uint64_t dmaTime = 0;
uint64_t ppuTime = 0;
uint64_t apuTime = 0;
uint64_t ctrlTime = 0;
uint64_t otherTime = 0;
uint64_t drawTime = 0;
chrono::steady_clock::time_point frameTime;

uint64_t emuStep = 0;
void emu()
{
    cpu->reset();
    ppu->reset();
    APU::Init();
    VTIMEL = 0xff;
    VTIMEH = 0x01;
    HTIMEL = 0xff;
    HTIMEH = 0x01;
    RDNMI = RDNMI & 0x7F;
    pauseEmu = true;
    frameTime = chrono::steady_clock::now();
    while (true) // Emulation Loop
    {

        
        if ((pauseEmu || ppu->pauseEmu) && !runStep && !runVBlank && !runInst && !runHBlank)
        {
            mfb_update_events(window);
            continue;
        }

        if (!dma->dmaActive)
        {

            if (emuStep % 12 == 0)
            {
                runInst = false;
                if (debug)
                {
                    cpu->printStatus();
                }
                if (cpuTrace)
                {
                    cpuTraceFile << cpu->stringStatus() << endl;
                }

                auto start = chrono::steady_clock::now();
                cpu->cpuStep();
                auto end = chrono::steady_clock::now();
                cpuTime += chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
            }
        }

        if (emuStep % 2 == 0)
        {
            auto start = chrono::steady_clock::now();
            dma->step(!hdmaRan);
            auto end = chrono::steady_clock::now();
            dmaTime += chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
            if (!hdmaRan)
            {
                hdmaRan = true;
            }
        }

        if (emuStep % 6 == 0)
        {
            auto start = chrono::steady_clock::now();
            APU::Step();
            auto end = chrono::steady_clock::now();
            apuTime += chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        }

        // ctrl is read
        if (emuStep % 2 == 0)
        {
            auto start = chrono::steady_clock::now();
            ctrlsys->step();
            auto end = chrono::steady_clock::now();
            ctrlTime += chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        }

        vBlankEntryMoment = false;
        if (emuStep)
        {

            auto start = chrono::steady_clock::now();
            ppu->step(); // every 341*262 = 89342 steps => 1 frame
            auto end = chrono::steady_clock::now();
            ppuTime += chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

            if (ppu->hCounter == 0 && ppu->vCounter == 225) // Start of VBlank
            {
                // dma->HDMAEN = 0;
                vBlankEntryMoment = true;
                runVBlank = false;
                RDNMI = 0b10000000;
                if (NMITIMEN & 0b10000000) // NMI is enabled
                    cpu->invokeNMI();

                // for (int y = 0; y < FB_HEIGHT * (WIN_WIDTH / FB_WIDTH); y++)
                // {
                //     for (int x = 0; x < WIN_WIDTH; x++)
                //     {
                //         window_fb[y * WIN_WIDTH + x] = ppu->fb[(y / (WIN_WIDTH / FB_WIDTH)) * FB_WIDTH + (x / (WIN_WIDTH / FB_WIDTH))];
                //     }
                // }
                
                start = chrono::steady_clock::now();
                for (int sy = 0; sy < FB_HEIGHT; sy++)
                {
                    int src_row_idx = sy * FB_WIDTH;
                    int dest_row_idx = sy * SCALE * WIN_WIDTH;

                    // 1. Scale a single row horizontally
                    for (int sx = 0; sx < FB_WIDTH; sx++)
                    {
                        auto pixel = ppu->fb[src_row_idx + sx];
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

                        uint16 col = ppu->cgram[x / (WIN_WIDTH / FB_WIDTH)];

                        uint8 R = (col & 0b0000000000011111) << 3;
                        uint8 G = (col & 0b0000001111100000) >> 2;
                        uint8 B = (col & 0b0111110000000000) >> 7;

                        window_fb[y * WIN_WIDTH + x] = MFB_ARGB(255, R, G, B);
                    }
                }

                mfb_update_ex(window, window_fb, WIN_WIDTH, WIN_HEIGHT);
                mfb_set_title(window, to_string(ppu->frameCount).c_str());
                mfb_wait_sync(window);
                auto end = chrono::steady_clock::now();
                drawTime +=  chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
                

                 start = chrono::steady_clock::now();
                cout << "==================TIMING================" << endl;
                cout << "Frame Time : " << dec << chrono::duration_cast<std::chrono::milliseconds>(chrono::steady_clock::now() - frameTime).count() << endl;
                frameTime = chrono::steady_clock::now();
                cout << "CPU Time : " << dec << cpuTime / 1000000 << endl;
                cout << "PPU Time : " << dec << ppuTime / 1000000 << endl;
                cout << "APU Time : " << dec << apuTime / 1000000 << endl;
                cout << "Ctrl Time : " << dec << ctrlTime / 1000000 << endl;
                cout << "DMA Time : " << dec << dmaTime / 1000000 << endl;
                cout << "Other Time : " << dec << otherTime / 1000000 << endl;
                cout << "Draw Time : " << dec << drawTime / 1000000 << endl;
                end = chrono::steady_clock::now();
                cout << "Debug : " << dec << chrono::duration_cast<std::chrono::nanoseconds>(end - start).count()/1000000 << endl;
                cpuTime = 0;
                ppuTime = 0;
                apuTime = 0;
                ctrlTime = 0;
                dmaTime = 0;
                otherTime = 0;
                drawTime = 0;
            }

             start = chrono::steady_clock::now();

            if (ppu->vCounter == 261 && ppu->hCounter == 340) // End of VBlank
            {
                RDNMI = 0b00000000;
            }
            if (ppu->vCounter < 225 && ppu->hCounter == 256) // Start of HBlank
            {
                hdmaRan = false;
                runHBlank = false;
            }
            if (ppu->hCounter == 0 && ppu->vCounter == 0) // Start of Frame
            {
                dma->initializeHDMA();
            }

            // H-V Timer IRQ
            uint8 type = (NMITIMEN & 0b00110000) >> 4;

            switch (type)
            {
            case 0:
                break;
            case 1:
                TIMEUP = 0b10000000;
                if ((HTIMEH << 8 | HTIMEL) == ppu->hCounter)
                    cpu->invokeIRQ();
                break;
            case 2:
                TIMEUP = 0b10000000;
                if ((VTIMEH << 8 | VTIMEL) == ppu->vCounter && ppu->hCounter == 0)
                    cpu->invokeIRQ();
                break;
            case 3:
                TIMEUP = 0b10000000;
                if ((VTIMEH << 8 | VTIMEL) == ppu->vCounter && ppu->hCounter == (HTIMEH << 8 | HTIMEL))
                    cpu->invokeIRQ();
                break;

            default:
                break;
            }
             end = chrono::steady_clock::now();
            otherTime += chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        }

        emuStep++;
        runStep = false;
    }
}

void exitEmu()
{
    cpuTraceFile.close();

    stringstream ss;
    ss << rom->title << ".srm";

    ofstream sramFile(ss.str(), std::ios::binary);
    sramFile.write(reinterpret_cast<const char *>(rom->sram), rom->sramSize * sizeof(uint8));
    sramFile.close();

    mfb_close(window);
    exit(0);
}

void keyboard_callback(struct mfb_window *window, mfb_key key, mfb_key_mod mod, bool is_pressed)
{
    if (is_pressed)
    {
        if (key == KB_KEY_ESCAPE)
        {
            exitEmu();
        }
        if (key == KB_KEY_P)
        {
            pauseEmu = !pauseEmu;
            ppu->pauseEmu = pauseEmu;
        }
        if (key == KB_KEY_1)
        {
            debug = !debug;
            cout << "Debug : " << (debug ? "true" : "false") << endl;
        }
        if (key == KB_KEY_9)
        {
            runStep = true;
        }
        if (key == KB_KEY_0)
        {
            cout << "Mode : " << hex << (uint16)ppu->mode << endl;
            cout << "INIDISP : " << hex << (uint16)ppu->regs.INIDISP << endl;
            cout << "BG1Base : " << hex << (uint16)ppu->BG1BaseAddr << endl;
            cout << "BG1ChrBase : " << hex << (uint16)ppu->BG1ChrsBaseAddr << endl;
            cout << "BG2Base : " << hex << (uint16)ppu->BG2BaseAddr << endl;
            cout << "BG2ChrBase : " << hex << (uint16)ppu->BG2ChrsBaseAddr << endl;
            runVBlank = true;
        }
        if (key == KB_KEY_O)
        {
            cout << "Mode : " << hex << (uint16)ppu->mode << endl;
            cout << "DirCol : " << hex << (uint16)ppu->directColor << endl;
            cout << "force blnk : " << hex << (uint16)ppu->forceBlank << endl;
            cout << "fade : " << hex << (uint16)ppu->fadeValue << endl;
            cout << "h : " << hex << (uint16)ppu->hCounter << endl;
            cout << "v : " << hex << (uint16)ppu->vCounter << endl;
            cout << "hBlank : " << hex << (uint16)ppu->hBlank << endl;
            cout << "vBlank : " << hex << (uint16)ppu->vBlank << endl;
            cout << "VMAINC : " << hex << (uint16)ppu->regs.VMAINC << endl;

            cout << "OBJChrTable1BaseAddr : " << hex << (uint16)ppu->OBJChrTable1BaseAddr << endl;
            cout << "OBJChrTable2BaseAddr : " << hex << (uint16)ppu->OBJChrTable2BaseAddr << endl;

            cout << "Add Sub : " << hex << (uint16)ppu->addSub << endl;

            cout << "BG1 MS EN : " << hex << ppu->BG1onMainScreen << endl;
            cout << "BG1Base : " << hex << (uint16)ppu->BG1BaseAddr << endl;
            cout << "BG1ChrBase : " << hex << (uint16)ppu->BG1ChrsBaseAddr << endl;

            cout << "BG2 MS EN : " << hex << ppu->BG2onMainScreen << endl;
            cout << "BG2Base : " << hex << (uint16)ppu->BG2BaseAddr << endl;
            cout << "BG2ChrBase : " << hex << (uint16)ppu->BG2ChrsBaseAddr << endl;
            cout << "BG2Scroll : (" << hex << ppu->BG2HScroll << "," << ppu->BG2VScroll << ")" << endl;
        }
        if (key == KB_KEY_MINUS)
        {
            runInst = true;
        }
        if (key == KB_KEY_EQUAL)
        {
            runHBlank = true;
        }
        if (key == KB_KEY_2)
        {
            cout << "Ram Dumped" << endl;
            DumpRam();
        }
        if (key == KB_KEY_4)
        {
            cout << "VRam Dumped" << endl;
            DumpVRam();
        }
        if (key == KB_KEY_3)
        {
            cpuTrace = !cpuTrace;
            cout << "Trace : " << (cpuTrace ? "true" : "false") << endl;
        }
        if (key == KB_KEY_5)
        {
            cout << "OARam Dumped" << endl;
            DumpOARam();
        }
        if (key == KB_KEY_F1)
        {
            ppu->bgFilter = 1;
        }
        if (key == KB_KEY_F2)
        {
            ppu->bgFilter = 2;
        }
        if (key == KB_KEY_F3)
        {
            ppu->bgFilter = 3;
        }
        if (key == KB_KEY_F4)
        {
            ppu->bgFilter = 4;
        }
        if (key == KB_KEY_F5)
        {
            ppu->bgFilter = 5;
        }
        if (key == KB_KEY_F6)
        {
            ppu->bgFilter = 0;
        }

        // Controller
        if (key == KB_KEY_Z)
        {
            ctrlsys->KeyPress(15); // B
        }
        if (key == KB_KEY_SPACE)
        {
            ctrlsys->KeyPress(12); // S
        }
    }
    // Controller
    if (key == KB_KEY_Z)
    {
        if (is_pressed)
            ctrlsys->KeyPress(15); // B
        else
            ctrlsys->KeyRelease(15);
    }
    if (key == KB_KEY_X)
    {
        if (is_pressed)
            ctrlsys->KeyPress(7); // A
        else
            ctrlsys->KeyRelease(7);
    }
    if (key == KB_KEY_A)
    {
        if (is_pressed)
            ctrlsys->KeyPress(14); // Y
        else
            ctrlsys->KeyRelease(14);
    }
    if (key == KB_KEY_S)
    {
        if (is_pressed)
            ctrlsys->KeyPress(6); // X
        else
            ctrlsys->KeyRelease(6);
    }
    if (key == KB_KEY_SPACE)
    {
        if (is_pressed)
            ctrlsys->KeyPress(12); // Start
        else
            ctrlsys->KeyRelease(12);
    }
    if (key == KB_KEY_ENTER)
    {
        if (is_pressed)
            ctrlsys->KeyPress(13); // Start
        else
            ctrlsys->KeyRelease(13);
    }
    if (key == KB_KEY_LEFT)
    {
        if (is_pressed)
            ctrlsys->KeyPress(9); // Left
        else
            ctrlsys->KeyRelease(9);
    }
    if (key == KB_KEY_RIGHT)
    {
        if (is_pressed)
            ctrlsys->KeyPress(8); // Right
        else
            ctrlsys->KeyRelease(8);
    }
    if (key == KB_KEY_UP)
    {
        if (is_pressed)
            ctrlsys->KeyPress(11); // UP
        else
            ctrlsys->KeyRelease(11);
    }
    if (key == KB_KEY_DOWN)
    {
        if (is_pressed)
            ctrlsys->KeyPress(10); // Down
        else
            ctrlsys->KeyRelease(10);
    }
}

void mouse_btn_callback(struct mfb_window *window, mfb_mouse_button button, mfb_key_mod mod, bool is_pressed)
{
    if (is_pressed)
    {
        mouseX /= (WIN_WIDTH / FB_WIDTH);
        mouseY /= (WIN_WIDTH / FB_WIDTH);
        uint16 vCounter = mouseY;
        uint16 hCounter = mouseX;
        uint16 objcol;
        uint8 ojbprior = 0;
        bool objopaque;
        cout << "Clicked at : (" << dec << mouseX << "," << mouseY << ")" << endl;
        for (int i = 0; i < 128; i++)
        {
            uint8 shftAmnt = ((i & 0b00000111) << 1);
            uint8 objY = (ppu->oam[i << 1] & 0xff00) >> 8;
            int16_t objX = (ppu->oam[i << 1] & 0x00ff);
            bool xSign = ppu->oam[256 + (i >> 3)] & (0x01 << shftAmnt);
            objX |= xSign ? 0xFF00 : 0x0000;

            bool objSizeFlag = (ppu->oam[256 + (i >> 3)] & (0x02 << shftAmnt));

            uint8 objSize = 8;
            switch (ppu->objAvailSize)
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

            if (vCounter < objY || vCounter >= objY + objSize || hCounter < objX || hCounter >= objX + objSize) // Point not in obj then go to next
            {
                continue;
            }

            uint16 dt = ppu->oam[(i << 1) + 1];
            uint16 chr = dt & 0x00ff;
            bool table = dt & 0x0100;
            uint8 pal = (dt & 0b0000111000000000) >> 9;
            uint8 prior = (dt & 0b0011000000000000) >> 12;
            bool vf = dt & 0b1000000000000000;
            bool hf = dt & 0b0100000000000000;

            // 00 01 101 110001100
            // 00 11 000 000000000

            uint8 colorIdx = 0;

            uint8 tileX = (hCounter - objX) >> 3;
            uint8 tileY = (vCounter - objY) >> 3;
            if (hf)
                tileX = ((objSize >> 3) - 1) - tileX;
            if (vf)
                tileY = ((objSize >> 3) - 1) - tileY;

            uint8 x = (hCounter - objX) & 0x07;
            uint8 y = (vCounter - objY) & 0x07;

            chr += tileX + (tileY << 4);

            if (hf)
                x = 7 - x;
            if (vf)
                y = 7 - y;

            uint16 objCharAddr = (table ? ppu->OBJChrTable2BaseAddr : ppu->OBJChrTable1BaseAddr) + (chr << 4); // Obj size later plays a part in here

            colorIdx |= ((ppu->vram[objCharAddr + y] >> (7 - x)) & 0x01);

            colorIdx |= ((ppu->vram[objCharAddr + y] >> (15 - x)) & 0x01) << 1;

            colorIdx |= ((ppu->vram[objCharAddr + y + 8] >> (7 - x)) & 0x01) << 2;

            colorIdx |= ((ppu->vram[objCharAddr + y + 8] >> (15 - x)) & 0x01) << 3;

            if (prior >= ojbprior && colorIdx != 0)
            {
                ojbprior = prior;
                objcol = ppu->cgram[0b10000000 | (pal << 4) | colorIdx];
                objopaque = true;
            }

            if (objopaque)
            {
                cout << "=-=-=-=-=-=- OBJ[" << i << "] -=-=-=-=-=-=-=" << endl;
                cout << "objData at " << dec << (256 + (i >> 3)) << " : " << hex << ppu->oam[256 + (i >> 3)] << endl;
                cout << "hCount " << mouseX << endl;
                cout << "vCount " << mouseY << endl;
                cout << "dt " << hex << dt << endl;
                cout << "tileX " << (uint16)tileX << endl;
                cout << "tileY " << (uint16)tileY << endl;
                cout << "colorIdx " << (uint16)colorIdx << endl;
                cout << "objCol " << (uint16)objcol << endl;
                cout << "objGlobalSize " << (uint16)ppu->objAvailSize << endl;
                cout << "objSizeFlag " << (uint16)objSizeFlag << endl;
                cout << "objSize " << (uint16)objSize << endl;
                cout << "ojbprior " << (uint16)prior << endl;
                cout << "vCount " << mouseY << endl;
                cout << "x " << objX << " y " << (uint16)objY << endl;
                cout << "Char " << chr << endl;
                cout << "CharAddr " << hex << objCharAddr << endl;
                cout << "paletter " << dec << (uint16)pal << endl;
                // cin.get();
            }
        }
    }
}

void mouse_move_callback(struct mfb_window *window, int x, int y)
{
    mouseX = x;
    mouseY = y;
}

void WriteIO(add24 address, uint8 data)
{
    uint16 port = address & 0x0000FFFF;
    // cout << "Read IO Here : " << port << endl;
    //  cin.get();
    if (port >= 0x2100 && port <= 0x213F) // PPU
    {
        ppu->IOWrite(port, data);
        return;
    }
    else if (port >= 0x2140 && port <= 0x2143) // APU
    {
        APU::IOWrite(port, data);
        return;
    }
    else if (port >= 0x2144 && port <= 0x217F) // APU Mirrored
    {
        APU::IOWrite(0x2140 | (port & 0x03), data);
        return;
    }
    else if (port >= 0x4300 && port <= 0x437a) // DMA
    {
        dma->IOWrite(port, data);
        return;
    }
    else if (port >= 0x4218 && port <= 0x421F) // Controller
    {
        ctrlsys->IOWrite(port, data);
        return;
    }
    else if (port >= 0x4202 && port <= 0x4206)
    {
        mdu->IOWrite(port, data);
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
            ctrlsys->IOWrite(port, data);
            break;

        case 0x4200: // NMITIMEN
            NMITIMEN = data;
            ctrlsys->stdCtrlEn = data & 0x01;

            break;
        case 0x4201: // WRIO
            ctrlsys->IOWrite(port, data);
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

            dma->IOWrite(port, data);
            break;

        case 0x420c: // HDMAEN

            dma->IOWrite(port, data);
            break;

        // TODO : implement actual memory speed here
        case 0x420d: // MEMSEL
            MEMSEL = data;
            break;

        // Some games really like to write into read-only regs. so here they are
        default:

            cout << "Undefined IO Address " << hex << address << endl;
            zeroWire = data;
            // pauseEmu = true;
            break;
        }
    }
}

uint8 ReadIO(add24 address)
{

    uint16 port = address & 0x0000FFFF;
    // cout << "Read IO Here : " << port << endl;

    if (port >= 0x2100 && port <= 0x213F) // PPU
    {
        if (port == 0x2137)
            cpu->setOverflow();
        return ppu->IORead(port);
    }
    else if (port >= 0x2140 && port <= 0x2143) // APU
    {
        return APU::IORead(port);
    }
    else if (port >= 0x2144 && port <= 0x217F) // APU Mirrored
    {
        return APU::IORead(0x2140 | (port & 0x03));
    }
    else if (port >= 0x4300 && port <= 0x437a) // DMA
    {
        return dma->IORead(port);
    }
    else if (port >= 0x4218 && port <= 0x421F) // Controller
    {
        return ctrlsys->IORead(port);
    }
    else if (port >= 0x4214 && port <= 0x4217)
    {
        return mdu->IORead(port);
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
            return ctrlsys->IORead(port);
            break;
        case 0x4017: // LCTRLREG2, Manual controller 1
            return ctrlsys->IORead(port);
            break;

        case 0x4212: // HVBJOY
            return ((ppu->vBlank) << 7) | ((ppu->hBlank) << 6) | (0);
            break;

        case 0x4213: // RDIO
            return ctrlsys->IORead(port);
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

            cout << "Undefined IO Address " << hex << address << endl;

            // pauseEmu = true;
            return zeroWire;
            break;
        }
    }
    return zeroWire;
}

uint8 BusAccess(add24 address, uint8 data, bool rd = false)
{

    // cout << "Translate Address : " << std::hex << address << endl;

    if (rom->mapType == LoROM)
    {
        // Extract bank and offset for easier math
        uint8_t bank = (address >> 16) & 0xFF;
        uint16_t offset = address & 0xFFFF;

        // 1. WRAM (Banks $7E and $7F)
        // This must be checked BEFORE we mirror the upper banks down.
        if (bank == 0x7E || bank == 0x7F)
        {

            // Mask to 17 bits (128KB of WRAM)
            if (rd)
            {
                return *(ram + (address & 0x01FFFF));
            }
            else
            {
                *(ram + (address & 0x01FFFF)) = data;
            }
        }

        // Mirror FastROM banks ($80-$FF) down to SlowROM banks ($00-$7F)
        bank = bank & 0x7F;

        // 2. ROM (Offsets $8000-$FFFF in banks $00-$7D)
        if (offset >= 0x8000)
        {
            // Calculate physical contiguous ROM offset without holes
            uint32_t rom_offset = (bank * 0x8000) + (offset - 0x8000);

            // Good practice: ensure we don't read out of bounds
            if (rom_offset < rom->romSize)
            {
                if (rd)
                    return *(rom->rom + rom_offset);
                else
                {
                    cout << "Writing to ROM?! I don't think so." << endl;
                    pauseEmu = true;
                }
            }
        }

        // 3. Low RAM and I/O (Banks $00-$3F)
        if (bank <= 0x3F)
        {
            if (offset < 0x2000) // First 8KB mirrors WRAM
            {

                if (rd)
                    return *(ram + offset);
                else
                    *(ram + offset) = data;
            }
            if (offset >= 0x2100 && offset < 0x6000) // Hardware I/O
            {
                if (rd)
                    return ReadIO(address);
                else
                    WriteIO(address, data);
            }
        }

        // 4. SRAM (Typically Banks $70-$7D, Offsets $0000-$7FFF)
        if (bank >= 0x70 && bank <= 0x7D && offset < 0x8000)
        {
            if (!rom->hasSram)
            {
                return zeroWire;
            }
            // Calculate contiguous SRAM offset
            uint32_t sram_offset = ((bank - 0x70) * 0x8000) + offset;
            sram_offset &= (rom->sramSize - 1);

            if (rd)
                return *(rom->sram + sram_offset);
            else
                *(rom->sram + sram_offset) = data;
        }
        return zeroWire;
    }
    else if (rom->mapType == HiROM || rom->mapType == ExHiROM)
    {
        uint8_t bank = (address >> 16) & 0xFF;
        uint16_t offset = address & 0xFFFF;

        // 1. WRAM (Banks $7E and $7F)
        // Same as LoROM, must be checked first
        if (bank == 0x7E || bank == 0x7F)
        {
            if (rd)
                return *(ram + (address & 0x01FFFF));
            else
            {
                *(ram + (address & 0x01FFFF)) = data;
                return zeroWire; // Assuming zeroWire is returned on write
            }
        }

        // 2. Low RAM, I/O, and SRAM (Banks $00-$3F and $80-$BF)
        // These banks share the same layout in the bottom 32KB
        if ((bank >= 0x00 && bank <= 0x3F) || (bank >= 0x80 && bank <= 0xBF))
        {
            if (offset < 0x2000) // First 8KB mirrors WRAM
            {
                if (rd)
                    return *(ram + offset);
                else
                    *(ram + offset) = data;
            }
            else if (offset >= 0x2100 && offset < 0x6000) // Hardware I/O
            {
                if (rd)
                    return ReadIO(address);
                else
                    WriteIO(address, data);
            }
            else if (offset >= 0x6000 && offset < 0x8000) // SRAM
            {
                if (!rom->hasSram)
                {
                    return zeroWire;
                }
                // SRAM maps to 8KB chunks. Strip to 5 bits for continuous SRAM mapping.
                uint32_t sram_offset = ((bank & 0x1F) * 0x2000) + (offset - 0x6000);
                sram_offset &= (rom->sramSize - 1);

                if (rd)
                    return *(rom->sram + sram_offset);
                else
                    *(rom->sram + sram_offset) = data;
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
            if (rom->mapType == ExHiROM)
            {
                rom_offset += 0x400000; // Offset by 4MB
            }

            if (rom_offset < rom->romSize)
            {

                if (rd)
                    return *(rom->rom + rom_offset);
                else
                {
                    cout << "Rom at : " << hex << rom_offset << endl;
                    cout << "Writing to ROM?! I don't think so." << endl;
                    pauseEmu = true;
                }
            }
        }
        else if (offset >= 0x8000)
        {
            // For banks $00-$3F (and their FastROM mirrors $80-$BF),
            // offsets $8000-$FFFF mirror the ROM data from banks $C0-$FF.
            uint8_t mirror_bank = bank & 0x3F;
            uint32_t rom_offset = (mirror_bank * 0x10000) + offset;

            if (rom_offset < rom->romSize)
            {
                if (rd)
                    return *(rom->rom + rom_offset);
                else
                {
                    cout << "Writing to ROM?! I don't think so." << endl;
                    pauseEmu = true;
                }
            }
        }

        return zeroWire;
    }

    return 0;
}

uint8 C65Read(add24 address)
{
    return BusAccess(address, NULL, true);
}
void C65Write(add24 address, uint8 data)
{

    BusAccess(address, data, false);
}

uint8 DMARead(add24 address)
{
    // cout << "Translate Address : " << std::hex << address << endl;
    return BusAccess(address, NULL, true);
}
void DMAWrite(add24 address, uint8 data)
{
    BusAccess(address, data, false);
}

int main(int argc, char *argv[])
{

    if (argc < 2)
    {
        std::cerr << "Error: No file path provided." << std::endl;
        std::cerr << "Usage: " << argv[0] << " <path_to_sfc_file>" << std::endl;
        return 1;
    }

    rom = new Cartridge(argv[1]);

    stringstream ss;
    ss << rom->title << ".srm";
    ifstream sramFile(ss.str(), std::ios::binary);

    if (rom->hasSram)
    {
        bool sramExists = true;
        if (!sramFile.is_open())
        {
            sramExists = false;
        }
        if (sramExists)
        {
            sramFile.read(reinterpret_cast<char *>(rom->sram), rom->sramSize);
        }
        sramFile.close();
    }

    cpu = new CPU(C65Read, C65Write);

    ppu = new PPU();
    dma = new DMA(DMARead, DMAWrite);
    mdu = new MDUnit();
    ctrlsys = new ControllerSystem();

    cpuTraceFile = ofstream("CPUTrace.txt");

    window = mfb_open_ex("MTOSFC", WIN_WIDTH, WIN_HEIGHT, NULL);
    mfb_set_keyboard_callback(window, keyboard_callback);
    mfb_set_mouse_button_callback(window, mouse_btn_callback);
    mfb_set_mouse_move_callback(window, mouse_move_callback);
    zeroWire = 0;

    emu();

    return 0;
}