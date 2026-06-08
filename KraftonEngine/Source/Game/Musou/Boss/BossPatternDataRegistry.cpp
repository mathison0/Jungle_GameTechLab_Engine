#include "Game/Musou/Boss/BossPatternDataRegistry.h"

#include "Core/Logging/Log.h"
#include "Lua/LuaScriptManager.h"
#include "Platform/Paths.h"
#include "sol/sol.hpp"

#include <filesystem>
#include <utility>

namespace
{
	constexpr const char* BossDataFile = "Data/boss_data.lua";

	float Clamp01(float Value)
	{
		if (Value < 0.0f) return 0.0f;
		if (Value > 1.0f) return 1.0f;
		return Value;
	}

	EBossPatternStepType ParseStepType(const FString& Type)
	{
		if (Type == "face_target") return EBossPatternStepType::FaceTarget;
		if (Type == "play_montage") return EBossPatternStepType::PlayMontage;
		if (Type == "attack") return EBossPatternStepType::Attack;
		if (Type == "dash") return EBossPatternStepType::Dash;
		if (Type == "projectile") return EBossPatternStepType::SpawnProjectile;
		if (Type == "effect") return EBossPatternStepType::SpawnEffect;
		if (Type == "invincible") return EBossPatternStepType::SetInvincible;
		if (Type == "warning_rim") return EBossPatternStepType::WarningRim;
		return EBossPatternStepType::Wait;
	}

	EBossSequenceStepType ParseSequenceStepType(const FString& Type)
	{
		if (Type == "dialogue") return EBossSequenceStepType::Dialogue;
		if (Type == "play_montage") return EBossSequenceStepType::PlayMontage;
		if (Type == "blend_camera") return EBossSequenceStepType::BlendCamera;
		if (Type == "restore_camera") return EBossSequenceStepType::RestoreCamera;
		if (Type == "play_sound") return EBossSequenceStepType::PlayAudio;
		if (Type == "lock_player") return EBossSequenceStepType::LockPlayer;
		if (Type == "unlock_player") return EBossSequenceStepType::UnlockPlayer;
		if (Type == "set_pattern_enabled") return EBossSequenceStepType::SetBossPatternEnabled;
		if (Type == "face_target") return EBossSequenceStepType::FaceBossToPlayer;
		if (Type == "stop_movement") return EBossSequenceStepType::StopMovement;
		if (Type == "set_invincible") return EBossSequenceStepType::SetInvincible;
		if (Type == "destroy_actor") return EBossSequenceStepType::DestroyActor;
		return EBossSequenceStepType::Wait;
	}

	FVector ParseVector(const sol::object& Obj, const FVector& DefaultValue = FVector::ZeroVector)
	{
		if (!Obj.is<sol::table>())
		{
			return DefaultValue;
		}

		sol::table T = Obj.as<sol::table>();
		return FVector(
			T.get_or(1, DefaultValue.X),
			T.get_or(2, DefaultValue.Y),
			T.get_or(3, DefaultValue.Z));
	}

	FVector4 ParseColor(const sol::object& Obj, const FVector4& DefaultValue)
	{
		if (!Obj.is<sol::table>())
		{
			return DefaultValue;
		}

		sol::table T = Obj.as<sol::table>();
		return FVector4(
			T.get_or(1, DefaultValue.X),
			T.get_or(2, DefaultValue.Y),
			T.get_or(3, DefaultValue.Z),
			T.get_or(4, DefaultValue.W));
	}

	FBossPatternStep ParseStep(const sol::table& T)
	{
		FBossPatternStep Step;
		Step.Type = ParseStepType(T.get_or("type", std::string("wait")));
		Step.Time = T.get_or("time", 0.0f);
		Step.Duration = T.get_or("duration", 0.0f);
		Step.AttackSpecId = FName(T.get_or("attack_id", std::string()));
		Step.MontagePath = T.get_or("montage", std::string());
		Step.ProjectileClassName = T.get_or("projectile", std::string());
		Step.EffectPath = T.get_or("effect", std::string());
		Step.Offset = ParseVector(T["offset"]);
		Step.Color = ParseColor(T["color"], Step.Color);
		Step.Speed = T.get_or("speed", 0.0f);
		Step.Intensity = T.get_or("intensity", Step.Intensity);
		Step.RimIntensity = T.get_or("rim", Step.RimIntensity);
		Step.RimPower = T.get_or("rim_power", Step.RimPower);
		Step.FillAmount = T.get_or("fill", Step.FillAmount);
		Step.bValue = T.get_or("value", false);
		return Step;
	}

