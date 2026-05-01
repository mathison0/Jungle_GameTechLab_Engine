#pragma once

#include "Component/PrimitiveComponent.h"

enum class ECollisionShapeType
{
	Box,
	Sphere,
	Capsule
};

class UShapeComponent : public UPrimitiveComponent
{
public:
    DECLARE_CLASS(UShapeComponent, UPrimitiveComponent)

    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
    void PostEditProperty(const char* PropertyName) override;

    void Serialize(FArchive& Ar) override;

    void SetShapeColor(FColor NewColor);
    void SetDrawOnlyIfSelected(bool bNewDrawOnlyIfSelected);

	virtual ECollisionShapeType GetCollisionShapeType() const = 0;

protected:
    FColor ShapeColor = FColor::Yellow();
    bool bDrawOnlyIfSelected = true;
};
