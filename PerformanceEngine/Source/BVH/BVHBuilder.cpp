#include "BVHBuilder.h"
#include "BVHTypes.h"

#include "StaticMesh/StaticMesh.h"

namespace
{
	struct FBVHTriangle
	{
		FVector Vertex0;
		FVector Vertex1;
		FVector Vertex2;
		FVector Centroid;
	};

	struct FBVHBuildContext
	{
		TArray<FBVHTriangle> Triangles;
		TArray<uint32>       TriangleIndices;
		TArray<FBVHNode>     Nodes;
		int32                UsedCount = 1;
	};

	auto CalculateExtentArea = [](FVector Extent) -> float
		{
			return Extent.X * Extent.Y + Extent.Y * Extent.Z + Extent.Z * Extent.X;
		};

	void UpdateBVHNodeBound(FBVHBuildContext& Ctx, uint32 NodeIndex)
	{
		FVector& AABBMin = Ctx.Nodes[NodeIndex].BoundMin;
		FVector& AABBMax = Ctx.Nodes[NodeIndex].BoundMax;

		AABBMin = FVector{ 1e30f,  1e30f,  1e30f };
		AABBMax = FVector{ -1e30f, -1e30f, -1e30f };

		const int32 First = Ctx.Nodes[NodeIndex].LeftFirst;
		const int32 Count = Ctx.Nodes[NodeIndex].PrimitiveCount;
		for (int32 i = 0; i < Count; ++i)
		{
			const FBVHTriangle& Triangle = Ctx.Triangles[Ctx.TriangleIndices[First + i]];
			AABBMin = FVector::Min(AABBMin, Triangle.Vertex0);
			AABBMin = FVector::Min(AABBMin, Triangle.Vertex1);
			AABBMin = FVector::Min(AABBMin, Triangle.Vertex2);
			AABBMax = FVector::Max(AABBMax, Triangle.Vertex0);
			AABBMax = FVector::Max(AABBMax, Triangle.Vertex1);
			AABBMax = FVector::Max(AABBMax, Triangle.Vertex2);
		}
	}

