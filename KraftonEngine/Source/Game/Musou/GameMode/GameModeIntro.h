#pragma once

#include "GameFramework/GameMode/GameModeBase.h"

#include "Source/Game/Musou/GameMode/GameModeIntro.generated.h"

class UUserWidget;

UCLASS()
class AGameModeIntro : public AGameModeBase
{
public:
	GENERATED_BODY()
	AGameModeIntro();
	~AGameModeIntro() override = default;

	void StartMatch() override;
	void EndPlay() override;

private:
	UUserWidget* IntroWidget = nullptr;
};
