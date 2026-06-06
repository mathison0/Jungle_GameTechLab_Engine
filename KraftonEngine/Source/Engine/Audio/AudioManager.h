#pragma once

#include "Core/Singleton.h"
#include "Core/Types/CoreTypes.h"
#include <fmod.hpp>

class FAudioManager : public TSingleton<FAudioManager>
{
	friend class TSingleton<FAudioManager>;

public:
	bool Initialize();
	void Shutdown();
	void Tick();

	bool LoadAudio(const FString& Key, const FString& Path, bool bLoop = false);
	void PlayAudio(const FString& Key, float Volume = 1.0f);
	void PlaySfxLimited(const FString& Key, float Volume = 1.0f, int32 MaxConcurrent = 4, float MinIntervalSeconds = 0.0f);
	void PlayBGM(const FString& Key, float Volume = 1.0f);
	void StopBGM();
	void PlayLoop(const FString& Key, const FString& LoopName, float Volume = 1.0f, float Pitch = 1.0f);
	void StopLoop(const FString& LoopName);
	void StopAllLoops();
	void SetLoopVolume(const FString& LoopName, float Volume);
	void SetLoopPitch(const FString& LoopName, float Pitch);
	bool IsLoopPlaying(const FString& LoopName);

	void SetMasterVolume(float Volume);

private:
	struct FManagedSfxChannel
	{
		FMOD::Channel* Channel = nullptr;
		double StartTimeSeconds = 0.0;
	};

	void LoadDefaultAudios();
	FMOD::Channel* FindPlayingLoopChannel(const FString& LoopName);
	void CleanupManagedSfxChannels(const FString& Key);
	void StopManagedSfxChannels(const FString& Key);
	double GetAudioTimeSeconds() const;

private:
	FAudioManager() = default;
	~FAudioManager() = default;

	FMOD::System* System = nullptr;
	FMOD::ChannelGroup* MasterGroup = nullptr;
	FMOD::Channel* BGMChannel = nullptr;

	TMap<FString, FMOD::Sound*> Audios;
	TMap<FString, FString> AudioPaths;
	TMap<FString, FMOD::Channel*> LoopChannels;
	TMap<FString, TArray<FManagedSfxChannel>> ManagedSfxChannels;
	TMap<FString, double> LastManagedSfxPlayTimes;
};
