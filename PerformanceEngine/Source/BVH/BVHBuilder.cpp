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

	void Subdivide(FBVHBuildContext& Ctx, int32 InNodeIndex)
	{
		FBVHNode& CurNode = Ctx.Nodes[InNodeIndex];

		if (CurNode.PrimitiveCount <= 4)
		{
			return;
		}

		FVector Extent = CurNode.BoundMax - CurNode.BoundMin;

		int32 Axis = 0;
		if (Extent.Y > Extent.X) Axis = 1;
		if (Extent.Z > Extent[Axis]) Axis = 2;

		float SplitPivot = CurNode.BoundMin[Axis] + Extent[Axis] * 0.5f;

		int lo = CurNode.LeftFirst;
		int hi = lo + CurNode.PrimitiveCount - 1;
		while (lo <= hi)
		{
			if (Ctx.Triangles[Ctx.TriangleIndices[lo]].Centroid[Axis] < SplitPivot)
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
	const uint32 TriangleCount = InStaticMesh->GetVertexCount() / 3;

	Ctx.Triangles.reserve(TriangleCount);
	Ctx.TriangleIndices.reserve(TriangleCount);

	for (uint32 j = 0; j < TriangleCount; ++j)
	{
		FBVHTriangle Tri;
		Tri.Vertex0  = Vertices[j * 3 + 0].Position;
		Tri.Vertex1  = Vertices[j * 3 + 1].Position;
		Tri.Vertex2  = Vertices[j * 3 + 2].Position;
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
