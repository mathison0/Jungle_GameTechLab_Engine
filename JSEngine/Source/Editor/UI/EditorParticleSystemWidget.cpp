#include "Editor/UI/EditorParticleSystemWidget.h"

#include "Editor/EditorEngine.h"
#include "Editor/UI/EditorDetachedWindowChrome.h"
#include "Editor/UI/EditorMainPanelViewportToolbarHelpers.h"
#include "Engine/Core/EditorResourcePaths.h"
#include "Core/ResourceManager.h"
#include "Core/Paths.h"
#include "Engine/Runtime/WindowsWindow.h"
#include "GameFramework/AActor.h"
#include "GameFramework/PrimitiveActors.h"
#include "GameFramework/World.h"
#include "Object/Class.h"
#include "Object/Property.h"
#include "Particle/ParticleSystemComponent.h"
#include "Particle/ParticleSystem.h"
#include "Render/Resource/Material.h"
#include "Component/PostProcess/Light/AmbientLightComponent.h"

#include "ImGui/imgui.h"
#include "ImGui/imgui_internal.h"
#include "WICTextureLoader.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace
{
	constexpr float SplitterThickness = 5.0f;
	constexpr float PanelHeaderHeight = 24.0f;
	constexpr const char* ParticleEmitterDragPayloadType = "PS_EMITTER";
	constexpr const char* ParticleModuleDragPayloadType = "PS_MODULE";
	constexpr int32 NoParticleModuleSelection = -1;
	constexpr int32 RequiredParticleModuleSelection = -2;
	constexpr int32 TypeDataParticleModuleSelection = -3;
	constexpr ImGuiDragDropFlags ParticleDragDropTargetFlags =
		ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect;

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

	bool ToolbarButton(
		const char* Id,
		const char* Label,
		ID3D11ShaderResourceView* Icon,
		const char* Tooltip = nullptr,
		bool bSelected = false)
	{
		constexpr float ButtonHeight = 28.0f;
		constexpr float IconSize = 18.0f;
		constexpr float PaddingX = 8.0f;
		constexpr float IconTextGap = 6.0f;

		const char* DisplayLabel = Label ? Label : "";
		const ImVec2 TextSize = ImGui::CalcTextSize(DisplayLabel);
		const bool bHasText = DisplayLabel[0] != '\0';
		const float ButtonWidth =
			PaddingX * 2.0f +
			(Icon ? IconSize : 0.0f) +
			(Icon && bHasText ? IconTextGap : 0.0f) +
			(bHasText ? TextSize.x : 0.0f);
		const ImVec2 ButtonSize(std::max(ButtonHeight, ButtonWidth), ButtonHeight);

		ImGui::PushID(Id);
		const bool bClicked = ImGui::InvisibleButton("##CascadeToolbarButton", ButtonSize);
		const ImVec2 Min = ImGui::GetItemRectMin();
		const ImVec2 Max = ImGui::GetItemRectMax();
		const bool bHovered = ImGui::IsItemHovered();
		const bool bActive = ImGui::IsItemActive();
		const float Alpha = ImGui::GetStyle().Alpha;
		ImDrawList* DrawList = ImGui::GetWindowDrawList();

		const ImVec4 BgColor =
			bSelected
				? (bHovered ? ImVec4(0.22f, 0.29f, 0.38f, 1.0f) : ImVec4(0.18f, 0.24f, 0.32f, 1.0f))
				: (bActive ? ImVec4(0.28f, 0.31f, 0.38f, 1.0f)
						   : bHovered ? ImVec4(0.23f, 0.25f, 0.30f, 1.0f)
									  : ImVec4(0.14f, 0.15f, 0.17f, 1.0f));
		const ImU32 Bg = ImGui::GetColorU32(ImVec4(BgColor.x, BgColor.y, BgColor.z, BgColor.w * Alpha));
		const ImU32 Border = ImGui::GetColorU32(
			bSelected
				? ImVec4(0.42f, 0.55f, 0.75f, Alpha)
				: bHovered ? ImVec4(0.33f, 0.36f, 0.42f, Alpha)
						   : ImVec4(0.18f, 0.19f, 0.22f, Alpha));
		DrawList->AddRectFilled(Min, Max, Bg, 3.0f);
		DrawList->AddRect(Min, Max, Border, 3.0f);

		float CursorX = Min.x + (ButtonSize.x - ButtonWidth) * 0.5f + PaddingX;
		if (Icon)
		{
			const float IconY = Min.y + (ButtonHeight - IconSize) * 0.5f;
			DrawList->AddImage(
				reinterpret_cast<ImTextureID>(Icon),
				ImVec2(CursorX, IconY),
				ImVec2(CursorX + IconSize, IconY + IconSize),
				ImVec2(0.0f, 0.0f),
				ImVec2(1.0f, 1.0f),
				ImGui::GetColorU32(ImVec4(1.0f, 1.0f, 1.0f, Alpha)));
			CursorX += IconSize + (bHasText ? IconTextGap : 0.0f);
		}
		if (bHasText)
		{
			DrawList->AddText(
				ImVec2(CursorX, Min.y + (ButtonHeight - TextSize.y) * 0.5f),
				ImGui::GetColorU32(ImVec4(0.86f, 0.88f, 0.92f, Alpha)),
				DisplayLabel);
		}

		if (Tooltip && ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("%s", Tooltip);
		}
		ImGui::PopID();
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

	bool IsPointInsideRect(const ImVec2& Point, const ImVec2& Min, const ImVec2& Max)
	{
		return Point.x >= Min.x && Point.x <= Max.x && Point.y >= Min.y && Point.y <= Max.y;
	}

	bool BeginParticleDetailsTable(const char* TableId, float LabelWidth)
	{
		ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(8.0f, 4.0f));
		if (ImGui::BeginTable(
			TableId,
			2,
			ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_BordersInnerH | ImGuiTableFlags_RowBg))
		{
			ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, LabelWidth);
			ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
			return true;
		}

		ImGui::PopStyleVar();
		return false;
	}

	void EndParticleDetailsTable()
	{
		ImGui::EndTable();
		ImGui::PopStyleVar();
	}

	void BeginParticleDetailsRow(const char* Label, float RowHeight = 28.0f)
	{
		ImGui::TableNextRow(ImGuiTableRowFlags_None, RowHeight);
		ImGui::TableSetColumnIndex(0);
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted(Label);
		ImGui::TableSetColumnIndex(1);
	}

	void PushParticlePopupStyle()
	{
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 8.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 5.0f));
	}

	void PopParticlePopupStyle()
	{
		ImGui::PopStyleVar(2);
	}

	bool BeginParticlePopup(const char* Id)
	{
		PushParticlePopupStyle();
		if (ImGui::BeginPopup(Id))
		{
			return true;
		}
		PopParticlePopupStyle();
		return false;
	}

	void EndParticlePopup()
	{
		ImGui::EndPopup();
		PopParticlePopupStyle();
	}

	bool BeginParticlePopupModal(const char* Id, bool* Open, ImGuiWindowFlags Flags)
	{
		PushParticlePopupStyle();
		if (ImGui::BeginPopupModal(Id, Open, Flags))
		{
			return true;
		}
		PopParticlePopupStyle();
		return false;
	}

	void EndParticlePopupModal()
	{
		ImGui::EndPopup();
		PopParticlePopupStyle();
	}

	bool BeginParticleMenu(const char* Label, bool bEnabled = true)
	{
		PushParticlePopupStyle();
		if (ImGui::BeginMenu(Label, bEnabled))
		{
			return true;
		}
		PopParticlePopupStyle();
		return false;
	}

	void EndParticleMenu()
	{
		ImGui::EndMenu();
		PopParticlePopupStyle();
	}

	bool BeginParticleCombo(const char* Label, const char* PreviewValue, ImGuiComboFlags Flags = ImGuiComboFlags_None)
	{
		PushParticlePopupStyle();
		if (ImGui::BeginCombo(Label, PreviewValue, Flags))
		{
			return true;
		}
		PopParticlePopupStyle();
		return false;
	}

	void EndParticleCombo()
	{
		ImGui::EndCombo();
		PopParticlePopupStyle();
	}

	bool ParticleCombo(const char* Label, int* CurrentItem, const char* const Items[], int ItemsCount)
	{
		PushParticlePopupStyle();
		const bool bChanged = ImGui::Combo(Label, CurrentItem, Items, ItemsCount);
		PopParticlePopupStyle();
		return bChanged;
	}

	bool ParticleCombo(
		const char* Label,
		int* CurrentItem,
		const char* (*Getter)(void*, int),
		void* UserData,
		int ItemsCount)
	{
		PushParticlePopupStyle();
		const bool bChanged = ImGui::Combo(Label, CurrentItem, Getter, UserData, ItemsCount);
		PopParticlePopupStyle();
		return bChanged;
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

	FString TrimCopy(const FString& Value)
	{
		size_t First = 0;
		while (First < Value.size() && std::isspace(static_cast<unsigned char>(Value[First])))
		{
			++First;
		}

		size_t Last = Value.size();
		while (Last > First && std::isspace(static_cast<unsigned char>(Value[Last - 1])))
		{
			--Last;
		}

		return Value.substr(First, Last - First);
	}

	bool IsParticleSystemAssetDocumentPath(const FString& Path)
	{
		FString NormalizedPath = FPaths::Normalize(Path);
		std::transform(
			NormalizedPath.begin(),
			NormalizedPath.end(),
			NormalizedPath.begin(),
			[](unsigned char Ch)
			{
				return static_cast<char>(std::tolower(Ch));
			});

		const FString Extension = ".particlesystem";
		return NormalizedPath.size() >= Extension.size() &&
			NormalizedPath.compare(NormalizedPath.size() - Extension.size(), Extension.size(), Extension) == 0;
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

	const char* GetPropertyDisplayName(const FProperty& Property)
	{
		return (Property.DisplayName && Property.DisplayName[0] != '\0') ? Property.DisplayName : Property.Name;
	}

	FString MakeParticlePropertyLabel(const FProperty& Property)
	{
		const char* DisplayName = GetPropertyDisplayName(Property);
		if (!DisplayName)
		{
			return "";
		}
		if (!Property.Name || std::strcmp(DisplayName, Property.Name) == 0)
		{
			return DisplayName;
		}
		return FString(DisplayName) + "##" + Property.Name;
	}

	bool IsInternalParticleModuleProperty(const FProperty& Property)
	{
		return Property.Name &&
			(std::strcmp(Property.Name, "bSpawnModule") == 0 ||
			 std::strcmp(Property.Name, "bUpdateModule") == 0);
	}

	const char* GetRenderModeLabel(EParticleEmitterRenderMode RenderMode)
	{
		switch (RenderMode)
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

	const char* GetRenderModeLabel(const UParticleLODLevel* LODLevel)
	{
		const UParticleModuleRequired* Required = LODLevel ? LODLevel->GetRequiredModule() : nullptr;
		return Required ? GetRenderModeLabel(Required->GetRenderMode()) : "Emitter";
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

	void DrawDisabledParticleModuleMenu(const char* MenuLabel)
	{
		if (BeginParticleMenu(MenuLabel, false))
		{
			EndParticleMenu();
		}
	}

	template <typename ModuleType, typename AddModuleFunc>
	void DrawParticleModuleAddMenu(
		const char* MenuLabel,
		const char* ItemLabel,
		bool bEnabled,
		AddModuleFunc AddModule)
	{
		if (BeginParticleMenu(MenuLabel, bEnabled))
		{
			if (ImGui::MenuItem(ItemLabel))
			{
				AddModule(CreateParticleModule<ModuleType>(ItemLabel));
			}
			EndParticleMenu();
		}
	}

	const char* GetTypeDataMenuItemLabel(EParticleEmitterRenderMode RenderMode)
	{
		switch (RenderMode)
		{
		case EParticleEmitterRenderMode::Sprite:
			return "New Sprite Data";
		case EParticleEmitterRenderMode::Mesh:
			return "New Mesh Data";
		case EParticleEmitterRenderMode::Beam:
			return "New Beam Data";
		case EParticleEmitterRenderMode::Ribbon:
			return "New Ribbon Data";
		default:
			return "New Type Data";
		}
	}

	UParticleEmitter* CreateDefaultParticleEmitter(const FString& Name)
	{
		UParticleEmitter* Emitter = UObjectManager::Get().CreateObject<UParticleEmitter>();
		UParticleLODLevel* LODLevel = UObjectManager::Get().CreateObject<UParticleLODLevel>();
		if (!Emitter || !LODLevel)
		{
			return Emitter;
		}

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

		Emitter->LODLevels.push_back(LODLevel);
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
	CurveEditorWidget.Initialize(InEditorEngine);
}

void FEditorParticleSystemWidget::Shutdown()
{
	ShutdownPreviewViewport();
	for (TComPtr<ID3D11ShaderResourceView>& Icon : CascadeToolbarIcons)
	{
		Icon.Reset();
	}
	bCascadeToolbarIconsLoadAttempted = false;
}

bool FEditorParticleSystemWidget::Save()
{
	if (!ParticleSystemAsset)
	{
		if (EditorEngine)
		{
			EditorEngine->GetNotificationService().Warning("No particle system to save.");
		}
		return false;
	}

	if (DocumentPath.empty() || !IsParticleSystemAssetDocumentPath(DocumentPath))
	{
		if (EditorEngine)
		{
			EditorEngine->GetNotificationService().Warning("Particle system has no asset path to save.");
		}
		return false;
	}

	ParticleSystemAsset->CacheEmitterModuleInfo();
	if (!FResourceManager::Get().SaveParticleSystem(ParticleSystemAsset, DocumentPath))
	{
		if (EditorEngine)
		{
			EditorEngine->GetNotificationService().Error("Failed to save particle system.");
		}
		return false;
	}

	bDirty = false;
	if (EditorEngine)
	{
		EditorEngine->GetNotificationService().Info("Particle system saved.");
	}
	return true;
}

bool FEditorParticleSystemWidget::CanUndo() const
{
	return !UndoHistory.empty();
}

bool FEditorParticleSystemWidget::CanRedo() const
{
	return !RedoHistory.empty();
}

bool FEditorParticleSystemWidget::Undo()
{
	if (UndoHistory.empty())
	{
		return false;
	}

	FParticleEditorUndoEntry CurrentEntry;
	CurrentEntry.Label = "Current";
	CurrentEntry.Snapshot = CaptureParticleSnapshot();
	CurrentEntry.CurrentLOD = CurrentLOD;
	CurrentEntry.SelectedEmitterIndex = SelectedEmitterIndex;
	CurrentEntry.SelectedModuleIndex = SelectedModuleIndex;

	const FParticleEditorUndoEntry PreviousEntry = UndoHistory.back();
	UndoHistory.pop_back();
	PushUndoEntry(RedoHistory, CurrentEntry, false);

	bRestoringParticleSnapshot = true;
	const bool bRestored = RestoreParticleSnapshot(
		PreviousEntry.Snapshot,
		PreviousEntry.CurrentLOD,
		PreviousEntry.SelectedEmitterIndex,
		PreviousEntry.SelectedModuleIndex);
	bRestoringParticleSnapshot = false;

	if (bRestored && EditorEngine)
	{
		EditorEngine->GetNotificationService().Info("Undo: " + PreviousEntry.Label);
	}
	return bRestored;
}

bool FEditorParticleSystemWidget::Redo()
{
	if (RedoHistory.empty())
	{
		return false;
	}

	FParticleEditorUndoEntry CurrentEntry;
	CurrentEntry.Label = "Current";
	CurrentEntry.Snapshot = CaptureParticleSnapshot();
	CurrentEntry.CurrentLOD = CurrentLOD;
	CurrentEntry.SelectedEmitterIndex = SelectedEmitterIndex;
	CurrentEntry.SelectedModuleIndex = SelectedModuleIndex;

	const FParticleEditorUndoEntry NextEntry = RedoHistory.back();
	RedoHistory.pop_back();
	PushUndoEntry(UndoHistory, CurrentEntry, false);

	bRestoringParticleSnapshot = true;
	const bool bRestored = RestoreParticleSnapshot(
		NextEntry.Snapshot,
		NextEntry.CurrentLOD,
		NextEntry.SelectedEmitterIndex,
		NextEntry.SelectedModuleIndex);
	bRestoringParticleSnapshot = false;

	if (bRestored && EditorEngine)
	{
		EditorEngine->GetNotificationService().Info("Redo: " + NextEntry.Label);
	}
	return bRestored;
}

void FEditorParticleSystemWidget::CloseDocument(const FString& InDocumentPath)
{
	PreviewBackgroundColorByDocument.erase(InDocumentPath);
	if (DocumentPath == InDocumentPath)
	{
		PreviewClient.ResetBackgroundColor();
	}
}

void FEditorParticleSystemWidget::Render(float DeltaTime)
{
	RenderEmbedded(DeltaTime);
}

void FEditorParticleSystemWidget::RenderEmbedded(float DeltaTime)
{
	LastDeltaTime = DeltaTime;
	if (CenterToastRemainingTime > 0.0f)
	{
		CenterToastRemainingTime = std::max(0.0f, CenterToastRemainingTime - DeltaTime);
		if (CenterToastRemainingTime <= 0.0f)
		{
			CenterToastMessage.clear();
		}
	}

	EnsurePreviewViewport();
	bPreviewViewportVisible = false;
	bPreviewViewportRectValid = false;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
	ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.055f, 0.058f, 0.070f, 1.0f));
	const ImVec2 ToastAreaMin = ImGui::GetCursorScreenPos();
	const ImVec2 ToastAreaSize = ImGui::GetContentRegionAvail();
	DrawMainLayout();
	DrawCenterToast(ToastAreaMin, ToastAreaSize);
	ImGui::PopStyleColor();
	ImGui::PopStyleVar(2);
}

void FEditorParticleSystemWidget::OpenParticleSystem(const FString& InDocumentPath)
{
	if (InDocumentPath.empty())
	{
		return;
	}

	DocumentPath = InDocumentPath;
	bDirty = false;
	ParticleSystemAsset = nullptr;
	SelectEmitter(0);
	ClearEmitterContext();
	ClearUndoHistory();
	bPropertyEditUndoCaptured = false;
	bEmitterNameEditUndoCaptured = false;

	if (auto BackgroundIt = PreviewBackgroundColorByDocument.find(DocumentPath);
		BackgroundIt != PreviewBackgroundColorByDocument.end())
	{
		PreviewClient.SetBackgroundColor(BackgroundIt->second);
	}
	else
	{
		PreviewClient.ResetBackgroundColor();
	}

	ParticleSystemAsset = FResourceManager::Get().LoadParticleSystem(InDocumentPath);

	EnsurePreviewViewport();
	RefreshPreviewComponent(true);
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
	EnsurePreviewActor();
}

void FEditorParticleSystemWidget::EnsurePreviewActor()
{
	if (PreviewComponent || !EditorEngine || PreviewWorldHandle == FName::None)
	{
		return;
	}

	FWorldContext* PreviewContext = EditorEngine->GetWorldContextFromHandle(PreviewWorldHandle);
	UWorld* PreviewWorld = PreviewContext ? PreviewContext->World : nullptr;
	if (!PreviewWorld)
	{
		return;
	}

	PreviewActor = PreviewWorld->SpawnActor<AActor>();
	if (!PreviewActor)
	{
		return;
	}

	PreviewActor->SetFName(FName("Particle Preview Actor"));
	PreviewActor->SetTickInEditor(true);
	PreviewActor->SetActorLocation(FVector::ZeroVector);

	PreviewComponent = PreviewActor->AddComponent<UParticleSystemComponent>();
	if (!PreviewComponent)
	{
		PreviewWorld->DestroyActor(PreviewActor);
		PreviewActor = nullptr;
		return;
	}

	PreviewComponent->SetTransient(true);
	PreviewComponent->SetEditorOnly(true);
	PreviewActor->SetRootComponent(PreviewComponent);
	PreviewComponent->SetTemplate(ParticleSystemAsset);
	PreviewClient.SetFocusTargetActor(PreviewActor);
	PreviewWorld->SyncSpatialIndex();
}

void FEditorParticleSystemWidget::RefreshPreviewComponent(bool bRestartSimulation)
{
	EnsurePreviewViewport();
	EnsurePreviewActor();
	if (!PreviewComponent)
	{
		return;
	}

	if (ParticleSystemAsset)
	{
		ParticleSystemAsset->CacheEmitterModuleInfo();
	}

	if (PreviewComponent->GetTemplate() != ParticleSystemAsset)
	{
		PreviewComponent->SetTemplate(ParticleSystemAsset);
	}
	else if (bRestartSimulation)
	{
		PreviewComponent->RecreateEmitterInstances();
	}

	if (PreviewActor && PreviewComponent->GetTotalActiveParticleCount() == 0)
	{
		PreviewActor->Tick(0.1f);
	}

	if (EditorEngine && PreviewWorldHandle != FName::None)
	{
		if (FWorldContext* PreviewContext = EditorEngine->GetWorldContextFromHandle(PreviewWorldHandle))
		{
			if (PreviewContext->World)
			{
				PreviewContext->World->SyncSpatialIndex();
			}
		}
	}
}

void FEditorParticleSystemWidget::ShutdownPreviewViewport()
{
	bPreviewViewportVisible = false;
	bPreviewViewportRectValid = false;
	PreviewComponent = nullptr;
	PreviewClient.SetFocusTargetActor(nullptr);
	PreviewActor = nullptr;

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

void FEditorParticleSystemWidget::LoadCascadeToolbarIcons()
{
	if (bCascadeToolbarIconsLoadAttempted)
	{
		return;
	}
	bCascadeToolbarIconsLoadAttempted = true;

	if (!EditorEngine)
	{
		return;
	}

	ID3D11Device* Device = EditorEngine->GetRenderer().GetFD3DDevice().GetDevice();
	if (!Device)
	{
		return;
	}

	static constexpr const wchar_t* IconFiles[CascadeToolbarIconCount] =
	{
		L"icon_file_save_40x.png",
		L"icon_toolbar_genericfinder_40px.png",
		L"icon_Cascade_RestartSim_40x.png",
		L"icon_Cascade_RestartInLevel_40x.png",
		L"icon_Generic_Undo_40x.png",
		L"icon_Generic_Redo_40x.png",
		L"icon_Cascade_Thumbnail_40x.png",
		L"icon_Cascade_Bounds_40x.png",
		L"icon_Cascade_Axis_40x.png",
		L"icon_Cascade_Color_40x.png",
		L"icon_Cascade_RegenLOD1_40x.png",
		L"icon_Cascade_LowestLOD_40x.png",
		L"icon_Cascade_LowerLOD_40x.png",
		L"icon_Cascade_AddLOD1_40x.png",
		L"icon_Cascade_HigherLOD_40x.png",
		L"icon_tab_Modules_40x.png"
	};

	const std::wstring IconDir = FEditorResourcePaths::CascadeAbsoluteDir();
	for (int32 IconIndex = 0; IconIndex < CascadeToolbarIconCount; ++IconIndex)
	{
		if (CascadeToolbarIcons[IconIndex])
		{
			continue;
		}

		const std::wstring IconPath = IconDir + IconFiles[IconIndex];
		DirectX::CreateWICTextureFromFile(Device, IconPath.c_str(), nullptr, CascadeToolbarIcons[IconIndex].GetAddressOf());
	}
}

ID3D11ShaderResourceView* FEditorParticleSystemWidget::GetCascadeToolbarIcon(ECascadeToolbarIcon Icon) const
{
	const int32 IconIndex = static_cast<int32>(Icon);
	if (IconIndex < 0 || IconIndex >= CascadeToolbarIconCount)
	{
		return nullptr;
	}
	return CascadeToolbarIcons[IconIndex].Get();
}

void FEditorParticleSystemWidget::RenderDocumentToolbarControls()
{
	LoadCascadeToolbarIcons();

	if (ToolbarButton("Save", "", GetCascadeToolbarIcon(ECascadeToolbarIcon::Save), "Save particle system"))
	{
		Save();
	}
	SameLineGap();
	ToolbarButton("FindInContentBrowser", "", GetCascadeToolbarIcon(ECascadeToolbarIcon::Find), "Find in Content Browser");

	SameLineGap(14.0f);
	ToolbarButton("RestartSim", "Restart Sim", GetCascadeToolbarIcon(ECascadeToolbarIcon::RestartSim), "Restart simulation");
	SameLineGap();
	ToolbarButton("RestartLevel", "Restart Level", GetCascadeToolbarIcon(ECascadeToolbarIcon::RestartLevel), "Restart preview level");

	SameLineGap(14.0f);
	ImGui::BeginDisabled(!CanUndo());
	if (ToolbarButton("Undo", "Undo", GetCascadeToolbarIcon(ECascadeToolbarIcon::Undo), "Undo"))
	{
		Undo();
	}
	ImGui::EndDisabled();
	SameLineGap();
	ImGui::BeginDisabled(!CanRedo());
	if (ToolbarButton("Redo", "Redo", GetCascadeToolbarIcon(ECascadeToolbarIcon::Redo), "Redo"))
	{
		Redo();
	}
	ImGui::EndDisabled();

	SameLineGap(14.0f);
	if (ToolbarButton("Thumbnail", "Thumbnail", GetCascadeToolbarIcon(ECascadeToolbarIcon::Thumbnail), "Toggle thumbnail preview", bShowThumbnail))
	{
		bShowThumbnail = !bShowThumbnail;
	}
	SameLineGap();
	if (ToolbarButton("Bounds", "Bounds", GetCascadeToolbarIcon(ECascadeToolbarIcon::Bounds), "Toggle bounds", bShowBounds))
	{
		bShowBounds = !bShowBounds;
		if (bPreviewViewportInitialized)
		{
			PreviewClient.GetParticleShowFlags().bBounds = bShowBounds;
		}
	}
	SameLineGap();
	if (ToolbarButton("OriginAxis", "Origin Axis", GetCascadeToolbarIcon(ECascadeToolbarIcon::OriginAxis), "Toggle origin axis", bShowOriginAxis))
	{
		bShowOriginAxis = !bShowOriginAxis;
		if (bPreviewViewportInitialized)
		{
			PreviewClient.GetParticleShowFlags().bAxis = bShowOriginAxis;
		}
	}
	SameLineGap();
	if (ToolbarButton("BackgroundColor", "Background Color", GetCascadeToolbarIcon(ECascadeToolbarIcon::BackgroundColor), "Change preview background color"))
	{
		ImGui::OpenPopup("##ParticlePreviewBackgroundColorPopup");
	}
	DrawBackgroundColorPopup();

	SameLineGap(14.0f);
	ToolbarButton("RegenLOD", "Regen LOD", GetCascadeToolbarIcon(ECascadeToolbarIcon::RegenLOD), "Regenerate LOD");
	SameLineGap();
	ToolbarButton("LowestLOD", "Lowest LOD", GetCascadeToolbarIcon(ECascadeToolbarIcon::LowestLOD), "Switch to lowest LOD");
	SameLineGap();
	ToolbarButton("LowerLOD", "Lower LOD", GetCascadeToolbarIcon(ECascadeToolbarIcon::LowerLOD), "Switch to lower LOD");
	SameLineGap();
	ToolbarButton("AddLOD", "Add LOD", GetCascadeToolbarIcon(ECascadeToolbarIcon::AddLOD), "Add LOD");

	SameLineGap(8.0f);
	ImGui::SetNextItemWidth(54.0f);
	ImGui::InputInt("LOD", &CurrentLOD, 0, 0);
	CurrentLOD = std::max(0, CurrentLOD);
	SameLineGap();
	ToolbarButton("HigherLOD", "Higher LOD", GetCascadeToolbarIcon(ECascadeToolbarIcon::HigherLOD), "Switch to higher LOD");

	SameLineGap(14.0f);
	ToolbarButton("ParticleEditorMenu", "Menu", GetCascadeToolbarIcon(ECascadeToolbarIcon::Menu), "Particle editor menu");
}

void FEditorParticleSystemWidget::DrawBackgroundColorPopup()
{
	if (!BeginParticlePopup("##ParticlePreviewBackgroundColorPopup"))
	{
		return;
	}

	FColor BackgroundColor = PreviewClient.GetBackgroundColor();
	float ColorValues[3] =
	{
		std::clamp(BackgroundColor.R, 0.0f, 1.0f),
		std::clamp(BackgroundColor.G, 0.0f, 1.0f),
		std::clamp(BackgroundColor.B, 0.0f, 1.0f)
	};

	const ImGuiColorEditFlags ColorFlags =
		ImGuiColorEditFlags_NoAlpha |
		ImGuiColorEditFlags_DisplayRGB |
		ImGuiColorEditFlags_InputRGB |
		ImGuiColorEditFlags_Uint8 |
		ImGuiColorEditFlags_PickerHueBar;

	bool bChanged = false;
	ImGui::SetNextItemWidth(260.0f);
	bChanged |= ImGui::ColorPicker3("##ParticlePreviewBackgroundPicker", ColorValues, ColorFlags);

	ImGui::Separator();
	ImGui::SetNextItemWidth(260.0f);
	bChanged |= ImGui::ColorEdit3(
		"RGB",
		ColorValues,
		ColorFlags | ImGuiColorEditFlags_NoPicker | ImGuiColorEditFlags_NoSmallPreview);

	if (ImGui::Button("Reset"))
	{
		const FColor DefaultColor = FParticleSystemViewportClient::GetDefaultBackgroundColor();
		ColorValues[0] = DefaultColor.R;
		ColorValues[1] = DefaultColor.G;
		ColorValues[2] = DefaultColor.B;
		bChanged = true;
	}

	if (bChanged)
	{
		BackgroundColor = FColor(
			std::clamp(ColorValues[0], 0.0f, 1.0f),
			std::clamp(ColorValues[1], 0.0f, 1.0f),
			std::clamp(ColorValues[2], 0.0f, 1.0f),
			1.0f);
		PreviewClient.SetBackgroundColor(BackgroundColor);
		if (!DocumentPath.empty())
		{
			PreviewBackgroundColorByDocument[DocumentPath] = BackgroundColor;
		}
	}

	EndParticlePopup();
}

void FEditorParticleSystemWidget::RenderDetachedDocumentChrome(bool& bCloseRequested)
{
	FEditorDetachedWindowChrome::RenderMenuBar(
		"Particle System Editor",
		"ParticleSystemEditor",
		[this]()
		{
			if (BeginParticleMenu("File"))
			{
				if (ImGui::MenuItem(bDirty ? "Save *" : "Save", "Ctrl+S"))
				{
					Save();
				}
				ImGui::MenuItem("Save As...", nullptr, false, false);
				EndParticleMenu();
			}
			if (BeginParticleMenu("Edit"))
			{
				if (ImGui::MenuItem("Undo", "Ctrl+Z", false, CanUndo()))
				{
					Undo();
				}
				if (ImGui::MenuItem("Redo", "Ctrl+Shift+Z", false, CanRedo()))
				{
					Redo();
				}
				EndParticleMenu();
			}
			if (BeginParticleMenu("View"))
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
				EndParticleMenu();
			}
			if (BeginParticleMenu("Particle"))
			{
				ImGui::MenuItem("Restart Simulation", nullptr, false, false);
				ImGui::MenuItem("Restart Level", nullptr, false, false);
				ImGui::Separator();
				ImGui::MenuItem("Regenerate LOD", nullptr, false, false);
				ImGui::MenuItem("Add LOD", nullptr, false, false);
				EndParticleMenu();
			}
			if (BeginParticleMenu("Window"))
			{
				if (ImGui::MenuItem("Reset Layout"))
				{
					TopAreaHeight = 0.0f;
					TopLeftWidth = 0.0f;
					BottomLeftWidth = 0.0f;
				}
				EndParticleMenu();
			}
			if (BeginParticleMenu("Help"))
			{
				ImGui::TextDisabled("Particle System Editor");
				if (!DocumentPath.empty())
				{
					ImGui::Separator();
					ImGui::TextDisabled("%s", DocumentPath.c_str());
				}
				EndParticleMenu();
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
	const FColor& BackgroundColor = PreviewClient.GetBackgroundColor();
	DrawList->AddRectFilled(
		CanvasMin,
		CanvasMax,
		ImGui::GetColorU32(ImVec4(BackgroundColor.R, BackgroundColor.G, BackgroundColor.B, 1.0f)));

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

	if (BeginParticlePopup("##ParticlePreviewViewPopup"))
	{
		ImGui::BeginDisabled();
		char SearchBuffer[1] = {};
		ImGui::SetNextItemWidth(170.0f);
		ImGui::InputText("##ParticlePreviewViewSearch", SearchBuffer, sizeof(SearchBuffer), ImGuiInputTextFlags_ReadOnly);
		ImGui::EndDisabled();
		ImGui::Separator();

		if (bPreviewViewportInitialized && BeginParticleMenu("View Modes"))
		{
			DrawViewportModeItem("Wireframe", EViewMode::Wireframe, PreviewViewport);
			DrawViewportModeItem("Unlit", EViewMode::Unlit, PreviewViewport);
			DrawViewportModeItem("Lit", EViewMode::Lit_BlinnPhong, PreviewViewport);
			DrawViewportModeItem("Shader Complexity", EViewMode::Heatmap, PreviewViewport);
			EndParticleMenu();
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

		EndParticlePopup();
	}

	ImGui::SameLine(0.0f, 4.0f);
	if (ImGui::Button("Time##ParticlePreviewTimeButton"))
	{
		ImGui::OpenPopup("##ParticlePreviewTimePopup");
	}

	if (BeginParticlePopup("##ParticlePreviewTimePopup"))
	{
		if (ImGui::MenuItem("Play/Pause", nullptr, bPreviewPaused))
		{
			bPreviewPaused = !bPreviewPaused;
		}
		ImGui::MenuItem("Realtime", nullptr, &bPreviewRealtime);
		ImGui::MenuItem("Loop", nullptr, &bPreviewLoop);

		if (BeginParticleMenu("AnimSpeed"))
		{
			ImGui::TextDisabled("ANIMSPEED");
			ImGui::Separator();

			const char* SpeedLabels[] = { "100%", "50%", "25%", "10%", "1%" };
			for (int32 SpeedIndex = 0; SpeedIndex < static_cast<int32>(IM_ARRAYSIZE(SpeedLabels)); ++SpeedIndex)
			{
				ImGui::PushID(SpeedIndex);
				if (ImGui::RadioButton(SpeedLabels[SpeedIndex], PreviewAnimSpeedIndex == SpeedIndex))
				{
					PreviewAnimSpeedIndex = SpeedIndex;
				}
				ImGui::PopID();
			}
			EndParticleMenu();
		}

		EndParticlePopup();
	}

	ImGui::PopStyleColor(3);
	ImGui::PopStyleVar(2);
}

void FEditorParticleSystemWidget::DrawEmittersPanel(const ImVec2& Size)
{
	DrawPanelHeader("Emitters");

	ResetPendingReorders();

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
	bool bMouseInsideEmitterColumnArea = false;
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
		if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
		{
			SelectParticleSystem();
		}
		if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Right))
		{
			SelectParticleSystem();
			OpenEmitterContextMenu(-1, NoParticleModuleSelection);
		}
	}
	else
	{
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));
		const float ColumnHeight = std::max(1.0f, BodySize.y - ImGui::GetStyle().ScrollbarSize);
		bool bHasEmitterColumnBounds = false;
		ImVec2 EmitterColumnAreaMin(0.0f, 0.0f);
		ImVec2 EmitterColumnAreaMax(0.0f, 0.0f);
		for (int32 EmitterIndex = 0; EmitterIndex < static_cast<int32>(Emitters->size()); ++EmitterIndex)
		{
			if (EmitterIndex > 0)
			{
				ImGui::SameLine(0.0f, 0.0f);
			}
			ImGui::BeginGroup();
			DrawEmitterColumn((*Emitters)[EmitterIndex], EmitterIndex, ColumnHeight);
			ImGui::EndGroup();

			const ImVec2 ColumnItemMin = ImGui::GetItemRectMin();
			const ImVec2 ColumnItemMax = ImGui::GetItemRectMax();
			if (!bHasEmitterColumnBounds)
			{
				EmitterColumnAreaMin = ColumnItemMin;
				EmitterColumnAreaMax = ColumnItemMax;
				bHasEmitterColumnBounds = true;
			}
			else
			{
				EmitterColumnAreaMin.x = std::min(EmitterColumnAreaMin.x, ColumnItemMin.x);
				EmitterColumnAreaMin.y = std::min(EmitterColumnAreaMin.y, ColumnItemMin.y);
				EmitterColumnAreaMax.x = std::max(EmitterColumnAreaMax.x, ColumnItemMax.x);
				EmitterColumnAreaMax.y = std::max(EmitterColumnAreaMax.y, ColumnItemMax.y);
			}
		}
		if (bHasEmitterColumnBounds)
		{
			const ImVec2 MousePos = ImGui::GetIO().MousePos;
			bMouseInsideEmitterColumnArea = IsPointInsideRect(MousePos, EmitterColumnAreaMin, EmitterColumnAreaMax);
		}
		ImGui::PopStyleVar();
	}

	ApplyPendingReorders();

	if (ImGui::IsWindowHovered() &&
		ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
		!bMouseInsideEmitterColumnArea &&
		!ImGui::IsAnyItemHovered())
	{
		SelectParticleSystem();
	}

	if (ImGui::IsWindowHovered() &&
		ImGui::IsMouseReleased(ImGuiMouseButton_Right) &&
		!bOpenEmitterContextMenu &&
		!bMouseInsideEmitterColumnArea &&
		!ImGui::IsAnyItemHovered())
	{
		SelectParticleSystem();
		OpenEmitterContextMenu(-1, NoParticleModuleSelection);
	}

	if (bOpenEmitterContextMenu)
	{
		ImGui::OpenPopup("##ParticleEmitterContextMenu");
		bOpenEmitterContextMenu = false;
	}
	DrawEmitterContextMenu();
	DrawEmitterRenamePopup();
	ImGui::EndChild();
	ImGui::PopStyleColor();
}

void FEditorParticleSystemWidget::DrawEmitterContextMenu()
{
	if (!BeginParticlePopup("##ParticleEmitterContextMenu"))
	{
		return;
	}

	const int32 TargetEmitterIndex = ContextEmitterIndex >= 0 ? ContextEmitterIndex : SelectedEmitterIndex;
	const bool bHasTargetEmitter =
		ParticleSystemAsset &&
		TargetEmitterIndex >= 0 &&
		TargetEmitterIndex < static_cast<int32>(ParticleSystemAsset->Emitters.size());
	UParticleEmitter* TargetEmitter = bHasTargetEmitter ? ParticleSystemAsset->Emitters[TargetEmitterIndex] : nullptr;
	UParticleLODLevel* TargetLODLevel = GetEmitterLODLevel(TargetEmitter);
	const bool bHasSpawnModule = TargetLODLevel && TargetLODLevel->GetSpawnModule();
	const UParticleModuleRequired* TargetRequired = TargetLODLevel ? TargetLODLevel->GetRequiredModule() : nullptr;
	const EParticleEmitterRenderMode CurrentRenderMode = TargetRequired
		? TargetRequired->GetRenderMode()
		: EParticleEmitterRenderMode::Sprite;

	auto DrawTypeDataItems = [&](bool bUseCreateLabels)
	{
		constexpr EParticleEmitterRenderMode RenderModes[] =
		{
			EParticleEmitterRenderMode::Sprite,
			EParticleEmitterRenderMode::Mesh,
			EParticleEmitterRenderMode::Beam,
			EParticleEmitterRenderMode::Ribbon,
		};
		for (EParticleEmitterRenderMode RenderMode : RenderModes)
		{
			const char* Label = bUseCreateLabels ? GetTypeDataMenuItemLabel(RenderMode) : GetRenderModeLabel(RenderMode);
			if (ImGui::MenuItem(Label, nullptr, CurrentRenderMode == RenderMode, bHasTargetEmitter))
			{
				ChangeEmitterRenderMode(TargetEmitterIndex, RenderMode);
			}
		}
	};

	if (ContextModuleIndex == TypeDataParticleModuleSelection)
	{
		ImGui::TextDisabled("EMITTER TYPE");
		ImGui::Separator();
		DrawTypeDataItems(false);
		EndParticlePopup();
		return;
	}

	if (ContextEmitterIndex < 0 && ContextModuleIndex == NoParticleModuleSelection)
	{
		if (ImGui::MenuItem("New Particle Sprite Emitter"))
		{
			AddDefaultEmitter();
		}
		EndParticlePopup();
		return;
	}

	if (ContextModuleIndex != NoParticleModuleSelection)
	{
		if (ImGui::MenuItem("Delete Module"))
		{
			DeleteModule(TargetEmitterIndex, ContextModuleIndex);
		}
		EndParticlePopup();
		return;
	}

	if (BeginParticleMenu("Emitter", bHasTargetEmitter))
	{
		ImGui::TextDisabled("EMITTER");
		ImGui::Separator();
		if (ImGui::MenuItem("Rename Emitter"))
		{
			BeginRenameEmitter(TargetEmitterIndex);
		}
		ImGui::MenuItem("Duplicate Emitter", nullptr, false, false);
		ImGui::MenuItem("Duplicate and Share Emitter", nullptr, false, false);
		if (ImGui::MenuItem("Delete Emitter"))
		{
			DeleteEmitter(TargetEmitterIndex);
		}
		ImGui::MenuItem("Export Emitter", nullptr, false, false);
		ImGui::MenuItem("Export All", nullptr, false, false);
		EndParticleMenu();
	}

	if (BeginParticleMenu("Particle System"))
	{
		ImGui::TextDisabled("PARTICLE SYSTEM");
		ImGui::Separator();
		if (ImGui::MenuItem("Select Particle System"))
		{
			SelectParticleSystem();
		}
		if (ImGui::MenuItem("Add New Emitter Before", nullptr, false, bHasTargetEmitter))
		{
			AddDefaultEmitterAt(TargetEmitterIndex);
		}
		if (ImGui::MenuItem("Add New Emitter After", nullptr, false, bHasTargetEmitter))
		{
			AddDefaultEmitterAt(TargetEmitterIndex + 1);
		}
		if (!bHasTargetEmitter && ImGui::MenuItem("Add Emitter"))
		{
			AddDefaultEmitter();
		}
		ImGui::MenuItem("Remove Duplicate Modules", nullptr, false, false);
		EndParticleMenu();
	}

	auto AddModuleToTargetEmitter = [&](UParticleModule* Module)
	{
		AddModuleToEmitter(TargetEmitterIndex, Module);
	};

	if (BeginParticleMenu("TypeData", bHasTargetEmitter))
	{
		ImGui::TextDisabled("TYPEDATA");
		ImGui::Separator();
		DrawTypeDataItems(true);
		EndParticleMenu();
	}
	DrawDisabledParticleModuleMenu("Acceleration");
	DrawDisabledParticleModuleMenu("Attraction");
	DrawDisabledParticleModuleMenu("Camera");
	DrawParticleModuleAddMenu<UParticleModuleCollision>("Collision", "Collision", bHasTargetEmitter, AddModuleToTargetEmitter);
	DrawParticleModuleAddMenu<UParticleModuleColor>("Color", "Color Over Life", bHasTargetEmitter, AddModuleToTargetEmitter);
	DrawParticleModuleAddMenu<UParticleModuleEventGenerator>("Event", "Event Generator", bHasTargetEmitter, AddModuleToTargetEmitter);
	DrawDisabledParticleModuleMenu("Kill");
	DrawParticleModuleAddMenu<UParticleModuleLifetime>("Lifetime", "Lifetime", bHasTargetEmitter, AddModuleToTargetEmitter);
	DrawDisabledParticleModuleMenu("Light");
	DrawParticleModuleAddMenu<UParticleModuleLocation>("Location", "Initial Location", bHasTargetEmitter, AddModuleToTargetEmitter);
	DrawDisabledParticleModuleMenu("Rotation");
	DrawDisabledParticleModuleMenu("Rotation Rate");
	DrawDisabledParticleModuleMenu("Orbit");
	DrawDisabledParticleModuleMenu("Orientation");
	DrawDisabledParticleModuleMenu("Parameter");
	DrawParticleModuleAddMenu<UParticleModuleSize>("Size", "Size By Life", bHasTargetEmitter, AddModuleToTargetEmitter);
	DrawParticleModuleAddMenu<UParticleModuleSpawn>("Spawn", "Spawn", bHasTargetEmitter && !bHasSpawnModule, AddModuleToTargetEmitter);
	DrawDisabledParticleModuleMenu("SubUV");
	DrawParticleModuleAddMenu<UParticleModuleVelocity>("Velocity", "Initial Velocity", bHasTargetEmitter, AddModuleToTargetEmitter);

	EndParticlePopup();
}

void FEditorParticleSystemWidget::AddDefaultEmitter()
{
	const int32 InsertIndex = ParticleSystemAsset ? static_cast<int32>(ParticleSystemAsset->Emitters.size()) : 0;
	AddDefaultEmitterAt(InsertIndex);
}

void FEditorParticleSystemWidget::AddDefaultEmitterAt(int32 InsertIndex)
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

	CaptureUndoSnapshot("Add Emitter");
	InsertIndex = std::clamp(InsertIndex, 0, static_cast<int32>(ParticleSystemAsset->Emitters.size()));
	ParticleSystemAsset->Emitters.insert(ParticleSystemAsset->Emitters.begin() + InsertIndex, NewEmitter);
	SelectEmitter(InsertIndex);
	ClearEmitterContext();
	bDirty = true;
	RefreshPreviewComponent(true);
}

