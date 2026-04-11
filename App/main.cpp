#include "Assembler/Assembler.h"
#include "Core/CPU.h"
#include "Core/MemoryBus.h"
#include "ImGuiApp.h"
#include "Core/CyclopsPrint.h"

using namespace MIPS;

int main() {
    MemoryBus bus;
    CPU cpu(bus);
    auto result = Assembler::assemble(R"(
        ADDI $t0, $zero, 99
        SW   $t0, 0($zero)
        LW   $t1, 0($zero)
        ADD  $t2, $t1, $t1
    )");
    
    if (!result) {
        Cyclops::println("Assembler Error: {}", result.error());
        return 1;
    }

    cpu.loadProgram(result->machineCode);

    // Start the UI
    MIPS::UI::ImGuiApp app(cpu, bus);
    if (!app.Initialize(1280, 720, "CyclopsMIPS Educator IDE")) {
        Cyclops::println("Failed to initialize ImGui application");
        return 1;
    }

    Cyclops::println("Starting CyclopsMIPS ImGui Educator...");
    app.Run();

    return 0;
}