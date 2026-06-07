#pragma once

#include "Core/Types/CoreTypes.h"
#include "Math/Vector.h"
#include "Object/FName.h"

class APawn;

// 공격 진입 컨텍스트 — 같은 입력이라도 시작 상태에 따라 다른 체인/몽타주로 진입.
// (MusouCharacter 가 콤보 시작 시 1회 판정, FAttackDataRegistry 의 체인 키)
enum class EAttackContext : uint8
{
	Idle,      // 지상 + 거의 정지
	Moving,    // 지상 + 이동 중 (속도 ≥ MovingAttackSpeedThreshold)
	Airborne,  // 점프/낙하 중
};

// ============================================================
// FAttackSpec — 공격 1종의 판정/연출 속성
//
// 정의/튜닝은 Content/Script/Data/attack_data.lua 의 specs 테이블에서
// (저장 시 핫리로드). FAttackDataRegistry 가 로드해서 보관한다.
// 신규 공격(콤보 단계/강공격/스킬)은 lua 에 spec + step 행 추가 + 몽타주에
// AnimNotify_MusouAttack(AttackId) 배치(또는 hit_frac 자동 주입)만 하면 된다.
// ============================================================
struct FAttackSpec
{
	FName Id;
	float Range = 4.0f;          // 판정 반경
	float Height = 2.5f;         // 수직 허용 거리 (위층/공중 대상 제외)
	float ConeCos = -1.0f;       // 전방 콘 cos 임계 (-1 = 360도 전방위)
	float DamageMult = 1.0f;     // 데미지 = 공격자 AttackPower × 배율
	float KnockbackDist = 2.5f;
	float KnockbackDur = 0.15f;
	float ShakeScale = 0.0f;     // 히트 시 카메라 셰이크 강도 (0 = 없음). lua: shake
};

// Id 로 spec 조회 — FAttackDataRegistry 위임 (AttackDataRegistry.cpp 정의). 없으면 nullptr.
const FAttackSpec* FindMusouAttackSpec(const FName& Id);

// ============================================================
// FMusouAttackEvent — 공격 발동 1회의 스냅샷
//
// AnimNotify_MusouAttack이 발동 시점의 기하(Origin/Forward)를 캡처해서
// AMusouGameMode::OnAttackPerformed로 브로드캐스트한다.
// 수신자(군체 Manager / 보스 BattleComponent)는 각자 자기 대상에 대해
// FMusouAttackEvent::IsInVolume으로 판정 후 데미지/넉백을 적용한다.
// ============================================================
struct FMusouAttackEvent
{
	APawn* Attacker = nullptr;   // 공격자 (군체 잡졸이 공격자면 null 가능)
	FAttackSpec Spec;            // 발동 시점 스펙 스냅샷
	FVector Origin;              // 발동 시점 공격자 위치
	FVector Forward;             // 발동 시점 공격자 전방
	float Damage = 0.0f;         // 최종 데미지 (AttackPower × DamageMult 적용 완료)
	bool bFromPlayer = false;    // 진영 필터 — 수신자가 자기 진영 공격은 무시

	// 공용 판정 기하 — 거리²(sqrt 없음) + 높이 + 콘 각도.
	// 군체 Manager는 SoA 위치 배열을 돌며, 보스는 자기 위치 하나로 호출.
	bool IsInVolume(const FVector& TargetPos) const
	{
		const FVector ToTarget = TargetPos - Origin;

		const float DistSq = ToTarget.Dot(ToTarget);
		const float RangeSq = Spec.Range * Spec.Range;
		if (DistSq > RangeSq || DistSq < 0.0001f)
		{
			return false;
		}

		if (ToTarget.Z > Spec.Height || ToTarget.Z < -Spec.Height)
		{
			return false;
		}

		// 360도 공격이면 각도 검사 생략
		if (Spec.ConeCos <= -1.0f)
		{
			return true;
		}

		const FVector Dir = ToTarget.Normalized();
		return Forward.Dot(Dir) >= Spec.ConeCos;
	}
};
