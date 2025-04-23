#pragma once

#include <AeonixNavigation/Public/Data/AeonixTypes.h>
#include <AeonixNavigation/Public/Interface/AeonixSubsystemInterface.h>
#include <AeonixNavigation/Public/Data/AeonixHandleTypes.h>

#include <Subsystems/EngineSubsystem.h>

#include "AeonixSubsystem.generated.h"

class AAeonixBoundingVolume;
class UAeonixNavAgentComponent;


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
	virtual void RegisterDynamicSubregion(AAeonixDynamicSubregion* DynamicSubregion) override;
	UFUNCTION()
	virtual void UnRegisterDynamicSubregion(AAeonixDynamicSubregion* DynamicSubregion) override;
	UFUNCTION()
	virtual void RegisterNavComponent(UAeonixNavAgentComponent* NavComponent, EAeonixMassEntityFlag bCreateMassEntity) override;
	UFUNCTION()
	virtual void UnRegisterNavComponent(UAeonixNavAgentComponent* NavComponent, EAeonixMassEntityFlag bDestroyMassEntity) override;
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

	void UpdateRequests();

private:
	TArray<FAeonixPathFindRequest> PathRequests;
};

