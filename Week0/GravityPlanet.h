#pragma once
#include "planet.h"

enum class PlanetType
{
	pull,
	push
};

class GravityPlanet : public Planet
{
public:
	PlanetType planetType;
	float standardDist = 0;
	float maxSpeed = 0;

	float standardDist_pull = 0.7f; // 계산 가능한 기준 범위
	float maxSpeed_pull = 0.4f;

	float standardDist_push = 0.7f;
	float maxSpeed_push = 2.0f;

public:
	GravityPlanet(FVector3 Location, FVector3 Velocity, float r, const std::string& textureName = "", PlanetType type = PlanetType::pull)
		: Planet(Location, Velocity, r, textureName), planetType(type) {}

	virtual ~GravityPlanet() override {}
	void Gravity(UBall* player, float deltatime);
};