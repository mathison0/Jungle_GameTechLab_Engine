#pragma once

#include "Component/ActorComponent.h"

#include "Source/Game/Musou/Combat/BattleComponent.generated.h"

class AActor;

// ============================================================
// UBattleComponent — 체력/데미지 처리 (초안)
//
// 플레이어/적 공용. 사망 시 GameMode에 통지한다:
//   - Possessed Pawn(플레이어) 사망 → NotifyPlayerDeath
//   - 그 외(적) 사망              → NotifyEnemyKilled (킬/콤보 누적)
//
// TODO(확장): 무적 시간, 피격 리액션(React Large 애님 연동),
//             넉백, 데미지 타입/배율, 사망 애니메이션 연동
// ============================================================
UCLASS()
class UBattleComponent : public UActorComponent
{
public:
	GENERATED_BODY()
	UBattleComponent() = default;
	~UBattleComponent() override = default;

	void BeginPlay() override;

	// 데미지 적용. 실제 적용된 데미지를 반환 (사망 상태면 0).
	virtual float ApplyDamage(float Damage, AActor* DamageInstigator);
	void Heal(float Amount);
	void Kill(AActor* DamageInstigator = nullptr);

	bool IsDead() const { return bDead; }
	float GetHealth() const { return Health; }
	float GetMaxHealth() const { return MaxHealth; }
	float GetHealthRatio() const { return MaxHealth > 0.0f ? Health / MaxHealth : 0.0f; }
	float GetAttackPower() const { return AttackPower; }

	UPROPERTY(Edit, Save, Category="Battle", DisplayName="Max Health")
	float MaxHealth = 100.0f;

	UPROPERTY(Edit, Save, Category="Battle", DisplayName="Attack Power")
	float AttackPower = 10.0f;

protected:
	// 사망 1회 진입점 — GameMode 통지 후 서브클래스/외부 확장.
	virtual void OnDeath(AActor* DamageInstigator);

	float Health = 0.0f;
	bool bDead = false;
};
