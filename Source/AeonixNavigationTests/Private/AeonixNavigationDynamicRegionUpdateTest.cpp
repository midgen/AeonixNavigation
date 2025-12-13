#include "Data/AeonixData.h"
#include "Data/AeonixGenerationParameters.h"
#include "Data/AeonixLink.h"
#include "Data/AeonixNode.h"
#include "Data/AeonixOctreeData.h"
#include "Data/AeonixLeafNode.h"
#include "Engine/World.h"
#include "Engine/EngineTypes.h"
#include "Interface/AeonixCollisionQueryInterface.h"
#include "Interface/AeonixDebugDrawInterface.h"
#include "Pathfinding/AeonixPathFinder.h"
#include "Pathfinding/AeonixNavigationPath.h"
#include "Misc/AutomationTest.h"
#include "AeonixNavigation.h"

// Silent debug interface for testing
class FSilentDebugDrawInterface : public IAeonixDebugDrawInterface
{
public:
    virtual void AeonixDrawDebugString(const FVector& Position, const FString& String, const FColor& Color) const override {}
    virtual void AeonixDrawDebugBox(const FVector& Position, const float Size, const FColor& Color) const override {}
    virtual void AeonixDrawDebugLine(const FVector& Start, const FVector& End, const FColor& Color, float Thickness = 0.0f) const override {}
    virtual void AeonixDrawDebugDirectionalArrow(const FVector& Start, const FVector& End, const FColor& Color, float ArrowSize = 0.0f) const override {}
};

// Dynamic obstacle mock collision that supports moving obstacles
class FDynamicObstacleMockCollision : public IAeonixCollisionQueryInterface
{
public:
    // Obstacle positions and their blocked states
    TMap<FVector, bool> Obstacles;
    FBox DynamicRegion;
    
    FDynamicObstacleMockCollision(const FBox& InDynamicRegion)
        : DynamicRegion(InDynamicRegion)
    {
        // Initialize with strategic obstacles for testing
        // Position A: Obstacle that will be moved
        Obstacles.Add(FVector(150, 0, 0), true);  // Obstacle A - initially blocking
        // Position B: Where obstacle A will move to
        Obstacles.Add(FVector(-150, 0, 0), false); // Obstacle B - initially not blocking
        
        // Add some static obstacles for more interesting pathfinding
        Obstacles.Add(FVector(0, 200, 0), true);   // Static obstacle north
        Obstacles.Add(FVector(0, -200, 0), true);  // Static obstacle south
    }
    
    // Move an obstacle from one position to another
    void MoveObstacle(const FVector& From, const FVector& To)
    {
        if (Obstacles.Contains(From))
        {
            bool bWasBlocked = Obstacles[From];
            Obstacles.Remove(From);
            Obstacles.Add(To, bWasBlocked);
            
            UE_LOG(LogTemp, Display, TEXT("Moved obstacle from %s to %s (blocked: %d)"),
                   *From.ToString(), *To.ToString(), bWasBlocked);
        }
    }
    
    // Toggle obstacle state at a position
    void SetObstacleBlocked(const FVector& Position, bool bBlocked)
    {
        if (Obstacles.Contains(Position))
        {
            Obstacles[Position] = bBlocked;
            UE_LOG(LogTemp, Display, TEXT("Set obstacle at %s to %s"),
                   *Position.ToString(), bBlocked ? TEXT("BLOCKED") : TEXT("CLEAR"));
        }
    }
    
    virtual bool IsBlocked(const FVector& Position, const float VoxelSize, ECollisionChannel CollisionChannel, const float AgentRadius) const override
    {
        // Check if position is near any active obstacle
        for (const auto& Obstacle : Obstacles)
        {
            if (Obstacle.Value) // Only check active obstacles
            {
                float Distance = FVector::Dist(Position, Obstacle.Key);
                // Use obstacle radius + voxel size for collision detection
                if (Distance < (50.0f + VoxelSize + AgentRadius))
                {
                    return true; // Blocked by obstacle
                }
            }
        }
        
        // Add some base terrain obstacles outside dynamic region
        if (FMath::Abs(Position.X) > 400.0f || FMath::Abs(Position.Y) > 400.0f)
        {
            return true; // Blocked boundaries
        }
        
        return false; // Not blocked
    }
    
