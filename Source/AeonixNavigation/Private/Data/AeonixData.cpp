#include "Data/AeonixData.h"
#include "Data/AeonixGenerationParameters.h"
#include "Interface/AeonixCollisionQueryInterface.h"
#include "Interface/AeonixDebugDrawInterface.h"

void FAeonixData::SetExtents(const FVector& Origin, const FVector& Extents)
{
	GenerationParameters.Origin = Origin;
	GenerationParameters.Extents = Extents;
}

void FAeonixData::SetDebugPosition(const FVector& DebugPosition)
{
	GenerationParameters.DebugPosition = DebugPosition;
}

void FAeonixData::ResetForGeneration()
{
	// Clear temp data
	OctreeData.BlockedIndices.Empty();
	// Clear existing Octree data
	OctreeData.Layers.Empty();
	OctreeData.LeafNodes.Empty();
}

void FAeonixData::UpdateGenerationParameters(const FAeonixGenerationParameters& Params)
{
	GenerationParameters = Params;
	OctreeData.NumLayers = Params.VoxelPower + 1;
}

const FAeonixGenerationParameters& FAeonixData::GetParams() const
{
	return GenerationParameters;
}

void FAeonixData::Generate(UWorld& World, const IAeonixCollisionQueryInterface& CollisionInterface, const IAeonixDebugDrawInterface& DebugInterface)
{
	FirstPassRasterise(CollisionInterface);

	// Allocate the leaf node data
	OctreeData.LeafNodes.Empty();
	OctreeData.LeafNodes.AddDefaulted(OctreeData.BlockedIndices[0].Num() * 8 * 0.25f);

	// Add layers
	for (int i = 0; i < OctreeData.NumLayers; i++)
	{
		OctreeData.Layers.Emplace();
	}

	// Rasterize layer, bottom up, adding parent/child links
	for (int i = 0; i < OctreeData.NumLayers; i++)
	{
		RasteriseLayer(i, CollisionInterface, DebugInterface);
	}

	// Now traverse down, adding neighbour links
	for (int i = OctreeData.NumLayers - 2; i >= 0; i--)
	{
		BuildNeighbourLinks(i, DebugInterface);
	}
}

void FAeonixData::RegenerateDynamicSubregions(const IAeonixCollisionQueryInterface& CollisionInterface, const IAeonixDebugDrawInterface& DebugInterface)
{
	// Regenerate leaf voxels within dynamic regions by re-sampling collision geometry
	for (const FBox& DynamicRegion : GenerationParameters.DynamicRegionBoxes)
	{
		const float VoxelSize = GetVoxelSize(0); // Layer 0 voxel size
		const int32 NodesPerSide = GetNumNodesPerSide(0);
		const FVector VoxelOrigin = GenerationParameters.Origin - GenerationParameters.Extents;

		// Calculate Layer 0 voxel coordinate bounds that overlap with the dynamic region
		const FVector RegionMin = DynamicRegion.Min - VoxelOrigin;
		const FVector RegionMax = DynamicRegion.Max - VoxelOrigin;

		const int32 MinX = FMath::Max(0, FMath::FloorToInt(RegionMin.X / VoxelSize));
		const int32 MinY = FMath::Max(0, FMath::FloorToInt(RegionMin.Y / VoxelSize));
		const int32 MinZ = FMath::Max(0, FMath::FloorToInt(RegionMin.Z / VoxelSize));

		const int32 MaxX = FMath::Min(NodesPerSide - 1, FMath::CeilToInt(RegionMax.X / VoxelSize));
		const int32 MaxY = FMath::Min(NodesPerSide - 1, FMath::CeilToInt(RegionMax.Y / VoxelSize));
		const int32 MaxZ = FMath::Min(NodesPerSide - 1, FMath::CeilToInt(RegionMax.Z / VoxelSize));

		// Re-rasterize all overlapping Layer 0 nodes
		TArray<AeonixNode>& Layer0 = OctreeData.GetLayer(0);
		for (int32 X = MinX; X <= MaxX; ++X)
		{
			for (int32 Y = MinY; Y <= MaxY; ++Y)
			{
				for (int32 Z = MinZ; Z <= MaxZ; ++Z)
				{
					mortoncode_t Code = morton3D_64_encode(X, Y, Z);

					// Find this node in Layer 0
					for (int32 NodeIdx = 0; NodeIdx < Layer0.Num(); ++NodeIdx)
					{
						if (Layer0[NodeIdx].Code == Code)
						{
							// Get the position of this Layer 0 node
							FVector NodePosition;
							GetNodePosition(0, Code, NodePosition);

							// Get or calculate the leaf index
							// During generation, leaf nodes are allocated 1:1 with Layer 0 nodes
							// NodeIdx in Layer0 array corresponds to the same index in LeafNodes array
							nodeindex_t LeafIndex = NodeIdx;

							// IMPORTANT: Clear the existing leaf node data first
							if (LeafIndex < OctreeData.LeafNodes.Num())
							{
								OctreeData.LeafNodes[LeafIndex].Clear();
							}

							// Re-rasterize the leaf voxels (updates the 64-bit VoxelGrid bitmask)
							// Also need to pass the corner of the node, not center
							FVector LeafOrigin = NodePosition - FVector(VoxelSize * 0.5f);
							RasterizeLeafNode(LeafOrigin, LeafIndex, CollisionInterface, DebugInterface);

							// Update the FirstChild link to mark this as having valid leaf data
							AeonixNode& Node = Layer0[NodeIdx];
							Node.FirstChild.SetLayerIndex(0);
							Node.FirstChild.SetNodeIndex(LeafIndex);
							Node.FirstChild.SetSubnodeIndex(0);

							break; // Found and updated this node, move to next
						}
					}
				}
			}
		}
	}
}

