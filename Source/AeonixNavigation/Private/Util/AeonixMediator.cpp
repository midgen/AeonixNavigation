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

	// The lookup lives on the nav data so it can be used without a volume actor
	// (pathfinder line-of-sight checks, async tasks, tests). It checks against the bounds
	// the octree was generated with rather than the actor's live bounds (issue #54).
	return aVolume.GetNavData().GetLinkForPosition(aPosition, oLink);
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