void FEditorParticleSystemWidget::DeleteSelectedEmitter()
{
	if (SelectedModuleIndex != NoParticleModuleSelection)
	{
		DeleteModule(SelectedEmitterIndex, SelectedModuleIndex);
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

	CaptureUndoSnapshot("Delete Emitter");
	ParticleSystemAsset->Emitters.erase(ParticleSystemAsset->Emitters.begin() + EmitterIndex);
	ClearEmitterContext();
	if (ParticleSystemAsset->Emitters.empty())
	{
		SelectParticleSystem();
	}
	else
	{
		SelectEmitter(std::clamp(EmitterIndex, 0, static_cast<int32>(ParticleSystemAsset->Emitters.size()) - 1));
	}
	bDirty = true;
	RefreshPreviewComponent(true);
}

void FEditorParticleSystemWidget::AddModuleToEmitter(int32 EmitterIndex, UParticleModule* Module)
{
	if (!ParticleSystemAsset ||
		!Module ||
		EmitterIndex < 0 ||
		EmitterIndex >= static_cast<int32>(ParticleSystemAsset->Emitters.size()))
	{
		return;
	}

	UParticleEmitter* Emitter = ParticleSystemAsset->Emitters[EmitterIndex];
	if (!Emitter)
	{
		return;
	}

	UParticleLODLevel* LODLevel = GetEmitterLODLevel(Emitter);
	if (!LODLevel)
	{
		return;
	}

	CaptureUndoSnapshot("Add Particle Module");
	LODLevel->Modules.push_back(Module);
	Emitter->CacheEmitterModuleInfo();
	SelectModule(EmitterIndex, static_cast<int32>(LODLevel->Modules.size()) - 1);
	ClearEmitterContext();
	bDirty = true;
	RefreshPreviewComponent(true);
}

void FEditorParticleSystemWidget::DeleteModule(int32 EmitterIndex, int32 ModuleIndex)
{
	if (!ParticleSystemAsset ||
		EmitterIndex < 0 ||
		EmitterIndex >= static_cast<int32>(ParticleSystemAsset->Emitters.size()))
	{
		return;
	}

	UParticleEmitter* Emitter = ParticleSystemAsset->Emitters[EmitterIndex];
	if (!Emitter)
	{
		return;
	}

	UParticleLODLevel* LODLevel = GetEmitterLODLevel(Emitter);
	if (!LODLevel)
	{
		return;
	}

	if (ModuleIndex == RequiredParticleModuleSelection)
	{
		ShowCenterToast("Required module cannot be deleted.");
		return;
	}
	if (ModuleIndex < 0 || ModuleIndex >= static_cast<int32>(LODLevel->Modules.size()))
	{
		return;
	}

	UParticleModule* Module = LODLevel->Modules[ModuleIndex];
	if (Cast<UParticleModuleSpawn>(Module))
	{
		ShowCenterToast("Spawn module cannot be deleted.");
		return;
	}

	CaptureUndoSnapshot("Delete Particle Module");
	LODLevel->Modules.erase(LODLevel->Modules.begin() + ModuleIndex);
	Emitter->CacheEmitterModuleInfo();
	if (LODLevel->Modules.empty())
	{
		SelectEmitter(EmitterIndex);
	}
	else
	{
		SelectModule(EmitterIndex, std::clamp(ModuleIndex, 0, static_cast<int32>(LODLevel->Modules.size()) - 1));
	}
	ClearEmitterContext();
	bDirty = true;
	RefreshPreviewComponent(true);
}

void FEditorParticleSystemWidget::ChangeEmitterRenderMode(int32 EmitterIndex, EParticleEmitterRenderMode RenderMode)
{
	if (!ParticleSystemAsset ||
		EmitterIndex < 0 ||
		EmitterIndex >= static_cast<int32>(ParticleSystemAsset->Emitters.size()))
	{
		return;
	}

	UParticleEmitter* Emitter = ParticleSystemAsset->Emitters[EmitterIndex];
	UParticleLODLevel* LODLevel = GetEmitterLODLevel(Emitter);
	UParticleModuleRequired* Required = LODLevel ? LODLevel->GetRequiredModule() : nullptr;
	if (!Emitter || !Required || Required->GetRenderMode() == RenderMode)
	{
		return;
	}

	CaptureUndoSnapshot("Change Emitter Type");
	Required->SetRenderMode(RenderMode);
	Emitter->CacheEmitterModuleInfo();
	ParticleSystemAsset->CacheEmitterModuleInfo();
	SelectEmitter(EmitterIndex);
	ClearEmitterContext();
	bDirty = true;
	RefreshPreviewComponent(true);
}

void FEditorParticleSystemWidget::ShowCenterToast(const FString& Message)
{
	if (Message.empty())
	{
		return;
	}

	CenterToastMessage = Message;
	CenterToastRemainingTime = 1.6f;
}

void FEditorParticleSystemWidget::DrawCenterToast(const ImVec2& AreaMin, const ImVec2& AreaSize)
{
	if (CenterToastRemainingTime <= 0.0f || CenterToastMessage.empty() || AreaSize.x <= 1.0f || AreaSize.y <= 1.0f)
	{
		return;
	}

	const float FadeAlpha = std::clamp(CenterToastRemainingTime / 0.25f, 0.0f, 1.0f);
	const ImVec2 ToastCenter(
		AreaMin.x + AreaSize.x * 0.5f,
		AreaMin.y + AreaSize.y * 0.5f);

	if (const ImGuiViewport* Viewport = ImGui::GetWindowViewport())
	{
		ImGui::SetNextWindowViewport(Viewport->ID);
	}
	ImGui::SetNextWindowPos(ToastCenter, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
	ImGui::SetNextWindowBgAlpha(0.92f * FadeAlpha);

	constexpr ImGuiWindowFlags ToastFlags =
		ImGuiWindowFlags_NoDecoration |
		ImGuiWindowFlags_NoDocking |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_AlwaysAutoResize |
		ImGuiWindowFlags_NoFocusOnAppearing |
		ImGuiWindowFlags_NoNav |
		ImGuiWindowFlags_NoMove |
		ImGuiWindowFlags_NoInputs;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(18.0f, 12.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 7.0f);
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
	ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.08f, 0.09f, 0.92f * FadeAlpha));
	ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.72f, 0.49f, 0.18f, 0.85f * FadeAlpha));
	ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.88f, 0.64f, FadeAlpha));
	if (ImGui::Begin("##ParticleEditorCenterToast", nullptr, ToastFlags))
	{
		ImGui::TextUnformatted(CenterToastMessage.c_str());
	}
	ImGui::End();
	ImGui::PopStyleColor(3);
	ImGui::PopStyleVar(3);
}

