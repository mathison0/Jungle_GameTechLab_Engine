#pragma once

class URenderer;
struct FVector3;
class ID3D11Buffer;

class UPrimitive
{
public:
	virtual void Update(float t) = 0;
	virtual void UpdateRenderer(URenderer& renderer) = 0;
	virtual void HandleCollision(UPrimitive* other) = 0;
	virtual void D(const FVector3& v) = 0;
	virtual void ApplyAttraction(const FVector3& point, float strength) = 0;
	virtual ID3D11Buffer* GetVertexBuffer() = 0;
	virtual ~UPrimitive() {}
	virtual void Render(URenderer& renderer) = 0;
}; 