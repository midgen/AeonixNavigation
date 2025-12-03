// Copyright 2024 Chris Ashworth

#include "AeonixBlockedVoxelVisualizer.h"

#include "Editor.h"
#include "LevelEditorViewport.h"

#include "Actor/AeonixBoundingVolume.h"
#include "Data/AeonixData.h"
#include "Data/AeonixOctreeData.h"
#include "Data/AeonixNode.h"
#include "Data/AeonixLeafNode.h"
#include "Data/AeonixGenerationParameters.h"
#include "Debug/AeonixDebugDrawManager.h"

// Include libmorton for morton encoding
#include "AeonixNavigation/Private/Library/libmorton/morton.h"

void FAeonixBlockedVoxelVisualizer::VisualizeBlockedVoxels(
	UWorld* World,
	AAeonixBoundingVolume* Volume,
	int32 MaxVoxels,
	float Range)
{
	if (!World || !Volume || !Volume->HasData())
	{
		return;
	}

	UAeonixDebugDrawManager* DebugManager = World->GetSubsystem<UAeonixDebugDrawManager>();
	if (!DebugManager)
	{
		return;
	}

	// Clear previous visualization
	DebugManager->Clear(EAeonixDebugCategory::Tests);

	const FAeonixData& NavData = Volume->GetNavData();
	const FAeonixGenerationParameters& Params = NavData.GetParams();

	// Calculate sub-voxel size (layer 0 voxels are subdivided 4x4x4)
	float Layer0VoxelSize = NavData.GetVoxelSize(0);
	float SubVoxelSize = Layer0VoxelSize * 0.25f;

	// Volume bounds in world space
	FVector VolumeMin = Params.Origin - Params.Extents;
	FVector VolumeMax = Params.Origin + Params.Extents;

	// Get flood fill start position (projected forward from camera)
	FVector StartPos = GetCameraStartPosition(Range);

	// Grid-based BFS flood fill
	TSet<FIntVector> Visited;
	TQueue<FIntVector> Queue;

	// Convert start position to grid coordinates
	FIntVector StartGrid(
		FMath::FloorToInt((StartPos.X - VolumeMin.X) / SubVoxelSize),
		FMath::FloorToInt((StartPos.Y - VolumeMin.Y) / SubVoxelSize),
		FMath::FloorToInt((StartPos.Z - VolumeMin.Z) / SubVoxelSize)
	);

	Queue.Enqueue(StartGrid);
	Visited.Add(StartGrid);

	int32 BlockedCount = 0;
	int32 Steps = 0;
	const int32 MaxSteps = MaxVoxels * 10; // Allow visiting more cells than blocked count

	// 6 face-adjacent directions
	static const FIntVector Directions[6] = {
		FIntVector(1, 0, 0), FIntVector(-1, 0, 0),
		FIntVector(0, 1, 0), FIntVector(0, -1, 0),
		FIntVector(0, 0, 1), FIntVector(0, 0, -1)
	};

	while (!Queue.IsEmpty() && Steps < MaxSteps && BlockedCount < MaxVoxels)
	{
		FIntVector CurrentGrid;
		Queue.Dequeue(CurrentGrid);
		Steps++;

		// Convert grid back to world position (center of sub-voxel)
		FVector WorldPos = VolumeMin + FVector(
			(CurrentGrid.X + 0.5f) * SubVoxelSize,
			(CurrentGrid.Y + 0.5f) * SubVoxelSize,
			(CurrentGrid.Z + 0.5f) * SubVoxelSize
		);

		// Check if within volume bounds
		if (WorldPos.X < VolumeMin.X || WorldPos.X > VolumeMax.X ||
			WorldPos.Y < VolumeMin.Y || WorldPos.Y > VolumeMax.Y ||
			WorldPos.Z < VolumeMin.Z || WorldPos.Z > VolumeMax.Z)
		{
			continue;
		}

		// Check if this position is blocked
		if (IsPositionBlocked(WorldPos, Volume))
		{
			// Draw red box for blocked voxel
			DebugManager->AddBox(
				WorldPos,
				FVector(SubVoxelSize * 0.5f), // Full voxel size (extent = half size)
				FQuat::Identity,
				FColor::Red,
				EAeonixDebugCategory::Tests
			);
			BlockedCount++;
		}

		// Add neighbors to queue
		for (const FIntVector& Dir : Directions)
		{
			FIntVector Neighbor = CurrentGrid + Dir;
			if (!Visited.Contains(Neighbor))
			{
				Visited.Add(Neighbor);
				Queue.Enqueue(Neighbor);
			}
		}
	}
}

void FAeonixBlockedVoxelVisualizer::ClearVisualization(UWorld* World)
{
	if (!World)
	{
		return;
	}

	UAeonixDebugDrawManager* DebugManager = World->GetSubsystem<UAeonixDebugDrawManager>();
	if (DebugManager)
	{
		DebugManager->Clear(EAeonixDebugCategory::Tests);
	}
}

