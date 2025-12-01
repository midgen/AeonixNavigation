#pragma once

#include "Data/AeonixTypes.h"
#include "Data/AeonixThreading.h"
#include "Interface/AeonixSubsystemInterface.h"
#include "Data/AeonixHandleTypes.h"

#include "Subsystems/EngineSubsystem.h"

#include "AeonixSubsystem.generated.h"

class AAeonixBoundingVolume;
class AAeonixModifierVolume;
class UAeonixNavAgentComponent;
class UAeonixDynamicObstacleComponent;


UCLASS()
class AEONIXNAVIGATION_API UAeonixSubsystem : public UTickableWorldSubsystem, public IAeonixSubsystemInterface
{
	GENERATED_BODY()

public:
	/* IAeonixSubsystemInterface BEGIN */
	UFUNCTION()
	virtual void RegisterVolume(AAeonixBoundingVolume* Volume) override;
	UFUNCTION()
	virtual void UnRegisterVolume(AAeonixBoundingVolume* Volume) override;
	UFUNCTION()
	virtual void RegisterModifierVolume(AAeonixModifierVolume* ModifierVolume) override;
	UFUNCTION()
	virtual void UnRegisterModifierVolume(AAeonixModifierVolume* ModifierVolume) override;
	UFUNCTION()
	virtual void RegisterNavComponent(UAeonixNavAgentComponent* NavComponent) override;
	UFUNCTION()
	virtual void UnRegisterNavComponent(UAeonixNavAgentComponent* NavComponent) override;
	UFUNCTION()
	virtual void RegisterDynamicObstacle(UAeonixDynamicObstacleComponent* ObstacleComponent) override;
	UFUNCTION()
	virtual void UnRegisterDynamicObstacle(UAeonixDynamicObstacleComponent* ObstacleComponent) override;
	UFUNCTION()
	virtual const AAeonixBoundingVolume* GetVolumeForPosition(const FVector& Position) override;
	UFUNCTION()
	virtual bool FindPathImmediateAgent(UAeonixNavAgentComponent* NavigationComponent, const FVector& End, FAeonixNavigationPath& OutPath) override;
	UFUNCTION()
	virtual FAeonixPathFindRequestCompleteDelegate& FindPathAsyncAgent(UAeonixNavAgentComponent* NavigationComponent, const FVector& End, FAeonixNavigationPath& OutPath) override;
	UFUNCTION()
	virtual const AAeonixBoundingVolume* GetVolumeForAgent(const UAeonixNavAgentComponent* NavigationComponent) override;
	UFUNCTION()
	virtual AAeonixBoundingVolume* GetMutableVolumeForAgent(const UAeonixNavAgentComponent* NavigationComponent) override;
	UFUNCTION()
	virtual void UpdateComponents() override;
	virtual FOnNavigationRegenCompleted& GetOnNavigationRegenCompleted() override { return OnNavigationRegenCompleted; }
	virtual FOnRegistrationChanged& GetOnRegistrationChanged() override { return OnRegistrationChanged; }
	virtual void RequestDebugPathUpdate(UAeonixNavAgentComponent* NavComponent) override;
	/* IAeonixSubsystemInterface END */

	// Path invalidation registry
	void RegisterPath(TSharedPtr<FAeonixNavigationPath> Path);
	void UnregisterPath(FAeonixNavigationPath* Path);
	void InvalidatePathsInRegions(const TSet<FGuid>& RegeneratedRegionIds);

	// Component-based path tracking (for invalidation)
	void RegisterComponentWithPath(UAeonixNavAgentComponent* Component);
	void UnregisterComponentWithPath(UAeonixNavAgentComponent* Component);

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	virtual bool IsTickable() const override;
	virtual bool IsTickableInEditor() const override;
	virtual bool IsTickableWhenPaused() const override;

	void CompleteAllPendingPathfindingTasks();
	size_t GetNumberOfPendingTasks() const;
	size_t GetNumberOfRegisteredNavAgents() const;
	size_t GetNumberOfRegisteredNavVolumes() const;

