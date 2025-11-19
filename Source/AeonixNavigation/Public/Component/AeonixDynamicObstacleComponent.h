#pragma once

#include "Interface/AeonixSubsystemInterface.h"
#include "Components/ActorComponent.h"
#include "Actor/AeonixBoundingVolume.h"

#include "AeonixDynamicObstacleComponent.generated.h"

/**
 * Component that tracks dynamic obstacles and triggers navigation regeneration
 * when the obstacle moves significantly or crosses dynamic region boundaries.
 *
 * The component does not tick itself - instead the UAeonixSubsystem ticks all
 * registered obstacles and checks for transform changes each frame.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class AEONIXNAVIGATION_API UAeonixDynamicObstacleComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAeonixDynamicObstacleComponent(const FObjectInitializer& ObjectInitializer);

	/** Position threshold in cm - triggers regeneration when moved beyond this distance */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aeonix|Obstacle", meta = (ClampMin = "0.0"))
	float PositionThreshold = 50.0f;

	/** Rotation threshold in degrees - triggers regeneration when rotated beyond this angle */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aeonix|Obstacle", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float RotationThreshold = 15.0f;

	/** Whether this obstacle should trigger navigation regeneration (can be disabled temporarily) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aeonix|Obstacle")
	bool bEnableNavigationRegen = true;

	/**
	 * Manually trigger navigation regeneration for all regions this obstacle is currently inside.
	 * Bypasses position/rotation thresholds.
	 */
	UFUNCTION(BlueprintCallable, CallInEditor, Category = "Aeonix|Obstacle")
	void TriggerNavigationRegen();

	/**
	 * Get the set of dynamic region IDs this obstacle is currently inside.
	 */
	const TSet<FGuid>& GetCurrentRegionIds() const { return CurrentDynamicRegionIds; }

	/**
	 * Set the current region IDs (called by subsystem).
	 */
	void SetCurrentRegionIds(const TSet<FGuid>& NewRegionIds) { CurrentDynamicRegionIds = NewRegionIds; }

	/**
	 * Get the bounding volume this obstacle is currently inside (can be null).
	 */
	AAeonixBoundingVolume* GetCurrentBoundingVolume() const { return CurrentBoundingVolume.Get(); }

	/**
	 * Set the current bounding volume (called by subsystem).
	 */
	void SetCurrentBoundingVolume(AAeonixBoundingVolume* Volume) { CurrentBoundingVolume = Volume; }

protected:
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

private:
	/** Whether we're currently registered with the subsystem */
	bool bRegisteredWithSubsystem = false;

	/** Initialize and register with subsystem */
	void RegisterWithSubsystem();

	/** Unregister from subsystem */
	void UnregisterFromSubsystem();

	/** Set of dynamic region IDs this obstacle is currently inside */
	TSet<FGuid> CurrentDynamicRegionIds;

	/** Bounding volume this obstacle is currently inside */
	TWeakObjectPtr<AAeonixBoundingVolume> CurrentBoundingVolume;
};