int32 FAeonixData::GetNumNodesInLayer(layerindex_t Layer) const
{
	return FMath::Pow(FMath::Pow(2.f, (GenerationParameters.VoxelPower - (Layer))), 3);
}

int32 FAeonixData::GetNumNodesPerSide(layerindex_t Layer) const
{
	return FMath::Pow(2.f, (GenerationParameters.VoxelPower - (Layer)));
}

bool FAeonixData::GetLinkPosition(const AeonixLink& Link, FVector& Position) const
{
	const AeonixNode& Node = OctreeData.GetLayer(Link.GetLayerIndex())[Link.GetNodeIndex()];

	GetNodePosition(Link.GetLayerIndex(), Node.Code, Position);
	// If this is layer 0, and there are valid children
	if (Link.GetLayerIndex() == 0 && Node.FirstChild.IsValid())
	{
		float VoxelSize = GetVoxelSize(0);
		uint_fast32_t X, Y, Z;
		morton3D_64_decode(Link.GetSubnodeIndex(), X, Y, Z);
		Position += FVector(X * VoxelSize * 0.25f, Y * VoxelSize * 0.25f, Z * VoxelSize * 0.25f) - FVector(VoxelSize * 0.375);
		const AeonixLeafNode& LeafNode = OctreeData.GetLeafNode(Node.FirstChild.NodeIndex);
		bool bIsBlocked = LeafNode.GetNode(Link.GetSubnodeIndex());
		return !bIsBlocked;
	}
	return true;
}

bool FAeonixData::GetNodePosition(layerindex_t aLayer, mortoncode_t aCode, FVector& oPosition) const
{
	float VoxelSize = GetVoxelSize(aLayer);
	uint_fast32_t X, Y, Z;
	morton3D_64_decode(aCode, X, Y, Z);
	oPosition = GenerationParameters.Origin - GenerationParameters.Extents + FVector(X * VoxelSize, Y * VoxelSize, Z * VoxelSize) + FVector(VoxelSize * 0.5f);
	return true;
}

float FAeonixData::GetVoxelSize(layerindex_t Layer) const
{
	return (GenerationParameters.Extents.X / FMath::Pow(2.f, GenerationParameters.VoxelPower)) * (FMath::Pow(2.0f, Layer + 1));
}

bool FAeonixData::IsInDebugRange(const FVector& aPosition) const
{
	// If a debug filter box is active, use it for filtering instead of distance-based filtering
	if (GenerationParameters.bUseDebugFilterBox)
	{
		return GenerationParameters.DebugFilterBox.IsInside(aPosition);
	}

	// Fall back to distance-based filtering if no filter box is active
	return FVector::DistSquared(GenerationParameters.DebugPosition, aPosition) < GenerationParameters.DebugDistance * GenerationParameters.DebugDistance;
}

