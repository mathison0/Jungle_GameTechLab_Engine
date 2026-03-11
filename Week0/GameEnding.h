#pragma once
#include "GameContext.h"
#include "UBall.h"

class GameEnding
{
public:
	GameEnding(float triggerHeight = 100.f, float clearHeight = 100.8f, float slowSpeed = 0.1f);
	void Update(UBall* player, float deltaTime);

private: 
	float endingTriggerHeight;
	float clearTargetHeight;
	float slowSpeed;
};