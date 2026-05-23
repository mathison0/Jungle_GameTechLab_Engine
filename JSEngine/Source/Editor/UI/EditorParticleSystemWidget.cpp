#include "Editor/UI/EditorParticleSystemWidget.h"

#include "ImGui/imgui.h"
#include "ImGui/imgui_internal.h"

#include <algorithm>

namespace
{
	constexpr float SplitterThickness = 5.0f;
	constexpr float PanelHeaderHeight = 24.0f;

	bool ToolbarButton(const char* Label, const char* Tooltip = nullptr, bool bSelected = false)
	{
		ImGui::PushStyleColor(
			ImGuiCol_Button,
			bSelected ? ImVec4(0.18f, 0.24f, 0.32f, 1.0f) : ImVec4(0.14f, 0.15f, 0.17f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.23f, 0.25f, 0.30f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.28f, 0.31f, 0.38f, 1.0f));
		const bool bClicked = ImGui::Button(Label, ImVec2(0.0f, 28.0f));
		ImGui::PopStyleColor(3);

		if (Tooltip && ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("%s", Tooltip);
		}
		return bClicked;
	}

	void SameLineGap(float Gap = 4.0f)
	{
		ImGui::SameLine(0.0f, Gap);
	}

	void DrawVerticalSplitter(const char* Id, float& LeftWidth, float TotalWidth, float MinLeft, float MinRight)
	{
		const ImVec2 Min = ImGui::GetCursorScreenPos();
		ImGui::InvisibleButton(Id, ImVec2(SplitterThickness, ImGui::GetContentRegionAvail().y));
		const ImVec2 Max = ImGui::GetItemRectMax();
		if (ImGui::IsItemActive())
		{
			LeftWidth += ImGui::GetIO().MouseDelta.x;
			LeftWidth = std::clamp(LeftWidth, MinLeft, std::max(MinLeft, TotalWidth - MinRight - SplitterThickness));
		}
		const bool bHot = ImGui::IsItemHovered() || ImGui::IsItemActive();
		ImGui::GetWindowDrawList()->AddRectFilled(
			Min,
			Max,
			ImGui::GetColorU32(bHot ? ImVec4(0.28f, 0.30f, 0.35f, 1.0f) : ImVec4(0.09f, 0.09f, 0.10f, 1.0f)));
		if (bHot)
		{
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
		}
	}

	void DrawHorizontalSplitter(const char* Id, float& TopHeight, float TotalHeight, float MinTop, float MinBottom)
	{
		const ImVec2 Min = ImGui::GetCursorScreenPos();
		ImGui::InvisibleButton(Id, ImVec2(ImGui::GetContentRegionAvail().x, SplitterThickness));
		const ImVec2 Max = ImGui::GetItemRectMax();
		if (ImGui::IsItemActive())
		{
			TopHeight += ImGui::GetIO().MouseDelta.y;
			TopHeight = std::clamp(TopHeight, MinTop, std::max(MinTop, TotalHeight - MinBottom - SplitterThickness));
		}
		const bool bHot = ImGui::IsItemHovered() || ImGui::IsItemActive();
		ImGui::GetWindowDrawList()->AddRectFilled(
			Min,
			Max,
			ImGui::GetColorU32(bHot ? ImVec4(0.28f, 0.30f, 0.35f, 1.0f) : ImVec4(0.09f, 0.09f, 0.10f, 1.0f)));
		if (bHot)
		{
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
		}
	}

	void DrawPanelHeader(const char* Title)
	{
		ImDrawList* DrawList = ImGui::GetWindowDrawList();
		const ImVec2 Min = ImGui::GetCursorScreenPos();
		const ImVec2 Max(Min.x + ImGui::GetContentRegionAvail().x, Min.y + PanelHeaderHeight);
		DrawList->AddRectFilled(Min, Max, ImGui::GetColorU32(ImVec4(0.15f, 0.15f, 0.15f, 1.0f)));
		DrawList->AddRect(Min, Max, ImGui::GetColorU32(ImVec4(0.07f, 0.07f, 0.07f, 1.0f)));
		ImGui::SetCursorScreenPos(ImVec2(Min.x + 8.0f, Min.y + 4.0f));
		ImGui::TextUnformatted(Title);
		ImGui::SetCursorScreenPos(ImVec2(Min.x, Max.y));
	}
}

void FEditorParticleSystemWidget::Initialize(UEditorEngine* InEditorEngine)
{
	FEditorWidget::Initialize(InEditorEngine);
}