bool FAeonixData::IsAnyMemberBlocked(layerindex_t aLayer, mortoncode_t aCode) const
{
	mortoncode_t parentCode = aCode >> 3;

	if (aLayer == OctreeData.BlockedIndices.Num())
	{
		return true;
	}
	// The parent of this code is blocked
	if (OctreeData.BlockedIndices[aLayer].Contains(parentCode))
	{
		return true;
	}

	return false;
}

bool FAeonixData::GetIndexForCode(layerindex_t aLayer, mortoncode_t aCode, nodeindex_t& oIndex) const
{
	const TArray<AeonixNode>& layer = OctreeData.GetLayer(aLayer);

	for (int i = 0; i < layer.Num(); i++)
	{
		if (layer[i].Code == aCode)
		{
			oIndex = i;
			return true;
		}
	}

	return false;
}

void FAeonixData::BuildNeighbourLinks(layerindex_t aLayer, const IAeonixDebugDrawInterface& DebugInterface)
{
	TArray<AeonixNode>& layer = OctreeData.GetLayer(aLayer);
	layerindex_t searchLayer = aLayer;

	// For each node
	for (nodeindex_t i = 0; i < layer.Num(); i++)
	{
		AeonixNode& node = layer[i];
		// Get our world co-ordinate
		uint_fast32_t x, y, z;
		morton3D_64_decode(node.Code, x, y, z);
		nodeindex_t backtrackIndex = -1;
		nodeindex_t index = i;
		FVector nodePos;
		GetNodePosition(aLayer, node.Code, nodePos);

		// For each direction
		for (int d = 0; d < 6; d++)
		{
			AeonixLink& linkToUpdate = node.myNeighbours[d];

			backtrackIndex = index;

			while (!FindLinkInDirection(searchLayer, index, d, linkToUpdate, nodePos, DebugInterface) && aLayer < OctreeData.Layers.Num() - 2)
			{
				AeonixLink& parent = OctreeData.GetLayer(searchLayer)[index].Parent;
				if (parent.IsValid())
				{
					index = parent.NodeIndex;
					searchLayer = parent.LayerIndex;
				}
				else
				{
					searchLayer++;
					GetIndexForCode(searchLayer, node.Code >> 3, index);
				}
			}
			index = backtrackIndex;
			searchLayer = aLayer;
		}
	}
}

bool FAeonixData::FindLinkInDirection(layerindex_t aLayer, const nodeindex_t aNodeIndex, uint8 aDir, AeonixLink& oLinkToUpdate, FVector& aStartPosForDebug, const IAeonixDebugDrawInterface& DebugInterface)
{
	int32 maxCoord = GetNumNodesPerSide(aLayer);
	AeonixNode& node = OctreeData.GetLayer(aLayer)[aNodeIndex];
	TArray<AeonixNode>& layer = OctreeData.GetLayer(aLayer);

	// Get our world co-ordinate
	uint_fast32_t x = 0, y = 0, z = 0;
	morton3D_64_decode(node.Code, x, y, z);
	int32 sX = x, sY = y, sZ = z;
	// Add the direction
	sX += AeonixStatics::dirs[aDir].X;
	sY += AeonixStatics::dirs[aDir].Y;
	sZ += AeonixStatics::dirs[aDir].Z;

	// If the coords are out of bounds, the link is invalid.
	if (sX < 0 || sX >= maxCoord || sY < 0 || sY >= maxCoord || sZ < 0 || sZ >= maxCoord)
	{
		oLinkToUpdate.SetInvalid();
		if (GenerationParameters.ShowNeighbourLinks && IsInDebugRange(aStartPosForDebug))
		{
			FVector startPos, endPos;
			GetNodePosition(aLayer, node.Code, startPos);
			endPos = startPos + (FVector(AeonixStatics::dirs[aDir]) * 100.f);
			DebugInterface.AeonixDrawDebugLine(aStartPosForDebug, endPos, FColor::Red, 0.0f);
		}
		return true;
	}
	x = sX;
	y = sY;
	z = sZ;
	// Get the morton code for the direction
	mortoncode_t thisCode = morton3D_64_encode(x, y, z);
	bool isHigher = thisCode > node.Code;
	int32 nodeDelta = (isHigher ? 1 : -1);

	while ((aNodeIndex + nodeDelta) < layer.Num() && aNodeIndex + nodeDelta >= 0)
	{
		// This is the node we're looking for
		if (layer[aNodeIndex + nodeDelta].Code == thisCode)
		{
			const AeonixNode& thisNode = layer[aNodeIndex + nodeDelta];
			// This is a leaf node
			if (aLayer == 0 && thisNode.HasChildren())
			{
				// Set invalid link if the leaf node is completely blocked, no point linking to it
				if (OctreeData.GetLeafNode(thisNode.FirstChild.GetNodeIndex()).IsCompletelyBlocked())
				{
					oLinkToUpdate.SetInvalid();
					return true;
				}
			}
			// Otherwise, use this link
			oLinkToUpdate.LayerIndex = aLayer;
			check(aNodeIndex + nodeDelta < layer.Num());
			oLinkToUpdate.NodeIndex = aNodeIndex + nodeDelta;
			if (GenerationParameters.ShowNeighbourLinks && IsInDebugRange(aStartPosForDebug))
			{
				FVector endPos;
				GetNodePosition(aLayer, thisCode, endPos);
				DebugInterface.AeonixDrawDebugLine(aStartPosForDebug, endPos, AeonixStatics::myLinkColors[aLayer], 0.0f);
			}
			return true;
		}
		// If we've passed the code we're looking for, it's not on this layer
		else if ((isHigher && layer[aNodeIndex + nodeDelta].Code > thisCode) || (!isHigher && layer[aNodeIndex + nodeDelta].Code < thisCode))
		{
			return false;
		}

		nodeDelta += (isHigher ? 1 : -1);
	}

	// I'm not entirely sure if it's valid to reach the end? Hmmm...
	return false;
}

