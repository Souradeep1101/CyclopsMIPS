#include "UI_Widgets.h"
#include <imgui.h>
#include <cstdio>
#include <string>

namespace MIPS::UI {
    void DrawSignalsWidget(const CPU& cpu) {
        ImGui::Begin("Pipeline Signals");
        
        if (ImGui::BeginTable("SigTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
            ImGui::TableSetupColumn("Control Signal");
            ImGui::TableSetupColumn("Real-Time Value");
            ImGui::TableHeadersRow();

            // Extract latched IDs from ID/EX for primary control monitoring
            // Since MIPS signals propagate through pipeline registers, we show what's currently streaming in ID_EX as primary signals.
            const auto& ex = cpu.id_ex;
            
            auto drawSig = [&](const char* name, int val, bool isMemOrWb = false) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%s", name);
                
                ImGui::TableSetColumnIndex(1);
                // Data-Path Chromaticity mapping for Control (Primary Cyan) via the design specs
                ImVec4 col = ImVec4(0.266f, 0.847f, 0.945f, 1.0f); // #44D8F1 Cyan
                if (!ex.valid && !isMemOrWb) {
                   ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f), "Bubble");
                } else {
                   ImGui::TextColored(col, "%d", val);
                }
            };
            
            drawSig("RegDst", ex.regDst);
            drawSig("ALUSrc", ex.aluSrc);
            drawSig("MemToReg", cpu.ex_mem.memToReg, true); // Active in MEM -> WB
            drawSig("RegWrite", cpu.mem_wb.regWrite, true); // Active in WB
            drawSig("MemRead", cpu.ex_mem.memRead, true);   // Active in MEM
            drawSig("MemWrite", cpu.ex_mem.memWrite, true); // Active in MEM
            drawSig("ALUCtrl", static_cast<int>(ex.aluCtrl));

            ImGui::EndTable();
        }
        ImGui::End();
    }
}
