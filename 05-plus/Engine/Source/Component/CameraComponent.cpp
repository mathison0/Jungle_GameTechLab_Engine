#include "CameraComponent.h"
#include "Object/Class.h"
#include "Camera/Camera.h"
#include "Serializer/Archive.h"

IMPLEMENT_RTTI(UCameraComponent, USceneComponent)

void UCameraComponent::PostConstruct()
{
	bCanEverTick = true;
	Camera = new FCamera();

	FTransform InitialTransform(
		FRotator(Camera->GetPitch(), Camera->GetYaw(), 0.0f),
		Camera->GetPosition()
	);
	SetRelativeTransform(InitialTransform);
}

UCameraComponent::~UCameraComponent()
{
	delete Camera;
	Camera = nullptr;
}

void UCameraComponent::DuplicateSubObjects()
{
	USceneComponent::DuplicateSubObjects();

	// copy constructor이 Camera 포인터를 얕게 복사했으므로
	// 원본과 설정이 동일한 새 FCamera를 생성해 소유권을 분리한다.

	Camera = Camera ? new FCamera(*Camera) : new FCamera();
}

void UCameraComponent::Tick(float DeltaTime)
{
	USceneComponent::Tick(DeltaTime);
	//TODO : will be add CameraArm, shake and interpolation  
}

void UCameraComponent::Serialize(FArchive& Ar)
{
	USceneComponent::Serialize(Ar);

	if (Camera == nullptr)
	{
		Camera = new FCamera();
	}

	float FieldOfView = Camera->GetFOV();
	float Speed = Camera->GetSpeed();
	float Sensitivity = Camera->GetMouseSensitivity();
	float NearPlane = Camera->GetNearPlane();
	float FarPlane = Camera->GetFarPlane();
	float OrthoWidth = Camera->GetOrthoWidth();
	uint32 ProjectionMode = static_cast<uint32>(Camera->GetProjectionMode());

	if (Ar.IsSaving())
	{
		Ar.Serialize("FieldOfView", FieldOfView);
		Ar.Serialize("Speed", Speed);
		Ar.Serialize("Sensitivity", Sensitivity);
		Ar.Serialize("NearPlane", NearPlane);
		Ar.Serialize("FarPlane", FarPlane);
		Ar.Serialize("OrthoWidth", OrthoWidth);
		Ar.Serialize("ProjectionMode", ProjectionMode);
	}
	else
	{
		if (Ar.Contains("FieldOfView"))
		{
			Ar.Serialize("FieldOfView", FieldOfView);
		}
		if (Ar.Contains("Speed"))
		{
			Ar.Serialize("Speed", Speed);
		}
		if (Ar.Contains("Sensitivity"))
		{
			Ar.Serialize("Sensitivity", Sensitivity);
		}
		if (Ar.Contains("NearPlane"))
		{
			Ar.Serialize("NearPlane", NearPlane);
		}
		if (Ar.Contains("FarPlane"))
		{
			Ar.Serialize("FarPlane", FarPlane);
		}
		if (Ar.Contains("OrthoWidth"))
		{
			Ar.Serialize("OrthoWidth", OrthoWidth);
		}
		if (Ar.Contains("ProjectionMode"))
		{
			Ar.Serialize("ProjectionMode", ProjectionMode);
		}

		Camera->SetFOV(FieldOfView);
		Camera->SetSpeed(Speed);
		Camera->SetMouseSensitivity(Sensitivity);
		Camera->SetNearPlane(NearPlane);
		Camera->SetFarPlane(FarPlane);
		Camera->SetProjectionMode(static_cast<ECameraProjectionMode>(ProjectionMode));
		Camera->SetOrthoWidth(OrthoWidth);
		OnTransformUpdated();
	}
}

void UCameraComponent::MoveForward(float Value)
{
	Camera->MoveForward(Value);
}

void UCameraComponent::MoveRight(float Value)
{
	Camera->MoveRight(Value);

}

void UCameraComponent::MoveUp(float Value)
{
	Camera->MoveUp(Value);

}

void UCameraComponent::Rotate(float DeltaYaw, float DeltaPitch)
{
	Camera->Rotate(DeltaYaw, DeltaPitch);
}

FCamera* UCameraComponent::GetCamera() const
{
	return Camera;
}

FMatrix UCameraComponent::GetViewMatrix() const
{
	return Camera->GetViewMatrix();
}

FMatrix UCameraComponent::GetProjectionMatrix() const
{
	return Camera->GetProjectionMatrix();
}

void UCameraComponent::SetFov(float inFov)
{
	Camera->SetFOV(inFov);
}

void UCameraComponent::SetSpeed(float Inspeed)
{
	Camera->SetSpeed(Inspeed);
}

void UCameraComponent::SetSensitivity(float InSetSensitivity)
{
	Camera->SetMouseSensitivity(InSetSensitivity);
}

void UCameraComponent::OnTransformUpdated()
{
	if (Camera == nullptr)
	{
		return;
	}

	const FTransform WorldTransform(GetWorldTransform());
	const FRotator WorldRotation = WorldTransform.Rotator();

	Camera->SetPosition(WorldTransform.GetLocation());
	Camera->SetRotation(WorldRotation.Yaw, WorldRotation.Pitch);
}
