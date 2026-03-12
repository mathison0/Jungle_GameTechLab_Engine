#include "UIManager.h"


UIManager::UIManager()
{
	startButton = new Button({ 0.0f, -0.4f, 0.0f }, { 0.4f, 0.2f }, "StartButton", []() {
		GameContext::GetiNSTANCE().SetState(EGameState::Running);
		});

	restartButton = new Button({ 0.0f, -0.4f, 0.0f }, { 0.4f, 0.2f }, "RestartButton", []() {
		GameContext::GetiNSTANCE().SetState(EGameState::Running);
		});

	quitButton = new Button({ 0.0f, -0.6f, 0.0f }, { 0.4f, 0.2f }, "QuitButton", []() {
		std::exit(0);
		});


	restartButton->SetActive(false);

	titleImage = new Button({ 0.f, 0.2f, 0.0f }, { 1.5f, 0.8f }, "Title", []() {});
	teamImage = new Button({ 0.7f, -0.9f, 0.0f }, { 0.5f, 0.5f }, "Team", []() {});


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
	quitButton->Update(mouseX, mouseY, isMousePressed);

	if (isRunning)
	{
		timeLap->Update(deltaTime);
	}
}

void UIManager::Render(URenderer& renderer)
{
	startButton->Render(renderer);
	restartButton->Render(renderer);
	quitButton->Render(renderer);
	timeLap->Render(renderer);
	titleImage->Render(renderer);
	teamImage->Render(renderer);
}

void UIManager::Reset()
{
	startButton->SetActive(false);
	restartButton->SetActive(false);
	quitButton->SetActive(false);
	titleImage->SetActive(false);
	teamImage->SetActive(false);
	isRunning = true;
	timeLap->Clear();
	timeLap->SetActive(true);

}

void UIManager::OnGameStateChanged(EGameState newState)
{
	switch (newState)
	{
	case EGameState::Title:
		startButton->SetActive(true);
		restartButton->SetActive(false);
		timeLap->Clear();
		timeLap->SetActive(false);
		break;

	case EGameState::Running:
		Reset();
		break;

	case EGameState::Ending:

		startButton->SetActive(false);
		restartButton->SetActive(false);
		isRunning = false;
		break;

	case EGameState::Clear:
		startButton->SetActive(false);
		restartButton->SetActive(true);
		quitButton->SetActive(true);
		break;
	}
}
