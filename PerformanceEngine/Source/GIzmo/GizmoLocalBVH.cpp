#include "Gizmo/GizmoLocalBVH.h"

#include <algorithm>

#include "Picking/PickingMath.h"

namespace
{
	struct FBuildTriangle
	{
		FVector Vertex0 = FVector::ZeroVector;
		FVector Vertex1 = FVector::ZeroVector;
		FVector Vertex2 = FVector::ZeroVector;
		FVector Centroid = FVector::ZeroVector;
	};

	struct FBuildContext
	{
		TArray<FBuildTriangle> Triangles;
		TArray<uint32> TriangleIndices;
		TArray<FBVHNode> Nodes;
		int32 UsedCount = 1;
	};

	float CalculateExtentArea(const FVector& InExtent)
	{
		return InExtent.X * InExtent.Y + InExtent.Y * InExtent.Z + InExtent.Z * InExtent.X;
	}

	void UpdateNodeBounds(FBuildContext& InOutContext, uint32 InNodeIndex)
	{
		FVector& BoundsMin = InOutContext.Nodes[InNodeIndex].BoundMin;
		FVector& BoundsMax = InOutContext.Nodes[InNodeIndex].BoundMax;

		BoundsMin = FVector(PickingMath::NoHitDistance, PickingMath::NoHitDistance, PickingMath::NoHitDistance);
		BoundsMax = FVector(-PickingMath::NoHitDistance, -PickingMath::NoHitDistance, -PickingMath::NoHitDistance);

		const int32 First = InOutContext.Nodes[InNodeIndex].LeftFirst;
		const int32 Count = InOutContext.Nodes[InNodeIndex].PrimitiveCount;
		for (int32 TriangleOffset = 0; TriangleOffset < Count; ++TriangleOffset)
		{
			const FBuildTriangle& Triangle = InOutContext.Triangles[InOutContext.TriangleIndices[First + TriangleOffset]];
			BoundsMin = FVector::Min(BoundsMin, Triangle.Vertex0);
			BoundsMin = FVector::Min(BoundsMin, Triangle.Vertex1);
			BoundsMin = FVector::Min(BoundsMin, Triangle.Vertex2);
			BoundsMax = FVector::Max(BoundsMax, Triangle.Vertex0);
			BoundsMax = FVector::Max(BoundsMax, Triangle.Vertex1);
			BoundsMax = FVector::Max(BoundsMax, Triangle.Vertex2);
		}
	}

	float EvaluateSplitCost(FBuildContext& InOutContext, FBVHNode& InNode, int32 InAxis, float InPivot)
	{
		FVector LeftBoundsMin(PickingMath::NoHitDistance, PickingMath::NoHitDistance, PickingMath::NoHitDistance);
		FVector LeftBoundsMax(-PickingMath::NoHitDistance, -PickingMath::NoHitDistance, -PickingMath::NoHitDistance);
		FVector RightBoundsMin(PickingMath::NoHitDistance, PickingMath::NoHitDistance, PickingMath::NoHitDistance);
		FVector RightBoundsMax(-PickingMath::NoHitDistance, -PickingMath::NoHitDistance, -PickingMath::NoHitDistance);

		uint32 LeftTriangleCount = 0;
		uint32 RightTriangleCount = 0;

		for (int32 TriangleOffset = 0; TriangleOffset < InNode.PrimitiveCount; ++TriangleOffset)
		{
			FBuildTriangle& Triangle = InOutContext.Triangles[InOutContext.TriangleIndices[InNode.LeftFirst + TriangleOffset]];
			if (Triangle.Centroid[InAxis] < InPivot)
			{
				++LeftTriangleCount;
				LeftBoundsMin = FVector::Min(LeftBoundsMin, Triangle.Vertex0);
				LeftBoundsMin = FVector::Min(LeftBoundsMin, Triangle.Vertex1);
				LeftBoundsMin = FVector::Min(LeftBoundsMin, Triangle.Vertex2);
				LeftBoundsMax = FVector::Max(LeftBoundsMax, Triangle.Vertex0);
				LeftBoundsMax = FVector::Max(LeftBoundsMax, Triangle.Vertex1);
				LeftBoundsMax = FVector::Max(LeftBoundsMax, Triangle.Vertex2);
			}
			else
			{
				++RightTriangleCount;
				RightBoundsMin = FVector::Min(RightBoundsMin, Triangle.Vertex0);
				RightBoundsMin = FVector::Min(RightBoundsMin, Triangle.Vertex1);
				RightBoundsMin = FVector::Min(RightBoundsMin, Triangle.Vertex2);
				RightBoundsMax = FVector::Max(RightBoundsMax, Triangle.Vertex0);
				RightBoundsMax = FVector::Max(RightBoundsMax, Triangle.Vertex1);
				RightBoundsMax = FVector::Max(RightBoundsMax, Triangle.Vertex2);
			}
		}

		const float LeftArea = CalculateExtentArea(LeftBoundsMax - LeftBoundsMin);
		const float RightArea = CalculateExtentArea(RightBoundsMax - RightBoundsMin);
		const float Cost = LeftArea * static_cast<float>(LeftTriangleCount) + RightArea * static_cast<float>(RightTriangleCount);
		return (Cost > 0.0f) ? Cost : PickingMath::NoHitDistance;
	}

