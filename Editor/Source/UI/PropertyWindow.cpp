#include "PropertyWindow.h"
#include "EditorEngine.h"
#include "Actor/Actor.h"
#include "Camera/Camera.h"
#include "Component/ActorComponent.h"
#include "Component/BillboardComponent.h"
#include "Component/CameraComponent.h"
#include "Component/MovementComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Component/SceneComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Component/SubUVComponent.h"
#include "Component/TextRenderComponent.h"
#include "Component/UUIDTextRenderComponent.h"
#include "Object/Class.h"
#include "Object/ObjectIterator.h"
#include "Object/ObjectFactory.h"
#include "Renderer/MeshData.h"
#include "Renderer/RenderMesh.h"
#include "Renderer/Material.h"
#include "Renderer/MaterialManager.h"
#include "Renderer/Texture.h"
#include "Core/Paths.h"

#include <commdlg.h>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <unordered_set>
#include <vector>

namespace
{
	struct FComponentMenuEntry
	{
		UClass* Class = nullptr;
		FString DisplayName;
	};

	char GAddComponentSearchBuffer[128] = "";
	UActorComponent* GTextBufferComponent = nullptr;
	char GTextBuffer[512] = "";

	FString OpenTextureFileDialog()
	{
		wchar_t FileName[MAX_PATH] = L"";
		const std::filesystem::path InitialDirectory = FPaths::TextureDir();

		OPENFILENAMEW Ofn = {};
		Ofn.lStructSize = sizeof(OPENFILENAMEW);
		Ofn.lpstrFilter =
			L"Image Files (*.png;*.jpg;*.jpeg;*.bmp;*.tga;*.dds)\0*.png;*.jpg;*.jpeg;*.bmp;*.tga;*.dds\0"
			L"All Files (*.*)\0*.*\0";
		Ofn.lpstrFile = FileName;
		Ofn.nMaxFile = MAX_PATH;
		Ofn.lpstrInitialDir = InitialDirectory.c_str();
		Ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

		if (GetOpenFileNameW(&Ofn))
		{
			return FPaths::FromWide(FileName);
		}

		if (CommDlgExtendedError() != 0)
		{
			return "";
		}

		return "";
	}

	FString GetComponentDisplayName(const UClass* ComponentClass)
	{
		if (ComponentClass == nullptr)
		{
			return "";
		}

		FString DisplayName = ComponentClass->GetName();
		if (!DisplayName.empty() && DisplayName.front() == 'U')
		{
			DisplayName.erase(DisplayName.begin());
		}

		return DisplayName;
	}

	FString ToLowerCopy(FString Text)
	{
		std::transform(Text.begin(), Text.end(), Text.begin(),
			[](unsigned char Character)
			{
				return static_cast<char>(std::tolower(Character));
			});
		return Text;
	}

	bool MatchesComponentFilter(const FString& Text, const char* Filter)
	{
		if (Filter == nullptr || Filter[0] == '\0')
		{
			return true;
		}

		return ToLowerCopy(Text).find(ToLowerCopy(Filter)) != FString::npos;
	}

	bool IsExposedComponentClass(const UClass* ComponentClass)
	{
		if (ComponentClass == nullptr || !ComponentClass->IsChildOf(UActorComponent::StaticClass()))
		{
			return false;
		}

		const FString& ClassName = ComponentClass->GetName();
		return ClassName != "UActorComponent"
			&& ClassName != "UArrowComponent"
			&& ClassName != "UPrimitiveComponent"
			&& ClassName != "UMeshComponent"
			&& ClassName != "ULineBatchComponent"
			&& ClassName != "UUUIDTextRenderComponent"
			&& ClassName != "USkyComponent";
	}

	std::vector<FComponentMenuEntry> BuildComponentMenuEntries()
	{
		std::vector<FComponentMenuEntry> Entries;

		for (const auto& RegisteredClass : UClass::GetRegisteredClasses())
		{
			UClass* ComponentClass = RegisteredClass.second;
			if (!IsExposedComponentClass(ComponentClass))
			{
				continue;
			}

			const auto ExistingEntry = std::find_if(Entries.begin(), Entries.end(),
				[ComponentClass](const FComponentMenuEntry& Entry)
				{
					return Entry.Class == ComponentClass;
				});

			if (ExistingEntry == Entries.end())
			{
				Entries.push_back({ ComponentClass, GetComponentDisplayName(ComponentClass) });
			}
		}

		std::sort(Entries.begin(), Entries.end(),
			[](const FComponentMenuEntry& Left, const FComponentMenuEntry& Right)
			{
				return Left.DisplayName < Right.DisplayName;
			});

		return Entries;
	}

	FString MakeUniqueComponentName(AActor* OwnerActor, UClass* ComponentClass)
	{
		const FString BaseName = GetComponentDisplayName(ComponentClass);
		if (OwnerActor == nullptr)
		{
			return BaseName;
		}

		auto HasNameCollision = [OwnerActor](const FString& CandidateName)
		{
			for (UActorComponent* Component : OwnerActor->GetComponents())
			{
				if (Component && !Component->IsPendingKill() && Component->GetName() == CandidateName)
				{
					return true;
				}
			}
			return false;
		};

		if (!HasNameCollision(BaseName))
		{
			return BaseName;
		}

		int32 Suffix = 1;
		while (true)
		{
			const FString CandidateName = BaseName + "_" + std::to_string(Suffix);
			if (!HasNameCollision(CandidateName))
			{
				return CandidateName;
			}
			++Suffix;
		}
	}

