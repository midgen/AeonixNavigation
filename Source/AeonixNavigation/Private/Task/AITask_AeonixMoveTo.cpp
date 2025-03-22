
#include <AeonixNavigation/Public/Task/AITask_AeonixMoveTo.h>

#include <AeonixNavigation/Public/Actor/AeonixBoundingVolume.h>
#include <AeonixNavigation/Public/Component/AeonixNavAgentComponent.h>
#include <AeonixNavigation/Public/Pathfinding/AeonixNavigationPath.h>
#include <AeonixNavigation/Public/AeonixNavigation.h>

#include <Runtime/AIModule/Classes/AIController.h>
#include <Runtime/AIModule/Classes/AIResources.h>
#include <Runtime/AIModule/Classes/AISystem.h>
#include <Runtime/Engine/Public/TimerManager.h>
#include <Runtime/Engine/Public/VisualLogger/VisualLogger.h>
#include <Runtime/GameplayTasks/Classes/GameplayTasksComponent.h>

UAITask_AeonixMoveTo::UAITask_AeonixMoveTo(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bIsPausable = true;
	MoveRequestID = FAIRequestID::InvalidRequest;

	MoveRequest.SetAcceptanceRadius(GET_AI_CONFIG_VAR(AcceptanceRadius));
	MoveRequest.SetReachTestIncludesAgentRadius(GET_AI_CONFIG_VAR(bFinishMoveOnGoalOverlap));
	MoveRequest.SetAllowPartialPath(GET_AI_CONFIG_VAR(bAcceptPartialPaths));
	MoveRequest.SetUsePathfinding(true);

	Result.Code = EAeonixPathfindingRequestResult::Failed;

	AddRequiredResource(UAIResource_Movement::StaticClass());
	AddClaimedResource(UAIResource_Movement::StaticClass());

	MoveResult = EPathFollowingResult::Invalid;

	Path = MakeShareable<FNavigationPath>(new FNavigationPath());
}

UAITask_AeonixMoveTo* UAITask_AeonixMoveTo::AeonixAIMoveTo(AAIController* Controller, FVector InGoalLocation, bool bUseAsyncPathfinding, AActor* InGoalActor,
	float AcceptanceRadius, EAIOptionFlag::Type StopOnOverlap, bool bLockAILogic, bool bUseContinuosGoalTracking)
{
	UAITask_AeonixMoveTo* MyTask = Controller ? UAITask::NewAITask<UAITask_AeonixMoveTo>(*Controller, EAITaskPriority::High) : nullptr;
	if (MyTask)
	{
		MyTask->bUseAsyncPathfinding = bUseAsyncPathfinding;
		// We need to tick the task if we're using async, to check when results are back
		MyTask->bTickingTask = bUseAsyncPathfinding;

		FAIMoveRequest MoveReq;
		if (InGoalActor)
		{
			MoveReq.SetGoalActor(InGoalActor);
		}
		else
		{
			MoveReq.SetGoalLocation(InGoalLocation);
		}

		MoveReq.SetAcceptanceRadius(AcceptanceRadius);
		MoveReq.SetReachTestIncludesAgentRadius(FAISystem::PickAIOption(StopOnOverlap, MoveReq.IsReachTestIncludingAgentRadius()));
		if (Controller)
		{
			MoveReq.SetNavigationFilter(Controller->GetDefaultNavigationFilterClass());
		}

		MyTask->SetUp(Controller, MoveReq, bUseAsyncPathfinding);
		MyTask->SetContinuousGoalTracking(bUseContinuosGoalTracking);

		if (bLockAILogic)
		{
			MyTask->RequestAILogicLocking();
		}

		MyTask->AeonixSubsystem = Controller->GetWorld()->GetSubsystem<UAeonixSubsystem>();
	}

	return MyTask;
}

