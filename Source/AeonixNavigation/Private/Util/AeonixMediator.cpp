#include "Util/AeonixMediator.h"
#include "Data/AeonixLink.h"
#include "Actor/AeonixBoundingVolume.h"

#include "DrawDebugHelpers.h"

bool AeonixMediator::GetLinkFromPosition(const FVector& aPosition, const AAeonixBoundingVolume& aVolume, AeonixLink& oLink)
{
	if (!aVolume.HasData())
	{
		return false;
	}

	const FAeonixData& NavData = aVolume.GetNavData();
	const FAeonixGenerationParameters& Params = NavData.GetParams();

	// Check against the bounds the octree was generated with, not the actor's live bounds.
	// Baked data keeps the generation-time Origin/Extents, so the two can drift apart if the
	// actor is moved or its brush edited after baking. The live bounds check would then
	// accept points that map outside the cached grid (issue #54).
	const FBox GenerationBounds(Params.Origin - Params.Extents, Params.Origin + Params.Extents);
	if (!GenerationBounds.IsInsideOrOn(aPosition))
	{
		return false;
	}

	const int32 NumLayers = NavData.OctreeData.GetNumLayers();
	if (NumLayers <= 0)
	{
		return false;
	}

	int layerIndex = NumLayers - 1;
	nodeindex_t nodeIndex = 0;
	bool bScanWholeLayer = true; // Only the top layer has no parent to narrow the search

	while (layerIndex >= 0 && layerIndex < NumLayers)
	{
		const TArray<AeonixNode>& layer = NavData.OctreeData.GetLayer(layerIndex);

		FIntVector voxel;
		if (!GetVolumeXYZ(aPosition, aVolume, layerIndex, voxel))
		{
			// Position rounds to a coordinate off the grid (e.g. exactly on the max face)
			return false;
		}

		// Get the morton code we want for this layer
		const mortoncode_t code = morton3D_64_encode(voxel.X, voxel.Y, voxel.Z);

		// Find the node with this code. A parent's 8 children are allocated contiguously
		// from FirstChild.NodeIndex, so below the top layer only that range needs checking.
		nodeindex_t foundIndex = INDEX_NONE;
		if (bScanWholeLayer)
		{
			if (!NavData.GetIndexForCode(layerIndex, code, foundIndex))
			{
				return false;
			}
		}
		else
		{
			const nodeindex_t endIndex = FMath::Min(nodeIndex + 8, layer.Num());
			for (nodeindex_t j = nodeIndex; j < endIndex; j++)
			{
				if (layer[j].Code == code)
				{
					foundIndex = j;
					break;
				}
			}
			if (foundIndex == INDEX_NONE)
			{
				// The sibling group under our parent doesn't contain this code. Nothing
				// further down can match either, so bail out rather than loop forever.
				return false;
			}
		}

		const AeonixNode& node = layer[foundIndex];

		// There are no child nodes, so this is our nav position
		if (!node.FirstChild.IsValid())
		{
			oLink.LayerIndex = layerIndex;
			oLink.NodeIndex = foundIndex;
			oLink.SubnodeIndex = 0;
			return true;
		}

		// If this is a leaf node, we need to find our subnode
		if (layerIndex == 0)
		{
			const AeonixLeafNode& leaf = NavData.OctreeData.GetLeafNode(node.FirstChild.NodeIndex);
			// We need to calculate the node local position to get the morton code for the leaf
			const float voxelSize = NavData.GetVoxelSize(layerIndex);
			// The world position of the 0 node
			FVector nodePosition;
			NavData.GetNodePosition(layerIndex, node.Code, nodePosition);
			// The morton origin of the node
			const FVector nodeOrigin = nodePosition - FVector(voxelSize * 0.5f);
			// The requested position, relative to the node origin
			const FVector nodeLocalPos = aPosition - nodeOrigin;
			// Now get our voxel coordinates, clamped to the 4x4x4 leaf grid so a point on the
			// far face of a node doesn't produce an out-of-range subnode index
			const float leafVoxelSize = voxelSize * 0.25f;
			FIntVector coord;
			coord.X = FMath::Clamp(FMath::FloorToInt(nodeLocalPos.X / leafVoxelSize), 0, 3);
			coord.Y = FMath::Clamp(FMath::FloorToInt(nodeLocalPos.Y / leafVoxelSize), 0, 3);
			coord.Z = FMath::Clamp(FMath::FloorToInt(nodeLocalPos.Z / leafVoxelSize), 0, 3);

			const mortoncode_t leafIndex = morton3D_64_encode(coord.X, coord.Y, coord.Z); // This morton code is our key into the 64-bit leaf node

			if (leaf.GetNode(leafIndex))
			{
				return false; // This voxel is blocked, oops!
			}

			oLink.LayerIndex = 0; // Layer 0 (leaf)
			oLink.NodeIndex = foundIndex;
			oLink.SubnodeIndex = leafIndex;
			return true;
		}

		// If we've got here, the current node has a child, and isn't a leaf, so lets go down...
		layerIndex = node.FirstChild.GetLayerIndex();
		nodeIndex = node.FirstChild.GetNodeIndex();
		bScanWholeLayer = false;
	}

	return false;
}

bool AeonixMediator::GetVolumeXYZ(const FVector& aPosition, const AAeonixBoundingVolume& aVolume, const int aLayer, FIntVector& oXYZ)
{
	const FAeonixData& NavData = aVolume.GetNavData();
	const FAeonixGenerationParameters& Params = NavData.GetParams();
	// The z-order origin of the volume (where code == 0)
	const FVector zOrigin = Params.Origin - Params.Extents;
	// The local position of the point in volume space
	const FVector localPos = aPosition - zOrigin;

	const float voxelSize = NavData.GetVoxelSize(aLayer);

	oXYZ.X = FMath::FloorToInt((localPos.X / voxelSize));
	oXYZ.Y = FMath::FloorToInt((localPos.Y / voxelSize));
	oXYZ.Z = FMath::FloorToInt((localPos.Z / voxelSize));

	// A point exactly on the max face floors to NodesPerSide, which is one past the grid.
	// Treat that as the last voxel so boundary points still resolve; anything further out
	// is genuinely outside the volume.
	const int32 nodesPerSide = NavData.GetNumNodesPerSide(aLayer);
	const int32 maxIndex = nodesPerSide - 1;
	if (oXYZ.X < 0 || oXYZ.Y < 0 || oXYZ.Z < 0 ||
		oXYZ.X > nodesPerSide || oXYZ.Y > nodesPerSide || oXYZ.Z > nodesPerSide)
	{
		return false;
	}
	oXYZ.X = FMath::Min(oXYZ.X, maxIndex);
	oXYZ.Y = FMath::Min(oXYZ.Y, maxIndex);
	oXYZ.Z = FMath::Min(oXYZ.Z, maxIndex);
	return true;
}
