#pragma once

#include "Component/ActorComponent.h"
#include "Core/Delegate.h"

#include "Source/Game/Musou/Combat/BattleComponent.generated.h"

class AActor;
struct FMusouAttackEvent;

// ============================================================
// UBattleComponent — 체력/데미지 처리 (액터 단위: 플레이어/보스)
//
// 잡졸 군체는 액터가 아니라 군체 Manager가 SoA로 직접 관리하므로
// 이 컴포넌트를 쓰지 않는다. 액터 기반 유닛(플레이어/보스)만 부착.
//
// 피격 경로 (단일 경로):
//   AMusouGameMode::OnAttackPerformed 구독 → HandleAttackEvent에서
//   자기 위치로 FMusouAttackEvent::IsInVolume 셀프 판정 → ApplyDamage.
//   진영(bIsPlayerTeam)이 공격자와 같으면 무시.
//
// 사망 시 GameMode에 통지:
//   - Possessed Pawn(플레이어) 사망 → NotifyPlayerDeath
//   - 그 외(보스 등)              → NotifyEnemyKilled
//
// TODO(확장): 무적 시간, 피격 리액션(React Large 애님 연동), 사망 애니메이션
// ============================================================
UCLASS()
class UBattleComponent : public UActorComponent
{
public:
	GENERATED_BODY()
	UBattleComponent() = default;
	~UBattleComponent() override = default;

	void BeginPlay() override;
	void EndPlay() override;

	// 데미지 적용. 실제 적용된 데미지를 반환 (사망 상태면 0).
	virtual float ApplyDamage(float Damage, AActor* DamageInstigator);
	void Heal(float Amount);
	void Kill(AActor* DamageInstigator = nullptr);

	// 무적 — 무쌍기 등 연출 구간 동안 피해 무시 (ApplyDamage 가 0 반환).
	void SetInvincible(bool bEnable) { bInvincible = bEnable; }
	bool IsInvincible() const { return bInvincible; }

	bool IsDead() const { return bDead; }
	float GetHealth() const { return Health; }
	float GetMaxHealth() const { return MaxHealth; }
	float GetHealthRatio() const { return MaxHealth > 0.0f ? Health / MaxHealth : 0.0f; }
	float GetAttackPower() const { return AttackPower; }

	UPROPERTY(Edit, Save, Category="Battle", DisplayName="Max Health")
	float MaxHealth = 100.0f;

	UPROPERTY(Edit, Save, Category="Battle", DisplayName="Attack Power")
	float AttackPower = 10.0f;

	// 진영 — 플레이어 캐릭터 true / 보스·적 false. 같은 진영 공격은 무시.
	UPROPERTY(Edit, Save, Category="Battle", DisplayName="Is Player Team")
	bool bIsPlayerTeam = false;

	// 피격 시 넉백 수용 여부 — 보스는 끄면 슈퍼아머.
	UPROPERTY(Edit, Save, Category="Battle", DisplayName="Accept Knockback")
	bool bAcceptKnockback = true;

protected:
	// OnAttackPerformed 수신 — 셀프 판정 후 데미지/넉백 적용 + 히트 회신.
	virtual void HandleAttackEvent(const FMusouAttackEvent& Event);

	// 사망 1회 진입점 — GameMode 통지 후 서브클래스/외부 확장.
	virtual void OnDeath(AActor* DamageInstigator);

	float Health = 0.0f;
	bool bDead = false;
	bool bInvincible = false;   // 무쌍기 등 일시 무적 (직렬화 제외 — 런타임 전용)

	FDelegateHandle AttackListenerHandle;
};
