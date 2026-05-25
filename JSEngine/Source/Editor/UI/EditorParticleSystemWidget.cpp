#include "Editor/UI/EditorParticleSystemWidget.h"

#include "Editor/EditorEngine.h"
#include "Editor/UI/EditorDetachedWindowChrome.h"
#include "Editor/UI/EditorMainPanelViewportToolbarHelpers.h"
#include "Core/ResourceManager.h"
#include "Engine/Runtime/WindowsWindow.h"
#include "GameFramework/PrimitiveActors.h"
#include "Object/Class.h"
#include "Particle/ParticleSystem.h"
#include "Component/PostProcess/Light/AmbientLightComponent.h"

#include "ImGui/imgui.h"
#include "ImGui/imgui_internal.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>

namespace
{
	constexpr float SplitterThickness = 5.0f;
	constexpr float PanelHeaderHeight = 24.0f;
	constexpr const char* ParticleEmitterDragPayloadType = "PS_EMITTER";
	constexpr const char* ParticleModuleDragPayloadType = "PS_MODULE";

	struct FEmitterDragPayload
	{
		int32 SourceEmitterIndex = -1;
	};

	struct FModuleDragPayload
	{
		int32 SourceEmitterIndex = -1;
		int32 SourceModuleIndex = -1;
	};

	template <typename ItemType>
	bool MoveArrayItemToInsertIndex(TArray<ItemType>& Items, int32 SourceIndex, int32 InsertIndex, int32& OutNewIndex)
	{
		const int32 Count = static_cast<int32>(Items.size());
		if (SourceIndex < 0 || SourceIndex >= Count)
		{
			return false;
		}

		InsertIndex = std::clamp(InsertIndex, 0, Count);
		if (InsertIndex == SourceIndex || InsertIndex == SourceIndex + 1)
		{
			return false;
		}

		ItemType Item = Items[SourceIndex];
		Items.erase(Items.begin() + SourceIndex);
		if (SourceIndex < InsertIndex)
		{
			--InsertIndex;
		}
		InsertIndex = std::clamp(InsertIndex, 0, static_cast<int32>(Items.size()));
		Items.insert(Items.begin() + InsertIndex, Item);
		OutNewIndex = InsertIndex;
		return true;
	}

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

	FString MakeSeparatedClassName(const char* ClassName, const char* Prefix)
	{
		FString Name = ClassName ? ClassName : "";
		if (Prefix)
		{
			const FString PrefixText = Prefix;
			if (Name.rfind(PrefixText, 0) == 0)
			{
				Name = Name.substr(PrefixText.size());
			}
		}

		FString Result;
		Result.reserve(Name.size() + 8);
		for (size_t Index = 0; Index < Name.size(); ++Index)
		{
			const unsigned char Current = static_cast<unsigned char>(Name[Index]);
			const unsigned char Previous = Index > 0 ? static_cast<unsigned char>(Name[Index - 1]) : 0;
			if (Index > 0 && std::isupper(Current) && (std::islower(Previous) || std::isdigit(Previous)))
			{
				Result.push_back(' ');
			}
			Result.push_back(Name[Index]);
		}
		return Result.empty() ? "Module" : Result;
	}

	FString GetEmitterDisplayName(const UParticleEmitter* Emitter, int32 EmitterIndex)
	{
		if (Emitter)
		{
			const FString Name = Emitter->GetName();
			if (!Name.empty())
			{
				return Name;
			}
		}
		return "Emitter " + std::to_string(EmitterIndex + 1);
	}

	FString GetModuleDisplayName(const UParticleModule* Module, bool bRequired)
	{
		if (bRequired)
		{
			return "Required";
		}
		if (Cast<UParticleModuleSpawn>(Module))
		{
			return "Spawn";
		}
		if (Cast<UParticleModuleLifetime>(Module))
		{
			return "Lifetime";
		}
		if (Cast<UParticleModuleLocation>(Module))
		{
			return "Initial Location";
		}
		if (Cast<UParticleModuleVelocity>(Module))
		{
			return "Initial Velocity";
		}
		if (Cast<UParticleModuleColor>(Module))
		{
			return "Color Over Life";
		}
		if (Cast<UParticleModuleSize>(Module))
		{
			return "Size By Life";
		}
		if (Cast<UParticleModuleCollision>(Module))
		{
			return "Collision";
		}
		if (Cast<UParticleModuleEventGenerator>(Module))
		{
			return "Event Generator";
		}
		if (!Module || !Module->GetClass())
		{
			return "Module";
		}
		return MakeSeparatedClassName(Module->GetClass()->GetName(), "UParticleModule");
	}

