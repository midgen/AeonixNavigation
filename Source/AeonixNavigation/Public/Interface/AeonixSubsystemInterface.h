#pragma once

#include <UObject/Interface.h>

#include "AeonixSubsystemInterface.generated.h"

class AAeonixBoundingVolume;
class UAeonixNavigationComponent;

UINTERFACE(MinimalAPI)
class UAeonixSubsystemInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 *  Interface for interacting with the main navigation subsystem
 */
class AEONIXNAVIGATION_API IAeonixSubsystemInterface
{
	GENERATED_BODY()

public:
	UFUNCTION()
	virtual void RegisterVolume(const AAeonixBoundingVolume* Volume) = 0;
	UFUNCTION()
	virtual void UnRegisterVolume(const AAeonixBoundingVolume* Volume) = 0;
	UFUNCTION()
	virtual void RegisterNavComponent(UAeonixNavigationComponent* NavComponent) = 0;
	UFUNCTION()
	virtual void UnRegisterNavComponent(UAeonixNavigationComponent* NavComponent) = 0;
	UFUNCTION()
	virtual const AAeonixBoundingVolume* GetVolumeForPosition(const FVector& Position) = 0;
	
};