	FBossSequenceStep ParseSequenceStep(const sol::table& T)
	{
		FBossSequenceStep Step;
		Step.Type = ParseSequenceStepType(T.get_or("type", std::string("wait")));
		Step.Time = T.get_or("time", 0.0f);
		Step.Duration = T.get_or("duration", 0.0f);
		Step.Text = T.get_or("text", std::string());
		Step.MontagePath = T.get_or("montage", std::string());
		Step.SoundPath = T.get_or("sound", std::string());
		Step.PlayRate = T.get_or("play_rate", 1.0f);
		Step.BlendIn = T.get_or("blend_in", 0.1f);
		Step.Volume = T.get_or("volume", 1.0f);
		Step.CameraOffset = ParseVector(T["offset"], Step.CameraOffset);
		Step.LookAtHeight = T.get_or("look_at_height", 1.5f);
		Step.FOV = T.get_or("fov", 0.87266463f);
		Step.bValue = T.get_or("value", false);
		Step.bLoop = T.get_or("loop", false);
		return Step;
	}

	FBossPhaseSequence ParsePhaseSequence(const sol::table& T)
	{
		FBossPhaseSequence Phase;
		Phase.Id = FName(T.get_or("id", std::string()));
		Phase.HealthRatio = Clamp01(T.get_or("hp", 0.5f));
		Phase.bOnce = T.get_or("once", true);

		if (sol::optional<sol::table> StepsT = T["steps"])
		{
			for (int32 Index = 1; Index <= static_cast<int32>(StepsT->size()); ++Index)
			{
				sol::optional<sol::table> StepT = (*StepsT)[Index];
				if (StepT)
				{
					Phase.Steps.push_back(ParseSequenceStep(*StepT));
				}
			}
		}

		return Phase;
	}

	FBossPattern ParsePattern(const FString& Id, const sol::table& T)
	{
		FBossPattern Pattern;
		Pattern.Id = FName(Id);
		Pattern.AttackSpecId = FName(T.get_or("attack_id", std::string()));
		Pattern.MontagePath = T.get_or("montage", std::string());
		Pattern.SequencePath = T.get_or("sequence", std::string());
		Pattern.MinRange = T.get_or("min_range", 0.0f);
		Pattern.MaxRange = T.get_or("max_range", 5.0f);
		Pattern.Cooldown = T.get_or("cooldown", 2.0f);
		Pattern.TelegraphTime = T.get_or("telegraph", 0.4f);
		Pattern.AttackTime = T.get_or("attack_time", 0.15f);
		Pattern.RecoveryTime = T.get_or("recovery", 0.8f);
		Pattern.PlayRate = T.get_or("play_rate", 1.0f);
		Pattern.BlendIn = T.get_or("blend_in", 0.1f);
		Pattern.Weight = T.get_or("weight", 1);
		Pattern.bFaceTargetBeforeAttack = T.get_or("face_target", true);
		Pattern.bCanMoveDuringTelegraph = T.get_or("move_during_telegraph", false);
		Pattern.bUseAnimNotify = T.get_or("use_anim_notify", false);

		if (sol::optional<sol::table> Hp = T["hp"])
		{
			Pattern.HealthRatioMin = Clamp01(Hp->get_or(1, 0.0f));
			Pattern.HealthRatioMax = Clamp01(Hp->get_or(2, 1.0f));
		}

		if (sol::optional<sol::table> StepsT = T["steps"])
		{
			for (int32 Index = 1; Index <= static_cast<int32>(StepsT->size()); ++Index)
			{
				sol::optional<sol::table> StepT = (*StepsT)[Index];
				if (StepT)
				{
					Pattern.Steps.push_back(ParseStep(*StepT));
				}
			}
		}

		if (Pattern.MaxRange < Pattern.MinRange)
		{
			std::swap(Pattern.MaxRange, Pattern.MinRange);
		}
		return Pattern;
	}
}

FBossPatternDataRegistry& FBossPatternDataRegistry::Get()
{
	static FBossPatternDataRegistry Instance;
	if (!Instance.bLoadedOnce)
	{
		Instance.EnsureFresh();
	}
	return Instance;
}

void FBossPatternDataRegistry::EnsureFresh()
{
	const std::wstring WidePath = FPaths::Combine(FPaths::ScriptDir(), FPaths::ToWide(BossDataFile));

	std::error_code EC;
	const auto WriteTime = std::filesystem::last_write_time(WidePath, EC);
	const long long Stamp = EC ? 0 : static_cast<long long>(WriteTime.time_since_epoch().count());

	if (bLoadedOnce && Stamp == LastWriteStamp)
	{
		return;
	}

	if (EC)
	{
		if (!bLoadedOnce)
		{
			UE_LOG("[BossData] '%s' not found - using built-in fallback", BossDataFile);
			LoadDefaults();
			bLoadedOnce = true;
		}
		LastWriteStamp = Stamp;
		return;
	}

	if (!LoadFromLua() && !bLoadedOnce)
	{
		UE_LOG("[BossData] first load failed - using built-in fallback");
		LoadDefaults();
	}

	bLoadedOnce = true;
	LastWriteStamp = Stamp;
}

