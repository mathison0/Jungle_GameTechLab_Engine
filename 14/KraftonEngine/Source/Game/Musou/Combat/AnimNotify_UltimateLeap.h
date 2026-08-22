#pragma once

#include "Animation/Notify/AnimNotify.h"
#include "Core/Types/CoreTypes.h"

#include "Source/Game/Musou/Combat/AnimNotify_UltimateLeap.generated.h"

class USkeletalMeshComponent;
class UAnimSequenceBase;

// ============================================================
// UAnimNotify_UltimateLeap — 궁극기 백플립 도약 notify (one-shot)
//
// 제자리 백플립 몽타주의 도약 프레임에 배치(또는 lua leap.trigger_frac 으로 주입).
// 발동 시 플레이어를 후방+상방으로 임펄스 — 백플립이 실제로 뒤로 빠지게 한다.
// BackSpeed/UpSpeed 는 m/s (BackSpeed = 캐릭터 전방의 반대 방향 수평 속도).
// ============================================================
UCLASS()
class UAnimNotify_UltimateLeap : public UAnimNotify
{
public:
	GENERATED_BODY()
	UAnimNotify_UltimateLeap() = default;
	~UAnimNotify_UltimateLeap() override = default;

	UPROPERTY(Edit, Save, Category = "UltimateLeap", DisplayName = "Back Speed (m/s)", Min = 0.0f, Max = 30.0f, Speed = 0.1f)
	float BackSpeed = 6.0f;

	UPROPERTY(Edit, Save, Category = "UltimateLeap", DisplayName = "Up Speed (m/s)", Min = 0.0f, Max = 20.0f, Speed = 0.1f)
	float UpSpeed = 4.0f;

	UPROPERTY(Edit, Save, Category = "UltimateLeap", DisplayName = "Gravity Scale", Min = 0.0f, Max = 1.0f, Speed = 0.01f)
	float GravityScale = 0.3f;

	void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Anim) override;
};
