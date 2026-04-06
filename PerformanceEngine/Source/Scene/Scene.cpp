#include "Scene.h"

#include <algorithm>
#include <cctype>
#include <memory>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include "limits"

#include <Windows.h>

#include "Math/MathUtility.h"
#include "StaticMesh/StaticMesh.h"
#include "Types/PathUtils.h"
#include "Types/Queue.h"
#include "BVH/BVHBuilder.h"


namespace
{
	constexpr uint32 VisibilityClusterPrimitiveTarget = 8;
	constexpr float VisibilityClusterMaxLongestAxisScale = 2.5f;
	constexpr float VisibilityClusterMaxAspectRatio = 3.0f;
	constexpr float VisibilityClusterAxisEpsilon = 1.0e-3f;

	struct FPendingPrimitive
	{
		int32 Id = 0;
		FString Type;
		FString MeshAssetPath;
		FVector Location = FVector::ZeroVector;
		FVector Rotation = FVector::ZeroVector;
		FVector Scale = FVector::OneVector;
	};

	std::string Trim(const std::string& InValue)
	{
		const auto Begin = std::find_if_not(InValue.begin(), InValue.end(), [](unsigned char Ch) { return std::isspace(Ch) != 0; });
		const auto End = std::find_if_not(InValue.rbegin(), InValue.rend(), [](unsigned char Ch) { return std::isspace(Ch) != 0; }).base();
		if (Begin >= End)
		{
			return {};
		}

		return std::string(Begin, End);
	}

	float ComputeLongestAxis(const FVector& InExtent)
	{
		return std::max(std::max(std::fabs(InExtent.X), std::fabs(InExtent.Y)), std::fabs(InExtent.Z));
	}

	float ComputeShortestAxis(const FVector& InExtent)
	{
		return std::min(std::min(std::fabs(InExtent.X), std::fabs(InExtent.Y)), std::fabs(InExtent.Z));
	}

	bool TryParseFloatArray1(const std::string& InLine, float& OutValue)
	{
		const size_t OpenBracket = InLine.find('[');
		const size_t CloseBracket = InLine.find(']');
		if (OpenBracket == std::string::npos || CloseBracket == std::string::npos || CloseBracket <= OpenBracket)
		{
			return false;
		}

		return ::sscanf_s(InLine.c_str() + OpenBracket + 1, "%f", &OutValue) == 1;
	}

	bool TryParseFloatArray3(const std::string& InLine, FVector& OutValue)
	{
		const size_t OpenBracket = InLine.find('[');
		const size_t CloseBracket = InLine.find(']');
		if (OpenBracket == std::string::npos || CloseBracket == std::string::npos || CloseBracket <= OpenBracket)
		{
			return false;
		}

		return ::sscanf_s(
			InLine.c_str() + OpenBracket + 1,
			"%f, %f, %f",
			&OutValue.X,
			&OutValue.Y,
			&OutValue.Z) == 3;
	}

	bool TryParseQuotedStringValue(const std::string& InLine, FString& OutValue)
	{
		const size_t Colon = InLine.find(':');
		if (Colon == std::string::npos)
		{
			return false;
		}

		const size_t FirstQuote = InLine.find('"', Colon);
		if (FirstQuote == std::string::npos)
		{
			return false;
		}

		const size_t SecondQuote = InLine.find('"', FirstQuote + 1);
		if (SecondQuote == std::string::npos || SecondQuote <= FirstQuote)
		{
			return false;
		}

		OutValue = InLine.substr(FirstQuote + 1, SecondQuote - FirstQuote - 1);
		return true;
	}

	bool TryParsePrimitiveId(const std::string& InLine, int32& OutId)
	{
		int ParsedId = 0;
		if (::sscanf_s(InLine.c_str(), "\"%d\" : {", &ParsedId) != 1)
		{
			return false;
		}

		OutId = ParsedId;
		return true;
	}

	bool LooksLikeRadians(const FVector& InRotation)
	{
		const float MaxComponent = std::max({ std::fabs(InRotation.X), std::fabs(InRotation.Y), std::fabs(InRotation.Z) });
		return MaxComponent <= 6.5f;
	}

	FVector ConvertRadiansToDegrees(const FVector& InRadians)
	{
		return FVector(
			FMath::RadiansToDegrees(InRadians.X),
			FMath::RadiansToDegrees(InRadians.Y),
			FMath::RadiansToDegrees(InRadians.Z));
	}

	FQuat MakeSceneRotation(const FVector& InRotation)
	{
		const FVector EulerDegrees = LooksLikeRadians(InRotation) ? ConvertRadiansToDegrees(InRotation) : InRotation;
		return FQuat::MakeFromEuler(EulerDegrees);
	}

	FTransform BuildSceneTransform(const FVector& InLocation, const FVector& InRotation, const FVector& InScale)
	{
		return FTransform(MakeSceneRotation(InRotation), InLocation, InScale);
	}

	void ExpandBounds(FVector& InOutMin, FVector& InOutMax, bool& bInOutHasBounds, const FVector& InPoint)
	{
		if (!bInOutHasBounds)
		{
			InOutMin = InPoint;
			InOutMax = InPoint;
			bInOutHasBounds = true;
			return;
		}

		InOutMin = FVector::Min(InOutMin, InPoint);
		InOutMax = FVector::Max(InOutMax, InPoint);
	}

	void ExpandBoundsWithTransformedAabb(
		const FVector& InLocalMin,
		const FVector& InLocalMax,
		const FTransform& InTransform,
		FVector& InOutMin,
		FVector& InOutMax,
		bool& bInOutHasBounds)
	{
		const FVector Corners[8] =
		{
			FVector(InLocalMin.X, InLocalMin.Y, InLocalMin.Z),
			FVector(InLocalMax.X, InLocalMin.Y, InLocalMin.Z),
			FVector(InLocalMin.X, InLocalMax.Y, InLocalMin.Z),
			FVector(InLocalMax.X, InLocalMax.Y, InLocalMin.Z),
			FVector(InLocalMin.X, InLocalMin.Y, InLocalMax.Z),
			FVector(InLocalMax.X, InLocalMin.Y, InLocalMax.Z),
			FVector(InLocalMin.X, InLocalMax.Y, InLocalMax.Z),
			FVector(InLocalMax.X, InLocalMax.Y, InLocalMax.Z),
		};

		for (const FVector& Corner : Corners)
		{
			ExpandBounds(InOutMin, InOutMax, bInOutHasBounds, InTransform.TransformPosition(Corner));
		}
	}

	FBoundingSphere TransformBoundingSphere(const FBoundingSphere& InLocalSphere, const FTransform& InTransform)
	{
		FBoundingSphere WorldSphere;
		WorldSphere.Center = InTransform.TransformPosition(InLocalSphere.Center);

		const FVector Scale = InTransform.GetScale3D();
		const float MaxScale = std::max({ std::fabs(Scale.X), std::fabs(Scale.Y), std::fabs(Scale.Z) });
		WorldSphere.Radius = InLocalSphere.Radius * MaxScale;
		return WorldSphere;
	}

