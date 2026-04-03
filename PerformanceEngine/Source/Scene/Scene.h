#pragma once

#include <filesystem>

#include "Graphics/D3D11/D3D11Common.h"
#include "Scene/SceneTypes.h"
#include "Types/Array.h"
#include "Picking/AABBNode.h"
#include "Types/Map.h"

class FStaticMesh;
struct AABBNode;

class FScene
{
public:
	bool LoadFromFile(ID3D11Device* InDevice, ID3D11DeviceContext* InDeviceContext, const std::filesystem::path& InSceneFilePath);
	void Release();

	const TArray<FRenderItem>& GetRenderItems() const { return RenderItems; }
	const FSceneCameraInitData& GetInitialCamera() const { return InitialCamera; }

	const FVector& GetSceneBoundsMin() const { return SceneBoundsMin; }
	const FVector& GetSceneBoundsMax() const { return SceneBoundsMax; }
	const TArray<AABBNode>& GetWorldBVHNodes() const { return WorldBVHNodes; }


private:
	//BVH Related Functions
	int CreateWorldBVH(int start, int end, int ParentIdx);

private:
	TArray<FRenderItem> RenderItems;
	TMap<FString, std::shared_ptr<FStaticMesh>> MeshCache;
	TArray<AABBNode> WorldBVHNodes;
	FSceneCameraInitData InitialCamera;

	FVector RawCameraLocation = FVector::ZeroVector;
	FVector RawCameraRotation = FVector::ZeroVector;

	FVector SceneBoundsMin = FVector::ZeroVector;
	FVector SceneBoundsMax = FVector::ZeroVector;
};
