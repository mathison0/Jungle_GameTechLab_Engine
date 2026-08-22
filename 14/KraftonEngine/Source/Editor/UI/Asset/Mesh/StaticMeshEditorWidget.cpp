#include "StaticMeshEditorWidget.h"

#include "Component/Light/DirectionalLightComponent.h"
#include "Component/Primitive/StaticMeshComponent.h"
#include "GameFramework/AActor.h"
#include "GameFramework/Light/DirectionalLightActor.h"
#include "GameFramework/Actor/StaticMeshActor.h"
#include "Mesh/MeshManager.h"
#include "Mesh/Static/StaticMesh.h"
#include "Mesh/Static/StaticMeshAsset.h"
#include "Runtime/Engine.h"
#include "Settings/EditorSettings.h"
#include "Slate/SlateApplication.h"
#include "UI/Toolbar/ViewportToolbar.h"
#include "Viewport/Viewport.h"

#include <imgui.h>

namespace
{
	FString FormatStaticMeshStatCount(size_t Value)
	{
		FString Result = std::to_string(Value);
		for (int32 InsertPos = static_cast<int32>(Result.length()) - 3; InsertPos > 0; InsertPos -= 3)
		{
			Result.insert(static_cast<size_t>(InsertPos), ",");
		}
		return Result;
	}
}

static uint32 GNextStaticMeshEditorInstanceId = 0;

FStaticMeshEditorWidget::FStaticMeshEditorWidget()
	: InstanceId(GNextStaticMeshEditorInstanceId++)
{
	const FString Id = std::to_string(InstanceId);
	PreviewWorldHandle = FName("StaticMeshEditorPreview_" + Id);
	WindowIdSuffix = "###StaticMeshEditor_" + Id;
}

bool FStaticMeshEditorWidget::CanEdit(UObject* Object) const
{
	return Object && Object->IsA<UStaticMesh>();
}

bool FStaticMeshEditorWidget::IsEditingObject(UObject* Object) const
{
	if (FAssetEditorWidget::IsEditingObject(Object))
	{
		return true;
	}

	const UStaticMesh* CurrentMesh = Cast<UStaticMesh>(EditedObject);
	const UStaticMesh* RequestedMesh = Cast<UStaticMesh>(Object);
	if (!IsOpen() || !CurrentMesh || !RequestedMesh)
	{
		return false;
	}

	const FString& CurrentPath = CurrentMesh->GetAssetPathFileName();
	return !CurrentPath.empty()
		&& CurrentPath != "None"
		&& CurrentPath == RequestedMesh->GetAssetPathFileName();
}

