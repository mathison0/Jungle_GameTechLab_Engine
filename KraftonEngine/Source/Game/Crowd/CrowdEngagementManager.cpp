#include "Game/Crowd/CrowdEngagementManager.h"

#include <algorithm>
#include <cmath>

namespace
{
	constexpr float TwoPi = 6.28318530717958647692f;

	float DistanceSquaredXY(const FVector& A, const FVector& B)
	{
		const float DX = A.X - B.X;
		const float DY = A.Y - B.Y;
		return DX * DX + DY * DY;
	}

	float LengthSquaredXY(const FVector& V)
	{
		return V.X * V.X + V.Y * V.Y;
	}

	float NormalizeAngleRadians(float Angle)
	{
		Angle = std::fmod(Angle + 3.14159265358979323846f, TwoPi);
		if (Angle < 0.0f)
		{
			Angle += TwoPi;
		}
		return Angle - 3.14159265358979323846f;
	}

	float SlotAngle(int32 SlotIndex, int32 SlotCount, float SlotAngleOffset)
	{
		if (SlotCount <= 0)
		{
			return 0.0f;
		}

		return TwoPi * (static_cast<float>(SlotIndex) + SlotAngleOffset) / static_cast<float>(SlotCount);
	}

	FVector SlotLocation(const FVector& PlayerLocation, int32 SlotIndex, int32 SlotCount, float SlotRadius, float SlotAngleOffset)
	{
		const float Angle = SlotAngle(SlotIndex, SlotCount, SlotAngleOffset);
		return PlayerLocation + FVector(std::cos(Angle) * SlotRadius, std::sin(Angle) * SlotRadius, 0.0f);
	}

	int32 FindNearestFreeSlot(
		const FVector& UnitPosition,
		const FVector& PlayerLocation,
		const TArray<uint8>& UsedSlots,
		int32 SlotCount,
		float SlotAngleOffset)
	{
		if (SlotCount <= 0)
		{
			return -1;
		}

		const FVector ToUnit = UnitPosition - PlayerLocation;
		const float DesiredAngle = LengthSquaredXY(ToUnit) > 1.e-6f
			? std::atan2(ToUnit.Y, ToUnit.X)
			: 0.0f;

		int32 BestSlot = -1;
		float BestDelta = 1000000.0f;
		for (int32 SlotIndex = 0; SlotIndex < SlotCount; ++SlotIndex)
		{
			if (UsedSlots[SlotIndex] != 0)
			{
				continue;
			}

			const float Delta = std::abs(NormalizeAngleRadians(SlotAngle(SlotIndex, SlotCount, SlotAngleOffset) - DesiredAngle));
			if (Delta < BestDelta)
			{
				BestDelta = Delta;
				BestSlot = SlotIndex;
			}
		}

		return BestSlot;
	}
}

void FCrowdEngagementManager::Update(
	FCrowdUnitStore& UnitStore,
	const FCrowdEngagementSettings& Settings,
	const FVector& PlayerLocation,
	bool bHasPlayer) const
{
	TArray<FCrowdUnit>& Units = UnitStore.GetUnits();
	TArray<uint32> MeleeCandidates;
	TArray<uint32> RangedCandidates;

	const float EngagementRadius = (std::max)(Settings.PlayerEngagementRadius, 0.0f);
	const float EngagementExitRadius = EngagementRadius + (std::max)(Settings.PlayerEngagementExitHysteresis, 0.0f);
	const bool bCanEngagePlayer = Settings.bEnablePlayerEngagement && bHasPlayer && EngagementRadius > 0.0f;

	for (uint32 Index = 0; Index < static_cast<uint32>(Units.size()); ++Index)
	{
		FCrowdUnit& Unit = Units[Index];
		const bool bHadPlayerEngagement = Unit.TargetKind == ECrowdTargetKind::Player && Unit.bHasCombatSlot;
		const bool bHadAttackToken = Unit.TargetKind == ECrowdTargetKind::Player && Unit.bHasAttackToken;
		const int32 PreviousCombatSlotIndex = bHadPlayerEngagement ? Unit.CombatSlotIndex : -1;
		ResetPlayerEngagement(Unit);
		Unit.bHasAttackToken = bHadAttackToken;
		Unit.CombatSlotIndex = PreviousCombatSlotIndex;
		const float EffectiveEngagementRadius = bHadPlayerEngagement ? EngagementExitRadius : EngagementRadius;
		const float EffectiveEngagementRadiusSq = EffectiveEngagementRadius * EffectiveEngagementRadius;

		if (!bCanEngagePlayer
			|| Unit.Team != EUnitTeam::Enemy
			|| !IsCrowdUnitCombatActive(Unit)
			|| IsCrowdUnitControlLocked(Unit.State)
			|| DistanceSquaredXY(Unit.Position, PlayerLocation) > EffectiveEngagementRadiusSq)
		{
			Unit.bHasAttackToken = false;
			Unit.CombatSlotIndex = -1;
			continue;
		}

		if (Unit.Archetype.CombatType == EUnitCombatType::Ranged)
		{
			RangedCandidates.push_back(Index);
		}
		else
		{
			MeleeCandidates.push_back(Index);
		}
	}

	AssignCombatTypeSlots(
		Units,
		MeleeCandidates,
		PlayerLocation,
		Settings.MeleeCombatSlotCount,
		Settings.MeleeSlotRadius,
		Settings.MeleeAttackTokenCount,
		0.0f);
	AssignCombatTypeSlots(
		Units,
		RangedCandidates,
		PlayerLocation,
		Settings.RangedCombatSlotCount,
		Settings.RangedSlotRadius,
		Settings.RangedAttackTokenCount,
		0.5f);
}

