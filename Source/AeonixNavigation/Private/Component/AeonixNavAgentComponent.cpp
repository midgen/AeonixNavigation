
#include <AeonixNavigation/Public/Component/AeonixNavAgentComponent.h>

#include <AeonixNavigation/Public/Subsystem/AeonixSubsystem.h>
#include <AeonixNavigation/Public/Task/AeonixFindPathTask.h>
#include <AeonixNavigation/Public/Data/AeonixLink.h>
#include <AeonixNavigation/Public/Pathfinding/AeonixNavigationPath.h>
#include <AeonixNavigation/Public/Pathfinding/AeonixPathFinder.h>
#include <AeonixNavigation/Public/Actor/AeonixBoundingVolume.h>
#include <AeonixNavigation/Public/Util/AeonixMediator.h>
#include <AeonixNavigation/Public/AeonixNavigation.h>

#include <Runtime/Engine/Classes/GameFramework/Actor.h>
#include <Runtime/Engine/Public/DrawDebugHelpers.h>

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
		UE_LOG(AeonixNavigation, Error, TEXT("No AeonixSubsystem with a valid AeonixInterface found"));
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
		UE_LOG(AeonixNavigation, Error, TEXT("No AeonixSubsystem with a valid AeonixInterface found"));
	}
	else
	{
		AeonixSubsystem->UnRegisterNavComponent(this);
	}

	Super::EndPlay(EndPlayReason);
}

// bool UAeonixNavAgentComponent::FindPathAsync(const FVector& StartPosition, const FVector& TargetPosition, FThreadSafeBool& CompleteFlag, FAeonixNavPathSharedPtr* OutNavPath)
// {
// 	// UE_LOG(AeonixNavigation, Log, TEXT("Finding path from %s and %s"), *StartPosition.ToString(), *TargetPosition.ToString());
// 	// AeonixLink StartNavLink;
// 	// AeonixLink TargetNavLink;
// 	// if (HasNavData())
// 	// {
// 	// 	// Get the nav link from our volume
// 	// 	if (!AeonixMediator::GetLinkFromPosition(StartPosition, *CurrentNavVolume, StartNavLink))
// 	// 	{
// 	// 		UE_LOG(AeonixNavigation, Error, TEXT("Path finder failed to find start nav link. Is your pawn blocking the channel you've selected to generate the nav data with?"));
// 	// 		return false;
// 	// 	}
// 	//
// 	// 	if (!AeonixMediator::GetLinkFromPosition(TargetPosition, *CurrentNavVolume, TargetNavLink))
// 	// 	{
// 	// 		UE_LOG(AeonixNavigation, Error, TEXT("Path finder failed to find target nav link"));
// 	// 		return false;
// 	// 	}
// 	//
// 	// 	(new FAutoDeleteAsyncTask<FAeonixFindPathTask>(CurrentNavVolume->GetNavData(), PathfinderSettings, GetWorld(), StartNavLink, TargetNavLink, StartPosition, TargetPosition, OutNavPath, CompleteFlag))->StartBackgroundTask();
// 	//
// 	// 	return true;
// 	// }
// 	// else
// 	// {
// 	// 	UE_LOG(AeonixNavigation, Error, TEXT("Pawn is not inside an Aeonix volume, or nav data has not been generated"));
// 	// }
// 	return false;
// }

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