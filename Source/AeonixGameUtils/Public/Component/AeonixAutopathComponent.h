#pragma once

#include "Pathfinding/AeonixNavigationPath.h"
#include "Data/AeonixTypes.h"
#include "Components/ActorComponent.h"

#include "AeonixAutopathComponent.generated.h"

class UAeonixAutopathSubsystem;
class UAeonixNavAgentComponent;

/** Delegate broadcast when path is updated (after async pathfinding completes) */
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAutopathUpdated, bool, bSuccess);

/**
 * Component that automatically pathfinds to a target actor.
 * The component does not tick itself - instead the UAeonixAutopathSubsystem
 * tracks all registered sources and triggers pathfinding when source or target moves.
 *
 * Requires a UAeonixNavAgentComponent on the same actor or its controller for pathfinding offsets.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class AEONIXGAMEUTILS_API UAeonixAutopathComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAeonixAutopathComponent(const FObjectInitializer& ObjectInitializer);

	/** Position threshold in cm - triggers pathfinding when source moved beyond this distance */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aeonix|Autopath", meta = (ClampMin = "0.0"))
	float PositionThreshold = 50.0f;

	/** Acceptance radius in cm - advances to next waypoint when within this distance */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aeonix|Autopath", meta = (ClampMin = "0.0"))
	float AcceptanceRadius = 50.0f;

	/** Whether this autopath source is active */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aeonix|Autopath")
	bool bEnableAutopath = true;

	/** Event broadcast when path is updated (after async pathfinding completes) */
	UPROPERTY(BlueprintAssignable, Category = "Aeonix|Autopath")
	FOnAutopathUpdated OnPathUpdated;

	/**
	 * Get the full path as an array of positions.
	 * Returns empty array if no path is available.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Aeonix|Autopath")
	TArray<FVector> GetPathPoints() const;

	/**
	 * Get the next path point to navigate towards.
	 * Automatically progresses through the path as the agent moves.
	 * Returns ZeroVector if no path is available.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Aeonix|Autopath")
	FVector GetNextPathPoint() const;

	/**
	 * Check if the agent has reached the final destination.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Aeonix|Autopath")
	bool HasReachedDestination() const;

	/**
	 * Get the current waypoint index in the path.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Aeonix|Autopath")
	int32 GetCurrentPathIndex() const { return CurrentPathIndex; }

	/**
	 * Check if a valid path exists.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Aeonix|Autopath")
	bool HasValidPath() const;

	/**
	 * Get the number of points in the current path.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Aeonix|Autopath")
	int32 GetNumPathPoints() const;

	/**
	 * Get the path point at a specific index.
	 * Returns false if index is out of bounds.
	 */
	UFUNCTION(BlueprintCallable, BlueprintPure, Category = "Aeonix|Autopath")
	bool GetPathPointAtIndex(int32 Index, FVector& OutPosition) const;

	/**
	 * Get the navigation path (internal use and subsystem access).
	 */
	FAeonixNavigationPath& GetNavigationPath() { return CurrentPath; }
	const FAeonixNavigationPath& GetNavigationPath() const { return CurrentPath; }

	/**
	 * Get the nav agent component on this actor (for offset access).
	 * Can be null if no nav agent component exists.
	 */
	UAeonixNavAgentComponent* GetNavAgentComponent() const { return CachedNavAgentComponent.Get(); }

	/** Called by subsystem when async pathfinding completes */
	UFUNCTION()
	void OnPathFindComplete(EAeonixPathFindStatus Status);

	/** Mark that a path request is pending (called by subsystem) */
	void SetPathRequestPending(bool bPending) { bPathRequestPending = bPending; }

	/** Check if a path request is pending */
	bool IsPathRequestPending() const { return bPathRequestPending; }

	/** Called by subsystem to update path progression based on current position */
	void UpdatePathProgression(const FVector& CurrentPosition);

	/** Reset path index to start of path */
	void ResetPathIndex();

protected:
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** Whether we're currently registered with the subsystem */
	bool bRegisteredWithSubsystem = false;

	/** Current navigation path */
	FAeonixNavigationPath CurrentPath{};

	/** Current waypoint index in the path */
	int32 CurrentPathIndex = 0;

	/** Cached reference to nav agent component on same actor */
	TWeakObjectPtr<UAeonixNavAgentComponent> CachedNavAgentComponent;

	/** Initialize and register with subsystem */
	void RegisterWithSubsystem();

	/** Unregister from subsystem */
	void UnregisterFromSubsystem();

	/** Cache the nav agent component reference */
	void CacheNavAgentComponent();

	/** Whether a path request is currently pending */
	bool bPathRequestPending = false;
};
