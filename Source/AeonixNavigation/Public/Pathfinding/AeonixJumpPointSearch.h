#pragma once

#include <AeonixNavigation/Public/Data/AeonixLeafNode.h>
#include <AeonixNavigation/Public/Data/AeonixLink.h>
#include <AeonixNavigation/Public/Data/AeonixDefines.h>
#include <AeonixNavigation/Public/Data/AeonixData.h>

class AEONIXNAVIGATION_API AeonixJumpPointSearch
{
public:
	AeonixJumpPointSearch(const FAeonixData& InNavigationData, bool bInAllowDiagonals)
		: NavigationData(InNavigationData)
		, bAllowDiagonals(bInAllowDiagonals) {}

	// Find jump points from a given position in the specified direction
	void FindJumpPoints(uint_fast32_t startX, uint_fast32_t startY, uint_fast32_t startZ,
					   const AeonixLink& currentLink, TArray<AeonixLink>& outJumpPoints) const;

	// Check if a position is a jump point
	bool IsJumpPoint(uint_fast32_t x, uint_fast32_t y, uint_fast32_t z, const AeonixLink& currentNodeLink,
					 const FIntVector& dir, const FIntVector& parentDir) const;

	// Find forced neighbors at a given position
	void FindForcedNeighbors(uint_fast32_t x, uint_fast32_t y, uint_fast32_t z, const AeonixLink& currentNodeLink,
							const FIntVector& dir, TArray<FIntVector>& outForcedDirs) const;

private:
	const FAeonixData& NavigationData;
	bool bAllowDiagonals;

	// Check if a position is valid and not blocked (can span multiple leaf nodes)
	bool IsValidPosition(int32 x, int32 y, int32 z, const AeonixLink& currentNodeLink) const;

	// Get the leaf node and local coordinates for a global position
	bool GetLeafNodeAtPosition(int32 x, int32 y, int32 z, const AeonixLink& currentNodeLink,
							  AeonixLink& outNodeLink, uint_fast32_t& outLocalX, uint_fast32_t& outLocalY, uint_fast32_t& outLocalZ) const;

	// Jump in a specific direction until hitting an obstacle or finding a jump point (iterative)
	bool Jump(uint_fast32_t& x, uint_fast32_t& y, uint_fast32_t& z, AeonixLink& currentNodeLink,
			  const FIntVector& dir, const FIntVector& parentDir) const;

	// Check for forced neighbors in straight line movement
	bool HasForcedNeighborsStraight(uint_fast32_t x, uint_fast32_t y, uint_fast32_t z,
								   const AeonixLink& currentNodeLink, const FIntVector& dir) const;

	// Check for forced neighbors in diagonal movement
	bool HasForcedNeighborsDiagonal(uint_fast32_t x, uint_fast32_t y, uint_fast32_t z,
								   const AeonixLink& currentNodeLink, const FIntVector& dir) const;

	// Get pruned neighbors based on parent direction
	void GetPrunedNeighbors(uint_fast32_t x, uint_fast32_t y, uint_fast32_t z, const AeonixLink& currentNodeLink,
						   const FIntVector& parentDir, TArray<FIntVector>& outDirs) const;
};