FVector FAeonixBlockedVoxelVisualizer::GetCameraPosition()
{
	if (!GEditor)
	{
		return FVector::ZeroVector;
	}

	// Try to get active level viewport
	FLevelEditorViewportClient* ViewportClient = nullptr;
	for (FLevelEditorViewportClient* LevelVC : GEditor->GetLevelViewportClients())
	{
		if (LevelVC && LevelVC->IsPerspective())
		{
			ViewportClient = LevelVC;
			break;
		}
	}

	if (!ViewportClient)
	{
		return FVector::ZeroVector;
	}

	return ViewportClient->GetViewLocation();
}

FVector FAeonixBlockedVoxelVisualizer::GetCameraStartPosition(float Range)
{
	if (!GEditor)
	{
		return FVector::ZeroVector;
	}

	// Try to get active level viewport
	FLevelEditorViewportClient* ViewportClient = nullptr;
	for (FLevelEditorViewportClient* LevelVC : GEditor->GetLevelViewportClients())
	{
		if (LevelVC && LevelVC->IsPerspective())
		{
			ViewportClient = LevelVC;
			break;
		}
	}

	if (!ViewportClient)
	{
		return FVector::ZeroVector;
	}

	FVector CameraLocation = ViewportClient->GetViewLocation();
	FRotator CameraRotation = ViewportClient->GetViewRotation();

	// Return point Range units in front of camera
	return CameraLocation + CameraRotation.Vector() * Range;
}

bool FAeonixBlockedVoxelVisualizer::IsPositionBlocked(
	const FVector& WorldPos,
	const AAeonixBoundingVolume* Volume)
{
	const FAeonixData& NavData = Volume->GetNavData();
	const FAeonixGenerationParameters& Params = NavData.GetParams();
	const FAeonixOctreeData& OctreeData = NavData.OctreeData;

	// Calculate voxel sizes
	float Layer0VoxelSize = NavData.GetVoxelSize(0);

	// Volume bounds
	FVector VolumeMin = Params.Origin - Params.Extents;

	// Calculate which layer 0 node contains this position
	FVector RelativePos = WorldPos - VolumeMin;
	int32 NodeX = FMath::FloorToInt(RelativePos.X / Layer0VoxelSize);
	int32 NodeY = FMath::FloorToInt(RelativePos.Y / Layer0VoxelSize);
	int32 NodeZ = FMath::FloorToInt(RelativePos.Z / Layer0VoxelSize);

	// Clamp to valid range
	if (NodeX < 0 || NodeY < 0 || NodeZ < 0)
	{
		return false;
	}

	// Encode to morton code
	mortoncode_t TargetCode = morton3D_64_encode(NodeX, NodeY, NodeZ);

	// Search for node in layer 0 (nodes are sorted by morton code)
	const TArray<AeonixNode>& Layer0 = OctreeData.GetLayer(0);

	if (Layer0.Num() == 0)
	{
		return false;
	}

	int32 NodeIdx = -1;

	// Binary search since nodes are sorted by Code
	int32 Lo = 0;
	int32 Hi = Layer0.Num() - 1;
	while (Lo <= Hi)
	{
		int32 Mid = (Lo + Hi) / 2;
		if (Layer0[Mid].Code == TargetCode)
		{
			NodeIdx = Mid;
			break;
		}
		else if (Layer0[Mid].Code < TargetCode)
		{
			Lo = Mid + 1;
		}
		else
		{
			Hi = Mid - 1;
		}
	}

	if (NodeIdx == -1)
	{
		return false; // Node doesn't exist (entirely open space)
	}

	const AeonixNode& Node = Layer0[NodeIdx];

	// Check if node has leaf subdivision
	if (!Node.FirstChild.IsValid())
	{
		return false; // No leaf data = open space
	}

	// Get the leaf node
	const AeonixLeafNode& Leaf = OctreeData.GetLeafNode(Node.FirstChild.GetNodeIndex());

	// Calculate sub-voxel size
	float SubVoxelSize = Layer0VoxelSize * 0.25f;

	// Calculate sub-voxel position within the node (0-3 for x, y, z)
	FVector NodeOrigin = VolumeMin + FVector(
		NodeX * Layer0VoxelSize,
		NodeY * Layer0VoxelSize,
		NodeZ * Layer0VoxelSize
	);
	FVector SubPos = (WorldPos - NodeOrigin) / SubVoxelSize;
	uint32 SubX = FMath::Clamp(FMath::FloorToInt(SubPos.X), 0, 3);
	uint32 SubY = FMath::Clamp(FMath::FloorToInt(SubPos.Y), 0, 3);
	uint32 SubZ = FMath::Clamp(FMath::FloorToInt(SubPos.Z), 0, 3);

	// Check if blocked (bit = 1 means blocked)
	return Leaf.GetNodeAt(SubX, SubY, SubZ);
}
