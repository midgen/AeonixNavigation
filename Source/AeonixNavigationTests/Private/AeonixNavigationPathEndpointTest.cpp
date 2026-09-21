// Regression test for GitHub issue #63: BuildPath drops the voxel adjacent to the goal.
//
// BuildPath walks the A* came-from chain starting at the goal link, but advances to the
// predecessor *before* recording a point. The goal voxel is therefore never recorded, and
// when points[0] is later overwritten with the target position, the goal's predecessor
// voxel is lost. The path then jumps straight from the second voxel back to the target,
// which can cut through blocked geometry when the goal sits around a corner.
//
// This test runs the pathfinder with every post-processing step disabled so the raw
// came-from chain is observable, then asserts:
//   1. The path starts exactly at the start position and ends exactly at the target position.
//   2. The point next to the goal is the centre of a voxel that neighbours the goal voxel
//      (this is the assertion that fails while #63 is unfixed).
//   3. The point next to the start is the centre of a voxel that neighbours the start voxel
//      (already correct today; guards against the symmetric regression).
//   4. Every raw path segment stays inside free voxels of the octree.

#include "Data/AeonixData.h"
#include "Data/AeonixLink.h"
#include "Data/AeonixNode.h"
#include "Pathfinding/AeonixPathFinder.h"
#include "Pathfinding/AeonixNavigationPath.h"
#include "Engine/EngineTypes.h"
#include "Misc/AutomationTest.h"
#include "../Public/AeonixNavigationTestMocks.h"

namespace
{
	// Mirrors the neighbour query FindPath uses for a given link.
	void GetPathfindingNeighbours(const FAeonixData& NavData, const AeonixLink& Link, TArray<AeonixLink>& OutNeighbours)
	{
		const AeonixNode& Node = NavData.OctreeData.GetNode(Link);
		if (Link.GetLayerIndex() == 0 && Node.FirstChild.IsValid())
		{
			NavData.OctreeData.GetLeafNeighbours(Link, OutNeighbours);
		}
		else
		{
			NavData.OctreeData.GetNeighbours(Link, OutNeighbours);
		}
	}

