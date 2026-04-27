#include <bits/stdc++.h>
using namespace std;

typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int add24;

using DMAReadByteFunc_t = uint8 (*)(add24);
using DMAWriteByteFunc_t = void (*)(add24, uint8);


class DMA
{
    DMAReadByteFunc_t ReadByte;
    DMAWriteByteFunc_t WriteByte;

public:
    uint8 MDMAEN; // What channels to start immediately, each flag is cleared after finishing
    uint8 HDMAEN; // What channels to start after HBlank
    bool dmaActive = false;

    uint16 dmaAAddr[8] = {0};
    uint8 dmaAAddrBank[8] = {0};
    add24 dmaBAddr[8] = {0};
    uint16 dmaDataPtr[8] = {0};
    uint8 dmaIndirBank[8] = {0};
    uint16 dmaTablePtr[8] = {0};
    uint8 dmaUnused[8] = {0};
    bool dmaDir[8] = {0};
    bool dmaFixedAddr[8] = {0};
    bool dmaDecAddr[8] = {0};
    uint8 dmaType[8] = {0};
    bool dmaIndir[8] = {0};
    bool dmaEn[8] = {0};
    bool hdmaEn[8] = {0};
    bool dmaCont[8] = {0};
    uint8 dmaLineCnt[8] = {0};
    bool hdmaDoTransfer[8] = {0};

    uint8 state = 0;

