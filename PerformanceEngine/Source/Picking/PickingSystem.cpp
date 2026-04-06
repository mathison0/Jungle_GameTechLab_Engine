#include "Picking/PickingSystem.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include "Types/Stack.h"

#include "Camera/Camera.h"
#include "Scene/Scene.h"
#include "StaticMesh/StaticMesh.h"
#include "Visibility/VisibilitySystem.h"

namespace
{
	uint64 QueryCycles64()
	{
		LARGE_INTEGER Counter = {};
		QueryPerformanceCounter(&Counter);
		return static_cast<uint64>(Counter.QuadPart);
	}

	double GetSecondsPerCycle()
	{
		static const double SecondsPerCycle = []()
			{
				LARGE_INTEGER Frequency = {};
				QueryPerformanceFrequency(&Frequency);
				return 1.0 / static_cast<double>(Frequency.QuadPart);
			}();
		return SecondsPerCycle;
	}

	double CyclesToMilliseconds(uint64 InStartCycles, uint64 InEndCycles)
	{
		return static_cast<double>(InEndCycles - InStartCycles) * GetSecondsPerCycle() * 1000.0;
	}

	FRay BuildPickRay(const FCamera& InCamera, int32 InMouseX, int32 InMouseY, int32 InViewportWidth, int32 InViewportHeight)
	{
		FRay Result = {};
		Result.Origin = InCamera.GetLocation();
		Result.Direction = InCamera.GetRotation().GetForwardVector();

		if (InViewportWidth <= 0 || InViewportHeight <= 0)
		{
			return Result;
		}

		const float PixelX = (static_cast<float>(InMouseX) + 0.5f) / static_cast<float>(InViewportWidth);
		const float PixelY = (static_cast<float>(InMouseY) + 0.5f) / static_cast<float>(InViewportHeight);
		const float NdcX = PixelX * 2.0f - 1.0f;
		const float NdcY = 1.0f - PixelY * 2.0f;

		const FMatrix ViewProjection = InCamera.GetViewMatrix() * InCamera.GetProjectionMatrix();
		const FMatrix InverseViewProjection = ViewProjection.GetInverse();
		const FVector WorldNear = InverseViewProjection.TransformPosition(FVector(NdcX, NdcY, 0.0f));
		const FVector WorldFar = InverseViewProjection.TransformPosition(FVector(NdcX, NdcY, 1.0f));
		const FVector Direction = (WorldFar - WorldNear).GetSafeNormal();
		if (!Direction.IsNearlyZero())
		{
			Result.Direction = Direction;
		}

		return Result;
	}

	// AABB와의 교차 진입 거리를 반환. 미교차 시 1e30f 반환.
	// InInvDirection: 레이 방향의 역수(1/Direction), 호출 전 미리 계산해서 전달.
	float IntersectRayAabb(const FVector& InOrigin, const FVector& InInvDirection, const FVector& InBoundsMin, const FVector& InBoundsMax)
	{
		float TMin = 0.0f;
		float TMax = std::numeric_limits<float>::max();

		for (int32 AxisIndex = 0; AxisIndex < 3; ++AxisIndex)
		{
			const float Origin = InOrigin[AxisIndex];
			const float InverseDirection = InInvDirection[AxisIndex];
			const float BoundsMin = InBoundsMin[AxisIndex];
			const float BoundsMax = InBoundsMax[AxisIndex];

			// InverseDirection이 매우 크면 원래 Direction이 거의 0 (평행 레이)
			if (std::fabs(InverseDirection) > 1.e8f)
			{
				if (Origin < BoundsMin || Origin > BoundsMax)
				{
					return 1e30f;
				}
				continue;
			}

			float T0 = (BoundsMin - Origin) * InverseDirection;
			float T1 = (BoundsMax - Origin) * InverseDirection;
			if (T0 > T1)
			{
				std::swap(T0, T1);
			}

			TMin = std::max(TMin, T0);
			TMax = std::min(TMax, T1);
			if (TMin > TMax)
			{
				return 1e30f;
			}
		}

		return TMin;
	}

