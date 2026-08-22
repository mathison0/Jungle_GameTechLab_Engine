#pragma once
#include <map>
#include <string>
#include <memory>
#include <Audio.h>
#include "IGameStateListener.h"

class SoundManager : public IGameStateListener
{
public:
	// 싱글톤
	static SoundManager& Get() { static SoundManager instance; return instance; }

	void Init();
	void Update();
	void Reset();

	void LoadSound(const std::string& name, const std::wstring& path);

	void PlayEffect(const std::string& name);

	void PlayBGM(const std::string& name, bool loop = true);
	void StopBGM();
	void SetBGMVolume(float volume);
	
	void Release();

	void OnGameStateChanged(EGameState newState) override;

private:
	SoundManager() = default;

	std::unique_ptr<DirectX::AudioEngine> audioEngine;
	std::map<std::string, std::unique_ptr<DirectX::SoundEffect>> soundLibrary;

	std::unique_ptr<DirectX::SoundEffectInstance> bgmInstance;
	std::string currentBgmName = "";
};
