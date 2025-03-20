#include <AeonixNavigation/Public/Subsystem/AeonixSubsystem.h>

#include <AeonixNavigation/Public/Actor/AeonixBoundingVolume.h>
#include <AeonixNavigation/Public/Component/AeonixNavigationComponent.h>

void UAeonixSubsystem::RegisterVolume(const AAeonixBoundingVolume* Volume)
{
	RegisteredVolumes.AddUnique(Volume);
}

void UAeonixSubsystem::UnRegisterVolume(const AAeonixBoundingVolume* Volume)
{
	RegisteredVolumes.RemoveSingleSwap(Volume, EAllowShrinking::No);
}

void UAeonixSubsystem::RegisterNavComponent(UAeonixNavigationComponent* NavComponent)
{
	RegisteredNavComponents.AddUnique(NavComponent);
}

void UAeonixSubsystem::UnRegisterNavComponent(UAeonixNavigationComponent* NavComponent)
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

void UAeonixSubsystem::Tick(float DeltaTime)
{
	for (UAeonixNavigationComponent* NavComponent : RegisteredNavComponents)
	{
		bool bIsInValidVolume = false;

		for (const AAeonixBoundingVolume* Volume : RegisteredVolumes)
		{
			// float DistanceToPoint = 0.f;
			if (Volume->EncompassesPoint(NavComponent->GetPawnPosition())) //, 0.5f));//, &DistanceToPoint))
			{
				NavComponent->SetCurrentNavVolume(Volume);
				bIsInValidVolume = true;
				break;
			}
		}

		if (!bIsInValidVolume)
		{
			NavComponent->SetCurrentNavVolume(nullptr);
		}
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
