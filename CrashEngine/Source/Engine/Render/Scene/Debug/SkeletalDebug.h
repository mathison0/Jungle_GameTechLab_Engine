#pragma once
#include "Math/Matrix.h"
#include "Core/ClassTypes.h"
#include "Core/EngineTypes.h"

struct FSkeletalDebugBone
{
    FMatrix WorldMatrix = FMatrix::Identity;
    int32   ParentIndex = -1;
    FColor  Color       = FColor(255, 255, 255);
    bool    bDraw       = true;
};

struct FSkeletalDebugInstance
{
    TArray<FSkeletalDebugBone> Bones;
};
