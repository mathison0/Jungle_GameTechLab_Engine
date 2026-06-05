#pragma once

#include "GameFramework/GameMode/GameModeBase.h"

#include "Source/Game/Musou/GameMode/MusouGameMode.generated.h"

class AMusouGameState;
class APawn;

// ============================================================
// AMusouGameMode — 무쌍 게임 룰의 주체
//
// 생성자에서 MusouGameState/MusouPlayerController를 지정한다.
// 적 처치/플레이어 사망 등 게임 룰 이벤트의 진입점.
//
// 활성화: Settings/ProjectSettings.ini → Game.GameModeClassName = "AMusouGameMode"
//         (또는 씬별 WorldSettings.GameMode)
// ============================================================
UCLASS()
class AMusouGameMode : public AGameModeBase
{
public:
	GENERATED_BODY()
	AMusouGameMode();
	~AMusouGameMode() override = default;

	// --- Match flow ---
	void StartMatch() override;
	void EndMatch() override;

	// --- 게임 룰 이벤트 진입점 ---
	// 적 처치 시 호출 (적 사망 처리 코드에서). GameState에 킬/콤보 누적.
	virtual void NotifyEnemyKilled(APawn* Killed);

	// 플레이어 사망 시 호출 — 매치 종료.
	virtual void NotifyPlayerDeath(APawn* Player);

	// --- Accessors ---
	AMusouGameState* GetMusouGameState() const;
};