void UAITask_AeonixMoveTo::SetUp(AAIController* Controller, const FAIMoveRequest& InMoveRequest, bool UseAsyncPathfinding)
{
	OwnerController = Controller;
	MoveRequest = InMoveRequest;
	bUseAsyncPathfinding = UseAsyncPathfinding;
	bTickingTask = bUseAsyncPathfinding;

	// Fail if no nav component
	NavComponent = Cast<UAeonixNavAgentComponent>(GetOwnerActor()->GetComponentByClass(UAeonixNavAgentComponent::StaticClass()));
	if (!NavComponent)
	{
		UE_VLOG(this, VAeonixNavigation, Error, TEXT("AeonixMoveTo request failed due missing AeonixNavComponent"), *MoveRequest.ToString());
		UE_LOG(AeonixNavigation, Error, TEXT("AeonixMoveTo request failed due missing AeonixNavComponent on the AIController"));
		return;
	}

	// Use the path instance from the navcomponent
	AeonixPath = &NavComponent->GetPath();
	AeonixSubsystem = Controller->GetWorld()->GetSubsystem<UAeonixSubsystem>();
}

void UAITask_AeonixMoveTo::SetContinuousGoalTracking(bool bEnable)
{
	bUseContinuousTracking = bEnable;
}

void UAITask_AeonixMoveTo::TickTask(float DeltaTime)
{
	UAeonixNavAgentComponent* AeonixNavComponent = Cast<UAeonixNavAgentComponent>(GetOwnerActor()->GetComponentByClass(UAeonixNavAgentComponent::StaticClass()));
	if (!AeonixNavComponent)
		return;

	int32 CurrentStatus = NavComponent->PathRequestStatus.GetValue();
	
	switch (CurrentStatus)
	{
	case 0 : break; // No path request, nothing to do
	case 1 : break; // Path request pending, waiting for response
	case 2 : // Path request is complete
		Result.Code = EAeonixPathfindingRequestResult::Success;
		RequestMove(); // Path request completed
		break;
	}
}

void UAITask_AeonixMoveTo::FinishMoveTask(EPathFollowingResult::Type InResult)
{
	if (MoveRequestID.IsValid())
	{
		UPathFollowingComponent* PFComp = OwnerController ? OwnerController->GetPathFollowingComponent() : nullptr;
		if (PFComp && PFComp->GetStatus() != EPathFollowingStatus::Idle)
		{
			ResetObservers();
			PFComp->AbortMove(*this, FPathFollowingResultFlags::OwnerFinished, MoveRequestID);
		}
	}

	MoveResult = InResult;
	EndTask();

	if (InResult == EPathFollowingResult::Invalid)
	{
		OnRequestFailed.Broadcast();
	}
	else
	{
		OnMoveFinished.Broadcast(InResult, OwnerController);
	}
}

void UAITask_AeonixMoveTo::Activate()
{
	Super::Activate();

	UE_CVLOG(bUseContinuousTracking, GetGameplayTasksComponent(), LogGameplayTasks, Log, TEXT("Continuous goal tracking requested, moving to: %s"),
		MoveRequest.IsMoveToActorRequest() ? TEXT("actor => looping successful moves!") : TEXT("location => will NOT loop"));

	MoveRequestID = FAIRequestID::InvalidRequest;
	ConditionalPerformMove();
}

void UAITask_AeonixMoveTo::ConditionalPerformMove()
{
	if (MoveRequest.IsUsingPathfinding() && OwnerController && OwnerController->ShouldPostponePathUpdates())
	{
		UE_VLOG(GetGameplayTasksComponent(), LogGameplayTasks, Log, TEXT("%s> can't path right now, waiting..."), *GetName());
		OwnerController->GetWorldTimerManager().SetTimer(MoveRetryTimerHandle, this, &UAITask_AeonixMoveTo::ConditionalPerformMove, 0.2f, false);
	}
	else
	{
		MoveRetryTimerHandle.Invalidate();
		PerformMove();
	}
}