	std::filesystem::path ResolveAssetPath(const std::filesystem::path& InScenePath, const FString& InRelativeAssetPath)
	{
		const std::filesystem::path AssetPath = PathUtils::PathFromUtf8(InRelativeAssetPath);
		if (AssetPath.is_absolute() && std::filesystem::exists(AssetPath))
		{
			return AssetPath;
		}

		std::filesystem::path Cursor = InScenePath.parent_path();
		for (int Depth = 0; Depth < 6; ++Depth)
		{
			const std::filesystem::path Candidate = (Cursor / AssetPath).lexically_normal();
			if (std::filesystem::exists(Candidate))
			{
				return Candidate;
			}

			if (!Cursor.has_parent_path())
			{
				break;
			}

			Cursor = Cursor.parent_path();
		}

		return (InScenePath.parent_path() / AssetPath).lexically_normal();
	}

}

FScene::FScene()
	: BVHBuilder(std::make_unique<FBVHBuilder>())
{

}

FScene::~FScene() = default;

bool FScene::LoadFromFile(ID3D11Device* InDevice, ID3D11DeviceContext* InDeviceContext, const std::filesystem::path& InSceneFilePath)
{
	Release();

	if (InDevice == nullptr || InDeviceContext == nullptr || InSceneFilePath.empty())
	{
		return false;
	}

	const std::filesystem::path SceneFilePath = std::filesystem::absolute(InSceneFilePath);
	std::string SceneText;
	if (!PathUtils::ReadTextFileUtf8(SceneFilePath, SceneText))
	{
		return false;
	}

	std::istringstream SceneFile(SceneText);

	bool bInCameraBlock = false;
	bool bInPrimitivesBlock = false;
	bool bInPrimitiveBlock = false;

	FPendingPrimitive PendingPrimitive;
	int32 MaxPrimitiveId = -1;
	std::string Line;
	while (std::getline(SceneFile, Line))
	{
		const std::string TrimmedLine = Trim(Line);
		if (TrimmedLine.empty())
		{
			continue;
		}

		if (!bInPrimitiveBlock && TrimmedLine.starts_with("\"PerspectiveCamera\""))
		{
			bInCameraBlock = true;
			continue;
		}

		if (!bInPrimitiveBlock && TrimmedLine.starts_with("\"Primitives\""))
		{
			bInPrimitivesBlock = true;
			continue;
		}

		if (bInCameraBlock)
		{
			if (TrimmedLine == "}," || TrimmedLine == "}")
			{
				bInCameraBlock = false;
				continue;
			}

			float ScalarValue = 0.0f;
			FVector VectorValue = FVector::ZeroVector;
			if (TrimmedLine.starts_with("\"FOV\"") && TryParseFloatArray1(TrimmedLine, ScalarValue))
			{
				InitialCamera.FovDegrees = ScalarValue;
			}
			else if (TrimmedLine.starts_with("\"NearClip\"") && TryParseFloatArray1(TrimmedLine, ScalarValue))
			{
				InitialCamera.NearClip = ScalarValue;
			}
			else if (TrimmedLine.starts_with("\"FarClip\"") && TryParseFloatArray1(TrimmedLine, ScalarValue))
			{
				InitialCamera.FarClip = ScalarValue;
			}
			else if (TrimmedLine.starts_with("\"Location\"") && TryParseFloatArray3(TrimmedLine, VectorValue))
			{
				RawCameraLocation = VectorValue;
			}
			else if (TrimmedLine.starts_with("\"Rotation\"") && TryParseFloatArray3(TrimmedLine, VectorValue))
			{
				RawCameraRotation = VectorValue;
			}

			continue;
		}

		if (bInPrimitiveBlock)
		{
			if (TrimmedLine == "}," || TrimmedLine == "}")
			{
				bInPrimitiveBlock = false;

				if (PendingPrimitive.Type != "StaticMeshComp" || PendingPrimitive.MeshAssetPath.empty())
				{
					continue;
				}

				const std::filesystem::path MeshPath = ResolveAssetPath(SceneFilePath, PendingPrimitive.MeshAssetPath);
				const FString MeshCacheKey = FStaticMeshManager::BuildAssetKey(MeshPath);

				std::shared_ptr<FStaticMesh> SharedMesh = MeshManager.LoadStaticMesh(InDevice, InDeviceContext, MeshPath);
				if (SharedMesh && !SharedMesh->GetSpatialData())
				{
					// BVH 트리 생성
					SharedMesh->SetSpatialData(
						std::make_shared<FBVHSpatialData>(BVHBuilder->BuildBVH(SharedMesh.get())));
				}

				if (!SharedMesh || !SharedMesh->IsValid())
				{
					continue;
				}

				const FTransform WorldTransform = BuildSceneTransform(PendingPrimitive.Location, PendingPrimitive.Rotation, PendingPrimitive.Scale);
				if (AppendStaticMeshPrimitive(PendingPrimitive.Id, MeshCacheKey, SharedMesh, WorldTransform))
				{
					MaxPrimitiveId = std::max(MaxPrimitiveId, PendingPrimitive.Id);
				}
				continue;
			}

			FVector VectorValue = FVector::ZeroVector;
			FString StringValue;
			if (TrimmedLine.starts_with("\"Location\"") && TryParseFloatArray3(TrimmedLine, VectorValue))
			{
				PendingPrimitive.Location = VectorValue;
			}
			else if (TrimmedLine.starts_with("\"Rotation\"") && TryParseFloatArray3(TrimmedLine, VectorValue))
			{
				PendingPrimitive.Rotation = VectorValue;
			}
			else if (TrimmedLine.starts_with("\"Scale\"") && TryParseFloatArray3(TrimmedLine, VectorValue))
			{
				PendingPrimitive.Scale = VectorValue;
			}
			else if (TrimmedLine.starts_with("\"ObjStaticMeshAsset\"") && TryParseQuotedStringValue(TrimmedLine, StringValue))
			{
				PendingPrimitive.MeshAssetPath = StringValue;
			}
			else if (TrimmedLine.starts_with("\"Type\"") && TryParseQuotedStringValue(TrimmedLine, StringValue))
			{
				PendingPrimitive.Type = StringValue;
			}

			continue;
		}

		if (bInPrimitivesBlock)
		{
			if (TrimmedLine == "}" || TrimmedLine == "},")
			{
				bInPrimitivesBlock = false;
				continue;
			}

			int32 PrimitiveId = 0;
			if (TryParsePrimitiveId(TrimmedLine, PrimitiveId))
			{
				PendingPrimitive = {};
				PendingPrimitive.Id = PrimitiveId;
				bInPrimitiveBlock = true;
			}
		}
	}

	if (PrimitiveRuntimeData.empty())
	{
		Release();
		return false;
	}

	NextPrimitiveId = MaxPrimitiveId + 1;
	RebuildSceneBoundsFromRenderItems();

	//Create World BVH here

	//For Check
	//For Check

	WorldBVHNodes.clear();
	FreeNodes.clear();
	WorldBVHNodes.reserve(RenderItems.size() * 2);
	AABBNode RootNode;
	RootNode.Min = SceneBoundsMin;
	RootNode.Max = SceneBoundsMax;
	WorldBVHNodes.push_back(RootNode); // index 0 = root
	RootIndex = 0;
	// Sort RenderItems
	CreateWorldBVH(0, (int)RenderItems.size(), 0);
	AdvanceWorldBvhRevision();
	InvalidateVisibilityClusters();

	InitialCamera.Transform = FTransform(MakeSceneRotation(RawCameraRotation), RawCameraLocation, FVector::OneVector);
	if (RenderItems.empty())
	{
		SceneBoundsMin = FVector::ZeroVector;
		SceneBoundsMax = FVector::ZeroVector;
	}

	return true;
}

