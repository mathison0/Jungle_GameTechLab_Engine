#include "planet.h"
#include "SoundManager.h"
#include <cmath>
#include <cstdlib>

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

	//Location.x += Velocity.x * t;
	//Location.y += Velocity.y * t;
}

void Planet::HandleCollision(UPrimitive* other)
{
	if (!bIsActive) return;

	UBall* player = static_cast<UBall*>(other);
	if (!player) return;
	if (player->bInvincible) return;

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

		// 행성의 질량(반지름)에 비례하는 튕김 힘
		// 큰 행성일수록 더 강하게 튕겨냄
		float explosionForce = 7.f * this->Radius;

		player->Velocity.x = normal.x * explosionForce;
		player->Velocity.y = normal.y * explosionForce;
		player->inputLockTimer = player->inputLockDuration;


		Moon* moon = dynamic_cast<Moon*>(player);
		if (moon) return;
		Explode();
	}

}

void Planet::Explode()
{
	bIsActive = false;
	RespawnTimer = 0.0f;
	SoundManager::Get().PlayEffect("Explosion");
}

void Planet::Respawn()
{
	bIsActive = true;
	this->Location = OriginalLocation;
	this->Velocity = OriginalVelocity;
}

void Planet::Render(URenderer& renderer)
{
	//if (!bIsActive)
	//{
	//	return;
	//}

	bool bIsPNGTexture = (TextureName == "Meteor");

	if (bIsPNGTexture)
	{
		renderer.EnableAlphaBlending(true);
	}


	// 텍스처 설정 (부모 클래스 로직 사용)
	if (!TextureName.empty())
	{
		ID3D11ShaderResourceView* texture = renderer.GetTexture(TextureName);
		if (texture)
		{
			int slot = bIsPNGTexture ? 1 : 0;
			renderer.DeviceContext->PSSetShaderResources(slot, 1, &texture);
			renderer.DeviceContext->PSSetSamplers(0, 1, &renderer.SamplerState);
		}
	}


	renderer.DeviceContext->VSSetShader(renderer.SimpleVertexShader, nullptr, 0);
	renderer.DeviceContext->PSSetShader(renderer.SimplePixelShader, nullptr, 0);


	ID3D11Buffer* bufferToUse = bIsPNGTexture ? UBall::PNGSphereVertexBuffer : UBall::SphereVertexBuffer;
	UINT numVertices = bIsPNGTexture ? UBall::NumVerticesPNGSphere : UBall::NumVerticesSphere;

	// 행성(구체)만 렌더링 - 추진체 제외
	FVector3 sphereTransform = { this->Location.x, this->Location.y, this->Radius };
	float colorAfterCollision = 0.5f;
	if (bIsActive)
	{
		renderer.UpdateConstant(sphereTransform, this->Angle, { 0, 0, 0 }, { 1.0f, 1.0f, 1.0f,this->brightness }, { 0.f,0.f }, { 0.f,0.f });
	}
	else
	{
		renderer.UpdateConstant(sphereTransform, this->Angle, { 0, 0, 0 }, { 1.0f, 1.0f, 1.0f,this->brightness * colorAfterCollision }, { 0.f,0.f }, { 0.f,0.f });
	}
	renderer.RenderPrimitive(bufferToUse, numVertices);

	if (bIsPNGTexture)
	{
		renderer.EnableAlphaBlending(false);
	}

	// 텍스처 언바인드
	ID3D11ShaderResourceView* nullSRV[2] = { nullptr, nullptr };
	renderer.DeviceContext->PSSetShaderResources(0, 2, nullSRV);
}

// --- Moon Implementation ---

Moon::Moon(FVector3 startPos, FVector3 startVel, float r, const std::string& textureName)
	: Planet(startPos, startVel, r, textureName)
{
}

Moon::Moon(FVector3 startPos, FVector3 startVel, float r, UBall* target, const std::string& textureName)
	: Planet(startPos, startVel, r, textureName), TargetObject(target)
{
}

