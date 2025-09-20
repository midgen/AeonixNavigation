#pragma once

#include "CoreMinimal.h"
#include "AeonixPerformanceTypes.generated.h"

class AAeonixBoundingVolume;

UENUM(BlueprintType)
enum class EAeonixPerformanceTestStatus : uint8
{
	NotStarted,
	Running,
	Completed,
	Failed,
	Cancelled
};

USTRUCT(BlueprintType)
struct AEONIXEDITOR_API FAeonixPerformanceTestSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Test Configuration")
	int32 NumberOfTests = 100;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Test Configuration")
	int32 RandomSeed = 12345;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Test Configuration")
	float MinPathDistance = 100.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Test Configuration")
	float MaxPathDistance = 2000.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Test Configuration")
	bool bVisualizeResults = true;

};

USTRUCT(BlueprintType)
struct AEONIXEDITOR_API FAeonixPerformanceTestResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Results")
	FVector StartPosition = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Results")
	FVector EndPosition = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Results")
	bool bPathFound = false;

	UPROPERTY(BlueprintReadOnly, Category = "Results")
	float PathfindingTime = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Results")
	float PathLength = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Results")
	int32 NodesExplored = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Results")
	int32 PathPoints = 0;
};

USTRUCT(BlueprintType)
struct AEONIXEDITOR_API FAeonixPerformanceTestSummary
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Summary")
	int32 TotalTests = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Summary")
	int32 SuccessfulPaths = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Summary")
	int32 FailedPaths = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Summary")
	float SuccessRate = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Summary")
	float MinPathfindingTime = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Summary")
	float MaxPathfindingTime = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Summary")
	float AveragePathfindingTime = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Summary")
	float MedianPathfindingTime = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Summary")
	float AveragePathLength = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Summary")
	float AverageNodesExplored = 0.0f;

	UPROPERTY(BlueprintReadOnly, Category = "Summary")
	float TotalTestTime = 0.0f;

	TArray<FAeonixPerformanceTestResult> Results;

	void CalculateSummary()
	{
		TotalTests = Results.Num();
		SuccessfulPaths = 0;
		FailedPaths = 0;

		if (TotalTests == 0)
		{
			return;
		}

		TArray<float> PathfindingTimes;
		float TotalPathfindingTime = 0.0f;
		float TotalPathLength = 0.0f;
		float TotalNodesExplored = 0.0f;

		for (const FAeonixPerformanceTestResult& Result : Results)
		{
			if (Result.bPathFound)
			{
				SuccessfulPaths++;
				PathfindingTimes.Add(Result.PathfindingTime);
				TotalPathfindingTime += Result.PathfindingTime;
				TotalPathLength += Result.PathLength;
				TotalNodesExplored += Result.NodesExplored;
			}
			else
			{
				FailedPaths++;
			}
		}

		SuccessRate = (float)SuccessfulPaths / (float)TotalTests * 100.0f;

		if (SuccessfulPaths > 0)
		{
			AveragePathfindingTime = TotalPathfindingTime / SuccessfulPaths;
			AveragePathLength = TotalPathLength / SuccessfulPaths;
			AverageNodesExplored = TotalNodesExplored / SuccessfulPaths;

			PathfindingTimes.Sort();
			MinPathfindingTime = PathfindingTimes[0];
			MaxPathfindingTime = PathfindingTimes.Last();

			if (PathfindingTimes.Num() % 2 == 0)
			{
				int32 Mid = PathfindingTimes.Num() / 2;
				MedianPathfindingTime = (PathfindingTimes[Mid - 1] + PathfindingTimes[Mid]) * 0.5f;
			}
			else
			{
				MedianPathfindingTime = PathfindingTimes[PathfindingTimes.Num() / 2];
			}
		}
	}
};