void FScene::Release()
{
	RenderItems.clear();
	PrimitiveRuntimeData.clear();
	PrimitiveColdData.clear();
	WorldBVHNodes.clear();
	FreeNodes.clear();
	RootIndex = -1;
	MeshManager.Release();
	InitialCamera = FSceneCameraInitData();
	RawCameraLocation = FVector::ZeroVector;
	RawCameraRotation = FVector::ZeroVector;
	SceneBoundsMin = FVector::ZeroVector;
	SceneBoundsMax = FVector::ZeroVector;
	StaticVisibilityClusters.clear();
	StaticClusterPrimitiveIndices.clear();
	DynamicPrimitiveMask.clear();
	VisibilityClusterBuildStatsCache.clear();
	VisibilityClusterBuildStatsValidMask.clear();
	WorldBVHRevision = 0;
	VisibilityClusterRevision = 0;
	bVisibilityClustersDirty = true;
	NextPrimitiveId = 0;
}

const TArray<FVisibilityCluster>& FScene::GetStaticVisibilityClusters() const
{
	EnsureVisibilityClustersBuilt();
	return StaticVisibilityClusters;
}

const TArray<uint32>& FScene::GetStaticClusterPrimitiveIndices() const
{
	EnsureVisibilityClustersBuilt();
	return StaticClusterPrimitiveIndices;
}

bool FScene::IsPrimitiveDynamic(uint32 PrimitiveIndex) const
{
	return PrimitiveIndex < DynamicPrimitiveMask.size() && DynamicPrimitiveMask[PrimitiveIndex] != 0;
}

bool FScene::TryRefineVisibilityCluster(
	const FVisibilityCluster& InCluster,
	TArray<FVisibilityCluster>& OutChildClusters,
	TArray<uint32>& InOutFramePrimitiveIndices) const
{
	OutChildClusters.clear();
	EnsureVisibilityClustersBuilt();

	if (!HasCurrentVisibilityClusterSnapshot()
		|| InCluster.bDynamic
		|| InCluster.SourceBvhNodeIndex < 0
		|| InCluster.SourceBvhNodeIndex >= static_cast<int>(WorldBVHNodes.size()))
	{
		return false;
	}

	const AABBNode& Node = WorldBVHNodes[InCluster.SourceBvhNodeIndex];
	if (Node.IsLeaf() || Node.LeftChildIndex < 0 || Node.RightChildIndex < 0)
	{
		return false;
	}

	FVisibilityCluster LeftCluster = {};
	FVisibilityCluster RightCluster = {};
	if (!TryBuildVisibilityClusterFromNode(Node.LeftChildIndex, InOutFramePrimitiveIndices, LeftCluster)
		|| !TryBuildVisibilityClusterFromNode(Node.RightChildIndex, InOutFramePrimitiveIndices, RightCluster))
	{
		return false;
	}

	OutChildClusters.push_back(LeftCluster);
	OutChildClusters.push_back(RightCluster);
	return true;
}

void FScene::BuildDynamicVisibilityClusters(TArray<FVisibilityCluster>& OutDynamicClusters, TArray<uint32>& OutDynamicPrimitiveIndices) const
{
	OutDynamicClusters.clear();
	OutDynamicPrimitiveIndices.clear();

	const TArray<FScenePrimitiveRuntimeData>& RuntimeData = GetPrimitiveRuntimeData();
	OutDynamicClusters.reserve(RuntimeData.size());
	OutDynamicPrimitiveIndices.reserve(RuntimeData.size());

	for (uint32 PrimitiveIndex = 0; PrimitiveIndex < static_cast<uint32>(RuntimeData.size()); ++PrimitiveIndex)
	{
		if (!IsPrimitiveDynamic(PrimitiveIndex))
		{
			continue;
		}

		const FScenePrimitiveRuntimeData& Primitive = RuntimeData[PrimitiveIndex];
		if (Primitive.StaticMesh == nullptr || !Primitive.StaticMesh->IsValid())
		{
			continue;
		}

		FVisibilityCluster Cluster = {};
		Cluster.BoundsMin = Primitive.WorldBoundsMin;
		Cluster.BoundsMax = Primitive.WorldBoundsMax;
		Cluster.PrimitiveOffset = static_cast<uint32>(OutDynamicPrimitiveIndices.size());
		Cluster.PrimitiveCount = 1;
		Cluster.SourceBvhNodeIndex = -1;
		Cluster.bDynamic = true;

		OutDynamicPrimitiveIndices.push_back(PrimitiveIndex);
		OutDynamicClusters.push_back(Cluster);
	}
}

bool FScene::TranslatePrimitiveWorld(int32 PrimitiveIndex, const FVector& Delta)
{
	if (PrimitiveIndex < 0 || Delta.IsNearlyZero())
	{
		return false;
	}

	const size_t PrimitiveArrayIndex = static_cast<size_t>(PrimitiveIndex);
	if (PrimitiveArrayIndex >= RenderItems.size() || PrimitiveArrayIndex >= PrimitiveRuntimeData.size())
	{
		return false;
	}

	FRenderItem& Item = RenderItems[PrimitiveArrayIndex];
	if (!Item.StaticMesh || !Item.StaticMesh->IsValid())
	{
		return false;
	}

	FTransform NewTransform = Item.Transform;
	NewTransform.AddToTranslation(Delta);
	MoveLeaf(PrimitiveIndex, NewTransform);
	return true;
}

bool FScene::SetPrimitiveTransformWorld(int32 PrimitiveIndex, const FTransform& NewTransform)
{
	if (PrimitiveIndex < 0)
	{
		return false;
	}

	const size_t PrimitiveArrayIndex = static_cast<size_t>(PrimitiveIndex);
	if (PrimitiveArrayIndex >= RenderItems.size() || PrimitiveArrayIndex >= PrimitiveRuntimeData.size())
	{
		return false;
	}

	FRenderItem& Item = RenderItems[PrimitiveArrayIndex];
	if (!Item.StaticMesh || !Item.StaticMesh->IsValid())
	{
		return false;
	}

	if (Item.Transform.Equals(NewTransform, 1.e-4f))
	{
		return false;
	}

	MoveLeaf(PrimitiveIndex, NewTransform);
	return true;
}

