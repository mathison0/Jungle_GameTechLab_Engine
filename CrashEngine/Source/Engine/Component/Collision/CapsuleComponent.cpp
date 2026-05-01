#include "CapsuleComponent.h"
#include "Object/ObjectFactory.h"
#include "Render/Scene/Debug/DebugRenderAPI.h"
#include "Serialization/Archive.h"

#include <algorithm>

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

FCollisionShapeGeometry UCapsuleComponent::GetCollisionShapeGeometry() const
{
    FCollisionShapeGeometry Geometry;
    Geometry.Type = GetCollisionShapeType();
    Geometry.Radius = GetScaledCapsuleRadius();
    Geometry.HalfHeight = GetScaledCapsuleHalfHeight();
    Geometry.Center = GetShapeWorldLocation();
    Geometry.Rotation = GetWorldRotation();
    Geometry.Axis = GetCapsuleAxis();
    return Geometry;
}

void UCapsuleComponent::SetHalfHeight(float NewHalfHeight)
{
    CapsuleHalfHeight = NewHalfHeight;
}

void UCapsuleComponent::SetRadius(float NewRadius)
{
    CapsuleRadius = NewRadius;
}

float UCapsuleComponent::GetScaledCapsuleRadius() const
{
    const FVector Scale = GetAbsWorldScale();
    const float RadiusScale = std::max(Scale.X, Scale.Y);

	return CapsuleRadius * RadiusScale;
}

float UCapsuleComponent::GetScaledCapsuleHalfHeight() const
{
    const FVector Scale = GetAbsWorldScale();

	return CapsuleHalfHeight * Scale.Z;
}

FVector UCapsuleComponent::GetCapsuleAxis() const
{
    return GetUpVector();
}

void UCapsuleComponent::RenderDebugShape(FScene& Scene) const
{
    RenderDebugCapsule(Scene,
                       GetShapeWorldLocation(),
                       GetCapsuleAxis(),
                       GetScaledCapsuleRadius(),
                       GetScaledCapsuleHalfHeight(),
                       24,
                       GetDebugShapeColor(),
                       0.0f);
}
