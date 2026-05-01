#pragma once

#include "Component/SceneComponent.h"
#include "Audio/AudioSystem.h"

class UAudioVolumeComponent : public USceneComponent
{
public:
	DECLARE_CLASS(UAudioVolumeComponent, USceneComponent)

	UAudioVolumeComponent();
	~UAudioVolumeComponent() override = default;

	void BeginPlay() override;
	void EndPlay() override;
	void OnUnregister() override;
	void PostDuplicate(UObject* Original) override;
	void Serialize(FArchive& Ar) override;
	void GetEditableProperties(TArray<FPropertyDescriptor>& OutProps) override;
	void PostEditProperty(const char* PropertyName) override;

	const FString& GetSoundPath() const { return SoundPath; }
	void SetSoundPath(const FString& InSoundPath) { SoundPath = InSoundPath; }
	const FVector& GetBoxExtent() const { return BoxExtent; }

	FAudioHandle PlayPreview();
	void PausePreview();
	void ResumePreview();
	void RestartPreview();
	void StopPreview();
	bool IsPreviewPlaying() const;
	bool HasPreviewPlayback() const { return PreviewPlaybackHandle.IsValid(); }
	void SetPreviewPlaybackTime(float TimeSeconds);
	float GetPreviewPlaybackTime() const;
	float GetPreviewDuration() const;

protected:
	void TickComponent(float DeltaTime) override;

private:
	bool IsListenerInside() const;
	FVector GetListenerLocation() const;
	void EnsurePlayback();
	void StopPlayback();
	void ClampEditableValues();
	EAudioOutsideBehavior GetOutsideVolumeBehavior() const;
	EAudioStartBehavior GetStartBehavior() const;

	FVector BoxExtent = FVector(4.0f, 4.0f, 2.0f);
	FString SoundPath;
	int32 StartBehavior = static_cast<int32>(EAudioStartBehavior::OnFirstEnter);
	bool bLoop = true;
	int32 OutsideVolumeBehavior = static_cast<int32>(EAudioOutsideBehavior::StopAndRestart);
	float Volume = 1.0f;
	float FadeInTime = 1.0f;
	float FadeOutTime = 1.0f;

	float CurrentVolume = 0.0f;
	bool bPausedByOutsideVolume = false;
	bool bStartedByStartBehavior = false;
	FAudioHandle PlaybackHandle;
	FAudioHandle PreviewPlaybackHandle;
};