	FString GetOwnedComponentLabel(AActor* OwnerActor, UActorComponent* Component)
	{
		if (Component == nullptr)
		{
			return "";
		}

		FString Label = Component->GetName();
		if (OwnerActor && Component == OwnerActor->GetRootComponent())
		{
			Label += " (Root)";
		}
		return Label;
	}

	USceneComponent* ResolveAttachParent(AActor* OwnerActor, UActorComponent* SelectedComponent)
	{
		if (SelectedComponent && SelectedComponent->IsA(USceneComponent::StaticClass()))
		{
			return static_cast<USceneComponent*>(SelectedComponent);
		}

		return OwnerActor ? OwnerActor->GetRootComponent() : nullptr;
	}

	void ApplyComponentDefaults(UActorComponent* Component, AActor* OwnerActor)
	{
		if (Component == nullptr || OwnerActor == nullptr)
		{
			return;
		}

		if (Component->GetClass() == UBillboardComponent::StaticClass())
		{
			UBillboardComponent* BillboardComponent = static_cast<UBillboardComponent*>(Component);
			if (UTexture* DefaultSprite = UTexture::FindOrLoad("Editor/Icons/Pawn_64x.png", OwnerActor))
			{
				BillboardComponent->SetSprite(DefaultSprite);
			}
		}
	}

	UActorComponent* AddComponentToActor(FEditorEngine* Engine, AActor* OwnerActor, UActorComponent* SelectedComponent, UClass* ComponentClass)
	{
		if (OwnerActor == nullptr || ComponentClass == nullptr || OwnerActor->GetComponentByExactClass(ComponentClass) != nullptr)
		{
			return nullptr;
		}

		UObject* NewObject = FObjectFactory::ConstructObject(ComponentClass, OwnerActor, MakeUniqueComponentName(OwnerActor, ComponentClass));
		UActorComponent* NewComponent = static_cast<UActorComponent*>(NewObject);
		if (NewComponent == nullptr)
		{
			return nullptr;
		}

		OwnerActor->AddOwnedComponent(NewComponent);

		if (NewComponent->IsA(USceneComponent::StaticClass()))
		{
			USceneComponent* NewSceneComponent = static_cast<USceneComponent*>(NewComponent);
			if (USceneComponent* AttachParent = ResolveAttachParent(OwnerActor, SelectedComponent))
			{
				if (AttachParent != NewSceneComponent)
				{
					NewSceneComponent->AttachTo(AttachParent);
				}
			}
		}

		ApplyComponentDefaults(NewComponent, OwnerActor);
		NewComponent->OnRegister();

		if (UPrimitiveComponent* PrimitiveComponent = dynamic_cast<UPrimitiveComponent*>(NewComponent))
		{
			PrimitiveComponent->UpdateBounds();
		}

		if (Engine)
		{
			Engine->SetSelectedComponent(NewComponent);
		}

		return NewComponent;
	}

	void RenderOwnedComponentList(FEditorEngine* Engine, AActor* SelectedActor, UActorComponent* SelectedComponent)
	{
		if (Engine == nullptr || SelectedActor == nullptr)
		{
			return;
		}

		ImGui::TextDisabled("Owned Components");
		if (ImGui::BeginChild("OwnedComponentsList", ImVec2(0.0f, 140.0f), true))
		{
			const bool bActorSelected = (SelectedComponent == nullptr);
			if (ImGui::Selectable(SelectedActor->GetName().c_str(), bActorSelected))
			{
				Engine->SetSelectedActor(SelectedActor);
			}

			for (UActorComponent* Component : SelectedActor->GetComponents())
			{
				if (Component == nullptr || Component->IsPendingKill())
				{
					continue;
				}

				ImGui::PushID(Component);
				const bool bComponentSelected = (Component == SelectedComponent);
				const FString Label = GetOwnedComponentLabel(SelectedActor, Component);
				if (ImGui::Selectable(Label.c_str(), bComponentSelected))
				{
					Engine->SetSelectedComponent(Component);
				}
				ImGui::PopID();
			}
		}
		ImGui::EndChild();
	}

	bool HasVisibleBillboardSection(AActor* SelectedActor, UActorComponent* SelectedComponent)
	{
		if (SelectedActor == nullptr)
		{
			return false;
		}

		for (UActorComponent* Component : SelectedActor->GetComponents())
		{
			if (Component == nullptr)
			{
				continue;
			}
			if (SelectedComponent && Component != SelectedComponent)
			{
				continue;
			}

			if (Component->IsA(USubUVComponent::StaticClass()) || Component->IsA(UBillboardComponent::StaticClass()))
			{
				return true;
			}
		}

		return false;
	}
}

