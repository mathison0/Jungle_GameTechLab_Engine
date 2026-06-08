#pragma once

#include "Core/Types/CoreTypes.h"
#include "Game/Musou/Combat/AttackTypes.h"
#include "Math/Vector.h"
#include "Math/Rotator.h"

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

// 몽타주 카메라 연출 샷 1개 — steps.<id>.camera 배열 항목.
// 구간(frac) 동안 메인 카메라 대신 캐릭터에 붙은 연출 카메라로 블렌드.
// 소비처: MusouCharacter 가 AnimNotifyState_CameraShot 으로 주입/구동.
struct FMusouCameraShot
{
	float BeginFrac = -1.0f;   // 샷 구간 (PlayLength 비율). lua: { 0.3, 0.7 } 또는 begin/end
	float EndFrac   = -1.0f;
	float BlendIn   = 0.15f;   // 연출 카메라 진입 블렌드 (초)
	float BlendOut  = 0.25f;   // 메인 카메라 복귀 블렌드 (초)

	FVector  Offset   = FVector(2.5f, 2.0f, 1.0f);  // 캡슐 로컬 (X=전방, Y=우측, Z=상단, m)
	FRotator Rotation = FRotator::ZeroRotator;       // 로컬 회전 — look_at=false 일 때만

	float FOVRad = 0.0f;        // 0 = 메인 카메라 FOV 유지. lua 'fov' 는 도(deg) 단위
	bool  bLookAt = true;       // 매 프레임 캐릭터를 바라봄 (Rotation 무시)
	float LookAtHeight = 0.5f;  // 시선 목표 — 액터 위치 기준 +Z (m)
	float LookAhead = 0.0f;     // >0 = 캐릭터 대신 전방 LookAhead m 지점을 바라봄 (검기 사선 프레이밍)
	bool  bFollow = true;       // false = 샷 시작 위치에 월드 고정
	float Letterbox = 0.0f;     // 레터박스 두께 비율 (0 = 없음)

	// offset/위치를 어느 yaw 기준으로 배치할지. true(기본) = 카메라 뷰(ControlRotation) 기준
	//  → 캐릭터가 어딜 보든 화면 기준 일관 + 메인캠과 가까워 블렌드 튐 감소.
	// false = 캐릭터 facing 기준 (도약/돌진 방향 추적샷 등). lua: anchor = "camera"/"character"
	bool  bCameraRelative = true;

	bool IsValid() const { return BeginFrac >= 0.0f && EndFrac > BeginFrac; }
};

// 전방 진행 충격파 — 궁극기 지면 강타용. 한 점에서 즉발 판정이 아니라 시작점에서
// 전방으로 Distance 만큼 Duration 동안 Pulses 개의 판정/검기를 순차 발사 → "길게 순차 데미지".
// 데미지 판정 기하는 AttackId 의 spec(range/cone/height/dmg)을 펄스마다 origin 만 전진시켜 사용.
struct FMusouShockwave
{
	float   TriggerFrac = -1.0f;   // 몽타주 발동 시점 (지면 강타 프레임). <0 = 없음
	float   Distance = 12.0f;      // 전방 진행 총 거리 (m)
	float   Duration = 0.7f;       // 진행 시간 (초)
	int32   Pulses = 8;            // 데미지/검기 펄스 수
	FString AttackId;              // 펄스 판정 spec 키 (비면 슬램 spec 사용)
	float   SlashSpeed = 9.0f;     // 검기(placeholder) 진행 속도
	float   SlashLife = 0.45f;     // 검기 수명 (초)
	float   SlashYaw = 90.0f;      // 검기 메시 시각 회전 보정 (진행 yaw + 이 값). 기존 검기 규칙=90

	bool IsValid() const { return TriggerFrac >= 0.0f && Pulses > 0 && Distance > 0.0f; }
};

