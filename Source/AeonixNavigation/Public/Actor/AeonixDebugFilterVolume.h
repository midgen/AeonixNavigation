// Copyright Chris Kang. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Volume.h"
#include "AeonixDebugFilterVolume.generated.h"

/**
 * Volume that filters leaf voxel debug rendering to only show voxels inside the volume.
 * When placed inside an AAeonixBoundingVolume, it will restrict debug visualization
 * of leaf voxels to only those contained within this filter volume's bounds.
 *
 * This is a static filter - it registers once and is assumed to not move during generation.
 */
UCLASS(hidecategories = (Tags, Cooking, Actor, HLOD, Mobile, LOD))
class AEONIXNAVIGATION_API AAeonixDebugFilterVolume : public AVolume
{
	GENERATED_BODY()

public:
	AAeonixDebugFilterVolume(const FObjectInitializer& ObjectInitializer);

	//~ Begin AActor Interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void Destroyed() override;
#if WITH_EDITOR
	virtual void PostEditMove(bool bFinished) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
	//~ End AActor Interface

private:
	/** Register this volume with bounding volumes it's inside */
	void RegisterWithBoundingVolumes();

	/** Unregister this volume from bounding volumes */
	void UnregisterFromBoundingVolumes();
};