const FBossDefinition* FBossPatternDataRegistry::FindBoss(const FName& BossId) const
{
	for (const FBossDefinition& Boss : Bosses)
	{
		if (Boss.BossId == BossId)
		{
			return &Boss;
		}
	}
	return nullptr;
}

bool FBossPatternDataRegistry::LoadFromLua()
{
	FString Content;
	if (!FLuaScriptManager::ReadScriptFileContent(BossDataFile, Content))
	{
		UE_LOG("[BossData] failed to read '%s'", BossDataFile);
		return false;
	}

	sol::state& Lua = FLuaScriptManager::GetState();
	sol::load_result Chunk = Lua.load(Content, BossDataFile);
	if (!Chunk.valid())
	{
		sol::error Err = Chunk;
		UE_LOG("[BossData] syntax error: %s", Err.what());
		return false;
	}

	sol::protected_function_result Result = Chunk();
	if (!Result.valid())
	{
		sol::error Err = Result;
		UE_LOG("[BossData] runtime error: %s", Err.what());
		return false;
	}

	sol::object RootObj = Result;
	if (RootObj.get_type() != sol::type::table)
	{
		UE_LOG("[BossData] root must be a table");
		return false;
	}

	sol::table Root = RootObj.as<sol::table>();
	sol::optional<sol::table> BossesT = Root["bosses"];
	if (!BossesT)
	{
		UE_LOG("[BossData] missing bosses table");
		return false;
	}

	TArray<FBossDefinition> NewBosses;
	for (const auto& BossKV : *BossesT)
	{
		if (BossKV.first.get_type() != sol::type::string || BossKV.second.get_type() != sol::type::table)
		{
			continue;
		}

		const FString BossId = BossKV.first.as<std::string>();
		sol::table BossT = BossKV.second.as<sol::table>();

		FBossDefinition Boss;
		Boss.BossId = FName(BossId);
		Boss.DisplayName = BossT.get_or("display_name", std::string());
		Boss.MaxHealth = BossT.get_or("max_hp", 1000.0f);
		Boss.AttackPower = BossT.get_or("attack_power", 20.0f);
		Boss.MoveSpeed = BossT.get_or("move_speed", 3.0f);
		Boss.StopDistance = BossT.get_or("stop_distance", 2.5f);
		Boss.MeshPath = BossT.get_or("mesh", std::string());
		Boss.AnimScript = BossT.get_or("anim_script", std::string());
		Boss.IdleMontagePath = BossT.get_or("idle_montage", std::string());
		Boss.IdleMontagePlayRate = BossT.get_or("idle_play_rate", 1.0f);
		Boss.IdleMontageBlendIn = BossT.get_or("idle_blend_in", 0.1f);
		Boss.RunMontagePath = BossT.get_or("run_montage", std::string());
		Boss.RunMontagePlayRate = BossT.get_or("run_play_rate", 1.0f);
		Boss.RunMontageBlendIn = BossT.get_or("run_blend_in", 0.1f);

		if (sol::optional<sol::table> IntroT = BossT["intro"])
		{
			for (int32 Index = 1; Index <= static_cast<int32>(IntroT->size()); ++Index)
			{
				sol::optional<sol::table> StepT = (*IntroT)[Index];
				if (StepT)
				{
					Boss.IntroSteps.push_back(ParseSequenceStep(*StepT));
				}
			}
		}

		if (sol::optional<sol::table> DeathT = BossT["death"])
		{
			for (int32 Index = 1; Index <= static_cast<int32>(DeathT->size()); ++Index)
			{
				sol::optional<sol::table> StepT = (*DeathT)[Index];
				if (StepT)
				{
					Boss.DeathSteps.push_back(ParseSequenceStep(*StepT));
				}
			}
		}

		if (sol::optional<sol::table> PhaseSequencesT = BossT["phase_sequences"])
		{
			for (int32 Index = 1; Index <= static_cast<int32>(PhaseSequencesT->size()); ++Index)
			{
				sol::optional<sol::table> PhaseT = (*PhaseSequencesT)[Index];
				if (!PhaseT)
				{
					continue;
				}

				FBossPhaseSequence Phase = ParsePhaseSequence(*PhaseT);
				if (Phase.Id.IsValid() && !Phase.Steps.empty())
				{
					Boss.PhaseSequences.push_back(std::move(Phase));
				}
			}
		}

		if (sol::optional<sol::table> PatternsT = BossT["patterns"])
		{
			for (int32 Index = 1; Index <= static_cast<int32>(PatternsT->size()); ++Index)
			{
				sol::optional<sol::table> PatternT = (*PatternsT)[Index];
				if (!PatternT)
				{
					continue;
				}
				const FString PatternId = PatternT->get_or("id", std::string());
				if (PatternId.empty())
				{
					continue;
				}
				FBossPattern Pattern = ParsePattern(PatternId, *PatternT);
				if ((Pattern.AttackSpecId.IsValid() || Pattern.IsSequence()) && Pattern.Weight > 0)
				{
					Boss.Patterns.push_back(std::move(Pattern));
				}
			}
		}

		if (Boss.IsValid())
		{
			NewBosses.push_back(std::move(Boss));
		}
	}

	if (NewBosses.empty())
	{
		UE_LOG("[BossData] no valid boss definitions");
		return false;
	}

	Bosses = std::move(NewBosses);
	UE_LOG("[BossData] loaded %d boss definitions", static_cast<int32>(Bosses.size()));
	return true;
}

