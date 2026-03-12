#pragma once
#include "URenderer.h"
#include "Button.h"
#include "GameContext.h"
#include "Sprite.h"
#include "TimeLap.h"

class UIManager : public IGameStateListener
{
private:
	Button* startButton;
	Button* restartButton;
	Button* quitButton;

	Button* titleImage;
	Button* teamImage;

	TimeLap* timeLap;

	bool isRunning = false;

	public:
	UIManager();
	~UIManager();
	void Update(float mouseX, float mouseY, float deltaTime, bool isMousePressed);
	void Render(URenderer& renderer);
	void Reset();

	void OnGameStateChanged(EGameState gameState) override;
};

