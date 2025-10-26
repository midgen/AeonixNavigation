#include "Data/AeonixData.h"
#include "Pathfinding/AeonixPathFinder.h"
#include "Pathfinding/AeonixNavigationPath.h"
#include "Data/AeonixLink.h"
#include "Engine/World.h"
#include "Engine/EngineTypes.h"
#include "Misc/AutomationTest.h"
#include "../Public/AeonixNavigationTestMocks.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAeonixNavigation_ObstacleNavigationTest,
    "AeonixNavigation.Pathfinding.ObstacleNavigation",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAeonixNavigation_ObstacleNavigationTest::RunTest(const FString& Parameters)
{
    // Setup
    FTestPartialObstacleCollisionQueryInterface ObstacleCollision;
    FTestDebugDrawInterface DebugDraw;
    FAeonixData NavData;

    // Setup generation parameters for a volume with obstacles
    FAeonixGenerationParameters Params;
    Params.Origin = FVector::ZeroVector;
    Params.Extents = FVector(500, 500, 500); // 1000x1000x1000 volume centered at origin
    Params.VoxelPower = 4; // 16x16x16 voxels at layer 0
    Params.CollisionChannel = ECollisionChannel::ECC_WorldStatic;
    Params.AgentRadius = 34.f;
    Params.ShowLeafVoxels = true; // Enable to see blocked voxels
    Params.ShowMortonCodes = false;

    NavData.UpdateGenerationParameters(Params);

    // Generate navigation data with the obstacles
    UWorld* DummyWorld = nullptr;
    NavData.Generate(*DummyWorld, ObstacleCollision, DebugDraw);

    UE_LOG(LogTemp, Display, TEXT("Navigation generation complete. Blocked voxels: %d/%d"),
        DebugDraw.BlockedVoxelCount, DebugDraw.TotalVoxelCount);

    // Log obstacle configuration for clarity
    UE_LOG(LogTemp, Display, TEXT("Obstacle configuration:"));
    UE_LOG(LogTemp, Display, TEXT("  Obstacle 1: X=%f, Y=[%f to %f] (gap in middle)"),
        ObstacleCollision.Obstacle1_X, ObstacleCollision.Obstacle1_YMin, ObstacleCollision.Obstacle1_YMax);
    UE_LOG(LogTemp, Display, TEXT("  Obstacle 2: X=%f, Y=[%f to %f] (gap in middle)"),
        ObstacleCollision.Obstacle2_X, ObstacleCollision.Obstacle2_YMin, ObstacleCollision.Obstacle2_YMax);
    UE_LOG(LogTemp, Display, TEXT("  Gap exists from Y=-50 to Y=50"));

    // Setup pathfinding test
    FAeonixPathFinderSettings PathSettings;
    PathSettings.MaxIterations = 10000;
    PathSettings.bUseUnitCost = false;
    PathSettings.bOptimizePath = true;
    PathSettings.bUseStringPulling = true;
    PathSettings.HeuristicSettings.EuclideanWeight = 1.0f;
    PathSettings.HeuristicSettings.GlobalWeight = 10.0f;

    AeonixPathFinder PathFinder(NavData, PathSettings);

    // Define start and end positions on opposite sides of the obstacles
    // Path should go through the gap in the middle
    FVector StartPos(-300, 0, 0);  // Left side of obstacles
    FVector EndPos(300, 0, 0);      // Right side of obstacles

    UE_LOG(LogTemp, Display, TEXT("Testing path from %s to %s (should navigate through gap)"),
        *StartPos.ToString(), *EndPos.ToString());

    // Find links for start and end positions
    AeonixLink StartLink, EndLink;
    FString StartLogMsg, EndLogMsg;

    bool bFoundStart = FAeonixNavigationTestUtils::FindLinkAtPosition(NavData, StartPos, StartLink, StartLogMsg);
    bool bFoundEnd = FAeonixNavigationTestUtils::FindLinkAtPosition(NavData, EndPos, EndLink, EndLogMsg);

    if (bFoundStart)
        UE_LOG(LogTemp, Display, TEXT("Start: %s"), *StartLogMsg);
    if (bFoundEnd)
        UE_LOG(LogTemp, Display, TEXT("End: %s"), *EndLogMsg);

    // Verify we found valid navigation links
    TestTrue(TEXT("Found valid start navigation link"), bFoundStart);
    TestTrue(TEXT("Found valid end navigation link"), bFoundEnd);

    if (!bFoundStart || !bFoundEnd)
    {
        UE_LOG(LogTemp, Error, TEXT("Could not find navigation links for test positions"));
        return false;
    }

    // Attempt to find a path
    FAeonixNavigationPath Path;
    bool bPathFound = PathFinder.FindPath(StartLink, EndLink, StartPos, EndPos, Path);

    // TEST EXPECTATION: There SHOULD be a valid path through the gap
    TestTrue(TEXT("Path should exist through the gap between obstacles"), bPathFound);

    if (bPathFound)
    {
        const TArray<FAeonixPathPoint>& PathPoints = Path.GetPathPoints();
        UE_LOG(LogTemp, Display, TEXT("SUCCESS: Path found around obstacles! Path has %d points:"),
            PathPoints.Num());

        bool bPassesThroughGap = false;

        for (int32 i = 0; i < PathPoints.Num(); ++i)
        {
            const FAeonixPathPoint& Point = PathPoints[i];
            UE_LOG(LogTemp, Display, TEXT("  Point %d: %s (Layer: %d)"),
                i, *Point.Position.ToString(), Point.Layer);

            // Check if path goes through the gap (Y between -50 and 50, X near 0)
            if (FMath::Abs(Point.Position.X) < 100.0f &&
                Point.Position.Y > -60.0f &&
                Point.Position.Y < 60.0f)
            {
                bPassesThroughGap = true;
                UE_LOG(LogTemp, Display, TEXT("    >>> Path correctly goes through gap!"));
            }

            // Verify no path point is inside an obstacle
            bool bInObstacle = false;

            // Check against obstacle 1
            if (FMath::Abs(Point.Position.X) < ObstacleCollision.Obstacle1_Thickness * 0.5f &&
                Point.Position.Y >= ObstacleCollision.Obstacle1_YMin &&
                Point.Position.Y <= ObstacleCollision.Obstacle1_YMax)
            {
                bInObstacle = true;
                AddError(FString::Printf(TEXT("Path point %d is inside Obstacle 1!"), i));
            }

            // Check against obstacle 2
            if (FMath::Abs(Point.Position.X) < ObstacleCollision.Obstacle2_Thickness * 0.5f &&
                Point.Position.Y >= ObstacleCollision.Obstacle2_YMin &&
                Point.Position.Y <= ObstacleCollision.Obstacle2_YMax)
            {
                bInObstacle = true;
                AddError(FString::Printf(TEXT("Path point %d is inside Obstacle 2!"), i));
            }

            TestFalse(FString::Printf(TEXT("Path point %d should not be inside an obstacle"), i), bInObstacle);
        }

        TestTrue(TEXT("Path should go through the gap between obstacles"), bPassesThroughGap);
    }
    else
    {
        AddError(TEXT("Pathfinder failed to find a path around obstacles with a clear gap!"));
        return false;
    }

    // Additional test: Try a path that requires more complex navigation
    FVector StartPos2(-300, -200, 0);  // Bottom-left
    FVector EndPos2(300, 200, 0);      // Top-right (should navigate around obstacles)

    UE_LOG(LogTemp, Display, TEXT("\nTesting diagonal path from %s to %s"),
        *StartPos2.ToString(), *EndPos2.ToString());

    // Find links for second test
    bFoundStart = FAeonixNavigationTestUtils::FindLinkAtPosition(NavData, StartPos2, StartLink, StartLogMsg);
    bFoundEnd = FAeonixNavigationTestUtils::FindLinkAtPosition(NavData, EndPos2, EndLink, EndLogMsg);

    if (bFoundStart && bFoundEnd)
    {
        FAeonixNavigationPath Path2;
        bool bPath2Found = PathFinder.FindPath(StartLink, EndLink, StartPos2, EndPos2, Path2);

        TestTrue(TEXT("Diagonal path should exist around obstacles"), bPath2Found);

        if (bPath2Found)
        {
            const TArray<FAeonixPathPoint>& PathPoints2 = Path2.GetPathPoints();
            UE_LOG(LogTemp, Display, TEXT("SUCCESS: Diagonal path found! Path has %d points"),
                PathPoints2.Num());

            // Path should be longer than direct distance due to navigating around obstacles
            float directDistance = FVector::Dist(StartPos2, EndPos2);
            float pathLength = 0.0f;

            for (int32 i = 1; i < PathPoints2.Num(); ++i)
            {
                pathLength += FVector::Dist(PathPoints2[i-1].Position, PathPoints2[i].Position);
            }

            UE_LOG(LogTemp, Display, TEXT("Direct distance: %f, Path length: %f"),
                directDistance, pathLength);

            TestTrue(TEXT("Path length should be greater than direct distance due to obstacles"),
                pathLength > directDistance);
        }
    }

    return true;
}