// Copyright 2024 Chris Ashworth

#include "Component/AeonixFlyingMovementComponent.h"
#include "GameFramework/Pawn.h"
#include "Components/StaticMeshComponent.h"
#include "Engine.h"
#include "DrawDebugHelpers.h"

UAeonixFlyingMovementComponent::UAeonixFlyingMovementComponent()
{
	PrimaryComponentTick.bCanEverTick = true;

	// Configure base pawn movement
	MaxSpeed = FlyingSettings.MaxSpeed;
}

void UAeonixFlyingMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	// Update base movement settings from our settings
	MaxSpeed = FlyingSettings.MaxSpeed;

	// Apply movement to the UpdatedComponent
	// Velocity is set by path following via RequestDirectMove
	if (UpdatedComponent && !Velocity.IsNearlyZero())
	{
		FHitResult Hit;
		SafeMoveUpdatedComponent(Velocity * DeltaTime, UpdatedComponent->GetComponentRotation(), true, Hit);
	}

	// DO NOT call Super::TickComponent - we're handling all movement ourselves
	// Calling Super would cause the base class to also process movement, fighting with our commands
	// Just call the component base class for housekeeping
	UActorComponent::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void UAeonixFlyingMovementComponent::RequestDirectMove(const FVector& MoveVelocity, bool bForceMaxSpeed)
{
	// Simple direct velocity assignment - no smoothing, no interpolation
	Velocity = MoveVelocity;
}

bool UAeonixFlyingMovementComponent::CanStartPathFollowing() const
{
	return true;
}