	const char* GetRenderModeLabel(const UParticleLODLevel* LODLevel)
	{
		const UParticleModuleRequired* Required = LODLevel ? LODLevel->GetRequiredModule() : nullptr;
		if (!Required)
		{
			return "Emitter";
		}

		switch (Required->GetRenderMode())
		{
		case EParticleEmitterRenderMode::Sprite:
			return "CPU Sprites";
		case EParticleEmitterRenderMode::Mesh:
			return "Mesh Particles";
		case EParticleEmitterRenderMode::Beam:
			return "Beam";
		case EParticleEmitterRenderMode::Ribbon:
			return "Ribbon";
		default:
			return "Emitter";
		}
	}

	bool IsCurveDrivenModule(const UParticleModule* Module)
	{
		return Module && (Module->IsSpawnModule() || Module->IsUpdateModule());
	}

	ImVec4 GetModuleRowColor(const UParticleModule* Module, bool bRequired)
	{
		if (bRequired)
		{
			return ImVec4(0.76f, 0.75f, 0.30f, 1.0f);
		}
		if (Cast<UParticleModuleSpawn>(Module))
		{
			return ImVec4(0.72f, 0.32f, 0.32f, 1.0f);
		}
		if (Module && Module->IsUpdateModule() && !Module->IsSpawnModule())
		{
			return ImVec4(0.24f, 0.43f, 0.27f, 1.0f);
		}
		return ImVec4(0.15f, 0.15f, 0.19f, 1.0f);
	}

	void DrawMiniCheck(ImDrawList* DrawList, const ImVec2& Min, bool bChecked)
	{
		const ImVec2 Max(Min.x + 13.0f, Min.y + 13.0f);
		DrawList->AddRectFilled(Min, Max, ImGui::GetColorU32(ImVec4(0.52f, 0.57f, 0.60f, 1.0f)));
		DrawList->AddRect(Min, Max, ImGui::GetColorU32(ImVec4(0.02f, 0.02f, 0.02f, 1.0f)));
		if (bChecked)
		{
			DrawList->AddLine(ImVec2(Min.x + 3.0f, Min.y + 7.0f), ImVec2(Min.x + 6.0f, Min.y + 10.0f), ImGui::GetColorU32(ImVec4(0.01f, 0.01f, 0.01f, 1.0f)), 1.4f);
			DrawList->AddLine(ImVec2(Min.x + 6.0f, Min.y + 10.0f), ImVec2(Min.x + 11.0f, Min.y + 3.0f), ImGui::GetColorU32(ImVec4(0.01f, 0.01f, 0.01f, 1.0f)), 1.4f);
		}
	}

	void DrawMiniCurveIcon(ImDrawList* DrawList, const ImVec2& Min, bool bEnabled)
	{
		const ImVec2 Max(Min.x + 13.0f, Min.y + 13.0f);
		const ImU32 BorderColor = ImGui::GetColorU32(bEnabled ? ImVec4(0.58f, 0.86f, 0.40f, 1.0f) : ImVec4(0.25f, 0.28f, 0.25f, 1.0f));
		DrawList->AddRectFilled(Min, Max, ImGui::GetColorU32(ImVec4(0.07f, 0.09f, 0.07f, 1.0f)));
		DrawList->AddRect(Min, Max, BorderColor);
		if (bEnabled)
		{
			DrawList->AddLine(ImVec2(Min.x + 2.0f, Min.y + 10.0f), ImVec2(Min.x + 5.0f, Min.y + 5.0f), BorderColor, 1.2f);
			DrawList->AddLine(ImVec2(Min.x + 5.0f, Min.y + 5.0f), ImVec2(Min.x + 8.0f, Min.y + 8.0f), BorderColor, 1.2f);
			DrawList->AddLine(ImVec2(Min.x + 8.0f, Min.y + 8.0f), ImVec2(Min.x + 11.0f, Min.y + 3.0f), BorderColor, 1.2f);
		}
	}

	void DrawEmitterThumbnail(ImDrawList* DrawList, const ImVec2& Min, const ImVec2& Max, int32 EmitterIndex)
	{
		DrawList->AddRectFilled(Min, Max, ImGui::GetColorU32(ImVec4(0.02f, 0.02f, 0.025f, 1.0f)));
		DrawList->AddRect(Min, Max, ImGui::GetColorU32(ImVec4(0.50f, 0.55f, 0.60f, 1.0f)));

		const ImVec2 Center((Min.x + Max.x) * 0.5f, (Min.y + Max.y) * 0.5f);
		const ImU32 CoreColor = ImGui::GetColorU32(ImVec4(0.86f, 0.88f, 0.88f, 0.92f));
		const ImU32 GlowColor = ImGui::GetColorU32(ImVec4(0.42f, 0.55f, 0.68f, 0.28f));
		const float Radius = 8.0f + static_cast<float>((EmitterIndex * 3) % 8);
		DrawList->AddCircleFilled(Center, Radius + 7.0f, GlowColor, 24);
		DrawList->AddCircleFilled(Center, Radius, CoreColor, 24);
		for (int32 Dot = 0; Dot < 9; ++Dot)
		{
			const float Angle = static_cast<float>(Dot) * 0.72f + static_cast<float>(EmitterIndex) * 0.37f;
			const float Distance = 8.0f + static_cast<float>((Dot * 5 + EmitterIndex * 3) % 17);
			const ImVec2 DotCenter(Center.x + std::cos(Angle) * Distance, Center.y + std::sin(Angle) * Distance);
			DrawList->AddCircleFilled(DotCenter, 1.2f + static_cast<float>(Dot % 3) * 0.4f, CoreColor, 8);
		}
	}