    virtual bool IsLeafBlocked(const FVector& Position, const float LeafSize, ECollisionChannel CollisionChannel, const float AgentRadius) const override
    {
        return IsBlocked(Position, LeafSize, CollisionChannel, AgentRadius);
    }
};

// Helper function to get a link from a position
static bool GetLinkFromPosition(const FVector& Position, const FAeonixData& NavData, AeonixLink& OutLink)
{
    const FAeonixGenerationParameters& Params = NavData.GetParams();
    const FVector& Origin = Params.Origin;
    const FVector& Extent = Params.Extents;
    
    // Check if position is within bounds
    const FBox Bounds(Origin - Extent, Origin + Extent);
    if (!Bounds.IsInside(Position))
    {
        return false;
    }
    
    // Z-order origin (where code == 0)
    const FVector ZOrigin = Origin - Extent;
    const FVector LocalPos = Position - ZOrigin;
    
    // Start from top layer and descend
    int32 LayerIndex = NavData.OctreeData.GetNumLayers() - 1;
    nodeindex_t NodeIndex = 0;
    
    while (LayerIndex >= 0 && LayerIndex < NavData.OctreeData.GetNumLayers())
    {
        const TArray<AeonixNode>& Layer = NavData.OctreeData.GetLayer(LayerIndex);
        const float VoxelSize = NavData.GetVoxelSize(LayerIndex);
        
        // Calculate XYZ coordinates
        const int32 X = FMath::FloorToInt(LocalPos.X / VoxelSize);
        const int32 Y = FMath::FloorToInt(LocalPos.Y / VoxelSize);
        const int32 Z = FMath::FloorToInt(LocalPos.Z / VoxelSize);
        const mortoncode_t Code = morton3D_64_encode(X, Y, Z);
        
        // Find node with this code
        for (nodeindex_t j = NodeIndex; j < Layer.Num(); j++)
        {
            const AeonixNode& Node = Layer[j];
            if (Node.Code == Code)
            {
                // No children - this is the link
                if (!Node.FirstChild.IsValid())
                {
                    OutLink.LayerIndex = LayerIndex;
                    OutLink.NodeIndex = j;
                    OutLink.SubnodeIndex = 0;
                    return true;
                }
                
                // Leaf node - find subnode
                if (LayerIndex == 0)
                {
                    const AeonixLeafNode& Leaf = NavData.OctreeData.GetLeafNode(Node.FirstChild.NodeIndex);
                    
                    // Get node world position
                    FVector NodePosition;
                    NavData.GetNodePosition(LayerIndex, Node.Code, NodePosition);
                    const FVector NodeOrigin = NodePosition - FVector(VoxelSize * 0.5f);
                    const FVector NodeLocalPos = Position - NodeOrigin;
                    
                    // Calculate leaf voxel coordinates
                    const int32 LeafX = FMath::FloorToInt(NodeLocalPos.X / (VoxelSize * 0.25f));
                    const int32 LeafY = FMath::FloorToInt(NodeLocalPos.Y / (VoxelSize * 0.25f));
                    const int32 LeafZ = FMath::FloorToInt(NodeLocalPos.Z / (VoxelSize * 0.25f));
                    const mortoncode_t LeafIndex = morton3D_64_encode(LeafX, LeafY, LeafZ);
                    
                    // Check if blocked
                    if (Leaf.GetNode(LeafIndex))
                    {
                        return false; // Blocked
                    }
                    
                    OutLink.LayerIndex = 0;
                    OutLink.NodeIndex = j;
                    OutLink.SubnodeIndex = LeafIndex;
                    return true;
                }
                
                // Has children - descend
                LayerIndex = Layer[j].FirstChild.GetLayerIndex();
                NodeIndex = Layer[j].FirstChild.GetNodeIndex();
                break;
            }
        }
    }
    
    return false;
}

// Helper function to verify path avoids a specific position
static bool VerifyPathAvoidsPosition(const FAeonixNavigationPath& Path, const FVector& AvoidPosition, const FString& PositionName, float MinDistance = 50.0f)
{
    const TArray<FAeonixPathPoint>& PathPoints = Path.GetPathPoints();
    for (const FAeonixPathPoint& Point : PathPoints)
    {
        float Distance = FVector::Dist(Point.Position, AvoidPosition);
        if (Distance <= MinDistance)
        {
            UE_LOG(LogTemp, Error, TEXT("Path should avoid %s but is only %.1f units away"), *PositionName, Distance);
            return false;
        }
    }
    UE_LOG(LogTemp, Display, TEXT("Path correctly avoids %s"), *PositionName);
    return true;
}