TArray<FString> FScene::GetLoadedMeshAssetKeys() const
{
	return MeshManager.GetLoadedMeshAssetKeys();
}

std::shared_ptr<FStaticMesh> FScene::FindLoadedStaticMesh(const FString& InMeshAssetKey) const
{
	return MeshManager.FindLoadedStaticMesh(InMeshAssetKey);
}

bool FScene::AddLoadedStaticMeshInstance(const FString& InMeshAssetKey, const FTransform& InTransform, int32& OutPrimitiveIndex)
{
	OutPrimitiveIndex = -1;

	const std::shared_ptr<FStaticMesh> StaticMesh = MeshManager.FindLoadedStaticMesh(InMeshAssetKey);
	if (!StaticMesh || !StaticMesh->IsValid())
	{
		return false;
	}

	if (!StaticMesh->GetSpatialData() && BVHBuilder)
	{
		StaticMesh->SetSpatialData(std::make_shared<FBVHSpatialData>(BVHBuilder->BuildBVH(StaticMesh.get())));
	}

	const int32 PrimitiveId = NextPrimitiveId;
	if (!AppendStaticMeshPrimitive(PrimitiveId, InMeshAssetKey, StaticMesh, InTransform))
	{
		return false;
	}

	++NextPrimitiveId;
	OutPrimitiveIndex = static_cast<int32>(RenderItems.size()) - 1;
	FRenderItem& NewItem = RenderItems.back();
	const FVector Margin = (NewItem.WorldBoundsMax - NewItem.WorldBoundsMin) * 0.1f;
	NewItem.LooseBoundsMin = NewItem.WorldBoundsMin - Margin;
	NewItem.LooseBoundsMax = NewItem.WorldBoundsMax + Margin;

	if (RootIndex < 0 || WorldBVHNodes.empty())
	{
		WorldBVHNodes.clear();
		FreeNodes.clear();

		AABBNode RootNode;
		RootNode.Min = NewItem.WorldBoundsMin;
		RootNode.Max = NewItem.WorldBoundsMax;
		RootNode.ParentIndex = -1;
		RootNode.LeftChildIndex = -1;
		RootNode.RightChildIndex = -1;
		RootNode.PrimitiveIndex = OutPrimitiveIndex;

		WorldBVHNodes.push_back(RootNode);
		RootIndex = 0;
		NewItem.BVHLeafIndex = 0;
	}
	else
	{
		NewItem.BVHLeafIndex = InsertLeaf(NewItem.WorldBoundsMin, NewItem.WorldBoundsMax, OutPrimitiveIndex);
	}

	UpdateSceneBoundsFromRoot();
	AdvanceWorldBvhRevision();
	InvalidateVisibilityClusters();

	return OutPrimitiveIndex >= 0;
}

bool FScene::RemovePrimitiveFromScene(int32 PrimitiveIndex)
{
	if (PrimitiveIndex < 0)
	{
		return false;
	}

	const size_t PrimitiveArrayIndex = static_cast<size_t>(PrimitiveIndex);
	if (PrimitiveArrayIndex >= RenderItems.size()
		|| PrimitiveArrayIndex >= PrimitiveRuntimeData.size()
		|| PrimitiveArrayIndex >= PrimitiveColdData.size())
	{
		return false;
	}

	const int32 RemovedLeafIndex = RenderItems[PrimitiveArrayIndex].BVHLeafIndex;
	if (RemovedLeafIndex >= 0 && RemovedLeafIndex < static_cast<int32>(WorldBVHNodes.size()))
	{
		RemoveLeaf(RemovedLeafIndex);
	}

	const size_t LastIndex = RenderItems.size() - 1;

	if (PrimitiveArrayIndex != LastIndex)
	{
		std::swap(RenderItems[PrimitiveArrayIndex], RenderItems[LastIndex]);
		std::swap(PrimitiveRuntimeData[PrimitiveArrayIndex], PrimitiveRuntimeData[LastIndex]);
		std::swap(PrimitiveColdData[PrimitiveArrayIndex], PrimitiveColdData[LastIndex]);
		if (PrimitiveArrayIndex < DynamicPrimitiveMask.size() && LastIndex < DynamicPrimitiveMask.size())
		{
			std::swap(DynamicPrimitiveMask[PrimitiveArrayIndex], DynamicPrimitiveMask[LastIndex]);
		}

		FRenderItem& MovedItem = RenderItems[PrimitiveArrayIndex];
		if (MovedItem.BVHLeafIndex >= 0 && MovedItem.BVHLeafIndex < static_cast<int32>(WorldBVHNodes.size()))
		{
			WorldBVHNodes[MovedItem.BVHLeafIndex].PrimitiveIndex = static_cast<int32>(PrimitiveArrayIndex);
		}
	}

	RenderItems.pop_back();
	PrimitiveRuntimeData.pop_back();
	PrimitiveColdData.pop_back();
	if (!DynamicPrimitiveMask.empty())
	{
		DynamicPrimitiveMask.pop_back();
	}

	if (RenderItems.empty())
	{
		WorldBVHNodes.clear();
		FreeNodes.clear();
		RootIndex = -1;
		SceneBoundsMin = FVector::ZeroVector;
		SceneBoundsMax = FVector::ZeroVector;
		AdvanceWorldBvhRevision();
		InvalidateVisibilityClusters();
		return true;
	}

	AdvanceWorldBvhRevision();
	InvalidateVisibilityClusters();
	return true;
}

