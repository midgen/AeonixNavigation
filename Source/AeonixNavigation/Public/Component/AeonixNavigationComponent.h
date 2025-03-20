#pragma once

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
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SVO Navigation | Debug")
	bool bDebugPrintCurrentPosition;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SVO Navigation | Debug")
	bool bDebugPrintMortonCodes;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SVO Navigation | Debug")
	bool bDebugDrawOpenNodes = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SVO Navigation | Heuristics")
	bool bUseUnitCost = false;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SVO Navigation | Heuristics")
	float UnitCost = 10.f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SVO Navigation | Heuristics")
	float EstimateWeight = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SVO Navigation | Heuristics")
	float NodeSizeCompensation = 1.0f;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SVO Navigation | Heuristics")
	EAeonixPathCostType PathCostType = EAeonixPathCostType::EUCLIDEAN;
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SVO Navigation | Smoothing")
	int SmoothingIterations = 0;

	const AAeonixBoundingVolume* GetCurrentVolume() const { return CurrentNavVolume; }

	AeonixLink GetNavPosition() const;
	virtual FVector GetPawnPosition() const;
	FAeonixNavPathSharedPtr& GetPath() { return CurrentPath; }
	void SetCurrentNavVolume(const AAeonixBoundingVolume* Volume);

	bool FindPathAsync(const FVector& aStartPosition, const FVector& aTargetPosition, FThreadSafeBool& aCompleteFlag, FAeonixNavPathSharedPtr* oNavPath);
	bool FindPathImmediate(const FVector& aStartPosition, const FVector& aTargetPosition, FAeonixNavPathSharedPtr* oNavPath);
	UFUNCTION(BlueprintCallable, Category = Aeonix)
	void FindPathImmediate(const FVector& aStartPosition, const FVector& aTargetPosition, TArray<FVector>& OutPathPoints);
	// The current navigation volume that the owned pawn is inside, null if not inside a volume
	UPROPERTY()
	const AAeonixBoundingVolume* CurrentNavVolume;
	
protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	
	UPROPERTY()
	TScriptInterface<IAeonixSubsystemInterface> AeonixSubsystem;

	bool HasNavData() const;

	// Print current layer/morton code information
	void DebugLocalPosition(FVector& aPosition);

protected:
	FAeonixNavPathSharedPtr CurrentPath;
};