// Copyright Chris Kang. All Rights Reserved.

#include "Actor/AeonixDebugFilterVolume.h"
#include "Actor/AeonixBoundingVolume.h"
#include "EngineUtils.h"

AAeonixDebugFilterVolume::AAeonixDebugFilterVolume(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	GetBrushComponent()->Mobility = EComponentMobility::Static;
	BrushColor = FColor::Cyan;
	bColored = true;
}

void AAeonixDebugFilterVolume::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);
	RegisterWithBoundingVolumes();
}

void AAeonixDebugFilterVolume::BeginPlay()
{
	Super::BeginPlay();
	RegisterWithBoundingVolumes();
}

void AAeonixDebugFilterVolume::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnregisterFromBoundingVolumes();
	Super::EndPlay(EndPlayReason);
}

void AAeonixDebugFilterVolume::Destroyed()
{
	UnregisterFromBoundingVolumes();
	Super::Destroyed();
}

#if WITH_EDITOR
void AAeonixDebugFilterVolume::PostEditMove(bool bFinished)
{
	Super::PostEditMove(bFinished);

	if (bFinished)
	{
		// Re-register when movement is finished
		UnregisterFromBoundingVolumes();
		RegisterWithBoundingVolumes();
	}
}

void AAeonixDebugFilterVolume::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Re-register when properties change (like brush being resized)
	UnregisterFromBoundingVolumes();
	RegisterWithBoundingVolumes();
}
#endif

void AAeonixDebugFilterVolume::RegisterWithBoundingVolumes()
{
	if (UWorld* World = GetWorld())
	{
		// Find all bounding volumes and register with ones we're inside
		for (TActorIterator<AAeonixBoundingVolume> It(World); It; ++It)
		{
			AAeonixBoundingVolume* BoundingVolume = *It;
			if (BoundingVolume && BoundingVolume->EncompassesPoint(GetActorLocation()))
			{
				// Get the bounding box of this filter volume
				FBox FilterBox = GetComponentsBoundingBox(true);
				BoundingVolume->SetDebugFilterBox(FilterBox);
			}
		}
	}
}

void AAeonixDebugFilterVolume::UnregisterFromBoundingVolumes()
{
	if (UWorld* World = GetWorld())
	{
		// Find all bounding volumes and clear debug filter from ones we registered with
		for (TActorIterator<AAeonixBoundingVolume> It(World); It; ++It)
		{
			AAeonixBoundingVolume* BoundingVolume = *It;
			if (BoundingVolume && BoundingVolume->EncompassesPoint(GetActorLocation()))
			{
				BoundingVolume->ClearDebugFilterBox();
			}
		}
	}
}
