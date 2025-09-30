// Copyright 2024 Chris Ashworth

#include "Component/AeonixPathFollowingComponent.h"
#include "Component/AeonixNavAgentComponent.h"
#include "Component/AeonixFlyingMovementComponent.h"
#include "Pathfinding/AeonixNavigationPath.h"
#include "Subsystem/AeonixSubsystem.h"
#include <GameFramework/Pawn.h>
#include <GameFramework/PawnMovementComponent.h>
#include <Components/StaticMeshComponent.h>
#include <Engine/Engine.h>
#include <DrawDebugHelpers.h>

UAeonixPathFollowingComponent::UAeonixPathFollowingComponent()
	: CurrentWaypointIndex(0)
	, LastVelocity(FVector::ZeroVector)
	, CurrentAeonixPath(nullptr)
	, bInitializationComplete(false)
	, InitializationRetryTimer(0.0f)
	, InitializationRetryCount(0)
	, bProcessingMovementThisFrame(false)
	, LastProcessedFrameNumber(0)
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UAeonixPathFollowingComponent::BeginPlay()
{
	Super::BeginPlay();

	// Start the initialization process
	TryInitializeNavigation();
}

void UAeonixPathFollowingComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	// Call parent to trigger FollowPathSegment() at the right time
	// We've overridden FollowPathSegment() to use our custom path following
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Handle initialization retry if not complete
	if (!bInitializationComplete)
	{
		InitializationRetryTimer += DeltaTime;
		if (InitializationRetryTimer >= INITIALIZATION_RETRY_INTERVAL)
		{
			InitializationRetryTimer = 0.0f;
			TryInitializeNavigation();
		}
	}
}

void UAeonixPathFollowingComponent::FollowPathSegment(float DeltaTime)
{
	// DO NOT call Super - we're completely replacing the base class path following logic
	// The base class would read the wrong path and send incorrect movement commands

	// CRITICAL: Prevent double-processing per frame
	const uint64 CurrentFrameNumber = GFrameCounter;
	if (CurrentFrameNumber == LastProcessedFrameNumber)
	{
		UE_LOG(LogTemp, Warning, TEXT("AeonixPathFollowing: Skipping duplicate FollowPathSegment call in same frame"));
		return;
	}
	LastProcessedFrameNumber = CurrentFrameNumber;

	if (!bInitializationComplete)
	{
		UE_LOG(LogTemp, VeryVerbose, TEXT("AeonixPathFollowing: Not initialized yet"));
		return;
	}

	if (!CurrentAeonixPath)
	{
		UE_LOG(LogTemp, Warning, TEXT("AeonixPathFollowing: CurrentAeonixPath is null"));
		return;
	}

	if (!CurrentAeonixPath->IsReady())
	{
		UE_LOG(LogTemp, Warning, TEXT("AeonixPathFollowing: CurrentAeonixPath is not ready (has %d points)"),
			CurrentAeonixPath->GetPathPoints().Num());
		return;
	}

	// Path is valid, follow it
	FollowAeonixPath();

	// Only continue moving if we're still following a path (not paused/finished)
	if (GetStatus() == EPathFollowingStatus::Moving)
	{
		UpdateMovement(DeltaTime);
		UpdateRotation(DeltaTime);
	}
}

void UAeonixPathFollowingComponent::UpdatePathSegment()
{
	// DO NOT call Super - we handle path updates ourselves using the Aeonix path
	// DO NOT do movement processing here - that only happens in FollowPathSegment()
	// This function is for segment transitions only

	if (!bInitializationComplete)
	{
		return;
	}

	// Just validate that we still have a valid path
	// Actual movement and waypoint advancement happens in FollowPathSegment()
}

