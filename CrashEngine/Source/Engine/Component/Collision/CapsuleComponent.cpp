#include "CapsuleComponent.h"
#include "Object/ObjectFactory.h"
#include "Serialization/Archive.h"

IMPLEMENT_CLASS(UCapsuleComponent, UShapeComponent)

void UCapsuleComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    UShapeComponent::GetEditableProperties(OutProps);
    OutProps.push_back({ "CapsuleHalfHeight", EPropertyType::Float, &CapsuleHalfHeight, 0.0f, 10000.0f, 1.0f });
    OutProps.push_back({ "CapsuleRadius", EPropertyType::Float, &CapsuleRadius, 0.0f, 10000.0f, 1.0f });
}

void UCapsuleComponent::PostEditProperty(const char* PropertyName)
{
    UShapeComponent::PostEditProperty(PropertyName);
}

void UCapsuleComponent::Serialize(FArchive& Ar)
{
    UShapeComponent::Serialize(Ar);
    Ar << CapsuleHalfHeight;
    Ar << CapsuleRadius;
}

void UCapsuleComponent::SetHalfHeight(float NewHalfHeight)
{
    CapsuleHalfHeight = NewHalfHeight;
}

void UCapsuleComponent::SetRadius(float NewRadius)
{
    CapsuleRadius = NewRadius;
}