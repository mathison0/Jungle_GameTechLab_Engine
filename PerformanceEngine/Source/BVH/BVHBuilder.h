#pragma once

#include "BVHTypes.h"

class FStaticMesh;

class FBVHBuilder
{
public:
	FBVHSpatialData BuildBVH(FStaticMesh* InStaticMesh);
};
