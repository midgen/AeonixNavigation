



#include "Subsystem/AeonixSubsystem.h"

#include "AeonixNavigation.h"
#include "Actor/AeonixBoundingVolume.h"
#include "Actor/AeonixModifierVolume.h"
#include "Component/AeonixNavAgentComponent.h"
#include "Component/AeonixDynamicObstacleComponent.h"
#include "Task/AeonixFindPathTask.h"
#include "Util/AeonixMediator.h"
#include "Mass/AeonixFragments.h"
#include "Data/AeonixHandleTypes.h"
#include "Data/AeonixStats.h"
#include "Data/AeonixGenerationParameters.h"
#include "Data/AeonixThreading.h"
#include "Settings/AeonixSettings.h"

#include "MassCommonFragments.h"
#include "MassEntityManager.h"
#include "MassEntitySubsystem.h"
#include "HAL/PlatformProcess.h"
#include "DrawDebugHelpers.h"

void UAeonixSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);

	// Get settings
	const UAeonixSettings* Settings = GetDefault<UAeonixSettings>();
	const int32 NumWorkerThreads = Settings ? Settings->PathfindingWorkerThreads : 2;

	// Initialize worker pool
	WorkerPool.Initialize(NumWorkerThreads);

	// Update max concurrent pathfinds from settings
	if (Settings)
	{
		MaxConcurrentPathfinds = Settings->MaxConcurrentPathfinds;
	}

	UE_LOG(LogAeonixNavigation, Log, TEXT("AeonixSubsystem initialized: %d worker threads, max %d concurrent pathfinds"),
		NumWorkerThreads, MaxConcurrentPathfinds);
}

void UAeonixSubsystem::RegisterVolume(AAeonixBoundingVolume* Volume, EAeonixMassEntityFlag CreateMassEntity)
{
	for (FAeonixBoundingVolumeHandle& Handle : RegisteredVolumes)
	{
		if (Handle.VolumeHandle == Volume)
		{
			return;
		}
	}

	// TODO: Create Mass Entity

	RegisteredVolumes.Emplace(Volume);

	// Subscribe to the volume's regeneration events for subsystem orchestration
	Volume->OnNavigationRegenerated.AddUObject(this, &UAeonixSubsystem::OnBoundingVolumeRegenerated);
	UE_LOG(LogAeonixNavigation, Verbose, TEXT("Subsystem subscribed to volume %s regeneration events"), *Volume->GetName());

	// Notify listeners that registration changed
	OnRegistrationChanged.Broadcast();
}

void UAeonixSubsystem::UnRegisterVolume(AAeonixBoundingVolume* Volume, EAeonixMassEntityFlag DestroyMassEntity)
{
	for (FAeonixBoundingVolumeHandle& Handle : RegisteredVolumes)
	{
		if (Handle.VolumeHandle == Volume)
		{
			if (DestroyMassEntity == EAeonixMassEntityFlag::Enabled)
			{
				UMassEntitySubsystem* MassEntitySubsystem = GetWorld()->GetSubsystem<UMassEntitySubsystem>();
				if (MassEntitySubsystem)
				{
					FMassEntityManager& EntityManager = MassEntitySubsystem->GetMutableEntityManager();
					EntityManager.DestroyEntity(Handle.EntityHandle);
				}
				else
				{
					UE_LOG(LogAeonixNavigation, Warning, TEXT("MassEntitySubsystem not available, skipping entity destruction"));
				}
			}

			// Unsubscribe from the volume's regeneration events
			Volume->OnNavigationRegenerated.RemoveAll(this);
			UE_LOG(LogAeonixNavigation, Verbose, TEXT("Subsystem unsubscribed from volume %s regeneration events"), *Volume->GetName());

			RegisteredVolumes.RemoveSingle(Handle);

			// Notify listeners that registration changed
			OnRegistrationChanged.Broadcast();
			return;
		}
	}

	UE_LOG(LogAeonixNavigation, Error, TEXT("Tried to remove a volume that isn't registered"))
}

void UAeonixSubsystem::RegisterModifierVolume(AAeonixModifierVolume* ModifierVolume)
{
	if (!ModifierVolume)
	{
		UE_LOG(LogAeonixNavigation, Warning, TEXT("Tried to register null modifier volume"));
		return;
	}

	if (RegisteredModifierVolumes.Contains(ModifierVolume))
	{
		UE_LOG(LogAeonixNavigation, Verbose, TEXT("Modifier volume %s already registered"), *ModifierVolume->GetName());
		return;
	}

	RegisteredModifierVolumes.Add(ModifierVolume);
	UE_LOG(LogAeonixNavigation, Verbose, TEXT("Registered modifier volume: %s"), *ModifierVolume->GetName());

	// Notify listeners that registration changed
	OnRegistrationChanged.Broadcast();
}

void UAeonixSubsystem::UnRegisterModifierVolume(AAeonixModifierVolume* ModifierVolume)
{
	if (!ModifierVolume)
	{
		UE_LOG(LogAeonixNavigation, Warning, TEXT("Tried to unregister null modifier volume"));
		return;
	}

	// If this modifier was registered with a bounding volume, unregister it
	if (AAeonixBoundingVolume** FoundVolume = ModifierToVolumeMap.Find(ModifierVolume))
	{
		AAeonixBoundingVolume* BoundingVolume = *FoundVolume;
		if (BoundingVolume)
		{
			// Remove dynamic region if it was a dynamic region modifier
			if (ModifierVolume->ModifierTypes & static_cast<int32>(EAeonixModifierType::DynamicRegion))
			{
				BoundingVolume->RemoveDynamicRegion(ModifierVolume->DynamicRegionId);
			}

			// Clear debug filter if it was a debug filter modifier
			if (ModifierVolume->ModifierTypes & static_cast<int32>(EAeonixModifierType::DebugFilter))
			{
				BoundingVolume->ClearDebugFilterBox();
			}
		}
		ModifierToVolumeMap.Remove(ModifierVolume);
	}

	const int32 NumRemoved = RegisteredModifierVolumes.Remove(ModifierVolume);
	if (NumRemoved == 0)
	{
		UE_LOG(LogAeonixNavigation, Warning, TEXT("Tried to unregister modifier volume %s that wasn't registered"), *ModifierVolume->GetName());
	}
	else
	{
		UE_LOG(LogAeonixNavigation, Verbose, TEXT("Unregistered modifier volume: %s"), *ModifierVolume->GetName());

		// Notify listeners that registration changed
		OnRegistrationChanged.Broadcast();
	}
}