void FEditorParticleSystemWidget::Render(float DeltaTime)
{
	RenderEmbedded(DeltaTime);
}

void FEditorParticleSystemWidget::RenderEmbedded(float DeltaTime)
{
	(void)DeltaTime;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.055f, 0.058f, 0.070f, 1.0f));
	DrawMainLayout();
	ImGui::PopStyleColor();
	ImGui::PopStyleVar(2);
}

void FEditorParticleSystemWidget::OpenLayoutTest(const FString& InDocumentPath)
{
	DocumentPath = InDocumentPath.empty() ? "P_Explosion_Big_B" : InDocumentPath;
	bDirty = true;
}

void FEditorParticleSystemWidget::RenderDocumentToolbarControls()
{
	if (ToolbarButton("[S]", "Save particle system"))
	{
		bDirty = false;
	}
	SameLineGap();
	ToolbarButton("[F]", "Find in Content Browser");

	SameLineGap(14.0f);
	ToolbarButton("Restart Sim", "Restart simulation");
	SameLineGap();
	ToolbarButton("Restart Level", "Restart preview level");

	SameLineGap(14.0f);
	ToolbarButton("Undo", "Undo");
	SameLineGap();
	ToolbarButton("Redo", "Redo");

	SameLineGap(14.0f);
	if (ToolbarButton("Thumbnail", "Toggle thumbnail preview", bShowThumbnail))
	{
		bShowThumbnail = !bShowThumbnail;
	}
	SameLineGap();
	if (ToolbarButton("Bounds", "Toggle bounds", bShowBounds))
	{
		bShowBounds = !bShowBounds;
	}
	SameLineGap();
	if (ToolbarButton("Origin Axis", "Toggle origin axis", bShowOriginAxis))
	{
		bShowOriginAxis = !bShowOriginAxis;
	}
	SameLineGap();
	ToolbarButton("Background Color", "Change preview background color");

	SameLineGap(14.0f);
	ToolbarButton("Regen LOD", "Regenerate LOD");
	SameLineGap();
	ToolbarButton("Lowest LOD", "Switch to lowest LOD");
	SameLineGap();
	ToolbarButton("Lower LOD", "Switch to lower LOD");
	SameLineGap();
	ToolbarButton("Add LOD", "Add LOD");

	SameLineGap(8.0f);
	ImGui::SetNextItemWidth(54.0f);
	ImGui::InputInt("LOD", &CurrentLOD, 0, 0);
	CurrentLOD = std::max(0, CurrentLOD);
	SameLineGap();
	ToolbarButton("Higher LOD", "Switch to higher LOD");

	SameLineGap(14.0f);
	ToolbarButton("Menu", "Particle editor menu");
}

