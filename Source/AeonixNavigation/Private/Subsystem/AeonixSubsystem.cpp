



#include <AeonixNavigation/Public/Subsystem/AeonixSubsystem.h>

#include <AeonixNavigation/Public/AeonixNavigation.h>
#include <AeonixNavigation/Public/Actor/AeonixBoundingVolume.h>
#include <AeonixNavigation/Public/Component/AeonixNavAgentComponent.h>
#include <AeonixNavigation/Public/Task/AeonixFindPathTask.h>
#include <AeonixNavigation/Public/Util/AeonixMediator.h>
#include <AeonixNavigation/Public/Mass/AeonixFragments.h>
#include <AeonixNavigation/Public/Data/AeonixHandleTypes.h>

#include <MassCommon/Public/MassCommonFragments.h>
#include <Runtime/MassEntity/Public/MassEntityManager.h>
#include <Runtime/MassEntity/Public/MassEntitySubsystem.h>

void UAeonixSubsystem::RegisterVolume(AAeonixBoundingVolume* Volume)
{
	for (FAeonixBoundingVolumeHandle& Handle : RegisteredVolumes)
	{
		if (Handle.VolumeHandle == Volume)
		{
			return;
		}
	}

	RegisteredVolumes.Emplace(Volume);
}

void UAeonixSubsystem::UnRegisterVolume(AAeonixBoundingVolume* Volume)
{
	UMassEntitySubsystem* MassEntitySubsystem = GetWorld()->GetSubsystem<UMassEntitySubsystem>();
	FMassEntityManager& EntityManager = MassEntitySubsystem->GetMutableEntityManager();
	
	for (FAeonixBoundingVolumeHandle& Handle : RegisteredVolumes)
	{
		if (Handle.VolumeHandle == Volume)
		{
			EntityManager.DestroyEntity(Handle.EntityHandle);
			RegisteredVolumes.Remove(Handle);
			return;
		}
	}

	UE_LOG(AeonixNavigation, Error, TEXT("Tried to remove a volume that isn't registered"))
}

void UAeonixSubsystem::RegisterDynamicSubregion(AAeonixDynamicSubregion* DynamicSubregion)
{
	
}

void UAeonixSubsystem::UnRegisterDynamicSubregion(AAeonixDynamicSubregion* DynamicSubregion)
{
	
}

void UAeonixSubsystem::RegisterNavComponent(UAeonixNavAgentComponent* NavComponent, bool bCreateMassEntity)
{
	if (RegisteredNavAgents.Contains(NavComponent))
	{
		return;
	}

	FMassEntityHandle Entity;

	if (bCreateMassEntity)
	{
		UMassEntitySubsystem* MassEntitySubsystem = GetWorld()->GetSubsystem<UMassEntitySubsystem>();
		FMassEntityManager& EntityManager = MassEntitySubsystem->GetMutableEntityManager();

		FMassArchetypeCompositionDescriptor Composition;
		Composition.Fragments.Add<FTransformFragment>();
		Composition.Fragments.Add<FAeonixNavAgentFragment>(); 

		FMassArchetypeHandle Archetype = EntityManager.CreateArchetype(Composition);
		Entity = EntityManager.CreateEntity(Archetype);
	}
	
	RegisteredNavAgents.Emplace(NavComponent, Entity);
}

void UAeonixSubsystem::UnRegisterNavComponent(UAeonixNavAgentComponent* NavComponent, bool bDestroyMassEntity)
{
	for (int i = 0; i < RegisteredNavAgents.Num(); i++)
	{
		FAeonixNavAgentHandle& Agent = RegisteredNavAgents[i];
		if (Agent.NavAgentComponent == NavComponent)
		{
			RegisteredNavAgents.RemoveAtSwap(i, EAllowShrinking::No);
			if (bDestroyMassEntity)
			{
				UMassEntitySubsystem* MassEntitySubsystem = GetWorld()->GetSubsystem<UMassEntitySubsystem>();
				FMassEntityManager& EntityManager = MassEntitySubsystem->GetMutableEntityManager();
				EntityManager.DestroyEntity(Agent.EntityHandle);
			}
			break;
		}
	}
}

