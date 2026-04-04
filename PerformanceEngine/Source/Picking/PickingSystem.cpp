#include "Picking/PickingSystem.h"

#include <algorithm>
#include <cmath>
#include <limits>

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
	float IntersectRayAabb(const FRay& InRay, const FVector& InBoundsMin, const FVector& InBoundsMax)
	{
		float TMin = 0.0f;
		float TMax = std::numeric_limits<float>::max();

		for (int32 AxisIndex = 0; AxisIndex < 3; ++AxisIndex)
		{
			const float Origin = InRay.Origin[AxisIndex];
			const float Direction = InRay.Direction[AxisIndex];
			const float BoundsMin = InBoundsMin[AxisIndex];
			const float BoundsMax = InBoundsMax[AxisIndex];

			if (std::fabs(Direction) < 1.e-8f)
			{
				if (Origin < BoundsMin || Origin > BoundsMax)
				{
					return 1e30f;
				}
				continue;
			}

			const float InverseDirection = 1.0f / Direction;
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
		if (std::fabs(Determinant) < 1.e-8f)
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

	bool IntersectRenderItem(const FRay& InRay, const FRenderItem& InRenderItem, FPickHit& InOutBestHit)
	{
		if (!InRenderItem.StaticMesh || !InRenderItem.StaticMesh->IsValid())
		{
			return false;
		}

		const FBVHSpatialData* SpatialData =
			static_cast<const FBVHSpatialData*>(InRenderItem.StaticMesh->GetSpatialData().get());

		if (!SpatialData || SpatialData->Nodes.empty()) return false;

		const TArray<FBVHNode>& Nodes = SpatialData->Nodes;
		const TArray<uint32>& TriangleIndices = SpatialData->TriangleIndices;
		const TArray<FStaticMeshVertex>& Vertices = InRenderItem.StaticMesh->GetVertices();


		bool bHit = { false };

		FRay LocalRay;
		LocalRay.Origin = InRenderItem.Transform.InverseTransformPosition(InRay.Origin);
		LocalRay.Direction = InRenderItem.Transform.InverseTransformVector(InRay.Direction).GetSafeNormal();

		// 루트 AABB miss면 즉시 탈출
		if (IntersectRayAabb(LocalRay, Nodes[0].BoundMin, Nodes[0].BoundMax) == 1e30f)
		{
			return false;
		}

		float BestLocalT = 1e30f;

		// 힙 할당 없는 고정 배열 스택 (BVH 깊이는 64 초과 불가)
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
					const FVector& Vertex0 = Vertices[TriangleIndex * 3 + 0].Position;
					const FVector& Vertex1 = Vertices[TriangleIndex * 3 + 1].Position;
					const FVector& Vertex2 = Vertices[TriangleIndex * 3 + 2].Position;

					float LocalT = 0.0f;
					FVector LocalHitPos;
					if (!IntersectRayTriangle(LocalRay, Vertex0, Vertex1, Vertex2, LocalT, LocalHitPos)) continue;
					if (LocalT >= BestLocalT) continue;

					const FVector WorldHitPos = InRenderItem.Transform.TransformPosition(LocalHitPos);
					const float DistSq = FVector::DistSquared(InRay.Origin, WorldHitPos);
					if (DistSq >= InOutBestHit.DistanceSquared) continue;

					BestLocalT = LocalT;
					InOutBestHit.DistanceSquared = DistSq;
					InOutBestHit.PrimitiveId = InRenderItem.PrimitiveId;
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
			float Dist1 = IntersectRayAabb(LocalRay, Child1->BoundMin, Child1->BoundMax);
			float Dist2 = IntersectRayAabb(LocalRay, Child2->BoundMin, Child2->BoundMax);

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

	const TArray<FRenderItem>& RenderItems = InScene.GetRenderItems();
	FPickHit BestHit;
	BestHit.DistanceSquared = std::numeric_limits<float>::max();

	for (uint32 PrimitiveIndex : InVisibilityResults.VisiblePrimitiveIndices)
	{
		if (PrimitiveIndex >= RenderItems.size())
		{
			continue;
		}

		if (IntersectRenderItem(PickRay, RenderItems[PrimitiveIndex], BestHit))
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
}
