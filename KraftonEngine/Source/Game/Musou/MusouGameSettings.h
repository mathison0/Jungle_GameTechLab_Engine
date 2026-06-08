#pragma once

#include "Core/Singleton.h"

// ============================================================
// FMusouGameSettings — 게임 전역 사용자 설정 (세션 내 유지).
//
// TSingleton 이라 씬 전환에도 살아남는다(월드는 파괴돼도 정적 인스턴스는 유지).
// 지금은 "카메라 연출(몽타주 카메라 샷)" on/off 하나. 설정 UI(Intro/HUD)가 토글하고,
// AMusouCharacter::BeginPlay 가 이 값으로 SetCameraShotsSuppressed 를 적용한다.
// ============================================================
class FMusouGameSettings : public TSingleton<FMusouGameSettings>
{
	friend class TSingleton<FMusouGameSettings>;

public:
	// 카메라 연출 — 몽타주 카메라 샷(시점 전환).
	bool IsCameraDirectionEnabled() const { return bCameraDirectionEnabled; }
	void SetCameraDirectionEnabled(bool bEnabled) { bCameraDirectionEnabled = bEnabled; }
	void ToggleCameraDirection() { bCameraDirectionEnabled = !bCameraDirectionEnabled; }

	// 카메라 셰이크 — 히트/킬/무쌍기/notify 흔들림.
	bool IsCameraShakeEnabled() const { return bCameraShakeEnabled; }
	void SetCameraShakeEnabled(bool bEnabled) { bCameraShakeEnabled = bEnabled; }
	void ToggleCameraShake() { bCameraShakeEnabled = !bCameraShakeEnabled; }

private:
	FMusouGameSettings() = default;
	~FMusouGameSettings() = default;

	bool bCameraDirectionEnabled = true;   // 기본 ON (몽타주 카메라 연출 사용)
	bool bCameraShakeEnabled = true;       // 기본 ON (카메라 흔들림 사용)
};
