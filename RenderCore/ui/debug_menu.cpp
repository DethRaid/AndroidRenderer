#include "debug_menu.hpp"

#include <imgui.h>

#include "console/cvars.hpp"

void DebugMenu::draw() {
    if(!ImGui::Begin("Debug", &is_open)) {
        ImGui::End();
        return;
    }

    auto cvars = CVarSystem::Get();

    ImGui::Text("cvars");

    cvars->DrawImguiEditor();

    ImGui::End();
}
