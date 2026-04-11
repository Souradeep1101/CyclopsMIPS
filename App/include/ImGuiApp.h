#pragma once
#include "Core/CPU.h"
#include "Core/MemoryBus.h"
#include "Assembler/Assembler.h"
#include "TextEditor.h"
#include "UI_Widgets.h"
#include <thread>
#include <atomic>
#include <glad/glad.h>

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
    
    // UI Visibility (View Menu Sync)
    bool showEditor = true;
    bool showArch = true;
    bool showMemory = true;
    bool showRegisters = true;
    bool showPipeline = true;
    bool showSignals = true;
    bool showTerminal = true;
    bool showControls = true;
    bool showAbout = false; // Brand Modal State

    GLuint m_logoTexture = 0; // High-DPI Brand Asset
    int m_logoWidth = 0, m_logoHeight = 0;

    // Dynamic Terminals
    std::vector<TerminalInstance> terminals;
};

} // namespace MIPS::UI
