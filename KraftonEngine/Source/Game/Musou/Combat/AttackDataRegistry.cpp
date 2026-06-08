#include "Game/Musou/Combat/AttackDataRegistry.h"

#include "Core/Logging/Log.h"
#include "Lua/LuaScriptManager.h"
#include "Platform/Paths.h"

#include <cmath>
#include <filesystem>

namespace
{
	// ScriptDir(Content/Script) 기준 상대 경로 — FLuaScriptManager 의 파일 API 들과 동일 규약.
	constexpr const char* AttackDataFile = "Data/attack_data.lua";

	// cone_deg(전체 각, 사람 단위) → ConeCos(전방 콘 cos 임계). 360 이상 = 전방위(-1).
	float ConeDegToCos(float ConeDeg)
	{
		if (ConeDeg >= 360.0f)
		{
			return -1.0f;
		}
		return std::cos(ConeDeg * 0.5f * 3.14159265f / 180.0f);
	}

	FAttackSpec ParseSpec(const FString& Id, const sol::table& T)
	{
		FAttackSpec S;
		S.Id            = FName(Id);
		S.Range         = T.get_or("range",    4.0f);
		S.Height        = T.get_or("height",   2.5f);
		S.ConeCos       = ConeDegToCos(T.get_or("cone_deg", 360.0f));
		S.DamageMult    = T.get_or("dmg",      1.0f);
		S.KnockbackDist = T.get_or("kb",       2.5f);
		S.KnockbackDur  = T.get_or("kb_dur",   0.15f);
		S.ShakeScale    = T.get_or("shake",    0.0f);
		S.LaunchZ       = T.get_or("launch",   0.0f);
		S.SelfLaunchZ   = T.get_or("self_launch", 0.0f);
		return S;
	}

