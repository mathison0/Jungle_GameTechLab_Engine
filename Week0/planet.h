#pragma once
#include "UBall.h"

class Planet : public UBall
{
public:
	bool bIsActive = true; 
	const float ExplosionForce = 0.005f; // 터지면서 튕기는 힘
	FVector3 OriginalLocation; // 초기 위치
	FVector3 OriginalVelocity; // 초기 속도

	// 리스폰 타이머 관련 변수들
	float RespawnTimer = 0.0f;
	const float RespawnDelay = 3000.0f;

public:
	Planet(FVector3 startPos, FVector3 startVel, float r, const std::string& textureName = "");

	virtual void HandleCollision(UPrimitive* other)override;
	virtual void Update(float t) override;
	virtual void Explode();
	virtual void Respawn();
	virtual void Render(URenderer& renderer) override;
};