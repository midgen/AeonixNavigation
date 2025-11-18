#pragma once

#include "CoreMinimal.h"
#include "Data/AeonixLink.h"

struct FAeonixData;
struct FAeonixPathFinderSettings;

/**
 * Result data for a single pathfinding benchmark run
 */
struct AEONIXNAVIGATION_API FAeonixPathfindBenchmarkResult
{
	/** Number of A* iterations used */
	int32 Iterations = 0;

	/** Time elapsed in seconds */
	double TimeSeconds = 0.0;

	/** Whether the path was found successfully */
	bool bSuccess = false;

	/** Total path length (0 if path not found) */
	float PathLength = 0.0f;

	/** Start position used for this run */
	FVector StartPos = FVector::ZeroVector;

	/** End position used for this run */
	FVector EndPos = FVector::ZeroVector;

	/** Direct distance between start and end */
	float DirectDistance = 0.0f;
};

/**
 * Summary statistics for a complete benchmark run
 */
struct AEONIXNAVIGATION_API FAeonixPathfindBenchmarkSummary
{
	/** Random seed used for this benchmark */
	int32 Seed = 0;

	/** Total number of pathfind attempts */
	int32 TotalRuns = 0;

	/** Number of successful pathfinds */
	int32 SuccessfulRuns = 0;

	/** Number of failed pathfinds */
	int32 FailedRuns = 0;

	// Iteration statistics (for successful runs only)
	double AvgIterations = 0.0;
	int32 MinIterations = 0;
	int32 MaxIterations = 0;
	double StdDevIterations = 0.0;

	// Time statistics in milliseconds (for successful runs only)
	double AvgTimeMs = 0.0;
	double MinTimeMs = 0.0;
	double MaxTimeMs = 0.0;
	double StdDevTimeMs = 0.0;
	double TotalTimeMs = 0.0;

	// Path length statistics (for successful runs only)
	float AvgPathLength = 0.0f;
	float MinPathLength = 0.0f;
	float MaxPathLength = 0.0f;

	// Distance statistics
	float AvgDirectDistance = 0.0f;

	/** All individual results */
	TArray<FAeonixPathfindBenchmarkResult> Results;

	/** Get success rate as percentage */
	float GetSuccessRate() const
	{
		return TotalRuns > 0 ? (SuccessfulRuns * 100.0f / TotalRuns) : 0.0f;
	}

	/** Log summary to output */
	void LogSummary() const;
};

/**
 * Benchmark runner for pathfinding performance testing
 *
 * Usage:
 *   FAeonixPathfindBenchmark Benchmark;
 *   FAeonixPathfindBenchmarkSummary Summary = Benchmark.RunBenchmark(
 *       12345,      // Seed for reproducibility
 *       100,        // Number of runs
 *       NavData,    // Navigation data
 *       PathSettings // Pathfinder settings
 *   );
 *   Summary.LogSummary();
 */
class AEONIXNAVIGATION_API FAeonixPathfindBenchmark
{
public:
	/**
	 * Run the benchmark with specified parameters
	 *
	 * @param Seed Random seed for deterministic position selection
	 * @param NumRuns Number of pathfinding attempts to perform
	 * @param NavData Navigation data to use for pathfinding
	 * @param PathSettings Settings for the pathfinder
	 * @return Summary of benchmark results
	 */
	FAeonixPathfindBenchmarkSummary RunBenchmark(
		int32 Seed,
		int32 NumRuns,
		FAeonixData& NavData,
		const FAeonixPathFinderSettings& PathSettings
	);

private:
	/**
	 * Collect all navigable node links from the navigation data
	 * Only includes nodes that can be used as pathfinding targets
	 */
	void CollectNavigableNodes(FAeonixData& NavData, TArray<AeonixLink>& OutNodes);

	/**
	 * Calculate summary statistics from individual results
	 */
	void CalculateSummary(FAeonixPathfindBenchmarkSummary& Summary);
};
