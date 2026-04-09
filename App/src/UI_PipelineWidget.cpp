#include "UI_Widgets.h"
#include "Core/CPU.h"
#include "Assembler/Assembler.h"
#include <imgui.h>
#include <deque>
#include <iostream>

struct PipelineSnapshot {
    uint64_t cycle;
    std::string if_stage;
    std::string id_stage;
    std::string ex_stage;
    std::string mem_stage;
    std::string wb_stage;
    std::string notes;
};

static std::deque<PipelineSnapshot> history;
static uint64_t last_recorded_cycle = ~(0ULL);

namespace MIPS::UI {
    void DrawPipelineWidget(const CPU& cpu, const AssembledProgram* program, bool* p_open) {
        if (!*p_open) return;
        if (!ImGui::Begin("Pipeline Datapath", p_open)) {
            ImGui::End();
            return;
        }
        uint64_t current_cycle = cpu.getState().perf.cycles;

        // Reset history if CPU was reset or we travel back in time
        if (current_cycle < last_recorded_cycle && current_cycle == 0) {
            history.clear();
            last_recorded_cycle = ~(0ULL);
        }

        auto getMnemonic = [&](uint32_t pc) -> std::string {
        // 1. Check if the program pointer is even arriving at the UI
        if (!program) {
            std::cout << "[UI DEBUG] FATAL: program pointer is NULL!" << std::endl;
            return ""; // Returning empty string fixes the colored dash UI bug
        }
        
        // 2. Standard Word Index
        uint32_t wordIndex = pc / 4;
        
        // 3. MIPS Latch Offset (Latches often hold PC + 4)
        uint32_t latchIndex = pc >= 4 ? (pc - 4) / 4 : ~(0u);
        
        std::cout << "[UI DEBUG] Lookup -> PC: 0x" << std::hex << pc 
                  << " | Word: " << std::dec << wordIndex 
                  << " | Latch Word: " << latchIndex << std::endl;

        if (program->sourceMap.count(wordIndex)) {
            return program->sourceMap.at(wordIndex).mnemonic;
        }
        if (latchIndex != ~(0u) && program->sourceMap.count(latchIndex)) {
            return program->sourceMap.at(latchIndex).mnemonic;
        }
        
        return ""; // Returning empty string fixes the colored dash UI bug
    };

        // Record new cycle snapshot if emulator stepped!
        if (current_cycle > last_recorded_cycle || (current_cycle == 0 && history.empty())) {
            PipelineSnapshot snap;
            snap.cycle = current_cycle;
            
            // FIX 2 (UI Side): Show fetched instruction even if flushed this cycle
            bool isFlushedThisCycle = (cpu.hazardFlags & 0x2) != 0;
            snap.if_stage = (cpu.if_id.valid || isFlushedThisCycle) ? getMnemonic(cpu.if_id.pc) : "";
            
            snap.id_stage = cpu.id_ex.valid ? getMnemonic(cpu.id_ex.pc) : "";
            snap.ex_stage = cpu.ex_mem.valid ? getMnemonic(cpu.ex_mem.pc) : "";
            snap.mem_stage = cpu.mem_wb.valid ? getMnemonic(cpu.mem_wb.pc) : "";
            
            // FIX 1: WB stage must read from the latch snapshot BEFORE memoryAccess overwrote it
            snap.wb_stage = cpu.mem_wb_old.valid ? getMnemonic(cpu.mem_wb_old.pc) : "";

            // Enhanced Educational Notes (Hazards & Branching)
            if (cpu.hazardFlags & 0x1) {
                snap.notes = "STALL (Load-Use)";
            } else if (cpu.hazardFlags & 0x2) {
                snap.notes = "FLUSH (Mispredict)";
            } else if (cpu.id_ex.branch && cpu.id_ex.valid) {
                if (cpu.id_ex.isBranchTaken) snap.notes = "Branch Taken";
                else snap.notes = "Branch Not Taken";
            }

            history.push_back(snap);
            last_recorded_cycle = current_cycle;
            
            // Limit history size to prevent infinite memory leak during long plays
            if (history.size() > 1000) {
                // Remove first 500 (O(1) in deque)
                history.erase(history.begin(), history.begin() + 500);
            }
        }

        // --- Render Table ---
        if (ImGui::BeginTable("ExecutionTable", 7, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
            ImGui::TableSetupScrollFreeze(0, 1); // Freeze header
            ImGui::TableSetupColumn("Cycle", ImGuiTableColumnFlags_WidthFixed, 50.0f);
            ImGui::TableSetupColumn("IF", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("ID", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("EX", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("MEM", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("WB", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableSetupColumn("Notes", ImGuiTableColumnFlags_WidthStretch);
            ImGui::TableHeadersRow();

            // Render all historical records
            for (const auto& snap : history) {
                ImGui::TableNextRow();
                
                ImGui::TableNextColumn(); ImGui::Text("%llu", static_cast<unsigned long long>(snap.cycle));
                
                // Active highlighting using standard ImGui Color text for stages
                // IF
                ImGui::TableNextColumn();
                if (!snap.if_stage.empty()) ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "%s", snap.if_stage.c_str());
                else ImGui::TextDisabled("-");

                // ID
                ImGui::TableNextColumn();
                if (!snap.id_stage.empty()) ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.6f, 1.0f), "%s", snap.id_stage.c_str());
                else ImGui::TextDisabled("-");

                // EX
                ImGui::TableNextColumn();
                if (!snap.ex_stage.empty()) ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.4f, 1.0f), "%s", snap.ex_stage.c_str());
                else ImGui::TextDisabled("-");

                // MEM
                ImGui::TableNextColumn();
                if (!snap.mem_stage.empty()) ImGui::TextColored(ImVec4(0.8f, 0.4f, 1.0f, 1.0f), "%s", snap.mem_stage.c_str());
                else ImGui::TextDisabled("-");

                // WB
                ImGui::TableNextColumn();
                if (!snap.wb_stage.empty()) ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", snap.wb_stage.c_str());
                else ImGui::TextDisabled("-");

                // Notes
                ImGui::TableNextColumn(); 
                if (!snap.notes.empty()) ImGui::TextColored(ImVec4(1.0f, 0.2f, 0.2f, 1.0f), "%s", snap.notes.c_str());
            }
            
            // Auto-scroll to bottom if at maximum scroll
            if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
                ImGui::SetScrollHereY(1.0f);

            ImGui::EndTable();
        }

        ImGui::End();
    }
}