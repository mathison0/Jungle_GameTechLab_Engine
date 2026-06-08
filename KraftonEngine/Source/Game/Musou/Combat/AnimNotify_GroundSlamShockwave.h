#pragma once

#include "Animation/Notify/AnimNotify.h"
#include "Core/Types/CoreTypes.h"

#include "Source/Game/Musou/Combat/AnimNotify_GroundSlamShockwave.generated.h"

class USkeletalMeshComponent;
class UAnimSequenceBase;

// ============================================================
// UAnimNotify_GroundSlamShockwave — 궁극기 지면 강타 충격파 발동 notify (one-shot)
//
// 몽타주의 "바닥을 후려치는" 프레임에 배치(또는 lua shockwave.trigger_frac 으로 주입).
// 발동 시 플레이어 캐릭터에서 전방 진행 충격파를 시작 — 시작점에서 전방으로 Distance 만큼
// Duration 동안 Pulses 개의 데미지 판정 + 검기(placeholder)를 순차 발사한다.
// 데미지 판정 기하는 AttackId 의 spec 을 펄스마다 origin 만 전진시켜 사용.
// ============================================================
UCLASS()
class UAnimNotify_GroundSlamShockwave : public UAnimNotify
{
public:
	GENERATED_BODY()
	UAnimNotify_GroundSlamShockwave() = default;
	~UAnimNotify_GroundSlamShockwave() override = default;

	UPROPERTY(Edit, Save, Category = "GroundSlam", DisplayName = "Distance (m)", Min = 1.0f, Max = 40.0f, Speed = 0.5f)
	float Distance = 12.0f;

	UPROPERTY(Edit, Save, Category = "GroundSlam", DisplayName = "Duration (s)", Min = 0.05f, Max = 3.0f, Speed = 0.01f)
	float Duration = 0.7f;

	UPROPERTY(Edit, Save, Category = "GroundSlam", DisplayName = "Pulses", Min = 1, Max = 40)
	int32 Pulses = 8;

	UPROPERTY(Edit, Save, Category = "GroundSlam", DisplayName = "Attack Id")
	FString AttackId = "musou_slam";

	UPROPERTY(Edit, Save, Category = "GroundSlam", DisplayName = "Slash Speed", Min = 0.0f, Max = 30.0f, Speed = 0.1f)
	float SlashSpeed = 9.0f;

	UPROPERTY(Edit, Save, Category = "GroundSlam", DisplayName = "Slash Life (s)", Min = 0.05f, Max = 2.0f, Speed = 0.01f)
	float SlashLife = 0.45f;

	void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Anim) override;
};
