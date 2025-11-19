



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

#include "MassCommonFragments.h"
#include "MassEntityManager.h"
#include "MassEntitySubsystem.h"

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
			Composition.GetFragments().Add<FTransformFragment>();
			Composition.GetFragments().Add<FAeonixNavAgentFragment>();

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
		if (!ObstacleLastTransformMap.Contains(ObstacleComponent))
		{
			ObstacleLastTransformMap.Add(ObstacleComponent, Owner->GetActorTransform());
		}

		// Immediately determine which bounding volume and regions the obstacle is in
		// This prevents timing issues where TriggerNavigationRegen() is called before first tick
		const FVector CurrentPosition = Owner->GetActorLocation();
		AAeonixBoundingVolume* FoundVolume = nullptr;
		TSet<FGuid> FoundRegionIds;

		UE_LOG(LogAeonixNavigation, Log, TEXT("RegisterDynamicObstacle: %s at %s, checking %d volumes"),
			*ObstacleComponent->GetName(), *CurrentPosition.ToString(), RegisteredVolumes.Num());

		for (FAeonixBoundingVolumeHandle& Handle : RegisteredVolumes)
		{
			if (Handle.VolumeHandle)
			{
				const FBox VolumeBounds = Handle.VolumeHandle->GetComponentsBoundingBox(true);
				const bool bIsInside = Handle.VolumeHandle->IsPointInside(CurrentPosition);
				UE_LOG(LogAeonixNavigation, Log, TEXT("  Volume %s: Bounds Min=%s Max=%s, IsPointInside=%d"),
					*Handle.VolumeHandle->GetName(), *VolumeBounds.Min.ToString(), *VolumeBounds.Max.ToString(), bIsInside);

				if (bIsInside)
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
	{
		FReadScopeLock ReadLock(NavVolume->GetOctreeDataLock());
		SCOPE_CYCLE_COUNTER(STAT_AeonixPathfindingSync);

		AeonixPathFinder pathFinder(NavVolume->GetNavData(), NavigationComponent->PathfinderSettings);
		Result = pathFinder.FindPath(StartNavLink, TargetNavLink, NavigationComponent->GetPathfindingStartPosition(), NavigationComponent->GetPathfindingEndPosition(End), OutPath);
	}

	OutPath.SetIsReady(true);
	UE_LOG(LogAeonixNavigation, Log, TEXT("AeonixSubsystem: Path found with %d points, marked as ready"), OutPath.GetPathPoints().Num());

	return Result;
}

FAeonixPathFindRequestCompleteDelegate& UAeonixSubsystem::FindPathAsyncAgent(UAeonixNavAgentComponent* NavigationComponent, const FVector& End, FAeonixNavigationPath& OutPath)
{
	AeonixLink StartNavLink;
	AeonixLink TargetNavLink;

	FAeonixPathFindRequest& Request = PathRequests.Emplace_GetRef();
	
	const AAeonixBoundingVolume* NavVolume = GetVolumeForAgent(NavigationComponent);

	if (!NavVolume)
	{
		UE_LOG(LogAeonixNavigation, Error, TEXT("Nav Agent Not In A Volume"));
		Request.PathFindPromise.SetValue(EAeonixPathFindStatus::Failed);
		return Request.OnPathFindRequestComplete;
	}
	
	// Get the nav link from our volume
	if (!AeonixMediator::GetLinkFromPosition(NavigationComponent->GetPathfindingStartPosition(), *NavVolume, StartNavLink))
	{
		UE_LOG(LogAeonixNavigation, Error, TEXT("Path finder failed to find start nav link"));
		Request.PathFindPromise.SetValue(EAeonixPathFindStatus::Failed);
		return Request.OnPathFindRequestComplete;
	}

	if (!AeonixMediator::GetLinkFromPosition(NavigationComponent->GetPathfindingEndPosition(End), *NavVolume, TargetNavLink))
	{
		UE_LOG(LogAeonixNavigation, Error, TEXT("Path finder failed to find target nav link"));
		Request.PathFindPromise.SetValue(EAeonixPathFindStatus::Failed);
		return Request.OnPathFindRequestComplete;
	}

	if (TargetNavLink == StartNavLink)
	{
		// TODO: this should succeed
		UE_LOG(LogAeonixNavigation, Error, TEXT("Trying to path from same start and end navlink"));
		Request.PathFindPromise.SetValue(EAeonixPathFindStatus::Failed);
		return Request.OnPathFindRequestComplete;
	}

	// Make sure the path isn't flagged ready
	OutPath.ResetForRepath();
	OutPath.SetIsReady(false);

	// Copy necessary data to avoid dangling references in async task
	// Using TWeakObjectPtr for UObjects that could be destroyed
	TWeakObjectPtr<const AAeonixBoundingVolume> WeakNavVolume = NavVolume;
	FAeonixPathFinderSettings PathfinderSettingsCopy = NavigationComponent->PathfinderSettings;
	FVector StartPosition = NavigationComponent->GetPathfindingStartPosition();
	FVector EndPosition = NavigationComponent->GetPathfindingEndPosition(End);

	// Kick off the pathfinding on the task graphs
	FGraphEventRef Task = FFunctionGraphTask::CreateAndDispatchWhenReady(
		[&Request, WeakNavVolume, PathfinderSettingsCopy, StartNavLink, TargetNavLink, StartPosition, EndPosition, &OutPath]()
	{
		SCOPE_CYCLE_COUNTER(STAT_AeonixPathfindingAsync);

		// Check if volume is still valid
		const AAeonixBoundingVolume* NavVolume = WeakNavVolume.Get();
		if (!NavVolume)
		{
			UE_LOG(LogAeonixNavigation, Warning, TEXT("AeonixSubsystem: Nav volume destroyed during async pathfinding"));
			OutPath.SetIsReady(false);
			Request.PathFindPromise.SetValue(EAeonixPathFindStatus::Failed);
			return;
		}

		// Acquire read lock for thread-safe octree access during pathfinding
		FReadScopeLock ReadLock(NavVolume->GetOctreeDataLock());

		AeonixPathFinder PathFinder(NavVolume->GetNavData(), PathfinderSettingsCopy);

		if (PathFinder.FindPath(StartNavLink, TargetNavLink, StartPosition, EndPosition, OutPath))
		{
			OutPath.SetIsReady(true);
			UE_LOG(LogAeonixNavigation, Log, TEXT("AeonixSubsystem: Async path found with %d points, marked as ready"), OutPath.GetPathPoints().Num());
			Request.PathFindPromise.SetValue(EAeonixPathFindStatus::Complete);
		}
		else
		{
			OutPath.SetIsReady(false);
			Request.PathFindPromise.SetValue(EAeonixPathFindStatus::Failed);
		}
	}, TStatId(), nullptr, ENamedThreads::AnyBackgroundThreadNormalTask);

	return Request.OnPathFindRequestComplete;
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
	// Clean up null/invalid obstacle components (iterate backwards for safe removal)
	for (int32 i = RegisteredDynamicObstacles.Num() - 1; i >= 0; i--)
	{
		UAeonixDynamicObstacleComponent* Obstacle = RegisteredDynamicObstacles[i];

		if (!Obstacle || !IsValid(Obstacle))
		{
			ObstacleLastTransformMap.Remove(Obstacle);
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

		// Get current and last transform
		const FTransform CurrentTransform = Owner->GetActorTransform();
		FTransform* LastTransform = ObstacleLastTransformMap.Find(Obstacle);
		if (!LastTransform)
		{
			// Initialize if missing
			ObstacleLastTransformMap.Add(Obstacle, CurrentTransform);
			LastTransform = ObstacleLastTransformMap.Find(Obstacle);
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
	for (int32 i = 0; i < PathRequests.Num();)
	{
		FAeonixPathFindRequest& Request = PathRequests[i];

		// If our task has finished
		if (Request.PathFindFuture.IsReady())
		{
			EAeonixPathFindStatus Status = Request.PathFindFuture.Get();
			Request.OnPathFindRequestComplete.ExecuteIfBound(Status);
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
	for (int32 i = PathRequests.Num() - 1; i >= 0; --i)
	{
		FAeonixPathFindRequest& Request = PathRequests[i];
		Request.PathFindPromise.SetValue(EAeonixPathFindStatus::Failed);
		Request.OnPathFindRequestComplete.ExecuteIfBound(EAeonixPathFindStatus::Failed);
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
