// Copyright 2024 Chris Ashworth

#include "Component/AeonixPathFollowingComponent.h"

UAeonixPathFollowingComponent::UAeonixPathFollowingComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void UAeonixPathFollowingComponent::BeginPlay()
{
	Super::BeginPlay();
}

void UAeonixPathFollowingComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

