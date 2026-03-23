#include "UI_Widgets.h"
#include <imgui.h>
#include <string>

namespace MIPS::UI {

void DrawEditorWidget(CPU& cpu, TextEditor& editor, const AssembledProgram* program) {
    ImGui::Begin("Code Editor & Execution");

    // Determine the active line for highlighting
    int activeLine = -1;
    if (program && program->sourceMap.count(cpu.getState().pc)) {
        activeLine = program->sourceMap.at(cpu.getState().pc).lineNum;
    }

    // Render the active execution line mnemonic above the editor
    if (activeLine > 0) {
        ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.4f, 1.0f), "Executing Line %d:    %s", 
            activeLine, program->sourceMap.at(cpu.getState().pc).mnemonic.c_str());
    } else {
        ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "CPU Idle / Program Halted");
    }

    // Update TextEditor PC Arrow state!
    if (activeLine > 0) {
        TextEditor::ErrorMarkers markers;
        // Inject a dummy error marker which essentially draws an arrow!
        // We can just use the colorizing properties. Wait! ImGuiColorTextEdit actually exposes breakpoints!
        editor.SetBreakpoints({activeLine}); // Let's use a breakpoint dot to mark it if arrow isn't visible?
        // Wait, the standard version lacks a PC arrow natively without breakpoints. 
        // Breakpoint draws a cool circle on the line number, which indicates the current PC cleanly!
    } else {
        editor.SetBreakpoints({});
    }

    ImGui::Separator();

    // To allow highlight under the text, we make the InputText Frame completely transparent
    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0, 0, 0, 0));
    
    // We get the current cursor to draw the highlight manually
    ImVec2 p = ImGui::GetCursorScreenPos();
    float lineHeight = ImGui::GetTextLineHeight();

    if (activeLine > 0) {
        ImDrawList* drawList = ImGui::GetWindowDrawList();
        float startY = p.y + (activeLine - 1) * lineHeight;
        
        drawList->AddRectFilled(
            ImVec2(p.x, startY + 2.0f),
            ImVec2(p.x + ImGui::GetContentRegionAvail().x, startY + lineHeight + 2.0f),
            IM_COL32(255, 216, 102, 60) // Transparent Yellow Monokai Accent
        );
    }

    editor.Render("##source");
    
    ImGui::PopStyleColor();

    ImGui::End();
}

} // namespace MIPS::UI
