#include "UIManager.h"


UIManager::UIManager()
{
	startButton = new Button({ 0.0f, -0.1f, 0.0f }, { 0.3f, 0.1f}, "StartButton", []() {
		// Start 버튼 클릭 시 실행할 코드
		// 예: 게임 시작, 메뉴 전환 등
		});

	restartButton = new Button({ 0.0f, -0.3f, 0.0f }, { 0.3f, 0.1f }, "RestartButton", []() {
		// Restart 버튼 클릭 시 실행할 코드
		// 예: 게임 재시작, 점수 초기화 등
		});

}

UIManager::~UIManager()
{
	if (startButton)
	{
		delete startButton;
		startButton = nullptr;
	}
	if (restartButton)
	{
		delete restartButton;
		restartButton = nullptr;
	}
}

void UIManager::Update(float mouseX, float mouseY, bool isMousePressed)
{
	startButton->Update(mouseX, mouseY, isMousePressed);
	restartButton->Update(mouseX, mouseY, isMousePressed);
}

void UIManager::Render(URenderer& renderer)
{
	startButton->Render(renderer);
	restartButton->Render(renderer);
}
