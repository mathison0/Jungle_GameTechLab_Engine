#include "UBall.h"
#include "Type.h"

static FVertexSimple cube_vertices[] =
{
	// Front face (Z+) - UV (0, 0) 추가
	{ -0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f },
	{ -0.5f,  0.5f,  0.5f,  1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f },
	{  0.5f, -0.5f,  0.5f,  0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f },
	{ -0.5f,  0.5f,  0.5f,  1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f },
	{  0.5f,  0.5f,  0.5f,  0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f },
	{  0.5f, -0.5f,  0.5f,  0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f },

	// Back face (Z-)
	{ -0.5f, -0.5f, -0.5f,  0.0f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f },
	{  0.5f, -0.5f, -0.5f,  1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f },
	{ -0.5f,  0.5f, -0.5f,  0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f },
	{ -0.5f,  0.5f, -0.5f,  0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f },
	{  0.5f, -0.5f, -0.5f,  1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f },
	{  0.5f,  0.5f, -0.5f,  1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f },

	// Left face (X-)
	{ -0.5f, -0.5f, -0.5f,  1.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f },
	{ -0.5f,  0.5f, -0.5f,  0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f },
	{ -0.5f, -0.5f,  0.5f,  0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f },
	{ -0.5f,  0.5f, -0.5f,  0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f },
	{ -0.5f,  0.5f,  0.5f,  1.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f },
	{ -0.5f, -0.5f,  0.5f,  0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f },

	// Right face (X+)
	{  0.5f, -0.5f, -0.5f,  1.0f, 0.5f, 0.0f, 1.0f, 0.0f, 0.0f },
	{  0.5f, -0.5f,  0.5f,  0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 0.0f },
	{  0.5f,  0.5f, -0.5f,  0.5f, 0.0f, 0.5f, 1.0f, 0.0f, 0.0f },
	{  0.5f,  0.5f, -0.5f,  0.5f, 0.0f, 0.5f, 1.0f, 0.0f, 0.0f },
	{  0.5f, -0.5f,  0.5f,  0.5f, 0.5f, 0.5f, 1.0f, 0.0f, 0.0f },
	{  0.5f,  0.5f,  0.5f,  0.0f, 0.0f, 0.5f, 1.0f, 0.0f, 0.0f },

	// Top face (Y+)
	{ -0.5f,  0.5f, -0.5f,  0.0f, 1.0f, 0.5f, 1.0f, 0.0f, 0.0f },
	{ -0.5f,  0.5f,  0.5f,  0.0f, 0.5f, 1.0f, 1.0f, 0.0f, 0.0f },
	{  0.5f,  0.5f, -0.5f,  0.5f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f },
	{ -0.5f,  0.5f,  0.5f,  0.0f, 0.5f, 1.0f, 1.0f, 0.0f, 0.0f },
	{  0.5f,  0.5f,  0.5f,  0.5f, 0.5f, 0.0f, 1.0f, 0.0f, 0.0f },
	{  0.5f,  0.5f, -0.5f,  0.5f, 1.0f, 1.0f, 1.0f, 0.0f, 0.0f },

	// Bottom face (Y-)
	{ -0.5f, -0.5f, -0.5f,  0.5f, 0.5f, 0.0f, 1.0f, 0.0f, 0.0f },
	{ -0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f },
	{  0.5f, -0.5f, -0.5f,  1.0f, 0.0f, 0.5f, 1.0f, 0.0f, 0.0f },
	{ -0.5f, -0.5f,  0.5f,  1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f },
	{  0.5f, -0.5f,  0.5f,  0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f },
	{  0.5f, -0.5f, -0.5f,  1.0f, 0.0f, 0.5f, 1.0f, 0.0f, 0.0f },
};

UBall::UBall()
{
	TotalNumBalls++;
	Radius = rand() % 100 * 0.001f + 0.01f;
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
	std::vector<FVertexSimple> circleVertices = GenerateCircleVertices(72);

	SphereVertexBuffer = renderer.CreateVertexBuffer(circleVertices.data(), circleVertices.size() * sizeof(FVertexSimple));
	NumVerticesSphere = (UINT)circleVertices.size();

	std::vector<FVertexSimple> pngCircleVertices = GenerateCircleVertices(72);
	for (auto& vertex : pngCircleVertices)
	{
		vertex.u = -vertex.u; // UV를 음수로 변환
		vertex.v = -vertex.v;
	}
	PNGSphereVertexBuffer = renderer.CreateVertexBuffer(pngCircleVertices.data(), pngCircleVertices.size() * sizeof(FVertexSimple));
	NumVerticesPNGSphere = (UINT)pngCircleVertices.size();

	CubeVertexBuffer = renderer.CreateVertexBuffer(cube_vertices, sizeof(cube_vertices));
	NumVerticesCube = sizeof(cube_vertices) / sizeof(FVertexSimple);
}

void UBall::ReleaseBuffer(URenderer& renderer)
{
	renderer.ReleaseVertexBuffer(SphereVertexBuffer);
	SphereVertexBuffer = nullptr;

	renderer.ReleaseVertexBuffer(PNGSphereVertexBuffer);
	PNGSphereVertexBuffer = nullptr;

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
		Velocity.y  = 0.0f;
	}
}

