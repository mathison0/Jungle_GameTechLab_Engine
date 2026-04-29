#pragma once

#include "Render/Collector/RenderCollectionStats.h"
#include "Render/Scene/RenderBus.h"

class ULightComponent;
class USpotLightComponent;
class UWorld;
struct FFrustum;

class FLightRenderCollector
{
public:
	void Collect(UWorld* World, FRenderBus& RenderBus, FRenderCollectionStats& LastStats, const FFrustum* ViewFrustum = nullptr);

private:
	struct FSpotShadowCandidate
	{
		FRenderLight RenderLight = {};
		const ULightComponent* LightComponent = nullptr;
		const USpotLightComponent* SpotLight = nullptr;

		FVector LightDirection = FVector::ZeroVector;

		float RequestedResolution = 0.0f;
		uint32 RequestedTileSize = 0;
		float PriorityScore = 0.0f;
	};

	FRenderCollectionStats* CurrentStats = nullptr;

	FRenderCollectionStats& GetStats() { return *CurrentStats; }

	void CollectAmbientLight(FRenderLight& RenderLight, FRenderBus& RenderBus);
	void CollectDirectionalLight(const ULightComponent* LightComponent, FRenderLight& RenderLight, FRenderBus& RenderBus);
	void CollectPointLight(const ULightComponent* LightComponent, FRenderLight& RenderLight, FRenderBus& RenderBus, const FFrustum* ViewFrustum, int32& NextPointShadowIndex);
	void CollectSpotLight(const ULightComponent* LightComponent, FRenderLight& RenderLight, FRenderBus& RenderBus, const FFrustum* ViewFrustum, TArray<FSpotShadowCandidate>& SpotShadowCandidates);
	void AllocateSpotShadowCandidates(TArray<FSpotShadowCandidate>& SpotShadowCandidates, FRenderBus& RenderBus, int32& NextSpotShadowIndex);
};