void UAeonixSubsystem::RegisterNavComponent(UAeonixNavAgentComponent* NavComponent, EAeonixMassEntityFlag CreateMassEntity)
{
	if (RegisteredNavAgents.Contains(NavComponent))
	{
		return;
	}

	FMassEntityHandle Entity;

	if (CreateMassEntity == EAeonixMassEntityFlag::Enabled)
	{
		UMassEntitySubsystem* MassEntitySubsystem = GetWorld()->GetSubsystem<UMassEntitySubsystem>();
		if (MassEntitySubsystem)
		{
			FMassEntityManager& EntityManager = MassEntitySubsystem->GetMutableEntityManager();

			FMassArchetypeCompositionDescriptor Composition;
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 7
			Composition.GetFragments().Add<FTransformFragment>();
			Composition.GetFragments().Add<FAeonixNavAgentFragment>();
#else
			// UE 5.5/5.6: Access Fragments member directly
			Composition.Fragments.Add<FTransformFragment>();
			Composition.Fragments.Add<FAeonixNavAgentFragment>();
#endif

			FMassArchetypeHandle Archetype = EntityManager.CreateArchetype(Composition);
			Entity = EntityManager.CreateEntity(Archetype);
		}
		else
		{
			UE_LOG(LogAeonixNavigation, Warning, TEXT("MassEntitySubsystem not available, skipping mass entity creation"));
		}
	}

	RegisteredNavAgents.Emplace(NavComponent, Entity);
}

void UAeonixSubsystem::UnRegisterNavComponent(UAeonixNavAgentComponent* NavComponent, EAeonixMassEntityFlag DestroyMassEntity)
{
	// CRITICAL: Immediately mark all pending requests for this component as invalidated
	// This prevents worker threads from writing to the component's path after destruction
	// Must happen BEFORE component is removed from registration
	{
		FScopeLock Lock(&PathRequestsLock);
		for (TUniquePtr<FAeonixPathFindRequest>& Request : PathRequests)
		{
			if (Request->RequestingAgent.Get() == NavComponent)
			{
				Request->bAgentInvalidated.store(true, std::memory_order_release);
			}
		}
	}

	for (int i = 0; i < RegisteredNavAgents.Num(); i++)
	{
		FAeonixNavAgentHandle& Agent = RegisteredNavAgents[i];
		if (Agent.NavAgentComponent == NavComponent)
		{
			RegisteredNavAgents.RemoveAtSwap(i, EAllowShrinking::No);
			if (DestroyMassEntity == EAeonixMassEntityFlag::Enabled)
			{
				UMassEntitySubsystem* MassEntitySubsystem = GetWorld()->GetSubsystem<UMassEntitySubsystem>();
				if (MassEntitySubsystem)
				{
					FMassEntityManager& EntityManager = MassEntitySubsystem->GetMutableEntityManager();
					EntityManager.DestroyEntity(Agent.EntityHandle);
				}
				else
				{
					UE_LOG(LogAeonixNavigation, Warning, TEXT("MassEntitySubsystem not available, skipping entity destruction"));
				}
			}
			break;
		}
	}
}

void UAeonixSubsystem::RegisterDynamicObstacle(UAeonixDynamicObstacleComponent* ObstacleComponent)
{
	if (!ObstacleComponent)
	{
		UE_LOG(LogAeonixNavigation, Warning, TEXT("Tried to register null obstacle component"));
		return;
	}

	if (RegisteredDynamicObstacles.Contains(ObstacleComponent))
	{
		UE_LOG(LogAeonixNavigation, Verbose, TEXT("Obstacle %s already registered"), *ObstacleComponent->GetName());
		return;
	}

	RegisteredDynamicObstacles.Add(ObstacleComponent);

	// Initialize transform tracking and immediately determine bounding volume/regions
	if (AActor* Owner = ObstacleComponent->GetOwner())
	{
		// Only set last transform if not already tracked (prevents reset on re-registration during editor moves)
		// Key by Actor because components can be recreated during editor moves
		if (!ObstacleLastTransformMap.Contains(Owner))
		{
			ObstacleLastTransformMap.Add(Owner, Owner->GetActorTransform());
		}

		// Immediately determine which bounding volume and regions the obstacle is in
		// This prevents timing issues where TriggerNavigationRegen() is called before first tick
		const FVector CurrentPosition = Owner->GetActorLocation();
		AAeonixBoundingVolume* FoundVolume = nullptr;
		TSet<FGuid> FoundRegionIds;

		for (FAeonixBoundingVolumeHandle& Handle : RegisteredVolumes)
		{
			if (Handle.VolumeHandle && Handle.VolumeHandle->IsPointInside(CurrentPosition))
			{
				FoundVolume = Handle.VolumeHandle;

				// Check which dynamic regions within this volume we're inside
				const FAeonixGenerationParameters& Params = Handle.VolumeHandle->GenerationParameters;
				for (const auto& RegionPair : Params.DynamicRegionBoxes)
				{
					if (RegionPair.Value.IsInsideOrOn(CurrentPosition))
					{
						FoundRegionIds.Add(RegionPair.Key);
					}
				}
				break;
			}
		}

		// Set the obstacle's current state immediately
		ObstacleComponent->SetCurrentBoundingVolume(FoundVolume);
		ObstacleComponent->SetCurrentRegionIds(FoundRegionIds);

		UE_LOG(LogAeonixNavigation, Verbose, TEXT("Registered dynamic obstacle: %s (Volume: %s, Regions: %d)"),
			*ObstacleComponent->GetName(),
			FoundVolume ? *FoundVolume->GetName() : TEXT("None"),
			FoundRegionIds.Num());
	}
	else
	{
		UE_LOG(LogAeonixNavigation, Verbose, TEXT("Registered dynamic obstacle: %s (no owner)"), *ObstacleComponent->GetName());
	}

	// Notify listeners that registration changed
	OnRegistrationChanged.Broadcast();
}

void UAeonixSubsystem::UnRegisterDynamicObstacle(UAeonixDynamicObstacleComponent* ObstacleComponent)
{
	if (!ObstacleComponent)
	{
		UE_LOG(LogAeonixNavigation, Warning, TEXT("Tried to unregister null obstacle component"));
		return;
	}

	const int32 NumRemoved = RegisteredDynamicObstacles.Remove(ObstacleComponent);
	// Note: Don't remove from ObstacleLastTransformMap here - preserve transform history
	// during editor move cycles (unregister/re-register). The map entry will be cleaned up
	// automatically in ProcessDynamicObstacles() when the obstacle becomes invalid.

	if (NumRemoved == 0)
	{
		UE_LOG(LogAeonixNavigation, Warning, TEXT("Tried to unregister obstacle %s that wasn't registered"), *ObstacleComponent->GetName());
	}
	else
	{
		UE_LOG(LogAeonixNavigation, Verbose, TEXT("Unregistered dynamic obstacle: %s"), *ObstacleComponent->GetName());

		// Notify listeners that registration changed
		OnRegistrationChanged.Broadcast();
	}
}

