#include "GameEnding.h"
#include <cmath>

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
		// 목표 지점까지의 방향을 매 프레임 계산
		FVector3 targetPos(0.0f, clearTargetHeight, 0.0f);
		FVector3 targetDir = targetPos - player->Location;
		targetDir.Normalize();

		float currentSpeed = player->Velocity.Length();
		player->Velocity = targetDir * currentSpeed;
		player->Move(deltaTime);

		// 기준 방향 벡터 (플레이어의 기본 up 방향: y축)
		FVector3 baseUp(0.0f, 1.0f, 0.0f);

		// 외적을 사용하여 회전 방향 결정
		float cross = baseUp.x * targetDir.y - baseUp.y * targetDir.x;

		// 내적을 사용하여 각도 계산
		float dot = baseUp.x * targetDir.x + baseUp.y * targetDir.y;

		// acos와 부호를 조합하여 각도 계산
		float targetAngle = acosf(dot);
		if (cross < 0.0f)
		{
			targetAngle = -targetAngle;
		}

		player->Angle = targetAngle;

		if (player->Location.y >= clearTargetHeight && ctx.GetState() != EGameState::Clear)
		{
			ctx.SetState(EGameState::Clear);
		}
	}
}