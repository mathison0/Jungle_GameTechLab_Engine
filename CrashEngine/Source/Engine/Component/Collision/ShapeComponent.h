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
    void ContributeSelectedVisuals(FScene& Scene) const override;

    void SetShapeColor(FColor NewColor);
    const FVector4& GetShapeColor() const { return ShapeColor; }
    void SetDrawOnlyIfSelected(bool bNewDrawOnlyIfSelected);
    bool ShouldDrawOnlyIfSelected() const { return bDrawOnlyIfSelected; }

	virtual ECollisionShapeType GetCollisionShapeType() const = 0;

	FVector GetShapeWorldLocation() const;
    FVector GetAbsWorldScale() const;

protected:
    void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction& ThisTickFunction) override;
    virtual void RenderDebugShape(FScene& Scene) const = 0;
    bool ShouldRenderDebugShape() const;
    FColor GetDebugShapeColor() const;

protected:
    FVector4 ShapeColor = FColor::Yellow().ToVector4();
    bool bDrawOnlyIfSelected = true;
};
