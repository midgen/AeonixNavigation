#include "Pathfinding/AeonixPathfindBenchmark.h"

#include "AeonixNavigation.h"
#include "Data/AeonixData.h"
#include "Data/AeonixLeafNode.h"
#include "Data/AeonixNode.h"
#include "Pathfinding/AeonixPathFinder.h"
#include "Pathfinding/AeonixNavigationPath.h"
#include "HAL/PlatformTime.h"
#include "Math/RandomStream.h"

void FAeonixPathfindBenchmarkSummary::LogSummary() const
{
	UE_LOG(LogTemp, Display, TEXT(""));
	UE_LOG(LogTemp, Display, TEXT("=== Pathfinding Benchmark Results ==="));
	UE_LOG(LogTemp, Display, TEXT("Seed: %d | Runs: %d | Success: %d (%.1f%%)"),
		Seed, TotalRuns, SuccessfulRuns, GetSuccessRate());
	UE_LOG(LogTemp, Display, TEXT(""));

	if (SuccessfulRuns > 0)
	{
		UE_LOG(LogTemp, Display, TEXT("Iterations: Avg=%.1f, Min=%d, Max=%d, StdDev=%.1f"),
			AvgIterations, MinIterations, MaxIterations, StdDevIterations);
		UE_LOG(LogTemp, Display, TEXT("Time (ms):  Avg=%.3f, Min=%.3f, Max=%.3f, StdDev=%.3f"),
			AvgTimeMs, MinTimeMs, MaxTimeMs, StdDevTimeMs);
		UE_LOG(LogTemp, Display, TEXT("Path Length: Avg=%.1f, Min=%.1f, Max=%.1f"),
			AvgPathLength, MinPathLength, MaxPathLength);
		UE_LOG(LogTemp, Display, TEXT("Avg Direct Distance: %.1f"),
			AvgDirectDistance);
	}
	else
	{
		UE_LOG(LogTemp, Display, TEXT("No successful pathfinds to report statistics"));
	}

	UE_LOG(LogTemp, Display, TEXT(""));
	UE_LOG(LogTemp, Display, TEXT("Total benchmark time: %.1fms"), TotalTimeMs);
	UE_LOG(LogTemp, Display, TEXT("====================================="));
	UE_LOG(LogTemp, Display, TEXT(""));
}

FAeonixPathfindBenchmarkSummary FAeonixPathfindBenchmark::RunBenchmark(
	int32 Seed,
	int32 NumRuns,
	FAeonixData& NavData,
	const FAeonixPathFinderSettings& PathSettings)
{
	FAeonixPathfindBenchmarkSummary Summary;
	Summary.Seed = Seed;
	Summary.TotalRuns = NumRuns;
	Summary.Results.Reserve(NumRuns);

	// Collect all navigable nodes
	TArray<AeonixLink> NavigableNodes;
	CollectNavigableNodes(NavData, NavigableNodes);

	if (NavigableNodes.Num() < 2)
	{
		UE_LOG(LogTemp, Error, TEXT("Benchmark failed: Need at least 2 navigable nodes, found %d"),
			NavigableNodes.Num());
		return Summary;
	}

	UE_LOG(LogTemp, Display, TEXT("Benchmark: Found %d navigable nodes"), NavigableNodes.Num());

	// Initialize random stream with seed
	FRandomStream RandomStream(Seed);

	// Create pathfinder
	AeonixPathFinder PathFinder(NavData, PathSettings);

	// Track total benchmark time
	double BenchmarkStartTime = FPlatformTime::Seconds();

	// Run benchmark
	for (int32 i = 0; i < NumRuns; ++i)
	{
		FAeonixPathfindBenchmarkResult Result;

		// Randomly select start and end nodes (ensure they're different)
		int32 StartIdx = RandomStream.RandRange(0, NavigableNodes.Num() - 1);
		int32 EndIdx = RandomStream.RandRange(0, NavigableNodes.Num() - 1);

		// Ensure different nodes
		while (EndIdx == StartIdx && NavigableNodes.Num() > 1)
		{
			EndIdx = RandomStream.RandRange(0, NavigableNodes.Num() - 1);
		}

		AeonixLink StartLink = NavigableNodes[StartIdx];
		AeonixLink EndLink = NavigableNodes[EndIdx];

		// Get positions
		NavData.GetLinkPosition(StartLink, Result.StartPos);
		NavData.GetLinkPosition(EndLink, Result.EndPos);
		Result.DirectDistance = FVector::Dist(Result.StartPos, Result.EndPos);

		// Time the pathfinding
		FAeonixNavigationPath Path;
		double StartTime = FPlatformTime::Seconds();

		Result.bSuccess = PathFinder.FindPath(StartLink, EndLink, Result.StartPos, Result.EndPos, Path);

		double EndTime = FPlatformTime::Seconds();
		Result.TimeSeconds = EndTime - StartTime;
		Result.Iterations = PathFinder.GetLastIterationCount();

		// Calculate path length if successful
		if (Result.bSuccess)
		{
			const TArray<FAeonixPathPoint>& PathPoints = Path.GetPathPoints();
			Result.PathLength = 0.0f;
			for (int32 j = 1; j < PathPoints.Num(); ++j)
			{
				Result.PathLength += FVector::Dist(PathPoints[j-1].Position, PathPoints[j].Position);
			}
			Summary.SuccessfulRuns++;
		}
		else
		{
			Summary.FailedRuns++;
		}

		Summary.Results.Add(Result);
	}

	double BenchmarkEndTime = FPlatformTime::Seconds();
	Summary.TotalTimeMs = (BenchmarkEndTime - BenchmarkStartTime) * 1000.0;

	// Calculate summary statistics
	CalculateSummary(Summary);

	return Summary;
}

