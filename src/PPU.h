#include <MiniFB.h>
typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int add24;

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

void PPU_Step();
void PPU_Reset();
void PPU_IOWrite(add24 address, uint8 data);
uint8 PPU_IORead(add24 address);