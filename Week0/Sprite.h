#pragma once
#include "dx11math.h"
#include "UPrimitive.h"
#include "URenderer.h"

class Sprite
{
private:
	static ID3D11Buffer* QuadVertexBuffer;

	ID3D11ShaderResourceView* textureResourceView = nullptr;

	FVector3 position{ 0.f, 0.f, 1.0f};
	FVector3 scale{ 1.f, 1.f };
	float uvOffset{ 0.f };

	std::string textureName{};

public:

	Sprite(const std::string& textureName) :textureName(textureName){}
	~Sprite();

	inline void SetPosition(float x, float y) { position = { x, y }; }
	inline void SetScale(float sw, float sh) { scale = { sw, sh }; }

	void Render(URenderer& renderer);

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
		renderer.ReleaseVertexBuffer(QuadVertexBuffer);
		QuadVertexBuffer = nullptr;
	}
};

