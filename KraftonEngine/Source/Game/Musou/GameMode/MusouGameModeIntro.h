#pragma once

#include "GameFramework/GameMode/GameModeBase.h"

#include "Source/Game/Musou/GameMode/MusouGameModeIntro.generated.h"

class UUserWidget;

UCLASS()
class AMusouGameModeIntro : public AGameModeBase
{
public:
	GENERATED_BODY()
	AMusouGameModeIntro();
	~AMusouGameModeIntro() override = default;

	void StartMatch() override;
	void EndPlay() override;

private:
	UUserWidget* IntroWidget = nullptr;
};
