#include "Game/Musou/Camera/AnimNotify_CameraShake.h"

#include "Component/Primitive/SkeletalMeshComponent.h"
#include "GameFramework/Pawn/Pawn.h"
#include "GameFramework/World.h"
#include "GameFramework/GameMode/PlayerController.h"
#include "GameFramework/Camera/PlayerCameraManager.h"
#include "GameFramework/Camera/WaveOscillatorCameraShake.h"
#include "GameFramework/Camera/CameraTypes.h"
#include "Game/Musou/MusouGameSettings.h"

void UAnimNotify_CameraShake::Notify(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Anim)
{
	(void)Anim;

	// 전역 설정 — 카메라 셰이크 off 면 흔들림 발동 안 함.
	if (!FMusouGameSettings::Get().IsCameraShakeEnabled())
	{
		return;
	}

	if (!MeshComp)
	{
		return;
	}

	AActor* OwnerActor = MeshComp->GetOwner();
	if (!OwnerActor)
	{
		return;
	}

	// 군체/적 애님에 같은 시퀀스가 물려 화면이 난사되는 것을 막는 가드.
	if (bOnlyIfPlayer)
	{
		APawn* Pawn = Cast<APawn>(OwnerActor);
		if (!Pawn || !Pawn->IsPossessed())
		{
			return;
		}
	}

	UWorld* World = OwnerActor->GetWorld();
	APlayerController* PC = World ? World->GetFirstPlayerController() : nullptr;
	APlayerCameraManager* CamMgr = PC ? PC->GetPlayerCameraManager() : nullptr;
	if (!CamMgr)
	{
		// Editor 프리뷰 / 카메라 매니저 미사용 월드 — 조용히 무시
		return;
	}

	const ECameraShakePlaySpace PlaySpace =
		bWorldSpace ? ECameraShakePlaySpace::World : ECameraShakePlaySpace::CameraLocal;

	UCameraShakeBase* Instance = CamMgr->StartCameraShake<UWaveOscillatorCameraShake>(Scale, PlaySpace);

	// 진폭/지속/블렌드는 노티파이에서 저작 — 인스턴스 필드는 매 프레임 라이브로
	// 읽히므로 StartShake 직후 덮어써도 첫 업데이트부터 반영된다. 빈도는 셰이크 기본값 유지.
	if (UWaveOscillatorCameraShake* Wave = Cast<UWaveOscillatorCameraShake>(Instance))
	{
		Wave->Duration     = Duration;
		Wave->BlendInTime  = BlendInTime;
		Wave->BlendOutTime = BlendOutTime;
		Wave->LocAmplitude = LocAmplitude;
		Wave->RotAmplitude = RotAmplitude;
		Wave->FOVAmplitude = FOVAmplitude;
	}
}
