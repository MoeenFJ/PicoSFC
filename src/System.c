#include <MiniFB.h>
#include <chrono>

using namespace std;

typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int add24;

uint8 C65Read(add24 address);
void C65Write(add24 address, uint8 data);

uint8 DMARead(add24 address);
void DMAWrite(add24 address, uint8 data);

#include "Cartridge.h"
#include "PPU.h"
#include "APU.h"
#include "CPU.h"
#include "DMA.h"
#include "CTRL.h"
#include "MDUnit.h"



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

    outFile.write(reinterpret_cast<const char *>(PPU::vram), (size_t)32 * 1024 * sizeof(uint16));

    outFile.close();
}
void DumpOARam()
{
    ofstream outFile("OAMDump.bin", std::ios::binary);

    outFile.write(reinterpret_cast<const char *>(PPU::oam), (size_t)272 * sizeof(uint16));

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
    CPU::reset();
    PPU::reset();
    APU_Init();
    VTIMEL = 0xff;
    VTIMEH = 0x01;
    HTIMEL = 0xff;
    HTIMEH = 0x01;
    RDNMI = RDNMI & 0x7F;
    pauseEmu = true;
    frameTime = chrono::steady_clock::now();
    while (true) // Emulation Loop
    {

        if ((pauseEmu || PPU::pauseEmu) && !runStep && !runVBlank && !runInst && !runHBlank)
        {
            mfb_update_events(window);
            continue;
        }

        if (!DMA::dmaActive)
        {

            if (emuStep % 8 == 0)
            {
                runInst = false;
                if (debug)
                {
                    CPU::printStatus();
                }
                if (cpuTrace)
                {
                    cpuTraceFile << CPU::stringStatus() << endl;
                }

                auto start = chrono::steady_clock::now();
                CPU::cpuStep();

                auto end = chrono::steady_clock::now();
                cpuTime += chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
                // if(CPU::cregs.PC == 0xfa88 && CPU::cregs.K == 0x09)
                //     pauseEmu = true;
            }
        }

        if (emuStep % 2 == 0)
        {
            auto start = chrono::steady_clock::now();
            DMA::step(!hdmaRan);
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
            APU_Step();
            auto end = chrono::steady_clock::now();
            apuTime += chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        }

        // ctrl is read
        if (emuStep % 2 == 0)
        {
            auto start = chrono::steady_clock::now();
            ControllerSystem::step();
            auto end = chrono::steady_clock::now();
            ctrlTime += chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();
        }

        vBlankEntryMoment = false;

        auto start = chrono::steady_clock::now();
        PPU::step(); // every 341*262 = 89342 steps => 1 frame
        auto end = chrono::steady_clock::now();
        ppuTime += chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

        if (PPU::hCounter == 0 && PPU::vCounter == 225) // Start of VBlank
        {
          
            vBlankEntryMoment = true;
            runVBlank = false;
            RDNMI = 0b10000000;
            if (NMITIMEN & 0b10000000) // NMI is enabled
                CPU::invokeNMI();

            // for (int y = 0; y < FB_HEIGHT * (WIN_WIDTH / FB_WIDTH); y++)
            // {
            //     for (int x = 0; x < WIN_WIDTH; x++)
            //     {
            //         window_fb[y * WIN_WIDTH + x] = PPU::fb[(y / (WIN_WIDTH / FB_WIDTH)) * FB_WIDTH + (x / (WIN_WIDTH / FB_WIDTH))];
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
                    auto pixel = PPU::fb[src_row_idx + sx];
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

                    uint16 col = PPU::cgram[x / (WIN_WIDTH / FB_WIDTH)];

                    uint8 R = (col & 0b0000000000011111) << 3;
                    uint8 G = (col & 0b0000001111100000) >> 2;
                    uint8 B = (col & 0b0111110000000000) >> 7;

                    window_fb[y * WIN_WIDTH + x] = MFB_ARGB(255, R, G, B);
                }
            }

            mfb_update_ex(window, window_fb, WIN_WIDTH, WIN_HEIGHT);
            mfb_set_title(window, to_string(PPU::frameCount).c_str());
            mfb_wait_sync(window);
            auto end = chrono::steady_clock::now();
            drawTime += chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

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
            cout << "Debug : " << dec << chrono::duration_cast<std::chrono::nanoseconds>(end - start).count() / 1000000 << endl;
            cpuTime = 0;
            ppuTime = 0;
            apuTime = 0;
            ctrlTime = 0;
            dmaTime = 0;
            otherTime = 0;
            drawTime = 0;
        }

        start = chrono::steady_clock::now();

        if (PPU::vCounter == 261 && PPU::hCounter == 340) // End of VBlank
        {
            RDNMI = 0b00000000;
        }
        if (PPU::vCounter < 225 && PPU::hCounter == 256) // Start of HBlank
        {
            hdmaRan = false;
            runHBlank = false;
        }
        if (PPU::hCounter == 0 && PPU::vCounter == 0) // Start of Frame
        {
            DMA::initializeHDMA();
        }

        // H-V Timer IRQ
        uint8 type = (NMITIMEN & 0b00110000) >> 4;

        switch (type)
        {
        case 0:
            break;
        case 1:
            TIMEUP = 0b10000000;
            if ((HTIMEH << 8 | HTIMEL) == PPU::hCounter)
                CPU::invokeIRQ();
            break;
        case 2:
            TIMEUP = 0b10000000;
            if ((VTIMEH << 8 | VTIMEL) == PPU::vCounter && PPU::hCounter == 0)
                CPU::invokeIRQ();
            break;
        case 3:
            TIMEUP = 0b10000000;
            if ((VTIMEH << 8 | VTIMEL) == PPU::vCounter && PPU::hCounter == (HTIMEH << 8 | HTIMEL))
                CPU::invokeIRQ();
            break;

        default:
            break;
        }
        end = chrono::steady_clock::now();
        otherTime += chrono::duration_cast<std::chrono::nanoseconds>(end - start).count();

        emuStep++;
        runStep = false;
    }
}

