#pragma once
#include "Core/CoreTypes.h"
#include "Render/Pipeline/RenderBus.h"

class FPrimitiveProxy;
class AActor;

class FWorldRenderProxy
{
public:
	void AddProxy(FPrimitiveProxy* Proxy);
	void RemoveProxy(FPrimitiveProxy* Proxy);

	void CollectWorld(FRenderBus& Bus, const TArray<AActor*>& SelectedActors);

private:
	TArray<FPrimitiveProxy*> Proxies;
};
