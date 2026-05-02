#pragma once

#include "Core/CoreTypes.h"
#include "Collision/SpatialHash.h"

class UCollider2DComponent;
class UWorld;
struct FCollision2DContact;
struct FCollision2DShapeGeometry;

class FCollision2DManager
{
public:
    void Update(UWorld& World);

private:
    void CollectColliders(UWorld& World, TArray<UCollider2DComponent*>& OutColliders);
    bool CanCollide(UCollider2DComponent* ShapeA, UCollider2DComponent* ShapeB);
    bool ComputePenetration(const FCollision2DShapeGeometry& A, const FCollision2DShapeGeometry& B, FCollision2DContact& OutContact) const;
    void ResolveBlock(UCollider2DComponent* ShapeA, UCollider2DComponent* ShapeB, const FCollision2DContact& Contact);

	private:
    FSpatialHash SpatialHash;
};
