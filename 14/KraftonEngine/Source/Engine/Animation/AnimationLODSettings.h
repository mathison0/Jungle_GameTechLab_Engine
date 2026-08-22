#pragma once

#include "Animation/AnimationTickLOD.h"
#include "Core/Singleton.h"
#include "Core/Types/CoreTypes.h"

class FAnimationLODSettings : public TSingleton<FAnimationLODSettings>
{
	friend class TSingleton<FAnimationLODSettings>;

public:
	static constexpr float kDefaultFullRateDistance = 8.0f;
	static constexpr float kDefaultHalfRateDistance = 16.0f;
	static constexpr float kDefaultQuarterRateDistance = 28.0f;
	static constexpr float kDefaultLowRateDistance = 45.0f;
	static constexpr EAnimationTickLOD kDefaultPreDepthMaxLOD = EAnimationTickLOD::HalfRate;
	static constexpr EAnimationTickLOD kDefaultShadowCasterMaxLOD = EAnimationTickLOD::LowRate;

	void SetTickLODDistances(float Full, float Half, float Quarter, float Low)
	{
		FullRateDistance = Full;
		HalfRateDistance = Half;
		QuarterRateDistance = Quarter;
		LowRateDistance = Low;
	}

	void ResetTickLODDistances()
	{
		FullRateDistance = kDefaultFullRateDistance;
		HalfRateDistance = kDefaultHalfRateDistance;
		QuarterRateDistance = kDefaultQuarterRateDistance;
		LowRateDistance = kDefaultLowRateDistance;
	}

	float GetFullRateDistance() const { return FullRateDistance; }
	float GetHalfRateDistance() const { return HalfRateDistance; }
	float GetQuarterRateDistance() const { return QuarterRateDistance; }
	float GetLowRateDistance() const { return LowRateDistance; }

	void SetPreDepthMaxLOD(EAnimationTickLOD InLOD) { PreDepthMaxLOD = InLOD; }
	void ResetPreDepthMaxLOD() { PreDepthMaxLOD = kDefaultPreDepthMaxLOD; }
	EAnimationTickLOD GetPreDepthMaxLOD() const { return PreDepthMaxLOD; }

	bool ShouldEmitSkeletalPreDepth(EAnimationTickLOD LOD) const
	{
		return static_cast<uint8>(LOD) <= static_cast<uint8>(PreDepthMaxLOD);
	}

	void SetShadowCasterMaxLOD(EAnimationTickLOD InLOD) { ShadowCasterMaxLOD = InLOD; }
	void ResetShadowCasterMaxLOD() { ShadowCasterMaxLOD = kDefaultShadowCasterMaxLOD; }
	EAnimationTickLOD GetShadowCasterMaxLOD() const { return ShadowCasterMaxLOD; }

	bool ShouldEmitSkeletalShadowCaster(EAnimationTickLOD LOD) const
	{
		return static_cast<uint8>(LOD) <= static_cast<uint8>(ShadowCasterMaxLOD);
	}

private:
	FAnimationLODSettings() = default;
	~FAnimationLODSettings() = default;

private:
	float FullRateDistance = kDefaultFullRateDistance;
	float HalfRateDistance = kDefaultHalfRateDistance;
	float QuarterRateDistance = kDefaultQuarterRateDistance;
	float LowRateDistance = kDefaultLowRateDistance;
	EAnimationTickLOD PreDepthMaxLOD = kDefaultPreDepthMaxLOD;
	EAnimationTickLOD ShadowCasterMaxLOD = kDefaultShadowCasterMaxLOD;
};
