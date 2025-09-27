#include <AeonixNavigation/Public/Pathfinding/AeonixJumpPointSearch.h>
#include <AeonixNavigation/Private/Library/libmorton/morton.h>

void AeonixJumpPointSearch::FindJumpPoints(uint_fast32_t startX, uint_fast32_t startY, uint_fast32_t startZ,
										   const AeonixLink& currentLink, TArray<AeonixLink>& outJumpPoints) const
{
	// Get directions to search based on settings
	const FIntVector* directions;
	int numDirections;

	if (bAllowDiagonals)
	{
		directions = AeonixStatics::allDirs26;
		numDirections = AeonixStatics::NUM_ALL_DIRS;
	}
	else
	{
		directions = AeonixStatics::straightDirs;
		numDirections = AeonixStatics::NUM_STRAIGHT_DIRS;
	}

	// Search in all valid directions
	for (int i = 0; i < numDirections; ++i)
	{
		const FIntVector& dir = directions[i];
		uint_fast32_t jumpX = startX;
		uint_fast32_t jumpY = startY;
		uint_fast32_t jumpZ = startZ;
		AeonixLink jumpNodeLink = currentLink;

		// Perform jump in this direction
		if (Jump(jumpX, jumpY, jumpZ, jumpNodeLink, dir, FIntVector::ZeroValue))
		{
			// Found a jump point, create link
			mortoncode_t jumpIndex = morton3D_64_encode(jumpX, jumpY, jumpZ);
			outJumpPoints.Emplace(0, jumpNodeLink.GetNodeIndex(), jumpIndex);
		}
	}
}

bool AeonixJumpPointSearch::GetLeafNodeAtPosition(int32 x, int32 y, int32 z, const AeonixLink& currentNodeLink,
												 AeonixLink& outNodeLink, uint_fast32_t& outLocalX, uint_fast32_t& outLocalY, uint_fast32_t& outLocalZ) const
{
	// Start with current node
	outNodeLink = currentNodeLink;

	// If position is within current leaf bounds (0-3), use current node
	if (x >= 0 && x < 4 && y >= 0 && y < 4 && z >= 0 && z < 4)
	{
		outLocalX = x;
		outLocalY = y;
		outLocalZ = z;
		return true;
	}

	// Position is outside current leaf - need to find neighboring node
	const AeonixNode& currentNode = NavigationData.OctreeData.GetNode(currentNodeLink);

	// Calculate which direction we're going
	FIntVector dir(0, 0, 0);
	if (x < 0) dir.X = -1;
	else if (x >= 4) dir.X = 1;
	if (y < 0) dir.Y = -1;
	else if (y >= 4) dir.Y = 1;
	if (z < 0) dir.Z = -1;
	else if (z >= 4) dir.Z = 1;

	// Find the appropriate neighbor
	for (int i = 0; i < 6; i++)
	{
		const FIntVector& checkDir = AeonixStatics::dirs[i];
		if ((dir.X != 0 && checkDir.X == dir.X) ||
			(dir.Y != 0 && checkDir.Y == dir.Y) ||
			(dir.Z != 0 && checkDir.Z == dir.Z))
		{
			const AeonixLink& neighborLink = currentNode.myNeighbours[i];
			if (neighborLink.IsValid() && neighborLink.GetLayerIndex() == 0)
			{
				const AeonixNode& neighborNode = NavigationData.OctreeData.GetNode(neighborLink);
				if (neighborNode.FirstChild.IsValid())
				{
					// Found a valid leaf neighbor
					outNodeLink = neighborLink;

					// Calculate local coordinates in the neighbor
					outLocalX = (x < 0) ? 3 : (x >= 4) ? 0 : x;
					outLocalY = (y < 0) ? 3 : (y >= 4) ? 0 : y;
					outLocalZ = (z < 0) ? 3 : (z >= 4) ? 0 : z;

					return true;
				}
			}
			break;
		}
	}

	return false;  // No valid neighboring leaf node found
}

