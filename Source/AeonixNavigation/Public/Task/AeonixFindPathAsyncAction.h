#pragma once

#include "Kismet/BlueprintAsyncActionBase.h"
#include "Data/AeonixTypes.h"
#include "Pathfinding/AeonixNavigationPath.h"
#include "AeonixFindPathAsyncAction.generated.h"

class UAeonixNavAgentComponent;

// Output pin delegates
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAeonixPathFound, const TArray<FAeonixPathPoint>&, PathPoints);
DECLARE_DYNAMIC_MULTICAST_DELEGATE(FOnAeonixPathFailed);

/**
 * Latent Blueprint node for async pathfinding.
 * Finds a path asynchronously and outputs the path points on completion.
 */
UCLASS()
class AEONIXNAVIGATION_API UAeonixFindPathAsyncAction : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	/** Called when pathfinding completes successfully */
	UPROPERTY(BlueprintAssignable)
	FOnAeonixPathFound OnSuccess;

	/** Called when pathfinding fails */
	UPROPERTY(BlueprintAssignable)
	FOnAeonixPathFailed OnFailed;

	/**
	 * Find a path asynchronously from the agent's current location to the target.
	 * @param WorldContextObject World context
	 * @param NavAgentComponent The navigation agent component
	 * @param TargetLocation The destination location
	 * @return The async action
	 */
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true", WorldContext = "WorldContextObject", DisplayName = "Aeonix Find Path Async"), Category = "Aeonix|Pathfinding")
	static UAeonixFindPathAsyncAction* FindPathAsync(
		UObject* WorldContextObject,
		UAeonixNavAgentComponent* NavAgentComponent,
		FVector TargetLocation);

	// UBlueprintAsyncActionBase interface
	virtual void Activate() override;

private:
	UFUNCTION()
	void OnPathFindComplete(EAeonixPathFindStatus Status);

	// Stored parameters
	TWeakObjectPtr<UAeonixNavAgentComponent> NavAgent;
	FVector Target;
	TWeakObjectPtr<UWorld> WorldPtr;

	// Path storage
	FAeonixNavigationPath ResultPath;
};