void UAeonixPathFollowingComponent::SetMoveSegment(int32 SegmentStartIndex)
{
	Super::SetMoveSegment(SegmentStartIndex);

	// When a new move starts, ensure we have the correct Aeonix path reference
	if (!CurrentAeonixPath || !bInitializationComplete)
	{
		AActor* Owner = GetOwner();
		if (Owner)
		{
			UAeonixNavAgentComponent* NavAgent = Owner->FindComponentByClass<UAeonixNavAgentComponent>();
			if (!NavAgent)
			{
				if (AController* Controller = Cast<AController>(Owner))
				{
					if (APawn* Pawn = Controller->GetPawn())
					{
						NavAgent = Pawn->FindComponentByClass<UAeonixNavAgentComponent>();
					}
				}
			}

			if (NavAgent)
			{
				CurrentAeonixPath = &NavAgent->GetPath();
				bInitializationComplete = true;
				UE_LOG(LogTemp, Log, TEXT("AeonixPathFollowing: Path reference updated on move start, path has %d points, ready: %s"),
				CurrentAeonixPath->GetPathPoints().Num(),
				CurrentAeonixPath->IsReady() ? TEXT("YES") : TEXT("NO"));
			}
		}
	}

	CurrentWaypointIndex = SegmentStartIndex;
	UE_LOG(LogTemp, Log, TEXT("AeonixPathFollowing: SetMoveSegment called with index %d"), SegmentStartIndex);
}

bool UAeonixPathFollowingComponent::HasReached(const FVector& TestPoint, float AcceptanceRadiusOverride, bool bExactSpot) const
{
	const float UseRadius = (AcceptanceRadiusOverride <= 0.0f) ? FlightSettings.AcceptanceRadius : AcceptanceRadiusOverride;
	const FVector TargetLocation = GetTargetLocation();
	const float DistanceSq = FVector::DistSquared(TestPoint, TargetLocation);
	return DistanceSq <= FMath::Square(UseRadius);
}

void UAeonixPathFollowingComponent::FollowAeonixPath()
{
	if (!CurrentAeonixPath || !CurrentAeonixPath->IsReady())
	{
		UE_LOG(LogTemp, VeryVerbose, TEXT("AeonixPathFollowing: No path or path not ready"));
		return;
	}

	// Don't process if we're not actively moving
	if (GetStatus() != EPathFollowingStatus::Moving)
	{
		return;
	}

	AActor* Owner = GetOwner();
	APawn* OwnerPawn = Cast<APawn>(Owner);
	if (!OwnerPawn)
	{
		// Try getting pawn from AI controller
		if (AController* Controller = Cast<AController>(Owner))
		{
			OwnerPawn = Controller->GetPawn();
		}
	}
	if (!OwnerPawn)
	{
		UE_LOG(LogTemp, Warning, TEXT("AeonixPathFollowing: No pawn found"));
		return;
	}

	const FVector CurrentLocation = OwnerPawn->GetActorLocation();
	const FVector TargetLocation = GetTargetLocation();

	UE_LOG(LogTemp, Log, TEXT("AeonixPathFollowing: Following path - Current: %s, Target: %s, Waypoint: %d"),
		*CurrentLocation.ToString(), *TargetLocation.ToString(), CurrentWaypointIndex);

	// Check if we've reached the current waypoint
	if (HasReached(CurrentLocation))
	{
		UE_LOG(LogTemp, Log, TEXT("AeonixPathFollowing: Reached waypoint %d, advancing"), CurrentWaypointIndex);
		AdvanceToNextWaypoint();

		// Check if we've completed the path
		if (!IsValidWaypointIndex(CurrentWaypointIndex))
		{
			UE_LOG(LogTemp, Log, TEXT("AeonixPathFollowing: Path completed! Stopping movement."));

			// Stop the pawn's movement immediately
			if (UPawnMovementComponent* PawnMovement = OwnerPawn->GetMovementComponent())
			{
				PawnMovement->StopMovementImmediately();
			}

			// Notify that we've finished
			OnSegmentFinished();
			OnPathFinished(EPathFollowingResult::Success);
			return;
		}
	}
}

