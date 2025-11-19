#pragma once

#include "Data/AeonixTypes.h"

#include "UObject/Interface.h"

#include "AeonixSubsystemInterface.generated.h"

class AAeonixModifierVolume;
class AAeonixBoundingVolume;
class UAeonixNavAgentComponent;
class UAeonixDynamicObstacleComponent;
struct FAeonixNavigationPath;

/** Delegate broadcast when navigation regeneration completes (full or dynamic subregions) */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnNavigationRegenCompleted, AAeonixBoundingVolume*);

/** Delegate broadcast when registration changes (volumes, modifiers, or obstacles added/removed) */
DECLARE_MULTICAST_DELEGATE(FOnRegistrationChanged);

UENUM()
enum class EAeonixMassEntityFlag : uint8
{
	Enabled,
	Disabled
};

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
	virtual void RegisterVolume(AAeonixBoundingVolume* Volume, EAeonixMassEntityFlag bCreateMassEntity) = 0;
	UFUNCTION()
	virtual void UnRegisterVolume(AAeonixBoundingVolume* Volume, EAeonixMassEntityFlag bDestroyMassEntity) = 0;
	UFUNCTION()
	virtual void RegisterModifierVolume(AAeonixModifierVolume* ModifierVolume) = 0;
	UFUNCTION()
	virtual void UnRegisterModifierVolume(AAeonixModifierVolume* ModifierVolume) = 0;
	UFUNCTION()
	virtual void RegisterNavComponent(UAeonixNavAgentComponent* NavComponent, EAeonixMassEntityFlag bCreateMassEntity) = 0;
	UFUNCTION()
	virtual void UnRegisterNavComponent(UAeonixNavAgentComponent* NavComponent, EAeonixMassEntityFlag bDestroyMassEntity) = 0;
	UFUNCTION()
	virtual void RegisterDynamicObstacle(UAeonixDynamicObstacleComponent* ObstacleComponent) = 0;
	UFUNCTION()
	virtual void UnRegisterDynamicObstacle(UAeonixDynamicObstacleComponent* ObstacleComponent) = 0;
	UFUNCTION()
	virtual const AAeonixBoundingVolume* GetVolumeForPosition(const FVector& Position) = 0;

	UFUNCTION()
	virtual const AAeonixBoundingVolume* GetVolumeForAgent(const UAeonixNavAgentComponent* NavigationComponent) = 0;
	UFUNCTION()
	virtual AAeonixBoundingVolume* GetMutableVolumeForAgent(const UAeonixNavAgentComponent* NavigationComponent) = 0;

	UFUNCTION(BlueprintCallable, Category="Aeonix")
	virtual FAeonixPathFindRequestCompleteDelegate& FindPathAsyncAgent(UAeonixNavAgentComponent* NavAgentComponent, const FVector& End, FAeonixNavigationPath& OutPath) = 0;

	UFUNCTION(BlueprintCallable, Category="Aeonix")
	virtual bool FindPathImmediateAgent(UAeonixNavAgentComponent* NavigationComponent, const FVector& End, FAeonixNavigationPath& OutPath) = 0;

	UFUNCTION()
	virtual void UpdateComponents() = 0;

	/** Get delegate for navigation regeneration completion notifications */
	virtual FOnNavigationRegenCompleted& GetOnNavigationRegenCompleted() = 0;

	/** Get delegate for registration change notifications */
	virtual FOnRegistrationChanged& GetOnRegistrationChanged() = 0;

	/** Request debug path update for a specific nav agent component */
	virtual void RequestDebugPathUpdate(UAeonixNavAgentComponent* NavComponent) = 0;

};
