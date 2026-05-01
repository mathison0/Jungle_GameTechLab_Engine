#pragma once

#include "ShapeComponent.h"

class USphereComponent : public UShapeComponent
{
public:
    DECLARE_CLASS(USphereComponent, UShapeComponent)

    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
    void PostEditProperty(const char* PropertyName) override;

    void Serialize(FArchive& Ar) override;

    ECollisionShapeType GetCollisionShapeType() const override { return ECollisionShapeType::Sphere; }

	const float GetRadius() const { return SphereRadius; }
	void SetRadius(float NewRadius);

protected:
    float SphereRadius = 50.0f;
};
