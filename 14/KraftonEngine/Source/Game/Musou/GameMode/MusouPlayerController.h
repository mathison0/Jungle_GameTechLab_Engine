#pragma once

#include "GameFramework/GameMode/PlayerController.h"

#include "Source/Game/Musou/GameMode/MusouPlayerController.generated.h"

// ============================================================
// AMusouPlayerController — 무쌍 게임 플레이어 컨트롤러
//
// 입력 바인딩 자체는 Pawn(SetupInputComponent)이 담당하는 구조라
// 현재는 베이스 동작(Possess/카메라) 위임. 추후 게임 레벨 입력
// (일시정지, UI 토글, 스킬 쿨다운 게이트 등)을 여기에 둔다.
// ============================================================
UCLASS()
class AMusouPlayerController : public APlayerController
{
public:
	GENERATED_BODY()
	AMusouPlayerController() = default;
	~AMusouPlayerController() override = default;

	void BeginPlay() override;
};
