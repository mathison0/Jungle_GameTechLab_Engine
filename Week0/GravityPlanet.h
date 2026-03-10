#pragma once
#include "planet.h"

class GravityPlanet : public Planet
{
public:
	float standardDist = 0.7f; // 계산 가능한 기준 범위
	float maxSpeed = 0.001f;

public:
	GravityPlanet(FVector3 Location, FVector3 Velocity, float r) : Planet(Location, Velocity, r) {}
	virtual ~GravityPlanet() override {}
	void Gravity(UBall* player);
};