// 궁극기 백플립 도약 — 제자리 백플립을 후방+상방 임펄스로 실제로 빼준다.
struct FMusouLeap
{
	float TriggerFrac = -1.0f;  // 도약 프레임 (PlayLength 비율). <0 = 없음
	float Back = 6.0f;          // 후방 수평 속도 (m/s)
	float Up = 4.0f;            // 상방 속도 (m/s)
	float Gravity = 0.3f;       // 도약 후 중력 배율 (체공 연장). 궁극기 종료 시 1.0 복원

	bool IsValid() const { return TriggerFrac >= 0.0f; }
};

// 궁극기 다음 슬롯 조기 전환 — 현재 슬롯 종료 전에 cross-fade (백플립→강타 블렌드).
struct FMusouAdvance
{
	float TriggerFrac = -1.0f;  // 전환 프레임 (PlayLength 비율). <0 = 없음 (몽타주 끝에 폴백)

	bool IsValid() const { return TriggerFrac >= 0.0f; }
};

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

	// 시퀀스의 bEnableRootMotion 을 로드 시 강제 — 임포트 직후 RM 플래그가 꺼진 에셋
	// (구르기 등) 을 에디터 재저장 없이 쓰기 위한 런타임 override. lua: force_root_motion
	bool    bForceRootMotion = false;

	// 궁극기 강타 제자리 고정 — 이 스텝이 무쌍기 중 재생되면 속도 0 + 중력 0 으로 공중에
	// 못 박는다 (백핸드가 뒤로 밀리지 않게). lua: plant_in_air = true. EndUltimate 에서 중력 복원.
	bool    bPlantInAir = false;

	// ── notify 주입 파라미터 (시퀀스에 저작 notify 가 없을 때만 사용) ──
	FString AttackId;               // specs 키. 비어 있으면 히트 notify 안 박음
	float   HitFrac = -1.0f;        // MusouAttack 위치 (PlayLength 비율). <0 = 없음
	float   WindowBeginFrac = -1.0f;// ComboWindow 시작 비율. <0 = 윈도우 없음 (체인 말단/단발)
	float   WindowEndFrac = -1.0f;

	// 몽타주 카메라 연출 — 비어 있으면 연출 없음 (대부분의 스텝). lua: camera = { {...}, ... }
	TArray<FMusouCameraShot> CameraShots;

	// 전방 진행 충격파 — 궁극기 지면 강타용. lua: shockwave = { trigger_frac=.., ... }
	FMusouShockwave Shockwave;

	// 궁극기 백플립 도약 — lua: leap = { trigger_frac=.., back=.., up=.. }
	FMusouLeap Leap;

	// 궁극기 다음 슬롯 조기 전환 — lua: advance = { trigger_frac=.. }
	FMusouAdvance Advance;

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

// 전투 피드백/연출 파라미터 — attack_data.lua 의 feedback 테이블 (없으면 아래 기본값).
// 소비처: AMusouGameMode (킬 버스트), AMusouCharacter (공중 콤보 행 타임).
struct FMusouFeedbackParams
{
	int32 KillBurstMinKills   = 5;      // 스윙 1회 판정으로 이 수 이상 처치 시 발동
	float KillBurstSlomoDur   = 0.25f;  // 슬로모 지속 (실시간 초)
	float KillBurstSlomoRate  = 0.25f;  // 슬로모 타임스케일 (0..1)
	float KillBurstShakeScale = 0.4f;   // 버스트 카메라 셰이크 강도

	// 공중 콤보 행 타임 — 공중 체인 진행 중 플레이어 중력 배율 (1 = 변화 없음)
	float AirComboGravityScale = 0.25f;

	// 무쌍 게이지 — 이 히트 수(적중 누적)를 채우면 무쌍기(R) 발동 가능
	int32 UltimateHitsToFill = 80;

	// 플레이어 피격 리액션 최소 간격 (초) — 군체 다단 히트로 스턴락되지 않게
	float HitReactCooldown = 1.2f;