void FAeonixData::RasterizeLeafNode(FVector& aOrigin, nodeindex_t aLeafIndex, const IAeonixCollisionQueryInterface& CollisionInterface, const IAeonixDebugDrawInterface& DebugInterface)
{
	for (int i = 0; i < 64; i++)
	{

		uint_fast32_t x, y, z;
		morton3D_64_decode(i, x, y, z);
		float leafVoxelSize = GetVoxelSize(0) * 0.25f;
		FVector position = aOrigin + FVector(x * leafVoxelSize, y * leafVoxelSize, z * leafVoxelSize) + FVector(leafVoxelSize * 0.5f);

		if (aLeafIndex >= OctreeData.LeafNodes.Num() - 1)
			OctreeData.LeafNodes.AddDefaulted(1);

		if (CollisionInterface.IsBlocked(position, leafVoxelSize * 0.5f, GenerationParameters.CollisionChannel, GenerationParameters.AgentRadius))
		{
			OctreeData.LeafNodes[aLeafIndex].SetNode(i);

			if (GenerationParameters.ShowLeafVoxels && IsInDebugRange(position))
			{
				DebugInterface.AeonixDrawDebugBox(position, leafVoxelSize * 0.5f, FColor::Red);
				// DrawDebugBox(GetWorld(), position, FVector(leafVoxelSize * 0.5f), FQuat::Identity, FColor::Red, true, -1.f, 0, .0f);
			}
			if (GenerationParameters.ShowMortonCodes && IsInDebugRange(position))
			{
				DebugInterface.AeonixDrawDebugString(position, FString::FromInt(aLeafIndex) + ":" + FString::FromInt(i), FColor::Red);
				// DrawDebugString(GetWorld(), position, FString::FromInt(aLeafIndex) + ":" + FString::FromInt(i), nullptr, FColor::Red, -1, false);
			}
		}
	}
}

