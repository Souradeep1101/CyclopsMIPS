#include "ImGuiApp.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <cstdio>
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <nfd.h>
#include "Core/CyclopsPrint.h"
#include <chrono>
#include "UI_Widgets.h"
#include "Core/Logger.h"
#include "Cyclops/Utils/PlatformUtils.h"
#include <fstream>
#include <sstream>
#include "BrandAsset.h"
#include "stb_image.h"

namespace MIPS::UI {

static std::string OpenFileDialog() {
    return Cyclops::FileDialogs::OpenFile("s,asm");
}

static std::string SaveFileDialog() {
    return Cyclops::FileDialogs::SaveFile("s");
}

void ApplyEngineeringPrecisionTheme() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    // The Monolithic Logic Engine Tokens
    const ImVec4 base_floor = ImVec4(0.070f, 0.074f, 0.086f, 1.0f); // #121316
    const ImVec4 trench     = ImVec4(0.051f, 0.055f, 0.067f, 1.0f); // #0D0E11
    const ImVec4 plateau    = ImVec4(0.161f, 0.165f, 0.176f, 1.0f); // #292A2D
    
    const ImVec4 primary    = ImVec4(0.266f, 0.847f, 0.945f, 1.0f); // #44D8F1
    const ImVec4 secondary  = ImVec4(1.000f, 0.843f, 0.600f, 1.0f); // #FFD799
    const ImVec4 tertiary   = ImVec4(0.470f, 0.862f, 0.466f, 1.0f); // #78DC77
    const ImVec4 error      = ImVec4(1.000f, 0.705f, 0.670f, 1.0f); // #FFB4AB

    const ImVec4 fg         = ImVec4(0.900f, 0.900f, 0.900f, 1.0f);
    const ImVec4 fg_mute    = ImVec4(0.500f, 0.500f, 0.500f, 1.0f);

    colors[ImGuiCol_Text]                   = fg;
    colors[ImGuiCol_TextDisabled]           = fg_mute;
    
    // Background Hierarchies
    colors[ImGuiCol_WindowBg]               = base_floor;
    colors[ImGuiCol_ChildBg]                = trench;
    colors[ImGuiCol_PopupBg]                = plateau;
    
    // Zero-Border Sectioning
    colors[ImGuiCol_Border]                 = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    colors[ImGuiCol_BorderShadow]           = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    
    colors[ImGuiCol_FrameBg]                = trench;
    colors[ImGuiCol_FrameBgHovered]         = plateau;
    colors[ImGuiCol_FrameBgActive]          = primary;
    
    colors[ImGuiCol_TitleBg]                = plateau;
    colors[ImGuiCol_TitleBgActive]          = plateau;
    colors[ImGuiCol_TitleBgCollapsed]       = base_floor;
    
    colors[ImGuiCol_MenuBarBg]              = plateau;
    colors[ImGuiCol_ScrollbarBg]            = base_floor;
    colors[ImGuiCol_ScrollbarGrab]          = plateau;
    colors[ImGuiCol_ScrollbarGrabHovered]   = primary;
    colors[ImGuiCol_ScrollbarGrabActive]    = primary;
    
    colors[ImGuiCol_CheckMark]              = primary;
    colors[ImGuiCol_SliderGrab]             = primary;
    colors[ImGuiCol_SliderGrabActive]       = primary;
    
    colors[ImGuiCol_Button]                 = plateau;
    colors[ImGuiCol_ButtonHovered]          = primary;
    colors[ImGuiCol_ButtonActive]           = primary;
    
    colors[ImGuiCol_Header]                 = plateau;
    colors[ImGuiCol_HeaderHovered]          = primary;
    colors[ImGuiCol_HeaderActive]           = primary;
    
    colors[ImGuiCol_Separator]              = base_floor;
    colors[ImGuiCol_SeparatorHovered]       = plateau;
    colors[ImGuiCol_SeparatorActive]        = primary;
    
