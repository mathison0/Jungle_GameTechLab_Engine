#pragma once

#include "Math/Vector.h"
#include "Types/Array.h"
#include "Types/PlatformTypes.h"

/**
 * BVH 트리의 노드 하나.
 * - 내부 노드: PrimitiveCount == 0, LeftFirst = 왼쪽 자식 인덱스
 * - 리프 노드: PrimitiveCount  > 0, LeftFirst = TriangleIndices 시작 오프셋
 */

struct alignas(16) FBVHNode
{
	// simd 연산을 사용하다 보니 union을 썼는데 내부적으로 사용자 정의 타입이 존재하면 기본 생성자가 자동으로 삭제됨
	FBVHNode() {};

	union
	{
		struct { FVector BoundMin; int32 LeftFirst; };
		__m128 aabbMin4;
	};

	union
	{
		struct { FVector BoundMax; int32 PrimitiveCount; };
		__m128 aabbMax4;
	};

	bool IsLeaf() const { return PrimitiveCount > 0; }
};

struct FBVHSpatialData 
{
	TArray<FBVHNode> Nodes;
	TArray<uint32>   TriangleIndices;
};
