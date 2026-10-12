// Copyright 2024 Chris Ashworth

#include "Component/AeonixFlyingMovementComponent.h"
#include "Interface/AeonixNavVolumeInterface.h"
#include "Subsystem/AeonixSubsystem.h"
#include "GameFramework/Pawn.h"
#include "Components/StaticMeshComponent.h"
#include "Engine.h"
#include "DrawDebugHelpers.h"

UAeonixFlyingMovementComponent::UAeonixFlyingMovementComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UAeonixFlyingMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	if (bRideNavVolume)
	{
		RideNavVolume();
	}

	// Determine if we have active input (non-zero target)
	const bool bHasInput = !TargetVelocity.IsNearlyZero();

	if (bHasInput)
	{
		// Accelerate toward target velocity
		const FVector Delta = TargetVelocity - Velocity;
		const float DeltaSize = Delta.Size();
		const float MaxStep = FlyingSettings.Acceleration * DeltaTime;

		if (DeltaSize <= MaxStep)
		{
			Velocity = TargetVelocity;
		}
		else
		{
			Velocity += Delta.GetSafeNormal() * MaxStep;
		}
	}
	else
	{
		// Decelerate to stop
		const float CurrentSpeed = Velocity.Size();
		if (CurrentSpeed > 0.0f)
		{
			const float SpeedDrop = FlyingSettings.Deceleration * DeltaTime;
			const float NewSpeed = FMath::Max(CurrentSpeed - SpeedDrop, 0.0f);
			Velocity = (NewSpeed > KINDA_SMALL_NUMBER) ? Velocity.GetSafeNormal() * NewSpeed : FVector::ZeroVector;
		}
	}

	// Update rotation to face velocity direction
	FRotator NewRotation = UpdatedComponent ? UpdatedComponent->GetComponentRotation() : FRotator::ZeroRotator;
	if (!Velocity.IsNearlyZero())
	{
		const FRotator DesiredRotation = Velocity.GetSafeNormal().Rotation();
		const float TurnRateRadians = FMath::DegreesToRadians(FlyingSettings.TurnRate);
		NewRotation = FMath::RInterpTo(NewRotation, DesiredRotation, DeltaTime, TurnRateRadians);
	}

	// Apply movement to the UpdatedComponent
	if (UpdatedComponent && !Velocity.IsNearlyZero())
	{
		FHitResult Hit;
		SafeMoveUpdatedComponent(Velocity * DeltaTime, NewRotation, true, Hit);
	}
	else if (UpdatedComponent)
	{
		// Still apply rotation even when not moving
		UpdatedComponent->SetWorldRotation(NewRotation);
	}

	// DO NOT call Super::TickComponent - we're handling all movement ourselves
	// Calling Super would cause the base class to also process movement, fighting with our commands
	// Just call the component base class for housekeeping
	UActorComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void UAeonixFlyingMovementComponent::RequestDirectMove(const FVector& MoveVelocity, bool bForceMaxSpeed)
{
	const FVector Direction = MoveVelocity.GetSafeNormal();

	if (bForceMaxSpeed)
	{
		// Set target to max speed in the requested direction
		TargetVelocity = Direction * FlyingSettings.MaxSpeed;
	}
	else
	{
		// Set target to requested velocity but clamp to max speed
		const float RequestedSpeed = MoveVelocity.Size();
		const float ClampedSpeed = FMath::Min(RequestedSpeed, FlyingSettings.MaxSpeed);
		TargetVelocity = Direction * ClampedSpeed;
	}
}

bool UAeonixFlyingMovementComponent::CanStartPathFollowing() const
{
	return true;
}

void UAeonixFlyingMovementComponent::StopMovementImmediately()
{
	Super::StopMovementImmediately();
	TargetVelocity = FVector::ZeroVector;
}

void UAeonixFlyingMovementComponent::RideNavVolume()
{
	if (!UpdatedComponent)
	{
		return;
	}

	// Carry the pawn by however far the volume moved, judged from where the volume was last tick
	if (const IAeonixNavVolumeInterface* Volume = RiddenVolume.Get())
	{
		const FTransform NavToWorld = Volume->GetNavToWorld();
		const FVector Location = UpdatedComponent->GetComponentLocation();
		const FVector NewLocation = NavToWorld.TransformPosition(RiddenNavToWorld.InverseTransformPosition(Location));
		const FQuat DeltaRotation = NavToWorld.GetRotation() * RiddenNavToWorld.GetRotation().Inverse();
		RiddenNavToWorld = NavToWorld;

		if (!NewLocation.Equals(Location) || !DeltaRotation.IsIdentity())
		{
			UpdatedComponent->SetWorldLocationAndRotation(NewLocation, DeltaRotation * UpdatedComponent->GetComponentQuat(), false, nullptr, ETeleportType::TeleportPhysics);
			Velocity = DeltaRotation.RotateVector(Velocity);
			TargetVelocity = DeltaRotation.RotateVector(TargetVelocity);
		}

		if (Volume->IsPointInside(NewLocation))
		{
			return;
		}
	}

	const FVector Location = UpdatedComponent->GetComponentLocation();
	UAeonixSubsystem* Subsystem = GetWorld() ? GetWorld()->GetSubsystem<UAeonixSubsystem>() : nullptr;
	const IAeonixNavVolumeInterface* Volume = Subsystem ? Subsystem->GetVolumeForPosition(Location) : nullptr;
	RiddenVolume.Reset();
	if (Volume)
	{
		RiddenVolume = TWeakInterfacePtr<IAeonixNavVolumeInterface>(const_cast<IAeonixNavVolumeInterface*>(Volume));
		RiddenNavToWorld = Volume->GetNavToWorld();
	}
}
