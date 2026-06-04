#pragma once

#include "Core/Types/CoreTypes.h"
#include "Object/Reflection/ObjectMacros.h"

UENUM()
enum class EAnimationTickLOD : uint8
{
	FullRate,		// every frame
	HalfRate,		// ~30Hz
	QuarterRate,	// ~15Hz
	LowRate,		// ~5-10Hz
	Frozen			// keep last pose
};
