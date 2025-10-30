// Copyright 2024 Chris Ashworth

#pragma once

#include "Navigation/PathFollowingComponent.h"
#include "CoreMinimal.h"
#include "AeonixPathFollowingComponent.generated.h"

struct FAeonixNavigationPath;
struct FAeonixPathPoint;

USTRUCT(BlueprintType)
struct AEONIXNAVIGATION_API FAeonixFlightSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flight")
	float MaxSpeed = 1200.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flight")
	float TurnRate = 180.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flight")
	float AcceptanceRadius = 100.0f;
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class AEONIXNAVIGATION_API UAeonixPathFollowingComponent : public UPathFollowingComponent
{
	GENERATED_BODY()

public:
	UAeonixPathFollowingComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aeonix Flight")
	FAeonixFlightSettings FlightSettings;

protected:
	virtual void BeginPlay() override;

	// Override path following methods
	virtual void FollowPathSegment(float DeltaTime) override;
	virtual void UpdatePathSegment() override;
	virtual void SetMoveSegment(int32 SegmentStartIndex) override;
	virtual bool HasReached(const FVector& TestPoint, float AcceptanceRadiusOverride = DefaultAcceptanceRadius, bool bExactSpot = false) const;

	// Shadow base class inline method to return Aeonix path target (can't override FORCEINLINE)
	FORCEINLINE FVector GetCurrentTargetLocation() const { return GetTargetLocation(); }

	// Aeonix-specific path following
	virtual void FollowAeonixPath();
	virtual FVector GetTargetLocation() const;
	virtual void UpdateMovement(float DeltaTime);
	virtual void UpdateRotation(float DeltaTime);

private:
	// Current path state
	int32 CurrentWaypointIndex;
	FVector LastVelocity;

	// Cached path reference
	const FAeonixNavigationPath* CurrentAeonixPath;

	// Prevent double-processing per frame
	bool bProcessingMovementThisFrame;
	uint64 LastProcessedFrameNumber;

	// Initialization retry system
	bool bInitializationComplete;
	float InitializationRetryTimer;
	int32 InitializationRetryCount;
	static constexpr float INITIALIZATION_RETRY_INTERVAL = 0.5f;
	static constexpr int32 MAX_INITIALIZATION_RETRIES = 10;

	// Helper functions
	const FAeonixPathPoint* GetCurrentWaypoint() const;
	const FAeonixPathPoint* GetNextWaypoint() const;
	bool IsValidWaypointIndex(int32 Index) const;
	void AdvanceToNextWaypoint();

	// Initialization helpers
	void TryInitializeNavigation();
	bool IsValidForNavigation() const;
};