#include "UI_Widgets.h"
#include <imgui.h>
#include <cstdio>
#include <string>

namespace MIPS::UI {

void DrawSignalsWidget(const CPU& cpu) {
    ImGui::Begin("Pipeline Signals");

    if (ImGui::BeginTable("SigTable", 2,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
        ImGui::TableSetupColumn("Control Signal");
        ImGui::TableSetupColumn("Real-Time Value");
        ImGui::TableHeadersRow();

        const auto& ex = cpu.id_ex;

        // Each row carries a wire id for cross-panel sync
        struct Row { const char* name; const char* wireId; int val; bool valid; };
        Row rows[] = {
            { "RegDst",   "RegDst_wire",   ex.regDst,                               ex.valid },
            { "ALUSrc",   "ALUSrc_wire",   ex.aluSrc,                               ex.valid },
            { "MemToReg", "MemToReg_wire", cpu.ex_mem.memToReg,                     cpu.ex_mem.valid },
            { "RegWrite", "RegWrite_wire", cpu.mem_wb.regWrite,                     cpu.mem_wb.valid },
            { "MemRead",  "MemRead_wire",  cpu.ex_mem.memRead,                      cpu.ex_mem.valid },
            { "MemWrite", "MemWrite_wire", cpu.ex_mem.memWrite,                     cpu.ex_mem.valid },
            { "ALUCtrl",  "",              static_cast<int>(ex.aluCtrl),            ex.valid },
        };

        for (auto& r : rows) {
            bool isSelected = (!g_schematicSelection.selectedWireId.empty() &&
                               g_schematicSelection.selectedWireId == r.wireId);

            // If this row is selected from the diagram, auto-scroll to it
            if (isSelected)
                ImGui::SetScrollHereY();

            ImGui::TableNextRow();

            // Highlight the selected row
            if (isSelected) {
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1,
                    IM_COL32(68, 216, 241, 40));
            }

            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%s", r.name);

            ImGui::TableSetColumnIndex(1);
            ImVec4 col = ImVec4(0.266f, 0.847f, 0.945f, 1.0f);  // Cyan
            if (!r.valid) {
                ImGui::TextColored(ImVec4(0.5f,0.5f,0.5f,1.0f), "Bubble");
            } else {
                ImGui::TextColored(col, "%d", r.val);
            }

            // Click row → select corresponding wire in diagram
            if (ImGui::IsItemClicked() && r.wireId[0] != '\0') {
                g_schematicSelection.selectedNodeId.clear();
                g_schematicSelection.selectedWireId = r.wireId;
            }
        }

        ImGui::EndTable();
    }
    ImGui::End();
}

} // namespace MIPS::UI
