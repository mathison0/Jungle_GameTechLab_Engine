#include "Game/Musou/Camera/AnimNotifyState_CameraShot.h"

#include "Game/Musou/Character/MusouCharacter.h"
#include "Game/Musou/Combat/AttackDataRegistry.h"
#include "Component/Primitive/SkeletalMeshComponent.h"
#include "GameFramework/AActor.h"

AMusouCharacter* UAnimNotifyState_CameraShot::ResolveCharacter(USkeletalMeshComponent* MeshComp)
{
	AActor* OwnerActor = MeshComp ? MeshComp->GetOwner() : nullptr;
	return Cast<AMusouCharacter>(OwnerActor);
}

void UAnimNotifyState_CameraShot::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Anim, float TotalDuration)
{
	(void)Anim; (void)TotalDuration;

	AMusouCharacter* Character = ResolveCharacter(MeshComp);
	if (!Character)
	{
		return;
	}

	FMusouCameraShot Shot;
	Shot.BlendIn      = BlendIn;
	Shot.BlendOut     = BlendOut;
	Shot.Offset       = Offset;
	Shot.Rotation     = Rotation;
	Shot.FOVRad       = FOV;
	Shot.bLookAt         = bLookAt;
	Shot.LookAtHeight    = LookAtHeight;
	Shot.bFollow         = bFollow;
	Shot.Letterbox       = Letterbox;
	Shot.bCameraRelative = bCameraRelative;

	// Token = this — 같은 몽타주에 샷이 여러 개일 때 뒷 샷이 인수하면
	// 앞 샷의 NotifyEnd 가 복귀 블렌드를 걸지 않도록 식별한다.
	Character->StartCameraShot(Shot, this);
}

void UAnimNotifyState_CameraShot::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Anim)
{
	(void)Anim;

	// 몽타주 캔슬/전환 시에도 호출되므로 샷이 새지 않는다 (ComboWindow 와 동일 보장).
	if (AMusouCharacter* Character = ResolveCharacter(MeshComp))
	{
		Character->EndCameraShot(this);
	}
}
