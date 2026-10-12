#pragma once

#include "UObject/Interface.h"

#include "AeonixNavVolumeInterface.generated.h"

class IAeonixNavVolumeInterface;
struct FAeonixData;
struct FAeonixGenerationParameters;

/** Broadcast when a volume's navigation data is regenerated (full or dynamic regions) */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnAeonixNavVolumeRegenerated, IAeonixNavVolumeInterface*);

UINTERFACE(MinimalAPI, NotBlueprintable, meta = (CannotImplementInterfaceInBlueprint))
class UAeonixNavVolumeInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 *  A navigable volume: owns octree navigation data and the frame it lives in.
 *
 *  Navigation data is stored in "nav space". For AAeonixBoundingVolume nav space is world space
 *  (GetNavToWorld() is identity). For UAeonixVolumeComponent nav space is the component's own
 *  frame, so the data stays valid while the component moves and rotates. Anything that feeds
 *  world positions into FAeonixData, or reads positions out of it, converts with
 *  WorldToNav / NavToWorld.
 */
class AEONIXNAVIGATION_API IAeonixNavVolumeInterface
{
	GENERATED_BODY()

public:
	virtual const FAeonixData& GetNavData() const = 0;
	virtual FAeonixData& GetMutableNavData() = 0;

	/** Read-write lock guarding the octree for async pathfinding */
	FRWLock& GetOctreeDataLock() const { return *GetSharedOctreeDataLock(); }

	/**
	 * The same lock, shared so an async pathfind can hold it past the volume. A worker re-checks its request for
	 * cancellation after taking it; UAeonixSubsystem::CancelPathRequestsForVolume cancels and drains a volume's
	 * requests before it unregisters or regenerates, so no worker reads a volume that is gone or rebuilt.
	 */
	virtual TSharedRef<FRWLock, ESPMode::ThreadSafe> GetSharedOctreeDataLock() const = 0;

	virtual bool HasData() const = 0;
	virtual bool IsReadyForNavigation() const = 0;

	/** True if a world position lies inside the volume */
	virtual bool IsPointInside(const FVector& WorldPosition) const = 0;

	/** Unscaled transform from nav space to world space, evaluated now */
	virtual FTransform GetNavToWorld() const = 0;

	virtual const FAeonixGenerationParameters& GetGenerationParameters() const = 0;

	virtual FOnAeonixNavVolumeRegenerated& GetOnNavigationRegenerated() = 0;

	virtual bool Generate() = 0;
	virtual void ClearData() = 0;

	/** Dynamic regions (modifier volumes, dynamic obstacles) are world-space and only supported by static volumes */
	virtual bool SupportsDynamicRegions() const { return false; }

	FVector WorldToNav(const FVector& WorldPosition) const { return GetNavToWorld().InverseTransformPosition(WorldPosition); }
	FVector NavToWorld(const FVector& NavPosition) const { return GetNavToWorld().TransformPosition(NavPosition); }

	UObject* GetVolumeObject() const;
	FString GetVolumeName() const;
};
