#include "Picker.h"

#include "Scene/Scene.h"
#include "Actor/Actor.h"
#include "Camera/Camera.h"
#include "Component/ArrowComponent.h"
#include "Component/PrimitiveComponent.h"
#include "Component/BillboardComponent.h"
#include "Component/SubUVComponent.h"
#include "Component/TextRenderComponent.h"
#include "Component/UUIDTextRenderComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Component/SkyComponent.h"
#include "Renderer/MeshData.h"
#include "Viewport/Viewport.h"

#include <limits>

namespace
{
	bool PickPrimitiveComponentInternal(
		UScene* Scene,
		const FViewportEntry* Entry,
		int32 ScreenX,
		int32 ScreenY,
		const FPicker& Picker,
		UPrimitiveComponent*& OutClosestComponent,
		AActor*& OutClosestActor)
	{
		if (!Scene || !Entry)
		{
			return false;
		}

		const FRay Ray = Picker.ScreenToRay(*Entry, ScreenX, ScreenY);
		float ClosestDistance = (std::numeric_limits<float>::max)();

		for (AActor* Actor : Scene->GetActors())
		{
			if (!Actor || Actor->IsPendingDestroy() || !Actor->IsVisible())
			{
				continue;
			}

			for (UActorComponent* Component : Actor->GetComponents())
			{
				if (!Component || !Component->IsA(UPrimitiveComponent::StaticClass()))
				{
					continue;
				}

				if (Component->IsA(UUUIDTextRenderComponent::StaticClass()))
				{
					continue;
				}

				if (Component->IsA(USkyComponent::StaticClass()))
				{
					continue;
				}

				UPrimitiveComponent* PrimComp = static_cast<UPrimitiveComponent*>(Component);

				if (PrimComp->IsA(UArrowComponent::StaticClass())
					|| PrimComp->IsA(UBillboardComponent::StaticClass())
					|| PrimComp->IsA(USubUVComponent::StaticClass())
					|| PrimComp->IsA(UTextRenderComponent::StaticClass()))
				{
					const FBoxSphereBounds Bounds = PrimComp->GetWorldBounds();
					const FVector ToCenter = Bounds.Center - Ray.Origin;
					const float T = FVector::DotProduct(ToCenter, Ray.Direction);
					if (T < 0.0f)
					{
						continue;
					}

					const FVector ClosestPoint = Ray.Origin + Ray.Direction * T;
					const float DistSq = (ClosestPoint - Bounds.Center).SizeSquared();
					const float RadiusSq = Bounds.Radius * Bounds.Radius;

					if (DistSq <= RadiusSq && T < ClosestDistance)
					{
						ClosestDistance = T;
						OutClosestComponent = PrimComp;
						OutClosestActor = Actor;
					}
					continue;
				}

				if (PrimComp->IsA(UStaticMeshComponent::StaticClass()))
				{
					UStaticMeshComponent* SMC = static_cast<UStaticMeshComponent*>(PrimComp);
					FRenderMesh* Mesh = SMC->GetRenderMesh();
					if (!Mesh || Mesh->Vertices.empty() || Mesh->Indices.empty())
					{
						continue;
					}

					const FMatrix World = SMC->GetWorldTransform();

					for (uint32 Index = 0; Index + 2 < Mesh->Indices.size(); Index += 3)
					{
						const FVector& P0 = Mesh->Vertices[Mesh->Indices[Index]].Position;
						const FVector& P1 = Mesh->Vertices[Mesh->Indices[Index + 1]].Position;
						const FVector& P2 = Mesh->Vertices[Mesh->Indices[Index + 2]].Position;

						const FVector W0 = {
							P0.X * World.M[0][0] + P0.Y * World.M[1][0] + P0.Z * World.M[2][0] + World.M[3][0],
							P0.X * World.M[0][1] + P0.Y * World.M[1][1] + P0.Z * World.M[2][1] + World.M[3][1],
							P0.X * World.M[0][2] + P0.Y * World.M[1][2] + P0.Z * World.M[2][2] + World.M[3][2]
						};
						const FVector W1 = {
							P1.X * World.M[0][0] + P1.Y * World.M[1][0] + P1.Z * World.M[2][0] + World.M[3][0],
							P1.X * World.M[0][1] + P1.Y * World.M[1][1] + P1.Z * World.M[2][1] + World.M[3][1],
							P1.X * World.M[0][2] + P1.Y * World.M[1][2] + P1.Z * World.M[2][2] + World.M[3][2]
						};
						const FVector W2 = {
							P2.X * World.M[0][0] + P2.Y * World.M[1][0] + P2.Z * World.M[2][0] + World.M[3][0],
							P2.X * World.M[0][1] + P2.Y * World.M[1][1] + P2.Z * World.M[2][1] + World.M[3][1],
							P2.X * World.M[0][2] + P2.Y * World.M[1][2] + P2.Z * World.M[2][2] + World.M[3][2]
						};

						float Distance = 0.0f;
						if (Picker.RayTriangleIntersect(Ray, W0, W1, W2, Distance) && Distance < ClosestDistance)
						{
							ClosestDistance = Distance;
							OutClosestComponent = PrimComp;
							OutClosestActor = Actor;
						}
					}
				}
			}
		}

		return OutClosestComponent != nullptr;
	}
}

