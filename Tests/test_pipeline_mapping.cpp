#include <gtest/gtest.h>
#include "Core/CPU.h"
#include "Assembler/Assembler.h"
#include "Core/MemoryBus.h"
#include <iostream>
#include <iomanip>

using namespace MIPS;

TEST(PipelineMappingTest, DebugDatapathLookup) {
    MemoryBus bus;
    CPU cpu(bus);
    
    std::string source = 
        "ADDI $t0, $zero, 5\n"
        "ADDI $t1, $zero, 10\n";
        
    auto result = Assembler::assemble(source);
    ASSERT_TRUE(result.has_value()) << "Failed to assemble test program: " << result.error();
    
    AssembledProgram program = result.value();
    
    // 1. Print the Map
    std::cout << "\n============================================\n";
    std::cout << "       SOURCE MAP CONTENTS DUMP\n";
    std::cout << "============================================\n";
    for (const auto& [key, line] : program.sourceMap) {
        std::cout << "Key: 0x" << std::setw(8) << std::setfill('0') << std::hex << key << std::dec 
                  << "   ->   Mnemonic: '" << line.mnemonic << "'" << std::endl;
    }
    std::cout << "============================================\n\n";
    
    // Load and execute
    cpu.loadProgram(program.machineCode);
    
    // Lambda to test lookups cleanly
    auto testLookup = [&](const std::string& method, uint32_t valToCheck) {
        std::cout << "      Lookup using " << std::setw(8) << std::left << method 
                  << " (0x" << std::hex << valToCheck << std::dec << "): ";
        if (program.sourceMap.count(valToCheck)) {
            std::cout << "SUCCESS -> '" << program.sourceMap.at(valToCheck).mnemonic << "'" << std::endl;
        } else {
            std::cout << "FAILED  -> returns '-'" << std::endl;
        }
    };

    // 2. Step the CPU for 3 cycles and print the latches & lookups
    for (int cycle = 1; cycle <= 3; ++cycle) {
        auto stepRes = cpu.step();
        ASSERT_TRUE(stepRes.has_value()) << "CPU step failed: " << stepRes.error();
        
        std::cout << "--- CPU CYCLE " << cycle << " ---" << std::endl;
        std::cout << "[Raw Latches]" << std::endl;
        std::cout << "  cpu.if_id.pc: 0x" << std::hex << cpu.if_id.pc << std::dec << std::endl;
        std::cout << "  cpu.id_ex.pc: 0x" << std::hex << cpu.id_ex.pc << std::dec << std::endl;
        
        std::cout << "\n[IF Latch Verification Test]" << std::endl;
        uint32_t pc = cpu.if_id.pc;
        testLookup("pc", pc);
        testLookup("pc - 4", pc >= 4 ? pc - 4 : 0);
        testLookup("pc / 4", pc / 4);
        
        std::cout << std::endl;
    }
}
