
#include <AeonixNavigation/Public/Component/AeonixNavigationComponent.h>

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

UAeonixNavigationComponent::UAeonixNavigationComponent(const FObjectInitializer& ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
	CurrentPath = MakeShareable<FAeonixNavigationPath>(new FAeonixNavigationPath());
}

void UAeonixNavigationComponent::BeginPlay()
{
	Super::BeginPlay();

	AeonixSubsystem = GEngine->GetEngineSubsystem<UAeonixSubsystem>();
	if (!AeonixSubsystem.GetInterface())
	{
		UE_LOG(AeonixNavigation, Error, TEXT("No AeonixSubsystem with a valid AeonixInterface found"));
	}
	else
	{
		AeonixSubsystem->RegisterNavComponent(this);
		CurrentNavVolume = AeonixSubsystem->GetVolumeForPosition(GetPawnPosition());
	}
}

void UAeonixNavigationComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
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

bool UAeonixNavigationComponent::HasNavData() const
{
	return CurrentNavVolume != nullptr;
}

AeonixLink UAeonixNavigationComponent::GetNavPosition() const
{
	AeonixLink NavLink;
	if (HasNavData())
	{
		AeonixMediator::GetLinkFromPosition(GetOwner()->GetActorLocation(), *CurrentNavVolume, NavLink);

		if (bDebugPrintCurrentPosition)
		{
			const AeonixNode& CurrentNode = CurrentNavVolume->GetNavData().OctreeData.GetNode(NavLink);
			FVector CurrentNodePosition;

			bool bIsValid = CurrentNavVolume->GetNavData().GetLinkPosition(NavLink, CurrentNodePosition);

			DrawDebugLine(GetWorld(), GetPawnPosition(), CurrentNodePosition, bIsValid ? FColor::Green : FColor::Red, false, -1.f, 0, 10.f);
			DrawDebugString(GetWorld(), GetPawnPosition() + FVector(0.f, 0.f, -50.f), NavLink.ToString(), NULL, FColor::Yellow, 0.01f);
		}
	}
	return NavLink;
}

bool UAeonixNavigationComponent::FindPathAsync(const FVector& StartPosition, const FVector& TargetPosition, FThreadSafeBool& CompleteFlag, FAeonixNavPathSharedPtr* OutNavPath)
{
	UE_LOG(AeonixNavigation, Log, TEXT("Finding path from %s and %s"), *StartPosition.ToString(), *TargetPosition.ToString());
	AeonixLink StartNavLink;
	AeonixLink TargetNavLink;
	if (HasNavData())
	{
		// Get the nav link from our volume
		if (!AeonixMediator::GetLinkFromPosition(StartPosition, *CurrentNavVolume, StartNavLink))
		{
			UE_LOG(AeonixNavigation, Error, TEXT("Path finder failed to find start nav link. Is your pawn blocking the channel you've selected to generate the nav data with?"));
			return false;
		}

		if (!AeonixMediator::GetLinkFromPosition(TargetPosition, *CurrentNavVolume, TargetNavLink))
		{
			UE_LOG(AeonixNavigation, Error, TEXT("Path finder failed to find target nav link"));
			return false;
		}

		FAeonixPathFinderSettings Settings;
		Settings.bUseUnitCost = bUseUnitCost;
		Settings.UnitCost = UnitCost;
		Settings.EstimateWeight = EstimateWeight;
		Settings.NodeSizeCompensation = NodeSizeCompensation;
		Settings.PathCostType = PathCostType;
		Settings.SmoothingIterations = SmoothingIterations;

		(new FAutoDeleteAsyncTask<FAeonixFindPathTask>(CurrentNavVolume->GetNavData(), Settings, GetWorld(), StartNavLink, TargetNavLink, StartPosition, TargetPosition, OutNavPath, CompleteFlag))->StartBackgroundTask();

		return true;
	}
	else
	{
		UE_LOG(AeonixNavigation, Error, TEXT("Pawn is not inside an Aeonix volume, or nav data has not been generated"));
	}
	return false;
}

