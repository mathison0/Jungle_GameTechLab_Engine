#include "Component/AudioVolumeComponent.h"

#include "Editor/Viewport/ViewportCamera.h"
#include "GameFramework/AActor.h"
#include "GameFramework/World.h"
#include "Object/ObjectFactory.h"
#include "Serialization/Archive.h"

#include <algorithm>
#include <cmath>
#include <cstring>

DEFINE_CLASS(UAudioVolumeComponent, USceneComponent)
REGISTER_FACTORY(UAudioVolumeComponent)

UAudioVolumeComponent::UAudioVolumeComponent()
{
	bCanEverTick = true;
}

void UAudioVolumeComponent::BeginPlay()
{
	USceneComponent::BeginPlay();
	CurrentVolume = 0.0f;
	bPausedByOutsideVolume = false;
	bStartedByStartBehavior = false;
	PlaybackHandle = {};

	if (GetStartBehavior() == EAudioStartBehavior::OnBeginPlay && !SoundPath.empty())
	{
		PlaybackHandle = FAudioSystem::Get().Play2D(SoundPath, 0.0f, bLoop);
		bStartedByStartBehavior = PlaybackHandle.IsValid();
	}
}

void UAudioVolumeComponent::EndPlay()
{
	StopPlayback();
	StopPreview();
}

void UAudioVolumeComponent::OnUnregister()
{
	StopPlayback();
	StopPreview();
	bRegistered = false;
}

void UAudioVolumeComponent::PostDuplicate(UObject* Original)
{
	USceneComponent::PostDuplicate(Original);
	CurrentVolume = 0.0f;
	PlaybackHandle = {};
	PreviewPlaybackHandle = {};
	bPausedByOutsideVolume = false;
	bStartedByStartBehavior = false;
}

void UAudioVolumeComponent::Serialize(FArchive& Ar)
{
	USceneComponent::Serialize(Ar);
	Ar << "BoxExtent" << BoxExtent;
	Ar << "SoundPath" << SoundPath;
	Ar << "StartBehavior" << StartBehavior;
	Ar << "Loop" << bLoop;
	Ar << "OutsideVolumeBehavior" << OutsideVolumeBehavior;
	Ar << "Volume" << Volume;
	Ar << "FadeInTime" << FadeInTime;
	Ar << "FadeOutTime" << FadeOutTime;

	if (Ar.IsLoading())
	{
		ClampEditableValues();
	}
}

void UAudioVolumeComponent::GetEditableProperties(TArray<FPropertyDescriptor>& OutProps)
{
	USceneComponent::GetEditableProperties(OutProps);
	static const char* StartBehaviorNames[] = { "On BeginPlay", "On First Enter", "Manual Only" };
	static const char* OutsideBehaviorNames[] = { "Continue Playing", "Pause And Resume", "Stop And Restart" };
	OutProps.push_back({ "Sound Path", EPropertyType::String, &SoundPath });
	OutProps.push_back({ "Box Extent", EPropertyType::Vec3, &BoxExtent, 0.01f, 10000.0f, 0.1f });
	OutProps.push_back({ "Start Behavior", EPropertyType::Enum, &StartBehavior, 0.0f, 0.0f, 0.0f, StartBehaviorNames, 3 });
	OutProps.push_back({ "Loop", EPropertyType::Bool, &bLoop });
	OutProps.push_back({ "Outside Volume Behavior", EPropertyType::Enum, &OutsideVolumeBehavior, 0.0f, 0.0f, 0.0f, OutsideBehaviorNames, 3 });
	OutProps.push_back({ "Volume", EPropertyType::Float, &Volume, 0.0f, 2.0f, 0.01f });
	OutProps.push_back({ "Fade In Time", EPropertyType::Float, &FadeInTime, 0.0f, 60.0f, 0.1f });
	OutProps.push_back({ "Fade Out Time", EPropertyType::Float, &FadeOutTime, 0.0f, 60.0f, 0.1f });
}

