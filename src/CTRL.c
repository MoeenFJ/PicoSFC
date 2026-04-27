#include "CTRL.h"

uint8 CTRL_IORead(add24 address)
{
    switch (address)
    {

    case 0x4017: // LCTRLREG2, Manual controller 1
        return LCTRLREG2;
        break;

    case 0x4016: // LCTRLREG1, Manual controller 1
        LCTRLREG1 = player1 >> (15 - legCnt1);
        legCnt1++;
        if (legCnt1 == 16)
            legCnt1 = 0;
        return LCTRLREG1;
        break;

    case 0x4212:
        return HVBJOY;
        break;
    case 0x4213:
        return RDIO;
        break;
    // Standard Controllers
    case 0x4218:
        return ctrl[0].LOW;
        break;
    case 0x4219:
        return ctrl[0].HIGH;
        break;

    case 0x421A:
        return ctrl[1].LOW;
        break;
    case 0x421B:
        return ctrl[1].HIGH;
        break;

    case 0x421C:
        return ctrl[2].LOW;
        break;
    case 0x421D:
        return ctrl[2].HIGH;
        break;

    case 0x421E:
        return ctrl[3].LOW;
        break;
    case 0x421F:
        return ctrl[3].HIGH;
        break;
    }
    return 0;
}
void CTRL_IOWrite(add24 address, uint8 data)
{
    switch (address)
    {

    case 0x4016: // LCTRLREG1, Manual controller 1
        LCTRLREG1 = data;
        break;
    case 0x4201:
        WRIO = data;
        break;
    }
}

void CTRL_Step()
{
    if (true || stdCtrlEn)
    {
        ctrl[0].LOW = player1;
        ctrl[0].HIGH = player1 >> 8;
    }
    else
    {
        ctrl[0].LOW = 0;
        ctrl[0].HIGH = 0;
    }
}

void CTRL_KeyPress(uint8 key)
{
    // 4 - R
    // 5 - L
    // 6 - X
    // 7 - A
    // 8 - Right
    // 9 - Left
    // 10 - Down
    // 11 - UP
    // 12 - Start
    // 13 - Select
    // 14 - Y
    // 15 - B

    player1 |= 1 << key;
}
void CTRL_KeyRelease(uint8 key)
{
    // 4 - R
    // 5 - L
    // 6 - X
    // 7 - A
    // 8 - Right
    // 9 - Left
    // 10 - Down
    // 11 - UP
    // 12 - Start
    // 13 - Select
    // 14 - Y
    // 15 - B
    player1 &= ~(1 << key);
}
