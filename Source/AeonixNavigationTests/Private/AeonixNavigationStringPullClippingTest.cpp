// Regression test for GitHub issue #53: path shortcuts through blocked geometry.
//
// The A* search only ever steps between neighbouring free voxels, so the raw came-from
// chain never crosses blocked space. The post-processing passes that follow it do not
// share that guarantee: string pulling decides whether two points can be joined purely
// from how far the intermediate points sit from the straight line (scaled by voxel size),
// the optimise pass culls points purely on segment angle, and position smoothing slides
// points toward the line between their neighbours. None of them ask the octree whether
// the resulting segment passes through a blocked voxel.
//
// This test sweeps a seeded set of start/target pairs through obstacle worlds with the
// default post-processing enabled and asserts that every segment of every returned path
// stays inside free voxels. The first clipping pair is reported in full so it can be
// turned into a focused case.
//
// The ring world is a thin solid with a curved surface, queried between points close to it, so
// the octree around it mixes voxel sizes and routes hug it across layer transitions. Paths there
// used to clip in ways the flat-wall worlds do not show: string pulling kept a point at a layer
// transition between two points it had just joined, leaving it attached by segments nobody had
// tested; the optimise pass measured angles against points string pulling had already culled;
// and line of sight let a segment through the edge where two cells meet, one of them blocked.
//
// A second test pins what string pulling is for: in the ring world a pulled path never has
// more points than the same path unpulled, and far fewer over the sweep.

#include "Data/AeonixData.h"
#include "Data/AeonixLink.h"
#include "Pathfinding/AeonixPathFinder.h"
#include "Pathfinding/AeonixNavigationPath.h"
#include "Engine/EngineTypes.h"
#include "Math/RandomStream.h"
#include "Misc/AutomationTest.h"
#include "../Public/AeonixNavigationTestMocks.h"

namespace
{
	// A thin flat ring lying in the XY plane, centred on the origin: blocked where a voxel overlaps it.
	class FTestRingCollisionQueryInterface : public IAeonixCollisionQueryInterface
	{
	public:
		float InnerRadius = 200.0f;
		float OuterRadius = 260.0f;
		float HalfHeight = 30.0f;

		virtual bool IsBlocked(const FVector& Position, const float VoxelSize, ECollisionChannel CollisionChannel, const float AgentRadius) const override
		{
			if (FMath::Abs(Position.Z) - VoxelSize > HalfHeight)
			{
				return false;
			}
			// The voxel's nearest and furthest distance from the ring's axis.
			const FVector2D Nearest(FMath::Max(FMath::Abs(Position.X) - VoxelSize, 0.0f), FMath::Max(FMath::Abs(Position.Y) - VoxelSize, 0.0f));
			const FVector2D Furthest(FMath::Abs(Position.X) + VoxelSize, FMath::Abs(Position.Y) + VoxelSize);
			return Furthest.Size() >= InnerRadius && Nearest.Size() <= OuterRadius;
		}

		virtual bool IsLeafBlocked(const FVector& Position, const float LeafSize, ECollisionChannel CollisionChannel, const float AgentRadius) const override
		{
			return IsBlocked(Position, LeafSize, CollisionChannel, AgentRadius);
		}

		// Within Margin of the ring: where the octree is finest, and a route has to hug the surface.
		bool IsNear(const FVector& Position, float Margin = 60.0f) const
		{
			const float Radius = FVector2D(Position.X, Position.Y).Size();
			return FMath::Abs(Position.Z) <= HalfHeight + Margin && Radius >= InnerRadius - Margin && Radius <= OuterRadius + Margin;
		}
	};

	struct FClipSweepResult
	{
		int32 PathsTested = 0;
		int32 PathsClipped = 0;
		int32 PointsTotal = 0;
		// The point count of each path tested, in query order.
		TArray<int32> PointCounts;
		FString FirstClipReport;
	};

