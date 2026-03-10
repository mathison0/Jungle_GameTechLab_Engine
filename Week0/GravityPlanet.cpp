#include "GravityPlanet.h"


void GravityPlanet::Gravity(UBall* player, float deltatime)
{
	if (!this->bIsActive)
		return;

	FVector3 direction = this->Location - player->Location;
	float dist = direction.Length();
	FVector3 normal = direction / dist;

	float strength;
	float sumRadius = this->Radius + player->Radius;

	if (dist < sumRadius && this->planetType == PlanetType::push)
	{
		FVector3 pushVector = (player->Location - this->Location) / (player->Location - this->Location).Length();
		player->Location = this->Location + (pushVector * (sumRadius + 0.001f));

		FVector3 currentVel = player->GetVelocity();
		if (currentVel.Dot(pushVector) < 0)
		{
			player->SetVelocity(pushVector * 0.5f);
		}

		return;
	}


	if (this->planetType == PlanetType::pull)
	{
		standardDist = standardDist_pull;
		maxSpeed = maxSpeed_pull;
		strength = 1.0f;
	}
	else if(this->planetType == PlanetType::push)
	{
		standardDist = standardDist_push;
		maxSpeed = maxSpeed_push;
		normal *= -1.0f;
		strength = 50.0f;
	}

	if (dist > standardDist) // 기준 범위보다 밖에 있을시 계산하지 않음
		return;

	// 거리에 반비례해서 중력이 강해지도록 설정
	float gravityForce = strength / ((dist * dist) + 0.001f); // 0.1f를 이용하여 힘이 0이 되지 않도록 방지
	FVector3 force = normal* GravityForce;

	FVector3 newVelocity = player->GetVelocity() + force * deltatime;

	if (newVelocity.Length() > maxSpeed)
	{
		newVelocity = newVelocity / newVelocity.Length() * maxSpeed;
	}

	player->SetVelocity(newVelocity);
}
