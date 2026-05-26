// Manages emitter/module list UI, context menus, add/delete, rename, and reorder operations.
#include "Editor/UI/EditorParticleSystemWidgetPrivate.h"

// Cycle 14 (M2): Mesh emitter 전용 RotationRate module — add-module 메뉴에서 사용자 노출.
#include "Particle/ParticleModuleMeshRotationRate.h"

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
	UParticleModule* TargetModule =
		TargetLODLevel &&
		ContextModuleIndex >= 0 &&
		ContextModuleIndex < static_cast<int32>(TargetLODLevel->Modules.size())
			? TargetLODLevel->Modules[ContextModuleIndex]
			: nullptr;
	const EParticleEmitterRenderMode CurrentRenderMode = TargetLODLevel
		? TargetLODLevel->GetEffectiveRenderMode()
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
		if (Cast<UParticleModuleTypeDataBase>(TargetModule))
		{
			ImGui::TextDisabled("EMITTER TYPE");
			ImGui::Separator();
			DrawTypeDataItems(false);
			EndParticlePopup();
			return;
		}
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
	DrawParticleModuleAddMenu<UParticleModuleAcceleration>("Acceleration", "Acceleration", bHasTargetEmitter, AddModuleToTargetEmitter);
	DrawDisabledParticleModuleMenu("Attraction");
	DrawParticleModuleAddMenu<UParticleModuleBurst>("Burst", "Burst", bHasTargetEmitter, AddModuleToTargetEmitter);
	DrawDisabledParticleModuleMenu("Camera");
	DrawParticleModuleAddMenu<UParticleModuleCollision>("Collision", "Collision", bHasTargetEmitter, AddModuleToTargetEmitter);
	DrawParticleModuleAddMenu<UParticleModuleColor>("Color", "Color Over Life", bHasTargetEmitter, AddModuleToTargetEmitter);
	DrawParticleModuleAddMenu<UParticleModuleEventGenerator>("Event", "Event Generator", bHasTargetEmitter, AddModuleToTargetEmitter);
	DrawDisabledParticleModuleMenu("Kill");
	DrawParticleModuleAddMenu<UParticleModuleLifetime>("Lifetime", "Lifetime", bHasTargetEmitter, AddModuleToTargetEmitter);
	DrawParticleModuleAddMenu<UParticleModuleLight>("Light", "Light", bHasTargetEmitter, AddModuleToTargetEmitter);
	DrawParticleModuleAddMenu<UParticleModuleLocation>("Location", "Initial Location", bHasTargetEmitter, AddModuleToTargetEmitter);
	DrawParticleModuleAddMenu<UParticleModuleLocationShape>("Location", "Shape Location", bHasTargetEmitter, AddModuleToTargetEmitter);
	DrawParticleModuleAddMenu<UParticleModuleRotationRate>("Rotation", "Initial Rotation Rate", bHasTargetEmitter, AddModuleToTargetEmitter);
	// Cycle 14 (M2): disabled placeholder → enabled menu 교체.
	// UParticleModuleMeshRotationRate 는 Mesh emitter 전용 — Mesh 가 아닌 emitter 에 추가하면 runtime 에 Cast nullptr → no-op (위험 13 방어).
	DrawParticleModuleAddMenu<UParticleModuleMeshRotationRate>("Rotation Rate", "Mesh Rotation Rate", bHasTargetEmitter, AddModuleToTargetEmitter);
	DrawParticleModuleAddMenu<UParticleModuleDrag>("Drag", "Drag", bHasTargetEmitter, AddModuleToTargetEmitter);
	DrawDisabledParticleModuleMenu("Orbit");
	DrawDisabledParticleModuleMenu("Orientation");
	DrawDisabledParticleModuleMenu("Parameter");
	DrawParticleModuleAddMenu<UParticleModuleSize>("Size", "Size By Life", bHasTargetEmitter, AddModuleToTargetEmitter);
	DrawParticleModuleAddMenu<UParticleModuleSpawn>("Spawn", "Spawn", bHasTargetEmitter && !bHasSpawnModule, AddModuleToTargetEmitter);
	DrawParticleModuleAddMenu<USubUVModule>("SubUV", "SubUV", bHasTargetEmitter, AddModuleToTargetEmitter);
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
	if (Cast<UParticleModuleTypeDataBase>(Module))
	{
		ShowCenterToast("Type data module cannot be deleted.");
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
	if (!Emitter || !LODLevel || !Required)
	{
		return;
	}

	const bool bHasMatchingTypeData =
		LODLevel->GetTypeDataModule() &&
		LODLevel->GetTypeDataModule()->GetRenderMode() == RenderMode;
	if (Required->GetRenderMode() == RenderMode && bHasMatchingTypeData)
	{
		return;
	}

	UParticleModuleTypeDataBase* NewTypeData = CreateTypeDataModule(RenderMode);
	if (!NewTypeData)
	{
		ShowCenterToast("Unsupported emitter type.");
		return;
	}

	CaptureUndoSnapshot("Change Emitter Type");
	for (auto It = LODLevel->Modules.begin(); It != LODLevel->Modules.end();)
	{
		if (UParticleModuleTypeDataBase* ExistingTypeData = Cast<UParticleModuleTypeDataBase>(*It))
		{
			It = LODLevel->Modules.erase(It);
			UObjectManager::Get().DestroyObject(ExistingTypeData);
			continue;
		}
		++It;
	}
	LODLevel->Modules.insert(LODLevel->Modules.begin(), NewTypeData);
	if (RenderMode == EParticleEmitterRenderMode::Beam)
	{
		EnsureBeamSupportModules(LODLevel);
	}
	else
	{
		RemoveBeamSupportModules(LODLevel);
	}
	Required->SetRenderMode(RenderMode);
	Emitter->CacheEmitterModuleInfo();
	ParticleSystemAsset->CacheEmitterModuleInfo();
	SelectModule(EmitterIndex, 0);
	ClearEmitterContext();
	bDirty = true;
	RefreshPreviewComponent(true);
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
	const bool bHeaderClicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
	if (bHeaderHovered && ImGui::IsMouseReleased(ImGuiMouseButton_Right))
	{
		SelectEmitter(EmitterIndex);
		OpenEmitterContextMenu(EmitterIndex, NoParticleModuleSelection);
	}

	const ImVec2 HeaderMin = ImGui::GetItemRectMin();
	const ImVec2 HeaderMax = ImGui::GetItemRectMax();
	const FString EmitterName = GetEmitterDisplayName(Emitter, EmitterIndex);
	const float ControlsY = HeaderMin.y + 31.0f;
	const ImVec2 ToggleMin(HeaderMin.x + 10.0f, ControlsY);
	const ImVec2 ToggleMax(ToggleMin.x + 13.0f, ToggleMin.y + 13.0f);
	const bool bToggleClicked =
		bHeaderClicked &&
		ImGui::GetIO().MousePos.x >= ToggleMin.x &&
		ImGui::GetIO().MousePos.x <= ToggleMax.x &&
		ImGui::GetIO().MousePos.y >= ToggleMin.y &&
		ImGui::GetIO().MousePos.y <= ToggleMax.y;
	if (bToggleClicked && LODLevel)
	{
		CaptureUndoSnapshot(LODLevel->IsEnabled() ? "Disable Emitter" : "Enable Emitter");
		LODLevel->bEnabled = !LODLevel->bEnabled;
		if (Emitter)
		{
			Emitter->CacheEmitterModuleInfo();
		}
		if (ParticleSystemAsset)
		{
			ParticleSystemAsset->CacheEmitterModuleInfo();
		}
		bDirty = true;
		RefreshPreviewComponent(true);
	}
	else if (bHeaderClicked)
	{
		SelectEmitter(EmitterIndex);
	}
	const bool bHeaderSelected = SelectedEmitterIndex == EmitterIndex && SelectedModuleIndex == NoParticleModuleSelection;
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

	DrawMiniEmitterRenderToggle(DrawList, ToggleMin, LODLevel ? LODLevel->IsEnabled() : true);

	if (LODLevel)
	{
		const TArray<UParticleModule*>& Modules = LODLevel->GetModules();
		for (int32 ModuleIndex = 0; ModuleIndex < static_cast<int32>(Modules.size()); ++ModuleIndex)
		{
			if (Cast<UParticleModuleTypeDataBase>(Modules[ModuleIndex]))
			{
				DrawEmitterModuleRow(Modules[ModuleIndex], EmitterIndex, ModuleIndex, false, ModuleRowHeight);
				break;
			}
		}
	}

	if (LODLevel && LODLevel->GetRequiredModule())
	{
		DrawEmitterModuleRow(LODLevel->GetRequiredModule(), EmitterIndex, RequiredParticleModuleSelection, true, ModuleRowHeight);
	}

	if (LODLevel)
	{
		const TArray<UParticleModule*>& Modules = LODLevel->GetModules();
		for (int32 ModuleIndex = 0; ModuleIndex < static_cast<int32>(Modules.size()); ++ModuleIndex)
		{
			if (Cast<UParticleModuleTypeDataBase>(Modules[ModuleIndex]))
			{
				continue;
			}
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
	const bool bLeftClicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
	if (bHovered && ImGui::IsMouseReleased(ImGuiMouseButton_Right))
	{
		SelectModule(EmitterIndex, ModuleIndex);
		OpenEmitterContextMenu(EmitterIndex, ModuleIndex);
	}

	const ImVec2 Min = ImGui::GetItemRectMin();
	const ImVec2 Max = ImGui::GetItemRectMax();
	ImDrawList* DrawList = ImGui::GetWindowDrawList();
	const FString ModuleName = GetModuleDisplayName(Module, bRequired);
	const bool bHasModuleToggle = Module && !bRequired && !Cast<UParticleModuleTypeDataBase>(Module);
	bool bHasModuleCurves = false;
	if (Module && !bRequired && Module->GetClass())
	{
		TArray<const FProperty*> Properties;
		Module->GetClass()->GetAllProperties(Properties);
		for (const FProperty* Property : Properties)
		{
			if (Property && IsParticleDistributionProperty(Module, *Property))
			{
				bHasModuleCurves = true;
				break;
			}
		}
	}
	const ImVec2 ToggleMin(Max.x - 38.0f, Min.y + 5.0f);
	const ImVec2 ToggleMax(ToggleMin.x + 13.0f, ToggleMin.y + 13.0f);
	const ImVec2 CurveMin(Max.x - 19.0f, Min.y + 5.0f);
	const ImVec2 CurveMax(CurveMin.x + 13.0f, CurveMin.y + 13.0f);
	const ImVec2 MousePos = ImGui::GetIO().MousePos;
	const bool bToggleClicked =
		bLeftClicked &&
		bHasModuleToggle &&
		MousePos.x >= ToggleMin.x && MousePos.x <= ToggleMax.x &&
		MousePos.y >= ToggleMin.y && MousePos.y <= ToggleMax.y;
	const bool bCurveClicked =
		bLeftClicked &&
		bHasModuleCurves &&
		MousePos.x >= CurveMin.x && MousePos.x <= CurveMax.x &&
		MousePos.y >= CurveMin.y && MousePos.y <= CurveMax.y;

	if (bCurveClicked)
	{
		SelectModule(EmitterIndex, ModuleIndex);
		OpenParticleModuleCurves(EmitterIndex, ModuleIndex);
	}
	else if (bToggleClicked)
	{
		CaptureUndoSnapshot(Module->IsEnabled() ? "Disable Particle Module" : "Enable Particle Module");
		Module->SetEnabled(!Module->IsEnabled());

		UParticleEmitter* OwnerEmitter = nullptr;
		if (ParticleSystemAsset && EmitterIndex >= 0 && EmitterIndex < static_cast<int32>(ParticleSystemAsset->Emitters.size()))
		{
			OwnerEmitter = ParticleSystemAsset->Emitters[EmitterIndex];
		}
		if (UParticleLODLevel* LODLevel = GetEmitterLODLevel(OwnerEmitter))
		{
			LODLevel->CacheModuleLists();
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
		RefreshPreviewComponent(true);
	}
	else if (bLeftClicked)
	{
		SelectModule(EmitterIndex, ModuleIndex);
	}

	const bool bSelected = SelectedEmitterIndex == EmitterIndex && SelectedModuleIndex == ModuleIndex;
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
	if (bHasModuleToggle)
	{
		DrawMiniCheck(DrawList, ToggleMin, Module ? Module->IsEnabled() : true);
		if (bHasModuleCurves)
		{
			DrawMiniCurveIcon(DrawList, CurveMin, true);
		}
	}

	ImGui::PopID();
}

