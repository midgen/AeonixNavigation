#pragma once

#include "AeonixCollisionQueryInterface.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UAeonixCollisionQueryInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 *  Answers "is there blocking geometry here?" during octree generation.
 *  Positions are world space. VoxelSize / LeafSize are half-extents of the box tested.
 */
class IAeonixCollisionQueryInterface
{
	GENERATED_BODY()

public:
	/** Add interface function declarations here */
	virtual bool IsBlocked(const FVector& Position, const float VoxelSize, ECollisionChannel CollisionChannel, const float AgentRadius) const = 0;

	/**
	 * Tests if an entire leaf node (4x4x4 voxel block) contains any blocking geometry.
	 * Used for two-pass rasterization optimization - if this returns false, all 64 voxels in the leaf are guaranteed clear.
	 * @param Position Center position of the leaf node
	 * @param LeafSize Size of the entire leaf node (typically VoxelSize * 4)
	 * @param CollisionChannel Collision channel to test against
	 * @param AgentRadius Radius of the agent for clearance testing
	 * @return true if the leaf node contains any blocking geometry, false if completely clear
	 */
	virtual bool IsLeafBlocked(const FVector& Position, const float LeafSize, ECollisionChannel CollisionChannel, const float AgentRadius) const = 0;

	/**
	 * IsBlocked for a box rotated into a volume's frame (UAeonixVolumeComponent).
	 * The default tests the axis-aligned box that encloses the rotated one, which over-reports
	 * blocking but never misses it; override to test the rotated box exactly.
	 */
	virtual bool IsBlockedOriented(const FVector& Position, const FQuat& Rotation, const float VoxelSize, ECollisionChannel CollisionChannel, const float AgentRadius) const
	{
		return IsBlocked(Position, (VoxelSize + AgentRadius) * UE_SQRT_3 - AgentRadius, CollisionChannel, AgentRadius);
	}

	/** IsLeafBlocked for a box rotated into a volume's frame. Same default as IsBlockedOriented. */
	virtual bool IsLeafBlockedOriented(const FVector& Position, const FQuat& Rotation, const float LeafSize, ECollisionChannel CollisionChannel, const float AgentRadius) const
	{
		return IsLeafBlocked(Position, (LeafSize + AgentRadius) * UE_SQRT_3 - AgentRadius, CollisionChannel, AgentRadius);
	}

	/**
	 * Bounds of the blocking geometry expressed in Frame's local space, so a volume can fit itself
	 * around it. Returns false if this source can't tell (e.g. a world physics query).
	 */
	virtual bool GetBlockingBounds(const FTransform& Frame, FBox& OutLocalBounds) const { return false; }
};