bool AeonixJumpPointSearch::IsValidPosition(int32 x, int32 y, int32 z, const AeonixLink& currentNodeLink) const
{
	AeonixLink nodeLink;
	uint_fast32_t localX, localY, localZ;

	// Get the leaf node that contains this position
	if (!GetLeafNodeAtPosition(x, y, z, currentNodeLink, nodeLink, localX, localY, localZ))
	{
		return false;  // Position is outside navigable area
	}

	// Check if position is not blocked in the found leaf node
	const AeonixNode& node = NavigationData.OctreeData.GetNode(nodeLink);
	if (!node.FirstChild.IsValid())
	{
		return false;  // Not a leaf node
	}

	const AeonixLeafNode& leafNode = NavigationData.OctreeData.GetLeafNode(node.FirstChild.GetNodeIndex());
	return !leafNode.GetNodeAt(localX, localY, localZ);
}

bool AeonixJumpPointSearch::Jump(uint_fast32_t& x, uint_fast32_t& y, uint_fast32_t& z, AeonixLink& currentNodeLink,
								const FIntVector& dir, const FIntVector& parentDir) const
{
	// Iterative jumping with safety limit
	const int maxJumps = 100;  // Prevent infinite loops
	int jumpCount = 0;

	while (jumpCount < maxJumps)
	{
		// Step in the direction
		int32 newX = static_cast<int32>(x) + dir.X;
		int32 newY = static_cast<int32>(y) + dir.Y;
		int32 newZ = static_cast<int32>(z) + dir.Z;

		// Check if new position is valid (potentially in a different leaf node)
		AeonixLink newNodeLink;
		uint_fast32_t localX, localY, localZ;

		if (!GetLeafNodeAtPosition(newX, newY, newZ, currentNodeLink, newNodeLink, localX, localY, localZ))
		{
			return false;  // Hit boundary or non-navigable area
		}

		if (!IsValidPosition(newX, newY, newZ, currentNodeLink))
		{
			return false;  // Hit obstacle
		}

		// If we changed nodes, that's also a potential jump point (transition point)
		bool changedNodes = (newNodeLink.GetNodeIndex() != currentNodeLink.GetNodeIndex());

		// Update position and node
		x = localX;
		y = localY;
		z = localZ;
		currentNodeLink = newNodeLink;

		// Check if this is a jump point
		if (IsJumpPoint(x, y, z, currentNodeLink, dir, parentDir))
		{
			return true;
		}

		// If we changed nodes, that's also a potential jump point (transition point)
		if (changedNodes)
		{
			return true;
		}

		jumpCount++;
	}

	return false;  // Hit jump limit
}

bool AeonixJumpPointSearch::IsJumpPoint(uint_fast32_t x, uint_fast32_t y, uint_fast32_t z, const AeonixLink& currentNodeLink,
										const FIntVector& dir, const FIntVector& parentDir) const
{
	// Check for forced neighbors
	if (HasForcedNeighborsStraight(x, y, z, currentNodeLink, dir))
	{
		return true;
	}

	if (bAllowDiagonals && HasForcedNeighborsDiagonal(x, y, z, currentNodeLink, dir))
	{
		return true;
	}

	return false;
}

