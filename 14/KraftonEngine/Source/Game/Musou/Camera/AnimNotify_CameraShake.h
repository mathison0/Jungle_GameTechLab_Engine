#pragma once

#include "Animation/Notify/AnimNotify.h"
#include "Math/Vector.h"
#include "Math/Rotator.h"

#include "Source/Game/Musou/Camera/AnimNotify_CameraShake.generated.h"

class USkeletalMeshComponent;
class UAnimSequenceBase;

// ============================================================
// UAnimNotify_CameraShake — 카메라 흔들림 발동 notify (one-shot)
//
// 몽타주의 임팩트 프레임(착지/강타/피니셔 히트)에 배치하면, 발동 시점에
// 로컬 플레이어 카메라 매니저로 UWaveOscillatorCameraShake 를 1회 시작한다.
// AnimNotifyState_CameraShot(샷 전환)과 독립적 — shake 는 ModifierList 로
// 누적되므로 시네마틱 샷 중에도 동시에 적용된다.
//
// 파라미터는 WaveOscillator 인스턴스에 그대로 주입 — 빈도(frequency)는 셰이크
// 기본값을 쓰고, 진폭/지속/블렌드/스케일만 노티파이에서 저작한다.
//
// bOnlyIfPlayer=true 면 빙의 중인 플레이어 폰이 재생하는 경우에만 발동 —
// 군체/적 애님에 같은 시퀀스가 물려 화면이 난사되는 것을 막는다. 보스 강타
// 처럼 적 애님에서도 흔들고 싶으면 끄면 된다.
// ============================================================
UCLASS()
class UAnimNotify_CameraShake : public UAnimNotify
{
public:
	GENERATED_BODY()
	UAnimNotify_CameraShake() = default;
	~UAnimNotify_CameraShake() override = default;

	// 전체 강도 배수 (1=프리셋 그대로).
	UPROPERTY(Edit, Save, Category = "CameraShake", DisplayName = "Scale", Min = 0.0f, Max = 5.0f, Speed = 0.05f)
	float Scale = 1.0f;

	// 지속 시간(초). 양 끝에서 BlendIn/Out 으로 ramp.
	UPROPERTY(Edit, Save, Category = "CameraShake", DisplayName = "Duration", Min = 0.05f, Max = 3.0f, Speed = 0.01f)
	float Duration = 0.3f;

	UPROPERTY(Edit, Save, Category = "CameraShake", DisplayName = "Blend In", Min = 0.0f, Max = 1.0f, Speed = 0.01f)
	float BlendInTime = 0.03f;

	UPROPERTY(Edit, Save, Category = "CameraShake", DisplayName = "Blend Out", Min = 0.0f, Max = 1.0f, Speed = 0.01f)
	float BlendOutTime = 0.12f;

	// 위치 진폭 (월드 단위). 채널별 흔들림 크기.
	UPROPERTY(Edit, Save, Category = "CameraShake", DisplayName = "Loc Amplitude", Type = Vec3, Speed = 0.05f)
	FVector LocAmplitude = FVector(2.5f, 2.5f, 1.5f);

	// 회전 진폭 (도, Pitch/Yaw/Roll).
	UPROPERTY(Edit, Save, Category = "CameraShake", DisplayName = "Rot Amplitude (deg)", Type = Rotator, Speed = 0.05f)
	FRotator RotAmplitude = FRotator(0.0f, 0.0f, 0.0f);

	// FOV 진폭 (라디안, 0=끔).
	UPROPERTY(Edit, Save, Category = "CameraShake", DisplayName = "FOV Amplitude", Min = 0.0f, Max = 0.3f, Speed = 0.005f)
	float FOVAmplitude = 0.0f;

	// 월드 좌표 흔들림 (false=카메라 로컬).
	UPROPERTY(Edit, Save, Category = "CameraShake", DisplayName = "World Space")
	bool bWorldSpace = false;

	// 빙의 플레이어가 재생할 때만 발동 (군체/적 애님 난사 방지).
	UPROPERTY(Edit, Save, Category = "CameraShake", DisplayName = "Only If Player")
	bool bOnlyIfPlayer = true;

	void Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Anim) override;
};
