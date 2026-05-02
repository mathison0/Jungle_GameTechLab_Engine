#include "Collision2DManager.h"

#include "Collision/Collision2DShapeGeometry.h"
#include "Collision/SpatialHash.h"
#include "Component/Collision/Collider2DComponent.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Math/MathUtils.h"

#include <algorithm>
#include <cmath>

namespace
{
float Dot2D(const FVector2& A, const FVector2& B)
{
    return A.X * B.X + A.Y * B.Y;
}

float LengthSquared2D(const FVector2& V)
{
    return Dot2D(V, V);
}

FVector2 NormalizeSafe2D(const FVector2& V, const FVector2& Fallback)
{
    const float LengthSq = LengthSquared2D(V);
    if (LengthSq <= 0.000001f)
    {
        return Fallback;
    }

    return V / std::sqrt(LengthSq);
}

float ProjectBoxRadius(const FCollision2DShapeGeometry& Box, const FVector2& Axis)
{
    return std::abs(Dot2D(Box.AxisX, Axis)) * Box.BoxExtent.X
        + std::abs(Dot2D(Box.AxisY, Axis)) * Box.BoxExtent.Y;
}

bool TestAxis(const FCollision2DShapeGeometry& A, const FCollision2DShapeGeometry& B, const FVector2& Axis, FCollision2DContact& InOutContact)
{
    const float RadiusA = ProjectBoxRadius(A, Axis);
    const float RadiusB = ProjectBoxRadius(B, Axis);
    const FVector2 Delta = B.Center - A.Center;
    const float Distance = Dot2D(Delta, Axis);
    const float Overlap = RadiusA + RadiusB - std::abs(Distance);

    if (Overlap <= 0.0f)
    {
        return false;
    }

    if (Overlap < InOutContact.PenetrationDepth)
    {
        InOutContact.PenetrationDepth = Overlap;
        InOutContact.Normal = Distance < 0.0f ? Axis * -1.0f : Axis;
    }

    return true;
}

FVector2 ClosestPointOnBox(const FCollision2DShapeGeometry& Box, const FVector2& Point)
{
    const FVector2 Delta = Point - Box.Center;
    const float X = std::clamp(Dot2D(Delta, Box.AxisX), -Box.BoxExtent.X, Box.BoxExtent.X);
    const float Y = std::clamp(Dot2D(Delta, Box.AxisY), -Box.BoxExtent.Y, Box.BoxExtent.Y);
    return Box.Center + Box.AxisX * X + Box.AxisY * Y;
}

bool ComputeCircleCircle(const FCollision2DShapeGeometry& A, const FCollision2DShapeGeometry& B, FCollision2DContact& OutContact)
{
    const FVector2 Delta = B.Center - A.Center;
    const float RadiusSum = A.Radius + B.Radius;
    const float DistanceSq = LengthSquared2D(Delta);

    if (DistanceSq > RadiusSum * RadiusSum)
    {
        return false;
    }

    const float Distance = std::sqrt((std::max)(DistanceSq, 0.0f));
    OutContact.Normal = NormalizeSafe2D(Delta, FVector2(1.0f, 0.0f));
    OutContact.PenetrationDepth = RadiusSum - Distance;
    return true;
}

bool ComputeBoxBox(const FCollision2DShapeGeometry& A, const FCollision2DShapeGeometry& B, FCollision2DContact& OutContact)
{
    OutContact.PenetrationDepth = 3.402823466e+38F;
    return TestAxis(A, B, A.AxisX, OutContact)
        && TestAxis(A, B, A.AxisY, OutContact)
        && TestAxis(A, B, B.AxisX, OutContact)
        && TestAxis(A, B, B.AxisY, OutContact);
}

bool ComputeBoxCircle(const FCollision2DShapeGeometry& Box, const FCollision2DShapeGeometry& Circle, FCollision2DContact& OutContact)
{
    const FVector2 ClosestPoint = ClosestPointOnBox(Box, Circle.Center);
    const FVector2 Delta = Circle.Center - ClosestPoint;
    const float DistanceSq = LengthSquared2D(Delta);

    if (DistanceSq > Circle.Radius * Circle.Radius)
    {
        return false;
    }

    if (DistanceSq > 0.000001f)
    {
        const float Distance = std::sqrt(DistanceSq);
        OutContact.Normal = Delta / Distance;
        OutContact.PenetrationDepth = Circle.Radius - Distance;
        return true;
    }

    FCollision2DContact AxisContact;
    AxisContact.PenetrationDepth = 3.402823466e+38F;
    TestAxis(Box, Box, Box.AxisX, AxisContact);
    TestAxis(Box, Box, Box.AxisY, AxisContact);

    const FVector2 LocalDelta = Circle.Center - Box.Center;
    const float XDepth = Box.BoxExtent.X - std::abs(Dot2D(LocalDelta, Box.AxisX));
    const float YDepth = Box.BoxExtent.Y - std::abs(Dot2D(LocalDelta, Box.AxisY));
    const bool bUseX = XDepth < YDepth;
    const FVector2 Axis = bUseX ? Box.AxisX : Box.AxisY;
    const float Direction = Dot2D(LocalDelta, Axis) < 0.0f ? -1.0f : 1.0f;

    OutContact.Normal = Axis * Direction;
    OutContact.PenetrationDepth = (bUseX ? XDepth : YDepth) + Circle.Radius;
    return true;
}
} // namespace

