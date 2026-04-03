#pragma once
#include "Render/Pipeline/RenderCommand.h"
#include "Render/Pipeline/RenderBus.h"

class UPrimitiveComponent;

class FPrimitiveProxy
{
public:
	FPrimitiveProxy(UPrimitiveComponent* InOwner);
	virtual ~FPrimitiveProxy() = default;

	virtual void UpdateProxy() = 0;
	virtual void CollectRender(FRenderBus& Bus, bool bSelected);

	void MarkDirty() { bIsDirty = true; }
	bool IsDirty() const { return bIsDirty; }

	UPrimitiveComponent* GetOwner() const { return Owner; }

protected:
	UPrimitiveComponent* Owner;
	FRenderCommand CachedCommand;
	bool bIsDirty = true;
};

class FDefaultPrimitiveProxy : public FPrimitiveProxy
{
public:
	FDefaultPrimitiveProxy(UPrimitiveComponent* InOwner);
	void UpdateProxy() override;
};
