#pragma once

#include "Animation/AnimationTickLOD.h"

inline float GetAnimationTickIntervalForLOD(EAnimationTickLOD LOD)
{
	switch (LOD)
	{
	case EAnimationTickLOD::FullRate:
		return 0.0f;
	case EAnimationTickLOD::HalfRate:
		return 1.0f / 30.0f;
	case EAnimationTickLOD::QuarterRate:
		return 1.0f / 15.0f;
	case EAnimationTickLOD::LowRate:
		return 1.0f / 8.0f;
	case EAnimationTickLOD::Frozen:
		return FLT_MAX;
	default:
		return 0.0f;
	}
}

inline float ComputePhaseOffset(uint32 PhaseSeed, EAnimationTickLOD LOD)
{
	const float Interval = GetAnimationTickIntervalForLOD(LOD);
	if (Interval <= 0.0f) return 0.0f;

	constexpr uint32 PhaseBucketCount = 16;
	const float Phase01 = static_cast<float>(PhaseSeed % PhaseBucketCount) / static_cast<float>(PhaseBucketCount);

	return Interval * Phase01;
}