void FEditorParticleSystemWidget::CaptureUndoSnapshot(const char* Label)
{
	if (bRestoringParticleSnapshot)
	{
		return;
	}

	FParticleEditorUndoEntry Entry;
	Entry.Label = (Label && Label[0] != '\0') ? Label : "Edit Particle System";
	Entry.Snapshot = CaptureParticleSnapshot();
	Entry.CurrentLOD = CurrentLOD;
	Entry.SelectedEmitterIndex = SelectedEmitterIndex;
	Entry.SelectedModuleIndex = SelectedModuleIndex;
	PushUndoEntry(UndoHistory, Entry, true);
	RedoHistory.clear();
}

FString FEditorParticleSystemWidget::CaptureParticleSnapshot() const
{
	return ParticleSystemAsset
		? FResourceManager::Get().SerializeParticleSystemToString(ParticleSystemAsset)
		: FString();
}

bool FEditorParticleSystemWidget::RestoreParticleSnapshot(
	const FString& Snapshot,
	int32 InCurrentLOD,
	int32 InSelectedEmitterIndex,
	int32 InSelectedModuleIndex)
{
	UParticleSystem* RestoredAsset = nullptr;
	if (!Snapshot.empty())
	{
		RestoredAsset = FResourceManager::Get().LoadParticleSystemFromString(Snapshot);
		if (!RestoredAsset)
		{
			return false;
		}
	}

	ParticleSystemAsset = RestoredAsset;
	CurrentLOD = std::max(0, InCurrentLOD);
	SelectedEmitterIndex = InSelectedEmitterIndex;
	SelectedModuleIndex = InSelectedModuleIndex;
	ClearEmitterContext();
	RenameEmitterIndex = -1;
	bOpenEmitterContextMenu = false;
	bOpenRenameEmitterPopup = false;
	bPropertyEditUndoCaptured = false;
	bEmitterNameEditUndoCaptured = false;

	if (ParticleSystemAsset)
	{
		ParticleSystemAsset->CacheEmitterModuleInfo();
	}
	ClampSelectionToParticleSystem();
	bDirty = true;
	RefreshPreviewComponent(true);
	return true;
}

