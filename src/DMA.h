typedef unsigned char bool;
typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int add24;

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

uint8 DMA_IORead(add24 address);
void DMA_IOWrite(add24 address, uint8 data);
void DMA_InitializeHDMA();
void DMA_Step(bool hBlank);