#include "DynmaicBVHTree.h"
#include "Debug/DebugDrawManager.h"
#include <queue>
#include <limits>

bool FAABB::RayIntersect(const FVector& Origin, const FVector& Direction, float& OutTMin) const
{
	constexpr float Epsilon = 1e-8f;
	float TMin = 0.0f;
	float TMax = std::numeric_limits<float>::max();

	// X
	if (std::abs(Direction.X) < Epsilon)
	{
		if (Origin.X < Min.X || Origin.X > Max.X) return false;
	}
	else
	{
		float T1 = (Min.X - Origin.X) / Direction.X;
		float T2 = (Max.X - Origin.X) / Direction.X;
		if (T1 > T2) std::swap(T1, T2);
		TMin = std::max(TMin, T1);
		TMax = std::min(TMax, T2);
		if (TMin > TMax) return false;
	}

	// Y
	if (std::abs(Direction.Y) < Epsilon)
	{
		if (Origin.Y < Min.Y || Origin.Y > Max.Y) return false;
	}
	else
	{
		float T1 = (Min.Y - Origin.Y) / Direction.Y;
		float T2 = (Max.Y - Origin.Y) / Direction.Y;
		if (T1 > T2) std::swap(T1, T2);
		TMin = std::max(TMin, T1);
		TMax = std::min(TMax, T2);
		if (TMin > TMax) return false;
	}

	// Z
	if (std::abs(Direction.Z) < Epsilon)
	{
		if (Origin.Z < Min.Z || Origin.Z > Max.Z) return false;
	}
	else
	{
		float T1 = (Min.Z - Origin.Z) / Direction.Z;
		float T2 = (Max.Z - Origin.Z) / Direction.Z;
		if (T1 > T2) std::swap(T1, T2);
		TMin = std::max(TMin, T1);
		TMax = std::min(TMax, T2);
		if (TMin > TMax) return false;
	}

	OutTMin = TMin;
	return TMax >= 0.0f;
}

float FAABB::SurfaceArea() const
{
	float ExtentX = Max.X - Min.X;
	float ExtentY = Max.Y - Min.Y;
	float ExtentZ = Max.Z - Min.Z;
	return 2.0f * (ExtentX * ExtentY + ExtentY * ExtentZ + ExtentZ * ExtentX);
}

int32 FDynamicBVHTree::InsertLeaf(UPrimitiveComponent* Component)
{
	if (!Component) return -1;
	if (ComponentToIndexMap.find(Component) != ComponentToIndexMap.end()) return -1;


	FAABB ComponentBound;
	FBoxSphereBounds BSBound = Component->GetWorldBounds();
	ComponentBound.Max = BSBound.Center + BSBound.BoxExtent;
	ComponentBound.Min = BSBound.Center - BSBound.BoxExtent;
	if (RootIdx == -1)
	{
		RootIdx = GetFreeIndex();
		Nodes[RootIdx].Bound = ComponentBound;
		Nodes[RootIdx].Component = Component;
		Nodes[RootIdx].BF = 0;
		ComponentToIndexMap.insert({ Component,RootIdx });
		return RootIdx;
	}

	int32 BestSiblingIdx = FindBestSibling(ComponentBound);

	int32 NewParentIdx = GetFreeIndex();
	int32 NewLeafIdx = GetFreeIndex();

	int32 OldParentIdx = Nodes[BestSiblingIdx].ParentIndex;

	Nodes[NewParentIdx].LeftChildIndex = BestSiblingIdx;
	Nodes[NewParentIdx].RightChildIndex = NewLeafIdx;
	Nodes[NewParentIdx].ParentIndex = OldParentIdx;
	Nodes[NewParentIdx].Bound = MergeBounds(Nodes[BestSiblingIdx].Bound, ComponentBound);
	//Nodes[NewParentIdx].BF = Nodes[BestSiblingIdx].BF - 

	Nodes[BestSiblingIdx].ParentIndex = NewParentIdx;

	Nodes[NewLeafIdx].ParentIndex = NewParentIdx;
	Nodes[NewLeafIdx].Bound = ComponentBound;
	Nodes[NewLeafIdx].Component = Component;
	ComponentToIndexMap.insert({ Component,NewLeafIdx });

	if (OldParentIdx != -1)
	{
		if (Nodes[OldParentIdx].LeftChildIndex == BestSiblingIdx)
			Nodes[OldParentIdx].LeftChildIndex = NewParentIdx;
		else
			Nodes[OldParentIdx].RightChildIndex = NewParentIdx;
	}
	else
	{
		RootIdx = NewParentIdx;
	}

	RefitUpward(NewParentIdx);
	return NewLeafIdx;
}

