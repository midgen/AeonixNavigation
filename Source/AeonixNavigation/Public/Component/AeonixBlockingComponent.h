// Copyright 2024 Chris Ashworth

#pragma once

#include <Runtime/Engine/Classes/Components/ActorComponent.h>

#include "AeonixBlockingComponent.generated.h"

UCLASS(ClassGroup=(Custom), meta=(BlueprintSpawnableComponent))
class AEONIXNAVIGATION_API UAeonixBlockingComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	// Sets default values for this component's properties
	UAeonixBlockingComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
};