// Copyright 2024 Chris Ashworth

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Volume.h"
#include "AeonixDynamicSubregion.generated.h"

/**
 *  Volume used to mark a region of an AeonixBoundingVolume as dynamic
 *  This will cause the entirety of the region to be marked as blocked for the first pass rasterise
 *  , meaning it will have leaf nodes allocated for the full region
 *  This means we can then update the blocking for the subregion without having to reconstruct
 *  the whole octree, and without any allocations
 */
UCLASS()
class AEONIXNAVIGATION_API AAeonixDynamicSubregion : public AVolume
{
	GENERATED_BODY()

	//~ Begin AActor Interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	//~ End AActor Interface
};
