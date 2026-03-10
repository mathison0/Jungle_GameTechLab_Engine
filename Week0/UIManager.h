#pragma once
#include "URenderer.h"
#include "Button.h"

class UIManager
{
private:
	Button* startButton;
	Button* restartButton;

	public:
	UIManager();
	~UIManager();
	void Update(float mouseX, float mouseY, bool isMousePressed);
	void Render(URenderer& renderer);
};

