#pragma once

#include "Data/AeonixData.h"
#include "Interface/AeonixDebugDrawInterface.h"
#include "Interface/AeonixSubsystemInterface.h"

#include "GameFramework/Volume.h"

#include "AeonixBoundingVolume.generated.h"

// Forward declaration
class AAeonixBoundingVolume;

/** Delegate broadcast when navigation is regenerated (full or dynamic subregions) */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnNavigationRegenerated, AAeonixBoundingVolume*);

/**
 *  AeonixVolume is a bounding volume that forms a navigable area
 */
UCLASS(hidecategories = (Tags, Cooking, Actor, HLOD, Mobile, LOD))
class AEONIXNAVIGATION_API AAeonixBoundingVolume : public AVolume, public IAeonixDebugDrawInterface
{
	GENERATED_BODY()

public:

	AAeonixBoundingVolume(const FObjectInitializer& ObjectInitializer);

	//~ Begin AActor Interface
	void BeginPlay() override;
	void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	bool ShouldTickIfViewportsOnly() const override { return true; }
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Destroyed() override;
	//~ End AActor Interface
	
	//~ Begin UObject 
	void Serialize(FArchive& Ar) override;
	//~ End UObject 

	void UpdateBounds();
	bool Generate();
	void RegenerateDynamicSubregions();
	void RegenerateDynamicSubregionsAsync();
	void RegenerateDynamicSubregion(const FGuid& RegionId);
	void RegenerateDynamicSubregionAsync(const FGuid& RegionId);
	void RegenerateDynamicSubregionsAsync(const TSet<FGuid>& RegionIds);
	bool HasData() const;
	void ClearData();

	// Called by editor subsystem to set the debug filter box
	void SetDebugFilterBox(const FBox& FilterBox);
	void ClearDebugFilterBox();

	// Called by modifier volumes to register dynamic regions
	void AddDynamicRegion(const FGuid& RegionId, const FBox& RegionBox);
	void RemoveDynamicRegion(const FGuid& RegionId);
	void ClearDynamicRegions();

	// Validate that loaded dynamic regions match modifier volumes in the level
	void ValidateDynamicRegions();

	// Called by dynamic obstacles to request regeneration (throttled)
	void RequestDynamicRegionRegen(const FGuid& RegionId);
	void TryProcessDirtyRegions();

	const FAeonixData& GetNavData() const { return NavigationData; }
	FAeonixData& GetMutableNavData() { return NavigationData; }

	/** Get the read-write lock for thread-safe octree access */
	FRWLock& GetOctreeDataLock() const { return OctreeDataLock; }

	/** Delegate broadcast when navigation is regenerated (full or dynamic subregions) */
	FOnNavigationRegenerated OnNavigationRegenerated;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aeonix")
	FAeonixGenerationParameters GenerationParameters;

	/** Minimum time between dynamic region regenerations (seconds) */
	UPROPERTY(EditAnywhere, Category = "Aeonix|Dynamic", meta = (ClampMin = "0.0"))
	float DynamicRegenCooldown = 0.5f;

	/** Delay after marking a region dirty before processing it at runtime (allows physics to settle) */
	UPROPERTY(EditAnywhere, Category = "Aeonix|Dynamic", meta = (ClampMin = "0.0", ClampMax = "2.0"))
	float DirtyRegionProcessDelay = 0.25f;

	/** Delay after marking a region dirty before processing it in editor (longer to allow editor overhead) */
	UPROPERTY(EditAnywhere, Category = "Aeonix|Dynamic", meta = (ClampMin = "0.0", ClampMax = "5.0"))
	float EditorDirtyRegionProcessDelay = 1.0f;

	bool bIsReadyForNavigation{false};

private:
	// Flag to indicate that old format baked data was loaded and needs bounds update in BeginPlay
	bool bNeedsLegacyBoundsUpdate{false};

	/** Read-write lock for thread-safe access to octree data during async pathfinding */
	mutable FRWLock OctreeDataLock;

	/** Dirty regions awaiting regeneration (throttling) */
	TSet<FGuid> DirtyRegionIds;

	/** Time when each region was marked dirty (used for processing delay) */
	TMap<FGuid, double> DirtyRegionTimestamps;

	/** Time of last dynamic region regeneration */
	double LastDynamicRegenTime = 0.0;

protected:
	FAeonixData NavigationData;

	UPROPERTY(Transient)
	TScriptInterface<IAeonixSubsystemInterface> AeonixSubsystemInterface;
	UPROPERTY(Transient)
	TScriptInterface<IAeonixCollisionQueryInterface> CollisionQueryInterface;

	// IAeonixDebugDrawInterface
	void AeonixDrawDebugString(const FVector& Position, const FString& String, const FColor& Color) const override;
	void AeonixDrawDebugBox(const FVector& Position, const float Size, const FColor& Color) const override;
	void AeonixDrawDebugLine(const FVector& Start, const FVector& End, const FColor& Color, float Thickness = 0.0f) const override;
	void AeonixDrawDebugDirectionalArrow(const FVector& Start, const FVector& End, const FColor& Color, float ArrowSize = 0.0f) const override;
};