	void AddModule(UParticleLODLevel* LODLevel, UParticleModule* Module)
	{
		if (LODLevel && Module)
		{
			LODLevel->Modules.push_back(Module);
		}
	}

	template <typename ModuleType>
	ModuleType* CreateParticleModule(const char* Name)
	{
		ModuleType* Module = UObjectManager::Get().CreateObject<ModuleType>();
		if (Module && Name)
		{
			Module->SetFName(FName(Name));
		}
		return Module;
	}

	UParticleEmitter* CreateLayoutPreviewEmitter(const char* Name, int32 Variant)
	{
		UParticleEmitter* Emitter = UObjectManager::Get().CreateObject<UParticleEmitter>();
        if (!Emitter)
            return nullptr;
        UParticleLODLevel* LODLevel = Emitter->AddLODLevel(0, 100000.0f);
        if (!LODLevel)
            return Emitter;


		const FString LODName = FString(Name) + "_LOD0";
		Emitter->SetFName(FName(Name));
		LODLevel->SetFName(FName(LODName));
		LODLevel->Level = 0;
		LODLevel->RequiredModule = CreateParticleModule<UParticleModuleRequired>("Required");
		AddModule(LODLevel, CreateParticleModule<UParticleModuleSpawn>("Spawn"));
		AddModule(LODLevel, CreateParticleModule<UParticleModuleLifetime>("Lifetime"));

		if (Variant != 1)
		{
			AddModule(LODLevel, CreateParticleModule<UParticleModuleLocation>("Initial Location"));
		}
		if (Variant != 2)
		{
			AddModule(LODLevel, CreateParticleModule<UParticleModuleVelocity>("Initial Velocity"));
		}
		AddModule(LODLevel, CreateParticleModule<UParticleModuleColor>("Color"));
		AddModule(LODLevel, CreateParticleModule<UParticleModuleSize>("Size"));
		if (Variant == 0 || Variant == 3)
		{
			AddModule(LODLevel, CreateParticleModule<UParticleModuleCollision>("Collision"));
		}
		if (Variant == 0)
		{
			AddModule(LODLevel, CreateParticleModule<UParticleModuleEventGenerator>("Event Generator"));
		}

		Emitter->CacheEmitterModuleInfo();
		return Emitter;
	}

	UParticleEmitter* CreateDefaultParticleEmitter(const FString& Name)
	{
        UParticleEmitter* Emitter = UObjectManager::Get().CreateObject<UParticleEmitter>();
        if (!Emitter)
            return nullptr;

        UParticleLODLevel* LODLevel = Emitter->AddLODLevel(0, 100000.0f);
        if (!LODLevel)
            return Emitter;

		const FString LODName = Name + "_LOD0";
		Emitter->SetFName(FName(Name));
		LODLevel->SetFName(FName(LODName));
		LODLevel->Level = 0;
		LODLevel->RequiredModule = CreateParticleModule<UParticleModuleRequired>("Required");
		AddModule(LODLevel, CreateParticleModule<UParticleModuleSpawn>("Spawn"));
		AddModule(LODLevel, CreateParticleModule<UParticleModuleLifetime>("Lifetime"));
		AddModule(LODLevel, CreateParticleModule<UParticleModuleLocation>("Initial Location"));
		AddModule(LODLevel, CreateParticleModule<UParticleModuleVelocity>("Initial Velocity"));
		AddModule(LODLevel, CreateParticleModule<UParticleModuleColor>("Color"));
		AddModule(LODLevel, CreateParticleModule<UParticleModuleSize>("Size"));

		Emitter->CacheEmitterModuleInfo();
		return Emitter;
	}

	FString MakeUniqueEmitterName(const UParticleSystem* ParticleSystem)
	{
		int32 CandidateIndex = ParticleSystem
			? static_cast<int32>(ParticleSystem->GetEmitters().size()) + 1
			: 1;

		while (true)
		{
			const FString CandidateName = "Emitter " + std::to_string(CandidateIndex);
			bool bExists = false;
			if (ParticleSystem)
			{
				for (const UParticleEmitter* Emitter : ParticleSystem->GetEmitters())
				{
					if (Emitter && Emitter->GetName() == CandidateName)
					{
						bExists = true;
						break;
					}
				}
			}
			if (!bExists)
			{
				return CandidateName;
			}
			++CandidateIndex;
		}
	}

