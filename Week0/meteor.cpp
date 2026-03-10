#include "meteor.h"
#include <cstdlib>

Meteor::Meteor(float r, const std::string& textureName)
	: Planet({ 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, r, textureName)
{
	Respawn();
}

void Meteor::Update(float t)
{
	if (!bIsActive)
	{
		spawnTimer += t;

		if (spawnTimer >= spawnDelay)
		{
			bIsActive = true;
			Location = OriginalLocation;
			Velocity = OriginalVelocity;
		}
		return;
	}

	Location.x += Velocity.x * t;
	Location.y += Velocity.y * t;

	if (Location.y < -1.2f)
	{
		Respawn();
	}
}

void Meteor::Respawn()
{
	// 랜덤 설정
	//float randomX = ((rand() % 181) / 100.0f) - 0.9f;
	float randomX = 0.0f; // 디버깅용 중앙에서 떨어지는 유성
	float randomSpeed = 0.0003f + (rand() % 8) * 0.0001f;

	OriginalLocation = { randomX, 1.2f, 0.0f };
	OriginalVelocity = { 0.0f, -randomSpeed, 0.0f };

	spawnDelay = static_cast<float>(rand() % 5000);
	spawnTimer = 0.0f;

	Location = OriginalLocation;
	Velocity = OriginalVelocity;

	bIsActive = false;
}

void Meteor::HandleCollision(UPrimitive* other)
{
	if (!bIsActive) return;

	UBall* player = static_cast<UBall*>(other);
	if (!player) return;

	FVector3 distance = player->Location - this->Location;
	float dist = distance.Length();
	float radiusSum = player->Radius + this->Radius;

	if (dist <= radiusSum)
	{
		//OutputDebugStringA("Meteor collision detected!\n"); // 디버깅
		// 유성과 충돌했을 때 입력 잠금
		player->inputLockTimer = player->inputLockDuration;
	
		Explode();
	}
}