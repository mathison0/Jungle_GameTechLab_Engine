#include "AudioManager.h"
#include "Core/Logging/Log.h"
#include "Platform/Paths.h"
#include <algorithm>
#include <chrono>

bool FAudioManager::Initialize()
{
	if (FMOD::System_Create(&System) != FMOD_OK || !System)
	{
		UE_LOG("Failed to create FMOD system.");
		return false;
	}

	System->setSoftwareChannels(128);

	if (System->init(512, FMOD_INIT_NORMAL, nullptr) != FMOD_OK)
	{
		UE_LOG("Failed to initialize FMOD system.");
		Shutdown();
		return false;
	}

	System->getMasterChannelGroup(&MasterGroup);

	LoadDefaultAudios();

	return true;
}

void FAudioManager::Shutdown()
{
	if (!System)
	{
		MasterGroup = nullptr;
		BGMChannel = nullptr;
		LoopChannels.clear();
		ManagedSfxChannels.clear();
		LastManagedSfxPlayTimes.clear();
		Audios.clear();
		AudioPaths.clear();
		return;
	}

	StopBGM();
	StopAllLoops();
	for (auto& Pair : ManagedSfxChannels)
	{
		for (FManagedSfxChannel& Entry : Pair.second)
		{
			if (Entry.Channel)
			{
				Entry.Channel->stop();
			}
		}
	}
	ManagedSfxChannels.clear();
	LastManagedSfxPlayTimes.clear();

	if (MasterGroup)
	{
		MasterGroup->stop();
		MasterGroup = nullptr;
	}
	System->update();

	for (auto& Pair : Audios)
	{
		if (Pair.second)
		{
			Pair.second->release();
		}
	}
	Audios.clear();
	AudioPaths.clear();

	System->update();
	System->close();
	System->release();
	System = nullptr;
}

void FAudioManager::Tick()
{
	if (System)
	{
		System->update();
	}
}

bool FAudioManager::LoadAudio(const FString& Key, const FString& Path, bool bLoop)
{
	if (!System)
	{
		return false;
	}

	FString FullPath = FPaths::ToUtf8(FPaths::Combine(FPaths::AudioDir(), FPaths::ToWide(Path)));

	if (Audios.contains(Key) && Audios[Key] && AudioPaths.contains(Key) && AudioPaths[Key] == FullPath)
	{
		return true;
	}

	FMOD::Sound* Sound = nullptr;
	const FMOD_MODE Mode = FMOD_DEFAULT | (bLoop ? FMOD_LOOP_NORMAL : FMOD_LOOP_OFF);

	if (System->createSound(FullPath.c_str(), Mode, nullptr, &Sound) != FMOD_OK)
	{
		return false;
	}

	if (Audios.contains(Key) && Audios[Key])
	{
		StopManagedSfxChannels(Key);
		Audios[Key]->release();
	}

	Audios[Key] = Sound;
	AudioPaths[Key] = FullPath;
	return true;
}

void FAudioManager::PlayAudio(const FString& Key, float Volume)
{
	if (!System || !Audios.contains(Key))
	{
		return;
	}

	FMOD::Channel* Channel = nullptr;
	System->playSound(Audios[Key], nullptr, false, &Channel);

	if (Channel)
	{
		Channel->setVolume(Volume);
	}
}

void FAudioManager::PlaySfxLimited(const FString& Key, float Volume, int32 MaxConcurrent, float MinIntervalSeconds)
{
	if (!System || !Audios.contains(Key) || MaxConcurrent <= 0)
	{
		return;
	}

	CleanupManagedSfxChannels(Key);

	const double Now = GetAudioTimeSeconds();
	if (MinIntervalSeconds > 0.0f && LastManagedSfxPlayTimes.contains(Key))
	{
		const double Elapsed = Now - LastManagedSfxPlayTimes[Key];
		if (Elapsed < static_cast<double>(MinIntervalSeconds))
		{
			return;
		}
	}

	TArray<FManagedSfxChannel>& Channels = ManagedSfxChannels[Key];
	if (static_cast<int32>(Channels.size()) >= MaxConcurrent)
	{
		auto OldestIt = std::min_element(Channels.begin(), Channels.end(),
			[](const FManagedSfxChannel& Lhs, const FManagedSfxChannel& Rhs)
			{
				return Lhs.StartTimeSeconds < Rhs.StartTimeSeconds;
			});

		if (OldestIt != Channels.end())
		{
			if (OldestIt->Channel)
			{
				OldestIt->Channel->stop();
			}
			Channels.erase(OldestIt);
		}
	}

	FMOD::Channel* Channel = nullptr;
	const FMOD_RESULT Result = System->playSound(Audios[Key], nullptr, false, &Channel);
	if (Result != FMOD_OK || !Channel)
	{
		UE_LOG("[AudioManager] PlaySfxLimited failed. Key=%s Result=%d", Key.c_str(), static_cast<int32>(Result));
		return;
	}

	Channel->setVolume(std::clamp(Volume, 0.0f, 1.0f));
	Channels.push_back({ Channel, Now });
	LastManagedSfxPlayTimes[Key] = Now;
}

void FAudioManager::PlayBGM(const FString& Key, float Volume)
{
	if (!System || !Audios.contains(Key))
	{
		return;
	}

	StopBGM();
	System->playSound(Audios[Key], nullptr, false, &BGMChannel);

	if (BGMChannel)
	{
		BGMChannel->setVolume(Volume);
	}
}

