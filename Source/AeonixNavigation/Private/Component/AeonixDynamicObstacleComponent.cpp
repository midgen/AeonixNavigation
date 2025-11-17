// Copyright Chris Kang. All Rights Reserved.

#include "Component/AeonixDynamicObstacleComponent.h"
#include "Actor/AeonixBoundingVolume.h"
#include "Subsystem/AeonixSubsystem.h"
#include "Data/AeonixGenerationParameters.h"
#include "AeonixNavigation.h"

#include "GameFramework/Actor.h"
#include "EngineUtils.h"

UAeonixDynamicObstacleComponent::UAeonixDynamicObstacleComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Component does not tick - subsystem handles all obstacle ticking
	PrimaryComponentTick.bCanEverTick = false;
}

void UAeonixDynamicObstacleComponent::OnRegister()
{
	Super::OnRegister();

	// Register with subsystem (works in editor and at runtime)
	RegisterWithSubsystem();
}

void UAeonixDynamicObstacleComponent::OnUnregister()
{
	// Unregister from subsystem
	UnregisterFromSubsystem();

	Super::OnUnregister();
}

void UAeonixDynamicObstacleComponent::BeginPlay()
{
	Super::BeginPlay();

	// Make sure we're registered (OnRegister should have already done this, but be safe)
	if (!bRegisteredWithSubsystem)
	{
		RegisterWithSubsystem();
	}
}

void UAeonixDynamicObstacleComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Unregister from subsystem
	UnregisterFromSubsystem();

	Super::EndPlay(EndPlayReason);
}

void UAeonixDynamicObstacleComponent::RegisterWithSubsystem()
{
	if (bRegisteredWithSubsystem)
	{
		return; // Already registered
	}

	// Get reference to the Aeonix subsystem
	if (UWorld* World = GetWorld())
	{
		AeonixSubsystem = World->GetSubsystem<UAeonixSubsystem>();
		if (!AeonixSubsystem.GetInterface())
		{
			UE_LOG(LogAeonixNavigation, Verbose, TEXT("DynamicObstacle %s: No AeonixSubsystem found"), *GetName());
			return;
		}

		// Initialize the tracked transform to current transform
		if (AActor* Owner = GetOwner())
		{
			LastTrackedTransform = Owner->GetActorTransform();
		}

		// Find which bounding volume and dynamic regions we're currently inside
		UpdateCurrentRegions();

		// Register with the subsystem
		AeonixSubsystem->RegisterDynamicObstacle(this);
		bRegisteredWithSubsystem = true;

		UE_LOG(LogAeonixNavigation, Verbose, TEXT("DynamicObstacle %s: Registered with subsystem, inside %d regions"),
			*GetName(), CurrentDynamicRegionIds.Num());
	}
}

void UAeonixDynamicObstacleComponent::UnregisterFromSubsystem()
{
	if (!bRegisteredWithSubsystem)
	{
		return; // Not registered
	}

	// Unregister from subsystem
	if (AeonixSubsystem.GetInterface())
	{
		AeonixSubsystem->UnRegisterDynamicObstacle(this);
		bRegisteredWithSubsystem = false;

		UE_LOG(LogAeonixNavigation, Verbose, TEXT("DynamicObstacle %s: Unregistered from subsystem"), *GetName());
	}
}

#if WITH_EDITOR
void UAeonixDynamicObstacleComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Update tracked transform when properties change to prevent immediate triggers
	// The subsystem tick will detect actual movement in editor and trigger regeneration
	if (AActor* Owner = GetOwner())
	{
		LastTrackedTransform = Owner->GetActorTransform();
	}

	// Update current regions to reflect any changes
	UpdateCurrentRegions();
}
#endif

void UAeonixDynamicObstacleComponent::TriggerNavigationRegen()
{
	if (!bEnableNavigationRegen)
	{
		UE_LOG(LogAeonixNavigation, Verbose, TEXT("DynamicObstacle %s: Manual trigger ignored (disabled)"), *GetName());
		return;
	}

	// Update regions first (in case we're calling this from editor before BeginPlay)
	UpdateCurrentRegions();

	if (!CurrentBoundingVolume.IsValid())
	{
		UE_LOG(LogAeonixNavigation, Warning, TEXT("DynamicObstacle %s: Manual trigger ignored (not inside any bounding volume)"), *GetName());
		return;
	}

	if (CurrentDynamicRegionIds.Num() == 0)
	{
		UE_LOG(LogAeonixNavigation, Warning, TEXT("DynamicObstacle %s: Manual trigger ignored (not inside any dynamic regions)"), *GetName());
		return;
	}

	// Request regeneration for all regions we're currently inside
	for (const FGuid& RegionId : CurrentDynamicRegionIds)
	{
		CurrentBoundingVolume->RequestDynamicRegionRegen(RegionId);
	}

	// In editor, immediately process dirty regions for instant feedback
	// At runtime, the subsystem tick will process them with throttling
#if WITH_EDITOR
	if (!GetWorld()->IsGameWorld())
	{
		CurrentBoundingVolume->TryProcessDirtyRegions();
	}
#endif

	UE_LOG(LogAeonixNavigation, Display, TEXT("DynamicObstacle %s: Manually triggered regen for %d regions"),
		*GetName(), CurrentDynamicRegionIds.Num());

	// Update tracked transform after regeneration
	if (AActor* Owner = GetOwner())
	{
		LastTrackedTransform = Owner->GetActorTransform();
	}
}

