#include "Scene.h"

#include <algorithm>
#include <cctype>
#include <memory>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <string>
#include "limits"

#include "Math/MathUtility.h"
#include "StaticMesh/StaticMesh.h"
#include "Types/Queue.h"
#include "BVH/BVHBuilder.h"


namespace
{
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

	std::filesystem::path ResolveAssetPath(const std::filesystem::path& InScenePath, const FString& InRelativeAssetPath)
	{
		const std::filesystem::path AssetPath(InRelativeAssetPath);
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
	std::ifstream SceneFile(SceneFilePath);
	if (!SceneFile)
	{
		return false;
	}

	bool bInCameraBlock = false;
	bool bInPrimitivesBlock = false;
	bool bInPrimitiveBlock = false;
	bool bHasSceneBounds = false;

	FPendingPrimitive PendingPrimitive;
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

				FRenderItem RenderItem;
				RenderItem.PrimitiveId = PendingPrimitive.Id;
				RenderItem.MeshAssetPath = MeshCacheKey;
				RenderItem.Transform = WorldTransform;
				RenderItem.StaticMesh = SharedMesh;

				FScenePrimitiveColdData ColdData;
				ColdData.MeshAssetPath = MeshCacheKey;
				ColdData.StaticMeshOwner = SharedMesh;

				FScenePrimitiveRuntimeData RuntimeData;
				RuntimeData.PrimitiveId = PendingPrimitive.Id;
				RuntimeData.WorldMatrix = WorldTransform.ToMatrixWithScale();
				RuntimeData.InverseWorldMatrix = RuntimeData.WorldMatrix.GetInverse();

				RuntimeData.StaticMesh = SharedMesh.get();

				bool bHasRuntimeBounds = false;
				ExpandBoundsWithTransformedAabb(
					SharedMesh->GetBoundsMin(),
					SharedMesh->GetBoundsMax(),
					WorldTransform,
					RuntimeData.WorldBoundsMin,
					RuntimeData.WorldBoundsMax,
					bHasRuntimeBounds);

				if (bHasRuntimeBounds)
				{
					RenderItem.WorldBoundsMin = RuntimeData.WorldBoundsMin;
					RenderItem.WorldBoundsMax = RuntimeData.WorldBoundsMax;
					ExpandBounds(SceneBoundsMin, SceneBoundsMax, bHasSceneBounds, RuntimeData.WorldBoundsMin);
					ExpandBounds(SceneBoundsMin, SceneBoundsMax, bHasSceneBounds, RuntimeData.WorldBoundsMax);
				}

				RenderItems.push_back(std::move(RenderItem));
				PrimitiveColdData.push_back(std::move(ColdData));
				PrimitiveRuntimeData.push_back(std::move(RuntimeData));
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

	//Create World BVH here

	//For Check
	//For Check

	WorldBVHNodes.clear();
	WorldBVHNodes.reserve(RenderItems.size() * 2);
	AABBNode RootNode;
	RootNode.Min = SceneBoundsMin;
	RootNode.Max = SceneBoundsMax;
	WorldBVHNodes.push_back(RootNode); // index 0 = root
	RootIndex = 0;
	// Sort RenderItems
	CreateWorldBVH(0, (int)RenderItems.size(), 0);

	InitialCamera.Transform = FTransform(MakeSceneRotation(RawCameraRotation), RawCameraLocation, FVector::OneVector);
	if (!bHasSceneBounds)
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
	MeshManager.Release();
	InitialCamera = FSceneCameraInitData();
	RawCameraLocation = FVector::ZeroVector;
	RawCameraRotation = FVector::ZeroVector;
	SceneBoundsMin = FVector::ZeroVector;
	SceneBoundsMax = FVector::ZeroVector;
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

		if (TotalCost < BestCost)
		{
			BestCost = TotalCost;
			BestNode = NodeIdx;
		}

		// 자식 탐색 가치가 있는지 판단
		// 자식에 삽입해도 최소 InheritedCost + SA(NewLeaf)는 들음
		// 이게 이미 BestCost보다 크면 이 가지는 스킵
		float LowerBound = InheritedCost + SurfaceArea(NewMin, NewMax);
		if (LowerBound < BestCost && !WorldBVHNodes[NodeIdx].IsLeaf())
		{
			// SA 증가량을 다음 노드의 InheritedCost에 누적
			float ChildInherited = InheritedCost + (DirectCost - SurfaceArea(WorldBVHNodes[NodeIdx].Min, WorldBVHNodes[NodeIdx].Max));
			PQ.push({ ChildInherited, WorldBVHNodes[NodeIdx].LeftChildIndex });
			PQ.push({ ChildInherited, WorldBVHNodes[NodeIdx].RightChildIndex });
		}
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

	const FVector& NewMin = Item.WorldBoundsMin;
	const FVector& NewMax = Item.WorldBoundsMax;

	// Loose AABB 안에 있으면 리프 AABB만 갱신 후 Refit
	bool bInsideLoose =
		NewMin.X >= Item.LooseBoundsMin.X && NewMin.Y >= Item.LooseBoundsMin.Y && NewMin.Z >= Item.LooseBoundsMin.Z &&
		NewMax.X <= Item.LooseBoundsMax.X && NewMax.Y <= Item.LooseBoundsMax.Y && NewMax.Z <= Item.LooseBoundsMax.Z;

	if (bInsideLoose)
	{
		WorldBVHNodes[Item.BVHLeafIndex].Min = NewMin;
		WorldBVHNodes[Item.BVHLeafIndex].Max = NewMax;
		RefitUpward(WorldBVHNodes[Item.BVHLeafIndex].ParentIndex);
		return;
	}

	// Loose AABB 벗어남 → 삭제 후 재삽입
	RemoveLeaf(Item.BVHLeafIndex);

	const FVector Margin = (NewMax - NewMin) * 0.1f;
	Item.LooseBoundsMin = NewMin - Margin;
	Item.LooseBoundsMax = NewMax + Margin;
	Item.BVHLeafIndex = InsertLeaf(Item.LooseBoundsMin, Item.LooseBoundsMax, RenderItemIndex);
}
