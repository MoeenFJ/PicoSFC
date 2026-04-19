#include <bits/stdc++.h>
typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int add24;

struct ControllerRegs
{
    uint8 LOW;
    uint8 HIGH;
};

class ControllerSystem
{
public:
    ControllerRegs ctrl[4];
    uint8 LCTRLREG1;
    uint8 LCTRLREG2;
    uint8 HVBJOY;
    uint8 RDIO;
    uint8 WRIO;
    uint16 player1;
    bool stdCtrlEn = true;

    uint8 legCnt1 = 0;

    uint8 IORead(add24 address)
    {
        switch (address)
        {

        case 0x4017: // LCTRLREG2, Manual controller 1
            return this->LCTRLREG2;
            break;

        case 0x4016: // LCTRLREG1, Manual controller 1
            LCTRLREG1 = player1 >> (15-legCnt1);
            legCnt1++;
            if(legCnt1 == 16)
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
            return this->ctrl[0].LOW;
            break;
        case 0x4219:
            return this->ctrl[0].HIGH;
            break;

        case 0x421A:
            return this->ctrl[1].LOW;
            break;
        case 0x421B:
            return this->ctrl[1].HIGH;
            break;

        case 0x421C:
            return this->ctrl[2].LOW;
            break;
        case 0x421D:
            return this->ctrl[2].HIGH;
            break;

        case 0x421E:
            return this->ctrl[3].LOW;
            break;
        case 0x421F:
            return this->ctrl[3].HIGH;
            break;
        }
        return 0;
    }
    void IOWrite(add24 address, uint8 data)
    {
        switch (address)
        {

        case 0x4016: // LCTRLREG1, Manual controller 1
            this->LCTRLREG1 = data;
            break;
        case 0x4201:
            WRIO = data;
            break;
        }
    }

    void step()
    {
        if(true || stdCtrlEn){
            ctrl[0].LOW = player1;
            ctrl[0].HIGH = player1>>8;
        }
        else
        {
            ctrl[0].LOW = 0;
            ctrl[0].HIGH = 0;
        }
    }

    void KeyPress(uint8 key)
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

        player1 |= 1<<key;
        
    }
    void KeyRelease(uint8 key)
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
        player1 &= ~(1<<key);
        
    }
    // Here needs to be a state machine
};