void FEditorParticleSystemWidget::ClearUndoHistory()
{
	UndoHistory.clear();
	RedoHistory.clear();
}

void FEditorParticleSystemWidget::PushUndoEntry(
	TArray<FParticleEditorUndoEntry>& Stack,
	const FParticleEditorUndoEntry& Entry,
	bool bSkipDuplicate)
{
	if (bSkipDuplicate && !Stack.empty() && Stack.back().Snapshot == Entry.Snapshot)
	{
		return;
	}

	constexpr int32 MaxParticleUndoHistory = 50;
	Stack.push_back(Entry);
	if (static_cast<int32>(Stack.size()) > MaxParticleUndoHistory)
	{
		Stack.erase(Stack.begin());
	}
}

void FEditorParticleSystemWidget::ClampSelectionToParticleSystem()
{
	if (!ParticleSystemAsset || ParticleSystemAsset->Emitters.empty())
	{
		SelectParticleSystem();
		CurrentLOD = std::max(0, CurrentLOD);
		return;
	}

	if (SelectedEmitterIndex < 0)
	{
		SelectParticleSystem();
		CurrentLOD = std::max(0, CurrentLOD);
		return;
	}

	SelectedEmitterIndex = std::clamp(
		SelectedEmitterIndex,
		0,
		static_cast<int32>(ParticleSystemAsset->Emitters.size()) - 1);

	UParticleEmitter* Emitter = ParticleSystemAsset->Emitters[SelectedEmitterIndex];
	if (!Emitter || Emitter->GetLODLevels().empty())
	{
		CurrentLOD = 0;
		SelectEmitter(SelectedEmitterIndex);
		return;
	}

	CurrentLOD = std::clamp(CurrentLOD, 0, static_cast<int32>(Emitter->GetLODLevels().size()) - 1);
	UParticleLODLevel* LODLevel = Emitter->GetLODLevel(CurrentLOD);
	if (!LODLevel)
	{
		SelectEmitter(SelectedEmitterIndex);
		return;
	}

	if (SelectedModuleIndex == RequiredParticleModuleSelection)
	{
		if (!LODLevel->GetRequiredModule())
		{
			SelectEmitter(SelectedEmitterIndex);
		}
		return;
	}

	if (SelectedModuleIndex < 0 || SelectedModuleIndex >= static_cast<int32>(LODLevel->Modules.size()))
	{
		SelectEmitter(SelectedEmitterIndex);
	}
}

