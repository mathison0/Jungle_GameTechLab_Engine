#include "planet.h"

Planet::Planet(FVector3 startPos, FVector3 startVel, float r)
{
	UBall::TotalNumBalls--;

	this->Location = startPos;
	this->OriginalLocation = startPos;

	this->Velocity = startVel;
	this->OriginalVelocity = startVel;

	this->Radius = r;
}

void Planet::Update(float t)
{
	if (!bIsActive)
	{
		RespawnTimer += t;
		if (RespawnTimer >= RespawnDelay) Respawn();
		return;
	}

	Location.x += Velocity.x * t;
	Location.y += Velocity.y * t;
}

void Planet::HandleCollision(UPrimitive* other)
{
	if (!bIsActive) return;

	UBall* player = static_cast<UBall*>(other);
	if (!player) return;

	FVector3 distance = player->Location - this->Location;
	float dist = distance.Length();
	float radiusSum = player->Radius + this->Radius;

	if (dist <= radiusSum)
	{
		// 튕겨나갈 방향
		FVector3 normal;
		if (dist < 0.0001f)
			normal = { 0.0f, 1.0f, 0.0f };
		else
			normal = distance / dist;

		// 겹침 방지를 위한 overlap
		float overlap = radiusSum - dist;
		player->Location.x += normal.x * overlap;
		player->Location.y += normal.y * overlap;

		// 일정한 힘을 
		player->Velocity.x = (normal.x * ExplosionForce) + (this->Velocity.x * 0.5f);
		player->Velocity.y = (normal.y * ExplosionForce) + (this->Velocity.y * 0.5f);

		Explode();
	}
}

void Planet::Explode()
{
	bIsActive = false;
	RespawnTimer = 0.0f;

}

void Planet::Respawn()
{
	bIsActive = true;
	this->Location = OriginalLocation;
	this->Velocity = OriginalVelocity;
}
