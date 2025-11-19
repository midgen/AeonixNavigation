// Copyright 2024 Chris Ashworth



#include "AenoixEditorDebugSubsystem.h"
#include "AeonixPathDebugActor.h"

#include "Actor/AeonixBoundingVolume.h"
#include "Component/AeonixNavAgentComponent.h"
#include "AeonixEditor/AeonixEditor.h"
#include "Debug/AeonixDebugDrawManager.h"

#include "Subsystem/AeonixSubsystem.h"

#include "EngineUtils.h"
#include "DrawDebugHelpers.h"

void UAenoixEditorDebugSubsystem::UpdateDebugActor(AAeonixPathDebugActor* DebugActor)
{
	if (!DebugActor)
	{
		UE_LOG(LogAeonixEditor, Warning, TEXT("UpdateDebugActor called with null DebugActor"));
		return;
	}

	if (DebugActor->DebugType == EAeonixPathDebugActorType::START)
	{
		StartDebugActor = DebugActor;
		UE_LOG(LogAeonixEditor, Log, TEXT("Updated START debug actor at %s"), *DebugActor->GetActorLocation().ToString());
	}
	else
	{
		EndDebugActor = DebugActor;
		UE_LOG(LogAeonixEditor, Log, TEXT("Updated END debug actor at %s"), *DebugActor->GetActorLocation().ToString());
	}

	// Flag that we need to redraw due to actor change
	bNeedsRedraw = true;

	 UAeonixSubsystem* AeonixSubsystem = DebugActor->GetWorld()->GetSubsystem<UAeonixSubsystem>();

	// If we've got a valid start and end target
	if (StartDebugActor && EndDebugActor && !bIsPathPending)
	{
		// Ensure we're subscribed to navigation regeneration events
		BindToBoundingVolumes();

		UE_LOG(LogAeonixEditor, Log, TEXT("Both START and END actors set, attempting pathfinding..."));
		if (AAeonixBoundingVolume* Volume = AeonixSubsystem->GetMutableVolumeForAgent(StartDebugActor->NavAgentComponent))
		{
			// Don't attempt pathfinding if the volume isn't ready (prevents hangs during asset loading)
			if (!Volume->bIsReadyForNavigation)
			{
				UE_LOG(LogAeonixEditor, Warning, TEXT("Volume not ready for navigation - skipping pathfinding"));
				return;
			}
			Volume->UpdateBounds();
			UE_LOG(LogAeonixEditor, Log, TEXT("Requesting pathfind from %s to %s"), *StartDebugActor->GetActorLocation().ToString(), *EndDebugActor->GetActorLocation().ToString());
			FAeonixPathFindRequestCompleteDelegate& PathRequestCompleteDelegate = AeonixSubsystem->FindPathAsyncAgent(StartDebugActor->NavAgentComponent, EndDebugActor->GetActorLocation(), CurrentDebugPath);
			PathRequestCompleteDelegate.BindDynamic(this, &UAenoixEditorDebugSubsystem::OnPathFindComplete);
			bIsPathPending = true;
		}
		else
		{
			UE_LOG(LogAeonixEditor, Warning, TEXT("Failed to get volume for agent"));
		}

	}
	else
	{
		if (!StartDebugActor)
		{
			UE_LOG(LogAeonixEditor, Log, TEXT("No START actor set yet"));
		}
		if (!EndDebugActor)
		{
			UE_LOG(LogAeonixEditor, Log, TEXT("No END actor set yet"));
		}
		if (bIsPathPending)
		{
			UE_LOG(LogAeonixEditor, Log, TEXT("Path already pending"));
		}
	}
}

void UAenoixEditorDebugSubsystem::OnPathFindComplete(EAeonixPathFindStatus Status)
{
	FScopeLock Lock(&PathMutex);

	if (Status == EAeonixPathFindStatus::Complete)
	{
		CurrentDebugPath.SetIsReady(true);
		// Cache the successfully calculated path
		CachedDebugPath = CurrentDebugPath;
		bHasValidCachedPath = true;
		bIsPathPending = false;
		// Flag that we need to redraw the new path
		bNeedsRedraw = true;
		UE_LOG(LogAeonixEditor, Log, TEXT("Pathfinding COMPLETE - path ready to draw with %d waypoints"), CurrentDebugPath.GetPathPoints().Num());
	}
	else if (Status == EAeonixPathFindStatus::Failed)
	{
		CurrentDebugPath.SetIsReady(false);
		bIsPathPending = false;
		// Keep showing the cached path even if new calculation failed
		UE_LOG(LogAeonixEditor, Warning, TEXT("Pathfinding FAILED"));
	}
	else
	{
		UE_LOG(LogAeonixEditor, Error, TEXT("Unhandled pathfinding state"));
	}
}

