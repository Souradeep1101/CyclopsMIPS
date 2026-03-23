#include "UI_Widgets.h"
#include <imgui.h>

namespace MIPS::UI {

void DrawArchitectureWidget(const CPU& cpu) {
    ImGui::Begin("MIPS CPU Architecture Diagram");

    ImVec2 p = ImGui::GetCursorScreenPos();
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    ImVec2 avail = ImGui::GetContentRegionAvail();

    // Determine sizing
    float width = avail.x * 0.15f;
    float height = avail.y * 0.6f;
    float startY = p.y + (avail.y * 0.2f);
    float startX = p.x + (avail.x * 0.05f);
    float gap = avail.x * 0.04f;

    const char* stages[] = {"Instruction\nFetch (IF)", "Instruction\nDecode (ID)", "Execute\n(EX)", "Memory\n(MEM)", "Write Back\n(WB)"};
    
    // Draw the 5 main pipeline blocks
    for (int i = 0; i < 5; ++i) {
        ImVec2 rectMin = ImVec2(startX + i * (width + gap), startY);
        ImVec2 rectMax = ImVec2(rectMin.x + width, rectMin.y + height);

        // Background Box (The Plateau)
        drawList->AddRectFilled(rectMin, rectMax, IM_COL32(41, 42, 45, 255), 0.0f);
        // Border (Ghost Border Fallback - 15% Outline Variant)
        drawList->AddRect(rectMin, rectMax, IM_COL32(150, 150, 160, 38), 0.0f, 0, 1.0f);

        // Centered Text (Primary Headline)
        ImVec2 textSize = ImGui::CalcTextSize(stages[i]);
        drawList->AddText(ImVec2(rectMin.x + (width - textSize.x) * 0.5f, rectMin.y + 20.0f), 
                          IM_COL32(230, 230, 235, 255), stages[i]);

        // Draw active data payloads inside the boxes based on CPU latches
        float dataY = rectMin.y + 80.0f;
        auto drawData = [&](const char* text, ImU32 col) {
            drawList->AddText(ImVec2(rectMin.x + 10.0f, dataY), col, text);
            dataY += 20.0f;
        };

        char buf[64];
        if (i == 0) {
            snprintf(buf, sizeof(buf), "PC: 0x%08X", cpu.getState().pc);
            drawData(buf, IM_COL32(120, 220, 119, 255)); // Tertiary (Execution Flow)
            if (cpu.if_id.valid) {
                snprintf(buf, sizeof(buf), "Instr: 0x%08X", cpu.if_id.instr.data);
                drawData(buf, IM_COL32(255, 215, 153, 255)); // Secondary (Data)
            }
        } else if (i == 1 && cpu.id_ex.valid) {
            snprintf(buf, sizeof(buf), "Reg1: %d", cpu.id_ex.regData1);
            drawData(buf, IM_COL32(68, 216, 241, 255)); // Primary (Control/Registers)
            snprintf(buf, sizeof(buf), "Reg2: %d", cpu.id_ex.regData2);
            drawData(buf, IM_COL32(68, 216, 241, 255));
        } else if (i == 2 && cpu.ex_mem.valid) {
            snprintf(buf, sizeof(buf), "ALU Out: %d", (int)cpu.ex_mem.aluResult);
            drawData(buf, IM_COL32(255, 215, 153, 255)); // Secondary (Data)
        } else if (i == 3 && cpu.mem_wb.valid) {
            snprintf(buf, sizeof(buf), "MemData: %d", (int)cpu.mem_wb.readData);
            drawData(buf, IM_COL32(255, 215, 153, 255)); // Secondary (Data)
        } else if (i == 4 && cpu.mem_wb.valid && cpu.mem_wb.regWrite) {
            snprintf(buf, sizeof(buf), "Write: R%d", cpu.mem_wb.destReg);
            drawData(buf, IM_COL32(120, 220, 119, 255)); // Tertiary (Execution Flow mapping)
        }

        // Draw pipeline registers (the bars between blocks - The Trench)
        if (i < 4) {
            float regX = rectMax.x + (gap / 2.0f) - 5.0f;
            drawList->AddRectFilled(ImVec2(regX, startY - 20.0f), 
                                    ImVec2(regX + 10.0f, rectMax.y + 20.0f), 
                                    IM_COL32(13, 14, 17, 255)); // #0D0E11
        }
    }

    ImGui::End();
}

}
