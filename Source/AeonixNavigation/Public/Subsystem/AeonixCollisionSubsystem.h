// Copyright 2024 Chris Ashworth

#pragma once

#include <AeonixNavigation/Public/Interface/AeonixCollisionQueryInterface.h>

#include <Subsystems/WorldSubsystem.h>

#include "AeonixCollisionSubsystem.generated.h"

/**
 *   Subsystem that provides an interface for doing blocking tests in the world
 */
UCLASS()
class AEONIXNAVIGATION_API UAeonixCollisionSubsystem : public UWorldSubsystem, public IAeonixCollisionQueryInterface
{
	GENERATED_BODY()

	/* IAeonixCollisionQueryInterface BEGIN */
	virtual bool IsBlocked(const FVector& Position, const float VoxelSize, ECollisionChannel CollisionChannel, const float AgentRadius) const override;
	/* IAeonixCollisionQueryInterface END */

protected:
	virtual bool DoesSupportWorldType(const EWorldType::Type WorldType) const override;
};