// Helper function to verify paths are meaningfully different
static bool VerifyPathsAreDifferent(const FAeonixNavigationPath& Path1, const FAeonixNavigationPath& Path2, const FString& TestName)
{
    const TArray<FAeonixPathPoint>& Points1 = Path1.GetPathPoints();
    const TArray<FAeonixPathPoint>& Points2 = Path2.GetPathPoints();
    
    // Check if paths are identical
    bool bPathsIdentical = (Points1.Num() == Points2.Num());
    if (bPathsIdentical)
    {
        for (int32 i = 0; i < Points1.Num(); i++)
        {
            if (!Points1[i].Position.Equals(Points2[i].Position, 1.0f))
            {
                bPathsIdentical = false;
                break;
            }
        }
    }
    
    if (bPathsIdentical)
    {
        UE_LOG(LogTemp, Error, TEXT("%s: Paths should be different after obstacle move but they are identical"), *TestName);
        return false;
    }
    
    // Verify paths have significantly different segments
    int32 DifferentSegments = 0;
    int32 MinPoints = FMath::Min(Points1.Num(), Points2.Num());
    
    for (int32 i = 0; i < MinPoints; i++)
    {
        float Distance = FVector::Dist(Points1[i].Position, Points2[i].Position);
        if (Distance > 75.0f) // Significant difference
        {
            DifferentSegments++;
        }
    }
    
    if (DifferentSegments < 2)
    {
        UE_LOG(LogTemp, Error, TEXT("%s: Paths should have at least 2 significantly different segments but only have %d"), *TestName, DifferentSegments);
        return false;
    }
    
    UE_LOG(LogTemp, Display, TEXT("%s: Paths are meaningfully different with %d significantly different segments"), *TestName, DifferentSegments);
    UE_LOG(LogTemp, Display, TEXT("%s: Found %d significantly different path segments"), *TestName, DifferentSegments);
    return true;
}

