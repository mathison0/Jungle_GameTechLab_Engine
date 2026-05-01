#pragma once

#include "ShapeComponent.h"

class UCapsuleComponent : public UShapeComponent
{
public:
    DECLARE_CLASS(UCapsuleComponent, UShapeComponent)

    void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
    void PostEditProperty(const char* PropertyName) override;

    void Serialize(FArchive& Ar) override;

protected:
    float CapsuleHalfHeight = 88.0f;
    float CapsuleRadius = 34.0f;
};
