#include "Component/AeonixAutopathComponent.h"
#include "Subsystem/AeonixAutopathSubsystem.h"
#include "Component/AeonixNavAgentComponent.h"
#include "AeonixNavigation.h"

#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"

UAeonixAutopathComponent::UAeonixAutopathComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Component does not tick - subsystem handles all autopath ticking
	PrimaryComponentTick.bCanEverTick = false;
}

void UAeonixAutopathComponent::OnRegister()
{
	Super::OnRegister();

	// Cache nav agent component
	CacheNavAgentComponent();

	// Register with subsystem (works in editor and at runtime)
	RegisterWithSubsystem();
}

void UAeonixAutopathComponent::OnUnregister()
{
	// Unregister from subsystem
	UnregisterFromSubsystem();

	Super::OnUnregister();
}

void UAeonixAutopathComponent::BeginPlay()
{
	Super::BeginPlay();

	// Reset state from editor world - PIE creates new world with new actor instances
	bRegisteredWithSubsystem = false;
	CurrentPath.ResetForRepath();

	// Cache nav agent component
	CacheNavAgentComponent();

	RegisterWithSubsystem();
}

void UAeonixAutopathComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Unregister from subsystem
	UnregisterFromSubsystem();

	Super::EndPlay(EndPlayReason);
}

void UAeonixAutopathComponent::RegisterWithSubsystem()
{
	if (bRegisteredWithSubsystem)
	{
		return; // Already registered
	}

	// Get reference to the Autopath subsystem
	if (UWorld* World = GetWorld())
	{
		UAeonixAutopathSubsystem* Subsystem = World->GetSubsystem<UAeonixAutopathSubsystem>();
		if (!Subsystem)
		{
			UE_LOG(LogAeonixNavigation, Verbose, TEXT("AutopathComponent %s: No AeonixAutopathSubsystem found"), *GetName());
			return;
		}

		// Register with the subsystem
		Subsystem->RegisterAutopathSource(this);
		bRegisteredWithSubsystem = true;

		UE_LOG(LogAeonixNavigation, Verbose, TEXT("AutopathComponent %s: Registered with subsystem"), *GetName());
	}
}

void UAeonixAutopathComponent::UnregisterFromSubsystem()
{
	if (!bRegisteredWithSubsystem)
	{
		return; // Not registered
	}

	// Unregister from subsystem
	UWorld* World = GetWorld();
	if (!World)
	{
		bRegisteredWithSubsystem = false;
		return;
	}

	UAeonixAutopathSubsystem* Subsystem = World->GetSubsystem<UAeonixAutopathSubsystem>();
	if (!Subsystem)
	{
		bRegisteredWithSubsystem = false;
		return;
	}

	Subsystem->UnregisterAutopathSource(this);
	bRegisteredWithSubsystem = false;

	UE_LOG(LogAeonixNavigation, Verbose, TEXT("AutopathComponent %s: Unregistered from subsystem"), *GetName());
}

void UAeonixAutopathComponent::CacheNavAgentComponent()
{
	if (AActor* Owner = GetOwner())
	{
		// First try finding on the actor directly
		CachedNavAgentComponent = Owner->FindComponentByClass<UAeonixNavAgentComponent>();

		// If not found and actor is a pawn, try the controller
		if (!CachedNavAgentComponent.IsValid())
		{
			if (APawn* Pawn = Cast<APawn>(Owner))
			{
				if (AController* Controller = Pawn->GetController())
				{
					CachedNavAgentComponent = Controller->FindComponentByClass<UAeonixNavAgentComponent>();
				}
			}
		}

		if (!CachedNavAgentComponent.IsValid())
		{
			UE_LOG(LogAeonixNavigation, Warning, TEXT("AutopathComponent %s: No UAeonixNavAgentComponent found on owner or controller - pathfinding may fail"), *GetName());
		}
	}
}

TArray<FVector> UAeonixAutopathComponent::GetPathPoints() const
{
	TArray<FVector> Result;
	const TArray<FAeonixPathPoint>& PathPoints = CurrentPath.GetPathPoints();
	Result.Reserve(PathPoints.Num());

	for (const FAeonixPathPoint& Point : PathPoints)
	{
		Result.Add(Point.Position);
	}

	return Result;
}

FVector UAeonixAutopathComponent::GetNextPathPoint() const
{
	const TArray<FAeonixPathPoint>& PathPoints = CurrentPath.GetPathPoints();

	if (PathPoints.Num() == 0)
	{
		return FVector::ZeroVector;
	}

	// Return second point if exists (first is current position)
	if (PathPoints.Num() > 1)
	{
		return PathPoints[1].Position;
	}

	// If only one point, return it
	return PathPoints[0].Position;
}

bool UAeonixAutopathComponent::HasValidPath() const
{
	return CurrentPath.IsReady() && CurrentPath.IsValid() && CurrentPath.GetNumPoints() > 0;
}

int32 UAeonixAutopathComponent::GetNumPathPoints() const
{
	return CurrentPath.GetNumPoints();
}

bool UAeonixAutopathComponent::GetPathPointAtIndex(int32 Index, FVector& OutPosition) const
{
	const TArray<FAeonixPathPoint>& PathPoints = CurrentPath.GetPathPoints();

	if (Index < 0 || Index >= PathPoints.Num())
	{
		OutPosition = FVector::ZeroVector;
		return false;
	}

	OutPosition = PathPoints[Index].Position;
	return true;
}

void UAeonixAutopathComponent::OnPathFindComplete(EAeonixPathFindStatus Status)
{
	bPathRequestPending = false;

	bool bSuccess = (Status == EAeonixPathFindStatus::Complete);

	// Broadcast to Blueprint
	OnPathUpdated.Broadcast(bSuccess);

	UE_LOG(LogAeonixNavigation, Verbose, TEXT("AutopathComponent %s: Pathfind complete (success=%d)"), *GetName(), bSuccess);
}
