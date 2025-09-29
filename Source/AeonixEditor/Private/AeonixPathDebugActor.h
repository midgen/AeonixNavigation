﻿#pragma once

#include <Runtime/Engine/Classes/GameFramework/Actor.h>

#include "AeonixPathDebugActor.generated.h"

class AAeonixBoundingVolume;
class UAeonixNavAgentComponent;

UENUM(BlueprintType)
enum class EAeonixPathDebugActorType : uint8
{
	START,
	END
};

/**
 *  Debug actor for testing Aeonix pathfinding, drop two into your level, set one to start and one to end
 */
UCLASS()
class AEONIXEDITOR_API AAeonixPathDebugActor : public AActor
{
	GENERATED_BODY()

public:
	explicit AAeonixPathDebugActor(const FObjectInitializer& Initializer);
	
	virtual void PostEditMove(bool bFinished) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void OnConstruction(const FTransform& Transform) override;
	virtual void BeginDestroy() override;
	virtual void Destroyed() override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aeonix")
	EAeonixPathDebugActorType DebugType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aeonix")
	TObjectPtr<UAeonixNavAgentComponent> NavAgentComponent;
};

