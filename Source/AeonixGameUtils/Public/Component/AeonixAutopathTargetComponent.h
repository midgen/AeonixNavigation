#pragma once

#include "Components/ActorComponent.h"

#include "AeonixAutopathTargetComponent.generated.h"

class UAeonixAutopathSubsystem;

/**
 * Component that marks an actor as an autopath target.
 * Only one target can be registered at a time.
 * The component does not tick itself - instead the UAeonixAutopathSubsystem
 * tracks the registered target and triggers pathfinding when it moves.
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class AEONIXGAMEUTILS_API UAeonixAutopathTargetComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UAeonixAutopathTargetComponent(const FObjectInitializer& ObjectInitializer);

	/** Position threshold in cm - triggers pathfinding when moved beyond this distance */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aeonix|Autopath", meta = (ClampMin = "0.0"))
	float PositionThreshold = 50.0f;

	/** Whether this target is active for autopath tracking */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aeonix|Autopath")
	bool bEnableAutopath = true;

protected:
	virtual void OnRegister() override;
	virtual void OnUnregister() override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	/** Whether we're currently registered with the subsystem */
	bool bRegisteredWithSubsystem = false;

	/** Initialize and register with subsystem */
	void RegisterWithSubsystem();

	/** Unregister from subsystem */
	void UnregisterFromSubsystem();
};