void exitEmu()
{
    cpuTraceFile.close();

    stringstream ss;
    ss << Cartridge::title << ".srm";

    ofstream sramFile(ss.str(), std::ios::binary);
    sramFile.write(reinterpret_cast<const char *>(Cartridge::sram), Cartridge::sramSize * sizeof(uint8));
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
            PPU::pauseEmu = pauseEmu;
        }
    }
    // Controller
    if (key == KB_KEY_Z)
    {
        if (is_pressed)
            ControllerSystem::KeyPress(15); // B
        else
            ControllerSystem::KeyRelease(15);
    }
    if (key == KB_KEY_X)
    {
        if (is_pressed)
            ControllerSystem::KeyPress(7); // A
        else
            ControllerSystem::KeyRelease(7);
    }
    if (key == KB_KEY_A)
    {
        if (is_pressed)
            ControllerSystem::KeyPress(14); // Y
        else
            ControllerSystem::KeyRelease(14);
    }
    if (key == KB_KEY_S)
    {
        if (is_pressed)
            ControllerSystem::KeyPress(6); // X
        else
            ControllerSystem::KeyRelease(6);
    }
    if (key == KB_KEY_SPACE)
    {
        if (is_pressed)
            ControllerSystem::KeyPress(12); // Start
        else
            ControllerSystem::KeyRelease(12);
    }
    if (key == KB_KEY_ENTER)
    {
        if (is_pressed)
            ControllerSystem::KeyPress(13); // Start
        else
            ControllerSystem::KeyRelease(13);
    }
    if (key == KB_KEY_LEFT)
    {
        if (is_pressed)
            ControllerSystem::KeyPress(9); // Left
        else
            ControllerSystem::KeyRelease(9);
    }
    if (key == KB_KEY_RIGHT)
    {
        if (is_pressed)
            ControllerSystem::KeyPress(8); // Right
        else
            ControllerSystem::KeyRelease(8);
    }
    if (key == KB_KEY_UP)
    {
        if (is_pressed)
            ControllerSystem::KeyPress(11); // UP
        else
            ControllerSystem::KeyRelease(11);
    }
    if (key == KB_KEY_DOWN)
    {
        if (is_pressed)
            ControllerSystem::KeyPress(10); // Down
        else
            ControllerSystem::KeyRelease(10);
    }
}