//start,end는 RenderItems의 인덱스 범위, ParentIdx는 WorldBVHNodes의 인덱스, bIsRight는 ParentIdx가 부모 노드의 오른쪽 자식인지 여부
int FScene::CreateWorldBVH(int start, int end, int ParentIdx)
{
	if (start >= end)
		return -1;

	if (end - start == 1)
	{
		WorldBVHNodes[ParentIdx].LeftChildIndex = -1;
		WorldBVHNodes[ParentIdx].RightChildIndex = -1;
		WorldBVHNodes[ParentIdx].PrimitiveIndex = start; // RenderItems 배열 인덱스

		FRenderItem& Item = RenderItems[start];
		Item.BVHLeafIndex = ParentIdx;
		const FVector Margin = (Item.WorldBoundsMax - Item.WorldBoundsMin) * 0.1f;
		Item.LooseBoundsMin = Item.WorldBoundsMin - Margin;
		Item.LooseBoundsMax = Item.WorldBoundsMax + Margin;

		return ParentIdx;
	}

	FVector Extent = WorldBVHNodes[ParentIdx].Max - WorldBVHNodes[ParentIdx].Min;
	int axis = (Extent.X >= Extent.Y && Extent.X >= Extent.Z) ? 0 : (Extent.Y >= Extent.Z ? 1 : 2);

	//양쪽 균일 보장
	int MidIdx = (start + end) / 2;
	std::nth_element(RenderItems.begin() + start, RenderItems.begin() + MidIdx, RenderItems.begin() + end,
		[&](const FRenderItem& A, const FRenderItem& B) {
			float CentA = (A.WorldBoundsMin[axis] + A.WorldBoundsMax[axis]) * 0.5f;
			float CentB = (B.WorldBoundsMin[axis] + B.WorldBoundsMax[axis]) * 0.5f;
			return CentA < CentB;
		});

	// 임시 코드
	std::nth_element(PrimitiveRuntimeData.begin() + start, PrimitiveRuntimeData.begin() + MidIdx, PrimitiveRuntimeData.begin() + end,
		[&](const FScenePrimitiveRuntimeData& A, const FScenePrimitiveRuntimeData& B) {
			float CentA = (A.WorldBoundsMin[axis] + A.WorldBoundsMax[axis]) * 0.5f;
			float CentB = (B.WorldBoundsMin[axis] + B.WorldBoundsMax[axis]) * 0.5f;
			return CentA < CentB;
		});

	AABBNode LeftNode;
	LeftNode.Min = RenderItems[start].WorldBoundsMin;
	LeftNode.Max = RenderItems[start].WorldBoundsMax;
	LeftNode.ParentIndex = ParentIdx;
	for (int i = start + 1; i < MidIdx; ++i)
	{
		LeftNode.Min = FVector::Min(LeftNode.Min, RenderItems[i].WorldBoundsMin);
		LeftNode.Max = FVector::Max(LeftNode.Max, RenderItems[i].WorldBoundsMax);
	}

	AABBNode RightNode;
	RightNode.Min = RenderItems[MidIdx].WorldBoundsMin;
	RightNode.Max = RenderItems[MidIdx].WorldBoundsMax;
	RightNode.ParentIndex = ParentIdx;
	for (int i = MidIdx + 1; i < end; ++i)
	{
		RightNode.Min = FVector::Min(RightNode.Min, RenderItems[i].WorldBoundsMin);
		RightNode.Max = FVector::Max(RightNode.Max, RenderItems[i].WorldBoundsMax);
	}

	int LeftIdx = (int)WorldBVHNodes.size();
	WorldBVHNodes.push_back(LeftNode);
	int RightIdx = (int)WorldBVHNodes.size();
	WorldBVHNodes.push_back(RightNode);

	WorldBVHNodes[ParentIdx].LeftChildIndex = CreateWorldBVH(start, MidIdx, LeftIdx);
	WorldBVHNodes[ParentIdx].RightChildIndex = CreateWorldBVH(MidIdx, end, RightIdx);

	return ParentIdx;
}

void FScene::BuildVisibilityClusters() const
{
	StaticVisibilityClusters.clear();
	StaticClusterPrimitiveIndices.clear();
	VisibilityClusterBuildStatsCache.clear();
	VisibilityClusterBuildStatsValidMask.clear();
	DynamicPrimitiveMask.assign(RenderItems.size(), 0u);
	VisibilityClusterRevision = WorldBVHRevision;
	bVisibilityClustersDirty = false;

	if (RootIndex < 0 || WorldBVHNodes.empty())
	{
		return;
	}

	VisibilityClusterBuildStatsCache.resize(WorldBVHNodes.size());
	VisibilityClusterBuildStatsValidMask.assign(WorldBVHNodes.size(), 0u);

	EmitVisibilityClustersFromNode(RootIndex);
	LogVisibilityClusterBuildSummary();
}

void FScene::EmitVisibilityClustersFromNode(int NodeIndex) const
{
	if (NodeIndex < 0 || NodeIndex >= static_cast<int>(WorldBVHNodes.size()))
	{
		return;
	}

	const AABBNode& Node = WorldBVHNodes[NodeIndex];
	FVisibilityClusterBuildStats BuildStats = {};
	ComputeVisibilityClusterBuildStats(NodeIndex, BuildStats);
	if (BuildStats.PrimitiveCount == 0)
	{
		return;
	}

	if (ShouldEmitVisibilityCluster(NodeIndex, BuildStats))
	{
		FVisibilityCluster Cluster = {};
		Cluster.BoundsMin = Node.Min;
		Cluster.BoundsMax = Node.Max;
		Cluster.PrimitiveOffset = static_cast<uint32>(StaticClusterPrimitiveIndices.size());
		Cluster.PrimitiveCount = BuildStats.PrimitiveCount;
		Cluster.SourceBvhNodeIndex = NodeIndex;
		Cluster.bDynamic = false;

		CollectNodePrimitiveIndices(NodeIndex, StaticClusterPrimitiveIndices);
		StaticVisibilityClusters.push_back(Cluster);
		return;
	}

	EmitVisibilityClustersFromNode(Node.LeftChildIndex);
	EmitVisibilityClustersFromNode(Node.RightChildIndex);
}

void FScene::ComputeVisibilityClusterBuildStats(int NodeIndex, FVisibilityClusterBuildStats& OutStats) const
{
	OutStats = {};

	if (NodeIndex < 0 || NodeIndex >= static_cast<int>(WorldBVHNodes.size()))
	{
		return;
	}

	if (NodeIndex < static_cast<int>(VisibilityClusterBuildStatsCache.size())
		&& NodeIndex < static_cast<int>(VisibilityClusterBuildStatsValidMask.size())
		&& VisibilityClusterBuildStatsValidMask[NodeIndex] != 0)
	{
		OutStats = VisibilityClusterBuildStatsCache[NodeIndex];
		return;
	}

	const AABBNode& Node = WorldBVHNodes[NodeIndex];
	if (Node.IsLeaf())
	{
		if (Node.PrimitiveIndex >= 0
			&& Node.PrimitiveIndex < static_cast<int>(PrimitiveRuntimeData.size()))
		{
			const FScenePrimitiveRuntimeData& Primitive = PrimitiveRuntimeData[Node.PrimitiveIndex];
			const FVector PrimitiveExtent = Primitive.WorldBoundsMax - Primitive.WorldBoundsMin;
			OutStats.PrimitiveCount = 1;
			OutStats.AveragePrimitiveLongestAxis = ComputeLongestAxis(PrimitiveExtent);
		}
	}
	else
	{
		FVisibilityClusterBuildStats LeftStats = {};
		FVisibilityClusterBuildStats RightStats = {};
		ComputeVisibilityClusterBuildStats(Node.LeftChildIndex, LeftStats);
		ComputeVisibilityClusterBuildStats(Node.RightChildIndex, RightStats);

		OutStats.PrimitiveCount = LeftStats.PrimitiveCount + RightStats.PrimitiveCount;
		if (OutStats.PrimitiveCount > 0)
		{
			const float WeightedLongestAxisSum =
				(LeftStats.AveragePrimitiveLongestAxis * static_cast<float>(LeftStats.PrimitiveCount))
				+ (RightStats.AveragePrimitiveLongestAxis * static_cast<float>(RightStats.PrimitiveCount));
			OutStats.AveragePrimitiveLongestAxis = WeightedLongestAxisSum / static_cast<float>(OutStats.PrimitiveCount);
		}
	}

	if (NodeIndex < static_cast<int>(VisibilityClusterBuildStatsCache.size())
		&& NodeIndex < static_cast<int>(VisibilityClusterBuildStatsValidMask.size()))
	{
		VisibilityClusterBuildStatsCache[NodeIndex] = OutStats;
		VisibilityClusterBuildStatsValidMask[NodeIndex] = 1u;
	}
}

