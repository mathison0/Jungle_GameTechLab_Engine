#pragma once
#include "dx11math.h"
#include "URenderer.h"
#include "UPrimitive.h"

class UBall : public UPrimitive
{
public:
	static constexpr float MaxSpeed = 0.01f;
	static constexpr float MaxSpeedBothThrusters = 0.0015f;
	static constexpr float gravity = -0.000001f;
	static constexpr float MaxAttractionForce = 0.001f;

	static ID3D11Buffer* SphereVertexBuffer;
	static UINT NumVerticesSphere;
	static ID3D11Buffer* CubeVertexBuffer;
	static UINT NumVerticesCube;

	static int TotalNumBalls;

	FVector3 Location{};
	FVector3 Velocity{};
	float Radius{};
	float Mass{};


	float Index{};

	float Angle = 0.0f;           
	float AngularVelocity = 0.0f; 

	int NumHits{};

	static bool bApplyGravity;
	static bool bApplyAttraction;

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

	void Move(float t); 
	void Update(float t) override;
	void UpdateRenderer(URenderer& renderer) override;
	void HandleCollision(UPrimitive* other)override;
	void D(const FVector3& v)override;
	void ClampSpeed();
	void ClampSpeed2(float maxSpeed);
	void Render(URenderer& renderer) override;
	void ApplyAttraction(const FVector3& point, float strength);
	void ApplyThrust(bool bLeftThruster, bool bRightThruster, float deltaTime);
};