	bool IntersectRayTriangle(
		const FRay& InRay,
		const FVector& InA,
		const FVector& InB,
		const FVector& InC,
		float& OutDistance,
		FVector& OutWorldPosition)
	{
		const FVector EdgeAB = InB - InA;
		const FVector EdgeAC = InC - InA;
		const FVector PVector = FVector::CrossProduct(InRay.Direction, EdgeAC);
		const float Determinant = FVector::DotProduct(EdgeAB, PVector);
		if (Determinant < 1.e-8f)  // 뒷면(Determinant <= 0) 및 평행 레이 모두 거부
		{
			return false;
		}

		const float InverseDeterminant = 1.0f / Determinant;
		const FVector TVector = InRay.Origin - InA;
		const float U = FVector::DotProduct(TVector, PVector) * InverseDeterminant;
		if (U < 0.0f || U > 1.0f)
		{
			return false;
		}

		const FVector QVector = FVector::CrossProduct(TVector, EdgeAB);
		const float V = FVector::DotProduct(InRay.Direction, QVector) * InverseDeterminant;
		if (V < 0.0f || U + V > 1.0f)
		{
			return false;
		}

		const float T = FVector::DotProduct(EdgeAC, QVector) * InverseDeterminant;
		if (T <= 0.0f)
		{
			return false;
		}

		OutDistance = T;
		OutWorldPosition = InRay.Origin + InRay.Direction * T;
		return true;
	}

	bool IntersectRenderItem(const FRay& InRay, const FScenePrimitiveRuntimeData& InPrimitiveRuntimeData, FPickHit& InOutBestHit)
	{
		FStaticMesh* StaticMesh = InPrimitiveRuntimeData.StaticMesh;
		if (StaticMesh == nullptr || !StaticMesh->IsValid())
		{
			return false;
		}

		const FBVHSpatialData* SpatialData =
			static_cast<const FBVHSpatialData*>(StaticMesh->GetSpatialData().get());

		if (!SpatialData || SpatialData->Nodes.empty()) return false;

		const TArray<FBVHNode>& Nodes = SpatialData->Nodes;
		const TArray<uint32>& TriangleIndices = SpatialData->TriangleIndices;
		const TArray<FStaticMeshVertex>& Vertices = StaticMesh->GetVertices();
		const TArray<uint32>& Indices = StaticMesh->GetIndices();


		bool bHit = { false };

		FRay LocalRay;

		LocalRay.Origin = InPrimitiveRuntimeData.InverseWorldMatrix.TransformPosition(InRay.Origin);
		LocalRay.Direction = InPrimitiveRuntimeData.InverseWorldMatrix.TransformVector(InRay.Direction).GetSafeNormal();

		// 레이 방향 역수를 미리 계산 — BVH 순회 중 방향은 변하지 않으므로 나눗셈을 1회로 줄임
		const FVector LocalInvDir(
			1.0f / LocalRay.Direction.X,
			1.0f / LocalRay.Direction.Y,
			1.0f / LocalRay.Direction.Z
		);

		// 루트 AABB miss면 즉시 탈출
		if (IntersectRayAabb(LocalRay.Origin, LocalInvDir, Nodes[0].BoundMin, Nodes[0].BoundMax) == 1e30f)
		{
			return false;
		}

		float BestLocalT = 1e30f;

		// BVH 깊이는 64 초과 불가
		int32 Stack[64];
		int32 StackPtr = 0;
		const FBVHNode* Node = &Nodes[0];

		while (true)
		{
			if (Node->IsLeaf())
			{
				for (int32 i = 0; i < Node->PrimitiveCount; ++i)
				{
					uint32 TriangleIndex = TriangleIndices[Node->LeftFirst + i];
					const FVector& Vertex0 = Vertices[Indices[TriangleIndex * 3 + 0]].Position;
					const FVector& Vertex1 = Vertices[Indices[TriangleIndex * 3 + 1]].Position;
					const FVector& Vertex2 = Vertices[Indices[TriangleIndex * 3 + 2]].Position;

					float LocalT = 0.0f;
					FVector LocalHitPos;
					if (!IntersectRayTriangle(LocalRay, Vertex0, Vertex1, Vertex2, LocalT, LocalHitPos)) continue;
					if (LocalT >= BestLocalT) continue;

					const FVector WorldHitPos = InPrimitiveRuntimeData.WorldMatrix.TransformPosition(LocalHitPos);
					const float DistSq = FVector::DistSquared(InRay.Origin, WorldHitPos);
					if (DistSq >= InOutBestHit.DistanceSquared) continue;

					BestLocalT = LocalT;
					InOutBestHit.DistanceSquared = DistSq;
					InOutBestHit.PrimitiveId = InPrimitiveRuntimeData.PrimitiveId;
					InOutBestHit.WorldPosition = WorldHitPos;
					bHit = true;
				}
				if (StackPtr == 0) break;
				Node = &Nodes[Stack[--StackPtr]];
				continue;
			}

			// 두 자식의 AABB 진입 거리 계산
			const FBVHNode* Child1 = &Nodes[Node->LeftFirst];
			const FBVHNode* Child2 = &Nodes[Node->LeftFirst + 1];
			float Dist1 = IntersectRayAabb(LocalRay.Origin, LocalInvDir, Child1->BoundMin, Child1->BoundMax);
			float Dist2 = IntersectRayAabb(LocalRay.Origin, LocalInvDir, Child2->BoundMin, Child2->BoundMax);

			// 가까운 쪽을 먼저 방문, 먼 쪽만 스택에 저장
			if (Dist1 > Dist2) { std::swap(Dist1, Dist2); std::swap(Child1, Child2); }

			if (Dist1 >= BestLocalT)
			{
				// 두 자식 모두 miss 또는 현재 hit보다 멀면 스택에서 꺼내기
				if (StackPtr == 0) break;
				Node = &Nodes[Stack[--StackPtr]];
			}
			else
			{
				// 가까운 쪽으로 직접 이동, 먼 쪽은 교차할 경우만 스택에 push
				Node = Child1;
				if (Dist2 < BestLocalT) Stack[StackPtr++] = static_cast<int32>(Child2 - &Nodes[0]);
			}
		}

		return bHit;
	}

