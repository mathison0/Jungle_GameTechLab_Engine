#pragma once

#include "Animation/Notify/AnimNotifyState.h"
#include "Math/Vector.h"
#include "Math/Rotator.h"

#include "Source/Game/Musou/Camera/AnimNotifyState_CameraShot.generated.h"

class USkeletalMeshComponent;
class UAnimSequenceBase;
class AMusouCharacter;

// ============================================================
// UAnimNotifyState_CameraShot — 몽타주 카메라 연출 샷
//
// 몽타주의 연출 구간에 배치하면, 구간 동안 메인(SpringArm) 카메라 대신
// 캐릭터에 붙은 연출 카메라로 블렌드해 들어갔다가 구간 끝에 복귀한다.
// 실제 카메라 관리(상시 2대 핑퐁/블렌드/시선 유지)는 AMusouCharacter 가
// 담당 — 이 notify 는 샷 파라미터를 전달하는 트리거.
//
// 저작 경로 2가지:
//   1) attack_data.lua steps.<id>.camera 배열 → 런타임 주입 (핫리로드 튜닝)
//   2) 에디터에서 몽타주에 직접 배치 (저작본이 있으면 lua 주입은 스킵)
//
// Offset 은 캡슐 로컬 (X=전방, Y=우측, Z=상단, m). bLookAt 이면 매 프레임
// 캐릭터를 바라보도록 회전이 갱신되어 Rotation 은 무시된다.
// ============================================================
UCLASS()
class UAnimNotifyState_CameraShot : public UAnimNotifyState
{
public:
	GENERATED_BODY()
	UAnimNotifyState_CameraShot() = default;
	~UAnimNotifyState_CameraShot() override = default;

	UPROPERTY(Edit, Save, Category = "CameraShot", DisplayName = "Blend In", Min = 0.0f, Max = 2.0f, Speed = 0.01f)
	float BlendIn = 0.15f;

	UPROPERTY(Edit, Save, Category = "CameraShot", DisplayName = "Blend Out", Min = 0.0f, Max = 2.0f, Speed = 0.01f)
	float BlendOut = 0.25f;

	UPROPERTY(Edit, Save, Category = "CameraShot", DisplayName = "Offset (Capsule Local)", Type = Vec3, Speed = 0.05f)
	FVector Offset = FVector(2.5f, 2.0f, 1.0f);

	// bLookAt = false 일 때만 사용 — 캡슐 로컬 회전.
	UPROPERTY(Edit, Save, Category = "CameraShot", DisplayName = "Rotation (Local)", Type = Rotator, Speed = 0.5f)
	FRotator Rotation = FRotator::ZeroRotator;

	// 0 = 메인 카메라 FOV 유지.
	UPROPERTY(Edit, Save, Category = "CameraShot", DisplayName = "FOV (deg, 0=keep)", Min = 0.0f, Max = 3.14f, Speed = 0.01f, DisplayUnit = "Degrees", StorageUnit = "Radians")
	float FOV = 0.0f;

	UPROPERTY(Edit, Save, Category = "CameraShot", DisplayName = "Look At Character")
	bool bLookAt = true;

	// 시선 목표 높이 — 액터 위치(캡슐 중심) 기준 +Z (m).
	UPROPERTY(Edit, Save, Category = "CameraShot", DisplayName = "Look At Height", Min = -2.0f, Max = 3.0f, Speed = 0.05f)
	float LookAtHeight = 0.5f;

	// >0 = 캐릭터 대신 전방 이 거리(m) 지점을 바라봄 — 검기 사선/전방 진행을 화면에 담는다.
	UPROPERTY(Edit, Save, Category = "CameraShot", DisplayName = "Look Ahead (m)", Min = 0.0f, Max = 20.0f, Speed = 0.1f)
	float LookAhead = 0.0f;

	// true = 캐릭터를 따라다니는 샷, false = 샷 시작 위치에 월드 고정 (캐릭터가 프레임 안에서 움직임).
	UPROPERTY(Edit, Save, Category = "CameraShot", DisplayName = "Follow Character")
	bool bFollow = true;

	// 레터박스 두께 비율 (0 = 없음). 화면 상하 시네마틱 바.
	UPROPERTY(Edit, Save, Category = "CameraShot", DisplayName = "Letterbox", Min = 0.0f, Max = 0.5f, Speed = 0.01f)
	float Letterbox = 0.0f;

	// true(기본) = offset 을 카메라 뷰(ControlRotation) 기준으로 배치 → 캐릭터가 옆을 봐도
	//  화면 기준 일관 + 블렌드 튐 감소. false = 캐릭터 facing 기준 (도약/돌진 추적샷).
	UPROPERTY(Edit, Save, Category = "CameraShot", DisplayName = "Camera Relative")
	bool bCameraRelative = true;

	void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Anim, float TotalDuration) override;
	void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Anim) override;

private:
	static AMusouCharacter* ResolveCharacter(USkeletalMeshComponent* MeshComp);
};
