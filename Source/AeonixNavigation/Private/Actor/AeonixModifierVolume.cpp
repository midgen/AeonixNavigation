// Copyright Chris Kang. All Rights Reserved.

#include "Actor/AeonixModifierVolume.h"
#include "Actor/AeonixBoundingVolume.h"
#include "Components/BrushComponent.h"
#include "EngineUtils.h"

AAeonixModifierVolume::AAeonixModifierVolume(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	GetBrushComponent()->Mobility = EComponentMobility::Static;
	BrushColor = FColor::Cyan;
	bColored = true;
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
		// Find all bounding volumes and register with ones we're inside
		for (TActorIterator<AAeonixBoundingVolume> It(World); It; ++It)
		{
			AAeonixBoundingVolume* BoundingVolume = *It;
			if (BoundingVolume && BoundingVolume->EncompassesPoint(GetActorLocation()))
			{
				// Only apply debug filter if the DebugFilter flag is set
				if (ModifierTypes & static_cast<int32>(EAeonixModifierType::DebugFilter))
				{
					// Get the bounding box of this filter volume
					FBox FilterBox = GetComponentsBoundingBox(true);
					BoundingVolume->SetDebugFilterBox(FilterBox);
				}
			}
		}
	}
}

void AAeonixModifierVolume::UnregisterFromBoundingVolumes()
{
	if (UWorld* World = GetWorld())
	{
		// Find all bounding volumes and clear debug filter from ones we registered with
		for (TActorIterator<AAeonixBoundingVolume> It(World); It; ++It)
		{
			AAeonixBoundingVolume* BoundingVolume = *It;
			if (BoundingVolume && BoundingVolume->EncompassesPoint(GetActorLocation()))
			{
				// Only clear if we were applying the debug filter
				if (ModifierTypes & static_cast<int32>(EAeonixModifierType::DebugFilter))
				{
					BoundingVolume->ClearDebugFilterBox();
				}
			}
		}
	}
}
