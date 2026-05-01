#pragma once

#include "CollisionShapeGeometry.h"

namespace CollisionShapeQuery
{
bool OverlapSphereSphere(const FCollisionShapeGeometry& A, const FCollisionShapeGeometry& B);
bool OverlapBoxSphere(const FCollisionShapeGeometry& Box, const FCollisionShapeGeometry& Sphere);
bool OverlapBoxBox(const FCollisionShapeGeometry& A, const FCollisionShapeGeometry& B);
bool OverlapCapsuleSphere(const FCollisionShapeGeometry& Capsule, const FCollisionShapeGeometry& Sphere);
bool OverlapCapsuleBox(const FCollisionShapeGeometry& Capsule, const FCollisionShapeGeometry& Box);
bool OverlapCapsuleCapsule(const FCollisionShapeGeometry& A, const FCollisionShapeGeometry& B);
bool OverlapShapeGeometry(const FCollisionShapeGeometry& A, const FCollisionShapeGeometry& B);
}