bool UAeonixDynamicObstacleComponent::CheckForTransformChange(TSet<FGuid>& OutOldRegionIds, TSet<FGuid>& OutNewRegionIds)
{
	if (!bEnableNavigationRegen)
	{
		return false;
	}

	// Store old region IDs
	OutOldRegionIds = CurrentDynamicRegionIds;

	// Check if we crossed region boundaries
	const bool bRegionsChanged = UpdateCurrentRegions();

	// Get new region IDs
	OutNewRegionIds = CurrentDynamicRegionIds;

	// Check position threshold
	const bool bPositionChanged = HasPositionChangedBeyondThreshold();

	// Check rotation threshold
	const bool bRotationChanged = HasRotationChangedBeyondThreshold();

	// Return true if any threshold was exceeded or regions changed
	return bRegionsChanged || bPositionChanged || bRotationChanged;
}

void UAeonixDynamicObstacleComponent::UpdateTrackedTransform()
{
	LastTrackedTransform = GetOwner()->GetActorTransform();
}

bool UAeonixDynamicObstacleComponent::UpdateCurrentRegions()
{
	if (!GetOwner())
	{
		return false;
	}

	const FVector CurrentPosition = GetOwner()->GetActorLocation();
	TSet<FGuid> NewRegionIds;
	AAeonixBoundingVolume* NewBoundingVolume = nullptr;

	// Find which bounding volume we're inside
	if (UWorld* World = GetWorld())
	{
		for (TActorIterator<AAeonixBoundingVolume> It(World); It; ++It)
		{
			AAeonixBoundingVolume* BoundingVolume = *It;
			if (BoundingVolume && BoundingVolume->EncompassesPoint(CurrentPosition))
			{
				NewBoundingVolume = BoundingVolume;

				// Check which dynamic regions within this volume we're inside
				const FAeonixGenerationParameters& Params = BoundingVolume->GenerationParameters;
				for (const auto& RegionPair : Params.DynamicRegionBoxes)
				{
					const FGuid& RegionId = RegionPair.Key;
					const FBox& RegionBox = RegionPair.Value;

					if (RegionBox.IsInsideOrOn(CurrentPosition))
					{
						NewRegionIds.Add(RegionId);
					}
				}

				// We only support being inside one bounding volume at a time
				break;
			}
		}
	}

	// Check if regions changed
	const bool bRegionsChanged = !(NewRegionIds.Num() == CurrentDynamicRegionIds.Num() && NewRegionIds.Includes(CurrentDynamicRegionIds));

	// Update current state
	CurrentBoundingVolume = NewBoundingVolume;
	CurrentDynamicRegionIds = NewRegionIds;

	if (bRegionsChanged)
	{
		UE_LOG(LogAeonixNavigation, Verbose, TEXT("DynamicObstacle %s: Regions changed, now inside %d regions"),
			*GetName(), CurrentDynamicRegionIds.Num());
	}

	return bRegionsChanged;
}

bool UAeonixDynamicObstacleComponent::HasPositionChangedBeyondThreshold() const
{
	if (!GetOwner())
	{
		return false;
	}

	const FVector CurrentPosition = GetOwner()->GetActorLocation();
	const FVector LastPosition = LastTrackedTransform.GetLocation();
	const float DistanceSq = FVector::DistSquared(CurrentPosition, LastPosition);
	const float ThresholdSq = PositionThreshold * PositionThreshold;

	return DistanceSq > ThresholdSq;
}

bool UAeonixDynamicObstacleComponent::HasRotationChangedBeyondThreshold() const
{
	if (!GetOwner())
	{
		return false;
	}

	const FQuat CurrentRotation = GetOwner()->GetActorQuat();
	const FQuat LastRotation = LastTrackedTransform.GetRotation();

	// Calculate angle between quaternions using the dot product operator
	const float DotProduct = FMath::Abs(CurrentRotation | LastRotation);
	const float AngleDegrees = FMath::RadiansToDegrees(2.0f * FMath::Acos(FMath::Min(DotProduct, 1.0f)));

	return AngleDegrees > RotationThreshold;
}