	void IntersectWorldBVH(const FRay& InRay, const FScene& Scene, FPickHit& InOutPickHit, FPickState& State)
	{
		// 현재 베스트 히트까지의 T값(월드 거리)으로 가지치기
		float BestDistSq = InOutPickHit.DistanceSquared;

		// 레이 방향 역수를 미리 계산 — 순회 중 방향은 변하지 않으므로 나눗셈을 1회로 줄임
		const FVector WorldInvDir(
			1.0f / InRay.Direction.X,
			1.0f / InRay.Direction.Y,
			1.0f / InRay.Direction.Z
		);

		const int32 RootIndex = Scene.GetWorldBVHRootIndex();
		if (RootIndex < 0 || RootIndex >= static_cast<int32>(Scene.GetWorldBVHNodes().size()))
		{
			return;
		}

		TStack<int> NodeStack;
		NodeStack.push(RootIndex);
		while (!NodeStack.empty())
		{
			int BVHNodeIndex = NodeStack.top(); NodeStack.pop();
			++State.TotalAABBCheckCount;
			const AABBNode& Node = Scene.GetWorldBVHNodes()[BVHNodeIndex];
			const float tNode = IntersectRayAabb(InRay.Origin, WorldInvDir, Node.Min, Node.Max);
			
			if (tNode == 1e30f || tNode * tNode >= BestDistSq)
			{
				continue;
			}

			// TODO : Debug용 로직 주석처리 해야함.
			State.TraversedWorldBVHNodes.push_back(Node);
			if (Node.IsLeaf())
			{
				const size_t PrimIdx = static_cast<size_t>(Node.PrimitiveIndex);
				if (PrimIdx < Scene.GetPrimitiveRuntimeData().size())
				{
					if (IntersectRenderItem(InRay, Scene.GetPrimitiveRuntimeData()[PrimIdx], InOutPickHit))
					{
						InOutPickHit.PrimitiveIndex = static_cast<int32>(PrimIdx);
						BestDistSq = InOutPickHit.DistanceSquared; // 더 가까운 히트 갱신
					}
				}
			}
			else
			{
				float distToLeft = 1e30f;
				float distToRight = 1e30f;
				if (Node.LeftChildIndex != -1)
				{
					AABBNode Left = Scene.GetWorldBVHNodes()[Node.LeftChildIndex];
					float t = IntersectRayAabb(InRay.Origin, WorldInvDir, Left.Min, Left.Max);
					if (t * t < BestDistSq) distToLeft = t;
				}
				if (Node.RightChildIndex != -1)
				{
					AABBNode Right = Scene.GetWorldBVHNodes()[Node.RightChildIndex];
					float t = IntersectRayAabb(InRay.Origin, WorldInvDir, Right.Min, Right.Max);
					if (t * t < BestDistSq) distToRight = t;
				}
				// 가까운 쪽을 나중에 꺼내도록 먼 쪽을 먼저 push
				if (distToLeft < 1e30f && distToRight < 1e30f)
				{
					if (distToLeft < distToRight)
					{
						NodeStack.push(Node.RightChildIndex);
						NodeStack.push(Node.LeftChildIndex);
					}
					else
					{
						NodeStack.push(Node.LeftChildIndex);
						NodeStack.push(Node.RightChildIndex);
					}
				}
				else if (distToLeft < 1e30f)  { NodeStack.push(Node.LeftChildIndex); }
				else if (distToRight < 1e30f) { NodeStack.push(Node.RightChildIndex); }
			}
		}
	}
}

