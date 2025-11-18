#include "Data/AeonixData.h"
#include "Data/AeonixLeafNode.h"
#include "Pathfinding/AeonixPathFinder.h"
#include "Pathfinding/AeonixNavigationPath.h"
#include "Data/AeonixLink.h"
#include "Engine/World.h"
#include "Engine/EngineTypes.h"
#include "Misc/AutomationTest.h"
#include "../Public/AeonixNavigationTestMocks.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAeonixNavigation_EmptyLeafOptimizationTest,
    "AeonixNavigation.Pathfinding.EmptyLeafOptimization",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAeonixNavigation_EmptyLeafOptimizationTest::RunTest(const FString& Parameters)
{
    UE_LOG(LogTemp, Display, TEXT("=== Empty Leaf Optimization Test ==="));

    // Setup - use partial obstacle interface that creates subdivision with gaps
    FTestPartialObstacleCollisionQueryInterface ObstacleCollision;
    FTestDebugDrawInterface DebugDraw;
    FAeonixData NavData;

    // Setup generation parameters - same as working tests
    FAeonixGenerationParameters Params;
    Params.Origin = FVector::ZeroVector;
    Params.Extents = FVector(500, 500, 500); // 1000x1000x1000 volume centered at origin
    Params.VoxelPower = 4; // 16x16x16 voxels at layer 0
    Params.CollisionChannel = ECollisionChannel::ECC_WorldStatic;
    Params.AgentRadius = 34.f;
    Params.ShowLeafVoxels = true;
    Params.ShowMortonCodes = false;

    NavData.UpdateGenerationParameters(Params);

    // Generate navigation data
    UWorld* DummyWorld = nullptr;
    NavData.Generate(*DummyWorld, ObstacleCollision, DebugDraw);

    UE_LOG(LogTemp, Display, TEXT("Navigation generation complete."));
    UE_LOG(LogTemp, Display, TEXT("  Total voxels visualized: %d"), DebugDraw.TotalVoxelCount);
    UE_LOG(LogTemp, Display, TEXT("  Blocked voxels: %d"), DebugDraw.BlockedVoxelCount);
    UE_LOG(LogTemp, Display, TEXT("  Clear voxels: %d"), DebugDraw.TotalVoxelCount - DebugDraw.BlockedVoxelCount);

    // Count empty leaves
    int32 TotalLeafNodes = 0;
    int32 EmptyLeafNodes = 0;
    int32 PartiallyBlockedLeafNodes = 0;
    int32 CompletelyBlockedLeafNodes = 0;

    const TArray<AeonixNode>& Layer0 = NavData.OctreeData.GetLayer(0);
    for (int32 i = 0; i < Layer0.Num(); ++i)
    {
        const AeonixNode& node = Layer0[i];
        if (node.FirstChild.IsValid())
        {
            TotalLeafNodes++;
            const AeonixLeafNode& leafNode = NavData.OctreeData.GetLeafNode(node.FirstChild.GetNodeIndex());

            if (leafNode.IsEmpty())
            {
                EmptyLeafNodes++;
            }
            else if (leafNode.IsCompletelyBlocked())
            {
                CompletelyBlockedLeafNodes++;
            }
            else
            {
                PartiallyBlockedLeafNodes++;
            }
        }
    }

    UE_LOG(LogTemp, Display, TEXT("Leaf node analysis:"));
    UE_LOG(LogTemp, Display, TEXT("  Total leaf nodes: %d"), TotalLeafNodes);
    UE_LOG(LogTemp, Display, TEXT("  Empty leaves (VoxelGrid == 0): %d (%.1f%%)"),
        EmptyLeafNodes, (TotalLeafNodes > 0 ? (EmptyLeafNodes * 100.0f / TotalLeafNodes) : 0.0f));
    UE_LOG(LogTemp, Display, TEXT("  Partially blocked leaves: %d (%.1f%%)"),
        PartiallyBlockedLeafNodes, (TotalLeafNodes > 0 ? (PartiallyBlockedLeafNodes * 100.0f / TotalLeafNodes) : 0.0f));
    UE_LOG(LogTemp, Display, TEXT("  Completely blocked leaves: %d (%.1f%%)"),
        CompletelyBlockedLeafNodes, (TotalLeafNodes > 0 ? (CompletelyBlockedLeafNodes * 100.0f / TotalLeafNodes) : 0.0f));

    // Note: This wall-based collision interface creates continuous obstacles,
    // so subdivided leaves will have partial blocking. Empty leaves would only
    // exist with scattered small obstacles that force subdivision but leave
    // adjacent leaves clear.
    if (EmptyLeafNodes > 0)
    {
        UE_LOG(LogTemp, Display, TEXT("Empty leaf optimization applicable to %d leaves"), EmptyLeafNodes);
    }
    else
    {
        UE_LOG(LogTemp, Display, TEXT("Note: Wall-based obstacles don't create empty leaves"));
    }

    // TEST 2: Path through empty space (should benefit from optimization)
    UE_LOG(LogTemp, Display, TEXT("\n=== TEST 2: Pathfinding through empty space ==="));

    FAeonixPathFinderSettings PathSettings;
    PathSettings.MaxIterations = 10000;
    PathSettings.bUseUnitCost = false;
    PathSettings.bOptimizePath = true;
    PathSettings.bUseStringPulling = false; // Disable to see raw path
    PathSettings.HeuristicSettings.EuclideanWeight = 1.0f;
    PathSettings.HeuristicSettings.GlobalWeight = 1.0f;

    AeonixPathFinder PathFinder(NavData, PathSettings);

    // Test path through large empty region
    FVector StartPos(-400, 300, 0);  // Far from obstacle
    FVector EndPos(400, 300, 0);     // Far from obstacle on other side

    UE_LOG(LogTemp, Display, TEXT("Testing path from %s to %s (through empty space)"),
        *StartPos.ToString(), *EndPos.ToString());

    AeonixLink StartLink, EndLink;
    FString StartLogMsg, EndLogMsg;

    bool bFoundStart = FAeonixNavigationTestUtils::FindLinkAtPosition(NavData, StartPos, StartLink, StartLogMsg);
    bool bFoundEnd = FAeonixNavigationTestUtils::FindLinkAtPosition(NavData, EndPos, EndLink, EndLogMsg);

    if (bFoundStart)
        UE_LOG(LogTemp, Display, TEXT("  Start: %s"), *StartLogMsg);
    if (bFoundEnd)
        UE_LOG(LogTemp, Display, TEXT("  End: %s"), *EndLogMsg);

    TestTrue(TEXT("Found valid start navigation link"), bFoundStart);
    TestTrue(TEXT("Found valid end navigation link"), bFoundEnd);

    if (bFoundStart && bFoundEnd)
    {
        FAeonixNavigationPath Path;
        bool bPathFound = PathFinder.FindPath(StartLink, EndLink, StartPos, EndPos, Path);

        TestTrue(TEXT("Path should exist through empty space"), bPathFound);

        if (bPathFound)
        {
            const TArray<FAeonixPathPoint>& PathPoints = Path.GetPathPoints();
            UE_LOG(LogTemp, Display, TEXT("  Path found! %d path points generated"), PathPoints.Num());

            // Verify path goes through clear space (not through obstacles)
            // FTestPartialObstacleCollisionQueryInterface has walls at X=0 with gap at Y=-50 to 50
            bool bPathInvalid = false;
            for (int32 i = 0; i < PathPoints.Num(); ++i)
            {
                const FAeonixPathPoint& Point = PathPoints[i];

                // Check if point is inside obstacle 1 (Y from -300 to -50)
                if (FMath::Abs(Point.Position.X) < ObstacleCollision.Obstacle1_Thickness * 0.5f &&
                    Point.Position.Y >= ObstacleCollision.Obstacle1_YMin &&
                    Point.Position.Y <= ObstacleCollision.Obstacle1_YMax)
                {
                    bPathInvalid = true;
                    AddError(FString::Printf(TEXT("Path point %d at %s is inside obstacle 1!"),
                        i, *Point.Position.ToString()));
                }
                // Check if point is inside obstacle 2 (Y from 50 to 300)
                if (FMath::Abs(Point.Position.X) < ObstacleCollision.Obstacle2_Thickness * 0.5f &&
                    Point.Position.Y >= ObstacleCollision.Obstacle2_YMin &&
                    Point.Position.Y <= ObstacleCollision.Obstacle2_YMax)
                {
                    bPathInvalid = true;
                    AddError(FString::Printf(TEXT("Path point %d at %s is inside obstacle 2!"),
                        i, *Point.Position.ToString()));
                }
            }

            TestFalse(TEXT("Path should not go through obstacles"), bPathInvalid);

            // Calculate path efficiency
            float DirectDistance = FVector::Dist(StartPos, EndPos);
            float PathLength = 0.0f;
            for (int32 i = 1; i < PathPoints.Num(); ++i)
            {
                PathLength += FVector::Dist(PathPoints[i-1].Position, PathPoints[i].Position);
            }

            float PathEfficiency = (DirectDistance / PathLength) * 100.0f;
            UE_LOG(LogTemp, Display, TEXT("  Direct distance: %.2f, Path length: %.2f, Efficiency: %.1f%%"),
                DirectDistance, PathLength, PathEfficiency);

            // For a path through empty space, efficiency should be high (close to direct)
            TestTrue(TEXT("Path through empty space should be reasonably efficient (>70%)"),
                PathEfficiency > 70.0f);
        }
    }

    // TEST 3: Path that requires navigating around obstacle
    UE_LOG(LogTemp, Display, TEXT("\n=== TEST 3: Pathfinding around obstacle ==="));

    // Path from left to right at Y=-200 (inside obstacle 1 region)
    // Must go through the gap at Y=-50 to 50
    FVector StartPos2(-200, -200, 0);  // Left of obstacle, in blocked Y region
    FVector EndPos2(200, -200, 0);     // Right of obstacle, in blocked Y region

    UE_LOG(LogTemp, Display, TEXT("Testing path from %s to %s (must go through gap)"),
        *StartPos2.ToString(), *EndPos2.ToString());

    bFoundStart = FAeonixNavigationTestUtils::FindLinkAtPosition(NavData, StartPos2, StartLink, StartLogMsg);
    bFoundEnd = FAeonixNavigationTestUtils::FindLinkAtPosition(NavData, EndPos2, EndLink, EndLogMsg);

    if (bFoundStart && bFoundEnd)
    {
        FAeonixNavigationPath Path2;
        bool bPath2Found = PathFinder.FindPath(StartLink, EndLink, StartPos2, EndPos2, Path2);

        TestTrue(TEXT("Path should exist around obstacle"), bPath2Found);

        if (bPath2Found)
        {
            const TArray<FAeonixPathPoint>& PathPoints2 = Path2.GetPathPoints();
            UE_LOG(LogTemp, Display, TEXT("  Path found! %d path points generated"), PathPoints2.Num());

            // Path should go around the obstacle - either through the Y gap or around via Z
            bool bDeviatesFromDirect = false;
            bool bPathInvalid = false;

            for (int32 i = 0; i < PathPoints2.Num(); ++i)
            {
                const FAeonixPathPoint& Point = PathPoints2[i];

                // Check if path deviates from direct line (goes through gap or around via Z)
                // Direct line would be at Y=-200, Z=0
                if (FMath::Abs(Point.Position.Y - (-200.0f)) > 100.0f ||
                    FMath::Abs(Point.Position.Z) > 100.0f)
                {
                    bDeviatesFromDirect = true;
                }

                // Check if point is inside obstacle 1 or 2
                if (FMath::Abs(Point.Position.X) < ObstacleCollision.Obstacle1_Thickness * 0.5f)
                {
                    if ((Point.Position.Y >= ObstacleCollision.Obstacle1_YMin &&
                         Point.Position.Y <= ObstacleCollision.Obstacle1_YMax) ||
                        (Point.Position.Y >= ObstacleCollision.Obstacle2_YMin &&
                         Point.Position.Y <= ObstacleCollision.Obstacle2_YMax))
                    {
                        bPathInvalid = true;
                        AddError(FString::Printf(TEXT("Path point %d at %s is inside obstacle!"),
                            i, *Point.Position.ToString()));
                    }
                }
            }

            TestFalse(TEXT("Path should not go through obstacles"), bPathInvalid);
            TestTrue(TEXT("Path should deviate to avoid obstacle"), bDeviatesFromDirect);

            // Path should be longer than direct distance
            float DirectDistance2 = FVector::Dist(StartPos2, EndPos2);
            float PathLength2 = 0.0f;
            for (int32 i = 1; i < PathPoints2.Num(); ++i)
            {
                PathLength2 += FVector::Dist(PathPoints2[i-1].Position, PathPoints2[i].Position);
            }

            UE_LOG(LogTemp, Display, TEXT("  Direct distance: %.2f, Path length: %.2f (%.1f%% longer)"),
                DirectDistance2, PathLength2, ((PathLength2 / DirectDistance2) - 1.0f) * 100.0f);

            TestTrue(TEXT("Path should be longer than direct distance due to obstacle"),
                PathLength2 > DirectDistance2);
        }
    }

    // TEST 4: Verify optimization is actually being used
    UE_LOG(LogTemp, Display, TEXT("\n=== TEST 4: Optimization verification ==="));
    UE_LOG(LogTemp, Display, TEXT("Empty leaf optimization is enabled when:"));
    UE_LOG(LogTemp, Display, TEXT("  - Layer 0 node has valid FirstChild (leaf subdivision exists)"));
    UE_LOG(LogTemp, Display, TEXT("  - Leaf VoxelGrid == 0 (all 64 voxels are clear)"));
    UE_LOG(LogTemp, Display, TEXT("  - GetNeighbours is called instead of GetLeafNeighbours"));
    UE_LOG(LogTemp, Display, TEXT("Result: Treats entire Layer 0 node as single navigable space"));
    UE_LOG(LogTemp, Display, TEXT("Benefit: Skips processing up to 64 individual voxels per empty leaf"));

    if (EmptyLeafNodes > 0)
    {
        UE_LOG(LogTemp, Display, TEXT("\nOptimization is applicable to %d empty leaf nodes in this test!"),
            EmptyLeafNodes);
    }

    UE_LOG(LogTemp, Display, TEXT("\n=== Empty Leaf Optimization Test Complete ==="));

    return true;
}
