// Copyright 2024 Chris Ashworth



#include <AeonixEditor/Private/AenoixEditorDebugSubsystem.h>
#include <AeonixEditor/Private/AeonixPathDebugActor.h>

#include <AeonixNavigation/Public/Actor/AeonixBoundingVolume.h>

#include "Component/AeonixNavigationComponent.h"
#include "Subsystem/AeonixSubsystem.h"

#include "EngineUtils.h"

UAenoixEditorDebugSubsystem::UAenoixEditorDebugSubsystem(const FObjectInitializer& Initializer)
{
	Path = MakeShareable<FAeonixNavigationPath>(new FAeonixNavigationPath());
}

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

	// Now pathfind between the two and show all the debug stuff

	UAeonixSubsystem* AeonixSubsystem = GEngine->GetEngineSubsystem<UAeonixSubsystem>();
	for (TActorIterator<AAeonixBoundingVolume> ActorItr(DebugActor->GetWorld()); ActorItr; ++ActorItr)
	{
		AeonixSubsystem->RegisterVolume(*ActorItr);
	}
	
	if (StartDebugActor && EndDebugActor)
	{
		if (DebugActor->NavigationComponent->FindPathImmediate(StartDebugActor->GetActorLocation(), EndDebugActor->GetActorLocation(), &Path))
		{
			Path->DebugDraw(DebugActor->GetWorld(), DebugActor->NavigationComponent->CurrentNavVolume->GetNavData());
		}
	}
}