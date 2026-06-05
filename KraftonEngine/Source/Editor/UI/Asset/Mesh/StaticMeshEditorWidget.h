#pragma once

#include "Editor/UI/Asset/AssetEditorWidget.h"
#include "Object/FName.h"
#include "Editor/Viewport/Asset/StaticMeshEditorViewportClient.h"
#include "Physics/BodySetup.h"

struct FStaticMesh;
struct ImDrawList;
struct ImVec2;
class UStaticMesh;

class FStaticMeshEditorWidget : public FAssetEditorWidget
{
public:
	FStaticMeshEditorWidget();

	bool CanEdit(UObject* Object) const override;
	bool IsEditingObject(UObject* Object) const override;

	void Open(UObject* Object) override;
	void Close() override;
	void Tick(float DeltaTime) override;

	void CollectPreviewViewports(TArray<IEditorPreviewViewportClient*>& OutClients) const override;

	bool AllowsMultipleInstances() const override { return true; }

	void Render(float DeltaTime) override;

private:
	void RenderMeshStatsOverlay(ImDrawList* DrawList, const ImVec2& ViewportPos) const;
	void RenderDetailsPanel(UStaticMesh* StaticMesh);
	void RenderCollisionPanel(UStaticMesh* StaticMesh);
	void EnsurePreviewCollisionBody();
	void AddBlockerBoxForEditedMesh(UStaticMesh* StaticMesh);
	void ClearBlockersForEditedMesh(UStaticMesh* StaticMesh);
	void RebuildCollisionForEditedMesh(UStaticMesh* StaticMesh);
	void SaveEditedStaticMesh(UStaticMesh* StaticMesh);

	void GenerateBoundaryBlockersForEditedMesh(UStaticMesh* StaticMesh);

private:
	FStaticMeshEditorViewportClient ViewportClient;

	uint32 InstanceId;
	FName PreviewWorldHandle = FName::None;
	FString WindowIdSuffix;
	FStaticMeshCollisionBuildSettings CollisionBuildSettings;
	FVector PendingBlockerCenter = FVector::ZeroVector;
	FVector PendingBlockerExtents = FVector(100.0f, 100.0f, 100.0f);
	FString LastCollisionBuildMessage;

	FStaticMeshBlockerBuildSettings BlockerBuildSettings;
};