	FMusouAttackStep ParseStep(const sol::table& T)
	{
		FMusouAttackStep Step;
		Step.MontagePath  = T.get_or("montage",   std::string());
		Step.SequencePath = T.get_or("sequence",  std::string());
		Step.BlendIn      = T.get_or("blend_in",  0.1f);
		Step.AttackId     = T.get_or("attack_id", std::string());
		Step.HitFrac      = T.get_or("hit_frac", -1.0f);

		Step.bForceRootMotion = T.get_or("force_root_motion", false);
		Step.bPlantInAir      = T.get_or("plant_in_air", false);

		// play_rate — 숫자(고정) 또는 { min, max } (균등 랜덤)
		sol::object RateObj = T["play_rate"];
		if (RateObj.is<float>())
		{
			Step.PlayRateMin = Step.PlayRateMax = RateObj.as<float>();
		}
		else if (RateObj.is<sol::table>())
		{
			sol::table Rate = RateObj.as<sol::table>();
			Step.PlayRateMin = Rate.get_or(1, 1.0f);
			Step.PlayRateMax = Rate.get_or(2, Step.PlayRateMin);
		}

		sol::optional<sol::table> Window = T["window"];
		if (Window)
		{
			Step.WindowBeginFrac = Window->get_or(1, -1.0f);
			Step.WindowEndFrac   = Window->get_or(2, -1.0f);
		}

		// camera — 몽타주 카메라 연출 샷 배열 (선택). 항목 스키마는 attack_data.lua 주석 참고.
		if (sol::optional<sol::table> CameraT = T["camera"])
		{
			const int32 NumShots = static_cast<int32>(CameraT->size());
			for (int32 i = 1; i <= NumShots; ++i)
			{
				sol::optional<sol::table> ShotT = (*CameraT)[i];
				if (!ShotT)
				{
					continue;
				}

				FMusouCameraShot Shot;
				Shot.BeginFrac = ShotT->get_or("begin_frac", -1.0f);
				Shot.EndFrac   = ShotT->get_or("end_frac",   -1.0f);
				Shot.BlendIn   = ShotT->get_or("blend_in",   Shot.BlendIn);
				Shot.BlendOut  = ShotT->get_or("blend_out",  Shot.BlendOut);

				if (sol::optional<sol::table> Off = (*ShotT)["offset"])
				{
					Shot.Offset.X = Off->get_or(1, Shot.Offset.X);
					Shot.Offset.Y = Off->get_or(2, Shot.Offset.Y);
					Shot.Offset.Z = Off->get_or(3, Shot.Offset.Z);
				}
				if (sol::optional<sol::table> Rot = (*ShotT)["rotation"])
				{
					Shot.Rotation.Pitch = Rot->get_or(1, 0.0f);
					Shot.Rotation.Yaw   = Rot->get_or(2, 0.0f);
					Shot.Rotation.Roll  = Rot->get_or(3, 0.0f);
				}

				// fov 는 사람 단위(deg) — 엔진 카메라는 radians 저장.
				const float FovDeg = ShotT->get_or("fov", 0.0f);
				Shot.FOVRad = FovDeg > 0.0f ? FovDeg * 3.14159265f / 180.0f : 0.0f;

				Shot.bLookAt      = ShotT->get_or("look_at", true);
				Shot.LookAtHeight = ShotT->get_or("look_height", Shot.LookAtHeight);
				Shot.LookAhead    = ShotT->get_or("look_ahead", 0.0f);
				Shot.bFollow      = ShotT->get_or("follow", true);
				Shot.Letterbox    = ShotT->get_or("letterbox", 0.0f);

				// anchor — offset 기준 yaw. "character" 면 캐릭터 facing, 그 외(기본)는 카메라 뷰.
				const std::string Anchor = ShotT->get_or("anchor", std::string("camera"));
				Shot.bCameraRelative = (Anchor != "character");

				if (Shot.IsValid())
				{
					Step.CameraShots.push_back(Shot);
				}
				else
				{
					UE_LOG("[AttackData] camera 샷 #%d 구간 무효 (begin_frac/end_frac 필수) — 스킵", i);
				}
			}
		}

		// shockwave — 전방 진행 충격파 (궁극기 지면 강타). 단일 테이블.
		if (sol::optional<sol::table> SwT = T["shockwave"])
		{
			FMusouShockwave& Sw = Step.Shockwave;
			Sw.TriggerFrac = SwT->get_or("trigger_frac", -1.0f);
			Sw.Distance    = SwT->get_or("distance",  Sw.Distance);
			Sw.Duration    = SwT->get_or("duration",  Sw.Duration);
			Sw.Pulses      = SwT->get_or("pulses",    Sw.Pulses);
			Sw.AttackId    = SwT->get_or("attack_id", Step.AttackId);   // 비면 스텝 attack_id 사용
			Sw.SlashSpeed  = SwT->get_or("slash_speed", Sw.SlashSpeed);
			Sw.SlashLife   = SwT->get_or("slash_life",  Sw.SlashLife);
			Sw.SlashYaw    = SwT->get_or("slash_yaw",   Sw.SlashYaw);
			if (!Sw.IsValid())
			{
				UE_LOG("[AttackData] shockwave 무효 (trigger_frac/pulses/distance 확인) — 스킵");
				Step.Shockwave = FMusouShockwave();
			}
		}

		// leap — 궁극기 백플립 도약 (후방+상방 임펄스). 단일 테이블.
		if (sol::optional<sol::table> LpT = T["leap"])
		{
			FMusouLeap& Lp = Step.Leap;
			Lp.TriggerFrac = LpT->get_or("trigger_frac", -1.0f);
			Lp.Back        = LpT->get_or("back", Lp.Back);
			Lp.Up          = LpT->get_or("up",   Lp.Up);
			Lp.Gravity     = LpT->get_or("gravity", Lp.Gravity);
			if (!Lp.IsValid())
			{
				UE_LOG("[AttackData] leap 무효 (trigger_frac 확인) — 스킵");
				Step.Leap = FMusouLeap();
			}
		}

		// advance — 궁극기 다음 슬롯 조기 전환. 단일 테이블.
		if (sol::optional<sol::table> AdT = T["advance"])
		{
			FMusouAdvance& Ad = Step.Advance;
			Ad.TriggerFrac = AdT->get_or("trigger_frac", -1.0f);
			if (!Ad.IsValid())
			{
				UE_LOG("[AttackData] advance 무효 (trigger_frac 확인) — 스킵");
				Step.Advance = FMusouAdvance();
			}
		}

		return Step;
	}
}