const AAeonixBoundingVolume* UAeonixSubsystem::GetVolumeForPosition(const FVector& Position)
{
	for (FAeonixBoundingVolumeHandle& Handle : RegisteredVolumes)
	{
		if (Handle.VolumeHandle->IsPointInside(Position))
		{
			return Handle.VolumeHandle;
		}
	}

	return nullptr;
}

bool UAeonixSubsystem::FindPathImmediateAgent(UAeonixNavAgentComponent* NavigationComponent, const FVector& End, FAeonixNavigationPath& OutPath)
{
	const AAeonixBoundingVolume* NavVolume = GetVolumeForAgent(NavigationComponent);

	if (!NavVolume)
	{
		return false;
	}

	AeonixLink StartNavLink;
	AeonixLink TargetNavLink;

	// Get the nav link from our volume
	if (!AeonixMediator::GetLinkFromPosition(NavigationComponent->GetPathfindingStartPosition(), *NavVolume, StartNavLink))
	{
		UE_LOG(LogAeonixNavigation, Error, TEXT("Path finder failed to find start nav link"));
		return false;
	}

	if (!AeonixMediator::GetLinkFromPosition(NavigationComponent->GetPathfindingEndPosition(End), *NavVolume, TargetNavLink))
	{
		UE_LOG(LogAeonixNavigation, Error, TEXT("Path finder failed to find target nav link"));
		return false;
	}

	OutPath.ResetForRepath();

	// Acquire read lock for thread-safe octree access during pathfinding
	bool Result;
	FAeonixPathFailureInfo FailureInfo;
	{
		FReadScopeLock ReadLock(NavVolume->GetOctreeDataLock());
		SCOPE_CYCLE_COUNTER(STAT_AeonixPathfindingSync);

		AeonixPathFinder pathFinder(NavVolume->GetNavData(), NavigationComponent->PathfinderSettings);
		Result = pathFinder.FindPath(StartNavLink, TargetNavLink, NavigationComponent->GetPathfindingStartPosition(), NavigationComponent->GetPathfindingEndPosition(End), OutPath, &FailureInfo);
	}

	// CRITICAL FIX: Track regions BEFORE marking path ready
	// This ensures path is registered before it can be invalidated
	if (Result)
	{
		TrackPathRegions(OutPath, NavVolume);
	}
	else if (FailureInfo.bFailedDueToMaxIterations)
	{
		// Draw debug visualization for max iteration failure
		if (UWorld* World = GetWorld())
		{
			// Red line from start to target
			DrawDebugLine(World, FailureInfo.StartPosition, FailureInfo.TargetPosition, FColor::Red, false, 10.0f, 0, 5.0f);

			// Red sphere at start position
			DrawDebugSphere(World, FailureInfo.StartPosition, 50.0f, 12, FColor::Red, false, 10.0f, 0, 3.0f);

			// Magenta sphere at target position
			DrawDebugSphere(World, FailureInfo.TargetPosition, 50.0f, 12, FColor::Magenta, false, 10.0f, 0, 3.0f);

			UE_LOG(LogAeonixNavigation, Warning, TEXT("Pathfinding visualization: Max iterations (%d) reached. Distance: %.2f units. Check viewport for red line and spheres (10 sec duration)."),
				FailureInfo.IterationCount, FailureInfo.StraightLineDistance);
		}
	}

	OutPath.SetIsReady(true);
	UE_LOG(LogAeonixNavigation, Log, TEXT("AeonixSubsystem: Path found with %d points, marked as ready"), OutPath.GetPathPoints().Num());

	return Result;
}