void FEditorParticleSystemWidget::BeginRenameEmitter(int32 EmitterIndex)
{
	if (!ParticleSystemAsset ||
		EmitterIndex < 0 ||
		EmitterIndex >= static_cast<int32>(ParticleSystemAsset->Emitters.size()))
	{
		return;
	}

	RenameEmitterIndex = EmitterIndex;
	const FString EmitterName = GetEmitterDisplayName(ParticleSystemAsset->Emitters[EmitterIndex], EmitterIndex);
	std::snprintf(RenameEmitterBuffer, sizeof(RenameEmitterBuffer), "%s", EmitterName.c_str());
	bOpenRenameEmitterPopup = true;
}

bool FEditorParticleSystemWidget::ApplyEmitterName(int32 EmitterIndex, const FString& NewName, bool bCaptureUndo, bool bWarnOnEmpty)
{
	if (!ParticleSystemAsset ||
		EmitterIndex < 0 ||
		EmitterIndex >= static_cast<int32>(ParticleSystemAsset->Emitters.size()))
	{
		return false;
	}

	const FString TrimmedName = TrimCopy(NewName);
	if (TrimmedName.empty())
	{
		if (bWarnOnEmpty && EditorEngine)
		{
			EditorEngine->GetNotificationService().Warning("Emitter name cannot be empty.");
		}
		return false;
	}

	UParticleEmitter* Emitter = ParticleSystemAsset->Emitters[EmitterIndex];
	if (!Emitter)
	{
		return false;
	}

	if (Emitter->GetFName().ToString() == TrimmedName)
	{
		return false;
	}

	if (bCaptureUndo)
	{
		CaptureUndoSnapshot("Rename Emitter");
	}
	Emitter->SetFName(FName(TrimmedName));
	SelectedEmitterIndex = EmitterIndex;
	SelectedModuleIndex = NoParticleModuleSelection;
	bDirty = true;
	return true;
}

void FEditorParticleSystemWidget::RenameEmitter(int32 EmitterIndex, const FString& NewName)
{
	ApplyEmitterName(EmitterIndex, NewName, true, true);
}

void FEditorParticleSystemWidget::DrawEmitterRenamePopup()
{
	if (bOpenRenameEmitterPopup)
	{
		ImGui::OpenPopup("Rename Emitter##ParticleEmitterRenamePopup");
		bOpenRenameEmitterPopup = false;
	}

	if (!BeginParticlePopupModal("Rename Emitter##ParticleEmitterRenamePopup", nullptr, ImGuiWindowFlags_AlwaysAutoResize))
	{
		return;
	}

	ImGui::SetNextItemWidth(260.0f);
	const bool bEnterPressed = ImGui::InputText("Name", RenameEmitterBuffer, sizeof(RenameEmitterBuffer), ImGuiInputTextFlags_EnterReturnsTrue);
	const bool bApply = bEnterPressed || ImGui::Button("OK", ImVec2(82.0f, 0.0f));
	if (bApply)
	{
		RenameEmitter(RenameEmitterIndex, RenameEmitterBuffer);
		RenameEmitterIndex = -1;
		ImGui::CloseCurrentPopup();
	}
	ImGui::SameLine();
	if (ImGui::Button("Cancel", ImVec2(82.0f, 0.0f)))
	{
		RenameEmitterIndex = -1;
		ImGui::CloseCurrentPopup();
	}

	EndParticlePopupModal();
}

void FEditorParticleSystemWidget::SelectParticleSystem()
{
	SelectedEmitterIndex = -1;
	SelectedModuleIndex = NoParticleModuleSelection;
	bEmitterNameEditUndoCaptured = false;
}

void FEditorParticleSystemWidget::SelectEmitter(int32 EmitterIndex)
{
	SelectedEmitterIndex = EmitterIndex;
	SelectedModuleIndex = NoParticleModuleSelection;
	bEmitterNameEditUndoCaptured = false;
}

void FEditorParticleSystemWidget::SelectModule(int32 EmitterIndex, int32 ModuleIndex)
{
	SelectedEmitterIndex = EmitterIndex;
	SelectedModuleIndex = ModuleIndex;
	bEmitterNameEditUndoCaptured = false;
}

void FEditorParticleSystemWidget::OpenEmitterContextMenu(int32 EmitterIndex, int32 ModuleIndex)
{
	ContextEmitterIndex = EmitterIndex;
	ContextModuleIndex = ModuleIndex;
	bOpenEmitterContextMenu = true;
}

void FEditorParticleSystemWidget::ClearEmitterContext()
{
	ContextEmitterIndex = -1;
	ContextModuleIndex = NoParticleModuleSelection;
}

void FEditorParticleSystemWidget::ResetPendingReorders()
{
	PendingEmitterMoveSource = -1;
	PendingEmitterMoveInsertIndex = -1;
	PendingModuleMoveEmitterIndex = -1;
	PendingModuleMoveTargetEmitterIndex = -1;
	PendingModuleMoveSource = -1;
	PendingModuleMoveInsertIndex = -1;
}

void FEditorParticleSystemWidget::ApplyPendingReorders()
{
	if (PendingModuleMoveEmitterIndex >= 0)
	{
		ReorderModule(PendingModuleMoveEmitterIndex, PendingModuleMoveSource, PendingModuleMoveTargetEmitterIndex, PendingModuleMoveInsertIndex);
	}

	if (PendingEmitterMoveSource >= 0)
	{
		ReorderEmitter(PendingEmitterMoveSource, PendingEmitterMoveInsertIndex);
	}

	ResetPendingReorders();
}

void FEditorParticleSystemWidget::ReorderEmitter(int32 SourceIndex, int32 InsertIndex)
{
	if (!ParticleSystemAsset)
	{
		return;
	}

	const int32 EmitterCount = static_cast<int32>(ParticleSystemAsset->Emitters.size());
	if (SourceIndex < 0 || SourceIndex >= EmitterCount)
	{
		return;
	}

	const int32 ClampedInsertIndex = std::clamp(InsertIndex, 0, EmitterCount);
	if (ClampedInsertIndex == SourceIndex || ClampedInsertIndex == SourceIndex + 1)
	{
		return;
	}

	CaptureUndoSnapshot("Reorder Emitter");
	int32 NewEmitterIndex = SourceIndex;
	if (!MoveArrayItemToInsertIndex(ParticleSystemAsset->Emitters, SourceIndex, ClampedInsertIndex, NewEmitterIndex))
	{
		return;
	}

	SelectEmitter(NewEmitterIndex);
	ClearEmitterContext();
	bDirty = true;
	RefreshPreviewComponent(true);
}