bool FScene::ShouldEmitVisibilityCluster(int NodeIndex, const FVisibilityClusterBuildStats& InStats) const
{
	if (NodeIndex < 0 || NodeIndex >= static_cast<int>(WorldBVHNodes.size()))
	{
		return false;
	}

	const AABBNode& Node = WorldBVHNodes[NodeIndex];
	if (Node.IsLeaf())
	{
		return true;
	}

	if (InStats.PrimitiveCount == 0 || InStats.PrimitiveCount > VisibilityClusterPrimitiveTarget)
	{
		return false;
	}

	const FVector NodeExtent = Node.Max - Node.Min;
	const float NodeLongestAxis = ComputeLongestAxis(NodeExtent);
	const float NodeShortestAxis = std::max(ComputeShortestAxis(NodeExtent), VisibilityClusterAxisEpsilon);
	const float NodeAspectRatio = NodeLongestAxis / NodeShortestAxis;
	const float AveragePrimitiveLongestAxis = std::max(InStats.AveragePrimitiveLongestAxis, VisibilityClusterAxisEpsilon);

	return NodeAspectRatio <= VisibilityClusterMaxAspectRatio
		&& NodeLongestAxis <= (VisibilityClusterMaxLongestAxisScale * AveragePrimitiveLongestAxis);
}

bool FScene::TryBuildVisibilityClusterFromNode(int NodeIndex, TArray<uint32>& InOutFramePrimitiveIndices, FVisibilityCluster& OutCluster) const
{
	OutCluster = FVisibilityCluster();

	if (NodeIndex < 0 || NodeIndex >= static_cast<int>(WorldBVHNodes.size()))
	{
		return false;
	}

	if (NodeIndex >= static_cast<int>(VisibilityClusterBuildStatsCache.size())
		|| NodeIndex >= static_cast<int>(VisibilityClusterBuildStatsValidMask.size())
		|| VisibilityClusterBuildStatsValidMask[NodeIndex] == 0)
	{
		return false;
	}

	const FVisibilityClusterBuildStats& BuildStats = VisibilityClusterBuildStatsCache[NodeIndex];
	if (BuildStats.PrimitiveCount == 0)
	{
		return false;
	}

	const AABBNode& Node = WorldBVHNodes[NodeIndex];
	OutCluster.BoundsMin = Node.Min;
	OutCluster.BoundsMax = Node.Max;
	OutCluster.PrimitiveOffset = static_cast<uint32>(InOutFramePrimitiveIndices.size());
	OutCluster.PrimitiveCount = BuildStats.PrimitiveCount;
	OutCluster.SourceBvhNodeIndex = NodeIndex;
	OutCluster.bUsesFramePrimitiveIndices = true;
	OutCluster.bDynamic = false;
	CollectNodePrimitiveIndices(NodeIndex, InOutFramePrimitiveIndices);
	return true;
}

void FScene::CollectNodePrimitiveIndices(int NodeIndex, TArray<uint32>& OutPrimitiveIndices) const
{
	if (NodeIndex < 0 || NodeIndex >= static_cast<int>(WorldBVHNodes.size()))
	{
		return;
	}

	const AABBNode& Node = WorldBVHNodes[NodeIndex];
	if (Node.IsLeaf())
	{
		if (Node.PrimitiveIndex >= 0)
		{
			OutPrimitiveIndices.push_back(static_cast<uint32>(Node.PrimitiveIndex));
		}
		return;
	}

	CollectNodePrimitiveIndices(Node.LeftChildIndex, OutPrimitiveIndices);
	CollectNodePrimitiveIndices(Node.RightChildIndex, OutPrimitiveIndices);
}

void FScene::LogVisibilityClusterBuildSummary() const
{
	if (StaticVisibilityClusters.empty())
	{
		return;
	}

	uint32 MaxPrimitivePerCluster = 0;
	for (const FVisibilityCluster& Cluster : StaticVisibilityClusters)
	{
		MaxPrimitivePerCluster = std::max(MaxPrimitivePerCluster, Cluster.PrimitiveCount);
	}

	const double AveragePrimitivePerCluster =
		static_cast<double>(StaticClusterPrimitiveIndices.size()) / static_cast<double>(StaticVisibilityClusters.size());

	std::ostringstream Stream;
	Stream
		<< "Scene Visibility Clusters: "
		<< "StaticClusters=" << StaticVisibilityClusters.size()
		<< ", AveragePrimitivePerCluster=" << AveragePrimitivePerCluster
		<< ", MaxPrimitivePerCluster=" << MaxPrimitivePerCluster
		<< '\n';
	OutputDebugStringA(Stream.str().c_str());
}

int FScene::AllocateBVHNode()
{
	if (FreeNodes.empty())
	{
		int NewIndex = (int)WorldBVHNodes.size();
		WorldBVHNodes.emplace_back();
		return NewIndex;
	}
	else
	{
		int ReuseIndex = FreeNodes.back();
		FreeNodes.pop_back();
		return ReuseIndex;
	}
}

int FScene::FreeNode(int NodeIdx)
{
	FreeNodes.push_back(NodeIdx);
	return NodeIdx;
}

void FScene::RefitUpward(int ParentIdx)
{
	while (ParentIdx != -1)
	{
		AABBNode& Node = WorldBVHNodes[ParentIdx];
		if (Node.LeftChildIndex != -1 && Node.RightChildIndex != -1)
		{
			Node.Min = FVector::Min(WorldBVHNodes[Node.LeftChildIndex].Min,
				WorldBVHNodes[Node.RightChildIndex].Min);
			Node.Max = FVector::Max(WorldBVHNodes[Node.LeftChildIndex].Max,
				WorldBVHNodes[Node.RightChildIndex].Max);
		}
		ParentIdx = Node.ParentIndex;
	}
}