void FStaticMeshEditorWidget::Open(UObject* Object)
{
	FAssetEditorWidget::Open(Object);

	FWorldContext& WorldContext = GEngine->CreateWorldContext(EWorldType::EditorPreview, PreviewWorldHandle);
	WorldContext.World->SetWorldType(EWorldType::EditorPreview);
	WorldContext.World->InitWorld();

	AActor* Actor = WorldContext.World->SpawnActor<AActor>();
	UStaticMeshComponent* PreviewComp = nullptr;
	if (UStaticMesh* Mesh = Cast<UStaticMesh>(EditedObject))
	{
		PreviewComp = Actor->AddComponent<UStaticMeshComponent>();
		PreviewComp->SetStaticMesh(Mesh);
		Actor->SetRootComponent(PreviewComp);
	}
	Actor->SetActorLocation(FVector(0.0f, 0.0f, 0.0f));

	const FBoundingBox Bounds = PreviewComp
		? PreviewComp->GetWorldBoundingBox()
		: FBoundingBox(FVector(-0.5f, -0.5f, -0.5f), FVector(0.5f, 0.5f, 0.5f));

	const FVector Extent = Bounds.GetExtent();
	const float FloorZ = Bounds.Min.Z - 0.02f;
	const float FloorScale = max(Extent.X, Extent.Y) * 10.0f;
	PendingBlockerCenter = Bounds.GetCenter();
	PendingBlockerExtents = FVector(
		max(Extent.X * 0.1f, 1.0f),
		max(Extent.Y * 0.1f, 1.0f),
		max(Extent.Z * 0.25f, 1.0f));

	ADirectionalLightActor* LightActor = WorldContext.World->SpawnActor<ADirectionalLightActor>();
	LightActor->InitDefaultComponents();
	LightActor->SetActorRotation(FVector(0.0f, 45.0f, -45.0f));
	UDirectionalLightComponent* LightComp = LightActor->GetComponentByClass<UDirectionalLightComponent>();
	LightComp->SetShadowBias(0.0f);
	LightComp->PushToScene();

	AStaticMeshActor* FloorActor = WorldContext.World->SpawnActor<AStaticMeshActor>();
	FloorActor->InitDefaultComponents("Content/Data/BasicShape/Cube.OBJ");
	FloorActor->SetActorLocation(FVector(Bounds.GetCenter().X, Bounds.GetCenter().Y, FloorZ));
	FloorActor->SetActorScale(FVector(FloorScale, FloorScale, 0.02f));

	ImVec2 ViewportSize = ImGui::GetContentRegionAvail();

	ViewportClient.Initialize(GEngine->GetRenderer().GetFD3DDevice().GetDevice(), static_cast<uint32>(ViewportSize.x), static_cast<uint32>(ViewportSize.y));
	ViewportClient.SetPreviewWorld(WorldContext.World);
	ViewportClient.SetPreviewActor(Actor);
	ViewportClient.SetPreviewMeshComponent(Actor->GetComponentByClass<UStaticMeshComponent>());
	if (PreviewComp)
	{
		PreviewComp->CreatePhysicsState();
	}
	ViewportClient.ResetCameraToPreviewBounds();

	WorldContext.World->SetEditorPOVProvider(&ViewportClient);

	FSlateApplication::Get().RegisterViewport(&ViewportClient);
}

void FStaticMeshEditorWidget::Close()
{
	FAssetEditorWidget::Close();

	if (UWorld* PreviewWorld = ViewportClient.GetPreviewWorld())
	{
		FScene& PreviewScene = PreviewWorld->GetScene();
		GEngine->GetRenderer().GetResources().ReleaseShadowResourcesForScene(&PreviewScene);

		if (PreviewWorldHandle.IsValid())
		{
			GEngine->DestroyWorldContext(PreviewWorldHandle);
		}
	}

	FSlateApplication::Get().UnregisterViewport(&ViewportClient);
	ViewportClient.Release();
}

void FStaticMeshEditorWidget::Tick(float DeltaTime)
{
	if (ViewportClient.IsRenderable())
	{
		ViewportClient.Tick(DeltaTime);
	}
}

void FStaticMeshEditorWidget::CollectPreviewViewports(TArray<IEditorPreviewViewportClient*>& OutClients) const
{
	if (IsOpen())
	{
		OutClients.push_back(const_cast<FStaticMeshEditorViewportClient*>(&ViewportClient));
	}
}

