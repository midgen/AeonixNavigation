

#include <AeonixNavigation/Public/Actor/AeonixBoundingVolume.h>
#include <AeonixNavigation/Public/Subsystem/AeonixSubsystem.h>
#include <AeonixNavigation/Public/Subsystem/AeonixCollisionSubsystem.h>
#include <AeonixNavigation/Public/AeonixNavigation.h>

#include <Runtime/Engine/Classes/Components/BrushComponent.h>
#include <Runtime/Engine/Classes/Components/LineBatchComponent.h>
#include <Runtime/Engine/Classes/Engine/CollisionProfile.h>
#include <Runtime/Engine/Classes/GameFramework/PlayerController.h>
#include <Runtime/Engine/Public/DrawDebugHelpers.h>

#include <chrono>

using namespace std::chrono;

AAeonixBoundingVolume::AAeonixBoundingVolume(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	GetBrushComponent()->Mobility = EComponentMobility::Static;
	BrushColor = FColor(255, 255, 255, 255);
	bColored = true;
	UpdateBounds();
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
		UE_LOG(AeonixNavigation, Error, TEXT("No AeonixSubsystem with a valid CollisionQueryInterface found"));
	}

#if WITH_EDITOR
	// Needed for debug rendering
	GetWorld()->PersistentLineBatcher->SetComponentTickEnabled(false);

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

	FlushPersistentDebugLines(GetWorld());

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

	UE_LOG(AeonixNavigation, Display, TEXT("Generation Time : %d"), BuildTime);
	UE_LOG(AeonixNavigation, Display, TEXT("Total Layers-Nodes : %d-%d"), NavigationData.OctreeData.GetNumLayers(), TotalNodes);
	UE_LOG(AeonixNavigation, Display, TEXT("Total Leaf Nodes : %d"), NavigationData.OctreeData.LeafNodes.Num());
	UE_LOG(AeonixNavigation, Display, TEXT("Total Size (bytes): %d"), TotalBytes);
#endif

	return true;
}

bool AAeonixBoundingVolume::HasData() const
{
	return NavigationData.OctreeData.GetNumLayers() > 0;	
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
	DrawDebugString(GetWorld(), Position, String, nullptr, Color, -1, false);
}

void AAeonixBoundingVolume::AeonixDrawDebugBox(const FVector& Position, const float Size, const FColor& Color) const
{
	DrawDebugBox(GetWorld(), Position, FVector(Size), FQuat::Identity, Color, true, -1.f, 0, .0f);
}

void AAeonixBoundingVolume::ClearData()
{
	NavigationData.ResetForGeneration();
	FlushPersistentDebugLines(GetWorld());
}


void AAeonixBoundingVolume::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	AeonixSubsystemInterface = GetWorld()->GetSubsystem<UAeonixSubsystem>();
	if (!AeonixSubsystemInterface.GetInterface())
	{
		UE_LOG(AeonixNavigation, Error, TEXT("No AeonixSubsystem with a valid AeonixInterface found"));
	}
	else
	{
		AeonixSubsystemInterface->RegisterVolume(this);
	}
}

void AAeonixBoundingVolume::Serialize(FArchive& Ar)
{
	// Serialize the usual UPROPERTIES
	Super::Serialize(Ar);

	if (GenerationParameters.GenerationStrategy == ESVOGenerationStrategy::UseBaked)
	{
		Ar << NavigationData.OctreeData;
	}
}

void AAeonixBoundingVolume::BeginPlay()
{
	AeonixSubsystemInterface = GetWorld()->GetSubsystem<UAeonixSubsystem>();
	if (!AeonixSubsystemInterface.GetInterface())
	{
		UE_LOG(AeonixNavigation, Error, TEXT("No AeonixSubsystem with a valid AeonixInterface found"));
	}
	else
	{
		AeonixSubsystemInterface->RegisterVolume(this);
	}

	CollisionQueryInterface = GetWorld()->GetSubsystem<UAeonixCollisionSubsystem>();
	if (!CollisionQueryInterface.GetInterface())
	{
		UE_LOG(AeonixNavigation, Error, TEXT("No AeonixSubsystem with a valid CollisionQueryInterface found"));
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
		UE_LOG(AeonixNavigation, Error, TEXT("No AeonixSubsystem with a valid AeonixInterface found"));
	}
	else
	{
		AeonixSubsystemInterface->UnRegisterVolume(this);
	}

	Super::EndPlay(EndPlayReason);
}
