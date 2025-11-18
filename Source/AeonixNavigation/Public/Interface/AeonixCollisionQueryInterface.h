#pragma once

#include "AeonixCollisionQueryInterface.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UAeonixCollisionQueryInterface : public UInterface
{
	GENERATED_BODY()
};

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
};