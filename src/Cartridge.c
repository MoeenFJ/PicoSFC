
#include "Cartridge.h"

void load(const char *address)
{
    FILE *romFile;
    romFile = fopen(address, "rb");

    fseek(romFile, 0, SEEK_END);
    romSize = ftell(romFile);
    rewind(romFile);

    printf("Size : %d\n", romSize);

    rom = (uint8_t *)malloc(romSize);

    size_t bytes_read = fread(rom, 1, romSize, romFile);

    fclose(rom);

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

    printf("Chipset settings : %h\n", (uint16)readByte);
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
    printf("Map Type : %s\n", (mapType == LoROM ? "LoROM" : mapType == HiROM ? "HiROM"
                                                        : mapType == ExHiROM ? "ExHiROM"
                                                                             : "Invalid"));
}
