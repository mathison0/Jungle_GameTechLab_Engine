#pragma once
#include "planet.h"

class UBall;

class Moon : public Planet
{
private:
	UBall* TargetObject = nullptr;
	float FollowSpeed = 0.000001f;       // 가속력
	float FollowDistance = 0.3f;        
	float MaxFollowSpeed = 0.003f;      // 최대 속도
	float RotationSpeed = 0.002f;       // 회전 속도 (더 크게 조정)

public:
	Moon(FVector3 startPos, FVector3 startVel, float r, const std::string& textureName = "");
	Moon(FVector3 startPos, FVector3 startVel, float r, UBall* target, const std::string& textureName = "");

	void SetTarget(UBall* target) { TargetObject = target; }
	void SetFollowSpeed(float speed) { FollowSpeed = speed; }
	void SetFollowDistance(float distance) { FollowDistance = distance; }
	void SetRotationSpeed(float speed) { RotationSpeed = speed; }
	
	void Update(float t) override;
};