void UAudioVolumeComponent::PostEditProperty(const char* PropertyName)
{
	USceneComponent::PostEditProperty(PropertyName);
	ClampEditableValues();

	if (PropertyName && strcmp(PropertyName, "Sound Path") == 0)
	{
		StopPlayback();
		StopPreview();
	}
}

FAudioHandle UAudioVolumeComponent::PlayPreview()
{
	if (SoundPath.empty())
	{
		return {};
	}

	StopPreview();
	PreviewPlaybackHandle = FAudioSystem::Get().Play2D(SoundPath, Volume, bLoop);
	return PreviewPlaybackHandle;
}

void UAudioVolumeComponent::PausePreview()
{
	if (PreviewPlaybackHandle.IsValid())
	{
		FAudioSystem::Get().Pause(PreviewPlaybackHandle);
	}
}

void UAudioVolumeComponent::ResumePreview()
{
	if (PreviewPlaybackHandle.IsValid())
	{
		FAudioSystem::Get().Resume(PreviewPlaybackHandle);
	}
	else
	{
		PlayPreview();
	}
}

void UAudioVolumeComponent::RestartPreview()
{
	if (PreviewPlaybackHandle.IsValid())
	{
		FAudioSystem::Get().Restart(PreviewPlaybackHandle);
	}
	else
	{
		PlayPreview();
	}
}

void UAudioVolumeComponent::StopPreview()
{
	if (PreviewPlaybackHandle.IsValid())
	{
		FAudioSystem::Get().Stop(PreviewPlaybackHandle);
		PreviewPlaybackHandle = {};
	}
}

bool UAudioVolumeComponent::IsPreviewPlaying() const
{
	return PreviewPlaybackHandle.IsValid() && FAudioSystem::Get().IsPlaying(PreviewPlaybackHandle);
}

void UAudioVolumeComponent::SetPreviewPlaybackTime(float TimeSeconds)
{
	if (PreviewPlaybackHandle.IsValid())
	{
		FAudioSystem::Get().SetPlaybackTime(PreviewPlaybackHandle, TimeSeconds);
	}
}

float UAudioVolumeComponent::GetPreviewPlaybackTime() const
{
	return PreviewPlaybackHandle.IsValid() ? FAudioSystem::Get().GetPlaybackTime(PreviewPlaybackHandle) : 0.0f;
}

float UAudioVolumeComponent::GetPreviewDuration() const
{
	if (PreviewPlaybackHandle.IsValid())
	{
		const float ActiveDuration = FAudioSystem::Get().GetDuration(PreviewPlaybackHandle);
		if (ActiveDuration > 0.0f)
		{
			return ActiveDuration;
		}
	}

	return FAudioSystem::Get().GetSoundDuration(SoundPath);
}

void UAudioVolumeComponent::TickComponent(float DeltaTime)
{
	if (SoundPath.empty())
	{
		StopPlayback();
		CurrentVolume = 0.0f;
		return;
	}

	const bool bInside = IsListenerInside();
	const float TargetVolume = bInside ? Volume : 0.0f;
	const float FadeTime = bInside ? FadeInTime : FadeOutTime;

	if (bInside)
	{
		const EAudioStartBehavior StartMode = GetStartBehavior();
		if (StartMode != EAudioStartBehavior::ManualOnly || PlaybackHandle.IsValid())
		{
			EnsurePlayback();
		}
	}

	if (FadeTime <= 0.0f)
	{
		CurrentVolume = TargetVolume;
	}
	else
	{
		const float Step = (Volume <= 0.0f ? 1.0f : Volume) * DeltaTime / FadeTime;
		if (CurrentVolume < TargetVolume)
		{
			CurrentVolume = std::min(TargetVolume, CurrentVolume + Step);
		}
		else if (CurrentVolume > TargetVolume)
		{
			CurrentVolume = std::max(TargetVolume, CurrentVolume - Step);
		}
	}

	if (PlaybackHandle.IsValid())
	{
		FAudioSystem::Get().SetVolume(PlaybackHandle, CurrentVolume);
	}

	if (!bInside && CurrentVolume <= 0.001f)
	{
		const EAudioOutsideBehavior OutsideBehavior = GetOutsideVolumeBehavior();
		if (OutsideBehavior == EAudioOutsideBehavior::PauseAndResume)
		{
			if (PlaybackHandle.IsValid() && FAudioSystem::Get().IsPlaying(PlaybackHandle))
			{
				FAudioSystem::Get().Pause(PlaybackHandle);
				bPausedByOutsideVolume = true;
			}
		}
		else if (OutsideBehavior == EAudioOutsideBehavior::StopAndRestart)
		{
			StopPlayback();
		}
		CurrentVolume = 0.0f;
	}
}

