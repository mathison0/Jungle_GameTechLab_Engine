#include "Game/Musou/Combat/BattleComponent.h"

#include "Game/Musou/GameMode/MusouGameMode.h"
#include "GameFramework/Pawn/Pawn.h"
#include "GameFramework/World.h"
#include "Core/Logging/Log.h"

void UBattleComponent::BeginPlay()
{
	UActorComponent::BeginPlay();

	Health = MaxHealth;
	bDead = false;
}

float UBattleComponent::ApplyDamage(float Damage, AActor* DamageInstigator)
{
	if (bDead || Damage <= 0.0f)
	{
		return 0.0f;
	}

	const float Applied = (Damage > Health) ? Health : Damage;
	Health -= Applied;

	// TODO: 피격 리액션 (React Large 애님), 무적 시간, 넉백

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
	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	AMusouGameMode* GameMode = Cast<AMusouGameMode>(World->GetGameMode());
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