	UParticleSystem* CreateLayoutPreviewParticleSystem()
	{
		UParticleSystem* System = UObjectManager::Get().CreateObject<UParticleSystem>();
		if (!System)
		{
			return nullptr;
		}

		System->SetFName(FName("P_Emitter_Column_Preview"));
		System->Emitters.push_back(CreateLayoutPreviewEmitter("smoke", 0));
		System->Emitters.push_back(CreateLayoutPreviewEmitter("blood_b", 1));
		System->Emitters.push_back(CreateLayoutPreviewEmitter("dirt", 2));
		System->Emitters.push_back(CreateLayoutPreviewEmitter("drops", 3));
		return System;
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
	ParticleSystemAsset = nullptr;
	SelectedEmitterIndex = 0;
	SelectedModuleIndex = -1;

	if (!InDocumentPath.empty())
	{
		ParticleSystemAsset = FResourceManager::Get().LoadParticleSystem(InDocumentPath);
	}
	else
	{
		ParticleSystemAsset = CreateLayoutPreviewParticleSystem();
	}

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

	PendingEmitterMoveSource = -1;
	PendingEmitterMoveInsertIndex = -1;
	PendingModuleMoveEmitterIndex = -1;
	PendingModuleMoveSource = -1;
	PendingModuleMoveInsertIndex = -1;

	const ImVec2 BodySize(Size.x, std::max(1.0f, Size.y - PanelHeaderHeight));
	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.045f, 0.046f, 0.055f, 1.0f));
	ImGui::BeginChild("##ParticleEmitterColumnScroller", BodySize, false, ImGuiWindowFlags_HorizontalScrollbar);

	if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) &&
		!ImGui::GetIO().WantTextInput &&
		ImGui::IsKeyPressed(ImGuiKey_Delete, false))
	{
		DeleteSelectedEmitter();
	}

	const TArray<UParticleEmitter*>* Emitters = ParticleSystemAsset ? &ParticleSystemAsset->GetEmitters() : nullptr;
	if (!Emitters || Emitters->empty())
	{
		const ImVec2 Cursor = ImGui::GetCursorScreenPos();
		ImDrawList* DrawList = ImGui::GetWindowDrawList();
		const ImVec2 Max(Cursor.x + BodySize.x, Cursor.y + BodySize.y);
		DrawList->AddRectFilled(Cursor, Max, ImGui::GetColorU32(ImVec4(0.055f, 0.056f, 0.066f, 1.0f)));
		DrawList->AddText(
			ImVec2(Cursor.x + 14.0f, Cursor.y + 14.0f),
			ImGui::GetColorU32(ImVec4(0.58f, 0.61f, 0.66f, 1.0f)),
			"No emitters");
		ImGui::Dummy(BodySize);
	}
	else
	{
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
		const float ColumnHeight = std::max(1.0f, BodySize.y - ImGui::GetStyle().ScrollbarSize);
		for (int32 EmitterIndex = 0; EmitterIndex < static_cast<int32>(Emitters->size()); ++EmitterIndex)
		{
			if (EmitterIndex > 0)
			{
				ImGui::SameLine(0.0f, 0.0f);
			}
			ImGui::BeginGroup();
			DrawEmitterColumn((*Emitters)[EmitterIndex], EmitterIndex, ColumnHeight);
			ImGui::EndGroup();
		}
		ImGui::PopStyleVar();
	}

	ApplyPendingReorders();

	if (ImGui::IsWindowHovered() &&
		ImGui::IsMouseReleased(ImGuiMouseButton_Right) &&
		!ImGui::IsAnyItemHovered())
	{
		ContextEmitterIndex = -1;
		ImGui::OpenPopup("##ParticleEmitterContextMenu");
	}

	DrawEmitterContextMenu();
	ImGui::EndChild();
	ImGui::PopStyleColor();
}

void FEditorParticleSystemWidget::DrawEmitterContextMenu()
{
	if (!ImGui::BeginPopup("##ParticleEmitterContextMenu"))
	{
		return;
	}

	if (ImGui::MenuItem("Add Emitter"))
	{
		AddDefaultEmitter();
	}

	if (ContextEmitterIndex >= 0)
	{
		ImGui::Separator();
		if (ImGui::MenuItem("Delete Emitter"))
		{
			DeleteEmitter(ContextEmitterIndex);
		}
	}

	ImGui::EndPopup();
}