void Moon::Update(float t)
{
	if (!bIsActive)
	{
		RespawnTimer += t;
		if (RespawnTimer >= RespawnDelay) Respawn();
		return;
	}

	if (bIsHiddenByCollision)
	{
		HideTimer += t;
		if (HideTimer >= 5.0f)
		{
			bIsHiddenByCollision = false;
			if (TargetObject && TargetObject->Location.y >= 30.0f)
			{
				this->Location = TargetObject->Location + FVector3(0.0f, -2.0f, 0.0f);
			}
		}
		else
		{
			this->Location = FVector3(0.0f, -1000.0f, 0.0f);
			return;
		}
	}

	if (TargetObject != nullptr)
	{
		if (TargetObject->Location.y >= 30.0f)
		{
			if (!bIsFollowing)
			{
				bIsFollowing = true;
				//처음 30.f를 돌파했을 때 화면 밖 근처에서 나타나게 함
				this->Location = TargetObject->Location + FVector3(0.0f, -3.0f, 0.0f);
				this->Velocity = FVector3(0.0f, 0.0f, 0.0f);
			}

			// --- 기존 추적 로직 시작 ---
			FVector3 direction = TargetObject->Location - this->Location;
			float distance = direction.Length();

			if (distance > FollowDistance)
			{
				direction = direction / distance;
				float forceMagnitude = (distance - FollowDistance) * FollowSpeed;

				this->Velocity.x += direction.x * forceMagnitude * t;
				this->Velocity.y += direction.y * forceMagnitude * t;
			}

			float currentSpeed = this->Velocity.Length();
			if (currentSpeed > 0.0001f && distance > 0.0001f)
			{
				direction = direction / distance;
				FVector3 currentDirection = this->Velocity / currentSpeed;
				float cross = currentDirection.x * direction.y - currentDirection.y * direction.x;
				float rotationDirection = (cross > 0.0f) ? 1.0f : -1.0f;
				this->Angle += rotationDirection * RotationSpeed * 10.0f;
			}

			this->Velocity.x *= 0.98f;
			this->Velocity.y *= 0.98f;
		}
		else
		{
			bIsFollowing = false;
			this->Location = FVector3(0.0f, -1000.0f, 0.0f);
			this->Velocity = FVector3(0.0f, 0.0f, 0.0f);
		}
	}
	
	Location.x += Velocity.x * t;
	Location.y += Velocity.y * t;
}

void Moon::Render(URenderer& renderer)
{
	if (bIsActive)
	{
		Planet::Render(renderer);
	}
}

void Moon::HandleCollision(UPrimitive* other)
{  
	Planet::HandleCollision(other);

	if (!bIsActive || bIsHiddenByCollision)
	{
		return;
	}

	UBall* targetObj = static_cast<UBall*>(other);
	if (!targetObj || targetObj != TargetObject)
	{
		return; //이미 Manager에서 충돌을 막고있지만 안전장치
	}
	FVector3 distance = targetObj->Location - this->Location;
	float dist = distance.Length();
	float radiusSum = targetObj->Radius + this->Radius;
	if (dist <= radiusSum)
	{
		bIsHiddenByCollision = true;
		HideTimer = 0.0f;
		this->Location = FVector3(0.0f, -1000.0f, 0.0f);
		this->Velocity = FVector3(0.0f, 0.0f, 0.0f);
	}
}

// --- GravityPlanet Implementation ---

ID3D11Buffer* GravityPlanet::RangeDashBuffer = nullptr;
UINT GravityPlanet::NumDashVertices = 0;
ID3D11RasterizerState* GravityPlanet::NoCullState = nullptr;
UBall* GravityPlanet::targetPlayer = nullptr;

void GravityPlanet::SetGravitySystem(UBall* player)
{
	targetPlayer = player;
}

