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
    FCollisionShapeGeometry GetCollisionShapeGeometry() const override;

	const float GetRadius() const { return SphereRadius; }
	void SetRadius(float NewRadius);

	float GetScaledSphereRadius() const;

protected:
    void RenderDebugShape(FScene& Scene) const override;

protected:
    float SphereRadius = 1.0f;
};
