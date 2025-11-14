#pragma once

#include "Pathfinding/AeonixPathFinder.h"
#include "Pathfinding/AeonixNavigationPath.h"
#include "Interface/AeonixSubsystemInterface.h"

#include "Components/ActorComponent.h"

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

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aeonix")
	FVector StartPointOffset = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aeonix")
	FVector EndPointOffset = FVector::ZeroVector;

	FAeonixNavigationPath& GetPath() { return CurrentPath; }
	const FAeonixNavigationPath& GetPath() const  { return CurrentPath; }
	FVector GetAgentPosition() const;
	FVector GetPathfindingStartPosition() const;
	FVector GetPathfindingEndPosition(const FVector& TargetLocation) const;

	// Debug rendering for the current path
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Debug")
	bool bEnablePathDebugRendering = false;

	UFUNCTION(BlueprintCallable, Category = "Debug")
	void RegisterPathForDebugRendering();

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