    colors[ImGuiCol_Tab]                    = trench;
    colors[ImGuiCol_TabHovered]             = plateau;
    colors[ImGuiCol_TabActive]              = plateau;
    colors[ImGuiCol_TabUnfocused]           = trench;
    colors[ImGuiCol_TabUnfocusedActive]     = plateau;
    
    colors[ImGuiCol_DockingPreview]         = primary;
    colors[ImGuiCol_DockingEmptyBg]         = base_floor;

    // Strict 0px Logic Aesthetic
    style.WindowBorderSize  = 0.0f;
    style.ChildBorderSize   = 0.0f;
    style.PopupBorderSize   = 0.0f;
    style.FrameBorderSize   = 0.0f;
    style.TabBorderSize     = 0.0f;

    style.WindowRounding    = 0.0f;
    style.ChildRounding     = 0.0f;
    style.FrameRounding     = 0.0f;
    style.PopupRounding     = 0.0f;
    style.ScrollbarRounding = 0.0f;
    style.GrabRounding      = 0.0f;
    style.TabRounding       = 0.0f;
    
    // Professional Spacing Specs
    style.ItemSpacing       = ImVec2(10.0f, 10.0f);
    style.FramePadding      = ImVec2(18.0f, 10.0f);
}

ImGuiApp::ImGuiApp(CPU& cpu_ref, MemoryBus& bus_ref) 
    : emulator(&cpu_ref), bus(&bus_ref) {
}

ImGuiApp::~ImGuiApp() {
    isRunning = false;
    if (emuThread.joinable()) {
        emuThread.join();
    }

    if (window) {
        if (m_logoTexture != 0) {
            glDeleteTextures(1, &m_logoTexture);
        }
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwDestroyWindow(window);
        glfwTerminate();
    }
    NFD_Quit();
}

bool ImGuiApp::Initialize(int width, int height, const char* title) {
    if (!glfwInit()) {
        Cyclops::println("Failed to initialize GLFW");
        return false;
    }

    // Global NFD extended lifecycle
    NFD_Init();

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (!window) {
        Cyclops::println("Failed to create GLFW window");
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        Cyclops::println("Failed to initialize GLAD");
        return false;
    }

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable; 
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    // Setup Typography Hierarchy
    io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 20.0f); // Default (Inter fallback)
    ImFont* titleFont = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeuib.ttf", 24.0f); // Bold Header
    ImFont* labelFont = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 18.0f); // Labels
    ImFont* monoFont  = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\consola.ttf", 20.0f); // Monospace Code
    
    ApplyEngineeringPrecisionTheme();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // --- High-Fidelity Branding Initialization (AFTER GL CONTEXT IS READY) ---
    stbi_set_flip_vertically_on_load(false);
    int channels;
    unsigned char* data = stbi_load_from_memory(logo_png, sizeof(logo_png), &m_logoWidth, &m_logoHeight, &channels, 4);
    if (data && m_logoWidth > 0 && m_logoHeight > 0) {
        glGenTextures(1, &m_logoTexture);
        glBindTexture(GL_TEXTURE_2D, m_logoTexture);
        
        // Setup Bilinear Filtering for High-DPI Scaling
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        
        // Ensure byte alignment (important for varying widths)
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m_logoWidth, m_logoHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        
        // --- System Icon Branding ---
        GLFWimage images[1];
        images[0].width = m_logoWidth;
        images[0].height = m_logoHeight;
        images[0].pixels = data;
        glfwSetWindowIcon(window, 1, images);

        stbi_image_free(data);
        
        Core::Logger::Get().Log(Core::Logger::Channel::Console, 
            std::format("Brand Asset Initialized: {}x{} [ID:{}]", m_logoWidth, m_logoHeight, m_logoTexture));
    } else {
        std::string reason = data ? "Invalid Dimensions" : (stbi_failure_reason() ? stbi_failure_reason() : "Unknown STB error");
        Core::Logger::Get().Log(Core::Logger::Channel::Console, "[Graphics] Branding Failed: " + reason);
        if (data) stbi_image_free(data);
    }

    // Start Emulator Thread
    emuThread = std::thread(&ImGuiApp::EmuThreadLoop, this);

    return true;
}

