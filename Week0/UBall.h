#pragma once
#include "dx11math.h"
#include "URenderer.h"
#include "UPrimitive.h"

class UBall : public UPrimitive
{
public:
	static constexpr float MaxSpeed = 0.001f;
	static constexpr float gravity = -0.000001f;
	static constexpr float MaxAttractionForce = 0.001f;

	static ID3D11Buffer* SphereVertexBuffer;
	static UINT NumVerticesSphere;
	static int TotalNumBalls;

	FVector3 Location{};
	FVector3 Velocity{};
	float Radius{};
	float Mass{};


	float Index{};

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

	void Move(float t); 
	void Update(float t) override;
	void UpdateRenderer(URenderer& renderer) override;
	void HandleCollision(UPrimitive* other)override;
	void D(const FVector3& v)override;
	void ClampSpeed();
	void ApplyAttraction(const FVector3& point, float strength);
};