void FEditorParticleSystemWidget::ReorderModule(int32 SourceEmitterIndex, int32 SourceModuleIndex, int32 TargetEmitterIndex, int32 InsertIndex)
{
	if (!ParticleSystemAsset ||
		SourceEmitterIndex < 0 ||
		SourceEmitterIndex >= static_cast<int32>(ParticleSystemAsset->Emitters.size()) ||
		TargetEmitterIndex < 0 ||
		TargetEmitterIndex >= static_cast<int32>(ParticleSystemAsset->Emitters.size()))
	{
		return;
	}

	UParticleEmitter* SourceEmitter = ParticleSystemAsset->Emitters[SourceEmitterIndex];
	UParticleEmitter* TargetEmitter = ParticleSystemAsset->Emitters[TargetEmitterIndex];
	if (!SourceEmitter || !TargetEmitter || SourceModuleIndex < 0)
	{
		return;
	}

	UParticleLODLevel* SourceLODLevel = GetEmitterLODLevel(SourceEmitter);
	UParticleLODLevel* TargetLODLevel = GetEmitterLODLevel(TargetEmitter);
	if (!SourceLODLevel || !TargetLODLevel)
	{
		return;
	}

	if (SourceEmitterIndex == TargetEmitterIndex)
	{
		const int32 ModuleCount = static_cast<int32>(SourceLODLevel->Modules.size());
		if (SourceModuleIndex < 0 || SourceModuleIndex >= ModuleCount)
		{
			return;
		}

		const int32 ClampedInsertIndex = std::clamp(InsertIndex, 0, ModuleCount);
		if (ClampedInsertIndex == SourceModuleIndex || ClampedInsertIndex == SourceModuleIndex + 1)
		{
			return;
		}

		CaptureUndoSnapshot("Reorder Particle Module");
		int32 NewModuleIndex = SourceModuleIndex;
		if (!MoveArrayItemToInsertIndex(SourceLODLevel->Modules, SourceModuleIndex, ClampedInsertIndex, NewModuleIndex))
		{
			return;
		}

		SourceEmitter->CacheEmitterModuleInfo();
		SelectModule(SourceEmitterIndex, NewModuleIndex);
		ClearEmitterContext();
		bDirty = true;
		RefreshPreviewComponent(true);
		return;
	}

	if (SourceModuleIndex >= static_cast<int32>(SourceLODLevel->Modules.size()))
	{
		return;
	}

	CaptureUndoSnapshot("Reorder Particle Module");
	UParticleModule* Module = SourceLODLevel->Modules[SourceModuleIndex];
	SourceLODLevel->Modules.erase(SourceLODLevel->Modules.begin() + SourceModuleIndex);
	const int32 NewModuleIndex = std::clamp(InsertIndex, 0, static_cast<int32>(TargetLODLevel->Modules.size()));
	TargetLODLevel->Modules.insert(TargetLODLevel->Modules.begin() + NewModuleIndex, Module);

	SourceEmitter->CacheEmitterModuleInfo();
	TargetEmitter->CacheEmitterModuleInfo();
	SelectModule(TargetEmitterIndex, NewModuleIndex);
	ClearEmitterContext();
	bDirty = true;
	RefreshPreviewComponent(true);
}

void FEditorParticleSystemWidget::DrawEmitterColumn(UParticleEmitter* Emitter, int32 EmitterIndex, float ColumnHeight)
{
	constexpr float ColumnWidth = 180.0f;
	constexpr float HeaderHeight = 62.0f;
	constexpr float TypeRowHeight = 22.0f;
	constexpr float ModuleRowHeight = 24.0f;

	UParticleLODLevel* LODLevel = GetEmitterLODLevel(Emitter);

	const ImVec2 ColumnMin = ImGui::GetCursorScreenPos();
	const ImVec2 ColumnMax(ColumnMin.x + ColumnWidth, ColumnMin.y + ColumnHeight);
	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	DrawList->AddRectFilled(ColumnMin, ColumnMax, ImGui::GetColorU32(ImVec4(0.075f, 0.076f, 0.088f, 1.0f)));
	DrawList->AddRect(ColumnMin, ColumnMax, ImGui::GetColorU32(ImVec4(0.02f, 0.02f, 0.025f, 1.0f)));

	ImGui::PushID(EmitterIndex);

	ImGui::InvisibleButton("##EmitterHeader", ImVec2(ColumnWidth, HeaderHeight));
	const bool bHeaderHovered = ImGui::IsItemHovered();
	if (ImGui::IsItemClicked())
	{
		SelectEmitter(EmitterIndex);
	}
	if (bHeaderHovered && ImGui::IsMouseReleased(ImGuiMouseButton_Right))
	{
		SelectEmitter(EmitterIndex);
		OpenEmitterContextMenu(EmitterIndex, NoParticleModuleSelection);
	}

	const ImVec2 HeaderMin = ImGui::GetItemRectMin();
	const ImVec2 HeaderMax = ImGui::GetItemRectMax();
	const bool bHeaderSelected = SelectedEmitterIndex == EmitterIndex && SelectedModuleIndex == NoParticleModuleSelection;
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
		const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload(ParticleEmitterDragPayloadType, ParticleDragDropTargetFlags);
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
	const bool bTypeRowHovered = ImGui::IsItemHovered();
	if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
	{
		SelectEmitter(EmitterIndex);
	}
	if (bTypeRowHovered && ImGui::IsMouseReleased(ImGuiMouseButton_Right))
	{
		SelectEmitter(EmitterIndex);
		OpenEmitterContextMenu(EmitterIndex, TypeDataParticleModuleSelection);
	}
	const ImVec2 TypeMin = ImGui::GetItemRectMin();
	const ImVec2 TypeMax = ImGui::GetItemRectMax();
	DrawList->AddRectFilled(TypeMin, TypeMax, ImGui::GetColorU32(ImVec4(0.055f, 0.056f, 0.066f, 1.0f)));
	DrawList->AddText(ImVec2(TypeMin.x + 10.0f, TypeMin.y + 3.0f), ImGui::GetColorU32(ImVec4(0.76f, 0.79f, 0.84f, 1.0f)), GetRenderModeLabel(LODLevel));

	if (LODLevel && LODLevel->GetRequiredModule())
	{
		DrawEmitterModuleRow(LODLevel->GetRequiredModule(), EmitterIndex, RequiredParticleModuleSelection, true, ModuleRowHeight);
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
		if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
		{
			SelectEmitter(EmitterIndex);
		}
		if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Right))
		{
			SelectEmitter(EmitterIndex);
			OpenEmitterContextMenu(EmitterIndex, NoParticleModuleSelection);
		}
		if (ImGui::BeginDragDropTarget())
		{
			const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload(ParticleModuleDragPayloadType, ParticleDragDropTargetFlags);
			if (Payload && Payload->DataSize == sizeof(FModuleDragPayload) && LODLevel)
			{
				const FModuleDragPayload* DragPayload = static_cast<const FModuleDragPayload*>(Payload->Data);
				const int32 InsertIndex = static_cast<int32>(LODLevel->Modules.size());
				const bool bSameEmitter = DragPayload->SourceEmitterIndex == EmitterIndex;
				const bool bNoMove = bSameEmitter &&
					(DragPayload->SourceModuleIndex == InsertIndex || DragPayload->SourceModuleIndex + 1 == InsertIndex);
				if (!bNoMove)
				{
					const ImVec2 EmptyMin = ImGui::GetItemRectMin();
					const ImVec2 EmptyMax = ImGui::GetItemRectMax();
					DrawList->AddLine(
						ImVec2(EmptyMin.x, EmptyMin.y + 1.0f),
						ImVec2(EmptyMax.x, EmptyMin.y + 1.0f),
						ImGui::GetColorU32(ImVec4(0.35f, 0.70f, 1.0f, 1.0f)),
						2.0f);
					if (Payload->Delivery)
					{
						PendingModuleMoveEmitterIndex = DragPayload->SourceEmitterIndex;
						PendingModuleMoveTargetEmitterIndex = EmitterIndex;
						PendingModuleMoveSource = DragPayload->SourceModuleIndex;
						PendingModuleMoveInsertIndex = InsertIndex;
					}
				}
			}
			ImGui::EndDragDropTarget();
		}
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
		SelectModule(EmitterIndex, ModuleIndex);
	}
	if (bHovered && ImGui::IsMouseReleased(ImGuiMouseButton_Right))
	{
		SelectModule(EmitterIndex, ModuleIndex);
		OpenEmitterContextMenu(EmitterIndex, ModuleIndex);
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
	if (ImGui::BeginDragDropTarget())
	{
		const ImGuiPayload* Payload = ImGui::AcceptDragDropPayload(ParticleModuleDragPayloadType, ParticleDragDropTargetFlags);
		if (Payload && Payload->DataSize == sizeof(FModuleDragPayload))
		{
			const FModuleDragPayload* DragPayload = static_cast<const FModuleDragPayload*>(Payload->Data);
			const bool bDropAfter = !bRequired && ImGui::GetIO().MousePos.y > (Min.y + Max.y) * 0.5f;
			const int32 InsertIndex = bRequired ? 0 : ModuleIndex + (bDropAfter ? 1 : 0);
			const bool bSameEmitter = DragPayload->SourceEmitterIndex == EmitterIndex;
			const bool bNoMove = bSameEmitter &&
				(DragPayload->SourceModuleIndex == InsertIndex || DragPayload->SourceModuleIndex + 1 == InsertIndex);
			if (!bNoMove)
			{
				bShowModuleInsertMarker = true;
				ModuleInsertMarkerY = (bRequired || bDropAfter) ? Max.y - 1.0f : Min.y + 1.0f;
				if (Payload->Delivery)
				{
					PendingModuleMoveEmitterIndex = DragPayload->SourceEmitterIndex;
					PendingModuleMoveTargetEmitterIndex = EmitterIndex;
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

	const ImVec2 BodySize(Size.x, std::max(1.0f, Size.y - PanelHeaderHeight));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f, 12.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 0.0f));
	ImGui::BeginChild(
		"##ParticleDetailsBody",
		BodySize,
		false,
		ImGuiWindowFlags_AlwaysVerticalScrollbar);
	ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX() + 8.0f, ImGui::GetCursorPosY() + 8.0f));
	ImGui::BeginGroup();

	UParticleEmitter* SelectedEmitter = GetSelectedEmitter();
	UParticleModule* SelectedModule = GetSelectedModule();
	if (!ParticleSystemAsset)
	{
		ImGui::TextDisabled("No particle system.");
	}
	else if (SelectedEmitterIndex < 0 || !SelectedEmitter)
	{
		DrawParticleSystemDetails(ParticleSystemAsset);
	}
	else if (!SelectedModule)
	{
		DrawEmitterDetails(SelectedEmitter, SelectedEmitterIndex);
	}
	else
	{
		DrawParticleModuleDetails(SelectedModule, SelectedEmitter);
	}

	ImGui::EndGroup();
	ImGui::EndChild();
	ImGui::PopStyleVar(2);
}

UParticleEmitter* FEditorParticleSystemWidget::GetSelectedEmitter() const
{
	if (!ParticleSystemAsset ||
		SelectedEmitterIndex < 0 ||
		SelectedEmitterIndex >= static_cast<int32>(ParticleSystemAsset->Emitters.size()))
	{
		return nullptr;
	}
	return ParticleSystemAsset->Emitters[SelectedEmitterIndex];
}

UParticleLODLevel* FEditorParticleSystemWidget::GetEmitterLODLevel(UParticleEmitter* Emitter) const
{
	if (!Emitter)
	{
		return nullptr;
	}

	UParticleLODLevel* LODLevel = Emitter->GetLODLevel(CurrentLOD);
	if (!LODLevel)
	{
		LODLevel = Emitter->GetLODLevel(0);
	}
	return LODLevel;
}

UParticleLODLevel* FEditorParticleSystemWidget::GetSelectedLODLevel() const
{
	return GetEmitterLODLevel(GetSelectedEmitter());
}

UParticleModule* FEditorParticleSystemWidget::GetSelectedModule() const
{
	UParticleLODLevel* LODLevel = GetSelectedLODLevel();
	if (!LODLevel)
	{
		return nullptr;
	}

	if (SelectedModuleIndex == RequiredParticleModuleSelection)
	{
		return LODLevel->GetRequiredModule();
	}
	if (SelectedModuleIndex >= 0 && SelectedModuleIndex < static_cast<int32>(LODLevel->Modules.size()))
	{
		return LODLevel->Modules[SelectedModuleIndex];
	}
	return nullptr;
}