void FStaticMeshEditorWidget::Render(float DeltaTime)
{
	(void)DeltaTime;

	if (!IsOpen() || !EditedObject)
	{
		return;
	}

	static float DetailsWidth = 300.0f;
	UStaticMesh* StaticMesh = Cast<UStaticMesh>(EditedObject);

	bool bWindowOpen = true;
	FString VisibleTitle = "Static Mesh Editor";
	const FString AssetPath = StaticMesh ? StaticMesh->GetAssetPathFileName() : FString();
	if (!AssetPath.empty())
	{
		VisibleTitle += " - ";
		VisibleTitle += AssetPath;
	}
	if (IsDirty())
	{
		VisibleTitle += " *";
	}

	ImGuiWindowFlags WindowFlags = ImGuiWindowFlags_None;
	if (ViewportClient.IsMouseOverViewport())
	{
		WindowFlags |= ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse;
	}

	FString WindowTitle = VisibleTitle + WindowIdSuffix;
	ImGui::SetNextWindowSize(ImVec2(1280.0f, 720.0f), ImGuiCond_Once);
	if (ConsumeFocusRequest())
	{
		ImGui::SetNextWindowFocus();
	}

	if (!ImGui::Begin(WindowTitle.c_str(), &bWindowOpen, WindowFlags))
	{
		ImGui::End();
		if (!bWindowOpen)
		{
			Close();
		}
		return;
	}

	if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
	{
		FSlateApplication::Get().BringViewportToFront(&ViewportClient);
	}

	ImGui::BeginGroup();
	{
		float AvailableWidth = ImGui::GetContentRegionAvail().x - DetailsWidth - ImGui::GetStyle().ItemSpacing.x;
		ImVec2 Size = ImVec2(AvailableWidth, ImGui::GetContentRegionAvail().y);

		ImVec2 ViewportPos = ImGui::GetCursorScreenPos();
		ViewportClient.SetViewportRect(ViewportPos.x, ViewportPos.y, Size.x, Size.y);

		FViewport* VP = ViewportClient.GetViewport();
		if (VP && Size.x > 0 && Size.y > 0)
		{
			VP->RequestResize(static_cast<uint32>(Size.x), static_cast<uint32>(Size.y));

			if (VP->GetSRV())
			{
				ImGui::Image((ImTextureID)VP->GetSRV(), Size);
				// ImGui 인지 hover 를 입력 소유권 중재에 보고.
				FSlateApplication::Get().SetViewportImGuiHovered(&ViewportClient, ImGui::IsItemHovered());
			}

			constexpr float ToolbarHeight = 28.0f;

			ImDrawList* DrawList = ImGui::GetWindowDrawList();
			DrawList->AddRectFilled(ViewportPos,
				ImVec2(ViewportPos.x + Size.x, ViewportPos.y + ToolbarHeight),
				IM_COL32(40, 40, 40, 255));

			FViewportToolbarContext Context;
			Context.Renderer = &GEngine->GetRenderer();
			Context.Settings = &FEditorSettings::Get().MeshEditorViewportSettings;
			Context.RenderOptions = &ViewportClient.GetRenderOptions();
			Context.ToolbarLeft = ViewportPos.x;
			Context.ToolbarTop = ViewportPos.y;
			Context.ToolbarWidth = Size.x;
			Context.bReservePlayStopSpace = false;
			Context.bShowAddActor = false;
			Context.bShowGizmoControls = false;

			FViewportToolbar::Render(Context);
			RenderMeshStatsOverlay(DrawList, ViewportPos);
		}
	}
	ImGui::EndGroup();

	ImGui::SameLine();

	ImGui::BeginChild("Details", ImVec2(DetailsWidth, 0), true);
	ImGui::Text("Static Mesh Details");
	ImGui::Separator();
	RenderDetailsPanel(StaticMesh);
	ImGui::EndChild();

	ImGui::End();

	if (!bWindowOpen)
	{
		Close();
	}
}

void FStaticMeshEditorWidget::RenderMeshStatsOverlay(ImDrawList* DrawList, const ImVec2& ViewportPos) const
{
	if (!DrawList || !EditedObject)
	{
		return;
	}

	size_t VertexCount = 0;
	size_t TriangleCount = 0;

	if (const UStaticMesh* StaticMesh = Cast<UStaticMesh>(EditedObject))
	{
		if (const FStaticMesh* Asset = StaticMesh->GetStaticMeshAsset())
		{
			VertexCount = Asset->Vertices.size();
			TriangleCount = Asset->Indices.size() / 3;
		}
	}

	const FString Text =
		"Triangles: " + FormatStaticMeshStatCount(TriangleCount) + "\n" +
		"Vertices: " + FormatStaticMeshStatCount(VertexCount);

	const ImVec2 TextPos(ViewportPos.x + 8.0f, ViewportPos.y + 36.0f);
	DrawList->AddText(ImVec2(TextPos.x + 1.0f, TextPos.y + 1.0f), IM_COL32(0, 0, 0, 220), Text.c_str());
	DrawList->AddText(TextPos, IM_COL32(235, 238, 242, 255), Text.c_str());
}

