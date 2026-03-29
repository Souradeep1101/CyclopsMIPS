#include "UI_Widgets.h"
#include <imgui.h>

namespace MIPS::UI {

void DrawAboutDialog(bool* p_open, ImTextureID logoTexture) {
    if (!*p_open) return;

    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    
    if (ImGui::BeginPopupModal("About CyclopsMIPS", p_open, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
        
        // --- Header Section ---
        if (logoTexture) {
            ImGui::SetCursorPosX((ImGui::GetWindowSize().x - 128) * 0.5f);
            ImGui::Image(logoTexture, ImVec2(128, 128));
        }

        ImGui::Spacing();
        float windowWidth = ImGui::GetWindowSize().x;
        
        const char* title = "CyclopsMIPS";
        float titleWidth = ImGui::CalcTextSize(title).x;
        ImGui::SetCursorPosX((windowWidth - titleWidth) * 0.5f);
        ImGui::TextColored(ImVec4(0.266f, 0.847f, 0.945f, 1.0f), "%s", title); // Electric Cyan
        
        ImGui::SetCursorPosX((windowWidth - ImGui::CalcTextSize("Version 1.0.0 (V1-Stable)").x) * 0.5f);
        ImGui::TextDisabled("Version 1.0.0 (V1-Stable)");
        
        ImGui::Separator();
        
        // --- Mission Statement ---
        ImGui::PushTextWrapPos(400.0f);
        ImGui::TextWrapped("The one-eyed sentinel of MIPS architectural education. CyclopsMIPS provides a "
                           "high-precision, cycle-accurate simulation environment for undergraduate and "
                           "graduate computer system studies.");
        ImGui::PopTextWrapPos();
        
        ImGui::Spacing();
        ImGui::Separator();

        // --- Credits & Meta ---
        ImGui::Columns(2, nullptr, false);
        ImGui::SetColumnWidth(0, 100);
        
        ImGui::Text("Core Logic:");
        ImGui::NextColumn();
        ImGui::Text("Systems Architect");
        ImGui::NextColumn();

        ImGui::Text("UI Context:");
        ImGui::NextColumn();
        ImGui::Text("ImGui Engineering");
        ImGui::NextColumn();
        
        ImGui::Text("Build Date:");
        ImGui::NextColumn();
        ImGui::Text("2026-03-29");
        ImGui::Columns(1);

        ImGui::Separator();
        
        if (ImGui::Button("Close", ImVec2(120, 0))) {
            *p_open = false;
            ImGui::CloseCurrentPopup();
        }
        
        ImGui::EndPopup();
    }
}

} // namespace MIPS::UI
