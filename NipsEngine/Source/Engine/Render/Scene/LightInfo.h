#pragma once

#include "Math/Vector.h"

struct FDirectionalLightInfo
{
    FVector Direction;
	float Intensity;

	FVector Color;
};

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