void UAenoixEditorDebugSubsystem::Tick(float DeltaTime)
{
	// Validate StartDebugActor before use
	if (!StartDebugActor.IsValid() || !IsValid(StartDebugActor.Get()))
	{
		return;
	}

	UAeonixSubsystem* AeonixSubsystem = StartDebugActor->GetWorld()->GetSubsystem<UAeonixSubsystem>();

	// If we've got a valid start and end target
	if (StartDebugActor.IsValid() && EndDebugActor.IsValid() && !bIsPathPending && !CurrentDebugPath.IsReady())
	{
		// this is just needed to deal with the lifetime of things in the editor world
		if (AAeonixBoundingVolume* Volume = AeonixSubsystem->GetMutableVolumeForAgent(StartDebugActor->NavAgentComponent))
		{
			// Don't attempt pathfinding if the volume isn't ready (prevents hangs during asset loading)
			if (!Volume->bIsReadyForNavigation)
			{
				return;
			}
			Volume->UpdateBounds();
		}
		FAeonixPathFindRequestCompleteDelegate& PathRequestCompleteDelegate = AeonixSubsystem->FindPathAsyncAgent(StartDebugActor->NavAgentComponent, EndDebugActor->GetActorLocation(), CurrentDebugPath);
		PathRequestCompleteDelegate.BindDynamic(this, &UAenoixEditorDebugSubsystem::OnPathFindComplete);
		bIsPathPending = true;
	}
	
	if (StartDebugActor.IsValid())
	{
		// Note: Removed FlushPersistentDebugLines to allow octree debug visualization to persist
		// Path debug lines now use non-persistent durations instead
	}

	// Only draw when we need to redraw (not every frame)
	if (bNeedsRedraw && StartDebugActor.IsValid())
	{
		UWorld* World = StartDebugActor->GetWorld();
		// Skip debug drawing if world is invalid or not in a playable state (e.g., during save)
		if (!World || !IsValid(World) || !AeonixSubsystem)
		{
			bNeedsRedraw = false;
			return;
		}

		// Clear only path debug visualization using the debug manager (doesn't affect other systems)
		if (UAeonixDebugDrawManager* DebugManager = World->GetSubsystem<UAeonixDebugDrawManager>())
		{
			DebugManager->Clear(EAeonixDebugCategory::Paths);
		}

		// Lock mutex to safely read path data while async pathfinding may be writing
		{
			FScopeLock Lock(&PathMutex);

			// Get the volume and validate it before drawing
			AAeonixBoundingVolume* Volume = AeonixSubsystem->GetMutableVolumeForAgent(StartDebugActor->NavAgentComponent);
			if (Volume && IsValid(Volume))
			{
				// Draw the current path if ready, otherwise draw the cached path
				if (CurrentDebugPath.IsReady())
				{
					CurrentDebugPath.DebugDraw(World, Volume->GetNavData());
				}
				else if (bHasValidCachedPath)
				{
					// Show cached path while recalculating
					CachedDebugPath.DebugDraw(World, Volume->GetNavData());
				}
			}
		}

		// Reset flag after drawing
		bNeedsRedraw = false;
	}

	// Draw batch run paths only when they change (not every frame)
	if (bBatchPathsNeedRedraw && BatchRunPaths.Num() > 0 && StartDebugActor.IsValid())
	{
		UWorld* World = StartDebugActor->GetWorld();
		if (World)
		{
			FScopeLock Lock(&PathMutex);
			for (const FAeonixNavigationPath& Path : BatchRunPaths)
			{
				Path.DebugDrawLite(World, FColor::Cyan, -1.0f); // Use persistent lines
			}
		}
		bBatchPathsNeedRedraw = false;
	}

	// Draw failed batch run paths only when they change (not every frame)
	if (bFailedPathsNeedRedraw && FailedBatchRunPaths.Num() > 0 && StartDebugActor.IsValid())
	{
		UWorld* World = StartDebugActor->GetWorld();
		if (World)
		{
			if (UAeonixDebugDrawManager* DebugManager = World->GetSubsystem<UAeonixDebugDrawManager>())
			{
				FScopeLock Lock(&PathMutex);
				for (const FAeonixFailedPath& FailedPath : FailedBatchRunPaths)
				{
					// Draw thick red line from start to end for failed paths
					DebugManager->AddLine(FailedPath.StartPoint, FailedPath.EndPoint, FColor::Red, 8.0f, EAeonixDebugCategory::Tests);

					// Draw spheres at endpoints for better visibility
					DebugManager->AddSphere(FailedPath.StartPoint, 30.0f, 8, FColor::Yellow, EAeonixDebugCategory::Tests);
					DebugManager->AddSphere(FailedPath.EndPoint, 30.0f, 8, FColor::Red, EAeonixDebugCategory::Tests);
				}
			}
		}
		bFailedPathsNeedRedraw = false;
	}
}

TStatId UAenoixEditorDebugSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UAenoixEditorDebugSubsystem, STATGROUP_Tickables);
}

bool UAenoixEditorDebugSubsystem::IsTickable() const 
{
	return true;
}

bool UAenoixEditorDebugSubsystem::IsTickableInEditor() const 
{
	return true;
}

bool UAenoixEditorDebugSubsystem::IsTickableWhenPaused() const
{
	return true;
}

