#include "Render/Geometry/SpriteParticleGeometry.h"

void FSpriteParticleGeometry::Create(ID3D11Device* InDevice)
{
	VertexBuffer.Create(InDevice, 1024, VertexStride);
	IndexBuffer.Create(InDevice, 1024 * 6);
}

void FSpriteParticleGeometry::Release()
{
	VertexBuffer.Release();
	IndexBuffer.Release();
}

void FSpriteParticleGeometry::Clear()
{
	Vertices.clear();
	Indices.clear();
	IndexCount = 0;
}

void FSpriteParticleGeometry::AddParticleQuad(const FBaseParticle& Particle, const FVector& CameraRight, const FVector& CameraUp)
{
	AddParticleQuad(Particle, CameraRight, CameraUp, false, 0.0f, { 0, 0 }, { 1, 0 }, { 0, 1 }, { 1, 1 });
}

void FSpriteParticleGeometry::AddParticleQuad(const FBaseParticle& Particle, const FVector& CameraRight, const FVector& CameraUp,
	const FVector2& TopLeftUV, const FVector2& TopRightUV, const FVector2& BottomLeftUV, const FVector2& BottomRightUV)
{
	AddParticleQuad(Particle, CameraRight, CameraUp, false, 0.0f, TopLeftUV, TopRightUV, BottomLeftUV, BottomRightUV);
}

void FSpriteParticleGeometry::AddParticleQuad(const FBaseParticle& Particle, const FVector& CameraRight, const FVector& CameraUp,
	bool bVelocityScreenAligned, float MinVelocitySquared,
	const FVector2& TopLeftUV, const FVector2& TopRightUV, const FVector2& BottomLeftUV, const FVector2& BottomRightUV)
{
	const FVector Center = Particle.Position;
	FVector AxisRight = CameraRight;
	FVector AxisUp = CameraUp;

	if (bVelocityScreenAligned && Particle.Velocity.LengthSquared() >= MinVelocitySquared)
	{
		const float ScreenVelocityX = Particle.Velocity.Dot(CameraRight);
		const float ScreenVelocityY = Particle.Velocity.Dot(CameraUp);
		const float ScreenVelocityLengthSquared = ScreenVelocityX * ScreenVelocityX + ScreenVelocityY * ScreenVelocityY;

		if (ScreenVelocityLengthSquared >= MinVelocitySquared)
		{
			AxisRight = CameraRight * ScreenVelocityX + CameraUp * ScreenVelocityY;
			AxisRight.Normalize();
			AxisUp = CameraRight * -ScreenVelocityY + CameraUp * ScreenVelocityX;
			AxisUp.Normalize();
		}
	}

	const FVector HalfRight = AxisRight * (Particle.Size.X * 0.5f);
	const FVector HalfUp = AxisUp * (Particle.Size.Y * 0.5f);

	const FVector TopLeft = Center - HalfRight + HalfUp;
	const FVector TopRight = Center + HalfRight + HalfUp;
	const FVector BottomLeft = Center - HalfRight - HalfUp;
	const FVector BottomRight = Center + HalfRight - HalfUp;

	AddQuad(TopLeft, TopRight, BottomLeft, BottomRight, Particle.Color, TopLeftUV, TopRightUV, BottomLeftUV, BottomRightUV);
}

void FSpriteParticleGeometry::AddQuad(const FVector& TopLeft, const FVector& TopRight, const FVector& BottomLeft, const FVector& BottomRight,
	const FVector4& Color, const FVector2& TopLeftUV, const FVector2& TopRightUV, const FVector2& BottomLeftUV, const FVector2& BottomRightUV)
{
	AddQuad(TopLeft, TopRight, BottomLeft, BottomRight,
		Color, Color, Color, Color,
		TopLeftUV, TopRightUV, BottomLeftUV, BottomRightUV);
}

void FSpriteParticleGeometry::AddQuad(const FVector& TopLeft, const FVector& TopRight, const FVector& BottomLeft, const FVector& BottomRight,
	const FVector4& TopLeftColor, const FVector4& TopRightColor, const FVector4& BottomLeftColor, const FVector4& BottomRightColor,
	const FVector2& TopLeftUV, const FVector2& TopRightUV, const FVector2& BottomLeftUV, const FVector2& BottomRightUV)
{
	const uint32 StartIndex = static_cast<uint32>(Vertices.size());
	Vertices.push_back({ TopLeft, TopLeftColor, TopLeftUV });
	Vertices.push_back({ TopRight, TopRightColor, TopRightUV });
	Vertices.push_back({ BottomLeft, BottomLeftColor, BottomLeftUV });
	Vertices.push_back({ BottomRight, BottomRightColor, BottomRightUV });
	Indices.push_back(StartIndex + 0);
	Indices.push_back(StartIndex + 1);
	Indices.push_back(StartIndex + 2);
	Indices.push_back(StartIndex + 1);
	Indices.push_back(StartIndex + 3);
	Indices.push_back(StartIndex + 2);
	IndexCount += 6;
}
uint32 FSpriteParticleGeometry::AddVertex(const FParticleSpriteVertex& Vertex)
{
	Vertices.push_back(Vertex);
	return static_cast<uint32>(Vertices.size() - 1);
}

void FSpriteParticleGeometry::AddTriangle(uint32 A, uint32 B, uint32 C)
{
	Indices.push_back(A);
	Indices.push_back(B);
	Indices.push_back(C);
	IndexCount += 3;
}

bool FSpriteParticleGeometry::Upload(ID3D11Device* Device, ID3D11DeviceContext* Context)
{
	if (!Device || !Context)
	{
		return false;
	}

	if (Vertices.empty() || Indices.empty())
	{
		return false;
	}

	VertexBuffer.EnsureCapacity(Device, static_cast<uint32>(Vertices.size()));
	IndexBuffer.EnsureCapacity(Device, static_cast<uint32>(Indices.size()));
	
	if (!VertexBuffer.Update(Context, Vertices.data(), static_cast<uint32>(Vertices.size())))
	{
		return false;
	}
	if (!IndexBuffer.Update(Context, Indices.data(), static_cast<uint32>(Indices.size())))
	{
		return false;
	}
	return true;
}
