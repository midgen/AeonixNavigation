// Copyright Notice

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Components/BillboardComponent.h"
#include "AeonixDebugFloodFillActor.generated.h"

/**
 * Debug actor that performs a flood fill visualization of the Aeonix navigation octree.
 * Visualizes octree connectivity from its location using debug drawing.
 */
UCLASS()
class AEONIXEDITOR_API AAeonixDebugFloodFillActor : public AActor
{
	GENERATED_BODY()

public:
	AAeonixDebugFloodFillActor();

	/** Billboard component for editor visibility */
	UPROPERTY()
	TObjectPtr<UBillboardComponent> BillboardComponent;

	// AActor interface
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void PostEditMove(bool bFinished) override;
	virtual void BeginDestroy() override;

	/** Maximum number of voxels to visit during flood fill */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aeonix Debug|Flood Fill", meta = (ClampMin = "1", ClampMax = "10000"))
	int32 MaxVoxelCount = 1000;

	/** Thickness of the debug connection lines */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aeonix Debug|Flood Fill", meta = (ClampMin = "0.1", ClampMax = "50.0"))
	float LineThickness = 5.0f;

	/** Automatically update flood fill when navigation is regenerated */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aeonix Debug|Flood Fill")
	bool bAutoUpdateOnRegeneration = true;

	/** Manually trigger clearing of the flood fill visualization */
	UFUNCTION(CallInEditor, Category = "Aeonix Debug|Flood Fill")
	void ClearVisualization();

	/** Performs the flood fill algorithm and visualizes the results */
	void PerformFloodFill();

	// AActor interface
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** Handler for when a bounding volume regenerates its navigation */
	void OnBoundingVolumeRegenerated(class AAeonixBoundingVolume* Volume);

	/** Bind to all bounding volumes in the level */
	void BindToBoundingVolumes();

	/** Unbind from all bounding volumes */
	void UnbindFromBoundingVolumes();
};
