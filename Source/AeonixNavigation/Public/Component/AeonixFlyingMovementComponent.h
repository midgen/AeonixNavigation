// Copyright 2024 Chris Ashworth

#pragma once

#include "GameFramework/PawnMovementComponent.h"
#include "CoreMinimal.h"
#include "AeonixFlyingMovementComponent.generated.h"

USTRUCT(BlueprintType)
struct AEONIXNAVIGATION_API FAeonixFlyingSettings
{
	GENERATED_BODY()

	// Maximum flight speed in units per second
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flying Movement")
	float MaxSpeed = 150.f;

	// Acceleration when input is applied (units/sec^2)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flying Movement", meta=(ClampMin="0.0"))
	float Acceleration = 100.0f;

	// Deceleration when no input / stopping (units/sec^2)
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flying Movement", meta=(ClampMin="0.0"))
	float Deceleration = 100.0f;

	// Rotation speed in degrees per second
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Flying Movement", meta=(ClampMin="0.0"))
	float TurnRate = 180.0f;
};

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class AEONIXNAVIGATION_API UAeonixFlyingMovementComponent : public UPawnMovementComponent
{
	GENERATED_BODY()

public:
	UAeonixFlyingMovementComponent();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aeonix Flying Movement")
	FAeonixFlyingSettings FlyingSettings;

	// Movement interface
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
	virtual void RequestDirectMove(const FVector& MoveVelocity, bool bForceMaxSpeed) override;
	virtual bool CanStartPathFollowing() const override;
	virtual void StopMovementImmediately() override;

	UFUNCTION(BlueprintCallable, Category = "Aeonix Flying Movement")
	FVector GetCurrentVelocity() const { return Velocity; }

	UFUNCTION(BlueprintCallable, Category = "Aeonix Flying Movement")
	float GetCurrentSpeed() const { return Velocity.Size(); }

private:
	// Target velocity from path following (we accelerate toward this)
	FVector TargetVelocity = FVector::ZeroVector;
};