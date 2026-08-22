#pragma once

#include "TextRenderComponent.h"


class ENGINE_API UUUIDTextRenderComponent : public UTextRenderComponent
{
public:
	DECLARE_RTTI(UUUIDTextRenderComponent, UTextRenderComponent)

	void PostConstruct() override;

	virtual FRenderMesh* GetRenderMesh() const override;
	virtual FString GetDisplayText() const override;

	virtual FVector GetRenderWorldPosition() const override;
	virtual FVector GetRenderWorldScale() const override;
	virtual bool ShouldIncludeInBVH() const override { return false; }

	const FVector& GetWorldOffset() const { return WorldOffset; }
	void SetWorldOffset(const FVector& InOffset) { WorldOffset = InOffset; }

	virtual FBoxSphereBounds GetWorldBounds() const override;

private:
	FVector WorldOffset = FVector(0.0f, 0.0f, 0.3f);
};
