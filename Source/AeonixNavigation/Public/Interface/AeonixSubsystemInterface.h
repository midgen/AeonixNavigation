#pragma once

#include <AeonixNavigation/Public/Data/AeonixTypes.h>

#include <UObject/Interface.h>

#include "AeonixSubsystemInterface.generated.h"

class AAeonixDynamicSubregion;
class AAeonixBoundingVolume;
class UAeonixNavAgentComponent;
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
	virtual void RegisterVolume(AAeonixBoundingVolume* Volume) = 0;
	UFUNCTION()
	virtual void UnRegisterVolume(AAeonixBoundingVolume* Volume) = 0;
	UFUNCTION()
	virtual void RegisterDynamicSubregion(AAeonixDynamicSubregion* DynamicSubregion) = 0;
	UFUNCTION()
	virtual void UnRegisterDynamicSubregion(AAeonixDynamicSubregion* DynamicSubregion) = 0;
	UFUNCTION()
	virtual void RegisterNavComponent(UAeonixNavAgentComponent* NavComponent, bool bCreateMassEntity) = 0;
	UFUNCTION()
	virtual void UnRegisterNavComponent(UAeonixNavAgentComponent* NavComponent, bool bDestroyMassEntity) = 0;
	UFUNCTION()
	virtual const AAeonixBoundingVolume* GetVolumeForPosition(const FVector& Position) = 0;

	UFUNCTION()
	virtual const AAeonixBoundingVolume* GetVolumeForAgent(const UAeonixNavAgentComponent* NavigationComponent) = 0;

	UFUNCTION(BlueprintCallable, Category="Aeonix")
	virtual FAeonixPathFindRequestCompleteDelegate& FindPathAsyncAgent(UAeonixNavAgentComponent* NavAgentComponent, const FVector& End, FAeonixNavigationPath& OutPath) = 0;

	UFUNCTION(BlueprintCallable, Category="Aeonix")
	virtual bool FindPathImmediateAgent(UAeonixNavAgentComponent* NavigationComponent, const FVector& End, FAeonixNavigationPath& OutPath) = 0;

	UFUNCTION()
	virtual void UpdateComponents() = 0;
	
};
