#pragma once

#include "AeonixDebugDrawInterface.generated.h"

UINTERFACE(MinimalAPI)
class UAeonixDebugDrawInterface : public UInterface
{
	GENERATED_BODY()
};

class AEONIXNAVIGATION_API IAeonixDebugDrawInterface
{
	GENERATED_BODY()

public:
	/** Add interface function declarations here */
	
	virtual void AeonixDrawDebugString(const FVector& Position, const FString& String, const FColor& Color) const = 0;
	virtual void AeonixDrawDebugBox(const FVector& Position, const float Size, const FColor& Color) const = 0;
};