bool UAeonixNavigationComponent::FindPathImmediate(const FVector& StartPosition, const FVector& TargetPosition, FAeonixNavPathSharedPtr* NavPath)
{
	UE_LOG(AeonixNavigation, Log, TEXT("Finding path immediate from %s and %s"), *StartPosition.ToString(), *TargetPosition.ToString());

	AeonixLink StartNavLink;
	AeonixLink TargetNavLink;
	if (HasNavData())
	{
		// Get the nav link from our volume
		if (!AeonixMediator::GetLinkFromPosition(StartPosition, *CurrentNavVolume, StartNavLink))
		{
			UE_LOG(AeonixNavigation, Error, TEXT("Path finder failed to find start nav link"));
			return false;
		}

		if (!AeonixMediator::GetLinkFromPosition(TargetPosition, *CurrentNavVolume, TargetNavLink))
		{
			UE_LOG(AeonixNavigation, Error, TEXT("Path finder failed to find target nav link"));
			return false;
		}

		if (!NavPath || !NavPath->IsValid())
		{
			UE_LOG(AeonixNavigation, Error, TEXT("Nav path data invalid"));
			return false;
		}

		FAeonixNavigationPath* Path = NavPath->Get();

		Path->ResetForRepath();

		FAeonixPathFinderSettings Settings;
		Settings.bUseUnitCost = bUseUnitCost;
		Settings.UnitCost = UnitCost;
		Settings.EstimateWeight = EstimateWeight;
		Settings.NodeSizeCompensation = NodeSizeCompensation;
		Settings.PathCostType = PathCostType;
		Settings.SmoothingIterations = SmoothingIterations;

		AeonixPathFinder pathFinder(GetWorld(), CurrentNavVolume->GetNavData(), Settings);

		int32 Result = pathFinder.FindPath(StartNavLink, TargetNavLink, StartPosition, TargetPosition, NavPath);

		Path->SetIsReady(true);

		return true;
	}
	else
	{
		UE_LOG(AeonixNavigation, Error, TEXT("Pawn is not inside an Aeonix volume, or nav data has not been generated"));
	}

	return false;
}

void UAeonixNavigationComponent::FindPathImmediate(const FVector& StartPosition, const FVector& TargetPosition, TArray<FVector>& OutPathPoints)
{
	FindPathImmediate(StartPosition, TargetPosition, &CurrentPath);

	OutPathPoints.Empty();

	for (const FAeonixPathPoint& PathPoint : CurrentPath->GetPathPoints())
	{
		OutPathPoints.Emplace(PathPoint.Position);
	}
}

void UAeonixNavigationComponent::SetCurrentNavVolume(const AAeonixBoundingVolume* Volume)
{
	CurrentNavVolume = Volume;

	if (CurrentNavVolume && CurrentNavVolume->bIsReadyForNavigation)
	{
		FVector Location = GetPawnPosition();
		if (bDebugPrintMortonCodes)
		{
			DebugLocalPosition(Location);
		}
	}
}

void UAeonixNavigationComponent::DebugLocalPosition(FVector& aPosition)
{
	if (HasNavData())
	{
		for (int i = 0; i < CurrentNavVolume->GetNavData().OctreeData.GetNumLayers() - 1; i++)
		{
			FIntVector Pos;
			AeonixMediator::GetVolumeXYZ(GetPawnPosition(), *CurrentNavVolume, i, Pos);
			uint_fast64_t Code = morton3D_64_encode(Pos.X, Pos.Y, Pos.Z);
			FString CodeString = FString::FromInt(Code);
			DrawDebugString(GetWorld(), GetPawnPosition() + FVector(0.f, 0.f, i * 50.0f), Pos.ToString() + " - " + CodeString, NULL, FColor::White, 0.01f);
		}
	}
}

FVector UAeonixNavigationComponent::GetPawnPosition() const
{
	FVector Result;

	AController* Controller = Cast<AController>(GetOwner());

	if (Controller)
	{
		if (APawn* Pawn = Controller->GetPawn())
			Result = Pawn->GetActorLocation();
	}

	return Result;
}