void UAITask_AeonixMoveTo::PerformMove()
{
	// Prepare the move first (check for early out)
	CheckPathPreConditions();

	ResetObservers();
	ResetTimers();
	ResetPaths();

	if (Result.Code == EAeonixPathfindingRequestResult::AlreadyAtGoal)
	{
		MoveRequestID = Result.MoveId;
		OnRequestFinished(Result.MoveId, FPathFollowingResult(EPathFollowingResult::Success, FPathFollowingResultFlags::AlreadyAtGoal));
		return;
	}

	// If we're ready to path, then request the path
	if (Result.Code == EAeonixPathfindingRequestResult::ReadyToPath)
	{
		bUseAsyncPathfinding ? RequestPathAsync() : RequestPathSynchronous();

		switch (Result.Code)
		{
		case EAeonixPathfindingRequestResult::Failed:
			FinishMoveTask(EPathFollowingResult::Invalid);
			break;
		case EAeonixPathfindingRequestResult::Success: // Synchronous pathfinding
			MoveRequestID = Result.MoveId;
			if (IsFinished())
			{
				UE_VLOG(GetGameplayTasksComponent(), LogGameplayTasks, Error, TEXT("%s> re-Activating Finished task!"), *GetName());
			}
			RequestMove(); // StartLink the move
			break;
		case EAeonixPathfindingRequestResult::Deferred: // Async...we're waiting on the task to return
			MoveRequestID = Result.MoveId;
			break;
		default:
			checkNoEntry();
			break;
		}
	}
}

void UAITask_AeonixMoveTo::Pause()
{
	if (OwnerController && MoveRequestID.IsValid())
	{
		OwnerController->PauseMove(MoveRequestID);
	}

	ResetTimers();
	Super::Pause();
}

void UAITask_AeonixMoveTo::Resume()
{
	Super::Resume();

	if (!MoveRequestID.IsValid() || (OwnerController && !OwnerController->ResumeMove(MoveRequestID)))
	{
		UE_CVLOG(MoveRequestID.IsValid(), GetGameplayTasksComponent(), LogGameplayTasks, Log, TEXT("%s> Resume move failed, starting new one."), *GetName());
		ConditionalPerformMove();
	}
}

void UAITask_AeonixMoveTo::SetObservedPath(FNavPathSharedPtr InPath)
{
	if (PathUpdateDelegateHandle.IsValid() && Path.IsValid())
	{
		Path->RemoveObserver(PathUpdateDelegateHandle);
	}

	PathUpdateDelegateHandle.Reset();

	Path = InPath;
	if (Path.IsValid())
	{
		// disable auto repaths, it will be handled by move task to include ShouldPostponePathUpdates condition
		Path->EnableRecalculationOnInvalidation(false);
		PathUpdateDelegateHandle = Path->AddObserver(FNavigationPath::FPathObserverDelegate::FDelegate::CreateUObject(this, &UAITask_AeonixMoveTo::OnPathEvent));
	}
}

void UAITask_AeonixMoveTo::CheckPathPreConditions()
{
	UE_VLOG(this, VAeonixNavigation, Log, TEXT("AeonixMoveTo: %s"), *MoveRequest.ToString());

	Result.Code = EAeonixPathfindingRequestResult::Failed;

	// Fail if no nav component
	NavComponent = Cast<UAeonixNavAgentComponent>(GetOwnerActor()->GetComponentByClass(UAeonixNavAgentComponent::StaticClass()));
	if (!NavComponent)
	{
		UE_VLOG(this, VAeonixNavigation, Error, TEXT("AeonixMoveTo request failed due missing AeonixNavComponent on the pawn"), *MoveRequest.ToString());
		UE_LOG(AeonixNavigation, Error, TEXT("AeonixMoveTo request failed due missing AeonixNavComponent on the pawn"));
		return;
	}

	if (MoveRequest.IsValid() == false)
	{
		UE_VLOG(this, VAeonixNavigation, Error, TEXT("AeonixMoveTo request failed due MoveRequest not being valid. Most probably desired Goal Actor not longer exists"), *MoveRequest.ToString());
		UE_LOG(AeonixNavigation, Error, TEXT("AeonixMoveTo request failed due MoveRequest not being valid. Most probably desired Goal Actor not longer exists"));
		return;
	}

	if (OwnerController->GetPathFollowingComponent() == nullptr)
	{
		UE_VLOG(this, VAeonixNavigation, Error, TEXT("AeonixMoveTo request failed due missing PathFollowingComponent"));
		UE_LOG(AeonixNavigation, Error, TEXT("AeonixMoveTo request failed due missing PathFollowingComponent"));
		return;
	}

	bool bCanRequestMove = true;
	bool bAlreadyAtGoal = false;

	if (!MoveRequest.IsMoveToActorRequest())
	{
		if (MoveRequest.GetGoalLocation().ContainsNaN() || FAISystem::IsValidLocation(MoveRequest.GetGoalLocation()) == false)
		{
			UE_VLOG(this, VAeonixNavigation, Error, TEXT("AeonixMoveTo: Destination is not valid! Goal(%s)"), TEXT_AI_LOCATION(MoveRequest.GetGoalLocation()));
			UE_LOG(AeonixNavigation, Error, TEXT("AeonixMoveTo: Destination is not valid! Goal(%s)"), TEXT_AI_LOCATION(MoveRequest.GetGoalLocation()));
			bCanRequestMove = false;
		}

		bAlreadyAtGoal = bCanRequestMove && OwnerController->GetPathFollowingComponent()->HasReached(MoveRequest);
	}
	else
	{
		bAlreadyAtGoal = bCanRequestMove && OwnerController->GetPathFollowingComponent()->HasReached(MoveRequest);
	}

	if (bAlreadyAtGoal)
	{
		UE_VLOG(this, VAeonixNavigation, Log, TEXT("AeonixMoveTo: already at goal!"));
		UE_LOG(AeonixNavigation, Log, TEXT("AeonixMoveTo: already at goal!"));
		Result.MoveId = OwnerController->GetPathFollowingComponent()->RequestMoveWithImmediateFinish(EPathFollowingResult::Success);
		Result.Code = EAeonixPathfindingRequestResult::AlreadyAtGoal;
	}

	if (bCanRequestMove)
	{
		Result.Code = EAeonixPathfindingRequestResult::ReadyToPath;
	}

	return;
}