void ImGuiApp::EmuThreadLoop() {
    while (isRunning) {
        if (emulator->waitingForInput) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }

        if (isPaused) {
            if (stepRequested) {
                (void)emulator->step();
                stepRequested = false;
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(20));
            }
            continue;
        }

        // Breakpoint Timing: Check BEFORE step()
        if (emulator->hasBreakpoint(emulator->getState().pc)) {
            isPaused = true;
            Core::Logger::Get().Log(Core::Logger::Channel::Emulation, 
                std::format("Paused at Breakpoint: 0x{:08X}", emulator->getState().pc));
            continue;
        }

        (void)emulator->step();
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
}

static void SetupDefaultLayout(ImGuiID dockspace_id) {
    ImGui::DockBuilderRemoveNode(dockspace_id);
    ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->Size);

    ImGuiID dock_main_id = dockspace_id;
    
    // 1. Right Pane (Inspector: Memory & Registers) - 20% width
    ImGuiID dock_id_right = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.20f, nullptr, &dock_main_id);

    // 2. Left Pane (Code Editor) - 25% of remainder (20% of total)
    ImGuiID dock_id_left = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, 0.25f, nullptr, &dock_main_id);

    // 3. Center Pane: Split at top for the non-resizable Execution Toolbar
    ImGuiID dock_id_toolbar = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Up, 0.06f, nullptr, &dock_main_id);
    // Lock the toolbar node
    if (auto* node = ImGui::DockBuilderGetNode(dock_id_toolbar)) {
        node->LocalFlags |= ImGuiDockNodeFlags_NoTabBar | ImGuiDockNodeFlags_NoResize | ImGuiDockNodeFlags_NoDockingOverMe;
    }

    // 4. Center Main: Diagram (Top 75%) vs Tabs (Bottom 25%)
    ImGuiID dock_id_center_bottom;
    ImGuiID dock_id_diagram = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Up, 0.75f, nullptr, &dock_id_center_bottom);

    // --- Assign Windows ---
    ImGui::DockBuilderDockWindow("Execution Toolbar", dock_id_toolbar);

    ImGui::DockBuilderDockWindow("Code Editor & Execution", dock_id_left);
    
    ImGui::DockBuilderDockWindow("MIPS CPU Architecture Diagram", dock_id_diagram);

    ImGui::DockBuilderDockWindow("Pipeline Datapath", dock_id_center_bottom);
    ImGui::DockBuilderDockWindow("Pipeline Signals", dock_id_center_bottom);
    ImGui::DockBuilderDockWindow("Terminal", dock_id_center_bottom);
    
    ImGui::DockBuilderDockWindow("Memory (Data)", dock_id_right);
    ImGui::DockBuilderDockWindow("Registers", dock_id_right);

    ImGui::DockBuilderFinish(dockspace_id);
}