void GravityPlanet::Update(float t)
{
	Planet::Update(t);

	if (targetPlayer == nullptr)return;
	if (targetPlayer->bInvincible) return;

	FVector3 direction = this->Location - targetPlayer->Location;
	float dist = direction.Length();
	FVector3 normal = direction / dist;

	if (range + targetPlayer->Radius < dist || !bIsActive)
		return;

	float strength = 1.0f;
	FVector3 newVelocity;

	if (this->planetType == PlanetType::pull)
	{
		strength = 1.0f;
		FVector3 force = normal * (strength / ((dist * dist) + 0.01f));
		newVelocity = targetPlayer->GetVelocity() + force * t;

		if (newVelocity.Length() > maxSpeed_pull)
		{
			newVelocity = newVelocity / newVelocity.Length() * maxSpeed;
		}
	}
	else if(this->planetType == PlanetType::push)
	{
		FVector3 pushDir = normal * -1.0f;
		float surfaceDist = dist - this->Radius;
		if (surfaceDist < 0.0f) surfaceDist = 0.01f;

		float pushStrength = 0.5f / (surfaceDist + 0.1f);
		FVector3 pushForce = pushDir * pushStrength;

		float approachSpeed = targetPlayer->GetVelocity().Dot(-pushDir);
		if (approachSpeed > 0)
		{
			pushForce += pushDir * approachSpeed * 2.0f;
		}

		newVelocity = targetPlayer->GetVelocity() + pushForce * t;

		float limit = 5.0f;
		if (newVelocity.Length() > limit)
			newVelocity = newVelocity / newVelocity.Length() * limit;
	}

	targetPlayer->SetVelocity(newVelocity);
}

void GravityPlanet::InitRangeResources(ID3D11Device* device)
{
	if (RangeDashBuffer) return;

	const int segments = 128;
	std::vector<FVertexSimple> vertices;

	for (int i = 0; i < segments; i+=2)
	{
		float t1 = (float)i / segments * 2.0f * 3.141592f;
		float t2 = (float)(i + 1) / segments * 2.0f * 3.141592f;

		FVertexSimple v1 = {}, v2 = {};

		// 1. Position (x, y, z)
		v1.x = cosf(t1); v1.y = sinf(t1); v1.z = 0.0f;
		v2.x = cosf(t2); v2.y = sinf(t2); v2.z = 0.0f;

		// 2. Color (r, g, b, a) - 노란색 점선
		v1.r = v2.r = 1.0f;
		v1.g = v2.g = 1.0f;
		v1.b = v2.b = 1.0f;
		v1.a = v2.a = 1.0f;

		// 3. UV (u, v) - 셰이더 조건문 통과를 위해 0.0f
		v1.u = v1.v = v2.u = v2.v = 0.0f;

		vertices.push_back(v1);
		vertices.push_back(v2);
	}
	NumDashVertices = (UINT)vertices.size();

	// D3D11 버퍼 생성 로직
	D3D11_BUFFER_DESC bd = {};
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.ByteWidth = sizeof(FVertexSimple) * NumDashVertices;
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	D3D11_SUBRESOURCE_DATA sd = {};
	sd.pSysMem = vertices.data();
	device->CreateBuffer(&bd, &sd, &RangeDashBuffer);

	D3D11_RASTERIZER_DESC rd = {};
	rd.FillMode = D3D11_FILL_SOLID;
	rd.CullMode = D3D11_CULL_NONE;
	device->CreateRasterizerState(&rd, &NoCullState);
}

void GravityPlanet::ReleaseRangeResources()
{
	if (RangeDashBuffer != nullptr)
	{
		RangeDashBuffer->Release();
		RangeDashBuffer = nullptr;
	}
}