void WriteIO(add24 address, uint8 data)
{
    uint16 port = address & 0x0000FFFF;
    // cout << "Read IO Here : " << port << endl;
    //  cin.get();
    if (port >= 0x2100 && port <= 0x213F) // PPU
    {
        PPU::IOWrite(port, data);
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
        DMA::IOWrite(port, data);
        return;
    }
    else if (port >= 0x4218 && port <= 0x421F) // Controller
    {
        ControllerSystem::IOWrite(port, data);
        return;
    }
    else if (port >= 0x4202 && port <= 0x4206)
    {
        MDU::IOWrite(port, data);
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
            ControllerSystem::IOWrite(port, data);
            break;

        case 0x4200: // NMITIMEN
            NMITIMEN = data;
            ControllerSystem::stdCtrlEn = data & 0x01;

            break;
        case 0x4201: // WRIO
            ControllerSystem::IOWrite(port, data);
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

            DMA::IOWrite(port, data);
            break;

        case 0x420c: // HDMAEN

            DMA::IOWrite(port, data);
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
            CPU::setOverflow();
        return PPU::IORead(port);
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
        return DMA::IORead(port);
    }
    else if (port >= 0x4218 && port <= 0x421F) // Controller
    {
        return ControllerSystem::IORead(port);
    }
    else if (port >= 0x4214 && port <= 0x4217)
    {
        return MDU::IORead(port);
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
            return ControllerSystem::IORead(port);
            break;
        case 0x4017: // LCTRLREG2, Manual controller 1
            return ControllerSystem::IORead(port);
            break;

        case 0x4212: // HVBJOY
            return ((PPU::vBlank) << 7) | ((PPU::hBlank) << 6) | (0);
            break;

        case 0x4213: // RDIO
            return ControllerSystem::IORead(port);
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

    if (Cartridge::mapType == LoROM)
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
            if (rom_offset < Cartridge::romSize)
            {
                if (rd)
                    return *(Cartridge::rom + rom_offset);
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
            if (!Cartridge::hasSram)
            {
                return zeroWire;
            }
            // Calculate contiguous SRAM offset
            uint32_t sram_offset = ((bank - 0x70) * 0x8000) + offset;
            sram_offset &= (Cartridge::sramSize - 1);

            if (rd)
                return *(Cartridge::sram + sram_offset);
            else
                *(Cartridge::sram + sram_offset) = data;
        }
        return zeroWire;
    }
    else if (Cartridge::mapType == HiROM || Cartridge::mapType == ExHiROM)
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
                if (!Cartridge::hasSram)
                {
                    return zeroWire;
                }
                // SRAM maps to 8KB chunks. Strip to 5 bits for continuous SRAM mapping.
                uint32_t sram_offset = ((bank & 0x1F) * 0x2000) + (offset - 0x6000);
                sram_offset &= (Cartridge::sramSize - 1);

                if (rd)
                    return *(Cartridge::sram + sram_offset);
                else
                    *(Cartridge::sram + sram_offset) = data;
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
            if (Cartridge::mapType == ExHiROM)
            {
                rom_offset += 0x400000; // Offset by 4MB
            }

            if (rom_offset < Cartridge::romSize)
            {

                if (rd)
                    return *(Cartridge::rom + rom_offset);
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

            if (rom_offset < Cartridge::romSize)
            {
                if (rd)
                    return *(Cartridge::rom + rom_offset);
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

    Cartridge::load(argv[1]);
    stringstream ss;
    ss << Cartridge::title << ".srm";
    ifstream sramFile(ss.str(), std::ios::binary);

    if (Cartridge::hasSram)
    {
        bool sramExists = true;
        if (!sramFile.is_open())
        {
            sramExists = false;
        }
        if (sramExists)
        {
            sramFile.read(reinterpret_cast<char *>(Cartridge::sram), Cartridge::sramSize);
        }
        sramFile.close();
    }





    window = mfb_open_ex("MTOSFC", WIN_WIDTH, WIN_HEIGHT, NULL);
    mfb_set_target_fps(240);  
    mfb_set_keyboard_callback(window, keyboard_callback);

    zeroWire = 0;

    emu();

    return 0;
}