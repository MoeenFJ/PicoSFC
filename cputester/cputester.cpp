#include <bits/stdc++.h>
#include "json.hpp"
#include "../src/CPU.cpp"
using namespace std;
using json = nlohmann::json;

CPU *cpu;

uint8 ram[16 * 1024 * 1024] = {0};

uint8 C65Read(add24 address)
{
    address &= 0xffffff;
    //cout << "addrees : " << hex << address << endl;
    return ram[address];
}
void C65Write(add24 address, uint8 data)
{
    address &= 0xffffff;
    //cout << "addrees : " << hex << address << endl;
    ram[address] = data;
}

int main()
{
    cpu = new CPU(C65Read, C65Write);



    for (uint16 inst = 0x00; inst < 256; inst++)
    {

        //Ignore MVN and MVP for now
        if(inst == 0x54 || inst == 0x44 || inst == 0xcb || inst == 0xdb || inst == 0x00 || inst == 0x02 )
            continue;
     
        // Load
        stringstream ss;
        ss << "./tests/" <<  hex << setfill('0') << setw(2) << (uint16)inst << ".n.json";
        std::ifstream tstVectorFile(ss.str());

        json tvs = json::parse(tstVectorFile);
        int testNum = 1;
        
        for (const auto &tv : tvs)
        {
            
            cpu->cregs.PC = (uint16)tv["initial"]["pc"];
            cpu->cregs.S = (uint16)tv["initial"]["s"];
            cpu->flags = (uint8)tv["initial"]["p"];
            cpu->cregs.C = (uint16)tv["initial"]["a"];
            cpu->cregs.X = (uint16)tv["initial"]["x"];
            cpu->cregs.Y = (uint16)tv["initial"]["y"];
            cpu->cregs.DBR = (uint8)tv["initial"]["dbr"];
            cpu->cregs.D = (uint16)tv["initial"]["d"];
            cpu->cregs.K = (uint8)tv["initial"]["pbr"];
            cpu->flagE = (bool)((uint8)tv["initial"]["e"]);

            
            for (const auto &ram_entry : tv["initial"]["ram"])
            {
                add24 address = ram_entry[0];
                uint8 value = ram_entry[1];
                ram[address] = value;
                cout << "Set " << hex << address << " to " << (uint16)value << endl;
            }
            cpu->printStatus();
            // Apply
            cpu->cpuStep();

            // Check
            bool passed = true;
            if (cpu->cregs.PC != (uint16)tv["final"]["pc"]){
                cout << "PC Should be : " << hex  << (uint16)tv["final"]["pc"] << endl;
                passed = false;
            }

            if (cpu->cregs.S != (uint16)tv["final"]["s"]){
                cout << "S Should be : " << hex  << (uint16)tv["final"]["s"] << endl;
                passed = false;
            }

            uint8 expFlags = (uint8)tv["final"]["p"];
            if (cpu->flags.C != (bool)(expFlags&0x01) || cpu->flags.Z != (bool)(expFlags&0x02) || cpu->flags.I != (bool)(expFlags&0x04) || 
                cpu->flags.D != (bool)(expFlags&0x08) || cpu->flags.X != (bool)(expFlags&0x10) || cpu->flags.M != (bool)(expFlags&0x20) ||
                (cpu->flags.V != (bool)(expFlags&0x40) && (bool)(expFlags&0x08) == 0) || cpu->flags.N != (bool)(expFlags&0x80)){
                cout << "flags Should be : " << hex  << (uint16)tv["final"]["p"] << endl;
                cout << "flags are : " << hex  << (uint16)cpu->flags << endl;
                passed = false;
            }

            if (cpu->cregs.C != (uint16)tv["final"]["a"])
            {
                cout << "C Should be : " << hex  << (uint16)tv["final"]["a"] << endl;
                passed = false;
            }

            if (cpu->cregs.X != (uint16)tv["final"]["x"]){
                cout << "X Should be : " << hex  << (uint16)tv["final"]["x"] << endl;
                passed = false;
            }

            if (cpu->cregs.Y != (uint16)tv["final"]["y"]){
                cout << "Y Should be : " << hex  << (uint16)tv["final"]["y"] << endl;
                passed = false;
            }

            if (cpu->cregs.DBR != (uint8)tv["final"]["dbr"]){
                cout << "dbr Should be : " << hex  << (uint8)tv["final"]["dbr"] << endl;
                passed = false;
            }

            if (cpu->cregs.D != (uint16)tv["final"]["d"]){
 cout << "D Should be : " << hex  << (uint8)tv["final"]["d"] << endl;
                passed = false;
            }

            if (cpu->cregs.K != (uint8)tv["final"]["pbr"]){
                 cout << "pbr Should be : " << hex  << (uint8)tv["final"]["pbr"] << endl;
                passed = false;
            }

            if (cpu->flagE != (bool)((uint8)tv["final"]["e"])){
                 cout << "e Should be : " << hex  << (uint8)tv["final"]["e"] << endl;
                passed = false;
            }

                
            for (const auto &ram_entry : tv["final"]["ram"])
            {
                add24 address = ram_entry[0];
                uint8 value = ram_entry[1];
                if(ram[address] != value){
                     cout << "ram at "<<hex<< address  << " Should be : " << hex  <<(uint16)value << " instead is " << (uint16)ram[address] <<endl;
                    passed = false;
                }
            }
            
            if(!passed)
            {
                cout << "ERROR!!!" << endl;
                cpu->printStatus();
                cin.get();
            }
            else
            {
                cout << "PASSED!! : " << dec << testNum << endl;
            }
            testNum ++;
        }
      
    }
}