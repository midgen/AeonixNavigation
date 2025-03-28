#pragma once

#include "Actor/AeonixBoundingVolume.h"

#include <AeonixNavigation/Public/Interface/AeonixSubsystemInterface.h>
#include <AeonixNavigation/Public/Data/AeonixTypes.h>

#include <Runtime/MassEntity/Public/MassEntityTypes.h>
#include <Subsystems/EngineSubsystem.h>

#include "AeonixSubsystem.generated.h"

class AAeonixBoundingVolume;
class UAeonixNavAgentComponent;

USTRUCT()
struct FAeonixBoundingVolumeHandle
{
	GENERATED_BODY()

	FAeonixBoundingVolumeHandle(){}
	FAeonixBoundingVolumeHandle(AAeonixBoundingVolume* Volume, FMassEntityHandle& Handle) : VolumeHandle(Volume) , EntityHandle(Handle){}

	bool operator==(const FAeonixBoundingVolumeHandle& Volume ) const { return Volume.VolumeHandle == VolumeHandle; }
	
	UPROPERTY()
	TObjectPtr<AAeonixBoundingVolume> VolumeHandle;

	UPROPERTY()
	FMassEntityHandle EntityHandle;
};

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
	virtual void RegisterNavComponent(UAeonixNavAgentComponent* NavComponent) override;
	UFUNCTION()
	virtual void UnRegisterNavComponent(UAeonixNavAgentComponent* NavComponent) override;
	UFUNCTION()
	virtual const AAeonixBoundingVolume* GetVolumeForPosition(const FVector& Position) override;
	UFUNCTION()
	virtual bool FindPathImmediateAgent(UAeonixNavAgentComponent* NavigationComponent, const FVector& End, FAeonixNavigationPath& OutPath) override;
	UFUNCTION()
	virtual FAeonixPathFindRequestCompleteDelegate& FindPathAsyncAgent(UAeonixNavAgentComponent* NavigationComponent, const FVector& End, FAeonixNavigationPath& OutPath) override;
	UFUNCTION()
	virtual const AAeonixBoundingVolume* GetVolumeForAgent(const UAeonixNavAgentComponent* NavigationComponent) override;
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

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	UPROPERTY()
	TArray<FAeonixBoundingVolumeHandle> RegisteredVolumes{};

	UPROPERTY()
	TArray<UAeonixNavAgentComponent*> RegisteredNavComponents{};

	UPROPERTY()
	TMap<UAeonixNavAgentComponent*, const AAeonixBoundingVolume*> AgentToVolumeMap;

	void UpdateRequests();
	TArray<FAeonixPathFindRequest> PathRequests;
};
