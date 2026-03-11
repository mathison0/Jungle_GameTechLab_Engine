#include "SoundManager.h"

void SoundManager::Init()
{
	audioEngine = std::make_unique<DirectX::AudioEngine>();
}

void SoundManager::Update()
{
	if (!audioEngine->Update())
	{
		// 장치가 없을 때
	}
}

void SoundManager::Reset()
{
	if (bgmInstance)
	{
		bgmInstance->Stop();
	}

	if (audioEngine)
	{
		audioEngine->Reset();
	}

	this->PlayBGM("bgm", true);
}

void SoundManager::LoadSound(const std::string& name, const std::wstring& path)
{
	soundLibrary[name] = std::make_unique<DirectX::SoundEffect>(audioEngine.get(), path.c_str());
}

void SoundManager::PlayEffect(const std::string& name)
{
	if (soundLibrary.count(name))
	{
		soundLibrary[name]->Play();
	}
}

void SoundManager::PlayBGM(const std::string& name, bool loop)
{
	// 이미 같은 노래가 재생중이라면 무시 
	if (currentBgmName == name && bgmInstance->GetState() == DirectX::SoundState::PLAYING)
		return;

	if (soundLibrary.count(name))
	{
		// 기존 배경음이 있다면 정지
		if (bgmInstance) bgmInstance->Stop();

		// 해당하는 사운드 데이터를 관리할 수 있는 인스턴스 생성
		bgmInstance = soundLibrary[name]->CreateInstance();
		bgmInstance->Play(loop);

		// 현재 재생 음악 업데이트
		currentBgmName = name;
	}
}

void SoundManager::StopBGM()
{
	if (bgmInstance)
	{
		bgmInstance->Stop();
		currentBgmName = "";
	}
}

void SoundManager::SetBGMVolume(float volume)
{
	if (bgmInstance)
	{
		bgmInstance->SetVolume(volume);
	}
}

void SoundManager::Release()
{
	if (bgmInstance)
	{
		bgmInstance->Stop();
		bgmInstance.reset();
	}


	if (audioEngine)
	{
		audioEngine->Suspend();
		audioEngine.reset();

	}

	soundLibrary.clear();

}

void SoundManager::OnGameStateChanged(EGameState newState)
{
	switch (newState)

	{
	case EGameState::Running:

		if (bgmInstance)
		{
			bgmInstance->Stop();

		}

		PlayBGM("bgm", true);
		break;
		
	case EGameState::Ending:
		StopBGM();
		PlayEffect("Ending1");
		break;
	}
}