FAeonixPathFindRequestCompleteDelegate& UAeonixSubsystem::FindPathAsyncAgent(UAeonixNavAgentComponent* NavigationComponent, const FVector& End, FAeonixNavigationPath& OutPath)
{
	AeonixLink StartNavLink;
	AeonixLink TargetNavLink;

	// Create request with enhanced metadata
	TUniquePtr<FAeonixPathFindRequest> Request = MakeUnique<FAeonixPathFindRequest>();
	FAeonixPathFindRequest* RequestPtr = Request.Get();

	// Set request metadata
	RequestPtr->SubmitTime = FPlatformTime::Seconds();
	RequestPtr->RequestingAgent = NavigationComponent;
	RequestPtr->Priority = EAeonixRequestPriority::Normal; // Can be customized per agent

	const AAeonixBoundingVolume* NavVolume = GetVolumeForAgent(NavigationComponent);

	if (!NavVolume)
	{
		UE_LOG(LogAeonixNavigation, Error, TEXT("Nav Agent Not In A Volume"));
		RequestPtr->PathFindPromise.SetValue(EAeonixPathFindStatus::Failed);
		LoadMetrics.FailedPathfindsTotal.fetch_add(1);

		// Add to requests array so we can return the delegate
		FScopeLock Lock(&PathRequestsLock);
		PathRequests.Add(MoveTemp(Request));
		return RequestPtr->OnPathFindRequestComplete;
	}

	// Get the nav link from our volume
	if (!AeonixMediator::GetLinkFromPosition(NavigationComponent->GetPathfindingStartPosition(), *NavVolume, StartNavLink))
	{
		UE_LOG(LogAeonixNavigation, Error, TEXT("Path finder failed to find start nav link"));
		RequestPtr->PathFindPromise.SetValue(EAeonixPathFindStatus::Failed);
		LoadMetrics.FailedPathfindsTotal.fetch_add(1);

		FScopeLock Lock(&PathRequestsLock);
		PathRequests.Add(MoveTemp(Request));
		return RequestPtr->OnPathFindRequestComplete;
	}

	if (!AeonixMediator::GetLinkFromPosition(NavigationComponent->GetPathfindingEndPosition(End), *NavVolume, TargetNavLink))
	{
		UE_LOG(LogAeonixNavigation, Error, TEXT("Path finder failed to find target nav link"));
		RequestPtr->PathFindPromise.SetValue(EAeonixPathFindStatus::Failed);
		LoadMetrics.FailedPathfindsTotal.fetch_add(1);

		FScopeLock Lock(&PathRequestsLock);
		PathRequests.Add(MoveTemp(Request));
		return RequestPtr->OnPathFindRequestComplete;
	}

	if (TargetNavLink == StartNavLink)
	{
		// Same voxel - create direct path with start and end points
		OutPath.ResetForRepath();

		FVector StartPosition = NavigationComponent->GetPathfindingStartPosition();
		FVector EndPosition = NavigationComponent->GetPathfindingEndPosition(End);

		OutPath.AddPoint(FAeonixPathPoint(StartPosition, StartNavLink.GetLayerIndex()));
		OutPath.AddPoint(FAeonixPathPoint(EndPosition, StartNavLink.GetLayerIndex()));

		OutPath.SetIsReady(true);
		UE_LOG(LogAeonixNavigation, Log, TEXT("AeonixSubsystem: Same voxel path - direct path with 2 points"));
		RequestPtr->PathFindPromise.SetValue(EAeonixPathFindStatus::Complete);
		LoadMetrics.CompletedPathfindsTotal.fetch_add(1);

		FScopeLock Lock(&PathRequestsLock);
		PathRequests.Add(MoveTemp(Request));
		return RequestPtr->OnPathFindRequestComplete;
	}

	// Make sure the path isn't flagged ready
	OutPath.ResetForRepath();
	OutPath.SetIsReady(false);

	// Store destination pointer for game-thread delivery (NEVER accessed from worker)
	RequestPtr->DestinationPath = &OutPath;

	// Copy necessary data to avoid dangling references in async task
	TWeakObjectPtr<const AAeonixBoundingVolume> WeakNavVolume = NavVolume;
	TWeakObjectPtr<UAeonixSubsystem> WeakSubsystem = this;
	FAeonixPathFinderSettings PathfinderSettingsCopy = NavigationComponent->PathfinderSettings;
	FVector StartPosition = NavigationComponent->GetPathfindingStartPosition();
	FVector EndPosition = NavigationComponent->GetPathfindingEndPosition(End);

	// Capture region versions BEFORE pathfinding starts
	// We'll validate these at the end to detect if regions were regenerated mid-calculation
	TMap<FGuid, uint32> CapturedRegionVersions;
	const FAeonixGenerationParameters& Params = NavVolume->GenerationParameters;
	for (const auto& RegionPair : Params.DynamicRegionBoxes)
	{
		CapturedRegionVersions.Add(RegionPair.Key, GetRegionVersion(RegionPair.Key));
	}

	// Update metrics
	LoadMetrics.PendingPathfinds.fetch_add(1);

	// Enqueue work to worker pool
	// CRITICAL: No &OutPath capture - workers write to RequestPtr->WorkerPath instead
	// Game thread will move results to DestinationPath in UpdateRequests()
	WorkerPool.EnqueueWork([RequestPtr, WeakNavVolume, WeakSubsystem, PathfinderSettingsCopy, StartNavLink, TargetNavLink, StartPosition, EndPosition, CapturedRegionVersions]()
	{
		SCOPE_CYCLE_COUNTER(STAT_AeonixPathfindingAsync);

		const double StartTime = FPlatformTime::Seconds();

		// Check if request was cancelled (thread-safe atomic check only)
		if (RequestPtr->IsStale())
		{
			RequestPtr->PathFindPromise.SetValue(EAeonixPathFindStatus::Cancelled);

			if (UAeonixSubsystem* Subsystem = WeakSubsystem.Get())
			{
				Subsystem->LoadMetrics.PendingPathfinds.fetch_sub(1);
				Subsystem->LoadMetrics.CancelledPathfindsTotal.fetch_add(1);
			}
			return;
		}

		// Check if volume is still valid
		const AAeonixBoundingVolume* NavVolume = WeakNavVolume.Get();
		if (!NavVolume)
		{
			UE_LOG(LogAeonixNavigation, Warning, TEXT("AeonixSubsystem: Nav volume destroyed during async pathfinding"));
			RequestPtr->PathFindPromise.SetValue(EAeonixPathFindStatus::Failed);

			if (UAeonixSubsystem* Subsystem = WeakSubsystem.Get())
			{
				Subsystem->LoadMetrics.PendingPathfinds.fetch_sub(1);
				Subsystem->LoadMetrics.FailedPathfindsTotal.fetch_add(1);
			}
			return;
		}

		UAeonixSubsystem* Subsystem = WeakSubsystem.Get();
		if (Subsystem)
		{
			Subsystem->LoadMetrics.PendingPathfinds.fetch_sub(1);
			Subsystem->LoadMetrics.ActivePathfinds.fetch_add(1);
		}

		// Acquire read lock for thread-safe octree access during pathfinding
		FReadScopeLock ReadLock(NavVolume->GetOctreeDataLock());

		AeonixPathFinder PathFinder(NavVolume->GetNavData(), PathfinderSettingsCopy);

		// Write to request-owned path (SAFE - survives component destruction)
		RequestPtr->WorkerPath.ResetForRepath();
		FAeonixPathFailureInfo FailureInfo;
		if (PathFinder.FindPath(StartNavLink, TargetNavLink, StartPosition, EndPosition, RequestPtr->WorkerPath, &FailureInfo))
		{
			// Validate that regions didn't change during pathfinding calculation
			// If any region was regenerated while we were calculating, mark path as invalidated
			bool bPathStale = false;
			if (Subsystem)
			{
				for (const auto& RegionVersionPair : CapturedRegionVersions)
				{
					const uint32 CurrentVersion = Subsystem->GetRegionVersion(RegionVersionPair.Key);
					if (CurrentVersion != RegionVersionPair.Value)
					{
						UE_LOG(LogAeonixNavigation, Warning,
							TEXT("AeonixSubsystem: Path calculated with stale data - region %s changed from version %d to %d during pathfinding"),
							*RegionVersionPair.Key.ToString(), RegionVersionPair.Value, CurrentVersion);
						bPathStale = true;
						break;
					}
				}
			}

			if (bPathStale)
			{
				// Path was calculated based on outdated navigation data
				RequestPtr->PathFindPromise.SetValue(EAeonixPathFindStatus::Invalidated);

				if (Subsystem)
				{
					Subsystem->LoadMetrics.CancelledPathfindsTotal.fetch_add(1);
				}

				UE_LOG(LogAeonixNavigation, Log, TEXT("AeonixSubsystem: Async path invalidated (region changed during calculation)"));
			}
			else
			{
				// Path is valid - signal ready for game thread delivery
				RequestPtr->bPathReady.store(true, std::memory_order_release);
				UE_LOG(LogAeonixNavigation, Log, TEXT("AeonixSubsystem: Async path found with %d points"), RequestPtr->WorkerPath.GetPathPoints().Num());

				RequestPtr->PathFindPromise.SetValue(EAeonixPathFindStatus::Complete);

				if (Subsystem)
				{
					Subsystem->LoadMetrics.CompletedPathfindsTotal.fetch_add(1);
					const float ElapsedMs = (FPlatformTime::Seconds() - StartTime) * 1000.0f;
					Subsystem->LoadMetrics.UpdatePathfindTime(ElapsedMs);
				}
			}
		}
		else
		{
			// Pathfinding failed
			RequestPtr->PathFindPromise.SetValue(EAeonixPathFindStatus::Failed);

			if (Subsystem)
			{
				Subsystem->LoadMetrics.FailedPathfindsTotal.fetch_add(1);
			}

			// Draw debug visualization if failed due to max iterations
			if (FailureInfo.bFailedDueToMaxIterations)
			{
				// Queue debug visualization to game thread
				AsyncTask(ENamedThreads::GameThread, [FailureInfo, WeakSubsystem]()
				{
					if (UAeonixSubsystem* Subsystem = WeakSubsystem.Get())
					{
						if (UWorld* World = Subsystem->GetWorld())
						{
							// Red line from start to target
							DrawDebugLine(World, FailureInfo.StartPosition, FailureInfo.TargetPosition, FColor::Red, false, 10.0f, 0, 5.0f);

							// Red sphere at start position
							DrawDebugSphere(World, FailureInfo.StartPosition, 50.0f, 12, FColor::Red, false, 10.0f, 0, 3.0f);

							// Magenta sphere at target position
							DrawDebugSphere(World, FailureInfo.TargetPosition, 50.0f, 12, FColor::Magenta, false, 10.0f, 0, 3.0f);

							UE_LOG(LogAeonixNavigation, Warning, TEXT("Async pathfinding visualization: Max iterations (%d) reached. Distance: %.2f units. Check viewport for red line and spheres (10 sec duration)."),
								FailureInfo.IterationCount, FailureInfo.StraightLineDistance);
						}
					}
				});
			}
		}

		if (Subsystem)
		{
			Subsystem->LoadMetrics.ActivePathfinds.fetch_sub(1);
		}
	});

	// Add to requests array
	FScopeLock Lock(&PathRequestsLock);
	PathRequests.Add(MoveTemp(Request));
	return RequestPtr->OnPathFindRequestComplete;
}