void FPickingSystem::Reset()
{
}


void FPickingSystem::UpdatePick(
	const FScene& InScene,
	const FCamera& InCamera,
	const FVisibilityResults& InVisibilityResults,
	POINT InMousePositionClient,
	int32 InViewportWidth,
	int32 InViewportHeight,
	FPickState& InOutPickState) const
{
	if (InViewportWidth <= 0 || InViewportHeight <= 0)
	{
		return;
	}

	if (InMousePositionClient.x < 0
		|| InMousePositionClient.y < 0
		|| InMousePositionClient.x >= InViewportWidth
		|| InMousePositionClient.y >= InViewportHeight)
	{
		return;
	}

	const FRay PickRay = BuildPickRay(InCamera, InMousePositionClient.x, InMousePositionClient.y, InViewportWidth, InViewportHeight);
	const uint64 PickStartCycles = QueryCycles64();

	const TArray<FScenePrimitiveRuntimeData>& PrimitiveRuntimeData = InScene.GetPrimitiveRuntimeData();
	FPickHit BestHit;
	BestHit.DistanceSquared = std::numeric_limits<float>::max();

	for (uint32 PrimitiveIndex : InVisibilityResults.VisiblePrimitiveIndices)
	{
		if (PrimitiveIndex >= PrimitiveRuntimeData.size())
		{
			continue;
		}

		if (IntersectRenderItem(PickRay, PrimitiveRuntimeData[PrimitiveIndex], BestHit))
		{
			BestHit.PrimitiveIndex = static_cast<int32>(PrimitiveIndex);
		}
	}


	const uint64 PickEndCycles = QueryCycles64();
	InOutPickState.LastPickTimeMs = CyclesToMilliseconds(PickStartCycles, PickEndCycles);
	InOutPickState.TotalPickTimeMs += InOutPickState.LastPickTimeMs;
	++InOutPickState.TotalPickCount;
	InOutPickState.bHit = BestHit.PrimitiveId >= 0;
	InOutPickState.SelectedPrimitiveId = BestHit.PrimitiveId;
	InOutPickState.SelectedPrimitiveIndex = BestHit.PrimitiveIndex;
	InOutPickState.HitWorldPosition = BestHit.WorldPosition;
	InOutPickState.TotalAABBCheckCount = 0;
	InOutPickState.TraversedWorldBVHNodes.clear();
}


void FPickingSystem::UpdatePickWorldBVH(
	const FScene& InScene,
	const FCamera& InCamera,
	const FVisibilityResults& InVisibilityResults,
	POINT InMousePositionClient,
	int32 InViewportWidth,
	int32 InViewportHeight,
	FPickState& InOutPickState) const
{
	if (InViewportWidth <= 0 || InViewportHeight <= 0)
	{
		return;
	}

	if (InMousePositionClient.x < 0
		|| InMousePositionClient.y < 0
		|| InMousePositionClient.x >= InViewportWidth
		|| InMousePositionClient.y >= InViewportHeight)
	{
		return;
	}

	const FRay PickRay = BuildPickRay(InCamera, InMousePositionClient.x, InMousePositionClient.y, InViewportWidth, InViewportHeight);
	const uint64 PickStartCycles = QueryCycles64();

	//const TArray<FRenderItem>& RenderItems = InScene.GetRenderItems();
	FPickHit BestHit;
	BestHit.DistanceSquared = std::numeric_limits<float>::max();
	InOutPickState.TotalAABBCheckCount = 0;
	InOutPickState.TraversedWorldBVHNodes.clear();

	IntersectWorldBVH(PickRay, InScene, BestHit, InOutPickState);

	const uint64 PickEndCycles = QueryCycles64();
	InOutPickState.LastPickTimeMsWorldBVH = CyclesToMilliseconds(PickStartCycles, PickEndCycles);
	InOutPickState.TotalPickTimeMs += InOutPickState.LastPickTimeMsWorldBVH;
	++InOutPickState.TotalPickCount;
	InOutPickState.bHit = BestHit.PrimitiveId >= 0;
	InOutPickState.SelectedPrimitiveId = BestHit.PrimitiveId;
	InOutPickState.SelectedPrimitiveIndex = BestHit.PrimitiveIndex;
	InOutPickState.HitWorldPosition = BestHit.WorldPosition;

}