	// Builds navigation data for the given collision world and runs NumPairs seeded queries through it,
	// both ends of each accepted by AcceptEndpoint.
	FClipSweepResult SweepWorld(const TCHAR* WorldName, IAeonixCollisionQueryInterface& Collision,
		const FAeonixPathFinderSettings& PathSettings, int32 Seed, int32 NumPairs,
		TFunctionRef<bool(const FVector&)> AcceptEndpoint = [](const FVector&) { return true; }, int32 AttemptsPerPair = 10)
	{
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
		NavData.Generate(*DummyWorld, Collision, DebugDraw);

		AeonixPathFinder PathFinder(NavData, PathSettings);
		FRandomStream Rng(Seed);
		FClipSweepResult Result;

		// Keep queries inside the volume with a margin so the endpoints resolve to real voxels.
		const float Range = 450.f;
		int32 Attempts = 0;
		while (Result.PathsTested < NumPairs && Attempts < NumPairs * AttemptsPerPair)
		{
			Attempts++;
			const FVector StartPos(Rng.FRandRange(-Range, Range), Rng.FRandRange(-Range, Range), Rng.FRandRange(-Range, Range));
			const FVector TargetPos(Rng.FRandRange(-Range, Range), Rng.FRandRange(-Range, Range), Rng.FRandRange(-Range, Range));
			if (!AcceptEndpoint(StartPos) || !AcceptEndpoint(TargetPos))
			{
				continue;
			}

			AeonixLink StartLink, GoalLink;
			if (!GetLinkFromPosition(StartPos, NavData, StartLink) || !GetLinkFromPosition(TargetPos, NavData, GoalLink))
			{
				continue; // Endpoint inside geometry, not a useful query.
			}
			if (StartLink == GoalLink)
			{
				continue;
			}

			FAeonixNavigationPath Path;
			if (!PathFinder.FindPath(StartLink, GoalLink, StartPos, TargetPos, Path))
			{
				continue; // Unreachable pairs are not what this test is about.
			}

			Result.PathsTested++;

			const TArray<FAeonixPathPoint>& Points = Path.GetPathPoints();
			Result.PointsTotal += Points.Num();
			Result.PointCounts.Add(Points.Num());
			int32 BlockedSegmentEnd = -1;
			FVector BlockedSample;
			if (FindBlockedPathSegment(NavData, Points, BlockedSegmentEnd, BlockedSample))
			{
				Result.PathsClipped++;
				if (Result.FirstClipReport.IsEmpty())
				{
					Result.FirstClipReport = FString::Printf(
						TEXT("World '%s', seed %d, query #%d: start %s -> target %s\n    segment %d (%s, layer %d) -> %d (%s, layer %d) passes through blocked space at %s\n    full path (%d points):"),
						WorldName, Seed, Attempts, *StartPos.ToString(), *TargetPos.ToString(),
						BlockedSegmentEnd - 1, *Points[BlockedSegmentEnd - 1].Position.ToString(), Points[BlockedSegmentEnd - 1].Layer,
						BlockedSegmentEnd, *Points[BlockedSegmentEnd].Position.ToString(), Points[BlockedSegmentEnd].Layer,
						*BlockedSample.ToString(), Points.Num());
					for (int32 i = 0; i < Points.Num(); ++i)
					{
						Result.FirstClipReport += FString::Printf(TEXT("\n      %d: %s (layer %d)"), i, *Points[i].Position.ToString(), Points[i].Layer);
					}
				}
			}
		}

		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAeonixNavigation_StringPullClippingTest,
	"AeonixNavigation.Pathfinding.PostProcessClipping",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAeonixNavigation_StringPullClippingTest::RunTest(const FString& Parameters)
{
	// Default post-processing: optimise, string pulling and position smoothing all on.
	FAeonixPathFinderSettings PathSettings;
	PathSettings.MaxIterations = 10000;
	PathSettings.bUseUnitCost = false;
	PathSettings.HeuristicSettings.EuclideanWeight = 1.0f;
	PathSettings.HeuristicSettings.GlobalWeight = 10.0f;

	const int32 Seed = 63053;
	const int32 PairsPerWorld = 150;
	// The ring clips rarely enough to need a longer sweep.
	const int32 RingPairs = 600;
	// Few random points lie close enough to the ring to be accepted.
	const int32 RingAttemptsPerPair = 1000;

	// World A: two walls on the X=0 plane with a gap between them.
	FTestPartialObstacleCollisionQueryInterface GapWorld;

	// World B: a wall on the Y=0 plane that stops short of the +X edge and the top of the volume,
	// so routes go around its end or over its top.
	FTestWallCollisionQueryInterface CornerWorld;
	CornerWorld.WallXMin = -1000.f;
	CornerWorld.WallXMax = 250.f;
	CornerWorld.WallZMin = -1000.f;
	CornerWorld.WallZMax = 250.f;

	// World C: a thin ring round the origin, queried between points close to it, so routes hug a curved
	// surface through the finest voxels and across layer transitions.
	FTestRingCollisionQueryInterface RingWorld;

	const FClipSweepResult GapResult = SweepWorld(TEXT("gap"), GapWorld, PathSettings, Seed, PairsPerWorld);
	const FClipSweepResult CornerResult = SweepWorld(TEXT("corner"), CornerWorld, PathSettings, Seed, PairsPerWorld);
	const FClipSweepResult RingResult = SweepWorld(TEXT("ring"), RingWorld, PathSettings, Seed, RingPairs,
		[&RingWorld](const FVector& Point) { return RingWorld.IsNear(Point); }, RingAttemptsPerPair);

	// The same ring with the intermediate point type, whose averaging pass moves points after string pulling.
	FAeonixPathFinderSettings IntermediateSettings = PathSettings;
	IntermediateSettings.PathPointType = EAeonixPathPointType::INTERMEDIATE;
	const FClipSweepResult IntermediateResult = SweepWorld(TEXT("ring, intermediate points"), RingWorld, IntermediateSettings, Seed, RingPairs,
		[&RingWorld](const FVector& Point) { return RingWorld.IsNear(Point); }, RingAttemptsPerPair);

	UE_LOG(LogTemp, Display, TEXT("Post-process clipping sweep: gap world %d/%d clipped, corner world %d/%d clipped, ring world %d/%d clipped, ring world with intermediate points %d/%d clipped"),
		GapResult.PathsClipped, GapResult.PathsTested, CornerResult.PathsClipped, CornerResult.PathsTested, RingResult.PathsClipped, RingResult.PathsTested,
		IntermediateResult.PathsClipped, IntermediateResult.PathsTested);

	TestTrue(TEXT("Sweep found enough reachable pairs in the gap world"), GapResult.PathsTested >= PairsPerWorld / 2);
	TestTrue(TEXT("Sweep found enough reachable pairs in the corner world"), CornerResult.PathsTested >= PairsPerWorld / 2);
	TestTrue(TEXT("Sweep found enough reachable pairs in the ring world"), RingResult.PathsTested >= RingPairs / 2);
	TestTrue(TEXT("Sweep found enough reachable pairs in the ring world with intermediate points"), IntermediateResult.PathsTested >= RingPairs / 2);

	for (const FClipSweepResult* Result : { &GapResult, &CornerResult, &RingResult, &IntermediateResult })
	{
		if (Result->PathsClipped > 0)
		{
			AddError(FString::Printf(TEXT("Issue #53: %d of %d post-processed paths pass through blocked voxels. First case:\n  %s"),
				Result->PathsClipped, Result->PathsTested, *Result->FirstClipReport));
		}
	}

	TestEqual(TEXT("No post-processed path in the gap world passes through a blocked voxel"), GapResult.PathsClipped, 0);
	TestEqual(TEXT("No post-processed path in the corner world passes through a blocked voxel"), CornerResult.PathsClipped, 0);
	TestEqual(TEXT("No post-processed path in the ring world passes through a blocked voxel"), RingResult.PathsClipped, 0);
	TestEqual(TEXT("No post-processed path of intermediate points in the ring world passes through a blocked voxel"), IntermediateResult.PathsClipped, 0);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAeonixNavigation_StringPullShortensTest,
	"AeonixNavigation.Pathfinding.StringPullShortensPaths",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FAeonixNavigation_StringPullShortensTest::RunTest(const FString& Parameters)
{
	// The same seeded queries with and without string pulling; nothing else post-processes.
	FAeonixPathFinderSettings PathSettings;
	PathSettings.MaxIterations = 10000;
	PathSettings.bOptimizePath = false;
	PathSettings.bSmoothPositions = false;

	const int32 Seed = 63053;
	const int32 NumPairs = 150;
	FTestRingCollisionQueryInterface RingWorld;

	PathSettings.bUseStringPulling = false;
	const FClipSweepResult Raw = SweepWorld(TEXT("ring, unpulled"), RingWorld, PathSettings, Seed, NumPairs);
	PathSettings.bUseStringPulling = true;
	const FClipSweepResult Pulled = SweepWorld(TEXT("ring, pulled"), RingWorld, PathSettings, Seed, NumPairs);

	UE_LOG(LogTemp, Display, TEXT("String pulling in the ring world: %d points over %d paths unpulled, %d pulled"),
		Raw.PointsTotal, Raw.PathsTested, Pulled.PointsTotal);

	if (!TestEqual(TEXT("Both sweeps ran the same queries"), Pulled.PointCounts.Num(), Raw.PointCounts.Num()))
	{
		return false;
	}
	TestTrue(TEXT("Sweep found enough reachable pairs"), Raw.PathsTested >= NumPairs / 2);

	int32 NumLonger = 0;
	for (int32 i = 0; i < Raw.PointCounts.Num(); ++i)
	{
		NumLonger += Pulled.PointCounts[i] > Raw.PointCounts[i] ? 1 : 0;
	}
	TestEqual(TEXT("No pulled path has more points than the same path unpulled"), NumLonger, 0);
	TestTrue(TEXT("Pulling at least halves the points over the sweep"), Pulled.PointsTotal * 2 <= Raw.PointsTotal);
	if (Pulled.PathsClipped > 0)
	{
		AddError(Pulled.FirstClipReport);
	}
	TestEqual(TEXT("No pulled path passes through a blocked voxel"), Pulled.PathsClipped, 0);

	return true;
}
