#include "CollisionShapeQuery.h"

#include "Math/Intersection.h"
#include "Math/MathUtils.h"

namespace CollisionShapeQuery
{
bool OverlapSphereSphere(const FCollisionShapeGeometry& A, const FCollisionShapeGeometry& B)
{
    const float RadiusSum = A.Radius + B.Radius;
    return (A.Center - B.Center).LengthSquared() <= RadiusSum * RadiusSum;
}

bool OverlapBoxSphere(const FCollisionShapeGeometry& Box, const FCollisionShapeGeometry& Sphere)
{
    const FVector LocalSphereCenter = Box.Rotation.Inverse().RotateVector(Sphere.Center - Box.Center);

    const FVector ClosestPoint(
        FMath::Clamp(LocalSphereCenter.X, -Box.BoxExtent.X, Box.BoxExtent.X),
        FMath::Clamp(LocalSphereCenter.Y, -Box.BoxExtent.Y, Box.BoxExtent.Y),
        FMath::Clamp(LocalSphereCenter.Z, -Box.BoxExtent.Z, Box.BoxExtent.Z));

    return (LocalSphereCenter - ClosestPoint).LengthSquared() <= Sphere.Radius * Sphere.Radius;
}

bool OverlapBoxBox(const FCollisionShapeGeometry& A, const FCollisionShapeGeometry& B)
{
    const FVector AxesA[3] = {
        A.Rotation.RotateVector(FVector(1.0f, 0.0f, 0.0f)).Normalized(),
        A.Rotation.RotateVector(FVector(0.0f, 1.0f, 0.0f)).Normalized(),
        A.Rotation.RotateVector(FVector(0.0f, 0.0f, 1.0f)).Normalized()
    };
    const FVector AxesB[3] = {
        B.Rotation.RotateVector(FVector(1.0f, 0.0f, 0.0f)).Normalized(),
        B.Rotation.RotateVector(FVector(0.0f, 1.0f, 0.0f)).Normalized(),
        B.Rotation.RotateVector(FVector(0.0f, 0.0f, 1.0f)).Normalized()
    };

    return FMath::IntersectOBBOBB(A.Center, A.BoxExtent, AxesA, B.Center, B.BoxExtent, AxesB);
}

bool OverlapShapeGeometry(const FCollisionShapeGeometry& A, const FCollisionShapeGeometry& B)
{
    switch (A.Type)
    {
    case ECollisionShapeType::Sphere:
        switch (B.Type)
        {
        case ECollisionShapeType::Sphere:
            return OverlapSphereSphere(A, B);
        case ECollisionShapeType::Box:
            return OverlapBoxSphere(B, A);
        default:
            break;
        }
        break;

    case ECollisionShapeType::Box:
        switch (B.Type)
        {
        case ECollisionShapeType::Box:
            return OverlapBoxBox(A, B);
        case ECollisionShapeType::Sphere:
            return OverlapBoxSphere(A, B);
        default:
            break;
        }
        break;

    case ECollisionShapeType::Capsule:
        break;
    }

    return false;
}
} // namespace CollisionShapeQuery
