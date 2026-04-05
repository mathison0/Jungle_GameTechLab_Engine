#pragma once

#include <Windows.h>

#include "Picking/AABBNode.h"
#include "Scene/SceneTypes.h"
#include "Types/Array.h"
#include "Types/PlatformTypes.h"

class FCamera;
class FScene;
struct FVisibilityResults;

struct FPickState
{
	int32 SelectedPrimitiveId = -1;
	int32 SelectedPrimitiveIndex = -1;
	bool bHit = false;
	FVector HitWorldPosition = FVector::ZeroVector;
	double LastPickTimeMs = 0.0;
	double TotalPickTimeMs = 0.0;
	uint64 TotalPickCount = 0;
	uint64 TotalAABBCheckCount = 0;
	double LastPickTimeMsWorldBVH = 0.0;
	TArray<AABBNode> TraversedWorldBVHNodes;
};

class FPickingSystem
{
public:
	void Reset();
	void UpdatePick(
		const FScene& InScene,
		const FCamera& InCamera,
		const FVisibilityResults& InVisibilityResults,
		POINT InMousePositionClient,
		int32 InViewportWidth,
		int32 InViewportHeight,
		FPickState& InOutPickState) const;

	void UpdatePickWorldBVH(
		const FScene& InScene,
		const FCamera& InCamera,
		const FVisibilityResults& InVisibilityResults,
		POINT InMousePositionClient,
		int32 InViewportWidth,
		int32 InViewportHeight,
		FPickState& InOutPickState) const;
};
