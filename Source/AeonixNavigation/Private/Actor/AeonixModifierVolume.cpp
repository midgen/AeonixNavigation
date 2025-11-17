// Copyright Chris Kang. All Rights Reserved.

#include "Actor/AeonixModifierVolume.h"
#include "Actor/AeonixBoundingVolume.h"
#include "Components/BrushComponent.h"
#include "EngineUtils.h"
#include "AeonixNavigation.h"

AAeonixModifierVolume::AAeonixModifierVolume(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	GetBrushComponent()->Mobility = EComponentMobility::Static;
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
		UE_LOG(LogAeonixNavigation, Verbose, TEXT("ModifierVolume %s: RegisterWithBoundingVolumes - ModifierTypes=%d"),
			*GetName(), ModifierTypes);

		// Find all bounding volumes and register with ones we're inside
		for (TActorIterator<AAeonixBoundingVolume> It(World); It; ++It)
		{
			AAeonixBoundingVolume* BoundingVolume = *It;
			if (BoundingVolume && BoundingVolume->EncompassesPoint(GetActorLocation()))
			{
				UE_LOG(LogAeonixNavigation, Verbose, TEXT("ModifierVolume %s: Inside bounding volume %s"),
					*GetName(), *BoundingVolume->GetName());

				// Get the bounding box of this modifier volume
				FBox ModifierBox = GetComponentsBoundingBox(true);

				// Apply debug filter if the DebugFilter flag is set
				if (ModifierTypes & static_cast<int32>(EAeonixModifierType::DebugFilter))
				{
					BoundingVolume->SetDebugFilterBox(ModifierBox);
					UE_LOG(LogAeonixNavigation, Verbose, TEXT("ModifierVolume %s: Registered DebugFilter"), *GetName());
				}

				// Register as dynamic region if the DynamicRegion flag is set
				if (ModifierTypes & static_cast<int32>(EAeonixModifierType::DynamicRegion))
				{
					BoundingVolume->AddDynamicRegion(DynamicRegionId, ModifierBox);
					UE_LOG(LogAeonixNavigation, Display, TEXT("ModifierVolume %s: Registered DynamicRegion (ID: %s) with %s"),
						*GetName(), *DynamicRegionId.ToString(), *BoundingVolume->GetName());
				}
			}
		}
	}
}

void AAeonixModifierVolume::UnregisterFromBoundingVolumes()
{
	if (UWorld* World = GetWorld())
	{
		// When unregistering, we need to rebuild the modifier lists since we support multiple modifiers
		// This ensures other modifier volumes remain registered
		for (TActorIterator<AAeonixBoundingVolume> It(World); It; ++It)
		{
			AAeonixBoundingVolume* BoundingVolume = *It;
			if (BoundingVolume && BoundingVolume->EncompassesPoint(GetActorLocation()))
			{
				// Clear all modifiers for this bounding volume
				BoundingVolume->ClearDebugFilterBox();
				BoundingVolume->ClearDynamicRegions();

				// Re-register all OTHER modifier volumes
				for (TActorIterator<AAeonixModifierVolume> ModIt(World); ModIt; ++ModIt)
				{
					AAeonixModifierVolume* OtherModifier = *ModIt;
					// Skip this modifier volume (the one being unregistered)
					if (OtherModifier && OtherModifier != this &&
						BoundingVolume->EncompassesPoint(OtherModifier->GetActorLocation()))
					{
						FBox ModifierBox = OtherModifier->GetComponentsBoundingBox(true);

						// Re-apply debug filter if enabled
						if (OtherModifier->ModifierTypes & static_cast<int32>(EAeonixModifierType::DebugFilter))
						{
							BoundingVolume->SetDebugFilterBox(ModifierBox);
						}

						// Re-register as dynamic region if enabled
						if (OtherModifier->ModifierTypes & static_cast<int32>(EAeonixModifierType::DynamicRegion))
						{
							BoundingVolume->AddDynamicRegion(OtherModifier->DynamicRegionId, ModifierBox);
						}
					}
				}
			}
		}
	}
}
