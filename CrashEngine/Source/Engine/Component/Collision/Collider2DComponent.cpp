#include "Collider2DComponent.h"
#include "Object/ObjectFactory.h"
#include "Serialization/Archive.h"

#include <cmath>

IMPLEMENT_ABSTRACT_CLASS(UCollider2DComponent, UShapeComponent)

UCollider2DComponent::UCollider2DComponent()
{
    ShapeColor = FColor(0, 220, 255, 255).ToVector4();
}

void UCollider2DComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
    UShapeComponent::GetEditableProperties(OutProps);
    OutProps.push_back({ "DebugPlaneOffsetZ", EPropertyType::Float, &DebugPlaneOffsetZ, -10000.0f, 10000.0f, 0.1f });
}

void UCollider2DComponent::Serialize(FArchive& Ar)
{
    UShapeComponent::Serialize(Ar);
    Ar << DebugPlaneOffsetZ;
}

FCollisionShapeGeometry UCollider2DComponent::GetCollisionShapeGeometry() const
{
    FCollisionShapeGeometry Geometry;
    Geometry.Type = ECollisionShapeType::Box;
    Geometry.Center = GetShapeWorldLocation();
    Geometry.BoxExtent = LocalExtents;
    return Geometry;
}

FVector2 UCollider2DComponent::GetShapeWorldLocation2D() const
{
    return ProjectWorldVectorTo2D(GetShapeWorldLocation());
}

float UCollider2DComponent::GetCollisionPlaneZ() const
{
    return GetShapeWorldLocation().Z + DebugPlaneOffsetZ;
}

FVector2 UCollider2DComponent::ProjectWorldVectorTo2D(const FVector& Vector) const
{
    return FVector2(Vector.X, Vector.Y);
}

FVector UCollider2DComponent::Expand2DPointToWorld(const FVector2& Point) const
{
    return FVector(Point.X, Point.Y, GetCollisionPlaneZ());
}

FVector2 UCollider2DComponent::GetSafeAxis2D(const FVector& WorldAxis, const FVector2& FallbackAxis) const
{
    FVector2 Axis = ProjectWorldVectorTo2D(WorldAxis);
    const float Length = Axis.Length();
    if (Length <= 0.0001f)
    {
        return FallbackAxis;
    }

    return Axis / Length;
}
