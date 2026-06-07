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
	TArray<FMusouAttackSlot> NewLight[3];
	FMusouAttackSlot         NewHeavy[3];
	TArray<FMusouAttackSlot> NewBranch;

	static const char* ContextKeys[3] = { "idle", "moving", "air" };   // EAttackContext 순서

	if (sol::optional<sol::table> ChainsT = Root["chains"])
	{
		if (sol::optional<sol::table> LightT = (*ChainsT)["light"])
		{
			for (int32 i = 0; i < 3; ++i)
			{
				if (sol::optional<sol::table> Arr = (*LightT)[ContextKeys[i]])
				{
					ParseSlotArray(*Arr, NewLight[i]);
				}
			}
		}
		if (sol::optional<sol::table> HeavyT = (*ChainsT)["heavy"])
		{
			for (int32 i = 0; i < 3; ++i)
			{
				NewHeavy[i] = ParseSlot((*HeavyT)[ContextKeys[i]]);
			}
		}
		if (sol::optional<sol::table> BranchT = (*ChainsT)["branch"])
		{
			ParseSlotArray(*BranchT, NewBranch);
		}
	}

	if (NewSpecs.empty() || NewLight[0].empty())
	{
		UE_LOG("[AttackData] specs 또는 light.idle 체인이 비어 있음 — 기존 데이터 유지");
		return false;
	}

	// 전부 성공 — 멤버 교체 + 세대 증가.
	Specs = std::move(NewSpecs);
	for (int32 i = 0; i < 3; ++i)
	{
		LightChains[i] = std::move(NewLight[i]);
		HeavySlots[i]  = std::move(NewHeavy[i]);
	}
	BranchFinishers = std::move(NewBranch);
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

	LightChains[ContextIndex(EAttackContext::Idle)]     = { Single(ComboV1), Single(ComboV2), Single(ComboV3) };
	LightChains[ContextIndex(EAttackContext::Moving)]   = { Single(Slide),   Single(ComboV2), Single(ComboV3) };
	LightChains[ContextIndex(EAttackContext::Airborne)] = { Single(JumpAtk) };

	HeavySlots[ContextIndex(EAttackContext::Idle)]     = Single(MakeStep("Barbarian_Melee Attack Backhand", nullptr, 0.2f, nullptr, -1.0f, -1.0f, -1.0f));
	HeavySlots[ContextIndex(EAttackContext::Moving)]   = Single(MakeStep("great sword high spin attack_mixamo_com", "great sword high spin attack_mixamo_com", 0.15f, "spin_attack", 0.40f, -1.0f, -1.0f));
	HeavySlots[ContextIndex(EAttackContext::Airborne)] = Single(JumpAtk);

	BranchFinishers = {
		Single(MakeStep("Barbarian_Melee Attack Horizontal", "Barbarian_Melee Attack Horizontal", 0.1f, "branch1", 0.40f, -1.0f, -1.0f)),
		Single(MakeStep("Barbarian_Melee Attack 360 Low",    "Barbarian_Melee Attack 360 Low",    0.1f, "branch2", 0.45f, -1.0f, -1.0f)),
		Single(MakeStep("Barbarian_Melee Attack 360 High",   "Barbarian_Melee Attack 360 High",   0.1f, "attack1", 0.45f, -1.0f, -1.0f)),
	};

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
	return LightChains[ContextIndex(Context)];
}

const FMusouAttackSlot* FAttackDataRegistry::GetHeavySlot(EAttackContext Context) const
{
	const FMusouAttackSlot& Slot = HeavySlots[ContextIndex(Context)];
	return Slot.IsValid() ? &Slot : nullptr;
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