void FEditorParticleSystemWidget::AddDefaultEmitter()
{
	if (!ParticleSystemAsset)
	{
		ParticleSystemAsset = UObjectManager::Get().CreateObject<UParticleSystem>();
		if (!ParticleSystemAsset)
		{
			return;
		}

		const FString SystemName = DocumentPath.empty() ? "Particle System" : DocumentPath;
		ParticleSystemAsset->SetFName(FName(SystemName));
	}

	UParticleEmitter* NewEmitter = CreateDefaultParticleEmitter(MakeUniqueEmitterName(ParticleSystemAsset));
	if (!NewEmitter)
	{
		return;
	}

	ParticleSystemAsset->Emitters.push_back(NewEmitter);
	SelectedEmitterIndex = static_cast<int32>(ParticleSystemAsset->Emitters.size()) - 1;
	SelectedModuleIndex = -1;
	bDirty = true;
}

void FEditorParticleSystemWidget::DeleteSelectedEmitter()
{
	if (SelectedModuleIndex != -1)
	{
		return;
	}
	DeleteEmitter(SelectedEmitterIndex);
}

void FEditorParticleSystemWidget::DeleteEmitter(int32 EmitterIndex)
{
	if (!ParticleSystemAsset || EmitterIndex < 0 || EmitterIndex >= static_cast<int32>(ParticleSystemAsset->Emitters.size()))
	{
		return;
	}

	ParticleSystemAsset->Emitters.erase(ParticleSystemAsset->Emitters.begin() + EmitterIndex);
	ContextEmitterIndex = -1;
	SelectedModuleIndex = -1;
	if (ParticleSystemAsset->Emitters.empty())
	{
		SelectedEmitterIndex = 0;
	}
	else
	{
		SelectedEmitterIndex = std::clamp(EmitterIndex, 0, static_cast<int32>(ParticleSystemAsset->Emitters.size()) - 1);
	}
	bDirty = true;
}

void FEditorParticleSystemWidget::ApplyPendingReorders()
{
	if (PendingModuleMoveEmitterIndex >= 0)
	{
		ReorderModule(PendingModuleMoveEmitterIndex, PendingModuleMoveSource, PendingModuleMoveInsertIndex);
	}

	if (PendingEmitterMoveSource >= 0)
	{
		ReorderEmitter(PendingEmitterMoveSource, PendingEmitterMoveInsertIndex);
	}

	PendingEmitterMoveSource = -1;
	PendingEmitterMoveInsertIndex = -1;
	PendingModuleMoveEmitterIndex = -1;
	PendingModuleMoveSource = -1;
	PendingModuleMoveInsertIndex = -1;
}

void FEditorParticleSystemWidget::ReorderEmitter(int32 SourceIndex, int32 InsertIndex)
{
	if (!ParticleSystemAsset)
	{
		return;
	}

	int32 NewEmitterIndex = SourceIndex;
	if (!MoveArrayItemToInsertIndex(ParticleSystemAsset->Emitters, SourceIndex, InsertIndex, NewEmitterIndex))
	{
		return;
	}

	SelectedEmitterIndex = NewEmitterIndex;
	SelectedModuleIndex = -1;
	ContextEmitterIndex = -1;
	bDirty = true;
}

void FEditorParticleSystemWidget::ReorderModule(int32 EmitterIndex, int32 SourceModuleIndex, int32 InsertIndex)
{
	if (!ParticleSystemAsset ||
		EmitterIndex < 0 ||
		EmitterIndex >= static_cast<int32>(ParticleSystemAsset->Emitters.size()))
	{
		return;
	}

	UParticleEmitter* Emitter = ParticleSystemAsset->Emitters[EmitterIndex];
	if (!Emitter || SourceModuleIndex < 0)
	{
		return;
	}

	UParticleLODLevel* LODLevel = Emitter->GetLODLevel(CurrentLOD);
	if (!LODLevel)
	{
		LODLevel = Emitter->GetLODLevel(0);
	}
	if (!LODLevel)
	{
		return;
	}

	int32 NewModuleIndex = SourceModuleIndex;
	if (!MoveArrayItemToInsertIndex(LODLevel->Modules, SourceModuleIndex, InsertIndex, NewModuleIndex))
	{
		return;
	}

	Emitter->CacheEmitterModuleInfo();
	SelectedEmitterIndex = EmitterIndex;
	SelectedModuleIndex = NewModuleIndex;
	bDirty = true;
}