bool AeonixJumpPointSearch::HasForcedNeighborsStraight(uint_fast32_t x, uint_fast32_t y, uint_fast32_t z,
													   const AeonixLink& currentNodeLink, const FIntVector& dir) const
{
	// Check for forced neighbors in straight line movement
	if (dir.X != 0 && dir.Y == 0 && dir.Z == 0) // Moving along X axis
	{
		// Check if there are blocked neighbors that force consideration of other directions
		return (!IsValidPosition(x, y + 1, z, currentNodeLink) && IsValidPosition(x + dir.X, y + 1, z, currentNodeLink)) ||
			   (!IsValidPosition(x, y - 1, z, currentNodeLink) && IsValidPosition(x + dir.X, y - 1, z, currentNodeLink)) ||
			   (!IsValidPosition(x, y, z + 1, currentNodeLink) && IsValidPosition(x + dir.X, y, z + 1, currentNodeLink)) ||
			   (!IsValidPosition(x, y, z - 1, currentNodeLink) && IsValidPosition(x + dir.X, y, z - 1, currentNodeLink));
	}
	else if (dir.Y != 0 && dir.X == 0 && dir.Z == 0) // Moving along Y axis
	{
		return (!IsValidPosition(x + 1, y, z, currentNodeLink) && IsValidPosition(x + 1, y + dir.Y, z, currentNodeLink)) ||
			   (!IsValidPosition(x - 1, y, z, currentNodeLink) && IsValidPosition(x - 1, y + dir.Y, z, currentNodeLink)) ||
			   (!IsValidPosition(x, y, z + 1, currentNodeLink) && IsValidPosition(x, y + dir.Y, z + 1, currentNodeLink)) ||
			   (!IsValidPosition(x, y, z - 1, currentNodeLink) && IsValidPosition(x, y + dir.Y, z - 1, currentNodeLink));
	}
	else if (dir.Z != 0 && dir.X == 0 && dir.Y == 0) // Moving along Z axis
	{
		return (!IsValidPosition(x + 1, y, z, currentNodeLink) && IsValidPosition(x + 1, y, z + dir.Z, currentNodeLink)) ||
			   (!IsValidPosition(x - 1, y, z, currentNodeLink) && IsValidPosition(x - 1, y, z + dir.Z, currentNodeLink)) ||
			   (!IsValidPosition(x, y + 1, z, currentNodeLink) && IsValidPosition(x, y + 1, z + dir.Z, currentNodeLink)) ||
			   (!IsValidPosition(x, y - 1, z, currentNodeLink) && IsValidPosition(x, y - 1, z + dir.Z, currentNodeLink));
	}

	return false;
}

bool AeonixJumpPointSearch::HasForcedNeighborsDiagonal(uint_fast32_t x, uint_fast32_t y, uint_fast32_t z,
													   const AeonixLink& currentNodeLink, const FIntVector& dir) const
{
	// For diagonal movement, check if any of the straight line components have forced neighbors
	if (FMath::Abs(dir.X) + FMath::Abs(dir.Y) + FMath::Abs(dir.Z) > 1)
	{
		if (dir.X != 0 && HasForcedNeighborsStraight(x, y, z, currentNodeLink, FIntVector(dir.X, 0, 0)))
			return true;
		if (dir.Y != 0 && HasForcedNeighborsStraight(x, y, z, currentNodeLink, FIntVector(0, dir.Y, 0)))
			return true;
		if (dir.Z != 0 && HasForcedNeighborsStraight(x, y, z, currentNodeLink, FIntVector(0, 0, dir.Z)))
			return true;
	}

	return false;
}

