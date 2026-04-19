#include <bits/stdc++.h>
using namespace std;

typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int add24;

using DMAReadByteFunc_t = uint8 (*)(add24);
using DMAWriteByteFunc_t = void (*)(add24, uint8);

struct DMARegs
{
    uint8 DMAPARAM[8];  // 0x43X0 Parameters
    uint8 DMABBADD[8];  // 0x43X1 B bus address
    uint8 DMAABADDL[8]; // 0x43X2 A bus address low
    uint8 DMAABADDH[8]; // 0x43X3 A bus address high
    uint8 DMAABADDB[8]; // 0x43X4 A bus address bank1

    uint8 DMABYTESL[8]; // 0x43X5 number of bytes to transfer low OR Data address low for HDMA
    uint8 DMABYTESH[8]; // 0x43X6 number of bytes to transfer high OR Data address high for HDMA
    uint8 HDMAADDB[8];  // 0x43X7 Data address bank for HDMA

    uint8 DMAA2ADDL[8]; // 0x43X8 table address of a-bus by dma low
    uint8 DMAA2ADDH[8]; // 0x43X9 table address of a-bus by dma high

    uint8 HDMALINES[8]; // 0x43Xa number of lines to be transfrered by HDA
};

class DMA
{
    DMAReadByteFunc_t ReadByte;
    DMAWriteByteFunc_t WriteByte;

public:
    DMARegs regs;
    uint8 MDMAEN; // What channels to start immediately, each flag is cleared after finishing
    uint8 HDMAEN; // What channels to start after HBlank
    int transferred[8] = {0};
    bool dmaActive = false;

    // HDMA
    add24 tableAddress[8] = {0};
    add24 tablePtr[8] = {0};
    bool hdmaEn[8] = {0};
    add24 hdmaDestAddr[8] = {0};
    bool hdmaDir[8] = {0}; // For now we will ignore it, since almost every games uses HDAM as A->B
    bool absAddr[8] = {0};
    uint8 hdmaType[8] = {0};
    bool hdmaCont[8] = {0};
    uint8 hdmaCount[8] = {0};