int FScene::InsertLeaf(const FVector& Min, const FVector& Max, int PrimitiveIndex)
{
	int LeafIdx = AllocateBVHNode();
	WorldBVHNodes[LeafIdx].LeftChildIndex = -1;
	WorldBVHNodes[LeafIdx].RightChildIndex = -1;
	WorldBVHNodes[LeafIdx].Min = Min;
	WorldBVHNodes[LeafIdx].Max = Max;
	WorldBVHNodes[LeafIdx].PrimitiveIndex = PrimitiveIndex;

	int SiblingIdx = FindBestSibling(Min, Max);

	int NewInternalIdx = AllocateBVHNode();
	WorldBVHNodes[NewInternalIdx].PrimitiveIndex = -1; // 내부 노드
	int OldParentIdx = WorldBVHNodes[SiblingIdx].ParentIndex;
	WorldBVHNodes[NewInternalIdx].ParentIndex = OldParentIdx;
	WorldBVHNodes[NewInternalIdx].LeftChildIndex = SiblingIdx;
	WorldBVHNodes[NewInternalIdx].RightChildIndex = LeafIdx;

	WorldBVHNodes[SiblingIdx].ParentIndex = NewInternalIdx;
	WorldBVHNodes[LeafIdx].ParentIndex = NewInternalIdx;

	if (OldParentIdx != -1)
	{
		if (WorldBVHNodes[OldParentIdx].LeftChildIndex == SiblingIdx)
			WorldBVHNodes[OldParentIdx].LeftChildIndex = NewInternalIdx;
		else
			WorldBVHNodes[OldParentIdx].RightChildIndex = NewInternalIdx;
	}
	else
	{
		RootIndex = NewInternalIdx;  // 루트 교체
	}

	// 5. AABB 갱신
	RefitUpward(NewInternalIdx);
	return LeafIdx;

}

int FScene::FindBestSibling(const FVector& NewMin, const FVector& NewMax)
{
	// 우선순위 큐: (누적비용, 노드인덱스)
	std::priority_queue<std::pair<float, int>,
		std::vector<std::pair<float, int>>,
		std::greater<>> PQ;
	PQ.push({ 0.0f, RootIndex });

	float BestCost = std::numeric_limits<float>::max();
	int BestNode = RootIndex;

	while (!PQ.empty())
	{
		auto [InheritedCost, NodeIdx] = PQ.top(); PQ.pop();

		// 이 노드 옆에 삽입했을 때 총 비용
		FVector MergedMin = FVector::Min(WorldBVHNodes[NodeIdx].Min, NewMin);
		FVector MergedMax = FVector::Max(WorldBVHNodes[NodeIdx].Max, NewMax);
		float DirectCost = SurfaceArea(MergedMin, MergedMax);
		float TotalCost = DirectCost + InheritedCost;


		float LowerBound = InheritedCost + SurfaceArea(NewMin, NewMax);
		if (LowerBound > BestCost) continue;

		if (BestCost > TotalCost)
		{
			BestCost = TotalCost;
			BestNode = NodeIdx;
		}

		if (WorldBVHNodes[NodeIdx].IsLeaf()) continue;

		// SA 증가량을 다음 노드의 InheritedCost에 누적
		float ChildInherited = InheritedCost + (DirectCost - SurfaceArea(WorldBVHNodes[NodeIdx].Min, WorldBVHNodes[NodeIdx].Max));
		PQ.push({ ChildInherited, WorldBVHNodes[NodeIdx].LeftChildIndex });
		PQ.push({ ChildInherited, WorldBVHNodes[NodeIdx].RightChildIndex });

	}

	return BestNode;
}

void FScene::RemoveLeaf(int LeafIdx)
{
	int ParentIdx = WorldBVHNodes[LeafIdx].ParentIndex;
	if (ParentIdx == -1)
	{
		// 루트가 리프인 경우
		RootIndex = -1;
		FreeNode(LeafIdx);
		return;
	}

	int GrandParentIdx = WorldBVHNodes[ParentIdx].ParentIndex;

	// 형제 찾기
	int SiblingIdx = (WorldBVHNodes[ParentIdx].LeftChildIndex == LeafIdx)
		? WorldBVHNodes[ParentIdx].RightChildIndex
		: WorldBVHNodes[ParentIdx].LeftChildIndex;

	// 형제를 부모 자리로 올림
	WorldBVHNodes[SiblingIdx].ParentIndex = GrandParentIdx;
	if (GrandParentIdx != -1)
	{
		if (WorldBVHNodes[GrandParentIdx].LeftChildIndex == ParentIdx)
			WorldBVHNodes[GrandParentIdx].LeftChildIndex = SiblingIdx;
		else
			WorldBVHNodes[GrandParentIdx].RightChildIndex = SiblingIdx;
	}
	else
	{
		RootIndex = SiblingIdx;
	}

	FreeNode(ParentIdx);
	FreeNode(LeafIdx);

	// AABB 갱신
	RefitUpward(GrandParentIdx);
}


float FScene::SurfaceArea(const FVector& Min, const FVector& Max)
{
	float X = Max.X - Min.X;
	float Y = Max.Y - Min.Y;
	float Z = Max.Z - Min.Z;
	return 2.0f * (X * Y + Y * Z + X * Z);
}

void FScene::MoveLeaf(int RenderItemIndex, const FTransform& NewTransform)
{
	FRenderItem& Item = RenderItems[RenderItemIndex];

	// Transform 갱신
	Item.Transform = NewTransform;

	// 새 AABB 계산
	bool bHasBounds = false;
	ExpandBoundsWithTransformedAabb(
		Item.StaticMesh->GetBoundsMin(),
		Item.StaticMesh->GetBoundsMax(),
		NewTransform,
		Item.WorldBoundsMin,
		Item.WorldBoundsMax,
		bHasBounds);
	Item.WorldBoundsSphere = TransformBoundingSphere(Item.StaticMesh->GetBoundsSphere(), NewTransform);

	const FVector& NewMin = Item.WorldBoundsMin;
	const FVector& NewMax = Item.WorldBoundsMax;
	if (RenderItemIndex >= 0 && RenderItemIndex < static_cast<int>(PrimitiveRuntimeData.size()))
	{
		FScenePrimitiveRuntimeData& RuntimeData = PrimitiveRuntimeData[RenderItemIndex];
		RuntimeData.WorldMatrix = NewTransform.ToMatrixWithScale();
		RuntimeData.InverseWorldMatrix = RuntimeData.WorldMatrix.GetInverse();
		RuntimeData.WorldBoundsMin = NewMin;
		RuntimeData.WorldBoundsMax = NewMax;
	}

	MarkPrimitiveDynamic(RenderItemIndex);

	if (Item.BVHLeafIndex < 0
		|| Item.BVHLeafIndex >= static_cast<int32>(WorldBVHNodes.size()))
	{
		UpdatePrimitiveRuntimeData(RenderItemIndex);
		RebuildSceneBoundsFromRenderItems();
		AdvanceWorldBvhRevision();
		return;
	}

	// Loose AABB 안에 있으면 리프 AABB만 갱신 후 Refit
	bool bInsideLoose =
		NewMin.X >= Item.LooseBoundsMin.X && NewMin.Y >= Item.LooseBoundsMin.Y && NewMin.Z >= Item.LooseBoundsMin.Z &&
		NewMax.X <= Item.LooseBoundsMax.X && NewMax.Y <= Item.LooseBoundsMax.Y && NewMax.Z <= Item.LooseBoundsMax.Z;

	if (bInsideLoose)
	{
		WorldBVHNodes[Item.BVHLeafIndex].Min = NewMin;
		WorldBVHNodes[Item.BVHLeafIndex].Max = NewMax;
		RefitUpward(WorldBVHNodes[Item.BVHLeafIndex].ParentIndex);
	}
	else
	{
		RemoveLeaf(Item.BVHLeafIndex);

		const FVector Margin = (NewMax - NewMin) * 0.1f;
		Item.LooseBoundsMin = NewMin - Margin;
		Item.LooseBoundsMax = NewMax + Margin;
		Item.BVHLeafIndex = InsertLeaf(Item.LooseBoundsMin, Item.LooseBoundsMax, RenderItemIndex);
	}

	UpdatePrimitiveRuntimeData(RenderItemIndex);
	AdvanceWorldBvhRevision();
}

