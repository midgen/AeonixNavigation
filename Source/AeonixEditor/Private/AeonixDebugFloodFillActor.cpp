// Copyright Notice

#include "AeonixDebugFloodFillActor.h"
#include "AeonixNavigation.h"
#include "Subsystem/AeonixSubsystem.h"
#include "Actor/AeonixBoundingVolume.h"
#include "Data/AeonixData.h"
#include "Data/AeonixOctreeData.h"
#include "Data/AeonixLink.h"
#include "Data/AeonixNode.h"
#include "Util/AeonixMediator.h"
#include "Debug/AeonixDebugDrawManager.h"

AAeonixDebugFloodFillActor::AAeonixDebugFloodFillActor()
{
	PrimaryActorTick.bCanEverTick = false;

	// Create billboard component for editor visibility and movement
	BillboardComponent = CreateDefaultSubobject<UBillboardComponent>(TEXT("BillboardComponent"));
	RootComponent = BillboardComponent;

	// Set default values
	MaxVoxelCount = 1000;
}

void AAeonixDebugFloodFillActor::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// Perform the flood fill visualization
	PerformFloodFill();
}

void AAeonixDebugFloodFillActor::PostEditMove(bool bFinished)
{
	Super::PostEditMove(bFinished);

	if (bFinished)
	{
		// Re-run flood fill when actor is moved
		PerformFloodFill();
	}
}

void AAeonixDebugFloodFillActor::BeginDestroy()
{
	// Clear visualization before destroying
	ClearVisualization();

	Super::BeginDestroy();
}

void AAeonixDebugFloodFillActor::ClearVisualization()
{
	if (!GetWorld())
		return;

	if (UAeonixDebugDrawManager* DebugManager = GetWorld()->GetSubsystem<UAeonixDebugDrawManager>())
	{
		DebugManager->Clear(EAeonixDebugCategory::Tests);
	}
}

