#include "moon.h"
#include "UBall.h"
#include <cmath>

//부모 생성자 호출
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
	// 비활성 상태일 때는 리스폰 로직만 실행
	if (!bIsActive)
	{
		RespawnTimer += t;
		if (RespawnTimer >= RespawnDelay) Respawn();
		return;
	}

	if (TargetObject != nullptr)
	{
		// 타겟과의 방향 벡터 계산
		FVector3 direction = TargetObject->Location - this->Location;
		float distance = direction.Length();

		// 일정 거리 이상 떨어져 있을 때만 따라감
		if (distance > FollowDistance)
		{
			direction = direction / distance;
			
			// 거리에 비례한 힘 계산 (가속도)
			float forceMagnitude = (distance - FollowDistance) * FollowSpeed;
			
			// 속도에 힘 추가 (관성 적용)
			this->Velocity.x += direction.x * forceMagnitude * t;
			this->Velocity.y += direction.y * forceMagnitude * t;
			
			// 최대 속도 제한
			float currentSpeed = this->Velocity.Length();
			if (currentSpeed > MaxFollowSpeed)
			{
				this->Velocity.x = (this->Velocity.x / currentSpeed) * MaxFollowSpeed;
				this->Velocity.y = (this->Velocity.y / currentSpeed) * MaxFollowSpeed;
			}
		}
		
		// === 텍스처 회전 로직 (거리와 무관하게 항상 실행) ===
		float currentSpeed = this->Velocity.Length();
		
		// 현재 속도가 충분히 있을 때만 회전
		if (currentSpeed > 0.0001f && distance > 0.0001f)
		{
			direction = direction / distance;
			
			// 현재 이동 방향 (정규화된 속도 벡터)
			FVector3 currentDirection = this->Velocity / currentSpeed;
			
			// 2D 외적: currentDir × targetDir
			// 양수면 목표가 왼쪽(반시계 회전), 음수면 오른쪽(시계 회전)
			float cross = currentDirection.x * direction.y - currentDirection.y * direction.x;
			
			// 회전 방향 결정
			float rotationDirection = (cross > 0.0f) ? 1.0f : -1.0f;
			
			// 텍스처 회전 각도 업데이트 (속도에 비례)
			this->Angle += rotationDirection * RotationSpeed * 10.0f;
		}
		
		// 항상 속도 감쇠 적용 (드리프트 효과)
		this->Velocity.x *= 0.98f;
		this->Velocity.y *= 0.98f;
	}
	
	// 위치 업데이트
	Location.x += Velocity.x * t;
	Location.y += Velocity.y * t;
}
