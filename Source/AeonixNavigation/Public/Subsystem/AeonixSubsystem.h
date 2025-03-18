#pragma once

#include <AeonixNavigation/Public/Interface/AeonixSubsystemInterface.h>

#include <Subsystems/WorldSubsystem.h>

#include "AeonixSubsystem.generated.h"

class AAeonixBoundingVolume;
class UAeonixNavigationComponent;

UCLASS()
class AEONIXNAVIGATION_API UAeonixSubsystem : public UTickableWorldSubsystem, public IAeonixSubsystemInterface
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

private:
	UPROPERTY()
	TArray<const AAeonixBoundingVolume*> RegisteredVolumes{};

	UPROPERTY()
	TArray<UAeonixNavigationComponent*> RegisteredNavComponents{};
};
