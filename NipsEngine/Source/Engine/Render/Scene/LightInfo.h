#pragma once

#include "Math/Vector.h"

struct FSpotLightInfo
{
    FVector Position;
    float   Radius;

	FVector Color;
    float   Intensity;

	FVector Direction;
    float   InnerConeCos;
    float   OuterConeCos;

};