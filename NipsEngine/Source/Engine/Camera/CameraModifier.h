#pragma once

#include "Core/CoreMinimal.h"
#include "Object/Object.h"

struct FCameraViewInfo;

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

private:
	bool bEnabled = true;
	int32 Priority = 0;
};
