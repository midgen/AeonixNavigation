#pragma once

#include <AeonixNavigation/Public/Interface/AeonixSubsystemInterface.h>

#include <Subsystems/EngineSubsystem.h>

#include "AeonixSubsystem.generated.h"

class AAeonixBoundingVolume;
class UAeonixNavigationComponent;

UCLASS()
class AEONIXNAVIGATION_API UAeonixSubsystem : public UEngineSubsystem, public IAeonixSubsystemInterface, public FTickableGameObject
{
	GENERATED_BODY()

public:
	/* IAeonixSubsystemInterface BEGIN */
	UFUNCTION()
	virtual void RegisterVolume(const AAeonixBoundingVolume* Volume) override;
	UFUNCTION()
	virtual void UnRegisterVolume(const AAeonixBoundingVolume* Volume) override;
	UFUNCTION()
	virtual void RegisterNavComponent(UAeonixNavigationComponent* NavComponent) override;
	UFUNCTION()
	virtual void UnRegisterNavComponent(UAeonixNavigationComponent* NavComponent) override;
	UFUNCTION()
	virtual const AAeonixBoundingVolume* GetVolumeForPosition(const FVector& Position) override;
	//UFUNCTION()
	//virtual bool FindPathImmediatePosition(const FVector& Start, const FVector& End, FAeonixNavigationPath& OutPath) override;
	UFUNCTION()
	virtual bool FindPathImmediateAgent(UAeonixNavigationComponent* NavigationComponent, const FVector& End, FAeonixNavigationPath& OutPath) override;
	UFUNCTION()
	virtual const AAeonixBoundingVolume* GetVolumeForAgent(const UAeonixNavigationComponent* NavigationComponent) override;
	UFUNCTION()
	virtual void UpdateComponents() override;
	/* IAeonixSubsystemInterface END */
	
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;
	
	virtual bool IsTickable() const override;
	virtual bool IsTickableInEditor() const override;
	virtual bool IsTickableWhenPaused() const override;


private:
	UPROPERTY()
	TArray<const AAeonixBoundingVolume*> RegisteredVolumes{};

	UPROPERTY()
	TArray<UAeonixNavigationComponent*> RegisteredNavComponents{};

	UPROPERTY()
	TMap<UAeonixNavigationComponent*, const AAeonixBoundingVolume*> AgentToVolumeMap;
};