    uint8 IORead(add24 address)
    {
        if ((address & 0xFF00) == 0x4300)
        {
            uint8 chNum = (address & 0x00F0) >> 4;
            uint8 chReg = (address & 0x000F);

            switch (chReg)
            {

            case 0x0: // 0x43_0
            {
                uint8 t = 0;
                t |= dmaType[chNum]&0b00000111;
                t |= dmaFixedAddr[chNum]<<3;
                t |= dmaDecAddr[chNum]<<4;
                t |= dmaIndir[chNum]<<6;
                t |= dmaDir[chNum]<<7;
                return t;
                break;
            }
            case 0x1: // 0x43_1
                return dmaBAddr[chNum];
                break;

            case 0x2: // 0x43_2 ABus/Table Address Low
                return dmaAAddr[chNum];
                break;
            case 0x3: // 0x43_3 ABus/Table Address High
                return dmaAAddr[chNum] >> 8;
                break;
            case 0x4: // 0x43_4 ABus/Table Address Bank
                return dmaAAddr[chNum] >> 16;
                break;

            case 0x5: // 0x43_5 IndirAddr/Counter Low
                return dmaDataPtr[chNum];
                break;
            case 0x6: // 0x43_6 IndirAddr/Counter High
                return dmaDataPtr[chNum] >> 8;
                break;
            case 0x7: // 0x43_7 IndirAddr Bank
                return dmaIndirBank[chNum];
                break;

            case 0x8: // 0x43_8 TableAddr Low
                return dmaTablePtr[chNum];
                break;
            case 0x9: // 0x43_9 TableAddr High
                return dmaTablePtr[chNum] >> 8;
                break;

          
            case 0xa: // 0x43_a Table Header (Rep + Line Count)
                return dmaLineCnt[chNum];
                break;

            case 0xf: // 0x43_f Mirror of 0x43_b
            case 0xb: // 0x43_b Unused can be written to or read from
                return dmaUnused[chNum];
                break;

            case 0xc: // 0x43_c OPENBUS
                // WriteByte(0xFFFFFF, data);
                break;
            case 0xd: // 0x43_d OPENBUS
                // WriteByte(0xFFFFFF, data);
                break;
            case 0xe: // 0x43_e OPENBUS
                // WriteByte(0xFFFFFF, data);
                break;

            default: // OPENBUS
                // WriteByte(0xFFFFFF, data);
                break;
            }
        }
        else
        {

            switch (address)
            {

            case 0x420b: // MDMAEN
            {
                uint8 t= 0;
                for (int i = 0; i < 8; i++)
                    t |= (dmaEn[i] << i);
                return t;
                break;
            }

            case 0x420c: // HDMAEN
                return HDMAEN;

                break;

            default: // OPENBUS
                // WriteByte(, data);
                break;
            }
        }
        return 0;
    }
    void IOWrite(add24 address, uint8 data)
    {

        if ((address & 0xFF00) == 0x4300)
        {
            uint8 chNum = (address & 0x00F0) >> 4;
            uint8 chReg = (address & 0x000F);

            switch (chReg)
            {

            case 0x0: // 0x43_0
                dmaType[chNum] = data & 0b00000111;
                dmaFixedAddr[chNum] = data & 0b00001000;
                dmaDecAddr[chNum] = data & 0b00010000;
                dmaIndir[chNum] = data & 0b01000000;
                dmaDir[chNum] = data & 0b10000000;
                break;

            case 0x1: // 0x43_1
                dmaBAddr[chNum] = 0x2100 | data;
                break;

            case 0x2: // 0x43_2 ABus/Table Address Low
                dmaAAddr[chNum] = (dmaAAddr[chNum] & 0xFF00) | data;
                break;
            case 0x3: // 0x43_3 ABus/Table Address High
                dmaAAddr[chNum] = (dmaAAddr[chNum] & 0x00FF) | (data << 8);
                break;
            case 0x4: // 0x43_4 ABus/Table Address Bank
                dmaAAddrBank[chNum] = data;
                break;

            case 0x5: // 0x43_5 IndirAddr/Counter Low
                dmaDataPtr[chNum] = (dmaDataPtr[chNum] & 0xFF00) | data;
                break;
            case 0x6: // 0x43_6 IndirAddr/Counter High
                dmaDataPtr[chNum] = (dmaDataPtr[chNum] & 0x00FF) | (data << 8);
                break;
            case 0x7: // 0x43_7 IndirAddr Bank
                dmaIndirBank[chNum] = data;
                break;

            case 0x8: // 0x43_8 TableAddr Low
                dmaTablePtr[chNum] = (dmaTablePtr[chNum] & 0xFF00) | data;
                break;
            case 0x9: // 0x43_9 TableAddr High
                dmaTablePtr[chNum] = (dmaTablePtr[chNum] & 0x00FF) | (data << 8);
                break;

            // TODO : Make sure the following is correct.
            case 0xa: // 0x43_a Table Header (Rep + Line Count)
                dmaCont[chNum] = data & 0b10000000;
                dmaLineCnt[chNum] = data & 0b11111111;
                break;

            case 0xf: // 0x43_f Mirror of 0x43_b
            case 0xb: // 0x43_b Unused can be written to or read from
                dmaUnused[chNum] = data;
                break;

            case 0xc: // 0x43_c OPENBUS
                // WriteByte(0xFFFFFF, data);
                break;
            case 0xd: // 0x43_d OPENBUS
                // WriteByte(0xFFFFFF, data);
                break;
            case 0xe: // 0x43_e OPENBUS
                // WriteByte(0xFFFFFF, data);
                break;

            default: // OPENBUS
                // WriteByte(0xFFFFFF, data);
                break;
            }
        }
        else
        {

            switch (address)
            {

            case 0x420b: // MDMAEN

                for (int i = 0; i < 8; i++)
                    dmaEn[i] = (1 << i) & data;
                break;

            case 0x420c: // HDMAEN
                HDMAEN = data;

                break;

            default: // OPENBUS
                // WriteByte(, data);
                break;
            }
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
            hdmaEn[i] = (1 << i) & HDMAEN;
        for (int i = 0; i < 8; i++)
        {
            if (!hdmaEn[i])
                continue;

            dmaTablePtr[i] = dmaAAddr[i]; // Current table addres = Start table address
            uint8 line = ReadByte((dmaAAddrBank[i] << 16) | dmaTablePtr[i]);
            if (line == 0x00)
            {
                hdmaEn[i] = false;
            }
            else
            {
                
                if (line == 0x80)
                {
                    dmaLineCnt[i] = 128;
                    dmaCont[i] = false;
                }
                else
                {
                    dmaLineCnt[i] = line & 0b11111111;
                    dmaCont[i] = line & 0b10000000;
                }
                    
                //dmaLineCnt[i] = line;

                hdmaDoTransfer[i] = true;
            }

            if (dmaIndir[i])
            {
                dmaTablePtr[i]++;
                add24 addL = (dmaTablePtr[i]) & 0xFFFF;
                addL |= (dmaAAddrBank[i] << 16);
                dmaTablePtr[i]++;
                add24 addH = (dmaTablePtr[i]) & 0xFFFF;
                addH |= (dmaAAddrBank[i] << 16);
                dmaDataPtr[i] = ReadByte(addL) | (ReadByte(addH) << 8);
                dmaTablePtr[i]++;
            }
            else
            {
                dmaTablePtr[i]++;
            }
            // cout << "========== HDMA[" << i << "] Init ==========" << endl;
            // cout << "table header : " << hex << (uint16)line << endl;
            // cout << "table addr : " << hex << ((dmaAAddrBank[i] << 16) | dmaAAddr[i]) << endl;
            // cout << "table ptr : " << hex << (dmaTablePtr[i]) << endl;
            // cout << "type : " << dec << (uint16)dmaType[i] << endl;
            // cout << "abs/indir : " << (dmaIndir[i] ? "indir" : "abs") << endl;
            // cout << "cont : " << (dmaCont[i] ? "y" : "n") << endl;
            // cout << "count : " << hex << (uint16)dmaLineCnt[i] << endl;
            // cout << "b addr : " << hex << dmaBAddr[i] << endl;
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

        dmaActive = false;
        for (int i = 0; i < 8; i++)
        {
            dmaActive = dmaActive || (hBlank && hdmaEn[i]) || dmaEn[i];
        }

        if (hBlank)
        {

            for (int i = 0; i < 8; i++)
            {

                if (!hdmaEn[i])
                    continue;

                if (hdmaDoTransfer[i])
                {

                    add24 dataAddr;
                    uint8 bytesTrf = 0;
                    if (dmaIndir[i])
                    {
                        dataAddr = dmaDataPtr[i];
                        dataAddr |= (dmaIndirBank[i] << 16);
                    }
                    else
                    {
                        dataAddr = dmaTablePtr[i];
                        dataAddr |= (dmaAAddrBank[i] << 16);
                    }

                    uint8 data;
                    switch (dmaType[i])
                    {
                    case 0:
                    {
                        data = ReadByte(dataAddr);
                        WriteByte(dmaBAddr[i], data);
                        bytesTrf = 1;
                        break;
                    }
                    case 1:
                    {

                        data = ReadByte(dataAddr);
                        WriteByte(dmaBAddr[i], data);
                        data = ReadByte(dataAddr + 1);
                        WriteByte(dmaBAddr[i] + 1, data);
                        bytesTrf = 2;
                        break;
                    }
                    case 6:
                    case 2:
                    {

                        data = ReadByte(dataAddr);
                        WriteByte(dmaBAddr[i], data);
                        data = ReadByte(dataAddr + 1);
                        WriteByte(dmaBAddr[i], data);
                        bytesTrf = 2;
                        break;
                    }
                    case 7:
                    case 3:
                    {

                        data = ReadByte(dataAddr);

                        WriteByte(dmaBAddr[i], data);
                        data = ReadByte(dataAddr + 1);
                        WriteByte(dmaBAddr[i], data);
                        data = ReadByte(dataAddr + 2);
                        WriteByte(dmaBAddr[i] + 1, data);
                        data = ReadByte(dataAddr + 3);
                        WriteByte(dmaBAddr[i] + 1, data);
                        bytesTrf = 4;
                        break;
                    }
                    case 4:
                    {

                        data = ReadByte(dataAddr);
                        WriteByte(dmaBAddr[i], data);
                        data = ReadByte(dataAddr + 1);
                        WriteByte(dmaBAddr[i] + 1, data);
                        data = ReadByte(dataAddr + 2);
                        WriteByte(dmaBAddr[i] + 2, data);
                        data = ReadByte(dataAddr + 3);
                        WriteByte(dmaBAddr[i] + 3, data);
                        bytesTrf = 4;
                        break;
                    }
                    case 5:
                    {
                        data = ReadByte(dataAddr);

                        WriteByte(dmaBAddr[i], data);
                        data = ReadByte(dataAddr + 1);
                        WriteByte(dmaBAddr[i] + 1, data);
                        data = ReadByte(dataAddr + 2);
                        WriteByte(dmaBAddr[i], data);
                        data = ReadByte(dataAddr + 3);
                        WriteByte(dmaBAddr[i] + 1, data);
                        bytesTrf = 4;
                        break;
                    }

                    default:
                        break;
                    }

                    if (dmaIndir[i])
                    {
                        dmaDataPtr[i] += bytesTrf;
                    }
                    else
                    {
                        dmaTablePtr[i] += bytesTrf;
                    }
                }

                dmaLineCnt[i] -= 1;
                hdmaDoTransfer[i] = dmaCont[i];//dmaLineCnt[i] & 0b10000000;

                if (dmaLineCnt[i] == 0) // Re-initialize for the next table
                {
                    uint8 line;

                    line = ReadByte((dmaAAddrBank[i] << 16) | dmaTablePtr[i]);
                    if (line == 0x00)
                    {
                        hdmaEn[i] = false;
                    }
                    else
                    {
                        
                        if (line == 0x80)
                        {
                            dmaLineCnt[i] = 128;
                            dmaCont[i] = false;
                        }
                        else
                        {
                            dmaLineCnt[i] = line & 0b11111111;
                            dmaCont[i] = line & 0b10000000;
                        }
                        
                        //dmaLineCnt[i] = line;

                        hdmaDoTransfer[i] = true;
                    }

                    if (dmaIndir[i])
                    {
                        dmaTablePtr[i]++;
                        add24 addL = (dmaTablePtr[i]) & 0xFFFF;
                        addL |= (dmaAAddrBank[i] << 16);

                        dmaTablePtr[i]++;
                        add24 addH = (dmaTablePtr[i]) & 0xFFFF;
                        addH |= (dmaAAddrBank[i] << 16);

                        dmaDataPtr[i] = ReadByte(addL) | (ReadByte(addH) << 8);
                        dmaTablePtr[i]++;
                    }
                    else
                    {
                        dmaTablePtr[i]++;
                    }
                }

                // cout << "-=-=-=-=-=-=-=-=-=HDMA[" << i << "]=-=-=-=-=-=-=-=-=-=" << endl;
                // cout << "table addr : " << hex << ((dmaAAddrBank[i] << 16) | dmaAAddr[i]) << endl;
                // cout << "table ptr : " << hex << (dmaTablePtr[i]) << endl;
                // cout << "type : " << dec << (uint16)dmaType[i] << endl;
                // cout << "abs/indir : " << (dmaIndir[i] ? "indir" : "abs") << endl;
                // cout << "cont : " << (dmaCont[i] ? "y" : "n") << endl;
                // cout << "count : " << hex << (uint16)dmaLineCnt[i] << endl;
                // cout << "b addr : " << hex << dmaBAddr[i] << endl;
            }
        }
        else
        {

            int i = 0;
            for (; i < 8; i++)
                if (dmaEn[i])
                    break;
            if (i == 8)
                return;

            add24 aAddr = dmaAAddrBank[i] << 16 | dmaAAddr[i];

            // cout << "-=-=-=-=-=-=-=-=-=DMA[" << i << "]=-=-=-=-=-=-=-=-=-=" << endl;
            // cout << "type : " << dec << (uint16)dmaType[i] << endl;
            // cout << "fixed : " << (dmaFixedAddr[i] ? "fixed" : "change") << endl;
            // cout << "inc : " << (dmaDecAddr[i] ? "-" : "+") << endl;
            // cout << "dir : " << (dmaDir[i] ? "B->A" : "A->B") << endl;
            // cout << "b addr : " << hex << dmaBAddr[i] << endl;
            // cout << "a addr : " << hex << aAddr << endl;
            // cout << "bytes : " << hex << dmaDataPtr[i] << endl;
            // cout << "state : " << dec << state << endl;

            add24 offset = dmaType[i] == 0 ? 0 : dmaType[i] == 1 ? state % 2
                                             : dmaType[i] == 2   ? 0
                                             : dmaType[i] == 3   ? (state >> 1) % 2
                                             : dmaType[i] == 4   ? state % 4
                                             : dmaType[i] == 5   ? state % 2
                                             : dmaType[i] == 6   ? 0
                                             : dmaType[i] == 7   ? state >> 1 % 2
                                                                 : 0;

            add24 bEffAdd = (dmaBAddr[i] + offset);
            if (dmaDir[i]) // B to A
            {
                uint8 t = ReadByte(bEffAdd);
                // cout << "effective Addr : " << hex << aAddr << endl;
                // cout << "data : " << hex << (uint16)t << endl;
                WriteByte(aAddr, t);
            }
            else // A to B
            {
                uint8 t = ReadByte(aAddr);
                // cout << "effective B Addr : " << hex << bAddr + offset << endl;
                // cout << "effective A Addr : " << hex << aEffAdd << endl;
                // cout << "data : " << hex << (uint16)t << endl;
                WriteByte(bEffAdd, t);
            }

            if (!dmaFixedAddr[i])
            {
                if (dmaDecAddr[i])
                {
                    dmaAAddr[i]--;
                }
                else
                {
                    dmaAAddr[i]++;
                }
            }

            dmaDataPtr[i]--;
            state++;
            if (dmaDataPtr[i] == 0)
            {
                state = 0;
                dmaEn[i] = 0;
            }
        }
    }
};