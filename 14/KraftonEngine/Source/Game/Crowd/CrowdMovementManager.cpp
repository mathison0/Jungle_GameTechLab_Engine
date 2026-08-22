#include "Game/Crowd/CrowdMovementManager.h"

#include <algorithm>
#include <cmath>

namespace
{
	constexpr float RadToDeg = 57.295779513082320876f;
	constexpr float GoldenAngleRadians = 2.39996322972865332f;
	constexpr float FriendlyBlockedMovingSpeedThresholdSq = 0.01f;

	FVector ToXY(const FVector& V)
	{
		return FVector(V.X, V.Y, 0.0f);
	}

	float LengthSquaredXY(const FVector& V)
	{
		return V.X * V.X + V.Y * V.Y;
	}

	FVector NormalizedXY(const FVector& V)
	{
		const float LenSq = LengthSquaredXY(V);
		if (LenSq <= 1.e-6f)
		{
			return FVector::ZeroVector;
		}

		const float InvLen = 1.0f / std::sqrt(LenSq);
		return FVector(V.X * InvLen, V.Y * InvLen, 0.0f);
	}

	FVector ClampLengthXY(const FVector& V, float MaxLength)
	{
		const float LenSq = LengthSquaredXY(V);
		const float MaxLen = (std::max)(MaxLength, 0.0f);
		if (LenSq <= MaxLen * MaxLen || LenSq <= 1.e-6f)
		{
			return V;
		}

		return NormalizedXY(V) * MaxLen;
	}

	FVector DeterministicUnitDirectionXY(uint32 UnitIndex)
	{
		const float Angle = static_cast<float>(UnitIndex) * GoldenAngleRadians;
		return FVector(std::cos(Angle), std::sin(Angle), 0.0f);
	}

	float DistanceSquaredXY(const FVector& A, const FVector& B)
	{
		const float DX = A.X - B.X;
		const float DY = A.Y - B.Y;
		return DX * DX + DY * DY;
	}

	bool IsSoftFriendlyChaseBlocker(
		const FCrowdUnit& Unit,
		const FCrowdUnit& Other,
		const FCrowdMovementSettings& Settings)
	{
		return Settings.bTreatFriendlyChaseBlockersAsSoft && Unit.Team == Other.Team;
	}

	float ChaseBlockOccupancyWeight(
		const FCrowdUnit& Unit,
		const FCrowdUnit& Other,
		const FCrowdMovementSettings& Settings)
	{
		return IsSoftFriendlyChaseBlocker(Unit, Other, Settings)
			? (std::max)(Settings.FriendlyChaseBlockScoreScale, 0.0f)
			: 1.0f;
	}

	FRotator RotationFromDirectionXY(const FVector& Direction)
	{
		if (LengthSquaredXY(Direction) <= 1.e-6f)
		{
			return FRotator::ZeroRotator;
		}

		return FRotator(0.0f, std::atan2(Direction.Y, Direction.X) * RadToDeg, 0.0f);
	}

	float NormalizeYawDegrees(float Angle)
	{
		Angle = std::fmod(Angle + 180.0f, 360.0f);
		if (Angle < 0.0f)
		{
			Angle += 360.0f;
		}

		return Angle - 180.0f;
	}

	float StepYawTowards(float CurrentYaw, float TargetYaw, float MaxStepDegrees)
	{
		const float DeltaYaw = NormalizeYawDegrees(TargetYaw - CurrentYaw);
		const float ClampedStep = (std::max)(MaxStepDegrees, 0.0f);
		const float Step = (std::max)(-ClampedStep, (std::min)(DeltaYaw, ClampedStep));
		return NormalizeYawDegrees(CurrentYaw + Step);
	}

	FRotator StepRotationTowardsDirectionXY(
		const FRotator& CurrentRotation,
		const FVector& Direction,
		float DeltaTime,
		float TurnSpeedDegreesPerSecond)
	{
		if (LengthSquaredXY(Direction) <= 1.e-6f)
		{
			return FRotator(0.0f, CurrentRotation.Yaw, 0.0f);
		}

		const float TargetYaw = RotationFromDirectionXY(Direction).Yaw;
		const float TurnSpeed = (std::max)(TurnSpeedDegreesPerSecond, 1.0f);
		const float MaxStepDegrees = TurnSpeed * (std::max)(DeltaTime, 0.0f);
		return FRotator(0.0f, StepYawTowards(CurrentRotation.Yaw, TargetYaw, MaxStepDegrees), 0.0f);
	}
}