	// Access to registered volumes for debug UI
	const TArray<FAeonixBoundingVolumeHandle>& GetRegisteredVolumes() const { return RegisteredVolumes; }
	const TArray<UAeonixDynamicObstacleComponent*>& GetRegisteredDynamicObstacles() const { return RegisteredDynamicObstacles; }

	// Load metrics and monitoring
	const FAeonixLoadMetrics& GetLoadMetrics() const { return LoadMetrics; }
	FAeonixLoadMetrics& GetLoadMetrics() { return LoadMetrics; }
	int32 GetNumWorkerThreads() const { return WorkerPool.GetNumWorkers(); }
	void RequeuePathfindRequest(TUniquePtr<FAeonixPathFindRequest>&& Request, float DelaySeconds = 0.05f);

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	UPROPERTY(Transient)
	TArray<FAeonixBoundingVolumeHandle> RegisteredVolumes{};

	UPROPERTY(Transient)
	TArray<FAeonixNavAgentHandle> RegisteredNavAgents{};

	UPROPERTY(Transient)
	TMap<UAeonixNavAgentComponent*, const AAeonixBoundingVolume*> AgentToVolumeMap;

	UPROPERTY(Transient)
	TArray<UAeonixDynamicObstacleComponent*> RegisteredDynamicObstacles{};

	// Tracks the last transform for each dynamic obstacle's owner actor (for threshold checking)
	// Keyed by Actor because components can be recreated during editor moves
	TMap<AActor*, FTransform> ObstacleLastTransformMap;

	UPROPERTY(Transient)
	TArray<AAeonixModifierVolume*> RegisteredModifierVolumes{};

	// Tracks which bounding volume each modifier volume is currently registered with
	UPROPERTY(Transient)
	TMap<AAeonixModifierVolume*, AAeonixBoundingVolume*> ModifierToVolumeMap;

	void UpdateRequests();
	void ProcessDynamicObstacles(float DeltaTime);
	void UpdateSpatialRelationships();

	/** Handler for when a bounding volume completes regeneration */
	void OnBoundingVolumeRegenerated(AAeonixBoundingVolume* Volume);

	/** Update debug paths for all nav agents within a specific volume */
	void UpdateDebugPathsForVolume(AAeonixBoundingVolume* Volume);

	/** Track which dynamic regions a path traverses through */
	void TrackPathRegions(FAeonixNavigationPath& Path, const AAeonixBoundingVolume* BoundingVolume);

	/** Delegate broadcast when navigation regeneration completes */
	FOnNavigationRegenCompleted OnNavigationRegenCompleted;

	/** Delegate broadcast when registration changes */
	FOnRegistrationChanged OnRegistrationChanged;

	// Path invalidation tracking
	FCriticalSection PathRegistryLock;
	TSet<TWeakPtr<FAeonixNavigationPath>> ActivePaths;

	// Component-based path tracking (for invalidation via dynamic regions)
	FCriticalSection ComponentPathRegistryLock;
	TSet<TWeakObjectPtr<UAeonixNavAgentComponent>> ComponentsWithPaths;

	// Threading infrastructure
	FAeonixPathfindWorkerPool WorkerPool;
	FAeonixLoadMetrics LoadMetrics;
	FCriticalSection PathRequestsLock;
	int32 MaxConcurrentPathfinds = 8; // Configurable limit

	// Priority-based request queue (sorted by priority, then FIFO within priority)
	TArray<TUniquePtr<FAeonixPathFindRequest>> PathRequests;

	// Region versioning for invalidation detection
	TMap<FGuid, uint32> RegionVersionMap;
	mutable FCriticalSection RegionVersionLock;

	// Helper methods
	bool TryAcquirePathfindReadLock(const AAeonixBoundingVolume* Volume, float TimeoutSeconds = 0.1f);

public:
	// Region versioning API
	uint32 GetRegionVersion(const FGuid& RegionId) const;
	void IncrementRegionVersion(const FGuid& RegionId);
};

