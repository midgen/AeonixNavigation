#pragma once

#include "Data/AeonixOctreeData.h"
#include "Data/AeonixGenerationParameters.h"

#include "AeonixData.generated.h"

class IAeonixCollisionQueryInterface;
class IAeonixDebugDrawInterface;
struct FAeonixGenerationParamaters;

USTRUCT()
struct AEONIXNAVIGATION_API FAeonixData
{
	GENERATED_BODY()

	// SVO data
	UPROPERTY()
	FAeonixOctreeData OctreeData;

public:
	void SetExtents(const FVector& Origin, const FVector& Extents);
	void SetDebugPosition(const FVector& DebugPosition);

	void ResetForGeneration();
	void UpdateGenerationParameters(const FAeonixGenerationParameters& Params);
	const FAeonixGenerationParameters& GetParams() const;
	void Generate(UWorld& World, const IAeonixCollisionQueryInterface& CollisionInterface, const IAeonixDebugDrawInterface& DebugInterface);
	void RegenerateDynamicRegions(const IAeonixCollisionQueryInterface& CollisionInterface, const IAeonixDebugDrawInterface& DebugInterface);
	void RegenerateDynamicRegions(const TSet<FGuid>& RegionIds, const IAeonixCollisionQueryInterface& CollisionInterface, const IAeonixDebugDrawInterface& DebugInterface);

	bool GetLinkPosition(const AeonixLink& aLink, FVector& oPosition) const;
	bool GetNodePosition(layerindex_t aLayer, mortoncode_t aCode, FVector& oPosition) const;
	float GetVoxelSize(layerindex_t aLayer) const;
	int32 GetNumNodesPerSide(layerindex_t aLayer) const;
	/** Binary search a layer for a morton code. Returns false if no node has that code. */
	bool GetIndexForCode(layerindex_t aLayer, mortoncode_t aCode, nodeindex_t& oIndex) const;

	/**
	 * Resolve a world position to the free voxel that contains it.
	 * Returns false if the position is outside the generation bounds or inside a blocked voxel.
	 */
	bool GetLinkForPosition(const FVector& aPosition, AeonixLink& oLink) const;

	/** World-space bounds of the voxel a link refers to (leaf sub-voxel bounds for subdivided layer 0 nodes). */
	bool GetLinkBounds(const AeonixLink& aLink, FBox& oBounds) const;

	/**
	 * True if the straight segment from aStart to aEnd passes only through free voxels.
	 * Walks the segment cell by cell through the octree, so the cost scales with the number
	 * of voxels crossed rather than the segment length. Leaving the volume counts as blocked.
	 */
	bool HasLineOfSight(const FVector& aStart, const FVector& aEnd) const;

	//~ Begin UObject
	//void Serialize(FArchive& Ar) override;
	//~ End UObject 

private:
	FAeonixGenerationParameters GenerationParameters;
	int32 GetNumNodesInLayer(layerindex_t aLayer) const;

	bool IsBlocked(const FVector& aPosition, const float aSize) const;
	bool IsInDebugRange(const FVector& aPosition) const;
	bool IsAnyMemberBlocked(layerindex_t aLayer, mortoncode_t aCode) const;

	void BuildNeighbourLinks(layerindex_t aLayer, const IAeonixDebugDrawInterface& DebugInterface);
	bool FindLinkInDirection(layerindex_t aLayer, const nodeindex_t aNodeIndex, uint8 aDir, AeonixLink& oLinkToUpdate, FVector& aStartPosForDebug, const IAeonixDebugDrawInterface& DebugInterface);

	void RasterizeLeafNode(FVector& aOrigin, nodeindex_t aLeafIndex, const IAeonixCollisionQueryInterface& CollisionInterface, const IAeonixDebugDrawInterface& DebugInterface);
	void RasteriseLayer(layerindex_t aLayer, const IAeonixCollisionQueryInterface& CollisionInterface, const IAeonixDebugDrawInterface& DebugInterface);

	void FirstPassRasterise(const IAeonixCollisionQueryInterface& CollisionInterface);
};
