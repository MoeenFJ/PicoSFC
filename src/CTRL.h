typedef unsigned char uint8;
typedef unsigned char bool;
typedef unsigned short uint16;
typedef unsigned int add24;

typedef struct
{
    uint8 LOW;
    uint8 HIGH;
} ControllerRegs;
ControllerRegs ctrl[4];
uint8 LCTRLREG1;
uint8 LCTRLREG2;
uint8 HVBJOY;
uint8 RDIO;
uint8 WRIO;
uint16 player1;
bool stdCtrlEn = 1;

uint8 legCnt1 = 0;

uint8 CTRL_IORead(add24 address);
void CTRL_IOWrite(add24 address, uint8 data);
void CTRL_Step();
void CTRL_KeyPress(uint8 key);
void CTRL_KeyRelease(uint8 key);