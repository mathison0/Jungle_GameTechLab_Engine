#include "SpatialHash.h"
#include "Component/Collision/Collider2DComponent.h"


namespace
{
uint64 MakePairKey(const UCollider2DComponent* A, const UCollider2DComponent* B)
{
    uint32 AId = A->GetUUID();
    uint32 BId = B->GetUUID();

    if (AId > BId)
    {
        std::swap(AId, BId);
    }

    return (static_cast<uint64>(AId) << 32) | static_cast<uint64>(BId);
}
} // namespace

void FSpatialHash::Clear()
{
    Cells.clear();
}

void FSpatialHash::Build(const TArray<UCollider2DComponent*>& Colliders)
{
    Clear();

    for (UCollider2DComponent* Collider : Colliders)
    {
        if (!Collider)
        {
            continue;
        }

        InsertCollider(Collider);
    }
}

void FSpatialHash::QueryPairs(TArray<FCollision2DPair>& OutPairs) const
{
    std::unordered_set<uint64> VisitedPairs;

    for (const auto& CellPair : Cells)
    {
        const TArray<UCollider2DComponent*>& CellColliders = CellPair.second;

        for (int32 AIndex = 0; AIndex < static_cast<int32>(CellColliders.size()); ++AIndex)
        {
            for (int32 BIndex = AIndex + 1; BIndex < static_cast<int32>(CellColliders.size()); ++BIndex)
            {
                UCollider2DComponent* A = CellColliders[AIndex];
                UCollider2DComponent* B = CellColliders[BIndex];

                if (!A || !B || A == B)
                {
                    continue;
                }

                const uint64 PairKey = MakePairKey(A, B);
                if (VisitedPairs.find(PairKey) != VisitedPairs.end())
                {
                    continue;
                }

                VisitedPairs.insert(PairKey);
                OutPairs.push_back({ A, B });
            }
        }
    }
}

void FSpatialHash::SetCellSize(float NewCellSize)
{
    CellSize = (std::max)(NewCellSize, 0.001f);
}

FCollision2DCell FSpatialHash::GetCell(const FVector2& Position) const
{
    FCollision2DCell Cell;
    Cell.X = static_cast<int32>(std::floor(Position.X / CellSize));
    Cell.Y = static_cast<int32>(std::floor(Position.Y / CellSize));
    return Cell;
}

void FSpatialHash::InsertCollider(UCollider2DComponent* Collider)
{
    const FCollision2DBounds Bounds = Collider->GetCollision2DBounds();

    const FCollision2DCell MinCell = GetCell(Bounds.Min);
    const FCollision2DCell MaxCell = GetCell(Bounds.Max);

    for (int32 Y = MinCell.Y; Y <= MaxCell.Y; ++Y)
    {
        for (int32 X = MinCell.X; X <= MaxCell.X; ++X)
        {
            const FCollision2DCellKey Key = MakeCollision2DCellKey(X, Y);
            Cells[Key].push_back(Collider);
        }
    }
}