void FStaticMeshEditorWidget::RenderDetailsPanel(UStaticMesh* StaticMesh)
{
	FStaticMesh* Asset = StaticMesh ? StaticMesh->GetStaticMeshAsset() : nullptr;
	if (!Asset)
	{
		ImGui::TextDisabled("No static mesh data.");
		return;
	}

	ImGui::Text("Vertices: %s", FormatStaticMeshStatCount(Asset->Vertices.size()).c_str());
	ImGui::Text("Indices: %s", FormatStaticMeshStatCount(Asset->Indices.size()).c_str());
	ImGui::Text("Triangles: %s", FormatStaticMeshStatCount(Asset->Indices.size() / 3).c_str());
	ImGui::Text("Sections: %s", FormatStaticMeshStatCount(Asset->Sections.size()).c_str());

	ImGui::Dummy(ImVec2(0.0f, 8.0f));
	RenderCollisionPanel(StaticMesh);
}

void FStaticMeshEditorWidget::RenderCollisionPanel(UStaticMesh* StaticMesh)
{
	if (!ImGui::CollapsingHeader("Collision", ImGuiTreeNodeFlags_DefaultOpen))
	{
		return;
	}

	const char* Modes[] = { "Full Mesh", "Walkable Only" };
	int ModeIndex = CollisionBuildSettings.Mode == EStaticMeshComplexCollisionMode::WalkableOnly ? 1 : 0;
	if (ImGui::Combo("Mode", &ModeIndex, Modes, 2))
	{
		CollisionBuildSettings.Mode = ModeIndex == 1
			? EStaticMeshComplexCollisionMode::WalkableOnly
			: EStaticMeshComplexCollisionMode::FullMesh;
	}

	UBodySetup* BodySetup = StaticMesh ? StaticMesh->GetBodySetup() : nullptr;
	const size_t CollisionVertexCount = BodySetup ? BodySetup->GetComplexCollisionVertices().size() : 0;
	const size_t CollisionTriangleCount = BodySetup ? BodySetup->GetComplexCollisionIndices().size() / 3 : 0;
	ImGui::Text("Collision Verts: %s", FormatStaticMeshStatCount(CollisionVertexCount).c_str());
	ImGui::Text("Collision Tris: %s", FormatStaticMeshStatCount(CollisionTriangleCount).c_str());

	bool& bPreviewCollision = ViewportClient.GetRenderOptions().ShowFlags.bShowCollisionShape;
	if (ImGui::Checkbox("Preview Collision", &bPreviewCollision) && bPreviewCollision)
	{
		EnsurePreviewCollisionBody();
	}

	const bool bWalkable = CollisionBuildSettings.Mode == EStaticMeshComplexCollisionMode::WalkableOnly;
	if (!bWalkable)
	{
		ImGui::BeginDisabled();
	}

	ImGui::DragFloat("Max Slope", &CollisionBuildSettings.MaxSlopeDegrees, 1.0f, 0.0f, 89.0f, "%.1f");
	ImGui::DragFloat("Min Tri Area", &CollisionBuildSettings.MinTriangleArea, 0.0001f, 0.0f, 100000.0f, "%.5f");
	ImGui::DragFloat("Weld Epsilon", &CollisionBuildSettings.WeldEpsilon, 0.1f, 0.0f, 1000.0f, "%.3f");
	ImGui::DragInt("Min Island Tris", &CollisionBuildSettings.MinIslandTriangleCount, 100.0f, 0, 1000000);

	ImGui::Checkbox("Simplify", &CollisionBuildSettings.bSimplify);
	if (!CollisionBuildSettings.bSimplify)
	{
		ImGui::BeginDisabled();
	}
	ImGui::DragInt("Simplify Above", &CollisionBuildSettings.SimplifyAboveTriangleCount, 1000.0f, 0, 5000000);
	ImGui::SliderFloat("Simplify Ratio", &CollisionBuildSettings.SimplifyTargetRatio, 0.01f, 1.0f, "%.2f");
	if (!CollisionBuildSettings.bSimplify)
	{
		ImGui::EndDisabled();
	}

	if (!bWalkable)
	{
		ImGui::EndDisabled();
	}

	if (ImGui::Button("Rebuild Collision", ImVec2(-1.0f, 0.0f)))
	{
		RebuildCollisionForEditedMesh(StaticMesh);
	}

	ImGui::Separator();

	ImGui::Text("Boundary Blockers");
	ImGui::DragFloat("Blocker Height", &BlockerBuildSettings.Height, 0.1f, 0.01f, 100000.0f, "%.2f");
	ImGui::DragFloat("Blocker Thickness", &BlockerBuildSettings.Thickness, 0.1f, 0.01f, 100000.0f, "%.2f");
	ImGui::DragFloat("Min Edge Length", &BlockerBuildSettings.MinEdgeLength, 0.1f, 0.0f, 100000.0f, "%.2f");
	ImGui::Checkbox("Clear Existing Blockers", &BlockerBuildSettings.bClearExistingBlockers);

	if (ImGui::Button("Generate Boundary Blockers", ImVec2(-1.0f, 0.0f)))
	{
		GenerateBoundaryBlockersForEditedMesh(StaticMesh);
	}

	ImGui::Separator();

	ImGui::Text("Blockers");

	const FKAggregateGeom& AggGeom = BodySetup ? BodySetup->GetAggGeom() : FKAggregateGeom();
	ImGui::Text("Boxes: %s", FormatStaticMeshStatCount(AggGeom.BoxElems.size()).c_str());
	ImGui::DragFloat3("Box Center", &PendingBlockerCenter.X, 1.0f, -1000000.0f, 1000000.0f, "%.2f");
	ImGui::DragFloat3("Box Extents", &PendingBlockerExtents.X, 1.0f, 1.0f, 1000000.0f, "%.2f");

	if (ImGui::Button("Add Blocker Box", ImVec2(-1.0f, 0.0f)))
	{
		AddBlockerBoxForEditedMesh(StaticMesh);
	}

	if (ImGui::Button("Clear Blockers", ImVec2(-1.0f, 0.0f)))
	{
		ClearBlockersForEditedMesh(StaticMesh);
	}

	ImGui::Separator();
	if (ImGui::Button("Save Static Mesh", ImVec2(-1.0f, 0.0f)))
	{
		SaveEditedStaticMesh(StaticMesh);
	}

	if (!LastCollisionBuildMessage.empty())
	{
		ImGui::TextWrapped("%s", LastCollisionBuildMessage.c_str());
	}
}

