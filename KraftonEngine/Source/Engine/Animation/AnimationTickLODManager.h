#pragma once

#include "Animation/AnimationTickLOD.h"
#include "Core/Types/CoreTypes.h"
#include "Core/Singleton.h"
#include "Math/Vector.h"

class USkeletalMeshComponent;
class UWorld;

struct FAnimationTickLODStats
{
	bool bValid = false;
	bool bEnabled = false;
	uint32 RegisteredCount = 0;
	uint32 ManagedCount = 0;
	uint32 LODCounts[5] = { 0, 0, 0, 0, 0 };
	uint32 EvaluatedCount = 0;
	uint32 SkippedCount = 0;
};

struct FAnimationTickLODEntry
{
	USkeletalMeshComponent* Component = nullptr;
	uint32 PhaseSeed = 0;
	EAnimationTickLOD LastLOD = EAnimationTickLOD::FullRate;
};

class FAnimationTickLODManager : public TSingleton<FAnimationTickLODManager>
{
	friend class TSingleton<FAnimationTickLODManager>;
public:
	void RegisterComponent(USkeletalMeshComponent* Component);
	void UnregisterComponent(USkeletalMeshComponent* Component);

	void Tick(UWorld* World, float DeltaTime, const FVector& ViewLocation);

	void SetEnabled(bool bInEnabled) { bEnabled = bInEnabled; }
	bool IsEnabled() const { return bEnabled; }
	const FAnimationTickLODStats& GetStats() const { return Stats; }

	void RecordAnimationEvaluated();
	void RecordAnimationSkipped();

private:
	FAnimationTickLODManager() = default;
	~FAnimationTickLODManager() = default;

private:
	void BeginStatsFrame();
	void RemoveInvalidComponents();
	void ApplyLOD(FAnimationTickLODEntry& Entry, const FVector& ViewLocation);
	void RecordLOD(EAnimationTickLOD LOD);

private:
	TArray<FAnimationTickLODEntry> Components;
	uint32 NextPhaseSeed = 0;
	FAnimationTickLODStats Stats;

	bool bEnabled = true;

	float FullRateDistance = 8.0f;
	float HalfRateDistance = 16.0f;
	float QuarterRateDistance = 28.0f;
	float LowRateDistance = 45.0f;
};
