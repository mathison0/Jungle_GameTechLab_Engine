#include "CollisionManager.h"

#include "Component/Collision/ShapeComponent.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Collision/CollisionShapeQuery.h"
#include "Math/MathUtils.h"

void FCollisionManager::Update(UWorld& World)
{
    TArray<UShapeComponent*> ShapeComponents;
    CollectShapes(World, ShapeComponents);
    ShapeBVH.Build(ShapeComponents);

    TArray<FShapeCollisionPair> CandidateShapePairs;
    ShapeBVH.QueryOverlappingPairs(CandidateShapePairs);

    for (const FShapeCollisionPair& Pair : CandidateShapePairs)
    {
        UShapeComponent* AShapeComp = Pair.ShapeCompA;
        UShapeComponent* BShapeComp = Pair.ShapeCompB;

        if (!CanCollide(AShapeComp, BShapeComp))
        {
            continue;
        }

        FCollisionContact Contact;
        if (CollisionShapeQuery::ComputePenetration(
                AShapeComp->GetCollisionShapeGeometry(),
                BShapeComp->GetCollisionShapeGeometry(),
                Contact))
        {
            const bool bBlocking = AShapeComp->IsBlockComponents() && BShapeComp->IsBlockComponents();
            if (bBlocking)
            {
                ResolveBlock(AShapeComp, BShapeComp, Contact);
            }
            else if (AShapeComp->ShouldGenerateOverlapEvents() && BShapeComp->ShouldGenerateOverlapEvents())
            {
                AShapeComp->AddOverlapInfo(BShapeComp);
                BShapeComp->AddOverlapInfo(AShapeComp);
            }

            AShapeComp->SetDebugOverlapping(true);
            BShapeComp->SetDebugOverlapping(true);
        }
    }
}

void FCollisionManager::CollectShapes(UWorld& World, TArray<UShapeComponent*>& OutShapes)
{
    for (AActor* Actor : World.GetActors())
    {
        if (!Actor)
        {
            continue;
        }

        for (UPrimitiveComponent* Primitive : Actor->GetPrimitiveComponents())
        {
            UShapeComponent* Shape = Cast<UShapeComponent>(Primitive);
            if (!Shape)
            {
                continue;
            }

            Shape->ClearOverlapInfos();
            Shape->SetDebugOverlapping(false);
            OutShapes.push_back(Shape);
        }
    }
}

bool FCollisionManager::CanCollide(UShapeComponent* ShapeA, UShapeComponent* ShapeB)
{
	if (!ShapeA || !ShapeB)
	{
		return false;
	}

	if (ShapeA == ShapeB)
	{
		return false;
	}

	if (ShapeA->GetOwner() == ShapeB->GetOwner())
	{
		return false;
	}

	const bool bWantsOverlap = ShapeA->ShouldGenerateOverlapEvents() && ShapeB->ShouldGenerateOverlapEvents();
	const bool bWantsBlock = ShapeA->IsBlockComponents() && ShapeB->IsBlockComponents();
	const bool bWantsHit = ShapeA->ShouldGenerateHitEvents() || ShapeB->ShouldGenerateHitEvents();

	return bWantsOverlap || bWantsBlock || bWantsHit;
}

void FCollisionManager::ResolveBlock(UShapeComponent* ShapeA, UShapeComponent* ShapeB, const FCollisionContact& Contact)
{
	if (!ShapeA || !ShapeB || Contact.PenetrationDepth <= FMath::Epsilon)
	{
		return;
	}

	const FVector Correction = Contact.Normal * (Contact.PenetrationDepth + 0.001f);
	const FVector HalfCorrection = Correction * 0.5f;

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
