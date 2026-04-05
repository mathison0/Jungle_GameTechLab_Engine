#pragma once

#include <filesystem>

#include "Graphics/D3D11/D3D11Common.h"
#include "Scene/SceneTypes.h"
#include "StaticMesh/StaticMeshManager.h"
#include "Types/Array.h"
#include "Picking/AABBNode.h"
#include "Types/Map.h"
#include "BVH/BVHTypes.h"

class FStaticMesh;
class FBVHBuilder;
struct AABBNode;

class FScene
{
public:
	FScene();
	~FScene();

	bool LoadFromFile(ID3D11Device* InDevice, ID3D11DeviceContext* InDeviceContext, const std::filesystem::path& InSceneFilePath);
	void Release();

	const TArray<FRenderItem>& GetRenderItems() const { return RenderItems; }
	const TArray<FScenePrimitiveRuntimeData>& GetPrimitiveRuntimeData() const { return PrimitiveRuntimeData; }
	const TArray<FScenePrimitiveColdData>& GetPrimitiveColdData() const { return PrimitiveColdData; }
	size_t GetPrimitiveCount() const { return PrimitiveRuntimeData.size(); }
	const FSceneCameraInitData& GetInitialCamera() const { return InitialCamera; }

	const TArray<FBVHNode>& GetBVHNodeDatas() const { return BVHDatas.Nodes; };

	const FVector& GetSceneBoundsMin() const { return SceneBoundsMin; }
	const FVector& GetSceneBoundsMax() const { return SceneBoundsMax; }
	const TArray<AABBNode>& GetWorldBVHNodes() const { return WorldBVHNodes; }
	int32 GetWorldBVHRootIndex() const { return RootIndex; }

	bool TranslatePrimitiveWorld(int32 PrimitiveIndex, const FVector& Delta);
	bool SetPrimitiveTransformWorld(int32 PrimitiveIndex, const FTransform& NewTransform);


private:
	//BVH Related Functions
	int CreateWorldBVH(int start, int end, int ParentIdx);
	int AllocateBVHNode();
	int FreeNode(int NodeIdx);
	void RefitUpward(int ParentIdx);
	int InsertLeaf(const FVector& Min, const FVector& Max, int PrimitiveIndex);
	int FindBestSibling(const FVector& Min, const FVector& Max);
	void RemoveLeaf(int LeafIdx);
	float SurfaceArea(const FVector& Min, const FVector& Max);
	void MoveLeaf(int RenderItemIndex, const FTransform& NewTransform);
	void UpdatePrimitiveRuntimeData(int32 PrimitiveIndex);
	void UpdateSceneBoundsFromRoot();


private:
	std::unique_ptr<FBVHBuilder> BVHBuilder;
	FBVHSpatialData BVHDatas;

	TArray<FRenderItem> RenderItems;
	TArray<FScenePrimitiveRuntimeData> PrimitiveRuntimeData;
	TArray<FScenePrimitiveColdData> PrimitiveColdData;
	FStaticMeshManager MeshManager;

	TMap<FString, std::shared_ptr<FStaticMesh>> MeshCache;
	FSceneCameraInitData InitialCamera;

	FVector RawCameraLocation = FVector::ZeroVector;
	FVector RawCameraRotation = FVector::ZeroVector;

	FVector SceneBoundsMin = FVector::ZeroVector;
	FVector SceneBoundsMax = FVector::ZeroVector;


	//BVH Related Variables
	TArray<AABBNode> WorldBVHNodes;
	TArray<int> FreeNodes;
	int RootIndex = -1;
	//BVH Related Variables
};
