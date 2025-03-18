#pragma once

class AAeonixBoundingVolume;
struct AeonixLink;

class AeonixMediator
{
public:
	static bool GetLinkFromPosition(const FVector& aPosition, const AAeonixBoundingVolume& aVolume, AeonixLink& oLink);

	static void GetVolumeXYZ(const FVector& aPosition, const AAeonixBoundingVolume& aVolume, const int aLayer, FIntVector& oXYZ);
};