#include <AeonixNavigation/Public/Util/AeonixMediator.h>
#include <AeonixNavigation/Public/Data/AeonixLink.h>
#include <AeonixNavigation/Public/Actor/AeonixBoundingVolume.h>

#include <Runtime/Engine/Public/DrawDebugHelpers.h>

bool AeonixMediator::GetLinkFromPosition(const FVector& aPosition, const AAeonixBoundingVolume& aVolume, AeonixLink& oLink)
{
	// Position is outside the volume, no can do
	if (!aVolume.EncompassesPoint(aPosition))
	{
		return false;
	}

	if (!aVolume.HasData())
	{
		return false;
	}

	FBox box = aVolume.GetComponentsBoundingBox(true);

	FVector origin;
	FVector extent;

	box.GetCenterAndExtents(origin, extent);
	// The z-order origin of the volume (where code == 0)
	FVector zOrigin = origin - extent;
	// The local position of the point in volume space
	FVector localPos = aPosition - zOrigin;

	int layerIndex = aVolume.GetNavData().OctreeData.GetNumLayers() - 1;
	nodeindex_t nodeIndex = 0;
	while (layerIndex >= 0 && layerIndex < aVolume.GetNavData().OctreeData.GetNumLayers())
	{
		// Get the layer and voxel size

		const TArray<AeonixNode>& layer = aVolume.GetNavData().OctreeData.GetLayer(layerIndex);
		// Calculate the XYZ coordinates

		// TODO: compiler probably tidies this up, but we can do better
		FIntVector voxel;
		GetVolumeXYZ(aPosition, aVolume, layerIndex, voxel);
		uint_fast32_t x, y, z;
		x = voxel.X;
		y = voxel.Y;
		z = voxel.Z;

		// Get the morton code we want for this layer
		mortoncode_t code = morton3D_64_encode(x, y, z);

		for (nodeindex_t j = nodeIndex; j < layer.Num(); j++)
		{
			const AeonixNode& node = layer[j];
			// This is the node we are in
			if (node.Code == code)
			{
				// There are no child nodes, so this is our nav position
				if (!node.FirstChild.IsValid()) // && layerIndex > 0)
				{
					oLink.LayerIndex = layerIndex;
					oLink.NodeIndex = j;
					oLink.SubnodeIndex = 0;
					return true;
				}

				// If this is a leaf node, we need to find our subnode
				if (layerIndex == 0)
				{
					const AeonixLeafNode& leaf = aVolume.GetNavData().OctreeData.GetLeafNode(node.FirstChild.NodeIndex);
					// We need to calculate the node local position to get the morton code for the leaf
					float voxelSize = aVolume.GetNavData().GetVoxelSize(layerIndex);
					// The world position of the 0 node
					FVector nodePosition;
					aVolume.GetNavData().GetNodePosition(layerIndex, node.Code, nodePosition);
					// The morton origin of the node
					FVector nodeOrigin = nodePosition - FVector(voxelSize * 0.5f);
					// The requested position, relative to the node origin
					FVector nodeLocalPos = aPosition - nodeOrigin;
					// Now get our voxel coordinates
					FIntVector coord;
					coord.X = FMath::FloorToInt((nodeLocalPos.X / (voxelSize * 0.25f)));
					coord.Y = FMath::FloorToInt((nodeLocalPos.Y / (voxelSize * 0.25f)));
					coord.Z = FMath::FloorToInt((nodeLocalPos.Z / (voxelSize * 0.25f)));

					// So our link is.....*drum roll*
					oLink.LayerIndex = 0; // Layer 0 (leaf)
					oLink.NodeIndex = j;	// This index

					mortoncode_t leafIndex = morton3D_64_encode(coord.X, coord.Y, coord.Z); // This morton code is our key into the 64-bit leaf node

					if (leaf.GetNode(leafIndex))
					{
						return false; // This voxel is blocked, oops!
					}						

					oLink.SubnodeIndex = leafIndex;

					return true;
				}

				// If we've got here, the current node has a child, and isn't a leaf, so lets go down...
				layerIndex = layer[j].FirstChild.GetLayerIndex();
				nodeIndex = layer[j].FirstChild.GetNodeIndex();

				break; //stop iterating this layer
			}
		}
	}

	return false;
}

void AeonixMediator::GetVolumeXYZ(const FVector& aPosition, const AAeonixBoundingVolume& aVolume, const int aLayer, FIntVector& oXYZ)
{
	FBox box = aVolume.GetComponentsBoundingBox(true);

	FVector origin;
	FVector extent;

	box.GetCenterAndExtents(origin, extent);
	// The z-order origin of the volume (where code == 0)
	FVector zOrigin = origin - extent;
	// The local position of the point in volume space
	FVector localPos = aPosition - zOrigin;

	int layerIndex = aLayer;

	// Get the layer and voxel size
	float voxelSize = aVolume.GetNavData().GetVoxelSize(layerIndex);

	// Calculate the XYZ coordinates

	oXYZ.X = FMath::FloorToInt((localPos.X / voxelSize));
	oXYZ.Y = FMath::FloorToInt((localPos.Y / voxelSize));
	oXYZ.Z = FMath::FloorToInt((localPos.Z / voxelSize));
}