void FEditorParticleSystemWidget::DrawEmitterColumn(UParticleEmitter* Emitter, int32 EmitterIndex, float ColumnHeight)
{
	constexpr float ColumnWidth = 180.0f;
	constexpr float HeaderHeight = 62.0f;
	constexpr float TypeRowHeight = 22.0f;
	constexpr float ModuleRowHeight = 24.0f;

	UParticleLODLevel* LODLevel = Emitter ? Emitter->GetLODLevel(CurrentLOD) : nullptr;
	if (!LODLevel && Emitter)
	{
		LODLevel = Emitter->GetLODLevel(0);
	}

	const ImVec2 ColumnMin = ImGui::GetCursorScreenPos();
	const ImVec2 ColumnMax(ColumnMin.x + ColumnWidth, ColumnMin.y + ColumnHeight);
	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	DrawList->AddRectFilled(ColumnMin, ColumnMax, ImGui::GetColorU32(ImVec4(0.075f, 0.076f, 0.088f, 1.0f)));
	DrawList->AddRect(ColumnMin, ColumnMax, ImGui::GetColorU32(ImVec4(0.02f, 0.02f, 0.025f, 1.0f)));

	const ImVec2 MousePos = ImGui::GetIO().MousePos;
	const bool bColumnHovered =
		ImGui::IsWindowHovered() &&
		MousePos.x >= ColumnMin.x && MousePos.x <= ColumnMax.x &&
		MousePos.y >= ColumnMin.y && MousePos.y <= ColumnMax.y;
	if (bColumnHovered && ImGui::IsMouseReleased(ImGuiMouseButton_Right))
	{
		SelectedEmitterIndex = EmitterIndex;
		SelectedModuleIndex = -1;
		ContextEmitterIndex = EmitterIndex;
		ImGui::OpenPopup("##ParticleEmitterContextMenu");
	}

	ImGui::PushID(EmitterIndex);

	ImGui::InvisibleButton("##EmitterHeader", ImVec2(ColumnWidth, HeaderHeight));
	const bool bHeaderHovered = ImGui::IsItemHovered();
	if (ImGui::IsItemClicked())
	{
		SelectedEmitterIndex = EmitterIndex;
		SelectedModuleIndex = -1;
	}

	const ImVec2 HeaderMin = ImGui::GetItemRectMin();
	const ImVec2 HeaderMax = ImGui::GetItemRectMax();
	const bool bHeaderSelected = SelectedEmitterIndex == EmitterIndex && SelectedModuleIndex == -1;
	const FString EmitterName = GetEmitterDisplayName(Emitter, EmitterIndex);
	bool bShowEmitterInsertMarker = false;
	float EmitterInsertMarkerX = HeaderMin.x;
	if (bHeaderSelected && ImGui::BeginDragDropSource())
	{
		const FEmitterDragPayload Payload{ EmitterIndex };
		ImGui::SetDragDropPayload(ParticleEmitterDragPayloadType, &Payload, sizeof(Payload));
		ImGui::TextUnformatted(EmitterName.c_str());
		ImGui::EndDragDropSource();
	}
	if (ImGui::BeginDragDropTarget())
	{
		const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload(ParticleEmitterDragPayloadType, ImGuiDragDropFlags_AcceptBeforeDelivery);
		if (Payload && Payload->DataSize == sizeof(FEmitterDragPayload))
		{
			const FEmitterDragPayload* DragPayload = static_cast<const FEmitterDragPayload*>(Payload->Data);
			const bool bDropAfter = ImGui::GetIO().MousePos.x > (HeaderMin.x + HeaderMax.x) * 0.5f;
			const int32 InsertIndex = EmitterIndex + (bDropAfter ? 1 : 0);
			const bool bNoMove = DragPayload->SourceEmitterIndex == InsertIndex || DragPayload->SourceEmitterIndex + 1 == InsertIndex;
			if (!bNoMove)
			{
				bShowEmitterInsertMarker = true;
				EmitterInsertMarkerX = bDropAfter ? HeaderMax.x - 1.0f : HeaderMin.x + 1.0f;
			}
			if (Payload->Delivery)
			{
				PendingEmitterMoveSource = DragPayload->SourceEmitterIndex;
				PendingEmitterMoveInsertIndex = InsertIndex;
			}
		}
		ImGui::EndDragDropTarget();
	}

	const ImVec4 AccentColors[] =
	{
		ImVec4(0.86f, 0.09f, 0.48f, 1.0f),
		ImVec4(0.14f, 0.67f, 0.92f, 1.0f),
		ImVec4(0.95f, 0.18f, 0.12f, 1.0f),
		ImVec4(0.64f, 0.30f, 0.91f, 1.0f)
	};
	constexpr int32 AccentColorCount = static_cast<int32>(sizeof(AccentColors) / sizeof(AccentColors[0]));
	const ImVec4 Accent = AccentColors[EmitterIndex % AccentColorCount];
	DrawList->AddRectFilled(HeaderMin, HeaderMax, ImGui::GetColorU32(bHeaderHovered ? ImVec4(0.18f, 0.20f, 0.24f, 1.0f) : ImVec4(0.13f, 0.14f, 0.16f, 1.0f)));
	DrawList->AddRectFilled(HeaderMin, ImVec2(HeaderMin.x + 4.0f, HeaderMax.y), ImGui::GetColorU32(Accent));
	if (bHeaderSelected)
	{
		DrawList->AddRect(HeaderMin, HeaderMax, ImGui::GetColorU32(ImVec4(0.38f, 0.58f, 0.92f, 1.0f)), 0.0f, 0, 1.5f);
	}
	if (bShowEmitterInsertMarker)
	{
		DrawList->AddLine(
			ImVec2(EmitterInsertMarkerX, HeaderMin.y),
			ImVec2(EmitterInsertMarkerX, ColumnMax.y),
			ImGui::GetColorU32(ImVec4(0.35f, 0.70f, 1.0f, 1.0f)),
			2.0f);
	}

	DrawList->AddText(ImVec2(HeaderMin.x + 10.0f, HeaderMin.y + 8.0f), ImGui::GetColorU32(ImVec4(0.88f, 0.90f, 0.94f, 1.0f)), EmitterName.c_str());

	const float ControlsY = HeaderMin.y + 31.0f;
	DrawMiniCheck(DrawList, ImVec2(HeaderMin.x + 10.0f, ControlsY), LODLevel ? LODLevel->IsEnabled() : true);
	DrawList->AddRectFilled(ImVec2(HeaderMin.x + 30.0f, ControlsY), ImVec2(HeaderMin.x + 43.0f, ControlsY + 13.0f), ImGui::GetColorU32(ImVec4(0.72f, 0.78f, 0.24f, 1.0f)));
	DrawList->AddText(ImVec2(HeaderMin.x + 33.0f, ControlsY - 1.0f), ImGui::GetColorU32(ImVec4(0.03f, 0.04f, 0.03f, 1.0f)), "S");
	DrawList->AddRectFilled(ImVec2(HeaderMin.x + 50.0f, ControlsY), ImVec2(HeaderMin.x + 63.0f, ControlsY + 13.0f), ImGui::GetColorU32(ImVec4(0.47f, 0.59f, 0.66f, 1.0f)));
	DrawList->AddText(ImVec2(HeaderMin.x + 53.0f, ControlsY - 1.0f), ImGui::GetColorU32(ImVec4(0.03f, 0.04f, 0.05f, 1.0f)), "U");

	const int32 MaxParticles = Emitter ? Emitter->GetMaxActiveParticleCount() : 0;
	char CountBuffer[32] = {};
	std::snprintf(CountBuffer, sizeof(CountBuffer), "%d", MaxParticles);
	const ImVec2 CountSize = ImGui::CalcTextSize(CountBuffer);
	DrawList->AddText(ImVec2(HeaderMin.x + 102.0f - CountSize.x, ControlsY - 1.0f), ImGui::GetColorU32(ImVec4(0.88f, 0.90f, 0.94f, 1.0f)), CountBuffer);

	DrawEmitterThumbnail(DrawList, ImVec2(HeaderMax.x - 58.0f, HeaderMin.y + 7.0f), ImVec2(HeaderMax.x - 7.0f, HeaderMax.y - 7.0f), EmitterIndex);

	ImGui::InvisibleButton("##EmitterType", ImVec2(ColumnWidth, TypeRowHeight));
	const ImVec2 TypeMin = ImGui::GetItemRectMin();
	const ImVec2 TypeMax = ImGui::GetItemRectMax();
	DrawList->AddRectFilled(TypeMin, TypeMax, ImGui::GetColorU32(ImVec4(0.055f, 0.056f, 0.066f, 1.0f)));
	DrawList->AddText(ImVec2(TypeMin.x + 10.0f, TypeMin.y + 3.0f), ImGui::GetColorU32(ImVec4(0.76f, 0.79f, 0.84f, 1.0f)), GetRenderModeLabel(LODLevel));

	if (LODLevel && LODLevel->GetRequiredModule())
	{
		DrawEmitterModuleRow(LODLevel->GetRequiredModule(), EmitterIndex, -1, true, ModuleRowHeight);
	}

	if (LODLevel)
	{
		const TArray<UParticleModule*>& Modules = LODLevel->GetModules();
		for (int32 ModuleIndex = 0; ModuleIndex < static_cast<int32>(Modules.size()); ++ModuleIndex)
		{
			DrawEmitterModuleRow(Modules[ModuleIndex], EmitterIndex, ModuleIndex, false, ModuleRowHeight);
		}
	}

	const float DrawnHeight = ImGui::GetCursorScreenPos().y - ColumnMin.y;
	if (DrawnHeight < ColumnHeight)
	{
		ImGui::Dummy(ImVec2(ColumnWidth, ColumnHeight - DrawnHeight));
	}

	ImGui::PopID();
}

