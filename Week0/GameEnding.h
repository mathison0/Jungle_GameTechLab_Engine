#pragma once
#include "GameContext.h"
#include "UBall.h"

class GameEnding
{
public:
	GameEnding(float triggerHeight = 10.f, float clearHeight = 10.4f, float slowSpeed = 0.1f);
	void Update(UBall* player, float deltaTime);

private: 
	float endingTriggerHeight;
	float clearTargetHeight;
	float slowSpeed;
};