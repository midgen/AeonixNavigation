
#include <AeonixNavigation/Public/Component/AeonixNavAgentComponent.h>

#include <AeonixNavigation/Public/Subsystem/AeonixSubsystem.h>
#include <AeonixNavigation/Public/AeonixNavigation.h>

#include <Runtime/Engine/Classes/GameFramework/Actor.h>
#include <Runtime/Engine/Public/DrawDebugHelpers.h>

UAeonixNavAgentComponent::UAeonixNavAgentComponent(const FObjectInitializer& ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UAeonixNavAgentComponent::BeginPlay()
{
	Super::BeginPlay();

	AeonixSubsystem = GetWorld()->GetSubsystem<UAeonixSubsystem>();
	if (!AeonixSubsystem.GetInterface())
	{
		UE_LOG(AeonixNavigation, Error, TEXT("No AeonixSubsystem with a valid AeonixInterface found"));
	}
	else
	{
		AeonixSubsystem->RegisterNavComponent(this, EAeonixMassEntityFlag::YES);
	}
}

void UAeonixNavAgentComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (!AeonixSubsystem.GetInterface())
	{
		UE_LOG(AeonixNavigation, Error, TEXT("No AeonixSubsystem with a valid AeonixInterface found"));
	}
	else
	{
		// No need to destroy the mass entity here, as the Mass world is being destroyed anyway
		AeonixSubsystem->UnRegisterNavComponent(this, EAeonixMassEntityFlag::NO);
	}

	Super::EndPlay(EndPlayReason);
}

FVector UAeonixNavAgentComponent::GetAgentPosition() const
{
	FVector Result;

	AController* Controller = Cast<AController>(GetOwner());

	if (Controller)
	{
		if (APawn* Pawn = Controller->GetPawn())
			Result = Pawn->GetActorLocation();
	}
	else // Maybe this is just on a debug actor, rather than an AI controller
	{
		Result = GetOwner()->GetActorLocation();
	}

	return Result;
}