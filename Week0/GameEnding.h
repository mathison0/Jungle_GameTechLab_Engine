#pragma once
#include "GameContext.h"
#include "UBall.h"

class GameEnding
{
public:
	GameEnding(float triggerHeight = 50.f, float clearHeight = 50.4f, float slowSpeed = 0.1f);
	void Update(UBall* player, float deltaTime);

private: 
	float endingTriggerHeight;
	float clearTargetHeight;
	float slowSpeed;
};