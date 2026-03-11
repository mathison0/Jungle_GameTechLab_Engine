#pragma once
#include "dx11math.h"
#include "URenderer.h"
#include "UPrimitive.h"

class UBall : public UPrimitive
{
public:
	float brightness = 1.0f;
public:
	static constexpr float MaxSpeed = 0.01f;
	static constexpr float gravity = -0.000001f;

	static ID3D11Buffer* SphereVertexBuffer;
	static UINT NumVerticesSphere;
	static ID3D11Buffer* CubeVertexBuffer;
	static UINT NumVerticesCube;
	static ID3D11Buffer* PNGSphereVertexBuffer;
	static UINT NumVerticesPNGSphere;

	static int TotalNumBalls;

	FVector3 Location{};
	FVector3 Velocity{};
	float Radius{};

	float Index{};

	float Angle = 0.0f;           
	float AngularVelocity = 0.0f; 

	int NumHits{};

	static bool bApplyGravity;
	static bool bApplyAttraction;

	static constexpr float MaxLinearSpeed = 10.0f;
	static constexpr float MaxAngularSpeed = 3.5f;

	static constexpr float MaxSoleJetpackSpeed = 8.0f;
	static constexpr float MaxBothJetpackSpeed = 10.0f;

	static constexpr float SoleJetpackForce = 4.f;
	static constexpr float BothJetpackForce = 3.f;

	static constexpr float JetpackTorqueAmount = 8.0f;
	static constexpr float LinearDamping = 0.995f;
	static constexpr float AngularDamping = 0.95f;
	static constexpr float GravityForce = 1.8f;

	static constexpr float MaxAttractionForce = 0.001f;

	float PendingTorque = 0.0f;

	std::string TextureName;

	float inputLockTimer = 0.0f;
	float inputLockDuration = 1.0f;

public:
	UBall();
	~UBall();

	static int GetTotalNumBalls() { return TotalNumBalls; }
	static void InitializeBuffer(URenderer& renderer);
	static void ReleaseBuffer(URenderer& renderer);

	virtual ID3D11Buffer* GetVertexBuffer() override
	{
		return SphereVertexBuffer;
	}

	FVector3 GetVelocity() const { return Velocity; }
	void SetVelocity(FVector3 val) { Velocity = val; }
	FVector3 GetLocation() const { return Location; }

	void Move(float t); 
	void Update(float t) override;
	void UpdateRenderer(URenderer& renderer) override;
	virtual void HandleCollision(UPrimitive* other)override;
	void D(const FVector3& v)override;
	/*void ClampSpeed();
	void ClampSpeed2(float maxSpeed);*/
	void Render(URenderer& renderer) override;
	void ApplyAttraction(const FVector3& point, float strength);
	void ApplyThrust(bool bLeftThruster, bool bRightThruster, float deltaTime);
	void ApplyJetpackForce(float thrustAmount);
	void LimitVelocities(float maxLinearSpeed);
};

//여기서 삼각형을 num개 사용해 원을 만들어내는데, num이 많아질 수록 성능은 저하되고 원은 더 부드러워집니다. 
inline std::vector<FVertexSimple> GenerateCircleVertices(int num)
{
	std::vector<FVertexSimple> vertices;
	const float PI = FVector3::PI;

	// 중심점 정점 (기본 색, UV는 중심 0.5, 0.5)
	FVertexSimple center = { 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 0.5f, 0.5f };

	for (int i = 0; i < num; ++i)
	{
		float theta1 = 2.0f * PI * (float)i / (float)num;
		float theta2 = 2.0f * PI * (float)(i + 1) / (float)num;

		//uv좌표계산
		float u1 = 0.5f + cosf(theta1) * 0.5f;
		float v1 = 0.5f - sinf(theta1) * 0.5f;
		float u2 = 0.5f + cosf(theta2) * 0.5f;
		float v2 = 0.5f - sinf(theta2) * 0.5f;

		FVertexSimple v1_vertex = { cosf(theta1), sinf(theta1), 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, u1, v1 };
		FVertexSimple v2_vertex = { cosf(theta2), sinf(theta2), 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, u2, v2 };

		vertices.push_back(center);
		vertices.push_back(v2_vertex);
		vertices.push_back(v1_vertex);
	}

	return vertices;
}