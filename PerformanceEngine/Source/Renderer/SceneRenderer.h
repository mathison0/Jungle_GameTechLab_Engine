#pragma once

#include <memory>

#include "Graphics/D3D11/D3D11Common.h"
#include "Types/Array.h"
#include "Types/PlatformTypes.h"

class FCamera;
class FD3D11RHI;
class FScene;
struct FOcclusionTimingStats;
struct FPickState;
struct FVisibilityFrameInput;
struct FVisibilityResults;

class FSceneRenderer
{
public:
	struct FResources;

	FSceneRenderer();
	~FSceneRenderer();

	bool Initialize(FD3D11RHI& InRHI);
	void Shutdown();
	void InvalidateDelayedVisibility();
	bool ResolveGpuVisibility(
		const FD3D11RHI& InRHI,
		const FCamera& InCamera,
		const FVisibilityFrameInput& InVisibilityFrameInput,
		TArray<uint32>& OutVisibleClusterIndices,
		FOcclusionTimingStats& OutOcclusionTimings,
		FVisibilityFrameInput& OutResolvedFrameInput,
		bool& OutResolvedDelayedResult,
		bool& OutHasPendingReadback);
	bool RenderVisibleScene(const FD3D11RHI& InRHI, const FScene& InScene, const FCamera& InCamera, const FVisibilityResults& InVisibilityResults, const FPickState& InPickState);

private:
	std::unique_ptr<FResources> Resources;
};
