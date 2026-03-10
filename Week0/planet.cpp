#include "planet.h"
#include <cmath>
#include <cstdlib>

Planet::Planet(FVector3 startPos, FVector3 startVel, float r, const std::string& textureName)
{
	TextureName = textureName;
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

		// 일정한 힘을 장애물 반대 방향으로
		player->Velocity.x = (normal.x * ExplosionForce);
		player->Velocity.y = (normal.y * ExplosionForce);

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

void Planet::Render(URenderer& renderer)
{
	if (!bIsActive) return;

	// 텍스처 설정 (부모 클래스 로직 사용)
	if (!TextureName.empty())
	{
		ID3D11ShaderResourceView* texture = renderer.GetTexture(TextureName);
		if (texture)
		{
			renderer.DeviceContext->PSSetShaderResources(0, 1, &texture);
			renderer.DeviceContext->PSSetSamplers(0, 1, &renderer.SamplerState);
		}
	}

	// 행성(구체)만 렌더링 - 추진체 제외
	FVector3 sphereTransform = { this->Location.x, this->Location.y, this->Radius };
	renderer.UpdateConstant(sphereTransform, this->Angle);
	renderer.RenderPrimitive(SphereVertexBuffer, NumVerticesSphere);

	// 텍스처 언바인드
	ID3D11ShaderResourceView* nullSRV = nullptr;
	renderer.DeviceContext->PSSetShaderResources(0, 1, &nullSRV);
}

// --- Moon Implementation ---

Moon::Moon(FVector3 startPos, FVector3 startVel, float r, const std::string& textureName)
	: Planet(startPos, startVel, r, textureName)
{
}

Moon::Moon(FVector3 startPos, FVector3 startVel, float r, UBall* target, const std::string& textureName)
	: Planet(startPos, startVel, r, textureName), TargetObject(target)
{
}

void Moon::Update(float t)
{
	if (!bIsActive)
	{
		RespawnTimer += t;
		if (RespawnTimer >= RespawnDelay) Respawn();
		return;
	}

	if (TargetObject != nullptr)
	{
		FVector3 direction = TargetObject->Location - this->Location;
		float distance = direction.Length();

		if (distance > FollowDistance)
		{
			direction = direction / distance;
			float forceMagnitude = (distance - FollowDistance) * FollowSpeed;
			
			this->Velocity.x += direction.x * forceMagnitude * t;
			this->Velocity.y += direction.y * forceMagnitude * t;
			
			float currentSpeed = this->Velocity.Length();
			if (currentSpeed > MaxFollowSpeed)
			{
				this->Velocity.x = (this->Velocity.x / currentSpeed) * MaxFollowSpeed;
				this->Velocity.y = (this->Velocity.y / currentSpeed) * MaxFollowSpeed;
			}
		}
		
		float currentSpeed = this->Velocity.Length();
		
		if (currentSpeed > 0.0001f && distance > 0.0001f)
		{
			direction = direction / distance;
			FVector3 currentDirection = this->Velocity / currentSpeed;
			float cross = currentDirection.x * direction.y - currentDirection.y * direction.x;
			float rotationDirection = (cross > 0.0f) ? 1.0f : -1.0f;
			this->Angle += rotationDirection * RotationSpeed * 10.0f;
		}
		
		this->Velocity.x *= 0.98f;
		this->Velocity.y *= 0.98f;
	}
	
	Location.x += Velocity.x * t;
	Location.y += Velocity.y * t;
}

// --- GravityPlanet Implementation ---

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

	if (dist > standardDist) 
		return;

	float gravityForce = strength / ((dist * dist) + 0.001f);
	FVector3 force = normal * GravityForce;

	FVector3 newVelocity = player->GetVelocity() + force * deltatime;

	if (newVelocity.Length() > maxSpeed)
	{
		newVelocity = newVelocity / newVelocity.Length() * maxSpeed;
	}

	player->SetVelocity(newVelocity);
}

// --- Meteor Implementation ---

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
	float randomX = 0.0f;
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
		player->inputLockTimer = player->inputLockDuration;
		Explode();
	}
}