FRay FPicker::ScreenToRay(const FViewportEntry& Entry, int32 ScreenX, int32 ScreenY) const
{
	if (!Entry.Viewport)
	{
		return { FVector::ZeroVector, FVector::ForwardVector };
	}

	const auto& Rect = Entry.Viewport->GetRect();
	if (Rect.Width <= 0 || Rect.Height <= 0)
	{
		return { FVector::ZeroVector, FVector::ForwardVector };
	}

	const float AspectRatio = static_cast<float>(Rect.Width) / static_cast<float>(Rect.Height);

	const FMatrix ViewMatrix = Entry.LocalState.BuildViewMatrix();
	const FMatrix ProjMatrix = Entry.LocalState.BuildProjMatrix(AspectRatio);
	const FMatrix ViewInverse = ViewMatrix.GetInverse();
	const float NdcX = (2.0f * (ScreenX + 0.5f) / Rect.Width) - 1.0f;
	const float NdcY = 1.0f - (2.0f * (ScreenY + 0.5f) / Rect.Height);

	if (Entry.LocalState.ProjectionType != EViewportType::Perspective)
	{
		const float ViewHeight = Entry.LocalState.OrthoZoom * 2.0f;
		const float ViewWidth = ViewHeight * AspectRatio;

		const float ViewRight = NdcX * (ViewWidth * 0.5f);
		const float ViewUp = NdcY * (ViewHeight * 0.5f);

		FVector RayOrigin;
		RayOrigin.X = ViewRight * ViewInverse.M[1][0] + ViewUp * ViewInverse.M[2][0] + ViewInverse.M[3][0];
		RayOrigin.Y = ViewRight * ViewInverse.M[1][1] + ViewUp * ViewInverse.M[2][1] + ViewInverse.M[3][1];
		RayOrigin.Z = ViewRight * ViewInverse.M[1][2] + ViewUp * ViewInverse.M[2][2] + ViewInverse.M[3][2];

		FVector Forward = FVector::ForwardVector;

		switch (Entry.LocalState.ProjectionType)
		{
		case EViewportType::OrthoTop:
			Forward = FVector::DownVector;
			break;
		case EViewportType::OrthoBottom:
			Forward = FVector::UpVector;
			break;
		case EViewportType::OrthoLeft:
			Forward = FVector::RightVector;
			break;
		case EViewportType::OrthoRight:
			Forward = FVector::LeftVector;
			break;
		case EViewportType::OrthoFront:
			Forward = FVector::BackwardVector;
			break;
		case EViewportType::OrthoBack:
			Forward = FVector::ForwardVector;
			break;
		default:
			break;
		}

		return { RayOrigin, Forward };
	}

	const float ViewForward = 1.0f;
	const float ViewRight = NdcX / ProjMatrix.M[1][0];
	const float ViewUp = NdcY / ProjMatrix.M[2][1];

	FVector RayDirectionWorld;
	RayDirectionWorld.X = ViewForward * ViewInverse.M[0][0] + ViewRight * ViewInverse.M[1][0] + ViewUp * ViewInverse.M[2][0];
	RayDirectionWorld.Y = ViewForward * ViewInverse.M[0][1] + ViewRight * ViewInverse.M[1][1] + ViewUp * ViewInverse.M[2][1];
	RayDirectionWorld.Z = ViewForward * ViewInverse.M[0][2] + ViewRight * ViewInverse.M[1][2] + ViewUp * ViewInverse.M[2][2];
	RayDirectionWorld = RayDirectionWorld.GetSafeNormal();

	FVector RayOrigin;
	RayOrigin.X = ViewInverse.M[3][0];
	RayOrigin.Y = ViewInverse.M[3][1];
	RayOrigin.Z = ViewInverse.M[3][2];

	return { RayOrigin, RayDirectionWorld };
}

bool FPicker::RayTriangleIntersect(
	const FRay& Ray,
	const FVector& V0,
	const FVector& V1,
	const FVector& V2,
	float& OutDistance) const
{
	constexpr float Epsilon = 1.e-6f;

	const FVector Edge1 = V1 - V0;
	const FVector Edge2 = V2 - V0;

	const FVector H = FVector::CrossProduct(Ray.Direction, Edge2);
	const float A = FVector::DotProduct(Edge1, H);
	if (A <= Epsilon)
	{
		return false;
	}

	const float F = 1.0f / A;
	const FVector S = Ray.Origin - V0;
	const float U = F * FVector::DotProduct(S, H);
	if (U < 0.0f || U > 1.0f)
	{
		return false;
	}

	const FVector Q = FVector::CrossProduct(S, Edge1);
	const float V = F * FVector::DotProduct(Ray.Direction, Q);
	if (V < 0.0f || U + V > 1.0f)
	{
		return false;
	}

	const float T = F * FVector::DotProduct(Edge2, Q);
	if (T > Epsilon)
	{
		OutDistance = T;
		return true;
	}

	return false;
}

AActor* FPicker::PickActor(UScene* Scene, const FViewportEntry* Entry, int32 ScreenX, int32 ScreenY) const
{
	UPrimitiveComponent* ClosestComponent = nullptr;
	AActor* ClosestActor = nullptr;
	PickPrimitiveComponentInternal(Scene, Entry, ScreenX, ScreenY, *this, ClosestComponent, ClosestActor);
	return ClosestActor;
}

UPrimitiveComponent* FPicker::PickPrimitiveComponent(UScene* Scene, const FViewportEntry* Entry, int32 ScreenX, int32 ScreenY) const
{
	UPrimitiveComponent* ClosestComponent = nullptr;
	AActor* ClosestActor = nullptr;
	PickPrimitiveComponentInternal(Scene, Entry, ScreenX, ScreenY, *this, ClosestComponent, ClosestActor);
	return ClosestComponent;
}