const AAeonixBoundingVolume* UAeonixSubsystem::GetVolumeForAgent(const UAeonixNavAgentComponent* NavigationComponent)
{
	if (!AgentToVolumeMap.Contains(NavigationComponent))
	{
		return nullptr;
	}
	
	return AgentToVolumeMap[NavigationComponent];
}

AAeonixBoundingVolume* UAeonixSubsystem::GetMutableVolumeForAgent(const UAeonixNavAgentComponent* NavigationComponent)
{
	if (!AgentToVolumeMap.Contains(NavigationComponent))
	{
		return nullptr;
	}

	// debug shenanigans, this is fine
	return const_cast<AAeonixBoundingVolume*>(AgentToVolumeMap[NavigationComponent]);
}

void UAeonixSubsystem::UpdateComponents()
{
	for (int32 i = RegisteredNavAgents.Num() -1; i >= 0; i--)
	{
		FAeonixNavAgentHandle& AgentHandle = RegisteredNavAgents[i];

		// Clean up null NavAgentComponent entries
		if (!AgentHandle.NavAgentComponent || !IsValid(AgentHandle.NavAgentComponent))
		{
			RegisteredNavAgents.RemoveAtSwap(i, EAllowShrinking::No);
			continue;
		}
		
		bool bIsInValidVolume = false;

		for (FAeonixBoundingVolumeHandle& Handle : RegisteredVolumes)
		{
			if (Handle.VolumeHandle->IsPointInside(AgentHandle.NavAgentComponent->GetAgentPosition()))
			{
				AgentToVolumeMap.Add(AgentHandle.NavAgentComponent, Handle.VolumeHandle);
				bIsInValidVolume = true;
				break;
			}
		}

		if (!bIsInValidVolume && AgentToVolumeMap.Contains(AgentHandle.NavAgentComponent))
		{
			AgentToVolumeMap[AgentHandle.NavAgentComponent] = nullptr;
		}
	}

}

