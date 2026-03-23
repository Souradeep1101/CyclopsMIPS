#include "Core/CPU.h"

#include <imgui.h>

namespace MIPS::UI {
    void DrawMemoryWidget(MemoryBus& bus) {
        ImGui::Begin("Memory (Data)");
        
        static int memOffset = 0;
        ImGui::InputInt("Address Offset", &memOffset, 4, 16, ImGuiInputTextFlags_CharsHexadecimal);
        if (memOffset < 0) memOffset = 0;

        if (ImGui::BeginTable("MemTable", 2, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY)) {
            ImGui::TableSetupColumn("Address");
            ImGui::TableSetupColumn("Value (Hex)");
            ImGui::TableHeadersRow();

            for (int i = 0; i < 32; ++i) { // Show 32 words
                uint32_t addr = memOffset + (i * 4);
                uint32_t val = bus.readWordDirect(addr);
                
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0);
                ImGui::Text("0x%08X", addr);
                
                ImGui::TableSetColumnIndex(1);
                ImGui::SetNextItemWidth(-FLT_MIN);
                ImGui::PushID(addr);
                int value = (int)val;
                ImGui::InputInt("##val", &value, 0, 0, ImGuiInputTextFlags_CharsHexadecimal);
                if (ImGui::IsItemDeactivatedAfterEdit()) {
                    bus.writeWord(addr, (uint32_t)value);
                }
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        ImGui::End();
    }
}
