#pragma once
#include "Math/Vector.h"

struct AABBNode
{
	FVector Min;
	FVector Max;
	int ParentIndex = -1;
	int LeftChildIndex = -1;
	int RightChildIndex = -1;
	int PrimitiveIndex = -1;  // 리프 노드일 때 RenderItems 배열 인덱스, 내부 노드는 -1
	bool IsLeaf() const
	{
		return PrimitiveIndex != -1;
	}
};