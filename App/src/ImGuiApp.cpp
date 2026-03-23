#include "ImGuiApp.h"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <cstdio>
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>
#include <print>
#include "UI_Widgets.h"

namespace MIPS::UI {

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
    
    // Engineering Spacing Specs
    style.ItemSpacing       = ImVec2(8.0f, 8.0f);
    style.FramePadding      = ImVec2(14.0f, 6.0f);
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
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
        glfwDestroyWindow(window);
        glfwTerminate();
    }
}

bool ImGuiApp::Initialize(int width, int height, const char* title) {
    if (!glfwInit()) {
        std::println("Failed to initialize GLFW");
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window = glfwCreateWindow(width, height, title, nullptr, nullptr);
    if (!window) {
        std::println("Failed to create GLFW window");
        glfwTerminate();
        return false;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // Enable vsync

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::println("Failed to initialize GLAD");
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
    io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 16.0f); // Default (Inter fallback)
    ImFont* titleFont = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeuib.ttf", 20.0f); // Bold Header (Space Grotesk fallback)
    ImFont* labelFont = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", 14.0f); // Labels
    ImFont* monoFont  = io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\consola.ttf", 16.0f); // Monospace Code
    
    // We can store these globally if needed, or pass them. 
    // For now, setting the default font handles the body, we will rely on standard sizing for the rest of this pass.

    ApplyEngineeringPrecisionTheme();

    // Setup Platform/Renderer backends
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 330");

    // Start Emulator Thread
    emuThread = std::thread(&ImGuiApp::EmuThreadLoop, this);

    return true;
}

void ImGuiApp::EmuThreadLoop() {
    // Loop running in background thread
    while (isRunning) {
        if (!isPaused) {
            emulator->step();
            std::this_thread::sleep_for(std::chrono::milliseconds(16)); // Throttle execution for visualizer
        } else if (stepRequested) {
            emulator->step();
            stepRequested = false;
        } else {
            std::this_thread::sleep_for(std::chrono::milliseconds(16)); // Idle poll
        }
    }
}

static void SetupDefaultLayout(ImGuiID dockspace_id) {
    ImGui::DockBuilderRemoveNode(dockspace_id); // clear any previous layout
    ImGui::DockBuilderAddNode(dockspace_id, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dockspace_id, ImGui::GetMainViewport()->Size);

    // Root is split horizontally: left (vast majority) and right (Memory/Registers)
    ImGuiID dock_main_id = dockspace_id;
    ImGuiID dock_id_right = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.20f, nullptr, &dock_main_id);
    ImGuiID dock_id_left  = dock_main_id; // Left is the remaining 80%

    // Split Right: left (Memory) and right (Registers)
    ImGuiID dock_id_right_left, dock_id_right_right;
    ImGui::DockBuilderSplitNode(dock_id_right, ImGuiDir_Right, 0.5f, &dock_id_right_right, &dock_id_right_left);

    // Split Left: top (Code Editor) and bottom (Panels)
    ImGuiID dock_id_bottom;
    ImGuiID dock_id_top = ImGui::DockBuilderSplitNode(dock_id_left, ImGuiDir_Up, 0.40f, nullptr, &dock_id_bottom);

    // Split Bottom: left (Controls & Signals) and right (Diagram & Datapath)
    ImGuiID dock_id_bottom_right;
    ImGuiID dock_id_bottom_left = ImGui::DockBuilderSplitNode(dock_id_bottom, ImGuiDir_Left, 0.25f, nullptr, &dock_id_bottom_right);

    // Split Bottom Left: top (Controls) and bottom (Signals)
    ImGuiID dock_id_bl_bottom;
    ImGuiID dock_id_bl_top = ImGui::DockBuilderSplitNode(dock_id_bottom_left, ImGuiDir_Up, 0.15f, nullptr, &dock_id_bl_bottom);

    // Split Bottom Right: top (Diagram) and bottom (Datapath)
    ImGuiID dock_id_br_bottom;
    ImGuiID dock_id_br_top = ImGui::DockBuilderSplitNode(dock_id_bottom_right, ImGuiDir_Up, 0.75f, nullptr, &dock_id_br_bottom);

    // Assign windows to specific docks (Wait! The exact names must match exactly what we have in code!)
    ImGui::DockBuilderDockWindow("Code Editor & Execution", dock_id_top);
    
    ImGui::DockBuilderDockWindow("Memory (Data)", dock_id_right_left);
    ImGui::DockBuilderDockWindow("Registers", dock_id_right_right); // Let's check the code: UI_RegisterWidget.cpp uses "Registers"
    
    ImGui::DockBuilderDockWindow("Emulator Controls", dock_id_bl_top); // Let's check the code: ImGuiApp.cpp uses "Emulator Controls"
    ImGui::DockBuilderDockWindow("Pipeline Signals", dock_id_bl_bottom);
    
    ImGui::DockBuilderDockWindow("MIPS CPU Architecture Diagram", dock_id_br_top);
    ImGui::DockBuilderDockWindow("Pipeline Datapath", dock_id_br_bottom);

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
    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("View")) {
            if (ImGui::MenuItem("Reset Layout")) {
                trigger_reset = true;
            }
            ImGui::EndMenu();
        }
        ImGui::EndMainMenuBar();
    }

    static bool layout_initialized = false;
    
    // Automatically trigger reset if imgui.ini does not exist on first boot
    if (!layout_initialized) {
        // If the dock node doesn't exist natively, or no ini is present
        if (ImGui::DockBuilderGetNode(dockspace_id) == NULL) {
            trigger_reset = true;
        }
        layout_initialized = true;
    }

    if (trigger_reset) {
        SetupDefaultLayout(dockspace_id);
    }

    // Draw the new UI Panels
    DrawArchitectureWidget(*emulator);
    DrawEditorWidget(*emulator, m_textEditor, hasCompiled ? &activeProgram : nullptr);
    
    // Standard Modules
    DrawMemoryWidget(*bus);
    DrawRegisterWidget(*emulator);
    DrawPipelineWidget(*emulator, hasCompiled ? &activeProgram : nullptr);
    DrawSignalsWidget(*emulator);

    // Emulator Controls
    ImGui::Begin("Emulator Controls");
    
    // Compilation & Hardware Re-initialization
    if (ImGui::Button("Compile & Load MIPS", ImVec2(-1, 0))) {
        std::string payload = m_textEditor.GetText();
        auto result = MIPS::Assembler::assemble(payload);
        if (result.has_value()) {
            bus->reset();
            emulator->reset();
            bus->loadProgram(result.value().machineCode);
            activeProgram = result.value();
            hasCompiled = true;
            isPaused = true;
        } else {
            hasCompiled = false;
            // TODO: Expose the error trace `result.error()` in UI
        }
    }
    ImGui::Separator();

    bool paused = isPaused.load();
    if (ImGui::Button(paused ? "Play" : "Pause")) {
        isPaused = !paused;
    }
    
    ImGui::SameLine();
    
    ImGui::BeginDisabled(!paused); // Only allow step if paused
    if (ImGui::Button("Step Cycle")) {
        stepRequested = true;
    }
    ImGui::EndDisabled();

    ImGui::End();
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
