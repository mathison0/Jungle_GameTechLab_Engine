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

    for (size_t i = 0; i < ShapeComponents.size(); ++i)
    {
        for (size_t j = i + 1; j < ShapeComponents.size(); ++j)
        {
            UShapeComponent* AShapeComp = ShapeComponents[i];
            UShapeComponent* BShapeComp = ShapeComponents[j];

			if (!CanCollide(AShapeComp, BShapeComp))
			{
				continue;
			}

			FCollisionContact Contact;
			if (CollisionShapeQuery::ComputePenetration(AShapeComp->GetCollisionShapeGeometry(), BShapeComp->GetCollisionShapeGeometry(), Contact))
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

	if (AActor* OwnerA = ShapeA->GetOwner())
	{
		OwnerA->AddActorWorldOffset(HalfCorrection * -1.0f);
	}
	else
	{
		ShapeA->AddWorldOffset(HalfCorrection * -1.0f);
	}

	if (AActor* OwnerB = ShapeB->GetOwner())
	{
		OwnerB->AddActorWorldOffset(HalfCorrection);
	}
	else
	{
		ShapeB->AddWorldOffset(HalfCorrection);
	}
}
