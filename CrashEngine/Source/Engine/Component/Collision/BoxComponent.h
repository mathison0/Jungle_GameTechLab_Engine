#pragma once
#include "ShapeComponent.h"
class UBoxComponent : public UShapeComponent
{
public:
    DECLARE_CLASS(UBoxComponent, UShapeComponent)

	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
    void PostEditProperty(const char* PropertyName) override;

    void Serialize(FArchive& Ar) override;

	ECollisionShapeType GetCollisionShapeType() const override{ return ECollisionShapeType::Box; }

	const FVector& GetBoxExtent() const { return BoxExtent; }
    void SetBoxExtent(const FVector& NewBoxExtent);


	protected:
    FVector BoxExtent;
};
