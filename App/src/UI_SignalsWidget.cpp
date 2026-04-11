#include "UI_Widgets.h"
#include <imgui.h>
#include <cstdio>
#include <string>
#include <vector>
#include <unordered_map>

namespace MIPS::UI {

void DrawSignalsWidget(const CPU& cpu, bool* p_open) {
    if (!*p_open) return;
    if (!ImGui::Begin("Pipeline Signals", p_open)) {
        ImGui::End();
        return;
    }

    // Transient Highlight Logic (Architect Note: Evaluated in ImGuiApp loop for global sync)

    if (ImGui::BeginTable("SigTable", 2,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
        ImGui::TableSetupColumn("Control Signal");
        ImGui::TableSetupColumn("Real-Time Value");
        ImGui::TableHeadersRow();

        const auto& ex = cpu.id_ex;

        // Each row carries a wire id for cross-panel sync
        struct Row { const char* name; const char* wireId; std::string valStr; bool valid; bool isHeader = false; };
        std::vector<Row> rows = {
            { "--- Basic Control ---",    "",               "",        true, true },
            { "RegDst",   "RegDst_wire",   std::to_string(ex.regDst),                               ex.valid },
            { "ALUSrc",   "ALUSrc_wire",   std::to_string(ex.aluSrc),                               ex.valid },
            { "Branch",   "Branch_wire",   std::to_string(ex.branch),                               ex.valid },
            { "MemToReg", "MemToReg_wire", std::to_string(cpu.ex_mem.memToReg),                     cpu.ex_mem.valid },
            { "RegWrite", "RegWrite_wire", std::to_string(cpu.mem_wb.regWrite),                     cpu.mem_wb.valid },
            { "MemRead",  "MemRead_wire",  std::to_string(cpu.ex_mem.memRead),                      cpu.ex_mem.valid },
            { "MemWrite", "MemWrite_wire", std::to_string(cpu.ex_mem.memWrite),                     cpu.ex_mem.valid },
            { "IsEqual",  "EqCmp_to_AND",  std::to_string(ex.isBranchTaken),                        ex.valid },
            { "PCSrc",    "PCSrc_wire",    std::to_string(ex.isBranchTaken && ex.branch),           ex.valid },

            { "--- Hazard & Reset ---",   "",               "",        true, true },
            { "Stall",    "HDU_stall",     ((cpu.hazardFlags & 0x1) ? "1" : "0"),                   true },
            { "PCWrite",  "HDU_pc_disable",((cpu.hazardFlags & 0x1) ? "0" : "1"),                   true },
            { "IF.Flush", "IF_Flush",      ((cpu.hazardFlags & 0x2) ? "1" : "0"),                   true },
            { "ID.Flush", "ID_Flush_Out",  ((cpu.hazardFlags & 0x1) ? "1" : "0"),                   true },

            { "--- Forwarding ---",       "",               "",        true, true },
            { "FwdA Mux", "FwdUnit_to_MuxA",std::to_string(cpu.forwardA),                           true },
            { "FwdB Mux", "FwdUnit_to_MuxB",std::to_string(cpu.forwardB),                           true },
            { "Rs (ID/EX)","IDEX_rs_to_Fwd", std::to_string(ex.rs),                                 ex.valid },
            { "Rt (ID/EX)","IDEX_rt_to_Fwd", std::to_string(ex.rt),                                 ex.valid },
            { "Rd (EX/MEM)","Fwd_EX_MEM",   std::to_string(cpu.ex_mem.destReg),                     cpu.ex_mem.valid },
            { "Rd (MEM/WB)","Fwd_MEM_WB",   std::to_string(cpu.mem_wb.destReg),                     cpu.mem_wb.valid },
        };

        static std::unordered_map<std::string, std::string> lastValues;

        for (auto& r : rows) {
            if (r.isHeader) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::TextDisabled("%s", r.name);
                ImGui::TableSetColumnIndex(1);
                ImGui::TextDisabled("---");
                continue;
            }

            bool isSelected = (!g_schematicSelection.selectedWireId.empty() &&
                               g_schematicSelection.selectedWireId == r.wireId);
            
            bool hasChanged = false;
            if (r.wireId[0] != '\0') {
               if (lastValues.count(r.name) && lastValues[r.name] != r.valStr) {
                   hasChanged = true;
               }
               lastValues[r.name] = r.valStr;
            }

            if (isSelected)
                ImGui::SetScrollHereY();

            ImGui::TableNextRow();
            if (isSelected) {
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1, IM_COL32(68, 216, 241, 60));
            } else if (hasChanged) {
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1, IM_COL32(255, 216, 102, 60)); // Yellow for change
            }

            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%s", r.name);

            ImGui::TableSetColumnIndex(1);
            if (!r.valid) {
                ImGui::TextColored(ImVec4(0.5f,0.5f,0.5f,1.0f), "Bubble");
            } else {
                ImGui::TextColored(ImVec4(0.266f, 0.847f, 0.945f, 1.0f), "%s", r.valStr.c_str());
            }

            if (ImGui::IsItemClicked()) {
                g_schematicSelection.selectedNodeId.clear();
                if (r.wireId[0] != '\0') {
                    g_schematicSelection.selectedWireId = r.wireId;
                    g_schematicSelection.selectionTime = ImGui::GetTime();
                }
            }
        }

        ImGui::EndTable();
    }
    ImGui::End();
}

} // namespace MIPS::UI