void FEditorParticleSystemWidget::DrawMainLayout()
{
	const ImVec2 Available = ImGui::GetContentRegionAvail();
	if (Available.x <= 16.0f || Available.y <= 16.0f)
	{
		return;
	}

	if (TopAreaHeight <= 0.0f)
	{
		TopAreaHeight = Available.y * 0.54f;
	}
	if (TopLeftWidth <= 0.0f)
	{
		TopLeftWidth = Available.x * 0.41f;
	}
	if (BottomLeftWidth <= 0.0f)
	{
		BottomLeftWidth = Available.x * 0.41f;
	}

	TopAreaHeight = std::clamp(TopAreaHeight, 280.0f, std::max(280.0f, Available.y - 260.0f - SplitterThickness));
	TopLeftWidth = std::clamp(TopLeftWidth, 360.0f, std::max(360.0f, Available.x - 500.0f - SplitterThickness));
	BottomLeftWidth = std::clamp(BottomLeftWidth, 360.0f, std::max(360.0f, Available.x - 500.0f - SplitterThickness));

	const float TopRightWidth = std::max(1.0f, Available.x - TopLeftWidth - SplitterThickness);
	const float BottomHeight = std::max(1.0f, Available.y - TopAreaHeight - SplitterThickness);
	const float BottomRightWidth = std::max(1.0f, Available.x - BottomLeftWidth - SplitterThickness);

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));

	ImGui::BeginChild("##ParticleTopRow", ImVec2(Available.x, TopAreaHeight), false, ImGuiWindowFlags_NoScrollbar);
	{
		ImGui::BeginChild("##ParticleViewportPanel", ImVec2(TopLeftWidth, TopAreaHeight), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
		DrawViewportPanel(ImGui::GetContentRegionAvail());
		ImGui::EndChild();

		ImGui::SameLine(0.0f, 0.0f);
		DrawVerticalSplitter("##ParticleTopVerticalSplitter", TopLeftWidth, Available.x, 360.0f, 500.0f);
		ImGui::SameLine(0.0f, 0.0f);
		ImGui::BeginChild("##ParticleEmittersPanel", ImVec2(TopRightWidth, TopAreaHeight), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
		DrawEmittersPanel(ImGui::GetContentRegionAvail());
		ImGui::EndChild();
	}
	ImGui::EndChild();

	DrawHorizontalSplitter("##ParticleMainHorizontalSplitter", TopAreaHeight, Available.y, 280.0f, 260.0f);

	ImGui::BeginChild("##ParticleBottomRow", ImVec2(Available.x, BottomHeight), false, ImGuiWindowFlags_NoScrollbar);
	{
		ImGui::BeginChild("##ParticleDetailsPanel", ImVec2(BottomLeftWidth, BottomHeight), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
		DrawDetailsPanel(ImGui::GetContentRegionAvail());
		ImGui::EndChild();

		ImGui::SameLine(0.0f, 0.0f);
		DrawVerticalSplitter("##ParticleBottomVerticalSplitter", BottomLeftWidth, Available.x, 360.0f, 500.0f);
		ImGui::SameLine(0.0f, 0.0f);
		ImGui::BeginChild("##ParticleCurvePanel", ImVec2(BottomRightWidth, BottomHeight), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
		DrawCurveEditorPanel(ImGui::GetContentRegionAvail());
		ImGui::EndChild();
	}
	ImGui::EndChild();

	ImGui::PopStyleVar(2);
}

void FEditorParticleSystemWidget::DrawViewportPanel(const ImVec2& Size)
{
	DrawPanelHeader("Viewport");
	const ImVec2 CanvasMin = ImGui::GetCursorScreenPos();
	const ImVec2 CanvasMax(CanvasMin.x + Size.x, CanvasMin.y + std::max(1.0f, Size.y - PanelHeaderHeight));
	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	DrawList->AddRectFilled(CanvasMin, CanvasMax, ImGui::GetColorU32(ImVec4(0.34f, 0.35f, 0.34f, 1.0f)));
	DrawList->AddText(ImVec2(CanvasMin.x + 10.0f, CanvasMin.y + 10.0f), ImGui::GetColorU32(ImVec4(0.08f, 0.08f, 0.08f, 1.0f)), "View");
	DrawList->AddText(ImVec2(CanvasMin.x + 62.0f, CanvasMin.y + 10.0f), ImGui::GetColorU32(ImVec4(0.08f, 0.08f, 0.08f, 1.0f)), "Time");
	DrawList->AddText(ImVec2(CanvasMin.x + 8.0f, CanvasMax.y - 72.0f), ImGui::GetColorU32(ImVec4(0.90f, 0.90f, 0.90f, 1.0f)), "WARNING: This particle system has no fixed bounding box and contains a GPU emitter.");
	DrawList->AddLine(ImVec2(CanvasMin.x + 24.0f, CanvasMax.y - 24.0f), ImVec2(CanvasMin.x + 48.0f, CanvasMax.y - 24.0f), ImGui::GetColorU32(ImVec4(1.0f, 0.1f, 0.0f, 1.0f)), 1.5f);
	DrawList->AddLine(ImVec2(CanvasMin.x + 24.0f, CanvasMax.y - 24.0f), ImVec2(CanvasMin.x + 24.0f, CanvasMax.y - 48.0f), ImGui::GetColorU32(ImVec4(0.0f, 0.45f, 1.0f, 1.0f)), 1.5f);
	ImGui::Dummy(ImVec2(Size.x, std::max(1.0f, Size.y - PanelHeaderHeight)));
}

void FEditorParticleSystemWidget::DrawEmittersPanel(const ImVec2& Size)
{
	DrawPanelHeader("Emitters");
	ImGui::TextDisabled("Emitter column layout placeholder");
	ImGui::Text("Available: %.0f x %.0f", Size.x, Size.y);
}

void FEditorParticleSystemWidget::DrawDetailsPanel(const ImVec2& Size)
{
	DrawPanelHeader("Details");
	ImGui::TextDisabled("Details property table placeholder");
	ImGui::Text("Available: %.0f x %.0f", Size.x, Size.y);
}

void FEditorParticleSystemWidget::DrawCurveEditorPanel(const ImVec2& Size)
{
	DrawPanelHeader("Curve Editor");
	ImGui::TextDisabled("Curve editor toolbar and graph placeholder");
	ImGui::Text("Available: %.0f x %.0f", Size.x, Size.y);
}
