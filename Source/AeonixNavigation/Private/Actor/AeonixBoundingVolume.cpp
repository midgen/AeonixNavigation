

#include "Actor/AeonixBoundingVolume.h"
#include "Subsystem/AeonixSubsystem.h"
#include "Subsystem/AeonixCollisionSubsystem.h"
#include "AeonixNavigation.h"
#include "Debug/AeonixDebugDrawManager.h"

#include "Components/BrushComponent.h"
#include "Components/LineBatchComponent.h"
#include "Engine/CollisionProfile.h"
#include "GameFramework/PlayerController.h"
#include "DrawDebugHelpers.h"

#include <chrono>

using namespace std::chrono;

AAeonixBoundingVolume::AAeonixBoundingVolume(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	GetBrushComponent()->Mobility = EComponentMobility::Static;
	BrushColor = FColor(255, 255, 255, 255);
	bColored = true;
}

// Regenerates the SVO Navigation Data
bool AAeonixBoundingVolume::Generate()
{
	// Reset nav data
	NavigationData.ResetForGeneration();
	// Update parameters
	NavigationData.UpdateGenerationParameters(GenerationParameters);

	if (!CollisionQueryInterface.GetInterface())
	{
		UAeonixCollisionSubsystem* CollisionSubsystem = GetWorld()->GetSubsystem<UAeonixCollisionSubsystem>();
		CollisionQueryInterface = CollisionSubsystem;
		UE_LOG(LogAeonixNavigation, Error, TEXT("No AeonixSubsystem with a valid CollisionQueryInterface found"));
	}

#if WITH_EDITOR

	// If we're running the game, use the first player controller position for debugging
	APlayerController* pc = GetWorld()->GetFirstPlayerController();
	if (pc)
	{
		NavigationData.SetDebugPosition(pc->GetPawn()->GetActorLocation());
	}
	// otherwise, use the viewport camera location if we're just in the editor
	else if (GetWorld()->ViewLocationsRenderedLastFrame.Num() > 0)
	{
		NavigationData.SetDebugPosition(GetWorld()->ViewLocationsRenderedLastFrame[0]);
	}

	// Clear only octree debug visualization using the debug manager (doesn't affect other systems)
	if (UAeonixDebugDrawManager* DebugManager = GetWorld()->GetSubsystem<UAeonixDebugDrawManager>())
	{
		DebugManager->Clear(EAeonixDebugCategory::Octree);
	}

	// Setup timing
	milliseconds startMs = duration_cast<milliseconds>(
		system_clock::now().time_since_epoch());

#endif // WITH_EDITOR

	UpdateBounds();
	NavigationData.Generate(*GetWorld(), *CollisionQueryInterface.GetInterface(), *this);

#if WITH_EDITOR

	int32 BuildTime = (duration_cast<milliseconds>(
						   system_clock::now().time_since_epoch()) -
					   startMs)
						  .count();

	int32 TotalNodes = 0;

	for (int i = 0; i < NavigationData.OctreeData.GetNumLayers(); i++)
	{
		TotalNodes += NavigationData.OctreeData.Layers[i].Num();
	}

	int32 TotalBytes = sizeof(AeonixNode) * TotalNodes;
	TotalBytes += sizeof(AeonixLeafNode) * NavigationData.OctreeData.LeafNodes.Num();

	UE_LOG(LogAeonixNavigation, Display, TEXT("Generation Time : %d"), BuildTime);
	UE_LOG(LogAeonixNavigation, Display, TEXT("Total Layers-Nodes : %d-%d"), NavigationData.OctreeData.GetNumLayers(), TotalNodes);
	UE_LOG(LogAeonixNavigation, Display, TEXT("Total Leaf Nodes : %d"), NavigationData.OctreeData.LeafNodes.Num());
	UE_LOG(LogAeonixNavigation, Display, TEXT("Total Size (bytes): %d"), TotalBytes);
#endif

	// Mark volume as ready for navigation after successful generation
	bIsReadyForNavigation = true;

	return true;
}

bool AAeonixBoundingVolume::HasData() const
{
	return NavigationData.OctreeData.LeafNodes.Num() > 0;	
}

void AAeonixBoundingVolume::UpdateBounds()
{
	FVector Origin, Extent;
	FBox Bounds = GetComponentsBoundingBox(true);
	Bounds.GetCenterAndExtents(Origin, Extent);

	NavigationData.SetExtents(Origin, Extent);
}

void AAeonixBoundingVolume::AeonixDrawDebugString(const FVector& Position, const FString& String, const FColor& Color) const
{
	if (UAeonixDebugDrawManager* DebugManager = GetWorld()->GetSubsystem<UAeonixDebugDrawManager>())
	{
		DebugManager->AddString(Position, String, Color, 1.f, EAeonixDebugCategory::Octree);
	}
}

