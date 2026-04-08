#include "PropertyWindow.h"
#include "EditorEngine.h"
#include "Actor/Actor.h"
#include "Component/ActorComponent.h"
#include "Component/BillboardComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Component/SceneComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Component/SubUVComponent.h"
#include "Component/TextRenderComponent.h"
#include "Component/UUIDTextRenderComponent.h"
#include "Object/ObjectIterator.h"
#include "Object/ObjectFactory.h"
#include "Renderer/MeshData.h"
#include "Renderer/RenderMesh.h"
#include "Renderer/Material.h"
#include "Renderer/MaterialManager.h"
#include "Renderer/Texture.h"
#include "Core/Paths.h"

#include <commdlg.h>

namespace
{
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
		Ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

		if (GetOpenFileNameW(&Ofn))
		{
			return FPaths::FromWide(FileName);
		}

		return "";
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

				if (ImGui::Button("Add Billboard Component"))
				{
					int32 BillboardIndex = 0;
					for (UActorComponent* Component : SelectedActor->GetComponents())
					{
						if (Component && Component->IsA(UBillboardComponent::StaticClass()))
						{
							++BillboardIndex;
						}
					}

					const FString ComponentName = "BillboardComponent_" + std::to_string(BillboardIndex);
					UBillboardComponent* BillboardComponent =
						FObjectFactory::ConstructObject<UBillboardComponent>(SelectedActor, ComponentName);

					if (BillboardComponent)
					{
						SelectedActor->AddOwnedComponent(BillboardComponent);

						if (USceneComponent* RootComponent = SelectedActor->GetRootComponent())
						{
							if (RootComponent != BillboardComponent)
							{
								BillboardComponent->AttachTo(RootComponent);
							}
						}
						else
						{
							SelectedActor->SetRootComponent(BillboardComponent);
						}

						if (UTexture* DefaultSprite = UTexture::FindOrLoad("Textures/FileIcon.png", SelectedActor))
						{
							BillboardComponent->SetSprite(DefaultSprite);
						}

						BillboardComponent->OnRegister();
						BillboardComponent->UpdateBounds();
						Engine->SetSelectedComponent(BillboardComponent);
						SelectedComponent = BillboardComponent;
					}
				}

				ImGui::Unindent(8.0f);
			}

			if (ImGui::CollapsingHeader("Billboard", ImGuiTreeNodeFlags_DefaultOpen))
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
							for (const FString& TextureAssetPath : TextureAssetPaths)
							{
								const std::filesystem::path TexturePath = FPaths::ToPath(TextureAssetPath);
								const std::string TextureName = TexturePath.stem().string();
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

						bool bScreenScaled = BillboardComp->IsScreenSizeScaled();
						if (ImGui::Checkbox("Sprite Screen Scaled", &bScreenScaled))
							BillboardComp->SetScreenSizeScaled(bScreenScaled);

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
					else if (Component->IsA(UTextRenderComponent::StaticClass()) && !Component->IsA(UUUIDTextRenderComponent::StaticClass()))
					{
						UTextRenderComponent* TextComp = static_cast<UTextRenderComponent*>(Component);
						bool bAlwaysFaceCamera = TextComp->IsAlwaysFaceCamera();
						if (ImGui::Checkbox("Text Billboard", &bAlwaysFaceCamera))
							TextComp->SetAlwaysFaceCamera(bAlwaysFaceCamera);
					}
				}
				ImGui::Unindent(8.0f);
			}
			if (UStaticMeshComponent* MeshComp = SelectedActor->GetComponentByClass<UStaticMeshComponent>())
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
