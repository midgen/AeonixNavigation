#pragma once

#include <Runtime/Engine/Classes/GameFramework/Actor.h>

#include "AeonixPathDebugActor.generated.h"

class AAeonixBoundingVolume;
class UAeonixNavigationComponent;

UENUM(BlueprintType)
enum class EAeonixPathDebugActorType : uint8
{
	START,
	END
};

/**
 * 
 */
UCLASS()
class AEONIXEDITOR_API AAeonixPathDebugActor : public AActor
{
	GENERATED_BODY()

public:
	explicit AAeonixPathDebugActor(const FObjectInitializer& Initializer);
	
	virtual void PostEditMove(bool bFinished) override;
	
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aeonix")
	EAeonixPathDebugActorType DebugType;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Aeonix")
	TObjectPtr<UAeonixNavigationComponent> NavigationComponent;
};
