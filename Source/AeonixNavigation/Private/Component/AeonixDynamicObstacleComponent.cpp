// Copyright Chris Kang. All Rights Reserved.

#include "Component/AeonixDynamicObstacleComponent.h"
#include "Actor/AeonixBoundingVolume.h"
#include "Subsystem/AeonixSubsystem.h"
#include "AeonixNavigation.h"

#include "GameFramework/Actor.h"

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

	// Reset state from editor world - PIE creates new world with new actor instances
	CurrentBoundingVolume = nullptr;
	CurrentDynamicRegionIds.Empty();
	bRegisteredWithSubsystem = false;

	RegisterWithSubsystem();
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
		UAeonixSubsystem* Subsystem = World->GetSubsystem<UAeonixSubsystem>();
		if (!Subsystem)
		{
			UE_LOG(LogAeonixNavigation, Verbose, TEXT("DynamicObstacle %s: No AeonixSubsystem found"), *GetName());
			return;
		}

		// Register with the subsystem - it handles all transform tracking and region detection
		Subsystem->RegisterDynamicObstacle(this);
		bRegisteredWithSubsystem = true;

		UE_LOG(LogAeonixNavigation, Verbose, TEXT("DynamicObstacle %s: Registered with subsystem"), *GetName());
	}
}

void UAeonixDynamicObstacleComponent::UnregisterFromSubsystem()
{
	if (!bRegisteredWithSubsystem)
	{
		return; // Not registered
	}

	// Unregister from subsystem
	UWorld* World = GetWorld();
	if (!World)
	{
		bRegisteredWithSubsystem = false;
		return;
	}

	UAeonixSubsystem* Subsystem = World->GetSubsystem<UAeonixSubsystem>();
	if (!Subsystem)
	{
		bRegisteredWithSubsystem = false;
		return;
	}

	Subsystem->UnRegisterDynamicObstacle(this);
	bRegisteredWithSubsystem = false;

	UE_LOG(LogAeonixNavigation, Verbose, TEXT("DynamicObstacle %s: Unregistered from subsystem"), *GetName());
}

#if WITH_EDITOR
void UAeonixDynamicObstacleComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Properties changed - the subsystem will detect movement in its next tick
}
#endif

void UAeonixDynamicObstacleComponent::TriggerNavigationRegen()
{
	if (!bEnableNavigationRegen)
	{
		UE_LOG(LogAeonixNavigation, Verbose, TEXT("DynamicObstacle %s: Manual trigger ignored (disabled)"), *GetName());
		return;
	}

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
}
