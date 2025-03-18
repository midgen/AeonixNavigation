#pragma once

#include "AeonixCollisionQueryInterface.generated.h"

UINTERFACE(MinimalAPI, Blueprintable)
class UAeonixCollisionQueryInterface : public UInterface
{
	GENERATED_BODY()
};

class IAeonixCollisionQueryInterface
{
	GENERATED_BODY()

public:
	/** Add interface function declarations here */
	virtual bool IsBlocked(const FVector& Position, const float VoxelSize, ECollisionChannel CollisionChannel, const float AgentRadius) const = 0;
};