    uint8 IORead(add24 address)
    {
        switch (address)
        {

        case 0x4300:
            return this->regs.DMAPARAM[0];
            break;
        case 0x4310:
            return this->regs.DMAPARAM[1];
            break;
        case 0x4320:
            return this->regs.DMAPARAM[2];
            break;
        case 0x4330:
            return this->regs.DMAPARAM[3];
            break;
        case 0x4340:
            return this->regs.DMAPARAM[4];
            break;
        case 0x4350:
            return this->regs.DMAPARAM[5];
            break;
        case 0x4360:
            return this->regs.DMAPARAM[6];
            break;
        case 0x4370:
            return this->regs.DMAPARAM[7];
            break;

        // 0x43X1 - DMAX B bus add
        case 0x4301:
            return this->regs.DMABBADD[0];
            break;
        case 0x4311:
            return this->regs.DMABBADD[1];
            break;
        case 0x4321:
            return this->regs.DMABBADD[2];
            break;
        case 0x4331:
            return this->regs.DMABBADD[3];
            break;
        case 0x4341:
            return this->regs.DMABBADD[4];
            break;
        case 0x4351:
            return this->regs.DMABBADD[5];
            break;
        case 0x4361:
            return this->regs.DMABBADD[6];
            break;
        case 0x4371:
            return this->regs.DMABBADD[7];
            break;

        // 0x43X2 - DMAX A bus add low
        case 0x4302:
            return this->regs.DMAABADDL[0];
            break;
        case 0x4312:
            return this->regs.DMAABADDL[1];
            break;
        case 0x4322:
            return this->regs.DMAABADDL[2];
            break;
        case 0x4332:
            return this->regs.DMAABADDL[3];
            break;
        case 0x4342:
            return this->regs.DMAABADDL[4];
            break;
        case 0x4352:
            return this->regs.DMAABADDL[5];
            break;
        case 0x4362:
            return this->regs.DMAABADDL[6];
            break;
        case 0x4372:
            return this->regs.DMAABADDL[7];
            break;

        // 0x43X3 - DMAX A bus add high
        case 0x4303:
            return this->regs.DMAABADDH[0];
            break;
        case 0x4313:
            return this->regs.DMAABADDH[1];
            break;
        case 0x4323:
            return this->regs.DMAABADDH[2];
            break;
        case 0x4333:
            return this->regs.DMAABADDH[3];
            break;
        case 0x4343:
            return this->regs.DMAABADDH[4];
            break;
        case 0x4353:
            return this->regs.DMAABADDH[5];
            break;
        case 0x4363:
            return this->regs.DMAABADDH[6];
            break;
        case 0x4373:
            return this->regs.DMAABADDH[7];
            break;

        // 0x43X4 - A bus add bank
        case 0x4304:
            return this->regs.DMAABADDB[0];
            break;
        case 0x4314:
            return this->regs.DMAABADDB[1];
            break;
        case 0x4324:
            return this->regs.DMAABADDB[2];
            break;
        case 0x4334:
            return this->regs.DMAABADDB[3];
            break;
        case 0x4344:
            return this->regs.DMAABADDB[4];
            break;
        case 0x4354:
            return this->regs.DMAABADDB[5];
            break;
        case 0x4364:
            return this->regs.DMAABADDB[6];
            break;
        case 0x4374:
            return this->regs.DMAABADDB[7];
            break;

        // 0x43X5 number of bytes low
        case 0x4305:
            return this->regs.DMABYTESL[0];
            break;
        case 0x4315:
            return this->regs.DMABYTESL[1];
            break;
        case 0x4325:
            return this->regs.DMABYTESL[2];
            break;
        case 0x4335:
            return this->regs.DMABYTESL[3];
            break;
        case 0x4345:
            return this->regs.DMABYTESL[4];
            break;
        case 0x4355:
            return this->regs.DMABYTESL[5];
            break;
        case 0x4365:
            return this->regs.DMABYTESL[6];
            break;
        case 0x4375:
            return this->regs.DMABYTESL[7];
            break;

        // 0x43X6 - DMAX number of bytes high
        case 0x4306:
            return this->regs.DMABYTESH[0];
            break;
        case 0x4316:
            return this->regs.DMABYTESH[1];
            break;
        case 0x4326:
            return this->regs.DMABYTESH[2];
            break;
        case 0x4336:
            return this->regs.DMABYTESH[3];
            break;
        case 0x4346:
            return this->regs.DMABYTESH[4];
            break;
        case 0x4356:
            return this->regs.DMABYTESH[5];
            break;
        case 0x4366:
            return this->regs.DMABYTESH[6];
            break;
        case 0x4376:
            return this->regs.DMABYTESH[7];
            break;

        // 0x43X7 - DMAX HDMA Data add bank
        case 0x4307:
            return this->regs.HDMAADDB[0];
            break;
        case 0x4317:
            return this->regs.HDMAADDB[1];
            break;
        case 0x4327:
            return this->regs.HDMAADDB[2];
            break;
        case 0x4337:
            return this->regs.HDMAADDB[3];
            break;
        case 0x4347:
            return this->regs.HDMAADDB[4];
            break;
        case 0x4357:
            return this->regs.HDMAADDB[5];
            break;
        case 0x4367:
            return this->regs.HDMAADDB[6];
            break;
        case 0x4377:
            return this->regs.HDMAADDB[7];
            break;

        // 0x43X8 - DMAX table add for a bus low
        case 0x4308:
            return this->regs.DMAA2ADDL[0];
            break;
        case 0x4318:
            return this->regs.DMAA2ADDL[1];
            break;
        case 0x4328:
            return this->regs.DMAA2ADDL[2];
            break;
        case 0x4338:
            return this->regs.DMAA2ADDL[3];
            break;
        case 0x4348:
            return this->regs.DMAA2ADDL[4];
            break;
        case 0x4358:
            return this->regs.DMAA2ADDL[5];
            break;
        case 0x4368:
            return this->regs.DMAA2ADDL[6];
            break;
        case 0x4378:
            return this->regs.DMAA2ADDL[7];
            break;

        // 0x43X9 - DMAX table add for a bus high
        case 0x4309:
            return this->regs.DMAA2ADDH[0];
            break;
        case 0x4319:
            return this->regs.DMAA2ADDH[1];
            break;
        case 0x4329:
            return this->regs.DMAA2ADDH[2];
            break;
        case 0x4339:
            return this->regs.DMAA2ADDH[3];
            break;
        case 0x4349:
            return this->regs.DMAA2ADDH[4];
            break;
        case 0x4359:
            return this->regs.DMAA2ADDH[5];
            break;
        case 0x4369:
            return this->regs.DMAA2ADDH[6];
            break;
        case 0x4379:
            return this->regs.DMAA2ADDH[7];
            break;

        // 0x43Xa - DMAX HDMA lines
        case 0x430a:
            return this->regs.HDMALINES[0];
            break;
        case 0x431a:
            return this->regs.HDMALINES[1];
            break;
        case 0x432a:
            return this->regs.HDMALINES[2];
            break;
        case 0x433a:
            return this->regs.HDMALINES[3];
            break;
        case 0x434a:
            return this->regs.HDMALINES[4];
            break;
        case 0x435a:
            return this->regs.HDMALINES[5];
            break;
        case 0x436a:
            return this->regs.HDMALINES[6];
            break;
        case 0x437a:
            return this->regs.HDMALINES[7];
            break;

        default:
            return 0;
            break;
        }
    }
    void IOWrite(add24 address, uint8 data)
    {
        switch (address)
        {

        case 0x420b: // MDMAEN

            this->MDMAEN = data;
            break;

        case 0x420c: // HDMAEN

            this->HDMAEN = data;
            break;
        case 0x4300:
            this->regs.DMAPARAM[0] = data;
            break;
        case 0x4310:
            this->regs.DMAPARAM[1] = data;
            break;
        case 0x4320:
            this->regs.DMAPARAM[2] = data;
            break;
        case 0x4330:
            this->regs.DMAPARAM[3] = data;
            break;
        case 0x4340:
            this->regs.DMAPARAM[4] = data;
            break;
        case 0x4350:
            this->regs.DMAPARAM[5] = data;
            break;
        case 0x4360:
            this->regs.DMAPARAM[6] = data;
            break;
        case 0x4370:
            this->regs.DMAPARAM[7] = data;
            break;

        // 0x43X1 - DMAX B bus add
        case 0x4301:
            this->regs.DMABBADD[0] = data;
            break;
        case 0x4311:
            this->regs.DMABBADD[1] = data;
            break;
        case 0x4321:
            this->regs.DMABBADD[2] = data;
            break;
        case 0x4331:
            this->regs.DMABBADD[3] = data;
            break;
        case 0x4341:
            this->regs.DMABBADD[4] = data;
            break;
        case 0x4351:
            this->regs.DMABBADD[5] = data;
            break;
        case 0x4361:
            this->regs.DMABBADD[6] = data;
            break;
        case 0x4371:
            this->regs.DMABBADD[7] = data;
            break;

        // 0x43X2 - DMAX A bus add low
        case 0x4302:
            this->regs.DMAABADDL[0] = data;
            break;
        case 0x4312:
            this->regs.DMAABADDL[1] = data;
            break;
        case 0x4322:
            this->regs.DMAABADDL[2] = data;
            break;
        case 0x4332:
            this->regs.DMAABADDL[3] = data;
            break;
        case 0x4342:
            this->regs.DMAABADDL[4] = data;
            break;
        case 0x4352:
            this->regs.DMAABADDL[5] = data;
            break;
        case 0x4362:
            this->regs.DMAABADDL[6] = data;
            break;
        case 0x4372:
            this->regs.DMAABADDL[7] = data;
            break;

        // 0x43X3 - DMAX A bus add high
        case 0x4303:
            this->regs.DMAABADDH[0] = data;
            break;
        case 0x4313:
            this->regs.DMAABADDH[1] = data;
            break;
        case 0x4323:
            this->regs.DMAABADDH[2] = data;
            break;
        case 0x4333:
            this->regs.DMAABADDH[3] = data;
            break;
        case 0x4343:
            this->regs.DMAABADDH[4] = data;
            break;
        case 0x4353:
            this->regs.DMAABADDH[5] = data;
            break;
        case 0x4363:
            this->regs.DMAABADDH[6] = data;
            break;
        case 0x4373:
            this->regs.DMAABADDH[7] = data;
            break;

        // 0x43X4 - A bus add bank
        case 0x4304:
            this->regs.DMAABADDB[0] = data;
            break;
        case 0x4314:
            this->regs.DMAABADDB[1] = data;
            break;
        case 0x4324:
            this->regs.DMAABADDB[2] = data;
            break;
        case 0x4334:
            this->regs.DMAABADDB[3] = data;
            break;
        case 0x4344:
            this->regs.DMAABADDB[4] = data;
            break;
        case 0x4354:
            this->regs.DMAABADDB[5] = data;
            break;
        case 0x4364:
            this->regs.DMAABADDB[6] = data;
            break;
        case 0x4374:
            this->regs.DMAABADDB[7] = data;
            break;

        // 0x43X5 number of bytes low
        case 0x4305:
            this->regs.DMABYTESL[0] = data;
            break;
        case 0x4315:
            this->regs.DMABYTESL[1] = data;
            break;
        case 0x4325:
            this->regs.DMABYTESL[2] = data;
            break;
        case 0x4335:
            this->regs.DMABYTESL[3] = data;
            break;
        case 0x4345:
            this->regs.DMABYTESL[4] = data;
            break;
        case 0x4355:
            this->regs.DMABYTESL[5] = data;
            break;
        case 0x4365:
            this->regs.DMABYTESL[6] = data;
            break;
        case 0x4375:
            this->regs.DMABYTESL[7] = data;
            break;

        // 0x43X6 - DMAX number of bytes high
        case 0x4306:
            this->regs.DMABYTESH[0] = data;
            break;
        case 0x4316:
            this->regs.DMABYTESH[1] = data;
            break;
        case 0x4326:
            this->regs.DMABYTESH[2] = data;
            break;
        case 0x4336:
            this->regs.DMABYTESH[3] = data;
            break;
        case 0x4346:
            this->regs.DMABYTESH[4] = data;
            break;
        case 0x4356:
            this->regs.DMABYTESH[5] = data;
            break;
        case 0x4366:
            this->regs.DMABYTESH[6] = data;
            break;
        case 0x4376:
            this->regs.DMABYTESH[7] = data;
            break;

        // 0x43X7 - DMAX HDMA Data add bank
        case 0x4307:
            this->regs.HDMAADDB[0] = data;
            break;
        case 0x4317:
            this->regs.HDMAADDB[1] = data;
            break;
        case 0x4327:
            this->regs.HDMAADDB[2] = data;
            break;
        case 0x4337:
            this->regs.HDMAADDB[3] = data;
            break;
        case 0x4347:
            this->regs.HDMAADDB[4] = data;
            break;
        case 0x4357:
            this->regs.HDMAADDB[5] = data;
            break;
        case 0x4367:
            this->regs.HDMAADDB[6] = data;
            break;
        case 0x4377:
            this->regs.HDMAADDB[7] = data;
            break;

        // 0x43X8 - DMAX table add for a bus low
        case 0x4308:
            this->regs.DMAA2ADDL[0] = data;
            break;
        case 0x4318:
            this->regs.DMAA2ADDL[1] = data;
            break;
        case 0x4328:
            this->regs.DMAA2ADDL[2] = data;
            break;
        case 0x4338:
            this->regs.DMAA2ADDL[3] = data;
            break;
        case 0x4348:
            this->regs.DMAA2ADDL[4] = data;
            break;
        case 0x4358:
            this->regs.DMAA2ADDL[5] = data;
            break;
        case 0x4368:
            this->regs.DMAA2ADDL[6] = data;
            break;
        case 0x4378:
            this->regs.DMAA2ADDL[7] = data;
            break;

        // 0x43X9 - DMAX table add for a bus high
        case 0x4309:
            this->regs.DMAA2ADDH[0] = data;
            break;
        case 0x4319:
            this->regs.DMAA2ADDH[1] = data;
            break;
        case 0x4329:
            this->regs.DMAA2ADDH[2] = data;
            break;
        case 0x4339:
            this->regs.DMAA2ADDH[3] = data;
            break;
        case 0x4349:
            this->regs.DMAA2ADDH[4] = data;
            break;
        case 0x4359:
            this->regs.DMAA2ADDH[5] = data;
            break;
        case 0x4369:
            this->regs.DMAA2ADDH[6] = data;
            break;
        case 0x4379:
            this->regs.DMAA2ADDH[7] = data;
            break;

        // 0x43Xa - DMAX HDMA lines
        case 0x430a:
            this->regs.HDMALINES[0] = data;
            break;
        case 0x431a:
            this->regs.HDMALINES[1] = data;
            break;
        case 0x432a:
            this->regs.HDMALINES[2] = data;
            break;
        case 0x433a:
            this->regs.HDMALINES[3] = data;
            break;
        case 0x434a:
            this->regs.HDMALINES[4] = data;
            break;
        case 0x435a:
            this->regs.HDMALINES[5] = data;
            break;
        case 0x436a:
            this->regs.HDMALINES[6] = data;
            break;
        case 0x437a:
            this->regs.HDMALINES[7] = data;
            break;

        default:
            break;
        }
    }

