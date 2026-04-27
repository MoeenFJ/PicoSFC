
#include <bits/stdc++.h>
using namespace std;

typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int add24;

namespace MDU
{
    uint8 WRMPYA; // 0X4202
    uint8 WRMPYB; // 0X4203
    uint8 WRDIVL; // 0X4204
    uint8 WRDIVH; // 0X4205
    uint8 WRDIVB; // 0X4206

    uint8 RDDIVL; // 0x4214
    uint8 RDDIVH; // 0x4215
    uint8 RDMPYL; // 0x4216
    uint8 RDMPYH; // 0x4217

    void IOWrite(add24 address, uint8 data)
    {
        switch (address)
        {
        case 0X4202:
            WRMPYA = data;
            break;
        case 0X4203:
        {
            WRMPYB = data;
            uint16 c = (uint16)WRMPYA*(uint16)WRMPYB;
            RDMPYL = c;
            RDMPYH = c>>8;
            break;

        }
            

        case 0X4204:
            WRDIVL = data;
            break;
        case 0X4205:
            WRDIVH = data;
            break;
        case 0X4206:
        {
            WRDIVB = data;
            uint16 c = WRDIVB!=0?((uint16)WRDIVL | (((uint16)WRDIVH)<<8))/(uint16)WRDIVB:0;
            RDDIVL = c;
            RDDIVH = c>>8;
            uint8 rem = WRDIVB!=0?((uint16)WRDIVL | (((uint16)WRDIVH)<<8))%(uint16)WRDIVB:0;
            RDMPYL = rem;
            RDMPYH = rem>>8;
            break;
        }
     
        
        default:
            break;
        }
    }

    uint8 IORead(add24 address)
    {
        switch (address)
        {
        case 0x4214: //RDDIVL
            return RDDIVL;
            break;
        case 0x4215: //RDDIVH
            return RDDIVH;
            break;
        case 0x4216: //RDMPYL
            return RDMPYL;
            break;
        case 0x4217: //RDMPYH
            return RDMPYH;
            break;
        
        default:
        return 0;
            break;
        }
        return 0;
    }

};