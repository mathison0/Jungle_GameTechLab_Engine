#pragma once

#include "BVH/BVHTypes.h"
#include "Gizmo/GizmoMeshFactory.h"
#include "Scene/SceneTypes.h"

struct FGizmoLocalBVH
{
	FBVHSpatialData SpatialData;
};

FGizmoLocalBVH BuildGizmoLocalBVH(const FGizmoMesh& InMesh);
bool IntersectGizmoLocalBVH(const FRay& InLocalRay, const FGizmoMesh& InMesh, const FGizmoLocalBVH& InLocalBVH, float& OutHitDistance, FVector& OutLocalHitPosition);
