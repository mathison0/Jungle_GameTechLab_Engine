#pragma once

#include "Viewport/ViewportTypes.h"

class AActor;
class UPrimitiveComponent;
class UScene;

struct FRay
{
	FVector Origin;
	FVector Direction;
};

class FPicker
{
public:
	FRay ScreenToRay(const FViewportEntry& Entry, int32 ScreenX, int32 ScreenY) const;

	bool RayTriangleIntersect(
		const FRay& Ray,
		const FVector& V0,
		const FVector& V1,
		const FVector& V2,
		float& OutDistance) const;

	AActor* PickActor(UScene* Scene, const FViewportEntry* Entry, int32 ScreenX, int32 ScreenY) const;
	UPrimitiveComponent* PickPrimitiveComponent(UScene* Scene, const FViewportEntry* Entry, int32 ScreenX, int32 ScreenY) const;
};
