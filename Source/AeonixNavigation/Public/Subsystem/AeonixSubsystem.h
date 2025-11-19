#pragma once

#include "Data/AeonixTypes.h"
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
	virtual void RegisterVolume(AAeonixBoundingVolume* Volume, EAeonixMassEntityFlag bCreateMassEntity) override;
	UFUNCTION()
	virtual void UnRegisterVolume(AAeonixBoundingVolume* Volume, EAeonixMassEntityFlag bDestroyMassEntity) override;
	UFUNCTION()
	virtual void RegisterModifierVolume(AAeonixModifierVolume* ModifierVolume) override;
	UFUNCTION()
	virtual void UnRegisterModifierVolume(AAeonixModifierVolume* ModifierVolume) override;
	UFUNCTION()
	virtual void RegisterNavComponent(UAeonixNavAgentComponent* NavComponent, EAeonixMassEntityFlag bCreateMassEntity) override;
	UFUNCTION()
	virtual void UnRegisterNavComponent(UAeonixNavAgentComponent* NavComponent, EAeonixMassEntityFlag bDestroyMassEntity) override;
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
	
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	
	virtual bool IsTickable() const override;
	virtual bool IsTickableInEditor() const override;
	virtual bool IsTickableWhenPaused() const override;

	void CompleteAllPendingPathfindingTasks();
	size_t GetNumberOfPendingTasks() const;
	size_t GetNumberOfRegisteredNavAgents() const;
	size_t GetNumberOfRegisteredNavVolumes() const;

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

	// Tracks the last transform for each dynamic obstacle (for threshold checking)
	TMap<UAeonixDynamicObstacleComponent*, FTransform> ObstacleLastTransformMap;

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

	/** Delegate broadcast when navigation regeneration completes */
	FOnNavigationRegenCompleted OnNavigationRegenCompleted;

	/** Delegate broadcast when registration changes */
	FOnRegistrationChanged OnRegistrationChanged;

private:
	TArray<FAeonixPathFindRequest> PathRequests;
};

