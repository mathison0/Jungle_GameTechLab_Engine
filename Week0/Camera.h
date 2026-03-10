#pragma once
#include "dx11math.h"
class Camera
{
private:
	float targetCameraY{};
	float currentCameraY{};
	float cameraLerpSpeed = 3.0f;

public:
	void Update(float deltaTime, float playerY);

	inline float GetCurrentCameraY() const { return currentCameraY; }
};