FAttackDataRegistry& FAttackDataRegistry::Get()
{
	static FAttackDataRegistry Instance;
	// 첫 접근(예: 데이터 로드 전 notify 발화) 에도 항상 유효한 데이터 보장.
	if (!Instance.bLoadedOnce)
	{
		Instance.EnsureFresh();
	}
	return Instance;
}

void FAttackDataRegistry::EnsureFresh()
{
	const std::wstring WidePath = FPaths::Combine(FPaths::ScriptDir(), FPaths::ToWide(AttackDataFile));

	std::error_code EC;
	const auto WriteTime = std::filesystem::last_write_time(WidePath, EC);
	const long long Stamp = EC ? 0 : static_cast<long long>(WriteTime.time_since_epoch().count());

	if (bLoadedOnce && Stamp == LastWriteStamp)
	{
		return;   // 변경 없음 — 대부분의 호출이 여기서 끝난다 (stat 1회)
	}

	// 파일 부재/접근 실패 — 최초라면 내장 기본값, 이후라면 기존 데이터 유지.
	if (EC)
	{
		if (!bLoadedOnce)
		{
			UE_LOG("[AttackData] '%s' 없음 — 내장 기본 테이블 사용", AttackDataFile);
			LoadDefaults();
			bLoadedOnce = true;
		}
		LastWriteStamp = Stamp;
		return;
	}

	// 재파싱. 실패 시 기존(또는 기본) 데이터 유지 — stamp 는 갱신해서 다음 저장까지 재시도 안 함.
	if (LoadFromLua())
	{
		UE_LOG("[AttackData] 로드 완료 (v%d): specs %d, light %d/%d/%d, branch %d",
			Version, static_cast<int32>(Specs.size()),
			static_cast<int32>(LightChains[0].size()), static_cast<int32>(LightChains[1].size()),
			static_cast<int32>(LightChains[2].size()), static_cast<int32>(BranchFinishers.size()));
	}
	else if (!bLoadedOnce)
	{
		UE_LOG("[AttackData] 최초 로드 실패 — 내장 기본 테이블 사용");
		LoadDefaults();
	}

	bLoadedOnce = true;
	LastWriteStamp = Stamp;
}

