#pragma once

#include <AeonixNavigation/Public/Data/AeonixOctreeData.h>
#include <AeonixNavigation/Public/Data/AeonixGenerationParameters.h>

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

	bool GetLinkPosition(const AeonixLink& aLink, FVector& oPosition) const;
	bool GetNodePosition(layerindex_t aLayer, mortoncode_t aCode, FVector& oPosition) const;
	float GetVoxelSize(layerindex_t aLayer) const;

	//~ Begin UObject
	//void Serialize(FArchive& Ar) override;
	//~ End UObject 

private:
	FAeonixGenerationParameters GenerationParameters;
	int32 GetNumNodesInLayer(layerindex_t aLayer) const;
	int32 GetNumNodesPerSide(layerindex_t aLayer) const;

	bool IsBlocked(const FVector& aPosition, const float aSize) const;
	bool IsInDebugRange(const FVector& aPosition) const;
	bool IsAnyMemberBlocked(layerindex_t aLayer, mortoncode_t aCode) const;
	bool GetIndexForCode(layerindex_t aLayer, mortoncode_t aCode, nodeindex_t& oIndex) const;

	void BuildNeighbourLinks(layerindex_t aLayer);
	bool FindLinkInDirection(layerindex_t aLayer, const nodeindex_t aNodeIndex, uint8 aDir, AeonixLink& oLinkToUpdate, FVector& aStartPosForDebug);

	void RasterizeLeafNode(FVector& aOrigin, nodeindex_t aLeafIndex, const IAeonixCollisionQueryInterface& CollisionInterface, const IAeonixDebugDrawInterface& DebugInterface);
	void RasteriseLayer(layerindex_t aLayer, const IAeonixCollisionQueryInterface& CollisionInterface, const IAeonixDebugDrawInterface& DebugInterface);

	void FirstPassRasterise(UWorld& World);
};
