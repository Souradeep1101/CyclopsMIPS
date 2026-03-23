#pragma once
#include "Core/CPU.h"
#include "Core/MemoryBus.h"
#include "Assembler/Assembler.h"
#include "TextEditor.h"
#include <thread>
#include <atomic>

struct GLFWwindow;

namespace MIPS::UI {

class ImGuiApp {
public:
    ImGuiApp(CPU& cpu, MemoryBus& bus);
    ~ImGuiApp();

    bool Initialize(int width = 1280, int height = 720, const char* title = "CyclopsMIPS Emulator");
    void Run();

private:
    void RenderUI();
    
    // CPU Emulation Thread
    void EmuThreadLoop();

    CPU* emulator = nullptr;
    MemoryBus* bus = nullptr;

    GLFWwindow* window = nullptr;

    std::thread emuThread;
    std::atomic<bool> isRunning{true};
    std::atomic<bool> isPaused{true}; // Start paused so students can observe
    std::atomic<bool> stepRequested{false};

    // Editor State
    TextEditor m_textEditor;
    AssembledProgram activeProgram;
    bool hasCompiled = false;
};

} // namespace MIPS::UI
