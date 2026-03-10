#include "GravityPlanet.h"


void GravityPlanet::Gravity(UBall* player)
{
	if (!this->bIsActive)
		return;

	FVector3 direction = this->Location - player->Location;
	float dist = direction.Length();
	FVector3 normal = direction / dist;

	if (dist > standardDist) // 기준 범위보다 밖에 있을시 계산하지 않음
		return;

	// 거리에 반비례해서 중력이 강해지도록 설정
	float gravityForce = 1.0f / (pow(dist, 2) + 0.1f); // 0.1f를 이용하여 힘이 0이 되지 않도록 방지
	FVector3 force = normal * GravityForce;

	FVector3 newVelocity = player->GetVelocity() + force;

	if (newVelocity.Length() > maxSpeed)
	{
		newVelocity = newVelocity / newVelocity.Length() * maxSpeed;
	}

	player->SetVelocity(newVelocity);
}
