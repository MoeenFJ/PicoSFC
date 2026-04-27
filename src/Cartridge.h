#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int add24;
typedef unsigned char bool;

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
ROMMapType mapType;
char title[21];
bool speed; // 0=Slow , 1=Fast

void Cartridge_Load(char* address);