void FStaticMeshEditorWidget::EnsurePreviewCollisionBody()
{
	if (UStaticMeshComponent* PreviewComp = ViewportClient.GetPreviewMeshComponent())
	{
		if (!PreviewComp->GetBodyInstance())
		{
			PreviewComp->CreatePhysicsState();
		}
	}
}

void FStaticMeshEditorWidget::AddBlockerBoxForEditedMesh(UStaticMesh* StaticMesh)
{
	if (!StaticMesh)
	{
		LastCollisionBuildMessage = "No static mesh.";
		return;
	}

	UBodySetup* BodySetup = StaticMesh->GetOrCreateBodySetup();
	if (!BodySetup)
	{
		LastCollisionBuildMessage = "Failed to create body setup.";
		return;
	}

	PendingBlockerExtents.X = max(PendingBlockerExtents.X, 1.0f);
	PendingBlockerExtents.Y = max(PendingBlockerExtents.Y, 1.0f);
	PendingBlockerExtents.Z = max(PendingBlockerExtents.Z, 1.0f);

	BodySetup->AddBox(PendingBlockerCenter, FQuat::Identity, PendingBlockerExtents);

	if (UStaticMeshComponent* PreviewComp = ViewportClient.GetPreviewMeshComponent())
	{
		PreviewComp->RecreatePhysicsState();
	}

	LastCollisionBuildMessage = "Blocker box added. Boxes: "
		+ FormatStaticMeshStatCount(BodySetup->GetAggGeom().BoxElems.size());
	MarkDirty();
}

