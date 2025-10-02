// Copyright 2024 Chris Ashworth



#include <AeonixEditor/Private/AenoixEditorDebugSubsystem.h>
#include <AeonixEditor/Private/AeonixPathDebugActor.h>

#include <AeonixNavigation/Public/Actor/AeonixBoundingVolume.h>
#include <AeonixNavigation/Public/Component/AeonixNavAgentComponent.h>
#include <AeonixEditor/AeonixEditor.h>

#include "Subsystem/AeonixSubsystem.h"

#include "EngineUtils.h"
#include "DrawDebugHelpers.h"

void UAenoixEditorDebugSubsystem::UpdateDebugActor(AAeonixPathDebugActor* DebugActor)
{
	if (!DebugActor)
	{
		UE_LOG(LogTemp, Warning, TEXT("UpdateDebugActor called with null DebugActor"));
		return;
	}

	if (DebugActor->DebugType == EAeonixPathDebugActorType::START)
	{
		StartDebugActor = DebugActor;
		UE_LOG(LogTemp, Log, TEXT("Updated START debug actor at %s"), *DebugActor->GetActorLocation().ToString());
	}
	else
	{
		EndDebugActor = DebugActor;
		UE_LOG(LogTemp, Log, TEXT("Updated END debug actor at %s"), *DebugActor->GetActorLocation().ToString());
	}

	// Flag that we need to redraw due to actor change
	bNeedsRedraw = true;

	 UAeonixSubsystem* AeonixSubsystem = DebugActor->GetWorld()->GetSubsystem<UAeonixSubsystem>();

	// If we've got a valid start and end target
	if (StartDebugActor && EndDebugActor && !bIsPathPending)
	{
		UE_LOG(LogTemp, Log, TEXT("Both START and END actors set, attempting pathfinding..."));
		if (AAeonixBoundingVolume* Volume = AeonixSubsystem->GetMutableVolumeForAgent(StartDebugActor->NavAgentComponent))
		{
			// Don't attempt pathfinding if the volume isn't ready (prevents hangs during asset loading)
			if (!Volume->bIsReadyForNavigation)
			{
				UE_LOG(LogTemp, Warning, TEXT("Volume not ready for navigation - skipping pathfinding"));
				return;
			}
			Volume->UpdateBounds();
			UE_LOG(LogTemp, Log, TEXT("Requesting pathfind from %s to %s"), *StartDebugActor->GetActorLocation().ToString(), *EndDebugActor->GetActorLocation().ToString());
			FAeonixPathFindRequestCompleteDelegate& PathRequestCompleteDelegate = AeonixSubsystem->FindPathAsyncAgent(StartDebugActor->NavAgentComponent, EndDebugActor->GetActorLocation(), CurrentDebugPath);
			PathRequestCompleteDelegate.BindDynamic(this, &UAenoixEditorDebugSubsystem::OnPathFindComplete);
			bIsPathPending = true;
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Failed to get volume for agent"));
		}

	}
	else
	{
		if (!StartDebugActor)
		{
			UE_LOG(LogTemp, Log, TEXT("No START actor set yet"));
		}
		if (!EndDebugActor)
		{
			UE_LOG(LogTemp, Log, TEXT("No END actor set yet"));
		}
		if (bIsPathPending)
		{
			UE_LOG(LogTemp, Log, TEXT("Path already pending"));
		}
	}
}

void UAenoixEditorDebugSubsystem::OnPathFindComplete(EAeonixPathFindStatus Status)
{
	if (Status == EAeonixPathFindStatus::Complete)
	{
		CurrentDebugPath.SetIsReady(true);
		// Cache the successfully calculated path
		CachedDebugPath = CurrentDebugPath;
		bHasValidCachedPath = true;
		bIsPathPending = false;
		// Flag that we need to redraw the new path
		bNeedsRedraw = true;
		UE_LOG(LogTemp, Log, TEXT("Pathfinding COMPLETE - path ready to draw with %d waypoints"), CurrentDebugPath.GetPathPoints().Num());
	}
	else if (Status == EAeonixPathFindStatus::Failed)
	{
		CurrentDebugPath.SetIsReady(false);
		bIsPathPending = false;
		// Keep showing the cached path even if new calculation failed
		UE_LOG(LogTemp, Warning, TEXT("Pathfinding FAILED"));
	}
	else
	{
		UE_LOG(AeonixEditor, Error, TEXT("Unhandled pathfinding state"));
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
		// Clear previous path debug lines before drawing new ones
		// Note: This will clear all persistent debug lines but octree debug is redrawn during generation
		FlushPersistentDebugLines(StartDebugActor->GetWorld());

		// Draw the current path if ready, otherwise draw the cached path
		if (CurrentDebugPath.IsReady())
		{
			CurrentDebugPath.DebugDraw(StartDebugActor->GetWorld(), AeonixSubsystem->GetVolumeForAgent(StartDebugActor->NavAgentComponent)->GetNavData());
		}
		else if (bHasValidCachedPath)
		{
			// Show cached path while recalculating
			CachedDebugPath.DebugDraw(StartDebugActor->GetWorld(), AeonixSubsystem->GetVolumeForAgent(StartDebugActor->NavAgentComponent)->GetNavData());
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
			for (const FAeonixFailedPath& FailedPath : FailedBatchRunPaths)
			{
				// Draw thick red line from start to end for failed paths (persistent)
				DrawDebugLine(World, FailedPath.StartPoint, FailedPath.EndPoint, FColor::Red, true, -1.0f, 0, 8.0f);

				// Optional: Draw spheres at endpoints for better visibility (persistent)
				DrawDebugSphere(World, FailedPath.StartPoint, 30.0f, 8, FColor::Yellow, true, -1.0f, 0, 3.0f);
				DrawDebugSphere(World, FailedPath.EndPoint, 30.0f, 8, FColor::Red, true, -1.0f, 0, 3.0f);
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
}

void UAenoixEditorDebugSubsystem::ClearCachedPath()
{
	bHasValidCachedPath = false;
	CachedDebugPath.ResetForRepath();
}


void UAenoixEditorDebugSubsystem::SetBatchRunPaths(const TArray<FAeonixNavigationPath>& Paths)
{
	BatchRunPaths = Paths;
	bBatchPathsNeedRedraw = true;
	UE_LOG(LogTemp, Log, TEXT("Debug subsystem received %d batch run paths for visualization"), Paths.Num());
}

void UAenoixEditorDebugSubsystem::ClearBatchRunPaths()
{
	BatchRunPaths.Empty();
	FailedBatchRunPaths.Empty(); // Also clear failed paths
	bBatchPathsNeedRedraw = false;
	bFailedPathsNeedRedraw = false;
	UE_LOG(LogTemp, VeryVerbose, TEXT("Debug subsystem cleared batch run paths"));
}

void UAenoixEditorDebugSubsystem::SetFailedBatchRunPaths(const TArray<TPair<FVector, FVector>>& FailedPaths)
{
	FailedBatchRunPaths.Empty();
	for (const auto& Path : FailedPaths)
	{
		FailedBatchRunPaths.Add(FAeonixFailedPath(Path.Key, Path.Value));
	}
	bFailedPathsNeedRedraw = true;
	UE_LOG(LogTemp, Log, TEXT("Debug subsystem received %d failed batch run paths for visualization"), FailedPaths.Num());
}

void UAenoixEditorDebugSubsystem::ClearFailedBatchRunPaths()
{
	FailedBatchRunPaths.Empty();
	bFailedPathsNeedRedraw = false;
	UE_LOG(LogTemp, VeryVerbose, TEXT("Debug subsystem cleared failed batch run paths"));
}
