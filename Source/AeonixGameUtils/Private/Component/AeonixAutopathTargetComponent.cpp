#include "Component/AeonixAutopathTargetComponent.h"
#include "Subsystem/AeonixAutopathSubsystem.h"
#include "AeonixNavigation.h"

#include "GameFramework/Actor.h"

UAeonixAutopathTargetComponent::UAeonixAutopathTargetComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Component does not tick - subsystem handles all autopath ticking
	PrimaryComponentTick.bCanEverTick = false;
}

void UAeonixAutopathTargetComponent::OnRegister()
{
	Super::OnRegister();

	// Register with subsystem (works in editor and at runtime)
	RegisterWithSubsystem();
}

void UAeonixAutopathTargetComponent::OnUnregister()
{
	// Unregister from subsystem
	UnregisterFromSubsystem();

	Super::OnUnregister();
}

void UAeonixAutopathTargetComponent::BeginPlay()
{
	Super::BeginPlay();

	// Reset state from editor world - PIE creates new world with new actor instances
	bRegisteredWithSubsystem = false;

	RegisterWithSubsystem();
}

void UAeonixAutopathTargetComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// Unregister from subsystem
	UnregisterFromSubsystem();

	Super::EndPlay(EndPlayReason);
}

void UAeonixAutopathTargetComponent::RegisterWithSubsystem()
{
	if (bRegisteredWithSubsystem)
	{
		return; // Already registered
	}

	// Get reference to the Autopath subsystem
	if (UWorld* World = GetWorld())
	{
		UAeonixAutopathSubsystem* Subsystem = World->GetSubsystem<UAeonixAutopathSubsystem>();
		if (!Subsystem)
		{
			UE_LOG(LogAeonixNavigation, Verbose, TEXT("AutopathTarget %s: No AeonixAutopathSubsystem found"), *GetName());
			return;
		}

		// Register with the subsystem
		Subsystem->RegisterAutopathTarget(this);
		bRegisteredWithSubsystem = true;

		UE_LOG(LogAeonixNavigation, Verbose, TEXT("AutopathTarget %s: Registered with subsystem"), *GetName());
	}
}

void UAeonixAutopathTargetComponent::UnregisterFromSubsystem()
{
	if (!bRegisteredWithSubsystem)
	{
		return; // Not registered
	}

	// Unregister from subsystem
	UWorld* World = GetWorld();
	if (!World)
	{
		bRegisteredWithSubsystem = false;
		return;
	}

	UAeonixAutopathSubsystem* Subsystem = World->GetSubsystem<UAeonixAutopathSubsystem>();
	if (!Subsystem)
	{
		bRegisteredWithSubsystem = false;
		return;
	}

	Subsystem->UnregisterAutopathTarget(this);
	bRegisteredWithSubsystem = false;

	UE_LOG(LogAeonixNavigation, Verbose, TEXT("AutopathTarget %s: Unregistered from subsystem"), *GetName());
}
