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
	void RegisterVolume(const AAeonixBoundingVolume* Volume) override;
	UFUNCTION()
	void UnRegisterVolume(const AAeonixBoundingVolume* Volume) override;
	UFUNCTION()
	void RegisterNavComponent(UAeonixNavigationComponent* NavComponent) override;
	UFUNCTION()
	void UnRegisterNavComponent(UAeonixNavigationComponent* NavComponent) override;
	UFUNCTION()
	const AAeonixBoundingVolume* GetVolumeForPosition(const FVector& Position) override;
	/* IAeonixSubsystemInterface END */
	
	virtual void Tick(float DeltaTime) override;
	TStatId GetStatId() const override;
	
	bool IsTickable() const override;
	bool IsTickableInEditor() const override;
	bool IsTickableWhenPaused() const override;

private:
	UPROPERTY()
	TArray<const AAeonixBoundingVolume*> RegisteredVolumes{};

	UPROPERTY()
	TArray<UAeonixNavigationComponent*> RegisteredNavComponents{};
};