void AAeonixDebugFloodFillActor::PerformFloodFill()
{
	if (!GetWorld())
		return;

	// Get the Aeonix subsystem
	UAeonixSubsystem* AeonixSubsystem = GetWorld()->GetSubsystem<UAeonixSubsystem>();
	if (!AeonixSubsystem)
	{
		UE_LOG(LogAeonixNavigation, Warning, TEXT("AeonixDebugFloodFillActor: UAeonixSubsystem not found"));
		return;
	}

	// Get the volume for this actor's position
	const AAeonixBoundingVolume* NavVolume = AeonixSubsystem->GetVolumeForPosition(GetActorLocation());
	if (!NavVolume || !NavVolume->HasData())
	{
		UE_LOG(LogAeonixNavigation, Warning, TEXT("AeonixDebugFloodFillActor: No navigation volume found at actor location"));
		return;
	}

	const FAeonixData& NavData = NavVolume->GetNavData();

	// Convert actor position to AeonixLink
	AeonixLink StartLink;
	if (!AeonixMediator::GetLinkFromPosition(GetActorLocation(), *NavVolume, StartLink))
	{
		UE_LOG(LogAeonixNavigation, Warning, TEXT("AeonixDebugFloodFillActor: Failed to get link from position"));
		return;
	}

	// Get debug draw manager
	UAeonixDebugDrawManager* DebugManager = GetWorld()->GetSubsystem<UAeonixDebugDrawManager>();
	if (!DebugManager)
	{
		UE_LOG(LogAeonixNavigation, Warning, TEXT("AeonixDebugFloodFillActor: UAeonixDebugDrawManager not found"));
		return;
	}

	// Clear previous visualization
	DebugManager->Clear(EAeonixDebugCategory::Tests);

	// Get start position for marker
	FVector StartPos;
	if (!NavData.GetLinkPosition(StartLink, StartPos))
	{
		UE_LOG(LogAeonixNavigation, Warning, TEXT("AeonixDebugFloodFillActor: Failed to get start position"));
		return;
	}

	// Draw start marker (bright yellow sphere)
	DebugManager->AddSphere(StartPos, 50.0f, 16, FColor::Yellow, EAeonixDebugCategory::Tests);

	// Breadth-first flood fill
	TSet<AeonixLink> Visited;
	TQueue<TPair<AeonixLink, int32>> Queue; // Pair of Link and depth for color gradient
	Queue.Enqueue(TPair<AeonixLink, int32>(StartLink, 0));
	Visited.Add(StartLink);

	int32 VoxelsVisited = 0;
	int32 MaxDepth = 0;

	// First pass: collect all voxels and determine max depth for color gradient
	TArray<TPair<AeonixLink, int32>> VisitedVoxels;
	VisitedVoxels.Add(TPair<AeonixLink, int32>(StartLink, 0));

	while (!Queue.IsEmpty() && VoxelsVisited < MaxVoxelCount)
	{
		TPair<AeonixLink, int32> Current;
		Queue.Dequeue(Current);
		AeonixLink CurrentLink = Current.Key;
		int32 CurrentDepth = Current.Value;

		VoxelsVisited++;
		MaxDepth = FMath::Max(MaxDepth, CurrentDepth);

		// Get neighbors
		TArray<AeonixLink> Neighbors;
		const AeonixNode& CurrentNode = NavData.OctreeData.GetNode(CurrentLink);

		if (CurrentLink.GetLayerIndex() == 0 && CurrentNode.FirstChild.IsValid())
		{
			// Subdivided leaf node - use leaf neighbor logic
			NavData.OctreeData.GetLeafNeighbours(CurrentLink, Neighbors);
		}
		else
		{
			// Non-leaf or non-subdivided node
			NavData.OctreeData.GetNeighbours(CurrentLink, Neighbors);
		}

		// Process neighbors
		for (const AeonixLink& Neighbor : Neighbors)
		{
			if (!Neighbor.IsValid() || Visited.Contains(Neighbor))
				continue;

			if (VoxelsVisited >= MaxVoxelCount)
				break;

			Visited.Add(Neighbor);
			Queue.Enqueue(TPair<AeonixLink, int32>(Neighbor, CurrentDepth + 1));
			VisitedVoxels.Add(TPair<AeonixLink, int32>(Neighbor, CurrentDepth + 1));
		}
	}

	// Second pass: draw connections with color gradient
	TSet<AeonixLink> DrawnSet;
	DrawnSet.Add(StartLink);

	for (const TPair<AeonixLink, int32>& VisitedVoxel : VisitedVoxels)
	{
		AeonixLink CurrentLink = VisitedVoxel.Key;
		int32 CurrentDepth = VisitedVoxel.Value;

		// Calculate color based on depth (green at start, red at max depth)
		float NormalizedDepth = MaxDepth > 0 ? (float)CurrentDepth / (float)MaxDepth : 0.0f;
		FColor CurrentColor = FLinearColor::LerpUsingHSV(
			FLinearColor::Green,
			FLinearColor::Red,
			NormalizedDepth
		).ToFColor(true);

		// Get current position
		FVector CurrentPos;
		if (!NavData.GetLinkPosition(CurrentLink, CurrentPos))
			continue;

		// Get neighbors to draw connections
		TArray<AeonixLink> Neighbors;
		const AeonixNode& CurrentNode = NavData.OctreeData.GetNode(CurrentLink);

		if (CurrentLink.GetLayerIndex() == 0 && CurrentNode.FirstChild.IsValid())
		{
			NavData.OctreeData.GetLeafNeighbours(CurrentLink, Neighbors);
		}
		else
		{
			NavData.OctreeData.GetNeighbours(CurrentLink, Neighbors);
		}

		// Draw lines to neighbors that were visited
		for (const AeonixLink& Neighbor : Neighbors)
		{
			if (!Neighbor.IsValid() || !Visited.Contains(Neighbor))
				continue;

			// Only draw each connection once
			if (DrawnSet.Contains(Neighbor))
				continue;

			FVector NeighborPos;
			if (!NavData.GetLinkPosition(Neighbor, NeighborPos))
				continue;

			// Draw connection line
			DebugManager->AddLine(CurrentPos, NeighborPos, CurrentColor, LineThickness, EAeonixDebugCategory::Tests);
		}

		DrawnSet.Add(CurrentLink);
	}

	UE_LOG(LogAeonixNavigation, Log, TEXT("AeonixDebugFloodFillActor: Flood fill completed. Visited %d voxels (max depth: %d)"), VoxelsVisited, MaxDepth);
}
