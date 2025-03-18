// Copyright 2024 Chris Ashworth

#pragma once

#include <Navigation/PathFollowingComponent.h>

#include "AeonixPathFollowingComponent.generated.h"

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class AEONIXNAVIGATION_API UAeonixPathFollowingComponent : public UPathFollowingComponent
{
	GENERATED_BODY()

public:
	UAeonixPathFollowingComponent();
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

protected:
	virtual void BeginPlay() override;


};