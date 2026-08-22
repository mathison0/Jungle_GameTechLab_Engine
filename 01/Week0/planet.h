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
	const float RespawnDelay = 3.5f;

public:
	Planet(FVector3 startPos, FVector3 startVel, float r, const std::string& textureName = "");
	virtual ~Planet() {};

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
	float FollowDistance = 0.1f;
	float MaxFollowSpeed = 0.003f;
	float RotationSpeed = 0.002f;

	bool bIsFollowing = false;
	bool bIsHiddenByCollision = false;
	float HideTimer = 0.0f;

public:
	Moon(FVector3 startPos, FVector3 startVel, float r, const std::string& textureName = "");
	Moon(FVector3 startPos, FVector3 startVel, float r, UBall* target, const std::string& textureName = "");

	void SetTarget(UBall* target) { TargetObject = target; }
	void SetFollowSpeed(float speed) { FollowSpeed = speed; }
	void SetFollowDistance(float distance) { FollowDistance = distance; }
	void SetRotationSpeed(float speed) { RotationSpeed = speed; }
	
	void Update(float t) override;
	virtual void Render(URenderer& renderer) override;
	virtual void HandleCollision(UPrimitive* other) override;
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
	static UBall* targetPlayer;

	float range;

	float maxSpeed = 0;
	float maxSpeed_pull = 5.0f;
	float maxSpeed_push = 3.0f;

	XMFLOAT4 Color_pull = { 1.0f, 0.0f, 0.0f, 1.0f};
	XMFLOAT4 Color_push = { 0.0f, 0.0f, 1.0f, 1.0f};

private:
	static ID3D11Buffer* RangeDashBuffer;
	static UINT NumDashVertices;
	static ID3D11RasterizerState* NoCullState;

public:
	GravityPlanet(FVector3 Location, FVector3 Velocity, float r, const std::string& textureName = "", PlanetType type = PlanetType::pull)
		: Planet(Location, Velocity, r, textureName), planetType(type) 
	{
		range = r * 3.0f;
	}

	virtual ~GravityPlanet() override 
	{
	}

	static void SetGravitySystem(UBall* player);
	void Update(float t) override;
	static void InitRangeResources(ID3D11Device* device);
	static void ReleaseRangeResources();
	virtual void Render(URenderer& renderer) override;
};

//--- Meteor ---
class Meteor : public Planet
{
public:
	UBall* targetPlayer = nullptr;
	float waitDuration = 0.0f;
	float waitTimer = 0.f;
	bool bIsWaiting = false;

public:
	Meteor(UBall* target, float r, const std::string& textureName);

	void Update(float t) override;
	void HandleCollision(UPrimitive* other) override;
	void HideAndWait();
	void RespawnAbovePlayer();
};
