// Renders particle system details, module details, and reflection-backed property editors.
#include "Editor/UI/EditorParticleSystemWidgetPrivate.h"

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
	TArray<const FProperty*> Properties;
	if (ParticleSystem->GetClass())
	{
		ParticleSystem->GetClass()->GetAllProperties(Properties);
	}

	auto FindProperty = [&](const char* Name) -> const FProperty*
	{
		for (const FProperty* Property : Properties)
		{
			if (Property && Property->Name && std::strcmp(Property->Name, Name) == 0)
			{
				return Property;
			}
		}
		return nullptr;
	};

	auto DrawPropertyByName = [&](const char* Name) -> bool
	{
		const FProperty* Property = FindProperty(Name);
		if (!Property || !Property->IsEditable())
		{
			return false;
		}

		BeginParticleDetailsRow(GetPropertyDisplayName(*Property));
		ImGui::SetNextItemWidth(-1.0f);
		DrawParticleObjectProperty(ParticleSystem, *Property);
		return true;
	};

	auto DrawPropertyGroup = [&](const char* TableId, std::initializer_list<const char*> Names)
	{
		if (BeginParticleDetailsTable(TableId, LabelWidth))
		{
			for (const char* Name : Names)
			{
				DrawPropertyByName(Name);
			}
			EndParticleDetailsTable();
		}
	};

	if (ImGui::CollapsingHeader("Particle System", ImGuiTreeNodeFlags_DefaultOpen))
	{
		DrawPropertyGroup(
			"##ParticleSystemDetailsTable",
			{ "UpdateTimeFPS" });
	}

	if (ImGui::CollapsingHeader("Thumbnail", ImGuiTreeNodeFlags_DefaultOpen))
	{
		DrawPropertyGroup("##ParticleSystemThumbnailTable", { "ThumbnailWarmup" });
	}

	if (ImGui::CollapsingHeader("LOD", ImGuiTreeNodeFlags_DefaultOpen))
	{
		DrawPropertyGroup("##ParticleSystemLODTable", { "LODDistanceCheckTime", "LODDistances", "LODSettings", "LODMethod" });
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

bool FEditorParticleSystemWidget::DrawParticleObjectProperty(UObject* Object, const FProperty& Property)
{
	if (!Object || !Property.Name)
	{
		return false;
	}

	void* ValuePtr = Property.GetValuePtr(Object);
	const FString Label = FString("##") + Property.Name;
	const bool bChanged = DrawParticlePropertyValue(Property, ValuePtr, Object, Label.c_str());
	if (ImGui::IsItemActivated() && !bPropertyEditUndoCaptured)
	{
		CaptureUndoSnapshot("Edit Particle System");
		bPropertyEditUndoCaptured = true;
	}
	if (bChanged)
	{
		if (!bPropertyEditUndoCaptured)
		{
			CaptureUndoSnapshot("Edit Particle System");
			bPropertyEditUndoCaptured = true;
		}
		Object->PostEditProperty(Property.Name);
		if (ParticleSystemAsset)
		{
			ParticleSystemAsset->CacheEmitterModuleInfo();
		}
		bDirty = true;
		RefreshPreviewComponent(false);
	}
	if (ImGui::IsItemDeactivatedAfterEdit() || !ImGui::IsAnyItemActive())
	{
		bPropertyEditUndoCaptured = false;
	}
	return bChanged;
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
		if (Property.Name && std::strcmp(Property.Name, "LODMethod") == 0)
		{
			const char* Items[] = { "Automatic", "Direct Set" };
			int32 EditedValue = std::clamp(*Value, 0, static_cast<int32>(IM_ARRAYSIZE(Items)) - 1);
			if (ParticleCombo(Label, &EditedValue, Items, IM_ARRAYSIZE(Items)))
			{
				*Value = EditedValue;
				return true;
			}
			return false;
		}

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
	case EPropertyType::Array:
	{
		const int32 ElementCount = Property.ArrayOps ? Property.ArrayOps->Num(ValuePtr) : 0;
		ImGui::Text("%d Array elements", ElementCount);
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