void FAeonixPathfindBenchmark::CollectNavigableNodes(FAeonixData& NavData, TArray<AeonixLink>& OutNodes)
{
	OutNodes.Empty();

	// For Layer 0 nodes, we need to check leaf data to find actually navigable nodes
	const TArray<AeonixNode>& Layer0 = NavData.OctreeData.GetLayer(0);

	for (int32 NodeIdx = 0; NodeIdx < Layer0.Num(); ++NodeIdx)
	{
		const AeonixNode& Node = Layer0[NodeIdx];

		if (Node.FirstChild.IsValid())
		{
			const AeonixLeafNode& Leaf = NavData.OctreeData.GetLeafNode(Node.FirstChild.GetNodeIndex());

			// Skip completely blocked nodes
			if (Leaf.IsCompletelyBlocked())
			{
				continue;
			}

			if (Leaf.IsEmpty())
			{
				// Empty leaf - any subnode is navigable, use 0
				OutNodes.Add(AeonixLink(0, NodeIdx, 0));
			}
			else
			{
				// Partial leaf - find an unblocked subnode
				for (int32 SubIdx = 0; SubIdx < 64; ++SubIdx)
				{
					if (!Leaf.GetNode(SubIdx))
					{
						OutNodes.Add(AeonixLink(0, NodeIdx, SubIdx));
						break;
					}
				}
			}
		}
		else
		{
			// No leaf data - treat as fully navigable
			OutNodes.Add(AeonixLink(0, NodeIdx, 0));
		}
	}

	// Also include higher layer leaf nodes (nodes without children)
	for (int32 LayerIdx = 1; LayerIdx < NavData.OctreeData.NumLayers; ++LayerIdx)
	{
		const TArray<AeonixNode>& Layer = NavData.OctreeData.GetLayer(LayerIdx);

		for (int32 NodeIdx = 0; NodeIdx < Layer.Num(); ++NodeIdx)
		{
			const AeonixNode& Node = Layer[NodeIdx];

			// Only include leaf nodes (nodes without children)
			if (!Node.HasChildren())
			{
				OutNodes.Add(AeonixLink(LayerIdx, NodeIdx, 0));
			}
		}
	}
}

void FAeonixPathfindBenchmark::CalculateSummary(FAeonixPathfindBenchmarkSummary& Summary)
{
	if (Summary.SuccessfulRuns == 0)
	{
		return;
	}

	// Initialize min/max
	Summary.MinIterations = INT32_MAX;
	Summary.MaxIterations = 0;
	Summary.MinTimeMs = DBL_MAX;
	Summary.MaxTimeMs = 0.0;
	Summary.MinPathLength = FLT_MAX;
	Summary.MaxPathLength = 0.0f;

	// Calculate sums for averages
	double SumIterations = 0.0;
	double SumTimeMs = 0.0;
	double SumPathLength = 0.0;
	double SumDirectDistance = 0.0;

	for (const FAeonixPathfindBenchmarkResult& Result : Summary.Results)
	{
		if (!Result.bSuccess)
		{
			continue;
		}

		double TimeMs = Result.TimeSeconds * 1000.0;

		// Update sums
		SumIterations += Result.Iterations;
		SumTimeMs += TimeMs;
		SumPathLength += Result.PathLength;
		SumDirectDistance += Result.DirectDistance;

		// Update min/max iterations
		if (Result.Iterations < Summary.MinIterations)
		{
			Summary.MinIterations = Result.Iterations;
		}
		if (Result.Iterations > Summary.MaxIterations)
		{
			Summary.MaxIterations = Result.Iterations;
		}

		// Update min/max time
		if (TimeMs < Summary.MinTimeMs)
		{
			Summary.MinTimeMs = TimeMs;
		}
		if (TimeMs > Summary.MaxTimeMs)
		{
			Summary.MaxTimeMs = TimeMs;
		}

		// Update min/max path length
		if (Result.PathLength < Summary.MinPathLength)
		{
			Summary.MinPathLength = Result.PathLength;
		}
		if (Result.PathLength > Summary.MaxPathLength)
		{
			Summary.MaxPathLength = Result.PathLength;
		}
	}

	// Calculate averages
	double N = static_cast<double>(Summary.SuccessfulRuns);
	Summary.AvgIterations = SumIterations / N;
	Summary.AvgTimeMs = SumTimeMs / N;
	Summary.AvgPathLength = static_cast<float>(SumPathLength / N);
	Summary.AvgDirectDistance = static_cast<float>(SumDirectDistance / N);

	// Calculate standard deviations
	double SumSqDiffIterations = 0.0;
	double SumSqDiffTimeMs = 0.0;

	for (const FAeonixPathfindBenchmarkResult& Result : Summary.Results)
	{
		if (!Result.bSuccess)
		{
			continue;
		}

		double TimeMs = Result.TimeSeconds * 1000.0;

		double DiffIterations = Result.Iterations - Summary.AvgIterations;
		double DiffTimeMs = TimeMs - Summary.AvgTimeMs;

		SumSqDiffIterations += DiffIterations * DiffIterations;
		SumSqDiffTimeMs += DiffTimeMs * DiffTimeMs;
	}

	Summary.StdDevIterations = FMath::Sqrt(SumSqDiffIterations / N);
	Summary.StdDevTimeMs = FMath::Sqrt(SumSqDiffTimeMs / N);
}