void UAenoixEditorDebugSubsystem::ClearDebugActor(AAeonixPathDebugActor* ActorToRemove)
{
	if (!ActorToRemove)
	{
		return;
	}

	// Clear references to the actor being removed
	if (StartDebugActor.Get() == ActorToRemove)
	{
		StartDebugActor = nullptr;
		// Clear any ongoing pathfinding when start actor is removed
		bIsPathPending = false;
		CurrentDebugPath.ResetForRepath();
	}

	if (EndDebugActor.Get() == ActorToRemove)
	{
		EndDebugActor = nullptr;
		// Clear any ongoing pathfinding when end actor is removed
		bIsPathPending = false;
		CurrentDebugPath.ResetForRepath();
	}

	// If both actors are now cleared, unbind from bounding volumes
	if (!StartDebugActor.IsValid() && !EndDebugActor.IsValid())
	{
		UnbindFromBoundingVolumes();
	}
}

void UAenoixEditorDebugSubsystem::ClearCachedPath()
{
	bHasValidCachedPath = false;
	CachedDebugPath.ResetForRepath();
}


void UAenoixEditorDebugSubsystem::SetBatchRunPaths(const TArray<FAeonixNavigationPath>& Paths)
{
	FScopeLock Lock(&PathMutex);
	BatchRunPaths = Paths;
	bBatchPathsNeedRedraw = true;
	UE_LOG(LogAeonixEditor, Log, TEXT("Debug subsystem received %d batch run paths for visualization"), Paths.Num());
}

void UAenoixEditorDebugSubsystem::ClearBatchRunPaths()
{
	FScopeLock Lock(&PathMutex);
	BatchRunPaths.Empty();
	FailedBatchRunPaths.Empty(); // Also clear failed paths
	bBatchPathsNeedRedraw = false;
	bFailedPathsNeedRedraw = false;
	UE_LOG(LogAeonixEditor, VeryVerbose, TEXT("Debug subsystem cleared batch run paths"));
}

void UAenoixEditorDebugSubsystem::SetFailedBatchRunPaths(const TArray<TPair<FVector, FVector>>& FailedPaths)
{
	FScopeLock Lock(&PathMutex);
	FailedBatchRunPaths.Empty();
	for (const auto& Path : FailedPaths)
	{
		FailedBatchRunPaths.Add(FAeonixFailedPath(Path.Key, Path.Value));
	}
	bFailedPathsNeedRedraw = true;
	UE_LOG(LogAeonixEditor, Log, TEXT("Debug subsystem received %d failed batch run paths for visualization"), FailedPaths.Num());
}

void UAenoixEditorDebugSubsystem::ClearFailedBatchRunPaths()
{
	FScopeLock Lock(&PathMutex);
	FailedBatchRunPaths.Empty();
	bFailedPathsNeedRedraw = false;
	UE_LOG(LogAeonixEditor, VeryVerbose, TEXT("Debug subsystem cleared failed batch run paths"));
}

void UAenoixEditorDebugSubsystem::BindToBoundingVolumes()
{
	if (!StartDebugActor.IsValid() || !StartDebugActor->GetWorld())
	{
		return;
	}

	// Subscribe to the subsystem's centralized regeneration delegate
	UAeonixSubsystem* AeonixSubsystem = StartDebugActor->GetWorld()->GetSubsystem<UAeonixSubsystem>();
	if (AeonixSubsystem)
	{
		AeonixSubsystem->GetOnNavigationRegenCompleted().AddUObject(this, &UAenoixEditorDebugSubsystem::OnBoundingVolumeRegenerated);
	}

	UE_LOG(LogAeonixEditor, Verbose, TEXT("Debug path subsystem bound to subsystem regeneration delegate"));
}

void UAenoixEditorDebugSubsystem::UnbindFromBoundingVolumes()
{
	if (!StartDebugActor.IsValid() || !StartDebugActor->GetWorld())
	{
		return;
	}

	// Unsubscribe from the subsystem's centralized regeneration delegate
	UAeonixSubsystem* AeonixSubsystem = StartDebugActor->GetWorld()->GetSubsystem<UAeonixSubsystem>();
	if (AeonixSubsystem)
	{
		AeonixSubsystem->GetOnNavigationRegenCompleted().RemoveAll(this);
	}

	UE_LOG(LogAeonixEditor, Verbose, TEXT("Debug path subsystem unbound from subsystem regeneration delegate"));
}

void UAenoixEditorDebugSubsystem::OnBoundingVolumeRegenerated(AAeonixBoundingVolume* Volume)
{
	if (!Volume || !StartDebugActor.IsValid() || !EndDebugActor.IsValid())
	{
		return;
	}

	// Check if either debug actor is within the regenerated volume
	if (Volume->IsPointInside(StartDebugActor->GetActorLocation()) ||
		Volume->IsPointInside(EndDebugActor->GetActorLocation()))
	{
		// Trigger path recalculation
		UE_LOG(LogAeonixEditor, Log, TEXT("Navigation regenerated in volume containing debug path actors - recalculating path"));
		UpdateDebugActor(StartDebugActor.Get());
	}
}
