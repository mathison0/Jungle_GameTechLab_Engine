#pragma once

#include "TextComponent.h"


class ENGINE_API UUUIDBillboardComponent : public UTextComponent
{
public:
	DECLARE_RTTI(UUUIDBillboardComponent, UTextComponent)

	void PostConstruct() override;

	virtual FString GetDisplayText() const override;
	// SetWorldOffset 諛섏쁺?댁꽌 ?ㅻ툕?앺듃 癒몃━ ?꾩뿉 ?⑤룄濡???
	virtual FVector GetRenderWorldPosition() const override;
	virtual FVector GetRenderWorldScale() const override;
	virtual bool ShouldIncludeInBVH() const override { return false; }

	const FVector& GetWorldOffset() const { return WorldOffset; }
	void SetWorldOffset(const FVector& InOffset) { WorldOffset = InOffset; }

	virtual FBoxSphereBounds GetWorldBounds() const override;

private:
	FVector WorldOffset = FVector(0.0f, 0.0f, 0.3f);
};