void AAeonixBoundingVolume::AeonixDrawDebugBox(const FVector& Position, const float Size, const FColor& Color) const
{
	if (UAeonixDebugDrawManager* DebugManager = GetWorld()->GetSubsystem<UAeonixDebugDrawManager>())
	{
		DebugManager->AddBox(Position, FVector(Size), FQuat::Identity, Color, EAeonixDebugCategory::Octree);
	}
}

void AAeonixBoundingVolume::AeonixDrawDebugLine(const FVector& Start, const FVector& End, const FColor& Color, float Thickness) const
{
	if (UAeonixDebugDrawManager* DebugManager = GetWorld()->GetSubsystem<UAeonixDebugDrawManager>())
	{
		DebugManager->AddLine(Start, End, Color, Thickness, EAeonixDebugCategory::Octree);
	}
}

void AAeonixBoundingVolume::AeonixDrawDebugDirectionalArrow(const FVector& Start, const FVector& End, const FColor& Color, float ArrowSize) const
{
	if (UAeonixDebugDrawManager* DebugManager = GetWorld()->GetSubsystem<UAeonixDebugDrawManager>())
	{
		DebugManager->AddArrow(Start, End, ArrowSize, Color, 0.0f, EAeonixDebugCategory::Octree);
	}
}

void AAeonixBoundingVolume::ClearData()
{
	NavigationData.ResetForGeneration();

	// Clear only octree debug visualization using the debug manager (doesn't affect other systems)
	if (UAeonixDebugDrawManager* DebugManager = GetWorld()->GetSubsystem<UAeonixDebugDrawManager>())
	{
		DebugManager->Clear(EAeonixDebugCategory::Octree);
	}
}


void AAeonixBoundingVolume::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	AeonixSubsystemInterface = GetWorld()->GetSubsystem<UAeonixSubsystem>();
	if (!AeonixSubsystemInterface.GetInterface())
	{
		UE_LOG(LogAeonixNavigation, Error, TEXT("No AeonixSubsystem with a valid AeonixInterface found"));
	}
	else
	{
		AeonixSubsystemInterface->RegisterVolume(this, EAeonixMassEntityFlag::Disabled);
	}
}

void AAeonixBoundingVolume::Destroyed()
{
	if (!AeonixSubsystemInterface.GetInterface())
	{
		UE_LOG(LogAeonixNavigation, Error, TEXT("No AeonixSubsystem with a valid AeonixInterface found"));
	}
	else
	{
		AeonixSubsystemInterface->UnRegisterVolume(this, EAeonixMassEntityFlag::Disabled);
	}
	
	Super::Destroyed();
}

void AAeonixBoundingVolume::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if (GenerationParameters.GenerationStrategy == ESVOGenerationStrategy::UseBaked)
	{
		Ar << NavigationData.OctreeData;

		// Mark as ready if we successfully loaded baked data
		if (Ar.IsLoading() && NavigationData.OctreeData.LeafNodes.Num() > 0)
		{
			bIsReadyForNavigation = true;
			UE_LOG(LogAeonixNavigation, Log, TEXT("Baked navigation data loaded - %d leaf nodes, marked ready for navigation"), NavigationData.OctreeData.LeafNodes.Num());
		}
	}
}

void AAeonixBoundingVolume::BeginPlay()
{
	AeonixSubsystemInterface = GetWorld()->GetSubsystem<UAeonixSubsystem>();
	if (!AeonixSubsystemInterface.GetInterface())
	{
		UE_LOG(LogAeonixNavigation, Error, TEXT("No AeonixSubsystem with a valid AeonixInterface found"));
	}
	else
	{
		AeonixSubsystemInterface->RegisterVolume(this, EAeonixMassEntityFlag::Disabled);
	}

	CollisionQueryInterface = GetWorld()->GetSubsystem<UAeonixCollisionSubsystem>();
	if (!CollisionQueryInterface.GetInterface())
	{
		UE_LOG(LogAeonixNavigation, Error, TEXT("No AeonixSubsystem with a valid CollisionQueryInterface found"));
	}

	if (!bIsReadyForNavigation && GenerationParameters.GenerationStrategy == ESVOGenerationStrategy::GenerateOnBeginPlay)
	{
		Generate();
	}
	else
	{
		UpdateBounds();
	}

	bIsReadyForNavigation = true;
}

void AAeonixBoundingVolume::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (!AeonixSubsystemInterface.GetInterface())
	{
		UE_LOG(LogAeonixNavigation, Error, TEXT("No AeonixSubsystem with a valid AeonixInterface found"));
	}
	else
	{
		AeonixSubsystemInterface->UnRegisterVolume(this, EAeonixMassEntityFlag::Disabled);
	}

	Super::EndPlay(EndPlayReason);
}
