#include "PIEToolbarWindow.h"
#include "imgui.h"
#include "IconsFontAwesome6.h"
#include "EditorEngine.h"

// 메뉴바 중앙에서 왼쪽으로 얼마나 이동할지 (양수 = 왼쪽)
static constexpr float CenterOffsetX = 40.0f;

void FPIEToolbarWindow::RenderInMenuBar(FEditorEngine* Engine)
{
	if (!Engine) return;

	const bool bPIE    = Engine->IsPIEActive();
	const bool bPaused = Engine->IsPIEPaused();

	const float Spacing = ImGui::GetStyle().ItemSpacing.x;
	const float Padding = ImGui::GetStyle().FramePadding.x * 2.0f;

	// Play 버튼은 아이콘 + " Play" 텍스트가 들어가므로 너비를 텍스트 기준으로 계산
	const char* PlayLabel  = ICON_FA_PLAY "  Play ";
	const float PlayBtnW   = ImGui::CalcTextSize(PlayLabel).x + Padding;
	const float IconBtnW   = 32.0f;   // Pause / Stop 아이콘 전용 버튼 너비

	const float TotalW  = bPIE ? (IconBtnW * 2.0f + Spacing) : PlayBtnW;
	const float MenuBarW = ImGui::GetWindowWidth();
	ImGui::SetCursorPosX((MenuBarW - TotalW) * 0.5f - CenterOffsetX);

	if (!bPIE)
	{
		ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.10f, 0.55f, 0.10f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.75f, 0.20f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.05f, 0.35f, 0.05f, 1.0f));
		if (ImGui::Button(PlayLabel, ImVec2(PlayBtnW, 0)))
			Engine->StartPIE();
		ImGui::PopStyleColor(3);
	}
	else
	{
		if (!bPaused)
		{
			ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.65f, 0.55f, 0.05f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.85f, 0.75f, 0.10f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.45f, 0.35f, 0.02f, 1.0f));
			if (ImGui::Button(ICON_FA_PAUSE, ImVec2(IconBtnW, 0)))
				Engine->PausePIE();
			ImGui::PopStyleColor(3);
		}
		else
		{
			ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.10f, 0.55f, 0.10f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.75f, 0.20f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.05f, 0.35f, 0.05f, 1.0f));
			if (ImGui::Button(ICON_FA_PLAY, ImVec2(IconBtnW, 0)))
				Engine->ResumePIE();
			ImGui::PopStyleColor(3);
		}

		ImGui::SameLine();

		// ■ Stop
		ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0.65f, 0.10f, 0.10f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.85f, 0.20f, 0.20f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive,  ImVec4(0.45f, 0.05f, 0.05f, 1.0f));
		if (ImGui::Button(ICON_FA_STOP, ImVec2(IconBtnW, 0)))
			Engine->StopPIE();
		ImGui::PopStyleColor(3);
	}
}