void FDynamicBVHTree::RemoveLeaf(UPrimitiveComponent* Component)
{
	auto It = ComponentToIndexMap.find(Component);
	if (It == ComponentToIndexMap.end()) return;

	int32 LeafIdx = It->second;
	int32 ParentIdx = Nodes[LeafIdx].ParentIndex;
	ComponentToIndexMap.erase(It);

	if (ParentIdx == -1)
	{
		ReleaseIndex(LeafIdx);
		RootIdx = -1;
		return;
	}

	int32 SiblingIdx = Nodes[ParentIdx].LeftChildIndex == LeafIdx
		? Nodes[ParentIdx].RightChildIndex
		: Nodes[ParentIdx].LeftChildIndex;

	int32 GrandParentIdx = Nodes[ParentIdx].ParentIndex;
	if (GrandParentIdx == -1)
	{
		Nodes[SiblingIdx].ParentIndex = -1;
		RootIdx = SiblingIdx;
	}
	else
	{
		if (Nodes[GrandParentIdx].LeftChildIndex == ParentIdx)
		{
			Nodes[GrandParentIdx].LeftChildIndex = SiblingIdx;
		}
		else
		{
			Nodes[GrandParentIdx].RightChildIndex = SiblingIdx;
		}
		Nodes[SiblingIdx].ParentIndex = GrandParentIdx;
	}

	ReleaseIndex(LeafIdx);
	ReleaseIndex(ParentIdx);

	if (GrandParentIdx != -1) RefitUpward(SiblingIdx);
}

void FDynamicBVHTree::MoveLeaf(UPrimitiveComponent* Component)
{
	if (!Component) return;
	RemoveLeaf(Component);
	InsertLeaf(Component);
}

void FDynamicBVHTree::QueryRay(const FVector& Origin, const FVector& Direction, TArray<UPrimitiveComponent*>& OutCandidates) const
{
	if (RootIdx == -1) return;

	TArray<int32> Stack;
	Stack.push_back(RootIdx);

	while (!Stack.empty())
	{
		int32 Idx = Stack.back();
		Stack.pop_back();

		float T;
		if (!Nodes[Idx].Bound.RayIntersect(Origin, Direction, T)) continue;

		if (Nodes[Idx].IsLeaf())
		{
			OutCandidates.push_back(Nodes[Idx].Component);
		}
		else
		{
			if (Nodes[Idx].LeftChildIndex != -1)
				Stack.push_back(Nodes[Idx].LeftChildIndex);
			if (Nodes[Idx].RightChildIndex != -1)
				Stack.push_back(Nodes[Idx].RightChildIndex);
		}
	}
}

void FDynamicBVHTree::Clear()
{
	Nodes.clear();
	ComponentToIndexMap.clear();
	FreeList.clear();
	RootIdx = -1;
}



int32 FDynamicBVHTree::GetFreeIndex()
{
	int32 Idx = -1;
	if (FreeList.empty())
	{
		Idx = Nodes.size();
		Nodes.emplace_back();
		InitNode(Idx);
		return Idx;
	}
	Idx = FreeList.back();
	InitNode(Idx);
	FreeList.pop_back();
	return Idx;
}

