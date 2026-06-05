#pragma once

#include "Core/Types/CoreTypes.h"
#include "Math/Vector.h"
#include "Object/FName.h"

class APawn;

// ============================================================
// FAttackSpec — 공격 1종의 판정/연출 속성 (lua ATTACKS 테이블의 C++ 이관)
//
// 튜닝은 FindMusouAttackSpec의 정적 테이블에서.
// 신규 공격(콤보 단계/강공격/스킬)은 테이블에 행 추가 + 몽타주에
// AnimNotify_MusouAttack(AttackId) 배치만 하면 된다.
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
};

inline const FAttackSpec* FindMusouAttackSpec(const FName& Id)
{
	static const FAttackSpec Table[] =
	{
		//  Id                  Range  Height ConeCos DmgMult KbDist KbDur
		{ FName("attack1"),     4.0f,  2.5f,  -1.0f,  1.0f,   2.5f,  0.15f }, // 360 High — 전방위
		{ FName("attack2"),     3.5f,  2.5f,   0.34f, 1.5f,   4.0f,  0.20f }, // Backhand — 전방 콘 (~140도)

		// 좌클릭 콤보 체인 (Combo Attack Ver. 1/2/3) — 단계가 오를수록 강해진다
		{ FName("combo1"),      3.5f,  2.5f,   0.34f, 1.0f,   1.5f,  0.10f }, // 1단 — 전방 콘, 짧은 넉백
		{ FName("combo2"),      3.5f,  2.5f,   0.34f, 1.2f,   2.0f,  0.12f }, // 2단
		{ FName("combo3"),      4.5f,  2.5f,  -1.0f,  2.0f,   5.0f,  0.25f }, // 3단 피니셔 — 전방위 + 강넉백
	};

	for (const FAttackSpec& Spec : Table)
	{
		if (Spec.Id == Id)
		{
			return &Spec;
		}
	}
	return nullptr;
}

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