void ImGuiApp::RenderUI() {
    // Dockspace setup
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGuiID dockspace_id = ImGui::GetID("CyclopsDockSpace");
    
    // Set up main dockspace OverViewport
    ImGui::DockSpaceOverViewport(dockspace_id, viewport);

    bool trigger_reset = false;

    // Optional Main Menu Bar for layout reset
    // Global Navbar & Menu System
    if (ImGui::BeginMainMenuBar()) {
        // --- UX Architect: Centered Brand Icon ---
        if (m_logoTexture) {
            float barHeight = ImGui::GetWindowHeight();
            float iconDim = 20.0f;
            ImGui::SetCursorPosY((barHeight - iconDim) * 0.5f);
            ImGui::Image((void*)(intptr_t)m_logoTexture, ImVec2(iconDim, iconDim));
            ImGui::Dummy(ImVec2(8.0f, 0.0f)); 
        }

        if (ImGui::BeginMenu("File")) {
            if (ImGui::MenuItem("New Project", "Ctrl+N")) {
                m_textEditor.SetText("");
                emulator->reset();
                bus->reset();
            }
            if (ImGui::MenuItem("Open File...", "Ctrl+O")) {
                std::string path = OpenFileDialog();
                if (!path.empty()) {
                    std::ifstream t(path);
                    std::stringstream buffer;
                    buffer << t.rdbuf();
                    m_textEditor.SetText(buffer.str());
                }
            }
            if (ImGui::MenuItem("Save", "Ctrl+S")) {
                std::string path = SaveFileDialog();
                if (!path.empty()) {
                    std::ofstream out(path);
                    out << m_textEditor.GetText();
                }
            }
            ImGui::Separator();
            if (ImGui::BeginMenu("Export Options")) {
                if (ImGui::MenuItem("Export Memory (.hex)")) {
                    std::ofstream out("memory_dump.hex");
                    for (uint32_t i = 0; i < 0x2000; i += 4) {
                        out << std::format("0x{:08X}: 0x{:08X}\n", i, bus->readWordDirect(i));
                    }
                    Core::Logger::Get().Log(Core::Logger::Channel::Emulation, "Memory state exported to memory_dump.hex");
                }
                if (ImGui::MenuItem("Export Execution Log (.txt)")) {
                    std::ofstream out("execution_log.txt");
                    for (const auto& log : Core::Logger::Get().GetEntries(Core::Logger::Channel::Emulation)) {
                        out << log << "\n";
                    }
                    Core::Logger::Get().Log(Core::Logger::Channel::Emulation, "Log exported to execution_log.txt");
                }
                ImGui::EndMenu();
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Exit", "Alt+F4")) isRunning = false;
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("View")) {
            ImGui::MenuItem("Code Editor", nullptr, &showEditor);
            ImGui::MenuItem("Architecture Diagram", nullptr, &showArch);
            ImGui::MenuItem("Memory (Data)", nullptr, &showMemory);
            ImGui::MenuItem("Registers", nullptr, &showRegisters);
            ImGui::MenuItem("Pipeline Datapath", nullptr, &showPipeline);
            ImGui::MenuItem("Pipeline Signals", nullptr, &showSignals);
            ImGui::MenuItem("Terminal", nullptr, &showTerminal);
            ImGui::Separator();
            if (ImGui::MenuItem("Reset Layout")) trigger_reset = true;
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Run")) {
            if (ImGui::MenuItem("Toggle Breakpoint", "F9")) {
                int line = m_textEditor.GetCursorPosition().mLine + 1;
                if (hasCompiled) {
                    for (const auto& [pc, meta] : activeProgram.sourceMap) {
                        if (meta.lineNum == line) {
                            emulator->toggleBreakpoint(pc);
                            break;
                        }
                    }
                }
            }
            if (ImGui::MenuItem("Run to Cursor", "F10")) {
                // Feature placeholder
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Compile & Load", "F5")) {
                std::string payload = m_textEditor.GetText();
                auto result = MIPS::Assembler::assemble(payload);
                if (result.has_value()) {
                    Core::Logger::Get().Log(Core::Logger::Channel::Emulation, "Compilation Successful.");
                    bus->reset();
                    emulator->reset();
                    bus->loadProgram(result.value().machineCode);
                    activeProgram = result.value();
                    hasCompiled = true;
                    isPaused = true;
                } else {
                    hasCompiled = false;
                    Core::Logger::Get().Log(Core::Logger::Channel::Emulation, "Assembler Error: " + result.error());
                }
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Terminal")) {
            if (ImGui::MenuItem("New Terminal", "Ctrl+T")) {
                terminals.push_back({ "CLI " + std::to_string(terminals.size() + 1) });
                showTerminal = true;
            }
            if (ImGui::MenuItem("Clear All Terminals")) {
                terminals.clear();
                Core::Logger::Get().GetEntries(Core::Logger::Channel::Console).clear();
                Core::Logger::Get().GetEntries(Core::Logger::Channel::Emulation).clear();
            }
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Help")) {
            if (ImGui::MenuItem("About CyclopsMIPS")) { 
                showAbout = true;
            }
            if (ImGui::MenuItem("Interactive Walkthrough")) { /* Placeholder */ }
            ImGui::Separator();
            ImGui::TextDisabled("Version 1.0.0 (V1-Stable)");
            ImGui::TextDisabled("Build: 2026-03-28");
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    if (trigger_reset) {
        SetupDefaultLayout(dockspace_id);
    }

    // Modal UI Layer
    if (showAbout) {
        ImGui::OpenPopup("About CyclopsMIPS");
    }
    MIPS::UI::DrawAboutDialog(&showAbout, (ImTextureID)(intptr_t)m_logoTexture);

    // --- Execution Toolbar (Phase 2) ---
    ImGuiWindowFlags toolbar_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse;
    if (ImGui::Begin("Execution Toolbar", nullptr, toolbar_flags)) {
        float btn_size = 32.0f;
        ImGui::SetCursorPos(ImVec2(10, 5));
        
        // Play/Pause
        bool current_paused = isPaused.load();
        if (ImGui::Button(current_paused ? "Play" : "Pause", ImVec2(80, 0))) {
            isPaused = !current_paused;
        }
        ImGui::SameLine();

        // Step
        if (ImGui::Button("Step", ImVec2(80, 0))) {
            stepRequested = true;
        }
        ImGui::SameLine();

        // Reset
        if (ImGui::Button("Reset", ImVec2(80, 0))) {
            emulator->reset();
            isPaused = true;
        }

        ImGui::SameLine();
        ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
        ImGui::SameLine();

        // Status Indicator
        if (current_paused) {
            ImGui::TextColored(ImVec4(1.0f, 0.84f, 0.0f, 1.0f), "SYSTEM IDLE / PROGRAM HALTED");
        } else {
            ImGui::TextColored(ImVec4(0.47f, 0.86f, 0.47f, 1.0f), "SYSTEM RUNNING...");
        }
    }
    ImGui::End();

    DrawArchitectureWidget(*emulator, &showArch);
    DrawEditorWidget(*emulator, m_textEditor, hasCompiled ? &activeProgram : nullptr, &showEditor);
    DrawMemoryWidget(*bus, *emulator, &showMemory);
    DrawRegisterWidget(*emulator, &showRegisters);
    DrawPipelineWidget(*emulator, hasCompiled ? &activeProgram : nullptr, &showPipeline);
    DrawSignalsWidget(*emulator, &showSignals);

    bool terminalStep = false;
    DrawTerminalWidget(&showTerminal, terminals, *emulator, &terminalStep);
    if (terminalStep) stepRequested = true;
}

void ImGuiApp::Run() {
    while (!glfwWindowShouldClose(window)) {
        auto frameStart = std::chrono::high_resolution_clock::now();

        glfwPollEvents();

        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        if (display_w == 0 || display_h == 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(16));
            continue;
        }

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        RenderUI();

        // 3.0s Transient Highlight evaluation (Shared across Architecture & Signals)
        if (!g_schematicSelection.selectedNodeId.empty() || !g_schematicSelection.selectedWireId.empty()) {
            if (ImGui::GetTime() - g_schematicSelection.selectionTime > 3.0) {
                g_schematicSelection.clear();
            }
        }

        // Rendering
        ImGui::Render();
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1f, 0.1f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Update and Render additional Platform Windows
        if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            GLFWwindow* backup_current_context = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup_current_context);
        }

        glfwSwapBuffers(window);

        auto frameEnd = std::chrono::high_resolution_clock::now();
        auto frameDuration = std::chrono::duration_cast<std::chrono::milliseconds>(frameEnd - frameStart).count();
        if (frameDuration < 16) {
            std::this_thread::sleep_for(std::chrono::milliseconds(16 - frameDuration));
        }
    }
    isRunning = false;
}

} // namespace MIPS::UI
