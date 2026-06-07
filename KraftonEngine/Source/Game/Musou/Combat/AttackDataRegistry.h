#pragma once

#include "Core/Types/CoreTypes.h"
#include "Game/Musou/Combat/AttackTypes.h"

// ============================================================
// FAttackDataRegistry — 플레이어 공격 데이터 보관소
//
// Content/Script/Data/attack_data.lua (순수 데이터 테이블) 를 로드해서
// 판정 스펙 / 공격 스텝 / 체인 구성을 C++ 측에 제공한다.
//
//   - 로직(콤보 상태머신/컨텍스트 판정/이동 잠금)은 전부 C++ 잔류 —
//     lua 는 UE 의 DataTable 포지션 (데이터만).
//   - 핫리로드: EnsureFresh() 가 파일 mtime 을 보고 변경 시 재파싱 +
//     Version 증가. 공격 입력 시점(저빈도) 에 호출 전제. 버전 변화는
//     MusouCharacter 가 주입 notify 재주입 판정에 사용한다.
//   - 안전망: 파싱 실패 시 마지막 정상 데이터 유지, 파일 부재/최초 실패 시
//     컴파일 내장 기본 테이블 사용. 게임이 데이터 오류로 죽지 않는다.
// ============================================================

class UAnimSequence;

// 공격 스텝 정의 — attack_data.lua 의 steps 항목 1개.
// 몽타주 경로가 있으면 에디터 저작본 우선, 로드 실패 시 SequencePath 의 시퀀스로
// 런타임 몽타주를 생성하고 아래 notify 파라미터를 주입한다 (MusouCharacter 담당).
struct FMusouAttackStep
{
	FString MontagePath;            // 비어 있으면 fallback 전용 스텝
	FString SequencePath;           // 비어 있으면 fallback 없음 (몽타주 필수)
	float   BlendIn = 0.1f;

	// 재생속도 — [Min, Max] 균등 랜덤 (같으면 고정). lua: play_rate = 1.1 또는 { 0.95, 1.15 }
	// notify/RM 은 시퀀스 시간축 기준이라 속도를 바꿔도 히트 타이밍 비율은 유지된다.
	float   PlayRateMin = 1.0f;
	float   PlayRateMax = 1.0f;

	// ── notify 주입 파라미터 (시퀀스에 저작 notify 가 없을 때만 사용) ──
	FString AttackId;               // specs 키. 비어 있으면 히트 notify 안 박음
	float   HitFrac = -1.0f;        // MusouAttack 위치 (PlayLength 비율). <0 = 없음
	float   WindowBeginFrac = -1.0f;// ComboWindow 시작 비율. <0 = 윈도우 없음 (체인 말단/단발)
	float   WindowEndFrac = -1.0f;

	bool IsValid() const { return !MontagePath.empty() || !SequencePath.empty(); }
};

// 체인의 한 칸 — 동일 역할(같은 단수/입력)의 변주 후보 묶음.
// lua chains 의 스텝 id 1개(string)/배열 표기를 모두 슬롯으로 정규화한다.
// 어느 변주를 재생할지는 호출측(MusouCharacter::PickVariant)이 결정 — 직전 변주 반복 회피.
struct FMusouAttackSlot
{
	TArray<FMusouAttackStep> Variants;

	bool IsValid() const { return !Variants.empty(); }
};

class FAttackDataRegistry
{
public:
	static FAttackDataRegistry& Get();

	// 파일 mtime 검사 후 변경 시 재로드 — 공격 입력 시점 등 저빈도 호출 전제.
	// 콤보 진행 중엔 호출하지 말 것 (체인 길이가 도중에 바뀌면 단수 판정이 흔들린다).
	void EnsureFresh();

	// 데이터 세대 — 재로드마다 증가. 주입 notify 의 갱신 필요 판정용.
	int32 GetVersion() const { return Version; }

	const FAttackSpec* FindSpec(const FName& Id) const;

	// 좌클릭 콤보 체인 (슬롯 = 단수, 슬롯 안 = 변주 후보) — 비어 있을 수 있음 (호출측이 empty 가드).
	const TArray<FMusouAttackSlot>& GetLightChain(EAttackContext Context) const;

	// 우클릭 강공격 — 미정의 컨텍스트면 nullptr.
	const FMusouAttackSlot* GetHeavySlot(EAttackContext Context) const;

	// 콤보 분기 피니셔 — 인덱스 = 분기 시점 단수 (1 기반). 단수 초과 시 마지막으로 clamp,
	// 테이블이 비었으면 nullptr.
	const FMusouAttackSlot* GetBranchFinisher(int32 ComboStep) const;

private:
	FAttackDataRegistry() = default;

	bool LoadFromLua();   // 파싱 성공 시 멤버 교체 + Version 증가
	void LoadDefaults();  // 컴파일 내장 기본 데이터 (lua 부재/최초 실패 시)

	static int32 ContextIndex(EAttackContext Context) { return static_cast<int32>(Context); }

	TArray<FAttackSpec>      Specs;
	TArray<FMusouAttackSlot> LightChains[3];   // EAttackContext 인덱스
	FMusouAttackSlot         HeavySlots[3];    // IsValid() == false 면 미정의
	TArray<FMusouAttackSlot> BranchFinishers;

	int32 Version = 0;
	bool  bLoadedOnce = false;
	long long LastWriteStamp = 0;   // 파일 mtime (epoch tick) — 변경 감지용
};