void FPropertyWindow::SetTarget(const FVector& Location, const FVector& Rotation,
								const FVector& Scale, const char* ActorName)
{
	EditLocation = Location;
	EditRotation = Rotation;
	EditScale = Scale;
	bModified = false;

	if (ActorName)
		snprintf(ActorNameBuf, sizeof(ActorNameBuf), "%s", ActorName);
	else
		snprintf(ActorNameBuf, sizeof(ActorNameBuf), "None");
}

void FPropertyWindow::DrawTransformSection()
{
	float Loc[3] = { EditLocation.X, EditLocation.Y, EditLocation.Z };
	float Rot[3] = { EditRotation.X, EditRotation.Y, EditRotation.Z };
	float Scl[3] = { EditScale.X,    EditScale.Y,    EditScale.Z };

	const float ResetBtnWidth = 14.0f;
	const float Spacing = ImGui::GetStyle().ItemInnerSpacing.x;
	const float DragUIWidth = 200.f;

	// Location
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.1f, 0.1f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.7f, 0.2f, 0.2f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.9f, 0.3f, 0.3f, 1.0f));
	if (ImGui::Button("##RL", ImVec2(ResetBtnWidth, 0)))
	{
		EditLocation = { 0.0f, 0.0f, 0.0f };
		bModified = true;
	}
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("Reset Location");
	ImGui::PopStyleColor(3);

	ImGui::SameLine(0, Spacing);
	// ImGui::PushItemWidth(-(ResetBtnWidth));
	ImGui::PushItemWidth(DragUIWidth);
	if (ImGui::DragFloat3("Location", Loc, 0.1f, 0.0f, 0.0f, "%.2f"))
	{
		EditLocation = { Loc[0], Loc[1], Loc[2] };
		bModified = true;
	}
	ImGui::PopItemWidth();

	// Rotation
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.4f, 0.1f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.6f, 0.2f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.3f, 0.8f, 0.3f, 1.0f));
	if (ImGui::Button("##RR", ImVec2(ResetBtnWidth, 0)))
	{
		EditRotation = { 0.0f, 0.0f, 0.0f };
		bModified = true;
	}
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("Reset Rotation");
	ImGui::PopStyleColor(3);

	ImGui::SameLine(0, Spacing);
	// ImGui::PushItemWidth(-(ResetBtnWidth));
	ImGui::PushItemWidth(DragUIWidth);
	if (ImGui::DragFloat3("Rotation", Rot, 0.5f, -360.0f, 360.0f, "%.1f"))
	{
		EditRotation = { Rot[0], Rot[1], Rot[2] };
		bModified = true;
	}
	ImGui::PopItemWidth();

	// Scale
	ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.1f, 0.2f, 0.5f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.3f, 0.7f, 1.0f));
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.3f, 0.4f, 0.9f, 1.0f));
	if (ImGui::Button("##RS", ImVec2(ResetBtnWidth, 0)))
	{
		EditScale = { 1.0f, 1.0f, 1.0f };
		bModified = true;
	}
	if (ImGui::IsItemHovered()) ImGui::SetTooltip("Reset Scale");
	ImGui::PopStyleColor(3);

	ImGui::SameLine(0, Spacing);
	// ImGui::PushItemWidth(-(ResetBtnWidth));
	ImGui::PushItemWidth(DragUIWidth);
	if (ImGui::DragFloat3("Scale", Scl, 0.01f, 0.001f, 100.0f, "%.3f"))
	{
		EditScale = { Scl[0], Scl[1], Scl[2] };
		bModified = true;
	}
	ImGui::PopItemWidth();

	if (bModified && OnChanged)
		OnChanged(EditLocation, EditRotation, EditScale);
}