void FCrowdEngagementManager::ResetPlayerEngagement(FCrowdUnit& Unit) const
{
	if (Unit.TargetKind == ECrowdTargetKind::Player)
	{
		Unit.TargetKind = ECrowdTargetKind::None;
		Unit.Target = {};
		Unit.MoveGoal = FVector::ZeroVector;
		Unit.LookAtLocation = FVector::ZeroVector;
	}

	Unit.bHasCombatSlot = false;
	Unit.bHasAttackToken = false;
	Unit.CombatSlotIndex = -1;
}

void FCrowdEngagementManager::AssignCombatTypeSlots(
	TArray<FCrowdUnit>& Units,
	TArray<uint32>& CandidateIndices,
	const FVector& PlayerLocation,
	int32 SlotCount,
	float SlotRadius,
	int32 AttackTokenCount,
	float SlotAngleOffset) const
{
	SlotCount = (std::max)(SlotCount, 0);
	AttackTokenCount = (std::max)(AttackTokenCount, 0);
	SlotRadius = (std::max)(SlotRadius, 0.0f);
	if (SlotCount <= 0 || CandidateIndices.empty())
	{
		for (uint32 UnitIndex : CandidateIndices)
		{
			if (UnitIndex < Units.size())
			{
				Units[UnitIndex].bHasAttackToken = false;
			}
		}
		return;
	}

	std::sort(CandidateIndices.begin(), CandidateIndices.end(), [&Units, &PlayerLocation](uint32 A, uint32 B)
	{
		return DistanceSquaredXY(Units[A].Position, PlayerLocation) < DistanceSquaredXY(Units[B].Position, PlayerLocation);
	});

	TArray<uint8> UsedSlots(static_cast<size_t>(SlotCount), 0);
	TArray<uint32> AssignedUnits;
	AssignedUnits.reserve((std::min)(CandidateIndices.size(), static_cast<size_t>(SlotCount)));

	auto AssignUnitToSlot = [&Units, &UsedSlots, &AssignedUnits, PlayerLocation, SlotCount, SlotRadius, SlotAngleOffset](
		uint32 UnitIndex,
		int32 SlotIndex) -> bool
	{
		if (UnitIndex >= Units.size()
			|| SlotIndex < 0
			|| SlotIndex >= SlotCount
			|| UsedSlots[SlotIndex] != 0
			|| AssignedUnits.size() >= static_cast<size_t>(SlotCount))
		{
			return false;
		}

		FCrowdUnit& Unit = Units[UnitIndex];
		const bool bHadCombatSlot = Unit.CombatSlotIndex >= 0 && Unit.CombatSlotIndex < SlotCount;
		if (!bHadCombatSlot)
		{
			Unit.CircleAroundDirectionSign = ((UnitIndex + static_cast<uint32>(SlotIndex)) % 2u == 0u) ? 1.0f : -1.0f;
		}

		UsedSlots[SlotIndex] = 1;
		Unit.TargetKind = ECrowdTargetKind::Player;
		Unit.Target = {};
		Unit.bHasCombatSlot = true;
		Unit.CombatSlotIndex = SlotIndex;
		Unit.MoveGoal = SlotLocation(PlayerLocation, SlotIndex, SlotCount, SlotRadius, SlotAngleOffset);
		Unit.LookAtLocation = PlayerLocation;
		AssignedUnits.push_back(UnitIndex);
		return true;
	};

	for (uint32 UnitIndex : CandidateIndices)
	{
		if (UnitIndex >= Units.size())
		{
			continue;
		}

		FCrowdUnit& Unit = Units[UnitIndex];
		const int32 PreviousSlotIndex = Unit.CombatSlotIndex;
		if (PreviousSlotIndex >= 0 && PreviousSlotIndex < SlotCount)
		{
			AssignUnitToSlot(UnitIndex, PreviousSlotIndex);
		}
	}

	for (uint32 UnitIndex : CandidateIndices)
	{
		if (AssignedUnits.size() >= static_cast<size_t>(SlotCount) || UnitIndex >= Units.size())
		{
			if (UnitIndex < Units.size() && !Units[UnitIndex].bHasCombatSlot)
			{
				Units[UnitIndex].bHasAttackToken = false;
				Units[UnitIndex].CombatSlotIndex = -1;
			}
			continue;
		}

		FCrowdUnit& Unit = Units[UnitIndex];
		if (Unit.bHasCombatSlot)
		{
			continue;
		}

		const int32 SlotIndex = FindNearestFreeSlot(Unit.Position, PlayerLocation, UsedSlots, SlotCount, SlotAngleOffset);
		if (!AssignUnitToSlot(UnitIndex, SlotIndex))
		{
			Unit.bHasAttackToken = false;
			Unit.CombatSlotIndex = -1;
		}
	}

	std::sort(AssignedUnits.begin(), AssignedUnits.end(), [&Units, &PlayerLocation](uint32 A, uint32 B)
	{
		const FCrowdUnit& UnitA = Units[A];
		const FCrowdUnit& UnitB = Units[B];
		if (UnitA.bHasAttackToken != UnitB.bHasAttackToken)
		{
			return UnitA.bHasAttackToken;
		}
		return DistanceSquaredXY(UnitA.Position, PlayerLocation) < DistanceSquaredXY(UnitB.Position, PlayerLocation);
	});

	int32 GrantedTokens = 0;
	for (uint32 UnitIndex : AssignedUnits)
	{
		FCrowdUnit& Unit = Units[UnitIndex];
		const bool bGrantToken = GrantedTokens < AttackTokenCount;
		Unit.bHasAttackToken = bGrantToken;
		if (bGrantToken)
		{
			++GrantedTokens;
		}
	}
}
