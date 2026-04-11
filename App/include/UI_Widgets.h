#pragma once
#include "Core/CPU.h"
#include "Core/MemoryBus.h"
#include "Assembler/Assembler.h"
#include "TextEditor.h"
#include <string>
#include <imgui.h>

namespace MIPS::UI {

// ---- Shared Cross-Panel Selection State ----------------------------------
// Written by the Architecture Diagram and Pipeline Signals panels.
// Read by both to provide two-way highlight synchronisation.
struct SchematicSelection {
    std::string selectedWireId;
    std::string selectedNodeId;
    double selectionTime = 0.0;
    void clear() { selectedWireId.clear(); selectedNodeId.clear(); selectionTime = 0.0; }
};

// ---- Dynamic Terminal State ----------------------------------------------
struct TerminalInstance {
    std::string name;
    bool open = true;
    std::vector<std::string> history;
    char inputBuffer[256] = "";
};

// Global instance (defined in UI_ArchitectureWidget.cpp)
extern SchematicSelection g_schematicSelection;

// ---- Widget Draw Functions -----------------------------------------------
void DrawPipelineWidget(const CPU& cpu, const AssembledProgram* program, bool* p_open);
void DrawMemoryWidget(MemoryBus& bus, const CPU& cpu, bool* p_open);
void DrawRegisterWidget(CPU& cpu, bool* p_open);
void DrawEditorWidget(CPU& cpu, TextEditor& editor, const AssembledProgram* program, bool* p_open);
void DrawArchitectureWidget(const CPU& cpu, bool* p_open);
void CleanupArchitectureWidget();
void DrawSignalsWidget(const CPU& cpu, bool* p_open);
void DrawTerminalWidget(bool* p_open, std::vector<TerminalInstance>& terminals, CPU& cpu, bool* stepRequested = nullptr);
void DrawAboutDialog(bool* p_open, ImTextureID logoTexture);

} // namespace MIPS::UI
