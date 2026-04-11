#include "UI_Widgets.h"
#include <imgui.h>
#include <string>

namespace MIPS::UI {

void DrawEditorWidget(CPU& cpu, TextEditor& editor, const AssembledProgram* program, bool* p_open) {
    if (!*p_open) return;
    if (!ImGui::Begin("Code Editor & Execution", p_open)) {
        ImGui::End();
        return;
    }

    // Determine the active line for highlighting
    int activeLine = -1;
    if (program && program->sourceMap.count(cpu.getState().pc)) {
        activeLine = program->sourceMap.at(cpu.getState().pc).lineNum;
    }

    // Sync breakpoints from CPU to Editor UI visually
    TextEditor::Breakpoints bpSet;
    for (uint32_t pcAddr : cpu.breakpoints) {
        if (program && program->sourceMap.count(pcAddr)) {
            bpSet.insert(program->sourceMap.at(pcAddr).lineNum);
        }
    }
    editor.SetBreakpoints(bpSet);

    // Render the active execution line mnemonic above the editor
    if (activeLine > 0) {
        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.4f, 1.0f), "Executing Line %d:    %s", 
            activeLine, program->sourceMap.at(cpu.getState().pc).mnemonic.c_str());
    } else {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "CPU Idle / Program Halted");
    }

    ImGui::Separator();
    editor.Render("##source");
    ImGui::End();
}

} // namespace MIPS::UI