void UAITask_AeonixMoveTo::RequestPathSynchronous()
{
	Result.Code = EAeonixPathfindingRequestResult::Failed;

	UE_VLOG(this, VAeonixNavigation, Log, TEXT("AeonixMoveTo: Requesting Synchronous pathfinding!"));
	UE_LOG(AeonixNavigation, Log, TEXT("AeonixMoveTo: Requesting Synchronous pathfinding!"));
	
	if (AeonixSubsystem->FindPathImmediateAgent(NavComponent, MoveRequest.GetGoalLocation(), NavComponent->GetPath()))
	{
		Result.Code = EAeonixPathfindingRequestResult::Success;
	}
	return;
}

void UAITask_AeonixMoveTo::RequestPathAsync()
{
	Result.Code = EAeonixPathfindingRequestResult::Failed;

	// Fail if no nav component
	UAeonixNavAgentComponent* AeonixNavComponent = Cast<UAeonixNavAgentComponent>(GetOwnerActor()->GetComponentByClass(UAeonixNavAgentComponent::StaticClass()));
	if (!AeonixNavComponent)
		return;

	// Reset status to zero
	AeonixNavComponent->PathRequestStatus.Reset();
	
	if (AeonixSubsystem->FindPathAsyncAgent(NavComponent, MoveRequest.GetGoalLocation(), NavComponent->GetPath()))
	{
		Result.Code = EAeonixPathfindingRequestResult::Deferred;
	}
}

/* Requests the move, based on the current path */
void UAITask_AeonixMoveTo::RequestMove()
{
	Result.Code = EAeonixPathfindingRequestResult::Failed;

	LogPathHelper();

	// Copy the SVO path into a regular path for now, until we implement our own path follower.
	AeonixPath->CreateNavPath(*Path);
	Path->MarkReady();

	UPathFollowingComponent* PFComp = OwnerController ? OwnerController->GetPathFollowingComponent() : nullptr;
	if (PFComp == nullptr)
	{
		FinishMoveTask(EPathFollowingResult::Invalid);
		return;
	}

	PathFinishDelegateHandle = PFComp->OnRequestFinished.AddUObject(this, &UAITask_AeonixMoveTo::OnRequestFinished);
	SetObservedPath(Path);

	const FAIRequestID RequestId = Path->IsValid() ? OwnerController->RequestMove(MoveRequest, Path) : FAIRequestID::InvalidRequest;

	if (RequestId.IsValid())
	{
		UE_VLOG(this, VAeonixNavigation, Log, TEXT("Aeonix Pathfinding successful, moving"));
		UE_LOG(AeonixNavigation, Log, TEXT("Aeonix Pathfinding successful, moving"));
		Result.MoveId = RequestId;
		Result.Code = EAeonixPathfindingRequestResult::Success;
	}

	if (Result.Code == EAeonixPathfindingRequestResult::Failed)
	{
		Result.MoveId = OwnerController->GetPathFollowingComponent()->RequestMoveWithImmediateFinish(EPathFollowingResult::Invalid);
	}
}

