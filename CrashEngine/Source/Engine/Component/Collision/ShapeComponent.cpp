#include "ShapeComponent.h"
#include "Object/ObjectFactory.h"
#include "Serialization/Archive.h"

IMPLEMENT_ABSTRACT_CLASS(UShapeComponent, UPrimitiveComponent)

void UShapeComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    UPrimitiveComponent::GetEditableProperties(OutProps);
    OutProps.push_back({ "Shape Color", EPropertyType::Color4, &ShapeColor });
    OutProps.push_back({ "DrawOnlyIfSelected", EPropertyType::Bool, &bDrawOnlyIfSelected });
}

void UShapeComponent::PostEditProperty(const char* PropertyName)
{
}

void UShapeComponent::Serialize(FArchive& Ar)
{
    UPrimitiveComponent::Serialize(Ar);
    Ar << ShapeColor;
    Ar << bDrawOnlyIfSelected;
}

void UShapeComponent::SetShapeColor(FColor NewColor)
{
    ShapeColor = NewColor;
}

void UShapeComponent::SetDrawOnlyIfSelected(bool bNewDrawOnlyIfSelected)
{
    bDrawOnlyIfSelected = bNewDrawOnlyIfSelected;
}


