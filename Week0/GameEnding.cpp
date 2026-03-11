#include "GameEnding.h"

GameEnding::GameEnding(float triggerHeight, float clearHeight, float slowSpeed)
	: endingTriggerHeight(triggerHeight)
	, clearTargetHeight(clearHeight)
	, slowSpeed(slowSpeed)
{
}

void GameEnding::Update(UBall* player, float deltaTime)
{
	if (!player) return;

	GameContext& ctx = GameContext::GetiNSTANCE();
	EGameState state = ctx.GetState();

	// Ending 시작
	if (state != EGameState::Ending && state != EGameState::Clear)
	{
		if (player->Location.y >= endingTriggerHeight)
		{
			ctx.SetState(EGameState::Ending);
			return;
		}
	}

	// Ending 상태 자동 이동 
	if (state == EGameState::Ending)
	{
		// 진행 방향을 속도로 판단, 속도가 거의 0이면 위로 이동
		FVector3 vel = player->Velocity;
		float len = vel.Length();
		FVector3 dir = (len > 1e-6f) ? (vel / len) : FVector3(0.0f, 1.0f, 0.0f);

		// 프레임 독립적으로 천천히 이동
		player->Location += dir * slowSpeed * deltaTime;

		// 감속
		player->Velocity *= 0.98f;

		if (player->Location.y >= clearTargetHeight && ctx.GetState() != EGameState::Clear)
		{
			ctx.SetState(EGameState::Clear);
		}
	}
}