void FAudioManager::StopBGM()
{
	if (BGMChannel)
	{
		BGMChannel->stop();
		BGMChannel = nullptr;
	}
}

void FAudioManager::PlayLoop(const FString& Key, const FString& LoopName, float Volume, float Pitch)
{
	if (!System || !Audios.contains(Key) || LoopName.empty())
	{
		return;
	}

	if (FMOD::Channel* ExistingChannel = FindPlayingLoopChannel(LoopName))
	{
		ExistingChannel->setVolume(std::clamp(Volume, 0.0f, 1.0f));
		ExistingChannel->setPitch(std::clamp(Pitch, 0.1f, 3.0f));
		return;
	}

	FMOD::Channel* Channel = nullptr;
	System->playSound(Audios[Key], nullptr, false, &Channel);

	if (Channel)
	{
		Channel->setMode(FMOD_LOOP_NORMAL);
		Channel->setVolume(std::clamp(Volume, 0.0f, 1.0f));
		Channel->setPitch(std::clamp(Pitch, 0.1f, 3.0f));
		LoopChannels[LoopName] = Channel;
	}
}

void FAudioManager::StopLoop(const FString& LoopName)
{
	if (!LoopChannels.contains(LoopName))
	{
		return;
	}

	if (LoopChannels[LoopName])
	{
		LoopChannels[LoopName]->stop();
	}
	LoopChannels.erase(LoopName);
}

void FAudioManager::StopAllLoops()
{
	for (auto& Pair : LoopChannels)
	{
		if (Pair.second)
		{
			Pair.second->stop();
		}
	}
	LoopChannels.clear();
}

void FAudioManager::SetLoopVolume(const FString& LoopName, float Volume)
{
	if (FMOD::Channel* Channel = FindPlayingLoopChannel(LoopName))
	{
		Channel->setVolume(std::clamp(Volume, 0.0f, 1.0f));
	}
}

void FAudioManager::SetLoopPitch(const FString& LoopName, float Pitch)
{
	if (FMOD::Channel* Channel = FindPlayingLoopChannel(LoopName))
	{
		Channel->setPitch(std::clamp(Pitch, 0.1f, 3.0f));
	}
}

bool FAudioManager::IsLoopPlaying(const FString& LoopName)
{
	return FindPlayingLoopChannel(LoopName) != nullptr;
}

FMOD::Channel* FAudioManager::FindPlayingLoopChannel(const FString& LoopName)
{
	if (!LoopChannels.contains(LoopName))
	{
		return nullptr;
	}

	FMOD::Channel* Channel = LoopChannels[LoopName];
	bool bIsPlaying = false;
	if (!Channel || Channel->isPlaying(&bIsPlaying) != FMOD_OK || !bIsPlaying)
	{
		LoopChannels.erase(LoopName);
		return nullptr;
	}

	return Channel;
}

void FAudioManager::CleanupManagedSfxChannels(const FString& Key)
{
	if (!ManagedSfxChannels.contains(Key))
	{
		return;
	}

	TArray<FManagedSfxChannel>& Channels = ManagedSfxChannels[Key];
	Channels.erase(
		std::remove_if(Channels.begin(), Channels.end(),
			[](const FManagedSfxChannel& Entry)
			{
				bool bIsPlaying = false;
				return !Entry.Channel || Entry.Channel->isPlaying(&bIsPlaying) != FMOD_OK || !bIsPlaying;
			}),
		Channels.end());

	if (Channels.empty())
	{
		ManagedSfxChannels.erase(Key);
	}
}

void FAudioManager::StopManagedSfxChannels(const FString& Key)
{
	if (!ManagedSfxChannels.contains(Key))
	{
		return;
	}

	for (FManagedSfxChannel& Entry : ManagedSfxChannels[Key])
	{
		if (Entry.Channel)
		{
			Entry.Channel->stop();
		}
	}

	ManagedSfxChannels.erase(Key);
	LastManagedSfxPlayTimes.erase(Key);
}

double FAudioManager::GetAudioTimeSeconds() const
{
	using FClock = std::chrono::steady_clock;
	return std::chrono::duration<double>(FClock::now().time_since_epoch()).count();
}

void FAudioManager::SetMasterVolume(float Volume)
{
	if (MasterGroup)
	{
		MasterGroup->setVolume(Volume);
	}
}

void FAudioManager::LoadDefaultAudios()
{
	LoadAudio("CityBgm", "city_bgm.mp3", true);
	LoadAudio("Phase_EscapePolice", "phase_escapepolice.wav", true);
	LoadAudio("Phase_Meteor", "phase_meteor.mp3", true);
	LoadAudio("Click", "pop.mp3");
	LoadAudio("CarEngineLoop", "car_engine_loop.mp3", true);
	LoadAudio("Notify", "notify.mp3");
	LoadAudio("Complete", "complete.mp3");
	LoadAudio("Crash", "crash.mp3");
	LoadAudio("Water", "water.mp3", true);
	LoadAudio("Siren", "siren.mp3", true);
	LoadAudio("Fueling", "fueling.mp3", true);
	LoadAudio("ScoreUp", "score_up.mp3");
	LoadAudio("MeteorBoom", "meteor_boom.mp3");
	LoadAudio("MeteorFall", "meteor_fall.mp3");
	LoadAudio("Whoosh", "whoosh.mp3");
}
