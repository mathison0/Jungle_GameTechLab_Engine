#include "Game/Musou/Combat/BattleComponent.h"

#include "Game/Musou/Combat/AttackTypes.h"
#include "Game/Musou/GameMode/MusouGameMode.h"
#include "Component/Input/ActionComponent.h"
#include "GameFramework/Pawn/Pawn.h"
#include "GameFramework/World.h"
#include "Core/Logging/Log.h"

static AMusouGameMode* GetMusouGameModeFor(const UActorComponent* Component)
{
	UWorld* World = Component ? Component->GetWorld() : nullptr;
	return World ? Cast<AMusouGameMode>(World->GetGameMode()) : nullptr;
}

void UBattleComponent::BeginPlay()
{
	UActorComponent::BeginPlay();

	Health = MaxHealth;
	bDead = false;

	// 공격 이벤트 구독 — 단일 피격 경로. (Editor 월드 등 GameMode 없으면 스킵)
	if (AMusouGameMode* GameMode = GetMusouGameModeFor(this))
	{
		AttackListenerHandle = GameMode->OnAttackPerformed.AddUObject(this, &UBattleComponent::HandleAttackEvent);
	}
}

void UBattleComponent::EndPlay()
{
	if (AttackListenerHandle.IsValid())
	{
		if (AMusouGameMode* GameMode = GetMusouGameModeFor(this))
		{
			GameMode->OnAttackPerformed.Remove(AttackListenerHandle);
		}
		AttackListenerHandle.Reset();
	}

	UActorComponent::EndPlay();
}

void UBattleComponent::HandleAttackEvent(const FMusouAttackEvent& Event)
{
	if (bDead)
	{
		return;
	}

	AActor* OwnerActor = GetOwner();
	if (!OwnerActor)
	{
		return;
	}

	// 자기 자신의 공격 / 같은 진영 공격은 무시
	if (Event.Attacker == OwnerActor || Event.bFromPlayer == bIsPlayerTeam)
	{
		return;
	}

	// 셀프 판정 — 군체 Manager와 동일한 공용 기하 사용
	const FVector MyPos = OwnerActor->GetActorLocation();
	if (!Event.IsInVolume(MyPos))
	{
		return;
	}

	if (AMusouGameMode* GameMode = GetMusouGameModeFor(this))
	{
		GameMode->NotifyAttackHits(Event, 1);
	}

	ApplyDamage(Event.Damage, Event.Attacker);

	// 넉백 — 공격자에서 멀어지는 수평 방향
	if (bAcceptKnockback && !bDead)
	{
		if (UActionComponent* Action = OwnerActor->GetComponentByClass<UActionComponent>())
		{
			FVector Away = MyPos - Event.Origin;
			Away.Z = 0.0f;
			if (Away.Length() > 0.001f)
			{
				Action->Knockback(Away.Normalized(), Event.Spec.KnockbackDist, Event.Spec.KnockbackDur);
			}
		}
	}

}

float UBattleComponent::ApplyDamage(float Damage, AActor* DamageInstigator)
{
	if (bDead || Damage <= 0.0f)
	{
		return 0.0f;
	}

	const float Applied = (Damage > Health) ? Health : Damage;
	Health -= Applied;

	// TODO: 피격 리액션 (React Large 애님), 무적 시간

	if (Health <= 0.0f)
	{
		Health = 0.0f;
		bDead = true;
		OnDeath(DamageInstigator);
	}

	return Applied;
}

void UBattleComponent::Heal(float Amount)
{
	if (bDead || Amount <= 0.0f)
	{
		return;
	}

	Health = (Health + Amount > MaxHealth) ? MaxHealth : Health + Amount;
}

void UBattleComponent::Kill(AActor* DamageInstigator)
{
	ApplyDamage(Health, DamageInstigator);
}

void UBattleComponent::OnDeath(AActor* DamageInstigator)
{
	AMusouGameMode* GameMode = GetMusouGameModeFor(this);
	if (!GameMode)
	{
		return;
	}

	// 소유자가 플레이어(Possessed Pawn)면 사망, 아니면 적 처치로 통지.
	APawn* OwnerPawn = Cast<APawn>(GetOwner());
	if (OwnerPawn && OwnerPawn->IsPossessed())
	{
		GameMode->NotifyPlayerDeath(OwnerPawn);
	}
	else
	{
		GameMode->NotifyEnemyKilled(OwnerPawn);
	}

	// TODO: 사망 애니메이션/래그돌 연동, 시체 정리(Destroy 지연)
}
