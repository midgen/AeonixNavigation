#pragma once

class IAeonixNavVolumeInterface;
struct AeonixLink;

/** Positions are in the volume's nav space (see IAeonixNavVolumeInterface::WorldToNav) */
class AEONIXNAVIGATION_API AeonixMediator
{
public:
	static bool GetLinkFromPosition(const FVector& aPosition, const IAeonixNavVolumeInterface& aVolume, AeonixLink& oLink);

	/** Voxel coordinate of a position on a layer. Returns false if the position is off the grid. A point exactly on the max face clamps to the last voxel. */
	static bool GetVolumeXYZ(const FVector& aPosition, const IAeonixNavVolumeInterface& aVolume, const int aLayer, FIntVector& oXYZ);
};
