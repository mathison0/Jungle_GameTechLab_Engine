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
uint64 MakeCollider2DPairKey(const UCollider2DComponent* A, const UCollider2DComponent* B)
{
    uint32 AId = A->GetUUID();
    uint32 BId = B->GetUUID();

    if (AId > BId)
    {
        std::swap(AId, BId);
    }

    return (static_cast<uint64>(AId) << 32) | static_cast<uint64>(BId);
}

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

	TArray<FCollision2DPair> CollisionPairs;
	BuildCollisionPairs(Colliders, CollisionPairs);

	TMap<uint64, FCollision2DPair> CurrentOverlapPairs;
	ProcessCollisionPairs(CollisionPairs, CurrentOverlapPairs);

	DispatchEndOverlapEvents(CurrentOverlapPairs);
	PreviousOverlapPairs = std::move(CurrentOverlapPairs);
}

void FCollision2DManager::BuildCollisionPairs(const TArray<UCollider2DComponent*>& Colliders, TArray<FCollision2DPair>& OutPairs)
{
	SpatialHash.Build(Colliders);
	SpatialHash.QueryPairs(OutPairs);
}

void FCollision2DManager::ProcessCollisionPairs(const TArray<FCollision2DPair>& CollisionPairs, TMap<uint64, FCollision2DPair>& OutCurrentOverlapPairs)
{
	for (const FCollision2DPair& Pair : CollisionPairs)
	{
		ProcessCollisionPair(Pair.A, Pair.B, OutCurrentOverlapPairs);
	}
}

void FCollision2DManager::ProcessCollisionPair(UCollider2DComponent* ColliderA, UCollider2DComponent* ColliderB, TMap<uint64, FCollision2DPair>& OutCurrentOverlapPairs)
{
	if (!CanCollide(ColliderA, ColliderB))
	{
		return;
	}

	FCollision2DContact Contact;
	if (!ComputePenetration(ColliderA->GetCollision2DShapeGeometry(), ColliderB->GetCollision2DShapeGeometry(), Contact))
	{
		return;
	}

	//디버그용 색 변경 로직입니다.
	ColliderA->SetDebugOverlapping(true);
	ColliderB->SetDebugOverlapping(true);

	if (ColliderA->IsBlockComponents() && ColliderB->IsBlockComponents())
	{
		HandleBlockingCollision(ColliderA, ColliderB, Contact);
		return;
	}

	HandleOverlapCollision(ColliderA, ColliderB, OutCurrentOverlapPairs);
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

void FCollision2DManager::HandleBlockingCollision(UCollider2DComponent* ShapeA, UCollider2DComponent* ShapeB, const FCollision2DContact& Contact)
{
	ResolveBlock(ShapeA, ShapeB, Contact);

	if (ShapeA->ShouldGenerateHitEvents())
	{
		ShapeA->OnComponentHit2D.Broadcast(ShapeB);
	}

	if (ShapeB->ShouldGenerateHitEvents())
	{
		ShapeB->OnComponentHit2D.Broadcast(ShapeA);
	}
}

void FCollision2DManager::HandleOverlapCollision(UCollider2DComponent* ShapeA, UCollider2DComponent* ShapeB, TMap<uint64, FCollision2DPair>& OutCurrentOverlapPairs)
{
	if (!ShapeA->ShouldGenerateOverlapEvents() || !ShapeB->ShouldGenerateOverlapEvents())
	{
		return;
	}

	const uint64 PairKey = MakeCollider2DPairKey(ShapeA, ShapeB);
	OutCurrentOverlapPairs[PairKey] = { ShapeA, ShapeB };

	ShapeA->AddOverlapInfo(ShapeB);
	ShapeB->AddOverlapInfo(ShapeA);

	if (PreviousOverlapPairs.find(PairKey) == PreviousOverlapPairs.end())
	{
		ShapeA->OnComponentBeginOverlap2D.Broadcast(ShapeB);
		ShapeB->OnComponentBeginOverlap2D.Broadcast(ShapeA);
	}
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

void FCollision2DManager::DispatchEndOverlapEvents(const TMap<uint64, FCollision2DPair>& CurrentOverlapPairs)
{
	for (const auto& PreviousPair : PreviousOverlapPairs)
	{
		const uint64 PairKey = PreviousPair.first;
		if (CurrentOverlapPairs.find(PairKey) != CurrentOverlapPairs.end())
		{
			continue;
		}

		UCollider2DComponent* ColliderA = PreviousPair.second.A;
		UCollider2DComponent* ColliderB = PreviousPair.second.B;

		if (ColliderA && ColliderB)
		{
			ColliderA->OnComponentEndOverlap2D.Broadcast(ColliderB);
			ColliderB->OnComponentEndOverlap2D.Broadcast(ColliderA);
		}
	}
}
