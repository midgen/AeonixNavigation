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
		return;
	}

	if (DebugActor->DebugType == EAeonixPathDebugActorType::START)
	{
		StartDebugActor = DebugActor;
	}
	else
	{
		EndDebugActor = DebugActor;
	}

	// Flag that we need to redraw due to actor change
	bNeedsRedraw = true;
	
	 UAeonixSubsystem* AeonixSubsystem = DebugActor->GetWorld()->GetSubsystem<UAeonixSubsystem>();
	
	// If we've got a valid start and end target
	if (StartDebugActor && EndDebugActor && !bIsPathPending)
	{
		if (AAeonixBoundingVolume* Volume = AeonixSubsystem->GetMutableVolumeForAgent(StartDebugActor->NavAgentComponent))
		{
			Volume->UpdateBounds();
			FAeonixPathFindRequestCompleteDelegate& PathRequestCompleteDelegate = AeonixSubsystem->FindPathAsyncAgent(StartDebugActor->NavAgentComponent, EndDebugActor->GetActorLocation(), CurrentDebugPath);
			PathRequestCompleteDelegate.BindDynamic(this, &UAenoixEditorDebugSubsystem::OnPathFindComplete);
			bIsPathPending = true;
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
	}
	else if (Status == EAeonixPathFindStatus::Failed)
	{
		CurrentDebugPath.SetIsReady(false);
		bIsPathPending = false;
		// Keep showing the cached path even if new calculation failed
	}
	else
	{
		UE_LOG(AeonixEditor, Error, TEXT("Unhandled state"));
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

	// Draw batch run paths using lightweight visualization
	if (BatchRunPaths.Num() > 0 && StartDebugActor.IsValid())
	{
		UWorld* World = StartDebugActor->GetWorld();
		if (World)
		{
			for (const FAeonixNavigationPath& Path : BatchRunPaths)
			{
				Path.DebugDrawLite(World, FColor::Cyan, 60.0f);
			}
		}
	}

	// Draw failed batch run paths as red lines
	if (FailedBatchRunPaths.Num() > 0 && StartDebugActor.IsValid())
	{
		UWorld* World = StartDebugActor->GetWorld();
		if (World)
		{
			for (const FAeonixFailedPath& FailedPath : FailedBatchRunPaths)
			{
				// Draw thick red line from start to end for failed paths
				DrawDebugLine(World, FailedPath.StartPoint, FailedPath.EndPoint, FColor::Red, false, 60.0f, 0, 8.0f);

				// Optional: Draw spheres at endpoints for better visibility
				DrawDebugSphere(World, FailedPath.StartPoint, 30.0f, 8, FColor::Yellow, false, 60.0f, 0, 3.0f);
				DrawDebugSphere(World, FailedPath.EndPoint, 30.0f, 8, FColor::Red, false, 60.0f, 0, 3.0f);
			}
		}
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
	UE_LOG(LogTemp, Log, TEXT("Debug subsystem received %d batch run paths for visualization"), Paths.Num());
}

void UAenoixEditorDebugSubsystem::ClearBatchRunPaths()
{
	BatchRunPaths.Empty();
	FailedBatchRunPaths.Empty(); // Also clear failed paths
	UE_LOG(LogTemp, VeryVerbose, TEXT("Debug subsystem cleared batch run paths"));
}

void UAenoixEditorDebugSubsystem::SetFailedBatchRunPaths(const TArray<TPair<FVector, FVector>>& FailedPaths)
{
	FailedBatchRunPaths.Empty();
	for (const auto& Path : FailedPaths)
	{
		FailedBatchRunPaths.Add(FAeonixFailedPath(Path.Key, Path.Value));
	}
	UE_LOG(LogTemp, Log, TEXT("Debug subsystem received %d failed batch run paths for visualization"), FailedPaths.Num());
}

void UAenoixEditorDebugSubsystem::ClearFailedBatchRunPaths()
{
	FailedBatchRunPaths.Empty();
	UE_LOG(LogTemp, VeryVerbose, TEXT("Debug subsystem cleared failed batch run paths"));
}
