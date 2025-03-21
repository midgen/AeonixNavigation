#pragma once

#include <UObject/Interface.h>

#include "AeonixSubsystemInterface.generated.h"

class AAeonixBoundingVolume;
class UAeonixNavigationComponent;
struct FAeonixNavigationPath;

UINTERFACE(MinimalAPI, NotBlueprintable)
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

	UFUNCTION()
	virtual const AAeonixBoundingVolume* GetVolumeForAgent(const UAeonixNavigationComponent* NavigationComponent) = 0;

	// UFUNCTION(BlueprintCallable, Category="Aeonix")
	// virtual bool FindPathImmediatePosition(const FVector& Start, const FVector& End, FAeonixNavigationPath& OutPath) = 0;

	UFUNCTION(BlueprintCallable, Category="Aeonix")
	virtual bool FindPathImmediateAgent(UAeonixNavigationComponent* NavigationComponent, const FVector& End, FAeonixNavigationPath& OutPath) = 0;

	UFUNCTION()
	virtual void UpdateComponents() = 0;
	
};