void GravityPlanet::Render(URenderer& renderer)
{
	Planet::Render(renderer);

	if (!RangeDashBuffer || !this->bIsActive) return;

	XMFLOAT4 Color;

	if (this->planetType == PlanetType::pull)
		Color = this->Color_pull;
	else
		Color = this->Color_push;

	// --- 점선 그리기 세팅 ---
	renderer.DeviceContext->RSSetState(NoCullState);
	renderer.DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

	// 원하는 수치로 상수 버퍼 업데이트
	FVector3 rangeTransform = { this->Location.x, this->Location.y, range};
	renderer.UpdateConstant(rangeTransform, 0.0f, {1.0f, 1.0f, 1.0f}, Color, {0.0f, 0.0f}, {0.0f, 0.0f}, 0, 0.0f);
	renderer.RenderPrimitive(RangeDashBuffer, NumDashVertices); // 그리기

	// --- 복구 ---
	renderer.DeviceContext->RSSetState(renderer.RasterizerState);
	renderer.DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}


// --- Meteor Implementation ---

Meteor::Meteor(UBall* target, float r, const std::string& textureName)
	: Planet({ 0.f, -1000.f, 0.f }, { 0.f, 0.f, 0.f }, r, textureName), targetPlayer(target)
{
	bIsActive = true;
	HideAndWait();
}

void Meteor::Update(float t)
{
	if (bIsWaiting)
	{
		waitTimer += t;

		if (waitTimer >= waitDuration)
		{
			RespawnAbovePlayer();
		}
		return;
	}

	float rotationSpeed = 1.002f;
	Angle += rotationSpeed * t;

	Location.x += Velocity.x * t; 
	Location.y += Velocity.y * t * 15.0;

	if (targetPlayer && Location.y < targetPlayer->Location.y - 2.5f)
	{
		HideAndWait();
	}
}

void Meteor::HandleCollision(UPrimitive* other)
{
	if (bIsWaiting || !bIsActive) return;

	UBall* player = static_cast<UBall*>(other);
	if (!player) return;
	if (player->bInvincible) return;

	FVector3 distance = player->Location - this->Location;
	float dist = distance.Length();
	float radiusSum = player->Radius + this->Radius;

	if (dist <= radiusSum)
	{
		//planet의 explode 사용 x
		player->inputLockTimer = player->inputLockDuration;
		SoundManager::Get().PlayEffect("Explosion");
		HideAndWait();
	}
}

void Meteor::HideAndWait()
{
	bIsWaiting = true;
	waitTimer = 0.0f;
	waitDuration = 1.5f + ((rand() % 1000) / 1000.0f) * 3.0f;
	// 화면 밖 보이지 않는 곳으로 순간이동
	Location = FVector3(0.0f, -1000.0f, 0.0f);
	Velocity = FVector3(0.0f, 0.0f, 0.0f);
}

void Meteor::RespawnAbovePlayer()
{
	bIsWaiting = false;
	if (!targetPlayer || targetPlayer->Location.y >= 90.f) return;

	float spawnHeight = targetPlayer->Location.y + 3.0f + ((rand() % 1000) / 1000.0f) * 2.0f;
	float randomXOffset = (((rand() % 1000) / 1000.0f) - 0.5f) * 1.0f;
	float spawnX = targetPlayer->Location.x + randomXOffset;
	if(-1.f >= spawnX) spawnX = -1.0f + this->Radius;
	if (1.f <= spawnX) spawnX = 1.0f - this->Radius;;

	Location = FVector3(spawnX, spawnHeight, 0.0f);

	float minSpeed = 0.01f;
	float maxSpeed = 0.03f;
	float baseRadius = 0.05f;

	float randomSpeed = minSpeed + ((rand() % 1000) / 1000.0f) * (maxSpeed - minSpeed);
	float sizeMultiplier = baseRadius / sqrtf(this->Radius + 0.001f);
	randomSpeed *= sizeMultiplier;

	Velocity.x = (((rand() % 100) / 100.0f) - 0.5f) * 0.02f;
	Velocity.y = -(randomSpeed * 2.0f);
	Velocity.z = 0.0f;
}