void FStaticMeshEditorWidget::ClearBlockersForEditedMesh(UStaticMesh* StaticMesh)
{
	if (!StaticMesh)
	{
		LastCollisionBuildMessage = "No static mesh.";
		return;
	}

	UBodySetup* BodySetup = StaticMesh->GetBodySetup();
	if (!BodySetup)
	{
		LastCollisionBuildMessage = "No body setup.";
		return;
	}

	BodySetup->ClearShapes();

	if (UStaticMeshComponent* PreviewComp = ViewportClient.GetPreviewMeshComponent())
	{
		PreviewComp->RecreatePhysicsState();
	}

	LastCollisionBuildMessage = "Blockers cleared.";
	MarkDirty();
}

void FStaticMeshEditorWidget::RebuildCollisionForEditedMesh(UStaticMesh* StaticMesh)
{
	if (!StaticMesh || !StaticMesh->GetStaticMeshAsset())
	{
		LastCollisionBuildMessage = "No static mesh.";
		return;
	}

	UBodySetup* BodySetup = StaticMesh->GetOrCreateBodySetup();
	if (!BodySetup)
	{
		LastCollisionBuildMessage = "Failed to create body setup.";
		return;
	}

	BodySetup->RebuildComplexCollisionFromStaticMesh(*StaticMesh->GetStaticMeshAsset(), CollisionBuildSettings);

	if (UStaticMeshComponent* PreviewComp = ViewportClient.GetPreviewMeshComponent())
	{
		PreviewComp->RecreatePhysicsState();
	}

	const size_t CollisionTriangleCount = BodySetup->GetComplexCollisionIndices().size() / 3;
	LastCollisionBuildMessage = "Collision rebuilt. Tris: " + FormatStaticMeshStatCount(CollisionTriangleCount);
	MarkDirty();
}

void FStaticMeshEditorWidget::SaveEditedStaticMesh(UStaticMesh* StaticMesh)
{
	if (!StaticMesh)
	{
		LastCollisionBuildMessage = "No static mesh to save.";
		return;
	}

	if (FMeshManager::SaveStaticMeshPackage(StaticMesh))
	{
		LastCollisionBuildMessage = "Static mesh saved.";
		ClearDirty();
	}
	else
	{
		LastCollisionBuildMessage = "Failed to save static mesh.";
	}
}

void FStaticMeshEditorWidget::GenerateBoundaryBlockersForEditedMesh(UStaticMesh* StaticMesh)
{
	if (!StaticMesh)
	{
		LastCollisionBuildMessage = "No static mesh.";
		return;
	}

	UBodySetup* BodySetup = StaticMesh->GetOrCreateBodySetup();
	if (!BodySetup)
	{
		LastCollisionBuildMessage = "Failed to create body setup.";
		return;
	}

	const int32 CreatedBoxCount =
		BodySetup->BuildBoundaryBlockersFromComplexCollision(BlockerBuildSettings);

	if (UStaticMeshComponent* PreviewComp = ViewportClient.GetPreviewMeshComponent())
	{
		PreviewComp->RecreatePhysicsState();
	}

	LastCollisionBuildMessage =
		"Boundary blockers generated. Boxes: " + FormatStaticMeshStatCount(CreatedBoxCount);

	MarkDirty();
}
