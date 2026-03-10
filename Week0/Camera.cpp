#include "Camera.h"

void Camera::Update(float deltaTime, float playerY)
{
	if (playerY > targetCameraY + 0.2f)
	{
		targetCameraY = playerY - 0.2f;
	}
	else if (playerY < targetCameraY - 0.2f)
	{
		targetCameraY = playerY + 0.2f;
	}
	float minCameraHeight = -0.2f;
	if (targetCameraY < minCameraHeight)
	{
		targetCameraY = minCameraHeight;
	}
	float lerpAlpha = 1.0f - expf(-cameraLerpSpeed * deltaTime);
	currentCameraY += (targetCameraY - currentCameraY) * lerpAlpha;
}