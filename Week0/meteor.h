#pragma once
#include "planet.h"

class Meteor : public Planet
{
public:
	float spawnDelay;
	float spawnTimer;

public:
	Meteor(float r, const std::string& textureName);

	void Update(float t) override;
	void Respawn() override;
	void HandleCollision(UPrimitive* other) override;
};