bool FAttackDataRegistry::LoadFromLua()
{
	FString Content;
	if (!FLuaScriptManager::ReadScriptFileContent(AttackDataFile, Content))
	{
		UE_LOG("[AttackData] '%s' 읽기 실패", AttackDataFile);
		return false;
	}

	sol::state& Lua = FLuaScriptManager::GetState();
	sol::load_result Chunk = Lua.load(Content, AttackDataFile);
	if (!Chunk.valid())
	{
		sol::error Err = Chunk;
		UE_LOG("[AttackData] 문법 오류 — 기존 데이터 유지: %s", Err.what());
		return false;
	}

	sol::protected_function_result Result = Chunk();
	if (!Result.valid())
	{
		sol::error Err = Result;
		UE_LOG("[AttackData] 실행 오류 — 기존 데이터 유지: %s", Err.what());
		return false;
	}

	sol::object RootObj = Result;
	if (RootObj.get_type() != sol::type::table)
	{
		UE_LOG("[AttackData] 루트가 테이블이 아님 (return { ... } 형태여야 함) — 기존 데이터 유지");
		return false;
	}
	sol::table Root = RootObj.as<sol::table>();

	// ── specs ──
	TArray<FAttackSpec> NewSpecs;
	if (sol::optional<sol::table> SpecsT = Root["specs"])
	{
		for (const auto& KV : *SpecsT)
		{
			if (KV.first.get_type() != sol::type::string || KV.second.get_type() != sol::type::table)
			{
				continue;
			}
			NewSpecs.push_back(ParseSpec(KV.first.as<std::string>(), KV.second.as<sol::table>()));
		}
	}

	// ── steps (id → 정의) ──
	TArray<std::pair<FString, FMusouAttackStep>> StepDefs;
	if (sol::optional<sol::table> StepsT = Root["steps"])
	{
		for (const auto& KV : *StepsT)
		{
			if (KV.first.get_type() != sol::type::string || KV.second.get_type() != sol::type::table)
			{
				continue;
			}
			StepDefs.push_back({ KV.first.as<std::string>(), ParseStep(KV.second.as<sol::table>()) });
		}
	}

	auto FindStep = [&StepDefs](const FString& Id) -> const FMusouAttackStep*
	{
		for (const auto& Entry : StepDefs)
		{
			if (Entry.first == Id)
			{
				return &Entry.second;
			}
		}
		UE_LOG("[AttackData] chains 가 모르는 스텝 id 참조: '%s' — 해당 스텝 스킵", Id.c_str());
		return nullptr;
	};

	// 체인 칸 1개 — 스텝 id 1개(string) 또는 변주 후보 배열({ "a", "b" }) 둘 다 슬롯으로 정규화.
	auto ParseSlot = [&FindStep](const sol::object& Entry) -> FMusouAttackSlot
	{
		FMusouAttackSlot Slot;
		if (Entry.is<std::string>())
		{
			if (const FMusouAttackStep* Step = FindStep(Entry.as<std::string>()))
			{
				Slot.Variants.push_back(*Step);
			}
		}
		else if (Entry.is<sol::table>())
		{
			sol::table Arr = Entry.as<sol::table>();
			const int32 Num = static_cast<int32>(Arr.size());
			for (int32 i = 1; i <= Num; ++i)
			{
				sol::optional<std::string> Id = Arr[i];
				if (!Id)
				{
					continue;
				}
				if (const FMusouAttackStep* Step = FindStep(*Id))
				{
					Slot.Variants.push_back(*Step);
				}
			}
		}
		return Slot;
	};

	auto ParseSlotArray = [&ParseSlot](const sol::table& Arr, TArray<FMusouAttackSlot>& Out)
	{
		const int32 Num = static_cast<int32>(Arr.size());
		for (int32 i = 1; i <= Num; ++i)
		{
			FMusouAttackSlot Slot = ParseSlot(Arr[i]);
			if (Slot.IsValid())
			{
				Out.push_back(std::move(Slot));
			}
		}
	};

	// ── chains ──
	TArray<FMusouAttackSlot> NewLight[NumContexts];
	FMusouAttackSlot         NewHeavy[NumContexts];
	TArray<FMusouAttackSlot> NewBranch;

	static const char* ContextKeys[NumContexts] = { "idle", "moving", "air", "air_juggle" };   // EAttackContext 순서

	if (sol::optional<sol::table> ChainsT = Root["chains"])
	{
		if (sol::optional<sol::table> LightT = (*ChainsT)["light"])
		{
			for (int32 i = 0; i < NumContexts; ++i)
			{
				if (sol::optional<sol::table> Arr = (*LightT)[ContextKeys[i]])
				{
					ParseSlotArray(*Arr, NewLight[i]);
				}
			}
		}
		if (sol::optional<sol::table> HeavyT = (*ChainsT)["heavy"])
		{
			for (int32 i = 0; i < NumContexts; ++i)
			{
				NewHeavy[i] = ParseSlot((*HeavyT)[ContextKeys[i]]);
			}
		}
		if (sol::optional<sol::table> BranchT = (*ChainsT)["branch"])
		{
			ParseSlotArray(*BranchT, NewBranch);
		}
	}

	// ── ultimate / dodge / hit_react / weapon (선택) ──
	TArray<FMusouAttackSlot> NewUltimate;
	FMusouAttackSlot         NewDodge;
	FMusouAttackSlot         NewHitReact;
	FMusouAttackSlot         NewDeath;
	FMusouAttackSlot         NewWeaponDraw;
	FMusouAttackSlot         NewWeaponSheathe;
	if (sol::optional<sol::table> ChainsT = Root["chains"])
	{
		if (sol::optional<sol::table> UltimateT = (*ChainsT)["ultimate"])
		{
			ParseSlotArray(*UltimateT, NewUltimate);
		}
		NewDodge = ParseSlot((*ChainsT)["dodge"]);
		NewHitReact = ParseSlot((*ChainsT)["hit_react"]);
		NewDeath = ParseSlot((*ChainsT)["death"]);
		NewWeaponDraw = ParseSlot((*ChainsT)["weapon_draw"]);
		NewWeaponSheathe = ParseSlot((*ChainsT)["weapon_sheathe"]);
	}

	// ── feedback (선택) — 없거나 일부만 있으면 구조체 기본값 유지 ──
	FMusouFeedbackParams NewFeedback;
	if (sol::optional<sol::table> FeedbackT = Root["feedback"])
	{
		if (sol::optional<sol::table> Burst = (*FeedbackT)["kill_burst"])
		{
			NewFeedback.KillBurstMinKills   = Burst->get_or("min_kills",  NewFeedback.KillBurstMinKills);
			NewFeedback.KillBurstSlomoDur   = Burst->get_or("slomo_dur",  NewFeedback.KillBurstSlomoDur);
			NewFeedback.KillBurstSlomoRate  = Burst->get_or("slomo_rate", NewFeedback.KillBurstSlomoRate);
			NewFeedback.KillBurstShakeScale = Burst->get_or("shake",      NewFeedback.KillBurstShakeScale);
		}
		if (sol::optional<sol::table> AirCombo = (*FeedbackT)["air_combo"])
		{
			NewFeedback.AirComboGravityScale = AirCombo->get_or("gravity_scale", NewFeedback.AirComboGravityScale);
		}
		if (sol::optional<sol::table> Ultimate = (*FeedbackT)["ultimate"])
		{
			NewFeedback.UltimateHitsToFill = Ultimate->get_or("hits_to_fill", NewFeedback.UltimateHitsToFill);
			NewFeedback.UltimateBossGaugeMult = Ultimate->get_or("boss_gauge_mult", NewFeedback.UltimateBossGaugeMult);
		}
		if (sol::optional<sol::table> Heal = (*FeedbackT)["heal"])
		{
			NewFeedback.HealPerComboHit = Heal->get_or("per_hit", NewFeedback.HealPerComboHit);
		}
		if (sol::optional<sol::table> HitReact = (*FeedbackT)["hit_react"])
		{
			NewFeedback.HitReactCooldown = HitReact->get_or("cooldown", NewFeedback.HitReactCooldown);
		}
		if (sol::optional<sol::table> Weapon = (*FeedbackT)["weapon"])
		{
			NewFeedback.WeaponSwapFrac = Weapon->get_or("swap_frac", NewFeedback.WeaponSwapFrac);
		}
	}

	if (NewSpecs.empty() || NewLight[0].empty())
	{
		UE_LOG("[AttackData] specs 또는 light.idle 체인이 비어 있음 — 기존 데이터 유지");
		return false;
	}

	// 전부 성공 — 멤버 교체 + 세대 증가.
	Specs = std::move(NewSpecs);
	for (int32 i = 0; i < NumContexts; ++i)
	{
		LightChains[i] = std::move(NewLight[i]);
		HeavySlots[i]  = std::move(NewHeavy[i]);
	}
	BranchFinishers = std::move(NewBranch);
	UltimateChain = std::move(NewUltimate);
	DodgeSlot = std::move(NewDodge);
	HitReactSlot = std::move(NewHitReact);
	DeathSlot = std::move(NewDeath);
	WeaponDrawSlot = std::move(NewWeaponDraw);
	WeaponSheatheSlot = std::move(NewWeaponSheathe);
	Feedback = NewFeedback;
	++Version;
	return true;
}