	// 발도/납도 — 모션 진행 중 무기 본 스왑(손↔등) 시점 (재생 길이 비율, 손이 등에 닿는 즈음)
	float WeaponSwapFrac = 0.45f;
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
	// AirborneJuggle 체인이 미정의면 Airborne 체인으로 폴백.
	const TArray<FMusouAttackSlot>& GetLightChain(EAttackContext Context) const;

	// 우클릭 강공격 — 미정의 컨텍스트면 nullptr (AirborneJuggle 은 Airborne 폴백).
	const FMusouAttackSlot* GetHeavySlot(EAttackContext Context) const;

	// 콤보 분기 피니셔 — 인덱스 = 분기 시점 단수 (1 기반). 단수 초과 시 마지막으로 clamp,
	// 테이블이 비었으면 nullptr.
	const FMusouAttackSlot* GetBranchFinisher(int32 ComboStep) const;

	// 무쌍기 난무 체인 — 순차 자동 재생 (몽타주 끝나면 다음 슬롯). 비어 있으면 발동 불가.
	const TArray<FMusouAttackSlot>& GetUltimateChain() const { return UltimateChain; }

	// 구르기 (회피) — 미정의면 nullptr (기능 비활성).
	const FMusouAttackSlot* GetDodgeSlot() const { return DodgeSlot.IsValid() ? &DodgeSlot : nullptr; }

	// 플레이어 피격 리액션 — 변주 슬롯 (좌/우 휘청 등). 미정의면 nullptr (기능 비활성).
	const FMusouAttackSlot* GetHitReactSlot() const { return HitReactSlot.IsValid() ? &HitReactSlot : nullptr; }

	// 플레이어 사망 연출 — 미정의면 nullptr (연출 없이 즉시 사망 처리).
	const FMusouAttackSlot* GetDeathSlot() const { return DeathSlot.IsValid() ? &DeathSlot : nullptr; }

	// 발도/납도 모션 — 미정의면 nullptr (호출측이 즉시 스왑으로 폴백).
	const FMusouAttackSlot* GetWeaponDrawSlot() const { return WeaponDrawSlot.IsValid() ? &WeaponDrawSlot : nullptr; }
	const FMusouAttackSlot* GetWeaponSheatheSlot() const { return WeaponSheatheSlot.IsValid() ? &WeaponSheatheSlot : nullptr; }

	// 전투 피드백 연출 파라미터 (킬 버스트 슬로모/셰이크 등).
	const FMusouFeedbackParams& GetFeedback() const { return Feedback; }

private:
	FAttackDataRegistry() = default;

	bool LoadFromLua();   // 파싱 성공 시 멤버 교체 + Version 증가
	void LoadDefaults();  // 컴파일 내장 기본 데이터 (lua 부재/최초 실패 시)

	static int32 ContextIndex(EAttackContext Context) { return static_cast<int32>(Context); }
	static constexpr int32 NumContexts = static_cast<int32>(EAttackContext::Count);

	TArray<FAttackSpec>      Specs;
	TArray<FMusouAttackSlot> LightChains[NumContexts];   // EAttackContext 인덱스
	FMusouAttackSlot         HeavySlots[NumContexts];    // IsValid() == false 면 미정의
	TArray<FMusouAttackSlot> BranchFinishers;
	TArray<FMusouAttackSlot> UltimateChain;
	FMusouAttackSlot         DodgeSlot;
	FMusouAttackSlot         HitReactSlot;
	FMusouAttackSlot         DeathSlot;
	FMusouAttackSlot         WeaponDrawSlot;
	FMusouAttackSlot         WeaponSheatheSlot;
	FMusouFeedbackParams     Feedback;

	int32 Version = 0;
	bool  bLoadedOnce = false;
	long long LastWriteStamp = 0;   // 파일 mtime (epoch tick) — 변경 감지용
};
