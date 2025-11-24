#include "Data/AeonixData.h"
#include "Data/AeonixLeafNode.h"
#include "Data/AeonixNode.h"
#include "Pathfinding/AeonixPathFinder.h"
#include "Pathfinding/AeonixPathfindBenchmark.h"
#include "Engine/World.h"
#include "Engine/EngineTypes.h"
#include "Misc/AutomationTest.h"
#include "../Public/AeonixNavigationTestMocks.h"

/**
 * Benchmark test for pathfinding performance
 *
 * This test provides deterministic performance metrics that can be used to:
 * - Establish baseline performance before optimizations
 * - Verify performance improvements after changes
 * - Detect performance regressions
 *
 * Run with: UnrealEditor-Cmd.exe <project> -ExecCmds="Automation RunTests AeonixNavigation.Benchmark"
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAeonixNavigation_BenchmarkTest,
    "AeonixNavigation.Benchmark.Pathfinding",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAeonixNavigation_BenchmarkTest::RunTest(const FString& Parameters)
{
    // Benchmark configuration
    const int32 BenchmarkSeed = 12345;
    const int32 NumRuns = 100;

    UE_LOG(LogTemp, Display, TEXT(""));
    UE_LOG(LogTemp, Display, TEXT("========================================"));
    UE_LOG(LogTemp, Display, TEXT("  Aeonix Pathfinding Benchmark Test"));
    UE_LOG(LogTemp, Display, TEXT("========================================"));
    UE_LOG(LogTemp, Display, TEXT(""));

    // Setup collision interface with obstacles to create interesting pathfinding scenarios
    FTestPartialObstacleCollisionQueryInterface ObstacleCollision;
    FTestDebugDrawInterface DebugDraw;
    FAeonixData NavData;

    // Setup generation parameters
    // Use a reasonably sized volume with enough voxels for interesting paths
    FAeonixGenerationParameters Params;
    Params.Origin = FVector::ZeroVector;
    Params.Extents = FVector(500, 500, 500); // 1000x1000x1000 volume
    Params.OctreeDepth = 5; // Higher resolution for more leaf nodes
    Params.CollisionChannel = ECollisionChannel::ECC_WorldStatic;
    Params.AgentRadius = 34.f;
    Params.ShowLeafVoxels = false; // Don't need debug visualization
    Params.ShowMortonCodes = false;

    NavData.UpdateGenerationParameters(Params);

    // Generate navigation data
    UE_LOG(LogTemp, Display, TEXT("Generating navigation data..."));
    UWorld* DummyWorld = nullptr;
    NavData.Generate(*DummyWorld, ObstacleCollision, DebugDraw);

    UE_LOG(LogTemp, Display, TEXT("Navigation data generated:"));
    UE_LOG(LogTemp, Display, TEXT("  Total voxels: %d"), DebugDraw.TotalVoxelCount);
    UE_LOG(LogTemp, Display, TEXT("  Blocked voxels: %d"), DebugDraw.BlockedVoxelCount);
    UE_LOG(LogTemp, Display, TEXT(""));

    // Setup pathfinder settings
    FAeonixPathFinderSettings PathSettings;
    PathSettings.MaxIterations = 10000;
    PathSettings.bUseUnitCost = false;
    PathSettings.bOptimizePath = true;
    PathSettings.bUseStringPulling = false; // Disable for raw benchmark
    PathSettings.bSmoothPositions = false;
    PathSettings.HeuristicSettings.EuclideanWeight = 1.0f;
    PathSettings.HeuristicSettings.GlobalWeight = 10.0f;
    PathSettings.HeuristicSettings.NodeSizeWeight = 1.0f;

    // Run benchmark
    UE_LOG(LogTemp, Display, TEXT("Running benchmark with seed %d, %d runs..."), BenchmarkSeed, NumRuns);

    FAeonixPathfindBenchmark Benchmark;
    FAeonixPathfindBenchmarkSummary Summary = Benchmark.RunBenchmark(
        BenchmarkSeed,
        NumRuns,
        NavData,
        PathSettings
    );

    // Log results (use AddInfo so it shows even when test passes)
    Summary.LogSummary();

    // Add key metrics to test output (always visible)
    AddInfo(FString::Printf(TEXT("=== WALL OBSTACLES BENCHMARK ===")));
    AddInfo(FString::Printf(TEXT("Tests pathfinding with continuous wall obstacles (gap in middle).")));
    AddInfo(FString::Printf(TEXT("Lower success rate expected due to walls creating unreachable regions.")));
    AddInfo(FString::Printf(TEXT("High iteration counts on failures = exhaustive search (no path exists).")));
    AddInfo(FString::Printf(TEXT("")));
    AddInfo(FString::Printf(TEXT("Seed: %d | Runs: %d | Success: %d (%.1f%%)"),
        Summary.Seed, Summary.TotalRuns, Summary.SuccessfulRuns, Summary.GetSuccessRate()));
    if (Summary.SuccessfulRuns > 0)
    {
        AddInfo(FString::Printf(TEXT("Iterations: Avg=%.1f, Min=%d, Max=%d, StdDev=%.1f"),
            Summary.AvgIterations, Summary.MinIterations, Summary.MaxIterations, Summary.StdDevIterations));
        AddInfo(FString::Printf(TEXT("Time (ms): Avg=%.3f, Min=%.3f, Max=%.3f, StdDev=%.3f"),
            Summary.AvgTimeMs, Summary.MinTimeMs, Summary.MaxTimeMs, Summary.StdDevTimeMs));
        AddInfo(FString::Printf(TEXT("Path Length: Avg=%.1f | Direct Distance: Avg=%.1f"),
            Summary.AvgPathLength, Summary.AvgDirectDistance));
        AddInfo(FString::Printf(TEXT("Total benchmark time: %.1fms"), Summary.TotalTimeMs));
    }

    // Validate benchmark ran successfully
    TestTrue(TEXT("Benchmark should complete all runs"), Summary.TotalRuns == NumRuns);
    TestTrue(TEXT("Benchmark should have at least some successful pathfinds"), Summary.SuccessfulRuns > 0);

    // Log some individual results for debugging
    UE_LOG(LogTemp, Display, TEXT("Sample individual results (first 5):"));
    for (int32 i = 0; i < FMath::Min(5, Summary.Results.Num()); ++i)
    {
        const FAeonixPathfindBenchmarkResult& Result = Summary.Results[i];
        UE_LOG(LogTemp, Display, TEXT("  Run %d: %s, %d iterations, %.3fms, path=%.1f, direct=%.1f"),
            i + 1,
            Result.bSuccess ? TEXT("SUCCESS") : TEXT("FAIL"),
            Result.Iterations,
            Result.TimeSeconds * 1000.0,
            Result.PathLength,
            Result.DirectDistance);
    }

    // Performance assertions (these are sanity checks, not strict requirements)
    // You can adjust these based on your baseline measurements
    if (Summary.SuccessfulRuns > 0)
    {
        // Sanity check: average iterations should be reasonable
        TestTrue(TEXT("Average iterations should be less than max iterations"),
            Summary.AvgIterations < PathSettings.MaxIterations);

        // Sanity check: success rate should be reasonable for our test scenario
        TestTrue(TEXT("Success rate should be at least 50%"),
            Summary.GetSuccessRate() >= 50.0f);

        UE_LOG(LogTemp, Display, TEXT(""));
        UE_LOG(LogTemp, Display, TEXT("Benchmark completed successfully!"));
        UE_LOG(LogTemp, Display, TEXT("Use these baseline numbers to measure optimization impact."));
    }
    else
    {
        AddError(TEXT("Benchmark failed: No successful pathfinds"));
        return false;
    }

    UE_LOG(LogTemp, Display, TEXT(""));
    UE_LOG(LogTemp, Display, TEXT("========================================"));
    UE_LOG(LogTemp, Display, TEXT(""));

    return true;
}

/**
 * Additional benchmark test with sparse obstacles
 * Creates many empty leaves while still having octree subdivision
 * This should show the benefit of empty leaf optimization
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAeonixNavigation_BenchmarkEmptySpaceTest,
    "AeonixNavigation.Benchmark.EmptySpace",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAeonixNavigation_BenchmarkEmptySpaceTest::RunTest(const FString& Parameters)
{
    const int32 BenchmarkSeed = 12345;
    const int32 NumRuns = 100;

    UE_LOG(LogTemp, Display, TEXT(""));
    UE_LOG(LogTemp, Display, TEXT("========================================"));
    UE_LOG(LogTemp, Display, TEXT("  Scattered Obstacles Benchmark"));
    UE_LOG(LogTemp, Display, TEXT("========================================"));
    UE_LOG(LogTemp, Display, TEXT(""));

    // Collision interface with scattered box obstacles that don't completely block paths
    class FScatteredObstaclesCollisionInterface : public IAeonixCollisionQueryInterface
    {
    public:
        // Multiple small obstacles scattered throughout the volume
        // Sized and positioned to force subdivision but not block all paths
        virtual bool IsBlocked(const FVector& Position, const float VoxelSize, ECollisionChannel CollisionChannel, const float AgentRadius) const override
        {
            // Array of scattered obstacle positions and sizes
            // These are small enough to navigate around
            struct FObstacle
            {
                FVector Center;
                float HalfExtent;
            };

            static const FObstacle Obstacles[] = {
                // Central cluster
                { FVector(0, 0, 0), 40.0f },
                { FVector(100, 100, 0), 30.0f },
                { FVector(-100, -100, 0), 30.0f },
                { FVector(100, -100, 0), 30.0f },
                { FVector(-100, 100, 0), 30.0f },

                // Mid-range obstacles
                { FVector(200, 0, 100), 35.0f },
                { FVector(-200, 0, -100), 35.0f },
                { FVector(0, 200, 100), 35.0f },
                { FVector(0, -200, -100), 35.0f },

                // Outer obstacles
                { FVector(300, 300, 0), 40.0f },
                { FVector(-300, -300, 0), 40.0f },
                { FVector(300, -300, 0), 40.0f },
                { FVector(-300, 300, 0), 40.0f },

                // Vertical spread
                { FVector(150, 0, 200), 30.0f },
                { FVector(-150, 0, -200), 30.0f },
                { FVector(0, 150, 200), 30.0f },
                { FVector(0, -150, -200), 30.0f },
            };

            const int32 NumObstacles = sizeof(Obstacles) / sizeof(Obstacles[0]);

            for (int32 i = 0; i < NumObstacles; ++i)
            {
                const FObstacle& Obs = Obstacles[i];
                // Box collision check
                if (FMath::Abs(Position.X - Obs.Center.X) < (Obs.HalfExtent + VoxelSize) &&
                    FMath::Abs(Position.Y - Obs.Center.Y) < (Obs.HalfExtent + VoxelSize) &&
                    FMath::Abs(Position.Z - Obs.Center.Z) < (Obs.HalfExtent + VoxelSize))
                {
                    return true;
                }
            }

            return false;
        }

        virtual bool IsLeafBlocked(const FVector& Position, const float LeafSize, ECollisionChannel CollisionChannel, const float AgentRadius) const override
        {
            return IsBlocked(Position, LeafSize, CollisionChannel, AgentRadius);
        }
    };

    FScatteredObstaclesCollisionInterface ScatteredCollision;
    FTestDebugDrawInterface DebugDraw;
    FAeonixData NavData;

    FAeonixGenerationParameters Params;
    Params.Origin = FVector::ZeroVector;
    Params.Extents = FVector(500, 500, 500);
    Params.OctreeDepth = 5; // Higher resolution for more leaf nodes and empty leaves
    Params.CollisionChannel = ECollisionChannel::ECC_WorldStatic;
    Params.AgentRadius = 34.f;

    NavData.UpdateGenerationParameters(Params);

    UE_LOG(LogTemp, Display, TEXT("Generating navigation data with scattered obstacles..."));
    UWorld* DummyWorld = nullptr;
    NavData.Generate(*DummyWorld, ScatteredCollision, DebugDraw);

    // Count empty leaves to verify optimization opportunity
    int32 TotalLeaves = 0;
    int32 EmptyLeaves = 0;
    int32 PartialLeaves = 0;
    const TArray<AeonixNode>& Layer0 = NavData.OctreeData.GetLayer(0);
    for (const AeonixNode& Node : Layer0)
    {
        if (Node.FirstChild.IsValid())
        {
            TotalLeaves++;
            const AeonixLeafNode& Leaf = NavData.OctreeData.GetLeafNode(Node.FirstChild.GetNodeIndex());
            if (Leaf.IsEmpty())
            {
                EmptyLeaves++;
            }
            else if (!Leaf.IsCompletelyBlocked())
            {
                PartialLeaves++;
            }
        }
    }

    UE_LOG(LogTemp, Display, TEXT("Navigation data generated:"));
    UE_LOG(LogTemp, Display, TEXT("  Total voxels: %d"), DebugDraw.TotalVoxelCount);
    UE_LOG(LogTemp, Display, TEXT("  Blocked voxels: %d"), DebugDraw.BlockedVoxelCount);
    UE_LOG(LogTemp, Display, TEXT(""));
    UE_LOG(LogTemp, Display, TEXT("Leaf analysis: %d total, %d empty (%.1f%%), %d partial"),
        TotalLeaves, EmptyLeaves, (TotalLeaves > 0 ? EmptyLeaves * 100.0f / TotalLeaves : 0.0f), PartialLeaves);
    UE_LOG(LogTemp, Display, TEXT(""));

    // Setup pathfinder
    FAeonixPathFinderSettings PathSettings;
    PathSettings.MaxIterations = 10000;
    PathSettings.bUseUnitCost = false;
    PathSettings.bOptimizePath = true;
    PathSettings.bUseStringPulling = false;
    PathSettings.bSmoothPositions = false;
    PathSettings.HeuristicSettings.EuclideanWeight = 1.0f;
    PathSettings.HeuristicSettings.GlobalWeight = 10.0f;
    PathSettings.HeuristicSettings.NodeSizeWeight = 1.0f;

    // Run benchmark
    UE_LOG(LogTemp, Display, TEXT("Running scattered obstacle benchmark..."));

    FAeonixPathfindBenchmark Benchmark;
    FAeonixPathfindBenchmarkSummary Summary = Benchmark.RunBenchmark(
        BenchmarkSeed,
        NumRuns,
        NavData,
        PathSettings
    );

    Summary.LogSummary();

    // Add key metrics to test output (always visible)
    AddInfo(FString::Printf(TEXT("=== SCATTERED OBSTACLES BENCHMARK ===")));
    AddInfo(FString::Printf(TEXT("Tests pathfinding with small scattered box obstacles (no blocking walls).")));
    AddInfo(FString::Printf(TEXT("All regions reachable - high success rate expected.")));
    AddInfo(FString::Printf(TEXT("Leaf nodes: %d total, %d empty (%.1f%%) - optimization applies to empty leaves"),
        TotalLeaves, EmptyLeaves, (TotalLeaves > 0 ? EmptyLeaves * 100.0f / TotalLeaves : 0.0f)));
    AddInfo(FString::Printf(TEXT("")));
    AddInfo(FString::Printf(TEXT("Seed: %d | Runs: %d | Success: %d (%.1f%%)"),
        Summary.Seed, Summary.TotalRuns, Summary.SuccessfulRuns, Summary.GetSuccessRate()));
    if (Summary.SuccessfulRuns > 0)
    {
        AddInfo(FString::Printf(TEXT("Iterations: Avg=%.1f, Min=%d, Max=%d, StdDev=%.1f"),
            Summary.AvgIterations, Summary.MinIterations, Summary.MaxIterations, Summary.StdDevIterations));
        AddInfo(FString::Printf(TEXT("Time (ms): Avg=%.3f, Min=%.3f, Max=%.3f, StdDev=%.3f"),
            Summary.AvgTimeMs, Summary.MinTimeMs, Summary.MaxTimeMs, Summary.StdDevTimeMs));
        AddInfo(FString::Printf(TEXT("Path Length: Avg=%.1f | Direct Distance: Avg=%.1f"),
            Summary.AvgPathLength, Summary.AvgDirectDistance));
        AddInfo(FString::Printf(TEXT("Total benchmark time: %.1fms"), Summary.TotalTimeMs));
    }

    // Analyze failures
    int32 QuickFailures = 0;  // iterations <= 1 (blocked start/end)
    int32 ExhaustiveFailures = 0;  // iterations > 1000 (no path exists)
    for (const FAeonixPathfindBenchmarkResult& Result : Summary.Results)
    {
        if (!Result.bSuccess)
        {
            if (Result.Iterations <= 1)
            {
                QuickFailures++;
            }
            else if (Result.Iterations > 1000)
            {
                ExhaustiveFailures++;
            }
        }
    }

    UE_LOG(LogTemp, Display, TEXT("Failure analysis:"));
    UE_LOG(LogTemp, Display, TEXT("  Quick failures (blocked start/end): %d"), QuickFailures);
    UE_LOG(LogTemp, Display, TEXT("  Exhaustive failures (no path): %d"), ExhaustiveFailures);
    UE_LOG(LogTemp, Display, TEXT(""));

    TestTrue(TEXT("Benchmark should complete all runs"), Summary.TotalRuns == NumRuns);
    // With scattered obstacles, most paths should be reachable
    // (some failures due to start/end in blocked voxels or hitting iteration limit)
    TestTrue(TEXT("Should have at least 70%% success rate"),
        Summary.GetSuccessRate() >= 70.0f);

    if (EmptyLeaves > 0)
    {
        UE_LOG(LogTemp, Display, TEXT("Empty leaf optimization is applicable to %d leaves!"), EmptyLeaves);
        UE_LOG(LogTemp, Display, TEXT("Compare iteration counts with dense obstacle benchmark."));
    }
    else
    {
        UE_LOG(LogTemp, Display, TEXT("Note: No empty leaves - all %d leaves have partial blocking geometry"), TotalLeaves);
    }

    UE_LOG(LogTemp, Display, TEXT(""));
    UE_LOG(LogTemp, Display, TEXT("========================================"));
    UE_LOG(LogTemp, Display, TEXT(""));

    return true;
}

/**
 * Benchmark test with dynamic subregion to create empty leaf nodes
 * This should demonstrate the empty leaf optimization
 */
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAeonixNavigation_BenchmarkDynamicSubregionTest,
    "AeonixNavigation.Benchmark.DynamicSubregion",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAeonixNavigation_BenchmarkDynamicSubregionTest::RunTest(const FString& Parameters)
{
    const int32 BenchmarkSeed = 12345;
    const int32 NumRuns = 100;

    UE_LOG(LogTemp, Display, TEXT(""));
    UE_LOG(LogTemp, Display, TEXT("========================================"));
    UE_LOG(LogTemp, Display, TEXT("  Dynamic Subregion Benchmark"));
    UE_LOG(LogTemp, Display, TEXT("========================================"));
    UE_LOG(LogTemp, Display, TEXT(""));

    // Use same scattered obstacles as EmptySpace test
    class FScatteredObstaclesCollisionInterface : public IAeonixCollisionQueryInterface
    {
    public:
        virtual bool IsBlocked(const FVector& Position, const float VoxelSize, ECollisionChannel CollisionChannel, const float AgentRadius) const override
        {
            struct FObstacle
            {
                FVector Center;
                float HalfExtent;
            };

            static const FObstacle Obstacles[] = {
                { FVector(0, 0, 0), 40.0f },
                { FVector(100, 100, 0), 30.0f },
                { FVector(-100, -100, 0), 30.0f },
                { FVector(100, -100, 0), 30.0f },
                { FVector(-100, 100, 0), 30.0f },
                { FVector(200, 0, 100), 35.0f },
                { FVector(-200, 0, -100), 35.0f },
                { FVector(0, 200, 100), 35.0f },
                { FVector(0, -200, -100), 35.0f },
                { FVector(300, 300, 0), 40.0f },
                { FVector(-300, -300, 0), 40.0f },
                { FVector(300, -300, 0), 40.0f },
                { FVector(-300, 300, 0), 40.0f },
                { FVector(150, 0, 200), 30.0f },
                { FVector(-150, 0, -200), 30.0f },
                { FVector(0, 150, 200), 30.0f },
                { FVector(0, -150, -200), 30.0f },
            };

            const int32 NumObstacles = sizeof(Obstacles) / sizeof(Obstacles[0]);

            for (int32 i = 0; i < NumObstacles; ++i)
            {
                const FObstacle& Obs = Obstacles[i];
                if (FMath::Abs(Position.X - Obs.Center.X) < (Obs.HalfExtent + VoxelSize) &&
                    FMath::Abs(Position.Y - Obs.Center.Y) < (Obs.HalfExtent + VoxelSize) &&
                    FMath::Abs(Position.Z - Obs.Center.Z) < (Obs.HalfExtent + VoxelSize))
                {
                    return true;
                }
            }

            return false;
        }

        virtual bool IsLeafBlocked(const FVector& Position, const float LeafSize, ECollisionChannel CollisionChannel, const float AgentRadius) const override
        {
            return IsBlocked(Position, LeafSize, CollisionChannel, AgentRadius);
        }
    };

    // Collision interface that returns nothing blocked (for dynamic region regeneration)
    class FEmptyCollisionInterface : public IAeonixCollisionQueryInterface
    {
    public:
        virtual bool IsBlocked(const FVector& Position, const float VoxelSize, ECollisionChannel CollisionChannel, const float AgentRadius) const override
        {
            return false;
        }

        virtual bool IsLeafBlocked(const FVector& Position, const float LeafSize, ECollisionChannel CollisionChannel, const float AgentRadius) const override
        {
            return false;
        }
    };

    FScatteredObstaclesCollisionInterface ScatteredCollision;
    FEmptyCollisionInterface EmptyCollision;
    FTestDebugDrawInterface DebugDraw;
    FAeonixData NavData;

    // Add dynamic subregion BEFORE generation (must be in params for leaf pre-allocation)
    FGuid RegionId = FGuid::NewGuid();
    FBox DynamicRegionBox(FVector(-200, -200, -200), FVector(200, 200, 200));

    FAeonixGenerationParameters Params;
    Params.Origin = FVector::ZeroVector;
    Params.Extents = FVector(500, 500, 500);
    Params.OctreeDepth = 5;
    Params.CollisionChannel = ECollisionChannel::ECC_WorldStatic;
    Params.AgentRadius = 34.f;
    Params.AddDynamicRegion(RegionId, DynamicRegionBox);

    NavData.UpdateGenerationParameters(Params);

    UE_LOG(LogTemp, Display, TEXT("Generating initial navigation data with dynamic region..."));
    UWorld* DummyWorld = nullptr;
    NavData.Generate(*DummyWorld, ScatteredCollision, DebugDraw);

    // Count leaves before clearing the dynamic region
    int32 LeavesBefore = 0;
    int32 EmptyLeavesBefore = 0;
    const TArray<AeonixNode>& Layer0Before = NavData.OctreeData.GetLayer(0);
    for (const AeonixNode& Node : Layer0Before)
    {
        if (Node.FirstChild.IsValid())
        {
            LeavesBefore++;
            const AeonixLeafNode& Leaf = NavData.OctreeData.GetLeafNode(Node.FirstChild.GetNodeIndex());
            if (Leaf.IsEmpty())
            {
                EmptyLeavesBefore++;
            }
        }
    }

    UE_LOG(LogTemp, Display, TEXT("Before clearing dynamic region: %d leaves, %d empty (%.1f%%)"),
        LeavesBefore, EmptyLeavesBefore, (LeavesBefore > 0 ? EmptyLeavesBefore * 100.0f / LeavesBefore : 0.0f));

    // Regenerate the dynamic region with empty collision (clears all obstacles in that area)
    TSet<FGuid> RegionsToRegen;
    RegionsToRegen.Add(RegionId);
    NavData.RegenerateDynamicSubregions(RegionsToRegen, EmptyCollision, DebugDraw);

    // Count leaves after dynamic region
    int32 TotalLeaves = 0;
    int32 EmptyLeaves = 0;
    int32 PartialLeaves = 0;
    const TArray<AeonixNode>& Layer0 = NavData.OctreeData.GetLayer(0);
    for (const AeonixNode& Node : Layer0)
    {
        if (Node.FirstChild.IsValid())
        {
            TotalLeaves++;
            const AeonixLeafNode& Leaf = NavData.OctreeData.GetLeafNode(Node.FirstChild.GetNodeIndex());
            if (Leaf.IsEmpty())
            {
                EmptyLeaves++;
            }
            else if (!Leaf.IsCompletelyBlocked())
            {
                PartialLeaves++;
            }
        }
    }

    UE_LOG(LogTemp, Display, TEXT("After clearing dynamic region: %d leaves, %d empty (%.1f%%), %d partial"),
        TotalLeaves, EmptyLeaves, (TotalLeaves > 0 ? EmptyLeaves * 100.0f / TotalLeaves : 0.0f), PartialLeaves);
    UE_LOG(LogTemp, Display, TEXT(""));

    // Setup pathfinder
    FAeonixPathFinderSettings PathSettings;
    PathSettings.MaxIterations = 10000;
    PathSettings.bUseUnitCost = false;
    PathSettings.bOptimizePath = true;
    PathSettings.bUseStringPulling = false;
    PathSettings.bSmoothPositions = false;
    PathSettings.HeuristicSettings.EuclideanWeight = 1.0f;
    PathSettings.HeuristicSettings.GlobalWeight = 10.0f;
    PathSettings.HeuristicSettings.NodeSizeWeight = 1.0f;

    // Run benchmark
    UE_LOG(LogTemp, Display, TEXT("Running dynamic subregion benchmark..."));

    FAeonixPathfindBenchmark Benchmark;
    FAeonixPathfindBenchmarkSummary Summary = Benchmark.RunBenchmark(
        BenchmarkSeed,
        NumRuns,
        NavData,
        PathSettings
    );

    Summary.LogSummary();

    // Add key metrics to test output (always visible)
    AddInfo(FString::Printf(TEXT("=== DYNAMIC SUBREGION BENCHMARK ===")));
    AddInfo(FString::Printf(TEXT("Tests pathfinding after dynamic subregion clears central area.")));
    AddInfo(FString::Printf(TEXT("Dynamic region creates empty leaf nodes where optimization applies.")));
    AddInfo(FString::Printf(TEXT("Leaf nodes: %d total, %d empty (%.1f%%) after clearing dynamic region"),
        TotalLeaves, EmptyLeaves, (TotalLeaves > 0 ? EmptyLeaves * 100.0f / TotalLeaves : 0.0f)));
    AddInfo(FString::Printf(TEXT("")));
    AddInfo(FString::Printf(TEXT("Seed: %d | Runs: %d | Success: %d (%.1f%%)"),
        Summary.Seed, Summary.TotalRuns, Summary.SuccessfulRuns, Summary.GetSuccessRate()));
    if (Summary.SuccessfulRuns > 0)
    {
        AddInfo(FString::Printf(TEXT("Iterations: Avg=%.1f, Min=%d, Max=%d, StdDev=%.1f"),
            Summary.AvgIterations, Summary.MinIterations, Summary.MaxIterations, Summary.StdDevIterations));
        AddInfo(FString::Printf(TEXT("Time (ms): Avg=%.3f, Min=%.3f, Max=%.3f, StdDev=%.3f"),
            Summary.AvgTimeMs, Summary.MinTimeMs, Summary.MaxTimeMs, Summary.StdDevTimeMs));
        AddInfo(FString::Printf(TEXT("Path Length: Avg=%.1f | Direct Distance: Avg=%.1f"),
            Summary.AvgPathLength, Summary.AvgDirectDistance));
        AddInfo(FString::Printf(TEXT("Total benchmark time: %.1fms"), Summary.TotalTimeMs));
    }

    TestTrue(TEXT("Benchmark should complete all runs"), Summary.TotalRuns == NumRuns);
    TestTrue(TEXT("Should have high success rate with cleared central area (>80%)"),
        Summary.GetSuccessRate() >= 80.0f);

    if (EmptyLeaves > 0)
    {
        AddInfo(FString::Printf(TEXT("SUCCESS: Dynamic region created %d empty leaves for optimization!"), EmptyLeaves));
    }
    else
    {
        AddWarning(TEXT("No empty leaves created - dynamic region may need adjustment"));
    }

    UE_LOG(LogTemp, Display, TEXT(""));
    UE_LOG(LogTemp, Display, TEXT("========================================"));
    UE_LOG(LogTemp, Display, TEXT(""));

    return true;
}
