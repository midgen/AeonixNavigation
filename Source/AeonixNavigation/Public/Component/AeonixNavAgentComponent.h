#pragma once

#include <AeonixNavigation/Public/Pathfinding/AeonixPathFinder.h>
#include <AeonixNavigation/Public/Pathfinding/AeonixNavigationPath.h>
#include <AeonixNavigation/Public/Interface/AeonixSubsystemInterface.h>

#include <Runtime/Engine/Classes/Components/ActorComponent.h>

#include "AeonixNavAgentComponent.generated.h"

class AAeonixBoundingVolume;
struct AeonixLink;

/**
 *  Component to provide Aeonix Navigation capabilities to an Agent.
 *  Typically placed on your AI Controller, but can also be on any actor (see debug actor)
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class AEONIXNAVIGATION_API UAeonixNavAgentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAeonixNavAgentComponent(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aeonix")
	FAeonixPathFinderSettings PathfinderSettings;
	
	FAeonixNavigationPath& GetPath() { return CurrentPath; }
	const FAeonixNavigationPath& GetPath() const  { return CurrentPath; }
	FVector GetAgentPosition() const;

	// Need to make a sane multithreading implementation, this is very crude and will have edge case crashes at present.
	// Thread safe counter to track async path requests

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	UPROPERTY()
	TScriptInterface<IAeonixSubsystemInterface> AeonixSubsystem;
	
	FAeonixNavigationPath CurrentPath{};
};
