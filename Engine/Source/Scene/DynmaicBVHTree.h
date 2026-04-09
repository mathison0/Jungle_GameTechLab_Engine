#pragma once
#include "Math/Vector.h"
#include "Types/Array.h"
#include "Types/Map.h"
#include "Component/PrimitiveComponent.h"
struct FAABB
{
	FVector Min;
	FVector Max;
	float SurfaceArea() const;
	bool RayIntersect(const FVector& Origin, const FVector& Direction, float& OutTMin) const;
};

struct DynamicBVHNode
{
	FAABB Bound;
	int32 LeftChildIndex = -1;
	int32 RightChildIndex = -1;
	int32 ParentIndex = -1;
	int32 BF = 0;
	UPrimitiveComponent* Component = nullptr; // 리프 노드인 경우에만 유효
	bool IsLeaf() const
	{
		return Component != nullptr;
	}
};


class ENGINE_API FDynamicBVHTree
{
public:
	int32 InsertLeaf(UPrimitiveComponent* Component);
	void RemoveLeaf(UPrimitiveComponent* Component);
	void MoveLeaf(UPrimitiveComponent* Component);
	void QueryRay(const FVector& Origin, const FVector& Direction, TArray<UPrimitiveComponent*>& OutCandidates) const;
	void Clear();

	void Visualize(class FDebugDrawManager& DebugDrawer) const;

private:
	int32 GetFreeIndex();
	void InitNode(int32 Index);
	void ReleaseIndex(int32 Index);
	int32 FindBestSibling(const FAABB& NewBound) const;
	FAABB MergeBounds(const FAABB& A, const FAABB& B) const;
	void RefitUpward(int32 Index);

	void Rotate(int32 Index);

	float SwapCost(int32 Idx1, int32 Idx2);

private:
	TArray<DynamicBVHNode> Nodes;
	TMap<UPrimitiveComponent*, int32> ComponentToIndexMap;
	TArray<int32> FreeList;
	int32 RootIdx = -1;
};