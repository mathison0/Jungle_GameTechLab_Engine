#include "UBall.h"
#include "Sphere.h"

UBall::UBall()
{
	TotalNumBalls++;
	Radius = rand() % 100 * 0.001f + 0.01f;
	Mass = Radius * Radius;
	Location.x = ((float)(rand() % 200 - 100)) * 0.01f;
	Location.y = ((float)(rand() % 200 - 100)) * 0.01f;
	float initialSpeed = 0.0005f;
	float randomAngle = (float)(rand() % 360) * (3.141592f / 180.0f);

	Velocity.x = cosf(randomAngle) * initialSpeed;
	Velocity.y = sinf(randomAngle) * initialSpeed;
	Velocity.z = 0.0f;
}

UBall::~UBall()
{
	TotalNumBalls--;
}

void UBall::InitializeBuffer(URenderer& renderer)
{
	SphereVertexBuffer = renderer.CreateVertexBuffer(sphere_vertices, sizeof(sphere_vertices));
	NumVerticesSphere = sizeof(sphere_vertices) / sizeof(FVertexSimple);

	CubeVertexBuffer = renderer.CreateVertexBuffer(cube_vertices, sizeof(cube_vertices));
	NumVerticesCube = sizeof(cube_vertices) / sizeof(FVertexSimple);
}

void UBall::ReleaseBuffer(URenderer& renderer)
{
	renderer.ReleaseVertexBuffer(SphereVertexBuffer);
	SphereVertexBuffer = nullptr;

	renderer.ReleaseVertexBuffer(CubeVertexBuffer);
	CubeVertexBuffer = nullptr;
}

void UBall::Move(float t)
{
	Location.x += Velocity.x * t;
	Location.y += Velocity.y * t;

	if (Location.x < -1.0f + Radius)
	{
		Location.x = -1.0f + Radius;
		Velocity.x *= -0.5f;
	}
	else if (Location.x > 1.0f - Radius)
	{
		Location.x = 1.0f - Radius;
		Velocity.x *= -0.5f;
	}
	if (Location.y < -1.0f + Radius)
	{
		Location.y = -1.0f + Radius;
		Velocity.y *= -0.8f;
	}
}

void UBall::Update(float t)
{
	// 1. 중력도 스케일에 맞춰 낮춤 (천천히 가속되며 떨어짐)
	Velocity.y -= 0.000002f * t;

	// 2. 우주의 마찰력 (조작하지 않을 때 미끄러지듯 감속)
	Velocity.x *= 0.995f;
	Velocity.y *= 0.995f;

	// 회전 마찰력 
	AngularVelocity *= 0.95f;

	// [추가] 3. 오뚝이(Auto-Balance) 기능
	// sinf(Angle)의 반대 방향으로 아주 미세한 힘을 가해 항상 0도(위쪽)를 향하도록 유도합니다.
	float autoBalancePower = 0.000001f; // 복원력 강도 (너무 세면 덜렁거리니 미세하게 조절)
	AngularVelocity += -sinf(Angle) * autoBalancePower * t;

	// 4. 각도 업데이트
	Angle += AngularVelocity * t;

	ClampSpeed();
	Move(t);
}

void UBall::Render(URenderer& renderer)
{
	// 1. 메인 공(구체) 렌더링
	FVector3 sphereTransform = { this->Location.x, this->Location.y, this->Radius };
	renderer.UpdateConstant(sphereTransform, this->Angle);
	renderer.RenderPrimitive(SphereVertexBuffer, NumVerticesSphere);

	// --- 시각적 장식(추진체) 추가 ---

	// 2. 사각형의 크기를 키웁니다 (기존 0.4f -> 0.8f)
	float thrusterScale = this->Radius * 0.8f;

	// 3. 겹침 방지를 위해 약간의 여백(Padding)을 줍니다.
	float padding = this->Radius * 0.1f; // 원 반경의 10%만큼 틈새를 만듦

	// 4. 공 중심에서 날개가 떨어져 있을 거리 = 원의 반지름 + 여백 + 사각형 절반
	float offsetDist = this->Radius + padding + (thrusterScale * 0.5f);

	// 5. 현재 회전 각도(Angle)를 기준으로 로컬 Right 방향(양옆 방향) 계산
	float rightX = cosf(this->Angle);
	float rightY = sinf(this->Angle);

	// 왼쪽 날개 렌더링 (중심에서 -Right 방향으로 이동)
	FVector3 leftPos = {
		this->Location.x - rightX * offsetDist,
		this->Location.y - rightY * offsetDist,
		thrusterScale
	};
	renderer.UpdateConstant(leftPos, this->Angle);
	renderer.RenderPrimitive(CubeVertexBuffer, NumVerticesCube);

	// 오른쪽 날개 렌더링 (중심에서 +Right 방향으로 이동)
	FVector3 rightPos = {
		this->Location.x + rightX * offsetDist,
		this->Location.y + rightY * offsetDist,
		thrusterScale
	};
	renderer.UpdateConstant(rightPos, this->Angle);
	renderer.RenderPrimitive(CubeVertexBuffer, NumVerticesCube);
}

void UBall::ApplyThrust(bool bLeftThruster, bool bRightThruster, float deltaTime)
{
	float thrustPower = 0.0001f;
	float torquePower = 0.0001f;

	FVector3 forwardDir(-sinf(Angle), cosf(Angle), 0.0f);
	FVector3 rightDir(cosf(Angle), sinf(Angle), 0.0f);

	if (bLeftThruster) // A키 (왼쪽 추진체 탭!)
	{
		FVector3 pushDir = forwardDir * 0.8f + rightDir * 0.5f;
		Velocity = Velocity + (pushDir * thrustPower);

		// [추가] 이미 오른쪽(도려는 방향)으로 기울어진 상태라면 회전력을 줄임
		float modifier = 1.0f;
		if (sinf(Angle) < 0.0f)
		{
			// 똑바로 서 있을 때(cos=1)는 100%, 눕거나 뒤집힐수록 최소 20%까지 힘을 제한
			float currentCos = cosf(Angle);
			modifier = currentCos < 0.2f ? 0.2f : currentCos;
		}
		AngularVelocity -= torquePower * modifier;
	}

	if (bRightThruster) // D키 (오른쪽 추진체 탭!)
	{
		FVector3 pushDir = forwardDir * 0.8f - rightDir * 0.5f;
		Velocity = Velocity + (pushDir * thrustPower);

		// [추가] 이미 왼쪽(도려는 방향)으로 기울어진 상태라면 회전력을 줄임
		float modifier = 1.0f;
		if (sinf(Angle) > 0.0f)
		{
			float currentCos = cosf(Angle);
			modifier = currentCos < 0.2f ? 0.2f : currentCos;
		}
		AngularVelocity += torquePower * modifier;
	}

	if (bLeftThruster && bRightThruster)
	{
		ClampSpeed2(MaxSpeedBothThrusters);
	}
	else
	{
		ClampSpeed2(MaxSpeed);
	}
}

void UBall::UpdateRenderer(URenderer& renderer)
{
	FVector3 transform = { this->Location.x, this->Location.y, this->Radius };
	renderer.UpdateConstant(transform, this->Angle);
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

void UBall::ClampSpeed2(float maxSpeed)
{
	float speedSquared = Velocity.x * Velocity.x + Velocity.y * Velocity.y;
	if (speedSquared > maxSpeed * maxSpeed)
	{
		float speed = sqrtf(speedSquared);
		Velocity.x = (Velocity.x / speed) * maxSpeed;
		Velocity.y = (Velocity.y / speed) * maxSpeed;
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