void UAeonixSubsystem::ProcessDynamicObstacles(float DeltaTime)
{
	// Clean up stale entries in transform map (actors that are no longer valid)
	for (auto It = ObstacleLastTransformMap.CreateIterator(); It; ++It)
	{
		if (!It.Key() || !IsValid(It.Key()))
		{
			It.RemoveCurrent();
		}
	}

	// Clean up null/invalid obstacle components (iterate backwards for safe removal)
	for (int32 i = RegisteredDynamicObstacles.Num() - 1; i >= 0; i--)
	{
		UAeonixDynamicObstacleComponent* Obstacle = RegisteredDynamicObstacles[i];

		if (!Obstacle || !IsValid(Obstacle))
		{
			RegisteredDynamicObstacles.RemoveAtSwap(i, EAllowShrinking::No);
			continue;
		}

		// Skip inactive obstacles
		if (!Obstacle->bEnableNavigationRegen)
		{
			continue;
		}

		AActor* Owner = Obstacle->GetOwner();
		if (!Owner)
		{
			continue;
		}

		// Get current and last transform (keyed by Actor for stability during editor moves)
		const FTransform CurrentTransform = Owner->GetActorTransform();
		FTransform* LastTransform = ObstacleLastTransformMap.Find(Owner);
		if (!LastTransform)
		{
			// Initialize if missing
			ObstacleLastTransformMap.Add(Owner, CurrentTransform);
			LastTransform = ObstacleLastTransformMap.Find(Owner);
		}

		// Store old region IDs
		const TSet<FGuid> OldRegionIds = Obstacle->GetCurrentRegionIds();

		// Find which bounding volume and regions the obstacle is now in
		const FVector CurrentPosition = Owner->GetActorLocation();
		AAeonixBoundingVolume* NewBoundingVolume = nullptr;
		TSet<FGuid> NewRegionIds;

		for (FAeonixBoundingVolumeHandle& Handle : RegisteredVolumes)
		{
			if (Handle.VolumeHandle && Handle.VolumeHandle->IsPointInside(CurrentPosition))
			{
				NewBoundingVolume = Handle.VolumeHandle;

				// Check which dynamic regions within this volume we're inside
				const FAeonixGenerationParameters& Params = Handle.VolumeHandle->GenerationParameters;
				for (const auto& RegionPair : Params.DynamicRegionBoxes)
				{
					if (RegionPair.Value.IsInsideOrOn(CurrentPosition))
					{
						NewRegionIds.Add(RegionPair.Key);
					}
				}
				break;
			}
		}

		// Update obstacle's current state
		Obstacle->SetCurrentBoundingVolume(NewBoundingVolume);
		Obstacle->SetCurrentRegionIds(NewRegionIds);

		// Check if regions changed
		const bool bRegionsChanged = !(NewRegionIds.Num() == OldRegionIds.Num() && NewRegionIds.Includes(OldRegionIds));

		// Check position threshold
		const FVector CurrentPos = CurrentTransform.GetLocation();
		const FVector LastPos = LastTransform->GetLocation();
		const float DistanceSq = FVector::DistSquared(CurrentPos, LastPos);
		const float PositionThresholdSq = Obstacle->PositionThreshold * Obstacle->PositionThreshold;
		const bool bPositionChanged = DistanceSq > PositionThresholdSq;

		// Check rotation threshold
		const FQuat CurrentRotation = CurrentTransform.GetRotation();
		const FQuat LastRotation = LastTransform->GetRotation();
		const float DotProduct = FMath::Abs(CurrentRotation | LastRotation);
		const float AngleDegrees = FMath::RadiansToDegrees(2.0f * FMath::Acos(FMath::Min(DotProduct, 1.0f)));
		const bool bRotationChanged = AngleDegrees > Obstacle->RotationThreshold;

		// If any threshold exceeded or regions changed, request regeneration
		if (bRegionsChanged || bPositionChanged || bRotationChanged)
		{
			// Log movement detection
			UE_LOG(LogAeonixNavigation, Log,
				TEXT("Obstacle %s: Movement detected (pos=%d, rot=%d, regions=%d), OldRegions=%d, NewRegions=%d"),
				*Obstacle->GetName(), bPositionChanged, bRotationChanged, bRegionsChanged,
				OldRegionIds.Num(), NewRegionIds.Num());

			if (NewBoundingVolume)
			{
				// Request regeneration for all affected regions (union of old and new)
				TSet<FGuid> AllAffectedRegions = OldRegionIds.Union(NewRegionIds);

				for (const FGuid& RegionId : AllAffectedRegions)
				{
					NewBoundingVolume->RequestDynamicRegionRegen(RegionId);
				}

				if (AllAffectedRegions.Num() > 0)
				{
					UE_LOG(LogAeonixNavigation, Display,
						TEXT("Obstacle %s: Transform changed - requested regen for %d regions (old: %d, new: %d)"),
						*Obstacle->GetName(),
						AllAffectedRegions.Num(),
						OldRegionIds.Num(),
						NewRegionIds.Num());
				}
				else
				{
					UE_LOG(LogAeonixNavigation, Warning,
						TEXT("Obstacle %s: Movement detected but not inside any dynamic regions - no regen triggered. Ensure obstacle is inside a modifier volume with DynamicRegion flag."),
						*Obstacle->GetName());
				}
			}
			else
			{
				UE_LOG(LogAeonixNavigation, Warning,
					TEXT("Obstacle %s: Movement detected but not inside any bounding volume - no regen triggered"),
					*Obstacle->GetName());
			}

			// Update the tracked transform
			*LastTransform = CurrentTransform;
		}
	}

	// Try to process dirty regions on all volumes (throttled by cooldown)
	// and process any pending regeneration results with time budget
	for (FAeonixBoundingVolumeHandle& Handle : RegisteredVolumes)
	{
		if (Handle.VolumeHandle)
		{
			Handle.VolumeHandle->TryProcessDirtyRegions();
			Handle.VolumeHandle->ProcessPendingRegenResults(DeltaTime);
		}
	}
}

void UAeonixSubsystem::UpdateSpatialRelationships()
{
	// Clean up null/invalid modifier volumes (iterate backwards for safe removal)
	for (int32 i = RegisteredModifierVolumes.Num() - 1; i >= 0; i--)
	{
		AAeonixModifierVolume* ModifierVolume = RegisteredModifierVolumes[i];
		if (!ModifierVolume || !IsValid(ModifierVolume))
		{
			// Clean up the map entry too
			ModifierToVolumeMap.Remove(ModifierVolume);
			RegisteredModifierVolumes.RemoveAtSwap(i, EAllowShrinking::No);
			continue;
		}

		// Find which bounding volume this modifier is currently inside
		AAeonixBoundingVolume* CurrentBoundingVolume = nullptr;
		const FVector ModifierLocation = ModifierVolume->GetActorLocation();

		for (FAeonixBoundingVolumeHandle& Handle : RegisteredVolumes)
		{
			if (Handle.VolumeHandle)
			{
				const bool bIsInside = Handle.VolumeHandle->IsPointInside(ModifierLocation);
				if (bIsInside)
				{
					CurrentBoundingVolume = Handle.VolumeHandle;
					break;
				}
			}
		}

		// Get the previous bounding volume this modifier was in
		AAeonixBoundingVolume* PreviousBoundingVolume = nullptr;
		if (AAeonixBoundingVolume** FoundVolume = ModifierToVolumeMap.Find(ModifierVolume))
		{
			PreviousBoundingVolume = *FoundVolume;
		}

		// Check if the modifier has moved to a different bounding volume
		if (CurrentBoundingVolume != PreviousBoundingVolume)
		{
			// Unregister from previous bounding volume
			if (PreviousBoundingVolume)
			{
				if (ModifierVolume->ModifierTypes & static_cast<int32>(EAeonixModifierType::DynamicRegion))
				{
					PreviousBoundingVolume->RemoveDynamicRegion(ModifierVolume->DynamicRegionId);
				}
				if (ModifierVolume->ModifierTypes & static_cast<int32>(EAeonixModifierType::DebugFilter))
				{
					PreviousBoundingVolume->ClearDebugFilterBox();
				}
			}

			// Register with new bounding volume
			if (CurrentBoundingVolume)
			{
				const FBox ModifierBox = ModifierVolume->GetComponentsBoundingBox(true);

				if (ModifierVolume->ModifierTypes & static_cast<int32>(EAeonixModifierType::DynamicRegion))
				{
					CurrentBoundingVolume->AddDynamicRegion(ModifierVolume->DynamicRegionId, ModifierBox);
				}
				if (ModifierVolume->ModifierTypes & static_cast<int32>(EAeonixModifierType::DebugFilter))
				{
					CurrentBoundingVolume->SetDebugFilterBox(ModifierBox);
				}
			}

			// Update the map
			if (CurrentBoundingVolume)
			{
				ModifierToVolumeMap.Add(ModifierVolume, CurrentBoundingVolume);
			}
			else
			{
				ModifierToVolumeMap.Remove(ModifierVolume);
			}
		}
		else if (CurrentBoundingVolume)
		{
			// Even if in the same volume, update the bounds in case the modifier moved/resized within the volume
			const FBox ModifierBox = ModifierVolume->GetComponentsBoundingBox(true);

			if (ModifierVolume->ModifierTypes & static_cast<int32>(EAeonixModifierType::DynamicRegion))
			{
				CurrentBoundingVolume->AddDynamicRegion(ModifierVolume->DynamicRegionId, ModifierBox);
			}
			if (ModifierVolume->ModifierTypes & static_cast<int32>(EAeonixModifierType::DebugFilter))
			{
				CurrentBoundingVolume->SetDebugFilterBox(ModifierBox);
			}
		}
	}
}

