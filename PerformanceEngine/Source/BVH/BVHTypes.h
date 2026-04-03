#pragma once

#include "Math/Vector.h"
#include "Types/Array.h"
#include "Types/PlatformTypes.h"

/**
 * BVH 트리의 노드 하나.
 * - 내부 노드: PrimitiveCount == 0, LeftFirst = 왼쪽 자식 인덱스
 * - 리프 노드: PrimitiveCount  > 0, LeftFirst = TriangleIndices 시작 오프셋
 */
struct FBVHNode
{
	FVector  BoundMin;
	FVector	 BoundMax;
	int32	 LeftFirst      = -1;
	int32 	 PrimitiveCount =  0;
	bool IsLeaf() const { return PrimitiveCount > 0; }
};

struct FBVHSpatialData 
{
	TArray<FBVHNode> Nodes;
	TArray<uint32>   TriangleIndices;
};