	float EvaluateSAH( FBVHBuildContext& InCtx, FBVHNode& InNode, int32 InAxis, float InPostion)
	{
		FVector LeftBoxMin = { 1e30f,  1e30f,  1e30f };
		FVector RightBoxMin = { 1e30f,  1e30f,  1e30f };

		FVector LeftBoxMax = { -1e30f, -1e30f, -1e30f };
		FVector RightBoxMax = { -1e30f, -1e30f, -1e30f };

		uint32 LeftTriangleCount = 0;
		uint32 RightTriangleCount = 0;

		for (int i = 0; i < InNode.PrimitiveCount; ++i)
		{
			FBVHTriangle& Triangle = InCtx.Triangles[InCtx.TriangleIndices[InNode.LeftFirst + i]];

			if (Triangle.Centroid[InAxis] < InPostion)
			{
				++LeftTriangleCount;
				LeftBoxMin = FVector::Min(LeftBoxMin, Triangle.Vertex0);
				LeftBoxMin = FVector::Min(LeftBoxMin, Triangle.Vertex1);
				LeftBoxMin = FVector::Min(LeftBoxMin, Triangle.Vertex2);
				LeftBoxMax = FVector::Max(LeftBoxMax, Triangle.Vertex0);
				LeftBoxMax = FVector::Max(LeftBoxMax, Triangle.Vertex1);
				LeftBoxMax = FVector::Max(LeftBoxMax, Triangle.Vertex2);
			}
			else
			{
				++RightTriangleCount;
				RightBoxMin = FVector::Min(RightBoxMin, Triangle.Vertex0);
				RightBoxMin = FVector::Min(RightBoxMin, Triangle.Vertex1);
				RightBoxMin = FVector::Min(RightBoxMin, Triangle.Vertex2);
				RightBoxMax = FVector::Max(RightBoxMax, Triangle.Vertex0);
				RightBoxMax = FVector::Max(RightBoxMax, Triangle.Vertex1);
				RightBoxMax = FVector::Max(RightBoxMax, Triangle.Vertex2);
			}
		}
		FVector LeftBoxExtent = LeftBoxMax - LeftBoxMin;
		FVector RightBoxExtent = RightBoxMax - RightBoxMin;

		float LeftBoxArea = CalculateExtentArea(LeftBoxExtent);
		float RightBoxArea = CalculateExtentArea(RightBoxExtent);

		float TotalCost = LeftBoxArea * LeftTriangleCount + RightBoxArea * RightTriangleCount;

		return (TotalCost > 0) ? (TotalCost) : (1e30f);
	}
	void Subdivide(FBVHBuildContext& Ctx, int32 InNodeIndex)
	{
		FBVHNode& CurNode = Ctx.Nodes[InNodeIndex];

		int32 BestAxis = -1;
		float BestPivot = -1;
		float BestCost = 1e30f;


		for (int32 Axis = 0; Axis < 3; ++Axis)
		{
			for (int32 i = 0; i < CurNode.PrimitiveCount; ++i)
			{
				FBVHTriangle& Triangle = Ctx.Triangles[Ctx.TriangleIndices[CurNode.LeftFirst + i]];

				float CandiatePosition = Triangle.Centroid[Axis];
				float Cost = EvaluateSAH(Ctx, CurNode, Axis, CandiatePosition);
				if (Cost < BestCost)
				{
					BestCost = Cost ;
					BestPivot = CandiatePosition;
					BestAxis = Axis;
				}
			}
		}

		FVector CurrentExtent = CurNode.BoundMax - CurNode.BoundMin;
		float CurrentArea = CurrentExtent.X * CurrentExtent.Y + CurrentExtent.Y * CurrentExtent.Z + CurrentExtent.Z * CurrentExtent.X;
		// 휴리스틱을 삼각형 * 표면적으로 판단
		float CurrentCost = CurNode.PrimitiveCount * CurrentArea;
		if (BestCost > CurrentCost)
		{
			return;
		}

		int32 lo = CurNode.LeftFirst;
		int32 hi = lo + CurNode.PrimitiveCount - 1;
		while (lo <= hi)
		{
			if (Ctx.Triangles[Ctx.TriangleIndices[lo]].Centroid[BestAxis] < BestPivot)
			{
				++lo;
			}
			else
			{
				std::swap(Ctx.TriangleIndices[lo], Ctx.TriangleIndices[hi--]);
			}
		}

		int32 LeftCount = lo - CurNode.LeftFirst;
		if (LeftCount == 0 || LeftCount == CurNode.PrimitiveCount)
		{
			return;
		}

		int32 LeftChildIndex  = ++Ctx.UsedCount;
		int32 RightChildIndex = ++Ctx.UsedCount;

		Ctx.Nodes[LeftChildIndex].LeftFirst      = CurNode.LeftFirst;
		Ctx.Nodes[LeftChildIndex].PrimitiveCount = LeftCount;

		Ctx.Nodes[RightChildIndex].LeftFirst      = lo;
		Ctx.Nodes[RightChildIndex].PrimitiveCount = CurNode.PrimitiveCount - LeftCount;

		CurNode.LeftFirst      = LeftChildIndex;
		CurNode.PrimitiveCount = 0;

		UpdateBVHNodeBound(Ctx, LeftChildIndex);
		UpdateBVHNodeBound(Ctx, RightChildIndex);

		Subdivide(Ctx, LeftChildIndex);
		Subdivide(Ctx, RightChildIndex);
	}
}

FBVHSpatialData FBVHBuilder::BuildBVH(FStaticMesh* InStaticMesh)
{
	FBVHBuildContext Ctx;

	const TArray<FStaticMeshVertex>& Vertices = InStaticMesh->GetVertices();
	const TArray<uint32>& Indices = InStaticMesh->GetIndices();
	const uint32 TriangleCount = InStaticMesh->GetIndexCount() / 3;

	Ctx.Triangles.reserve(TriangleCount);
	Ctx.TriangleIndices.reserve(TriangleCount);

	for (uint32 j = 0; j < TriangleCount; ++j)
	{
		FBVHTriangle Tri;
		Tri.Vertex0  = Vertices[Indices[j * 3 + 0]].Position;
		Tri.Vertex1  = Vertices[Indices[j * 3 + 1]].Position;
		Tri.Vertex2  = Vertices[Indices[j * 3 + 2]].Position;
		Tri.Centroid = (Tri.Vertex0 + Tri.Vertex1 + Tri.Vertex2) * 0.3333f;

		Ctx.TriangleIndices.push_back(j);
		Ctx.Triangles.push_back(Tri);
	}

	Ctx.Nodes.resize(TriangleCount * 2);

	constexpr int32 RootNodeIdx = 0;
	Ctx.Nodes[RootNodeIdx].LeftFirst      = 0;
	Ctx.Nodes[RootNodeIdx].PrimitiveCount = static_cast<int32>(Ctx.Triangles.size());

	UpdateBVHNodeBound(Ctx, RootNodeIdx);
	Subdivide(Ctx, RootNodeIdx);

	FBVHSpatialData Result;
	Result.Nodes           = std::move(Ctx.Nodes);
	Result.TriangleIndices = std::move(Ctx.TriangleIndices);
	return Result;
}
