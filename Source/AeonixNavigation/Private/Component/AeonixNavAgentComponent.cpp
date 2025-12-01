
#include "Component/AeonixNavAgentComponent.h"

#include "Subsystem/AeonixSubsystem.h"
#include "AeonixNavigation.h"

#include "GameFramework/Actor.h"
#include "DrawDebugHelpers.h"

UAeonixNavAgentComponent::UAeonixNavAgentComponent(const FObjectInitializer& ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UAeonixNavAgentComponent::BeginPlay()
{
	Super::BeginPlay();

	AeonixSubsystem = GetWorld()->GetSubsystem<UAeonixSubsystem>();
	if (!AeonixSubsystem.GetInterface())
	{
		UE_LOG(LogAeonixNavigation, Error, TEXT("No AeonixSubsystem with a valid AeonixInterface found"));
	}
	else
	{
		AeonixSubsystem->RegisterNavComponent(this);
	}
}

void UAeonixNavAgentComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (!AeonixSubsystem.GetInterface())
	{
		UE_LOG(LogAeonixNavigation, Error, TEXT("No AeonixSubsystem with a valid AeonixInterface found"));
	}
	else
	{
		AeonixSubsystem->UnRegisterNavComponent(this);
	}

	Super::EndPlay(EndPlayReason);
}

FVector UAeonixNavAgentComponent::GetAgentPosition() const
{
	FVector Result;

	AController* Controller = Cast<AController>(GetOwner());

	if (Controller)
	{
		if (APawn* Pawn = Controller->GetPawn())
			Result = Pawn->GetActorLocation();
	}
	else // Maybe this is just on a debug actor, rather than an AI controller
	{
		Result = GetOwner()->GetActorLocation();
	}

	return Result;
}

FVector UAeonixNavAgentComponent::GetPathfindingStartPosition() const
{
	return GetAgentPosition() + StartPointOffset;
}

FVector UAeonixNavAgentComponent::GetPathfindingEndPosition(const FVector& TargetLocation) const
{
	return TargetLocation + EndPointOffset;
}

void UAeonixNavAgentComponent::RegisterPathForDebugRendering()
{
	if (bEnablePathDebugRendering && CurrentPath.GetPathPoints().Num() > 0)
	{
		UE_LOG(LogAeonixNavigation, Log, TEXT("NavAgent: Registering path with %d points for debug rendering"), CurrentPath.GetPathPoints().Num());
		CurrentPath.DebugDrawLite(GetWorld(), FColor::Green, 10.0f);
	}
}