void FEditorParticleSystemWidget::DrawEmitterDetails(UParticleEmitter* Emitter, int32 EmitterIndex)
{
	if (!Emitter)
	{
		return;
	}

	ImGui::PushID(Emitter);

	const float AvailableWidth = ImGui::GetContentRegionAvail().x;
	const float LabelWidth = std::clamp(AvailableWidth * 0.38f, 180.0f, 300.0f);

	auto SyncEmitterNameBuffer = [&]()
	{
		const FString EmitterName = GetEmitterDisplayName(Emitter, EmitterIndex);
		std::snprintf(DetailEmitterNameEditBuffer, sizeof(DetailEmitterNameEditBuffer), "%s", EmitterName.c_str());
		DetailEmitterNameEditIndex = EmitterIndex;
	};

	auto WarnAndSyncEmptyEmitterName = [&]()
	{
		if (EditorEngine)
		{
			EditorEngine->GetNotificationService().Warning("Emitter name cannot be empty.");
		}
		SyncEmitterNameBuffer();
	};

	auto ApplyLiveEmitterNameBuffer = [&](bool bWarnOnEmpty)
	{
		const FString TrimmedName = TrimCopy(DetailEmitterNameEditBuffer);
		if (TrimmedName.empty())
		{
			if (bWarnOnEmpty)
			{
				WarnAndSyncEmptyEmitterName();
			}
			return;
		}

		if (Emitter->GetFName().ToString() == TrimmedName)
		{
			return;
		}

		if (!bEmitterNameEditUndoCaptured)
		{
			CaptureUndoSnapshot("Rename Emitter");
			bEmitterNameEditUndoCaptured = true;
		}
		ApplyEmitterName(EmitterIndex, TrimmedName, false, false);
	};

	if (DetailEmitterNameEditIndex != EmitterIndex)
	{
		SyncEmitterNameBuffer();
		bEmitterNameEditUndoCaptured = false;
	}

	if (ImGui::CollapsingHeader("Particle", ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (BeginParticleDetailsTable("##ParticleEmitterDetailsTable", LabelWidth))
		{
			BeginParticleDetailsRow("Emitter Name", 30.0f);

			ImGui::SetNextItemWidth(std::min(220.0f, ImGui::GetContentRegionAvail().x));
			const bool bNameChanged = ImGui::InputText(
				"##EmitterName",
				DetailEmitterNameEditBuffer,
				sizeof(DetailEmitterNameEditBuffer));
			const bool bNameActive = ImGui::IsItemActive();
			const bool bNameDeactivatedAfterEdit = ImGui::IsItemDeactivatedAfterEdit();
			if (bNameChanged)
			{
				ApplyLiveEmitterNameBuffer(false);
			}
			if (bNameDeactivatedAfterEdit)
			{
				ApplyLiveEmitterNameBuffer(true);
				SyncEmitterNameBuffer();
				bEmitterNameEditUndoCaptured = false;
			}
			else if (!bNameActive)
			{
				const FString CurrentName = GetEmitterDisplayName(Emitter, EmitterIndex);
				if (CurrentName != DetailEmitterNameEditBuffer)
				{
					SyncEmitterNameBuffer();
				}
			}

			EndParticleDetailsTable();
		}
	}

	ImGui::PopID();
}

void FEditorParticleSystemWidget::DrawParticleSystemDetails(UParticleSystem* ParticleSystem)
{
	if (!ParticleSystem)
	{
		return;
	}

	ImGui::PushID(ParticleSystem);

	const float AvailableWidth = ImGui::GetContentRegionAvail().x;
	const float LabelWidth = std::clamp(AvailableWidth * 0.38f, 180.0f, 300.0f);

	auto CaptureSystemEditUndo = [this]()
	{
		if (!bPropertyEditUndoCaptured)
		{
			CaptureUndoSnapshot("Edit Particle System");
			bPropertyEditUndoCaptured = true;
		}
	};

	auto DrawFloatRow = [&](const char* Label, const char* Id, float& Value, float Speed, float MinValue, const char* Format)
	{
		BeginParticleDetailsRow(Label);
		float EditedValue = Value;
		ImGui::SetNextItemWidth(-1.0f);
		if (ImGui::DragFloat(Id, &EditedValue, Speed, MinValue, 0.0f, Format))
		{
			CaptureSystemEditUndo();
			Value = std::max(MinValue, EditedValue);
			bDirty = true;
		}
		if (ImGui::IsItemDeactivatedAfterEdit())
		{
			bPropertyEditUndoCaptured = false;
		}
	};

	auto DrawBoolRow = [&](const char* Label, const char* Id, bool& Value)
	{
		BeginParticleDetailsRow(Label);
		bool EditedValue = Value;
		if (ImGui::Checkbox(Id, &EditedValue))
		{
			CaptureSystemEditUndo();
			Value = EditedValue;
			bDirty = true;
		}
	};

	auto DrawComboRow = [&](const char* Label, const char* Id, int32& Value, const char* const* Items, int32 ItemCount)
	{
		BeginParticleDetailsRow(Label);
		int32 EditedValue = std::clamp(Value, 0, std::max(0, ItemCount - 1));
		ImGui::SetNextItemWidth(-1.0f);
		if (ParticleCombo(Id, &EditedValue, Items, ItemCount))
		{
			CaptureSystemEditUndo();
			Value = EditedValue;
			bDirty = true;
		}
	};

	auto DrawArraySummaryRow = [&](const char* Label, const char* Id, int32 ElementCount, bool bShowControls)
	{
		BeginParticleDetailsRow(Label);
		ImGui::Text("%d Array elements", ElementCount);
		if (bShowControls)
		{
			ImGui::SameLine(0.0f, 12.0f);
			ImGui::BeginDisabled();
			ImGui::SmallButton((FString("+##") + Id).c_str());
			ImGui::SameLine(0.0f, 6.0f);
			ImGui::SmallButton((FString("Delete##") + Id).c_str());
			ImGui::EndDisabled();
		}
	};

	if (ImGui::CollapsingHeader("Particle System", ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (BeginParticleDetailsTable("##ParticleSystemDetailsTable", LabelWidth))
		{
			const char* UpdateModeItems[] = { "Real-Time", "Fixed Time" };
			DrawFloatRow("Update Time FPS", "##UpdateTimeFPS", ParticleSystem->UpdateTimeFPS, 0.1f, 0.0f, "%.1f");
			DrawFloatRow("Warmup Time - beware hitches!", "##WarmupTime", ParticleSystem->WarmupTime, 0.1f, 0.0f, "%.1f");
			DrawFloatRow("Warmup Tick Rate", "##WarmupTickRate", ParticleSystem->WarmupTickRate, 0.1f, 0.0f, "%.1f");
			DrawFloatRow("Seconds Before Inactive", "##SecondsBeforeInactive", ParticleSystem->SecondsBeforeInactive, 0.1f, 0.0f, "%.1f");
			DrawBoolRow("Orient ZAxis Toward Camera", "##OrientZAxisTowardCamera", ParticleSystem->bOrientZAxisTowardCamera);
			DrawComboRow("System Update Mode", "##SystemUpdateMode", ParticleSystem->SystemUpdateMode, UpdateModeItems, IM_ARRAYSIZE(UpdateModeItems));
			EndParticleDetailsTable();
		}
	}

	if (ImGui::CollapsingHeader("Thumbnail", ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (BeginParticleDetailsTable("##ParticleSystemThumbnailTable", LabelWidth))
		{
			DrawFloatRow("Thumbnail Warmup", "##ThumbnailWarmup", ParticleSystem->ThumbnailWarmup, 0.1f, 0.0f, "%.1f");
			DrawBoolRow("Use Realtime Thumbnail", "##UseRealtimeThumbnail", ParticleSystem->bUseRealtimeThumbnail);
			EndParticleDetailsTable();
		}
	}

	if (ImGui::CollapsingHeader("LOD", ImGuiTreeNodeFlags_DefaultOpen))
	{
		if (BeginParticleDetailsTable("##ParticleSystemLODTable", LabelWidth))
		{
			const char* LODMethodItems[] = { "Automatic", "Direct Set" };
			DrawFloatRow("LODDistance Check Time", "##LODDistanceCheckTime", ParticleSystem->LODDistanceCheckTime, 0.01f, 0.0f, "%.2f");
			DrawArraySummaryRow("LODDistances", "LODDistances", static_cast<int32>(ParticleSystem->LODDistances.size()), false);
			DrawArraySummaryRow("LODSettings", "LODSettings", static_cast<int32>(ParticleSystem->LODSettings.size()), true);
			DrawComboRow("LODMethod", "##LODMethod", ParticleSystem->LODMethod, LODMethodItems, IM_ARRAYSIZE(LODMethodItems));
			EndParticleDetailsTable();
		}
	}

	if (!ImGui::IsAnyItemActive())
	{
		bPropertyEditUndoCaptured = false;
	}

	ImGui::PopID();
}

void FEditorParticleSystemWidget::DrawParticleModuleDetails(UParticleModule* Module, UParticleEmitter* OwnerEmitter)
{
	if (!Module)
	{
		return;
	}

	const bool bRequired = SelectedModuleIndex == RequiredParticleModuleSelection;
	ImGui::PushID(Module);
	ImGui::TextUnformatted(GetModuleDisplayName(Module, bRequired).c_str());
	if (Module->GetClass())
	{
		ImGui::TextDisabled("%s", Module->GetClass()->GetName());
	}
	ImGui::Dummy(ImVec2(0.0f, 4.0f));
	ImGui::Separator();
	ImGui::Dummy(ImVec2(0.0f, 8.0f));

	TArray<const FProperty*> Properties;
	if (Module->GetClass())
	{
		Module->GetClass()->GetAllProperties(Properties);
	}

	const float AvailableWidth = ImGui::GetContentRegionAvail().x;
	const float LabelWidth = std::clamp(AvailableWidth * 0.38f, 128.0f, 190.0f);

	auto DrawPropertyTable = [&](const char* TableId, const char* CategoryFilter, bool bIncludeUncategorized) -> int32
	{
		int32 RenderedPropertyCount = 0;
		ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(4.0f, 3.0f));
		if (ImGui::BeginTable(TableId, 2, ImGuiTableFlags_SizingStretchProp))
		{
			ImGui::TableSetupColumn("Property", ImGuiTableColumnFlags_WidthFixed, LabelWidth);
			ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch);
			for (const FProperty* Property : Properties)
			{
				if (!Property || !Property->Name || !Property->IsEditable() || IsInternalParticleModuleProperty(*Property))
				{
					continue;
				}

				const bool bHasCategory = Property->Category && Property->Category[0] != '\0';
				const bool bCategoryMatch = CategoryFilter && bHasCategory && std::strcmp(Property->Category, CategoryFilter) == 0;
				const bool bUncategorizedMatch = bIncludeUncategorized && !bHasCategory;
				if (!bCategoryMatch && !bUncategorizedMatch)
				{
					continue;
				}

				ImGui::PushID(Property->Name);
				ImGui::TableNextRow(ImGuiTableRowFlags_None, 30.0f);
				ImGui::TableSetColumnIndex(0);
				ImGui::AlignTextToFramePadding();
				ImGui::TextUnformatted(GetPropertyDisplayName(*Property));
				ImGui::TableSetColumnIndex(1);
				ImGui::SetNextItemWidth(-1.0f);
				DrawParticleModuleProperty(Module, *Property);
				ImGui::PopID();
				++RenderedPropertyCount;
			}
			ImGui::EndTable();
		}
		ImGui::PopStyleVar();
		return RenderedPropertyCount;
	};

	int32 RenderedPropertyCount = 0;
	if (Cast<UParticleModuleRequired>(Module))
	{
		if (ImGui::CollapsingHeader("Emitter", ImGuiTreeNodeFlags_DefaultOpen))
		{
			RenderedPropertyCount += DrawPropertyTable("##ParticleRequiredEmitterTable", "Emitter", false);
		}
		if (ImGui::CollapsingHeader("SubUV", ImGuiTreeNodeFlags_DefaultOpen))
		{
			RenderedPropertyCount += DrawPropertyTable("##ParticleRequiredSubUVTable", "SubUV", false);
		}
		if (ImGui::CollapsingHeader("Required", ImGuiTreeNodeFlags_DefaultOpen))
		{
			RenderedPropertyCount += DrawPropertyTable("##ParticleRequiredGeneralTable", nullptr, true);
		}
	}
	else
	{
		RenderedPropertyCount = DrawPropertyTable("##ParticleModuleDetailsTable", nullptr, true);
	}

	if (RenderedPropertyCount == 0)
	{
		ImGui::TextDisabled("No editable properties.");
	}

	(void)OwnerEmitter;
	ImGui::PopID();
}

bool FEditorParticleSystemWidget::DrawParticleModuleProperty(UParticleModule* Module, const FProperty& Property)
{
	if (!Module || !Property.Name)
	{
		return false;
	}

	void* ValuePtr = Property.GetValuePtr(Module);
	const FString Label = FString("##") + Property.Name;
	const bool bChanged = DrawParticlePropertyValue(Property, ValuePtr, Module, Label.c_str());
	if (ImGui::IsItemActivated() && !bPropertyEditUndoCaptured)
	{
		CaptureUndoSnapshot("Edit Particle Module");
		bPropertyEditUndoCaptured = true;
	}
	if (bChanged)
	{
		if (!bPropertyEditUndoCaptured)
		{
			CaptureUndoSnapshot("Edit Particle Module");
			bPropertyEditUndoCaptured = true;
		}
		NotifyParticleModulePropertyChanged(Module, GetSelectedEmitter(), Property);
		RefreshPreviewComponent(false);
	}
	if (ImGui::IsItemDeactivatedAfterEdit() || !ImGui::IsAnyItemActive())
	{
		bPropertyEditUndoCaptured = false;
	}
	return bChanged;
}

bool FEditorParticleSystemWidget::DrawParticlePropertyValue(const FProperty& Property, void* ValuePtr, UObject* NotifyTarget, const char* Label)
{
	if (!ValuePtr)
	{
		return false;
	}

	switch (Property.Type)
	{
	case EPropertyType::Bool:
		return ImGui::Checkbox(Label, static_cast<bool*>(ValuePtr));
	case EPropertyType::Int:
	{
		int32* Value = static_cast<int32*>(ValuePtr);
		const bool bChanged = ImGui::DragInt(Label, Value, Property.Speed);
		if (bChanged)
		{
			if (Property.Min != 0.0f)
			{
				*Value = std::max(*Value, static_cast<int32>(Property.Min));
			}
			if (Property.Max != 0.0f)
			{
				*Value = std::min(*Value, static_cast<int32>(Property.Max));
			}
		}
		return bChanged;
	}
	case EPropertyType::Float:
	{
		float* Value = static_cast<float*>(ValuePtr);
		const bool bChanged = ImGui::DragFloat(Label, Value, Property.Speed);
		if (bChanged)
		{
			if (Property.Min != 0.0f)
			{
				*Value = std::max(*Value, Property.Min);
			}
			if (Property.Max != 0.0f)
			{
				*Value = std::min(*Value, Property.Max);
			}
		}
		return bChanged;
	}
	case EPropertyType::Struct:
		return DrawParticleStructPropertyValue(Property, ValuePtr, NotifyTarget, Label);
	case EPropertyType::String:
	{
		FString* Value = static_cast<FString*>(ValuePtr);
		char Buffer[512];
		strncpy_s(Buffer, sizeof(Buffer), Value->c_str(), _TRUNCATE);
		if (ImGui::InputText(Label, Buffer, sizeof(Buffer)))
		{
			*Value = Buffer;
			return true;
		}
		return false;
	}
	case EPropertyType::Name:
	{
		FName* Value = static_cast<FName*>(ValuePtr);
		FString Current = Value->ToString();
		const char* DisplayName = GetPropertyDisplayName(Property);
		const bool bSubUVProperty =
			(Property.Name && std::strcmp(Property.Name, "SubUVName") == 0) ||
			(DisplayName && std::strcmp(DisplayName, "SubUV") == 0);
		const TArray<FString>* Names = (bSubUVProperty && EditorEngine)
			? &EditorEngine->GetAssetService().GetSubUVNames()
			: nullptr;

		if (Names && !Names->empty())
		{
			bool bChanged = false;
			const char* Preview = Current.empty() || Current == FName::None.ToString() ? "<None>" : Current.c_str();
			if (BeginParticleCombo(Label, Preview))
			{
				const bool bNoneSelected = Current.empty() || Current == FName::None.ToString();
				if (ImGui::Selectable("<None>", bNoneSelected))
				{
					*Value = FName::None;
					bChanged = true;
				}
				for (const FString& Name : *Names)
				{
					const bool bSelected = Current == Name;
					if (ImGui::Selectable(Name.c_str(), bSelected))
					{
						*Value = FName(Name);
						bChanged = true;
					}
					if (bSelected)
					{
						ImGui::SetItemDefaultFocus();
					}
				}
				EndParticleCombo();
			}
			return bChanged;
		}

		char Buffer[256];
		strncpy_s(Buffer, sizeof(Buffer), Current.c_str(), _TRUNCATE);
		if (ImGui::InputText(Label, Buffer, sizeof(Buffer)))
		{
			*Value = FName(Buffer);
			return true;
		}
		return false;
	}
	case EPropertyType::Enum:
	{
		if (!Property.EnumMeta || !Property.EnumMeta->Values || Property.EnumMeta->Count == 0)
		{
			return false;
		}

		int64 CurrentValue = 0;
		switch (Property.EnumMeta->Size)
		{
		case 1: CurrentValue = static_cast<int64>(*static_cast<uint8*>(ValuePtr)); break;
		case 2: CurrentValue = static_cast<int64>(*static_cast<uint16*>(ValuePtr)); break;
		case 4: CurrentValue = static_cast<int64>(*static_cast<int32*>(ValuePtr)); break;
		case 8: CurrentValue = static_cast<int64>(*static_cast<int64*>(ValuePtr)); break;
		default: break;
		}

		int32 CurrentIndex = 0;
		for (uint32 Index = 0; Index < Property.EnumMeta->Count; ++Index)
		{
			if (Property.EnumMeta->Values[Index].Value == CurrentValue)
			{
				CurrentIndex = static_cast<int32>(Index);
				break;
			}
		}

		const auto ComboGetter = [](void* Data, int Index) -> const char*
		{
			const UEnum* EnumMeta = static_cast<const UEnum*>(Data);
			if (!EnumMeta || Index < 0 || static_cast<uint32>(Index) >= EnumMeta->Count)
			{
				return "";
			}
			const FEnumValue& ValueMeta = EnumMeta->Values[Index];
			return (ValueMeta.DisplayName && ValueMeta.DisplayName[0] != '\0') ? ValueMeta.DisplayName : ValueMeta.Name;
		};

		if (ParticleCombo(Label, &CurrentIndex, ComboGetter, const_cast<UEnum*>(Property.EnumMeta), static_cast<int>(Property.EnumMeta->Count)))
		{
			const int64 NewValue = Property.EnumMeta->Values[CurrentIndex].Value;
			switch (Property.EnumMeta->Size)
			{
			case 1: *static_cast<uint8*>(ValuePtr) = static_cast<uint8>(NewValue); break;
			case 2: *static_cast<uint16*>(ValuePtr) = static_cast<uint16>(NewValue); break;
			case 4: *static_cast<int32*>(ValuePtr) = static_cast<int32>(NewValue); break;
			case 8: *static_cast<int64*>(ValuePtr) = static_cast<int64>(NewValue); break;
			default: break;
			}
			return true;
		}
		return false;
	}
	case EPropertyType::ObjectPtr:
	{
		if (!Property.ObjectPtrOps)
		{
			return false;
		}

		UObject* CurrentObject = Property.ObjectPtrOps->GetObject(ValuePtr);
		const bool bMaterialAsset =
			Property.ReferenceKind == EObjectReferenceKind::Asset &&
			Property.ObjectClass &&
			Property.ObjectClass->IsChildOf(UMaterialInterface::StaticClass());
		if (bMaterialAsset && EditorEngine)
		{
			FEditorAssetService& AssetService = EditorEngine->GetAssetService();
			const TArray<FString>& MaterialNames = AssetService.GetMaterialInterfaceNames();
			UMaterialInterface* CurrentMaterial = Cast<UMaterialInterface>(CurrentObject);
			const FString CurrentIdentifier = CurrentMaterial
				? (CurrentMaterial->GetFilePath().empty() ? CurrentMaterial->GetName() : FPaths::Normalize(CurrentMaterial->GetFilePath()))
				: FString();
			const FString CurrentLabel = CurrentIdentifier.empty() ? FString("None") : CurrentIdentifier;
			bool bChanged = false;

			if (BeginParticleCombo(Label, CurrentLabel.c_str()))
			{
				if (ImGui::Selectable("None", CurrentMaterial == nullptr))
				{
					Property.ObjectPtrOps->SetObject(ValuePtr, nullptr);
					bChanged = true;
				}

				for (int32 MaterialIndex = 0; MaterialIndex < static_cast<int32>(MaterialNames.size()); ++MaterialIndex)
				{
					ImGui::PushID(MaterialIndex);
					const FString& MaterialLabel = MaterialNames[MaterialIndex].empty()
						? FString("<Unnamed Material>")
						: MaterialNames[MaterialIndex];
					const bool bSelected = CurrentIdentifier == MaterialLabel;
					if (ImGui::Selectable(MaterialLabel.c_str(), bSelected))
					{
						if (UMaterialInterface* Candidate = AssetService.ResolveMaterialInterfaceByIndex(MaterialIndex))
						{
							Property.ObjectPtrOps->SetObject(ValuePtr, Candidate);
							bChanged = true;
						}
					}
					if (bSelected)
					{
						ImGui::SetItemDefaultFocus();
					}
					ImGui::PopID();
				}
				EndParticleCombo();
			}
			return bChanged;
		}

		ImGui::TextDisabled("%s <unsupported object>", Label);
		return false;
	}
	default:
		ImGui::TextDisabled("%s <unsupported>", Label);
		break;
	}

	return false;
}

bool FEditorParticleSystemWidget::DrawParticleStructPropertyValue(const FProperty& Property, void* ValuePtr, UObject* NotifyTarget, const char* Label)
{
	if (!ValuePtr)
	{
		return false;
	}

	const char* Hint = Property.EditorHint;
	if ((!Hint || Hint[0] == '\0') && Property.ScriptStruct)
	{
		Hint = Property.ScriptStruct->GetName();
	}

	if (Hint && std::strcmp(Hint, "FVector") == 0)
	{
		return ImGui::DragFloat3(Label, static_cast<float*>(ValuePtr), Property.Speed);
	}
	if (Hint && std::strcmp(Hint, "FVector4") == 0)
	{
		return ImGui::DragFloat4(Label, static_cast<float*>(ValuePtr), Property.Speed);
	}
	if (Hint && std::strcmp(Hint, "FColor") == 0)
	{
		return ImGui::ColorEdit4(Label, &static_cast<FColor*>(ValuePtr)->R);
	}

	if (!Property.ScriptStruct)
	{
		ImGui::TextDisabled("%s <unregistered struct>", Label);
		return false;
	}

	bool bChanged = false;
	if (ImGui::TreeNodeEx(Label, ImGuiTreeNodeFlags_DefaultOpen))
	{
		TArray<const FProperty*> ChildProperties;
		Property.ScriptStruct->GetAllProperties(ChildProperties);
		for (const FProperty* Child : ChildProperties)
		{
			if (!Child || !Child->Name)
			{
				continue;
			}

			void* ChildPtr = reinterpret_cast<uint8*>(ValuePtr) + Child->Offset;
			const FString ChildLabel = MakeParticlePropertyLabel(*Child);
			if (DrawParticlePropertyValue(*Child, ChildPtr, NotifyTarget, ChildLabel.c_str()))
			{
				bChanged = true;
			}
		}
		ImGui::TreePop();
	}
	return bChanged;
}

void FEditorParticleSystemWidget::NotifyParticleModulePropertyChanged(UParticleModule* Module, UParticleEmitter* OwnerEmitter, const FProperty& Property)
{
	if (Module && Property.Name)
	{
		Module->PostEditProperty(Property.Name);
	}
	if (OwnerEmitter)
	{
		OwnerEmitter->CacheEmitterModuleInfo();
	}
	if (ParticleSystemAsset)
	{
		ParticleSystemAsset->CacheEmitterModuleInfo();
	}
	bDirty = true;
}

void FEditorParticleSystemWidget::DrawCurveEditorPanel(const ImVec2& Size)
{
	DrawPanelHeader("Curve Editor");

	const ImVec2 BodySize(Size.x, std::max(1.0f, Size.y - PanelHeaderHeight));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12.0f, 10.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 7.0f));
	ImGui::BeginChild(
		"##ParticleCurveEditorBody",
		BodySize,
		false,
		ImGuiWindowFlags_AlwaysVerticalScrollbar);
	ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX() + 8.0f, ImGui::GetCursorPosY() + 8.0f));
	ImGui::BeginGroup();

	TArray<FString> CurvePaths = FResourceManager::Get().GetCurvePaths();
	std::sort(CurvePaths.begin(), CurvePaths.end());

	const bool bSelectedPathExists = !SelectedCurveAssetPath.empty() &&
		std::find(CurvePaths.begin(), CurvePaths.end(), SelectedCurveAssetPath) != CurvePaths.end();
	if (!SelectedCurveAssetPath.empty() && !bSelectedPathExists)
	{
		SelectedCurveAssetPath.clear();
		CurveEditorWidget.Clear();
	}

	ImGui::SetNextItemWidth(-1.0f);
	const char* CurrentCurveLabel = SelectedCurveAssetPath.empty() ? "<None>" : SelectedCurveAssetPath.c_str();
	if (BeginParticleCombo("##ParticleCurveAsset", CurrentCurveLabel))
	{
		const bool bNoneSelected = SelectedCurveAssetPath.empty();
		if (ImGui::Selectable("<None>", bNoneSelected))
		{
			SelectedCurveAssetPath.clear();
			CurveEditorWidget.Clear();
		}
		if (bNoneSelected)
		{
			ImGui::SetItemDefaultFocus();
		}

		for (const FString& CurvePath : CurvePaths)
		{
			const bool bSelected = SelectedCurveAssetPath == CurvePath;
			if (ImGui::Selectable(CurvePath.c_str(), bSelected))
			{
				SelectedCurveAssetPath = CurvePath;
				CurveEditorWidget.OpenCurveAsset(SelectedCurveAssetPath);
			}
			if (bSelected)
			{
				ImGui::SetItemDefaultFocus();
			}
		}
		EndParticleCombo();
	}

	if (CurvePaths.empty())
	{
		ImGui::TextDisabled("No curve assets found.");
	}

	ImGui::Separator();
	CurveEditorWidget.RenderEmbedded(LastDeltaTime);

	ImGui::EndGroup();
	ImGui::EndChild();
	ImGui::PopStyleVar(2);
}
