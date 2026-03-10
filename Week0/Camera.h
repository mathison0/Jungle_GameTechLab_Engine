#pragma once
#include "dx11math.h"
#include "UBall.h"

class Camera
{
private:
	float targetCameraY{};
	float currentCameraY{};
	float cameraLerpSpeed = 20.0f;

public:
	void Update(float deltaTime, UBall* player);

	inline float GetCurrentCameraY() const { return currentCameraY; }
};