void AeonixJumpPointSearch::FindForcedNeighbors(uint_fast32_t x, uint_fast32_t y, uint_fast32_t z, const AeonixLink& currentNodeLink,
												const FIntVector& dir, TArray<FIntVector>& outForcedDirs) const
{
	// Implementation for finding forced neighbors with cross-boundary support
	if (dir.X != 0 && dir.Y == 0 && dir.Z == 0) // Moving along X axis
	{
		if (!IsValidPosition(x, y + 1, z, currentNodeLink) && IsValidPosition(x + dir.X, y + 1, z, currentNodeLink))
			outForcedDirs.Add(FIntVector(dir.X, 1, 0));
		if (!IsValidPosition(x, y - 1, z, currentNodeLink) && IsValidPosition(x + dir.X, y - 1, z, currentNodeLink))
			outForcedDirs.Add(FIntVector(dir.X, -1, 0));
		if (!IsValidPosition(x, y, z + 1, currentNodeLink) && IsValidPosition(x + dir.X, y, z + 1, currentNodeLink))
			outForcedDirs.Add(FIntVector(dir.X, 0, 1));
		if (!IsValidPosition(x, y, z - 1, currentNodeLink) && IsValidPosition(x + dir.X, y, z - 1, currentNodeLink))
			outForcedDirs.Add(FIntVector(dir.X, 0, -1));
	}
	else if (dir.Y != 0 && dir.X == 0 && dir.Z == 0) // Moving along Y axis
	{
		if (!IsValidPosition(x + 1, y, z, currentNodeLink) && IsValidPosition(x + 1, y + dir.Y, z, currentNodeLink))
			outForcedDirs.Add(FIntVector(1, dir.Y, 0));
		if (!IsValidPosition(x - 1, y, z, currentNodeLink) && IsValidPosition(x - 1, y + dir.Y, z, currentNodeLink))
			outForcedDirs.Add(FIntVector(-1, dir.Y, 0));
		if (!IsValidPosition(x, y, z + 1, currentNodeLink) && IsValidPosition(x, y + dir.Y, z + 1, currentNodeLink))
			outForcedDirs.Add(FIntVector(0, dir.Y, 1));
		if (!IsValidPosition(x, y, z - 1, currentNodeLink) && IsValidPosition(x, y + dir.Y, z - 1, currentNodeLink))
			outForcedDirs.Add(FIntVector(0, dir.Y, -1));
	}
	else if (dir.Z != 0 && dir.X == 0 && dir.Y == 0) // Moving along Z axis
	{
		if (!IsValidPosition(x + 1, y, z, currentNodeLink) && IsValidPosition(x + 1, y, z + dir.Z, currentNodeLink))
			outForcedDirs.Add(FIntVector(1, 0, dir.Z));
		if (!IsValidPosition(x - 1, y, z, currentNodeLink) && IsValidPosition(x - 1, y, z + dir.Z, currentNodeLink))
			outForcedDirs.Add(FIntVector(-1, 0, dir.Z));
		if (!IsValidPosition(x, y + 1, z, currentNodeLink) && IsValidPosition(x, y + 1, z + dir.Z, currentNodeLink))
			outForcedDirs.Add(FIntVector(0, 1, dir.Z));
		if (!IsValidPosition(x, y - 1, z, currentNodeLink) && IsValidPosition(x, y - 1, z + dir.Z, currentNodeLink))
			outForcedDirs.Add(FIntVector(0, -1, dir.Z));
	}
}

void AeonixJumpPointSearch::GetPrunedNeighbors(uint_fast32_t x, uint_fast32_t y, uint_fast32_t z, const AeonixLink& currentNodeLink,
											   const FIntVector& parentDir, TArray<FIntVector>& outDirs) const
{
	// If no parent direction, consider all directions
	if (parentDir == FIntVector::ZeroValue)
	{
		const FIntVector* directions = bAllowDiagonals ? AeonixStatics::allDirs26 : AeonixStatics::straightDirs;
		int numDirections = bAllowDiagonals ? AeonixStatics::NUM_ALL_DIRS : AeonixStatics::NUM_STRAIGHT_DIRS;

		for (int i = 0; i < numDirections; ++i)
		{
			if (IsValidPosition(x + directions[i].X, y + directions[i].Y, z + directions[i].Z, currentNodeLink))
			{
				outDirs.Add(directions[i]);
			}
		}
		return;
	}

	// Prune neighbors based on parent direction
	// Add the current direction
	if (IsValidPosition(x + parentDir.X, y + parentDir.Y, z + parentDir.Z, currentNodeLink))
	{
		outDirs.Add(parentDir);
	}

	// Add forced neighbors
	FindForcedNeighbors(x, y, z, currentNodeLink, parentDir, outDirs);
}