void FAeonixData::RasteriseLayer(layerindex_t aLayer, const IAeonixCollisionQueryInterface& CollisionInterface, const IAeonixDebugDrawInterface& DebugInterface)
{
	nodeindex_t leafIndex = 0;
	// Layer 0 Leaf nodes are special
	if (aLayer == 0)
	{
		// Run through all our coordinates
		int32 numNodes = GetNumNodesInLayer(aLayer);
		for (int32 i = 0; i < numNodes; i++)
		{
			int index = (i);

			// If we know this node needs to be added, from the low res first pass
			if (OctreeData.BlockedIndices[0].Contains(i >> 3))
			{
				// Add a node
				index = OctreeData.GetLayer(aLayer).Emplace();
				AeonixNode& node = OctreeData.GetLayer(aLayer)[index];

				// Set my code and position
				node.Code = (i);

				FVector nodePos;
				GetNodePosition(aLayer, node.Code, nodePos);

				// Debug stuff
				if (GenerationParameters.ShowMortonCodes && IsInDebugRange(nodePos))
				{
					DebugInterface.AeonixDrawDebugString(nodePos, FString::FromInt(aLayer) + ":" + FString::FromInt(index), AeonixStatics::myLayerColors[aLayer]);
					// DrawDebugString(GetWorld(), nodePos, FString::FromInt(aLayer) + ":" + FString::FromInt(index), nullptr, AeonixStatics::myLayerColors[aLayer], -1, false);
				}
				if (GenerationParameters.ShowVoxels && IsInDebugRange(nodePos))
				{

					DebugInterface.AeonixDrawDebugBox(nodePos, GetVoxelSize(aLayer) * 0.5f, AeonixStatics::myLayerColors[aLayer]);
					// DrawDebugBox(GetWorld(), nodePos, FVector(GetVoxelSize(aLayer) * 0.5f), FQuat::Identity, AeonixStatics::myLayerColors[aLayer], true, -1.f, 0, .0f);
				}

				// Now check if we have any blocking, and search leaf nodes
				FVector Position;
				GetNodePosition(0, i, Position);

				FCollisionQueryParams params;
				params.bFindInitialOverlaps = true;
				params.bTraceComplex = false;
				params.TraceTag = "AeonixRasterize";
				if (CollisionInterface.IsBlocked(Position, GetVoxelSize(0) * 0.5f, GenerationParameters.CollisionChannel, GenerationParameters.AgentRadius))
				// if (IsBlocked(Position, GetVoxelSize(0) * 0.5f))
				{
					// Rasterize my leaf nodes
					FVector leafOrigin = nodePos - (FVector(GetVoxelSize(aLayer) * 0.5f));
					RasterizeLeafNode(leafOrigin, leafIndex, CollisionInterface, DebugInterface);
					node.FirstChild.SetLayerIndex(0);
					node.FirstChild.SetNodeIndex(leafIndex);
					node.FirstChild.SetSubnodeIndex(0);
					leafIndex++;
				}
				else
				{
					OctreeData.LeafNodes.AddDefaulted(1);
					leafIndex++;
					node.FirstChild.SetInvalid();
				}
			}
		}
	}
	// Deal with the other layers
	else if (OctreeData.GetLayer(aLayer - 1).Num() > 1)
	{
		int nodeCounter = 0;
		int32 numNodes = GetNumNodesInLayer(aLayer);
		for (int32 i = 0; i < numNodes; i++)
		{
			// Do we have any blocking children, or siblings?
			// Remember we must have 8 children per parent
			if (IsAnyMemberBlocked(aLayer, i))
			{
				// Add a node
				int32 index = OctreeData.GetLayer(aLayer).Emplace();
				nodeCounter++;
				AeonixNode& node = OctreeData.GetLayer(aLayer)[index];
				// Set details
				node.Code = i;
				nodeindex_t childIndex = 0;
				if (GetIndexForCode(aLayer - 1, node.Code << 3, childIndex))
				{
					// Set parent->child links
					node.FirstChild.SetLayerIndex(aLayer - 1);
					node.FirstChild.SetNodeIndex(childIndex);
					// Set child->parent links, this can probably be done smarter, as we're duplicating work here
					for (int iter = 0; iter < 8; iter++)
					{
						OctreeData.GetLayer(node.FirstChild.GetLayerIndex())[node.FirstChild.GetNodeIndex() + iter].Parent.SetLayerIndex(aLayer);
						OctreeData.GetLayer(node.FirstChild.GetLayerIndex())[node.FirstChild.GetNodeIndex() + iter].Parent.SetNodeIndex(index);
					}

					if (GenerationParameters.ShowParentChildLinks) // Debug all the things
					{
						FVector startPos, endPos;
						GetNodePosition(aLayer, node.Code, startPos);
						GetNodePosition(aLayer - 1, node.Code << 3, endPos);
						if (IsInDebugRange(startPos))
						{
							DebugInterface.AeonixDrawDebugDirectionalArrow(startPos, endPos, AeonixStatics::myLinkColors[aLayer], 0.0f);
						}
					}
				}
				else
				{
					node.FirstChild.SetInvalid();
				}

				if (GenerationParameters.ShowMortonCodes || GenerationParameters.ShowVoxels)
				{
					FVector nodePos;
					GetNodePosition(aLayer, i, nodePos);

					// Debug stuff
					if (GenerationParameters.ShowVoxels && IsInDebugRange(nodePos))
					{
						DebugInterface.AeonixDrawDebugBox(nodePos, GetVoxelSize(aLayer) * 0.5f, AeonixStatics::myLayerColors[aLayer]);
						// DrawDebugBox(GetWorld(), nodePos, FVector(GetVoxelSize(aLayer) * 0.5f), FQuat::Identity, AeonixStatics::myLayerColors[aLayer], true, -1.f, 0, .0f);
					}
					if (GenerationParameters.ShowMortonCodes && IsInDebugRange(nodePos))
					{
						DebugInterface.AeonixDrawDebugString(nodePos, FString::FromInt(aLayer) + ":" + FString::FromInt(index), AeonixStatics::myLayerColors[aLayer]);
						// DrawDebugString(GetWorld(), nodePos, FString::FromInt(aLayer) + ":" + FString::FromInt(index), nullptr, AeonixStatics::myLayerColors[aLayer], -1, false);
					}
				}
			}
		}
	}
}