void FScene::EnsureVisibilityClustersBuilt() const
{
	if (!bVisibilityClustersDirty)
	{
		return;
	}

	BuildVisibilityClusters();
}

void FScene::InvalidateVisibilityClusters()
{
	StaticVisibilityClusters.clear();
	StaticClusterPrimitiveIndices.clear();
	VisibilityClusterBuildStatsCache.clear();
	VisibilityClusterBuildStatsValidMask.clear();
	VisibilityClusterRevision = 0;
	bVisibilityClustersDirty = true;
}

bool FScene::HasCurrentVisibilityClusterSnapshot() const
{
	return !bVisibilityClustersDirty && VisibilityClusterRevision == WorldBVHRevision;
}

void FScene::MarkPrimitiveDynamic(int32 PrimitiveIndex)
{
	if (PrimitiveIndex < 0)
	{
		return;
	}

	if (DynamicPrimitiveMask.size() != RenderItems.size())
	{
		DynamicPrimitiveMask.assign(RenderItems.size(), 0u);
	}

	const size_t PrimitiveArrayIndex = static_cast<size_t>(PrimitiveIndex);
	if (PrimitiveArrayIndex < DynamicPrimitiveMask.size())
	{
		DynamicPrimitiveMask[PrimitiveArrayIndex] = 1u;
	}
}

void FScene::AdvanceWorldBvhRevision()
{
	++WorldBVHRevision;
}

void FScene::UpdatePrimitiveRuntimeData(int32 PrimitiveIndex)
{
	if (PrimitiveIndex < 0)
	{
		return;
	}

	const size_t PrimitiveArrayIndex = static_cast<size_t>(PrimitiveIndex);
	if (PrimitiveArrayIndex >= RenderItems.size() || PrimitiveArrayIndex >= PrimitiveRuntimeData.size())
	{
		return;
	}

	const FRenderItem& Item = RenderItems[PrimitiveArrayIndex];
	FScenePrimitiveRuntimeData& RuntimeData = PrimitiveRuntimeData[PrimitiveArrayIndex];
	RuntimeData.PrimitiveId = Item.PrimitiveId;
	RuntimeData.WorldMatrix = Item.Transform.ToMatrixWithScale();
	RuntimeData.InverseWorldMatrix = RuntimeData.WorldMatrix.GetInverse();
	RuntimeData.WorldBoundsMin = Item.WorldBoundsMin;
	RuntimeData.WorldBoundsMax = Item.WorldBoundsMax;
	RuntimeData.WorldBoundsSphere = Item.WorldBoundsSphere;
	RuntimeData.StaticMesh = Item.StaticMesh.get();
}

void FScene::UpdateSceneBoundsFromRoot()
{
	if (RootIndex < 0 || RootIndex >= static_cast<int32>(WorldBVHNodes.size()))
	{
		SceneBoundsMin = FVector::ZeroVector;
		SceneBoundsMax = FVector::ZeroVector;
		return;
	}

	SceneBoundsMin = WorldBVHNodes[RootIndex].Min;
	SceneBoundsMax = WorldBVHNodes[RootIndex].Max;
}

bool FScene::AppendStaticMeshPrimitive(
	int32 PrimitiveId,
	const FString& InMeshAssetKey,
	const std::shared_ptr<FStaticMesh>& InStaticMesh,
	const FTransform& InTransform)
{
	if (!InStaticMesh || !InStaticMesh->IsValid())
	{
		return false;
	}

	FRenderItem RenderItem;
	RenderItem.PrimitiveId = PrimitiveId;
	RenderItem.MeshAssetPath = InMeshAssetKey;
	RenderItem.Transform = InTransform;
	RenderItem.StaticMesh = InStaticMesh;

	FScenePrimitiveColdData ColdData;
	ColdData.MeshAssetPath = InMeshAssetKey;
	ColdData.StaticMeshOwner = InStaticMesh;

	FScenePrimitiveRuntimeData RuntimeData;
	RuntimeData.PrimitiveId = PrimitiveId;
	RuntimeData.WorldMatrix = InTransform.ToMatrixWithScale();
	RuntimeData.InverseWorldMatrix = RuntimeData.WorldMatrix.GetInverse();
	RuntimeData.StaticMesh = InStaticMesh.get();
	RuntimeData.WorldBoundsSphere = TransformBoundingSphere(InStaticMesh->GetBoundsSphere(), InTransform);

	bool bHasRuntimeBounds = false;
	ExpandBoundsWithTransformedAabb(
		InStaticMesh->GetBoundsMin(),
		InStaticMesh->GetBoundsMax(),
		InTransform,
		RuntimeData.WorldBoundsMin,
		RuntimeData.WorldBoundsMax,
		bHasRuntimeBounds);

	if (bHasRuntimeBounds)
	{
		RenderItem.WorldBoundsMin = RuntimeData.WorldBoundsMin;
		RenderItem.WorldBoundsMax = RuntimeData.WorldBoundsMax;
		RenderItem.WorldBoundsSphere = RuntimeData.WorldBoundsSphere;
	}

	RenderItems.push_back(std::move(RenderItem));
	PrimitiveColdData.push_back(std::move(ColdData));
	PrimitiveRuntimeData.push_back(std::move(RuntimeData));
	return true;
}

void FScene::RebuildSceneBoundsFromRenderItems()
{
	bool bHasSceneBounds = false;
	FVector BoundsMin = FVector::ZeroVector;
	FVector BoundsMax = FVector::ZeroVector;

	for (const FRenderItem& RenderItem : RenderItems)
	{
		if (!RenderItem.StaticMesh || !RenderItem.StaticMesh->IsValid())
		{
			continue;
		}

		ExpandBounds(BoundsMin, BoundsMax, bHasSceneBounds, RenderItem.WorldBoundsMin);
		ExpandBounds(BoundsMin, BoundsMax, bHasSceneBounds, RenderItem.WorldBoundsMax);
	}

	if (!bHasSceneBounds)
	{
		SceneBoundsMin = FVector::ZeroVector;
		SceneBoundsMax = FVector::ZeroVector;
		return;
	}

	SceneBoundsMin = BoundsMin;
	SceneBoundsMax = BoundsMax;
}
