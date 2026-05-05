#include "Engine/Camera/CameraModifier.h"

#include "Engine/Camera/PlayerCameraManager.h"

DEFINE_CLASS(UCameraModifier, UObject)

bool UCameraModifier::ModifyCamera(float DeltaTime, FCameraViewInfo& InOutView)
{
	(void)DeltaTime;
	(void)InOutView;
	return false;
}

void UCameraModifier::EnableModifier()
{
	bEnabled = true;
}

void UCameraModifier::DisableModifier()
{
	bEnabled = false;
}