void FCrowdMovementManager::Update(
	float DeltaTime,
	FCrowdUnitStore& UnitStore,
	const FCrowdSpatialPartition& SpatialPartition,
	FCrowdGroundQuery& GroundQuery,
	const FCrowdMovementSettings& Settings) const
{
	if (DeltaTime <= 0.0f)
	{
		return;
	}

	TArray<FCrowdUnit>& Units = UnitStore.GetUnits();
	TArray<uint32> Neighbors;
	float MaxAliveRadius = 0.0f;
	for (const FCrowdUnit& Unit : Units)
	{
		if (IsCrowdUnitCombatActive(Unit))
		{
			MaxAliveRadius = (std::max)(MaxAliveRadius, Unit.Radius);
		}
	}

	// 띄움 상태 — Z 포물선 적분 + 착지 판정. 지면 스냅은 착지 전까지 생략.
	// Hit/KnockDown(저글) 과 Dead(시체 낙하) 가 공유한다.
	auto IntegrateAirborneFall = [&GroundQuery, &Settings](FCrowdUnit& Unit, float Dt)
	{
		Unit.Position.Z += Unit.AirborneVelZ * Dt;
		Unit.AirborneVelZ -= Settings.LaunchGravity * Dt;

		if (Unit.AirborneVelZ < 0.0f)
		{
			// 하강 중 — 지면을 찾아 착지 체크. 못 찾으면 SpawnZ 를 바닥으로.
			FCrowdGroundSampleParams Params;
			Params.TraceUp = Settings.GroundTraceUp;
			Params.TraceDown = Settings.GroundTraceDown;
			Params.HeightOffset = Settings.GroundHeightOffset;

			float FloorZ = Unit.SpawnZ;
			FCrowdGroundHit GroundHit;
			if (GroundQuery.SampleGround(Unit.Position, Params, GroundHit))
			{
				FloorZ = GroundHit.Location.Z;
			}

			if (Unit.Position.Z <= FloorZ)
			{
				Unit.Position.Z = FloorZ;
				Unit.bAirborne = false;
				Unit.AirborneVelZ = 0.0f;
			}
		}
	};

	for (uint32 Index = 0; Index < static_cast<uint32>(Units.size()); ++Index)
	{
		FCrowdUnit& Unit = Units[Index];
		if (!Unit.bAlive)
		{
			continue;
		}

		if (Unit.State == EUnitState::Dead)
		{
			Unit.Velocity = FVector::ZeroVector;
			Unit.FriendlyBlockedTime = 0.0f;

			// 공중 사망 시체 낙하 — 저글 킬이 공중에 떠 있지 않게 착지까지 떨어뜨린다.
			if (Unit.bAirborne)
			{
				IntegrateAirborneFall(Unit, DeltaTime);
			}
			continue;
		}

		if (!ShouldSimulateCrowdUnitThisFrame(Unit))
		{
			continue;
		}

		const float UnitDeltaTime = Unit.SimulationDeltaTime > 0.0f ? Unit.SimulationDeltaTime : DeltaTime;

		if (Unit.State == EUnitState::Hit || Unit.State == EUnitState::KnockDown)
		{
			Unit.FriendlyBlockedTime = 0.0f;
			const float KnockbackStep = (std::min)(UnitDeltaTime, (std::max)(Unit.KnockbackTimeRemaining, 0.0f));
			if (KnockbackStep > 0.0f && LengthSquaredXY(Unit.KnockbackVelocity) > 1.e-6f)
			{
				Unit.Position += Unit.KnockbackVelocity * KnockbackStep;
				Unit.Velocity = Unit.KnockbackVelocity;
				Unit.KnockbackTimeRemaining = (std::max)(Unit.KnockbackTimeRemaining - UnitDeltaTime, 0.0f);
			}
			else
			{
				Unit.Velocity = FVector::ZeroVector;
				Unit.KnockbackTimeRemaining = 0.0f;
			}

			// (상태 만료는 UpdateStateTimers 가 bAirborne 동안 보류 — 착지 후 소화)
			if (Unit.bAirborne)
			{
				IntegrateAirborneFall(Unit, UnitDeltaTime);
				continue;
			}

			ApplySurfaceFollowing(Unit, GroundQuery, Settings);
			continue;
		}

		const FUnitArchetype& Archetype = Unit.Archetype;
		FVector Desired = FVector::ZeroVector;
		FVector FacingDir = FVector::ZeroVector;
		float MoveSpeedScale = 1.0f;
		bool bFriendlyBlockAhead = false;
		if (Unit.TargetKind == ECrowdTargetKind::Player)
		{
			FacingDir = NormalizedXY(Unit.LookAtLocation - Unit.Position);
			if (Unit.State == EUnitState::Move)
			{
				Desired = NormalizedXY(Unit.MoveGoal - Unit.Position);
			}
			else if (Unit.State == EUnitState::Chase)
			{
				Desired = FacingDir;
			}
			else if (Unit.State == EUnitState::CircleAround && LengthSquaredXY(FacingDir) > 1.e-6f)
			{
				FVector RadialDir = NormalizedXY(Unit.Position - Unit.LookAtLocation);
				if (LengthSquaredXY(RadialDir) <= 1.e-6f)
				{
					RadialDir = NormalizedXY(Unit.MoveGoal - Unit.LookAtLocation);
				}
				if (LengthSquaredXY(RadialDir) <= 1.e-6f)
				{
					RadialDir = FVector(1.0f, 0.0f, 0.0f);
				}

				const float DirectionSign = Unit.CircleAroundDirectionSign >= 0.0f ? 1.0f : -1.0f;
				Desired = FVector(-RadialDir.Y, RadialDir.X, 0.0f) * DirectionSign;

				const float TargetRadius = std::sqrt(LengthSquaredXY(Unit.MoveGoal - Unit.LookAtLocation));
				const float CurrentRadius = std::sqrt(LengthSquaredXY(Unit.Position - Unit.LookAtLocation));
				const float RadiusError = TargetRadius - CurrentRadius;
				const float RadiusTolerance = (std::max)(Settings.CircleAroundRadiusTolerance, 0.0f);
				const float CorrectionWeight = (std::max)(Settings.CircleAroundRadialCorrectionWeight, 0.0f);
				if (std::abs(RadiusError) > RadiusTolerance && CorrectionWeight > 0.0f)
				{
					const float CorrectionSign = RadiusError > 0.0f ? 1.0f : -1.0f;
					Desired += RadialDir * CorrectionSign * CorrectionWeight;
				}

				Desired = NormalizedXY(Desired);
				MoveSpeedScale = (std::max)(Settings.CircleAroundSpeedScale, 0.0f);
			}
		}
		else if (Unit.State == EUnitState::Move)
		{
			Desired = NormalizedXY(Unit.MoveGoal - Unit.Position);
			FacingDir = Desired;
		}
		else if (Unit.State == EUnitState::Chase || Unit.State == EUnitState::Attack || Unit.State == EUnitState::CircleAround)
		{
			if (const FCrowdUnit* Target = UnitStore.ResolveUnit(Unit.Target))
			{
				if (IsCrowdUnitCombatActive(*Target))
				{
					FacingDir = NormalizedXY(Target->Position - Unit.Position);
					Desired = FacingDir;
					if (Unit.State == EUnitState::CircleAround)
					{
						Desired = FVector(-FacingDir.Y, FacingDir.X, 0.0f);
					}
				}
				else
				{
					Unit.State = EUnitState::Idle;
				}
			}
			else
			{
				Unit.State = EUnitState::Idle;
			}
		}

		if (LengthSquaredXY(FacingDir) > 1.e-6f)
		{
			Unit.Rotation = StepRotationTowardsDirectionXY(
				Unit.Rotation,
				FacingDir,
				UnitDeltaTime,
				Settings.VisualTurnSpeedDegreesPerSecond);
		}

		FVector Separation = FVector::ZeroVector;
		if (Archetype.SeparationRadius > 0.0f && Archetype.SeparationWeight > 0.0f)
		{
			SpatialPartition.QueryUnitsInRadius(Units, Unit.Position, Archetype.SeparationRadius, Neighbors);
			for (uint32 OtherIndex : Neighbors)
			{
				if (OtherIndex == Index || OtherIndex >= Units.size())
				{
					continue;
				}

				const FCrowdUnit& Other = Units[OtherIndex];
				if (!IsCrowdUnitCombatActive(Other))
				{
					continue;
				}

				const FVector Away = ToXY(Unit.Position - Other.Position);
				const float DistSq = LengthSquaredXY(Away);
				if (DistSq <= 1.e-6f)
				{
					Separation += DeterministicUnitDirectionXY(Index);
					continue;
				}

				const float Dist = std::sqrt(DistSq);
				const float Strength = (std::max)(Archetype.SeparationRadius - Dist, 0.0f) / Archetype.SeparationRadius;
				Separation += (Away / Dist) * Strength;
			}
		}

		FVector PlayerSeparation = FVector::ZeroVector;
		if (Settings.bEnablePlayerSeparation
			&& Settings.bHasPlayerSeparationTarget
			&& Settings.PlayerSeparationWeight > 0.0f)
		{
			const float EffectiveRadius = (std::max)(
				Settings.PlayerProxyRadius + Unit.Radius + Settings.PlayerSeparationPadding,
				0.0f);
			const FVector AwayFromPlayer = ToXY(Unit.Position - Settings.PlayerSeparationLocation);
			const float DistSq = LengthSquaredXY(AwayFromPlayer);
			if (EffectiveRadius > 0.0f && DistSq < EffectiveRadius * EffectiveRadius)
			{
				if (DistSq > 1.e-6f)
				{
					const float Dist = std::sqrt(DistSq);
					const float Strength = (EffectiveRadius - Dist) / EffectiveRadius;
					PlayerSeparation = (AwayFromPlayer / Dist) * Strength;
				}
				else
				{
					PlayerSeparation = DeterministicUnitDirectionXY(Index);
				}
			}
		}

		if (Settings.bWaitWhenChaseBlocked && Unit.State == EUnitState::Chase && LengthSquaredXY(Desired) > 1.e-6f)
		{
			bool bCurrentlyOverlapping = false;
			const float CurrentOverlapQueryRadius = (std::max)(Unit.Radius + MaxAliveRadius, 0.0f);
			SpatialPartition.QueryUnitsInRadius(Units, Unit.Position, CurrentOverlapQueryRadius, Neighbors);
			for (uint32 OtherIndex : Neighbors)
			{
				if (OtherIndex == Index || OtherIndex >= Units.size())
				{
					continue;
				}

				const FCrowdUnit& Other = Units[OtherIndex];
				if (!IsCrowdUnitCombatActive(Other))
				{
					continue;
				}

				const float OverlapRadius = Unit.Radius + Other.Radius;
				if (OverlapRadius > 0.0f && DistanceSquaredXY(Unit.Position, Other.Position) < OverlapRadius * OverlapRadius)
				{
					bCurrentlyOverlapping = true;
					break;
				}
			}

			if (!bCurrentlyOverlapping)
			{
				const float ClearancePadding = (std::max)(Settings.ChaseBlockedClearancePadding, 0.0f);
				const float MinProbeDistance = (std::max)(Settings.ChaseBlockedProbeDistance, 0.0f);
				const float ProbeDistance = (std::max)(MinProbeDistance, Archetype.MoveSpeed * UnitDeltaTime);
				const FVector ProbePosition = Unit.Position + Desired * ProbeDistance;
				const float ProbeQueryRadius = (std::max)(Unit.Radius + MaxAliveRadius + ClearancePadding, 0.0f);

				float ForwardHardOccupancyScore = 0.0f;
				float ForwardSoftOccupancyScore = 0.0f;
				SpatialPartition.QueryUnitsInRadius(Units, ProbePosition, ProbeQueryRadius, Neighbors);
				for (uint32 OtherIndex : Neighbors)
				{
					if (OtherIndex == Index || OtherIndex >= Units.size())
					{
						continue;
					}

					const FCrowdUnit& Other = Units[OtherIndex];
					if (!IsCrowdUnitCombatActive(Other))
					{
						continue;
					}

					const float BlockedRadius = Unit.Radius + Other.Radius + ClearancePadding;
					if (BlockedRadius > 0.0f && DistanceSquaredXY(ProbePosition, Other.Position) <= BlockedRadius * BlockedRadius)
					{
						const float BlockedRadiusSq = BlockedRadius * BlockedRadius;
						const float DistSq = DistanceSquaredXY(ProbePosition, Other.Position);
						const float Occupancy = (BlockedRadiusSq - DistSq) / BlockedRadiusSq;
						if (IsSoftFriendlyChaseBlocker(Unit, Other, Settings))
						{
							const float FriendlyWeight = ChaseBlockOccupancyWeight(Unit, Other, Settings);
							if (FriendlyWeight > 0.0f)
							{
								ForwardSoftOccupancyScore += Occupancy * FriendlyWeight;
								bFriendlyBlockAhead = true;
							}
						}
						else
						{
							ForwardHardOccupancyScore += Occupancy;
						}
					}
				}

				const float ForwardOccupancyScore = ForwardHardOccupancyScore + ForwardSoftOccupancyScore;
				if (ForwardOccupancyScore > 0.0f)
				{
					bool bHasSideStep = false;
					FVector SideStepDirection = FVector::ZeroVector;
					float BestSideStepScore = ForwardOccupancyScore;

					if (Settings.bEnableChaseBlockedSideStep)
					{
						auto EvaluateSideStep = [&](const FVector& CandidateDirection)
						{
							if (LengthSquaredXY(CandidateDirection) <= 1.e-6f)
							{
								return;
							}

							const FVector CandidatePosition = Unit.Position + CandidateDirection * ProbeDistance;
							float CandidateScore = 0.0f;
							SpatialPartition.QueryUnitsInRadius(Units, CandidatePosition, ProbeQueryRadius, Neighbors);
							for (uint32 OtherIndex : Neighbors)
							{
								if (OtherIndex == Index || OtherIndex >= Units.size())
								{
									continue;
								}

								const FCrowdUnit& Other = Units[OtherIndex];
								if (!IsCrowdUnitCombatActive(Other))
								{
									continue;
								}

								const float BlockedRadius = Unit.Radius + Other.Radius + ClearancePadding;
								const float BlockedRadiusSq = BlockedRadius * BlockedRadius;
								if (BlockedRadiusSq <= 0.0f)
								{
									continue;
								}

								const float DistSq = DistanceSquaredXY(CandidatePosition, Other.Position);
								if (DistSq <= BlockedRadiusSq)
								{
									const float Occupancy = (BlockedRadiusSq - DistSq) / BlockedRadiusSq;
									CandidateScore += Occupancy * ChaseBlockOccupancyWeight(Unit, Other, Settings);
								}
							}

							if (CandidateScore < BestSideStepScore)
							{
								BestSideStepScore = CandidateScore;
								SideStepDirection = CandidateDirection;
								bHasSideStep = true;
							}
						};

						const FVector Right(-Desired.Y, Desired.X, 0.0f);
						EvaluateSideStep(Right);
						EvaluateSideStep(Right * -1.0f);
					}

					if (!bHasSideStep)
					{
						if (ForwardHardOccupancyScore > 0.0f)
						{
							Unit.Velocity = FVector::ZeroVector;
							Unit.FriendlyBlockedTime = 0.0f;
							ApplySurfaceFollowing(Unit, GroundQuery, Settings);
							continue;
						}
					}
					else
					{
						Desired = SideStepDirection;
						MoveSpeedScale *= (std::max)(Settings.ChaseBlockedSideStepSpeedScale, 0.0f);
					}
				}
			}
		}

		const float SeparationDeadZone = (std::max)(Settings.SeparationDeadZone, 0.0f);
		const bool bHasUnitSeparation = LengthSquaredXY(Separation) > SeparationDeadZone * SeparationDeadZone;
		const bool bHasPlayerSeparation = LengthSquaredXY(PlayerSeparation) > SeparationDeadZone * SeparationDeadZone;
		const bool bHasSeparationInfluence = bHasUnitSeparation || bHasPlayerSeparation;
		const bool bHasIntentionalMove = Unit.State != EUnitState::Attack && LengthSquaredXY(Desired) > 1.e-6f;
		FVector MoveDir = Desired;
		if (Unit.State == EUnitState::Attack)
		{
			if (Settings.bEnableAttackSeparation)
			{
				MoveDir = FVector::ZeroVector;
				if (bHasUnitSeparation)
				{
					MoveDir += NormalizedXY(Separation) * Archetype.SeparationWeight;
				}
				if (bHasPlayerSeparation)
				{
					MoveDir += PlayerSeparation * Settings.PlayerSeparationWeight;
				}
				MoveSpeedScale *= (std::max)(Settings.AttackSeparationSpeedScale, 0.0f);
			}
			else
			{
				MoveDir = bHasPlayerSeparation ? PlayerSeparation * Settings.PlayerSeparationWeight : FVector::ZeroVector;
			}
		}
		else if (bHasUnitSeparation)
		{
			MoveDir += NormalizedXY(Separation) * Archetype.SeparationWeight;
		}
		if (Unit.State != EUnitState::Attack && bHasPlayerSeparation)
		{
			MoveDir += PlayerSeparation * Settings.PlayerSeparationWeight;
		}

		MoveDir = NormalizedXY(MoveDir);
		if (LengthSquaredXY(MoveDir) <= 1.e-6f)
		{
			Unit.Velocity = FVector::ZeroVector;
			if (Unit.State == EUnitState::Chase && bFriendlyBlockAhead)
			{
				Unit.FriendlyBlockedTime += UnitDeltaTime;
			}
			else
			{
				Unit.FriendlyBlockedTime = 0.0f;
			}
			ApplySurfaceFollowing(Unit, GroundQuery, Settings);
			continue;
		}

		if (Unit.State != EUnitState::Attack && !bHasIntentionalMove && bHasSeparationInfluence)
		{
			MoveSpeedScale *= (std::max)(Settings.SeparationOnlySpeedScale, 0.0f);
		}

		FVector TargetVelocity = MoveDir * Archetype.MoveSpeed * MoveSpeedScale;
		if (Settings.bEnableSeparationVelocitySmoothing && bHasSeparationInfluence)
		{
			const float BlendSpeed = (std::max)(Settings.SeparationVelocityBlendSpeed, 0.0f);
			const float BlendAlpha = BlendSpeed > 0.0f
				? (std::min)(BlendSpeed * UnitDeltaTime, 1.0f)
				: 1.0f;
			const FVector SmoothedVelocity = Unit.Velocity + (TargetVelocity - Unit.Velocity) * BlendAlpha;
			Unit.Velocity = ClampLengthXY(SmoothedVelocity, std::sqrt(LengthSquaredXY(TargetVelocity)));
		}
		else
		{
			Unit.Velocity = TargetVelocity;
		}
		if (Unit.State == EUnitState::Chase
			&& bFriendlyBlockAhead
			&& LengthSquaredXY(Unit.Velocity) <= FriendlyBlockedMovingSpeedThresholdSq)
		{
			Unit.FriendlyBlockedTime += UnitDeltaTime;
		}
		else
		{
			Unit.FriendlyBlockedTime = 0.0f;
		}
		Unit.Position += Unit.Velocity * UnitDeltaTime;
		ApplySurfaceFollowing(Unit, GroundQuery, Settings);
	}
}

void FCrowdMovementManager::ApplySurfaceFollowing(
	FCrowdUnit& Unit,
	FCrowdGroundQuery& GroundQuery,
	const FCrowdMovementSettings& Settings) const
{
	if (!Settings.bSurfaceFollowingEnabled || !Unit.bAlive || !GroundQuery.HasData())
	{
		return;
	}

	FCrowdGroundSampleParams Params;
	Params.TraceUp = Settings.GroundTraceUp;
	Params.TraceDown = Settings.GroundTraceDown;
	Params.HeightOffset = Settings.GroundHeightOffset;

	FCrowdGroundHit Hit;
	if (GroundQuery.SampleGround(Unit.Position, Params, Hit))
	{
		Unit.Position.Z = Hit.Location.Z;
		Unit.GroundNormal = Hit.Normal;
		Unit.GroundMissFrames = 0;
		Unit.bHasGround = true;
		return;
	}

	++Unit.GroundMissFrames;
	if (Unit.GroundMissFrames > Settings.GroundMissToleranceFrames && !Unit.bHasGround)
	{
		Unit.Position.Z = Unit.SpawnZ;
	}
}
