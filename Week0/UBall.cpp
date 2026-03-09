#include "UBall.h"
#include "Sphere.h"

UBall::UBall()
{
	TotalNumBalls++;
	Radius = rand() % 100 * 0.001f + 0.01f;
	Mass = Radius * Radius;
	Location.x = ((float)(rand() % 200 - 100)) * 0.01f;
	Location.y = ((float)(rand() % 200 - 100)) * 0.01f;

	Velocity.x = ((float)(rand() % 100 - 50)) * 0.00001f;
}

UBall::~UBall()
{
	TotalNumBalls--;
}

void UBall::InitializeBuffer(URenderer& renderer)
{
	SphereVertexBuffer = renderer.CreateVertexBuffer(sphere_vertices, sizeof(sphere_vertices));
	NumVerticesSphere = sizeof(sphere_vertices) / sizeof(FVertexSimple);
}

void UBall::ReleaseBuffer(URenderer& renderer)
{
	renderer.ReleaseVertexBuffer(SphereVertexBuffer);
	SphereVertexBuffer = nullptr;
}

void UBall::Move(float t)
{
	Location.x += Velocity.x * t;
	Location.y += Velocity.y * t;

	if (Location.x < -1.0f + Radius)
	{
		Location.x = -1.0f + Radius;
		Velocity.x *= -1;
	}
	else if (Location.x > 1.0f - Radius)
	{
		Location.x = 1.0f - Radius;
		Velocity.x *= -1;
	}
	if (Location.y < -1.0f + Radius)
	{
		Location.y = -1.0f + Radius;
		Velocity.y *= -0.8f;
	}
	else if (Location.y > 1.0f - Radius)
	{
		Location.y = 1.0f - Radius;
		Velocity.y *= -1;
	}
}

void UBall::Update(float t)
{
	if (bApplyGravity)
	{
		Velocity.y += gravity * t;
	}


	ClampSpeed();

	Move(t);
}

void UBall::UpdateRenderer(URenderer& renderer)
{
	FVector3 transform = { this->Location.x, this->Location.y, this->Radius };
	renderer.UpdateConstant(transform);
}

void UBall::HandleCollision(UPrimitive* other)
{
	UBall* otherBall = static_cast<UBall*>(other);

	float deltaX = otherBall->Location.x - Location.x;
	float deltaY = otherBall->Location.y - Location.y;

	float distanceSquared = deltaX * deltaX + deltaY * deltaY;

	float radiusSum = Radius + otherBall->Radius;
	float radiusSumSquared = radiusSum * radiusSum;

	if (distanceSquared <= radiusSumSquared)
	{
		float distance = sqrtf(distanceSquared);
		distance = distance < 0.0001f ? 0.0001f : distance;
		float normalX = deltaX / distance;
		float normalY = deltaY / distance;

		float relativeVelocityX = otherBall->Velocity.x - Velocity.x;
		float relativeVelocityY = otherBall->Velocity.y - Velocity.y;

		float velocityAlongNormal = relativeVelocityX * normalX + relativeVelocityY * normalY;



		float overlap = radiusSum - distance;
		float totalInverseMass = (1.0f / Mass) + (1.0f / otherBall->Mass);

		float percent = 0.2f;
		float correctionMagnitude = (overlap / totalInverseMass) * percent;
		float moveX = correctionMagnitude * normalX;
		float moveY = correctionMagnitude * normalY;

		this->Location.x -= moveX * (1.0f / Mass);
		this->Location.y -= moveY * (1.0f / Mass);
		otherBall->Location.x += moveX * (1.0f / otherBall->Mass);
		otherBall->Location.y += moveY * (1.0f / otherBall->Mass);

		if (velocityAlongNormal > 0)
		{
			return;
		}

		float elasticity = 1.0f;

		float impulseMagnitude = -(1 + elasticity) * velocityAlongNormal;
		impulseMagnitude /= (1 / Mass) + (1 / otherBall->Mass);

		FVector3 impulseThis = { -(impulseMagnitude / Mass) * normalX, -(impulseMagnitude / Mass) * normalY, 0.0f };
		FVector3 impulseOther = { (impulseMagnitude / otherBall->Mass) * normalX, (impulseMagnitude / otherBall->Mass) * normalY, 0.0f };

		D(impulseThis);
		other->D(impulseOther);
	}
}


void UBall::D(const FVector3& v)
{
	this->Velocity.x += v.x;
	this->Velocity.y += v.y;
}

void UBall::ClampSpeed()
{
	float speedSquared = Velocity.x * Velocity.x + Velocity.y * Velocity.y;
	if (speedSquared > MaxSpeed * MaxSpeed)
	{
		float speed = sqrtf(speedSquared);
		Velocity.x = (Velocity.x / speed) * MaxSpeed;
		Velocity.y = (Velocity.y / speed) * MaxSpeed;
	}
}

void UBall::ApplyAttraction(const FVector3& point, float strength)
{
	if (bApplyAttraction == false)
	{
		return;
	}

	float deltaX = point.x - Location.x;
	float deltaY = point.y - Location.y;

	float distanceSquared = deltaX * deltaX + deltaY * deltaY + 0.001f;
	distanceSquared = distanceSquared < 0.0001f ? 0.0001f : distanceSquared;

	float distance = sqrtf(distanceSquared);
	float forceMagnitude = strength / distanceSquared;
	forceMagnitude = forceMagnitude > MaxAttractionForce ? MaxAttractionForce : forceMagnitude;
	float forceX = (deltaX / distance) * forceMagnitude;
	float forceY = (deltaY / distance) * forceMagnitude;

	FVector3 force = { forceX, forceY, 0.0f };
	D(force);
}
