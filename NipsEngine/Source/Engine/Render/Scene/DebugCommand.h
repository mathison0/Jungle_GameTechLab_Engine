#pragma once

#include "Core/CoreMinimal.h"

enum class EDebugShapeType : uint8
{
    Line,
    AABB,
    OBB,
    Sphere,
    Cone
};

struct FDebugSphere
{
    FVector  Center;
    float    Radius;
    FVector4 Color;
    int32    SegmentCount;
};

struct FDebugCone
{
    FVector  Apex;
    FVector  Direction; // normalize
    float    Height;
    float    Angle; // radians (half-angle)
    FVector4 Color;
    int32    SegmentCount;
};

struct FDebugRenderCommand
{
    EDebugShapeType Type;

    union
    {
        FDebugSphere Sphere;
        FDebugCone   Cone;
    };
};

namespace DebugCmd
{
    inline FDebugRenderCommand MakeCone(const FVector& Apex, const FVector& Dir, float Height, float Angle,
                                        const FVector4& Color, int32 SegmentCount = 16)
    {
        FDebugRenderCommand Cmd{};
        Cmd.Type = EDebugShapeType::Cone;
        Cmd.Cone = {Apex, Dir, Height, Angle, Color, SegmentCount};
        return Cmd;
    }
} // namespace DebugCmd