void FEditorParticleSystemWidget::DrawEmitterModuleRow(UParticleModule* Module, int32 EmitterIndex, int32 ModuleIndex, bool bRequired, float RowHeight)
{
	constexpr float ColumnWidth = 180.0f;

	ImGui::PushID(bRequired ? -1000 : ModuleIndex);
	ImGui::InvisibleButton("##ModuleRow", ImVec2(ColumnWidth, RowHeight));
	const bool bHovered = ImGui::IsItemHovered();
	if (ImGui::IsItemClicked())
	{
		SelectedEmitterIndex = EmitterIndex;
		SelectedModuleIndex = ModuleIndex;
	}

	const bool bSelected = SelectedEmitterIndex == EmitterIndex && SelectedModuleIndex == ModuleIndex;
	const ImVec2 Min = ImGui::GetItemRectMin();
	const ImVec2 Max = ImGui::GetItemRectMax();
	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	const FString ModuleName = GetModuleDisplayName(Module, bRequired);
	bool bShowModuleInsertMarker = false;
	float ModuleInsertMarkerY = Min.y;
	if (!bRequired && bSelected && ImGui::BeginDragDropSource())
	{
		const FModuleDragPayload Payload{ EmitterIndex, ModuleIndex };
		ImGui::SetDragDropPayload(ParticleModuleDragPayloadType, &Payload, sizeof(Payload));
		ImGui::TextUnformatted(ModuleName.c_str());
		ImGui::EndDragDropSource();
	}
	if (!bRequired && ImGui::BeginDragDropTarget())
	{
		const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload(ParticleModuleDragPayloadType, ImGuiDragDropFlags_AcceptBeforeDelivery);
		if (Payload && Payload->DataSize == sizeof(FModuleDragPayload))
		{
			const FModuleDragPayload* DragPayload = static_cast<const FModuleDragPayload*>(Payload->Data);
			if (DragPayload->SourceEmitterIndex == EmitterIndex)
			{
				const bool bDropAfter = ImGui::GetIO().MousePos.y > (Min.y + Max.y) * 0.5f;
				const int32 InsertIndex = ModuleIndex + (bDropAfter ? 1 : 0);
				const bool bNoMove = DragPayload->SourceModuleIndex == InsertIndex || DragPayload->SourceModuleIndex + 1 == InsertIndex;
				if (!bNoMove)
				{
					bShowModuleInsertMarker = true;
					ModuleInsertMarkerY = bDropAfter ? Max.y - 1.0f : Min.y + 1.0f;
				}
				if (Payload->Delivery)
				{
					PendingModuleMoveEmitterIndex = EmitterIndex;
					PendingModuleMoveSource = DragPayload->SourceModuleIndex;
					PendingModuleMoveInsertIndex = InsertIndex;
				}
			}
		}
		ImGui::EndDragDropTarget();
	}

	ImVec4 RowColor = GetModuleRowColor(Module, bRequired);
	if (Module && !Module->IsEnabled())
	{
		RowColor = ImVec4(0.12f, 0.12f, 0.14f, 1.0f);
	}
	if (bHovered)
	{
		RowColor.x = std::min(RowColor.x + 0.07f, 1.0f);
		RowColor.y = std::min(RowColor.y + 0.07f, 1.0f);
		RowColor.z = std::min(RowColor.z + 0.07f, 1.0f);
	}

	DrawList->AddRectFilled(Min, Max, ImGui::GetColorU32(RowColor));
	DrawList->AddLine(ImVec2(Min.x, Max.y - 1.0f), ImVec2(Max.x, Max.y - 1.0f), ImGui::GetColorU32(ImVec4(0.03f, 0.03f, 0.035f, 1.0f)));
	if (bSelected)
	{
		DrawList->AddRect(Min, Max, ImGui::GetColorU32(ImVec4(0.38f, 0.58f, 0.92f, 1.0f)), 0.0f, 0, 1.5f);
	}
	if (bShowModuleInsertMarker)
	{
		DrawList->AddLine(
			ImVec2(Min.x, ModuleInsertMarkerY),
			ImVec2(Max.x, ModuleInsertMarkerY),
			ImGui::GetColorU32(ImVec4(0.35f, 0.70f, 1.0f, 1.0f)),
			2.0f);
	}

	DrawList->AddText(ImVec2(Min.x + 10.0f, Min.y + 4.0f), ImGui::GetColorU32(ImVec4(0.95f, 0.96f, 0.98f, 1.0f)), ModuleName.c_str());
	DrawMiniCheck(DrawList, ImVec2(Max.x - 38.0f, Min.y + 5.0f), Module ? Module->IsEnabled() : true);
	DrawMiniCurveIcon(DrawList, ImVec2(Max.x - 19.0f, Min.y + 5.0f), IsCurveDrivenModule(Module));

	ImGui::PopID();
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
