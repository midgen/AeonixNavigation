#include "Subsystem/AeonixAutopathSubsystem.h"
#include "Component/AeonixAutopathComponent.h"
#include "Component/AeonixAutopathTargetComponent.h"
#include "Subsystem/AeonixSubsystem.h"
#include "Component/AeonixNavAgentComponent.h"
#include "AeonixNavigation.h"

#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Controller.h"

void UAeonixAutopathSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	UE_LOG(LogAeonixNavigation, Verbose, TEXT("AeonixAutopathSubsystem: Initialized"));
}

void UAeonixAutopathSubsystem::Deinitialize()
{
	RegisteredSources.Empty();
	RegisteredTarget = nullptr;
	SourceLastPositionMap.Empty();
	TargetLastPosition = FVector::ZeroVector;
	bTargetPositionInitialized = false;

	UE_LOG(LogAeonixNavigation, Verbose, TEXT("AeonixAutopathSubsystem: Deinitialized"));

	Super::Deinitialize();
}

void UAeonixAutopathSubsystem::Tick(float DeltaTime)
{
	ProcessAutopathSources();
}

TStatId UAeonixAutopathSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UAeonixAutopathSubsystem, STATGROUP_Tickables);
}

bool UAeonixAutopathSubsystem::IsTickable() const
{
	return RegisteredSources.Num() > 0;
}

bool UAeonixAutopathSubsystem::IsTickableInEditor() const
{
	return false;
}

bool UAeonixAutopathSubsystem::IsTickableWhenPaused() const
{
	return false;
}

bool UAeonixAutopathSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	return WorldType == EWorldType::Game || WorldType == EWorldType::PIE;
}

void UAeonixAutopathSubsystem::RegisterAutopathSource(UAeonixAutopathComponent* Component)
{
	if (!Component)
	{
		return;
	}

	if (!RegisteredSources.Contains(Component))
	{
		RegisteredSources.Add(Component);
		UE_LOG(LogAeonixNavigation, Verbose, TEXT("AeonixAutopathSubsystem: Registered source %s"), *Component->GetName());
	}
}

void UAeonixAutopathSubsystem::UnregisterAutopathSource(UAeonixAutopathComponent* Component)
{
	if (!Component)
	{
		return;
	}

	RegisteredSources.Remove(Component);

	// Clean up position tracking for this component's owner
	if (AActor* Owner = Component->GetOwner())
	{
		SourceLastPositionMap.Remove(Owner);
	}

	UE_LOG(LogAeonixNavigation, Verbose, TEXT("AeonixAutopathSubsystem: Unregistered source %s"), *Component->GetName());
}

void UAeonixAutopathSubsystem::RegisterAutopathTarget(UAeonixAutopathTargetComponent* Component)
{
	if (!Component)
	{
		return;
	}

	// Only one target allowed
	if (RegisteredTarget != nullptr && RegisteredTarget != Component)
	{
		UE_LOG(LogAeonixNavigation, Error, TEXT("AeonixAutopathSubsystem: Cannot register target %s - a target (%s) is already registered. Only one target is allowed."),
			*Component->GetName(), *RegisteredTarget->GetName());
		return;
	}

	RegisteredTarget = Component;
	bTargetPositionInitialized = false;
	UE_LOG(LogAeonixNavigation, Verbose, TEXT("AeonixAutopathSubsystem: Registered target %s"), *Component->GetName());
}

void UAeonixAutopathSubsystem::UnregisterAutopathTarget(UAeonixAutopathTargetComponent* Component)
{
	if (!Component)
	{
		return;
	}

	if (RegisteredTarget == Component)
	{
		RegisteredTarget = nullptr;
		TargetLastPosition = FVector::ZeroVector;
		bTargetPositionInitialized = false;
		UE_LOG(LogAeonixNavigation, Verbose, TEXT("AeonixAutopathSubsystem: Unregistered target %s"), *Component->GetName());
	}
}

