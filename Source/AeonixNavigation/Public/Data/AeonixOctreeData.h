#pragma once

#include <AeonixNavigation/Public/Data/AeonixLeafNode.h>
#include <AeonixNavigation/Public/Data/AeonixNode.h>

#include "AeonixOctreeData.generated.h"

USTRUCT()
struct FAeonixOctreeData
{
	GENERATED_BODY()

	// SVO data
	TArray<TArray<AeonixNode>> Layers;
	TArray<AeonixLeafNode> LeafNodes;
	// temporary data used during nav data generation first pass rasterize
	TArray<TSet<mortoncode_t>> BlockedIndices;

	void Reset()
	{
		Layers.Empty();
		LeafNodes.Empty();
	}

	int GetSize()
	{
		int Result = 0;
		Result += LeafNodes.Num() * sizeof(AeonixLeafNode);
		for (int i = 0; i < Layers.Num(); i++)
		{
			Result += Layers[i].Num() * sizeof(AeonixNode);
		}

		return Result;
	}

	uint8 NumLayers = 0;
	int NumBytes = 0;

	const uint8 GetNumLayers() const { return NumLayers; }
	TArray<AeonixNode>& GetLayer(layerindex_t aLayer) { return Layers[aLayer]; };
	const TArray<AeonixNode>& GetLayer(layerindex_t aLayer) const { return Layers[aLayer]; };
	const AeonixNode& GetNode(const AeonixLink& aLink) const;
	const AeonixLeafNode& GetLeafNode(nodeindex_t aIndex) const;
	void GetLeafNeighbours(const AeonixLink& aLink, TArray<AeonixLink>& oNeighbours) const;
	void GetNeighbours(const AeonixLink& aLink, TArray<AeonixLink>& oNeighbours) const;
};

FORCEINLINE FArchive& operator<<(FArchive& Ar, FAeonixOctreeData& AeonixData)
{
	Ar << AeonixData.Layers;
	Ar << AeonixData.LeafNodes;
	Ar << AeonixData.NumLayers;

	return Ar;
}
