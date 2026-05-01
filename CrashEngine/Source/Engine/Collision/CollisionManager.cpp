#include "CollisionManager.h"

#include "Component/Collision/ShapeComponent.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Collision/CollisionShapeQuery.h"

void FCollisionManager::Update(UWorld& World)
{
    TArray<UShapeComponent*> Shapes;

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
            Shapes.push_back(Shape);
        }
    }

    for (size_t i = 0; i < Shapes.size(); ++i)
    {
        UShapeComponent* A = Shapes[i];
        if (!A || !A->ShouldGenerateOverlapEvents())
        {
            continue;
        }

        for (size_t j = i + 1; j < Shapes.size(); ++j)
        {
            UShapeComponent* B = Shapes[j];
            if (!B || !B->ShouldGenerateOverlapEvents())
            {
                continue;
            }



			if (CollisionShapeQuery::OverlapShapeGeometry(A->GetCollisionShapeGeometry(), B->GetCollisionShapeGeometry()))
			{
                A->AddOverlapInfo(B);
                B->AddOverlapInfo(A);
                A->SetDebugOverlapping(true);
                B->SetDebugOverlapping(true);
			}
        }
    }
}
