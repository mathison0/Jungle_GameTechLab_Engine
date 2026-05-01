// Content Drawer 내부 콘텐츠를 구현합니다.
#include "Editor/UI/EditorContentDrawerPanel.h"

#include "ImGui/imgui.h"

void FEditorContentDrawerPanel::Render(float DeltaTime)
{
    (void)DeltaTime;

    ImGui::TextUnformatted("Content Drawer");
    ImGui::Separator();
    ImGui::TextDisabled("Asset/Content");
    ImGui::TextDisabled("Asset/Scripts");
}
