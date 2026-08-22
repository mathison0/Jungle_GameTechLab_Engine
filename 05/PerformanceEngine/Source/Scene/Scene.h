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
	const TArray<FVisibilityCluster>& GetStaticVisibilityClusters() const;
	const TArray<uint32>& GetStaticClusterPrimitiveIndices() const;
	bool IsPrimitiveDynamic(uint32 PrimitiveIndex) const;
	void BuildDynamicVisibilityClusters(TArray<FVisibilityCluster>& OutDynamicClusters, TArray<uint32>& OutDynamicPrimitiveIndices) const;
	bool TryRefineVisibilityCluster(
		const FVisibilityCluster& InCluster,
		TArray<FVisibilityCluster>& OutChildClusters,
		TArray<uint32>& InOutFramePrimitiveIndices) const;
	int32 GetWorldBVHRootIndex() const { return RootIndex; }
	TArray<FString> GetLoadedMeshAssetKeys() const;
	std::shared_ptr<FStaticMesh> FindLoadedStaticMesh(const FString& InMeshAssetKey) const;

	bool TranslatePrimitiveWorld(int32 PrimitiveIndex, const FVector& Delta);
	bool SetPrimitiveTransformWorld(int32 PrimitiveIndex, const FTransform& NewTransform);
	bool AddLoadedStaticMeshInstance(const FString& InMeshAssetKey, const FTransform& InTransform, int32& OutPrimitiveIndex);
	bool RemovePrimitiveFromScene(int32 PrimitiveIndex);


private:
	struct FVisibilityClusterBuildStats
	{
		uint32 PrimitiveCount = 0;
		float AveragePrimitiveLongestAxis = 0.0f;
	};

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
	void BuildVisibilityClusters() const;
	void EmitVisibilityClustersFromNode(int NodeIndex) const;
	void ComputeVisibilityClusterBuildStats(int NodeIndex, FVisibilityClusterBuildStats& OutStats) const;
	bool ShouldEmitVisibilityCluster(int NodeIndex, const FVisibilityClusterBuildStats& InStats) const;
	bool TryBuildVisibilityClusterFromNode(int NodeIndex, TArray<uint32>& InOutFramePrimitiveIndices, FVisibilityCluster& OutCluster) const;
	void CollectNodePrimitiveIndices(int NodeIndex, TArray<uint32>& OutPrimitiveIndices) const;
	void LogVisibilityClusterBuildSummary() const;
	void UpdatePrimitiveRuntimeData(int32 PrimitiveIndex);
	void UpdateSceneBoundsFromRoot();
	bool AppendStaticMeshPrimitive(int32 PrimitiveId, const FString& InMeshAssetKey, const std::shared_ptr<FStaticMesh>& InStaticMesh, const FTransform& InTransform);
	void RebuildSceneBoundsFromRenderItems();
	void EnsureVisibilityClustersBuilt() const;
	void InvalidateVisibilityClusters();
	bool HasCurrentVisibilityClusterSnapshot() const;
	void MarkPrimitiveDynamic(int32 PrimitiveIndex);
	void AdvanceWorldBvhRevision();


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
	int32 NextPrimitiveId = 0;
	uint64 WorldBVHRevision = 0;
	//BVH Related Variables

	// Static visibility clusters are a derived snapshot layered on top of the live world BVH.
	mutable TArray<FVisibilityCluster> StaticVisibilityClusters;
	mutable TArray<uint32> StaticClusterPrimitiveIndices;
	mutable TArray<uint8> DynamicPrimitiveMask;
	mutable TArray<FVisibilityClusterBuildStats> VisibilityClusterBuildStatsCache;
	mutable TArray<uint8> VisibilityClusterBuildStatsValidMask;
	mutable uint64 VisibilityClusterRevision = 0;
	mutable bool bVisibilityClustersDirty = true;
};