	void Subdivide(FBuildContext& InOutContext, int32 InNodeIndex)
	{
		FBVHNode& Node = InOutContext.Nodes[InNodeIndex];
		if (Node.PrimitiveCount <= 4)
		{
			return;
		}

		int32 BestAxis = -1;
		float BestPivot = 0.0f;
		float BestCost = PickingMath::NoHitDistance;

		for (int32 AxisIndex = 0; AxisIndex < 3; ++AxisIndex)
		{
			for (int32 TriangleOffset = 0; TriangleOffset < Node.PrimitiveCount; ++TriangleOffset)
			{
				FBuildTriangle& Triangle = InOutContext.Triangles[InOutContext.TriangleIndices[Node.LeftFirst + TriangleOffset]];
				const float CandidatePivot = Triangle.Centroid[AxisIndex];
				const float Cost = EvaluateSplitCost(InOutContext, Node, AxisIndex, CandidatePivot);
				if (Cost < BestCost)
				{
					BestCost = Cost;
					BestPivot = CandidatePivot;
					BestAxis = AxisIndex;
				}
			}
		}

		const float CurrentCost = static_cast<float>(Node.PrimitiveCount) * CalculateExtentArea(Node.BoundMax - Node.BoundMin);
		if (BestAxis < 0 || BestCost >= CurrentCost)
		{
			return;
		}

		int32 Low = Node.LeftFirst;
		int32 High = Low + Node.PrimitiveCount - 1;
		while (Low <= High)
		{
			if (InOutContext.Triangles[InOutContext.TriangleIndices[Low]].Centroid[BestAxis] < BestPivot)
			{
				++Low;
			}
			else
			{
				std::swap(InOutContext.TriangleIndices[Low], InOutContext.TriangleIndices[High--]);
			}
		}

		const int32 LeftCount = Low - Node.LeftFirst;
		if (LeftCount == 0 || LeftCount == Node.PrimitiveCount)
		{
			return;
		}

		const int32 LeftChildIndex = ++InOutContext.UsedCount;
		const int32 RightChildIndex = ++InOutContext.UsedCount;

		InOutContext.Nodes[LeftChildIndex].LeftFirst = Node.LeftFirst;
		InOutContext.Nodes[LeftChildIndex].PrimitiveCount = LeftCount;
		InOutContext.Nodes[RightChildIndex].LeftFirst = Low;
		InOutContext.Nodes[RightChildIndex].PrimitiveCount = Node.PrimitiveCount - LeftCount;

		Node.LeftFirst = LeftChildIndex;
		Node.PrimitiveCount = 0;

		UpdateNodeBounds(InOutContext, LeftChildIndex);
		UpdateNodeBounds(InOutContext, RightChildIndex);

		Subdivide(InOutContext, LeftChildIndex);
		Subdivide(InOutContext, RightChildIndex);
	}
}

FGizmoLocalBVH BuildGizmoLocalBVH(const FGizmoMesh& InMesh)
{
	FGizmoLocalBVH Result;
	if (!InMesh.IsValid())
	{
		return Result;
	}

	FBuildContext BuildContext;
	const uint32 TriangleCount = static_cast<uint32>(InMesh.Indices.size() / 3);
	BuildContext.Triangles.reserve(TriangleCount);
	BuildContext.TriangleIndices.reserve(TriangleCount);
	BuildContext.Nodes.resize(std::max<uint32>(TriangleCount * 2, 2u));

	for (uint32 TriangleIndex = 0; TriangleIndex < TriangleCount; ++TriangleIndex)
	{
		const uint32 Index0 = InMesh.Indices[TriangleIndex * 3 + 0];
		const uint32 Index1 = InMesh.Indices[TriangleIndex * 3 + 1];
		const uint32 Index2 = InMesh.Indices[TriangleIndex * 3 + 2];
		if (Index0 >= InMesh.Vertices.size() || Index1 >= InMesh.Vertices.size() || Index2 >= InMesh.Vertices.size())
		{
			continue;
		}

		FBuildTriangle Triangle;
		Triangle.Vertex0 = InMesh.Vertices[Index0].Position;
		Triangle.Vertex1 = InMesh.Vertices[Index1].Position;
		Triangle.Vertex2 = InMesh.Vertices[Index2].Position;
		Triangle.Centroid = (Triangle.Vertex0 + Triangle.Vertex1 + Triangle.Vertex2) / 3.0f;
		BuildContext.Triangles.push_back(Triangle);
		BuildContext.TriangleIndices.push_back(static_cast<uint32>(BuildContext.Triangles.size() - 1));
	}

	if (BuildContext.Triangles.empty())
	{
		return Result;
	}

	BuildContext.Nodes[0].LeftFirst = 0;
	BuildContext.Nodes[0].PrimitiveCount = static_cast<int32>(BuildContext.Triangles.size());
	UpdateNodeBounds(BuildContext, 0);
	Subdivide(BuildContext, 0);

	BuildContext.Nodes.resize(static_cast<size_t>(BuildContext.UsedCount + 1));
	Result.SpatialData.Nodes = std::move(BuildContext.Nodes);
	Result.SpatialData.TriangleIndices = std::move(BuildContext.TriangleIndices);
	return Result;
}

