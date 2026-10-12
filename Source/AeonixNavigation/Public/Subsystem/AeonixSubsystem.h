#pragma once

#include "Data/AeonixTypes.h"
#include "Data/AeonixThreading.h"
#include "Interface/AeonixSubsystemInterface.h"
#include "Data/AeonixHandleTypes.h"

#include "UObject/WeakInterfacePtr.h"

#include "Subsystems/EngineSubsystem.h"

#include "AeonixSubsystem.generated.h"

class AAeonixBoundingVolume;
class AAeonixModifierVolume;
class IAeonixNavVolumeInterface;
struct FAeonixPathFinderSettings;
class UAeonixNavAgentComponent;
class UAeonixDynamicObstacleComponent;


UCLASS()
class AEONIXNAVIGATION_API UAeonixSubsystem : public UTickableWorldSubsystem, public IAeonixSubsystemInterface
{
	GENERATED_BODY()

public:
	/* IAeonixSubsystemInterface BEGIN */
	virtual void RegisterVolume(IAeonixNavVolumeInterface* Volume) override;
	virtual void UnRegisterVolume(IAeonixNavVolumeInterface* Volume) override;
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
	virtual const IAeonixNavVolumeInterface* GetVolumeForPosition(const FVector& Position) override;
	UFUNCTION()
	virtual bool FindPathImmediateAgent(UAeonixNavAgentComponent* NavigationComponent, const FVector& End, FAeonixNavigationPath& OutPath) override;
	UFUNCTION()
	virtual FAeonixPathFindRequestCompleteDelegate& FindPathAsyncAgent(UAeonixNavAgentComponent* NavigationComponent, const FVector& End, FAeonixNavigationPath& OutPath) override;
	virtual const IAeonixNavVolumeInterface* GetVolumeForAgent(const UAeonixNavAgentComponent* NavigationComponent) override;
	virtual IAeonixNavVolumeInterface* GetMutableVolumeForAgent(const UAeonixNavAgentComponent* NavigationComponent) override;
	UFUNCTION()
	virtual void UpdateComponents() override;
	virtual FOnNavigationRegenCompleted& GetOnNavigationRegenCompleted() override { return OnNavigationRegenCompleted; }
	virtual FOnRegistrationChanged& GetOnRegistrationChanged() override { return OnRegistrationChanged; }
	virtual void RequestDebugPathUpdate(UAeonixNavAgentComponent* NavComponent) override;
	/* IAeonixSubsystemInterface END */

	/**
	 * Synchronous pathfind between two positions in a volume's nav space, with no agent.
	 * Path points come back in nav space (FAeonixPathPoint::NavPosition) and in world space as of now (Position).
	 * Use this to path agents that move in a moving volume's own frame.
	 */
	bool FindPathInVolume(const IAeonixNavVolumeInterface& Volume, const FVector& NavStart, const FVector& NavEnd, const FAeonixPathFinderSettings& Settings, FAeonixNavigationPath& OutPath);

	/**
	 * Cancels a volume's queued and in-flight async pathfinds and waits for any worker reading its octree.
	 * Call before the octree is rebuilt or the volume goes away; UnRegisterVolume does.
	 */
	void CancelPathRequestsForVolume(const IAeonixNavVolumeInterface& Volume);

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

	// Keys are compared, never dereferenced; RegisteredNavAgents owns agent lifetime
	TMap<const UAeonixNavAgentComponent*, TWeakInterfacePtr<IAeonixNavVolumeInterface>> AgentToVolumeMap;

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
	void OnBoundingVolumeRegenerated(IAeonixNavVolumeInterface* Volume);

	/** Update debug paths for all nav agents within a specific volume */
	void UpdateDebugPathsForVolume(IAeonixNavVolumeInterface* Volume);

	/** Track which dynamic regions a path traverses through */
	void TrackPathRegions(FAeonixNavigationPath& Path, const IAeonixNavVolumeInterface* Volume);

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
	bool TryAcquirePathfindReadLock(const IAeonixNavVolumeInterface* Volume, float TimeoutSeconds = 0.1f);

public:
	// Region versioning API
	uint32 GetRegionVersion(const FGuid& RegionId) const;
	void IncrementRegionVersion(const FGuid& RegionId);
};

