
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

UAeonixNavAgentComponent::~UAeonixNavAgentComponent()
{
	// if (!AeonixSubsystem.GetInterface())
	// {
	// 	UE_LOG(AeonixNavigation, Error, TEXT("No AeonixSubsystem with a valid AeonixInterface found"));
	// }
	// else
	// {
	// 	AeonixSubsystem->UnRegisterNavComponent(this);
	// }
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

// bool UAeonixNavAgentComponent::HasNavData() const
// {
// 	return CurrentNavVolume != nullptr;
// }

// AeonixLink UAeonixNavAgentComponent::GetNavPosition() const
// {
// 	AeonixLink NavLink;
// 	if (HasNavData())
// 	{
// 		AeonixMediator::GetLinkFromPosition(GetOwner()->GetActorLocation(), *CurrentNavVolume, NavLink);
//
// 		if (bDebugPrintCurrentPosition)
// 		{
// 			const AeonixNode& CurrentNode = CurrentNavVolume->GetNavData().OctreeData.GetNode(NavLink);
// 			FVector CurrentNodePosition;
//
// 			bool bIsValid = CurrentNavVolume->GetNavData().GetLinkPosition(NavLink, CurrentNodePosition);
//
// 			DrawDebugLine(GetWorld(), GetAgentPosition(), CurrentNodePosition, bIsValid ? FColor::Green : FColor::Red, false, -1.f, 0, 10.f);
// 			DrawDebugString(GetWorld(), GetAgentPosition() + FVector(0.f, 0.f, -50.f), NavLink.ToString(), NULL, FColor::Yellow, 0.01f);
// 		}
// 	}
// 	return NavLink;
// }

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

// bool UAeonixNavAgentComponent::FindPathImmediate(const FVector& StartPosition, const FVector& TargetPosition, FAeonixNavPathSharedPtr* NavPath)
// {
// 	UE_LOG(AeonixNavigation, Log, TEXT("Finding path immediate from %s and %s"), *StartPosition.ToString(), *TargetPosition.ToString());
//
// 	return AeonixSubsystem->FindPathImmediateAgent(this, TargetPosition, CurrentPath);
// 	
	// AeonixLink StartNavLink;
	// AeonixLink TargetNavLink;
	// if (HasNavData())
	// {
	// 	// Get the nav link from our volume
	// 	if (!AeonixMediator::GetLinkFromPosition(StartPosition, *CurrentNavVolume, StartNavLink))
	// 	{
	// 		UE_LOG(AeonixNavigation, Error, TEXT("Path finder failed to find start nav link"));
	// 		return false;
	// 	}
	//
	// 	if (!AeonixMediator::GetLinkFromPosition(TargetPosition, *CurrentNavVolume, TargetNavLink))
	// 	{
	// 		UE_LOG(AeonixNavigation, Error, TEXT("Path finder failed to find target nav link"));
	// 		return false;
	// 	}
	//
	// 	if (!NavPath || !NavPath->IsValid())
	// 	{
	// 		UE_LOG(AeonixNavigation, Error, TEXT("Nav path data invalid"));
	// 		return false;
	// 	}
	//
	// 	FAeonixNavigationPath* Path = NavPath->Get();
	//
	// 	Path->ResetForRepath();
	//
	// 	AeonixPathFinder pathFinder(GetWorld(), CurrentNavVolume->GetNavData(), PathfinderSettings);
	//
	// 	int32 Result = pathFinder.FindPath(StartNavLink, TargetNavLink, StartPosition, TargetPosition, NavPath);
	//
	// 	Path->SetIsReady(true);
	//
	// 	return true;
	// }
	// else
	// {
	// 	UE_LOG(AeonixNavigation, Error, TEXT("Pawn is not inside an Aeonix volume, or nav data has not been generated"));
	// }
	//
	// return false;
//}

// void UAeonixNavAgentComponent::FindPathImmediate(const FVector& StartPosition, const FVector& TargetPosition, TArray<FVector>& OutPathPoints)
// {
// 	AeonixSubsystem->FindPathImmediatePosition(StartPosition, TargetPosition, CurrentPath);
// 	
// 	OutPathPoints.Empty();
//
// 	for (const FAeonixPathPoint& PathPoint : CurrentPath.GetPathPoints())
// 	{
// 		OutPathPoints.Emplace(PathPoint.Position);
// 	}
// }

// void UAeonixNavAgentComponent::SetCurrentNavVolume(const AAeonixBoundingVolume* Volume)
// {
// 	CurrentNavVolume = Volume;
//
// 	if (CurrentNavVolume && CurrentNavVolume->bIsReadyForNavigation)
// 	{
// 		FVector Location = GetAgentPosition();
// 		if (bDebugPrintMortonCodes)
// 		{
// 			DebugLocalPosition(Location);
// 		}
// 	}
// }

// void UAeonixNavAgentComponent::DebugLocalPosition(FVector& aPosition)
// {
// 	if (HasNavData())
// 	{
// 		for (int i = 0; i < CurrentNavVolume->GetNavData().OctreeData.GetNumLayers() - 1; i++)
// 		{
// 			FIntVector Pos;
// 			AeonixMediator::GetVolumeXYZ(GetAgentPosition(), *CurrentNavVolume, i, Pos);
// 			uint_fast64_t Code = morton3D_64_encode(Pos.X, Pos.Y, Pos.Z);
// 			FString CodeString = FString::FromInt(Code);
// 			DrawDebugString(GetWorld(), GetAgentPosition() + FVector(0.f, 0.f, i * 50.0f), Pos.ToString() + " - " + CodeString, NULL, FColor::White, 0.01f);
// 		}
// 	}
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
	else // For debug shenangans, we might not be calling this on a controller component
	{
		Result = GetOwner()->GetActorLocation();
	}

	return Result;
}