void FAttackDataRegistry::LoadDefaults()
{
	// attack_data.lua 와 동일 내용의 컴파일 내장본 — 파일 부재/최초 파싱 실패 시 안전망.
	// 평소 튜닝은 lua 쪽에서. 여기는 lua 와 어긋나도 게임이 도는 것만 보장하면 된다.
	Specs = {
		//  Id                  Range  Height ConeCos          DmgMult KbDist KbDur
		{ FName("attack1"),     5.0f,  2.5f,  -1.0f,           2.5f,   6.0f,  0.30f },
		{ FName("attack2"),     3.5f,  2.5f,  ConeDegToCos(140.0f), 1.5f, 4.0f, 0.20f },
		{ FName("combo1"),      3.5f,  2.5f,  ConeDegToCos(140.0f), 1.0f, 1.5f, 0.10f },
		{ FName("combo2"),      3.5f,  2.5f,  ConeDegToCos(140.0f), 1.2f, 2.0f, 0.12f },
		{ FName("combo3"),      4.5f,  2.5f,  -1.0f,           2.0f,   5.0f,  0.25f },
		{ FName("dash_attack"), 4.0f,  2.5f,  ConeDegToCos(140.0f), 1.5f, 3.5f, 0.20f },
		{ FName("spin_attack"), 4.5f,  2.5f,  -1.0f,           1.8f,   4.0f,  0.20f },
		{ FName("jump_attack"), 4.5f,  3.5f,  -1.0f,           2.0f,   4.5f,  0.25f },
		{ FName("branch1"),     4.0f,  2.5f,  ConeDegToCos(140.0f), 1.4f, 3.0f, 0.18f },
		{ FName("branch2"),     4.5f,  2.0f,  -1.0f,           1.8f,   4.5f,  0.22f },
	};

	auto MakeStep = [](const char* MontageName, const char* SequenceName, float BlendIn,
	                   const char* AttackId, float HitFrac, float WinBegin, float WinEnd)
	{
		FMusouAttackStep Step;
		Step.MontagePath  = FString("Content/Montages/") + MontageName + "_Montage.uasset";
		if (SequenceName)
		{
			Step.SequencePath = FString("Content/Data/GameJam/Barbarian/") + SequenceName + ".uasset";
		}
		Step.BlendIn  = BlendIn;
		Step.AttackId = AttackId ? AttackId : "";
		Step.HitFrac  = HitFrac;
		Step.WindowBeginFrac = WinBegin;
		Step.WindowEndFrac   = WinEnd;
		return Step;
	};

	const FMusouAttackStep ComboV1  = MakeStep("Barbarian_Melee Combo Attack Ver. 1", nullptr, 0.1f, nullptr, -1.0f, -1.0f, -1.0f);
	const FMusouAttackStep ComboV2  = MakeStep("Barbarian_Melee Combo Attack Ver. 2", nullptr, 0.1f, nullptr, -1.0f, -1.0f, -1.0f);
	const FMusouAttackStep ComboV3  = MakeStep("Barbarian_Melee Combo Attack Ver. 3", nullptr, 0.1f, nullptr, -1.0f, -1.0f, -1.0f);
	const FMusouAttackStep Slide    = MakeStep("great sword slide attack_mixamo_com", "great sword slide attack_mixamo_com", 0.1f, "dash_attack", 0.35f, 0.55f, 0.85f);
	const FMusouAttackStep JumpAtk  = MakeStep("great sword jump attack_mixamo_com", "great sword jump attack_mixamo_com", 0.15f, "jump_attack", 0.45f, -1.0f, -1.0f);

	auto Single = [](const FMusouAttackStep& Step)
	{
		FMusouAttackSlot Slot;
		Slot.Variants.push_back(Step);
		return Slot;
	};

	LightChains[ContextIndex(EAttackContext::Idle)]           = { Single(ComboV1), Single(ComboV2), Single(ComboV3) };
	LightChains[ContextIndex(EAttackContext::Moving)]         = { Single(Slide),   Single(ComboV2), Single(ComboV3) };
	LightChains[ContextIndex(EAttackContext::Airborne)]       = { Single(JumpAtk) };
	LightChains[ContextIndex(EAttackContext::AirborneJuggle)] = { Single(JumpAtk) };

	HeavySlots[ContextIndex(EAttackContext::Idle)]           = Single(MakeStep("Barbarian_Melee Attack Backhand", nullptr, 0.2f, nullptr, -1.0f, -1.0f, -1.0f));
	HeavySlots[ContextIndex(EAttackContext::Moving)]         = Single(MakeStep("great sword high spin attack_mixamo_com", "great sword high spin attack_mixamo_com", 0.15f, "spin_attack", 0.40f, -1.0f, -1.0f));
	HeavySlots[ContextIndex(EAttackContext::Airborne)]       = Single(JumpAtk);
	HeavySlots[ContextIndex(EAttackContext::AirborneJuggle)] = Single(JumpAtk);

	BranchFinishers = {
		Single(MakeStep("Barbarian_Melee Attack Horizontal", "Barbarian_Melee Attack Horizontal", 0.1f, "branch1", 0.40f, -1.0f, -1.0f)),
		Single(MakeStep("Barbarian_Melee Attack 360 Low",    "Barbarian_Melee Attack 360 Low",    0.1f, "branch2", 0.45f, -1.0f, -1.0f)),
		Single(MakeStep("Barbarian_Melee Attack 360 High",   "Barbarian_Melee Attack 360 High",   0.1f, "attack1", 0.45f, -1.0f, -1.0f)),
	};

	UltimateChain.clear();               // 내장 fallback 에선 무쌍기/구르기/피격 리액션/발도납도 비활성 (lua 전용 구성)
	DodgeSlot = FMusouAttackSlot();
	HitReactSlot = FMusouAttackSlot();
	DeathSlot = FMusouAttackSlot();
	WeaponDrawSlot = FMusouAttackSlot();
	WeaponSheatheSlot = FMusouAttackSlot();
	Feedback = FMusouFeedbackParams();   // 구조체 기본값

	++Version;
}

