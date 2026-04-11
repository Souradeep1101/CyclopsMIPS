#include "UI_Widgets.h"
#include "Core/Logger.h"
#include <imgui.h>
#include <vector>
#include <string>

namespace MIPS::UI {

void DrawTerminalWidget(bool* p_open, std::vector<TerminalInstance>& terminals, CPU& cpu, bool* stepRequested) {
    if (!*p_open) return;

    ImGui::Begin("Terminal", p_open);

    if (ImGui::BeginTabBar("TerminalTabs")) {
        // Tab 1: Emulation Log (The Diagnostics channel)
        if (ImGui::BeginTabItem("Emulation Log")) {
            auto entries = Core::Logger::Get().GetEntries(Core::Logger::Channel::Emulation);
            ImGui::BeginChild("LogScroll", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);
            for (const auto& log : entries) {
                ImGui::TextUnformatted(log.c_str());
            }
            if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
                ImGui::SetScrollHereY(1.0f);
            ImGui::EndChild();
            ImGui::EndTabItem();
        }

        // Tab 2: MIPS Console (The Syscall channel)
        if (ImGui::BeginTabItem("MIPS Console")) {
            auto entries = Core::Logger::Get().GetEntries(Core::Logger::Channel::Console);
            
            float input_height = 0.0f;
            if (cpu.waitingForInput) {
                input_height = ImGui::GetTextLineHeightWithSpacing() + 10;
            }

            ImGui::BeginChild("ConsoleScroll", ImVec2(0, -input_height), false, ImGuiWindowFlags_HorizontalScrollbar);
            for (const auto& log : entries) {
                ImGui::TextUnformatted(log.c_str());
            }
            if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
                ImGui::SetScrollHereY(1.0f);
            ImGui::EndChild();

            if (cpu.waitingForInput) {
                ImGui::Separator();
                ImGui::Text("Input Required (Enter Integer):");
                ImGui::SameLine();
                static char inputBuf[32] = "";
                if (ImGui::InputText("##ConsoleInput", inputBuf, 32, ImGuiInputTextFlags_EnterReturnsTrue)) {
                    cpu.inputBuffer = (uint32_t)std::atoi(inputBuf);
                    cpu.waitingForInput = false;
                    inputBuf[0] = '\0';
                }
                ImGui::SetKeyboardFocusHere(-1);
            }
            ImGui::EndTabItem();
        }

        // Tab 3+: Dynamic CLI instances
        for (int i = 0; i < (int)terminals.size(); i++) {
            TerminalInstance& term = terminals[i];
            if (!term.open) continue;

            if (ImGui::BeginTabItem(term.name.c_str(), &term.open)) {
                ImGui::BeginChild("CLIScroll", ImVec2(0, -ImGui::GetTextLineHeightWithSpacing() - 10));
                for (const auto& line : term.history) {
                    ImGui::TextUnformatted(line.c_str());
                }
                ImGui::EndChild();

                ImGui::Separator();
                if (ImGui::InputText("##CLIInput", term.inputBuffer, 256, ImGuiInputTextFlags_EnterReturnsTrue)) {
                    term.history.push_back("> " + std::string(term.inputBuffer));
                    term.history.push_back("Command execution placeholder.");
                    term.inputBuffer[0] = '\0';
                }
                ImGui::EndTabItem();
            }
        }

        // Tab: Execution Control (High-frequency debug access)
        if (ImGui::BeginTabItem("Execution Control")) {
            ImGui::Spacing();
            ImGui::TextUnformatted("Cycle-Stepping Diagnostics:");
            ImGui::Separator();
            ImGui::Spacing();

            if (ImGui::Button("Step (1 Cycle)", ImVec2(-1, 50))) {
                if (stepRequested) *stepRequested = true;
            }
            ImGui::TextWrapped("Use this to advance the processor state while monitoring the logs above.");
            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::End();
}

} // namespace MIPS::UI
