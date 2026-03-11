#pragma once
#include "GameContext.h"
#include "UBall.h"

class GameEnding
{
public:
	GameEnding(float triggerHeight = 3.f, float clearHeight = 3.5f, float slowSpeed = 0.1f);
	void Update(UBall* player, float deltaTime);

private:
	float endingTriggerHeight;
	float clearTargetHeight;
	float slowSpeed;
};