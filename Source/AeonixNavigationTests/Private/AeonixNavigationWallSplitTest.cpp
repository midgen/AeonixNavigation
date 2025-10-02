#include <AeonixNavigation/Public/Data/AeonixData.h>
#include <AeonixNavigation/Public/Pathfinding/AeonixPathFinder.h>
#include <AeonixNavigation/Public/Pathfinding/AeonixNavigationPath.h>
#include <AeonixNavigation/Public/Data/AeonixLink.h>
#include <Engine/World.h>
#include <Engine/EngineTypes.h>
#include <Misc/AutomationTest.h>
#include "../Public/AeonixNavigationTestMocks.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAeonixNavigation_WallSplitPathfindingTest,
    "AeonixNavigation.Pathfinding.WallSplitBug",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAeonixNavigation_WallSplitPathfindingTest::RunTest(const FString& Parameters)
{
    // Setup
    FTestWallCollisionQueryInterface WallCollision;
    FTestDebugDrawInterface DebugDraw;
    FAeonixData NavData;

    // Setup generation parameters for a volume split by a wall
    FAeonixGenerationParameters Params;
    Params.Origin = FVector::ZeroVector;
    Params.Extents = FVector(500, 500, 500); // 1000x1000x1000 volume centered at origin
    Params.VoxelPower = 4; // 16x16x16 voxels at layer 0
    Params.CollisionChannel = ECollisionChannel::ECC_WorldStatic;
    Params.AgentRadius = 34.f;
    Params.ShowLeafVoxels = true; // Enable to see blocked voxels
    Params.ShowMortonCodes = false;

    NavData.UpdateGenerationParameters(Params);

    // Generate navigation data with the wall
    // Note: World parameter is still required but not used for collision after our fix
    UWorld* DummyWorld = nullptr;
    NavData.Generate(*DummyWorld, WallCollision, DebugDraw);

    UE_LOG(LogTemp, Display, TEXT("Navigation generation complete. Blocked voxels: %d/%d"),
        DebugDraw.BlockedVoxelCount, DebugDraw.TotalVoxelCount);

    // Setup pathfinding test
    FAeonixPathFinderSettings PathSettings;
    PathSettings.MaxIterations = 10000;
    PathSettings.bUseUnitCost = false;
    PathSettings.bOptimizePath = true;
    PathSettings.bUseStringPulling = true;
    PathSettings.HeuristicSettings.EuclideanWeight = 1.0f;
    PathSettings.HeuristicSettings.GlobalWeight = 10.0f;

    AeonixPathFinder PathFinder(NavData, PathSettings);

    // Define start and end positions on opposite sides of the wall
    FVector StartPos(-200, -200, 0); // One side of the wall (Y < 0)
    FVector EndPos(200, 200, 0);     // Other side of the wall (Y > 0)

    UE_LOG(LogTemp, Display, TEXT("Testing path from %s to %s (wall at Y=0)"),
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

    // Log the result
    if (bPathFound)
    {
        const TArray<FAeonixPathPoint>& PathPoints = Path.GetPathPoints();
        UE_LOG(LogTemp, Error, TEXT("BUG DEMONSTRATED: Path found through wall! Path has %d points:"),
            PathPoints.Num());

        for (int32 i = 0; i < PathPoints.Num(); ++i)
        {
            const FAeonixPathPoint& Point = PathPoints[i];
            UE_LOG(LogTemp, Error, TEXT("  Point %d: %s (Layer: %d)"),
                i, *Point.Position.ToString(), Point.Layer);

            // Check if any path point crosses the wall
            if (i > 0)
            {
                const FAeonixPathPoint& PrevPoint = PathPoints[i - 1];

                // Check if we crossed from negative Y to positive Y (or vice versa)
                if ((PrevPoint.Position.Y < -WallCollision.WallThickness * 0.5f &&
                     Point.Position.Y > WallCollision.WallThickness * 0.5f) ||
                    (PrevPoint.Position.Y > WallCollision.WallThickness * 0.5f &&
                     Point.Position.Y < -WallCollision.WallThickness * 0.5f))
                {
                    UE_LOG(LogTemp, Error, TEXT("    >>> Path crosses wall between points %d and %d!"), i-1, i);
                }
            }
        }
    }
    else
    {
        UE_LOG(LogTemp, Display, TEXT("No path found (expected behavior - wall blocks the path)"));
    }

    // TEST EXPECTATION: There should be NO valid path through the wall
    // This test will FAIL initially, demonstrating the bug
    TestFalse(TEXT("No path should exist through the wall"), bPathFound);

    if (bPathFound)
    {
        AddError(TEXT("Pathfinder incorrectly found a path through a solid wall that splits the navigation volume!"));
        return false;
    }

    return true;
}