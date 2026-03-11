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


	timeLap = new TimeLap({ 0.0f, 0.9f, 0.f }, "TestSprite", 15, 8, 300, 160);

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

void UIManager::Update(float mouseX, float mouseY, float deltaTime, bool isMousePressed)
{
	startButton->Update(mouseX, mouseY, isMousePressed);
	restartButton->Update(mouseX, mouseY, isMousePressed);

	if (isRunning)
	{
		timeLap->Update(deltaTime);
	}
}

void UIManager::Render(URenderer& renderer)
{
	startButton->Render(renderer);
	restartButton->Render(renderer);
	timeLap->Render(renderer);
}

void UIManager::Reset()
{
	startButton->SetActive(false);
	restartButton->SetActive(false);
	isRunning = true;
	timeLap->Clear();

}

void UIManager::OnGameStateChanged(EGameState newState)
{
	switch (newState)
	{
	case EGameState::Title:
		startButton->SetActive(true);
		restartButton->SetActive(false);
		timeLap->Clear();
		break;

	case EGameState::Running:
		Reset();
		break;

	case EGameState::Ending:

		startButton->SetActive(false);
		restartButton->SetActive(false);
		break;

	case EGameState::Clear:
		startButton->SetActive(false);
		restartButton->SetActive(true);
		break;
	}
}