	// True if Position coincides with the centre of any voxel that neighbours Link.
	bool IsPositionAtNeighbourOf(const FAeonixData& NavData, const AeonixLink& Link, const FVector& Position, FString& OutNeighbourList)
	{
		TArray<AeonixLink> Neighbours;
		GetPathfindingNeighbours(NavData, Link, Neighbours);

		bool bMatched = false;
		OutNeighbourList.Empty();
		for (const AeonixLink& Neighbour : Neighbours)
		{
			FVector NeighbourPos;
			if (!NavData.GetLinkPosition(Neighbour, NeighbourPos))
			{
				continue;
			}
			OutNeighbourList += FString::Printf(TEXT("\n      L%d N%d S%d @ %s"),
				Neighbour.GetLayerIndex(), Neighbour.GetNodeIndex(), Neighbour.GetSubnodeIndex(), *NeighbourPos.ToString());
			if (NeighbourPos.Equals(Position, 0.1f))
			{
				bMatched = true;
			}
		}
		return bMatched;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAeonixNavigation_PathEndpointVoxelTest,
	"AeonixNavigation.Pathfinding.PathEndpointVoxels",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAeonixNavigation_PathEndpointVoxelTest::RunTest(const FString& Parameters)
{
	// Reuse the partial-obstacle world: two walls on the X=0 plane with a gap at Y in [-50, 50].
	FTestPartialObstacleCollisionQueryInterface ObstacleCollision;
	FSilentDebugDrawInterface DebugDraw;
	FAeonixData NavData;

	FAeonixGenerationParameters Params;
	Params.Origin = FVector::ZeroVector;
	Params.Extents = FVector(500, 500, 500);
	Params.OctreeDepth = 4;
	Params.CollisionChannel = ECollisionChannel::ECC_WorldStatic;
	Params.AgentRadius = 34.f;
	Params.ShowLeafVoxels = false;
	Params.ShowMortonCodes = false;
	NavData.UpdateGenerationParameters(Params);

	UWorld* DummyWorld = nullptr;
	NavData.Generate(*DummyWorld, ObstacleCollision, DebugDraw);

	// Disable every post-processing step so the output is the raw came-from chain.
	FAeonixPathFinderSettings PathSettings;
	PathSettings.MaxIterations = 10000;
	PathSettings.bUseUnitCost = false;
	PathSettings.bOptimizePath = false;
	PathSettings.bUseStringPulling = false;
	PathSettings.bSmoothPositions = false;
	PathSettings.SmoothingIterations = 0;
	PathSettings.PathPointType = EAeonixPathPointType::NODE_CENTER;
	PathSettings.HeuristicSettings.EuclideanWeight = 1.0f;
	PathSettings.HeuristicSettings.GlobalWeight = 10.0f;

	AeonixPathFinder PathFinder(NavData, PathSettings);

	// The goal sits just past the edge of obstacle 2, so the last hops of the path turn a corner.
	// Start on the far side of the wall so the route must pass through the gap first.
	const FVector StartPos(-300.f, 0.f, 0.f);
	const FVector TargetPos(60.f, 120.f, 0.f);

	AeonixLink StartLink, GoalLink;
	const bool bFoundStart = GetLinkFromPosition(StartPos, NavData, StartLink);
	const bool bFoundGoal = GetLinkFromPosition(TargetPos, NavData, GoalLink);
	TestTrue(TEXT("Found a free navigation link for the start position"), bFoundStart);
	TestTrue(TEXT("Found a free navigation link for the target position"), bFoundGoal);
	if (!bFoundStart || !bFoundGoal)
	{
		return false;
	}

	FVector StartVoxelPos, GoalVoxelPos;
	NavData.GetLinkPosition(StartLink, StartVoxelPos);
	NavData.GetLinkPosition(GoalLink, GoalVoxelPos);
	UE_LOG(LogTemp, Display, TEXT("Start link L%d N%d S%d @ %s, Goal link L%d N%d S%d @ %s"),
		StartLink.GetLayerIndex(), StartLink.GetNodeIndex(), StartLink.GetSubnodeIndex(), *StartVoxelPos.ToString(),
		GoalLink.GetLayerIndex(), GoalLink.GetNodeIndex(), GoalLink.GetSubnodeIndex(), *GoalVoxelPos.ToString());

	FAeonixNavigationPath Path;
	const bool bPathFound = PathFinder.FindPath(StartLink, GoalLink, StartPos, TargetPos, Path);
	TestTrue(TEXT("Path should exist from start to target"), bPathFound);
	if (!bPathFound)
	{
		return false;
	}

	const TArray<FAeonixPathPoint>& Points = Path.GetPathPoints();
	UE_LOG(LogTemp, Display, TEXT("Raw path has %d points:"), Points.Num());
	for (int32 i = 0; i < Points.Num(); ++i)
	{
		UE_LOG(LogTemp, Display, TEXT("  Point %d: %s (Layer: %d)"), i, *Points[i].Position.ToString(), Points[i].Layer);
	}

	// The scenario is only meaningful if the path has at least start, one intermediate voxel, and goal.
	// With start and goal this far apart there must be many more than that.
	TestTrue(TEXT("Path has enough points to contain both endpoint-adjacent voxels"), Points.Num() >= 4);
	if (Points.Num() < 4)
	{
		return false;
	}

	const int32 LastIdx = Points.Num() - 1;

	// 1. Endpoints are the requested positions (output order is start -> goal).
	TestTrue(TEXT("First path point is the start position"), Points[0].Position.Equals(StartPos, 0.1f));
	TestTrue(TEXT("Last path point is the target position"), Points[LastIdx].Position.Equals(TargetPos, 0.1f));

	// 2. The point before the target must be a voxel that neighbours the goal voxel.
	//    Issue #63: BuildPath skips the goal's predecessor, so this point is two hops away.
	{
		FString NeighbourList;
		const bool bAdjacent = IsPositionAtNeighbourOf(NavData, GoalLink, Points[LastIdx - 1].Position, NeighbourList);
		if (!bAdjacent)
		{
			AddError(FString::Printf(
				TEXT("Issue #63: point before the target (%s) is not a neighbour of the goal voxel (%s). Goal neighbours:%s"),
				*Points[LastIdx - 1].Position.ToString(), *GoalVoxelPos.ToString(), *NeighbourList));
		}
		TestTrue(TEXT("Point before the target is the centre of a voxel adjacent to the goal voxel"), bAdjacent);
	}

	// 3. Symmetric check on the start end.
	{
		FString NeighbourList;
		const bool bAdjacent = IsPositionAtNeighbourOf(NavData, StartLink, Points[1].Position, NeighbourList);
		if (!bAdjacent)
		{
			AddError(FString::Printf(
				TEXT("Point after the start (%s) is not a neighbour of the start voxel (%s). Start neighbours:%s"),
				*Points[1].Position.ToString(), *StartVoxelPos.ToString(), *NeighbourList));
		}
		TestTrue(TEXT("Point after the start is the centre of a voxel adjacent to the start voxel"), bAdjacent);
	}

	// 4. Every raw segment must stay inside free voxels. Sample at half a leaf voxel so no cell is skipped.
	{
		const float LeafSize = NavData.GetVoxelSize(0) * 0.25f;
		const float StepSize = LeafSize * 0.5f;
		int32 BlockedSegments = 0;

		for (int32 i = 1; i < Points.Num(); ++i)
		{
			const FVector& A = Points[i - 1].Position;
			const FVector& B = Points[i].Position;
			const float Length = FVector::Dist(A, B);
			const int32 NumSteps = FMath::Max(1, FMath::CeilToInt(Length / StepSize));

			for (int32 s = 0; s <= NumSteps; ++s)
			{
				const FVector Sample = FMath::Lerp(A, B, static_cast<float>(s) / static_cast<float>(NumSteps));
				AeonixLink SampleLink;
				if (!GetLinkFromPosition(Sample, NavData, SampleLink))
				{
					AddError(FString::Printf(TEXT("Segment %d -> %d passes through blocked space at %s"), i - 1, i, *Sample.ToString()));
					BlockedSegments++;
					break;
				}
			}
		}

		TestEqual(TEXT("No raw path segment passes through a blocked voxel"), BlockedSegments, 0);
	}

	return true;
}