// void UAITask_AeonixMoveTo::HandleAsyncPathTaskComplete()
// {
// 	Result.Code = EAeonixPathfindingRequestResult::Success;
// 	// Request the move
// 	RequestMove();
// 	// Flag that we've processed the task
// 	AsyncTaskComplete = false;
// }

void UAITask_AeonixMoveTo::ResetPaths()
{
	if (Path.IsValid())
		Path->ResetForRepath();

	if (AeonixPath)
	{
		AeonixPath->ResetForRepath();
	}
}

/** Renders the octree path as a 3d tunnel in the visual logger */
void UAITask_AeonixMoveTo::LogPathHelper()
{
#if WITH_EDITOR
#if ENABLE_VISUAL_LOG

	// UAeonixNavAgentComponent* AeonixNavComponent = Cast<UAeonixNavAgentComponent>(GetOwnerActor()->GetComponentByClass(UAeonixNavAgentComponent::StaticClass()));
	// if (!AeonixNavComponent)
	// 	return;
	//
	// FVisualLogger& Vlog = FVisualLogger::Get();
	// if (Vlog.IsRecording() &&
	// 	AeonixPath.IsValid() && AeonixPath.Get()->GetPathPoints().Num())
	// {
	//
	// 	FVisualLogEntry* Entry = FVisualLogger::GetEntryToWrite(OwnerController->GetPawn(), VAeonixNavigation);
	// 	if (Entry)
	// 	{
	// 		for (int i = 0; i < AeonixPath->GetPathPoints().Num(); i++)
	// 		{
	// 			if (i == 0 || i == AeonixPath->GetPathPoints().Num() - 1)
	// 				continue;
	//
	// 			const FAeonixPathPoint& Point = AeonixPath->GetPathPoints()[i];
	//
	// 			float size = 0.f;
	//
	// 			if (Point.Layer == 0)
	// 			{
	// 				size = AeonixNavComponent->GetCurrentVolume()->GetNavData().GetVoxelSize(0) * 0.25f;
	// 			}
	// 			else
	// 			{
	// 				size = AeonixNavComponent->GetCurrentVolume()->GetNavData().GetVoxelSize(Point.Layer - 1);
	// 			}
	//
	// 			UE_VLOG_BOX(OwnerController->GetPawn(), VAeonixNavigation, Verbose, FBox(Point.Position + FVector(size * 0.5f), Point.Position - FVector(size * 0.5f)), FColor::Black, TEXT_EMPTY);
	// 		}
	// 	}
	// }
#endif // ENABLE_VISUAL_LOG
#endif // WITH_EDITOR
}

void UAITask_AeonixMoveTo::ResetObservers()
{
	if (Path.IsValid())
	{
		Path->DisableGoalActorObservation();
	}

	if (PathFinishDelegateHandle.IsValid())
	{
		UPathFollowingComponent* PFComp = OwnerController ? OwnerController->GetPathFollowingComponent() : nullptr;
		if (PFComp)
		{
			PFComp->OnRequestFinished.Remove(PathFinishDelegateHandle);
		}

		PathFinishDelegateHandle.Reset();
	}

	if (PathUpdateDelegateHandle.IsValid())
	{
		if (Path.IsValid())
		{
			Path->RemoveObserver(PathUpdateDelegateHandle);
		}

		PathUpdateDelegateHandle.Reset();
	}
}

void UAITask_AeonixMoveTo::ResetTimers()
{
	if (MoveRetryTimerHandle.IsValid())
	{
		if (OwnerController)
		{
			OwnerController->GetWorldTimerManager().ClearTimer(MoveRetryTimerHandle);
		}

		MoveRetryTimerHandle.Invalidate();
	}

	if (PathRetryTimerHandle.IsValid())
	{
		if (OwnerController)
		{
			OwnerController->GetWorldTimerManager().ClearTimer(PathRetryTimerHandle);
		}

		PathRetryTimerHandle.Invalidate();
	}
}

