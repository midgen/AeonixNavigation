// Copyright 2024 Chris Ashworth

#pragma once

#include <AeonixNavigation/Public/Interface/AeonixCollisionQueryInterface.h>

#include <Subsystems/EngineSubsystem.h>

#include "AeonixCollisionSubsystem.generated.h"

/**
 *   Subsystem that provides an interface for doing blocking tests in the world
 */
UCLASS()
class AEONIXNAVIGATION_API UAeonixCollisionSubsystem : public UEngineSubsystem, public IAeonixCollisionQueryInterface
{
	GENERATED_BODY()

	/* IAeonixCollisionQueryInterface BEGIN */
	virtual bool IsBlocked(const FVector& Position, const float VoxelSize, ECollisionChannel CollisionChannel, const float AgentRadius) const override;
	/* IAeonixCollisionQueryInterface END */

	UPROPERTY()
	TObjectPtr<UWorld> World{nullptr};

public:
	void SetWorld(UWorld* InWorld) {World = InWorld;} 
};
