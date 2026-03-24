#pragma once
#include "Core/CPU.h"
#include "Core/MemoryBus.h"
#include "Assembler/Assembler.h"
#include "TextEditor.h"
#include <string>

namespace MIPS::UI {

// ---- Shared Cross-Panel Selection State ----------------------------------
// Written by the Architecture Diagram and Pipeline Signals panels.
// Read by both to provide two-way highlight synchronisation.
struct SchematicSelection {
    std::string selectedWireId;
    std::string selectedNodeId;
    void clear() { selectedWireId.clear(); selectedNodeId.clear(); }
};

// Global instance (defined in UI_ArchitectureWidget.cpp)
extern SchematicSelection g_schematicSelection;

// ---- Widget Draw Functions -----------------------------------------------
void DrawPipelineWidget(const CPU& cpu, const AssembledProgram* program);
void DrawMemoryWidget(MemoryBus& bus);
void DrawRegisterWidget(CPU& cpu);
void DrawEditorWidget(CPU& cpu, TextEditor& editor, const AssembledProgram* program);
void DrawArchitectureWidget(const CPU& cpu);
void CleanupArchitectureWidget();
void DrawSignalsWidget(const CPU& cpu);

} // namespace MIPS::UI
