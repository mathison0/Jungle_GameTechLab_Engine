#include "Game/Musou/Combat/AnimNotify_MusouAttack.h"

#include "Game/Musou/Combat/AttackTypes.h"
#include "Game/Musou/Combat/BattleComponent.h"
#include "Game/Musou/GameMode/MusouGameMode.h"
#include "Component/Primitive/SkeletalMeshComponent.h"
#include "Core/Types/EngineTypes.h"
#include "Debug/DrawDebugHelpers.h"
#include "GameFramework/Pawn/Pawn.h"
#include "GameFramework/World.h"
#include "Core/Logging/Log.h"

#include <cmath>

void UAnimNotify_MusouAttack::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Anim)
{
	(void)Anim;

	if (!MeshComp)
	{
		return;
	}

	AActor* OwnerActor = MeshComp->GetOwner();
	if (!OwnerActor)
	{
		return;
	}

	const FAttackSpec* Spec = FindMusouAttackSpec(FName(AttackId));
	if (!Spec)
	{
		UE_LOG("[MusouAttack] unknown AttackId '%s' — AttackTypes.h 테이블 확인", AttackId.c_str());
		return;
	}

	UWorld* World = OwnerActor->GetWorld();
	AMusouGameMode* GameMode = World ? Cast<AMusouGameMode>(World->GetGameMode()) : nullptr;
	if (!GameMode)
	{
		// Editor 프리뷰 / GameMode 미사용 월드 — 조용히 무시
		return;
	}

	APawn* AttackerPawn = Cast<APawn>(OwnerActor);

	// 공격력 — BattleComponent가 있으면 사용, 없으면 기본값
	float AttackPower = 10.0f;
	if (UBattleComponent* Battle = OwnerActor->GetComponentByClass<UBattleComponent>())
	{
		AttackPower = Battle->GetAttackPower();
	}

	FMusouAttackEvent Event;
	Event.Attacker = AttackerPawn;
	Event.Spec = *Spec;
	Event.Origin = OwnerActor->GetActorLocation();
	Event.Forward = OwnerActor->GetActorForward();
	Event.Damage = AttackPower * Spec->DamageMult;
	Event.bFromPlayer = AttackerPawn && AttackerPawn->IsPossessed();

	if (bDrawDebug)
	{
		DrawAttackVolume(World, Event);
	}

	GameMode->BroadcastAttack(Event);
}

// 판정 범위 시각화 — 360도는 원, 콘은 부채꼴(호 + 양 끝 변).
// 수직 허용 범위(±Height)도 위/아래 라인으로 표시한다.
void UAnimNotify_MusouAttack::DrawAttackVolume(UWorld* World, const FMusouAttackEvent& Event) const
{
	if (!World)
	{
		return;
	}

	const FColor Color(
		static_cast<uint32>(DebugColor.X * 255.0f),
		static_cast<uint32>(DebugColor.Y * 255.0f),
		static_cast<uint32>(DebugColor.Z * 255.0f),
		static_cast<uint32>(DebugColor.W * 255.0f));

	const FVector Origin = Event.Origin;
	const FVector Forward = Event.Forward;
	const FVector Up(0.0f, 0.0f, 1.0f);
	FVector Right = Forward.Cross(Up);
	if (Right.Length() < 0.001f)
	{
		Right = FVector(0.0f, 1.0f, 0.0f);
	}
	Right.Normalize();

	const float Range = Event.Spec.Range;
	const float Height = Event.Spec.Height;
	const bool bFullCircle = Event.Spec.ConeCos <= -1.0f;
	const float HalfAngle = bFullCircle ? 3.14159265f : std::acos(Event.Spec.ConeCos);

	constexpr int32 Segments = 24;

	// 부채꼴 호 — 전방 기준 -HalfAngle ~ +HalfAngle
	auto PointAt = [&](float Angle, float ZOffset)
	{
		return Origin
			+ Forward * (std::cos(Angle) * Range)
			+ Right * (std::sin(Angle) * Range)
			+ Up * ZOffset;
	};

	FVector PrevTop = PointAt(-HalfAngle, Height);
	FVector PrevBottom = PointAt(-HalfAngle, -Height);
	for (int32 i = 1; i <= Segments; ++i)
	{
		const float Angle = -HalfAngle + (2.0f * HalfAngle) * (static_cast<float>(i) / Segments);
		const FVector Top = PointAt(Angle, Height);
		const FVector Bottom = PointAt(Angle, -Height);

		DrawDebugLine(World, PrevTop, Top, Color, DebugDrawDuration);
		DrawDebugLine(World, PrevBottom, Bottom, Color, DebugDrawDuration);

		PrevTop = Top;
		PrevBottom = Bottom;
	}

	if (!bFullCircle)
	{
		// 콘 양 끝 변 — 중심에서 호 끝까지
		DrawDebugLine(World, Origin, PointAt(-HalfAngle, 0.0f), Color, DebugDrawDuration);
		DrawDebugLine(World, Origin, PointAt(HalfAngle, 0.0f), Color, DebugDrawDuration);
	}

	// 전방 표시 — 중심에서 전방 가장자리까지
	DrawDebugLine(World, Origin, PointAt(0.0f, 0.0f), Color, DebugDrawDuration);
}
