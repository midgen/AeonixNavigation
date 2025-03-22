#pragma once

#include <AeonixNavigation/Public/Pathfinding/AeonixPathFinder.h>
#include <AeonixNavigation/Public/Data/AeonixLink.h>
#include <AeonixNavigation/Public/Pathfinding/AeonixNavigationPath.h>
#include <AeonixNavigation/Public/Interface/AeonixSubsystemInterface.h>
#include <AeonixNavigation/Public/Data/AeonixTypes.h>

#include <Runtime/Engine/Classes/Components/ActorComponent.h>

#include "AeonixNavAgentComponent.generated.h"

class AAeonixBoundingVolume;
struct AeonixLink;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class AEONIXNAVIGATION_API UAeonixNavAgentComponent : public UActorComponent
{
	GENERATED_BODY()

	UAeonixNavAgentComponent(const FObjectInitializer& ObjectInitializer);
	~UAeonixNavAgentComponent();

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aeonix")
	FAeonixPathFinderSettings PathfinderSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aeonix | Debug")
	bool bDebugPrintCurrentPosition{false};
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aeonix | Debug")
	bool bDebugPrintMortonCodes{false};
	
	FAeonixNavigationPath& GetPath() { return CurrentPath; }
	FVector GetAgentPosition() const;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
	UPROPERTY()
	TScriptInterface<IAeonixSubsystemInterface> AeonixSubsystem;

protected:
	FAeonixNavigationPath CurrentPath{};
};