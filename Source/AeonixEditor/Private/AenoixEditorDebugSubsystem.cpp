// Copyright 2024 Chris Ashworth



#include <AeonixEditor/Private/AenoixEditorDebugSubsystem.h>
#include <AeonixEditor/Private/AeonixPathDebugActor.h>

#include <AeonixNavigation/Public/Actor/AeonixBoundingVolume.h>

#include "Component/AeonixNavAgentComponent.h"
#include "Subsystem/AeonixSubsystem.h"

#include "EngineUtils.h"

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

	// TODO: if we're in Editor, the bounding volumes only register themselves on BeginPlay, so just force register them here for now
	UAeonixSubsystem* AeonixSubsystem = DebugActor->GetWorld()->GetSubsystem<UAeonixSubsystem>();
	for (TActorIterator<AAeonixBoundingVolume> ActorItr(DebugActor->GetWorld()); ActorItr; ++ActorItr)
	{
		AeonixSubsystem->RegisterVolume(*ActorItr);
	}

	AeonixSubsystem->RegisterNavComponent(DebugActor->NavAgentComponent);
	
	AeonixSubsystem->UpdateComponents();

	// TODO: same with nav components
	

	// If we've got a valid start and end
	if (StartDebugActor && EndDebugActor)
	{
		// Find a path between them
		if (AeonixSubsystem->FindPathImmediateAgent(StartDebugActor->NavAgentComponent, EndDebugActor->GetActorLocation(), CurrentDebugPath))
		//if (DebugActor->NavAgentComponent->FindPathImmediate(StartDebugActor->GetActorLocation(), EndDebugActor->GetActorLocation(), &Path))
		{
			// Debug draw the path
			CurrentDebugPath.DebugDraw(DebugActor->GetWorld(), AeonixSubsystem->GetVolumeForAgent(DebugActor->NavAgentComponent)->GetNavData());
		}
	}
}