void FDynamicBVHTree::InitNode(int32 Index)
{
	DynamicBVHNode& Node = Nodes[Index];
	Node.Bound = FAABB();
	Node.Component = nullptr;
	Node.LeftChildIndex = -1;
	Node.RightChildIndex = -1;
	Node.ParentIndex = -1;
	Node.BF = 0;
}

void FDynamicBVHTree::ReleaseIndex(int32 Index)
{
	InitNode(Index);
	FreeList.push_back(Index);
}

int32 FDynamicBVHTree::FindBestSibling(const FAABB& NewBound) const
{
	std::priority_queue<std::pair<float, int32>, std::vector<std::pair<float, int32>>, std::greater<>> Queue;
	Queue.emplace(0.0f, RootIdx);

	float BestCost = std::numeric_limits<float>::max();
	int32 BestIdx = -1;
	while (!Queue.empty())
	{
		auto [InheritedCost, Idx] = Queue.top();
		Queue.pop();

		if (InheritedCost + NewBound.SurfaceArea() >= BestCost) continue;

		float DirectCost = MergeBounds(NewBound, Nodes[Idx].Bound).SurfaceArea();
		float TotalCost = DirectCost + InheritedCost;

		if (TotalCost < BestCost)
		{
			BestCost = TotalCost;
			BestIdx = Idx;
		}

		float LowerBound = InheritedCost + NewBound.SurfaceArea();
		if (LowerBound < BestCost && !Nodes[Idx].IsLeaf())
		{
			float ChildInheritanceCost = InheritedCost + (DirectCost - Nodes[Idx].Bound.SurfaceArea());
			Queue.push(std::make_pair(ChildInheritanceCost, Nodes[Idx].LeftChildIndex));
			Queue.push(std::make_pair(ChildInheritanceCost, Nodes[Idx].RightChildIndex));
		}
	}
	return BestIdx;
}

FAABB FDynamicBVHTree::MergeBounds(const FAABB& A, const FAABB& B) const
{
	FAABB Merged;

	Merged.Min.X = std::min(A.Min.X, B.Min.X);
	Merged.Min.Y = std::min(A.Min.Y, B.Min.Y);
	Merged.Min.Z = std::min(A.Min.Z, B.Min.Z);

	Merged.Max.X = std::max(A.Max.X, B.Max.X);
	Merged.Max.Y = std::max(A.Max.Y, B.Max.Y);
	Merged.Max.Z = std::max(A.Max.Z, B.Max.Z);

	return Merged;
}

void FDynamicBVHTree::RefitUpward(int32 Index)
{
	int32 ParentIdx = Nodes[Index].ParentIndex;
	while (ParentIdx != -1)
	{
		const DynamicBVHNode& ParentNode = Nodes[ParentIdx];
		Nodes[ParentIdx].Bound = MergeBounds(Nodes[ParentNode.LeftChildIndex].Bound, Nodes[ParentNode.RightChildIndex].Bound);
		ParentIdx = Nodes[ParentIdx].ParentIndex;
	}
}

void FDynamicBVHTree::Visualize(FDebugDrawManager& DebugDrawer) const
{
	if (RootIdx == -1) return;

	TArray<int32> Stack;
	Stack.push_back(RootIdx);

	while (!Stack.empty())
	{
		int32 Idx = Stack.back();
		Stack.pop_back();

		const DynamicBVHNode& Node = Nodes[Idx];

		FVector Center = (Node.Bound.Min + Node.Bound.Max) * 0.5f;
		FVector Extent = (Node.Bound.Max - Node.Bound.Min) * 0.5f;

		// Leaf nodes are Green, internal nodes are Yellow.
		FVector4 Color = Node.IsLeaf() ? FVector4(0.0f, 1.0f, 0.0f, 1.0f) : FVector4(1.0f, 1.0f, 0.0f, 1.0f);

		DebugDrawer.DrawCube(Center, Extent, Color);

		if (Node.LeftChildIndex != -1)
			Stack.push_back(Node.LeftChildIndex);
		if (Node.RightChildIndex != -1)
			Stack.push_back(Node.RightChildIndex);
	}
}

