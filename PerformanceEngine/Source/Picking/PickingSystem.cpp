#include "Picking/PickingSystem.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "Camera/Camera.h"
#include "Scene/Scene.h"
#include "StaticMesh/StaticMesh.h"
#include "Visibility/VisibilitySystem.h"
#include "Types/Stack.h"

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

	bool IntersectRayAabb(const FRay& InRay, const FVector& InBoundsMin, const FVector& InBoundsMax)
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
					return false;
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
				return false;
			}
		}

		return true;
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

		TStack<int32> NodeStack;
		NodeStack.push(0);

		while (!NodeStack.empty())
		{
			const FBVHNode& Node = Nodes[NodeStack.top()]; 
			NodeStack.pop();

			if (!IntersectRayAabb(LocalRay, Node.BoundMin, Node.BoundMax))
			{
				continue;
			}

			if (Node.IsLeaf())
			{
				for (int32 i = 0; i < Node.PrimitiveCount; ++i)
				{
					uint32 TriangleIndex = TriangleIndices[Node.LeftFirst + i];
					const FVector& Vertex0 = Vertices[TriangleIndex].Position;
					const FVector& Vertex1 = Vertices[TriangleIndex + 1].Position;
					const FVector& Vertex2 = Vertices[TriangleIndex + 2].Position;

					float LocalT = 0.0f;
					FVector LocalHitPos;
					if (!IntersectRayTriangle(LocalRay, Vertex0, Vertex1, Vertex2, LocalT, LocalHitPos)) continue;

					const FVector WorldHitPos = InRenderItem.Transform.TransformPosition(LocalHitPos);
					const float DistSq = FVector::DistSquared(InRay.Origin, WorldHitPos);
					if (DistSq >= InOutBestHit.DistanceSquared) continue;

					InOutBestHit.DistanceSquared = DistSq;
					InOutBestHit.PrimitiveId = InRenderItem.PrimitiveId;
					InOutBestHit.WorldPosition = WorldHitPos;
					bHit = true;
				}
			}
			else
			{
				NodeStack.push(Node.LeftFirst);
				NodeStack.push(Node.LeftFirst + 1);
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
