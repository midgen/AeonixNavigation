// Copyright 2024 Chris Ashworth

#pragma once

#include "CoreMinimal.h"

class AAeonixBoundingVolume;

/**
 * Static utility class for visualizing blocked voxels in the octree.
 * Uses a grid-based BFS flood fill to find and display blocked leaf sub-voxels.
 */
class AEONIXEDITOR_API FAeonixBlockedVoxelVisualizer
{
public:
	/**
	 * Visualize blocked voxels starting from a point projected forward from the camera.
	 * @param World The world context
	 * @param Volume The bounding volume containing the octree data
	 * @param MaxVoxels Maximum number of blocked voxels to draw
	 * @param Range Distance in front of camera to project the flood fill center
	 */
	static void VisualizeBlockedVoxels(UWorld* World, AAeonixBoundingVolume* Volume, int32 MaxVoxels, float Range);

	/**
	 * Clear the blocked voxel visualization.
	 * @param World The world context
	 */
	static void ClearVisualization(UWorld* World);

	/**
	 * Get the editor camera position.
	 * @return Position of the active perspective viewport camera
	 */
	static FVector GetCameraPosition();

	/**
	 * Get a position projected forward from the camera.
	 * @param Range Distance to project forward
	 * @return Position Range units in front of the active perspective viewport camera
	 */
	static FVector GetCameraStartPosition(float Range);

private:
	/**
	 * Check if a world position is inside a blocked leaf sub-voxel.
	 * @param WorldPos The world position to check
	 * @param Volume The bounding volume containing the octree data
	 * @return True if the position is blocked
	 */
	static bool IsPositionBlocked(const FVector& WorldPos, const AAeonixBoundingVolume* Volume);
};