const AAeonixBoundingVolume* UAeonixSubsystem::GetVolumeForPosition(const FVector& Position)
{
	for (FAeonixBoundingVolumeHandle& Handle : RegisteredVolumes)
	{
		if (Handle.VolumeHandle->EncompassesPoint(Position))
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
	if (!AeonixMediator::GetLinkFromPosition(NavigationComponent->GetAgentPosition(), *NavVolume, StartNavLink))
	{
		UE_LOG(AeonixNavigation, Error, TEXT("Path finder failed to find start nav link"));
		return false;
	}

	if (!AeonixMediator::GetLinkFromPosition(End, *NavVolume, TargetNavLink))
	{
		UE_LOG(AeonixNavigation, Error, TEXT("Path finder failed to find target nav link"));
		return false;
	}

	OutPath.ResetForRepath();

	AeonixPathFinder pathFinder(NavVolume->GetNavData(), NavigationComponent->PathfinderSettings);

	int32 Result = pathFinder.FindPath(StartNavLink, TargetNavLink, NavigationComponent->GetAgentPosition(), End, OutPath);

	OutPath.SetIsReady(true);
	
	return true;
}

FAeonixPathFindRequestCompleteDelegate& UAeonixSubsystem::FindPathAsyncAgent(UAeonixNavAgentComponent* NavigationComponent, const FVector& End, FAeonixNavigationPath& OutPath)
{
	const AAeonixBoundingVolume* NavVolume = GetVolumeForAgent(NavigationComponent);

	AeonixLink StartNavLink;
	AeonixLink TargetNavLink;

	FAeonixPathFindRequest& Request = PathRequests.Emplace_GetRef();
	
	// Get the nav link from our volume
	if (!AeonixMediator::GetLinkFromPosition(NavigationComponent->GetAgentPosition(), *NavVolume, StartNavLink))
	{
		UE_LOG(AeonixNavigation, Error, TEXT("Path finder failed to find start nav link"));
		//Request (EAeonixPathFindStatus::Failed);
		Request.PathFindPromise.SetValue(EAeonixPathFindStatus::Failed);
		return Request.OnPathFindRequestComplete;
	}

	if (!AeonixMediator::GetLinkFromPosition(End, *NavVolume, TargetNavLink))
	{
		UE_LOG(AeonixNavigation, Error, TEXT("Path finder failed to find target nav link"));
		Request.PathFindPromise.SetValue(EAeonixPathFindStatus::Failed);
		return Request.OnPathFindRequestComplete;
	}

	// Make sure the path isn't flagged ready
	OutPath.ResetForRepath();
	OutPath.SetIsReady(false);

	// Kick off the pathfinding on the task graphs
	// TODO: Bit more scope in this lambda than I'd like
	FGraphEventRef Task = FFunctionGraphTask::CreateAndDispatchWhenReady([&Request, NavVolume, NavigationComponent, StartNavLink, TargetNavLink, End, &OutPath ]()
	{
		AeonixPathFinder PathFinder(NavVolume->GetNavData(), NavigationComponent->PathfinderSettings);
			
		if (PathFinder.FindPath(StartNavLink, TargetNavLink, NavigationComponent->GetAgentPosition(), End, OutPath))
		{
			Request.PathFindPromise.SetValue(EAeonixPathFindStatus::Complete);
		}
		else
		{
			Request.PathFindPromise.SetValue(EAeonixPathFindStatus::Failed);
		}
	}, TStatId(), nullptr, ENamedThreads::AnyBackgroundThreadNormalTask);

	return Request.OnPathFindRequestComplete;
}

const AAeonixBoundingVolume* UAeonixSubsystem::GetVolumeForAgent(const UAeonixNavAgentComponent* NavigationComponent)
{
	return AgentToVolumeMap[NavigationComponent];
}

void UAeonixSubsystem::UpdateComponents()
{
	for (int32 i = RegisteredNavAgents.Num() -1; i >= 0 ;)
	{
		FAeonixNavAgentHandle& AgentHandle = RegisteredNavAgents[i];
		
		bool bIsInValidVolume = false;

		for (FAeonixBoundingVolumeHandle& Handle : RegisteredVolumes)
		{
			if (Handle.VolumeHandle->EncompassesPoint(AgentHandle.NavAgentComponent->GetAgentPosition()))
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
		i--;
	}

}


void UAeonixSubsystem::Tick(float DeltaTime)
{
	UpdateComponents();
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
	for (int32 i = 0; i < PathRequests.Num();)
	{
		FAeonixPathFindRequest& Request = PathRequests[i];

		Request.PathFindPromise.SetValue(EAeonixPathFindStatus::Failed);
	}
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

bool UAeonixSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	// All the worlds, so it works in editor
	return true;
}
