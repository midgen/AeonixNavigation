#include "Task/AeonixFindPathAsyncAction.h"
#include "Component/AeonixNavAgentComponent.h"
#include "Subsystem/AeonixSubsystem.h"
#include "Engine/World.h"

UAeonixFindPathAsyncAction* UAeonixFindPathAsyncAction::FindPathAsync(
	UObject* WorldContextObject,
	UAeonixNavAgentComponent* NavAgentComponent,
	FVector TargetLocation)
{
	UAeonixFindPathAsyncAction* Action = NewObject<UAeonixFindPathAsyncAction>();
	Action->NavAgent = NavAgentComponent;
	Action->Target = TargetLocation;

	if (WorldContextObject)
	{
		Action->WorldPtr = WorldContextObject->GetWorld();
	}

	Action->RegisterWithGameInstance(WorldContextObject);
	return Action;
}

void UAeonixFindPathAsyncAction::Activate()
{
	if (!NavAgent.IsValid())
	{
		OnFailed.Broadcast();
		return;
	}

	if (!WorldPtr.IsValid())
	{
		OnFailed.Broadcast();
		return;
	}

	// Get the Aeonix subsystem
	UAeonixSubsystem* Subsystem = WorldPtr->GetSubsystem<UAeonixSubsystem>();
	if (!Subsystem)
	{
		OnFailed.Broadcast();
		return;
	}

	// Request async pathfinding
	FAeonixPathFindRequestCompleteDelegate& Delegate =
		Subsystem->FindPathAsyncAgent(NavAgent.Get(), Target, ResultPath);
	Delegate.BindDynamic(this, &UAeonixFindPathAsyncAction::OnPathFindComplete);
}

void UAeonixFindPathAsyncAction::OnPathFindComplete(EAeonixPathFindStatus Status)
{
	if (Status == EAeonixPathFindStatus::Complete)
	{
		OnSuccess.Broadcast(ResultPath.GetPathPoints());
	}
	else
	{
		OnFailed.Broadcast();
	}
}