// Main test implementation
IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FAeonixNavigation_DynamicRegionUpdateTest,
    "AeonixNavigation.DynamicRegion.UpdateVerification",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FAeonixNavigation_DynamicRegionUpdateTest::RunTest(const FString& Parameters)
{
    UE_LOG(LogTemp, Display, TEXT("========================================"));
    UE_LOG(LogTemp, Display, TEXT("Dynamic Region Update Verification Test"));
    UE_LOG(LogTemp, Display, TEXT("========================================"));
    
    // Test configuration
    const FVector TestOrigin = FVector::ZeroVector;
    const FVector TestExtents = FVector(500.0f, 500.0f, 50.0f); // Flat test area
    const int32 VoxelPower = 4;
    
    // Dynamic region covers central area where obstacles will move
    const FBox DynamicRegion(FVector(-250, -250, -25), FVector(250, 250, 25));
    
    // Start and end positions that require pathfinding through dynamic region
    const FVector StartPosition = FVector(-300, 0, 0);
    const FVector EndPosition = FVector(300, 0, 0);
    
    UE_LOG(LogTemp, Display, TEXT("Test Configuration:"));
    UE_LOG(LogTemp, Display, TEXT("  Volume: %s to %s"), 
           *(TestOrigin - TestExtents).ToString(), *(TestOrigin + TestExtents).ToString());
    UE_LOG(LogTemp, Display, TEXT("  Dynamic Region: %s to %s"), 
           *DynamicRegion.Min.ToString(), *DynamicRegion.Max.ToString());
    UE_LOG(LogTemp, Display, TEXT("  Start: %s, End: %s"), *StartPosition.ToString(), *EndPosition.ToString());
    
    // Setup interfaces
    FDynamicObstacleMockCollision* MockCollision = new FDynamicObstacleMockCollision(DynamicRegion);
    FSilentDebugDrawInterface MockDebug;
    FAeonixData NavData;
    
    // Setup navigation parameters
    FAeonixGenerationParameters Params;
    Params.Origin = TestOrigin;
    Params.Extents = TestExtents;
    Params.OctreeDepth = VoxelPower;
    Params.CollisionChannel = ECollisionChannel::ECC_WorldStatic;
    Params.AgentRadius = 34.0f;
    
    // Add dynamic region
    FGuid RegionId = FGuid::NewGuid();
    Params.AddDynamicRegion(RegionId, DynamicRegion);
    
    NavData.UpdateGenerationParameters(Params);
    
    // Generate initial navigation data
    UE_LOG(LogTemp, Display, TEXT("Generating initial navigation data..."));
    UWorld* DummyWorld = nullptr;
    NavData.Generate(*DummyWorld, *MockCollision, MockDebug);
    UE_LOG(LogTemp, Display, TEXT("Initial navigation generated"));
    
    // Setup pathfinder
    FAeonixPathFinderSettings PathSettings;
    PathSettings.MaxIterations = 5000;
    PathSettings.bUseUnitCost = false;
    PathSettings.bOptimizePath = true;
    
    AeonixPathFinder PathFinder(NavData, PathSettings);
    
    // ============================================
    // PHASE 1: Get initial path with obstacle at position A
    // ============================================
    UE_LOG(LogTemp, Display, TEXT("\n=== PHASE 1: Initial Path (Obstacle at Position A) ==="));
    
    // Get links for start and end positions
    AeonixLink StartLink, EndLink;
    bool bStartValid = GetLinkFromPosition(StartPosition, NavData, StartLink);
    bool bEndValid = GetLinkFromPosition(EndPosition, NavData, EndLink);
    
    if (!bStartValid)
    {
        AddError(TEXT("Start position should be valid"));
        return false;
    }
    
    if (!bEndValid)
    {
        AddError(TEXT("End position should be valid"));
        return false;
    }
    
    // Find initial path
    FAeonixNavigationPath InitialPath;
    bool bInitialPathFound = PathFinder.FindPath(StartLink, EndLink, StartPosition, EndPosition, InitialPath, nullptr);
    
    if (!bInitialPathFound)
    {
        AddError(TEXT("Initial path should be found"));
        return false;
    }
    
    if (bInitialPathFound)
    {
        const TArray<FAeonixPathPoint>& InitialPoints = InitialPath.GetPathPoints();
        UE_LOG(LogTemp, Display, TEXT("Initial path found with %d points, %d iterations"),
               InitialPoints.Num(), PathFinder.GetLastIterationCount());
        
        // Verify initial path avoids obstacle A (150, 0, 0)
        if (!VerifyPathAvoidsPosition(InitialPath, FVector(150, 0, 0), TEXT("Obstacle A")))
        {
            return false;
        }
        
        // Log initial path for debugging
        UE_LOG(LogTemp, Display, TEXT("Initial path points:"));
        for (int32 i = 0; i < FMath::Min(5, InitialPoints.Num()); i++)
        {
            UE_LOG(LogTemp, Display, TEXT("  %d: %s"), i, *InitialPoints[i].Position.ToString());
        }
        if (InitialPoints.Num() > 5)
        {
            UE_LOG(LogTemp, Display, TEXT("  ... (%d more points)"), InitialPoints.Num() - 5);
        }
    }
    else
    {
        AddError(TEXT("Failed to find initial path"));
        return false;
    }
    
    // ============================================
    // PHASE 2: Move obstacle and regenerate dynamic region
    // ============================================
    UE_LOG(LogTemp, Display, TEXT("\n=== PHASE 2: Move Obstacle and Regenerate ==="));
    
    // Move obstacle from position A to position B
    MockCollision->MoveObstacle(FVector(150, 0, 0), FVector(-150, 0, 0));
    
    // Regenerate dynamic region with updated collision
    UE_LOG(LogTemp, Display, TEXT("Regenerating dynamic region..."));
    TSet<FGuid> RegionsToRegen;
    RegionsToRegen.Add(RegionId);
    NavData.RegenerateDynamicRegions(RegionsToRegen, *MockCollision, MockDebug);
    UE_LOG(LogTemp, Display, TEXT("Dynamic region regenerated"));
    
    // ============================================
    // PHASE 3: Get updated path with obstacle at position B
    // ============================================
    UE_LOG(LogTemp, Display, TEXT("\n=== PHASE 3: Updated Path (Obstacle at Position B) ==="));
    
    // Find updated path
    FAeonixNavigationPath UpdatedPath;
    bool bUpdatedPathFound = PathFinder.FindPath(StartLink, EndLink, StartPosition, EndPosition, UpdatedPath, nullptr);
    
    if (!bUpdatedPathFound)
    {
        AddError(TEXT("Updated path should be found"));
        return false;
    }
    
    if (bUpdatedPathFound)
    {
        const TArray<FAeonixPathPoint>& UpdatedPoints = UpdatedPath.GetPathPoints();
        UE_LOG(LogTemp, Display, TEXT("Updated path found with %d points, %d iterations"),
               UpdatedPoints.Num(), PathFinder.GetLastIterationCount());
        
        // Verify updated path avoids obstacle B (-150, 0, 0)
        if (!VerifyPathAvoidsPosition(UpdatedPath, FVector(-150, 0, 0), TEXT("Obstacle B")))
        {
            return false;
        }
        
        // Verify updated path is different from initial path
        if (!VerifyPathsAreDifferent(InitialPath, UpdatedPath, TEXT("Initial vs Updated")))
        {
            return false;
        }
        
        // Log updated path for debugging
        UE_LOG(LogTemp, Display, TEXT("Updated path points:"));
        for (int32 i = 0; i < FMath::Min(5, UpdatedPoints.Num()); i++)
        {
            UE_LOG(LogTemp, Display, TEXT("  %d: %s"), i, *UpdatedPoints[i].Position.ToString());
        }
        if (UpdatedPoints.Num() > 5)
        {
            UE_LOG(LogTemp, Display, TEXT("  ... (%d more points)"), UpdatedPoints.Num() - 5);
        }
    }
    else
    {
        AddError(TEXT("Failed to find updated path"));
        return false;
    }
    
    // ============================================
    // PHASE 4: Additional verification
    // ============================================
    UE_LOG(LogTemp, Display, TEXT("\n=== PHASE 4: Additional Verification ==="));
    
    // Verify both paths are reasonable lengths
    float InitialPathLength = 0.0f;
    const TArray<FAeonixPathPoint>& InitialPoints = InitialPath.GetPathPoints();
    for (int32 i = 1; i < InitialPoints.Num(); i++)
    {
        InitialPathLength += FVector::Dist(InitialPoints[i-1].Position, InitialPoints[i].Position);
    }
    
    float UpdatedPathLength = 0.0f;
    const TArray<FAeonixPathPoint>& UpdatedPoints = UpdatedPath.GetPathPoints();
    for (int32 i = 1; i < UpdatedPoints.Num(); i++)
    {
        UpdatedPathLength += FVector::Dist(UpdatedPoints[i-1].Position, UpdatedPoints[i].Position);
    }
    
    float DirectDistance = FVector::Dist(StartPosition, EndPosition);
    
    UE_LOG(LogTemp, Display, TEXT("Path length analysis:"));
    UE_LOG(LogTemp, Display, TEXT("  Direct distance: %.1f units"), DirectDistance);
    UE_LOG(LogTemp, Display, TEXT("  Initial path: %.1f units (%.1f%% of direct)"),
           InitialPathLength, (InitialPathLength / DirectDistance) * 100.0f);
    UE_LOG(LogTemp, Display, TEXT("  Updated path: %.1f units (%.1f%% of direct)"),
           UpdatedPathLength, (UpdatedPathLength / DirectDistance) * 100.0f);
    
    // Paths should be reasonably efficient (not more than 3x direct distance)
    if (InitialPathLength >= DirectDistance * 3.0f)
    {
        AddError(FString::Printf(TEXT("Initial path should be reasonably efficient but is %.1f units (direct: %.1f)"), InitialPathLength, DirectDistance));
        return false;
    }
    
    if (UpdatedPathLength >= DirectDistance * 3.0f)
    {
        AddError(FString::Printf(TEXT("Updated path should be reasonably efficient but is %.1f units (direct: %.1f)"), UpdatedPathLength, DirectDistance));
        return false;
    }
    
    // Success!
    UE_LOG(LogTemp, Display, TEXT("\n✅ TEST PASSED: Dynamic region updates correctly affect pathfinding!"));
    UE_LOG(LogTemp, Display, TEXT("========================================"));
    
    AddInfo(FString::Printf(TEXT("Dynamic region update test completed successfully")));
    AddInfo(FString::Printf(TEXT("Initial path: %d points, %.1f units"), InitialPoints.Num(), InitialPathLength));
    AddInfo(FString::Printf(TEXT("Updated path: %d points, %.1f units"), UpdatedPoints.Num(), UpdatedPathLength));
    AddInfo(FString::Printf(TEXT("Paths are meaningfully different after obstacle movement")));
    
    // Cleanup
    delete MockCollision;
    
    return true;
}