#include "Camera.h"
void Camera::Update(float deltaTime, UBall* player)
{
	float velocityY = player->GetVelocity().y;
	float targetOffset = 0.5f;

	float threshold = 0.5f;

	if (velocityY > threshold)
	{
		targetOffset = 0.5f;
	}
	else if (velocityY < -threshold)
	{
		targetOffset = -0.2f;
	}

	static float smoothOffset = 0.5f;
	float offsetLerpSpeed = 3.0f;
	smoothOffset += (targetOffset - smoothOffset) * (1.0f - expf(-offsetLerpSpeed * deltaTime));

	float desiredTargetY = player->GetLocation().y + smoothOffset;

	if (desiredTargetY < -0.2f)
	{
		desiredTargetY = -0.2f;
	}

	float lerpAlpha = 1.0f - expf(-cameraLerpSpeed * deltaTime);

	currentCameraY += (desiredTargetY - currentCameraY) * lerpAlpha;
}