void UBall::Render(URenderer& renderer)
{
	// 텍스처 설정 (텍스처가 지정되어 있으면)
	if (!TextureName.empty())
	{
		ID3D11ShaderResourceView* texture = renderer.GetTexture(TextureName);
		if (texture)
		{
			renderer.DeviceContext->PSSetShaderResources(0, 1, &texture);
			renderer.DeviceContext->PSSetSamplers(0, 1, &renderer.SamplerState);
		}
	}

	// 1. 메인 공(구체) 렌더링
	FVector3 sphereTransform = { this->Location.x, this->Location.y, this->Radius };
	renderer.UpdateConstant(sphereTransform, this->Angle, { 0, 0, 0 }, 0.f, { 1.0f, 1.0f,1.0f,this->brightness });
	renderer.RenderPrimitive(SphereVertexBuffer, NumVerticesSphere);

	// 텍스처 언바인드 (추진체 렌더링에는 영향 안 가도록)
	ID3D11ShaderResourceView* nullSRV = nullptr;
	renderer.DeviceContext->PSSetShaderResources(0, 1, &nullSRV);

	// --- 시각적 장식(추진체) 추가 , 아래 부분은 플레이어를 파생시킬 때 따라가면 됩니다.---

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

void UBall::ApplyJetpackForce(float thrustAmount)
{
	FVector3 upDir(-sinf(Angle), cosf(Angle), 0.0f);
	Velocity.x += upDir.x * thrustAmount;
	Velocity.y += upDir.y * thrustAmount;
}

void UBall::LimitVelocities(float maxLinearSpeed)
{
	float speedSquared = Velocity.x * Velocity.x + Velocity.y * Velocity.y;
	if (speedSquared > maxLinearSpeed * maxLinearSpeed)
	{
		float speed = sqrtf(speedSquared);
		Velocity.x = (Velocity.x / speed) * maxLinearSpeed;
		Velocity.y = (Velocity.y / speed) * maxLinearSpeed;
	}

	if (fabsf(AngularVelocity) > MaxAngularSpeed)
	{
		AngularVelocity = (AngularVelocity > 0.0f ? 1.0f : -1.0f) * MaxAngularSpeed;
	}
}

void UBall::ApplyThrust(bool bLeftThruster, bool bRightThruster, float deltaTime)
{
	PendingTorque = 0.0f;

	if (!bLeftThruster && !bRightThruster)
	{
		return;
	}

	float thrustPerJetpack = (bLeftThruster && bRightThruster)
		? BothJetpackForce
		: SoleJetpackForce;

	float currentMaxLinearSpeed = (bLeftThruster && bRightThruster)
		? MaxBothJetpackSpeed
		: MaxSoleJetpackSpeed;

	if (bLeftThruster)
	{
		ApplyJetpackForce(thrustPerJetpack * deltaTime);
		PendingTorque -= JetpackTorqueAmount * deltaTime;
	}

	if (bRightThruster)
	{
		ApplyJetpackForce(thrustPerJetpack * deltaTime);
		PendingTorque += JetpackTorqueAmount * deltaTime;
	}

	AngularVelocity += PendingTorque;
	//LimitVelocities(currentMaxLinearSpeed);
}

void UBall::Update(float t)
{
	if (bApplyGravity)
	{
		Velocity.y -= GravityForce * t;
	}

	//Velocity.x *= LinearDamping;
	//Velocity.y *= LinearDamping;

	AngularVelocity *= AngularDamping;

	float autoBalancePower = 0.001f;
	AngularVelocity += -sinf(Angle) * autoBalancePower * t;

	Angle += AngularVelocity * t;

	LimitVelocities(MaxLinearSpeed);
	Move(t);
}

/*현재 전혀 호출되지 않는 함수이므로 지워도 됩니다. 해당 코드로 작업하고 계신 분이 있을까 남겨둡니다.*/
void UBall::UpdateRenderer(URenderer& renderer)
{
	FVector3 transform = { this->Location.x, this->Location.y, this->Radius };
	renderer.UpdateConstant(transform, this->Angle, { 0, 0, 0 }, 0.f, { 1.0f, 1.0f,1.0f,this->brightness });
}

void UBall::HandleCollision(UPrimitive* other)
{
	/*
	float Mass = this->Radius * this->Radius;
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
	*/
}


void UBall::D(const FVector3& v)
{
	this->Velocity.x += v.x;
	this->Velocity.y += v.y;
}

//void UBall::ClampSpeed()
//{
//	float speedSquared = Velocity.x * Velocity.x + Velocity.y * Velocity.y;
//	if (speedSquared > MaxSpeed * MaxSpeed)
//	{
//		float speed = sqrtf(speedSquared);
//		Velocity.x = (Velocity.x / speed) * MaxSpeed;
//		Velocity.y = (Velocity.y / speed) * MaxSpeed;
//	}
//}
//
//void UBall::ClampSpeed2(float maxSpeed)
//{
//	float speedSquared = Velocity.x * Velocity.x + Velocity.y * Velocity.y;
//	if (speedSquared > maxSpeed * maxSpeed)
//	{
//		float speed = sqrtf(speedSquared);
//		Velocity.x = (Velocity.x / speed) * maxSpeed;
//		Velocity.y = (Velocity.y / speed) * maxSpeed;
//	}
//}

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
