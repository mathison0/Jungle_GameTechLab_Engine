#include "planet.h"

Planet::Planet(FVector3 startPos, FVector3 startVel, float r, const std::string& textureName)
{
	TextureName = textureName;
	UBall::TotalNumBalls--;

	this->Location = startPos;
	this->OriginalLocation = startPos;

	this->Velocity = startVel;
	this->OriginalVelocity = startVel;

	this->Radius = r;
}

void Planet::Update(float t)
{
	if (!bIsActive)
	{
		RespawnTimer += t;
		if (RespawnTimer >= RespawnDelay) Respawn();
		return;
	}

	Location.x += Velocity.x * t;
	Location.y += Velocity.y * t;
}

void Planet::HandleCollision(UPrimitive* other)
{
	if (!bIsActive) return;

	UBall* player = static_cast<UBall*>(other);
	if (!player) return;

	FVector3 distance = player->Location - this->Location;
	float dist = distance.Length();
	float radiusSum = player->Radius + this->Radius;

	if (dist <= radiusSum)
	{
		// 튕겨나갈 방향
		FVector3 normal;
		if (dist < 0.0001f)
			normal = { 0.0f, 1.0f, 0.0f };
		else
			normal = distance / dist;

		// 겹침 방지를 위한 overlap
		float overlap = radiusSum - dist;
		player->Location.x += normal.x * overlap;
		player->Location.y += normal.y * overlap;

		// 일정한 힘을 장애물 반대 방향으로
		player->Velocity.x = (normal.x * ExplosionForce);
		player->Velocity.y = (normal.y * ExplosionForce);

		Explode();
	}
}

void Planet::Explode()
{
	bIsActive = false;
	RespawnTimer = 0.0f;

}

void Planet::Respawn()
{
	bIsActive = true;
	this->Location = OriginalLocation;
	this->Velocity = OriginalVelocity;
}

void Planet::Render(URenderer& renderer)
{
	if (!bIsActive) return;

	// 텍스처 설정 (부모 클래스 로직 사용)
	if (!TextureName.empty())
	{
		ID3D11ShaderResourceView* texture = renderer.GetTexture(TextureName);
		if (texture)
		{
			renderer.DeviceContext->PSSetShaderResources(0, 1, &texture);
			renderer.DeviceContext->PSSetSamplers(0, 1, &renderer.SamplerState);
		}
	}

	// 행성(구체)만 렌더링 - 추진체 제외
	FVector3 sphereTransform = { this->Location.x, this->Location.y, this->Radius };
	renderer.UpdateConstant(sphereTransform, 0.0f);
	renderer.RenderPrimitive(SphereVertexBuffer, NumVerticesSphere);

	// 텍스처 언바인드
	ID3D11ShaderResourceView* nullSRV = nullptr;
	renderer.DeviceContext->PSSetShaderResources(0, 1, &nullSRV);
}