void FAeonixData::FirstPassRasterise(const IAeonixCollisionQueryInterface& CollisionInterface)
{
	// Add the first layer of blocking
	OctreeData.BlockedIndices.Emplace();

	int32 NumNodes = GetNumNodesInLayer(1);
	for (int32 i = 0; i < NumNodes; i++)
	{
		FVector Position;
		GetNodePosition(1, i, Position);
		float VoxelSize = GetVoxelSize(1);

		// Use the collision interface instead of direct world query
		if (CollisionInterface.IsBlocked(Position, VoxelSize * 0.5f, GenerationParameters.CollisionChannel, GenerationParameters.AgentRadius))
		{
			OctreeData.BlockedIndices[0].Add(i);
		}
	}

	// Force-allocate voxels within dynamic regions (ensures leaf nodes exist for runtime updates)
	for (const FBox& DynamicRegion : GenerationParameters.DynamicRegionBoxes)
	{
		const float VoxelSize = GetVoxelSize(1);
		const int32 NodesPerSide = GetNumNodesPerSide(1);
		const FVector VoxelOrigin = GenerationParameters.Origin - GenerationParameters.Extents;

		// Calculate voxel coordinate bounds that overlap with the dynamic region
		const FVector RegionMin = DynamicRegion.Min - VoxelOrigin;
		const FVector RegionMax = DynamicRegion.Max - VoxelOrigin;

		const int32 MinX = FMath::Max(0, FMath::FloorToInt(RegionMin.X / VoxelSize));
		const int32 MinY = FMath::Max(0, FMath::FloorToInt(RegionMin.Y / VoxelSize));
		const int32 MinZ = FMath::Max(0, FMath::FloorToInt(RegionMin.Z / VoxelSize));

		const int32 MaxX = FMath::Min(NodesPerSide - 1, FMath::CeilToInt(RegionMax.X / VoxelSize));
		const int32 MaxY = FMath::Min(NodesPerSide - 1, FMath::CeilToInt(RegionMax.Y / VoxelSize));
		const int32 MaxZ = FMath::Min(NodesPerSide - 1, FMath::CeilToInt(RegionMax.Z / VoxelSize));

		// Force-add all voxels in this range to ensure leaf nodes are allocated
		for (int32 X = MinX; X <= MaxX; ++X)
		{
			for (int32 Y = MinY; Y <= MaxY; ++Y)
			{
				for (int32 Z = MinZ; Z <= MaxZ; ++Z)
				{
					mortoncode_t Code = morton3D_64_encode(X, Y, Z);
					OctreeData.BlockedIndices[0].Add(Code); // TSet will ignore duplicates
				}
			}
		}
	}

	int LayerIndex = 0;

	while (OctreeData.BlockedIndices[LayerIndex].Num() > 1)
	{
		// Add a new layer to structure
		OctreeData.BlockedIndices.Emplace();
		// Add any parent morton codes to the new layer
		for (mortoncode_t& code : OctreeData.BlockedIndices[LayerIndex])
		{
			OctreeData.BlockedIndices[LayerIndex + 1].Add(code >> 3);
		}
		LayerIndex++;
	}
}
