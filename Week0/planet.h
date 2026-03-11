#pragma once
#include "UBall.h"
#include <string>

class Planet : public UBall
{
public:
	bool bIsActive = true; 
	const float ExplosionForce = 100.0f; // 터지면서 튕기는 힘
	FVector3 OriginalLocation; // 초기 위치
	FVector3 OriginalVelocity; // 초기 속도

	// 리스폰 타이머 관련 변수들
	float RespawnTimer = 0.0f;
	const float RespawnDelay = 3.0f;

public:
	Planet(FVector3 startPos, FVector3 startVel, float r, const std::string& textureName = "");

	virtual void HandleCollision(UPrimitive* other)override;
	virtual void Update(float t) override;
	virtual void Explode();
	virtual void Respawn();
	virtual void Render(URenderer& renderer) override;
};

//--- Moon ---
class Moon : public Planet
{
private:
	UBall* TargetObject = nullptr;
	float FollowSpeed = 1.0f;
	float FollowDistance = 0.3f;        
	float MaxFollowSpeed = 0.003f;
	float RotationSpeed = 0.002f;

public:
	Moon(FVector3 startPos, FVector3 startVel, float r, const std::string& textureName = "");
	Moon(FVector3 startPos, FVector3 startVel, float r, UBall* target, const std::string& textureName = "");

	void SetTarget(UBall* target) { TargetObject = target; }
	void SetFollowSpeed(float speed) { FollowSpeed = speed; }
	void SetFollowDistance(float distance) { FollowDistance = distance; }
	void SetRotationSpeed(float speed) { RotationSpeed = speed; }
	
	void Update(float t) override;
};

//--- GravityPlanet ---
enum class PlanetType
{
	pull,
	push
};

class GravityPlanet : public Planet
{
public:
	PlanetType planetType;

	float range;

	float maxSpeed = 0;
	float maxSpeed_pull = 0.4f;
	float maxSpeed_push = 2.0f;

private:
	static ID3D11Buffer* RangeDashBuffer;
	static UINT NumDashVertices;
	static int ReferenceCount;
	static ID3D11RasterizerState* NoCullState;

public:
	GravityPlanet(FVector3 Location, FVector3 Velocity, float r, const std::string& textureName = "", PlanetType type = PlanetType::pull)
		: Planet(Location, Velocity, r, textureName), planetType(type) 
	{
		range = r * 5.0f;
		ReferenceCount++;
	}

	virtual ~GravityPlanet() override 
	{
		ReferenceCount--;
		if (ReferenceCount == 0 && RangeDashBuffer)
		{
			RangeDashBuffer->Release();
			RangeDashBuffer = nullptr;
		}
	}

	void Gravity(UBall* player, float deltatime);

	static void InitRangeResources(ID3D11Device* device);
	virtual void Render(URenderer& renderer) override;
};

//--- Meteor ---
class Meteor : public Planet
{
public:
	float spawnDelay;
	float spawnTimer;

public:
	Meteor(FVector3 startPos, FVector3 startVel, float r, const std::string& textureName);

	void Update(float t) override;
	void Respawn() override;
	void HandleCollision(UPrimitive* other) override;
};
