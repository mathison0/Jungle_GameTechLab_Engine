#include "Editor/UI/EditorParticleSystemWidget.h"

#include "Editor/EditorEngine.h"
#include "Editor/UI/EditorDetachedWindowChrome.h"
#include "Editor/UI/EditorMainPanelViewportToolbarHelpers.h"
#include "Engine/Runtime/WindowsWindow.h"
#include "GameFramework/PrimitiveActors.h"
#include "Component/PostProcess/Light/AmbientLightComponent.h"

#include "ImGui/imgui.h"
#include "ImGui/imgui_internal.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace
{
	constexpr float SplitterThickness = 5.0f;
	constexpr float PanelHeaderHeight = 24.0f;

	void SetOpaqueBlendStateCallback(const ImDrawList*, const ImDrawCmd* Cmd)
	{
		ID3D11DeviceContext* DeviceContext = static_cast<ID3D11DeviceContext*>(Cmd->UserCallbackData);
		if (!DeviceContext)
		{
			return;
		}

		const float BlendFactor[4] = { 0.f, 0.f, 0.f, 0.f };
		DeviceContext->OMSetBlendState(nullptr, BlendFactor, 0xffffffff);
	}

	bool UsesAbsoluteImGuiCoordinates()
	{
		return (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable) != 0;
	}

	POINT ImGuiScreenToClientPoint(FWindowsWindow* Window, const ImVec2& Point)
	{
		POINT Result =
		{
			static_cast<LONG>(std::lround(Point.x)),
			static_cast<LONG>(std::lround(Point.y))
		};
		if (Window && Window->GetHWND() && UsesAbsoluteImGuiCoordinates())
		{
			::ScreenToClient(Window->GetHWND(), &Result);
		}
		return Result;
	}

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

	void DrawViewportModeItem(const char* Label, EViewMode Mode, FSceneViewport& Viewport)
	{
		FEditorViewportState& State = Viewport.GetState();
		if (ImGui::MenuItem(Label, nullptr, State.ViewMode == Mode))
		{
			State.ViewMode = Mode;
		}
	}

	float SmoothStep(float Edge0, float Edge1, float Value)
	{
		const float T = std::clamp((Value - Edge0) / (Edge1 - Edge0), 0.0f, 1.0f);
		return T * T * (3.0f - 2.0f * T);
	}

	void DrawAxisLabel(
		ImDrawList* DrawList,
		const ImVec2& AxisEnd,
		const ImVec2& AxisDirection,
		const char* Label,
		const ImVec4& BaseColor,
		float Alpha)
	{
		const ImVec2 TextSize = ImGui::CalcTextSize(Label);
		const ImVec2 LabelCenter(
			AxisEnd.x + AxisDirection.x * 7.0f,
			AxisEnd.y + AxisDirection.y * 7.0f);
		const ImVec2 TextPosition(
			LabelCenter.x - TextSize.x * 0.5f,
			LabelCenter.y - TextSize.y * 0.5f);

		const ImU32 ShadowColor = ImGui::GetColorU32(ImVec4(0.02f, 0.02f, 0.02f, 0.95f * Alpha));
		const ImU32 TextColor = ImGui::GetColorU32(ImVec4(BaseColor.x, BaseColor.y, BaseColor.z, BaseColor.w * Alpha));
		DrawList->AddText(ImVec2(TextPosition.x + 1.0f, TextPosition.y + 1.0f), ShadowColor, Label);
		DrawList->AddText(TextPosition, TextColor, Label);
	}

	void DrawViewportOrientationAxis(
		ImDrawList* DrawList,
		const ImVec2& CanvasMin,
		const ImVec2& CanvasMax,
		const FViewportCamera* Camera)
	{
		if (!DrawList || !Camera)
		{
			return;
		}

		const FVector CameraForward = Camera->GetForwardVector().GetSafeNormal();
		const FVector CameraRight = Camera->GetRightVector().GetSafeNormal();
		const FVector CameraUp = Camera->GetUpVector().GetSafeNormal();

		struct FAxisDrawItem
		{
			const char* Label;
			FVector Axis;
			ImVec4 Color;
			ImVec2 End;
			ImVec2 Direction;
			float Alpha;
			float Depth;
		};

		const ImVec2 Origin(CanvasMin.x + 46.0f, CanvasMax.y - 46.0f);
		constexpr float AxisLength = 28.0f;
		constexpr float MinProjectedLength = 0.35f;
		constexpr float LabelFadeStart = 0.10f;
		constexpr float LabelFadeEnd = 0.28f;

		std::array<FAxisDrawItem, 3> Axes =
		{ {
			{ "X", FVector::XAxisVector, ImVec4(0.95f, 0.12f, 0.04f, 1.0f), Origin, ImVec2(1.0f, 0.0f), 1.0f, 0.0f },
			{ "Y", FVector::YAxisVector, ImVec4(0.42f, 0.86f, 0.12f, 1.0f), Origin, ImVec2(0.0f, 1.0f), 1.0f, 0.0f },
			{ "Z", FVector::ZAxisVector, ImVec4(0.10f, 0.45f, 1.0f, 1.0f), Origin, ImVec2(0.0f, -1.0f), 1.0f, 0.0f }
		} };

		for (FAxisDrawItem& Item : Axes)
		{
			const float ScreenX = FVector::DotProduct(Item.Axis, CameraRight);
			const float ScreenY = -FVector::DotProduct(Item.Axis, CameraUp);
			const float ProjectedLength = std::sqrt(ScreenX * ScreenX + ScreenY * ScreenY);
			Item.Alpha = SmoothStep(LabelFadeStart, LabelFadeEnd, ProjectedLength);
			if (ProjectedLength > 1.0e-4f)
			{
				Item.Direction = ImVec2(ScreenX / ProjectedLength, ScreenY / ProjectedLength);
				const float VisualLength = AxisLength * std::clamp(ProjectedLength, MinProjectedLength, 1.0f);
				Item.End = ImVec2(
					Origin.x + Item.Direction.x * VisualLength,
					Origin.y + Item.Direction.y * VisualLength);
			}
			Item.Depth = FVector::DotProduct(Item.Axis, CameraForward);
		}

		std::sort(Axes.begin(), Axes.end(), [](const FAxisDrawItem& A, const FAxisDrawItem& B)
		{
			return A.Depth > B.Depth;
		});

		const ImU32 ShadowColor = ImGui::GetColorU32(ImVec4(0.02f, 0.02f, 0.02f, 0.95f));
		for (const FAxisDrawItem& Item : Axes)
		{
			if (Item.Alpha <= 0.01f)
			{
				continue;
			}

			const ImU32 AxisColor = ImGui::GetColorU32(ImVec4(Item.Color.x, Item.Color.y, Item.Color.z, Item.Color.w * Item.Alpha));
			DrawList->AddLine(Origin, Item.End, ShadowColor, 3.0f);
			DrawList->AddLine(Origin, Item.End, AxisColor, 1.6f);
			DrawAxisLabel(DrawList, Item.End, Item.Direction, Item.Label, Item.Color, Item.Alpha);
		}

		DrawList->AddCircleFilled(Origin, 2.4f, ShadowColor, 12);
		DrawList->AddCircleFilled(Origin, 1.6f, ImGui::GetColorU32(ImVec4(0.84f, 0.84f, 0.84f, 1.0f)), 12);
	}
}

