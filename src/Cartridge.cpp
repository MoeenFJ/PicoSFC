#include <bits/stdc++.h>
using namespace std;
typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int add24;

enum ROMMapType
{
    LoROM,
    HiROM,
    ExHiROM
};
namespace Cartridge
{


    unsigned char *rom;
    unsigned char *sram;
    bool hasSram = false;
    int romSize;
    int sramSize=0;
    ROMMapType mapType;
    char title[21];
    bool speed; // 0=Slow , 1=Fast

    void load(string address)
    {

        ifstream inROM(address);

        inROM.seekg(0, ios::end);
        romSize = inROM.tellg();
        inROM.seekg(0, ios::beg);
        cout << "Size : " << romSize << endl;

        rom = (uint8_t *)malloc(romSize);

        inROM.read((char *)rom, romSize);

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
                cout << i << " , " << (int)(readByte) << endl;
                if ((((1 << (readByte + 10)) + (i % 2 ? 512 : 0)) >= (romSize) && ((1 << (readByte + 9)) + (i % 2 ? 512 : 0)) < (romSize)) ||
                    (((1 << (readByte + 11)) + (i % 2 ? 512 : 0)) >= (romSize) && ((1 << (readByte + 10)) + (i % 2 ? 512 : 0)) < (romSize)))
                    break;
            }

            i++;

            if (i == 6)
            {
                cout << "Couldn't identify the ROM." << endl;
                exit(0);
            }
        } while (true);

        // At this point we can do a checksum to make sure of rom map type and header location
        headerLoc = headerLocs[i];

        memcpy(title, rom + headerLoc + 0, 21); // Title

        switch (romTypeNum[i])
        {
        case 0:
            mapType = LoROM;
            break;
        case 1:
            mapType = HiROM;
            break;
        case 5:
            mapType = ExHiROM;
            break;
        }

        memcpy(&readByte, rom + headerLoc + 21, 1); // Speed And Type - 0xFFD5
        speed = readByte & 0b00010000;

        memcpy(&readByte, rom + headerLoc + 22, 1); // 0xFFD6

        cout << "Chipset settings : " << hex << (uint16)readByte << endl;
        if (readByte == 0x02)
        {
            hasSram = true;
            memcpy(&readByte, rom + headerLoc + 24, 1); // SramSize - 0xFFD8

            sramSize = 1 << (readByte + 10);
            cout << "Sram size : " << dec << sramSize << endl;
            sram = (uint8_t *)malloc(sramSize);
        }

        if (i % 2)
        {
            rom += 512;
        }

        cout << "Size : " << romSize << endl;
        cout << "Title : " << title << endl;
        cout << "Speed : " << (speed ? "Fast" : "Slow") << endl;
        cout << "Map Type : " << (mapType == LoROM ? "LoROM" : mapType == HiROM ? "HiROM"
                                                                 : mapType == ExHiROM ? "ExHiROM"
                                                                                            : "Invalid")
             << endl;
    }
}