void UAeonixAutopathSubsystem::ProcessAutopathSources()
{
	// Must have a registered target
	if (!RegisteredTarget || !IsValid(RegisteredTarget) || !RegisteredTarget->bEnableAutopath)
	{
		return;
	}

	AActor* TargetOwner = RegisteredTarget->GetOwner();
	if (!TargetOwner)
	{
		return;
	}

	// Clean stale entries from source position map
	for (auto It = SourceLastPositionMap.CreateIterator(); It; ++It)
	{
		if (!It.Key() || !IsValid(It.Key()))
		{
			It.RemoveCurrent();
		}
	}

	// Check if target moved beyond threshold
	const FVector CurrentTargetPos = TargetOwner->GetActorLocation();
	bool bTargetMoved = false;

	if (!bTargetPositionInitialized)
	{
		TargetLastPosition = CurrentTargetPos;
		bTargetPositionInitialized = true;
		bTargetMoved = true; // First time - trigger paths for all sources
	}
	else
	{
		const float TargetDistSq = FVector::DistSquared(CurrentTargetPos, TargetLastPosition);
		const float TargetThresholdSq = RegisteredTarget->PositionThreshold * RegisteredTarget->PositionThreshold;
		if (TargetDistSq > TargetThresholdSq)
		{
			bTargetMoved = true;
			TargetLastPosition = CurrentTargetPos;
		}
	}

	// Process each source (iterate backwards for safe removal)
	for (int32 i = RegisteredSources.Num() - 1; i >= 0; i--)
	{
		UAeonixAutopathComponent* Source = RegisteredSources[i];

		// Validate source
		if (!Source || !IsValid(Source))
		{
			RegisteredSources.RemoveAtSwap(i, EAllowShrinking::No);
			continue;
		}

		// Skip if disabled
		if (!Source->bEnableAutopath)
		{
			continue;
		}

		AActor* SourceOwner = Source->GetOwner();
		if (!SourceOwner)
		{
			continue;
		}

		// Skip if already have pending request
		if (Source->IsPathRequestPending())
		{
			continue;
		}

		// Check if source moved beyond threshold
		bool bSourceMoved = HasMovedBeyondThreshold(SourceOwner, SourceLastPositionMap, Source->PositionThreshold);

		if (bSourceMoved || bTargetMoved)
		{
			RequestAsyncPathfinding(Source);

			// Update tracked source position
			SourceLastPositionMap.Add(SourceOwner, SourceOwner->GetActorLocation());
		}

		// Update path progression for this source
		if (Source->HasValidPath())
		{
			Source->UpdatePathProgression(SourceOwner->GetActorLocation());
		}
	}
}

bool UAeonixAutopathSubsystem::HasMovedBeyondThreshold(AActor* Actor, TMap<AActor*, FVector>& LastPositionMap, float Threshold)
{
	if (!Actor)
	{
		return false;
	}

	const FVector CurrentPos = Actor->GetActorLocation();
	FVector* LastPos = LastPositionMap.Find(Actor);

	if (!LastPos)
	{
		// First time seeing this actor - initialize and trigger path
		LastPositionMap.Add(Actor, CurrentPos);
		return true;
	}

	// Use squared distance for performance
	const float DistSq = FVector::DistSquared(CurrentPos, *LastPos);
	const float ThresholdSq = Threshold * Threshold;

	return DistSq > ThresholdSq;
}

void UAeonixAutopathSubsystem::RequestAsyncPathfinding(UAeonixAutopathComponent* Source)
{
	if (!Source)
	{
		return;
	}

	// Must have a registered target
	if (!RegisteredTarget || !RegisteredTarget->GetOwner())
	{
		return;
	}

	// Get nav subsystem
	UAeonixSubsystem* NavSubsystem = GetWorld()->GetSubsystem<UAeonixSubsystem>();
	if (!NavSubsystem)
	{
		UE_LOG(LogAeonixNavigation, Warning, TEXT("AeonixAutopathSubsystem: No UAeonixSubsystem found"));
		return;
	}

	// Get nav agent component - first try the component's cached reference,
	// then try the actor directly, then try the actor's controller (for pawns)
	UAeonixNavAgentComponent* NavAgent = Source->GetNavAgentComponent();
	if (!NavAgent)
	{
		AActor* SourceOwner = Source->GetOwner();
		if (SourceOwner)
		{
			// Try finding on the actor directly
			NavAgent = SourceOwner->FindComponentByClass<UAeonixNavAgentComponent>();

			// If not found and actor is a pawn, try the controller
			if (!NavAgent)
			{
				if (APawn* Pawn = Cast<APawn>(SourceOwner))
				{
					if (AController* Controller = Pawn->GetController())
					{
						NavAgent = Controller->FindComponentByClass<UAeonixNavAgentComponent>();
					}
				}
			}
		}
	}

	if (!NavAgent)
	{
		UE_LOG(LogAeonixNavigation, Warning, TEXT("AeonixAutopathSubsystem: Source %s has no NavAgentComponent (checked actor and controller)"), *Source->GetName());
		return;
	}

	// Get target position with offset from nav agent
	FVector TargetPos = RegisteredTarget->GetOwner()->GetActorLocation() + NavAgent->EndPointOffset;

	// Mark as pending on component
	Source->SetPathRequestPending(true);

	// Reset path for new pathfinding
	Source->GetNavigationPath().ResetForRepath();

	// Request async pathfinding
	FAeonixPathFindRequestCompleteDelegate& Delegate =
		NavSubsystem->FindPathAsyncAgent(NavAgent, TargetPos, Source->GetNavigationPath());

	// Bind completion callback to the component's UFUNCTION
	Delegate.BindDynamic(Source, &UAeonixAutopathComponent::OnPathFindComplete);

	UE_LOG(LogAeonixNavigation, Verbose, TEXT("AeonixAutopathSubsystem: Requested async pathfind for %s"), *Source->GetName());
}
