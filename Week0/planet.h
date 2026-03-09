#pragma once
#include "dx11math.h"

class UBall;

class Planet
{
public:
	FVector3 Location;
	float radius;
	float mass;

public:
	Planet(FVector3 L, float r);

	bool Collision(UBall* other); // 충돌 판정
	void Boom(UBall* other); // 터지며 튕겨내는 함수

	// 충돌의 세기를 조정할 수 있는 변수 추가
};