void FBossPatternDataRegistry::LoadDefaults()
{
	FBossDefinition Boss;
	Boss.BossId = FName("knight_boss");
	Boss.DisplayName = "Knight Boss";
	Boss.MaxHealth = 1200.0f;
	Boss.AttackPower = 18.0f;
	Boss.MoveSpeed = 3.2f;
	Boss.StopDistance = 3.0f;
	Boss.MeshPath = "Content/Data/GameJam/Knight/SK_Knight_SkeletalMesh.uasset";
	Boss.AnimScript = "Anim/boss_knight_anim.lua";
	Boss.IdleMontagePath = "Content/Montages/sword and shield idle (4)_mixamo_com_Montage.uasset";
	Boss.RunMontagePath = "Content/Montages/sword and shield run_mixamo_com_Montage.uasset";
	Boss.IntroSteps = {
		{ EBossSequenceStepType::LockPlayer, 0.0f, 0.0f, FString(), FString(), FString(), 1.0f, 0.1f, 1.0f, FVector(-4.5f, -3.0f, 2.0f), 1.5f, 0.87266463f, true },
		{ EBossSequenceStepType::SetBossPatternEnabled, 0.0f, 0.0f, FString(), FString(), FString(), 1.0f, 0.1f, 1.0f, FVector(-4.5f, -3.0f, 2.0f), 1.5f, 0.87266463f, false },
		{ EBossSequenceStepType::FaceBossToPlayer, 0.0f, 0.0f },
		{ EBossSequenceStepType::BlendCamera, 0.0f, 0.35f, FString(), FString(), FString(), 1.0f, 0.1f, 1.0f, FVector(-4.5f, -3.0f, 2.0f), 1.5f, 0.87266463f, false },
		{ EBossSequenceStepType::PlayAudio, 0.35f, 0.0f, FString(), FString(), "Boss_Warrior/boss_warrior_00.wav", 1.0f, 0.1f, 1.0f },
		{ EBossSequenceStepType::Dialogue, 0.4f, 0.0f, "Boss encounter started." },
		{ EBossSequenceStepType::PlayMontage, 1.2f, 0.0f, FString(), Boss.IdleMontagePath, FString(), 1.0f, 0.1f },
		{ EBossSequenceStepType::RestoreCamera, 3.75f, 0.25f },
		{ EBossSequenceStepType::UnlockPlayer, 4.0f, 0.0f },
		{ EBossSequenceStepType::SetBossPatternEnabled, 4.0f, 0.0f, FString(), FString(), FString(), 1.0f, 0.1f, 1.0f, FVector(-4.5f, -3.0f, 2.0f), 1.5f, 0.87266463f, true },
	};
	Boss.DeathSteps = {
		{ EBossSequenceStepType::SetBossPatternEnabled, 0.0f, 0.0f, FString(), FString(), FString(), 1.0f, 0.1f, 1.0f, FVector::ZeroVector, 1.5f, 0.87266463f, false },
		{ EBossSequenceStepType::StopMovement, 0.0f, 0.0f },
		{ EBossSequenceStepType::SetInvincible, 0.0f, 0.0f, FString(), FString(), FString(), 1.0f, 0.1f, 1.0f, FVector::ZeroVector, 1.5f, 0.87266463f, true },
		{ EBossSequenceStepType::PlayAudio, 0.05f, 0.0f, FString(), FString(), "Boss_Warrior/boss_warrior_00.wav", 1.0f, 0.1f, 1.0f },
	};

	FBossPattern Slash;
	Slash.Id = FName("slash");
	Slash.AttackSpecId = FName("boss_slash");
	Slash.MinRange = 0.0f;
	Slash.MaxRange = 4.5f;
	Slash.Cooldown = 2.0f;
	Slash.TelegraphTime = 0.4f;
	Slash.AttackTime = 0.15f;
	Slash.RecoveryTime = 0.8f;
	Slash.Weight = 5;
	Boss.Patterns.push_back(Slash);

	Bosses = { Boss };
}