void UAeonixSubsystem::Tick(float DeltaTime)
{
	UpdateSpatialRelationships();
	UpdateComponents();
	ProcessDynamicObstacles(DeltaTime);
	UpdateRequests();
}

void UAeonixSubsystem::UpdateRequests()
{
	FScopeLock Lock(&PathRequestsLock);

	for (int32 i = 0; i < PathRequests.Num();)
	{
		FAeonixPathFindRequest* Request = PathRequests[i].Get();

		// If our task has finished
		if (Request->PathFindFuture.IsReady())
		{
			EAeonixPathFindStatus Status = Request->PathFindFuture.Get();

			// GAME THREAD DELIVERY: Move results from WorkerPath to DestinationPath
			// This is safe because we're on game thread and can check UObject validity
			if (Status == EAeonixPathFindStatus::Complete &&
			    Request->bPathReady.load(std::memory_order_acquire) &&
			    Request->RequestingAgent.IsValid() &&  // Game thread - safe to call IsValid()
			    Request->DestinationPath)
			{
				// Move path data efficiently (TArray move = pointer swap)
				*Request->DestinationPath = MoveTemp(Request->WorkerPath);
				Request->DestinationPath->SetIsReady(true);

				// Track regions for invalidation (game thread only)
				if (const AAeonixBoundingVolume* NavVolume = GetVolumeForAgent(Request->RequestingAgent.Get()))
				{
					TrackPathRegions(*Request->DestinationPath, NavVolume);
				}

				UE_LOG(LogAeonixNavigation, Log, TEXT("AeonixSubsystem: Path delivered to component, marked as ready"));
			}
			// else: Component was destroyed or path not ready - just drop it

			Request->OnPathFindRequestComplete.ExecuteIfBound(Status);
			PathRequests.RemoveAtSwap(i);
			continue;
		}
		i++;
	}
}

TStatId UAeonixSubsystem::GetStatId() const
{
	RETURN_QUICK_DECLARE_CYCLE_STAT(UAeonixSubsystem, STATGROUP_Tickables);
}

bool UAeonixSubsystem::IsTickable() const 
{
	return true;
}

bool UAeonixSubsystem::IsTickableInEditor() const 
{
	return true;
}

bool UAeonixSubsystem::IsTickableWhenPaused() const
{
	return true;
}

void UAeonixSubsystem::CompleteAllPendingPathfindingTasks()
{
	FScopeLock Lock(&PathRequestsLock);  // Thread safety - prevent races with UpdateRequests()

	for (int32 i = PathRequests.Num() - 1; i >= 0; --i)
	{
		FAeonixPathFindRequest* Request = PathRequests[i].Get();

		// Only set promise if not already fulfilled by worker thread
		// This prevents crashes from double-setting promises during shutdown
		if (!Request->PathFindFuture.IsReady())
		{
			Request->PathFindPromise.SetValue(EAeonixPathFindStatus::Cancelled);
		}

		// Always execute delegate (safe to call even if worker already did)
		Request->OnPathFindRequestComplete.ExecuteIfBound(EAeonixPathFindStatus::Cancelled);
	}
	PathRequests.Empty();
}

size_t UAeonixSubsystem::GetNumberOfPendingTasks() const
{
	return PathRequests.Num();
}

size_t UAeonixSubsystem::GetNumberOfRegisteredNavAgents() const
{
	return RegisteredNavAgents.Num();
}

size_t UAeonixSubsystem::GetNumberOfRegisteredNavVolumes() const
{
	return RegisteredVolumes.Num();
}

void UAeonixSubsystem::OnBoundingVolumeRegenerated(AAeonixBoundingVolume* Volume)
{
	if (!Volume)
	{
		return;
	}

	UE_LOG(LogAeonixNavigation, Log, TEXT("Subsystem: Navigation regenerated for volume %s - broadcasting and updating debug paths"), *Volume->GetName());

	// Broadcast to external subscribers (like EditorDebugSubsystem)
	OnNavigationRegenCompleted.Broadcast(Volume);

	// Update debug paths for nav agents within this volume
	UpdateDebugPathsForVolume(Volume);
}

void UAeonixSubsystem::UpdateDebugPathsForVolume(AAeonixBoundingVolume* Volume)
{
	if (!Volume)
	{
		return;
	}

	// Find all nav agents within this volume that have debug rendering enabled
	for (const FAeonixNavAgentHandle& AgentHandle : RegisteredNavAgents)
	{
		if (!AgentHandle.NavAgentComponent || !IsValid(AgentHandle.NavAgentComponent))
		{
			continue;
		}

		// Check if this agent is in the regenerated volume
		if (const AAeonixBoundingVolume** AgentVolume = AgentToVolumeMap.Find(AgentHandle.NavAgentComponent))
		{
			if (*AgentVolume == Volume)
			{
				// Check if this agent has debug path rendering enabled
				if (AgentHandle.NavAgentComponent->bEnablePathDebugRendering)
				{
					RequestDebugPathUpdate(AgentHandle.NavAgentComponent);
				}
			}
		}
	}
}

void UAeonixSubsystem::RequestDebugPathUpdate(UAeonixNavAgentComponent* NavComponent)
{
	if (!NavComponent)
	{
		return;
	}

	// Trigger path recalculation and debug rendering for this component
	NavComponent->RegisterPathForDebugRendering();

	UE_LOG(LogAeonixNavigation, Verbose, TEXT("Requested debug path update for nav agent %s"), *NavComponent->GetName());
}

bool UAeonixSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	// All the worlds, so it works in editor
	return true;
}

void UAeonixSubsystem::Deinitialize()
{
	// STEP 1: Mark all requests as cancelled (workers check this via IsStale())
	{
		FScopeLock Lock(&PathRequestsLock);
		for (TUniquePtr<FAeonixPathFindRequest>& Request : PathRequests)
		{
			Request->bCancelled = true;
		}
	}

	// STEP 2: Shutdown worker pool (waits for in-flight tasks to complete)
	// Workers will check bCancelled and handle gracefully
	WorkerPool.Shutdown();

	// STEP 3: Clean up any requests that workers didn't finish
	// This must happen AFTER workers are shut down to avoid race conditions
	CompleteAllPendingPathfindingTasks();

	UE_LOG(LogAeonixNavigation, Log, TEXT("AeonixSubsystem deinitialized"));

	Super::Deinitialize();
}

