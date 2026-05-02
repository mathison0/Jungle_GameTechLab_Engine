#pragma once

#include "Math/Vector.h"

enum class ECollision2DShapeType
{
    Box,
    Circle
};

struct FCollision2DShapeGeometry
{
    ECollision2DShapeType Type = ECollision2DShapeType::Box;
    FVector2 Center = FVector2(0.0f, 0.0f);
    FVector2 AxisX = FVector2(1.0f, 0.0f);
    FVector2 AxisY = FVector2(0.0f, 1.0f);
    FVector2 BoxExtent = FVector2(0.0f, 0.0f);
    float Radius = 0.0f;
};

struct FCollision2DContact
{
    FVector2 Normal = FVector2(1.0f, 0.0f);
    float PenetrationDepth = 0.0f;
};