bool IntersectGizmoLocalBVH(const FRay& InLocalRay, const FGizmoMesh& InMesh, const FGizmoLocalBVH& InLocalBVH, float& OutHitDistance, FVector& OutLocalHitPosition)
{
	OutHitDistance = PickingMath::NoHitDistance;
	OutLocalHitPosition = FVector::ZeroVector;

	const TArray<FBVHNode>& Nodes = InLocalBVH.SpatialData.Nodes;
	const TArray<uint32>& TriangleIndices = InLocalBVH.SpatialData.TriangleIndices;
	if (Nodes.empty() || TriangleIndices.empty() || !InMesh.IsValid())
	{
		return false;
	}

	const FVector InverseDirection(
		(InLocalRay.Direction.X != 0.0f) ? (1.0f / InLocalRay.Direction.X) : PickingMath::NoHitDistance,
		(InLocalRay.Direction.Y != 0.0f) ? (1.0f / InLocalRay.Direction.Y) : PickingMath::NoHitDistance,
		(InLocalRay.Direction.Z != 0.0f) ? (1.0f / InLocalRay.Direction.Z) : PickingMath::NoHitDistance);

	if (PickingMath::IntersectRayAabb(InLocalRay.Origin, InverseDirection, Nodes[0].BoundMin, Nodes[0].BoundMax) == PickingMath::NoHitDistance)
	{
		return false;
	}

	bool bHit = false;
	int32 NodeStack[128] = {};
	int32 StackCount = 0;
	const FBVHNode* Node = &Nodes[0];

	while (true)
	{
		if (Node->IsLeaf())
		{
			for (int32 PrimitiveOffset = 0; PrimitiveOffset < Node->PrimitiveCount; ++PrimitiveOffset)
			{
				const uint32 TriangleIndex = TriangleIndices[Node->LeftFirst + PrimitiveOffset];
				const uint32 Index0 = InMesh.Indices[TriangleIndex * 3 + 0];
				const uint32 Index1 = InMesh.Indices[TriangleIndex * 3 + 1];
				const uint32 Index2 = InMesh.Indices[TriangleIndex * 3 + 2];
				if (Index0 >= InMesh.Vertices.size() || Index1 >= InMesh.Vertices.size() || Index2 >= InMesh.Vertices.size())
				{
					continue;
				}

				float HitDistance = 0.0f;
				if (!PickingMath::IntersectRayTriangleTwoSided(
					InLocalRay,
					InMesh.Vertices[Index0].Position,
					InMesh.Vertices[Index1].Position,
					InMesh.Vertices[Index2].Position,
					HitDistance))
				{
					continue;
				}

				if (HitDistance >= OutHitDistance)
				{
					continue;
				}

				OutHitDistance = HitDistance;
				OutLocalHitPosition = InLocalRay.Origin + InLocalRay.Direction * HitDistance;
				bHit = true;
			}

			if (StackCount == 0)
			{
				break;
			}

			Node = &Nodes[NodeStack[--StackCount]];
			continue;
		}

		const FBVHNode* LeftChild = &Nodes[Node->LeftFirst];
		const FBVHNode* RightChild = &Nodes[Node->LeftFirst + 1];
		float LeftDistance = PickingMath::IntersectRayAabb(InLocalRay.Origin, InverseDirection, LeftChild->BoundMin, LeftChild->BoundMax);
		float RightDistance = PickingMath::IntersectRayAabb(InLocalRay.Origin, InverseDirection, RightChild->BoundMin, RightChild->BoundMax);

		if (LeftDistance > RightDistance)
		{
			std::swap(LeftDistance, RightDistance);
			std::swap(LeftChild, RightChild);
		}

		if (LeftDistance >= OutHitDistance)
		{
			if (StackCount == 0)
			{
				break;
			}

			Node = &Nodes[NodeStack[--StackCount]];
			continue;
		}

		Node = LeftChild;
		if (RightDistance < OutHitDistance && StackCount < static_cast<int32>(std::size(NodeStack)))
		{
			NodeStack[StackCount++] = static_cast<int32>(RightChild - &Nodes[0]);
		}
	}

	return bHit;
}
