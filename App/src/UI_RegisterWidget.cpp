#include "Core/CPU.h"

#include <imgui.h>

namespace MIPS::UI {
    void DrawRegisterWidget(CPU& cpu) {
        ImGui::Begin("Registers");
        
        if (ImGui::BeginTable("RegTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
            ImGui::TableSetupColumn("Register");
            ImGui::TableSetupColumn("Value (Hex)");
            ImGui::TableHeadersRow();

            const auto& state = cpu.getState();
            for (int i = 0; i < 32; ++i) {
                bool isChanged = (i == (int)state.lastChangedReg);
                ImGui::TableNextRow();
                if (isChanged) {
                    ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1, IM_COL32(255, 216, 102, 80));
                }

                ImGui::TableSetColumnIndex(0);
                ImGui::Text("R%d", i);
                
                ImGui::TableSetColumnIndex(1);
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::PushID(i);
                int value = (int)state.regs[i];
                ImGui::InputInt("##val", &value, 0, 0, ImGuiInputTextFlags_CharsHexadecimal);
                if (ImGui::IsItemDeactivatedAfterEdit()) {
                    cpu.setRegister(i, (uint32_t)value);
                }
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        ImGui::End();
    }
}
