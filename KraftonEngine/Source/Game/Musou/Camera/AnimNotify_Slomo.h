#pragma once

#include "Animation/Notify/AnimNotify.h"

#include "Source/Game/Musou/Camera/AnimNotify_Slomo.generated.h"

class USkeletalMeshComponent;
class UAnimSequenceBase;

// ============================================================
// UAnimNotify_Slomo — 슬로모(전역 시간 감속) 발동 notify (one-shot)
//
// 몽타주의 결정타/피니셔 임팩트 프레임에 배치하면, 발동 시점에 재생 폰의
// UActionComponent::Slomo 를 호출해 전역 TimeDilation 을 잠깐 낮춘다.
// (무쌍기 발동에서 쓰는 Slomo 인프라와 동일 — 카메라/타이머는 raw delta 라
//  슬로모 영향을 받지 않아 셰이크·블렌드는 정상 속도로 돈다.)
//
// Duration 은 실시간(raw) 초 — 감속 중에도 일정하게 카운트다운한다.
// TimeDilation 은 목표 배율(0~1, 0.3=30% 속도). bOnlyIfPlayer=true 면 빙의
// 플레이어가 재생할 때만 발동 — 적/군체 애님이 전역 시간을 늦추는 것을 막는다.
// ============================================================
UCLASS()
class UAnimNotify_Slomo : public UAnimNotify
{
public:
	GENERATED_BODY()
	UAnimNotify_Slomo() = default;
	~UAnimNotify_Slomo() override = default;

	// 지속 시간(실시간 초).
	UPROPERTY(Edit, Save, Category = "Slomo", DisplayName = "Duration", Min = 0.02f, Max = 3.0f, Speed = 0.01f)
	float Duration = 0.25f;

	// 목표 시간 배율 (0~1, 낮을수록 느림).
	UPROPERTY(Edit, Save, Category = "Slomo", DisplayName = "Time Dilation", Min = 0.0f, Max = 1.0f, Speed = 0.01f)
	float TimeDilation = 0.35f;

	// true 면 "맞았을 때만" 발동 — GameMode 큐에 예약해 두고, 이 공격자가 HitWindow
	// 내에 실제 히트를 내면 그 순간 슬로모. 빗나가면 폐기. false 면 즉시 발동(기존).
	UPROPERTY(Edit, Save, Category = "Slomo", DisplayName = "Only On Hit")
	bool bOnlyOnHit = false;

	// Only On Hit 일 때 히트를 기다리는 시간(실시간 초). 공격 임팩트 notify 와의
	// 프레임 간격을 덮을 만큼만. 이 시간 내 히트가 없으면 발동하지 않는다.
	UPROPERTY(Edit, Save, Category = "Slomo", DisplayName = "Hit Window", Min = 0.02f, Max = 0.5f, Speed = 0.01f)
	float HitWindow = 0.2f;

	// 빙의 플레이어가 재생할 때만 발동 (적/군체 애님 전역 감속 방지).
	UPROPERTY(Edit, Save, Category = "Slomo", DisplayName = "Only If Player")
	bool bOnlyIfPlayer = true;

	void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Anim) override;
};
