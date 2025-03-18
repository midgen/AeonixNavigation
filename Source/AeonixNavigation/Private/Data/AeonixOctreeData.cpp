#include <AeonixNavigation/Public/Data/AeonixOctreeData.h>

const AeonixNode& FAeonixOctreeData::GetNode(const AeonixLink& aLink) const
{
	if (aLink.GetLayerIndex() < 14)
	{
		return GetLayer(aLink.GetLayerIndex())[aLink.GetNodeIndex()];
	}
	else
	{
		return GetLayer(NumLayers - 1)[0];
	}
}

const AeonixLeafNode& FAeonixOctreeData::GetLeafNode(nodeindex_t aIndex) const
{
	return LeafNodes[aIndex];
}

void FAeonixOctreeData::GetLeafNeighbours(const AeonixLink& aLink, TArray<AeonixLink>& oNeighbours) const
{
	mortoncode_t leafIndex = aLink.GetSubnodeIndex();
	const AeonixNode& node = GetNode(aLink);
	const AeonixLeafNode& leaf = GetLeafNode(node.FirstChild.GetNodeIndex());

	// Get our starting co-ordinates
	uint_fast32_t x = 0, y = 0, z = 0;
	morton3D_64_decode(leafIndex, x, y, z);

	for (int i = 0; i < 6; i++)
	{
		// Need to switch to signed ints
		int32 sX = x + AeonixStatics::dirs[i].X;
		int32 sY = y + AeonixStatics::dirs[i].Y;
		int32 sZ = z + AeonixStatics::dirs[i].Z;

		// If the neighbour is in bounds of this leaf node
		if (sX >= 0 && sX < 4 && sY >= 0 && sY < 4 && sZ >= 0 && sZ < 4)
		{
			mortoncode_t thisIndex = morton3D_64_encode(sX, sY, sZ);
			// If this node is blocked, then no link in this direction, continue
			if (leaf.GetNode(thisIndex))
			{
				continue;
			}
			else // Otherwise, this is a valid link, add it
			{
				oNeighbours.Emplace(0, aLink.GetNodeIndex(), thisIndex);
				continue;
			}
		}
		else // the neighbours is out of bounds, we need to find our neighbour
		{
			const AeonixLink& neighbourLink = node.myNeighbours[i];
			const AeonixNode& neighbourNode = GetNode(neighbourLink);

			// If the neighbour layer 0 has no leaf nodes, just return it
			if (!neighbourNode.FirstChild.IsValid())
			{
				oNeighbours.Add(neighbourLink);
				continue;
			}

			const AeonixLeafNode& leafNode = GetLeafNode(neighbourNode.FirstChild.GetNodeIndex());

			if (leafNode.IsCompletelyBlocked())
			{
				// The leaf node is completely blocked, we don't return it
				continue;
			}
			else // Otherwise, we need to find the correct subnode
			{
				if (sX < 0)
					sX = 3;
				else if (sX > 3)
					sX = 0;
				else if (sY < 0)
					sY = 3;
				else if (sY > 3)
					sY = 0;
				else if (sZ < 0)
					sZ = 3;
				else if (sZ > 3)
					sZ = 0;
				//
				mortoncode_t subNodeCode = morton3D_64_encode(sX, sY, sZ);

				// Only return the neighbour if it isn't blocked!
				if (!leafNode.GetNode(subNodeCode))
				{
					oNeighbours.Emplace(0, neighbourNode.FirstChild.GetNodeIndex(), subNodeCode);
				}
			}
		}
	}
}

void FAeonixOctreeData::GetNeighbours(const AeonixLink& aLink, TArray<AeonixLink>& oNeighbours) const
{
	const AeonixNode& node = GetNode(aLink);

	for (int i = 0; i < 6; i++)
	{
		const AeonixLink& neighbourLink = node.myNeighbours[i];

		if (!neighbourLink.IsValid())
			continue;

		const AeonixNode& neighbour = GetNode(neighbourLink);

		// If the neighbour has no children, it's empty, we just use it
		if (!neighbour.HasChildren())
		{
			oNeighbours.Add(neighbourLink);
			continue;
		}

		// If the node has children, we need to look down the tree to see which children we want to add to the neighbour set

		// Start working set, and put the link into it
		TArray<AeonixLink, TInlineAllocator<16>> workingSet;
		workingSet.Push(neighbourLink);

		while (workingSet.Num() > 0)
		{
			// Pop off the top of the working set
			AeonixLink thisLink = workingSet.Pop();
			const AeonixNode& thisNode = GetNode(thisLink);

			// If the node as no children, it's clear, so add to neighbours and continue
			if (!thisNode.HasChildren())
			{
				oNeighbours.Add(neighbourLink);
				continue;
			}

			// We know it has children

			if (thisLink.GetLayerIndex() > 0)
			{
				// If it's above layer 0, we will need to potentially add 4 children using our offsets
				for (const nodeindex_t& childIndex : AeonixStatics::dirChildOffsets[i])
				{
					// Each of the childnodes
					AeonixLink childLink = thisNode.FirstChild;
					childLink.NodeIndex += childIndex;
					const AeonixNode& childNode = GetNode(childLink);

					if (childNode.HasChildren()) // If it has children, add them to the working set to keep going down
					{
						workingSet.Emplace(childLink);
					}
					else // Or just add to the outgoing links
					{
						oNeighbours.Emplace(childLink);
					}
				}
			}
			else
			{
				// If this is a leaf layer, then we need to add whichever of the 16 facing leaf nodes aren't blocked
				for (const nodeindex_t& leafIndex : AeonixStatics::dirLeafChildOffsets[i])
				{
					// Each of the childnodes
					AeonixLink link = neighbour.FirstChild;
					const AeonixLeafNode& leafNode = GetLeafNode(link.NodeIndex);
					link.SubnodeIndex = leafIndex;

					if (!leafNode.GetNode(leafIndex))
					{
						oNeighbours.Emplace(link);
					}
				}
			}
		}
	}
}