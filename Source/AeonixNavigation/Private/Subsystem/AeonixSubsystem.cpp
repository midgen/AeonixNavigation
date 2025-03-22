#include <AeonixNavigation/Public/Subsystem/AeonixSubsystem.h>

#include <AeonixNavigation/Public/Actor/AeonixBoundingVolume.h>
#include <AeonixNavigation/Public/Util/AeonixMediator.h>
#include <AeonixNavigation/Public/Component/AeonixNavAgentComponent.h>
#include <AeonixNavigation/Public/AeonixNavigation.h>

void UAeonixSubsystem::RegisterVolume(const AAeonixBoundingVolume* Volume)
{
	RegisteredVolumes.AddUnique(Volume);
}

void UAeonixSubsystem::UnRegisterVolume(const AAeonixBoundingVolume* Volume)
{
	RegisteredVolumes.RemoveSingleSwap(Volume, EAllowShrinking::No);
}

void UAeonixSubsystem::RegisterNavComponent(UAeonixNavAgentComponent* NavComponent)
{
	RegisteredNavComponents.AddUnique(NavComponent);
}

void UAeonixSubsystem::UnRegisterNavComponent(UAeonixNavAgentComponent* NavComponent)
{
	RegisteredNavComponents.RemoveSingleSwap(NavComponent, EAllowShrinking::No);
}

const AAeonixBoundingVolume* UAeonixSubsystem::GetVolumeForPosition(const FVector& Position)
{
	for (const AAeonixBoundingVolume* Volume : RegisteredVolumes)
	{
		if (Volume->EncompassesPoint(Position))
		{
			return Volume;
		}
	}

	return nullptr;
}

// bool UAeonixSubsystem::FindPathImmediatePosition(const FVector& Start, const FVector& End, FAeonixNavigationPath& OutPath)
// {
// 	
//
//
// 	
// 	return false;
// }

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
	
		AeonixPathFinder pathFinder(GetWorld(), NavVolume->GetNavData(), NavigationComponent->PathfinderSettings);
	
		int32 Result = pathFinder.FindPath(StartNavLink, TargetNavLink, NavigationComponent->GetAgentPosition(), End, OutPath);
	
		OutPath.SetIsReady(true);
	
	return true;
}

const AAeonixBoundingVolume* UAeonixSubsystem::GetVolumeForAgent(const UAeonixNavAgentComponent* NavigationComponent)
{
	return AgentToVolumeMap[NavigationComponent];
}

void UAeonixSubsystem::UpdateComponents()
{
	for (int32 i = RegisteredNavComponents.Num() -1; i >= 0 ;)
	{
		UAeonixNavAgentComponent* NavComponent = RegisteredNavComponents[i];
		
		// // We might have some nulled nav agents due to copying actors in editor
		// if (!NavComponent)
		// {
		// 	RegisteredNavComponents.RemoveAtSwap(i);
		// 	AgentToVolumeMap.Remove(NavComponent);
		// 	continue;
		// }
		
		bool bIsInValidVolume = false;

		for (const AAeonixBoundingVolume* Volume : RegisteredVolumes)
		{
			if (Volume->EncompassesPoint(NavComponent->GetAgentPosition()))
			{
				AgentToVolumeMap.Add(NavComponent, Volume);
				bIsInValidVolume = true;
				break;
			}
		}

		if (!bIsInValidVolume && AgentToVolumeMap.Contains(NavComponent))
		{
			AgentToVolumeMap[NavComponent] = nullptr;
		}
		i--;
	}

}


void UAeonixSubsystem::Tick(float DeltaTime)
{
	UpdateComponents();
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

bool UAeonixSubsystem::DoesSupportWorldType(const EWorldType::Type WorldType) const
{
	// All the worlds, so it works in editor
	return true;
}
