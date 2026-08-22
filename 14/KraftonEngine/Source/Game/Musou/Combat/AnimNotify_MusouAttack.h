#pragma once

#include "Animation/Notify/AnimNotify.h"
#include "Core/Types/CoreTypes.h"
#include "Math/Vector.h"

#include "Source/Game/Musou/Combat/AnimNotify_MusouAttack.generated.h"

class USkeletalMeshComponent;
class UAnimSequenceBase;
struct FMusouAttackEvent;
class UWorld;

// ============================================================
// UAnimNotify_MusouAttack — 공격 발동 notify
//
// 몽타주의 스윙 임팩트 프레임에 배치하고 AttackId만 지정하면,
// 발동 시점의 공격자/기하를 캡처해 AMusouGameMode::BroadcastAttack으로
// FMusouAttackEvent를 발행한다. 판정/적용은 수신자(군체 Manager /
// 보스 BattleComponent) 책임 — 이 notify는 "누가 어떤 공격을 했다"만 알린다.
//
// AttackId는 AttackTypes.h의 FindMusouAttackSpec 테이블 키 ("attack1" 등).
// ============================================================
UCLASS()
class UAnimNotify_MusouAttack : public UAnimNotify
{
public:
	GENERATED_BODY()
	UAnimNotify_MusouAttack() = default;
	~UAnimNotify_MusouAttack() override = default;

	UPROPERTY(Edit, Save, Category="MusouAttack", DisplayName="Attack Id")
	FString AttackId = "attack1";

	// ── 디버그 — 발동 시점의 판정 범위(원/콘)를 라인으로 표시 ──
	UPROPERTY(Edit, Save, Category="MusouAttack|Debug", DisplayName="Draw Debug")
	bool bDrawDebug = true;

	UPROPERTY(Edit, Save, Category="MusouAttack|Debug", DisplayName="Debug Duration", Min=0.0f, Max=5.0f, Speed=0.05f)
	float DebugDrawDuration = 0.5f;

	UPROPERTY(Edit, Save, Category="MusouAttack|Debug", DisplayName="Debug Color", Type=Color4)
	FVector4 DebugColor = FVector4(1.0f, 0.85f, 0.0f, 1.0f);

	void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Anim) override;

private:
	void DrawAttackVolume(UWorld* World, const FMusouAttackEvent& Event) const;
};
