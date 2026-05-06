#pragma once

#include "Core/CoreMinimal.h"
#include "Object/Object.h"

struct FCameraViewInfo;

enum class ECameraModifierResult : uint8
{
	Continue, // 다음 Camera Modifier 계속 실행
	Stop      // 이후의 Camera Modifier 연산 중단
};

class UCameraModifier : public UObject
{
public:
	DECLARE_CLASS(UCameraModifier, UObject)

	UCameraModifier() = default;
	~UCameraModifier() override = default;

	virtual bool ModifyCamera(float DeltaTime, FCameraViewInfo& InOutView);

	virtual void EnableModifier();
	virtual void DisableModifier();

	bool IsEnabled() const { return bEnabled; }
	void SetPriority(int32 InPriority) { Priority = InPriority; }
	int32 GetPriority() const { return Priority; }

	bool ShouldAutoRemove() const { return bAutoRemoveModifiers; }

private:
	bool bEnabled = true;
	bool bAutoRemoveModifiers = true;
	int32 Priority = 0;
};
