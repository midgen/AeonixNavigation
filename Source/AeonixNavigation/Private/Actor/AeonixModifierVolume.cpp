// Copyright Chris Kang. All Rights Reserved.

#include "Actor/AeonixModifierVolume.h"
#include "Actor/AeonixBoundingVolume.h"
#include "Subsystem/AeonixSubsystem.h"
#include "Components/BrushComponent.h"
#include "AeonixNavigation.h"

AAeonixModifierVolume::AAeonixModifierVolume(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	GetBrushComponent()->Mobility = EComponentMobility::Static;
	GetBrushComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BrushColor = FColor::Cyan;
	bColored = true;

	// Generate a unique ID for this dynamic region if not already set
	if (!DynamicRegionId.IsValid())
	{
		DynamicRegionId = FGuid::NewGuid();
	}
}

void AAeonixModifierVolume::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RegisterWithBoundingVolumes();
}

void AAeonixModifierVolume::BeginPlay()
{
	Super::BeginPlay();
	RegisterWithBoundingVolumes();
}

void AAeonixModifierVolume::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnregisterFromBoundingVolumes();
	Super::EndPlay(EndPlayReason);
}

void AAeonixModifierVolume::Destroyed()
{
	UnregisterFromBoundingVolumes();
	Super::Destroyed();
}

#if WITH_EDITOR
void AAeonixModifierVolume::PostEditMove(bool bFinished)
{
	Super::PostEditMove(bFinished);

	if (bFinished)
	{
		// Re-register when movement is finished
		UnregisterFromBoundingVolumes();
		RegisterWithBoundingVolumes();
	}
}

void AAeonixModifierVolume::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Re-register when properties change (like brush being resized)
	UnregisterFromBoundingVolumes();
	RegisterWithBoundingVolumes();
}
#endif

void AAeonixModifierVolume::RegisterWithBoundingVolumes()
{
	if (UWorld* World = GetWorld())
	{
		UE_LOG(LogAeonixNavigation, Verbose, TEXT("ModifierVolume %s: Registering with subsystem - ModifierTypes=%d"),
			*GetName(), ModifierTypes);

		// Simply register with the subsystem - it will handle spatial relationships
		if (UAeonixSubsystem* Subsystem = World->GetSubsystem<UAeonixSubsystem>())
		{
			Subsystem->RegisterModifierVolume(this);
		}
	}
}

void AAeonixModifierVolume::UnregisterFromBoundingVolumes()
{
	if (UWorld* World = GetWorld())
	{
		UE_LOG(LogAeonixNavigation, Verbose, TEXT("ModifierVolume %s: Unregistering from subsystem"),
			*GetName());

		// Simply unregister from the subsystem - it will handle cleanup
		if (UAeonixSubsystem* Subsystem = World->GetSubsystem<UAeonixSubsystem>())
		{
			Subsystem->UnRegisterModifierVolume(this);
		}
	}
}
