#pragma once

#include "UPrimitive.h"
#include "URenderer.h"

class Image
{
protected:
	static ID3D11Buffer* QuadVertexBuffer;

	FVector3 position{ 0.f, 0.f, 1.0f };
	FVector3 scale{ 1.f, 1.f };
	XMFLOAT4 color{ 1.f, 1.f, 1.f, 1.f };
	float angle{};

	float uvOffset{ 0.f };

	std::string textureName{};

	bool bIsActive{ true };

public:

	Image() {}
	Image(const std::string& textureName) : textureName{ textureName } {}
	virtual ~Image() {};

	inline void SetPosition(float x, float y) { position = { x, y }; }
	inline void SetScale(float sw, float sh) { scale = { sw, sh }; }
	inline void SetAngle(float value) { angle = value; }
	inline void SetAlpha(float a) { color.w = a; }

	inline void SetActive(bool active) { bIsActive = active; }
	inline bool IsActive() const { return bIsActive; }

	FVector3 GetScale() { return scale; }

	virtual void Render(URenderer& renderer);

	static void CreateQuadVertexBuffer(URenderer& renderer)
	{
		FVertexSimple quadVertices[] =
		{
			{ -0.5f,  0.5f, 0.0f,  1.0f, 1.0f, 1.0f, 1.0f,	0.0f, 0.0f}, // Top-left vertex
			{  0.5f,  0.5f, 0.0f,  1.0f, 1.0f, 1.0f, 1.0f,	1.0f, 0.0f}, // Top-right vertex
			{ -0.5f, -0.5f, 0.0f,  1.0f, 1.0f, 1.0f, 1.0f,	0.0f, 1.0f}, // Bottom-left vertex
			{ -0.5f, -0.5f, 0.0f,  1.0f, 1.0f, 1.0f, 1.0f,	0.0f, 1.0f}, // Bottom-left vertex (repeated for second triangle)
			{  0.5f,  0.5f, 0.0f,  1.0f, 1.0f, 1.0f, 1.0f,	1.0f, 0.0f}, // Top-right vertex (repeated for second triangle)
			{  0.5f, -0.5f, 0.0f,  1.0f, 1.0f, 1.0f, 1.0f,	1.0f, 1.0f} // Bottom-right vertex
		};
		QuadVertexBuffer = renderer.CreateVertexBuffer(quadVertices, sizeof(quadVertices));
	}

	static void ReleaseQuadVertexBuffer(URenderer& renderer)
	{
		if (QuadVertexBuffer)
		{
			renderer.ReleaseVertexBuffer(QuadVertexBuffer);
			QuadVertexBuffer = nullptr;

		}
	}

};

