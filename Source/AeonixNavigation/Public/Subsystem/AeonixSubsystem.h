#pragma once

#include <AeonixNavigation/Public/Interface/AeonixSubsystemInterface.h>

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
	virtual void RegisterVolume(const AAeonixBoundingVolume* Volume) override;
	UFUNCTION()
	virtual void UnRegisterVolume(const AAeonixBoundingVolume* Volume) override;
	UFUNCTION()
	virtual void RegisterNavComponent(UAeonixNavAgentComponent* NavComponent) override;
	UFUNCTION()
	virtual void UnRegisterNavComponent(UAeonixNavAgentComponent* NavComponent) override;
	UFUNCTION()
	virtual const AAeonixBoundingVolume* GetVolumeForPosition(const FVector& Position) override;
	UFUNCTION()
	virtual bool FindPathImmediateAgent(UAeonixNavAgentComponent* NavigationComponent, const FVector& End, FAeonixNavigationPath& OutPath) override;
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

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	UPROPERTY()
	TArray<const AAeonixBoundingVolume*> RegisteredVolumes{};

	UPROPERTY()
	TArray<UAeonixNavAgentComponent*> RegisteredNavComponents{};

	UPROPERTY()
	TMap<UAeonixNavAgentComponent*, const AAeonixBoundingVolume*> AgentToVolumeMap;
};