FVector UAeonixPathFollowingComponent::GetTargetLocation() const
{
	const FAeonixPathPoint* CurrentWaypoint = GetCurrentWaypoint();
	if (!CurrentWaypoint)
	{
		return FVector::ZeroVector;
	}

	return CurrentWaypoint->Position;
}

void UAeonixPathFollowingComponent::UpdateMovement(float DeltaTime)
{
	AActor* Owner = GetOwner();
	APawn* OwnerPawn = Cast<APawn>(Owner);
	if (!OwnerPawn)
	{
		if (AController* Controller = Cast<AController>(Owner))
		{
			OwnerPawn = Controller->GetPawn();
		}
	}

	if (!OwnerPawn)
	{
		return;
	}

	const FVector CurrentLocation = OwnerPawn->GetActorLocation();
	const FVector TargetLocation = GetTargetLocation();
	const FVector DirectionToTarget = (TargetLocation - CurrentLocation).GetSafeNormal();

	// Simple constant speed movement
	const FVector DesiredVelocity = DirectionToTarget * FlightSettings.MaxSpeed;

	// Check if we have our custom flying movement component
	if (UAeonixFlyingMovementComponent* FlyingMovement = OwnerPawn->FindComponentByClass<UAeonixFlyingMovementComponent>())
	{
		FlyingMovement->RequestDirectMove(DesiredVelocity, false);
		LastVelocity = DesiredVelocity;
	}
	else
	{
		// Fallback to standard movement component
		UPawnMovementComponent* PawnMovementComp = OwnerPawn->GetMovementComponent();
		if (!PawnMovementComp)
		{
			return;
		}

		PawnMovementComp->Velocity = DesiredVelocity;
		LastVelocity = PawnMovementComp->Velocity;
	}
}

void UAeonixPathFollowingComponent::UpdateRotation(float DeltaTime)
{
	AActor* Owner = GetOwner();
	APawn* OwnerPawn = Cast<APawn>(Owner);
	if (!OwnerPawn)
	{
		if (AController* Controller = Cast<AController>(Owner))
		{
			OwnerPawn = Controller->GetPawn();
		}
	}
	if (!OwnerPawn)
	{
		return;
	}

	const FVector CurrentVelocity = LastVelocity;
	if (CurrentVelocity.IsNearlyZero())
	{
		return;
	}

	// Simple rotation towards velocity direction
	const FVector VelocityDirection = CurrentVelocity.GetSafeNormal();
	const FRotator DesiredRotation = VelocityDirection.Rotation();

	// Smooth rotation interpolation
	const float TurnRateRadians = FMath::DegreesToRadians(FlightSettings.TurnRate);
	const FRotator NewRotation = FMath::RInterpTo(OwnerPawn->GetActorRotation(), DesiredRotation, DeltaTime, TurnRateRadians);

	OwnerPawn->SetActorRotation(NewRotation);
}

const FAeonixPathPoint* UAeonixPathFollowingComponent::GetCurrentWaypoint() const
{
	if (!IsValidWaypointIndex(CurrentWaypointIndex))
	{
		return nullptr;
	}

	const TArray<FAeonixPathPoint>& PathPoints = CurrentAeonixPath->GetPathPoints();
	return &PathPoints[CurrentWaypointIndex];
}

const FAeonixPathPoint* UAeonixPathFollowingComponent::GetNextWaypoint() const
{
	const int32 NextIndex = CurrentWaypointIndex + 1;
	if (!IsValidWaypointIndex(NextIndex))
	{
		return nullptr;
	}

	const TArray<FAeonixPathPoint>& PathPoints = CurrentAeonixPath->GetPathPoints();
	return &PathPoints[NextIndex];
}

bool UAeonixPathFollowingComponent::IsValidWaypointIndex(int32 Index) const
{
	if (!CurrentAeonixPath || !CurrentAeonixPath->IsReady())
	{
		return false;
	}

	const TArray<FAeonixPathPoint>& PathPoints = CurrentAeonixPath->GetPathPoints();
	return Index >= 0 && Index < PathPoints.Num();
}

