#pragma once

#include <AeonixNavigation/Public/Pathfinding/AeonixPathFinder.h>
#include <AeonixNavigation/Public/Data/AeonixLink.h>
#include <AeonixNavigation/Public/Pathfinding/AeonixNavigationPath.h>
#include <AeonixNavigation/Public/Interface/AeonixSubsystemInterface.h>
#include <AeonixNavigation/Public/Data/AeonixTypes.h>

#include <Runtime/Engine/Classes/Components/ActorComponent.h>

#include "AeonixNavigationComponent.generated.h"

class AAeonixBoundingVolume;
struct AeonixLink;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class AEONIXNAVIGATION_API UAeonixNavigationComponent : public UActorComponent
{
	GENERATED_BODY()

	UAeonixNavigationComponent(const FObjectInitializer& ObjectInitializer);

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aeonix")
	FAeonixPathFinderSettings PathfinderSettings;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aeonix | Debug")
	bool bDebugPrintCurrentPosition{false};
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aeonix | Debug")
	bool bDebugPrintMortonCodes{false};
	
	// virtual FVector GetPawnPosition() const;
	FAeonixNavigationPath& GetPath() { return CurrentPath; }
	FVector GetPawnPosition() const;

	//bool FindPathAsync(const FVector& aStartPosition, const FVector& aTargetPosition, FThreadSafeBool& aCompleteFlag, FAeonixNavPathSharedPtr* oNavPath);
	//bool FindPathImmediate(const FVector& aStartPosition, const FVector& aTargetPosition, FAeonixNavPathSharedPtr* oNavPath);
	//UFUNCTION(BlueprintCallable, Category = Aeonix)
	//void FindPathImmediate(const FVector& aStartPosition, const FVector& aTargetPosition, TArray<FVector>& OutPathPoints);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UPROPERTY()
	TScriptInterface<IAeonixSubsystemInterface> AeonixSubsystem;

protected:
	FAeonixNavigationPath CurrentPath{};
};