    DMA(DMAReadByteFunc_t readfunc, DMAWriteByteFunc_t writefunc)
    {
        this->ReadByte = readfunc;
        this->WriteByte = writefunc;
    }

    // Priority :
    //       HDMA > DMA > CPU
    //       DMA0 > DMA1 > .... > DMA7

    void initializeHDMA()
    {
        for (int i = 0; i < 8; i++)
        {
            hdmaEn[i] = (1 << i) & HDMAEN;
            if (!hdmaEn[i])
                continue;

            hdmaType[i] = regs.DMAPARAM[i] & 0b00000111;
            absAddr[i] = !(regs.DMAPARAM[i] & 0b01000000);
            hdmaDestAddr[i] = 0x002100 | regs.DMABBADD[i];
            tableAddress[i] = regs.DMAABADDB[i] << 16 | regs.DMAABADDH[i] << 8 | regs.DMAABADDL[i];
            tablePtr[i] = tableAddress[i];
            uint16 line = ReadByte(tablePtr[i]);
            if (line == 0x00)
            {
                hdmaEn[i] = false;
            }
            else
            {
                hdmaCount[i] = line & 0b01111111;
                if (hdmaCount[i] == 0x00)
                    hdmaCount[i] = 128;
                hdmaCont[i] = line & 0b10000000;

          
                tablePtr[i] = 0;
            }

            //cout << "========== HDMA[" << i << "] Init ==========" << endl;
            //cout << "table header : " << hex << line << endl;
            //cout << "table addr : " << hex << tableAddress[i] << endl;
            //cout << "type : " << dec << (uint16)hdmaType[i] << endl;
            //cout << "abs/indir : " << (absAddr[i] ? "abs" : "indir") << endl;
            //cout << "cont : " << (hdmaCont[i] ? "y" : "n") << endl;
            //cout << "count : " << hex << (uint16)hdmaCount[i] << endl;
            //cout << "b addr : " << hex << hdmaDestAddr[i] << endl;
        }
    }
    void step(bool hBlank)
    {
        // Check for active HDMA transactions
        // Do the active HDMA transactions
        // Check for active Normal DMA transactions
        // Do the active Normal DMA transactions

        // Regs:
        //  Param :
        //       0,1,2 :
        //       3 : 1 -> Fixed address, 0 -> Automatically change address
        //       4 : 1 -> dec , 0 -> inc
        //       5 : unused
        //       6 : (HDMA ONLY) 1 -> INDIRECT , 0 -> ABSOLUTE
        //       7 : Dir, 1 -> B to A , 0 -> A to B
        //

        // BBADD : B-bus address, 0021XX will become the B-bus address

        // ABADD : A-bus address

        // DMABYTESL DMABYTESH and HDMAADDB
        // General DMA : holds the number of bytes to transfer
        // HDMA : if using indirect mode, the inderct address will be placed here

        // DMAA2ADDL: (not sure)
        // General DMA : Not used.
        // HDMA : IDK really

        // HDMALINES : number of lines for HDMA

        // For now, we preform 1 transfer/cycle

        dmaActive = (MDMAEN) || (hBlank && HDMAEN);

        if (hBlank)
        {

            for (int i = 0; i < 8; i++)
            {
                if (!hdmaEn[i])
                    continue;

                uint8 data = 0;
                add24 dataAddr;

                if (absAddr[i])
                {
                    dataAddr = (tablePtr[i] + tableAddress[i] + 1) & 0x00FFFF;
                    dataAddr |= (regs.DMAABADDB[i] << 16);
                }
                else
                {
                    add24 addL = (tableAddress[i] + 1) & 0x00FFFF;
                    addL |= (regs.DMAABADDB[i] << 16);
                    add24 addH = (tableAddress[i] + 2) & 0x00FFFF;
                    addH |= (regs.DMAABADDB[i] << 16);

                    //cout << "indir add add : " << hex << (uint16)addL << endl;
                    uint16 addr = ReadByte(addL) | (ReadByte(addH) << 8);
                    //cout << "indir add : " << hex << addr << endl;
                    dataAddr = (addr + tablePtr[i]) & 0x00FFFF;
                    dataAddr |= (regs.HDMAADDB[i] << 16);
                }

                switch (hdmaType[i])
                {
                case 0:
                {
                    data = ReadByte(dataAddr);
                    WriteByte(hdmaDestAddr[i], data);
                    tablePtr[i]++;
                    break;
                }
                case 1:
                {

                    data = ReadByte(dataAddr);
                    WriteByte(hdmaDestAddr[i], data);
                    tablePtr[i]++;
                    data = ReadByte(dataAddr + 1);
                    WriteByte(hdmaDestAddr[i] + 1, data);
                    tablePtr[i]++;
                    break;
                }
                case 6:
                case 2:
                {

                    data = ReadByte(dataAddr);
                    WriteByte(hdmaDestAddr[i], data);
                    tablePtr[i]++;
                    data = ReadByte(dataAddr + 1);
                    WriteByte(hdmaDestAddr[i], data);
                    tablePtr[i]++;
                    break;
                }
                case 7:
                case 3:
                {

                    data = ReadByte(dataAddr);

                    WriteByte(hdmaDestAddr[i], data);
                    tablePtr[i]++;
                    data = ReadByte(dataAddr + 1);
                    WriteByte(hdmaDestAddr[i], data);
                    tablePtr[i]++;
                    data = ReadByte(dataAddr + 2);
                    WriteByte(hdmaDestAddr[i] + 1, data);
                    tablePtr[i]++;
                    data = ReadByte(dataAddr + 3);
                    WriteByte(hdmaDestAddr[i] + 1, data);
                    tablePtr[i]++;
                    break;
                }
                case 4:
                {

                    data = ReadByte(dataAddr);
                    WriteByte(hdmaDestAddr[i], data);
                    tablePtr[i]++;
                    data = ReadByte(dataAddr + 1);
                    WriteByte(hdmaDestAddr[i] + 1, data);
                    tablePtr[i]++;
                    data = ReadByte(dataAddr + 2);
                    WriteByte(hdmaDestAddr[i] + 2, data);
                    tablePtr[i]++;
                    data = ReadByte(dataAddr + 3);
                    WriteByte(hdmaDestAddr[i] + 3, data);
                    tablePtr[i]++;
                    break;
                }
                case 5:
                {
                    data = ReadByte(dataAddr);

                    WriteByte(hdmaDestAddr[i], data);
                    tablePtr[i]++;
                    data = ReadByte(dataAddr + 1);
                    WriteByte(hdmaDestAddr[i] + 1, data);
                    tablePtr[i]++;
                    data = ReadByte(dataAddr + 2);
                    WriteByte(hdmaDestAddr[i], data);
                    tablePtr[i]++;
                    data = ReadByte(dataAddr + 3);
                    WriteByte(hdmaDestAddr[i] + 1, data);
                    tablePtr[i]++;
                    break;
                }

                default:
                    break;
                }

                hdmaCount[i] -= 1;
             

                if (hdmaCount[i] == 0)
                {
                    uint8 line;
                    if (absAddr[i])
                    {
                        tableAddress[i] += tablePtr[i] + 1;
                        tableAddress[i] &= 0x00FFFF;
                        tableAddress[i] |= regs.DMAABADDB[i] << 16;
                    }
                    else
                    {
                        tableAddress[i] += 3;
                        tableAddress[i] &= 0x00FFFF;
                        tableAddress[i] |= regs.DMAABADDB[i] << 16;
                    }

                    line = ReadByte(tableAddress[i]);
                    if (line == 0x00) // End of table, Done
                    {
                        hdmaEn[i] = false;
                    }
                    else
                    {
                        hdmaCount[i] = line & 0b01111111;
                        if (hdmaCount[i] == 0x00)
                            hdmaCount[i] = 128;
                        hdmaCont[i] = line & 0b10000000;
                        tablePtr[i] = 0;
                    }
                }

                // WHAT SHOULD THIS BE?!
                if (!hdmaCont[i])
                {
                    tablePtr[i] = 0;
                }

                //cout << "-=-=-=-=-=-=-=-=-=HDMA[" << i << "]=-=-=-=-=-=-=-=-=-=" << endl;
                //cout << "type : " << dec << (uint16)hdmaType[i] << endl;
                //cout << "abs/indir : " << (absAddr[i] ? "abs" : "indir") << endl;
                //cout << "rep : " << (hdmaCont[i] ? "y" : "n") << endl;
                //cout << "b addr : " << hex << hdmaDestAddr[i] << endl;
                //cout << "a table : " << hex << tableAddress[i] << endl;
                //cout << "a bank : " << hex << (uint16)regs.DMAABADDB[i] << endl;
                //cout << "indir bank : " << hex << (uint16)regs.HDMAADDB[i] << endl;
                //cout << "a tab ptr : " << dec << tablePtr[i] << endl;
                //cout << "data addr : " << hex << dataAddr << endl;
                //cout << "counter : " << dec << (uint16)hdmaCount[i] << endl;
            
            }
        }
        else
        {

            int i = 0;
            for (; i < 8; i++)
                if ((1 << i) & MDMAEN)
                    break;
            if (i == 8)
                return;

            uint8 type = regs.DMAPARAM[i] & 0b00000111;
            bool fixedAddr = (1 << 3) & regs.DMAPARAM[i];
            bool decAddr = (1 << 4) & regs.DMAPARAM[i];
            bool dir = (1 << 7) & regs.DMAPARAM[i];

            add24 bAddr = 0x002100 | regs.DMABBADD[i];
            add24 aAddr = regs.DMAABADDH[i] << 8 | regs.DMAABADDL[i];
            uint16 num = regs.DMABYTESH[i] << 8 | regs.DMABYTESL[i];
            if (num == 0)
                num = 0x10000;

            //cout << "-=-=-=-=-=-=-=-=-=DMA[" << i << "]=-=-=-=-=-=-=-=-=-=" << endl;
            //cout << "type : " << dec << (uint16)type << endl;
            //cout << "fixed : " << (fixedAddr ? "fixed" : "change") << endl;
            //cout << "inc : " << (decAddr ? "-" : "+") << endl;
            //cout << "dir : " << (dir ? "B->A" : "A->B") << endl;
            //cout << "b addr : " << hex << bAddr << endl;
            //cout << "a addr : " << hex << aAddr << endl;
            //cout << "bytes : " << dec << num << endl;
            //cout << "done : " << dec << transferred[i] << endl;
            // if(bAddr == 0x2116 )
            //     cin.get();

            add24 offset = type == 0 ? 0 : type == 1 ? transferred[i] % 2
                                       : type == 2   ? 0
                                       : type == 3   ? (transferred[i] >> 1) % 2
                                       : type == 4   ? transferred[i] % 4
                                       : type == 5   ? transferred[i] % 2
                                       : type == 6   ? 0
                                       : type == 7   ? (transferred[i] >> 1) % 2
                                                     : 0;
            add24 bEffAdd = (bAddr + offset);
            add24 aEffAdd = (regs.DMAABADDB[i] << 16) | aAddr;
            if (dir) // B to A
            {

                uint8 t = ReadByte(bEffAdd);
                // cout << "effective Addr : " << hex << aAddr << endl;
                // cout << "data : " << hex << (uint16)t << endl;
                WriteByte(aEffAdd, t);
            }
            else // A to B
            {
                uint8 t = ReadByte(aEffAdd);
                // cout << "effective Addr : " << hex << bAddr + offset << endl;
                // cout << "data : " << hex <<(uint16) t << endl;
                WriteByte(bEffAdd, t);
            }

            if (!fixedAddr)
            {
                if (decAddr)
                {
                    aAddr--;
                }
                else
                {
                    aAddr++;
                }

                // Does the address wrap?aAddr = regs.DMAABADDH[i] << 8 | regs.DMAABADDL[i];
                // regs.DMAABADDB[i] = (aAddr & 0xFF0000) >> 16;
                regs.DMAABADDH[i] = (aAddr & 0x00FF00) >> 8;
                regs.DMAABADDL[i] = (aAddr & 0x0000FF) >> 0;
            }

            // Increase first, check after!!!
            transferred[i]++;
            if (transferred[i] >= num)
            {
                transferred[i] = 0;
                // cin.get();
                MDMAEN = MDMAEN & (~(1 << i));
            }
        }
    }
};