FEditorParticleSystemWidget::~FEditorParticleSystemWidget()
{
	ShutdownPreviewViewport();
}

void FEditorParticleSystemWidget::Initialize(UEditorEngine* InEditorEngine)
{
	FEditorWidget::Initialize(InEditorEngine);
}

void FEditorParticleSystemWidget::Shutdown()
{
	ShutdownPreviewViewport();
}

void FEditorParticleSystemWidget::Render(float DeltaTime)
{
	RenderEmbedded(DeltaTime);
}

void FEditorParticleSystemWidget::RenderEmbedded(float DeltaTime)
{
	(void)DeltaTime;

	EnsurePreviewViewport();
	bPreviewViewportVisible = false;
	bPreviewViewportRectValid = false;

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
	bDirty = InDocumentPath.empty();
	EnsurePreviewViewport();
}

void FEditorParticleSystemWidget::EnsurePreviewViewport()
{
	if (bPreviewViewportInitialized || !EditorEngine)
	{
		return;
	}

	static int32 PreviewCounter = 0;
	const FString HandleText = "__ParticleSystemPreview_" + std::to_string(PreviewCounter++);
	PreviewWorldHandle = FName(HandleText.c_str());

	FWorldContext& PreviewContext = EditorEngine->CreateWorldContext(
		EWorldType::ViewerPreview,
		PreviewWorldHandle,
		"Particle System Preview");
	EditorEngine->ApplySpatialIndexMaintenanceSettings(PreviewContext.World);

	PreviewViewport.SetClient(&PreviewClient);
	PreviewClient.Initialize(EditorEngine->GetWindow(), EditorEngine);
	PreviewClient.SetWorld(PreviewContext.World);
	PreviewClient.SetGizmo(PreviewContext.SelectionManager ? PreviewContext.SelectionManager->GetGizmo() : nullptr);
	PreviewClient.SetSelectionManager(PreviewContext.SelectionManager);
	PreviewClient.SetSceneEditingShortcutsEnabled(false);
	PreviewClient.SetViewport(&PreviewViewport);
	PreviewClient.SetState(&PreviewViewport.GetState());
	PreviewClient.SetViewportType(EEditorViewportType::EVT_Perspective);
	PreviewClient.CreateCamera();
	PreviewClient.ApplyCameraMode();

	PreviewViewport.GetState().ViewMode = EViewMode::Lit_BlinnPhong;
	PreviewViewport.GetState().LightCullMode = ELightCullMode::None;

	const FViewportRect InitialRect(0, 0, 300, 300);
	PreviewViewport.SetRect(InitialRect);
	PreviewClient.SetViewportSize(static_cast<float>(InitialRect.Width), static_cast<float>(InitialRect.Height));

	if (UWorld* PreviewWorld = PreviewContext.World)
	{
		ADirectionalLightActor* DirectionalLight = PreviewWorld->SpawnActor<ADirectionalLightActor>();
		if (DirectionalLight)
		{
			DirectionalLight->InitDefaultComponents();
			DirectionalLight->SetFName(FName("Particle Preview Directional Light"));
			DirectionalLight->SetActorLocation(FVector(100000.0f, 100000.0f, 100000.0f));
			DirectionalLight->SetActorRotation(FVector(0.0f, 44.0f, 0.0f));
		}

		AAmbientLightActor* AmbientLight = PreviewWorld->SpawnActor<AAmbientLightActor>();
		if (AmbientLight)
		{
			AmbientLight->InitDefaultComponents();
			AmbientLight->SetFName(FName("Particle Preview Ambient Light"));
			AmbientLight->SetActorLocation(FVector(100000.0f, 100000.0f, 100000.0f));
			if (UAmbientLightComponent* AmbientComp = AmbientLight->FindComponent<UAmbientLightComponent>())
			{
				AmbientComp->Intensity = 0.7f;
			}
		}

		PreviewWorld->SyncSpatialIndex();
	}

	bPreviewViewportInitialized = true;
}

