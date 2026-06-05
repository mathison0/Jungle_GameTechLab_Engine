#include "Animation/AnimationTickLODManager.h"

#include "Animation/AnimationLODSettings.h"
#include "Animation/AnimationTickLODHelper.h"
#include "Component/Primitive/SkeletalMeshComponent.h"
#include "GameFramework/World.h"

#include <algorithm>

namespace
{
	int32 GetAnimationTickLODStatIndex(EAnimationTickLOD LOD)
	{
		switch (LOD)
		{
		case EAnimationTickLOD::FullRate:
			return 0;
		case EAnimationTickLOD::HalfRate:
			return 1;
		case EAnimationTickLOD::QuarterRate:
			return 2;
		case EAnimationTickLOD::LowRate:
			return 3;
		case EAnimationTickLOD::Frozen:
			return 4;
		default:
			return -1;
		}
	}
}

void FAnimationTickLODManager::RegisterComponent(USkeletalMeshComponent* Component)
{
	if (!Component) return;

	for (const FAnimationTickLODEntry& Entry : Components)
	{
		if (Entry.Component == Component) return;
	}

	FAnimationTickLODEntry Entry;
	Entry.Component = Component;
	Entry.PhaseSeed = NextPhaseSeed++;
	Entry.LastLOD = Component->GetAnimationTickLOD();

	Components.push_back(Entry);
}

void FAnimationTickLODManager::UnregisterComponent(USkeletalMeshComponent* Component)
{
	if (!Component) return;

	Components.erase(std::remove_if(Components.begin(), Components.end(),
		[Component](const FAnimationTickLODEntry& Entry) { return Entry.Component == Component; }),
		Components.end());
}

void FAnimationTickLODManager::Tick(UWorld* World, float DeltaTime, const FVector& ViewLocation)
{
	if (!World) return;

	(void)DeltaTime;

	RemoveInvalidComponents();
	BeginStatsFrame();

	for (FAnimationTickLODEntry& Entry : Components)
	{
		USkeletalMeshComponent* Component = Entry.Component;
		if (!Component) continue;

		if (World && Component->GetWorld() != World) continue;

		++Stats.ManagedCount;
		ApplyLOD(Entry, ViewLocation);
	}
}

void FAnimationTickLODManager::BeginStatsFrame()
{
	Stats = FAnimationTickLODStats {};
	Stats.bValid = true;
	Stats.bEnabled = bEnabled;
	Stats.RegisteredCount = static_cast<uint32>(Components.size());
}

void FAnimationTickLODManager::RemoveInvalidComponents()
{
	Components.erase(std::remove_if(Components.begin(), Components.end(),
		[](const FAnimationTickLODEntry& Entry) { return Entry.Component == nullptr; }),
		Components.end());
}

void FAnimationTickLODManager::RecordLOD(EAnimationTickLOD LOD)
{
	const int32 Index = GetAnimationTickLODStatIndex(LOD);
	if (Index >= 0 && Index < 5)
	{
		++Stats.LODCounts[Index];
	}
}

void FAnimationTickLODManager::RecordAnimationEvaluated()
{
	++Stats.EvaluatedCount;
}

void FAnimationTickLODManager::RecordAnimationSkipped()
{
	++Stats.SkippedCount;
}

void FAnimationTickLODManager::ApplyLOD(FAnimationTickLODEntry& Entry, const FVector& ViewLocation)
{
	USkeletalMeshComponent* Component = Entry.Component;
	if (!Component) return;

	if (!bEnabled)
	{
		Component->SetEnableAnimationTickLOD(false);
		Component->SetAnimationTickLOD(EAnimationTickLOD::FullRate);
		RecordLOD(EAnimationTickLOD::FullRate);
		return;
	}

	const EAnimationTickLODPolicy Policy = Component->GetAnimationTickLODPolicy();

	if (Policy == EAnimationTickLODPolicy::Disabled)
	{
		Component->SetEnableAnimationTickLOD(false);
		Component->SetAnimationTickLOD(EAnimationTickLOD::FullRate);
		RecordLOD(EAnimationTickLOD::FullRate);
		return;
	}

	if (Policy == EAnimationTickLODPolicy::AlwaysFullRate)
	{
		Component->SetEnableAnimationTickLOD(true);
		Component->SetAnimationTickLOD(EAnimationTickLOD::FullRate);
		RecordLOD(EAnimationTickLOD::FullRate);
		return;
	}

	if (Policy == EAnimationTickLODPolicy::Manual)
	{
		Component->SetEnableAnimationTickLOD(true);
		RecordLOD(Component->GetAnimationTickLOD());
		return;
	}

	const FVector ToComponent = Component->GetWorldLocation() - ViewLocation;
	const float DistanceSq = ToComponent.Dot(ToComponent);
	const FAnimationLODSettings& Settings = FAnimationLODSettings::Get();

	const float FullDistance = Settings.GetFullRateDistance();
	const float HalfDistance = Settings.GetHalfRateDistance();
	const float QuarterDistance = Settings.GetQuarterRateDistance();
	const float LowDistance = Settings.GetLowRateDistance();

	const float FullSq = FullDistance * FullDistance;
	const float HalfSq = HalfDistance * HalfDistance;
	const float QuarterSq = QuarterDistance * QuarterDistance;
	const float LowSq = LowDistance * LowDistance;

	EAnimationTickLOD NewLOD = EAnimationTickLOD::Frozen;

	if (DistanceSq <= FullSq)
	{
		NewLOD = EAnimationTickLOD::FullRate;
	}
	else if (DistanceSq <= HalfSq)
	{
		NewLOD = EAnimationTickLOD::HalfRate;
	}
	else if (DistanceSq <= QuarterSq)
	{
		NewLOD = EAnimationTickLOD::QuarterRate;
	}
	else if (DistanceSq <= LowSq)
	{
		NewLOD = EAnimationTickLOD::LowRate;
	}

	Component->SetEnableAnimationTickLOD(true);

	if (Entry.LastLOD != NewLOD)
	{
		const float PhaseOffset = ComputePhaseOffset(Entry.PhaseSeed, NewLOD);
		Component->SetAnimationTickInitialOffset(PhaseOffset);
		Entry.LastLOD = NewLOD;
	}

	Component->SetAnimationTickLOD(NewLOD);
	RecordLOD(NewLOD);
}
