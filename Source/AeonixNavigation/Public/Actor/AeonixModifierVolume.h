// Copyright Chris Kang. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Volume.h"
#include "AeonixModifierVolume.generated.h"

/**
 * Modifier type flags for AeonixModifierVolume
 */
UENUM(BlueprintType, meta = (Bitflags, UseEnumValuesAsMaskValuesInEditor = "true"))
enum class EAeonixModifierType : uint8
{
	None = 0,
	DebugFilter = 1 << 0 UMETA(DisplayName = "Debug Filter", ToolTip = "Only leaf voxels inside this volume will be debug rendered"),
	DynamicRegion = 1 << 1 UMETA(DisplayName = "Dynamic Region", ToolTip = "Voxels in this region can be updated at runtime without full regeneration"),
	// Future modifiers can be added here
};
ENUM_CLASS_FLAGS(EAeonixModifierType)

/**
 * Volume that modifies Aeonix navigation behavior within its bounds.
 * Supports multiple modifier types through flags:
 * - DebugFilter: Filters leaf voxel debug rendering to only show voxels inside this volume
 * - DynamicRegion: Marks voxels for runtime updates without full regeneration
 *
 * Multiple modifier volumes can be used per bounding volume.
 * Modifier volumes should be placed before generation for proper leaf node allocation.
 */
UCLASS(hidecategories = (Tags, Cooking, Actor, HLOD, Mobile, LOD))
class AEONIXNAVIGATION_API AAeonixModifierVolume : public AVolume
{
	GENERATED_BODY()

public:
	AAeonixModifierVolume(const FObjectInitializer& ObjectInitializer);

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

	/** Modifier types active in this volume */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aeonix", meta = (Bitmask, BitmaskEnum = "/Script/AeonixNavigation.EAeonixModifierType"))
	int32 ModifierTypes = static_cast<int32>(EAeonixModifierType::None);

	/** Unique ID for this dynamic region (used for selective regeneration) */
	UPROPERTY(SaveGame)
	FGuid DynamicRegionId;

private:
	/** Register this volume with bounding volumes it's inside */
	void RegisterWithBoundingVolumes();

	/** Unregister this volume from bounding volumes */
	void UnregisterFromBoundingVolumes();
};