void FEditorParticleSystemWidget::ShutdownPreviewViewport()
{
	bPreviewViewportVisible = false;
	bPreviewViewportRectValid = false;

	PreviewClient.DestroyCamera();
	PreviewClient.SetWorld(nullptr);
	PreviewViewport.SetClient(nullptr);
	PreviewViewport.SetRenderTargetSet(nullptr);

	if (EditorEngine && PreviewWorldHandle != FName::None && EditorEngine->GetWorldContextFromHandle(PreviewWorldHandle))
	{
		EditorEngine->UnregisterWorld(PreviewWorldHandle);
	}

	PreviewWorldHandle = FName::None;
	bPreviewViewportInitialized = false;
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
		if (bPreviewViewportInitialized)
		{
			PreviewClient.GetParticleShowFlags().bBounds = bShowBounds;
		}
	}
	SameLineGap();
	if (ToolbarButton("Origin Axis", "Toggle origin axis", bShowOriginAxis))
	{
		bShowOriginAxis = !bShowOriginAxis;
		if (bPreviewViewportInitialized)
		{
			PreviewClient.GetParticleShowFlags().bAxis = bShowOriginAxis;
		}
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

void FEditorParticleSystemWidget::RenderDetachedDocumentChrome(bool& bCloseRequested)
{
	FEditorDetachedWindowChrome::RenderMenuBar(
		"Particle System Editor",
		"ParticleSystemEditor",
		[this]()
		{
			if (ImGui::BeginMenu("File"))
			{
				if (ImGui::MenuItem(bDirty ? "Save *" : "Save", "Ctrl+S"))
				{
					bDirty = false;
				}
				ImGui::MenuItem("Save As...", nullptr, false, false);
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Edit"))
			{
				ImGui::MenuItem("Undo", "Ctrl+Z", false, false);
				ImGui::MenuItem("Redo", "Ctrl+Shift+Z", false, false);
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("View"))
			{
				if (ImGui::MenuItem("Thumbnail", nullptr, bShowThumbnail))
				{
					bShowThumbnail = !bShowThumbnail;
				}
				if (ImGui::MenuItem("Bounds", nullptr, bShowBounds))
				{
					bShowBounds = !bShowBounds;
					if (bPreviewViewportInitialized)
					{
						PreviewClient.GetParticleShowFlags().bBounds = bShowBounds;
					}
				}
				if (ImGui::MenuItem("Origin Axis", nullptr, bShowOriginAxis))
				{
					bShowOriginAxis = !bShowOriginAxis;
					if (bPreviewViewportInitialized)
					{
						PreviewClient.GetParticleShowFlags().bAxis = bShowOriginAxis;
					}
				}
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Particle"))
			{
				ImGui::MenuItem("Restart Simulation", nullptr, false, false);
				ImGui::MenuItem("Restart Level", nullptr, false, false);
				ImGui::Separator();
				ImGui::MenuItem("Regenerate LOD", nullptr, false, false);
				ImGui::MenuItem("Add LOD", nullptr, false, false);
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Window"))
			{
				if (ImGui::MenuItem("Reset Layout"))
				{
					TopAreaHeight = 0.0f;
					TopLeftWidth = 0.0f;
					BottomLeftWidth = 0.0f;
				}
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("Help"))
			{
				ImGui::TextDisabled("Particle System Editor");
				if (!DocumentPath.empty())
				{
					ImGui::Separator();
					ImGui::TextDisabled("%s", DocumentPath.c_str());
				}
				ImGui::EndMenu();
			}
		},
		bCloseRequested);
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
	EnsurePreviewViewport();

	const ImVec2 CanvasSize(Size.x, std::max(1.0f, Size.y - PanelHeaderHeight));
	const ImVec2 CanvasMin = ImGui::GetCursorScreenPos();

	ImGui::Dummy(CanvasSize);
	const bool bViewportHovered = ImGui::IsItemHovered();
	const bool bViewportClicked =
		bViewportHovered &&
		(ImGui::IsMouseClicked(ImGuiMouseButton_Left) ||
		 ImGui::IsMouseClicked(ImGuiMouseButton_Right) ||
		 ImGui::IsMouseClicked(ImGuiMouseButton_Middle));

	const ImVec2 CanvasMax = ImGui::GetItemRectMax();
	const POINT ClientMin = ImGuiScreenToClientPoint(EditorEngine ? EditorEngine->GetWindow() : nullptr, CanvasMin);
	const FViewportRect NewRect(
		static_cast<int32>(ClientMin.x),
		static_cast<int32>(ClientMin.y),
		static_cast<int32>(CanvasMax.x - CanvasMin.x),
		static_cast<int32>(CanvasMax.y - CanvasMin.y));

	bPreviewViewportVisible = true;
	bPreviewViewportRectValid = NewRect.Width > 0 && NewRect.Height > 0;

	if (bPreviewViewportInitialized)
	{
		PreviewViewport.SetRect(NewRect);
		PreviewClient.SetViewportSize(static_cast<float>(NewRect.Width), static_cast<float>(NewRect.Height));
	}

	if (bViewportClicked && EditorEngine && bPreviewViewportInitialized)
	{
		EditorEngine->FocusViewportInput(&PreviewViewport);
	}

	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	DrawList->AddRectFilled(CanvasMin, CanvasMax, ImGui::GetColorU32(ImVec4(0.34f, 0.35f, 0.34f, 1.0f)));

	ID3D11ShaderResourceView* SRV = bPreviewViewportInitialized ? PreviewViewport.GetOutSRV() : nullptr;
	if (SRV && EditorEngine)
	{
		ID3D11DeviceContext* DeviceContext = EditorEngine->GetRenderer().GetFD3DDevice().GetDeviceContext();
		DrawList->AddCallback(SetOpaqueBlendStateCallback, DeviceContext);
		DrawList->AddImage(reinterpret_cast<ImTextureID>(SRV), CanvasMin, CanvasMax);
		DrawList->AddCallback(ImDrawCallback_ResetRenderState, nullptr);
	}
	else
	{
		DrawList->AddText(
			ImVec2(CanvasMin.x + 12.0f, CanvasMin.y + 44.0f),
			ImGui::GetColorU32(ImVec4(0.78f, 0.80f, 0.84f, 1.0f)),
			"Preview render target is not ready.");
	}

	DrawViewportMenuBar(CanvasMin);

	/*DrawList->AddText(
		ImVec2(CanvasMin.x + 8.0f, CanvasMax.y - 72.0f),
		ImGui::GetColorU32(ImVec4(0.90f, 0.90f, 0.90f, 1.0f)),
		"WARNING: This particle system has no fixed bounding box and contains a GPU emitter.");*/

	const bool bDrawOrientationAxis = bPreviewViewportInitialized
		? PreviewClient.GetParticleShowFlags().bAxis
		: bShowOriginAxis;
	if (bDrawOrientationAxis)
	{
		DrawViewportOrientationAxis(DrawList, CanvasMin, CanvasMax, PreviewClient.GetRenderCamera());
	}
}

void FEditorParticleSystemWidget::DrawViewportMenuBar(const ImVec2& CanvasMin)
{
	ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 2.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 8.0f);
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.10f, 0.10f, 0.10f, 0.88f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.18f, 0.18f, 0.95f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.16f, 0.30f, 0.53f, 1.0f));

	ImGui::SetCursorScreenPos(ImVec2(CanvasMin.x + 8.0f, CanvasMin.y + 6.0f));
	if (ImGui::Button("View##ParticlePreviewViewButton"))
	{
		ImGui::OpenPopup("##ParticlePreviewViewPopup");
	}

	if (ImGui::BeginPopup("##ParticlePreviewViewPopup"))
	{
		ImGui::BeginDisabled();
		char SearchBuffer[1] = {};
		ImGui::SetNextItemWidth(170.0f);
		ImGui::InputText("##ParticlePreviewViewSearch", SearchBuffer, sizeof(SearchBuffer), ImGuiInputTextFlags_ReadOnly);
		ImGui::EndDisabled();
		ImGui::Separator();

		if (bPreviewViewportInitialized && ImGui::BeginMenu("View Modes"))
		{
			DrawViewportModeItem("Wireframe", EViewMode::Wireframe, PreviewViewport);
			DrawViewportModeItem("Unlit", EViewMode::Unlit, PreviewViewport);
			DrawViewportModeItem("Lit", EViewMode::Lit_BlinnPhong, PreviewViewport);
			DrawViewportModeItem("Shader Complexity", EViewMode::Heatmap, PreviewViewport);
			ImGui::EndMenu();
		}

		if (bPreviewViewportInitialized)
		{
			ImGui::Separator();
			FParticleSystemViewportShowFlags& ShowFlags = PreviewClient.GetParticleShowFlags();
			ImGui::MenuItem("Grid", nullptr, &ShowFlags.bGrid);
			if (ImGui::MenuItem("World Axis", nullptr, &ShowFlags.bAxis))
			{
				bShowOriginAxis = ShowFlags.bAxis;
			}
		}

		ImGui::EndPopup();
	}

	ImGui::SameLine(0.0f, 4.0f);
	ImGui::BeginDisabled();
	ImGui::Button("Time##ParticlePreviewTimeButton");
	ImGui::EndDisabled();

	ImGui::PopStyleColor(3);
	ImGui::PopStyleVar(2);
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