const FAttackSpec* FAttackDataRegistry::FindSpec(const FName& Id) const
{
	for (const FAttackSpec& Spec : Specs)
	{
		if (Spec.Id == Id)
		{
			return &Spec;
		}
	}
	return nullptr;
}

const TArray<FMusouAttackSlot>& FAttackDataRegistry::GetLightChain(EAttackContext Context) const
{
	const TArray<FMusouAttackSlot>& Chain = LightChains[ContextIndex(Context)];

	// 저글 체인 미정의 시 일반 공중 체인 폴백 — lua 에 air_juggle 키가 없어도 동작.
	if (Context == EAttackContext::AirborneJuggle && Chain.empty())
	{
		return LightChains[ContextIndex(EAttackContext::Airborne)];
	}
	return Chain;
}

const FMusouAttackSlot* FAttackDataRegistry::GetHeavySlot(EAttackContext Context) const
{
	const FMusouAttackSlot& Slot = HeavySlots[ContextIndex(Context)];
	if (Slot.IsValid())
	{
		return &Slot;
	}
	if (Context == EAttackContext::AirborneJuggle)
	{
		const FMusouAttackSlot& AirSlot = HeavySlots[ContextIndex(EAttackContext::Airborne)];
		return AirSlot.IsValid() ? &AirSlot : nullptr;
	}
	return nullptr;
}

const FMusouAttackSlot* FAttackDataRegistry::GetBranchFinisher(int32 ComboStep) const
{
	if (BranchFinishers.empty() || ComboStep < 1)
	{
		return nullptr;
	}
	const int32 Last  = static_cast<int32>(BranchFinishers.size()) - 1;
	const int32 Index = (ComboStep - 1 <= Last) ? ComboStep - 1 : Last;
	return &BranchFinishers[Index];
}

// AttackTypes.h 선언의 정의 — notify (AnimNotify_MusouAttack) 가 호출하는 전역 조회.
const FAttackSpec* FindMusouAttackSpec(const FName& Id)
{
	return FAttackDataRegistry::Get().FindSpec(Id);
}