void FPropertyWindow::Render(FEditorEngine* Engine)
{
	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
	bool bOpen = ImGui::Begin("Properties");
	ImGui::PopStyleVar();

	if (!bOpen)
	{
		ImGui::End();
		return;
	}

	bModified = false;

	ImGui::TextDisabled("Selected:");
	ImGui::SameLine();
	ImGui::TextColored(ImVec4(0.9f, 0.9f, 0.4f, 1.0f), "%s", ActorNameBuf);

	ImGui::Separator();

	if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Indent(8.0f);
		DrawTransformSection();
		ImGui::Unindent(8.0f);
	}
	if (Engine)
	{
		AActor* SelectedActor = Engine->GetSelectedActor();
		UActorComponent* SelectedComponent = Engine->GetSelectedComponent();
		if (SelectedActor)
		{
			if (SelectedComponent)
			{
				ImGui::TextDisabled("Selected Component:");
				ImGui::SameLine();
				ImGui::TextColored(ImVec4(0.6f, 0.85f, 1.0f, 1.0f), "%s", SelectedComponent->GetName().c_str());
				ImGui::Separator();
			}

			if (ImGui::CollapsingHeader("Components", ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::Indent(8.0f);
				const bool bCanAddComponent = !Engine->IsPIEActive();
				if (ImGui::Button("Add Component") && bCanAddComponent)
				{
					GAddComponentSearchBuffer[0] = '\0';
					ImGui::OpenPopup("AddComponentPopup");
				}
				if (!bCanAddComponent && ImGui::IsItemHovered())
				{
					ImGui::SetTooltip("Add Component is disabled while PIE is active.");
				}

				if (ImGui::BeginPopup("AddComponentPopup"))
				{
					ImGui::SetNextItemWidth(240.0f);
					ImGui::InputTextWithHint("##AddComponentSearch", "Search components...", GAddComponentSearchBuffer, IM_ARRAYSIZE(GAddComponentSearchBuffer));
					ImGui::Separator();

					const std::vector<FComponentMenuEntry> ComponentEntries = BuildComponentMenuEntries();
					bool bFoundMatch = false;
					for (const FComponentMenuEntry& Entry : ComponentEntries)
					{
						if (!MatchesComponentFilter(Entry.DisplayName, GAddComponentSearchBuffer))
						{
							continue;
						}
						if (SelectedActor->GetComponentByExactClass(Entry.Class) != nullptr)
						{
							continue;
						}

						bFoundMatch = true;
						if (ImGui::Selectable(Entry.DisplayName.c_str()))
						{
							if (UActorComponent* NewComponent = AddComponentToActor(Engine, SelectedActor, SelectedComponent, Entry.Class))
							{
								SelectedComponent = NewComponent;
							}
							ImGui::CloseCurrentPopup();
						}
					}

					if (!bFoundMatch)
					{
						ImGui::TextDisabled("No addable components");
					}

					ImGui::EndPopup();
				}

				ImGui::Spacing();
				RenderOwnedComponentList(Engine, SelectedActor, SelectedComponent);

				ImGui::Unindent(8.0f);
			}

			if (HasVisibleBillboardSection(SelectedActor, SelectedComponent)
				&& ImGui::CollapsingHeader("Billboard", ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::Indent(8.0f);
				for (UActorComponent* Component : SelectedActor->GetComponents())
				{
					if (!Component) continue;
					if (SelectedComponent && Component != SelectedComponent) continue;

					if (Component->IsA(USubUVComponent::StaticClass()))
					{
						USubUVComponent* SubUVComp = static_cast<USubUVComponent*>(Component);
						bool bBillboard = SubUVComp->IsBillboard();
						if (ImGui::Checkbox("SubUV Billboard", &bBillboard))
							SubUVComp->SetBillboard(bBillboard);
					}
					else if (Component->IsA(UBillboardComponent::StaticClass()))
					{
						UBillboardComponent* BillboardComp = static_cast<UBillboardComponent*>(Component);
						UTexture* CurrentSprite = BillboardComp->GetSprite();
						std::string CurrentSpriteName = "None";
						if (CurrentSprite)
						{
							const std::filesystem::path CurrentSpritePath = FPaths::ToPath(CurrentSprite->GetAssetPathFileName());
							CurrentSpriteName = CurrentSpritePath.stem().string();
						}

						ImGui::Text("Sprite Asset:");
						ImGui::SameLine();

						ImGui::PushItemWidth(200.f);
						if (ImGui::BeginCombo("##BillboardSpriteAssign", CurrentSpriteName.c_str()))
						{
							if (ImGui::Selectable("None", CurrentSprite == nullptr))
							{
								BillboardComp->SetSprite(nullptr);
							}

							const TArray<FString> TextureAssetPaths = UTexture::GetAvailableTextureAssetPaths();
							std::unordered_set<std::string> SeenTextureNames;
							for (const FString& TextureAssetPath : TextureAssetPaths)
							{
								const std::filesystem::path TexturePath = FPaths::ToPath(TextureAssetPath);
								const std::string TextureName = TexturePath.stem().string();
								std::string TextureNameKey = TextureName;
								std::transform(TextureNameKey.begin(), TextureNameKey.end(), TextureNameKey.begin(),
									[](unsigned char Character)
									{
										return static_cast<char>(std::tolower(Character));
									});
								if (!SeenTextureNames.insert(TextureNameKey).second)
								{
									continue;
								}

								const bool bSelected = (CurrentSprite && CurrentSprite->GetAssetPathFileName() == TextureAssetPath);

								if (ImGui::Selectable(TextureName.c_str(), bSelected))
								{
									if (UTexture* TextureAsset = UTexture::FindOrLoad(TextureAssetPath, SelectedActor))
									{
										BillboardComp->SetSprite(TextureAsset);
									}
								}

								if (bSelected)
								{
									ImGui::SetItemDefaultFocus();
								}
							}
							ImGui::EndCombo();
						}
						ImGui::PopItemWidth();

						if (ImGui::Button("Browse..."))
						{
							const FString SelectedFilePath = OpenTextureFileDialog();
							if (!SelectedFilePath.empty())
							{
								if (UTexture* SelectedTexture = UTexture::FindOrLoad(SelectedFilePath, SelectedActor))
								{
									BillboardComp->SetSprite(SelectedTexture);
								}
							}
						}

						/*bool bScreenScaled = BillboardComp->IsScreenSizeScaled();
						if (ImGui::Checkbox("Sprite Screen Scaled", &bScreenScaled))
							BillboardComp->SetScreenSizeScaled(bScreenScaled);*/

						FVector RelativeLocation = BillboardComp->GetRelativeLocation();
						float OffsetValues[3] = { RelativeLocation.X, RelativeLocation.Y, RelativeLocation.Z };
						if (ImGui::DragFloat3("Sprite Offset", OffsetValues, 0.01f, -1000.0f, 1000.0f, "%.2f"))
						{
							BillboardComp->SetRelativeLocation(FVector(OffsetValues[0], OffsetValues[1], OffsetValues[2]));
							BillboardComp->UpdateBounds();
						}

						FTransform RelativeTransform = BillboardComp->GetRelativeTransform();
						FVector RelativeRotationEuler = RelativeTransform.Rotator().Euler();
						float RotationValues[3] = { RelativeRotationEuler.X, RelativeRotationEuler.Y, RelativeRotationEuler.Z };
						if (ImGui::DragFloat3("Sprite Rotation", RotationValues, 0.5f, -360.0f, 360.0f, "%.1f"))
						{
							RelativeTransform.SetRotation(FRotator::MakeFromEuler(FVector(RotationValues[0], RotationValues[1], RotationValues[2])));
							BillboardComp->SetRelativeTransform(RelativeTransform);
							BillboardComp->UpdateBounds();
						}

						FVector RelativeScale = RelativeTransform.GetScale3D();
						float ScaleValues[3] = { RelativeScale.X, RelativeScale.Y, RelativeScale.Z };
						if (ImGui::DragFloat3("Sprite Scale", ScaleValues, 0.01f, 0.001f, 100.0f, "%.3f"))
						{
							RelativeTransform.SetScale3D(FVector(ScaleValues[0], ScaleValues[1], ScaleValues[2]));
							BillboardComp->SetRelativeTransform(RelativeTransform);
							BillboardComp->UpdateBounds();
						}

						FVector2 SpriteSize = BillboardComp->GetSize();
						float SizeValues[2] = { SpriteSize.X, SpriteSize.Y };
						if (ImGui::DragFloat2("Sprite Size", SizeValues, 0.01f, 0.01f, 100.0f, "%.2f"))
							BillboardComp->SetSize(FVector2(SizeValues[0], SizeValues[1]));

						float ScreenSize = BillboardComp->GetScreenSize();
						if (ImGui::DragFloat("Sprite Screen Size", &ScreenSize, 0.0001f, 0.0001f, 1.0f, "%.4f"))
							BillboardComp->SetScreenSize(ScreenSize);
					}
				}
				ImGui::Unindent(8.0f);
			}

			if (UClass* MovementComponentClass = UClass::FindClass("UMovementComponent"))
			{
				if (UActorComponent* MovementComponentBase = SelectedActor->GetComponentByExactClass(MovementComponentClass))
				{
					UMovementComponent* MovementComp = static_cast<UMovementComponent*>(MovementComponentBase);
					const bool bShowMovementSection = !SelectedComponent || SelectedComponent == MovementComp;
					if (bShowMovementSection && ImGui::CollapsingHeader("Movement", ImGuiTreeNodeFlags_DefaultOpen))
					{
						ImGui::Indent(8.0f);

						bool bEnabled = MovementComp->IsEnabled();
						if (ImGui::Checkbox("Enabled", &bEnabled))
						{
							MovementComp->SetEnabled(bEnabled);
						}

						float Amplitude = MovementComp->GetAmplitude();
						if (ImGui::DragFloat("Amplitude", &Amplitude, 0.01f, 0.0f, 1000.0f, "%.2f"))
						{
							MovementComp->SetAmplitude(Amplitude);
						}

						float Speed = MovementComp->GetSpeed();
						if (ImGui::DragFloat("Speed", &Speed, 0.01f, 0.0f, 100.0f, "%.2f"))
						{
							MovementComp->SetSpeed(Speed);
						}

						ImGui::Unindent(8.0f);
					}
				}
			}

			if (UCameraComponent* CameraComp = SelectedActor->GetExactComponentByClass<UCameraComponent>())
			{
				const bool bShowCameraSection = !SelectedComponent || SelectedComponent == CameraComp;
				if (bShowCameraSection && ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen))
				{
					ImGui::Indent(8.0f);

					if (FCamera* Camera = CameraComp->GetCamera())
					{
						int ProjectionMode = Camera->IsOrthographic() ? 1 : 0;
						const char* ProjectionLabels[] = { "Perspective", "Orthographic" };
						if (ImGui::Combo("Projection", &ProjectionMode, ProjectionLabels, IM_ARRAYSIZE(ProjectionLabels)))
						{
							Camera->SetProjectionMode(ProjectionMode == 0 ? ECameraProjectionMode::Perspective : ECameraProjectionMode::Orthographic);
						}

						float NearPlane = Camera->GetNearPlane();
						if (ImGui::DragFloat("Near Plane", &NearPlane, 0.01f, 0.001f, 1000.0f, "%.3f"))
						{
							Camera->SetNearPlane((std::max)(NearPlane, 0.001f));
						}

						float FarPlane = Camera->GetFarPlane();
						if (ImGui::DragFloat("Far Plane", &FarPlane, 1.0f, 1.0f, 100000.0f, "%.1f"))
						{
							Camera->SetFarPlane((std::max)(FarPlane, NearPlane + 0.001f));
						}

						float Speed = Camera->GetSpeed();
						if (ImGui::DragFloat("Move Speed", &Speed, 0.1f, 0.01f, 1000.0f, "%.2f"))
						{
							CameraComp->SetSpeed((std::max)(Speed, 0.01f));
						}

						float Sensitivity = Camera->GetMouseSensitivity();
						if (ImGui::DragFloat("Sensitivity", &Sensitivity, 0.01f, 0.01f, 10.0f, "%.2f"))
						{
							CameraComp->SetSensitivity((std::max)(Sensitivity, 0.01f));
						}

						if (ProjectionMode == 0)
						{
							float FieldOfView = Camera->GetFOV();
							if (ImGui::SliderFloat("Field Of View", &FieldOfView, 1.0f, 179.0f, "%.1f"))
							{
								CameraComp->SetFov(FieldOfView);
							}
						}
						else
						{
							float OrthoWidth = Camera->GetOrthoWidth();
							if (ImGui::DragFloat("Ortho Width", &OrthoWidth, 0.1f, 0.01f, 100000.0f, "%.2f"))
							{
								Camera->SetOrthoWidth((std::max)(OrthoWidth, 0.01f));
							}
						}
					}

					ImGui::Unindent(8.0f);
				}
			}

			if (UTextRenderComponent* TextComp = SelectedActor->GetExactComponentByClass<UTextRenderComponent>())
			{
				const bool bShowTextSection = !SelectedComponent || SelectedComponent == TextComp;
				if (bShowTextSection && ImGui::CollapsingHeader("Text", ImGuiTreeNodeFlags_DefaultOpen))
				{
					ImGui::Indent(8.0f);

					if (GTextBufferComponent != TextComp)
					{
						GTextBufferComponent = TextComp;
						std::snprintf(GTextBuffer, sizeof(GTextBuffer), "%s", TextComp->GetText().c_str());
					}

					if (ImGui::InputText("Text Value", GTextBuffer, IM_ARRAYSIZE(GTextBuffer)))
					{
						TextComp->SetText(GTextBuffer);
						TextComp->UpdateBounds();
					}

					FVector4 TextColor = TextComp->GetTextColor();
					float TextColorValues[4] = { TextColor.X, TextColor.Y, TextColor.Z, TextColor.W };
					if (ImGui::ColorEdit4("Text Color", TextColorValues))
					{
						TextComp->SetTextColor(FVector4(TextColorValues[0], TextColorValues[1], TextColorValues[2], TextColorValues[3]));
						TextComp->MarkTextMeshDirty();
						TextComp->UpdateBounds();
					}

					float WorldSize = TextComp->GetWorldSize();
					if (ImGui::DragFloat("World Size", &WorldSize, 0.01f, 0.01f, 100.0f, "%.2f"))
					{
						TextComp->SetWorldSize((std::max)(WorldSize, 0.01f));
						TextComp->MarkTextMeshDirty();
						TextComp->UpdateBounds();
					}

					bool bAlwaysFaceCamera = TextComp->IsAlwaysFaceCamera();
					if (ImGui::Checkbox("Always Face Camera", &bAlwaysFaceCamera))
					{
						TextComp->SetAlwaysFaceCamera(bAlwaysFaceCamera);
					}

					ImGui::Unindent(8.0f);
				}
			}

			if (USubUVComponent* SubUVComp = SelectedActor->GetExactComponentByClass<USubUVComponent>())
			{
				const bool bShowSubUVSection = !SelectedComponent || SelectedComponent == SubUVComp;
				if (bShowSubUVSection && ImGui::CollapsingHeader("SubUV", ImGuiTreeNodeFlags_DefaultOpen))
				{
					ImGui::Indent(8.0f);

					FVector2 Size = SubUVComp->GetSize();
					float SizeValues[2] = { Size.X, Size.Y };
					if (ImGui::DragFloat2("Size", SizeValues, 0.01f, 0.01f, 100.0f, "%.2f"))
					{
						SubUVComp->SetSize(FVector2((std::max)(SizeValues[0], 0.01f), (std::max)(SizeValues[1], 0.01f)));
						SubUVComp->UpdateBounds();
					}

					int Columns = SubUVComp->GetColumns();
					if (ImGui::DragInt("Columns", &Columns, 1.0f, 1, 128))
					{
						SubUVComp->SetColumns((std::max)(Columns, 1));
					}

					int Rows = SubUVComp->GetRows();
					if (ImGui::DragInt("Rows", &Rows, 1.0f, 1, 128))
					{
						SubUVComp->SetRows((std::max)(Rows, 1));
					}

					int TotalFrames = SubUVComp->GetTotalFrames();
					if (ImGui::DragInt("Total Frames", &TotalFrames, 1.0f, 1, 1024))
					{
						TotalFrames = (std::max)(TotalFrames, 1);
						SubUVComp->SetTotalFrames(TotalFrames);
						SubUVComp->SetFirstFrame((std::min)(SubUVComp->GetFirstFrame(), TotalFrames - 1));
						SubUVComp->SetLastFrame((std::min)(SubUVComp->GetLastFrame(), TotalFrames - 1));
					}

					int FirstFrame = SubUVComp->GetFirstFrame();
					if (ImGui::DragInt("First Frame", &FirstFrame, 1.0f, 0, SubUVComp->GetTotalFrames() - 1))
					{
						FirstFrame = (std::clamp)(FirstFrame, 0, SubUVComp->GetTotalFrames() - 1);
						SubUVComp->SetFirstFrame(FirstFrame);
						if (SubUVComp->GetLastFrame() < FirstFrame)
						{
							SubUVComp->SetLastFrame(FirstFrame);
						}
					}

					int LastFrame = SubUVComp->GetLastFrame();
					if (ImGui::DragInt("Last Frame", &LastFrame, 1.0f, 0, SubUVComp->GetTotalFrames() - 1))
					{
						LastFrame = (std::clamp)(LastFrame, SubUVComp->GetFirstFrame(), SubUVComp->GetTotalFrames() - 1);
						SubUVComp->SetLastFrame(LastFrame);
					}

					float FramesPerSecond = SubUVComp->GetFPS();
					if (ImGui::DragFloat("FPS", &FramesPerSecond, 0.1f, 0.1f, 240.0f, "%.2f"))
					{
						SubUVComp->SetFPS((std::max)(FramesPerSecond, 0.1f));
					}

					bool bLoop = SubUVComp->IsLoop();
					if (ImGui::Checkbox("Loop", &bLoop))
					{
						SubUVComp->SetLoop(bLoop);
					}

					ImGui::Unindent(8.0f);
				}
			}

			if (UStaticMeshComponent* MeshComp = SelectedActor->GetExactComponentByClass<UStaticMeshComponent>())
			{
				const bool bShowStaticMeshSection = !SelectedComponent || SelectedComponent == MeshComp;
				if (bShowStaticMeshSection && ImGui::CollapsingHeader("Static Mesh", ImGuiTreeNodeFlags_DefaultOpen))
				{
					ImGui::Indent(8.0f);

					// 1. 현재 컴포넌트에 할당된 메쉬 정보 가져오기
					UStaticMesh* CurrentMesh = MeshComp->GetStaticMesh();
					std::string CurrentMeshName = CurrentMesh ? CurrentMesh->GetAssetPathFileName() : "None";

					ImGui::Text("Mesh Asset:");
					ImGui::SameLine();

					ImGui::PushItemWidth(200.f);
					if (ImGui::BeginCombo("##StaticMeshAssign", CurrentMeshName.c_str()))
					{
						// 2. TObjectIterator를 사용하여 로드된 모든 UStaticMesh를 순회
						for (TObjectIterator<UStaticMesh> It; It; ++It)
						{
							UStaticMesh* MeshAsset = It.Get();
							if (!MeshAsset) continue;

							std::string MeshName = MeshAsset->GetAssetPathFileName();
							bool bSelected = (CurrentMesh == MeshAsset);

							if (ImGui::Selectable(MeshName.c_str(), bSelected))
							{
								// 3. 선택 시 새로운 메쉬 할당
								MeshComp->SetStaticMesh(MeshAsset);
							}

							if (bSelected)
							{
								ImGui::SetItemDefaultFocus();
							}
						}
						ImGui::EndCombo();
					}
					ImGui::PopItemWidth();

					ImGui::Unindent(8.0f);
				}

				if (bShowStaticMeshSection && ImGui::CollapsingHeader("Materials", ImGuiTreeNodeFlags_DefaultOpen))
				{
					ImGui::Indent(8.0f);

					if (UStaticMesh* MeshData = MeshComp->GetStaticMesh())
					{
						// 매니저에서 모든 머티리얼 리스트 가져오기
						TArray<FString> MatNames = FMaterialManager::Get().GetAllMaterialNames();
						uint32 NumSections = MeshData->GetNumSections();

						// ========================================================
						// [기능 1] 전체 섹션 머티리얼 일괄 변경
						// ========================================================
						ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.8f, 1.0f, 1.0f));
						ImGui::Text("Apply to All Sections:");
						ImGui::PopStyleColor();
						ImGui::SameLine();

						ImGui::PushItemWidth(180.f);
						if (ImGui::BeginCombo("##SetAllMaterials", "Select Material..."))
						{
							for (const FString& MatName : MatNames)
							{
								ImGui::PushID(MatName.c_str());

								auto ListMaterial = FMaterialManager::Get().FindByName(MatName);
								ImTextureID TexID = (ImTextureID)0; // 빨간줄 방지용 0 캐스팅

								if (ListMaterial && ListMaterial->GetMaterialTexture() && ListMaterial->GetMaterialTexture()->TextureSRV)
								{
									TexID = (ImTextureID)ListMaterial->GetMaterialTexture()->TextureSRV;
								}

								// 텍스처가 있으면 리스트에 썸네일 렌더링
								if (TexID)
								{
									ImGui::Image(TexID, ImVec2(24.0f, 24.0f));
									ImGui::SameLine();
									ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4.0f); // 텍스트와 높이 맞춤
								}

								if (ImGui::Selectable(MatName.c_str(), false))
								{
									if (ListMaterial)
									{
										for (uint32 j = 0; j < NumSections; ++j)
										{
											MeshComp->SetMaterial(j, ListMaterial);
										}
									}
								}
								ImGui::PopID();
							}
							ImGui::EndCombo();
						}
						ImGui::PopItemWidth();

						float MasterScroll[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

						if (NumSections > 0)
						{
							if (std::shared_ptr<FMaterial> FirstMat = MeshComp->GetMaterial(0))
							{
								FirstMat->GetParameterData("UVScrollSpeed", MasterScroll, sizeof(MasterScroll));
							}
						}

						ImGui::PushItemWidth(180.f);
						// DragFloat2를 사용하므로 MasterScroll[0], MasterScroll[1] 값만 조작됩니다. (나머지 2개는 패딩 역할)
						if (ImGui::DragFloat2("Scroll All Sections", MasterScroll, 0.001f, -5.0f, 5.0f, "%.2f"))
						{
							for (uint32 j = 0; j < NumSections; ++j)
							{
								if (std::shared_ptr<FMaterial> Mat = MeshComp->GetMaterial(j))
								{
									Mat->SetParameterData("UVScrollSpeed", MasterScroll, sizeof(MasterScroll));
								}
							}
						}
						ImGui::PopItemWidth();

						ImGui::Separator();
						ImGui::Spacing();
						// ========================================================

						// 섹션 개수만큼 머티리얼 슬롯(콤보박스) 생성
						for (uint32 i = 0; i < NumSections; ++i)
						{
							std::shared_ptr<FMaterial> CurrentMat = MeshComp->GetMaterial(i);
							std::string CurrentMatName = CurrentMat ? CurrentMat->GetOriginName() : "None";

							ImGui::PushID(i); // ID 충돌 방지
							std::string Label = "Section " + std::to_string(i);

							ImGui::PushItemWidth(180.f); // 콤보박스 너비 조절

							// ========================================================
							// [기능 2] 개별 섹션 콤보박스 오픈 시 미리보기 출력
							// ========================================================
							if (ImGui::BeginCombo(Label.c_str(), CurrentMatName.c_str()))
							{
								for (const FString& MatName : MatNames)
								{
									ImGui::PushID(MatName.c_str());
									bool bSelected = (CurrentMatName == MatName);

									auto ListMaterial = FMaterialManager::Get().FindByName(MatName);
									ImTextureID TexID = (ImTextureID)0; // 빨간줄 방지용 0 캐스팅

									if (ListMaterial && ListMaterial->GetMaterialTexture() && ListMaterial->GetMaterialTexture()->TextureSRV)
									{
										TexID = (ImTextureID)ListMaterial->GetMaterialTexture()->TextureSRV;
									}

									// 텍스처가 있으면 리스트에 썸네일 렌더링
									if (TexID)
									{
										ImGui::Image(TexID, ImVec2(24.0f, 24.0f));
										ImGui::SameLine();
										ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4.0f); // 텍스트와 높이 맞춤
									}

									if (ImGui::Selectable(MatName.c_str(), bSelected))
									{
										if (ListMaterial)
										{
											MeshComp->SetMaterial(i, ListMaterial);
										}
									}
									if (bSelected)
									{
										ImGui::SetItemDefaultFocus();
									}
									ImGui::PopID();
								}
								ImGui::EndCombo();
							}

							if (CurrentMat)
							{
								FVector4 MatColor = CurrentMat->GetVectorParameter("BaseColor");
								float ColorArray[4] = { MatColor.X, MatColor.Y, MatColor.Z, MatColor.W };

								ImGui::PushID(i + 1000);
								if (ImGui::ColorEdit4("Base Color", ColorArray))
								{
									CurrentMat->SetParameterData("BaseColor", ColorArray, sizeof(ColorArray));
								}
								ImGui::PopID();

								if (auto MatTex = CurrentMat->GetMaterialTexture())
								{
									float SpeedArray[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
									CurrentMat->GetParameterData("UVScrollSpeed", SpeedArray, sizeof(SpeedArray));

									ImGui::PushID(i + 2000);
									// 마찬가지로 UI 조작은 X, Y 2개만 합니다.
									if (ImGui::DragFloat2("UV Scroll", SpeedArray, 0.001f, -5.0f, 5.0f, "%.2f"))
									{
										CurrentMat->SetParameterData("UVScrollSpeed", SpeedArray, sizeof(SpeedArray));
									}
									ImGui::PopID();

								}
							}
							ImGui::PopID(); // PushID(i)에 대한 Pop
							ImGui::Spacing();
						}
					}
					else
					{
						ImGui::TextDisabled("No Static Mesh Assigned");
					}
					ImGui::Unindent(8.0f);
				}
			}
		}
	}
	ImGui::End();
}