bool UAudioVolumeComponent::IsListenerInside() const
{
	const FVector Delta = GetListenerLocation() - GetWorldLocation();
	return std::abs(Delta.X) <= BoxExtent.X
		&& std::abs(Delta.Y) <= BoxExtent.Y
		&& std::abs(Delta.Z) <= BoxExtent.Z;
}

FVector UAudioVolumeComponent::GetListenerLocation() const
{
	const AActor* OwnerActor = GetOwner();
	const UWorld* World = OwnerActor ? OwnerActor->GetFocusedWorld() : nullptr;
	const FViewportCamera* Camera = World ? World->GetActiveCamera() : nullptr;
	return Camera ? Camera->GetLocation() : GetWorldLocation();
}

void UAudioVolumeComponent::EnsurePlayback()
{
	if (PlaybackHandle.IsValid())
	{
		if (bPausedByOutsideVolume)
		{
			FAudioSystem::Get().Resume(PlaybackHandle);
			bPausedByOutsideVolume = false;
			return;
		}

		if (FAudioSystem::Get().IsPlaying(PlaybackHandle))
		{
			return;
		}
		FAudioSystem::Get().Stop(PlaybackHandle);
		PlaybackHandle = {};
	}

	PlaybackHandle = FAudioSystem::Get().Play2D(SoundPath, CurrentVolume, bLoop);
	bPausedByOutsideVolume = false;
	bStartedByStartBehavior = PlaybackHandle.IsValid();
}

void UAudioVolumeComponent::StopPlayback()
{
	if (PlaybackHandle.IsValid())
	{
		FAudioSystem::Get().Stop(PlaybackHandle);
		PlaybackHandle = {};
	}
	bPausedByOutsideVolume = false;
}

void UAudioVolumeComponent::ClampEditableValues()
{
	BoxExtent.X = std::max(0.01f, std::abs(BoxExtent.X));
	BoxExtent.Y = std::max(0.01f, std::abs(BoxExtent.Y));
	BoxExtent.Z = std::max(0.01f, std::abs(BoxExtent.Z));
	Volume = std::clamp(Volume, 0.0f, 2.0f);
	FadeInTime = std::max(0.0f, FadeInTime);
	FadeOutTime = std::max(0.0f, FadeOutTime);
	StartBehavior = std::clamp(StartBehavior, 0, static_cast<int32>(EAudioStartBehavior::Count) - 1);
	OutsideVolumeBehavior = std::clamp(OutsideVolumeBehavior, 0, static_cast<int32>(EAudioOutsideBehavior::Count) - 1);
}

EAudioOutsideBehavior UAudioVolumeComponent::GetOutsideVolumeBehavior() const
{
	const int32 ClampedValue = std::clamp(
		OutsideVolumeBehavior,
		0,
		static_cast<int32>(EAudioOutsideBehavior::Count) - 1);
	return static_cast<EAudioOutsideBehavior>(ClampedValue);
}

EAudioStartBehavior UAudioVolumeComponent::GetStartBehavior() const
{
	const int32 ClampedValue = std::clamp(
		StartBehavior,
		0,
		static_cast<int32>(EAudioStartBehavior::Count) - 1);
	return static_cast<EAudioStartBehavior>(ClampedValue);
}