void UAeonixPathFollowingComponent::AdvanceToNextWaypoint()
{
	CurrentWaypointIndex++;
}

void UAeonixPathFollowingComponent::TryInitializeNavigation()
{
	if (bInitializationComplete)
	{
		return;
	}

	InitializationRetryCount++;

	UE_LOG(LogTemp, Log, TEXT("AeonixPathFollowingComponent: Initialization attempt %d/%d"),
		InitializationRetryCount, MAX_INITIALIZATION_RETRIES);

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		UE_LOG(LogTemp, Error, TEXT("AeonixPathFollowingComponent: No owner found during initialization"));
		return;
	}

	// Try to find the nav agent component
	UAeonixNavAgentComponent* NavAgent = Owner->FindComponentByClass<UAeonixNavAgentComponent>();
	if (!NavAgent)
	{
		// Try to find it on the pawn if we're on a controller
		if (AController* Controller = Cast<AController>(Owner))
		{
			if (APawn* Pawn = Controller->GetPawn())
			{
				NavAgent = Pawn->FindComponentByClass<UAeonixNavAgentComponent>();
			}
		}
	}

	if (NavAgent)
	{
		// Set the path reference
		CurrentAeonixPath = &NavAgent->GetPath();

		// Check if we can validate navigation (volume registration)
		if (IsValidForNavigation())
		{
			bInitializationComplete = true;
			UE_LOG(LogTemp, Log, TEXT("AeonixPathFollowingComponent: Successfully initialized navigation on %s"),
				*Owner->GetName());
			return;
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("AeonixPathFollowingComponent: NavAgent found but not in volume yet (attempt %d)"),
				InitializationRetryCount);
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("AeonixPathFollowingComponent: No NavAgent component found on %s (attempt %d)"),
			*Owner->GetName(), InitializationRetryCount);
	}

	if (InitializationRetryCount >= MAX_INITIALIZATION_RETRIES)
	{
		UE_LOG(LogTemp, Warning, TEXT("AeonixPathFollowingComponent: Proceeding without NavAgent after %d attempts"),
			MAX_INITIALIZATION_RETRIES);
		bInitializationComplete = true; // Proceed anyway - path will be set when move starts
	}
}

bool UAeonixPathFollowingComponent::IsValidForNavigation() const
{
	// Check if we have a world and subsystem
	UWorld* World = GetWorld();
	if (!World)
	{
		return false;
	}

	// Try to get the Aeonix subsystem
	if (UAeonixSubsystem* AeonixSubsystem = World->GetSubsystem<UAeonixSubsystem>())
	{
		AActor* Owner = GetOwner();
		if (!Owner)
		{
			return false;
		}

		// Get the pawn location to test
		FVector TestLocation = Owner->GetActorLocation();

		// If we're on a controller, get the pawn's location
		if (AController* Controller = Cast<AController>(Owner))
		{
			if (APawn* Pawn = Controller->GetPawn())
			{
				TestLocation = Pawn->GetActorLocation();
			}
		}

		// Try to find nav agent component to test volume registration
		UAeonixNavAgentComponent* NavAgent = Owner->FindComponentByClass<UAeonixNavAgentComponent>();
		if (!NavAgent)
		{
			// Try to find it on the pawn if we're on a controller
			if (AController* Controller = Cast<AController>(Owner))
			{
				if (APawn* Pawn = Controller->GetPawn())
				{
					NavAgent = Pawn->FindComponentByClass<UAeonixNavAgentComponent>();
				}
			}
		}

		if (NavAgent)
		{
			// Test if the subsystem can find a volume for this agent
			// This is a simplified check - the real validation happens in the subsystem
			UE_LOG(LogTemp, Log, TEXT("AeonixPathFollowingComponent: Testing navigation validity at location %s"),
				*TestLocation.ToString());
			return true; // For now, assume it's valid if we have the components
		}
	}

	return false;
}

