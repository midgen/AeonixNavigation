// Regression test for GitHub issue #62: NodeSizeWeight heuristic uses the goal layer instead of
// the candidate node layer.
//
// CalculateHeuristic scales the score by (1 - layer / numLayers * NodeSizeWeight). The intent is
// to lower the score of larger voxels so the search prefers them and completes in fewer
// iterations. The layer used, however, is aTarget's, and aTarget is always GoalLink, which is
// constant for the whole search. Every candidate therefore gets the same multiplier and node
// ordering is unaffected. With a layer-0 goal the multiplier is exactly 1.0, so NodeSizeWeight
// has no effect at all.
//
// This test builds a volume where start and goal are leaf voxels but the space between them is
// open, so the route crosses higher-layer nodes. It runs the same search twice, once with
// NodeSizeWeight = 0 and once with NodeSizeWeight = 1, and asserts the two searches behave
// differently. While #62 is unfixed the two runs are bitwise identical and the test fails.

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
	// Blocks two small cubes, one next to the start and one next to the goal. Only the layer-0
	// voxels touching a cube get subdivided, so the middle of the volume stays coarse.
	class FTestEndpointBlockerCollisionQueryInterface : public IAeonixCollisionQueryInterface
	{
	public:
		FVector BlockerA;
		FVector BlockerB;
		float BlockerHalfExtent = 20.f;

		FTestEndpointBlockerCollisionQueryInterface(const FVector& InA, const FVector& InB)
			: BlockerA(InA), BlockerB(InB) {}

		bool OverlapsBlocker(const FVector& Center, float HalfSize, const FVector& Blocker) const
		{
			const FVector Delta = (Center - Blocker).GetAbs();
			const float Reach = HalfSize + BlockerHalfExtent;
			return Delta.X <= Reach && Delta.Y <= Reach && Delta.Z <= Reach;
		}

		virtual bool IsBlocked(const FVector& Position, const float VoxelSize, ECollisionChannel CollisionChannel, const float AgentRadius) const override
		{
			// VoxelSize is passed as a half-extent by the generator.
			return OverlapsBlocker(Position, VoxelSize, BlockerA) || OverlapsBlocker(Position, VoxelSize, BlockerB);
		}

		virtual bool IsLeafBlocked(const FVector& Position, const float LeafSize, ECollisionChannel CollisionChannel, const float AgentRadius) const override
		{
			return IsBlocked(Position, LeafSize, CollisionChannel, AgentRadius);
		}
	};

	struct FSearchResult
	{
		bool bFound = false;
		int32 Iterations = 0;
		TArray<FAeonixPathPoint> Points;
	};

	FSearchResult RunSearch(const FAeonixData& NavData, const FAeonixPathFinderSettings& Settings,
		const AeonixLink& StartLink, const AeonixLink& GoalLink, const FVector& StartPos, const FVector& TargetPos)
	{
		AeonixPathFinder PathFinder(NavData, Settings);
		FAeonixNavigationPath Path;
		FSearchResult Result;
		Result.bFound = PathFinder.FindPath(StartLink, GoalLink, StartPos, TargetPos, Path);
		Result.Iterations = PathFinder.GetLastIterationCount();
		Result.Points = Path.GetPathPoints();
		return Result;
	}

	bool PathsAreIdentical(const TArray<FAeonixPathPoint>& A, const TArray<FAeonixPathPoint>& B)
	{
		if (A.Num() != B.Num())
		{
			return false;
		}
		for (int32 i = 0; i < A.Num(); ++i)
		{
			if (A[i].Layer != B[i].Layer || !A[i].Position.Equals(B[i].Position, 0.01f))
			{
				return false;
			}
		}
		return true;
	}

	void LogSearch(const TCHAR* Label, const FSearchResult& Result)
	{
		UE_LOG(LogTemp, Display, TEXT("%s: found=%d iterations=%d points=%d"), Label, Result.bFound, Result.Iterations, Result.Points.Num());
		for (int32 i = 0; i < Result.Points.Num(); ++i)
		{
			UE_LOG(LogTemp, Display, TEXT("  Point %d: %s (Layer: %d)"), i, *Result.Points[i].Position.ToString(), Result.Points[i].Layer);
		}
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAeonixNavigation_NodeSizeHeuristicTest,
	"AeonixNavigation.Pathfinding.NodeSizeHeuristicUsesCandidateLayer",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAeonixNavigation_NodeSizeHeuristicTest::RunTest(const FString& Parameters)
{
	// Volume is 1600 units across with 5 layers: layer-0 voxels are 100 units, the top layer is
	// the whole volume. A blocker sits in one layer-0 voxel at each end; everything else is open.
	const FVector BlockerA(-550.f, 50.f, 50.f);
	const FVector BlockerB(550.f, 50.f, 50.f);
	FTestEndpointBlockerCollisionQueryInterface Collision(BlockerA, BlockerB);
	FSilentDebugDrawInterface DebugDraw;
	FAeonixData NavData;

	FAeonixGenerationParameters Params;
	Params.Origin = FVector::ZeroVector;
	Params.Extents = FVector(800, 800, 800);
	Params.OctreeDepth = 4;
	Params.CollisionChannel = ECollisionChannel::ECC_WorldStatic;
	Params.AgentRadius = 0.f;
	Params.ShowLeafVoxels = false;
	Params.ShowMortonCodes = false;
	NavData.UpdateGenerationParameters(Params);

	UWorld* DummyWorld = nullptr;
	NavData.Generate(*DummyWorld, Collision, DebugDraw);

	const int32 NumLayers = NavData.OctreeData.GetNumLayers();
	TestTrue(TEXT("Octree has several layers so node size can influence ordering"), NumLayers >= 3);

	// Start and goal are free sub-voxels in the corner of the blocked layer-0 voxels.
	const FVector StartPos(-512.f, 88.f, 88.f);
	const FVector TargetPos(512.f, 88.f, 88.f);

	AeonixLink StartLink, GoalLink;
	const bool bFoundStart = GetLinkFromPosition(StartPos, NavData, StartLink);
	const bool bFoundGoal = GetLinkFromPosition(TargetPos, NavData, GoalLink);
	TestTrue(TEXT("Found a free navigation link for the start position"), bFoundStart);
	TestTrue(TEXT("Found a free navigation link for the target position"), bFoundGoal);
	if (!bFoundStart || !bFoundGoal)
	{
		return false;
	}

	UE_LOG(LogTemp, Display, TEXT("Start link L%d N%d S%d, Goal link L%d N%d S%d, NumLayers=%d"),
		StartLink.GetLayerIndex(), StartLink.GetNodeIndex(), StartLink.GetSubnodeIndex(),
		GoalLink.GetLayerIndex(), GoalLink.GetNodeIndex(), GoalLink.GetSubnodeIndex(), NumLayers);

	// The issue is only observable with a layer-0 goal: that is when the buggy multiplier is exactly 1.0.
	TestEqual(TEXT("Start link is a leaf voxel"), static_cast<int32>(StartLink.GetLayerIndex()), 0);
	TestEqual(TEXT("Goal link is a leaf voxel"), static_cast<int32>(GoalLink.GetLayerIndex()), 0);

	// Raw A* output: every post-processing step off, euclidean heuristic only.
	FAeonixPathFinderSettings BaseSettings;
	BaseSettings.MaxIterations = 20000;
	BaseSettings.bUseUnitCost = false;
	BaseSettings.bOptimizePath = false;
	BaseSettings.bUseStringPulling = false;
	BaseSettings.bSmoothPositions = false;
	BaseSettings.SmoothingIterations = 0;
	BaseSettings.PathPointType = EAeonixPathPointType::NODE_CENTER;
	BaseSettings.HeuristicSettings.EuclideanWeight = 1.0f;
	BaseSettings.HeuristicSettings.VelocityWeight = 0.0f;
	BaseSettings.HeuristicSettings.GlobalWeight = 1.0f;

	FAeonixPathFinderSettings WithoutNodeSize = BaseSettings;
	WithoutNodeSize.HeuristicSettings.NodeSizeWeight = 0.0f;

	FAeonixPathFinderSettings WithNodeSize = BaseSettings;
	WithNodeSize.HeuristicSettings.NodeSizeWeight = 1.0f;

	const FSearchResult Baseline = RunSearch(NavData, WithoutNodeSize, StartLink, GoalLink, StartPos, TargetPos);
	const FSearchResult Weighted = RunSearch(NavData, WithNodeSize, StartLink, GoalLink, StartPos, TargetPos);

	LogSearch(TEXT("NodeSizeWeight=0"), Baseline);
	LogSearch(TEXT("NodeSizeWeight=1"), Weighted);

	TestTrue(TEXT("Path found with NodeSizeWeight=0"), Baseline.bFound);
	TestTrue(TEXT("Path found with NodeSizeWeight=1"), Weighted.bFound);
	if (!Baseline.bFound || !Weighted.bFound)
	{
		return false;
	}

	// Precondition: the search really does cross larger voxels, otherwise node size cannot matter.
	bool bCrossesHigherLayer = false;
	for (const FAeonixPathPoint& Point : Baseline.Points)
	{
		if (Point.Layer > 0)
		{
			bCrossesHigherLayer = true;
			break;
		}
	}
	TestTrue(TEXT("Baseline path passes through at least one voxel above layer 0"), bCrossesHigherLayer);

	// Issue #62: with the goal at layer 0 the node size multiplier is 1.0 for every candidate, so
	// enabling NodeSizeWeight changes nothing. Once the candidate's own layer is used, larger voxels
	// score lower and the expansion order (and hence iteration count and/or path) changes.
	const bool bSameIterations = Baseline.Iterations == Weighted.Iterations;
	const bool bSamePath = PathsAreIdentical(Baseline.Points, Weighted.Points);
	if (bSameIterations && bSamePath)
	{
		AddError(FString::Printf(
			TEXT("Issue #62: NodeSizeWeight=1 produced exactly the same search as NodeSizeWeight=0 (%d iterations, %d points). The node size heuristic is not affecting node ordering."),
			Baseline.Iterations, Baseline.Points.Num()));
	}
	TestFalse(TEXT("Enabling NodeSizeWeight changes the search result"), bSameIterations && bSamePath);

	return true;
}