void UAeonixSubsystem::RegisterPath(TSharedPtr<FAeonixNavigationPath> Path)
{
	if (!Path.IsValid())
	{
		UE_LOG(LogAeonixNavigation, Warning, TEXT("Attempted to register invalid path"));
		return;
	}

	FScopeLock Lock(&PathRegistryLock);
	ActivePaths.Add(Path);
	UE_LOG(LogAeonixNavigation, Verbose, TEXT("Path registered - %d active paths"), ActivePaths.Num());
}

void UAeonixSubsystem::UnregisterPath(FAeonixNavigationPath* Path)
{
	if (!Path)
	{
		return;
	}

	FScopeLock Lock(&PathRegistryLock);

	// Find and remove the matching weak pointer
	for (auto It = ActivePaths.CreateIterator(); It; ++It)
	{
		if (TSharedPtr<FAeonixNavigationPath> PinnedPath = It->Pin())
		{
			if (PinnedPath.Get() == Path)
			{
				It.RemoveCurrent();
				UE_LOG(LogAeonixNavigation, Verbose, TEXT("Path unregistered - %d active paths remaining"), ActivePaths.Num());
				return;
			}
		}
		else
		{
			// Clean up stale pointer while we're here
			It.RemoveCurrent();
		}
	}
}

void UAeonixSubsystem::InvalidatePathsInRegions(const TSet<FGuid>& RegeneratedRegionIds)
{
	if (RegeneratedRegionIds.Num() == 0)
	{
		return;
	}

	// Component-based invalidation approach
	TArray<UAeonixNavAgentComponent*> ComponentsToCheck;

	// Lock only for copying weak object pointers
	{
		FScopeLock Lock(&ComponentPathRegistryLock);
		for (auto It = ComponentsWithPaths.CreateIterator(); It; ++It)
		{
			if (UAeonixNavAgentComponent* Component = It->Get())
			{
				ComponentsToCheck.Add(Component);
			}
			else
			{
				// Clean up stale pointers
				It.RemoveCurrent();
			}
		}
	}
	// Lock released - now safe to access components and call delegates

	int32 NumInvalidated = 0;
	for (UAeonixNavAgentComponent* Component : ComponentsToCheck)
	{
		FAeonixNavigationPath& Path = Component->GetPath();
		if (Path.CheckInvalidation(RegeneratedRegionIds))
		{
			Path.MarkInvalid(); // Broadcasts delegate
			LoadMetrics.InvalidatedPathsTotal.fetch_add(1);
			NumInvalidated++;
		}
	}

	if (NumInvalidated > 0)
	{
		UE_LOG(LogAeonixNavigation, Log, TEXT("Invalidated %d paths across %d components affected by %d regenerated regions"),
			NumInvalidated, ComponentsToCheck.Num(), RegeneratedRegionIds.Num());
	}
}

void UAeonixSubsystem::TrackPathRegions(FAeonixNavigationPath& Path, const AAeonixBoundingVolume* BoundingVolume)
{
	if (!BoundingVolume)
	{
		return;
	}

	const FAeonixGenerationParameters& Params = BoundingVolume->GenerationParameters;
	if (Params.DynamicRegionBoxes.Num() == 0)
	{
		// No dynamic regions to track
		return;
	}

	// Check each path point against all dynamic regions
	const TArray<FAeonixPathPoint>& PathPoints = Path.GetPathPoints();
	for (const FAeonixPathPoint& Point : PathPoints)
	{
		for (const auto& RegionPair : Params.DynamicRegionBoxes)
		{
			if (RegionPair.Value.IsInsideOrOn(Point.Position))
			{
				Path.AddTraversedRegion(RegionPair.Key);
			}
		}
	}

	if (Path.GetTraversedRegionIds().Num() > 0)
	{
		UE_LOG(LogAeonixNavigation, Verbose, TEXT("Path tracks %d dynamic regions"), Path.GetTraversedRegionIds().Num());
	}
}

void UAeonixSubsystem::RegisterComponentWithPath(UAeonixNavAgentComponent* Component)
{
	if (!Component)
	{
		return;
	}

	FScopeLock Lock(&ComponentPathRegistryLock);
	ComponentsWithPaths.Add(Component);
	UE_LOG(LogAeonixNavigation, Verbose, TEXT("Registered component %s for path invalidation tracking"), *Component->GetName());
}

void UAeonixSubsystem::UnregisterComponentWithPath(UAeonixNavAgentComponent* Component)
{
	if (!Component)
	{
		return;
	}

	FScopeLock Lock(&ComponentPathRegistryLock);
	ComponentsWithPaths.Remove(Component);
	UE_LOG(LogAeonixNavigation, Verbose, TEXT("Unregistered component %s from path invalidation tracking"), *Component->GetName());
}

// Region versioning for invalidation detection

uint32 UAeonixSubsystem::GetRegionVersion(const FGuid& RegionId) const
{
	FScopeLock Lock(&RegionVersionLock);
	const uint32* Version = RegionVersionMap.Find(RegionId);
	return Version ? *Version : 0;
}

void UAeonixSubsystem::IncrementRegionVersion(const FGuid& RegionId)
{
	FScopeLock Lock(&RegionVersionLock);
	uint32& Version = RegionVersionMap.FindOrAdd(RegionId, 0);
	Version++;
	UE_LOG(LogAeonixNavigation, Verbose, TEXT("Incremented region %s version to %d"), *RegionId.ToString(), Version);
}

// Request requeuing for lock contention

void UAeonixSubsystem::RequeuePathfindRequest(TUniquePtr<FAeonixPathFindRequest>&& Request, float DelaySeconds)
{
	if (!Request.IsValid())
	{
		return;
	}

	// Re-add to queue with delay (will be processed in next tick)
	// In a production system, you might want a separate delayed queue
	FScopeLock Lock(&PathRequestsLock);
	PathRequests.Add(MoveTemp(Request));
	LoadMetrics.PendingPathfinds.fetch_add(1);

	UE_LOG(LogAeonixNavigation, Verbose, TEXT("Requeued pathfind request due to lock contention (delay: %.3fs)"), DelaySeconds);
}

// Try to acquire read lock with timeout

bool UAeonixSubsystem::TryAcquirePathfindReadLock(const AAeonixBoundingVolume* Volume, float TimeoutSeconds)
{
	if (!Volume)
	{
		return false;
	}

	// For now, we'll use a simple approach - just try to acquire
	// In a more sophisticated implementation, you could use FRWLock::TryReadLock if available
	// For this implementation, we'll assume lock acquisition succeeds
	// The actual locking happens in the pathfinding lambda
	return true;
}
