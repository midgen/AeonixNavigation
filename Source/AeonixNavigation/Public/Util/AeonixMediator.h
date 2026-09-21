#pragma once

class AAeonixBoundingVolume;
struct AeonixLink;

class AEONIXNAVIGATION_API AeonixMediator
{
public:
	static bool GetLinkFromPosition(const FVector& aPosition, const AAeonixBoundingVolume& aVolume, AeonixLink& oLink);

	/** Voxel coordinate of a position on a layer. Returns false if the position is off the grid. A point exactly on the max face clamps to the last voxel. */
	static bool GetVolumeXYZ(const FVector& aPosition, const AAeonixBoundingVolume& aVolume, const int aLayer, FIntVector& oXYZ);
};