void FCollision2DManager::Update(UWorld& World)
{
    TArray<UCollider2DComponent*> Colliders;
    CollectColliders(World, Colliders);

	SpatialHash.Build(Colliders);
    TArray<FCollision2DPair> CollisionPairs;
    SpatialHash.QueryPairs(CollisionPairs);

	for (FCollision2DPair& Pair : CollisionPairs)
	{
        UCollider2DComponent* ColliderA = Pair.A;
        UCollider2DComponent* ColliderB = Pair.B;

		if (!CanCollide(ColliderA, ColliderB))
        {
            continue;
        }

		FCollision2DContact Contact;
        if (ComputePenetration(ColliderA->GetCollision2DShapeGeometry(), ColliderB->GetCollision2DShapeGeometry(), Contact))
        {
            const bool bBlocking = ColliderA->IsBlockComponents() && ColliderB->IsBlockComponents();
            if (bBlocking)
            {
				//Generate HitEvent
                ResolveBlock(ColliderA, ColliderB, Contact);
            }
            else if (ColliderA->ShouldGenerateOverlapEvents() && ColliderB->ShouldGenerateOverlapEvents())
            {
				//Generate Overlap Event
                ColliderA->AddOverlapInfo(ColliderB);
                ColliderB->AddOverlapInfo(ColliderA);
            }

            ColliderA->SetDebugOverlapping(true);
            ColliderB->SetDebugOverlapping(true);
        }
	}
}

void FCollision2DManager::CollectColliders(UWorld& World, TArray<UCollider2DComponent*>& OutColliders)
{
    for (AActor* Actor : World.GetActors())
    {
        if (!Actor)
        {
            continue;
        }

        for (UPrimitiveComponent* Primitive : Actor->GetPrimitiveComponents())
        {
            UCollider2DComponent* Collider = Cast<UCollider2DComponent>(Primitive);
            if (!Collider)
            {
                continue;
            }

            Collider->ClearOverlapInfos();
            Collider->SetDebugOverlapping(false);
            OutColliders.push_back(Collider);
        }
    }
}

bool FCollision2DManager::CanCollide(UCollider2DComponent* ShapeA, UCollider2DComponent* ShapeB)
{
    if (!ShapeA || !ShapeB || ShapeA == ShapeB || ShapeA->GetOwner() == ShapeB->GetOwner())
    {
        return false;
    }

    const bool bWantsOverlap = ShapeA->ShouldGenerateOverlapEvents() && ShapeB->ShouldGenerateOverlapEvents();
    const bool bWantsBlock = ShapeA->IsBlockComponents() && ShapeB->IsBlockComponents();
    const bool bWantsHit = ShapeA->ShouldGenerateHitEvents() || ShapeB->ShouldGenerateHitEvents();

    return bWantsOverlap || bWantsBlock || bWantsHit;
}

bool FCollision2DManager::ComputePenetration(const FCollision2DShapeGeometry& A, const FCollision2DShapeGeometry& B, FCollision2DContact& OutContact) const
{
    if (A.Type == ECollision2DShapeType::Circle && B.Type == ECollision2DShapeType::Circle)
    {
        return ComputeCircleCircle(A, B, OutContact);
    }

    if (A.Type == ECollision2DShapeType::Box && B.Type == ECollision2DShapeType::Box)
    {
        return ComputeBoxBox(A, B, OutContact);
    }

    if (A.Type == ECollision2DShapeType::Box && B.Type == ECollision2DShapeType::Circle)
    {
        return ComputeBoxCircle(A, B, OutContact);
    }

    if (A.Type == ECollision2DShapeType::Circle && B.Type == ECollision2DShapeType::Box)
    {
        const bool bHit = ComputeBoxCircle(B, A, OutContact);
        OutContact.Normal = OutContact.Normal * -1.0f;
        return bHit;
    }

    return false;
}

void FCollision2DManager::ResolveBlock(UCollider2DComponent* ShapeA, UCollider2DComponent* ShapeB, const FCollision2DContact& Contact)
{
    if (!ShapeA || !ShapeB || Contact.PenetrationDepth <= FMath::Epsilon)
    {
        return;
    }

    const FVector Correction = FVector(Contact.Normal.X, Contact.Normal.Y, 0.0f) * (Contact.PenetrationDepth + 0.001f);

    const bool bAMovable = ShapeA->IsMovable();
    const bool bBMovable = ShapeB->IsMovable();

    if (bAMovable && bBMovable)
    {
        ShapeA->AddWorldOffset(Correction * -0.5f);
        ShapeB->AddWorldOffset(Correction * 0.5f);
    }
    else if (bAMovable)
    {
        ShapeA->AddWorldOffset(Correction * -1.0f);
    }
    else if (bBMovable)
    {
        ShapeB->AddWorldOffset(Correction);
    }
}
