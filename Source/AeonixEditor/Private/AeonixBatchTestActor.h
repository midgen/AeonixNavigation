#pragma once

#include "CoreMinimal.h"
#include "Runtime/Engine/Classes/GameFramework/Actor.h"
#include "../Public/AeonixPerformanceTypes.h"
#include "AeonixNavigation/Public/Pathfinding/AeonixNavigationPath.h"
#include "AeonixBatchTestActor.generated.h"	

class AAeonixBoundingVolume;
class UAeonixNavAgentComponent;
class UAeonixSubsystem;

UCLASS()
class AEONIXEDITOR_API AAeonixBatchTestActor : public AActor
{
	GENERATED_BODY()

public:
	AAeonixBatchTestActor();

	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;

#if WITH_EDITOR
	virtual void PostEditMove(bool bFinished) override;
	virtual void OnConstruction(const FTransform& Transform) override;
#endif

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Aeonix Navigation")
	FAeonixPerformanceTestSettings TestSettings;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aeonix Navigation")
	EAeonixPerformanceTestStatus CurrentStatus = EAeonixPerformanceTestStatus::NotStarted;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aeonix Navigation")
	FAeonixPerformanceTestSummary LastTestSummary;

	UFUNCTION(BlueprintCallable, Category = "Aeonix Navigation")
	void StartBatchTest();

	UFUNCTION(BlueprintCallable, Category = "Aeonix Navigation")
	void CancelBatchTest();

	UFUNCTION(BlueprintCallable, Category = "Aeonix Navigation")
	void ClearResults();

	UFUNCTION(CallInEditor, Category = "Aeonix Navigation")
	void RunTestInEditor();

	UFUNCTION(CallInEditor, Category = "Aeonix Navigation")
	void ClearResultsInEditor();

protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Aeonix Navigation")
	TObjectPtr<UAeonixNavAgentComponent> NavAgentComponent;

	UPROPERTY(Transient)
	FRandomStream RandomStream;

	UPROPERTY(Transient)
	int32 CurrentTestIndex = 0;

	UPROPERTY(Transient)
	float TestStartTime = 0.0f;

	UPROPERTY(Transient)
	TArray<FAeonixPerformanceTestResult> CurrentResults;

	UPROPERTY(Transient)
	TObjectPtr<UAeonixSubsystem> AeonixSubsystem;

	virtual void RunSynchronousTests();

	virtual bool GenerateRandomNavigablePoint(const AAeonixBoundingVolume* Volume, FVector& OutPoint);
	virtual bool GenerateRandomEndPoint(FVector& OutEnd);

	virtual void ExecuteSingleTest(const FVector& Start, const FVector& End, FAeonixPerformanceTestResult& OutResult);

	virtual void OnTestCompleted();

	virtual void VisualizeTestResults();
	virtual void ClearVisualization();

private:
	TArray<FVector> VisualizationPoints;
	TArray<FAeonixNavigationPath> VisualizationPaths;
	AAeonixBoundingVolume* GetTargetVolume() const;
	TArray<AAeonixBoundingVolume*> GetAllVolumes() const;
	void InitializeSubsystemIfNeeded();
};