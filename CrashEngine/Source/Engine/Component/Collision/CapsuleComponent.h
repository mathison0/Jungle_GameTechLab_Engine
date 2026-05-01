#pragma once

#include "ShapeComponent.h"

class UCapsuleComponent : public UShapeComponent
{
public:
    DECLARE_CLASS(UCapsuleComponent, UShapeComponent)

    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
    void PostEditProperty(const char* PropertyName) override;

    void Serialize(FArchive& Ar) override;

    ECollisionShapeType GetCollisionShapeType() const override { return ECollisionShapeType::Capsule; }

	void SetHalfHeight(float NewHalfHeight);
    void SetRadius(float NewRadius);
	const float GetHalfHeight() const { return CapsuleHalfHeight; }
    const float SetRadius() const { return CapsuleRadius; }

protected:
    float CapsuleHalfHeight = 88.0f;
    float CapsuleRadius = 34.0f;
};
