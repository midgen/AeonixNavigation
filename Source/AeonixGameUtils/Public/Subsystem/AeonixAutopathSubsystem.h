#pragma once

#include "Data/AeonixTypes.h"
#include "Subsystems/WorldSubsystem.h"

#include "AeonixAutopathSubsystem.generated.h"

class UAeonixAutopathComponent;
class UAeonixAutopathTargetComponent;

/**
 * Subsystem that manages autopath components, tracking movement and triggering pathfinding.
 * When source or target actors move beyond their position threshold, an async pathfind is requested.
 */
UCLASS()
class AEONIXGAMEUTILS_API UAeonixAutopathSubsystem : public UTickableWorldSubsystem
{
	GENERATED_BODY()

public:
	// Registration API (called by components)
	void RegisterAutopathSource(UAeonixAutopathComponent* Component);
	void UnregisterAutopathSource(UAeonixAutopathComponent* Component);
	void RegisterAutopathTarget(UAeonixAutopathTargetComponent* Component);
	void UnregisterAutopathTarget(UAeonixAutopathTargetComponent* Component);

	// Access to registered components
	const TArray<UAeonixAutopathComponent*>& GetRegisteredSources() const { return RegisteredSources; }
	UAeonixAutopathTargetComponent* GetRegisteredTarget() const { return RegisteredTarget; }

	// UTickableWorldSubsystem interface
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;
	virtual void Tick(float DeltaTime) override;
	virtual TStatId GetStatId() const override;

	virtual bool IsTickable() const override;
	virtual bool IsTickableInEditor() const override;
	virtual bool IsTickableWhenPaused() const override;

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;

private:
	UPROPERTY(Transient)
	TArray<UAeonixAutopathComponent*> RegisteredSources{};

	/** The single registered target (only one allowed) */
	UPROPERTY(Transient)
	UAeonixAutopathTargetComponent* RegisteredTarget = nullptr;

	// Track last positions for threshold comparison
	// Not UPROPERTY - we manually clean up stale actor keys in ProcessAutopathSources()
	TMap<AActor*, FVector> SourceLastPositionMap;

	FVector TargetLastPosition = FVector::ZeroVector;
	bool bTargetPositionInitialized = false;

	/** Process all autopath sources and trigger pathfinding as needed */
	void ProcessAutopathSources();

	/** Check if an actor has moved beyond threshold since last check */
	bool HasMovedBeyondThreshold(AActor* Actor, TMap<AActor*, FVector>& LastPositionMap, float Threshold);

	/** Request async pathfinding for a source component */
	void RequestAsyncPathfinding(UAeonixAutopathComponent* Source);
};
