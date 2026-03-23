#pragma once
#include "Core/CPU.h"
#include "Core/MemoryBus.h"
#include "Assembler/Assembler.h"
#include "TextEditor.h"

namespace MIPS::UI {
    void DrawPipelineWidget(const CPU& cpu, const AssembledProgram* program);
    void DrawMemoryWidget(MemoryBus& bus);
    void DrawRegisterWidget(CPU& cpu);
    void DrawEditorWidget(CPU& cpu, TextEditor& editor, const AssembledProgram* program);
    void DrawArchitectureWidget(const CPU& cpu);
    void DrawSignalsWidget(const CPU& cpu);
}
