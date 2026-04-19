#include <bits/stdc++.h>
#include "json.hpp"
#include "../src/SPC700.cpp"
using namespace std;
using json = nlohmann::json;

SPC700 *cpu;

uint8 ram[64 * 1024] = {0};

uint8 SPC700Read(uint16 address)
{
    cout << "addrees : " << hex << address << endl;
    return ram[address];
}
void SPC700Write(uint16 address, uint8 data)
{
    cout << "addrees : " << hex << address << endl;
    ram[address] = data;
}

int main(int argc, char *argv[])
{

    cpu = new SPC700(SPC700Read, SPC700Write);

    uint16 inst = 0x00;
    bool once = false;
    if(argc == 2){
        inst = stoul(argv[1], nullptr, 16);
        once = true;
    }

   
    for (; inst < 256; inst++)
    {

        

        // Load
        stringstream ss;
        ss << "./spctests/" << hex << setfill('0') << setw(2) << (uint16)inst << ".json";
        std::ifstream tstVectorFile(ss.str());

        json tvs = json::parse(tstVectorFile);
        int testNum = 1;

        for (const auto &tv : tvs)
        {

            cpu->PC = (uint16)tv["initial"]["pc"];
            cpu->SP = (uint8)tv["initial"]["sp"];
            cpu->flags = (uint8)tv["initial"]["psw"];
            cpu->A = (uint8)tv["initial"]["a"];
            cpu->X = (uint16)tv["initial"]["x"];
            cpu->Y = (uint16)tv["initial"]["y"];

            for (const auto &ram_entry : tv["initial"]["ram"])
            {
                uint16 address = ram_entry[0];
                uint8 value = ram_entry[1];
                ram[address] = value;
                cout << "Set " << hex << address << " to " << (uint16)value << endl;
            }
            cpu->PrintState();
            // Apply
            cpu->Step();

            // Check
            bool passed = true;
            if (cpu->PC != (uint16)tv["final"]["pc"])
            {
                cout << "PC Should be : " << hex << (uint16)tv["final"]["pc"] << endl;
                passed = false;
            }

            if (cpu->SP != (uint8)tv["final"]["sp"])
            {
                cout << "S Should be : " << hex << (uint16)tv["final"]["sp"] << endl;
                passed = false;
            }

            uint8 expFlags = (uint8)tv["final"]["psw"];
            if (cpu->flags != expFlags)
            {
                cout << "flags Should be : " << hex << (uint16)tv["final"]["psw"] << endl;
                cout << "flags are : " << hex << (uint16)cpu->flags << endl;
                passed = false;
            }

            if (cpu->A != (uint8)tv["final"]["a"])
            {
                cout << "A Should be : " << hex << (uint16)tv["final"]["a"] << endl;
                passed = false;
            }

            if (cpu->X != (uint8)tv["final"]["x"])
            {
                cout << "X Should be : " << hex << (uint16)tv["final"]["x"] << endl;
                passed = false;
            }

            if (cpu->Y != (uint8)tv["final"]["y"])
            {
                cout << "Y Should be : " << hex << (uint16)tv["final"]["y"] << endl;
                passed = false;
            }

            for (const auto &ram_entry : tv["final"]["ram"])
            {
                uint16 address = ram_entry[0];
                uint8 value = ram_entry[1];
                if (ram[address] != value)
                {
                    cout << "ram at " << hex << address << " Should be : " << hex << (uint16)value << " instead is " << (uint16)ram[address] << endl;
                    passed = false;
                }
            }

            if (!passed)
            {
                cout << "ERROR!!!" << endl;
                cpu->PrintState();
                cin.get();
            }
            else
            {
                cout << "PASSED!! : " << dec << testNum << endl;
            }
            testNum++;
        }

        if(once)
            break;
    }
}