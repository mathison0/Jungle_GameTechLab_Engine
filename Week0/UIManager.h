#pragma once
#include "URenderer.h"
#include "Button.h"
#include "GameContext.h"

class UIManager : public IGameStateListener
{
private:
	Button* startButton;
	Button* restartButton;

	public:
	UIManager();
	~UIManager();
	void Update(float mouseX, float mouseY, bool isMousePressed);
	void Render(URenderer& renderer);

	void OnGameStateChanged(EGameState gameState) override;
};

