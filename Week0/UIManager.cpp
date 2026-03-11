#include "UIManager.h"


UIManager::UIManager()
{
	startButton = new Button({ 0.0f, -0.1f, 0.0f }, { 0.3f, 0.1f }, "StartButton", []() {
		GameContext::GetiNSTANCE().SetState(EGameState::Running);
		});

	restartButton = new Button({ 0.0f, -0.3f, 0.0f }, { 0.3f, 0.1f }, "RestartButton", []() {
		GameContext::GetiNSTANCE().SetState(EGameState::Running);
		});

	restartButton->SetActive(false);


	testSprite = new Sprite({ 0.f,0.f,0.f }, { 0.1f,0.1f,0.f }, "TestSprite", 15, 8, 300.0f, 160.0f);

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
	testSprite->Render(renderer);
}

void UIManager::OnGameStateChanged(EGameState newState)
{
	switch (newState)
	{
	case EGameState::Title:
		startButton->SetActive(true);
		restartButton->SetActive(false);
		break;

	case EGameState::Running:
		startButton->SetActive(false);
		restartButton->SetActive(false);
		break;

	case EGameState::Ending:
		break;

	case EGameState::Clear:
		startButton->SetActive(false);
		restartButton->SetActive(true);
		break;
	}
}