void UAITask_AeonixMoveTo::OnDestroy(bool bInOwnerFinished)
{
	Super::OnDestroy(bInOwnerFinished);

	ResetObservers();
	ResetTimers();

	if (MoveRequestID.IsValid())
	{
		UPathFollowingComponent* PfComp = OwnerController ? OwnerController->GetPathFollowingComponent() : nullptr;
		if (PfComp && PfComp->GetStatus() != EPathFollowingStatus::Idle)
		{
			PfComp->AbortMove(*this, FPathFollowingResultFlags::OwnerFinished, MoveRequestID);
		}
	}

	// clear the shared pointer now to make sure other systems
	// don't think this path is still being used
	Path = nullptr;
	AeonixPath = nullptr;
}

void UAITask_AeonixMoveTo::OnRequestFinished(FAIRequestID RequestId, const FPathFollowingResult& InResult)
{
	FinishMoveTask(InResult.Code);
}

void UAITask_AeonixMoveTo::OnPathEvent(FNavigationPath* InPath, ENavPathEvent::Type Event)
{
	const static UEnum* NavPathEventEnum = FindObject<UEnum>(this, TEXT("ENavPathEvent"));
	UE_VLOG(GetGameplayTasksComponent(), LogGameplayTasks, Log, TEXT("%s> Path event: %s"), *GetName(), *NavPathEventEnum->GetNameStringByValue(Event));

	switch (Event)
	{
	case ENavPathEvent::NewPath:
	case ENavPathEvent::UpdatedDueToGoalMoved:
	case ENavPathEvent::UpdatedDueToNavigationChanged:
		if (InPath && InPath->IsPartial() && !MoveRequest.IsUsingPartialPaths())
		{
			UE_VLOG(GetGameplayTasksComponent(), LogGameplayTasks, Log, TEXT(">> partial path is not allowed, aborting"));
			UPathFollowingComponent::LogPathHelper(OwnerController, InPath, MoveRequest.GetGoalActor());
			FinishMoveTask(EPathFollowingResult::Aborted);
		}
#if ENABLE_VISUAL_LOG
		else if (!IsActive())
		{
			UPathFollowingComponent::LogPathHelper(OwnerController, InPath, MoveRequest.GetGoalActor());
		}
#endif // ENABLE_VISUAL_LOG
		break;

	case ENavPathEvent::Invalidated:
		ConditionalUpdatePath();
		break;

	case ENavPathEvent::Cleared:
	case ENavPathEvent::RePathFailed:
		UE_VLOG(GetGameplayTasksComponent(), LogGameplayTasks, Log, TEXT(">> no path, aborting!"));
		FinishMoveTask(EPathFollowingResult::Aborted);
		break;

	case ENavPathEvent::MetaPathUpdate:
	default:
		break;
	}
}

void UAITask_AeonixMoveTo::ConditionalUpdatePath()
{
	// mark this path as waiting for repath so that PathFollowingComponent doesn't abort the move while we
	// micro manage repathing moment
	// note that this flag fill get cleared upon repathing end
	if (Path.IsValid())
	{
		Path->SetManualRepathWaiting(true);
	}

	if (MoveRequest.IsUsingPathfinding() && OwnerController && OwnerController->ShouldPostponePathUpdates())
	{
		UE_VLOG(GetGameplayTasksComponent(), LogGameplayTasks, Log, TEXT("%s> can't path right now, waiting..."), *GetName());
		OwnerController->GetWorldTimerManager().SetTimer(PathRetryTimerHandle, this, &UAITask_AeonixMoveTo::ConditionalUpdatePath, 0.2f, false);
	}
	else
	{
		PathRetryTimerHandle.Invalidate();

		ANavigationData* NavData = Path.IsValid() ? Path->GetNavigationDataUsed() : nullptr;
		if (NavData)
		{
			NavData->RequestRePath(Path, ENavPathUpdateType::NavigationChanged);
		}
		else
		{
			UE_VLOG(GetGameplayTasksComponent(), LogGameplayTasks, Log, TEXT("%s> unable to repath, aborting!"), *GetName());
			FinishMoveTask(EPathFollowingResult::Aborted);
		}
	}
}
